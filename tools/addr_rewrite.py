#!/usr/bin/env python3
"""Rewrite the exe addresses of production files to the address layer (docs/DECOMPILATION.md 13, "Batch 6-3").

    addr_rewrite.py FILE...            show what would change (a unified diff), nothing written
    addr_rewrite.py --write FILE...    rewrite the files in place
    addr_rewrite.py --audit [FILE...]  count the uses not yet converted per file (default: every production file of
                                       reimpl/src); an unconverted use of an address whose block's home is `own` in
                                       docs/exe_layout.csv is an error (exit status 1)
    addr_rewrite.py --audit --movable  also list the blocks every use of which goes through the layer (candidates
                                       for home `own`; an argument of a helper with a key parameter counts as not
                                       through the layer, to be safe)

What it rewrites (the kind follows from the address: docs/reimpl.csv for code, docs/exe_layout.csv for the data):
- `at<T>(A)(` with A in .text and in docs/reimpl.csv          -> `fn<T>(A)(`        (a call)
- `at<T>(A)` otherwise, A a reimplemented function             -> `code<T>(A)`       (the address as a value)
- `at<T>(A)`, A an IAT slot                                    -> `imp_slot<T>(A)`
- `at<T>(A)`, A the start of a vtable                          -> `vt_ptr<T>(A)`
- `at<T>(A)`, A other data (.rdata, .data, .bss, .text's table) -> `glob<T>(A)`
- `import<F>(A)` (the files' own `*at<F *>(slot)` helper)       -> `imp<F>(A)`; the helper's definition goes when no
                                                                   use is left
- `= A;`, `== A`, `!= A` with a bare literal A                  -> `vt(A)`, `glob_addr(A)`, `fn_addr(A)`
A is a hex literal or a name a `constexpr`/`const`/`enum` gives an exe address in the file or a header (the value must
be a constant expression, which the layer's keys need anyway). Untouched: hook tables (`ZAX_HOOKS`, `*_hooks.inc`),
`original<>`, comments, addresses known only at run time (helpers with an address parameter: give the parameter a key
type, FnKey/CodeKey/GlobKey/VtKey/ImpKey, or use fn_dyn/glob_dyn), code that is not reimplemented (the import thunks,
`_matherr`; reported), numbers in the image's range that are not addresses (reported as bare literals; a human decides).
A rewritten file builds a byte-identical DLL (the DLL backend is the address itself); the compile of both builds is
the check that every key is a constant expression the layer knows.
"""
import argparse
import bisect
import collections
import csv
import difflib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'reimpl/src'
sys.path.insert(0, str(ROOT / 'tools'))
import exe_layout  # noqa: E402

HEX = r'0x[0-9a-fA-F]{6}(?:[uUlL]{1,3})?'   # with an integer suffix too (0x6125c8u, static_c2*.cpp)
NAMED = re.compile(r'\b([A-Za-z_]\w*)\s*=\s*(0x[0-9a-fA-F]{6})(?:[uUlL]{1,3})?\b')
TEMPLATES = ['fn', 'code', 'glob', 'vt_ptr', 'imp_slot', 'imp']
VALUES = ['vt', 'glob_addr', 'fn_addr']
IMPORT_DEF = re.compile(r'^\s*template <typename F> inline F import\(DWORD slot\) \{ return \*at<F \*>\(slot\); \}\s*\n', re.M)


class Kinds:
    def __init__(self):
        self.fns = {int(r['address'], 16) for r in exe_layout.read_csv(ROOT / 'docs/reimpl.csv')}
        self.blocks = exe_layout.load_blocks()
        self.starts = sorted(self.blocks)
        self.vts = {a for a, b in self.blocks.items() if b['kind'] == 'vtable'}

    def block(self, a):
        i = bisect.bisect_right(self.starts, a) - 1
        if i >= 0:
            b = self.blocks[self.starts[i]]
            if a < b['address'] + b['size']:
                return b
        return None

    def kind(self, a):
        if exe_layout.TEXT[0] <= a < exe_layout.TEXT[1] and not self.block(a):
            return 'code' if a in self.fns else 'code not reimplemented'
        if exe_layout.IAT[0] <= a < exe_layout.IAT[1]:
            return 'iat'
        if a in self.vts:
            return 'vtable'
        if self.block(a):
            return 'data'
        return None


def header_names():
    names = {}
    for p in sorted(SRC.glob('*.h')):
        names.update(file_names(p.read_text(encoding='latin-1')))
    return names


def file_names(text):
    names = {}
    for line in text.splitlines():
        code = line.split('//')[0]
        if re.search(r'\b(constexpr|const|enum|#define)\b', code) or re.match(r'\s*[A-Z_][A-Z0-9_]*\s*=', code):
            for m in NAMED.finditer(code):
                a = int(m.group(2), 16)
                if 0x400000 <= a < 0x75d000:
                    names[m.group(1)] = a
    return names


def template_end(s, i):
    """s[i] is the '<' of a template argument list: the index of its '>'."""
    depth = 0
    paren = 0
    while i < len(s):
        c = s[i]
        if c == '(':
            paren += 1
        elif c == ')':
            paren -= 1
        elif c == '<' and paren == 0:
            depth += 1
        elif c == '>' and paren == 0:
            depth -= 1
            if depth == 0:
                return i
        elif c in ';\n' and paren == 0:
            return -1
        i += 1
    return -1


def hook_line_flags(text):
    """Per line: inside a ZAX_HOOKS block."""
    flags, inside = [], False
    for line in text.split('\n'):
        if line.startswith('ZAX_HOOKS('):
            inside = True
        flags.append(inside)
        if inside and line.startswith('}'):
            inside = False
    return flags


def comment_start(line):
    """Position of a // comment outside string literals (or len)."""
    i, n, q = 0, len(line), None
    while i < n:
        c = line[i]
        if q:
            if c == '\\':
                i += 2
                continue
            if c == q:
                q = None
        elif c in '"\'':
            q = c
        elif line.startswith('//', i):
            return i
        i += 1
    return n


def raw_string_state(line, in_raw):
    """(the line is part of a raw string literal over several lines, inside one after it) for the generated assembler
    modules (`__asm__(R"( ... )");`, split by `)" ZAX_ASM_DATA(A) R"(`)."""
    opens, closes = line.rfind('R"('), line.rfind(')"')
    raw = in_raw or opens >= 0
    if opens >= 0 or closes >= 0:
        in_raw = opens > closes
    return raw, in_raw


def rewrite(path, kinds, names, stats):
    text = path.read_text(encoding='latin-1')
    if path.name.endswith('_hooks.inc'):
        return text
    local = dict(names)
    local.update(file_names(text))
    hooks = hook_line_flags(text)
    # a name of the layer the file also uses for something else (a parameter `fn`, a local `vt`): qualified
    stripped = re.sub(r'//[^\n]*', '', text)
    qual = {n: ('zax::' + n if re.search(r'(?<![\w:])%s\b(?!\s*%s)' % (n, '<' if n in TEMPLATES else r'\('), stripped) else n)
            for n in TEMPLATES + VALUES}
    out_lines = []
    in_block_comment = in_decl = in_decl_before = False
    table_depth = 0
    sigs = file_signatures(path)
    in_raw = False
    for no, line in enumerate(text.split('\n')):
        # a raw string literal over several lines (the generated assembler modules): left as it is
        raw, in_raw = raw_string_state(line, in_raw)
        if raw:
            out_lines.append(line)
            continue
        if hooks[no] or in_block_comment:
            if '*/' in line:
                in_block_comment = False
            out_lines.append(line)
            continue
        cut = comment_start(line)
        code, rest = line[:cut], line[cut:]
        if '/*' in code and '*/' not in code[code.index('/*'):]:
            in_block_comment = True
        # a declaration of constants over several lines: its later lines keep their literals
        decl = in_decl or bool(re.search(r'\b(constexpr|const|enum)\b', code))
        in_decl = decl and ';' not in code and '{' not in code.split('=')[0] if decl else False
        code = rewrite_code(code, kinds, local, stats, '%s:%d' % (path.name, no + 1), qual, decl)
        # the rows of a constant table: up to the brace that closes its initializer
        table_rows = table_depth > 0
        if table_depth or decl and '=' in code:
            init = code if table_depth else code[code.index('='):]
            table_depth = max(0, table_depth + init.count('{') - init.count('}'))
        if not (in_decl_before or table_rows or re.search(r'\b(constexpr|consteval|enum|case|static_assert|template)\b|#\s*define|\bstatic\b[^(]*=', code)):
            code = wrap_args(code, kinds, local, stats, '%s:%d' % (path.name, no + 1), qual, sigs[0])
        in_decl_before = in_decl
        out_lines.append(code + rest)
    new = '\n'.join(out_lines)
    if 'import<' not in IMPORT_DEF.sub('', new):
        new = IMPORT_DEF.sub('', new)
    return new


LAYER_NAMES = {'fn', 'code', 'glob', 'vt_ptr', 'imp_slot', 'imp', 'vt', 'glob_addr', 'fn_addr', 'fn_dyn', 'glob_dyn',
               'glob_addr_dyn', 'vt_dyn', 'at', 'import', 'original', 'original_address', 'key_vt', 'key_glob_addr',
               'key_fn_addr', 'key_address', 'exe_address_of', 'if', 'while', 'for', 'switch', 'return', 'sizeof', 'ptr'}


def arg_context(code, start, end):
    """(name, index) when code[start:end] is a whole argument of a call `name(...)` on the line, else None."""
    depth, i, commas = 0, start - 1, 0
    while i >= 0:
        c = code[i]
        if c in ')]}':
            depth += 1
        elif c in '([{':
            if depth:
                depth -= 1
            elif c == '(':
                break
            else:
                return None
        elif c == ',' and depth == 0:
            commas += 1
        elif c in ';?:' and depth == 0 and not (c == ':' and ':' in (code[i - 1:i], code[i + 1:i + 2])):
            return None
        i -= 1
    if i < 0:
        return None
    m = re.search(r'(?:(\w+)::)?(\w+)\s*(?:<[^()]*>)?\s*$', code[:i])
    if not m:
        return None
    seg = code[max(code.rfind(',', i, start), i) + 1:start]
    if re.sub(r'/\*.*?\*/', '', seg).strip():
        return None
    k, depth = 0, 0
    rest = code[end:]
    while k < len(rest) and not (rest[k] in ',)' and depth == 0):
        depth += {'(': 1, ')': -1}.get(rest[k], 0)
        k += 1
    if k >= len(rest) or re.sub(r'/\*.*?\*/', '', rest[:k]).strip():
        return None
    return (m.group(1) + '::' + m.group(2) if m.group(1) else m.group(2)), commas


def enclosing(code, start):
    """(bracket, name before it) of the innermost open bracket around code[start] on the line ('' at the top level);
    None when a '<' that may be a comparison cannot be told from a template's."""
    depth, i = 0, start - 1
    while i >= 0:
        c = code[i]
        if c in ')]}':
            depth += 1
        elif c in '([{':
            if depth:
                depth -= 1
            else:
                m = re.search(r'(\w+)\s*(?:<[^()]*>)?\s*$', code[:i])
                return c, m.group(1) if m else ''
        elif c == '>' and depth == 0 and code[i - 1:i] != '-' and code[i + 1:i + 2] != '=' and code[i - 1:i] not in '>=':
            # the end of a template argument list before the token (fn<...>(...) is handled by call_before): skip it
            d, j = 1, i - 1
            while j >= 0 and d:
                d += {'>': 1, '<': -1}.get(code[j], 0)
                j -= 1
            i = j
            continue
        elif c == '<' and depth == 0 and re.search(r'\w$', code[:i]) and code[i + 1:i + 2] not in '<=':
            if not re.search(r'\s$', code[:i]):
                return '<', ''   # a template argument list
        i -= 1
    return '', ''


def wrap_args(code, kinds, local, stats, where, qual, funcs):
    """Since batch 6-4: an exe address passed as a whole argument of a call of a helper with a DWORD parameter becomes
    the layer's value of it (glob_addr, vt, fn_addr; the literal itself in the DLL, so its code stays the same). Not at
    a key parameter (funcs), not into a macro (an all-caps name: the hook table rows), not into the layer's own calls."""
    name_re = re.compile(r'(?<![\w:])(%s)\b' % '|'.join([HEX] + sorted(map(re.escape, local), key=len, reverse=True)))
    out, pos = [], 0
    blank = blank_comments(code)
    for m in name_re.finditer(code):
        if blank[m.start()] != code[m.start()]:
            continue   # inside a string literal (inline assembly)
        a = addr_of(m.group(1), local)
        k = kinds.kind(a) if a is not None else None
        f = {'vtable': 'vt', 'data': 'glob_addr', 'code': 'fn_addr'}.get(k)
        if not f or layer_how(code[:m.start()]) == 'layer':
            continue
        ctx = arg_context(code, m.start(), m.end())
        if layer_how(code[:m.start()]) == 'at':
            # at<T>(A + offset): the address computed from the block's start (at<T>(A) alone was rewritten above)
            if ctx or f == 'fn_addr' or not re.match(r'\s*[-+]', code[m.end():]):
                continue
            what = 'as the base of at<>'
        elif ctx:
            base = ctx[0].split('::')[-1]
            if base in LAYER_NAMES or base.isupper() or ctx[1] in funcs.get(ctx[0] if ctx[0] in funcs else base, ()):
                continue
            what = 'in %s()' % ctx[0]
        else:
            # an operand of an expression (address arithmetic, a comparison): not inside a template argument list,
            # a macro's arguments or a subscript, and not a whole argument of a call (above)
            enc = enclosing(code, m.start())
            if enc is None or enc[0] and enc[0] in '<[{' or enc[0] == '(' and (enc[1].isupper() or enc[1] in LAYER_NAMES - {'if', 'while', 'for', 'switch', 'return'}):
                continue
            what = 'in an expression'
        stats['rewritten: argument ' + f] += 1
        stats.setdefault('_args', []).append('%s %s(%s) %s' % (where, f, m.group(1), what))
        out.append(code[pos:m.start()] + '%s(%s)' % (qual[f], m.group(1)))
        pos = m.end()
    out.append(code[pos:])
    return ''.join(out)


def addr_of(tok, local):
    if re.fullmatch(HEX, tok):
        return int(tok.rstrip('uUlL'), 16)
    return local.get(tok)


def rewrite_code(code, kinds, local, stats, where, qual, decl):
    out, i = [], 0
    pat = re.compile(r'(?<![\w:])(zax::)?(at|import)<')
    while True:
        m = pat.search(code, i)
        if not m:
            break
        lt = m.end() - 1
        gt = template_end(code, lt)
        if gt < 0 or gt + 1 >= len(code) or code[gt + 1] != '(':
            out.append(code[i:m.end()])
            i = m.end()
            continue
        close = code.find(')', gt + 2)
        tok = re.sub(r'/\*.*?\*/', '', code[gt + 2:close]).strip() if close > 0 else ''   # an address with a comment
        a = addr_of(tok, local)
        if a is None:
            out.append(code[i:m.end()])
            i = m.end()
            if re.fullmatch(r'\w+', tok) and m.group(2) == 'at':
                stats['at<> with a run-time address'] += 1
            continue
        k = kinds.kind(a)
        targ = code[lt + 1:gt]
        call = close + 1 < len(code) and code[close + 1] == '('
        new = None
        name = None
        if m.group(2) == 'import':
            if k == 'iat':
                name = 'imp'
        elif k == 'code':
            name = 'fn' if call else 'code'
        elif k in ('iat', 'vtable', 'data'):
            name = {'iat': 'imp_slot', 'vtable': 'vt_ptr', 'data': 'glob'}[k]
        if name:
            new = '%s<%s>(%s)' % ('zax::' + name if m.group(1) else qual[name], targ, code[gt + 2:close].strip())
        if new is None:
            stats['not rewritten: %s' % (k or 'outside the layer')] += 1
            stats.setdefault('_where', []).append('%s 0x%x %s' % (where, a, k))
            out.append(code[i:close + 1])
        else:
            stats['rewritten: ' + name] += 1
            out.append(code[i:m.start()] + new)
        i = close + 1
    out.append(code[i:])
    code = ''.join(out)

    # bare literals and (since batch 6-4) named constants as values: = A; == A; != A
    def bare(mm):
        a = addr_of(mm.group(2), local)
        k = kinds.kind(a)
        f = {'vtable': 'vt', 'data': 'glob_addr', 'code': 'fn_addr'}.get(k)
        if not f:
            return mm.group(0)
        stats['rewritten: ' + f] += 1
        return '%s%s(%s)' % (mm.group(1), qual[f], mm.group(2))
    # (since batch 6-4) a vtable as the first member of an aggregate built at run time: {A, ...}
    def first_member(mm):
        a = addr_of(mm.group(2), local)
        if kinds.kind(a) != 'vtable':
            return mm.group(0)
        stats['rewritten: vt'] += 1
        return '%s%s(%s)%s' % (mm.group(1), qual['vt'], mm.group(2), mm.group(3))
    if not decl and not re.search(r'\b(case|static_assert|#define)\b', code):
        alts = [HEX] + sorted((re.escape(n) for n in local), key=len, reverse=True)
        code = re.sub(r'((?:=|\w)\s*\{\s*)(?<![\w:])(%s)\b(\s*,)' % '|'.join(alts), first_member, code)
    if not decl and not re.search(r'\b(case|static_assert|#define)\b', code):
        alts = [HEX] + sorted((re.escape(n) for n in local), key=len, reverse=True)
        code = re.sub(r'((?<![=!<>+\-*/|&^])=\s*|[=!]=\s*)(?<![\w:])(%s)\b(?!\s*})' % '|'.join(alts), bare, code)
    return code


def production_files():
    return sorted(p for p in SRC.iterdir() if p.suffix in ('.cpp', '.h', '.inc') and exe_layout_role(p) == 'prod')


def exe_layout_role(p):
    b = p.name
    return 'check' if b.startswith('check') or '_check' in b or b == 'main.cpp' else 'prod'


WRAP_NAMES = {'fn', 'code', 'glob', 'vt_ptr', 'imp_slot', 'imp', 'fn_dyn', 'glob_dyn', 'glob_addr', 'fn_addr', 'vt', 'vt_dyn',
              'glob_addr_dyn', 'ZAX_ASM_DATA', 'ZAX_ASM_OFFSET', 'ZAX_ASM_PUSH',
              'fn_exe'}   # since batch 6-10b: the exe address of a reimplemented function as original<>'s key


def call_before(pre):
    """The name of the call whose opening parenthesis ends pre (only blanks after it), its template arguments skipped:
    `fn<void(__fastcall *)(const char *)>(0x4a2650)(at<const char *>(` gives `at`. Since batch 6-4; the regular
    expressions before it let a template argument list run over an earlier call on the line, so `at<>(A)` behind a
    `fn<...>(...)(` counted as a layer use."""
    i = len(pre.rstrip())
    if i == 0 or pre[i - 1] != '(':
        return None
    i -= 1
    while i > 0 and pre[i - 1] in ' \t':
        i -= 1
    if i > 0 and pre[i - 1] == '>':
        depth, j = 0, i - 1
        while j >= 0:
            c = pre[j]
            if c == '>':
                depth += 1
            elif c == '<':
                depth -= 1
                if depth == 0:
                    break
            j -= 1
        i = j
    m = re.search(r'(\w+)\s*$', pre[:max(i, 0)])
    return m.group(1) if m else None


def layer_how(pre):
    name = call_before(pre)
    return 'layer' if name in WRAP_NAMES else 'at' if name in ('at', 'import') else None




KEY_TYPES = ('FnKey', 'CodeKey', 'GlobKey', 'VtKey', 'ImpKey',
             'GlobKeyOr0')   # since batch 6-8: blitters_screen.cpp's data key that may be 0 (a member of ScreenMode)


def split_args(s):
    """The top-level comma-separated parts of an argument or parameter list (without its parentheses)."""
    parts, depth, cur = [], 0, ''
    for c in s:
        if c in '(<[{':
            depth += 1
        elif c in ')>]}':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += c
    parts.append(cur)
    return parts


def key_signatures(texts, base=None):
    """Since batch 6-4: the helpers with key parameters (reimpl.h's FnKey, CodeKey, GlobKey, VtKey, ImpKey) and the
    templates whose DWORD parameters reach the exe only through the layer: name -> the argument positions that are
    keys. A constant passed there goes through the layer (the exe build checks it), so the audit counts it as a layer
    use. A position counts only when every declaration of the name that has it agrees (overloads with a DWORD there do
    not count)."""
    funcs = collections.defaultdict(list)
    for text in texts:
        code = re.sub(r'//[^\n]*', '', text)
        for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(?:const\s*)?[{;]', code):
            params = split_args(m.group(2))
            if m.group(1) in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
                continue
            # a declaration: every parameter a type and a name (a call has values there)
            if not all(re.fullmatch(r'\s*(?:const\s+)?[A-Za-z_][\w:]*(?:<[^()]*>)?(?:\s*\.\.\.)?[\s*&]+(?:\.\.\.\s*)?[A-Za-z_]\w*'
                                    r'(?:\s*=[^,]*)?\s*|\s*(?:void)?\s*', x) for x in params):
                continue
            sig = [any(re.search(r'\b%s\b' % k, x) for k in KEY_TYPES) for x in params]
            funcs[m.group(1)].append(sig)
            ns = re.findall(r'\bnamespace\s+(?:\w+::)*(\w+)\s*\{', code[:m.start()])
            if ns:   # also by its namespace (`deflist::zs_format` when another header has a zs_format of its own)
                funcs[ns[-1] + '::' + m.group(1)].append(sig)
    out = {}
    for name, sigs in funcs.items():
        pos = set()
        for i in range(max(len(x) for x in sigs)):
            have = [x[i] for x in sigs if len(x) > i]
            if have and all(have):
                pos.add(i)
        if pos:
            out[name] = pos
    # templates: a DWORD parameter every use of which in the body is the key of a layer call, a key parameter of a
    # helper (the headers' too: since batch 6-7 the base's signatures are seen here) or a parameter of another such
    # template (repeated until nothing changes: `deleting_dtor<8, ItemAction_destroy<VT>>` inside a template of VT)
    known = dict(base[0], **out) if base else out
    base_templ = dict(base[1]) if base else {}
    templ = {}
    while True:
        found = {}
        seen_templ = dict(base_templ, **templ)
        for text in texts:
            code = re.sub(r'//[^\n]*', '', text)
            for m in re.finditer(r'template\s*<([^<>]*)>\s*', code):
                params = [x.strip() for x in split_args(m.group(1))]
                names = [re.fullmatch(r'(?:DWORD|unsigned|uint32_t)\s+(\w+)(?:\s*=.*)?', x) for x in params]
                if not any(names):
                    continue
                head = code[m.end():m.end() + 400]
                hm = re.match(r'(?:(?:inline|static|constexpr|struct|class|typename)\s+)*[\w:*&<>, ]*?\b(\w+)\s*(?:\(|\{|:(?!:))', head)
                if not hm:
                    continue
                b = code.find('{', m.end())
                semi = code.find(';', m.end())
                if b < 0 or 0 <= semi < b and '(' in code[m.end():semi]:
                    continue   # a declaration (the definition is found by itself)
                depth, e = 0, b
                while e < len(code):
                    if code[e] == '{':
                        depth += 1
                    elif code[e] == '}':
                        depth -= 1
                        if depth == 0:
                            break
                    e += 1
                body = code[b:e]
                pos = set()
                for i, nm in enumerate(names):
                    if not nm:
                        continue
                    occ = [x for x in re.finditer(r'\b%s\b' % nm.group(1), body)]
                    if occ and all(layer_how(body[:x.start()]) == 'layer' or key_position(body, x.start(), x.end(), known, seen_templ)
                                   for x in occ):
                        pos.add(i)
                if pos:
                    name = hm.group(1)
                    found[name] = found[name] & pos if name in found else pos
        found = {k: v for k, v in found.items() if v}
        if found == templ:
            break
        templ = found
    templ = {k: v for k, v in templ.items() if v}
    if base:   # the headers' signatures under the file's own (for its macros)
        out = dict(base[0], **out)
        templ = dict(base[1], **templ)
    # macros (hook table rows): a parameter every use of which in the body is at a key position
    for text in texts:
        for m in re.finditer(r'^#define\s+(\w+)\(([^)]*)\)((?:[^\n]*\\\n)*[^\n]*)', text, re.M):
            params = [x.strip() for x in m.group(2).split(',')]
            body = m.group(3).replace('\\\n', ' ')
            pos = set()
            for i, nm in enumerate(params):
                occ = list(re.finditer(r'(?<![\w#])%s\b' % re.escape(nm), body)) if re.fullmatch(r'\w+', nm) else []
                if occ and all(key_position(body, x.start(), x.end(), out, templ) for x in occ):
                    pos.add(i)
            if pos:
                out[m.group(1)] = pos
    return out, templ


def blank_comments(text):
    """The text with comments and the contents of string and character literals blanked (offsets and newlines kept)."""
    out, i, n = list(text), 0, len(text)
    while i < n:
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out[i:j] = ' ' * (j - i)
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out[i:j] = [c if c == '\n' else ' ' for c in text[i:j]]
            i = j
        elif text[i] in '"\'' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_') or text[i] == '"'):
            q, j = text[i], i + 1
            while j < n and text[j] != q and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out[i + 1:j] = ' ' * (j - i - 1)
            i = j + 1
        else:
            i += 1
    return ''.join(out)


def struct_members(texts):
    """Since batch 6-4: struct name -> the types of its data members in order (for the aggregates whose members are
    keys of the layer, e.g. a table of class specs with GlobKey members)."""
    out = {}
    for text in texts:
        code = blank_comments(text)
        for m in re.finditer(r'\bstruct\s+(\w+)\s*(?::[^{;]*)?\{', code):
            i, depth, body = m.end(), 1, []
            while i < len(code) and depth:
                c = code[i]
                if c == '{':
                    depth += 1
                elif c == '}':
                    depth -= 1
                elif depth == 1:
                    body.append(c)
                i += 1
            types = []
            for decl in ''.join(body).split(';'):
                d = decl.strip()
                if not d or '(' in d or d.startswith(('using', 'static', 'typedef', 'friend', 'enum', 'public', 'private')):
                    continue
                dm = re.match(r'(?:const\s+|volatile\s+)*([A-Za-z_][\w:]*(?:<[^<>]*>)?)\s*(.*)$', d, re.S)
                if not dm:
                    continue
                for x in split_args(dm.group(2)):
                    x = x.strip()
                    if not x:
                        continue
                    types.append(dm.group(1) if not x.startswith(('*', '&')) and '[' not in x else '?')
            out[m.group(1)] = types
    return out


def aggregate_key(full, start, end, structs):
    """Whether the token at full[start:end] is an element of a braced initializer at a member whose type is a key type
    (or an element of an array of a key type)."""
    def back(pos):
        """The enclosing '{' of pos and the number of elements before pos in it (or None)."""
        depth, i, commas = 0, pos - 1, 0
        while i >= 0:
            c = full[i]
            if c in ')]}':
                depth += 1
            elif c in '([{':
                if depth:
                    depth -= 1
                elif c == '{':
                    return i, commas
                else:
                    return None
            elif c == ';' and depth == 0:
                return None
            elif c == ',' and depth == 0:
                commas += 1
            i -= 1
        return None

    def type_of(brace):
        """('struct', T) / ('array', T) / None for the initializer that opens at brace."""
        pre = full[:brace].rstrip()
        m = re.search(r'([A-Za-z_][\w:]*)\s+[A-Za-z_]\w*\s*(\[[^\]]*\])?\s*=?$', pre)
        if m and (pre.endswith('=') or m.group(2) is None):
            return ('array' if m.group(2) is not None else 'struct', m.group(1).split('::')[-1])
        if pre.endswith(('{', ',')):
            parent = back(brace)
            if parent is None:
                return None
            pt = type_of(parent[0])
            if pt is None:
                return None
            if pt[0] == 'array':
                return ('struct', pt[1])
            members = structs.get(pt[1], [])
            if parent[1] < len(members):
                return ('struct', members[parent[1]].split('::')[-1])
        return None

    before = full[:start].rstrip()
    after = full[end:].lstrip()
    if not before.endswith(('{', ',')) or not after.startswith((',', '}')):
        return False
    b = back(start)
    if b is None:
        return False
    t = type_of(b[0])
    if t is None:
        return False
    if t[0] == 'array':
        return t[1] in KEY_TYPES
    members = structs.get(t[1], [])
    return b[1] < len(members) and members[b[1]] in KEY_TYPES


def key_position(code, start, end, funcs, templ):
    """Whether the token at code[start:end] is a whole argument of a call of a helper at a key position, or of a
    template instantiation at a parameter that only the layer reads."""
    def bare_part(s):
        return not re.sub(r'/\*.*?\*/', '', s).strip()
    depth, i, commas = 0, start - 1, 0
    while i >= 0:
        c = code[i]
        if c in ')]}':
            depth += 1
        elif c in '([{' and depth:
            depth -= 1
        elif c in '(<' and depth == 0:
            break
        elif c in '[{' and depth == 0:
            return False
        elif c == ',' and depth == 0:
            commas += 1
        elif c == '>' and depth == 0 and code[i - 1:i] != '-':
            # a nested template argument list closes before the token: skip it
            j = i - 1
            d = 1
            while j >= 0 and d:
                d += {'>': 1, '<': -1}.get(code[j], 0)
                j -= 1
            i = j
            continue
        i -= 1
    if i < 0:
        return False
    open_ = code[i]
    pre = code[:i]
    nm = re.search(r'(\w+)\s*(<[^()]*>)?\s*$' if open_ == '(' else r'(\w+)\s*$', pre)
    if not nm:
        return False
    # the argument is the token alone
    seg_start = max(code.rfind(',', i, start), i) + 1
    if not bare_part(code[seg_start:start]):
        return False
    rest = code[end:]
    k = 0
    depth = 0
    while k < len(rest) and not (rest[k] in ',)>' and depth == 0):
        depth += {'(': 1, ')': -1}.get(rest[k], 0)
        k += 1
    if not bare_part(rest[:k]):
        return False
    table = funcs if open_ == '(' else templ
    q = re.search(r'(\w+)::%s\s*(<[^()]*>)?\s*$' % nm.group(1), pre)
    if q and q.group(1) + '::' + nm.group(1) in table:
        return commas in table[q.group(1) + '::' + nm.group(1)]
    return commas in table.get(nm.group(1), ())


def file_signatures(p):
    """The key signatures (key_signatures, struct_members) a production file sees: the headers' for every file, a file's
    own definitions (a static helper of the same name) win; an .inc file sees those of the .cpp files that include it."""
    global _SIGS
    if _SIGS is None:
        headers = [q.read_text(encoding='latin-1') for q in sorted(SRC.rglob('*.h')) if exe_layout_role(q) == 'prod']
        _SIGS = key_signatures(headers) + (struct_members(headers),)
    own_text = p.read_text(encoding='latin-1')
    if p.name.endswith('.inc'):   # a hook table or another part of the production .cpp files that include it
        inc = '#include "%s"' % p.name
        own_text = ''.join(q.read_text(encoding='latin-1') for q in sorted(p.parent.glob('*.cpp'))
                           if exe_layout_role(q) == 'prod' and inc in q.read_text(encoding='latin-1')) + own_text
    defined = set(re.findall(r'\b(\w+)\s*\([^;{}()]*\)\s*(?:const\s*)?\{', re.sub(r'//[^\n]*', '', own_text)))
    funcs, templ = key_signatures([own_text], ({k: v for k, v in _SIGS[0].items() if k not in defined}, _SIGS[1]))
    return funcs, templ, dict(_SIGS[2], **struct_members([own_text]))


# Batch 6-5's rule (merged 2026-10-29): an address alone at a key-typed parameter of a function or named lambda
# the file or a header defines counts as through the layer (6-4's key_position covers helpers, not named lambdas).
FUNC_DEF = re.compile(r'^[ \t]*(?:\[\[\w+\]\]\s*)?(?:template\s*<[^;{]*?>\s*)?(?:[\w:]+(?:<[^;{()]*>)?[\s*&]+)+([A-Za-z_]\w*)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*(?:const\s*)?\{', re.M)
LAMBDA_DEF = re.compile(r'\bauto\s+([A-Za-z_]\w*)\s*=\s*\[[^\]]*\]\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)')


def split_top(s, sep=','):
    """s split at sep outside (), <>, [], {}."""
    parts, depth, cur = [], 0, ''
    for i, c in enumerate(s):
        if c in '<>' and (s[i - 1:i] in ('-', '<', '>', ' ') or s[i + 1:i + 2] in ('<', '>', '=', ' ')):
            pass   # ->, <<, >>, <=, a comparison: no template bracket
        elif c in '(<[{':
            depth += 1
        elif c in ')>]}':
            depth -= 1
        if c == sep and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += c
    parts.append(cur)
    return parts


def key_params(text):
    """The functions and named lambdas a source defines: name -> one set per definition (overload) with the positions
    of its key-typed parameters (FnKey, GlobKey, ...); a definition without one adds an empty set, so an argument
    of a name that has an overload without a key there counts as bare."""
    out = collections.defaultdict(list)
    code = re.sub(r'//[^\n]*', '', text)
    for m in list(FUNC_DEF.finditer(code)) + list(LAMBDA_DEF.finditer(code)):
        name, params = m.group(1), m.group(2)
        if name in ('if', 'while', 'for', 'switch', 'return', 'sizeof', 'case') or not params.strip():
            continue
        keys = frozenset(i for i, a in enumerate(split_top(params))
                         if re.match(r'\s*(?:const\s+)?(?:zax::)?(%s)\b' % '|'.join(KEY_TYPES), a))
        out[name].append(keys)
    return out


def key_argument(code, start, end, keys):
    """True when code[start:end] (an address) is alone a whole argument of a call on the line whose parameter at that
    place has a key type in every overload of the callee (FnKey, GlobKey, ...: the exe build's consteval key checks it
    and the helper reaches the address only through the layer)."""
    depth, i = 0, start - 1
    while i >= 0:
        c = code[i]
        if c in ')]}':
            depth += 1
        elif c in '([{':
            if depth == 0:
                break
            depth -= 1
        i -= 1
    if i < 0 or code[i] != '(':
        return False
    head = code[:i].rstrip()
    if head.endswith('>'):
        # since batch 6-10a: a template argument list with parentheses (a function type: api<HANDLE(WINAPI *)(DWORD)>)
        depth_t, j = 0, len(head) - 1
        while j >= 0:
            depth_t += {'>': 1, '<': -1}.get(head[j], 0)
            if depth_t == 0:
                break
            j -= 1
        head = head[:max(j, 0)]
    callee = re.search(r'([A-Za-z_]\w*)\s*$', head)
    if not callee or callee.group(1) not in keys:
        return False
    bare_text = lambda t: re.sub(r'/\*.*?\*/', '', t).strip()
    before = split_top(re.sub(r'/\*.*?\*/', '', code[i + 1:start]))
    tail = re.sub(r'/\*.*?\*/', '', code[end:])
    after = split_top(tail)[0]
    # alone: nothing else in the argument before it, and after it only a comment up to the call's ')' or the next ','
    # a namespace qualifier before a constant is part of it (iat::HeapAlloc; since batch 6-10a)
    if bare_text(re.sub(r'(?:\w+::)+\s*$', '', before[-1])) or (bare_text(after.split(')')[0]) if ')' in after else bare_text(after)):
        return False
    if ')' not in after and len(after) >= len(tail):
        return False   # the argument goes on in the next line
    pos = len(before) - 1
    sets = keys[callee.group(1)]
    return bool(sets) and all(pos in k for k in sets)


def uses(files, kinds, names):
    """Every use of an exe address in the files outside hook tables and comments: (file, line, address, how) with how
    `layer` (the key of a layer call, or since batch 6-4 a constant argument at a key parameter of a helper or of a
    template that reads it only through the layer, key_signatures), `at` (at<>/import<>) or `bare` (a literal or
    constant anywhere else: an argument of a helper with a DWORD parameter, a table, a number). The name a constant's
    definition defines is no use (until batch 6-4 it counted as bare, so every named constant kept its block)."""
    header_keys = collections.defaultdict(list)
    for h in sorted(SRC.glob('*.h')):
        for k, v in key_params(h.read_text(encoding='latin-1')).items():
            header_keys[k] += v
    for p in files:
        funcs, templ, structs = file_signatures(p)
        text = p.read_text(encoding='latin-1')
        keys = dict(header_keys)
        keys.update(key_params(text) if not p.name.endswith('.h') else {})
        full = blank_comments(text)
        line_starts = [0]
        for line in text.split('\n'):
            line_starts.append(line_starts[-1] + len(line) + 1)
        local = dict(names)
        if p.name.endswith('.inc'):   # since batch 6-7: a hook table sees the constants of the .cpp files that include it
            inc = '#include "%s"' % p.name
            for q in sorted(p.parent.glob('*.cpp')):
                qt = q.read_text(encoding='latin-1')
                if exe_layout_role(q) == 'prod' and inc in qt:
                    local.update(file_names(qt))
        local.update(file_names(text))
        name_re = re.compile(r'(?<![\w])(%s)\b' % '|'.join([HEX] + sorted(map(re.escape, local), key=len, reverse=True)))
        hooks = hook_line_flags(text)
        in_decl = False
        in_raw = False
        for no, line in enumerate(text.split('\n')):
            raw, in_raw = raw_string_state(line, in_raw)
            if raw:   # a line of a generated assembler module: its # comments are no uses
                line = line.split('#')[0] if not line.startswith('#') else ''
            code = line[:comment_start(line)]
            decl = in_decl or bool(re.search(r'\b(constexpr|const|enum)\b', code))
            in_decl = decl and ';' not in code and '{' not in code.split('=')[0] if decl else False
            for m in name_re.finditer(code):
                a = addr_of(m.group(1), local)
                if a is None or kinds.kind(a) is None:
                    continue
                if not raw and not m.group(1).startswith('0x') and code[:m.start()].replace('\\"', '').count('"') % 2:
                    continue   # a constant's name inside a string literal ("InitializeCriticalSection"; since batch 6-10a)
                if (hooks[no] or p.name.endswith('_hooks.inc')) and kinds.kind(a).startswith('code'):
                    continue   # a hook site; data in a hook table (a template argument) is a use

                after = re.sub(r'^(\s*/\*.*?\*/)*', '', code[m.end():]).lstrip()   # a /* "text" */ behind the literal
                if decl and after[:1] in (',', ';', '') and re.search(r'\b\w+\s*=\s*$', code[:m.start()]):
                    continue   # the definition of a constant: its uses count
                if not m.group(1).startswith('0x') and re.match(r'=(?!=)', after):
                    continue   # the name a definition defines
                pre = code[:m.start()]
                how = layer_how(pre) or 'bare'
                if how == 'bare' and (key_position(code, m.start(), m.end(), funcs, templ)
                                      or aggregate_key(full, line_starts[no] + m.start(), line_starts[no] + m.end(), structs)
                                      or key_argument(code, m.start(), m.end(), keys)):
                    how = 'layer'
                yield p, no + 1, a, how


_SIGS = None
# Since batch 6-10c every block is own and the standalone exe has no image of Zax.exe: its at<> translates an address of
# the exe's range at run time (reimpl.h, layer::translate), so a use spelled with at<> or as a number that reaches at<>,
# fn_dyn or glob_dyn is valid in both builds and no longer an error; the uses are still counted per file. What the exe
# still takes from the exe's addresses at run time shows as a fault in its inaccessible range (loader.cpp).
ALL_OWN = True


def audit(files, kinds, names, movable=False, pins=()):
    """Unconverted spellings per file; with movable the blocks every use of which goes through the layer."""
    errors = 0
    total = collections.Counter()
    per_file = collections.defaultdict(collections.Counter)
    block_uses = collections.defaultdict(collections.Counter)
    for p, no, a, how in uses(files, kinds, names):
        b = kinds.block(a)
        if b:
            block_uses[b['address']][how] += 1
        if how == 'layer':
            continue
        per_file[p.name][how] += 1
        if b and b['home'] == 'own' and not ALL_OWN:
            print('%s:%d: 0x%x is in the own block 0x%x but spelled %s' % (p.name, no, a, b['address'], 'at<>/import<>' if how == 'at'
                                                                         else 'without the layer'))
            errors += 1
    for name in sorted(per_file):
        print('%-32s %s' % (name, ', '.join('%s %d' % kv for kv in sorted(per_file[name].items()))))
        total.update(per_file[name])
    print('total not through the layer: %s' % ', '.join('%s %d' % kv for kv in sorted(total.items())))
    if movable:
        pinned = set()
        for a in pins:
            b = kinds.block(a)
            if b:
                pinned.add(b['address'])
        # the IAT too since batch 6-10a (until then it was never listed: the loader resolved the imports into the image's
        # slots, and the exe's import thunks and the launcher's /Cheats read them there; now the loader resolves through the
        # layer, points the thunks at our slots and mirrors the launcher's, loader_iat_handled)
        ok = [a for a, c in sorted(block_uses.items()) if c['layer'] and not c['at'] and not c['bare'] and a not in pinned]
        print('blocks every use of which goes through the layer: %d (%d bytes)' % (len(ok), sum(kinds.blocks[a]['size'] for a in ok)))
        for a in ok:
            print('movable 0x%x' % a)
    return errors


def loader_iat_handled():
    """The IAT reads of code outside the layer that loader.cpp handles (since batch 6-10a): the import thunks' range
    (IMPORT_THUNKS, IMPORT_THUNKS_SIZE: their jumps pointed at our own IAT) and the launcher's slots (LAUNCHER_IAT:
    mirrored into the image around the launcher's attach)."""
    loader = (ROOT / 'reimpl/standalone/loader.cpp').read_text(encoding='latin-1')
    m = re.search(r'IMPORT_THUNKS = (0x[0-9a-f]+), IMPORT_THUNKS_SIZE = (\d+);', loader)
    thunks = (int(m.group(1), 16), int(m.group(2))) if m else (0, 0)
    m = re.search(r'LAUNCHER_IAT\[\] = \{([^}]*)\}', loader)
    launcher = {int(x, 16) for x in re.findall(r'0x[0-9a-f]+', m.group(1))} if m else set()
    return thunks, launcher


def kept_code_data(exe, kinds):
    """The data the exe's own code still running in the standalone exe names by absolute address: the functions our
    code calls through original<> (the game menu's constructor when the launcher has patched it, the C runtime's locale
    paths), the rows of docs/not_reimplemented.csv that stay the exe's code and the extra ranges of loader.cpp's
    PoisonText list. A block they read cannot move while that code runs (found by the trial of batch 6-3: the menu's
    original constructor stored the image's vtable 0x605ab8)."""
    import capstone
    from capstone import x86
    data = exe.read_bytes()
    import struct
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    n, opt = struct.unpack_from('<HxxxxxxxxxxxxH', data, pe + 6)
    secs = []
    for i in range(n):
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, pe + 0x18 + opt + 40 * i + 8)
        secs.append((0x400000 + va, rsize, raw))

    def code_at(a, size):
        for va, rsize, raw in secs:
            if va <= a < va + rsize:
                return data[raw + a - va:raw + a - va + size]
        return b''
    sizes = {int(r['address'], 16): int(r['size'] or 0) for r in exe_layout.read_csv(ROOT / 'docs/functions.csv')}
    ranges = []
    for p in production_files():
        for m in re.finditer(r'original<[^;]*?>\(\s*(0x[0-9a-fA-F]{6})', p.read_text(encoding='latin-1')):
            a = int(m.group(1), 16)
            ranges.append((a, sizes.get(a, 0)))
    for r in exe_layout.read_csv(ROOT / 'docs/not_reimplemented.csv'):
        # since batch 6-4 not the rows that never run: `dead code` (no path reaches it: the Windows 9x small-block
        # heap behind __heap_select's dead branch, exp and the Pentium FDIV workaround of the x87 library) and
        # `covered by caller` (reached only from callers that are reimplemented whole: strtoxl, shortsort, swap, the
        # __finally funclets of reimplemented functions); they pinned the C runtime's data blocks 0x627814 and
        # 0x714568. PoisonText keeps their bytes, so if one ran after all it would read the poisoned image.
        if r['reason'] in ('identical by bytes', 'no function'):
            ranges.append((int(r['address'], 16), int(r['size'])))
    loader = (ROOT / 'reimpl/standalone/loader.cpp').read_text(encoding='latin-1')
    if 'const Range POISON_KEEP[]' not in loader:
        return []   # since batch 6-10c no code of the exe runs in the standalone exe
    keep = loader[loader.index('const Range POISON_KEEP[]'):]
    keep = keep[:keep.index('};')]
    for m in re.finditer(r'\{(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+|\d+)\}', keep):
        ranges.append((int(m.group(1), 16), int(m.group(2), 0)))
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    found = set()
    (thunks, thunks_size), _ = loader_iat_handled()
    for a, size in ranges:
        for insn in md.disasm(code_at(a, size), a):
            for op in insn.operands:
                v = op.mem.disp & 0xffffffff if op.type == x86.X86_OP_MEM else op.imm & 0xffffffff if op.type == x86.X86_OP_IMM else None
                if v is not None and kinds.kind(v) == 'iat' and thunks <= insn.address < thunks + thunks_size:
                    continue   # an import thunk the loader points at our own IAT
                if v is not None and kinds.kind(v) in ('data', 'vtable', 'iat'):
                    found.add(v)
    return sorted(found)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='*', type=Path)
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--audit', action='store_true')
    ap.add_argument('--where', action='store_true', help='list every use left unconverted with its file and line')
    ap.add_argument('--movable', action='store_true',
                    help='with --audit: the blocks whose every use in the production code (and the built-in launcher code) '
                         'goes through the layer, i.e. the ones whose home may become `own`')
    ap.add_argument('--src', type=Path, help='the source folder (default reimpl/src)')
    ap.add_argument('--exe', default=str(ROOT / 'gamedata/Zax - German/Zax.exe'),
                    help='with --movable: the exe whose code that still runs in the standalone exe is disassembled for the '
                         'data it reads (needs the venv: capstone)')
    args = ap.parse_args()
    global SRC
    if args.src:
        SRC = args.src.resolve()
    kinds = Kinds()
    names = header_names()
    if args.audit:
        # the launcher's DLL code is compiled into the exe as it is (batch 6-1): its addresses stay the image's, and
        # the import thunks (the exe's own code) read the image's IAT
        # until batch 6-10c the launcher's DLL code was compiled into the exe as it is and pinned its addresses; since
        # then standalone/launcher.cpp goes through the layer
        pins = []
        if args.movable:
            pins += kept_code_data(Path(args.exe), kinds)
        sys.exit(1 if audit(args.files or production_files(), kinds, names, args.movable, pins) else 0)
    if not args.files:
        ap.error('files to rewrite')
    stats = collections.Counter()
    for p in args.files:
        if exe_layout_role(p) != 'prod':
            print('%s: a check file (DLL only), left as it is' % p, file=sys.stderr)
            continue
        old = p.read_text(encoding='latin-1')
        new = rewrite(p, kinds, names, stats)
        if new == old:
            continue
        if args.write:
            p.write_text(new, encoding='latin-1')
        else:
            sys.stdout.writelines(difflib.unified_diff(old.splitlines(True), new.splitlines(True), str(p), str(p)))
    where = stats.pop('_where', [])
    args_list = stats.pop('_args', [])
    if args.where:
        print('\n'.join(args_list), file=sys.stderr)
    print('; '.join('%s %d' % kv for kv in sorted(stats.items())), file=sys.stderr)
    if args.where:
        print('\n'.join(where), file=sys.stderr)


if __name__ == '__main__':
    main()
