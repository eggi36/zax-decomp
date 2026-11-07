// Checks of the grid control and CJHeader (ui_grid.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): a fake grid (window 0) with a fake header (window 1: the exe's pointer array of 0-6
// columns in the block, titles as ZStrings), a fake CJScrollBar (window 2; the scroll bar's own functions run as
// they are on its fields), scroll infos; the constructors and destructors on real objects (the allocators as
// stand-ins).
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_grid(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_grid", &hooks_ui_grid};
Globals g_globals;

const Stub S_GRID[] = {
    UIC_ALLOC_STUBS,
};
constexpr int N_GRID = sizeof S_GRID / sizeof S_GRID[0];

int g_ncols;

BYTE *make_column(Gen &g, int left)
{
    BYTE *c = xalloc(0x24);
    at_<int>(c, 0) = left;
    at_(c, 4) = g.chance(6) ? g.below(3) : g.below(200);
    at_<float>(c, 8) = g.chance(4) ? 0.0f : (float)g.below(1000) / 1000.0f;
    make_text(c + 0xc, TEXTS[g.below(8)]);
    at_(c, 0x10) = ptrv(real_font());
    at_(c, 0x14) = g.next(), c[0x18] = (BYTE)g.below(2), at_(c, 0x1c) = g.next(), c[0x20] = (BYTE)g.below(2);
    return c;
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    // the header
    BYTE *h = make_plain_window(1, C_HEADER, g);
    g_ncols = (int)g.below(7);
    const DWORD cap = (DWORD)g_ncols + g.below(3);
    DWORD *cols = reinterpret_cast<DWORD *>(xalloc(4 * cap + 4));
    int left = 0;
    for (int i = 0; i < g_ncols; i++) {
        BYTE *c = make_column(g, left);
        cols[i] = ptrv(c);
        left += (int)at_(c, 4) + (g.chance(5) ? g.range(-3, 3) : 0);
    }
    at_(h, 0x100) = 0x5ea5dc;
    at_(h, 0x104) = ptrv(cols), at_(h, 0x108) = (DWORD)g_ncols, at_(h, 0x10c) = cap;
    for (int k = 0; k < 16; k++)
        h[0x110 + k] = (BYTE)g.next();
    at_(h, 0x124) = g.chance(3) ? 0 : g.below(800);
    make_window(8, C_CALLBACK);
    make_window(9, C_CALLBACK);
    make_window(10, C_CALLBACK);
    at_(h, 0x138) = g.chance(2) ? 0 : ptrv(win(8));
    at_(h, 0x13c) = g.chance(2) ? 0 : ptrv(win(9));
    // the scroll bar
    BYTE *bar = make_plain_window(2, C_SCROLLBAR, g);
    at_(bar, 0x140) = g.below(2);
    at_<float>(bar, 0x144) = 0.0f, at_<float>(bar, 0x148) = (float)g.below(1000), at_<float>(bar, 0x14c) = (float)g.range(-50, 1100);
    at_<float>(bar, 0x150) = 10.0f, at_<float>(bar, 0x154) = 100.0f;
    at_(bar, 0x158) = g.below(50), at_(bar, 0x15c) = g.below(2), at_(bar, 0x160) = ptrv(win(0));
    // the grid
    BYTE *grid = make_plain_window(0, C_GRID, g);
    at_(grid, 0x100) = ptrv(h);
    at_(grid, 0x114) = (DWORD)g.range(-20, 600);
    at_(grid, 0x118) = g.chance(5) ? 0 : g.below(60);
    at_(grid, 0x11c) = g.chance(3) ? 0 : ptrv(bar);
    at_(grid, 0x120) = 1 + g.below(30);
    at_(grid, 0x124) = g.chance(3) ? (DWORD)-1 : g.below(70);
    grid[0x128] = at_(grid, 0x11c) ? (BYTE)g.below(2) : 0, grid[0x129] = (BYTE)g.below(2);   // the caller's bar is there
    if (g.chance(2))
        make_scroll(grid, g.range(0, 100), g.range(-10, 400), g.range(0, 800), g.chance(4) ? 0 : g.range(0, 1500), g.chance(2), nullptr,
                    nullptr);
    set_kind_all(79, K_WINDOW_NN);   // slot 79 makes the grid's scroll bar
    g_self = grid;
}

void s_none(Gen &) {}
void s_header(Gen &) { g_self = win(1); }
void s_select(Gen &g)
{
    g_args[0] = g.chance(4) ? (DWORD)-1 : g.below(80);
    g_args[1] = g.below(2);
}
void s_rows(Gen &g) { g_args[0] = g.chance(5) ? 0 : g.below(80); }
void s_color(Gen &g)
{
    BYTE *c = xalloc(16);
    for (int i = 0; i < 16; i++)
        c[i] = (BYTE)g.next();
    g_args[0] = ptrv(c);
}
void s_header_color(Gen &g)
{
    s_color(g);
    g_self = win(1);
}
void s_set_bar(Gen &g)
{
    g_args[0] = g.chance(3) ? 0 : ptrv(win(3));
    g_args[1] = g.below(2);
}
void s_row_h(Gen &g) { g_args[0] = 1 + g.below(40); }
void s_notify(Gen &g)
{
    g_args[0] = g.chance(3) ? ptrv(win(4)) : at_(g_self, 0x11c);
    g_args[1] = g.chance(3) ? g.below(4) : 1;
}
void s_clamp(Gen &g)
{
    const int v[] = {(int)0x80000000, 0x7fffffff, -1, 0, 1, 5, 100};
    for (int i = 0; i < 3; i++)
        g_args[i] = g.chance(3) ? (DWORD)v[g.below(7)] : (DWORD)g.range(-100, 100);
}
void s_col_index(Gen &g)
{
    g_self = win(1);
    g_args[0] = g_ncols ? g.below((DWORD)g_ncols) : 0;
    if (!g_ncols)   // a column is needed: one
        at_(win(1), 0x108) = 1, at<DWORD *>(at_(win(1), 0x104))[0] = ptrv(make_column(g, 0));
}
void s_col_index_any(Gen &g)
{
    g_self = win(1);
    g_args[0] = g.chance(3) ? g.below(10) : g_ncols ? g.below((DWORD)g_ncols) : 0;
}
void s_col_width(Gen &g)
{
    s_col_index(g);
    g_args[1] = (DWORD)g.range(-5, 300);
}
void s_col_title(Gen &g)
{
    s_col_index(g);
    BYTE *z = xalloc(4);
    make_text(z, TEXTS[g.below(8)]);
    g_args[1] = ptrv(z);
}
void s_col_flag(Gen &g)
{
    s_col_index(g);
    g_args[1] = g.next();
}
void s_callback(Gen &g)
{
    g_self = win(1);
    g_args[0] = g.chance(3) ? 0 : ptrv(win(10));
}
void s_add_column(Gen &g)
{
    g_self = win(1);
    BYTE *z = xalloc(4);
    make_text(z, TEXTS[g.below(8)]);
    g_args[0] = ptrv(z);
    g_args[1] = g.below(300);
    g_args[2] = g.chance(2) ? 0 : ptrv(real_font());
    g_args[3] = g.next(), g_args[4] = g.next();
}
void s_header_width(Gen &g)
{
    g_self = win(1);
    g_args[0] = g.chance(6) ? g.below(5) : g.below(900);
    if (at_(g_self, 0xc) == at_(g_self, 4))   // the old width divides
        at_(g_self, 0xc) += 1 + g.below(100);
}

// Construction and destruction on real objects (the colours' index bytes are stack garbage: cleared)
void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void clear_garbage() { clear_colour_garbage(g_self); }
void s_grid_ctor(Gen &g)
{
    g_self = xalloc(0x140);
    g_args[0] = g.chance(4) ? 0 : ptrv(win(3));
    g_args[1] = ptrv(win(1));
}
void destroy_grid() { original<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x401290)(g_self, 0); }
void s_grid_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x140);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *)>(0x401000)(obj, win(3), win(1));
    clear_colour_garbage(obj);
    for (int k = 0; k < (int)g.below(3); k++)
        link_child(obj, win(4 + k));
    g_self = obj;
    g_args[0] = g.below(4);
}
void s_header_ctor(Gen &g)
{
    g_self = xalloc(0x140);
    g_args[0] = g.chance(4) ? 0 : ptrv(win(3));
}
void destroy_header() { original<void(THISCALL *)(BYTE *)>(0x40b190)(g_self); }

const Test TESTS[] = {
    {"Clamp", 0x402070, CALL_FAST, 3, ~0u, 300, s_clamp},
    {"Grid ctor", 0x401000, CALL_THIS, 2, ~0u, 150, s_grid_ctor, S_GRID, N_GRID, destroy_grid, clear_garbage},
    {"Grid HasChildren", 0x4011f0, CALL_THIS, 0, ~0u, 50, s_none},
    {"Grid deleting dtor", 0x401290, CALL_THIS, 1, ~0u, 150, s_grid_dtor, S_GRID, N_GRID},
    {"Grid Select", 0x401930, CALL_THIS, 2, 0, 600, s_select, S_GRID, N_GRID},
    {"Grid SetRows", 0x401bb0, CALL_THIS, 1, 0, 600, s_rows, S_GRID, N_GRID},
    {"Grid SetColor", 0x401ca0, CALL_THIS, 1, ~0u, 50, s_color},
    {"Grid UpdateBar", 0x401cf0, CALL_THIS, 0, 0, 400, s_none, S_GRID, N_GRID},
    {"Grid UpdateScroll", 0x401e30, CALL_THIS, 0, 0, 400, s_none, S_GRID, N_GRID},
    {"Grid MakeBar", 0x401ef0, CALL_THIS, 0, ~0u, 100, s_none, S_GRID, N_GRID},
    {"Grid SetBar", 0x401f20, CALL_THIS, 2, 0, 100, s_set_bar, S_GRID, N_GRID},
    {"Grid SetRowHeight", 0x401f70, CALL_THIS, 1, 0, 200, s_row_h, S_GRID, N_GRID},
    {"Grid Notify", 0x401f90, CALL_THIS, 2, 0, 400, s_notify, S_GRID, N_GRID},
    {"Header ctor", 0x4020d0, CALL_THIS, 1, ~0u, 150, s_header_ctor, S_GRID, N_GRID, destroy_header, clear_garbage},
    {"Header AddColumn", 0x402770, CALL_THIS, 5, 0, 400, s_add_column, S_GRID, N_GRID},
    {"Header SetColumnWidth", 0x402890, CALL_THIS, 2, 0, 300, s_col_width, S_GRID, N_GRID},
    {"Header ColumnWidth", 0x4028f0, CALL_THIS, 1, ~0u, 100, s_col_index_any},
    {"Header ColumnFlag", 0x402920, CALL_THIS, 1, ~0u, 100, s_col_index_any},
    {"Header ColumnLeft", 0x402950, CALL_THIS, 1, ~0u, 100, s_col_index},
    {"Header SetColumnTitle", 0x402970, CALL_THIS, 2, ~0u, 100, s_col_title, S_GRID, N_GRID},
    {"Header SetSortCallback", 0x402990, CALL_THIS, 1, 0, 100, s_callback},
    {"Header SetChangeCallback", 0x4029d0, CALL_THIS, 1, 0, 100, s_callback},
    {"Header SetWidth", 0x402a10, CALL_THIS, 1, 0, 600, s_header_width, S_GRID, N_GRID},
    {"Header SetColor", 0x402b40, CALL_THIS, 1, ~0u, 50, s_header_color},
    {"Header SetColumnFlag18", 0x402b90, CALL_THIS, 2, 0, 100, s_col_flag},
    {"Header Recalc", 0x402bb0, CALL_THIS, 0, ~0u, 300, s_header},
};

}  // namespace

ZAX_CHECKS(ui_grid)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        g_globals.add(0x62e5c4, 1);
    Tally t{"ui grid: the grid control and CJHeader"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 11300);
    t.report();
}

}  // namespace zax
