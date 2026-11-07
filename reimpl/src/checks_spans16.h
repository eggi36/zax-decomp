// The checks of the 16-bit spans (checks_spans16.cpp for spans16.cpp, checks_spans16_opacity.cpp for
// spans16_opacity.cpp) on the frame check harness (checks_frame_common.h):
//  - the live tables: T10-T17 (entry 0 the stub, 1-16 the unrolled instances, 17-63 the generic loop), the
//    Amplify Light and Overlap tables, every entry where the module's hooks are;
//  - every span directly, the original (its trampoline) against the hooked address on the same fake 16-bit
//    frames: every table entry 1-63 with its own count (and the unrolled ones with counts they ignore), the
//    generic loops with every count 0-16 and above 63, destination and source at every word alignment
//    (0-3 pixels into a row), 5-6-5 and 5-5-5 pixels, random pixels and real ones (a ground texture's rows,
//    a sprite's pixel block), alpha bytes random, the edge values and a sprite's alpha and edge blocks,
//    values with garbage in the upper word, the opacity through the 256 real fade tables and through a
//    table of random dwords (weights above 0xff);
//  - through the blitters: copies of the live blend modes (Copy, Transparent Mask, Black Bias, Amplify
//    Light, Overlap) drawing a real sprite (a 16-bit RLE frame copied whole) into a fake 5-6-5 or 5-5-5
//    frame with FindBlitter (CFrame slot 62 0x4a8f10: the live registry's blitter, MMX on an MMX CPU) and
//    with the scalar blitters by address (0x438c40, 0x435950, the Overlap 0x446e30), the whole frame and
//    cut rectangles (the RLE drawers' full and clipped paths), opacity 1.0, 0 and random; side 0 with the
//    module's hooks out.
// eax is not compared for the spans (no caller reads it; the original leaves whatever its last
// instruction put there).
#pragma once
#include "checks_frame_r1.h"
#include "spans16.h"

namespace zax::render {
// The span table T k (64 entries each, one every 0x100 bytes from 0x60cd90; blitters.h T0-T35)
constexpr DWORD span_table(int k) { return 0x60cd90 + (DWORD)k * 0x100; }
// The Amplify Light tables of 16-bit literals and runs (the span tables, filled at start)
constexpr DWORD AMPLIFY_LITERAL_TABLES[2] = {0x6b6ff0, 0x6b78f0}, AMPLIFY_RUN_TABLES[2] = {0x6b74f0, 0x6b7af0};
// The Overlap tables (literal, literal, run, alpha run)
constexpr DWORD OVERLAP_TABLES[4] = {0x6bc84c, 0x6bc94c, 0x6bca4c, 0x6bc518};
}  // namespace zax::render

namespace zax::spans16c {

using namespace framec;
using r1::rnd;
using r1::rnd_range;

enum Kind { COPY, FILL, ALPHA_COPY, ALPHA_FILL, OPACITY_COPY, OPACITY_FILL, AO_COPY, AO_FILL, AMP_LITERAL, AMP_FILL, OVL_FILL, OVL_FILL_ALPHA, KINDS };
inline const char *kind_name(int k)
{
    static const char *const names[KINDS] = {"Copy", "Fill", "Alpha_Copy", "Alpha_Fill", "Opacity_Copy", "Opacity_Fill",
                                             "AlphaOpacity_Copy", "AlphaOpacity_Fill", "Amplify16_Literal", "Amplify16_Fill",
                                             "Overlap16_Fill", "Overlap16_FillAlpha"};
    return names[k];
}
inline bool takes_alpha(int k) { return k == ALPHA_COPY || k == ALPHA_FILL || k == AO_COPY || k == AO_FILL || k == OVL_FILL_ALPHA; }
inline bool src_pointer(int k) { return k == COPY || k == ALPHA_COPY || k == OPACITY_COPY || k == AO_COPY || k == AMP_LITERAL; }
inline bool alpha_pointer(int k) { return k == ALPHA_COPY || k == AO_COPY; }
inline bool reads_opacity(int k) { return k >= OPACITY_COPY && k <= AO_FILL; }
// The generic loops that run their body before testing the count (n = 0 would run 2^32 times)
inline bool do_while(int k) { return k == ALPHA_COPY || k == OPACITY_COPY || k == AO_COPY || k == OVL_FILL || k == OVL_FILL_ALPHA; }

// A span of the module: its address, kind and count (0: a generic loop that takes the count)
struct SpanFn {
    DWORD address;
    int kind, n;
    const char *name;
};
struct SpanList {
    SpanFn list[80];
    int n = 0;
};

// The module's hooks by address (the shared 8-bit copies T10[4/6/8] are not in the list)
inline const char *hook_name(int (*table)(Hook *), DWORD address)
{
    Hook hooks[ModuleHooks::MAX];
    const int count = table(nullptr);
    if (count > ModuleHooks::MAX)
        return nullptr;
    table(hooks);
    for (int i = 0; i < count; i++)
        if (hooks[i].address == address)
            return hooks[i].name;
    return nullptr;
}

// The tables first..last (T10-T13 or T14-T17) checked and their spans listed
inline void check_tables(Tally &t, int (*table)(Hook *), int first, int last, SpanList &out)
{
    static const DWORD SHARED[3] = {0x44a030, 0x44a0d0, 0x44a170};   // T10[4], T10[6], T10[8]: Span8_Copy_8/12/16 (R2)
    for (int k = first; k <= last; k++) {
        const DWORD *e = at<const DWORD *>(render::span_table(k));
        const bool alpha = takes_alpha(k - 10);
        char d[120];
        wsprintfA(d, "[T%d[0] %x]", k, e[0]);
        t.check(e[0] == (alpha ? 0x4f0a80u : 0x401110u), "", d);
        for (int n = 1; n <= 16; n++) {
            const char *name = hook_name(table, e[n]);
            const bool shared = k == 10 && (e[n] == SHARED[0] || e[n] == SHARED[1] || e[n] == SHARED[2]) && (n == 4 || n == 6 || n == 8);
            wsprintfA(d, "[T%d[%d] %x: not a hook of the module]", k, n, e[n]);
            t.check(name || shared, "", d);
            if (name && out.n < 80)
                out.list[out.n++] = {e[n], k - 10, n, name};
        }
        const char *generic = hook_name(table, e[17]);
        bool same = generic != nullptr;
        for (int n = 18; n < 64; n++)
            same = same && e[n] == e[17];
        wsprintfA(d, "[T%d[17..63] %x: not one hooked generic loop]", k, e[17]);
        t.check(same, "", d);
        if (generic && out.n < 80)
            out.list[out.n++] = {e[17], k - 10, 0, generic};
    }
}

// A dynamic table of 64 entries, all `fn`
inline void check_dynamic_table(Tally &t, DWORD table, DWORD fn)
{
    const DWORD *e = at<const DWORD *>(table);
    int bad = -1;
    for (int n = 0; n < 64 && bad < 0; n++)
        if (e[n] != fn)
            bad = n;
    char d[80];
    wsprintfA(d, "[table %x entry %d: %x, not %x]", table, bad, bad >= 0 ? e[bad] : 0, fn);
    t.check(bad < 0, "", d);
}

// ---- The spans directly -------------------------------------------------------------------------------------

inline ModuleHooks g_no_hooks{"spans16 (direct)", [](Hook *) { return 0; }, {}, {}, {}, 0, true};
inline Sources g_sources;
constexpr int ROW = 140;   // pixels of a fake row: up to 3 + 100 + some room after it

constexpr BYTE EDGE_ALPHA[] = {0, 1, 2, 3, 4, 5, 7, 8, 63, 64, 127, 128, 129, 191, 192, 251, 252, 253, 254, 255};

// Real data of the level: a 16-bit plain frame (ground texture, world frame) and a 16-bit compressed one
inline const FrameData *pick_source(bool compressed)
{
    int cand[Sources::MAX], m = 0;
    for (int i = 0; i < g_sources.n; i++) {
        const FrameData *f = g_sources.list[i].frame;
        if ((f->flags & F_DEPTH) == F_16 && ((f->flags & F_COMPRESSED) != 0) == compressed && (!compressed || r1::compressed_sane(f)))
            cand[m++] = i;
    }
    return m ? g_sources.list[cand[rnd((DWORD)m)]].frame : nullptr;
}
// `bytes` bytes of block b of a compressed frame from a random offset, or false
inline bool block_bytes(const FrameData *f, int b, BYTE *out, DWORD bytes)
{
    const BYTE *d = f ? block_data(f, b) : nullptr;
    if (!d)
        return false;
    const DWORD size = *reinterpret_cast<const DWORD *>(d);
    if (size < 4 + bytes)
        return false;
    memcpy(out, d + 4 + rnd(size - 4 - bytes + 1), bytes);
    return true;
}

struct DirectCase {
    int kind;
    DWORD address;
    DWORD n;                 // the count passed
    int dst_x, src_x;        // pixels into the row (alignment)
    Format dst_format, src_format;
    bool real_dst, real_src, real_alpha;
    int alpha_mode;          // 0 random, 1 an edge value for all, 2 edge values mixed
    int opacity;             // 0..255 a real fade table, -1 a table of random dwords
    DWORD value, alpha_value;
    const char *name;
};

// Runs one direct case: fake rows on both sides, the original's trampoline on side 0, the hook on side 1
inline void run_direct(Tally &t, const DirectCase &c)
{
    begin_case(random());
    FrameSpec s;
    s.width = ROW;
    s.height = 2;
    s.format = c.dst_format;
    const int dst = make_frame(s);
    s.format = c.src_format;
    const int src = make_frame(s);
    const DWORD alpha = alloc_object(ROW), table = c.opacity < 0 ? alloc_object(0x400) : 0xffffffffu;
    if (dst < 0 || src < 0 || alpha == 0xffffffffu || (c.opacity < 0 && table == 0xffffffffu))
        return;
    if (c.real_dst)
        if (const FrameData *f = pick_source(false))
            copy_source(dst, f);
    BYTE bytes[ROW * 2];
    if (c.real_src && block_bytes(pick_source(true), 0, bytes, sizeof bytes))
        for (int side = 0; side < 2; side++)
            memcpy(frame(side, src)->pixels, bytes, sizeof bytes);
    BYTE a[ROW];
    for (int i = 0; i < ROW; i++)
        a[i] = (BYTE)(random() >> 13);
    if (c.alpha_mode == 1)
        memset(a, EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)], sizeof a);
    else if (c.alpha_mode == 2)
        for (int i = 0; i < ROW; i++)
            a[i] = EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)];
    if (c.real_alpha) {
        const FrameData *f = pick_source(true);
        if (!block_bytes(f, 4, a, sizeof a))
            block_bytes(f, 3, a, sizeof a);
    }
    for (int side = 0; side < 2; side++) {
        memcpy(object<BYTE>(side, alpha), a, sizeof a);
        if (c.opacity < 0) {
            DWORD *tb = object<DWORD>(side, table);
            for (int i = 0; i < 256; i++)
                tb[i] = side ? object<DWORD>(0, table)[i] : (rnd(4) ? random() >> rnd(32) : random() & 0xff);
        }
    }
    char what[100];
    wsprintfA(what, "%s n %u dst %s+%d src %s+%d op %d", c.name, c.n, format_name(c.dst_format), c.dst_x, format_name(c.src_format),
              c.src_x, c.opacity);
    const DWORD saved_table = *at<DWORD *>(render::OPACITY_TABLE);
    run_sides(t, g_no_hooks, what, [&](int side) -> DWORD {
        WORD *d = reinterpret_cast<WORD *>(frame(side, dst)->pixels) + c.dst_x;
        const DWORD sv = src_pointer(c.kind) ? ptrv(reinterpret_cast<WORD *>(frame(side, src)->pixels) + c.src_x) : c.value;
        const DWORD av = alpha_pointer(c.kind) ? ptrv(object<BYTE>(side, alpha)) : c.alpha_value;
        *at<DWORD *>(render::OPACITY_TABLE) = c.opacity < 0 ? ptrv(object<DWORD>(side, table)) : render::FADE_TABLES + (DWORD)c.opacity * 0x400;
        const DWORD fn = side ? c.address : ptrv(original_address(c.address));
        if (takes_alpha(c.kind))
            reinterpret_cast<render::AlphaSpan>(fn)(d, sv, av, c.n);
        else
            reinterpret_cast<render::Span>(fn)(d, sv, c.n);
        *at<DWORD *>(render::OPACITY_TABLE) = saved_table;
        return 0;
    });
}

// A case with random data around span f and count n
inline DirectCase random_case(const SpanFn &f, DWORD n)
{
    DirectCase c;
    c.kind = f.kind;
    c.address = f.address;
    c.n = n;
    c.name = f.name;
    c.dst_x = (int)rnd(4);
    c.src_x = (int)rnd(4);
    c.dst_format = rnd(3) ? RGB565 : RGB555;
    c.src_format = rnd(4) ? c.dst_format : (c.dst_format == RGB565 ? RGB555 : RGB565);
    c.real_dst = rnd(3) == 0;
    c.real_src = rnd(3) == 0;
    c.real_alpha = rnd(4) == 0;
    c.alpha_mode = (int)rnd(3);
    c.opacity = rnd(5) ? (int)rnd(256) : -1;
    c.value = rnd(2) ? random() : random() & 0xffff;
    c.alpha_value = rnd(3) ? random() : EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)] | (rnd(2) ? random() & 0xffffff00u : 0);
    return c;
}

// Every span of the list: its own count (an unrolled entry: its n; a generic loop: every entry 17-63), counts
// the unrolled ones ignore, the generic loops with 0-16 and above 63; `per` random cases each
inline void check_direct(Tally &t, const SpanList &spans, int per)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (f.n) {
            for (int k = 0; k < per; k++)
                run_direct(t, random_case(f, k % 4 == 3 ? rnd(100) : (DWORD)f.n));
            continue;
        }
        for (DWORD n = 17; n < 64; n++)
            for (int k = 0; k < per; k++)
                run_direct(t, random_case(f, n));
        for (DWORD n = do_while(f.kind) ? 1 : 0; n <= 16; n++)
            run_direct(t, random_case(f, n));
        for (int k = 0; k < per; k++)
            run_direct(t, random_case(f, 64 + rnd(37)));
    }
}

// Every opacity: the 256 fade tables for each span that reads one (a random count of its range)
inline void check_opacities(Tally &t, const SpanList &spans)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (!reads_opacity(f.kind) || (f.n != 0 && f.n != 1 && f.n != 16))
            continue;
        for (int op = 0; op < 256; op++) {
            DirectCase c = random_case(f, f.n ? (DWORD)f.n : 17 + rnd(47));
            c.opacity = op;
            run_direct(t, c);
        }
    }
}

// Every alpha value 0-255 for the alpha spans (all pixels the same alpha; for the pointer ones also the
// opacity table at random)
inline void check_alphas(Tally &t, const SpanList &spans)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (!takes_alpha(f.kind) || f.kind == OVL_FILL_ALPHA || (f.n != 0 && f.n != 1 && f.n != 7))
            continue;
        for (DWORD a = 0; a < 256; a++) {
            DirectCase c = random_case(f, f.n ? (DWORD)f.n : 17 + rnd(47));
            c.real_alpha = false;
            c.alpha_mode = 3;   // set below
            c.alpha_value = a | (rnd(2) ? random() & 0xffffff00u : 0);
            if (alpha_pointer(f.kind)) {
                c.alpha_mode = 1;
                c.alpha_value = a;
            }
            run_direct(t, c);
        }
    }
}

// ---- Through the blitters ----------------------------------------------------------------------------------------

struct BlitMode {
    DWORD object;      // the live mode
    DWORD scalar;      // its scalar 16R -> 16 blitter
    const char *name;
};
constexpr BlitMode BLIT_MODES[] = {{0x6bb4e0, 0x438c40, "Copy"},
                                   {0x6bcc68, 0x438c40, "Transparent Mask"},
                                   {0x6b9728, 0x438c40, "Black Bias"},
                                   {0x6b85f0, 0x435950, "Amplify Light"},
                                   {0x6bc618, 0x446e30, "Overlap"}};

// `cases` blits of real sprites; opacity_one: the share of cases with opacity 1.0 (of 4)
inline void check_blits(Tally &t, ModuleHooks &hooks, int cases, int opacity_one)
{
    r1::SavedMemory saved;
    saved.add(render::OPACITY_TABLE, 4);
    saved.add(0x6bc64c, 0x200);   // the Overlap maps and their state (FindBlitter builds them once)
    saved.add(0x6bcb4c, 0x100);
    saved.add(0x6bcc50, 0x10);
    saved.add(0x6bcc60, 1);
    int sprites = 0;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const FrameData *real = pick_source(true);
        if (!real)
            break;
        const int src = copy_compressed(real);
        if (src < 0)
            continue;
        sprites++;
        const int sw = real->width, sh = real->height;
        FrameSpec d;
        d.format = rnd(3) ? RGB565 : RGB555;
        d.width = rnd_range(sw / 2 > 1 ? sw / 2 : 1, sw + 20 < 150 ? sw + 20 : 150);
        d.height = rnd_range(sh / 2 > 1 ? sh / 2 : 1, sh + 10 < 120 ? sh + 10 : 120);
        if (rnd(4) == 0)
            d.pitch = d.width + rnd_range(1, 7);
        const int dst = make_frame(d);
        if (dst < 0)
            continue;
        const BlitMode &bm = BLIT_MODES[(DWORD)k % (sizeof BLIT_MODES / sizeof BLIT_MODES[0])];
        const DWORD mode = r1::copy_mode_object(bm.object);
        // Amplify Light registers no edge spans (its tables 0x6b70f0/0x6b76f0 are 0): its drawers would call
        // address 0 for an edge block, and its light sprites have none; the copy drops the edge block
        if (bm.object == 0x6b85f0)
            for (int side = 0; side < 2; side++)
                frame(side, src)->blocks[6] = frame(side, src)->blocks[7] = nullptr;
        const DWORD srect = alloc_object(16), drect = alloc_object(16);
        // the rectangles inside both frames: the whole sprite or a cut (the drawers' clipped path)
        const int w = sw < d.width ? sw : d.width, h = sh < d.height ? sh : d.height;
        const bool cut = rnd(2) != 0;
        const int cw = cut ? rnd_range(1, w) : w, ch = cut ? rnd_range(1, h) : h;
        const int sl = cut ? rnd_range(0, sw - cw) : 0, st = cut ? rnd_range(0, sh - ch) : 0;
        const int dl = rnd_range(0, d.width - cw), dt = rnd_range(0, d.height - ch);
        const int o = (int)rnd(4);
        const DWORD opacity_bits = o < opacity_one ? 0x3f800000u : rnd(6) == 0 ? 0u : rnd(6) == 0 ? 0x3f000000u : random() % 0x3f800000u;
        for (int side = 0; side < 2; side++) {
            *object<framec::Rect>(side, srect) = {sl, st, sl + cw, st + ch};
            *object<framec::Rect>(side, drect) = {dl, dt, dl + cw, dt + ch};
            at<void(THISCALL *)(void *, DWORD)>(0x433be0)(object<void>(side, mode), opacity_bits);   // BlendMode_SetOpacity
        }
        const bool scalar = rnd(2) != 0;
        char what[100];
        wsprintfA(what, "%s %s src %dx%d (%d,%d %dx%d) -> %s %dx%d at %d,%d op %x", bm.name, scalar ? "scalar" : "FindBlitter", sw, sh, sl, st,
                  cw, ch, format_name(d.format), d.width, d.height, dl, dt, opacity_bits);
        if (r1::g_frame_trace)
            log("  blit case %d: %s", k, what);
        run_sides(t, hooks, what, [&](int side) -> DWORD {
            void *m = object<void>(side, mode);
            framec::Rect *s0 = object<framec::Rect>(side, srect), *d0 = object<framec::Rect>(side, drect);
            if (scalar)
                at<render::Blitter>(bm.scalar)(reinterpret_cast<render::CFrame *>(frame(side, src)), reinterpret_cast<const render::Rect *>(s0),
                                               reinterpret_cast<render::CFrame *>(frame(side, dst)), reinterpret_cast<const render::Rect *>(d0),
                                               static_cast<render::CBlendMode *>(m));
            else
                at<void(THISCALL *)(void *, void *, void *, void *, void *)>(0x4a8f10)(frame(side, dst), frame(side, src), s0, d0, m);
            __asm__ volatile("emms");   // the MMX Amplify blitter 0x4355f0 leaves the MMX state set
            saved.restore();
            return 0;
        });
    }
    char d[60];
    wsprintfA(d, "[no 16-bit sprite among the sources]");
    t.check(sprites > 0, "", d);
    saved.release();
}

inline int read_span_mutation()
{
    char ini[MAX_PATH + 32];
    GetModuleFileNameA(NULL, ini, MAX_PATH);
    for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
        ini[n - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    return (int)GetPrivateProfileIntA("Reimpl", "SpanMutation", 0, ini);
}

}  // namespace zax::spans16c
