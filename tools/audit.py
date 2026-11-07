#!/usr/bin/env python3
"""The milestone audit (docs/DECOMPILATION.md 10, "The milestone audit"): every function of Zax.exe that is not
reimplemented, with its reason, and the code of .text that no table covers.

1. Every row of docs/functions.csv that docs/reimpl.csv does not list gets a reason (REASONS below; the
   libraries' and the copy protection's by their subsystem). An unclassified row is an error.
2. The gaps: a recursive descent from every function start of both tables (branches, jump tables through
   `jmp [reg*4 + table]`, the byte index tables they read up to the switch's bound, and the filters and
   handlers of the function's own __try scope table) marks the reached code; the declared sizes, the
   generated assembler modules' ranges (tools/crt_asm_gen.py) and alignment filler (nop, int3,
   `mov edi, edi`, `lea ecx, [ecx]`, `lea esp, [esp]`) count as covered too. What is left is listed with a
   reason from GAPS; an unknown gap is an error.
3. Real coverage only (since 2026-10-28, phase 6 batch 6-0): a .rdata/.data pointer into .text is no start of
   the descent. Its target must be a function of the tables, already covered, an SEH filter or handler of a
   table function, or a row of DATA_TARGETS; a call of code that no table has as a function is an error too.
   Before, the descent started from every data pointer and so counted three game functions that no table
   had (0x551550, 0x480310, 0x55da60) as covered, and a byte index table ran on to the next covered byte,
   over the function behind it.

Writes docs/not_reimplemented.csv (address, size, reason, fnv1a of the bytes, the bytes in hex up to 64,
subsystem, name, source "functions.csv" or "gap", note). reimpl/build.sh turns its rows that stay the exe's
code (identical by bytes, covered by caller, no function, dead code) into the byte table of
checks_not_hookable.cpp.

    .venv/bin/python -I tools/audit.py [--summary]
"""
import argparse
import collections
import csv
import re
import struct
import sys
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from exe_classes import Exe  # noqa: E402
import crt_asm_gen  # noqa: E402

OUT = ROOT / 'docs/not_reimplemented.csv'
GAME_END = 0x5c3bf0

# reason, note per address (rows of docs/functions.csv)
REASONS = {
    0x488f18: ('no function', 'the tail of Entity_Draw 0x488d70, reached by a branch from it; reimplemented with it (section 9)'),
    0x5d0ad0: ('covered by caller', 'shortsort: only qsort 0x5d097c calls it, reimplemented with its own sort of short ranges'),
    0x5d0b1e: ('covered by caller', 'swap: only qsort and shortsort call it'),
    0x5d10c7: ('covered by caller', 'strtoxl: only strtol 0x5d10b0 calls it, reimplemented whole'),
    0x5dc595: ('identical by bytes', '_matherr (xor eax, eax; ret): 3 bytes and the next function right behind it, no room for a jump'),
    0x5dc2a0: ('dead code', 'strcspn: no call, jump or pointer reaches it (section 11, "Which runtime functions the game reaches")'),
    0x5dc2e0: ('dead code', 'strspn: no call, jump or pointer reaches it'),
}
FUNCLET = ('covered by caller', 'a __finally funclet _unlock(9) of {}: the reimplemented function unlocks directly, so the exe\'s SEH frame and its funclet never run')
FUNCLETS = {0x5d16f4: '_heap_alloc', 0x5d1753: '_heap_alloc', 0x5d344a: 'free', 0x5d34a2: 'free', 0x5d3562: 'calloc',
            0x5d35eb: 'calloc', 0x5d38d0: 'realloc', 0x5d3a1e: 'realloc', 0x5d4258: '_msize', 0x5d42d3: '_msize'}
SBH = ('dead code', 'the Windows 9x small-block heap of VC6 (sbheap.c): __heap_select 0x5d6936 picks the system heap on Windows NT 5 and later, so it never runs on the target systems')

# gaps of .text that no table covers: (start, end, reason, note)
GAPS = [
    (0x5caa58, 0x5caa90, 'identical by bytes', 'the linker\'s import thunks of msacm32, ddraw and dsound the game code calls (jmp [IAT], 6 bytes each; the reimplementation calls them as the original does)'),
    (0x5d0748, 0x5d0787, 'no function', 'the end of memmove 0x5d04b0 (its unrolled tail behind the jump tables, past the size the table gives); reimplemented with it'),
    (0x5d84c8, 0x5d8507, 'no function', 'the end of memcpy 0x5d8230 (the same tail as memmove\'s); reimplemented with it'),
    (0x5d9650, 0x5d9857, 'dead code', 'exp of the x87 library (tran/i386, not in the tables): its code and its dispatch table 0x62c27b ("exp") refer only to each other; no call, jump or pointer from elsewhere reaches it'),
    (0x5d9c60, 0x5da466, 'dead code', 'the Pentium FDIV workaround of the runtime (_adj_fdiv_*; its tables 0x62c2a0-0x62c400): reached only from itself and from exp above'),
    (0x5da71e, 0x5da9eb, 'dead code', 'the rest of the Pentium workaround behind _adj_fprem (crt_x87\'s last range): reached only from itself'),
    (0x5e9c70, 0x5ea000, 'no function', 'zero fill at the end of .text'),
]
# targets of data pointers into .text that are no function of the tables: (kind, size, note). "not code" are
# dwords of strings that happen to look like addresses and land in alignment padding (such dwords that
# land inside a function's declared size need no row); the others become rows (source "data pointer")
DATA_TARGETS = {
    0x41786f: ('not code', 0, 'the text "oxA\\0" of "MessageBoxA" (0x6076a8, 0x6081f8), not a pointer'),
    0x494179: ('not code', 0, 'the text "yAI\\0" of "CWalkingUpperBodyAI" (0x6245e0), not a pointer'),
    0x5d044c: ('dead code', 1, 'a single ret behind _fpmath, named _fpclear after VC6\'s fpinit.c [assumption]: the slots _FPmtinit and '
                              '_FPmtterm 0x629628/0x62962c behind _FPinit 0x629624 point to it, and no byte of Zax.exe holds either '
                              'slot\'s address (only _cinit reads _FPinit)'),
}
DATA_NAMES = {0x5d044c: '_fpclear'}
FILL = [bytes.fromhex(h) for h in ('8da42400000000', '8d642400', '8d4900', '8bff', '90', 'cc')]


def fnv(b):
    h = 2166136261
    for x in b:
        h = ((h ^ x) * 16777619) & 0xffffffff
    return h


def reason_of(r):
    a = int(r['address'], 16)
    sub = r['subsystem']
    if sub == 'Copy protection':
        return 'dropped (SecuROM)', 'the SecuROM loader of the protected edition: dead in this exe, left out by decision (section 11, "Copy protection")'
    if sub == 'Library: GameSpy SDK':
        return 'GameSpy batch', 'reimplemented in the parallel batch phase5b-gamespy (2026-10-28)'
    if a in REASONS:
        return REASONS[a]
    if a in FUNCLETS:
        return FUNCLET[0], FUNCLET[1].format(FUNCLETS[a])
    if r['name'].startswith(('__sbh_', '__old_sbh_')):
        return SBH
    raise SystemExit(f'{r["address"]} {r["name"]}: no reason')


EXCEPT_HANDLER3 = 0x5d2304   # _except_handler3, the frame handler of every __try in the exe


def flow_cover(exe, starts, cov):
    """recursive descent from every start; marks reached code and jump/index tables in cov.

    Returns (visited, seh, calls): the instruction starts reached; per start with a __try frame
    (push -1; push scope table; push _except_handler3) its filters and handlers {target: start}, read from
    the scope table up to the highest try level the start's code sets; the call targets that are no start
    {target: caller}. The descent starts only from the tables' functions and their own SEH targets: a data
    pointer into .text does not make its target covered (data_targets below decides about those)."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    tlo, tsz = exe.secs['.text'][:2]
    thi = tlo + tsz
    base = exe.off(tlo)
    tables = []
    visited = set()   # instruction starts reached
    seh = {}
    calls = {}

    def flow(s):
        if s in visited:
            return None, -1
        work, seen = [s], visited
        prologue = []        # the first instructions of s, for the __try frame
        scope, level = None, -1
        bound = None         # the last `cmp reg, imm`: the highest index of a switch's byte table
        btab = None          # the byte index table read since, waiting for its jump table
        while work:
            a = work.pop()
            while tlo <= a < thi and a not in seen and not (a != s and a in starts):
                seen.add(a)
                i = next(md.disasm_lite(exe.data[a - tlo + base:a - tlo + base + 16], a), None)
                if not i:
                    break
                _, size, mn, op = i
                cov[a - tlo:a - tlo + size] = b'\1' * size
                if len(prologue) < 6:
                    prologue.append((mn, op))
                    if (len(prologue) >= 3 and prologue[-3] == ('push', '-1') and prologue[-2][0] == 'push'
                            and prologue[-1] == ('push', f'{EXCEPT_HANDLER3:#x}') and prologue[-2][1].startswith('0x')):
                        scope = int(prologue[-2][1], 16)
                m = re.match(r'dword ptr \[ebp - 4\], (0x[0-9a-f]+|\d+)$', op) if mn == 'mov' else None
                if m and int(m.group(1), 0) != 0xffffffff:
                    level = max(level, int(m.group(1), 0))
                if mn in ('ret', 'retf', 'int3', 'hlt', 'ud2'):
                    break
                m = re.match(r'e\w\w, (0x[0-9a-f]+|\d+)$', op) if mn == 'cmp' else None
                if m:
                    bound = int(m.group(1), 0)
                m = re.search(r'byte ptr \[e\w\w \+ (0x[0-9a-f]{6})\]', op)
                if m:
                    btab = (int(m.group(1), 16), bound)
                if mn == 'call' and op.startswith('0x'):
                    t = int(op, 16)
                    if tlo <= t < thi and t not in starts:
                        calls.setdefault(t, a)
                if mn == 'jmp':
                    if op.startswith('0x'):
                        t = int(op, 16)
                        if t in starts:
                            break
                        a = t
                        continue
                    m = re.match(r'dword ptr \[e\w\w\*4 \+ (0x[0-9a-f]+)\]', op)
                    if m:
                        q = int(m.group(1), 16)
                        k = 0
                        while tlo <= q < thi:
                            v = struct.unpack_from('<I', exe.data, q - tlo + base)[0]
                            if not tlo <= v < thi:
                                break
                            cov[q - tlo:q - tlo + 4] = b'\1' * 4
                            work.append(v)
                            q += 4
                            k += 1
                        if btab:
                            tables.append((btab[0], btab[1], k))
                            btab = None
                    break
                if (mn.startswith('j') or mn.startswith('loop')) and op.startswith('0x'):
                    work.append(int(op, 16))
                a += size
        return scope, level

    pending = []
    for s in sorted(starts):
        scope, level = flow(s)
        if scope is not None:
            pending.append((s, scope, level))
    # the filters and handlers of the __try frames belong to their function (the compiler puts them behind
    # its body, where a table's declared size may already have ended)
    while pending:
        s, scope, level = pending.pop()
        for k in range(level + 1):
            _, flt, hnd = struct.unpack_from('<III', exe.data, exe.off(scope + 12 * k))
            for t in (flt, hnd):
                if tlo <= t < thi and t not in starts:
                    seh.setdefault(t, s)
                    flow(t)
    # a byte index table: as many bytes as the switch's bound allows (`cmp reg, n` before it: n + 1), each an
    # index into its jump table; before 2026-10-28 it ran on to the next covered byte and so covered the
    # function behind it (0x480310 behind DPlay_MessageHandler's table 0x480300, 0x55da60 behind 0x55d9f0's)
    for t, bound, k in tables:
        n = bound + 1 if bound is not None and bound < 0x400 else 0x400
        q = t
        while tlo <= q < min(thi, t + n) and not cov[q - tlo] and exe.data[q - tlo + base] < k:
            cov[q - tlo] = 1
            q += 1
    return visited, seh, calls


def data_targets(exe, starts, visited, seh, cov):
    """every aligned dword of .rdata/.data that points into .text: the target must be a function of the
    tables, inside a table function's declared size or code the descent reached (many of these dwords are
    text that looks like an address), an SEH filter or handler, inside a generated assembler module, or a
    row of DATA_TARGETS. Anything else is a function no table has, which an error reports (before 2026-10-28 the
    descent started from every such pointer and so counted those functions as covered)."""
    tlo, tsz = exe.secs['.text'][:2]
    thi = tlo + tsz
    asm = [r for mod in crt_asm_gen.MODULES for r in mod.ranges]
    open_ = collections.defaultdict(list)
    for name in ('.rdata', '.data'):
        lo, size, raw, rsize = exe.secs[name]
        d = exe.data[raw:raw + min(size, rsize)]
        for k in range(0, len(d) - 3, 4):
            v = struct.unpack_from('<I', d, k)[0]
            if not tlo <= v < thi or v in starts or v in seh or cov[v - tlo]:
                continue   # (covered: inside a function's declared size or code the descent reached)
            if any(s <= v < e for s, e in asm) or v in DATA_TARGETS:
                continue
            open_[v].append(lo + k)
    if open_:
        raise SystemExit('data pointers into .text whose target is in no table (a missed function?):\n' + '\n'.join(
            f'  {v:#x} from {", ".join(f"{x:#x}" for x in refs)}: {exe.data[exe.off(v):exe.off(v) + 16].hex()}'
            for v, refs in sorted(open_.items())))
    rows = []
    for v, (kind, size, note) in sorted(DATA_TARGETS.items()):
        if kind != 'not code':
            cov[v - tlo:v - tlo + size] = b'\1' * size
            rows.append((v, size, kind, note))
    return rows


def gaps(exe, funcs, reimpl):
    tlo, tsz = exe.secs['.text'][:2]
    cov = bytearray(tsz)
    starts = set()
    for a, n in list(funcs.items()) + [(a, 0) for a in reimpl]:
        if tlo <= a < tlo + tsz:
            starts.add(a)
            cov[a - tlo:a - tlo + n] = b'\1' * n
    for mod in crt_asm_gen.MODULES:
        for s, e in mod.ranges:
            cov[s - tlo:e - tlo] = b'\1' * (e - s)
    visited, seh, calls = flow_cover(exe, starts, cov)
    thunks = [g for g in GAPS if 'import thunks' in g[3]]
    asm = [r for mod in crt_asm_gen.MODULES for r in mod.ranges]
    # (terminate's `call $+5` stays inside its generated module)
    bad = {t: c for t, c in calls.items() if not any(s <= t < e for s, e, *_ in thunks + asm)}
    if bad:
        raise SystemExit('calls of code no table has as a function:\n' + '\n'.join(f'  {t:#x} from {c:#x}' for t, c in sorted(bad.items())))
    extra = data_targets(exe, starts, visited, seh, cov)
    past = sorted((s, t) for t, s in seh.items() if s in funcs and not s <= t < s + funcs[s])
    base = exe.off(tlo)
    out, i = [], 0
    while i < tsz:
        if cov[i]:
            i += 1
            continue
        b = exe.data[base + i:base + i + 8]
        f = next((f for f in FILL if b.startswith(f)), None)
        if f:
            i += len(f)
            continue
        j = i
        while j < tsz and not cov[j]:
            j += 1
        out.append((tlo + i, tlo + j))
        i = j
    rows = []
    for s, e in out:
        g = next((g for g in GAPS if g[0] <= s and e <= g[1]), None)
        if not g:
            raise SystemExit(f'gap {s:#x}-{e:#x} ({e - s} bytes): no reason; {exe.data[exe.off(s):exe.off(s) + 16].hex()}')
    for gs, ge, reason, note in GAPS:
        parts = [(s, e) for s, e in out if gs <= s and e <= ge]
        if not parts:
            raise SystemExit(f'GAPS entry {gs:#x}-{ge:#x} matches no gap')
        s, e = parts[0][0], parts[-1][1]
        rows.append((s, e - s, reason, note))
    return rows, extra, past, len(seh)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=str(ROOT / 'gamedata/Zax - German/Zax.exe'))
    ap.add_argument('--summary', action='store_true')
    args = ap.parse_args()
    exe = Exe(Path(args.exe))
    rows = list(csv.DictReader(open(ROOT / 'docs/functions.csv')))
    reimpl = {int(r['address'], 16): r for r in csv.DictReader(open(ROOT / 'docs/reimpl.csv'))}
    funcs = {int(r['address'], 16): int(r['size'] or 0) for r in rows}
    out = []
    for r in rows:
        a = int(r['address'], 16)
        if a in reimpl:
            continue
        reason, note = reason_of(r)
        n = int(r['size'] or 0)
        b = exe.data[exe.off(a):exe.off(a) + n]
        out.append((a, n, reason, fnv(b), b.hex() if n <= 64 else '', r['subsystem'], r['name'], 'functions.csv', note))
    gap_rows, extra, past, nseh = gaps(exe, funcs, reimpl)
    for s, n, reason, note in gap_rows:
        b = exe.data[exe.off(s):exe.off(s) + n]
        out.append((s, n, reason, fnv(b), b.hex() if n <= 64 else '', '', '', 'gap', note))
    for s, n, reason, note in extra:
        b = exe.data[exe.off(s):exe.off(s) + n]
        out.append((s, n, reason, fnv(b), b.hex() if n <= 64 else '', '', DATA_NAMES.get(s, ''), 'data pointer', note))
    out.sort()
    with open(OUT, 'w', newline='') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(['address', 'size', 'reason', 'fnv1a', 'bytes', 'subsystem', 'name', 'source', 'note'])
        for a, n, reason, h, hx, sub, name, src, note in out:
            w.writerow([f'0x{a:x}', n, reason, f'0x{h:08x}', hx, sub, name, src, note])
    c = collections.Counter((o[7], o[2]) for o in out)
    total = len(rows)
    done = sum(1 for a in funcs if a in reimpl)
    print(f'{OUT.relative_to(ROOT)}: {len(out)} rows')
    print(f'docs/functions.csv: {total} functions, {done} reimplemented, {total - done} not')
    # the game code: the game's own functions of docs/functions.csv (no library, no copy protection) and the
    # functions of docs/reimpl.csv below GAME_END that docs/functions.csv does not have yet
    game = {int(r['address'], 16) for r in rows if int(r['address'], 16) < GAME_END
            and not r['subsystem'].startswith('Library') and r['subsystem'] != 'Copy protection'}
    only = {a for a in reimpl if a < GAME_END and a not in funcs}
    game |= only
    print(f'game code: {len(game)} functions ({len(only)} only in docs/reimpl.csv), {sum(1 for a in game if a in reimpl)} reimplemented')
    print(f'SEH filters and handlers: {nseh}; behind their function\'s size in docs/functions.csv: '
          + ', '.join(f'{t:#x} of {s:#x}' for s, t in past))
    for (src, reason), k in sorted(c.items()):
        print(f'  {src:13} {reason:26} {k}')


if __name__ == '__main__':
    main()
