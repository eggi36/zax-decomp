// Checks of the sound and video configuration dialogs (ui_settings.cpp) against the original with the toolkit's
// harness and the screens' setup (checks_ui_screens.h). The constructors run on real objects of the extra area;
// the screen (the reporter's +4) is a game object with a resolution list of the case; never touched: the INI file
// (GetInt, GetFloat, WriteInt, WriteFloat stand-ins), the display mode (0x567f20 a stand-in that never refuses a
// mode: a refusal ends the game), the gamma (0x569520), the depth names 0x713bf0 (their Add a stand-in, the count
// a case value), the depth
// 0x713bd0 (a case value).
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_settings(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_settings", &hooks_ui_settings};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "asv"},
    {0x4038a0, CV_THIS, 0, K_VALUE, "CFrameWindow::AddCloseButton"},
    {0x412280, CV_THIS, 3, 'e', "CheckBox::CheckBox", "vsv"},
    {0x40ff50, CV_THIS, 3, K_VALUE, "Button::LoadImage", "ssv"},
    {0x4109a0, CV_THIS, 3, 'e', "RadioButton::RadioButton", "vsv"},
    {0x4a4670, CV_FAST, 2, K_SMALL, "Ini_GetInt", "vvsv"},
    {0x4a46e0, CV_FAST, 2, K_FLOAT, "Ini_GetFloat", "vvsv"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vvsv"},
    {0x4a4740, CV_FAST, 2, K_VALUE, "Ini_WriteFloat", "vvsv"},
    {0x57aef0, CV_CDECL, 0, K_FIXED0, "ResourceCache_Get"},
    {0x40d0f0, CV_THIS, 1, K_SMALL, "CJWindow::RightOf"},
    {0x40d100, CV_THIS, 2, K_VALUE, "CJWindow::SpreadWidth"},
    {0x40a940, CV_THIS, 2, 'e', "CJWindow::CJWindow(ZString)"},
    {0x54f470, CV_THIS, 1, K_VALUE, "FieldNameList::Add", "s"},
    {0x408cd0, CV_THIS, 3, 'e', "ComboBox2::ComboBox2"},
    {0x408bc0, CV_THIS, 2, K_VALUE, "ComboBox::SelectId"},
    {0x410880, CV_THIS, 2, K_VALUE, "CJSlider::SetRange"},
    {0x409be0, CV_THIS, 1, 'e', "CJTabControl::CJTabControl"},
    {0x40a2f0, CV_THIS, 1, K_VALUE, "CJTabControl::Add"},
    {0x40a4b0, CV_THIS, 2, K_VALUE, "CJTabControl::Select"},
    {0x403b30, CV_THIS, 3, 'e', "CMessageBox::CMessageBox", "ssv"},
    {0x40c6e0, CV_THIS, 2, K_VALUE, "CJWindow::AddModalToParent"},
    {0x567f20, CV_THIS, 4, K_VALUE, "Screen_SetMode", "aavv"},
    {0x4d24c0, CV_FAST, 0, K_VALUE, "ShowErrorBox", "ss"},
    {0x569520, CV_THIS, 1, K_VALUE, "Screen_SetGamma"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);


void effect(int cls, int slot, DWORD *, DWORD answer)
{
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x4109a0:
    case 0x412280: made(answer, W_BUTTON_T); return;
    case 0x40a940:
    case 0x409be0:
    case 0x403b30: made(answer, W_PLAIN_T); return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

void base(Gen &g)
{
    screen_base(g, &effect);
    object_args(g_object_args, "0:1,1:0,13:0,23:2");
    // the screen: brightness +0xd0, pixel format +0xcc, card +0x44/+0x50, the resolutions (+0x4c of 0xc bytes
    // from +0x54)
    BYTE *s = win(W_SCREEN);
    const float b = (float)g.range(0, 40) / 20.0f;
    memcpy(s + 0xd0, &b, 4);
    s[0xcc] = g.chance(2), s[0x50] = g.chance(2);
    at_(s, 0x44) = g.below(4);
    const DWORD n = g.below(5);
    at_(s, 0x4c) = n;
    for (DWORD i = 0; i < n; i++)
        at_(s, 0x54 + i * 0xc) = g.chance(2) ? 640 : 800, at_(s, 0x58 + i * 0xc) = g.chance(2) ? 480 : 600;
    set_kind(C_OBJECT, 45, K_FIXED1);
    set_kind(C_OBJECT, 39, K_FIXED2);
    set_kind(C_OBJECT, 29, K_FIXED3);
    set_kind(C_OBJECT, 13, K_BOOL);
    g_case.fixed[1] = g.chance(2) ? 640 : 800;
    g_case.fixed[2] = g.chance(2) ? 480 : 600;
    g_case.fixed[3] = g.chance(2) ? 0x10 : 8;
    set_kind(C_OBJECT, 1, K_FIXED5);   // (the world's object: a game object, its slot 13 has no arguments)
    g_case.fixed[5] = ptrv(win(W_OBJECT));
    BYTE *cache = xalloc(0x40);
    cache[0x30] = g.chance(2);
    at_(cache, 8) = g.chance(2) ? 5 : 3;
    g_case.fixed[0] = ptrv(cache);
    // the sliders' value: a resolution of the list (the notify reads it without a test)
    set_kind(C_SLIDER, 77, K_FIXED4);
    g_case.fixed[4] = n ? g.below(n) : 0;
    const DWORD depths[4] = {8, 0x10, 0x18, 0x20};
    case_global(0x713bd0, depths[g.below(4)]);
    case_global(0x713bfc, g.below(3));
    at_(win(W_REPORTER), 0x24) = depths[g.below(4)];
}

void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    (void)g;
}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
// The video dialog under test (window 0)
void video(Gen &g)
{
    BYTE *w = make_plain_window(0, C_DIALOG, g);
    members(g, w, 0x134, 0x168);
    at_(w, 0x138) = ptrv(win(W_SLIDER_T)), at_(w, 0x150) = ptrv(win(W_LABEL_T)), at_(w, 0x160) = ptrv(win(W_COMBO_T));
    at_(w, 0x16c) = ptrv(win(W_BUTTON_T));
    at_(w, 0x14c) = ptrv(win(W_OTHER));
    make_plain_window(W_OTHER, C_SLIDER, g);
    const DWORD n = at_(win(W_SCREEN), 0x4c);
    at_(w, 0x170) = n;
    at_(w, 0x174) = n ? g.below(n) : 0;
    at_(w, 0x178) = n ? g.below(n) : 0;
    at_(w, 0x17c) = g.chance(2) ? *at<DWORD *>(0x713bd0) : 0x10;
    set_kind(C_COMBO, 76, K_SMALL);
    g_self = w;
}
void s_video_dtor(Gen &g)
{
    video(g);
    s_flags(g);
}
void s_notify(Gen &g)
{
    video(g);
    const DWORD from[5] = {at_(g_self, 0x16c), at_(g_self, 0x138), at_(g_self, 0x14c), at_(g_self, 0x160), 0};
    g_args[0] = from[g.below(5)];
    g_args[1] = g.chance(3) ? 0 : g.chance(4) ? 2 : 1;
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x713bd0, 0x713bfc};

const Test TESTS[] = {
    {"SoundDialog ctor", 0x57efd0, CALL_THIS, 1, ~0u, 200, s_ctor, STUBS, NSTUBS},
    {"SoundDialog deleting dtor", 0x57f840, CALL_THIS, 1, ~0u, 30, s_flags, STUBS, NSTUBS},
    {"VideoDialog ctor", 0x591020, CALL_THIS, 1, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"VideoDialog deleting dtor", 0x591720, CALL_THIS, 1, ~0u, 100, s_video_dtor, STUBS, NSTUBS},
    {"VideoDialog dtor", 0x591750, CALL_THIS, 0, 0, 200, video, STUBS, NSTUBS},
    {"VideoDialog Notify", 0x5918c0, CALL_THIS, 2, 0, 300, s_notify, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_settings)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui settings: the sound and video configuration dialogs"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 29000);
    t.report();
}

}  // namespace zax
