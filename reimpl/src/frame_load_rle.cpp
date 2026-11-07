// The RLE encoder of the frame load filters (kind 8, vtable 0x5f4924, +0x14: 0 for 8-bit, 1 for 16-bit
// frames; batch R7, docs/DECOMPILATION.md 9): 0x4aec50-0x4b1c90. Types in frame_load.h; the RLE format is
// that of zax-formats/docs/FORMATS.md 5.
//
// What it does [confirmed: code]: a loaded 32-bit frame (TGA with alpha) becomes a compressed frame whose
// blocks hold the pixels by their alpha:
//  - 8 bits (RleEncode_Frame8 0x4aef00): five alpha bands cut with RleEncode_AlphaBand 0x4aecd0 (a pixel
//    is kept when lo < alpha <= hi): above 229 the pixel block, 178-229 the edge block (+0x34), 127-178 the
//    alpha block (+0x2c), 76-127 the third block (+0x24), 25-76 the extra block (+0x1c); the first two
//    through the row encoder RleEncode_Pixels8 0x4b0d80 into a buffer of max(8 x width x height, 0x400)
//    bytes, the last two through RleEncode_Blocks8 0x4b0450. With a dithering conversion filter in the
//    list the opaque band is dithered to the game palette first (RleEncode_DitherToPalette 0x4aeda0).
//  - the encoded rows read the frame through the scratch frame 0x6be7e8 (a copy), the row width goes to
//    0x6be7e0, the bytes saved are added to 0x6be828.
#include "frame_load.h"

namespace zax {

using namespace render;

namespace {

constexpr DWORD VT_FILTER_RLE = 0x5f4924;
constexpr DWORD SCRATCH = 0x6be7e8, ROW_WIDTH = 0x6be7e0, BYTES_SAVED = 0x6be828;

inline CFrame *scratch() { return glob<CFrame *>(SCRATCH); }
inline void *exe_new(DWORD n) { return fn<void *(__cdecl *)(DWORD)>(0x5d033a)(n); }
inline void exe_delete(void *p) { fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
inline void frame_ctor(CFrame *f) { fn<void(THISCALL *)(CFrame *)>(0x4a8a90)(f); }
inline void frame_dtor(CFrame *f) { fn<void(THISCALL *)(CFrame *)>(0x4a8b00)(f); }
inline void frame_copy_from(CFrame *f, void *src) { fn<void(THISCALL *)(CFrame *, void *)>(0x4ab5d0)(f, src); }
inline bool any_pixel(CFrame *f) { return (BYTE)fn<DWORD(THISCALL *)(CFrame *)>(0x4ab9e0)(f) != 0; }
inline void row_table_free(DWORD **rows) { fn<void(THISCALL *)(DWORD **)>(0x4a8480)(rows); }
inline DWORD *row_table_alloc(DWORD w, DWORD h, DWORD flags) { return fn<DWORD *(__fastcall *)(DWORD, DWORD, DWORD)>(0x4a8440)(w, h, flags); }
inline void alpha_band(void *filter, CFrame *dst, void *src, DWORD lo, DWORD hi)
{
    fn<void(THISCALL *)(void *, CFrame *, void *, DWORD, DWORD)>(0x4aecd0)(filter, dst, src, lo, hi);
}

// ---- 0x4aec50 (thiscall this; 16-bit), 0x4aec70 deleting destructor, 0x4aecc0 the scratch frame's atexit

FrameFilter *THISCALL FrameFilterRle_Ctor(FrameFilter *f, DWORD rle16)
{
    fn<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1cc0)(f, 8);
    f->vftable = vt(VT_FILTER_RLE);
    f->param = rle16;
    return f;
}

FrameFilter *THISCALL FrameFilterRle_DeletingDtor(FrameFilter *f, DWORD flags)
{
    fn<void(THISCALL *)(FrameFilter *)>(0x4b1d20)(f);
    if (flags & 1)
        pool_delete(f, 0x18);
    return f;
}

void __cdecl StaticExit_FrameScratch() { frame_dtor(scratch()); }

// ---- 0x4aecd0 (thiscall filter; dst, src, lo, hi): dst made the size and depth of src, then every 32-bit
// pixel of src whose alpha lies in (lo, hi] copied, the others 0 (hi at most 255; lo 0 stays 0)

void THISCALL RleEncode_AlphaBand(void *, CFrame *dst, void *src, DWORD lo, DWORD hi)
{
    if (hi >= 0xff)
        hi = 0xff;
    const DWORD bits = f_bits(src), h = vcall<DWORD>(src, 39), w = vcall<DWORD>(src, 45);
    fn<void(THISCALL *)(CFrame *, DWORD, DWORD, DWORD)>(0x4ab5a0)(dst, w, h, bits);
    const BYTE *s = f_pixels(src);
    BYTE *d = f_pixels(dst);
    const DWORD sw = vcall<DWORD>(src, 45);
    for (DWORD n = sw * vcall<DWORD>(src, 39); n; n--, s += 4, d += 4) {
        const DWORD a = s[3];
        if (a > lo && a <= hi)
            memcpy(d, s, 4);
        else
            d[2] = d[1] = d[0] = d[3] = 0;
    }
}

// ---- 0x4aeda0 (thiscall frame): a 32-bit frame dithered to the game palette's colours in place

void THISCALL RleEncode_DitherToPalette(CFrame *f)
{
    alignas(8) BYTE ditherer[0x60];
    void *pal = fn<void *(__cdecl *)()>(0x433a80)();
    fn<void(THISCALL *)(void *, DWORD, float, void *, void *)>(0x4811b0)(ditherer, vcall<DWORD>(f, 45), raw_float(0x3f000000), pal,
                                                                       glob<void *>(INV_COLOR15_HOLDER));
    const DWORD h = vcall<DWORD>(f, 39), w = vcall<DWORD>(f, 45);
    const DWORD *rows = f_rows(f);
    BYTE *pixels = f_pixels(f);
    for (DWORD y = 0; y < h; y++) {
        BYTE *p = pixels + rows[y] * 4;
        for (DWORD x = 0; x < w; x++, p += 4) {
            const float b = fstp(fild(p[0]) * cst(glob_addr(0x5f172c))), g = fstp(fild(p[1]) * cst(glob_addr(0x5f172c))), r = fstp(fild(p[2]) * cst(glob_addr(0x5f172c)));
            const DWORD i = (BYTE)fn<DWORD(THISCALL *)(void *, float, float, float)>(0x481290)(ditherer, r, g, b) * 3u;
            p[2] = (*static_cast<BYTE **>(fn<void *(__cdecl *)()>(0x433a80)()))[i + 2];
            p[1] = (*static_cast<BYTE **>(fn<void *(__cdecl *)()>(0x433a80)()))[i + 1];
            p[0] = (*static_cast<BYTE **>(fn<void *(__cdecl *)()>(0x433a80)()))[i];
        }
    }
    fn<void(THISCALL *)(void *)>(0x481240)(ditherer);
}

// ---- The row encoders -------------------------------------------------------------------------------------------
// One row of the scratch frame (its width in 0x6be7e0) from x (*x, advanced) appended to the buffer: up to
// 0x7f clear pixels as a skip code; then the following non-clear pixels as runs (more than 8 equal pixels,
// at most 0x3f: the count and one pixel) and literals (0x40 | n and the n pixels; a literal ends before 8
// equal pixels or at 63); pixels are copied whole (4 bytes for 32-bit sources, so the 8/16-bit encoders
// leave the depth conversion to the conversion filter). "Equal" is the probe's: the nearest palette index
// (8-bit), the 5-6-5 pixel (16-bit), the byte or word itself (8- and 16-bit sources). A probe counts from its
// start pixel with the row's current x (not the probe position) as its bound.

inline DWORD row_width() { return *glob<DWORD *>(ROW_WIDTH); }
inline DWORD nearest_of(const BYTE *p) { return palette_nearest(glob<Palette *>(INV_COLOR15_HOLDER), p[2], p[1], p[0], 1, 0xff); }
inline WORD pixel16_of(const BYTE *p) { return (WORD)pixel16(p[2], p[1], p[0]); }

// 0x4b1aa0 (fastcall x, p; max): equal nearest indices from p, at most max
DWORD __fastcall RleEncode_RunLength8(DWORD x, const BYTE *p, DWORD max)
{
    DWORD n = 0;
    if (!max)
        return 0;
    for (const BYTE *q = p; x < row_width(); q += 4) {
        if (nearest_of(p) != nearest_of(q))
            break;
        n++;
        x++;
        if (n >= max)
            break;
    }
    return n;
}
// 0x4b1b60 (fastcall x, p; max): the 16-bit encoder's probe as compiled: the first pixel's index looked up
// twice, no run (0)
DWORD __fastcall RleEncode_RunLengthNone(DWORD x, const BYTE *p, DWORD max)
{
    if (max && x < row_width()) {
        nearest_of(p);
        nearest_of(p);
    }
    return 0;
}
// 0x4b1c00 (fastcall x, p; max): equal 5-6-5 pixels whose alpha differs from the first's only in the low
// three bits
DWORD __fastcall RleEncode_RunLength16(DWORD x, const BYTE *p, DWORD max)
{
    DWORD n = 0;
    if (!max)
        return 0;
    for (const BYTE *q = p; x < row_width(); q += 4) {
        const WORD a = pixel16_of(p);
        if (a != pixel16_of(q) || ((DWORD)p[3] ^ q[3]) & 0xfffffff8u)
            break;
        n++;
        x++;
        if (n >= max)
            break;
    }
    return n;
}
// 0x4b1b30, 0x4b1bd0, 0x4b1c90 (three copies; fastcall x, p; max): pixels with alpha from p, at most max
DWORD __fastcall RleEncode_OpaqueLength(DWORD x, const BYTE *p, DWORD max)
{
    DWORD n = 0;
    if (!max)
        return 0;
    for (const BYTE *a = p + 3; x < row_width() && *a; a += 4) {
        n++;
        x++;
        if (n >= max)
            break;
    }
    return n;
}
DWORD __fastcall RleEncode_OpaqueLength2(DWORD x, const BYTE *p, DWORD max) { return RleEncode_OpaqueLength(x, p, max); }
DWORD __fastcall RleEncode_OpaqueLength3(DWORD x, const BYTE *p, DWORD max) { return RleEncode_OpaqueLength(x, p, max); }

using Probe = DWORD(__fastcall *)(DWORD x, const BYTE *p, DWORD max);

// The pixel kinds: bytes per pixel, "clear", the probe
template <int N> inline bool is_clear(const BYTE *p)
{
    if constexpr (N == 4)
        return !p[3];
    else if constexpr (N == 2)
        return !(p[0] | p[1]);
    else
        return !p[0];
}
template <int N> DWORD __fastcall probe_raw(DWORD x, const BYTE *p, DWORD max)
{
    DWORD n = 0;
    if (!max)
        return 0;
    for (const BYTE *q = p; x < row_width() && !memcmp(p, q, N); q += N) {
        n++;
        x++;
        if (n >= max)
            break;
    }
    return n;
}
DWORD __fastcall probe_565(DWORD x, const BYTE *p, DWORD max)
{
    DWORD n = 0;
    if (!max)
        return 0;
    for (const BYTE *q = p; x < row_width() && pixel16_of(p) == pixel16_of(q); q += 4) {
        n++;
        x++;
        if (n >= max)
            break;
    }
    return n;
}

template <int N> void encode_row(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, Probe probe)
{
    while (*x < row_width()) {
        DWORD c = 0;
        for (const BYTE *p = src; *x + c < row_width() && is_clear<N>(p); p += N)
            if (++c >= 0x7f)
                break;
        if (c) {
            src += c * N;
            *x += c;
            buf[(*size)++] = (BYTE)(c | 0x80);
        }
        DWORD n = 0;
        const DWORD lim = row_width() - *x;
        if (lim)
            for (DWORD xx = *x; xx < row_width() && !is_clear<N>(src + n * N); xx++)
                if (++n >= lim)
                    break;
        const BYTE *end = src + n * N;
        while (src < end) {
            const int rem = (int)(end - src) / N;
            const DWORD count = probe(*x, src, rem < 0x3f ? (DWORD)rem : 0x3f);
            if (count > 8) {
                buf[(*size)++] = (BYTE)count;
                memcpy(buf + *size, src, N);
                *size += N;
                src += count * N;
                *x += count;
                continue;
            }
            const BYTE *q = src;
            DWORD k = 0;
            int left = rem;
            while (q < end) {
                if ((int)(q - src) / N >= 0x3f || k >= 8)
                    break;
                left--;
                q += N;
                k = probe(*x, q, left < 8 ? (DWORD)left : 8);
            }
            const DWORD lit = (DWORD)(q - src) / N;
            buf[(*size)++] = (BYTE)(lit | 0x40);
            memcpy(buf + *size, src, lit * N);
            src += lit * N;
            *x += lit;
            *size += lit * N;
        }
    }
}

// 0x4b0d80, 0x4b10c0 (identical), 0x4b1400, 0x4b16d0, 0x4b18b0 (fastcall row, &x; buffer, &size, capacity
// (unused)): 32-bit rows by nearest index, 32-bit rows by 5-6-5 pixel, 8-bit rows, 16-bit rows
void __fastcall RleEncode_Pixels8(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<4>(src, x, buf, size, &RleEncode_RunLength8);
}
void __fastcall RleEncode_Pixels8b(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<4>(src, x, buf, size, &RleEncode_RunLength8);
}
void __fastcall RleEncode_Pixels16(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<4>(src, x, buf, size, &probe_565);
}
void __fastcall RleEncode_Bytes(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<1>(src, x, buf, size, &probe_raw<1>);
}
void __fastcall RleEncode_Words(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<2>(src, x, buf, size, &probe_raw<2>);
}
// The rows of the block encoders (their probes are the helper functions above)
void __fastcall rows_blocks16(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<4>(src, x, buf, size, &RleEncode_RunLengthNone);
}
void __fastcall rows_blocks24(const BYTE *src, DWORD *x, BYTE *buf, DWORD *size, DWORD)
{
    encode_row<4>(src, x, buf, size, &RleEncode_RunLength16);
}

// The pixel block of `f` (a CFrame on the stack or the frame itself) encoded by the row encoder `encode`
// (fastcall (src, &state; buffer, &size, capacity)) through the scratch frame; the frame's flags get
// `rle` (0x2002 or 0x2004), a new row table, the encoded block (the old pixels freed). The buffer holds
// `cap_shift` bytes per pixel (at least 0x400), a source row starts at its row offset times `bpp`, the
// bytes saved count width x height << `saved_shift` minus the block.
using RowEncoder = void(__fastcall *)(const BYTE *, DWORD *, BYTE *, DWORD *, DWORD);
void encode_pixels(CFrame *f, RowEncoder encode, DWORD rle = 0x2002, int cap_shift = 3, DWORD bpp = 4, int saved_shift = 2)
{
    DWORD size = 4;
    const DWORD fw = vcall<DWORD>(f, 45);
    DWORD cap = fw * vcall<DWORD>(f, 39) << cap_shift;
    if (cap <= 0x400)
        cap = 0x400;
    BYTE *buf = static_cast<BYTE *>(exe_new(cap));
    frame_copy_from(scratch(), f);
    f->flags |= rle;
    row_table_free(&f->rows);
    const DWORD keep = f_flags(f) & 0x2000;
    const DWORD h = vcall<DWORD>(f, 39);
    DWORD *table = row_table_alloc(vcall<DWORD>(f, 45), h, keep);
    f->rows = table;
    const BYTE *spix = scratch()->pixels;
    *glob<DWORD *>(ROW_WIDTH) = scratch()->width;
    const DWORD *srows = scratch()->rows;
    for (DWORD y = 0; y < vcall<DWORD>(f, 39); y++) {
        DWORD state = 0;
        table[y] = size;
        encode(spix + srows[y] * bpp, &state, buf, &size, cap);
    }
    const DWORD sw = vcall<DWORD>(f, 45);
    const DWORD total = sw * vcall<DWORD>(f, 39) << saved_shift;
    *glob<DWORD *>(BYTES_SAVED) += total - size;
    if (f->pixels) {
        exe_delete(f->pixels);
        f->pixels = nullptr;
    }
    *reinterpret_cast<DWORD *>(buf) = size;
    BYTE *block = static_cast<BYTE *>(exe_new(size));
    memcpy(block, buf, size);
    f->pixels = block;
    exe_delete(buf);
}

// A band frame's encoded pixel block moved into the frame's block `b` (data, table at blocks[2b], [2b+1])
void take_block(CFrame *f, CFrame *band, int b)
{
    f->blocks[2 * b] = band->pixels;
    f->blocks[2 * b + 1] = band->rows;
    band->pixels = nullptr;
    band->rows = nullptr;
}

// ---- 0x4aef00 (thiscall filter; frame): the 8-bit encoder (header comment); al 0 for an already compressed
// frame, else 1

DWORD THISCALL RleEncode_Frame8(FrameFilter *filter, CFrame *f)
{
    if (f_flags(f) & F_COMPRESSED)
        return 0;
    CFrame copy, band1, band2, band3, band4;
    frame_ctor(&copy);
    frame_copy_from(&copy, f);
    alpha_band(filter, f, &copy, 0xe5, 0x118);
    frame_ctor(&band1);
    alpha_band(filter, &band1, &copy, 0xb2, 0xe5);
    frame_ctor(&band2);
    alpha_band(filter, &band2, &copy, 0x7f, 0xb2);
    frame_ctor(&band3);
    alpha_band(filter, &band3, &copy, 0x4c, 0x7f);
    frame_ctor(&band4);
    alpha_band(filter, &band4, &copy, 0x19, 0x4c);
    const BYTE *convert = fn<const BYTE *(THISCALL *)(FrameFilter *, DWORD)>(0x4b2140)(filter, 11);
    if (convert && convert[0x18])
        fn<void(THISCALL *)(CFrame *)>(0x4aeda0)(f);
    encode_pixels(f, code<RowEncoder>(0x4b0d80));
    if (any_pixel(&band1)) {
        encode_pixels(&band1, code<RowEncoder>(0x4b0d80));
        take_block(f, &band1, 3);   // edge block +0x34/+0x38
    } else {
        f->blocks[6] = f->blocks[7] = nullptr;
    }
    if (any_pixel(&band2)) {
        encode_pixels(&band2, code<RowEncoder>(0x4b0d80));
        take_block(f, &band2, 2);   // alpha block +0x2c/+0x30
    } else {
        f->blocks[4] = f->blocks[5] = nullptr;
    }
    if (any_pixel(&band3)) {
        fn<void(THISCALL *)(CFrame *)>(0x4b0450)(&band3);
        take_block(f, &band3, 1);   // third block +0x24/+0x28
    } else {
        f->blocks[2] = f->blocks[3] = nullptr;
    }
    if (any_pixel(&band4)) {
        fn<void(THISCALL *)(CFrame *)>(0x4b0450)(&band4);
        take_block(f, &band4, 0);   // extra block +0x1c/+0x20
    } else {
        f->blocks[0] = f->blocks[1] = nullptr;
    }
    frame_dtor(&band4);
    frame_dtor(&band3);
    frame_dtor(&band2);
    frame_dtor(&band1);
    frame_dtor(&copy);
    return 1;
}

// ---- 0x4afc00 (fastcall row, width; ditherer): a row of 32-bit pixels dithered to the 5-6-5 levels in place
// (the ditherer's error added, then each channel quantised to 31 or 63 steps until it is satisfied)

void __fastcall RleEncode_DitherRow565(BYTE *p, DWORD n, void *ditherer)
{
    const long double inv = cst(glob_addr(0x5f172c)), k31 = cst(glob_addr(0x5eee8c)), q31 = cst(glob_addr(0x5f4920)), k63 = cst(glob_addr(0x5eee88)), q63 = cst(glob_addr(0x5f4930)),
                      c255 = cst(glob_addr(0x5eb728));
    for (; n; n--, p += 4) {
        float b = fstp(fild(p[0]) * inv), g = fstp(fild(p[1]) * inv), r = fstp(fild(p[2]) * inv);
        fn<void(THISCALL *)(void *, float *, float *, float *)>(0x481380)(ditherer, &r, &g, &b);
        do {
            r = fstp(fild(x87_ftol(fld(r) * k31)) * q31);
            g = fstp(fild(x87_ftol(fld(g) * k63)) * q63);
            b = fstp(fild(x87_ftol(fld(b) * k31)) * q31);
        } while ((BYTE)fn<DWORD(THISCALL *)(void *, float *, float *, float *)>(0x4814c0)(ditherer, &r, &g, &b));
        const int ri = x87_fistp(fld(fstp(fld(r) * c255))), gi = x87_fistp(fld(fstp(fld(g) * c255))),
                  bi = x87_fistp(fld(fstp(fld(b) * c255)));
        p[2] = (BYTE)ri;
        p[1] = (BYTE)gi;
        p[0] = (BYTE)bi;
    }
}

// ---- 0x4af690 (thiscall filter; frame): the 16-bit encoder: alpha above 250 the pixel block, 5-250 the edge
// and the alpha block (two copies of the band, each through the band encoder: RleEncode_Blocks16 0x4b0760 for
// an 8-bit target, RleEncode_Blocks24 0x4b0a70 for a 16-bit one); the pixel block through
// RleEncode_Pixels8b 0x4b10c0 (target 8, dithered to the palette first when the conversion dithers) or
// RleEncode_Pixels16 0x4b1400 (target 16, each row dithered to 5-6-5 first when it dithers); other targets
// leave the pixel block as it is. The third and extra blocks are cleared.

DWORD THISCALL RleEncode_Frame16(FrameFilter *filter, CFrame *f)
{
    if (f_flags(f) & F_COMPRESSED)
        return 0;
    CFrame copy, band;
    frame_ctor(&copy);
    frame_copy_from(&copy, f);
    alpha_band(filter, f, &copy, 0xfa, 0xff);
    frame_ctor(&band);
    alpha_band(filter, &band, &copy, 5, 0xfa);
    const BYTE *convert = fn<const BYTE *(THISCALL *)(FrameFilter *, DWORD)>(0x4b2140)(filter, 11);
    const bool dither = convert && convert[0x18];
    const int depth = fn<int(THISCALL *)(FrameFilter *)>(0x4b2160)(filter);
    if (depth == 8) {
        if (dither)
            fn<void(THISCALL *)(CFrame *)>(0x4aeda0)(f);
        encode_pixels(f, code<RowEncoder>(0x4b10c0), 0x2004);
    } else if (depth == 16) {
        if (dither) {
            alignas(8) BYTE ditherer[0x60];
            fn<void(THISCALL *)(void *, DWORD, float, void *, void *)>(0x4811b0)(ditherer, vcall<DWORD>(f, 45), raw_float(0x42c9999a), nullptr,
                                                                               nullptr);
            const DWORD h = vcall<DWORD>(f, 39), w = vcall<DWORD>(f, 45);
            const DWORD *rows = f_rows(f);
            BYTE *pixels = f_pixels(f);
            for (DWORD y = 0; y < h; y++)
                fn<void(__fastcall *)(BYTE *, DWORD, void *)>(0x4afc00)(pixels + rows[y] * 4, w, ditherer);
            fn<void(THISCALL *)(void *)>(0x481240)(ditherer);
        }
        encode_pixels(f, code<RowEncoder>(0x4b1400), 0x2004);
    }
    if (any_pixel(&band)) {
        CFrame second;
        frame_ctor(&second);
        frame_copy_from(&second, &band);
        if (depth == 8)
            fn<void(THISCALL *)(CFrame *)>(0x4b0760)(&band);
        else if (depth == 16)
            fn<void(THISCALL *)(CFrame *)>(0x4b0a70)(&band);
        take_block(f, &band, 3);
        if (depth == 8)
            fn<void(THISCALL *)(CFrame *)>(0x4b0760)(&second);
        else if (depth == 16)
            fn<void(THISCALL *)(CFrame *)>(0x4b0a70)(&second);
        take_block(f, &second, 2);
        frame_dtor(&second);
    } else {
        f->blocks[6] = f->blocks[7] = nullptr;
        f->blocks[4] = f->blocks[5] = nullptr;
    }
    f->blocks[2] = f->blocks[3] = nullptr;
    f->blocks[0] = f->blocks[1] = nullptr;
    frame_dtor(&band);
    frame_dtor(&copy);
    return 1;
}

// ---- 0x4afd40 Apply (thiscall filter; frame, rect): nothing while 0x613d74 is clear or the frame has no
// pixels; a compressed frame gives 0. By depth: 8 bits encoded as is through RleEncode_Edge 0x4b16d0 (1 byte
// per pixel, flags 0x2002), 16 bits through RleEncode_Alpha 0x4b18b0 (flags 0x2004); 24 bits first blitted
// into a 32-bit frame (its clip rectangle, black pixels with alpha 0) that replaces the frame, then like 32
// bits: the 8-bit encoder (+0x14 = 0) or the 16-bit one (+0x14 = 1; a 24-bit source at target 16 always the
// 16-bit one). The origin is put back at the end.

DWORD THISCALL FrameFilterRle_Apply(FrameFilter *filter, void *frame, const Rect *)
{
    if (!*glob<BYTE *>(0x613d74))
        return 1;
    CFrame *f = as_frame(frame);
    if (!f_pixels(f))
        return 1;
    BYTE result = 0;
    if (f_flags(f) & F_COMPRESSED)
        return result;
    const int x = vcall<int>(f, 43), y = vcall<int>(f, 44);
    switch (f_flags(f) & F_DEPTH) {
    case F_8:
        encode_pixels(f, code<RowEncoder>(0x4b16d0), 0x2002, 1, 1, 0);
        result = 1;
        break;
    case F_16:
        encode_pixels(f, code<RowEncoder>(0x4b18b0), 0x2004, 2, 2, 1);
        result = 1;
        break;
    case F_24: {
        CFrame *src = as_frame(frame);
        CFrame temp;
        frame_ctor(&temp);
        Rect r;
        f_clip_rect(src, &r);
        const DWORD w = (DWORD)(r.right - r.left), h = (DWORD)(r.bottom - r.top);
        const DWORD a = fn<DWORD(THISCALL *)(CFrame *, DWORD, DWORD, DWORD)>(0x4aa5f0)(&temp, w, h, 0x20);
        fn<void(THISCALL *)(CFrame *, DWORD, DWORD)>(0x4aa2c0)(&temp, a, 1);
        Rect dst = {0, 0, r.right - r.left, r.bottom - r.top};
        const int ny = -vcall<int>(src, 44);
        const int nx = -vcall<int>(src, 43);
        Rect sr = {r.left + nx, r.top + ny, r.right + nx, r.bottom + ny};
        fn<void(THISCALL *)(CFrame *, void *, Rect *, Rect *, void *)>(0x4a8f10)(&temp, src, &sr, &dst, glob<void *>(COPY_MODE));
        DWORD *p = reinterpret_cast<DWORD *>(temp.pixels);
        for (DWORD n = (DWORD)temp.width * temp.height; n; n--, p++)
            if (!(*p & 0xffffff))
                *p = 0;
        const int oy = vcall<int>(src, 44);
        fn<void(THISCALL *)(CFrame *, int, int)>(0x4abaa0)(&temp, vcall<int>(src, 43), oy);
        frame_copy_from(f, &temp);
        const int depth = fn<int(THISCALL *)(FrameFilter *)>(0x4b2160)(filter);
        if (depth == 8) {
            if (filter->param == 0)
                result = (BYTE)fn<DWORD(THISCALL *)(FrameFilter *, CFrame *)>(0x4aef00)(filter, f);
            else if (filter->param == 1)
                result = (BYTE)fn<DWORD(THISCALL *)(FrameFilter *, CFrame *)>(0x4af690)(filter, f);
        } else if (depth == 16) {
            result = (BYTE)fn<DWORD(THISCALL *)(FrameFilter *, CFrame *)>(0x4af690)(filter, f);
        }
        frame_dtor(&temp);
        break;
    }
    case F_32:
        if (filter->param == 0)
            result = (BYTE)fn<DWORD(THISCALL *)(FrameFilter *, CFrame *)>(0x4aef00)(filter, f);
        else if (filter->param == 1)
            result = (BYTE)fn<DWORD(THISCALL *)(FrameFilter *, CFrame *)>(0x4af690)(filter, f);
        break;
    }
    f->x = (short)x;
    f->y = (short)y;
    return result;
}

// 0x4b0410: slot 1, a kind 8 filter with the same +0x14
FrameFilter *THISCALL FrameFilterRle_Clone(FrameFilter *filter)
{
    FrameFilter *c = static_cast<FrameFilter *>(pool_new(0x18));
    if (!c)
        return nullptr;
    const DWORD rle16 = filter->param;
    fn<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1cc0)(c, 8);
    c->param = rle16;
    c->vftable = vt(VT_FILTER_RLE);
    return c;
}

// ---- 0x4b0450, 0x4b0760, 0x4b0a70 (thiscall frame): a band frame's pixel block encoded with the probes
// RleEncode_RunLength8 (flags 0x2002), RleEncode_RunLengthNone (only literals; 0x2004) and
// RleEncode_RunLength16 (0x2004); al 1

DWORD THISCALL RleEncode_Blocks8(CFrame *f)
{
    encode_pixels(f, &RleEncode_Pixels8);
    return 1;
}
DWORD THISCALL RleEncode_Blocks16(CFrame *f)
{
    encode_pixels(f, &rows_blocks16, 0x2004);
    return 1;
}
DWORD THISCALL RleEncode_Blocks24(CFrame *f)
{
    encode_pixels(f, &rows_blocks24, 0x2004);
    return 1;
}

}  // namespace

ZAX_HOOKS(frame_load_rle)
{
    const Hook list[] = {
        {0x4aec50, (void *)&FrameFilterRle_Ctor, "FrameFilterRle_Ctor", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x08, 0xe8, 0x66, 0x30}},
        {0x4aec70, (void *)&FrameFilterRle_DeletingDtor, "FrameFilterRle_DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xa8, 0x30, 0x00, 0x00}, 4},
        {0x4aecc0, (void *)&StaticExit_FrameScratch, "StaticExit_FrameScratch", 5, {0xb9, 0xe8, 0xe7, 0x6b, 0x00, 0xe9, 0x36, 0x9e}},
        {0x4aecd0, (void *)&RleEncode_AlphaBand, "RleEncode_AlphaBand", 6, {0x8b, 0x44, 0x24, 0x0c, 0x85, 0xc0, 0x77, 0x08}},
        {0x4aeda0, (void *)&RleEncode_DitherToPalette, "RleEncode_DitherToPalette", 5, {0x83, 0xec, 0x68, 0x53, 0x56, 0x57, 0x8b, 0xf1}},
        {0x4aef00, (void *)&RleEncode_Frame8, "RleEncode_Frame8", 6, {0x81, 0xec, 0x54, 0x01, 0x00, 0x00, 0x55, 0x8b}},
        {0x4af690, (void *)&RleEncode_Frame16, "RleEncode_Frame16", 6, {0x81, 0xec, 0xe0, 0x00, 0x00, 0x00, 0x55, 0x8b}},
        {0x4afc00, (void *)&RleEncode_DitherRow565, "RleEncode_DitherRow565", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x8b, 0xc2}},
        {0x4afd40, (void *)&FrameFilterRle_Apply, "FrameFilterRle::Apply", 5, {0xa0, 0x74, 0x3d, 0x61, 0x00, 0x81, 0xec, 0x90}},
        {0x4b0410, (void *)&FrameFilterRle_Clone, "FrameFilterRle::Clone", 9, {0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x18, 0x00, 0x00}},
        {0x4b0450, (void *)&RleEncode_Blocks8, "RleEncode_Blocks8", 6, {0x83, 0xec, 0x24, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x4b0760, (void *)&RleEncode_Blocks16, "RleEncode_Blocks16", 6, {0x83, 0xec, 0x24, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x4b0a70, (void *)&RleEncode_Blocks24, "RleEncode_Blocks24", 6, {0x83, 0xec, 0x24, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x4b0d80, (void *)&RleEncode_Pixels8, "RleEncode_Pixels8", 5, {0x83, 0xec, 0x1c, 0x53, 0x56, 0x8b, 0x35, 0xe0}},
        {0x4b10c0, (void *)&RleEncode_Pixels8b, "RleEncode_Pixels8b", 5, {0x83, 0xec, 0x1c, 0x53, 0x56, 0x8b, 0x35, 0xe0}},
        {0x4b1400, (void *)&RleEncode_Pixels16, "RleEncode_Pixels16", 10, {0x83, 0xec, 0x18, 0x53, 0x8b, 0x1d, 0xe0, 0xe7}},
        {0x4b16d0, (void *)&RleEncode_Bytes, "RleEncode_Bytes", 8, {0x83, 0xec, 0x10, 0xa1, 0xe0, 0xe7, 0x6b, 0x00}},
        {0x4b18b0, (void *)&RleEncode_Words, "RleEncode_Words", 8, {0x83, 0xec, 0x10, 0xa1, 0xe0, 0xe7, 0x6b, 0x00}},
        {0x4b1aa0, (void *)&RleEncode_RunLength8, "RleEncode_RunLength8", 5, {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56}},
        {0x4b1b30, (void *)&RleEncode_OpaqueLength, "RleEncode_OpaqueLength", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x33, 0xc0, 0x85}},
        {0x4b1b60, (void *)&RleEncode_RunLengthNone, "RleEncode_RunLengthNone", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x85, 0xc0, 0x8b}},
        {0x4b1bd0, (void *)&RleEncode_OpaqueLength2, "RleEncode_OpaqueLength2", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x33, 0xc0, 0x85}},
        {0x4b1c00, (void *)&RleEncode_RunLength16, "RleEncode_RunLength16", 5, {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56}},
        {0x4b1c90, (void *)&RleEncode_OpaqueLength3, "RleEncode_OpaqueLength3", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x33, 0xc0, 0x85}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
