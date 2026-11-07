// Checks of frame_basics.cpp with the frame check harness (checks_frame_common.h), in the game thread.
//  - Pixel16_FromRGB: with the tables built and with the test entry 0x630578 cleared (the rounding path):
//    every value of each channel with the others random, the edge values of all three channels, random
//    triples; arguments beyond 255 (up to 300 with the tables, which the original indexes unclipped, any
//    dword without them).
//  - Frame_FillRect: random fake frames of every format (8 bits with a palette index or through
//    FieldColor_PaletteIndex, 5-6-5 and 5-5-5 with alpha 0xff, 0 and in between, 24 bits, 32 bits and
//    depths the code does not draw), sizes 0-80 x 0-40 with pitch padding, origins, CFrame and CDIBFrame,
//    no pixels; rectangles inside, across and outside the frame, empty ones; some through a
//    COffsetClippedProxyFrame with an offset and a clip rectangle; some frames start with the pixels of a
//    real frame of the level. Every case twice: Cpu_HasMmx on and off.
//  - CFrame::Clear and CFrame::Fill on frames of every format and other depths, with padding; ClipRect on
//    random sizes and origins.
//  - Real sources: the level's frames found read only (counts logged), the copies compared with the
//    originals' pixels.
// [Reimpl] FrameMutation= sets the reimplementation's deliberate bugs (frame_basics.h) for the run.
#include "checks_frame_common.h"
#include "frame_basics.h"

namespace zax {

int hooks_frame_basics(Hook *out);

namespace {

using namespace framec;

ModuleHooks g_hooks{"frame_basics", &hooks_frame_basics, {}, {}, {}, 0, true};
Sources g_sources;

DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }
int rnd_range(int lo, int hi) { return lo + (int)rnd((DWORD)(hi - lo + 1)); }

template <typename R, typename... A> R call_slot(void *obj, int i, A... a)
{
    return reinterpret_cast<R(THISCALL *)(void *, A...)>((*static_cast<DWORD **>(obj))[i])(obj, a...);
}

// ---- Pixel16_FromRGB ------------------------------------------------------------------------------------------

void check_pixel16(Tally &t)
{
    using F = DWORD(__fastcall *)(DWORD, DWORD, DWORD);
    Pixel16Tables tables;
    auto one = [&](DWORD r, DWORD g, DWORD b) {
        const DWORD o = original<F>(0x433a10)(r, g, b), x = at<F>(0x433a10)(r, g, b);
        if (o == x) {
            t.check(true, "");
            return;
        }
        char d[120];
        wsprintfA(d, "[Pixel16_FromRGB(%u, %u, %u) tables %d: %x / %x]", r, g, b, *at<DWORD *>(PIXEL16_TABLES_BUILT) != 0, o, x);
        t.check(false, "", d);
    };
    static const DWORD EDGES[] = {0, 1, 2, 3, 4, 5, 7, 8, 127, 128, 251, 252, 253, 255};
    for (int built = 1; built >= 0; built--) {
        tables.set(built != 0);
        g_seed = 0x433a10u + (unsigned)built;
        for (DWORD v = 0; v < 256; v++) {
            one(v, rnd(256), rnd(256));
            one(rnd(256), v, rnd(256));
            one(rnd(256), rnd(256), v);
        }
        for (DWORD r : EDGES)
            for (DWORD g : EDGES)
                for (DWORD b : EDGES)
                    one(r, g, b);
        for (int k = 0; k < scaled(3000); k++)
            one(rnd(256), rnd(256), rnd(256));
        for (int k = 0; k < scaled(300); k++)
            if (built)
                one(256 + rnd(45), 256 + rnd(45), 256 + rnd(45));
            else
                one(random(), random(), k & 1 ? random() : rnd(1024));
    }
    tables.restore();
}

// ---- Frame_FillRect ------------------------------------------------------------------------------------------

struct FillCase {
    FrameSpec spec;
    DWORD flags;              // the frame's flags (normally the format's)
    bool no_pixels, proxy;
    int dx, dy;
    Rect clip, rect;
    FieldColor color;
    const FrameData *source;
};

Format pick_format()
{
    const DWORD k = rnd(20);
    return k < 5 ? PAL8 : k < 11 ? RGB565 : k < 15 ? RGB555 : k < 18 ? RGB24 : RGB32;
}

Rect pick_rect(int x, int y, int w, int h)
{
    Rect r;
    switch (rnd(6)) {
    case 0:   // the whole frame
        r = {x, y, x + w, y + h};
        break;
    case 1:   // empty
        r.left = rnd_range(x - 4, x + w + 4);
        r.top = rnd_range(y - 4, y + h + 4);
        r.right = rnd(2) ? r.left : r.left + rnd_range(0, 8);
        r.bottom = r.right == r.left ? r.top + rnd_range(0, 8) : r.top;
        break;
    case 2:   // far outside
        r.left = x + w + rnd_range(0, 50);
        r.top = rnd_range(y - 60, y + h);
        r.right = r.left + rnd_range(1, 30);
        r.bottom = r.top + rnd_range(1, 30);
        if (rnd(2)) {
            r.left -= w + 90;
            r.right -= w + 90;
        }
        break;
    default:   // inside or across an edge
        r.left = rnd_range(x - 10, x + w + 2);
        r.top = rnd_range(y - 10, y + h + 2);
        r.right = r.left + rnd_range(0, w + 12);
        r.bottom = r.top + rnd_range(0, h + 12);
        break;
    }
    return r;
}

FieldColor pick_color(Format f)
{
    FieldColor c;
    BYTE *b = reinterpret_cast<BYTE *>(&c);
    for (DWORD i = 0; i < sizeof c; i++)
        b[i] = (BYTE)rnd(256);
    const DWORD k = rnd(10);
    c.a = k < 4 ? 0xff : k == 4 ? 0 : k == 5 ? 0xfe : (BYTE)rnd(255);
    c.flags = (c.flags & ~3u) | (rnd(5) ? 1u : 2u);
    if (f == RGB555)
        c.pixel16 &= 0x7fff;
    return c;
}

FillCase make_fill_case(int k)
{
    FillCase c = {};
    FrameSpec &s = c.spec;
    s.format = pick_format();
    s.width = rnd(8) ? rnd_range(1, 80) : rnd_range(0, 3);
    s.height = rnd(8) ? rnd_range(1, 40) : rnd_range(0, 2);
    s.pitch = rnd(3) ? s.width : s.width + rnd_range(1, 13);
    s.x = rnd(2) ? 0 : rnd_range(-20, 20);
    s.y = rnd(2) ? 0 : rnd_range(-20, 20);
    s.dib = rnd(6) == 0;
    c.flags = format_flag(s.format);
    if (s.format == RGB32 && rnd(3) == 0) {
        static const DWORD ODD[] = {0, 0x60, 0x1e0, 0xa0};
        c.flags = ODD[rnd(4)];
    }
    if (rnd(4) == 0)
        c.flags |= 0x4000;
    c.no_pixels = rnd(40) == 0;
    c.proxy = rnd(5) == 0;
    if (c.proxy) {
        c.dx = rnd_range(-15, 15);
        c.dy = rnd_range(-15, 15);
        c.clip = {s.x + rnd_range(-5, s.width), s.y + rnd_range(-5, s.height), 0, 0};
        c.clip.right = c.clip.left + rnd_range(0, s.width + 5);
        c.clip.bottom = c.clip.top + rnd_range(0, s.height + 5);
    }
    c.rect = pick_rect(s.x, s.y, s.width, s.height);
    c.color = pick_color(s.format);
    // every third case of 8 or 16 bits starts with the pixels of a real frame of the level
    if (g_sources.n && k % 3 == 0 && (s.format == PAL8 || s.format == RGB565)) {
        const Source &src = g_sources.list[(DWORD)k / 3 % (DWORD)g_sources.n];
        const DWORD depth = src.frame->flags & F_DEPTH;
        if (!(src.frame->flags & F_COMPRESSED)) {
            s.format = depth == F_8 ? PAL8 : RGB565;
            c.flags = format_flag(s.format);
            s.width = src.frame->width < 96 ? src.frame->width : 96;
            s.height = src.frame->height < 64 ? src.frame->height : 64;
            s.pitch = s.width + (k & 4 ? 3 : 0);
            c.rect = pick_rect(s.x, s.y, s.width, s.height);
            c.color = pick_color(s.format);
            c.source = src.frame;
        }
    }
    return c;
}

void run_fill_case(Tally &t, const FillCase &c, unsigned seed, bool mmx, MmxSwitch &sw)
{
    begin_case(seed);
    const int fi = make_frame(c.spec);
    if (fi < 0) {
        t.check(false, "", "[the case does not fit the arena]");
        return;
    }
    if (c.source)
        copy_source(fi, c.source);
    DWORD proxy = 0xffffffffu;
    if (c.proxy)
        proxy = make_offset_clipped(fi, c.dx, c.dy, c.clip);
    const DWORD rect = alloc_object(sizeof(Rect)), color = alloc_object(sizeof(FieldColor));
    for (int side = 0; side < 2; side++) {
        FrameData *f = frame(side, fi);
        f->flags = c.flags;
        if (c.no_pixels)
            f->pixels = nullptr;
        *object<Rect>(side, rect) = c.rect;
        *object<FieldColor>(side, color) = c.color;
    }
    sw.set(mmx);
    char what[96];
    wsprintfA(what, "FillRect %s %dx%d pitch %u flags %x%s%s mmx %d alpha %u", format_name(c.spec.format), c.spec.width,
              c.spec.height, g_h.frames[fi].pitch, c.flags, c.proxy ? " proxy" : "", c.source ? " source" : "", mmx, c.color.a);
    run_sides(t, g_hooks, what, [&](int side) -> DWORD {
        Rect *r = object<Rect>(side, rect);
        FieldColor *col = object<FieldColor>(side, color);
        if (c.proxy)
            return call_slot<DWORD>(object<void>(side, proxy), 28, r, col);
        return at<DWORD(THISCALL *)(FrameData *, Rect *, FieldColor *)>(0x4abe90)(frame(side, fi), r, col);
    });
}

void check_fill_rect(Tally &t)
{
    MmxSwitch sw;
    g_seed = 0x4abe90;
    for (int k = 0; k < scaled(3000); k++) {
        const FillCase c = make_fill_case(k);
        const unsigned seed = random();
        run_fill_case(t, c, seed, true, sw);
        run_fill_case(t, c, seed, false, sw);
    }
    sw.restore();
}

// ---- Clear, Fill, ClipRect -------------------------------------------------------------------------------------

void check_clear_fill(Tally &t)
{
    MmxSwitch sw;
    sw.set(cpu_has_mmx());
    g_seed = 0x4a9010;
    for (int k = 0; k < scaled(1200); k++) {
        FrameSpec s;
        s.format = pick_format();
        s.width = rnd(10) ? rnd_range(1, 70) : 0;
        s.height = rnd(10) ? rnd_range(1, 30) : rnd_range(0, 1);
        s.pitch = rnd(2) ? s.width : s.width + rnd_range(1, 9);
        s.dib = rnd(5) == 0;
        DWORD flags = format_flag(s.format);
        if (s.format == RGB32 && rnd(3) == 0) {
            static const DWORD ODD[] = {0, 0x60, 0x1e0, 0xa0, 0xc0};
            flags = ODD[rnd(5)];
        }
        const bool no_pixels = rnd(30) == 0;
        const bool fill = k & 1;
        const DWORD value = rnd(3) ? rnd(256) : random();
        begin_case(random());
        const int fi = make_frame(s);
        if (fi < 0) {
            t.check(false, "", "[the case does not fit the arena]");
            continue;
        }
        for (int side = 0; side < 2; side++) {
            frame(side, fi)->flags = flags;
            if (no_pixels && (!fill || !s.width || !s.height))
                frame(side, fi)->pixels = nullptr;
        }
        char what[96];
        wsprintfA(what, "%s %s %dx%d pitch %u flags %x", fill ? "Fill" : "Clear", format_name(s.format), s.width, s.height,
                  g_h.frames[fi].pitch, flags);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            FrameData *f = frame(side, fi);
            if (fill)
                return at<DWORD(THISCALL *)(FrameData *, DWORD)>(0x4ac490)(f, value);
            return at<DWORD(THISCALL *)(FrameData *)>(0x4a9010)(f);
        });
    }
    sw.restore();
}

void check_clip_rect(Tally &t)
{
    g_seed = 0x4aa520;
    for (int k = 0; k < scaled(600); k++) {
        FrameSpec s;
        s.format = PAL8;
        s.width = rnd_range(0, 8);
        s.height = rnd_range(0, 8);
        begin_case(random());
        const int fi = make_frame(s);
        const DWORD out = alloc_object(sizeof(Rect));
        const short x = (short)(rnd(4) ? rnd_range(-300, 300) : (int)random()), y = (short)(rnd(4) ? rnd_range(-300, 300) : (int)random());
        const WORD w = (WORD)(rnd(4) ? rnd(1000) : random()), h = (WORD)(rnd(4) ? rnd(1000) : random());
        for (int side = 0; side < 2; side++) {
            FrameData *f = frame(side, fi);
            f->x = x;
            f->y = y;
            f->width = w;
            f->height = h;
        }
        run_sides(t, g_hooks, "ClipRect", [&](int side) -> DWORD {
            return at<DWORD(THISCALL *)(FrameData *, Rect *)>(0x4aa520)(frame(side, fi), object<Rect>(side, out));
        });
    }
}

// ---- Real sources --------------------------------------------------------------------------------------------

void check_sources(Tally &t)
{
    collect_sources(g_sources, 12);
    log("  frame sources: sprite %d (%d compressed), font %d (%d), image %d (%d), texture %d (%d), world %d (%d); %d picked",
        g_sources.found[SRC_SPRITE], g_sources.compressed[SRC_SPRITE], g_sources.found[SRC_FONT], g_sources.compressed[SRC_FONT],
        g_sources.found[SRC_IMAGE], g_sources.compressed[SRC_IMAGE], g_sources.found[SRC_TEXTURE], g_sources.compressed[SRC_TEXTURE],
        g_sources.found[SRC_WORLD], g_sources.compressed[SRC_WORLD], g_sources.n);
    BYTE palette[768];
    t.check(copy_game_palette(palette), "", "[the game palette 0x633f98 is not loaded]");
    // every source copied into fake frames: a compressed one whole (its blocks equal the source's), a plain
    // one into a fake frame of its size (capped)
    bool shown[SRC_KINDS] = {};
    for (int i = 0; i < g_sources.n; i++) {
        const FrameData *src = g_sources.list[i].frame;
        const int kind = g_sources.list[i].kind;
        if (src->flags & F_COMPRESSED) {
            begin_case((unsigned)i);
            const int fi = copy_compressed(src);
            DWORD sizes[5] = {};
            bool same = fi >= 0;
            for (int b = 0; b < 5 && same; b++) {
                const BYTE *d = block_data(src, b);
                if (!d)
                    continue;
                sizes[b] = *reinterpret_cast<const DWORD *>(d);
                for (int side = 0; side < 2 && same; side++) {
                    const FrameData *f = frame(side, fi);
                    same = !memcmp(block_data(f, b), d, sizes[b]) &&
                           (!block_table(src, b) || !memcmp(block_table(f, b), block_table(src, b), src->height * 4u));
                }
            }
            if (!shown[kind]) {
                shown[kind] = true;
                log("  first compressed %s: %ux%u flags %x, blocks %u %u %u %u %u, tables %d %d %d %d %d", source_kind_name(kind),
                    src->width, src->height, src->flags, sizes[0], sizes[1], sizes[2], sizes[3], sizes[4], src->rows != nullptr,
                    src->blocks[1] != nullptr, src->blocks[3] != nullptr, src->blocks[5] != nullptr, src->blocks[7] != nullptr);
            }
            char d[96];
            wsprintfA(d, "[compressed %s source %d: %dx%d flags %x]", source_kind_name(kind), i, src->width, src->height, src->flags);
            t.check(same, "", d);
            continue;
        }
        FrameSpec s;
        s.format = (src->flags & F_DEPTH) == F_8 ? PAL8 : RGB565;
        s.width = src->width < 256 ? src->width : 256;
        s.height = src->height < 128 ? src->height : 128;
        s.pitch = s.width + 5;
        begin_case((unsigned)i);
        const int fi = make_frame(s);
        if (fi < 0) {
            t.check(false, "", "[a source does not fit the arena]");
            continue;
        }
        copy_source(fi, src);
        const DWORD bpp = format_bytes(s.format);
        bool same = true;
        for (int side = 0; side < 2 && same; side++)
            for (int y = 0; y < s.height && same; y++)
                same = !memcmp(g_h.arena[side].pixels + g_h.frames[fi].buffer + y * g_h.frames[fi].pitch * bpp,
                               src->pixels + src->rows[y] * bpp, s.width * bpp);
        char d[96];
        wsprintfA(d, "[%s source %d: %dx%d]", source_kind_name(g_sources.list[i].kind), i, src->width, src->height);
        t.check(same, "", d);
    }
}

int read_mutation()
{
    char ini[MAX_PATH + 32];
    GetModuleFileNameA(NULL, ini, MAX_PATH);
    for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
        ini[n - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    return (int)GetPrivateProfileIntA("Reimpl", "FrameMutation", 0, ini);
}

}  // namespace

ZAX_CHECKS(frame_basics)
{
    (void)cls;
    (void)n;
    g_frame_mutation = read_mutation();
    if (g_frame_mutation)
        log("  FrameMutation=%d: the reimplementation's deliberate bugs are on", g_frame_mutation);
    log("  CPU MMX flag: %d", cpu_has_mmx());
    Tally sources{"frame sources (real frames of the level, read only)"};
    check_sources(sources);
    sources.report();
    Tally pixel{"frame basics: Pixel16_FromRGB, tables built and not"};
    check_pixel16(pixel);
    pixel.report();
    Tally fill{"frame basics: Frame_FillRect, all formats, MMX on and off"};
    check_fill_rect(fill);
    fill.report();
    Tally clear{"frame basics: CFrame Clear and Fill"};
    check_clear_fill(clear);
    clear.report();
    Tally clip{"frame basics: CFrame ClipRect"};
    check_clip_rect(clip);
    clip.report();
    g_frame_mutation = 0;
}

}  // namespace zax
