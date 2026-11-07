// Checks of batch C3's menu slots (ui_c3_menus.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): the menus as fake windows (their buttons other fakes), the display, the platform, the world
// and the resource lists (the texture list 0x6bf850, the game type list 0x6c02e8) as "game object" fakes with
// records of the case, frames as recording fakes; every constructor, the window replacement, the message box, the
// sequence player's drawing, the frame helpers, the bevel, the credits text and the game's exit as recording
// stand-ins (the exit never runs).
#include "checks_ui_common.h"
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_c3_menus(Hook *out);

namespace {

using namespace uic;
using scr::object_args;

constinit ModuleHooks g_hooks{"ui_c3_menus", &hooks_ui_c3_menus};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x40f5b0, CV_THIS, 1, K_VALUE, "CJWindow::SetWidth"},
    {0x40f5d0, CV_THIS, 1, K_VALUE, "CJWindow::SetHeight"},
    {0x40d630, CV_THIS, 2, K_VALUE, "CJWindow::MoveTo"},
    {0x40d680, CV_THIS, 1, K_VALUE, "CJWindow::SetLeft"},
    {0x40d6c0, CV_THIS, 1, K_VALUE, "CJWindow::SetTop"},
    {0x56c600, CV_THIS, 1, K_VALUE, "SequencePlayer::Draw"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize"},
    {0x4a8a90, CV_STD, 0, 'e', "CFrame::Ctor"},
    {0x4ab5d0, CV_THIS, 1, K_VALUE, "CFrame_CopyFrom"},
    {0x4a8b00, CV_THIS, 0, K_VALUE, "CFrame::Dtor"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4b3a70, CV_THIS, 5, K_VALUE, "Frame_FillRectXY"},
    {0x403af0, CV_THIS, 1, K_VALUE, "CDialog2::Draw"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x4040c0, CV_THIS, 2, K_VALUE, "CDialog::ReplaceWindow"},
    {0x40c740, CV_THIS, 2, K_VALUE, "CJWindow::MessageBox", "ss"},
    {0x5bf820, CV_THIS, 1, 'e', "CDifficultyMenu_ctor"},
    {0x518e00, CV_THIS, 1, 'e', "WindowServerBrowser::Ctor"},
    {0x562970, CV_THIS, 2, 'e', "SaveLoadScreen::SaveLoadScreen"},
    {0x5c07f0, CV_THIS, 1, 'e', "CreditsMenu::CreditsMenu"},
    {0x5c0220, CV_THIS, 2, 'e', "QuitMenu::QuitMenu", "vs"},
    {0x5be0d0, CV_THIS, 0, K_VALUE, "ScreenImages_Show"},
    {0x4d2470, CV_CDECL, 0, K_VALUE, "Platform_CloseWindow"},
    {0x591020, CV_THIS, 1, 'e', "VideoDialog::Ctor"},
    {0x4713b0, CV_THIS, 2, 'e', "ControlsDialog::Ctor"},
    {0x46d010, CV_THIS, 1, 'e', "NetOptionsDialog::Ctor"},
    {0x57efd0, CV_THIS, 1, 'e', "SoundDialog::Ctor"},
    {0x4035f0, CV_THIS, 2, K_VALUE, "CFrameWindow::Notify"},
    {0x40a740, CV_FAST, 6, K_VALUE, "Bevel_Draw", "vvvv"},
    {0x40ef90, CV_THIS, 4, K_BOOL, "CJWindow::VisibleClient", "vvxv"},
    {0x524fe0, CV_THIS, 2, K_VALUE, "COffsetClippedProxyFrame_SetOffset"},
    {0x524f50, CV_THIS, 1, K_VALUE, "COffsetClippedProxyFrame_SetClip", "r"},
    {0x475ff0, CV_THIS, 2, K_VALUE, "CreditsText::Draw", "vr"},
    {0x59b230, CV_THIS, 1, K_VALUE, "NetGame_SendText", "s"},
    {0x40c170, CV_THIS, 1, K_VALUE, "CJWindow::Key"},
    {0x592200, CV_THIS, 0, K_VALUE, "FieldNameList::Ctor"},
    {0x5922b0, CV_THIS, 2, K_VALUE, "FieldNameList::AddValue", "sv"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x4127d0, CV_THIS, 2, 'e', "TextLabel::TextLabel", "vs"},
    {0x408040, CV_THIS, 3, 'e', "ComboBox::ComboBox"},
    {0x408bc0, CV_THIS, 2, K_VALUE, "ComboBox::SelectId"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

char g_object_args[MAXSLOT + 1];

void effect(int cls, int slot, DWORD *args, DWORD)
{
    Gen &v = g_case.values;
    if (cls == -1) {
        switch ((DWORD)slot) {
        case 0x4a8a90: memset(at<void *>(g_effect_ecx), 0, 0x40); break;
        case 0x40a740: log_event(0x7a74, args[2] ^ (args[3] << 8), args[4] ^ (args[5] << 16)); break;
        case 0x4b3a70: log_event(0x7b3a, args[4], 0); break;
        case 0x40ef90: {   // the visible client rect through the third argument
            int *r = at<int *>(args[2]);
            r[0] = v.range(-20, 300), r[1] = v.range(-20, 300), r[2] = r[0] + v.range(0, 300), r[3] = r[1] + v.range(0, 300);
            break;
        }
        default: break;
        }
        return;
    }
    if ((cls == C_PROXY_FRAME || cls == C_CFRAME) && slot == 36 && args[0]) {
        int *r = at<int *>(args[0]);
        r[0] = v.range(-20, 50), r[1] = v.range(-20, 50), r[2] = v.range(0, 800), r[3] = v.range(0, 600);
    }
}

// The reporter (a game object whose +4 is the display, slot 1 the platform) and the display's size
BYTE *g_platform;
void reporter(Gen &g)
{
    BYTE *rep = make_window(15, C_OBJECT);
    BYTE *disp = xalloc(0x10);
    at_(disp, 0) = at_(rep, 0);   // a game object too
    at_(rep, 4) = ptrv(disp);
    case_global(0x6c02cc, ptrv(rep));
    g_case.fixed[0] = g.chance(2) ? 0x280 : 800;
    set_kind(C_OBJECT, 45, K_FIXED0);
    set_kind(C_OBJECT, 39, K_SMALL);
    g_platform = xalloc(0x10);
    at_(g_platform, 0) = at_(rep, 0);
    g_case.fixed[3] = ptrv(g_platform);
    set_kind(C_OBJECT, 1, K_FIXED3);
    set_kind(C_OBJECT, 11, K_BOOL);
}
// A resource list global at b (a game object; 1-4 records, all loaded or loadable)
void global_list(Gen &g, DWORD b, int first, bool loaded = false)
{
    const DWORD n = 1 + g.below(4);
    BYTE *recs = xalloc(n * 0x80);
    for (DWORD i = 0; i < n; i++)
        at_(recs, i * 0x80 + 0x78) = g.chance(3) && !loaded ? 0 : ptrv(make_window(first + (int)i, C_OBJECT));
    case_global(b, at_(make_window(first + 4, C_OBJECT), 0));
    case_global(b + 0x30, ptrv(recs));
    case_global(b + 0x34, n);
    g_case.fixed[2] = ptrv(make_window(first + 5, C_OBJECT));
    set_kind(C_OBJECT, 37, K_FIXED2);
    set_kind(C_OBJECT, 3, K_SMALL);
    g_case.argspec[37][0] = A_TEXT, g_case.argspec[37][1] = A_TEXT;
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    g_case.effect = &effect;
    object_args(g_object_args, "1:1,3:2,11:0,23:1,37:3,39:0,44:0,45:0,88:0");
    set_kind(C_OBJECT, 1, K_TEXT);   // (the lists' names; the reporter's tests set their own)
    g_self = win(0);
    case_global(0x716190, g.below(4));
}

void s_bg(Gen &g)
{
    reporter(g);
    BYTE *w = win(0);
    at_(w, 0x100) = ptrv(make_window(10, C_OBJECT));
    BYTE *src = make_window(12, C_CFRAME);
    g_case.fixed[1] = ptrv(src);
    set_kind(C_OBJECT, 23, K_FIXED1);
    at_(w, 0x10c) = at_(make_window(13, C_CFRAME), 0);
    set_kind(C_CFRAME, 45, g.chance(2) ? K_ZERO : K_SMALL);
    g_case.argspec[23][1] = A_RECT, g_case.argspec[23][2] = A_RECT;
    g_args[0] = ptrv(make_window(14, C_CFRAME));
}
void s_logo(Gen &g)
{
    reporter(g);
    at_(g_self, 0x10c) = g.chance(3) ? g.next() : g.below(640);
    at_(g_self, 0x110) = g.chance(3) ? g.next() : g.below(480);
    g_args[0] = g.next();
}
void s_end_screen(Gen &g)
{
    reporter(g);
    // (the records loaded: a load through the texture list's real folder ran into the field code in the VM; the
    // inline load is the resource lists' one, checked with Select Map's list and in ui_c3_misc)
    global_list(g, 0x6bf850, 3, true);
    set_kind(C_OBJECT, 1, K_TEXT);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
}
void s_main_draw(Gen &g)
{
    reporter(g);
    at_(g_self, 0x154) = ptrv(win(3));
    at_(g_self, 0x134) = ptrv(win(4));
    set_kind(C_CFRAME, 45, K_VALUE);
    set_kind(C_CFRAME, 39, K_VALUE);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
}
void s_main_notify(Gen &g)
{
    reporter(g);
    object_args(g_object_args, "1:0,11:0,39:0,45:0");   // (the reporter's slot 1, the platform, takes none)
    for (int k = 0; k < 6; k++)
        at_(g_self, 0x138 + 4 * k) = ptrv(win(3 + k));
    g_args[0] = ptrv(win(3 + (int)g.below(8)));
    g_args[1] = g.chance(5) ? g.below(3) : 0;
    case_global(0x62485c, g.below(2));
}
void s_options_notify(Gen &g)
{
    for (int k = 0; k < 5; k++)
        at_(g_self, 0x138 + 4 * k) = ptrv(win(3 + k));
    if (g.chance(3))
        at_(g_self, 0x144) = 0;
    g_args[0] = g.chance(4) ? 0 : ptrv(win(3 + (int)g.below(7)));
    g_args[1] = g.chance(5) ? g.below(3) : 0;
    case_global(0x6c2080, ptrv(make_window(10, C_OBJECT)));
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
void s_credits_text(Gen &g)
{
    colours(g_self, g);
    g_self[0x104] = (BYTE)g.below(2);
    at_(g_self, 0x100) = g.next();
    at_(g_self, 0x7c) = ptrv(fake_font(g.below(30)));
    g_args[0] = ptrv(make_window(14, C_PROXY_FRAME));
    g_case.argspec[36][0] = A_IGNORE;
    g_case.argspec[28][0] = A_RECT;
}
void s_chat(Gen &g)
{
    at_(g_self, 0x100) = ptrv(make_window(10, C_OBJECT));
    set_kind(C_OBJECT, 44, g.chance(3) ? K_ZERO : K_TEXT);
    const DWORD k[] = {0xd, 0x1b, 5, 9};
    g_args[0] = k[g.below(4)];
}
void s_select_map(Gen &g)
{
    global_list(g, 0x6c02e8, 3);
    set_kind(C_OBJECT, 1, K_TEXT);
}

const Test TESTS[] = {
    {"MainMenuBg Draw", 0x5beae0, CALL_THIS, 1, 0, 150, s_bg, STUBS, NSTUBS},
    {"MainMenuLogo Draw", 0x5bec70, CALL_THIS, 1, ~0u, 150, s_logo, STUBS, NSTUBS},
    {"EndScreen Draw", 0x5bf080, CALL_THIS, 1, ~0u, 150, s_end_screen, STUBS, NSTUBS},
    {"MainMenu Draw", 0x5bf290, CALL_THIS, 1, ~0u, 200, s_main_draw, STUBS, NSTUBS},
    {"MainMenu Notify", 0x5bf4a0, CALL_THIS, 2, ~0u, 300, s_main_notify, STUBS, NSTUBS},
    {"OptionsMenu Notify", 0x5c0490, CALL_THIS, 2, ~0u, 200, s_options_notify, STUBS, NSTUBS},
    {"CreditsText Draw", 0x5c0610, CALL_THIS, 1, 0, 200, s_credits_text, STUBS, NSTUBS},
    {"Chat Key", 0x5c1170, CALL_THIS, 1, ~0u, 80, s_chat, STUBS, NSTUBS},
    {"SelectMap AddGameTypes", 0x5bde70, CALL_THIS, 0, ~0u, 150, s_select_map, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_c3_menus)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        const DWORD g4[] = {0x6c02cc, 0x6c2080, 0x6bf850, 0x6bf880, 0x6bf884, 0x6c02e8, 0x6c0318, 0x6c031c,
                            0x7161a0, 0x7161a4, 0x7161a8, 0x7161ac};
        for (DWORD a : g4)
            g_globals.add(a, 4);
        g_globals.add(0x716190, 1);
        g_globals.add(0x62485c, 1);
    }
    Tally t{"ui c3 menus: the main menu's background, logo, frame and buttons, the options menu, the end credits, the chat line, Select Map's game types"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 66000);
    t.report();
}

}  // namespace zax
