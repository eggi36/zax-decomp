// Checks of the user interface's flow pieces (ui_flow.cpp) against the original with the toolkit's harness and
// the screens' setup (checks_ui_screens.h): the screen image's functions, the credits text's constructor and the
// dialog's ReplaceWindow are stand-ins; the image lists 0x6bf850/0x6bf9b8 get a fake vtable and records of the case.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_flow(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_flow", &hooks_ui_flow};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x578910, CV_FAST, 0, K_VALUE, "ScreenImage_Init", "xx"},
    {0x578e70, CV_FAST, 2, K_VALUE, "ScreenImage::ShowFrame", "xxvv"},
    {0x578eb0, CV_FAST, 0, K_VALUE, "ScreenImage::WaitAndHide", "xx"},
    {0x578930, CV_FAST, 0, K_VALUE, "ScreenImage::Clear", "xx"},
    {0x475eb0, CV_THIS, 2, 'e', "CreditsText_Ctor", "sv"},
    {0x4040c0, CV_THIS, 2, K_VALUE, "CDialog::ReplaceWindow"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7901, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7902, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7903, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    return 0xf0000000u | (rec & 0xffff);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *, DWORD answer)
{
    if (cls != -1)
        return;
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

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

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[1] = ptrv((void *)&list_name);
        g_list_vt[3] = ptrv((void *)&list_find);
        g_list_vt[37] = ptrv((void *)&list_load);
    }
    screen_base(g, &effect);
    object_args(g_object_args, "0:1,65:0");
    set_kind(C_OBJECT, 45, g.chance(2) ? K_FIXED0 : K_SMALL);
    g_case.fixed[0] = 0x280;
    records(g, 0x6bf850);
    records(g, 0x6bf9b8);
}

void s_slot65(Gen &)
{
    at_(g_self, 0x34) = ptrv(win(W_OBJECT));
}
void s_images(Gen &g)
{
    const DWORD n = g.below(5);
    DWORD *names = reinterpret_cast<DWORD *>(xalloc(4 * (n + 1)));
    for (DWORD i = 0; i < n; i++)
        names[i] = ptrv(TEXTS[g.below(8)]);
    names[n] = 0;
    g_self = reinterpret_cast<BYTE *>(names);
}
void s_none(Gen &) {}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6bf850, 0x6bf880, 0x6bf884, 0x6bf9b8, 0x6bf9e8, 0x6bf9ec};

const Test TESTS[] = {
    {"App WindowSlot65", 0x4cdf60, CALL_THIS, 0, ~0u, 20, s_slot65, STUBS, NSTUBS},
    {"ScreenImages Show", 0x5be0d0, CALL_THIS, 0, 0, 200, s_images, STUBS, NSTUBS},
    {"EndCredits Show", 0x5bede0, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_flow)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui flow: the application window's slot 65, the screen images, the end game credits"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 30000);
    t.report();
}

}  // namespace zax
