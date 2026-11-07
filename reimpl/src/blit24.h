// The 24- and 32-bit destinations of the software renderer (phase 4, batch R6 of the blitter map,
// docs/DECOMPILATION.md 9): the span tables T24-T35, the blitters with 24/32-bit destinations or 24/32-bit
// sources (Copy, Transparent Mask, Normal Scale, Tile, TileWrap, ScreenCopy 24 -> 16), their rows and RLE
// drawers, and Transparent Mask's own blitters beyond 0x460000. Modules: blit24_spans32.cpp (T24-T27),
// blit24_spans.cpp (T28-T31), blit24_spans_op.cpp (T32-T35), blit24.cpp (the Copy family, Normal Scale,
// Tile, TileWrap, the screen mode's 24 -> 16), blit24_tm.cpp (Transparent Mask 0x464fe0-0x466910).
//
// The spans [confirmed: code of every entry; compared in the running game]: fastcall (ecx dst, edx src or
// value; [alpha,] n), ret 4 or 8, entry 0 a no-op, entries 1-16 one template instance per count (they ignore
// n), entries 17-63 the generic loop (written by CBlendTransparentMask_Ctor 0x4479c0).
//   T24 Span16To32_Copy        n 5-6-5 source pixels to 32 bits: blue = p << 3, green = (p >> 5) << 2,
//                              red = (p >> 8) & 0xf8 (each its low byte), alpha 0xff
//   T25 Span16To32_Fill        n pixels of one value (its low word) the same way
//   T26 Span16To32_Alpha_Copy  as T24 with the alpha byte alpha[i] (a pointer) as the fourth byte
//   T27 Span16To32_Alpha_Fill  as T25 with the alpha's low byte
//   T28 Span24_Copy            n 3-byte pixels (a word and a byte each)
//   T29 Span24_Fill            the value's low three bytes n times (the first pixel stored, then copied
//                              forward with rep movsb over the rest)
//   T30 Span24_Alpha_Copy      every channel c = d + trunc((s - d) * w / 256), w = alpha[i]
//   T31 Span24_Alpha_Fill      the value's bytes, w = alpha & 0xff
//   T32 Span24_Opacity_Copy    w = the dword at g_OpacityTable + 0x3fc
//   T33 Span24_Opacity_Fill    w = the byte at g_OpacityTable + 0x3fc
//   T34 Span24_AlphaOpacity_Copy  w = the dword g_OpacityTable[alpha[i]]
//   T35 Span24_AlphaOpacity_Fill  w = the byte at g_OpacityTable + 4 * (alpha & 0xff)
// The 24-bit blend (`blend24`) multiplies in 32-bit signed arithmetic (imul, wrapping) and divides by 256
// towards zero (cdq, and edx 0xff, sar 8); the three channels are put back together as
// ((d2 + q2) << 8 + d1 + q1) << 8 + d0 + q0 and its low three bytes stored, so a weight above 0xff (the
// dwords of T32/T34 are not clamped) carries one channel into the next.
// Entry 0: T24, T25, T28, T32 `0x401110` (ret 4), T26, T27, T30, T34 `Stub_Ret8`, T29 and T33 0x45e730
// and T31 and T35 0x463e10: the Fill template for n = 0 (push ecx; mov [esp], edx; pop ecx; ret 4 / 8).
// Do-while loops: the generic loops of T30, T32 and T34 run their body before testing the count (n = 0
// would run 2^32 times); T24-T29, T31, T33 and T35 test it first.
#pragma once
#include "blitters.h"

namespace zax::render {

using Span24 = void(__fastcall *)(BYTE *dst, DWORD src, DWORD n);
using AlphaSpan24 = void(__fastcall *)(BYTE *dst, DWORD src, DWORD alpha, DWORD n);

// [Reimpl] Span24Mutation= (checks only; bits): 1 rounds the 24-bit blend up for w = 0x80, 2 lets T29's generic loop
// write one byte past its pixels, 4 rounds the 32 -> 24 pixel blend up for w = 0x80, 8 lets the 24-bit halving skip
// its byte q + 6; the checks must report each
inline int g_span24_mutation;

// ---- One pixel ------------------------------------------------------------------------------------------------------

// 5-6-5 to the bytes blue, green, red
inline void px16_to_24(BYTE *d, DWORD p)
{
    d[0] = (BYTE)(p << 3);
    d[1] = (BYTE)((p >> 5) << 2);
    d[2] = (BYTE)((p >> 8) & 0xf8);
}

// The quotient of one channel: trunc(p / 256) of the wrapping product p
inline DWORD q256(DWORD s, DWORD d, DWORD w)
{
    const int p = (int)((s - d) * w);
    return (DWORD)((int)((DWORD)p + (p < 0 ? 0xffu : 0u)) >> 8);
}
// The 24-bit blend of destination d with the source bytes s0-s2 and weight w (see above)
inline void blend24(BYTE *d, DWORD s0, DWORD s1, DWORD s2, DWORD w)
{
    const DWORD d0 = d[0], d1 = d[1], d2 = d[2];
    DWORD q0 = q256(s0, d0, w);
    if ((g_span24_mutation & 1) && w == 0x80)
        q0++;
    const DWORD r = ((((d2 + q256(s2, d2, w)) << 8) + d1 + q256(s1, d1, w)) << 8) + d0 + q0;
    d[0] = (BYTE)r;
    d[1] = (BYTE)(r >> 8);
    d[2] = (BYTE)(r >> 16);
}

inline DWORD opacity_table24() { return *glob<DWORD *>(OPACITY_TABLE); }

// ---- The span templates (N fixed for entries 1-16, 0 for the generic loops that take the count) ------------------

// T24 / T26
template <bool ALPHA> inline void span16to32_copy(BYTE *dst, const WORD *src, const BYTE *alpha, DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        px16_to_24(dst + 4 * i, src[i]);
        dst[4 * i + 3] = ALPHA ? alpha[i] : 0xff;
    }
}
// T25 / T27
inline void span16to32_fill(BYTE *dst, DWORD value, BYTE a, DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        px16_to_24(dst + 4 * i, value);
        dst[4 * i + 3] = a;
    }
}
// T28
inline void span24_copy(BYTE *dst, const BYTE *src, DWORD n)
{
    for (DWORD i = 0; i < 3 * n; i++)
        dst[i] = src[i];
}
// T29
inline void span24_fill(BYTE *dst, DWORD value, DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        dst[3 * i] = (BYTE)value;
        dst[3 * i + 1] = (BYTE)(value >> 8);
        dst[3 * i + 2] = (BYTE)(value >> 16);
    }
    if ((g_span24_mutation & 2) && n > 16)
        dst[3 * n] = (BYTE)(value >> 24);
}

// The weights of T30-T35 for pixel i
enum Weight24 { W_ALPHA_PTR, W_ALPHA_VALUE, W_OPACITY_DWORD, W_OPACITY_BYTE, W_ALPHA_OPACITY_DWORD, W_ALPHA_OPACITY_BYTE };
template <int W> inline DWORD weight24(DWORD alpha, DWORD i)
{
    if (W == W_ALPHA_PTR)
        return reinterpret_cast<const BYTE *>(alpha)[i];
    if (W == W_ALPHA_VALUE)
        return alpha & 0xff;
    if (W == W_OPACITY_DWORD)
        return *at<const DWORD *>(opacity_table24() + 0x3fc);
    if (W == W_OPACITY_BYTE)
        return *at<const BYTE *>(opacity_table24() + 0x3fc);
    if (W == W_ALPHA_OPACITY_DWORD)
        return at<const DWORD *>(opacity_table24())[reinterpret_cast<const BYTE *>(alpha)[i]];
    return *at<const BYTE *>(opacity_table24() + 4 * (alpha & 0xff));
}
// T30, T32, T34: n source pixels; DO_WHILE: the generic loop's body runs before the count is tested
template <int W, bool DO_WHILE> inline void span24_blend_copy(BYTE *dst, const BYTE *src, DWORD alpha, DWORD n)
{
    DWORD i = 0;
    if (DO_WHILE) {
        do {
            blend24(dst + 3 * i, src[3 * i], src[3 * i + 1], src[3 * i + 2], weight24<W>(alpha, i));
            i++;
        } while (--n);
        return;
    }
    for (; i < n; i++)
        blend24(dst + 3 * i, src[3 * i], src[3 * i + 1], src[3 * i + 2], weight24<W>(alpha, i));
}
// T31, T33, T35: n pixels of one value
template <int W> inline void span24_blend_fill(BYTE *dst, DWORD value, DWORD alpha, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        blend24(dst + 3 * i, value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, weight24<W>(alpha, i));
}

// ---- The spans (blit24_spans32.cpp, blit24_spans.cpp, blit24_spans_op.cpp) ------------------------------------------

template <DWORD N> void __fastcall Span16To32_Copy(BYTE *dst, DWORD src, DWORD n)
{
    span16to32_copy<false>(dst, reinterpret_cast<const WORD *>(src), nullptr, N ? N : n);
}
template <DWORD N> void __fastcall Span16To32_Fill(BYTE *dst, DWORD value, DWORD n) { span16to32_fill(dst, value, 0xff, N ? N : n); }
template <DWORD N> void __fastcall Span16To32_Alpha_Copy(BYTE *dst, DWORD src, DWORD alpha, DWORD n)
{
    span16to32_copy<true>(dst, reinterpret_cast<const WORD *>(src), reinterpret_cast<const BYTE *>(alpha), N ? N : n);
}
template <DWORD N> void __fastcall Span16To32_Alpha_Fill(BYTE *dst, DWORD value, DWORD alpha, DWORD n)
{
    span16to32_fill(dst, value, (BYTE)alpha, N ? N : n);
}
template <DWORD N> void __fastcall Span24_Copy(BYTE *dst, DWORD src, DWORD n) { span24_copy(dst, reinterpret_cast<const BYTE *>(src), N ? N : n); }
template <DWORD N> void __fastcall Span24_Fill(BYTE *dst, DWORD value, DWORD n) { span24_fill(dst, value, N ? N : n); }
template <DWORD N> void __fastcall Span24_Alpha_Copy(BYTE *dst, DWORD src, DWORD alpha, DWORD n)
{
    span24_blend_copy<W_ALPHA_PTR, N == 0>(dst, reinterpret_cast<const BYTE *>(src), alpha, N ? N : n);
}
template <DWORD N> void __fastcall Span24_Alpha_Fill(BYTE *dst, DWORD value, DWORD alpha, DWORD n)
{
    span24_blend_fill<W_ALPHA_VALUE>(dst, value, alpha, N ? N : n);
}
template <DWORD N> void __fastcall Span24_Opacity_Copy(BYTE *dst, DWORD src, DWORD n)
{
    span24_blend_copy<W_OPACITY_DWORD, N == 0>(dst, reinterpret_cast<const BYTE *>(src), 0, N ? N : n);
}
template <DWORD N> void __fastcall Span24_Opacity_Fill(BYTE *dst, DWORD value, DWORD n) { span24_blend_fill<W_OPACITY_BYTE>(dst, value, 0, N ? N : n); }
template <DWORD N> void __fastcall Span24_AlphaOpacity_Copy(BYTE *dst, DWORD src, DWORD alpha, DWORD n)
{
    span24_blend_copy<W_ALPHA_OPACITY_DWORD, N == 0>(dst, reinterpret_cast<const BYTE *>(src), alpha, N ? N : n);
}
template <DWORD N> void __fastcall Span24_AlphaOpacity_Fill(BYTE *dst, DWORD value, DWORD alpha, DWORD n)
{
    span24_blend_fill<W_ALPHA_OPACITY_BYTE>(dst, value, alpha, N ? N : n);
}
// 0x45e730 (T29[0], T33[0]) and 0x463e10 (T31[0], T35[0]): the Fill template for n = 0
void __fastcall Span24_Fill_0(BYTE *dst, DWORD value, DWORD n);
void __fastcall Span24_AlphaFill_0(BYTE *dst, DWORD value, DWORD alpha, DWORD n);

}  // namespace zax::render

namespace zax::render::blit {

// ---- The evidence counter: calls of the batch's blitters --------------------------------------------------------
// Every blitter of the batch counts its calls by its exe address (only the reimplementation counts: with the
// hooks in, every call the game makes reaches it). The first check of the batch logs the counts the game left
// before any check of the batch ran (checks_blit24.cpp), the evidence for which of them the game draws with.
constexpr DWORD COUNTED[] = {0x434cf0, 0x434f50, 0x435300, 0x435cb0, 0x43f070, 0x43f300, 0x43f780, 0x43f9b0, 0x43fbf0,
                             0x43fe50, 0x4400b0, 0x440390, 0x4427c0, 0x442bf0, 0x443d70, 0x4463d0, 0x4467f0, 0x464fe0,
                             0x4651d0, 0x4653d0, 0x465660, 0x4658d0, 0x465ae0, 0x465d10, 0x435230};
constexpr int NCOUNTED = sizeof COUNTED / sizeof COUNTED[0];
inline DWORD g_counted_calls[NCOUNTED];
inline void count_call(DWORD address)
{
    for (int i = 0; i < NCOUNTED; i++)
        if (COUNTED[i] == address) {
            g_counted_calls[i]++;
            return;
        }
}
template <DWORD A, auto F> void __fastcall Counted(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    count_call(A);
    F(src, sr, dst, dr, mode);
}

// ---- RLE rows and drawers with any source and destination pixel size (SB bytes per RLE pixel, DB per
// destination pixel) [confirmed: code; the same shapes as the 16-bit ones of batch R5] ---------------------------------

template <int SB, int DB, DWORD LIT, DWORD RUN> void __fastcall RleRowClipped(const BYTE *p, const BYTE *, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row_clipped<SB, DB, false, LIT, RUN>(p, nullptr, sr, row, dr);
}
template <int SB, int DB, DWORD LIT, DWORD RUN> void __fastcall RleEdgeClipped(const BYTE *p, const BYTE *a, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row_clipped<SB, DB, true, LIT, RUN>(p, a, sr, row, dr);
}
template <int SB, int DB, DWORD LIT, DWORD RUN> void __fastcall RleEdgeRow(const BYTE *p, const BYTE *a, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row<SB, DB, true, LIT, RUN>(p, a, dr->left - sr->left, dr->right, row);
}

// The edge block row by row through `edge_row` (an exe address) from the destination row `row`: src slots 35,
// 34, 50, 49
inline void rle_edge_rows(CFrame *src, const Rect *sr, BYTE *row, DWORD row_bytes, const Rect *dr, DWORD edge_row)
{
    BYTE *const edge = sl_ptr(src, S_EDGE);
    if (!edge)
        return;
    BYTE *const alpha = sl_ptr(src, S_ALPHA);
    const DWORD *erows = sl_rows(src, S_EDGE_ROWS) + sr->top;
    const DWORD *arows = sl_rows(src, S_ALPHA_ROWS) + sr->top;
    for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, erows++, arows++, row += row_bytes)
        at<RowCall>(edge_row)(edge + *erows, alpha + *arows, sr, row, dr);
}

// The first destination row (dst slots 40, src 31, dst 31, dst 46, then src 46 for the source rows)
struct RleSetup {
    DWORD pitch;
    BYTE *spix, *row0;
    const DWORD *srows;
};
template <int DB> inline RleSetup rle_setup(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr)
{
    RleSetup s;
    s.pitch = sl(dst, S_PITCH);
    s.spix = sl_ptr(src, S_PIXELS);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    s.row0 = dpix + sl_rows(dst, S_ROWS)[dr->top] * DB;
    s.srows = sl_rows(src, S_ROWS) + sr->top;
    return s;
}

// A full drawer (fastcall (src, srcRect; dst, dstRect), ret 8): the pixel block unclipped with LIT/RUN, then
// the edge block through EDGE_ROW
template <int SB, int DB, DWORD LIT, DWORD RUN, DWORD EDGE_ROW> void __fastcall RleDrawFull(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr)
{
    const RleSetup s = rle_setup<DB>(src, sr, dst, dr);
    if (s.spix) {
        BYTE *row = s.row0;
        const DWORD *srows = s.srows;
        for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, srows++, row += s.pitch * DB)
            rle_row<SB, DB, false, LIT, RUN>(s.spix + *srows, nullptr, dr->left - sr->left, dr->right, row);
    }
    rle_edge_rows(src, sr, s.row0, s.pitch * DB, dr, fn_addr(EDGE_ROW));
}

// The clipped rows of an RLE blit: the pixel block through ROW, the edge block through EDGE (exe addresses)
template <int DB> inline void rle_draw_clipped(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, DWORD row_fn, DWORD edge_fn)
{
    const RleSetup s = rle_setup<DB>(src, sr, dst, dr);
    if (s.spix) {
        BYTE *row = s.row0;
        const DWORD *srows = s.srows;
        for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, srows++, row += s.pitch * DB)
            at<RowCall>(row_fn)(s.spix + *srows, nullptr, sr, row, dr);
    }
    rle_edge_rows(src, sr, s.row0, s.pitch * DB, dr, edge_fn);
}

inline bool rle_full_width(CFrame *src, const Rect *sr) { return sr->left == 0 && (DWORD)sr->right == sl(src, S_WIDTH); }

// An RLE blitter: srcRect.left == 0 and srcRect.right == the width (slot 45, asked only when left is 0) take
// the full drawer, otherwise the clipped rows; opacity 1 (or NaN) the copy functions, else g_OpacityTable
// first and the opacity functions (the same ones where the exe has no others)
template <int DB, DWORD ROW, DWORD EDGE, DWORD ROW_OP, DWORD EDGE_OP, DWORD FULL, DWORD FULL_OP>
void __fastcall BlitRle(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (!rle_full_width(src, sr)) {
        if (opacity_one(mode)) {
            rle_draw_clipped<DB>(src, sr, dst, dr, fn_addr(ROW), fn_addr(EDGE));
        } else {
            set_opacity_table(mode);
            rle_draw_clipped<DB>(src, sr, dst, dr, fn_addr(ROW_OP), fn_addr(EDGE_OP));
        }
    } else if (opacity_one(mode)) {
        code<FullCall>(FULL)(src, sr, dst, dr);
    } else {
        set_opacity_table(mode);
        code<FullCall>(FULL_OP)(src, sr, dst, dr);
    }
}

// ---- Plain blits -------------------------------------------------------------------------------------------------------

// The palette of the 8-bit sources: the mode's palette data (vtable slot 3) in g_Blend8Table; on the opaque path
// the source's palette data instead when the mode has none and the source has one (slots 59, 58)
inline BYTE *mode_palette(CBlendMode *mode, CFrame *src, bool opaque)
{
    DWORD table = mode_table(mode);
    *glob<DWORD *>(BLEND8_TABLE) = table;
    if (opaque && (BYTE)sl(src, S_HAS_PALETTE) && !table)
        table = *reinterpret_cast<DWORD *>(sl_ptr(src, S_PALETTE));
    return reinterpret_cast<BYTE *>(table);
}
// The opaque paths of the other plain blitters ask the source for a palette (slots 59, 58) and ignore it
inline void ask_palette(CFrame *src)
{
    if ((BYTE)sl(src, S_HAS_PALETTE))
        sl(src, S_PALETTE);
}
// The opacity test: opaque, or g_OpacityTable set
inline bool opaque_or_table(CBlendMode *mode)
{
    if (opacity_one(mode))
        return true;
    set_opacity_table(mode);
    return false;
}

// Rows of a plain blit through `px(d, s)` per pixel; the width loop a do-while (width 0 runs 2^32 times)
template <int SB, int DB, class P> inline void plain_rows(const PlainSetup &p, P px)
{
    const BYTE *s = p.s;
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * SB, d += p.d_pitch * DB) {
        DWORD n = p.w, i = 0;
        do {
            px(d + i * DB, s + i * SB);
            i++;
        } while (--n);
    }
}
// The same with a loop that tests first (`s < end` by pointers)
template <int SB, int DB, class P> inline void plain_rows_tested(const PlainSetup &p, P px)
{
    const BYTE *s = p.s;
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * SB, d += p.d_pitch * DB) {
        const BYTE *const end = s + p.w * SB;
        BYTE *q = d;
        for (const BYTE *r = s; r < end; r += SB, q += DB)
            px(q, r);
    }
}

// A byte copy of n bytes (the rows 0x4378f0 / 0x437950 and the inline copies: bytes up to an aligned
// destination, dwords, the rest)
inline void copy_bytes(BYTE *dst, const BYTE *src, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        dst[i] = src[i];
}

}  // namespace zax::render::blit
