// Checks of the controls configuration dialog, the key button, the "save the changes?" question and the credits
// text (ui_controls.cpp) against the original with the toolkit's harness and the screens' setup
// (checks_ui_screens.h). The constructors run on real objects of the extra area; the configuration is a block of
// the case behind stand-ins (its texts, bindings, joystick); the tab control is a fake with fake pages; the INI
// file, the joysticks, the time, the file reader, the font and the frame are stand-ins. The credits text's 256 KB
// of lines live in a buffer of this file (operator new's stand-in gives it), hashed into the log after the call.
// The file reader and the colour are locals of the caller (their methods as fastcall stand-ins whose ecx is not
// compared).
// The key buttons get the real vtable 0x5effb0 from the constructor: its slots the constructor reaches
// (SetTitle 0x40cdc0, UpdateLook 0x40fc50, slot 86 0x471220) are stand-ins.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_controls(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_controls", &hooks_ui_controls};
Globals g_globals;

#define CONTROLS_STUBS \
    UIC_SCREEN_STUBS, {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "asa"}, {0x40fb80, CV_THIS, 0, K_VALUE, "Button::~Button"}, \
        {0x470b60, CV_THIS, 2, K_VALUE, "ControlConfig::BindingText", "vx"}, {0x4d2460, CV_CDECL, 0, K_VALUE, "Time_Ms"}, \
        {0x40fc20, CV_THIS, 1, K_VALUE, "Button::Draw"}, {0x40ccd0, CV_THIS, 0, K_WINDOW, "CJWindow::Screen"}, \
        {0x4098b0, CV_THIS, 0, K_VALUE, "CScreenWindow::HideCursor"}, {0x410160, CV_THIS, 0, K_VALUE, "Button::Click"}, \
        {0x46fa40, CV_THIS, 0, K_VALUE, "ControlConfig::SaveCopy2"}, {0x4038a0, CV_THIS, 0, K_VALUE, "CFrameWindow::AddCloseButton"}, \
        {0x409be0, CV_THIS, 1, 'e', "CJTabControl::CJTabControl"}, {0x470d00, CV_THIS, 1, K_TEXT, "ControlConfig::IniWord2"}, \
        {0x40a4a0, CV_THIS, 1, K_FIXED1, "CJTabControl::Find", "s"}, {0x40a2f0, CV_THIS, 1, K_VALUE, "CJTabControl::Add"}, \
        {0x470ce0, CV_THIS, 1, K_TEXT, "ControlConfig::IniWord1"}, {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"}, \
        {0x40da30, CV_THIS, 1, K_VALUE, "CJWindow::ShowScrollBars"}, {0x40a520, CV_THIS, 0, K_VALUE, "CJTabControl::FitAll"}, \
        {0x40a4b0, CV_THIS, 2, K_VALUE, "CJTabControl::Select"}, {0x412280, CV_THIS, 3, 'e', "CheckBox::CheckBox", "vsv"}, \
        {0x4a4670, CV_FAST, 2, K_VALUE, "Ini_GetInt", "vvsv"}, {0x408cd0, CV_THIS, 3, 'e', "ComboBox2::ComboBox2"}, \
        {0x408b50, CV_THIS, 1, K_VALUE, "ComboBox::SetList"}, {0x408bc0, CV_THIS, 2, K_VALUE, "ComboBox::SelectId"}, \
        {0x46ffa0, CV_THIS, 0, K_VALUE, "ControlConfig::CheckJoystick"}, {0x46fb00, CV_THIS, 0, K_BOOL, "ControlConfig::Changed"}, \
        {0x40c6e0, CV_THIS, 2, K_VALUE, "CJWindow::AddModalToParent"}, {0x4dd920, CV_CDECL, 0, K_SMALL, "Joystick_Count"}, \
        {0x470e50, CV_THIS, 0, K_SMALL, "ControlConfig::JoystickId"}, {0x4dd940, CV_FAST, 0, K_BOOL, "Joystick_Answers", "vx"}, \
        {0x4dd930, CV_FAST, 0, K_VALUE, "Joystick_HasOemName", "vx"}, {0x4a1780, CV_FAST, 0, K_VALUE, "FileBuf_Ctor", "xx"}, \
        {0x4a1840, CV_FAST, 2, K_BOOL, "FileBuf_Open", "xxsv"}, {0x4a20a0, CV_FAST, 2, K_BOOL, "FileBuf_ReadLine", "xxvv"}, \
        {0x4a2100, CV_FAST, 0, K_VALUE, "FileBuf_Close", "xx"}, {0x4a1800, CV_FAST, 0, K_VALUE, "FileBuf_Dtor", "xx"}, \
        {0x433c20, CV_THIS, 0, K_FLOAT, "CBlendMode_GetOpacity"}, {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"}, \
        {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"}, {0x53f000, CV_FAST, 0, K_VALUE, "FieldColor_Ctor", "xx"}, \
        {0x53f080, CV_FAST, 4, K_VALUE, "FieldColor_SetRGBA", "xxvvvv"}, {0x4b3a70, CV_THIS, 5, K_VALUE, "Frame_FillRectXY", "vvvvx"}, \
        {0x40cdc0, CV_THIS, 1, K_VALUE, "CJWindow::SetTitle", "s"}, {0x40fc50, CV_THIS, 0, K_VALUE, "Button::UpdateLook"}, \
        {0x471220, CV_THIS, 0, K_VALUE, "KeyButton slot 86"}

const Stub STUBS[] = {
    CONTROLS_STUBS,
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);
// The credits text: operator new gives the big buffer
const Stub STUBS_CREDITS[] = {
    CONTROLS_STUBS,
    {0x5d033a, CV_CDECL, 1, K_FIXED2, "operator new (the credits' lines)"},
};
constexpr int NSTUBS_CREDITS = sizeof STUBS_CREDITS / sizeof STUBS_CREDITS[0];

constexpr int W_TABS = 11, W_CHECK = 12, W_PAGE = 14, W_FRAME_ = 13;
constexpr DWORD LINES = 0x3e800;
alignas(16) BYTE g_lines[LINES];

const char *const CREDITS[8] = {"Programming:Someone", "_Heading", "+Another heading", "/a comment", "Plain line",
                                "a:b:c", "", "::"};

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    // a page's AddChild makes it the child's parent (the constructor reads the key button's parent; a new page
    // has the real vtable 0x5ea72c, whose AddChild is the stand-in 0x40c7c0)
    if ((cls == C_WINDOW && slot == 31) || (cls == -1 && (DWORD)slot == 0x40c7c0)) {
        if (in_block(args[0]))
            at_(at<BYTE *>(args[0]), 0xe8) = g_effect_ecx;
        return;
    }
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x46ec30: ini_effect(args, answer); return;
    case 0x409be0: made(answer, W_TABS); return;
    case 0x412280: made(answer, W_CHECK); return;
    case 0x470b60:   // the binding text (action, ZString out)
        if (answer & 1)
            at<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(args[1], TEXTS[(answer >> 1) & 7]);
        return;
    case 0x4a20a0: {   // a line of the file (buffer, size)
        const char *t = CREDITS[(answer >> 8) & 7];
        char *out = at<char *>(args[0]);
        int i = 0;
        for (; t[i] && i < 255; i++)
            out[i] = t[i];
        out[i] = 0;
        return;
    }
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

void base(Gen &g)
{
    screen_base(g, &effect);
    object_args(g_object_args, "0:1");
    // the tab control with its pages (each a plain window with a first and a last child)
    BYTE *tabs = make_window(W_TABS, C_TABS);
    const DWORD pages = g.below(3);
    BYTE *d = xalloc(4 * (pages + 1));
    for (DWORD i = 0; i < pages; i++) {
        BYTE *p = make_plain_window(W_PAGE + (int)i, C_WINDOW, g);
        at_(p, 0xec) = ptrv(win(W_BUTTON_T));
        at_(p, 0xf0) = ptrv(win(W_LABEL_T));
        at_(p, 0xfc) = g.chance(2) ? 0 : ptrv(xalloc(0x1c));
        at_(d, 4 * i) = ptrv(p);
    }
    at_(tabs, 0x104) = pages, at_(tabs, 0x108) = ptrv(d);
    make_window(W_CHECK, C_CHECKBOX);
    g_case.fixed[1] = g.chance(3) ? 0 : ptrv(win(W_PAGE));
    set_kind(C_WINDOW, 65, K_BOOL);
    set_kind(C_TABS, 58, K_RECT);
    set_kind(C_WINDOW, 58, K_RECT);
    set_kind(C_LABEL, 58, K_RECT);
    set_kind(C_CFRAME, 45, K_SMALL);
    set_kind(C_CFRAME, 39, K_VALUE);
    set_kind(C_CFRAME, 29, K_SMALL);
    case_global(0x62e5e0, g.below(2));
}

// The configuration: +0x3c the number of actions
BYTE *config(Gen &g)
{
    BYTE *c = xalloc(0x40);
    at_(c, 0x3c) = g.below(6);
    return c;
}
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(6) ? 0 : ptrv(win(W_PARENT));
    g_args[1] = ptrv(config(g));
}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
// The key button under test (window 0)
void key_button(Gen &g)
{
    BYTE *w = make_plain_window(0, C_BUTTON, g);
    at_(w, 0x138) = ptrv(win(W_LABEL_T));
    at_(w, 0x13c) = g.chance(3) ? 0 : ptrv(config(g));
    at_(w, 0x140) = g.below(6);
    w[0x144] = g.chance(2);
    g_self = w;
}
void s_kb_frame(Gen &g)
{
    key_button(g);
    g_args[0] = g.next();
}
void s_kb_dtor(Gen &g)
{
    key_button(g);
    s_flags(g);
}
// The controls dialog under test (window 0)
void dialog(Gen &g)
{
    BYTE *w = make_plain_window(0, C_DIALOG, g);
    at_(w, 0x134) = ptrv(config(g));
    at_(w, 0x138) = g.chance(4) ? 0 : ptrv(xalloc(0x20));
    at_(w, 0x15c) = g.chance(4) ? 0 : ptrv(xalloc(0x20));
    at_(w, 0x160) = g.chance(4) ? 0 : ptrv(win(W_OBJECT));
    at_(w, 0x164) = g.chance(4) ? 0 : ptrv(win(W_COMBO_T));
    g_self = w;
}
void s_dlg_dtor(Gen &g)
{
    dialog(g);
    s_flags(g);
}
void s_question(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    g_args[1] = g.next();
}
void s_names_dtor(Gen &g)
{
    g_self = xalloc(0x20);
    s_flags(g);
}
// The credits text
void s_credits_ctor(Gen &g)
{
    memset(g_lines, 0x55, sizeof g_lines);
    g_case.fixed[2] = ptrv(g_lines);
    g_self = xalloc(0x20);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.next();
}
void s_credits(Gen &g)
{
    memset(g_lines, 0, sizeof g_lines);
    const DWORD n = g.below(40);
    for (DWORD i = 0; i < n; i++) {
        const char *t = CREDITS[g.below(8)];
        for (int k = 0; t[k]; k++)
            g_lines[i * 256 + k] = (BYTE)t[k];
    }
    BYTE *c = xalloc(0x20);
    at_(c, 0) = g.next();
    at_(c, 4) = g.next();
    const float speed = (float)g.range(0, 400) / 1000.0f;
    memcpy(c + 0xc, &speed, 4);
    at_(c, 0x10) = 1 + g.below(30);
    at_(c, 0x18) = n;
    at_(c, 0x1c) = ptrv(g_lines);
    g_self = c;
    make_window(W_FRAME_, C_CFRAME);
    g_args[0] = ptrv(win(W_FRAME_));
    int *view = reinterpret_cast<int *>(xalloc(16));
    view[0] = 0, view[1] = g.range(0, 100), view[2] = 640, view[3] = view[1] + g.range(0, 480);
    g_args[1] = ptrv(view);
}
void s_credits_dtor(Gen &g)
{
    g_self = xalloc(0x20);
    at_(g_self, 0x1c) = g.chance(3) ? 0 : ptrv(xalloc(0x10));
}
// The credits' lines into the log
void hash_lines() { log_event(0x7c01, fnv(2166136261u, g_lines, sizeof g_lines), 0); }

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080};
constexpr DWORD GLOBALS1[] = {0x62e5e0};

const Test TESTS[] = {
    {"KeyButton deleting dtor", 0x471080, CALL_THIS, 1, ~0u, 40, s_kb_dtor, STUBS, NSTUBS},
    {"KeyButton UpdateText", 0x4710b0, CALL_THIS, 0, 0, 100, key_button, STUBS, NSTUBS},
    {"KeyButton Draw", 0x471120, CALL_THIS, 1, 0, 100, s_kb_frame, STUBS, NSTUBS},
    {"KeyButton Click", 0x471370, CALL_THIS, 0, 0, 100, key_button, STUBS, NSTUBS},
    {"ControlsDialog ctor", 0x4713b0, CALL_THIS, 2, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"ControlsDialog deleting dtor", 0x471a40, CALL_THIS, 1, ~0u, 60, s_dlg_dtor, STUBS, NSTUBS},
    {"ControlsDialog CanClose", 0x471bd0, CALL_THIS, 0, 0xff, 100, dialog, STUBS, NSTUBS},
    {"ControlsDialog Joysticks", 0x471fb0, CALL_THIS, 0, 0, 200, dialog, STUBS, NSTUBS},
    {"JoystickNames deleting dtor", 0x472180, CALL_THIS, 1, ~0u, 30, s_names_dtor, STUBS, NSTUBS},
    {"SaveChangesDialog ctor", 0x4721b0, CALL_THIS, 2, ~0u, 100, s_question, STUBS, NSTUBS},
    {"SaveChangesDialog deleting dtor", 0x472300, CALL_THIS, 1, ~0u, 30, s_flags, STUBS, NSTUBS},
    {"CreditsText ctor", 0x475eb0, CALL_THIS, 2, ~0u, 60, s_credits_ctor, STUBS_CREDITS, NSTUBS_CREDITS, nullptr, &hash_lines},
    {"CreditsText dtor", 0x475fd0, CALL_THIS, 0, 0, 30, s_credits_dtor, STUBS, NSTUBS},
    {"CreditsText Draw", 0x475ff0, CALL_THIS, 2, 0, 300, s_credits, STUBS, NSTUBS, nullptr, &hash_lines},
};

}  // namespace

ZAX_CHECKS(ui_controls)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        for (DWORD a : GLOBALS1)
            g_globals.add(a, 1);
    }
    Tally t{"ui controls: the controls configuration dialog, the key button, the save question, the credits text"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 26000);
    t.report();
}

}  // namespace zax
