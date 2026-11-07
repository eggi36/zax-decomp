// Checks of spans8.cpp, in the game thread, one module per hook table (spans8: T0, T1, the mode table and
// 8 -> 16 spans; spans8_ds: T2-T5; spans8_sd: T6-T9), each:
//  - The span tables: entry 0 of T0-T9 the no-op (`ret 4`), entries 1-16 the unrolled instances, 17-63 the
//    generic loop; the 64 entries of the dynamic tables of Amplify Light, Black Bias and Transparent Mask
//    their one function. The expected functions come from the module's hook names.
//  - Every function on its own, the original (its trampoline) against the hooked address, on random
//    buffers and on bytes of the level's frames (plain rows and RLE blocks), with the destination and the
//    source at every alignment, odd counts, n ignored by the unrolled entries, upper bytes in the run
//    value, and the tables real or random (the 20/80 and 40/60 tables, the mode table, its row tables, the
//    16-bit palette; a missing table is a random one); destination and source between guard bands.
//  - Every entry 1-63 of the module's span tables called through the table (some of every dynamic table),
//    with the module's hooks out on side 0.
//  - 8-bit RLE blits through FindBlitter (CFrame::vf62 and the clipping vf23) of synthetic frames (literal
//    and run codes of 1-63 pixels, with and without an edge block, zero-length codes only where the drawer
//    reaches T0-T9, whose entry 0 is a no-op: the mode table and 8 -> 16 drawers go through dynamic tables
//    whose entry 0 is the loop, 2^32 pixels) and of the level's 8-bit RLE frames: Copy, Transparent
//    Mask, Black Bias, Test Light and Amplify Light to 8 bits, Copy and Transparent Mask to 16 bits, at the
//    opacities 1, 0.2, 0.4, 0.6, 0.8 and others; the RLE drawers stay the exe's (R5), so the spans run in
//    their real callers. The blitters' globals (the drawers' table pointers 0x630330-0x630357, the mode
//    table pointer, the opacity table) are put back afterwards.
#include "checks_frame_r1.h"
#include "spans8.h"

namespace zax {

int hooks_spans8(Hook *out);
int hooks_spans8_ds(Hook *out);
int hooks_spans8_sd(Hook *out);

namespace {

using namespace r1;
using render::Span;
// The span table T k (64 entries each, one every 0x100 bytes from 0x60cd90)
inline DWORD span_table(int k) { return 0x60cd90 + (DWORD)k * 0x100; }

ModuleHooks g_hooks8{"spans8", &hooks_spans8, {}, {}, {}, 0, true};
ModuleHooks g_hooks_ds{"spans8_ds", &hooks_spans8_ds, {}, {}, {}, 0, true};
ModuleHooks g_hooks_sd{"spans8_sd", &hooks_spans8_sd, {}, {}, {}, 0, true};

Sources g_sources;

template <typename R, typename... A> R exe(DWORD address, A... a) { return at<R(THISCALL *)(A...)>(address)(a...); }

bool same_text(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
DWORD lo8(DWORD v) { return v & 0xff; }

bool starts_with(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++)
            return false;
    return true;
}

// ---- The module's functions, from its hook names ------------------------------------------------------------------

const char *const TABLE_PREFIX[10] = {"Span8_Copy_",           "Span8_Fill_",           "Span8_Alpha2080DS_Copy_", "Span8_Alpha2080DS_Fill_",
                                      "Span8_Alpha4060DS_Copy_", "Span8_Alpha4060DS_Fill_", "Span8_Alpha4060SD_Copy_", "Span8_Alpha4060SD_Fill_",
                                      "Span8_Alpha2080SD_Copy_", "Span8_Alpha2080SD_Fill_"};

struct Fn {
    DWORD address;
    const char *name;
    int table;        // T0-T9, -1 for the others
    int count;        // the unrolled entry's N, 0 for a loop over n
    bool fill, to16, zero_ok;
};

struct FnList {
    Fn list[ModuleHooks::MAX];
    int n = 0;
};

void list_functions(ModuleHooks &m, FnList &out)
{
    Hook hooks[ModuleHooks::MAX];
    const int count = m.table(nullptr);
    out.n = 0;
    if (count > ModuleHooks::MAX)
        return;
    m.table(hooks);
    for (int i = 0; i < count; i++) {
        Fn f = {hooks[i].address, hooks[i].name, -1, 0, false, false, false};
        const char *s = f.name;
        for (int k = 0; k < 10; k++)
            if (starts_with(s, TABLE_PREFIX[k])) {
                f.table = k;
                const char *num = s + lstrlenA(TABLE_PREFIX[k]);
                while (*num >= '0' && *num <= '9')
                    f.count = f.count * 10 + (*num++ - '0');
            }
        if (f.table >= 0) {
            f.fill = (f.table & 1) != 0;
        } else {   // the other run spans end in "_Fill"
            const int len = lstrlenA(s);
            f.fill = len >= 5 && same_text(s + len - 5, "_Fill");
        }
        f.to16 = starts_with(s, "Span8To16_");
        // loops that test the count first: T0/T1's generic loops and the 8 -> 16 run
        f.zero_ok = (f.table == 0 || f.table == 1) ? f.count == 0 : f.to16 && f.fill;
        out.list[out.n++] = f;
    }
}

const Fn *find_fn(const FnList &l, int table, int count)
{
    for (int i = 0; i < l.n; i++)
        if (l.list[i].table == table && l.list[i].count == count)
            return &l.list[i];
    return nullptr;
}
const Fn *find_name(const FnList &l, const char *name)
{
    for (int i = 0; i < l.n; i++)
        if (same_text(l.list[i].name, name))
            return &l.list[i];
    return nullptr;
}

// The dynamic tables: Amplify Light and Black Bias (their constructors 0x433df0, 0x4386f0) and Transparent Mask
struct DynTable {
    DWORD table;
    const char *fn;
};
constexpr DynTable DYN_TABLES[] = {
    {0x6b79f0, "Span8_ModeTable_Copy"},  {0x6b7bf0, "Span8_ModeTable_Fill"},  {0x6b77f0, "Span8_ModeTableA_Copy"},
    {0x6b71f0, "Span8_ModeTableA_Fill"}, {0x6b9128, "Span8_ModeTableB_Copy"}, {0x6b8628, "Span8_ModeTableB_Fill"},
    {0x6b8728, "Span8_ModeTableC_Copy"}, {0x6b72f0, "Span8_ModeTableC_Fill"}, {0x6b73f0, "Span8_ModeTableD_Copy"},
    {0x6b8828, "Span8_ModeTableD_Fill"},  // Amplify Light
    {0x6b9428, "Span8_ModeTable_Copy"},  {0x6b9a5c, "Span8_ModeTable_Fill"},  {0x6b9b5c, "Span8_ModeTableA_Copy"},
    {0x6b975c, "Span8_ModeTableA_Fill"}, {0x6b9628, "Span8_ModeTableB_Copy"}, {0x6b9228, "Span8_ModeTableB_Fill"},
    {0x6b985c, "Span8_ModeTableC_Copy"}, {0x6b995c, "Span8_ModeTableC_Fill"}, {0x6b9328, "Span8_ModeTableD_Copy"},
    {0x6b9528, "Span8_ModeTableD_Fill"},  // Black Bias (and Test Light)
    {render::SPAN8TO16_COPY_TABLE, "Span8To16_Copy"}, {render::SPAN8TO16_FILL_TABLE, "Span8To16_Fill"},
};

// ---- The tables the spans read: real or random ---------------------------------------------------------------------

constexpr DWORD ROW_TABLES[4] = {render::MODE_ROWS_A, render::MODE_ROWS_B, render::MODE_ROWS_C, render::MODE_ROWS_D};
constexpr DWORD DRAWER_GLOBALS = 0x630330, DRAWER_GLOBALS_BYTES = 0x28;
constexpr DWORD BLACK_BIAS_MODE = 0x6b9728, TEST_LIGHT_MODE = 0x6b9c60, AMPLIFY_MODE = 0x6b85f0, TRANSPARENT_MASK_MODE = 0x6bcc68;

struct Tables {
    SavedMemory *saved;         // run_module's
    BYTE *noise[3];             // 64 KB each: 20/80, 40/60, the mode table
    BYTE *real[3];              // the live tables (null: none)
    bool real_rows_fit[4];      // every entry + 255 inside a 64 KB table
    DWORD real_rows[4][256], real_palette16[256];
    void begin()
    {
        saved->add(render::ALPHA20_80, 4);
        saved->add(render::ALPHA40_60, 4);
        saved->add(render::BLEND8_TABLE, 4);
        saved->add(render::OPACITY_TABLE, 4);
        saved->add(DRAWER_GLOBALS, DRAWER_GLOBALS_BYTES);
        for (DWORD r : ROW_TABLES)
            saved->add(r, 0x400);
        saved->add(render::PALETTE16_DWORDS, 0x400);
        for (int i = 0; i < 3; i++) {
            noise[i] = static_cast<BYTE *>(heap_alloc(0x10000));
            fill_pattern(noise[i], 0x10000, 0x5a0000u + (unsigned)i);
        }
        real[0] = *at<BYTE **>(render::ALPHA20_80);
        real[1] = *at<BYTE **>(render::ALPHA40_60);
        real[2] = at<render::CBlendMode *>(BLACK_BIAS_MODE)->palette.data;
        if (!real[2])
            real[2] = at<render::CBlendMode *>(AMPLIFY_MODE)->palette.data;
        for (int k = 0; k < 4; k++) {
            memcpy(real_rows[k], at<void *>(ROW_TABLES[k]), 0x400);
            real_rows_fit[k] = true;
            for (DWORD v : real_rows[k])
                real_rows_fit[k] = real_rows_fit[k] && v <= 0xff00;
        }
        memcpy(real_palette16, at<void *>(render::PALETTE16_DWORDS), 0x400);
        log("  tables: 20/80 %x, 40/60 %x, mode table %x, row tables in range %d%d%d%d", ptrv(real[0]), ptrv(real[1]), ptrv(real[2]),
            real_rows_fit[0], real_rows_fit[1], real_rows_fit[2], real_rows_fit[3]);
    }
    // variant 0: the live tables (random where there is none), 1: random tables, rows and palette
    void set(int variant)
    {
        const bool rnd_all = variant == 1;
        *at<BYTE **>(render::ALPHA20_80) = rnd_all || !real[0] ? noise[0] : real[0];
        *at<BYTE **>(render::ALPHA40_60) = rnd_all || !real[1] ? noise[1] : real[1];
        *at<BYTE **>(render::BLEND8_TABLE) = rnd_all || !real[2] ? noise[2] : real[2];
        for (int k = 0; k < 4; k++) {
            DWORD *rows = at<DWORD *>(ROW_TABLES[k]);
            if (rnd_all || !real_rows_fit[k])
                for (int i = 0; i < 256; i++)
                    rows[i] = rnd(0xff01);
            else
                memcpy(rows, real_rows[k], 0x400);
        }
        DWORD *pal = at<DWORD *>(render::PALETTE16_DWORDS);
        if (rnd_all)
            for (int i = 0; i < 256; i++)
                pal[i] = random();
        else
            memcpy(pal, real_palette16, 0x400);
    }
    void end()
    {
        saved->release();
        for (BYTE *&r : noise) {
            heap_free(r);
            r = nullptr;
        }
    }
};
Tables g_tables_data;   // global: 5 KB, no stack probes
Tables *g_tables = &g_tables_data;

// ---- Cases ---------------------------------------------------------------------------------------------------------

BYTE *buffer(int side, int i) { return g_h.arena[side].pixels + g_h.frames[i].buffer; }

// Bytes of a level frame (a plain frame's rows, a compressed frame's pixel block) into frame i on both sides
bool copy_real_bytes(int i, DWORD offset, DWORD n)
{
    if (!g_sources.n)
        return false;
    const FrameData *f = g_sources.list[rnd((DWORD)g_sources.n)].frame;
    const BYTE *p;
    DWORD avail;
    if (f->flags & F_COMPRESSED) {
        if (!compressed_sane(f))
            return false;
        p = block_data(f, 0);
        avail = *reinterpret_cast<const DWORD *>(p);
    } else {
        if (!geo::readable(f, sizeof *f) || !f->rows || !geo::readable(f->rows, 8))
            return false;
        p = f->pixels;
        avail = (f->rows[f->height - 1] + f->width) * ((f->flags & F_DEPTH) == F_8 ? 1u : 2u);
        if (!geo::readable(p, avail))
            return false;
    }
    if (!avail)
        return false;
    const DWORD start = rnd(avail);
    for (int side = 0; side < 2; side++)
        for (DWORD k = 0; k < n; k++)
            buffer(side, i)[offset + k] = p[(start + k) % avail];
    return true;
}

// Both sides through call(side) without putting hooks out (side 0 calls the trampolines); compares all pixel
// memory of the case
template <typename F> bool run_direct(Tally &t, const char *what, F call)
{
    SideResult r[2];
    for (int side = 0; side < 2; side++) {
        call(side);
        state(side, r[side]);
    }
    const bool same = r[0].pixels == r[1].pixels;
    char d[160];
    wsprintfA(d, "[%.110s: pixels %x/%x]", what, r[0].pixels, r[1].pixels);
    const int before = t.mismatches;
    t.check(same, "", d);
    if (!same && before < 10)
        log_pixel_differences();
    return same;
}

// One call of span f with n (the count for the buffers: the unrolled N or n): destination and source buffers
// with room on both sides; through_table: the address `fn_address` (a table entry) with the module's hooks
// out on side 0
void span_case(Tally &t, ModuleHooks *hooks, const Fn &f, DWORD fn_address, DWORD n, const char *how)
{
    begin_case(random());
    const DWORD count = f.count ? (DWORD)f.count : n;
    FrameSpec d;
    d.format = f.to16 ? RGB565 : PAL8;
    d.width = (int)count + 8;
    d.height = 1;
    d.recording = false;
    FrameSpec s = d;
    s.format = PAL8;
    const int di = make_frame(d), si = make_frame(s);
    if (di < 0 || si < 0)
        return;
    const DWORD ad = rnd(4), as = rnd(4);
    const int content = (int)rnd(4);
    if (content == 1)
        copy_real_bytes(si, 0, count + 8);
    if (content == 2) {
        copy_real_bytes(si, 0, count + 8);
        copy_real_bytes(di, 0, (count + 8) * (f.to16 ? 2 : 1));
    }
    if (content == 3)   // the extremes
        for (int side = 0; side < 2; side++)
            for (DWORD k = 0; k < count + 8; k++)
                buffer(side, si)[k] = (BYTE)((k & 1) ? 0xff : 0);
    const DWORD value = rnd(3) ? random() : rnd(2) ? 0xffffff00u | rnd(256) : rnd(256);
    const int variant = (int)rnd(2);
    g_tables->set(variant);
    const DWORD n_arg = f.count && rnd(3) == 0 ? rnd(64) : n;
    char what[120];
    wsprintfA(what, "%s %s n %u (%u) align %u/%u tables %s %s", f.name, how, n_arg, count, ad, as, variant ? "random" : "live",
              content == 0 ? "pattern" : content == 3 ? "0/255" : "level bytes");
    auto call = [&](int side) -> DWORD {
        BYTE *dp = buffer(side, di) + ad * (f.to16 ? 2 : 1), *sp = buffer(side, si) + as;
        const DWORD arg = f.fill ? value : ptrv(sp);
        if (hooks)
            at<Span>(fn_address)(dp, arg, n_arg);
        else
            (side ? at<Span>(f.address) : original<Span>(f.address))(dp, arg, n_arg);
        return 0;
    };
    if (g_frame_trace)
        log("  case %s", what);
    if (hooks)
        run_sides(t, *hooks, what, call);
    else
        run_direct(t, what, call);
}

// A count for a loop over n: mostly the generic entries' 17-63, also 1-16, wider and 0 where allowed
DWORD loop_count(const Fn &f)
{
    switch (rnd(8)) {
    case 0: return 1 + rnd(16);
    case 1: return 64 + rnd(240);
    case 2: return f.zero_ok && rnd(2) ? 0 : 1 + rnd(3);
    default: return 17 + rnd(47);
    }
}

void check_tables(Tally &t, const FnList &fns)
{
    for (int k = 0; k < 10; k++) {
        if (!find_fn(fns, k, 1))
            continue;
        const DWORD *e = at<const DWORD *>(span_table(k));
        const BYTE *nop = at<const BYTE *>(e[0]);
        char d[96];
        wsprintfA(d, "[T%d entry 0 %x]", k, e[0]);
        // (the toolkit's shared no-op 0x401110, hooked since batch C3: a jump to its reimplementation there)
        t.check(e[0] == 0x401110 || (nop[0] == 0xc2 && nop[1] == 4 && nop[2] == 0), "", d);
        for (int i = 1; i < (int)render::SPAN_ENTRIES; i++) {
            const Fn *f = find_fn(fns, k, i <= (int)render::SPAN_UNROLLED ? i : 0);
            wsprintfA(d, "[T%d entry %d %x, expected %x]", k, i, e[i], f ? f->address : 0);
            t.check(f && e[i] == f->address, "", d);
        }
    }
    for (const DynTable &dt : DYN_TABLES) {
        const Fn *f = find_name(fns, dt.fn);
        if (!f)
            continue;
        const DWORD *e = at<const DWORD *>(dt.table);
        for (DWORD i = 0; i < render::SPAN_ENTRIES; i++) {
            char d[96];
            wsprintfA(d, "[table %x entry %u %x, expected %x (%s)]", dt.table, i, e[i], f->address, dt.fn);
            t.check(e[i] == f->address, "", d);
        }
    }
}

void check_direct(Tally &t, const FnList &fns)
{
    for (int i = 0; i < fns.n; i++) {
        const Fn &f = fns.list[i];
        const int cases = f.count ? scaled(60) : scaled(f.table >= 0 ? 600 : 400);
        for (int c = 0; c < cases; c++)
            span_case(t, nullptr, f, f.address, f.count ? (DWORD)f.count : loop_count(f), "direct");
    }
}

constexpr int DYN_ENTRIES[] = {1, 2, 3, 4, 7, 16, 17, 31, 63};   // the entries of the dynamic tables called

void check_through_tables(Tally &t, ModuleHooks &hooks, const FnList &fns)
{
    for (int k = 0; k < 10; k++) {
        if (!find_fn(fns, k, 1))
            continue;
        const DWORD *e = at<const DWORD *>(span_table(k));
        for (int i = 1; i < (int)render::SPAN_ENTRIES; i++) {
            const Fn *f = find_fn(fns, k, i <= (int)render::SPAN_UNROLLED ? i : 0);
            if (f)
                span_case(t, &hooks, *f, e[i], (DWORD)i, "table");
        }
    }
    for (const DynTable &dt : DYN_TABLES) {
        const Fn *f = find_name(fns, dt.fn);
        if (!f)
            continue;
        for (int i : DYN_ENTRIES)
            span_case(t, &hooks, *f, at<const DWORD *>(dt.table)[i], (DWORD)i, "dynamic table");
    }
}

// ---- 8-bit RLE blits through FindBlitter ---------------------------------------------------------------------------

// One row of codes: skip 0x80 | n, literal 0x40 | n and n bytes, run n and its byte; n 1-63 (skips up to 40),
// zero-length codes when allowed, some rows starting with a 63-pixel segment
BYTE *rle8_row(BYTE *p, int w, unsigned &s, bool zero_codes)
{
    int x = 0;
    if (w > 0x3f && pattern_next(s) % 5 == 0) {
        const bool lit = pattern_next(s) & 1;
        *p++ = (BYTE)(lit ? 0x7f : 0x3f);
        for (DWORD i = 0; i < (lit ? 0x3fu : 1u); i++)
            *p++ = (BYTE)(pattern_next(s) >> 9);
        x = 0x3f;
    }
    while (x < w) {
        const DWORD k = pattern_next(s) % 20;
        const DWORD n = k < 10 ? 1 + pattern_next(s) % 16 : 1 + pattern_next(s) % 63;
        if (k % 3 == 0) {
            *p++ = (BYTE)(0x80 | (n < 40 ? n : 40));
            x += (int)(n < 40 ? n : 40);
        } else if (k % 3 == 1) {
            *p++ = (BYTE)(0x40 | n);
            for (DWORD i = 0; i < n; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
            x += (int)n;
        } else {
            *p++ = (BYTE)n;
            *p++ = (BYTE)(pattern_next(s) >> 9);
            x += (int)n;
        }
        if (zero_codes && pattern_next(s) % 16 == 0) {
            const bool lit = pattern_next(s) & 1;
            *p++ = lit ? 0x40 : 0;
            if (!lit)
                *p++ = (BYTE)(pattern_next(s) >> 9);
        }
    }
    return p;
}

// An 8-bit RLE frame (flags 0x22) of w x h: the pixel block, optionally the edge block (+0x34/+0x38) in the same
// codes; both sides. Returns the frame index or -1.
int make_rle8_frame(int w, int h, bool edge, bool zero_codes, unsigned seed)
{
    const int nb = edge ? 2 : 1;
    const DWORD worst = (DWORD)nb * ((DWORD)h * (DWORD)(w + 0x40) * 3 + 8) + 64;
    if (g_h.nframes >= (int)MAX_FRAMES || g_h.next_object + sizeof(FrameData) > ARENA_OBJECTS || g_h.next_rows + (DWORD)(nb * h) > ARENA_ROWS ||
        g_h.next_pixels + GUARD * 2 + worst + 16 > ARENA_PIXELS || h < 1)
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
    f->flags = F_8 | render::F_RLE8;
    unsigned s = seed * 2654435761u + 23;
    if (!s)
        s = 1;
    for (int b = 0; b < nb; b++) {
        BYTE *data = p;
        p += 4;
        for (int y = 0; y < h; y++) {
            table[y] = (DWORD)(p - data);
            p = rle8_row(p, w, s, zero_codes);
        }
        *reinterpret_cast<DWORD *>(data) = (DWORD)(p - data);
        while ((p - base) & 3)
            *p++ = 0;
        if (b == 0) {
            f->pixels = data;
            f->rows = table;
        } else {
            f->blocks[6] = data;
            f->blocks[7] = table;
        }
        table += h;
    }
    const DWORD bytes = (DWORD)(p - (base + GUARD));
    const DWORD total = (GUARD + bytes + GUARD + 15) & ~15u;
    fill_pattern(base, GUARD, seed * 31u + 1);
    fill_pattern(base + GUARD + bytes, total - GUARD - bytes, seed * 31u + 2);
    FrameInfo &fi = g_h.frames[g_h.nframes];
    fi = {g_h.next_object, start + GUARD, bytes, bytes, 1, 1};
    memcpy(a1.pixels + start, base, total);
    memcpy(a1.rows + g_h.next_rows, a0.rows + g_h.next_rows, (DWORD)(nb * h) * 4);
    FrameData *f1 = reinterpret_cast<FrameData *>(a1.objects + g_h.next_object);
    *f1 = *f;
    const DWORD dp = ptrv(a1.pixels) - ptrv(a0.pixels), dr = ptrv(a1.rows) - ptrv(a0.rows);
    f1->pixels = f->pixels + dp;
    f1->rows = reinterpret_cast<DWORD *>(ptrv(f->rows) + dr);
    if (edge) {
        f1->blocks[6] = static_cast<BYTE *>(f->blocks[6]) + dp;
        f1->blocks[7] = reinterpret_cast<void *>(ptrv(f->blocks[7]) + dr);
    }
    g_h.next_object += sizeof(FrameData);
    g_h.next_rows += (DWORD)(nb * h);
    g_h.next_pixels += total;
    return g_h.nframes++;
}

struct BlitMode {
    DWORD object;
    const char *name;
    bool to16, mode_table;
    // the drawers reach these spans through dynamic tables whose entry 0 is the loop (2^32 pixels for n = 0):
    // no zero-length literal or run codes
    bool no_zero() const { return to16 || mode_table; }
};
constexpr BlitMode BLIT_MODES[] = {
    {render::COPY_MODE, "Copy", false, false},      {TRANSPARENT_MASK_MODE, "Transparent Mask", false, false},
    {BLACK_BIAS_MODE, "Black Bias", false, true},   {TEST_LIGHT_MODE, "Test Light", false, true},
    {AMPLIFY_MODE, "Amplify Light", false, true},   {render::COPY_MODE, "Copy", true, false},
    {TRANSPARENT_MASK_MODE, "Transparent Mask", true, false},
};
constexpr float OPACITIES[] = {1.0f, 0.2f, 0.4f, 0.6f, 0.8f};

// Real 8-bit RLE frames of the level without zero-length codes in their pixel and edge blocks (for the mode
// table modes): parsed with the drawers' codes
constexpr int RLE8_BLOCKS[2] = {0, 4};   // the pixel and the edge block
bool no_zero_codes(const FrameData *f)
{
    for (int b : RLE8_BLOCKS) {
        const BYTE *d = block_data(f, b);
        const DWORD *tb = block_table(f, b);
        if (!d || !tb)
            continue;
        const DWORD size = *reinterpret_cast<const DWORD *>(d);
        for (DWORD y = 0; y < f->height; y++) {
            DWORD o = tb[y];
            for (int x = 0; x < f->width && o < size;) {
                const BYTE c = d[o++];
                if (c & 0x80)
                    x += c & 0x7f;
                else if (!(c & 0x3f))
                    return false;
                else {
                    x += c & 0x3f;
                    o += c & 0x40 ? (DWORD)(c & 0x3f) : 1u;
                }
            }
        }
    }
    return true;
}

void check_blits(Tally &t, ModuleHooks &hooks)
{
    int real8 = 0;
    for (int i = 0; i < g_sources.n; i++)
        real8 += (g_sources.list[i].frame->flags & (F_DEPTH | render::F_RLE8)) == (F_8 | render::F_RLE8);
    log("  sources: %d picked, %d of them 8-bit RLE frames", g_sources.n, real8);
    for (int k = 0; k < scaled(400); k++) {
        if (g_frame_trace > 1)
            log("  blit setup %d", k);
        begin_case(random());
        const BlitMode &bm = BLIT_MODES[rnd(sizeof BLIT_MODES / sizeof BLIT_MODES[0])];
        int src = -1, sw_ = 0, sh = 0;
        bool real = false;
        if (real8 && k % 4 == 3) {
            const Source &so = g_sources.list[rnd((DWORD)g_sources.n)];
            const FrameData *f = so.frame;
            if ((f->flags & (F_DEPTH | render::F_RLE8)) == (F_8 | render::F_RLE8) && compressed_sane(f) && (!bm.no_zero() || no_zero_codes(f))) {
                src = copy_compressed(f);
                sw_ = f->width;
                sh = f->height;
                real = src >= 0;
            }
        }
        if (src < 0) {
            sw_ = rnd_range(1, 100);
            sh = rnd_range(1, 12);
            src = make_rle8_frame(sw_, sh, rnd(2) != 0, !bm.no_zero(), random());
        }
        if (src < 0)
            continue;
        frame(0, src)->x = frame(1, src)->x = 0;
        frame(0, src)->y = frame(1, src)->y = 0;
        FrameSpec d;
        d.format = bm.to16 ? RGB565 : PAL8;
        d.width = rnd_range(1, 110);
        d.height = rnd_range(1, 16);
        d.pitch = d.width + (rnd(3) ? 0 : rnd_range(1, 9));
        d.x = rnd(2) ? 0 : rnd_range(-10, 10);
        d.y = rnd(2) ? 0 : rnd_range(-10, 10);
        const int dst = make_frame(d);
        if (dst < 0)
            continue;
        if (rnd(3) == 0)
            copy_real_bytes(dst, 0, g_h.frames[dst].bytes);
        const DWORD mode = copy_mode_object(bm.object);
        const float op = rnd(3) == 0 ? (float)rnd(256) / 255.0f : OPACITIES[rnd(sizeof OPACITIES / sizeof OPACITIES[0])];
        g_tables->set((int)rnd(2));
        for (int side = 0; side < 2; side++) {
            render::CBlendMode *m = object<render::CBlendMode>(side, mode);
            if (bm.mode_table && !m->palette.data)
                m->palette.data = g_tables->noise[2];
            exe<void>(0x433be0, m, bits(op));   // CBlendMode_SetOpacity
        }
        const DWORD srect = alloc_object(sizeof(Rect)), drect = alloc_object(sizeof(Rect));
        Rect sr;
        sr.left = rnd_range(0, sw_ - 1);
        sr.top = rnd_range(0, sh - 1);
        sr.right = rnd(3) ? sw_ : sr.left + rnd_range(1, sw_ - sr.left);
        sr.bottom = rnd(3) ? sh : sr.top + rnd_range(1, sh - sr.top);
        const int px = rnd_range(d.x - 20, d.x + d.width), py = rnd_range(d.y - 8, d.y + d.height);
        const Rect dr = {px, py, px + (sr.right - sr.left), py + (sr.bottom - sr.top)};
        const bool direct = k & 1;
        char what[120];
        wsprintfA(what, "%s %s %dx%d -> %s %dx%d opacity %u/255 %s", real ? "real" : "synthetic", bm.name, sw_, sh, format_name(d.format), d.width,
                  d.height, (DWORD)(op * 255.0f + 0.5f), direct ? "vf62" : "vf23");
        if (g_frame_trace)
            log("  blit case %d: %s src (%d,%d,%d,%d) dst (%d,%d,%d,%d)", k, what, sr.left, sr.top, sr.right, sr.bottom, dr.left, dr.top, dr.right,
                dr.bottom);
        for (int side = 0; side < 2; side++) {
            *object<Rect>(side, srect) = sr;
            *object<Rect>(side, drect) = dr;
        }
        if (g_frame_trace)
            log("    src flags %x edge %d, mode %x data %x", frame(0, src)->flags, frame(0, src)->blocks[6] != nullptr, bm.object,
                ptrv(object<render::CBlendMode>(0, mode)->palette.data));
        run_sides(t, hooks, what, [&](int side) -> DWORD {
            if (g_frame_trace > 1)
                log("    side %d", side);
            if (direct) {
                // slot 62 needs rectangles inside both frames
                Rect *s0 = object<Rect>(side, srect), *d0 = object<Rect>(side, drect);
                *d0 = {0, 0, sw_ < d.width ? sw_ : d.width, sh < d.height ? sh : d.height};
                *s0 = {0, 0, d0->right, d0->bottom};
                exe<void>(0x4a8f10, frame(side, dst), frame(side, src), s0, d0, object<void>(side, mode));
                return 0;
            }
            return lo8(exe<DWORD>(0x4a8f40, frame(side, dst), frame(side, src), object<Rect>(side, srect), object<Rect>(side, drect),
                                  object<void>(side, mode)));
        });
    }
}

void run_module(ModuleHooks &hooks, const char *title)
{
    read_frame_trace();
    collect_sources(g_sources, 10);
    FnList *fns = static_cast<FnList *>(heap_alloc(sizeof(FnList)));
    list_functions(hooks, *fns);
    SavedMemory saved;
    g_tables->saved = &saved;
    g_tables->begin();
    char name[96];
    wsprintfA(name, "%s: the span tables", title);
    Tally tables{name};
    check_tables(tables, *fns);
    tables.report();
    wsprintfA(name, "%s: every span on its own (buffers, level bytes)", title);
    Tally direct{name};
    check_direct(direct, *fns);
    direct.report();
    wsprintfA(name, "%s: the spans through their table entries", title);
    Tally through{name};
    check_through_tables(through, hooks, *fns);
    through.report();
    wsprintfA(name, "%s: 8-bit RLE blits through FindBlitter", title);
    Tally blits{name};
    check_blits(blits, hooks);
    blits.report();
    g_tables->end();
    heap_free(fns);
}

}  // namespace

ZAX_CHECKS(spans8)
{
    (void)cls;
    (void)n;
    g_seed = 0x449f50;
    run_module(g_hooks8, "spans8 T0/T1, mode tables, 8 -> 16");
}

ZAX_CHECKS(spans8_ds)
{
    (void)cls;
    (void)n;
    g_seed = 0x44a440;
    run_module(g_hooks_ds, "spans8 T2-T5 (20/80, 40/60 DS)");
}

ZAX_CHECKS(spans8_sd)
{
    (void)cls;
    (void)n;
    g_seed = 0x44b000;
    run_module(g_hooks_sd, "spans8 T6-T9 (40/60, 20/80 SD)");
}

}  // namespace zax
