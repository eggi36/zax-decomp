// Checks of CJScrollBar and the image scroll bar (ui_scrollbar.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): a fake scroll bar (window 0) with random parts, range, position (whole and fractional
// floats) and pressed part under a fake parent (window 7) that may be its owner, a fake owner with a scroll info
// (window 5), fake CFrames as the frame and the images (windows 10, 12, 13, 14); the constructors and destructors on
// real objects. The static zero point 0x716188 and its guard are compared as globals.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_scrollbar(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_scrollbar", &hooks_ui_scrollbar};
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
    {0x4b3430, CV_THIS, 2, K_VALUE, "Frame_DrawRect", "rr"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_Blit"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "Font::Draw", "vvvsvv"},
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_OWNER = 5, W_PARENT_ = 7, W_FRAME_ARG = 10, W_IMG_UP = 12, W_IMG_DOWN = 13, W_IMG_TRACK = 14;

DWORD fbits(float f) { return *reinterpret_cast<DWORD *>(&f); }
float some_float(Gen &g)
{
    if (g.chance(6))
        return 0.0f;
    const float v = (float)g.range(-50, 1500);
    return g.chance(3) ? v / (float)(1 + g.below(7)) : v;
}
void some_rect(Gen &g, BYTE *w, DWORD off)
{
    int *r = &at_<int>(w, off);
    r[0] = g.range(-5, 40), r[1] = g.range(-5, 300), r[2] = r[0] + g.range(-3, 30), r[3] = r[1] + g.range(-3, 300);
}

int g_cls;

void base(Gen &g)
{
    g_case.nwin = 8;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    const int frames[] = {W_FRAME_ARG, W_IMG_UP, W_IMG_DOWN, W_IMG_TRACK};
    for (int i : frames)
        make_window(i, C_CFRAME);
    set_kind(C_CFRAME, 39, K_SMALL);
    set_kind(C_CFRAME, 45, K_SMALL);
    g_case.argspec[23][1] = A_RECT, g_case.argspec[23][2] = A_RECT;   // the frame's blit: source and target
    g_case.argspec[57][0] = A_RECT;
    BYTE *w = make_plain_window(0, g_cls, g);
    if (g.chance(3)) {   // a long bar
        at_<int>(w, 0xc) = at_<int>(w, 4) + 15;
        at_<int>(w, 0x10) = at_<int>(w, 8) + g.range(30, 400);
    }
    for (DWORD o = 0x100; o < 0x140; o += 0x10)
        some_rect(g, w, o);
    at_(w, 0x140) = g.chance(8) ? 2 : g.below(2);
    at_(w, 0x144) = fbits(g.chance(2) ? 0.0f : some_float(g));
    at_(w, 0x148) = fbits(some_float(g));
    at_(w, 0x14c) = fbits(some_float(g));
    at_(w, 0x150) = fbits(g.chance(2) ? 1.0f : g.chance(2) ? 10.0f : some_float(g));
    at_(w, 0x154) = fbits(g.chance(2) ? 10.0f : some_float(g));
    at_(w, 0x158) = fbits(g.chance(4) ? 0.0f : some_float(g) / 100.0f);
    at_(w, 0x15c) = g.chance(2) ? 0 : g.below(6);
    at_(w, 0x160) = g.chance(3) ? 0 : g.chance(2) ? ptrv(win(W_PARENT_)) : ptrv(win(W_OWNER));
    w[0x164] = (BYTE)g.below(2);
    if (g_cls == C_SCROLLBAR2) {
        at_(w, 0x168) = g.chance(3) ? 0 : ptrv(win(W_IMG_UP));
        at_(w, 0x16c) = g.chance(3) ? 0 : ptrv(win(W_IMG_DOWN));
        at_(w, 0x170) = g.chance(2) ? 0 : ptrv(win(W_IMG_UP));
        at_(w, 0x174) = g.chance(2) ? 0 : ptrv(win(W_IMG_DOWN));
        at_(w, 0x178) = g.chance(2) ? 0 : ptrv(win(W_IMG_TRACK));
        w[0x17c] = (BYTE)g.below(2);
        for (DWORD o = 0x180; o < 0x190; o++)
            w[o] = (BYTE)g.next();
    }
    if (!g.chance(5))
        link_child(win(W_PARENT_), w);
    const int owners[] = {W_OWNER, W_PARENT_};
    for (int i : owners)
        if (g.chance(3))
            make_scroll(win(i), g.range(-20, 400), g.range(-20, 400), g.chance(4) ? 0 : g.range(0, 1200), g.chance(4) ? 0 : g.range(0, 1200),
                        g.chance(2), nullptr, nullptr);
    case_global(0x716180, g.chance(4) ? 0 : 1);
    g_self = w;
}

void s_none(Gen &) {}
int *some_point(Gen &g, int lo, int hi)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = g.range(lo, hi), p[1] = g.range(lo, hi);
    return p;
}
int *some_rect_arg(Gen &g)
{
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 600), r[1] = g.range(-50, 400), r[2] = r[0] + g.range(-10, 400), r[3] = r[1] + g.range(-10, 300);
    return r;
}
void s_size(Gen &g) { g_args[0] = (DWORD)g.range(-5, 400), g_args[1] = (DWORD)g.range(-5, 400); }
void s_clip(Gen &g)
{
    int *r = some_rect_arg(g);
    if (g.chance(3)) {   // edges on the bar's screen rect
        r[0] = at_<int>(g_self, 0x24) + g.range(-2, 2);
        r[1] = at_<int>(g_self, 0x28);
    }
    g_args[0] = ptrv(r);
    g_args[1] = ptrv(g.chance(2) ? some_point(g, 0, 0) : some_point(g, -20, 20));
}
void s_frame(Gen &) { g_args[0] = ptrv(win(W_FRAME_ARG)); }
void s_part(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_ARG));
    g_args[1] = ptrv(some_rect_arg(g));
    g_args[2] = ptrv(g_self + 0xb0), g_args[3] = ptrv(g_self + 0xa0);
    g_args[4] = g.below(2);
}
void s_arrow(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_ARG));
    g_args[1] = ptrv(some_rect_arg(g));
    g_args[2] = g.chance(3) ? 0 : 0x1c + g.below(4);
    g_args[3] = g.below(2);
}
void s_button(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_ARG));
    g_args[1] = ptrv(some_rect_arg(g));
    g_args[2] = g.chance(2) ? 0 : g.next();
    g_args[3] = g.chance(3) ? 0 : 0x1c + g.below(4);
    g_args[4] = g.below(2);
}
// A point on one of the bar's parts (screen coordinates for the press)
void s_mouse(Gen &g)
{
    g_args[0] = g.chance(4) ? g.below(3) : 0;
    BYTE *s = at<BYTE *>(at_(g_self, 0xfc));
    const DWORD part = 0x100 + 0x10 * g.below(4);
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(g_self, part) + g.range(-2, 20) + at_<int>(g_self, 0x24) - (s ? at_<int>(s, 0) : 0);
    p[1] = at_<int>(g_self, part + 4) + g.range(-2, 30) + at_<int>(g_self, 0x28) - (s ? at_<int>(s, 4) : 0);
    g_args[1] = ptrv(p);
}
void s_move(Gen &g)
{
    g_args[0] = ptrv(some_point(g, 0, 300));
    g_args[1] = ptrv(some_point(g, -30, 30));
    g_args[2] = ptrv(xalloc(4));
}
void s_wheel(Gen &g)
{
    g_args[0] = (DWORD)g.range(-3, 3);
    g_args[1] = g.next();
}
void s_point(Gen &g)
{
    const DWORD part = 0x100 + 0x10 * g.below(4);
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(g_self, part) + g.range(-2, 20), p[1] = at_<int>(g_self, part + 4) + g.range(-2, 30);
    g_args[0] = ptrv(p);
}
void s_set_rect(Gen &g)
{
    g_args[0] = ptrv(some_rect_arg(g));
    if (g.chance(2)) {   // a window's own bar
        if (!at_(g_self, 0xe8))
            link_child(win(W_PARENT_), g_self);
        at_(g_self, 0x160) = ptrv(win(W_PARENT_));
    }
}
void s_range(Gen &g)
{
    g_args[0] = fbits(some_float(g));
    g_args[1] = fbits(some_float(g));
}
void s_pos(Gen &g)
{
    g_args[0] = g.chance(4) ? at_(g_self, 0x14c) : fbits(some_float(g));
    g_args[1] = g.below(2);
}
void s_range_from(Gen &g) { g_args[0] = ptrv(win(g.chance(2) ? W_OWNER : W_PARENT_)); }
void s_colour(Gen &g)
{
    BYTE *c = xalloc(16);
    for (int i = 0; i < 16; i++)
        c[i] = (BYTE)g.next();
    g_args[0] = ptrv(c);
}
void s_two(Gen &g) { g_args[0] = g.next(), g_args[1] = g.next(); }
void s_one(Gen &g) { g_args[0] = g.next(); }

// ---- Construction and destruction on real objects ---------------------------------------------------------------

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
    g_args[1] = g.chance(6) ? 2 : g.below(2);
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
void destroy() { original<void(THISCALL *)(BYTE *)>(0x40b190)(g_self); }
void s_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(g_cls == C_SCROLLBAR2 ? 0x4144b0 : 0x412ea0)(obj, win(W_PARENT_), g.below(2));
    clear_colour_garbage(obj);
    for (int k = 0; k < (int)g.below(3); k++)
        link_child(obj, win(1 + k));
    g_self = obj;
    g_args[0] = g.below(4);
}

const Test TESTS[] = {
    {"ScrollBar ctor", 0x412ea0, CALL_THIS, 2, ~0u, 200, s_ctor, STUBS, NSTUBS, destroy, clear_garbage},
    {"ScrollBar deleting dtor", 0x412f50, CALL_THIS, 1, ~0u, 150, s_dtor, STUBS, NSTUBS},
    {"ScrollBar SetSize", 0x412f80, CALL_THIS, 2, ~0u, 100, s_size, STUBS, NSTUBS},
    {"ScrollBar ClipRect", 0x412fb0, CALL_THIS, 2, 0xff, 600, s_clip, STUBS, NSTUBS},
    {"ScrollBar Draw", 0x4130f0, CALL_THIS, 1, 0, 200, s_frame, STUBS, NSTUBS},
    {"ScrollBar DrawPart", 0x4131c0, CALL_THIS, 5, 0, 300, s_part, STUBS, NSTUBS},
    {"ScrollBar DrawArrow", 0x413260, CALL_THIS, 4, 0, 200, s_arrow, STUBS, NSTUBS},
    {"ScrollBar DrawThumb", 0x413290, CALL_THIS, 2, 0, 100, s_arrow, STUBS, NSTUBS},
    {"ScrollBar MouseDown", 0x4132b0, CALL_THIS, 2, ~0u, 1000, s_mouse, STUBS, NSTUBS},
    {"ScrollBar MouseUp", 0x413650, CALL_THIS, 2, ~0u, 100, s_mouse, STUBS, NSTUBS},
    {"ScrollBar MouseMove", 0x413680, CALL_THIS, 3, ~0u, 500, s_move, STUBS, NSTUBS},
    {"ScrollBar Wheel", 0x4137a0, CALL_THIS, 2, 0xff, 400, s_wheel, STUBS, NSTUBS},
    {"ScrollBar Cursor", 0x413870, CALL_THIS, 1, ~0u, 400, s_point, STUBS, NSTUBS},
    {"ScrollBar SetRect", 0x413940, CALL_THIS, 1, 0, 800, s_set_rect, STUBS, NSTUBS},
    {"ScrollBar SetRange", 0x413bb0, CALL_THIS, 2, 0, 300, s_range, STUBS, NSTUBS},
    {"ScrollBar SetPos", 0x413c20, CALL_THIS, 2, 0, 500, s_pos, STUBS, NSTUBS},
    {"ScrollBar SetRangeFrom", 0x413ce0, CALL_THIS, 1, 0, 800, s_range_from, STUBS, NSTUBS},
    {"ScrollBar PartAt", 0x413fb0, CALL_THIS, 1, ~0u, 400, s_point, STUBS, NSTUBS},
    {"ScrollBar DrawButton", 0x4140a0, CALL_THIS, 5, 0, 500, s_button, STUBS, NSTUBS},
    {"ScrollBar Layout", 0x414240, CALL_THIS, 0, 0, 300, s_none, STUBS, NSTUBS},
    {"ScrollBar UpdateThumb", 0x414340, CALL_THIS, 0, 0, 800, s_none, STUBS, NSTUBS},
    {"ScrollBar Clamp", 0x414470, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
};
const Test TESTS_IMAGE[] = {
    {"ImageScrollBar ctor", 0x4144b0, CALL_THIS, 2, ~0u, 150, s_ctor, STUBS, NSTUBS, destroy, clear_garbage},
    {"ImageScrollBar deleting dtor", 0x414510, CALL_THIS, 1, ~0u, 100, s_dtor, STUBS, NSTUBS},
    {"ImageScrollBar SetColour", 0x414560, CALL_THIS, 1, 0, 100, s_colour, STUBS, NSTUBS},
    {"ImageScrollBar DrawPart", 0x4145b0, CALL_THIS, 5, 0, 100, s_part, STUBS, NSTUBS},
    {"ImageScrollBar SetUpImages", 0x4145e0, CALL_THIS, 2, 0, 50, s_two, STUBS, NSTUBS},
    {"ImageScrollBar SetDownImages", 0x414600, CALL_THIS, 2, 0, 50, s_two, STUBS, NSTUBS},
    {"ImageScrollBar SetTrackImage", 0x414620, CALL_THIS, 1, 0, 50, s_one, STUBS, NSTUBS},
    {"ImageScrollBar DrawUp", 0x414630, CALL_THIS, 4, 0, 300, s_arrow, STUBS, NSTUBS},
    {"ImageScrollBar DrawDown", 0x4146a0, CALL_THIS, 4, 0, 300, s_arrow, STUBS, NSTUBS},
    {"ImageScrollBar DrawThumb", 0x414710, CALL_THIS, 2, 0, 300, s_arrow, STUBS, NSTUBS},
    {"ImageScrollBar Layout", 0x4147a0, CALL_THIS, 0, 0, 400, s_none, STUBS, NSTUBS},
    {"ImageScrollBar UpdateThumb", 0x414900, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_scrollbar)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62e5e0, 1);
        g_globals.add(0x60a528, 4);
        g_globals.add(0x60a52c, 1);
        g_globals.add(0x62e5c4, 1);
        g_globals.add(0x716180, 1);   // the guard of the static zero point
        g_globals.add(0x716188, 4);
        g_globals.add(0x71618c, 4);
    }
    Tally t{"ui scrollbar: CJScrollBar, the image scroll bar"};
    g_cls = C_SCROLLBAR;
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 10300);
    g_cls = C_SCROLLBAR2;
    run_tests(t, g_hooks, &g_globals, TESTS_IMAGE, sizeof TESTS_IMAGE / sizeof TESTS_IMAGE[0], &base, 10400);
    t.report();
}

}  // namespace zax
