// The depth conversion of the frame load filters (kind 11, vtable 0x5f4914; batch R7, docs/DECOMPILATION.md
// 9): 0x4acda0-0x4aeba0. Types in frame_load.h.
//
// What it does [confirmed: code]: a loaded frame (a TGA of 24 or 32 bits, or the RLE encoder's output whose
// pixels still have 3 or 4 bytes) is brought to the filter's depth (+0x14) in place:
//  - plain frames: every pixel converted, the buffer reused (8 bits: the nearest palette index (0x526e40 on
//    the 5-5-5 inverse table's palette 0x633388, indices 1-254) or the error-diffusion ditherer 0x4811b0
//    with the game palette; 16 bits: Pixel16_FromRGB, or the ditherer quantising to 5 bits per channel
//    until it is satisfied; 24 bits: from 32 bits only, and only when the first pixel's alpha is at least
//    250 (it tests that pixel width x height times));
//  - compressed frames: every block re-encoded row by row in place: the control bytes copied (or dropped,
//    for the alpha block), each literal pixel and each run's one pixel converted, the row table rewritten
//    with the new offsets, the block's size word updated. The alpha block of a 32-bit source becomes one
//    alpha byte per entry (FrameConvert_RleAlphaFrom32 0x4aeba0).
// The flags' RLE kind (2 or 4) and the depth (slot 29) pick the blocks: an "8-bit RLE" frame of 24 bits
// converts only its pixel block, of 32 bits all five; a "16-bit RLE" frame converts pixel, edge and alpha.
// Then CFrame_SetDepth gives the frame its new depth flag. A literal code of length 0 would loop 2^32
// times in the original (do-while counts); the encoder never writes one.
#include "frame_load.h"

namespace zax {

using namespace render;

namespace {

// The ditherer (0x4811b0 ctor (width, spread, palette, inverse palette), 0x481240 dtor, 0x481290 index of
// (r, g, b), 0x481380 add the error to (&r, &g, &b), 0x4814c0 take the quantised (&r, &g, &b); al whether
// to quantise again)
struct Ditherer {
    alignas(8) BYTE data[0x60];
};
void dither_ctor(Ditherer *d, DWORD w, float spread, void *pal, void *inv)
{
    fn<void(THISCALL *)(Ditherer *, DWORD, float, void *, void *)>(0x4811b0)(d, w, spread, pal, inv);
}
void dither_dtor(Ditherer *d) { fn<void(THISCALL *)(Ditherer *)>(0x481240)(d); }
BYTE dither_index(Ditherer *d, float r, float g, float b) { return (BYTE)fn<DWORD(THISCALL *)(Ditherer *, float, float, float)>(0x481290)(d, r, g, b); }
void dither_spread(Ditherer *d, float *r, float *g, float *b) { fn<void(THISCALL *)(Ditherer *, float *, float *, float *)>(0x481380)(d, r, g, b); }
bool dither_settle(Ditherer *d, float *r, float *g, float *b)
{
    return (BYTE)fn<DWORD(THISCALL *)(Ditherer *, float *, float *, float *)>(0x4814c0)(d, r, g, b) != 0;
}
void *game_palette() { return fn<void *(__cdecl *)()>(0x433a80)(); }

inline long double inv255() { return cst(glob_addr(0x5f172c)); }
inline float unit(DWORD v) { return fstp(fild((int)v) * inv255()); }
inline BYTE nearest(Palette *p, const BYTE *s) { return (BYTE)palette_nearest(p, s[2], s[1], s[0], 1, 0xff); }

// One block re-encoded in place (see the header): table[y] = the new offset; `pixel(code, n)` converts n
// pixels of a literal (do-while) or the one of a run (n = 1) from src to dst and advances both and out
using Pixels = void (*)(BYTE *&src, BYTE *&dst, DWORD &out, DWORD n, Palette *pal);

void reencode(BYTE *block, DWORD *table, DWORD w, DWORD h, bool copy_codes, Pixels pixels, Palette *pal = nullptr)
{
    DWORD out = 4;
    for (DWORD y = 0; y < h; y++) {
        BYTE *src = block + table[y];
        table[y] = out;
        BYTE *dst = block + out;
        DWORD rem = w;
        if (!rem)
            continue;
        do {
            const BYTE c = *src++;
            if (copy_codes) {
                *dst++ = c;
                out++;
            }
            if (c & 0x80) {
                rem -= c & 0x7f;
            } else if (c & 0x40) {
                pixels(src, dst, out, c & 0x3fu, pal);
                rem -= c & 0x3f;
            } else {
                pixels(src, dst, out, 1, pal);
                rem -= c & 0x3f;
            }
        } while (rem);
    }
    *reinterpret_cast<DWORD *>(block) = out;
}

void px_32to8(BYTE *&s, BYTE *&d, DWORD &out, DWORD n, Palette *pal)
{
    do {
        *d++ = nearest(pal, s);
        out++;
        s += 4;
    } while (--n);
}
void px_24to8(BYTE *&s, BYTE *&d, DWORD &out, DWORD n, Palette *pal)
{
    do {
        *d++ = nearest(pal, s);
        out++;
        s += 3;
    } while (--n);
}
template <int N> void px_to16(BYTE *&s, BYTE *&d, DWORD &out, DWORD n, Palette *)
{
    do {
        const WORD p = (WORD)pixel16(s[2], s[1], s[0]);
        memcpy(d, &p, 2);
        d += 2;
        out += 2;
        s += N;
    } while (--n);
}
void px_32to24(BYTE *&s, BYTE *&d, DWORD &out, DWORD n, Palette *)
{
    do {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d += 3;
        out += 3;
        s += 4;
    } while (--n);
}
void px_alpha(BYTE *&s, BYTE *&d, DWORD &out, DWORD n, Palette *)
{
    do {
        *d++ = s[3];
        out++;
        s += 4;
    } while (--n);
}

// A block by its slots (table, data) in the order the original asks: height, width, table, data
void convert_block(CFrame *f, int table_slot, int data_slot, bool copy_codes, Pixels pixels, Palette *pal = nullptr)
{
    const DWORD h = vcall<DWORD>(f, 39), w = vcall<DWORD>(f, 45);
    DWORD *table = vcall<DWORD *>(f, table_slot);
    BYTE *block = vcall<BYTE *>(f, data_slot);
    if (block)
        reencode(block, table, w, h, copy_codes, pixels, pal);
}

// The RleTo* helpers' order: (block, table; width, height, copy, unused, palette), the slots height, width,
// table, data
template <DWORD Address> void row_call(CFrame *f, int table_slot, int data_slot, bool copy, Palette *pal)
{
    const DWORD h = vcall<DWORD>(f, 39), w = vcall<DWORD>(f, 45);
    DWORD *table = vcall<DWORD *>(f, table_slot);
    BYTE *block = vcall<BYTE *>(f, data_slot);
    fn_dyn<void(__fastcall *)(BYTE *, DWORD *, DWORD, DWORD, DWORD, DWORD, Palette *)>(Address)(block, table, w, h, copy, 0, pal);
}

// ---- 0x4ad860, 0x4ad8c0, 0x4ae3d0: n pixels through the pointers (fastcall &out, &dst; &src, n, unused,
// palette), the out counter in bytes

void __fastcall FrameConvert_Nearest24(DWORD *out, BYTE **dst, BYTE **src, DWORD n, DWORD, Palette *pal)
{
    do {
        **dst = nearest(pal, *src);
        (*dst)++;
        (*out)++;
        *src += 3;
    } while (--n);
}
void __fastcall FrameConvert_Nearest32(DWORD *out, BYTE **dst, BYTE **src, DWORD n, DWORD, Palette *pal)
{
    do {
        **dst = nearest(pal, *src);
        (*dst)++;
        (*out)++;
        *src += 4;
    } while (--n);
}
void __fastcall FrameConvert_Pixel16From32(DWORD *out, BYTE **dst, BYTE **src, DWORD n, DWORD)
{
    do {
        const WORD p = (WORD)pixel16((*src)[2], (*src)[1], (*src)[0]);
        memcpy(*dst, &p, 2);
        *dst += 2;
        *src += 4;
        *out += 2;
    } while (--n);
}

// ---- 0x4ae820, 0x4ae940, 0x4aea90, 0x4aeba0: one RLE block (fastcall block, table; width, height, copy the
// codes, unused, palette)

void __fastcall FrameConvert_RleTo8From32(BYTE *block, DWORD *table, DWORD w, DWORD h, BYTE copy, DWORD, Palette *pal)
{
    if (block)
        reencode(block, table, w, h, copy != 0, &px_32to8, pal);
}
void __fastcall FrameConvert_RleTo16From24(BYTE *block, DWORD *table, DWORD w, DWORD h, BYTE copy, DWORD, DWORD)
{
    if (block)
        reencode(block, table, w, h, copy != 0, &px_to16<3>);
}
void __fastcall FrameConvert_RleTo16From32(BYTE *block, DWORD *table, DWORD w, DWORD h, BYTE copy, DWORD, DWORD)
{
    if (block)
        reencode(block, table, w, h, copy != 0, &px_to16<4>);
}
void __fastcall FrameConvert_RleAlphaFrom32(BYTE *block, DWORD *table, DWORD w, DWORD h, BYTE copy, DWORD, DWORD)
{
    if (block)
        reencode(block, table, w, h, copy != 0, &px_alpha);
}

constexpr DWORD RLE8_FROM32 = 0x4ae820, RLE16_FROM24 = 0x4ae940, RLE16_FROM32 = 0x4aea90, RLE_ALPHA = 0x4aeba0;

// ---- 0x4ace00 (thiscall filter; frame, palette): a plain 16, 24 or 32-bit frame to palette indices in its
// own buffer (the nearest colour in 0x633388's palette, or the ditherer when the filter's +0x18 is set)

DWORD THISCALL FrameConvert_PlainTo8(FrameFilter *filter, CFrame *f, Palette *)
{
    const DWORD width = vcall<DWORD>(f, 45);
        DWORD n = width * vcall<DWORD>(f, 39);
    const DWORD bits = f_bits(f);
    if (bits != 16 && bits != 24 && bits != 32)
        return 1;
    const BYTE *s = f_pixels(f);
    BYTE *d = f_pixels(f);
    const DWORD step = bits / 8;
    Palette *inv = glob<Palette *>(INV_COLOR15_HOLDER);
    if (reinterpret_cast<const BYTE *>(filter)[0x18]) {
        Ditherer dd;
        void *pal = game_palette();
        dither_ctor(&dd, vcall<DWORD>(f, 45), raw_float(0x3f000000), pal, inv);
        for (; n; n--, s += step) {
            DWORD r, g, b;
            if (bits == 16) {
                const DWORD p = s[0] | s[1] << 8;
                b = (BYTE)(p << 3);
                g = (BYTE)((p >> 5) << 2);
                r = p >> 8 & 0xf8;
            } else {
                b = s[0];
                g = s[1];
                r = s[2];
            }
            const float fb = unit(b), fg = unit(g), fr = unit(r);
            *d++ = dither_index(&dd, fr, fg, fb);
        }
        dither_dtor(&dd);
        return 1;
    }
    for (; n; n--, s += step) {
        if (bits == 16) {
            const DWORD p = s[0] | s[1] << 8;
            *d++ = (BYTE)palette_nearest(inv, p >> 8 & 0xfffff8, p >> 3 & 0xfc, (p & 0x1f) << 3, 1, 0xff);
        } else {
            *d++ = nearest(inv, s);
        }
    }
    return 1;
}

// ---- 0x4ad1e0, 0x4ad920, 0x4ae420 (thiscall filter; frame, palette): to 8, 16, 24 bits

DWORD THISCALL FrameConvert_To8(FrameFilter *filter, CFrame *f, Palette *pal)
{
    const DWORD kind = f_flags(f) & F_COMPRESSED;
    if (kind == 0) {
        fn<DWORD(THISCALL *)(FrameFilter *, CFrame *, Palette *)>(0x4ace00)(filter, f, pal);
    } else if (kind == 2) {
        const DWORD bits = f_bits(f);
        if (bits == 24) {
            convert_block(f, 46, 31, true, &px_24to8, pal);
        } else if (bits == 32) {
            convert_block(f, 46, 31, true, &px_32to8, pal);
            convert_block(f, 50, 35, true, &px_32to8, pal);
            convert_block(f, 49, 34, true, &px_32to8, pal);
            row_call<RLE8_FROM32>(f, 48, 33, true, pal);
            row_call<RLE8_FROM32>(f, 47, 32, true, pal);
        }
    } else if (kind == 4) {
        const DWORD bits = f_bits(f);
        if (bits == 24) {
            row_call<RLE16_FROM24>(f, 46, 31, true, nullptr);
        } else if (bits == 32) {
            row_call<RLE16_FROM32>(f, 46, 31, true, nullptr);
            row_call<RLE16_FROM32>(f, 50, 35, true, nullptr);
            row_call<RLE_ALPHA>(f, 49, 34, false, nullptr);
        }
    }
    frame_set_depth(f, 8);
    return 1;
}

// The 16-bit ditherer of a plain frame: per pixel the three channels as units, the error added, quantised
// to 5 bits (x 31, truncated, / 31) until the ditherer is satisfied, then rounded to 0..255
DWORD dither16(Ditherer *d, const BYTE *s)
{
    float r = unit(s[2]), g = unit(s[1]), b = unit(s[0]);
    dither_spread(d, &r, &g, &b);
    const long double k = cst(glob_addr(0x5eee8c)), q = cst(glob_addr(0x5f4920));
    do {
        r = fstp(fild(x87_ftol(fld(r) * k)) * q);
        g = fstp(fild(x87_ftol(fld(g) * k)) * q);
        b = fstp(fild(x87_ftol(fld(b) * k)) * q);
    } while (dither_settle(d, &r, &g, &b));
    const long double c255 = cst(glob_addr(0x5eb728));
    const int bi = x87_fistp(fld(fstp(fld(b) * c255))), gi = x87_fistp(fld(fstp(fld(g) * c255))),
              ri = x87_fistp(fld(fstp(fld(r) * c255)));
    return pixel16((DWORD)ri, (DWORD)gi, (DWORD)bi);
}

DWORD THISCALL FrameConvert_To16(FrameFilter *filter, CFrame *f, Palette *)
{
    const DWORD kind = f_flags(f) & F_COMPRESSED;
    if (kind == 0) {
        const DWORD width = vcall<DWORD>(f, 45);
        DWORD n = width * vcall<DWORD>(f, 39);
        const DWORD bits = f_bits(f);
        if (bits == 24 || bits == 32) {
            const DWORD step = bits / 8;
            const BYTE *s = f_pixels(f);
            BYTE *d = f_pixels(f);
            if (reinterpret_cast<const BYTE *>(filter)[0x18]) {
                Ditherer dd;
                dither_ctor(&dd, vcall<DWORD>(f, 45), raw_float(0x3f000000), nullptr, nullptr);
                for (; n; n--, s += step, d += 2) {
                    const WORD p = (WORD)dither16(&dd, s);
                    memcpy(d, &p, 2);
                }
                dither_dtor(&dd);
            } else {
                for (; n; n--, s += step, d += 2) {
                    const WORD p = (WORD)pixel16(s[2], s[1], s[0]);
                    memcpy(d, &p, 2);
                }
            }
        }
    } else if (kind == 4) {
        const DWORD bits = f_bits(f);
        if (bits == 32) {
            convert_block(f, 46, 31, true, &px_to16<4>);
            convert_block(f, 50, 35, true, &px_to16<4>);
            row_call<RLE_ALPHA>(f, 49, 34, false, nullptr);
        } else if (bits == 24) {
            convert_block(f, 49, 34, true, &px_to16<3>);
            convert_block(f, 50, 35, true, &px_to16<3>);
            convert_block(f, 46, 31, true, &px_to16<3>);
        }
    }
    frame_set_depth(f, 16);
    return 1;
}

DWORD THISCALL FrameConvert_To24(FrameFilter *, CFrame *f, Palette *)
{
    const DWORD kind = f_flags(f) & F_COMPRESSED;
    if (kind == 0) {
        const DWORD width = vcall<DWORD>(f, 45);
        DWORD n = width * vcall<DWORD>(f, 39);
        if (f_bits(f) == 32) {
            const BYTE *first = f_pixels(f);
            const DWORD h = vcall<DWORD>(f, 39), w = vcall<DWORD>(f, 45);
            // the first pixel's alpha, tested w x h times
            for (DWORD y = 0; y < h; y++)
                for (DWORD x = 0; x < w; x++)
                    if (first[3] < 0xfa)
                        return 1;
            const BYTE *s = f_pixels(f);
            BYTE *d = f_pixels(f);
            for (; n; n--, s += 4, d += 3) {
                d[2] = s[2];
                d[1] = s[1];
                d[0] = s[0];
            }
        }
    } else if (kind == 4) {
        if (f_bits(f) == 32) {
            convert_block(f, 46, 31, true, &px_32to24);
            convert_block(f, 50, 35, true, &px_32to24);
            row_call<RLE_ALPHA>(f, 49, 34, false, nullptr);
        }
    }
    frame_set_depth(f, 24);
    return 1;
}

// ---- The filter object: 0x4acda0 (thiscall this; dither, depth), 0x4acdd0 deleting destructor, 0x4ae740
// Apply (frame, rect), 0x4ae7e0 Clone

FrameFilter *THISCALL FrameFilterConvert_Ctor(FrameFilter *f, BYTE dither, DWORD depth)
{
    fn<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1cc0)(f, 11);
    f->param = depth;
    f->vftable = vt(VT_FILTER_CONVERT);
    reinterpret_cast<BYTE *>(f)[0x18] = dither;
    return f;
}

FrameFilter *THISCALL FrameFilterConvert_DeletingDtor(FrameFilter *f, DWORD flags)
{
    fn<void(THISCALL *)(FrameFilter *)>(0x4b1d20)(f);
    if (flags & 1)
        pool_delete(f, 0x1c);
    return f;
}

DWORD THISCALL FrameFilterConvert_Apply(FrameFilter *filter, void *frame, const Rect *)
{
    CFrame *f = as_frame(frame);
    if (!f_pixels(f)) {
        frame_set_depth(f, filter->param);
        return 1;
    }
    const DWORD depth = filter->param;
    if (f_bits(f) == depth)
        return 1;
    Palette *inv = glob<Palette *>(INV_COLOR15_HOLDER);
    if (depth == 8)
        fn<DWORD(THISCALL *)(FrameFilter *, CFrame *, Palette *)>(0x4ad1e0)(filter, f, inv);
    else if (depth == 16)
        fn<DWORD(THISCALL *)(FrameFilter *, CFrame *, Palette *)>(0x4ad920)(filter, f, inv);
    else if (depth == 24)
        fn<DWORD(THISCALL *)(FrameFilter *, CFrame *, Palette *)>(0x4ae420)(filter, f, inv);
    return 1;
}

FrameFilter *THISCALL FrameFilterConvert_Clone(FrameFilter *filter)
{
    FrameFilter *c = static_cast<FrameFilter *>(pool_new(0x1c));
    if (!c)
        return nullptr;
    const BYTE dither = reinterpret_cast<BYTE *>(filter)[0x18];
    const DWORD depth = filter->param;
    fn<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1cc0)(c, 11);
    c->param = depth;
    reinterpret_cast<BYTE *>(c)[0x18] = dither;
    c->vftable = vt(VT_FILTER_CONVERT);
    return c;
}

}  // namespace

ZAX_HOOKS(frame_load_convert)
{
    const Hook list[] = {
        {0x4acda0, (void *)&FrameFilterConvert_Ctor, "FrameFilterConvert_Ctor", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x0b, 0xe8, 0x16, 0x4f}},
        {0x4acdd0, (void *)&FrameFilterConvert_DeletingDtor, "FrameFilterConvert_DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x48, 0x4f, 0x00, 0x00}, 4},
        {0x4ace00, (void *)&FrameConvert_PlainTo8, "FrameConvert_PlainTo8", 5, {0x83, 0xec, 0x54, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4ad1e0, (void *)&FrameConvert_To8, "FrameConvert_To8", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x4ad860, (void *)&FrameConvert_Nearest24, "FrameConvert_Nearest24", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56, 0x8b}},
        {0x4ad8c0, (void *)&FrameConvert_Nearest32, "FrameConvert_Nearest32", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56, 0x8b}},
        {0x4ad920, (void *)&FrameConvert_To16, "FrameConvert_To16", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x80, 0x00, 0x00}},
        {0x4ae3d0, (void *)&FrameConvert_Pixel16From32, "FrameConvert_Pixel16From32", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56, 0x8b}},
        {0x4ae420, (void *)&FrameConvert_To24, "FrameConvert_To24", 5, {0x83, 0xec, 0x14, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4ae740, (void *)&FrameFilterConvert_Apply, "FrameFilterConvert::Apply", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x57, 0x85}},
        {0x4ae7e0, (void *)&FrameFilterConvert_Clone, "FrameFilterConvert::Clone", 9, {0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x1c, 0x00, 0x00}},
        {0x4ae820, (void *)&FrameConvert_RleTo8From32, "FrameConvert_RleTo8From32", 5, {0x83, 0xec, 0x10, 0x85, 0xc9, 0x89, 0x4c, 0x24}},
        {0x4ae940, (void *)&FrameConvert_RleTo16From24, "FrameConvert_RleTo16From24", 5, {0x83, 0xec, 0x28, 0x85, 0xc9, 0x89, 0x4c, 0x24}},
        {0x4aea90, (void *)&FrameConvert_RleTo16From32, "FrameConvert_RleTo16From32", 5, {0x83, 0xec, 0x10, 0x85, 0xc9, 0x89, 0x4c, 0x24}},
        {0x4aeba0, (void *)&FrameConvert_RleAlphaFrom32, "FrameConvert_RleAlphaFrom32", 5, {0x83, 0xec, 0x08, 0x85, 0xc9, 0x89, 0x4c, 0x24}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
