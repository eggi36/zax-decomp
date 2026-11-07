"""Build the name list for the Ghidra project from the tables in docs/.

Sources (in order of precedence):
1. ``docs/exe_symbols.csv`` - names confirmed by hand,
2. ``docs/classes.csv`` - per class vtable, class variable, factory, constructor, field registration,
3. the vtables themselves: every function in a vtable belongs to the lowest common parent class of
   all classes whose vtable contains it; named after its slot (``vf<n>``).

    .venv/bin/python -I tools/ghidra_names.py ['gamedata/Zax - German/Zax.exe']

writes ``out/ghidra/names.tsv`` (address, namespace, name, kind, source), which
``tools/ghidra/ZaxNames.java`` imports into Ghidra.
"""
from __future__ import annotations

import csv
import struct
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parent.parent


def main(argv):
    exe_path = Path(argv[1]) if len(argv) > 1 else ROOT / 'gamedata' / 'Zax - German' / 'Zax.exe'
    data = exe_path.read_bytes()
    pe = pefile.PE(data=data, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.text')
    tlo, thi = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize

    def dword(va):
        return struct.unpack_from('<I', data, pe.get_offset_from_rva(va - base))[0]

    classes = {r['name']: r for r in csv.DictReader(open(ROOT / 'docs' / 'classes.csv'))}
    parent = {n: r['parent'] or None for n, r in classes.items()}

    def chain(n):
        out = []
        while n:
            out.append(n)
            n = parent.get(n)
        return out

    def common(names):
        chains = [chain(n) for n in names]
        shared = set(chains[0]).intersection(*map(set, chains[1:]))
        return next((c for c in chains[0] if c in shared), None)

    names: dict[int, tuple] = {}

    def put(addr, ns, name, kind, src):
        if addr and addr not in names:
            names[addr] = (ns, name, kind, src)

    for r in csv.DictReader(open(ROOT / 'docs' / 'exe_symbols.csv')):
        put(int(r['address'], 16), '', r['name'], 'data' if r['kind'] == 'data' else 'function',
            'exe_symbols')

    for n, r in classes.items():
        h = lambda k: int(r[k], 16) if r[k] else None
        put(h('vtable'), n, 'vftable', 'vtable', 'classes')
        put(h('class_var'), n, 'classInfo', 'data', 'classes')
        if r['abstract'] != '1':
            put(h('create'), n, 'Create', 'function', 'classes')
            put(h('create_array'), n, 'CreateArray', 'function', 'classes')
        put(h('constructor'), n, n, 'function', 'classes')
        put(h('register_fields'), n, 'RegisterFields', 'function', 'classes')

    # Functions from vtables: slot and all classes that contain them
    seen: dict[int, dict] = {}
    for n, r in classes.items():
        if not r['vtable']:
            continue
        va = int(r['vtable'], 16)
        for i in range(int(r['vtable_entries'] or 0)):
            p = dword(va + 4 * i)
            if tlo <= p < thi:
                e = seen.setdefault(p, {'slots': set(), 'classes': set()})
                e['slots'].add(i)
                e['classes'].add(n)
    for p, e in seen.items():
        owner = common(sorted(e['classes']))
        if not owner:
            continue                       # in separate hierarchies (e.g. empty default methods)
        slot = '_'.join(str(s) for s in sorted(e['slots']))
        put(p, owner, f'vf{slot}', 'function', 'vtable')

    out = ROOT / 'out' / 'ghidra'
    out.mkdir(parents=True, exist_ok=True)
    with open(out / 'names.tsv', 'w', newline='') as fh:
        w = csv.writer(fh, delimiter='\t', lineterminator='\n')
        for a in sorted(names):
            w.writerow([f'0x{a:x}', *names[a]])
    kinds = {}
    for v in names.values():
        kinds[v[3]] = kinds.get(v[3], 0) + 1
    print(f'{len(names)} names', kinds)


if __name__ == '__main__':
    main(sys.argv)
