// x87 building blocks for code that must round exactly like the original: values on the FPU stack are
// long double (80 bit, rounded by the FPU's precision control like the original's), values the
// original keeps in memory are float. Arithmetic is only ever done on long double; a float comes into
// being only where the original stores one (fstp/fst dword), through fstp() below. Comparisons are
// named after the status word bits the original tests after fcom (unordered sets C0 and C3).
#pragma once
#include "reimpl.h"

namespace zax {

// fstp/fst dword: rounded to float through memory
inline float fstp(long double v)
{
    float r;
    __asm__ volatile("fsts %0" : "=m"(r) : "t"(v));
    return r;
}
// fld dword (exact; quiets a signalling NaN like the original's fld)
inline long double fld(float f)
{
    long double r;
    __asm__("flds %1" : "=t"(r) : "m"(f));
    return r;
}
// fild dword / fild qword
inline long double fild(int i) { return (long double)i; }
inline long double fild64(long long i) { return (long double)i; }
inline long double fsqrt(long double v)
{
    __asm__("fsqrt" : "+t"(v));
    return v;
}
// fpatan with st(1) = y, st(0) = x: the angle of (x, y)
inline long double fpatan(long double y, long double x)
{
    long double r;
    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}
// fcom a, b followed by test ah, 1 (C0: below or unordered), test ah, 0x41 (C0|C3: not above) and
// test ah, 0x40 (C3: equal or unordered)
inline bool c0(long double a, long double b) { return !(a >= b); }
inline bool c03(long double a, long double b) { return !(a > b); }
inline bool c3(long double a, long double b) { return !(a < b) && !(a > b); }
// A float constant of the exe, read where the original reads it
inline long double cst(DWORD address) { return fld(*at<const float *>(address)); }
// A float written by integer moves (mov), not through the FPU
inline float raw_float(DWORD bits)
{
    float f;
    memcpy(&f, &bits, 4);
    return f;
}
inline DWORD float_bits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// Constants of the exe the geometry code uses
constexpr DWORD C_ZERO = 0x5ebed0;          // 0.0
constexpr DWORD C_ONE = 0x5ec3f4;           // 1.0
constexpr DWORD C_HALF = 0x5eee2c;          // 0.5
constexpr DWORD C_QUARTER = 0x5f05b8;       // 0.25
constexpr DWORD C_TWO = 0x5eee9c;           // 2.0
constexpr DWORD C_FIVE = 0x5f08b8;          // 5.0
constexpr DWORD C_PI = 0x5eecc8;            // pi
constexpr DWORD C_MINUS_PI = 0x5eecc4;      // -pi
constexpr DWORD C_HALF_PI = 0x5ee000;       // pi / 2
constexpr DWORD C_MINUS_HALF_PI = 0x5f7de0; // -pi / 2
constexpr DWORD C_TWO_PI = 0x5edffc;        // c_TwoPi
constexpr DWORD C_EPS4 = 0x5f1c3c;          // 0.0001
constexpr DWORD C_MINUS_EPS4 = 0x5fd294;    // -0.0001
constexpr DWORD C_EPS5 = 0x5fd298;          // 0.00001
constexpr DWORD C_MINUS_EPS5 = 0x5fd29c;    // -0.00001
constexpr DWORD C_EPS2 = 0x5f1e48;          // 0.01
constexpr DWORD C_MINUS_EPS2 = 0x5f9fa8;    // -0.01
constexpr DWORD C_SNAP = 0x5f8334;          // 0.05
constexpr DWORD C_MINUS_SNAP = 0x5fd28c;    // -0.05
constexpr DWORD C_SWEEP_SCALE = 0x5fd290;   // 1.01
constexpr DWORD C_OVAL_RATIO = 0x5ee9b8;    // 0.712766 (YX ratio of the oval queries)

}  // namespace zax
