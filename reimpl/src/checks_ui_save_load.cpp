// Checks of the save/load screen's smaller functions (ui_save_load.cpp) against the original with the toolkit's
// harness and the screens' setup (checks_ui_screens.h): the screen is a fake dialog (window 0) with fake lists,
// buttons and statistics (game objects); the font list gets a fake vtable and records of the case; the details'
// labels are made on the caller's dummy window (its constructor and destructor stand-ins); the statistics' compare
// is a stand-in.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_save_load(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_save_load", &hooks_ui_save_load};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x592220, CV_THIS, 0, K_VALUE, "FieldNameList::Dtor"},
    {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"},
    {0x48d8f0, CV_THIS, 1, K_VALUE, "DefinitionList_Object"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x5bcc10, CV_THIS, 2, K_VALUE, "CZaxSaveGameStats::Compare"},
    // (the dummy window is a local of the caller: its address and contents are not compared; these entries
    // override the screens' ones)
    {0x4127d0, CV_THIS, 2, 'e', "TextLabel::TextLabel", "xs"},
    {0x40a8a0, CV_FAST, 2, 'e', "CJWindow::CJWindow (dummy)", "xxvs"},
    {0x40b190, CV_FAST, 0, K_VALUE, "CJWindow::~CJWindow (dummy)", "xx"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

DWORD g_list_vt[40];
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7801, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *, DWORD answer)
{
    if (cls == -1)
        if (const int t = template_of((DWORD)slot))
            made(answer, t);
}

char g_object_args[MAXSLOT + 1];

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[3] = ptrv((void *)&list_find);
    }
    screen_base(g, &effect);
    object_args(g_object_args, "0:1,1:1,5:0,23:1,39:0,45:0");
    const DWORD n = g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    case_global(0x6be1e0, ptrv(g_list_vt));
    case_global(0x6be210, g.chance(8) ? 0 : ptrv(recs));
    case_global(0x6be214, n);
    case_global(0x71320c, g.next());
    // the screen under test
    BYTE *w = win(0);
    at_(w, 0x138) = ptrv(win(W_LIST_T));
    at_(w, 0x144) = ptrv(win(W_LIST_T));
    at_(w, 0x13c) = g.chance(4) ? 0 : ptrv(win(W_EDIT_T));
    win(W_EDIT_T)[0x5c] = g.chance(2);
    at_(w, 0x140) = g.chance(4) ? 0 : ptrv(win(W_BUTTON_T));
    at_(w, 0x174) = g.chance(4) ? 0 : ptrv(win(W_BUTTON_T));
    at_(w, 0x178) = ptrv(win(W_BUTTON_T));
    at_(w, 0x184) = g.chance(4) ? 0 : ptrv(win(W_APP));
    at_(w, 0x198) = g.chance(4) ? 0 : ptrv(win(W_WORLD));
    const DWORD k = g.below(4);
    BYTE *d = xalloc(4 * (k + 1));
    for (DWORD i = 0; i < k; i++)
        at_(d, 4 * i) = g.chance(5) ? 0 : ptrv(win(W_OBJECT));
    at_(w, 0x188) = 0x5ff07c, at_(w, 0x18c) = k ? ptrv(d) : 0, at_(w, 0x190) = k, at_(w, 0x194) = k + 1;
    at_(w, 0x14c) = ptrv(win(W_PARENT));
    set_kind(C_LISTBOX, 77, g.chance(3) ? K_SMALL : K_VALUE);
    set_kind(C_OBJECT, 45, K_WINDOW_NN);
    set_kind(C_OBJECT, 23, K_FIXED0);
    g_case.fixed[0] = ptrv(win(W_OBJECT));
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_LABEL, 45, K_FIXED0);
    set_kind(C_WINDOW, 58, K_RECT);
}

void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_names(Gen &g)
{
    g_self = xalloc(0x20);
    s_flags(g);
}
void s_details(Gen &g)
{
    g_self = xalloc(0x110);
    s_flags(g);
}
void s_dt(Gen &g) { g_args[0] = g.next(); }
void s_row(Gen &g)
{
    BYTE *page = make_plain_window(W_OTHER, C_WINDOW, g);
    at_(page, 0xf0) = g.chance(3) ? 0 : ptrv(win(W_OTHER2));
    g_args[0] = ptrv(page);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = ptrv(TEXTS[g.below(8)]);
    g_args[3] = g.below(300);
    g_args[4] = g.below(3);
    g_args[5] = ptrv(TEXTS[g.below(8)]);
    g_args[6] = ptrv(TEXTS[g.below(8)]);
    g_args[7] = g.below(300);
    g_args[8] = g.below(3);
}
void s_compare(Gen &g)
{
    // the source's slot 5 object (+0x14 the screen with the statistics)
    BYTE *o = win(W_OBJECT);
    at_(o, 0x14) = ptrv(win(0));
    g_case.fixed[1] = ptrv(o);
    set_kind(C_OBJECT, 5, K_FIXED1);
    BYTE *d = xalloc(0x20);
    for (int i = 0; i < 6; i++)
        at_(d, 4 * i) = g.next();
    at_(win(0), 0x18c) = ptrv(d);
    g_args[0] = g.next(), g_args[1] = g.next(), g_args[2] = g.next();
    g_args[3] = ptrv(win(W_APP));
    g_args[4] = g.below(6), g_args[5] = g.below(6);
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214, 0x71320c};

const Test TESTS[] = {
    {"SaveLoadScreen deleting dtor", 0x563680, CALL_THIS, 1, ~0u, 100, s_flags, STUBS, NSTUBS},
    {"SaveLoadNames deleting dtor", 0x5636b0, CALL_THIS, 1, ~0u, 20, s_names, STUBS, NSTUBS},
    {"SaveLoadScreen dtor", 0x5636e0, CALL_THIS, 0, 0, 100, s_dt, STUBS, NSTUBS},
    {"SaveLoadScreen Update", 0x5637d0, CALL_THIS, 1, 0, 100, s_dt, STUBS, NSTUBS},
    {"SaveLoadDetails deleting dtor", 0x564c70, CALL_THIS, 1, ~0u, 20, s_details, STUBS, NSTUBS},
    {"SaveLoadScreen DetailRow", 0x564ce0, CALL_THIS, 9, 0, 200, s_row, STUBS, NSTUBS},
    {"SaveLoadScreen Compare", 0x564ef0, CALL_FAST, 6, ~0u, 60, s_compare, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_save_load)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui save load: the save/load screen's destructors, update, detail rows and sort"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 31000);
    t.report();
}

}  // namespace zax
