// Checks of math_tables.cpp against the original, in the game thread: every function called through its
// trampoline (the original) and through the hooked exe address (the reimplementation) with the same
// arguments. Results on the FPU are compared as all 80 bits, ints and the written outputs by value.
//  - Angles for the table cosine, sine and both: small and large magnitudes, the bounds 0 and 2 pi and
//    their neighbours, multiples of the table step, signed zeros, NaN, infinities, denormals.
//  - atan2: pairs from the same values plus equal and opposite magnitudes and zeros of both signs.
//  - The square root, floor and ceil (as int and on the FPU): random bit patterns of every class.
//  - The highest set bit: random dwords, 0 and the powers of two (and their neighbours).
//  - The table initialisation 0x508cb0: the tables (0x6c2dc8-0x6cfcf8) saved, filled by the original and
//    by the reimplementation, compared, then put back as they were.
#include "check_util.h"
#include "x87.h"

namespace zax {

namespace {

using F1 = long double(STDCALL *)(float);
using F2 = long double(STDCALL *)(float, float);
using I1 = int(STDCALL *)(float);
using SinCos = DWORD(__fastcall *)(DWORD *, DWORD *, float);
using Bit = int(__fastcall *)(DWORD);

DWORD bits_of(float f) { return float_bits(f); }
float from_bits(DWORD b) { return raw_float(b); }

// An angle: mostly small (a few turns), sometimes large, special values one time in eight
float angle(unsigned k)
{
    const DWORD r = random();
    const DWORD special[] = {0, 0x80000000u, 0x7fc00000u, 0xffc00000u, 0x7f800000u, 0xff800000u, 0x00000001u, 0x80000001u,
                             0x40c90fdbu, 0x40c90fdau, 0x40c90fdcu, 0xc0c90fdbu, 0xc0c90fdau, 0xc0c90fdcu, 0x3acde32eu, 0x7f7fffffu,
                             0x3f800000u, 0xbf800000u, 0x40490fdbu, 0xc0490fdbu, 0x3fc90fdbu, 0xbfc90fdbu, 0x358637bdu, 0x47c35000u};
    switch (k % 8) {
    case 0: return from_bits(special[(r >> 3) % (sizeof special / 4)]);
    case 1: return (float)(int)(r % 2001 - 1000) * 6.2831855f + (float)(int)((r >> 11) % 3 - 1) * 1e-6f;
    case 2: return (float)(int)(r % 4001) * 0.0015707964f * (r & 0x100000 ? -1.0f : 1.0f);
    case 3: return (float)(int)(r % 200000 - 100000) * 0.5f;
    default: return (float)(int)(r % 0x1000000 - 0x800000) * (1.0f / 0x40000);   // -32 to 32
    }
}

// Any float: random bits (all classes), one time in four a moderate value
float any_float(unsigned k)
{
    const DWORD r = random();
    if (k % 4 == 0)
        return (float)(int)(r % 0x2000000 - 0x1000000) * (1.0f / 1024);
    if (k % 4 == 1)
        return (float)(int)(r % 20001 - 10000) * 0.25f;
    return from_bits(r);
}

struct Value {
    BYTE b[10];
};
Value value_of(long double v)
{
    Value r;
    memcpy(r.b, &v, 10);
    return r;
}
bool same(const Value &a, const Value &b) { return memcmp(a.b, b.b, 10) == 0; }
DWORD hi_of(const Value &v) { DWORD d; memcpy(&d, v.b + 6, 4); return d; }

void check_f1(Tally &t, DWORD address, const char *name, float x)
{
    const Value o = value_of(original<F1>(address)(x)), r = value_of(at<F1>(address)(x));
    char d[160];
    wsprintfA(d, "[0x%x %s(%x): original %x.., reimpl %x..]", address, name, bits_of(x), hi_of(o), hi_of(r));
    t.check(same(o, r), name, d);
}
void check_f2(Tally &t, DWORD address, const char *name, float x, float y)
{
    const Value o = value_of(original<F2>(address)(x, y)), r = value_of(at<F2>(address)(x, y));
    char d[160];
    wsprintfA(d, "[0x%x %s(%x, %x): original %x.., reimpl %x..]", address, name, bits_of(x), bits_of(y), hi_of(o), hi_of(r));
    t.check(same(o, r), name, d);
}
void check_i1(Tally &t, DWORD address, const char *name, float x)
{
    const int o = original<I1>(address)(x), r = at<I1>(address)(x);
    char d[160];
    wsprintfA(d, "[0x%x %s(%x): original %x, reimpl %x]", address, name, bits_of(x), o, r);
    t.check(o == r, name, d);
}
void check_sincos(Tally &t, float x)
{
    DWORD so = 0x11111111, co = 0x22222222, sr = 0x11111111, cr = 0x22222222;
    const DWORD eo = original<SinCos>(0x508fe0)(&so, &co, x), er = at<SinCos>(0x508fe0)(&sr, &cr, x);
    char d[160];
    wsprintfA(d, "[0x508fe0 (%x): original %x %x eax %x, reimpl %x %x eax %x]", bits_of(x), so, co, eo, sr, cr, er);
    t.check(so == sr && co == cr && eo == er, "Table_SinCos", d);
}

// The tables of 0x508cb0: sine 0x6c2dc8 (4001), arc tangent 0x6c6c4c (1001), cosine 0x6c7bf0 (4001), the
// square root mantissas 0x6cba74 (256), tangent 0x6cbe74 (4001), one block up to 0x6cfcf8
constexpr DWORD TABLES = 0x6c2dc8, TABLES_END = 0x6cfcf8, TABLES_SIZE = TABLES_END - TABLES;
BYTE *g_tables_saved;   // allocated once, kept
void check_tables_init(Tally &t)
{
    BYTE *&saved = g_tables_saved;
    if (!g_tables_saved)
        g_tables_saved = static_cast<BYTE *>(VirtualAlloc(nullptr, TABLES_SIZE * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!saved) {
        t.check(false, "Tables_Init", "[no memory]");
        return;
    }
    BYTE *o = saved + TABLES_SIZE, *r = saved + 2 * TABLES_SIZE;
    memcpy(saved, at<void *>(TABLES), TABLES_SIZE);
    memset(at<void *>(TABLES), 0xa5, TABLES_SIZE);
    original<void(__cdecl *)()>(0x508cb0)();
    memcpy(o, at<void *>(TABLES), TABLES_SIZE);
    memset(at<void *>(TABLES), 0x5a, TABLES_SIZE);
    at<void(__cdecl *)()>(0x508cb0)();
    memcpy(r, at<void *>(TABLES), TABLES_SIZE);
    memcpy(at<void *>(TABLES), saved, TABLES_SIZE);
    DWORD first = 0xffffffff, same_as_game = 0;
    for (DWORD i = 0; i < TABLES_SIZE; i += 4) {
        if (*reinterpret_cast<DWORD *>(o + i) != *reinterpret_cast<DWORD *>(r + i) && first == 0xffffffff)
            first = i;
        if (*reinterpret_cast<DWORD *>(o + i) == *reinterpret_cast<DWORD *>(saved + i))
            same_as_game++;
    }
    char d[160];
    wsprintfA(d, "[0x508cb0: first difference at %x: %x / %x]", first == 0xffffffff ? 0 : TABLES + first,
              first == 0xffffffff ? 0 : *reinterpret_cast<DWORD *>(o + first), first == 0xffffffff ? 0 : *reinterpret_cast<DWORD *>(r + first));
    t.check(first == 0xffffffff, "Tables_Init", d);
    // the original's tables now against the game's own (made at startup, maybe with another FPU control word)
    log("  tables: %u of %u dwords as the game made them at startup", same_as_game, TABLES_SIZE / 4);
}

}  // namespace

ZAX_CHECKS(math_tables)
{
    (void)cls;
    (void)n;
    g_seed = 0x5a7ab1e5;
    Tally trig{"math tables: table cosine, sine, both, atan2 (0x508da0-0x509100)"};
    for (unsigned k = 0; k < (unsigned)scaled(40000); k++) {
        const float a = angle(k);
        check_f1(trig, 0x508da0, "Table_Cos", a);
        check_f1(trig, 0x508ec0, "Table_Sin", a);
        check_sincos(trig, a);
    }
    for (unsigned k = 0; k < (unsigned)scaled(40000); k++) {
        float y = angle(k), x = angle(k >> 3);
        switch (random() % 8) {
        case 0: x = y; break;
        case 1: x = -y; break;
        case 2: x = from_bits(random() % 2 ? 0 : 0x80000000u); break;
        case 3: y = from_bits(random() % 2 ? 0 : 0x80000000u); break;
        default: break;
        }
        check_f2(trig, 0x509100, "Table_Atan2", y, x);
    }
    trig.report();
    Tally misc{"math tables: table sqrt, floor, ceil, highest bit (0x508c50, 0x509310-0x5093a0, 0x509680)"};
    for (unsigned k = 0; k < (unsigned)scaled(40000); k++) {
        const float v = any_float(k);
        check_f1(misc, 0x508c50, "Table_Sqrt", v);
        check_i1(misc, 0x509310, "Float_Floor", v);
        check_f1(misc, 0x509340, "Float_FloorF", v);
        check_i1(misc, 0x509370, "Float_Ceil", v);
        check_f1(misc, 0x5093a0, "Float_CeilF", v);
    }
    for (unsigned k = 0; k < (unsigned)scaled(20000); k++) {
        const DWORD v = k < 96 ? (1u << (k % 32)) + (DWORD)(k / 32) - 1 : random() >> (random() % 32);
        const int o = original<Bit>(0x509680)(v), r = at<Bit>(0x509680)(v);
        char d[96];
        wsprintfA(d, "[0x509680 (%x): original %d, reimpl %d]", v, o, r);
        misc.check(o == r, "HighestBit", d);
    }
    misc.report();
    Tally init{"math tables: the table initialisation 0x508cb0"};
    for (int k = 0; k < 2; k++)
        check_tables_init(init);
    init.report();
}

}  // namespace zax
