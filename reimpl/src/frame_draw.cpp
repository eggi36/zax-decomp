// The frame drawing helpers: the byte fill 0x432c10, rectangle and line clipping 0x4b2c20-0x4b2f50 and
// 0x4b3d30/0x4b3e50, the blit wrappers 0x4b3280-0x4b3390 and the rectangle, line and bar helpers
// 0x4b33f0-0x4b3ce0 (types in frame.h).
//
// Not here: FrameDraw_StaticSwitch 0x4b3530 with its helper 0x4b39a0 (no caller or pointer found) and the
// one-byte stub 0x4b3a60.
#include "frame.h"

namespace zax {

using namespace render;

// 0x432c10 (fastcall dst, value; count)
DWORD __fastcall render::Mem_FillBytes(BYTE *dst, DWORD value, DWORD count)
{
    while (((uintptr_t)dst & 3) && count) {
        *dst++ = (BYTE)value;
        count--;
    }
    for (DWORD n = count >> 2; n; n--, dst += 4)
        memcpy(dst, &value, 4);
    switch (count & 3) {
    case 3: dst[2] = (BYTE)value; [[fallthrough]];
    case 2: dst[1] = (BYTE)value; [[fallthrough]];
    case 1: dst[0] = (BYTE)value;
    }
    return value;
}

// 0x4b2cd0 (fastcall clip, rect): top, bottom, left, right cut in this order, each failing when the rectangle
// becomes empty in that axis; an inverted rectangle that needs no cut passes. eax as the exe leaves it:
// the low byte the result, above it the last value the code loaded (the clip edge or the rectangle's
// right or left edge).
DWORD __fastcall render::Rect_Intersect(const Rect *clip, Rect *r)
{
    DWORD eax = (DWORD)clip->top;
    if (r->top < clip->top) {
        r->top = clip->top;
        if (clip->top >= r->bottom)
            return eax & ~0xffu;
    }
    eax = (DWORD)clip->bottom;
    if (r->bottom > clip->bottom) {
        r->bottom = clip->bottom;
        if (clip->bottom <= r->top)
            return eax & ~0xffu;
    }
    eax = (DWORD)clip->left;
    if (r->left < clip->left) {
        r->left = clip->left;
        if (clip->left >= r->right)
            return eax & ~0xffu;
    }
    eax = (DWORD)r->right;
    if (r->right > clip->right) {
        eax = (DWORD)r->left;
        r->right = clip->right;
        if (clip->right <= r->left)
            return eax & ~0xffu;
    }
    return (eax & ~0xffu) | 1;
}

// 0x4b2c20 (fastcall clip, src; dst): dst cut to clip (top, bottom, left, right), each cut moving the same
// edge of src by the amount cut; al whether dst stays non-empty, the upper bytes of eax dst's address
DWORD __fastcall render::Blit_ClipRects(const Rect *clip, Rect *src, Rect *dst)
{
    const DWORD hi = (DWORD)(uintptr_t)dst & ~0xffu;
    if (dst->top < clip->top) {
        src->top += clip->top - dst->top;
        dst->top = clip->top;
        if (clip->top >= dst->bottom)
            return hi;
    }
    if (dst->bottom > clip->bottom) {
        src->bottom += clip->bottom - dst->bottom;
        dst->bottom = clip->bottom;
        if (clip->bottom <= dst->top)
            return hi;
    }
    if (dst->left < clip->left) {
        src->left += clip->left - dst->left;
        dst->left = clip->left;
        if (clip->left >= dst->right)
            return hi;
    }
    if (dst->right > clip->right) {
        src->right += clip->right - dst->right;
        dst->right = clip->right;
        if (clip->right <= dst->left)
            return hi;
    }
    return hi | 1;
}

namespace {

// 0x4b3d30 (fastcall clip, xo; yo, &x, &y, code): the point (x, y) of the line through (xo, yo) moved onto
// the clip edges of `code` (1 top, 2 right, 4 bottom, 8 left) in this order until it lies inside the
// rectangle: k = dy / dx and b = y - x * k as floats, x = ftol((edge - b) / k) or y = ftol(edge * k + b);
// the left edge is taken without the test
void __fastcall Line_ClipIntersect(const Rect *c, int xo, int yo, int *x, int *y, DWORD code)
{
    const float k = fstp(fild(*y - yo) / fild(*x - xo));
    const float b = fstp(fild(*y) - fild(*x) * fld(k));
    const BYTE bits = (BYTE)code;
    auto inside = [&] { return *x >= c->left && *x < c->right && *y >= c->top && *y < c->bottom; };
    if (bits & 1) {
        *y = c->top;
        *x = x87_ftol((fild(c->top) - fld(b)) / fld(k));
        if (inside())
            return;
    }
    if (bits & 2) {
        *x = c->right - 1;
        *y = x87_ftol(fild(c->right - 1) * fld(k) + fld(b));
        if (inside())
            return;
    }
    if (bits & 4) {
        *y = c->bottom - 1;
        *x = x87_ftol((fild(c->bottom - 1) - fld(b)) / fld(k));
        if (inside())
            return;
    }
    if (bits & 8) {
        *x = c->left;
        *y = x87_ftol(fild(c->left) * fld(k) + fld(b));
    }
}

// 0x4b3e50 (fastcall clip, &x; xo, yo, &y, code): the float variant; k = (y - yo) / (x - xo) and b = y - x * k
// (k unrounded in it), stored as floats; the tests of the computed coordinate use its unrounded value,
// the stored edge coordinate its float; edges right and bottom at right - 1.0 / bottom - 1.0
void __fastcall Line_ClipIntersectF(const RectF *c, float *x, float xo, float yo, float *y, DWORD code)
{
    const long double kk = (fld(*y) - fld(yo)) / (fld(*x) - fld(xo));
    const float k = fstp(kk);
    const float b = fstp(fld(*y) - kk * fld(*x));
    const BYTE bits = (BYTE)code;
    if (bits & 1) {
        *y = fstp(fld(c->top));
        const long double v = (fld(*y) - fld(b)) / fld(k);
        *x = fstp(v);
        if (!c0(v, fld(c->left)) && c0(v, fld(c->right)) && !c0(fld(*y), fld(c->top)) && c0(fld(*y), fld(c->bottom)))
            return;
    }
    if (bits & 2) {
        const long double e = fld(c->right) - cst(glob_addr(C_ONE));
        *x = fstp(e);
        const float v = fstp(e * fld(k) + fld(b));
        *y = v;
        if (!c0(fld(*x), fld(c->left)) && c0(fld(*x), fld(c->right)) && !c0(fld(v), fld(c->top)) && c0(fld(v), fld(c->bottom)))
            return;
    }
    if (bits & 4) {
        const long double e = fld(c->bottom) - cst(glob_addr(C_ONE));
        *y = fstp(e);
        const long double v = (e - fld(b)) / fld(k);
        *x = fstp(v);
        if (!c0(v, fld(c->left)) && c0(v, fld(c->right)) && !c0(fld(*y), fld(c->top)) && c0(fld(*y), fld(c->bottom)))
            return;
    }
    if (bits & 8) {
        const long double e = fld(c->left);
        *x = fstp(e);
        *y = fstp(e * fld(k) + fld(b));
    }
}

}  // namespace

// 0x4b2d30 (fastcall clip, &x1; &y1, &x2, &y2): both end points inside: al 1. Else Cohen-Sutherland in one
// pass: the outcodes (1 above, 2 right of, 4 below, 8 left of the rectangle; right and bottom exclusive),
// none in common; an outside end point moved onto the rectangle: a vertical line to the top or bottom - 1,
// a horizontal one to the left or right - 1, others by 0x4b3d30 towards the other end point (the second
// end point always towards the first one's unclipped position). Al 1 and the points written back when both
// are inside then, else 0 and nothing written.
DWORD __fastcall render::Line_ClipToRect(const Rect *c, int *x1, int *y1, int *x2, int *y2)
{
    if (*x1 >= c->left && *y1 >= c->top && *x1 < c->right && *y1 < c->bottom && *x2 >= c->left && *y2 >= c->top && *x2 < c->right &&
        *y2 < c->bottom)
        return ((DWORD)*x1 & ~0xffu) | 1;
    int nx1 = *x1, ny1 = *y1, nx2 = *x2, ny2 = *y2;
    auto outcode = [&](int x, int y) {
        DWORD k = y < c->top ? 1 : 0;
        if (x >= c->right)
            k |= 2;
        if (y >= c->bottom)
            k |= 4;
        if (x < c->left)
            k |= 8;
        return k;
    };
    const DWORD k1 = outcode(nx1, ny1), k2 = outcode(nx2, ny2);
    if (k1 & k2)
        return 0;
    if (k1) {
        if (*x1 == *x2)
            ny1 = *y1 < c->top ? c->top : c->bottom - 1;
        else if (*y1 == *y2)
            nx1 = *x1 < c->left ? c->left : c->right - 1;
        else
            fn<void(__fastcall *)(const Rect *, int, int, int *, int *, DWORD)>(0x4b3d30)(c, *x2, *y2, &nx1, &ny1, k1);
    }
    if (k2) {
        if (*x1 == *x2)
            ny2 = *y2 < c->top ? c->top : c->bottom - 1;
        else if (*y1 == *y2)
            nx2 = *x2 < c->left ? c->left : c->right - 1;
        else
            fn<void(__fastcall *)(const Rect *, int, int, int *, int *, DWORD)>(0x4b3d30)(c, *x1, *y1, &nx2, &ny2, k2);
    }
    if (nx1 < c->left || ny1 < c->top || nx1 >= c->right || ny1 >= c->bottom || nx2 < c->left || ny2 < c->top || nx2 >= c->right ||
        ny2 >= c->bottom)
        return 0;
    *x1 = nx1;
    *y1 = ny1;
    *x2 = nx2;
    *y2 = ny2;
    return ((DWORD)(uintptr_t)x2 & ~0xffu) | 1;
}

// 0x4b2f50 (fastcall clip, &x1; &y1, &x2, &y2): the float variant (comparisons as fcom: a NaN coordinate
// counts as below every edge); the moved coordinates through 0x4b3e50
DWORD __fastcall render::Line_ClipToRectF(const RectF *c, float *x1, float *y1, float *x2, float *y2)
{
    auto lt = [](float a, float b) { return c0(fld(a), fld(b)); };
    auto in = [&](float x, float y) { return !lt(x, c->left) && !lt(y, c->top) && lt(x, c->right) && lt(y, c->bottom); };
    if (in(*x1, *y1) && in(*x2, *y2))
        return 1;
    float nx1 = *x1, ny1 = *y1, nx2 = *x2, ny2 = *y2;
    auto outcode = [&](float x, float y) {
        DWORD k = lt(y, c->top) ? 1 : 0;
        if (!lt(x, c->right))
            k |= 2;
        if (!lt(y, c->bottom))
            k |= 4;
        if (lt(x, c->left))
            k |= 8;
        return k;
    };
    const DWORD k1 = outcode(nx1, ny1), k2 = outcode(nx2, ny2);
    if (k1 & k2)
        return 0;
    using Intersect = void(__fastcall *)(const RectF *, float *, float, float, float *, DWORD);
    if (k1) {
        const float ox2 = *x2;
        if (c3(fld(*x1), fld(ox2)))
            ny1 = lt(*y1, c->top) ? c->top : fstp(fld(c->bottom) - cst(glob_addr(C_ONE)));
        else if (const float oy2 = *y2; c3(fld(*y1), fld(oy2)))
            nx1 = lt(*x1, c->left) ? c->left : fstp(fld(c->right) - cst(glob_addr(C_ONE)));
        else
            fn<Intersect>(0x4b3e50)(c, &nx1, ox2, oy2, &ny1, k1);
    }
    if (k2) {
        const float ox1 = fstp(fld(*x1));   // through fld/fst
        if (c3(fld(ox1), fld(*x2)))
            ny2 = lt(*y2, c->top) ? c->top : fstp(fld(c->bottom) - cst(glob_addr(C_ONE)));
        else if (const float oy1 = fstp(fld(*y1)); c3(fld(oy1), fld(*y2)))
            nx2 = lt(*x2, c->left) ? c->left : fstp(fld(c->right) - cst(glob_addr(C_ONE)));
        else
            fn<Intersect>(0x4b3e50)(c, &nx2, ox1, oy1, &ny2, k2);
    }
    if (!in(nx1, ny1) || !in(nx2, ny2))
        return 0;
    *x1 = nx1;
    *y1 = ny1;
    *x2 = nx2;
    *y2 = ny2;
    return 1;
}

namespace {

Rect clip_of(const void *f)
{
    Rect r;
    f_clip_rect(f, &r);
    return r;
}

// 0x4b3280 (src, x, y, mode): the whole source (its slot 36 rectangle) blitted with its top left at (x, y)
// plus its origin: slot 23 (src, its rectangle, the rectangle moved by (x, y), mode); al its result
DWORD THISCALL Frame_BlitAt(CFrame *f, CFrame *src, int x, int y, CBlendMode *mode)
{
    Rect s = clip_of(src), d = {s.left + x, s.top + y, s.right + x, s.bottom + y};
    return vcall<DWORD>(f, 23, src, &s, &d, mode);
}

// 0x4b3300 (src, x, y, clip, mode): the same, both rectangles cut by Blit_ClipRects to `clip` first
DWORD THISCALL Frame_BlitAtClipped(CFrame *f, CFrame *src, int x, int y, const Rect *clip, CBlendMode *mode)
{
    Rect s = clip_of(src), d = {s.left + x, s.top + y, s.right + x, s.bottom + y};
    const DWORD ok = fn<DWORD(__fastcall *)(const Rect *, Rect *, Rect *)>(0x4b2c20)(clip, &s, &d);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(f, 23, src, &s, &d, mode);
}

// 0x4b3390 (src, rect, x, y, mode): a part of the source blitted at (x, y): slot 23 (src, rect itself, rect
// moved by (x, y), mode); slot 23 moves the caller's rect by the source's origin
DWORD THISCALL Frame_BlitRectAt(CFrame *f, CFrame *src, Rect *rect, int x, int y, CBlendMode *mode)
{
    Rect d = {rect->left + x, rect->top + y, rect->right + x, rect->bottom + y};
    return vcall<DWORD>(f, 23, src, rect, &d, mode);
}

// 0x4b3430 (rect, FieldColor): the outline with slot 51: top, bottom - 1, left, right - 1 (each line from
// left/top to right - 1/bottom - 1)
DWORD THISCALL Frame_DrawRect(CFrame *f, const Rect *r, FieldColor *c)
{
    vcall<DWORD>(f, 51, r->left, r->top, r->right - 1, r->top, c);
    vcall<DWORD>(f, 51, r->left, r->bottom - 1, r->right - 1, r->bottom - 1, c);
    vcall<DWORD>(f, 51, r->left, r->top, r->left, r->bottom - 1, c);
    return vcall<DWORD>(f, 51, r->right - 1, r->top, r->right - 1, r->bottom - 1, c);
}

// 0x4b33f0 (x1, y1, x2, y2, FieldColor): 0x4b3430 with the rectangle (x1, y1, x2, y2)
DWORD THISCALL Frame_DrawRectXY(CFrame *f, int x1, int y1, int x2, int y2, FieldColor *c)
{
    Rect r = {x1, y1, x2, y2};
    return fn<DWORD(THISCALL *)(CFrame *, const Rect *, FieldColor *)>(0x4b3430)(f, &r, c);
}

// 0x4b34b0 (rect, light, dark): the outline in two colours: top and left in the first, bottom and right in
// the second (the order of 0x4b3430)
DWORD THISCALL Frame_DrawRectTwoColors(CFrame *f, const Rect *r, FieldColor *light, FieldColor *dark)
{
    vcall<DWORD>(f, 51, r->left, r->top, r->right - 1, r->top, light);
    vcall<DWORD>(f, 51, r->left, r->bottom - 1, r->right - 1, r->bottom - 1, dark);
    vcall<DWORD>(f, 51, r->left, r->top, r->left, r->bottom - 1, light);
    return vcall<DWORD>(f, 51, r->right - 1, r->top, r->right - 1, r->bottom - 1, dark);
}

// 0x4b3a70 (x1, y1, x2, y2, FieldColor): slot 28 (FillRect) with the rectangle
DWORD THISCALL Frame_FillRectXY(CFrame *f, int x1, int y1, int x2, int y2, FieldColor *c)
{
    Rect r = {x1, y1, x2, y2};
    return vcall<DWORD>(f, 28, &r, c);
}

// 0x4b3ce0 (x1, y1, x2, y2, table): slot 57 (TintRect) with the rectangle when x1 <= x2 and y1 <= y2
void THISCALL Frame_TintRectXY(CFrame *f, int x1, int y1, int x2, int y2, BYTE **table)
{
    if (x1 > x2 || y1 > y2)
        return;
    Rect r = {x1, y1, x2, y2};
    vcall<DWORD>(f, 57, &r, table);
}

// 0x4b3c80 / 0x4b3cb0 (p1, p2, colour): slot 51 / slot 52 between two points
DWORD THISCALL Frame_DrawLinePts(CFrame *f, const Point *p1, const Point *p2, FieldColor *c)
{
    return vcall<DWORD>(f, 51, p1->x, p1->y, p2->x, p2->y, c);
}
DWORD THISCALL Frame_DrawThickLinePts(CFrame *f, const Point *p1, const Point *p2, DWORD c)
{
    return vcall<DWORD>(f, 52, p1->x, p1->y, p2->x, p2->y, c);
}

// 0x4b3ab0 (rect, top colour, bottom colour): a vertical gradient, one slot 51 line per row from left to
// right - 1: the channels (b, g, r of +4) as floats from the top colour, each row adding (bottom - top) /
// height (stored as floats), rounded with fistp
void THISCALL EnergyMeter_DrawBar(CFrame *f, const Rect *rect, const FieldColor *top, const FieldColor *bottom)
{
    const long double h = fild64((long long)(DWORD)(rect->bottom - rect->top));
    float r = fstp(fild(top->r2));
    const float dr = fstp(fild((int)bottom->r2 - (int)top->r2) / h);
    float g = fstp(fild(top->g2));
    const float dg = fstp(fild((int)bottom->g2 - (int)top->g2) / h);
    float b = fstp(fild(top->b2));
    const float db = fstp(fild((int)bottom->b2 - (int)top->b2) / h);
    for (int y = rect->top; y < rect->bottom; y++) {
        const int bi = x87_fistp(fld(b)), gi = x87_fistp(fld(g)), ri = x87_fistp(fld(r));
        FieldColor c;
        color_ctor_rgba(&c, (DWORD)ri, (DWORD)gi, (DWORD)bi, 0xff);
        r = fstp(fld(dr) + fld(r));
        g = fstp(fld(dg) + fld(g));
        b = fstp(fld(db) + fld(b));
        vcall<DWORD>(f, 51, rect->left, y, rect->right - 1, y, &c);
    }
}

}  // namespace

ZAX_HOOKS(frame_draw)
{
    const Hook list[] = {
        {0x432c10, (void *)&Mem_FillBytes, "Mem_FillBytes", 8, {0x53, 0x8b, 0xd9, 0x56, 0x8b, 0x74, 0x24, 0x0c}},
        {0x4b2cd0, (void *)&Rect_Intersect, "Rect_Intersect", 7, {0x8b, 0x41, 0x04, 0x56, 0x39, 0x42, 0x04, 0x7d}},
        {0x4b2c20, (void *)&Blit_ClipRects, "Blit_ClipRects", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0x71, 0x04}},
        {0x4b3d30, (void *)&Line_ClipIntersect, "Line_ClipIntersect", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x57}},
        {0x4b3e50, (void *)&Line_ClipIntersectF, "Line_ClipIntersectF", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x14, 0x8a, 0x5c}},
        {0x4b2d30, (void *)&Line_ClipToRect, "Line_ClipToRect", 7, {0x83, 0xec, 0x18, 0x8b, 0x44, 0x24, 0x1c, 0x53}},
        {0x4b2f50, (void *)&Line_ClipToRectF, "Line_ClipToRectF", 5, {0x83, 0xec, 0x14, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x4b3280, ptr(&Frame_BlitAt), "Frame_BlitAt", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4b3300, ptr(&Frame_BlitAtClipped), "Frame_BlitAtClipped", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x4b3390, ptr(&Frame_BlitRectAt), "Frame_BlitRectAt", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x18, 0x53}},
        {0x4b3430, ptr(&Frame_DrawRect), "Frame_DrawRect", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x8b, 0x74}},
        {0x4b33f0, ptr(&Frame_DrawRectXY), "Frame_DrawRectXY", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x8b}},
        {0x4b34b0, ptr(&Frame_DrawRectTwoColors), "Frame_DrawRectTwoColors", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56, 0x8b}},
        {0x4b3a70, ptr(&Frame_FillRectXY), "Frame_FillRectXY", 7, {0x83, 0xec, 0x10, 0x8b, 0x54, 0x24, 0x18, 0x8b}},
        {0x4b3ce0, ptr(&Frame_TintRectXY), "Frame_TintRectXY", 8, {0x83, 0xec, 0x10, 0x56, 0x8b, 0x74, 0x24, 0x18}},
        {0x4b3c80, ptr(&Frame_DrawLinePts), "Frame_DrawLinePts", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x50, 0x8b, 0x44}},
        {0x4b3cb0, ptr(&Frame_DrawThickLinePts), "Frame_DrawThickLinePts", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x50, 0x8b, 0x44}},
        {0x4b3ab0, ptr(&EnergyMeter_DrawBar), "EnergyMeter_DrawBar", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x34, 0x8b, 0x45}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
