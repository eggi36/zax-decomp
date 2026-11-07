// The C runtime's formatted output and input: _output (sprintf, vsprintf; fprintf in crt_stdio.cpp), the
// floating point layouts behind %e/%f/%g (crt_float.cpp) through the exe's signatures, _input (sscanf),
// _fltin and atof, __fassign, wctomb and mbtowc (docs/DECOMPILATION.md 10, "C runtime").
#include "crt_common.h"
#include "crt_float.h"

namespace zax::crt {

namespace {

constexpr DWORD DECIMAL_POINT = 0x629d70;   // __decimal_point, '.'
constexpr DWORD NULL_STRING = 0x629f5c, NULL_WSTRING = 0x629f60;   // "(null)", L"(null)"
constexpr DWORD SCANSET_C = 0x62a0c0, SCANSET_S = 0x62a0b8;          // "]" and " \t-\r]" (rejected sets)
constexpr DWORD MBTOWC_LK = 0x5dc5f5, WCTOMB_LK = 0x5dc3e0;

char point() { return *glob<char *>(DECIMAL_POINT); }
size_t length(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

}  // namespace

// ---- wide characters (C locale) -------------------------------------------------------------------------

// _wctomb_lk 0x5dc3e0: a null target only asks for the state (0); above 255 EILSEQ
int __cdecl wctomb_lk(char *s, WORD wc)
{
    if (!s)
        return 0;
    if (*glob<DWORD *>(LC_HANDLE_CTYPE))
        return original_or_dead<int(__cdecl *)(char *, WORD)>(fn_exe(WCTOMB_LK))(s, wc);
    if (wc > 0xff) {
        *errno_ptr() = 42;
        return -1;
    }
    *s = (char)wc;
    return 1;
}

// The locale guard of wctomb/mbtowc: __unguarded_readlc_active counts the readers; while setlocale runs
// (never in the exe) the setlocale lock instead
template <typename F> int locale_guard(F f)
{
    long *readers = glob<long *>(UNGUARDED_READLC);
    api<long(WINAPI *)(long *)>(iat::InterlockedIncrement)(readers);
    bool locked = false;
    if (*glob<int *>(SETLC_ACTIVE)) {
        api<long(WINAPI *)(long *)>(iat::InterlockedDecrement)(readers);
        lock(SETLOCALE_LOCK);
        locked = true;
    }
    const int r = f();
    if (locked)
        unlock(SETLOCALE_LOCK);
    else
        api<long(WINAPI *)(long *)>(iat::InterlockedDecrement)(readers);
    return r;
}

// wctomb 0x5dc387
int __cdecl wctomb_(char *s, WORD wc) { return locale_guard([&] { return wctomb_lk(s, wc); }); }

// _mbtowc_lk 0x5dc5f5: in the C locale one byte is one character
int __cdecl mbtowc_lk(WORD *pwc, const char *s, size_t n)
{
    if (!s || !n)
        return 0;
    if (!*s) {
        if (pwc)
            *pwc = 0;
        return 0;
    }
    if (*glob<DWORD *>(LC_HANDLE_CTYPE))
        return original_or_dead<int(__cdecl *)(WORD *, const char *, size_t)>(fn_exe(MBTOWC_LK))(pwc, s, n);
    if (pwc)
        *pwc = (BYTE)*s;
    return 1;
}
// mbtowc 0x5dc598
int __cdecl mbtowc_(WORD *pwc, const char *s, size_t n) { return locale_guard([&] { return mbtowc_lk(pwc, s, n); }); }

// ---- _output --------------------------------------------------------------------------------------------

namespace {

// write_char 0x5d4df8 (the putc macro), write_multi_char 0x5d4e2d, write_string 0x5d4e5e
void write_char(int c, File *f, int *count)
{
    int r;
    if (--f->cnt >= 0)
        r = (BYTE)(*f->ptr++ = (char)c);
    else
        r = flsbuf(c, f);
    if (r == -1)
        *count = -1;
    else
        ++*count;
}
void write_multi_char(int c, int n, File *f, int *count)
{
    while (n-- > 0) {
        write_char(c, f, count);
        if (*count == -1)
            break;
    }
}
void write_string(const char *s, int n, File *f, int *count)
{
    while (n-- > 0) {
        write_char(*s++, f, count);
        if (*count == -1)
            break;
    }
}

// The format parser's character classes and states (the exe's __lookuptable 0x607150 holds both: low
// nibble the class of the character - 0x20, high nibble the next state for class * 8 + state)
enum { CH_OTHER, CH_PERCENT, CH_DOT, CH_STAR, CH_ZERO, CH_DIGIT, CH_FLAG, CH_SIZE, CH_TYPE };
enum { ST_NORMAL, ST_PERCENT, ST_FLAG, ST_WIDTH, ST_DOT, ST_PRECIS, ST_SIZE, ST_TYPE };

int char_class(int c)
{
    if (c < 0x20 || c > 0x78)
        return CH_OTHER;
    switch (c) {
    case ' ': case '#': case '+': case '-':
        return CH_FLAG;
    case '%':
        return CH_PERCENT;
    case '.':
        return CH_DOT;
    case '*':
        return CH_STAR;
    case '0':
        return CH_ZERO;
    case 'F': case 'I': case 'L': case 'N': case 'h': case 'l': case 'w':
        return CH_SIZE;
    case 'B': case 'C': case 'E': case 'G': case 'S': case 'X': case 'Z': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'i': case 'n': case 'o': case 'p': case 's': case 'u': case 'x':
        return CH_TYPE;
    }
    return c >= '1' && c <= '9' ? CH_DIGIT : CH_OTHER;
}

// next[class][state]
constexpr signed char NEXT_STATE[9][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 1}, {0, 4, 4, 4, 0, 0, 0, 0},
    {0, 3, 3, 0, 5, 0, 0, 0}, {0, 2, 2, 3, 5, 5, 0, 0}, {0, 3, 3, 3, 5, 5, 0, 0},
    {0, 2, 2, 0, 0, 0, 0, 0}, {0, 6, 6, 6, 6, 6, 6, 0}, {0, 7, 7, 7, 7, 7, 7, 0},
};

enum : unsigned {
    FL_SIGN = 1, FL_SIGNSP = 2, FL_LEFT = 4, FL_LEADZERO = 8, FL_LONG = 0x10, FL_SHORT = 0x20, FL_SIGNED = 0x40,
    FL_ALTERNATE = 0x80, FL_NEGATIVE = 0x100, FL_FORCEOCTAL = 0x200, FL_WIDECHAR = 0x800, FL_I64 = 0x8000
};

int get_int_arg(char **ap)
{
    *ap += 4;
    return *(int *)(*ap - 4);
}
long long get_int64_arg(char **ap)
{
    *ap += 8;
    return *(long long *)(*ap - 8);
}
WORD get_short_arg(char **ap)
{
    *ap += 4;
    return *(WORD *)(*ap - 4);
}

}  // namespace

// The helpers with the exe's signatures (write_char 0x5d4df8 ... get_short_arg 0x5d4eb3)
void __cdecl write_char_(int c, File *f, int *count) { write_char(c, f, count); }
void __cdecl write_multi_char_(int c, int n, File *f, int *count) { write_multi_char(c, n, f, count); }
void __cdecl write_string_(const char *s, int n, File *f, int *count) { write_string(s, n, f, count); }
int __cdecl get_int_arg_(char **ap) { return get_int_arg(ap); }
long long __cdecl get_int64_arg_(char **ap) { return get_int64_arg(ap); }
WORD __cdecl get_short_arg_(char **ap) { return get_short_arg(ap); }

// _output 0x5d46b7
int __cdecl output(File *f, const char *format, char *ap)
{
    int count = 0, state = ST_NORMAL;
    // these survive from one conversion to the next, as in the exe
    int textlen = 0, precision = 0, width = 0, prefixlen = 0, capexp = 0, no_output = 0, bufferiswide = 0, hexadd = 0;
    unsigned flags = 0, radix = 10;
    char *text = nullptr;
    char buffer[0x200], prefix[2];
    int c;
    while ((c = (BYTE)*format++) && count >= 0) {
        const signed char ch = (signed char)c;
        state = NEXT_STATE[char_class(ch)][state];
        switch (state) {
        case ST_NORMAL:
        normal:
            bufferiswide = 0;
            if ((*glob<const WORD **>(PCTYPE))[(BYTE)ch] & 0x8000) {   // lead byte: two characters
                write_char(ch, f, &count);
                c = (BYTE)*format++;
                write_char((signed char)c, f, &count);
                break;
            }
            write_char(ch, f, &count);
            break;
        case ST_PERCENT:
            precision = -1;
            capexp = no_output = width = prefixlen = 0;
            flags = 0;
            bufferiswide = 0;
            break;
        case ST_FLAG:
            switch (ch) {
            case ' ': flags |= FL_SIGNSP; break;
            case '#': flags |= FL_ALTERNATE; break;
            case '+': flags |= FL_SIGN; break;
            case '-': flags |= FL_LEFT; break;
            case '0': flags |= FL_LEADZERO; break;
            }
            break;
        case ST_WIDTH:
            if (ch == '*') {
                width = get_int_arg(&ap);
                if (width < 0) {
                    flags |= FL_LEFT;
                    width = -width;
                }
            } else
                width = width * 10 + ch - '0';
            break;
        case ST_DOT:
            precision = 0;
            break;
        case ST_PRECIS:
            if (ch == '*') {
                precision = get_int_arg(&ap);
                if (precision < 0)
                    precision = -1;
            } else
                precision = precision * 10 + ch - '0';
            break;
        case ST_SIZE:
            if (ch == 'I') {
                if (format[0] == '6' && format[1] == '4') {
                    format += 2;
                    flags |= FL_I64;
                    break;
                }
                state = ST_NORMAL;   // "%I" without 64: the I is written
                goto normal;
            }
            if (ch == 'h')
                flags |= FL_SHORT;
            else if (ch == 'l')
                flags |= FL_LONG;
            else if (ch == 'w')
                flags |= FL_WIDECHAR;
            break;
        case ST_TYPE: {
            bool integer = false;
            int t = ch;
            switch (t) {
            case 'S':
                if (!(flags & (FL_SHORT | FL_LONG | FL_WIDECHAR)))
                    flags |= FL_WIDECHAR;
                [[fallthrough]];
            case 's': {
                int n = precision == -1 ? 0x7fffffff : precision;
                text = (char *)(uintptr_t)get_int_arg(&ap);
                if (flags & (FL_LONG | FL_WIDECHAR)) {
                    if (!text)
                        text = *glob<char **>(NULL_WSTRING);
                    bufferiswide = 1;
                    const WORD *w = (const WORD *)text;
                    while (n-- && *w)
                        w++;
                    textlen = (int)(w - (const WORD *)text);
                } else {
                    if (!text)
                        text = *glob<char **>(NULL_STRING);
                    const char *p = text;
                    while (n-- && *p)
                        p++;
                    textlen = (int)(p - text);
                }
                break;
            }
            case 'C':
                if (!(flags & (FL_SHORT | FL_LONG | FL_WIDECHAR)))
                    flags |= FL_WIDECHAR;
                [[fallthrough]];
            case 'c':
                if (flags & (FL_LONG | FL_WIDECHAR)) {
                    const WORD wc = get_short_arg(&ap);
                    textlen = wctomb_(buffer, wc);
                    if (textlen < 0)
                        no_output = 1;
                } else {
                    buffer[0] = (char)get_int_arg(&ap);
                    textlen = 1;
                }
                text = buffer;
                break;
            case 'Z': {
                // ANSI_STRING / UNICODE_STRING: {short Length, MaximumLength; char *Buffer}
                const BYTE *s = (const BYTE *)(uintptr_t)get_int_arg(&ap);
                if (s && *(char *const *)(s + 4)) {
                    text = *(char *const *)(s + 4);
                    if (flags & FL_WIDECHAR) {
                        textlen = (int)((unsigned)(int)*(const short *)s >> 1);
                        bufferiswide = 1;
                    } else {
                        bufferiswide = 0;
                        textlen = *(const short *)s;
                    }
                } else {
                    text = *glob<char **>(NULL_STRING);
                    textlen = (int)length(text);
                }
                break;
            }
            case 'e': case 'f': case 'g': case 'E': case 'G': {
                if (t == 'E' || t == 'G') {
                    capexp = 1;
                    t += 0x20;
                }
                flags |= FL_SIGNED;
                text = buffer;
                if (precision < 0)
                    precision = 6;
                else if (precision == 0 && t == 'g')
                    precision = 1;
                double v;
                memcpy(&v, ap, 8);
                ap += 8;
                cfltcvt(&v, buffer, t, precision, capexp, point());
                if ((flags & FL_ALTERNATE) && precision == 0)
                    forcdecpt(buffer, point());
                if (t == 'g' && !(flags & FL_ALTERNATE))
                    cropzeros(buffer, point());
                if (buffer[0] == '-') {
                    flags |= FL_NEGATIVE;
                    text = buffer + 1;
                }
                textlen = (int)length(text);
                break;
            }
            case 'd': case 'i':
                flags |= FL_SIGNED;
                radix = 10;
                integer = true;
                break;
            case 'u':
                radix = 10;
                integer = true;
                break;
            case 'n': {
                void *p = (void *)(uintptr_t)get_int_arg(&ap);
                if (flags & FL_SHORT)
                    *(short *)p = (short)count;
                else
                    *(int *)p = count;
                no_output = 1;
                break;
            }
            case 'o':
                radix = 8;
                if (flags & FL_ALTERNATE)
                    flags |= FL_FORCEOCTAL;
                integer = true;
                break;
            case 'p':
                precision = 8;
                [[fallthrough]];
            case 'X':
            case 'x':
                hexadd = t == 'x' ? 0x27 : 7;
                radix = 16;
                if (flags & FL_ALTERNATE) {
                    prefix[0] = '0';
                    prefix[1] = (char)('Q' + hexadd);
                    prefixlen = 2;
                }
                integer = true;
                break;
            }
            if (integer) {
                unsigned long long number;
                if (flags & FL_I64)
                    number = (unsigned long long)get_int64_arg(&ap);
                else if (flags & FL_SHORT)
                    number = flags & FL_SIGNED ? (unsigned long long)(long long)(short)get_int_arg(&ap)
                                               : (unsigned long long)(WORD)get_int_arg(&ap);
                else
                    number = flags & FL_SIGNED ? (unsigned long long)(long long)get_int_arg(&ap)
                                               : (unsigned long long)(unsigned)get_int_arg(&ap);
                if ((flags & FL_SIGNED) && (long long)number < 0) {
                    number = 0 - number;
                    flags |= FL_NEGATIVE;
                }
                if (!(flags & FL_I64))
                    number &= 0xffffffffull;
                if (precision < 0)
                    precision = 1;
                else
                    flags &= ~FL_LEADZERO;
                if (!number)
                    prefixlen = 0;
                text = buffer + 0x1ff;
                while (precision-- > 0 || number) {
                    int digit = (int)(number % radix) + '0';
                    number /= radix;
                    if (digit > '9')
                        digit += hexadd;
                    *text-- = (char)digit;
                }
                textlen = (int)(buffer + 0x1ff - text);
                text++;
                if ((flags & FL_FORCEOCTAL) && (*text != '0' || textlen == 0)) {
                    *--text = '0';
                    textlen++;
                }
            }
            if (!no_output) {
                if (flags & FL_SIGNED) {
                    if (flags & FL_NEGATIVE) {
                        prefix[0] = '-';
                        prefixlen = 1;
                    } else if (flags & FL_SIGN) {
                        prefix[0] = '+';
                        prefixlen = 1;
                    } else if (flags & FL_SIGNSP) {
                        prefix[0] = ' ';
                        prefixlen = 1;
                    }
                }
                const int padding = width - prefixlen - textlen;
                if (!(flags & (FL_LEFT | FL_LEADZERO)))
                    write_multi_char(' ', padding, f, &count);
                write_string(prefix, prefixlen, f, &count);
                if ((flags & FL_LEADZERO) && !(flags & FL_LEFT))
                    write_multi_char('0', padding, f, &count);
                if (bufferiswide && textlen > 0) {
                    const WORD *w = (const WORD *)text;
                    for (int n = textlen; n > 0; n--) {
                        char mb[8];
                        const int k = wctomb_(mb, *w++);
                        if (k <= 0)
                            break;
                        write_string(mb, k, f, &count);
                    }
                } else
                    write_string(text, textlen, f, &count);
                if (flags & FL_LEFT)
                    write_multi_char(' ', padding, f, &count);
            }
            break;
        }
        }
    }
    return count;
}

// sprintf 0x5d0db7, vsprintf 0x5d21ba: a string stream (_IOWRT | _IOSTRG) of unlimited size
int __cdecl vsprintf_(char *buf, const char *format, char *ap)
{
    File f;
    f.flag = IOWRT | IOSTRG;
    f.ptr = f.base = buf;
    f.cnt = 0x7fffffff;
    const int n = output(&f, format, ap);
    if (--f.cnt >= 0)
        *f.ptr++ = 0;
    else
        flsbuf(0, &f);
    return n;
}
int __cdecl sprintf_(char *buf, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    const int n = vsprintf_(buf, format, (char *)ap);
    va_end(ap);
    return n;
}

// ---- the floating point pieces through the exe's signatures ----------------------------------------------

void __cdecl cfltcvt_(const double *x, char *buf, int format, int precision, int caps) { cfltcvt(x, buf, format, precision, caps, point()); }
char *__cdecl cftoe_(const double *x, char *buf, int ndec, int caps) { return cftoe(x, buf, ndec, caps, point()); }
char *__cdecl cftof_(const double *x, char *buf, int ndec) { return cftof(x, buf, ndec, point()); }
char *__cdecl cftog_(const double *x, char *buf, int ndec, int caps) { return cftog(x, buf, ndec, caps, point()); }
char *__cdecl cftoe2_(char *buf, int ndec, int caps, StrFlt *flt, char g) { return cftoe2(buf, ndec, caps, flt, g, point()); }
char *__cdecl cftof2_(char *buf, int ndec, StrFlt *flt, char g) { return cftof2(buf, ndec, flt, g, point()); }
void __cdecl cropzeros_(char *buf) { cropzeros(buf, point()); }
void __cdecl forcdecpt_(char *buf) { forcdecpt(buf, point()); }
StrFlt *__cdecl fltout2_(double x, StrFlt *flt, char *digits) { return fltout2(x, flt, digits); }
void __cdecl fptostr_(char *buf, int digits, StrFlt *flt) { fptostr(buf, digits, flt); }
void __cdecl dtold_(Ld10 *out, const double *x) { dtold(out, x); }
int __cdecl i10_output_(Ld10 ld, int ndigits, unsigned flags, Fos *fos) { return i10_output(ld, ndigits, flags, fos); }
void __cdecl shift_(char *s, int n)
{
    if (n) {
        size_t k = length(s) + 1;
        while (k--)
            s[n + k] = s[k];
    }
}
void __cdecl ld12mul_(Ld12 *x, const Ld12 *y) { ld12mul(x, y); }
void __cdecl multtenpow12_(Ld12 *x, int pow, int mult12) { multtenpow12(x, pow, mult12); }
void __cdecl mtold12_(const char *man, unsigned len, Ld12 *out) { mtold12(man, len, out); }
int __cdecl add_carry_(DWORD x, DWORD y, DWORD *sum) { return add_carry(x, y, sum); }
void __cdecl add_12_(Ld12 *a, const Ld12 *b) { add_12(*a, *b); }
void __cdecl shl_12_(Ld12 *a) { shl_12(*a); }
void __cdecl shr_12_(Ld12 *a) { shr_12(*a); }
// __strgtold12 0x5dbad3: the exe passes scale, decpt and implicit_E as 0 everywhere
unsigned __cdecl strgtold12_(Ld12 *out, const char **end, const char *s, int mult12, int, int, int)
{
    return strgtold12(out, end, s, mult12, point());
}
int __cdecl ld12tod_(const Ld12 *in, double *out) { return ld12tod(in, out); }
int __cdecl ld12tof_(const Ld12 *in, float *out) { return ld12tof(in, out); }
int __cdecl atodbl_(double *out, const char *s) { return atodbl(out, s, point()); }
int __cdecl atoflt_(float *out, const char *s) { return atoflt(out, s, point()); }

// __fassign 0x5d3e88: the text to a double (flag != 0) or a float
void __cdecl fassign(int flag, char *arg, const char *s)
{
    if (flag) {
        double d;
        atodbl(&d, s, point());
        memcpy(arg, &d, 8);
    } else {
        float x;
        atoflt(&x, s, point());
        memcpy(arg, &x, 4);
    }
}

// _fltin 0x5d3cd0: {flags, characters read, -, double}; 0x200 no digits, 0x80 overflow, 0x100 underflow
struct Flt {
    int flags, nbytes;
    long lval;
    int pad;
    double dval;
};
Flt *__cdecl fltin(Flt *out, const char *s, int, int, int)
{
    Ld12 ld;
    const char *end;
    const unsigned r = strgtold12(&ld, &end, s, 0, point());
    int flags = 0;
    double d;
    if (r & 4) {
        flags = 0x200;
        d = 0;
    } else {
        const int c = ld12tod(&ld, &d);
        if ((r & 2) || c == 1)
            flags = 0x80;
        if ((r & 1) || c == 2)
            flags |= 0x100;
    }
    out->flags = flags;
    out->dval = d;
    out->nbytes = (int)(end - s);
    return out;
}

// atof 0x5d03de: white space (the ctype table), then _fltin
double __cdecl atof_(const char *s)
{
    while (ctype((BYTE)*s, SPACE))
        s++;
    Flt flt;
    fltin(&flt, s, (int)length(s), 0, 0);
    return flt.dval;
}

// ---- _input ---------------------------------------------------------------------------------------------

int __cdecl isspace_(int c);   // crt_string.cpp

// _inc 0x5d6751 (getc), _un_inc 0x5d676b, _whiteout 0x5d6782, _hextodec 0x5d671a
int __cdecl inc(File *f) { return --f->cnt >= 0 ? (BYTE)*f->ptr++ : filbuf(f); }
void __cdecl un_inc(int c, File *f)
{
    if (c != -1)
        ungetc_lk(c, f);
}
int __cdecl whiteout(int *counter, File *f)
{
    int c;
    do {
        ++*counter;
        c = inc(f);
    } while (isspace_(c));
    return c;
}
int __cdecl hextodec(int c) { return ctype(c, DIGIT) ? c : (c & ~0x20) - 7; }

// _input 0x5d5cf5
int __cdecl input(File *stream, const char *format, char *ap)
{
    int charcount = 0, count = 0, ch = 0;
    signed char match = 0;
    const BYTE *fmt = (const BYTE *)format;
    if (!*fmt)
        goto end;
    while (*fmt) {
        if (ctype(*fmt, SPACE)) {
            charcount--;
            un_inc(whiteout(&charcount, stream), stream);
            while (isspace_(*++fmt))
                ;
        }
        if (*fmt == '%') {
            char reject = 0, negative = 0, suppress = 0, done_flag = 0, widechar_out = 0, widechar = 0, longone = 1;
            unsigned char prevchar = 0;
            int number = 0, started = 0, widthset = 0, width = 0, integer64 = 0;
            long long num64 = 0;
            char *argsave = ap;
            BYTE *pointer = nullptr;
            int comchr;
            auto assign = [&] {
                if (integer64)
                    *(long long *)pointer = num64;
                else if (longone)
                    *(int *)pointer = number;
                else
                    *(short *)pointer = (short)number;
            };
            do {
                const int c = *++fmt;
                if (ctype(c, DIGIT)) {
                    widthset++;
                    width = width * 10 + c - '0';
                } else
                    switch (c) {
                    case 'N': case 'F':
                        break;
                    case '*':
                        suppress++;
                        break;
                    case 'I':
                        if (fmt[1] == '6' && fmt[2] == '4') {
                            integer64++;
                            num64 = 0;
                            fmt += 2;
                        } else
                            done_flag++;
                        break;
                    case 'L':
                        longone++;
                        break;
                    case 'h':
                        longone--;
                        widechar--;
                        break;
                    case 'l':
                        longone++;
                        widechar++;
                        break;
                    case 'w':
                        widechar++;
                        break;
                    default:
                        done_flag++;
                    }
            } while (!done_flag);
            if (!suppress) {
                argsave = ap;
                pointer = *(BYTE **)ap;
                ap += 4;
            }
            done_flag = 0;
            if (!widechar)
                widechar = *fmt == 'S' || *fmt == 'C' ? 1 : -1;
            comchr = *fmt | 0x20;
            if (comchr != 'n') {
                if (comchr == 'c' || comchr == '{') {
                    charcount++;
                    ch = inc(stream);
                } else
                    ch = whiteout(&charcount, stream);
            }
            if (widthset && !width)
                goto error_return;
            switch (comchr) {
            case 'c':
            case 's':
            case '{': {
                const BYTE *set;
                if (comchr == 'c') {
                    if (!widthset) {
                        width++;
                        widthset = 1;
                    }
                    if (widechar > 0)
                        widechar_out = 1;
                    set = glob<const BYTE *>(SCANSET_C);
                    reject = -1;
                } else if (comchr == 's') {
                    if (widechar > 0)
                        widechar_out = 1;
                    set = glob<const BYTE *>(SCANSET_S);
                    reject = -1;
                } else {
                    if (widechar > 0)
                        widechar_out = 1;
                    set = ++fmt;
                    if (*set == '^') {
                        set++;
                        reject = -1;
                    }
                }
                BYTE table[32];
                memset(table, 0, sizeof table);
                unsigned char prev;
                if (comchr == '{' && *set == ']') {
                    prev = ']';
                    set++;
                    table[']' >> 3] = 1 << (']' & 7);
                } else
                    prev = prevchar;
                while (*set != ']') {
                    unsigned char c = *set++;
                    if (c == '-' && prev && *set != ']') {
                        const unsigned char next = *set++;
                        unsigned char lo = prev, hi = next;
                        if (prev >= next) {
                            lo = next;
                            hi = prev;
                        }
                        for (unsigned v = lo; v <= hi; v++)
                            table[v >> 3] |= (BYTE)(1 << (v & 7));
                        prev = 0;
                    } else {
                        table[c >> 3] |= (BYTE)(1 << (c & 7));
                        prev = c;
                    }
                }
                if (!*set)
                    goto end;
                if (comchr == '{')
                    fmt = set;
                BYTE *start = pointer;
                charcount--;
                un_inc(ch, stream);
                for (;;) {
                    if (widthset && !width--)
                        break;
                    charcount++;
                    ch = inc(stream);
                    if (ch == -1 || !(((signed char)table[ch >> 3] ^ reject) & (1 << (ch & 7)))) {
                        charcount--;
                        un_inc(ch, stream);
                        break;
                    }
                    if (!suppress) {
                        if (widechar_out) {
                            char mb[2] = {(char)ch, 0};
                            if ((*glob<const WORD **>(PCTYPE))[(BYTE)ch] & 0x8000) {
                                charcount++;
                                mb[1] = (char)inc(stream);
                            }
                            WORD wc;
                            mbtowc_(&wc, mb, (size_t)*glob<int *>(MB_CUR_MAX));
                            *(WORD *)pointer = wc;
                            pointer += 2;
                        } else
                            *pointer++ = (BYTE)ch;
                    } else
                        start++;
                }
                if (start == pointer)
                    goto end;
                if (!suppress) {
                    count++;
                    if (comchr != 'c') {
                        if (widechar_out)
                            *(WORD *)pointer = 0;
                        else
                            *pointer = 0;
                    }
                }
                break;
            }
            case 'n':
                number = charcount;
                if (!suppress)
                    assign();
                break;
            case 'e': case 'f': case 'g': {
                char floatstring[0x160];
                char *p = floatstring;
                if (ch == '-' || ch == '+') {
                    if (ch == '-')
                        *p++ = '-';
                    width--;
                    charcount++;
                    ch = inc(stream);
                }
                if (!widthset || width > 0x15d)
                    width = 0x15d;
                while (ctype(ch, DIGIT) && width-- != 0) {
                    started++;
                    *p++ = (char)ch;
                    charcount++;
                    ch = inc(stream);
                }
                if (ch == (BYTE)point() && width-- != 0) {
                    charcount++;
                    ch = inc(stream);
                    *p++ = point();
                    while (ctype(ch, DIGIT) && width-- != 0) {
                        started++;
                        *p++ = (char)ch;
                        charcount++;
                        ch = inc(stream);
                    }
                }
                if (started && (ch == 'e' || ch == 'E') && width-- != 0) {
                    *p++ = 'e';
                    charcount++;
                    ch = inc(stream);
                    bool read_next = false;
                    if (ch == '-') {
                        *p++ = '-';
                        read_next = true;
                    } else if (ch == '+')
                        read_next = true;
                    if (read_next) {
                        if (width-- == 0)
                            width = 0;
                        else {
                            charcount++;
                            ch = inc(stream);
                        }
                    }
                    while (ctype(ch, DIGIT) && width-- != 0) {
                        started++;
                        *p++ = (char)ch;
                        charcount++;
                        ch = inc(stream);
                    }
                }
                charcount--;
                un_inc(ch, stream);
                if (!started)
                    goto end;
                if (!suppress) {
                    count++;
                    *p = 0;
                    // through the exe's _cfltcvt_tab slot (__fassign), as the original
                    (*glob<void(__cdecl **)(int, char *, const char *)>(0x629d80))(longone - 1, (char *)pointer, floatstring);
                }
                break;
            }
            case 'i': case 'x': case 'd': case 'o': case 'u': case 'p': {
                int type = comchr == 'i' ? 'd' : comchr;
                if (comchr == 'p')
                    longone = 1;
                if (ch == '-' || ch == '+') {
                    if (ch == '-')
                        negative = 1;
                    if (--width == 0 && widthset)
                        done_flag = 1;
                    else {
                        charcount++;
                        ch = inc(stream);
                    }
                }
                if ((comchr == 'i' || comchr == 'x') && ch == '0') {
                    charcount++;
                    ch = inc(stream);
                    if ((char)ch == 'x' || (char)ch == 'X') {
                        charcount++;
                        ch = inc(stream);
                        type = 'x';
                    } else {
                        started = 1;
                        if (type == 'x') {
                            charcount--;
                            un_inc(ch, stream);
                            ch = '0';
                        } else
                            type = 'o';
                    }
                }
                if (integer64) {
                    if (!done_flag) {
                        for (;;) {
                            if (type == 'x') {
                                if (!ctype(ch, HEX))
                                    break;
                                num64 <<= 4;
                                ch = hextodec(ch);
                            } else {
                                if (!ctype(ch, DIGIT) || (type == 'o' && ch >= '8'))
                                    break;
                                num64 = type == 'o' ? num64 << 3 : num64 * 10;
                            }
                            started++;
                            num64 += ch - '0';
                            if (widthset && --width == 0)
                                goto done64;
                            charcount++;
                            ch = inc(stream);
                        }
                        charcount--;
                        un_inc(ch, stream);
                    }
                done64:
                    if (negative)
                        num64 = -num64;
                } else {
                    if (!done_flag) {
                        for (;;) {
                            if (type == 'x' || type == 'p') {
                                if (!ctype(ch, HEX))
                                    break;
                                number <<= 4;
                                ch = hextodec(ch);
                            } else {
                                if (!ctype(ch, DIGIT) || (type == 'o' && ch >= '8'))
                                    break;
                                number = type == 'o' ? number << 3 : number * 10;
                            }
                            started++;
                            number += ch - '0';
                            if (widthset && --width == 0)
                                goto done32;
                            charcount++;
                            ch = inc(stream);
                        }
                        charcount--;
                        un_inc(ch, stream);
                    }
                done32:
                    if (negative)
                        number = -number;
                }
                if (!started)
                    goto end;
                if (!suppress) {
                    count++;
                    assign();
                }
                break;
            }
            default:
                if (*fmt != ch)
                    goto error_return;
                match--;
                if (!suppress)
                    ap = argsave;
                break;
            }
            match++;
            fmt++;
        } else {
            charcount++;
            ch = inc(stream);
            const int c = *fmt++;
            if (c != ch) {
                charcount--;
                un_inc(ch, stream);
                goto end;
            }
            if ((*glob<const WORD **>(PCTYPE))[(BYTE)ch] & 0x8000) {
                charcount++;
                const int c2 = inc(stream), f2 = *fmt++;
                if (f2 != c2) {
                    charcount--;
                    un_inc(c2, stream);
                    charcount--;
                    un_inc(ch, stream);
                    goto end;
                }
                charcount--;
            }
        }
        if (ch == -1 && (*fmt != '%' || fmt[1] != 'n'))
            goto eof;
    }
    goto end;
error_return:
    charcount--;
    un_inc(ch, stream);
end:
    if (ch != -1)
        return count;
eof:
    return count || match ? count : -1;
}

// sscanf 0x5d1539: a string stream (_IOREAD | _IOSTRG | _IOMYBUF) over the text
int __cdecl sscanf_(const char *s, const char *format, ...)
{
    File f;
    f.flag = IOREAD | IOSTRG | IOMYBUF;
    f.ptr = f.base = (char *)s;
    f.cnt = (int)length(s);
    va_list ap;
    va_start(ap, format);
    const int n = input(&f, format, (char *)ap);
    va_end(ap);
    return n;
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_format)
{
    const Hook list[] = {
        {0x5dd09e, (void *)&add_carry_, "__addl", 5, {0x8b, 0x54, 0x24, 0x04, 0x56, 0x8b, 0x74, 0x24}},
        {0x5dd0bf, (void *)&add_12_, "___add_12", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0x7c}},
        {0x5dd11d, (void *)&shl_12_, "__shl_12", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0x30}},
        {0x5dd14b, (void *)&shr_12_, "__shr_12", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0x50}},
        {0x5dd178, (void *)&mtold12_, "__mtold12", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x8b, 0x45}},
        {0x5dd23f, (void *)&ld12mul_, "__ld12mul", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x24, 0x53, 0x8b}},
        {0x5dd45f, (void *)&multtenpow12_, "__multtenpow12", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53, 0xbb}},
        {0x5dd69a, (void *)&i10_output_, "$I10_OUTPUT", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c, 0x8b, 0x45}},
        {0x5dc1dd, (void *)&dtold_, "__dtold", 7, {0x55, 0x8b, 0xec, 0x51, 0x8b, 0x55, 0x0c, 0x53}},
        {0x5dc181, (void *)&fltout2_, "_fltout2", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28, 0x56, 0x8d}},
        {0x5dc10a, (void *)&fptostr_, "_fptostr", 6, {0x55, 0x8b, 0xec, 0x8b, 0x55, 0x10, 0x53, 0x8b}},
        {0x5d3ec6, (void *)&cftoe_, "_cftoe", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28, 0x8d, 0x45}},
        {0x5d3f27, (void *)&cftoe2_, "_cftoe2", 6, {0x55, 0x8b, 0xec, 0x53, 0x33, 0xdb, 0x38, 0x5d}},
        {0x5d3fe9, (void *)&cftof_, "_cftof", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28, 0x8d, 0x45}},
        {0x5d403e, (void *)&cftof2_, "_cftof2", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x8b, 0x75, 0x10}},
        {0x5d40e5, (void *)&cftog_, "_cftog", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28, 0x53, 0x56}},
        {0x5d4178, (void *)&cfltcvt_, "_cfltcvt", 7, {0x55, 0x8b, 0xec, 0x83, 0x7d, 0x10, 0x65, 0x74}},
        {0x5d3e22, (void *)&cropzeros_, "_cropzeros", 10, {0x8b, 0x44, 0x24, 0x04, 0x8a, 0x15, 0x70, 0x9d}},
        {0x5d3dc8, (void *)&forcdecpt_, "_forcdecpt", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x0f, 0xbe, 0x06}},
        {0x5d41c9, (void *)&shift_, "_shift", 5, {0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x85, 0xff, 0x74}},
        {0x5dbad3, (void *)&strgtold12_, "__strgtold12", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x5c, 0x53, 0x56}},
        {0x5dba4d, (void *)&ld12tod_, "_ld12tod", 5, {0x68, 0x38, 0xc5, 0x62, 0x00, 0xff, 0x74, 0x24}},
        {0x5dba63, (void *)&ld12tof_, "_ld12tof", 5, {0x68, 0x50, 0xc5, 0x62, 0x00, 0xff, 0x74, 0x24}},
        {0x5dba79, (void *)&atodbl_, "_atodbl", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x33, 0xc0}},
        {0x5dbaa6, (void *)&atoflt_, "_atoflt", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x33, 0xc0}},
        {0x5dc387, (void *)&wctomb_, "wctomb", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0xbe, 0x2c, 0x65}},
        {0x5dc3e0, (void *)&wctomb_lk, "_wctomb_lk", 6, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x85, 0xc0}},
        {0x5dc598, (void *)&mbtowc_, "mbtowc", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0xbe, 0x2c, 0x65}},
        {0x5dc5f5, (void *)&mbtowc_lk, "_mbtowc_lk", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x8b, 0x75, 0x0c}},
        {0x5d4df8, (void *)&write_char_, "write_char", 6, {0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x0c, 0xff, 0x49}},
        {0x5d4e2d, (void *)&write_multi_char_, "write_multi_char", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0xc7}},
        {0x5d4e5e, (void *)&write_string_, "write_string", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x8b, 0xc3, 0x4b}},
        {0x5d4e96, (void *)&get_int_arg_, "get_int_arg", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0x00, 0x04, 0x8b}},
        {0x5d4ea3, (void *)&get_int64_arg_, "get_int64_arg", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0x00, 0x08, 0x8b}},
        {0x5d4eb3, (void *)&get_short_arg_, "get_short_arg", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0x00, 0x04, 0x8b}},
        {0x5d46b7, (void *)&output, "_output", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x48, 0x02, 0x00}},
        {0x5d21ba, (void *)&vsprintf_, "vsprintf", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x8b, 0x45}},
        {0x5d0db7, (void *)&sprintf_, "sprintf", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x8b, 0x45}},
        {0x5d3e88, (void *)&fassign, "__fassign", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x83, 0x7d, 0x08}},
        {0x5d3cd0, (void *)&fltin, "_fltin", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x53, 0x56}},
        {0x5d03de, (void *)&atof_, "atof", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x56, 0x8b}},
        {0x5d6751, (void *)&inc, "_inc", 7, {0x8b, 0x54, 0x24, 0x04, 0xff, 0x4a, 0x04, 0x78}},
        {0x5d676b, (void *)&un_inc, "_un_inc", 5, {0x83, 0x7c, 0x24, 0x04, 0xff, 0x74, 0x0f, 0xff}},
        {0x5d6782, (void *)&whiteout, "_whiteout", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0xff, 0x74}},
        {0x5d671a, (void *)&hextodec, "_hextodec", 7, {0x83, 0x3d, 0x6c, 0x9d, 0x62, 0x00, 0x01, 0x56}},
        {0x5d5cf5, (void *)&input, "_input", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0xc4, 0x01, 0x00}},
        {0x5d1539, (void *)&sscanf_, "sscanf", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x8b, 0x45}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
