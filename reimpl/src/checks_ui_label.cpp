// Checks of the check box and the text label (ui_label.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): a fake check box or label (window 0) under a fake parent (window 7), drawn on a fake
// CFrame (window 10) with the font's text drawing recorded (the measuring and the wrap layout run in the real
// font); the constructors and destructors on real objects. The blend modes' opacities the check box changes
// while it draws disabled are compared as globals.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_label(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_label", &hooks_ui_label};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
    {0x518570, CV_THIS, 2, K_VALUE, "Cursor::SetPos"},
    {0x518590, CV_THIS, 1, K_VALUE, "Cursor::Set"},
    {0x5186a0, CV_THIS, 1, K_VALUE, "Cursor::Restore", "b"},
    {0x518620, CV_THIS, 0, K_VALUE, "Cursor::Show"},
    {0x5184e0, CV_THIS, 2, K_VALUE, "Cursor::GetPos", "xx"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vssv"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll"},
    {0x4e2760, CV_THIS, 1, K_VALUE, "Keys::ClearChars"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "time"},
    {0x409330, CV_THIS, 3, K_VALUE, "ScreenWindow::StartKeyRepeat"},
    {0x4093c0, CV_THIS, 0, K_VALUE, "ScreenWindow::KeyRepeatDue"},
    {0x4098d0, CV_THIS, 4, K_VALUE, "ScreenWindow::ShowTip", "vsvv"},
    {0x4098f0, CV_THIS, 5, K_VALUE, "ScreenWindow::ShowTipAccel"},
    {0x409b30, CV_THIS, 0, K_VALUE, "ScreenWindow::MouseUpdate"},
    {0x40b620, CV_THIS, 1, K_VALUE, "Window_DrawChildren"},
    {0x4b3ce0, CV_THIS, 5, K_VALUE, "Frame_FillRectChecked"},
    {0x4a66c0, CV_THIS, 6, K_VALUE, "Font::DrawLines", "vvvsvv"},
    {0x4a6ae0, CV_THIS, 12, K_VALUE, "Font::DrawWrapped", "vvvsvv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_PARENT_ = 7, W_FRAME_ARG = 10, W_CALLBACK = 11, W_LOOK = 12;

const char *const LABEL_TEXTS[] = {
    "", "OK", "Load &Game", "Two\nlines", "Three\nlines of\ntext",
    ("A label text long enough to be cut off by a narrow label or wrapped into several lines by a wrapping one"),
    "x", "Zax: The Alien Hunter"};

// The wrapped drawing's arguments beyond the sixth into the log
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == -1 && slot == 0x4a6ae0)
        for (int i = 6; i < 12; i++)
            log_event(0x4a6ae0, (DWORD)i, args[i]);
}

int g_cls;

void base(Gen &g)
{
    g_case.effect = &effect;
    g_case.nwin = 8;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_window(W_FRAME_ARG, C_CFRAME);
    make_window(W_CALLBACK, C_CALLBACK);
    make_window(W_LOOK, C_LOOK);
    BYTE *w = make_plain_window(0, g_cls, g);
    make_text(w + 0x50, LABEL_TEXTS[g.below(8)]);
    if (g_cls == C_CHECKBOX) {
        if (g.chance(4))
            at_(w, 0x7c) = ptrv(fake_font(g.below(40)));
        for (DWORD o = 0x100; o < 0x104; o++)
            w[o] = g.chance(2) ? 0 : g.chance(2) ? 1 : (BYTE)g.next();
        at_(w, 0x104) = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
        at_(w, 0x10c) = g.chance(2) ? 0 : ptrv(win(1));
        at_(w, 0x110) = g.chance(4) ? ptrv(win(W_FRAME_ARG)) : 0;
        make_text(w + 0x11c, "");
        at_(w, 0x120) = (DWORD)-1;
        at_(w, 0x128) = g.chance(2) ? 0 : ptrv(win(W_CALLBACK));
        at_(w, 0x134) = ptrv(win(W_LOOK));
        w[0x138] = (BYTE)g.below(2);
        at_(w, 0x13c) = g.chance(2) ? 0 : ptrv(xalloc(4));
        at_(w, 0x140) = g.below(30);
        case_global(0x60a52d, g.chance(2) ? 0 : 1);
    } else {
        // a copy of the game's font: the wrap layout caches its last text and parameters in the font (+0x48 ...)
        BYTE *font = xalloc(0x84);
        memcpy(font, real_font(), 0x84);
        at_(w, 0x7c) = ptrv(font);
        make_text(w + 0x100, g.chance(2) ? "" : LABEL_TEXTS[g.below(8)]);
        w[0x104] = (BYTE)g.below(2);
        // (a wrap width narrower than a word hangs the font's wrap layout, in the original as well)
        at_(w, 0x108) = g.chance(2) ? (DWORD)-1 : 150 + g.below(250);
        at_(w, 0x10c) = g.chance(2) ? (DWORD)-1 : g.below(100);
        at_(w, 0x110) = g.below(4);
        for (DWORD o = 0x114; o < 0x128; o += 4)
            at_(w, o) = g.chance(2) ? 0 : g.below(4);
    }
    if (!g.chance(5))
        link_child(win(W_PARENT_), w);
    g_self = w;
}

void s_none(Gen &) {}
void s_frame(Gen &) { g_args[0] = ptrv(win(W_FRAME_ARG)); }
void s_frame_xy(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_ARG));
    g_args[1] = (DWORD)g.range(-10, 300), g_args[2] = (DWORD)g.range(-10, 100);
}
void s_title(Gen &g) { g_args[0] = ptrv(LABEL_TEXTS[g.below(8)]); }
void s_any(Gen &g) { g_args[0] = g.chance(2) ? g.below(4) : g.next(); }
void s_font(Gen &g) { g_args[0] = g.chance(3) ? ptrv(fake_font(g.below(40))) : ptrv(real_font()); }
void s_point(Gen &g)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = g.range(-20, 450), p[1] = g.range(-20, 100);
    g_args[0] = ptrv(p);
}

void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void clear_garbage() { clear_colour_garbage(g_self); }
char *text_copy(Gen &g)
{
    char *t = reinterpret_cast<char *>(xalloc(160));
    lstrcpyA(t, LABEL_TEXTS[g.below(8)]);
    return t;
}
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    at_(win(W_PARENT_), 0x4c) = 0;   // the real look factory
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_PARENT_));
    g_args[1] = ptrv(text_copy(g));
    g_args[2] = 0;
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
void s_ctor_string(Gen &g)
{
    s_ctor(g);
    BYTE *z = xalloc(4);
    make_text(z, at<const char *>(g_args[1]));
    g_args[1] = ptrv(z);
}
void destroy_checkbox() { original<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x412350)(g_self, 0); }
void destroy_label() { original<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x412900)(g_self, 0); }
void s_checkbox_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    at_(win(W_PARENT_), 0x4c) = 0;
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, BYTE *)>(0x412280)(obj, win(W_PARENT_), LABEL_TEXTS[g.below(8)], nullptr);
    clear_colour_garbage(obj);
    if (g.chance(2))
        at_(obj, 0x128) = ptrv(win(W_CALLBACK));
    g_self = obj;
    g_args[0] = g.below(4);
}
void s_label_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(obj, win(W_PARENT_), LABEL_TEXTS[g.below(8)]);
    clear_colour_garbage(obj);
    if (g.chance(2))
        at<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(obj + 0x100, LABEL_TEXTS[g.below(8)]);
    g_self = obj;
    g_args[0] = g.below(4);
}

const Test TESTS_CHECKBOX[] = {
    {"CheckBox ctor", 0x412280, CALL_THIS, 3, ~0u, 200, s_ctor, STUBS, NSTUBS, destroy_checkbox, clear_garbage},
    {"CheckBox deleting dtor", 0x412350, CALL_THIS, 1, ~0u, 150, s_checkbox_dtor, STUBS, NSTUBS},
    {"CheckBox Box", 0x412380, CALL_THIS, 3, 0, 200, s_frame_xy, STUBS, NSTUBS},
    {"CheckBox BoxHover", 0x412420, CALL_THIS, 3, ~0u, 100, s_frame_xy, STUBS, NSTUBS},
    {"CheckBox BoxSunken", 0x412440, CALL_THIS, 3, 0, 200, s_frame_xy, STUBS, NSTUBS},
    {"CheckBox CheckMark", 0x4124e0, CALL_THIS, 3, 0, 200, s_frame_xy, STUBS, NSTUBS},
    {"CheckBox Text", 0x412550, CALL_THIS, 1, ~0u, 100, s_frame, STUBS, NSTUBS},
    {"CheckBox Draw", 0x412580, CALL_THIS, 1, 0, 500, s_frame, STUBS, NSTUBS},
    {"CheckBox SetTitle", 0x4126f0, CALL_THIS, 1, 0, 300, s_title, STUBS, NSTUBS},
    {"CheckBox Click", 0x412760, CALL_THIS, 0, 0, 300, s_none, STUBS, NSTUBS},
};
const Test TESTS_LABEL[] = {
    {"Label ctor", 0x4127d0, CALL_THIS, 2, ~0u, 200, s_ctor, STUBS, NSTUBS, destroy_label, clear_garbage},
    {"Label ctor ZString", 0x412930, CALL_THIS, 2, ~0u, 150, s_ctor_string, STUBS, NSTUBS, destroy_label, clear_garbage},
    {"Label deleting dtor", 0x412900, CALL_THIS, 1, ~0u, 150, s_label_dtor, STUBS, NSTUBS},
    {"Label SetAlign", 0x4128c0, CALL_THIS, 1, 0, 50, s_any, STUBS, NSTUBS},
    {"Label SetFont", 0x4128d0, CALL_THIS, 1, 0, 200, s_font, STUBS, NSTUBS},
    {"Label Draw", 0x412a30, CALL_THIS, 1, 0, 500, s_frame, STUBS, NSTUBS},
    {"Label SetTitle", 0x412b70, CALL_THIS, 1, 0, 800, s_title, STUBS, NSTUBS},
    {"Label SetTitleOnly", 0x412d70, CALL_THIS, 1, ~0u, 100, s_title, STUBS, NSTUBS},
    {"Label Tip", 0x412d80, CALL_THIS, 1, 0xff, 500, s_point, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_label)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62e5e0, 1);
        g_globals.add(0x60a528, 4);
        g_globals.add(0x60a52c, 1);
        g_globals.add(0x60a52d, 1);
        g_globals.add(0x62e5c4, 1);
        const DWORD blends[] = {0x6bcc68, 0x6b9728};   // the blend modes' opacity (+0x20 float, +0x24, +0x28)
        for (DWORD b : blends)
            for (DWORD o = 0x20; o <= 0x28; o += 4)
                g_globals.add(b + o, 4);
    }
    Tally t{"ui label: the check box, the text label"};
    g_cls = C_CHECKBOX;
    run_tests(t, g_hooks, &g_globals, TESTS_CHECKBOX, sizeof TESTS_CHECKBOX / sizeof TESTS_CHECKBOX[0], &base, 10100);
    g_cls = C_LABEL;
    run_tests(t, g_hooks, &g_globals, TESTS_LABEL, sizeof TESTS_LABEL / sizeof TESTS_LABEL[0], &base, 10200);
    t.report();
}

}  // namespace zax
