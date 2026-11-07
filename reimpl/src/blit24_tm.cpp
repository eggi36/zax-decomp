// Transparent Mask's own blitters 0x464fe0-0x466910 (batch R6 of the blitter map, docs/DECOMPILATION.md 9;
// the map left them out): the raw 8 -> 8/16/24/32 and 16 -> 16/24 blitters that skip pixel 0, and the
// 24R -> 32 blitter with its drawers and rows. CBlendTransparentMask_Ctor 0x4479c0 registers them; its other
// keys are Copy's.
//
// [confirmed: code; compared in the running game]
//  - Raw: the plain blit start of blit24.h (slots src 31, 46, dst 46, 31, src 40, dst 40; the opaque path asks
//    the source for a palette, the opacity path only sets g_OpacityTable; both draw the same). A source
//    pixel 0 (index or word) leaves the destination as it is. 8 -> 8 and 16 -> 16 test the width first
//    (a loop over source pointers); 8 -> 16 (g_Palette16's low word), 8 -> 24/32 (the mode's palette data,
//    or the source's on the opaque path, as Copy) and 16 -> 24 (the T24 conversion, no alpha byte) are
//    do-whiles. 16 -> 16 is the blitter of the fog of war (0x508180) and of the sprites drawn from plain
//    16-bit frames: the one blitter of this batch the game uses.
//  - 24R -> 32: the RLE shapes of blit24.h with the **24-bit** span tables T28-T35 on a 32-bit destination:
//    every span writes 3-byte pixels from x * 4, so a literal or run of n pixels packs 3n bytes and the
//    pixels after the first land between the destination's pixels [quirk kept; unreachable in the game:
//    it has no 24-bit RLE frames].
#include "blit24.h"

namespace zax {

namespace render::blit {

namespace {

// 0x464fe0 / 0x4658d0 BlitTransparentMask_8To8 / _16To16: tested width loops
template <int BYTES> void __fastcall BlitTM_Same(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opaque_or_table(mode))
        ask_palette(src);
    plain_rows_tested<BYTES, BYTES>(plain_setup<BYTES, BYTES>(src, sr, dst, dr), [](BYTE *d, const BYTE *s) {
        if (BYTES == 1) {
            if (*s)
                *d = *s;
        } else if (const WORD v = *reinterpret_cast<const WORD *>(s)) {
            *reinterpret_cast<WORD *>(d) = v;
        }
    });
}

// 0x4651d0 BlitTransparentMask_8To16
void __fastcall BlitTM_8To16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opaque_or_table(mode))
        ask_palette(src);
    plain_rows<1, 2>(plain_setup<1, 2>(src, sr, dst, dr), [](BYTE *d, const BYTE *s) {
        if (*s)
            *reinterpret_cast<WORD *>(d) = (WORD)glob<const DWORD *>(PALETTE16_DWORDS)[*s];
    });
}

// 0x4653d0 / 0x465660 BlitTransparentMask_8To24 / _8To32
template <int DB> void __fastcall BlitTM_8To(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool opaque = opaque_or_table(mode);
    const BYTE *const palette = mode_palette(mode, src, opaque);
    plain_rows<1, DB>(plain_setup<1, DB>(src, sr, dst, dr), [&](BYTE *d, const BYTE *s) {
        if (!*s)
            return;
        const BYTE *c = palette + *s * 3;
        d[0] = c[0];
        d[1] = c[1];
        d[2] = c[2];
        if (DB == 4)
            d[3] = 0xff;
    });
    *glob<DWORD *>(BLEND8_TABLE) = 0;
}

// 0x465ae0 BlitTransparentMask_16To24
void __fastcall BlitTM_16To24(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opaque_or_table(mode))
        ask_palette(src);
    plain_rows<2, 3>(plain_setup<2, 3>(src, sr, dst, dr), [](BYTE *d, const BYTE *s) {
        if (const WORD v = *reinterpret_cast<const WORD *>(s))
            px16_to_24(d, v);
    });
}

}  // namespace

}  // namespace render::blit

using namespace render;
using namespace render::blit;

ZAX_HOOKS(blit24_tm)
{
    const Hook list[] = {
        {0x464fe0, (void *)&Counted<0x464fe0, &BlitTM_Same<1>>, "BlitTransparentMask_8To8", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4651d0, (void *)&Counted<0x4651d0, &BlitTM_8To16>, "BlitTransparentMask_8To16", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x18}},
        {0x4653d0, (void *)&Counted<0x4653d0, &BlitTM_8To<3>>, "BlitTransparentMask_8To24", 8, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x1c}},
        {0x465660, (void *)&Counted<0x465660, &BlitTM_8To<4>>, "BlitTransparentMask_8To32", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4658d0, (void *)&Counted<0x4658d0, &BlitTM_Same<2>>, "BlitTransparentMask_16To16", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x14, 0x55, 0x56}},
        {0x465ae0, (void *)&Counted<0x465ae0, &BlitTM_16To24>, "BlitTransparentMask_16To24", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x14, 0x55, 0x56}},
        {0x466440, (void *)&RleRowClipped<3, 4, T28, T29>, "Rle24_DrawRowClipped_Copy24To32", 5, {0x8b, 0x54, 0x24, 0x04, 0x53, 0x8b, 0xd9, 0x55}},
        {0x466540, (void *)&RleEdgeClipped<3, 4, T30, T31>, "RleEdge_RowClipped_Alpha24To32", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xea}},
        {0x466660, (void *)&RleRowClipped<3, 4, T32, T33>, "Rle24_DrawRowClipped_Opacity24To32", 5, {0x8b, 0x54, 0x24, 0x04, 0x53, 0x8b, 0xd9, 0x55}},
        {0x466760, (void *)&RleEdgeClipped<3, 4, T34, T35>, "RleEdge_RowClipped_AlphaOpacity24To32", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xea}},
        {0x466880, (void *)&RleEdgeRow<3, 4, T30, T31>, "RleEdge_Row_Alpha24To32", 5, {0x8b, 0x44, 0x24, 0x0c, 0x55, 0x8b, 0xea, 0x56}},
        {0x466910, (void *)&RleEdgeRow<3, 4, T34, T35>, "RleEdge_Row_AlphaOpacity24To32", 5, {0x8b, 0x44, 0x24, 0x0c, 0x55, 0x8b, 0xea, 0x56}},
        {0x466080, (void *)&RleDrawFull<3, 4, T28, T29, 0x466880>, "Rle24_DrawFull_Copy24To32_EdgeAlpha", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x466260, (void *)&RleDrawFull<3, 4, T32, T33, 0x466910>, "Rle24_DrawFull_Opacity24To32_EdgeAlphaOpacity", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x465d10, (void *)&Counted<0x465d10, &BlitRle<4, 0x466440, 0x466540, 0x466660, 0x466760, 0x466080, 0x466260>>, "BlitTransparentMask_24RleTo32", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x8b, 0xf2}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
