#!/usr/bin/env python3
"""Generate the hooks of the game code's short functions that only the padding behind them makes hookable.

The milestone audit (docs/DECOMPILATION.md 10, "The milestone audit") found that every function of the game
code that the earlier batches left as "not hookable" (shorter than 5 bytes, or a relative branch in its first
5 bytes: getters, `ret` stubs, jump thunks, the tile cache handle's three functions) has alignment padding
(`nop`/`int3`) behind it that no branch and no pointer reaches, so a 5-byte jump fits without a trampoline
(hook length 0, as batch C3 did for the toolkit's stubs). Their reimplementation is the same instructions
(assembly: the callers depend on the exact registers, e.g. the upper bytes of eax after `mov al, [ecx+n]`),
branches between them as labels, jumps and calls out through pointers to the exe's entry points (hooked
where reimplemented) - the machinery of tools/crt_asm_gen.py.

Selection: the rows of docs/functions.csv in the game code (below 0x5c3bf0) that docs/reimpl.csv does not
list or lists with this module's file; not the functions still open in a parallel batch (OPEN), not the
tail 0x488f18 (no function); with at least 5 bytes up to the next function start of which everything
after the function is padding, and no branch target, pointer or function start in bytes 1-4.

Writes reimpl/src/short_hooks.cpp (the module and its hook table) and reimpl/src/short_hooks_cases.inc (what
checks_short_hooks.cpp needs per function: the arena kind, the stand-in of the jump or call out, globals,
the FPU). The rows of docs/reimpl.csv are appended by hand after the VM run.

    .venv/bin/python -I tools/short_hooks_gen.py [--list]
"""
import argparse
import contextlib
import io
import bisect
import csv
import re
import struct
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_asm_gen as cg  # noqa: E402

ROOT = cg.ROOT
MODULE_PATH = 'reimpl/src/short_hooks.cpp'
CASES_PATH = 'reimpl/src/short_hooks_cases.inc'
GAME_END = 0x5c3bf0
# still open, in the parallel batch "phase5-leftovers" (2026-10-28)
OPEN = {0x50dc10, 0x4f8120, 0x517f20, 0x518080}
NOT_FUNCTION = {0x488f18}   # the tail of Entity_Draw 0x488d70, reached by a branch
PAD = (0x90, 0xcc)


def references(exe):
    """branch targets and immediates/displacements into .text from every function of docs/functions.csv, and
    the dwords of .rdata/.data that point into .text"""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    tlo, tsz = exe.secs['.text'][:2]
    refs = set()
    for r in csv.DictReader(open(ROOT / 'docs/functions.csv')):
        a, n = int(r['address'], 16), int(r['size'] or 0)
        if not tlo <= a < tlo + tsz:
            continue
        o = exe.off(a)
        for _, _, mn, op in md.disasm_lite(exe.data[o:o + n], a):
            for m in re.finditer(r'0x[0-9a-f]{6}\b', op):
                refs.add(int(m.group(), 16))
    for name in ('.rdata', '.data'):
        lo, size, raw, rsize = exe.secs[name]
        d = exe.data[raw:raw + min(size, rsize)]
        for i in range(0, len(d) - 3, 4):
            refs.add(struct.unpack_from('<I', d, i)[0])
    return refs


def select(exe):
    rows = list(csv.DictReader(open(ROOT / 'docs/functions.csv')))
    reimpl = {int(r['address'], 16): r['file'] for r in csv.DictReader(open(ROOT / 'docs/reimpl.csv'))}
    starts = sorted(int(r['address'], 16) for r in rows)
    refs = references(exe)
    out = []
    for r in rows:
        a, n = int(r['address'], 16), int(r['size'] or 0)
        if a >= GAME_END or a in OPEN or a in NOT_FUNCTION or reimpl.get(a, MODULE_PATH) != MODULE_PATH:
            continue
        nxt = starts[bisect.bisect_right(starts, a)]
        o = exe.off(a)
        pad = 0
        while a + n + pad < nxt and exe.data[o + n + pad] in PAD:
            pad += 1
        if n > 64 or n + pad < 5 or nxt < a + 5 or any(t in refs for t in range(a + 1, a + 5)):
            print(f'{a:#x} {r["name"]}: not hookable ({n} bytes, {pad} of padding)')
            continue
        out.append((a, n, r['name'], r['class']))
    return out


def describe(ins, name):
    """the function's meaning from its code (the comment of the generated entry)"""
    t = '; '.join(f'{i.mnemonic} {i.op_str}'.strip() for i in ins)
    pats = [
        (r'ret', 'void f() {} (an empty slot, initializer or atexit stub)'),
        (r'ret (0x\w+|\d+)', lambda m: f'stdcall/thiscall stub: returns and pops {int(m[1], 0)} bytes of arguments'),
        (r'mov eax, dword ptr \[ecx \+ (0x\w+|\d+)\]; ret', lambda m: f'return the dword at this+{m[1]}'),
        (r'mov al, byte ptr \[ecx \+ (0x\w+|\d+)\]; ret', lambda m: f'return the byte at this+{m[1]} in al (the rest of eax the caller\'s)'),
        (r'fld dword ptr \[ecx \+ (0x\w+|\d+)\]; ret', lambda m: f'return the float at this+{m[1]} (st0)'),
        (r'lea eax, \[ecx \+ (0x\w+|\d+)\]; ret', lambda m: f'return this+{m[1]} (the address of a member)'),
        (r'mov eax, ecx; ret', 'return this'),
        (r'xor eax, eax; ret', 'return 0'),
        (r'xor al, al; ret', 'return false in al (the rest of eax the caller\'s)'),
        (r'or eax, 0xffffffff; ret', 'return -1'),
        (r'inc dword ptr \[ecx \+ (0x\w+)\]; ret', lambda m: f'++ the dword at this+{m[1]}'),
        (r'dec dword ptr \[ecx \+ (0x\w+)\]; ret', lambda m: f'-- the dword at this+{m[1]}'),
        (r'jmp (0x\w+)', lambda m: f'a jump thunk to {m[1]}'),
        (r'(add ecx, \w+|mov ecx, dword ptr \[ecx \+ \w+\]|xor cl, cl|mov byte ptr \[ecx \+ \w+\], 0); jmp (0x\w+)',
         lambda m: f'{m[1]}, then a tail jump to {m[2]}'),
    ]
    for p, d in pats:
        m = re.fullmatch(p, t)
        if m:
            return d(m) if callable(d) else d
    return {
        0x423960: 'a null part clears the flag 0x62e9d1; else slot 43 of the part writes its target into 0x62e9d8 and the flag is set',
        0x4d2670: 'the block of the handle (if any) freed through its heap (+0x1c) by HandleHeap::Free',
        0x4d2760: 'counts the block\'s locks (+8), moves it to the front of its heap\'s list, returns its data (+0x20); 0 for no block',
        0x4d27a0: 'counts the block\'s locks down while above 0',
    }.get(ins[0].address, t)


def ret_pop(gen, a):
    """the bytes a function pops (its first ret)"""
    rows = {int(r['address'], 16): int(r['size'] or 0) for r in csv.DictReader(open(ROOT / 'docs/functions.csv'))}
    n = rows.get(a, 4000)
    for i in gen.md.disasm(gen.bytes(a, n), a):
        if i.mnemonic == 'ret':
            return int(i.op_str, 0) if i.op_str else 0
    raise SystemExit(f'{a:#x}: no ret')


def case_info(gen, a, n):
    """the check's needs: arena kind, the stand-in (target, the bytes it pops), globals, FPU, null this"""
    ins = [gen.ins[x] for k, x in gen.order if k == 'ins' and a <= x < a + n]
    text = [f'{i.mnemonic} {i.op_str}' for i in ins]
    kind = 'ANY'
    if any(re.search(r'\[e(ax|dx|si|di|bx) ', t) or re.search(r'\[e(ax|dx|si|di|bx)\]', t) for t in text):
        kind = 'PTR' if a == 0x4d2760 else 'PTR0'
    target, pop, pushes = 0, 0, 0
    for i in ins:
        if i.mnemonic == 'push':
            pushes += 1
        t = gen.branch_target(i)
        if t is not None and not a <= t < a + n:
            target, pop = t, ret_pop(gen, t)
        elif i.mnemonic == 'call' and '[eax +' in i.op_str:
            kind, pop = 'VCALL', 4 * pushes
            m = re.search(r'\[eax \+ (0x\w+)\]', i.op_str)
            target = int(m.group(1), 16)   # the slot offset
    globs = []
    for i in ins:
        for op in i.operands:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                globs.append((op.mem.disp & 0xffffffff, op.size))
    fpu = any(i.mnemonic.startswith('f') for i in ins)
    null_this = text[0] == 'test ecx, ecx'   # only where the function tests this before using it
    return kind, target, pop, sorted(set(globs)), fpu, null_this


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=str(ROOT / 'gamedata/Zax - German/Zax.exe'))
    ap.add_argument('--clang', default=sorted(Path('/opt/homebrew/opt').glob('llvm*/bin/clang'))[-1].as_posix())
    ap.add_argument('--list', action='store_true')
    args = ap.parse_args()
    from exe_classes import Exe
    exe = Exe(Path(args.exe))
    funcs = select(exe)
    names = cg.load_names()
    mod = cg.Module(MODULE_PATH, 'short_hooks', '', [(a, a + n) for a, n, _, _ in funcs],
                    [(a, (c + '::' if c else '') + nm, True, '') for a, n, nm, c in funcs])
    gen = cg.Gen(args.exe, mod, names)
    gen.label = lambda x: (f'_zax_short_{x:x}' if x in gen.entry else f'L{x:x}')
    entries = []
    for a, n, nm, c in funcs:
        ins = [gen.ins[x] for k, x in gen.order if k == 'ins' and a <= x < a + n]
        d = describe(ins, nm)
        entries.append((a, (c + '::' if c else '') + nm, True, d))
        if args.list:
            print(f'{a:#x} {n:3} {nm}: {d}')
    mod.entries = entries
    gen.entry = {a: (n, h, c) for a, n, h, c in entries}
    # one function at a time, so a jump to another of them goes through the exe's entry (its hook) as in the
    # original, not straight to its label
    body, ok, n_ins = [], True, 0
    for a, n, nm, c in funcs:
        g1 = cg.Gen(args.exe, cg.Module(MODULE_PATH, 'short_hooks', '', [(a, a + n)], [e for e in entries if e[0] == a]), names)
        g1.label = (lambda g: lambda x: f'_zax_short_{x:x}' if x in g.entry else f'L{x:x}')(g1)
        g1.check_hook_sites()
        b1 = g1.lines()
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            good = cg.verify(g1, b1, args.clang)
        if not good:
            print(out.getvalue(), end='')
        ok &= good
        n_ins += sum(1 for k, x in g1.order if k == 'ins')
        body += b1
        gen.ext.update(g1.ext)
    print(f'{MODULE_PATH}: {n_ins} instructions assembled, {"all" if ok else "NOT all"} with the original\'s bytes')
    (ROOT / MODULE_PATH).write_text(emit(gen, body, funcs))
    write_cases(gen, funcs)
    print(f'{MODULE_PATH}: {len(funcs)} functions')
    sys.exit(0 if ok else 1)


def emit(gen, body, funcs):
    o = [
        '// The short functions of the game code that only the padding behind them makes hookable: getters, `ret` stubs,',
        '// jump thunks and the tile cache handle\'s three functions, which the batches of phases 1-5 left as "not',
        '// hookable" (shorter than 5 bytes or a relative branch in their first 5 bytes). Behind each lies alignment',
        '// padding that no branch or pointer reaches, so the hook\'s 5-byte jump fits without a trampoline (length 0).',
        '//',
        '// Generated by tools/short_hooks_gen.py (docs/DECOMPILATION.md 10, "The milestone audit") from the exe\'s code',
        '// with the machinery of tools/crt_asm_gen.py: the same instructions as the original (the callers depend on',
        '// the exact registers: the upper bytes of eax after `mov al, [ecx + n]`, st0, ecx after a thunk), branches',
        '// between them as labels, jumps and calls out through pointers to the exe\'s entry points (hooked where',
        '// reimplemented). Each entry says what the function does; each instruction carries its original\'s address.',
        '// Checked against the original by checks_short_hooks.cpp.',
        '#include "reimpl.h"',
        '',
        '// The data the instructions address, as in the generated C runtime modules (tools/crt_asm_gen.py layer_line; since',
        '// batch 6-10c): the address in the DLL, the symbol tools/extract_exe_data.py defines in the standalone exe',
        '#ifdef ZAX_STANDALONE',
        '#define ZAX_ASM_DATA(a) "_zax_data_" #a',
        '#define ZAX_ASM_OFFSET(a) "offset _zax_data_" #a',
        '#define ZAX_ASM_PUSH(a) ".byte 0x68\\n    .long _zax_data_" #a',
        '#else',
        '#define ZAX_ASM_DATA(a) #a',
        '#define ZAX_ASM_OFFSET(a) #a',
        '#define ZAX_ASM_PUSH(a) "push " #a',
        '#endif',
        '',
        'extern "C" {',
    ]
    for a, *_ in funcs:
        o.append(f'void zax_short_{a:x}();')
    o += ['}', '', '__asm__(R"(', '    .intel_syntax noprefix', '    .text']
    o += [cg.layer_line(ln) for ln in body]
    o += ['', ')" ZAX_ASM_CODE_VALUES R"(    .p2align 2']   # converted to the function table's entries in the exe (reimpl.h)
    for a, p in sorted(gen.ext.items()):
        o.append(f'{p}: .long 0x{a:x}   # {gen.ext_name(a)}')
    o += ['    .att_syntax', ')");', '', 'namespace zax {', '', 'ZAX_HOOKS(short_hooks)', '{',
          '    // length 0: no trampoline (the padding takes the rest of the jump)', '    const Hook list[] = {']
    for a, n, h, c in gen.mod.entries:
        exp = ', '.join(f'0x{x:02x}' for x in gen.bytes(a, 8))
        o.append(f'        {{0x{a:x}, (void *)&zax_short_{a:x}, "{n}", 0, {{{exp}}}}},')
    o += ['    };', '    return emit_hooks(out, list);', '}', '', '}  // namespace zax']
    return '\n'.join(o) + '\n'


def write_cases(gen, funcs):
    o = ['// Generated by tools/short_hooks_gen.py (the entry points are short_hooks.cpp\'s)']
    o += [f'extern "C" void zax_short_{a:x}();' for a, *_ in funcs]
    o += ['', '// Per function of short_hooks.cpp the address, the arena kind (ANY:',
         '// random dwords; PTR0: pointers into the arena or 0; PTR: pointers only, this->+0 sometimes 0; VCALL: a fake',
         '// vtable whose slot at `target` is the stand-in), the jump or call out (the stand-in takes its place; for',
         '// VCALL the slot offset), the bytes the stand-in pops, a global the function uses (address, size), whether',
         '// it uses the FPU and whether it tests this for null.',
         'const ShortCase SHORT_CASES[] = {']
    for a, n, nm, c in funcs:
        kind, target, pop, globs, fpu, null_this = case_info(gen, a, n)
        g, gs = globs[0] if globs else (0, 0)
        if len(globs) > 1:
            raise SystemExit(f'{a:#x}: more than one global')
        o.append(f'    {{0x{a:x}, (void *)&zax_short_{a:x}, {kind}, 0x{target:x}, {pop}, 0x{g:x}, {gs}, {int(fpu)}, '
                 f'{int(null_this)}, "{(c + "::" if c else "") + nm}"}},')
    o.append('};')
    (ROOT / CASES_PATH).write_text('\n'.join(o) + '\n')


if __name__ == '__main__':
    main()
