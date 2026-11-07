// The 16-bit spans of the software renderer (phase 4, batch R3 of the blitter map, docs/DECOMPILATION.md 9):
// the span tables T10-T17 (entries 1-16 unrolled, 17-63 the generic loop of each table), the Amplify Light
// spans (its dynamic tables) and the Overlap spans. spans16.cpp has T10-T13, Amplify and Overlap,
// spans16_opacity.cpp T14-T17; both use the templates below. The MMX spans T18-T23 (batch R4) compute the
// same blends and are compared with these scalar versions.
//
// The tables [confirmed: .data, the generic entries from CBlendTransparentMask_Ctor 0x4479c0]: T k at
// 0x60cd90 + k * 0x100, 64 entries; entry 0 a no-op (0x401110 ret 4, Stub_Ret8 0x4f0a80 ret 8), entry n of
// 1-16 an unrolled instance for n pixels (it ignores the count argument), entries 17-63 the generic loop.
// T10[4], T10[6], T10[8] are the 8-bit copies Span8_Copy_8/12/16 (the same bytes; batch R2).
// Calls: fastcall (ecx dst, edx src or value; stack: [alpha,] n); the caller (an RLE frame drawer or row
// function of batch R5) pushes n, the alpha spans also the alpha. eax is left as the code leaves it (no
// caller reads it).
//   T10 Copy           (dst, src; n)          n source pixels, n / 2 dwords then the odd word
//   T11 Fill           (dst, value; n)        n pixels of value (low word), as dwords then the odd word
//   T12 Alpha_Copy     (dst, src; alpha, n)   per pixel blended with the alpha byte alpha[i] (a pointer)
//   T13 Alpha_Fill     (dst, value; alpha, n) value blended with the low byte of alpha (a value)
//   T14 Opacity_Copy   (dst, src; n)          blended with the dword at g_OpacityTable + 0x3fc
//   T15 Opacity_Fill   (dst, value; n)        blended with the byte at g_OpacityTable + 0x3fc
//   T16 AlphaOpacity_Copy (dst, src; alpha, n)   with the dword g_OpacityTable[alpha[i]]
//   T17 AlphaOpacity_Fill (dst, value; alpha, n) with the byte at g_OpacityTable + 4 * (alpha & 0xff)
// g_OpacityTable (0x6b67e8) is set by the blitter to g_FadeTables (0x6347b0) + opacity255 * 0x400.
//
// The blend (Blend16_AlphaPixel 0x44bf70 and every blending span) [confirmed: code; the checks compare all
// instances]: a 5-6-5 destination d and source s, the weight w (the alpha or opacity value) taken as
// a6 = w >> 2 for green and a5 = a6 >> 1 for red and blue, in 32-bit unsigned arithmetic:
//   green = ((((s >> 5) & 63) - ((d >> 5) & 63)) * a6 >> 1) + (d & 0xffe0)) & 0x7e0
//   red   = ((((s >> 11) - (d >> 11)) * a5 << 6) + (d & 0xf83f)) & 0xf800
//   blue  = ((s & 31) - (d & 31)) * a5 >> 5          (a logical shift; a negative difference borrows)
//   result = (d & 31) + blue + green + red, the low word stored.
// The weight is not clamped: a dword weight above 0xff (T14/T16 read whole dwords) multiplies as it is.
// So 255 gives a6 = 63, a5 = 31: never the full source (31/32 and 63/64 of the difference). A 5-5-5 pixel is
// blended as if it were 5-6-5 (the spans know one format; the checks run them on both).
//
// Do-while loops [confirmed: code]: the generic loops of T12, T14 and T16 and both Overlap spans run their
// body before testing the count (dec, jne), so n = 0 would run 2^32 times; their callers never pass 0 (the
// generic entries are only reached for 17-63). The Fill loops T11/T13/T15/T17, Copy T10 and the Amplify
// spans test n first.
//
// Amplify Light (0x434280 literal, 0x434310 fill; its dynamic tables 0x6b6ff0/0x6b78f0 literal and
// 0x6b74f0/0x6b7af0 run, all 64 entries the one function) [confirmed: code]: per channel a 32 x 32 word
// table built by CBlendAmplifyLight_Ctor 0x433df0 (min(31, s + j * s / 31)), indexed (d << 5 | s) by the top
// five bits of each channel: red 0x6b7cf0 (d >> 11, s >> 11), green 0x6b8928 (bits 6-10 of each, the low
// green bit dropped), blue 0x6b67f0; the pixel is the three words or'ed.
// Overlap (0x446a10 run, its tables 0x6bc84c/0x6bc94c; 0x446a30 with alpha, 0x6bca4c/0x6bc518): every
// destination pixel replaced by the word of the ramp map 0x6bc64c at its low byte (built by
// CBlendOverlap_BuildColor 0x446a90); the source and the alpha are ignored.
//
// [Reimpl] SpanMutation= (checks only, 0 normally): 1 lets the blend round green up for a6 = 32, 2 lets the
// generic Fill loop of T11 write its odd word one pixel further; the checks must report both.
#pragma once
#include "frame.h"

namespace zax::render {

constexpr DWORD AMPLIFY_RED = 0x6b7cf0, AMPLIFY_GREEN = 0x6b8928, AMPLIFY_BLUE = 0x6b67f0;   // 1024 words each
constexpr DWORD OVERLAP_MAP = 0x6bc64c;                 // 256 words by the low byte of a pixel

// fastcall (dst, src or value; n), ret 4 and (dst, src or value; alpha, n), ret 8
using Span = void(__fastcall *)(WORD *dst, DWORD src, DWORD n);
using AlphaSpan = void(__fastcall *)(WORD *dst, DWORD src, DWORD alpha, DWORD n);

inline int g_span_mutation;

// The blend of one pixel (see above); w the weight before >> 2
inline WORD blend565(DWORD d, DWORD s, DWORD w)
{
    const DWORD a6 = w >> 2, a5 = a6 >> 1;
    DWORD green = (((((s >> 5) & 63) - ((d >> 5) & 63)) * a6 >> 1) + (d & 0xffffffe0u)) & 0x7e0;
    const DWORD red = ((((s >> 11) - (d >> 11)) * a5 << 6) + (d & 0xf83f)) & 0xf800;
    const DWORD blue = ((s & 31) - (d & 31)) * a5 >> 5;
    if (g_span_mutation == 1 && a6 == 32)
        green = (green + 0x20) & 0x7e0;
    return (WORD)((d & 31) + blue + green + red);
}

// Unaligned dword access (the spans store dwords at any word address)
typedef DWORD __attribute__((aligned(1), may_alias)) UDWORD;
typedef WORD __attribute__((aligned(1), may_alias)) UWORD;

inline DWORD opacity_table() { return *glob<DWORD *>(OPACITY_TABLE); }

// ---- The templates (n fixed for the unrolled entries, 0 for the generic loops that take the count) ----------

// T10: n / 2 dwords, then the odd word
inline void copy16(WORD *dst, const WORD *src, DWORD n)
{
    UDWORD *d = reinterpret_cast<UDWORD *>(dst);
    const UDWORD *s = reinterpret_cast<const UDWORD *>(src);
    for (DWORD i = 0; i < n / 2; i++)
        d[i] = s[i];
    if (n & 1)
        reinterpret_cast<UWORD *>(dst)[n - 1] = reinterpret_cast<const UWORD *>(src)[n - 1];
}
// T11: the value doubled into a dword, n / 2 of them, then the odd word
inline void fill16(WORD *dst, DWORD value, DWORD n)
{
    const DWORD v = value & 0xffff, two = v << 16 | v;
    UDWORD *d = reinterpret_cast<UDWORD *>(dst);
    for (DWORD i = 0; i < n / 2; i++)
        d[i] = two;
    if (n & 1)
        reinterpret_cast<UWORD *>(dst)[n - 1 + (g_span_mutation == 2 && n > 16 ? 1 : 0)] = (WORD)value;
}
// T12 (alpha a pointer to one byte per pixel) and T16 (that byte through the opacity table's dwords)
template <bool OPACITY> inline void alpha_copy16(WORD *dst, const WORD *src, const BYTE *alpha, DWORD n)
{
    DWORD i = 0;
    do {
        const DWORD w = OPACITY ? at<const DWORD *>(opacity_table())[alpha[i]] : alpha[i];
        dst[i] = blend565(dst[i], src[i], w);
        i++;
    } while (--n);
}
// T13 (the alpha's low byte) and T17 (the byte at table + 4 * alpha)
template <bool OPACITY> inline void alpha_fill16(WORD *dst, DWORD value, DWORD alpha, DWORD n)
{
    const DWORD w = OPACITY ? at<const BYTE *>(opacity_table())[(alpha & 0xff) * 4] : alpha & 0xff;
    for (DWORD i = 0; i < n; i++)
        dst[i] = blend565(dst[i], value & 0xffff, w);
}
// T14: the dword at table + 0x3fc
inline void opacity_copy16(WORD *dst, const WORD *src, DWORD n)
{
    const DWORD w = *at<const DWORD *>(opacity_table() + 0x3fc);
    DWORD i = 0;
    do {
        dst[i] = blend565(dst[i], src[i], w);
        i++;
    } while (--n);
}
// T15: the byte at table + 0x3fc
inline void opacity_fill16(WORD *dst, DWORD value, DWORD n)
{
    const DWORD w = *at<const BYTE *>(opacity_table() + 0x3fc);
    for (DWORD i = 0; i < n; i++)
        dst[i] = blend565(dst[i], value & 0xffff, w);
}

// Amplify Light: the three channel tables indexed by destination and source
inline WORD amplify565(DWORD d, DWORD s)
{
    const DWORD g = ((d & 0x7c0) | ((s >> 5) & 0x3e)) >> 1, b = (d & 0x1f) << 5 | (s & 0x1f), r = ((d & 0xf800) | (s >> 5)) >> 6;
    return (WORD)(glob<const WORD *>(AMPLIFY_GREEN)[g] | glob<const WORD *>(AMPLIFY_BLUE)[b] | glob<const WORD *>(AMPLIFY_RED)[r]);
}

// ---- The spans (spans16.cpp, spans16_opacity.cpp) ---------------------------------------------------------------

template <DWORD N> void __fastcall Span16_Copy(WORD *dst, DWORD src, DWORD n);
template <DWORD N> void __fastcall Span16_Fill(WORD *dst, DWORD value, DWORD n);
template <DWORD N> void __fastcall Span16_Alpha_Copy(WORD *dst, DWORD src, DWORD alpha, DWORD n);
template <DWORD N> void __fastcall Span16_Alpha_Fill(WORD *dst, DWORD value, DWORD alpha, DWORD n);
template <DWORD N> void __fastcall Span16_Opacity_Copy(WORD *dst, DWORD src, DWORD n);
template <DWORD N> void __fastcall Span16_Opacity_Fill(WORD *dst, DWORD value, DWORD n);
template <DWORD N> void __fastcall Span16_AlphaOpacity_Copy(WORD *dst, DWORD src, DWORD alpha, DWORD n);
template <DWORD N> void __fastcall Span16_AlphaOpacity_Fill(WORD *dst, DWORD value, DWORD alpha, DWORD n);
void __fastcall Amplify16_Literal(WORD *dst, DWORD src, DWORD n);    // 0x434280
void __fastcall Amplify16_Fill(WORD *dst, DWORD value, DWORD n);     // 0x434310
void __fastcall Overlap16_Fill(WORD *dst, DWORD src, DWORD n);       // 0x446a10
void __fastcall Overlap16_FillAlpha(WORD *dst, DWORD src, DWORD alpha, DWORD n);   // 0x446a30

}  // namespace zax::render
