// Checks of CQuestStat::Draw (quest_stat_draw_c5.cpp) against the original with the toolkit's harness
// (checks_ui_common.h, checks_ui_screens.h): the statistic is memory in the extra area with the game object fake's
// vtable (slots 25-31 and 35 recorded: the bools, the float, texts written into the ZString argument), its name a
// ZString of the case and two font indices; the quest game type (its format +0x38) and the columns (format, x, width,
// value) are case memory; the cache 0x71232c is a case value (empty: the inline lookup through a fake game type list,
// DefinitionList_Object, the class test and getter as stand-ins); the fonts a fake font list; CFont::DrawWrapped and
// the column function 0x5525e0 are stand-ins. ZString code is the exe's. Every case runs from a cleared stack area.
#include "checks_ui_screens.h"

namespace zax {

int hooks_quest_stat_draw_c5(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"quest_stat_draw_c5", &hooks_quest_stat_draw_c5};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a6790, CV_THIS, 7, K_SMALL, "CFont::DrawWrapped", "vvvs"},
    {0x5525e0, CV_THIS, 1, K_FIXED2, "CQuestGameType::Column"},
    {0x48d8f0, CV_THIS, 1, K_FIXED3, "DefinitionList_Object"},
    {0x416790, CV_THIS, 1, K_FIXED4, "Object_IsKindOf"},
    {0x551c20, CV_CDECL, 0, K_VALUE, "CQuestGameType_ClassInfo"},
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
const char *const FORMATS[4] = {"%s", "Total: %s", "%s points", "[%s]"};

// slots 25/27 (a, text) and 26/28 (a, b, text) write a text into the ZString
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls != C_OBJECT)
        return;
    const DWORD z = slot == 25 || slot == 27 ? args[1] : slot == 26 || slot == 28 ? args[2] : 0;
    if (z)
        at<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(z, TEXTS[answer & 7]);
}

BYTE *new_font(Gen &g)
{
    BYTE *f = xalloc(0x90);
    for (DWORD o = 0; o < 0x90; o += 4)
        at_(f, o) = g.next();
    return f;
}

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[3] = ptrv((void *)&list_find);
    }
    screen_base(g, &effect);
    object_args(g_object_args, "25:2,26:3,27:2,28:3,29:2,30:2,31:2,35:2");
    set_kind(C_OBJECT, 29, K_BOOL);
    set_kind(C_OBJECT, 30, K_BOOL);
    set_kind(C_OBJECT, 35, K_BOOL);
    set_kind(C_OBJECT, 31, K_FLOAT);
    set_kind(C_OBJECT, 25, K_VALUE);
    BYTE *recs = xalloc(4 * 0x80);
    for (DWORD i = 0; i < 4; i++)
        at_(recs, i * 0x80 + 0x78) = ptrv(new_font(g));
    case_global(0x6be1e0, ptrv(g_list_vt));
    case_global(0x6be210, ptrv(recs));
    case_global(0x6be214, 4);
    BYTE *quest = xalloc(0x40);
    make_text(quest + 0x3c, FORMATS[g.below(4)]);
    BYTE *col = xalloc(0x28);
    make_text(col + 0x14, FORMATS[g.below(4)]);
    at_(col, 0x1c) = g.below(300);
    at_(col, 0x20) = g.below(200);
    at_(col, 0x24) = g.next();
    g_case.fixed[2] = ptrv(col);
    g_case.fixed[3] = ptrv(quest);
    g_case.fixed[4] = 1;
    case_global(0x6c02e8, ptrv(g_list_vt));
    case_global(0x71232c, g.chance(2) ? 0 : ptrv(quest));
}
void s_draw(Gen &g)
{
    BYTE *s = xalloc(0x30);
    for (DWORD o = 0; o < 0x30; o += 4)
        at_(s, o) = g.next();
    at_(s, 0) = at_(win(W_OBJECT), 0);
    make_text(s + 0x18, TEXTS[g.below(8)]);
    at_(s, 0x1c) = g.below(6);
    at_(s, 0x20) = g.below(6);
    g_self = s;
    g_args[0] = g.next();
    g_args[1] = g.next();
    g_args[2] = ptrv(win(W_OTHER));
    g_args[3] = g.range(-50, 400);
    g_args[4] = g.range(-50, 400);
}

__attribute__((noinline)) void clear_stack()
{
    volatile BYTE area[0x1400];
    for (int i = (int)sizeof area - 1; i >= 0; i--)
        area[i] = 0;
}
DWORD THISCALL from_clear_stack(BYTE *self, DWORD a, DWORD b, DWORD f, DWORD x, DWORD y)
{
    clear_stack();
    return at<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x5533e0)(self, a, b, f, x, y);
}

constexpr DWORD GLOBALS_C5[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214, 0x6c02e8, 0x71232c};

}  // namespace

ZAX_CHECKS(quest_stat_draw_c5)
{
    (void)cls;
    (void)n;
    const Test tests[] = {
        {"CQuestStat::Draw", ptrv((void *)&from_clear_stack), CALL_THIS, 5, 0xffffffff, 1000, s_draw, STUBS, NSTUBS},
    };
    if (!g_globals.n)
        for (DWORD a : GLOBALS_C5)
            g_globals.add(a, 4);
    Tally t{"quest stat draw c5: CQuestStat::Draw"};
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 93000);
    t.report();
}

}  // namespace zax
