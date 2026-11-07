// Checks of batch R5 (rle16.cpp, rle8.cpp, blitters_raw.cpp, blitters_screen.cpp, blitters_filter.cpp) on the
// frame check harness (checks_frame_common.h), in the game thread: every blitter called by its exe address
// (both MMX and plain variants, whatever the registry holds) and some through CFrame::vf62 with copies of the
// live modes, side 0 with the module's hooks out (so its rows and drawers are the originals too), on fake
// frames: real sprites of the level (copied whole) and synthetic RLE frames (exact rows, any of the five
// blocks, zero-length codes where the spans allow them), plain sources and destinations in 8 bits, 5-6-5 and
// 5-5-5 with pitch padding and odd widths, whole and cut rectangles (the drawers' full and clipped paths),
// opacity 1, 0, 0.5, the 20/40/60/80 % levels, random and NaN. Besides the frames and the call log the
// globals the blitters write (g_OpacityTable, g_Blend8Table, the 8-bit drawers' block pairs, the hue table,
// the Filter's red range) are compared by value (arena pointers by offset) and put back after each side.
#include "checks_frame_r1.h"
#include "blitters.h"

namespace zax {

int hooks_rle16(Hook *out);
int hooks_rle8(Hook *out);
int hooks_blitters_raw(Hook *out);
int hooks_blitters_screen(Hook *out);
int hooks_blitters_filter(Hook *out);

namespace {

using namespace framec;
using r1::rnd;
using r1::rnd_range;

ModuleHooks g_rle16{"rle16", &hooks_rle16, {}, {}, {}, 0, true};
ModuleHooks g_rle8{"rle8", &hooks_rle8, {}, {}, {}, 0, true};
ModuleHooks g_raw{"blitters_raw", &hooks_blitters_raw, {}, {}, {}, 0, true};
ModuleHooks g_screen{"blitters_screen", &hooks_blitters_screen, {}, {}, {}, 0, true};
ModuleHooks g_filter{"blitters_filter", &hooks_blitters_filter, {}, {}, {}, 0, true};
Sources g_sources;

using Blit = void(__fastcall *)(void *src, const void *sr, void *dst, const void *dr, void *mode);

DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// ---- Globals the blitters write ------------------------------------------------------------------------------------

constexpr DWORD WATCHED[][2] = {{render::OPACITY_TABLE, 4}, {render::blit::BLEND8_TABLE, 4}, {render::blit::RLE8_BLOCKS, 0x28},
                                {render::blit::HUE_TABLE16, 4}, {0x6b9c98, 4}, {0x6bacd4, 4}, {0x6bb4e0 + 0x2c, 8}};

// The watched globals of a side, arena pointers by their offset
unsigned watched_hash(int side)
{
    unsigned h = 2166136261u;
    for (const auto &w : WATCHED)
        for (DWORD o = 0; o < w[1]; o += 4) {
            const DWORD v = map_value(side, *at<const DWORD *>(w[0] + o));
            h = fnv(h, &v, 4);
        }
    return h;
}

struct Globals {
    r1::SavedMemory saved;
    Globals()
    {
        for (const auto &w : WATCHED)
            saved.add(w[0], w[1]);
    }
};

// ---- Synthetic RLE frames with exact rows ----------------------------------------------------------------------------

// A row of codes covering exactly w pixels: skips, literals and runs of 1-63 (ps bytes per pixel), zero-length
// codes when allowed
BYTE *exact_row(BYTE *p, int w, DWORD ps, unsigned &s, bool zero_codes)
{
    int x = 0;
    while (x < w) {
        const DWORD k = pattern_next(s) % 20;
        const int left = w - x;
        int n = (int)(1 + pattern_next(s) % (k < 4 ? 63 : 14));
        if (n > left)
            n = left;
        if (k < 6) {
            *p++ = (BYTE)(0x80 | n);
        } else if (k < 12 || (k == 19 && !zero_codes)) {
            *p++ = (BYTE)(0x40 | n);
            for (DWORD i = 0; i < (DWORD)n * ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
        } else if (k < 19) {
            *p++ = (BYTE)n;
            for (DWORD i = 0; i < ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
        } else {
            const DWORD kind = pattern_next(s) % 3;
            *p++ = (BYTE)(kind == 0 ? 0x80 : kind == 1 ? 0x40 : 0);
            if (kind == 2)
                for (DWORD i = 0; i < ps; i++)
                    *p++ = (BYTE)(pattern_next(s) >> 9);
            continue;
        }
        x += n;
    }
    return p;
}

// A compressed frame of w x h (flags: depth | 2 or 4) with the blocks of `mask` (bit 0 pixels, 1 +0x1c, 2 +0x24,
// 3 alpha, 4 edge); `ps` bytes per pixel. The alpha block gets raw bytes (one per pixel of its row, plus room)
int make_rle(DWORD flags, int w, int h, DWORD ps, DWORD mask, bool zero_codes, unsigned seed)
{
    int nb = 0;
    for (int b = 0; b < 5; b++)
        nb += (mask >> b) & 1;
    const DWORD worst = (DWORD)nb * ((DWORD)h * (DWORD)(w * 2 + 8) * (1 + ps) + 8) + 64;
    if (g_h.nframes >= (int)MAX_FRAMES || g_h.next_object + sizeof(FrameData) > ARENA_OBJECTS || g_h.next_rows + (DWORD)(nb * h) > ARENA_ROWS ||
        g_h.next_pixels + GUARD * 2 + worst + 16 > ARENA_PIXELS || h < 1 || w < 1)
        return -1;
    Arena &a0 = g_h.arena[0], &a1 = g_h.arena[1];
    const DWORD start = g_h.next_pixels;
    BYTE *base = a0.pixels + start, *p = base + GUARD;
    DWORD *table = a0.rows + g_h.next_rows;
    FrameData *f = reinterpret_cast<FrameData *>(a0.objects + g_h.next_object);
    memset(f, 0, sizeof *f);
    f->vftable = vtable_for(0, VT_CFRAME, true);
    f->width = (WORD)w;
    f->height = (WORD)h;
    f->log2_width = log2_of(f->width);
    f->log2_height = log2_of(f->height);
    f->flags = flags;
    unsigned s = seed * 2654435761u + 29;
    if (!s)
        s = 1;
    for (int b = 0; b < 5; b++) {
        if (!((mask >> b) & 1))
            continue;
        BYTE *data = p;
        p += 4;
        for (int y = 0; y < h; y++) {
            table[y] = (DWORD)(p - data);
            if (b == 3 && ps == 2)
                for (int i = 0; i < w + 8; i++)
                    *p++ = (BYTE)(pattern_next(s) % 5 == 0 ? (pattern_next(s) & 1) * 255 : pattern_next(s) >> 9);
            else
                p = exact_row(p, w, ps, s, zero_codes);
        }
        *reinterpret_cast<DWORD *>(data) = (DWORD)(p - data);
        while ((p - base) & 3)
            *p++ = 0;
        if (b == 0) {
            f->pixels = data;
            f->rows = table;
        } else {
            f->blocks[2 * b - 2] = data;
            f->blocks[2 * b - 1] = table;
        }
        table += h;
    }
    const DWORD bytes = (DWORD)(p - (base + GUARD));
    const DWORD total = (GUARD + bytes + GUARD + 15) & ~15u;
    fill_pattern(base, GUARD, seed * 31u + 1);
    fill_pattern(base + GUARD + bytes, total - GUARD - bytes, seed * 31u + 2);
    g_h.frames[g_h.nframes] = {g_h.next_object, start + GUARD, bytes, bytes, 1, 1};
    memcpy(a1.pixels + start, base, total);
    memcpy(a1.rows + g_h.next_rows, a0.rows + g_h.next_rows, (DWORD)(nb * h) * 4);
    FrameData *f1 = reinterpret_cast<FrameData *>(a1.objects + g_h.next_object);
    *f1 = *f;
    auto rebase = [&](void *q) -> void * {
        if (!q)
            return q;
        const DWORD v = ptrv(q);
        if (v >= ptrv(a0.pixels) && v < ptrv(a0.pixels) + ARENA_PIXELS)
            return a1.pixels + (v - ptrv(a0.pixels));
        return a1.rows + (v - ptrv(a0.rows)) / 4;
    };
    f1->pixels = static_cast<BYTE *>(rebase(f->pixels));
    f1->rows = static_cast<DWORD *>(rebase(f->rows));
    for (void *&q : f1->blocks)
        q = rebase(q);
    g_h.next_object += sizeof(FrameData);
    g_h.next_rows += (DWORD)(nb * h);
    g_h.next_pixels += total;
    return g_h.nframes++;
}

// A real 16-bit compressed sprite of the level, or null
const FrameData *pick_sprite()
{
    int cand[Sources::MAX], m = 0;
    for (int i = 0; i < g_sources.n; i++) {
        const FrameData *f = g_sources.list[i].frame;
        if ((f->flags & F_DEPTH) == F_16 && (f->flags & render::F_RLE16) && f->width <= 150 && f->height <= 120 && r1::compressed_sane(f))
            cand[m++] = i;
    }
    return m ? g_sources.list[cand[rnd((DWORD)m)]].frame : nullptr;
}

// ---- Modes and rectangles -----------------------------------------------------------------------------------------------

constexpr float OPACITIES[] = {1.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.2f, 0.4f, 0.6f, 0.8f, 0.1f};

// A copy of a live mode (0x48 bytes: the hue remap's +0x44), its opacity set on both sides (SetOpacity 0x433be0;
// NaN written directly: SetOpacity would index its table with it)
DWORD mode_copy(DWORD live, int op_kind, DWORD *op_out)
{
    const DWORD off = alloc_object(0x48);
    if (off == 0xffffffffu)
        return off;
    float op = op_kind < 0 ? (float)rnd(256) / 255.0f : OPACITIES[op_kind];
    if (op_kind == -2)
        op = 1.0f + (float)rnd(3) / 64.0f;
    for (int side = 0; side < 2; side++) {
        memcpy(object<void>(side, off), at<void *>(live), sizeof(render::CBlendMode));
        at<void(THISCALL *)(void *, DWORD)>(0x433be0)(object<void>(side, off), fbits(op));
    }
    if (rnd(40) == 0)
        for (int side = 0; side < 2; side++)
            object<render::CBlendMode>(side, off)->opacity = raw_float(0x7fc00000u);
    *op_out = fbits(object<render::CBlendMode>(0, off)->opacity);
    return off;
}
DWORD op_kind() { return rnd(4) ? rnd(sizeof OPACITIES / sizeof OPACITIES[0]) : (DWORD)-1; }

struct Rects {
    DWORD s, d;   // object offsets
    framec::Rect sr, dr;
};
// Rectangles of equal size inside the source (sw x sh) and the destination (dw x dh); `full` takes the source's
// whole width (the RLE drawers' full path), else a random cut
bool make_rects(Rects &r, int sw, int sh, int dw, int dh, bool full)
{
    const int w = sw < dw ? sw : dw, h = sh < dh ? sh : dh;
    if (w < 1 || h < 1)
        return false;
    int cw = full ? sw : rnd_range(1, w), ch = rnd(3) ? rnd_range(1, h) : h;
    if (full && sw > dw)
        return false;
    const int sl = full ? 0 : rnd_range(0, sw - cw), st = rnd_range(0, sh - ch);
    const int dl = rnd_range(0, dw - cw), dt = rnd_range(0, dh - ch);
    r.sr = {sl, st, sl + cw, st + ch};
    r.dr = {dl, dt, dl + cw, dt + ch};
    r.s = alloc_object(16);
    r.d = alloc_object(16);
    if (r.s == 0xffffffffu || r.d == 0xffffffffu)
        return false;
    for (int side = 0; side < 2; side++) {
        *object<framec::Rect>(side, r.s) = r.sr;
        *object<framec::Rect>(side, r.d) = r.dr;
    }
    return true;
}

int make_dst(Format f, int min_w, int min_h)
{
    FrameSpec d;
    d.format = f;
    d.width = rnd_range(min_w, min_w + 40 < 150 ? min_w + 40 : 150);
    d.height = rnd_range(min_h, min_h + 10 < 120 ? min_h + 10 : 120);
    if (rnd(3) == 0)
        d.pitch = d.width + rnd_range(1, 7);
    return make_frame(d);
}

// One blit on both sides: `fn` an exe address, or 0 for CFrame::vf62 (FindBlitter on the mode copy)
void run_blit(Tally &t, ModuleHooks &hooks, Globals &g, const char *what, DWORD fn, int src, const Rects &r, int dst, DWORD mode)
{
    if (r1::g_frame_trace)
        log("  blit case: %s", what);
    run_sides(t, hooks, what, [&](int side) -> DWORD {
        void *m = object<void>(side, mode);
        void *s = frame(side, src), *d = frame(side, dst);
        const void *sr = object<void>(side, r.s), *dr = object<void>(side, r.d);
        if (fn)
            at<Blit>(fn)(s, sr, d, dr, m);
        else
            at<void(THISCALL *)(void *, void *, const void *, const void *, void *)>(0x4a8f10)(d, s, sr, dr, m);
        __asm__ volatile("emms");
        const unsigned h = watched_hash(side);
        g.saved.restore();
        return h;
    });
}

// ---- rle16 -----------------------------------------------------------------------------------------------------------------

struct Rle16Kind {
    DWORD fn, mode;
    const char *name;
    bool no_edge, no_zero, exact, hue;
};
constexpr Rle16Kind RLE16_KINDS[] = {
    {0x438c40, 0x6bb4e0, "Copy plain", false, false, false, false},
    {0x4388f0, 0x6bb4e0, "Copy MMX", false, false, false, false},
    {0x4388f0, 0x6bcc68, "Transparent Mask MMX", false, false, false, false},
    {0x438c40, 0x6b9728, "Black Bias plain", false, false, false, false},
    {0x435950, 0x6b85f0, "Amplify Light plain", true, false, false, false},
    {0x4355f0, 0x6b85f0, "Amplify Light MMX", true, false, false, false},
    {0x446e30, 0x6bc618, "Overlap", false, true, false, false},
    {0x43d2d0, 0x6bb4e0, "hue remap plain", false, true, true, true},
    {0x43ced0, 0x6bb4e0, "hue remap MMX", false, true, true, true},
    {0, 0x6bb4e0, "Copy vf62", false, false, false, false},
    {0, 0x6b85f0, "Amplify Light vf62", true, false, false, false},
    {0, 0x6bc618, "Overlap vf62", false, true, false, false},
};

WORD *g_hue_table;   // 64K words for the hue modes (+0x44), the same on both sides
BYTE *g_hue_table8;  // 256 bytes for the 8-bit hue mode's palette data

void hue_tables()
{
    if (!g_hue_table) {
        g_hue_table = static_cast<WORD *>(heap_alloc(0x20000));
        g_hue_table8 = static_cast<BYTE *>(heap_alloc(0x100));
    }
    for (DWORD i = 0; i < 0x10000; i++)
        g_hue_table[i] = (WORD)(rnd(4) ? random() >> 9 : (i ^ 0x0841));
    for (DWORD i = 0; i < 0x100; i++)
        g_hue_table8[i] = (BYTE)(random() >> 13);
}

void check_rle16(Tally &t, int cases)
{
    Globals g;
    hue_tables();
    int real_used = 0;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const Rle16Kind &kind = RLE16_KINDS[(DWORD)k % (sizeof RLE16_KINDS / sizeof RLE16_KINDS[0])];
        const FrameData *real = rnd(2) ? pick_sprite() : nullptr;
        int src;
        if (real) {
            src = copy_compressed(real);
            real_used++;
        } else {
            const DWORD mask = 1 | (kind.no_edge || rnd(3) == 0 ? 0 : 0x18) | (rnd(3) == 0 ? 0x6 : 0);
            src = make_rle(F_16 | render::F_RLE16, rnd_range(1, 140), rnd_range(1, 36), 2, mask, !kind.no_zero && rnd(2), random());
        }
        if (src < 0)
            continue;
        if (kind.no_edge)
            for (int side = 0; side < 2; side++)
                frame(side, src)->blocks[6] = frame(side, src)->blocks[7] = nullptr;
        const int sw = frame(0, src)->width, sh = frame(0, src)->height;
        const bool full = rnd(2) != 0 && sw <= 150;
        const int dst = make_dst(rnd(3) ? RGB565 : RGB555, full ? sw : sw / 2 > 1 ? sw / 2 : 1, sh / 2 > 1 ? sh / 2 : 1);
        if (dst < 0)
            continue;
        DWORD op;
        const DWORD mode = mode_copy(kind.mode, (int)op_kind(), &op);
        if (mode == 0xffffffffu)
            continue;
        if (kind.hue)
            for (int side = 0; side < 2; side++)
                object<DWORD>(side, mode)[0x44 / 4] = ptrv(g_hue_table);
        Rects r;
        if (!make_rects(r, sw, sh, frame(0, dst)->width, frame(0, dst)->height, full))
            continue;
        char what[150];
        wsprintfA(what, "%s %s %dx%d (%d,%d %dx%d) -> %s %dx%d at %d,%d op %x", kind.name, real ? "real" : "synthetic", sw, sh, r.sr.left,
                  r.sr.top, r.sr.right - r.sr.left, r.sr.bottom - r.sr.top, frame(0, dst)->flags == F_16 ? "16" : "?", frame(0, dst)->width,
                  frame(0, dst)->height, r.dr.left, r.dr.top, op);
        run_blit(t, g_rle16, g, what, kind.fn, src, r, dst, mode);
    }
    char d[60];
    wsprintfA(d, "[no 16-bit sprite among the sources]");
    t.check(real_used > 0, "", d);
}

// The 8-bit hue remap blitter 0x43c6b0 (in rle16.cpp) on synthetic 8-bit RLE frames
void check_hue8(Tally &t, int cases)
{
    Globals g;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const DWORD mask = rnd(4) ? 0x1f : 1 | rnd(32);
        const int src = make_rle(F_8 | render::F_RLE8, rnd_range(1, 120), rnd_range(1, 24), 1, mask, false, random());
        if (src < 0)
            continue;
        const int dst = make_dst(PAL8, frame(0, src)->width / 2 + 1, frame(0, src)->height / 2 + 1);
        DWORD op;
        const DWORD mode = mode_copy(0x6bb4e0, (int)op_kind(), &op);
        if (dst < 0 || mode == 0xffffffffu)
            continue;
        for (int side = 0; side < 2; side++)
            object<render::CBlendMode>(side, mode)->palette.data = g_hue_table8;
        Rects r;
        if (!make_rects(r, frame(0, src)->width, frame(0, src)->height, frame(0, dst)->width, frame(0, dst)->height, rnd(2) != 0))
            continue;
        char what[120];
        wsprintfA(what, "hue remap 8R->8 mask %x %dx%d -> %dx%d op %x", mask, frame(0, src)->width, frame(0, src)->height, frame(0, dst)->width,
                  frame(0, dst)->height, op);
        run_blit(t, g_rle16, g, what, 0x43c6b0, src, r, dst, mode);
    }
}

// ---- rle8 -------------------------------------------------------------------------------------------------------------------

struct Rle8Kind {
    DWORD fn, mode;
    const char *name;
    bool to16, zero, table;
};
constexpr Rle8Kind RLE8_KINDS[] = {
    {0x4402d0, 0x6bb4e0, "Copy 8R->8", false, true, false},     {0x440330, 0x6bb4e0, "Copy 8R->16", true, false, false},
    {0x4402d0, 0x6bcc68, "Transparent Mask 8R->8", false, true, false}, {0x438870, 0x6b9728, "Black Bias 8R->8", false, false, true},
    {0x438870, 0x6b9c60, "Test Light 8R->8", false, false, true}, {0x435570, 0x6b85f0, "Amplify Light 8R->8", false, false, true},
    {0, 0x6bb4e0, "Copy vf62", false, true, false},             {0, 0x6b9728, "Black Bias vf62", false, false, true},
};

BYTE *g_noise_table;   // a 64 KB blend table for modes without one

void check_rle8(Tally &t, int cases)
{
    Globals g;
    if (!g_noise_table)
        g_noise_table = static_cast<BYTE *>(heap_alloc(0x10000));
    for (DWORD i = 0; i < 0x10000; i++)
        g_noise_table[i] = (BYTE)(random() >> 11);
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const Rle8Kind &kind = RLE8_KINDS[(DWORD)k % (sizeof RLE8_KINDS / sizeof RLE8_KINDS[0])];
        const DWORD mask = rnd(3) ? 0x1f : rnd(32);
        const int src = make_rle(F_8 | render::F_RLE8, rnd_range(1, 120), rnd_range(1, 24), 1, mask ? mask : 1, kind.zero && rnd(2), random());
        if (src < 0)
            continue;
        const int dst = make_dst(kind.to16 ? RGB565 : PAL8, frame(0, src)->width / 2 + 1, frame(0, src)->height / 2 + 1);
        DWORD op;
        const DWORD mode = mode_copy(kind.mode, (int)op_kind(), &op);
        if (dst < 0 || mode == 0xffffffffu)
            continue;
        if (kind.table && (!object<render::CBlendMode>(0, mode)->palette.data || rnd(4) == 0))
            for (int side = 0; side < 2; side++)
                object<render::CBlendMode>(side, mode)->palette.data = g_noise_table;
        Rects r;
        if (!make_rects(r, frame(0, src)->width, frame(0, src)->height, frame(0, dst)->width, frame(0, dst)->height, rnd(2) != 0))
            continue;
        char what[120];
        wsprintfA(what, "%s mask %x %dx%d -> %dx%d op %x level %u", kind.name, mask, frame(0, src)->width, frame(0, src)->height,
                  frame(0, dst)->width, frame(0, dst)->height, op, object<render::CBlendMode>(0, mode)->opacity_entry);
        run_blit(t, g_rle8, g, what, kind.fn, src, r, dst, mode);
    }
}

// ---- Plain sources ---------------------------------------------------------------------------------------------------------

struct PlainKind {
    DWORD fn, mode;
    const char *name;
    Format src, dst;
    int shape;   // 0 same-size rectangles, 1 Tile (power-of-two source), 2 TileWrap, 3 Normal Scale
};
constexpr PlainKind RAW_KINDS[] = {
    {0x434a10, 0x6bb4e0, "Copy 16->16", RGB565, RGB565, 0},     {0x434a10, 0x6bb4e0, "Copy 16->16 555", RGB555, RGB555, 0},
    {0x43ec50, 0x6bb4e0, "Copy 8->8", PAL8, PAL8, 0},           {0x43ee70, 0x6bb4e0, "Copy 8->16", PAL8, RGB565, 0},
    {0x43f570, 0x6bb4e0, "Copy 16->8", RGB565, PAL8, 0},        {0x434390, 0x6b9728, "Black Bias 8->8", PAL8, PAL8, 0},
    {0x434600, 0x6b9728, "Black Bias 16->16 MMX", RGB565, RGB565, 0}, {0x447170, 0x6bc618, "Overlap 8->8", PAL8, PAL8, 0},
    {0x447300, 0x6bc618, "Overlap 16->16", RGB565, RGB555, 0},  {0x4465b0, 0x6bc4e0, "Tile 8->8", PAL8, PAL8, 1},
    {0x4466d0, 0x6bc4e0, "Tile 16->16", RGB565, RGB565, 1},     {0x446190, 0x6bc4a8, "TileWrap 8->8", PAL8, PAL8, 2},
    {0x4462b0, 0x6bc4a8, "TileWrap 16->16", RGB565, RGB565, 2}, {0x441b70, 0x6bb520, "Normal Scale 8->8", PAL8, PAL8, 3},
    {0x441f70, 0x6bb520, "Normal Scale 8->16", PAL8, RGB565, 3}, {0x4423c0, 0x6bb520, "Normal Scale 16->16", RGB565, RGB565, 3},
    {0, 0x6bb4e0, "Copy vf62 16->16", RGB565, RGB565, 0},       {0, 0x6bb4e0, "Copy vf62 8->16", PAL8, RGB565, 0},
};
constexpr PlainKind SCREEN_KINDS[] = {
    {0x443400, 0x6bbaa0, "ScreenCopy 8->16", PAL8, RGB555, 0},   {0x4439e0, 0x6bbaa0, "ScreenCopy plain", RGB565, RGB555, 0},
    {0x443600, 0x6bbaa0, "ScreenCopy MMX", RGB565, RGB555, 0},   {0x444410, 0x6bb968, "Up555 plain", RGB565, RGB555, 0},
    {0x443fd0, 0x6bb968, "Up555 MMX", RGB565, RGB555, 0},        {0x444b60, 0x6bbad8, "Down555 plain", RGB565, RGB555, 0},
    {0x444720, 0x6bbad8, "Down555 MMX", RGB565, RGB555, 0},      {0x445630, 0x6bc328, "Up565 plain", RGB565, RGB565, 0},
    {0x4450d0, 0x6bc328, "Up565 MMX", RGB565, RGB565, 0},        {0x445e50, 0x6bc468, "Down565 plain", RGB565, RGB565, 0},
    {0x4458f0, 0x6bc468, "Down565 MMX", RGB565, RGB565, 0},
};
// The screen levels (0x6bb558, 0x6bbb10, 0x6bb55c, 0x6bb660, 0x6bc360, 0x6bbc18, 0x6bbd20, 0x6bbc1c) as SetLevel
// writes them for a level 0..255
constexpr DWORD LEVELS[4][4] = {{0x6bb558, 3, 0x6bbb10, 2}, {0x6bb55c, 3, 0x6bb660, 2}, {0x6bc360, 2, 0x6bbc18, 3}, {0x6bbd20, 2, 0x6bbc1c, 3}};

void check_plain(Tally &t, ModuleHooks &hooks, const PlainKind *kinds, int nkinds, int cases)
{
    Globals g;
    for (const auto &l : LEVELS) {
        g.saved.add(l[0], 4);
        g.saved.add(l[2], 4);
    }
    const bool inverse = *at<DWORD *>(render::blit::INV_COLOR_TABLE) != 0;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const PlainKind &kind = kinds[(DWORD)k % (DWORD)nkinds];
        if (kind.fn == 0x43f570 && !inverse)
            continue;
        FrameSpec s;
        s.format = kind.src;
        if (kind.shape == 1) {
            s.width = 1 << rnd_range(0, 6);
            s.height = 1 << rnd_range(0, 5);
        } else {
            s.width = rnd_range(1, kind.shape ? 64 : 100);
            s.height = rnd_range(1, kind.shape ? 30 : 30);
        }
        if (rnd(3) == 0)
            s.pitch = s.width + rnd_range(1, 9);
        const int src = make_frame(s);
        const int dst = make_dst(kind.dst, kind.shape ? 1 : s.width / 2 + 1, kind.shape ? 1 : s.height / 2 + 1);
        DWORD op;
        const DWORD mode = mode_copy(kind.mode, (int)op_kind(), &op);
        if (src < 0 || dst < 0 || mode == 0xffffffffu)
            continue;
        if (kind.mode == 0x6b9728 && (!object<render::CBlendMode>(0, mode)->palette.data || rnd(4) == 0))
            for (int side = 0; side < 2; side++)
                object<render::CBlendMode>(side, mode)->palette.data = g_noise_table;
        const int dw = frame(0, dst)->width, dh = frame(0, dst)->height;
        Rects r;
        if (kind.shape == 0) {
            if (!make_rects(r, s.width, s.height, dw, dh, false))
                continue;
        } else {
            // the destination rectangle anywhere in the frame, the source corner anywhere (TileWrap: inside)
            const int cw = rnd_range(1, dw), ch = rnd_range(1, dh), dl = rnd_range(0, dw - cw), dt = rnd_range(0, dh - ch);
            r.dr = {dl, dt, dl + cw, dt + ch};
            const int sl = kind.shape == 2 ? rnd_range(0, s.width - 1) : kind.shape == 3 ? (rnd(3) ? 0 : rnd_range(0, 3)) : rnd_range(0, 200);
            const int st = kind.shape == 2 ? rnd_range(0, s.height - 1) : kind.shape == 3 ? 0 : rnd_range(0, 200);
            r.sr = {sl, st, s.width, s.height};
            if (kind.shape == 3 && rnd(12) == 0)   // the early returns: empty, negative, beyond the frame
                r.dr = rnd(2) ? framec::Rect{dl, dt, dl, dt + ch} : framec::Rect{dl - 1, dt, dl + cw + rnd_range(0, 3), dt + ch};
            r.s = alloc_object(16);
            r.d = alloc_object(16);
            if (r.s == 0xffffffffu || r.d == 0xffffffffu)
                continue;
            for (int side = 0; side < 2; side++) {
                *object<framec::Rect>(side, r.s) = r.sr;
                *object<framec::Rect>(side, r.d) = r.dr;
            }
        }
        const DWORD level = rnd(256);
        for (const auto &l : LEVELS) {
            *at<int *>(l[0]) = (int)(level >> l[1]);
            *at<int *>(l[2]) = (int)(level >> l[3]);
        }
        char what[150];
        wsprintfA(what, "%s %dx%d (%d,%d) -> %dx%d (%d,%d %dx%d) op %x level %u", kind.name, s.width, s.height, r.sr.left, r.sr.top, dw, dh,
                  r.dr.left, r.dr.top, r.dr.right - r.dr.left, r.dr.bottom - r.dr.top, op, level);
        // the levels are part of the saved memory: put them back after each side as set for the case
        run_sides(t, hooks, what, [&](int side) -> DWORD {
            void *m = object<void>(side, mode);
            void *sf = frame(side, src), *df = frame(side, dst);
            const void *sr = object<void>(side, r.s), *dr = object<void>(side, r.d);
            for (const auto &l : LEVELS) {
                *at<int *>(l[0]) = (int)(level >> l[1]);
                *at<int *>(l[2]) = (int)(level >> l[3]);
            }
            if (kind.fn)
                at<Blit>(kind.fn)(sf, sr, df, dr, m);
            else
                at<void(THISCALL *)(void *, void *, const void *, const void *, void *)>(0x4a8f10)(df, sf, sr, dr, m);
            __asm__ volatile("emms");
            const unsigned h = watched_hash(side);
            g.saved.restore();
            return h;
        });
    }
}

// ---- The Filter --------------------------------------------------------------------------------------------------------------

constexpr DWORD FILTER_STATE = 0x716120, FILTER_GUARD = 0x716100;

// A fresh FilterState at `s` (the original's or the reimplemented constructor through the exe address)
void filter_construct(void *s) { at<void *(THISCALL *)(void *)>(0x43ad90)(s); }
// The state's lists and frame freed; their contents hashed first (tap lists, the intermediate frame's pixels)
unsigned filter_destroy(void *s)
{
    struct TapList {
        DWORD count;
        BYTE *taps;
    };
    DWORD *d = static_cast<DWORD *>(s);
    unsigned h = fnv(2166136261u, d + 0x48 / 4, 0x18);
    for (int l = 0; l < 2; l++) {
        TapList *list = reinterpret_cast<TapList *>(d[0x40 / 4 + l]);
        const DWORD count = d[(l ? 0x58 : 0x4c) / 4];
        for (DWORD i = 0; list && i < count; i++) {
            h = fnv(h, &list[i].count, 4);
            if (list[i].taps) {
                h = fnv(h, list[i].taps, list[i].count * 8);
                heap_free(list[i].taps);
            }
        }
        if (list)
            heap_free(list);
    }
    const render::CFrame *f = static_cast<const render::CFrame *>(s);
    if (f->pixels)
        h = fnv(h, f->pixels, (DWORD)f->width * f->height * 2);
    at<void(THISCALL *)(void *)>(0x4a8b00)(s);
    return h;
}

// The tap lists of side 0 (copied) against side 1's: the first difference logged
struct TapCopy {
    DWORD counts[64];
    DWORD taps[64][40][2];
    DWORD n;
};
TapCopy *g_taps;
DWORD *g_dbg;   // side 0's tables (3 x 512) and intermediate pixels (up to 4096 words)
int g_dbg_logged;
void dbg_save_or_compare(int side, const void *state)
{
    if (!g_dbg)
        g_dbg = static_cast<DWORD *>(heap_alloc(0x8000));
    const render::CFrame *f = static_cast<const render::CFrame *>(state);
    const DWORD npix = f->pixels ? (DWORD)f->width * f->height : 0;
    const DWORD *tb[3] = {at<const DWORD *>(0x6b9cd4), at<const DWORD *>(0x6ba4d4), at<const DWORD *>(0x6bacd8)};
    if (side == 0) {
        for (int t = 0; t < 3; t++)
            memcpy(g_dbg + t * 512, tb[t], 0x800);
        g_dbg[1536] = npix;
        if (npix && npix < 4096)
            memcpy(g_dbg + 1540, f->pixels, npix * 2);
        return;
    }
    for (int t = 0; t < 3 && g_dbg_logged < 8; t++)
        for (int i = 0; i < 512; i++)
            if (g_dbg[t * 512 + i] != tb[t][i]) {
                log("    table %d [%d]: %x / %x", t, i - 128, g_dbg[t * 512 + i], tb[t][i]);
                g_dbg_logged++;
                break;
            }
    if (npix && npix == g_dbg[1536] && npix < 4096)
        for (DWORD i = 0; i < npix && g_dbg_logged < 8; i++)
            if (reinterpret_cast<const WORD *>(g_dbg + 1540)[i] != reinterpret_cast<const WORD *>(f->pixels)[i]) {
                log("    intermediate (%u, %u) of %ux%u: %x / %x", i % f->width, i / f->width, f->width, f->height,
                    reinterpret_cast<const WORD *>(g_dbg + 1540)[i], reinterpret_cast<const WORD *>(f->pixels)[i]);
                g_dbg_logged++;
                break;
            }
}
void taps_save(const void *s, int list, DWORD n)
{
    const DWORD *d = static_cast<const DWORD *>(s);
    const DWORD *l = reinterpret_cast<const DWORD *>(d[0x40 / 4 + list]);
    g_taps->n = n;
    for (DWORD i = 0; i < n && i < 64; i++) {
        g_taps->counts[i] = l[i * 2];
        for (DWORD k = 0; k < l[i * 2] && k < 40; k++) {
            g_taps->taps[i][k][0] = reinterpret_cast<const DWORD *>(l[i * 2 + 1])[k * 2];
            g_taps->taps[i][k][1] = reinterpret_cast<const DWORD *>(l[i * 2 + 1])[k * 2 + 1];
        }
    }
}
void taps_compare(const void *s, int list, DWORD n, int &logged)
{
    const DWORD *d = static_cast<const DWORD *>(s);
    const DWORD *l = reinterpret_cast<const DWORD *>(d[0x40 / 4 + list]);
    for (DWORD i = 0; i < n && i < 64 && logged < 6; i++) {
        if (g_taps->counts[i] != l[i * 2]) {
            log("    list %d entry %u: count %u / %u", list, i, g_taps->counts[i], l[i * 2]);
            logged++;
            continue;
        }
        for (DWORD k = 0; k < l[i * 2] && k < 40; k++) {
            const DWORD *t = reinterpret_cast<const DWORD *>(l[i * 2 + 1]) + k * 2;
            if (t[0] != g_taps->taps[i][k][0] || t[1] != g_taps->taps[i][k][1]) {
                log("    list %d entry %u tap %u: index %d / %d, weight %x / %x", list, i, k, (int)g_taps->taps[i][k][0], (int)t[0],
                    g_taps->taps[i][k][1], t[1]);
                logged++;
                break;
            }
        }
    }
}

void check_filter_parts(Tally &ctor, Tally &taps, int cases)
{
    Globals g;
    g.saved.add(0x6b9cd4, 0x800);
    g.saved.add(0x6ba4d4, 0x800);
    g.saved.add(0x6bacd8, 0x800);
    if (!g_taps)
        g_taps = static_cast<TapCopy *>(heap_alloc(sizeof(TapCopy)));
    begin_case(random());
    run_sides(ctor, g_filter, "FilterState_Ctor: the channel tables", [&](int) -> DWORD {
        void *s = heap_alloc(0x60);
        memset(s, 0, 0x60);
        filter_construct(s);
        unsigned h = filter_destroy(s);
        heap_free(s);
        h = fnv(h, at<const void *>(0x6b9cd4), 0x800);
        h = fnv(h, at<const void *>(0x6ba4d4), 0x800);
        h = fnv(h, at<const void *>(0x6bacd8), 0x800);
        g.saved.restore();
        return h;
    });
    int logged = 0;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const DWORD n_in = (DWORD)rnd_range(1, 64), n_out = (DWORD)rnd_range(1, 64);
        const float scale = rnd(4) ? (float)n_out / (float)n_in : (float)rnd_range(1, 4000) / 1000.0f;
        const int list = (int)rnd(2);
        char what[100];
        wsprintfA(what, "%s in %u out %u scale %x", list ? "Filter_ResampleColumns" : "Filter_ResampleRows", n_in, n_out, fbits(scale));
        run_sides(taps, g_filter, what, [&](int side) -> DWORD {
            void *s = heap_alloc(0x60);
            memset(s, 0, 0x60);
            filter_construct(s);
            at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(list ? 0x43b260 : 0x43ae70)(s, n_in, n_out, fbits(scale));
            if (side == 0)
                taps_save(s, list, n_out);
            else
                taps_compare(s, list, n_out, logged);
            unsigned h = filter_destroy(s);
            heap_free(s);
            g.saved.restore();
            return h;
        });
    }
}

void check_filter(Tally &apply, Tally &blits, int cases)
{
    Globals g;
    g.saved.add(0x6b9cd4, 0x800);
    g.saved.add(0x6ba4d4, 0x800);
    g.saved.add(0x6bacd8, 0x800);
    g.saved.add(FILTER_GUARD, 1);
    g.saved.add(FILTER_STATE, 0x60);
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const bool blit = k & 1;
        FrameSpec in;
        in.format = RGB565;
        // at least 3 x 3 on both ends: below that the mirrored taps leave the row buffers and the original reads
        // heap bytes around them (different on the two sides)
        in.width = rnd_range(3, 48);
        in.height = rnd_range(3, 32);
        if (blit && rnd(3) == 0)
            in.format = rnd(2) ? PAL8 : RGB555;
        const int src = make_frame(in);
        FrameSpec out;
        out.format = blit ? (rnd(2) ? PAL8 : RGB565) : RGB565;
        out.width = rnd_range(3, 64);
        out.height = rnd_range(3, 40);
        if (!blit)
            out.pitch = 0;   // Apply steps through `out` by its width
        else if (rnd(3) == 0)
            out.pitch = out.width + rnd_range(1, 5);
        const int dst = make_frame(out);
        DWORD op;
        const DWORD mode = mode_copy(0x6b9ca0, (int)op_kind(), &op);
        if (src < 0 || dst < 0 || mode == 0xffffffffu)
            continue;
        char what[120];
        if (!blit) {
            const bool twice = rnd(2) == 0;
            wsprintfA(what, "FilterState_Apply %dx%d -> %dx%d%s", in.width, in.height, out.width, out.height, twice ? " twice" : "");
            run_sides(apply, g_filter, what, [&](int side) -> DWORD {
                void *s = heap_alloc(0x60);
                filter_construct(s);
                at<void(THISCALL *)(void *, void *, void *)>(0x43b650)(s, frame(side, dst), frame(side, src));
                if (twice)   // again: the lists kept
                    at<void(THISCALL *)(void *, void *, void *)>(0x43b650)(s, frame(side, dst), frame(side, src));
                dbg_save_or_compare(side, s);
                unsigned h = filter_destroy(s);
                heap_free(s);
                h ^= watched_hash(side);
                h = fnv(h, at<const void *>(0x6b9cd4), 0x800);
                g.saved.restore();
                return h;
            });
            continue;
        }
        // the blitters use the static state: a fresh one for each side, the game's put back after
        const int dw = frame(0, dst)->width, dh = frame(0, dst)->height;
        const int cw = rnd_range(3, in.width), ch = rnd_range(3, in.height);
        const int ow = rnd_range(3, dw), oh = rnd_range(3, dh);
        Rects r;
        r.sr = {rnd_range(0, in.width - cw), rnd_range(0, in.height - ch), 0, 0};
        r.sr.right = r.sr.left + cw;
        r.sr.bottom = r.sr.top + ch;
        r.dr = {rnd_range(0, dw - ow), rnd_range(0, dh - oh), 0, 0};
        r.dr.right = r.dr.left + ow;
        r.dr.bottom = r.dr.top + oh;
        if (rnd(10) == 0)
            r.dr.right = r.dr.left;   // empty: no resampling
        r.s = alloc_object(16);
        r.d = alloc_object(16);
        if (r.s == 0xffffffffu || r.d == 0xffffffffu)
            continue;
        for (int side = 0; side < 2; side++) {
            *object<framec::Rect>(side, r.s) = r.sr;
            *object<framec::Rect>(side, r.d) = r.dr;
        }
        const DWORD fn = out.format == PAL8 ? 0x43bad0 : 0x43bd10;
        wsprintfA(what, "BlitFilter %s %dx%d (%d,%d %dx%d) -> %dx%d (%d,%d %dx%d) op %x", format_name(out.format), in.width, in.height,
                  r.sr.left, r.sr.top, cw, ch, dw, dh, r.dr.left, r.dr.top, r.dr.right - r.dr.left, oh, op);
        run_sides(blits, g_filter, what, [&](int side) -> DWORD {
            *at<BYTE *>(FILTER_GUARD) |= 1;
            filter_construct(at<void *>(FILTER_STATE));
            at<Blit>(fn)(frame(side, src), object<void>(side, r.s), frame(side, dst), object<void>(side, r.d), object<void>(side, mode));
            unsigned h = filter_destroy(at<void *>(FILTER_STATE));
            h ^= watched_hash(side);
            g.saved.restore();
            return h;
        });
    }
}

}  // namespace

ZAX_CHECKS(rle16)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    collect_sources(g_sources, 12);
    g_seed = 0x438c40;
    Tally blits{"rle16: 16-bit RLE blitters (Copy, Transparent Mask, Black Bias, Amplify Light, Overlap, hue remap; MMX and plain; real and synthetic)"};
    check_rle16(blits, scaled(2400));
    blits.report();
    g_seed = 0x43c6b0;
    Tally hue8{"rle16: the 8-bit hue remap blitter on synthetic 8-bit RLE frames"};
    check_hue8(hue8, scaled(300));
    hue8.report();
}

ZAX_CHECKS(rle8)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    g_seed = 0x4402d0;
    Tally blits{"rle8: 8-bit RLE blitters and drawers (Copy to 8 and 16 bits, Transparent Mask, Black Bias, Test Light, Amplify Light; all levels)"};
    check_rle8(blits, scaled(2000));
    blits.report();
}

ZAX_CHECKS(blitters_raw)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    g_seed = 0x434a10;
    if (!g_noise_table) {
        g_noise_table = static_cast<BYTE *>(heap_alloc(0x10000));
        for (DWORD i = 0; i < 0x10000; i++)
            g_noise_table[i] = (BYTE)(random() >> 11);
    }
    Tally blits{"blitters_raw: plain blitters (Copy, Black Bias, Overlap, Tile, TileWrap, Normal Scale)"};
    check_plain(blits, g_raw, RAW_KINDS, sizeof RAW_KINDS / sizeof RAW_KINDS[0], scaled(2700));
    blits.report();
}

ZAX_CHECKS(blitters_screen)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    g_seed = 0x443400;
    Tally blits{"blitters_screen: the screen modes (ScreenCopy, Up/Down 5-5-5 and 5-6-5; MMX and plain; all levels)"};
    check_plain(blits, g_screen, SCREEN_KINDS, sizeof SCREEN_KINDS / sizeof SCREEN_KINDS[0], scaled(2200));
    blits.report();
}

ZAX_CHECKS(blitters_filter)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    g_seed = 0x43bd10;
    Tally ctor{"blitters_filter: FilterState_Ctor (the channel tables)"};
    Tally taps{"blitters_filter: the row and column weights"};
    check_filter_parts(ctor, taps, scaled(600));
    ctor.report();
    taps.report();
    Tally apply{"blitters_filter: FilterState_Apply on fake frames"};
    Tally blits{"blitters_filter: the Filter blitters 8->8 and 16->16"};
    check_filter(apply, blits, scaled(600));
    apply.report();
    blits.report();
}

}  // namespace zax
