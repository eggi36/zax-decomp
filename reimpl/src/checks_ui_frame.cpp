// Checks of the frame windows, dialogs and the message box (ui_frame.cpp) against the original with the toolkit's
// harness (checks_ui_common.h): a fake frame window, dialog or message box (window 0) under a fake parent
// (window 7), with fake close button, resize grip and toggle (windows 1-3), replaced window and default and
// cancel buttons (windows 4-6); the constructors and destructors on real objects in the block. What the
// functions construct (buttons, the grip, the label) comes from the allocator stand-in with recorded
// constructors that give it the fake vtable of its class.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_frame(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_frame", &hooks_ui_frame};
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
    // what the frame windows construct: the constructors recorded (each returns its object, made a fake)
    {0x40f9b0, CV_THIS, 3, 'e', "Button::Button", "vsv"},
    {0x40f870, CV_THIS, 1, 'e', "ResizeControl::ResizeControl"},
    {0x4127d0, CV_THIS, 2, 'e', "TextLabel::TextLabel", "vs"},
    {0x4100c0, CV_THIS, 2, K_VALUE, "Button::SetTip", "sv"},
    {0x40c7c0, CV_THIS, 6, K_VALUE, "Window_AddChild"},
    // the drawing of the children (proxy frames and the dirty-rect cache, Rendering): recorded
    {0x40b620, CV_THIS, 1, K_VALUE, "Window_DrawChildren"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// Windows of the case: 0 the window under test, 1 close button, 2 resize grip, 3 toggle, 4 replaced window,
// 5 default button, 6 cancel button, 7 parent, 8 another dialog, 9 its replaced window, 10 a frame, 12-14 the
// fakes the constructors' objects take the vtable of (label, grip, button)
constexpr int W_FRAME_ARG = 10, W_LABEL = 12, W_GRIP = 13, W_BUTTON = 14;

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    Gen &v = g_case.values;
    if (cls == -1) {
        if (!in_block(answer))
            return;
        if (slot == 0x40f9b0)
            at_(at<BYTE *>(answer), 0) = at_(win(W_BUTTON), 0);
        else if (slot == 0x40f870)
            at_(at<BYTE *>(answer), 0) = at_(win(W_GRIP), 0);
        else if (slot == 0x4127d0)
            at_(at<BYTE *>(answer), 0) = at_(win(W_LABEL), 0);
        return;
    }
    // slot 57 (the rect) of the window under test may enlarge the rect (its minimum size)
    if (slot == 57 && cls >= C_FRAME && cls <= C_DIALOG && readable(at<void *>(args[0]), 16) && v.chance(2)) {
        int *r = at<int *>(args[0]);
        r[2] += v.range(0, 30), r[3] += v.range(0, 30);
    } else if (slot == 14 && readable(at<void *>(args[3]), 4)) {   // a cursor out
        *at<DWORD *>(args[3]) = v.chance(2) ? 0 : v.next();
    }
}

int g_cls;   // the class of window 0

void base(Gen &g)
{
    g_case.effect = &effect;
    g_case.nwin = 10;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_plain_window(1, C_BUTTON, g);
    make_plain_window(2, C_RESIZE, g);
    make_plain_window(3, C_BUTTON, g);
    make_plain_window(5, C_BUTTON, g);
    make_plain_window(6, C_BUTTON, g);
    make_plain_window(8, C_DIALOG, g);
    make_window(W_FRAME_ARG, C_CFRAME);
    make_window(W_LABEL, C_LABEL);
    make_window(W_GRIP, C_RESIZE);
    make_window(W_BUTTON, C_BUTTON);
    BYTE *w = make_plain_window(0, g_cls, g);
    if (g.chance(4))
        at_(w, 0x7c) = ptrv(fake_font(g.below(40)));
    make_text(w + 0x50, TEXTS[g.below(8)]);
    at_(win(2), 0x100) = g.chance(3) ? 0 : g.below(16);
    at_(w, 0x100) = g.chance(3) ? 0 : ptrv(win(1));
    at_(w, 0x104) = g.chance(3) ? 0 : ptrv(win(2));
    at_(w, 0x108) = g.chance(3) ? 0 : ptrv(win(3));
    w[0x10c] = g.chance(3) ? 0 : g.chance(2) ? 1 : (BYTE)g.next();
    at_(w, 0x110) = g.below(300), at_(w, 0x114) = g.below(300);
    if (g_cls != C_FRAME) {
        at_(w, 0x118) = g.chance(3) ? 0 : ptrv(win(g_cls == C_MESSAGE_BOX ? 5 : 4));
        at_(w, 0x11c) = g.chance(3) ? 0 : ptrv(win(5));
        at_(w, 0x120) = g.chance(3) ? 0 : ptrv(win(6));
        for (DWORD o = 0x124; o < 0x134; o += 4)
            at_(w, o) = (DWORD)g.range(-20, 600);
    }
    at_(win(8), 0x118) = g.chance(2) ? 0 : ptrv(win(9));
    if (!g.chance(4))
        link_child(win(7), w);
    const int kids[] = {11, 1, 15, 3};
    for (int k = 0; k < (int)g.below(5); k++)
        link_child(w, win(kids[k]));
    if (g.chance(3))
        make_scroll(w, g.range(-20, 50), g.range(-20, 50), g.range(0, 500), g.range(0, 500), g.chance(2), nullptr, nullptr);
    case_global(0x60a52d, g.chance(2) ? 0 : 1);
    g_self = w;
}

void s_none(Gen &) {}
void s_any(Gen &g) { g_args[0] = g.chance(2) ? 0 : g.next(); }
int *point_near(Gen &g, BYTE *w)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(w, 0x24) + g.range(-10, 300);
    p[1] = at_<int>(w, 0x28) + g.range(-10, 200);
    return p;
}
void s_drag(Gen &g)
{
    g_args[0] = ptrv(point_near(g, g_self));
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-40, 40), d[1] = g.range(-40, 40);
    g_args[1] = ptrv(d);
    g_args[2] = g.chance(4) ? g.below(8) : 1;
    g_args[3] = ptrv(xalloc(4));
}
void s_mouse(Gen &g)
{
    g_args[0] = g.below(3);
    g_args[1] = ptrv(point_near(g, g_self));
    g_args[2] = ptrv(xalloc(4));
}
void s_point(Gen &g) { g_args[0] = ptrv(point_near(g, g_self)); }
void s_notify(Gen &g)
{
    const DWORD pick[] = {at_(g_self, 0x100), at_(g_self, 0x108), at_(g_self, 0x118), ptrv(win(4)), 0};
    g_args[0] = pick[g.below(5)];
    g_args[1] = g.chance(3) ? g.below(5) : 0;
}
void s_rect(Gen &g)
{
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 600), r[1] = g.range(-50, 400), r[2] = r[0] + g.range(-10, 400), r[3] = r[1] + g.range(-10, 300);
    g_args[0] = ptrv(r);
}
void s_resize_to(Gen &g)
{
    s_rect(g);
    g_args[1] = g.below(16);
    g_args[2] = ptrv(point_near(g, g_self));
}
void s_frame_arg(Gen &) { g_args[0] = ptrv(win(W_FRAME_ARG)); }
void s_key(Gen &g)
{
    const DWORD keys[] = {6, 10, 11, 12, 13, 27, 0xfa, 9};
    g_args[0] = g.chance(4) ? g.below(0x100) : keys[g.below(8)];
}
void s_hand_over(Gen &g)
{
    if (!at_(g_self, 0xe8))
        link_child(win(7), g_self);
    g_args[0] = ptrv(win(8));
    g_args[1] = g.below(3);
}
void s_replaced(Gen &g) { g_args[0] = g.chance(3) ? 0 : ptrv(win(4)); }

// ---- Construction and destruction on real objects (the colours' index bytes are stack garbage: cleared) ----

void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void clear_garbage() { clear_colour_garbage(g_self); }
const char *some_title(Gen &g)
{
    char *t = reinterpret_cast<char *>(xalloc(64));
    lstrcpyA(t, TEXTS[g.below(8)]);
    return t;
}
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(4) ? 0 : ptrv(win(7));
    g_args[1] = ptrv(some_title(g));
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
const char *const BOX_TEXTS[] = {
    "", "Saved.", "Two\nlines",
    ("A message box text long enough to be wrapped at five hundred pixels by the font of the toolkit, with some "
     "more words after that so the label gets several lines")};
void s_box_ctor(Gen &g)
{
    s_ctor(g);
    g_args[2] = g_args[0];
    g_args[0] = ptrv(some_title(g));
    g_args[1] = ptrv(BOX_TEXTS[g.below(4)]);
}
void destroy_frame() { original<void(THISCALL *)(BYTE *)>(0x402f00)(g_self); }
void destroy_dialog() { original<void(THISCALL *)(BYTE *)>(0x403d70)(g_self); }
// A frame window built by the original constructor, as a dialog with a replaced window, under window 7 or none
void built(Gen &g, DWORD vtable)
{
    BYTE *obj = xalloc(0x200);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x402d80)(obj, win(7), TEXTS[g.below(8)]);
    clear_colour_garbage(obj);
    at_(obj, 0) = vtable;
    at_(obj, 0xe8) = g.chance(3) ? 0 : ptrv(win(7));
    if (at_(obj, 0xe8) && g.chance(2))
        link_child(win(7), win(9));   // the parent's focused child (modal or not)
    for (int k = 0; k < (int)g.below(3); k++)
        link_child(obj, win(1 + k));
    if (vtable != 0x5ea5f8) {
        at_(obj, 0x118) = g.chance(3) ? 0 : ptrv(win(4));
        for (DWORD o = 0x11c; o < 0x134; o += 4)
            at_(obj, o) = g.below(500);
    }
    g_self = obj;
    g_args[0] = g.below(4);
}
void s_frame_dtor(Gen &g) { built(g, 0x5ea5f8); }
void s_base_dtor(Gen &g) { built(g, 0x5ea72c); }
void s_box_dtor(Gen &g) { built(g, 0x5ea990); }
void s_dialog_dtor(Gen &g) { built(g, 0x5eaac4); }

const Test TESTS[] = {
    // the frame window (window 0 a fake of the frame window class)
    {"Frame ParentCursor", 0x402cf0, CALL_THIS, 1, 0xff, 100, s_any, STUBS, NSTUBS},
    {"FrameBase Slot22", 0x402d00, CALL_THIS, 1, 0xff, 100, s_any, STUBS, NSTUBS},
    {"FrameBase Slot23", 0x402d20, CALL_THIS, 1, 0xff, 100, s_any, STUBS, NSTUBS},
    {"FrameBase Slot24", 0x402d40, CALL_THIS, 1, 0xff, 100, s_any, STUBS, NSTUBS},
    {"FrameBase Slot25", 0x402d60, CALL_THIS, 1, 0xff, 100, s_any, STUBS, NSTUBS},
    {"Frame IsActive", 0x402e70, CALL_THIS, 0, ~0u, 50, s_none, STUBS, NSTUBS},
    {"Frame Draw", 0x402f30, CALL_THIS, 1, 0, 200, s_frame_arg, STUBS, NSTUBS},
    {"Frame MouseDrag", 0x403260, CALL_THIS, 4, 0xff, 800, s_drag, STUBS, NSTUBS},
    {"Frame MouseDown", 0x403470, CALL_THIS, 2, ~0u, 300, s_mouse, STUBS, NSTUBS},
    {"Frame Mouse9", 0x4034c0, CALL_THIS, 3, 0xff, 300, s_mouse, STUBS, NSTUBS},
    {"Frame MouseUp", 0x403530, CALL_THIS, 2, ~0u, 300, s_mouse, STUBS, NSTUBS},
    {"Frame Cursor", 0x403550, CALL_THIS, 1, ~0u, 400, s_point, STUBS, NSTUBS},
    {"Frame Notify", 0x4035f0, CALL_THIS, 2, 0, 400, s_notify, STUBS, NSTUBS},
    {"Frame SetRect", 0x403640, CALL_THIS, 1, 0, 500, s_rect, STUBS, NSTUBS},
    {"Frame ResizeTo", 0x403720, CALL_THIS, 3, 0, 600, s_resize_to, STUBS, NSTUBS},
    {"Frame AddClose", 0x4038a0, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"Frame AddResize", 0x403930, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"Frame BorderHeight", 0x403a80, CALL_THIS, 0, ~0u, 200, s_none, STUBS, NSTUBS},
    // construction and destruction
    {"Frame ctor", 0x402d80, CALL_THIS, 2, ~0u, 200, s_ctor, STUBS, NSTUBS, destroy_frame, clear_garbage},
    {"Frame deleting dtor", 0x402e80, CALL_THIS, 1, ~0u, 150, s_frame_dtor, STUBS, NSTUBS},
    {"FrameBase deleting dtor", 0x402ed0, CALL_THIS, 1, ~0u, 100, s_base_dtor, STUBS, NSTUBS},
    {"Frame dtor", 0x402f00, CALL_THIS, 0, 0, 100, s_frame_dtor, STUBS, NSTUBS},
    {"Dialog2 ctor", 0x403ab0, CALL_THIS, 2, ~0u, 150, s_ctor, STUBS, NSTUBS, destroy_dialog, clear_garbage},
    {"Dialog ctor", 0x403d00, CALL_THIS, 2, ~0u, 150, s_ctor, STUBS, NSTUBS, destroy_dialog, clear_garbage},
    {"MessageBox ctor", 0x403b30, CALL_THIS, 3, ~0u, 200, s_box_ctor, STUBS, NSTUBS, destroy_frame, clear_garbage},
    {"MessageBox deleting dtor", 0x403c60, CALL_THIS, 1, ~0u, 100, s_box_dtor, STUBS, NSTUBS},
    {"Dialog deleting dtor", 0x403d40, CALL_THIS, 1, ~0u, 150, s_dialog_dtor, STUBS, NSTUBS},
    {"Dialog dtor", 0x403d70, CALL_THIS, 0, 0, 250, s_dialog_dtor, STUBS, NSTUBS},
};
// dialog and message box functions (window 0 a fake of that class)
const Test TESTS_DIALOG[] = {
    {"Dialog2 Draw", 0x403af0, CALL_THIS, 1, 0, 100, s_frame_arg, STUBS, NSTUBS},
    {"Dialog Key", 0x403e40, CALL_THIS, 1, ~0u, 600, s_key, STUBS, NSTUBS},
    {"Dialog HandOver", 0x404000, CALL_THIS, 2, 0, 400, s_hand_over, STUBS, NSTUBS},
    {"Dialog ReplaceWindow", 0x4040c0, CALL_THIS, 2, 0, 400, s_hand_over, STUBS, NSTUBS},
    {"Dialog SetReplaced", 0x404180, CALL_THIS, 1, 0, 300, s_replaced, STUBS, NSTUBS},
};
const Test TESTS_BOX[] = {
    {"MessageBox Notify", 0x403cb0, CALL_THIS, 2, 0, 200, s_notify, STUBS, NSTUBS},
    {"MessageBox Key", 0x403cd0, CALL_THIS, 1, ~0u, 300, s_key, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_frame)
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
    Tally t{"ui frame: frame windows, dialogs, the message box"};
    g_cls = C_FRAME;
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 9300);
    g_cls = C_DIALOG;
    run_tests(t, g_hooks, &g_globals, TESTS_DIALOG, sizeof TESTS_DIALOG / sizeof TESTS_DIALOG[0], &base, 9400);
    g_cls = C_MESSAGE_BOX;
    run_tests(t, g_hooks, &g_globals, TESTS_BOX, sizeof TESTS_BOX / sizeof TESTS_BOX[0], &base, 9500);
    t.report();
}

}  // namespace zax
