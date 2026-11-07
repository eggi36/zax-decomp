// Checks of the full-screen image (screen_image.cpp) against the original with the toolkit's harness and the
// screens' setup (checks_ui_screens.h): the image, its holder and the frames are fakes (the frames and the
// screen of the CFrame class's recording vtable), the screen frame's fades, lock, unlock and flip, the tick
// count, the keyboard, the message pump, the frame functions and the overlay options' drawing are stand-ins;
// the list "Frame Overlay Options" 0x6bf9b8 gets a fake vtable and records of the case.
#include "checks_ui_screens.h"

namespace zax {

int hooks_screen_image(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"screen_image", &hooks_screen_image};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x4d2460, CV_CDECL, 0, K_VALUE, "Tick"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll"},
    {0x4e2900, CV_THIS, 0, K_VALUE, "Keys::TakeAnyPressed"},
    {0x5998f0, CV_CDECL, 0, K_VALUE, "App_PumpMessages"},
    {0x568630, CV_THIS, 5, K_VALUE, "Screen_FadeOut"},
    {0x568660, CV_THIS, 7, K_VALUE, "Screen_FadeIn"},
    {0x567bb0, CV_THIS, 0, K_VALUE, "Screen_Lock"},
    {0x567c90, CV_THIS, 0, K_VALUE, "Screen_Unlock"},
    {0x5693a0, CV_THIS, 0, K_VALUE, "Screen_Flip"},
    {0x55d030, CV_CDECL, 0, K_FIXED1, "ColorDepth"},
    {0x416760, CV_THIS, 2, K_FIXED2, "Object_Clone"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4b52e0, CV_THIS, 1, K_VALUE, "CFrameOverlayOptions::Draw"},
    {0x433a80, CV_CDECL, 0, K_VALUE, "GamePalette_Get"},
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4a9b60, CV_THIS, 3, K_VALUE, "Frame_LoadFile", "svv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7901, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7903, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    return 0xf0000000u | (rec & 0xffff);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void records(Gen &g, DWORD list)
{
    const DWORD n = g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    case_global(list, ptrv(g_list_vt));
    case_global(list + 0x30, g.chance(8) ? 0 : ptrv(recs));
    case_global(list + 0x34, n);
}

constexpr int W_FRAME = W_OBJECT, W_CLONE = W_OTHER, W_FRAME2 = W_OTHER2;

// A frame's slot 36 writes its rectangle through its argument
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_CFRAME && slot == 36 && args[0]) {
        Gen &v = g_case.values;
        int *r = at<int *>(args[0]);
        r[0] = v.range(-50, 100), r[1] = v.range(-50, 100), r[2] = r[0] + v.range(0, 700), r[3] = r[1] + v.range(0, 500);
    } else if (cls == -1 && (DWORD)slot == 0x4a8a90 && in_block(answer)) {   // a new frame: the fake frame class
        at_(at<BYTE *>(answer), 0) = at_(win(W_FRAME), 0);
    } else if (cls == -1) {
        if (const int t = template_of((DWORD)slot))
            made(answer, t);
    }
}

BYTE *g_image;

BYTE *holder(Gen &g, int frame_win)
{
    if (g.chance(6))
        return nullptr;
    BYTE *h = xalloc(8);
    at_(h, 0) = g.chance(6) ? 0 : ptrv(win(frame_win));
    h[4] = g.chance(2) ? 1 : 0;
    return h;
}

BYTE *image(Gen &g)
{
    BYTE *s = xalloc(0x3c);
    for (DWORD o = 0; o < 0x3c; o += 4)
        at_(s, o) = g.next();
    at_(s, 0) = g.chance(3) ? 0 : g.next();
    at_<float>(s, 4) = g.chance(4) ? 3.0f : (float)g.range(0, 60) / 7.0f;
    at_(s, 0x2c) = ptrv(holder(g, W_FRAME2));
    s[0x30] = g.chance(2) ? 1 : 0;
    return s;
}

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[1] = ptrv((void *)&list_name);
        g_list_vt[37] = ptrv((void *)&list_load);
    }
    screen_base(g, &effect);
    make_window(W_SCREEN, C_CFRAME);
    make_window(W_FRAME, C_CFRAME);
    make_window(W_CLONE, C_CFRAME);
    make_window(W_FRAME2, C_CFRAME);
    g_case.argspec[36][0] = A_IGNORE;
    set_kind(C_CFRAME, 29, K_FIXED0);
    set_kind(C_CFRAME, 31, K_VALUE);
    const DWORD depths[3] = {8, 16, 24};
    g_case.fixed[0] = depths[g.below(3)];
    g_case.fixed[1] = g.chance(2) ? g_case.fixed[0] : depths[g.below(3)];
    g_case.fixed[2] = ptrv(win(W_CLONE));   // (a null clone crashes both sides in Layout)
    records(g, 0x6bf9b8);
    g_image = image(g);
    g_self = g_image;
}

void s_free(Gen &g)
{
    g_self = holder(g, W_FRAME);
    if (!g_self)
        g_self = xalloc(8);
}
void s_none(Gen &) {}
void s_layout(Gen &g)
{
    if (!at_(g_self, 0x2c))
        at_(g_self, 0x2c) = ptrv(xalloc(8));
    if (!at_(at<BYTE *>(at_(g_self, 0x2c)), 0))
        at_(at<BYTE *>(at_(g_self, 0x2c)), 0) = ptrv(win(W_FRAME2));
    g_args[0] = ptrv(win(W_SCREEN));
    (void)g;
}
void s_show(Gen &g)
{
    g_args[0] = ptrv(holder(g, W_FRAME));
    g_args[1] = g.chance(3) ? 0 : ptrv(win(W_PARENT));
}
void s_show_file(Gen &g)
{
    const char *const files[4] = {"Screens/Intro.frm16", "Load.frm", "Screens\\Title", "x.y.frm16"};
    g_args[0] = g.chance(6) ? 0 : ptrv(files[g.below(4)]);
    g_args[1] = g.chance(2) ? 0 : ptrv(win(W_PARENT));
}
void s_show_frame(Gen &g)
{
    g_args[0] = g.chance(6) ? 0 : ptrv(win(W_FRAME));
    g_args[1] = g.chance(3) ? 0 : ptrv(win(W_PARENT));
}
void s_blank(Gen &g)
{
    g_args[0] = g.chance(6) ? 0 : ptrv(win(W_FRAME));
    g_args[1] = g.next();
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6bf9b8, 0x6bf9e8, 0x6bf9ec};

const Test TESTS[] = {
    {"ScreenImage FreeFrame", 0x5788f0, CALL_THIS, 0, 0, 50, s_free, STUBS, NSTUBS},
    {"ScreenImage Init", 0x578910, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"ScreenImage Clear", 0x578930, CALL_THIS, 0, 0, 50, s_none, STUBS, NSTUBS},
    {"ScreenImage Layout", 0x578970, CALL_THIS, 1, 0, 200, s_layout, STUBS, NSTUBS},
    {"ScreenImage Show", 0x578a70, CALL_THIS, 2, 0, 400, s_show, STUBS, NSTUBS},
    {"ScreenImage ShowFile", 0x578cb0, CALL_THIS, 2, 0, 300, s_show_file, STUBS, NSTUBS},
    {"ScreenImage ShowFrame", 0x578e70, CALL_THIS, 2, 0, 100, s_show_frame, STUBS, NSTUBS},
    {"ScreenImage WaitAndHide", 0x578eb0, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"ScreenImage Wait", 0x578fa0, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"ScreenImage Blank", 0x579030, CALL_FAST, 2, 0, 100, s_blank, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(screen_image)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"screen image: the full-screen image's show, wait, fades and layout"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 44000);
    t.report();
}

}  // namespace zax
