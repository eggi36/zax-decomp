// The blitters with 24- and 32-bit destinations or sources (batch R6 of the blitter map,
// docs/DECOMPILATION.md 9; shapes in blit24.h and blitters.h): Copy (registered by Copy, Transparent Mask,
// Black Bias / Test Light and Amplify Light for their raw 24/32-bit keys) with its rows, the 24-bit and
// 16 -> 32-bit RLE blitters with their drawers and rows, Normal Scale 24 -> 24/32, Tile and TileWrap 24 -> 24
// and the screen mode's 24 -> 16.
//
// [confirmed: code; compared in the running game]
//  - Plain blits: dstRect's size, the row starts src + (rows[srcRect.top] + srcRect.left) * bytes (slots src
//    31, 46, dst 46, 31, src 40, dst 40), the height tested first. The opaque paths ask the source for a
//    palette (slots 59, 58); 24 -> 24 and 32 -> 32 pass its data to their rows (0x4378f0, 0x437950: (dst, src;
//    width, palette, dstRect.left, y), a byte copy of width * 3 or * 4 bytes), which ignore it; their opacity
//    paths copy the same bytes inline: no blend at all.
//  - The conversions: 8 -> 24/32 through the mode's palette data (vtable slot 3, also in g_Blend8Table,
//    cleared at the end), on the opaque path the source's palette when the mode has none (a source without
//    one reads from address 0); 16 -> 24/32 as the spans T24 (blue p << 3, green (p >> 5) << 2, red
//    (p >> 8) & 0xf8); 24 -> 8 the inverse colour table *0x633388 at blue[b] + green15[g] + red15[r]
//    (0x630378, 0x6753b8, 0x633b94); 24 -> 16 Pixel16's tables (blue, 0x675bb8, 0x633394); the screen mode's
//    24 -> 16 the 5-5-5 ones; to 32 bits the alpha byte 0xff. Both paths draw the same; the width loops are
//    do-whiles (width 0 runs 2^32 times).
//  - 32 -> 24 blends with the source's alpha byte a: opaque path a >= 0xfd copies, a = 0 skips, else the
//    pixel function 0x435230 (fastcall (weight, dst; out, dst pixel, src pixel), ret 0xc, returns out:
//    c = d + trunc((s - d) * w / 255) per channel, put back together like blend24); opacity path a <= 5
//    skips, w = g_OpacityTable[a] (a dword), w > 0xfa copies, else the same blend with w.
//  - RLE: 24R -> 24 (Transparent Mask, Black Bias, Amplify Light) and 16R -> 32 (Copy, Transparent Mask)
//    like the 16-bit ones of R5: the full drawer when srcRect covers the whole width, else the clipped rows;
//    24-bit data has 3 bytes per pixel. 16R -> 32 has no opacity functions: its opacity paths set
//    g_OpacityTable and draw with the copy rows (T24-T27).
//  - Normal Scale, Tile and TileWrap: R5's shapes with 3-byte pixels; Normal Scale's 24 -> 32 row writes the
//    three colour bytes and leaves the destination's alpha byte as it was.
#include "blit24.h"

namespace zax {

namespace render::blit {

namespace {

constexpr DWORD INV_COLOR15 = 0x633388;   // pointer to the 32K-byte table 5-5-5 -> palette index

// ---- Rows ---------------------------------------------------------------------------------------------------------

// 0x4378f0 / 0x437950 (dst, src; width, palette, left, y): width * BYTES bytes
template <int BYTES> void __fastcall CopyRow(BYTE *dst, const BYTE *src, DWORD width, DWORD, DWORD, DWORD) { copy_bytes(dst, src, width * BYTES); }

// 0x435230 (weight, dst; out, dst pixel, src pixel), ret 0xc: the 32 -> 24 blend of one pixel into `out`
using Blend32To24 = BYTE *(__fastcall *)(DWORD w, BYTE *dst, BYTE *out, DWORD d, DWORD s);
DWORD q255(DWORD s, DWORD d, DWORD w) { return (DWORD)((int)((s - d) * w) / 255); }
BYTE *__fastcall Blend32To24_Pixel(DWORD w, BYTE *, BYTE *out, DWORD d, DWORD s)
{
    count_call(fn_addr(0x435230));
    const DWORD d0 = d & 0xff, d1 = (d >> 8) & 0xff, d2 = (d >> 16) & 0xff;
    const DWORD r = ((((d2 + q255((s >> 16) & 0xff, d2, w)) << 8) + d1 + q255((s >> 8) & 0xff, d1, w)) << 8) + d0 + q255(s & 0xff, d0, w);
    out[0] = (BYTE)(r + ((g_span24_mutation & 4) && w == 0x80 ? 1 : 0));
    out[1] = (BYTE)(r >> 8);
    out[2] = (BYTE)(r >> 16);
    return out;
}

// ---- Plain blits --------------------------------------------------------------------------------------------------

// 0x434cf0 / 0x435300 BlitCopy_24To24 / _32To32: the opaque path through the row (an exe address) with the
// source's palette data, the opacity path the same bytes inline
template <int BYTES, DWORD ROW> void __fastcall BlitCopyRows(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode)) {
        const DWORD palette = source_palette(src);
        const PlainSetup p = plain_setup<BYTES, BYTES>(src, sr, dst, dr);
        const BYTE *s = p.s;
        BYTE *d = p.d;
        for (DWORD h = p.h, y = (DWORD)dr->top; h; h--, y++, s += p.s_pitch * BYTES, d += p.d_pitch * BYTES)
            code<PlainRowCall>(ROW)(d, s, p.w, palette, (DWORD)dr->left, y);
        return;
    }
    set_opacity_table(mode);
    const PlainSetup p = plain_setup<BYTES, BYTES>(src, sr, dst, dr);
    const BYTE *s = p.s;
    BYTE *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * BYTES, d += p.d_pitch * BYTES)
        copy_bytes(d, s, p.w * BYTES);
}

// 0x434f50 BlitCopy_32To24
void __fastcall BlitCopy_32To24(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool opaque = opacity_one(mode);
    if (opaque)
        ask_palette(src);
    else
        set_opacity_table(mode);
    const PlainSetup p = plain_setup<4, 3>(src, sr, dst, dr);
    plain_rows<4, 3>(p, [&](BYTE *d, const BYTE *s) {
        const DWORD a = s[3];
        DWORD w = a;
        if (opaque) {
            if (!a)
                return;
        } else {
            if (a <= 5)
                return;
            w = at<const DWORD *>(*glob<DWORD *>(OPACITY_TABLE))[a];
        }
        if (opaque ? a >= 0xfd : w > 0xfa) {
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            return;
        }
        BYTE out[4];
        const DWORD dp = d[0] | (DWORD)d[1] << 8 | (DWORD)d[2] << 16 | (w & 0xff000000u);
        const BYTE *r = fn<Blend32To24>(0x435230)(w, d, out, dp, blit::dword_at(s));
        d[0] = r[0];
        d[1] = r[1];
        d[2] = r[2];
    });
}

// 0x43f070 / 0x43f300 BlitCopy_8To24 / _8To32: the palette's three bytes (to 32 bits and alpha 0xff)
template <int DB> void __fastcall BlitCopy_8To(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool opaque = opaque_or_table(mode);
    const BYTE *const palette = mode_palette(mode, src, opaque);
    plain_rows<1, DB>(plain_setup<1, DB>(src, sr, dst, dr), [&](BYTE *d, const BYTE *s) {
        const BYTE *c = palette + *s * 3;
        d[0] = c[0];
        d[1] = c[1];
        d[2] = c[2];
        if (DB == 4)
            d[3] = 0xff;
    });
    *glob<DWORD *>(BLEND8_TABLE) = 0;
}

// The other conversions: the opaque path asks for the source's palette, both draw the same
template <int SB, int DB, class P> void blit_convert(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode, P px)
{
    if (opaque_or_table(mode))
        ask_palette(src);
    plain_rows<SB, DB>(plain_setup<SB, DB>(src, sr, dst, dr), px);
}
// 0x43f780 / 0x43f9b0 BlitCopy_16To24 / _16To32
template <int DB> void __fastcall BlitCopy_16To(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    blit_convert<2, DB>(src, sr, dst, dr, mode, [](BYTE *d, const BYTE *s) {
        px16_to_24(d, *reinterpret_cast<const WORD *>(s));
        if (DB == 4)
            d[3] = 0xff;
    });
}
// 0x43fbf0 BlitCopy_24To8
void __fastcall BlitCopy_24To8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    blit_convert<3, 1>(src, sr, dst, dr, mode, [](BYTE *d, const BYTE *s) {
        const DWORD i = glob<const DWORD *>(PIXEL16_BLUE)[s[0]] + glob<const DWORD *>(PIXEL15_GREEN)[s[1]] + glob<const DWORD *>(PIXEL15_RED)[s[2]];
        *d = (*glob<const BYTE *const *>(INV_COLOR15))[i];
    });
}
// 0x43fe50 BlitCopy_24To16 (5-6-5) and 0x443d70 BlitScreenCopy_24To16 (5-5-5)
template <DWORD GREEN, DWORD RED> void __fastcall Blit24To16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    blit_convert<3, 2>(src, sr, dst, dr, mode, [](BYTE *d, const BYTE *s) {
        *reinterpret_cast<WORD *>(d) = (WORD)(glob<const DWORD *>(PIXEL16_BLUE)[s[0]] + glob<const DWORD *>(GREEN)[s[1]] + glob<const DWORD *>(RED)[s[2]]);
    });
}
// 0x4400b0 BlitCopy_24To32
void __fastcall BlitCopy_24To32(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    blit_convert<3, 4>(src, sr, dst, dr, mode, [](BYTE *d, const BYTE *s) {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = 0xff;
    });
}

// ---- Normal Scale, Tile, TileWrap ------------------------------------------------------------------------------------

// 0x4430e0 / 0x443020 (dst, src; width, start, step, 0): the three bytes of src[x >> 16], x from start
template <int DB> void __fastcall ScaleRow24(BYTE *dst, const BYTE *src, DWORD width, DWORD start, DWORD step, DWORD)
{
    BYTE *const end = dst + width * DB;
    DWORD x = start;
    for (BYTE *d = dst; d < end; d += DB, x += step) {
        const BYTE *s = src + (DWORD)((int)x >> 16) * 3;
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
    }
}

// The 16.16 step for `size` source pixels over `n` destination pixels, lowered by 2 while it overshoots
DWORD scale_step(DWORD size, DWORD n)
{
    DWORD step = (size << 16) / n, total = step * n;
    while ((total >> 16) > size) {
        total += (DWORD)(-(int)n) * 2;
        step -= 2;
    }
    return step;
}

// 0x4427c0 / 0x442bf0 BlitNormalScale_24To24 / _24To32: src 45, 39, dst 45, 39, src 31, dst 31, 46, 40, per row
// src 46
template <int DB, DWORD ROW> void __fastcall BlitNormalScale24(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
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
        const BYTE *srow = spix + (sl_rows(src, S_ROWS)[(int)y >> 16] + sr->left) * 3;
        if (opaque)
            ScaleRow24<DB>(d, srow, dw, 0, xstep, 0);
        else
            code<PlainRowCall>(ROW)(d, srow, dw, 0, xstep, 0);
    }
}

inline void copy3(BYTE *d, const BYTE *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// 0x4467f0 BlitTile_24To24: slots src 31, 46, dst 31, 46, 40, src 41, 42
void __fastcall BlitTile_24To24(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *)
{
    const DWORD w = (DWORD)(dr->right - dr->left), h = (DWORD)(dr->bottom - dr->top);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    const DWORD *srows = sl_rows(src, S_ROWS);
    const DWORD sx = (DWORD)sr->left;
    DWORD sy = (DWORD)sr->top;
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *d = dpix + (sl_rows(dst, S_ROWS)[dr->top] + dr->left) * 3;
    const DWORD pitch = sl(dst, S_PITCH);
    const DWORD xmask = (1u << (sl(src, S_LOG2W) & 31)) - 1;
    const DWORD ymask = (1u << (sl(src, S_LOG2H) & 31)) - 1;
    for (DWORD y = h; y; y--, d += pitch * 3) {
        const BYTE *srow = spix + srows[ymask & sy++] * 3;
        if (!w)
            continue;
        DWORD n = w, x = sx, i = 0;
        do {
            copy3(d + i * 3, srow + (x & xmask) * 3);
            x++;
            i++;
        } while (--n);
    }
}

// 0x4463d0 BlitTileWrap_24To24: slots src 31, 46, dst 46, 31, 40, src 45, 39
void __fastcall BlitTileWrap_24To24(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *)
{
    const DWORD w = (DWORD)(dr->right - dr->left), h = (DWORD)(dr->bottom - dr->top);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    const DWORD *srows = sl_rows(src, S_ROWS);
    const DWORD sx = (DWORD)sr->left;
    DWORD sy = (DWORD)sr->top;
    const DWORD *drows = sl_rows(dst, S_ROWS);
    BYTE *d = sl_ptr(dst, S_PIXELS) + (drows[dr->top] + dr->left) * 3;
    const DWORD pitch = sl(dst, S_PITCH);
    const DWORD width = sl(src, S_WIDTH), height = sl(src, S_HEIGHT);
    for (DWORD y = h; y; y--) {
        const BYTE *srow = spix + srows[sy] * 3;
        sy++;
        if (w) {
            DWORD n = w, x = sx, i = 0;
            do {
                copy3(d + i * 3, srow + x * 3);
                i++;
                if (++x >= width)
                    x = 0;
            } while (--n);
        }
        d += pitch * 3;
        if (sy >= height)
            sy = 0;
    }
}

}  // namespace

}  // namespace render::blit

using namespace render;
using namespace render::blit;

ZAX_HOOKS(blit24)
{
    const Hook list[] = {
        {0x4378f0, (void *)&CopyRow<3>, "BlitCopy_24To24_Row", 7, {0x8b, 0xc1, 0x56, 0x8b, 0x4c, 0x24, 0x08, 0xa8}},
        {0x437950, (void *)&CopyRow<4>, "BlitCopy_32To32_Row", 7, {0x8b, 0xc1, 0x56, 0x8b, 0x4c, 0x24, 0x08, 0xa8}},
        {0x435230, (void *)&Blend32To24_Pixel, "Blend32To24_Pixel", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x434cf0, (void *)&Counted<0x434cf0, &BlitCopyRows<3, 0x4378f0>>, "BlitCopy_24To24", 8, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x1c}},
        {0x435300, (void *)&Counted<0x435300, &BlitCopyRows<4, 0x437950>>, "BlitCopy_32To32", 8, {0x83, 0xec, 0x10, 0x53, 0x8b, 0x5c, 0x24, 0x20}},
        {0x434f50, (void *)&Counted<0x434f50, &BlitCopy_32To24>, "BlitCopy_32To24", 8, {0x83, 0xec, 0x10, 0x53, 0x8b, 0x5c, 0x24, 0x20}},
        {0x43f070, (void *)&Counted<0x43f070, &BlitCopy_8To<3>>, "BlitCopy_8To24", 8, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x1c}},
        {0x43f300, (void *)&Counted<0x43f300, &BlitCopy_8To<4>>, "BlitCopy_8To32", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x43f780, (void *)&Counted<0x43f780, &BlitCopy_16To<3>>, "BlitCopy_16To24", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x14, 0x55, 0x56}},
        {0x43f9b0, (void *)&Counted<0x43f9b0, &BlitCopy_16To<4>>, "BlitCopy_16To32", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x18}},
        {0x43fbf0, (void *)&Counted<0x43fbf0, &BlitCopy_24To8>, "BlitCopy_24To8", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x18}},
        {0x43fe50, (void *)&Counted<0x43fe50, &Blit24To16<PIXEL16_GREEN, PIXEL16_RED>>, "BlitCopy_24To16", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x18}},
        {0x443d70, (void *)&Counted<0x443d70, &Blit24To16<PIXEL15_GREEN, PIXEL15_RED>>, "BlitScreenCopy_24To16", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x18}},
        {0x4400b0, (void *)&Counted<0x4400b0, &BlitCopy_24To32>, "BlitCopy_24To32", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x14, 0x55, 0x56}},
        {0x438080, (void *)&RleRowClipped<3, 3, T28, T29>, "Rle24_DrawRowClipped_Copy24", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x04}},
        {0x438180, (void *)&RleEdgeClipped<3, 3, T30, T31>, "RleEdge_RowClipped_Alpha24", 5, {0x51, 0x8b, 0x44, 0x24, 0x10, 0x53, 0x55, 0x8b}},
        {0x4382a0, (void *)&RleRowClipped<3, 3, T32, T33>, "Rle24_DrawRowClipped_Opacity24", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x04}},
        {0x4383a0, (void *)&RleEdgeClipped<3, 3, T34, T35>, "RleEdge_RowClipped_AlphaOpacity24", 5, {0x51, 0x8b, 0x44, 0x24, 0x10, 0x53, 0x55, 0x8b}},
        {0x4384c0, (void *)&RleEdgeRow<3, 3, T30, T31>, "RleEdge_Row_Alpha24", 5, {0x8b, 0x44, 0x24, 0x0c, 0x55, 0x8b, 0xea, 0x56}},
        {0x438560, (void *)&RleEdgeRow<3, 3, T34, T35>, "RleEdge_Row_AlphaOpacity24", 5, {0x8b, 0x44, 0x24, 0x0c, 0x55, 0x8b, 0xea, 0x56}},
        {0x4374d0, (void *)&RleDrawFull<3, 3, T28, T29, 0x4384c0>, "Rle24_DrawFull_Copy24_EdgeAlpha24", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x4376b0, (void *)&RleDrawFull<3, 3, T32, T33, 0x438560>, "Rle24_DrawFull_Opacity24_EdgeAlphaOpacity24", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x435cb0, (void *)&Counted<0x435cb0, &BlitRle<3, 0x438080, 0x438180, 0x4382a0, 0x4383a0, 0x4374d0, 0x4376b0>>, "BlitTransparentMask_24RleTo24", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x8b, 0xf2}},
        {0x4417e0, (void *)&RleRowClipped<2, 4, T24, T25>, "Rle16_DrawRowClipped_16To32", 5, {0x8b, 0x54, 0x24, 0x04, 0x53, 0x8b, 0xd9, 0x55}},
        {0x4418d0, (void *)&RleEdgeClipped<2, 4, T26, T27>, "RleEdge_RowClipped_16To32_Alpha", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xea}},
        {0x4419e0, (void *)&RleEdgeRow<2, 4, T26, T27>, "RleEdge_Row_16To32_Alpha", 5, {0x8b, 0x44, 0x24, 0x0c, 0x55, 0x8b, 0xea, 0x56}},
        {0x441430, (void *)&RleDrawFull<2, 4, T24, T25, 0x4419e0>, "Rle16_DrawFull_16To32_Edge16To32_Alpha", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x440390, (void *)&Counted<0x440390, &BlitRle<4, 0x4417e0, 0x4418d0, 0x4417e0, 0x4418d0, 0x441430, 0x441430>>, "BlitCopy_16RleTo32", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x8b, 0xf2}},
        {0x4430e0, (void *)&ScaleRow24<3>, "BlitNormalScale_24To24_Row", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0x8d, 0x04, 0x79}},
        {0x443020, (void *)&ScaleRow24<4>, "BlitNormalScale_24To32_Row", 5, {0x8b, 0x44, 0x24, 0x04, 0x55, 0x57, 0x8b, 0xea}},
        {0x4427c0, (void *)&Counted<0x4427c0, &BlitNormalScale24<3, 0x4430e0>>, "BlitNormalScale_24To24", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x442bf0, (void *)&Counted<0x442bf0, &BlitNormalScale24<4, 0x443020>>, "BlitNormalScale_24To32", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x4467f0, (void *)&Counted<0x4467f0, &BlitTile_24To24>, "BlitTile_24To24", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4463d0, (void *)&Counted<0x4463d0, &BlitTileWrap_24To24>, "BlitTileWrap_24To24", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x57, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
