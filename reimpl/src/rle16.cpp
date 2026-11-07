// The blitters of 16-bit RLE sources to 16-bit frames and their drawers and rows (batch R5 of the blitter map,
// docs/DECOMPILATION.md 9; the shapes in blitters.h): Copy (also Transparent Mask's and Black Bias's 16R -> 16),
// Amplify Light, Overlap and the hue remap, each with its MMX and plain variant where the exe has two.
//
// The family [confirmed: code; compared in the running game]:
//  - Clipped rows (fastcall (data, 0; srcRect, dstRow, dstRect), ret 0xc) and their edge rows (data, alpha;
//    ...), unclipped edge rows (same call), all one shape with different span tables (T10/T11 copy, T12/T13
//    edge, T14/T15 opacity, T16/T17 edge with opacity, T18-T23 the MMX versions, the Amplify and Overlap
//    tables). Amplify's edge tables 0x6b70f0/0x6b76f0 are 0: its edge rows call address 0 (R3).
//  - Full drawers (fastcall (src, srcRect; dst, dstRect), ret 8): the pixel block's rows unclipped (dst
//    slots 40, src 31, dst 31, dst 46, src 46), then the edge block through an unclipped edge row (src
//    slots 35, 34, 50, 49), both from the same first destination row.
//  - Blitters: srcRect.left == 0 and srcRect.right == the source's width (slot 45, asked only when left is 0)
//    take the full drawer, otherwise the clipped rows row by row (the same slots); opacity 1 (or NaN) the
//    copy drawers, else g_OpacityTable and the opacity drawers. The MMX blitters end with `emms` on every
//    path, except Amplify Light's (0x4355f0, the blitter map's finding). Overlap draws the full frame
//    whatever the opacity (only setting g_OpacityTable when it is not 1) and uses the same rows on both
//    clipped paths.
//  - The hue remap (CBlendHueRemap, +0x44 its 16-bit table): the blitter remaps the source's blocks in place
//    through a stack render effect (vtable 0x5eeed4; 0x571b70 constructor, 0x572020 slot 28 saves the
//    blocks and gives the frame a buffer, 0x572680 slot 30 puts them back, 0x571bb0 destructor) and the
//    remap helpers HueRemap8_Literal (an 8-bit RLE block through a 256-byte table) and HueRemap8_Run (a
//    16-bit RLE block through the word table 0x6bb4d8), then draws like Copy. Its opacity clipped path goes
//    through a drawer of its own (0x43d980, 0x43d720: the clipped rows with T14/T15 and the clipped edge rows
//    with opacity in one function). The 16-bit blitters on an 8-bit RLE source would pass an uninitialized
//    stack dword as the remap table (unreachable: they are registered for 16R -> 16 only); 0 here.
//  - Quirks kept: the remap helpers loop while the pixels left in a row are not exactly 0 (a row whose codes
//    overshoot its width runs on) and their literal loops are do-while (a zero-length literal runs 2^32 times).
#include "blitters.h"

namespace zax {

namespace render::blit {

namespace {

constexpr DWORD AMP_LIT_A = 0x6b6ff0, AMP_RUN_A = 0x6b74f0, AMP_LIT_B = 0x6b78f0, AMP_RUN_B = 0x6b7af0, AMP_EDGE_LIT = 0x6b70f0,
                AMP_EDGE_RUN = 0x6b76f0, OVL_LIT = 0x6bc84c, OVL_RUN = 0x6bc94c, OVL_EDGE_LIT = 0x6bca4c, OVL_EDGE_RUN = 0x6bc518;

template <DWORD LIT, DWORD RUN> void __fastcall RowClipped16(const BYTE *p, const BYTE *, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row_clipped<2, 2, false, LIT, RUN>(p, nullptr, sr, row, dr);
}
template <DWORD LIT, DWORD RUN> void __fastcall EdgeClipped16(const BYTE *p, const BYTE *a, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row_clipped<2, 2, true, LIT, RUN>(p, a, sr, row, dr);
}
template <DWORD LIT, DWORD RUN> void __fastcall EdgeRow16(const BYTE *p, const BYTE *a, const Rect *sr, BYTE *row, const Rect *dr)
{
    rle_row<2, 2, true, LIT, RUN>(p, a, dr->left - sr->left, dr->right, row);
}

// The edge block row by row through `edge_row` (an exe address) from the destination row `row`
inline void edge_rows(CFrame *src, const Rect *sr, BYTE *row, DWORD pitch2, const Rect *dr, DWORD edge_row)
{
    BYTE *const edge = sl_ptr(src, S_EDGE);
    if (!edge)
        return;
    BYTE *const alpha = sl_ptr(src, S_ALPHA);
    const DWORD *erows = sl_rows(src, S_EDGE_ROWS) + sr->top;
    const DWORD *arows = sl_rows(src, S_ALPHA_ROWS) + sr->top;
    for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, erows++, arows++, row += pitch2)
        at<RowCall>(edge_row)(edge + *erows, alpha + *arows, sr, row, dr);
}

// A full drawer: the pixel block unclipped with LIT/RUN, then the edge block through EDGE_ROW
template <DWORD LIT, DWORD RUN, DWORD EDGE_ROW> void __fastcall DrawFull16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr)
{
    const DWORD pitch = sl(dst, S_PITCH);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *const row0 = dpix + sl_rows(dst, S_ROWS)[dr->top] * 2;
    const DWORD *srows = sl_rows(src, S_ROWS) + sr->top;
    if (spix) {
        BYTE *row = row0;
        for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, srows++, row += pitch * 2)
            rle_row<2, 2, false, LIT, RUN>(spix + *srows, nullptr, dr->left - sr->left, dr->right, row);
    }
    edge_rows(src, sr, row0, pitch * 2, dr, fn_addr(EDGE_ROW));
}

// The clipped rows of a 16-bit RLE blit: the pixel block through ROW, the edge block through EDGE (exe addresses)
inline void draw_clipped(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, DWORD row_fn, DWORD edge_fn)
{
    const DWORD pitch = sl(dst, S_PITCH);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *const row0 = dpix + sl_rows(dst, S_ROWS)[dr->top] * 2;
    const DWORD *srows = sl_rows(src, S_ROWS) + sr->top;
    if (spix) {
        BYTE *row = row0;
        for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, srows++, row += pitch * 2)
            at<RowCall>(row_fn)(spix + *srows, nullptr, sr, row, dr);
    }
    edge_rows(src, sr, row0, pitch * 2, dr, edge_fn);
}

// 0x43d980 / 0x43d720 (src, srcRect; dst, dstRect), ret 8: the hue remap's opacity clipped path in one function:
// the pixel block's clipped rows with LIT/RUN inline, then the edge block through EDGE_FN
template <DWORD LIT, DWORD RUN, DWORD EDGE_FN> void __fastcall DrawClippedOpacity16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr)
{
    const DWORD pitch = sl(dst, S_PITCH);
    BYTE *const spix = sl_ptr(src, S_PIXELS);
    BYTE *const dpix = sl_ptr(dst, S_PIXELS);
    BYTE *const row0 = dpix + sl_rows(dst, S_ROWS)[dr->top] * 2;
    const DWORD *srows = sl_rows(src, S_ROWS) + sr->top;
    if (spix) {
        BYTE *row = row0;
        for (DWORD y = (DWORD)sr->top; y < (DWORD)sr->bottom; y++, srows++, row += pitch * 2)
            rle_row_clipped<2, 2, false, LIT, RUN>(spix + *srows, nullptr, sr, row, dr);
    }
    edge_rows(src, sr, row0, pitch * 2, dr, fn_addr(EDGE_FN));
}

inline bool full_width(CFrame *src, const Rect *sr) { return sr->left == 0 && (DWORD)sr->right == sl(src, S_WIDTH); }

// The 16R -> 16 blitters of Copy and Amplify Light: the clipped rows and edge rows, the full drawers, with and
// without the opacity, `emms` at the end for the MMX ones that have it
template <DWORD ROW, DWORD EDGE, DWORD ROW_OP, DWORD EDGE_OP, DWORD FULL, DWORD FULL_OP, bool EMMS>
void __fastcall Blit16Rle(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (!full_width(src, sr)) {
        if (opacity_one(mode)) {
            draw_clipped(src, sr, dst, dr, fn_addr(ROW), fn_addr(EDGE));
        } else {
            set_opacity_table(mode);
            draw_clipped(src, sr, dst, dr, fn_addr(ROW_OP), fn_addr(EDGE_OP));
        }
    } else if (opacity_one(mode)) {
        code<FullCall>(FULL)(src, sr, dst, dr);
    } else {
        set_opacity_table(mode);
        code<FullCall>(FULL_OP)(src, sr, dst, dr);
    }
    if (EMMS)
        emms();
}

// 0x446e30 BlitOverlap_16RleTo16: the same rows on both clipped paths, the full drawer whatever the opacity
void __fastcall BlitOverlap_16RleTo16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (!full_width(src, sr)) {
        if (!opacity_one(mode))
            set_opacity_table(mode);
        draw_clipped(src, sr, dst, dr, fn_addr(0x4476d0), fn_addr(0x4477c0));
        return;
    }
    if (!opacity_one(mode))
        set_opacity_table(mode);
    fn<FullCall>(0x4474f0)(src, sr, dst, dr);
}

// ---- The hue remap ------------------------------------------------------------------------------------------------

// 0x43cd00 (src block, rows; dst block, width, height, table), ret 0x10: an 8-bit RLE block copied with its
// literal and run pixels through the 256-byte table
void __fastcall HueRemap8_Literal(const BYTE *src, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, const BYTE *table)
{
    if (!src || !height)
        return;
    do {
        const BYTE *s = src + *rows;
        BYTE *d = dst + *rows;
        DWORD left = width;
        do {
            const BYTE c = *s++;
            *d++ = c;
            const DWORD n = c & 0x3f;
            if (c & 0x80) {
                left -= c & 0x7f;
            } else if (c & 0x40) {
                DWORD k = n, i = 0;
                do {
                    d[i] = table[s[i]];
                    i++;
                } while (--k);
                s += n;
                d += n;
                left -= n;
            } else {
                *d++ = table[*s++];
                left -= n;
            }
        } while (left);
        rows++;
    } while (--height);
}

// 0x43cde0 (src block, rows; dst block, width, height, unused), ret 0x10: a 16-bit RLE block through the word
// table at 0x6bb4d8
void __fastcall HueRemap8_Run(const BYTE *src, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, DWORD)
{
    if (!src || !height)
        return;
    do {
        const BYTE *s = src + *rows;
        BYTE *d = dst + *rows;
        DWORD left = width;
        do {
            const BYTE c = *s++;
            *d++ = c;
            const DWORD n = c & 0x3f;
            if (c & 0x80) {
                left -= c & 0x7f;
            } else if (c & 0x40) {
                DWORD k = n, i = 0;
                do {
                    const WORD *table = *glob<const WORD *const *>(HUE_TABLE16);
                    reinterpret_cast<WORD *>(d)[i] = table[reinterpret_cast<const WORD *>(s)[i]];
                    i++;
                } while (--k);
                s += n * 2;
                d += n * 2;
                left -= n;
            } else {
                *reinterpret_cast<WORD *>(d) = (*glob<const WORD *const *>(HUE_TABLE16))[*reinterpret_cast<const WORD *>(s)];
                s += 2;
                d += 2;
                left -= n;
            }
        } while (left);
        rows++;
    } while (--height);
}

using RemapCall = void(__fastcall *)(const BYTE *src, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, DWORD table);

// The stack render effect of the hue blitters (0x2c bytes, rounded up): +0x10 .. +0x20 the saved blocks
// (pixels, +0x1c, +0x24, alpha, edge) that slot 28 fills
struct RemapEffect {
    DWORD d[12];
};
inline CFrame *remap_source(CFrame *src)
{
    if (!src)
        return nullptr;
    void *const cls = fn<void *(__cdecl *)()>(0x4a8760)();   // CFrame_ClassInfo
    return fn<bool(THISCALL *)(const void *, void *)>(0x416790)(src, cls) ? src : nullptr;
}
inline void effect_begin(RemapEffect *e) { fn<void(THISCALL *)(void *)>(0x571b70)(e), e->d[0] = vt(0x5eeed4); }
inline void effect_save(RemapEffect *e, CFrame *f) { fn<void(THISCALL *)(void *, CFrame *)>(0x572020)(e, f); }
inline void effect_end(RemapEffect *e, CFrame *f)
{
    fn<void(THISCALL *)(void *, CFrame *)>(0x572680)(e, f);
    fn<void(THISCALL *)(void *)>(0x571bb0)(e);
}

// The five blocks of an 8-bit RLE frame in the order the blitters remap them: data slot, rows slot, the
// effect's dword with the saved copy
constexpr BYTE REMAP_BLOCKS[5][3] = {{S_PIXELS, S_ROWS, 4}, {S_EDGE, S_EDGE_ROWS, 8}, {S_ALPHA, S_ALPHA_ROWS, 7},
                                     {S_THIRD, S_THIRD_ROWS, 6}, {S_EXTRA, S_EXTRA_ROWS, 5}};

// Remaps the source's blocks: an 8-bit RLE frame all five blocks through `table`, a 16-bit RLE frame the pixel
// and edge blocks through 0x6bb4d8 (the effect's saved copies as the source)
inline void remap_blocks(CFrame *f, RemapEffect *e, DWORD table)
{
    const DWORD w = sl(f, S_WIDTH), h = sl(f, S_HEIGHT);
    const DWORD kind = sl(f, S_FLAGS) & 6;
    if (kind == 2) {
        sl(f, S_ORIGIN_X);
        sl(f, S_ORIGIN_Y);
        for (const auto &b : REMAP_BLOCKS) {
            BYTE *const block = sl_ptr(f, b[0]);
            const DWORD *rows = sl_rows(f, b[1]);
            fn<RemapCall>(0x43cd00)(reinterpret_cast<const BYTE *>(e->d[b[2]]), rows, block, w, h, table);
        }
    } else if (kind == 4) {
        sl(f, S_ORIGIN_X);
        sl(f, S_ORIGIN_Y);
        BYTE *block = sl_ptr(f, S_PIXELS);
        const DWORD *rows = sl_rows(f, S_ROWS);
        fn<RemapCall>(0x43cde0)(reinterpret_cast<const BYTE *>(e->d[4]), rows, block, w, h, 0);
        block = sl_ptr(f, S_EDGE);
        rows = sl_rows(f, S_EDGE_ROWS);
        fn<RemapCall>(0x43cde0)(reinterpret_cast<const BYTE *>(e->d[8]), rows, block, w, h, 0);
    }
}

// 0x43c6b0 BlitHueRemap_8RleTo8: remap (the mode's 8-bit table), then Copy's 8-bit RLE drawers with level
// mode->opacity_entry
void __fastcall BlitHueRemap_8RleTo8(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    CFrame *const f = remap_source(src);
    RemapEffect e;
    effect_begin(&e);
    const DWORD table = mode_table(mode);
    effect_save(&e, f);
    remap_blocks(f, &e, table);
    if (!full_width(src, sr))
        fn<Rle8Call>(0x43dbe0)(src, sr, dst, dr, mode->opacity_entry, 0);
    else
        fn<Rle8Call>(0x43e380)(src, sr, dst, dr, mode->opacity_entry, 0);
    effect_end(&e, f);
}

// 0x43d2d0 / 0x43ced0 BlitHueRemap_16RleTo16: the mode's +0x44 table into 0x6bb4d8, remap, then Copy's paths
// (the opacity clipped path through 0x43d980 / 0x43d720)
template <DWORD EDGE, DWORD CLIPPED_OP, DWORD FULL, DWORD FULL_OP, bool EMMS>
void __fastcall BlitHueRemap16(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    CFrame *const f = remap_source(src);
    *glob<DWORD *>(HUE_TABLE16) = reinterpret_cast<const DWORD *>(mode)[0x44 / 4];
    RemapEffect e;
    effect_begin(&e);
    effect_save(&e, f);
    remap_blocks(f, &e, 0);
    if (!full_width(src, sr)) {
        if (opacity_one(mode)) {
            draw_clipped(src, sr, dst, dr, fn_addr(0x43a5c0), fn_addr(EDGE));
        } else {
            set_opacity_table(mode);
            code<FullCall>(CLIPPED_OP)(src, sr, dst, dr);
        }
    } else if (opacity_one(mode)) {
        code<FullCall>(FULL)(src, sr, dst, dr);
    } else {
        set_opacity_table(mode);
        code<FullCall>(FULL_OP)(src, sr, dst, dr);
    }
    if (EMMS)
        emms();
    effect_end(&e, f);
}

}  // namespace

}  // namespace render::blit

using namespace render::blit;

ZAX_HOOKS(rle16)
{
    const Hook list[] = {
#include "rle16_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
