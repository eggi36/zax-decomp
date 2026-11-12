"""Read the classes of Zax.exe with size, parent class, factory, vtable and field offsets.

Every class registers itself at startup with (call to ``ClassInfo_Register`` 0x415780, ``this`` = new
``CClassInfo``)::

    Register(&classVar, "CName", version (float), parent class, size, abstract (byte),
             create, create array (new[] n), register fields)

The parent class is 0/-1 or the result of a getter function (``mov eax, [classVar]``).
The factory allocates the object with ``operator_new`` and calls the constructor; the last vtable the
constructor writes to ``[this]`` is the class's own. The field registration creates one descriptor
per field (``Ctor(name, [extra,] offset, size, offset, [bit mask], ...)``) and appends it with
``ClassInfo_AddField`` (0x415700).

    .venv/bin/python -I tools/exe_classes.py "$(python3 -I tools/game_paths.py exe)"

writes ``docs/classes.csv`` and ``docs/fields.csv``. Needs capstone and pefile.
"""
from __future__ import annotations

import csv
import re
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import game_paths  # noqa: E402

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import formats_path  # noqa: E402,F401  zaxtools (zax-formats)
from zaxtools.exeschema import ADD_FIELD, ENCODING, ENUM_ADD, HELP, NEW, REG_CLASS  # noqa: E402

OPNEW_ECX = 0x417710          # operator_new with the size in ecx


class Exe:
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        pe = pefile.PE(data=self.data, fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.secs = {s.Name.rstrip(b'\0').decode(): (self.base + s.VirtualAddress, s.Misc_VirtualSize,
                                                      s.PointerToRawData, s.SizeOfRawData)
                     for s in pe.sections}
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.skipdata = True

    def off(self, va):
        for lo, size, raw, rsize in self.secs.values():
            if lo <= va < lo + size and va - lo < rsize:
                return raw + va - lo
        return None

    def within(self, va, name):
        lo, size, _, _ = self.secs[name]
        return lo <= va < lo + size

    def dword(self, va):
        o = self.off(va)
        return struct.unpack_from('<I', self.data, o)[0] if o is not None else None

    def cstr(self, va):
        o = self.off(va) if va else None
        if o is None:
            return None
        e = self.data.find(b'\0', o)
        s = self.data[o:e]
        return s.decode('latin-1') if 0 < len(s) < 120 and all(32 <= c < 127 for c in s) else None

    def func(self, va, limit=4000):
        """Instructions from va up to the first ret (linear)."""
        o = self.off(va)
        out = []
        if o is None:
            return out
        for i in self.md.disasm(self.data[o:o + limit], va):
            out.append(i)
            if i.mnemonic in ('ret', 'retn'):
                break
        return out

    def text(self):
        lo, size, raw, _ = self.secs['.text']
        return self.md.disasm(self.data[raw:raw + size], lo)


def imm(op):
    try:
        return int(op, 16) if op.startswith('0x') or op.startswith('-0x') else int(op)
    except ValueError:
        return None


_getters: dict = {}


def getter(exe, t):
    if t in _getters:
        return _getters[t]
    ins = exe.func(t, 16)
    if ins and ins[0].mnemonic == 'mov' and ins[0].op_str.startswith('eax, dword ptr [0x'):
        _getters[t] = int(ins[0].op_str.split('[')[1].rstrip(']'), 16)
    else:
        _getters[t] = None
    return _getters[t]


def registrations(exe):
    """All Register calls with their 9 arguments (registers replaced by known values)."""
    pushes, regs, out = [], {}, []
    for i in exe.text():
        m, op = i.mnemonic, i.op_str
        if m == 'push':
            pushes = (pushes + [regs.get(op, op)])[-40:]
        elif m == 'mov' and re.match(r'^e[a-z]{2}, ', op):
            r, src = op[:3], op[5:]
            mm = re.match(r'^dword ptr \[(0x[0-9a-f]+)\]$', src)
            regs[r] = ('var', int(mm.group(1), 16)) if mm else None
        elif m == 'call':
            t = imm(op)
            if t == REG_CLASS:
                args = list(reversed(pushes[-9:]))
                out.append((i.address, args))
                pushes = []
            elif t:
                g = getter(exe, t)
                regs['eax'] = ('var', g) if g else None
            else:
                regs['eax'] = None
    return out


def _vtable_writes(exe, fn):
    """vtables that fn writes to [this] (offset 0), in order."""
    out = []
    for i in exe.func(fn, 6000):
        mm = re.match(r'^dword ptr \[e[a-z]{2}\], (0x[0-9a-f]+)$', i.op_str)
        if i.mnemonic == 'mov' and mm and exe.within(int(mm.group(1), 16), '.rdata'):
            out.append(int(mm.group(1), 16))
    return out


def vtable_of(exe, create):
    """vtable and constructor of the class, taken from its factory.

    The factory calls ``operator_new`` and then either calls the constructor (``jmp``/``call``) or
    has it inlined (call the base constructor, then write its own vtable). The last vtable written
    to [this] is the one that counts."""
    ins = exe.func(create, 400)
    if len(ins) <= 2:                       # xor eax, eax; ret: abstract class
        return None, None
    calls = [imm(i.op_str) for i in ins if i.mnemonic in ('call', 'jmp')]
    calls = [t for t in calls if t and t not in (NEW, OPNEW_ECX) and exe.within(t, '.text')
             and not create <= t < ins[-1].address + ins[-1].size]
    own = _vtable_writes(exe, create)
    if own:                                 # constructor inlined
        return own[-1], None
    ctor = calls[0] if calls else None
    vts = _vtable_writes(exe, ctor) if ctor else []
    return (vts[-1] if vts else None), ctor


def all_vtable_writes(exe):
    out = set()
    for i in exe.text():
        if i.mnemonic == 'mov' and i.op_str.startswith('dword ptr [e'):
            mm = re.match(r'^dword ptr \[e[a-z]{2}(?: \+ 0x[0-9a-f]+)?\], (0x[0-9a-f]+)$', i.op_str)
            if mm:
                v = int(mm.group(1), 16)
                if exe.within(v, '.rdata') and (p := exe.dword(v)) is not None and exe.within(p, '.text'):
                    out.add(v)
    return out


def fields_of(exe, regfn):
    """Fields from the registration function: name, constructor (encoding), offset, size."""
    out, cur, pushes = [], None, []
    for i in exe.func(regfn, 200000):
        m, op = i.mnemonic, i.op_str
        if m == 'push':
            pushes.append(op)
        elif m == 'call':
            t = imm(op)
            if t == ADD_FIELD:
                if cur:
                    out.append(cur)
                cur = None
            elif t in (NEW, ENUM_ADD, HELP, OPNEW_ECX) or t is None:
                pass
            elif len(pushes) >= 4 and exe.cstr(imm(pushes[-1]) or 0):
                a = [imm(p) for p in reversed(pushes)]
                # arguments: [extra,] offset, size, offset, [bit mask for switches], ...
                i = next((j for j in range(1, len(a) - 2) if a[j] is not None and a[j] == a[j + 2]
                          and a[j + 1] in (1, 2, 4, 8)), None)
                cur = {'name': exe.cstr(a[0]), 'ctor': t,
                       'offset': a[i] if i else None, 'size': a[i + 1] if i else None,
                       'bit': a[i + 3] if i and ENCODING.get(t) == 'u8' and i + 3 < len(a) else None}
            pushes = []
    return out


def main(argv):
    exe_path = Path(argv[1]) if len(argv) > 1 else Path(game_paths.exe_arg())
    exe = Exe(exe_path)
    regs = registrations(exe)
    var2name = {}
    for _, a in regs:
        if isinstance(a[0], str) and imm(a[0]):
            var2name[imm(a[0])] = exe.cstr(imm(a[1]) or 0)

    def val(x):
        if isinstance(x, tuple):
            return x
        return imm(x) if isinstance(x, str) else None

    # Every getter registers the class itself on demand; the first call counts
    sites: dict[int, list] = {}
    for site, a in regs:
        if len(a) >= 9 and isinstance(val(a[0]), int):
            sites.setdefault(val(a[0]), []).append((site, a))

    rows, frows, vt_starts = [], [], set()
    for var, calls in sites.items():
        site, a = calls[0]
        name = exe.cstr(val(a[1]) or 0)
        if not name:
            continue
        par = val(a[3])
        parent = var2name.get(par[1]) if isinstance(par, tuple) else None
        ver = val(a[2])
        version = struct.unpack('<f', struct.pack('<I', ver & 0xffffffff))[0] if isinstance(ver, int) else None
        create, fn8, regfn = (val(a[6]), val(a[7]), val(a[8]))
        vt, ctor = vtable_of(exe, create) if isinstance(create, int) and create else (None, None)
        if vt:
            vt_starts.add(vt)
        flds = fields_of(exe, regfn) if isinstance(regfn, int) and regfn else []
        rows.append({'name': name, 'parent': parent or '', 'size': val(a[4]),
                     'version': '' if version is None else f'{version:g}', 'abstract': val(a[5]),
                     'class_var': hex(var), 'register_call': hex(site), 'calls': len(calls),
                     'create': hex(create) if create else '', 'constructor': hex(ctor) if ctor else '',
                     'create_array': hex(fn8) if isinstance(fn8, int) and fn8 else '',
                     'register_fields': hex(regfn) if isinstance(regfn, int) and regfn else '',
                     'vtable': hex(vt) if vt else '', 'fields': len(flds)})
        for n, f in enumerate(flds):
            frows.append({'class': name, 'index': n, 'name': f['name'].split('/')[-1], 'path': f['name'],
                          'encoding': ENCODING.get(f['ctor'], hex(f['ctor'])),
                          'offset': hex(f['offset']) if f['offset'] is not None else '',
                          'size': f['size'] if f['size'] is not None else '',
                          'bitmask': hex(f['bit']) if f['bit'] is not None else ''})
    # vtable length: entries up to the next vtable start or a pointer that is not code.
    # vtable starts are all .rdata addresses that code writes to [this] or [this+n] (including
    # unregistered and abstract classes).
    vt_starts |= all_vtable_writes(exe)
    for r in rows:
        if not r['vtable']:
            r['vtable_entries'] = ''
            continue
        va, n = int(r['vtable'], 16), 0
        while True:
            p = exe.dword(va + 4 * n)
            if p is None or not exe.within(p, '.text') or (n and va + 4 * n in vt_starts):
                break
            n += 1
        r['vtable_entries'] = n

    cols = ['name', 'parent', 'size', 'version', 'abstract', 'vtable', 'vtable_entries', 'fields',
            'class_var', 'register_call', 'calls', 'create', 'constructor', 'create_array',
            'register_fields']
    with open(ROOT / 'docs' / 'classes.csv', 'w', newline='') as fh:
        w = csv.DictWriter(fh, cols)
        w.writeheader()
        w.writerows(sorted(rows, key=lambda r: r['name']))
    with open(ROOT / 'docs' / 'fields.csv', 'w', newline='') as fh:
        w = csv.DictWriter(fh, ['class', 'index', 'name', 'path', 'encoding', 'offset', 'size', 'bitmask'])
        w.writeheader()
        w.writerows(frows)
    print(f'{len(rows)} classes, {sum(1 for r in rows if r["vtable"])} with vtable, {len(frows)} fields')


if __name__ == '__main__':
    main(sys.argv)
