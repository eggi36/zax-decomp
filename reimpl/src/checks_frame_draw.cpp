// Checks of frame_draw.cpp with the frame check harness, in the game thread.
//  - Mem_FillBytes on buffers of every alignment and length (dword values, not only repeated bytes).
//  - Rect_Intersect and Blit_ClipRects: random, empty, inverted and outside rectangles; eax whole.
//  - Line_ClipToRect and Line_ClipIntersect (integer), Line_ClipToRectF and Line_ClipIntersectF (float,
//    fractional coordinates and rectangles): end points inside, outside, on the edges, vertical and
//    horizontal lines; al and the end points.
//  - The blit wrappers (BlitAt, BlitAtClipped, BlitRectAt) between fake frames with copies of the Copy and
//    Transparent Mask objects; the rectangle and line helpers and the energy bar on fake frames of every
//    format (their slot calls through the recording vtables).
#include "checks_frame_r1.h"

namespace zax {

int hooks_frame_draw(Hook *out);

namespace {

using namespace r1;
using render::Point;
using render::RectF;

ModuleHooks g_hooks{"frame_draw", &hooks_frame_draw, {}, {}, {}, 0, true};

template <typename R, typename... A> R exe(DWORD address, A... a) { return at<R(THISCALL *)(A...)>(address)(a...); }
DWORD lo8(DWORD v) { return v & 0xff; }

Rect pick_rect(int span)
{
    Rect r;
    r.left = rnd_range(-span, span);
    r.top = rnd_range(-span, span);
    switch (rnd(6)) {
    case 0: r.right = r.left - rnd_range(0, 8); r.bottom = r.top + rnd_range(0, 20); break;   // inverted or empty
    case 1: r.right = r.left + rnd_range(0, 20); r.bottom = r.top - rnd_range(0, 8); break;
    default: r.right = r.left + rnd_range(0, span); r.bottom = r.top + rnd_range(0, span); break;
    }
    return r;
}

void check_fill_bytes(Tally &t)
{
    g_seed = 0x432c10;
    for (int k = 0; k < scaled(2000); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = PAL8;
        s.width = 300;
        s.height = 1;
        const int fi = make_frame(s);
        const DWORD off = rnd(8), n = rnd(4) ? rnd(40) : rnd(290);
        const DWORD v = rnd(3) ? random() : rnd(256) * 0x01010101u;
        run_sides(t, g_hooks, "Mem_FillBytes", [&](int side) -> DWORD {
            return at<DWORD(__fastcall *)(BYTE *, DWORD, DWORD)>(0x432c10)(frame(side, fi)->pixels + off, v, n);
        });
    }
}

void check_rects(Tally &t)
{
    g_seed = 0x4b2cd0;
    for (int k = 0; k < scaled(4000); k++) {
        begin_case(random());
        const DWORD clip = alloc_object(sizeof(Rect)), a = alloc_object(sizeof(Rect)), b = alloc_object(sizeof(Rect));
        const Rect c = pick_rect(40), r1 = pick_rect(60), r2 = pick_rect(60);
        for (int side = 0; side < 2; side++) {
            *object<Rect>(side, clip) = c;
            *object<Rect>(side, a) = r1;
            *object<Rect>(side, b) = r2;
        }
        if (k & 1)
            run_sides(t, g_hooks, "Rect_Intersect", [&](int side) -> DWORD {
                return at<DWORD(__fastcall *)(const Rect *, Rect *)>(0x4b2cd0)(object<Rect>(side, clip), object<Rect>(side, a));
            });
        else
            run_sides(t, g_hooks, "Blit_ClipRects", [&](int side) -> DWORD {
                // eax: al the result, above it the destination's address (compared as such: it may lie below the arena)
                Rect *dst = object<Rect>(side, b);
                const DWORD r = at<DWORD(__fastcall *)(const Rect *, Rect *, Rect *)>(0x4b2c20)(object<Rect>(side, clip), object<Rect>(side, a), dst);
                return (r & 0xff) | ((r & ~0xffu) == (ptrv(dst) & ~0xffu) ? 0x100 : 0);
            });
    }
}

int pick_coord(int lo, int hi)
{
    switch (rnd(8)) {
    case 0: return lo;
    case 1: return hi - 1;
    case 2: return hi;
    case 3: return lo - 1;
    case 4: return rnd_range(lo - 200, hi + 200);
    default: return rnd_range(lo - 20, hi + 20);
    }
}

void check_lines(Tally &t)
{
    g_seed = 0x4b2d30;
    for (int k = 0; k < scaled(6000); k++) {
        begin_case(random());
        const DWORD clip = alloc_object(sizeof(Rect)), pts = alloc_object(16);
        Rect c;
        c.left = rnd_range(-30, 30);
        c.top = rnd_range(-30, 30);
        c.right = c.left + rnd_range(1, 80);
        c.bottom = c.top + rnd_range(1, 60);
        int p[4];
        for (int i = 0; i < 4; i++)
            p[i] = (i & 1) ? pick_coord(c.top, c.bottom) : pick_coord(c.left, c.right);
        if (rnd(6) == 0)
            p[2] = p[0];
        if (rnd(6) == 0)
            p[3] = p[1];
        for (int side = 0; side < 2; side++) {
            *object<Rect>(side, clip) = c;
            memcpy(object<int>(side, pts), p, 16);
        }
        run_sides(t, g_hooks, "Line_ClipToRect", [&](int side) -> DWORD {
            int *q = object<int>(side, pts);
            return lo8(at<DWORD(__fastcall *)(const Rect *, int *, int *, int *, int *)>(0x4b2d30)(object<Rect>(side, clip), q, q + 1, q + 2, q + 3));
        });
        // the intersection directly (dx and dy not 0, as Line_ClipToRect calls it)
        if (p[0] != p[2] && p[1] != p[3]) {
            const DWORD code = rnd(3) ? 1 + rnd(15) : (DWORD)random();
            run_sides(t, g_hooks, "Line_ClipIntersect", [&](int side) -> DWORD {
                int *q = object<int>(side, pts);
                at<void(__fastcall *)(const Rect *, int, int, int *, int *, DWORD)>(0x4b3d30)(object<Rect>(side, clip), p[2], p[3], q, q + 1, code);
                return 0;
            });
        }
    }
}

float pick_coord_f(float lo, float hi)
{
    switch (rnd(9)) {
    case 0: return lo;
    case 1: return hi - 1.0f;
    case 2: return hi;
    case 3: return lo - 0.5f;
    case 4: return (float)rnd_range((int)lo - 200, (int)hi + 200) + (float)rnd(1000) / 1000.0f;
    case 5: return (float)rnd_range((int)lo - 20, (int)hi + 20);
    default: return (float)rnd_range((int)lo - 20, (int)hi + 20) + (float)rnd(64) / 64.0f;
    }
}

void check_lines_f(Tally &t)
{
    g_seed = 0x4b2f50;
    for (int k = 0; k < scaled(6000); k++) {
        begin_case(random());
        const DWORD clip = alloc_object(sizeof(RectF)), pts = alloc_object(16);
        RectF c;
        c.left = (float)rnd_range(-30, 30) + (rnd(3) ? 0.0f : 0.25f);
        c.top = (float)rnd_range(-30, 30) + (rnd(3) ? 0.0f : 0.5f);
        c.right = c.left + (float)rnd_range(1, 80) + (rnd(3) ? 0.0f : 0.75f);
        c.bottom = c.top + (float)rnd_range(1, 60);
        float p[4];
        for (int i = 0; i < 4; i++)
            p[i] = (i & 1) ? pick_coord_f(c.top, c.bottom) : pick_coord_f(c.left, c.right);
        if (rnd(6) == 0)
            p[2] = p[0];
        if (rnd(6) == 0)
            p[3] = p[1];
        for (int side = 0; side < 2; side++) {
            *object<RectF>(side, clip) = c;
            memcpy(object<float>(side, pts), p, 16);
        }
        run_sides(t, g_hooks, "Line_ClipToRectF", [&](int side) -> DWORD {
            float *q = object<float>(side, pts);
            return lo8(at<DWORD(__fastcall *)(const RectF *, float *, float *, float *, float *)>(0x4b2f50)(object<RectF>(side, clip), q, q + 1, q + 2,
                                                                                                            q + 3));
        });
        if (p[0] != p[2] && p[1] != p[3]) {
            const DWORD code = 1 + rnd(15);
            run_sides(t, g_hooks, "Line_ClipIntersectF", [&](int side) -> DWORD {
                float *q = object<float>(side, pts);
                at<void(__fastcall *)(const RectF *, float *, float, float, float *, DWORD)>(0x4b3e50)(object<RectF>(side, clip), q, p[2], p[3], q + 1,
                                                                                                      code);
                return 0;
            });
        }
    }
}

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

void check_blit_wrappers(Tally &t)
{
    g_seed = 0x4b3280;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    for (int k = 0; k < scaled(600); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = rnd(2) ? PAL8 : RGB565;
        s.width = rnd_range(1, 40);
        s.height = rnd_range(1, 30);
        s.x = rnd(2) ? 0 : rnd_range(-6, 6);
        s.y = rnd(2) ? 0 : rnd_range(-6, 6);
        const int src = make_frame(s);
        FrameSpec d;
        d.format = s.format == PAL8 && rnd(3) ? PAL8 : RGB565;
        d.width = rnd_range(1, 70);
        d.height = rnd_range(1, 40);
        d.x = rnd(2) ? 0 : rnd_range(-8, 8);
        d.y = rnd(2) ? 0 : rnd_range(-8, 8);
        const int dst = make_frame(d);
        if (src < 0 || dst < 0)
            continue;
        const DWORD mode = copy_mode_object(rnd(3) ? render::COPY_MODE : 0x6bcc68);
        const DWORD clip = alloc_object(sizeof(Rect)), rect = alloc_object(sizeof(Rect));
        const Rect c = {rnd_range(d.x - 5, d.x + d.width), rnd_range(d.y - 5, d.y + d.height), rnd_range(d.x, d.x + d.width + 5),
                        rnd_range(d.y, d.y + d.height + 5)};
        Rect r;
        r.left = s.x + rnd_range(0, s.width - 1);
        r.top = s.y + rnd_range(0, s.height - 1);
        r.right = r.left + rnd_range(1, s.x + s.width - r.left);
        r.bottom = r.top + rnd_range(1, s.y + s.height - r.top);
        for (int side = 0; side < 2; side++) {
            *object<Rect>(side, clip) = c;
            *object<Rect>(side, rect) = r;
        }
        const int x = rnd_range(d.x - 30, d.x + d.width + 5), y = rnd_range(d.y - 30, d.y + d.height + 5);
        char what[96];
        wsprintfA(what, "blit wrapper %u %s %dx%d -> %s %dx%d at %d,%d", (DWORD)k % 3, format_name(s.format), s.width, s.height, format_name(d.format),
                  d.width, d.height, x, y);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            void *m = object<void>(side, mode);
            switch (k % 3) {
            case 0: return lo8(exe<DWORD>(0x4b3280, frame(side, dst), frame(side, src), x, y, m));
            case 1: return lo8(exe<DWORD>(0x4b3300, frame(side, dst), frame(side, src), x, y, object<Rect>(side, clip), m));
            default: return lo8(exe<DWORD>(0x4b3390, frame(side, dst), frame(side, src), object<Rect>(side, rect), x, y, m));
            }
        });
    }
    sw.restore();
}

void check_helpers(Tally &t)
{
    g_seed = 0x4b3430;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    SavedMemory cache;
    cache.add(0x6be388, 4);
    cache.add(0x6be3bc, 0x400);
    for (int k = 0; k < scaled(1200); k++) {
        begin_case(random());
        FrameSpec s;
        const DWORD fk = rnd(20);
        s.format = fk < 7 ? PAL8 : fk < 15 ? RGB565 : fk < 18 ? RGB24 : RGB32;
        s.width = rnd_range(1, 64);
        s.height = rnd_range(1, 40);
        s.pitch = rnd(3) ? s.width : s.width + rnd_range(1, 7);
        s.x = rnd(3) ? 0 : rnd_range(-10, 10);
        s.y = rnd(3) ? 0 : rnd_range(-10, 10);
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        const DWORD c1 = alloc_object(sizeof(FieldColor)), c2 = alloc_object(sizeof(FieldColor)), rect = alloc_object(sizeof(Rect)),
                    pts = alloc_object(16), table = alloc_object(256), tptr = alloc_object(4);
        const FieldColor a = pick_color(s.format), b = pick_color(s.format);
        Rect r;
        r.left = rnd_range(s.x - 10, s.x + s.width);
        r.top = rnd_range(s.y - 10, s.y + s.height);
        r.right = r.left + rnd_range(rnd(8) ? 1 : -3, s.width + 10);
        r.bottom = r.top + rnd_range(rnd(8) ? 1 : -3, s.height + 10);
        const DWORD op = (DWORD)k % 8;
        if (op == 3) {   // FillRect with an inverted rectangle fills a negative width (Rect_Intersect passes it): left out
            if (r.right < r.left)
                r.right = r.left;
            if (r.bottom < r.top)
                r.bottom = r.top;
        }
        for (int side = 0; side < 2; side++) {
            *object<FieldColor>(side, c1) = a;
            *object<FieldColor>(side, c2) = b;
            *object<Rect>(side, rect) = r;
            BYTE *tb = object<BYTE>(side, table);
            for (int i = 0; i < 256; i++)
                tb[i] = (BYTE)(i * 13 + k);
            *object<BYTE *>(side, tptr) = tb;
        }
        if (op == 6) {   // float points inside the frame for slot 52
            const float f[4] = {(float)(s.x + rnd(s.width)), (float)(s.y + rnd(s.height)), (float)(s.x + rnd(s.width)), (float)(s.y + rnd(s.height))};
            for (int side = 0; side < 2; side++)
                memcpy(object<int>(side, pts), f, 16);
        }
        if (op == 5) {
            int p[4];
            for (int i = 0; i < 4; i++)
                p[i] = (i & 1) ? rnd_range(s.y - 15, s.y + s.height + 15) : rnd_range(s.x - 15, s.x + s.width + 15);
            for (int side = 0; side < 2; side++)
                memcpy(object<int>(side, pts), p, 16);
        }
        if (op == 6 && s.format != PAL8 && s.format != RGB565)
            continue;
        char what[96];
        wsprintfA(what, "helper %u %s %dx%d rect (%d,%d,%d,%d)", op, format_name(s.format), s.width, s.height, r.left, r.top, r.right, r.bottom);
        if (g_frame_trace)
            log("  case %d: %s", k, what);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            FrameData *f = frame(side, fi);
            const Rect *q = object<Rect>(side, rect);
            const Point *p = object<Point>(side, pts);
            switch (op) {
            case 0: exe<DWORD>(0x4b3430, f, q, object<FieldColor>(side, c1)); return 0;
            case 1: exe<DWORD>(0x4b33f0, f, q->left, q->top, q->right, q->bottom, object<FieldColor>(side, c1)); return 0;
            case 2: exe<DWORD>(0x4b34b0, f, q, object<FieldColor>(side, c1), object<FieldColor>(side, c2)); return 0;
            case 3: return exe<DWORD>(0x4b3a70, f, q->left, q->top, q->right, q->bottom, object<FieldColor>(side, c1));
            case 4: {
                exe<void>(0x4b3ce0, f, q->left, q->top, q->right, q->bottom, object<BYTE *>(side, tptr));
                const DWORD h = cache.hash();
                cache.restore();
                return h;
            }
            case 5: exe<DWORD>(0x4b3c80, f, p, p + 1, object<FieldColor>(side, c1)); return 0;
            case 6: exe<DWORD>(0x4b3cb0, f, p, p + 1, object<FieldColor>(side, c1)); return 0;
            default: exe<void>(0x4b3ab0, f, q, object<FieldColor>(side, c1), object<FieldColor>(side, c2)); return 0;
            }
        });
    }
    cache.release();
    sw.restore();
}

}  // namespace

ZAX_CHECKS(frame_draw)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    Tally fill{"frame draw: Mem_FillBytes"};
    check_fill_bytes(fill);
    fill.report();
    Tally rects{"frame draw: Rect_Intersect, Blit_ClipRects"};
    check_rects(rects);
    rects.report();
    Tally lines{"frame draw: Line_ClipToRect, Line_ClipIntersect"};
    check_lines(lines);
    lines.report();
    Tally lines_f{"frame draw: Line_ClipToRectF, Line_ClipIntersectF"};
    check_lines_f(lines_f);
    lines_f.report();
    Tally blits{"frame draw: BlitAt, BlitAtClipped, BlitRectAt"};
    check_blit_wrappers(blits);
    blits.report();
    Tally helpers{"frame draw: rectangles, lines, the energy bar"};
    check_helpers(helpers);
    helpers.report();
}

}  // namespace zax
