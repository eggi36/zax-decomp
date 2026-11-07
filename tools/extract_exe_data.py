#!/usr/bin/env python3
"""The standalone exe's own data blocks (docs/DECOMPILATION.md 13, "Batch 6-3: the address layer"), at build time.

    extract_exe_data.py <Zax.exe> <dir>

For every block of docs/exe_layout.csv whose home is `own` it writes our own definition into <dir>/own_blocks.cpp
(compiled into Zax-Standalone.exe by reimpl/standalone/build_exe.sh): `zax_block_<address>`, as many bytes as the
block, aligned as the original address is (up to 16), with the initial bytes of the user's Zax.exe (.rdata and the
initialised .data; .bss and the IAT zero: the loader fills the IAT).

The pointer map: every aligned dword of the initialised .rdata (without the IAT) and .data whose value lies in the
image is classified by its target: a function start or a place inside a function (code), the data, or ambiguous (the
four bytes are also text, e.g. "kAI\\0"; docs/exe_pointer_classes.csv decides those: `pointer` or `value`). The
relocations the loader applies at its start are the data pointers whose target lies in an own block, wherever the
pointer lies (in an own block or in the image): `zax_relocs` (location, target as exe addresses; the loader resolves
both through the layer). Pointers to code (since batch 6-10b): every dword whose value is a
function of the function table, in an own block or in the image, becomes that table entry at the start
(`zax_code_relocs`: location, index; the layer's code values are the same entries, reimpl.h); <dir>/code_pointers_left.csv
lists the text-like ones left as exe addresses. An ambiguous dword that lies in or points into an own block
and is not classified stops the build. <dir>/pointer_map.csv lists the pointers that touch own blocks (location,
target, kind) for whoever moves the next block.

Everything written is derived from the user's own Zax.exe and stays in the build's folders (out/, the temporary
folder); it is never committed.
"""
import bisect
import csv
import hashlib
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import exe_layout  # noqa: E402

SHA256 = '7d9efcae1ef09975b0826e7c5d164d97a490bda5fa826ddf906e2e4acea236e4'
ROOT = exe_layout.ROOT
CLASSES = ROOT / 'docs/exe_pointer_classes.csv'


class Image:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if hashlib.sha256(self.data).hexdigest() != SHA256:
            sys.exit('%s: not the supported Zax.exe (SHA-256 %s...)' % (path, SHA256[:8]))
        pe = struct.unpack_from('<I', self.data, 0x3c)[0]
        n, opt = struct.unpack_from('<HxxxxxxxxxxxxH', self.data, pe + 6)
        base = struct.unpack_from('<I', self.data, pe + 0x34)[0]
        self.secs = []
        for i in range(n):
            o = pe + 0x18 + opt + 40 * i
            vsize, va, rsize, raw = struct.unpack_from('<IIII', self.data, o + 8)
            self.secs.append((base + va, rsize, raw))

    def bytes(self, a, n):
        """The file's bytes of [a, a + n), zero behind a section's raw data."""
        out = bytearray(n)
        for va, rsize, raw in self.secs:
            lo, hi = max(a, va), min(a + n, va + rsize)
            if lo < hi:
                out[lo - a:hi - a] = self.data[raw + lo - va:raw + hi - va]
        return bytes(out)


def texty(v):
    return all(b == 0 or 0x20 <= b < 0x7f for b in struct.pack('<I', v))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    img = Image(sys.argv[1])
    out = Path(sys.argv[2])
    blocks = exe_layout.load_blocks()
    own = sorted(a for a, b in blocks.items() if b['home'] == 'own')
    own_end = [a + blocks[a]['size'] for a in own]

    def own_block(v):
        i = bisect.bisect_right(own, v) - 1
        return own[i] if i >= 0 and v < own_end[i] else None

    classes = {}
    if CLASSES.exists():
        with open(CLASSES, newline='', encoding='utf-8') as f:
            classes = {int(r['address'], 16): r['class'] for r in csv.DictReader(f)}
    funcs = sorted(int(r['address'], 16) for r in exe_layout.read_csv(ROOT / 'docs/functions.csv'))

    relocs, listed, errors = [], [], []
    for lo, hi in ((exe_layout.IAT[1], exe_layout.RDATA[1]), exe_layout.DATA):
        raw = img.bytes(lo, hi - lo)
        for off in range(0, hi - lo - 3, 4):
            v = struct.unpack_from('<I', raw, off)[0]
            if not 0x401000 <= v < exe_layout.BSS[1]:
                continue
            loc = lo + off
            if own_block(loc) is None and own_block(v) is None:
                continue
            if exe_layout.TEXT[0] <= v < exe_layout.TEXT[1]:
                kind = 'code' if v in funcs or not texty(v) else 'ambiguous code'
            else:
                kind = 'data' if not texty(v) else 'ambiguous data'
            if kind.startswith('ambiguous'):
                c = classes.get(loc)
                if c is None:
                    errors.append('0x%x -> 0x%x (%s)' % (loc, v, kind))
                    continue
                if c == 'value':
                    listed.append((loc, v, 'value'))
                    continue
                kind = kind.split()[1]
            listed.append((loc, v, kind))
            if kind == 'data' and own_block(v) is not None:
                relocs.append((loc, v))
    if errors:
        sys.exit('pointer-like dwords in or into own blocks not classified in docs/exe_pointer_classes.csv:\n  ' + '\n  '.join(errors))

    # The code pointers of the data (since batch 6-10b): every aligned dword of .rdata (without the IAT) and the
    # initialised .data, in an own block or in the image, whose value is a function of the function table (the
    # addresses of docs/reimpl.csv in .text) becomes that table entry at the start (the replacement once the hooks are
    # installed), so the code values the source spells with fn_addr/code (reimpl.h) and the data agree. Taken: a slot of
    # a vtable, a dword whose bytes are no text, an ambiguous one classified `pointer`; left as the exe address (and
    # counted): an unclassified dword whose bytes are also text (it may be text; as an exe address it still reaches our
    # code through the hook jump, only a comparison with a code value would differ).
    fn_index = {a: i for i, a in enumerate(sorted({int(r['address'], 16) for r in exe_layout.read_csv(ROOT / 'docs/reimpl.csv')
                                                   if exe_layout.TEXT[0] <= int(r['address'], 16) < exe_layout.TEXT[1]}))}
    vt_blocks = sorted((a, a + b['size']) for a, b in blocks.items() if b['kind'] == 'vtable')
    vt_starts = [a for a, _ in vt_blocks]

    def in_vtable(loc):
        i = bisect.bisect_right(vt_starts, loc) - 1
        return i >= 0 and loc < vt_blocks[i][1]
    code_relocs, code_skipped = [], []
    func_set = set(funcs)
    for lo, hi in ((exe_layout.IAT[1], exe_layout.RDATA[1]), exe_layout.DATA):
        raw = img.bytes(lo, hi - lo)
        vals = [struct.unpack_from('<I', raw, off)[0] for off in range(0, hi - lo - 3, 4)]
        # runs of function starts (tables of code pointers: the C runtime's initializer tables, callbacks): a text-like
        # member of a run of at least three with a member that is no text is a code pointer too (the initializer
        # table 0x609000 has 220 such entries, e.g. 0x403870 = "p8@\0")
        in_run = [False] * len(vals)
        i = 0
        while i < len(vals):
            j = i
            while j < len(vals) and vals[j] in func_set:
                j += 1
            if j - i >= 3 and any(not texty(v) for v in vals[i:j]):
                in_run[i:j] = [True] * (j - i)
            i = max(j, i + 1)
        for k, v in enumerate(vals):
            if v not in fn_index:
                continue
            loc = lo + 4 * k
            if in_vtable(loc) or not texty(v) or in_run[k] or classes.get(loc) == 'pointer':
                code_relocs.append((loc, fn_index[v]))
            elif classes.get(loc) != 'value':
                code_skipped.append((loc, v))

    lines = ['// Generated at build time by tools/extract_exe_data.py from the user\'s Zax.exe: the blocks of docs/exe_layout.csv',
             '// whose home is our own definition, and the relocations of the pointers to them. Never committed.',
             'extern "C" {']
    for a in own:
        b = blocks[a]
        align = min(16, a & -a)
        if b['section'] in ('.bss', 'IAT'):
            lines.append('__attribute__((aligned(%d))) unsigned char zax_block_%x[0x%x];' % (align, a, b['size']))
            continue
        data = img.bytes(a, b['size'])
        lines.append('__attribute__((aligned(%d))) unsigned char zax_block_%x[0x%x] = {' % (align, a, b['size']))
        for i in range(0, len(data), 32):
            lines.append('    ' + ','.join(str(x) for x in data[i:i + 32]) + ',')
        lines.append('};')
    # The data the generated assembler modules address (tools/crt_asm_gen.py, ZAX_ASM_DATA/OFFSET/PUSH; since batch
    # 6-4): a symbol per address, our own block's place when its home is `own`, else the image's address
    asm_data = set()
    for src in sorted((ROOT / 'reimpl/src').glob('*.cpp')):
        asm_data.update(int(x, 16) for x in re.findall(r'\bZAX_ASM_(?:DATA|OFFSET|PUSH)\((0x[0-9a-f]+)\)', src.read_text(encoding='latin-1')))
    if asm_data:
        lines.append('}')
        lines.append('__asm__(')
        for a in sorted(asm_data):
            b = own_block(a)
            value = '_zax_block_%x + 0x%x' % (b, a - b) if b is not None else '0x%x' % a
            lines.append('    ".globl _zax_data_0x%x\\n.set _zax_data_0x%x, %s\\n"' % (a, a, value))
        lines.append(');')
        lines.append('extern "C" {')
    # The code values the assembler text names (ZAX_ASM_PUSH_CODE, ZAX_ASM_CODE_ATT; since batch 6-10c): a dword cell per
    # address in .zaxcv with the exe address, which the loader replaces by the function table's entry
    asm_code = set()
    for src in sorted((ROOT / 'reimpl/src').glob('*.cpp')):
        asm_code.update(int(x, 16) for x in re.findall(r'\bZAX_ASM_(?:PUSH_CODE|CODE_ATT)\((0x[0-9a-f]+)\)', src.read_text(encoding='latin-1')))
    if asm_code:
        lines.append('}')
        lines.append('__asm__(".section .zaxcv$m,\\"dw\\"\\n.p2align 2\\n"')
        for a in sorted(asm_code):
            lines.append('    ".globl _zax_code_0x%x\\n_zax_code_0x%x: .long 0x%x\\n"' % (a, a, a))
        lines.append('    ".text\\n");')
        lines.append('extern "C" {')
    # The class variable each class getter reads first (`mov eax, [var]`, a1 imm32; since batch 6-10c): replay.cpp's class
    # kinds of the frame log read it from the getter's code, which the standalone exe no longer has
    getter_vars = []
    for fa in sorted(fn_index):
        head = img.bytes(fa, 5)
        if len(head) == 5 and head[0] == 0xa1:
            getter_vars.append((fa, int.from_bytes(head[1:5], 'little')))
    lines.append('extern const unsigned long zax_getter_vars[][2] = {   // (function, the dword its mov eax, [imm32] reads)')
    for fa, v in getter_vars:
        lines.append('    {0x%x, 0x%x},' % (fa, v))
    lines += ['    {0, 0},', '};', 'extern const int zax_getter_var_count = %d;' % len(getter_vars)]
    lines.append('extern const unsigned long zax_relocs[][2] = {')
    for loc, v in relocs:
        lines.append('    {0x%x, 0x%x},' % (loc, v))
    lines += ['    {0, 0},', '};', 'extern const int zax_reloc_count = %d;' % len(relocs)]
    lines.append('extern const unsigned long zax_code_relocs[][2] = {   // (location as an exe address, function table index)')
    for loc, i in code_relocs:
        lines.append('    {0x%x, %d},' % (loc, i))
    lines += ['    {0, 0},', '};', 'extern const int zax_code_reloc_count = %d;' % len(code_relocs), '}', '']
    (out / 'own_blocks.cpp').write_text('\n'.join(lines), encoding='utf-8')
    with open(out / 'pointer_map.csv', 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(['location', 'target', 'kind'])
        for loc, v, kind in listed:
            w.writerow(['0x%x' % loc, '0x%x' % v, kind])
    with open(out / 'code_pointers_left.csv', 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(['location', 'target'])
        for loc, v in code_skipped:
            w.writerow(['0x%x' % loc, '0x%x' % v])
    print('own blocks: %d (%d bytes), pointers touching them: %d, relocations: %d, code pointers to the function table: %d '
          '(%d text-like left)' % (len(own), sum(blocks[a]['size'] for a in own), len(listed), len(relocs), len(code_relocs),
                                   len(code_skipped)))


if __name__ == '__main__':
    main()
