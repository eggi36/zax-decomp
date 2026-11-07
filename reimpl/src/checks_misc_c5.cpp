// Checks of misc_c5.cpp against the original with the toolkit's harness (checks_ui_common.h, checks_ui_screens.h):
// the static object's vtable 0x62eae0 is a case value, its FreeFields and destructor are stand-ins; the chat line's
// MessageLines_Add and the world's view frame (a game object fake) answer per case; the main menu's allocation, the
// application's window and the constructor the call 0x59bace points at (the launcher's redirection when it is
// there) are stand-ins, the world a game object fake (slot 23 recorded).
#include "checks_ui_screens.h"

namespace zax {

int hooks_misc_c5(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"misc_c5", &hooks_misc_c5};
Globals g_globals;

Stub g_stubs[] = {
    UIC_ALLOC_STUBS,
    {0x4173f0, CV_THIS, 0, K_VALUE, "Object_FreeFields"},
    {0x4166d0, CV_THIS, 0, K_VALUE, "CObject_Dtor"},
    {0x506c30, CV_THIS, 3, K_VALUE, "MessageLines_Add"},
    {0x4cdf30, CV_THIS, 0, K_VALUE, "App_Window"},
    {0x5be4a0, CV_THIS, 1, K_WINDOW, "CMainMenu_ctor (the target of 0x59bace)"},
};
constexpr int NSTUBS = sizeof g_stubs / sizeof g_stubs[0];

char g_object_args[MAXSLOT + 1];
void effect(int, int, DWORD *, DWORD) {}

void base(Gen &g)
{
    screen_base(g, &effect);
    object_args(g_object_args, "23:2,45:0");
    set_kind(C_OBJECT, 45, K_SMALL);
    at_(win(W_WORLD), 0x2a8) = ptrv(win(W_OBJECT));
    case_global(0x62eae0, g.next());
}
void s_exit(Gen &) { g_self = nullptr; }
void s_chat(Gen &g)
{
    BYTE *app = xalloc(0x290);
    for (DWORD o = 0; o < 0x290; o += 4)
        at_(app, o) = g.next();
    g_self = app;
    g_args[0] = g.next();
    g_args[1] = g.next();
}
void s_menu(Gen &) { g_self = win(W_WORLD); }

constexpr DWORD GLOBALS_C5[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x62eae0};

}  // namespace

ZAX_CHECKS(misc_c5)
{
    (void)cls;
    (void)n;
    g_stubs[NSTUBS - 1].address = 0x59bad3 + *at<DWORD *>(0x59bacf);
    const Test tests[] = {
        {"StaticExit_MoveToPosChecker", 0x430870, CALL_CDECL, 0, 0xffffffff, 50, s_exit, g_stubs, NSTUBS},
        {"Game_ShowChatLine", 0x59b370, CALL_THIS, 2, 0xffffffff, 200, s_chat, g_stubs, NSTUBS},
        {"World_OpenMainMenu", 0x59bab0, CALL_THIS, 0, 0xffffffff, 200, s_menu, g_stubs, NSTUBS},
    };
    if (!g_globals.n)
        for (DWORD a : GLOBALS_C5)
            g_globals.add(a, 4);
    Tally t{"misc c5: the static object's exit, the chat line, the main menu"};
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 91000);
    t.report();
}

}  // namespace zax
