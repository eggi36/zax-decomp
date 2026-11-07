// The view's drawing helpers (phase 4, rendering): the oval, arc and arrow 0x4fccc0-0x4fceb0 the AIs' and
// layer parts' debug drawing uses, the polygon outline 0x546d40, and the light beam's block reader
// 0x4fe6a0 (it makes the beam's CLightningLayerPart).
//
// The arc 0x4fcd10 [confirmed: code; VM] (fastcall frame, centre; radius, y ratio, colour, start, end,
// closed): max(radius, radius * ratio) truncated and divided by 4 (towards zero) gives the number of
// segments, clamped to [10, 25]; the step is (end - start) / segments. The first point is the centre plus
// (cos(start) * radius, sin(start) * radius * ratio) (Table_Cos 0x508da0 / Table_Sin 0x508ec0, the
// table's entries), each next point one step further; the segments go through the frame's slot 52 (the
// float line, x1, y1, x2, y2, colour). Closed: a line from the centre to the first point before and from
// the last point to the centre after. The oval 0x4fccc0 is the arc from 0 to 2 pi (0x40c90fdb) open,
// 0x4fcce0 the arc closed.
//
// The arrow 0x4fceb0 [confirmed: code; VM] (fastcall frame, point; angle, y ratio, r, g, b): a colour
// (r, g, b, 255) (FieldColor_CtorRGBA 0x53f010), the shaft from the point 30 pixels along the angle (the
// rotation of (30, 0) by -angle with Table_SinCos 0x508fe0, its y times the ratio) and the two barbs of
// 10 pixels from the tip at 0x5f953c - angle and -(angle - 0x5f9538), each through Frame_DrawThickLinePts
// 0x4b3cb0. The terms multiplied by the vectors' zero component are computed as the exe does (they keep
// the sign of zero).
//
// The polygon outline 0x546d40 [confirmed: code; VM] (polygon; frame, dx, dy, colour, numbers, skip): the
// polygon's points (+0, pairs of floats; +8 the count; +4 a record of 12 bytes per point whose first two
// floats are the point's normal) moved by (dx, dy); count - skip - 1 lines from point i to i + 1 through
// slot 52 (unsigned: a skip of count - 1 or more draws nothing or runs over the array, as in the
// original); with "numbers" each point's index in the application's font (+0x30 of 0x6c02cc) at the point
// plus its normal times the font height and 0x5eee2c, y centred on half the height (fistp of the floats,
// CFont::DrawInt 0x4a6b90 right justified). A single point: slot 55 (set pixel) at the rounded point and
// a rectangle 0x5f08b8 around it (Frame_DrawRectXY 0x4b33f0); no point: nothing.
//
// The light beam's reader 0x4fe6a0 [confirmed: code; VM] (packet; this unused): a new CLightningLayerPart
// (0xb4 bytes, operator new; CSimpleLayerPart's constructor 0x571680, then the class's own fields: two
// embedded pointer arrays (vtables 0x5f99b4, 0x5f99a8) and two colours 0x53f000; vtable 0x5f984c and
// Object_InitFields), its slots 19 and 27 (read the packet), and LightBeam_SetLast 0x4fd5c0 with it; eax
// slot 27's. The class itself is the effects batch's.
//
// Return values: the drawing functions are void for their callers (eax not reproduced).
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

using LD = long double;

LD tab_508da0(float a) { return fn<LD(__stdcall *)(float)>(0x508da0)(a); }
LD tab_508ec0(float a) { return fn<LD(__stdcall *)(float)>(0x508ec0)(a); }
void tab_sincos(float *sin_out, float *cos_out, float a) { fn<DWORD(__fastcall *)(float *, float *, float)>(0x508fe0)(sin_out, cos_out, a); }
void frame_line_f(void *frame, float x1, float y1, float x2, float y2, DWORD colour) { vcall<DWORD>(frame, 52, x1, y1, x2, y2, colour); }
void thick_line_pts(void *frame, const float *p1, const float *p2, const void *colour)
{
    fn<DWORD(THISCALL *)(void *, const float *, const float *, const void *)>(0x4b3cb0)(frame, p1, p2, colour);
}
void arc(void *frame, const float *centre, float radius, float ratio, DWORD colour, float start, float end, DWORD closed)
{
    fn<void(__fastcall *)(void *, const float *, float, float, DWORD, float, float, DWORD)>(0x4fcd10)(frame, centre, radius, ratio, colour, start, end, closed);
}
DWORD font_height(void *font) { return fn<DWORD(__fastcall *)(void *)>(0x4a6be0)(font); }

}  // namespace

// 0x4fcd10
void __fastcall DrawView_Arc(void *frame, const float *centre, float radius, float ratio, DWORD colour, float start, float end, BYTE closed)
{
    const LD prod = (LD)radius * ratio;
    const LD m = x87_le((LD)radius, prod) ? prod : (LD)radius;
    const int n = x87_ftol(m) / 4;
    const int segments = n < 10 ? 10 : n > 25 ? 25 : n;
    const float step = fstp(((LD)end - start) / (LD)segments);
    float angle = start;
    const float c0 = fstp(tab_508da0(angle));
    const LD s0 = tab_508ec0(angle);
    const float cx = centre[0], cy = centre[1];
    const float rc = fstp((LD)c0 * radius);
    const LD rs = s0 * radius * ratio;
    float px = fstp((LD)cx + rc);
    float py = fstp((LD)cy + rs);
    if (closed)
        frame_line_f(frame, cx, cy, px, py, colour);
    for (int k = segments; k; k--) {
        angle = fstp((LD)angle + step);
        const float c = fstp(tab_508da0(angle));
        const LD s = tab_508ec0(angle);
        const float xc = fstp((LD)c * radius);
        const LD ys = s * radius * ratio;
        const float x = fstp((LD)xc + centre[0]);
        const float y = fstp(ys + centre[1]);
        frame_line_f(frame, px, py, x, y, colour);
        px = x;
        py = y;
    }
    if (closed)
        frame_line_f(frame, centre[0], centre[1], px, py, colour);
}

// 0x4fccc0: the arc from 0 to 2 pi, open
void __fastcall DrawView_Oval(void *frame, const float *centre, float radius, float ratio, DWORD colour)
{
    arc(frame, centre, radius, ratio, colour, 0.0f, raw_float(0x40c90fdb), 0);
}

// 0x4fcce0: the arc closed
void __fastcall DrawView_ArcClosed(void *frame, const float *centre, float radius, float ratio, DWORD colour, float start, float end)
{
    arc(frame, centre, radius, ratio, colour, start, end, 1);
}

// 0x4fceb0
void __fastcall DrawView_Arrow(void *frame, const float *p, float angle, float ratio, DWORD r, DWORD g, DWORD b)
{
    float s, c;
    tab_sincos(&s, &c, fstp(-(LD)angle));
    float v[2] = {30.0f, 0.0f};
    const LD x1 = (LD)v[0] * c + (LD)s * v[1];
    const LD y1 = -(LD)s * v[0] + (LD)c * v[1];
    BYTE colour[16];
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, r, g, b, 0xff);
    float tip[2];
    v[1] = fstp(y1 * ratio);
    tip[0] = fstp(x1 + p[0]);
    tip[1] = fstp((LD)v[1] + p[1]);
    thick_line_pts(frame, p, tip, colour);

    // the first barb (sine and cosine into the stack slots of the colour arguments)
    float u[2] = {10.0f, 0.0f};
    float sb, cb;
    tab_sincos(&sb, &cb, fstp(cst(glob_addr(0x5f953c)) - angle));
    const LD x2 = (LD)u[0] * cb + (LD)sb * u[1];
    const LD y2 = -(LD)sb * u[0] + (LD)cb * u[1];
    u[1] = fstp(y2 * ratio);
    u[0] = fstp(x2 + tip[0]);
    u[1] = fstp((LD)u[1] + tip[1]);
    thick_line_pts(frame, tip, u, colour);

    // the second barb
    float w[2] = {10.0f, 0.0f};
    float sc, cc;
    tab_sincos(&sc, &cc, fstp(-((LD)angle - cst(glob_addr(0x5f9538)))));
    const LD x3 = (LD)sc * w[1] + (LD)w[0] * cc;
    const LD y3 = -(LD)sc * w[0] + (LD)cc * w[1];
    w[1] = fstp(y3 * ratio);
    w[0] = fstp(x3 + tip[0]);
    w[1] = fstp((LD)w[1] + tip[1]);
    thick_line_pts(frame, tip, w, colour);
}

// 0x546d40
struct PolygonPoints {
    float *points;      // pairs
    BYTE *normals;      // 12 bytes per point, the first two floats the normal
    DWORD count;
};
void THISCALL DrawPolygonOutline(PolygonPoints *self, void *frame, float dx, float dy, DWORD colour, BYTE numbers, DWORD skip)
{
    const DWORD count = self->count;
    if (count <= 1) {
        if (count != 1)
            return;
        const float *pt = self->points;
        const int y = x87_fistp(fld(fstp((LD)dy + pt[1])));
        const int x = x87_fistp(fld(fstp((LD)dx + pt[0])));
        vcall<DWORD>(frame, 55, x, y, colour);
        const int bottom = x87_fistp(fld(fstp((LD)dy + self->points[1] + cst(glob_addr(0x5f08b8)))));
        const int right = x87_fistp(fld(fstp((LD)dx + self->points[0] + cst(glob_addr(0x5f08b8)))));
        const int top = x87_fistp(fld(fstp((LD)dy + self->points[1] - cst(glob_addr(0x5f08b8)))));
        const int left = x87_fistp(fld(fstp((LD)dx + self->points[0] - cst(glob_addr(0x5f08b8)))));
        fn<DWORD(THISCALL *)(void *, int, int, int, int, DWORD)>(0x4b33f0)(frame, left, top, right, bottom, colour);
        return;
    }
    const DWORD lines = count - skip - 1;
    DWORD off = 0;
    for (DWORD i = 0; i < lines; i++, off += 0xc) {
        const float *pt = self->points;
        frame_line_f(frame, fstp((LD)dx + pt[2 * i]), fstp((LD)dy + pt[2 * i + 1]), fstp((LD)dx + pt[2 * i + 2]), fstp((LD)dy + pt[2 * i + 3]), colour);
        if (!numbers)
            continue;
        void *font = bp(dw(*glob<BYTE **>(0x6c02cc), 0x30));
        const DWORD h1 = font_height(font);
        const float *nrm = reinterpret_cast<const float *>(self->normals + off);
        const int x = x87_fistp(fld(fstp(fild64(h1) * nrm[0] * cst(glob_addr(0x5eee2c)) + self->points[2 * i] + dx)));
        void *font2 = bp(dw(*glob<BYTE **>(0x6c02cc), 0x30));
        const DWORD h2 = font_height(font2);
        const float *nrm2 = reinterpret_cast<const float *>(self->normals + off);
        float yf = fstp(fild64(h2) * nrm2[1] * cst(glob_addr(0x5eee2c)) + self->points[2 * i + 1] + dy);
        const DWORD h3 = font_height(font2) >> 1;
        yf = fstp((LD)yf - fild64(h3));
        const int y = x87_fistp(fld(yf));
        fn<DWORD(THISCALL *)(void *, void *, int, int, DWORD, DWORD)>(0x4a6b90)(bp(dw(*glob<BYTE **>(0x6c02cc), 0x30)), frame, x, y, i, 1);
    }
}

// 0x4fe6a0
DWORD THISCALL LightBeam_ReadBlock(void *, void *packet)
{
    BYTE *o = static_cast<BYTE *>(fn<void *(__fastcall *)(DWORD)>(0x417710)(0xb4));
    if (o) {
        fn<void *(THISCALL *)(void *)>(0x571680)(o);
        dw(o, 0x64) = vt(0x5f99b4);
        dw(o, 0x68) = 0;
        dw(o, 0x6c) = 0;
        dw(o, 0x70) = 0;
        dw(o, 0x74) = vt(0x5f99a8);
        dw(o, 0x78) = 0;
        dw(o, 0x7c) = 0;
        dw(o, 0x80) = 0;
        fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0x94);
        fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0xa4);
        dw(o, 0) = vt(0x5f984c);
        obj_init_fields(o);
    }
    vcall<DWORD>(o, 19);
    const DWORD r = vcall<DWORD>(o, 27, packet);
    fn<void(__fastcall *)(void *)>(0x4fd5c0)(o);
    return r;
}

ZAX_HOOKS(view_draw)
{
    const Hook list[] = {
#include "view_draw_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
