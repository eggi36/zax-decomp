// ZString reimplemented (see zstring.h).
#include "zstring.h"
#include "checks.h"
#include "exe_base.h"

namespace zax {

// 0x4dffb0
ZStringBuffer *THISCALL ZStringBuffer::Init(DWORD refs_)
{
    refs = refs_;
    capacity = 0;
    text[0] = 0;
    return this;
}

// 0x4dec40: the shared buffer of all empty strings. It starts with reference count 1, so it is never
// released as long as the count stays balanced. 0x4dec80 is its (empty) exit function.
ZStringBuffer *__cdecl ZString_EmptyBuffer()
{
    BYTE &guard = *glob<BYTE *>(ZSTRING_EMPTY_GUARD);
    auto *empty = glob<ZStringBuffer *>(ZSTRING_EMPTY);
    if (!(guard & 1)) {
        guard |= 1;
        empty->Init(1);
        crt_atexit(code<void(__cdecl *)()>(0x4dec80));
    }
    return empty;
}

static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

// strncpy: at most n characters, stops at a terminator and pads with zeros
static void copy_padded(char *d, const char *s, DWORD n)
{
    DWORD i = 0;
    for (; i < n && s[i]; i++)
        d[i] = s[i];
    for (; i < n; i++)
        d[i] = 0;
}

static void copy_bytes(char *d, const char *s, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        d[i] = s[i];
}

// Drop the reference to the buffer, free it when it was the last one (no null check, as in the
// original; only the destructor checks)
void ZString::release()
{
    if (--buffer->refs == 0)
        pool_free(buffer, buffer->capacity + 9);
}

// New own buffer of capacity n; the text stays uninitialized, the old buffer is not released
void ZString::alloc(DWORD n)
{
    buffer = static_cast<ZStringBuffer *>(pool_alloc(n + 9));
    buffer->refs = 1;
    buffer->capacity = n;
}

// Copy the text into a new own buffer of capacity n, then release the old one. The original goes
// through a temporary string (reference count 2, then 1); the pool sees the same calls.
void ZString::grow_copy(DWORD n)
{
    ZStringBuffer *old = buffer;
    alloc(n);
    copy_bytes(buffer->text, old->text, text_length(old->text) + 1);
    ZStringBuffer *mine = buffer;
    buffer = old;
    release();
    buffer = mine;
}

void ZString::set_empty()
{
    buffer = ZString_EmptyBuffer();
    buffer->refs++;
}

// Copy n characters with strncpy semantics. Writes in place if the buffer is not shared and big
// enough, otherwise into a new buffer of capacity n. The old buffer is released before copying, like
// in the original.
void ZString::set_text(const char *s, DWORD n)
{
    if (buffer->refs != 1 || buffer->capacity < n) {
        release();
        alloc(n);
    }
    copy_padded(buffer->text, s, n);
    buffer->text[n] = 0;
}

// 0x4dec90
ZString *THISCALL ZString::Ctor()
{
    run_due_checks();   // runs the checks in the game thread (checks.cpp)
    set_empty();
    return this;
}

// 0x4dedf0
ZString *THISCALL ZString::CopyCtor(const ZString *other)
{
    buffer = other->buffer;
    buffer->refs++;
    return this;
}

// 0x4defd0
void THISCALL ZString::Dtor()
{
    if (buffer) {
        release();
        buffer = nullptr;
    }
}

// 0x4e13a0
const char *THISCALL ZString::CStr() { return buffer->text; }

// 0x4e13b0
DWORD THISCALL ZString::Length()
{
    DWORD n = 0;
    while (buffer->text[n])
        n++;
    return n;
}

// 0x4e13d0 / 0x4e13e0
bool THISCALL ZString::IsEmpty() { return buffer->text[0] == 0; }
bool THISCALL ZString::NotEmpty() { return buffer->text[0] != 0; }

// 0x4e18f0
ZString *THISCALL ZString::Assign(const ZString *other)
{
    if (other != this) {
        release();
        buffer = other->buffer;
        buffer->refs++;
    }
    return this;
}

// 0x4e1930: null and "" both give the shared empty string
ZString *THISCALL ZString::AssignText(const char *s)
{
    DWORD n = 0;
    if (s)
        while (s[n])
            n++;
    if (!n) {
        release();
        set_empty();
        return this;
    }
    set_text(s, n);
    return this;
}

// 0x4df160: the first n characters of s (fewer if s ends earlier, the capacity is still n). Returns
// nothing: eax holds whatever the last call left there.
void THISCALL ZString::AssignTextN(const char *s, DWORD n)
{
    if (!s || !n) {
        release();
        set_empty();
        return;
    }
    set_text(s, n);
}


// 0x4dece0: like Ctor, but returns nothing (eax holds the reference count)
void THISCALL ZString::InitEmpty() { set_empty(); }

// 0x4dee00: null gives the shared empty string, "" a buffer of its own with capacity 0
ZString *THISCALL ZString::CtorText(const char *s)
{
    if (!s) {
        set_empty();
        return this;
    }
    DWORD n = text_length(s);
    alloc(n);
    copy_padded(buffer->text, s, n);
    buffer->text[n] = 0;
    return this;
}

// 0x4deeb0: the first n characters of s (strncpy, capacity n)
ZString *THISCALL ZString::CtorTextN(const char *s, DWORD n)
{
    if (!n) {
        set_empty();
        return this;
    }
    AllocCopy(s, n);
    return this;
}

// 0x4def50: a + b with given lengths (for operator+), always a buffer of its own
ZString *THISCALL ZString::CtorConcat(const char *a, DWORD na, const char *b, DWORD nb)
{
    alloc(na + nb);
    copy_bytes(buffer->text, a, na);
    copy_bytes(buffer->text + na, b, nb);
    buffer->text[na + nb] = 0;
    return this;
}

// 0x4ded30: make the string empty unless it has no buffer or already is the shared empty one
void THISCALL ZString::Empty()
{
    if (!buffer)
        return;
    if (buffer == ZString_EmptyBuffer())
        return;
    release();
    set_empty();
}

// 0x4df010
void THISCALL ZString::SetText(const char *s) { AssignText(s); }

// 0x4df270
void THISCALL ZString::SetString(const ZString *other) { Assign(other); }

// 0x4df2b0
void THISCALL ZString::Clear()
{
    release();
    set_empty();
}

// Second half of SetPath/SetPath3: the string holds dir already. Appends '/' unless it is empty or
// ends with one, then the name. Buffers grow to exactly the needed size.
void ZString::join_path(const char *name, DWORD nname)
{
    DWORD len = text_length(buffer->text);
    if (!len || buffer->text[len - 1] == '/') {
        Reserve(len + nname);
    } else {
        if (buffer->capacity < len + nname + 1 || buffer->refs > 1)
            grow_copy(len + nname + 1);
        buffer->text[len] = '/';
        len++;
    }
    copy_bytes(buffer->text + len, name, nname);
    buffer->text[len + nname] = 0;
}

// 0x4dfa50: dir + '/' + name. First makes room for both (own buffer of capacity ndir + nname + 1),
// then assigns dir like AssignText, which writes into that room.
void THISCALL ZString::SetPath(const char *dir, const char *name)
{
    DWORD ndir = text_length(dir), nname = text_length(name), total = ndir + nname + 1;
    if (buffer->capacity < total || buffer->refs > 1) {
        release();
        alloc(total);
    }
    if (!dir || !ndir) {
        release();
        set_empty();
    } else {
        set_text(dir, ndir);
    }
    join_path(name, nname);
}

// 0x4e02c0: dir + '/' + name + suffix (suffix without a separator, e.g. an extension)
void THISCALL ZString::SetPath3(const char *dir, const char *name, const char *suffix)
{
    DWORD ndir = text_length(dir), nname = text_length(name), nsuffix = text_length(suffix);
    DWORD total = nsuffix + nname + ndir + 1;
    if (buffer->capacity < total || buffer->refs > 1) {
        release();
        alloc(total);
    }
    if (!dir || !ndir) {
        release();
        set_empty();
    } else {
        set_text(dir, ndir);
    }
    join_path(name, nname);
    if (nsuffix) {
        DWORD len = text_length(buffer->text);
        if (buffer->capacity < len + nsuffix || buffer->refs > 1)
            grow_copy(len + nsuffix);
        copy_bytes(buffer->text + len, suffix, nsuffix);
        buffer->text[len + nsuffix] = 0;
    }
}

// 0x4e1be0: null and "" change nothing
void THISCALL ZString::Append(const char *s)
{
    if (s)
        AppendTextN(s, text_length(s));
}

// 0x4df330
void THISCALL ZString::AppendText(const char *s) { AppendTextN(s, text_length(s)); }

// 0x4df440: copies n bytes as they are, even past a terminator in s
void THISCALL ZString::AppendTextN(const char *s, DWORD n)
{
    if (!n)
        return;
    DWORD len = text_length(buffer->text);
    if (buffer->capacity < len + n || buffer->refs > 1)
        grow_copy(len + n);
    copy_bytes(buffer->text + len, s, n);
    buffer->text[len + n] = 0;
}

// 0x4df540: n times c
void THISCALL ZString::AppendRepeat(char c, DWORD n)
{
    if (!n)
        return;
    DWORD len = text_length(buffer->text);
    if (buffer->capacity < len + n || buffer->refs > 1)
        grow_copy(len + n);
    for (DWORD i = 0; i < n; i++)
        buffer->text[len + i] = c;
    buffer->text[len + n] = 0;
}

// 0x4e1ab0: the source buffer is taken before growing, so appending a string to itself reads the
// old buffer (released already if it was the only reference), as in the original
void THISCALL ZString::AppendString(const ZString *other)
{
    const char *src = other->buffer->text;
    DWORD n = text_length(src);
    if (!n)
        return;
    DWORD len = text_length(buffer->text);
    if (buffer->capacity < len + n || buffer->refs > 1)
        grow_copy(len + n);
    copy_bytes(buffer->text + len, src, n);
    buffer->text[len + n] = 0;
}

// 0x4e1d10
void THISCALL ZString::AppendChar(char c)
{
    DWORD len = text_length(buffer->text);
    if (buffer->capacity < len + 1 || buffer->refs > 1)
        grow_copy(len + 1);
    buffer->text[len] = c;
    buffer->text[len + 1] = 0;
}

// 0x4e0d30: s in front of position pos. Even an empty s makes a shared buffer its own.
void THISCALL ZString::Insert(const char *s, DWORD pos)
{
    DWORD n = text_length(s);
    DWORD len = text_length(buffer->text);
    if (buffer->capacity < len + n || buffer->refs > 1)
        grow_copy(len + n);
    char *at_pos = buffer->text + pos;
    // memmove of the rest including the terminator, backwards because the target lies behind
    for (DWORD i = text_length(at_pos) + 1; i-- > 0;)
        at_pos[n + i] = at_pos[i];
    copy_bytes(at_pos, s, n);
}

// 0x4df650
void THISCALL ZString::AppendPathName(const char *name)
{
    if (name) {
        join_path(name, text_length(name));
        return;
    }
    DWORD len = text_length(buffer->text);
    if (!len || buffer->text[len - 1] == '/') {
        Reserve(len);
        buffer->text[len] = 0;
        return;
    }
    if (buffer->capacity < len + 1 || buffer->refs > 1)
        grow_copy(len + 1);
    buffer->text[len] = '/';
    buffer->text[len + 1] = 0;
}

// 0x4df880
void THISCALL ZString::AppendPathN(const char *name, DWORD n) { join_path(name, n); }

// 0x4e0e60
void THISCALL ZString::GrowTo(DWORD n)
{
    if (buffer->capacity < n)
        grow_copy(n);
}

// Behind the text: '/' unless it is empty or ends with one, the dash (0x60f634, "-"), then the name
void ZString::append_dash_name(const ZString &dirpart, const ZString &name)
{
    join_path(dirpart.buffer->text, text_length(dirpart.buffer->text));
    DWORD len = text_length(buffer->text);
    if (len && buffer->text[len - 1] != '/') {
        Reserve(len + 2);
        buffer->text[len++] = '/';
    } else {
        Reserve(len + 1);
    }
    buffer->text[len] = *glob<const char *>(0x60f634);
    buffer->text[len + 1] = 0;
    AppendTextN(name.buffer->text, text_length(name.buffer->text));
}

// 0x4dffd0: e.g. ("Save", "Levels/a.zax", ".bak") gives "Save/Levels/-a.zax.bak". Like SetPath it
// first makes room for everything; dir may be null only in theory (its length is taken first).
void THISCALL ZString::SetDashPath(const char *dir, const char *file, const char *suffix)
{
    DWORD ndir = text_length(dir), nfile = text_length(file), nsuffix = suffix ? text_length(suffix) : 0;
    DWORD total = nsuffix + nfile + ndir + 2;
    if (buffer->capacity < total || buffer->refs > 1) {
        release();
        alloc(total);
    }
    ZString dirpart, name;
    dirpart.set_empty();
    name.set_empty();
    const char *p = path_name(file);
    name.AssignTextN(p, text_length(p));
    if (p > file) {
        dirpart.AssignTextN(file, p - file - 1);
    } else {
        dirpart.release();
        dirpart.set_empty();
    }
    if (!dir || !ndir) {
        release();
        set_empty();
    } else {
        set_text(dir, ndir);
    }
    append_dash_name(dirpart, name);
    if (nsuffix) {
        DWORD len = text_length(buffer->text);
        Reserve(len + nsuffix);
        copy_bytes(buffer->text + len, suffix, nsuffix);
        buffer->text[len + nsuffix] = 0;
    }
    name.release();
    dirpart.release();
}

// 0x4dfc70: like SetDashPath with the path sub ('/' + sub) between name and suffix
void THISCALL ZString::SetDashPathSub(const char *dir, const char *file, const char *sub, const char *suffix)
{
    DWORD ndir = text_length(dir), nfile = text_length(file), nsub = text_length(sub);
    DWORD nsuffix = suffix ? text_length(suffix) : 0;
    DWORD total = nsuffix + nsub + nfile + 1 + ndir + 2;
    if (buffer->capacity < total || buffer->refs > 1) {
        release();
        alloc(total);
    }
    ZString dirpart, name;
    dirpart.set_empty();
    name.set_empty();
    const char *p = path_name(file);
    name.AssignText(p);
    if (p > file) {
        dirpart.AssignTextN(file, p - file - 1);
    } else {
        dirpart.release();
        dirpart.set_empty();
    }
    if (!dir || !ndir) {
        release();
        set_empty();
    } else {
        set_text(dir, ndir);
    }
    append_dash_name(dirpart, name);
    join_path(sub, nsub);
    if (nsuffix)
        AppendTextN(suffix, nsuffix);
    name.release();
    dirpart.Dtor();
}

// Common part of SplitPath/SplitPathText. Both outputs are optional; the directory is taken from
// path as it was before the name was assigned (they may be the same string).
void ZString::split(const char *path, ZString *dir, ZString *name)
{
    const char *p = path_name(path);
    if (name) {
        DWORD n = text_length(p);
        if (!n) {
            name->release();
            name->set_empty();
        } else {
            name->set_text(p, n);
        }
    }
    if (!dir)
        return;
    DWORD n = path < p ? p - path - 1 : 0;
    if (path && n) {
        dir->set_text(path, n);
    } else {
        dir->release();
        dir->set_empty();
    }
}

// 0x4e0570 (this in ecx, dir in edx, name on the stack)
void __fastcall ZString::SplitPath(const ZString *path, ZString *dir, ZString *name)
{
    split(path->buffer->text, dir, name);
}

// 0x4e0770: a null path gives the name "In Path Name Null!" and an empty directory
void __fastcall ZString::SplitPathText(const char *path, ZString *dir, ZString *name) { split(path, dir, name); }

// Own buffer of the same capacity for editing in place
void ZString::make_own()
{
    if (buffer->refs > 1)
        grow_copy(buffer->capacity);
}

// 0x4e0f20
void THISCALL ZString::TrimRight()
{
    make_own();
    text_trim_right(buffer->text);
}

// 0x4e10f0
void THISCALL ZString::TrimLeft()
{
    make_own();
    text_trim_left(buffer->text);
}

// 0x4e1000
void THISCALL ZString::KeepChars(const char *chars)
{
    make_own();
    text_keep_chars(buffer->text, chars);
}

// 0x4e11d0
void THISCALL ZString::MakeUpper()
{
    make_own();
    text_upper(buffer->text);
}

// 0x4e12b0
void THISCALL ZString::ReplaceChar(char from, char to)
{
    make_own();
    text_replace_char(buffer->text, from, to);
}

// 0x4e1880: the old text is discarded
void THISCALL ZString::SetHex(const void *data, DWORD n)
{
    if (buffer->capacity < 2 * n || buffer->refs > 1) {
        release();
        alloc(2 * n);
    }
    text_hex(buffer->text, data, n);
}

// 0x585490: upper bound of the length of a printf result. Literal characters count 1, %d/%i/%u 32,
// floating point 64, %p 10, %c and %% 1, %s its length (cut to the precision), '#' adds 2, then at
// least the width. Characters it does not know (x, o, l, h, n, X and even the terminator) count as
// flags, so the specification goes on behind them; %C consumes an argument without moving on and
// never ends. All of this as in the original.
DWORD __fastcall FormatLength(const char *p, const DWORD *args)
{
    DWORD total = 0;
    if (!*p)
        return 0;
    do {
        if (*p != '%') {
            total++;
            p++;
            continue;
        }
        p++;
        DWORD n = 0, precision = 0, width = 0;
        bool has_precision = false, has_width = false, after_dot = false;
        for (;;) {
            char c = *p;
            switch (c) {
            case ' ': case '+': case '-': case '0': p++; continue;
            case '#': n += 2; p++; continue;
            case '.': after_dot = true; p++; continue;
            case 'C':   // the original consumes arguments here forever without moving on
                for (;;)
                    __asm__ volatile("");
            case '%': n += 1; break;
            case 'c': args++; n += 1; break;
            case 'E': case 'G': case 'e': case 'f': case 'g': args += 2; n += 0x40; break;
            case 'S': args++; break;
            case 'd': case 'i': case 'u': args++; n += 0x20; break;
            case 'p': args++; n += 10; break;
            case 's':
                n += text_length(reinterpret_cast<const char *>(*args++));
                if (has_precision && n > precision)
                    n = precision;
                break;
            default:
                if ((c >= '0' && c <= '9') || c == '*') {
                    DWORD v;
                    if (c == '*') {
                        v = *args++;
                        p++;
                    } else {
                        v = (DWORD)crt_atoi(p);
                        do
                            p++;
                        while (*p >= '0' && *p <= '9');
                    }
                    if (after_dot)
                        has_precision = true, precision = v;
                    else
                        has_width = true, width = v;
                } else {
                    p++;
                }
                continue;
            }
            break;
        }
        p++;
        if (has_width && n < width)
            n = width;
        total += n;
    } while (*p);
    return total;
}

// 0x4e09b0. With length 0 the text is written into the current buffer even if it is shared.
void __cdecl ZString::Format(ZString *z, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    DWORD n = FormatLength(fmt, reinterpret_cast<const DWORD *>(ap));
    if (n && (z->buffer->capacity < n || z->buffer->refs > 1)) {
        z->release();
        z->alloc(n);
    }
    crt_vsprintf(z->buffer->text, fmt, ap);
    va_end(ap);
}

// 0x4e0a30
void __cdecl ZString::AppendFormat(ZString *z, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    DWORD len = text_length(z->buffer->text);
    DWORD total = FormatLength(fmt, reinterpret_cast<const DWORD *>(ap)) + len;
    if (z->buffer->capacity < total || z->buffer->refs > 1)
        z->grow_copy(total);
    crt_vsprintf(z->buffer->text + len, fmt, ap);
    va_end(ap);
}

static const char *time_text(DWORD global) { return at<ZString *>(global)->c_str(); }

// 0x4e0b30. Computed on the FPU stack like the original; the factors are the float constants
// 1/3600 and 1/60, cut with __ftol.
void THISCALL ZString::SetTimeHMS(float seconds)
{
    int hours = x87_ftol((long double)seconds * (1.0f / 3600));
    long double rest = seconds - (long double)(hours * 3600);
    int minutes = x87_ftol(rest * (1.0f / 60));
    int secs = x87_ftol(rest - (long double)(minutes * 60));
    Format(this, "%02d%s%02d%s%02d", hours, time_text(glob_addr(TIME_SEP_HOURS)), minutes, time_text(glob_addr(TIME_SEP_MINUTES)), secs);
}

// 0x4e0bc0. Over a day "1d 2:03min", over an hour "2:03:04s", otherwise "3:04" with the seconds
// rounded up from more than 0.50 (can give 60).
void THISCALL ZString::SetDuration(float seconds)
{
    int days = x87_ftol((long double)seconds * (1.0f / 86400));
    long double rest = seconds - (long double)(days * 86400);
    int hours = x87_ftol(rest * (1.0f / 3600));
    rest -= hours * 3600;
    int minutes = x87_ftol(rest * (1.0f / 60));
    float sec_part = (float)(rest - (long double)(minutes * 60));
    int secs = x87_ftol(sec_part);
    if (!x87_le(seconds, 86400.0f)) {
        Format(this, "%d%s %d%s%02d%s", days, time_text(glob_addr(TIME_SUFFIX_DAYS)), hours, time_text(glob_addr(TIME_SEP_HOURS)), minutes,
               time_text(glob_addr(TIME_SUFFIX_MINUTES)));
    } else if (!x87_le(seconds, 3600.0f)) {
        Format(this, "%d%s%02d%s%02d%s", hours, time_text(glob_addr(TIME_SEP_HOURS)), minutes, time_text(glob_addr(TIME_SEP_MINUTES)), secs,
               time_text(glob_addr(TIME_SUFFIX_SECONDS)));
    } else {
        int hundredths = x87_ftol(((long double)sec_part - secs) * 100.0f);
        Format(this, "%d%s%02d", minutes, time_text(glob_addr(TIME_SEP_MINUTES)), secs + (hundredths > 50));
    }
}

// 0x4e1400: the text from start on, into the unconstructed string out. No check of start.
ZString *THISCALL ZString::Mid(ZString *out, DWORD start)
{
    const char *text = buffer->text;
    DWORD count = text_length(text) - start;
    if (!count) {
        out->set_empty();
        return out;
    }
    out->AllocCopy(text + start, count);
    return out;
}

// 0x4e14c0: count characters from start on, cut at the end of the text
ZString *THISCALL ZString::MidN(ZString *out, DWORD start, DWORD count)
{
    const char *text = buffer->text;
    DWORD len = text_length(text);
    if (len < start + count)
        count = len - start;
    if (!count) {
        out->set_empty();
        return out;
    }
    out->AllocCopy(text + start, count);
    return out;
}

// 0x4e1590: own buffer with room for at least n characters, text kept
char *THISCALL ZString::GetBuffer(DWORD n)
{
    DWORD cap = buffer->capacity;
    if (n < cap)
        n = cap;
    if (cap < n || buffer->refs > 1)
        grow_copy(n);
    return buffer->text;
}

// 0x4e1660: like GetBuffer, but a new buffer is not initialized
char *THISCALL ZString::GetBufferDiscard(DWORD n)
{
    DWORD cap = buffer->capacity;
    if (n < cap)
        n = cap;
    if (cap < n || buffer->refs > 1) {
        release();
        alloc(n);
    }
    return buffer->text;
}

// 0x4e16d0: end the text at n (-1: at its terminator)
void THISCALL ZString::ReleaseBuffer(int n)
{
    if (n == -1) {
        n = (int)text_length(buffer->text);
        if (n == -1)
            return;
    }
    buffer->text[n] = 0;
}

// strcmp with -1/0/1 (unsigned characters)
static int compare_bytes(const char *a, const char *b)
{
    const BYTE *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    for (;; x++, y++) {
        if (*x != *y)
            return *x < *y ? -1 : 1;
        if (!*x)
            return 0;
    }
}

// 0x4e1700
int THISCALL ZString::Compare(const ZString *other)
{
    if (!buffer->text[0])
        return other->buffer->text[0] ? -1 : 0;
    if (!other->buffer->text[0])
        return 1;
    return compare_bytes(buffer->text, other->buffer->text);
}

// 0x4e1770: null counts as empty
int THISCALL ZString::CompareText(const char *s)
{
    if (!buffer->text[0])
        return s && *s ? -1 : 0;
    if (!s)
        return 1;
    return compare_bytes(buffer->text, s);
}

// 0x4e17e0
int THISCALL ZString::CompareNoCase(const ZString *other)
{
    if (!buffer->text[0])
        return other->buffer->text[0] ? -1 : 0;
    if (!other->buffer->text[0])
        return 1;
    return crt_stricmp(buffer->text, other->buffer->text);
}

// 0x4e1830
int THISCALL ZString::CompareNoCaseText(const char *s)
{
    if (!buffer->text[0])
        return s && *s ? -1 : 0;
    if (!s || !*s)
        return 1;
    return crt_stricmp(buffer->text, s);
}

// 0x4e1aa0: no range check
char THISCALL ZString::GetAt(DWORD i) { return buffer->text[i]; }

// 0x4e1e00
void THISCALL ZString::Reserve(DWORD n)
{
    if (buffer->capacity < n || buffer->refs > 1)
        grow_copy(n);
}

// 0x4e1ec0
void THISCALL ZString::Release() { release(); }

// 0x4e1ef0
void THISCALL ZString::AllocBuffer(DWORD n) { alloc(n); }

// 0x4e1f20
void THISCALL ZString::AllocCopy(const char *s, DWORD n)
{
    alloc(n);
    copy_padded(buffer->text, s, n);
    buffer->text[n] = 0;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(zstring)
{
    const Hook list[] = {
        {0x4dffb0, ptr(&ZStringBuffer::Init), "ZStringBuffer::Init", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x89, 0x08}},
        {0x4dec40, ptr(&ZString_EmptyBuffer), "ZString_EmptyBuffer", 6, {0x8a, 0x0d, 0x08, 0x0f, 0x6c, 0x00, 0xb8, 0x01}},
        {0x4dec90, ptr(&ZString::Ctor), "ZString::Ctor", 9, {0x56, 0x8b, 0xf1, 0x8a, 0x0d, 0x08, 0x0f, 0x6c}},
        {0x4dedf0, ptr(&ZString::CopyCtor), "ZString::CopyCtor", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x11}},
        {0x4defd0, ptr(&ZString::Dtor), "ZString::Dtor", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0x85, 0xc0, 0x74}},
        {0x4e13a0, ptr(&ZString::CStr), "ZString::CStr", 5, {0x8b, 0x01, 0x83, 0xc0, 0x08, 0xc3, 0x90, 0x90}},
        {0x4e13b0, ptr(&ZString::Length), "ZString::Length", 6, {0x57, 0x8b, 0x39, 0x83, 0xc7, 0x08, 0x83, 0xc9}},
        {0x4e13d0, ptr(&ZString::IsEmpty), "ZString::IsEmpty", 5, {0x8b, 0x01, 0x8a, 0x48, 0x08, 0x84, 0xc9, 0x0f}},
        {0x4e13e0, ptr(&ZString::NotEmpty), "ZString::NotEmpty", 5, {0x8b, 0x01, 0x8a, 0x48, 0x08, 0x84, 0xc9, 0x0f}},
        {0x4e18f0, ptr(&ZString::Assign), "ZString::Assign", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x4e1930, ptr(&ZString::AssignText), "ZString::AssignText", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x33, 0xdb}},
        {0x4df160, ptr(&ZString::AssignTextN), "ZString::AssignTextN", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x33, 0xdb}},
        {0x4dece0, ptr(&ZString::InitEmpty), "ZString::InitEmpty", 9, {0x56, 0x8b, 0xf1, 0x8a, 0x0d, 0x08, 0x0f, 0x6c}},
        {0x4dee00, ptr(&ZString::CtorText), "ZString::CtorText", 5, {0x55, 0x8b, 0x6c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4deeb0, ptr(&ZString::CtorTextN), "ZString::CtorTextN", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0xf1}},
        {0x4def50, ptr(&ZString::CtorConcat), "ZString::CtorConcat", 5,
         {0x8b, 0x44, 0x24, 0x10, 0x53, 0x55, 0x8b, 0x6c}},
        {0x4ded30, ptr(&ZString::Empty), "ZString::Empty", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x39, 0x1e}},
        {0x4df010, ptr(&ZString::SetText), "ZString::SetText", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x33, 0xdb}},
        {0x4df270, ptr(&ZString::SetString), "ZString::SetString", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x4df2b0, ptr(&ZString::Clear), "ZString::Clear", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0x8b, 0x10, 0x4a}},
        {0x4dfa50, ptr(&ZString::SetPath), "ZString::SetPath", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x4e02c0, ptr(&ZString::SetPath3), "ZString::SetPath3", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x10}},
        {0x4e09b0, ptr(&ZString::Format), "ZString::Format", 5, {0x8b, 0x4c, 0x24, 0x08, 0x56, 0x57, 0x8d, 0x54}},
        {0x4e0a30, ptr(&ZString::AppendFormat), "ZString::AppendFormat", 6,
         {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x57}},
        {0x4e1400, ptr(&ZString::Mid), "ZString::Mid", 5, {0x8b, 0x11, 0x53, 0x56, 0x57, 0x8d, 0x7a, 0x08}},
        {0x4e14c0, ptr(&ZString::MidN), "ZString::MidN", 5, {0x8b, 0x11, 0x55, 0x56, 0x57, 0x8d, 0x7a, 0x08}},
        {0x4e1590, ptr(&ZString::GetBuffer), "ZString::GetBuffer", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x8b, 0x74, 0x24}},
        {0x4e1660, ptr(&ZString::GetBufferDiscard), "ZString::GetBufferDiscard", 8,
         {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x4e16d0, ptr(&ZString::ReleaseBuffer), "ZString::ReleaseBuffer", 6,
         {0x8b, 0xd1, 0x8b, 0x4c, 0x24, 0x04, 0x83, 0xf9}},
        {0x4e1700, ptr(&ZString::Compare), "ZString::Compare", 6, {0x8b, 0x09, 0x56, 0x8d, 0x41, 0x08, 0x8a, 0x49}},
        {0x4e1770, ptr(&ZString::CompareText), "ZString::CompareText", 6,
         {0x8b, 0x09, 0x56, 0x8d, 0x41, 0x08, 0x8a, 0x49}},
        {0x4e17e0, ptr(&ZString::CompareNoCase), "ZString::CompareNoCase", 5,
         {0x8b, 0x09, 0x8d, 0x41, 0x08, 0x8a, 0x49, 0x08}},
        {0x4e1830, ptr(&ZString::CompareNoCaseText), "ZString::CompareNoCaseText", 5,
         {0x8b, 0x09, 0x8d, 0x41, 0x08, 0x8a, 0x49, 0x08}},
        {0x4e1aa0, ptr(&ZString::GetAt), "ZString::GetAt", 6, {0x8b, 0x01, 0x8b, 0x4c, 0x24, 0x04, 0x8a, 0x44}},
        {0x4e1be0, ptr(&ZString::Append), "ZString::Append", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x57, 0x8b, 0x7c}},
        {0x4e1e00, ptr(&ZString::Reserve), "ZString::Reserve", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x8b, 0x74, 0x24}},
        {0x4e1ec0, ptr(&ZString::Release), "ZString::Release", 5, {0x8b, 0x01, 0x8b, 0x10, 0x4a, 0x89, 0x10, 0x8b}},
        {0x4e1ef0, ptr(&ZString::AllocBuffer), "ZString::AllocBuffer", 6,
         {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x4e1f20, ptr(&ZString::AllocCopy), "ZString::AllocCopy", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0xf1}},
        {0x4df330, ptr(&ZString::AppendText), "ZString::AppendText", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xd9, 0x57}},
        {0x4df440, ptr(&ZString::AppendTextN), "ZString::AppendTextN", 7, {0x51, 0x53, 0x56, 0x8b, 0x74, 0x24, 0x14, 0x8b}},
        {0x4df540, ptr(&ZString::AppendRepeat), "ZString::AppendRepeat", 7, {0x51, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x14, 0x8b}},
        {0x4df650, ptr(&ZString::AppendPathName), "ZString::AppendPathName", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4df880, ptr(&ZString::AppendPathN), "ZString::AppendPathN", 5, {0x51, 0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0x83}},
        {0x4dfc70, ptr(&ZString::SetDashPathSub), "ZString::SetDashPathSub", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x8b, 0xe9}},
        {0x4dffd0, ptr(&ZString::SetDashPath), "ZString::SetDashPath", 5, {0x83, 0xec, 0x0c, 0x33, 0xc0, 0x53, 0x55, 0x56}},
        {0x4e0570, ptr(&ZString::SplitPath), "ZString::SplitPath", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0x31, 0x57, 0x83}},
        {0x4e0770, ptr(&ZString::SplitPathText), "ZString::SplitPathText", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf2, 0x89}},
        {0x4e0d30, ptr(&ZString::Insert), "ZString::Insert", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xd9, 0x57, 0x8b}},
        {0x4e0e60, ptr(&ZString::GrowTo), "ZString::GrowTo", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x8b, 0x74, 0x24}},
        {0x4e0f20, ptr(&ZString::TrimRight), "ZString::TrimRight", 5, {0x51, 0x55, 0x8b, 0xe9, 0x56, 0x33, 0xf6, 0x8b}},
        {0x4e1000, ptr(&ZString::KeepChars), "ZString::KeepChars", 6, {0x55, 0x8b, 0xe9, 0x56, 0x33, 0xf6, 0x8b, 0x4d}},
        {0x4e10f0, ptr(&ZString::TrimLeft), "ZString::TrimLeft", 5, {0x51, 0x55, 0x8b, 0xe9, 0x56, 0x33, 0xf6, 0x8b}},
        {0x4e11d0, ptr(&ZString::MakeUpper), "ZString::MakeUpper", 5, {0x51, 0x55, 0x8b, 0xe9, 0x56, 0x33, 0xf6, 0x8b}},
        {0x4e12b0, ptr(&ZString::ReplaceChar), "ZString::ReplaceChar", 6, {0x55, 0x8b, 0xe9, 0x56, 0x33, 0xf6, 0x8b, 0x4d}},
        {0x4e1880, ptr(&ZString::SetHex), "ZString::SetHex", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x4e1ab0, ptr(&ZString::AppendString), "ZString::AppendString", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x0c, 0x53}},
        {0x4e1d10, ptr(&ZString::AppendChar), "ZString::AppendChar", 5, {0x51, 0x53, 0x8b, 0xd9, 0x55, 0x56, 0x8b, 0x13}},
        {0x4e0b30, ptr(&ZString::SetTimeHMS), "ZString::SetTimeHMS", 5, {0x51, 0xd9, 0x44, 0x24, 0x08, 0xd8, 0x0d, 0x38}},
        {0x4e0bc0, ptr(&ZString::SetDuration), "ZString::SetDuration", 7, {0x83, 0xec, 0x08, 0xd9, 0x44, 0x24, 0x0c, 0xd8}},
        {0x585490, ptr(&FormatLength), "FormatLength", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
