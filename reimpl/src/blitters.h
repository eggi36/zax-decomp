// The registered blitters with 8- and 16-bit destinations, their row and edge functions, the RLE frame
// drawers, the screen modes and the Filter mode (phase 4, batch R5 of the blitter map,
// docs/DECOMPILATION.md 9). Modules: rle16.cpp (16-bit RLE sources), rle8.cpp (8-bit RLE sources),
// blitters_raw.cpp (plain frames: Copy, Black Bias, Overlap, Tile, TileWrap, Normal Scale),
// blitters_screen.cpp (the five screen modes of CScreenFrame::vf23) and blitters_filter.cpp (the Filter
// mode); checks in checks_blitters.cpp.
//
// A blitter is fastcall (src, srcRect; dst, dstRect, mode), ret 0xc (frame.h `Blitter`), registered in its
// mode's registry and called by CFrame::vf62 0x4a8f10 after FindBlitter. Every blitter is hooked at its
// entry (E9 with a trampoline, like every other function) rather than patched in the registries: the
// registries hold the original addresses, and a hooked address reaches the reimplementation from the
// registry, from the direct callers (the hue remap and Normal Scale blitters call drawers by address) and
// from the checks alike, and `Disable=` and the checks' hook switching work unchanged [confirmed: every
// entry has five bytes without a relative jump].
//
// The spans (T0-T23 and the dynamic tables of the modes; batches R2-R4) are called through their tables
// (`call [n * 4 + table]`), so the original or the reimplemented span runs, whichever the table holds; the
// rows, edge rows and drawers call each other through their exe addresses, so the checks can put any of
// them back to the original.
//
// The common shapes [confirmed: code; compared in the running game]:
//  - The opacity test of every blitter: `CBlendMode_GetOpacity` (0x433c20) compared with 1.0f (0x5ec3f4)
//    by `fcomp`/`test ah, 0x40`: equal or unordered (NaN) takes the opaque path; otherwise the blitter
//    sets g_OpacityTable 0x6b67e8 = g_FadeTables 0x6347b0 + mode->opacity255 * 0x400 first.
//  - An RLE row (FORMATS.md 5): codes 0x80 | n skip n pixels, 0x40 | n a literal of n source pixels, 0 | n
//    a run of one value (the dword at the code's data; its low byte or word counts); the row ends when the
//    destination x reaches dstRect.right. x starts at dstRect.left - srcRect.left (the source's x = 0).
//    The full rows draw whole segments (no clipping); the clipped rows cut every segment to
//    [dstRect.left, dstRect.right). Edge rows (the edge block) blend with one alpha byte per literal pixel
//    and one per run (the alpha block's bytes in order).
//  - Rows of plain frames: src + (rows[srcRect.top] + srcRect.left) * bytes, dst likewise, the rectangle's
//    height counted first (0 rows: nothing), the width of most inner loops not (do-while: width 0 runs
//    2^32 times; kept).
#pragma once
#include "frame.h"

namespace zax::render::blit {

// fastcall (dst, src or value; n) ret 4, and (dst, src or value; alpha, n) ret 8
using SpanCall = void(__fastcall *)(void *dst, DWORD src, DWORD n);
using AlphaSpanCall = void(__fastcall *)(void *dst, DWORD src, DWORD alpha, DWORD n);
// An RLE row: fastcall (data, alpha; srcRect, dstRow, dstRect), ret 0xc
using RowCall = void(__fastcall *)(const BYTE *data, const BYTE *alpha, const Rect *src_rect, BYTE *row, const Rect *dst_rect);
// A full 16-bit RLE drawer: fastcall (src, srcRect; dst, dstRect), ret 8
using FullCall = void(__fastcall *)(CFrame *src, const Rect *src_rect, CFrame *dst, const Rect *dst_rect);
// An 8-bit RLE drawer: fastcall (src, srcRect; dst, dstRect, level, table), ret 0x10
using Rle8Call = void(__fastcall *)(CFrame *src, const Rect *src_rect, CFrame *dst, const Rect *dst_rect, DWORD level, DWORD table);
// The row of a plain blit: fastcall (dst, src; width, palette, dstRect.left, y), ret 0x10
using PlainRowCall = void(__fastcall *)(BYTE *dst, const BYTE *src, DWORD width, DWORD palette, DWORD left, DWORD y);

constexpr DWORD ONE_F = 0x5ec3f4;          // 1.0f
constexpr DWORD BLEND8_TABLE = 0x6bcc64;   // g_Blend8Table (R2)
constexpr DWORD RLE8_BLOCKS = 0x630330;    // the 8-bit RLE drawers' five (data, rows) pairs, 10 dwords
constexpr DWORD PALETTE16_DWORDS = 0x6343b0, PALETTE15_DWORDS = 0x62fb30;   // the 5-6-5 / 5-5-5 forms, entry i at + 4i
constexpr DWORD INV_COLOR_TABLE = 0x630368;   // pointer to the 64K-byte table 5-6-5 -> palette index
constexpr DWORD OVERLAP_MAP8 = 0x6bcb4c, OVERLAP_MAP16 = 0x6bc64c;
constexpr DWORD HUE_TABLE16 = 0x6bb4d8;    // the current hue remap's 16-bit table (the blitter sets it)

// The span tables T0-T35 (64 entries each, one every 0x100 bytes from 0x60cd90; spans8.h)
constexpr DWORD T0 = 0x60cd90, T1 = 0x60ce90, T2 = 0x60cf90, T3 = 0x60d090, T4 = 0x60d190, T5 = 0x60d290,
                T6 = 0x60d390, T7 = 0x60d490, T8 = 0x60d590, T9 = 0x60d690, T10 = 0x60d790, T11 = 0x60d890,
                T12 = 0x60d990, T13 = 0x60da90, T14 = 0x60db90, T15 = 0x60dc90, T16 = 0x60dd90, T17 = 0x60de90,
                T18 = 0x60df90, T19 = 0x60e090, T20 = 0x60e190, T21 = 0x60e290, T22 = 0x60e390, T23 = 0x60e490,
                T24 = 0x60e590, T25 = 0x60e690, T26 = 0x60e790, T27 = 0x60e890, T28 = 0x60e990, T29 = 0x60ea90,
                T30 = 0x60eb90, T31 = 0x60ec90, T32 = 0x60ed90, T33 = 0x60ee90, T34 = 0x60ef90, T35 = 0x60f090;

// ---- Frame slots (calls through the vtable, so the recording thunks see them) ---------------------------------

inline DWORD sl(const void *f, int slot) { return vcall<DWORD>(f, slot); }
inline BYTE *sl_ptr(const void *f, int slot) { return vcall<BYTE *>(f, slot); }
inline DWORD *sl_rows(const void *f, int slot) { return vcall<DWORD *>(f, slot); }
enum Slot {
    S_EXTRA = 32, S_THIRD = 33, S_ALPHA = 34, S_EDGE = 35, S_FLAGS = 38, S_HEIGHT = 39, S_PITCH = 40, S_LOG2W = 41, S_LOG2H = 42,
    S_ORIGIN_X = 43, S_ORIGIN_Y = 44, S_WIDTH = 45, S_ROWS = 46, S_EXTRA_ROWS = 47, S_THIRD_ROWS = 48, S_ALPHA_ROWS = 49,
    S_EDGE_ROWS = 50, S_PALETTE = 58, S_HAS_PALETTE = 59, S_PIXELS = 31
};

// ---- Blend mode ----------------------------------------------------------------------------------------------

// CBlendMode_GetOpacity (0x433c20) == 1.0f by fcomp: equal or unordered
inline bool opacity_one(const CBlendMode *m)
{
    const float o = m->opacity, one = *glob<const float *>(ONE_F);
    return !(o < one) && !(o > one);
}
inline void set_opacity_table(const CBlendMode *m) { *glob<DWORD *>(OPACITY_TABLE) = glob_addr(FADE_TABLES) + (m->opacity255 << 10); }
// The mode's vtable slot 3 (0x5007b0 in the base): its palette object; the 8-bit blend table is its data
inline DWORD mode_table(CBlendMode *m) { return *vcall<DWORD *>(m, 3); }

inline void emms() { __asm__ volatile("emms"); }

// ---- Spans ------------------------------------------------------------------------------------------------------

inline void span(GlobKey table, DWORD n, void *dst, DWORD src) { at<SpanCall>(glob<const DWORD *>(table)[n])(dst, src, n); }
inline void alpha_span(GlobKey table, DWORD n, void *dst, DWORD src, DWORD alpha)
{
    at<AlphaSpanCall>(glob<const DWORD *>(table)[n])(dst, src, alpha, n);
}
inline DWORD dword_at(const BYTE *p) { return *reinterpret_cast<const DWORD __attribute__((aligned(1), may_alias)) *>(p); }

// ---- RLE rows ---------------------------------------------------------------------------------------------------
// SB bytes per source pixel in the data (1: an 8-bit RLE frame, 2: 16-bit), DB bytes per destination pixel;
// ALPHA: an edge row (one alpha byte per literal pixel and per run, the alpha span tables take it)

// A full row: whole segments from x = dstRect.left - srcRect.left while x < dstRect.right
template <int SB, int DB, bool ALPHA, DWORD LIT, DWORD RUN>
inline void rle_row(const BYTE *p, const BYTE *a, int x, int right, BYTE *row)
{
    if (x >= right)
        return;
    do {
        const BYTE c = *p++;
        if (c & 0x80) {
            x += c & 0x7f;
        } else if (c & 0x40) {
            const DWORD n = c & 0x3f;
            if (ALPHA)
                alpha_span(LIT, n, row + x * DB, (DWORD)p, (DWORD)a);
            else
                span(LIT, n, row + x * DB, (DWORD)p);
            p += n * SB;
            if (ALPHA)
                a += n;
            x += (int)n;
        } else {
            const DWORD n = c & 0x3f;
            if (ALPHA)
                alpha_span(RUN, n, row + x * DB, dword_at(p), *a);
            else
                span(RUN, n, row + x * DB, dword_at(p));
            p += SB;
            if (ALPHA)
                a++;
            x += (int)n;
        }
    } while (x < right);
}

// A clipped row: every segment cut to [dstRect.left, dstRect.right)
template <int SB, int DB, bool ALPHA, DWORD LIT, DWORD RUN>
inline void rle_row_clipped(const BYTE *p, const BYTE *a, const Rect *sr, BYTE *row, const Rect *dr)
{
    const int left = dr->left, right = dr->right;
    int x = left - sr->left;
    if (x >= right)
        return;
    do {
        const BYTE c = *p++;
        if (c & 0x80) {
            x += c & 0x7f;
            continue;
        }
        DWORD n = c & 0x3f;
        const bool literal = (c & 0x40) != 0;
        if (x < left) {
            if ((int)(n + x) < left) {
                x += (int)n;
                if (literal) {
                    p += n * SB;
                    if (ALPHA)
                        a += n;
                } else {
                    p += SB;
                    if (ALPHA)
                        a++;
                }
                continue;
            }
            DWORD k = (DWORD)(left - x);
            if (k >= n)
                k = n;
            n -= k;
            x += (int)k;
            if (literal) {
                p += k * SB;
                if (ALPHA)
                    a += k;
            }
        }
        if (n) {
            DWORD end = n + (DWORD)x;
            if (end >= (DWORD)right)
                end = (DWORD)right;
            const DWORD cnt = end - (DWORD)x;
            if (literal) {
                if (ALPHA)
                    alpha_span(LIT, cnt, row + x * DB, (DWORD)p, (DWORD)a);
                else
                    span(LIT, cnt, row + x * DB, (DWORD)p);
                p += cnt * SB;
                if (ALPHA)
                    a += cnt;
            } else {
                if (ALPHA)
                    alpha_span(RUN, cnt, row + x * DB, dword_at(p), *a);
                else
                    span(RUN, cnt, row + x * DB, dword_at(p));
            }
            x = (int)end;
        }
        if (!literal) {
            p += SB;
            if (ALPHA)
                a++;
        }
    } while (x < right);
}

// ---- MMX emulation (the blitters' own MMX code; the MMX spans T18-T23 are batch R4's) -------------------------

typedef unsigned long long QWORD;
inline QWORD q_at(GlobKey address) { return *glob<const QWORD *>(address); }
inline QWORD q_load(const void *p) { return *reinterpret_cast<const QWORD __attribute__((aligned(1), may_alias)) *>(p); }
inline void q_store(void *p, QWORD v) { *reinterpret_cast<QWORD __attribute__((aligned(1), may_alias)) *>(p) = v; }
inline DWORD lane(QWORD v, int i) { return (DWORD)(v >> (16 * i)) & 0xffff; }
inline QWORD lanes(DWORD a, DWORD b, DWORD c, DWORD d)
{
    return (QWORD)(a & 0xffff) | (QWORD)(b & 0xffff) << 16 | (QWORD)(c & 0xffff) << 32 | (QWORD)(d & 0xffff) << 48;
}
// movd + punpcklwd + punpckldq: the low word of v in all four lanes
inline QWORD dup_low_word(DWORD v) { return lanes(v, v, v, v); }
inline QWORD paddusw(QWORD a, QWORD b)
{
    DWORD r[4];
    for (int i = 0; i < 4; i++) {
        const DWORD s = lane(a, i) + lane(b, i);
        r[i] = s > 0xffff ? 0xffff : s;
    }
    return lanes(r[0], r[1], r[2], r[3]);
}
inline QWORD psllw(QWORD a, int n) { return lanes(lane(a, 0) << n, lane(a, 1) << n, lane(a, 2) << n, lane(a, 3) << n); }
inline QWORD psrlw(QWORD a, int n) { return lanes(lane(a, 0) >> n, lane(a, 1) >> n, lane(a, 2) >> n, lane(a, 3) >> n); }

// ---- Plain blits: the common start ----------------------------------------------------------------------------

// Width and height of dstRect, the source and destination row starts and pitches (slots src 31, 46, dst 46,
// 31, src 40, dst 40 in this order, as all the plain Copy-like blitters call them)
struct PlainSetup {
    DWORD w, h;
    BYTE *s, *d;
    DWORD s_pitch, d_pitch;
};
template <int SB, int DB> inline PlainSetup plain_setup(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr)
{
    PlainSetup p;
    p.w = (DWORD)(dr->right - dr->left);
    p.h = (DWORD)(dr->bottom - dr->top);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    const DWORD *srows = sl_rows(src, S_ROWS);
    p.s = spix + (srows[sr->top] + sr->left) * SB;
    const DWORD *drows = sl_rows(dst, S_ROWS);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    p.d = dpix + (drows[dr->top] + dr->left) * DB;
    p.s_pitch = sl(src, S_PITCH);
    p.d_pitch = sl(dst, S_PITCH);
    return p;
}
// The palette argument of the opaque paths: the source's palette data when it has one (slots 59, 58)
inline DWORD source_palette(CFrame *src) { return (BYTE)sl(src, S_HAS_PALETTE) ? *reinterpret_cast<DWORD *>(sl_ptr(src, S_PALETTE)) : 0; }

// The per-pixel conversion of 8 -> 16 through g_Palette16 or its 5-5-5 form (do-while: width 0 runs 2^32 times)
template <DWORD PALETTE> inline void palette_rows(const PlainSetup &p)
{
    const BYTE *s = p.s;
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch, d += p.d_pitch * 2) {
        DWORD n = p.w, i = 0;
        do {
            reinterpret_cast<WORD *>(d)[i] = (WORD)glob<const DWORD *>(PALETTE)[s[i]];
            i++;
        } while (--n);
    }
}

// 0x43ee70 BlitCopy_8To16 and the screen modes' 0x443400 (the 5-5-5 palette): both paths the same,
// the opaque one asking for the source's palette (slots 59, 58) and ignoring it
template <DWORD PALETTE> void __fastcall BlitPalette_8To16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode))
        source_palette(src);
    else
        set_opacity_table(mode);
    palette_rows<PALETTE>(plain_setup<1, 2>(src, sr, dst, dr));
}

}  // namespace zax::render::blit
