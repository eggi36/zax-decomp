// The blitters of 8-bit RLE sources (batch R5 of the blitter map, docs/DECOMPILATION.md 9; the shapes in
// blitters.h): Copy to 8 and to 16 bits (also Transparent Mask's), Black Bias / Test Light and Amplify Light
// to 8 bits, their frame drawers and edge rows.
//
// [confirmed: code; compared in the running game]
//  - The drawers (fastcall (src, srcRect; dst, dstRect, level, table), ret 0x10; `table` is not read) draw up
//    to five blocks of the source with four pairs of span tables and one row function: `level` (the mode's
//    opacity entry +0x24, 5 - (6 * opacity255 >> 8)) shifts the blocks along the slots, so at level 0 the
//    pixel block is drawn with pair 0 (T0/T1: opaque), the edge block with pair 1, the alpha block with
//    pair 2, the third (+0x24) with pair 3 and the extra block (+0x1c) through the row function, at level k
//    the pixel block with pair k and the first k pairs get nothing; level 5 (opacity below 43/255) and
//    above draws nothing. The pairs are kept in the globals 0x630330-0x630357 ((data, rows) x 5) and read
//    from there; the source's slots are asked in the order (31, 46), (35, 50), (34, 49), (33, 48), (32, 47)
//    from the first pair that gets a block.
//  - The pair tables: Copy T0-T7 and the row with T8/T9 (the 20/80 SD blend); Black Bias and Amplify Light
//    their dynamic tables (R2); to 16 bits every pair the Transparent Mask's palette spans 0x6bcc9c/0x6bcda0.
//  - The full drawers draw whole segments, the clipped ones cut them to the destination rectangle; the
//    blitters choose like the 16-bit ones (srcRect.left == 0 and right == the source's width: full). Black
//    Bias and Amplify Light put their mode's palette data (vtable slot 3) into g_Blend8Table first.
#include "blitters.h"

namespace zax {

namespace render::blit {

namespace {

template <int DB, DWORD LIT, DWORD RUN> void __fastcall RowClipped8(const BYTE *p, const BYTE *, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row_clipped<1, DB, false, LIT, RUN>(p, nullptr, sr, row, dr);
}
template <int DB, DWORD LIT, DWORD RUN> void __fastcall Row8(const BYTE *p, const BYTE *, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row<1, DB, false, LIT, RUN>(p, nullptr, dr->left - sr->left, dr->right, row);
}

// The blocks in drawing order: data slot, rows slot
constexpr BYTE RLE8_SLOTS[5][2] = {{S_PIXELS, S_ROWS}, {S_EDGE, S_EDGE_ROWS}, {S_ALPHA, S_ALPHA_ROWS}, {S_THIRD, S_THIRD_ROWS},
                                   {S_EXTRA, S_EXTRA_ROWS}};

// The four pairs' span tables of a drawer (data keys; key_of gives the address a template argument takes)
struct Rle8Tables {
    GlobKey lit0, run0, lit1, run1, lit2, run2, lit3, run3;
};
#ifdef ZAX_STANDALONE
constexpr DWORD key_of(const GlobKey &k) { return k.address; }
#else
constexpr DWORD key_of(DWORD k) { return k; }
#endif
constexpr Rle8Tables COPY8 = {T0, T1, T2, T3, T4, T5, T6, T7};
constexpr Rle8Tables BLACK_BIAS8 = {0x6b9428, 0x6b9a5c, 0x6b9b5c, 0x6b975c, 0x6b9628, 0x6b9228, 0x6b985c, 0x6b995c};
constexpr Rle8Tables AMPLIFY8 = {0x6b79f0, 0x6b7bf0, 0x6b77f0, 0x6b71f0, 0x6b9128, 0x6b8628, 0x6b8728, 0x6b72f0};
constexpr Rle8Tables TO16 = {0x6bcc9c, 0x6bcda0, 0x6bcc9c, 0x6bcda0, 0x6bcc9c, 0x6bcda0, 0x6bcc9c, 0x6bcda0};

template <int DB, bool CLIPPED, DWORD LIT, DWORD RUN>
inline void rle8_block(const Rect *sr, BYTE *row, DWORD pitch, const Rect *dr, int pair)
{
    DWORD *const g = glob<DWORD *>(RLE8_BLOCKS);
    if (!g[pair * 2])
        return;
    for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, row += pitch * DB) {
        const BYTE *p = reinterpret_cast<const BYTE *>(g[pair * 2] + reinterpret_cast<const DWORD *>(g[pair * 2 + 1])[y]);
        if (CLIPPED)
            rle_row_clipped<1, DB, false, LIT, RUN>(p, nullptr, sr, row, dr);
        else
            rle_row<1, DB, false, LIT, RUN>(p, nullptr, dr->left - sr->left, dr->right, row);
    }
}

// 0x43e380 and the other full drawers, 0x43dbe0 and the other clipped ones
template <int DB, bool CLIPPED, const Rle8Tables &T, DWORD ROW_FN>
void __fastcall DrawRle8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, DWORD level, DWORD)
{
    if (level > 4)
        return;
    DWORD *const g = glob<DWORD *>(RLE8_BLOCKS);
    for (DWORD k = 0; k < level; k++)
        g[k * 2] = 0;
    for (DWORD k = level; k < 5; k++) {
        g[k * 2] = (DWORD)sl_ptr(src, RLE8_SLOTS[k - level][0]);
        g[k * 2 + 1] = (DWORD)sl_rows(src, RLE8_SLOTS[k - level][1]);
    }
    const DWORD pitch = sl(dst, S_PITCH);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *const row0 = dpix + sl_rows(dst, S_ROWS)[dr->top] * DB;
    rle8_block<DB, CLIPPED, key_of(T.lit0), key_of(T.run0)>(sr, row0, pitch, dr, 0);
    rle8_block<DB, CLIPPED, key_of(T.lit1), key_of(T.run1)>(sr, row0, pitch, dr, 1);
    rle8_block<DB, CLIPPED, key_of(T.lit2), key_of(T.run2)>(sr, row0, pitch, dr, 2);
    rle8_block<DB, CLIPPED, key_of(T.lit3), key_of(T.run3)>(sr, row0, pitch, dr, 3);
    if (!g[8])
        return;
    BYTE *row = row0;
    for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, row += pitch * DB)
        code<RowCall>(ROW_FN)(reinterpret_cast<const BYTE *>(g[8] + reinterpret_cast<const DWORD *>(g[9])[y]), nullptr, sr, row, dr);
}

inline bool full_width(CFrame *src, const Rect *sr) { return sr->left == 0 && (DWORD)sr->right == sl(src, S_WIDTH); }

// 0x4402d0 / 0x440330: Copy's 8R -> 8 and 8R -> 16, level mode->opacity_entry
template <DWORD CLIPPED_FN, DWORD FULL_FN> void __fastcall BlitRle8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    code<Rle8Call>(full_width(src, sr) ? CodeKey(FULL_FN) : CodeKey(CLIPPED_FN))(src, sr, dst, dr, mode->opacity_entry, 0);
}

// 0x438870 / 0x435570: Black Bias's and Amplify Light's 8R -> 8 with the mode's table in g_Blend8Table
template <DWORD CLIPPED_FN, DWORD FULL_FN> void __fastcall BlitRle8Table(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool full = full_width(src, sr);
    const DWORD table = mode_table(mode);
    *glob<DWORD *>(BLEND8_TABLE) = table;
    code<Rle8Call>(full ? CodeKey(FULL_FN) : CodeKey(CLIPPED_FN))(src, sr, dst, dr, mode->opacity_entry, table);
}

}  // namespace

}  // namespace render::blit

using namespace render::blit;

ZAX_HOOKS(rle8)
{
    const Hook list[] = {
#include "rle8_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
