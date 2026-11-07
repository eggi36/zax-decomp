// Checks of frame_proxy.cpp with the frame check harness, in the game thread.
//  - COffsetClippedProxyFrame on fake frames (offsets, clip rectangles inside, across and outside the
//    frame): every drawing slot through its recording vtable, SetClip, SetOffset, SetTarget.
//  - Its offset frame and its clip frame alone (aimed at the frame): their slots, the offset frame's GetColor
//    with the colour moved by dy FieldColors (a block of room around the colour), the clip frame's width,
//    height, memory size, SetClip and SetTarget.
//  - CProxyFrame objects on fake frames: all forwarding slots 21-57 and SetTarget.
//  - Construction by content: the constructors of the three proxy parts and of COffsetClippedProxyFrame
//    (empty and on a frame), Create, CreateArray and the deleting destructors of CProxyFrame and
//    COffsetClippedProxyFrame, the deleting destructors of the offset and clip frame and of
//    CFrameInterface (on pool blocks of their size), the class getters.
#include "checks_frame_r1.h"

namespace zax {

int hooks_frame_proxy(Hook *out);

namespace {

using namespace r1;
using render::OffsetClippedProxyFrame;
using render::ProxyFrame;

ModuleHooks g_hooks{"frame_proxy", &hooks_frame_proxy, {}, {}, {}, 0, true};

template <typename R, typename... A> R exe(DWORD address, A... a) { return at<R(THISCALL *)(A...)>(address)(a...); }
DWORD lo8(DWORD v) { return v & 0xff; }

FieldColor pick_color(Format f)
{
    FieldColor c;
    BYTE *b = reinterpret_cast<BYTE *>(&c);
    for (DWORD i = 0; i < sizeof c; i++)
        b[i] = (BYTE)rnd(256);
    c.flags = (c.flags & ~3u) | (rnd(4) ? 1u : 2u);
    if (f == RGB555)
        c.pixel16 &= 0x7fff;
    return c;
}
Rect pick_rect(int x, int y, int w, int h)
{
    Rect r;
    r.left = rnd_range(x - 12, x + w + 4);
    r.top = rnd_range(y - 12, y + h + 4);
    r.right = r.left + rnd_range(0, w + 14);
    r.bottom = r.top + rnd_range(0, h + 14);
    return r;
}

// One case: a frame, an offset clipped proxy on it, a source frame for the blits, the case's objects
struct Case {
    FrameSpec s;
    int fi, src, fi2;
    DWORD ocp, mode, rect, rect2, out, color, cblock, table, tptr, pts, fpts;
    int dx, dy;
    Rect clip;
};

bool make_case(Case &c, unsigned seed)
{
    begin_case(seed);
    FrameSpec &s = c.s;
    const DWORD k = rnd(10);
    s.format = k < 4 ? PAL8 : k < 9 ? RGB565 : RGB24;
    s.width = rnd_range(1, 60);
    s.height = rnd_range(1, 36);
    s.x = rnd(2) ? 0 : rnd_range(-8, 8);
    s.y = rnd(2) ? 0 : rnd_range(-8, 8);
    c.fi = make_frame(s);
    FrameSpec ss;
    ss.format = s.format == PAL8 ? PAL8 : RGB565;
    ss.width = rnd_range(1, 24);
    ss.height = rnd_range(1, 16);
    c.src = make_frame(ss);
    FrameSpec s2 = s;
    s2.width = rnd_range(1, 30);
    s2.height = rnd_range(1, 20);
    c.fi2 = make_frame(s2);
    if (c.fi < 0 || c.src < 0 || c.fi2 < 0)
        return false;
    c.dx = rnd(3) ? rnd_range(-12, 12) : 0;
    c.dy = rnd(3) ? rnd_range(-12, 12) : 0;
    c.clip = {s.x + rnd_range(-6, s.width), s.y + rnd_range(-6, s.height), 0, 0};
    c.clip.right = c.clip.left + rnd_range(0, s.width + 8);
    c.clip.bottom = c.clip.top + rnd_range(0, s.height + 8);
    c.ocp = make_offset_clipped(c.fi, c.dx, c.dy, c.clip);
    c.mode = copy_mode_object();
    c.rect = alloc_object(sizeof(Rect));
    c.rect2 = alloc_object(sizeof(Rect));
    c.out = alloc_object(sizeof(Rect));
    c.color = alloc_object(sizeof(FieldColor));
    c.cblock = alloc_object(0x180);
    c.table = alloc_object(256);
    c.tptr = alloc_object(4);
    c.pts = alloc_object(16);
    c.fpts = alloc_object(16);
    if (c.ocp == 0xffffffffu || c.fpts == 0xffffffffu)
        return false;
    const Rect r = pick_rect(s.x, s.y, s.width, s.height);
    Rect r2;
    r2.left = rnd_range(-4, ss.width - 1);
    r2.top = rnd_range(-4, ss.height - 1);
    r2.right = r2.left + rnd_range(1, ss.width);
    r2.bottom = r2.top + rnd_range(1, ss.height);
    const FieldColor col = pick_color(s.format);
    int p[4];
    for (int i = 0; i < 4; i++)
        p[i] = (i & 1) ? rnd_range(s.y - 15, s.y + s.height + 15) : rnd_range(s.x - 15, s.x + s.width + 15);
    float fp[4];
    for (int i = 0; i < 4; i++)
        fp[i] = (float)((i & 1) ? rnd_range(s.y - 5, s.y + s.height + 5) : rnd_range(s.x - 5, s.x + s.width + 5)) + (rnd(3) ? 0.0f : 0.5f);
    for (int side = 0; side < 2; side++) {
        *object<Rect>(side, c.rect) = r;
        *object<Rect>(side, c.rect2) = r2;
        *object<FieldColor>(side, c.color) = col;
        memset(object<BYTE>(side, c.cblock), 0x33, 0x180);
        BYTE *tb = object<BYTE>(side, c.table);
        for (int i = 0; i < 256; i++)
            tb[i] = (BYTE)(255 - i);
        *object<BYTE *>(side, c.tptr) = tb;
        memcpy(object<int>(side, c.pts), p, 16);
        memcpy(object<float>(side, c.fpts), fp, 16);
    }
    return true;
}

// The drawing slots of a proxy-like object `obj(side)` with the case's arguments; `which` selects the slot
template <typename O> DWORD call_drawing(Case &c, int side, O obj, int which, SavedMemory &cache)
{
    void *o = obj(side);
    Rect *r = object<Rect>(side, c.rect);
    FieldColor *col = object<FieldColor>(side, c.color);
    const int *p = object<int>(side, c.pts);
    const DWORD *fp = object<DWORD>(side, c.fpts);
    switch (which) {
    case 0: return lo8(call_slot(o, 23, frame(side, c.src), object<Rect>(side, c.rect2), r, object<void>(side, c.mode)));
    case 1: call_slot(o, 24, r); return 0;
    case 2: return call_slot(o, 28, r, col);
    case 3: {
        call_slot(o, 57, r, object<BYTE *>(side, c.tptr));
        const DWORD h = cache.hash();
        cache.restore();
        return h;
    }
    case 4: return call_slot(o, 36, object<Rect>(side, c.out));
    case 5: return call_slot(o, 43);
    case 6: return call_slot(o, 44);
    case 7: call_slot(o, 51, p[0], p[1], p[2], p[3], col); return 0;
    case 8:
        // slot 52 rasterizes: end points the clip keeps inside
        call_slot(o, 52, fp[0], fp[1], fp[2], fp[3], col);
        return 0;
    case 9: return lo8(call_slot(o, 53, p[0], p[1], object<FieldColor>(side, c.out)));
    case 10: return call_slot(o, 54, p[0], p[1]);
    case 11: call_slot(o, 55, p[0], p[1], col); return 0;
    default: return lo8(call_slot(o, 56, p[0], p[1]));
    }
}

void check_offset_clipped(Tally &t)
{
    g_seed = 0x524840;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    SavedMemory cache;
    cache.add(0x6be388, 4);
    cache.add(0x6be3bc, 0x400);
    for (int k = 0; k < scaled(500); k++) {
        Case c;
        if (!make_case(c, random()))
            continue;
        char what[96];
        wsprintfA(what, "ocp %s %dx%d offset %d,%d clip (%d,%d,%d,%d)", format_name(c.s.format), c.s.width, c.s.height, c.dx, c.dy, c.clip.left,
                  c.clip.top, c.clip.right, c.clip.bottom);
        const int which = k % 13;
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            return call_drawing(c, side, [&](int sd) { return object<void>(sd, c.ocp); }, which, cache);
        });
        // SetClip, SetOffset, SetTarget
        switch (k % 3) {
        case 0:
            run_sides(t, g_hooks, "ocp SetClip", [&](int side) -> DWORD { return exe<DWORD>(0x524f50, object<void>(side, c.ocp), object<Rect>(side, c.rect)); });
            break;
        case 1:
            run_sides(t, g_hooks, "ocp SetOffset", [&](int side) -> DWORD {
                exe<void>(0x524fe0, object<void>(side, c.ocp), c.dy, c.dx);
                return 0;
            });
            break;
        default:
            run_sides(t, g_hooks, "ocp SetTarget", [&](int side) -> DWORD {
                call_slot(object<void>(side, c.ocp), 62, frame(side, c.fi2));
                return 0;
            });
            break;
        }
    }
    cache.release();
    sw.restore();
}

void check_parts(Tally &t)
{
    g_seed = 0x525080;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    SavedMemory cache;
    cache.add(0x6be388, 4);
    cache.add(0x6be3bc, 0x400);
    for (int k = 0; k < scaled(700); k++) {
        Case c;
        if (!make_case(c, random()))
            continue;
        const bool offset = k & 1;
        // the part aimed at the frame
        const int odx = rnd(2) ? c.dx : c.dx / 4;
        for (int side = 0; side < 2; side++) {
            OffsetClippedProxyFrame *o = object<OffsetClippedProxyFrame>(side, c.ocp);
            o->offset.proxy.target = frame(side, c.fi);
            o->offset.dx = odx;
            o->offset.dy = c.dy / 4;
        }
        auto part = [&](int side) -> void * {
            OffsetClippedProxyFrame *o = object<OffsetClippedProxyFrame>(side, c.ocp);
            return offset ? static_cast<void *>(&o->offset) : static_cast<void *>(&o->clip);
        };
        char what[96];
        wsprintfA(what, "%s frame %s %dx%d", offset ? "offset" : "clip", format_name(c.s.format), c.s.width, c.s.height);
        const int which = (k / 2) % 17;
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            void *o = part(side);
            if (which < 13) {
                if (offset && which == 9)   // GetColor: the colour moved by dy FieldColors
                    return lo8(call_slot(o, 53, object<int>(side, c.pts)[0], object<int>(side, c.pts)[1], object<BYTE>(side, c.cblock) + 0xa0));
                if (offset && (which == 5 || which == 6))
                    return 0;   // slots 43/44 of the offset frame are 4-byte getters of other classes
                return call_drawing(c, side, part, which, cache);
            }
            if (offset)
                return 0;
            switch (which) {
            case 13: return call_slot(o, 39);
            case 14: return call_slot(o, 45);
            case 15: return exe<DWORD>(0x46db90, o, object<Rect>(side, c.rect));
            default: return call_slot(o, 62, frame(side, c.fi2));
            }
        });
    }
    cache.release();
    sw.restore();
}

// CProxyFrame on a fake frame: the forwarding slots (exe addresses) with the case's arguments
struct Fwd {
    DWORD address;
    int args;   // -1: the case's rectangle; -2: rectangle and colour; -3: blit; -4: line; -5: point; -6: point and colour
};
constexpr Fwd FORWARDS[] = {
    {0x40b400, 0}, {0x40b2c0, -3}, {0x40b2f0, -1}, {0x40b2e0, 0}, {0x40b300, -7}, {0x40b310, 1}, {0x40b320, -2}, {0x40b3a0, 0},
    {0x40b390, 0}, {0x40b340, 0}, {0x40b350, 0}, {0x40b360, 0}, {0x40b370, 0}, {0x40b380, 0}, {0x40b3b0, -8}, {0x40b3d0, 0},
    {0x40b3e0, 0}, {0x40b3f0, 0}, {0x40b410, 0}, {0x40b420, 0}, {0x40b430, 0}, {0x40b440, 0}, {0x40b450, 0}, {0x40b460, 0},
    {0x40b470, 0}, {0x40b480, 0}, {0x40b490, 0}, {0x40b4a0, 0}, {0x40b4b0, 0}, {0x40b540, -4}, {0x40b520, -9}, {0x40b500, -5},
    {0x40b4e0, -6}, {0x40b4c0, -5}, {0x40b5a0, -10},
};

void check_proxy(Tally &t)
{
    g_seed = 0x40b2c0;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    SavedMemory cache;
    cache.add(0x6be388, 4);
    cache.add(0x6be3bc, 0x400);
    const int nf = (int)(sizeof FORWARDS / sizeof FORWARDS[0]);
    for (int k = 0; k < scaled(500); k++) {
        Case c;
        if (!make_case(c, random()))
            continue;
        const DWORD proxy = alloc_object(sizeof(ProxyFrame));
        for (int side = 0; side < 2; side++) {
            ProxyFrame *p = object<ProxyFrame>(side, proxy);
            p->vftable = render::VT_PROXY_FRAME;
            p->target = frame(side, c.fi);
        }
        const Fwd &f = FORWARDS[k % nf];
        const DWORD fill_value = rnd(256);
        char what[64];
        wsprintfA(what, "proxy slot at %x", f.address);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            void *o = object<void>(side, proxy);
            Rect *r = object<Rect>(side, c.rect);
            FieldColor *col = object<FieldColor>(side, c.color);
            const int *p = object<int>(side, c.pts);
            switch (f.args) {
            case 0: return exe<DWORD>(f.address, o);
            case 1: return exe<DWORD>(f.address, o, fill_value);
            case -1: exe<DWORD>(f.address, o, r); return 0;
            case -2: return exe<DWORD>(f.address, o, r, col);
            case -3: return lo8(exe<DWORD>(f.address, o, frame(side, c.src), object<Rect>(side, c.rect2), r, object<void>(side, c.mode)));
            case -4: exe<DWORD>(f.address, o, p[0], p[1], p[2], p[3], col); return 0;
            case -5: return exe<DWORD>(f.address, o, p[0], p[1]);
            case -6: exe<DWORD>(f.address, o, p[0], p[1], col); return 0;
            case -7: return exe<DWORD>(f.address, o, col);
            case -8: return exe<DWORD>(f.address, o, object<Rect>(side, c.out));
            case -9: return lo8(exe<DWORD>(f.address, o, p[0], p[1], object<FieldColor>(side, c.out)));
            default: {
                exe<DWORD>(f.address, o, r, object<BYTE *>(side, c.tptr));
                const DWORD h = cache.hash();
                cache.restore();
                return h;
            }
            }
        });
        run_sides(t, g_hooks, "proxy SetTarget", [&](int side) -> DWORD {
            exe<void>(0x54fdb0, object<void>(side, proxy), frame(side, c.fi2));
            return 0;
        });
    }
    cache.release();
    sw.restore();
}

// ---- Construction ----------------------------------------------------------------------------------------------

// An object of `words` dwords: pointers into itself by their offset, to the frame as such, the rest raw
void snap_object(Snap &s, const void *o, int words, const void *frame_object)
{
    const DWORD *d = static_cast<const DWORD *>(o);
    for (int i = 0; i < words; i++) {
        const DWORD v = d[i];
        // pointers into the object itself or to the frame by what they point at
        if (words == 15 && !frame_object && i >= 11)
            s.add(0);   // the clip rectangle of a new COffsetClippedProxyFrame: its constructor does not set it (heap rest)
        else if (v >= ptrv(o) && v < ptrv(o) + (DWORD)words * 4)
            s.add(0xc0000000u | (v - ptrv(o)));
        else if (frame_object && v == ptrv(frame_object))
            s.add(0xf0000000u);
        else
            s.add(v);
    }
}

void check_construction(Tally &t)
{
    g_seed = 0x524770;
    alignas(16) BYTE mem[2][0x40];
    BYTE fake_frame[2][0x40];
    for (int k = 0; k < scaled(60); k++) {
        const DWORD op = (DWORD)k % 6;
        const WORD fw = (WORD)rnd(100), fh = (WORD)rnd(100);
        const short fx = (short)rnd_range(-9, 9);
        for (int side = 0; side < 2; side++) {
            memset(fake_frame[side], 0, sizeof fake_frame[side]);
            *reinterpret_cast<DWORD *>(fake_frame[side]) = render::VT_CFRAME;
            *reinterpret_cast<WORD *>(fake_frame[side] + 0x12) = fw;
            *reinterpret_cast<WORD *>(fake_frame[side] + 0x14) = fh;
            *reinterpret_cast<short *>(fake_frame[side] + 8) = fx;
        }
        compare_sides(t, g_hooks, "proxy construction", [&](int side) -> Snap {
            Snap s;
            memset(mem[side], 0xcc, sizeof mem[side]);
            OffsetClippedProxyFrame *o = reinterpret_cast<OffsetClippedProxyFrame *>(mem[side]);
            switch (op) {
            case 0: s.add(exe<void *>(0x524770, o) == o); break;
            case 1: s.add(exe<void *>(0x5247c0, o, fake_frame[side]) == o); break;
            case 2: s.add(exe<void *>(0x525000, o) == o); break;
            case 3: s.add(exe<void *>(0x46d790, o) == o); break;
            case 4:
                original<void *(THISCALL *)(void *)>(0x524770)(o);
                exe<void>(0x524f10, o, fake_frame[side]);
                break;
            default:
                original<void *(THISCALL *)(void *, void *)>(0x5247c0)(o, fake_frame[side]);
                s.add(exe<DWORD>(0x46dbb0, &o->clip, fake_frame[side]));
                break;
            }
            snap_object(s, o, 15, fake_frame[side]);
            return s;
        });
    }
    // Create, CreateArray and the deleting destructors on heap objects
    struct Class {
        DWORD create, create_array, dtor, size;
    };
    const Class CLASSES[2] = {{0x54fbf0, 0x54fc40, 0x54fcb0, 0xc}, {0x5244a0, 0x524500, 0x5245a0, 0x3c}};
    for (int k = 0; k < scaled(60); k++) {
        const Class &c = CLASSES[k & 1];
        const int count = rnd(4) ? rnd_range(1, 4) : 0;
        compare_sides(t, g_hooks, "proxy Create/CreateArray/DeletingDtor", [&](int) -> Snap {
            Snap s;
            BYTE *one = at<BYTE *(__cdecl *)()>(c.create)();
            s.add(one != nullptr);
            if (one) {
                snap_object(s, one, (int)(c.size / 4), nullptr);
                s.add(at<BYTE *(THISCALL *)(BYTE *, DWORD)>(c.dtor)(one, 1) == one);
            }
            BYTE *arr = at<BYTE *(__fastcall *)(int, int, int)>(c.create_array)(count, 0, 0);
            s.add(arr != nullptr);
            if (arr) {
                s.add(reinterpret_cast<DWORD *>(arr)[-1]);
                for (int i = 0; i < count; i++) {
                    Snap e;
                    snap_object(e, arr + i * c.size, (int)(c.size / 4), nullptr);
                    s.add_bytes(e.v, c.size);
                }
                s.add(at<BYTE *(THISCALL *)(BYTE *, DWORD)>(c.dtor)(arr, 3) == arr - 4);
            }
            return s;
        });
    }
    // the deleting destructors of the parts and of CFrameInterface on pool blocks of their sizes
    struct Part {
        DWORD ctor, dtor, size, vtable;
    };
    const Part PARTS[3] = {{0x525000, 0x525040, 0x14, 0}, {0x46d790, 0x46d800, 0x1c, 0}, {0, 0x40b290, 8, render::VT_FRAME_INTERFACE}};
    for (int k = 0; k < scaled(30); k++) {
        const Part &p = PARTS[k % 3];
        const DWORD flags = rnd(2);
        compare_sides(t, g_hooks, "part DeletingDtor", [&](int) -> Snap {
            Snap s;
            BYTE *o = static_cast<BYTE *>(pool_alloc(p.size));
            if (p.ctor)
                at<void *(THISCALL *)(void *)>(p.ctor)(o);
            else {
                render::obj_ctor(o);
                *reinterpret_cast<DWORD *>(o) = p.vtable;
            }
            s.add(at<BYTE *(THISCALL *)(BYTE *, DWORD)>(p.dtor)(o, flags) == o);
            if (!(flags & 1)) {
                s.add_bytes(o, 8);
                pool_free(o, p.size);
            }
            return s;
        });
    }
    // the class getters
    constexpr DWORD GETTERS[] = {0x4b2bc0, 0x54fba0, 0x524450};
    for (DWORD g : GETTERS)
        compare_sides(t, g_hooks, "proxy class getter", [&](int) -> Snap {
            Snap s;
            s.add(ptrv(at<void *(__cdecl *)()>(g)()));
            return s;
        });
    constexpr DWORD SLOTS[] = {0x54fd50, 0x524710};
    for (DWORD g : SLOTS)
        compare_sides(t, g_hooks, "proxy class slot", [&](int) -> Snap {
            Snap s;
            s.add(ptrv(at<void *(THISCALL *)(void *)>(g)(nullptr)));
            return s;
        });
}

}  // namespace

ZAX_CHECKS(frame_proxy)
{
    (void)cls;
    (void)n;
    Tally ocp{"frame proxy: COffsetClippedProxyFrame slots"};
    check_offset_clipped(ocp);
    ocp.report();
    Tally parts{"frame proxy: the offset and the clip frame"};
    check_parts(parts);
    parts.report();
    Tally proxy{"frame proxy: CProxyFrame forwarding"};
    check_proxy(proxy);
    proxy.report();
    Tally cons{"frame proxy: construction, destruction, classes"};
    check_construction(cons);
    cons.report();
}

}  // namespace zax
