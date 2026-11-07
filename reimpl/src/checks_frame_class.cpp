// Checks of frame_class.cpp with the frame check harness, in the game thread.
//  - Getters, depth, size and origin slots on random fake frames of every format and odd depths.
//  - The pixel slots (SetPixel with the five 8-bit blend levels, GetPixel, GetColor, IsPixelSet, the byte
//    access) on plain frames, synthetic RLE frames (8- and 16-bit RLE, every block, some rows starting with
//    a 63-pixel segment, odd depth/compression combinations) and real compressed frames of the level; the
//    three RLE pixel tests directly; the bounding box.
//  - ClearRect, FillColor, TintRect (8 bits through a table, 16 and 24 bits through the game palette; the
//    24-bit cache 0x6be388/0x6be3bc saved), DrawLine (every format, end points outside, origins),
//    DrawThickLine (end points inside), Blit and BlitWithMode (plain and compressed sources, copies of the
//    Copy and Transparent Mask objects).
//  - The functions that allocate, by content: constructor, destructor, Create, CreateArray and the deleting
//    destructors of CFrame, CDIBFrame and CPaletteHolder, SetPitch, Allocate, Free, SetSize, Resize,
//    CopyRect, CopyFrom (the live Copy mode's cache saved), the palette slots (the holder dropped again), the
//    class getters, the file extension.
#include "checks_frame_r1.h"

namespace zax {

int hooks_frame_class(Hook *out);

namespace {

using namespace r1;
using render::CFrame;
using render::Palette;

constexpr DWORD PICK_0[] = {0, 0x60, 0x1e0, 0xa0, 0xc0};
constexpr DWORD PICK_1[] = {8, 16, 24, 32, 0, 12, 7, 33, 64};
constexpr DWORD PICK_2[] = {0x20, 0x40, 0x80};
constexpr DWORD PICK_3[] = {0, 0x60, 0x1e0, 0xa0};
constexpr DWORD PICK_4[] = {0x22, 0x44, 0x2044};
constexpr DWORD PICK_5[] = {0, 0x60, 0x1e0};
constexpr DWORD PICK_6[] = {8, 16, 24, 32, 8, 16, 12};
constexpr DWORD PICK_7[] = {8, 16, 24, 32, 0};
template <unsigned N> DWORD pick(const DWORD (&a)[N]) { return a[rnd(N)]; }

constexpr DWORD GETTERS[] = {0x431b40, 0x431b50, 0x431b60, 0x431b70, 0x431b80, 0x431b90, 0x4aa3d0, 0x4aa580, 0x4aa510, 0x4aad10, 0x4ab9e0};
constexpr DWORD RLE_FLAGS[] = {0x22, 0x22, 0x44, 0x44, 0x24, 0x84, 0x42, 0x26, 0x104, 0x2044};
constexpr DWORD CLASS_FUNCS[] = {0x4a8760, 0x4a84a0};
constexpr DWORD CLASS_SLOTS[] = {0x4a8920, 0x4a7980, 0x4a8600};

ModuleHooks g_hooks{"frame_class", &hooks_frame_class, {}, {}, {}, 0, true};
Sources g_sources;

Format pick_format()
{
    const DWORD k = rnd(20);
    return k < 6 ? PAL8 : k < 12 ? RGB565 : k < 14 ? RGB555 : k < 17 ? RGB24 : RGB32;
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
FrameSpec pick_spec(int max_w = 64, int max_h = 40)
{
    FrameSpec s;
    s.format = pick_format();
    s.width = rnd(10) ? rnd_range(1, max_w) : rnd_range(0, 2);
    s.height = rnd(10) ? rnd_range(1, max_h) : rnd_range(0, 2);
    s.pitch = rnd(3) ? s.width : s.width + rnd_range(1, 9);
    s.x = rnd(2) ? 0 : rnd_range(-20, 20);
    s.y = rnd(2) ? 0 : rnd_range(-20, 20);
    s.dib = rnd(6) == 0;
    return s;
}
// Frames that start with sparse pixels: mostly 0
void sparse(int fi, DWORD keep_one_in)
{
    const FrameInfo &f = g_h.frames[fi];
    for (int side = 0; side < 2; side++) {
        BYTE *p = g_h.arena[side].pixels + f.buffer;
        unsigned s = g_h.seed * 7 + (unsigned)fi;
        for (DWORD i = 0; i < f.bytes; i++)
            if (pattern_next(s) % keep_one_in)
                p[i] = 0;
    }
}
template <typename R, typename... A> R exe(DWORD address, A... a) { return at<R(THISCALL *)(A...)>(address)(a...); }
DWORD lo8(DWORD v) { return v & 0xff; }

// ---- Getters, depth, size, origin ---------------------------------------------------------------------------

void check_getters(Tally &t)
{
    g_seed = 0x431b40;
    static_assert(sizeof(CFrame) == sizeof(FrameData));
    for (int k = 0; k < scaled(300); k++) {
        begin_case(random());
        FrameSpec s = pick_spec();
        const int fi = make_frame(s);
        if (fi < 0) {
            t.check(false, "", "[the case does not fit]");
            continue;
        }
        DWORD flags = format_flag(s.format);
        if (rnd(4) == 0)
            flags = pick(PICK_0);
        flags |= rnd(3) == 0 ? 0x4000 : 0;
        flags |= rnd(5) == 0 ? 0x2000 : 0;
        if (rnd(6) == 0)
            sparse(fi, 1000000);   // all zero
        const short ox = (short)(rnd(3) ? rnd_range(-300, 300) : (int)random());
        for (int side = 0; side < 2; side++) {
            frame(side, fi)->flags = flags;
            frame(side, fi)->x = ox;
        }
        char what[96];
        wsprintfA(what, "getters %s %dx%d flags %x", format_name(s.format), s.width, s.height, flags);
        for (DWORD g : GETTERS)
            run_sides(t, g_hooks, what, [&](int side) -> DWORD {
                const DWORD v = exe<DWORD>(g, frame(side, fi));
                return g == 0x4aad10 || g == 0x4ab9e0 ? lo8(v) : v;
            });
        const DWORD bits = pick(PICK_1);
        run_sides(t, g_hooks, "SetDepth", [&](int side) -> DWORD {
            exe<void>(0x4aa240, frame(side, fi), bits);
            return 0;
        });
        const DWORD a = rnd(3) ? (DWORD)rnd_range(-50, 50) : random(), b = rnd(3) ? (DWORD)rnd_range(-50, 50) : random();
        run_sides(t, g_hooks, "SetOrigin", [&](int side) -> DWORD {
            exe<void>(0x4abaa0, frame(side, fi), a, b);
            return 0;
        });
        run_sides(t, g_hooks, "MoveOrigin", [&](int side) -> DWORD {
            exe<void>(0x4abac0, frame(side, fi), b, a);
            return 0;
        });
        if (s.width && s.height) {
            const DWORD x = rnd(s.width), y = rnd(s.height);
            run_sides(t, g_hooks, "ByteAt", [&](int side) -> DWORD { return exe<DWORD>(0x4aad30, frame(side, fi), x, y); });
        }
        const int x = rnd_range(-25, s.width + 25), y = rnd_range(-25, s.height + 25);
        const BYTE v = (BYTE)rnd(256);
        for (int side = 0; side < 2; side++)
            frame(side, fi)->x = (short)s.x;
        run_sides(t, g_hooks, "SetByte", [&](int side) -> DWORD {
            exe<void>(0x4a8b60, frame(side, fi), x, y, (DWORD)v);
            return 0;
        });
        run_sides(t, g_hooks, "ClearPointers", [&](int side) -> DWORD { return exe<DWORD>(0x4a9d00, frame(side, fi)); });
    }
    // MemorySize: plain frames (slot 30 * height * width) and synthetic compressed ones (the block sizes)
    for (int k = 0; k < scaled(300); k++) {
        begin_case(random());
        int fi;
        if (k & 1) {
            fi = make_frame(pick_spec());
        } else {
            const DWORD depth = pick(PICK_2);
            fi = make_rle_frame(depth | (rnd(2) ? 2 : 4), rnd_range(1, 70), rnd_range(1, 20), 1 + rnd(3), 1 | rnd(32), random());
        }
        if (fi < 0) {
            t.check(false, "", "[MemorySize case does not fit]");
            continue;
        }
        run_sides(t, g_hooks, "MemorySize", [&](int side) -> DWORD { return exe<DWORD>(0x4aca00, frame(side, fi)); });
    }
}

// ---- Pixels -------------------------------------------------------------------------------------------------------

// A frame of a pixel case: plain (any format), synthetic RLE or a copy of a real compressed frame
int pixel_frame(int k, FrameSpec &s, DWORD &flags)
{
    const DWORD kind = (DWORD)k % 4;
    if (kind < 2) {
        s = pick_spec();
        const int fi = make_frame(s);
        flags = format_flag(s.format);
        if (rnd(8) == 0)
            flags = pick(PICK_3);
        if (fi >= 0)
            for (int side = 0; side < 2; side++)
                frame(side, fi)->flags = flags;
        if (fi >= 0 && rnd(3) == 0)
            sparse(fi, 3);
        return fi;
    }
    if (kind == 2 || !g_sources.n) {
        flags = pick(RLE_FLAGS);
        // bytes per pixel as the tests parse the frame: 8-bit RLE 1, 16-bit RLE by depth (IsPixelSet), 2 for 32 bits
        const DWORD depth = flags & 0x1e0;
        const DWORD ps = (flags & 6) == 2 ? 1 : depth == 0x20 ? 1 : depth == 0x80 ? 3 : 2;
        s.width = rnd_range(1, 90);
        s.height = rnd_range(1, 24);
        s.x = rnd(2) ? 0 : rnd_range(-10, 10);
        s.y = rnd(2) ? 0 : rnd_range(-10, 10);
        const int fi = make_rle_frame(flags, s.width, s.height, ps, rnd(5) ? 1 | rnd(32) : rnd(32), random());
        if (fi >= 0)
            for (int side = 0; side < 2; side++) {
                frame(side, fi)->x = (short)s.x;
                frame(side, fi)->y = (short)s.y;
            }
        return fi;
    }
    // a real compressed frame of the level
    for (int tries = 0; tries < 8; tries++) {
        const Source &src = g_sources.list[rnd((DWORD)g_sources.n)];
        if (!compressed_sane(src.frame))
            continue;
        const int fi = copy_compressed(src.frame);
        if (fi < 0)
            return -1;
        s.width = src.frame->width;
        s.height = src.frame->height;
        s.x = src.frame->x;
        s.y = src.frame->y;
        flags = src.frame->flags;
        return fi;
    }
    s = pick_spec();
    flags = format_flag(s.format);
    return make_frame(s);
}

void check_pixels(Tally &t)
{
    g_seed = 0x4a8bb0;
    SavedMemory level;
    level.add(0x6be7d8, 4);
    Pixel16Tables rounding;
    for (int k = 0; k < scaled(320); k++) {
        begin_case(random());
        FrameSpec s;
        DWORD flags = 0;
        const int fi = pixel_frame(k, s, flags);
        if (fi < 0) {
            t.check(false, "", "[pixel case does not fit]");
            continue;
        }
        const DWORD color = alloc_object(sizeof(FieldColor)), out = alloc_object(sizeof(FieldColor));
        const FieldColor c = pick_color(s.format);
        for (int side = 0; side < 2; side++) {
            *object<FieldColor>(side, color) = c;
            memset(object<FieldColor>(side, out), 0x5a, sizeof(FieldColor));
        }
        char what[96];
        wsprintfA(what, "pixels %dx%d flags %x origin %d,%d", s.width, s.height, flags, s.x, s.y);
        if (g_frame_trace)
            log("  case %d: %s", k, what);
        for (int n = 0; n < 6; n++) {
            int x = rnd_range(s.x - 3, s.x + s.width + 3), y = rnd_range(s.y - 2, s.y + s.height + 2);
            if (n == 0)
                x = s.x + 0x3f;
            if (n == 1)
                x = s.x;
            if (g_frame_trace > 1)
                log("    (%d, %d)", x, y);
            run_sides(t, g_hooks, what, [&](int side) -> DWORD { return exe<DWORD>(0x4ab120, frame(side, fi), x, y); });
            if (g_frame_trace > 1)
                log("    GetPixel done");
            // GetColor parses a 16-bit RLE frame with 2 bytes per pixel whatever its depth: not on 8/24-bit ones (it
            // would read past the block, the original as well)
            // On a drawn pixel of a 16-bit RLE frame it passes r = 0xfffff8 to FieldColor_SetRGBA, whose Pixel16_FromRGB
            // indexes its tables with it unclipped (a read 64 MB past them; the original crashes when that is not
            // mapped): such frames with the rounding path (tables "not built")
            if (!((flags & 6) == 4 && ((flags & 0x1e0) == 0x20 || (flags & 0x1e0) == 0x80))) {
                if ((flags & 6) == 4)
                    rounding.set(false);
                run_sides(t, g_hooks, what, [&](int side) -> DWORD { return lo8(exe<DWORD>(0x4aad60, frame(side, fi), x, y, object<FieldColor>(side, out))); });
                rounding.restore();
            }
            if (g_frame_trace > 1)
                log("    GetColor done");
            run_sides(t, g_hooks, what, [&](int side) -> DWORD { return lo8(exe<DWORD>(0x4aa880, frame(side, fi), x, y)); });
            if (g_frame_trace > 1)
                log("    IsPixelSet done");
            if (!(flags & F_COMPRESSED)) {
                *at<DWORD *>(0x6be7d8) = rnd(7);
                run_sides(t, g_hooks, what, [&](int side) -> DWORD {
                    exe<void>(0x4a8bb0, frame(side, fi), x, y, object<FieldColor>(side, color));
                    return 0;
                });
                level.restore();
            }
        }
    }
    // the three RLE pixel tests on the blocks of synthetic frames (and no block)
    for (int k = 0; k < scaled(500); k++) {
        begin_case(random());
        const DWORD ps = 1 + (DWORD)k % 3;
        const int w = rnd_range(1, 90), h = rnd_range(1, 10);
        const int fi = make_rle_frame(0x40 | 4, w, h, ps, 1, random());
        if (fi < 0)
            continue;
        const DWORD fn = ps == 1 ? 0x4aca70 : ps == 2 ? 0x4acaf0 : 0x4acb80;
        const bool none = rnd(10) == 0;
        for (int n = 0; n < 6; n++) {
            const DWORD x = n == 0 ? 0x3f : (DWORD)rnd_range(0, w + 3), y = rnd((DWORD)h), width = rnd(5) ? (DWORD)w : rnd((DWORD)w + 5);
            run_sides(t, g_hooks, "RleFrame_PixelAt", [&](int side) -> DWORD {
                FrameData *f = frame(side, fi);
                return at<DWORD(__fastcall *)(const BYTE *, const DWORD *, DWORD, DWORD, DWORD)>(fn)(none ? nullptr : f->pixels, f->rows, width, x, y);
            });
        }
    }
    // the bounding box: sparse plain frames and small compressed ones
    for (int k = 0; k < scaled(200); k++) {
        begin_case(random());
        FrameSpec s;
        DWORD flags = 0;
        int fi;
        if (k % 3 == 2) {
            fi = make_rle_frame(pick(PICK_4), rnd_range(1, 40), rnd_range(1, 12), 2, 1 | rnd(32), random());
        } else {
            s = pick_spec(40, 24);
            fi = make_frame(s);
            if (fi >= 0)
                sparse(fi, rnd(2) ? 60 : 1000000);
        }
        if (fi < 0)
            continue;
        (void)flags;
        const DWORD out = alloc_object(sizeof(Rect));
        run_sides(t, g_hooks, "BoundingBox", [&](int side) -> DWORD {
            exe<void>(0x4ab7b0, frame(side, fi), object<Rect>(side, out));
            return 0;
        });
    }
    level.release();
}

// ---- Fills, lines, blits --------------------------------------------------------------------------------------

Rect pick_rect(int x, int y, int w, int h)
{
    Rect r;
    switch (rnd(5)) {
    case 0: r = {x, y, x + w, y + h}; break;
    case 1:
        r.left = rnd_range(x - 4, x + w + 4);
        r.top = rnd_range(y - 4, y + h + 4);
        r.right = r.left + (rnd(2) ? 0 : rnd_range(0, 6));
        r.bottom = r.top + (r.right == r.left ? rnd_range(0, 6) : 0);
        break;
    case 2:
        r.left = x + w + rnd_range(0, 40);
        r.top = rnd_range(y - 50, y + h);
        r.right = r.left + rnd_range(1, 20);
        r.bottom = r.top + rnd_range(1, 20);
        break;
    default:
        r.left = rnd_range(x - 10, x + w + 2);
        r.top = rnd_range(y - 10, y + h + 2);
        r.right = r.left + rnd_range(0, w + 12);
        r.bottom = r.top + rnd_range(0, h + 12);
        break;
    }
    return r;
}

void check_fills(Tally &t)
{
    g_seed = 0x4a91d0;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    SavedMemory cache;
    cache.add(0x6be388, 4);
    cache.add(0x6be3bc, 0x400);
    for (int k = 0; k < scaled(600); k++) {
        begin_case(random());
        FrameSpec s = pick_spec();
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        DWORD flags = format_flag(s.format);
        if (rnd(10) == 0)
            flags = pick(PICK_5);
        const bool no_pixels = rnd(30) == 0;
        for (int side = 0; side < 2; side++) {
            frame(side, fi)->flags = flags;
            if (no_pixels)
                frame(side, fi)->pixels = nullptr;
        }
        const DWORD rect = alloc_object(sizeof(Rect)), color = alloc_object(sizeof(FieldColor)), table = alloc_object(256),
                    tptr = alloc_object(4);
        const Rect r = pick_rect(s.x, s.y, s.width, s.height);
        const FieldColor c = pick_color(s.format);
        for (int side = 0; side < 2; side++) {
            *object<Rect>(side, rect) = r;
            *object<FieldColor>(side, color) = c;
            BYTE *tb = object<BYTE>(side, table);
            unsigned ts = (unsigned)k + 77;
            for (int i = 0; i < 256; i++)
                tb[i] = (BYTE)(pattern_next(ts) >> 5);
            *object<BYTE *>(side, tptr) = tb;
        }
        char what[96];
        wsprintfA(what, "fill %s %dx%d pitch %u flags %x origin %d,%d", format_name(s.format), s.width, s.height, g_h.frames[fi].pitch, flags, s.x,
                  s.y);
        switch (k % 3) {
        case 0:
            run_sides(t, g_hooks, what, [&](int side) -> DWORD {
                exe<void>(0x4a91d0, frame(side, fi), object<Rect>(side, rect));
                return 0;
            });
            break;
        case 1:
            if (no_pixels)
                break;
            run_sides(t, g_hooks, what, [&](int side) -> DWORD { return exe<DWORD>(0x4ac4d0, frame(side, fi), object<FieldColor>(side, color)); });
            break;
        default:
            run_sides(t, g_hooks, what, [&](int side) -> DWORD {
                exe<void>(0x4ac710, frame(side, fi), object<Rect>(side, rect), object<BYTE *>(side, tptr));
                const DWORD h = cache.hash();
                cache.restore();
                return h;
            });
            break;
        }
    }
    cache.release();
    sw.restore();
}

void check_lines(Tally &t)
{
    g_seed = 0x4abae0;
    for (int k = 0; k < scaled(1000); k++) {
        begin_case(random());
        FrameSpec s = pick_spec();
        if (rnd(3))
            s.x = s.y = 0;
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        if (rnd(12) == 0)
            for (int side = 0; side < 2; side++)
                frame(side, fi)->pixels = nullptr;
        const DWORD color = alloc_object(sizeof(FieldColor));
        const FieldColor c = pick_color(s.format);
        for (int side = 0; side < 2; side++)
            *object<FieldColor>(side, color) = c;
        int p[4];
        for (int i = 0; i < 4; i++)
            p[i] = (i & 1) ? rnd_range(s.y - 30, s.y + s.height + 30) : rnd_range(s.x - 30, s.x + s.width + 30);
        if (rnd(5) == 0)
            p[2] = p[0];
        if (rnd(5) == 0)
            p[3] = p[1];
        char what[96];
        wsprintfA(what, "DrawLine %s %dx%d (%d,%d)-(%d,%d) origin %d,%d", format_name(s.format), s.width, s.height, p[0], p[1], p[2], p[3], s.x, s.y);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            exe<void>(0x4abae0, frame(side, fi), p[0], p[1], p[2], p[3], object<FieldColor>(side, color));
            return 0;
        });
        // the 24-bit Bresenham directly (end points inside)
        if (s.format == RGB24 && s.width && s.height && frame(0, fi)->pixels) {
            const int x1 = rnd(s.width), y1 = rnd(s.height), x2 = rnd(s.width), y2 = rnd(s.height);
            const DWORD c3 = random() & 0xffffff;
            run_sides(t, g_hooks, "Bresenham24", [&](int side) -> DWORD {
                at<void(__fastcall *)(FrameData *, int, int, int, int, DWORD)>(0x4acc10)(frame(side, fi), x1, y1, x2, y2, c3);
                return 0;
            });
        }
    }
    // DrawThickLine (slot 52) with end points inside 8/16-bit frames
    for (int k = 0; k < scaled(120); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = rnd(2) ? PAL8 : RGB565;
        s.width = rnd_range(8, 60);
        s.height = rnd_range(8, 40);
        const int fi = make_frame(s);
        const DWORD color = alloc_object(sizeof(FieldColor));
        const FieldColor c = pick_color(s.format);
        for (int side = 0; side < 2; side++)
            *object<FieldColor>(side, color) = c;
        const float x1 = (float)rnd_range(1, s.width - 2), y1 = (float)rnd_range(1, s.height - 2), x2 = (float)rnd_range(1, s.width - 2),
                    y2 = (float)rnd_range(1, s.height - 2);
        run_sides(t, g_hooks, "DrawThickLine", [&](int side) -> DWORD {
            exe<DWORD>(0x4abe60, frame(side, fi), float_bits(x1), float_bits(y1), float_bits(x2), float_bits(y2), object<FieldColor>(side, color));
            return 0;
        });
    }
}

void check_blits(Tally &t)
{
    g_seed = 0x4a8f40;
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    for (int k = 0; k < scaled(500); k++) {
        begin_case(random());
        // source: plain 8/16 or a real compressed frame
        int src;
        int sw_ = 0, sh = 0, sx = 0, sy = 0;
        if (k % 3 == 2 && g_sources.n) {
            const Source &so = g_sources.list[rnd((DWORD)g_sources.n)];
            src = compressed_sane(so.frame) ? copy_compressed(so.frame) : -1;
            if (src >= 0) {
                sw_ = so.frame->width;
                sh = so.frame->height;
                sx = so.frame->x;
                sy = so.frame->y;
            }
        } else {
            FrameSpec s;
            s.format = rnd(2) ? PAL8 : RGB565;
            s.width = rnd_range(1, 48);
            s.height = rnd_range(1, 32);
            s.x = rnd(2) ? 0 : rnd_range(-8, 8);
            s.y = rnd(2) ? 0 : rnd_range(-8, 8);
            src = make_frame(s);
            sw_ = s.width;
            sh = s.height;
            sx = s.x;
            sy = s.y;
        }
        if (src < 0)
            continue;
        FrameSpec d;
        d.format = frame(0, src)->flags & 0x20 ? (rnd(4) ? PAL8 : RGB565) : RGB565;
        if (rnd(8) == 0 && !(frame(0, src)->flags & 0x20))   // 8 -> 24/32 bits reads the source frame's own palette (slot 58)
            d.format = rnd(2) ? RGB24 : RGB32;
        d.width = rnd_range(1, 80);
        d.height = rnd_range(1, 40);
        d.x = rnd(2) ? 0 : rnd_range(-10, 10);
        d.y = rnd(2) ? 0 : rnd_range(-10, 10);
        const int dst = make_frame(d);
        if (dst < 0)
            continue;
        const DWORD mode = copy_mode_object(rnd(3) ? render::COPY_MODE : 0x6bcc68);
        const DWORD srect = alloc_object(sizeof(Rect)), drect = alloc_object(sizeof(Rect));
        Rect sr;
        sr.left = sx + rnd_range(0, sw_ - 1);
        sr.top = sy + rnd_range(0, sh - 1);
        sr.right = rnd(3) ? sx + sw_ : sr.left + rnd_range(1, sx + sw_ - sr.left);
        sr.bottom = rnd(3) ? sy + sh : sr.top + rnd_range(1, sy + sh - sr.top);
        const int px = rnd_range(d.x - 20, d.x + d.width), py = rnd_range(d.y - 20, d.y + d.height);
        const Rect dr = {px, py, px + (sr.right - sr.left), py + (sr.bottom - sr.top)};
        for (int side = 0; side < 2; side++) {
            *object<Rect>(side, srect) = sr;
            *object<Rect>(side, drect) = dr;
        }
        char what[96];
        wsprintfA(what, "Blit src %dx%d flags %x -> %s %dx%d", sw_, sh, frame(0, src)->flags, format_name(d.format), d.width, d.height);
        const bool direct = k & 1;
        if (g_frame_trace)
            log("  case %d: %s mode %x direct %d src (%d,%d,%d,%d) dst (%d,%d,%d,%d)", k, what, *object<DWORD>(0, mode), direct, sr.left, sr.top,
                sr.right, sr.bottom, dr.left, dr.top, dr.right, dr.bottom);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            if (direct) {
                // slot 62 needs rectangles inside both frames: the source's own (minus its origin) and the destination
                Rect *s0 = object<Rect>(side, srect), *d0 = object<Rect>(side, drect);
                *s0 = {0, 0, sw_, sh};
                *d0 = {0, 0, sw_ < d.width ? sw_ : d.width, sh < d.height ? sh : d.height};
                s0->right = s0->left + (d0->right - d0->left);
                s0->bottom = s0->top + (d0->bottom - d0->top);
                exe<void>(0x4a8f10, frame(side, dst), frame(side, src), s0, d0, object<void>(side, mode));
                return 0;
            }
            return lo8(exe<DWORD>(0x4a8f40, frame(side, dst), frame(side, src), object<Rect>(side, srect), object<Rect>(side, drect),
                                  object<void>(side, mode)));
        });
    }
    sw.restore();
}

// ---- The functions that allocate ----------------------------------------------------------------------------------

CFrame *orig_frame(void *mem) { return original<CFrame *(THISCALL *)(void *)>(0x4a8a90)(mem); }
void orig_set_size(CFrame *f, DWORD w, DWORD h, DWORD bits) { original<void(THISCALL *)(CFrame *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, bits); }
void orig_destroy(CFrame *f) { original<void(THISCALL *)(CFrame *)>(0x4a8b00)(f); }

// An object's bytes from +8 on; in a frame the word +0x16 (never written by the constructor: heap rest) as 0
void add_object(Snap &s, const BYTE *o, DWORD size)
{
    BYTE copy[0x48];
    memcpy(copy, o, size);
    if (size >= 0x40)
        copy[0x16] = copy[0x17] = 0;
    s.add_bytes(copy + 8, size - 8);
}

void check_alloc(Tally &t)
{
    g_seed = 0x4aa5f0;
    alignas(16) BYTE mem[2][0x48];
    for (int k = 0; k < scaled(300); k++) {
        const DWORD w = rnd(5) ? rnd(60) : rnd(3), h = rnd(5) ? rnd(30) : rnd(3);
        const DWORD bits = pick(PICK_6);
        const DWORD w2 = rnd(3) ? w : rnd(60), h2 = rnd(3) ? h : rnd(30), bits2 = rnd(3) ? bits : pick(PICK_7);
        const DWORD pitch = rnd(3) ? w2 : w2 + rnd(9);
        const DWORD rebuild = rnd(5) ? 1 : rnd(3);
        const DWORD op = (DWORD)k % 7;
        char what[64];
        wsprintfA(what, "alloc op %u %ux%u/%u -> %ux%u/%u", op, w, h, bits, w2, h2, bits2);
        compare_sides(t, g_hooks, what, [&](int side) -> Snap {
            Snap s;
            CFrame *f = reinterpret_cast<CFrame *>(mem[side]);
            memset(mem[side], 0, sizeof mem[side]);
            if (op == 0) {
                s.add(exe<CFrame *>(0x4a8a90, f) == f);
            } else {
                orig_frame(f);
                if (w && h)
                    orig_set_size(f, w, h, bits);
                switch (op) {
                case 1: s.add(exe<DWORD>(0x4aa5f0, f, w2, h2, bits2)); break;
                case 2: exe<void>(0x4aa2c0, f, pitch, rebuild); break;
                case 3: s.add(exe<DWORD>(0x4aa790, f)); break;
                case 4: exe<void>(0x4ab5a0, f, w2, h2, bits2); break;
                case 5: exe<void>(0x4aa550, f, w2, h2); break;
                default:
                    exe<void>(0x4a8b00, f);
                    snap_frame(s, f);
                    return s;
                }
            }
            snap_frame(s, f);
            orig_destroy(f);
            return s;
        });
    }
    // Create, CreateArray, the deleting destructors (CFrame, CDIBFrame, CPaletteHolder)
    struct Class {
        DWORD create, create_array, dtor, size;
    };
    const Class CLASSES[3] = {{0x4a87b0, 0x4a87d0, 0x4a8820, 0x40}, {0x4a7800, 0x4a7840, 0x4a78a0, 0x48}, {0x4a84f0, 0x4a8520, 0x4a8580, 0x14}};
    for (int k = 0; k < scaled(90); k++) {
        const Class &c = CLASSES[k % 3];
        const int count = rnd(4) ? rnd_range(1, 4) : 0;
        const DWORD w = rnd(40), h = rnd(20);
        compare_sides(t, g_hooks, "Create/CreateArray/DeletingDtor", [&](int side) -> Snap {
            Snap s;
            (void)side;
            BYTE *one = at<BYTE *(__cdecl *)()>(c.create)();
            s.add(one != nullptr);
            if (one) {
                s.add_bytes(one, 8);
                add_object(s, one, c.size);
                if (c.size == 0x40 && w && h)   // a CFrame (the DIB frame does not free its pixels)
                    orig_set_size(reinterpret_cast<CFrame *>(one), w, h, 16);
                s.add(at<BYTE *(THISCALL *)(BYTE *, DWORD)>(c.dtor)(one, 1) == one);
            }
            BYTE *arr = at<BYTE *(__fastcall *)(int, int, int)>(c.create_array)(count, 0, 0);
            s.add(arr != nullptr);
            if (arr) {
                s.add(reinterpret_cast<DWORD *>(arr)[-1]);
                for (int i = 0; i < count; i++)
                    s.add_bytes(arr + i * c.size, 8), add_object(s, arr + i * c.size, c.size);
                s.add(at<BYTE *(THISCALL *)(BYTE *, DWORD)>(c.dtor)(arr, 3) == arr - 4);
            }
            return s;
        });
    }
    // the class getters (registered already: the same class info) and the file extension
    for (DWORD g : CLASS_FUNCS)
        compare_sides(t, g_hooks, "class getter", [&](int) -> Snap {
            Snap s;
            s.add(ptrv(at<void *(__cdecl *)()>(g)()));
            return s;
        });
    for (DWORD g : CLASS_SLOTS)
        compare_sides(t, g_hooks, "class slot", [&](int) -> Snap {
            Snap s;
            s.add(ptrv(at<void *(THISCALL *)(void *)>(g)(nullptr)));
            return s;
        });
    compare_sides(t, g_hooks, "FileExtension", [&](int) -> Snap {
        Snap s;
        s.add(ptrv(at<const char *(__cdecl *)()>(0x4a8980)()));
        return s;
    });
}

// The palette slots on fake frames (the holder dropped again) and CopyRect/CopyFrom into a heap frame
void check_palette_copy(Tally &t)
{
    g_seed = 0x4aac20;
    SavedMemory copy_cache;
    copy_cache.add(render::COPY_MODE + 0x2c, 8);
    BYTE pal_data[2][0x300];
    alignas(16) BYTE mem[2][0x40];
    for (int k = 0; k < scaled(240); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = rnd(2) ? PAL8 : RGB565;
        s.width = rnd_range(1, 40);
        s.height = rnd_range(1, 24);
        s.x = rnd(2) ? 0 : rnd_range(-6, 6);
        s.y = rnd(2) ? 0 : rnd_range(-6, 6);
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        const DWORD op = (DWORD)k % 5;
        const Rect r = {s.x + rnd_range(0, s.width - 1), s.y + rnd_range(0, s.height - 1), s.x + s.width, s.y + s.height};
        const bool give_palette = rnd(2);
        for (int side = 0; side < 2; side++)
            for (int i = 0; i < 0x300; i++)
                pal_data[side][i] = (BYTE)(i * 7 + k);
        compare_sides(t, g_hooks, "palette / copy", [&](int side) -> Snap {
            Snap sn;
            FrameData *f = frame(side, fi);
            Palette src_pal = {pal_data[side], 1, 0};
            if (op == 0) {
                Palette *p = exe<Palette *>(0x4aac20, f);
                sn.add(f->object_04 != 0);
                Palette *again = exe<Palette *>(0x4aac20, f);
                sn.add(again == p);
                sn.add(p ? p->data != nullptr : 2);
            } else if (op == 1) {
                exe<void>(0x4aa390, f, &src_pal);
                Palette *p = original<Palette *(THISCALL *)(void *)>(0x4aac20)(f);
                sn.add(f->flags);
                sn.add_bytes(p->data, 0x300);
                sn.add(p->d4);
            } else if (op == 2) {
                exe<void>(0x4aa770, f);
                Palette *p = original<Palette *(THISCALL *)(void *)>(0x4aac20)(f);
                sn.add(f->flags);
                sn.add(p->d4);
                sn.add(p->data != nullptr);
            } else {
                CFrame *dst = reinterpret_cast<CFrame *>(mem[side]);
                memset(mem[side], 0, sizeof mem[side]);
                orig_frame(dst);
                if (give_palette)
                    original<void(THISCALL *)(void *, const Palette *)>(0x4aa390)(f, &src_pal);
                if (op == 3)
                    exe<void>(0x4ab620, dst, f, &r);
                else
                    exe<void>(0x4ab5d0, dst, f);
                snap_frame(sn, dst);
                if (dst->object_04) {
                    Palette *p = original<Palette *(THISCALL *)(void *)>(0x4aac20)(dst);
                    sn.add_bytes(p->data, 0x300);
                }
                orig_destroy(dst);
                copy_cache.restore();
            }
            drop_dynamics(f);
            return sn;
        });
    }
    copy_cache.release();
}

}  // namespace

ZAX_CHECKS(frame_class)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    collect_sources(g_sources, 10);
    int sane = 0;
    for (int i = 0; i < g_sources.n; i++)
        sane += (g_sources.list[i].frame->flags & F_COMPRESSED) && compressed_sane(g_sources.list[i].frame);
    log("  frame sources: %d picked, %d compressed and sound (sprite %d, font %d)", g_sources.n, sane, g_sources.found[SRC_SPRITE], g_sources.found[SRC_FONT]);
    Tally getters{"frame class: getters, depth, size, origin"};
    check_getters(getters);
    getters.report();
    Tally pixels{"frame class: pixels (plain, synthetic RLE, real frames), RLE tests, bounding box"};
    check_pixels(pixels);
    pixels.report();
    Tally fills{"frame class: ClearRect, FillColor, TintRect"};
    check_fills(fills);
    fills.report();
    Tally lines{"frame class: DrawLine, Bresenham, DrawThickLine"};
    check_lines(lines);
    lines.report();
    Tally blits{"frame class: Blit, BlitWithMode"};
    check_blits(blits);
    blits.report();
    Tally alloc{"frame class: construction and memory (heap contents)"};
    check_alloc(alloc);
    alloc.report();
    Tally pal{"frame class: palette slots, CopyRect, CopyFrom"};
    check_palette_copy(pal);
    pal.report();
}

}  // namespace zax
