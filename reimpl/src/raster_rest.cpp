// The rest of batch R7 (docs/DECOMPILATION.md 9, batch R6): the 24-bit polygon fills with their half-triangle
// helpers, the textured fills of 8 and 16 bits, the green fills' helpers, and filter 10's Apply with its
// colour histogram. The point lists, the clipping, the edges and the other fills are in raster_poly.cpp.
//
// [confirmed: code; compared in the running game]
//  - The fills walk a triangle like raster_poly.cpp: the corners sorted by y, three edges (PolyEdge_Setup
//    0x54d040), the long edge left when the cross product is >= 0, two halves of rows stepped by
//    PolyEdge_Step 0x54d0f0, a span [left x, right x) minus the origin when left < right. A half asks the
//    frame for slots 43, 44 (the origin), 46 (rows), 31 (pixels); a textured half also the texture's 41, 42
//    (the power-of-two masks), 46 and 31, in the order frame 43, 44, texture 41, 42, frame 46, 31, texture
//    46, 31. The texture is tiled in absolute frame coordinates: row (y & ymask), column (x & xmask) from
//    the span's absolute left x.
//  - The whole-polygon fills (fastcall polygon, frame; parameter, 0; ret 8: the 24-bit fills 0x549d50 (type
//    0), 0x549fe0 (1), 0x54a200 (2), 0x54a640 (3), 0x54a420 (4), 0x54a860 (5) and the 16-bit texture
//    0x549aa0) draw the fan (0, i, i + 1); the first half inline, the second through a helper (fastcall
//    frame, parameter; left edge, right edge, rows, 0; ret 0x10), except type 0, which draws both inline.
//    The 8-bit texture fill 0x54b230 is a triangle fill (fastcall frame, texture; points, a, b, c, 0;
//    ret 0x14) with both halves inline.
//  - 24 bits: type 0 zero bytes (Mem_FillBytes); type 1 the 3-byte step halving the bytes at q + 6, q + 3
//    and q while q < end, so every byte from the span's fourth on is halved three times and the five bytes
//    after the span's end are halved too [quirk kept]; types 2, 3, 4 add the low byte of the tint constant
//    shifted by 3, 2, 3 to the blue, green, red byte with an 8-bit wrap (the "saturation" compares the
//    wrapped sum with 0xff), so with the constants 10, 0x140 and 0x5000 only blue changes (+ 80), green
//    and red add 0 [quirk kept]; type 5 copies texels of a 24-bit texture.
//  - The green helpers: 0x549820 (fastcall start, 0; end, 0, 0, 0) the scalar tail of the MMX fill's first
//    half, 0x54c8e0 and 0x54c790 the second halves of 0x549870 and 0x5495a0 (raster_poly.cpp draws its
//    halves itself and does not call them).
//  - Filter 10 (the palette from a colour histogram): a histogram of 2^18 dwords (6 bits per channel) on
//    the stack (1 MB with _chkstk: as much as the exe's whole stack reserve, SizeOfStackReserve 0x100000,
//    so the original overflows the main thread's stack whenever it runs [confirmed: VM, the check crashed
//    the game]; here on the stack too since batch decomp-faithful2, through the exe's _chkstk in a naked
//    entry, so it overflows the same way; until then on the heap), cleared (0x46dc30),
//    filled from the frame's pixels as 3-byte pixels, width * height of them without the pitch (0x46dc50:
//    slots 31, 45, 39), then DrawPalette_Build 0x527fa0 (the frame's palette, slot 58, which a frame without
//    one creates; kind 1, 0xfe colours); al = 1.
#include "blit24.h"

namespace zax {

using namespace render;

namespace {

struct Pt {
    int x, y;
};
struct PtArray {
    DWORD vftable;
    Pt *data;
    DWORD count, capacity;
};
struct PolyPoints {   // vtable 0x5fd35c
    DWORD vftable;
    PtArray pts;      // 0x04
    DWORD type;       // 0x14
    void *param;      // 0x18
};
struct Edge {         // 0x1c
    int x, step, rstep, den, rem, y, height;
};

constexpr DWORD ADD_BLUE = 0x61eacc, ADD_RED = 0x61ead0, ADD_GREEN = 0x61ead4;
constexpr DWORD GREEN_FIELD = 0x61eae0, GREEN_KEEP = 0x61ead8;

using Setup = Edge *(__fastcall *)(Edge *, DWORD, const Pt *, DWORD, DWORD);
using Half = void(__fastcall *)(void *frame, DWORD param, Edge *l, Edge *r, int rows, DWORD);
inline void step(Edge *e) { fn<void(__fastcall *)(Edge *)>(0x54d0f0)(e); }

// The three edges of triangle (a, b, c) and the cross product's sign
struct Tri {
    Edge e1, e2, e3;   // top -> bottom, top -> middle, middle -> bottom
    bool left_long;    // cross >= 0: e1 is the left edge
};
void setup(Tri &t, const Pt *pts, DWORD a, DWORD b, DWORD c)
{
    DWORD top, mid, bot;
    if (pts[a].y < pts[b].y) {
        if (pts[c].y < pts[a].y) {
            top = c, mid = a, bot = b;
        } else {
            top = a;
            if (pts[b].y < pts[c].y)
                mid = b, bot = c;
            else
                mid = c, bot = b;
        }
    } else if (pts[c].y < pts[b].y) {
        top = c, mid = b, bot = a;
    } else {
        top = b;
        if (pts[a].y < pts[c].y)
            mid = a, bot = c;
        else
            mid = c, bot = a;
    }
    fn<Setup>(0x54d040)(&t.e1, 0, pts, top, bot);
    fn<Setup>(0x54d040)(&t.e2, 0, pts, top, mid);
    fn<Setup>(0x54d040)(&t.e3, 0, pts, mid, bot);
    const int cross = (int)((DWORD)(pts[bot].y - pts[top].y) * (DWORD)(pts[mid].x - pts[top].x) -
                            (DWORD)(pts[mid].y - pts[top].y) * (DWORD)(pts[bot].x - pts[top].x));
    t.left_long = cross >= 0;
}

// A half of plain rows: span(start, end) for every row with left < right; N bytes per pixel
template <int N, typename F> void half_rows(void *frame, Edge *l, Edge *r, int rows, F span)
{
    const int ox = vcall<int>(frame, 43), oy = vcall<int>(frame, 44);
    const DWORD *row = f_rows(frame) + (l->y - oy);
    BYTE *const pixels = f_pixels(frame);
    for (; rows; rows--, row++) {
        if (l->x < r->x)
            span(pixels + (*row + (DWORD)(l->x - ox)) * N, pixels + (*row + (DWORD)(r->x - ox)) * N);
        step(l);
        step(r);
    }
}

// A half of textured rows: texels (x & xmask, y & ymask) of `tex` from the left edge's absolute x
template <int N> void half_texture(void *frame, void *tex, Edge *l, Edge *r, int rows)
{
    const int ox = vcall<int>(frame, 43), oy = vcall<int>(frame, 44);
    const DWORD xmask = (1u << (vcall<DWORD>(tex, 41) & 31)) - 1;
    const DWORD ymask = (1u << (vcall<DWORD>(tex, 42) & 31)) - 1;
    const DWORD *row = f_rows(frame) + (l->y - oy);
    BYTE *const pixels = f_pixels(frame);
    const DWORD *trows = f_rows(tex);
    BYTE *const tpix = f_pixels(tex);
    for (; rows; rows--, row++) {
        if (l->x < r->x) {
            BYTE *p = pixels + (*row + (DWORD)(l->x - ox)) * N, *const end = pixels + (*row + (DWORD)(r->x - ox)) * N;
            const BYTE *trow = tpix + trows[(DWORD)l->y & ymask] * N;
            for (DWORD x = (DWORD)l->x; p < end; p += N, x++) {
                const BYTE *t = trow + (x & xmask) * N;
                for (int k = 0; k < N; k++)
                    p[k] = t[k];
            }
        }
        step(l);
        step(r);
    }
}

// ---- The span operations ---------------------------------------------------------------------------------------------

void green_scalar(BYTE *p, BYTE *end)
{
    for (; p < end; p += 2) {
        WORD *w = reinterpret_cast<WORD *>(p);
        DWORD v = (*w & 0x7e0) + *glob<DWORD *>(ADD_GREEN);
        if (v >= 0x7e0)
            v = 0x7e0;
        *w = (WORD)((*w & 0xf81f) | v);
    }
}
// Four words per qword: ((q & 0x7e0) << 5) +us (k << 5) >> 5, or (q & 0xf81f); emms after the groups
void green_mmx(BYTE *p, BYTE *end)
{
    DWORD groups = (DWORD)(end - p) >> 3;
    if (groups) {
        BYTE *q = p;
        __asm__ volatile("movd %[k], %%mm1\n\t"
                         "punpcklwd %%mm1, %%mm1\n\t"
                         "punpckldq %%mm1, %%mm1\n\t"
                         "psllw $5, %%mm1\n"
                         "1:\n\t"
                         "movq (%[p]), %%mm2\n\t"
                         "movq %%mm2, %%mm3\n\t"
                         "addl $8, %[p]\n\t"
                         "pand (%[fm]), %%mm3\n\t"
                         "pand (%[km]), %%mm2\n\t"
                         "psllw $5, %%mm3\n\t"
                         "paddusw %%mm1, %%mm3\n\t"
                         "psrlw $5, %%mm3\n\t"
                         "por %%mm3, %%mm2\n\t"
                         "movq %%mm2, -8(%[p])\n\t"
                         "decl %[n]\n\t"
                         "jnz 1b\n\t"
                         "emms"
                         : [p] "+r"(q), [n] "+r"(groups)
                         : [k] "r"(*glob<DWORD *>(ADD_GREEN)), [fm] "r"(glob_addr(GREEN_FIELD)), [km] "r"(glob_addr(GREEN_KEEP))
                         : "memory", "mm1", "mm2", "mm3", "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
        p += ((DWORD)(end - p) >> 3) * 8;
    }
    green_scalar(p, end);
}

// 24 bits: type 1 (halving three bytes per step), 2-4 (a byte added with an 8-bit wrap)
void halve24(BYTE *q, BYTE *end)
{
    for (; q < end; q += 3) {
        if (!(g_span24_mutation & 8))
            q[6] >>= 1;
        q[3] >>= 1;
        q[0] >>= 1;
    }
}
template <int CH, DWORD K, int SHIFT> void add24(BYTE *q, BYTE *end)
{
    for (; q < end; q += 3)
        q[CH] = (BYTE)(q[CH] + (BYTE)(*glob<BYTE *>(K) << SHIFT));
}
void zero24(BYTE *q, BYTE *end) { Mem_FillBytes(q, 0, (DWORD)(end - q)); }

// ---- The helpers (fastcall frame, parameter; left, right, rows, 0; ret 0x10) ---------------------------------------

// 0x549820 (fastcall start, 0; end, 0, 0, 0)
void __fastcall PolyFill16_GreenTail(BYTE *start, DWORD, BYTE *end, DWORD, DWORD, DWORD) { green_scalar(start, end); }
// 0x54c8e0 / 0x54c790
void __fastcall PolyFill16_GreenHalf(void *f, DWORD, Edge *l, Edge *r, int rows, DWORD) { half_rows<2>(f, l, r, rows, &green_scalar); }
void __fastcall PolyFill16_GreenHalf_MMX(void *f, DWORD, Edge *l, Edge *r, int rows, DWORD) { half_rows<2>(f, l, r, rows, &green_mmx); }
// 0x54c9b0 (the parameter: the texture)
void __fastcall PolyFill16_TextureHalf(void *f, void *tex, Edge *l, Edge *r, int rows, DWORD) { half_texture<2>(f, tex, l, r, rows); }
// 0x54caf0, 0x54cbb0, 0x54cd30, 0x54cc70, 0x54cdf0
void __fastcall PolyFill24_HalveHalf(void *f, DWORD, Edge *l, Edge *r, int rows, DWORD) { half_rows<3>(f, l, r, rows, &halve24); }
void __fastcall PolyFill24_BlueHalf(void *f, DWORD, Edge *l, Edge *r, int rows, DWORD) { half_rows<3>(f, l, r, rows, &add24<0, ADD_BLUE, 3>); }
void __fastcall PolyFill24_GreenHalf(void *f, DWORD, Edge *l, Edge *r, int rows, DWORD) { half_rows<3>(f, l, r, rows, &add24<1, ADD_GREEN, 2>); }
void __fastcall PolyFill24_RedHalf(void *f, DWORD, Edge *l, Edge *r, int rows, DWORD) { half_rows<3>(f, l, r, rows, &add24<2, ADD_RED, 3>); }
void __fastcall PolyFill24_TextureHalf(void *f, void *tex, Edge *l, Edge *r, int rows, DWORD) { half_texture<3>(f, tex, l, r, rows); }

// ---- The fills -----------------------------------------------------------------------------------------------------

// A whole-polygon fill: the fan (0, i, i + 1), the first half by `first`, the second through HALF (an exe
// address; 0: by `first` too)
template <DWORD HALF, typename F> void whole(const PolyPoints *poly, void *f, DWORD param, DWORD zero, F first)
{
    for (DWORD i = 1; i + 1 < poly->pts.count; i++) {
        Tri t;
        setup(t, poly->pts.data, 0, i, i + 1);
        if (t.left_long)
            first(&t.e1, &t.e2, t.e2.height);
        else
            first(&t.e2, &t.e1, t.e2.height);
        Edge *const l = t.left_long ? &t.e1 : &t.e3, *const r = t.left_long ? &t.e3 : &t.e1;
        if (HALF)
            code<Half>(HALF)(f, param, l, r, t.e3.height, zero);
        else
            first(l, r, t.e3.height);
    }
}

template <DWORD HALF, void (*SPAN)(BYTE *, BYTE *)> void __fastcall PolyFill24(const PolyPoints *poly, void *f, DWORD param, DWORD zero)
{
    whole<HALF>(poly, f, param, zero, [&](Edge *l, Edge *r, int rows) { half_rows<3>(f, l, r, rows, SPAN); });
}
template <int N, DWORD HALF> void __fastcall PolyFillTexture(const PolyPoints *poly, void *f, void *tex, DWORD zero)
{
    whole<HALF>(poly, f, (DWORD)(uintptr_t)tex, zero, [&](Edge *l, Edge *r, int rows) { half_texture<N>(f, tex, l, r, rows); });
}

// 0x54b230 (fastcall frame, texture; points, a, b, c, 0), ret 0x14: one triangle, both halves inline
void __fastcall PolyFill8_Texture(void *f, void *tex, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    Tri t;
    setup(t, pts, a, b, c);
    if (t.left_long)
        half_texture<1>(f, tex, &t.e1, &t.e2, t.e2.height);
    else
        half_texture<1>(f, tex, &t.e2, &t.e1, t.e2.height);
    if (t.left_long)
        half_texture<1>(f, tex, &t.e1, &t.e3, t.e3.height);
    else
        half_texture<1>(f, tex, &t.e3, &t.e1, t.e3.height);
}

// ---- Filter 10 --------------------------------------------------------------------------------------------------------

// 0x46dc30 (thiscall histogram): 2^18 dwords cleared
DWORD *THISCALL Histogram_Clear(DWORD *h)
{
    for (DWORD i = 0; i < 0x40000; i++)
        h[i] = 0;
    return h;
}
// 0x46dc50 (thiscall histogram; frame), ret 4: width * height 3-byte pixels counted from the pixels' start
void THISCALL Histogram_Add24(DWORD *h, void *frame)
{
    DWORD n = vcall<DWORD>(frame, 45);
    n *= vcall<DWORD>(frame, 39);
    const BYTE *p = f_pixels(frame);
    for (; n; n--, p += 3)
        h[((((DWORD)(p[0] & 0xfc) << 6) + (p[1] & 0xfc)) << 4) + (p[2] >> 2)]++;
}

}  // namespace

// 0x4b2b50 (thiscall filter; frame, rect), ret 8 [verified 2026-11-12: the instructions 0x4b2b50-0x4b2bb8]: the colour
// histogram, 2^18 dwords (1 MB), lies on the stack as in the exe: the entry makes the exe's frame with the exe's
// `mov eax, 0x100000; call _chkstk` (0x5d2080, which probes it page by page from the top), and the histogram is the
// whole frame ([esp + 8] after the exe's two pushes, the frame's base here), cleared by 0x46dc30 and filled by
// 0x46dc50. On a stack with less than 1 MB free below the call (the main thread's whole reserve is 1 MB,
// SizeOfStackReserve 0x100000) the probe runs into the end of the stack and the game ends with a stack overflow,
// as the original does (docs/ORIGINAL_BUGS.md 2.27). Then slot 58 (the palette) of the frame as the class test left
// it (null: read through address 0, as in the exe, which has no test there). al: 1, the upper bytes of eax the
// palette builder's.
extern "C" DWORD __cdecl zax_filter10_apply(void *, void *frame, const Rect *, BYTE *histogram)
{
    void *f = frame && obj_is_kind_of(frame, fn<CClassInfo *(__cdecl *)()>(0x4a8760)()) ? frame : nullptr;
    __asm__("" : "+r"(f));   // no test of f before its slot is called: the compiler may not assume it non-null
    DWORD *const h = reinterpret_cast<DWORD *>(histogram);
    fn<DWORD *(THISCALL *)(DWORD *)>(0x46dc30)(h);
    fn<void(THISCALL *)(DWORD *, void *)>(0x46dc50)(h, frame);
    Palette *const pal = f_palette(f);
    return with_low_byte(fn<DWORD(THISCALL *)(Palette *, DWORD *, DWORD, DWORD)>(0x527fa0)(pal, h, 1, 0xfe), 1);
}
extern "C" __attribute__((naked)) void zax_filter10_apply_entry()
{
    __asm__("movl $0x100000, %eax\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x5d2080) ", %edx\n\t"   // _chkstk: esp lowered by eax, every page probed; ecx kept
            "calll *%edx\n\t"
            "pushl %esp\n\t"                                  // the histogram: the frame's base
            "pushl (0x100000+4+8)(%esp)\n\t"                  // rect
            "pushl (0x100000+4+8)(%esp)\n\t"                  // frame
            "pushl %ecx\n\t"
            "calll _zax_filter10_apply\n\t"
            "addl $(0x100000+16), %esp\n\t"
            "retl $8");
}

ZAX_HOOKS(raster_rest)
{
    const Hook list[] = {
        {0x549820, (void *)&PolyFill16_GreenTail, "PolyFill16_GreenTail", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0xce, 0x73}},
        {0x54c8e0, (void *)&PolyFill16_GreenHalf, "PolyFill16_GreenHalf", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x54c790, (void *)&PolyFill16_GreenHalf_MMX, "PolyFill16_GreenHalf_MMX", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x14, 0x53, 0x56}},
        {0x54c9b0, (void *)&PolyFill16_TextureHalf, "PolyFill16_TextureHalf", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x54caf0, (void *)&PolyFill24_HalveHalf, "PolyFill24_HalfBrightnessHalf", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06}},
        {0x54cbb0, (void *)&PolyFill24_BlueHalf, "PolyFill24_BlueHalf", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06}},
        {0x54cd30, (void *)&PolyFill24_GreenHalf, "PolyFill24_GreenHalf", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06}},
        {0x54cc70, (void *)&PolyFill24_RedHalf, "PolyFill24_RedHalf", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06}},
        {0x54cdf0, (void *)&PolyFill24_TextureHalf, "PolyFill24_TextureHalf", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x549d50, (void *)&PolyFill24<0, &zero24>, "PolyFill24_Black", 6, {0x83, 0xec, 0x74, 0x8b, 0x41, 0x0c, 0x53, 0x85}},
        {0x549fe0, (void *)&PolyFill24<0x54caf0, &halve24>, "PolyFill24_HalfBrightness", 7, {0x83, 0xec, 0x74, 0x89, 0x54, 0x24, 0x14, 0x89}},
        {0x54a200, (void *)&PolyFill24<0x54cbb0, &add24<0, ADD_BLUE, 3>>, "PolyFill24_Blue", 7, {0x83, 0xec, 0x74, 0x89, 0x54, 0x24, 0x14, 0x89}},
        {0x54a640, (void *)&PolyFill24<0x54cd30, &add24<1, ADD_GREEN, 2>>, "PolyFill24_Green", 7, {0x83, 0xec, 0x74, 0x89, 0x54, 0x24, 0x14, 0x89}},
        {0x54a420, (void *)&PolyFill24<0x54cc70, &add24<2, ADD_RED, 3>>, "PolyFill24_Red", 7, {0x83, 0xec, 0x74, 0x89, 0x54, 0x24, 0x14, 0x89}},
        {0x54a860, (void *)&PolyFillTexture<3, 0x54cdf0>, "PolyFill24_Texture", 6, {0x81, 0xec, 0x8c, 0x00, 0x00, 0x00, 0x55, 0x8b}},
        {0x549aa0, (void *)&PolyFillTexture<2, 0x54c9b0>, "PolyFill16_Texture", 6, {0x81, 0xec, 0x8c, 0x00, 0x00, 0x00, 0x89, 0x54}},
        {0x54b230, (void *)&PolyFill8_Texture, "PolyFill8_Texture", 6, {0x81, 0xec, 0x88, 0x00, 0x00, 0x00, 0x8b, 0x84}},
        {0x46dc30, (void *)&Histogram_Clear, "Histogram_Clear", 8, {0x8b, 0xd1, 0x57, 0xb9, 0x00, 0x00, 0x04, 0x00}},
        {0x46dc50, (void *)&Histogram_Add24, "Histogram_Add24", 6, {0x55, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x57, 0x8b}},
        {0x4b2b50, (void *)&zax_filter10_apply_entry, "FrameFilter10::Apply", 5, {0xb8, 0x00, 0x00, 0x10, 0x00, 0xe8, 0x26, 0xf5}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
