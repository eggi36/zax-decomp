// Checks of CJTabControl and the bevel (ui_tabs.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): a fake tab control (window 0) with 0-6 fake tab windows (children, their titles answered
// by slot 44 from a few texts, the real font for the widths), the arrays in the block (the allocators as
// stand-ins); the constructor and destructor on real objects; the bevel with the frame's fill recorded.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_tabs(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_tabs", &hooks_ui_tabs};
Globals g_globals;

const Stub S_TABS[] = {
    UIC_ALLOC_STUBS,
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
};
constexpr int N_TABS = sizeof S_TABS / sizeof S_TABS[0];
const Stub S_BEVEL[] = {{0x4b3ce0, CV_THIS, 5, K_VALUE, "Frame_FillRectChecked"}};

int g_ntabs;

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    BYTE *t = make_plain_window(0, C_TABS, g);
    t[0x100] = g.chance(4) ? 0 : 1;
    g_ntabs = (int)g.below(7);
    at_(t, 0x104) = (DWORD)g_ntabs;
    DWORD *tabs = reinterpret_cast<DWORD *>(xalloc(4 * (DWORD)g_ntabs + 4));
    for (int i = 0; i < g_ntabs; i++) {
        tabs[i] = ptrv(win(1 + i));
        link_child(t, win(1 + i));
        make_text(win(1 + i) + 0x50, TEXTS[g.below(8)]);
    }
    at_(t, 0x108) = g_ntabs || g.chance(2) ? ptrv(tabs) : 0;
    at_(t, 0x10c) = t[0x100] || g.chance(3) ? ptrv(xalloc(16 * (DWORD)g_ntabs + 16)) : 0;
    at_(t, 0x110) = g.below(30);
    at_(t, 0x114) = g_ntabs ? g.below((DWORD)g_ntabs) : 0;
    at_(t, 0x118) = g.next();
    set_kind_all(44, K_TEXT);
    g_self = t;
}

void s_none(Gen &) {}
void s_key(Gen &g)
{
    g_args[0] = g.chance(4) ? g.below(0x40) : 9;
    if (!g_ntabs)   // Ctrl+Tab without tabs reads before the array (the original as well): headers off
        g_self[0x100] = 0;
}
void s_flag(Gen &g) { g_args[0] = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_add(Gen &)
{
    g_args[0] = ptrv(win(10));
    if (!g_ntabs)   // the fake slot 31 links nothing: one child, so that first and last are set (the sizes)
        link_child(g_self, win(9));
}
void s_find(Gen &g) { g_args[0] = ptrv(TEXTS[g.below(8)]); }
void s_select(Gen &g)
{
    g_args[0] = g_ntabs ? g.below((DWORD)g_ntabs) : 0;
    g_args[1] = g.below(2);
    if (!g_ntabs)   // nothing to switch to: the current one
        g_args[0] = at_(g_self, 0x114);
}
void s_layout(Gen &)
{
    if (!at_(g_self, 0x10c))   // the layout writes the header rects (made with the headers)
        at_(g_self, 0x10c) = ptrv(xalloc(16 * (DWORD)g_ntabs + 16));
}
void s_bevel(Gen &g)
{
    // (a height beyond a few hundred pixels would draw height / 5 rings for minutes on both sides)
    g_args[0] = g.chance(3) ? ptrv(real_font()) : ptrv(fake_font(g.chance(5) ? g.below(300) : g.below(60)));
    g_args[1] = ptrv(win(11));
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-20, 300), r[1] = g.range(-20, 200), r[2] = r[0] + g.range(-5, 200), r[3] = r[1] + g.range(-5, 100);
    g_args[2] = ptrv(r);
    g_args[3] = g.next(), g_args[4] = g.next();
    g_args[5] = g.chance(2) ? 0 : g.next();
    g_args[6] = g.below(16);
    g_args[7] = g.chance(2) ? 1 : g.below(3);
}

// Construction and destruction on real objects (the colours' index bytes are stack garbage: cleared)
void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(4) ? 0 : ptrv(win(1));
}
void clear_garbage() { clear_colour_garbage(g_self); }
void destroy() { original<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x409c40)(g_self, 0); }
void s_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x409be0)(obj, win(1));
    clear_colour_garbage(obj);
    if (g.chance(3))
        at_(obj, 0x108) = ptrv(xalloc(16));
    if (g.chance(3))
        at_(obj, 0x10c) = ptrv(xalloc(32));
    for (int k = 0; k < (int)g.below(3); k++)
        link_child(obj, win(2 + k));
    g_self = obj;
    g_args[0] = g.below(4);
}

const Test TESTS[] = {
    {"Tabs ctor", 0x409be0, CALL_THIS, 1, ~0u, 200, s_ctor, S_TABS, N_TABS, destroy, clear_garbage},
    {"Tabs deleting dtor", 0x409c40, CALL_THIS, 1, ~0u, 200, s_dtor, S_TABS, N_TABS},
    {"Tabs Key", 0x40a1a0, CALL_THIS, 1, ~0u, 600, s_key, S_TABS, N_TABS},
    {"Tabs ShowHeaders", 0x40a2b0, CALL_THIS, 1, 0, 200, s_flag, S_TABS, N_TABS},
    {"Tabs Add", 0x40a2f0, CALL_THIS, 1, ~0u, 600, s_add, S_TABS, N_TABS},
    {"Tabs Find", 0x40a4a0, CALL_THIS, 1, ~0u, 300, s_find, S_TABS, N_TABS},
    {"Tabs Select", 0x40a4b0, CALL_THIS, 2, 0, 500, s_select, S_TABS, N_TABS},
    {"Tabs FitAll", 0x40a520, CALL_THIS, 0, 0, 200, s_none, S_TABS, N_TABS},
    {"Tabs Layout", 0x40a550, CALL_THIS, 0, 0, 800, s_layout, S_TABS, N_TABS},
    {"Bevel", 0x40a740, CALL_FAST, 8, ~0u, 1000, s_bevel, S_BEVEL, 1},
};

}  // namespace

ZAX_CHECKS(ui_tabs)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        g_globals.add(0x62e5c4, 1);
    Tally t{"ui tabs: CJTabControl, the bevel"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 9100);
    t.report();
}

}  // namespace zax
