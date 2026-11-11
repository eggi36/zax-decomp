#!/usr/bin/env python3
"""The static audit (docs/DECOMPILATION.md 5): every function of docs/reimpl.csv compared with the original by its
calls and constants, without running anything.

The exe side, per function (its code from a recursive descent from its start, as tools/audit.py descends: branches,
jump tables, a jump to another table function is a tail call; the descent stops at every other start of
docs/functions.csv and docs/reimpl.csv):
- calls: direct call and tail-jump targets and code addresses used as values (a function pointer pushed or stored);
  the import thunks and `call [IAT]` by the import's name; `call [reg + off]` as a vtable slot offset; `call [G]`
  through a global pointer as the global G;
- constants: immediates that are not trivial (|v| < MIN_CONST) and not stack frame arithmetic (`sub esp`, `ret n`,
  [esp/ebp + d]); field offsets ([reg + d]) as their own set; floats and doubles an x87 instruction loads from .rdata
  by their value; global data addresses (.rdata, .data, .bss, vtables), strings resolved to their text.

The reimplementation side: the function a hook entry (`ZAX_HOOKS`, `*_hooks.inc`) names, found by its name in its
module (the module itself first, then the headers it includes, then any other file). Its body is read token by token
(comments dropped); calls of helpers that are no hook target (static functions, inline helpers of the headers,
function-like macros) are expanded into it, their parameters bound to the call's literal arguments, so the helper's
own calls and constants count as the function's (the exe inlines what those helpers stand for, or calls it, which the
address the helper passes shows). A call of a hooked function by its C++ name is a call of its exe address. Every
literal is classified by its value: an address of .text is code, of the IAT an import, of the other sections a
global; numbers outside the image are constants. Named constants (constexpr, const, enum, #define) are resolved to
their value; `vcall(obj, slot)` is a vtable slot; string literals are strings (not those of the reimplementation's
own log()); inline assembler is read for its hex numbers.

Then the differences per function, each either matched, explained by one of RULES (mechanical, certain) or open.
An open difference may carry a hint (a likely reason the tool cannot prove). The output goes outside the repository:

    .venv/bin/python -I tools/static_audit.py [--exe PATH] [--out DIR] [--only 0x...]

    functions.jsonl   one line per function: both sides' sets, the explained and the open differences
    summary.md        counts per subsystem and file, the rules and how many differences each explained
    partitions.json   the open differences in PARTS parts of about equal review work, by subsystem and module

The exe is --exe, else ZAX_EXE, else gamedata/Zax - German/Zax.exe of this repository (as the other tools);
the output folder is --out, else runs/static-audit/ of the workspace (the folder the repositories sit in).
"""
from __future__ import annotations

import argparse
import bisect
import collections
import csv
import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

import capstone
from capstone import x86 as X
import pefile

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'reimpl/src'
sys.path.insert(0, str(ROOT / 'tools'))
import crt_asm_gen  # noqa: E402

MIN_CONST = 0x10        # immediates with |v| below this are trivial (indices, shifts, small counts, booleans)
PARTS = 6
SHIFTS = {'shl', 'sal', 'shr', 'sar', 'rol', 'ror', 'rcl', 'rcr', 'shld', 'shrd'}
EXPAND_DEPTH = 8        # helper expansion depth
EXCEPT_HANDLER3 = 0x5d2304
THUNKS = (0x5caa58, 0x5caa90)   # the linker's import thunks (tools/audit.py GAPS)

RULES = {
    'alias': 'the C++ names a function hooked at several exe addresses (one body, e.g. ClassInfo and ClassInfo2); a '
             'reference to any of them matches',
    'identical_code': 'the other side refers to another exe function whose bytes are identical (same size in '
                      'docs/functions.csv, same bytes)',
    'callee_inlined_cpp': 'the exe calls X; the C++ does not, but every call, import, vtable slot and global of X\'s '
                          'exe code (at least two) appears in the C++ function: X is inlined there; X\'s own calls, '
                          'globals and constants are explained with it',
    'callee_inlined_exe': 'the C++ calls X; the exe does not, but every call, import, vtable slot and global of X\'s '
                          'exe code (at least two) appears in the exe function: the exe inlined X; X\'s own calls, '
                          'globals and constants are explained with it',
    'devirtualized': 'one side calls vtable slot s, the other calls X directly, and X is slot s of a vtable of the exe',
    'intrinsic': 'the C++ calls a runtime function the exe compiled inline (memcpy/memmove: rep movs; memset: rep '
                 'stos; strlen/strcpy/strcat/strcmp: repne scas/rep movs/repe cmps; fabs/sqrt/sin/cos/atan2/tan/log: '
                 'fabs/fsqrt/fsin/fcos/fpatan/fptan/fyl2x)',
    'crt_by_name': 'the C++ calls a C runtime function by its name; the exe calls the function of that name '
                   '(docs/functions.csv)',
    'stack_probe': 'the exe calls _chkstk for a frame of 4 KiB or more: the compiler\'s stack probe',
    'seh_frame': 'the exe\'s __try/C++ unwind frame (push -1; push scope table or unwind handler; '
                 'push _except_handler3 or mov eax, fs:[0]): the frame\'s table, handler and constants',
    'folded': 'the value is the C++\'s constant expression of literals and named constants, folded '
              '(e.g. ~0x10, 1 << 12, 0x100 - 1); the expression\'s operands are explained with it',
    'negated': 'add with the negated value (add r, -c) for a subtraction by c, or the other way round',
    'compare_bound': 'cmp with v and a jump on <= or > (or < and >=) for the C++\'s comparison with v + 1 (v - 1)',
    'div_magic': 'the multiplier of a division by a constant d of the C++ (MSVC\'s magic number for signed or unsigned '
                 'division by d)',
    'fpu_flags': 'an immediate of test/and/cmp/xor within three instructions after fnstsw: the status word bits of '
                 'a float comparison, which the C++ writes as a comparison',
    'width_mask': 'and with 0xff, 0xffff or 0xff00: the zero-extension/byte handling of a narrower type',
    'float_bits': 'an integer immediate that is the IEEE bits of a float or double literal of the other side',
    'float_by_value': 'a float constant of .rdata the exe loads, written in the C++ as a literal or named constant of '
                      'the same value (exact at the load\'s width), or the other way round',
    'float_same_value': 'both sides load a float constant of the same value from different addresses of .rdata',
    'x87_builtin': '0.0, 1.0, pi, log2(10), log2(e), log10(2), ln(2) the exe loads with fldz/fld1/fldpi/fldl2t/'
                   'fldl2e/fldlg2/fldln2',
    'string_by_text': 'a string of the exe written as a C++ string literal with the same text, or the same text at '
                      'another address',
    'global_block': 'the exe address lies in the same block of docs/exe_layout.csv as an address the C++ uses, and '
                    'the difference is a literal of the C++ (a member of a global struct or array)',
    'slot_index': 'the C++ literal is a vtable slot index the C++ calls (vcall), the exe has it as the call\'s offset',
    'field_in_exe': 'the C++ literal is a field offset ([reg + d]) of the exe function (the C++ writes field accesses '
                    'as offsets, the exe as displacements: a match, not a difference)',
}

INTRINSIC = {
    'memcpy': {'movs'}, 'memmove': {'movs'}, 'memset': {'stos'}, 'strlen': {'scas'}, 'strcpy': {'movs', 'scas'},
    'strcat': {'movs', 'scas'}, 'strcmp': {'cmps', 'scas'}, 'memcmp': {'cmps'}, 'fabs': {'fabs'}, 'fabsf': {'fabs'},
    'sqrt': {'fsqrt'}, 'sqrtf': {'fsqrt'}, 'sin': {'fsin'}, 'sinf': {'fsin'}, 'cos': {'fcos'}, 'cosf': {'fcos'},
    'atan2': {'fpatan'}, 'atan': {'fpatan'}, 'tan': {'fptan'}, 'log': {'fyl2x'}, 'log10': {'fyl2x'},
    '__builtin_memcpy': {'movs'}, '__builtin_memset': {'stos'}, '__builtin_fabsf': {'fabs'}, '__builtin_fabs': {'fabs'},
    '__builtin_sqrtf': {'fsqrt'}, '__builtin_sqrt': {'fsqrt'},
}
X87_BUILTIN = {'fldz': 0.0, 'fld1': 1.0, 'fldpi': 3.141592653589793, 'fldl2t': 3.321928094887362,
               'fldl2e': 1.4426950408889634, 'fldlg2': 0.3010299956639812, 'fldln2': 0.6931471805599453}

KEYWORDS = set('''if for while switch catch return sizeof alignof alignas decltype static_assert do else case default
new delete throw typeid noexcept operator template typename using namespace struct class union enum public private
protected static const constexpr consteval constinit inline volatile extern unsigned signed int char short long float
double void bool auto register goto break continue this true false nullptr reinterpret_cast static_cast const_cast
dynamic_cast __declspec __attribute__ __asm__ asm __asm __cdecl __stdcall __fastcall __thiscall THISCALL STDCALL WINAPI
virtual override final friend mutable explicit typedef'''.split())
ASM_WORDS = {'__asm__', 'asm', '__asm'}
# the address layer (reimpl.h): its literal argument is the address; its own body is the layer's mechanics
LAYER = set('''at fn code glob vt vt_ptr imp imp_slot glob_addr fn_addr fn_exe fn_dyn glob_dyn glob_addr_dyn fn_addr_dyn
vt_dyn key_address key_vt key_glob_addr key_fn_addr original original_address original_or_dead exe_or_dead import_thunk
exe_address_of exe_code_of translate own_of find fn_index is_data dead_path log ptr'''.split())
# C++ helpers that stand for a function of the exe the original calls (the same operation, written inline)
STANDS_FOR = {'x87_ftol': 0x5d0488}   # __ftol: truncating float-to-integer conversion


def u32(v):
    return v & 0xffffffff


def s32(v):
    v &= 0xffffffff
    return v - (1 << 32) if v & 0x80000000 else v


def f32_bits(x):
    try:
        return struct.unpack('<I', struct.pack('<f', x))[0]
    except (OverflowError, struct.error):
        return None


def as_f32(x):
    try:
        return struct.unpack('<f', struct.pack('<f', x))[0]
    except (OverflowError, struct.error):
        return None


def workspace_runs():
    """runs/ of the workspace: next to this repository, or, in a git worktree, next to its main checkout."""
    if (ROOT.parent / 'runs').is_dir():
        return ROOT.parent / 'runs'
    common = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                            capture_output=True, text=True).stdout.strip()
    return Path(common).parent.parent / 'runs' if common else ROOT.parent / 'runs'


# ---- the exe ------------------------------------------------------------------------------------------------------

class ExeModel:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        pe = pefile.PE(data=self.data)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.secs = {s.Name.rstrip(b'\0').decode(): (self.base + s.VirtualAddress, s.Misc_VirtualSize,
                                                      s.PointerToRawData, s.SizeOfRawData) for s in pe.sections}
        self.lo = self.base
        self.hi = max(lo + size for lo, size, _, _ in self.secs.values())
        self.text = self.secs['.text'][:2]
        self.iat = {}
        for d in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
            for imp in d.imports:
                nm = imp.name.decode() if imp.name else f'#{imp.ordinal}'
                self.iat[imp.address] = nm
        self.iat_lo, self.iat_hi = min(self.iat), max(self.iat) + 4
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self._cache = {}
        # import thunks: jmp [slot]
        self.thunk = {}
        a = THUNKS[0]
        while a < THUNKS[1]:
            i = self.insn(a)
            if i and i.mnemonic == 'jmp' and i.operands[0].type == X.X86_OP_MEM:
                slot = u32(i.operands[0].mem.disp)
                if slot in self.iat:
                    self.thunk[a] = self.iat[slot]
            a += i.size if i else 1

    def off(self, va):
        for lo, size, raw, rsize in self.secs.values():
            if lo <= va < lo + size and va - lo < rsize:
                return raw + va - lo
        return None

    def section(self, va):
        for name, (lo, size, _, _) in self.secs.items():
            if lo <= va < lo + size:
                return name
        return None

    def read(self, va, n):
        o = self.off(va)
        return self.data[o:o + n] if o is not None else None

    def insn(self, a):
        if a in self._cache:
            return self._cache[a]
        o = self.off(a)
        i = next(self.md.disasm(self.data[o:o + 16], a), None) if o is not None else None
        self._cache[a] = i
        return i

    def string(self, va):
        o = self.off(va) if va else None
        if o is None:
            return None
        e = self.data.find(b'\0', o, o + 600)
        if e < 0:
            return None
        s = self.data[o:e]
        # printable ASCII, tabs and line ends, and the Latin-1 letters of the German texts
        if not s or not all(32 <= c < 127 or c in (9, 10, 13) or 0xc0 <= c <= 0xfe for c in s):
            return None
        if len(s) <= 3:
            # three bytes and the terminating zero: as often a pointer of a vtable or table as a short text
            v = struct.unpack_from('<I', self.data, o)[0]
            if self.lo <= v < self.hi or not all(32 <= c < 127 for c in s):
                return None
        return s.decode('latin-1')

    def in_text(self, v):
        return self.text[0] <= v < self.text[0] + self.text[1]


class Layout:
    """docs/exe_layout.csv: blocks and vtables."""
    def __init__(self):
        self.rows = list(csv.DictReader(open(ROOT / 'docs/exe_layout.csv')))
        self.block_of = {}
        self.vtables = {}
        self.text_data = []
        starts = []
        for r in self.rows:
            a, n = int(r['address'], 16), int(r['size'] or 0)
            starts.append((a, n, int(r['block'], 16) if r['block'].startswith('0x') else a, r['kind']))
            if r['kind'] == 'vtable':
                self.vtables[a] = n
            if r['kind'] == 'text data':
                self.text_data.append((a, a + n))
        starts.sort()
        self.starts = starts
        self.keys = [s[0] for s in starts]

    def block(self, a):
        i = bisect.bisect_right(self.keys, a) - 1
        if i >= 0:
            s, n, b, _ = self.starts[i]
            if a < s + max(n, 1):
                return b
        return None


def exe_functions(exe, starts, sizes, layout):
    """per start: the exe side's sets (see the module's docstring)."""
    tlo, tsz = exe.text
    thi = tlo + tsz
    out = {}
    td = layout.text_data

    def classify_addr(v):
        if exe.iat_lo <= v < exe.iat_hi:
            return 'import'
        if tlo <= v < thi:
            if any(s <= v < e for s, e in td):
                return 'global'
            return 'code'
        if exe.lo <= v < exe.hi:
            return 'global'
        return None

    for s in sorted(starts):
        f = {'code': {}, 'imports': set(), 'vslots': set(), 'globals': {}, 'consts': {}, 'fields': set(),
             'floats': {}, 'markers': set(), 'seh': set(), 'cmp': [], 'addsub': set(), 'stack': set(),
             'gptr': set(), 'ninsn': 0, 'fpu_masks': set(), 'width': set()}
        seen = set()
        work = [s]
        # a frame pointer: mov ebp, esp among the first instructions; otherwise ebp is a register like the others
        fp, a = False, s
        for _ in range(4):
            i = exe.insn(a)
            if not i:
                break
            if i.mnemonic == 'mov' and i.op_str == 'ebp, esp':
                fp = True
                break
            a += i.size
        stack_regs = (X.X86_REG_ESP, X.X86_REG_EBP) if fp else (X.X86_REG_ESP,)
        prev = []   # the last instructions (mnemonic, op_str) in linear order of the current run
        while work:
            a = work.pop()
            prev = []
            while tlo <= a < thi and a not in seen and not (a != s and a in starts):
                seen.add(a)
                i = exe.insn(a)
                if not i:
                    break
                f['ninsn'] += 1
                mn, ops = i.mnemonic, i.operands
                prefix = i.prefix[0] if i.prefix else 0
                if mn in ('movsb', 'movsd', 'movsw') or mn.startswith('rep movs'):
                    f['markers'].add('movs')
                if 'stos' in mn:
                    f['markers'].add('stos')
                if 'scas' in mn:
                    f['markers'].add('scas')
                if 'cmps' in mn:
                    f['markers'].add('cmps')
                if prefix in (0xf2, 0xf3):
                    for k in ('movs', 'stos', 'scas', 'cmps'):
                        if k in mn:
                            f['markers'].add(k)
                if mn in ('fabs', 'fsqrt', 'fsin', 'fcos', 'fpatan', 'fptan', 'fyl2x', 'fsincos', 'frndint', 'f2xm1',
                          'fscale', 'fistp'):
                    f['markers'].add(mn)
                if mn in X87_BUILTIN:
                    f['floats'].setdefault(('builtin', mn), X87_BUILTIN[mn])
                after_fnstsw = any(p[0] == 'fnstsw' for p in prev[-3:])
                # SEH / unwind frame: push -1; push X; push _except_handler3 | mov eax, fs:[0]
                if mn == 'push' and len(prev) >= 1 and prev[-1] == ('push', '0xffffffff') and ops and ops[0].type == X.X86_OP_IMM:
                    f['seh'].add(u32(ops[0].imm))
                if mn == 'push' and ops and ops[0].type == X.X86_OP_IMM and u32(ops[0].imm) == EXCEPT_HANDLER3:
                    f['seh'].add(EXCEPT_HANDLER3)
                is_fpu = mn.startswith('f')
                for k, op in enumerate(ops):
                    if op.type == X.X86_OP_IMM:
                        v = u32(op.imm)
                        if mn in ('ret', 'retn', 'enter') or mn.startswith('j') or mn.startswith('loop'):
                            continue
                        if mn == 'call':
                            continue
                        if mn in SHIFTS:
                            continue   # a shift count (the compiler's multiplications and divisions)
                        if mn in ('add', 'sub', 'and') and ops[0].type == X.X86_OP_REG and ops[0].reg in (X.X86_REG_ESP,):
                            f['stack'].add(v)
                            continue
                        kind = classify_addr(v)
                        if kind == 'code':
                            f['code'].setdefault(v, 'ref')
                        elif kind == 'import':
                            f['imports'].add(exe.iat.get(v & ~3, f'iat_{v:#x}'))
                        elif kind == 'global':
                            f['globals'].setdefault(v, 'ref')
                        else:
                            sv = s32(v)
                            if after_fnstsw and mn in ('test', 'and', 'cmp', 'xor'):
                                f['fpu_masks'].add(v)
                            if mn == 'and' and v in (0xff, 0xffff, 0xff00):
                                f['width'].add(v)
                            if mn in ('add', 'sub'):
                                f['addsub'].add((mn, sv))
                            if abs(sv) >= MIN_CONST:
                                f['consts'].setdefault(v, mn)
                            if mn == 'cmp':
                                f['cmp'].append((v, a))
                    elif op.type == X.X86_OP_MEM:
                        m = op.mem
                        disp = u32(m.disp)
                        if m.segment != 0:
                            continue   # fs:[0]
                        if m.base == 0 and m.index == 0:
                            kind = classify_addr(disp)
                            if kind == 'import':
                                f['imports'].add(exe.iat.get(disp & ~3, f'iat_{disp:#x}'))
                                continue
                            if kind == 'global':
                                if is_fpu and exe.section(disp) == '.rdata' and op.size in (4, 8, 10):
                                    b = exe.read(disp, op.size)
                                    if b and op.size == 4:
                                        val = struct.unpack('<f', b)[0]
                                    elif b and op.size == 8:
                                        val = struct.unpack('<d', b)[0]
                                    else:
                                        val = None
                                    f['floats'][('addr', disp, op.size)] = val
                                f['globals'].setdefault(disp, 'call' if mn == 'call' else 'mem')
                                if mn == 'call':
                                    f['gptr'].add(disp)
                            elif kind == 'code':
                                f['code'].setdefault(disp, 'mem')
                            continue
                        if m.base in stack_regs and m.index == 0:
                            f['stack'].add(s32(disp))
                            continue
                        if m.base == 0 and m.index != 0:
                            kind = classify_addr(disp)
                            if kind == 'global':
                                f['globals'].setdefault(disp, 'table')
                            continue   # a jump table in .text: part of the function
                        kind = classify_addr(disp)
                        if kind == 'global':
                            f['globals'].setdefault(disp, 'indexed')
                            continue
                        if mn == 'call' and m.index == 0:
                            f['vslots'].add(s32(disp))
                            continue
                        if disp:
                            f['fields'].add(s32(disp))
                prev.append((mn, i.op_str))
                if mn in ('ret', 'retf', 'int3', 'hlt', 'ud2'):
                    break
                if mn == 'call':
                    if ops and ops[0].type == X.X86_OP_IMM:
                        t = u32(ops[0].imm)
                        if t in exe.thunk:
                            f['imports'].add(exe.thunk[t])
                        else:
                            f['code'][t] = 'call'
                    elif ops and ops[0].type == X.X86_OP_MEM and ops[0].mem.base != 0 and ops[0].mem.index == 0 \
                            and ops[0].mem.disp == 0 and not exe.lo <= u32(ops[0].mem.disp) < exe.hi:
                        f['vslots'].add(0)
                if mn == 'jmp':
                    if ops[0].type == X.X86_OP_IMM:
                        t = u32(ops[0].imm)
                        if t in exe.thunk:
                            f['imports'].add(exe.thunk[t])
                            break
                        if t in starts and t != s:
                            f['code'][t] = 'tail'
                            break
                        a = t
                        continue
                    if ops[0].type == X.X86_OP_MEM and ops[0].mem.index != 0 and ops[0].mem.base == 0:
                        q = u32(ops[0].mem.disp)
                        while tlo <= q < thi:
                            v = struct.unpack_from('<I', exe.data, exe.off(q))[0]
                            if not tlo <= v < thi or (v in starts and v != s):
                                break
                            work.append(v)
                            q += 4
                    elif ops[0].type == X.X86_OP_MEM and ops[0].mem.base != 0 and ops[0].mem.index == 0:
                        f['vslots'].add(s32(ops[0].mem.disp))
                    break
                if (mn.startswith('j') or mn.startswith('loop')) and ops and ops[0].type == X.X86_OP_IMM:
                    t = u32(ops[0].imm)
                    if t in starts and t != s:
                        f['code'][t] = 'tail'
                    else:
                        work.append(t)
                a += i.size
            if tlo <= a < thi and a != s and a in starts and a not in seen:
                f['code'].setdefault(a, 'fallthrough')
        # the cmp's jump: the next instruction after each cmp
        cmps = []
        for v, a in f['cmp']:
            i = exe.insn(a)
            j = exe.insn(a + i.size) if i else None
            cmps.append((v, j.mnemonic if j else ''))
        f['cmp'] = cmps
        f['code'].pop(s, None)
        out[s] = f
    return out


# ---- the C++ ------------------------------------------------------------------------------------------------------

TOKEN = re.compile(r'''
    (?P<ws>\s+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<pp>^[ \t]*\#(?:[^\n\\]|\\.|\\\n)*)
  | (?P<raw>(?:u8|u|U|L)?R"(?P<delim>[^(\s]*)\((?:.|\n)*?\)(?P=delim)")
  | (?P<str>(?:u8|u|U|L)?"(?:[^"\\\n]|\\.)*")
  | (?P<chr>(?:u8|u|U|L)?'(?:[^'\\\n]|\\.)+')
  | (?P<num>(?:0[xX][0-9a-fA-F']+(?:\.[0-9a-fA-F]*)?(?:[pP][+-]?\d+)?|\d[\d']*\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)[uUlLfF]*)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op>::|->|\.\.\.|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||\+\+|--|[-+*/%&|^~!=<>?:;,.(){}\[\]\#])
''', re.X | re.S | re.M)

ESC = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', '\\': '\\', '"': '"', "'": "'", 'a': '\a', 'b': '\b',
       'f': '\f', 'v': '\v', '?': '?'}


def unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c != '\\':
            out.append(c)
            i += 1
            continue
        n = s[i + 1] if i + 1 < len(s) else ''
        if n == 'x':
            m = re.match(r'[0-9a-fA-F]+', s[i + 2:])
            out.append(chr(int(m.group(), 16) & 0xff) if m else 'x')
            i += 2 + (len(m.group()) if m else 0)
        elif n in '01234567':
            m = re.match(r'[0-7]{1,3}', s[i + 1:])
            out.append(chr(int(m.group(), 8) & 0xff))
            i += 1 + len(m.group())
        else:
            out.append(ESC.get(n, n))
            i += 2
    return ''.join(out)


def parse_number(t):
    """(kind, value): ('int', n) or ('float', x, width) or None."""
    s = t.replace("'", '')
    low = s.lower()
    if low.startswith('0x') and not ('.' in low or 'p' in low):
        return ('int', int(re.sub(r'[uUlL]+$', '', s), 16))
    m = re.match(r'^(\d+)[uUlL]*$', s)
    if m:
        d = m.group(1)
        return ('int', int(d, 8) if len(d) > 1 and d.startswith('0') else int(d))
    try:
        if low.startswith('0x'):
            x = float.fromhex(re.sub(r'[fFlL]+$', '', s))
        else:
            x = float(re.sub(r'[fFlL]+$', '', s))
    except ValueError:
        return None
    width = 4 if low.endswith('f') else (10 if low.endswith('l') else 8)
    return ('float', x, width)


class Tok:
    __slots__ = ('k', 't', 'v', 'line')

    def __init__(self, k, t, v=None, line=0):
        self.k, self.t, self.v, self.line = k, t, v, line


def tokenize(text):
    toks, pp = [], []
    line = 1
    for m in TOKEN.finditer(text):
        kind = m.lastgroup if m.lastgroup != 'delim' else 'raw'
        g = m.group()
        if kind in ('ws', 'lc', 'bc'):
            line += g.count('\n')
            continue
        if kind == 'pp':
            pp.append((line, g))
            line += g.count('\n')
            continue
        if kind == 'raw':
            body = g[g.index('(') + 1:g.rindex(')')]
            toks.append(Tok('str', g, body, line))
            line += g.count('\n')
            continue
        if kind == 'str':
            body = g[g.index('"') + 1:-1]
            if toks and toks[-1].k == 'str' and not toks[-1].t.endswith('R'):
                toks[-1].v += unescape(body)   # adjacent literals concatenate
                continue
            toks.append(Tok('str', g, unescape(body), line))
            continue
        if kind == 'chr':
            body = unescape(g[g.index("'") + 1:-1])
            v = 0
            for c in body:
                v = (v << 8) | (ord(c) & 0xff)
            toks.append(Tok('num', g, ('int', v), line))
            continue
        if kind == 'num':
            toks.append(Tok('num', g, parse_number(g), line))
            continue
        toks.append(Tok(kind, g, None, line))
    return toks, pp


class Def:
    __slots__ = ('name', 'qual', 'file', 'line', 'toks', 'body', 'params', 'static', 'macro', 'tparams', 'ptoks')

    def __init__(self, name, qual, file, line, toks, body, params, static=False, macro=False, tparams=()):
        self.name, self.qual, self.file, self.line = name, qual, file, line
        self.toks, self.body, self.params, self.static, self.macro = toks, body, params, static, macro
        self.tparams = list(tparams)
        self.ptoks = []


SKIP_BEFORE_BRACE = {'const', 'noexcept', 'override', 'final', 'volatile', '&', '&&', 'mutable'}


def param_names(toks):
    """the names of a parameter list's tokens (between the parentheses)."""
    out, cur, depth = [], [], 0
    for t in toks + [Tok('op', ',')]:
        if t.t in ('(', '<', '[', '{'):
            depth += 1
        elif t.t in (')', '>', ']', '}'):
            depth -= 1
        if t.t == ',' and depth == 0:
            name = None
            ids = [x for x in cur if x.k == 'id']
            # a default argument: the name is the identifier before '='
            if any(x.t == '=' for x in cur):
                k = next(j for j, x in enumerate(cur) if x.t == '=')
                ids = [x for x in cur[:k] if x.k == 'id']
            if ids and not (len(ids) == 1 and ids[0].t in KEYWORDS) and len([x for x in cur if x.k == 'id']) >= 1:
                cand = ids[-1].t
                if cand not in KEYWORDS and (len(ids) > 1 or any(x.t in ('*', '&') for x in cur)):
                    name = cand
            out.append(name)
            cur = []
        else:
            cur.append(t)
    if out == [None] and not toks:
        return []
    return out


def strip_inactive(text, defined):
    """blank the lines of `#if 0` and of `#ifdef X` / `#if defined(X)` for a name X that nothing defines (the
    libraries' DEBUG and tracing code); every other conditional keeps both branches."""
    out = []
    stack = []   # per #if: 'skip' (this branch inactive), 'else' (the #else is inactive) or 'both'
    lines = text.split('\n')
    for line in lines:
        m = re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', line)
        if m:
            kw, rest = m.group(1), m.group(2).strip()
            if kw in ('if', 'ifdef', 'ifndef'):
                mm = re.fullmatch(r'(?:defined\s*\(?\s*)?(\w+)\s*\)?', rest) if kw != 'ifndef' else None
                if kw == 'if' and rest == '0':
                    stack.append('skip')
                elif kw == 'ifdef' and rest.split()[0] not in defined or (
                        kw == 'if' and mm and rest.startswith('defined') and mm.group(1) not in defined):
                    stack.append('skip')
                else:
                    stack.append('both')
            elif kw in ('else', 'elif') and stack:
                stack[-1] = 'both' if stack[-1] == 'skip' and kw == 'else' else stack[-1]
            elif kw == 'endif' and stack:
                stack.pop()
            out.append(line if 'skip' not in stack or kw == 'endif' else '')
            continue
        out.append('' if 'skip' in stack else line)
    return '\n'.join(out)


def param_names_t(toks):
    """the names of a template parameter list (typename T, DWORD N, auto F, void (*F)(BYTE *), A... a)"""
    out = []
    for a0, a1 in split_args(toks, 0, len(toks)):
        piece = toks[a0:a1]
        if any(x.t == '=' for x in piece):
            piece = piece[:next(j for j, x in enumerate(piece) if x.t == '=')]
        # a function pointer parameter: the name inside (* name)
        star = [j for j, x in enumerate(piece) if x.t == '*' and j + 1 < len(piece) and piece[j + 1].k == 'id'
                and j > 0 and piece[j - 1].t == '(']
        if star:
            out.append(piece[star[0] + 1].t)
            continue
        ids = [x.t for x in piece if x.k == 'id']
        out.append(ids[-1] if ids else None)
    return out


def match_close(toks, i, o, c):
    d = 0
    for j in range(i, len(toks)):
        if toks[j].t == o:
            d += 1
        elif toks[j].t == c or (o == '<' and toks[j].t == '>>'):
            d -= 2 if toks[j].t == '>>' else 1
            if d <= 0:
                return j
        elif o == '<' and toks[j].t in (';', '{', '}'):
            return j
    return len(toks) - 1


def match_open(toks, i, o, c):
    d = 0
    for j in range(i, -1, -1):
        if toks[j].t == c:
            d += 1
        elif toks[j].t == o:
            d -= 1
            if d == 0:
                return j
    return -1


class Source:
    def __init__(self):
        self.files = {}      # rel name -> tokens
        self.defs = collections.defaultdict(list)   # name -> [Def]
        self.consts = collections.defaultdict(dict)  # file -> name -> value tuple
        self.includes = {}
        self.hooks = []      # (address, name, file)
        self.inc_owner = {}
        self.texts = {}
        self.sizes = {}      # type name -> (file, size expression) from static_assert(sizeof(T) == n)
        self.aliases = {}    # file -> alias -> type (using A = B;)
        self.classes = {}    # class name -> [(tokens, body start, body end)]
        self.macros = collections.defaultdict(dict)   # file -> name -> (params, body text)
        paths = sorted(p for p in SRC.iterdir() if p.suffix in ('.cpp', '.h', '.inc') and p.is_file())
        paths += sorted(p for p in (SRC / 'libc').rglob('*.h'))
        paths += sorted(p for p in (SRC / 'libs').glob('*.[ch]'))
        paths += sorted(p for p in (SRC.parent / 'third_party').rglob('*.[ch]'))
        raw = {p: p.read_text(errors='replace') for p in paths}
        # the names any file defines or the build passes (-D): an #ifdef of any other name is inactive
        defined = {'ZAX_STANDALONE', '_WIN32', '_MSC_VER', '__cplusplus', '__clang__', 'WIN32'}
        for text in raw.values():
            defined.update(re.findall(r'^[ \t]*#[ \t]*define[ \t]+(\w+)', text, re.M))
        for p in paths:
            rel = str(p.relative_to(SRC)) if p.is_relative_to(SRC) else str(p.relative_to(SRC.parent))
            text = strip_inactive(raw[p], defined)
            self.texts[rel] = text
            toks, pp = tokenize(text)
            self.files[rel] = toks
            self.includes[rel] = [m.group(1) for _, l in pp for m in [re.match(r'\s*#\s*include\s+"([^"]+)"', l)] if m]
            self._defines(rel, pp)
            self._defs(rel, toks)
            self._consts(rel, toks)
        for f, incs in self.includes.items():
            for i in incs:
                if i.endswith('.inc'):
                    self.inc_owner.setdefault(i, f)
        for rel in list(self.files):
            if rel.endswith('.cpp') and not rel.startswith('checks_'):
                self._macro_defs(rel)
                self._hooks(rel)
        self._closure = {}

    def _defines(self, rel, pp):
        for line, l in pp:
            m = re.match(r'\s*#\s*define\s+(\w+)(\(([^)]*)\))?\s*(.*)$', l.replace('\\\n', ' '), re.S)
            if not m:
                continue
            name, params, body = m.group(1), m.group(3), m.group(4)
            toks, _ = tokenize(body)
            if m.group(2) is not None:
                ps = [x.strip() for x in params.split(',') if x.strip()]
                ps = ['__VA_ARGS__' if x == '...' else x for x in ps]
                self.macros[rel][name] = (ps, body)
                self.defs[name].append(Def(name, name, rel, line, toks, (0, len(toks)), ps, macro=True))
            elif toks:
                self.consts[rel].setdefault(name, ('expr', toks))

    def _defs(self, rel, toks):
        depth_fn = 0
        stack = []   # per '{': ('fn' | 'class' name | None)
        for i, t in enumerate(toks):
            if t.t == '{':
                fn = None
                if depth_fn == 0:
                    fn = self._def_at(rel, toks, i)
                if fn is not None:
                    stack.append(('fn', None))
                    depth_fn += 1
                    name, qual, params, static, tparams = fn[:5]
                    classes = [c for k, c in stack if k == 'class']
                    if '::' not in qual and classes:
                        qual = classes[-1] + '::' + qual
                    e = match_close(toks, i, '{', '}')
                    d = Def(name, qual, rel, t.line, toks, (i + 1, e), params, static, tparams=tparams)
                    d.ptoks = fn[5]
                    self.defs[name].append(d)
                else:
                    # a class body: struct/class/union NAME [final] [: bases] {
                    cls = None
                    k = i - 1
                    while k >= 0 and k > i - 30 and toks[k].t not in (';', '{', '}', ')', '='):
                        if toks[k].t in ('struct', 'class', 'union') and k + 1 < i and toks[k + 1].k == 'id':
                            cls = toks[k + 1].t
                            break
                        k -= 1
                    stack.append(('class', cls) if cls else (None, None))
                    if cls:
                        self.classes.setdefault(cls, []).append((toks, i + 1, match_close(toks, i, '{', '}')))
            elif t.t == '}':
                if stack and stack.pop()[0] == 'fn':
                    depth_fn -= 1

    def _def_at(self, rel, toks, i):
        j = i - 1
        while j >= 0 and toks[j].t in SKIP_BEFORE_BRACE:
            j -= 1
        # a trailing return type: -> T
        k = j
        while k >= 0 and k > j - 8 and toks[k].t != '->' and toks[k].t not in (')', ';', '{', '}'):
            k -= 1
        if k >= 0 and toks[k].t == '->':
            j = k - 1
            while j >= 0 and toks[j].t in SKIP_BEFORE_BRACE:
                j -= 1
        if rel.endswith('.c') and j >= 0 and toks[j].t == ';':
            # a K&R definition: name(a, b) TYPE a; TYPE *b; {
            k = j
            while k >= 0 and k > j - 80 and (toks[k].k in ('id', 'num') or toks[k].t in (';', ',', '*', '[', ']')):
                k -= 1
            if k >= 0 and toks[k].t == ')' and toks[k + 1].k == 'id':
                j = k
        if j < 0 or toks[j].t != ')':
            return None
        o = match_open(toks, j, '(', ')')
        if o <= 0:
            return None
        n = o - 1
        if toks[n].k != 'id' or toks[n].t in KEYWORDS:
            return None
        if n > 0 and toks[n - 1].t in ('.', '->', ',', ':') and toks[n - 1].t != '::':
            return None
        qual = toks[n].t
        q = n
        while q >= 2 and toks[q - 1].t == '::' and toks[q - 2].k == 'id':
            qual = toks[q - 2].t + '::' + qual
            q -= 2
        if q > 0 and toks[q - 1].t == '~':
            return None
        # a declaration before: the token before the name must end a type (identifier, *, &, >) or be a calling convention
        if q > 0 and not (toks[q - 1].k == 'id' or toks[q - 1].t in ('*', '&', '>', '&&')):
            return None
        static = any(toks[x].t == 'static' for x in range(max(0, q - 6), q))
        # template <...> before the declaration
        tparams = []
        k = q - 1
        while k >= 0 and k > q - 40 and toks[k].t not in (';', '{', '}'):
            if toks[k].t == 'template' and k + 1 < len(toks) and toks[k + 1].t == '<':
                e = match_close(toks, k + 1, '<', '>')
                tparams = param_names_t(toks[k + 2:e])
                break
            k -= 1
        return toks[n].t, qual, param_names(toks[o + 1:j]), static, tparams, toks[o + 1:j]

    def _consts(self, rel, toks):
        n = len(toks)
        i = 0
        while i < n:
            t = toks[i]
            # a type alias: using NAME = [ns::]TYPE;
            if t.t == 'using' and i + 4 < n and toks[i + 1].k == 'id' and toks[i + 2].t == '=':
                e = i + 3
                while e < n and (toks[e].k == 'id' or toks[e].t == '::'):
                    e += 1
                if e < n and toks[e].t == ';' and toks[e - 1].k == 'id':
                    self.aliases.setdefault(rel, {}).setdefault(toks[i + 1].t, toks[e - 1].t)
            # static_assert(sizeof(NAME) == value): the type's size, for sizeof(NAME) in the code
            if (t.t == 'static_assert' and i + 6 < n and toks[i + 2].t == 'sizeof' and toks[i + 3].t == '('
                    and toks[i + 4].k == 'id'):
                q = i + 4
                while q + 2 < n and toks[q + 1].t == '::':
                    q += 2
                if toks[q + 1].t == ')' and toks[q + 2].t == '==':
                    e = q + 3
                    while e < n and toks[e].t not in (',', ')', ';'):
                        e += 1
                    self.sizes.setdefault(toks[q].t, (rel, toks[q + 3:e]))
            if t.t in ('constexpr', 'const') and i + 1 < n:
                # const TYPE ... NAME = expr ;
                j = i + 1
                while j < n and j < i + 8 and toks[j].t not in ('=', ';', '(', '{', ')'):
                    j += 1
                if j < n and toks[j].t == '=' and toks[j - 1].k == 'id' and toks[j - 1].t not in KEYWORDS:
                    e = j + 1
                    d = 0
                    while e < n and not (d == 0 and toks[e].t in (';', ',')):
                        if toks[e].t in ('(', '{', '['):
                            d += 1
                        elif toks[e].t in (')', '}', ']'):
                            d -= 1
                            if d < 0:
                                break
                        e += 1
                    if toks[j + 1].t != '{' and e - j - 1 <= 40:
                        self.consts[rel].setdefault(toks[j - 1].t, ('expr', toks[j + 1:e]))
                    # further declarators: , NAME = expr
                    while e + 3 < n and toks[e].t == ',' and toks[e + 1].k == 'id' and toks[e + 2].t == '=':
                        nm, b = toks[e + 1].t, e + 3
                        e = b
                        while e < n and toks[e].t not in (';', ',', ')', '}'):
                            e += 1
                        if e - b <= 40:
                            self.consts[rel].setdefault(nm, ('expr', toks[b:e]))
                    i = e
                    continue
            if t.t == 'enum':
                j = i + 1
                while j < n and toks[j].t not in ('{', ';'):
                    j += 1
                if j < n and toks[j].t == '{':
                    e = match_close(toks, j, '{', '}')
                    k = j + 1
                    last = None
                    while k < e:
                        if toks[k].k == 'id':
                            name = toks[k].t
                            if toks[k + 1].t == '=':
                                m = k + 2
                                while m < e and toks[m].t != ',':
                                    m += 1
                                expr = toks[k + 2:m]
                                self.consts[rel].setdefault(name, ('expr', expr))
                                last = (name, 0)
                                k = m + 1
                            else:
                                self.consts[rel].setdefault(name, ('enum_next', last))
                                last = (name, 0)
                                k += 1
                                while k < e and toks[k].t != ',':
                                    k += 1
                                k += 1
                        else:
                            k += 1
                    i = e
                    continue
            i += 1

    HOOK = re.compile(r'\{\s*(0x[0-9a-fA-F]+)u?\s*,\s*(ptr\(\s*|\(void\s*\*\)\s*)?&?\s*([A-Za-z_][^"{}]*?)\s*,\s*"([^"]*)"')

    @staticmethod
    def expand_text(text, macros):
        """function-like macros expanded in a text (#a stringified, ## pasted, __VA_ARGS__), one level."""
        if not macros:
            return text
        pat = re.compile(r'\b(' + '|'.join(map(re.escape, macros)) + r')\s*\(')
        out, pos = [], 0
        for m in pat.finditer(text):
            if m.start() < pos:
                continue
            d, k = 1, m.end()
            while k < len(text) and d:
                d += {'(': 1, ')': -1}.get(text[k], 0)
                k += 1
            args, cur, dd = [], '', 0
            for ch in text[m.end():k - 1]:
                if ch == ',' and dd == 0:
                    args.append(cur.strip())
                    cur = ''
                    continue
                dd += {'(': 1, ')': -1, '{': 1, '}': -1}.get(ch, 0)
                cur += ch
            args.append(cur.strip())
            params, body = macros[m.group(1)]
            env = {}
            for j, pn in enumerate(params):
                env[pn] = ', '.join(args[j:]) if pn == '__VA_ARGS__' else (args[j] if j < len(args) else '')
            b = body.replace('\\\n', ' ').replace('##', '\0')
            b = re.sub(r'(?<!\w)#\s*(\w+)', lambda mm: json.dumps(env[mm.group(1)]) if mm.group(1) in env else mm.group(0), b)
            b = re.sub(r'\b(\w+)\b', lambda mm: env.get(mm.group(1), mm.group(1)), b)
            b = re.sub(r'\s*\0\s*', '', b)
            out.append(text[pos:m.start()])
            out.append(b)
            pos = k
        out.append(text[pos:])
        return ''.join(out)

    def _macro_defs(self, rel):
        """functions a macro of the module defines (NAME##_Create): the module's invocations expanded."""
        gen = {n: v for n, v in self.macros[rel].items() if '{' in v[1] and '(' in v[1]}
        if not gen:
            return
        text = re.sub(r'^[ \t]*#[^\n]*(?:\\\n[^\n]*)*', '', self.texts[rel], flags=re.M)
        toks, _ = tokenize(self.expand_text(text, gen))
        known = {id(d) for v in self.defs.values() for d in v if d.file == rel}
        self._defs(rel, toks)
        for v in self.defs.values():
            for d in v:
                if d.file == rel and id(d) not in known and d.toks is toks:
                    d.line = -1   # defined by a macro's expansion
        # drop the duplicates of the functions the expanded text defines as the original text does
        for name, v in self.defs.items():
            orig = [d for d in v if d.file == rel and d.toks is not toks]
            if orig:
                v[:] = [d for d in v if not (d.file == rel and d.toks is toks)]

    def _hooks(self, rel):
        text = self.texts[rel]

        def inc(mm):
            return self.texts.get(mm.group(1), mm.group(0)) if mm.group(1).endswith('.inc') else mm.group(0)
        text = re.sub(r'^[ \t]*#\s*include\s+"([^"]+)"[^\n]*', inc, text, flags=re.M)
        text = re.sub(r'//[^\n]*', '', text)
        macros = dict(self.macros[rel])
        for f in re.findall(r'#\s*include\s+"([^"]+)"', self.texts[rel]):
            if f.endswith('.inc'):
                macros.update(self.macros.get(f, {}))
        text = re.sub(r'^[ \t]*#[^\n]*(?:\\\n[^\n]*)*', '', text, flags=re.M)
        text = self.expand_text(text, {n: v for n, v in macros.items() if v[1].lstrip().startswith('{')})
        for m in self.HOOK.finditer(text):
            sym = m.group(3).strip()
            if m.group(2) and m.group(2).startswith('ptr') and sym.endswith(')'):
                sym = sym[:-1].strip()
            sym = sym.lstrip('&').strip()
            self.hooks.append((int(m.group(1), 16), sym, rel, m.group(4)))

    def asm_label(self, name):
        """a function written as a label of a top-level asm block ("_name:" in a string): its tokens up to the
        next label, read like inline assembler."""
        lab = re.compile(r'(?:^|\n)\s*_?' + re.escape(name) + r':')
        nxt = re.compile(r'\n\s*_?\w+:(?!\w)')
        for rel, toks in self.files.items():
            for i, t in enumerate(toks):
                if t.k == 'str' and lab.search(t.v):
                    rest = t.v[lab.search(t.v).end():]
                    e = i + 1
                    if not nxt.search(rest):
                        while e < len(toks) and toks[e].t != ';' and not (
                                toks[e].k == 'str' and re.match(r'\s*_?\w+:(?!\w)', toks[e].v)):
                            e += 1
                    synth = [Tok('id', '__asm__'), Tok('op', '(')] + toks[i:e] + [Tok('op', ')')]
                    return [Def(name, name, rel, t.line, synth, (0, len(synth)), [])]
        return []

    def closure(self, rel):
        """the file and every header it includes, transitively (an .inc counts as its including module)."""
        if rel in self._closure:
            return self._closure[rel]
        seen, work = [], [rel]
        while work:
            f = work.pop(0)
            if f in seen or f not in self.files:
                continue
            seen.append(f)
            work.extend(self.includes.get(f, []))
        self._closure[rel] = seen
        return seen


class Resolver:
    def __init__(self, src, exe, layout, fnames):
        self.src, self.exe, self.layout = src, exe, layout
        self.fnames = fnames   # exe function name -> addresses (docs/functions.csv)
        self.hook_targs = {}   # address -> its template arguments (normalized below)
        self.hook_names = collections.defaultdict(list)   # C++ name -> [(address, file, qualified name)]
        for a, name, rel, _ in src.hooks:
            owner = src.inc_owner.get(rel, rel)
            q = name.split('<')[0].strip()
            self.hook_names[q.split('::')[-1]].append((a, owner, q))
            if '<' in name:
                st, _ = tokenize(name)
                k = next(j for j, t in enumerate(st) if t.t == '<')
                self.hook_targs[a] = (st, k + 1, match_close(st, k, '<', '>'), owner)
        self.imports = set(exe.iat.values())
        self._cval = {}
        self._memo = {}

        def plain_value(toks, s0, s1, rel):
            if s1 - s0 == 1 and toks[s0].k == 'num' and toks[s0].v and toks[s0].v[0] == 'int':
                return toks[s0].v
            return self.evaluate(toks[s0:s1], rel) if s1 - s0 <= 12 else None
        self.plain_value = plain_value
        for a, (st, k0, k1, owner) in list(self.hook_targs.items()):
            self.hook_targs[a] = self.norm_targs(st, k0, k1, owner, plain_value)

    # -- names
    def scope(self, rel):
        return self.src.closure(self.src.inc_owner.get(rel, rel))

    @staticmethod
    def pick(items, name, qual, cls, qof):
        """the items a (possibly qualified) name means: by its qualifier, else the current class's, else the
        free ones, else all of them when they share one qualified name; [] when that is ambiguous. A member
        call (qual '.') means the one method of that name, if there is only one."""
        if qual == '.':
            return items if len({qof(x) for x in items}) == 1 and '::' in qof(items[0]) else []
        if qual:
            return [x for x in items if ('::' + qof(x)).endswith('::' + qual + '::' + name)]
        if cls:
            mine = [x for x in items if qof(x) == cls + '::' + name]
            if mine:
                return mine
        free = [x for x in items if '::' not in qof(x)]
        if free:
            return free
        if len({qof(x) for x in items}) == 1:
            return items
        return []

    def is_hook_def(self, name, defs):
        """one of the definitions is the hooked one (its file is a module with a hook of that name)"""
        files = {f for _, f, _ in self.hook_names.get(name, [])}
        return any(self.src.inc_owner.get(d.file, d.file) in files or d.file in files for d in defs)

    def unalias(self, qual, rel):
        if not qual or qual == '.':
            return qual
        parts = qual.split('::')
        for f in self.scope(rel):
            a = self.src.aliases.get(f, {}).get(parts[0])
            if a:
                return '::'.join([a] + parts[1:])
        return qual

    def hooked(self, name, rel, qual=None, cls=None, targs=None):
        """the exe addresses a C++ function name is hooked at; a template's instance by its template arguments
        (normalized: each argument's value when it has one)"""
        qual = self.unalias(qual, rel)
        key = ('h', name, self.src.inc_owner.get(rel, rel), qual, cls, targs)
        if key not in self._memo:
            c = self.hook_names.get(name)
            if c and any(self.hook_targs.get(x[0]) for x in c):
                if not any(self.hook_targs.get(x[0], '') == (targs or '') for x in c):
                    self._memo[key] = None
                    return None
                save = self.hook_names[name]
                self.hook_names[name] = [x for x in c if self.hook_targs.get(x[0], '') == (targs or '')]
                self._memo[key] = self._hooked(name, rel, qual, cls)
                self.hook_names[name] = save
            else:
                r = self._hooked(name, rel, qual, cls)
                if not r and qual and qual != '.' and qual not in self.src.classes:
                    r = self._hooked(name, rel, None, cls)   # a namespace, not a class
                self._memo[key] = r
        return self._memo[key]

    def norm_targs(self, toks, a, e, rel, value_of):
        """template arguments as a comparable text: each argument's value in hex when it has one"""
        out = []
        for s0, s1 in split_args_angle(toks, a, e):
            v = value_of(toks, s0, s1, rel)
            if v and v[0] == 'int':
                out.append(f'{u32(v[1]):#x}')
            else:
                out.append(''.join(t.t if t.k != 'num' or not t.v or t.v[0] != 'int' else f'{u32(t.v[1]):#x}'
                                   for t in toks[s0:s1]))
        return ','.join(out)

    def definition(self, name, rel, local=False, qual=None, cls=None):
        qual = self.unalias(qual, rel)
        key =('d', name, self.src.inc_owner.get(rel, rel), local, qual, cls)
        if key not in self._memo:
            r = self._definition(name, rel, local, qual, cls)
            if not r and qual and qual != '.' and qual not in self.src.classes:
                r = self._definition(name, rel, local, None, cls)   # a namespace, not a class
            self._memo[key] = r
        return self._memo[key]

    def _hooked(self, name, rel, qual=None, cls=None):
        c = self.hook_names.get(name)
        if not c:
            return None
        if qual == '.':
            sc = self.scope(rel)
            mine = [x for x in c if x[1] in sc]
            if mine and len({x[2] for x in mine}) == 1:
                return sorted({x[0] for x in mine})
        c = self.pick(c, name, qual, cls, lambda x: x[2])
        if not c:
            return None
        sc = self.scope(rel)
        same = [a for a, f, _ in c if f in sc]
        if same:
            return sorted(set(same))
        files = {f for _, f, _ in c}
        if len(files) == 1 or len({q for _, _, q in c}) == 1 and qual:
            return sorted({a for a, _, _ in c})
        if len({q for _, _, q in c}) == 1 and '::' in c[0][2]:
            return sorted({a for a, _, _ in c})   # one class method, hooked in several modules' tables
        return None   # ambiguous: static functions of the same name in other modules

    def _definition(self, name, rel, local=False, qual=None, cls=None):
        ds = self.src.defs.get(name)
        if not ds:
            return []
        if qual == '.':
            # a member call: the one method of that name in the first file of the scope that has any, else the one
            # method of that name anywhere
            for f in self.scope(rel):
                here = [d for d in ds if d.file == f]
                if here:
                    return here if len({d.qual for d in here}) == 1 else []
        ds = self.pick(ds, name, qual, cls, lambda d: d.qual)
        if not ds:
            return []
        sc = self.scope(rel)
        for f in sc:
            here = [d for d in ds if d.file == f]
            if here:
                return here
        if local:
            return []
        ext = [d for d in ds if not d.static and d.file.endswith(('.cpp', '.c'))]
        if len({d.file for d in ext}) == 1:
            return ext
        if qual and len({d.file for d in ds}) == 1:
            return ds
        return []

    def const(self, name, rel, depth=0):
        key = ('c', name, self.src.inc_owner.get(rel, rel))
        if key not in self._memo:
            self._memo[key] = self._const(name, rel, depth)
        return self._memo[key]

    def _const(self, name, rel, depth=0):
        sc = self.scope(rel)
        for f in sc:
            c = self.src.consts.get(f, {}).get(name)
            if c is not None:
                return self.const_value(c, f, name, depth)
        return None

    def const_value(self, c, f, name, depth):
        key = (f, name)
        if key in self._cval:
            return self._cval[key]
        self._cval[key] = None
        if depth > 12:
            return None
        if c[0] == 'expr':
            v = self.evaluate(c[1], f, depth + 1)
        else:
            last = c[1]
            if last is None:
                v = ('int', 0)
            else:
                p = self.const(last[0], f, depth + 1)
                v = ('int', p[1] + 1) if p and p[0] == 'int' else None
        self._cval[key] = v
        return v

    def evaluate(self, toks, rel, depth=0, binds=None):
        """the value of a constant expression of literals, named constants and casts, or None."""
        parts = []
        i = 0
        n = len(toks)
        isfloat = False
        while i < n:
            t = toks[i]
            if t.k == 'num':
                if not t.v:
                    return None
                if t.v[0] == 'int':
                    parts.append(str(t.v[1]))
                else:
                    parts.append(repr(t.v[1]))
                    isfloat = True
            elif t.k == 'id':
                if t.t in ('static_cast', 'reinterpret_cast') and i + 1 < n and toks[i + 1].t == '<':
                    e = match_close(toks, i + 1, '<', '>')
                    i = e + 1
                    continue
                if t.t in ('DWORD', 'int', 'unsigned', 'WORD', 'BYTE', 'long', 'short', 'char', 'uint32_t', 'int32_t',
                           'float', 'double', 'size_t', 'uintptr_t', 'signed', 'UINT', 'LONG', 'u32', 'i32',
                           'GlobKey', 'FnKey', 'CodeKey', 'VtKey', 'ImpKey', 'const', 'auto'):
                    if parts and parts[-1] == '(' and i + 1 < n and toks[i + 1].t == ')':
                        parts.pop()
                        i += 2
                        continue
                    if i + 1 < n and toks[i + 1].t in ('(', '{'):
                        i += 1
                        if toks[i].t == '{':
                            parts.append('(')
                            e = match_close(toks, i, '{', '}')
                            inner = self.evaluate(toks[i + 1:e], rel, depth + 1, binds)
                            if inner is None:
                                return None
                            parts.append(str(inner[1]))
                            parts.append(')')
                            i = e + 1
                            continue
                        continue
                    i += 1
                    continue
                if t.t in ('true', 'false'):
                    parts.append('1' if t.t == 'true' else '0')
                else:
                    v = binds.get(t.t) if binds else None
                    if v is None:
                        v = self.const(t.t, rel, depth + 1)
                    if v is None or v[0] not in ('int', 'float'):
                        return None
                    parts.append(str(v[1]) if v[0] == 'int' else repr(v[1]))
                    if v[0] == 'float':
                        isfloat = True
            elif t.t in ('+', '-', '*', '/', '%', '<<', '>>', '|', '&', '^', '~', '(', ')'):
                parts.append(t.t)
            else:
                return None
            i += 1
        if not parts:
            return None
        expr = ' '.join(parts)
        if not re.fullmatch(r'[\d\s+\-*/%<>|&^~().eE]+|[\d\s+\-*/%<>|&^~().eEinfa]+', expr):
            return None
        try:
            if not isfloat:
                expr = expr.replace('/', '//')
            v = eval(expr, {'__builtins__': {}}, {})   # only digits and operators (checked above)
        except Exception:
            return None
        if isinstance(v, float):
            return ('float', v)
        return ('int', v)


class Collector:
    """one function's C++ side."""
    def __init__(self, res, rel):
        self.res = res
        self.rel = rel
        self.code = {}        # address -> via
        self.alias = []       # [addresses] of one C++ name hooked at several addresses
        self.imports = {}
        self.externals = {}
        self.vslots = {}
        self.globals = {}
        self.ints = {}
        self.floats = {}      # (value, width) -> via
        self.strings = {}
        self.folded = {}      # value -> (operand values, via)
        self.slot_ints = set()
        self.unresolved_slots = 0
        self.ambiguous = set()
        self.helpers = set()
        self.asm = False
        self.dims = set()
        self.params = [set()]
        self.cls = [None]
        self.defs = []
        self.indirect = 0

    def literal(self, v, via):
        if v[0] == 'float':
            self.floats.setdefault((v[1], v[2] if len(v) > 2 else 8), via)
            return
        x = v[1]
        ux = u32(x)
        exe = self.res.exe
        if exe.lo <= ux < exe.hi and x >= 0:
            if exe.iat_lo <= ux < exe.iat_hi:
                self.imports.setdefault(exe.iat.get(ux & ~3, f'iat_{ux:#x}'), via)
            elif exe.in_text(ux) and not any(s <= ux < e for s, e in self.res.layout.text_data):
                self.code.setdefault(ux, via)
            else:
                self.globals.setdefault(ux, via)
            return
        self.ints.setdefault(s32(x), via)

    def run(self, d, binds, chain, depth):
        self.defs.append(d)
        self.params.append(set(p for p in d.params if p))
        self.cls.append(d.qual.rsplit('::', 1)[0] if '::' in d.qual else self.cls[-1])
        self.walk(d.toks, d.body[0], d.body[1], d.file, binds, chain, depth)
        self.cls.pop()
        self.params.pop()
        self.defs.pop()

    def walk(self, toks, lo, hi, rel, binds, chain, depth):
        via = '>'.join(chain[1:]) if len(chain) > 1 else ''
        res = self.res
        i = lo
        skip_str_until = -1
        consumed = set()
        while i < hi:
            t = toks[i]
            if i in consumed:
                i += 1
                continue
            if t.k == 'num':
                if t.v:
                    v = t.v if t.v[0] == 'int' else ('float', t.v[1], t.v[2])
                    # an array's dimension in a declaration (TYPE name[N]) is no value of the code
                    if (i >= lo + 3 and toks[i - 1].t == '[' and toks[i - 2].k == 'id' and i + 1 < hi
                            and toks[i + 1].t == ']' and (toks[i - 3].k == 'id' and toks[i - 3].t not in KEYWORDS
                                                          or toks[i - 3].t in ('*', '&'))):
                        self.dims.add(v[1])
                    elif i > lo and toks[i - 1].t in ('<<', '>>', '<<=', '>>=') and v[0] == 'int' and 0 <= v[1] < 32:
                        pass   # a shift count (not compared, as on the exe side)
                    else:
                        self.literal(v, via)
                i += 1
                continue
            if t.k == 'str':
                if i > skip_str_until:
                    self.strings.setdefault(t.v, via)
                i += 1
                continue
            if t.k == 'op':
                if t.t in ('(', '~', '-') and i + 2 < hi:
                    self.fold(toks, i, hi, rel, binds, via)
                i += 1
                continue
            if t.k != 'id':
                i += 1
                continue
            name = t.t
            nxt = toks[i + 1].t if i + 1 < hi else ''
            if name in ASM_WORDS:
                e = match_close(toks, i + 1, '(', ')') if nxt in ('(', 'volatile', '__volatile__') else i
                if nxt in ('volatile', '__volatile__'):
                    e = match_close(toks, i + 2, '(', ')')
                self.asm = True
                for k in range(i, min(e + 1, hi)):
                    if toks[k].k == 'str':
                        for m in re.finditer(r'(?<![\w$])(0x[0-9a-fA-F]+)', toks[k].v):
                            self.literal(('int', int(m.group(1), 16)), via + ('>' if via else '') + 'asm')
                    elif toks[k].k == 'num' and toks[k].v and toks[k].v[0] == 'int' and toks[k].v[1] > 0xffff:
                        self.literal(toks[k].v, via + ('>' if via else '') + 'asm')   # ZAX_ASM_DATA(0x...) and the like
                i = e + 1
                continue
            if name == 'sizeof' and nxt == '(':
                e = match_close(toks, i + 1, '(', ')')
                q = e - 1
                if toks[q].k == 'id' and toks[q].t in res.src.sizes and all(
                        x.k == 'id' or x.t in ('::', 'struct', 'class') for x in toks[i + 2:e]):
                    f, expr = res.src.sizes[toks[q].t]
                    v = res.evaluate(expr, f)
                    if v and v[0] == 'int':
                        self.literal(v, via + ('>' if via else '') + f'sizeof({toks[q].t})')
                i = e + 1
                continue
            if name == 'log' and nxt == '(':
                skip_str_until = match_close(toks, i + 1, '(', ')')
                i += 1
                continue
            # template arguments: name<...>( or a template function as a value
            targs = None
            call_open = None
            if nxt == '(':
                call_open = i + 1
            elif nxt == '<' and name not in KEYWORDS and (
                    any(d.tparams for d in self.res.src.defs.get(name, [])) or name in ('vcall', 'vc')):
                e = match_close(toks, i + 1, '<', '>')
                targs = (toks, i + 2, e)
                if e + 1 < hi and toks[e + 1].t == '(':
                    call_open = e + 1
            if binds and name in binds and binds[name] is not None:
                b = binds[name]
                if b[0] == 'ref':
                    args = None
                    if call_open is not None:
                        e = match_close(toks, call_open, '(', ')')
                        args = (toks, split_args(toks, call_open + 1, e))
                    self.call(b[1], b[2], args, b[3], binds, chain, depth, via, '&', True)
                else:
                    self.literal(b, via)
                i += 1
                continue
            prev = toks[i - 1].t if i > lo else ''
            qual = None
            if prev in ('.', '->'):
                if call_open is None:
                    i += 1
                    continue
                qual = '.'
                recv = toks[i - 2] if i >= lo + 2 else None
                ty = None
                if recv is not None and recv.t == 'this':
                    ty = self.cls[-1]
                elif recv is not None and recv.k == 'id':
                    ty = decl_type(recv.t, toks, lo, i - 2)
                    if ty is None and self.defs:
                        pt = self.defs[-1].ptoks
                        ty = decl_type(recv.t, pt, 0, len(pt))
                    if ty is None and self.cls[-1]:
                        for ctoks, c0, c1 in res.src.classes.get(self.cls[-1], []):
                            ty = decl_type(recv.t, ctoks, c0, c1)
                            if ty:
                                break
                if ty and (res.definition(name, rel, False, ty) or res.hooked(name, rel, ty)):
                    qual = ty
            elif prev == '::' and i >= lo + 2 and toks[i - 2].k == 'id':
                q = i - 2
                qual = toks[q].t
                while q >= lo + 2 and toks[q - 1].t == '::' and toks[q - 2].k == 'id':
                    q -= 2
                    qual = toks[q].t + '::' + qual
            elif prev == '::':
                qual = None   # ::name, the global one
            if name in ('vcall', 'vc') and call_open is not None:
                e = match_close(toks, call_open, '(', ')')
                args = split_args(toks, call_open + 1, e)
                if len(args) >= 2:
                    a0, a1 = args[1]
                    v = self.value_of(toks, a0, a1, rel, binds)
                    q = [k for k in range(a0, a1) if toks[k].t in ('?', ':')]
                    if v is None and len(q) == 2 and toks[q[0]].t == '?':
                        # cond ? slot : slot
                        for b0, b1 in ((q[0] + 1, q[1]), (q[1] + 1, a1)):
                            w = self.value_of(toks, b0, b1, rel, binds)
                            if w and w[0] == 'int':
                                self.vslots.setdefault(4 * w[1], via)
                                self.slot_ints.add(w[1])
                            else:
                                self.unresolved_slots += 1
                    elif v and v[0] == 'int':
                        self.vslots.setdefault(4 * v[1], via)
                        for k in range(a0, a1):
                            consumed.add(k)
                        if a1 - a0 == 1 and toks[a0].k == 'num':
                            self.slot_ints.add(v[1])
                    else:
                        self.unresolved_slots += 1
                i += 1
                continue
            # a call or a reference of a function
            is_ref = call_open is not None or prev == '&' or targs is not None or (prev in (',', '<') and nxt in (',', '>'))
            if not is_ref or name in KEYWORDS:
                c = res.const(name, rel) if name not in KEYWORDS else None
                if c is not None and (c[0] in ('int', 'float')):
                    self.literal(c if c[0] == 'int' else ('float', c[1], 8), via + ('>' if via else '') + name)
                i += 1
                continue
            if name in LAYER:
                if name in ('at', 'fn_dyn', 'code', 'original') and call_open is not None:
                    e = match_close(toks, call_open, '(', ')')
                    if e + 1 < hi and toks[e + 1].t == '(' and not (e - call_open == 2 and toks[call_open + 1].k == 'num'):
                        self.indirect += 1   # a call through an address read at run time
                i += 1
                continue
            args = None
            if call_open is not None:
                e = match_close(toks, call_open, '(', ')')
                args = (toks, split_args(toks, call_open + 1, e))
            self.call(name, targs, args, rel, binds, chain, depth, via, prev, call_open is not None, qual)
            if targs is not None and call_open is None:
                i = targs[2] + 1
                continue
            i += 1

    def targ_binds(self, defs, targs, rel, binds):
        """template parameters bound to the template arguments (literals, named constants, functions)."""
        out = {}
        if not targs:
            return out
        toks, a, e = targs
        spans = split_args_angle(toks, a, e)
        for d in defs:
            for k, p in enumerate(d.tparams):
                if p and k < len(spans):
                    s0, s1 = spans[k]
                    v = self.value_of(toks, s0, s1, rel, binds)
                    if v is None and s1 > s0 and toks[s0].k == 'id':
                        q = s0
                        while q + 2 < s1 and toks[q + 1].t == '::' and toks[q + 2].k == 'id':
                            q += 2
                        inner = None
                        if q + 1 < s1 and toks[q + 1].t == '<':
                            inner = (toks, q + 2, match_close(toks, q + 1, '<', '>'))
                        v = ('ref', toks[q].t, inner, rel)
                    if v is not None:
                        out[p] = v
        return out

    def ref_of(self, toks, s0, s1, rel):
        """an argument that names a function ([&][A::]name[<...>]): ('ref', name, template arguments, file)"""
        if s0 < s1 and toks[s0].t == '&':
            s0 += 1
        if s0 >= s1 or toks[s0].k != 'id':
            return None
        q = s0
        while q + 2 < s1 and toks[q + 1].t == '::' and toks[q + 2].k == 'id':
            q += 2
        inner = None
        if q + 1 < s1:
            if toks[q + 1].t != '<' or match_close(toks, q + 1, '<', '>') != s1 - 1:
                return None
            inner = (toks, q + 2, s1 - 1)
        nm = toks[q].t
        if nm in self.params[-1] or nm in KEYWORDS:
            return None
        if self.res.hooked(nm, rel) or any(not x.macro for x in self.res.definition(nm, rel, local=True)):
            return ('ref', nm, inner, rel)
        return None

    def call(self, name, targs, args, rel, binds, chain, depth, via, prev, is_call, qual=None):
        """a call or reference of the function `name` (a hook target, a helper to expand, an import, an external)."""
        res = self.res
        cls = self.cls[-1]
        tn = None
        if targs is not None:
            tn = res.norm_targs(targs[0], targs[1], targs[2], rel,
                                lambda t, s0, s1, r: self.value_of(t, s0, s1, r, binds))
        hooked = res.hooked(name, rel, qual, cls, tn)
        defs_here = res.definition(name, rel, True, qual, cls)
        if hooked and defs_here and not res.is_hook_def(name, defs_here):
            hooked = None
        if name in STANDS_FOR and is_call:
            self.code.setdefault(STANDS_FOR[name], via + ('>' if via else '') + name)
            return
        if not is_call and not hooked:
            return   # a helper as a value is expanded where it is called (a bound parameter)
        if hooked:
            if len(hooked) > 1:
                self.alias.append(hooked)
            for a in hooked:
                self.code.setdefault(a, via + ('>' if via else '') + name)
            return
        defs = [d for d in res.definition(name, rel, False, qual, cls) if d.qual not in chain]
        if defs and depth < EXPAND_DEPTH:
            new_binds = self.targ_binds(defs, targs, rel, binds)
            if args is not None:
                atoks, spans = args
                for d in defs:
                    for k, p in enumerate(d.params):
                        if p and k < len(spans):
                            v = self.value_of(atoks, spans[k][0], spans[k][1], rel, binds)
                            if v is None:
                                v = self.ref_of(atoks, spans[k][0], spans[k][1], rel)
                            if v is not None:
                                new_binds.setdefault(p, v)
            self.helpers.add(name)
            for d in defs:
                self.run(d, new_binds, chain + [d.qual], depth + 1)
            return
        if defs or res.src.defs.get(name):
            if not defs and is_call and not any(d.qual in chain for d in res.src.defs[name]):
                self.ambiguous.add(name if not qual or qual == '.' else f'{qual}::{name}')
            return
        if res.hook_names.get(name):
            self.ambiguous.add(name)
        if is_call:
            if name in res.imports:
                self.imports.setdefault(name, via)
            elif name not in KEYWORDS and (name[0].islower() or name.startswith('_') or name in res.fnames):
                self.externals.setdefault(name, via)

    def value_of(self, toks, a0, a1, rel, binds):
        if a1 - a0 == 1:
            t = toks[a0]
            if t.k == 'num' and t.v:
                return t.v if t.v[0] == 'int' else ('float', t.v[1], t.v[2])
            if t.k == 'id':
                if binds and t.t in binds:
                    return binds[t.t]
                c = self.res.const(t.t, rel)
                if c and c[0] in ('int', 'float'):
                    return c if c[0] == 'int' else ('float', c[1], 8)
            return None
        if a1 - a0 <= 12:
            v = self.res.evaluate(toks[a0:a1], rel, 0, binds)
            if v is not None:
                return v if v[0] == 'int' else ('float', v[1], 8)
        return None

    def fold(self, toks, i, hi, rel, binds, via):
        """a constant expression starting at i (a parenthesis or unary ~/-): its folded value."""
        if toks[i].t == '(':
            e = match_close(toks, i, '(', ')')
            if e >= hi or e - i < 3:
                return
            span = toks[i + 1:e]
        else:
            # unary ~ or - applied to the next literal or name (- only after an operator or an opening bracket)
            prev = toks[i - 1] if i > 0 else None
            if toks[i].t == '-' and prev is not None and (prev.k in ('num', 'id', 'str') or prev.t in (')', ']')):
                # a binary minus: fold "a - b" of two literals
                if prev.k == 'num' and i + 1 < hi and toks[i + 1].k == 'num':
                    span = [prev, toks[i], toks[i + 1]]
                else:
                    return
            else:
                span = toks[i:i + 2]
        if not span or not any(t.k == 'num' or t.k == 'id' for t in span):
            return
        if any(t.k == 'id' and t.t not in ('DWORD', 'int', 'unsigned') and not (binds and t.t in binds)
               and self.res.const(t.t, rel) is None for t in span):
            return
        v = self.res.evaluate(span, rel, 0, binds)
        if v is None or v[0] != 'int':
            return
        ops = []
        for t in span:
            if t.k == 'num' and t.v and t.v[0] == 'int':
                ops.append(t.v[1])
            elif t.k == 'id':
                c = (binds or {}).get(t.t) or self.res.const(t.t, rel)
                if c and c[0] == 'int':
                    ops.append(c[1])
        self.folded.setdefault(v[1], (ops, via))
        self.folded.setdefault(u32(v[1]), (ops, via))


def decl_type(name, toks, a, b):
    """the type a variable, parameter or member `name` is declared with in toks[a:b] (TYPE [*&]name followed by
    ; = , ) [ { :), the last declaration first; `auto` takes the type of a static_cast/reinterpret_cast."""
    for k in range(b - 1, a, -1):
        t = toks[k]
        if t.t != name or t.k != 'id' or k + 1 >= len(toks) or toks[k + 1].t not in (';', '=', ',', ')', '[', '{', ':'):
            continue
        j = k - 1
        while j >= a and toks[j].t in ('*', '&', 'const', '&&'):
            j -= 1
        if j < a or toks[j].k != 'id' or toks[j].t in KEYWORDS and toks[j].t != 'auto':
            continue
        if toks[j].t == 'auto':
            e = k + 1
            if e + 3 < len(toks) and toks[e].t == '=' and toks[e + 1].t in ('static_cast', 'reinterpret_cast') \
                    and toks[e + 2].t == '<' and toks[e + 3].k == 'id':
                return toks[e + 3].t
            return None
        return toks[j].t
    return None


def split_args_angle(toks, lo, hi):
    """template arguments split at the top-level commas (angle brackets count as brackets)."""
    out, d, s = [], 0, lo
    for k in range(lo, hi):
        t = toks[k].t
        if t in ('(', '[', '{', '<'):
            d += 1
        elif t in (')', ']', '}', '>'):
            d -= 1
        elif t == '>>':
            d -= 2
        elif t == ',' and d == 0:
            out.append((s, k))
            s = k + 1
    if hi > s:
        out.append((s, hi))
    return out


def split_args(toks, lo, hi):
    out, d, s = [], 0, lo
    for k in range(lo, hi):
        t = toks[k].t
        if t in ('(', '[', '{'):
            d += 1
        elif t in (')', ']', '}'):
            d -= 1
        elif t == ',' and d == 0:
            out.append((s, k))
            s = k + 1
    if hi > s:
        out.append((s, hi))
    return out


# ---- the comparison ------------------------------------------------------------------------------------------------

def div_magics(d):
    """MSVC's magic multipliers for a 32-bit division by d (signed and unsigned), Hacker's Delight 10-1/10-2."""
    out = set()
    if d < 3 or d >= 1 << 31 or d & (d - 1) == 0:
        return out   # (a power of two is a shift)
    # signed
    two31 = 1 << 31
    anc = two31 - 1 - (two31 % d)
    p = 31
    q1, r1 = divmod(two31, anc)
    q2, r2 = divmod(two31, d)
    while True:
        p += 1
        q1, r1 = 2 * q1, 2 * r1
        if r1 >= anc:
            q1, r1 = q1 + 1, r1 - anc
        q2, r2 = 2 * q2, 2 * r2
        if r2 >= d:
            q2, r2 = q2 + 1, r2 - d
        delta = d - r2
        if not (q1 < delta or (q1 == delta and r1 == 0)) or p > 64:
            break
    out.add(u32(q2 + 1))
    # unsigned: smallest m with m = ceil(2^(32+s)/d) fitting 33 bits
    for sh in range(0, 33):
        m = -(-(1 << (32 + sh)) // d)
        out.add(u32(m))
    return {m for m in out if m >= 0x10000}


def vtable_slots(exe, layout):
    """function address -> set of slot offsets it fills in some vtable of the exe."""
    out = collections.defaultdict(set)
    for a, n in layout.vtables.items():
        for k in range(0, max(n, 0) // 4):
            v = struct.unpack('<I', exe.read(a + 4 * k, 4) or b'\0\0\0\0')[0]
            if exe.in_text(v):
                out[v].add(4 * k)
    return out


def compare(a, ef, cc, ctx):
    """the differences of one function: (matched counts, explained [(kind, side, item, rule, note)], open [...])."""
    exe = ctx['exe']
    explained, open_ = [], []
    E, C = ef, cc

    def add_exp(kind, side, item, rule, note=''):
        explained.append({'kind': kind, 'side': side, 'item': item, 'rule': rule, 'note': note})

    def add_open(kind, side, item, hint=''):
        d = {'kind': kind, 'side': side, 'item': item}
        if hint:
            d['hint'] = hint
        open_.append(d)

    sizes = ctx['sizes']

    def same_bytes(x, y):
        nx, ny = sizes.get(x), sizes.get(y)
        if not nx or nx != ny:
            return False
        return exe.read(x, nx) == exe.read(y, ny)

    # ---- calls: code addresses, imports, vtable slots
    e_code = {t: k for t, k in E['code'].items() if t not in E['seh'] or k == 'call'}
    seh_items = [t for t in E['code'] if t in E['seh'] and E['code'][t] != 'call']
    for t in seh_items:
        add_exp('calls', 'exe', f'{t:#x}', 'seh_frame')
    c_code = dict(C.code)
    # externals by name to exe functions
    ext_left = {}
    for name, via in C.externals.items():
        base = name.lstrip('_')
        cands = ctx['fnames'].get(name) or ctx['fnames'].get('_' + base) or ctx['fnames'].get(base)
        hit = [x for x in (cands or []) if x in e_code]
        if hit:
            for x in hit:
                c_code.setdefault(x, via + ('>' if via else '') + name)
                add_exp('calls', 'both', f'{x:#x} {name}', 'crt_by_name')
            continue
        ext_left[name] = via
    e_only = {t for t in e_code if t not in c_code}
    c_only = {t for t in c_code if t not in e_code}
    # aliases (one C++ name hooked at several addresses): a reference to one of them matches
    for group in C.alias:
        if any(x in e_code for x in group):
            for x in sorted(set(group) & c_only):
                add_exp('calls', 'cpp', f'{x:#x}', 'alias', f'hooked together with {", ".join(hex(y) for y in group if y != x)}')
            c_only -= set(group)
    for x in sorted(c_only):
        for y in sorted(e_only):
            if same_bytes(x, y):
                add_exp('calls', 'both', f'{y:#x} = {x:#x}', 'identical_code')
                c_only.discard(x)
                e_only.discard(y)
                break
    e_imp = set(E['imports'])
    c_imp = set(C.imports)
    e_slots = set(E['vslots'])
    c_slots = set(C.vslots)
    exes = ctx['exe_funcs']

    def callee_set(x):
        f = exes.get(x)
        if not f:
            return None
        s = {('code', t) for t, k in f['code'].items() if k in ('call', 'tail')} | {('imp', n) for n in f['imports']} | \
            {('slot', o) for o in f['vslots']}
        return s

    c_all = {('code', t) for t in c_code} | {('imp', n) for n in c_imp} | {('slot', o) for o in c_slots}
    e_all = {('code', t) for t in e_code} | {('imp', n) for n in e_imp} | {('slot', o) for o in e_slots}
    expl_c, expl_e = set(), set()   # items of the C++ (exe) explained by a callee the C++ (exe) inlined

    def items_of(x):
        f = exes[x]
        return ({('code', t) for t in f['code']} | {('imp', n) for n in f['imports']} | {('slot', o) for o in f['vslots']}
                | {('glob', g) for g in f['globals']} | {('const', c) for c in f['consts']}
                | {('const', u32(c)) for c in f['fields']} | {('float', v) for v in f['floats'].values()})

    c_all |= {('glob', g) for g in C.globals}
    e_all |= {('glob', g) for g in E['globals']}

    def evidence(x):
        """X's calls, imports, vtable slots and globals: what must all appear where X is inlined (at least two)"""
        s = callee_set(x)
        if s is None:
            return None
        s = s | {('glob', g) for g in exes[x]['globals']}
        return s if len(s) >= 2 else None

    for x in sorted(e_only):
        s = evidence(x)
        if s and s <= c_all:
            add_exp('calls', 'exe', f'{x:#x}', 'callee_inlined_cpp', f'its {len(s)} calls and globals appear in the C++')
            expl_c |= items_of(x)
            e_only.discard(x)
    for x in sorted(c_only):
        s = evidence(x)
        if s and s <= e_all:
            add_exp('calls', 'cpp', f'{x:#x}', 'callee_inlined_exe', f'its {len(s)} calls and globals appear in the exe')
            expl_e |= items_of(x)
            c_only.discard(x)
    # devirtualized: exe slot s vs C++ direct call of a function in slot s (and the other way round)
    vts = ctx['vt_slots']
    e_slot_only = e_slots - c_slots
    c_slot_only = c_slots - e_slots
    for x in sorted(c_only):
        hit = vts.get(x, set()) & e_slot_only
        if hit:
            add_exp('calls', 'both', f'{x:#x} / slot {min(hit):#x}', 'devirtualized')
            c_only.discard(x)
            e_slot_only -= hit
    for x in sorted(e_only):
        hit = vts.get(x, set()) & c_slot_only
        if hit:
            add_exp('calls', 'both', f'{x:#x} / slot {min(hit):#x}', 'devirtualized')
            e_only.discard(x)
            c_slot_only -= hit
    fn_name = ctx['names']
    for x in sorted(e_only):
        nm = fn_name.get(x, '')
        if ('code', x) in expl_e:
            add_exp('calls', 'exe', f'{x:#x} {nm}'.strip(), 'callee_inlined_exe')
            continue
        if ('code', x) in expl_c:
            continue
        if nm == '_chkstk':
            add_exp('calls', 'exe', f'{x:#x} _chkstk', 'stack_probe')
            continue
        hint = ''
        if nm == '__ftol':
            hint = 'a float-to-integer conversion; a C++ cast compiles to it and is not visible to the tool'
        if x in ctx['reimpl'] and E['code'].get(x) == 'ref':
            hint = 'a function address as a value'
        add_open('calls', 'exe', f'{x:#x} {nm}'.strip(), hint)
    for x in sorted(c_only):
        if ('code', x) in expl_e:
            continue
        if ('code', x) in expl_c:
            add_exp('calls', 'cpp', f'{x:#x}', 'callee_inlined_cpp')
            continue
        add_open('calls', 'cpp', f'{x:#x} {fn_name.get(x, "")}'.strip(), f'via {c_code[x]}' if c_code[x] else '')
    for n in sorted(e_imp - c_imp):
        if ('imp', n) in expl_c or ('imp', n) in expl_e:
            continue
        add_open('calls', 'exe', f'import {n}')
    for n in sorted(c_imp - e_imp):
        if ('imp', n) in expl_e or ('imp', n) in expl_c:
            continue
        add_open('calls', 'cpp', f'import {n}', f'via {C.imports[n]}' if C.imports[n] else '')
    for name, via in sorted(ext_left.items()):
        need = INTRINSIC.get(name)
        if need and need & E['markers']:
            add_exp('calls', 'cpp', name, 'intrinsic')
            continue
        # externals the tool cannot map stay informational (compiler builtins, the reimplementation's own runtime)
    for o in sorted(e_slot_only):
        if ('slot', o) in expl_c or ('slot', o) in expl_e:
            continue
        add_open('vslots', 'exe', f'slot {o // 4} (+{o:#x})', 'the C++ calls through a pointer it reads at run time (a struct\'s function pointer?)' if C.indirect or C.unresolved_slots else '')
    for o in sorted(c_slot_only):
        if ('slot', o) in expl_e or ('slot', o) in expl_c:
            continue
        add_open('vslots', 'cpp', f'slot {o // 4} (+{o:#x})', f'via {C.vslots[o]}' if C.vslots[o] else '')

    # ---- globals and strings
    e_glob = {g for g in E['globals'] if g not in E['seh']}
    for g in E['globals']:
        if g in E['seh']:
            add_exp('globals', 'exe', f'{g:#x}', 'seh_frame')
    c_glob = set(C.globals)
    c_strings = set(C.strings)
    e_float_addr = {k[1]: (k[2], v) for k, v in E['floats'].items() if k[0] == 'addr'}
    c_float_vals = {}
    for (v, w), via in C.floats.items():
        c_float_vals.setdefault(v, w)
    c_ints = dict(C.ints)
    layout = ctx['layout']

    def float_at(g, width):
        b = exe.read(g, width)
        if not b:
            return None
        return struct.unpack('<f' if width == 4 else '<d', b)[0]

    def c_has_float(val, width):
        if val is None:
            return False
        for v in c_float_vals:
            if width == 4 and as_f32(v) == val:
                return True
            if width == 8 and v == val:
                return True
        return False

    e_only_g = e_glob - c_glob
    c_only_g = c_glob - e_glob
    # floats of different addresses with the same value (both sides load a constant of .rdata)
    c_float_glob = {}
    for g in c_only_g:
        if exe.section(g) == '.rdata':
            for w in (4, 8):
                c_float_glob.setdefault((w, float_at(g, w)), g)
    c_texts = {}
    for g in c_glob:
        s = exe.string(g)
        if s is not None:
            c_texts.setdefault(s, g)
    e_texts = {}
    for g in e_glob:
        s = exe.string(g)
        if s is not None:
            e_texts.setdefault(s, g)
    used_c = set()
    for g in sorted(e_only_g):
        if ('glob', g) in expl_e:
            add_exp('globals', 'exe', f'{g:#x}', 'callee_inlined_exe')
            continue
        if g in e_float_addr:
            w, val = e_float_addr[g]
            if c_has_float(val, w):
                add_exp('constants', 'exe', f'{g:#x} = {val!r}', 'float_by_value')
                continue
            other = c_float_glob.get((w, val))
            if other is not None and val is not None:
                add_exp('constants', 'both', f'{g:#x} = {other:#x} = {val!r}', 'float_same_value')
                used_c.add(other)
                continue
            b = f32_bits(val) if (val is not None and w == 4) else None
            if b is not None and (b in c_ints or s32(b) in c_ints):
                add_exp('constants', 'exe', f'{g:#x} = {val!r}', 'float_bits')
                continue
        s = exe.string(g)
        if s is not None and (s in c_strings or (s in c_texts and c_texts[s] != g)):
            add_exp('strings', 'exe', f'{g:#x} {s[:60]!r}', 'string_by_text')
            if s in c_texts:
                used_c.add(c_texts[s])
            continue
        b = layout.block(g)
        if b is not None:
            hit = [h for h in C.globals if layout.block(h) == b and ((g - h) in c_ints or (g - h) in C.folded or g - h == 0)]
            if hit:
                add_exp('globals', 'exe', f'{g:#x}', 'global_block', f'{hit[0]:#x} + {g - hit[0]:#x}')
                continue
        kind = 'strings' if s is not None else ('constants' if g in e_float_addr else 'globals')
        hint = ''
        if b is not None and any(layout.block(h) == b for h in C.globals):
            hint = 'the C++ uses another address of the same block'
        label = f'{g:#x}' + (f' {s[:60]!r}' if s is not None else '') + (f' = {e_float_addr[g][1]!r}' if g in e_float_addr else '')
        add_open(kind, 'exe', label, hint)
    for g in sorted(c_only_g):
        if g in used_c:
            continue
        if ('glob', g) in expl_c:
            add_exp('globals', 'cpp', f'{g:#x}', 'callee_inlined_cpp')
            continue
        s = exe.string(g)
        if s is not None and s in e_texts:
            add_exp('strings', 'cpp', f'{g:#x} {s[:60]!r}', 'string_by_text')
            continue
        if exe.section(g) == '.rdata':
            hitf = False
            for w in (4, 8):
                val = float_at(g, w)
                if any(k[0] == 'addr' and k[2] == w and v == val for k, v in E['floats'].items()) and val is not None:
                    add_exp('constants', 'cpp', f'{g:#x} = {val!r}', 'float_same_value')
                    hitf = True
                    break
                if val is not None and any(k[0] == 'builtin' and v == val for k, v in E['floats'].items()):
                    add_exp('constants', 'cpp', f'{g:#x} = {val!r}', 'x87_builtin')
                    hitf = True
                    break
            if hitf:
                continue
            v4 = float_at(g, 4)
            bits = f32_bits(v4) if v4 is not None else None
            if bits is not None and bits in E['consts'] and bits >= 0x10000:
                add_exp('constants', 'cpp', f'{g:#x} = {v4!r}', 'float_bits')
                continue
        b = layout.block(g)
        if b is not None:
            hit = [h for h in e_glob if layout.block(h) == b and ((h - g) in c_ints or (h - g) in C.folded)]
            if hit:
                add_exp('globals', 'cpp', f'{g:#x}', 'global_block', f'{hit[0]:#x}')
                continue
        kind = 'strings' if s is not None else 'globals'
        hint = f'via {C.globals[g]}' if C.globals[g] else ''
        if b is not None and any(layout.block(h) == b for h in e_glob):
            hint = (hint + '; ' if hint else '') + 'the exe uses another address of the same block'
        add_open(kind, 'cpp', f'{g:#x}' + (f' {s[:60]!r}' if s is not None else ''), hint)
    # string literals of the C++ that the exe has as no address
    for s, via in sorted(C.strings.items()):
        if s in e_texts:
            if e_texts[s] not in c_glob:
                add_exp('strings', 'cpp', repr(s[:60]), 'string_by_text')
            continue
        add_open('strings', 'cpp', repr(s[:60]), f'via {via}' if via else '')

    # ---- integer constants
    c_vals = set(c_ints) | {u32(x) for x in c_ints}
    folded = C.folded
    e_consts = E['consts']
    c_float_bits = set()
    for v, w in C.floats:
        b = f32_bits(v)
        if b is not None:
            c_float_bits.add(b)
        try:
            q = struct.unpack('<Q', struct.pack('<d', v))[0]
            c_float_bits.add(q & 0xffffffff)
            c_float_bits.add(q >> 32)
        except (OverflowError, struct.error):
            pass
    for g in C.globals:
        if exe.section(g) == '.rdata':
            b = exe.read(g, 4)
            if b:
                c_float_bits.add(struct.unpack('<I', b)[0])
    magics = {}
    for c in c_vals:
        if 2 <= c < 1 << 20:
            for m in div_magics(c):
                magics.setdefault(m, c)
    cmp_next = collections.defaultdict(set)
    for v, j in E['cmp']:
        cmp_next[v].add(j)
    addsub = E['addsub']
    used_ops = set()
    for v in sorted(e_consts):
        sv = s32(v)
        if v in c_vals or sv in c_vals:
            continue
        if ('const', v) in expl_e:
            add_exp('constants', 'exe', f'{v:#x}', 'callee_inlined_exe')
            continue
        if v in E['fpu_masks']:
            add_exp('constants', 'exe', f'{v:#x}', 'fpu_flags')
            continue
        if v in E['width']:
            add_exp('constants', 'exe', f'{v:#x}', 'width_mask')
            continue
        if v in folded or sv in folded:
            ops = folded.get(v) or folded.get(sv)
            add_exp('constants', 'exe', f'{v:#x}', 'folded', f'from {", ".join(hex(o) for o in ops[0])}')
            used_ops.update(ops[0])
            continue
        if (('add', sv) in addsub or ('sub', sv) in addsub) and (-sv in c_vals or u32(-sv) in c_vals):
            add_exp('constants', 'exe', f'{v:#x}', 'negated', f'{-sv:#x}')
            continue
        js = cmp_next.get(v, set())
        if js & {'jle', 'jg', 'jbe', 'ja', 'setle', 'setg', 'setbe', 'seta'} and (sv + 1 in c_vals):
            add_exp('constants', 'exe', f'{v:#x}', 'compare_bound', f'C++ {sv + 1:#x}')
            continue
        if js & {'jl', 'jge', 'jb', 'jae', 'setl', 'setge', 'setb', 'setae'} and (sv - 1 in c_vals):
            add_exp('constants', 'exe', f'{v:#x}', 'compare_bound', f'C++ {sv - 1:#x}')
            continue
        if v in magics:
            add_exp('constants', 'exe', f'{v:#x}', 'div_magic', f'/ {magics[v]}')
            continue
        if v in c_float_bits:
            add_exp('constants', 'exe', f'{v:#x}', 'float_bits')
            continue
        add_open('constants', 'exe', f'{v:#x}' if sv >= 0 else f'{sv:#x}')
    e_vals = set(e_consts) | {s32(x) for x in e_consts}
    e_fields = set(E['fields']) | {u32(x) for x in E['fields']}
    e_float_bits = set()
    for k, val in E['floats'].items():
        if k[0] == 'addr' and val is not None:
            b = exe.read(k[1], 4)
            if b:
                e_float_bits.add(struct.unpack('<I', b)[0])
    for c in sorted(c_ints):
        if abs(c) < MIN_CONST:
            continue
        uc = u32(c)
        if c in e_vals or uc in e_vals:
            continue
        if ('const', uc) in expl_c:
            add_exp('constants', 'cpp', f'{uc:#x}', 'callee_inlined_cpp')
            continue
        if c in E['fields'] or uc in e_fields:
            add_exp('constants', 'cpp', f'{c:#x}', 'field_in_exe')
            continue
        if c in C.slot_ints or (4 * c) in e_slots:
            add_exp('constants', 'cpp', f'{c:#x}', 'slot_index')
            continue
        if c in used_ops:
            add_exp('constants', 'cpp', f'{c:#x}', 'folded')
            continue
        if c in magics.values() and any(m in e_consts for m, d in magics.items() if d == c):
            add_exp('constants', 'cpp', f'{c:#x}', 'div_magic')
            continue
        if (c - 1 in e_vals and cmp_next.get(u32(c - 1), set()) & {'jle', 'jg', 'jbe', 'ja', 'setle', 'setg', 'setbe', 'seta'}) or \
           (c + 1 in e_vals and cmp_next.get(u32(c + 1), set()) & {'jl', 'jge', 'jb', 'jae', 'setl', 'setge', 'setb', 'setae'}):
            add_exp('constants', 'cpp', f'{c:#x}', 'compare_bound')
            continue
        if (('add', -c) in addsub or ('sub', -c) in addsub):
            add_exp('constants', 'cpp', f'{c:#x}', 'negated')
            continue
        if uc in e_float_bits:
            add_exp('constants', 'cpp', f'{c:#x}', 'float_bits')
            continue
        add_open('constants', 'cpp', f'{c:#x}' if c >= 0 else f'{c:#x}', f'via {c_ints[c]}' if c_ints[c] else '')
    # float literals of the C++ the exe has no constant for
    e_vals_f = {(v, k[2] if k[0] == 'addr' else 0) for k, v in E['floats'].items()}
    for (v, w), via in sorted(C.floats.items(), key=lambda x: (str(x[0][0]), x[0][1])):
        ok = False
        for ev, ew in e_vals_f:
            if ev is None:
                continue
            if ev == v or (as_f32(v) is not None and ev == as_f32(v)) or (ew == 4 and as_f32(v) == ev):
                ok = True
                break
        if ok:
            continue
        b = f32_bits(v)
        if b is not None and (b in e_consts):
            add_exp('constants', 'cpp', repr(v), 'float_bits')
            continue
        if any(k[0] == 'builtin' and val == v for k, val in E['floats'].items()):
            add_exp('constants', 'cpp', repr(v), 'x87_builtin')
            continue
        if v == int(v) if v == v and abs(v) < 1e18 else False:
            iv = int(v)
            if abs(iv) < MIN_CONST:
                continue   # a small whole number: trivial as an integer is
        add_open('constants', 'cpp', repr(v), f'via {via}' if via else '')
    return explained, open_


# ---- main -----------------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', default=os.environ.get('ZAX_EXE') or str(ROOT / 'gamedata/Zax - German/Zax.exe'))
    ap.add_argument('--out', default=None)
    ap.add_argument('--only', nargs='*', default=None, help='addresses to compare (prints them)')
    args = ap.parse_args()
    exe = ExeModel(args.exe)
    layout = Layout()
    frows = list(csv.DictReader(open(ROOT / 'docs/functions.csv')))
    rrows = list(csv.DictReader(open(ROOT / 'docs/reimpl.csv')))
    sizes = {int(r['address'], 16): int(r['size'] or 0) for r in frows}
    names = {int(r['address'], 16): (f"{r['class']}::{r['name']}" if r['class'] else r['name']) for r in frows}
    subsystem = {int(r['address'], 16): r['subsystem'] for r in frows}
    fnames = collections.defaultdict(list)
    for r in frows:
        fnames[r['name']].append(int(r['address'], 16))
    reimpl = {int(r['address'], 16): r for r in rrows}
    for a, r in reimpl.items():
        names.setdefault(a, r['name'])
    starts = set(sizes) | set(reimpl)
    asm_ranges = [rg for mod in crt_asm_gen.MODULES for rg in mod.ranges]
    todo = sorted(reimpl)
    if args.only:
        todo = [int(x, 16) for x in args.only]
    # the exe functions: the compared ones and every function they or their C++ call (for the inlining rules)
    print(f'exe: descending {len(starts)} functions', file=sys.stderr)
    exe_funcs = exe_functions(exe, starts, sizes, layout)
    print('C++: reading reimpl/src', file=sys.stderr)
    src = Source()
    res = Resolver(src, exe, layout, fnames)
    hooks_by_addr = collections.defaultdict(list)
    for a, name, rel, label in src.hooks:
        hooks_by_addr[a].append((name, rel, label))
    checked = set()
    for f in src.files:
        if f.startswith('checks_'):
            for t in src.files[f]:
                if t.k == 'num' and t.v and t.v[0] == 'int' and t.v[1] in reimpl:
                    checked.add(t.v[1])
    ctx = {'exe': exe, 'sizes': sizes, 'fnames': fnames, 'exe_funcs': exe_funcs, 'vt_slots': vtable_slots(exe, layout),
           'names': names, 'reimpl': reimpl, 'layout': layout}
    out_dir = Path(args.out) if args.out else workspace_runs() / 'static-audit'
    if not args.only:
        out_dir.mkdir(parents=True, exist_ok=True)
    results = []
    for a in todo:
        r = reimpl.get(a, {})
        rec = {'address': f'{a:#x}', 'name': names.get(a, ''), 'subsystem': subsystem.get(a, '') or 'unassigned',
               'file': (r.get('file') or '').replace('reimpl/src/', ''), 'calibration': a in checked}
        ef = exe_funcs.get(a)
        hk = hooks_by_addr.get(a)
        if any(s <= a < e for s, e in asm_ranges):
            rec['status'] = 'generated assembler (tools/crt_asm_gen.py checks its bytes)'
            results.append(rec)
            continue
        if not hk:
            rec['status'] = 'no hook entry found'
            results.append(rec)
            continue
        name, rel, label = hk[0]
        owner = src.inc_owner.get(rel, rel)
        if owner == 'short_hooks.cpp':
            rec['status'] = 'generated from the exe\'s bytes (tools/short_hooks_gen.py)'
            rec['symbol'] = name
            results.append(rec)
            continue
        stoks, _ = tokenize(name)
        k = next((j for j, t in enumerate(stoks) if t.t == '<'), len(stoks))
        base = stoks[k - 1].t if k else name
        targs = (stoks, k + 1, match_close(stoks, k, '<', '>')) if k < len(stoks) else None
        squal = '::'.join(t.t for t in stoks[:k - 1] if t.k == 'id') or None
        defs = res.definition(base, owner, False, squal)
        if not defs and squal:
            defs = res.definition(base, owner, False, squal.split('::')[-1])
        if not defs and base.startswith('zaxlib_'):
            base = base[len('zaxlib_'):]
            defs = res.definition(base, owner) or [d for d in src.defs.get(base, []) if not d.macro][:1]
        asm_def = False
        if not defs:
            defs = src.asm_label(base)
            asm_def = bool(defs)
        if not defs:
            rec['status'] = f'no definition of {name} found'
            rec['symbol'] = name
            results.append(rec)
            continue
        if not rec['file']:
            rec['file'] = owner
        cc = Collector(res, owner)
        binds = cc.targ_binds(defs, targs, owner, {})
        for d in defs:
            cc.run(d, binds, [d.qual], 0)
        if asm_def:
            cc.asm = True
        cc.code.pop(a, None)
        explained, open_ = compare(a, ef, cc, ctx)
        rec.update({
            'status': 'compared', 'symbol': name, 'definition': f'{defs[0].file}:{defs[0].line}',
            'exe': {'calls': sorted(f'{t:#x}' for t in ef['code']), 'imports': sorted(ef['imports']),
                    'vslots': sorted(ef['vslots']), 'globals': sorted(f'{g:#x}' for g in ef['globals']),
                    'consts': sorted(f'{c:#x}' for c in ef['consts']), 'fields': sorted(ef['fields']),
                    'floats': sorted(repr(v) for v in ef['floats'].values()), 'instructions': ef['ninsn']},
            'cpp': {'calls': sorted(f'{t:#x}' for t in cc.code), 'imports': sorted(cc.imports),
                    'externals': sorted(cc.externals), 'vslots': sorted(cc.vslots),
                    'globals': sorted(f'{g:#x}' for g in cc.globals),
                    'ints': sorted((f'{c:#x}' for c in cc.ints), key=lambda x: int(x, 16)),
                    'floats': sorted(repr(v) for v, _ in cc.floats), 'strings': sorted(cc.strings),
                    'helpers': sorted(cc.helpers), 'asm': cc.asm, 'unresolved_slots': cc.unresolved_slots, 'indirect_calls': cc.indirect,
                    'ambiguous': sorted(cc.ambiguous)},
            'explained': explained, 'open': open_,
        })
        results.append(rec)
    if args.only:
        for rec in results:
            for k, v in rec.items():
                if isinstance(v, dict):
                    for k2, v2 in v.items():
                        print(f'  {k}.{k2}: {json.dumps(v2)}')
                elif isinstance(v, list):
                    print(f'  {k}:')
                    for x in v:
                        print(f'    {json.dumps(x)}')
                else:
                    print(f'{k}: {v}')
            print()
        return
    write_outputs(out_dir, results)


def write_outputs(out_dir, results):
    with open(out_dir / 'functions.jsonl', 'w') as f:
        for rec in results:
            f.write(json.dumps(rec, sort_keys=False) + '\n')
    comp = [r for r in results if r['status'] == 'compared']
    rule_n = collections.Counter(e['rule'] for r in comp for e in r['explained'])
    rule_cal = collections.Counter(e['rule'] for r in comp if r['calibration'] for e in r['explained'])
    kinds = collections.Counter((o['kind'], o['side']) for r in comp for o in r['open'])
    kinds_cal = collections.Counter((o['kind'], o['side']) for r in comp if r['calibration'] for o in r['open'])
    status = collections.Counter(r['status'] if not r['status'].startswith('no definition') else 'no definition found'
                                 for r in results)
    clean = sum(1 for r in comp if not r['open'])
    clean_cal = sum(1 for r in comp if r['calibration'] and not r['open'])
    ncal = sum(1 for r in comp if r['calibration'])
    by_sub = collections.defaultdict(lambda: [0, 0, 0])
    by_file = collections.defaultdict(lambda: [0, 0, 0, ''])
    for r in comp:
        s = by_sub[r['subsystem']]
        s[0] += 1
        s[1] += not r['open']
        s[2] += len(r['open'])
        fl = by_file[r['file']]
        fl[0] += 1
        fl[1] += not r['open']
        fl[2] += len(r['open'])
        fl[3] = r['subsystem'] if not fl[3] else fl[3]
    parts = partition(comp)
    with open(out_dir / 'partitions.json', 'w') as f:
        json.dump(parts, f, indent=1)
    L = []
    L.append('# Static audit of zax-decomp against Zax.exe\n')
    L.append('Written by `tools/static_audit.py` (docs/DECOMPILATION.md 5). Every function of `docs/reimpl.csv`: '
             'the exe function\'s calls, imports, vtable slots, globals, strings and constants against the same sets '
             'of its C++ definition (helpers expanded). A difference is matched, explained by a rule, or open.\n')
    L.append('## Totals\n')
    L.append(f'- functions of docs/reimpl.csv: {len(results)}')
    for k, n in status.most_common():
        L.append(f'  - {k}: {n}')
    L.append(f'- compared functions with no open difference: {clean} of {len(comp)}')
    L.append(f'- calibration set (the functions whose address a `checks_*.cpp` module uses, compared at run time): '
             f'{ncal}, with no open difference: {clean_cal}')
    L.append(f'- explained differences: {sum(rule_n.values())}; open differences: {sum(kinds.values())}\n')
    L.append('## Open differences by kind\n')
    L.append('| kind | only in the exe | only in the C++ | calibration set (exe / C++) |')
    L.append('|---|---|---|---|')
    for k in ('calls', 'vslots', 'globals', 'strings', 'constants'):
        L.append(f'| {k} | {kinds[(k, "exe")]} | {kinds[(k, "cpp")]} | {kinds_cal[(k, "exe")]} / {kinds_cal[(k, "cpp")]} |')
    L.append('\n## Rules and how many differences each explained\n')
    L.append('| rule | explained | in the calibration set | meaning |')
    L.append('|---|---|---|---|')
    for k, text in RULES.items():
        L.append(f'| `{k}` | {rule_n[k]} | {rule_cal[k]} | {text} |')
    L.append('\nNot differences at all (no rule needed): integer immediates with |v| < '
             f'{MIN_CONST:#x}, shift counts on both sides, stack frame arithmetic (`add/sub/and esp`, `ret n`, '
             '[esp + d], [ebp + d] where the function sets up ebp as its frame), array dimensions of C++ declarations; '
             'the C++\'s calls of functions it does not define and the exe does not import (compiler builtins, the '
             'reimplementation\'s own runtime) are listed per function as `externals`, not compared. `x87_ftol` of the '
             'C++ counts as a call of __ftol 0x5d0488 (the same truncating conversion).\n')
    L.append('## What the tool cannot see\n')
    L.append('- Control flow and order: two functions with the same sets can still differ in their branches, loop '
             'bounds held in registers, argument order or the number of calls of one callee.')
    L.append('- Constants below the threshold, field offsets of the C++ written as struct members (only literals '
             'count), arithmetic the compiler turns into lea/shift sequences.')
    L.append('- Indirect calls through registers (`call eax`) and function pointers in structs the C++ does not reach '
             'through `vcall`; vtable calls with a slot known only at run time.')
    L.append('- C++ helpers resolved by name: an overloaded or shadowed name may pick the wrong definition; a helper '
             'defined in no file the module includes is not expanded (listed as an external).')
    L.append('- Member calls whose receiver type the tool cannot read (a template parameter, an expression) and names '
             'a macro builds at the call (`ZAX_GROUPS(fill)`): not resolved (`ambiguous` per function).')
    L.append('- Preprocessor conditions: only `#if 0` and `#ifdef` of a name nothing defines are left out; every other '
             'conditional counts with both branches (the DLL\'s and the standalone exe\'s code together).')
    L.append('- Constants only in the C++ are mostly field offsets the exe reaches from another base (an embedded '
             'object\'s register, a pointer advanced by lea) and the operands of arithmetic the compiler rewrote; '
             'they are the noisiest kind and the first to review with the code side by side.')
    L.append('- Generated code is not compared: the assembler modules of tools/crt_asm_gen.py and the short hooks of '
             'tools/short_hooks_gen.py (both made from and checked against the exe\'s bytes).\n')
    L.append('## Per subsystem\n')
    L.append('| subsystem | compared | no open difference | open differences |')
    L.append('|---|---|---|---|')
    for s, (n, c, o) in sorted(by_sub.items(), key=lambda x: -x[1][2]):
        L.append(f'| {s} | {n} | {c} | {o} |')
    L.append('\n## Per file\n')
    L.append('| file | subsystem | compared | no open difference | open differences |')
    L.append('|---|---|---|---|---|')
    for fl, (n, c, o, s) in sorted(by_file.items(), key=lambda x: (-x[1][2], x[0])):
        L.append(f'| {fl} | {s} | {n} | {c} | {o} |')
    L.append('\n## Partitions\n')
    for p in parts['parts']:
        L.append(f'- part {p["part"]}: {p["open"]} open differences in {p["functions"]} functions, {len(p["files"])} '
                 f'files ({", ".join(p["subsystems"])})')
    (out_dir / 'summary.md').write_text('\n'.join(L) + '\n')
    print(f'{out_dir}: {len(results)} functions, {len(comp)} compared, {clean} without open difference, '
          f'{sum(rule_n.values())} explained, {sum(kinds.values())} open', file=sys.stderr)


def partition(comp):
    """the open differences in PARTS parts of about equal review work: whole files, ordered by subsystem and file,
    cut where the running sum passes k/PARTS of the work (work = open differences + 1 per function with any)."""
    files = collections.defaultdict(list)
    sub = {}
    for r in comp:
        if r['open']:
            files[r['file']].append(r)
    for f, rs in files.items():
        sub[f] = collections.Counter(r['subsystem'] for r in rs).most_common(1)[0][0]
    order = sorted(files, key=lambda f: (sub[f], f))
    work = {f: sum(len(r['open']) + 1 for r in files[f]) for f in order}
    total = sum(work.values())
    parts, cur, acc, k = [], [], 0, 1
    for f in order:
        cur.append(f)
        acc += work[f]
        if acc >= total * k / PARTS and k < PARTS:
            parts.append(cur)
            cur, k = [], k + 1
    if cur:
        parts.append(cur)
    out = []
    for n, fs in enumerate(parts, 1):
        rs = [r for f in fs for r in files[f]]
        out.append({'part': n, 'work': sum(work[f] for f in fs), 'open': sum(len(r['open']) for r in rs),
                    'functions': len(rs), 'subsystems': sorted({sub[f] for f in fs}), 'files': fs,
                    'addresses': [r['address'] for r in rs]})
    return {'rule': partition.__doc__.split('\n')[0] + ' ' + ' '.join(partition.__doc__.split('\n')[1:]).strip(),
            'total_work': total, 'parts': out}


if __name__ == '__main__':
    main()
