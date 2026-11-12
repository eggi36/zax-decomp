#!/usr/bin/env python3
"""Generate the hand-written assembly parts of the exe's MSVC 6.0 C runtime as reimplementation modules.

The x87 math library (the _CI* intrinsics, pow, asin, acos, fmod and their dispatch and error helpers,
tran/i386/*.asm of VC6) and the structured exception helpers (exsup.asm, exsup3.asm, lowhelpr.asm) were
written in assembly; their results depend on the exact x87 instruction sequence. They are therefore
reimplemented as the same instructions, written out here as GNU assembler text (Intel syntax) from the
exe's code: every branch between the ranges becomes a label, every call or jump out of them goes through
a pointer to the exe's entry point (which the hooks send to the reimplementation where there is one), and
the dispatch descriptors that hold code addresses are copied with the addresses replaced by the labels.
Data stays in the exe (absolute addresses).

The generated module is checked here: it is assembled with clang and every instruction that is not a
branch or a replaced address must have the original's bytes (the replaced ones the original's length
apart from the external jumps); a branch from anywhere in the ranges into the first five bytes of a
hooked entry point is an error (the hook's jump would be entered in the middle).

Usage: tools/crt_asm_gen.py [--exe "$(python3 -I tools/game_paths.py exe)"] [--clang PATH]   (writes reimpl/src/crt_x87.cpp,
reimpl/src/crt_seh.cpp)
"""
import argparse
import csv
import re
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import game_paths  # noqa: E402

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x400000


# ---- what is generated ------------------------------------------------------------------------------------

class Module:
    def __init__(self, path, hooks_name, title, ranges, entries, tables=(), data=(), notes=''):
        self.path = path
        self.hooks_name = hooks_name
        self.title = title
        self.ranges = ranges        # [(start, end)] of code, end exclusive
        self.entries = entries      # [(address, name, hooked, comment)]
        self.tables = tables        # [(address, size, comment)]: data copied, code pointers replaced
        self.data = data            # [(start, end)]: bytes inside a range emitted as .byte (not code)
        self.notes = notes


X87 = Module(
    'reimpl/src/crt_x87.cpp', 'crt_x87',
    'The x87 math library of the C runtime: the intrinsics the compiler calls (_CIpow, _CIasin, _CIacos, '
    '_CIfmod) and the C entry points pow, asin, acos, fmod, their dispatchers (__trandisp1/2, '
    '__ctrandisp1/2, __cintrindisp1/2), argument loaders (__fload, __fload_withFB), the error exits '
    '(__startOneArgErrorHandling, __math_exit and their variants), the Pentium fprem workaround and the five '
    'small functions of ieee87.c/frnd.c that are inline assembly in C (_frnd, _statfp, _clrfp, _ctrlfp, _set_statfp)',
    ranges=[(0x5d12d0, 0x5d139b), (0x5d23e0, 0x5d25fd), (0x5d2600, 0x5d2632), (0x5d4f90, 0x5d515a),
            (0x5d5160, 0x5d534c), (0x5d5b09, 0x5d5b1b), (0x5d5c5f, 0x5d5cf5), (0x5d99f0, 0x5d9c5a), (0x5da466, 0x5da71e), (0x5e97e0, 0x5e98ab)],
    entries=[
        (0x5d12d0, '_CIasin', True, 'asin of st(0); the argument also on the stack as a double'),
        (0x5d12e4, 'asin', True, 'asin(double): loads the argument (__fload_withFB), then the common body'),
        (0x5d12ed, '_asin_body', True, 'the common body: eax the argument\'s high word, the flags of the load'),
        (0x5d23e0, '_CIpow', True, 'pow of st(1), st(0)'),
        (0x5d23f9, 'pow', True, 'pow(double, double)'),
        (0x5d2402, '_pow_body', True, 'the common body: x in st(1) and at [esp+4], y in st(0) and at [esp+12]'),
        (0x5d25d5, '_pow_isint', True, 'whether st(0) is an integer (cl 1) and an odd one (cl 2); pops it when not'),
        (0x5d2600, 'fmod', True, 'fmod(double, double) through __ctrandisp2 with the fmod descriptor'),
        (0x5d260a, '_CIfmod', True, 'fmod of st(1), st(0) through __cintrindisp2'),
        (0x5d2614, '_fmod_core', False, 'fprem until the reduction is complete (or the Pentium workaround)'),
        (0x5d4f90, '__trandisp1', True, 'dispatch on the class of st(0) through the descriptor in edx'),
        (0x5d4ff7, '__trandisp2', True, 'dispatch on the classes of st(1) and st(0)'),
        (0x5d5160, '__startTwoArgErrorHandling', True, 'like __startOneArgErrorHandling with a second argument'),
        (0x5d5177, '__startOneArgErrorHandling', True, 'the result into the exception record, _87except, the result back, the control word restored'),
        (0x5d51c0, '_twoToTOS', True, '2^st(0)'),
        (0x5d51d5, '_load_CW', True, 'the caller\'s control word with all exceptions masked and its precision and rounding'),
        (0x5d51ec, '_convertTOStoQNaN', True, 'st(0) a QNaN: an SNaN made quiet (eax 1) or kept (eax 7)'),
        (0x5d5205, '__fload_withFB', True, 'the double at [edx] into st(0), a NaN or infinity as an 80-bit value; eax its high word'),
        (0x5d5248, '_checkTOS_withFB', True, 'eax the high word of the double at [esp+8]'),
        (0x5d525e, '_fast_exit_restore_cw', True, 'the saved control word back (unless it is the default), pop edx'),
        (0x5d526b, '__math_exit', True, 'the control word back; with a precision exception unmasked and raised the error handler'),
        (0x5d52a9, '_math_exit_checkrange', True, 'like __math_exit, after checking the result for underflow/overflow'),
        (0x5d5b09, '_frnd', True, 'frndint of the double argument (C with inline assembly in VC6)'),
        (0x5d5c5f, '_statfp', True, 'the status word (sign-extended)'),
        (0x5d5c6d, '_clrfp', True, 'the status word, then fnclex'),
        (0x5d5c7c, '_ctrlfp', True, 'control word = (new & mask) | (old & ~mask); the old one (sign-extended)'),
        (0x5d5c9f, '_set_statfp', True, 'raises the exceptions of the status bits by operations that cause them'),
        (0x5d99f0, '__cintrindisp2', True, 'the frame of a two-argument intrinsic, __trandisp2, the error check'),
        (0x5d9a2e, '__cintrindisp1', True, 'the frame of a one-argument intrinsic, __trandisp1, the error check'),
        (0x5d9a6b, '__ctrandisp2', True, 'the frame of a two-argument C function: both arguments loaded, __trandisp2'),
        (0x5d9ab3, '_ctrandisp_exit', True, 'marks a C call, then the error check'),
        (0x5d9aba, '_trandisp_errorcheck', True, 'the result checked for errors, _87except on one'),
        (0x5d9beb, '__ctrandisp1', True, 'the frame of a one-argument C function'),
        (0x5d9c1e, '__fload', True, 'the double argument into st(0), a NaN or infinity as an 80-bit value'),
        (0x5da466, '_fprem_common', True, 'the Pentium FDIV-bug-safe fprem for operands in the dangerous range'),
        (0x5da66c, '_adj_fprem', True, 'fprem with the Pentium workaround (selected when _adjust_fdiv is set)'),
        (0x5e97e0, '_CIacos', True, 'acos of st(0)'),
        (0x5e97f4, 'acos', True, 'acos(double)'),
        (0x5e97fd, '_acos_body', True, 'the common body'),
    ],
    tables=[(0x6297d0, 0x50, 'the fmod descriptor: name, error type, 16 handlers by the classes of both arguments')],
)

SEH = Module(
    'reimpl/src/crt_seh.cpp', 'crt_seh',
    'The structured exception helpers of the C runtime (exsup.asm, exsup3.asm, lowhelpr.asm): '
    '_except_handler3 (the handler of every __try frame of the exe, with the "VC20XC00" signature before '
    'it), __global_unwind2, __local_unwind2 and its unwind handler, _abnormal_termination, the '
    '__NLG_Notify hooks for debuggers, _seh_longjmp_unwind and __CallSettingFrame',
    ranges=[(0x5d220c, 0x5d22fa), (0x5d22fc, 0x5d23dc), (0x5db470, 0x5db4bc)],
    entries=[
        (0x5d220c, '__global_unwind2', True, 'RtlUnwind up to the frame, with the unwind handler below'),
        (0x5d222c, '__unwind_handler', True, 'the handler __local_unwind2 registers: a collided unwind gets the frame to continue from'),
        (0x5d224e, '__local_unwind2', True, 'runs the __finally blocks of a frame down to a try level'),
        (0x5d22b6, '__abnormal_termination', True, 'whether the __finally block runs during an unwind'),
        (0x5d22d9, '__NLG_Notify1', True, 'records the target for a debugger (eax the address, ebx the data block)'),
        (0x5d22e2, '__NLG_Notify', True, 'the same with the frame\'s first argument'),
        (0x5d2304, '_except_handler3', True, 'the exe\'s frame handler: filters, unwinds, the __except block'),
        (0x5d23c1, '_seh_longjmp_unwind', True, 'longjmp\'s unwind of a frame (jmp_buf\'s frame and try level)'),
        (0x5db470, '__CallSettingFrame@12', True, 'calls a funclet with the frame\'s ebp (the C++ catch/unwind helper)'),
    ],
    data=[(0x5d22fc, 0x5d2304)],
)

SEHC = Module(
    'reimpl/src/crt_sehc.cpp', 'crt_sehc',
    'The C functions of the C runtime whose frame is a __try block (crt0.c, winxfltr.c\'s terminate, a_map.c, '
    'a_str.c, a_cmp.c): WinMainCRTStartup (the program\'s entry with its __except filter and exit), terminate '
    '(the terminate handler under __try/__finally, then abort), __crtLCMapStringA, __crtGetStringTypeA and '
    '__crtCompareStringA (their _alloca buffers under __try/__except). The frame layout that _except_handler3 and '
    'the scope tables rely on (the try level at [ebp-4], the saved esp at [ebp-0x18], the filters and handlers '
    'as code inside the function) is kept by keeping the instructions; their scope tables are copied',
    ranges=[(0x5d328f, 0x5d3397), (0x5d437b, 0x5d459f), (0x5db4bc, 0x5db51d), (0x5db59e, 0x5db6e7), (0x5ddd54, 0x5ddfd1)],
    entries=[
        (0x5d328f, 'WinMainCRTStartup', True, 'the entry point: version, heap, threads, files, command line, environment, '
                   '_cinit, WinMain, exit; an exception reaching it goes through _XcptFilter and _exit'),
        (0x5d437b, '__crtLCMapStringA', True, 'LCMapStringA, or LCMapStringW through MultiByteToWideChar on NT'),
        (0x5db4bc, 'terminate', True, 'the thread\'s terminate handler (+0x60), then abort'),
        (0x5db59e, '__crtGetStringTypeA', True, 'GetStringTypeW through MultiByteToWideChar, or GetStringTypeA'),
        (0x5ddd54, '__crtCompareStringA', True, 'CompareStringW through MultiByteToWideChar, or CompareStringA'),
    ],
    tables=[(0x607080, 12, 'WinMainCRTStartup\'s scope table: filter, handler'),
            (0x607138, 24, '__crtLCMapStringA\'s scope table'),
            (0x607658, 24, 'terminate\'s scope table: the __finally, the __except'),
            (0x607670, 12, '__crtGetStringTypeA\'s scope table'),
            (0x6076d8, 24, '__crtCompareStringA\'s scope table')],
)

THUNKS = Module(
    'reimpl/src/crt_thunks.cpp', 'crt_thunks',
    'Two compiler/linker thunks of the C runtime area: the virtual call thunk 0x4020c0 (MSVC\'s '
    '`vcall\'{304}\': jumps to slot 0x130 / 4 = 76 of the object\'s vtable in ecx; batch C4 found it belongs to the '
    'runtime) and the import thunk of RtlUnwind that __global_unwind2 calls',
    ranges=[(0x4020c0, 0x4020c8), (0x5e96f0, 0x5e96f6)],
    entries=[
        (0x4020c0, 'vcall_304', True, 'jmp [[ecx] + 0x130]'),
        (0x5e96f0, 'RtlUnwind_thunk', True, 'jmp [RtlUnwind import slot]'),
    ],
)

MODULES = [X87, SEH, SEHC, THUNKS]

# The exe's floating point library as the checks' original side (checks_crt_rest.cpp): these ranges are
# copied from the exe (the hooked sites with their original bytes) and the rel32 branches out of a range
# relocated, to the copy where the target lies in a copied range. The original then runs entirely as the
# original, down to the runtime's other functions (errno, RaiseException), not through the hooks.
FP_COPY = [(0x5d12d0, 0x5d1539), (0x5d23e0, 0x5d2632), (0x5d4f90, 0x5d5cf5), (0x5d9857, 0x5d9c5a),
           (0x5da466, 0x5da71e), (0x5dbfa4, 0x5dc10a), (0x5dc4c5, 0x5dc598), (0x5dca99, 0x5dcb4c),
           (0x5ddb6e, 0x5ddd54), (0x5e97e0, 0x5e98ab)]
COPY_PATH = 'reimpl/src/checks_crt_copy.inc'


def copy_table(exe):
    pe = pefile.PE(exe, fast_load=True)
    img = pe.get_memory_mapped_image()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    inside = lambda v: any(s <= v < e for s, e in FP_COPY)
    fields = []
    for s, e in FP_COPY:
        a = s
        while a < e:
            i = next(md.disasm(bytes(img[a - BASE:a - BASE + 16]), a, 1))
            if i.mnemonic == 'int3':
                a += 1
                continue
            for op in i.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.index != 0 and s <= (op.mem.disp & 0xffffffff) < e:
                    raise SystemExit(f'{a:#x}: a table inside the copied code ({i.mnemonic} {i.op_str})')
            if i.group(capstone.CS_GRP_JUMP) or i.group(capstone.CS_GRP_CALL):
                op = i.operands[0]
                if op.type == capstone.x86.X86_OP_IMM:
                    t = op.imm & 0xffffffff
                    if not s <= t < e:
                        if i.size < 5:
                            raise SystemExit(f'{a:#x}: short branch out of its range')
                        fields.append((a + i.size - 4, t))
            a += i.size
        if a != e:
            raise SystemExit(f'copy range {s:#x}-{e:#x} ends inside an instruction')
    o = ['// Generated by tools/crt_asm_gen.py: the exe\'s floating point library copied for the checks\' original',
         '// side (checks_crt_rest.cpp). Ranges, then the rel32 fields that branch out of their range.',
         'const DWORD FP_COPY_RANGES[][2] = {']
    o += [f'    {{0x{s:x}, 0x{e:x}}},' for s, e in FP_COPY]
    o += ['};', 'const DWORD FP_COPY_REL32[] = {']
    for k in range(0, len(fields), 8):
        o.append('    ' + ' '.join(f'0x{f:x},' for f, t in fields[k:k + 8]))
    o += ['};']
    (ROOT / COPY_PATH).write_text('\n'.join(o) + '\n')
    print(f'{COPY_PATH}: {len(FP_COPY)} ranges, {len(fields)} relocated branches')


# ---- generation -------------------------------------------------------------------------------------------

def load_names():
    names = {}
    for path, col in ((ROOT / 'docs/functions.csv', 'name'), (ROOT / 'docs/exe_symbols.csv', 'name')):
        for r in csv.DictReader(open(path, encoding='utf-8')):
            try:
                names.setdefault(int(r['address'], 16), r[col])
            except ValueError:
                pass
    return names


def sym(name):
    return re.sub(r'[^A-Za-z0-9_]', '_', name)


class Gen:
    def __init__(self, exe, mod, names):
        self.pe = pefile.PE(exe, fast_load=True)
        self.img = self.pe.get_memory_mapped_image()
        self.mod = mod
        self.names = names
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.ins = {}       # address -> capstone instruction
        self.order = []
        self.datab = {}     # address -> (end) of data blocks
        for s, e in mod.ranges:
            a = s
            while a < e:
                d = next((d for d in mod.data if d[0] == a), None)
                if d:
                    self.datab[a] = d[1]
                    self.order.append(('data', a))
                    a = d[1]
                    continue
                i = next(self.md.disasm(self.bytes(a, 16), a, 1))
                if i.mnemonic == 'int3':   # padding between functions
                    a += 1
                    continue
                self.ins[a] = i
                self.order.append(('ins', a))
                a += i.size
            if a != e:
                raise SystemExit(f'range {s:#x}-{e:#x} ends inside an instruction at {a:#x}')
        self.entry = {a: (n, h, c) for a, n, h, c in mod.entries}
        for a in self.entry:
            if a not in self.ins:
                raise SystemExit(f'entry {a:#x} is no instruction start')
        self.ext = {}       # external address -> pointer symbol
        self.targets = set()
        for t, size, c in mod.tables:   # handlers named by the copied descriptors keep their labels
            for k in range(0, size, 4):
                v = int.from_bytes(self.bytes(t + k, 4), 'little')
                if self.inside(v):
                    self.targets.add(v)

    def bytes(self, a, n):
        return bytes(self.img[a - BASE:a - BASE + n])

    def inside(self, a):
        return any(s <= a < e for s, e in self.mod.ranges)

    def label(self, a):
        if a in self.entry:   # the C name zax_<name> (i386 COFF: with a leading underscore)
            return '_zax_' + sym(self.entry[a][0])
        return f'L{a:x}'

    def table_label(self, a):
        return f'T{a:x}'

    def ext_ptr(self, a):
        if a not in self.ext:
            self.ext[a] = f'X{a:x}'
        return self.ext[a]

    def ext_name(self, a):
        return self.names.get(a, f'{a:#x}')

    def table_at(self, v):
        return next((t for t in self.mod.tables if t[0] == v), None)

    def operands(self, i):
        """capstone's Intel text with immediates naming code or a copied table replaced"""
        text = i.op_str
        for op in i.operands:
            if op.type == capstone.x86.X86_OP_IMM:
                v = op.imm & 0xffffffff
                if self.inside(v) or self.table_at(v):
                    if self.inside(v) and v not in self.ins:
                        raise SystemExit(f'{i.address:#x}: immediate {v:#x} inside a range but no instruction')
                    rep = self.table_label(v) if self.table_at(v) else self.label(v)
                    if self.inside(v):
                        self.targets.add(v)
                    text = re.sub(rf'\b{v:#x}\b', 'offset ' + rep, text)
            elif op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                v = op.mem.disp & 0xffffffff
                if self.inside(v) or self.table_at(v):
                    raise SystemExit(f'{i.address:#x}: memory operand into a range/table {v:#x}')
        return text

    def branch_target(self, i):
        if i.group(capstone.CS_GRP_JUMP) or i.group(capstone.CS_GRP_CALL):
            op = i.operands[0]
            if op.type == capstone.x86.X86_OP_IMM:
                return op.imm & 0xffffffff
        return None

    def lines(self):
        out = []
        replaced = {}   # address -> 'branch' | 'ext' | 'imm'
        for kind, a in self.order:
            if kind == 'data':
                e = self.datab[a]
                b = self.bytes(a, e - a)
                out.append(f'    .byte {", ".join(f"0x{x:02x}" for x in b)}   # {a:#x}: {b!r}')
                continue
            i = self.ins[a]
            if a in self.entry:
                n, h, c = self.entry[a]
                out.append('')
                out.append(f'# {a:#x} {n}: {c}')
                out.append(f'    .globl {self.label(a)}')
                out.append(f'{self.label(a)}:')
            else:
                out.append(f'{self.label(a)}:')   # pruned later when no branch names it
            t = self.branch_target(i)
            if t is not None:
                if t in self.ins:
                    self.targets.add(t)
                    out.append(f'    {i.mnemonic} {self.label(t)}   # {a:#x}')
                    replaced[a] = 'branch'
                elif self.inside(t):
                    raise SystemExit(f'{a:#x}: branch into the middle of an instruction {t:#x}')
                else:
                    p = self.ext_ptr(t)
                    if i.mnemonic == 'call':
                        out.append(f'    call dword ptr [{p}]   # {a:#x} {self.ext_name(t)}')
                    elif i.mnemonic == 'jmp':
                        out.append(f'    jmp dword ptr [{p}]   # {a:#x} {self.ext_name(t)}')
                    else:   # a conditional jump out: the inverse condition around an indirect jump
                        inv = INVERSE[i.mnemonic]
                        out.append(f'    {inv} 1f   # {a:#x} {i.mnemonic} {self.ext_name(t)}')
                        out.append(f'    jmp dword ptr [{p}]')
                        out.append('1:')
                    replaced[a] = 'ext'
                continue
            text = self.operands(i)
            if text != i.op_str:
                replaced[a] = 'imm'
            mn = i.mnemonic
            if mn == 'push' and text.startswith('offset '):   # push imm32 of a label (the assembler would pick imm8)
                out.append(f'    .byte 0x68   # {a:#x} push {text}')
                out.append(f'    .long {text[7:]}')
                continue
            out.append(f'    {mn} {text}'.rstrip() + f'   # {a:#x}')
        # labels only where something branches to them
        pruned = []
        for ln in out:
            m = re.fullmatch(r'L([0-9a-f]+):', ln)
            if m and int(m.group(1), 16) not in self.targets:
                continue
            pruned.append(ln)
        self.replaced = replaced
        return pruned

    def check_hook_sites(self):
        """no branch from the ranges into bytes 1-4 of a hooked entry"""
        sites = [a for a, (n, h, c) in self.entry.items() if h]
        for kind, a in self.order:
            if kind != 'ins':
                continue
            t = self.branch_target(self.ins[a])
            if t is not None:
                for s in sites:
                    if s < t < s + 5:
                        raise SystemExit(f'{a:#x} branches to {t:#x}, inside the hook jump of {s:#x}')

    def hook_length(self, a):
        """whole instructions covering 5 bytes, without relative branches (one relative call allowed)"""
        n, call_at = 0, 0
        x = a
        while n < 5:
            i = self.ins.get(x) or next(self.md.disasm(self.bytes(x, 16), x, 1))
            t = self.branch_target(i)
            if t is not None:
                if i.mnemonic == 'call' and i.bytes[0] == 0xe8 and not call_at:
                    call_at = n + 1
                else:
                    return 0, 0
            n += i.size
            x += i.size
        return n, call_at


INVERSE = {'je': 'jne', 'jne': 'je', 'ja': 'jbe', 'jbe': 'ja', 'jae': 'jb', 'jb': 'jae', 'jg': 'jle', 'jle': 'jg',
           'jge': 'jl', 'jl': 'jge', 'js': 'jns', 'jns': 'js', 'jp': 'jnp', 'jnp': 'jp', 'jo': 'jno', 'jno': 'jo'}


_KINDS = None


def layer_line(ln):
    """Since batch 6-4: the data addresses of an instruction line (the exe's .rdata, .data, .bss and the IAT; not
    code, not the comment) through the address layer: `)" ZAX_ASM_DATA(A) R"(` inside a memory operand and
    `)" ZAX_ASM_OFFSET(A) R"(` as an immediate (`ZAX_ASM_PUSH(A)` for a push of it), which the module's macros spell as the address itself in the DLL (the
    same text, so the same bytes) and as the symbol tools/extract_exe_data.py defines in the standalone exe (our own
    block's place once its home is `own`, else the image's address)."""
    global _KINDS
    if _KINDS is None:
        sys.path.insert(0, str(ROOT / 'tools'))
        import addr_rewrite
        _KINDS = addr_rewrite.Kinds()
    if ln.lstrip().startswith(('#', '.')) or ':' in ln.split('#')[0] and not ln.startswith('    '):
        return ln
    code, sep, comment = ln.partition('#')
    m = re.fullmatch(r'(\s*)push (0x[0-9a-f]{6})(\s*)', code)
    if m and _KINDS.kind(int(m.group(2), 16)) in ('data', 'vtable', 'iat'):
        # push imm32 (the assembler would take an unknown symbol for an imm8)
        return '%s)" ZAX_ASM_PUSH(%s) R"(%s' % (m.group(1), m.group(2), m.group(3)) + sep + comment
    if m and _KINDS.kind(int(m.group(2), 16)) == 'code':
        # since batch 6-10c: a push of a reimplemented function (the SEH frames' _except_handler3), the function table's
        # entry in the exe (reimpl.h)
        return '%s)" ZAX_ASM_PUSH_CODE(%s) R"(%s' % (m.group(1), m.group(2), m.group(3)) + sep + comment

    def rep(m):
        a = int(m.group(0), 16)
        if _KINDS.kind(a) not in ('data', 'vtable', 'iat'):
            return m.group(0)
        mem = code.rfind('[', 0, m.start()) > code.rfind(']', 0, m.start())
        return ')" %s(0x%x) R"(' % ('ZAX_ASM_DATA' if mem else 'ZAX_ASM_OFFSET', a)
    return re.sub(r'\b0x[0-9a-f]{6}\b', rep, code) + sep + comment


def emit(gen, body):
    mod = gen.mod
    o = []
    o.extend('// ' + ln for ln in textwrap.wrap(mod.title + '.', 110))
    o.append('//')
    o.append('// Generated by tools/crt_asm_gen.py from the exe\'s code (docs/DECOMPILATION.md 11, "C runtime, phase 5b"):')
    o.append('// the same instructions as the original (assembly in VC6\'s runtime sources too; the results depend on')
    o.append('// the exact x87 sequence), branches between them as labels, calls and jumps out of them through')
    o.append('// pointers to the exe\'s entry points (hooked where reimplemented), copied descriptors with the')
    o.append('// handlers replaced by the labels. Data stays in the exe. The generator assembles the module and')
    o.append('// checks every unchanged instruction against the original bytes. Each instruction carries the')
    o.append('// address of its original.')
    o.append('#include "reimpl.h"')
    o.append('')
    o.append('// The data the instructions address, through the address layer (docs/DECOMPILATION.md 13, batch 6-4): the')
    o.append('// address itself in the DLL (the same text as before), in the standalone exe the symbol')
    o.append('// tools/extract_exe_data.py defines for it (our own block\'s place once its home is `own`, else the image\'s)')
    o.append('#ifdef ZAX_STANDALONE')
    o.append('#define ZAX_ASM_DATA(a) "_zax_data_" #a')
    o.append('#define ZAX_ASM_OFFSET(a) "offset _zax_data_" #a')
    o.append('#define ZAX_ASM_PUSH(a) ".byte 0x68\\n    .long _zax_data_" #a')
    o.append('#else')
    o.append('#define ZAX_ASM_DATA(a) #a')
    o.append('#define ZAX_ASM_OFFSET(a) #a')
    o.append('#define ZAX_ASM_PUSH(a) "push " #a')
    o.append('#endif')
    o.append('')
    o.append('extern "C" {')
    for a, n, h, c in mod.entries:
        if h:
            o.append(f'void {gen.label(a)[1:]}();')
    o.append('}')
    o.append('')
    o.append('__asm__(R"(')
    o.append('    .intel_syntax noprefix')
    o.append('    .text')
    o.extend(layer_line(ln) for ln in body)
    o.append('')
    if mod.tables:
        o.append('    .data')
        for a, size, c in mod.tables:
            o.append(f'# {a:#x}: {c}')
            o.append(f'{gen.table_label(a)}:')
            for k in range(0, size, 4):
                v = int.from_bytes(gen.bytes(a + k, 4), 'little')
                if gen.inside(v):
                    if v not in gen.ins:
                        raise SystemExit(f'table {a:#x}+{k}: {v:#x} no instruction')
                    gen.targets.add(v)
                    o.append(f'    .long {gen.label(v)}')
                else:
                    o.append(f'    .long 0x{v:08x}')
    o.append(')" ZAX_ASM_CODE_VALUES R"(    .p2align 2')   # converted to the function table's entries in the exe (reimpl.h)
    for a, p in sorted(gen.ext.items(), key=lambda x: x[0]):
        o.append(f'{p}: .long 0x{a:x}   # {gen.ext_name(a)}')
    o.append('    .att_syntax')
    o.append(')");')
    o.append('')
    o.append('namespace zax {')
    o.append('')
    o.append(f'ZAX_HOOKS({mod.hooks_name})')
    o.append('{')
    o.append('    const Hook list[] = {')
    for a, n, h, c in mod.entries:
        if not h:
            continue
        ln, call_at = gen.hook_length(a)
        b = gen.bytes(a, 8)
        exp = ', '.join(f'0x{x:02x}' for x in b)
        extra = f', {call_at}' if call_at else ''
        o.append(f'        {{0x{a:x}, (void *)&{gen.label(a)[1:]}, "{n}", {ln}, {{{exp}}}{extra}}},')
    o.append('    };')
    o.append('    return emit_hooks(out, list);')
    o.append('}')
    o.append('')
    o.append('}  // namespace zax')
    return '\n'.join(o) + '\n'


def same_decoding(md, a, i, mine):
    """the same instruction in another encoding (mov ecx, eax as 8b c8 or 89 c1)"""
    j = next(md.disasm(mine, a, 1), None)
    return j is not None and j.size == i.size and (j.mnemonic, j.op_str) == (i.mnemonic, i.op_str)


def coff_text(obj):
    """the raw bytes of .text of a COFF object"""
    nsec = int.from_bytes(obj[2:4], 'little')
    opt = int.from_bytes(obj[16:18], 'little')
    for k in range(nsec):
        h = obj[20 + opt + 40 * k:20 + opt + 40 * (k + 1)]
        if h[:8].rstrip(b'\0') == b'.text':
            size, ptr = int.from_bytes(h[16:20], 'little'), int.from_bytes(h[20:24], 'little')
            return obj[ptr:ptr + size]
    raise SystemExit('no .text')


def verify(gen, body, clang):
    """assemble with a symbol before every instruction and compare the bytes"""
    chk = []
    for ln in body:
        m = re.search(r'#\s*(0x[0-9a-f]+)', ln)
        if m and not ln.lstrip().startswith('#') and ": b'" not in ln:
            a = int(m.group(1), 16)
            if not any(re.match(rf'\s*chk_{a:x}:', c) for c in chk[-3:]):
                chk.append(f'chk_{a:x}:')
        chk.append(ln)
    src = ['.intel_syntax noprefix', '.text'] + chk + ['.p2align 2'] + [f'{p}: .long 0x{a:x}' for a, p in gen.ext.items()]
    src += [f'{gen.table_label(a)}: .long 0' for a, s, c in gen.mod.tables]
    with tempfile.TemporaryDirectory() as tmp:
        s = Path(tmp) / 'x.s'
        o = Path(tmp) / 'x.o'
        s.write_text('\n'.join(src) + '\n')
        (ROOT / 'out').mkdir(exist_ok=True)
        (ROOT / 'out' / (Path(gen.mod.path).stem + '_check.s')).write_text('\n'.join(src) + '\n')
        subprocess.run([clang, '-target', 'i386-pc-windows-msvc', '-c', str(s), '-o', str(o)], check=True)
        code = coff_text(o.read_bytes())
        nm = subprocess.run([str(Path(clang).parent / 'llvm-nm'), str(o)], check=True, capture_output=True, text=True).stdout
    offs = {}
    for ln in nm.splitlines():
        p = ln.split()
        if len(p) == 3 and p[2].startswith('chk_'):
            offs[int(p[2][4:], 16)] = int(p[0], 16)
    n = bad = 0
    for kind, a in gen.order:
        if kind != 'ins':
            continue
        i = gen.ins[a]
        if a not in offs:
            raise SystemExit(f'{a:#x}: no check symbol')
        mine = code[offs[a]:offs[a] + i.size]
        r = gen.replaced.get(a)
        n += 1
        if r is None and mine != bytes(i.bytes) and not same_decoding(gen.md, a, i, mine):
            bad += 1
            print(f'  {a:#x} {i.mnemonic} {i.op_str}: {bytes(i.bytes).hex()} / {mine.hex()}')
        elif r == 'imm' and len(mine) and mine[:1] != bytes(i.bytes[:1]):
            bad += 1
            print(f'  {a:#x} (address replaced) {i.mnemonic}: {bytes(i.bytes).hex()} / {mine.hex()}')
    print(f'{gen.mod.path}: {n} instructions, {len(gen.replaced)} with a branch or address replaced, {bad} differ')
    return bad == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=None, help='default: the build 1308 exe of tools/game_paths.py')
    ap.add_argument('--clang', default=sorted(Path('/opt/homebrew/opt').glob('llvm*/bin/clang'))[-1].as_posix())
    args = ap.parse_args()
    args.exe = args.exe or game_paths.exe_arg()
    names = load_names()
    ok = True
    for mod in MODULES:
        gen = Gen(args.exe, mod, names)
        gen.check_hook_sites()
        body = gen.lines()
        text = emit(gen, body)
        ok &= verify(gen, body, args.clang)
        (ROOT / mod.path).write_text(text)
    copy_table(args.exe)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
