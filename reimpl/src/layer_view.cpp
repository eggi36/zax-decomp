// CLayer's mini map mapping and view position reimplemented (see layer_view.h).
#include "layer_view.h"
#include "x87.h"

namespace zax {

static int &i32(BYTE *p, DWORD off) { return *reinterpret_cast<int *>(p + off); }
static float &f32(BYTE *p, DWORD off) { return *reinterpret_cast<float *>(p + off); }
static void *world() { return *glob<void **>(0x6c2080); }

constexpr DWORD CLIP_TOP = 0x1a8, CLIP_BOTTOM = 0x1ac, CLIP_LEFT = 0x1b0, CLIP_RIGHT = 0x1b4;
constexpr DWORD MM_X_OFFSET = 0x1b8, MM_X_SCALE = 0x1bc, MM_Y_OFFSET = 0x1c0, MM_Y_SCALE = 0x1c4;

// 0x4ec280. Halving as cdq/sub/sar: towards 0. eax: the left margin.
DWORD THISCALL LayerView::MiniMapImageRect(BYTE *layer, int *out)
{
    const float zoom = fstp(vcall<float>(world(), 100));
    const DWORD w = (DWORD)i32(layer, 0x48), h = (DWORD)i32(layer, 0x4c);
    out[0] = 0;
    out[1] = 0;
    out[2] = (int)w;
    out[3] = (int)h;
    out[1] = i32(layer, CLIP_TOP);
    out[0] = i32(layer, CLIP_LEFT);
    out[3] = (int)(h - (DWORD)i32(layer, CLIP_BOTTOM));
    out[2] = (int)(w - (DWORD)i32(layer, CLIP_RIGHT));
    int size[2];   // the mini map's width and height
    vcall<void>(world(), 99, size);
    out[3] = (int)((DWORD)out[3] - (DWORD)out[1]);
    out[2] = (int)((DWORD)out[2] - (DWORD)out[0]);
    out[1] = 0;
    out[0] = 0;
    const int ih = x87_ftol(fild(out[3]) / fld(zoom));
    out[3] = ih;
    const int iw = x87_ftol(fild(out[2]) / fld(zoom));
    out[2] = iw;
    long double s = cst(glob_addr(C_ONE));
    if (ih > size[1])
        s = fild(size[1]) / fild(ih);
    const float wf = fstp(fild(iw));
    const float sxf = fstp(fild(size[0]));
    if (!c03(fld(wf) * s, fld(sxf)))
        s = fld(sxf) / fld(wf);
    const int h2 = x87_ftol(fild(ih) * s);
    const int w2 = x87_ftol(fild(iw) * s);
    const int ypad = (int)((DWORD)size[1] - (DWORD)h2) / 2;
    const int xpad = (int)((DWORD)size[0] - (DWORD)w2) / 2;
    out[3] = (int)((DWORD)h2 + (DWORD)ypad);
    out[0] = xpad;
    out[1] = ypad;
    out[2] = (int)((DWORD)w2 + (DWORD)xpad);
    return (DWORD)xpad;
}

// 0x4ec3c0. eax: the last __ftol.
DWORD THISCALL LayerView::WorldRectToMiniMap(BYTE *layer, int *out, const int *in)
{
    if (c3(fld(f32(layer, MM_X_SCALE)), cst(glob_addr(C_ZERO))) || c3(fld(f32(layer, MM_Y_SCALE)), cst(glob_addr(C_ZERO)))) {
        const DWORD bottom = (DWORD)i32(layer, 0x4c) - (DWORD)i32(layer, CLIP_BOTTOM);
        const int left = i32(layer, CLIP_LEFT), top = i32(layer, CLIP_TOP);
        DWORD right = (DWORD)i32(layer, 0x48) - (DWORD)i32(layer, CLIP_RIGHT);
        int r[4];
        fn<DWORD(THISCALL *)(BYTE *, int *)>(0x4ec280)(layer, r);
        right -= (DWORD)left;
        const long double xs = fild64((long long)(DWORD)((DWORD)r[2] - (DWORD)r[0])) / fild64((long long)right);
        f32(layer, MM_X_SCALE) = fstp(xs);
        const DWORD hc = bottom - (DWORD)top;
        const long double ys = fild64((long long)(DWORD)((DWORD)r[3] - (DWORD)r[1])) / fild64((long long)hc);
        const float ysf = fstp(ys);
        f32(layer, MM_Y_SCALE) = ysf;
        f32(layer, MM_X_OFFSET) = fstp(fild(r[0]) - fild(left) * xs);
        f32(layer, MM_Y_OFFSET) = fstp(fild(r[1]) - fild(top) * fld(ysf));
    }
    out[0] = x87_ftol(fild(in[0]) * fld(f32(layer, MM_X_SCALE)) + fld(f32(layer, MM_X_OFFSET)));
    out[1] = x87_ftol(fild(in[1]) * fld(f32(layer, MM_Y_SCALE)) + fld(f32(layer, MM_Y_OFFSET)));
    out[2] = x87_ftol(fild(in[2]) * fld(f32(layer, MM_X_SCALE)) + fld(f32(layer, MM_X_OFFSET)));
    const int y1 = x87_ftol(fild(in[3]) * fld(f32(layer, MM_Y_SCALE)) + fld(f32(layer, MM_Y_OFFSET)));
    out[3] = y1;
    return (DWORD)y1;
}

// 0x4ed7f0. The x limit stays unrounded on the FPU stack, the y limit goes through a float.
DWORD THISCALL LayerView::ClampView(BYTE *layer, float *pos)
{
    BYTE *map = *reinterpret_cast<BYTE **>(layer + 0x7c);
    if (!map)
        return 0;
    int view[4];
    vcall<void>(layer, 36, view);
    const DWORD mw = ((DWORD)i32(map, 0x2cfc) - 1) & 0xffffff80;
    const DWORD mh = ((DWORD)i32(map, 0x2d00) - 1) & 0xffffff80;
    const long double wlim = fild64((long long)mw);
    const float hlim = fstp(fild64((long long)mh));
    if (c0(fld(pos[0]), cst(glob_addr(C_ZERO)))) {
        pos[0] = raw_float(0);
    } else {
        long double d = wlim - fild((int)((DWORD)view[2] - (DWORD)view[0]));
        if (c0(d, fld(pos[0]))) {
            if (!c03(cst(glob_addr(C_ZERO)), d))
                d = cst(glob_addr(C_ZERO));
            pos[0] = fstp(d);
        }
    }
    if (c0(fld(pos[1]), cst(glob_addr(C_ZERO)))) {
        pos[1] = raw_float(0);
        return 0;
    }
    const long double e = fld(hlim) - fild64((long long)(DWORD)((DWORD)view[3] - (DWORD)view[1]));
    if (c0(e, fld(pos[1])))
        pos[1] = fstp(e);
    return 0;
}

// 0x4ed6e0. The x correction goes through a float, the y correction stays unrounded.
DWORD THISCALL LayerView::ScrollToward(BYTE *layer, float *pos, float tx, float ty, float ox, float oy, float w, float h)
{
    const float hw = fstp(fld(w) * cst(glob_addr(C_HALF)));
    const float hh = fstp(fld(h) * cst(glob_addr(C_HALF)));
    float dxf = raw_float(0);
    const long double dx = fld(tx) - (fld(ox) * cst(glob_addr(C_HALF)) + fld(pos[0]));
    const float dy = fstp(fld(ty) - (fld(oy) * cst(glob_addr(C_HALF)) + fld(pos[1])));
    if (c0(dx, -fld(hw)))
        dxf = fstp(fld(hw) + dx);
    else if (!c03(dx, fld(hw)))
        dxf = fstp(dx - fld(hw));
    long double dyc = cst(glob_addr(C_ZERO));
    if (!c03(-fld(hh), fld(dy)))
        dyc = fld(hh) + fld(dy);
    else if (!c03(fld(dy), fld(hh)))
        dyc = fld(dy) - fld(hh);
    long double x = fld(dxf) + fld(pos[0]);
    if (!c03(cst(glob_addr(C_ZERO)), x))
        x = cst(glob_addr(C_ZERO));
    pos[0] = fstp(x);
    long double y = dyc + fld(pos[1]);
    if (!c03(cst(glob_addr(C_ZERO)), y))
        y = cst(glob_addr(C_ZERO));
    pos[1] = fstp(y);
    return fn<DWORD(THISCALL *)(BYTE *, float *)>(0x4ed7f0)(layer, pos);
}

// 0x4ed8f0. x and y are copied as they are (integer moves).
DWORD THISCALL LayerView::SetView(BYTE *layer, float x, float y)
{
    if (!*reinterpret_cast<void **>(layer + 0x7c))
        return 0;
    DWORD xb, yb;
    memcpy(&xb, &x, 4);
    memcpy(&yb, &y, 4);
    float *view = &f32(layer, 0x80);
    *reinterpret_cast<DWORD *>(layer + 0x84) = yb;
    *reinterpret_cast<DWORD *>(layer + 0x80) = xb;
    fn<DWORD(THISCALL *)(BYTE *, float *)>(0x4ed7f0)(layer, view);
    const int ix = x87_fistp(fld(view[0]));
    i32(layer, 0x88) = ix;
    const int iy = x87_fistp(fld(view[1]));
    i32(layer, 0x8c) = iy;
    return fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x524fe0)(layer, ix, iy);
}

ZAX_HOOKS(layer_view)
{
    using V = LayerView;
    const Hook list[] = {
#include "layer_view_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
