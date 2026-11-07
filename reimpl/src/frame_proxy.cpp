// The proxy frames (layouts in frame.h): CFrameInterface's class and destructor, CProxyFrame (0x40b2c0-
// 0x40b5a0, 0x54fba0-0x54fdb7), COffsetClippedProxyFrame (0x524450-0x524fef), its offset frame
// (0x525000-0x525374) and its clip frame (0x46d790-0x46dbf5) [confirmed: code].
//
// Quirks kept [confirmed: code]:
//  - The offset frame's GetColor (slot 53, 0x525230) passes (x - dx, y, colour - dy): the colour pointer
//    moved by dy FieldColors instead of y by dy (a source-level slip). COffsetClippedProxyFrame does not
//    call it (its own slot 53 subtracts the offset and calls the frame).
//  - COffsetClippedProxyFrame's ClearRect (slot 24) moves the caller's rectangle by the offset in place;
//    FillRect and TintRect (slots 28, 57) work on a copy. Blit (slot 23) moves only the destination.
//  - The clip frame's ClipRect (slot 36) gives an empty rectangle (right = left, bottom = top) when its
//    rectangle does not meet the target's.
#include "frame.h"

namespace zax {

using namespace render;

namespace {

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);

CClassInfo *class_getter(DWORD var, DWORD name, float version, DWORD parent, DWORD size, DWORD abstract, DWORD create,
                         DWORD create_array, DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(0x48))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, parent ? fn_dyn<Getter>(parent)() : nullptr, size, abstract,
                                     create, create_array, register_fields);
    return v;
}

constexpr DWORD PROXY_FRAME_CLASS = 0x712228, OFFSET_CLIPPED_CLASS = 0x6d0148, FRAME_INTERFACE_CLASS = 0x6bf84c;

void set_target(void *proxy, void *frame) { fn<void(THISCALL *)(void *, void *)>(0x54fdb0)(proxy, frame); }
DWORD intersect(const Rect *clip, Rect *r) { return fn<DWORD(__fastcall *)(const Rect *, Rect *)>(0x4b2cd0)(clip, r); }
DWORD clip_rects(const Rect *clip, Rect *src, Rect *dst) { return fn<DWORD(__fastcall *)(const Rect *, Rect *, Rect *)>(0x4b2c20)(clip, src, dst); }
DWORD clip_line(const Rect *clip, int *x1, int *y1, int *x2, int *y2)
{
    return fn<DWORD(__fastcall *)(const Rect *, int *, int *, int *, int *)>(0x4b2d30)(clip, x1, y1, x2, y2);
}
DWORD clip_line_f(const RectF *clip, float *x1, float *y1, float *x2, float *y2)
{
    return fn<DWORD(__fastcall *)(const RectF *, float *, float *, float *, float *)>(0x4b2f50)(clip, x1, y1, x2, y2);
}
// A float through fld/fstp (how the exe passes the coordinates on)
float through_fpu(float f) { return fstp(fld(f)); }
DWORD fbits(float f) { return float_bits(f); }

// ---- CFrameInterface ------------------------------------------------------------------------------------------

// 0x4b2bc0: abstract, 8 bytes, no parent
CClassInfo *__cdecl FrameInterface_ClassInfo()
{
    return class_getter(glob_addr(FRAME_INTERFACE_CLASS), glob_addr(0x613d7c) /* "CFrameInterface" */, 1.0f, 0, 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}
// 0x40b290 (slot 1 of 0x5eb534): the destructor level, the pool block freed with bit 0
void *THISCALL FrameInterface_DeletingDtor(BYTE *self, DWORD flags)
{
    *reinterpret_cast<DWORD *>(self) = vt(VT_FRAME_INTERFACE);
    obj_free_fields(self);
    obj_dtor(self);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}

// ---- CProxyFrame ---------------------------------------------------------------------------------------------

void proxy_construct(BYTE *p)
{
    obj_ctor(p);
    *reinterpret_cast<DWORD *>(p) = vt(VT_FRAME_INTERFACE);
    obj_init_fields(p);
    *reinterpret_cast<DWORD *>(p) = vt(VT_PROXY_FRAME);
    obj_init_fields(p);
    reinterpret_cast<ProxyFrame *>(p)->target = nullptr;
}
void proxy_destroy(BYTE *p)
{
    *reinterpret_cast<DWORD *>(p) = vt(VT_PROXY_FRAME);
    obj_free_fields(p);
    *reinterpret_cast<DWORD *>(p) = vt(VT_FRAME_INTERFACE);
    obj_free_fields(p);
    obj_dtor(p);
}

CClassInfo *proxy_class()
{
    return class_getter(glob_addr(PROXY_FRAME_CLASS), glob_addr(0x61eb0c) /* "CProxyFrame" */, 1.0f, fn_addr(0x4b2bc0), 0xc, 0, fn_addr(0x54fbf0), fn_addr(0x54fc40), fn_addr(0x58ea40));
}
CClassInfo *__cdecl Proxy_ClassInfo() { return proxy_class(); }        // 0x54fba0
CClassInfo *THISCALL Proxy_ClassSlot(void *) { return proxy_class(); } // 0x54fd50 (slot 2)

void *__cdecl Proxy_Create()   // 0x54fbf0
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0xc));
    if (!p)
        return nullptr;
    proxy_construct(p);
    return p;
}
void *__fastcall Proxy_CreateArray(int count, int, int)   // 0x54fc40
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * 0xc + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            proxy_construct(e + i * 0xc);
    return e;
}
void *THISCALL Proxy_DeletingDtor(BYTE *self, DWORD flags)   // 0x54fcb0 (slot 1)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                proxy_destroy(self + (i - 1) * 0xc);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    proxy_destroy(self);
    if (flags & 1)
        pool_free(self, 0xc);
    return self;
}

// 0x54fdb0 (slot 62; frame): the target
void THISCALL Proxy_SetTarget(ProxyFrame *p, void *frame) { p->target = frame; }

// The forwarding slots: the same slot of the target with the same arguments; slot 21 adds 4
template <int S> DWORD THISCALL fwd0(ProxyFrame *p) { return vcall<DWORD>(p->target, S); }
template <int S> DWORD THISCALL fwd1(ProxyFrame *p, DWORD a) { return vcall<DWORD>(p->target, S, a); }
template <int S> DWORD THISCALL fwd2(ProxyFrame *p, DWORD a, DWORD b) { return vcall<DWORD>(p->target, S, a, b); }
template <int S> DWORD THISCALL fwd3(ProxyFrame *p, DWORD a, DWORD b, DWORD c) { return vcall<DWORD>(p->target, S, a, b, c); }
template <int S> DWORD THISCALL fwd4(ProxyFrame *p, DWORD a, DWORD b, DWORD c, DWORD d) { return vcall<DWORD>(p->target, S, a, b, c, d); }
template <int S> DWORD THISCALL fwd5(ProxyFrame *p, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    return vcall<DWORD>(p->target, S, a, b, c, d, e);
}
DWORD THISCALL Proxy_MemorySize(ProxyFrame *p) { return vcall<DWORD>(p->target, 21) + 4; }   // 0x40b400

// ---- The offset frame ------------------------------------------------------------------------------------------

// 0x525000
OffsetFrame *THISCALL Offset_Ctor(OffsetFrame *o)
{
    BYTE *p = reinterpret_cast<BYTE *>(o);
    obj_ctor(p);
    o->proxy.vftable = vt(VT_FRAME_INTERFACE);
    obj_init_fields(p);
    o->proxy.vftable = vt(VT_PROXY_FRAME);
    obj_init_fields(p);
    o->proxy.vftable = vt(VT_OFFSET_FRAME);
    o->proxy.target = nullptr;
    o->dx = o->dy = 0;
    return o;
}
void *THISCALL Offset_DeletingDtor(BYTE *self, DWORD flags)   // 0x525040
{
    proxy_destroy(self);
    if (flags & 1)
        pool_free(self, 0x14);
    return self;
}
Rect moved(const Rect *r, int dx, int dy) { return {r->left - dx, r->top - dy, r->right - dx, r->bottom - dy}; }

// 0x525080 (slot 23): the destination rectangle moved by minus the offset in place
DWORD THISCALL Offset_Blit(OffsetFrame *o, DWORD src, DWORD src_rect, Rect *dst, DWORD mode)
{
    const int dx = o->dx, dy = o->dy;
    dst->left -= dx;
    dst->top -= dy;
    dst->right -= dx;
    dst->bottom -= dy;
    return vcall<DWORD>(o->proxy.target, 23, src, src_rect, dst, mode);
}
DWORD THISCALL Offset_ClearRect(OffsetFrame *o, const Rect *r)   // 0x5250d0 (slot 24)
{
    Rect m = moved(r, o->dx, o->dy);
    return vcall<DWORD>(o->proxy.target, 24, &m);
}
DWORD THISCALL Offset_FillRect(OffsetFrame *o, const Rect *r, DWORD c)   // 0x525120 (slot 28)
{
    Rect m = moved(r, o->dx, o->dy);
    return vcall<DWORD>(o->proxy.target, 28, &m, c);
}
DWORD THISCALL Offset_TintRect(OffsetFrame *o, const Rect *r, DWORD t)   // 0x525320 (slot 57)
{
    Rect m = moved(r, o->dx, o->dy);
    return vcall<DWORD>(o->proxy.target, 57, &m, t);
}
// 0x525180 (slot 36; out): the target's rectangle moved by minus the offset; eax -dy
DWORD THISCALL Offset_ClipRect(OffsetFrame *o, Rect *out)
{
    vcall<DWORD>(o->proxy.target, 36, out);
    const int dx = -o->dx, dy = -o->dy;
    out->left += dx;
    out->right += dx;
    out->top += dy;
    out->bottom += dy;
    return (DWORD)dy;
}
DWORD THISCALL Offset_DrawLine(OffsetFrame *o, int x1, int y1, int x2, int y2, DWORD c)   // 0x525260 (slot 51)
{
    return vcall<DWORD>(o->proxy.target, 51, x1 - o->dx, y1 - o->dy, x2 - o->dx, y2 - o->dy, c);
}
// 0x5252a0 (slot 52; floats x1, y1, x2, y2, colour)
DWORD THISCALL Offset_DrawThickLine(OffsetFrame *o, float x1, float y1, float x2, float y2, DWORD c)
{
    const long double dy = fild(o->dy);
    const float dx = fstp(fild(o->dx));
    const float ny2 = fstp(fld(y2) - dy), nx2 = fstp(fld(x2) - fld(dx)), ny1 = fstp(fld(y1) - dy), nx1 = fstp(fld(x1) - fld(dx));
    return vcall<DWORD>(o->proxy.target, 52, fbits(nx1), fbits(ny1), fbits(nx2), fbits(ny2), c);
}
DWORD THISCALL Offset_GetColor(OffsetFrame *o, int x, int y, FieldColor *c)   // 0x525230 (slot 53; quirk above)
{
    return vcall<DWORD>(o->proxy.target, 53, x - o->dx, y, c - o->dy);
}
DWORD THISCALL Offset_GetPixel(OffsetFrame *o, int x, int y) { return vcall<DWORD>(o->proxy.target, 54, x - o->dx, y - o->dy); }          // 0x525200
DWORD THISCALL Offset_SetPixel(OffsetFrame *o, int x, int y, DWORD c) { return vcall<DWORD>(o->proxy.target, 55, x - o->dx, y - o->dy, c); } // 0x5251d0
DWORD THISCALL Offset_IsPixelSet(OffsetFrame *o, int x, int y) { return vcall<DWORD>(o->proxy.target, 56, x - o->dx, y - o->dy); }        // 0x5252f0

// ---- The clip frame --------------------------------------------------------------------------------------------

// 0x46d790
ClipFrame *THISCALL Clip_Ctor(ClipFrame *c)
{
    BYTE *p = reinterpret_cast<BYTE *>(c);
    obj_ctor(p);
    c->proxy.vftable = vt(VT_FRAME_INTERFACE);
    obj_init_fields(p);
    c->proxy.vftable = vt(VT_PROXY_FRAME);
    obj_init_fields(p);
    c->proxy.target = nullptr;
    c->proxy.vftable = vt(VT_CLIP_FRAME);
    return c;
}
void *THISCALL Clip_DeletingDtor(BYTE *self, DWORD flags)   // 0x46d800
{
    proxy_destroy(self);
    if (flags & 1)
        pool_free(self, 0x1c);
    return self;
}
// 0x46d7d0 (slot 21 of the three proxy classes): the target's (+8 through 0x482de0) plus 4
DWORD THISCALL Clipped_MemorySize(ProxyFrame *p) { return vcall<DWORD>(fn<void *(THISCALL *)(void *)>(0x482de0)(p), 21) + 4; }
DWORD THISCALL Clip_Height(const ClipFrame *c) { return (DWORD)(c->clip.bottom - c->clip.top); }   // 0x46d7e0 (slot 39)
DWORD THISCALL Clip_Width(const ClipFrame *c) { return (DWORD)(c->clip.right - c->clip.left); }    // 0x46d7f0 (slot 45)

// 0x46d840 (slot 23): Blit_ClipRects with the clip rectangle, then the target; al 0 when nothing is left
DWORD THISCALL Clip_Blit(ClipFrame *c, DWORD src, Rect *src_rect, Rect *dst, DWORD mode)
{
    const DWORD ok = clip_rects(&c->clip, src_rect, dst);
    if (!(BYTE)ok)
        return ok & ~0xffu;
    return vcall<DWORD>(c->proxy.target, 23, src, src_rect, dst, mode);
}
DWORD THISCALL Clip_ClearRect(ClipFrame *c, Rect *r)   // 0x46d880 (slot 24): the caller's rectangle cut in place
{
    const DWORD ok = intersect(&c->clip, r);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(c->proxy.target, 24, r);
}
DWORD THISCALL Clip_FillRect(ClipFrame *c, const Rect *r, DWORD color)   // 0x46d8b0 (slot 28)
{
    Rect m = *r;
    const DWORD ok = intersect(&c->clip, &m);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(c->proxy.target, 28, &m, color);
}
DWORD THISCALL Clip_TintRect(ClipFrame *c, const Rect *r, DWORD t)   // 0x46db30 (slot 57)
{
    Rect m = *r;
    const DWORD ok = intersect(&c->clip, &m);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(c->proxy.target, 57, &m, t);
}
// 0x46d900 (slot 36; out): the clip rectangle cut to the target's; empty (right = left, bottom = top) when
// they do not meet; eax the intersection's, or the left edge
DWORD THISCALL Clip_ClipRect(ClipFrame *c, Rect *out)
{
    *out = c->clip;
    Rect t;
    vcall<DWORD>(fn<void *(THISCALL *)(void *)>(0x482de0)(c), 36, &t);
    const DWORD ok = intersect(&t, out);
    if ((BYTE)ok)
        return ok;
    out->bottom = out->top;
    out->right = out->left;
    return (DWORD)out->left;
}
DWORD THISCALL Clip_DrawLine(ClipFrame *c, int x1, int y1, int x2, int y2, DWORD color)   // 0x46d960 (slot 51)
{
    const DWORD ok = clip_line(&c->clip, &x1, &y1, &x2, &y2);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(c->proxy.target, 51, x1, y1, x2, y2, color);
}
// 0x46d9b0 (slot 52): the clip rectangle as floats, Line_ClipToRectF, then the target
DWORD THISCALL Clip_DrawThickLine(ClipFrame *c, float x1, float y1, float x2, float y2, DWORD color)
{
    const RectF cf = {fstp(fild(c->clip.left)), fstp(fild(c->clip.top)), fstp(fild(c->clip.right)), fstp(fild(c->clip.bottom))};
    const DWORD ok = clip_line_f(&cf, &x1, &y1, &x2, &y2);
    if (!(BYTE)ok)
        return ok;
    const DWORD b4 = fbits(through_fpu(y2)), b3 = fbits(through_fpu(x2)), b2 = fbits(through_fpu(y1)), b1 = fbits(through_fpu(x1));
    return vcall<DWORD>(c->proxy.target, 52, b1, b2, b3, b4, color);
}
bool clip_in(const ClipFrame *c, int x, int y) { return x >= c->clip.left && y >= c->clip.top && x < c->clip.right && y < c->clip.bottom; }
// 0x46da30 / 0x46da70 / 0x46dab0 / 0x46daf0 (slots 55, 54, 53, 56): the point inside the clip rectangle,
// else nothing (eax/al 0)
void THISCALL Clip_SetPixel(ClipFrame *c, int x, int y, DWORD color)
{
    if (clip_in(c, x, y))
        vcall<DWORD>(c->proxy.target, 55, x, y, color);
}
DWORD THISCALL Clip_GetPixel(ClipFrame *c, int x, int y) { return clip_in(c, x, y) ? vcall<DWORD>(c->proxy.target, 54, x, y) : 0; }
DWORD THISCALL Clip_GetColor(ClipFrame *c, int x, int y, DWORD color) { return clip_in(c, x, y) ? vcall<DWORD>(c->proxy.target, 53, x, y, color) : 0; }
DWORD THISCALL Clip_IsPixelSet(ClipFrame *c, int x, int y) { return clip_in(c, x, y) ? vcall<DWORD>(c->proxy.target, 56, x, y) : 0; }
// 0x46db90 (rect): the clip rectangle; eax its bottom
DWORD THISCALL Clip_SetClip(ClipFrame *c, const Rect *r)
{
    c->clip = *r;
    return (DWORD)r->bottom;
}
// 0x46dbb0 (slot 62; frame): the target and its rectangle (its slot 36); eax its top
DWORD THISCALL Clip_SetTarget(ClipFrame *c, void *frame)
{
    Rect r;
    vcall<DWORD>(frame, 36, &r);
    set_target(c, frame);
    c->clip = r;
    return (DWORD)r.top;
}

// ---- COffsetClippedProxyFrame -------------------------------------------------------------------------------------

using OCP = OffsetClippedProxyFrame;

void ocp_construct(BYTE *p)
{
    proxy_construct(p);
    OCP *o = reinterpret_cast<OCP *>(p);
    fn<void *(THISCALL *)(void *)>(0x525000)(&o->offset);
    fn<void *(THISCALL *)(void *)>(0x46d790)(&o->clip);
    o->proxy.vftable = vt(VT_OFFSET_CLIPPED);
    obj_init_fields(p);
}
void ocp_destroy(BYTE *p)
{
    OCP *o = reinterpret_cast<OCP *>(p);
    o->proxy.vftable = vt(VT_OFFSET_CLIPPED);
    obj_free_fields(p);
    proxy_destroy(reinterpret_cast<BYTE *>(&o->clip));
    proxy_destroy(reinterpret_cast<BYTE *>(&o->offset));
    proxy_destroy(p);
}
CClassInfo *ocp_class()
{
    return class_getter(glob_addr(OFFSET_CLIPPED_CLASS), glob_addr(0x61d2bc) /* "COffsetClippedProxyFrame" */, 1.0f, fn_addr(0x54fba0), 0x3c, 0, fn_addr(0x5244a0), fn_addr(0x524500), fn_addr(0x58ea40));
}
CClassInfo *__cdecl Ocp_ClassInfo() { return ocp_class(); }        // 0x524450
CClassInfo *THISCALL Ocp_ClassSlot(void *) { return ocp_class(); } // 0x524710 (slot 2)
void *__cdecl Ocp_Create()                                         // 0x5244a0
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0x3c));
    if (!p)
        return nullptr;
    ocp_construct(p);
    return p;
}
void *__fastcall Ocp_CreateArray(int count, int, int)   // 0x524500
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * 0x3c + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            ocp_construct(e + i * 0x3c);
    return e;
}
void *THISCALL Ocp_DeletingDtor(BYTE *self, DWORD flags)   // 0x5245a0 (slot 1)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                ocp_destroy(self + (i - 1) * 0x3c);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    ocp_destroy(self);
    if (flags & 1)
        pool_free(self, 0x3c);
    return self;
}
// 0x524770 / 0x5247c0 (frame): the constructor; on a frame the offset frame aimed at the clip frame
// (its slot 62), the clip frame and the proxy at the frame
OCP *THISCALL Ocp_Ctor(OCP *o)
{
    ocp_construct(reinterpret_cast<BYTE *>(o));
    return o;
}
OCP *THISCALL Ocp_CtorFrame(OCP *o, void *frame)
{
    ocp_construct(reinterpret_cast<BYTE *>(o));
    vcall<DWORD>(&o->offset, 62, &o->clip);
    vcall<DWORD>(&o->clip, 62, frame);
    set_target(o, frame);
    return o;
}
// 0x524f10 (slot 62; frame): the same wiring on an existing object
void THISCALL Ocp_SetTarget(OCP *o, void *frame)
{
    vcall<DWORD>(&o->offset, 62, &o->clip);
    vcall<DWORD>(&o->clip, 62, frame);
    set_target(o, frame);
}
// 0x524fe0 (x, y): the offset
void THISCALL Ocp_SetOffset(OCP *o, int x, int y)
{
    o->offset.dx = x;
    o->offset.dy = y;
}
// The offset through the offset frame's slots 43 (x) and 44 (y)
int off_x(OCP *o) { return vcall<int>(&o->offset, 43); }
int off_y(OCP *o) { return vcall<int>(&o->offset, 44); }
Rect clip_rect(OCP *o)
{
    Rect r;
    vcall<DWORD>(&o->clip, 36, &r);
    return r;
}

// 0x524840 (slot 23): the destination moved by minus the offset (in place), Blit_ClipRects with the clip
// frame's rectangle, then the frame
DWORD THISCALL Ocp_Blit(OCP *o, DWORD src, Rect *src_rect, Rect *dst, DWORD mode)
{
    const int dy = -off_y(o), dx = -off_x(o);
    dst->left += dx;
    dst->top += dy;
    dst->right += dx;
    dst->bottom += dy;
    Rect c = clip_rect(o);
    const DWORD ok = clip_rects(&c, src_rect, dst);
    if (!(BYTE)ok)
        return ok & ~0xffu;
    return vcall<DWORD>(o->proxy.target, 23, src, src_rect, dst, mode);
}
// 0x5248e0 (slot 24): the caller's rectangle moved in place, cut, cleared through the frame
DWORD THISCALL Ocp_ClearRect(OCP *o, Rect *r)
{
    const int dy = -off_y(o), dx = -off_x(o);
    r->left += dx;
    r->top += dy;
    r->right += dx;
    r->bottom += dy;
    Rect c = clip_rect(o);
    const DWORD ok = intersect(&c, r);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(o->proxy.target, 24, r);
}
// 0x524960 (slot 28) / 0x524e60 (slot 57): a moved copy, cut, then the frame
DWORD THISCALL Ocp_FillRect(OCP *o, const Rect *r, DWORD color)
{
    Rect m = *r;
    const int dy = -off_y(o), dx = -off_x(o);
    m.left += dx;
    m.top += dy;
    m.right += dx;
    m.bottom += dy;
    Rect c = clip_rect(o);
    const DWORD ok = intersect(&c, &m);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(o->proxy.target, 28, &m, color);
}
DWORD THISCALL Ocp_TintRect(OCP *o, const Rect *r, DWORD t)
{
    Rect m = *r;
    const int dy = -off_y(o), dx = -off_x(o);
    m.left += dx;
    m.top += dy;
    m.right += dx;
    m.bottom += dy;
    Rect c = clip_rect(o);
    const DWORD ok = intersect(&c, &m);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(o->proxy.target, 57, &m, t);
}
// 0x524a10 (slot 36; out): the clip frame's rectangle moved by the offset; eax the new bottom
DWORD THISCALL Ocp_ClipRect(OCP *o, Rect *out)
{
    vcall<DWORD>(&o->clip, 36, out);
    const int dy = off_y(o), dx = off_x(o);
    out->left += dx;
    out->top += dy;
    out->right += dx;
    out->bottom += dy;
    return (DWORD)out->bottom;
}
// 0x524a70 / 0x524aa0 (slots 43, 44): the frame's origin plus the offset
int THISCALL Ocp_OriginX(OCP *o)
{
    const int f = vcall<int>(o->proxy.target, 43);
    return off_x(o) + f;
}
int THISCALL Ocp_OriginY(OCP *o)
{
    const int f = vcall<int>(o->proxy.target, 44);
    return off_y(o) + f;
}
// 0x524ad0 (slot 51): the end points minus the offset, Line_ClipToRect with the clip frame's rectangle
DWORD THISCALL Ocp_DrawLine(OCP *o, int x1, int y1, int x2, int y2, DWORD color)
{
    const int dx = off_x(o), dy = off_y(o);
    x2 -= dx;
    x1 -= dx;
    y1 -= dy;
    y2 -= dy;
    Rect c = clip_rect(o);
    const DWORD ok = clip_line(&c, &x1, &y1, &x2, &y2);
    if (!(BYTE)ok)
        return ok;
    return vcall<DWORD>(o->proxy.target, 51, x1, y1, x2, y2, color);
}
// 0x524b80 (slot 52; floats): the same with floats (the offset as fild values, y through a float)
DWORD THISCALL Ocp_DrawThickLine(OCP *o, float x1, float y1, float x2, float y2, DWORD color)
{
    const int ix = off_x(o), iy = off_y(o);
    const long double dx = fild(ix);
    x1 = fstp(fld(x1) - dx);
    const float dy = fstp(fild(iy));
    y1 = fstp(fld(y1) - fld(dy));
    x2 = fstp(fld(x2) - dx);
    y2 = fstp(fld(y2) - fld(dy));
    Rect c = clip_rect(o);
    const RectF cf = {fstp(fild(c.left)), fstp(fild(c.top)), fstp(fild(c.right)), fstp(fild(c.bottom))};
    const DWORD ok = clip_line_f(&cf, &x1, &y1, &x2, &y2);
    if (!(BYTE)ok)
        return ok;
    const DWORD b4 = fbits(through_fpu(y2)), b3 = fbits(through_fpu(x2)), b2 = fbits(through_fpu(y1)), b1 = fbits(through_fpu(x1));
    return vcall<DWORD>(o->proxy.target, 52, b1, b2, b3, b4, color);
}
// 0x524c70 / 0x524ce0 / 0x524d60 / 0x524de0 (slots 55, 54, 53, 56): the point minus the offset, inside the
// clip frame's rectangle, then the frame; else nothing (eax/al 0)
bool ocp_point(OCP *o, int &x, int &y)
{
    x -= off_x(o);
    y -= off_y(o);
    Rect c = clip_rect(o);
    return x >= c.left && y >= c.top && x < c.right && y < c.bottom;
}
void THISCALL Ocp_SetPixel(OCP *o, int x, int y, DWORD color)
{
    if (ocp_point(o, x, y))
        vcall<DWORD>(o->proxy.target, 55, x, y, color);
}
DWORD THISCALL Ocp_GetPixel(OCP *o, int x, int y) { return ocp_point(o, x, y) ? vcall<DWORD>(o->proxy.target, 54, x, y) : 0; }
DWORD THISCALL Ocp_GetColor(OCP *o, int x, int y, DWORD color) { return ocp_point(o, x, y) ? vcall<DWORD>(o->proxy.target, 53, x, y, color) : 0; }
DWORD THISCALL Ocp_IsPixelSet(OCP *o, int x, int y) { return ocp_point(o, x, y) ? vcall<DWORD>(o->proxy.target, 56, x, y) : 0; }
// 0x524f50 (rect): the clip rectangle (0x46db90) set to rect minus the offset
DWORD THISCALL Ocp_SetClip(OCP *o, const Rect *r)
{
    Rect m = *r;
    const int dy = -off_y(o), dx = -off_x(o);
    m.left += dx;
    m.top += dy;
    m.right += dx;
    m.bottom += dy;
    return fn<DWORD(THISCALL *)(ClipFrame *, const Rect *)>(0x46db90)(&o->clip, &m);
}

}  // namespace

ZAX_HOOKS(frame_proxy)
{
    const Hook list[] = {
        {0x4b2bc0, (void *)&FrameInterface_ClassInfo, "CFrameInterface_ClassInfo", 5, {0xa1, 0x4c, 0xf8, 0x6b, 0x00, 0x85, 0xc0, 0x75}},
        {0x40b290, ptr(&FrameInterface_DeletingDtor), "CFrameInterface::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x34, 0xb5, 0x5e}},
        // CProxyFrame
        {0x54fba0, (void *)&Proxy_ClassInfo, "CProxyFrame_ClassInfo", 5, {0xa1, 0x28, 0x22, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x54fd50, ptr(&Proxy_ClassSlot), "CProxyFrame::ClassInfo", 5, {0xa1, 0x28, 0x22, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x54fbf0, (void *)&Proxy_Create, "CProxyFrame::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x54fc40, (void *)&Proxy_CreateArray, "CProxyFrame::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x54fcb0, ptr(&Proxy_DeletingDtor), "CProxyFrame::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x54fdb0, ptr(&Proxy_SetTarget), "CProxyFrame::SetTarget", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x08, 0xc2}},
        {0x40b400, ptr(&Proxy_MemorySize), "CProxyFrame::MemorySize", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0x50, 0x54}},
        {0x40b2c0, ptr(&fwd4<23>), "CProxyFrame::Blit", 7, {0x8b, 0x54, 0x24, 0x10, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b2f0, ptr(&fwd1<24>), "CProxyFrame::ClearRect", 7, {0x8b, 0x49, 0x08, 0x8b, 0x54, 0x24, 0x04, 0x52}},
        {0x40b2e0, ptr(&fwd0<25>), "CProxyFrame::Clear", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0x60, 0x64}},
        {0x40b300, ptr(&fwd1<26>), "CProxyFrame::FillColor", 7, {0x8b, 0x49, 0x08, 0x8b, 0x54, 0x24, 0x04, 0x52}},
        {0x40b310, ptr(&fwd1<27>), "CProxyFrame::Fill", 7, {0x8b, 0x49, 0x08, 0x8b, 0x54, 0x24, 0x04, 0x52}},
        {0x40b320, ptr(&fwd2<28>), "CProxyFrame::FillRect", 7, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b3a0, ptr(&fwd0<29>), "CProxyFrame::BitsPerPixel", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0x60, 0x74}},
        {0x40b390, ptr(&fwd0<30>), "CProxyFrame::BytesPerPixel", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0x60, 0x78}},
        {0x40b340, ptr(&fwd0<31>), "CProxyFrame::Pixels", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0x60, 0x7c}},
        {0x40b350, ptr(&fwd0<32>), "CProxyFrame::Block1C", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x80}},
        {0x40b360, ptr(&fwd0<33>), "CProxyFrame::Block24", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x84}},
        {0x40b370, ptr(&fwd0<34>), "CProxyFrame::Block2C", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x88}},
        {0x40b380, ptr(&fwd0<35>), "CProxyFrame::Block34", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x8c}},
        {0x40b3b0, ptr(&fwd1<36>), "CProxyFrame::ClipRect", 7, {0x8b, 0x49, 0x08, 0x8b, 0x54, 0x24, 0x04, 0x52}},
        {0x40b3d0, ptr(&fwd0<37>), "CProxyFrame::Frame", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x94}},
        {0x40b3e0, ptr(&fwd0<38>), "CProxyFrame::Flags", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x98}},
        {0x40b3f0, ptr(&fwd0<39>), "CProxyFrame::Height", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0x9c}},
        {0x40b410, ptr(&fwd0<40>), "CProxyFrame::Pitch", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xa0}},
        {0x40b420, ptr(&fwd0<41>), "CProxyFrame::Log2Width", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xa4}},
        {0x40b430, ptr(&fwd0<42>), "CProxyFrame::Log2Height", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xa8}},
        {0x40b440, ptr(&fwd0<43>), "CProxyFrame::OriginX", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xac}},
        {0x40b450, ptr(&fwd0<44>), "CProxyFrame::OriginY", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xb0}},
        {0x40b460, ptr(&fwd0<45>), "CProxyFrame::Width", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xb4}},
        {0x40b470, ptr(&fwd0<46>), "CProxyFrame::Rows", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xb8}},
        {0x40b480, ptr(&fwd0<47>), "CProxyFrame::Table20", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xbc}},
        {0x40b490, ptr(&fwd0<48>), "CProxyFrame::Table28", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xc0}},
        {0x40b4a0, ptr(&fwd0<49>), "CProxyFrame::Table30", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xc4}},
        {0x40b4b0, ptr(&fwd0<50>), "CProxyFrame::Table38", 5, {0x8b, 0x49, 0x08, 0x8b, 0x01, 0xff, 0xa0, 0xc8}},
        {0x40b540, ptr(&fwd5<51>), "CProxyFrame::DrawLine", 7, {0x8b, 0x54, 0x24, 0x14, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b570, ptr(&fwd5<52>), "CProxyFrame::DrawThickLine", 7, {0x8b, 0x54, 0x24, 0x14, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b520, ptr(&fwd3<53>), "CProxyFrame::GetColor", 7, {0x8b, 0x54, 0x24, 0x0c, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b500, ptr(&fwd2<54>), "CProxyFrame::GetPixel", 7, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b4e0, ptr(&fwd3<55>), "CProxyFrame::SetPixel", 7, {0x8b, 0x54, 0x24, 0x0c, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b4c0, ptr(&fwd2<56>), "CProxyFrame::IsPixelSet", 7, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x49, 0x08, 0x52}},
        {0x40b5a0, ptr(&fwd2<57>), "CProxyFrame::TintRect", 7, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x49, 0x08, 0x52}},
        // the offset frame
        {0x525000, ptr(&Offset_Ctor), "OffsetFrame_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x08, 0x16, 0xef, 0xff}, 4},
        {0x525040, ptr(&Offset_DeletingDtor), "OffsetFrame::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x2c, 0xb6, 0x5e}},
        {0x525080, ptr(&Offset_Blit), "OffsetFrame::Blit", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x51, 0x10, 0x56}},
        {0x5250d0, ptr(&Offset_ClearRect), "OffsetFrame::ClearRect", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x53}},
        {0x525120, ptr(&Offset_FillRect), "OffsetFrame::FillRect", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x53}},
        {0x525180, ptr(&Offset_ClipRect), "OffsetFrame::ClipRect", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x5251d0, ptr(&Offset_SetPixel), "OffsetFrame::SetPixel", 5, {0x56, 0x8b, 0x74, 0x24, 0x10, 0x8b, 0xc1, 0x57}},
        {0x525200, ptr(&Offset_GetPixel), "OffsetFrame::GetPixel", 7, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x51, 0x10, 0x56}},
        {0x525230, ptr(&Offset_GetColor), "OffsetFrame::GetColor", 7, {0x8b, 0x41, 0x10, 0x56, 0x8b, 0x71, 0x0c, 0x8b}},
        {0x525260, ptr(&Offset_DrawLine), "OffsetFrame::DrawLine", 5, {0x8b, 0x41, 0x10, 0x56, 0x57, 0x8b, 0x7c, 0x24}},
        {0x5252a0, ptr(&Offset_DrawThickLine), "OffsetFrame::DrawThickLine", 7, {0x51, 0xdb, 0x41, 0x10, 0xdb, 0x41, 0x0c, 0x8b}},
        {0x5252f0, ptr(&Offset_IsPixelSet), "OffsetFrame::IsPixelSet", 7, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x51, 0x10, 0x56}},
        {0x525320, ptr(&Offset_TintRect), "OffsetFrame::TintRect", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x53}},
        // the clip frame
        {0x46d790, ptr(&Clip_Ctor), "ClipFrame_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x78, 0x8e, 0xfa, 0xff}, 4},
        {0x46d800, ptr(&Clip_DeletingDtor), "ClipFrame::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x2c, 0xb6, 0x5e}},
        {0x46d7d0, ptr(&Clipped_MemorySize), "ClipFrame::MemorySize", 5, {0xe8, 0x0b, 0x56, 0x01, 0x00, 0x8b, 0x10, 0x8b}, 1},
        {0x46d7e0, ptr(&Clip_Height), "ClipFrame::Height", 6, {0x8b, 0x41, 0x18, 0x8b, 0x51, 0x10, 0x2b, 0xc2}},
        {0x46d7f0, ptr(&Clip_Width), "ClipFrame::Width", 6, {0x8b, 0x41, 0x14, 0x8b, 0x51, 0x0c, 0x2b, 0xc2}},
        {0x46d840, ptr(&Clip_Blit), "ClipFrame::Blit", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x57, 0x8b}},
        {0x46d880, ptr(&Clip_ClearRect), "ClipFrame::ClearRect", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x46d8b0, ptr(&Clip_FillRect), "ClipFrame::FillRect", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x56}},
        {0x46d900, ptr(&Clip_ClipRect), "ClipFrame::ClipRect", 6, {0x83, 0xec, 0x10, 0x8d, 0x41, 0x0c, 0x56, 0x8b}},
        {0x46d960, ptr(&Clip_DrawLine), "ClipFrame::DrawLine", 7, {0x56, 0x8b, 0xf1, 0x8d, 0x44, 0x24, 0x14, 0x8d}},
        {0x46d9b0, ptr(&Clip_DrawThickLine), "ClipFrame::DrawThickLine", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x8d, 0x44}},
        {0x46da30, ptr(&Clip_SetPixel), "ClipFrame::SetPixel", 7, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x41, 0x0c, 0x3b}},
        {0x46da70, ptr(&Clip_GetPixel), "ClipFrame::GetPixel", 7, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x41, 0x0c, 0x3b}},
        {0x46dab0, ptr(&Clip_GetColor), "ClipFrame::GetColor", 7, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x41, 0x0c, 0x3b}},
        {0x46daf0, ptr(&Clip_IsPixelSet), "ClipFrame::IsPixelSet", 7, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x41, 0x0c, 0x3b}},
        {0x46db30, ptr(&Clip_TintRect), "ClipFrame::TintRect", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x56}},
        {0x46db90, ptr(&Clip_SetClip), "ClipFrame_SetClip", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x0c, 0x8b}},
        {0x46dbb0, ptr(&Clip_SetTarget), "ClipFrame::SetTarget", 5, {0x83, 0xec, 0x10, 0x56, 0x57, 0x8b, 0x7c, 0x24}},
        // COffsetClippedProxyFrame
        {0x524450, (void *)&Ocp_ClassInfo, "COffsetClippedProxyFrame_ClassInfo", 5, {0xa1, 0x48, 0x01, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x524710, ptr(&Ocp_ClassSlot), "COffsetClippedProxyFrame::ClassInfo", 5, {0xa1, 0x48, 0x01, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x5244a0, (void *)&Ocp_Create, "COffsetClippedProxyFrame::Create", 6, {0x56, 0xb9, 0x3c, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x524500, (void *)&Ocp_CreateArray, "COffsetClippedProxyFrame::CreateArray", 7, {0x51, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x5245a0, ptr(&Ocp_DeletingDtor), "COffsetClippedProxyFrame::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x56, 0x57}},
        {0x524770, ptr(&Ocp_Ctor), "COffsetClippedProxyFrame::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x98, 0x1e, 0xef, 0xff}, 4},
        {0x5247c0, ptr(&Ocp_CtorFrame), "COffsetClippedProxyFrame::CtorFrame", 5, {0x53, 0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x46, 0x1e}},
        {0x524f10, ptr(&Ocp_SetTarget), "COffsetClippedProxyFrame::SetTarget", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x46, 0x0c}},
        {0x524fe0, ptr(&Ocp_SetOffset), "COffsetClippedProxyFrame_SetOffset", 8, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x524840, ptr(&Ocp_Blit), "COffsetClippedProxyFrame::Blit", 6, {0x83, 0xec, 0x10, 0x53, 0x8b, 0xd9, 0x56, 0x57}},
        {0x5248e0, ptr(&Ocp_ClearRect), "COffsetClippedProxyFrame::ClearRect", 6, {0x83, 0xec, 0x10, 0x53, 0x8b, 0xd9, 0x56, 0x57}},
        {0x524960, ptr(&Ocp_FillRect), "COffsetClippedProxyFrame::FillRect", 7, {0x83, 0xec, 0x20, 0x8b, 0x44, 0x24, 0x24, 0x53}},
        {0x524e60, ptr(&Ocp_TintRect), "COffsetClippedProxyFrame::TintRect", 7, {0x83, 0xec, 0x20, 0x8b, 0x44, 0x24, 0x24, 0x53}},
        {0x524a10, ptr(&Ocp_ClipRect), "COffsetClippedProxyFrame::ClipRect", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x57, 0x8b}},
        {0x524a70, ptr(&Ocp_OriginX), "COffsetClippedProxyFrame::OriginX", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4e, 0x08, 0x8b}},
        {0x524aa0, ptr(&Ocp_OriginY), "COffsetClippedProxyFrame::OriginY", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4e, 0x08, 0x8b}},
        {0x524ad0, ptr(&Ocp_DrawLine), "COffsetClippedProxyFrame::DrawLine", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x524b80, ptr(&Ocp_DrawThickLine), "COffsetClippedProxyFrame::DrawThickLine", 5, {0x83, 0xec, 0x28, 0x56, 0x57, 0x8b, 0xf9, 0x8b}},
        {0x524c70, ptr(&Ocp_SetPixel), "COffsetClippedProxyFrame::SetPixel", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x524ce0, ptr(&Ocp_GetPixel), "COffsetClippedProxyFrame::GetPixel", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x524d60, ptr(&Ocp_GetColor), "COffsetClippedProxyFrame::GetColor", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x524de0, ptr(&Ocp_IsPixelSet), "COffsetClippedProxyFrame::IsPixelSet", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x524f50, ptr(&Ocp_SetClip), "COffsetClippedProxyFrame_SetClip", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x53}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
