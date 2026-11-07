// Checks of the save/load screen's constructor (ui_save_load_ctor.cpp) against the original with the toolkit's
// harness and the screens' setup (checks_ui_screens.h): the screen is built in the extra area; the toolkit's
// constructors and functions, the directory list, the object stream, the statistics' functions, the name lists' Add,
// the details page and the update are recording stand-ins (the statistics a game object with a ZString at +0x68,
// their descriptions and names texts of the case with tabs); the file spec, the name lists' constructor, the pointer
// array's Add, the ZString code, strchr, _stricmp and memmove are the exe's; the font list 0x6be1e0 gets the vtable of
// a fake of this file and records of the case.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_save_load_ctor(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_save_load_ctor", &hooks_ui_save_load_ctor};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x54f470, CV_THIS, 1, K_VALUE, "FieldNameList::Add", "s"},
    {0x4038a0, CV_THIS, 0, K_VALUE, "CFrameWindow::AddCloseButton"},
    {0x40c600, CV_THIS, 1, K_VALUE, "CJWindow::SetTransparent"},
    {0x403930, CV_THIS, 0, K_VALUE, "CFrameWindow::AddResizeGrip"},
    {0x4a2c50, CV_FAST, 0, 'e', "DirList_Ctor", "xx"},
    {0x4a2ea0, CV_FAST, 4, K_FIXED0, "DirList_Read", "xxxvvv"},
    {0x4a3310, CV_FAST, 1, K_FIXED1, "DirList_NameAt", "xxv"},
    {0x4a33b0, CV_FAST, 1, K_TEXT, "DirList_Path", "xxs"},
    {0x4a2db0, CV_FAST, 0, K_VALUE, "DirList_Dtor", "xx"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x5bc230, CV_THIS, 0, 'e', "CZaxSaveGameStats::Ctor"},
    {0x5240b0, CV_FAST, 0, 'e', "ObjectStream_Ctor", "xx"},
    {0x5242b0, CV_FAST, 3, K_BOOL, "ObjectStream_OpenFile", "xxsvv"},
    {0x416af0, CV_THIS, 1, K_BOOL, "Object_ReadFromStream", "x"},
    {0x524340, CV_FAST, 0, K_VALUE, "ObjectStream_Finish", "xx"},
    {0x5241e0, CV_FAST, 0, K_VALUE, "ObjectStream_Dtor", "xx"},
    {0x5bc910, CV_THIS, 1, K_VALUE, "CZaxSaveGameStats::SetPattern"},
    {0x5bcb60, CV_THIS, 1, 'p', "CZaxSaveGameStats::Description", "x"},
    {0x5bc810, CV_THIS, 1, 'p', "CZaxSaveGameStats::Name", "x"},
    {0x5bc2a0, CV_THIS, 0, K_VALUE, "CZaxSaveGameStats::Fill"},
    {0x4020d0, CV_THIS, 1, 'e', "CJHeader::CJHeader"},
    {0x402770, CV_THIS, 5, K_VALUE, "CJHeader::AddColumn"},
    {0x402bb0, CV_THIS, 0, K_VALUE, "CJHeader::Recalc"},
    {0x4028f0, CV_THIS, 1, K_SMALL, "CJHeader::ColumnWidth"},
    {0x4041f0, CV_THIS, 4, 'e', "CJListBox::CJListBox"},
    {0x407870, CV_THIS, 1, K_VALUE, "CJListBox::SetRows"},
    {0x406aa0, CV_THIS, 1, K_VALUE, "CJListBox::SetHeader"},
    {0x406b60, CV_THIS, 1, K_VALUE, "CJListBox::SetSortDown"},
    {0x406b70, CV_THIS, 2, K_VALUE, "CJListBox::Sort"},
    {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"},
    {0x4100f0, CV_THIS, 1, K_VALUE, "Button::Set114"},
    {0x409be0, CV_THIS, 1, 'e', "CJTabControl::CJTabControl"},
    {0x40a2f0, CV_THIS, 1, K_VALUE, "CJTabControl::Add"},
    {0x40a4b0, CV_THIS, 2, K_VALUE, "CJTabControl::Select"},
    {0x40c9b0, CV_THIS, 1, K_VALUE, "CJWindow::RemoveChild"},
    {0x564090, CV_THIS, 0, K_VALUE, "SaveLoadScreen::ShowDetails"},
    {0x5637d0, CV_THIS, 1, K_VALUE, "SaveLoadScreen::Update"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);

constexpr int W_HEADER_T = 11, W_TABS_T = 13, W_STATS = 14;

// The font list's fake vtable: slot 3 the index of a name
DWORD g_list_vt[40];
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7f02, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(5);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

// The statistics' descriptions (at least two tabs: saving takes the text between them out) and names
const char *const DESCRIPTIONS[5] = {"M3\tMy Save\tMon Oct 27 2026", "\tQuick\t", "M12\tA\tB\tC", "\t\t", "M1\tLong name of a save game\tdate"};

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x4020d0: made(answer, W_HEADER_T); return;
    case 0x409be0: made(answer, W_TABS_T); return;
    case 0x5bc230:   // a statistics object: a game object, its path a ZString
        if (in_block(answer)) {
            at_(at<BYTE *>(answer), 0) = at_(win(W_OBJECT), 0);
            at<void(THISCALL *)(void *)>(0x4dec90)(at<BYTE *>(answer) + 0x68);
        }
        return;
    case 0x5bcb60:   // the description into *out (constructed)
        at<void(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(args[0]), DESCRIPTIONS[g_case.values.below(5)]);
        return;
    case 0x5bc810:   // the name into *out (constructed)
        at<void(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(args[0]), TEXTS[g_case.values.below(8)]);
        return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];
const char *const NAMES[3] = {"QuickSave.sav", "quicksave.SAV", "Save 7.sav"};

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[3] = ptrv((void *)&list_find);
    }
    screen_base(g, &effect);
    make_window(W_HEADER_T, C_HEADER);
    make_window(W_TABS_T, C_TABS);
    make_window(W_STATS, C_OBJECT);
    object_args(g_object_args, "1:1,5:0,28:0");
    // the saves, the name the list gives, the current game's statistics, an open screen
    g_case.fixed[0] = g.chance(4) ? 0 : 1 + g.below(4);
    g_case.fixed[1] = ptrv(NAMES[g.below(3)]);
    at_(win(W_APP), 0x350) = ptrv(win(W_STATS));
    case_global(0x71320c, g.chance(2) ? 0 : ptrv(win(W_OBJECT)));
    // the font list
    const DWORD n = g.below(5);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    case_global(0x6be1e0, ptrv(g_list_vt));
    case_global(0x6be210, g.chance(8) ? 0 : ptrv(recs));
    case_global(0x6be214, n);
    g_self = xalloc(0x1b0);
    g_args[0] = ptrv(win(W_PARENT));
    g_args[1] = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
}

void s_none(Gen &) {}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x71320c, 0x6be1e0, 0x6be210, 0x6be214};

const Test TESTS[] = {
    {"SaveLoadScreen ctor", 0x562970, CALL_THIS, 2, ~0u, 500, s_none, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_save_load_ctor)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui save load ctor: the save/load screen's constructor"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 53000);
    t.report();
}

}  // namespace zax
