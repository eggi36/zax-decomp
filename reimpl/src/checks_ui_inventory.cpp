// Checks of the inventory screen (ui_inventory.cpp) against the original with the toolkit's harness and the
// screens' setup (checks_ui_screens.h). The screen and the display page under test are fake windows (window 0)
// or real objects of the extra area (the constructors); their tabs, pages, buttons, the character, the
// inventory, the sequences and the reporter are fakes; the sequence list 0x6c0540 gets the vtable of a fake of
// this file (slot 1 a name, slot 3 an index, slot 37 a loaded object) and records of the case. Never touched: the
// mouse cursor (GetXY, SetPos, Hide and the shown test are stand-ins), the sound list (stand-ins), the game's
// error box and exit (the item records' categories are always item types).
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_inventory(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_inventory", &hooks_ui_inventory};
Globals g_globals;

#define INVENTORY_STUBS \
    UIC_SCREEN_STUBS, {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "asa"}, \
        {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"}, {0x5a1d40, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::InventoryDisplay"}, \
        {0x429de0, CV_CDECL, 0, K_FIXED0, "InventoryDisplayOptions"}, {0x5a1fa0, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::HudOptions"}, \
        {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"}, {0x409be0, CV_THIS, 1, 'e', "CJTabControl::CJTabControl"}, \
        {0x40a2b0, CV_THIS, 1, K_VALUE, "CJTabControl::ShowHeaders"}, {0x40a2f0, CV_THIS, 1, K_VALUE, "CJTabControl::Add"}, \
        {0x40a4b0, CV_THIS, 2, K_VALUE, "CJTabControl::Select"}, {0x50ffe0, CV_THIS, 1, 'e', "WindowMissionDetails_Page"}, \
        {0x5948b0, CV_THIS, 1, 'e', "VoiceLogPanel_Ctor"}, {0x4109a0, CV_THIS, 3, 'e', "RadioButton::RadioButton", "vsv"}, \
        {0x410ab0, CV_THIS, 3, 'e', "ToggleButton::ToggleButton", "vsv"}, {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"}, \
        {0x4101c0, CV_THIS, 1, K_VALUE, "Button::ClearTip"}, {0x4100c0, CV_THIS, 2, K_VALUE, "Button::SetTip"}, \
        {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"}, {0x4a9a30, CV_THIS, 5, K_VALUE, "CFrame::Load", "ssvvv"}, \
        {0x57e7b0, CV_THIS, 1, K_VALUE, "CSoundList::PrecacheByName", "s"}, {0x57e890, CV_THIS, 1, K_VALUE, "CSoundList::PlayIndex"}, \
        {0x4f5d80, CV_THIS, 0, K_FIXED1, "World::FirstCharacter"}, {0x4267e0, CV_THIS, 0, K_VALUE, "LayerPart_GetInventory"}, \
        {0x46ed70, CV_THIS, 1, K_BOOL, "BitSet::Test"}, {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"}, \
        {0x4cdf30, CV_THIS, 0, K_FIXED3, "App_Window"}, {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"}, \
        {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"}, {0x428b10, CV_CDECL, 0, K_FIXED2, "Player_Inventory"}, \
        {0x5b2aa0, CV_FAST, 0, K_VALUE, "Inventory_ItemInCategoryOrder"}, {0x40d940, CV_THIS, 1, K_VALUE, "CJWindow::ScrollToShow", "r"}, \
        {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"}, {0x4144b0, CV_THIS, 2, 'e', "ImageScrollBar::ImageScrollBar"}, \
        {0x413ce0, CV_THIS, 1, K_VALUE, "CJScrollBar::SetRangeFrom"}, {0x426070, CV_THIS, 0, K_SMALL, "CInventory_ItemInFocus"}, \
        {0x4dd480, CV_THIS, 0, K_FIXED4, "ItemRef_Definition"}, {0x5b5f10, CV_CDECL, 0, K_VALUE, "CZaxInventoryItemDefinition::ClassInfo"}, \
        {0x5b7740, CV_THIS, 0, K_TEXT, "ItemDefinition::UseLabel"}, {0x5b76e0, CV_THIS, 0, K_TEXT, "ItemDefinition::PlaceLabel"}, \
        {0x5b7770, CV_THIS, 0, K_TEXT, "ItemDefinition::UndefinedLabel"}, {0x5b7710, CV_THIS, 0, K_TEXT, "ItemDefinition::DisableLabel"}, \
        {0x5184e0, CV_THIS, 2, K_VALUE, "Cursor::GetXY"}, {0x518570, CV_THIS, 2, K_VALUE, "Cursor::SetPos"}, \
        {0x518690, CV_THIS, 0, K_BOOL, "Cursor::IsShown"}, {0x5185f0, CV_THIS, 0, K_VALUE, "Cursor::Hide"}, \
        {0x4251c0, CV_THIS, 1, K_SMALL, "CInventory_CountOfCategory"}, {0x425290, CV_THIS, 1, K_FIXED5, "CInventory_CurIndex"}, \
        {0x429e50, CV_CDECL, 0, K_VALUE, "CInventoryItem::ClassInfo"}, {0x4dd580, CV_THIS, 0, K_VALUE, "ItemRef_Category"}, \
        {0x4252e0, CV_THIS, 1, K_VALUE, "CInventory_ChangingToItem"}, {0x5b9400, CV_CDECL, 0, K_VALUE, "CZaxInventoryTypeDefinition::ClassInfo"}, \
        {0x4298d0, CV_THIS, 4, K_VALUE, "InventoryHUD_Helper", "vvxx"}, {0x5b7f00, CV_THIS, 5, K_VALUE, "ZaxNetPlayer::ItemRecord"}, \
        {0x425320, CV_THIS, 1, K_FLOAT, "CInventory_SecondsIntoChange"}, {0x425300, CV_THIS, 1, K_FLOAT, "CInventory_SecondsToChange"}, \
        {0x48d030, CV_THIS, 0, K_FLOAT, "EntityDestructable_HitPointsLeft"}, {0x5bac80, CV_THIS, 0, K_WINDOW_NN, "ZaxNet_PlayerOf"}, \
        {0x5ba340, CV_THIS, 1, K_VALUE, "ZaxNetPlayer::SetEC"}, {0x5ba450, CV_THIS, 1, K_VALUE, "ZaxNetPlayer::SetRecordCount"}, \
        {0x5a01a0, CV_THIS, 0, K_VALUE, "CZaxWorld::GameTypeIndex"}, {0x59ff80, CV_THIS, 0, K_FIXED3, "Game_AppGameType"}

const Stub STUBS[] = {
    INVENTORY_STUBS,
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);
// The item records: every category is an item type (the original stops the game otherwise)
const Stub STUBS_RECORDS[] = {
    INVENTORY_STUBS,
    {0x416790, CV_THIS, 1, K_ONE, "Object_IsKindOf"},
};
constexpr int NSTUBS_RECORDS = sizeof STUBS_RECORDS / sizeof STUBS_RECORDS[0];

constexpr int W_TABS = 11, W_SEQ = 12, W_CHAR = 13;

// The sequence list's fake vtable: slot 1 the name of an index, slot 3 the index of a name, slot 37 a load
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7f01, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7f02, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7f03, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    log_event(0x7f04, rec, 0);
    return 0xf0000000u | (rec & 0xffff);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)args;
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x409be0: made(answer, W_TABS); return;
    case 0x4109a0:
    case 0x410ab0: made(answer, W_BUTTON_T); return;
    case 0x50ffe0:
    case 0x5948b0:
    case 0x4144b0: made(answer, W_PLAIN_T); return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

// Records of a list at `list` (+0x30 records, +0x34 count) of the case
void records(Gen &g, DWORD list, DWORD object)
{
    const DWORD n = g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : object ? object : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
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
    // the options: the inventory display's (+0x3c: rows, columns, the cell size, positions) and the HUD's
    BYTE *opt = xalloc(0x100);
    for (DWORD o = 8; o < 0x100; o += 4)
        at_<int>(opt, o) = g.range(-50, 600);
    at_(opt, 0x20) = g.below(6);
    at_(opt, 0x3c + 0x58) = 1 + g.below(4), at_(opt, 0x3c + 0x5c) = 1 + g.below(8), at_(opt, 0x3c + 0x60) = g.below(64);
    const float length = g.chance(5) ? 0.0f : (float)g.range(1, 40) / 8.0f;
    memcpy(opt + 0x18, &length, 4);
    at_(opt, 0x1c) = g.below(10), at_(opt, 0x20) = g.below(10);
    g_case.fixed[0] = ptrv(opt);
    // the character (a game object; slot 36 gives another), the inventory (+0x20 the count), the definition
    make_window(W_CHAR, C_OBJECT);
    g_case.fixed[1] = g.chance(6) ? 0 : ptrv(win(W_CHAR));
    BYTE *inv = make_window(W_APP, C_OBJECT);
    at_(inv, 0x20) = g.chance(4) ? 0 : 1 + g.below(30);
    g_case.fixed[2] = ptrv(inv);
    g_case.fixed[3] = ptrv(win(W_OBJECT));
    BYTE *def = xalloc(0x100);
    at_(def, 0xf4) = g.next();
    g_case.fixed[4] = g.chance(5) ? 0 : ptrv(def);
    g_case.fixed[5] = g.chance(4) ? 0xffffffff : g.below(10);
    object_args(g_object_args, "0:1,1:1,23:2,24:2,26:1,33:1,40:2");
    set_kind(C_OBJECT, 36, K_FIXED3);
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_OBJECT, 45, K_SMALL);
    set_kind(C_OBJECT, 27, K_VALUE);
    // the tab control (its pages +0x104/+0x108) and the screen's sequence
    BYTE *tabs = make_window(W_TABS, C_TABS);
    const DWORD pages = g.below(4);
    BYTE *d = xalloc(4 * (pages + 1));
    for (DWORD i = 0; i < pages; i++)
        at_(d, 4 * i) = ptrv(win(W_PLAIN_T));
    at_(tabs, 0x104) = pages, at_(tabs, 0x108) = ptrv(d);
    make_window(W_SEQ, C_OBJECT);
    // the lists
    case_global(0x6c0540, ptrv(g_list_vt));
    records(g, 0x6c0540, 0);
    records(g, 0x6be1e0, 0);
    case_global(0x6c0d64, g.chance(2) ? 0 : g_case.fixed[4]);
    case_global(0x6c0d68, g.chance(3) ? 0 : 1);
    case_global(0x6c2cfc, g.chance(3) ? 0 : ptrv(xalloc(0x140)));
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : g.next();
    at_(win(W_APP), 0x2e0) = ptrv(holder);
}

void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(6) ? 0 : ptrv(win(W_PARENT));
}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }

// The screen under test (window 0): its state, height, sequence, tabs and tab buttons; its parent
void screen(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = g.below(5);
    const float h = g.chance(4) ? 0.0f : (float)g.range(-20, 300) / 3.0f;
    memcpy(w + 0x104, &h, 4);
    at_(w, 0x108) = (DWORD)g.range(-10, 250);
    at_(w, 0x10c) = ptrv(win(W_SEQ));
    at_(w, 0x110) = g.chance(3) ? 0 : ptrv(win(W_OBJECT));
    for (DWORD o = 0x114; o <= 0x120; o += 4)
        at_(w, o) = ptrv(win(W_BUTTON_T));
    at_(w, 0x124) = ptrv(win(W_TABS));
    link_child(win(W_PARENT), w);
    at_(win(W_APP), 0x340) = ptrv(w);
    BYTE *slide = xalloc(0x18);
    at_(slide, 0) = g.below(4);
    const float t = (float)g.range(0, 20) / 7.0f;
    memcpy(slide + 4, &t, 4);
    slide[0x14] = g.chance(2);
    at_(win(W_APP), 0x344) = ptrv(slide);
    g_self = w;
}
// The "Inventory Window" sequence must exist (the original reads it without a test): every record the screen's
// sequence
void s_init(Gen &g)
{
    screen(g);
    const DWORD n = 6;
    BYTE *recs = xalloc(n * 0x80);
    for (DWORD i = 0; i < n; i++)
        at_(recs, i * 0x80 + 0x78) = ptrv(win(W_SEQ));
    case_global(0x6c0570, ptrv(recs));
    case_global(0x6c0574, n);
}
// Open reads the first character without a test
void s_open(Gen &g)
{
    screen(g);
    g_case.fixed[1] = ptrv(win(W_CHAR));
}
void s_screen_dtor(Gen &g)
{
    screen(g);
    s_flags(g);
}
void s_dt(Gen &g)
{
    screen(g);
    const float dt = g.chance(5) ? 0.0f : (float)g.range(-3, 40) / 16.0f;
    memcpy(&g_args[0], &dt, 4);
}
void s_frame(Gen &g)
{
    screen(g);
    g_args[0] = ptrv(win(W_OBJECT));
}
void s_record(Gen &g)
{
    screen(g);
    g_args[0] = g.next(), g_args[1] = g.next(), g_args[2] = g.below(8), g_args[3] = g.next();
    g_args[4] = ptrv(win(W_APP));
}
void s_records(Gen &g)
{
    screen(g);
    // the item categories: every record an item type (+0x18 its switches, +0x24 a value), some empty
    const DWORD n = g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        BYTE *cat = xalloc(0x30);
        at_(cat, 0x18) = g.below(8), at_(cat, 0x24) = g.next();
        at_(recs, i * 0x80 + 0x78) = g.chance(5) ? 0 : ptrv(cat);
    }
    case_global(0x6c0830, ptrv(recs));
    case_global(0x6c0834, n);
    g_args[0] = g.chance(5) ? 0 : ptrv(win(W_CHAR));
    g_args[1] = g.next();
    g_args[2] = ptrv(win(W_APP));
    g_args[3] = g.chance(2);
}
void s_page_base(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    g_args[1] = ptrv(TEXTS[g.below(8)]);
}
// The display page under test (window 0) with its list, bar and buttons
void page(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = ptrv(win(W_OTHER));
    at_(w, 0x104) = ptrv(win(W_OTHER2));
    for (DWORD o = 0x108; o < 0x124; o += 4)
        at_(w, o) = g.chance(2) ? 0 : ptrv(win(W_BUTTON_T));
    at_(w, 0x124) = g.chance(2) ? 0 : ptrv(win(W_BUTTON_T));
    at_(w, 0x128) = ptrv(win(W_BUTTON_T));
    at_(w, 0x12c) = g.chance(3) ? 0xffffffff : g.below(21);
    const float settle = g.chance(3) ? 1.0f : (float)g.range(-5, 20) / 16.0f;
    memcpy(w + 0x130, &settle, 4);
    g_self = w;
}
void s_page_dtor(Gen &g)
{
    page(g);
    s_flags(g);
}
void s_page_dt(Gen &g)
{
    page(g);
    const float dt = g.chance(5) ? 0.0f : (float)g.range(-3, 40) / 16.0f;
    memcpy(&g_args[0], &dt, 4);
}
void s_add_button(Gen &g)
{
    page(g);
    g_args[0] = g.chance(4) ? 0 : 1 + g.below(255);
    g_args[1] = ptrv(xalloc(4));
    g_args[2] = g.chance(3) ? *at<DWORD *>(0x6c0d5c) + 8 : ptrv(TEXTS[g.below(8)]);
}
void s_list_dtor(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = g.chance(3) ? 0 : ptrv(win(W_OBJECT));
    s_flags(g);
}
void s_show_item(Gen &g)
{
    make_plain_window(0, C_WINDOW, g);
    g_args[0] = (DWORD)g.range(-5, 40);
}
// The application under test for Open/CloseInventory: the screen +0x340, the window +0x2a8, the HUD's slide +0x344
void app(Gen &g)
{
    screen(g);
    g_case.fixed[1] = ptrv(win(W_CHAR));   // (Open reads the first character without a test)
    BYTE *a = xalloc(0x400);
    at_(a, 0x340) = ptrv(win(0));   // (OpenInventory reads the screen without a test)
    at_(a, 0x2a8) = ptrv(win(W_OBJECT));
    at_(a, 0x278) = g.below(640), at_(a, 0x27c) = g.below(480);
    BYTE *slide = xalloc(0x18);
    at_(slide, 0) = g.below(4);
    at_(a, 0x344) = ptrv(slide);
    g_self = a;
}
// The HUD's slide under test
void s_slide(Gen &g)
{
    BYTE *s = xalloc(0x20);
    at_(s, 0) = g.below(5);
    const float t = g.chance(4) ? 0.0f : (float)g.range(-4, 40) / 9.0f;
    memcpy(s + 4, &t, 4);
    s[0x14] = g.chance(2);
    g_self = s;
}
void s_none(Gen &) {}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6c0540, 0x6c0570, 0x6c0574, 0x6be210, 0x6be214, 0x6c0d64,
                              0x6c0830, 0x6c0834, 0x6c2cfc};
constexpr DWORD GLOBALS1[] = {0x6c0d68};

// The x87 stack emptied after a call that may leave a value on it (the control word kept)
void clear_fpu()
{
    for (int k = 0; k < 8; k++) {
        WORD sw;
        __asm__ volatile("fnstsw %0" : "=m"(sw));
        if (!((sw >> 11) & 7))
            break;
        __asm__ volatile("fstp %%st(0)" : : : "st");
    }
}

// A function's float result (st0) into the log, the x87 stack emptied
void take_st0()
{
    WORD sw;
    __asm__ volatile("fnstsw %0" : "=m"(sw));
    if ((sw >> 11) & 7) {
        long double v;
        __asm__ volatile("fstpt %0" : "=m"(v) : : "st");
        DWORD w[3] = {0, 0, 0};
        memcpy(w, &v, 10);
        log_event(0x7f10, w[0], w[1] ^ (w[2] << 16));
    }
    clear_fpu();
}

const Test TESTS[] = {
    {"InventoryScreen ctor", 0x428c00, CALL_THIS, 1, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"InventoryScreen deleting dtor", 0x429370, CALL_THIS, 1, ~0u, 60, s_screen_dtor, STUBS, NSTUBS},
    {"InventoryScreen Init", 0x4293c0, CALL_THIS, 0, 0, 200, s_init, STUBS, NSTUBS},
    {"InventoryScreen Open", 0x4294f0, CALL_THIS, 0, 0, 200, s_open, STUBS, NSTUBS},
    {"InventoryScreen Close", 0x4295e0, CALL_THIS, 0, 0, 100, screen, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"InventoryScreen Update", 0x429620, CALL_THIS, 1, 0, 800, s_dt, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"InventoryScreen RoomAbove", 0x429880, CALL_THIS, 1, ~0u, 60, s_frame, STUBS, NSTUBS},
    {"InventoryScreen ItemRecord", 0x429980, CALL_THIS, 5, 0, 400, s_record, STUBS, NSTUBS},
    {"InventoryScreen ItemRecords", 0x429ae0, CALL_THIS, 4, 0, 300, s_records, STUBS_RECORDS, NSTUBS_RECORDS},
    {"InventoryScreen Draw", 0x429da0, CALL_THIS, 1, 0, 40, s_frame, STUBS, NSTUBS},
    {"InventoryScreen ShowVoiceLog", 0x429e20, CALL_THIS, 0, ~0u, 20, screen, STUBS, NSTUBS},
    {"InventoryScreen ShowMission", 0x429e30, CALL_THIS, 0, ~0u, 20, screen, STUBS, NSTUBS},
    {"InventoryScreen ShowInventory", 0x429e40, CALL_THIS, 0, ~0u, 20, screen, STUBS, NSTUBS},
    {"PageWindow ctor", 0x42d3d0, CALL_THIS, 2, ~0u, 40, s_page_base, STUBS, NSTUBS},
    {"PageWindow dtor", 0x42d3f0, CALL_THIS, 0, 0, 20, page, STUBS, NSTUBS},
    {"ItemListWindow deleting dtor", 0x4da240, CALL_THIS, 1, ~0u, 60, s_list_dtor, STUBS, NSTUBS},
    {"ItemListWindow ShowItem", 0x4da780, CALL_THIS, 1, 0, 200, s_show_item, STUBS, NSTUBS},
    {"InventoryDisplayPage ctor", 0x4dabd0, CALL_THIS, 1, ~0u, 200, s_ctor, STUBS, NSTUBS},
    {"InventoryDisplayPage deleting dtor", 0x4dadb0, CALL_THIS, 1, ~0u, 40, s_page_dtor, STUBS, NSTUBS},
    {"InventoryDisplayPage Update", 0x4db0c0, CALL_THIS, 1, 0, 400, s_page_dt, STUBS, NSTUBS},
    {"InventoryDisplayPage AddButton", 0x4db170, CALL_THIS, 3, 0xff, 300, s_add_button, STUBS, NSTUBS},
    {"InventoryDisplayPage Buttons", 0x4db430, CALL_THIS, 0, 0, 400, page, STUBS, NSTUBS},
    {"CZaxWorld OpenInventory", 0x5a0760, CALL_THIS, 0, 0, 200, app, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"CZaxWorld CloseInventory", 0x5a07e0, CALL_THIS, 0, 0, 200, app, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"HudSlide ctor", 0x507f20, CALL_THIS, 0, 0, 10, s_slide, STUBS, NSTUBS},
    {"HudSlide Fraction", 0x507f40, CALL_THIS, 0, 0, 200, s_slide, STUBS, NSTUBS, nullptr, &take_st0},
    {"HudSlide Toggle", 0x5084a0, CALL_THIS, 0, 0, 200, s_slide, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"HudSlide HideForInventory", 0x508570, CALL_THIS, 0, 0, 200, s_slide, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"HudSlide ShowAfterInventory", 0x5085f0, CALL_THIS, 0, 0, 200, s_slide, STUBS, NSTUBS, nullptr, &clear_fpu},
    {"MaterialsGenerator RequestClose", 0x505370, CALL_CDECL, 0, 0, 20, s_none, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_inventory)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        for (DWORD a : GLOBALS1)
            g_globals.add(a, 1);
    }
    Tally t{"ui inventory: the inventory screen, the display page, the HUD bar's slide, opening and closing"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 24000);
    t.report();
}

}  // namespace zax
