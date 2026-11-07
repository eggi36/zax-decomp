// Checks of batch R6 (blit24_spans32.cpp, blit24_spans.cpp, blit24_spans_op.cpp, blit24.cpp, blit24_tm.cpp) on
// the frame check harness (checks_frame_common.h), in the game thread:
//  - the evidence first: the calls of the batch's blitters the game made before any check of the batch ran
//    (blit24.h, `count_call`) and the depths of the level's frames (every frame the harness can find);
//  - the span tables T24-T35 (entry 0 the stub, 1-16 the module's unrolled instances, 17-63 one generic loop);
//  - every span directly, the original (its trampoline) against the hooked address, on fake 24/32-bit rows
//    (140 pixels between guards) with 5-6-5 or 24-bit source rows: every entry with its own count and counts
//    the unrolled ones ignore, the generic loops with every count 0-16 (not 0 for the do-while ones), 17-63
//    and above; destination and source 0-3 pixels into the row; random bytes and bytes of the level's
//    sprites; values with garbage in their upper byte; alpha bytes random, edge values and a sprite's alpha
//    and edge bytes; every alpha 0-255; every opacity through the 256 real fade tables and through tables of
//    random dwords (weights above 0xff carry into the next channel);
//  - every blitter by its exe address and through CFrame::vf62 with copies of the live modes (FindBlitter),
//    side 0 with the module's hooks out: plain sources and destinations of 8, 16, 24 and 32 bits with pitch
//    padding, synthetic 24-bit RLE frames (3 bytes per pixel, any of the five blocks) and the level's 16-bit
//    sprites (16R -> 32), whole and cut rectangles (the RLE drawers' full and clipped paths), opacity 1, 0,
//    0.5, the 20-80 % levels, random and NaN. Besides the frames and the call log the globals the blitters
//    write (g_OpacityTable, g_Blend8Table) are compared and put back after each side.
// The 8-bit sources get a 64 KB noise table as the mode's palette data: a fake frame has no palette, and the
// opaque path would read the source's (from address 0 when it has none: R1).
#include "checks_frame_r1.h"
#include "blit24.h"

namespace zax {

int hooks_blit24_spans32(Hook *out);
int hooks_blit24_spans(Hook *out);
int hooks_blit24_spans_op(Hook *out);
int hooks_blit24(Hook *out);
int hooks_blit24_tm(Hook *out);

namespace {

using namespace framec;
using r1::rnd;
using r1::rnd_range;

ModuleHooks g_spans32{"blit24_spans32", &hooks_blit24_spans32, {}, {}, {}, 0, true};
ModuleHooks g_spans{"blit24_spans", &hooks_blit24_spans, {}, {}, {}, 0, true};
ModuleHooks g_spans_op{"blit24_spans_op", &hooks_blit24_spans_op, {}, {}, {}, 0, true};
ModuleHooks g_blit{"blit24", &hooks_blit24, {}, {}, {}, 0, true};
ModuleHooks g_tm{"blit24_tm", &hooks_blit24_tm, {}, {}, {}, 0, true};
ModuleHooks g_none{"blit24 (direct)", [](Hook *) { return 0; }, {}, {}, {}, 0, true};
Sources g_sources;
bool g_sources_ready, g_evidence_logged;

int read_ini(const char *key)
{
    char ini[MAX_PATH + 32];
    GetModuleFileNameA(NULL, ini, MAX_PATH);
    for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
        ini[n - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    return (int)GetPrivateProfileIntA("Reimpl", key, 0, ini);
}

DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// ---- The evidence --------------------------------------------------------------------------------------------------

constexpr const char *COUNTED_NAMES[render::blit::NCOUNTED] = {
    "Copy 24->24", "Copy 32->24", "Copy 32->32", "24R->24 (TM, Black Bias, Amplify)", "Copy 8->24", "Copy 8->32", "Copy 16->24",
    "Copy 16->32", "Copy 24->8", "Copy 24->16", "Copy 24->32", "Copy 16R->32", "Normal Scale 24->24", "Normal Scale 24->32",
    "ScreenCopy 24->16", "TileWrap 24->24", "Tile 24->24", "TM 8->8", "TM 8->16", "TM 8->24", "TM 8->32", "TM 16->16", "TM 16->24",
    "TM 24R->32", "32->24 pixel blend"};

void log_evidence()
{
    if (g_evidence_logged)
        return;
    g_evidence_logged = true;
    log("  R6 evidence: calls of the batch's blitters before its checks (by the game, unless an earlier module's check drew):");
    DWORD total = 0;
    for (int i = 0; i < render::blit::NCOUNTED; i++) {
        total += render::blit::g_counted_calls[i];
        if (render::blit::g_counted_calls[i])
            log("    %x %s: %u", render::blit::COUNTED[i], COUNTED_NAMES[i], render::blit::g_counted_calls[i]);
    }
    log("    all others 0; %u calls in all", total);
    DWORD depth[5][SRC_KINDS] = {}, rle[5] = {};
    each_source([&](const FrameData *f, int kind) {
        const DWORD d = f->flags & F_DEPTH;
        const int i = d == F_8 ? 0 : d == F_16 ? 1 : d == F_24 ? 2 : d == F_32 ? 3 : 4;
        depth[i][kind]++;
        if (f->flags & F_COMPRESSED)
            rle[i]++;
    });
    static const char *const names[5] = {"8", "16", "24", "32", "other"};
    for (int i = 0; i < 5; i++)
        log("    level frames of %s bits: sprites %u, glyphs %u, images %u, textures %u, world %u (%u compressed)", names[i], depth[i][SRC_SPRITE],
            depth[i][SRC_FONT], depth[i][SRC_IMAGE], depth[i][SRC_TEXTURE], depth[i][SRC_WORLD], rle[i]);
}

void prepare()
{
    r1::read_frame_trace();
    log_evidence();
    if (!g_sources_ready) {
        collect_sources(g_sources, 12);
        g_sources_ready = true;
    }
    render::g_span24_mutation = read_ini("Span24Mutation");
}

// ---- Spans ---------------------------------------------------------------------------------------------------------

// The kinds by table: T24 + k
enum { K_C32, K_F32, K_AC32, K_AF32, K_C24, K_F24, K_AC24, K_AF24, K_OC24, K_OF24, K_AOC24, K_AOF24, KINDS };
bool takes_alpha(int k) { return k == K_AC32 || k == K_AF32 || k == K_AC24 || k == K_AF24 || k == K_AOC24 || k == K_AOF24; }
bool src_pointer(int k) { return k == K_C32 || k == K_AC32 || k == K_C24 || k == K_AC24 || k == K_OC24 || k == K_AOC24; }
bool alpha_pointer(int k) { return k == K_AC32 || k == K_AC24 || k == K_AOC24; }
bool reads_opacity(int k) { return k >= K_OC24; }
bool do_while(int k) { return k == K_AC24 || k == K_OC24 || k == K_AOC24; }
DWORD stub_of(int k)
{
    static const DWORD stubs[KINDS] = {0x401110, 0x401110, 0x4f0a80, 0x4f0a80, 0x401110, 0x45e730,
                                       0x4f0a80, 0x463e10, 0x401110, 0x45e730, 0x4f0a80, 0x463e10};
    return stubs[k];
}

struct SpanFn {
    DWORD address;
    int kind, n;   // n 0: a generic loop; -1: an entry-0 stub of the module
    const char *name;
};
struct SpanList {
    SpanFn list[80];
    int n = 0;
};

const char *hook_name(int (*table)(Hook *), DWORD address)
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

// The tables first..last checked and their spans listed (and the module's entry-0 stubs)
void check_tables(Tally &t, int (*table)(Hook *), int first, int last, SpanList &out)
{
    for (int k = first; k <= last; k++) {
        const int kind = k - 24;
        const DWORD *e = at<const DWORD *>(0x60cd90 + (DWORD)k * 0x100);
        char d[120];
        wsprintfA(d, "[T%d[0] %x]", k, e[0]);
        t.check(e[0] == stub_of(kind), "", d);
        if (const char *stub = hook_name(table, e[0])) {
            bool listed = false;
            for (int i = 0; i < out.n; i++)
                listed = listed || out.list[i].address == e[0];
            if (!listed && out.n < 80)
                out.list[out.n++] = {e[0], kind, -1, stub};
        }
        for (int n = 1; n <= 16; n++) {
            const char *name = hook_name(table, e[n]);
            wsprintfA(d, "[T%d[%d] %x: not a hook of the module]", k, n, e[n]);
            t.check(name != nullptr, "", d);
            if (name && out.n < 80)
                out.list[out.n++] = {e[n], kind, n, name};
        }
        const char *generic = hook_name(table, e[17]);
        bool same = generic != nullptr;
        for (int n = 18; n < 64; n++)
            same = same && e[n] == e[17];
        wsprintfA(d, "[T%d[17..63] %x: not one hooked generic loop]", k, e[17]);
        t.check(same, "", d);
        if (generic && out.n < 80)
            out.list[out.n++] = {e[17], kind, 0, generic};
    }
}

constexpr int ROW = 140;
constexpr BYTE EDGE_ALPHA[] = {0, 1, 2, 3, 4, 5, 7, 8, 63, 64, 127, 128, 129, 191, 192, 251, 252, 253, 254, 255};

// A real 16-bit sprite of the level (for its bytes), or null
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
bool block_bytes(const FrameData *f, int b, BYTE *out, DWORD bytes)
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
    const SpanFn *f;
    DWORD n;
    int dst_x, src_x;
    bool real_src, real_dst, real_alpha;
    int alpha_mode;   // 0 random, 1 one value for all (alpha_value), 2 edge values mixed
    int opacity;      // 0..255 a real fade table, -1 random dwords, -2 random bytes
    DWORD value, alpha_value;
};

void run_direct(Tally &t, const DirectCase &c)
{
    begin_case(random());
    const int k = c.f->kind;
    FrameSpec s;
    s.width = ROW;
    s.height = 2;
    s.format = k < K_C24 ? RGB32 : RGB24;
    const int dst = make_frame(s);
    s.format = k < K_C24 ? RGB565 : RGB24;
    const int src = make_frame(s);
    const DWORD alpha = alloc_object(ROW), table = c.opacity < 0 ? alloc_object(0x400) : 0;
    if (dst < 0 || src < 0 || alpha == 0xffffffffu || table == 0xffffffffu)
        return;
    BYTE bytes[ROW * 4];
    if (c.real_src && block_bytes(pick_sprite(), 0, bytes, ROW * format_bytes(s.format)))
        for (int side = 0; side < 2; side++)
            memcpy(frame(side, src)->pixels, bytes, ROW * format_bytes(s.format));
    if (c.real_dst && block_bytes(pick_sprite(), 0, bytes, ROW * (k < K_C24 ? 4 : 3)))
        for (int side = 0; side < 2; side++)
            memcpy(frame(side, dst)->pixels, bytes, ROW * (k < K_C24 ? 4 : 3));
    BYTE a[ROW];
    for (int i = 0; i < ROW; i++)
        a[i] = (BYTE)(random() >> 13);
    if (c.alpha_mode == 1)
        memset(a, (BYTE)c.alpha_value, sizeof a);
    else if (c.alpha_mode == 2)
        for (int i = 0; i < ROW; i++)
            a[i] = EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)];
    if (c.real_alpha) {
        const FrameData *f = pick_sprite();
        if (!block_bytes(f, 4, a, sizeof a))
            block_bytes(f, 3, a, sizeof a);
    }
    for (int side = 0; side < 2; side++) {
        memcpy(object<BYTE>(side, alpha), a, sizeof a);
        if (c.opacity < 0) {
            DWORD *tb = object<DWORD>(side, table);
            for (int i = 0; i < 256; i++)
                tb[i] = side ? object<DWORD>(0, table)[i] : c.opacity == -2 ? random() & 0xff : rnd(4) ? random() >> rnd(32) : random() & 0x1ff;
        }
    }
    char what[110];
    wsprintfA(what, "%s n %u dst+%d src+%d op %d value %x", c.f->name, c.n, c.dst_x, c.src_x, c.opacity, c.value);
    const DWORD saved_table = *at<DWORD *>(render::OPACITY_TABLE);
    const DWORD db = k < K_C24 ? 4 : 3, sb = k < K_C24 ? 2 : 3;
    run_sides(t, g_none, what, [&](int side) -> DWORD {
        BYTE *d = frame(side, dst)->pixels + c.dst_x * db;
        const DWORD sv = src_pointer(k) ? ptrv(frame(side, src)->pixels + c.src_x * sb) : c.value;
        const DWORD av = alpha_pointer(k) ? ptrv(object<BYTE>(side, alpha)) : c.alpha_value;
        *at<DWORD *>(render::OPACITY_TABLE) = c.opacity < 0 ? ptrv(object<DWORD>(side, table)) : render::FADE_TABLES + (DWORD)c.opacity * 0x400;
        const DWORD fn = side ? c.f->address : ptrv(original_address(c.f->address));
        if (takes_alpha(k) || (c.f->n < 0 && stub_of(k) == 0x463e10))
            reinterpret_cast<render::AlphaSpan24>(fn)(d, sv, av, c.n);
        else
            reinterpret_cast<render::Span24>(fn)(d, sv, c.n);
        *at<DWORD *>(render::OPACITY_TABLE) = saved_table;
        return 0;
    });
}

DirectCase random_case(const SpanFn &f, DWORD n)
{
    DirectCase c;
    c.f = &f;
    c.n = n;
    c.dst_x = (int)rnd(4);
    c.src_x = (int)rnd(4);
    c.real_src = rnd(3) == 0;
    c.real_dst = rnd(3) == 0;
    c.real_alpha = rnd(4) == 0;
    c.alpha_mode = (int)rnd(3);
    c.opacity = rnd(5) ? (int)rnd(256) : rnd(2) ? -1 : -2;
    c.value = random();
    c.alpha_value = rnd(3) ? random() : EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)] | (rnd(2) ? random() & 0xffffff00u : 0);
    return c;
}

// Every span of the list with its own and other counts; `per` random cases each
void check_direct(Tally &t, const SpanList &spans, int per)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (f.n < 0) {
            for (int k = 0; k < per; k++)
                run_direct(t, random_case(f, rnd(70)));
            continue;
        }
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

// Every opacity (the 256 fade tables) and every alpha value for entries 1, 7, 16 and the generic loops
void check_weights(Tally &t, const SpanList &spans)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (f.n != 0 && f.n != 1 && f.n != 7 && f.n != 16)
            continue;
        const DWORD n = f.n ? (DWORD)f.n : 17 + rnd(47);
        if (reads_opacity(f.kind))
            for (int op = 0; op < 256; op++) {
                DirectCase c = random_case(f, n);
                c.opacity = op;
                run_direct(t, c);
            }
        if (takes_alpha(f.kind))
            for (DWORD a = 0; a < 256; a++) {
                DirectCase c = random_case(f, n);
                c.real_alpha = false;
                c.alpha_mode = 1;
                c.alpha_value = a | (alpha_pointer(f.kind) || rnd(2) ? 0 : random() & 0xffffff00u);
                run_direct(t, c);
            }
    }
}

// ---- Blitters -------------------------------------------------------------------------------------------------------

using Blit = void(__fastcall *)(void *src, const void *sr, void *dst, const void *dr, void *mode);
constexpr DWORD WATCHED[][2] = {{render::OPACITY_TABLE, 4}, {render::blit::BLEND8_TABLE, 4}};

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

// A row of RLE codes covering exactly w pixels (ps bytes per pixel)
BYTE *exact_row(BYTE *p, int w, DWORD ps, unsigned &s)
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
        } else if (k < 13) {
            *p++ = (BYTE)(0x40 | n);
            for (DWORD i = 0; i < (DWORD)n * ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
        } else {
            *p++ = (BYTE)n;
            for (DWORD i = 0; i < ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
        }
        x += n;
    }
    return p;
}

// A compressed frame of w x h (flags) with the blocks of `mask` (bit 0 pixels, 1 +0x1c, 2 +0x24, 3 alpha,
// 4 edge), `ps` bytes per pixel; the alpha block gets raw bytes (one per pixel of its row, plus room)
int make_rle(DWORD flags, int w, int h, DWORD ps, DWORD mask, unsigned seed)
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
            if (b == 3)
                for (int i = 0; i < w + 8; i++)
                    *p++ = (BYTE)(pattern_next(s) % 5 == 0 ? (pattern_next(s) & 1) * 255 : pattern_next(s) >> 9);
            else
                p = exact_row(p, w, ps, s);
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

constexpr float OPACITIES[] = {1.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.2f, 0.4f, 0.6f, 0.8f, 0.1f};
BYTE *g_noise_table;   // 64 KB of noise: the palette data of the modes for 8-bit sources

// A copy of a live mode, its opacity set on both sides (SetOpacity 0x433be0; NaN written directly), the noise
// table as its palette data when `noise`
DWORD mode_copy(DWORD live, bool noise, DWORD *op_out)
{
    const DWORD off = alloc_object(0x48);
    if (off == 0xffffffffu)
        return off;
    const int kind = rnd(4) ? (int)rnd(sizeof OPACITIES / sizeof OPACITIES[0]) : -1;
    const float op = kind < 0 ? (float)rnd(256) / 255.0f : OPACITIES[kind];
    for (int side = 0; side < 2; side++) {
        memcpy(object<void>(side, off), at<void *>(live), sizeof(render::CBlendMode));
        at<void(THISCALL *)(void *, DWORD)>(0x433be0)(object<void>(side, off), fbits(op));
        if (noise)
            object<render::CBlendMode>(side, off)->palette.data = g_noise_table;
    }
    if (rnd(40) == 0)
        for (int side = 0; side < 2; side++)
            object<render::CBlendMode>(side, off)->opacity = raw_float(0x7fc00000u);
    *op_out = fbits(object<render::CBlendMode>(0, off)->opacity);
    return off;
}

struct Rects {
    DWORD s, d;
    framec::Rect sr, dr;
};
bool store_rects(Rects &r)
{
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
// Rectangles of equal size inside the source and the destination; `full` takes the source's whole width
bool make_rects(Rects &r, int sw, int sh, int dw, int dh, bool full)
{
    const int w = sw < dw ? sw : dw, h = sh < dh ? sh : dh;
    if (w < 1 || h < 1 || (full && sw > dw))
        return false;
    const int cw = full ? sw : rnd_range(1, w), ch = rnd(3) ? rnd_range(1, h) : h;
    const int sl = full ? 0 : rnd_range(0, sw - cw), st = rnd_range(0, sh - ch);
    const int dl = rnd_range(0, dw - cw), dt = rnd_range(0, dh - ch);
    r.sr = {sl, st, sl + cw, st + ch};
    r.dr = {dl, dt, dl + cw, dt + ch};
    return store_rects(r);
}
int make_dst(Format f, int min_w, int min_h)
{
    FrameSpec d;
    d.format = f;
    d.width = rnd_range(min_w, min_w + 40 < 120 ? min_w + 40 : 120);
    d.height = rnd_range(min_h, min_h + 10 < 60 ? min_h + 10 : 60);
    if (rnd(3) == 0)
        d.pitch = d.width + rnd_range(1, 7);
    return make_frame(d);
}

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
        const unsigned h = watched_hash(side);
        g.saved.restore();
        return h;
    });
}

// ---- Plain blits ----------------------------------------------------------------------------------------------------

constexpr DWORD COPY = 0x6bb4e0, TM = 0x6bcc68, BLACK_BIAS = 0x6b9728, AMPLIFY = 0x6b85f0, SCALE = 0x6bb520, SCREEN = 0x6bbaa0, TILE = 0x6bc4e0,
                TILE_WRAP = 0x6bc4a8;
struct PlainKind {
    DWORD fn, mode;
    const char *name;
    Format src, dst;
    int shape;   // 0 same-size rectangles, 1 Tile, 2 TileWrap, 3 Normal Scale
};
constexpr PlainKind BLIT_KINDS[] = {
    {0x434cf0, COPY, "Copy 24->24", RGB24, RGB24, 0},          {0x434f50, COPY, "Copy 32->24", RGB32, RGB24, 0},
    {0x435300, COPY, "Copy 32->32", RGB32, RGB32, 0},          {0x43f070, COPY, "Copy 8->24", PAL8, RGB24, 0},
    {0x43f300, COPY, "Copy 8->32", PAL8, RGB32, 0},            {0x43f780, COPY, "Copy 16->24", RGB565, RGB24, 0},
    {0x43f9b0, COPY, "Copy 16->32", RGB565, RGB32, 0},         {0x43fbf0, COPY, "Copy 24->8", RGB24, PAL8, 0},
    {0x43fe50, COPY, "Copy 24->16", RGB24, RGB565, 0},         {0x4400b0, COPY, "Copy 24->32", RGB24, RGB32, 0},
    {0x443d70, SCREEN, "ScreenCopy 24->16", RGB24, RGB555, 0}, {0x4427c0, SCALE, "Normal Scale 24->24", RGB24, RGB24, 3},
    {0x442bf0, SCALE, "Normal Scale 24->32", RGB24, RGB32, 3}, {0x4467f0, TILE, "Tile 24->24", RGB24, RGB24, 1},
    {0x4463d0, TILE_WRAP, "TileWrap 24->24", RGB24, RGB24, 2}, {0, COPY, "Copy vf62 24->24", RGB24, RGB24, 0},
    {0, COPY, "Copy vf62 32->24", RGB32, RGB24, 0},            {0, BLACK_BIAS, "Black Bias vf62 32->32", RGB32, RGB32, 0},
    {0, AMPLIFY, "Amplify vf62 16->32", RGB565, RGB32, 0},     {0, COPY, "Copy vf62 8->32", PAL8, RGB32, 0},
    {0, SCALE, "Normal Scale vf62 24->32", RGB24, RGB32, 3},
};
constexpr PlainKind TM_KINDS[] = {
    {0x464fe0, TM, "TM 8->8", PAL8, PAL8, 0},           {0x4651d0, TM, "TM 8->16", PAL8, RGB565, 0},
    {0x4653d0, TM, "TM 8->24", PAL8, RGB24, 0},         {0x465660, TM, "TM 8->32", PAL8, RGB32, 0},
    {0x4658d0, TM, "TM 16->16", RGB565, RGB565, 0},     {0x465ae0, TM, "TM 16->24", RGB565, RGB24, 0},
    {0, TM, "TM vf62 16->16", RGB565, RGB565, 0},       {0, TM, "TM vf62 8->24", PAL8, RGB24, 0},
    {0, TM, "TM vf62 8->16", PAL8, RGB565, 0},
};

// Source pixels with many zeros (Transparent Mask skips index / word 0) and alpha bytes at the edges (32 -> 24)
void shape_pixels(int src, Format f)
{
    const DWORD bytes = g_h.frames[src].bytes;
    BYTE *p0 = frame(0, src)->pixels, *p1 = frame(1, src)->pixels;
    const DWORD px = format_bytes(f);
    for (DWORD i = 0; i + px <= bytes; i += px) {
        const DWORD r = rnd(6);
        if (r == 0)
            memset(p0 + i, 0, px);
        if (f == RGB32 && r >= 4)
            p0[i + 3] = (BYTE)(r == 4 ? EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)] : rnd(2) ? 0xff : 0);
        memcpy(p1 + i, p0 + i, px);
    }
}

void check_plain(Tally &t, ModuleHooks &hooks, const PlainKind *kinds, int nkinds, int cases)
{
    Globals g;
    const bool inverse = *at<DWORD *>(0x633388) != 0;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const PlainKind &kind = kinds[(DWORD)k % (DWORD)nkinds];
        if (kind.fn == 0x43fbf0 && !inverse)
            continue;
        FrameSpec s;
        s.format = kind.src;
        if (kind.shape == 1) {
            s.width = 1 << rnd_range(0, 6);
            s.height = 1 << rnd_range(0, 4);
        } else {
            s.width = rnd_range(1, kind.shape ? 50 : 80);
            s.height = rnd_range(1, 24);
        }
        if (rnd(3) == 0)
            s.pitch = s.width + rnd_range(1, 9);
        const int src = make_frame(s);
        const int dst = make_dst(kind.dst, kind.shape ? 1 : s.width / 2 + 1, kind.shape ? 1 : s.height / 2 + 1);
        DWORD op;
        const DWORD mode = mode_copy(kind.mode, kind.src == PAL8, &op);
        if (src < 0 || dst < 0 || mode == 0xffffffffu)
            continue;
        shape_pixels(src, kind.src);
        const int dw = frame(0, dst)->width, dh = frame(0, dst)->height;
        Rects r;
        if (kind.shape == 0) {
            if (!make_rects(r, s.width, s.height, dw, dh, false))
                continue;
        } else {
            const int cw = rnd_range(1, dw), ch = rnd_range(1, dh), dl = rnd_range(0, dw - cw), dt = rnd_range(0, dh - ch);
            r.dr = {dl, dt, dl + cw, dt + ch};
            const int sl = kind.shape == 2 ? rnd_range(0, s.width - 1) : kind.shape == 3 ? (rnd(3) ? 0 : rnd_range(0, 3)) : rnd_range(0, 200);
            const int st = kind.shape == 2 ? rnd_range(0, s.height - 1) : kind.shape == 3 ? 0 : rnd_range(0, 200);
            r.sr = {sl, st, s.width, s.height};
            if (kind.shape == 3 && rnd(12) == 0)
                r.dr = rnd(2) ? framec::Rect{dl, dt, dl, dt + ch} : framec::Rect{dl - 1, dt, dl + cw + rnd_range(0, 3), dt + ch};
            if (!store_rects(r))
                continue;
        }
        char what[150];
        wsprintfA(what, "%s %dx%d (%d,%d) -> %dx%d (%d,%d %dx%d) op %x", kind.name, s.width, s.height, r.sr.left, r.sr.top, dw, dh, r.dr.left,
                  r.dr.top, r.dr.right - r.dr.left, r.dr.bottom - r.dr.top, op);
        run_blit(t, hooks, g, what, kind.fn, src, r, dst, mode);
    }
}

// ---- RLE blits ------------------------------------------------------------------------------------------------------

struct RleKind {
    DWORD fn, mode;
    const char *name;
    bool src24;   // 24-bit RLE (synthetic), else 16-bit (sprites of the level and synthetic)
    Format dst;
};
constexpr RleKind RLE_KINDS[] = {
    {0x435cb0, TM, "TM 24R->24", true, RGB24},          {0x435cb0, BLACK_BIAS, "Black Bias 24R->24", true, RGB24},
    {0x435cb0, AMPLIFY, "Amplify 24R->24", true, RGB24}, {0x440390, COPY, "Copy 16R->32", false, RGB32},
    {0, COPY, "Copy vf62 16R->32", false, RGB32},       {0, TM, "TM vf62 24R->24", true, RGB24},
    {0, BLACK_BIAS, "Black Bias vf62 24R->24", true, RGB24},
};
constexpr RleKind RLE_TM_KINDS[] = {
    {0x465d10, TM, "TM 24R->32", true, RGB32},
    {0, TM, "TM vf62 24R->32", true, RGB32},
};

constexpr RleKind RLE_ALL24[] = {
    {0x435cb0, TM, "TM 24R->24", true, RGB24}, {0x435cb0, BLACK_BIAS, "Black Bias 24R->24", true, RGB24}, {0x465d10, TM, "TM 24R->32", true, RGB32},
};

int check_rle(Tally &t, ModuleHooks &hooks, const RleKind *kinds, int nkinds, int cases)
{
    Globals g;
    int real_used = 0;
    for (int k = 0; k < cases; k++) {
        begin_case(random());
        const RleKind &kind = kinds[(DWORD)k % (DWORD)nkinds];
        const FrameData *real = !kind.src24 && rnd(2) ? pick_sprite() : nullptr;
        int src;
        if (real) {
            src = copy_compressed(real);
            real_used++;
        } else {
            const DWORD mask = 1 | (rnd(3) == 0 ? 0 : 0x18) | (rnd(3) == 0 ? 0x6 : 0);
            src = make_rle((kind.src24 ? F_24 : F_16) | render::F_RLE16, rnd_range(1, 100), rnd_range(1, 24), kind.src24 ? 3 : 2, mask, random());
        }
        if (src < 0)
            continue;
        const int sw = frame(0, src)->width, sh = frame(0, src)->height;
        const bool full = rnd(2) != 0;
        const int dst = make_dst(kind.dst, full ? sw : sw / 2 > 1 ? sw / 2 : 1, sh / 2 > 1 ? sh / 2 : 1);
        DWORD op;
        const DWORD mode = mode_copy(kind.mode, false, &op);
        if (dst < 0 || mode == 0xffffffffu)
            continue;
        Rects r;
        if (!make_rects(r, sw, sh, frame(0, dst)->width, frame(0, dst)->height, full))
            continue;
        char what[150];
        wsprintfA(what, "%s %s %dx%d (%d,%d %dx%d) -> %dx%d at %d,%d op %x", kind.name, real ? "real" : "synthetic", sw, sh, r.sr.left, r.sr.top,
                  r.sr.right - r.sr.left, r.sr.bottom - r.sr.top, frame(0, dst)->width, frame(0, dst)->height, r.dr.left, r.dr.top, op);
        run_blit(t, hooks, g, what, kind.fn, src, r, dst, mode);
    }
    return real_used;
}

// The spans of a module: tables, directly, every weight, and through the RLE blitters with the module's hooks out
void check_spans(const char *module, ModuleHooks &hooks, int first, int last, const RleKind *kinds, int nkinds)
{
    char title[100];
    wsprintfA(title, "%s: the span tables T%d-T%d", module, first, last);
    Tally tables{title};
    SpanList spans;
    check_tables(tables, hooks.table, first, last, spans);
    tables.report();
    wsprintfA(title, "%s: every span directly (original against reimplementation)", module);
    Tally direct{title};
    check_direct(direct, spans, scaled(6));
    direct.report();
    wsprintfA(title, "%s: every alpha and opacity", module);
    Tally weights{title};
    check_weights(weights, spans);
    weights.report();
    wsprintfA(title, "%s: through the RLE blitters", module);
    Tally blits{title};
    check_rle(blits, hooks, kinds, nkinds, scaled(400));
    blits.report();
}

void noise()
{
    if (!g_noise_table) {
        g_noise_table = static_cast<BYTE *>(heap_alloc(0x10000));
        for (DWORD i = 0; i < 0x10000; i++)
            g_noise_table[i] = (BYTE)(random() >> 11);
    }
}

}  // namespace

ZAX_CHECKS(blit24_spans32)
{
    (void)cls;
    (void)n;
    prepare();
    g_seed = 0x45d2c0;
    check_spans("blit24_spans32", g_spans32, 24, 27, RLE_KINDS + 3, 2);
}

ZAX_CHECKS(blit24_spans)
{
    (void)cls;
    (void)n;
    prepare();
    g_seed = 0x45e450;
    check_spans("blit24_spans", g_spans, 28, 31, RLE_ALL24, sizeof RLE_ALL24 / sizeof RLE_ALL24[0]);
}

ZAX_CHECKS(blit24_spans_op)
{
    (void)cls;
    (void)n;
    prepare();
    g_seed = 0x460ac0;
    check_spans("blit24_spans_op", g_spans_op, 32, 35, RLE_ALL24, sizeof RLE_ALL24 / sizeof RLE_ALL24[0]);
}

ZAX_CHECKS(blit24)
{
    (void)cls;
    (void)n;
    prepare();
    noise();
    g_seed = 0x434cf0;
    Tally plain{"blit24: plain blitters (Copy x->24/32 and 24/32->x, the 32->24 blend, Normal Scale, Tile, TileWrap, ScreenCopy 24->16)"};
    check_plain(plain, g_blit, BLIT_KINDS, sizeof BLIT_KINDS / sizeof BLIT_KINDS[0], scaled(2100));
    plain.report();
    Tally rle{"blit24: RLE blitters, drawers and rows (24R->24, 16R->32)"};
    const int real = check_rle(rle, g_blit, RLE_KINDS, sizeof RLE_KINDS / sizeof RLE_KINDS[0], scaled(1400));
    char d[60];
    wsprintfA(d, "[no 16-bit sprite among the sources]");
    rle.check(real > 0, "", d);
    rle.report();
}

ZAX_CHECKS(blit24_tm)
{
    (void)cls;
    (void)n;
    prepare();
    noise();
    g_seed = 0x464fe0;
    Tally plain{"blit24_tm: Transparent Mask's raw blitters 8->8/16/24/32, 16->16/24"};
    check_plain(plain, g_tm, TM_KINDS, sizeof TM_KINDS / sizeof TM_KINDS[0], scaled(1800));
    plain.report();
    Tally rle{"blit24_tm: Transparent Mask 24R->32 with its drawers and rows"};
    check_rle(rle, g_tm, RLE_TM_KINDS, sizeof RLE_TM_KINDS / sizeof RLE_TM_KINDS[0], scaled(600));
    rle.report();
}

}  // namespace zax
