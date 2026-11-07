// Checks of the portable C runtime versions (crt_portable.h) against the exe's MSVC 6 runtime, in the game
// thread. The exe's functions are called through their trampolines (the hooked reimplementations of
// crt_string.cpp and crt_math.cpp have their own checks in checks_crt_impl.cpp).
//  - qsort 0x5d097c: arrays of 0-300 records {key, id} of 4, 8 and 12 bytes with few distinct keys (many
//    equal ones), sorted by key only: the ids' order and the sequence of compared id pairs.
//  - rand/srand 0x5d2198/0x5d218b: 64 numbers after random seeds and seed 1 (the thread's holdrand saved
//    and restored through _getptd 0x5d4f28).
//  - __ftol 0x5d0488 on doubles and floats: random bit patterns, halves, values near 2^31/2^32/2^63, NaN,
//    infinities, denormals, zeros: all 64 bits of the result.
//  - atoi/atol 0x5d03d3/0x5d0348 and strtol 0x5d10b0 (bases 0, 2, 8, 10, 16, 36, 1, 37): random texts
//    with white space, signs, "0x", overflowing digit runs and high bytes; the value and the end offset.
//  - _stricmp 0x5d3640, _strnicmp 0x5dd9c0 (lengths 0-12), toupper 0x5d0c4e, tolower 0x5d272a, isupper
//    0x5d0f29, islower 0x5d0f51, isspace 0x5d0f79 on every byte and EOF: the exact return values.
#include "check_util.h"
#include "crt_portable.h"

namespace zax {
namespace {

constexpr DWORD QSORT = 0x5d097c, RAND = 0x5d2198, SRAND = 0x5d218b, GETPTD = 0x5d4f28, FTOL = 0x5d0488;
constexpr DWORD ATOI = 0x5d03d3, ATOL = 0x5d0348, STRTOL = 0x5d10b0, STRICMP = 0x5d3640, STRNICMP = 0x5dd9c0;
constexpr DWORD TOUPPER = 0x5d0c4e, TOLOWER = 0x5d272a, ISUPPER = 0x5d0f29, ISLOWER = 0x5d0f51, ISSPACE = 0x5d0f79;

// ---- qsort ------------------------------------------------------------------------------------------------

unsigned g_cmp_hash, g_id_word;
int g_cmp_count;
int __cdecl compare_key(const void *a, const void *b)
{
    const DWORD *x = (const DWORD *)a, *y = (const DWORD *)b;
    const DWORD e[2] = {x[g_id_word], y[g_id_word]};
    g_cmp_hash = fnv(g_cmp_hash, e, 8);
    g_cmp_count++;
    return (int)x[0] - (int)y[0];
}

void check_qsort(Tally &t)
{
    DWORD a[2][300 * 3];
    g_seed = 0x5d097c;
    for (int k = 0; k < scaled(3000); k++) {
        const unsigned width = 4 * (1 + random() % 3), words = width / 4;
        const unsigned num = k < 40 ? (unsigned)k : random() % 301;
        const unsigned keys = 1 + random() % (random() % 2 ? 4 : 40);
        for (unsigned i = 0; i < num; i++) {
            a[0][i * words] = random() % keys;
            if (words > 1)
                a[0][i * words + 1] = i;
            if (words > 2)
                a[0][i * words + 2] = random();
        }
        memcpy(a[1], a[0], num * width);
        unsigned h[2];
        int c[2];
        g_id_word = words > 1 ? 1 : 0;   // 4-byte records: the keys stand for the ids
        for (int side = 0; side < 2; side++) {
            g_cmp_hash = 2166136261u;
            g_cmp_count = 0;
            if (side)
                crt::qsort(a[1], num, width, compare_key);
            else
                original<void(__cdecl *)(void *, unsigned, unsigned, crt::Compare)>(QSORT)(a[0], num, width, compare_key);
            h[side] = g_cmp_hash;
            c[side] = g_cmp_count;
        }
        char d[96];
        wsprintfA(d, "[qsort n %u width %u keys %u: compares %d/%d]", num, width, keys, c[0], c[1]);
        t.check(!memcmp(a[0], a[1], num * width) && h[0] == h[1] && c[0] == c[1], "", d);
    }
}

// ---- rand -------------------------------------------------------------------------------------------------

void check_rand(Tally &t)
{
    DWORD *ptd = at<DWORD *(__cdecl *)()>(GETPTD)();
    const DWORD saved = ptd[5];
    g_seed = 0x5d2198;
    for (int k = 0; k < scaled(200); k++) {
        const unsigned s = k == 0 ? 1u : k == 1 ? 0u : k == 2 ? 0xffffffffu : random();
        original<void(__cdecl *)(unsigned)>(SRAND)(s);
        crt::Rand r;
        r.seed(s);
        bool same = true;
        for (int i = 0; i < 64; i++)
            same &= original<int(__cdecl *)()>(RAND)() == r.next();
        same &= ptd[5] == r.holdrand;
        char d[48];
        wsprintfA(d, "[rand seed %x]", s);
        t.check(same, "", d);
    }
    ptd[5] = saved;
}

// ---- __ftol -----------------------------------------------------------------------------------------------

long long exe_ftol(double x)
{
    long long r;
    __asm__ volatile("fldl %1\n\tcall *%2" : "=A"(r) : "m"(x), "r"(original_address(FTOL)) : "ecx", "memory", "st");
    return r;
}

double special(unsigned r)
{
    static const unsigned long long bits[] = {
        0x0000000000000000ull, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
        0x7ff8000000000000ull, 0xfff8000000000001ull, 0x7ff0000000000001ull, 0x0000000000000001ull,
        0x800fffffffffffffull, 0x3fe0000000000000ull, 0xbfe0000000000000ull, 0x3fefffffffffffffull,
        0x41dfffffffc00000ull, 0x41e0000000000000ull, 0xc1e0000000000000ull, 0xc1e0000000200000ull,
        0x41efffffffe00000ull, 0x41f0000000000000ull, 0x43dfffffffffffffull, 0x43e0000000000000ull,
        0xc3e0000000000000ull, 0xc3e0000000000001ull, 0x4330000000000000ull, 0x4330000000000001ull,
    };
    double x;
    memcpy(&x, &bits[r % (sizeof bits / sizeof bits[0])], 8);
    return x;
}

void check_ftol(Tally &t)
{
    g_seed = 0x5d0488;
    for (int k = 0; k < scaled(20000); k++) {
        double x;
        const unsigned r = random();
        if (k < 48)
            x = special((unsigned)k);
        else if (r % 4 == 0) {
            const unsigned long long u = (unsigned long long)random() << 32 | random();
            memcpy(&x, &u, 8);
        } else if (r % 4 == 1) {
            float f;
            const unsigned u = random();
            memcpy(&f, &u, 4);
            x = f;
        } else if (r % 4 == 2)
            x = (double)(int)random() / (double)(1 + random() % 1000) + (random() % 2 ? 0.5 : 0.0);
        else
            x = (double)(int)random() * (double)(1u << (random() % 32));
        const long long a = exe_ftol(x), b = crt::ftol(x);
        unsigned long long u;
        memcpy(&u, &x, 8);
        char d[96];
        wsprintfA(d, "[__ftol %08x%08x: %08x%08x / %08x%08x]", (DWORD)(u >> 32), (DWORD)u, (DWORD)((unsigned long long)a >> 32),
                  (DWORD)a, (DWORD)((unsigned long long)b >> 32), (DWORD)b);
        t.check(a == b, "", d);
    }
}

// ---- numbers from text ------------------------------------------------------------------------------------

void random_number_text(char *s, int max)
{
    static const char pieces[][8] = {" ", "\t", "\n\v", "+", "-", "0", "0x", "0X", "1", "9", "7f", "Zz", "ff",
                                     "2147", "48364", "8", "9999", "4294", "967296", "\x80", "\xa0", "\xff", "e", "."};
    int n = 0;
    const int parts = (int)(random() % 9);
    for (int i = 0; i < parts; i++) {
        const char *p = pieces[random() % (sizeof pieces / sizeof pieces[0])];
        while (*p && n < max - 1)
            s[n++] = *p++;
    }
    if (random() % 4 == 0)
        while (n < max - 1 && random() % 3)
            s[n++] = (char)('0' + random() % 10);
    s[n] = 0;
}

void check_numbers(Tally &t)
{
    static const int bases[] = {0, 10, 16, 8, 2, 36, 1, 37, -1};
    char s[64];
    g_seed = 0x5d10b0;
    for (int k = 0; k < scaled(20000); k++) {
        random_number_text(s, sizeof s);
        const int base = bases[random() % (sizeof bases / sizeof bases[0])];
        char *e0 = nullptr;
        const char *e1 = nullptr;
        const long v0 = original<long(__cdecl *)(const char *, char **, int)>(STRTOL)(s, &e0, base);
        const long v1 = crt::strtol(s, &e1, base);
        const int i0 = original<int(__cdecl *)(const char *)>(ATOI)(s), l0 = (int)original<long(__cdecl *)(const char *)>(ATOL)(s);
        const int i1 = (int)crt::atol(s);
        char d[128];
        wsprintfA(d, "[\"%.40s\" base %d: strtol %d/%d end %d/%d atoi %d %d/%d]", s, base, v0, v1, (int)(e0 - s), (int)(e1 - s),
                  i0, l0, i1);
        t.check(v0 == v1 && e0 == e1 && i0 == i1 && l0 == i1, "", d);
    }
}

// ---- case and character classes ---------------------------------------------------------------------------

void check_case(Tally &t)
{
    for (int c = -1; c < 256; c++) {
        const int r0[5] = {original<int(__cdecl *)(int)>(TOUPPER)(c), original<int(__cdecl *)(int)>(TOLOWER)(c),
                           original<int(__cdecl *)(int)>(ISUPPER)(c), original<int(__cdecl *)(int)>(ISLOWER)(c),
                           original<int(__cdecl *)(int)>(ISSPACE)(c)};
        // the is* functions return the table bit (1, 2, 8), compared as a truth value
        const bool same = r0[0] == (c < 0 ? c : crt::to_upper(c)) && r0[1] == (c < 0 ? c : crt::to_lower(c)) &&
                          (r0[2] != 0) == crt::is_upper(c) && (r0[3] != 0) == crt::is_lower(c) && (r0[4] != 0) == crt::is_space(c);
        char d[80];
        wsprintfA(d, "[char %d: %d %d %d %d %d]", c, r0[0], r0[1], r0[2], r0[3], r0[4]);
        t.check(same, "", d);
    }
    char a[16], b[16];
    g_seed = 0x5d3640;
    for (int k = 0; k < scaled(20000); k++) {
        const int n = (int)(random() % 8);
        for (int i = 0; i < n; i++) {
            const unsigned r = random();
            a[i] = r % 5 == 0 ? (char)(r >> 8) : (char)("aAzZ@[`{_0"[r % 10]);
            b[i] = random() % 3 ? (char)crt::to_upper((BYTE)a[i]) : random() % 2 ? a[i] : (char)(random() >> 8);
        }
        a[n] = 0;
        const int m = random() % 3 ? n : (int)(random() % 8);
        for (int i = n; i < m; i++)
            b[i] = 'a';
        b[m < n ? n : m] = 0;
        const unsigned count = random() % 13;
        const int s0 = original<int(__cdecl *)(const char *, const char *)>(STRICMP)(a, b), s1 = crt::stricmp(a, b);
        const int n0 = original<int(__cdecl *)(const char *, const char *, unsigned)>(STRNICMP)(a, b, count), n1 = crt::strnicmp(a, b, count);
        char d[96];
        wsprintfA(d, "[\"%s\" \"%s\" %u: _stricmp %d/%d _strnicmp %d/%d]", a, b, count, s0, s1, n0, n1);
        t.check(s0 == s1 && n0 == n1, "", d);
    }
}

}  // namespace

ZAX_CHECKS(crt)
{
    (void)cls;
    (void)n;
    Tally q{"crt: qsort order and compares"};
    check_qsort(q);
    q.report();
    Tally r{"crt: rand/srand"};
    check_rand(r);
    r.report();
    Tally f{"crt: __ftol"};
    check_ftol(f);
    f.report();
    Tally x{"crt: atoi/atol/strtol"};
    check_numbers(x);
    x.report();
    Tally c{"crt: _stricmp/_strnicmp/toupper/tolower/isupper/islower/isspace"};
    check_case(c);
    c.report();
}

}  // namespace zax
