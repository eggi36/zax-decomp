// Checks of the buttons and the resize grip (ui_button.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): a fake button (window 0) in a radio ring of fake buttons (windows 1-3) under a fake
// parent (window 7), with a fake look (window 12, also the look factory), callback (11) and images (CFrame fakes
// 10 and 13); the constructors and destructors on real objects in the block (the real look factory, the
// allocators as stand-ins).
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_button(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_button", &hooks_ui_button};
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
    {0x4098d0, CV_THIS, 4, K_VALUE, "ScreenWindow::ShowTip"},
    {0x4098f0, CV_THIS, 5, K_VALUE, "ScreenWindow::ShowTipAccel"},
    {0x409b30, CV_THIS, 0, K_VALUE, "ScreenWindow::MouseUpdate"},
    {0x40b620, CV_THIS, 1, K_VALUE, "Window_DrawChildren"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "Font::Draw", "vvvsvv"},
    {0x546830, CV_THIS, 2, K_BOOL, "Polygon_Contains", "vv"},
    {0x545cf0, CV_THIS, 0, K_VALUE, "Polygon_Dtor"},
    // the image loading: the frame's constructor recorded (made a fake), the filters and the load
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4a8a30, CV_CDECL, 0, K_VALUE, "FrameFilters"},
    {0x4b20e0, CV_THIS, 1, K_VALUE, "FrameFilterList_Remove"},
    {0x4a9a30, CV_THIS, 5, K_BOOL, "CFrame::Load", "svvvv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// Windows: 0 the window under test, 1-3 the radio ring, 7 the parent, 10 and 13 images, 11 the callback, 12 the
// look and its factory
constexpr int W_PARENT_ = 7, W_IMAGE = 10, W_CALLBACK = 11, W_LOOK = 12, W_IMAGE2 = 13;

void effect(int cls, int slot, DWORD *, DWORD answer)
{
    if (cls == -1 && slot == 0x4a8a90 && in_block(answer))
        at_(at<BYTE *>(answer), 0) = at_(win(W_IMAGE), 0);
}

int g_cls;

void base(Gen &g)
{
    g_case.effect = &effect;
    g_case.nwin = 8;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    for (int i = 1; i <= 3; i++)
        make_plain_window(i, C_BUTTON, g);
    make_plain_window(W_PARENT_, g_cls == C_RESIZE ? C_FRAME : C_WINDOW, g);
    make_window(W_IMAGE, C_CFRAME);
    make_window(W_IMAGE2, C_CFRAME);
    make_window(W_CALLBACK, C_CALLBACK);
    make_window(W_LOOK, C_LOOK);
    set_kind(C_CFRAME, 39, K_SMALL);
    set_kind(C_CFRAME, 45, K_SMALL);
    g_case.argspec[83][0] = A_POINT;
    g_case.argspec[70][0] = A_POINT;
    BYTE *w = make_plain_window(0, g_cls, g);
    if (g.chance(5))
        at_(w, 0x7c) = ptrv(fake_font(g.below(40)));
    make_text(w + 0x50, TEXTS[g.below(8)]);
    for (DWORD o = 0x100; o < 0x104; o++)
        w[o] = g.chance(2) ? 0 : g.chance(2) ? 1 : (BYTE)g.next();
    at_(w, 0x104) = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
    at_(w, 0x108) = g.next();
    // the radio ring: 0 -> 1 -> ... -> k -> 0, alone (0) or a ring of one
    const int k = (int)g.below(4);
    if (k) {
        at_(w, 0x10c) = ptrv(win(1));
        for (int i = 1; i <= k; i++)
            at_(win(i), 0x10c) = ptrv(i < k ? win(i + 1) : w);
        for (int i = 1; i <= k; i++)
            at_(win(i), 0x104) = g.chance(2) ? 0 : 1;
    } else {
        at_(w, 0x10c) = g.chance(4) ? ptrv(w) : 0;
    }
    at_(w, 0x110) = g.chance(2) ? 0 : ptrv(win(W_IMAGE));
    at_(w, 0x114) = g.next();
    w[0x118] = (BYTE)g.below(2), w[0x119] = (BYTE)g.below(2), w[0x11a] = (BYTE)g.below(2);
    make_text(w + 0x11c, g.chance(2) ? "" : TEXTS[g.below(8)]);
    at_(w, 0x120) = g.chance(2) ? (DWORD)-1 : g.below(6);
    w[0x124] = (BYTE)g.below(2);
    at_(w, 0x128) = g.chance(2) ? 0 : ptrv(win(W_CALLBACK));
    w[0x12c] = (BYTE)g.next();
    at_(w, 0x130) = g.chance(3) ? ptrv(xalloc(16)) : 0;
    at_(w, 0x134) = ptrv(win(W_LOOK));
    at_(w, 0x4c) = ptrv(win(W_LOOK));
    if (!g.chance(5))
        link_child(win(W_PARENT_), w);
    case_global(0x60a52d, g.chance(2) ? 0 : 1);
    g_self = w;
}

void s_none(Gen &) {}
void s_any(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_frame(Gen &) { g_args[0] = ptrv(win(W_IMAGE2)); }
int *point_near(Gen &g, BYTE *w)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(w, 0x14) + g.range(-10, 300);
    p[1] = at_<int>(w, 0x18) + g.range(-10, 200);
    return p;
}
void s_point(Gen &g) { g_args[0] = ptrv(point_near(g, g_self)); }
void s_mouse(Gen &g)
{
    g_args[0] = g.chance(3) ? g.below(3) : 0;
    g_args[1] = ptrv(point_near(g, g_self));
}
void s_move(Gen &g)
{
    g_args[0] = ptrv(point_near(g, g_self));
    g_args[1] = g.next(), g_args[2] = g.next();
}
void s_drag(Gen &g)
{
    g_args[0] = ptrv(point_near(g, g_self));
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-40, 40), d[1] = g.range(-40, 40);
    g_args[1] = ptrv(d);
    g_args[2] = g.chance(4) ? g.below(8) : 1;
    g_args[3] = ptrv(xalloc(4));
    if (!at_(g_self, 0xe8))   // the grip resizes its parent (the original needs one)
        link_child(win(W_PARENT_), g_self);
}
void s_key(Gen &g)
{
    const DWORD keys[] = {0x20, 11, 12, 9, 13};
    g_args[0] = g.chance(4) ? g.below(0x100) : keys[g.below(5)];
    if (!at_(g_self, 0xe8))   // keys 11 and 12 in a ring focus through the parent (the original needs one too)
        link_child(win(W_PARENT_), g_self);
}
void s_init(Gen &g) { g_args[0] = g.chance(2) ? 0 : ptrv(win(1 + (int)g.below(3))); }
void s_load(Gen &g)
{
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.below(3);
    g_args[2] = g.chance(2) ? 0 : g.next();
}
void s_set_image(Gen &g)
{
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_IMAGE2));
    g_args[1] = g.chance(2) ? 0 : g.next();
    g_args[2] = g.chance(2) ? 0 : g.next();
}
void s_tip(Gen &g)
{
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.chance(2) ? (DWORD)-1 : g.below(6);
}

// ---- Construction and destruction on real objects --------------------------------------------------------------

void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void clear_garbage() { clear_colour_garbage(g_self); }
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    at_(win(W_PARENT_), 0x4c) = 0;   // the parent without a look: the real look factory
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_PARENT_));
    char *t = reinterpret_cast<char *>(xalloc(64));
    lstrcpyA(t, TEXTS[g.below(8)]);
    g_args[1] = ptrv(t);
    g_args[2] = g.chance(2) ? 0 : ptrv(win(1 + (int)g.below(3)));
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
void s_ctor_string(Gen &g)
{
    s_ctor(g);
    BYTE *z = xalloc(4);
    make_text(z, at<const char *>(g_args[1]));
    g_args[1] = ptrv(z);
}
void s_grip_ctor(Gen &g)
{
    s_ctor(g);
    g_args[1] = 0;
    g_args[2] = 0;
}
void destroy_button() { original<void(THISCALL *)(BYTE *)>(0x40fb80)(g_self); }
void destroy_grip() { original<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x40f8b0)(g_self, 0); }
void s_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    at_(win(W_PARENT_), 0x4c) = 0;
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, BYTE *)>(0x40f9b0)(obj, win(W_PARENT_), TEXTS[g.below(8)], nullptr);
    clear_colour_garbage(obj);
    if (g.chance(2))
        at_(obj, 0x128) = ptrv(win(W_CALLBACK));
    if (g.chance(3))
        at_(obj, 0x130) = ptrv(xalloc(16));
    if (g.chance(2))
        at_(obj, 0x110) = ptrv(win(W_IMAGE));
    obj[0x119] = (BYTE)g.below(2);
    for (int k = 0; k < (int)g.below(3); k++)
        link_child(obj, win(4 + k));
    g_self = obj;
    g_args[0] = g.below(4);
}
void s_grip_dtor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x40f870)(obj, win(W_PARENT_));
    clear_colour_garbage(obj);
    g_self = obj;
    g_args[0] = g.below(4);
}

const Test TESTS[] = {
    {"Button SetTarget", 0x40f9f0, CALL_THIS, 1, 0, 50, s_any, STUBS, NSTUBS},
    {"Button Activate", 0x40fa00, CALL_THIS, 0, 0xff, 100, s_none, STUBS, NSTUBS},
    {"Button Checked", 0x40fa20, CALL_THIS, 0, ~0u, 50, s_none, STUBS, NSTUBS},
    {"Button Init", 0x40faa0, CALL_THIS, 1, 0, 300, s_init, STUBS, NSTUBS},
    {"Button Draw", 0x40fc20, CALL_THIS, 1, 0, 100, s_frame, STUBS, NSTUBS},
    {"Button UpdateLook", 0x40fc50, CALL_THIS, 0, ~0u, 50, s_none, STUBS, NSTUBS},
    {"Button MouseDown", 0x40fc60, CALL_THIS, 2, ~0u, 300, s_mouse, STUBS, NSTUBS},
    {"Button MouseUp", 0x40fcb0, CALL_THIS, 2, ~0u, 300, s_mouse, STUBS, NSTUBS},
    {"Button Slot12", 0x40fd00, CALL_THIS, 2, ~0u, 50, s_mouse, STUBS, NSTUBS},
    {"Button MouseMove", 0x40fd10, CALL_THIS, 3, ~0u, 400, s_move, STUBS, NSTUBS},
    {"Button Key", 0x40fd80, CALL_THIS, 1, ~0u, 500, s_key, STUBS, NSTUBS},
    {"Button Tip", 0x40fe30, CALL_THIS, 1, 0xff, 400, s_point, STUBS, NSTUBS},
    {"Button SetChecked", 0x40feb0, CALL_THIS, 1, 0, 400, s_any, STUBS, NSTUBS},
    {"Button TakeClick", 0x40ff10, CALL_THIS, 0, ~0u, 50, s_none, STUBS, NSTUBS},
    {"Button UpdateEnabled", 0x40ff30, CALL_THIS, 0, 0, 100, s_none, STUBS, NSTUBS},
    {"Button LoadImage", 0x40ff50, CALL_THIS, 3, 0, 300, s_load, STUBS, NSTUBS},
    {"Button SetImage", 0x40ffd0, CALL_THIS, 3, 0, 400, s_set_image, STUBS, NSTUBS},
    {"Button SetTip", 0x4100c0, CALL_THIS, 2, 0, 100, s_tip, STUBS, NSTUBS},
    {"Button Set114", 0x4100f0, CALL_THIS, 1, 0, 50, s_any, STUBS, NSTUBS},
    {"Button Hit", 0x410100, CALL_THIS, 1, 0xff, 300, s_point, STUBS, NSTUBS},
    {"Button ClickSound", 0x410150, CALL_THIS, 0, 0, 50, s_none, STUBS, NSTUBS},
    {"Button Click", 0x410160, CALL_THIS, 0, 0, 50, s_none, STUBS, NSTUBS},
    {"Button DoClick", 0x410180, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"Button ClearTip", 0x4101c0, CALL_THIS, 1, 0, 100, s_any, STUBS, NSTUBS},
    // construction and destruction
    {"Button ctor", 0x40f9b0, CALL_THIS, 3, ~0u, 200, s_ctor, STUBS, NSTUBS, destroy_button, clear_garbage},
    {"Button ctor ZString", 0x40fa60, CALL_THIS, 3, ~0u, 150, s_ctor_string, STUBS, NSTUBS, destroy_button, clear_garbage},
    {"Radio ctor", 0x4109a0, CALL_THIS, 3, ~0u, 150, s_ctor, STUBS, NSTUBS, destroy_button, clear_garbage},
    {"Radio ctor ZString", 0x4109e0, CALL_THIS, 3, ~0u, 100, s_ctor_string, STUBS, NSTUBS, destroy_button, clear_garbage},
    {"Toggle ctor", 0x410ab0, CALL_THIS, 3, ~0u, 150, s_ctor, STUBS, NSTUBS, destroy_button, clear_garbage},
    {"Button deleting dtor", 0x40fa30, CALL_THIS, 1, ~0u, 200, s_dtor, STUBS, NSTUBS},
    {"Button dtor", 0x40fb80, CALL_THIS, 0, 0, 150, s_dtor, STUBS, NSTUBS},
    {"Grip ctor", 0x40f870, CALL_THIS, 1, ~0u, 150, s_grip_ctor, STUBS, NSTUBS, destroy_grip, clear_garbage},
    {"Grip deleting dtor", 0x40f8b0, CALL_THIS, 1, ~0u, 100, s_grip_dtor, STUBS, NSTUBS},
};
const Test TESTS_RADIO[] = {
    {"Radio Activate", 0x410a20, CALL_THIS, 0, 0xff, 200, s_none, STUBS, NSTUBS},
    {"Radio Click", 0x410a40, CALL_THIS, 0, 0, 300, s_none, STUBS, NSTUBS},
};
const Test TESTS_TOGGLE[] = {
    {"Toggle Click", 0x410af0, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"Toggle ClickSound", 0x412750, CALL_THIS, 0, 0, 50, s_none, STUBS, NSTUBS},
};
const Test TESTS_GRIP[] = {
    {"Grip Draw", 0x40f8e0, CALL_THIS, 1, 0, 100, s_frame, STUBS, NSTUBS},
    {"Grip MouseDrag", 0x40f910, CALL_THIS, 4, 0xff, 400, s_drag, STUBS, NSTUBS},
    {"Grip Cursor", 0x40f9a0, CALL_THIS, 1, ~0u, 50, s_point, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_button)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62e5e0, 1);
        g_globals.add(0x60a528, 4);
        g_globals.add(0x60a52c, 1);
        g_globals.add(0x60a52d, 1);
        g_globals.add(0x62e5c4, 1);
    }
    Tally t{"ui button: the buttons, the resize grip"};
    g_cls = C_BUTTON;
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 9600);
    g_cls = C_BUTTON2;
    run_tests(t, g_hooks, &g_globals, TESTS_RADIO, sizeof TESTS_RADIO / sizeof TESTS_RADIO[0], &base, 9700);
    g_cls = C_BUTTON3;
    run_tests(t, g_hooks, &g_globals, TESTS_TOGGLE, sizeof TESTS_TOGGLE / sizeof TESTS_TOGGLE[0], &base, 9800);
    g_cls = C_RESIZE;
    run_tests(t, g_hooks, &g_globals, TESTS_GRIP, sizeof TESTS_GRIP / sizeof TESTS_GRIP[0], &base, 9900);
    t.report();
}

}  // namespace zax
