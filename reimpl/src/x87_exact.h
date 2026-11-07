// x87 arithmetic in the original's operand roles (ai_rest_a.cpp, input_rest_a.cpp). With two NaN operands the
// FPU's result depends on which one is the destination, so where the original computes with NaNs that can
// differ, the reimplementation uses the same instruction forms: st(0) with a float in memory (m_*), and the
// popping forms between st(1) (the destination) and st(0) (p_*). The register forms are written as opcode
// bytes (GNU as swaps fsubp/fdivp with their reverse forms in AT&T syntax).
#pragma once
#include "x87.h"

namespace zax {

// st(0) op float in memory (fadd/fsub/fsubr/fmul/fdiv/fdivr dword)
inline long double m_add(long double st0, float m)
{
    __asm__("fadds %1" : "+t"(st0) : "m"(m));
    return st0;
}
inline long double m_sub(long double st0, float m)
{
    __asm__("fsubs %1" : "+t"(st0) : "m"(m));
    return st0;
}
inline long double m_subr(long double st0, float m)
{
    __asm__("fsubrs %1" : "+t"(st0) : "m"(m));
    return st0;
}
inline long double m_mul(long double st0, float m)
{
    __asm__("fmuls %1" : "+t"(st0) : "m"(m));
    return st0;
}
inline long double m_div(long double st0, float m)
{
    __asm__("fdivs %1" : "+t"(st0) : "m"(m));
    return st0;
}
inline long double m_divr(long double st0, float m)
{
    __asm__("fdivrs %1" : "+t"(st0) : "m"(m));
    return st0;
}
// st(1) = st(1) op st(0), popped (faddp/fmulp/fdivp st(1), st(0)); the first argument is st(1)
inline long double p_add(long double st1, long double st0)
{
    long double r;
    __asm__(".byte 0xde, 0xc1" : "=t"(r) : "0"(st0), "u"(st1) : "st(1)");   // faddp st(1), st(0)
    return r;
}
inline long double p_mul(long double st1, long double st0)
{
    long double r;
    __asm__(".byte 0xde, 0xc9" : "=t"(r) : "0"(st0), "u"(st1) : "st(1)");   // fmulp st(1), st(0)
    return r;
}
inline long double p_div(long double st1, long double st0)
{
    long double r;
    __asm__(".byte 0xde, 0xf9" : "=t"(r) : "0"(st0), "u"(st1) : "st(1)");   // fdivp st(1), st(0): st(1) / st(0)
    return r;
}

}  // namespace zax
