// Checks of CJSlider (ui_slider.cpp) against the original with the toolkit's harness (checks_ui_common.h): a fake
// slider (window 0) with random value, count and step under a fake parent; the drawing against a fake CFrame
// (window 10) with the frame's fill and rectangle functions recorded; the constructor on a real object.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_slider(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_slider", &hooks_ui_slider};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
    {0x518570, CV_THIS, 2, K_VALUE, "Cursor::SetPos"},
    {0x518590, CV_THIS, 1, K_VALUE, "Cursor::Set"},
    {0x5186a0, CV_THIS, 1, K_VALUE, "Cursor::Restore", "b"},
    {0x518620, CV_THIS, 0, K_VALUE, "Cursor::Show"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vssv"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll"},
    {0x4e2760, CV_THIS, 1, K_VALUE, "Keys::ClearChars"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "time"},
    {0x409330, CV_THIS, 3, K_VALUE, "ScreenWindow::StartKeyRepeat"},
    {0x4093c0, CV_THIS, 0, K_VALUE, "ScreenWindow::KeyRepeatDue"},
    {0x4098d0, CV_THIS, 4, K_VALUE, "ScreenWindow::ShowTip"},
    {0x4098f0, CV_THIS, 5, K_VALUE, "ScreenWindow::ShowTipAccel"},
    {0x409b30, CV_THIS, 0, K_VALUE, "ScreenWindow::MouseUpdate"},
    {0x4b3ce0, CV_THIS, 5, K_VALUE, "Frame_FillRectChecked"},
    {0x4b3430, CV_THIS, 2, K_VALUE, "Frame_DrawRect", "rv"},
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_FRAME_ARG = 10, W_PARENT_ = 7;

DWORD thumb(BYTE *w) { return at<DWORD(THISCALL *)(BYTE *)>(0x4a6140)(at<BYTE *>(at_(w, 0x7c))); }

void base(Gen &g)
{
    g_case.nwin = 8;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_window(W_FRAME_ARG, C_CFRAME);
    BYTE *w = make_plain_window(0, C_SLIDER, g);
    if (g.chance(3))
        at_(w, 0x7c) = ptrv(fake_font(g.below(60)));
    make_text(w + 0x50, TEXTS[g.below(8)]);
    const DWORD n = g.chance(5) ? g.below(3) : g.below(60);
    at_(w, 0x104) = n;
    at_(w, 0x100) = g.chance(8) ? g.next() : g.below(n + 1);
    at_(w, 0x108) = g.chance(4) ? g.next() : 1 + g.below(10);
    if (!g.chance(5))
        link_child(win(W_PARENT_), w);
    g_case.argspec[23][1] = A_RECT, g_case.argspec[23][2] = A_RECT;   // the frame's slot 23: source and target rects
    g_self = w;
}

void s_none(Gen &) {}
void s_frame(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_ARG));
    g_args[1] = g.next();
    case_global(0x716190, g.chance(4) ? 0 : 1);
}
void s_point(Gen &g)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = g.range(-20, 450), p[1] = g.range(-20, 100);
    g_args[0] = ptrv(p);
}
void s_drag(Gen &g)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(g_self, 0x24) + g.range(-40, 450), p[1] = g.range(-20, 100);
    g_args[0] = ptrv(p);
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-40, 40), d[1] = g.range(-40, 40);
    g_args[1] = ptrv(d);
    g_args[2] = g.chance(4) ? g.below(8) : 1;
    g_args[3] = ptrv(xalloc(4));
    // the drag divides by width - thumb - 1 (both sides would fault on 0)
    const int span = at_<int>(g_self, 0xc) - at_<int>(g_self, 4) - (int)thumb(g_self) - 1;
    if (span == 0)
        at_<int>(g_self, 0xc) += 1 + (int)g.below(100);
}
void s_wheel(Gen &g)
{
    g_args[0] = (DWORD)g.range(-3, 3);
    g_args[1] = g.next();
}
void s_key(Gen &g)
{
    const DWORD keys[] = {5, 11, 12, 14, 9};
    g_args[0] = g.chance(4) ? g.below(0x40) : keys[g.below(5)];
}
void s_set(Gen &g)
{
    g_args[0] = g.chance(3) ? at_(g_self, 0x100) : g.below(60);
    g_args[1] = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_range(Gen &g)
{
    g_args[0] = g.chance(8) ? g.next() : g.below(200);
    g_args[1] = g.chance(2) ? 0 : g.below(30);
}

void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void clear_garbage() { clear_colour_garbage(g_self); }
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_PARENT_));
    g_args[1] = g.chance(8) ? g.next() : g.below(200);
    g_args[2] = g.chance(2) ? 0 : g.below(30);
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
void destroy() { original<void(THISCALL *)(BYTE *)>(0x40b190)(g_self); }

const Test TESTS[] = {
    {"Slider ctor", 0x4101f0, CALL_THIS, 3, ~0u, 200, s_ctor, STUBS, NSTUBS, destroy, clear_garbage},
    {"Slider Value", 0x4102c0, CALL_THIS, 0, ~0u, 50, s_none, STUBS, NSTUBS},
    {"Slider Draw", 0x4102d0, CALL_THIS, 1, 0, 600, s_frame, STUBS, NSTUBS},
    {"Slider DrawFocus", 0x4105a0, CALL_THIS, 2, 0, 300, s_frame, STUBS, NSTUBS},
    {"Slider MouseDrag", 0x410650, CALL_THIS, 4, 0xff, 600, s_drag, STUBS, NSTUBS},
    {"Slider Wheel", 0x410700, CALL_THIS, 2, ~0u, 100, s_wheel, STUBS, NSTUBS},
    {"Slider Key", 0x410730, CALL_THIS, 1, ~0u, 500, s_key, STUBS, NSTUBS},
    {"Slider SetValue", 0x410850, CALL_THIS, 2, ~0u, 300, s_set, STUBS, NSTUBS},
    {"Slider SetRange", 0x410880, CALL_THIS, 2, 0, 200, s_range, STUBS, NSTUBS},
    {"Slider Cursor", 0x4108c0, CALL_THIS, 1, ~0u, 500, s_point, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_slider)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62e5e0, 1);
        g_globals.add(0x60a528, 4);
        g_globals.add(0x60a52c, 1);
        g_globals.add(0x62e5c4, 1);
        g_globals.add(0x716190, 1);   // the guard of the static rect 0x7161a0
        for (DWORD o = 0; o < 16; o += 4)
            g_globals.add(0x7161a0 + o, 4);
    }
    Tally t{"ui slider: CJSlider"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 10000);
    t.report();
}

}  // namespace zax
