// Checks of the materials generator (ui_materials.cpp) against the original with the toolkit's harness and the
// screens' setup (checks_ui_screens.h). The grid, the window and the description under test are fake windows
// (window 0) or real objects of the extra area (the constructors); the options, the item definitions of the
// catalogue, the icons, the character, its inventory, the default items, the quest objects and the reporter are
// game objects of the case; the sequence, track and font lists 0x6c0540, 0x6c22e0, 0x6be1e0 get the vtable of a
// fake of this file and records of the case, the catalogue 0x6c0c08 and the icons 0x6c0aa8 records of the case.
// Every function of the generator calls the others by address, so they run as they are (the original side the
// original's, the reimplementation's side the reimplementation's); everything else is a recording stand-in except
// the ZString code, Path_Join/PathName, the colour constructor, qsort and the 4-byte getters 0x5b7aa0/0x482de0.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_materials(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_materials", &hooks_ui_materials};
Globals g_globals, g_no_globals;

#define MAT_STUBS \
    UIC_SCREEN_STUBS, {0x401000, CV_THIS, 2, 'e', "GridControl::GridControl"}, {0x401f70, CV_THIS, 1, K_VALUE, "GridControl::SetRowHeight"}, \
        {0x401ca0, CV_THIS, 1, K_VALUE, "GridControl::SetColor"}, {0x40cfe0, CV_THIS, 1, K_VALUE, "CJWindow::SetColorA0"}, \
        {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"}, {0x5a1cd0, CV_THIS, 0, K_FIXED0, "MaterialsGeneratorDisplay"}, \
        {0x401f90, CV_THIS, 2, K_VALUE, "GridControl::Notify"}, {0x402950, CV_THIS, 1, K_SMALL, "CJHeader::ColumnLeft"}, \
        {0x4028f0, CV_THIS, 1, K_SMALL, "CJHeader::ColumnWidth"}, {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"}, \
        {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"}, {0x4a6790, CV_THIS, 7, K_SMALL, "CFont::DrawWrapped", "vvvsvvv"}, \
        {0x4a6be0, CV_FAST, 0, K_SMALL, "Sequence_Height", "vx"}, {0x4a6b90, CV_THIS, 5, K_VALUE, "CFont::DrawInt"}, \
        {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"}, {0x4d66d0, CV_FAST, 0, K_VALUE, "Item_DisplayName", "va"}, \
        {0x4d67c0, CV_FAST, 0, K_VALUE, "Item_Description", "va"}, {0x48d8f0, CV_THIS, 1, K_FIXED4, "DefinitionList_Object"}, \
        {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"}, {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"}, \
        {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"}, {0x59ff80, CV_THIS, 0, K_FIXED3, "Game_AppGameType"}, \
        {0x551c20, CV_CDECL, 0, K_VALUE, "CQuestGameType_ClassInfo"}, {0x552940, CV_THIS, 1, K_FIXED3, "CQuestGameType::Stat"}, \
        {0x554ae0, CV_CDECL, 0, K_VALUE, "CQuestCounterStat::ClassInfo"}, {0x5525b0, CV_CDECL, 0, K_FIXED3, "Quest_SpecificData"}, \
        {0x555930, CV_THIS, 2, K_VALUE, "CQuestCounterStat::Add"}, {0x401930, CV_THIS, 2, K_VALUE, "GridControl::Select"}, \
        {0x4f5d80, CV_THIS, 0, K_FIXED1, "World::FirstCharacter"}, {0x4267e0, CV_THIS, 0, K_FIXED2, "LayerPart_GetInventory"}, \
        {0x5b5f10, CV_CDECL, 0, K_VALUE, "CZaxInventoryItemDefinition::ClassInfo"}, {0x429e50, CV_CDECL, 0, K_VALUE, "CInventoryItem::ClassInfo"}, \
        {0x426650, CV_THIS, 1, K_BOOL, "CInventory_IsDiscovered"}, {0x4257a0, CV_THIS, 1, K_FIXED6, "CInventory_IndexOfDefinitionIndex"}, \
        {0x5aef60, CV_FAST, 0, K_SMALL, "Inventory_MineralCount", "vv"}, {0x401bb0, CV_THIS, 1, K_VALUE, "GridControl::SetRows"}, \
        {0x426930, CV_FAST, 1, K_VALUE, "Part_UseItems", "vvv"}, {0x416760, CV_THIS, 2, K_FIXED7, "Object_Clone"}, \
        {0x42a2b0, CV_THIS, 0, 'e', "CInventoryItem::CInventoryItem"}, {0x54fdb0, CV_THIS, 1, K_VALUE, "CProxyFrame::SetTarget"}, \
        {0x426070, CV_THIS, 0, K_SMALL, "CInventory_ItemInFocus"}, {0x425900, CV_THIS, 1, K_FIXED6, "CInventory_IndexOfDefinition"}, \
        {0x57e890, CV_THIS, 1, K_VALUE, "CSoundList::PlayIndex"}, {0x41a720, CV_THIS, 6, K_VALUE, "CAction::Execute"}, \
        {0x40cdc0, CV_THIS, 1, K_VALUE, "CJWindow::SetTitle", "s"}, {0x4a7010, CV_THIS, 9, K_SMALL, "CFont::BoxHeight", "svvvvv"}, \
        {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"}, {0x40d700, CV_THIS, 2, K_VALUE, "CJWindow::SetScrollPos"}, \
        {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"}, {0x4a6ae0, CV_THIS, 12, K_VALUE, "CFont::DrawParagraphs", "vvvsvv"}, \
        {0x40c0b0, CV_THIS, 2, K_BOOL, "CJWindow::Slot15"}, {0x4cdf30, CV_THIS, 0, K_VALUE, "App_Window"}, \
        {0x4144b0, CV_THIS, 2, 'e', "ImageScrollBar::ImageScrollBar"}, {0x414560, CV_THIS, 1, K_VALUE, "ImageScrollBar::SetColour", "x"}, \
        {0x4145e0, CV_THIS, 2, K_VALUE, "ImageScrollBar::SetUpImages"}, {0x414600, CV_THIS, 2, K_VALUE, "ImageScrollBar::SetDownImages"}, \
        {0x414620, CV_THIS, 1, K_VALUE, "ImageScrollBar::SetTrackImage"}, {0x413ce0, CV_THIS, 1, K_VALUE, "CJScrollBar::SetRangeFrom"}, \
        {0x4020d0, CV_THIS, 1, 'e', "CJHeader::CJHeader"}, {0x402b40, CV_THIS, 1, K_VALUE, "CJHeader::SetColor"}, \
        {0x402770, CV_THIS, 5, K_VALUE, "CJHeader::AddColumn"}, {0x402b90, CV_THIS, 2, K_VALUE, "CJHeader::SetColumnFlag18"}, \
        {0x401f20, CV_THIS, 2, K_VALUE, "GridControl::SetBar"}, {0x4109e0, CV_THIS, 3, 'e', "RadioButton::RadioButton(ZString)"}, \
        {0x4109a0, CV_THIS, 3, 'e', "RadioButton::RadioButton", "vsv"}, {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"}, \
        {0x4100f0, CV_THIS, 1, K_VALUE, "Button::Set114"}, {0x4101c0, CV_THIS, 1, K_VALUE, "Button::ClearTip"}, \
        {0x5a07e0, CV_THIS, 0, K_VALUE, "CZaxWorld::CloseInventory"}, {0x40bb90, CV_THIS, 0, K_VALUE, "CJWindow::Close"}, \
        {0x402970, CV_THIS, 2, K_VALUE, "CJHeader::SetColumnTitle"}, {0x413bb0, CV_THIS, 2, K_VALUE, "CJScrollBar::SetRange"}, \
        {0x40c400, CV_THIS, 2, K_VALUE, "CJWindow::Notify"}, {0x40c170, CV_THIS, 1, K_VALUE, "CJWindow::Key"}, \
        {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"}

const Stub STUBS[] = {
    MAT_STUBS,
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);
// The rules always a CZaxCustomRules (the preload reads the options without a test)
const Stub STUBS_RULES[] = {
    MAT_STUBS,
    {0x416790, CV_THIS, 1, K_ONE, "Object_IsKindOf"},
};
constexpr int NSTUBS_RULES = sizeof STUBS_RULES / sizeof STUBS_RULES[0];
// The labels' static initializers
const Stub STUBS_LABELS[] = {
    {0x5d0863, CV_CDECL, 1, K_VALUE, "atexit"},
    {0x501030, CV_THIS, 2, K_VALUE, "LocalString_Ctor", "vv"},
    {0x4defd0, CV_THIS, 0, K_VALUE, "ZString_Dtor"},
};
constexpr int NSTUBS_LABELS = sizeof STUBS_LABELS / sizeof STUBS_LABELS[0];

// Windows of the case besides the screens' (checks_ui_screens.h)
constexpr int W_HEADER_T = 11, W_CHAR = 12, W_INV = 13, W_DETAIL = 14, W_GRID = 15;

// The lists' fake vtable: slot 1 the name of an index, slot 3 the index of a name, slot 37 a load
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7f01, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *list, const char *name, int a)
{
    log_event(0x7f02, fnv_text(2166136261u, name), (DWORD)a);
    log_event(0x7f05, ptrv(list), 0);
    return g_case.values.below(7);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7f03, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    log_event(0x7f04, rec, 0);
    return g_case.values.chance(3) ? 0 : ptrv(win(W_OBJECT));
}
DWORD THISCALL list_other(BYTE *) { return 0; }

// A game object in the extra area (the recording vtable of the class "game object")
BYTE *object(DWORD size)
{
    BYTE *o = xalloc(size);
    if (o)
        at_(o, 0) = at_(win(W_OBJECT), 0);
    return o;
}

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)args;
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x4020d0: made(answer, W_HEADER_T); return;
    case 0x4109a0:
    case 0x4109e0: made(answer, W_BUTTON_T); return;
    case 0x4144b0: made(answer, W_PLAIN_T); return;
    case 0x42a2b0:   // a new inventory item: a game object
        if (in_block(answer))
            at_(at<BYTE *>(answer), 0) = at_(win(W_OBJECT), 0);
        return;
    case 0x401000:   // the grid's base: no row selected
        if (in_block(answer))
            at_(at<BYTE *>(answer), 0x124) = 0xffffffff;
        return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

// Records of a list at `list` (+0x30 records, +0x34 count): objects from `pick`, some missing or being loaded
// (none of either with `whole`), now and then no records at all (never with `keep`: the catalogue, which the
// original searches without a test)
enum { ANY, KEEP, WHOLE };
template <typename Pick> void records(Gen &g, DWORD list, DWORD max, int mode, Pick pick, DWORD minimum = 0)
{
    const bool whole = mode == WHOLE;
    DWORD n = (whole ? 1 : 0) + g.below(max);
    if (n < minimum)
        n = minimum;
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = !whole && g.chance(4) ? 0 : pick(i);
        recs[i * 0x80 + 0x7f] = !whole && g.chance(7) ? 1 : 0;
        recs[i * 0x80 + 0x7d] = (BYTE)g.below(2);
        recs[i * 0x80 + 0x7e] = (BYTE)g.below(2);
    }
    case_global(list + 0x30, mode == ANY && g.chance(10) ? 0 : ptrv(recs));
    case_global(list + 0x34, n);
}

// An item definition of the catalogue (a game object): icon, category, default item, the "Usage" item and number,
// the costs, a required item
BYTE *make_def(Gen &g)
{
    BYTE *d = object(0x100);
    at_(d, 0x10) = g.below(6);
    at_(d, 0x18) = g.below(5);
    at_(d, 0x1c) = g.chance(3) ? ptrv(object(0x40)) : 0;
    at_(d, 0x2c) = g.chance(2) ? 0 : g.below(6);
    const float u = g.chance(3) ? 0.0f : (float)g.range(-20, 400) / 4.0f;
    memcpy(d + 0x4c, &u, 4);
    d[0x9c] = (BYTE)g.chance(3);
    for (DWORD o = 0xa4; o <= 0xb8; o += 4)
        at_(d, o) = g.chance(3) ? 0 : g.below(8);
    at_(d, 0xf8) = g.chance(3) ? g.below(6) : 0;
    return d;
}
BYTE *g_defs[8];

// The options (a game object with random fields; small font, sound and record indices)
BYTE *make_display(Gen &g)
{
    BYTE *d = object(0x100);
    for (DWORD o = 8; o < 0x100; o += 4)
        at_<int>(d, o) = g.range(-30, 500);
    for (DWORD o = 0xd8; o <= 0xec; o += 4)
        at_(d, o) = g.below(8);
    at_(d, 0x64) = g.below(12), at_(d, 0x68) = g.below(8);
    at_(d, 0x6c) = g.below(20), at_(d, 0x70) = g.below(20), at_(d, 0x74) = g.below(20);
    at_(d, 0xf0) = g.chance(2) ? 0 : ptrv(object(0x20));
    return d;
}

// A grid at w: the header, the rows {definition, affordable} and their selection, the categories, the options
void grid_fields(BYTE *w, Gen &g, BYTE *disp)
{
    at_(w, 0x100) = ptrv(win(W_HEADER_T));
    at_(w, 0x7c) = g.chance(2) ? ptrv(real_font()) : ptrv(fake_font(g.range(4, 20)));
    const DWORD n = 1 + g.below(5);
    BYTE *rows = xalloc(8 * (n + 2));
    for (DWORD i = 0; i < n; i++) {
        at_(rows, i * 8) = ptrv(g_defs[g.below(8)]);
        rows[i * 8 + 4] = (BYTE)g.below(2);
    }
    if (g.chance(6))
        at_(rows, g.below(n) * 8) = 0;
    at_(w, 0x144) = 0x5f9b90;
    at_(w, 0x148) = ptrv(rows);
    at_(w, 0x14c) = n;
    at_(w, 0x150) = n + g.below(3);
    at_(w, 0x124) = g.chance(3) ? 0xffffffff : g.below(n);
    w[0x12c] = (BYTE)g.below(2);
    for (DWORD o = 0x130; o <= 0x140; o += 4)
        at_(w, o) = g.below(6);
    at_(w, 0x154) = ptrv(disp);
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
    make_window(W_HEADER_T, C_HEADER);
    BYTE *ch = make_window(W_CHAR, C_OBJECT);
    at_(ch, 0x48) = g.next();
    make_window(W_INV, C_OBJECT);
    make_plain_window(W_DETAIL, C_WINDOW, g);
    object_args(g_object_args, "1:1,23:2,24:2,25:4,26:1,32:1,33:2");
    set_kind(C_OBJECT, 25, K_BOOL);
    set_kind(C_OBJECT, 32, K_BOOL);
    set_kind(C_OBJECT, 26, K_WINDOW_NN);
    set_kind(C_OBJECT, 45, K_FIXED5);
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_WINDOW, 44, K_TEXT);
    set_kind(C_WINDOW, 45, K_VALUE);
    set_kind(C_WINDOW, 15, K_BOOL);
    // the quest objects (the game type, its statistic and the specific data are one object: +0xc its data)
    BYTE *q = win(W_OBJECT);
    at_(q, 0xc) = ptrv(xalloc(0x30));
    at_(at_<BYTE *>(q, 0xc), 0x24) = g.below(5);
    // the catalogue, the icons, the lists
    for (BYTE *&d : g_defs)
        d = make_def(g);
    records(g, 0x6c0c08, 8, KEEP, [&](DWORD i) { return ptrv(g_defs[i]); });
    // (the icons: Fill marks the definition's icon record +0x7d/+0x7e by its index unclamped and tells the object
    // without a test: every icon there)
    records(g, 0x6c0aa8, 7, WHOLE, [&](DWORD) { return ptrv(object(0x20)); }, 6);
    records(g, 0x6be1e0, 7, ANY, [&](DWORD) { return g.next(); });
    records(g, 0x6c0540, 7, ANY, [&](DWORD) { return ptrv(object(0x20)); });
    records(g, 0x6c22e0, 7, ANY, [&](DWORD) { return g.next(); });
    case_global(0x6c0540, ptrv(g_list_vt));
    case_global(0x6c22e0, ptrv(g_list_vt));
    case_global(0x6be1e0, ptrv(g_list_vt));
    case_global(0x6c0834, g.below(6));
    case_global(0x6c2cfc, g.chance(3) ? 0 : ptrv(xalloc(0x140)));
    // the rules holder, the HUD bar's slide
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(4) ? 0 : g.next();
    at_(win(W_APP), 0x2e0) = ptrv(holder);
    BYTE *slide = xalloc(0x40);
    at_(slide, 0x128) = ptrv(xalloc(0x140));
    at_(win(W_APP), 0x340) = g.chance(4) ? 0 : ptrv(slide);
    // the answers
    BYTE *disp = make_display(g);
    g_case.fixed[0] = ptrv(disp);
    g_case.fixed[1] = ptrv(ch);
    g_case.fixed[2] = g.chance(5) ? 0 : ptrv(win(W_INV));
    g_case.fixed[3] = g.chance(4) ? 0 : ptrv(q);
    g_case.fixed[4] = ptrv(g_defs[g.below(8)]);
    g_case.fixed[5] = g.chance(2) ? 0x280 : 0x320;
    g_case.fixed[6] = g.chance(3) ? 0xffffffff : g.below(6);
    g_case.fixed[7] = ptrv(object(0x40));
    make_plain_window(W_GRID, C_GRID, g);
    grid_fields(win(W_GRID), g, disp);
}

BYTE *display() { return at<BYTE *>(g_case.fixed[0]); }

// ---- The grid ----

void s_grid_ctor(Gen &)
{
    g_self = xalloc(0x160);
    g_args[0] = ptrv(win(W_PARENT));
    g_args[1] = ptrv(win(W_HEADER_T));
}
void s_grid(Gen &g)
{
    BYTE *w = make_plain_window(0, C_GRID, g);
    grid_fields(w, g, display());
    g_self = w;
}
void s_grid_dtor(Gen &g)
{
    s_grid(g);
    if (g.chance(3))
        at_(g_self, 0x148) = 0;
    g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_grid_notify(Gen &g)
{
    s_grid(g);
    g_args[0] = ptrv(win(W_PLAIN_T)), g_args[1] = g.below(4);
}
void s_cell(Gen &g)
{
    records(g, 0x6c0c08, 8, WHOLE, [&](DWORD i) { return ptrv(g_defs[i]); });   // (the costs' definitions are read without a test)
    s_grid(g);
    g_args[0] = g.below(at_(g_self, 0x14c));
    if (!at_(at_<BYTE *>(g_self, 0x148), g_args[0] * 8))   // (the original reads the row's definition without a test)
        at_(at_<BYTE *>(g_self, 0x148), g_args[0] * 8) = ptrv(g_defs[0]);
    g_args[1] = g.chance(12) ? 5 + g.below(3) : g.chance(20) ? 0xffffffff : g.below(5);
    g_args[2] = ptrv(win(W_SCREEN));
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-20, 500), r[1] = g.range(-20, 400), r[2] = r[0] + g.range(0, 300), r[3] = r[1] + g.range(0, 60);
    g_args[3] = ptrv(r);
    if (g.chance(3))
        g_case.fixed[4] = at_(at_<BYTE *>(g_self, 0x148), g_args[0] * 8);   // the "Usage" item is the row's own
}
void s_generate(Gen &g)
{
    s_grid(g);
    BYTE *rows = at_<BYTE *>(g_self, 0x148);
    for (DWORD i = 0; i < at_(g_self, 0x14c); i++)   // (Make reads the definition without a test)
        if (!at_(rows, i * 8))
            at_(rows, i * 8) = ptrv(g_defs[i & 7]);
    g_case.fixed[2] = ptrv(win(W_INV));
}
void s_compare(Gen &g)
{
    DWORD *a = reinterpret_cast<DWORD *>(xalloc(8));
    a[0] = ptrv(g_defs[g.below(8)]), a[1] = ptrv(g_defs[g.below(8)]);
    for (DWORD o = 0xa4; o <= 0xb8; o += 4) {
        at_(at<BYTE *>(a[0]), o) = g.chance(4) ? g.next() : g.below(20);
        at_(at<BYTE *>(a[1]), o) = g.below(20);
    }
    g_args[0] = ptrv(a), g_args[1] = ptrv(a + 1);
}
void s_make(Gen &g)
{
    s_grid(g);
    g_args[0] = ptrv(g_defs[g.below(8)]);
    g_case.fixed[2] = ptrv(win(W_INV));
}

// ---- The description ----

void s_detail_ctor(Gen &)
{
    g_self = xalloc(0x110);
    g_args[0] = ptrv(win(W_PARENT));
}
void s_detail(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = ptrv(display());
    at_(w, 0x104) = g.chance(3) ? 0 : ptrv(win(W_PLAIN_T));
    at_(w, 0x108) = g.below(12);
    at_(w, 0xfc) = g.chance(2) ? 0 : ptrv(make_scroll(w, 0, 0, g.range(0, 300), g.range(0, 300), true, nullptr, nullptr));
    g_self = w;
}
void s_detail_dtor(Gen &g)
{
    s_detail(g);
    at_(g_self, 0xfc) = 0;
    g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_detail_title(Gen &g)
{
    s_detail(g);
    g_args[0] = g.chance(6) ? 0 : ptrv(TEXTS[g.below(8)]);
}
void s_detail_frame(Gen &g)
{
    s_detail(g);
    g_args[0] = ptrv(win(W_SCREEN));
}
void s_detail_mouse(Gen &g)
{
    s_detail(g);
    g_args[0] = (DWORD)g.range(-10, 600), g_args[1] = (DWORD)g.range(-10, 500);
}

// ---- The window ----

void s_window_ctor(Gen &g)
{
    // every sequence record there (the original reads the background without a test)
    records(g, 0x6c0540, 7, WHOLE, [&](DWORD) { return ptrv(object(0x20)); });
    at_(at_<BYTE *>(win(W_APP), 0x2e0), 0x38) = 1 + g.below(0x1000);   // (the rules a CZaxCustomRules: the options are read)
    g_self = xalloc(0x150);
}
// The window under test (window 0) with its members; the grid is W_GRID
void window(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = ptrv(object(0x20));
    at_(w, 0x104) = ptrv(display());
    at_(w, 0x108) = ptrv(win(W_DETAIL));
    at_(w, 0x10c) = ptrv(win(W_PLAIN_T));
    at_(w, 0x110) = g.chance(2) ? 0 : ptrv(object(0x20));
    at_(w, 0x114) = g.chance(5) ? 0 : ptrv(xalloc(0x80));
    for (DWORD o = 0x118; o <= 0x128; o += 4)
        at_(w, o) = ptrv(xalloc(0x10));
    at_(w, 0x12c) = ptrv(win(W_HEADER_T));
    at_(w, 0x130) = ptrv(win(W_GRID));
    at_(w, 0x134) = ptrv(xalloc(0x10));
    BYTE *slide = xalloc(0x40);
    BYTE *state = xalloc(0x140);
    const float f = g.chance(3) ? 1.0f : (float)g.range(-4, 20) / 9.0f;
    memcpy(state + 0x130, &f, 4);
    at_(slide, 0x128) = ptrv(state);
    at_(w, 0x138) = ptrv(slide);
    w[0x13c] = (BYTE)g.below(2);
    w[0x13d] = (BYTE)g.chance(4);
    g_self = w;
}
void s_window_dtor(Gen &g)
{
    window(g);
    g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_notify(Gen &g)
{
    window(g);
    const DWORD MEMBERS[] = {0x114, 0x118, 0x11c, 0x120, 0x124, 0x128, 0x130, 0x134};
    const DWORD k = g.below(10);
    g_args[0] = k < 8 ? at_(g_self, MEMBERS[k]) : k == 8 ? ptrv(win(W_PLAIN_T)) : 0;
    g_args[1] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.below(5);
    g_case.fixed[2] = ptrv(win(W_INV));   // (Make reads the inventory without a test)
    // (Generate makes the selected row's item: its definition there)
    BYTE *grid = win(W_GRID);
    BYTE *rows = at_<BYTE *>(grid, 0x148);
    for (DWORD i = 0; i < at_(grid, 0x14c); i++)
        if (!at_(rows, i * 8))
            at_(rows, i * 8) = ptrv(g_defs[i & 7]);
}
void s_frame(Gen &g)
{
    window(g);
    g_args[0] = ptrv(win(W_SCREEN));
}
void s_key(Gen &g)
{
    window(g);
    g_args[0] = g.chance(2) ? 0x1b : g.below(0x80);
}
void s_update(Gen &g)
{
    window(g);
    g_args[0] = g.next();
    g_case.fixed[2] = g.chance(4) ? 0 : ptrv(win(W_INV));
}
void s_by_name(Gen &g)
{
    BYTE *l = xalloc(0x140);
    at_(l, 0) = ptrv(g_list_vt);
    const DWORD n = g.below(7);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : ptrv(object(0x20));
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    at_(l, 0x30) = n && g.chance(10) ? 0 : ptrv(recs);
    at_(l, 0x34) = n;
    const char *dir = g.chance(3) ? "" : g.chance(2) ? "Cache/Sequences" : "Cache/Sequences/";
    for (int i = 0; dir[i]; i++)
        l[0x38 + i] = (BYTE)dir[i];
    g_self = l;
    g_args[0] = ptrv(TEXTS[g.below(8)]);
}
void s_none(Gen &) {}
void s_preload(Gen &g) { at_(at_<BYTE *>(win(W_APP), 0x2e0), 0x38) = 1 + g.below(0x1000); }

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6c0540, 0x6c0570, 0x6c0574, 0x6c22e0, 0x6c2310, 0x6c2314,
                              0x6be1e0, 0x6be210, 0x6be214, 0x6c0ad8, 0x6c0adc, 0x6c0c38, 0x6c0c3c, 0x6c0834, 0x6c2cfc};

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

#define T(name, addr, call, n, mask, count, setup) {name, addr, call, n, mask, count, setup, STUBS, NSTUBS, nullptr, &clear_fpu}
const Test TESTS[] = {
    T("MatGrid ctor", 0x502960, CALL_THIS, 2, ~0u, 150, s_grid_ctor),
    T("MatGrid deleting dtor", 0x502a80, CALL_THIS, 1, ~0u, 60, s_grid_dtor),
    T("MatGrid Notify", 0x502af0, CALL_THIS, 2, ~0u, 30, s_grid_notify),
    T("MatGrid DrawCell", 0x502b10, CALL_THIS, 4, 0, 900, s_cell),
    T("MatGrid Generate", 0x503430, CALL_THIS, 0, 0, 300, s_generate),
    T("MatGrid Fill", 0x5034f0, CALL_THIS, 0, 0, 400, s_grid),
    T("MatGrid Compare", 0x503910, CALL_CDECL, 2, ~0u, 300, s_compare),
    T("MatGrid Make", 0x503960, CALL_THIS, 1, 0, 300, s_make),
    T("MatDetail ctor", 0x503b80, CALL_THIS, 1, ~0u, 150, s_detail_ctor),
    T("MatDetail deleting dtor", 0x503d10, CALL_THIS, 1, ~0u, 40, s_detail_dtor),
    T("MatDetail SetTitle", 0x503d40, CALL_THIS, 1, 0, 150, s_detail_title),
    T("MatDetail Draw", 0x503dd0, CALL_THIS, 1, ~0u, 100, s_detail_frame),
    T("MatDetail Slot15", 0x503e30, CALL_THIS, 2, ~0u, 100, s_detail_mouse),
    {"MatWindow ctor", 0x504050, CALL_THIS, 0, ~0u, 150, s_window_ctor, STUBS_RULES, NSTUBS_RULES, nullptr, &clear_fpu},
    T("MatWindow deleting dtor", 0x5052e0, CALL_THIS, 1, ~0u, 60, s_window_dtor),
    T("MatWindow IsOpen", 0x505330, CALL_CDECL, 0, ~0u, 20, s_none),
    T("MatWindow Close", 0x505340, CALL_THIS, 0, ~0u, 30, window),
    {"MatWindow Preload", 0x505390, CALL_CDECL, 0, ~0u, 200, s_preload, STUBS_RULES, NSTUBS_RULES, nullptr, &clear_fpu},
    T("MatWindow Notify", 0x505630, CALL_THIS, 2, 0, 900, s_notify),
    T("MatWindow Draw", 0x505990, CALL_THIS, 1, 0, 60, s_frame),
    T("MatWindow Key", 0x505a00, CALL_THIS, 1, ~0u, 60, s_key),
    T("MatWindow Update", 0x505a30, CALL_THIS, 1, ~0u, 300, s_update),
    T("MatWindow ShowDetails", 0x505ad0, CALL_THIS, 0, 0, 300, window),
    T("List RecordByName", 0x505c20, CALL_THIS, 1, ~0u, 300, s_by_name),
};
#undef T

#define L(name, addr) {name, addr, CALL_CDECL, 0, ~0u, 4, s_none, STUBS_LABELS, NSTUBS_LABELS}
const Test LABELS[] = {
    L("StaticInit_MatItem", 0x503e70),     L("StaticExit_MatItem", 0x503e90),     L("StaticInit_MatUsage", 0x503ea0),
    L("StaticExit_MatUsage", 0x503ec0),    L("StaticInit_MatPower", 0x503ed0),    L("StaticExit_MatPower", 0x503ef0),
    L("StaticInit_MatMineral", 0x503f00),  L("StaticExit_MatMineral", 0x503f20),  L("StaticInit_MatQty", 0x503f30),
    L("StaticExit_MatQty", 0x503f50),      L("StaticInit_MatGenerate", 0x503f60), L("StaticExit_MatGenerate", 0x503f80),
    L("StaticInit_MatDone", 0x503f90),     L("StaticExit_MatDone", 0x503fb0),     L("StaticInit_MatWeapons", 0x503fc0),
    L("StaticExit_MatWeapons", 0x503fe0),  L("StaticInit_MatShields", 0x503ff0),  L("StaticExit_MatShields", 0x504010),
    L("StaticInit_MatMisc", 0x504020),     L("StaticExit_MatMisc", 0x504040),
};
#undef L

void base_labels(Gen &) {}

}  // namespace

ZAX_CHECKS(ui_materials)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui materials: the materials generator's window, grid, description, preload and labels"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 51000);
    run_tests(t, g_hooks, &g_no_globals, LABELS, sizeof LABELS / sizeof LABELS[0], &base_labels, 52000);
    t.report();
}

}  // namespace zax
