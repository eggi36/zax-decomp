// The MMX 16-bit spans of the software renderer (phase 4, batch R4 of the blitter map, docs/DECOMPILATION.md 9):
// the span tables T18-T23, the MMX twins of T12-T17 (spans16.h). The RLE drawers and edge rows of the MMX
// blitters (registered when Cpu_HasMmx answered yes, i.e. on every CPU the game meets today) call them through
// the tables. spans_mmx.cpp has T18/T19, spans_mmx_opacity.cpp T20-T23.
//
// Tables and calls as T12-T17 [confirmed: .data, the generic entries from CBlendTransparentMask_Ctor 0x4479c0]:
// T k at 0x60cd90 + k * 0x100; entry 0 a stub (Stub_Ret8 0x4f0a80 for T18, T19, T22, T23; 0x401110 `ret 4`
// for T20, T21), entries 1-16 one instance per count (they ignore the count argument), 17-63 the generic loop.
// fastcall (ecx dst, edx src or value; stack: [alpha,] n), ret 8 / ret 4:
//   T18 Alpha_Copy_MMX        (dst, src; alpha, n)    alpha a pointer to one byte per pixel
//   T19 Alpha_Fill_MMX        (dst, value; alpha, n)  the low byte of alpha
//   T20 Opacity_Copy_MMX      (dst, src; n)           g_OpacityTable + 0x3fc
//   T21 Opacity_Fill_MMX      (dst, value; n)         the byte at g_OpacityTable + 0x3fc
//   T22 AlphaOpacity_Copy_MMX (dst, src; alpha, n)    alpha bytes and the opacity
//   T23 AlphaOpacity_Fill_MMX (dst, value; alpha, n)  the byte at g_OpacityTable + 4 * (alpha & 0xff)
//
// Structure [confirmed: code of all 102 functions]: the C++ source had one template per table with an
// `__asm` block. A span of n pixels (the entry's own n, or the count for the generic loop) blends n >> 2
// groups of four pixels in the MMX block (a do-while over the groups, so only entered for n >= 4), then the
// n & 3 last pixels in C with the scalar blend of spans16.h (blend565) and the scalar weight of the twin
// table T12-T17. Entries 1-3 and the generic loops with n <= 3 run no MMX instruction at all. The generic
// loops test the count (n = 0 does nothing), unlike the do-while generic loops of T12, T14, T16.
// There are four MMX blocks (the same instructions in every entry of a table; T19, T21 and T23 share one):
//   literal T18: the four alpha bytes per group;        fill T19/T21/T23: the weight byte, set up in C;
//   literal T20: the LOW BYTE of the dword at g_OpacityTable + 0x3fc (the scalar T14 and the tail of T20
//                multiply the whole dword);
//   literal T22: per pixel (alpha * o) >> 8 with o the low byte of the dword at g_OpacityTable + 0x3fc
//                (pmullw, psrlw 8), NOT the opacity table's entry g_OpacityTable[alpha] the scalar T16 and
//                the tail of T22 use. It also loads the group's alpha dword into edx (dead).
//
// The MMX blend (per 16-bit lane; w the weight, always 0..255 here) [confirmed: code; the checks compare
// the formula below with the original]: a6 = w >> 2, a5 = a6 >> 1, i6 = 63 - a6, i5 = 31 - a5 (psubsb from
// 0x003f / 0x001f), then
//   green: G = 16 * gs * a6 + 16 * gd * i6 + 0x200;  g = ((G + (G >> 6)) >> 5) & 0x7e0
//   blue:  B = bs * a5 + bd * i5 + 0x10;            b = (B + (B >> 5)) >> 5
//   red:   R = rs * a5 + rd * i5 + 0x10;            r = ((R + (R >> 5)) >> 5) << 11
// (gs, bs, rs the 6/5/5-bit fields of the source, gd, bd, rd of the destination), the pixel b | g | r. With
// w <= 255 no lane overflows (G <= 64016), so the 16-bit arithmetic equals plain integer arithmetic. This is
// a weighted mean with the divisor 31 (63 for green) and rounding, where the scalar blend adds 31/32 (63/64)
// of the difference rounded down: w = 255 gives the full source here, not there. MMX and scalar spans do NOT
// give the same pixels (docs/DECOMPILATION.md 9, batch R4: up to 2 steps per channel, most pixels).
//
// MMX state [confirmed: code]: the fill blocks end with `emms`, the literal blocks do not; so after a literal
// span of four or more pixels the MMX state is set (all x87 tags valid, top 0) until the blitter's own
// `emms` (BlitAmplifyLight_16RleTo16_MMX 0x4355f0 has none). The reimplementation runs the same instructions
// in the same order (generated from the original blocks), so it leaves the same MMX registers and tags.
//
// [Reimpl] SpanMutation= (checks only; 1 and 2 are those of spans16.h): 3 lets the groups go through the
// portable form with green one step off for a6 = 32, 4 adds an `emms` after T18's groups (the MMX state the
// original leaves set); the checks must report both.
//
// For the port (no MMX, phase 6): blend565_mmx below is the portable form, the groups_*_c functions the
// portable groups; spans built with C = true use them (also on x86; the checks compare them with the original).
#pragma once
#include "spans16.h"

namespace zax::render {

// The masks of the MMX blocks (the exe keeps one set per block at 0x60cc78-0x60cd8f)
alignas(8) inline constexpr WORD MMX_K3F[4] = {0x3f, 0x3f, 0x3f, 0x3f}, MMX_K1F[4] = {0x1f, 0x1f, 0x1f, 0x1f},
                                 MMX_K7E0[4] = {0x7e0, 0x7e0, 0x7e0, 0x7e0}, MMX_K3F0[4] = {0x3f0, 0x3f0, 0x3f0, 0x3f0},
                                 MMX_KF800[4] = {0xf800, 0xf800, 0xf800, 0xf800}, MMX_K10[4] = {0x10, 0x10, 0x10, 0x10},
                                 MMX_K200[4] = {0x200, 0x200, 0x200, 0x200};

// The MMX blend of one pixel (portable; w 0..255)
inline WORD blend565_mmx(DWORD d, DWORD s, DWORD w)
{
    const DWORD a6 = w >> 2, a5 = a6 >> 1, i6 = 63 - a6, i5 = 31 - a5;
    const DWORD G = ((s >> 1) & 0x3f0) * a6 + ((d >> 1) & 0x3f0) * i6 + 0x200;
    const DWORD B = (s & 31) * a5 + (d & 31) * i5 + 0x10;
    const DWORD R = ((s & 0xffff) >> 11) * a5 + ((d & 0xffff) >> 11) * i5 + 0x10;
    DWORD g = ((G + (G >> 6)) >> 5) & 0x7e0;
    if (g_span_mutation == 3 && a6 == 32)
        g ^= 0x20;
    return (WORD)(g | ((B + (B >> 5)) >> 5) | (((R + (R >> 5)) >> 5) << 11));
}

// ---- The portable groups (four pixels each, groups >= 1) ------------------------------------------------------

inline void groups_alpha_copy_c(WORD *dst, const WORD *src, const BYTE *alpha, DWORD groups)
{
    for (DWORD i = 0; i < groups * 4; i++)
        dst[i] = blend565_mmx(dst[i], src[i], alpha[i]);
}
inline void groups_fill_c(WORD *dst, DWORD value, DWORD w, DWORD groups)
{
    for (DWORD i = 0; i < groups * 4; i++)
        dst[i] = blend565_mmx(dst[i], value & 0xffff, w & 0xff);
}
inline void groups_opacity_copy_c(WORD *dst, const WORD *src, DWORD opacity, DWORD groups)
{
    for (DWORD i = 0; i < groups * 4; i++)
        dst[i] = blend565_mmx(dst[i], src[i], opacity & 0xff);
}
inline void groups_alpha_opacity_copy_c(WORD *dst, const WORD *src, const BYTE *alpha, DWORD opacity, DWORD groups)
{
    for (DWORD i = 0; i < groups * 4; i++)
        dst[i] = blend565_mmx(dst[i], src[i], (alpha[i] * (opacity & 0xff)) >> 8);
}

#if defined(__i386__)
// ---- The MMX blocks, instruction for instruction those of the original (entry 4 of each table: 0x4516e0,
// 0x453510, 0x455470, 0x459350); edi dst, esi src, eax alpha or weight, ebx the groups, ecx the doubled
// value or the opacity dword --------------------------------------------------------------------------------------

#define ZAX_MMX_KONST                                                                                                    \
    [k3f] "m"(MMX_K3F), [k1f] "m"(MMX_K1F), [k7e0] "m"(MMX_K7E0), [k3f0] "m"(MMX_K3F0), [kf800] "m"(MMX_KF800),         \
        [k10] "m"(MMX_K10), [k200] "m"(MMX_K200)
#define ZAX_MMX_CLOBBERS                                                                                                 \
    "memory", "cc", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7", "st", "st(1)", "st(2)", "st(3)", "st(4)",    \
        "st(5)", "st(6)", "st(7)"

// The blend of one group (shared by the three literal blocks, from the destination load to the store); the
// literal blocks differ only in how mm1 (the weights) is made
#define ZAX_MMX_LITERAL_BODY                                                                                             \
    "movq (%%edi), %%mm5\n\t"                                                                                            \
    "psrlw $2, %%mm1\n\t"                                                                                                \
    "movq %[k3f0], %%mm7\n\t"                                                                                            \
    "psrlw $1, %%mm4\n\t"                                                                                                \
    "movq %%mm1, %%mm0\n\t"                                                                                              \
    "psrlw $1, %%mm5\n\t"                                                                                                \
    "psrlw $1, %%mm1\n\t"                                                                                                \
    "pand %%mm7, %%mm4\n\t"                                                                                              \
    "movq %[k3f], %%mm2\n\t"                                                                                             \
    "pand %%mm7, %%mm5\n\t"                                                                                              \
    "movq (%%esi), %%mm3\n\t"                                                                                            \
    "psubsb %%mm0, %%mm2\n\t"                                                                                            \
    "movq %[k1f], %%mm7\n\t"                                                                                             \
    "pmullw %%mm0, %%mm4\n\t"                                                                                            \
    "movq (%%edi), %%mm0\n\t"                                                                                            \
    "pmullw %%mm2, %%mm5\n\t"                                                                                            \
    "movq %%mm7, %%mm2\n\t"                                                                                              \
    "pand %%mm7, %%mm3\n\t"                                                                                              \
    "pmullw %%mm1, %%mm3\n\t"                                                                                            \
    "pand %%mm7, %%mm0\n\t"                                                                                              \
    "movq (%%esi), %%mm7\n\t"                                                                                            \
    "paddw %%mm5, %%mm4\n\t"                                                                                             \
    "pand %[kf800], %%mm7\n\t"                                                                                           \
    "psubsb %%mm1, %%mm2\n\t"                                                                                            \
    "paddw %[k200], %%mm4\n\t"                                                                                           \
    "pmullw %%mm2, %%mm0\n\t"                                                                                            \
    "movq %%mm4, %%mm5\n\t"                                                                                              \
    "psrlw $0xb, %%mm7\n\t"                                                                                              \
    "psrlw $6, %%mm4\n\t"                                                                                                \
    "paddw %%mm5, %%mm4\n\t"                                                                                             \
    "paddw %%mm3, %%mm0\n\t"                                                                                             \
    "movq (%%edi), %%mm5\n\t"                                                                                            \
    "paddw %[k10], %%mm0\n\t"                                                                                            \
    "pand %[kf800], %%mm5\n\t"                                                                                           \
    "psrlw $5, %%mm4\n\t"                                                                                                \
    "movq %%mm0, %%mm3\n\t"                                                                                              \
    "psrlw $5, %%mm0\n\t"                                                                                                \
    "psrlw $0xb, %%mm5\n\t"                                                                                              \
    "paddw %%mm3, %%mm0\n\t"                                                                                             \
    "psrlw $5, %%mm0\n\t"                                                                                                \
    "pmullw %%mm1, %%mm7\n\t"                                                                                            \
    "pand %[k7e0], %%mm4\n\t"                                                                                            \
    "pmullw %%mm2, %%mm5\n\t"                                                                                            \
    "por %%mm4, %%mm0\n\t"

#define ZAX_MMX_LITERAL_STORE                                                                                            \
    "paddw %%mm7, %%mm5\n\t"                                                                                             \
    "paddw %[k10], %%mm5\n\t"                                                                                            \
    "movq %%mm5, %%mm7\n\t"                                                                                              \
    "psrlw $5, %%mm5\n\t"                                                                                                \
    "paddw %%mm7, %%mm5\n\t"                                                                                             \
    "psrlw $5, %%mm5\n\t"                                                                                                \
    "psllw $0xb, %%mm5\n\t"                                                                                              \
    "por %%mm5, %%mm0\n\t"                                                                                               \
    "movq %%mm0, -8(%%edi)\n\t"                                                                                          \
    "decl %%ebx\n\t"

// T18 (0x4516e0): the alpha bytes of the group as words
inline void groups_alpha_copy(WORD *dst, const WORD *src, const BYTE *alpha, DWORD groups)
{
    __asm__ volatile("movd (%%eax), %%mm1\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "1:\n\t" ZAX_MMX_LITERAL_BODY "addl $4, %%eax\n\t"
                     "addl $8, %%esi\n\t"
                     "addl $8, %%edi\n\t" ZAX_MMX_LITERAL_STORE "je 2f\n\t"
                     "movd (%%eax), %%mm1\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "jmp 1b\n\t"
                     "2:"
                     : "+D"(dst), "+S"(src), "+a"(alpha), "+b"(groups)
                     : ZAX_MMX_KONST
                     : ZAX_MMX_CLOBBERS);
}

// T20 (0x455470): the low byte of the opacity dword in all four lanes
inline void groups_opacity_copy(WORD *dst, const WORD *src, DWORD opacity, DWORD groups)
{
    __asm__ volatile("movd %%eax, %%mm1\n\t"
                     "punpcklbw %%mm1, %%mm1\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "punpcklwd %%mm1, %%mm1\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "movq %%mm1, %%mm6\n\t"
                     "psrlw $2, %%mm1\n\t"
                     "psrlw $3, %%mm6\n\t"
                     "1:\n\t"
                     "movq (%%edi), %%mm5\n\t"
                     "movq %[k3f0], %%mm7\n\t"
                     "psrlw $1, %%mm4\n\t"
                     "movq %%mm1, %%mm0\n\t"
                     "psrlw $1, %%mm5\n\t"
                     "pand %%mm7, %%mm4\n\t"
                     "movq %[k3f], %%mm2\n\t"
                     "pand %%mm7, %%mm5\n\t"
                     "movq (%%esi), %%mm3\n\t"
                     "psubsb %%mm0, %%mm2\n\t"
                     "movq %[k1f], %%mm7\n\t"
                     "pmullw %%mm0, %%mm4\n\t"
                     "movq (%%edi), %%mm0\n\t"
                     "pmullw %%mm2, %%mm5\n\t"
                     "movq %%mm7, %%mm2\n\t"
                     "pand %%mm7, %%mm3\n\t"
                     "pmullw %%mm6, %%mm3\n\t"
                     "pand %%mm7, %%mm0\n\t"
                     "movq (%%esi), %%mm7\n\t"
                     "paddw %%mm5, %%mm4\n\t"
                     "pand %[kf800], %%mm7\n\t"
                     "psubsb %%mm6, %%mm2\n\t"
                     "paddw %[k200], %%mm4\n\t"
                     "pmullw %%mm2, %%mm0\n\t"
                     "movq %%mm4, %%mm5\n\t"
                     "psrlw $0xb, %%mm7\n\t"
                     "psrlw $6, %%mm4\n\t"
                     "paddw %%mm5, %%mm4\n\t"
                     "paddw %%mm3, %%mm0\n\t"
                     "movq (%%edi), %%mm5\n\t"
                     "paddw %[k10], %%mm0\n\t"
                     "pand %[kf800], %%mm5\n\t"
                     "psrlw $5, %%mm4\n\t"
                     "movq %%mm0, %%mm3\n\t"
                     "psrlw $5, %%mm0\n\t"
                     "psrlw $0xb, %%mm5\n\t"
                     "paddw %%mm3, %%mm0\n\t"
                     "psrlw $5, %%mm0\n\t"
                     "pmullw %%mm6, %%mm7\n\t"
                     "pand %[k7e0], %%mm4\n\t"
                     "pmullw %%mm2, %%mm5\n\t"
                     "por %%mm4, %%mm0\n\t"
                     "addl $8, %%esi\n\t"
                     "addl $8, %%edi\n\t" ZAX_MMX_LITERAL_STORE "je 2f\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "jmp 1b\n\t"
                     "2:"
                     : "+D"(dst), "+S"(src), "+b"(groups)
                     : "a"(opacity), ZAX_MMX_KONST
                     : ZAX_MMX_CLOBBERS);
}

// T22 (0x459350): per pixel the alpha byte times the opacity's low byte, >> 8; the dead load into edx kept
inline void groups_alpha_opacity_copy(WORD *dst, const WORD *src, const BYTE *alpha, DWORD opacity, DWORD groups)
{
    __asm__ volatile("movd %%ecx, %%mm6\n\t"
                     "punpcklbw %%mm6, %%mm6\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "punpcklwd %%mm6, %%mm6\n\t"
                     "movd (%%eax), %%mm1\n\t"
                     "punpcklbw %%mm2, %%mm6\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "pmullw %%mm6, %%mm1\n\t"
                     "psrlw $8, %%mm1\n\t"
                     "1:\n\t"
                     "movl (%%eax), %%edx\n\t" ZAX_MMX_LITERAL_BODY "addl $4, %%eax\n\t"
                     "addl $8, %%esi\n\t"
                     "addl $8, %%edi\n\t" ZAX_MMX_LITERAL_STORE "je 2f\n\t"
                     "movd (%%eax), %%mm1\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "pmullw %%mm6, %%mm1\n\t"
                     "psrlw $8, %%mm1\n\t"
                     "movd (%%eax), %%mm1\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "movq (%%esi), %%mm4\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "pmullw %%mm6, %%mm1\n\t"
                     "psrlw $8, %%mm1\n\t"
                     "jmp 1b\n\t"
                     "2:"
                     : "+D"(dst), "+S"(src), "+a"(alpha), "+b"(groups)
                     : "c"(opacity), ZAX_MMX_KONST
                     : ZAX_MMX_CLOBBERS, "edx");
}

// T19, T21, T23 (0x453510): eax the weight byte in all four bytes, ecx the value in both words; ends with emms
inline void groups_fill(WORD *dst, DWORD value, DWORD w, DWORD groups)
{
    const DWORD w4 = (w & 0xff) * 0x01010101u, v2 = (value & 0xffff) << 16 | (value & 0xffff);
    __asm__ volatile("movd %%eax, %%mm1\n\t"
                     "pxor %%mm2, %%mm2\n\t"
                     "movd %%ecx, %%mm4\n\t"
                     "punpcklbw %%mm2, %%mm1\n\t"
                     "movq %%mm1, %%mm6\n\t"
                     "punpcklwd %%mm4, %%mm4\n\t"
                     "psrlw $2, %%mm1\n\t"
                     "psrlw $3, %%mm6\n\t"
                     "1:\n\t"
                     "movq (%%edi), %%mm5\n\t"
                     "movq %[k3f0], %%mm7\n\t"
                     "psrlw $1, %%mm4\n\t"
                     "movq %%mm1, %%mm0\n\t"
                     "psrlw $1, %%mm5\n\t"
                     "pand %%mm7, %%mm4\n\t"
                     "movq %[k3f], %%mm2\n\t"
                     "pand %%mm7, %%mm5\n\t"
                     "movd %%ecx, %%mm3\n\t"
                     "psubsb %%mm0, %%mm2\n\t"
                     "punpcklwd %%mm3, %%mm3\n\t"
                     "movq %[k1f], %%mm7\n\t"
                     "pmullw %%mm0, %%mm4\n\t"
                     "movq (%%edi), %%mm0\n\t"
                     "pmullw %%mm2, %%mm5\n\t"
                     "movq %%mm7, %%mm2\n\t"
                     "pand %%mm7, %%mm3\n\t"
                     "pmullw %%mm6, %%mm3\n\t"
                     "pand %%mm7, %%mm0\n\t"
                     "movd %%ecx, %%mm7\n\t"
                     "paddw %%mm5, %%mm4\n\t"
                     "punpcklwd %%mm7, %%mm7\n\t"
                     "pand %[kf800], %%mm7\n\t"
                     "psubsb %%mm6, %%mm2\n\t"
                     "paddw %[k200], %%mm4\n\t"
                     "pmullw %%mm2, %%mm0\n\t"
                     "movq %%mm4, %%mm5\n\t"
                     "psrlw $0xb, %%mm7\n\t"
                     "psrlw $6, %%mm4\n\t"
                     "paddw %%mm5, %%mm4\n\t"
                     "paddw %%mm3, %%mm0\n\t"
                     "movq (%%edi), %%mm5\n\t"
                     "paddw %[k10], %%mm0\n\t"
                     "pand %[kf800], %%mm5\n\t"
                     "psrlw $5, %%mm4\n\t"
                     "movq %%mm0, %%mm3\n\t"
                     "psrlw $5, %%mm0\n\t"
                     "psrlw $0xb, %%mm5\n\t"
                     "paddw %%mm3, %%mm0\n\t"
                     "psrlw $5, %%mm0\n\t"
                     "pmullw %%mm6, %%mm7\n\t"
                     "pand %[k7e0], %%mm4\n\t"
                     "pmullw %%mm2, %%mm5\n\t"
                     "por %%mm4, %%mm0\n\t"
                     "addl $8, %%edi\n\t"
                     "paddw %%mm7, %%mm5\n\t"
                     "paddw %[k10], %%mm5\n\t"
                     "movq %%mm5, %%mm7\n\t"
                     "psrlw $5, %%mm5\n\t"
                     "movd %%ecx, %%mm4\n\t"
                     "paddw %%mm7, %%mm5\n\t"
                     "punpcklwd %%mm4, %%mm4\n\t"
                     "psrlw $5, %%mm5\n\t"
                     "psllw $0xb, %%mm5\n\t"
                     "por %%mm5, %%mm0\n\t"
                     "movq %%mm0, -8(%%edi)\n\t"
                     "decl %%ebx\n\t"
                     "je 2f\n\t"
                     "jmp 1b\n\t"
                     "2:\n\t"
                     "emms"
                     : "+D"(dst), "+b"(groups)
                     : "a"(w4), "c"(v2), ZAX_MMX_KONST
                     : ZAX_MMX_CLOBBERS);
}
#undef ZAX_MMX_KONST
#undef ZAX_MMX_CLOBBERS
#undef ZAX_MMX_LITERAL_BODY
#undef ZAX_MMX_LITERAL_STORE
#endif

// ---- The span templates (N the entry's count, 0 the generic loop; C: the portable groups) -------------------------

#if defined(__i386__)
#define ZAX_GROUPS(name) (C || g_span_mutation == 3 ? &groups_##name##_c : &groups_##name)
#else
#define ZAX_GROUPS(name) (&groups_##name##_c)
#endif

// T18: groups with the alpha bytes, the tail with alpha[i] (blend565)
template <DWORD N, bool C = false> void __fastcall Span16_Alpha_Copy_MMX(WORD *dst, DWORD src, DWORD alpha, DWORD n)
{
    const DWORD count = N ? N : n, groups = count >> 2;
    const WORD *s = reinterpret_cast<const WORD *>(src);
    const BYTE *a = reinterpret_cast<const BYTE *>(alpha);
    if (groups)
        ZAX_GROUPS(alpha_copy)(dst, s, a, groups);
#if defined(__i386__)
    if (!C && groups && g_span_mutation == 4)
        __asm__ volatile("emms");
#endif
    for (DWORD i = groups * 4; i < count; i++)
        dst[i] = blend565(dst[i], s[i], a[i]);
}
// T19: the low byte of alpha
template <DWORD N, bool C = false> void __fastcall Span16_Alpha_Fill_MMX(WORD *dst, DWORD value, DWORD alpha, DWORD n)
{
    const DWORD count = N ? N : n, groups = count >> 2, w = alpha & 0xff;
    if (groups)
        ZAX_GROUPS(fill)(dst, value, w, groups);
    for (DWORD i = groups * 4; i < count; i++)
        dst[i] = blend565(dst[i], value & 0xffff, w);
}
// T20: the groups with the opacity dword's low byte, the tail with the whole dword
template <DWORD N, bool C = false> void __fastcall Span16_Opacity_Copy_MMX(WORD *dst, DWORD src, DWORD n)
{
    const DWORD count = N ? N : n, groups = count >> 2, w = *at<const DWORD *>(opacity_table() + 0x3fc);
    const WORD *s = reinterpret_cast<const WORD *>(src);
    if (groups)
        ZAX_GROUPS(opacity_copy)(dst, s, w, groups);
    for (DWORD i = groups * 4; i < count; i++)
        dst[i] = blend565(dst[i], s[i], w);
}
// T21: the byte at g_OpacityTable + 0x3fc
template <DWORD N, bool C = false> void __fastcall Span16_Opacity_Fill_MMX(WORD *dst, DWORD value, DWORD n)
{
    const DWORD count = N ? N : n, groups = count >> 2, w = *at<const BYTE *>(opacity_table() + 0x3fc);
    if (groups)
        ZAX_GROUPS(fill)(dst, value, w, groups);
    for (DWORD i = groups * 4; i < count; i++)
        dst[i] = blend565(dst[i], value & 0xffff, w);
}
// T22: the groups with (alpha * opacity byte) >> 8, the tail with the opacity table's dword g_OpacityTable[alpha]
template <DWORD N, bool C = false> void __fastcall Span16_AlphaOpacity_Copy_MMX(WORD *dst, DWORD src, DWORD alpha, DWORD n)
{
    const DWORD count = N ? N : n, groups = count >> 2;
    const WORD *s = reinterpret_cast<const WORD *>(src);
    const BYTE *a = reinterpret_cast<const BYTE *>(alpha);
    if (groups)
        ZAX_GROUPS(alpha_opacity_copy)(dst, s, a, *at<const DWORD *>(opacity_table() + 0x3fc), groups);
    for (DWORD i = groups * 4; i < count; i++)
        dst[i] = blend565(dst[i], s[i], at<const DWORD *>(opacity_table())[a[i]]);
}
// T23: the byte at g_OpacityTable + 4 * (alpha & 0xff)
template <DWORD N, bool C = false> void __fastcall Span16_AlphaOpacity_Fill_MMX(WORD *dst, DWORD value, DWORD alpha, DWORD n)
{
    const DWORD count = N ? N : n, groups = count >> 2, w = at<const BYTE *>(opacity_table())[(alpha & 0xff) * 4];
    if (groups)
        ZAX_GROUPS(fill)(dst, value, w, groups);
    for (DWORD i = groups * 4; i < count; i++)
        dst[i] = blend565(dst[i], value & 0xffff, w);
}
#undef ZAX_GROUPS

}  // namespace zax::render
