// Checks of the save/load screen's details page (ui_save_load_rest4c.cpp) against the original with the toolkit's
// harness and the screens' setup (checks_ui_screens.h): the screen is window 0 with its lists (their choice a case
// value), its button, tab control and pages; the saves' statistics and their items are fakes of the case with
// ZStrings of the harness; the toolkit's functions, the button's image, the tab control, the detail row, the colour
// font, the mission number and the client width (a case value, so the page loops settle) are stand-ins; the exe's
// static row texts point to ZStrings of the block (their guard bits set: built), the localised labels are the game's.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_save_load_rest4c(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_save_load_rest4c", &hooks_ui_save_load_rest4c};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"},
    {0x4100f0, CV_THIS, 1, K_VALUE, "Button::Set114"},
    {0x409be0, CV_THIS, 1, 'e', "CJTabControl::CJTabControl"},
    {0x40a2f0, CV_THIS, 1, K_VALUE, "CJTabControl::Add"},
    {0x40a4b0, CV_THIS, 2, K_VALUE, "CJTabControl::Select"},
    {0x40da30, CV_THIS, 1, K_VALUE, "CJWindow::ShowScrollBars"},
    {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"},
    {0x40d490, CV_THIS, 0, K_FIXED2, "CJWindow::ClientWidth"},
    {0x564ce0, CV_THIS, 9, K_VALUE, "SaveLoadScreen::DetailRow", "vssvvssvv"},
    {0x5aedf0, CV_THIS, 0, K_TEXT, "StatsColorFont"},
    {0x512280, CV_THIS, 0, K_SMALL, "CMission::Number"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_LIST = W_OTHER, W_LIST1 = W_OTHER2, W_TABS_T = 13, W_BUTTON = 12;
constexpr DWORD STATIC_TEXTS[4] = {0x7131c8, 0x713204, 0x713188, 0x713194};

void effect(int cls, int slot, DWORD *, DWORD answer)
{
    if (cls != -1)
        return;
    if ((DWORD)slot == 0x409be0 && in_block(answer)) {   // a new tab control: the tab control class's fake
        made(answer, W_PLAIN_T);
        at_(at<BYTE *>(answer), 0) = at_(win(W_TABS_T), 0);
    } else if (const int t = template_of((DWORD)slot)) {
        made(answer, t);
    }
}

char g_object_args[MAXSLOT + 1];

BYTE *text_at(BYTE *z, Gen &g)
{
    make_text(z, TEXTS[g.below(8)]);
    return z;
}

BYTE *make_stats(Gen &g)
{
    BYTE *s = xalloc(0x80);
    for (DWORD o = 0; o < 0x80; o += 4)
        at_(s, o) = g.next();
    text_at(s + 0xc, g);
    text_at(s + 0x14, g);
    text_at(s + 0x5c, g);
    at_(s, 0x10) = g.chance(3) ? 0 : 0x5a000000u + g.below(0x100);
    at_<float>(s, 0x54) = g.chance(8) ? 0.0f : (float)g.range(-10, 1100) / 10.0f;
    at_(s, 0x58) = g.below(100000);
    at_(s, 0x60) = g.below(40);
    at_(s, 0x64) = g.below(40);
    const DWORD n = g.below(4);   // (at most 3 items: the harness keeps 64 ZStrings a case)
    DWORD *items = reinterpret_cast<DWORD *>(xalloc(4 * (n + 1)));
    for (DWORD i = 0; i < n; i++) {
        BYTE *it = xalloc(0x20);
        at_(it, 8) = g.below(2);
        text_at(it + 0xc, g);
        text_at(it + 0x14, g);
        at_(it, 0x10) = g.next();
        at_(it, 0x18) = g.next();
        items[i] = ptrv(it);
    }
    at_(s, 0x70) = ptrv(items);
    at_(s, 0x74) = n;
    return s;
}

void base(Gen &g)
{
    screen_base(g, &effect);
    object_args(g_object_args, "0:1");
    make_window(W_TABS_T, C_TABS);
    make_plain_window(W_BUTTON, C_BUTTON, g);
    make_window(W_LIST, C_LISTBOX);
    make_window(W_LIST1, C_DROPLIST);
    set_kind(C_LISTBOX, 77, K_FIXED0);
    set_kind(C_DROPLIST, 77, K_FIXED1);
    BYTE *s = win(0);
    s[0x78] = (BYTE)g.range(0, 12);
    s[0x79] = (BYTE)g.range(0, 12);
    // the saves' statistics, the lists' choices
    const DWORD n = 1 + g.below(3);
    DWORD *all = reinterpret_cast<DWORD *>(xalloc(4 * n));
    for (DWORD i = 0; i < n; i++)
        all[i] = ptrv(make_stats(g));
    at_(s, 0x18c) = ptrv(all);
    at_(s, 0x19c) = ptrv(make_stats(g));
    g_case.fixed[0] = g.chance(3) ? 0xffffffff : g.below(n);
    g_case.fixed[1] = g.chance(2) ? 0xffffffff : g.below(n);
    g_case.fixed[2] = (DWORD)g.range(50, 400);
    at_(s, 0x138) = g.chance(3) ? 0 : ptrv(win(W_LIST1));
    at_(s, 0x144) = ptrv(win(W_LIST));
    at_(s, 0x148) = ptrv(win(W_BUTTON));
    // the old tab control and pages (their widths are kept)
    if (g.chance(2)) {
        BYTE *t = xalloc(0x120);
        at_(t, 0) = at_(win(W_TABS_T), 0);
        at_(t, 0x114) = g.below(4);
        at_(s, 0x14c) = ptrv(t);
    } else {
        at_(s, 0x14c) = 0;
    }
    const DWORD pages[4] = {0x150, 0x154, 0x158, 0x15c};
    for (int i = 0; i < 4; i++) {
        BYTE *p = xalloc(0x40);
        int *r = reinterpret_cast<int *>(p + 4);
        r[0] = g.range(0, 100), r[1] = g.range(0, 100), r[2] = r[0] + g.range(0, 500), r[3] = r[1] + g.range(0, 300);
        at_(s, pages[i]) = ptrv(p);
    }
    // the page members of the empty details
    const DWORD members = g.below(5);
    for (DWORD i = 0; i < members; i++)
        at_(s, 0x160 + i * 4) = ptrv(s + pages[i]);
    at_(s, 0x160 + members * 4) = 0;
    // the exe's static row texts: ZStrings of the block, built
    for (DWORD z : STATIC_TEXTS) {
        BYTE *b = xalloc(4);
        make_text(b, TEXTS[g.below(8)]);
        case_global(z, at_(b, 0));
    }
    case_global(0x7131b8, 0xf);
    g_self = s;
}

void s_none(Gen &) {}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x7131c8, 0x713204, 0x713188, 0x713194};

const Test TESTS[] = {
    {"SaveLoadScreen ShowDetails", 0x564090, CALL_THIS, 0, 0, 400, s_none, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_save_load_rest4c)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        g_globals.add(0x7131b8, 1);
    }
    Tally t{"ui save load rest 4c: the save/load screen's details page"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 49000);
    t.report();
}

}  // namespace zax
