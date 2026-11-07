// Checks of batch C3's screen window, tab control and frame window slots (ui_c3_screen.cpp) against the original
// with the toolkit's harness (checks_ui_common.h): fake windows of the screen window, tab control and frame window
// classes, a fake frame (CFrame) and a fake proxy frame (COffsetClippedProxyFrame), the tab windows plain fakes;
// the bevel, the font's text, the frame helpers, the cursor, the keyboard, the clock, the base class's functions
// they hand over to and atexit as recording stand-ins (the arguments the log does not hold, beyond the fourth,
// logged by the effect); the reporter a fake game object for the key handler; the palette index real.
#include "checks_ui_common.h"
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_c3_screen(Hook *out);

namespace {

using namespace uic;
using scr::object_args;

constinit ModuleHooks g_hooks{"ui_c3_screen", &hooks_ui_c3_screen};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x40a740, CV_FAST, 6, K_VALUE, "Bevel_Draw", "vvrv"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvs"},
    {0x4b3ce0, CV_THIS, 5, K_VALUE, "Frame_TintRectXY"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x524fe0, CV_THIS, 2, K_VALUE, "COffsetClippedProxyFrame_SetOffset"},
    {0x524f50, CV_THIS, 1, K_VALUE, "COffsetClippedProxyFrame_SetClip", "r"},
    {0x4b3430, CV_THIS, 2, K_VALUE, "Frame_DrawRect", "rv"},
    {0x4a66c0, CV_THIS, 6, K_VALUE, "CFont::DrawLines", "vvvs"},
    {0x4098f0, CV_THIS, 5, K_VALUE, "CScreenWindow::ShowTipAccel"},
    {0x40c170, CV_THIS, 1, K_VALUE, "CJWindow::Key"},
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "Platform_GetTickCount"},
    {0x409b30, CV_THIS, 0, K_VALUE, "CScreenWindow::MouseUpdate"},
    {0x518500, CV_THIS, 1, K_VALUE, "Cursor::GetPos", "x"},
    {0x518520, CV_THIS, 0, K_VALUE, "Cursor::KeepWheelFraction"},
    {0x51f450, CV_THIS, 0, K_SMALL, "Cursor::Buttons"},
    {0x518540, CV_THIS, 1, K_BOOL, "Cursor::TakePress"},
    {0x518590, CV_THIS, 1, K_VALUE, "Cursor::SetImage"},
    {0x4e2720, CV_THIS, 0, K_VALUE, "Keys::ReadChar"},
    {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"},
    {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
    {0x40bcc0, CV_THIS, 2, K_VALUE, "CJWindow::MouseDown"},
    {0x40d2a0, CV_THIS, 1, K_VALUE, "CJWindow::SetRect", "r"},
    {0x40a550, CV_THIS, 0, K_VALUE, "CJTabControl::Layout"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

char g_object_args[MAXSLOT + 1];

void effect(int cls, int slot, DWORD *args, DWORD)
{
    Gen &v = g_case.values;
    if (cls == -1) {
        switch ((DWORD)slot) {
        case 0x40a740: log_event(0x7a74, args[2] ^ (args[3] << 8), args[4] ^ (args[5] << 16)); break;   // dark, 1, kind, flag
        case 0x4a6470: log_event(0x7a64, args[4], args[5]); break;   // the alignment and the last
        case 0x4b3ce0: log_event(0x7b3c, args[4], 0); break;         // the colour
        case 0x4a66c0: log_event(0x7a66, args[4], args[5]); break;
        case 0x4098f0: log_event(0x7098, args[4], 0); break;
        case 0x518500: {   // the cursor position through the argument
            int *p = at<int *>(args[0]);
            p[0] = v.range(-20, 700), p[1] = v.range(-20, 500);
            break;
        }
        default: break;
        }
        return;
    }
    if ((cls == C_CFRAME || cls == C_PROXY_FRAME) && slot == 36 && args[0]) {   // the clip rect through the argument
        int *r = at<int *>(args[0]);
        r[0] = v.range(-20, 50), r[1] = v.range(-20, 50);
        r[2] = v.chance(2) ? at_<int>(win(0), 0xc) : v.range(0, 800);
        r[3] = v.range(0, 600);
    } else if (cls == C_SCREEN && slot == 9 && args[2]) {   // a press says whether it was handled
        *at<BYTE *>(args[2]) = (BYTE)v.below(2);
    } else if (slot == 13 && args[2] && cls == C_SCREEN) {   // the cursor asked for after a press
        *at<DWORD *>(args[2]) = v.chance(2) ? 0 : v.next();
    }
}

void colours(BYTE *w, Gen &g)
{
    for (DWORD o = 0x80; o < 0xe0; o += 0x10) {
        for (int k = 0; k < 7; k++)
            w[o + k] = (BYTE)g.next();
        w[o + 0xa] = (BYTE)g.next();
        at_(w, o + 0xc) = g.below(4);
    }
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    g_case.effect = &effect;
    case_global(0x716190, g.below(4));
}

// The frame window (window 0) on a fake frame (window 14)
void s_frame(Gen &g)
{
    BYTE *w = make_plain_window(0, C_FRAME, g);
    colours(w, g);
    at_(w, 0x7c) = ptrv(fake_font(g.chance(4) ? g.next() & 0xffff : g.below(40)));
    make_text(w + 0x50, TEXTS[g.below(8)]);
    w[0x65] = (BYTE)(g.chance(3) ? 1 : 0);
    case_global(0x62e588, g.chance(3) ? 0 : ptrv(make_window(12, C_CFRAME)));
    case_global(0x62e58c, ptrv(make_window(13, C_CFRAME)));
    set_kind(C_FRAME, 30, K_BOOL);
    g_self = w;
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    g_case.argspec[23][1] = A_RECT, g_case.argspec[23][2] = A_RECT;
}

// The screen window (window 0): +0x100 a fake proxy frame (window 13), +0x104 a fake frame (window 14)
BYTE *screen(Gen &g)
{
    BYTE *w = make_plain_window(0, C_SCREEN, g);
    colours(w, g);
    at_(w, 0x100) = ptrv(make_window(13, C_PROXY_FRAME));
    at_(w, 0x104) = ptrv(make_window(14, C_CFRAME));
    make_text(w + 0x118, g.chance(2) ? "" : TEXTS[g.below(8)]);
    make_text(w + 0x11c, TEXTS[g.below(8)]);
    for (int k = 0; k < 4; k++)
        at_(w, 0x120 + 4 * k) = (DWORD)g.range(-20, 600);
    w[0x130] = (BYTE)g.below(2);
    at_(w, 0x134) = g.below(20), at_(w, 0x138) = g.below(100), at_(w, 0x13c) = g.below(100);
    at_(w, 0x114) = ptrv(fake_font(g.below(30)));
    w[0x108] = (BYTE)(g.chance(5) ? 0 : 1);
    at_(w, 0x10c) = g.next();
    at_(w, 0x140) = g.below(700), at_(w, 0x144) = g.below(500);
    for (int i = 0; i < 3; i++) {
        BYTE *e = w + 0x148 + 0x10 * i;
        at_(e, 0) = g.chance(2) ? 0 : ptrv(win(1 + i));
        at_(e, 4) = g.chance(3) ? 0 : g.next();
        at_(e, 8) = g.below(500);
        e[0xc] = (BYTE)g.below(2);
    }
    at_(w, 0x178) = g.below(3);
    g_self = w;
    return w;
}
void s_screen_draw(Gen &g)
{
    screen(g);
    g_args[0] = g.next();
    g_case.argspec[36][0] = A_IGNORE;
    g_case.argspec[28][0] = A_RECT;
}
void s_screen_redraw(Gen &g) { screen(g); }
void s_screen_tip(Gen &g)
{
    screen(g);
    g_args[0] = g.next();
}
void s_screen_key(Gen &g)
{
    screen(g);
    const DWORD k[] = {3, 4, 0x10, 0x11, 0x16, 0x18, 5, 9};
    g_args[0] = g.chance(5) ? g.next() & 0x1ff : k[g.below(8)];
    BYTE *rep = make_window(15, C_OBJECT);
    case_global(0x6c02cc, ptrv(rep));
    object_args(g_object_args, "10:1");
    set_kind(C_OBJECT, 10, K_BOOL);
    set_kind(C_SCREEN, 17, g.chance(2) ? K_ZERO : K_VALUE);
    for (int s = 22; s <= 25; s++)
        set_kind(C_SCREEN, s, K_VALUE);
}
void s_screen_update(Gen &g)
{
    screen(g);
    g_args[0] = g.next();
    case_global(0x62e55c, g.below(8));
    case_global(0x6cff44, g.chance(2) ? 0 : g.next());
    set_kind(C_SCREEN, 9, K_BOOL);
    set_kind(C_SCREEN, 17, g.chance(2) ? K_ZERO : K_VALUE);
    g_case.argspec[15][1] = A_POINT;
    g_case.argspec[9][1] = A_POINT, g_case.argspec[9][2] = A_IGNORE;
    g_case.argspec[10][1] = A_POINT, g_case.argspec[11][1] = A_POINT, g_case.argspec[12][1] = A_POINT;
    g_case.argspec[13][1] = A_POINT, g_case.argspec[13][2] = A_IGNORE;
}

// The tab control (window 0): 0-5 tabs (windows 1-5), their header rects
BYTE *tabs(Gen &g)
{
    BYTE *w = make_plain_window(0, C_TABS, g);
    colours(w, g);
    at_(w, 0x7c) = ptrv(fake_font(g.below(30)));
    w[0x100] = (BYTE)(g.chance(4) ? 0 : 1);
    const DWORD n = g.below(6);
    at_(w, 0x104) = n;
    DWORD *pages = reinterpret_cast<DWORD *>(xalloc(4 * n + 4));
    int *rects = reinterpret_cast<int *>(xalloc(16 * n + 16));
    int x = g.range(0, 5);
    for (DWORD i = 0; i < n; i++) {
        pages[i] = ptrv(win(1 + (int)i));
        rects[4 * i] = x, rects[4 * i + 1] = g.range(0, 3);
        x += g.range(5, 90);
        rects[4 * i + 2] = x, rects[4 * i + 3] = g.range(10, 30);
        x -= g.range(0, 6);
    }
    at_(w, 0x108) = ptrv(pages);
    at_(w, 0x10c) = ptrv(rects);
    at_(w, 0x110) = g.below(30);
    at_(w, 0x114) = n ? g.below(n) : 0;
    at_(w, 0x118) = g.below(8);
    set_kind_all(44, K_TEXT);
    g_self = w;
    return w;
}
void s_tabs_draw(Gen &g)
{
    tabs(g);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
}
void s_tabs_down(Gen &g)
{
    BYTE *w = tabs(g);
    g_args[0] = g.chance(4) ? g.below(3) : 0;
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(w, 0x24) + g.range(-5, 400), p[1] = at_<int>(w, 0x28) + g.range(-5, 40);
    g_args[1] = ptrv(p);
    if (g.chance(2))
        make_scroll(w, g.range(-20, 50), g.range(-20, 50), 0, 0, false, nullptr, nullptr);
}
void s_tabs_rect(Gen &g)
{
    tabs(g);
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 300), r[1] = g.range(-50, 300), r[2] = r[0] + g.range(0, 400), r[3] = r[1] + g.range(0, 300);
    g_args[0] = ptrv(r);
}

const Test TESTS[] = {
    {"FrameWindow DrawFrame", 0x402f80, CALL_THIS, 1, 0, 300, s_frame, STUBS, NSTUBS},
    {"Screen Draw", 0x4090d0, CALL_THIS, 1, 0, 300, s_screen_draw, STUBS, NSTUBS},
    {"Screen Redraw", 0x4092f0, CALL_THIS, 0, 0xff, 60, s_screen_redraw, STUBS, NSTUBS},
    {"Screen HideTip", 0x409310, CALL_THIS, 1, 0xff, 30, s_screen_tip, STUBS, NSTUBS},
    {"Screen Key", 0x4093e0, CALL_THIS, 1, ~0u, 400, s_screen_key, STUBS, NSTUBS},
    {"Screen Update", 0x409560, CALL_THIS, 1, 0, 500, s_screen_update, STUBS, NSTUBS},
    {"Tabs Draw", 0x409cb0, CALL_THIS, 1, 0, 300, s_tabs_draw, STUBS, NSTUBS},
    {"Tabs MouseDown", 0x40a080, CALL_THIS, 2, ~0u, 300, s_tabs_down, STUBS, NSTUBS},
    {"Tabs SetRect", 0x40a420, CALL_THIS, 1, 0, 150, s_tabs_rect, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_c3_screen)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x716190, 1);
        for (DWORD a = 0x7161a0; a < 0x7161b0; a += 4)
            g_globals.add(a, 4);
        g_globals.add(0x62e588, 4);
        g_globals.add(0x62e58c, 4);
        g_globals.add(0x6c02cc, 4);
        g_globals.add(0x62e55c, 4);
        g_globals.add(0x62e5e0, 1);
        g_globals.add(0x6cff58, 4);
        g_globals.add(0x6cff44, 4);
    }
    Tally t{"ui c3 screen: the screen window's drawing, keys and update, the tab control's headers, the frame window's frame"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 64000);
    t.report();
}

}  // namespace zax
