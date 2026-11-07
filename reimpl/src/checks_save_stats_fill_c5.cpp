// Checks of CZaxSaveGameStats::Fill (save_stats_fill_c5.cpp) against the original with the toolkit's harness
// (checks_ui_common.h, checks_ui_screens.h): the statistics object is memory in the extra area (its ZStrings, the
// screen shot frame and the item list with the game object fake's vtable, the list's items game object fakes); the
// application, its view, window and mission manager, the inventory items are game object fakes; the C runtime's
// time, localtime and strftime (a text of its arguments), the world's queries (first character, current layer and its
// file name, status text), the inventory functions, the mission's clone, the frame's size, the expanded save name
// (a ZString of the case), the file test and the category order are stand-ins; ZString code and memmove are the
// exe's. Every case runs from a cleared stack area.
#include "checks_ui_screens.h"

namespace zax {

int hooks_save_stats_fill_c5(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"save_stats_fill_c5", &hooks_save_stats_fill_c5};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5d20af, CV_CDECL, 1, K_VALUE, "time"},
    {0x5d312f, CV_CDECL, 1, K_FIXED1, "localtime"},
    {0x5d287d, CV_CDECL, 4, K_SMALL, "strftime", "xvsv"},
    {0x4f5d80, CV_THIS, 0, K_FIXED2, "World::FirstCharacter"},
    {0x4267e0, CV_THIS, 0, K_FIXED3, "LayerPart_GetInventory"},
    {0x428b10, CV_CDECL, 0, K_FIXED3, "Player_Inventory"},
    {0x5127e0, CV_THIS, 0, K_WINDOW, "CMissionManager::Current"},
    {0x416760, CV_THIS, 2, K_VALUE, "Object_Clone"},
    {0x55d030, CV_CDECL, 0, K_SMALL, "ColorDepth"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize"},
    {0x4cdf30, CV_THIS, 0, K_FIXED5, "App_Window"},
    {0x5a0c60, CV_THIS, 1, K_VALUE, "CZaxWorld::StatusText"},
    {0x5a0a00, CV_THIS, 3, K_VALUE, "CZaxWorld::SetStatusText"},
    {0x48d030, CV_THIS, 0, K_FLOAT, "EntityDestructable_HitPointsLeft"},
    {0x523df0, CV_THIS, 2, K_SMALL, "ResourceList_Find", "sv"},
    {0x4257f0, CV_THIS, 1, K_VALUE, "CInventory_TotalCount"},
    {0x4f1050, CV_THIS, 0, K_FIXED6, "World::CurrentLayer"},
    {0x4eb670, CV_THIS, 0, K_FIXED7, "CLayer::FileName"},
    {0x5bc810, CV_THIS, 1, 'p', "CZaxSaveGameStats::Name", "x"},
    {0x4a1570, CV_FAST, 0, K_BOOL, "File_CanOpen", "sv"},
    {0x5b2aa0, CV_FAST, 0, K_SMALL, "Inventory_ItemInCategoryOrder"},
    {0x424f60, CV_THIS, 1, K_FIXED0, "CInventory::ItemAt"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

char g_object_args[MAXSLOT + 1];
const char *const NAMES[6] = {"Levels/ACT 1/M1M2/01 Main.zax", "Main", "a\\b.c.d", "noext", "dir/", "x.y/z"};
const char *const SAVE_NAMES[6] = {"M1 Main", "", "123abc_DEF", "Q", "A very long save game name with many many letters 0123456789 and more", "\xe4\xf6!"};

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)answer;
    Gen &v = g_case.values;
    if (cls == -1 && (DWORD)slot == 0x5d287d && args[0]) {   // strftime: a text of the time struct
        wsprintfA(at<char *>(args[0]), "date %x", at_(at<void *>(args[3]), 0));
    } else if (cls == -1 && (DWORD)slot == 0x5a0c60 && args[0]) {
        at<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(args[0], TEXTS[v.below(8)]);
    } else if (cls == -1 && (DWORD)slot == 0x5bc810 && args[0]) {   // the expanded name, a new ZString
        at<void(THISCALL *)(DWORD, const char *)>(0x4dee00)(args[0], SAVE_NAMES[v.below(6)]);
    }
}

void base(Gen &g)
{
    screen_base(g, &effect);
    object_args(g_object_args, "1:1,2:1,3:1,4:0,14:0,17:1,23:4,26:2,31:1,39:0,45:0,97:0");
    set_kind(C_OBJECT, 14, K_FLOAT);
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_OBJECT, 45, K_SMALL);
    set_kind(C_OBJECT, 31, K_WINDOW);
    BYTE *app = win(W_APP);
    at_(app, 0x2a8) = ptrv(win(W_OTHER));
    at_<float>(app, 0x368) = (float)g.below(100);
    BYTE *mm = xalloc(0x10);
    at_(mm, 8) = g.next();
    set_kind(C_OBJECT, 97, K_FIXED4);
    g_case.fixed[0] = ptrv(win(W_OBJECT));
    BYTE *tm = xalloc(0x24);
    for (DWORD o = 0; o < 0x24; o += 4)
        at_(tm, o) = g.next();
    g_case.fixed[1] = ptrv(tm);
    g_case.fixed[2] = g.chance(4) ? 0 : ptrv(win(W_OBJECT));
    BYTE *inv = xalloc(0x40);
    for (DWORD o = 0; o < 0x40; o += 4)
        at_(inv, o) = g.next();
    at_(inv, 0x20) = g.below(6);
    g_case.fixed[3] = ptrv(inv);
    g_case.fixed[4] = ptrv(mm);
    g_case.fixed[5] = ptrv(win(W_OTHER2));
    BYTE *layer = xalloc(0xd0);
    make_text(layer + 0xc4, g.chance(2) ? "" : g.chance(2) ? "  Main Map  " : TEXTS[g.below(8)]);
    g_case.fixed[6] = ptrv(layer);
    char *fname = reinterpret_cast<char *>(xalloc(64));
    const char *n = NAMES[g.below(6)];
    for (int i = 0; n[i] && i < 63; i++)
        fname[i] = n[i];
    g_case.fixed[7] = ptrv(fname);
}
void s_fill(Gen &g)
{
    BYTE *s = xalloc(0x80);
    for (DWORD o = 0; o < 0x80; o += 4)
        at_(s, o) = g.next();
    make_text(s + 8, "");
    make_text(s + 0x5c, "");
    make_text(s + 0x68, "");
    make_text(s + 0x7c, "");
    at_(s, 0x10) = g.chance(2) ? 0 : ptrv(win(W_SCREEN));
    at_(s, 0x14) = at_(win(W_OBJECT), 0);   // the screen shot frame: the game object fake
    at_(s, 0x6c) = at_(win(W_OBJECT), 0);   // the item list
    const DWORD n = g.below(4);
    BYTE *data = xalloc(4 * 4 + 4);
    for (DWORD i = 0; i < n; i++)
        at_(data, i * 4) = g.chance(4) ? 0 : ptrv(win(W_WORLD));
    at_(s, 0x70) = ptrv(data);
    at_(s, 0x74) = n;
    at_(s, 0x78) = n + g.below(3);
    g_self = s;
}

__attribute__((noinline)) void clear_stack()
{
    volatile BYTE area[0x1400];
    for (int i = (int)sizeof area - 1; i >= 0; i--)
        area[i] = 0;
}
DWORD THISCALL from_clear_stack(BYTE *self)
{
    clear_stack();
    at<void(THISCALL *)(BYTE *)>(0x5bc2a0)(self);
    return 0;
}

constexpr DWORD GLOBALS_C5[] = {0x6c02cc, 0x713f14, 0x6c2080};

}  // namespace

ZAX_CHECKS(save_stats_fill_c5)
{
    (void)cls;
    (void)n;
    const Test tests[] = {
        {"CZaxSaveGameStats::Fill", ptrv((void *)&from_clear_stack), CALL_THIS, 0, 0, 600, s_fill, STUBS, NSTUBS},
    };
    if (!g_globals.n)
        for (DWORD a : GLOBALS_C5)
            g_globals.add(a, 4);
    Tally t{"save stats fill c5: CZaxSaveGameStats::Fill"};
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 95000);
    t.report();
}

}  // namespace zax
