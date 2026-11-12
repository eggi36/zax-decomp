"""Disassemble functions of Zax.exe, annotated with names from docs/functions.csv and strings.

    .venv/bin/python -I tools/disasm.py "$(python3 -I tools/game_paths.py exe)" 0x415780 [0x415700 ...]
    .venv/bin/python -I tools/disasm.py "$(python3 -I tools/game_paths.py exe)" 0x4154d0-0x415970   # range, linear
    .venv/bin/python -I tools/disasm.py 0x415780   # without the exe: the build 1308 exe of tools/game_paths.py

A function's length comes from docs/functions.csv (column size), otherwise up to the first ret.
"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import game_paths  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from exe_classes import Exe  # noqa: E402


def load_names() -> tuple[dict[int, str], dict[int, int]]:
    name, size = {}, {}
    with open(ROOT / 'docs/functions.csv', newline='') as f:
        for r in csv.DictReader(f):
            a = int(r['address'], 16)
            n = r['name']
            name[a] = f"{r['class']}::{n}" if r['class'] else n
            size[a] = int(r['size'] or 0)
    with open(ROOT / 'docs/exe_symbols.csv', newline='') as f:
        for r in csv.reader(f):
            if r and r[0].startswith('0x'):
                name.setdefault(int(r[0], 16), r[1])
    return name, size


def show(exe: Exe, lo: int, hi: int | None, name: dict[int, str]) -> None:
    o = exe.off(lo)
    n = (hi - lo) if hi else 4000
    for i in exe.md.disasm(exe.data[o:o + n], lo):
        if i.address in name and i.address != lo:
            print(f'\n{i.address:#x} <{name[i.address]}>:')
        notes = []
        for m in re.finditer(r'0x[0-9a-f]{5,8}', i.op_str):
            v = int(m.group(), 16)
            if v in name:
                notes.append(name[v])
            elif (s := exe.cstr(v)) and len(s) > 1:
                notes.append(repr(s))
        print(f'  {i.address:#x}: {i.bytes.hex():<20} {i.mnemonic} {i.op_str}' + (f'   ; {", ".join(notes)}' if notes else ''))
        if hi is None and i.mnemonic in ('ret', 'retn'):
            break


def main() -> None:
    # the exe path may be left out: the build 1308 exe of tools/game_paths.py
    args = sys.argv[1:]
    if args and not args[0].lower().startswith('0x'):
        exe_path, args = args[0], args[1:]
    else:
        exe_path = game_paths.exe_arg()
    exe = Exe(Path(exe_path))
    name, size = load_names()
    for arg in args:
        if '-' in arg:
            lo, hi = (int(x, 16) for x in arg.split('-'))
        else:
            lo = int(arg, 16)
            hi = lo + size[lo] if size.get(lo) else None
        print(f'{lo:#x} <{name.get(lo, "?")}>:')
        show(exe, lo, hi, name)
        print()


if __name__ == '__main__':
    main()
