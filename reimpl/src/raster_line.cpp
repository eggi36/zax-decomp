// The line span rasterizer of CFrame slot 52 (batch R7 of the blitter map, docs/DECOMPILATION.md 9):
// CFrame_DrawLineSpans 0x42b160 and the three per-depth rasterizers 0x42b6e0 (8 bits), 0x42c0f0 (16)
// and 0x42c9e0 (24), plus the table builder 0x42b320 and its free 0x42b6a0 (types in frame.h).
//
// What it draws [confirmed: code]: an antialiased line of one pixel (Wu's algorithm). The end points are
// floats; 0x42b160 makes them 16.16 fixed point relative to the frame's origin, clips them with the frame's
// rectangle (slot 36, which includes the origin) shifted to 16.16 (Line_ClipToRect 0x4b2d30) and calls the
// rasterizer for the depth (32 bits draw nothing). The rasterizer steps the major axis one pixel at a time
// and the minor one by a 16.16 slope (Platform_MulDivShift 0x4d2620, MulDiv(d_minor, 0x10000, |d_major|),
// made one smaller in magnitude when |d_major >> 16| * slope would overshoot d_minor) and blends two pixels
// per step across the minor axis: the first with 255 - f, the second (one row down, or one pixel right)
// with f, f the minor coordinate's fraction (bits 8-15). The 8-bit rasterizer gets the end points with
// their fractions cut off.
//  - 8 bits: the palette colours' channels (tables 0x62eb18 blue, 0x675fb8 green, 0x674fb0 red, per index)
//    blended as c = p + (C - p) * w / 256 (truncated towards zero), then the index through the 5-5-5
//    inverse colour table (0x633388) of the 5-5-5 pixel (tables 0x630378, 0x6753b8, 0x633b94).
//  - 16 bits (5-6-5): green with w >> 2 and a logical shift right by 1, red and blue with w >> 3, on the
//    differences as unsigned 32-bit values, each masked into its field.
//  - 24 bits: every byte c = p + (C - p) * w / 256 (truncated).
// Quirks kept: when both end points' major coordinates (+2) lie inside the frame the rasterizer draws
// without clipping (the x-major paths clamp a negative row to 0, the steep ones do not clamp); otherwise it
// tests every pixel against slot 36's rectangle, which includes the origin while the coordinates do not.
// The 16-bit red value keeps the colour argument's upper word (0 from the only caller).
#include "frame.h"

namespace zax {

using namespace render;

namespace {

// The palette's channels per index and the 5-5-5 inverse colour table (CPaletteHolder 0x633388, the data
// pointer at +0)
constexpr DWORD PAL_BLUE = 0x62eb18, PAL_GREEN = 0x675fb8, PAL_RED = 0x674fb0, INV_COLOR15 = 0x633388;
// SpanTables_Init's globals
constexpr DWORD SPAN_SHIFT = 0x62ea30, SPAN_LIMIT = 0x62ea34, SPAN_SHIFT2 = 0x62ea38, SPAN_SQRT = 0x62ea3c,
                SPAN_COVER = 0x62ea40, SPAN_BUILT = 0x62ea44, SPAN_COVER_SIZE = 0x62ea48;

inline int muldiv_shift(int a, int b, int shift) { return fn<int(__fastcall *)(int, int, int)>(0x4d2620)(a, b, shift); }
inline int iabs(int v) { return v < 0 ? (int)(0u - (DWORD)v) : v; }
// cdq; and edx, 0xff; add eax, edx; sar eax, 8: v / 256 truncated towards zero
inline int div256(int v) { return (int)((DWORD)v + ((DWORD)(v >> 31) & 0xff)) >> 8; }
inline DWORD tab(DWORD table, DWORD i) { return at<const DWORD *>(table)[i]; }

// The slope of the minor axis per major pixel, 16.16
int line_slope(int d_minor, int d_major)
{
    int slope = muldiv_shift(d_minor, iabs(d_major), 16);
    if (iabs((int)((DWORD)(d_major >> 16) * (DWORD)slope)) > iabs(d_minor))
        slope += slope < 0 ? 1 : -1;
    return slope;
}

struct Blend8 {
    static constexpr int BYTES = 1;
    DWORD color;   // the palette index
    explicit Blend8(DWORD c) : color(c & 0xff) {}
    static int channel(DWORD table, DWORD color, DWORD i, int w)
    {
        return div256((int)(tab(table, color) - tab(table, i)) * w) + (int)tab(table, i);
    }
    void operator()(BYTE *p, int w) const
    {
        const DWORD i = *p;
        const int b = channel(glob_addr(PAL_BLUE), color, i, w), g = channel(glob_addr(PAL_GREEN), color, i, w), r = channel(glob_addr(PAL_RED), color, i, w);
        const DWORD k = tab(glob_addr(PIXEL16_BLUE), (DWORD)b) + tab(glob_addr(PIXEL15_GREEN), (DWORD)g) + tab(glob_addr(PIXEL15_RED), (DWORD)r);
        *p = (*glob<BYTE **>(INV_COLOR15))[k];
    }
};

struct Blend16 {
    static constexpr int BYTES = 2;
    DWORD r, g, b;
    explicit Blend16(DWORD c) : r((c & 0xffff0000u) | ((c & 0xffff) >> 11)), g((c & 0xffff) >> 5 & 0x3f), b(c & 0x1f) {}
    void operator()(BYTE *at_p, int w) const
    {
        WORD *pp = reinterpret_cast<WORD *>(at_p);
        const DWORD p = *pp, wg = (DWORD)w >> 2, wrb = wg >> 1;
        const DWORD gg = ((g - (p >> 5 & 0x3f)) * wg >> 1) + (p & 0xffe0);
        const DWORD rr = ((r - (p >> 11)) * wrb << 6) + (p & 0xf83f);
        const DWORD bb = (b - (p & 0x1f)) * wrb >> 5;
        *pp = (WORD)((p & 0x1f) + bb + (gg & 0x7e0) + (rr & 0xf800));
    }
};

struct Blend24 {
    static constexpr int BYTES = 3;
    int b, g, r;
    explicit Blend24(DWORD c) : b((int)(c & 0xff)), g((int)(c >> 8 & 0xff)), r((int)(c >> 16 & 0xff)) {}
    void operator()(BYTE *p, int w) const
    {
        const int pb = p[0], pg = p[1], pr = p[2];
        p[2] = (BYTE)(div256((r - pr) * w) + pr);
        p[1] = (BYTE)(div256((g - pg) * w) + pg);
        p[0] = (BYTE)(div256((b - pb) * w) + pb);
    }
};

inline bool inside(const Rect &c, int x, int y) { return x >= c.left && y >= c.top && x < c.right && y < c.bottom; }

// The rasterizer of one depth (fastcall frame, x1; y1, x2, y2, colour; ret 0x10), coordinates 16.16
template <typename B> void raster(CFrame *f, int x1, int y1, int x2, int y2, DWORD color)
{
    constexpr int N = B::BYTES;
    BYTE *const pixels = f_pixels(f);
    int dy = y2 - y1, dx = x2 - x1;
    const B blend(color);
    if (iabs(dx) >= iabs(dy)) {
        // x-major: x as integers, y 16.16
        int xa = x1 >> 16, xb = x2 >> 16;
        if (dx < 0) {
            const int t = xa;
            xa = xb;
            xb = t;
            const int u = y1;
            y1 = y2;
            y2 = u;
            dx = -dx;
            dy = -dy;
        }
        const int slope = line_slope(dy, dx);
        int y = y1;
        if ((y2 >> 16) + 2 < (int)vcall<DWORD>(f, 39) && (y1 >> 16) + 2 < (int)vcall<DWORD>(f, 39)) {
            DWORD *const rows = f_rows(f);
            for (int x = xa; x <= xb; x++, y += slope) {
                const int frac = (DWORD)y >> 8 & 0xff;
                const int row = (y >> 16) > 0 ? y >> 16 : 0;
                BYTE *p = pixels + (rows[row] + (DWORD)x) * N;
                blend(p, 255 - frac);
                blend(p + rows[1] * N, frac);
            }
            return;
        }
        DWORD *const rows = f_rows(f);
        Rect clip;
        f_clip_rect(f, &clip);
        for (int x = xa; x <= xb; x++, y += slope) {
            const int frac = (DWORD)y >> 8 & 0xff;
            const int row = (y >> 16) > 0 ? y >> 16 : 0;
            BYTE *p = pixels + (rows[row] + (DWORD)x) * N;
            if (inside(clip, x, row))
                blend(p, 255 - frac);
            p += rows[1] * N;
            if (inside(clip, x, row + 1))
                blend(p, frac);
        }
        return;
    }
    // steep: y as integers, x 16.16
    int ya = y1 >> 16, yb = y2 >> 16, xs = x1;
    if (dy < 0) {
        xs = x2;
        x2 = x1;
        const int t = ya;
        ya = yb;
        yb = t;
        dx = -dx;
        dy = -dy;
    }
    const int slope = line_slope(dx, dy);
    if ((x2 >> 16) + 2 < (int)vcall<DWORD>(f, 45) && (xs >> 16) + 2 < (int)vcall<DWORD>(f, 45)) {
        DWORD *const rows = f_rows(f);
        for (int y = ya; y <= yb; y++, xs += slope) {
            const int frac = (DWORD)xs >> 8 & 0xff;
            BYTE *p = pixels + (rows[y] + (DWORD)(xs >> 16)) * N;
            blend(p, 255 - frac);
            blend(p + N, frac);
        }
        return;
    }
    DWORD *const rows = f_rows(f);
    Rect clip;
    f_clip_rect(f, &clip);
    for (int y = ya; y <= yb; y++, xs += slope) {
        const int frac = (DWORD)xs >> 8 & 0xff, x = xs >> 16;
        BYTE *p = pixels + (rows[y] + (DWORD)x) * N;
        if (inside(clip, x, y))
            blend(p, 255 - frac);
        if (inside(clip, x + 1, y))
            blend(p + N, frac);
    }
}

// 0x42b6e0, 0x42c0f0, 0x42c9e0
void __fastcall Raster8_Spans(CFrame *f, int x1, int y1, int x2, int y2, DWORD color) { raster<Blend8>(f, x1, y1, x2, y2, color); }
void __fastcall Raster16_Spans(CFrame *f, int x1, int y1, int x2, int y2, DWORD color) { raster<Blend16>(f, x1, y1, x2, y2, color); }
void __fastcall Raster24_Spans(CFrame *f, int x1, int y1, int x2, int y2, DWORD color) { raster<Blend24>(f, x1, y1, x2, y2, color); }

// 0x42b160 (fastcall frame, colour; x1, y1, x2, y2 as floats; ret 0x10)
void __fastcall DrawLineSpans(CFrame *f, FieldColor *c, float x1, float y1, float x2, float y2)
{
    Rect clip;
    f_clip_rect(f, &clip);
    const int ox = f_x(f), oy = f_y(f);
    const long double k = cst(glob_addr(0x5ee610));   // 65536
    int lx1 = x87_ftol((fld(x1) - fild(ox)) * k), lx2 = x87_ftol((fld(x2) - fild(ox)) * k);
    int ly1 = x87_ftol((fld(y1) - fild(oy)) * k), ly2 = x87_ftol((fld(y2) - fild(oy)) * k);
    clip.left <<= 16;
    clip.top <<= 16;
    clip.right <<= 16;
    clip.bottom <<= 16;
    if (!(BYTE)fn<DWORD(__fastcall *)(const Rect *, int *, int *, int *, int *)>(0x4b2d30)(&clip, &lx1, &ly1, &lx2, &ly2))
        return;
    switch (f_flags(f) & F_DEPTH) {
    case F_8: {
        const DWORD index = c->flags & 1 ? c->index : color_index(c);
        // the colour is the dword of x1's argument slot with the index in its low byte
        const DWORD arg = (float_bits(x1) & 0xffffff00u) | index;
        Raster8_Spans(f, lx1 & (int)0xffff0000, ly1 & (int)0xffff0000, lx2 & (int)0xffff0000, ly2 & (int)0xffff0000, arg);
        return;
    }
    case F_16:
        Raster16_Spans(f, lx1, ly1, lx2, ly2, c->pixel16);
        return;
    case F_24:
        Raster24_Spans(f, lx1, ly1, lx2, ly2, (DWORD)c->b2 | (DWORD)c->g2 << 8 | (DWORD)c->r2 << 16);
        return;
    }
}

// _CIasin of the C runtime (0x5d12d0): the argument and the result in st(0)
constexpr DWORD CI_ASIN = 0x5d12d0;
long double ci_asin(long double v)
{
    long double r;
#ifdef ZAX_STANDALONE
    const DWORD ci_asin_fn = fn_addr(CI_ASIN);   // the function table's entry (batch 6-10c)
#else
    const DWORD &ci_asin_fn = CI_ASIN;
#endif
    __asm__ volatile("call *%2"
                     : "=t"(r)
                     : "0"(v), "m"(ci_asin_fn)
                     : "eax", "ecx", "edx", "memory", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
    return r;
}
inline long double dbl(DWORD a) { return (long double)*at<const double *>(a); }
// fst qword: rounded to double through memory
inline double fst_double(long double v)
{
    double d;
    __asm__ volatile("fstl %0" : "=m"(d) : "t"(v));
    return d;
}
inline long double fyl2x(long double y, long double x)   // y * log2(x)
{
    long double r;
    __asm__("fyl2x" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}
inline long double ln(long double x)
{
    long double ln2;
    __asm__("fldln2" : "=t"(ln2));
    return fyl2x(ln2, x);
}

// The coverage of a pixel by a line of width 1 at distance d (Gupta-Sproull filter): 2/pi times the
// sector term plus the triangle term, one of the three forms the table builder evaluates
inline long double cover_term(long double sq_arg, long double a_mul, long double a_asin)
{
    const long double c = dbl(glob_addr(0x5ee650)), two_pi = dbl(glob_addr(0x5ee648)), half = dbl(glob_addr(0x5ee658)), sqrt2 = dbl(glob_addr(0x5ee640)), inv_pi = dbl(glob_addr(0x5ee638));
    const long double head = half - fsqrt(c - sq_arg) * a_mul * two_pi;
    return head - ci_asin(a_asin * sqrt2) * inv_pi;
}

// 0x42b320 (no arguments): the coverage table 0x62ea40 (sizes from log2(128) and log2(1.2071)) and the
// table 0x62ea3c of 65536 / sqrt(1 + x^2) for x = 0 .. 1 in 1024 steps; reuses 0x62ea40 when it is big
// enough
void __cdecl SpanTables_Init()
{
    *glob<BYTE *>(SPAN_BUILT) = 1;
    const long double one = dbl(glob_addr(0x5ee678)), half = dbl(glob_addr(0x5ee658)), limit = dbl(glob_addr(0x5ee668)), c255 = dbl(glob_addr(0x5ee630));
    const long double ln2 = ln(dbl(glob_addr(0x5ee680)));
    const double ln2_d = fst_double(ln2);
    const long double inv_ln2 = one / ln2;
    const int log_steps = x87_ftol(ln(fild(*glob<int *>(0x60c1b8))) * inv_ln2 + dbl(glob_addr(0x5ee670)));
    const int log_limit = x87_ftol(ln(limit) * inv_ln2);
    const int shift = log_limit - log_steps + 0x11;
    const int steps = 0x13505 >> (shift & 31);
    *glob<int *>(SPAN_SHIFT) = shift;
    const int n = steps + 2;
    *glob<int *>(SPAN_LIMIT) = steps << (shift & 31);
    int *table = *glob<int **>(SPAN_COVER);
    if (table && n > *glob<int *>(SPAN_COVER_SIZE)) {
        fn<void(__cdecl *)(void *)>(0x5d0320)(table);
        *glob<int **>(SPAN_COVER) = nullptr;
        *glob<int *>(SPAN_COVER_SIZE) = 0;
        table = nullptr;
    }
    if (!table) {
        table = fn<int *(__cdecl *)(DWORD)>(0x5d033a)((DWORD)n * 4);
        *glob<int **>(SPAN_COVER) = table;
        *glob<int *>(SPAN_COVER_SIZE) = n;
    }
    int *out = table;
    double t_d = fst_double(dbl(glob_addr(0x5ee660)));
    const double step = fst_double(limit / fild(n - 2));
    long double t = t_d;
    // t < 0.2071: both edges of the pixel inside the line's reach
    do {
        const double a = fst_double(half - t), b = fst_double(t + half);
        const long double v1 = one - cover_term((long double)a * a, a, a);
        const long double v2 = cover_term((long double)b * b, b, b);
        *out++ = x87_ftol((v1 - v2) * c255 + half);
        t = (long double)step + t_d;
        t_d = fst_double(t);
    } while (c0(t, dbl(glob_addr(0x5ee628))));
    // t < 0.5: one edge
    while (c0(t, half)) {
        const double a = fst_double(half - t);
        *out++ = x87_ftol((one - cover_term((long double)a * a, a, a)) * c255 + half);
        t = (long double)step + t_d;
        t_d = fst_double(t);
    }
    // t < 1.2071: the far side
    while (c0(t, limit)) {
        const long double a = t - half;
        const double a_d = fst_double(a);
        *out++ = x87_ftol(cover_term(a * a_d, a_d, a_d) * c255 + half);
        t = (long double)step + t_d;
        t_d = fst_double(t);
    }
    *out = 0;
    (*glob<int **>(SPAN_COVER))[n - 1] = 0;
    // the second table
    const int bits = x87_ftol(ln(fild(*glob<int *>(0x60c1b4))) / (long double)ln2_d + dbl(glob_addr(0x5ee620)));
    int count = (1 << (bits & 31)) + 1;
    *glob<int *>(SPAN_SHIFT2) = 16 - bits;
    if (*glob<void **>(SPAN_SQRT)) {
        fn<void(__cdecl *)(void *)>(0x5d0320)(*glob<void **>(SPAN_SQRT));
        *glob<void **>(SPAN_SQRT) = nullptr;
    }
    int *sq = fn<int *(__cdecl *)(DWORD)>(0x5d033a)((DWORD)count * 4);
    *glob<int **>(SPAN_SQRT) = sq;
    const long double s = one / fild(count - 1), k = dbl(glob_addr(0x5ee618));
    long double x = dbl(glob_addr(0x5ee660));
    do {
        *sq++ = x87_ftol(fsqrt(one / (x * x + one)) * k + half);
        x = x + s;
    } while (c0(x, one));
    (*glob<int **>(SPAN_SQRT))[count - 1] = x87_ftol(fsqrt(half) * k + half);
}

// 0x42b6a0: frees both tables
void __cdecl SpanTables_Free()
{
    if (*glob<void **>(SPAN_COVER)) {
        fn<void(__cdecl *)(void *)>(0x5d0320)(*glob<void **>(SPAN_COVER));
        *glob<void **>(SPAN_COVER) = nullptr;
    }
    if (*glob<void **>(SPAN_SQRT)) {
        fn<void(__cdecl *)(void *)>(0x5d0320)(*glob<void **>(SPAN_SQRT));
        *glob<void **>(SPAN_SQRT) = nullptr;
    }
}

}  // namespace

ZAX_HOOKS(raster_line)
{
    const Hook list[] = {
        {0x42b160, (void *)&DrawLineSpans, "CFrame_DrawLineSpans", 6, {0x83, 0xec, 0x24, 0x56, 0x8b, 0xf1, 0x8d, 0x4c}},
        {0x42b320, (void *)&SpanTables_Init, "SpanTables_Init", 6, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec}},
        {0x42b6a0, (void *)&SpanTables_Free, "SpanTables_Free", 5, {0xa1, 0x40, 0xea, 0x62, 0x00, 0x85, 0xc0, 0x74}},
        {0x42b6e0, (void *)&Raster8_Spans, "Raster8_Spans", 5, {0x83, 0xec, 0x34, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x42c0f0, (void *)&Raster16_Spans, "Raster16_Spans", 5, {0x83, 0xec, 0x28, 0x8b, 0x01, 0x53, 0x55, 0x56}},
        {0x42c9e0, (void *)&Raster24_Spans, "Raster24_Spans", 5, {0x83, 0xec, 0x3c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
