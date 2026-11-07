// Checks of the reimplemented C runtime (crt_*.cpp) against the exe's functions, in the game thread.
// Every hooked function is compared with its original through the trampoline. The hooks stay in: an
// original that calls another runtime function then calls the reimplementation of it, which has its own
// check (each function is compared at its own level). Nothing is put out, because the game's other
// threads (sound streaming, Vorbis) call memcpy, malloc, the stream functions and the 64-bit helpers at
// any time. The three 64-bit shifts have no trampoline (a relative jump in their first bytes); their
// original bytes run from a copy (they are position independent).
//  - formatting: sprintf/vsprintf (_output) on 60 formats x random values (doubles of every kind:
//    specials, denormals, powers of ten and their neighbours, random bits; ints, strings, wide strings,
//    %n, %*.*), and its pieces: $I10_OUTPUT on random 80-bit numbers, __ld12mul, __multtenpow12,
//    __mtold12, __dtold, _fltout2, _fptostr, _cftoe/_cftof/_cftog, _cropzeros, _forcdecpt;
//  - input: sscanf (_input) on 40 formats x random texts (the count, every target byte, %n), atof,
//    _fltin, __strgtold12 (the 12 bytes, the flags, the end), _ld12tod/_ld12tof, _atodbl/_atoflt;
//  - strings and memory: memmove/memcpy with overlaps, memset, memchr, strlen, wcslen, strcpy, strcat,
//    strncpy, strcmp, strncmp, strchr, strrchr, strstr, _strdup, _itoa/xtoa (radix 2-36), wctomb, mbtowc;
//  - errno: _dosmaperr for every code 0-300 (errno and _doserrno), _getptd (the same block);
//  - the heap (the system heap is active, logged): malloc/calloc/realloc/_msize/free and operator
//    new/delete of both sides on the same sizes (the rounded sizes, zeroed calloc blocks, kept contents),
//    blocks of one side freed by the other;
//  - the exit table: _onexit/atexit on a private table (__onexitbegin/__onexitend swapped for the test),
//    _initterm, doexit with retcaller on private tables; _NMSG_WRITE through a recording message box;
//  - files: random operation sequences on two scratch files in the temp folder, one through the original
//    functions, one through the reimplementation (fopen modes, fwrite, fread, fseek, ftell, _chsize,
//    fprintf, fclose; _sopen/_read/_write/_lseek/_close on handles): every return value, errno, the
//    FILE fields and the file contents; remove and rename;
//  - the 64-bit helpers, __ftol and _chkstk on random operands.
#include "check_util.h"
#include "crt_common.h"
#include "crt_float.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI DeleteFileA(const char *name);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);

namespace zax {
namespace {

using crt::File;
using crt::Fos;
using crt::Ld10;
using crt::Ld12;
using crt::StrFlt;

template <typename F> F orig(DWORD a) { return original<F>(a); }
template <typename F> F mine(DWORD a) { return at<F>(a); }

bool g_crt_trace = false;   // log every sprintf case before it runs (to find a case that crashes)
unsigned long long rnd64() { return (unsigned long long)random() << 32 | random(); }

bool same_text(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (*a != *b)
            return false;
        if (!*a)
            return true;
    }
}

// Doubles of every kind
double some_double(int k)
{
    static const unsigned long long specials[] = {
        0, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000000ull,
        0xfff8000000000000ull, 0x7ff0000000000001ull, 0x7ff4000000000000ull, 1, 0x800fffffffffffffull,
        0x0010000000000000ull, 0x7fefffffffffffffull, 0x3ff0000000000000ull, 0xbff0000000000000ull,
        0x3fe0000000000000ull, 0x3fb999999999999aull, 0x4024000000000000ull, 0x412e848000000000ull,
        0x3f1a36e2eb1c432dull, 0x3eb0c6f7a0b5ed8dull, 0x4197d78400000000ull, 0x3ff0000000000001ull,
        0x3fefffffffffffffull, 0x400921fb54442d18ull, 0xfff8000000000001ull};
    const int n = sizeof specials / sizeof specials[0];
    double d;
    unsigned long long u;
    if (k < n)
        u = specials[k];
    else {
        const unsigned r = random() % 8;
        if (r < 2)
            u = rnd64();
        else if (r < 4) {   // a power of ten and its neighbours
            static const double tens[] = {1e-7, 1e-5, 1e-4, 1e-3, 0.01, 0.1, 1, 10, 100, 1e3, 1e5, 1e6, 1e7, 1e15, 1e16, 1e17, 1e21, 1e22, 1e100, 1e300, 1e-300};
            d = tens[random() % (sizeof tens / sizeof tens[0])];
            memcpy(&u, &d, 8);
            u += (unsigned long long)(int)(random() % 5) - 2;
        } else if (r < 6) {   // short decimals: 0.5, 12.25, -3.75 ...
            d = (double)((int)(random() % 20001) - 10000) / (double)(1 << (random() % 8));
            memcpy(&u, &d, 8);
        } else {   // values with 5, 6, 7 significant digits around rounding ties
            d = (double)(int)(random() % 2000000) * 0.0000005 * (double)(1 + random() % 1000);
            memcpy(&u, &d, 8);
        }
    }
    memcpy(&d, &u, 8);
    return d;
}

// ---- formatting -----------------------------------------------------------------------------------------

constexpr DWORD SPRINTF = 0x5d0db7;

void check_sprintf(Tally &t)
{
    using Fn = int(__cdecl *)(char *, const char *, ...);
    static const char *const float_formats[] = {
        "%g", "%f", "%e", "%E", "%G", "%.1f", "%3.1f", "%10.4g", "%#g", "%+e", "%-12.3E|", "%.0f", "%.17g",
        "%.0e", "%#.0f", "%#.0e", "%.20f", "%015.3f", "% g", "%.3g", "%g, %g%s", "+%g; %.2g%s", "%.10e", "%lf", "%Lg",
        "%.30g", "%12.8e"};
    static const char *const int_formats[] = {
        "%d", "%5d", "%-5d|", "%05d", "%x", "%X", "%#x", "%#X", "%o", "%#o", "%u", "%+d", "% d", "%.3d",
        "%.0d", "%hd", "%hu", "%ld", "%i", "%c", "%010u", "%p", "%%", "%-+8.3d|", "%I", "%I3d", "%q", "%5%",
        "%#.0o", "%hx", "%wc", "%C", "%lc"};
    char a[0x400], b[0x400];
    g_seed = 0x5d0db7;
    for (int k = 0; k < scaled(6000); k++) {
        const bool fl = k % 3 != 2;
        int ra, rb;
        if (fl) {
            const double v = some_double(k / 3 < 40 ? k / 3 : 40 + (int)(random() % 1000));
            const char *f = float_formats[random() % (sizeof float_formats / sizeof float_formats[0])];
            unsigned long long tu;
            memcpy(&tu, &v, 8);
            if (g_crt_trace)
                log("T o %s %08x%08x", f, (DWORD)(tu >> 32), (DWORD)tu);
            ra = orig<Fn>(SPRINTF)(a, f, v, v, "x");
            if (g_crt_trace)
                log("T m");
            rb = mine<Fn>(SPRINTF)(b, f, v, v, "x");
            unsigned long long u;
            memcpy(&u, &v, 8);
            char d[200];
            wsprintfA(d, "[%s %08x%08x: %d '%.40s' / %d '%.40s']", f, (DWORD)(u >> 32), (DWORD)u, ra, a, rb, b);
            t.check(ra == rb && same_text(a, b), "", d);
        } else {
            const int v = random() % 4 == 0 ? (int)random() : (int)(random() % 2001) - 1000;
            const char *f = int_formats[random() % (sizeof int_formats / sizeof int_formats[0])];
            if (g_crt_trace)
                log("T o %s %d", f, v);
            ra = orig<Fn>(SPRINTF)(a, f, v, v);
            if (g_crt_trace)
                log("T m");
            rb = mine<Fn>(SPRINTF)(b, f, v, v);
            char d[160];
            wsprintfA(d, "[%s %d: %d '%.40s' / %d '%.40s']", f, v, ra, a, rb, b);
            t.check(ra == rb && same_text(a, b), "", d);
        }
    }
    // strings, 64-bit, %n, width and precision from the arguments, wide strings
    static const WORD wide[] = {'W', 'i', 'd', 'e', 0x41, 0x1234, 0};
    const char *const texts[] = {"", "abc", "Zax: The Alien Hunter", nullptr};
    for (int k = 0; k < scaled(400); k++) {
        const char *s = texts[k % 4];
        const int w = (int)(random() % 21) - 10, p = (int)(random() % 12) - 2;
        const long long q = (long long)rnd64();
        int na = -1, nb = -1;
        const int ra = orig<Fn>(SPRINTF)(a, "[%*.*s|%-*s|%.2s|%s]%n %I64d %I64x %I64u %*.*f %ls %.3S %hn", w, p, s ? s : "", w, s ? s : "",
                                          s ? s : "", s, &na, q, q, q, w, p, (double)q, wide, wide, &na);
        const int rb = mine<Fn>(SPRINTF)(b, "[%*.*s|%-*s|%.2s|%s]%n %I64d %I64x %I64u %*.*f %ls %.3S %hn", w, p, s ? s : "", w, s ? s : "",
                                          s ? s : "", s, &nb, q, q, q, w, p, (double)q, wide, wide, &nb);
        char d[200];
        wsprintfA(d, "[strings %d %d: %d %d '%.60s' / %d %d '%.60s']", w, p, ra, na, a, rb, nb, b);
        t.check(ra == rb && na == nb && same_text(a, b), "", d);
    }
}

// 80-bit numbers of every kind for $I10_OUTPUT
Ld10 some_ld10()
{
    Ld10 x;
    const unsigned r = random() % 6;
    if (r == 0) {
        double d = some_double((int)(random() % 30));
        crt::dtold(&x, &d);
        return x;
    }
    x.lo = random();
    x.hi = random() | (r == 5 ? 0 : 0x80000000);
    x.exp = (WORD)(r == 1 ? 0x7fff | (random() & 0x8000) : r == 2 ? random() : (0x3fff + (int)(random() % 160) - 80) | (random() & 0x8000));
    return x;
}

void check_float_pieces(Tally &t)
{
    g_seed = 0x5dd69a;
    for (int k = 0; k < scaled(4000); k++) {
        // $I10_OUTPUT
        const Ld10 x = some_ld10();
        const int nd = (int)(random() % 26) - 1;
        const unsigned flags = random() % 2;
        Fos fa, fb;
        memset(&fa, 0x55, sizeof fa);
        memset(&fb, 0x55, sizeof fb);
        const int ra = orig<int(__cdecl *)(Ld10, int, unsigned, Fos *)>(0x5dd69a)(x, nd, flags, &fa);
        const int rb = mine<int(__cdecl *)(Ld10, int, unsigned, Fos *)>(0x5dd69a)(x, nd, flags, &fb);
        char d[160];
        wsprintfA(d, "[$I10_OUTPUT %04x %08x%08x %d %u: %d %d '%.22s' / %d %d '%.22s']", x.exp, x.hi, x.lo, nd, flags, ra, fa.exp,
                  fa.man, rb, fb.exp, fb.man);
        t.check(ra == rb && !memcmp(&fa, &fb, 4 + (fa.man_len > 0 && fa.man_len < 22 ? fa.man_len + 1 : 22)), "", d);
        // __ld12mul and __multtenpow12
        Ld12 p, q, pa, pb;
        for (int i = 0; i < 12; i++) {
            p.b[i] = (BYTE)random();
            q.b[i] = (BYTE)random();
        }
        if (random() % 2) {
            p.w(5) = (WORD)((0x3fff + (int)(random() % 400) - 200) | (random() & 0x8000));
            q.w(5) = (WORD)((0x3fff + (int)(random() % 400) - 200) | (random() & 0x8000));
        }
        pa = pb = p;
        orig<void(__cdecl *)(Ld12 *, const Ld12 *)>(0x5dd23f)(&pa, &q);
        mine<void(__cdecl *)(Ld12 *, const Ld12 *)>(0x5dd23f)(&pb, &q);
        wsprintfA(d, "[__ld12mul %08x%08x%08x x %08x%08x%08x]", p.d(2), p.d(1), p.d(0), q.d(2), q.d(1), q.d(0));
        t.check(!memcmp(&pa, &pb, 12), "", d);
        const int pw = random() % 3 ? (int)(random() % 700) - 350 : (int)(random() % 10000) - 5000;
        const int mult = random() % 2;
        p.w(5) = (WORD)(0x3fff + (int)(random() % 64) - 32);
        p.b[9] |= 0x80;
        pa = pb = p;
        orig<void(__cdecl *)(Ld12 *, int, int)>(0x5dd45f)(&pa, pw, mult);
        mine<void(__cdecl *)(Ld12 *, int, int)>(0x5dd45f)(&pb, pw, mult);
        wsprintfA(d, "[__multtenpow12 %d %d]", pw, mult);
        t.check(!memcmp(&pa, &pb, 12), "", d);
        // __mtold12
        char man[26];
        const unsigned len = 1 + random() % 25;
        for (unsigned i = 0; i < len; i++)
            man[i] = (char)(i == 0 ? 1 + random() % 9 : random() % (random() % 8 ? 10 : 11));
        orig<void(__cdecl *)(const char *, unsigned, Ld12 *)>(0x5dd178)(man, len, &pa);
        mine<void(__cdecl *)(const char *, unsigned, Ld12 *)>(0x5dd178)(man, len, &pb);
        wsprintfA(d, "[__mtold12 len %u]", len);
        t.check(!memcmp(&pa, &pb, 12), "", d);
        // __dtold, _fltout2, _fptostr, the layouts
        double v = some_double(k < 40 ? k : 40 + (int)(random() % 1000));
        Ld10 la, lb;
        orig<void(__cdecl *)(Ld10 *, const double *)>(0x5dc1dd)(&la, &v);
        mine<void(__cdecl *)(Ld10 *, const double *)>(0x5dc1dd)(&lb, &v);
        t.check(la.lo == lb.lo && la.hi == lb.hi && la.exp == lb.exp, "", "[__dtold]");
        StrFlt sa, sb;
        char ma[32], mb[32];
        orig<StrFlt *(__cdecl *)(double, StrFlt *, char *)>(0x5dc181)(v, &sa, ma);
        mine<StrFlt *(__cdecl *)(double, StrFlt *, char *)>(0x5dc181)(v, &sb, mb);
        t.check(sa.sign == sb.sign && sa.decpt == sb.decpt && sa.flag == sb.flag && same_text(ma, mb), "", "[_fltout2]");
        const int digits = (int)(random() % 24) - 2;
        char ba[64], bb[64];
        sa.mantissa = ma;
        sb.mantissa = mb;
        orig<void(__cdecl *)(char *, int, StrFlt *)>(0x5dc10a)(ba, digits, &sa);
        mine<void(__cdecl *)(char *, int, StrFlt *)>(0x5dc10a)(bb, digits, &sb);
        wsprintfA(d, "[_fptostr %d '%s' / '%s']", digits, ba, bb);
        t.check(sa.decpt == sb.decpt && same_text(ba, bb), "", d);
        const int ndec = (int)(random() % 18), caps = random() % 2;
        char ca[400], cb[400];
        static const DWORD layouts[] = {0x5d3ec6, 0x5d40e5};
        const DWORD fn = layouts[random() % 2];
        orig<char *(__cdecl *)(const double *, char *, int, int)>(fn)(&v, ca, ndec, caps);
        mine<char *(__cdecl *)(const double *, char *, int, int)>(fn)(&v, cb, ndec, caps);
        wsprintfA(d, "[%s %d '%.40s' / '%.40s']", fn == 0x5d3ec6 ? "_cftoe" : "_cftog", ndec, ca, cb);
        t.check(same_text(ca, cb), "", d);
        orig<char *(__cdecl *)(const double *, char *, int)>(0x5d3fe9)(&v, ca, ndec);
        mine<char *(__cdecl *)(const double *, char *, int)>(0x5d3fe9)(&v, cb, ndec);
        wsprintfA(d, "[_cftof %d '%.40s' / '%.40s']", ndec, ca, cb);
        t.check(same_text(ca, cb), "", d);
        orig<void(__cdecl *)(char *)>(0x5d3e22)(ca);
        mine<void(__cdecl *)(char *)>(0x5d3e22)(cb);
        t.check(same_text(ca, cb), "", "[_cropzeros]");
        orig<void(__cdecl *)(char *)>(0x5d3dc8)(ca);
        mine<void(__cdecl *)(char *)>(0x5d3dc8)(cb);
        t.check(same_text(ca, cb), "", "[_forcdecpt]");
    }
}

// ---- input ----------------------------------------------------------------------------------------------

void random_number_text(char *s, int max)
{
    static const char *const pieces[] = {" ", "\t", "-", "+", "0", "1", "9", "123", "4567", ".", ".", "e", "E", "d", "D",
                                         "e-", "e+", "00", "999999999", "x", ",", "inf", "1#INF", "3.25", "5", "49999",
                                         "e308", "e-320", "12345678901234567890", "nan", "\n", "a", "f"};
    int n = 0;
    const int parts = 1 + (int)(random() % 8);
    for (int i = 0; i < parts; i++)
        for (const char *p = pieces[random() % (sizeof pieces / sizeof pieces[0])]; *p && n < max - 1;)
            s[n++] = *p++;
    s[n] = 0;
}

void check_input(Tally &t)
{
    using Fn = int(__cdecl *)(const char *, const char *, ...);
    static const char *const formats[] = {
        "%f,%f", "%d", "%010u", "%x", "%i", "%s", "%c", "%5c", "%[a-z0-9]", "%[^,]", "%lf", "%*d %d", "%3d%n", "%f %n%f",
        "%e", "%g", "%le", "%hd", "%ld", "%I64d", "%I64x", "%o", "%p", "%u", " %d", "%d ", "x%d", "%%%d", "%[]a]",
        "%[^]a-]", "%2f", "%[-9]", "%S", "%C", "%lc", "%4s%n", "%*s%n", "%hf", "%Lf", "%5[0-9e.]"};
    char s[64];
    BYTE ta[4][64], tb[4][64];
    g_seed = 0x5d1539;
    for (int k = 0; k < scaled(5000); k++) {
        random_number_text(s, sizeof s);
        const char *f = formats[random() % (sizeof formats / sizeof formats[0])];
        memset(ta, 0x5a, sizeof ta);
        memset(tb, 0x5a, sizeof tb);
        const int ra = orig<Fn>(0x5d1539)(s, f, ta[0], ta[1], ta[2], ta[3]);
        const int rb = mine<Fn>(0x5d1539)(s, f, tb[0], tb[1], tb[2], tb[3]);
        char d[160];
        wsprintfA(d, "[sscanf \"%s\" \"%.30s\": %d / %d]", f, s, ra, rb);
        t.check(ra == rb && !memcmp(ta, tb, sizeof ta), "", d);
        // atof, _fltin, __strgtold12, _atodbl/_atoflt
        const double da = orig<double(__cdecl *)(const char *)>(0x5d03de)(s), db = mine<double(__cdecl *)(const char *)>(0x5d03de)(s);
        wsprintfA(d, "[atof \"%.30s\"]", s);
        t.check(!memcmp(&da, &db, 8), "", d);
        BYTE fa[24], fb[24];
        memset(fa, 0, sizeof fa);
        memset(fb, 0, sizeof fb);
        orig<void *(__cdecl *)(void *, const char *, int, int, int)>(0x5d3cd0)(fa, s, 0, 0, 0);
        mine<void *(__cdecl *)(void *, const char *, int, int, int)>(0x5d3cd0)(fb, s, 0, 0, 0);
        wsprintfA(d, "[_fltin \"%.30s\"]", s);
        t.check(!memcmp(fa, fb, 8) && !memcmp(fa + 16, fb + 16, 8), "", d);
        Ld12 la, lb;
        const char *ea, *eb;
        const int mult = random() % 2;
        const unsigned ga = orig<unsigned(__cdecl *)(Ld12 *, const char **, const char *, int, int, int, int)>(0x5dbad3)(&la, &ea, s, mult, 0, 0, 0);
        const unsigned gb = mine<unsigned(__cdecl *)(Ld12 *, const char **, const char *, int, int, int, int)>(0x5dbad3)(&lb, &eb, s, mult, 0, 0, 0);
        wsprintfA(d, "[__strgtold12 \"%.30s\" %d: %x %d / %x %d]", s, mult, ga, (int)(ea - s), gb, (int)(eb - s));
        t.check(ga == gb && ea == eb && !memcmp(&la, &lb, 12), "", d);
        double xa = 0, xb = 0;
        float ya = 0, yb = 0;
        const int ca = orig<int(__cdecl *)(double *, const char *)>(0x5dba79)(&xa, s), cb = mine<int(__cdecl *)(double *, const char *)>(0x5dba79)(&xb, s);
        const int fa2 = orig<int(__cdecl *)(float *, const char *)>(0x5dbaa6)(&ya, s), fb2 = mine<int(__cdecl *)(float *, const char *)>(0x5dbaa6)(&yb, s);
        wsprintfA(d, "[_atodbl/_atoflt \"%.30s\"]", s);
        t.check(ca == cb && fa2 == fb2 && !memcmp(&xa, &xb, 8) && bits(ya) == bits(yb), "", d);
        // _ld12tod / _ld12tof on random 96-bit numbers
        for (int i = 0; i < 12; i++)
            la.b[i] = (BYTE)random();
        if (random() % 2)
            la.w(5) = (WORD)((0x3fff + (int)(random() % 2400) - 1200) | (random() & 0x8000));
        la.b[9] |= (BYTE)(random() % 4 ? 0x80 : 0);
        const int ta2 = orig<int(__cdecl *)(const Ld12 *, double *)>(0x5dba4d)(&la, &xa), tb2 = mine<int(__cdecl *)(const Ld12 *, double *)>(0x5dba4d)(&la, &xb);
        const int tf = orig<int(__cdecl *)(const Ld12 *, float *)>(0x5dba63)(&la, &ya), tg = mine<int(__cdecl *)(const Ld12 *, float *)>(0x5dba63)(&la, &yb);
        wsprintfA(d, "[_ld12tod/f %08x%08x%08x]", la.d(2), la.d(1), la.d(0));
        t.check(ta2 == tb2 && tf == tg && !memcmp(&xa, &xb, 8) && bits(ya) == bits(yb), "", d);
    }
}

// ---- strings and memory ---------------------------------------------------------------------------------

void random_text(char *s, int n)
{
    for (int i = 0; i < n; i++)
        s[i] = (char)("abcABC xyz\x80\xff,.0"[random() % 16]);
    s[n] = 0;
}

void check_strings(Tally &t)
{
    char a[300], b[300], s[64], u[64];
    g_seed = 0x5d04b0;
    for (int k = 0; k < scaled(3000); k++) {
        // memmove / memcpy with every kind of overlap
        for (int i = 0; i < 300; i++)
            a[i] = b[i] = (char)i;
        const int n = (int)(random() % 120), from = (int)(random() % 150), to = (int)(random() % 150);
        const DWORD fn = random() % 2 ? 0x5d04b0 : 0x5d8230;
        void *ra = orig<void *(__cdecl *)(void *, const void *, size_t)>(fn)(a + to, a + from, (size_t)n);
        void *rb = mine<void *(__cdecl *)(void *, const void *, size_t)>(fn)(b + to, b + from, (size_t)n);
        char d[200];
        wsprintfA(d, "[%s %d %d %d]", fn == 0x5d04b0 ? "memmove" : "memcpy", to, from, n);
        t.check(!memcmp(a, b, 300) && (char *)ra - a == (char *)rb - b, "", d);
        const int fill = (int)random();
        ra = orig<void *(__cdecl *)(void *, int, size_t)>(0x5d8e60)(a + from, fill, (size_t)n);
        rb = mine<void *(__cdecl *)(void *, int, size_t)>(0x5d8e60)(b + from, fill, (size_t)n);
        t.check(!memcmp(a, b, 300) && (char *)ra - a == (char *)rb - b, "", "[memset]");
        // the string functions on random texts
        const int ls = (int)(random() % 20), lu = (int)(random() % 6);
        random_text(s, ls);
        random_text(u, lu);
        if (random() % 3 == 0 && ls > lu)
            memcpy(s + random() % (ls - lu + 1), u, (size_t)lu);
        const int c = random() % 4 ? (BYTE)s[random() % (ls + 1)] : (int)random();
        const size_t cnt = random() % 25;
        auto pos = [](const char *p, const char *base) { return p ? (int)(p - base) : -1; };
        const int r1[] = {
            pos(orig<char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c), s), pos(orig<char *(__cdecl *)(const char *, int)>(0x5d0d90)(s, c), s),
            pos(orig<char *(__cdecl *)(const char *, const char *)>(0x5d1030)(s, u), s),
            pos((const char *)orig<void *(__cdecl *)(const void *, int, size_t)>(0x5e9730)(s, c, cnt), s),
            (int)orig<size_t(__cdecl *)(const char *)>(0x5d3be0)(s), orig<int(__cdecl *)(const char *, const char *)>(0x5db3e0)(s, u),
            orig<int(__cdecl *)(const char *, const char *, size_t)>(0x5d17d0)(s, u, cnt)};
        const int r2[] = {
            pos(mine<char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c), s), pos(mine<char *(__cdecl *)(const char *, int)>(0x5d0d90)(s, c), s),
            pos(mine<char *(__cdecl *)(const char *, const char *)>(0x5d1030)(s, u), s),
            pos((const char *)mine<void *(__cdecl *)(const void *, int, size_t)>(0x5e9730)(s, c, cnt), s),
            (int)mine<size_t(__cdecl *)(const char *)>(0x5d3be0)(s), mine<int(__cdecl *)(const char *, const char *)>(0x5db3e0)(s, u),
            mine<int(__cdecl *)(const char *, const char *, size_t)>(0x5d17d0)(s, u, cnt)};
        wsprintfA(d, "[\"%s\" \"%s\" %d %u: %d %d %d %d %d %d %d / %d %d %d %d %d %d %d]", s, u, c, cnt, r1[0], r1[1], r1[2], r1[3], r1[4],
                  r1[5], r1[6], r2[0], r2[1], r2[2], r2[3], r2[4], r2[5], r2[6]);
        t.check(!memcmp(r1, r2, sizeof r1), "", d);
        memset(a, 0x33, 64);
        memset(b, 0x33, 64);
        orig<char *(__cdecl *)(char *, const char *, size_t)>(0x5d0b50)(a, s, cnt);
        mine<char *(__cdecl *)(char *, const char *, size_t)>(0x5d0b50)(b, s, cnt);
        orig<char *(__cdecl *)(char *, const char *)>(0x5daae0)(a + 30, u);
        mine<char *(__cdecl *)(char *, const char *)>(0x5daae0)(b + 30, u);
        orig<char *(__cdecl *)(char *, const char *)>(0x5daaf0)(a + 30, s);
        mine<char *(__cdecl *)(char *, const char *)>(0x5daaf0)(b + 30, s);
        t.check(!memcmp(a, b, 64), "", "[strncpy/strcpy/strcat]");
        char *da = orig<char *(__cdecl *)(const char *)>(0x5d3606)(s), *db = mine<char *(__cdecl *)(const char *)>(0x5d3606)(s);
        t.check(same_text(da, db), "", "[_strdup]");
        crt::free_(da);
        crt::free_(db);
        // _itoa / xtoa
        const int v = random() % 3 ? (int)random() : (int)(random() % 200) - 100;
        const int radix = 2 + (int)(random() % 35);
        orig<char *(__cdecl *)(int, char *, int)>(0x5dd92d)(v, a, radix);
        mine<char *(__cdecl *)(int, char *, int)>(0x5dd92d)(v, b, radix);
        wsprintfA(d, "[_itoa %d %d '%s' / '%s']", v, radix, a, b);
        t.check(same_text(a, b), "", d);
    }
    // wcslen, wctomb, mbtowc on every value
    for (int w = 0; w < 0x10000; w += 1 + (w > 0x200 ? 97 : 0)) {
        char ca[4] = {}, cb[4] = {};
        const int ra = orig<int(__cdecl *)(char *, WORD)>(0x5dc387)(ca, (WORD)w), rb = mine<int(__cdecl *)(char *, WORD)>(0x5dc387)(cb, (WORD)w);
        int *e = crt::errno_ptr();
        *e = 0;
        WORD wa = 0x5555, wb = 0x5555;
        const char m[2] = {(char)w, 0};
        const int ma = orig<int(__cdecl *)(WORD *, const char *, size_t)>(0x5dc598)(&wa, m, 1 + w % 2);
        const int mb = mine<int(__cdecl *)(WORD *, const char *, size_t)>(0x5dc598)(&wb, m, 1 + w % 2);
        char d[80];
        wsprintfA(d, "[wctomb/mbtowc %x: %d %d / %d %d]", w, ra, ma, rb, mb);
        t.check(ra == rb && ca[0] == cb[0] && ma == mb && wa == wb, "", d);
    }
    static const WORD ws[] = {'a', 'b', 0x1234, 0};
    for (int i = 0; i < 4; i++)
        t.check(orig<size_t(__cdecl *)(const WORD *)>(0x5d1789)(ws + i) == mine<size_t(__cdecl *)(const WORD *)>(0x5d1789)(ws + i), "", "[wcslen]");
}

// The hooked case, class, number, qsort and rand functions against their originals (crt_string.cpp)
int __cdecl compare_ints(const void *a, const void *b) { return (*(const int *)a >> 4) - (*(const int *)b >> 4); }

void check_misc(Tally &t)
{
    using I1 = int(__cdecl *)(int);
    static const DWORD by_char[] = {0x5d0c4e, 0x5d0cbd, 0x5d272a, 0x5d2799, 0x5d0f29, 0x5d0f51, 0x5d0f79};
    for (int c = -1; c < 256; c++)
        for (DWORD fn : by_char) {
            char d[48];
            wsprintfA(d, "[0x%x %d]", fn, c);
            t.check(orig<I1>(fn)(c) == mine<I1>(fn)(c), "", d);
        }
    for (int c = -1; c < 256; c++)
        for (int mask = 1; mask < 0x200; mask <<= 1)
            t.check(orig<int(__cdecl *)(int, int)>(0x5d3c5b)(c, mask) == mine<int(__cdecl *)(int, int)>(0x5d3c5b)(c, mask), "", "[_isctype]");
    char s[40], u[40];
    g_seed = 0x5d10b0;
    for (int k = 0; k < scaled(5000); k++) {
        random_number_text(s, sizeof s);
        random_text(u, (int)(random() % 8));
        const int base = (int)(random() % 40) - 2;
        char *ea, *eb;
        int *e = crt::errno_ptr();
        *e = 0;
        const long va = orig<long(__cdecl *)(const char *, char **, int)>(0x5d10b0)(s, &ea, base);
        const int erra = *e;
        *e = 0;
        const long vb = mine<long(__cdecl *)(const char *, char **, int)>(0x5d10b0)(s, &eb, base);
        char d[120];
        wsprintfA(d, "[strtol \"%s\" %d: %d %d %d / %d %d %d]", s, base, va, (int)(ea - s), erra, vb, (int)(eb - s), *e);
        t.check(va == vb && ea == eb && erra == *e, "", d);
        t.check(orig<long(__cdecl *)(const char *)>(0x5d0348)(s) == mine<long(__cdecl *)(const char *)>(0x5d0348)(s) &&
                    orig<int(__cdecl *)(const char *)>(0x5d03d3)(s) == mine<int(__cdecl *)(const char *)>(0x5d03d3)(s),
                "", "[atol/atoi]");
        const int n = (int)(random() % 10);
        t.check(orig<int(__cdecl *)(const char *, const char *)>(0x5d3640)(s, u) == mine<int(__cdecl *)(const char *, const char *)>(0x5d3640)(s, u) &&
                    orig<int(__cdecl *)(const char *, const char *, size_t)>(0x5dd9c0)(s, u, (size_t)n) ==
                        mine<int(__cdecl *)(const char *, const char *, size_t)>(0x5dd9c0)(s, u, (size_t)n),
                "", "[_stricmp/_strnicmp]");
    }
    int qa[200], qb[200];
    for (int k = 0; k < scaled(300); k++) {
        const int n = (int)(random() % 200);
        for (int i = 0; i < n; i++)
            qa[i] = qb[i] = (int)(random() % 64);
        orig<void(__cdecl *)(void *, unsigned, unsigned, int(__cdecl *)(const void *, const void *))>(0x5d097c)(qa, (unsigned)n, 4, compare_ints);
        mine<void(__cdecl *)(void *, unsigned, unsigned, int(__cdecl *)(const void *, const void *))>(0x5d097c)(qb, (unsigned)n, 4, compare_ints);
        t.check(!memcmp(qa, qb, 4 * (size_t)n), "", "[qsort]");
    }
    DWORD *ptd = (DWORD *)crt::getptd();
    const DWORD saved = ptd[5];
    for (int k = 0; k < 50; k++) {
        const unsigned seed = random();
        orig<void(__cdecl *)(unsigned)>(0x5d218b)(seed);
        int ra[8];
        for (int &r : ra)
            r = orig<int(__cdecl *)()>(0x5d2198)();
        const DWORD ha = ptd[5];
        mine<void(__cdecl *)(unsigned)>(0x5d218b)(seed);
        bool same = true;
        for (int r : ra)
            same = same && r == mine<int(__cdecl *)()>(0x5d2198)();
        t.check(same && ha == ptd[5], "", "[srand/rand]");
    }
    ptd[5] = saved;
}

// ---- errno, per-thread data -----------------------------------------------------------------------------

void check_errno(Tally &t)
{
    t.check(orig<void *(__cdecl *)()>(0x5d4f28)() == mine<void *(__cdecl *)()>(0x5d4f28)(), "", "[_getptd]");
    t.check(orig<int *(__cdecl *)()>(0x5d1014)() == mine<int *(__cdecl *)()>(0x5d1014)(), "", "[_errno]");
    t.check(orig<DWORD *(__cdecl *)()>(0x5d101d)() == mine<DWORD *(__cdecl *)()>(0x5d101d)(), "", "[__doserrno]");
    int *e = crt::errno_ptr();
    DWORD *de = crt::doserrno_ptr();
    const int saved = *e;
    const DWORD saved_dos = *de;
    for (DWORD code = 0; code <= 300; code++) {
        *e = -7;
        *de = 7;
        orig<void(__cdecl *)(DWORD)>(0x5d0fa1)(code);
        const int ea = *e;
        const DWORD da = *de;
        *e = -7;
        *de = 7;
        mine<void(__cdecl *)(DWORD)>(0x5d0fa1)(code);
        char d[64];
        wsprintfA(d, "[_dosmaperr %u: %d %u / %d %u]", code, ea, da, *e, *de);
        t.check(ea == *e && da == *de, "", d);
    }
    *e = saved;
    *de = saved_dos;
}

// ---- heap -----------------------------------------------------------------------------------------------

void check_heap(Tally &t)
{
    using Malloc = void *(__cdecl *)(size_t);
    using Free = void(__cdecl *)(void *);
    using Msize = size_t(__cdecl *)(void *);
    using Calloc = void *(__cdecl *)(size_t, size_t);
    using Realloc = void *(__cdecl *)(void *, size_t);
    log("  CRT heap: __active_heap %d, _crtheap %x", *at<int *>(crt::ACTIVE_HEAP), *at<DWORD *>(crt::CRT_HEAP));
    g_seed = 0x5d164f;
    for (int k = 0; k < scaled(2000); k++) {
        const size_t n = k < 40 ? (size_t)k : random() % 4 ? random() % 2000 : random() % 200000;
        const bool cross = random() % 2;
        BYTE *pa = (BYTE *)orig<Malloc>(0x5d164f)(n), *pb = (BYTE *)mine<Malloc>(0x5d164f)(n);
        size_t sa = orig<Msize>(0x5d41ee)(pa), sb = mine<Msize>(0x5d41ee)(pb);
        char d[96];
        wsprintfA(d, "[malloc %u: _msize %u / %u]", n, sa, sb);
        t.check(pa && pb && sa == sb, "", d);
        for (size_t i = 0; i < n; i++)
            pa[i] = pb[i] = (BYTE)(i * 7);
        const size_t m = random() % 3 ? random() % 3000 : 0;
        BYTE *qa = (BYTE *)orig<Realloc>(0x5d3745)(cross ? pb : pa, m), *qb = (BYTE *)mine<Realloc>(0x5d3745)(cross ? pa : pb, m);
        bool same = !qa == !qb;
        if (qa && qb) {
            sa = orig<Msize>(0x5d41ee)(qa);
            sb = mine<Msize>(0x5d41ee)(qb);
            same = same && sa == sb && !memcmp(qa, qb, n < m ? n : m);
        }
        wsprintfA(d, "[realloc %u -> %u: %u / %u]", n, m, sa, sb);
        t.check(same, "", d);
        if (qa)
            (cross ? mine<Free>(0x5d33e0) : orig<Free>(0x5d33e0))(qa);
        if (qb)
            (cross ? orig<Free>(0x5d33e0) : mine<Free>(0x5d33e0))(qb);
        const size_t num = random() % 50, size = random() % 50;
        pa = (BYTE *)orig<Calloc>(0x5d34c9)(num, size);
        pb = (BYTE *)mine<Calloc>(0x5d34c9)(num, size);
        bool zero = pa && pb && orig<Msize>(0x5d41ee)(pa) == mine<Msize>(0x5d41ee)(pb);
        for (size_t i = 0; zero && i < num * size; i++)
            zero = !pa[i] && !pb[i];
        wsprintfA(d, "[calloc %u x %u]", num, size);
        t.check(zero, "", d);
        mine<Free>(0x5d33e0)(pa);
        orig<Free>(0x5d33e0)(pb);
        pa = (BYTE *)orig<Malloc>(0x5d033a)(n);
        pb = (BYTE *)mine<Malloc>(0x5d033a)(n);
        t.check(pa && pb && orig<Msize>(0x5d41ee)(pa) == orig<Msize>(0x5d41ee)(pb), "", "[operator new]");
        orig<Free>(0x5d0320)(pb);
        mine<Free>(0x5d0320)(pa);
    }
    orig<Free>(0x5d33e0)(nullptr);
    mine<Free>(0x5d33e0)(nullptr);
    t.check(!orig<Realloc>(0x5d3745)(orig<Malloc>(0x5d164f)(5), 0) && !mine<Realloc>(0x5d3745)(mine<Malloc>(0x5d164f)(5), 0), "", "[realloc to 0]");
    t.check(orig<Malloc>(0x5d164f)(0xffffffe1) == mine<Malloc>(0x5d164f)(0xffffffe1), "", "[malloc too big]");
}

// ---- exit table, run-time error messages ----------------------------------------------------------------

DWORD g_exit_log[64];
int g_exit_n;
void __cdecl exit_fn_a() { g_exit_log[g_exit_n++ & 63] = 1; }
void __cdecl exit_fn_b() { g_exit_log[g_exit_n++ & 63] = 2; }
void __cdecl exit_fn_c() { g_exit_log[g_exit_n++ & 63] = 3; }
char g_box[512];
unsigned g_box_type;
int WINAPI fake_message_box(HANDLE, const char *text, const char *caption, unsigned type)
{
    lstrcpyA(g_box, text);
    lstrcatA(g_box, "|");
    lstrcatA(g_box, caption);
    g_box_type = type;
    return 1;
}

void check_exit(Tally &t)
{
    using Pfv = void(__cdecl *)();
    constexpr DWORD BEGIN = 0x71755c, END = 0x717558, C_EXIT_DONE = 0x7158bc, TERM_DONE = 0x7158b8, EXITFLAG = 0x7158b4;
    constexpr DWORD XP_A = 0x60a40c, XT_A = 0x60a418;   // pre-terminators and terminators, 2 entries each
    const DWORD saved_begin = *at<DWORD *>(BEGIN), saved_end = *at<DWORD *>(END);
    const DWORD saved_flags[3] = {*at<DWORD *>(C_EXIT_DONE), *at<DWORD *>(TERM_DONE), *at<DWORD *>(EXITFLAG)};
    DWORD saved_tables[4];
    memcpy(saved_tables, at<void *>(XP_A), 8);
    memcpy(saved_tables + 2, at<void *>(XT_A), 8);
    const Pfv fns[3] = {exit_fn_a, exit_fn_b, exit_fn_c};
    g_seed = 0x5d07e5;
    for (int k = 0; k < scaled(200); k++) {
        unsigned h[2];
        int count[2];
        DWORD sizes[2];
        const int n = (int)(random() % 40);
        unsigned picks[40];
        for (int i = 0; i < n; i++)
            picks[i] = random() % 4;
        const int quick = random() % 2;
        for (int side = 0; side < 2; side++) {
            Pfv *table = (Pfv *)crt::malloc_(0x80);
            *at<Pfv **>(BEGIN) = table;
            *at<Pfv **>(END) = table;
            unsigned hh = 2166136261u;
            for (int i = 0; i < n; i++) {
                const Pfv f = picks[i] == 3 ? nullptr : fns[picks[i]];
                const DWORD r = i % 2 ? (DWORD)(side ? mine<int(__cdecl *)(Pfv)>(0x5d0863) : orig<int(__cdecl *)(Pfv)>(0x5d0863))(f)
                                      : (DWORD)(uintptr_t)(side ? mine<Pfv(__cdecl *)(Pfv)>(0x5d07e5) : orig<Pfv(__cdecl *)(Pfv)>(0x5d07e5))(f);
                hh = fnv(hh, &r, 4);
            }
            sizes[side] = (DWORD)((BYTE *)*at<Pfv **>(END) - (BYTE *)*at<Pfv **>(BEGIN));
            // doexit with retcaller (_cexit's way) on these tables and empty pre-terminators/terminators
            memset(at<void *>(XP_A), 0, 8);
            memset(at<void *>(XT_A), 0, 8);
            *at<DWORD *>(C_EXIT_DONE) = 0;
            g_exit_n = 0;
            (side ? mine<void(__cdecl *)(int, int, int)>(0x5d0e58) : orig<void(__cdecl *)(int, int, int)>(0x5d0e58))(3, quick, 1);
            hh = fnv(hh, g_exit_log, 4 * (g_exit_n < 64 ? g_exit_n : 64));
            hh = fnv(hh, at<void *>(TERM_DONE), 4);
            hh = fnv(hh, at<void *>(EXITFLAG), 1);
            h[side] = hh;
            count[side] = g_exit_n;
            crt::free_(*at<Pfv **>(BEGIN));
            memcpy(at<void *>(XP_A), saved_tables, 8);
            memcpy(at<void *>(XT_A), saved_tables + 2, 8);
        }
        char d[96];
        wsprintfA(d, "[atexit/_onexit/doexit %d %d: %d %u / %d %u]", n, quick, count[0], sizes[0], count[1], sizes[1]);
        t.check(h[0] == h[1] && sizes[0] == sizes[1], "", d);
    }
    *at<DWORD *>(BEGIN) = saved_begin;
    *at<DWORD *>(END) = saved_end;
    *at<DWORD *>(C_EXIT_DONE) = saved_flags[0];
    *at<DWORD *>(TERM_DONE) = saved_flags[1];
    *at<DWORD *>(EXITFLAG) = saved_flags[2];
    // _initterm
    Pfv table[8];
    for (int k = 0; k < 20; k++) {
        for (int i = 0; i < 8; i++)
            table[i] = random() % 3 ? fns[random() % 3] : nullptr;
        const int n = (int)(random() % 9);
        g_exit_n = 0;
        orig<void(__cdecl *)(Pfv *, Pfv *)>(0x5d0f0f)(table, table + n);
        DWORD la[64];
        const int na = g_exit_n;
        memcpy(la, g_exit_log, sizeof la);
        g_exit_n = 0;
        mine<void(__cdecl *)(Pfv *, Pfv *)>(0x5d0f0f)(table, table + n);
        t.check(na == g_exit_n && !memcmp(la, g_exit_log, 4 * (na < 64 ? na : 64)), "", "[_initterm]");
    }
    // _NMSG_WRITE through a recording message box (the cached MessageBoxA pointer of __crtMessageBoxA)
    constexpr DWORD MSGBOX = 0x715c1c, ACTIVE = 0x715c20, POPUP = 0x715c24;
    const DWORD saved_box[3] = {*at<DWORD *>(MSGBOX), *at<DWORD *>(ACTIVE), *at<DWORD *>(POPUP)};
    *at<void **>(MSGBOX) = (void *)&fake_message_box;
    *at<DWORD *>(ACTIVE) = 0;
    *at<DWORD *>(POPUP) = 0;
    // every number of the table and some outside it (not 0: the entry behind the table is {0, null}, both sides would
    // take its null text)
    static const int numbers[] = {2, 8, 9, 10, 16, 17, 18, 19, 24, 25, 26, 27, 28, 30, 31, 32, 120, 121, 122, 252, 255, 1, 99};
    for (int number : numbers) {
        char ba[512];
        g_box[0] = 0;
        g_box_type = 0;
        orig<void(__cdecl *)(int)>(0x5db211)(number);
        lstrcpyA(ba, g_box);
        const unsigned ta = g_box_type;
        g_box[0] = 0;
        g_box_type = 0;
        mine<void(__cdecl *)(int)>(0x5db211)(number);
        char d[200];
        wsprintfA(d, "[_NMSG_WRITE %d: '%.60s' / '%.60s']", number, ba, g_box);
        t.check(same_text(ba, g_box) && ta == g_box_type, "", d);
    }
    *at<DWORD *>(MSGBOX) = saved_box[0];
    *at<DWORD *>(ACTIVE) = saved_box[1];
    *at<DWORD *>(POPUP) = saved_box[2];
}

// ---- files ----------------------------------------------------------------------------------------------

struct FileOps {
    bool reimpl;
    template <typename F> F fn(DWORD a) const { return reimpl ? mine<F>(a) : orig<F>(a); }
};

unsigned file_state(unsigned h, const File *f)
{
    if (!f)
        return fnv(h, "null", 4);
    // _getstream does not clear bufsiz: before a buffer is assigned it is whatever the slot's last stream left
    const bool buffered = f->flag & (crt::IOMYBUF | crt::IOYOURBUF | crt::IONBF);
    const DWORD v[4] = {(DWORD)f->cnt, (DWORD)f->flag, buffered ? (DWORD)f->bufsiz : 0, (DWORD)(f->ptr - f->base)};
    return fnv(h, v, sizeof v);
}

// The last contents read per side (for the log of a case whose contents alone differ: batch 6-13, the one "files"
// difference of 6-12's run 20261030-142958-fe69 could not be told from its hashes)
BYTE g_file_copy[2][4096];
DWORD g_file_len[2];

unsigned file_contents(unsigned h, const char *path, int side = -1)
{
    HANDLE f = CreateFileA(path, 0x80000000, 3, nullptr, 3, 0x80, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        if (side >= 0)
            g_file_len[side] = 0xffffffffu;
        return fnv(h, "none", 4);
    }
    using ReadFileFn = BOOL(WINAPI *)(HANDLE, void *, DWORD, DWORD *, void *);
    BYTE buf[4096];
    DWORD got = 0;
    crt::api<ReadFileFn>(crt::iat::ReadFile)(f, buf, sizeof buf, &got, nullptr);
    CloseHandle(f);
    if (side >= 0) {
        memcpy(g_file_copy[side], buf, got);
        g_file_len[side] = got;
    }
    return fnv(fnv(h, &got, 4), buf, got);
}

// Where two sides' contents part: the sizes, the first differing offset and 32 bytes of each side from there
void log_contents_difference()
{
    const DWORD n0 = g_file_len[0], n1 = g_file_len[1];
    if (n0 == 0xffffffffu || n1 == 0xffffffffu) {
        log("    contents: a side's file could not be opened (%d / %d)", (int)n0, (int)n1);
        return;
    }
    DWORD at = 0;
    while (at < n0 && at < n1 && g_file_copy[0][at] == g_file_copy[1][at])
        at++;
    char hex[2][100];
    for (int s = 0; s < 2; s++) {
        hex[s][0] = 0;
        for (DWORD i = at; i < at + 32 && i < g_file_len[s]; i++)
            wsprintfA(hex[s] + lstrlenA(hex[s]), "%02x", g_file_copy[s][i]);
    }
    log("    contents: sizes %u / %u, first difference at %u: %s / %s", n0, n1, at, hex[0], hex[1]);
}

void check_files(Tally &t)
{
    char dir[MAX_PATH], path[2][MAX_PATH], other[2][MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, dir)) {
        t.check(false, "", "[no temp folder]");
        return;
    }
    for (int s = 0; s < 2; s++) {
        wsprintfA(path[s], "%szax_crt_%d.tmp", dir, s);
        wsprintfA(other[s], "%szax_crt_%d.ren", dir, s);
    }
    static const char *const modes[] = {"wb", "w+b", "rb", "r+b", "ab", "a+b", "wt", "w+t", "rt", "r+t", "at", "w", "r+", "wbc", "rbn", "wD", "x", "w+b+"};
    int *e = crt::errno_ptr();
    g_seed = 0x5d1a29;
    for (int k = 0; k < scaled(300); k++) {
        unsigned h[2];
        unsigned seed = random();
        char trace[2][96];
        DWORD steps[2][16][8];
        memset(steps, 0, sizeof steps);
        for (int side = 0; side < 2; side++) {
            const FileOps ops{side == 1};
            g_seed = seed;
            unsigned hh = 2166136261u;
            DeleteFileA(path[side]);
            DeleteFileA(other[side]);
            // a start content (some cases read an existing file)
            if (random() % 2) {
                File *f = ops.fn<File *(__cdecl *)(const char *, const char *)>(0x5d1a29)(path[side], "wb");
                char data[300];
                const int n = (int)(random() % 300);
                for (int i = 0; i < n; i++)
                    data[i] = "ab\n\r\x1a z"[random() % 7];
                if (f) {
                    ops.fn<size_t(__cdecl *)(const void *, size_t, size_t, File *)>(0x5d1a3c)(data, 1, (size_t)n, f);
                    ops.fn<int(__cdecl *)(File *)>(0x5d1808)(f);
                }
            }
            const char *mode = modes[random() % (sizeof modes / sizeof modes[0])];
            File *f = ops.fn<File *(__cdecl *)(const char *, const char *)>(0x5d1a29)(path[side], mode);
            hh = file_state(hh, f);
            int last = 0, direction = 0;
            for (int op = 0; f && op < 12; op++) {
                *e = 0;
                const unsigned what = random() % 7;
                DWORD r = 0;
                // C requires a positioning call between writing and reading (without it fread copies the
                // unwritten rest of the write buffer, uninitialised memory)
                const int dir = what == 1 ? 1 : what == 0 || what == 5 ? 2 : 0;
                if (dir && direction && dir != direction) {
                    const int rs = ops.fn<int(__cdecl *)(File *, long, int)>(0x5d1c8c)(f, 0, 1);
                    hh = fnv(hh, &rs, 4);
                }
                if (dir)
                    direction = dir;
                char buf[700];
                if (what == 0) {
                    const int n = (int)(random() % 600), size = 1 + (int)(random() % 3);
                    for (int i = 0; i < n * size && i < 700; i++)
                        buf[i] = "xy\n\r\x1a 0"[random() % 7];
                    r = (DWORD)ops.fn<size_t(__cdecl *)(const void *, size_t, size_t, File *)>(0x5d1a3c)(buf, (size_t)size, (size_t)(n < 700 / size ? n : 700 / size), f);
                } else if (what == 1) {
                    memset(buf, 0, sizeof buf);
                    const int n = (int)(random() % 600);
                    r = (DWORD)ops.fn<size_t(__cdecl *)(void *, size_t, size_t, File *)>(0x5d1b75)(buf, 1, (size_t)n, f);
                    hh = fnv(hh, buf, (unsigned)n);
                } else if (what == 2)
                    r = (DWORD)ops.fn<int(__cdecl *)(File *, long, int)>(0x5d1c8c)(f, (long)(random() % 500) - 100, (int)(random() % 4));
                else if (what == 3)
                    r = (DWORD)ops.fn<long(__cdecl *)(File *)>(0x5d1d45)(f);
                else if (what == 4) {
                    const int fd = ops.fn<int(__cdecl *)(File *)>(0x5de2e0)(f);
                    r = (DWORD)ops.fn<int(__cdecl *)(int, long)>(0x5d1ec8)(fd, (long)(random() % 800));
                } else if (what == 5)
                    r = (DWORD)ops.fn<int(__cdecl *)(File *, const char *, ...)>(0x5d19bc)(f, "%d %s %g\n", (int)random(), "text", 1.5);
                else {
                    // the low-level functions on the stream's handle
                    const int fd = ops.fn<int(__cdecl *)(File *)>(0x5de2e0)(f);
                    r = (DWORD)ops.fn<long(__cdecl *)(int, long, int)>(0x5d887f)(fd, 0, (int)(random() % 3));
                    char c[8];
                    r ^= (DWORD)ops.fn<int(__cdecl *)(int, char *, unsigned)>(0x5d8641)(fd, c, 3) << 8;
                }
                hh = fnv(hh, &r, 4);
                hh = fnv(hh, e, 4);
                hh = file_state(hh, f);
                last = (int)what;
                HANDLE oh = crt::ioinfo(f->file)->osfhnd;
                const DWORD ospos = crt::api<DWORD(WINAPI *)(HANDLE, long, long *, DWORD)>(crt::iat::SetFilePointer)(oh, 0, nullptr, 1);
                const DWORD step[8] = {what, r, (DWORD)*e, (DWORD)f->cnt, (DWORD)f->flag, (DWORD)(f->ptr - f->base), ospos, (DWORD)f->bufsiz};
                memcpy(steps[side][op], step, sizeof step);
            }
            int rc = 99;
            if (f) {
                rc = ops.fn<int(__cdecl *)(File *)>(0x5d1808)(f);
                hh = fnv(hh, &rc, 4);
            }
            const unsigned contents = file_contents(0, path[side], side);
            hh = fnv(hh, &contents, 4);
            const int rr = ops.fn<int(__cdecl *)(const char *, const char *)>(0x5d2046)(path[side], other[side]);
            const int rm = ops.fn<int(__cdecl *)(const char *)>(0x5d17a6)(other[side]);
            const int rm2 = ops.fn<int(__cdecl *)(const char *)>(0x5d17a6)(other[side]);
            hh = fnv(fnv(fnv(hh, &rr, 4), &rm, 4), &rm2, 4);
            hh = fnv(hh, e, 4);
            h[side] = hh;
            wsprintfA(trace[side], "%s %x %d close %d contents %x %d %d %d", mode, hh, last, rc, contents, rr, rm, rm2);
        }
        char d[400];
        int op = 0;
        while (op < 15 && !memcmp(steps[0][op], steps[1][op], sizeof steps[0][op]))
            op++;
        const DWORD *x = steps[0][op], *y = steps[1][op];
        const DWORD *w = op ? steps[0][op - 1] : steps[0][0];
        wsprintfA(d, "[file sequence %u: %s / %s; op %d (after %u %x): %u %x %d %d %x %d pos %x buf %x / %u %x %d %d %x %d pos %x buf %x]", seed,
                  trace[0], trace[1], op, w[0], w[1], x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7]);
        t.check(h[0] == h[1], "", d);
        if (h[0] != h[1] && t.mismatches <= 2)
            log_contents_difference();
        if (h[0] != h[1] && t.mismatches <= 2)
            for (int side = 0; side < 2; side++)
                for (int i = 0; i < 12; i++)
                    log("    side %d op %d: what %u r %x errno %d cnt %d flag %x ptr %d pos %x buf %x", side, i, steps[side][i][0], steps[side][i][1],
                        steps[side][i][2], steps[side][i][3], steps[side][i][4], steps[side][i][5], steps[side][i][6], steps[side][i][7]);
        g_seed = seed + 1;
    }
    // _sopen with every share and open flag on handles
    for (int k = 0; k < scaled(200); k++) {
        unsigned hh[2];
        const int oflag = (int)(random() % 4) | (int)((random() % 8) << 8) | (random() % 2 ? 0x8000 : 0x4000) | (random() % 4 == 0 ? 8 : 0);
        const int sh = 0x10 * (int)(random() % 5);
        for (int side = 0; side < 2; side++) {
            const FileOps ops{side == 1};
            *e = 0;
            const int fh = ops.fn<int(__cdecl *)(const char *, int, int, int)>(0x5dc74d)(path[side], oflag, sh, 0x180);
            unsigned x = fnv(fnv(2166136261u, &fh, 4), e, 4);
            if (fh >= 0) {
                const int w = ops.fn<int(__cdecl *)(int, const char *, unsigned)>(0x5d7d45)(fh, "line\nnext\r\n\x1a", 12);
                const long p = ops.fn<long(__cdecl *)(int, long, int)>(0x5d887f)(fh, 0, 0);
                char buf[32] = {};
                const int r = ops.fn<int(__cdecl *)(int, char *, unsigned)>(0x5d8641)(fh, buf, 32);
                const int c = ops.fn<int(__cdecl *)(int)>(0x5d7ba7)(fh);
                x = fnv(fnv(fnv(fnv(fnv(x, &w, 4), &p, 4), &r, 4), buf, 32), &c, 4);
                x = fnv(x, e, 4);
            }
            x = file_contents(x, path[side]);
            hh[side] = x;
        }
        char d[64];
        wsprintfA(d, "[_sopen %x %x]", oflag, sh);
        t.check(hh[0] == hh[1], "", d);
    }
    for (int s = 0; s < 2; s++) {
        DeleteFileA(path[s]);
        DeleteFileA(other[s]);
    }
}

// ---- 64-bit helpers, __ftol, _chkstk ---------------------------------------------------------------------

// A helper called with its convention: a and b on the stack (callee pops), result in edx:eax
unsigned long long call_ll(void *fn, unsigned long long a, unsigned long long b)
{
    // pushed from memory through a register: operands relative to esp would move with every push
    const DWORD args[4] = {(DWORD)a, (DWORD)(a >> 32), (DWORD)b, (DWORD)(b >> 32)};
    unsigned long long r;
    __asm__ volatile("pushl 12(%1)\n\tpushl 8(%1)\n\tpushl 4(%1)\n\tpushl (%1)\n\tcall *%2"
                     : "=&A"(r)
                     : "S"(args), "D"(fn)
                     : "ecx", "memory");
    return r;
}
// A shift: value in edx:eax, count in cl
unsigned long long call_shift(void *fn, unsigned long long v, unsigned count)
{
    unsigned long long r;
    __asm__ volatile("call *%3" : "=A"(r), "+c"(count) : "A"(v), "r"(fn) : "memory");   // the shifts change cl
    return r;
}

void check_int64(Tally &t)
{
    static const DWORD helpers[] = {0x5d2640, 0x5d2680, 0x5d1570, 0x5db370, 0x5dc450};
    static const char *const names[] = {"__allmul", "__alldiv", "__allrem", "__aulldiv", "__aullrem"};
    // the shifts' original bytes as executable copies (no trampolines)
    static const DWORD shifts[] = {0x5dc6c0, 0x5e9700, 0x5d1630};
    static const char *const shift_names[] = {"__allshl", "__allshr", "__aullshr"};
    static const BYTE first[3][5] = {{0x80, 0xf9, 0x40, 0x73, 0x15}, {0x80, 0xf9, 0x40, 0x73, 0x16}, {0x80, 0xf9, 0x40, 0x73, 0x15}};
    BYTE *copies = (BYTE *)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    for (int i = 0; copies && i < 3; i++) {
        memcpy(copies + 64 * i, first[i], 5);
        memcpy(copies + 64 * i + 5, at<BYTE *>(shifts[i] + 5), 31);
    }
    g_seed = 0x5d2640;
    for (int k = 0; k < scaled(20000); k++) {
        const int which = (int)(random() % 5);
        unsigned long long a = rnd64(), b = rnd64();
        const unsigned r = random() % 6;
        if (r == 0)
            b >>= 32;
        else if (r == 1)
            b >>= 33 + random() % 31;
        else if (r == 2)
            a >>= random() % 64;
        if (!b)
            b = 1;
        const unsigned long long ra = call_ll(original_address(helpers[which]), a, b), rb = call_ll(at<void *>(helpers[which]), a, b);
        char d[120];
        wsprintfA(d, "[%s %08x%08x %08x%08x]", names[which], (DWORD)(a >> 32), (DWORD)a, (DWORD)(b >> 32), (DWORD)b);
        t.check(ra == rb, "", d);
        if (copies) {
            const int s = (int)(random() % 3);
            const unsigned count = random() % 4 ? random() % 70 : random();
            const unsigned long long sa = call_shift(copies + 64 * s, a, count), sb = call_shift(at<void *>(shifts[s]), a, count);
            wsprintfA(d, "[%s %08x%08x %u]", shift_names[s], (DWORD)(a >> 32), (DWORD)a, count & 0xff);
            t.check(sa == sb, "", d);
        }
    }
    if (copies)
        VirtualFree(copies, 0, 0x8000);
    log("  __ftol ...");
    // __ftol through both, on the same doubles as the crt module
    for (int k = 0; k < scaled(5000); k++) {
        double x = some_double(k < 25 ? k : 25 + (int)(random() % 1000));
        long long ra, rb;
        void *fa = original_address(0x5d0488), *fb = at<void *>(0x5d0488);
        __asm__ volatile("fldl %1\n\tcall *%2" : "=A"(ra) : "m"(x), "r"(fa) : "ecx", "memory", "st");
        __asm__ volatile("fldl %1\n\tcall *%2" : "=A"(rb) : "m"(x), "r"(fb) : "ecx", "memory", "st");
        t.check(ra == rb, "", "[__ftol]");
    }
    log("  _chkstk ...");
    // _chkstk: the stack pointer after the probe (relative to before), sizes up to 64 KB
    for (int k = 0; k < 64; k++) {
        const DWORD size = k < 8 ? (DWORD)k * 0x800 : random() % 0x10000;
        DWORD da, db;
        void *fa = original_address(0x5d2080), *fb = at<void *>(0x5d2080);
        __asm__ volatile("movl %%esp, %%edx\n\tcall *%2\n\tmovl %%esp, %%eax\n\tmovl %%edx, %%esp\n\tsubl %%eax, %%edx\n\tmovl %%edx, %0"
                         : "=r"(da) : "a"(size), "r"(fa) : "edx", "ecx", "memory");
        __asm__ volatile("movl %%esp, %%edx\n\tcall *%2\n\tmovl %%esp, %%eax\n\tmovl %%edx, %%esp\n\tsubl %%eax, %%edx\n\tmovl %%edx, %0"
                         : "=r"(db) : "a"(size), "r"(fb) : "edx", "ecx", "memory");
        t.check(da == db, "", "[_chkstk]");
    }
}

}  // namespace

ZAX_CHECKS(crt_impl)
{
    (void)cls;
    (void)n;
    Tally a{"crt_impl: sprintf (_output)"};
    log("  %s ...", "sprintf (_output)");
    check_sprintf(a);
    a.report();
    Tally b{"crt_impl: $I10_OUTPUT, 96-bit arithmetic, %e/%f/%g layouts"};
    log("  %s ...", "$I10_OUTPUT, 96-bit arithmetic, %e/%f/%g");
    check_float_pieces(b);
    b.report();
    Tally c{"crt_impl: sscanf, atof, _fltin, __strgtold12, _ld12tod/f"};
    log("  %s ...", "sscanf, atof, _fltin, __strgtold12, _ld1");
    check_input(c);
    c.report();
    Tally d{"crt_impl: string and memory functions, _itoa, wctomb/mbtowc"};
    log("  %s ...", "string and memory functions, _itoa, wcto");
    check_strings(d);
    d.report();
    Tally m{"crt_impl: case and class functions, atol/strtol, _stricmp, qsort, rand"};
    log("  %s ...", "case and class functions, atol/strtol, _");
    check_misc(m);
    m.report();
    Tally e{"crt_impl: _getptd, errno, _dosmaperr"};
    log("  %s ...", "_getptd, errno, _dosmaperr");
    check_errno(e);
    e.report();
    Tally f{"crt_impl: malloc/calloc/realloc/_msize/free, new/delete"};
    log("  %s ...", "malloc/calloc/realloc/_msize/free, new/d");
    check_heap(f);
    f.report();
    Tally g{"crt_impl: atexit/_onexit/doexit/_initterm, _NMSG_WRITE"};
    log("  %s ...", "atexit/_onexit/doexit/_initterm, _NMSG_W");
    check_exit(g);
    g.report();
    Tally h{"crt_impl: files (fopen ... fclose, _sopen ... _close, remove, rename)"};
    log("  %s ...", "files (fopen ... fclose, _sopen ... _clo");
    check_files(h);
    h.report();
    Tally i{"crt_impl: 64-bit helpers, __ftol, _chkstk"};
    log("  %s ...", "64-bit helpers, __ftol, _chkstk");
    check_int64(i);
    i.report();
}

}  // namespace zax
