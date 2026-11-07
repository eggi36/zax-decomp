// Checks of view_draw.cpp with the frame check harness, in the game thread.
//  - The arc, oval and closed arc on fake 5-6-5 frames (sometimes through a COffsetClippedProxyFrame):
//    centres inside, on the edge and off the frame, radii from 0 to beyond the frame with ratios from 0 to
//    2 (the segment count's clamps at 10 and 25 and between), start and end angles both ways round and
//    beyond 2 pi, NaN and infinite radii; the colour a FieldColor in the case's objects.
//  - The arrow: random points, angles (negative and beyond 2 pi), ratios and colours.
//  - The polygon outline: fake polygons of 0-8 points with normals, every skip that keeps the line count
//    within the points, with and without the numbers (the application's font, real).
//  - The light beam's reader: CLightningLayerPart's slot 27 a recording stand-in; the new object's bytes
//    (pointers into itself by offset), the last beam 0x6c29fc, eax; the object deleted through its slot 1.
#include "checks_frame_r1.h"
#include "checks_hud_common.h"

namespace zax {

int hooks_view_draw(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"view_draw", &hooks_view_draw, {}, {}, {}, 0, true};

float rnd_float(float lo, float hi) { return lo + (hi - lo) * (float)rnd(100001) / 100000.0f; }

// A frame (5-6-5, the case's) and a colour object on both sides; the target index of the draw (a proxy
// on the frame every fourth case)
struct Target {
    int frame;
    DWORD proxy;   // object offset or ~0
    DWORD colour;
    void *on(int side) const { return proxy != ~0u ? (void *)object<BYTE>(side, proxy) : (void *)framec::frame(side, frame); }
};
Target make_target(int w, int h)
{
    FrameSpec s;
    s.format = RGB565;
    s.width = w;
    s.height = h;
    s.pitch = rnd(3) ? 0 : w + (int)rnd(7);
    Target t;
    t.frame = make_frame(s);
    t.proxy = ~0u;
    if (rnd(4) == 0) {
        Rect clip{rnd_range(-10, w / 2), rnd_range(-10, h / 2), rnd_range(w / 2, w + 10), rnd_range(h / 2, h + 10)};
        t.proxy = make_offset_clipped(t.frame, rnd_range(-20, 20), rnd_range(-20, 20), clip);
    }
    t.colour = alloc_object(sizeof(FieldColor));
    const DWORD r = rnd(256), g = rnd(256), b = rnd(256), a = rnd(3) ? 0xff : rnd(256);
    for (int side = 0; side < 2; side++)
        at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(object<BYTE>(side, t.colour), r, g, b, a);
    return t;
}

float pick_radius()
{
    switch (rnd(10)) {
    case 0: return 0.0f;
    case 1: return rnd_float(0, 3);
    case 2: return rnd_float(100, 300);
    case 3: return rnd(2) ? __builtin_nanf("") : __builtin_inff();
    default: return rnd_float(2, 90);
    }
}
float pick_angle()
{
    switch (rnd(6)) {
    case 0: return 0.0f;
    case 1: return rnd_float(-12.0f, 0.0f);
    case 2: return rnd_float(6.0f, 20.0f);
    default: return rnd_float(0.0f, 6.3f);
    }
}

void check_arcs(Tally &t)
{
    g_seed = 0x4fcd10;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        const Target tg = make_target(96, 80);
        const DWORD centre = alloc_object(8);
        const float c[2] = {rnd_float(-30, 130), rnd_float(-30, 110)};
        for (int side = 0; side < 2; side++)
            memcpy(object<BYTE>(side, centre), c, 8);
        const float radius = pick_radius(), ratio = rnd(5) ? rnd_float(0, 2) : rnd(2) ? 1.0f : 0.0f;
        const float start = pick_angle(), end = pick_angle();
        const int kind = (int)rnd(3);
        const DWORD closed = rnd(2);
        run_sides(t, g_hooks, kind == 0 ? "DrawView_Oval" : kind == 1 ? "DrawView_ArcClosed" : "DrawView_Arc", [&](int side) -> DWORD {
            void *f = tg.on(side);
            const float *cp = object<float>(side, centre);
            const DWORD col = ptrv(object<BYTE>(side, tg.colour));
            if (kind == 0)
                at<void(__fastcall *)(void *, const float *, float, float, DWORD)>(0x4fccc0)(f, cp, radius, ratio, col);
            else if (kind == 1)
                at<void(__fastcall *)(void *, const float *, float, float, DWORD, float, float)>(0x4fcce0)(f, cp, radius, ratio, col, start, end);
            else
                at<void(__fastcall *)(void *, const float *, float, float, DWORD, float, float, DWORD)>(0x4fcd10)(f, cp, radius, ratio, col, start, end,
                                                                                                                  closed);
            return 0;
        });
    }
}

void check_arrows(Tally &t)
{
    g_seed = 0x4fceb0;
    for (int k = 0; k < scaled(1200); k++) {
        begin_case(random());
        const Target tg = make_target(96, 80);
        const DWORD point = alloc_object(8);
        const float p[2] = {rnd_float(-30, 130), rnd_float(-30, 110)};
        for (int side = 0; side < 2; side++)
            memcpy(object<BYTE>(side, point), p, 8);
        const float angle = rnd(8) ? pick_angle() : rnd_float(-200, 200), ratio = rnd(5) ? rnd_float(0, 2) : 1.0f;
        const DWORD r = rnd(256), g = rnd(256), b = rnd(256);
        run_sides(t, g_hooks, "DrawView_Arrow", [&](int side) -> DWORD {
            at<void(__fastcall *)(void *, const float *, float, float, DWORD, DWORD, DWORD)>(0x4fceb0)(tg.on(side), object<float>(side, point), angle,
                                                                                                      ratio, r, g, b);
            return 0;
        });
    }
}

// The polygon the outline takes: points, normals (12 bytes each), count
struct Poly {
    DWORD points, normals, count;
};

void check_outline(Tally &t)
{
    g_seed = 0x546d40;
    for (int k = 0; k < scaled(1200); k++) {
        begin_case(random());
        const Target tg = make_target(112, 96);
        const DWORD n = rnd(9);
        const DWORD poly = alloc_object(sizeof(Poly)), pts = alloc_object(8 * 9), nrm = alloc_object(12 * 9);
        float pv[18], nv[27];
        for (DWORD i = 0; i < 18; i++)
            pv[i] = rnd(6) ? rnd_float(-20, 130) : (float)rnd_range(0, 100);
        for (DWORD i = 0; i < 27; i++)
            nv[i] = rnd_float(-1.5f, 1.5f);
        for (int side = 0; side < 2; side++) {
            memcpy(object<BYTE>(side, pts), pv, sizeof pv);
            memcpy(object<BYTE>(side, nrm), nv, sizeof nv);
            Poly *pp = object<Poly>(side, poly);
            pp->points = ptrv(object<BYTE>(side, pts));
            pp->normals = ptrv(object<BYTE>(side, nrm));
            pp->count = n;
        }
        const DWORD skip = n > 1 ? rnd(n) : rnd(3);
        const float dx = rnd(3) ? rnd_float(-15, 15) : 0.0f, dy = rnd(3) ? rnd_float(-15, 15) : 0.0f;
        const BYTE numbers = (BYTE)rnd(2);
        hudcheck::run_sides(t, g_hooks, "DrawPolygonOutline", [&](int side) -> DWORD {
            at<void(THISCALL *)(void *, void *, float, float, DWORD, DWORD, DWORD)>(0x546d40)(object<Poly>(side, poly), tg.on(side), dx, dy,
                                                                                              ptrv(object<BYTE>(side, tg.colour)), numbers, skip);
            return 0;
        });
    }
}

// ---- The light beam's reader ----------------------------------------------------------------------------------

BYTE *g_made;
DWORD g_read_packet, g_read_vtable;
DWORD THISCALL s_read_block(BYTE *o, DWORD packet)
{
    g_made = o;
    g_read_packet = packet;
    g_read_vtable = *reinterpret_cast<DWORD *>(o);
    return 0x4ff9c000u + (packet & 0xff);
}

void check_beam_reader(Tally &t)
{
    g_seed = 0x4fe6a0;
    const DWORD saved_beam = *at<DWORD *>(0x6c29fc);
    for (int k = 0; k < scaled(40); k++) {
        const DWORD packet = random();
        DWORD r[2] = {}, beam[2] = {}, seen[2] = {};
        alignas(16) BYTE bytes[2][0xb4] = {};
        for (int side = 0; side < 2; side++) {
            g_made = nullptr;
            *at<DWORD *>(0x6c29fc) = 0x12345678;
            CodePatch p;
            patch_jump(p, 0x4ff9c0, (void *)&s_read_block);
            if (side == 0)
                g_hooks.out();
            r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4fe6a0)(nullptr, packet);
            if (side == 0)
                g_hooks.in();
            unpatch(p);
            beam[side] = g_made && *at<DWORD *>(0x6c29fc) == ptrv(g_made);
            seen[side] = g_read_packet == packet && g_read_vtable == 0x5f984c;
            if (g_made) {
                for (DWORD i = 0; i < 0xb4; i += 4) {
                    DWORD v = *reinterpret_cast<DWORD *>(g_made + i);
                    if (v >= ptrv(g_made) && v < ptrv(g_made) + 0xb4)
                        v = 0xa0000000u + (v - ptrv(g_made));
                    memcpy(bytes[side] + i, &v, 4);
                }
                vcall<void *>(g_made, 1, 1);
            }
        }
        char d[120];
        wsprintfA(d, "[case %d: eax %x / %x, beam %u / %u, read %u / %u]", k, r[0], r[1], beam[0], beam[1], seen[0], seen[1]);
        t.check(g_hooks.fits && r[0] == r[1] && beam[0] && beam[1] && seen[0] && seen[1] && !memcmp(bytes[0], bytes[1], 0xb4), "", d);
    }
    *at<DWORD *>(0x6c29fc) = saved_beam;
}

}  // namespace

ZAX_CHECKS(view_draw)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    Tally arcs{"view draw: the arc, oval and closed arc"};
    check_arcs(arcs);
    arcs.report();
    Tally arrows{"view draw: the arrow"};
    check_arrows(arrows);
    arrows.report();
    Tally outline{"view draw: the polygon outline"};
    check_outline(outline);
    outline.report();
    Tally beam{"view draw: the light beam's reader"};
    check_beam_reader(beam);
    beam.report();
}

}  // namespace zax
