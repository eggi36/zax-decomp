"""Find reimplementations that return less than the original leaves in eax for a caller that reads it.

A function the reimplementation declares `void` (or `float`/`double`, which come back in st(0), or a 1- or
2-byte type) leaves `eax` with whatever its code last put there. That is harmless as long as no caller of the
original reads `eax` (or the part of it beyond the declared type) after the call: MSVC 6 never reads `eax`
after calling a function its source declared `void`, so a read in the original's code means the original
source had a return value (6-12's `CEntityCharacter::TurnDischarge`, docs/DECOMPILATION.md 13).

What the scan does (docs/DECOMPILATION.md 13, "Batch 6-13"):

1. The hooked functions and their declared return types, from the compiler: every module's hook table
   (`ZAX_HOOKS`) is dumped by clang (`-ast-dump=json`, filtered to `hooks_<module>`) with the build's flags;
   each `Hook` entry gives the original's address and the replacement's declared function type.
2. The original's callers, from Zax.exe (read at run time from gamedata/, never stored): every function of
   docs/functions.csv is disassembled over its size;
   - direct callers: `call rel32` to the address;
   - vtable callers: the address as a dword in a vtable of docs/exe_layout.csv gives a slot offset; every
     `call [reg+offset]` (any register, the receiver's class is not resolved) is a possible caller;
   - other dwords (function pointer tables, data): listed by address for a manual look.
3. For each call site: does the code after the call read `eax` (or `ax`/`ah` beyond a 1-byte type, the upper
   half beyond a 2-byte one) before something writes it again? Followed through jumps and both ways of a
   conditional branch, up to a bound; a `call` kills `eax` (code after a call of exit()/_exit()/_amsg_exit()/abort()
   is another path); `xor al, al`, `or eax, -1` and the like write without reading; `mov al, x; push eax` pushes a
   byte argument (not a use of the upper part). A caller that returns with the value still in `eax` passes it on:
   its own direct callers are followed (four levels).
4. The original's own code: a write of `eax` (not a call's result, not one made for its flags before a branch) that
   reaches a `ret` unread and whole is a return value (MSVC 6 /O2 drops dead stores). A write that is read on the
   way (`return p` after using p in `eax`) is not counted: such functions are found by 3 or 5.
   The vtable callers of 2 are not resolved to classes; a slot counts when a related vtable (one sharing an inherited
   entry at that slot or a later one, so the same base declares the slot) holds there a function whose original
   returns a value by 4.
5. The reimplementation's own typed calls of exe addresses (`fn<R (CC *)(...)>(0xADDR)`, `original<>`,
   `exe_or_dead<>`; clang's JSON AST of every source, with typedefs resolved): a call whose declared return type is
   wider than the hooked function's declaration, and how its value is used (discarded; returned by a function whose
   own value is used, only escapes to the exe's callers through a hook or a table, or is unused; used; only its
   low byte/word by a cast or mask; only its upper part, `& 0xffffff00` or `with_al`, the reimplementation's emulation
   of a leftover `eax`).
6. (batch 6-16b) The vtable-only readers of 3, per call site: the register holding the vtable is followed back over
   every path of the caller to `mov reg, [obj]`, and obj further. A receiver that is the caller's own `this` resolves
   to the vtables holding the caller (or those of the callers that pass their own `this`, or a static object, to it
   directly); a static object (a global whose constructors and destructors, called with ecx = its address, store
   constant vtables at +0) to those vtables. Vtables shorter than the slot are left out. A resolved site can reach
   only the entries at that offset; the scan counts the sites that cannot reach the function, can, or stay unresolved
   (by the receiver's origin: an argument, a field, a call's result, ...), and lists every resolved site that reaches a
   hooked function that provides less than the site reads.
7. (batch 6-16b) The reimplementation's `vcall<R>(obj, slot)` sites (clang's AST: R and the slot): where the value is
   used and the enclosing function is a hooked replacement, the original's calls at that vtable offset in the original
   function: whether they read eax (an x87 result: not followed), and at a resolved receiver the overrides' declared
   types against R; a site whose original call ignores the value, or whose override provides less, is listed with the
   original's callers of the enclosing function that read eax (al only, or beyond al).
8. (batch 6-16b) The typed calls of 5 that pass a void callee's leftover eax on ("used"): whether an original caller of
   the hooked function that ends with it reads eax (direct calls; vtable calls with a resolved receiver), al only or
   beyond al.
Replacements written in assembly (labels of the sources' asm blocks, `naked` functions) keep the original's registers
and are not scanned; the library replacements of zlib and Ogg Vorbis (C variables naming the original functions) are
compiled from the original sources and keep their types.

    .venv/bin/python -I tools/void_return_scan.py [--exe 'gamedata/Zax - German/Zax.exe'] [--jobs 8] [--verbose]
        [--assume-void 0x49b1d0]     (shows a known case: batch 6-12's TurnDischarge as it was)
        [--no-sources]               (parts 1-4 and 6 only, without the clang runs over the sources)

Prints the counts and the candidates; nothing is written into the repository.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import subprocess
import sys
import struct
import tempfile
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import capstone
from capstone import x86

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from exe_classes import Exe  # noqa: E402

CXXFLAGS = ('-target i386-pc-windows-msvc -march=pentiumpro -mno-sse -mlong-double-80 -std=c++20 -ffreestanding '
            '-fno-builtin -fno-exceptions -fno-rtti -fsyntax-only').split()

# ---------------------------------------------------------------------------------------------------------
# 1. The hook tables' declared return types


def find_clang() -> str:
    c = os.environ.get('CLANG')
    if c:
        return c
    cands = sorted(Path('/opt/homebrew/opt').glob('llvm*/bin/clang'), key=lambda p: [int(x) if x.isdigit() else x for x in re.split(r'(\d+)', str(p))])
    if not cands:
        sys.exit('clang not found (brew install llvm or CLANG=...)')
    return str(cands[-1])


def _walk(node):
    stack = [node]
    while stack:
        n = stack.pop()
        if isinstance(n, dict):
            yield n
            stack.extend(reversed(n.get('inner', [])))


def _int_value(n) -> int | None:
    for m in _walk(n):
        if m.get('kind') == 'ConstantExpr' and 'value' in m:
            return int(m['value'])
        if m.get('kind') == 'IntegerLiteral':
            return int(m['value'])
    return None


def _replacement(n):
    for m in _walk(n):
        if m.get('kind') == 'DeclRefExpr':
            d = m.get('referencedDecl', {})
            if d.get('name') == 'ptr':
                continue
            return d.get('kind'), d.get('name'), d.get('type', {}).get('qualType', '')
    return None, None, ''


def module_hooks(clang: str, incdir: str, src: Path, module: str) -> list[dict]:
    cmd = [clang, *CXXFLAGS, f'-I{incdir}', '-Xclang', '-ast-dump=json', '-Xclang', f'-ast-dump-filter=hooks_{module}',
           str(src)]
    r = subprocess.run(cmd, cwd=ROOT / 'reimpl', capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(f'{src.name}: clang failed\n{r.stderr[-2000:]}')
    out = []
    dec = json.JSONDecoder()
    s, i = r.stdout, 0
    while True:
        while i < len(s) and s[i].isspace():
            i += 1
        if i >= len(s):
            break
        tree, i = dec.raw_decode(s, i)
        if tree.get('name') != f'hooks_{module}':
            continue
        for n in _walk(tree):
            if n.get('kind') != 'InitListExpr' or n.get('type', {}).get('qualType') not in ('Hook', 'const Hook'):
                continue
            inner = n.get('inner', [])
            if len(inner) < 2:
                continue
            addr = _int_value(inner[0])
            kind, name, qt = _replacement(inner[1])
            out.append({'address': addr, 'module': module, 'decl_kind': kind, 'replacement': name, 'type': qt,
                        'src': src.name})
    return out


def prepare_includes(tmp: str) -> None:
    """The generated includes reimpl/build.sh makes, for the clang runs (the Local.txt table empty)."""
    subprocess.run([sys.executable, '-I', str(ROOT / 'tools/exe_layout.py'), 'tables', tmp], check=True,
                   stdout=subprocess.DEVNULL)
    t = Path(tmp)
    with open(ROOT / 'docs/classes.csv', newline='') as f, open(t / 'class_vars.inc', 'w') as o:
        for row in csv.DictReader(f):
            if row['class_var']:
                o.write(row['class_var'] + ',\n')
    hooks, checks = [], []
    for src in sorted((ROOT / 'reimpl/src').glob('*.cpp')):
        text = src.read_text(errors='replace')
        hooks += re.findall(r'^ZAX_HOOKS\(([A-Za-z0-9_]*)\)', text, re.M)
        checks += re.findall(r'^ZAX_CHECKS\(([A-Za-z0-9_]*)\)', text, re.M)
    (t / 'hook_tables.inc').write_text(''.join(f'ZAX_HOOK_MODULE({m})\n' for m in hooks))
    (t / 'check_list.inc').write_text(''.join(f'ZAX_CHECK_MODULE({m})\n' for m in checks))
    with open(ROOT / 'docs/reimpl.csv', newline='') as f:
        (t / 'reimpl_addresses.inc').write_text(''.join(r['address'] + ',\n' for r in csv.DictReader(f)))
    (t / 'not_hookable_table.inc').write_text('')


def hook_table(jobs: int) -> list[dict]:
    clang = find_clang()
    with tempfile.TemporaryDirectory() as tmp:
        prepare_includes(tmp)
        work = []
        for src in sorted((ROOT / 'reimpl/src').glob('*.cpp')):
            for m in re.findall(r'^ZAX_HOOKS\(([A-Za-z0-9_]+)\)', src.read_text(errors='replace'), re.M):
                work.append((src, m))
        with ThreadPoolExecutor(jobs) as ex:
            res = list(ex.map(lambda w: module_hooks(clang, tmp, *w), work))
    return [h for r in res for h in r]


def asm_names() -> set[str]:
    """Replacements written in assembly: labels of the sources' asm blocks and `naked` functions. They keep the
    original's registers themselves; their C++ declaration (`void name();`) says nothing about eax."""
    names = set()
    for src in (ROOT / 'reimpl/src').glob('*.cpp'):
        text = src.read_text(errors='replace')
        for m in re.finditer(r'(?m)(?:^|"|\\n)\s*_?([A-Za-z_][A-Za-z0-9_@]*):(?!:)', text):
            names.add(m.group(1))
        for m in re.finditer(r'__attribute__\(\(naked\)\)[^(;{]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', text):
            names.add(m.group(1))
    for inc in (ROOT / 'reimpl/src').glob('*.inc'):
        text = inc.read_text(errors='replace')
        for m in re.finditer(r'(?m)(?:^|"|\\n)\s*_?([A-Za-z_][A-Za-z0-9_@]*):(?!:)', text):
            names.add(m.group(1))
    return names


def return_type(qt: str) -> str:
    """The return type part of a function type as clang prints it: 'float *(Character *) __attribute__((thiscall))'."""
    depth = 0
    for i, c in enumerate(qt):
        if c == '(':
            if depth == 0 and i > 0:
                return qt[:i].strip()
            depth += 1
        elif c == ')':
            depth -= 1
    return qt.strip()


SIZE1 = {'bool', 'char', 'signed char', 'unsigned char', 'BYTE', 'uint8_t', 'int8_t', 'BOOLEAN'}
SIZE2 = {'short', 'unsigned short', 'WORD', 'uint16_t', 'int16_t', 'wchar_t', 'SHORT', 'USHORT'}
FLOATS = {'float', 'double', 'long double', 'FLOAT', 'LD'}
SIZE8 = {'long long', 'unsigned long long', '__int64', 'unsigned __int64', 'int64_t', 'uint64_t', 'QWORD',
         'LONGLONG', 'ULONGLONG', 'LARGE_INTEGER', 'ULARGE_INTEGER'}


def classify(rt: str) -> str:
    """What of eax the declared return type sets: none, al, ax, eax, st0 (floats: eax untouched), edx:eax."""
    t = re.sub(r'\b(const|volatile|struct|class|enum)\b', '', rt).strip()
    t = re.sub(r'\s+', ' ', t)
    if t == 'void':
        return 'none'
    if '*' in t or '&' in t:
        return 'eax'
    if t in SIZE1:
        return 'al'
    if t in SIZE2:
        return 'ax'
    if t in FLOATS:
        return 'st0'
    if t in SIZE8:
        return 'edx:eax'
    return 'eax'   # int, DWORD, BOOL, enums, typedefs of 4 bytes (checked by hand for structs below)


# ---------------------------------------------------------------------------------------------------------
# 2./3. The original's callers and their use of eax

EAX_PARTS = {x86.X86_REG_EAX: 'eax', x86.X86_REG_AX: 'ax', x86.X86_REG_AL: 'al', x86.X86_REG_AH: 'ah'}
NEEDS = {'none': {'al', 'ah', 'ax', 'eax'}, 'st0': {'al', 'ah', 'ax', 'eax'}, 'al': {'ah', 'ax', 'eax'},
         'ax': {'eax'}, 'eax': set(), 'edx:eax': set()}


class Code:
    def __init__(self, exe: Exe):
        self.exe = exe
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.funcs = []   # (address, size)
        with open(ROOT / 'docs/functions.csv', newline='') as f:
            for r in csv.DictReader(f):
                self.funcs.append((int(r['address'], 16), int(r['size'] or 0)))
        self.funcs.sort()
        self.starts = [a for a, _ in self.funcs]
        self.insn = {}     # address -> capstone instruction (detail)
        self.func_of = {}  # instruction address -> function start
        for a, n in self.funcs:
            o = exe.off(a)
            if o is None or n <= 0:
                continue
            for i in self.md.disasm(exe.data[o:o + n], a):
                self.insn[i.address] = i
                self.func_of[i.address] = a

    def at(self, va):
        i = self.insn.get(va)
        if i is None:
            o = self.exe.off(va)
            if o is None:
                return None
            i = next(self.md.disasm(self.exe.data[o:o + 16], va), None)
            if i is not None:
                self.insn[va] = i
        return i


def _reads_writes(i):
    try:
        r, w = i.regs_access()
    except capstone.CsError:
        return set(), set()
    reads = {EAX_PARTS[x] for x in r if x in EAX_PARTS}
    writes = {EAX_PARTS[x] for x in w if x in EAX_PARTS}
    # idioms whose result does not depend on the register's old value: `xor al, al`, `sub eax, eax`, `or eax, -1`,
    # `and eax, 0` (capstone counts the register as read)
    ops = i.op_str.replace(' ', '').split(',')
    if len(ops) == 2 and ops[0] in ('eax', 'ax', 'al', 'ah'):
        if i.mnemonic in ('xor', 'sub', 'sbb') and ops[1] == ops[0]:
            reads = reads - {ops[0]}
        elif i.mnemonic == 'or' and ops[1] in ('0xffffffff', '0xffff', '0xff', '-1'):
            reads = reads - {ops[0]}
        elif i.mnemonic == 'and' and ops[1] == '0':
            reads = reads - {ops[0]}
        elif i.mnemonic == 'and' and ops[0] in ('eax', 'ax') and re.fullmatch(r'0x[0-9a-f]+|\d+', ops[1]) and int(ops[1], 0) <= 0xff:
            reads = (reads - {ops[0]}) | {'al'}   # `and eax, 0xff`: only al reaches the result (batch 6-16b)
    return reads, writes


# the C runtime's exits: exit(), _exit(), _amsg_exit() and abort() never return (doexit with its third argument 0,
# which exit() and _exit() pass); code after a call of them is another path
NORETURN = {0x5d0e36, 0x5d0e47, 0x5d3397, 0x5dd087}

PARTS = {'eax': {'al', 'ah', 'hi'}, 'ax': {'al', 'ah'}, 'al': {'al'}, 'ah': {'ah'}}


def eax_use_after(code: Code, call_insn, needs: set[str], limit=300, start=None, live0=None, stop_at_read_any=False,
                  within=None):
    """First read of a part of eax in `needs` after the call (DFS), or a ret reached with eax live.

    The parts are 'al', 'ah' and 'hi' (the upper half). `start`/`live0` start elsewhere with other live parts
    (the own-code test). Returns a list of (kind, address, text): kind 'read', 'ret' or 'jmp?'."""
    found = []
    seen = set()
    stack = [(start if start is not None else call_insn.address + call_insn.size,
              frozenset(live0) if live0 is not None else frozenset({'al', 'ah', 'hi'}))]
    steps = 0
    while stack and steps < limit:
        va, live = stack.pop()
        while steps < limit:
            if (va, live) in seen:
                break
            seen.add((va, live))
            steps += 1
            i = code.at(va)
            if i is None:
                break
            reads, writes = _reads_writes(i)
            # which parts the read touches
            touched = set()
            for r in reads:
                touched |= PARTS[r]
            need_parts = set()
            for p in needs:
                need_parts |= {'eax': {'hi'}, 'ax': {'ah'}, 'ah': {'ah'}, 'al': {'al'}}[p]
            if stop_at_read_any:
                # the own-code test: any read makes the write a temporary, not a return value. This misses
                # `return p` after a use of p in eax (6-12's TurnDischarge: `mov eax, edi; fmul [eax+4]`); such
                # functions are found through their callers or their vtable family instead
                if touched & live:
                    found.append(('read', i.address, f'{i.mnemonic} {i.op_str}'))
                    return found
            elif touched & live & need_parts:
                if i.mnemonic == 'push' and 'al' not in live:
                    # `mov al, x; push eax`: MSVC pushes a byte argument as the whole register, the callee reads
                    # only its low byte; the stale upper part is no use of the result
                    va = i.address + i.size
                    continue
                found.append(('read', i.address, f'{i.mnemonic} {i.op_str}'))
                break
            m = i.mnemonic
            if m == 'call':
                break   # the callee's result replaces eax (or the callee never returns: NORETURN)
            if m in ('ret', 'retn'):
                if live & need_parts:
                    found.append(('ret', i.address, f'{m} {i.op_str}', live))
                break
            for w in writes:
                live = live - PARTS[w]
            if not live:
                break
            if m == 'jmp':
                op = i.operands[0] if i.operands else None
                if op is not None and op.type == x86.X86_OP_IMM:
                    if within is not None and code.func_of.get(op.imm) != within:
                        break   # a tail jump out of the function
                    va = op.imm
                    continue
                found.append(('jmp?', i.address, f'{m} {i.op_str}'))
                break
            if m.startswith('j') or m in ('loop', 'loope', 'loopne', 'jecxz'):
                op = i.operands[0]
                if op.type == x86.X86_OP_IMM and (within is None or code.func_of.get(op.imm) == within):
                    stack.append((op.imm, live))
            va = i.address + i.size
    return found


def own_return_writes(code: Code, fn: int, needs: set[str]):
    """The original's own code: writes of eax that reach a `ret` of the function unread.

    MSVC 6 with /O2 drops dead stores, so a write of eax (other than a call's result) that no instruction reads
    before the function returns is the function's return value. Returns [(write address, text, ret address)]."""
    out = []
    addrs = sorted(a for a, f in code.func_of.items() if f == fn)
    need_parts = set()
    for p in needs:
        need_parts |= {'eax': {'hi'}, 'ax': {'ah'}, 'ah': {'ah'}, 'al': {'al'}}[p]
    for a in addrs:
        i = code.insn[a]
        if i.mnemonic in ('call', 'ret', 'retn') or i.mnemonic.startswith('j'):
            continue
        _, writes = _reads_writes(i)
        nxt = code.insn.get(a + i.size)
        if (nxt is not None and i.mnemonic not in ('mov', 'lea', 'movzx', 'movsx', 'pop', 'xchg') and
                (nxt.mnemonic.startswith(('j', 'set', 'cmov')) or nxt.mnemonic in ('adc', 'sbb'))
                and nxt.mnemonic != 'jmp'):
            continue   # written for its flags (a switch's `dec eax; je`): the compiler keeps it for the branch
        live = set()
        for w in writes:
            live |= PARTS[w]
        if not live & need_parts:
            continue
        res = eax_use_after(code, None, needs, start=a + i.size, live0=live, stop_at_read_any=True, within=fn)
        if any(x[0] == 'read' for x in res):
            continue   # read on some path (a loop's counter or pointer): not dead at the ret
        for x in res:
            # the whole written part must arrive (a later `fnstsw ax` over a dead `lea eax` makes no return value)
            if x[0] == 'ret' and x[3] == live:
                out.append((a, f'{i.mnemonic} {i.op_str}', x[1]))
                break
    return out


def last_eax_writes(code: Code, fn: int):
    """Per ret of the function: the instructions before it that wrote eax last on the straight line back."""
    out = []
    addrs = sorted(a for a, f in code.func_of.items() if f == fn)
    for k, a in enumerate(addrs):
        i = code.insn[a]
        if i.mnemonic not in ('ret', 'retn'):
            continue
        w = None
        for b in reversed(addrs[:k]):
            j = code.insn[b]
            _, writes = _reads_writes(j)
            if j.mnemonic == 'call':
                w = f'{b:#x} {j.mnemonic} {j.op_str} (its result)'
                break
            if writes:
                w = f'{b:#x} {j.mnemonic} {j.op_str}'
                break
            if j.mnemonic in ('ret', 'retn', 'jmp'):
                w = '(reached by a jump; see the branch)'
                break
        out.append((a, w))
    return out


# ---------------------------------------------------------------------------------------------------------
# 5. The reimplementation's own typed calls: `fn<R (CC *)(...)>(0xADDR)` (and `original<>`, `exe_or_dead<>`)
#    declare what the caller expects back; a hooked function that provides less is read as garbage there

WIDTH = {'none': 0, 'al': 1, 'ax': 2, 'eax': 4, 'edx:eax': 8}
CALLERS = ('fn', 'original', 'exe_or_dead')
SKIP = ('ImplicitCastExpr', 'ParenExpr', 'ExprWithCleanups', 'MaterializeTemporaryExpr', 'CXXBindTemporaryExpr')


FUNC_KINDS = ('FunctionDecl', 'CXXMethodDecl')


def _const(n):
    """An integer constant expression of literals, casts, parentheses, ~ and unary minus; None otherwise."""
    k = n.get('kind')
    if k == 'IntegerLiteral':
        return int(n['value'])
    if k == 'ConstantExpr' and 'value' in n:
        return int(n['value'])
    if k in SKIP + ('CStyleCastExpr',) and n.get('inner'):
        return _const(n['inner'][0])
    if k == 'UnaryOperator' and n.get('inner'):
        v = _const(n['inner'][0])
        if v is None:
            return None
        return {'~': ~v, '-': -v, '+': v}.get(n.get('opcode'))
    return None


def _use_of(node, chain):
    """How the value of the expression `node` (with its ancestors `chain`) is used: 'used', 'returned' (the
    enclosing function returns it) or 'discarded'."""
    child = node
    for par in reversed(chain):
        k = par.get('kind')
        if k in SKIP:
            child = par
            continue
        idx = next((x for x, c in enumerate(par.get('inner', [])) if c is child), -1)
        if k == 'CompoundStmt':
            return 'discarded'
        if k == 'CStyleCastExpr' and par.get('type', {}).get('qualType') == 'void':
            return 'discarded'
        if k in ('IfStmt', 'WhileStmt') and idx >= 1:
            return 'discarded'
        if k == 'DoStmt' and idx == 0:
            return 'discarded'
        if k in ('CaseStmt', 'DefaultStmt', 'LabelStmt', 'AttributedStmt'):
            return 'discarded' if k != 'CaseStmt' or idx >= 1 else 'used'
        if k == 'ForStmt' and idx >= 3:
            return 'discarded'
        if k == 'BinaryOperator' and par.get('opcode') == ',':
            if idx == 0:
                return 'discarded'
            child = par
            continue
        if k == 'ReturnStmt':
            return 'returned'
        # what part of eax the use reads: a cast to a 1- or 2-byte type or a mask (`(BYTE)f()`, `f() & 0xff`) reads
        # al or ax; `f() & 0xffffff00` and `with_al(f(), ...)` keep the upper part of a leftover eax for the caller's
        # own eax (the reimplementation's eax emulation)
        if k in ('CStyleCastExpr', 'CXXStaticCastExpr', 'CXXFunctionalCastExpr'):
            w = classify(par.get('type', {}).get('qualType', ''))
            if w in ('al', 'ax'):
                return 'used ' + w
        if k == 'BinaryOperator' and par.get('opcode') == '&':
            other = [c for x, c in enumerate(par.get('inner', [])) if x != idx]
            v = _const(other[0]) if other else None
            if v is not None:
                v &= 0xffffffff
                if v <= 0xff:
                    return 'used al'
                if v <= 0xffff:
                    return 'used ax'
                if v & 0xff == 0:
                    return 'used hi'
        if k == 'CallExpr' and idx == 1:
            f = par['inner'][0]
            while f.get('kind') in SKIP and f.get('inner'):
                f = f['inner'][0]
            if f.get('kind') == 'DeclRefExpr' and f.get('referencedDecl', {}).get('name') == 'with_al':
                return 'used hi'
        return 'used'
    return 'used'


def _file_typed_calls(clang: str, incdir: str, src: str):
    """One source file's typed calls of exe addresses and its function calls, from clang's JSON AST of the whole
    translation unit. Returns (typed, calls, escapes):
      typed:   [(file, offset, address, return type, sugared type, use, enclosing function key)]
      calls:   [(callee key, use, enclosing function key)]   direct calls of named functions
      escapes: [callee key]   functions whose address is taken (their result goes to unknown callers)
    A function's key is its mangled name, or (source, name, type) for one with internal linkage."""
    cmd = [clang, *CXXFLAGS, f'-I{incdir}', '-Xclang', '-ast-dump=json', src]
    r = subprocess.run(cmd, cwd=ROOT / 'reimpl', capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(f'{src}: clang failed\n{r.stderr[-2000:]}')
    tree = json.loads(r.stdout)
    typed, calls, escapes = [], [], []
    cur = {'file': src}
    key_of_id = {}

    def note(loc):
        if not isinstance(loc, dict):
            return None
        res = None
        for part in ('spellingLoc', 'expansionLoc'):
            if part in loc:
                if 'file' in loc[part]:
                    cur['file'] = loc[part]['file']
                if part == 'expansionLoc':
                    res = (cur['file'], loc[part].get('offset'))
        if 'spellingLoc' not in loc and 'expansionLoc' not in loc:
            if 'file' in loc:
                cur['file'] = loc['file']
            res = (cur['file'], loc.get('offset'))
        return res

    def fkey(d):
        if d.get('id') in key_of_id:
            return key_of_id[d['id']]
        return d.get('mangledName') or (src, d.get('name'), d.get('type', {}).get('qualType'))

    def strip(c):
        while c.get('kind') in SKIP and c.get('inner'):
            c = c['inner'][0]
        return c

    def callee_address(call):
        inner = call.get('inner', [])
        if not inner:
            return None
        c = strip(inner[0])
        if c.get('kind') != 'CallExpr' or len(c.get('inner', [])) < 2:
            return None
        f = strip(c['inner'][0])
        if f.get('kind') != 'DeclRefExpr' or f.get('referencedDecl', {}).get('name') not in CALLERS:
            return None
        return _int_value(c['inner'][1])

    vcalls = []
    st = [(tree, [], None, None)]
    while st:
        node, ch, encl, encl_name = st.pop()
        if not isinstance(node, dict):
            continue
        note(node.get('loc'))
        rng = node.get('range', {})
        begin = note(rng.get('begin'))
        note(rng.get('end'))
        kind = node.get('kind')
        if kind in FUNC_KINDS:
            k = node.get('mangledName') or (src, node.get('name'), node.get('type', {}).get('qualType'))
            key_of_id[node.get('id')] = k
            if node.get('previousDecl'):
                key_of_id.setdefault(node['previousDecl'], k)
            if any(c.get('kind') == 'CompoundStmt' for c in node.get('inner', [])):
                encl = k
                encl_name = node.get('name')
        elif kind == 'CallExpr':
            a = callee_address(node)
            vf = strip(node['inner'][0]) if node.get('inner') else {}
            if (a is None and vf.get('kind') == 'DeclRefExpr' and vf.get('referencedDecl', {}).get('name') == 'vcall'
                    and len(node['inner']) >= 3):
                slot = _const(node['inner'][2])
                t = node.get('type', {})
                vcalls.append((begin[0] if begin else '?', begin[1] if begin else 0, slot,
                               t.get('desugaredQualType', t.get('qualType', '')), _use_of(node, ch), encl, encl_name))
            if a is not None:
                t = node.get('type', {})
                typed.append((begin[0] if begin else '?', begin[1] if begin else 0, a,
                              t.get('desugaredQualType', t.get('qualType', '')), t.get('qualType', ''),
                              _use_of(node, ch), encl))
            elif node.get('inner'):
                f = strip(node['inner'][0])
                if f.get('kind') == 'DeclRefExpr' and f.get('referencedDecl', {}).get('kind') in FUNC_KINDS:
                    calls.append((fkey(f['referencedDecl']), _use_of(node, ch), encl,
                                  begin[0] if begin else '?', begin[1] if begin else 0))
        elif kind == 'DeclRefExpr' and node.get('referencedDecl', {}).get('kind') in FUNC_KINDS:
            # an address taken: not the callee of a call
            is_callee = False
            if ch:
                n2 = node
                for p in reversed(ch):
                    if p.get('kind') in SKIP:
                        n2 = p
                        continue
                    is_callee = p.get('kind') == 'CallExpr' and p.get('inner') and p['inner'][0] is n2
                    break
            if not is_callee:
                escapes.append(fkey(node['referencedDecl']))
        nch = ch + [node]
        for c in reversed(node.get('inner', [])):
            st.append((c, nch, encl, encl_name))
    return typed, calls, escapes, vcalls


def typed_calls(jobs: int):
    """[(file, line, address, return type, use, sugared type)] of every source's typed calls of an exe address; a
    call whose value its function returns counts as used when that function's value is used somewhere (followed
    through further returns) or its address is taken."""
    clang = find_clang()
    from concurrent.futures import ProcessPoolExecutor
    with tempfile.TemporaryDirectory() as tmp:
        prepare_includes(tmp)
        srcs = [f'src/{p.name}' for p in sorted((ROOT / 'reimpl/src').glob('*.cpp'))]
        with ProcessPoolExecutor(jobs) as ex:
            res = list(ex.map(_file_typed_calls, [clang] * len(srcs), [tmp] * len(srcs), srcs))
    uses = defaultdict(set)      # function key -> uses of its calls' values (with the enclosing function)
    escaped = set()
    where = defaultdict(list)    # function key -> (file, offset, use) of its calls
    for _, calls, esc, _ in res:
        for k, use, encl, fname, off in calls:
            uses[k].add((use, encl))
            where[k].append((fname, off, use))
        escaped.update(esc)
    memo = {}

    def value_used(k, depth=0):
        """'used' when a call of k uses its value (followed through returns), 'escapes' when only its address is
        taken (a hook's replacement or a table entry: its value goes to the exe's callers, which part 3 checks),
        '' otherwise."""
        if k in memo:
            return memo[k]
        memo[k] = ''
        best = 'escapes' if k in escaped else ''
        for use, encl in uses.get(k, ()):
            if use.startswith('used') and use != 'used hi':
                best = 'used'
                break
            if use == 'returned' and encl is not None and depth < 20:
                r = value_used(encl, depth + 1)
                if r == 'used':
                    best = 'used'
                    break
                if r == 'escapes':
                    best = 'escapes'
        memo[k] = best
        return best

    seen = {}
    lines_cache = {}
    for typed, _, _, _ in res:
        for fname, off, a, rt, rt_sugar, use, encl in typed:
            key = (fname, off)
            if key in seen:
                continue
            if use == 'returned':
                use = 'returned, ' + ((value_used(encl) if encl is not None else '') or 'unused')
            path = ROOT / 'reimpl' / fname
            if path not in lines_cache:
                try:
                    lines_cache[path] = path.read_bytes()
                except OSError:
                    lines_cache[path] = b''
            line = lines_cache[path].count(b'\n', 0, off or 0) + 1
            via = []
            if use == 'returned, used':
                for f2, off2, u2 in where.get(encl, ()):
                    if u2.startswith('used') or u2 == 'returned':
                        p2 = ROOT / 'reimpl' / f2
                        if p2 not in lines_cache:
                            lines_cache[p2] = p2.read_bytes() if p2.exists() else b''
                        ln = lines_cache[p2].count(b'\n', 0, off2 or 0) + 1
                        via.append(f'{f2}:{ln} ({u2})')
            seen[key] = (fname, line, a, rt, use, rt_sugar, encl, via)
    vseen = {}
    for _, _, _, vcalls in res:
        for fname, off, slot, rt, use, encl, encl_name in vcalls:
            key = (fname, off)
            if key in vseen:
                continue
            path = ROOT / 'reimpl' / fname
            if path not in lines_cache:
                lines_cache[path] = path.read_bytes() if path.exists() else b''
            line = lines_cache[path].count(b'\n', 0, off or 0) + 1
            if use == 'returned':
                use = 'returned, ' + ((value_used(encl) if encl is not None else '') or 'unused')
            vseen[key] = (fname, line, slot, rt, use, encl_name)
    return list(seen.values()), list(vseen.values())


# ---------------------------------------------------------------------------------------------------------
# 6. Per call site: whose vtable a `call [reg+offset]` reads (batch 6-16b)
#
# The register holding the vtable is followed back through the caller's code (every path, over the
# predecessors of each instruction) to its definition: `mov reg, [obj]` makes obj the receiver, and obj in turn
# is followed back. A receiver that is the caller's own `this` (ecx at the caller's entry, or a register copied
# from it) is resolved: `this` is an object of a class whose vtable holds the caller (any slot), or one that a
# direct caller passes as its own `this` (a derived class's override calling the base's implementation), followed
# through those callers. The call can then only reach the entries at that offset of those vtables. Other receivers
# (a field, a global, a call's result, an argument) stay unresolved and are judged as before (related vtables).

REG32 = {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp'}
SUB = {'al': 'eax', 'ah': 'eax', 'ax': 'eax', 'bl': 'ebx', 'bh': 'ebx', 'bx': 'ebx', 'cl': 'ecx', 'ch': 'ecx',
       'cx': 'ecx', 'dl': 'edx', 'dh': 'edx', 'dx': 'edx', 'si': 'esi', 'di': 'edi', 'bp': 'ebp', 'sp': 'esp'}


class Receivers:
    def __init__(self, code: Code, vt_entries: dict, direct: dict, jumps: dict, other_refs: set):
        self.code, self.vt_entries, self.direct, self.jumps, self.other_refs = code, vt_entries, direct, jumps, other_refs
        self.preds = defaultdict(list)
        by_fn = defaultdict(list)
        for a, f in code.func_of.items():
            by_fn[f].append(a)
        for f, addrs in by_fn.items():
            addrs.sort()
            for k, a in enumerate(addrs):
                i = code.insn[a]
                m = i.mnemonic
                if k + 1 < len(addrs) and m not in ('jmp', 'ret', 'retn') and not (
                        m == 'call' and i.operands and i.operands[0].type == x86.X86_OP_IMM and i.operands[0].imm in NORETURN):
                    self.preds[addrs[k + 1]].append(a)
                if (m.startswith('j') or m.startswith('loop')) and i.operands and i.operands[0].type == x86.X86_OP_IMM:
                    if code.func_of.get(i.operands[0].imm) == f:
                        self.preds[i.operands[0].imm].append(a)
        self.in_vtables = defaultdict(set)   # function -> vtables holding it
        for va, ents in vt_entries.items():
            for e in ents:
                self.in_vtables[e].add(va)
        self.memo = {}
        self.fp = {}
        self.static_memo = {}
        code.starts_set = set(code.starts)
        self.mov_ecx = defaultdict(list)     # constant -> `mov ecx, constant` instructions
        self.vptr_stores = defaultdict(list)  # address -> the vtables stored there as constants (None: another value)
        for i in code.insn.values():
            ops = i.operands
            if i.mnemonic == 'mov' and len(ops) == 2:
                if ops[0].type == x86.X86_OP_REG and i.reg_name(ops[0].reg) == 'ecx' and ops[1].type == x86.X86_OP_IMM:
                    self.mov_ecx[ops[1].imm].append(i)
                elif ops[0].type == x86.X86_OP_MEM and ops[0].mem.base == 0 and ops[0].mem.index == 0 and ops[0].size == 4:
                    v = ops[1].imm if ops[1].type == x86.X86_OP_IMM else None
                    self.vptr_stores[ops[0].mem.disp & 0xffffffff].append(v if v in vt_entries else None)

    def frame_pointer(self, fn):
        """Whether fn keeps a frame in ebp (`push ebp; mov ebp, esp`); otherwise MSVC /O2 uses ebp as a register."""
        if fn not in self.fp:
            a = self.code.insn.get(fn)
            b = self.code.insn.get(fn + a.size) if a else None
            self.fp[fn] = bool(a and b and a.mnemonic == 'push' and a.op_str == 'ebp' and b.mnemonic == 'mov'
                               and b.op_str.replace(' ', '') == 'ebp,esp')
        return self.fp[fn]

    def _def_origin(self, i, reg, fn, depth):
        """The origin of reg as instruction i writes it."""
        m, ops = i.mnemonic, i.operands
        if m == 'call':
            if reg == 'eax':
                return ('result', ops[0].imm if ops and ops[0].type == x86.X86_OP_IMM else None)
            return ('clobbered',)
        if m in ('mov', 'lea') and len(ops) == 2 and ops[0].type == x86.X86_OP_REG:
            src = ops[1]
            if src.type == x86.X86_OP_REG:
                return self.origin(fn, i.address, i.reg_name(src.reg), depth + 1)
            if src.type == x86.X86_OP_MEM and src.mem.index == 0:
                base = i.reg_name(src.mem.base) if src.mem.base else None
                if base is None:
                    return ('global', src.mem.disp & 0xffffffff)
                if base == 'esp' or (base == 'ebp' and self.frame_pointer(fn)):
                    return ('stack',)
                inner = self.origin(fn, i.address, base, depth + 1)
                return ('sub' if m == 'lea' else 'field', inner, src.mem.disp)
            if src.type == x86.X86_OP_IMM:
                return ('const', src.imm)
        if m == 'xor' and len(ops) == 2 and ops[1].type == x86.X86_OP_REG and i.reg_name(ops[1].reg) == reg:
            return ('const', 0)
        if m == 'pop':
            return ('stack',)
        return ('other', m)

    def origin(self, fn, at, reg, depth=0, seen=None):
        """Where the 32-bit register reg comes from just before the instruction at `at` (all paths must agree)."""
        if depth > 12:
            return ('deep',)
        seen = seen if seen is not None else set()
        found = set()
        stack = [at]
        while stack:
            a = stack.pop()
            ps = self.preds.get(a, [])
            if not ps:
                found.add(('this',) if a == fn and reg == 'ecx' else ('entry', reg) if a == fn else ('nopred',))
                continue
            for p in ps:
                if (p, reg) in seen:
                    continue
                seen.add((p, reg))
                i = self.code.insn[p]
                try:
                    _, w = i.regs_access()
                except capstone.CsError:
                    w = ()
                wn = {SUB.get(i.reg_name(x), i.reg_name(x)) for x in w}
                if reg in wn:
                    if any(i.reg_name(x) not in REG32 for x in w if SUB.get(i.reg_name(x), i.reg_name(x)) == reg):
                        found.add(('partial',))
                    else:
                        found.add(self._def_origin(i, reg, fn, depth))
                elif i.mnemonic == 'call' and reg in ('eax', 'ecx', 'edx'):
                    found.add(self._def_origin(i, reg, fn, depth))   # eax its result, ecx/edx clobbered
                else:
                    stack.append(p)
            if len(found) > 1:
                return ('join',)
        return found.pop() if len(found) == 1 else ('none',)

    def vtable_receiver(self, call):
        """The receiver whose vtable `call [reg+disp]` reads: the origin of obj in `mov reg, [obj]`."""
        fn = self.code.func_of.get(call.address)
        base = call.reg_name(call.operands[0].mem.base)
        o = self.origin(fn, call.address, base)
        if o[0] == 'field' and o[2] == 0:
            return o[1]
        return ('vptr', o)

    def this_classes(self, fn, depth=0):
        """The vtables `this` of fn can have, or None when not every way in is known."""
        if fn in self.memo:
            return self.memo[fn]
        if depth > 8:
            return None
        self.memo[fn] = set()   # a cycle adds nothing
        res = set(self.in_vtables.get(fn, ()))
        if fn in self.other_refs:
            res = None
        sites = self.direct.get(fn, []) + self.jumps.get(fn, [])
        if res is not None and not sites and not res:
            res = None
        for c in sites if res is not None else []:
            h = self.code.func_of.get(c.address)
            o = self.origin(h, c.address, 'ecx') if h is not None else ('none',)
            if o[0] == 'const':   # a static object passed as this
                r2 = self.static_object(o[1])
                if not r2:
                    res = None
                    break
                res |= r2
                continue
            if o != ('this',):
                res = None
                break
            r2 = self.this_classes(h, depth + 1)
            if r2 is None:
                res = None
                break
            res |= r2
        self.memo[fn] = res
        return res

    def own_vptr_stores(self, fn, depth=0, seen=None):
        """The vtables fn stores into its own `this` (+0), and those of the functions it calls or jumps to with its
        `this` in ecx (the base constructors, the destructors): None when such a store is not a constant."""
        seen = seen if seen is not None else set()
        if fn in seen or depth > 6:
            return set()
        seen.add(fn)
        out = set()
        for a, f in self.code.func_of.items():
            if f != fn:
                continue
            i = self.code.insn[a]
            ops = i.operands
            if i.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM and ops[0].size == 4 \
                    and ops[0].mem.base and ops[0].mem.index == 0 and ops[0].mem.disp == 0 \
                    and self.origin(fn, a, i.reg_name(ops[0].mem.base)) == ('this',):
                if ops[1].type == x86.X86_OP_IMM and ops[1].imm in self.vt_entries:
                    out.add(ops[1].imm)
                else:
                    return None
            elif i.mnemonic in ('call', 'jmp') and ops and ops[0].type == x86.X86_OP_IMM and ops[0].imm in self.code.starts_set \
                    and ops[0].imm != fn and self.origin(fn, a, 'ecx') == ('this',):
                r = self.own_vptr_stores(ops[0].imm, depth + 1, seen)
                if r is None:
                    return None
                out |= r
        return out

    def static_object(self, addr):
        """The vtables a static object at addr can have: those its constructor, destructor and any other function
        called with ecx = addr store at +0, and constant stores of a vtable into [addr]; None if anything else
        writes there or the object's address is taken otherwise."""
        if addr in self.static_memo:
            return self.static_memo[addr]
        self.static_memo[addr] = None
        out = set()
        for i in self.mov_ecx.get(addr, []):
            fn = self.code.func_of.get(i.address)
            # the next call or jump with ecx still addr
            nxt = self.code.insn.get(i.address + i.size)
            k = 0
            while nxt is not None and k < 12 and nxt.mnemonic not in ('call', 'jmp'):
                nxt = self.code.insn.get(nxt.address + nxt.size)
                k += 1
            if nxt is None or not nxt.operands or nxt.operands[0].type != x86.X86_OP_IMM:
                continue   # an indirect call on the object (it reads its vtable: no store)
            if self.origin(fn, nxt.address, 'ecx')[0] != 'const':
                continue
            r = self.own_vptr_stores(nxt.operands[0].imm)
            if r is None:
                return None
            out |= r
        for v in self.vptr_stores.get(addr, []):
            if v is None:
                return None
            out.add(v)
        self.static_memo[addr] = out or None
        return self.static_memo[addr]

    def targets(self, call):
        """(the functions the call can reach, or None, and the receiver's origin)."""
        recv = self.vtable_receiver(call)
        disp = call.operands[0].mem.disp
        if disp % 4:
            return None, recv
        static = None
        if recv[0] == 'vptr' and recv[1][0] == 'global':
            static = recv[1][1]
        elif recv[0] == 'const':
            static = recv[1]
        if static is not None:
            vts = self.static_object(static)
            if not vts:
                return None, ('static object, classes unknown',)
            # a vtable shorter than the slot (a base class's, stored by the base constructor before the derived
            # one's) cannot be the object's at a call of that slot
            out = {self.vt_entries[va][disp // 4] for va in vts if disp // 4 < len(self.vt_entries[va])}
            if not out:
                return None, ('static object, slot past every vtable',)
            return out, ('static object',)
        if recv != ('this',):
            return None, recv
        vts = self.this_classes(self.code.func_of.get(call.address))
        if not vts:
            return None, ('this, classes unknown',)
        out = {self.vt_entries[va][disp // 4] for va in vts if disp // 4 < len(self.vt_entries[va])}
        if not out:
            return None, ('this, slot past every vtable',)
        return out, recv


def origin_kind(o) -> str:
    k = o[0]
    if k == 'vptr':
        if o[1][0] == 'field':
            return 'embedded object of this' if o[1][1] == ('this',) else 'embedded object of another object'
        return 'vtable pointer from ' + origin_kind(o[1])
    if k == 'field':
        return 'field of this' if o[1] == ('this',) else 'field of another object'
    if k == 'sub':
        return 'embedded object'
    return {'global': 'global', 'result': 'a call\'s result', 'stack': 'argument or local', 'this': 'this',
            'join': 'differs by path', 'entry': 'register at entry'}.get(k, k)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', default=str(ROOT / 'gamedata/Zax - German/Zax.exe'))
    ap.add_argument('--jobs', type=int, default=8)
    ap.add_argument('--verbose', action='store_true', help='print every call site with a read')
    ap.add_argument('--assume-void', default='', help='comma-separated addresses to treat as declared void (to show '
                    'that the scan finds a known case, e.g. 0x49b1d0, the TurnDischarge of batch 6-12)')
    ap.add_argument('--no-sources', action='store_true', help='stop before the reimplementation\'s own calls (parts 5, 7, 8)')
    args = ap.parse_args(argv)

    hooks = hook_table(args.jobs)
    by_addr = {}
    for h in hooks:
        h['ret'] = return_type(h['type']) if h['decl_kind'] in ('FunctionDecl', 'CXXMethodDecl') else '?'
        h['provides'] = classify(h['ret']) if h['ret'] != '?' else '?'
        by_addr.setdefault(h['address'], h)
    for x in filter(None, args.assume_void.split(',')):
        h = by_addr[int(x, 16)]
        h['ret'], h['provides'] = 'void (assumed)', 'none'
    kinds = defaultdict(int)
    for h in by_addr.values():
        kinds[h['provides']] += 1
    print(f'hook entries: {len(hooks)}, distinct addresses: {len(by_addr)}')
    print('declared return:', ', '.join(f'{k} {v}' for k, v in sorted(kinds.items())))
    others = [h for h in by_addr.values() if h['provides'] == '?']
    for h in others:
        print(f'  not a function: {h["address"]:#x} {h["replacement"]} ({h["decl_kind"]}, {h["module"]})')

    exe = Exe(Path(args.exe))
    code = Code(exe)
    print(f'functions disassembled: {len(code.funcs)}, instructions: {len(code.insn)}')

    # call sites: direct and indirect by displacement
    direct = defaultdict(list)
    indirect = defaultdict(list)   # displacement -> call instructions [reg+disp]
    for i in code.insn.values():
        if i.mnemonic != 'call' or not i.operands:
            continue
        op = i.operands[0]
        if op.type == x86.X86_OP_IMM:
            direct[op.imm].append(i)
        elif op.type == x86.X86_OP_MEM and op.mem.base != 0 and op.mem.index == 0:
            indirect[op.mem.disp].append(i)

    # dword references: vtable slots and other
    vtables = []
    with open(ROOT / 'docs/exe_layout.csv', newline='') as f:
        for r in csv.DictReader(f):
            if r['kind'] == 'vtable':
                vtables.append((int(r['address'], 16), int(r['size'])))
    vtables.sort()
    import bisect
    vstarts = [a for a, _ in vtables]

    def vtable_slot(va):
        k = bisect.bisect_right(vstarts, va) - 1
        if k >= 0 and va < vtables[k][0] + vtables[k][1]:
            return vtables[k][0], va - vtables[k][0]
        return None

    refs = defaultdict(list)
    for sec in ('.rdata', '.data'):
        lo, size, raw, rsize = exe.secs[sec]
        n = min(size, rsize)
        for off in range(0, n - 3, 4):
            v = struct.unpack_from('<I', exe.data, raw + off)[0]
            if v in by_addr:
                refs[v].append(lo + off)

    asm = asm_names()
    low = [h for h in by_addr.values() if h['provides'] in ('none', 'st0', 'al', 'ax')]
    in_asm = [h for h in low if h['replacement'] in asm]
    scanned = [h for h in low if h['replacement'] not in asm]
    print(f'declared void, float/double, 1 or 2 bytes: {len(low)}; of them written in assembly (they keep the '
          f'original\'s registers, not scanned): {len(in_asm)}; scanned: {len(scanned)}')

    def call_passthrough(fn):
        """Calls whose result reaches a ret of fn unread (`return g();` or a void tail call)."""
        out = []
        for a, f in code.func_of.items():
            if f == fn and code.insn[a].mnemonic == 'call':
                c = code.insn[a]
                if c.operands and c.operands[0].type == x86.X86_OP_IMM and c.operands[0].imm in NORETURN:
                    continue
                res = eax_use_after(code, None, {'eax'}, start=a + c.size, live0={'al', 'ah', 'hi'},
                                    stop_at_read_any=True, within=fn)
                if any(x[0] == 'ret' for x in res) and not any(x[0] == 'read' for x in res):
                    out.append(a)
        return out

    rows = []
    for h in sorted(scanned, key=lambda h: h['address']):
        a = h['address']
        needs = NEEDS[h['provides']]
        h['own'] = own_return_writes(code, a, needs)
        h['tail'] = call_passthrough(a) if h['provides'] in ('none', 'st0') else []
        h['direct_reads'], h['direct_rets'] = [], []
        for c in ([] if a in NORETURN else direct.get(a, [])):
            for x in eax_use_after(code, c, needs):
                if x[0] == 'read':
                    h['direct_reads'].append((c.address, x[1], x[2]))
                elif x[0] == 'ret':
                    h['direct_rets'].append((c.address, x[1], x[3]))
        # a caller that returns with the value still in eax passes it on: follow its own direct callers (a few
        # levels; through vtables the chain is not followed)
        h['through_ret'] = []
        frontier = [(code.func_of.get(c), 1, live) for c, _, live in h['direct_rets']]
        done = set()
        while frontier:
            fn2, depth, live = frontier.pop()
            if fn2 is None or fn2 in NORETURN or (fn2, live) in done or depth > 4:
                continue
            done.add((fn2, live))
            for c in direct.get(fn2, []):
                for x in eax_use_after(code, c, needs, live0=live):
                    if x[0] == 'read':
                        h['through_ret'].append((fn2, c.address, x[1], x[2]))
                    elif x[0] == 'ret':
                        frontier.append((code.func_of.get(c.address), depth + 1, x[3]))
        slots, other = set(), []
        for r in refs.get(a, []):
            vs = vtable_slot(r)
            if vs:
                slots.add(vs[1])
            else:
                other.append(r)
        h['slots'], h['other_refs'] = sorted(slots), other
        h['slot_reads'] = []
        for disp in h['slots']:
            for c in indirect.get(disp, []):
                for x in eax_use_after(code, c, needs):
                    if x[0] == 'read':
                        h['slot_reads'].append((disp, c.address, x[1], x[2]))
        h['n_direct'] = len(direct.get(a, []))
        rows.append(h)

    # vtable families per slot: two vtables share the virtual function of slot d when they hold the same function at
    # a slot k >= d and that function occurs at no other slot index anywhere: a base class declares slots 0..k and
    # both inherit it unchanged, so slot d is that base's too (sound and transitive for single inheritance). A function
    # folded with others of the same body (an empty stub) sits at several indexes and links nothing. Shared entries
    # below d prove nothing: the CObject slots link classes whose later slots mean different things.
    vt_entries = {}
    for va, size in vtables:
        vt_entries[va] = [exe.dword(va + 4 * k) for k in range(size // 4)]
    idx_of = defaultdict(set)
    for va, ents in vt_entries.items():
        for k, e in enumerate(ents):
            idx_of[e].add(k)
    shared = defaultdict(list)   # (k, e) -> vtables
    for va, ents in vt_entries.items():
        for k, e in enumerate(ents):
            if len(idx_of[e]) == 1 and code.func_of.get(e) is not None:
                shared[(k, e)].append(va)
    family_cache = {}

    def family(d):
        """Union-find components of the vtables at slot index d."""
        if d in family_cache:
            return family_cache[d]
        parent = {va: va for va in vt_entries}

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x
        for (k, e), vas in shared.items():
            if k >= d and len(vas) > 1:
                r = find(vas[0])
                for v in vas[1:]:
                    parent[find(v)] = r
        comp = {va: find(va) for va in vt_entries}
        family_cache[d] = comp
        return comp
    own_cache = {}

    def sibling_returns(e):
        """A sibling entry's original writes eax for its ret (its replacement's declared type is no evidence: many
        replacements declare DWORD for a slot whose value no caller uses)."""
        if e not in own_cache:
            own_cache[e] = own_return_writes(code, e, {'eax'})
        if own_cache[e]:
            w, txt, ret = own_cache[e][0]
            return f'{e:#x} original writes eax for its ret ({w:#x} {txt})'
        return None

    for h in rows:
        h['family'] = []
        if not h['slots']:
            continue
        mine = [r for r in refs.get(h['address'], []) if vtable_slot(r)]
        for r in mine:
            vt, d = vtable_slot(r)
            fam = family(d // 4)
            comp = fam[vt]
            for va2, ents in vt_entries.items():
                if fam[va2] != comp or d // 4 >= len(ents):
                    continue
                e = ents[d // 4]
                if e == h['address']:
                    continue
                why = sibling_returns(e)
                if why:
                    h['family'].append((vt, d, va2, why))

    void_like = [h for h in rows if h['provides'] in ('none', 'st0')]
    narrow = [h for h in rows if h['provides'] in ('al', 'ax')]
    a_own = [h for h in void_like if h['own']]
    b_direct = [h for h in rows if h['direct_reads']]
    c_tail_read = [h for h in void_like if not h['own'] and h['tail'] and h['direct_reads']]
    e_through = [h for h in rows if h['through_ret']]
    slot_only = [h for h in rows if h['slot_reads'] and not h['own'] and not h['direct_reads'] and h not in c_tail_read]
    # a 1- or 2-byte replacement whose original writes no more than that itself (`mov al, 1; ret`) is as narrow as
    # its original, whatever the slot's other overrides return
    d_family = [h for h in slot_only if h['family'] and (h['provides'] in ('none', 'st0') or h['own'])]
    cands = {id(h): h for h in a_own + b_direct + c_tail_read + d_family + e_through}
    print(f'void/float/double: {len(void_like)}, of them the original writes eax for its ret (a return value): '
          f'{len(a_own)}; a call result reaches its ret and a caller reads eax: {len(c_tail_read)}')
    print(f'1/2-byte: {len(narrow)}; any function with a direct caller reading eax beyond the declared type: '
          f'{len(b_direct)}')
    print(f'only calls through a vtable slot offset read eax (receiver class not resolved): {len(slot_only)}')
    print(f'  of them a related vtable (same hierarchy) has a function at that slot that returns a value: '
          f'{len(d_family)}')
    print(f'a caller passes the value on with its own ret and a caller of that reads it: {len(e_through)}')
    print(f'candidates: {len(cands)}')
    for h in sorted(cands.values(), key=lambda h: h['address']):
        print(f'\n{h["address"]:#x} {h["replacement"]}  [{h["module"]}] declared {h["ret"]!r} ({h["provides"]}); '
              f'direct callers {h["n_direct"]} ({len(h["direct_reads"])} read eax), vtable slots '
              f'{", ".join(hex(s) for s in h["slots"]) or "-"} ({len(h["slot_reads"])} call sites at those offsets read eax)'
              + (f', other dwords {", ".join(hex(x) for x in h["other_refs"])}' if h['other_refs'] else ''))
        seen_f = set()
        for vt, d, va2, why in h['family']:
            if (d, why) in seen_f:
                continue
            seen_f.add((d, why))
            print(f'  family: vtable {va2:#x} slot {d:#x} (ours in {vt:#x}): {why}')
        for w, txt, ret in h['own']:
            print(f'  own: {w:#x} {txt} reaches ret {ret:#x}')
        for c in h['tail']:
            print(f'  call result reaches a ret: {c:#x}')
        for fn2, call, at, txt in h['through_ret']:
            print(f'  through {fn2:#x}: its caller {call:#x} (in {code.func_of.get(call, 0):#x}) reads at {at:#x} {txt}')
        for call, at, txt in h['direct_reads']:
            print(f'  direct caller {call:#x} (in {code.func_of.get(call, 0):#x}) reads at {at:#x} {txt}')
        for disp, call, at, txt in (h['slot_reads'] if args.verbose else h['slot_reads'][:4]):
            print(f'  slot {disp:#x}: call {call:#x} (in {code.func_of.get(call, 0):#x}) reads at {at:#x} {txt}')
    # 6. the vtable-only reads per call site
    starts = set(code.starts)
    jumps = defaultdict(list)
    for i in code.insn.values():
        if i.mnemonic == 'jmp' and i.operands and i.operands[0].type == x86.X86_OP_IMM and i.operands[0].imm in starts \
                and code.func_of.get(i.address) != i.operands[0].imm:
            jumps[i.operands[0].imm].append(i)
    fn_refs = set()
    for sec in ('.rdata', '.data'):
        lo, size, raw, rsize = exe.secs[sec]
        for off in range(0, min(size, rsize) - 3, 4):
            v = struct.unpack_from('<I', exe.data, raw + off)[0]
            if v in starts and not vtable_slot(lo + off):
                fn_refs.add(v)
    rv = Receivers(code, vt_entries, direct, jumps, fn_refs)
    site_stats = defaultdict(int)
    unresolved_kinds = defaultdict(int)
    reached = []        # (function, site, read)
    fn_class = defaultdict(list)
    slot_only_ids = {id(h) for h in slot_only}
    for h in rows:
        if not h['slot_reads']:
            continue
        mine = id(h) in slot_only_ids
        state = set()
        for disp, call, at, txt in h['slot_reads']:
            tg, recv = rv.targets(code.insn[call])
            if not mine:   # already a candidate or cleared by the other parts: only a reaching site is listed
                if tg is not None and h['address'] in tg:
                    reached.append((h, call, at, txt, tg))
                continue
            site_stats['sites'] += 1
            if tg is None:
                site_stats['unresolved'] += 1
                unresolved_kinds[origin_kind(recv)] += 1
                state.add('unresolved')
            elif h['address'] in tg:
                site_stats['reach'] += 1
                reached.append((h, call, at, txt, tg))
                state.add('reach')
            else:
                site_stats['excluded'] += 1
                state.add('excluded')
        if not mine:
            continue
        if 'reach' in state:
            fn_class['reached'].append(h)
        elif state == {'excluded'}:
            fn_class['excluded'].append(h)
        elif 'excluded' in state:
            fn_class['partly'].append(h)
        else:
            fn_class['unresolved'].append(h)
    print(f'\n6. the {len(slot_only)} functions read only through vtable offsets, per call site: {site_stats["sites"]} reading '
          f'call sites; receiver resolved (the caller\'s this, its classes known) {site_stats["sites"] - site_stats["unresolved"]}: '
          f'{site_stats["excluded"]} cannot reach the function, {site_stats["reach"]} can; unresolved {site_stats["unresolved"]} '
          f'({", ".join(f"{k} {v}" for k, v in sorted(unresolved_kinds.items(), key=lambda x: -x[1]))})')
    print(f'   functions: every reading site excluded {len(fn_class["excluded"])}, some excluded and the rest unresolved '
          f'{len(fn_class["partly"])}, none resolved {len(fn_class["unresolved"])}, a resolved site reaches it '
          f'{len(fn_class["reached"])}')
    for h, call, at, txt, tg in reached:
        print(f'   reaches: {h["address"]:#x} {h["replacement"]} [{h["module"]}] declared {h["ret"]!r}: call {call:#x} '
              f'(in {code.func_of.get(call, 0):#x}) reads at {at:#x} {txt}; the site\'s targets '
              f'{", ".join(hex(t) for t in sorted(tg)[:8])}{" …" if len(tg) > 8 else ""}')

    if args.no_sources:
        return 0
    calls, vcalls = typed_calls(args.jobs)
    known = [c for c in calls if c[2] in by_addr]
    mism = []
    for fname, line, addr, rt, use, rt_raw, encl, via in known:
        h = by_addr[addr]
        if h['provides'] == '?' or h['replacement'] in asm:
            continue
        want = classify(rt)
        if want == 'eax' and classify(rt_raw) != 'eax':
            want = classify(rt_raw)
        if use in ('used al', 'used ax') and want not in ('st0', 'none'):
            want = use[5:]
        if use == 'used hi':
            want = 'eax'     # the upper part only: a leftover the reimplementation passes on (listed apart)
        have = h['provides']
        bad = False
        if want == 'st0':
            bad = have != 'st0'          # the caller pops st(0): an x87 stack the callee did not push
        elif WIDTH.get(want, 0) > 0:
            bad = have == 'st0' or WIDTH.get(have, 0) < WIDTH[want]
        if bad:
            mism.append((fname, line, addr, rt, use, h, encl, via))
    by_use = defaultdict(int)
    for x in mism:
        by_use[x[4]] += 1
    print(f'\nthe reimplementation\'s typed calls of exe addresses (fn<>, original<>, exe_or_dead<>): {len(calls)}, of '
          f'a hooked function {len(known)}; expecting more than the hooked function declares: {len(mism)} '
          f'({", ".join(f"{k} {v}" for k, v in sorted(by_use.items()))})')
    for fname, line, addr, rt, use, h, encl, via in sorted(mism, key=lambda x: (x[4] in ('discarded', 'returned, unused', 'returned, escapes', 'used hi'), x[2], x[0], x[1])):
        if use in ('discarded', 'returned, unused', 'returned, escapes', 'used hi') and not args.verbose:
            continue
        print(f'  {fname}:{line} {addr:#x} expects {rt!r} ({use}), {h["replacement"]} [{h["module"]}] declares '
              f'{h["ret"]!r}' + (f'; {encl[1] if isinstance(encl, tuple) else encl} used at {", ".join(via[:6])}'
                                  if via else ''))
    # 7. the reimplementation's vcall<R>(obj, slot) sites against the original's call at that slot and the overrides
    hook_at = {}
    for h in hooks:
        if h['replacement']:
            hook_at.setdefault((h.get('src'), h['replacement']), h['address'])

    def hooked_address(fname, name):
        return hook_at.get((Path(fname).name, name)) if name else None

    def is_production(fname):
        n = Path(fname).name
        return n.endswith('.cpp') and not n.startswith('checks') and n not in ('main.cpp', 'check_util.cpp')

    def reading_callers(a):
        """The original's callers of a that read eax after the call: direct calls and jumps, and vtable calls whose
        receiver resolves to classes holding a at that offset ([] when none; unresolved vtable calls not counted)."""
        out = []

        def part(c):
            if any(x[0] in ('read', 'jmp?') for x in eax_use_after(code, c, NEEDS['al'])):
                return 'beyond al'
            if any(x[0] in ('read', 'jmp?') for x in eax_use_after(code, c, NEEDS['none'])):
                return 'al only'
            return None
        for c in direct.get(a, []) + jumps.get(a, []):
            if part(c):
                out.append(f'direct caller {c.address:#x} ({part(c)})')
        for r in refs.get(a, []):
            vs = vtable_slot(r)
            if not vs:
                continue
            for c in indirect.get(vs[1], []):
                if not part(c):
                    continue
                tg, _ = rv.targets(c)
                if tg is not None and a in tg:
                    out.append(f'vtable caller {c.address:#x} (receiver resolved, {part(c)})')
        return sorted(set(out))

    vstats = defaultdict(int)
    vflags = []
    for fname, line, slot, rt, use, encl_name in vcalls:
        if not is_production(fname) or slot is None:
            continue
        want = classify(return_type(rt) if '(' in rt else rt)
        if use in ('used al', 'used ax') and want not in ('st0', 'none'):
            want = use[5:]   # only the low byte or word is read (a cast or a mask)
        vstats['sites'] += 1
        if use == 'used hi':
            vstats['value not used'] += 1   # the upper part kept for the own eax: a leftover passed on (part 8)
            continue
        if want == 'none' or use in ('discarded', 'returned, unused'):
            vstats['value not used'] += 1
            continue
        vstats['value used'] += 1
        a = hooked_address(fname, encl_name)
        if a is None:
            vstats['helper'] += 1
            continue
        sites = [c for c in indirect.get(slot * 4, []) if code.func_of.get(c.address) == a]
        if not sites:
            vstats['the original function has no call at that offset'] += 1
            continue
        if want == 'st0':
            reads = sites   # an x87 result: the original's use of st(0) is not followed; its overrides are compared
        else:               # (a jump through a table counts as a possible read)
            reads = [c for c in sites if any(x[0] in ('read', 'ret', 'jmp?') for x in eax_use_after(code, c, NEEDS['none']))]
        if not reads:
            vstats['the original ignores the value'] += 1
            vflags.append((fname, line, slot, rt, use, encl_name, a, 'the original\'s call at that offset reads no eax'))
            continue
        vstats['the original reads it too'] += 1
        for c in reads:
            tg, recv = rv.targets(c)
            if tg is None:
                vstats['  receiver unresolved'] += 1
                continue
            vstats['  receiver resolved'] += 1
            for t in tg:
                h2 = by_addr.get(t)
                if h2 is None or h2['provides'] == '?' or h2['replacement'] in asm:
                    continue
                have = h2['provides']
                if (want == 'st0') != (have == 'st0') or (want != 'st0' and WIDTH.get(have, 0) < WIDTH.get(want, 0)):
                    vflags.append((fname, line, slot, rt, use, encl_name, a,
                                   f'override {t:#x} {h2["replacement"]} declares {h2["ret"]!r} (call {c.address:#x})'))
    print(f'\n7. vcall<R>(obj, slot) sites of the production sources: {vstats["sites"]}; the value not used (void, '
          f'discarded or returned unused) {vstats["value not used"]}, used {vstats["value used"]}: in a helper, not a hooked '
          f'function\'s own body {vstats["helper"]}; the original function has no call '
          f'at that offset {vstats["the original function has no call at that offset"]}; the original\'s call reads eax too '
          f'{vstats["the original reads it too"]} (receiver resolved at {vstats["  receiver resolved"]} of their reading '
          f'calls, unresolved {vstats["  receiver unresolved"]}); the original ignores the value '
          f'{vstats["the original ignores the value"]}; flagged {len(vflags)}')
    for fname, line, slot, rt, use, encl_name, a, why in sorted(set(vflags)):
        rc = reading_callers(a)
        print(f'  {fname}:{line} vcall<{rt}> slot {slot} ({use}) in {encl_name} ({a:#x}): {why}; the original\'s callers '
              f'of {a:#x} reading eax: {", ".join(rc) or "none found"}')

    # 8. the typed calls that pass a void callee's leftover eax on (part 5's "used"): do the original's callers of the
    #    hooked function that ends with it read eax (direct calls, and vtable calls whose receiver resolves to it)?
    def name_of(k):
        if isinstance(k, tuple):
            return k[1]
        m = re.match(r'\?([A-Za-z_][A-Za-z0-9_]*)@', k or '')
        return m.group(1) if m else None
    hooked_names = {}
    for h in hooks:
        hooked_names.setdefault(h['replacement'], []).append(h['address'])
    left = [x for x in mism if x[4].startswith('used') or x[4] == 'returned, used']
    observed, unmapped, checked = [], 0, 0
    for fname, line, addr, rt, use, h, encl, via in left:
        n = name_of(encl)
        cand = [a for a in hooked_names.get(n, []) if by_addr[a].get('src') == Path(fname).name]
        if not cand:
            unmapped += 1
            continue
        checked += 1
        for a in cand:
            for why in reading_callers(a):
                observed.append((fname, line, a, why))
    print(f'\n8. typed calls passing a void callee\'s leftover eax on ("used"): {len(left)}; in a hooked function\'s own body '
          f'{checked} (the rest in helpers: {unmapped}); of those, an original caller of that function reads eax: '
          f'{len(observed)}')
    for fname, line, a, why in observed:
        print(f'  {fname}:{line} in {a:#x}: {why}')

    if args.verbose:
        print('\nonly vtable-offset reads:')
        for h in slot_only:
            print(f'  {h["address"]:#x} {h["replacement"]} ({h["provides"]}) slots '
                  f'{", ".join(hex(s) for s in h["slots"])}: {len(h["slot_reads"])} sites')
    return 0


if __name__ == '__main__':
    sys.exit(main())
