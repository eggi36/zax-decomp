// Checks of the last user interface pieces of band C (ui_rest4c.cpp) against the original with the toolkit's
// harness and the screens' setup (checks_ui_screens.h): the toolkit's window constructor and destructor, the
// fonts' drawing and height, time and ctime, the keyboard state and the font list's record loader are stand-ins;
// the font list 0x6be1e0 gets a fake vtable and records of the case; images and the reporter are game objects.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_rest4c(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_rest4c", &hooks_ui_rest4c};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"},
    {0x4a6be0, CV_THIS, 0, K_SMALL, "Sequence_Height"},
    {0x5d20af, CV_CDECL, 1, K_VALUE, "time"},
    {0x5d2864, CV_CDECL, 1, K_TEXT, "ctime"},
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// The font list: slot 3 finds a name (a case value), the records random
DWORD g_list_vt[40];
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7902, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
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

// An image's slot 3 writes a rectangle through its argument
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_OBJECT && slot == 3 && args[0]) {   // (the rectangle is a local of the caller)
        Gen &v = g_case.values;
        int *r = at<int *>(args[0]);
        r[0] = v.range(-300, 300), r[1] = v.range(-300, 300), r[2] = r[0] + v.range(-5, 700), r[3] = r[1] + v.range(-5, 500);
    } else if (cls == -1) {
        if (const int t = template_of((DWORD)slot))
            made(answer, t);
    }
}

char g_object_args[MAXSLOT + 1];

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[3] = ptrv((void *)&list_find);
    }
    screen_base(g, &effect);
    object_args(g_object_args, "0:1,3:1,14:0");
    set_kind(C_OBJECT, 14, K_FLOAT);
    records(g, 0x6be1e0);
}

BYTE *image(Gen &g) { return g.chance(4) ? nullptr : win(g.chance(2) ? W_OBJECT : W_OTHER2); }

void s_ctor(Gen &g)
{
    g_args[0] = g.chance(3) ? 0 : ptrv(win(W_PARENT));
}
void s_dtor(Gen &g)
{
    make_window(W_OTHER2, C_OBJECT);
    at_(g_self, 0x100) = ptrv(image(g));
    g_args[0] = g.below(4);
}
void s_set_image(Gen &g)
{
    make_plain_window(0, C_WINDOW, g);
    make_window(W_OTHER2, C_OBJECT);
    g_case.argspec[3][0] = A_IGNORE;
    at_(g_self, 0x100) = g.chance(2) ? ptrv(win(W_OTHER2)) : 0;
    at_(g_self, 0x104) = g.next();
    at_(g_self, 0x108) = g.next();
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_OBJECT));
}
void s_watch(Gen &g)
{
    g_args[0] = g.next();
    g_args[1] = g.chance(4) ? 0 : ptrv(xalloc(0x40));
    g_args[2] = (DWORD)g.range(-100, 700);
    g_args[3] = (DWORD)g.range(-100, 500);
    g_args[4] = g.next();
}
void s_none4c(Gen &) {}
void s_key(Gen &g)
{
    g_args[0] = g.chance(8) ? g.next() : g.below(0x100);
    g_args[1] = g.next();
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214};

const Test TESTS[] = {
    {"BrowsableImage Ctor", 0x599330, CALL_THIS, 1, ~0u, 50, s_ctor, STUBS, NSTUBS},
    {"BrowsableImage DeletingDtor", 0x599370, CALL_THIS, 1, ~0u, 100, s_dtor, STUBS, NSTUBS},
    {"BrowsableImage SetImage", 0x5993c0, CALL_THIS, 1, 0, 200, s_set_image, STUBS, NSTUBS},
    {"Watch Draw", 0x596850, CALL_THIS, 5, 0, 300, s_watch, STUBS, NSTUBS},
    {"Input VirtualKeyToGameKey", 0x599580, CALL_FAST, 2, ~0u, 1500, s_key, STUBS, NSTUBS},
    {"ListBoxListSource NoIndex", 0x592380, CALL_THIS, 0, ~0u, 10, s_none4c, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_rest4c)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui rest 4c: the windowed browsable image, the watch's detail renderer, the virtual key mapping, the list source's no index"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 43000);
    t.report();
}

}  // namespace zax
