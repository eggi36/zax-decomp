// Checks of batch C4's window slots (c4_windows.cpp) against the original with the toolkit's harness and the
// screens' setup (checks_ui_screens.h), as phase 4's checks of the same windows (checks_ui_inventory.cpp): the
// window under test is window 0; its buttons, tabs, list, bar and frame are fakes; the custom rules' options,
// the inventory, the item definition and the character are fakes of the extra area or game objects; the lists
// the slots load from (sequences 0x6c0540, thumbs 0x6c22e0, item images 0x6c0aa8, fonts 0x6be1e0) get a fake
// vtable (slot 1 a name, slot 3 an index, slot 37 a load) and records of the case. The frame filter and the
// colour are real (locals of the slot, compared through the stand-ins' arguments).
#include "checks_ui_screens.h"

namespace zax {

int hooks_c4_windows(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"c4_windows", &hooks_c4_windows};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x5a1d40, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::InventoryDisplay"},
    {0x40c170, CV_THIS, 1, K_VALUE, "CJWindow::Key"},
    {0x5a07e0, CV_THIS, 0, K_VALUE, "CZaxWorld::CloseInventory"},
    {0x40a4b0, CV_THIS, 2, K_VALUE, "CJTabControl::Select"},
    {0x4b3a70, CV_THIS, 5, K_VALUE, "Frame_FillRectXY"},
    {0x4b33f0, CV_THIS, 5, K_VALUE, "Frame_DrawRectXY"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4a6b90, CV_THIS, 5, K_VALUE, "CFont::DrawInt"},
    {0x482de0, CV_THIS, 0, K_SMALL, "DynamicProperties_Count"},
    {0x4268d0, CV_FAST, 0, K_SMALL, "Inventory_ItemCount", "vv"},
    {0x428b10, CV_CDECL, 0, K_FIXED2, "Player_Inventory"},
    {0x5b2aa0, CV_FAST, 0, K_SMALL, "Inventory_ItemInCategoryOrder", "vv"},
    {0x426070, CV_THIS, 0, K_SMALL, "CInventory_ItemInFocus"},
    {0x4dd480, CV_THIS, 0, K_FIXED4, "ItemRef_Definition"},
    {0x4da780, CV_THIS, 1, K_VALUE, "CItemListWindow::ShowItem"},
    {0x40bcc0, CV_THIS, 2, K_VALUE, "CJWindow::MouseDown"},
    {0x48d8f0, CV_THIS, 1, K_FIXED3, "DefinitionList_Object"},
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4a9a30, CV_THIS, 5, K_VALUE, "CFrame::Load", "ssavv"},
    {0x414560, CV_THIS, 1, K_VALUE, "ImageScrollBar::SetColour"},
    {0x4145e0, CV_THIS, 2, K_VALUE, "ImageScrollBar::SetUpImages"},
    {0x414600, CV_THIS, 2, K_VALUE, "ImageScrollBar::SetDownImages"},
    {0x414620, CV_THIS, 1, K_VALUE, "ImageScrollBar::SetTrackImage"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x4f5d80, CV_THIS, 0, K_FIXED1, "World::FirstCharacter"},
    {0x4267e0, CV_THIS, 0, K_FIXED2, "LayerPart_GetInventory"},
    {0x5b5f10, CV_CDECL, 0, K_VALUE, "CZaxInventoryItemDefinition::ClassInfo"},
    {0x4f1050, CV_THIS, 0, K_VALUE, "World::CurrentLayer"},
    {0x41a740, CV_THIS, 5, K_BOOL, "CAction::ExecuteFrom", "vvvvs"},
    // the dialogs' and pages'
    {0x517a80, CV_THIS, 1, K_VALUE, "WindowModelPreview::SetModel"},
    {0x46d450, CV_THIS, 0, K_VALUE, "NetOptionsDialog::ApplyColours"},
    {0x470ac0, CV_THIS, 0, K_BOOL, "ControlConfig::CheckJoystick2"},
    {0x46ffa0, CV_THIS, 0, K_VALUE, "ControlConfig::CheckJoystick"},
    {0x470ce0, CV_THIS, 1, K_TEXT, "ControlConfig::IniWord1"},
    {0x470b60, CV_THIS, 2, K_VALUE, "ControlConfig::BindingText"},
    {0x402f30, CV_THIS, 1, K_VALUE, "CFrameWindow::Draw"},
    {0x4d2570, CV_FAST, 1, K_VALUE, "Platform_StartProcess", "ssv"},
    {0x46fb00, CV_THIS, 0, K_BOOL, "ControlConfig::Changed"},
    {0x46fb60, CV_THIS, 1, K_VALUE, "ControlConfig::WriteIni"},
    {0x46fa10, CV_THIS, 0, K_VALUE, "ControlConfig::RestoreCopy"},
    {0x46fad0, CV_THIS, 0, K_VALUE, "ControlConfig::RestoreCopy2"},
    {0x470d20, CV_THIS, 1, K_VALUE, "ControlConfig::SetJoystick"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vvsv"},
    {0x4a4740, CV_FAST, 2, K_VALUE, "Ini_WriteFloat", "vvsv"},
    {0x5abe30, CV_CDECL, 0, K_VALUE, "ClientOptionsReader_Get"},
    {0x5abfe0, CV_THIS, 0, K_VALUE, "ClientOptionsReader::Write"},
    {0x40bde0, CV_THIS, 2, K_VALUE, "CJWindow::MouseUp"},
    {0x4d3c80, CV_THIS, 0, K_VALUE, "ServerDialog::FillGameTypes"},
    {0x4d3e20, CV_THIS, 0, K_VALUE, "ServerDialog::FillMaps"},
    {0x4d3f90, CV_THIS, 0, K_VALUE, "ServerDialog::OnOk"},
    {0x562970, CV_THIS, 2, 'e', "SaveLoadScreen::SaveLoadScreen"},
    {0x4040c0, CV_THIS, 2, K_VALUE, "CDialog::ReplaceWindow"},
    {0x5be4a0, CV_THIS, 1, 'e', "CMainMenu_ctor"},
    {0x4cdf30, CV_THIS, 0, K_VALUE, "App_Window"},
    {0x5ac560, CV_CDECL, 0, K_VALUE, "ContinueReader_Get"},
    {0x5ac500, CV_THIS, 0, K_VALUE, "ContinueReader::Write"},
    {0x5127e0, CV_THIS, 0, K_VALUE, "CMissionManager::Current"},
    {0x509910, CV_FAST, 7, K_SMALL, "MissionObjectives_Draw", "vvvvvvvbv"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x40d2a0, CV_THIS, 1, K_VALUE, "CJWindow::SetRect", "r"},
    {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"},
    {0x413ce0, CV_THIS, 1, K_VALUE, "CJScrollBar::SetRangeFrom"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x4a6be0, CV_THIS, 0, K_SMALL, "Sequence_Height"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"},
    {0x57aef0, CV_CDECL, 0, K_FIXED6, "ResourceCache_Get"},
    {0x57b1b0, CV_THIS, 1, K_VALUE, "ResourceCache_SetLimit"},
    {0x57b1d0, CV_THIS, 1, K_VALUE, "ResourceCache_SetScale"},
    {0x407630, CV_THIS, 0, K_VALUE, "CJListBox::RemakeBar"},
    {0x402890, CV_THIS, 2, K_VALUE, "CJHeader::SetColumnWidth"},
    {0x4028f0, CV_THIS, 1, K_SMALL, "CJHeader::ColumnWidth"},
    {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);

// The lists' fake vtable: slot 1 the name of an index, slot 3 the index of a name, slot 37 a load
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7c01, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *l, const char *name, int a)
{
    log_event(0x7c02, fnv_text(2166136261u, name), (DWORD)a ^ ptrv(l));
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7c03, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    log_event(0x7c04, rec, 0);
    return 0xf0000000u | (rec & 0xffff);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *, DWORD answer)
{
    if (cls == -1)
        if (const int t = template_of((DWORD)slot))
            made(answer, t);
}

char g_object_args[MAXSLOT + 1];
BYTE *g_def;

// Records of a list at `list` (+0x30 records, +0x34 count) of the case
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

constexpr DWORD LISTS[] = {0x6c0540, 0x6c22e0, 0x6c0aa8, 0x6be1e0};

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
    // the inventory display options (+0x3c: +0x5c columns, +0x60 the cell size, +0x64 the font, +0x68/+0x6c the
    // count's offset, +0x88/+0x98 the focus frame's colours)
    BYTE *opt = xalloc(0x200);
    for (DWORD o = 8; o < 0x200; o += 4)
        at_<int>(opt, o) = g.range(-50, 600);
    at_(opt, 0x3c + 0x5c) = 1 + g.below(6), at_(opt, 0x3c + 0x60) = g.below(40), at_(opt, 0x3c + 0x64) = g.below(8);
    g_case.fixed[0] = ptrv(opt);
    // the character, the inventory (+0x20 the count), the definition (+0x10 its image, +0xf4 the switches,
    // +0x80…+0x94 the actions)
    make_window(W_OTHER, C_OBJECT);
    g_case.fixed[1] = g.chance(6) ? 0 : ptrv(win(W_OTHER));
    BYTE *inv = make_window(W_APP, C_OBJECT);
    at_(inv, 0x20) = g.chance(4) ? 0 : 1 + g.below(20);
    g_case.fixed[2] = ptrv(inv);
    g_case.fixed[3] = ptrv(win(W_OBJECT));
    BYTE *def = xalloc(0x100);
    at_(def, 0x10) = g.below(8);
    at_(def, 0xf4) = g.next();
    for (DWORD o = 0x80; o <= 0x94; o += 4)
        at_(def, o) = g.chance(3) ? 0 : g.next();
    g_def = def;
    g_case.fixed[4] = g.chance(6) ? 0 : ptrv(def);
    object_args(g_object_args, "0:1,5:1,6:1,23:4,24:2,26:1,39:0,45:0");
    set_kind(C_OBJECT, 76, K_SMALL);
    set_kind(C_OBJECT, 77, K_SMALL);
    set_kind(C_OBJECT, 22, K_SMALL);
    set_kind(C_NUMBER, 48, K_FLOAT);
    set_kind(C_SLIDER, 77, K_VALUE);
    g_case.fixed[6] = ptrv(make_window(W_SCREEN, C_OBJECT));
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_OBJECT, 45, K_SMALL);
    g_case.argspec[23][1] = A_RECT;
    g_case.argspec[23][2] = A_RECT;
    for (DWORD l : LISTS)
        records(g, l);
    case_global(0x6c0adc, g.below(30));
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : g.next();
    at_(win(W_APP), 0x2e0) = ptrv(holder);
}

DWORD pick(Gen &g, BYTE *w, DWORD first, DWORD last)
{
    const DWORD k = g.below((last - first) / 4 + 2);
    return first + k * 4 <= last ? at_(w, first + k * 4) : g.chance(2) ? 0 : g.next();
}

// The inventory screen: its state, tab buttons (+0x114 close, +0x118… the tabs) and tab control
void s_screen(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = g.below(5);
    for (DWORD o = 0x114; o <= 0x120; o += 4)
        at_(w, o) = ptrv(win(1 + (int)g.below(4)));
    at_(w, 0x124) = ptrv(win(W_OTHER2));
    g_self = w;
}
void s_screen_key(Gen &g)
{
    s_screen(g);
    g_args[0] = g.chance(2) ? 0x1b : g.below(40);
}
void s_screen_notify(Gen &g)
{
    s_screen(g);
    g_args[0] = pick(g, g_self, 0x114, 0x120);
    g_args[1] = g.chance(4) ? g.below(3) : 0;
}
// The item list: content size, its line image, the scroll position
void s_list(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x34) = g.below(160), at_(w, 0x38) = g.below(120);
    at_(w, 0x100) = ptrv(win(W_OTHER2));
    BYTE *scroll = xalloc(8);
    at_<int>(scroll, 0) = g.range(-5, 100), at_<int>(scroll, 4) = g.range(-5, 100);
    at_(w, 0xfc) = g.chance(3) ? 0 : ptrv(scroll);
    at_<int>(w, 0x24) = g.range(-20, 200), at_<int>(w, 0x28) = g.range(-20, 200);
    g_self = w;
}
void s_draw(Gen &g)
{
    s_list(g);
    g_args[0] = ptrv(win(W_OBJECT));
    g_case.fixed[4] = ptrv(g_def);   // (the original reads the definition without a test)
}
void s_drag(Gen &g)
{
    s_list(g);
    BYTE *p = xalloc(8);
    at_<int>(p, 0) = g.range(0, 400), at_<int>(p, 4) = g.range(0, 300);
    g_args[0] = ptrv(p), g_args[1] = g.next(), g_args[2] = g.below(4), g_args[3] = g.next();
}
void s_down(Gen &g)
{
    s_list(g);
    g_args[0] = g.next(), g_args[1] = g.next();
}
void s_key(Gen &g)
{
    s_list(g);
    static const DWORD KEYS[] = {6, 10, 11, 12, 7, 0x1b};
    g_args[0] = KEYS[g.below(6)];
}
// The display page: the list +0x100, the bar +0x104, the buttons +0x10c…+0x120
void s_page(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = ptrv(win(W_OTHER2));
    at_(w, 0x104) = ptrv(win(W_PLAIN_T));
    for (DWORD o = 0x10c; o <= 0x120; o += 4)
        at_(w, o) = ptrv(win(1 + (int)g.below(4)));
    g_self = w;
}
void s_page_notify(Gen &g)
{
    s_page(g);
    g_args[0] = pick(g, g_self, 0x10c, 0x120);
    g_args[1] = g.chance(4) ? g.below(3) : 0;
}

// A dialog under test (window 0, a frame window) with buttons (fakes 1-4 or the given windows) at the offsets
BYTE *dialog(Gen &g, DWORD first, DWORD last)
{
    BYTE *w = make_plain_window(0, C_FRAME, g);
    for (DWORD o = first; o <= last; o += 4)
        at_(w, o) = ptrv(win(1 + (int)g.below(4)));
    g_self = w;
    return w;
}
void notify_args(Gen &g, DWORD first, DWORD last)
{
    g_args[0] = pick(g, g_self, first, last);
    g_args[1] = g.chance(5) ? g.below(4) : g.below(2);
}
void s_net_options(Gen &g)
{
    dialog(g, 0x13c, 0x154);
    notify_args(g, 0x13c, 0x154);
}
// The controls dialog: its config +0x134, the key buttons +0x138 (count +0x13c; each with a label +0x138 or none),
// the buttons +0x144…+0x154 (+0x154 the auto switch box), the joysticks +0x15c, the combo box +0x164
void controls(Gen &g)
{
    BYTE *w = dialog(g, 0x144, 0x150);
    at_(w, 0x134) = g.chance(5) ? 0 : ptrv(xalloc(0x40));
    const DWORD n = g.below(4);
    BYTE *a = xalloc(4 * (n + 1));
    for (DWORD i = 0; i < n; i++) {
        BYTE *b = make_window(11 + (int)i, C_BUTTON);
        at_(b, 0x138) = g.chance(3) ? 0 : ptrv(win(W_LABEL_T));
        at_(a, 4 * i) = ptrv(b);
    }
    at_(w, 0x138) = ptrv(a), at_(w, 0x13c) = n;
    at_(w, 0x154) = ptrv(make_window(W_EDIT_T, C_CHECKBOX));
    BYTE *joy = xalloc(4 * 21);
    for (DWORD i = 0; i < 21; i++)
        at_(joy, 4 * i) = g.next();
    at_(w, 0x15c) = ptrv(joy);
    at_(w, 0x164) = ptrv(win(W_OBJECT));
    make_window(W_LABEL_T, C_LABEL);
}
void s_controls_draw(Gen &g)
{
    controls(g);
    g_args[0] = ptrv(win(W_OTHER));
}
void s_controls_notify(Gen &g)
{
    controls(g);
    const DWORD k = g.below(4);
    if (k == 0 && at_(g_self, 0x13c))
        g_args[0] = at_(at<BYTE *>(at_(g_self, 0x138)), 4 * g.below(at_(g_self, 0x13c)));
    else if (k == 1)
        g_args[0] = at_(g_self, 0x164);
    else
        g_args[0] = pick(g, g_self, 0x144, 0x154);
    g_args[1] = g.chance(5) ? g.below(4) : g.below(2);
}
void s_save_changes(Gen &g)
{
    dialog(g, 0x134, 0x138);
    BYTE *d = make_plain_window(W_OTHER2, C_WINDOW, g);
    at_(d, 0x134) = g.next();
    at_(g_self, 0x13c) = ptrv(d);
    notify_args(g, 0x134, 0x138);
}
void s_dialog_key(Gen &g)
{
    dialog(g, 0x150, 0x150);
    static const DWORD KEYS[] = {0xd, 0x1b, 0x20, 0xfa, 0xfb, 0xc, 0x21, 0xfc, 0x41};
    g_args[0] = g.chance(3) ? g.below(0x100) : KEYS[g.below(9)];
}
void s_dialog_up(Gen &g)
{
    dialog(g, 0x150, 0x150);
    at_<BYTE>(g_self, 0x100) = (BYTE)g.below(2);
    g_args[0] = g.chance(2) ? 0 : g.below(3), g_args[1] = g.next();
}
void s_dialog_notify(Gen &g)
{
    dialog(g, 0x150, 0x150);
    notify_args(g, 0x150, 0x150);
}
// The server dialog: buttons, the number fields +0x154/+0x160 and their boxes +0x158/+0x164, the settings +0x19c
void s_server(Gen &g)
{
    BYTE *w = dialog(g, 0x13c, 0x17c);
    at_(w, 0x154) = ptrv(make_window(11, C_NUMBER));
    at_(w, 0x160) = ptrv(make_window(12, C_NUMBER));
    at_(w, 0x158) = ptrv(make_window(13, C_CHECKBOX));
    at_(w, 0x164) = ptrv(make_window(W_EDIT_T, C_CHECKBOX));
    at_(w, 0x19c) = ptrv(xalloc(0x40));
    notify_args(g, 0x13c, 0x17c);
}
// The mission-over window: its buttons, the state +0x1ac, the continue flag +0x1b0; the world's slot 1 a game object
void s_mission_over(Gen &g)
{
    BYTE *w = dialog(g, 0x13c, 0x150);
    at_(w, 0x1ac) = g.below(4);
    at_<BYTE>(w, 0x1b0) = (BYTE)g.below(2);
    at_(w, 0x14c) = g.chance(4) ? 0 : at_(w, 0x14c);
    g_case.fixed[5] = ptrv(win(W_OBJECT));
    set_kind(C_OBJECT, 1, K_FIXED5);
    static char args[MAXSLOT + 1];
    object_args(args, "23:2,81:1");   // (the world's slot 23 shows a window, 81 ends the level)
    notify_args(g, 0x13c, 0x150);
    if (g.chance(6))
        g_args[0] = 0;
}
// A list of objectives (+0x100…+0x110)
void objectives(Gen &g, BYTE *w)
{
    for (DWORD o = 0x100; o <= 0x110; o += 4)
        at_(w, o) = g.chance(3) ? g.below(30) : g.next();
}
void s_details(Gen &g)
{
    objectives(g, make_plain_window(0, C_WINDOW, g));
    g_args[0] = ptrv(win(W_OTHER));
}
// The mission display page: the bar +0x104, the objectives list +0x108, the flag +0x10c
void s_mission_rect(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x104) = ptrv(make_plain_window(W_OTHER2, C_WINDOW, g));
    BYTE *l = make_plain_window(W_PLAIN_T, C_WINDOW, g);
    objectives(g, l);
    at_(w, 0x108) = ptrv(l);
    at_<BYTE>(w, 0x10c) = (BYTE)g.below(2);
    BYTE *r = xalloc(16);
    for (int i = 0; i < 4; i++)
        at_<int>(r, 4 * i) = g.range(-10, 600);
    g_args[0] = ptrv(r);
    g_self = w;
}
void s_mission_images(Gen &g)
{
    s_page(g);
}
// The pages' draw: the diagram +0x100 / +0x12c; the options' +0x4c object (its +0x14 a font index)
void s_page_draw(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = g.next(), at_(w, 0x12c) = g.next();
    BYTE *opt = at<BYTE *>(g_case.fixed[0]);
    BYTE *o = xalloc(0x20);
    at_(o, 0x14) = g.below(8);
    at_(opt, 0xe4 + 0x4c) = ptrv(o);
    g_args[0] = ptrv(win(W_OTHER));
    g_self = w;
}
void s_question(Gen &g)
{
    dialog(g, 0x134, 0x138);
    at_(g_self, 0x13c) = ptrv(make_plain_window(W_OTHER2, C_WINDOW, g));
    notify_args(g, 0x134, 0x138);
}
// The sound dialog: the boxes +0x138/+0x144/+0x150, the sliders +0x13c/+0x148/+0x154, the buttons +0x15c…+0x17c;
// the reporter a plain window (its slot 6 without arguments), the resource cache a game object
void s_sound(Gen &g)
{
    BYTE *w = dialog(g, 0x15c, 0x17c);
    at_(w, 0x138) = ptrv(make_window(11, C_CHECKBOX));
    at_(w, 0x144) = ptrv(make_window(12, C_CHECKBOX));
    at_(w, 0x150) = ptrv(make_window(13, C_CHECKBOX));
    at_(w, 0x13c) = ptrv(make_window(W_PARENT, C_SLIDER));
    at_(w, 0x148) = ptrv(make_window(W_OTHER, C_SLIDER));
    at_(w, 0x154) = ptrv(make_window(W_OTHER2, C_SLIDER));
    make_window(W_REPORTER, C_WINDOW);
    const DWORD k = g.below(3);
    g_args[0] = k == 0 ? pick(g, g_self, 0x15c, 0x17c) : k == 1 ? at_(w, 0x138 + 0xc * g.below(2) + 4 * g.below(2)) : at_(w, 0x150 + 4 * g.below(2));
    g_args[1] = g.chance(5) ? g.below(4) : g.below(2);
}
void s_voice_images(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x114) = ptrv(win(W_PLAIN_T));
    g_self = w;
}
// The voice log panel: the list +0x100, +0x104, the header +0x10c, the replay button +0x110, the flags
void s_voice(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = ptrv(make_window(11, C_LISTBOX));
    at_(w, 0x104) = ptrv(win(W_OBJECT));
    BYTE *h = make_plain_window(12, C_HEADER, g);
    at_(w, 0x10c) = ptrv(h);
    at_(w, 0x110) = ptrv(win(W_PLAIN_T));
    at_<BYTE>(w, 0x108) = (BYTE)g.below(2);
    at_<BYTE>(w, 0x118) = (BYTE)g.below(3) != 0;
    at_(at<BYTE *>(g_case.fixed[0]), 0x144 + 0x58) = g.chance(2) ? 0 : g.below(100);
    BYTE *r = xalloc(16);
    g_args[0] = ptrv(r);
    g_self = w;
}
void s_voice_update(Gen &g)
{
    s_voice(g);
    g_args[0] = g.next();
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6c0540, 0x6c0570, 0x6c0574, 0x6c22e0, 0x6c2310, 0x6c2314,
                              0x6c0aa8, 0x6c0ad8, 0x6c0adc, 0x6be1e0, 0x6be210, 0x6be214};

const Test TESTS[] = {
    {"InventoryScreen Key", 0x429cd0, CALL_THIS, 1, ~0u, 100, s_screen_key, STUBS, NSTUBS},
    {"InventoryScreen Notify", 0x429d20, CALL_THIS, 2, ~0u, 100, s_screen_notify, STUBS, NSTUBS},
    {"ItemListWindow Draw", 0x4da290, CALL_THIS, 1, ~0u, 200, s_draw, STUBS, NSTUBS},
    {"ItemListWindow MouseDrag", 0x4da840, CALL_THIS, 4, ~0u, 100, s_drag, STUBS, NSTUBS},
    {"ItemListWindow MouseDown", 0x4da8f0, CALL_THIS, 2, ~0u, 20, s_down, STUBS, NSTUBS},
    {"ItemListWindow Key", 0x4da910, CALL_THIS, 1, ~0u, 200, s_key, STUBS, NSTUBS},
    {"InventoryDisplayPage LoadImages", 0x4dade0, CALL_THIS, 0, ~0u, 100, s_page, STUBS, NSTUBS},
    {"InventoryDisplayPage Notify", 0x4db630, CALL_THIS, 2, ~0u, 200, s_page_notify, STUBS, NSTUBS},
    {"NetOptionsDialog Notify", 0x46d520, CALL_THIS, 2, ~0u, 100, s_net_options, STUBS, NSTUBS},
    {"ControlsDialog Draw", 0x471ac0, CALL_THIS, 1, ~0u, 100, s_controls_draw, STUBS, NSTUBS},
    {"ControlsDialog Notify", 0x471c30, CALL_THIS, 2, 0, 300, s_controls_notify, STUBS, NSTUBS},
    {"SaveChangesDialog Notify", 0x472330, CALL_THIS, 2, ~0u, 60, s_save_changes, STUBS, NSTUBS},
    {"DialogDisplayWindow Key", 0x47c120, CALL_THIS, 1, ~0u, 60, s_dialog_key, STUBS, NSTUBS},
    {"DialogDisplayWindow MouseUp", 0x47c2b0, CALL_THIS, 2, ~0u, 40, s_dialog_up, STUBS, NSTUBS},
    {"DialogDisplayWindow Notify", 0x47c9b0, CALL_THIS, 2, ~0u, 40, s_dialog_notify, STUBS, NSTUBS},
    {"ServerDialog Notify", 0x4d40b0, CALL_THIS, 2, ~0u, 300, s_server, STUBS, NSTUBS},
    {"WindowMissionOver Notify", 0x50e9f0, CALL_THIS, 2, 0, 300, s_mission_over, STUBS, NSTUBS},
    {"WindowMissionDetails Draw", 0x50ff20, CALL_THIS, 1, ~0u, 60, s_details, STUBS, NSTUBS},
    {"MissionDisplayPage SetRect", 0x5101d0, CALL_THIS, 1, 0, 100, s_mission_rect, STUBS, NSTUBS},
    {"MissionDisplayPage LoadImages", 0x510310, CALL_THIS, 0, ~0u, 60, s_mission_images, STUBS, NSTUBS},
    {"MissionDisplayPage Draw", 0x510590, CALL_THIS, 1, ~0u, 200, s_page_draw, STUBS, NSTUBS},
    {"QuestionDialog Notify", 0x515820, CALL_THIS, 2, ~0u, 40, s_question, STUBS, NSTUBS},
    {"SoundDialog Notify", 0x57f870, CALL_THIS, 2, ~0u, 400, s_sound, STUBS, NSTUBS},
    {"VoiceLogPanel LoadImages", 0x594e30, CALL_THIS, 0, ~0u, 60, s_voice_images, STUBS, NSTUBS},
    {"VoiceLogPanel SetRect", 0x5950c0, CALL_THIS, 1, 0, 100, s_voice, STUBS, NSTUBS},
    {"VoiceLogPanel Draw", 0x595210, CALL_THIS, 1, ~0u, 100, s_page_draw, STUBS, NSTUBS},
    {"VoiceLogPanel Update", 0x595650, CALL_THIS, 1, ~0u, 60, s_voice_update, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(c4_windows)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"c4 windows: the inventory, mission, voice log, dialogue display and option dialogs' slots"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 51000);
    t.report();
}

}  // namespace zax
