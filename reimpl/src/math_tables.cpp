// The exe's generic math helpers of 0x508c50-0x509680 (Core): the table square root, the tables of sine,
// cosine, tangent, arc tangent and square root mantissas and their initialisation, the table cosine and
// sine of an angle, both at once, the table atan2, floor and ceil (as int and as a float on the FPU) and
// the highest set bit. Called by every subsystem (AI, World, Objects, Rendering, Sound).
//
// The tables [confirmed: 0x508cb0, filled by Game_Startup]: 4001 floats each from i = 0 to 4000 of
// i * 0.0015707964 (pi / 2000, so up to 2 pi): sine 0x6c2dc8, cosine 0x6c7bf0, tangent 0x6cbe74; the arc
// tangent of k / 1000 for k = 0 to 1000 at 0x6c6c4c (entry 0 written as 0, the others as atan2(10.0,
// 10 / k * 1000)); 2 x 128 dwords at 0x6cba74 with bits 16-22 of sqrt(1 + m / 128) and of sqrt(2 (1 +
// m / 128)) (the mantissa's first 7 bits of the root of an even and an odd exponent).
//
// Floating point follows the original instruction by instruction (x87.h): values on the FPU are long
// double, a float only where the original stores one. The table lookups take floor(angle * 636.6) as the
// index without a range check, like the original (an infinity or NaN gives 0x80000000, which wraps to
// the table's start).
#include "reimpl.h"
#include "x87.h"

namespace zax {

namespace {

constexpr DWORD SIN_TABLE = 0x6c2dc8, COS_TABLE = 0x6c7bf0, TAN_TABLE = 0x6cbe74, ATAN_TABLE = 0x6c6c4c, SQRT_TABLE = 0x6cba74;
constexpr DWORD C_TABLE_STEP = 0x5f9f9c;        // 0.0015707964 (pi / 2000)
constexpr DWORD C_TEN = 0x5eecd8;               // 10.0
constexpr DWORD C_TEN_DOUBLE = 0x5f0b98;        // 10.0 (double)
constexpr DWORD C_THOUSAND = 0x5eb2cc;          // 1000.0
constexpr DWORD C_MINUS_INV_TWO_PI = 0x5f9fa4;  // -1 / (2 pi)
constexpr DWORD C_INV_TWO_PI = 0x5edff8;        // 1 / (2 pi)
constexpr DWORD C_TABLE_SCALE = 0x5f9fa0;       // 636.61975 (2000 / pi)

inline long double x_fsin(long double v)
{
    __asm__("fsin" : "+t"(v));
    return v;
}
inline long double x_fcos(long double v)
{
    __asm__("fcos" : "+t"(v));
    return v;
}
// fptan, then fstp st(0) (the 1.0 it pushes)
inline long double x_fptan(long double v)
{
    __asm__("fptan\n\tfstp %%st(0)" : "+t"(v));
    return v;
}
inline long double fld64(DWORD address)
{
    long double r;
    __asm__("fldl %1" : "=t"(r) : "m"(*at<const double *>(address)));
    return r;
}
inline float table(DWORD base, int k) { return *at<const float *>(base + (DWORD)k * 4); }

// floor of a float through fistp (round to nearest) and fild, as the original: one less when the
// rounded value is above it (NaN and out of range: 0x80000000)
inline int floor_int(float t)
{
    int n = x87_fistp(fld(t));
    if (!c03(fild(n), fld(t)))
        n--;
    return n;
}

// The table index of an angle, as 0x508da0, 0x508ec0 and 0x508fe0 compute it: the angle into [0, 2 pi)
// (below 0: + (floor(-a / 2 pi) + 1) * 2 pi, + 0.01 when still below 0; from 2 pi: - floor(a / 2 pi) *
// 2 pi, - 0.01 when not below 2 pi, 0 when below 0), then floor(angle * 636.6)
int angle_index(float a)
{
    long double r;
    if (c0(fld(a), cst(glob_addr(C_ZERO)))) {
        const int n = floor_int(fstp(fld(a) * cst(glob_addr(C_MINUS_INV_TWO_PI))));
        r = (fild(n) + cst(glob_addr(C_ONE))) * cst(glob_addr(C_TWO_PI)) + fld(a);
        if (c0(r, cst(glob_addr(C_ZERO))))
            r = r + cst(glob_addr(C_EPS2));
    } else if (c0(fld(a), cst(glob_addr(C_TWO_PI)))) {
        r = fld(a);
    } else {
        const int n = floor_int(fstp(fld(a) * cst(glob_addr(C_INV_TWO_PI))));
        r = fld(a) - fild(n) * cst(glob_addr(C_TWO_PI));
        if (!c0(r, cst(glob_addr(C_TWO_PI))))
            r = r - cst(glob_addr(C_EPS2));
        if (c0(r, cst(glob_addr(C_ZERO))))
            r = cst(glob_addr(C_ZERO));
    }
    return floor_int(fstp(r * cst(glob_addr(C_TABLE_SCALE))));
}

// The arc tangent table's entry for a ratio (fstp to a float, fistp rounding to nearest)
inline long double atan_of(long double ratio) { return fld(table(glob_addr(ATAN_TABLE), x87_fistp(fld(fstp(ratio * cst(glob_addr(C_THOUSAND))))))); }

// 0x508c50 (stdcall float, st0): the square root through the table: 0 for 0 and NaN (the compare's C3);
// otherwise exponent (bits 23-31 minus 127; the sign bit counts into it) halved by an arithmetic shift
// of its low 16 bits, the mantissa's first 7 bits (and the exponent's lowest bit as bit 7) index the table
// of the root's mantissa
long double STDCALL Table_Sqrt(float v)
{
    if (c3(fld(v), cst(glob_addr(C_ZERO))))
        return cst(glob_addr(C_ZERO));
    const DWORD b = float_bits(v);
    DWORD m = b & 0x7fffff;
    const DWORD e = (b >> 23) - 0x7f;
    if (e & 1)
        m |= 0x800000;
    const short half = (short)(WORD)e >> 1;
    const DWORD r = (*at<const DWORD *>(glob_addr(SQRT_TABLE) + (m >> 16) * 4) << 16) | ((DWORD)((int)half + 0x7f) << 23);
    return fld(raw_float(r));
}

// 0x508cb0: fills the tables (see above). eax: undefined in use (the loop counter).
void __cdecl Tables_Init()
{
    for (int i = 0; i < 0xfa1; i++) {
        const long double x = fild64(i) * cst(glob_addr(C_TABLE_STEP));
        *at<float *>(glob_addr(SIN_TABLE) + (DWORD)i * 4) = fstp(x_fsin(x));
        *at<float *>(glob_addr(COS_TABLE) + (DWORD)i * 4) = fstp(x_fcos(x));
        *at<float *>(glob_addr(TAN_TABLE) + (DWORD)i * 4) = fstp(x_fptan(x));
    }
    *glob<DWORD *>(ATAN_TABLE) = 0;
    for (int k = 1; k <= 1000; k++) {
        const long double y = fld64(glob_addr(C_TEN_DOUBLE));
        *at<float *>(glob_addr(ATAN_TABLE) + (DWORD)k * 4) = fstp(fpatan(y, cst(glob_addr(C_TEN)) / fild64(k) * cst(glob_addr(C_THOUSAND))));
    }
    for (DWORD m = 0; m < 0x80; m++) {
        *at<DWORD *>(glob_addr(SQRT_TABLE) + m * 4) = (float_bits(fstp(fsqrt(fld(raw_float((m | 0x3f80) << 16))))) >> 16) & 0x7f;
        *at<DWORD *>(glob_addr(SQRT_TABLE) + 0x200 + m * 4) = (float_bits(fstp(fsqrt(fld(raw_float((m | 0x4000) << 16))))) >> 16) & 0x7f;
    }
}

// 0x508da0 (stdcall float, st0): the cosine table's entry of the angle (the table filled with fcos;
// ai_common.h's table_cos)
long double STDCALL Table_Cos(float a) { return fld(table(glob_addr(COS_TABLE), angle_index(a))); }

// 0x508ec0 (stdcall float, st0): the sine table's entry of the angle
long double STDCALL Table_Sin(float a) { return fld(table(glob_addr(SIN_TABLE), angle_index(a))); }

// 0x508fe0 (fastcall sin out, cos out; float): both entries of the angle (copied as dwords). eax: the
// sine's bits.
DWORD __fastcall Table_SinCos(DWORD *sin_out, DWORD *cos_out, float a)
{
    const int k = angle_index(a);
    const DWORD s = *at<const DWORD *>(glob_addr(SIN_TABLE) + (DWORD)k * 4);
    *sin_out = s;
    *cos_out = *at<const DWORD *>(glob_addr(COS_TABLE) + (DWORD)k * 4);
    return s;
}

// 0x509100 (stdcall y, x; st0): the angle of (x, y) in (-pi, pi] through the arc tangent table of the
// ratio of the smaller to the larger magnitude (rounded to a thousandth). y 0 or NaN: 0 for x above 0,
// otherwise pi; x 0 or NaN: pi / 2 for y above 0, otherwise -pi / 2. The original tests y's compare
// after loading x (fld leaves C3 as the compare set it).
long double STDCALL Table_Atan2(float y, float x)
{
    if (c3(fld(y), cst(glob_addr(C_ZERO))))
        return c03(fld(x), cst(glob_addr(C_ZERO))) ? cst(glob_addr(C_PI)) : cst(glob_addr(C_ZERO));
    if (c3(fld(x), cst(glob_addr(C_ZERO))))
        return c03(fld(y), cst(glob_addr(C_ZERO))) ? cst(glob_addr(C_MINUS_HALF_PI)) : cst(glob_addr(C_HALF_PI));
    if (!c03(fld(x), cst(glob_addr(C_ZERO)))) {
        if (!c03(fld(y), cst(glob_addr(C_ZERO)))) {
            if (!c03(fld(x), fld(y)))
                return atan_of(fld(y) / fld(x));
            return cst(glob_addr(C_HALF_PI)) - atan_of(fld(x) / fld(y));
        }
        const long double ny = -fld(y);
        if (!c03(fld(x), ny))
            return -atan_of(ny / fld(x));
        return atan_of(fld(x) / ny) - cst(glob_addr(C_HALF_PI));
    }
    const long double nx = -fld(x);
    if (!c03(fld(y), cst(glob_addr(C_ZERO)))) {
        if (!c03(nx, fld(y)))
            return cst(glob_addr(C_PI)) - atan_of(fld(y) / nx);
        return atan_of(nx / fld(y)) + cst(glob_addr(C_HALF_PI));
    }
    const long double ny = -fld(y);
    if (!c03(nx, ny))
        return atan_of(ny / nx) - cst(glob_addr(C_PI));
    return cst(glob_addr(C_MINUS_HALF_PI)) - atan_of(nx / ny);
}

// 0x509310 Float_Floor (stdcall float): floor as an int (fistp rounding to nearest, one less when that
// is above the value)
int STDCALL Float_Floor(float v) { return floor_int(v); }

// 0x509340 (stdcall float, st0): floor as a float on the FPU (the rounded value, 1 less when above)
long double STDCALL Float_FloorF(float v)
{
    const long double n = fild(x87_fistp(fld(v)));
    return !c03(n, fld(v)) ? n - cst(glob_addr(C_ONE)) : n;
}

// 0x509370 Float_Ceil (stdcall float): ceil as an int (one more when the rounded value is below the
// value or it is NaN)
int STDCALL Float_Ceil(float v)
{
    const int n = x87_fistp(fld(v));
    return c0(fild(n), fld(v)) ? n + 1 : n;
}

// 0x5093a0 (stdcall float, st0): ceil as a float on the FPU
long double STDCALL Float_CeilF(float v)
{
    const long double n = fild(x87_fistp(fld(v)));
    return c0(n, fld(v)) ? n + cst(glob_addr(C_ONE)) : n;
}

// 0x509680 (value in ecx): the index of the highest set bit, -1 for 0 (the original leaves the bit count
// in edx; its only caller does not read it)
int __fastcall HighestBit(DWORD v)
{
    int n = 0;
    while (v) {
        v >>= 1;
        n++;
    }
    return n - 1;
}

}  // namespace

ZAX_HOOKS(math_tables)
{
    const Hook list[] = {
        {0x508c50, ptr(&Table_Sqrt), "Table_Sqrt", 10, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x1d, 0xd0, 0xbe}},
        {0x508cb0, ptr(&Tables_Init), "Tables_Init", 5, {0x83, 0xec, 0x08, 0x33, 0xc0, 0x33, 0xc9, 0x89}},
        {0x508da0, ptr(&Table_Cos), "Table_Cos", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0xd9, 0x45}},
        {0x508ec0, ptr(&Table_Sin), "Table_Sin", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0xd9, 0x45}},
        {0x508fe0, ptr(&Table_SinCos), "Table_SinCos", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0xd9, 0x45}},
        {0x509100, ptr(&Table_Atan2), "Table_Atan2", 6, {0x55, 0x8b, 0xec, 0xd9, 0x45, 0x08, 0xd8, 0x1d}},
        {0x509310, ptr(&Float_Floor), "Float_Floor", 7, {0x55, 0x8b, 0xec, 0x51, 0xd9, 0x45, 0x08, 0xdb}},
        {0x509340, ptr(&Float_FloorF), "Float_FloorF", 7, {0x55, 0x8b, 0xec, 0x51, 0xd9, 0x45, 0x08, 0xdb}},
        {0x509370, ptr(&Float_Ceil), "Float_Ceil", 7, {0x55, 0x8b, 0xec, 0x51, 0xd9, 0x45, 0x08, 0xdb}},
        {0x5093a0, ptr(&Float_CeilF), "Float_CeilF", 7, {0x55, 0x8b, 0xec, 0x51, 0xd9, 0x45, 0x08, 0xdb}},
        {0x509680, ptr(&HighestBit), "HighestBit", 6, {0x8b, 0xc1, 0x33, 0xd2, 0x85, 0xc0, 0x74, 0x07}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
