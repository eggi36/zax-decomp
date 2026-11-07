#ifndef ZAX_LIBC_MATH_H
#define ZAX_LIBC_MATH_H
/* See zax_libc.h: floor, ldexp, pow and acos are the exe's C runtime functions; sin, cos, sqrt,
 * fabs, log, exp and atan are the x87 sequences MSVC 6 compiles inline for them (/Oi), as in the
 * exe's library code. */
#include "zax_libc.h"

static __inline double __cdecl floor(double x) { return ZAX_CRT(double(__cdecl *)(double), 0x5d139b)(x); }
static __inline double __cdecl ldexp(double x, int e) { return ZAX_CRT(double(__cdecl *)(double, int), 0x5ddb6e)(x, e); }
/* pow and acos as the exe's libraries call them: through the runtime's register forms _CIpow 0x5d23e0 (x in
 * st(1), y in st(0); both popped, the result in st(0)) and _CIacos 0x5e97e0 (x in st(0), the result there).
 * The exe has no public pow: 0x5d2402 is _CIpow's inner part, which also expects y's high dword in eax. */
static __inline double __cdecl pow(double x, double y)
{
    long double r, a = x, b = y;
    __asm__ volatile("call *%3" : "=t"(r) : "0"(b), "u"(a), "r"(ZAX_CRT_ADDR(0x5d23e0)) : "st(1)", "eax", "ecx", "edx", "memory");
    return (double)r;
}
static __inline double __cdecl acos(double x)
{
    long double v = x;
    __asm__ volatile("call *%1" : "+t"(v) : "r"(ZAX_CRT_ADDR(0x5e97e0)) : "eax", "ecx", "edx", "memory");
    return (double)v;
}

/* The x87 intrinsics return the register's value as it is (long double), as MSVC 6 leaves them in st(0):
 * fsin, fcos and the others round to 64 bits whatever the precision control says, and a store to a float
 * then rounds once. Returning double would round to 53 bits first (twice in all). */
static __inline long double zax_fsin(long double v) { __asm__("fsin" : "+t"(v)); return v; }
static __inline long double zax_fcos(long double v) { __asm__("fcos" : "+t"(v)); return v; }
static __inline long double zax_fsqrt(long double v) { __asm__("fsqrt" : "+t"(v)); return v; }
static __inline long double zax_fabs(long double v) { __asm__("fabs" : "+t"(v)); return v; }
/* fldln2; fxch; fyl2x (0x5e30eb) */
static __inline long double zax_log(long double v)
{
    __asm__("fldln2\n\tfxch %%st(1)\n\tfyl2x" : "+t"(v));
    return v;
}
/* fld1; fpatan (0x5e5d35) */
static __inline long double zax_atan(long double v)
{
    __asm__("fld1\n\tfpatan" : "+t"(v));
    return v;
}
/* fldl2e; fmulp; fld st; frndint; fxch; fsub st(1); f2xm1; fld1; faddp; fscale; fstp st(1) (0x5e919b) */
static __inline long double zax_exp(long double v)
{
    __asm__("fldl2e\n\tfmulp\n\tfld %%st(0)\n\tfrndint\n\tfxch %%st(1)\n\tfsub %%st(1), %%st\n\tf2xm1\n\t"
            "fld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "+t"(v));
    return v;
}
#define sin(x) zax_fsin(x)
#define cos(x) zax_fcos(x)
#define sqrt(x) zax_fsqrt(x)
#define fabs(x) zax_fabs(x)
#define log(x) zax_log(x)
#define atan(x) zax_atan(x)
#define exp(x) zax_exp(x)
#endif
