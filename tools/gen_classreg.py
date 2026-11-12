"""Generate the reimplementations of the startup class registrations and the class info getters (batch C1 of
phase 5a, docs/DECOMPILATION.md section 10, "Batch C1").

    .venv/bin/python -I tools/gen_classreg.py [--check]

Reads the functions from docs/exe_symbols.csv (the rows of the phase 5a map "startup class registration" and
"class info getter with the registration inlined") and their code from Zax.exe build 1308 (tools/game_paths.py), and writes:

- reimpl/src/classreg_<a-d>.cpp: the 16-byte registration stubs (`push getter; mov ecx, object; call 0x417690;
  ret`), split into four modules (the check harness's module holds at most 160 hooks);
- reimpl/src/classreg_vars.h: one inline function per class variable that the getters register (the inlined
  getter: the class info allocated from the pool, the parent's getter called, ClassInfo_Register), with the
  parameters read from the code; every copy of a class variable's block must have the same parameters;
- reimpl/src/classreg_getters.cpp: the getters whose code is nothing but such blocks (also nested: a parent's
  getter inlined into the child's) and `return [var]`, and the two whose code continues into the class
  registry (ClassRegistry_Add(ClassRegistry_Get(), class, 0), the registration stub with the getter inlined);
- reimpl/src/checks_classreg_<a-d>.cpp and checks_classreg_getters.cpp: their checks (the toolkit's harness,
  checks_ui_common.h);
- with --check nothing is written; the classification is printed.

A getter whose code does more (a virtual slot that only begins with a getter, a static initializer that also
stores a field descriptor, ...) is printed as "manual" and written by hand in reimpl/src/classreg_slots.cpp.

How a block is recognised: the instructions are executed symbolically along the path "class variable 0,
allocation not null" (pushes, loads of globals, calls of 0x417710, of ClassInfo_Register 0x415780 and of
other functions); every ClassInfo_Register call gives one block {var, name, version, parent, size, abstract,
create, create array, register fields}, its parent either an immediate, the result of a called getter or the
load of an earlier block's variable (the inlined parent). The branches are then checked: every block's
`jne` (variable set) and `je` (allocation failed) must go past its ClassInfo_Register call.
"""
from __future__ import annotations

import argparse
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

REGISTER = 0x415780
NEW = 0x417710
STATIC_REGISTER = 0x417690
REGISTRY_GET = 0x415d20
REGISTRY_ADD = 0x415a20
SRC = ROOT / 'reimpl/src'
STUB_MODULES = 'abcd'


# The hand-written functions of reimpl/src/classreg_slots.cpp (their hook table classreg_slots_hooks.inc is
# written from this list)
MANUAL = {
    0x41f880: 'OtherMapWrapper_Describe', 0x41fa50: 'ThisMapWrapper_Describe',
    0x483ee0: 'StaticInit_ExtraEntry_6bdcb1', 0x48dc50: 'StaticInit_ExtraEntry_6bdd5a',
    0x4a8660: 'StaticInit_PaletteHolderField', 0x4a86f0: 'StaticInit_ExtraEntry_6be7cc',
    0x4b8300: 'SingleActionWrapper_Describe', 0x4c80f0: 'MultipleActionsBaseWrapper_Describe',
    0x4d4410: 'StaticInit_HueRemapField', 0x4f95d0: 'StaticInit_ExtraEntry_6c29ac',
    0x4f9680: 'StaticInit_ExtraEntry_6c29b4', 0x4f96f0: 'StaticInit_ChildFieldClass',
    0x4fca00: 'StaticInit_SubAIField', 0x4fca90: 'StaticInit_ExtraEntry_6c29ec',
    0x4fe160: 'LevelOfDifficultyList_SetObjectValue', 0x521d20: 'StaticInit_ExtraEntry_6d00f9',
    0x521d90: 'StaticInit_ExtraEntry_6d0120', 0x521e00: 'StaticInit_ExtraEntry_6d0101',
    0x560660: 'StaticInit_CustomRulesListClass', 0x598280: 'WaterWarpEffect_FilePath',
    0x59fca0: 'ZaxApp_RulesPair', 0x59fd80: 'ZaxApp_RulesFloat', 0x59fe70: 'ZaxApp_RulesColor',
    0x5b4590: 'StaticInit_ExtraEntry_71437a',
}


def rows():
    stubs, getters = [], []
    with open(ROOT / 'docs/exe_symbols.csv', newline='') as f:
        for r in csv.reader(f):
            if not r or not r[0].startswith('0x') or len(r) < 6:
                continue
            m = re.search(r'phase 5a map: (\d+) bytes', r[5]) or re.search(r'(\d+) bytes', r[5])
            # the phase 5a map's notes, or (since C1 rewrote them) the notes naming the generated modules; the
            # sizes stay in the notes' "phase 5a map: N bytes"
            if 'startup class registration (push getter' in r[5] or re.match(
                    r'reimpl/src/classreg_[a-d]\.cpp \(batch C1, DECOMPILATION\.md 10: startup class registration of ', r[5]):
                stubs.append((int(r[0], 16), r[1], int(m.group(1))))
            elif 'class info getter with the registration inlined' in r[5] or re.match(
                    r'reimpl/src/classreg_(getters|slots)\.cpp \(batch C1, ', r[5]):
                getters.append((int(r[0], 16), r[1], int(m.group(1))))
    return stubs, getters


def hook_entry(exe: Exe, a: int, name: str, repl: str) -> str:
    """A hook table entry: the whole instructions covering 5 bytes, the first 8 bytes, a relative call in them."""
    o = exe.off(a)
    n, call_at = 0, 0
    for i in exe.md.disasm(exe.data[o:o + 32], a):
        if n >= 5:
            break
        if i.mnemonic == 'call' and i.bytes[0] == 0xe8:
            call_at = n + 1
        elif i.mnemonic.startswith('j') or i.mnemonic in ('ret', 'call'):
            raise SystemExit(f'{a:#x}: {i.mnemonic} in the first five bytes')
        n += i.size
    exp = ', '.join(f'0x{b:02x}' for b in exe.data[o:o + 8])
    return f'        {{{a:#x}, ptr(&{repl}), "{name}", {n}, {{{exp}}}' + (f', {call_at}}},' if call_at else '},')


# ---- The registration stubs ------------------------------------------------------------------------------------

def parse_stub(exe: Exe, a: int):
    b = exe.data[exe.off(a):exe.off(a) + 16]
    if b[0] != 0x68 or b[5] != 0xb9 or b[10] != 0xe8 or b[15] != 0xc3:
        return None
    getter, obj = struct.unpack_from('<II', b, 1)[0], struct.unpack_from('<I', b, 6)[0]
    target = a + 15 + struct.unpack_from('<i', b, 11)[0]
    if target != STATIC_REGISTER:
        return None
    return getter, obj


# ---- The getters -----------------------------------------------------------------------------------------------

class Block:
    def __init__(self, var, args, call_at, new_index):
        self.var = var
        self.new_index = new_index   # index of its operator_new_ecx call in the instructions
        (_, self.name, self.version, self.parent, self.size, self.abstract, self.create, self.create_array,
         self.register_fields) = args
        self.call_at = call_at   # address of its ClassInfo_Register call

    def key(self):
        return (self.name, self.version, self.parent, self.size, self.abstract, self.create, self.create_array,
                self.register_fields)


def run(exe: Exe, a: int, size: int):
    """Symbolic straight-line run (variable 0, allocations not null) up to the first ret.

    Returns (blocks, tail, insns): tail is a list of the calls after the last block and what eax is at the ret."""
    o = exe.off(a)
    insns = list(exe.md.disasm(exe.data[o:o + size], a))
    regs = {'eax': ('?',), 'ecx': ('?',), 'edx': ('?',)}
    stack = []
    blocks = []
    events = []   # calls and stores in order
    opened = []   # indices of the operator_new_ecx calls whose block is not closed yet
    for idx, i in enumerate(insns):
        m, ops = i.mnemonic, i.op_str
        if m == 'push':
            if ops in ('esi', 'edi', 'ebx', 'ebp'):
                stack.append(('save', ops))
            elif ops in regs:
                stack.append(regs[ops])
            elif ops.startswith('0x') or ops.lstrip('-').isdigit():
                stack.append(('imm', int(ops, 0) & 0xffffffff))
            else:
                return None, f'push {ops}', insns
        elif m == 'pop':
            if not stack or stack[-1][0] != 'save':
                return None, 'pop of an argument', insns
            stack.pop()
        elif m == 'mov':
            d, s = [x.strip() for x in ops.split(',', 1)]
            mm = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', s)
            if mm:
                val = ('load', int(mm.group(1), 16))
            elif s.startswith('0x') or s.isdigit():
                val = ('imm', int(s, 0))
            elif s in regs:
                val = regs[s]
            elif s in ('esi', 'edi', 'ebx', 'ebp'):
                val = regs.get(s, ('?',))
            else:
                return None, f'mov {ops}', insns
            if d.startswith('dword ptr'):
                return None, f'store {ops}', insns
            regs[d] = val
        elif m == 'test':
            pass
        elif m in ('jne', 'je'):
            pass
        elif m == 'call':
            if not ops.startswith('0x'):
                return None, f'call {ops}', insns
            t = int(ops, 16)
            if t == NEW:
                regs['eax'] = ('new', regs['ecx'])
                opened.append(idx)
            elif t == REGISTER:
                args = []
                while len(args) < 9:
                    if not stack:
                        return None, 'stack', insns
                    v = stack.pop()
                    if v[0] == 'save':
                        return None, 'saved register as argument', insns
                    args.append(v)
                var = args[0]
                if var[0] != 'imm':
                    return None, 'var', insns
                if not opened:
                    return None, 'register without new', insns
                blocks.append(Block(var[1], args, i.address, opened.pop()))
                regs['eax'] = ('?',)
            else:
                events.append(('call', t, regs['ecx'], i.address))
                regs['eax'] = ('call', t)
            regs['ecx'] = regs['edx'] = ('?',)
        elif m == 'ret':
            return blocks, (events, regs['eax'], ops, stack), insns
        else:
            return None, f'{m} {ops}', insns
    return None, 'no ret', insns


def harvest(exe: Exe, a: int, size: int):
    """The complete registration blocks in a function the generator does not write (the hand-written ones use
    the same per-variable functions): a lenient linear run over the whole code; an unknown instruction makes its
    destination register unknown, a pop or `add esp` drops arguments, a ret starts afresh."""
    o = exe.off(a)
    insns = list(exe.md.disasm(exe.data[o:o + size], a))
    regs = {}
    stack, opened, blocks = [], [], []
    for idx, i in enumerate(insns):
        m, ops = i.mnemonic, i.op_str
        parts = [x.strip() for x in ops.split(',', 1)] if ops else []
        if m == 'push':
            if ops.startswith('0x') or ops.lstrip('-').isdigit():
                stack.append(('imm', int(ops, 0) & 0xffffffff))
            else:
                stack.append(regs.get(ops, ('?',)))
        elif m == 'pop':
            if stack:
                stack.pop()
            regs[ops] = ('?',)
        elif m == 'add' and parts[0] == 'esp':
            for _ in range(int(parts[1], 0) // 4):
                if stack:
                    stack.pop()
        elif m == 'mov' and not parts[0].startswith(('dword', 'word', 'byte')):
            mm = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', parts[1])
            if mm:
                regs[parts[0]] = ('load', int(mm.group(1), 16))
            elif parts[1].startswith('0x') or parts[1].isdigit():
                regs[parts[0]] = ('imm', int(parts[1], 0))
            elif parts[1] in regs:
                regs[parts[0]] = regs[parts[1]]
            else:
                regs[parts[0]] = ('?',)
        elif m == 'call' and ops.startswith('0x'):
            t = int(ops, 16)
            if t == NEW:
                regs['eax'] = ('new',)
                opened.append(idx)
            elif t == REGISTER and len(stack) >= 9 and opened:
                args = [stack.pop() for _ in range(9)]
                if args[0][0] == 'imm' and all(x[0] == 'imm' for k, x in enumerate(args) if k != 3):
                    blocks.append(Block(args[0][1], args, i.address, opened.pop()))
                regs['eax'] = ('?',)
            else:
                regs['eax'] = ('call', t)
            regs['ecx'] = regs['edx'] = ('?',)
        elif m == 'call':
            regs['eax'] = regs['ecx'] = regs['edx'] = ('?',)
        elif m == 'ret':
            stack, opened, regs = [], [], {}
        elif parts and parts[0] in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp') and m not in ('test', 'cmp'):
            regs[parts[0]] = ('?',)
    return blocks


def check_branches(blocks, insns):
    """Every block's jne/je skip its ClassInfo_Register call (they go past it)."""
    addrs = [i.address for i in insns]
    for b in blocks:
        # the load of the variable that starts the block: the last `mov eax, [var]` before its new
        k = b.new_index
        jumps = [i for i in insns[max(0, k - 8):k + 6] if i.mnemonic in ('je', 'jne')]
        if len(jumps) < 2:
            return False
        for j in jumps[-2:]:
            t = int(j.op_str, 16)
            if t <= b.call_at or t not in addrs:
                return False
    return True


def classify(exe: Exe, a: int, size: int):
    blocks, tail, insns = run(exe, a, size)
    if blocks is None:
        return 'manual', tail, None
    if not blocks or not check_branches(blocks, insns):
        return 'manual', 'branches', None
    events, eax, ret_ops, stack = tail
    outer = blocks[-1].var
    events = [e for e in events if e[3] > blocks[-1].call_at]   # the parents' getters are part of the blocks
    # parents: an inlined parent must be an earlier block
    for b in blocks:
        p = b.parent
        if p[0] == 'load' and p[1] not in [x.var for x in blocks]:
            return 'manual', f'parent load {p[1]:#x}', None
        if p[0] not in ('imm', 'load', 'call'):
            return 'manual', f'parent {p}', None
    if ret_ops:
        return 'manual', 'ret n', None
    after = [i for i in insns if i.address > blocks[-1].call_at]
    if not events and eax == ('load', outer):
        # the rest of the code: only exits that return [outer]
        for i in after:
            if i.mnemonic == 'mov' and i.op_str == f'eax, dword ptr [{outer:#x}]':
                continue
            if i.mnemonic in ('pop', 'ret') and i.op_str in ('esi', 'edi', 'ebx', 'ebp', ''):
                continue
            return 'manual', f'after: {i.mnemonic} {i.op_str}', None
        return 'getter', blocks, None
    if [e[1] for e in events] == [REGISTRY_GET, REGISTRY_ADD] and len(blocks) == 1:
        return 'static_register', blocks, None
    return 'manual', 'tail ' + ', '.join(hex(e[1]) for e in events), None


# ---- Writing ---------------------------------------------------------------------------------------------------

HEADER = '// Generated by tools/gen_classreg.py from Zax.exe build 1308 (SHA-1 a918c784) (docs/DECOMPILATION.md 10, "Batch C1"); do not edit.\n'


def c_ident(name: str) -> str:
    return re.sub(r'[^A-Za-z0-9_]', '_', name)


def parent_expr(p, getter_names):
    if p[0] == 'imm':
        return f'konst<{p[1]:#x}>'
    if p[0] == 'call':
        return f'exe_getter<{p[1]:#x}>'
    return f'reg_{p[1]:x}'


def var_function(exe: Exe, b: Block) -> str:
    name = exe.cstr(b.name[1]) or '?'
    params = (f'{b.var:#x}, {b.name[1]:#x}, {b.version[1]:#x}, {parent_expr(b.parent, None)}, {b.size[1]:#x}, '
              f'{b.abstract[1]:#x}, {b.create[1]:#x}, {b.create_array[1]:#x}, {b.register_fields[1]:#x}')
    return (f'// {name}\ninline DWORD __cdecl reg_{b.var:x}() {{ return class_reg({params}); }}\n'
            f'inline DWORD reg_{b.var:x}_ran(bool &ran) {{ return class_reg({params}, &ran); }}\n')


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    exe = Exe(Path(game_paths.exe_arg()))
    stubs, getters = rows()

    # Stubs
    parsed = []
    for a, name, size in stubs:
        p = parse_stub(exe, a)
        if p is None or size != 16:
            raise SystemExit(f'stub {a:#x} {name}: not the 16-byte pattern')
        cls = registered_class(exe, p[0])
        parsed.append((a, f'StaticRegister_{cls}' if cls else name, *p))
    names = [n for _, n, _, _ in parsed]
    if len(set(names)) != len(names):
        dup = sorted({n for n in names if names.count(n) > 1})
        raise SystemExit(f'duplicate stub names: {dup}')

    # Getters
    kinds = {}
    var_blocks: dict[int, Block] = {}
    manual = []
    for a, name, size in getters:
        k, info, _ = classify(exe, a, size)
        kinds[a] = (k, info, name, size)
        if k == 'manual':
            manual.append((a, name, size, info))
        for b in (harvest(exe, a, size) if k == 'manual' else info):
            old = var_blocks.get(b.var)
            if old and old.key() != b.key():
                raise SystemExit(f'{a:#x}: class variable {b.var:#x} registered with other parameters than at {old.call_at:#x}')
            var_blocks.setdefault(b.var, b)
    print(f'stubs {len(parsed)}, getters {sum(1 for v in kinds.values() if v[0] == "getter")}, '
          f'static registrations {sum(1 for v in kinds.values() if v[0] == "static_register")}, manual {len(manual)}')
    for a, name, size, info in manual:
        print(f'  manual {a:#x} {name} ({size} bytes): {info}')
    if sorted(MANUAL) != sorted(a for a, _, _, _ in manual):
        raise SystemExit('the hand-written list MANUAL does not match the manual functions')
    if args.check:
        return
    with open(SRC / 'classreg_slots_hooks.inc', 'w') as f:
        f.write(HEADER)
        for a in sorted(MANUAL):
            f.write(hook_entry(exe, a, MANUAL[a], MANUAL[a]) + '\n')
    write_stubs(exe, parsed)
    write_getters(exe, getters, kinds, var_blocks)


def registered_class(exe: Exe, getter: int) -> str | None:
    """The class a getter registers: the name of its last ClassInfo_Register block (its code up to the first ret)."""
    blocks, _, _ = run(exe, getter, 1200)
    return exe.cstr(blocks[-1].name[1]) if blocks else None


def write_stubs(exe: Exe, parsed):
    n = len(parsed)
    per = (n + len(STUB_MODULES) - 1) // len(STUB_MODULES)
    for mi, letter in enumerate(STUB_MODULES):
        part = parsed[mi * per:(mi + 1) * per]
        if not part:
            continue
        mod = f'classreg_{letter}'
        lo, hi = part[0][0], part[-1][0]
        out = [HEADER,
               f'// The startup class registrations {lo:#x}-{hi:#x} ({len(part)} of {n}): entries of the C runtime\'s initializer\n'
               '// table, each `push getter; mov ecx, object; call ClassInfo_StaticRegister 0x417690; ret` (16 bytes). They run\n'
               '// before WinMain through the hooks installed in DllMain; eax is 0x417690\'s (the object). The object is a byte of\n'
               '// .data that only serves as `this`; the getter is called through its exe address.\n',
               '#include "classreg_common.h"\n\nnamespace zax {\n\nnamespace {\n']
        for a, name, getter, obj in part:
            out.append(f'DWORD __cdecl {c_ident(name)}() {{ return static_register({obj:#x}, {getter:#x}); }}   // {a:#x}\n')
        out.append('\n}  // namespace\n\n')
        out.append(f'ZAX_HOOKS({mod})\n{{\n    const Hook list[] = {{\n')
        for a, name, getter, obj in part:
            out.append(hook_entry(exe, a, name, c_ident(name)) + '\n')
        out.append('    };\n    return emit_hooks(out, list);\n}\n\n}  // namespace zax\n')
        (SRC / f'{mod}.cpp').write_text(''.join(out))

        chk = [HEADER,
               f'// Checks of the startup class registrations of {mod}.cpp against the original (toolkit harness, checks_ui_common.h):\n'
               '// ClassInfo_StaticRegister 0x417690 a recording stand-in (its this and the getter it is given compared), eax\n'
               '// compared. Nothing else is called, so nothing of the game changes.\n',
               '#include "checks_classreg_common.h"\n\nnamespace zax {\n\n',
               f'int hooks_{mod}(Hook *out);\n\nnamespace {{\n\nusing namespace uic;\n\n',
               f'constinit ModuleHooks g_hooks{{"{mod}", &hooks_{mod}}};\n\n',
               '// constinit: the table must not need a dynamic initialiser (the DLL has no C runtime)\n'
               '#define T(name, addr) {name, addr, CALL_CDECL, 0, ~0u, 2, classreg_base, STATIC_REGISTER_STUBS, NSTATIC_REGISTER_STUBS}\n',
               'constinit const Test TESTS[] = {\n']
        for a, name, getter, obj in part:
            chk.append(f'    T("{name}", {a:#x}),\n')
        chk.append('};\n#undef T\n\n}  // namespace\n\n')
        chk.append(f'ZAX_CHECKS({mod})\n{{\n    (void)cls;\n    (void)n;\n'
                   f'    Tally t{{"class registration {letter}: the startup registrations {lo:#x}-{hi:#x}"}};\n'
                   f'    run_tests(t, g_hooks, nullptr, TESTS, sizeof TESTS / sizeof TESTS[0], &classreg_base, {52000 + mi * 1000});\n'
                   '    t.report();\n}\n\n}  // namespace zax\n')
        (SRC / f'checks_{mod}.cpp').write_text(''.join(chk))


def getter_name(exe: Exe, a: int, kind: str, blocks) -> str:
    cls = exe.cstr(blocks[-1].name[1])
    return f'{cls}_ClassInfo' if kind == 'getter' else f'StaticRegister_{cls}'


def write_getters(exe: Exe, getters, kinds, var_blocks):
    # the variables in the order of their first use, parents first
    order = []

    def need(v):
        if v in order:
            return
        b = var_blocks[v]
        if b.parent[0] == 'load':
            need(b.parent[1])
        order.append(v)

    for v in sorted(var_blocks):
        need(v)
    h = [HEADER,
         '// The class registrations the getters inline, one function per class variable: if the variable is 0, a class\n'
         '// info from the pool (0x48 bytes), the parent class (its getter, an inlined one of these, or an immediate) and\n'
         '// ClassInfo_Register (version as its float bits); eax the variable. Parameters read from the exe.\n',
         '#pragma once\n#include "classreg_common.h"\n\nnamespace zax::classreg {\n\n']
    for v in order:
        h.append(var_function(exe, var_blocks[v]) + '\n')
    h.append('}  // namespace zax::classreg\n')
    (SRC / 'classreg_vars.h').write_text(''.join(h))

    done = [(a, *kinds[a]) for a, _, _ in getters if kinds[a][0] != 'manual']
    out = [HEADER,
           '// The class info getters whose code is only the inlined registration of their class (and of parents inlined\n'
           '// into them): eax the class variable. Some are the slot 2 (GetClassInfo) of a vtable, called with ecx = the\n'
           '// object, which they ignore. The two static registrations with the getter inlined (CObject, the\n'
           '// "CClassThatCanNotBeConstructedOrDerivedFrom" class) add the class to the class registry after it:\n'
           '// ClassRegistry_Add(ClassRegistry_Get(), class, 0); eax ClassRegistry_Add\'s.\n',
           '#include "classreg_vars.h"\n\nnamespace zax {\n\nnamespace {\n\nusing namespace classreg;\n\n']
    fnames = {}
    base_names = [getter_name(exe, a, kind, blocks) for a, kind, blocks, _, _ in done]
    for (a, kind, blocks, oldname, size), fn in zip(done, base_names):
        fnames[a] = fn if base_names.count(fn) == 1 else f'{fn}_{a:x}'
        if kind == 'getter':
            out.append(f'DWORD __cdecl {fnames[a]}() {{ return reg_{blocks[-1].var:x}(); }}\n')
        else:
            out.append(f'DWORD __cdecl {fnames[a]}() {{ return registry_add(reg_{blocks[-1].var:x}()); }}\n')
    out.append('\n}  // namespace\n\nZAX_HOOKS(classreg_getters)\n{\n    const Hook list[] = {\n')
    for a, kind, blocks, oldname, size in done:
        out.append(hook_entry(exe, a, fnames[a], fnames[a]) + '\n')
    out.append('    };\n    return emit_hooks(out, list);\n}\n\n}  // namespace zax\n')
    (SRC / 'classreg_getters.cpp').write_text(''.join(out))

    # checks
    chk = [HEADER,
           '// Checks of the generated class info getters (classreg_getters.cpp) against the original (toolkit harness,\n'
           '// checks_ui_common.h): operator_new_ecx, ClassInfo_Register, the parents\' getters and the class registry are\n'
           '// recording stand-ins (ClassInfo_Register writes its object into the variable and logs its last five\n'
           '// arguments); the class variables of every block are saved, set per case (0 or a value) and compared.\n',
           '#include "checks_classreg_common.h"\n\nnamespace zax {\n\nint hooks_classreg_getters(Hook *out);\n\nnamespace {\n\n'
           'using namespace uic;\n\nconstinit ModuleHooks g_hooks{"classreg_getters", &hooks_classreg_getters};\n\n']
    parents = sorted({b.parent[1] for a, kind, blocks, _, _ in done for b in blocks if b.parent[0] == 'call'})
    chk.append('using namespace classreg;\n\nconstinit const Stub STUBS[] = {\n'
               '    {0x417710, CV_FAST, 0, K_VALUE, "operator_new_ecx", "vx"},\n'
               '    {0x415780, CV_THIS, 9, K_VALUE, "ClassInfo_Register", "vvvv"},\n'
               '    {0x415d20, CV_CDECL, 0, K_VALUE, "ClassRegistry_Get"},\n'
               '    {0x415a20, CV_THIS, 2, K_VALUE, "ClassRegistry_Add", "vv"},\n')
    for t in parents:
        chk.append(f'    {{{t:#x}, CV_CDECL, 0, K_VALUE, "getter {t:#x}"}},\n')
    chk.append('};\n\nconstinit const VarTest TESTS[] = {\n')
    for a, kind, blocks, oldname, size in done:
        vs = []
        for b in blocks:
            if b.var not in vs:
                vs.append(b.var)
        vlist = ', '.join(f'{v:#x}' for v in vs)
        chk.append(f'    {{"{fnames[a]}", {a:#x}, {{{vlist}}}, {len(vs)}}},\n')
    chk.append('};\n\n}  // namespace\n\n')
    chk.append('ZAX_CHECKS(classreg_getters)\n{\n    (void)cls;\n    (void)n;\n'
               '    Tally t{"class registration: the class info getters"};\n'
               '    run_var_tests(t, g_hooks, TESTS, sizeof TESTS / sizeof TESTS[0], STUBS, sizeof STUBS / sizeof STUBS[0], 12, 57000);\n'
               '    t.report();\n}\n\n}  // namespace zax\n')
    (SRC / 'checks_classreg_getters.cpp').write_text(''.join(chk))

    # the names for the docs
    with open(ROOT / 'out/classreg_names.csv', 'w', newline='') as f:
        w = csv.writer(f)
        for a, kind, blocks, oldname, size in done:
            w.writerow([f'{a:#x}', fnames[a], oldname, kind, size])


if __name__ == '__main__':
    main()
