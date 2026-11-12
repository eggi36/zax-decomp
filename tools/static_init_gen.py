"""Generate the reimplementation, hook tables and checks of batch C2 of phase 5a (docs/DECOMPILATION.md 10):
the static initializers (entries of the CRT initializer table 0x609000-0x60a3ec), the atexit destructors
and the function pointers in .data that docs/exe_symbols.csv lists as "open game code, not system layer".

    .venv/bin/python -I tools/static_init_gen.py            # writes the files, prints a summary
    .venv/bin/python -I tools/static_init_gen.py --list     # one line per function: kind, shape, size

How the code is made [the exe's bytes are the only input besides the names]:
- Every function is disassembled over the size the map gave it and run through a small symbolic
  translator: registers hold constants or C++ expressions, `push` builds the argument list of the next
  call, a call's convention comes from the code (ecx set since the last call: thiscall; the callee's own
  `ret n`: the arguments it pops; else cdecl with the caller's cleanup), memory operands with a constant
  address become get32/put32 of that address, a load is read where the original reads it, a store
  written where the original writes it. A conditional forward jump becomes if/else with both paths
  translated (the paths end at their own ret, so the tail is duplicated); a backward jump (a loop),
  string instructions or anything else the translator does not know makes the function "manual": it is
  written by hand in reimpl/src/static_c2_manual.cpp (MANUAL below names them) and only its hook entry
  and check entry are generated.
- eax: a function returns what the original leaves in eax where that is defined by its own code (a
  call's result, a constant); where the original leaves the caller's eax the reimplementation returns
  nothing and the check does not compare eax (mask 0); a byte register left in al compares the low byte.
- Hooks: the first whole instructions of at least 5 bytes are the trampoline (a relative call among them
  through call_at; a relative jump: no trampoline, length 0). Functions shorter than 5 bytes cannot be
  hooked (all of them are a single `ret`): they are listed as identical by their bytes and the checks
  read the byte in the running exe.
- Checks (reimpl/src/checks_static_c2*.cpp, harness checks_static_c2.h): every callee is a recording
  stand-in of its convention and argument count (so no live object is constructed or destroyed again and
  atexit never gets a second entry), the globals the function reads or writes by address are saved, set
  to case values, compared after each side and put back.

The rows whose "static initializer" kind does not hold (referenced only from vtables or code, methods that
use ecx) are not part of C2; the tool reports them (NOT_C2) for the vtable batches.
"""
from __future__ import annotations

import csv
import re
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import game_paths  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from exe_classes import Exe  # noqa: E402

XC_BEGIN, XC_END = 0x609000, 0x60a3ec       # the CRT's C++ initializer table (__xc_a .. __xc_z)
ATEXIT, OP_NEW = 0x5d0863, 0x5d033a
KINDS = ('atexit destructor', 'static initializer', 'function pointer in .data')

# Functions the translator does not take (loops, string instructions, virtual calls on objects the
# function reaches through globals); written by hand in static_c2_manual.cpp. Per function: the check's
# extra setup (a function in checks_static_c2_manual.cpp, or None), the ranges of memory it writes beyond
# its direct globals (hashed into the state after each side) and extra globals {address: (size, kind)}.
# Rows the map filed under a kind that does not hold, with what they are (the comment of the generated code;
# they stay in C2 with the shape the map's kind gives them)
KIND_FIX = {
    0x47eaf0: 'the callback StaticInit_DPlayGame 0x47eb00 passes to CallMember 0x55d050 (registers the DirectPlay '
              'net game factory 0x47eb10 through NetHandler_Set), not an atexit destructor',
}

MANUAL = {
    0x46eb90: dict(setup=None, ranges=[(0x6bd330, 0x200)],
                   callees={0x4defd0: ('fastx', 0, 'v'), 0x4e13a0: ('fastx', 0, 't')}),
    0x487550: dict(setup='setup_487550', object_args=True, mask=0),
    0x4b4020: dict(setup='setup_texture_list_4b4020', object_args=True),
    0x4d5360: dict(setup='setup_texture_list_4d5360', object_args=True, mask=0xff),
    0x4d5460: dict(setup='setup_texture_list_4d5460', object_args=True, mask=0xff),
    0x4ef1f0: dict(setup='setup_texture_list_4ef1f0', object_args=True),
    0x4ef320: dict(setup='setup_texture_list_4ef320', object_args=True),
    0x56bfb0: dict(setup='setup_texture_list_56bfb0', object_args=True),
    0x4dd890: dict(setup=None, ranges=[(0x6c0e2c, 0x80)]),
    0x4ef3f0: dict(setup=None),
    0x4ef420: dict(setup=None),
    0x53f470: dict(setup=None),
    0x5470f0: dict(setup=None),
    0x547200: dict(setup='setup_547200', ranges=[(0x711738, 0x648)]),
    0x525650: dict(setup='setup_525650', object_args=True),
    0x57cde0: dict(setup='setup_57cde0', object_args=True),
    0x57dfa0: dict(setup='setup_57dfa0', object_args=True),
    0x59a3c0: dict(setup='setup_59a3c0', mask=0, extra={0x713e24: 4}),
}

MODULES = [  # name, first address (inclusive), up to (exclusive)
    ('static_c2a', 0x400000, 0x430000),
    ('static_c2b', 0x430000, 0x4b0000),
    ('static_c2c', 0x4b0000, 0x4d0000),
    ('static_c2d', 0x4d0000, 0x520000),
    ('static_c2e', 0x520000, 0x570000),
    ('static_c2f', 0x570000, 0x600000),
]


class Manual(Exception):
    pass


def load_names() -> dict[int, str]:
    names: dict[int, str] = {}
    with open(ROOT / 'docs/functions.csv', newline='') as f:
        for r in csv.DictReader(f):
            n = r['name']
            names[int(r['address'], 16)] = f"{r['class']}::{n}" if r['class'] and '::' not in n else n
    with open(ROOT / 'docs/reimpl.csv', newline='') as f:
        for r in csv.DictReader(f):
            if r['address'].startswith('0x'):
                names.setdefault(int(r['address'], 16), r['name'])
    with open(ROOT / 'docs/exe_symbols.csv', newline='') as f:
        for r in csv.DictReader(f):
            if r['address'].startswith('0x'):
                names.setdefault(int(r['address'], 16), r['name'])
    return names


def load_sizes() -> dict[int, int]:
    sizes = {}
    with open(ROOT / 'docs/functions.csv', newline='') as f:
        for r in csv.DictReader(f):
            if r['size']:
                sizes[int(r['address'], 16)] = int(r['size'])
    return sizes


def c2_rows() -> list[dict]:
    rows = []
    with open(ROOT / 'docs/exe_symbols.csv', newline='') as f:
        for r in csv.DictReader(f):
            if 'not system layer' in r['source'] and any(k in r['source'] for k in KINDS):
                m = re.search(r'(\d+) bytes', r['source'])
                refs = [int(x, 16) for x in re.findall(r'0x[0-9a-f]+', r['source'].split('referenced from')[1].split(';')[0])]
                kind = next(k for k in KINDS if k in r['source'])
                rows.append(dict(address=int(r['address'], 16), name=r['name'], size=int(m.group(1)), refs=refs, kind=kind))
    return rows


class Exe2(Exe):
    def dword(self, a: int) -> int:
        return struct.unpack_from('<I', self.data, self.off(a))[0]

    def cstring(self, a: int) -> str | None:
        if not (0x5ea000 <= a < 0x718000):
            return None
        try:
            o = self.off(a)
        except Exception:
            return None
        if o is None or o >= len(self.data):
            return None
        end = self.data.find(b'\0', o, o + 200)
        if end <= o:
            return None
        s = self.data[o:end]
        if len(s) < 2 or any(c < 0x20 or c > 0x7e for c in s):
            return None
        return s.decode('latin-1')


# ---- The callees' conventions ----------------------------------------------------------------------------

def callee_pops(exe: Exe2, addr: int, sizes: dict[int, int], depth: int = 0) -> int:
    """The bytes the callee pops (its ret n): every ret of its known size, else the first ret."""
    n = sizes.get(addr, 0) or 4000
    o = exe.off(addr)
    rets = set()
    for i in exe.md.disasm(exe.data[o:o + n], addr):
        if i.mnemonic == 'ret':
            rets.add(int(i.op_str, 16) if i.op_str else 0)
            if addr not in sizes:
                break
        if i.mnemonic == 'jmp' and i.address == addr and i.op_str.startswith('0x') and depth < 3:
            return callee_pops(exe, int(i.op_str, 16), sizes, depth + 1)
        if i.mnemonic == 'int3' and addr not in sizes:
            break
    if len(rets) != 1:
        raise Manual(f'callee {addr:#x}: ret {sorted(rets)}')
    return rets.pop()


# ---- The symbolic translator ----------------------------------------------------------------------------

REG32 = ('eax', 'ecx', 'edx', 'ebx', 'esi', 'edi')
REG8 = {'al': 'eax', 'cl': 'ecx', 'dl': 'edx', 'bl': 'ebx'}


class Val:
    """A register's value: a constant (const) or a C++ expression (expr), `mask` the bits it defines
    (0xffffffff, 0xff for a byte register over an unknown rest), None: not set by the function."""

    def __init__(self, const=None, expr=None, mask=0xffffffff, ptr=False):
        self.const, self.expr, self.mask, self.ptr = const, expr, mask, ptr

    def c(self) -> str:
        if self.const is not None:
            lit = f'{self.const:#x}u' if self.const > 9 else f'{self.const}u'
            w = layer_spelling(self.const) if self.mask == 0xffffffff else None
            return f'{w}({lit})' if w else lit
        return self.expr

    def __repr__(self):
        return f'Val({self.c()}, {self.mask:#x})'


_KINDS = None


def layer_spelling(a: int) -> str | None:
    """The address layer's value spelling of a constant that is an exe address (docs/DECOMPILATION.md 13, "Batch 6-3:
    the address layer"; since batch 6-4): a reimplemented function fn_addr, a vtable vt, other data glob_addr (the kinds
    of tools/addr_rewrite.py); None for a number or an address the layer has no value for (an import slot, code that is
    not reimplemented). Both backends: the DLL's are consteval and give the literal itself, so its code stays the same;
    the exe's give our own definition once the block's home is `own`."""
    global _KINDS
    if a < 0x401000:
        return None
    if _KINDS is None:
        import addr_rewrite
        _KINDS = addr_rewrite.Kinds()
    return {'code': 'fn_addr', 'vtable': 'vt', 'data': 'glob_addr'}.get(_KINDS.kind(a))


class Path_:
    def __init__(self):
        self.regs: dict[str, Val | None] = {r: None for r in REG32}
        self.stack: list = []          # Val, ('saved', reg), ('done',) for a cdecl call's argument
        self.ecx_fresh = False
        self.flags = None
        self.barrier = 0               # stack height after the last call

    def copy(self) -> 'Path_':
        p = Path_()
        p.regs = dict(self.regs)
        p.stack = list(self.stack)
        p.ecx_fresh, p.flags, p.barrier = self.ecx_fresh, self.flags, self.barrier
        return p


class Translator:
    def __init__(self, exe: Exe2, names, sizes, addr: int, size: int):
        self.exe, self.names, self.sizes = exe, names, sizes
        self.addr, self.size = addr, size
        o = exe.off(addr)
        self.ins = {i.address: i for i in exe.md.disasm(exe.data[o:o + size], addr)}
        if sum(i.size for i in self.ins.values()) != size:
            raise Manual('size does not decode')
        self.order = sorted(self.ins)
        self.tmp = 0
        self.calls: dict[tuple, int] = {}   # (address, conv, nargs) -> first seen
        self.result_kinds: dict[int, str] = {}
        self.globals: dict[int, list] = {}  # address -> [size, read, written, deref, tested]
        self.masks: list[int] = []          # eax mask per return
        self.deref_tmp: set[str] = set()
        self.tmp_global: dict[str, int] = {}
        self.tmp_call: dict[str, int] = {}
        self.tested_tmp: set[str] = set()

    # -- helpers
    def name(self, a: int) -> str:
        return self.names.get(a, f'sub_{a:x}')

    def note(self, a: int) -> str:
        s = self.exe.cstring(a)
        if s is not None:
            s = s.replace('*/', '* /').replace('\\', '\\\\')
            return f' /* "{s[:48]}" */'
        if a in self.names and 0x401000 <= a < 0x5ea000:
            return f' /* {self.names[a]} */'
        return ''

    def arg(self, v: Val) -> str:
        if v.const is not None:
            return v.c() + self.note(v.const)
        return v.expr

    def new_tmp(self, prefix='v') -> str:
        self.tmp += 1
        return f'{prefix}{self.tmp}'

    def glob(self, a: int, size: int, read=False, write=False):
        g = self.globals.setdefault(a, [size, False, False, False, False])
        g[0] = max(g[0], size)
        g[1] |= read
        g[2] |= write

    def operand(self, p: Path_, op: str):
        """('reg', name) / ('imm', value) / ('mem', size, address Val, const address or None)"""
        op = op.strip()
        if op in REG32:
            return ('reg', op)
        if op in REG8:
            return ('reg8', op)
        if re.fullmatch(r'-?0x[0-9a-f]+|-?\d+', op):
            return ('imm', int(op, 0) & 0xffffffff)
        m = re.fullmatch(r'(dword|byte) ptr (?:es:)?\[(.+)\]', op)
        if m:
            size = 4 if m.group(1) == 'dword' else 1
            expr = m.group(2).replace(' ', '')
            if re.fullmatch(r'0x[0-9a-f]+', expr):
                return ('mem', size, Val(const=int(expr, 16)), int(expr, 16))
            m2 = re.fullmatch(r'(e[a-z]{2})([+-]0x[0-9a-f]+|[+-]\d+)?', expr)
            if m2 and m2.group(1) in REG32:
                base = p.regs[m2.group(1)]
                if base is None or base.mask != 0xffffffff:
                    raise Manual(f'memory through unknown {m2.group(1)}')
                disp = int(m2.group(2), 0) if m2.group(2) else 0
                if base.const is not None:
                    a = (base.const + disp) & 0xffffffff
                    return ('mem', size, Val(const=a), a)
                if base.expr in self.tmp_global:
                    self.globals[self.tmp_global[base.expr]][3] = True
                self.deref_tmp.add(base.expr)
                e = base.expr if not disp else f'{base.expr} {"+" if disp > 0 else "-"} {abs(disp):#x}u'
                return ('mem', size, Val(expr=e), None)
            raise Manual(f'address {expr}')
        raise Manual(f'operand {op}')

    def read(self, p: Path_, out: list, opnd) -> Val:
        if opnd[0] == 'imm':
            return Val(const=opnd[1])
        if opnd[0] == 'reg':
            v = p.regs[opnd[1]]
            if v is None:
                raise Manual(f'read of {opnd[1]} not set')
            return v
        if opnd[0] == 'reg8':
            v = p.regs[REG8[opnd[1]]]
            if v is None:
                raise Manual(f'read of {opnd[1]} not set')
            if v.const is not None:
                return Val(const=v.const & 0xff, mask=0xff)
            return Val(expr=f'(BYTE){v.expr}' if v.mask != 0xff else v.expr, mask=0xff)
        _, size, av, ca = opnd
        t = self.new_tmp()
        fn = 'get32' if size == 4 else 'get8'
        out.append(f'const {"DWORD" if size == 4 else "BYTE"} {t} = {fn}({av.c()});')
        if ca is not None:
            self.glob(ca, size, read=True)
            self.tmp_global[t] = ca
        return Val(expr=t, mask=0xffffffff if size == 4 else 0xff)

    def write(self, p: Path_, out: list, opnd, v: Val):
        if opnd[0] == 'reg':
            p.regs[opnd[1]] = v
            if opnd[1] == 'ecx':
                p.ecx_fresh = True
            return
        if opnd[0] == 'reg8':
            r = REG8[opnd[1]]
            old = p.regs[r]
            if old is not None and old.mask == 0xffffffff:
                raise Manual('byte write into a full register')
            p.regs[r] = Val(const=v.const & 0xff, mask=0xff) if v.const is not None else Val(expr=v.expr, mask=0xff)
            return
        if opnd[0] == 'mem':
            _, size, av, ca = opnd
            if size == 4 and v.mask != 0xffffffff:
                raise Manual('dword store of a byte value')
            fn = 'put32' if size == 4 else 'put8'
            val = self.arg(v) if size == 4 else (f'{v.const & 0xff:#x}' if v.const is not None else v.expr)
            out.append(f'{fn}({av.c()}{self.note(ca) if ca else ""}, {val});')
            if ca is not None:
                self.glob(ca, size, write=True)
            return
        raise Manual('write to an immediate')

    # -- calls
    def do_call(self, p: Path_, out: list, target: int, tail: bool, at: int, via: str | None = None) -> Val:
        """A call of target; via: the expression the original reads the target from (a constant slot of a vtable),
        called through the helper's `_at` form (a run-time address, not a key of the layer)."""
        pops = callee_pops(self.exe, target, self.sizes) // 4
        thiscall = p.ecx_fresh
        # arguments: the callee's own `ret n`, or for a cdecl callee the caller's cleanup right after the
        # call; a cdecl callee without cleanup takes none (the pushes before it belong to a later call)
        if pops:
            top = p.stack[len(p.stack) - pops:] if len(p.stack) >= pops else None
            if top is None or any(not isinstance(e, Val) for e in top):
                raise Manual(f'call {target:#x}: pops {pops}, stack {p.stack}')
            args = top
            del p.stack[len(p.stack) - pops:]
            conv = 'this' if thiscall else 'std'
        else:
            nxt = self.next_ins(at)
            while nxt is not None and nxt.mnemonic == 'mov' and 'esp' not in nxt.op_str and not tail:
                nxt = self.next_ins(nxt.address)   # mov esi, eax before the cleanup
            clean = 0
            if nxt is not None and not tail:
                if nxt.mnemonic == 'pop' and nxt.op_str == 'ecx':
                    clean = 1
                elif nxt.mnemonic == 'add' and nxt.op_str.startswith('esp, '):
                    clean = int(nxt.op_str.split(', ')[1], 0) // 4
            top = p.stack[len(p.stack) - clean:] if clean else []
            if any(not isinstance(e, Val) for e in top) or len(top) != clean:
                raise Manual(f'call {target:#x}: cleanup {clean} over {p.stack}')
            args = top
            for k in range(len(p.stack) - clean, len(p.stack)):
                p.stack[k] = ('done', p.stack[k])
            # a cdecl callee with the caller's cleanup: ecx is not its `this` even when set (0x4f8bf0 loads
            # a vtable into ecx before atexit)
            conv = 'cdecl' if clean or not thiscall else 'this'
        args = list(reversed(args))   # first argument = last pushed
        self.calls.setdefault((target, conv, len(args)), at)
        fn = {'this': 'this_call', 'std': 'std_call', 'cdecl': 'c_call'}[conv]
        parts = [f'{target:#x}u /* {self.name(target)} */']
        if via:
            fn += '_at'
            parts = [f'{via} /* {self.name(target)} {target:#x} */']
        if conv == 'this':
            ecx = p.regs['ecx']
            if ecx is None or ecx.mask != 0xffffffff:
                raise Manual('this call without ecx')
            parts.append(self.arg(ecx))
        parts += [self.arg(a) for a in args]
        call = f'{fn}({", ".join(parts)})'
        for r in ('eax', 'ecx', 'edx'):
            p.regs[r] = None
        p.ecx_fresh = False
        p.barrier = len(p.stack)
        if tail:
            return Val(expr=call)
        t = self.new_tmp('r')
        self.tmp_call[t] = target
        out.append(f'const DWORD {t} = {call};')
        v = Val(expr=t)
        p.regs['eax'] = v
        return v

    def next_ins(self, at: int):
        i = self.ins[at]
        return self.ins.get(at + i.size)

    # -- conditions
    def cond(self, p: Path_, mnem: str) -> str:
        if p.flags is None:
            raise Manual('jump without flags')
        kind = p.flags[0]
        if kind == 'test':
            a, b = p.flags[1], p.flags[2]
            e = a.c() if a.c() == b.c() else f'({a.c()} & {b.c()})'
            for v in (a, b):
                if v.expr:
                    self.tested_tmp.add(v.expr)
            if mnem == 'je':
                return f'{e} == 0'
            if mnem == 'jne':
                return f'{e} != 0'
        if kind == 'cmp':
            a, b = p.flags[1], p.flags[2]
            for v in (a, b):
                if v.expr:
                    self.tested_tmp.add(v.expr)
            ops = {'je': '==', 'jne': '!=', 'jae': '>=', 'jb': '<', 'jbe': '<=', 'ja': '>'}
            if mnem in ops:
                return f'{a.c()} {ops[mnem]} {b.c()}'
        if kind == 'res':
            a = p.flags[1]
            if mnem in ('je', 'jne'):
                return f'{a.c()} {"==" if mnem == "je" else "!="} 0'
            if mnem == 'js':
                return f'(int){a.c()} < 0'
        raise Manual(f'condition {mnem} after {kind}')

    # -- the walk
    def run(self, at: int, p: Path_, out: list, depth=0):
        if depth > 6:
            raise Manual('too many branches')
        while True:
            if at not in self.ins:
                raise Manual(f'jump outside the function to {at:#x}')
            i = self.ins[at]
            m, ops = i.mnemonic, [o.strip() for o in i.op_str.split(',')] if i.op_str else []
            nxt = at + i.size
            if m == 'push':
                o = self.operand(p, ops[0])
                if o[0] == 'reg' and o[1] == 'ecx':
                    p.ecx_fresh = False
                if o[0] == 'reg' and p.regs[o[1]] is None:
                    p.stack.append(('saved', o[1]))
                else:
                    p.stack.append(self.read(p, out, o))
            elif m == 'pop':
                if not p.stack:
                    raise Manual('pop of an empty stack')
                e = p.stack.pop()
                if isinstance(e, tuple) and e[0] == 'saved':
                    if e[1] != ops[0]:
                        raise Manual('pop into another register')
                    p.regs[ops[0]] = None
                elif isinstance(e, tuple) and e[0] == 'done':
                    p.regs[ops[0]] = None    # the cleanup of a cdecl call (pop ecx)
                    if ops[0] == 'ecx':
                        p.ecx_fresh = False
                else:
                    raise Manual('pop of a value')
                p.barrier = min(p.barrier, len(p.stack))
            elif m == 'add' and ops[0] == 'esp':
                k = int(ops[1], 0) // 4
                for _ in range(k):
                    e = p.stack.pop()
                    if not (isinstance(e, tuple) and e[0] == 'done'):
                        raise Manual('add esp over values')
                p.barrier = min(p.barrier, len(p.stack))
            elif m == 'mov':
                d, s = self.operand(p, ops[0]), self.operand(p, ops[1])
                v = self.read(p, out, s)
                self.write(p, out, d, v)
            elif m == 'lea':
                mm = re.fullmatch(r'(e[a-z]{2}), \[(e[a-z]{2}) ([+-]) (0x[0-9a-f]+|\d+)\]', i.op_str)
                if not mm or p.regs.get(mm.group(2)) is None or p.regs[mm.group(2)].const is None:
                    raise Manual(f'lea {i.op_str}')
                v = p.regs[mm.group(2)].const + (1 if mm.group(3) == '+' else -1) * int(mm.group(4), 0)
                self.write(p, out, ('reg', mm.group(1)), Val(const=v & 0xffffffff))
            elif m == 'xor' and ops[0] == ops[1] and ops[0] in REG32:
                self.write(p, out, ('reg', ops[0]), Val(const=0))
                p.flags = ('res', Val(const=0))
            elif m in ('test', 'cmp'):
                a = self.read(p, out, self.operand(p, ops[0]))
                b = self.read(p, out, self.operand(p, ops[1]))
                p.flags = (m, a, b)
            elif m in ('or', 'and', 'add', 'sub') and len(ops) == 2:
                d = self.operand(p, ops[0])
                if d[0] == 'mem':
                    raise Manual('arithmetic on memory')
                a = self.read(p, out, d)
                b = self.read(p, out, self.operand(p, ops[1]))
                op = {'or': '|', 'and': '&', 'add': '+', 'sub': '-'}[m]
                mask = a.mask
                if a.const is not None and b.const is not None:
                    v = Val(const=eval(f'{a.const} {op} {b.const}') & mask, mask=mask)
                else:
                    t = self.new_tmp()
                    ty = 'DWORD' if mask == 0xffffffff else 'BYTE'
                    out.append(f'const {ty} {t} = ({ty})({a.c()} {op} {b.c()});')
                    v = Val(expr=t, mask=mask)
                self.write(p, out, d, v)
                p.flags = ('res', v)
            elif m in ('shl', 'shr') and re.fullmatch(r'0x[0-9a-f]+|\d+', ops[1]):
                d = self.operand(p, ops[0])
                a = self.read(p, out, d)
                k = int(ops[1], 0)
                if a.const is not None:
                    v = Val(const=((a.const << k) if m == 'shl' else (a.const >> k)) & 0xffffffff)
                else:
                    t = self.new_tmp()
                    out.append(f'const DWORD {t} = {a.c()} {"<<" if m == "shl" else ">>"} {k};')
                    v = Val(expr=t)
                self.write(p, out, d, v)
                p.flags = ('res', v)
            elif m in ('dec', 'inc'):
                d = self.operand(p, ops[0])
                a = self.read(p, out, d)
                t = self.new_tmp()
                out.append(f'const DWORD {t} = {a.c()} {"-" if m == "dec" else "+"} 1;')
                v = Val(expr=t)
                self.write(p, out, d, v)
                p.flags = ('res', v)
            elif m == 'call':
                if ops[0].startswith('0x'):
                    self.do_call(p, out, int(ops[0], 16), False, at)
                else:
                    mm = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', ops[0])
                    if not mm or not (0x5ea000 <= int(mm.group(1), 16) < 0x609000):
                        raise Manual(f'call {ops[0]}')
                    slot = int(mm.group(1), 16)   # a constant slot of a vtable in .rdata
                    target = self.exe.dword(slot)
                    self.do_call(p, out, target, False, at, via=f'get32({Val(const=slot).c()})')
            elif m == 'jmp':
                t = int(ops[0], 16)
                if self.addr <= t < self.addr + self.size:
                    if t <= at:
                        raise Manual('backward jump')
                    at = t
                    continue
                if any(isinstance(e, tuple) and e[0] == 'saved' for e in p.stack) or any(isinstance(e, Val) for e in p.stack):
                    raise Manual('tail jump with a stack')
                v = self.do_call(p, out, t, True, at)
                out.append(f'return {v.expr};')
                self.masks.append(0xffffffff)
                return
            elif m.startswith('j'):
                t = int(ops[0], 16)
                if t <= at:
                    raise Manual('backward jump')
                c = self.cond(p, m)
                a_out, b_out = [], []
                self.run(t, p.copy(), a_out, depth + 1)
                self.run(nxt, p.copy(), b_out, depth + 1)
                out.append(f'if ({c}) {{')
                out += ['    ' + x for x in a_out]
                out.append('}')
                out += b_out
                return
            elif m == 'ret':
                if i.op_str:
                    raise Manual('ret n')
                if p.stack:
                    raise Manual('ret with a stack')
                v = p.regs['eax']
                if v is None:
                    out.append('return;')
                    self.masks.append(0)
                else:
                    out.append(f'return {self.arg(v) if v.mask == 0xffffffff else v.c()};')
                    self.masks.append(v.mask)
                return
            else:
                raise Manual(f'instruction {m} {i.op_str}')
            at = nxt

    def translate(self) -> list[str]:
        out: list[str] = []
        self.run(self.addr, Path_(), out)
        return out


def tidy(lines: list[str], returns_value: bool) -> list[str]:
    """Drop the temporaries nobody reads; `return;` paths of a DWORD function return 0 ([the caller's
    eax in the original] is not compared then)."""
    text = '\n'.join(lines)
    res = []
    for ln in lines:
        m = re.match(r'(\s*)const (?:DWORD|BYTE) (r\d+) = (.*);$', ln)
        if m and len(re.findall(rf'\b{m.group(2)}\b', text)) == 1:
            ln = f'{m.group(1)}{m.group(3)};'
        if returns_value and ln.strip() == 'return;':
            ln = ln.replace('return;', 'return 0;   // the original leaves the caller\'s eax')
        res.append(ln)
    # `const DWORD rN = X; return rN;` -> `return X;`
    out = []
    for ln in res:
        m = re.match(r'(\s*)return (r\d+);$', ln)
        if m and out:
            p = re.match(rf'\s*const DWORD {m.group(2)} = (.*);$', out[-1])
            if p:
                out[-1] = f'{m.group(1)}return {p.group(1)};'
                continue
        out.append(ln)
    res = out
    # a final plain `return;` is implicit
    if res and res[-1] == 'return;':
        res.pop()
    return res


# ---- Hooks --------------------------------------------------------------------------------------------------

def hook_entry(exe: Exe2, a: int, name: str) -> str:
    o = exe.off(a)
    exp = exe.data[o:o + 8]
    length, call_at = 0, 0
    for i in exe.md.disasm(exe.data[o:o + 32], a):
        if i.mnemonic.startswith('j'):
            length = 0
            break
        if i.mnemonic == 'call' and i.bytes[0] == 0xe8:
            if call_at:
                length = 0
                break
            call_at = i.address - a + 1
        length = i.address - a + i.size
        if length >= 5:
            break
    if length == 0:
        call_at = 0
    e = ', '.join(f'{b:#04x}' for b in exp)
    tail = f', {call_at}' if call_at else ''
    return f'        {{{a:#x}, ptr(&{name}), "{name}", {length}, {{{e}}}{tail}}},'


# ---- Main ---------------------------------------------------------------------------------------------------

def main() -> None:
    exe = Exe2(Path(game_paths.exe_arg()))
    names, sizes = load_names(), load_sizes()
    rows = c2_rows()
    funcs, not_c2, unhookable = [], [], []
    for r in rows:
        a = r['address']
        if r['kind'] != 'atexit destructor' and not any(XC_BEGIN <= x < XC_END for x in r['refs']):
            not_c2.append(r)
            continue
        if r['size'] < 5:
            unhookable.append(r)
            continue
        funcs.append(r)
    results = []
    for r in funcs:
        a = r['address']
        t = Translator(exe, names, sizes, a, r['size'])
        try:
            if a in MANUAL:
                raise Manual('listed as manual')
            body = t.translate()
            r['manual'] = None
        except Manual as e:
            body = None
            r['manual'] = str(e)
            # the callees and globals of a manual function still come from its code
            o = exe.off(a)
            for i in exe.md.disasm(exe.data[o:o + r['size']], a):
                if i.mnemonic in ('call', 'jmp') and i.op_str.startswith('0x'):
                    tg = int(i.op_str, 16)
                    if not (a <= tg < a + r['size']):
                        t.calls.setdefault((tg, '?', -1), i.address)
                for mm in re.finditer(r'(dword|byte) ptr \[(0x[0-9a-f]+)\]', i.op_str):
                    ga = int(mm.group(2), 16)
                    if XC_END <= ga < 0x718000:
                        t.glob(ga, 4 if mm.group(1) == 'dword' else 1, read=True, write=True)
        r['t'] = t
        r['body'] = body
        results.append(r)
    if '--list' in sys.argv:
        for r in results:
            print(f"{r['address']:#x} {r['name']} {r['size']} {'manual: ' + r['manual'] if r['manual'] else 'generated'}")
        return
    manual = [r for r in results if r['manual']]
    unlisted = [r for r in manual if r['address'] not in MANUAL]
    print(f'C2 rows {len(rows)}: functions {len(funcs)}, generated {len(results) - len(manual)}, manual {len(manual)}, '
          f'not hookable {len(unhookable)}, not C2 {len(not_c2)}')
    for r in unlisted:
        print(f"  manual, not in MANUAL: {r['address']:#x} {r['name']}: {r['manual']}")
    write_outputs(exe, names, results, unhookable, not_c2)


def ctype_return(r) -> bool:
    t = r['t']
    if r['manual']:
        return True
    return any(m for m in t.masks)


def write_outputs(exe, names, results, unhookable, not_c2):
    src = ROOT / 'reimpl/src'
    for mod, lo, hi in MODULES:
        fs = [r for r in results if lo <= r['address'] < hi]
        code = [f'// Generated by tools/static_init_gen.py from Zax.exe; do not edit (edit the tool or',
                f'// static_c2_manual.cpp). Batch C2 of phase 5a (docs/DECOMPILATION.md 10): the static initializers,',
                f'// atexit destructors and .data function pointers of {lo:#x}-{hi - 1:#x}, translated from their',
                f'// code: constants, the calls with their conventions and arguments, the stores to globals in the',
                f"// original's order; eax as the original leaves it.",
                '#include "static_c2.h"', '', 'namespace zax {', '',
                '// written by hand (static_c2_manual.cpp)']
        for r in fs:
            if r['manual']:
                ret = 'void' if MANUAL.get(r['address'], {}).get('mask', 1) == 0 else 'DWORD'
                code.append(f"{ret} __cdecl {r['name']}();")
        code += ['', 'namespace {', '', 'using namespace sinit;', '']
        for r in fs:
            if r['manual']:
                continue
            t = r['t']
            ret = ctype_return(r)
            body = tidy(r['body'], ret)
            mask = 0
            for m in t.masks:
                mask = m if mask == 0 else (mask & m)
            if any(m == 0 for m in t.masks):
                mask = 0
            r['mask'] = mask
            code.append(f"// {r['address']:#x}, {r['size']} bytes, {KIND_FIX.get(r['address'], r['kind'])}")
            code.append(f"{'DWORD' if ret else 'void'} __cdecl {r['name']}()")
            code.append('{')
            code += ['    ' + x for x in body]
            code.append('}')
        code += ['', '}  // namespace', '', f'ZAX_HOOKS({mod})', '{', '    const Hook list[] = {']
        for r in fs:
            code.append(hook_entry(exe, r['address'], r['name']))
        code += ['    };', '    return emit_hooks(out, list);', '}', '', '}  // namespace zax', '']
        (src / f'{mod}.cpp').write_text('\n'.join(code))
        write_checks(mod, lo, hi, fs, names, [u for u in unhookable if lo <= u['address'] < hi])


def stub_kind(target: int, t: Translator, r) -> str:
    if target == OP_NEW:
        return 'n'
    # a call result the function dereferences: a block of the harness
    for tmp, tg in t.tmp_call.items():
        if tg == target and tmp in t.deref_tmp:
            return 'n'
    return 'v'


def write_checks(mod, lo, hi, fs, names, unhook):
    stubsets: dict[tuple, str] = {}
    lines = [f'// Generated by tools/static_init_gen.py; do not edit. The checks of {mod}.cpp (batch C2 of',
             '// phase 5a, docs/DECOMPILATION.md 10) on the harness checks_static_c2.h: per function its callees',
             '// as recording stand-ins (convention and arguments from the code), its globals saved, set per case,',
             '// compared and put back.',
             '#include "checks_static_c2.h"', '', 'namespace zax {', '', f'int hooks_{mod}(Hook *out);', '',
             'namespace {', '', 'using namespace uic;', 'using namespace sc2;', '']
    tests = []
    for r in fs:
        t = r['t']
        stubs = []
        seen = set()
        for (tg, conv, n), _ in sorted(t.calls.items(), key=lambda kv: kv[1]):
            if tg in seen:
                continue
            seen.add(tg)
            kind = None
            if conv == '?':
                over = MANUAL.get(r['address'], {}).get('callees', {})
                conv, n = over[tg][:2] if tg in over else manual_conv(tg)
                kind = over[tg][2] if tg in over else None
            cv = {'this': 'CV_THIS', 'std': 'CV_STD', 'cdecl': 'CV_CDECL', 'fastx': 'CV_FAST'}[conv]
            nm = names.get(tg, f'sub_{tg:x}').replace('"', '')
            extra = ', "xx"' if conv == 'fastx' else ''
            stubs.append(f'{{{tg:#x}, {cv}, {n}, \'{kind or stub_kind(tg, t, r)}\', "{nm}"{extra}}}')
        key = tuple(stubs)
        if key not in stubsets:
            sname = f'S{len(stubsets)}'
            stubsets[key] = sname
            lines.append(f'const Stub {sname}[] = {{{", ".join(stubs) if stubs else "{0, CV_CDECL, 0, K_VALUE, nullptr}"}}};')
        gl = []
        for a, (size, rd, wr, deref, _) in sorted(t.globals.items()):
            kind = 'p' if deref else 'v'
            gl.append(f'{{{a:#x}, {size}, \'{kind}\'}}')
        man = MANUAL.get(r['address'], {})
        for ea, es in man.get('extra', {}).items():
            gl.append(f"{{{ea:#x}, {es}, 'v'}}")
        for (ra, rn) in man.get('ranges', []):
            gl.append(f'{{{ra:#x}, {rn:#x}, \'r\'}}')
        gname = f'G_{r["address"]:x}'
        if gl:
            lines.append(f'const SGlobal {gname}[] = {{{", ".join(gl)}}};')
        mask = r.get('mask', 0xffffffff) if not r['manual'] else man.get('mask', 0xffffffff)
        has_new = any(tg == OP_NEW for (tg, _, _) in t.calls)
        branchy = r['manual'] or bool(t.tested_tmp) or len(t.masks) > 1
        count = 8 if branchy else 3
        setup = man.get('setup') or 'nullptr'
        oa = 'true' if man.get('object_args') else 'false'
        nstub = len(stubs)
        tests.append(f'    {{"{r["name"]}", {r["address"]:#x}, {mask:#x}u, {count}, {gname if gl else "nullptr"}, '
                     f'{len(gl)}, {stubsets[key]}, {nstub}, {setup}, {oa}, {"true" if has_new else "false"}}},')
    lines += ['', 'const STest TESTS[] = {'] + tests + ['};']
    lines.append('// Not hookable (a single ret): identical by their bytes, read in the running exe')
    lines.append('const DWORD RETS[] = {' + ', '.join(f'{u["address"]:#x}' for u in unhook) + ('' if unhook else '0') + '};')
    lines += ['', '}  // namespace', '', f'ZAX_CHECKS({mod})', '{', '    (void)cls;', '    (void)n;',
              f'    run_static_tests("static C2 {lo:#x}-{hi - 1:#x}: initializers, atexit destructors, .data pointers", &hooks_{mod}, '
              f'TESTS, sizeof TESTS / sizeof TESTS[0], RETS, {len(unhook)}, {lo // 0x100 + 7});', '}', '', '}  // namespace zax', '']
    (ROOT / f'reimpl/src/checks_{mod}.cpp').write_text('\n'.join(lines))


_EXE = None


def manual_conv(target: int):
    """Convention of a manual function's callee: thiscall when the callee reads ecx before it sets it
    is not decidable from the bytes in general; the manual table of the tool decides (MANUAL_CALLEES)."""
    return MANUAL_CALLEES[target]


# The callees of the manual functions with their conventions (from the callees' code and their callers)
MANUAL_CALLEES: dict[int, tuple[str, int]] = {
    0x4a45f0: ('fastx', 0),   # ConfigPath: ecx a local ZString of the caller (not compared)
    0x4a8a90: ('this', 0), 0x4a8b00: ('this', 0), 0x4b1d20: ('this', 0), 0x4dda00: ('this', 1),
    0x4dec90: ('this', 0), 0x4defd0: ('this', 0), 0x4e13a0: ('fastx', 0), 0x506470: ('cdecl', 0),
    0x5065e0: ('this', 2), 0x545ca0: ('this', 0), 0x55c2c0: ('this', 0), 0x55d0c0: ('this', 0),
    0x57aef0: ('cdecl', 0), 0x57af60: ('this', 1), 0x591cd0: ('this', 5), 0x5d0320: ('cdecl', 1),
    0x5d04b0: ('cdecl', 3), 0x5d0863: ('cdecl', 1),
}


if __name__ == '__main__':
    main()
