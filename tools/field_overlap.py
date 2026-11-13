#!/usr/bin/env python3
"""Misread fields (audit class 3): members of docs/fields.csv whose bytes lie inside another member's.

    python3 -I tools/field_overlap.py [--csv OUT]

docs/fields.csv holds every field the game registers for its classes (tools/exe_classes.py: offset, size, encoding, bit
mask); docs/classes.csv the classes with their parent and size. Reported:
  inside     two fields of one class whose byte ranges [offset, offset + size) overlap, unless both are bits of one
             byte or word (encoding u8 with a bit mask) with disjoint masks;
  base       a field of a class whose range overlaps a field of one of its base classes (parent, its parent, ...),
             unless it is the same field again (same name, offset, size and encoding: a re-registration);
  base-part  a field of a class that lies inside a base class's size (offset < the nearest base's size) without
             overlapping a registered field of that base: the derived class's view of bytes the base owns;
  beyond     a field whose end lies past its class's size.
Fields without an offset or size (embedded objects, encodings named by a function) take part only with their offset
(as one byte) and are counted as "no size". Every hit needs a look at the exe's accesses before it is called a misread.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Fields of the flags kind (vtable 0x5ec684, constructor 0x414cf0) are 16 bits of their dword: its slot 30 0x414dd0 returns 0xffff
# and FlagsField_FromText 0x414fa0 stores (old ^ ((old ^ new) & 0xffff)) without a setter. fields.csv names them "str"; here they
# take that mask, so the u8 bits above them in the same dword are not taken for an overlap. Constructor calls with offset and setter 0
# [verified: reimpl/src/layer_parts.cpp, CLayerPart RegisterFields 0x4f9c50; the exe's accesses mask 0xffff].
FLAG_MASKS = {('CLayerPart', 'Category'): 0xffff, ('CLayerPart', 'Used In'): 0xffff}


def load() -> tuple[dict, dict]:
    classes = {r['name']: r for r in csv.DictReader(open(ROOT / 'docs/classes.csv', newline=''))}
    fields: dict[str, list[dict]] = {}
    for r in csv.DictReader(open(ROOT / 'docs/fields.csv', newline='')):
        if not r['offset']:
            continue
        r = dict(r)
        r['off'] = int(r['offset'], 16)
        r['len'] = int(r['size']) if r['size'] else 1
        r['nosize'] = not r['size']
        r['mask'] = int(r['bitmask'], 16) if r['bitmask'] else FLAG_MASKS.get((r['class'], r['name']))
        fields.setdefault(r['class'], []).append(r)
    return classes, fields


def overlap(a: dict, b: dict) -> bool:
    return a['off'] < b['off'] + b['len'] and b['off'] < a['off'] + a['len']


def bits_apart(a: dict, b: dict) -> bool:
    """Both bits of the same byte or word with disjoint masks (a mask 0 is a whole byte: not apart; the flags fields of FLAG_MASKS count)."""
    return (a['mask'] and b['mask'] and a['off'] == b['off']
            and a['len'] == b['len'] and not (a['mask'] & b['mask']))


def same_field(a: dict, b: dict) -> bool:
    return (a['name'], a['off'], a['len'], a['encoding'], a['mask']) == (b['name'], b['off'], b['len'], b['encoding'], b['mask'])


def bases(classes: dict, name: str) -> list[str]:
    out, seen = [], set()
    p = classes.get(name, {}).get('parent')
    while p and p not in seen:
        out.append(p)
        seen.add(p)
        p = classes.get(p, {}).get('parent')
    return out


def desc(f: dict) -> str:
    m = f' mask {f["bitmask"]}' if f['bitmask'] else ''
    s = '?' if f['nosize'] else f['len']
    return f'{f["class"]}.{f["name"]} @{f["offset"]} size {s} {f["encoding"]}{m}'


def scan() -> list[tuple[str, dict, dict | None, str]]:
    classes, fields = load()
    hits = []
    for cls, fs in sorted(fields.items()):
        for i, a in enumerate(fs):
            for b in fs[i + 1:]:
                if overlap(a, b) and not bits_apart(a, b):
                    hits.append(('inside', a, b, ''))
        chain = bases(classes, cls)
        for a in fs:
            hit_base = False
            for base in chain:
                for b in fields.get(base, []):
                    if overlap(a, b):
                        hit_base = True
                        if not same_field(a, b) and not bits_apart(a, b):
                            hits.append(('base', a, b, base))
            if chain and not hit_base:
                size = classes.get(chain[0], {}).get('size')
                if size and a['off'] < int(size):
                    hits.append(('base-part', a, None, f'{chain[0]} size {size}'))
            size = classes.get(cls, {}).get('size')
            if size and a['off'] + a['len'] > int(size):
                hits.append(('beyond', a, None, f'{cls} size {size}'))
    return hits


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--csv', type=Path, help='write the hits as CSV')
    a = ap.parse_args()
    hits = scan()
    for kind, f, g, note in hits:
        print(f'{kind:9} {desc(f)}' + (f'  <->  {desc(g)}' if g else '') + (f'  ({note})' if note else ''))
    counts: dict[str, int] = {}
    for h in hits:
        counts[h[0]] = counts.get(h[0], 0) + 1
    print('hits:', ', '.join(f'{k} {v}' for k, v in sorted(counts.items())) or 'none')
    if a.csv:
        with open(a.csv, 'w', newline='') as fh:
            w = csv.writer(fh)
            w.writerow(['kind', 'class', 'field', 'offset', 'size', 'encoding', 'bitmask', 'other_class', 'other_field', 'other_offset',
                        'other_size', 'note'])
            for kind, f, g, note in hits:
                w.writerow([kind, f['class'], f['name'], f['offset'], f['size'], f['encoding'], f['bitmask'],
                            g['class'] if g else '', g['name'] if g else '', g['offset'] if g else '', g['size'] if g else '', note])
    return 0


if __name__ == '__main__':
    sys.exit(main())
