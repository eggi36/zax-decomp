// Checks of the mission objectives' drawing (mission_draw_c5.cpp) against the original with the toolkit's harness
// (checks_ui_common.h, checks_ui_screens.h): the mission, its objectives (game object fakes for their slots 24/25),
// the options object and the fonts are memory in the extra area (names, titles and descriptions ZStrings of the
// case, the fonts' line heights +0x80); the font list 0x6be1e0 gets a fake vtable and records of fake fonts; the
// fonts' drawing (DrawText, DrawParagraphs, BoxLastRight), DefinitionList_Object and the mission manager's current
// mission are stand-ins. The local options object, the CMission count and the Transparent Mask's opacity (0x6bcc88,
// a case value) are the exe's. Every case runs from a cleared stack area.
#include "checks_ui_screens.h"

namespace zax {

int hooks_mission_draw_c5(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"mission_draw_c5", &hooks_mission_draw_c5};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvs"},
    {0x4a6ae0, CV_THIS, 12, K_SMALL, "CFont::DrawParagraphs", "vvvs"},
    {0x4a7210, CV_THIS, 9, K_SMALL, "CFont::BoxLastRight", "svvv"},
    {0x48d8f0, CV_THIS, 1, K_FIXED0, "DefinitionList_Object"},
    {0x5127e0, CV_THIS, 0, K_FIXED1, "CMissionManager::Current"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

DWORD g_list_vt[40];
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7902, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(4);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

char g_object_args[MAXSLOT + 1];
BYTE *g_fonts[4];

void effect(int, int, DWORD *, DWORD) {}

BYTE *new_font(Gen &g)
{
    BYTE *f = xalloc(0x90);
    for (DWORD o = 0; o < 0x90; o += 4)
        at_(f, o) = g.next();
    at_(f, 0x80) = 6 + g.below(20);
    return f;
}
// A mission or objective: +8 a ZString, +0xc and +0x14 two Local.txt entries (texts at +0x10 and +0x18)
BYTE *new_entry(Gen &g, DWORD size)
{
    BYTE *e = xalloc(size);
    for (DWORD o = 0; o < size; o += 4)
        at_(e, o) = g.next();
    make_text(e + 8, g.chance(5) ? "" : TEXTS[g.below(8)]);
    make_text(e + 0xc, TEXTS[g.below(8)]);
    make_text(e + 0x10, g.chance(4) ? "" : TEXTS[g.below(8)]);
    make_text(e + 0x14, TEXTS[g.below(8)]);
    make_text(e + 0x18, g.chance(4) ? "" : TEXTS[g.below(8)]);
    return e;
}

BYTE *g_mission;

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[3] = ptrv((void *)&list_find);
    }
    screen_base(g, &effect);
    object_args(g_object_args, "13:0,24:0,25:0,39:0,45:0");
    set_kind(C_OBJECT, 13, K_FLOAT);
    set_kind(C_OBJECT, 24, K_BOOL);
    set_kind(C_OBJECT, 25, K_BOOL);
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_OBJECT, 45, K_SMALL);
    for (BYTE *&f : g_fonts)
        f = new_font(g);
    const DWORD n = 4;
    BYTE *recs = xalloc(n * 0x80);
    for (DWORD i = 0; i < n; i++)
        at_(recs, i * 0x80 + 0x78) = ptrv(g_fonts[i]);
    case_global(0x6be210, ptrv(recs));
    case_global(0x6be214, n);
    const DWORD opacities[4] = {0x3f800000, 0x3f000000, 0x3e800000, 0};
    case_global(0x6bcc88, opacities[g.below(4)]);
    g_case.fixed[0] = ptrv(new_font(g));
    // the mission and its objectives
    BYTE *mission = new_entry(g, 0x40);
    const DWORD count = g.below(5);
    BYTE *list = xalloc(4 * 5);
    for (DWORD i = 0; i < count; i++) {
        BYTE *o = new_entry(g, 0x40);
        at_(o, 0) = at_(win(W_OBJECT), 0);   // the objective's slots 24/25: the game object fake
        at_(o, 0x20) = 0;
        if (g.chance(4)) {
            BYTE *linked = new_entry(g, 0x40);
            at_(linked, 0) = at_(win(W_OBJECT), 0);
            at_(o, 0x20) = ptrv(linked);
        }
        at_(list, i * 4) = ptrv(o);
    }
    at_(mission, 0x18) = ptrv(list);
    at_(mission, 0x1c) = count;
    at_(mission, 0x28) = g.chance(3) ? 0xffffffffu : g.below(count + 2);
    g_mission = mission;
    g_case.fixed[1] = g.chance(6) ? 0 : ptrv(mission);
}

BYTE *new_options(Gen &g)
{
    BYTE *o = xalloc(0x20);
    for (DWORD i = 0; i < 0x20; i += 4)
        at_(o, i) = g.next();
    at_(o, 8) = g.below(40);
    at_(o, 0xc) = g.below(20);
    for (DWORD i = 0x10; i < 0x20; i += 4)
        at_(o, i) = g.below(6);
    return o;
}

void s_draw(Gen &g)
{
    g_self = g.chance(6) ? nullptr : g_mission;
    g_args[0] = g.chance(8) ? 0 : ptrv(win(W_OTHER));
    g_args[1] = g.range(-20, 300);
    g_args[2] = g.range(-20, 300);
    g_args[3] = g.range(0, 400);
    const DWORD opacities[3] = {0x3f800000, 0x3f400000, 0};
    g_args[4] = opacities[g.below(3)];
    g_args[5] = g.below(2) | (g.next() & 0xffffff00u);
    g_args[6] = g.below(2) | (g.next() & 0xffffff00u);
    g_args[7] = g.chance(3) ? 0 : ptrv(new_options(g));
}
void s_panel(Gen &g)
{
    BYTE *manager = xalloc(0x40);
    for (DWORD i = 0; i < 0x40; i += 4)
        at_(manager, i) = g.next();
    at_<float>(manager, 0x1c) = (float)g.below(100);
    g_self = manager;
    g_args[0] = ptrv(win(W_OTHER));
    g_args[1] = g.below(2) | (g.next() & 0xffffff00u);
}

__attribute__((noinline)) void clear_stack()
{
    volatile BYTE area[0x1400];
    for (int i = (int)sizeof area - 1; i >= 0; i--)
        area[i] = 0;
}
// fastcall: ecx the object, edx the first argument, the rest on the stack
DWORD THISCALL draw_from_clear_stack(BYTE *self, DWORD frame, DWORD x, DWORD y, DWORD w, DWORD op, DWORD draw, DWORD title, DWORD opts)
{
    clear_stack();
    return (DWORD)at<int(__fastcall *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x509910)(self, frame, x, y, w, op, draw, title,
                                                                                                         opts);
}
DWORD THISCALL panel_from_clear_stack(BYTE *self, DWORD frame, DWORD draw)
{
    clear_stack();
    return (DWORD)at<int(__fastcall *)(BYTE *, DWORD, DWORD)>(0x509890)(self, frame, draw);
}

constexpr DWORD GLOBALS_C5[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214, 0x6bcc88};

}  // namespace

ZAX_CHECKS(mission_draw_c5)
{
    (void)cls;
    (void)n;
    const Test tests[] = {
        {"MissionObjectives_Draw", ptrv((void *)&draw_from_clear_stack), CALL_THIS, 8, 0xffffffff, 1200, s_draw, STUBS, NSTUBS},
        {"MissionObjectives_DrawPanel", ptrv((void *)&panel_from_clear_stack), CALL_THIS, 2, 0, 300, s_panel, STUBS, NSTUBS},
    };
    if (!g_globals.n)
        for (DWORD a : GLOBALS_C5)
            g_globals.add(a, 4);
    Tally t{"mission draw c5: the mission objectives' drawing"};
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 81000);
    t.report();
}

}  // namespace zax
