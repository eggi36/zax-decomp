// Checks of blit_rest4b.cpp (the simple warp mode's 16-bit RLE blitters 0x4e2f10 (MMX) and 0x4e32c0) on the frame check harness, in
// the game thread: called by its exe address on fake frames, side 0 with the module's hooks out: synthetic RLE
// sources (exact rows; pixels, alpha and edge blocks in any combination) and real 16-bit sprites of the level
// (copied whole), 5-6-5 and 5-5-5 destinations with pitch padding, whole-width (the full drawers) and cut
// rectangles (the row drawers), copies of the live mode 0x6c1a78 at opacity 1, 0, 0.5, the fade levels,
// random and NaN; every drawer the exe's, the warp object's methods (0x571b70, slots 28-30, 0x571bb0: the effects
// batch's) recording stand-ins (with the warp value 0x6c1ab0 unset, as on the test levels, the real slot 29 ends
// the game, in the original too).
// Compared besides the frames and the call log: the fade table pointer 0x6b67e8 (put back after each side).
// The row and edge drawers' make_rle and the mode and rectangle helpers are checks_blitters.cpp's.
#include "checks_frame_r1.h"

namespace zax {

int hooks_blit_rest4b(Hook *out);

namespace {

using namespace framec;
using r1::rnd;
using r1::rnd_range;

ModuleHooks g_hooks{"blit_rest4b", &hooks_blit_rest4b, {}, {}, {}, 0, true};
Sources g_sources;

using Blit = void(__fastcall *)(void *src, const void *sr, void *dst, const void *dr, void *mode);

DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// ---- Synthetic RLE frames with exact rows (checks_blitters.cpp) -------------------------------------------------------

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

// ---- Modes and rectangles (checks_blitters.cpp) -------------------------------------------------------------

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

// The warp object's methods (the effects batch's CSimpleWarpEffect code) as recording stand-ins: with the test
// levels' warp value (0x6c1ab0) unset the real slot 29 ends the game (in the original too); the events hashed
// per side and returned with the case's eax
unsigned g_ev;
void ev(DWORD a, DWORD b = 0, DWORD c = 0)
{
    const DWORD e[3] = {a, b, c};
    g_ev = fnv(g_ev, e, sizeof e);
}
int g_side_now;
DWORD rel_frame(const void *p) { return map_value(g_side_now, ptrv(p)); }
void *THISCALL s_warp_ctor(BYTE *w)
{
    ev(0x571b70);
    memset(w, 0x5a, 0x2c);
    return w;
}
DWORD THISCALL s_warp_dtor(BYTE *w)
{
    ev(0x571bb0, *reinterpret_cast<DWORD *>(w + 8), *reinterpret_cast<DWORD *>(w + 0x2c));
    return 0;
}
DWORD THISCALL s_warp28(BYTE *w, void *f) { ev(0x572020, rel_frame(f), *reinterpret_cast<DWORD *>(w + 0x2c)); return 0; }
DWORD THISCALL s_warp29(BYTE *, void *f) { ev(0x572300, rel_frame(f)); return 0; }
DWORD THISCALL s_warp30(BYTE *, void *f) { ev(0x572680, rel_frame(f)); return 0; }
struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {{0x571b70, (void *)&s_warp_ctor}, {0x571bb0, (void *)&s_warp_dtor}, {0x572020, (void *)&s_warp28},
                        {0x572300, (void *)&s_warp29},    {0x572680, (void *)&s_warp30}};
CodePatch g_patches[5];

// The x87 environment (tag, status and control word) at a side's entry and after its call, for the log of a
// difference (the MMX state left set inside the blit made the edge rows' drawer choice differ on real x86 only,
// docs/DECOMPILATION.md 12, "Seven checks that differ only on real x86")
DWORD g_env[2][2][3];
void fpu_env(DWORD *out)
{
    DWORD env[7];
    __asm__ volatile("fnstenv %0\n\tfldenv %0" : "+m"(env));
    out[0] = env[2] & 0xffff;
    out[1] = env[1] & 0xffff;
    out[2] = env[0] & 0xffff;
}

void check_warp_blit(Tally &t, int cases)
{
    int real_used = 0;
    const DWORD saved_fade = *at<DWORD *>(0x6b67e8);
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const FrameData *real = rnd(3) == 0 ? pick_sprite() : nullptr;
        int src;
        if (real) {
            src = copy_compressed(real);
            real_used++;
        } else {
            const DWORD mask = 1 | (rnd(3) == 0 ? 0 : 0x18) | (rnd(3) == 0 ? 0x6 : 0);
            src = make_rle(F_16 | render::F_RLE16, rnd_range(1, 140), rnd_range(1, 36), 2, rnd(12) ? mask : 0x18, rnd(2), random());
        }
        if (src < 0)
            continue;
        const int sw = frame(0, src)->width, sh = frame(0, src)->height;
        const bool full = rnd(2) != 0 && sw <= 150;
        const int dst = make_dst(rnd(3) ? RGB565 : RGB555, full ? sw : sw / 2 > 1 ? sw / 2 : 1, sh / 2 > 1 ? sh / 2 : 1);
        if (dst < 0)
            continue;
        DWORD op;
        const DWORD mode = mode_copy(0x6c1a78, (int)op_kind(), &op);
        if (mode == 0xffffffffu)
            continue;
        Rects r;
        if (!make_rects(r, sw, sh, frame(0, dst)->width, frame(0, dst)->height, full))
            continue;
        char what[150];
        wsprintfA(what, "warp blit %s %s %dx%d (%d,%d %dx%d) -> %dx%d at %d,%d op %x", k % 2 ? "plain" : "MMX", real ? "real" : "synthetic", sw, sh, r.sr.left, r.sr.top,
                  r.sr.right - r.sr.left, r.sr.bottom - r.sr.top, frame(0, dst)->width, frame(0, dst)->height, r.dr.left, r.dr.top, op);
        if (r1::g_frame_trace)
            log("  blit case: %s", what);
        for (int i = 0; i < 5; i++)
            patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
        const bool same = run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            g_ev = 2166136261u;
            g_side_now = side;
            *at<DWORD *>(0x6b67e8) = saved_fade;
            fpu_env(g_env[side][0]);
            at<Blit>(k % 2 ? 0x4e32c0 : 0x4e2f10)(frame(side, src), object<void>(side, r.s), frame(side, dst), object<void>(side, r.d), object<void>(side, mode));
            fpu_env(g_env[side][1]);
            const DWORD fade = *at<DWORD *>(0x6b67e8);
            *at<DWORD *>(0x6b67e8) = saved_fade;
            return fade ^ g_ev;
        });
        if (!same && t.mismatches <= 10)
            log("    x87 tag/status/control at entry %x/%x/%x and %x/%x/%x, after %x/%x/%x and %x/%x/%x", g_env[0][0][0], g_env[0][0][1],
                g_env[0][0][2], g_env[1][0][0], g_env[1][0][1], g_env[1][0][2], g_env[0][1][0], g_env[0][1][1], g_env[0][1][2],
                g_env[1][1][0], g_env[1][1][1], g_env[1][1][2]);
        for (int i = 5; i-- > 0;)
            unpatch(g_patches[i]);
    }
    *at<DWORD *>(0x6b67e8) = saved_fade;
    t.check(real_used > 0, "", "[no 16-bit sprite among the sources]");
}

}  // namespace

ZAX_CHECKS(blit_rest4b)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    collect_sources(g_sources, 12);
    g_seed = 0x4e2f10;
    Tally t{"warp blit: the simple warp mode's 16-bit RLE blitters, MMX and plain (synthetic and real sprites, whole and cut, all opacities)"};
    check_warp_blit(t, scaled(1500));
    t.report();
}

}  // namespace zax
