// The blitters of plain (uncompressed) 8- and 16-bit sources to 8- and 16-bit frames (batch R5 of the blitter
// map, docs/DECOMPILATION.md 9; the shapes in blitters.h): Copy, Black Bias / Test Light, Overlap, Tile,
// TileWrap and Normal Scale.
//
// [confirmed: code; compared in the running game]
//  - Copy 16 -> 16 and 8 -> 8 copy row by row through a byte copy (0x437890, 0x441610: (dst, src; width,
//    palette, dstRect.left, y): bytes up to a dword-aligned destination, dwords, the tail from its last byte
//    down); their opaque paths ask the source for a palette (slots 59, 58) and pass its data to the row,
//    which ignores it. With an opacity: 16 -> 16 blends every pixel with the dword at g_OpacityTable + 0x3fc
//    (the 16-bit blend of R3, `blend565`), 8 -> 8 takes the 20/80 table whatever the opacity
//    (dst = Alpha20_80[dst * 256 + src]).
//  - 8 -> 16 through g_Palette16, 16 -> 8 through the inverse colour table *0x630368, both paths the same
//    (the opacity only sets g_OpacityTable).
//  - Black Bias 8 -> 8: dst = table[src * 256 + dst] with the mode's table (vtable slot 3) in g_Blend8Table,
//    on the opaque path the source's palette data when the mode has no table; g_Blend8Table is 0 afterwards.
//    Black Bias 16 -> 16 (registered with MMX only): the opaque path is Copy's; with an opacity groups of four
//    pixels in MMX (only the low byte of the weight dword; a6 = w >> 2, a5 = w >> 3, rounded (x + (x >> 6)) >> 5
//    and (x + (x >> 5)) >> 5 in word lanes) and the rest (width mod 4, or the whole row below 4 pixels) with the
//    scalar blend and the whole dword; `emms` at the end.
//  - Overlap: every destination pixel through the Overlap map (8 bits the byte map 0x6bcb4c, 16 bits the word
//    map 0x6bc64c by the pixel's low byte); the source is not read.
//  - Tile (power-of-two sources, the masks from slots 41/42) and TileWrap (any size, the wrap by compare;
//    dead code: no reference to its mode) copy the source repeated from srcRect's corner; no opacity test.
//  - Normal Scale: the whole source scaled to dstRect in 16.16 fixed point, step (size << 16) / dstSize
//    lowered by 2 while step * dstSize >> 16 exceeds the size; nothing when dstRect is empty, starts left of
//    or above 0 or ends beyond the destination (signed). The source row is rows[y >> 16] + srcRect.left
//    (srcRect.top is ignored), asked per row (slot 46). The opaque path draws inline, the opacity path
//    through the row functions (0x443070, 0x442380, 0x4430a0: (dst, src; width, start, step, 0)); both the
//    same. A loop at the start halves copies of the source's size against dstRect and uses neither result.
//  - Quirks kept: the do-while width loops (width 0 runs 2^32 times) of the blending, converting and Overlap
//    loops; Copy's and Tile's row loops test first.
#include "blitters.h"
#include "spans16.h"

namespace zax {

namespace render::blit {

namespace {

// 0x437890 / 0x441610 (dst, src; width, palette, left, y): width * BYTES bytes
template <int BYTES> void __fastcall CopyRow(BYTE *dst, const BYTE *src, DWORD width, DWORD, DWORD, DWORD)
{
    DWORD n = width * BYTES;
    while (((DWORD)dst & 3) && n) {
        *dst++ = *src++;
        n--;
    }
    for (DWORD q = n >> 2; q; q--) {
        *reinterpret_cast<DWORD *>(dst) = dword_at(src);
        dst += 4;
        src += 4;
    }
    switch (n & 3) {
    case 3:
        dst[2] = src[2];
        [[fallthrough]];
    case 2:
        dst[1] = src[1];
        [[fallthrough]];
    case 1:
        dst[0] = src[0];
    }
}

// The opaque path of Copy 16 -> 16 and 8 -> 8 (also Black Bias 16 -> 16's)
template <int BYTES, DWORD ROW> inline void copy_rows(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr)
{
    const DWORD palette = source_palette(src);
    const PlainSetup p = plain_setup<BYTES, BYTES>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    DWORD y = (DWORD)dr->top;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * BYTES, d += p.d_pitch * BYTES)
        code<PlainRowCall>(ROW)(d, s, p.w, palette, (DWORD)dr->left, y++);
}

// 0x434a10 BlitCopy_16To16
void __fastcall BlitCopy_16To16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode)) {
        copy_rows<2, 0x437890>(src, sr, dst, dr);
        return;
    }
    set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 2>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * 2, d += p.d_pitch * 2) {
        const DWORD w = *at<const DWORD *>(*glob<DWORD *>(OPACITY_TABLE) + 0x3fc);
        WORD *dw = reinterpret_cast<WORD *>(d);
        const WORD *sw = reinterpret_cast<const WORD *>(s);
        DWORD n = p.w;
        do {
            *dw = blend565(*dw, *sw++, w);
            dw++;
        } while (--n);
    }
}

// 0x43ec50 BlitCopy_8To8
void __fastcall BlitCopy_8To8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode)) {
        copy_rows<1, 0x441610>(src, sr, dst, dr);
        return;
    }
    set_opacity_table(mode);
    const PlainSetup p = plain_setup<1, 1>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch, d += p.d_pitch) {
        DWORD n = p.w, i = 0;
        do {
            d[i] = (*glob<const BYTE *const *>(ALPHA20_80))[(DWORD)d[i] << 8 | s[i]];
            i++;
        } while (--n);
    }
}

// 0x43f570 BlitCopy_16To8
void __fastcall BlitCopy_16To8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode))
        source_palette(src);
    else
        set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 1>(src, sr, dst, dr);
    const BYTE *s = p.s;
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * 2, d += p.d_pitch) {
        DWORD n = p.w, i = 0;
        do {
            d[i] = (*glob<const BYTE *const *>(INV_COLOR_TABLE))[reinterpret_cast<const WORD *>(s)[i]];
            i++;
        } while (--n);
    }
}

// 0x434390 BlitBlackBias_8To8
void __fastcall BlitBlackBias_8To8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    DWORD table;
    if (opacity_one(mode)) {
        table = mode_table(mode);
        *glob<DWORD *>(BLEND8_TABLE) = table;
        if ((BYTE)sl(src, S_HAS_PALETTE) && !table)
            table = *reinterpret_cast<DWORD *>(sl_ptr(src, S_PALETTE));
    } else {
        set_opacity_table(mode);
        table = mode_table(mode);
        *glob<DWORD *>(BLEND8_TABLE) = table;
    }
    const PlainSetup p = plain_setup<1, 1>(src, sr, dst, dr);
    const BYTE *s = p.s;
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch, d += p.d_pitch) {
        DWORD n = p.w, i = 0;
        do {
            d[i] = reinterpret_cast<const BYTE *>(table)[(DWORD)s[i] << 8 | d[i]];
            i++;
        } while (--n);
    }
    *glob<DWORD *>(BLEND8_TABLE) = 0;
}

// The MMX blend of 0x434600 for one pixel: the weight's low byte a, a6 = a >> 2, a5 = a >> 3, word lanes
inline WORD blend_mmx(DWORD d, DWORD s, DWORD a)
{
    const DWORD a6 = (a & 0xff) >> 2, a5 = (a & 0xff) >> 3;
    const DWORD g = ((((s >> 1) & 0x3f0) * a6 + ((d >> 1) & 0x3f0) * (63 - a6)) & 0xffff) + 0x200;
    const DWORD green = ((((g & 0xffff) + ((g & 0xffff) >> 6)) & 0xffff) >> 5) & 0x7e0;
    const DWORD b = ((d & 0x1f) * (31 - a5) + (s & 0x1f) * a5 + 0x10) & 0xffff;
    const DWORD blue = ((b + (b >> 5)) & 0xffff) >> 5;
    const DWORD r = ((d >> 11) * (31 - a5) + (s >> 11) * a5 + 0x10) & 0xffff;
    const DWORD red = ((((r + (r >> 5)) & 0xffff) >> 5) << 11) & 0xffff;
    return (WORD)(blue | green | red);
}

// 0x434600 BlitBlackBias_16To16_MMX
void __fastcall BlitBlackBias_16To16_MMX(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode)) {
        copy_rows<2, 0x437890>(src, sr, dst, dr);
        emms();
        return;
    }
    set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 2>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * 2, d += p.d_pitch * 2) {
        const DWORD w = *at<const DWORD *>(*glob<DWORD *>(OPACITY_TABLE) + 0x3fc);
        WORD *dw = reinterpret_cast<WORD *>(d);
        const WORD *sw = reinterpret_cast<const WORD *>(s);
        DWORD rest = p.w;
        if (p.w > 3) {
            const DWORD groups = p.w >> 2;
            for (DWORD g = 0; g < groups; g++, dw += 4, sw += 4)
                for (int i = 0; i < 4; i++)
                    dw[i] = blend_mmx(dw[i], sw[i], w);
            rest = p.w - groups * 4;
        }
        for (; rest; rest--, dw++, sw++)
            *dw = blend565(*dw, *sw, w);
    }
    emms();
}

// 0x447170 BlitOverlap_8To8 and 0x447300 BlitOverlap_16To16
void __fastcall BlitOverlap_8To8(CFrame *src, const Rect *, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode))
        source_palette(src);
    else
        set_opacity_table(mode);
    const DWORD w = (DWORD)(dr->right - dr->left), h = (DWORD)(dr->bottom - dr->top);
    sl(src, S_PIXELS);
    sl(src, S_ROWS);
    const DWORD *drows = sl_rows(dst, S_ROWS);
    BYTE *d = sl_ptr(dst, S_PIXELS) + drows[dr->top] + dr->left;
    sl(src, S_PITCH);
    const DWORD pitch = sl(dst, S_PITCH);
    for (DWORD y = h; y; y--, d += pitch) {
        DWORD n = w, i = 0;
        do {
            d[i] = glob<const BYTE *>(OVERLAP_MAP8)[d[i]];
            i++;
        } while (--n);
    }
}
void __fastcall BlitOverlap_16To16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode))
        source_palette(src);
    else
        set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 2>(src, sr, dst, dr);
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, d += p.d_pitch * 2) {
        DWORD n = p.w, i = 0;
        do {
            WORD *const px = reinterpret_cast<WORD *>(d) + i;
            *px = glob<const WORD *>(OVERLAP_MAP16)[*px & 0xff];
            i++;
        } while (--n);
    }
}

// 0x4465b0 / 0x4466d0 BlitTile: slots src 31, 46, dst 31, 46, 40, src 41, 42
template <int BYTES> void __fastcall BlitTile(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *)
{
    const DWORD w = (DWORD)(dr->right - dr->left), h = (DWORD)(dr->bottom - dr->top);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    const DWORD *srows = sl_rows(src, S_ROWS);
    const DWORD sx = (DWORD)sr->left;
    DWORD sy = (DWORD)sr->top;
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *d = dpix + (sl_rows(dst, S_ROWS)[dr->top] + dr->left) * BYTES;
    const DWORD pitch = sl(dst, S_PITCH);
    const DWORD xmask = (1u << (sl(src, S_LOG2W) & 31)) - 1;
    const DWORD ymask = (1u << (sl(src, S_LOG2H) & 31)) - 1;
    for (DWORD y = h; y; y--, d += pitch * BYTES) {
        const BYTE *srow = spix + srows[ymask & sy++] * BYTES;
        if (!w)
            continue;
        DWORD n = w, x = sx, i = 0;
        do {
            if (BYTES == 1)
                d[i] = srow[x & xmask];
            else
                reinterpret_cast<WORD *>(d)[i] = reinterpret_cast<const WORD *>(srow)[x & xmask];
            x++;
            i++;
        } while (--n);
    }
}

// 0x446190 / 0x4462b0 BlitTileWrap: slots src 31, 46, dst 46, 31, 40, src 45, 39
template <int BYTES> void __fastcall BlitTileWrap(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *)
{
    const DWORD w = (DWORD)(dr->right - dr->left), h = (DWORD)(dr->bottom - dr->top);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    const DWORD *srows = sl_rows(src, S_ROWS);
    const DWORD sx = (DWORD)sr->left;
    DWORD sy = (DWORD)sr->top;
    const DWORD *drows = sl_rows(dst, S_ROWS);
    BYTE *d = sl_ptr(dst, S_PIXELS) + (drows[dr->top] + dr->left) * BYTES;
    const DWORD pitch = sl(dst, S_PITCH);
    const DWORD width = sl(src, S_WIDTH), height = sl(src, S_HEIGHT);
    for (DWORD y = h; y; y--) {
        const BYTE *srow = spix + srows[sy] * BYTES;
        sy++;
        if (w) {
            DWORD n = w, x = sx, i = 0;
            do {
                if (BYTES == 1)
                    d[i] = srow[x];
                else
                    reinterpret_cast<WORD *>(d)[i] = reinterpret_cast<const WORD *>(srow)[x];
                i++;
                if (++x >= width)
                    x = 0;
            } while (--n);
        }
        d += pitch * BYTES;
        if (sy >= height)
            sy = 0;
    }
}

// ---- Normal Scale ---------------------------------------------------------------------------------------------

// 0x443070 / 0x442380 / 0x4430a0 (dst, src; width, start, step, 0): dst[i] = src[x >> 16], x from start
template <int SB, int DB> void __fastcall ScaleRow(BYTE *dst, const BYTE *src, DWORD width, DWORD start, DWORD step, DWORD)
{
    BYTE *const end = dst + width * DB;
    DWORD x = start;
    for (BYTE *d = dst; d < end; d += DB, x += step) {
        const DWORD i = (DWORD)((int)x >> 16);
        if (SB == 2)
            *reinterpret_cast<WORD *>(d) = reinterpret_cast<const WORD *>(src)[i];
        else if (DB == 2)
            *reinterpret_cast<WORD *>(d) = (WORD)glob<const DWORD *>(PALETTE16_DWORDS)[src[i]];
        else
            *d = src[i];
    }
}

// The 16.16 step for `size` source pixels over `n` destination pixels, lowered by 2 while it overshoots
inline DWORD scale_step(DWORD size, DWORD n)
{
    DWORD step = (size << 16) / n, total = step * n;
    while ((total >> 16) > size) {
        total += (DWORD)(-(int)n) * 2;
        step -= 2;
    }
    return step;
}

// 0x441b70 / 0x441f70 / 0x4423c0 BlitNormalScale: src 45, 39, dst 45, 39, src 31, dst 31, 46, 40, per row src 46
template <int SB, int DB, DWORD ROW> void __fastcall BlitNormalScale(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool opaque = opacity_one(mode);
    if (!opaque)
        set_opacity_table(mode);
    const DWORD sw = sl(src, S_WIDTH), sh = sl(src, S_HEIGHT);
    if (dr->right == dr->left || dr->bottom == dr->top || dr->left < 0 || dr->top < 0)
        return;
    if (dr->right > (int)sl(dst, S_WIDTH))
        return;
    if (dr->bottom > (int)sl(dst, S_HEIGHT))
        return;
    const DWORD dw = (DWORD)(dr->right - dr->left), dh = (DWORD)(dr->bottom - dr->top);
    const DWORD xstep = scale_step(sw, dw), ystep = scale_step(sh, dh);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *d = dpix + (sl_rows(dst, S_ROWS)[dr->top] + dr->left) * DB;
    const DWORD pitch = sl(dst, S_PITCH);
    DWORD y = 0;
    for (DWORD n = dh; n; n--, y += ystep, d += pitch * DB) {
        const BYTE *srow = spix + (sl_rows(src, S_ROWS)[(int)y >> 16] + sr->left) * SB;
        if (opaque)
            ScaleRow<SB, DB>(d, srow, dw, 0, xstep, 0);
        else
            code<PlainRowCall>(ROW)(d, srow, dw, 0, xstep, 0);
    }
}

}  // namespace

}  // namespace render::blit

using namespace render::blit;

ZAX_HOOKS(blitters_raw)
{
    const Hook list[] = {
#include "blitters_raw_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
