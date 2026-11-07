// The polygon rasterizer of CRenderablePolygon (batch R7 of the blitter map, docs/DECOMPILATION.md 9):
// the point lists, the clipping and edge code and the 8- and 16-bit fills of 0x5484a0-0x54d0f0.
//
// The polygon [confirmed: code]: a point list (vtable 0x5fd35c: 0 deleting destructor, 1 SetCount, 2 Clear)
// with an embedded array of 8-byte points at +4 (vtable 0x5f8dc4: +8 data, +0xc count, +0x10 capacity; the
// pool allocator 0x506470/0x5065b0/0x5065e0), +0x14 the render type (0-5), +0x18 its parameter (the texture
// frame of type 5). Points are 12.4 fixed point (PolyPoints_FromFloat 0x549530 multiplies by 16). The
// clipped copies (vtable 0x5fd368) add an array of outcodes at +0x1c (+0x20 data, +0x24 count, +0x28
// capacity); two of them are function-local statics of PolyRaster_Clip (0x711d88 and 0x711ec8, guard bits
// 1 and 2 of 0x711db4).
//
// Drawing (PolyRaster_Draw 0x549050): the polygon clipped to the frame's rectangle (slot 36, origin
// included) by Sutherland-Hodgman with outcodes (left, right, top, bottom; a polygon inside is drawn as it
// is), fewer than 3 points draw nothing; then by the frame's depth and the type: 16 bits 0 black, 1 half
// brightness, 2 blue + 10, 3 green + 10, 4 red + 10 (each field saturated), 5 the texture tiled in screen
// space; 8 bits 0 black (the palette index of the colour 0x6be368, a static), 1-4 through the tables the
// polygon's constructor built (half, blue + 72, green + 72, red + 72 per index), 5 the texture; 24 bits
// the 24-bit fills (raster_rest.cpp). Every fill draws the triangle fan (0, i, i + 1); a triangle's
// edges are set up for ceil'd rows with an exact fraction (PolyEdge_Setup 0x54d040) and stepped per row
// (PolyEdge_Step 0x54d0f0); a span covers [left x, right x) of the row, minus the origin.
// MMX: types 2-4 of 16 bits have MMX twins (picked by 0x7121f4, the constructor's Cpu_HasMmx): groups of
// four pixels by paddusw on the field shifted to the top of the word, the rest scalar. They differ from the
// scalar fills when a field saturates: green gives 0x7ff (blue bits set) instead of 0x7e0, red 0xffff
// instead of 0xf800 | the other fields (blue saturates the same way in both).
#include "frame.h"

namespace zax {

using namespace render;

namespace {

struct Pt {
    int x, y;
};
struct PtArray {   // vtable 0x5f8dc4 / 0x5f8db8
    DWORD vftable;
    Pt *data;
    DWORD count, capacity;
};
struct CodeArray {   // vtable 0x5ee15c / 0x5ee138
    DWORD vftable;
    DWORD *data;
    DWORD count, capacity;
};
struct PolyPoints {   // vtable 0x5fd35c
    DWORD vftable;
    PtArray pts;      // 0x04
    DWORD type;       // 0x14
    void *param;      // 0x18
};
struct PolyClipped {   // vtable 0x5fd368
    PolyPoints base;
    CodeArray codes;   // 0x1c
};
static_assert(sizeof(PolyClipped) == 0x2c);

// The edge of a triangle (0x1c): x, the step, the remainder step, the denominator, the remainder, the row,
// the rows left
struct Edge {
    int x, step, rstep, den, rem, y, height;
};

constexpr DWORD VT_POINTS = 0x5fd35c, VT_CLIPPED = 0x5fd368, VT_PT_ARRAY = 0x5f8dc4, VT_PT_ARRAY_BASE = 0x5f8db8, VT_CODE_ARRAY = 0x5ee138,
                VT_CODE_ARRAY_BASE = 0x5ee15c;
constexpr DWORD MMX_FLAG = 0x7121f4, TABLES_DONE = 0x7121f3, TABLE_HALF = 0x7120f4, TABLE_BLUE = 0x711ff4, TABLE_RED = 0x711db8,
                TABLE_GREEN = 0x711ef4;
constexpr DWORD CLIP_A = 0x711ec8, CLIP_B = 0x711d88, CLIP_GUARD = 0x711db4;
constexpr DWORD ADD_BLUE = 0x61eacc, ADD_RED = 0x61ead0, ADD_GREEN = 0x61ead4;
constexpr DWORD BLACK_GUARD = 0x7160b8, BLACK8 = 0x7160bc, BLACK_COLOR = 0x6be368;

// The pool: 0x506470 the global allocator, 0x5065b0 (pool; size), 0x5065e0 (pool; block, size)
inline void *pool() { return zax::fn<void *(__cdecl *)()>(0x506470)(); }
inline void *pool_alloc(DWORD n) { return zax::fn<void *(THISCALL *)(void *, DWORD)>(0x5065b0)(pool(), n); }
inline void pool_free(void *p, DWORD n) { zax::fn<void(THISCALL *)(void *, void *, DWORD)>(0x5065e0)(pool(), p, n); }
inline void array_append(void *array, const void *v) { vcall<void>(array, 2, v); }

// ---- The point lists ---------------------------------------------------------------------------------------------

// 0x5484a0 (thiscall this): the constructor; the first time (0x7121f3 clear) the four 8-bit tint tables
// from the game palette
PolyPoints *THISCALL PolyPoints_Ctor(PolyPoints *p)
{
    p->pts.data = nullptr;
    p->pts.count = 0;
    p->pts.capacity = 0;
    p->pts.vftable = vt(VT_PT_ARRAY);
    p->vftable = vt(VT_POINTS);
    p->type = 0;
    *glob<BYTE *>(MMX_FLAG) = cpu_has_mmx();
    if (*glob<BYTE *>(TABLES_DONE))
        return p;
    Palette *pal = zax::fn<Palette *(__cdecl *)()>(0x433a80)();
    for (DWORD i = 0; i < 0x100; i++) {
        DWORD r, g, b;
        zax::fn<void(THISCALL *)(Palette *, DWORD, DWORD *, DWORD *, DWORD *)>(0x527000)(pal, i, &r, &g, &b);
        glob<BYTE *>(TABLE_HALF)[i] = (BYTE)palette_nearest(pal, r >> 1, g >> 1, b >> 1, 1, 0xff);
        glob<BYTE *>(TABLE_BLUE)[i] = (BYTE)palette_nearest(pal, r, g, b + 0x48 < 0xff ? b + 0x48 : 0xff, 1, 0xff);
        glob<BYTE *>(TABLE_RED)[i] = (BYTE)palette_nearest(pal, r + 0x48 < 0xff ? r + 0x48 : 0xff, g, b, 1, 0xff);
        glob<BYTE *>(TABLE_GREEN)[i] = (BYTE)palette_nearest(pal, r, g + 0x48 < 0xff ? g + 0x48 : 0xff, b, 1, 0xff);
    }
    return p;
}

void free_points(PolyPoints *p)
{
    p->vftable = vt(VT_POINTS);
    p->pts.vftable = vt(VT_PT_ARRAY_BASE);
    if (p->pts.data) {
        pool_free(p->pts.data, p->pts.capacity * 8);
        p->pts.data = nullptr;
        p->pts.capacity = 0;
    }
    p->pts.count = 0;
}

// 0x5485e0 (thiscall this; flags), 0x548640 (the array freed)
PolyPoints *THISCALL PolyPoints_DeletingDtor(PolyPoints *p, DWORD flags)
{
    free_points(p);
    if (flags & 1)
        zax::fn<void(__cdecl *)(void *)>(0x5d0320)(p);
    return p;
}
void THISCALL PolyPoints_Free(PolyPoints *p) { free_points(p); }

// 0x548680 (thiscall this; n): the point count raised to n (never lowered), the array grown to exactly n
void THISCALL PolyPoints_SetCount(PolyPoints *p, DWORD n)
{
    if (p->pts.count >= n)
        return;
    const DWORD add = n - p->pts.count, old_cap = p->pts.capacity;
    if (old_cap < p->pts.count + add) {
        p->pts.capacity = p->pts.count + add;
        Pt *d = static_cast<Pt *>(pool_alloc(p->pts.capacity * 8));
        if (p->pts.count)
            zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, p->pts.data, p->pts.count * 8);
        if (p->pts.data) {
            pool_free(p->pts.data, old_cap * 8);
            p->pts.data = nullptr;
        }
        p->pts.data = d;
    }
    p->pts.count += add;
}

// 0x548710 (thiscall this; x, y): a point appended (array slot 2)
void THISCALL PolyPoints_Add(PolyPoints *p, int x, int y)
{
    const Pt pt = {x, y};
    array_append(&p->pts, &pt);
}
// 0x548740: slot 2, the points removed (array slot 1)
void THISCALL PolyPoints_Clear(PolyPoints *p) { vcall<void>(&p->pts, 1); }

// 0x548750 (thiscall this; flags): the clipped list's deleting destructor
PolyClipped *THISCALL PolyClipped_DeletingDtor(PolyClipped *p, DWORD flags)
{
    p->base.vftable = vt(VT_CLIPPED);
    p->codes.vftable = vt(VT_CODE_ARRAY_BASE);
    if (p->codes.data) {
        pool_free(p->codes.data, p->codes.capacity * 4);
        p->codes.data = nullptr;
        p->codes.capacity = 0;
    }
    p->codes.count = 0;
    free_points(&p->base);
    if (flags & 1)
        zax::fn<void(__cdecl *)(void *)>(0x5d0320)(p);
    return p;
}

// 0x5487d0 (thiscall this; n): both counts raised to n
void THISCALL PolyClipped_SetCount(PolyClipped *p, DWORD n)
{
    if (p->codes.count < n) {
        const DWORD add = n - p->codes.count, old_cap = p->codes.capacity;
        if (old_cap < p->codes.count + add) {
            p->codes.capacity = p->codes.count + add;
            DWORD *d = static_cast<DWORD *>(pool_alloc(p->codes.capacity * 4));
            if (p->codes.count)
                zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, p->codes.data, p->codes.count * 4);
            if (p->codes.data) {
                pool_free(p->codes.data, old_cap * 4);
                p->codes.data = nullptr;
            }
            p->codes.data = d;
        }
        p->codes.count += add;
    }
    PolyPoints_SetCount(&p->base, n);
}

// 0x5488c0 (thiscall this; x, y, code): a point and its outcode appended
void THISCALL PolyClipped_Add(PolyClipped *p, int x, int y, DWORD code)
{
    const Pt pt = {x, y};
    array_append(&p->base.pts, &pt);
    array_append(&p->codes, &code);
}
// 0x548900: slot 2, both arrays emptied
void THISCALL PolyClipped_Clear(PolyClipped *p)
{
    vcall<void>(&p->codes, 1);
    vcall<void>(&p->base.pts, 1);
}

// 0x54cf50 (thiscall array; points, n): the array freed, then exactly n points copied in
void THISCALL PtArray_Assign(PtArray *a, const Pt *pts, DWORD n)
{
    if (a->data) {
        pool_free(a->data, a->capacity * 8);
        a->data = nullptr;
        a->capacity = 0;
    }
    a->count = 0;
    if (a->capacity < n) {
        DWORD cap = n;
        const DWORD grown = (DWORD)x87_ftol(fild64((long long)(unsigned long long)a->capacity) * cst(glob_addr(0x5ea5f4)));
        if (grown > n)
            cap = grown;
        if (a->capacity < cap) {
            const DWORD old = a->capacity;
            a->capacity = cap;
            Pt *d = static_cast<Pt *>(pool_alloc(cap * 8));
            if (a->count)
                zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, a->data, a->count * 8);
            if (a->data) {
                pool_free(a->data, old * 8);
                a->data = nullptr;
            }
            a->data = d;
        }
    }
    if (n)
        memcpy(a->data, pts, n * 8);
    a->count = n;
}

// 0x549530 (thiscall this; float polygon {points, ?, count}): the list made of the float points times 16
// (truncated)
void THISCALL PolyPoints_FromFloat(PolyPoints *p, const void *poly)
{
    vcall<void>(p, 2);
    const DWORD n = static_cast<const DWORD *>(poly)[2];
    const float *pts = *static_cast<float *const *>(poly);
    for (DWORD i = 0; i < n; i++) {
        const int x = x87_ftol(fld(pts[2 * i]) * cst(glob_addr(0x5eecd4)));
        const int y = x87_ftol(fld(pts[2 * i + 1]) * cst(glob_addr(0x5eecd4)));
        const Pt pt = {x, y};
        array_append(&p->pts, &pt);
    }
}

// ---- Clipping ------------------------------------------------------------------------------------------------------

// 0x548b60 (fastcall point, &code; rect): left 1, right 2, top 4, bottom 8 against the rectangle << 4
void __fastcall Poly_Outcode(const Pt *p, DWORD *code, const Rect *r)
{
    *code = 0;
    if (p->x < r->left << 4)
        *code = 1;
    if (p->x > r->right << 4)
        *code |= 2;
    if (p->y < r->top << 4)
        *code |= 4;
    if (p->y > r->bottom << 4)
        *code |= 8;
}

// The crossing of edge i -> j with the line (axis 0: x = value, 1: y = value) and its outcode
DWORD crossing(const Pt *pts, DWORD i, DWORD j, DWORD axis, DWORD other_y, int value, const Rect *r, Pt *out)
{
    const int a_i = axis ? pts[i].y : pts[i].x, b_i = other_y ? pts[i].y : pts[i].x;
    const int da = (axis ? pts[j].y : pts[j].x) - a_i, db = (other_y ? pts[j].y : pts[j].x) - b_i;
    int c[2];
    c[axis ? 1 : 0] = value;
    c[other_y ? 1 : 0] = zax::fn<int(__fastcall *)(int, int, int)>(0x4d2640)(db, value - a_i, da) + b_i;
    out->x = c[0];
    out->y = c[1];
    DWORD code = 0;
    if (c[0] < r->left << 4)
        code = 1;
    if (c[0] > r->right << 4)
        code |= 2;
    if (c[1] < r->top << 4)
        code |= 4;
    if (c[1] > r->bottom << 4)
        code |= 8;
    return code;
}

// 0x548da0 (fastcall src, i; j, axis, other is y, bit, value, dst, rect): the closing edge i -> j
DWORD __fastcall PolyClip_Closing(PolyClipped *src, DWORD i, DWORD j, DWORD axis, DWORD other_y, DWORD bit, int value, PolyClipped *dst,
                                  const Rect *r)
{
    DWORD acc = 0;
    const DWORD ci = src->codes.data[i], cj = src->codes.data[j], mi = ci & bit, mj = cj & bit;
    if (mi & mj)
        return ci;   // the register the original returns holds the first point's code
    const Pt *pts = src->base.pts.data;
    if (!mi) {
        DWORD code = ci;
        PolyPoints_Add(&dst->base, pts[i].x, pts[i].y);
        array_append(&dst->codes, &code);
        acc = src->codes.data[i];
        if (!mj)
            return acc;
    }
    Pt c;
    DWORD code = crossing(pts, i, j, axis, other_y, value, r, &c);
    array_append(&dst->base.pts, &c);
    array_append(&dst->codes, &code);
    return acc | code;
}

// 0x548bc0 (fastcall src, axis; other is y, bit, value, dst, rect): one clip plane; the OR of the new codes
DWORD __fastcall PolyClip_Plane(PolyClipped *src, DWORD axis, DWORD other_y, DWORD bit, int value, PolyClipped *dst, const Rect *r)
{
    DWORD acc = 0;
    vcall<void>(dst, 2);
    if (!src->base.pts.count)
        return 0;
    DWORD i = 0;
    for (; i + 1 < src->base.pts.count; i++) {
        // a pair outside the plane adds its first point's whole code (the original ORs the register it
        // loaded the code into)
        DWORD codes = 0;
        const DWORD ci = src->codes.data[i], mi = ci & bit, mj = src->codes.data[i + 1] & bit;
        if (mi & mj) {
            codes = ci;
        } else {
            const Pt *pts = src->base.pts.data;
            if (!mi) {
                PolyClipped_Add(dst, pts[i].x, pts[i].y, ci);
                codes = src->codes.data[i];
            }
            if (mi || mj) {
                Pt c;
                const DWORD code = crossing(pts, i, i + 1, axis, other_y, value, r, &c);
                PolyPoints_Add(&dst->base, c.x, c.y);
                array_append(&dst->codes, &code);
                codes |= code;
            }
        }
        acc |= codes;
    }
    return zax::fn<DWORD(__fastcall *)(PolyClipped *, DWORD, DWORD, DWORD, DWORD, DWORD, int, PolyClipped *, const Rect *)>(0x548da0)(
               src, i, 0, axis, other_y, bit, value, dst, r) |
           acc;
}

void init_clip_list(DWORD at_addr, DWORD exit_fn)
{
    zax::fn<PolyPoints *(THISCALL *)(void *)>(0x5484a0)(at<void *>(at_addr));
    zax::fn<void(THISCALL *)(void *)>(0x535440)(at<void *>(at_addr + 0x1c));
    *at<DWORD *>(at_addr + 0x1c) = vt(VT_CODE_ARRAY);
    *at<DWORD *>(at_addr) = vt(VT_CLIPPED);
    zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(exit_fn));
}

// 0x548920 (thiscall polygon; rect): the polygon itself when every point lies inside, else one of the two
// static lists with the clipped polygon
PolyPoints *THISCALL PolyRaster_Clip(PolyPoints *poly, const Rect *r)
{
    BYTE &guard = *glob<BYTE *>(CLIP_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        init_clip_list(glob_addr(CLIP_B), fn_addr(0x548fb0));
    }
    if (!(guard & 2)) {
        guard |= 2;
        init_clip_list(glob_addr(CLIP_A), fn_addr(0x548f10));
    }
    PolyClipped *a = glob<PolyClipped *>(CLIP_A), *b = glob<PolyClipped *>(CLIP_B);
    const DWORD n = poly->pts.count;
    if (b->codes.count < n) {
        const DWORD add = n - b->codes.count;
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x5a4080)(&b->codes, b->codes.count + add);
        b->codes.count += add;
    }
    zax::fn<void(THISCALL *)(PolyPoints *, DWORD)>(0x548680)(&b->base, n);
    DWORD all = 0;
    for (DWORD i = 0; i < n; i++) {
        zax::fn<void(__fastcall *)(const Pt *, DWORD *, const Rect *)>(0x548b60)(poly->pts.data + i, b->codes.data + i, r);
        all |= b->codes.data[i];
    }
    if (!all)
        return poly;
    b->base.type = a->base.type = poly->type;
    b->base.param = a->base.param = poly->param;
    zax::fn<void(THISCALL *)(PtArray *, const Pt *, DWORD)>(0x54cf50)(&b->base.pts, poly->pts.data, n);
    using Plane = DWORD(__fastcall *)(PolyClipped *, DWORD, DWORD, DWORD, int, PolyClipped *, const Rect *);
    const Plane plane = zax::code<Plane>(0x548bc0);
    PolyClipped *src = b, *dst = a;
    if (all & 1) {
        all = plane(b, 0, 1, 1, r->left << 4, a, r);
        if (!all)
            return &a->base;
        src = a;
        dst = b;
    }
    if (all & 4) {
        all = plane(src, 1, 0, 4, r->top << 4, dst, r);
        if (!all)
            return &dst->base;
        PolyClipped *t = src;
        src = dst;
        dst = t;
    }
    if (all & 2) {
        plane(src, 0, 1, 2, r->right << 4, dst, r);   // the result is not looked at (the original tests the old codes)
        PolyClipped *t = src;
        src = dst;
        dst = t;
    }
    if (all & 8) {
        plane(src, 1, 0, 8, r->bottom << 4, dst, r);
        return &dst->base;
    }
    return &src->base;
}

// 0x548f10, 0x548fb0: the static lists' atexit destructors
void free_clip_list(DWORD a)
{
    PolyClipped *p = at<PolyClipped *>(a);
    p->base.vftable = vt(VT_CLIPPED);
    p->codes.vftable = vt(VT_CODE_ARRAY_BASE);
    if (p->codes.data) {
        pool_free(p->codes.data, p->codes.capacity * 4);
        p->codes.data = nullptr;
        p->codes.capacity = 0;
    }
    p->codes.count = 0;
    free_points(&p->base);
}
void __cdecl StaticExit_PolyClipA() { free_clip_list(glob_addr(CLIP_A)); }
void __cdecl StaticExit_PolyClipB() { free_clip_list(glob_addr(CLIP_B)); }

// ---- The edges ----------------------------------------------------------------------------------------------------

// 0x54d040 (fastcall edge; points, i, j): the edge i -> j from row ceil(y_i / 16) for ceil(y_j / 16) minus
// that rows; x and its step with floor division and the remainders
Edge *__fastcall PolyEdge_Setup(Edge *e, DWORD, const Pt *pts, DWORD i, DWORD j)
{
    const int yi = pts[i].y, yj = pts[j].y;
    const int top = (yi >> 4) + ((yi & 15) ? 1 : 0);
    e->y = top;
    const int rows = (yj >> 4) + ((yj & 15) ? 1 : 0) - top;
    e->height = rows;
    if (!rows)
        return e;
    const int dx = pts[j].x - pts[i].x, dy = yj - yi;
    const int num = (int)(((DWORD)top * (DWORD)dx + (DWORD)dy) << 4) - (int)((DWORD)yi * (DWORD)dx) + (int)((DWORD)pts[i].x * (DWORD)dy) - 1;
    const int den = (int)((DWORD)dy << 4);
    e->x = num / den;
    e->rem = num - e->x * den;
    if (e->rem < 0) {
        e->x--;
        e->rem += den;
    }
    const int sx = (int)((DWORD)dx << 4);
    e->step = sx / den;
    e->rstep = sx - e->step * den;
    if (e->rstep < 0) {
        e->step--;
        e->rstep += den;
    }
    e->den = den;
    return e;
}

// 0x54d0f0 (fastcall edge): one row down
void __fastcall PolyEdge_Step(Edge *e)
{
    e->x += e->step;
    e->y++;
    e->height--;
    e->rem += e->rstep;
    if (e->rem >= e->den) {
        e->x++;
        e->rem -= e->den;
    }
}

// ---- The triangles ---------------------------------------------------------------------------------------------------

// The span of a row: [start, end) in the frame's pixels, xl the absolute left x, y the row
struct Span {
    BYTE *start, *end;
    int xl, y;
};

// Triangle (a, b, c) of the points: sorted by y, the three edges, then the two halves; `fill(Span)` per
// non-empty row; `n` bytes per pixel
template <int N, typename F> void triangle(void *frame, const Pt *pts, DWORD a, DWORD b, DWORD c, F fill)
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
    using Setup = Edge *(__fastcall *)(Edge *, DWORD, const Pt *, DWORD, DWORD);
    Edge e1, e2, e3;
    zax::fn<Setup>(0x54d040)(&e1, 0, pts, top, bot);
    zax::fn<Setup>(0x54d040)(&e2, 0, pts, top, mid);
    zax::fn<Setup>(0x54d040)(&e3, 0, pts, mid, bot);
    const int cross = (int)((DWORD)(pts[bot].y - pts[top].y) * (DWORD)(pts[mid].x - pts[top].x) -
                            (DWORD)(pts[mid].y - pts[top].y) * (DWORD)(pts[bot].x - pts[top].x));
    auto half = [&](Edge *l, Edge *r, int rows) {
        const int ox = vcall<int>(frame, 43), oy = vcall<int>(frame, 44);
        const DWORD *row = f_rows(frame) + (l->y - oy);
        BYTE *pixels = f_pixels(frame);
        for (; rows; rows--, row++) {
            if (l->x < r->x)
                fill(Span{pixels + (*row + (DWORD)(l->x - ox)) * N, pixels + (*row + (DWORD)(r->x - ox)) * N, l->x, l->y});
            zax::fn<void(__fastcall *)(Edge *)>(0x54d0f0)(l);
            zax::fn<void(__fastcall *)(Edge *)>(0x54d0f0)(r);
        }
    };
    if (cross >= 0)
        half(&e1, &e2, e2.height);
    else
        half(&e2, &e1, e2.height);
    if (cross < 0)
        half(&e3, &e1, e3.height);
    else
        half(&e1, &e3, e3.height);
}

// The 16-bit field operations
inline void add_field(BYTE *p, DWORD k, DWORD field, DWORD keep)
{
    WORD *w = reinterpret_cast<WORD *>(p);
    DWORD v = (*w & field) + k;
    if (v >= field)
        v = field;
    *w = (WORD)((*w & keep) | v);
}
void scalar_blue(const Span &s)
{
    for (BYTE *p = s.start; p < s.end; p += 2) {
        WORD *w = reinterpret_cast<WORD *>(p);
        DWORD v = (*w & 0x1f) + *glob<DWORD *>(ADD_BLUE);
        if (v >= 0x1f)
            v = 0x1f;
        *w = (WORD)((*w & 0xffe0) | v);
    }
}
void scalar_green(BYTE *p, BYTE *end)
{
    for (; p < end; p += 2)
        add_field(p, *glob<DWORD *>(ADD_GREEN), 0x7e0, 0xf81f);
}
void scalar_red(BYTE *p, BYTE *end)
{
    for (; p < end; p += 2)
        add_field(p, *glob<DWORD *>(ADD_RED), 0xf800, 0x7ff);
}

// The MMX groups: four words per qword: ((q & field) << shift) +us (k << shift) >> shift, or (q & keep)
template <int SHIFT> void mmx_groups(BYTE *p, DWORD groups, DWORD k, DWORD field_mask, DWORD keep_mask)
{
    __asm__ volatile("movd %[k], %%mm1\n\t"
                     "punpcklwd %%mm1, %%mm1\n\t"
                     "punpckldq %%mm1, %%mm1\n\t"
                     ".if %c[s]\n\t"
                     "psllw %[s], %%mm1\n\t"
                     ".endif\n"
                     "1:\n\t"
                     "movq (%[p]), %%mm2\n\t"
                     "movq %%mm2, %%mm3\n\t"
                     "addl $8, %[p]\n\t"
                     "pand (%[fm]), %%mm3\n\t"
                     "pand (%[km]), %%mm2\n\t"
                     ".if %c[s]\n\t"
                     "psllw %[s], %%mm3\n\t"
                     ".endif\n\t"
                     "paddusw %%mm1, %%mm3\n\t"
                     ".if %c[s]\n\t"
                     "psrlw %[s], %%mm3\n\t"
                     ".endif\n\t"
                     "por %%mm3, %%mm2\n\t"
                     "movq %%mm2, -8(%[p])\n\t"
                     "decl %[n]\n\t"
                     "jnz 1b\n\t"
                     "emms"
                     : [p] "+r"(p), [n] "+r"(groups)
                     : [k] "r"(k), [s] "i"(SHIFT), [fm] "r"(field_mask), [km] "r"(keep_mask)
                     : "memory", "mm1", "mm2", "mm3", "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
}

// ---- The fills (fastcall frame, 0; points, a, b, c, parameter; ret 0x14) ------------------------------------------

using Fan = void(__fastcall *)(void *, DWORD, const Pt *, DWORD, DWORD, DWORD, DWORD);
// The 24-bit fills by type (raster_rest.cpp)
constexpr CodeKey FILL24[6] = {0x549d50, 0x549fe0, 0x54a200, 0x54a640, 0x54a420, 0x54a860};

// 16 bits: 0x54b600 black, 0x54b870 half brightness, 0x54bee0 / 0x54bb70 blue, 0x54c4e0 / 0x54c180 red
void __fastcall PolyFill16_Black(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<2>(f, pts, a, b, c, [](const Span &s) { Mem_FillBytes(s.start, 0, (DWORD)(s.end - s.start)); });
}
void __fastcall PolyFill16_Half(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<2>(f, pts, a, b, c, [](const Span &s) {
        for (BYTE *p = s.start; p < s.end; p += 2) {
            WORD *w = reinterpret_cast<WORD *>(p);
            *w = (WORD)(*w >> 1 & 0x7bef);
        }
    });
}
void __fastcall PolyFill16_Blue(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<2>(f, pts, a, b, c, [](const Span &s) { scalar_blue(s); });
}
void __fastcall PolyFill16_Red(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<2>(f, pts, a, b, c, [](const Span &s) { scalar_red(s.start, s.end); });
}
// The MMX twins: whole groups of four pixels by MMX, the rest of the span scalar
void __fastcall PolyFill16_Blue_MMX(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<2>(f, pts, a, b, c, [](const Span &s) {
        const DWORD groups = (DWORD)(s.end - s.start) >> 3;
        BYTE *p = s.start;
        if (groups) {
            mmx_groups<11>(p, groups, *glob<DWORD *>(ADD_BLUE), glob_addr(0x61eaf0), glob_addr(0x61eae8));
            p += groups * 8;
        }
        scalar_blue(Span{p, s.end, 0, 0});
    });
}
void __fastcall PolyFill16_Red_MMX(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<2>(f, pts, a, b, c, [](const Span &s) {
        const DWORD groups = (DWORD)(s.end - s.start) >> 3;
        BYTE *p = s.start;
        if (groups) {
            mmx_groups<0>(p, groups, *glob<DWORD *>(ADD_RED), glob_addr(0x61eb00), glob_addr(0x61eaf8));
            p += groups * 8;
        }
        scalar_red(p, s.end);
    });
}
void green_mmx_span(const Span &s)
{
    const DWORD groups = (DWORD)(s.end - s.start) >> 3;
    BYTE *p = s.start;
    if (groups) {
        mmx_groups<5>(p, groups, *glob<DWORD *>(ADD_GREEN), glob_addr(0x61eae0), glob_addr(0x61ead8));
        p += groups * 8;
    }
    scalar_green(p, s.end);
}

// 16 bits, the whole polygon (fastcall polygon, frame; parameter, 0; ret 8): 0x549870 green, 0x5495a0 its
// MMX twin; 0x549aa0 (the tiled texture) stays the exe's
template <typename F> void whole(const PolyPoints *poly, F fan)
{
    for (DWORD i = 1; i + 1 < poly->pts.count; i++)
        fan(poly->pts.data, i);
}
void __fastcall PolyFill16_Green(const PolyPoints *poly, void *f, DWORD, DWORD)
{
    whole(poly, [&](const Pt *pts, DWORD i) {
        triangle<2>(f, pts, 0, i, i + 1, [](const Span &s) { scalar_green(s.start, s.end); });
    });
}
void __fastcall PolyFill16_Green_MMX(const PolyPoints *poly, void *f, DWORD, DWORD)
{
    whole(poly, [&](const Pt *pts, DWORD i) { triangle<2>(f, pts, 0, i, i + 1, &green_mmx_span); });
}

// 8 bits: 0x54ab60 black (the colour 0x6be368's index, made once into the dword 0x7160bc), 0x54afc0 through
// a table (the parameter); 0x54b230 (the tiled texture) stays the exe's
void __fastcall PolyFill8_Black(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, DWORD)
{
    triangle<1>(f, pts, a, b, c, [](const Span &s) {
        BYTE &guard = *glob<BYTE *>(BLACK_GUARD);
        if (!(guard & 1)) {
            guard |= 1;
            FieldColor *col = glob<FieldColor *>(BLACK_COLOR);
            DWORD idx[4];
            for (DWORD &i : idx)
                i = col->flags & 1 ? col->index : color_index(col);
            *glob<DWORD *>(BLACK8) = idx[0] | idx[1] << 8 | idx[2] << 16 | idx[3] << 24;
        }
        Mem_FillBytes(s.start, *glob<DWORD *>(BLACK8), (DWORD)(s.end - s.start));
    });
}
void __fastcall PolyFill8_Table(void *f, DWORD, const Pt *pts, DWORD a, DWORD b, DWORD c, const BYTE *table)
{
    triangle<1>(f, pts, a, b, c, [&](const Span &s) {
        for (BYTE *p = s.start; p < s.end; p++)
            *p = table[*p];
    });
}

// ---- 0x549050 Draw (thiscall polygon; frame) ----------------------------------------------------------------------

void THISCALL PolyRaster_Draw(PolyPoints *poly, void *f)
{
    Rect r;
    f_clip_rect(f, &r);
    PolyPoints *c = zax::fn<PolyPoints *(THISCALL *)(PolyPoints *, Rect *)>(0x548920)(poly, &r);
    if (c->pts.count < 3)
        return;
    const DWORD bits = f_bits(f);
    const bool mmx = *glob<BYTE *>(MMX_FLAG) != 0;
    auto fan = [&](CodeKey fn, DWORD edx, DWORD param) {
        for (DWORD i = 1; i + 1 < c->pts.count; i++)
            code<Fan>(fn)(f, edx, c->pts.data, 0, i, i + 1, param);
    };
    if (bits == 8) {
        switch (poly->type) {
        case 0: fan(0x54ab60, 0, 0); break;
        case 1: fan(0x54afc0, 0, glob_addr(TABLE_HALF)); break;
        case 2: fan(0x54afc0, 0, glob_addr(TABLE_BLUE)); break;
        case 3: fan(0x54afc0, 0, glob_addr(TABLE_GREEN)); break;
        case 4: fan(0x54afc0, 0, glob_addr(TABLE_RED)); break;
        case 5: fan(0x54b230, (DWORD)(uintptr_t)poly->param, 0); break;
        }
    } else if (bits == 16) {
        using Whole = void(__fastcall *)(const PolyPoints *, void *, DWORD, DWORD);
        switch (poly->type) {
        case 0: fan(0x54b600, 0, 0); break;
        case 1: fan(0x54b870, 0, 0); break;
        case 2: fan(mmx ? CodeKey(0x54bb70) : CodeKey(0x54bee0), 0, 0); break;
        case 3: code<Whole>(mmx ? CodeKey(0x5495a0) : CodeKey(0x549870))(c, f, 0, 0); break;
        case 4: fan(mmx ? CodeKey(0x54c180) : CodeKey(0x54c4e0), 0, 0); break;
        case 5: zax::fn<Whole>(0x549aa0)(c, f, (DWORD)(uintptr_t)poly->param, 0); break;
        }
    } else if (bits == 24 && poly->type <= 5) {
        code<void(__fastcall *)(const PolyPoints *, void *, DWORD, DWORD)>(FILL24[poly->type])(c, f, poly->type == 5 ? (DWORD)(uintptr_t)poly->param : 0, 0);
    }
}

}  // namespace

ZAX_HOOKS(raster_poly)
{
    const Hook list[] = {
        {0x5484a0, (void *)&PolyPoints_Ctor, "PolyPoints_Ctor", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x8b, 0xe9, 0x33}},
        {0x5485e0, (void *)&PolyPoints_DeletingDtor, "PolyPoints_DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x5c, 0xd3, 0x5f}},
        {0x548640, (void *)&PolyPoints_Free, "PolyPoints_Free", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0xc7, 0x06}},
        {0x548680, (void *)&PolyPoints_SetCount, "PolyPoints_SetCount", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x8b}},
        {0x548710, (void *)&PolyPoints_Add, "PolyPoints_Add", 7, {0x83, 0xec, 0x08, 0x8b, 0x54, 0x24, 0x10, 0x8b}},
        {0x548740, (void *)&PolyPoints_Clear, "PolyPoints_Clear", 6, {0x8b, 0x41, 0x04, 0x83, 0xc1, 0x04, 0xff, 0x60}},
        {0x548750, (void *)&PolyClipped_DeletingDtor, "PolyClipped_DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0xc7, 0x06}},
        {0x5487d0, (void *)&PolyClipped_SetCount, "PolyClipped_SetCount", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c}},
        {0x5488c0, (void *)&PolyClipped_Add, "PolyClipped_Add", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x0c, 0x56}},
        {0x548900, (void *)&PolyClipped_Clear, "PolyClipped_Clear", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x1c, 0x8d, 0x4e}},
        {0x548920, (void *)&PolyRaster_Clip, "PolyRaster_Clip", 5, {0xa0, 0xb4, 0x1d, 0x71, 0x00, 0x83, 0xec, 0x08}},
        {0x548b60, (void *)&Poly_Outcode, "Poly_Outcode", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x55, 0x56, 0x8b}},
        {0x548bc0, (void *)&PolyClip_Plane, "PolyClip_Plane", 5, {0x83, 0xec, 0x18, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x548da0, (void *)&PolyClip_Closing, "PolyClip_Closing", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x548f10, (void *)&StaticExit_PolyClipA, "StaticExit_PolyClipA", 6, {0x8b, 0x0d, 0xe8, 0x1e, 0x71, 0x00, 0x56, 0x33}},
        {0x548fb0, (void *)&StaticExit_PolyClipB, "StaticExit_PolyClipB", 6, {0x8b, 0x0d, 0xa8, 0x1d, 0x71, 0x00, 0x56, 0x33}},
        {0x549050, (void *)&PolyRaster_Draw, "PolyRaster_Draw", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x549530, (void *)&PolyPoints_FromFloat, "PolyPoints_FromFloat", 6, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0xd9, 0x56, 0x8b}},
        {0x5495a0, (void *)&PolyFill16_Green_MMX, "PolyFill16_Green_MMX", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x88, 0x00, 0x00}},
        {0x549870, (void *)&PolyFill16_Green, "PolyFill16_Green", 7, {0x83, 0xec, 0x74, 0x89, 0x54, 0x24, 0x14, 0x89}},
        {0x54ab60, (void *)&PolyFill8_Black, "PolyFill8_Black", 7, {0x83, 0xec, 0x68, 0x8b, 0x44, 0x24, 0x78, 0x53}},
        {0x54afc0, (void *)&PolyFill8_Table, "PolyFill8_Table", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x68, 0x53}},
        {0x54b600, (void *)&PolyFill16_Black, "PolyFill16_Black", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x68, 0x53}},
        {0x54b870, (void *)&PolyFill16_Half, "PolyFill16_Half", 7, {0x83, 0xec, 0x5c, 0x8b, 0x44, 0x24, 0x6c, 0x53}},
        {0x54bb70, (void *)&PolyFill16_Blue_MMX, "PolyFill16_Blue_MMX", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x6c, 0x53, 0x56}},
        {0x54bee0, (void *)&PolyFill16_Blue, "PolyFill16_Blue", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x68, 0x53}},
        {0x54c180, (void *)&PolyFill16_Red_MMX, "PolyFill16_Red_MMX", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x6c, 0x53, 0x56}},
        {0x54c4e0, (void *)&PolyFill16_Red, "PolyFill16_Red", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x68, 0x53}},
        {0x54cf50, (void *)&PtArray_Assign, "PtArray_Assign", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0xd9, 0x56}},
        {0x54d040, (void *)&PolyEdge_Setup, "PolyEdge_Setup", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56, 0x8b}},
        {0x54d0f0, (void *)&PolyEdge_Step, "PolyEdge_Step", 6, {0x8b, 0x41, 0x04, 0x8b, 0x51, 0x14, 0x56, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
