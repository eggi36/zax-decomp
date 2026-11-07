// The Filter mode (CBlendFilter, object 0x6b9ca0; batch R5 of the blitter map, docs/DECOMPILATION.md 9): its two
// blitters and the resampler behind them, a static FilterState at 0x716120.
//
// [confirmed: code; compared in the running game]
//  - The blitters (8 -> 8 0x43bad0, 16 -> 16 0x43bd10) copy srcRect into a 16-bit stack frame (CFrame::Blit
//    with the Copy mode, with the screen's +0xd0 setting of [[0x6c02cc] + 4] switched off by 0x569520 around
//    it), resample that into a second 16-bit frame of dstRect's size and blit the result to dstRect.left/top
//    (Frame_BlitAt with Copy); the 8-bit one converts the result to 8 bits first (a depth filter (dither 1,
//    depth 8) run over it, FrameFilterList_Run). The resampling is skipped (the second frame stays black) when
//    dstRect is empty or larger than the frame it was made for. The opacity only sets g_OpacityTable.
//  - FilterState (0x60 bytes, a CFrame (the intermediate frame) and +0x40/+0x44 the weight lists of the rows
//    and columns ({count, entries} per output pixel, entries {source index, float weight}), +0x48/+0x54 their
//    scale, +0x4c/+0x58 the output and +0x50/+0x5c the input size) is built the first time the blitter
//    resamples (guard bit 1 of 0x716100, the constructor 0x43ad90 and atexit 0x43bf20). The constructor
//    fills three 512-entry tables for channel values -128..383: blue round(i * 31/255) clamped to 0..31
//    (0x6b9cd4), the same << 11 (red, 0x6bacd8), green round(i * 63/255) clamped to 0..63, << 5 (0x6ba4d4).
//  - Apply (0x43b650, thiscall (out, in)) resamples the rows into the state's frame (width out x height in),
//    then the columns into `out`; the weight lists are rebuilt when the scale (out / in as 80 bits against
//    the stored float) or the sizes changed. Each pixel is unpacked to three bytes (blue << 3, green << 2,
//    red & 0xf8 << 0 of the high byte), the weighted sums go to the tables at sum + 128.5 (truncated). The
//    column pass also keeps the minimum and maximum of red + 0.5 in 0x6b9c98 / 0x6bacd4 (unused elsewhere)
//    and steps through the state's frame and `out` by width * 2 bytes (their pitch, both made by SetSize).
//  - The weights (0x43ae70 rows, 0x43b260 columns, thiscall (in, out, scale)): the Mitchell-Netravali cubic
//    (B = C = 1/3: (7|x|^3 - 12x^2 + 16/3) / 6 below 1, (-7/3|x|^3 + 12x^2 - 20|x| + 32/3) / 6 below 2),
//    for output pixel i at the source position c = i / scale (a float), the taps from ceil(c - r) to
//    floor(c + r), r = 2 / scale (downscaling, x scaled by `scale` and the weight too) or 2, indices outside
//    the source mirrored (-j, 2n - j - 1). A downscaling list has room for floor(4 / scale + 1) taps, an
//    upscaling one for 5 (0x28 bytes).
#include "blitters.h"
#include "x87.h"

namespace zax {

namespace render::blit {

namespace {

constexpr DWORD FILTER_STATE = 0x716120, FILTER_GUARD = 0x716100;
constexpr DWORD T_BLUE = 0x6b9cd4, T_GREEN = 0x6ba4d4, T_RED = 0x6bacd8, RED_MIN = 0x6b9c98, RED_MAX = 0x6bacd4;
constexpr DWORD C_SEVEN = 0x5eeeac, C_M12 = 0x5eeea8, C_16_3 = 0x5eeea4, C_SIXTH = 0x5eeea0, C_M7_3 = 0x5eee98, C_TWELVE = 0x5ec3f8,
                C_M20 = 0x5eee94, C_32_3 = 0x5eee90, C_ONE_D = 0x5ee678, C_128_5 = 0x5eeeb0, C_31_255 = 0x5eee30, C_63_255 = 0x5eee28,
                C_31 = 0x5eee8c, C_63 = 0x5eee88;

struct Tap {
    int index;
    float weight;
};
struct TapList {
    DWORD count;
    Tap *taps;
};
struct FilterState {
    CFrame frame;          // 0x00
    TapList *rows;         // 0x40
    TapList *cols;         // 0x44
    float row_scale;       // 0x48
    DWORD row_out, row_in; // 0x4c, 0x50
    float col_scale;       // 0x54
    DWORD col_out, col_in; // 0x58, 0x5c
};
static_assert(sizeof(FilterState) == 0x60);

inline double crt_ceil(double v) { return fn<double(__cdecl *)(double)>(0x5d146a)(v); }
inline double crt_floor(double v) { return fn<double(__cdecl *)(double)>(0x5d139b)(v); }

// The cubic of t (80 bits), as the code evaluates it
inline long double kernel(long double t)
{
    const long double t2 = t * t;
    if (c0(t, cst(glob_addr(C_ZERO))))
        t = -t;
    if (c0(t, (long double)*glob<const double *>(C_ONE_D))) {
        const long double t3 = t * t2;
        return (t3 * cst(glob_addr(C_SEVEN)) + t2 * cst(glob_addr(C_M12)) + cst(glob_addr(C_16_3))) * cst(glob_addr(C_SIXTH));
    }
    if (c0(t, cst(glob_addr(C_TWO)))) {
        const long double a = t2 * t * cst(glob_addr(C_M7_3));
        const long double s = a + t2 * cst(glob_addr(C_TWELVE));
        return (s + t * cst(glob_addr(C_M20)) + cst(glob_addr(C_32_3))) * cst(glob_addr(C_SIXTH));
    }
    return cst(glob_addr(C_ZERO));
}

// 0x43ae70 / 0x43b260 (thiscall in, out, scale), ret 0xc: the weight lists at LIST with their bookkeeping
template <int LIST> void THISCALL BuildTaps(FilterState *s, DWORD n_in, DWORD n_out, float scale)
{
    TapList *&list = LIST == 0 ? s->rows : s->cols;
    DWORD &count = LIST == 0 ? s->row_out : s->col_out;
    for (DWORD i = 0; i < count; i++)
        if (list[i].taps) {
            heap_free(list[i].taps);
            list[i].taps = nullptr;
        }
    if (list) {
        heap_free(list);
        list = nullptr;
    }
    count = 0;
    list = static_cast<TapList *>(heap_alloc(n_out * 8));
    const float inv = fstp(cst(glob_addr(C_ONE)) / fld(scale));
    const bool down = c0(fld(scale), cst(glob_addr(C_ONE)));
    const float radius = down ? fstp(fld(inv) + fld(inv)) : 0.0f;
    for (DWORD i = 0; i < n_out; i++) {
        long double centre80;
        float centre;
        int lo, hi;
        if (down) {
            const int room = x87_ftol(fld(radius) + fld(radius) + cst(glob_addr(C_ONE)));
            list[i].count = 0;
            list[i].taps = static_cast<Tap *>(heap_alloc((DWORD)room * 8));
            centre80 = fild64((long long)i) * fld(inv);
            centre = fstp(centre80);
            const float c = fstp(crt_ceil((double)(centre80 - fld(radius))));
            const long double f = crt_floor((double)(fld(centre) + fld(radius)));
            lo = x87_ftol(fld(c));
            hi = x87_ftol(f);
        } else {
            list[i].count = 0;
            centre = fstp(fild64((long long)i) * fld(inv));
            list[i].taps = static_cast<Tap *>(heap_alloc(0x28));
            const float c = fstp(crt_ceil((double)(fld(centre) - cst(glob_addr(C_TWO)))));
            const long double f = crt_floor((double)(fld(centre) + cst(glob_addr(C_TWO))));
            lo = x87_ftol(fld(c));
            hi = x87_ftol(f);
        }
        for (int j = lo; j <= hi; j++) {
            long double t = fld(centre) - fild(j);
            if (down)
                t = t * fld(scale);
            long double w = kernel(t);
            if (down)
                w = w * fld(scale);
            int index = -j;
            if (j >= 0)
                index = j < (int)n_in ? j : (int)(n_in * 2) - j - 1;
            const DWORD k = list[i].count;
            list[i].count = k + 1;
            list[i].taps[k].index = index;
            list[i].taps[k].weight = fstp(w);
        }
    }
    count = n_out;
    (LIST == 0 ? s->row_scale : s->col_scale) = scale;
    (LIST == 0 ? s->row_in : s->col_in) = n_in;
}

// One pixel unpacked to blue << 3, green << 2, red (the high byte & 0xf8); the fourth byte is not written
inline void unpack(BYTE *b, DWORD p)
{
    b[2] = (BYTE)((p >> 8) & 0xf8);
    b[1] = (BYTE)((p >> 5) << 2);
    b[0] = (BYTE)(p << 3);
}

// The weighted sums of a tap list over the unpacked buffer
struct Sums {
    long double b, g, r;
};
inline Sums weigh(const TapList &l, const BYTE *buf)
{
    Sums s = {cst(glob_addr(C_ZERO)), cst(glob_addr(C_ZERO)), cst(glob_addr(C_ZERO))};
    for (DWORD k = 0; k < l.count; k++) {
        const BYTE *px = buf + l.taps[k].index * 4;
        const long double w = fld(l.taps[k].weight);
        s.r = s.r + fild(px[2]) * w;
        s.g = s.g + fild(px[1]) * w;
        s.b = s.b + fild(px[0]) * w;
    }
    return s;
}
inline WORD pack(const Sums &s)
{
    const int b = x87_ftol(s.b + cst(glob_addr(C_128_5)));
    const int g = x87_ftol(s.g + cst(glob_addr(C_128_5)));
    WORD v = (WORD)glob<const DWORD *>(T_BLUE)[b];
    v = (WORD)(v + (WORD)glob<const DWORD *>(T_GREEN)[g]);
    const int r = x87_ftol(s.r + cst(glob_addr(C_128_5)));
    return (WORD)(v + (WORD)glob<const DWORD *>(T_RED)[r]);
}

// 0x43b650 FilterState_Apply (thiscall out, in), ret 8
void THISCALL FilterState_Apply(FilterState *s, CFrame *out, CFrame *in)
{
    const DWORD w_out = sl(out, S_WIDTH), h_out = sl(out, S_HEIGHT);
    const DWORD w_in = sl(in, S_WIDTH), h_in = sl(in, S_HEIGHT);
    if (sl(&s->frame, S_WIDTH) != w_out || sl(&s->frame, S_HEIGHT) != h_in)
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(&s->frame, w_out, h_in, 0x10);
    const long double sx = fild64((long long)w_out) / fild((int)w_in);
    const float sx_f = fstp(sx);
    const float sy_f = fstp(fild64((long long)h_out) / fild((int)h_in));
    if (!c3(sx, fld(s->row_scale)) || w_out != s->row_out || w_in != s->row_in)
        BuildTaps<0>(s, w_in, w_out, sx_f);
    BYTE *buf = static_cast<BYTE *>(heap_alloc(w_in * 4));
    const DWORD *in_rows = sl_rows(in, S_ROWS);
    const DWORD *mid_rows = sl_rows(&s->frame, S_ROWS);
    BYTE *const in_pix = sl_ptr(in, S_PIXELS);
    BYTE *const mid_pix = sl_ptr(&s->frame, S_PIXELS);
    for (DWORD y = 0; y < h_in; y++) {
        const WORD *src = reinterpret_cast<const WORD *>(in_pix + in_rows[y] * 2);
        for (DWORD x = 0; x < w_in; x++)
            unpack(buf + x * 4, src[x]);
        WORD *dst = reinterpret_cast<WORD *>(mid_pix + mid_rows[y] * 2);
        for (DWORD x = 0; x < w_out; x++)
            dst[x] = pack(weigh(s->rows[x], buf));
    }
    if (buf)
        heap_free(buf);
    if (!c3(fld(sy_f), fld(s->col_scale)) || h_out != s->col_out || h_in != s->col_in)
        BuildTaps<1>(s, h_in, h_out, sy_f);
    buf = static_cast<BYTE *>(heap_alloc(h_in * 4));
    BYTE *const out_pix = sl_ptr(out, S_PIXELS);
    for (DWORD x = 0; x < w_out; x++) {
        const BYTE *col = mid_pix + x * 2;
        for (DWORD y = 0; y < h_in; y++, col += w_out * 2)
            unpack(buf + y * 4, *reinterpret_cast<const WORD *>(col));
        BYTE *dst = out_pix + x * 2;
        for (DWORD y = 0; y < h_out; y++, dst += w_out * 2) {
            const Sums sum = weigh(s->cols[y], buf);
            const long double red = sum.r + cst(glob_addr(C_HALF));
            if (!c0(fld(*glob<const float *>(RED_MIN)), red))
                *glob<float *>(RED_MIN) = fstp(red);
            if (c03(fld(*glob<const float *>(RED_MAX)), red))
                *glob<float *>(RED_MAX) = fstp(red);
            *reinterpret_cast<WORD *>(dst) = pack(sum);
        }
    }
    if (buf)
        heap_free(buf);
}

// 0x43ad90 FilterState_Ctor (thiscall): the frame, the lists empty and the three channel tables
FilterState *THISCALL FilterState_Ctor(FilterState *s)
{
    fn<void(THISCALL *)(void *)>(0x4a8a90)(&s->frame);
    s->rows = s->cols = nullptr;
    s->row_scale = raw_float(0);
    s->col_scale = raw_float(0);
    s->row_out = s->col_out = 0;
    for (int i = -128; i < 384; i++) {
        const long double v = fild(i);
        long double b = v * cst(glob_addr(C_31_255)) + cst(glob_addr(C_HALF));
        if (c0(b, cst(glob_addr(C_ZERO))))
            b = cst(glob_addr(C_ZERO));
        else if (!c03(b, cst(glob_addr(C_31))))
            b = cst(glob_addr(C_31));
        const int blue = x87_ftol(b);
        glob<DWORD *>(T_BLUE)[i + 128] = (DWORD)blue;
        glob<DWORD *>(T_RED)[i + 128] = (DWORD)blue << 11;
        long double g = v * cst(glob_addr(C_63_255)) + cst(glob_addr(C_HALF));
        if (c0(g, cst(glob_addr(C_ZERO))))
            g = cst(glob_addr(C_ZERO));
        else if (!c03(g, cst(glob_addr(C_63))))
            g = cst(glob_addr(C_63));
        glob<DWORD *>(T_GREEN)[i + 128] = (DWORD)x87_ftol(g) << 5;
    }
    return s;
}

// The screen setting the blit into the first frame switches off: [[0x6c02cc] + 4] + 0xd0, set by 0x569520
inline void *screen_object() { return *reinterpret_cast<void **>(*glob<BYTE **>(0x6c02cc) + 4); }

// 0x43bad0 / 0x43bd10 BlitFilter
template <bool TO8> void __fastcall BlitFilter(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    CFrame in, out;
    fn<void(THISCALL *)(void *)>(0x4a8a90)(&in);
    const int w = sr->right - sr->left, h = sr->bottom - sr->top;
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(&in, (DWORD)w, (DWORD)h, 0x10);
    const Rect to = {0, 0, w, h};
    const Rect from = *sr;
    void *const screen = screen_object();
    const DWORD saved = *reinterpret_cast<DWORD *>(static_cast<BYTE *>(screen) + 0xd0);
    fn<void(THISCALL *)(void *, DWORD)>(0x569520)(screen, 0);
    fn<void(THISCALL *)(void *, CFrame *, const Rect *, const Rect *, DWORD)>(0x4a8f40)(&in, src, &from, &to, glob_addr(COPY_MODE));
    fn<void(THISCALL *)(void *, DWORD)>(0x569520)(screen_object(), saved);
    fn<void(THISCALL *)(void *)>(0x4a8a90)(&out);
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(&out, (DWORD)(dr->right - dr->left), (DWORD)(dr->bottom - dr->top), 0x10);
    const DWORD ew = (DWORD)(dr->right - dr->left), eh = (DWORD)(dr->bottom - dr->top);
    if (!opacity_one(mode))
        set_opacity_table(mode);
    if (ew && eh && (int)ew <= (int)sl(&out, S_WIDTH) && (int)eh <= (int)sl(&out, S_HEIGHT)) {
        if (!(*glob<BYTE *>(FILTER_GUARD) & 1)) {
            *glob<BYTE *>(FILTER_GUARD) |= 1;
            fn<void *(THISCALL *)(void *)>(0x43ad90)(glob<void *>(FILTER_STATE));
            crt_atexit(code<void(__cdecl *)()>(0x43bf20));
        }
        fn<void(THISCALL *)(void *, CFrame *, CFrame *)>(0x43b650)(glob<void *>(FILTER_STATE), &out, &in);
    }
    DWORD convert[7];
    if (TO8) {
        fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x4acda0)(convert, 1, 8);
        fn<DWORD(THISCALL *)(void *, void *)>(0x4b21a0)(convert, &out);
    }
    fn<DWORD(THISCALL *)(CFrame *, CFrame *, int, int, DWORD)>(0x4b3280)(dst, &out, dr->left, dr->top, glob_addr(COPY_MODE));
    if (TO8)
        fn<void(THISCALL *)(void *)>(0x4b1d20)(convert);
    fn<void(THISCALL *)(void *)>(0x4a8b00)(&out);
    fn<void(THISCALL *)(void *)>(0x4a8b00)(&in);
}

}  // namespace

}  // namespace render::blit

using namespace render::blit;

ZAX_HOOKS(blitters_filter)
{
    const Hook list[] = {
#include "blitters_filter_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
