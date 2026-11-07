// Checks of batch C3's grid and header slots (ui_c3_grid.cpp) against the original with the toolkit's harness
// (checks_ui_common.h) and the grid setup of checks_ui_grid.cpp: a fake grid (window 0), its fake header (window 1,
// the exe's pointer array of 0-6 columns in the block, titles as ZStrings), a fake CJScrollBar (window 2); the
// frame's clip setter, the font's drawing, the blitter, the drawing of the children and the window destructor as
// recording stand-ins, the frame a recording fake whose slot 36 gives a clip rectangle. The grid's drawing calls
// its row slots 77/78 with 4 and 3 arguments, which the toolkit's grid table does not know: there the grid is a
// "game object" fake with those argument counts.
#include "checks_ui_common.h"
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_c3_grid(Hook *out);

namespace {

using namespace uic;
using scr::object_args;

constinit ModuleHooks g_hooks{"ui_c3_grid", &hooks_ui_c3_grid};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x524f50, CV_THIS, 1, K_VALUE, "COffsetClippedProxyFrame_SetClip", "r"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"},
    {0x40b190, CV_THIS, 0, K_VALUE, "CJWindow::~CJWindow"},
    {0x4defd0, CV_THIS, 0, K_VALUE, "ZString_Dtor"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

int g_ncols;
char g_object_args[MAXSLOT + 1];

BYTE *make_column(Gen &g, int left)
{
    BYTE *c = xalloc(0x24);
    at_<int>(c, 0) = left;
    at_(c, 4) = g.chance(6) ? g.below(3) : g.below(200);
    at_<float>(c, 8) = g.chance(4) ? 0.0f : (float)g.below(1000) / 1000.0f;
    make_text(c + 0xc, TEXTS[g.below(8)]);
    at_(c, 0x10) = ptrv(real_font());
    at_(c, 0x14) = 0, c[0x18] = (BYTE)g.below(2), at_(c, 0x1c) = g.chance(6) ? g.next() : g.below(3), c[0x20] = (BYTE)g.below(2);
    return c;
}

// The frame's slot 36 writes the clip rectangle through its argument
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == C_CFRAME && slot == 36 && args[0]) {
        Gen &v = g_case.values;
        int *r = at<int *>(args[0]);
        r[0] = v.range(-20, 300), r[1] = v.range(-20, 400);
        r[2] = r[0] + v.range(-5, 500), r[3] = r[1] + v.range(-5, 600);
    }
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
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
    at_(h, 0x120) = g_ncols && g.chance(3) ? g.below((DWORD)g_ncols) : 0xffffffff;
    at_(h, 0x124) = g.chance(3) ? 0 : g.below(800);
    at_(h, 0x128) = g.below(8);
    at_(h, 0x12c) = g.next();
    h[0x131] = (BYTE)g.below(2), h[0x132] = (BYTE)g.below(2), h[0x133] = (BYTE)(g.chance(4) ? 1 : 0);
    at_(h, 0x134) = g_ncols && g.chance(2) ? g.below((DWORD)g_ncols + 1) : 0xffffffff;
    make_window(8, C_CALLBACK);
    make_window(9, C_CALLBACK);
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
    grid[0x128] = at_(grid, 0x11c) ? (BYTE)g.below(2) : 0, grid[0x129] = (BYTE)g.below(2);
    if (g.chance(2))
        make_scroll(grid, g.range(0, 100), g.range(-10, 400), g.range(0, 800), g.chance(4) ? 0 : g.range(0, 1500), g.chance(2), nullptr,
                    nullptr);
    if (g.chance(3))
        make_scroll(h, g.range(-10, 100), g.range(-10, 100), g.range(0, 800), g.range(0, 300), false, nullptr, nullptr);
    link_child(win(5), grid);
    link_child(win(5), h);
    set_kind_all(79, K_WINDOW_NN);
    case_global(0x62e510, g.chance(5) ? 1 : 0);
    g_case.effect = &effect;
    g_case.argspec[36][0] = A_IGNORE;   // (the frame's clip: a local of the caller the slot fills)
    g_self = grid;
}

// The grid as a game object (its row slots 77 and 78 with their arguments)
void s_draw(Gen &g)
{
    BYTE *grid = win(0);
    at_(grid, 0) = at_(make_window(15, C_OBJECT), 0);
    object_args(g_object_args, "21:2,38:1,57:1,77:4,78:3");
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    g_case.argspec[77][3] = A_RECT;
    at_(win(2), 0x160) = ptrv(win(3));   // the scroll bar tells another window than the grid
    g_case.argspec[78][2] = A_RECT;
    (void)g;
}
void s_wheel(Gen &g) { g_args[0] = (DWORD)g.range(-8, 8), g_args[1] = g.next(); }
int *point(Gen &g, BYTE *w)
{
    int *p = reinterpret_cast<int *>(xalloc(16));
    if (g.chance(2)) {
        p[0] = at_<int>(w, 0x14) + g.range(-3, 300), p[1] = at_<int>(w, 0x18) + g.range(-3, 300);
    } else {
        p[0] = g.range(-60, 700), p[1] = g.range(-60, 500);
    }
    return p;
}
void s_drag(Gen &g)
{
    g_args[0] = ptrv(point(g, g_self));
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-60, 60), d[1] = g.range(-60, 60);
    g_args[1] = ptrv(d);
    g_args[2] = g.chance(4) ? g.next() : g.below(8);
    g_args[3] = ptrv(xalloc(4));
    g_self[0x67] = (BYTE)g.below(4);
}
void s_click(Gen &g)
{
    g_args[0] = g.chance(3) ? g.below(3) : 0;
    g_args[1] = ptrv(point(g, g_self));
    g_args[2] = ptrv(xalloc(4));
}
void s_set_rect(Gen &g)
{
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 400), r[1] = g.range(-50, 300), r[2] = r[0] + g.range(-10, 500), r[3] = r[1] + g.range(-10, 400);
    g_args[0] = ptrv(r);
    if (g.chance(5))
        at_(g_self, 0x100) = 0;
}
void s_selection(Gen &g)
{
    g_args[0] = g.below(50);
    g_args[1] = ptrv(make_window(14, C_CFRAME));
    g_args[2] = ptrv(xalloc(16));
}
void s_key(Gen &g)
{
    const DWORD k[] = {5, 6, 7, 10, 13, 14, 9, 0x20};
    g_args[0] = g.chance(6) ? g.next() & 0x1ff : k[g.below(8)];
    if (g_args[0] == 0x1b)   // (Escape with a capture and no screen window: the original crashes, section 9)
        g_args[0] = 0x1c;
    if (at_(g_self, 0x118) == 0 && g_args[0] == 14)
        at_(g_self, 0x118) = 1;
}
void s_header_dtor(Gen &g)
{
    g_self = win(1);
    BYTE *h = g_self;
    const DWORD n = at_(h, 0x108);
    for (DWORD i = 0; i < n; i++)
        if (g.chance(3))
            at_(at<BYTE *>(at<DWORD *>(at_(h, 0x104))[i]), 0x14) = ptrv(make_window(10 + (int)(i % 4), C_CFRAME));
    if (n && g.chance(4))
        at<DWORD *>(at_(h, 0x104))[g.below(n)] = 0;
    g_args[0] = g.below(4);
}
void s_header_draw(Gen &g)
{
    g_self = win(1);
    BYTE *h = g_self;
    const DWORD n = at_(h, 0x108);
    for (DWORD i = 0; i < n; i++)
        if (g.chance(3))
            at_(at<BYTE *>(at<DWORD *>(at_(h, 0x104))[i]), 0x14) = ptrv(make_window(10 + (int)(i % 4), C_CFRAME));
    set_kind(C_CFRAME, 39, K_SMALL);
    set_kind(C_CFRAME, 45, K_SMALL);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
}
void s_header_cursor(Gen &g)
{
    g_self = win(1);
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = g.range(-20, 900), p[1] = g.range(-20, 100);
    // near a divider now and then
    const DWORD n = at_(g_self, 0x108);
    if (n && g.chance(2)) {
        BYTE *c = at<BYTE *>(at<DWORD *>(at_(g_self, 0x104))[g.below(n)]);
        p[0] = at_<int>(c, 0) + at_<int>(c, 4) - 1 + g.range(-4, 4);
    }
    g_args[0] = ptrv(p);
}
void s_header_drag(Gen &g)
{
    g_self = win(1);
    s_drag(g);
    const DWORD n = at_(g_self, 0x108);
    if (n && g.chance(2)) {
        int *p = at<int *>(g_args[0]);
        BYTE *c = at<BYTE *>(at<DWORD *>(at_(g_self, 0x104))[g.below(n)]);
        p[0] = at_<int>(c, 0) + at_<int>(c, 4) - 1 + g.range(-4, 4) + at_<int>(g_self, 0x24);
    }
    // (a column being resized must exist: the original writes its width)
    if (at_(g_self, 0x120) != 0xffffffff && at_(g_self, 0x120) >= n)
        at_(g_self, 0x120) = 0xffffffff;
    if (g.chance(3))
        g_args[2] |= 1;
}
void s_header_up(Gen &g)
{
    g_self = win(1);
    g_args[0] = g.chance(3) ? g.below(3) : 0;
    g_args[1] = ptrv(point(g, g_self));
}

const Test TESTS[] = {
    {"Grid Draw", 0x4012c0, CALL_THIS, 1, 0, 400, s_draw, STUBS, NSTUBS},
    {"Grid Wheel", 0x401600, CALL_THIS, 2, ~0u, 150, s_wheel, STUBS, NSTUBS},
    {"Grid Drag", 0x401690, CALL_THIS, 4, ~0u, 300, s_drag, STUBS, NSTUBS},
    {"Grid Click", 0x401760, CALL_THIS, 3, 0xff, 200, s_click, STUBS, NSTUBS},
    {"Grid SetRect", 0x4017b0, CALL_THIS, 1, 0, 300, s_set_rect, STUBS, NSTUBS},
    {"Grid DrawSelection", 0x401ab0, CALL_THIS, 3, ~0u, 40, s_selection, STUBS, NSTUBS},
    {"Grid Key", 0x401ad0, CALL_THIS, 1, ~0u, 250, s_key, STUBS, NSTUBS},
    {"Header DeletingDtor", 0x402190, CALL_THIS, 1, ~0u, 150, s_header_dtor, STUBS, NSTUBS},
    {"Header Draw", 0x4022b0, CALL_THIS, 1, 0, 300, s_header_draw, STUBS, NSTUBS},
    {"Header Cursor", 0x4024f0, CALL_THIS, 1, ~0u, 150, s_header_cursor, STUBS, NSTUBS},
    {"Header Drag", 0x402560, CALL_THIS, 4, 0xff, 400, s_header_drag, STUBS, NSTUBS},
    {"Header MouseUp", 0x402c20, CALL_THIS, 2, ~0u, 200, s_header_up, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_c3_grid)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62e5c4, 1);
        g_globals.add(0x62e510, 1);
    }
    Tally t{"ui c3 grid: the grid control's and CJHeader's slots (drawing, wheel, drag, click, keys, the header's columns)"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 63500);
    t.report();
}

}  // namespace zax
