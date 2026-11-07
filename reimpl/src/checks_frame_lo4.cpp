// Checks of frame_lo4.cpp (the static switch's outline 0x4b3530 with its row helper 0x4b39a0, the hue remap effect's
// slot 29 0x43c8e0) against the original with the toolkit's harness (checks_ui_common.h): the frames are fake CFrames
// whose slots answer the case's size, depth, kind and RLE blocks (synthetic rows of skips, literal runs and fills
// that add up to the width) and record the drawing (slots 51 and 55); the RLE blocks, the remap table and the effect's
// saved copies lie in the extra area; the outline's row table 0x6be848 is set from the case and copied into the block
// after the call (compared there), its guard 0x6bf849 is a saved global, atexit a stand-in; the 16-bit remap's word
// table 0x6bb4d8 is a table of the check (set per case). The RLE helpers 0x43cce0, 0x43cd00 and 0x43cde0 are the
// exe's (or their batches' reimplementations).
#include "checks_ui_common.h"

namespace zax {

int hooks_frame_lo4(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"frame_lo4", &hooks_frame_lo4};
Globals g_globals;

const Stub STUBS[] = {
    {0x5d0863, CV_CDECL, 1, K_VALUE, "atexit"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr DWORD EDGES = 0x6be848, EDGES_GUARD = 0x6bf849, HUE_TABLE16 = 0x6bb4d8;
DWORD g_edges_saved[0x400];
BYTE *g_edges_copy;
WORD g_word_table[0x10000];

// The case's RLE rows: `rows` rows of `width` pixels with `bpp` bytes each (runs that add up to the width), their
// offsets in the row table; returns the data
struct Rle {
    BYTE *data;
    DWORD *rows;
    DWORD size;
};
Rle make_rle(Gen &g, DWORD width, DWORD rows, DWORD bpp, bool shared)
{
    Rle r{};
    r.rows = reinterpret_cast<DWORD *>(xalloc(4 * (rows + 1)));
    const DWORD distinct = shared ? 1 : rows;
    // a row: at most `width` codes of at most 1 + 63 * bpp bytes; keep the block small
    BYTE *buf = xalloc(0x1400);
    DWORD used = 0;
    for (DWORD k = 0; k < distinct && used < 0x1300; k++) {
        if (r.rows)
            r.rows[k] = used;
        DWORD left = width;
        while (left && used < 0x13a0) {
            const DWORD n = 1 + g.below(left < 12 ? left : 12);
            const DWORD kind = g.below(3);
            if (kind == 0) {
                buf[used++] = (BYTE)(0x80 | n);
            } else if (kind == 1) {
                buf[used++] = (BYTE)(0x40 | n);
                for (DWORD i = 0; i < n * bpp; i++)
                    buf[used++] = (BYTE)g.next();
            } else {
                buf[used++] = (BYTE)n;
                for (DWORD i = 0; i < bpp; i++)
                    buf[used++] = (BYTE)g.next();
            }
            left -= n;
        }
    }
    if (r.rows)
        for (DWORD k = distinct; k < rows; k++)
            r.rows[k] = r.rows[g.below(distinct)];
    r.data = buf;
    r.size = used;
    return r;
}

BYTE *g_dst;
void base(Gen &g)
{
    BYTE *dst = make_window(0, C_CFRAME);
    make_window(1, C_CFRAME);
    g_case.nwin = 1;
    set_kind(C_CFRAME, 43, K_SMALL);
    set_kind(C_CFRAME, 44, K_SMALL);
    set_kind(C_CFRAME, 29, K_FIXED0);
    set_kind(C_CFRAME, 39, K_FIXED1);
    set_kind(C_CFRAME, 38, K_FIXED2);
    set_kind(C_CFRAME, 45, K_FIXED3);
    for (int s = 31; s <= 35; s++)
        set_kind(C_CFRAME, s, K_FIXED4);
    for (int s = 46; s <= 50; s++)
        set_kind(C_CFRAME, s, K_FIXED7);
    g_dst = dst;
    g_edges_copy = xalloc(0x1000);
    // the row table: the game's saved one or a pattern
    DWORD *e = at<DWORD *>(EDGES);
    for (int i = 0; i < 0x400; i++)
        e[i] = g.chance(2) ? g_edges_saved[i] : g.below(40);
    case_global(EDGES_GUARD, g.chance(3) ? 0 : 1 + g.below(255));
}
void copy_edges() { memcpy(g_edges_copy, at<void *>(EDGES), 0x1000); }

// The source frame of the outline: depth, height (now and then above the table's 0x200 rows), kind, width, the
// blocks (each now and then none), one row table
void s_switch(Gen &g)
{
    const DWORD DEPTHS[6] = {8, 16, 24, 32, 15, 0};
    const DWORD depth = DEPTHS[g.below(6)];
    const DWORD bpp = depth == 16 ? 2 : depth == 24 ? 3 : depth == 32 ? 4 : 1;
    const DWORD height = g.chance(12) ? 0x200 + g.below(20) : g.below(30);
    const DWORD width = 1 + g.below(40);
    g_case.fixed[0] = depth;
    g_case.fixed[1] = height;
    g_case.fixed[2] = (g.next() & ~6u) | (g.below(4) << 1);
    g_case.fixed[3] = width;
    const Rle r = make_rle(g, width, height < 0x200 ? height : 0x200, bpp, height > 40);
    g_case.fixed[4] = g.chance(5) ? 0 : ptrv(r.data);
    g_case.fixed[7] = ptrv(r.rows);
    set_kind(C_CFRAME, 35, g.chance(4) ? K_ZERO : K_FIXED4);
    set_kind(C_CFRAME, 34, g.chance(4) ? K_ZERO : K_FIXED4);
    g_self = g_dst;
    g_args[0] = ptrv(win(1));
    g_args[1] = (DWORD)g.range(-50, 700);
    g_args[2] = (DWORD)g.range(-50, 500);
    g_args[3] = g.next();
}
// The row helper alone: a table of the extra area, the rows
void s_rows(Gen &g)
{
    const DWORD bpp = 1 + g.below(4), count = g.below(20), width = g.chance(10) ? 0 : 1 + g.below(40);
    const Rle r = make_rle(g, width ? width : 1, count, bpp, false);
    DWORD *t = reinterpret_cast<DWORD *>(xalloc(8 * (count + 1)));
    for (DWORD i = 0; i < 2 * count; i++)
        t[i] = g.chance(3) ? (i & 1 ? 0 : 999) : g.below(50);
    g_args[0] = ptrv(t);
    g_args[1] = ptrv(r.data);
    g_args[2] = ptrv(r.rows);
    g_args[3] = width;
    g_args[4] = count;
    g_args[5] = bpp;
}
// The effect's slot 29: kind 2 (8-bit blocks through the effect's table) or 4 (16-bit through the word table)
void s_remap(Gen &g)
{
    const DWORD kind = g.chance(8) ? g.below(4) << 1 : g.chance(2) ? 2 : 4;
    const DWORD bpp = kind == 4 ? 2 : 1;
    const DWORD width = 1 + g.below(30), height = g.below(12);
    g_case.fixed[1] = height;
    g_case.fixed[2] = (g.next() & ~6u) | kind;
    g_case.fixed[3] = width;
    const Rle r = make_rle(g, width, height, bpp, false);
    BYTE *dst = xalloc(r.size + 4);
    for (DWORD i = 0; i < r.size; i++)
        dst[i] = (BYTE)g.next();
    g_case.fixed[4] = ptrv(dst);
    g_case.fixed[7] = ptrv(r.rows);
    BYTE *e = xalloc(0x40);
    for (DWORD o = 0x10; o <= 0x20; o += 4)
        at_(e, o) = g.chance(5) ? 0 : ptrv(r.data);
    BYTE *table = xalloc(0x100);
    for (int i = 0; i < 0x100; i++)
        table[i] = (BYTE)g.next();
    at_(e, 0x30) = ptrv(table);
    for (DWORD i = 0; i < 0x10000; i++)
        g_word_table[i] = (WORD)(i * 2654435761u >> 7 ^ g_case.seed);
    case_global(HUE_TABLE16, ptrv(g_word_table));
    g_self = e;
    g_args[0] = ptrv(win(1));
}

constexpr DWORD GLOBALS4[] = {HUE_TABLE16};

const Test TESTS[] = {
    {"FrameDraw StaticSwitch", 0x4b3530, CALL_THIS, 4, 0, 1200, s_switch, STUBS, NSTUBS, nullptr, &copy_edges},
    {"StaticSwitch RowEdges", 0x4b39a0, CALL_FAST, 6, 0, 600, s_rows, STUBS, NSTUBS},
    {"HueRemapEffect vf29", 0x43c8e0, CALL_THIS, 1, 0, 800, s_remap, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(frame_lo4)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        g_globals.add(EDGES_GUARD, 1);
    }
    memcpy(g_edges_saved, at<void *>(EDGES), sizeof g_edges_saved);
    Tally t{"frame lo4: the static switch's outline, its row helper, the hue remap effect's slot 29"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 54000);
    memcpy(at<void *>(EDGES), g_edges_saved, sizeof g_edges_saved);
    t.report();
}

}  // namespace zax
