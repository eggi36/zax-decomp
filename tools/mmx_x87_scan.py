"""Find x87 instructions the reimplementation can run while the MMX state is set (docs/DECOMPILATION.md 13,
"Batch analyses 2026-11-01", 2).

After an MMX instruction all eight x87 tags are valid and the stack top is 0 until `emms`. An x87 load then
overflows the stack and loads the indefinite NaN on real x86 (the ARM VMs' emulation loads the value anyway), so
an x87 instruction between MMX code and the next `emms` gives a different result on real hardware. The MMX span
tables T18, T20 and T22 end without `emms` (spans_mmx.h, "MMX state"), and so do the drawers and some blitters that
call them, as in the original. 2026-10-31's `warp_blit` bug was such a case: clang redid a spilled 80-bit compare
after the rows' MMX spans, where the original compares once before any drawing.

Usage (the objects of a build, kept with ZAX_KEEP_TMP, an absolute path, see reimpl/build.sh):
    ZAX_KEEP_TMP=$PWD/out/build-dll reimpl/build.sh
    .venv/bin/python -I tools/mmx_x87_scan.py 'gamedata/Zax - German/Zax.exe' out/build-dll
    ZAX_EXE=... ZAX_KEEP_TMP=$PWD/out/build-exe reimpl/build.sh --exe
    .venv/bin/python -I tools/mmx_x87_scan.py 'gamedata/Zax - German/Zax.exe' out/build-exe \\
        --fn-layer out/build-exe/addr_layer.inc --risky blit_rest4b.o,rle16.o,spans_mmx.o,spans_mmx_opacity.o
Options: -v lists the functions that may return with the MMX state set; --no-tables leaves out step 5 below.

The analysis (whole program, over every COFF object of the build; Zax.exe is read for the span tables and the frame
vtables only):
1. Functions: the function symbols of the code sections (COMDAT sections included), disassembled with capstone.
2. The hooks (exe address -> own function): the records of every `hooks_<module>` (ZAX_HOOKS): static records in
   .rdata ({address, function relocation, name string relocation}) and records the function builds on the stack
   (entries with ptr(&f)).
3. Call targets: direct calls (relocations, offsets in the same section), calls of exe addresses (`mov reg, imm;
   call reg`, every `mov reg, imm` reaching the call; the standalone exe's `call [zax_fn_table + 4i]`) through
   the hooks, own function pointers (`mov reg, offset f`, `cmov`), and calls through the span tables T0-T35
   (`call [table + 4n]`, the standalone exe's read of its own copy `_zax_block_XXXXXX` followed by `call reg`):
   every hooked entry of the table, the exe's static entries plus the generic loop CBlendTransparentMask_Ctor
   0x4479c0 stores into entries 17-63. Other indirect calls (vtables, function pointers) are unknown.
4. Per function and per entry state (clean, MMX set) a forward may-analysis: an instruction with an mm register
   sets the state, emms/fninit/frstor clear it, a call continues with the callee's exit state for the state at
   the call (a fixpoint over all functions; a callee called with the state set is analysed for that entry too), a
   tail jump is a call and a return, an unknown call keeps the state. Every x87 instruction reached with the state
   set is a finding, except the control and status word ones (fnstcw, fldcw, fnstsw, fnclex, fwait, the
   environment), which do not touch the stack.
5. Unknown calls in objects whose data points to a function that may return with the state set (a table of
   drawers like blit_rest4b.cpp's `Drawers`; hook records do not count; check modules are left out) may set the
   state too, except the frame getters (slots 30-50, all without x87 and not hooked, listed at the end). In the
   standalone build every object holds the address layer's FN[] array, so the objects are named (--risky, the
   ones the DLL's scan finds).
The unknown calls made with the state set are listed (UNFOLLOWED) with the frame slots' targets in the exe.
Validated against the bug of 2026-10-31: blit_rest4b.cpp without its fix gives warp_blit's second compare as
the only finding in both builds.
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

import capstone
import pefile
from capstone import x86

TBASE, TCOUNT = 0x60cd90, 36        # the span tables T0-T35, 64 entries each
EXE_LO, EXE_HI = 0x401000, 0x600000
HARMLESS = {'fnstcw', 'fstcw', 'fldcw', 'fnstsw', 'fstsw', 'fnclex', 'fclex', 'fwait', 'wait', 'fnstenv', 'fstenv',
            'fldenv'}
CLEARS = {'emms', 'fninit', 'finit', 'femms', 'frstor'}
FRAME_SLOTS = set(range(30, 51))    # the frame getters the drawers call between rows
FRAME_VTS = {'CFrame': 0x5f4814, 'CDIBFrame': 0x5f4460, 'CGroundTextureFrame': 0x5f7954, 'CScreenFrame': 0x5ff360,
             'COffsetClippedProxyFrame': 0x5f8ca8, 'CProxyFrame': 0x5eb62c, 'CLayer': 0x5f8bac}
MM = {getattr(x86, f'X86_REG_MM{i}') for i in range(8)}
SCRATCH = (x86.X86_REG_EAX, x86.X86_REG_ECX, x86.X86_REG_EDX)


def disassembler():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return md


MD = disassembler()


class Obj:
    """A COFF object: sections (name, data, characteristics, relocations by offset) and symbols by index"""

    def __init__(self, path: Path):
        self.path = path
        b = path.read_bytes()
        _m, nsec, _t, symptr, nsym, opt, _c = struct.unpack_from('<HHIIIHH', b, 0)
        strtab = symptr + nsym * 18

        def string(off):
            return b[strtab + off:b.index(b'\0', strtab + off)].decode('latin-1')

        self.secs = []
        for i in range(nsec):
            o = 20 + opt + i * 40
            name = b[o:o + 8].rstrip(b'\0').decode('latin-1')
            _vs, _va, size, raw, relp, _l, nrel, _nl, ch = struct.unpack_from('<IIIIIIHHI', b, o + 8)
            if name.startswith('/'):
                name = string(int(name[1:]))
            relocs = {}
            for k in range(nrel):
                ra, si, ty = struct.unpack_from('<IIH', b, relp + k * 10)
                relocs[ra] = (si, ty)
            self.secs.append({'name': name, 'data': b[raw:raw + size] if raw else b'', 'ch': ch, 'relocs': relocs})
        self.syms = {}
        i = 0
        while i < nsym:
            o = symptr + i * 18
            name = string(struct.unpack_from('<I', b, o + 4)[0]) if b[o:o + 4] == b'\0\0\0\0' \
                else b[o:o + 8].rstrip(b'\0').decode('latin-1')
            value, sec, typ, cls, naux = struct.unpack_from('<IhHBB', b, o + 8)
            self.syms[i] = (name, value, sec, typ, cls)
            i += 1 + naux


class Program:
    def __init__(self, exe: str, objdir: Path, fn_layer: str | None, risky_names: list[str] | None, tables: bool):
        self.objs = [Obj(p) for p in sorted(objdir.glob('*.o'))]
        pe = pefile.PE(exe, fast_load=True)
        self.img, self.base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
        self.fn = []
        if fn_layer:
            txt = Path(fn_layer).read_text()
            body = txt[txt.index('FN[FN_COUNT] = {'):]
            self.fn = [int(x, 16) for x in re.findall(r'0x[0-9a-f]+', body[:body.index('}')])]
        self.risky_names, self.tables = risky_names, tables
        self.find_functions()
        self.hook = {}
        self.find_static_hooks()
        self.find_stack_hooks()
        self.generic = self.generic_entries()
        self.jt = self.jump_tables()
        self.insns = {k: list(MD.disasm(self.code(f), f['start'])) for k, f in self.funcs.items()}
        self.kinds = {k: self.classify(k) for k in self.funcs}

    # ---- functions, symbols, hooks -------------------------------------------------------------------------------

    def find_functions(self):
        self.funcs, self.by_name, self.local, self.at_off = {}, {}, {}, defaultdict(list)
        for oi, o in enumerate(self.objs):
            per = defaultdict(list)
            for name, value, sec, typ, cls in o.syms.values():
                if sec > 0 and (typ >> 4) == 2 and o.secs[sec - 1]['ch'] & 0x20:
                    per[sec - 1].append((value, name, cls))
            for sec, lst in per.items():
                lst.sort()
                size = len(o.secs[sec]['data'])
                for k, (value, name, cls) in enumerate(lst):
                    key = (oi, sec, value)
                    if key in self.funcs:
                        continue
                    end = lst[k + 1][0] if k + 1 < len(lst) else size
                    self.funcs[key] = {'name': name, 'obj': o.path.name, 'oi': oi, 'sec': sec, 'start': value, 'end': end,
                                       'relocs': o.secs[sec]['relocs']}
                    if cls == 2:
                        self.by_name.setdefault(name, key)
                    self.local[(oi, name)] = key
                    self.at_off[(oi, sec)].append((value, key))

    def code(self, f):
        return self.objs[f['oi']].secs[f['sec']]['data'][f['start']:f['end']]

    def func_at(self, oi, sec, off):
        for start, key in self.at_off.get((oi, sec), []):
            if self.funcs[key]['start'] <= off < self.funcs[key]['end']:
                return key
        return None

    def sym_target(self, oi, si, addend=0):
        """The function a symbol of object oi names: an external, a static, or a section symbol plus addend"""
        name, value, sec, typ, cls = self.objs[oi].syms[si]
        if sec > 0 and (typ >> 4) == 2:
            return self.local.get((oi, name)) or self.by_name.get(name)
        if sec == 0 and cls == 2:
            return self.by_name.get(name)
        if sec > 0 and self.objs[oi].secs[sec - 1]['ch'] & 0x20:
            return self.func_at(oi, sec - 1, value + addend)
        return None

    def sym_name(self, oi, rel):
        return self.objs[oi].syms[rel[0]][0] if rel else ''

    def find_static_hooks(self):
        """Hook records in data: {exe address, function relocation, name string relocation, ...}"""
        for oi, o in enumerate(self.objs):
            for sec in o.secs:
                if not sec['name'].startswith(('.rdata', '.data')):
                    continue
                for ra, (si, _ty) in sec['relocs'].items():
                    if ra < 4 or ra + 4 not in sec['relocs'] or not o.syms[sec['relocs'][ra + 4][0]][0].startswith('??_C@'):
                        continue
                    addr = struct.unpack_from('<I', sec['data'], ra - 4)[0]
                    if EXE_LO <= addr < EXE_HI:
                        key = self.sym_target(oi, si, struct.unpack_from('<I', sec['data'], ra)[0])
                        if key:
                            self.hook.setdefault(addr, key)

    def find_stack_hooks(self):
        """Hook records a hooks_<module> function builds on the stack: `mov [esp+X], address`, a function
        relocation (at X+4 or through a temporary), the name string's relocation at X+8"""
        for f in self.funcs.values():
            if not f['name'].startswith('?hooks_'):
                continue
            pending, last = {}, None
            for ins in MD.disasm(self.code(f), f['start']):
                rel = self.reloc(f, ins)
                ops = ins.operands
                if not (ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM and ops[1].type == x86.X86_OP_IMM):
                    continue
                disp = ops[0].mem.disp
                if rel and self.sym_name(f['oi'], rel).startswith('??_C@'):
                    addr = pending.pop(disp - 8, None)
                    if addr and last and addr not in self.hook:
                        self.hook[addr] = last
                    last = None
                elif rel:
                    last = self.sym_target(f['oi'], rel[0], ops[1].imm & 0xffffffff) or last
                elif EXE_LO <= (ops[1].imm & 0xffffffff) < EXE_HI:
                    pending[disp] = ops[1].imm & 0xffffffff

    def generic_entries(self):
        """What CBlendTransparentMask_Ctor 0x4479c0 stores into the tables at run time (`rep stosd` of the generic
        loop's address into entries 17-63, single stores)"""
        out, regs = defaultdict(set), {}
        md = disassembler()
        for ins in md.disasm(self.img[0x4479c0 - self.base:0x4479c0 - self.base + 0x2000], 0x4479c0):
            ops = ins.operands
            if ins.mnemonic.startswith('rep stos'):
                d, v = regs.get(x86.X86_REG_EDI), regs.get(x86.X86_REG_EAX)
                if d and v and TBASE <= d < TBASE + TCOUNT * 0x100 and EXE_LO <= v < EXE_HI:
                    out[(d - TBASE) // 0x100].add(v)
            elif ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_REG and ops[1].type == x86.X86_OP_IMM:
                regs[ops[0].reg] = ops[1].imm & 0xffffffff
            elif ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM:
                d = ops[0].mem.disp & 0xffffffff
                v = ops[1].imm & 0xffffffff if ops[1].type == x86.X86_OP_IMM else regs.get(ops[1].reg)
                if TBASE <= d < TBASE + TCOUNT * 0x100 and v and EXE_LO <= v < EXE_HI:
                    out[(d - TBASE) // 0x100].add(v)
            if ins.mnemonic == 'ret':
                break
        return out

    def table_entries(self, t):
        a = TBASE + t * 0x100
        return sorted({x for x in struct.unpack_from('<64I', self.img, a - self.base) if x} | self.generic.get(t, set()))

    def table_call(self, t, m):
        entries = self.table_entries(t)
        return ('call', [self.hook[a] for a in entries if a in self.hook], f'T{t}', m == 'jmp',
                [a for a in entries if a not in self.hook])

    def jump_tables(self):
        """Switch tables: relocations from data into code by section symbol, per (object, code section)"""
        jt = defaultdict(set)
        for oi, o in enumerate(self.objs):
            for sec in o.secs:
                if sec['ch'] & 0x20:
                    continue
                for ra, (si, ty) in sec['relocs'].items():
                    _name, value, ssec, typ, _cls = o.syms[si]
                    if ssec > 0 and o.secs[ssec - 1]['ch'] & 0x20 and (typ >> 4) != 2 and ty == 6:
                        jt[(oi, ssec - 1)].add(value + struct.unpack_from('<I', sec['data'], ra)[0])
        return jt

    # ---- instructions --------------------------------------------------------------------------------------------

    @staticmethod
    def reloc(f, ins):
        rel = None
        for p in range(ins.address, ins.address + ins.size):
            rel = f['relocs'].get(p, rel)
        return rel

    @staticmethod
    def written(ins):
        try:
            out = set(ins.regs_access()[1])
        except capstone.CsError:
            out = set()
        if ins.mnemonic == 'call':
            out |= set(SCRATCH)
        ops = ins.operands
        if ops and ops[0].type == x86.X86_OP_REG and ins.mnemonic not in ('cmp', 'test', 'push'):
            out.add(ops[0].reg)
        return out

    @staticmethod
    def is_mmx(ins):
        if ins.mnemonic in CLEARS:
            return False
        try:
            regs = ins.regs_access()
            used = set(regs[0]) | set(regs[1])
        except capstone.CsError:
            used = set()
        used |= {op.reg for op in ins.operands if op.type == x86.X86_OP_REG}
        return bool(used & MM)

    def classify(self, key):
        """One entry per instruction: None, ('clear',), ('mmx',), ('x87', harmless), ('jcc', target),
        ('jmp', target), ('ijmp',), ('ret',), ('call', targets, why, tail, unhooked entries), ('icall', why, tail[, slot])"""
        f, lst = self.funcs[key], self.insns[key]
        at = {ins.address: i for i, ins in enumerate(lst)}
        preds = defaultdict(list)
        for i, ins in enumerate(lst):
            ops = ins.operands
            if ops and ops[0].type == x86.X86_OP_IMM and (ins.mnemonic == 'jmp' or ins.group(x86.X86_GRP_JUMP)) and ops[0].imm in at:
                preds[at[ops[0].imm]].append(i)
            if ins.mnemonic not in ('jmp', 'ret') and i + 1 < len(lst):
                preds[i + 1].append(i)

        def reaching_imm(i, reg):
            """The `mov reg, imm` that reach instruction i on every path back; None when another definition or the
            entry does"""
            out, seen, work = set(), set(), list(preds[i])
            while work:
                j = work.pop()
                if j in seen:
                    continue
                seen.add(j)
                ins = lst[j]
                if reg in self.written(ins):
                    ops = ins.operands
                    if ins.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == x86.X86_OP_IMM and ops[0].reg == reg:
                        out.add(ops[1].imm & 0xffffffff)
                        continue
                    return None
                if j == 0:
                    return None
                work.extend(preds[j])
            return out

        kinds, imm, fnp, table = [], {}, {}, None
        for idx, ins in enumerate(lst):
            rel = self.reloc(f, ins)
            m, ops = ins.mnemonic, ins.operands
            kind = None
            if m in CLEARS:
                kind = ('clear',)
            elif self.is_mmx(ins):
                kind = ('mmx',)
            elif m.startswith('f'):
                kind = ('x87', m in HARMLESS)
            elif m in ('call', 'jmp'):
                op = ops[0]
                if op.type == x86.X86_OP_IMM:
                    if rel:
                        targets, why = [self.sym_target(f['oi'], rel[0])], self.sym_name(f['oi'], rel)
                    elif f['start'] <= op.imm < f['end'] and m == 'jmp':
                        targets, why = None, None
                    else:
                        targets, why = [self.func_at(f['oi'], f['sec'], op.imm)], 'offset'
                    if m == 'jmp' and (targets is None or targets[0] == key):
                        kind = ('jmp', op.imm)
                    else:
                        kind = ('call', targets, why, m == 'jmp')
                elif op.type == x86.X86_OP_REG:
                    v = imm.get(op.reg)
                    if op.reg in fnp:
                        kind = ('call', sorted(fnp[op.reg]), 'own function pointer', m == 'jmp')
                    elif v is not None and EXE_LO <= v < EXE_HI:
                        alts = reaching_imm(idx, op.reg) or {v}
                        kind = ('call', [self.hook[a] for a in sorted(alts) if a in self.hook],
                                f'exe {v:#x}' + ('' if v in self.hook else ' (no hook)'), m == 'jmp')
                    elif table is not None:
                        kind = self.table_call(table, m)
                    else:
                        kind = ('icall', 'reg', m == 'jmp')
                else:
                    mem = op.mem
                    if self.fn and self.sym_name(f['oi'], rel) == '_zax_fn_table' and not mem.base and not mem.index:
                        e = self.fn[mem.disp // 4]
                        kind = ('call', [self.hook[e]] if e in self.hook else [], f'exe {e:#x}' + ('' if e in self.hook else ' (no hook)'),
                                m == 'jmp')
                    else:
                        b = imm.get(mem.base) if mem.base else None
                        addr = (b or 0) + mem.disp if (b is not None or mem.base == 0) else None
                        if addr is not None and TBASE <= addr < TBASE + TCOUNT * 0x100 and mem.scale == 4:
                            kind = self.table_call((addr - TBASE) // 0x100, m)
                        elif m == 'jmp':
                            kind = ('ijmp',)
                        elif mem.base and not mem.index and mem.disp % 4 == 0 and 0 <= mem.disp < 0x200:
                            kind = ('icall', f'slot {mem.disp // 4}', False, mem.disp // 4)
                        else:
                            kind = ('icall', 'mem', False)
            elif m in ('ret', 'retf'):
                kind = ('ret',)
            elif ins.group(x86.X86_GRP_JUMP):
                kind = ('jcc', ops[0].imm) if ops[0].type == x86.X86_OP_IMM else ('ijmp',)
            kinds.append(kind)

            # the registers' known values for the call shapes above (linear)
            if rel and len(ops) == 2 and ops[1].type == x86.X86_OP_MEM and ops[1].mem.index:
                name = self.sym_name(f['oi'], rel)
                if name.startswith('_zax_block_'):
                    a = int(name[len('_zax_block_'):], 16) + (ops[1].mem.disp & 0xffffffff)
                    if TBASE <= a < TBASE + TCOUNT * 0x100:
                        table = (a - TBASE) // 0x100
            dst = ops[0].reg if len(ops) == 2 and ops[0].type == x86.X86_OP_REG else None
            if m == 'mov' and dst and self.fn and ops[1].type == x86.X86_OP_MEM and not ops[1].mem.index \
                    and self.sym_name(f['oi'], rel) == '_zax_fn_table':
                imm[dst] = self.fn[(ops[1].mem.disp & 0xffffffff) // 4]
                fnp.pop(dst, None)
            elif m == 'mov' and dst and ops[1].type == x86.X86_OP_IMM:
                imm.pop(dst, None)
                fnp.pop(dst, None)
                if not rel:
                    imm[dst] = ops[1].imm & 0xffffffff
                else:
                    t = self.sym_target(f['oi'], rel[0], ops[1].imm & 0xffffffff)
                    if t:
                        fnp[dst] = {t}
            elif (m == 'mov' or m.startswith('cmov')) and dst and ops[1].type == x86.X86_OP_REG:
                src = ops[1].reg
                keep = fnp.get(dst, set()) if m.startswith('cmov') else set()
                if src in fnp:
                    fnp[dst] = keep | fnp[src]
                else:
                    fnp.pop(dst, None)
                if m == 'mov' and src in imm:
                    imm[dst] = imm[src]
                else:
                    imm.pop(dst, None)
            else:
                for r in self.written(ins):
                    imm.pop(r, None)
                    fnp.pop(r, None)
        return kinds

    # ---- the analysis --------------------------------------------------------------------------------------------

    def analyse(self, key, entry):
        lst, kd, f = self.insns[key], self.kinds[key], self.funcs[key]
        at = {ins.address: i for i, ins in enumerate(lst)}
        state, work = {}, [(0, entry)]
        out, found, unknown, calls = False, [], [], []
        while work:
            i, s = work.pop()
            if i >= len(lst) or state.get(i) is True or state.get(i) == s:
                continue
            state[i] = s = s or state.get(i, False)
            ins, kind = lst[i], kd[i]
            nxt = [(i + 1, s)]
            if kind is None:
                pass
            elif kind[0] == 'clear':
                nxt = [(i + 1, False)]
            elif kind[0] == 'mmx':
                nxt = [(i + 1, True)]
            elif kind[0] == 'x87':
                if s and not kind[1]:
                    found.append((ins.address, ins.mnemonic, ins.op_str))
            elif kind[0] == 'jcc':
                nxt.append((at.get(kind[1], len(lst)), s))
            elif kind[0] == 'jmp':
                nxt = [(at.get(kind[1], len(lst)), s)]
            elif kind[0] == 'ijmp':
                nxt = [(at[a], s) for a in self.jt[(f['oi'], f['sec'])] if a in at]
            elif kind[0] == 'ret':
                out |= s
                nxt = []
            elif kind[0] == 'call':
                targets = [t for t in (kind[1] or []) if t]
                after = False
                for t in targets:
                    calls.append((t, s))
                    after |= self.exit_state[t][s]
                if not targets:
                    after = s
                    if s:
                        unknown.append((ins.address, kind[2]))
                if len(kind) > 4 and kind[4] and s:
                    unknown.append((ins.address, f'{kind[2]} unhooked entries {[hex(a) for a in kind[4]]}'))
                if kind[3]:
                    out |= after
                    nxt = []
                else:
                    nxt = [(i + 1, after)]
            elif kind[0] == 'icall':
                if s:
                    unknown.append((ins.address, 'indirect ' + kind[1]))
                after = s or (f['oi'] in self.risky and not (len(kind) > 3 and kind[3] in FRAME_SLOTS))
                if kind[2]:
                    out |= after
                    nxt = []
                else:
                    nxt = [(i + 1, after)]
            work.extend(nxt)
        return out, found, unknown, calls

    def data_functions(self, oi):
        """The functions an object's data points to (relocations, hooked exe addresses), hook records left out"""
        o, out = self.objs[oi], set()
        for sec in o.secs:
            if sec['ch'] & 0x20 or not sec['data'] or sec['name'].startswith('.debug'):
                continue
            d = sec['data']
            for off in range(0, len(d) - 3, 4):
                v = struct.unpack_from('<I', d, off)[0]
                if off in sec['relocs']:
                    if off + 4 in sec['relocs'] and o.syms[sec['relocs'][off + 4][0]][0].startswith('??_C@'):
                        continue
                    t = self.sym_target(oi, sec['relocs'][off][0], v)
                    if t:
                        out.add(t)
                elif v in self.hook:
                    out.add(self.hook[v])
        return out

    def run(self):
        self.exit_state = defaultdict(lambda: {False: False, True: True})
        self.entered = defaultdict(set)
        self.risky, fixed = set(), not self.tables
        self.results = {}
        self.rounds, changed = 0, True
        while changed:
            changed = False
            self.rounds += 1
            todo = [(k, False) for k in self.funcs] + [(k, True) for k in self.funcs if True in self.entered[k]]
            for key, entry in todo:
                out, found, unknown, calls = self.analyse(key, entry)
                self.results[(key, entry)] = (found, unknown)
                if self.exit_state[key][entry] != out:
                    self.exit_state[key][entry] = out
                    changed = True
                for t, s in calls:
                    if s and True not in self.entered[t]:
                        self.entered[t].add(True)
                        changed = True
            if not changed and not fixed:
                # step 5, once on the converged base result (no feedback)
                if self.risky_names is not None:
                    self.risky = {oi for oi, o in enumerate(self.objs) if o.path.name in self.risky_names}
                else:
                    leaves = {k for k in self.funcs if self.exit_state[k][False]}
                    self.risky = {oi for oi in range(len(self.objs))
                                  if not self.objs[oi].path.name.startswith('check') and self.data_functions(oi) & leaves}
                fixed = changed = True

    def exe_x87(self, addr, depth=0):
        """x87 stack instructions of an exe function up to its first ret (and of its direct callees, one level)"""
        out = []
        for ins in disassembler().disasm(self.img[addr - self.base:addr - self.base + 0x400], addr):
            if ins.mnemonic.startswith('f') and ins.mnemonic not in HARMLESS:
                out.append(f'{ins.address:#x} {ins.mnemonic}')
            if ins.mnemonic == 'call' and depth < 1 and ins.op_str.startswith('0x'):
                out += self.exe_x87(int(ins.op_str, 16), depth + 1)
            if ins.mnemonic in ('ret', 'jmp'):
                break
        return out

    def report(self, verbose):
        funcs = self.funcs
        order = sorted(self.results.items(), key=lambda kv: (funcs[kv[0][0]]['obj'], kv[0][0][2], kv[0][1]))
        print('objects whose unknown calls may set the state:', sorted(self.objs[o].path.name for o in self.risky))
        print(f'objects {len(self.objs)}, functions {len(funcs)}, hooks {len(self.hook)}, rounds {self.rounds}')
        print(f"functions with MMX instructions: {sum(any(k and k[0] == 'mmx' for k in self.kinds[f]) for f in funcs)}; "
              f'may return with the state set: {sum(self.exit_state[k][False] for k in funcs)}; '
              f'entered with the state set: {sum(True in self.entered[k] for k in funcs)}')
        n = 0
        for (key, entry), (found, _unknown) in order:
            if found:
                n += len(found)
                print(f"FINDING {funcs[key]['obj']} {funcs[key]['name']} entered {'set' if entry else 'clean'}: "
                      + '; '.join(f'{a:#x} {m} {o}' for a, m, o in found))
        print('x87 findings:', n)
        u, slots = 0, defaultdict(list)
        for (key, entry), (_found, unknown) in order:
            if unknown:
                u += len(unknown)
                print(f"UNFOLLOWED {funcs[key]['obj']} {funcs[key]['name']} entered {'set' if entry else 'clean'}: "
                      + '; '.join(f'{a:#x} {w}' for a, w in unknown))
                at = {ins.address: i for i, ins in enumerate(self.insns[key])}
                for a, _w in unknown:
                    k = self.kinds[key][at[a]]
                    if k[0] == 'icall' and len(k) > 3:
                        slots[k[3]].append(f"{funcs[key]['obj']}:{a:#x}")
        print('calls not followed with the state set:', u)
        for s, sites in sorted(slots.items()):
            print(f'SLOT {s} ({len(sites)} call sites with the state set, e.g. {sites[0]}); the frame classes\' targets:')
            for t in sorted({struct.unpack_from('<I', self.img, vt - self.base + 4 * s)[0] for vt in FRAME_VTS.values()}):
                h = self.hook.get(t)
                own = [f'{i.address:#x} {i.mnemonic}' for i, k in zip(self.insns[h], self.kinds[h]) if k and k[0] == 'x87' and not k[1]] if h else None
                print(f"   {t:#x} exe x87 {self.exe_x87(t)[:4]} hook {funcs[h]['name'][:60] if h else None} own x87 {own}")
        if verbose:
            for k in sorted(funcs, key=lambda k: (funcs[k]['obj'], k[2])):
                if self.exit_state[k][False]:
                    print('MAY-RETURN-SET', funcs[k]['obj'], funcs[k]['name'])
        return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('exe', help='Zax.exe (the German build 1308)')
    ap.add_argument('objdir', type=Path, help='the folder with the build\'s .o files (ZAX_KEEP_TMP)')
    ap.add_argument('--fn-layer', help='the standalone build\'s addr_layer.inc (its FN[] array)')
    ap.add_argument('--risky', help='comma-separated object names for step 5 instead of the data scan')
    ap.add_argument('--no-tables', action='store_true', help='leave out step 5')
    ap.add_argument('-v', action='store_true', help='list the functions that may return with the state set')
    a = ap.parse_args()
    if not any(a.objdir.glob('*.o')):
        ap.error(f'no .o files in {a.objdir}')
    p = Program(a.exe, a.objdir, a.fn_layer, a.risky.split(',') if a.risky else None, not a.no_tables)
    p.run()
    return 1 if p.report(a.v) else 0


if __name__ == '__main__':
    sys.exit(main())
