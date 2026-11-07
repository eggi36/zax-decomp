// Checks of the window base class (ui_window.cpp) against the original, in the game thread, with the
// toolkit's harness (checks_ui_common.h): random trees of fake windows (rectangles, flags, accelerators,
// titles, scroll infos, scroll bars), every function on both sides from the same seed with its virtual calls
// and the exe functions beyond the module recorded and answered alike. Compared: eax (where the original
// defines it), the call log, the whole block and the toolkit's globals.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_window(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_window", &hooks_ui_window};

// The exe functions the base class reaches beyond its module, recorded for the case
const Stub STUBS[] = {
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
    {0x518570, CV_THIS, 2, K_VALUE, "Cursor::SetPos"},
    {0x518590, CV_THIS, 1, K_VALUE, "Cursor::Set"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vssv"},
    {0x409330, CV_THIS, 3, K_VALUE, "ScreenWindow::StartKeyRepeat"},
    {0x4093c0, CV_THIS, 0, K_VALUE, "ScreenWindow::KeyRepeatDue"},
    {0x4098d0, CV_THIS, 4, K_VALUE, "ScreenWindow::ShowTip"},
    {0x4098f0, CV_THIS, 5, K_VALUE, "ScreenWindow::ShowTipAccel"},
    {0x409b30, CV_THIS, 0, K_VALUE, "ScreenWindow::MouseUpdate"},
    // the message box: its block in the extra area, its constructor recorded (returns the block)
    {0x417710, CV_FAST, 0, 'n', "operator_new_ecx", "vx"},
    {0x403b30, CV_THIS, 3, 'e', "MessageBox::MessageBox"},
};
constexpr int NSTUBS_PLAIN = 10, NSTUBS_ALL = 12;

Globals g_globals;

void init_globals()
{
    if (g_globals.n)
        return;
    g_globals.add(0x62e5e0, 1);   // focus by key
    g_globals.add(0x60a528, 4);   // redraw counter
    g_globals.add(0x60a52c, 1);   // shared border
    g_globals.add(0x60a52d, 1);   // no textures
    g_globals.add(0x62e5c8, 4);   // texture list owner
    g_globals.add(0x62e5cc, 4);   // texture list
    g_globals.add(0x62e5d4, 4);
    g_globals.add(0x62e5d8, 4);
    g_globals.add(0x62e5dc, 4);
}

// ---- The cases --------------------------------------------------------------------------------------------------

int g_n;   // windows in the tree

int any(Gen &g) { return (int)g.below((DWORD)g_n); }
DWORD any_win(Gen &g) { return ptrv(win(any(g))); }
// A window with children (or any)
BYTE *parent_win(Gen &g)
{
    for (int k = 0; k < 8; k++) {
        BYTE *w = win(any(g));
        if (at_(w, 0xec))
            return w;
    }
    return win(any(g));
}
BYTE *child_of(BYTE *p, Gen &g)
{
    int n = 0;
    for (BYTE *c = at<BYTE *>(at_(p, 0xec)); c; c = at<BYTE *>(at_(c, 0xf4)))
        n++;
    if (!n)
        return nullptr;
    int k = (int)g.below((DWORD)n);
    BYTE *c = at<BYTE *>(at_(p, 0xec));
    while (k--)
        c = at<BYTE *>(at_(c, 0xf4));
    return c;
}
int *point(Gen &g)
{
    int *p = reinterpret_cast<int *>(xalloc(16));
    p[0] = g.range(-60, 700), p[1] = g.range(-60, 500);
    return p;
}
int *rect(Gen &g)
{
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-60, 600), r[1] = g.range(-60, 450);
    r[2] = r[0] + g.range(-10, 400), r[3] = r[1] + g.range(-10, 300);
    return r;
}

void effect(int cls, int slot, DWORD *args, DWORD)
{
    // slot 14 (a drag) names the cursor through its fourth argument
    if (cls >= 0 && slot == 14 && args[3] && readable(at<void *>(args[3]), 4))
        *at<DWORD *>(args[3]) = g_case.values.chance(2) ? 0 : g_case.values.next();
}

// The tree of every case: 2-10 windows, some scroll infos (with fake scroll bars from the spare windows),
// titles from TEXTS, a few fake fonts
void build_tree(Gen &g)
{
    g_n = g.range(2, 10);
    make_tree(g, g_n, C_WINDOW, g.chance(4) ? C_BUTTON : C_WINDOW);
    for (int i = g_n; i < NWIN; i++)   // the spare windows (answers of slots 36/37, scroll bars, new children)
        make_plain_window(i, C_WINDOW, g);
    g_case.nwin = NWIN;
    for (int i = 0; i < g_n; i++) {
        BYTE *w = win(i);
        if (g.chance(3))
            make_text(w + 0x50, TEXTS[g.below(8)]);
        if (g.chance(5))
            at_(w, 0x7c) = ptrv(fake_font(g.chance(4) ? g.next() : g.below(60)));
        if (g.chance(3)) {
            BYTE *vb = g.chance(2) ? win(12) : nullptr;
            BYTE *hb = g.chance(2) ? win(13) : nullptr;
            const int z = (int)g.below(4);   // some all zero (freed when the call finds them empty)
            make_scroll(w, z ? g.range(-20, 300) : 0, z ? g.range(-20, 300) : 0, z > 1 ? g.range(0, 900) : 0,
                        z > 1 ? g.range(0, 900) : 0, z == 3 && g.chance(2), vb, hb);
        }
        if (g.chance(6))
            at_(w, 0xe0) = g.next(), at_(w, 0xe4) = g.chance(2) ? g.next() : 0;
    }
    // the focus by key and the shared border vary
    case_global(0x62e5e0, g.below(2));
    case_global(0x60a52d, g.below(2));
    g_self = win(any(g));
    g_case.effect = &effect;
}


// Setups
void s_none(Gen &) {}
void s_value(Gen &g) { g_args[0] = g.chance(3) ? g.below(3) : g.next(); }
void s_values2(Gen &g) { g_args[0] = g.next(), g_args[1] = g.chance(2) ? g.below(4) : g.next(); }
void s_parent(Gen &g) { g_self = parent_win(g); }
void s_point3(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = g.below(4), g_args[1] = ptrv(point(g)), g_args[2] = g.next();
}
void s_point2(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = g.below(4), g_args[1] = ptrv(point(g));
}
void s_button(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = g.chance(5) ? g.below(32) : g.below(3);
    g_args[1] = ptrv(point(g));
    if (g.chance(2))
        win(any(g))[0x67] = (BYTE)g.next();
}
void s_cache(Gen &g)
{
    BYTE *c = xalloc(0x64);
    if (!g.chance(3)) {
        int *d = &at_<int>(c, 0x20);
        d[0] = g.range(-50, 400), d[1] = g.range(-50, 300), d[2] = d[0] + g.range(-5, 300), d[3] = d[1] + g.range(-5, 200);
    }
    at_(c, 0x30) = any_win(g);
    g_self = c;
    g_args[0] = ptrv(rect(g));
}
void s_key(Gen &g)
{
    g_self = parent_win(g);
    const DWORD k[] = {9, 9, 0x1b, 0x1b, 0x0d, 0x20};
    g_args[0] = g.chance(4) ? g.next() & 0x1ff : k[g.below(6)];
    if (g.chance(2))
        g_self[0x67] = (BYTE)g.below(4);
}
void s_char(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = g.chance(2) ? 'a' + g.below(26) : 'A' + g.below(26);
}
void s_fast2(Gen &g) { g_args[0] = g.next(), g_args[1] = g.next(); }
void s_fast3(Gen &g) { g_args[0] = g.next(), g_args[1] = g.next(), g_args[2] = g.next(); }
void s_cursor_index(Gen &g) { g_args[0] = g.below(12); }
// A free window (not in the tree) to add, sometimes with a child of its own that takes the focus or not
void s_add(Gen &g)
{
    g_self = parent_win(g);
    BYTE *c = win(14);
    if (g.chance(2))
        link_child(c, win(15));
    g_args[0] = ptrv(c);
    g_args[1] = g.chance(2) ? 0 : g.below(20);
    g_args[2] = g.next(), g_args[3] = g.range(-1000, 400), g_args[4] = g.range(-1000, 400);
    g_args[5] = g.below(8);
}
void s_child(Gen &g)
{
    g_self = parent_win(g);
    BYTE *c = child_of(g_self, g);
    g_args[0] = c ? ptrv(c) : ptrv(win(0));
    if (!c)
        g_self = win(0), g_args[0] = at_(win(0), 0xec);
    if (!g_args[0]) {   // no children anywhere: give window 0 one
        link_child(win(0), win(14));
        g_self = win(0);
        g_args[0] = ptrv(win(14));
    }
}
void s_win_place(Gen &g)
{
    g_args[0] = any_win(g);
    g_args[1] = g.below(20);
}
void s_title(Gen &g)
{
    const char *const titles[] = {"&OK", "Load &Game", "&Save && &Exit", "No accel", "&", "&\x01x", "a&b&c&d", "&&",
                                  "End &Game", "Te&st &one"};
    char *t = reinterpret_cast<char *>(xalloc(32));
    lstrcpyA(t, titles[g.below(10)]);
    g_args[0] = g.chance(8) ? 0 : ptrv(t);
}
void s_find(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
}
void s_tip(Gen &g) { g_args[0] = ptrv(TEXTS[g.below(8)]), g_args[1] = g.next(), g_args[2] = g.next(), g_args[3] = g.next(); }
void s_color(Gen &g)
{
    BYTE *c = xalloc(16);
    for (int i = 0; i < 16; i++)
        c[i] = (BYTE)g.next();
    g_args[0] = ptrv(c);
}
void s_border(Gen &g)
{
    at_(g_self, 0x7c) = ptrv(fake_font(g.chance(3) ? g.next() : g.below(80)));
    g_args[0] = g.chance(3) ? g.next() : g.below(6);
}
void s_other(Gen &g) { g_args[0] = any_win(g); }
// A run of siblings: self and a later one
void s_siblings(Gen &g)
{
    BYTE *p = parent_win(g);
    BYTE *a = child_of(p, g);
    if (!a)
        a = win(0);
    int n = 0;
    for (BYTE *c = a; c; c = at<BYTE *>(at_(c, 0xf4)))
        n++;
    int k = (int)g.below((DWORD)n);
    BYTE *b = a;
    while (k--)
        b = at<BYTE *>(at_(b, 0xf4));
    g_self = a;
    g_args[0] = ptrv(b);
    g_args[1] = g.chance(4) ? g.next() : (DWORD)g.range(-200, 400);
}
void s_rect(Gen &g) { g_args[0] = ptrv(g.chance(5) ? reinterpret_cast<int *>(g_self + 4) : rect(g)); }
void s_size(Gen &g) { g_args[0] = (DWORD)g.range(-50, 800), g_args[1] = (DWORD)g.range(-50, 800); }
void s_scroll2(Gen &g) { g_args[0] = (DWORD)g.range(-300, 1200), g_args[1] = (DWORD)g.range(-300, 1200); }
void s_scroll1(Gen &g) { g_args[0] = (DWORD)g.range(-300, 1200); }
void s_scroll_rect(Gen &g) { g_args[0] = ptrv(rect(g)); }
void s_show(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_fit(Gen &g)
{
    g_args[0] = any_win(g);
    g_args[1] = g.chance(2) ? (DWORD)-1000 : g.below(6);
}
void s_ini(Gen &g)
{
    g_args[0] = g.chance(2) ? 0 : ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.below(2);
}
void s_visible(Gen &g)
{
    g_args[0] = (DWORD)g.range(-100, 300), g_args[1] = (DWORD)g.range(-100, 300);
    g_args[2] = ptrv(xalloc(16));
    g_args[3] = g.next();
}
void s_kinds_texture(Gen &g)
{
    (void)g;
    set_kind_all(30, K_BOOL);
}

const Test TESTS[] = {
    {"SetLook", 0x40b5c0, CALL_THIS, 1, ~0u, 100, s_value, STUBS, NSTUBS_PLAIN},
    {"Update", 0x40b5d0, CALL_THIS, 1, 0, 150, s_value, STUBS, NSTUBS_PLAIN},
    {"cache AddDirty", 0x40bac0, CALL_THIS, 1, 0, 200, s_cache, STUBS, NSTUBS_PLAIN},
    {"Close", 0x40bb90, CALL_THIS, 0, ~0u, 50, s_none, STUBS, NSTUBS_PLAIN},
    {"CloseChildren", 0x40bbb0, CALL_THIS, 0, 0xff, 150, s_parent, STUBS, NSTUBS_PLAIN},
    {"slot 9", 0x40bc30, CALL_THIS, 3, 0xff, 300, s_point3, STUBS, NSTUBS_PLAIN},
    {"MouseDown", 0x40bcc0, CALL_THIS, 2, ~0u, 400, s_button, STUBS, NSTUBS_PLAIN},
    {"MouseUp", 0x40bde0, CALL_THIS, 2, ~0u, 200, s_button, STUBS, NSTUBS_PLAIN},
    {"StartKeyRepeat", 0x40be70, CALL_THIS, 2, ~0u, 100, s_values2, STUBS, NSTUBS_PLAIN},
    {"KeyRepeatDue", 0x40bec0, CALL_THIS, 0, ~0u, 100, s_none, STUBS, NSTUBS_PLAIN},
    {"SetTextureList", 0x40bf00, CALL_FAST, 2, 0, 50, s_fast2, STUBS, NSTUBS_PLAIN},
    {"slot 15", 0x40c0b0, CALL_THIS, 2, 0xff, 300, s_point2, STUBS, NSTUBS_PLAIN},
    {"Key", 0x40c170, CALL_THIS, 1, ~0u, 400, s_key, STUBS, NSTUBS_PLAIN},
    {"Char", 0x40c310, CALL_THIS, 1, ~0u, 400, s_char, STUBS, NSTUBS_PLAIN},
    {"SetActive", 0x40c3e0, CALL_THIS, 1, ~0u, 100, s_value, STUBS, NSTUBS_PLAIN},
    {"Notify", 0x40c400, CALL_THIS, 2, 0, 100, s_values2, STUBS, NSTUBS_PLAIN},
    {"SetTransparent", 0x40c600, CALL_THIS, 1, 0, 100, s_value, STUBS, NSTUBS_PLAIN},
    {"IsEnabled", 0x40c630, CALL_THIS, 0, ~0u, 100, s_none, STUBS, NSTUBS_PLAIN},
    {"AddToParentFront", 0x40c680, CALL_THIS, 2, ~0u, 100, s_win_place, STUBS, NSTUBS_PLAIN},
    {"AddModalToParent", 0x40c6e0, CALL_THIS, 2, ~0u, 100, s_win_place, STUBS, NSTUBS_PLAIN},
    {"MessageBox", 0x40c740, CALL_THIS, 2, ~0u, 50, s_tip, STUBS, NSTUBS_ALL},
    {"AddChild", 0x40c7c0, CALL_THIS, 6, 0, 400, s_add, STUBS, NSTUBS_PLAIN},
    {"AddChildFront", 0x40c8d0, CALL_THIS, 6, 0, 400, s_add, STUBS, NSTUBS_PLAIN},
    {"RemoveChild", 0x40c9b0, CALL_THIS, 1, ~0u, 300, s_child, STUBS, NSTUBS_PLAIN},
    {"Focus", 0x40ca40, CALL_THIS, 1, 0, 400, s_child, STUBS, NSTUBS_PLAIN},
    {"ToBack", 0x40caf0, CALL_THIS, 1, ~0u, 200, s_child, STUBS, NSTUBS_PLAIN},
    {"FocusNext", 0x40cb30, CALL_THIS, 0, ~0u, 150, s_parent, STUBS, NSTUBS_PLAIN},
    {"FocusPrev", 0x40cb70, CALL_THIS, 0, ~0u, 150, s_parent, STUBS, NSTUBS_PLAIN},
    {"Root", 0x40cc70, CALL_THIS, 0, ~0u, 100, s_none, STUBS, NSTUBS_PLAIN},
    {"FindChild", 0x40cc90, CALL_THIS, 1, ~0u, 200, s_find, STUBS, NSTUBS_PLAIN},
    {"Screen", 0x40ccd0, CALL_THIS, 0, ~0u, 100, s_none, STUBS, NSTUBS_PLAIN},
    {"ShowTip", 0x40cd00, CALL_THIS, 3, 0, 100, s_tip, STUBS, NSTUBS_PLAIN},
    {"ShowTipAccel", 0x40cd50, CALL_THIS, 4, 0, 100, s_tip, STUBS, NSTUBS_PLAIN},
    {"ClearAccel", 0x40cdb0, CALL_THIS, 0, 0, 50, s_none, STUBS, NSTUBS_PLAIN},
    {"SetTitle", 0x40cdc0, CALL_THIS, 1, 0, 400, s_title, STUBS, NSTUBS_PLAIN},
    {"SetTitleChar", 0x40cee0, CALL_THIS, 1, ~0u, 100, s_value, STUBS, NSTUBS_PLAIN},
    {"SetColorA0", 0x40cfe0, CALL_THIS, 1, ~0u, 50, s_color, STUBS, NSTUBS_PLAIN},
    {"SetBorder", 0x40d0c0, CALL_THIS, 1, ~0u, 300, s_border, STUBS, NSTUBS_PLAIN},
    {"RightOf", 0x40d0f0, CALL_THIS, 1, ~0u, 100, s_other, STUBS, NSTUBS_PLAIN},
    {"SpreadWidth", 0x40d100, CALL_THIS, 2, ~0u, 400, s_siblings, STUBS, NSTUBS_PLAIN},
    {"SameWidth", 0x40d230, CALL_THIS, 1, ~0u, 300, s_siblings, STUBS, NSTUBS_PLAIN},
    {"SetRect", 0x40d2a0, CALL_THIS, 1, 0, 500, s_rect, STUBS, NSTUBS_PLAIN},
    {"ClientWidth", 0x40d490, CALL_THIS, 0, ~0u, 150, s_none, STUBS, NSTUBS_PLAIN},
    {"ClientHeight", 0x40d4d0, CALL_THIS, 0, ~0u, 150, s_none, STUBS, NSTUBS_PLAIN},
    {"SetSize", 0x40d510, CALL_THIS, 2, ~0u, 150, s_size, STUBS, NSTUBS_PLAIN},
    {"SetContentSize", 0x40d550, CALL_THIS, 2, 0, 300, s_size, STUBS, NSTUBS_PLAIN},
    {"MoveTo", 0x40d630, CALL_THIS, 2, ~0u, 150, s_size, STUBS, NSTUBS_PLAIN},
    {"SetLeft", 0x40d680, CALL_THIS, 1, ~0u, 100, s_scroll1, STUBS, NSTUBS_PLAIN},
    {"SetTop", 0x40d6c0, CALL_THIS, 1, ~0u, 100, s_scroll1, STUBS, NSTUBS_PLAIN},
    {"SetScrollPos", 0x40d700, CALL_THIS, 2, ~0u, 500, s_scroll2, STUBS, NSTUBS_PLAIN},
    {"SetScrollX", 0x40d8c0, CALL_THIS, 1, 0, 250, s_scroll1, STUBS, NSTUBS_PLAIN},
    {"SetScrollY", 0x40d900, CALL_THIS, 1, 0, 250, s_scroll1, STUBS, NSTUBS_PLAIN},
    {"ScrollToShow", 0x40d940, CALL_THIS, 1, 0, 300, s_scroll_rect, STUBS, NSTUBS_PLAIN},
    {"ScrollBy", 0x40d9f0, CALL_THIS, 2, ~0u, 200, s_scroll2, STUBS, NSTUBS_PLAIN},
    {"ShowScrollBars", 0x40da30, CALL_THIS, 1, 0, 300, s_show, STUBS, NSTUBS_PLAIN},
    {"FitChild", 0x40e590, CALL_THIS, 2, 0, 300, s_fit, STUBS, NSTUBS_PLAIN},
    {"SaveIni", 0x40e8f0, CALL_THIS, 2, 0, 100, s_ini, STUBS, NSTUBS_PLAIN},
    {"TexturesOn", 0x40e9b0, CALL_CDECL, 0, ~0u, 50, s_none, STUBS, NSTUBS_PLAIN},
    {"SetGlobals3", 0x40e9c0, CALL_FAST, 3, ~0u, 50, s_fast3, STUBS, NSTUBS_PLAIN},
    {"CursorEntry", 0x40e9e0, CALL_FAST, 2, ~0u, 50, s_cursor_index, STUBS, NSTUBS_PLAIN},
    {"Cursor", 0x40ea00, CALL_THIS, 1, ~0u, 50, s_value, STUBS, NSTUBS_PLAIN},
    {"slot 70", 0x40ea10, CALL_THIS, 1, 0xff, 100, s_value, STUBS, NSTUBS_PLAIN},
    {"Texture", 0x40eb00, CALL_THIS, 0, ~0u, 150, s_kinds_texture, STUBS, NSTUBS_PLAIN},
    {"VisibleClient", 0x40ef90, CALL_THIS, 4, ~0u, 400, s_visible, STUBS, NSTUBS_PLAIN},
    {"SetWidth", 0x40f5b0, CALL_THIS, 1, ~0u, 100, s_scroll1, STUBS, NSTUBS_PLAIN},
    {"SetHeight", 0x40f5d0, CALL_THIS, 1, ~0u, 100, s_scroll1, STUBS, NSTUBS_PLAIN},
    {"slot 66", 0x40f840, CALL_THIS, 0, 0, 150, s_none, STUBS, NSTUBS_PLAIN},
};

}  // namespace

ZAX_CHECKS(ui_window)
{
    (void)cls;
    (void)n;
    init_globals();
    Tally t{"ui window: CJWindow base class (child list, focus, dispatch, titles, rectangles, scroll info)"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &build_tree, 5100);
    t.report();
}

}  // namespace zax
