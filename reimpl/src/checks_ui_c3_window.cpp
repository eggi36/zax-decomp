// Checks of batch C3's window base class slots (ui_c3_window.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): random trees of fake windows as in checks_ui_window.cpp; the INI file, the cursor, the
// font, the opacity of the blend modes and the frame loading as recording stand-ins (the frame filter on the
// stack runs for real); the display object behind the error reporter a fake game object.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_c3_window(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_c3_window", &hooks_ui_c3_window};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4b3430, CV_THIS, 2, K_VALUE, "Frame_DrawRect", "rv"},
    {0x4a4670, CV_FAST, 2, K_VALUE, "Ini_GetInt", "vssv"},
    {0x40c600, CV_THIS, 1, K_VALUE, "CJWindow::SetTransparent"},
    {0x5184e0, CV_THIS, 2, K_VALUE, "Cursor::GetXY", "xx"},
    {0x518570, CV_THIS, 2, K_VALUE, "Cursor::SetPos"},
    {0x5d0348, CV_CDECL, 1, K_VALUE, "atol", "s"},
    {0x5d03de, CV_CDECL, 1, K_FLOAT, "atof", "s"},
    {0x5d0db7, CV_CDECL, 3, K_VALUE, "sprintf", "xsv"},
    // the title's underline colour is a local of the caller: the stand-ins fill it completely (the real ones
    // leave bytes of the stack in it)
    {0x53f000, CV_STD, 0, 'e', "FieldColor_Ctor"},
    {0x53f080, CV_THIS, 4, K_VALUE, "FieldColor_SetRGBA"},
    {0x433c20, CV_THIS, 0, K_FLOAT, "CBlendMode_GetOpacity"},
    {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"},
    {0x4a7310, CV_THIS, 2, K_SMALL, "CFont::TextWidthN", "sv"},
    {0x4a6140, CV_THIS, 0, K_SMALL, "CFont::LineHeight"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"},
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4a9a30, CV_THIS, 5, K_ONE, "CFrame::Load", "ssxvv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
// sprintf and SetRGBA run for real where the text or the colour matters (the title setters, the colour setters)
const Stub STUBS_REAL_TEXT[] = {
    UIC_ALLOC_STUBS,
};
constexpr int NSTUBS_REAL = sizeof STUBS_REAL_TEXT / sizeof STUBS_REAL_TEXT[0];

int g_n;
int any(Gen &g) { return (int)g.below((DWORD)g_n); }
BYTE *parent_win(Gen &g)
{
    for (int k = 0; k < 8; k++) {
        BYTE *w = win(any(g));
        if (at_(w, 0xec))
            return w;
    }
    return win(0);
}
BYTE *child_win(Gen &g)   // a window with a parent (never the root)
{
    return win(1 + (int)g.below((DWORD)(g_n - 1)));
}
int *point(Gen &g)
{
    int *p = reinterpret_cast<int *>(xalloc(16));
    p[0] = g.range(-60, 700), p[1] = g.range(-60, 500);
    return p;
}

// The display object of the reporter (slots 45 and 39: its width and height) and the reporter itself (+0x24 the
// colour depth)
BYTE *g_reporter;
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == -1 && (DWORD)slot == 0x5184e0) {   // the cursor position through its two outputs
        Gen &v = g_case.values;
        *at<int *>(args[0]) = v.range(-20, 700);
        *at<int *>(args[1]) = v.range(-20, 500);
    } else if (cls == -1 && (DWORD)slot == 0x53f000) {
        memset(at<void *>(g_effect_ecx), 0, 16);
    } else if (cls == -1 && (DWORD)slot == 0x53f080) {
        BYTE *c = at<BYTE *>(g_effect_ecx);
        c[0] = (BYTE)args[0], c[1] = (BYTE)args[1], c[2] = (BYTE)args[2], c[3] = (BYTE)args[3];
    }
}

void build(Gen &g)
{
    g_n = g.range(2, 10);
    make_tree(g, g_n, C_WINDOW, g.chance(4) ? C_BUTTON : C_WINDOW);
    for (int i = g_n; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    g_case.nwin = NWIN;
    for (int i = 0; i < g_n; i++) {
        BYTE *w = win(i);
        if (g.chance(3))
            make_text(w + 0x50, TEXTS[g.below(8)]);
        if (g.chance(5))
            at_(w, 0x7c) = ptrv(fake_font(g.chance(4) ? g.next() : g.below(60)));
        else
            at_(w, 0x7c) = ptrv(fake_font(g.below(40)));
        w[0x62] = (BYTE)g.below(2);
    }
    g_self = win(any(g));
    g_case.effect = &effect;
}

// The reporter for the INI position and the textures only (the real one stays for everything else, e.g. the
// colour setters' pixel format): a game object whose +4 is another (the display)
void fake_reporter(Gen &g)
{
    g_reporter = xalloc(0x40);
    BYTE *disp = make_window(15, C_OBJECT);
    at_(g_reporter, 4) = ptrv(disp);
    at_(g_reporter, 0x24) = g.chance(2) ? 8 : 16;
    case_global(0x6c02cc, ptrv(g_reporter));
    set_kind(C_OBJECT, 45, K_SMALL);
    set_kind(C_OBJECT, 39, K_SMALL);
}

void s_none(Gen &) {}
void s_value(Gen &g) { g_args[0] = g.chance(3) ? g.below(3) : g.next(); }
void s_values2(Gen &g) { g_args[0] = g.next(), g_args[1] = g.chance(2) ? g.below(4) : g.next(); }
void s_parent(Gen &g) { g_self = parent_win(g); }
void s_child(Gen &g) { g_self = child_win(g); }
void s_title_text(Gen &g)
{
    BYTE *rep = xalloc(0x20);
    lstrcpyA(reinterpret_cast<char *>(rep + 8), TEXTS[g.below(8)]);
    at_(g_self, 0x50) = ptrv(rep);
}
void s_text_slot(Gen &) { set_kind_all(44, K_TEXT); }
void s_callback(Gen &g);
void s_delete(Gen &g)
{
    if (g.chance(4))
        g_self = nullptr;
}
void s_outline(Gen &g)
{
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    g_args[1] = g.next();
}
void s_move(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = ptrv(point(g));
    g_args[1] = g.next();
    g_args[2] = ptrv(xalloc(4));
    if (g.chance(2))
        g_self[0x67] = (BYTE)g.below(4);
    set_kind_all(65, K_BOOL);
    set_kind_all(14, K_BOOL);
    set_kind_all(13, g.chance(2) ? K_ZERO : K_VALUE);
    g_case.argspec[69][0] = A_POINT;
    g_case.argspec[14][1] = A_RAW;
}
void s_children_slot(Gen &g)
{
    g_self = parent_win(g);
    g_args[0] = g.next();
    for (int s = 22; s <= 25; s++)
        set_kind_all(s, g.chance(2) ? K_ZERO : K_BOOL);
}
void s_show(Gen &g)
{
    g_self = win(any(g));
    g_args[0] = g.chance(3) ? g.below(2) : g.chance(2) ? 0x100 : g.next();
    g_args[1] = g.chance(3) ? g.next() : g.below(2);
}
void s_title_int(Gen &g) { g_args[0] = g.chance(3) ? g.next() : (DWORD)g.range(-1000, 1000); }
void s_title_float(Gen &g)
{
    float f = g.chance(3) ? (float)g.range(-1000, 1000) / 7.0f : (float)g.range(-50, 50);
    memcpy(&g_args[0], &f, 4);
}
void s_rgb(Gen &g) { g_args[0] = g.below(256), g_args[1] = g.below(256), g_args[2] = g.below(256); }   // (the pixel conversion indexes tables by them: bytes only, as every caller passes)
void s_place(Gen &g)
{
    g_self = child_win(g);
    const DWORD how = g.chance(10) ? g.next() % 40 : g.below(0x18);
    g_args[0] = how;
    BYTE *other = win(any(g));
    g_args[1] = ptrv(other);
    if ((how == 10 || how == 12 || how == 14 || how == 16 || how == 18 || how == 0x15) && !at_(g_self, 0xf8))
        g_args[0] = how - 1;   // (the original reads the previous sibling's slots: there must be one)
    g_args[2] = g.chance(3) ? 0xfffffc18 : (DWORD)g.range(-20, 40);
    g_args[3] = g.chance(3) ? 0xfffffc18 : (DWORD)g.range(-20, 40);
    g_args[4] = g.below(8);
    set_kind_all(39, K_ONE);   // the root is a screen window (mode 22 reads its rectangle)
}
void s_on_rect(Gen &g)
{
    g_self = parent_win(g);
    BYTE *c = win(any(g));
    g_args[0] = g.chance(2) ? ptrv(c + 0x24) : ptrv(xalloc(16));
    g_args[1] = g.chance(3) ? 0 : ptrv(win(any(g)));
    set_kind_all(64, K_BOOL);
}
void s_ini(Gen &g)
{
    g_self = win(any(g));
    g_args[0] = g.chance(3) ? 0 : ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.chance(4) ? g.next() : g.below(2);
    set_kind_all(64, K_VALUE);   // (the loop moves the window while slot 64 answers nonzero: 0 a third of the time)
    fake_reporter(g);
}
void s_texture(Gen &g)
{
    at_(g_self, 0xe0) = g.chance(2) ? ptrv(make_window(12, C_CFRAME)) : 0;
    at_(g_self, 0xe4) = g.chance(2) ? ptrv(make_window(13, C_CFRAME)) : 0;
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.chance(3) ? g.next() : g.below(2);
    fake_reporter(g);
}
void s_draw_title(Gen &g)
{
    const char *const titles[] = {"/\\", "\\/", "><", "&OK", "Load &Game", "", "/", "\\/x", ">", "Te&st"};
    g_self = win(any(g));
    make_text(g_self + 0x50, titles[g.below(10)]);
    g_self[0x59] = g.chance(3) ? 0xff : (BYTE)g.below(5);
    g_self[0x5a] = (BYTE)g.below(4);
    g_self[0x7a] = (BYTE)g.next();
    g_self[0x61] = (BYTE)(g.chance(3) ? 0 : 1);
    if (BYTE *p = at<BYTE *>(at_(g_self, 0xe8)))
        p[0x61] = (BYTE)(g.chance(3) ? 0 : 1);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    g_args[1] = (DWORD)g.range(-20, 600), g_args[2] = (DWORD)g.range(-20, 400);
    g_args[3] = g.chance(4) ? g.next() : g.below(3);
    g_case.argspec[28][0] = A_RECT, g_case.argspec[28][1] = A_RECT;
}

// The member function callback: +4 an object of the block, +8/+0x10 functions of the check (they log their this)
DWORD THISCALL cb_fn1(BYTE *self)
{
    log_event(0x7c31, ptrv(self), 0);
    return g_case.values.next();
}
DWORD THISCALL cb_fn2(BYTE *self)
{
    log_event(0x7c32, ptrv(self), 0);
    return g_case.values.next();
}
void s_callback(Gen &g)
{
    BYTE *cb = xalloc(0x18);
    at_(cb, 0) = 0x5ea350;
    at_(cb, 4) = ptrv(win(any(g)));
    at_(cb, 8) = ptrv((void *)&cb_fn1);
    at_(cb, 0xc) = g.below(0x40);
    at_(cb, 0x10) = g.chance(3) ? 0 : ptrv((void *)&cb_fn2);
    at_(cb, 0x14) = g.below(0x40);
    g_self = cb;
    g_args[0] = g.below(4);
}

// A float result (st0) into the log, the x87 stack emptied
void take_st0()
{
    WORD sw;
    __asm__ volatile("fnstsw %0" : "=m"(sw));
    if ((sw >> 11) & 7) {
        long double v;
        __asm__ volatile("fstpt %0" : "=m"(v) : : "st");
        DWORD w[3] = {0, 0, 0};
        memcpy(w, &v, 10);
        log_event(0x7f10, w[0], w[1] ^ (w[2] << 16));
    }
    for (int k = 0; k < 8; k++) {
        __asm__ volatile("fnstsw %0" : "=m"(sw));
        if (!((sw >> 11) & 7))
            break;
        __asm__ volatile("fstp %%st(0)" : : : "st");
    }
}

const Test TESTS[] = {
    {"Nop4", 0x401110, CALL_THIS, 1, 0, 20, s_value, STUBS, NSTUBS},
    {"IsShown", 0x401120, CALL_THIS, 0, ~0u, 30, s_none, STUBS, NSTUBS},
    {"TreeActive", 0x401130, CALL_THIS, 0, ~0u, 60, s_none, STUBS, NSTUBS},
    {"SetAccel", 0x401150, CALL_THIS, 1, 0xff, 30, s_value, STUBS, NSTUBS},
    {"Accel", 0x401160, CALL_THIS, 0, 0xff, 30, s_none, STUBS, NSTUBS},
    {"TitleText", 0x401170, CALL_THIS, 0, ~0u, 30, s_title_text, STUBS, NSTUBS},
    {"Font", 0x401180, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"SetFont", 0x401190, CALL_THIS, 1, ~0u, 20, s_value, STUBS, NSTUBS},
    {"TitleInt", 0x4011a0, CALL_THIS, 0, ~0u, 40, s_text_slot, STUBS, NSTUBS},
    {"TitleFloat", 0x4011c0, CALL_THIS, 0, 0, 40, s_text_slot, STUBS, NSTUBS, nullptr, &take_st0},
    {"Rect", 0x4011d0, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"False", 0x401200, CALL_THIS, 0, 0xff, 20, s_none, STUBS, NSTUBS},
    {"True", 0x401260, CALL_THIS, 0, 0xff, 20, s_none, STUBS, NSTUBS},
    {"Callback Call", 0x401210, CALL_THIS, 0, ~0u, 40, s_callback, STUBS, NSTUBS},
    {"Callback Test", 0x401220, CALL_THIS, 0, ~0u, 60, s_callback, STUBS, NSTUBS},
    {"Callback DeletingDtor", 0x401240, CALL_THIS, 1, ~0u, 40, s_callback, STUBS, NSTUBS},
    {"Delete", 0x40bba0, CALL_THIS, 0, 0, 40, s_delete, STUBS, NSTUBS},
    {"DrawOutline", 0x40bbe0, CALL_THIS, 2, ~0u, 40, s_outline, STUBS, NSTUBS},
    {"Slot12", 0x40be50, CALL_THIS, 2, ~0u, 30, s_values2, STUBS, NSTUBS},
    {"MouseMove", 0x40bf10, CALL_THIS, 3, ~0u, 600, s_move, STUBS, NSTUBS},
    {"ChildrenSlot22", 0x40c430, CALL_THIS, 1, 0xff, 100, s_children_slot, STUBS, NSTUBS},
    {"ChildrenSlot23", 0x40c480, CALL_THIS, 1, 0xff, 100, s_children_slot, STUBS, NSTUBS},
    {"ChildrenSlot24", 0x40c4d0, CALL_THIS, 1, 0xff, 100, s_children_slot, STUBS, NSTUBS},
    {"ChildrenSlot25", 0x40c520, CALL_THIS, 1, 0xff, 100, s_children_slot, STUBS, NSTUBS},
    {"Show", 0x40c570, CALL_THIS, 2, 0, 300, s_show, STUBS, NSTUBS},
    {"IsVisible", 0x40c5e0, CALL_THIS, 0, ~0u, 100, s_none, STUBS, NSTUBS},
    {"ChildrenSlot29", 0x40c660, CALL_THIS, 0, 0, 60, s_parent, STUBS, NSTUBS},
    {"NextTabStop", 0x40cbb0, CALL_THIS, 0, ~0u, 200, s_child, STUBS, NSTUBS},
    {"PrevTabStop", 0x40cc00, CALL_THIS, 0, ~0u, 200, s_child, STUBS, NSTUBS},
    {"NotifyParent", 0x40cc50, CALL_THIS, 1, ~0u, 60, s_value, STUBS, NSTUBS},
    {"SetTitleInt", 0x40cf00, CALL_THIS, 1, ~0u, 100, s_title_int, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"SetTitleFloat", 0x40cf40, CALL_THIS, 1, ~0u, 100, s_title_float, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"SetColor80", 0x40cf80, CALL_THIS, 3, ~0u, 40, s_rgb, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"SetColor90", 0x40cfb0, CALL_THIS, 3, ~0u, 40, s_rgb, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"SetColorB0", 0x40d030, CALL_THIS, 3, ~0u, 40, s_rgb, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"SetColorC0", 0x40d060, CALL_THIS, 3, ~0u, 40, s_rgb, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"SetColorD0", 0x40d090, CALL_THIS, 3, ~0u, 40, s_rgb, STUBS_REAL_TEXT, NSTUBS_REAL},
    {"Place", 0x40db20, CALL_THIS, 5, 0, 1200, s_place, STUBS, NSTUBS},
    {"ChildOnRect", 0x40e510, CALL_THIS, 2, ~0u, 200, s_on_rect, STUBS, NSTUBS},
    {"LoadIni", 0x40e620, CALL_THIS, 2, 0, 500, s_ini, STUBS, NSTUBS},
    {"LoadTexture", 0x40ea30, CALL_THIS, 2, 0, 100, s_texture, STUBS, NSTUBS},
    {"DrawTitle", 0x40eb40, CALL_THIS, 4, 0, 600, s_draw_title, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_c3_window)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62e5c4, 1);
        g_globals.add(0x6c02cc, 4);
        g_globals.add(0x62e640, 4);
    }
    Tally t{"ui c3 window: CJWindow's shared slots, the mouse move, showing, the focus order, the placement, the INI position, the textures, the title"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &build, 63000);
    t.report();
}

}  // namespace zax
