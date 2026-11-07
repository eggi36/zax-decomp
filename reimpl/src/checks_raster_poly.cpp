// Checks of raster_poly.cpp (the polygon rasterizer, batch R7) in the game thread.
//  - PolyRaster_Draw on fake frames of 8, 16 and 24 bits (origins, pitch padding) with fake polygons of 3-12
//    points in 12.4 fixed point: inside, across every edge and corner, outside, degenerate (collinear,
//    equal points, zero height), sub-pixel and convex/concave fans; every render type, the MMX flag
//    0x7121f4 on and off, the 8-bit black colour's static made anew or kept; a power-of-two texture for
//    type 5.
//  - The clip (PolyRaster_Clip) on its own: the returned list's points and codes; the outcodes; the edge
//    set-up and step on random edges (the struct compared after up to 40 steps).
//  - The point lists on heap objects: SetCount, Add, Clear, Assign, the destructors, FromFloat; the
//    constructor with the tint tables rebuilt (0x7121f3 cleared for the case); the static lists' atexit
//    destructors on stand-in contents.
#include "checks_frame_r1.h"

namespace zax {

int hooks_raster_poly(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"raster_poly", &hooks_raster_poly, {}, {}, {}, 0, true};

struct Pt {
    int x, y;
};
struct PolyObj {   // the polygon's layout (raster_poly.cpp)
    DWORD vftable;
    DWORD pts_vt;
    Pt *data;
    DWORD count, capacity;
    DWORD type;
    void *param;
};

constexpr DWORD MMX_FLAG = 0x7121f4, BLACK_GUARD = 0x7160b8, BLACK8 = 0x7160bc, TABLES_DONE = 0x7121f3;

int coord(int size)
{
    switch (rnd(8)) {
    case 0: return 0;
    case 1: return size * 16;
    case 2: return rnd_range(-400, size * 16 + 400);
    case 3: return rnd_range(-3000, 3000);
    default: return rnd_range(-40, size * 16 + 40);
    }
}

// A fake polygon of the case in the objects (both sides); returns its offset
DWORD make_poly(int w, int h, DWORD type, DWORD texture, int &n)
{
    n = rnd(6) ? rnd_range(3, 8) : rnd_range(0, 12);
    const DWORD off = alloc_object(sizeof(PolyObj)), pts = alloc_object(12 * 8);
    Pt p[12];
    const int shape = rnd(6);
    for (int i = 0; i < n; i++) {
        p[i] = {coord(w), coord(h)};
        if (shape == 0 && i)   // collinear
            p[i] = {p[0].x + (p[1 % n].x - p[0].x) * i, p[0].y + (p[1 % n].y - p[0].y) * i};
        if (shape == 1 && i)   // sub-pixel
            p[i] = {p[0].x + rnd_range(-20, 20), p[0].y + rnd_range(-20, 20)};
        if (shape == 2 && i == 2)   // a flat edge
            p[i].y = p[1].y;
    }
    for (int side = 0; side < 2; side++) {
        PolyObj *o = object<PolyObj>(side, off);
        o->vftable = 0x5fd35c;
        o->pts_vt = 0x5f8dc4;
        o->data = object<Pt>(side, pts);
        memcpy(o->data, p, sizeof p);
        o->count = (DWORD)n;
        o->capacity = 12;
        o->type = type;
        o->param = texture != 0xffffffffu ? object<void>(side, texture) : nullptr;
    }
    return off;
}

void check_draw(Tally &t)
{
    g_seed = 0x549050;
    BYTE &mmx = *at<BYTE *>(MMX_FLAG);
    const BYTE saved_mmx = mmx, saved_guard = *at<BYTE *>(BLACK_GUARD);
    const DWORD saved_black = *at<DWORD *>(BLACK8);
    static const Format FORMATS[3] = {PAL8, RGB565, RGB24};
    for (int k = 0; k < scaled(3000); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = FORMATS[k % 7 == 6 ? 2 : k & 1];
        s.width = rnd_range(1, 48);
        s.height = rnd_range(1, 40);
        s.pitch = rnd(3) ? 0 : s.width + rnd_range(1, 5);
        if (rnd(4) == 0) {
            s.x = rnd_range(-3, 3);
            s.y = rnd_range(-3, 3);
        }
        const int fi = make_frame(s);
        FrameSpec ts;
        ts.format = s.format;
        ts.width = 1 << rnd_range(0, 4);
        ts.height = 1 << rnd_range(0, 4);
        const int tex = make_frame(ts);
        if (fi < 0 || tex < 0)
            continue;
        const DWORD type = rnd(6);
        int n;
        const DWORD poly = make_poly(s.width, s.height, type, g_h.frames[tex].object, n);
        const bool use_mmx = rnd(2), fresh_black = rnd(4) == 0;
        if (g_frame_trace)
            log("raster_poly draw %d: format %d %dx%d at %d,%d type %u points %d mmx %d", k, s.format, s.width, s.height, s.x, s.y, type, n, use_mmx);
        run_sides(t, g_hooks, "PolyRaster_Draw", [&](int side) -> DWORD {
            mmx = use_mmx;
            if (fresh_black)
                *at<BYTE *>(BLACK_GUARD) &= 0xfe;
            at<void(THISCALL *)(void *, void *)>(0x549050)(object<void>(side, poly), frame(side, fi));
            return 0;
        });
    }
    mmx = saved_mmx;
    *at<BYTE *>(BLACK_GUARD) = saved_guard;
    *at<DWORD *>(BLACK8) = saved_black;
}

void check_clip_edges(Tally &t)
{
    g_seed = 0x548920;
    for (int k = 0; k < scaled(3000); k++) {
        begin_case(random());
        const int w = rnd_range(1, 64), h = rnd_range(1, 64);
        int n;
        const DWORD poly = make_poly(w, h, 0, 0xffffffffu, n);
        const DWORD rect = alloc_object(16), out = alloc_object(0x20);
        const Rect r = {rnd_range(-4, 4), rnd_range(-4, 4), w, h};
        for (int side = 0; side < 2; side++)
            *object<Rect>(side, rect) = r;
        if (k % 3 == 0) {
            const DWORD i = n ? rnd((DWORD)n) : 0, j = n ? rnd((DWORD)n) : 0, steps = rnd(40);
            if (!n)
                continue;
            run_sides(t, g_hooks, "PolyEdge_Setup, PolyEdge_Step", [&](int side) -> DWORD {
                auto *e = object<int>(side, out);
                at<void *(__fastcall *)(void *, DWORD, const Pt *, DWORD, DWORD)>(0x54d040)(e, 0, object<PolyObj>(side, poly)->data, i, j);
                for (DWORD m = 0; m < steps && e[6] > 0; m++)
                    at<void(__fastcall *)(void *)>(0x54d0f0)(e);
                return 0;
            });
            continue;
        }
        Snap res[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                g_hooks.out();
            Snap &sn = res[side];
            const PolyObj *in = object<PolyObj>(side, poly);
            const PolyObj *c = at<const PolyObj *(THISCALL *)(const void *, const Rect *)>(0x548920)(in, object<Rect>(side, rect));
            sn.add(c == in ? 0 : ptrv(c));
            sn.add(c->count);
            for (DWORD i = 0; i < c->count && i < 8; i++) {
                sn.add((DWORD)c->data[i].x);
                sn.add((DWORD)c->data[i].y);
                sn.add(c != in ? reinterpret_cast<const DWORD *const *>(c)[8][i] : 0);
            }
            DWORD code;
            for (int i = 0; i < n; i++) {
                at<void(__fastcall *)(const Pt *, DWORD *, const Rect *)>(0x548b60)(in->data + i, &code, object<Rect>(side, rect));
                sn.add(code);
            }
            if (side == 0)
                g_hooks.in();
        }
        const bool same = g_hooks.fits && res[0] == res[1];
        t.check(same, "", "[PolyRaster_Clip]");
        if (!same && t.mismatches <= 3) {
            const PolyObj *in = object<PolyObj>(0, poly);
            log("    clip rect (%d, %d, %d, %d), %d points:", r.left, r.top, r.right, r.bottom, n);
            for (int i = 0; i < n; i++)
                log("      in %d: %d, %d", i, in->data[i].x, in->data[i].y);
            for (int side = 0; side < 2; side++)
                for (int i = 0; i < res[side].n; i += 6)
                    log("      side %d: %x %x %x %x %x %x", side, res[side].v[i], i + 1 < res[side].n ? res[side].v[i + 1] : 0,
                        i + 2 < res[side].n ? res[side].v[i + 2] : 0, i + 3 < res[side].n ? res[side].v[i + 3] : 0,
                        i + 4 < res[side].n ? res[side].v[i + 4] : 0, i + 5 < res[side].n ? res[side].v[i + 5] : 0);
        }
    }
}

void check_lists(Tally &t)
{
    g_seed = 0x548680;
    // the constructor with the tables rebuilt
    {
        static const DWORD TABLES[4] = {0x7120f4, 0x711ff4, 0x711db8, 0x711ef4};
        SavedMemory saved;
        for (DWORD a : TABLES)
            saved.add(a, 0x100);
        saved.add(TABLES_DONE, 1);
        saved.add(MMX_FLAG, 1);
        compare_sides(t, g_hooks, "PolyPoints_Ctor (tables)", [&](int) -> Snap {
            Snap s;
            saved.restore();
            *at<BYTE *>(TABLES_DONE) = 0;
            BYTE obj[0x1c];
            memset(obj, 0xcc, sizeof obj);
            at<void *(THISCALL *)(void *)>(0x5484a0)(obj);
            s.add_bytes(obj, sizeof obj);
            for (DWORD a : TABLES)
                s.add_bytes(at<void *>(a), 0x100);
            s.add(*at<BYTE *>(MMX_FLAG));
            return s;
        });
    }
    for (int k = 0; k < scaled(600); k++) {
        const unsigned seed = random();
        if (g_frame_trace)
            log("raster_poly lists %d", k);
        compare_sides(t, g_hooks, "point lists", [&](int) -> Snap {
            Snap s;
            unsigned q = seed;
            BYTE *obj = static_cast<BYTE *>(heap_alloc(0x2c));
            const bool clipped = pattern_next(q) & 1;
            at<void *(THISCALL *)(void *)>(0x5484a0)(obj);
            if (clipped) {
                at<void(THISCALL *)(void *)>(0x535440)(obj + 0x1c);
                *reinterpret_cast<DWORD *>(obj + 0x1c) = 0x5ee138;
                *reinterpret_cast<DWORD *>(obj) = 0x5fd368;
            }
            DWORD valid = 0, valid_codes = 0;   // elements known to be written (SetCount leaves pool garbage)
            for (int i = 0; i < 10; i++) {
                const DWORD op = pattern_next(q) % 6;
                s.add(op);
                if (g_frame_trace)
                    log("  op %u", op);
                const DWORD *dd = reinterpret_cast<const DWORD *>(obj);
                if (op == 0) {
                    valid = dd[3] < valid ? dd[3] : valid;
                    valid_codes = clipped && dd[9] < valid_codes ? dd[9] : valid_codes;
                }
                if (op == 0)
                    vcall<void>(obj, 1, pattern_next(q) % 20);
                else if (op == 1)
                    at<void(THISCALL *)(void *, int, int)>(0x548710)(obj, (int)pattern_next(q), (int)pattern_next(q));
                else if (op == 2 && clipped)
                    at<void(THISCALL *)(void *, int, int, DWORD)>(0x5488c0)(obj, (int)pattern_next(q), (int)pattern_next(q), pattern_next(q) % 16);
                else if (op == 3)
                    vcall<void>(obj, 2);
                else if (op == 4) {
                    Pt p[6];
                    for (Pt &x : p)
                        x = {(int)pattern_next(q), (int)pattern_next(q)};
                    at<void(THISCALL *)(void *, const Pt *, DWORD)>(0x54cf50)(obj + 4, p, pattern_next(q) % 7);
                } else {
                    float f[8];
                    for (float &x : f)
                        x = (float)(int)(pattern_next(q) % 2000) / 7.0f - 100.0f;
                    const void *fp[3] = {f, nullptr, (const void *)(uintptr_t)(pattern_next(q) % 5)};
                    at<void(THISCALL *)(void *, const void *)>(0x549530)(obj, fp);
                }
                const DWORD *d = reinterpret_cast<const DWORD *>(obj);
                if (op == 1 || op == 2)
                    valid = valid + 1 == d[3] ? valid + 1 : valid;
                if (op == 2)
                    valid_codes = valid_codes + 1 == d[9] ? valid_codes + 1 : valid_codes;
                if (op == 3)
                    valid = valid_codes = 0;
                if (op == 4 || op == 5)
                    valid = d[3];
                if (op == 5 && clipped)
                    valid_codes = 0;
                s.add(d[0]);
                s.add(d[1]);
                s.add(d[3]);
                s.add(d[4]);
                s.add_bytes(reinterpret_cast<void *>(d[2]), (valid < d[3] ? valid : d[3]) * 8);
                if (clipped) {
                    s.add(d[9]);
                    s.add(d[10]);
                    s.add_bytes(reinterpret_cast<void *>(d[8]), (valid_codes < d[9] ? valid_codes : d[9]) * 4);
                }
            }
            if (pattern_next(q) & 1)
                at<void(THISCALL *)(void *)>(0x548640)(obj);
            vcall<void *>(obj, 0, 1);
            return s;
        });
    }
    // the static lists' atexit destructors on stand-ins (no arrays)
    for (int k = 0; k < 4; k++) {
        const DWORD addr = k & 1 ? 0x711ec8 : 0x711d88, fn = k & 1 ? 0x548f10 : 0x548fb0;
        SavedMemory saved;
        saved.add(addr, 0x2c);
        if (g_frame_trace)
            log("raster_poly static exit %d", k);
        compare_sides(t, g_hooks, "StaticExit_PolyClip", [&](int) -> Snap {
            Snap s;
            DWORD *d = at<DWORD *>(addr);
            memset(d, 0, 0x2c);
            d[3] = 7;
            d[9] = 5;
            at<void(__cdecl *)()>(fn)();
            s.add_bytes(d, 0x2c);
            return s;
        });
        saved.restore();
    }
}

}  // namespace

ZAX_CHECKS(raster_poly)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    Tally draw{"raster poly: PolyRaster_Draw (8/16/24 bits, all types, MMX on and off)"};
    check_draw(draw);
    draw.report();
    Tally clip{"raster poly: the clip, outcodes, edges"};
    check_clip_edges(clip);
    clip.report();
    Tally lists{"raster poly: point lists, constructor, static destructors"};
    check_lists(lists);
    lists.report();
}

}  // namespace zax
