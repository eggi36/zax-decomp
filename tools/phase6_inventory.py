#!/usr/bin/env python3
"""Phase 6 inventory (docs/DECOMPILATION.md, "Phase 6: the standalone exe"): everything reimpl/src still takes
from Zax.exe, counted from the sources and the exe.

What it reads:
- reimpl/src (*.cpp, *.h, *.inc, libs/, libc/): every hex literal in the exe's image (0x400000-0x75d000) with
  the comments removed (C++ comments and the `#` comments of the generated assembler), and every name a
  `constexpr`/`const`/`enum` gives such an address in the same file or a header; each use is tagged with its
  file's role (production, check, generated), whether it sits in a hook table (ZAX_HOOKS, *_hooks.inc), in an
  `original<...>(...)` call (the trampoline of the exe's own code) and whether the line writes through it
  (`at<T *>(a)[i] = …`, `*at<…>(a) op= …`; a heuristic).
- Zax.exe: the sections, the import table, the resources; every instruction of the functions of
  docs/functions.csv (capstone) for the absolute memory operands and immediates into .rdata/.data/.idata,
  read or written, by which function; the function-local static guards (`test`/`or` of the same bss byte or
  dword with a bit); every dword of .rdata and the initialised .data that points into .text or the data; the
  vtables (a .rdata address the code stores or the class table names, whose dwords are function starts); the
  CRT initializer tables `_cinit` walks (0x609000-0x60a3ec and 0x60a3f0-0x60a408) and the atexit calls of
  their entries.
- docs/functions.csv, docs/reimpl.csv, docs/not_reimplemented.csv, docs/exe_symbols.csv, docs/classes.csv.

Data objects have no sizes in the exe: a region runs from a referenced address to the next address anything
references (the source, the exe's code, a pointer in the data, a named symbol), so a size is an upper bound.

Writes docs/phase6_inventory.csv (kind, address, size, section, name, counts; never the exe's bytes: only
how many of them are non-zero) and prints the summary tables of the section.

    .venv/bin/python -I tools/phase6_inventory.py [--exe "$(python3 -I tools/game_paths.py exe)"] [--csv docs/phase6_inventory.csv]
"""
import argparse
import bisect
import collections
import csv
import re
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import game_paths  # noqa: E402

import capstone
from capstone import x86
import pefile

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'reimpl/src'
IMAGE_LO, IMAGE_HI = 0x400000, 0x75d000
GAME_END = 0x5c3bf0                      # the end of the game code (docs/DECOMPILATION.md 11)
INIT_TABLES = [(0x609000, 0x60a3ec, 'C++ initializers (__xc)'), (0x60a3f0, 0x60a408, 'C initializers (__xi)')]
ATEXIT = 0x5d0863
# a literal may carry an integer suffix (0x6125c8u, the generated static_c2*.cpp); until 2026-10-29 (batch 6-3) the
# suffixed ones (5,550 uses) were not counted
HEX = re.compile(r'\b0x([0-9a-fA-F]{6})(?:[uUlL]{1,3})?\b')
NAMED = re.compile(r'\b([A-Za-z_]\w*)\s*=\s*0x([0-9a-fA-F]{6})(?:[uUlL]{1,3})?\b')


def read_csv(name):
    with open(ROOT / 'docs' / name, newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))


def strip_comments(text):
    """C++ comments out, string literals kept but the assembler's `# …` comments inside them removed."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            seg = text[i:n if j < 0 else j + 2]
            out.append('\n' * seg.count('\n'))
            i = n if j < 0 else j + 2
        elif c == 'R' and text.startswith('R"', i):
            k = text.find('(', i)
            delim = ')' + text[i + 2:k] + '"'
            j = text.find(delim, k)
            j = n if j < 0 else j + len(delim)
            out.append(re.sub(r'#[^\n]*', '', text[i:j]))
            i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"' and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(re.sub(r'#.*?(?=\\n|$)', '', text[i:j + 1]))
            i = j + 1
        elif c == "'":
            j = i + 1
            while j < n and text[j] != "'" and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def file_role(path):
    b = path.name
    if b.startswith('check') or '_check' in b:
        return 'check'
    return 'prod'


class Exe:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.pe = pefile.PE(data=self.data)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.secs = []
        for s in self.pe.sections:
            lo = self.base + s.VirtualAddress
            self.secs.append((s.Name.rstrip(b'\0').decode(), lo, lo + s.Misc_VirtualSize, s.PointerToRawData,
                              s.SizeOfRawData))
        self.sec = {s[0]: s for s in self.secs}
        iat = self.pe.OPTIONAL_HEADER.DATA_DIRECTORY[12]
        self.iat = (self.base + iat.VirtualAddress, self.base + iat.VirtualAddress + iat.Size)

    def section(self, va):
        for name, lo, hi, _, rsize in self.secs:
            if lo <= va < hi:
                if name == '.rdata' and self.iat[0] <= va < self.iat[1]:
                    return 'IAT'
                if name == '.data' and va - lo >= rsize:
                    return '.bss'
                return name
        return 'header' if self.base <= va < self.secs[0][1] else None

    def off(self, va):
        for _, lo, hi, raw, rsize in self.secs:
            if lo <= va < hi and va - lo < rsize:
                return raw + va - lo
        return None

    def dword(self, va):
        o = self.off(va)
        return struct.unpack_from('<I', self.data, o)[0] if o is not None and o + 4 <= len(self.data) else None

    def nonzero(self, lo, hi):
        o = self.off(lo)
        if o is None:
            return 0
        sec_raw_end = next(raw + rsize for _, slo, shi, raw, rsize in self.secs if slo <= lo < shi)
        e = min(o + (hi - lo), sec_raw_end)
        return sum(1 for b in self.data[o:e] if b)


def scan_sources():
    """Every address use in reimpl/src: (address, file, role, line number, flags)."""
    files = sorted(p for p in SRC.rglob('*') if p.suffix in ('.cpp', '.h', '.inc', '.c'))
    texts = {p: strip_comments(p.read_text(encoding='latin-1')) for p in files}
    header_names = collections.defaultdict(set)
    file_names = {}
    for p, t in texts.items():
        names = collections.defaultdict(set)
        for line in t.splitlines():
            if re.search(r'\b(constexpr|const|enum|#define)\b', line) or re.match(r'\s*[A-Z_][A-Z0-9_]*\s*=', line):
                for m in NAMED.finditer(line):
                    a = int(m.group(2), 16)
                    if IMAGE_LO <= a < IMAGE_HI:
                        names[m.group(1)].add(a)
        file_names[p] = names
        if p.suffix == '.h':
            for k, v in names.items():
                header_names[k] |= v
    generated = {p for p, t in texts.items() if 'Generated by tools/' in p.read_text(encoding='latin-1')[:2000]}
    uses = []
    write_re = re.compile(r'at<[^;]*?\*\s*>\(\s*(?P<a>\w+)\s*\)\s*(\[[^\]]*\])?\s*(\.\w+|->\w+)?\s*([-+|&^*/]|<<|>>)?=(?!=)')
    deref_write = re.compile(r'\*\s*at<[^;]*?>\(\s*(?P<a>\w+)\s*\)\s*([-+|&^*/]|<<|>>)?=(?!=)')
    for p, t in texts.items():
        role = file_role(p)
        in_hooks = p.name.endswith('_hooks.inc')
        local = file_names[p]
        name_re = None
        known = {k: v for k, v in header_names.items()}
        for k, v in local.items():
            known[k] = v
        if known:
            name_re = re.compile(r'\b(' + '|'.join(sorted(map(re.escape, known), key=len, reverse=True)) + r')\b')
        depth_hooks = False
        for no, line in enumerate(t.splitlines(), 1):
            if line.startswith('ZAX_HOOKS('):
                depth_hooks = True
            hook = in_hooks or depth_hooks
            if depth_hooks and line.startswith('}'):
                depth_hooks = False
            orig_spans = [m.span() for m in re.finditer(r'original<[^;]*?>\(\s*\w+\s*\)', line)]
            writes = {m.group('a') for m in write_re.finditer(line)} | {m.group('a') for m in deref_write.finditer(line)}
            defining = set()
            for m in NAMED.finditer(line):
                defining.add(m.start(2) - 2)
            hits = []
            for m in HEX.finditer(line):
                a = int(m.group(1), 16)
                if IMAGE_LO <= a < IMAGE_HI:
                    hits.append((a, m.start(), m.group(0)))
            if name_re:
                for m in name_re.finditer(line):
                    if m.end() < len(line) and re.match(r'\s*=\s*0x', line[m.end():]):
                        continue   # the definition itself; its literal is counted
                    for a in known[m.group(1)]:
                        hits.append((a, m.start(), m.group(1)))
            assigns = [m.start() for m in re.finditer(r'(?<![=!<>+\-*/|&^])=(?!=)', line)
                       if not re.search(r'\b(constexpr|const)\b', line[:m.start()])]
            for a, pos, tok in hits:
                flags = set()
                if any(q < pos for q in assigns) or re.search(r'\breturn\b', line[:pos]) is None and '{' in line[:pos] and assigns:
                    flags.add('assign')
                if hook:
                    flags.add('hook')
                if any(lo <= pos < hi for lo, hi in orig_spans):
                    flags.add('original')
                if tok in writes or ('0x' + format(a, 'x')) in writes:
                    flags.add('write')
                if p in generated:
                    flags.add('generated')
                uses.append((a, p.relative_to(SRC).as_posix(), role, no, frozenset(flags)))
    return uses, files


def mem_writes(insn):
    """Operand indices the instruction writes (capstone's access flags)."""
    w = set()
    for i, op in enumerate(insn.operands):
        if op.access & capstone.CS_AC_WRITE:
            w.add(i)
    return w


def scan_exe(exe, functions):
    """Absolute references of the exe's code: target -> {(function, 'r'/'w'/'i')}; guard candidates; the immediates
    a `mov` or `push` takes (stored into memory, loaded into a register or pushed: a value the code uses as such)."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    refs = collections.defaultdict(set)
    guard_read = collections.defaultdict(set)    # bss address -> functions that read it and test right after
    guard_write = collections.defaultdict(set)   # ... that write it right after an `or`
    atexit_callers = set()
    stored = set()                               # immediates the code stores or loads (mov, push): used as a value
    for f, size in functions:
        o = exe.off(f)
        if o is None or size <= 0:
            continue
        code = exe.data[o:o + size]
        insns = list(md.disasm(code, f))
        for n, insn in enumerate(insns):
            try:
                ops = insn.operands
            except capstone.CsError:
                continue
            w = mem_writes(insn)
            for i, op in enumerate(ops):
                if op.type == x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                    t = op.mem.disp & 0xffffffff
                    if exe.base <= t < IMAGE_HI:
                        refs[t].add((f, 'w' if i in w else 'r'))
                        if exe.section(t) == '.bss':
                            nxt = [x.mnemonic for x in insns[n + 1:n + 4]]
                            prv = [x.mnemonic for x in insns[max(0, n - 3):n]]
                            if i not in w and insn.mnemonic in ('mov', 'movzx') and 'test' in nxt:
                                guard_read[t].add(f)
                            if i in w and insn.mnemonic == 'mov' and 'or' in prv:
                                guard_write[t].add(f)
                            if insn.mnemonic == 'test' and i not in w:
                                guard_read[t].add(f)
                            if insn.mnemonic == 'or' and i in w:
                                guard_write[t].add(f)
                elif op.type == x86.X86_OP_MEM and op.mem.index == 0 and op.mem.base != 0:
                    t = op.mem.disp & 0xffffffff
                    if 0x5ea000 <= t < 0x718000:
                        refs[t].add((f, 'x'))      # base register + table address
                elif op.type == x86.X86_OP_IMM:
                    t = op.imm & 0xffffffff
                    if exe.base <= t < IMAGE_HI:
                        refs[t].add((f, 'i'))
                        if insn.mnemonic in ('mov', 'push'):
                            stored.add(t)
            if insn.mnemonic == 'call' and ops and ops[0].type == x86.X86_OP_IMM and (ops[0].imm & 0xffffffff) == ATEXIT:
                atexit_callers.add(f)
    guards = {t for t in guard_read if guard_read[t] & guard_write.get(t, set())}
    return refs, guards, atexit_callers, stored


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', default=None, help='default: the build 1308 exe of tools/game_paths.py')
    ap.add_argument('--csv', default=str(ROOT / 'docs/phase6_inventory.csv'))
    args = ap.parse_args()
    args.exe = args.exe or game_paths.exe_arg()
    exe = Exe(Path(args.exe))

    funcs = {int(r['address'], 16): r for r in read_csv('functions.csv')}
    reimpl = {int(r['address'], 16): r for r in read_csv('reimpl.csv')}
    notre = {int(r['address'], 16): r for r in read_csv('not_reimplemented.csv')}
    syms = {}
    for r in read_csv('exe_symbols.csv'):
        try:
            syms.setdefault(int(r['address'], 16), r)
        except ValueError:
            pass
    classes = read_csv('classes.csv')
    func_starts = sorted(funcs)
    func_list = [(a, int(funcs[a]['size'] or 0)) for a in func_starts]

    def containing(a):
        i = bisect.bisect_right(func_starts, a) - 1
        if i >= 0 and a < func_starts[i] + max(int(funcs[func_starts[i]]['size'] or 0), 1):
            return func_starts[i]
        return None

    def name_of(a):
        if a in syms:
            return syms[a]['name']
        if a in funcs:
            return funcs[a]['name']
        return ''

    uses, files = scan_sources()
    print('scanning the exe code ...', file=sys.stderr)
    refs, guards, atexit_callers, stored = scan_exe(exe, func_list)

    # ---- the import table
    imports = {}
    for d in exe.pe.DIRECTORY_ENTRY_IMPORT:
        for imp in d.imports:
            imports[imp.address] = '%s!%s' % (d.dll.decode().lower(), imp.name.decode() if imp.name else '#%d' % imp.ordinal)

    # ---- pointers in the data
    data_ptrs = collections.defaultdict(set)   # target -> {source addresses}
    for name in ('.rdata', '.data'):
        _, lo, hi, raw, rsize = exe.sec[name]
        for va in range(lo, lo + rsize - 3, 4):
            v = exe.dword(va)
            if v and exe.base <= v < IMAGE_HI and not (exe.iat[0] <= va < exe.iat[1]):
                data_ptrs[v].add(va)

    # ---- vtables: .rdata addresses stored by code or named by the class table whose first dword is a function
    vt_starts = set()
    for c in classes:
        if c['vtable']:
            vt_starts.add(int(c['vtable'], 16))
    # by the code: an immediate that a `mov` stores into an object or loads (or a `push` passes). Until 2026-10-29
    # (batch 6-10a) any immediate counted, so two numbers that happen to point at a vtable slot were taken for vtables
    # and cut the real one before them: the flag mask 0x600000 (`test eax, 0x600000` at 0x48a731 …) cut CShootOverAI's
    # 0x5ffffc after 3 bytes (batch 6-6), the SecuROM loader's address bound 0x5f0000 (`cmp dword ptr [0x7533dc],
    # 0x5f0000` in Cms_CheckReturnAddresses) the key button's 0x5effb0 after 80 bytes (batch 6-9). Every other vtable
    # the code names is stored by a `mov` (1,018 into memory, 24 loaded into a register first)
    for t in stored:
        if exe.section(t) == '.rdata' and exe.dword(t) in funcs:
            vt_starts.add(t)
    for a, r in syms.items():
        if r['kind'] == 'vtable':
            vt_starts.add(a)
    vt_starts = sorted(v for v in vt_starts if exe.section(v) == '.rdata' and exe.dword(v) in funcs)
    vtables = {}
    for i, v in enumerate(vt_starts):
        nxt = vt_starts[i + 1] if i + 1 < len(vt_starts) else exe.sec['.rdata'][2]
        n = 0
        while v + 4 * n < nxt:
            d = exe.dword(v + 4 * n)
            if d is None or not (0x401000 <= d < 0x5ea000):
                break
            n += 1
        vtables[v] = n
    vt_class = {}
    for c in classes:
        if c['vtable']:
            vt_class[int(c['vtable'], 16)] = c['name']
    for v in vtables:
        if v not in vt_class:
            s = name_of(v)
            if s:
                vt_class[v] = s.replace('::vftable', '')
            else:
                # the class by the slots' names (Class::vfN or Class::Method)
                names = collections.Counter(name_of(exe.dword(v + 4 * k)).split('::')[0] for k in range(vtables[v])
                                            if '::' in name_of(exe.dword(v + 4 * k)))
                vt_class[v] = names.most_common(1)[0][0] if names else ''

    # ---- data regions
    starts = set()
    for a, *_ in uses:
        if exe.section(a) in ('.rdata', '.data', '.bss', 'IAT', '.idata', '.rsrc'):
            starts.add(a)
    for t in refs:
        if exe.section(t) in ('.rdata', '.data', '.bss', 'IAT'):
            starts.add(t)
    for t in data_ptrs:   # a dword of text ("Fri ") is no pointer
        if exe.section(t) in ('.rdata', '.data', '.bss', 'IAT') and not all(
                b == 0 or 0x20 <= b < 0x7f for b in struct.pack('<I', t)):
            starts.add(t)
    for a, r in syms.items():
        if r['kind'] in ('data', 'vtable') and exe.section(a):
            starts.add(a)
    bounds = set()
    for name, lo, hi, _, rsize in exe.secs:
        bounds |= {lo, hi}
        if name == '.data':
            bounds.add(lo + rsize)
    bounds |= set(exe.iat)
    allstarts = sorted(starts | bounds)

    def region_size(a):
        i = bisect.bisect_right(allstarts, a)
        return (allstarts[i] if i < len(allstarts) else IMAGE_HI) - a

    # ---- the source's uses grouped
    by_addr = collections.defaultdict(list)
    for u in uses:
        by_addr[u[0]].append(u)

    # code addresses
    code = {'hook': set(), 'call_reimpl': set(), 'call_notre': set(), 'mid': set(), 'original': set(), 'other': set()}
    code_refs = collections.Counter()
    original_prod = collections.defaultdict(set)
    for a, us in by_addr.items():
        if exe.section(a) != '.text':
            continue
        prod = [u for u in us if u[2] == 'prod']
        if not prod:
            continue
        hook = any('hook' in u[4] for u in prod)
        other = [u for u in prod if 'hook' not in u[4]]
        if hook:
            code['hook'].add(a)
        for u in other:
            if 'original' in u[4]:
                code['original'].add(a)
                original_prod[a].add(u[1])
        if other:
            code_refs[a] += len(other)
            if a in reimpl:
                code['call_reimpl'].add(a)
            elif a in funcs or a in notre:
                code['call_notre'].add(a)
            elif containing(a) is not None:
                code['mid'].add(a)
            else:
                code['other'].add(a)

    # data addresses used by the source
    data_rows = []
    for a, us in sorted(by_addr.items()):
        sec = exe.section(a)
        if sec not in ('.rdata', '.data', '.bss', 'IAT', '.idata', '.rsrc', 'header', '.cms_t', '.cms_d'):
            continue
        prod = [u for u in us if u[2] == 'prod']
        chk = [u for u in us if u[2] == 'check']
        size = region_size(a)
        kind = sec
        if a in vtables:
            kind = 'vtable'
        elif sec == 'IAT':
            kind = 'iat'
        elif a in guards:
            kind = 'guard'
        writers = {f for f, k in refs.get(a, ()) if k == 'w'}
        readers = {f for f, k in refs.get(a, ()) if k != 'w'}
        nz = exe.nonzero(a, a + size) if sec in ('.rdata', '.data', 'IAT') else 0
        cptrs = sum(1 for k in range(0, size - 3, 4) if sec in ('.rdata', '.data')
                    and (exe.dword(a + k) or 0) and 0x401000 <= exe.dword(a + k) < 0x5ea000)
        r = dict(kind=kind, address='0x%x' % a, size=size if kind != 'vtable' else 4 * vtables[a], section=sec,
                 name=(vt_class.get(a, '') if kind == 'vtable' else imports.get(a, '') if kind == 'iat' else name_of(a)),
                 prod_refs=len(prod), check_refs=len(chk),
                 prod_writes=sum(1 for u in prod if 'write' in u[4]),
                 prod_assigns=sum(1 for u in prod if 'assign' in u[4]),
                 prod_files=len({u[1] for u in prod}), exe_writers=len(writers), exe_readers=len(readers),
                 init_nonzero=nz, code_ptrs=cptrs,
                 files=' '.join(sorted({u[1] for u in prod}))[:300])
        data_rows.append(r)

    # ---- summary
    def secsum(sel):
        rs = [r for r in data_rows if sel(r)]
        return len(rs), sum(r['size'] for r in rs), sum(r['prod_refs'] for r in rs)

    prod_files = [p for p in files if file_role(p) == 'prod']
    chk_files = [p for p in files if file_role(p) == 'check']
    print('\n## Source files')
    print('| | files | address uses | distinct addresses |')
    print('|---|---|---|---|')
    for role, fl in (('prod', prod_files), ('check', chk_files)):
        us = [u for u in uses if u[2] == role]
        print('| %s | %d | %d | %d |' % (role, len(fl), len(us), len({u[0] for u in us})))
    gen = [u for u in uses if u[2] == 'prod' and 'generated' in u[4]]
    print('| prod, generated files | %d | %d | %d |' % (len({u[1] for u in gen}), len(gen), len({u[0] for u in gen})))

    print('\n## Production uses by section (distinct addresses, region bytes, uses)')
    print('| section / kind | addresses | bytes (upper bound) | uses | written by the source (uses) |')
    print('|---|---|---|---|---|')
    for label, sel in (
            ('.rdata vtables', lambda r: r['kind'] == 'vtable'),
            ('.rdata other (constants, strings, tables)', lambda r: r['section'] == '.rdata' and r['kind'] != 'vtable'),
            ('IAT slots', lambda r: r['kind'] == 'iat'),
            ('.data initialised', lambda r: r['section'] == '.data'),
            ('.bss guards', lambda r: r['kind'] == 'guard'),
            ('.bss other', lambda r: r['section'] == '.bss' and r['kind'] != 'guard'),
            ('other sections', lambda r: r['section'] not in ('.rdata', '.data', '.bss', 'IAT'))):
        rs = [r for r in data_rows if sel(r) and r['prod_refs']]
        print('| %s | %d | %d | %d | %d |' % (label, len(rs), sum(r['size'] for r in rs), sum(r['prod_refs'] for r in rs),
                                            sum(r['prod_writes'] for r in rs)))
    rs = [r for r in data_rows if r['section'] == '.data' and r['prod_refs']]
    print('\n.data initialised, used by prod: %d regions, %d with non-zero bytes (%d non-zero bytes), %d holding code pointers (%d)'
          % (len(rs), sum(1 for r in rs if r['init_nonzero']), sum(r['init_nonzero'] for r in rs),
             sum(1 for r in rs if r['code_ptrs']), sum(r['code_ptrs'] for r in rs)))
    rs = [r for r in data_rows if r['section'] == '.rdata' and r['kind'] != 'vtable' and r['prod_refs']]
    print('.rdata other, used by prod: %d regions, %d bytes, holding code pointers: %d regions (%d pointers)'
          % (len(rs), sum(r['size'] for r in rs), sum(1 for r in rs if r['code_ptrs']), sum(r['code_ptrs'] for r in rs)))
    for label, sec in (('.data initialised', '.data'), ('.bss', '.bss')):
        rs = [r for r in data_rows if r['section'] == sec and r['prod_refs']]
        ro = [r for r in rs if not r['exe_writers']]
        print('%s used by prod: %d regions; the exe code writes %d of them by absolute address, %d never (%d bytes: written only through pointers, by initializers through ecx, or constant)'
              % (label, len(rs), len(rs) - len(ro), len(ro), sum(r['size'] for r in ro)))
    for sec in ('.rdata', '.data', '.bss'):
        rs = [r for r in data_rows if r['section'] == sec and r['prod_refs']]
        _, lo, hi, _, rsize = exe.sec['.data' if sec == '.bss' else sec]
        if sec == '.data':
            hi = lo + rsize
        elif sec == '.bss':
            lo = lo + rsize
        print('%s: %d of %d bytes lie in regions production code names (upper bound)' % (sec, sum(r['size'] for r in rs), hi - lo))
    big = []
    for i, a in enumerate(allstarts[:-1]):
        sec = exe.section(a)
        if sec in ('.data', '.bss') and not any(u[2] == 'prod' for u in by_addr.get(a, ())):
            big.append((allstarts[i + 1] - a, a, sec))
    big.sort(reverse=True)
    print('largest data regions production code does not name:')
    for size, a, sec in big[:12]:
        print('   0x%x %s %d bytes %s; exe code refs %d; data pointers to it %d; checks %d' % (
            a, sec, size, name_of(a), len(refs.get(a, ())), len(data_ptrs.get(a, ())),
            sum(1 for u in by_addr.get(a, ()) if u[2] == 'check')))
    only_check = [r for r in data_rows if not r['prod_refs'] and r['check_refs']]
    print('data addresses used only by checks: %d' % len(only_check))

    print('\n## Sections of the exe (whole)')
    for name, lo, hi, raw, rsize in exe.secs:
        print('%-7s 0x%06x-0x%06x virtual %7d raw %7d' % (name, lo, hi, hi - lo, rsize))
    d = exe.sec['.data']
    print('.data: initialised 0x%x bytes (%d non-zero), .bss 0x%x bytes' % (d[4], exe.nonzero(d[1], d[1] + d[4]), d[2] - d[1] - d[4]))
    r_ = exe.sec['.rdata']
    print('.rdata: %d non-zero bytes; pointers into .text in .rdata %d, in .data %d' % (
        exe.nonzero(r_[1], r_[2]),
        sum(1 for t, s in data_ptrs.items() if 0x401000 <= t < 0x5ea000 for v in s if exe.section(v) == '.rdata'),
        sum(1 for t, s in data_ptrs.items() if 0x401000 <= t < 0x5ea000 for v in s if exe.section(v) == '.data')))
    # the pointer census of the initialised data: what a data image at other addresses must relocate
    def texty(v):
        return all(b == 0 or 0x20 <= b < 0x7f for b in struct.pack('<I', v))
    census = collections.Counter()
    code_targets_notre = set()
    for name in ('.rdata', '.data'):
        _, lo, hi, raw, rsize = exe.sec[name]
        for va in range(lo, lo + rsize - 3, 4):
            if exe.iat[0] <= va < exe.iat[1]:
                continue
            v = exe.dword(va)
            sec = exe.section(v) if v else None
            if sec is None:
                continue
            if sec == '.text' and v in funcs:
                census[(name, 'function start')] += 1
                if v not in reimpl:
                    code_targets_notre.add(v)
            elif texty(v):
                census[(name, 'ambiguous (could be text) to ' + sec)] += 1
            else:
                census[(name, 'to ' + sec + (' (not a function start)' if sec == '.text' else ''))] += 1
    print('pointer-like dwords in the initialised data (aligned; the IAT left out):')
    for k, v in sorted(census.items()):
        print('   %s %s: %d' % (k[0], k[1], v))
    print('   function starts they name that are not reimplemented: %s' % ' '.join(
        '0x%x %s' % (t, name_of(t)) for t in sorted(code_targets_notre)))
    # code of the game the data points at that lies in no function of docs/functions.csv: functions every table
    # missed (the audit's descent counts pointer targets as reached)
    missed = []
    for t, srcs in sorted(data_ptrs.items()):
        if 0x401000 <= t < GAME_END and t not in funcs and containing(t) is None and not texty(t):
            missed.append((t, srcs))
    print('game code the data points at outside every function (not text bytes): %d' % len(missed))
    for t, srcs in missed:
        print('   0x%x %s, from %s%s' % (t, name_of(t), ' '.join('0x%x' % v for v in sorted(srcs)),
                                       '' if t in reimpl else '; NOT REIMPLEMENTED'))

    print('\n## Code addresses used by production code')
    print('hook sites: %d' % len(code['hook']))
    print('calls/pointers to reimplemented entries: %d addresses, %d uses' % (len(code['call_reimpl']), sum(code_refs[a] for a in code['call_reimpl'])))
    print('calls/pointers to functions not in reimpl.csv: %d: %s' % (len(code['call_notre']), ' '.join('0x%x %s' % (a, name_of(a)) for a in sorted(code['call_notre']))))
    print('addresses inside functions (not an entry): %d: %s' % (len(code['mid']), ', '.join(
        '0x%x (in 0x%x %s; %s)' % (a, containing(a), name_of(containing(a)), ' '.join(sorted({u[1] for u in by_addr[a] if u[2] == 'prod'})))
        for a in sorted(code['mid']))))
    print('addresses in .text outside every function: %d: %s' % (len(code['other']), ' '.join('0x%x' % a for a in sorted(code['other']))))
    print('original<> (the exe code itself through a trampoline) in production: %d' % len(code['original']))
    for a in sorted(code['original']):
        print('   0x%x %s (%s) %s' % (a, name_of(a), 'reimpl' if a in reimpl else 'not reimpl', ' '.join(sorted(original_prod[a]))))

    vt_used = [r for r in data_rows if r['kind'] == 'vtable' and r['prod_refs']]
    print('\n## Vtables')
    print('vtables in .rdata: %d, %d slots; named by class: %d' % (len(vtables), sum(vtables.values()), sum(1 for v in vtables if vt_class.get(v))))
    slot_targets = {exe.dword(v + 4 * k) for v, n in vtables.items() for k in range(n)}
    print('distinct slot functions: %d, reimplemented: %d' % (len(slot_targets), sum(1 for t in slot_targets if t in reimpl)))
    print('vtables used by production code: %d (%d uses; %d vtables with %d uses as an assigned or initialised value, i.e. objects our code builds); by checks only: %d' % (
        len(vt_used), sum(r['prod_refs'] for r in vt_used), sum(1 for r in vt_used if r['prod_assigns']),
        sum(r['prod_assigns'] for r in vt_used),
        sum(1 for r in data_rows if r['kind'] == 'vtable' and not r['prod_refs'] and r['check_refs'])))
    stored_by_prod = [r for r in vt_used]
    print('  classes: %s' % ', '.join(sorted({r['name'] or r['address'] for r in stored_by_prod})[:400]))

    print('\n## CRT initializer tables and atexit')
    for lo, hi, label in INIT_TABLES:
        ent = [exe.dword(a) for a in range(lo, hi, 4)]
        ent = [e for e in ent if e]
        print('%s 0x%x-0x%x: %d entries, %d reimplemented, %d call atexit; not reimplemented: %s' % (
            label, lo, hi, len(ent), sum(1 for e in ent if e in reimpl), sum(1 for e in ent if e in atexit_callers),
            ' '.join('0x%x %s' % (e, name_of(e)) for e in ent if e not in reimpl)))
    pre = exe.dword(0x629624)
    print('_fpmath pointer 0x629624 -> 0x%x %s' % (pre or 0, name_of(pre or 0)))

    print('\n## Import table')
    dlls = collections.Counter(v.split('!')[0] for v in imports.values())
    print('imports: %d from %d DLLs: %s' % (len(imports), len(dlls), ', '.join('%s %d' % kv for kv in sorted(dlls.items()))))
    iat_prod = sorted({a for a, us in by_addr.items() if exe.section(a) == 'IAT' and any(u[2] == 'prod' for u in us)})
    iat_chk = sorted({a for a, us in by_addr.items() if exe.section(a) == 'IAT' and any(u[2] == 'check' for u in us)})
    print('IAT slots named by production code: %d; by checks: %d' % (len(iat_prod), len(iat_chk)))
    print('  prod: %s' % ', '.join(imports.get(a, hex(a)) for a in iat_prod))
    used_by_code = {t for t in refs if exe.iat[0] <= t < exe.iat[1]}
    print('IAT slots the exe code uses: %d (game code: %d)' % (
        len(used_by_code), sum(1 for t in used_by_code if any(f < GAME_END for f, _ in refs[t]))))
    win = (SRC / 'win32.h').read_text(encoding='latin-1')
    own = re.findall(r'WINIMPORT[^;(]*?\bWINAPI\s+(\w+)\s*\(', win)
    print('the DLL\'s own imports declared in win32.h: %d' % len(set(own)))

    print('\n## Resources')
    def walk(entry, path):
        for e in entry.entries:
            label = e.name.decode() if e.name else (pefile.RESOURCE_TYPE.get(e.id, e.id) if not path else e.id)
            if hasattr(e, 'directory'):
                walk(e.directory, path + [str(label)])
            else:
                print('  %s size %d' % ('/'.join(path + [str(label)]), e.data.struct.Size))
    walk(exe.pe.DIRECTORY_ENTRY_RESOURCE, [])

    print('\n## Function-local statics (guards)')
    gs = sorted(guards)
    gfun = set()
    for g in gs:
        gfun |= {f for f, k in refs[g]}
    print('functions using them: %d' % len(gfun))
    print('guard bytes/dwords in .bss the exe code tests and sets: %d; named by production code: %d' % (
        len(gs), sum(1 for g in gs if any(u[2] == 'prod' for u in by_addr.get(g, ())))))
    own_statics = 0
    for p in prod_files:
        if p.suffix not in ('.cpp',):
            continue
        for line in strip_comments(p.read_text(encoding='latin-1')).splitlines():
            if re.match(r'\s+static\s+(?!_assert|inline|void|constexpr)\w', line) and '(' not in line.split('=')[0]:
                own_statics += 1
    print('function-local static variables in our production code (indented `static`, not constexpr): %d' % own_statics)

    print('\n## not_reimplemented.csv')
    for reason, n in collections.Counter(r['reason'] for r in notre.values()).most_common():
        print('  %s: %d (%d bytes)' % (reason, n, sum(int(r['size']) for r in notre.values() if r['reason'] == reason)))

    asm_files = [p for p in prod_files if '__asm__' in p.read_text(encoding='latin-1') or 'asm(' in p.read_text(encoding='latin-1')]
    asm_stmts = sum(len(re.findall(r'__asm__|\basm\s*\(', strip_comments(p.read_text(encoding='latin-1')))) for p in prod_files)
    mmx = [p for p in prod_files if re.search(r'\b(movq|punpck\w+|pmull\w|paddus\w|psrl\w|packuswb|emms)\b', p.read_text(encoding='latin-1'))]
    print('\n## Inline assembly in production code')
    print('files with inline assembly: %d, asm statements: %d; files with MMX instructions: %d (%s)' % (
        len(asm_files), asm_stmts, len(mmx), ' '.join(p.name for p in mmx)))

    # ---- the CSV
    with open(args.csv, 'w', newline='', encoding='utf-8') as f:
        cols = ['kind', 'address', 'size', 'section', 'name', 'prod_refs', 'check_refs', 'prod_writes', 'prod_assigns', 'prod_files',
                'exe_writers', 'exe_readers', 'init_nonzero', 'code_ptrs', 'files']
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for r in data_rows:
            w.writerow(r)
        for v, n in sorted(vtables.items()):
            if not any(r['address'] == '0x%x' % v for r in data_rows):
                w.writerow(dict(kind='vtable', address='0x%x' % v, size=4 * n, section='.rdata', name=vt_class.get(v, ''),
                                prod_refs=0, check_refs=0, prod_writes=0, prod_assigns=0, prod_files=0, exe_writers=0,
                                exe_readers=len(refs.get(v, ())), init_nonzero=4 * n, code_ptrs=n, files=''))
        for a in sorted(code['call_notre'] | code['mid'] | code['other'] | code['original']):
            w.writerow(dict(kind='code ' + ('original' if a in code['original'] else 'not reimplemented' if a in code['call_notre']
                                            else 'inside a function' if a in code['mid'] else 'outside functions'),
                            address='0x%x' % a, size='', section='.text', name=name_of(a) or name_of(containing(a) or 0),
                            prod_refs=code_refs[a], check_refs=0, prod_writes=0, prod_assigns=0, prod_files=0, exe_writers=0, exe_readers=0,
                            init_nonzero=0, code_ptrs=0, files=' '.join(sorted(original_prod.get(a, ())))))
    print('\nwrote %s' % args.csv, file=sys.stderr)


if __name__ == '__main__':
    main()
