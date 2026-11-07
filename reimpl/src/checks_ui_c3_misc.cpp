// Checks of batch C3's smaller window and rendering slots (ui_c3_misc.cpp) against the original with the toolkit's
// harness (checks_ui_common.h): the windows, lists, polygons, looks and the model preview as fakes of the toolkit's
// classes or "game objects" (their slots recorded with the argument counts of the case), every exe function outside
// the module a recording stand-in (the arguments beyond the fourth logged by the effect), the stack objects the
// stand-ins construct (the warp effect, the clip frame) filled by the effect so their contents compare; ZStrings,
// Path_Join and PathName real.
#include "checks_ui_common.h"
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_c3_misc(Hook *out);

namespace {

using namespace uic;
using scr::object_args;

constinit ModuleHooks g_hooks{"ui_c3_misc", &hooks_ui_c3_misc};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x4a9740, CV_THIS, 1, K_VALUE, "CFrame::WriteFrm"},
    {0x4a9340, CV_THIS, 1, K_VALUE, "CFrame::ReadFrm"},
    {0x4ce510, CV_THIS, 0, K_VALUE, "App_UpdateMusic"},
    {0x40f840, CV_THIS, 0, K_VALUE, "CJWindow::Slot66"},
    {0x53bbc0, CV_THIS, 2, K_VALUE, "Stub_ReturnZero8"},
    {0x4a6140, CV_THIS, 0, K_SMALL, "CFont::LineHeight"},
    {0x4a7310, CV_THIS, 2, K_SMALL, "CFont::TextWidthN", "sv"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvs"},
    {0x4df440, CV_THIS, 2, K_VALUE, "ZString_AppendTextN", "sv"},
    // (the path is a ZString on the caller's stack: not logged, a fastcall stand-in's effect does not see ecx)
    {0x4164c0, CV_FAST, 2, K_VALUE, "Object_CreateFromFile", "xvvv"},
    {0x40bcc0, CV_THIS, 2, K_VALUE, "CJWindow::MouseDown"},
    {0x40c170, CV_THIS, 1, K_VALUE, "CJWindow::Key"},
    {0x403e40, CV_THIS, 1, K_VALUE, "CDialog::Key"},
    {0x402f30, CV_THIS, 1, K_VALUE, "CFrameWindow::Draw"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x403a80, CV_THIS, 0, K_SMALL, "CFrameWindow::BorderHeight"},
    {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"},
    {0x40bde0, CV_THIS, 2, K_VALUE, "CJWindow::MouseUp"},
    {0x4035f0, CV_THIS, 2, K_VALUE, "CFrameWindow::Notify"},
    {0x4173f0, CV_THIS, 0, K_VALUE, "Object_FreeFields"},
    {0x4b5fd0, CV_THIS, 0, K_VALUE, "CFreeRangePoly_Dtor"},
    {0x4fa8c0, CV_THIS, 0, K_VALUE, "CLayerPart_TellLayerRect"},
    {0x40fd80, CV_THIS, 1, K_VALUE, "Button::Key"},
    {0x46f4c0, CV_THIS, 5, K_VALUE, "KeyBinding_SetDefault"},
    {0x470b60, CV_THIS, 2, K_VALUE, "ControlConfig::BindingText"},
    {0x46f680, CV_THIS, 2, K_BOOL, "ControlConfig::Capture", "vx"},
    {0x40ccd0, CV_THIS, 0, K_WINDOW, "CJWindow::Screen"},
    {0x409870, CV_THIS, 0, K_VALUE, "CScreenWindow::ShowCursor"},
    {0x403640, CV_THIS, 1, K_VALUE, "CFrameWindow::SetRect", "r"},
    {0x40d630, CV_THIS, 2, K_VALUE, "CJWindow::MoveTo"},
    {0x432d60, CV_THIS, 0, K_VALUE, "CBlendMode_Ctor"},
    {0x432de0, CV_THIS, 5, K_VALUE, "BlendMode_RegisterBlitter"},
    {0x5156b0, CV_CDECL, 0, K_BOOL, "Cpu_HasMmx"},
    {0x571b70, CV_STD, 0, 'e', "CSimpleWarpEffect::Ctor"},
    {0x4a8760, CV_CDECL, 0, K_VALUE, "CFrame_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x572020, CV_THIS, 1, K_VALUE, "CSimpleWarpEffect::BeginFrame"},
    {0x572300, CV_THIS, 1, K_VALUE, "CSimpleWarpEffect::DrawFrame"},
    {0x572680, CV_THIS, 1, K_VALUE, "CSimpleWarpEffect::EndFrame"},
    {0x571bb0, CV_THIS, 0, K_VALUE, "CSimpleWarpEffect::Dtor"},
    {0x43dbe0, CV_FAST, 4, K_VALUE, "Rle8_DrawClipped_Copy", "vrvr"},
    {0x43e380, CV_FAST, 4, K_VALUE, "Rle8_DrawFull_Copy", "vrvr"},
    {0x49c9d0, CV_THIS, 0, K_VALUE, "DrawQueue_Ctor"},
    {0x49ccb0, CV_THIS, 0, K_VALUE, "DrawQueue_Clear"},
    {0x49ca40, CV_THIS, 0, K_VALUE, "DrawQueue_Sort"},
    {0x566e40, CV_THIS, 1, K_FIXED0, "DrawQueue_At"},
    {0x46d790, CV_STD, 0, 'e', "ClipFrame_Ctor"},
    {0x46dc00, CV_THIS, 2, K_VALUE, "ClipFrame_SetTargetRect", "vr"},
    {0x4166d0, CV_THIS, 0, K_VALUE, "CObject_Dtor"},
    {0x5c7850, CV_CDECL, 1, K_VALUE, "ServerList_Stop"},
    {0x519cf0, CV_THIS, 0, K_VALUE, "WindowServerBrowser::Refresh"},
    {0x519730, CV_THIS, 0, K_VALUE, "WindowServerBrowser::Join"},
    {0x4d3080, CV_THIS, 2, 'e', "ServerDialog::Ctor"},
    {0x404000, CV_THIS, 2, K_VALUE, "CDialog::HandOver"},
    {0x46d010, CV_THIS, 1, 'e', "NetOptionsDialog::Ctor"},
    {0x40c6e0, CV_THIS, 2, K_VALUE, "CJWindow::AddModalToParent"},
    {0x592310, CV_THIS, 1, K_VALUE, "FieldNameList::Value"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vssv"},
    {0x5c8130, CV_CDECL, 3, K_SMALL, "ServerGetIntValue", "vsv"},
    {0x53f010, CV_THIS, 4, K_VALUE, "FieldColor_CtorRGBA"},
    {0x4b5ff0, CV_THIS, 1, K_VALUE, "CFreeRangePoly_Slot29"},
    {0x402a10, CV_THIS, 1, K_VALUE, "CJHeader::SetWidth"},
    {0x40f5b0, CV_THIS, 1, K_VALUE, "CJWindow::SetWidth"},
    {0x40f5d0, CV_THIS, 1, K_VALUE, "CJWindow::SetHeight"},
    {0x40d680, CV_THIS, 1, K_VALUE, "CJWindow::SetLeft"},
    {0x40d490, CV_THIS, 0, K_SMALL, "CJWindow::ClientWidth"},
    {0x4028f0, CV_THIS, 1, K_SMALL, "CJHeader::ColumnWidth"},
    {0x564090, CV_THIS, 0, K_VALUE, "SaveLoadScreen::ShowDetails"},
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x5a2190, CV_THIS, 0, K_FIXED5, "CZaxCustomRules::LoadSaveScreenOptions"},
    {0x509910, CV_FAST, 7, K_SMALL, "MissionObjectives_Draw"},
    {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x5b9400, CV_CDECL, 0, K_VALUE, "CZaxInventoryTypeDefinition::ClassInfo"},
    {0x519b60, CV_THIS, 1, K_VALUE, "WindowServerBrowser::AddServer"},
    {0x5c9cc0, CV_CDECL, 1, K_FIXED6, "ServerList_State"},
    {0x519ea0, CV_THIS, 1, K_VALUE, "WindowServerBrowser::AddText"},
    {0x5bc910, CV_THIS, 1, K_VALUE, "CZaxSaveGameStats::SetPattern"},
    {0x5bc2a0, CV_THIS, 0, K_VALUE, "CZaxSaveGameStats::Fill"},
    {0x404180, CV_THIS, 1, K_VALUE, "CDialog::SetReplaced"},
    {0x5156e0, CV_THIS, 5, 'e', "QuestionDialog::Ctor", "vvss"},
    {0x4a14e0, CV_FAST, 0, K_BOOL, "File_Delete", "sv"},
    {0x5bcb60, CV_THIS, 1, 'p', "CZaxSaveGameStats::Description", "x"},
    {0x406890, CV_THIS, 1, K_VALUE, "CJListBox::SetList"},
    {0x403b30, CV_THIS, 3, 'e', "CMessageBox::CMessageBox", "ss"},
    {0x40f5f0, CV_FAST, 2, K_VALUE, "CJWindow_RunModal"},
    {0x4100f0, CV_THIS, 1, K_VALUE, "Button::Set114"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x5a1d40, CV_THIS, 0, K_FIXED5, "CZaxCustomRules::InventoryDisplay"},
    {0x428b10, CV_CDECL, 0, K_FIXED6, "Player_Inventory"},
    {0x426070, CV_THIS, 0, K_SMALL, "CInventory_ItemInFocus"},
    {0x429e50, CV_CDECL, 0, K_VALUE, "CInventoryItem::ClassInfo"},
    {0x4dd480, CV_THIS, 0, K_FIXED7, "ItemRef_Definition"},
    {0x5b5f10, CV_CDECL, 0, K_VALUE, "CZaxInventoryItemDefinition::ClassInfo"},
    {0x4db960, CV_FAST, 3, K_VALUE, "InventoryWindow_DrawFrame"},
    {0x4d66d0, CV_FAST, 0, K_VALUE, "Item_DisplayName", "vx"},
    {0x4d67c0, CV_FAST, 0, K_VALUE, "Item_Description", "vx"},
    {0x4a6790, CV_THIS, 7, K_VALUE, "CFont::DrawWrapped", "vvvs"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
// The inventory page's stand-ins: the same, the kind tests always true (the original uses the item's definition
// without a test, as in the game every item in focus is an item with an item definition)
Stub INVENTORY_STUBS[NSTUBS];   // (filled before the run)

char g_object_args[MAXSLOT + 1];

void effect(int cls, int slot, DWORD *args, DWORD)
{
    Gen &v = g_case.values;
    if (cls == -1) {
        switch ((DWORD)slot) {
        case 0x4a6470: log_event(0x7a64, args[4], args[5]); break;
        case 0x46f4c0: log_event(0x76f4, args[3], args[4]); break;
        case 0x432de0: log_event(0x732d, args[4], 0); break;
        case 0x571b70: memset(at<void *>(g_effect_ecx), 0, 0x30); break;
        case 0x46d790: memset(at<void *>(g_effect_ecx), 0, 0x1c); break;
        case 0x43dbe0:
        case 0x43e380: log_event(0x73db, args[2], args[3]); break;   // the mode's table and the last
        case 0x46f680: *at<BYTE *>(args[1]) = (BYTE)v.below(2); break;   // whether the row keeps waiting
        case 0x509910: log_event(0x7099, args[4] ^ args[5], args[6]); break;
        case 0x4a6790: log_event(0x7a67, args[4], args[5] ^ (args[6] << 8)); break;
        case 0x5bcb60:   // the description: a ZString made in the caller's local (freed by the caller)
            original<void *(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(args[0]), TEXTS[v.below(8)]);
            break;
        case 0x55e090: {   // the record loaded: an object (or none) in the list's record; a font for the font list
            const DWORD list = g_effect_ecx;
            const DWORD recs = *at<DWORD *>(list + 0x30);
            if (recs && args[0] < *at<DWORD *>(list + 0x34))
                *at<DWORD *>(recs + (args[0] << 7) + 0x78) = list == 0x6be1e0 ? g_case.fixed[4]
                                                            : v.chance(4) ? 0 : ptrv(win(12 + (int)v.below(4)));
            break;
        }
        default: break;
        }
        return;
    }
    if (cls == C_OBJECT && slot == 37 && args[1] == 3 && args[0]) {   // the polygon's rectangle
        int *r = at<int *>(args[0]);
        r[0] = v.range(-50, 500), r[1] = v.range(-50, 400), r[2] = r[0] + v.range(0, 300), r[3] = r[1] + v.range(0, 200);
    }
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    g_case.effect = &effect;
    g_self = win(0);
}

BYTE *object(int i)
{
    return make_window(i, C_OBJECT);
}

void s_none(Gen &) {}
void s_value(Gen &g) { g_args[0] = g.next(); }
void s_values2(Gen &g) { g_args[0] = g.next(), g_args[1] = g.next(); }
void s_image_field(Gen &g)
{
    at_(g_self, 0x10) = g.below(0x100);
    g_args[0] = ptrv(xalloc(0x100)), g_args[1] = g.next();
}
void s_button_look(Gen &g)
{
    BYTE *look = xalloc(8);
    at_(look, 4) = ptrv(win(3));
    win(3)[0x7a] = (BYTE)g.next();
    set_kind(C_WINDOW, 44, g.chance(3) ? K_ZERO : K_TEXT);
    g_self = look;
}
void s_append(Gen &g)
{
    BYTE *z = xalloc(4);
    make_text(z, TEXTS[g.below(8)]);
    g_args[0] = ptrv(z);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = g.chance(3) ? 0xffffffff : g.below(10);
}
// A list (window 4 as a game object) with 0-4 records of 0x80 bytes (+0x78 the object, +0x7f "loading") and a folder
BYTE *list(Gen &g)
{
    BYTE *l = object(4);
    object_args(g_object_args, "1:1,37:3");
    const DWORD n = g.below(5);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(2) ? 0 : ptrv(object(5 + (int)i));
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    at_(l, 0x30) = g.chance(8) ? 0 : ptrv(recs);
    at_(l, 0x34) = n;
    make_text(l + 0x38, g.chance(2) ? "" : "Cache/Folder");
    set_kind(C_OBJECT, 1, K_TEXT);
    g_case.argspec[37][0] = A_TEXT, g_case.argspec[37][1] = A_TEXT;
    return l;
}
void s_record(Gen &g)
{
    g_self = list(g);
    g_args[0] = g.chance(4) ? g.below(10) : g.below(5);
}
void s_texture_load(Gen &g)
{
    g_args[0] = ptrv(TEXTS[g.below(8)]), g_args[1] = ptrv(TEXTS[g.below(8)]), g_args[2] = g.next();
}
void s_browsable(Gen &g)
{
    object_args(g_object_args, "1:2,2:1");
    at_(g_self, 0x100) = g.chance(4) ? 0 : ptrv(object(6));
    at_(g_self, 0x104) = g.next(), at_(g_self, 0x108) = g.next();
    g_args[0] = g.chance(2) ? g.below(3) : 0, g_args[1] = g.next();
}
// The popup menu (window 0): items {.., +4 window}, listeners (game objects: slot 1 their window, slot 2)
void popup(Gen &g)
{
    BYTE *w = g_self;
    object_args(g_object_args, "1:0,2:1");
    const DWORD n = g.below(4);
    DWORD *items = reinterpret_cast<DWORD *>(xalloc(4 * n + 4));
    for (DWORD i = 0; i < n; i++) {
        BYTE *it = xalloc(8);
        at_(it, 4) = ptrv(win(1 + (int)i));
        items[i] = ptrv(it);
    }
    at_(w, 0x124) = ptrv(items), at_(w, 0x128) = n;
    const DWORD k = g.below(3);
    DWORD *lis = reinterpret_cast<DWORD *>(xalloc(4 * k + 4));
    for (DWORD i = 0; i < k; i++) {
        lis[i] = ptrv(object(8 + (int)i));
        at_(object(8 + (int)i), 4) = ptrv(win(11 + (int)i));   // (the menu's layout reads a listener's window at +4)
    }
    at_(w, 0x134) = ptrv(lis), at_(w, 0x138) = k;
    at_(w, 0x14c) = g.chance(2) ? 0xffffffff : g.below(4);
    at_(w, 0x140) = g.below(300);
    w[0x11c] = (BYTE)g.below(2);
    at_(w, 0x118) = ptrv(win(6));
    set_kind(C_OBJECT, 1, K_WINDOW);
}
void s_popup_notify(Gen &g)
{
    popup(g);
    const DWORD n = at_(g_self, 0x128);
    g_args[0] = n && g.chance(2) ? ptrv(win(1 + (int)g.below(n))) : ptrv(win((int)g.below(NWIN)));
    g_args[1] = g.chance(2) ? 0 : g.below(3);
}
void s_popup_key(Gen &g)
{
    popup(g);
    const DWORD k[] = {0xd, 0x1b, 5, 9};
    g_args[0] = k[g.below(4)];
}
void s_popup_close(Gen &)
{
    link_child(win(7), g_self);
}
void s_popup_rect(Gen &g)
{
    popup(g);
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(0, 300), r[1] = g.range(0, 300), r[2] = r[0] + g.range(0, 400), r[3] = r[1] + g.range(0, 300);
    g_args[0] = ptrv(r);
}
void s_server_key(Gen &g)
{
    g_self[0x195] = (BYTE)g.below(2);
    at_(g_self, 0x140) = ptrv(win(3));
    g_args[0] = g.chance(2) ? 0x1b : g.below(0x40);
}
void s_flag134(Gen &g)
{
    g_self[0x134] = (BYTE)g.below(2);
    g_args[0] = g.next();
}
void s_credits(Gen &g)
{
    at_<float>(g_self, 0x138) = g.chance(5) ? 0.0f : (float)g.range(-10, 400) / 7.0f;
    float dt = (float)g.range(0, 100) / 33.0f;
    memcpy(&g_args[0], &dt, 4);
    g_args[1] = g.next();
}
void s_menu_notify(Gen &g)
{
    at_(g_self, 0x13c) = ptrv(win(3));
    at_(g_self, 0x138) = ptrv(win(4));
    g_args[0] = ptrv(win(3 + (int)g.below(3)));
    g_args[1] = g.chance(2) ? 0 : g.below(3);
    case_global(0x6c2080, ptrv(xalloc(0x400)));
}
// The renderable polygon (a game object with the slots it calls)
void s_polygon(Gen &g)
{
    BYTE *p = object(0);
    object_args(g_object_args, "6:1,35:1,37:3,58:1,59:1");
    at_(p, 0x78) = g.chance(5) ? g.next() : g.below(9);
    at_(p, 0x8c) = g.chance(3) ? 0 : ptrv(object(9));
    g_self = p;
    g_args[0] = g.chance(3) ? g.below(9) : g.next();
}
void s_polygon_dtor(Gen &g)
{
    object_args(g_object_args, "6:1");
    g_args[0] = g.below(4);
    if (g_args[0] & 2) {
        const DWORD n = g.below(4);
        BYTE *b = xalloc(4 + n * 0x9c);
        at_(b, 0) = n;
        for (DWORD i = 0; i < n; i++)
            at_(b, 4 + i * 0x9c + 0x8c) = g.chance(3) ? 0 : ptrv(object(9 + (int)i));
        g_self = b + 4;
    } else {
        g_self = xalloc(0x9c);
        at_(g_self, 0x8c) = g.chance(3) ? 0 : ptrv(object(9));
    }
}
void s_polygon_anim(Gen &g)
{
    s_polygon(g);
    at_(g_self, 0x78) = g.chance(6) ? 2 : 6;
    at_(g_self, 0x94) = 0;
    at_<float>(g_self, 0x94) = g.chance(5) ? 0.0f : (float)g.range(0, 3000) / 13.0f;
    float dt = (float)g.range(0, 100) / 30.0f;
    memcpy(&g_args[0], &dt, 4);
    // the animated texture list 0x6c1ee8: a game object with records whose sequences exist
    object_args(g_object_args, "1:1,25:0,26:0,37:3");
    const DWORD n = 1 + g.below(4);
    BYTE *recs = xalloc(n * 0x80);
    for (DWORD i = 0; i < n; i++)
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : ptrv(object(10 + (int)i));
    at_(g_self, 0x7c) = g.chance(4) ? g.below(10) : g.below(n);
    case_global(0x6c1ee8, at_(object(15), 0));
    case_global(0x6c1f18, ptrv(recs));
    case_global(0x6c1f1c, n);
    set_kind(C_OBJECT, 1, K_TEXT);
    set_kind(C_OBJECT, 26, K_FLOAT);
    set_kind(C_OBJECT, 25, K_SMALL);
    set_kind(C_OBJECT, 37, K_FIXED0);
    g_case.fixed[0] = ptrv(object(14));
    g_case.argspec[37][0] = A_TEXT, g_case.argspec[37][1] = A_TEXT;
}
void s_polygon_draw(Gen &g)
{
    s_polygon(g);
    g_case.argspec[37][0] = A_IGNORE;   // (the rectangle on the stack the slot fills)
    at_(g_self, 0x1c) = g.chance(3) ? 0 : 0x400000 | g.below(16);
    at_(g_self, 0x74) = g.below(40);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    case_global(0x7130c8, g.below(4));
}
void s_control_row(Gen &g)
{
    BYTE *w = object(0);
    object_args(g_object_args, "38:1,77:1,78:1");
    at_(w, 0x138) = ptrv(object(3));
    at_(w, 0x13c) = g.chance(3) ? 0 : g.next();
    at_(w, 0x140) = g.below(40);
    w[0x144] = (BYTE)(g.chance(4) ? 0 : 1);
    g_self = w;
    const DWORD k[] = {0x11, 8, 5, 0xd};
    g_args[0] = k[g.below(4)];
    g_case.argspec[77][0] = A_TEXT;
}
void s_text_look(Gen &g)
{
    BYTE *look = xalloc(0xc);
    BYTE *w = object(3);
    object_args(g_object_args, "44:0,79:0");
    w[0x100] = (BYTE)g.below(2), w[0x101] = (BYTE)g.below(2), w[0x103] = (BYTE)g.below(2);
    for (int k = 0; k < 4; k++)
        at_(w, 4 + 4 * k) = (DWORD)g.range(-20, 400);
    BYTE *style = xalloc(0x14);
    at_(style, 4) = ptrv(fake_font(g.below(30)));
    at_(style, 8) = ptrv(fake_font(g.below(30)));
    BYTE *hf = xalloc(0x40);
    at_(hf, 0x38) = g.next();
    at_(style, 0xc) = ptrv(hf);
    at_(style, 0x10) = g.next();
    at_(look, 4) = ptrv(w), at_(look, 8) = ptrv(style);
    set_kind(C_OBJECT, 44, K_TEXT);
    g_self = look;
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    case_global(0x6c1aac, g.below(4));
}
void s_warp8(Gen &g)
{
    BYTE *src = make_window(13, C_CFRAME);
    int *sr = reinterpret_cast<int *>(xalloc(16));
    sr[0] = g.chance(2) ? 0 : g.range(1, 20), sr[1] = g.range(0, 20), sr[2] = g.range(20, 200), sr[3] = g.range(20, 200);
    g_case.fixed[1] = g.chance(2) ? (DWORD)sr[2] : g.below(300);
    set_kind(C_CFRAME, 45, K_FIXED1);
    BYTE *mode = xalloc(0x40);
    at_(mode, 0x24) = g.next();
    g_args[0] = ptrv(src), g_args[1] = ptrv(sr), g_args[2] = ptrv(make_window(14, C_CFRAME));
    g_args[3] = ptrv(xalloc(16)), g_args[4] = ptrv(mode);
}
void s_model_preview(Gen &g)
{
    BYTE *p = xalloc(0x20);
    object_args(g_object_args, "1:1,23:5");
    at_(p, 0xc) = ptrv(object(3));
    g_self = p;
    g_case.fixed[0] = ptrv(object(4));
    int *pt = reinterpret_cast<int *>(xalloc(8));
    pt[0] = g.range(-100, 600), pt[1] = g.range(-100, 400);
    g_args[0] = ptrv(make_window(14, C_CFRAME)), g_args[1] = ptrv(pt);
    case_global(0x6cff38, g.below(4));
    case_global(0x6cff20, g.below(4));
    case_global(0x6cff30, g.below(4));
    g_case.argspec[23][2] = A_RECT, g_case.argspec[23][3] = A_RECT;
    g_case.argspec[1][0] = A_IGNORE;   // (the clip frame on the stack)
}
void s_server_notify(Gen &g)
{
    BYTE *w = g_self;
    object_args(g_object_args, "76:0,77:0");
    at_(w, 0x138) = ptrv(object(9)), at_(w, 0x13c) = ptrv(win(3)), at_(w, 0x140) = ptrv(win(4));
    at_(w, 0x14c) = ptrv(object(10)), at_(w, 0x164) = ptrv(win(5));
    at_(w, 0x168) = ptrv(win(6)), at_(w, 0x16c) = ptrv(win(7)), at_(w, 0x170) = ptrv(win(8));
    at_(w, 0x190) = g.next();
    w[0x195] = (BYTE)g.below(2);
    const DWORD from[] = {0x138, 0x13c, 0x140, 0x14c, 0x164, 0x168, 0x16c, 0x170};
    g_args[0] = g.chance(8) ? ptrv(win(11)) : at_(w, from[g.below(8)]);
    g_args[1] = g.chance(3) ? 1 : g.chance(4) ? g.below(4) : 0;
    set_kind(C_OBJECT, 77, K_VALUE);
}

void s_save_load_rect(Gen &g)
{
    BYTE *w = g_self;
    w[0x1a0] = (BYTE)(g.chance(5) ? 0 : 1);
    at_(w, 0x134) = ptrv(make_plain_window(1, C_HEADER, g));
    at_(w, 0x138) = g.chance(3) ? 0 : ptrv(win(2));
    at_(w, 0x13c) = ptrv(win(3));
    at_(w, 0x144) = ptrv(win(4));
    at_(w, 0x140) = g.chance(3) ? 0 : ptrv(win(5));
    at_(w, 0x148) = ptrv(win(6));
    at_(w, 0x14c) = ptrv(win(7));
    at_(win(7), 0x110) = g.below(30);
    const DWORD n = g.below(4);
    for (DWORD k = 0; k < 4; k++)
        if (k < n) {   // (pointers to the pages' window pointers)
            BYTE *ref = xalloc(4);
            at_(ref, 0) = ptrv(win(8 + (int)k));
            at_(w, 0x160 + 4 * k) = ptrv(ref);
        } else {
            at_(w, 0x160 + 4 * k) = 0;
        }
    at_(w, 0x174) = g.chance(3) ? 0 : ptrv(win(11));
    at_(w, 0x178) = ptrv(win(12));
    at_(w, 0x17c) = ptrv(win(13));
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = at_<int>(w, 4) + g.range(-20, 20), r[1] = at_<int>(w, 8) + (g.chance(2) ? 0 : g.range(-20, 20));
    r[2] = at_<int>(w, 0xc) + g.range(-30, 30), r[3] = at_<int>(w, 0x10) + (g.chance(2) ? r[1] - at_<int>(w, 8) : g.range(-30, 30));
    g_args[0] = ptrv(r);
}
void s_details(Gen &g)
{
    BYTE *app = xalloc(0x300);
    BYTE *world = xalloc(0x40);
    at_(app, 0x2e0) = ptrv(world);
    at_(world, 0x38) = g.chance(3) ? 0 : g.next() | 1;
    case_global(0x713f14, ptrv(app));
    BYTE *opts = xalloc(0x10);
    at_(opts, 8) = g.next();
    g_case.fixed[5] = ptrv(opts);
    at_(g_self, 0x100) = g.next();
    g_self[0x78] = (BYTE)g.below(10);
    if (g.chance(2))
        make_scroll(g_self, g.range(0, 50), g.range(0, 50), g.range(0, 500), g.range(0, 500), false, nullptr, nullptr);
    g_args[0] = g.next();
}
// A definition list at b: 1-5 records, objects (game objects with an order at +0x1c) or none, now and then loading
void def_list(Gen &g, DWORD b, bool all = false)
{
    const DWORD n = 1 + g.below(5);
    BYTE *recs = xalloc(n * 0x80);
    for (DWORD i = 0; i < n; i++) {
        BYTE *o = win(12 + (int)(i % 4));
        at_(o, 0x1c) = g.below(20);
        at_<float>(o, 0x10) = (float)g.range(-100, 100) / 8.0f;
        at_(recs, i * 0x80 + 0x78) = g.chance(4) && !all ? 0 : ptrv(o);
        recs[i * 0x80 + 0x7f] = g.chance(8) && !all ? 1 : 0;
    }
    case_global(b + 0x30, ptrv(recs));
    case_global(b + 0x34, n);
    DWORD *ia = reinterpret_cast<DWORD *>(xalloc(4)), *ib = reinterpret_cast<DWORD *>(xalloc(4));
    *ia = g.below(n), *ib = g.below(n);
    g_args[0] = ptrv(ia), g_args[1] = ptrv(ib);
}
void s_category_order(Gen &g) { def_list(g, 0x6c0800); }
void s_difficulty_order(Gen &g)
{
    def_list(g, 0x6c2a00, true);   // (a missing or loading record: the original reads through null)
    object_args(g_object_args, "1:1,37:3");
    case_global(0x6c2a00, at_(object(10), 0));
    g_case.fixed[0] = ptrv(win(12));
    set_kind(C_OBJECT, 37, K_FIXED0);
    set_kind(C_OBJECT, 1, K_TEXT);
}

void s_server_list(Gen &g)
{
    BYTE *w = g_self;
    object_args(g_object_args, "17:0,22:0");
    at_(w, 0x13c) = ptrv(win(3)), at_(w, 0x140) = ptrv(win(4)), at_(w, 0x148) = ptrv(win(5));
    at_(w, 0x17c) = at_(object(10), 0);
    make_text(w + 0x198, TEXTS[g.below(8)]);
    at_(w, 0x19c) = g.chance(3) ? 100 : g.below(101);
    w[0x195] = (BYTE)g.below(2);
    set_kind(C_OBJECT, 22, K_SMALL);
    g_case.fixed[6] = g.below(5);
    g_args[0] = g.next(), g_args[1] = g.chance(4) ? g.below(5) : 1 + g.below(2), g_args[2] = ptrv(w), g_args[3] = g.next();
    g_args[4] = g.chance(3) ? at_(w, 0x19c) : g.below(101);
}

// The save/load screen (window 0 as a game object: slot 5): its parts fakes, the saves game objects with a file
// name at +0x68, the application and the world game objects
void s_save_load_notify(Gen &g)
{
    BYTE *w = object(0);
    // (the list and the title bar are list boxes: their slot 80 takes 2 arguments, the application's and the
    // world's none; the saves are windows: their slot 1 takes the delete flag, the world's none)
    object_args(g_object_args, "1:0,5:0,10:0,17:0,18:1,44:0,57:1,77:0,80:0,109:1,110:1");
    const int part[] = {0x134, 0x138, 0x13c, 0x140, 0x144, 0x148, 0x174, 0x178, 0x17c, 0x180, 0x184};
    for (int k = 0; k < 11; k++) {
        BYTE *o = part[k] == 0x138 || part[k] == 0x144 ? make_window(1 + k, C_LISTBOX) : object(1 + k);
        at_(w, (DWORD)part[k]) = g.chance(8) && (part[k] == 0x138 || part[k] == 0x140 || part[k] == 0x174) ? 0 : ptrv(o);
    }
    set_kind(C_LISTBOX, 77, K_FIXED4);
    const DWORD n = g.below(4);
    DWORD *saves = reinterpret_cast<DWORD *>(xalloc(4 * n + 8));
    for (DWORD k = 0; k < n; k++) {
        BYTE *sv = xalloc(0x70);
        make_text(sv + 0x68, TEXTS[1 + g.below(7)]);
        at_(sv, 0) = at_(make_window(12, C_WINDOW), 0);
        saves[k] = ptrv(sv);
    }
    at_(w, 0x18c) = ptrv(saves), at_(w, 0x190) = n;
    BYTE *stats = xalloc(0x70);
    make_text(stats + 0x68, "Save 1");
    at_(w, 0x19c) = ptrv(stats);
    g_self = w;
    BYTE *zdesc = xalloc(4);
    make_text(zdesc, "Description");
    g_case.fixed[7] = ptrv(zdesc);
    set_kind(C_OBJECT, 44, K_TEXT);
    set_kind(C_OBJECT, 77, K_FIXED4);
    g_case.fixed[4] = n ? g.below(n) : 0;
    set_kind(C_OBJECT, 1, K_FIXED3);
    g_case.fixed[3] = ptrv(object(13));
    case_global(0x713f14, ptrv(object(14)));
    BYTE *world = object(15);
    at_(world, 0x2a8) = g.next();
    case_global(0x6c2080, ptrv(world));
    const int from[] = {0x134, 0x138, 0x140, 0x144, 0x174, 0x178, 0x17c, 0x180};
    g_args[0] = g.chance(8) ? ptrv(win(13)) : at_(w, (DWORD)from[g.below(8)]);
    g_args[1] = g.chance(3) ? 1 : g.chance(5) ? g.below(4) : 0;
    if (!n && (g_args[0] == at_(w, 0x174) || g_args[0] == at_(w, 0x144) || g_args[0] == at_(w, 0x180)) && !g_args[1])
        g_args[0] = at_(w, 0x17c);   // (loading or deleting needs a save: the original indexes the saves)
    if (g_args[1] == 1 && g_args[0] == at_(w, 0x134))
        at_(w, 0x138) = ptrv(win(2));   // (the header's resize reads the title bar without a test)
}

// The inventory page (window 0): the rules' options, the player's inventory (a game object: slot 26 the item),
// the item (slot 28 its drawing), the definition (slot 33 the picture), the picture and font lists with records
void s_inventory_page(Gen &g)
{
    BYTE *w = g_self;
    object_args(g_object_args, "1:1,23:1,26:1,28:4,33:0,37:3,79:0");
    g_case.argspec[37][0] = A_TEXT, g_case.argspec[37][1] = A_TEXT;   // (the font loader's folder and file)
    at_(w, 0x134) = g.next();
    at_(w, 0x108) = g.chance(3) ? 0 : ptrv(object(3));
    at_<float>(w, 0x130) = g.chance(2) ? 1.0f : (float)g.below(100) / 100.0f;
    set_kind(C_OBJECT, 79, K_VALUE);
    BYTE *app = xalloc(0x300);
    BYTE *world = xalloc(0x40);
    at_(app, 0x2e0) = ptrv(world);
    at_(world, 0x38) = g.chance(3) ? 0 : g.next() | 1;
    case_global(0x713f14, ptrv(app));
    BYTE *disp = xalloc(0x90);
    for (DWORD o = 0x3c; o < 0x80; o += 4)
        at_(disp, o) = g.below(40);
    g_case.fixed[5] = ptrv(disp);
    BYTE *inv = object(4);
    at_(inv, 0x20) = g.chance(6) ? 0 : 1 + g.below(5);
    g_case.fixed[6] = ptrv(inv);
    BYTE *item = object(5);
    at_(item, 8) = g.next();
    g_case.fixed[0] = ptrv(item);
    set_kind(C_OBJECT, 26, K_FIXED0);
    g_case.fixed[7] = ptrv(object(6));
    set_kind(C_OBJECT, 33, K_SMALL);
    set_kind(C_OBJECT, 23, g.chance(4) ? K_ZERO : K_VALUE);
    set_kind(C_OBJECT, 1, K_TEXT);
    set_kind(C_OBJECT, 37, K_FIXED4);
    // the picture list 0x6c0948 and the font list 0x6be1e0: records with objects (the picture records game objects,
    // the font records fonts), the font list a game object for its inline loader
    const DWORD n = 21;
    BYTE *pics = xalloc(n * 0x80);
    for (DWORD i = 0; i < n; i++)
        at_(pics, i * 0x80 + 0x78) = ptrv(object(7 + (int)(i % 3)));
    case_global(0x6c0978, ptrv(pics));
    case_global(0x6c097c, g.chance(5) ? g.below(3) : n);
    const DWORD nf = 40;
    BYTE *fonts = xalloc(nf * 0x80);
    for (DWORD i = 0; i < nf; i++)
        at_(fonts, i * 0x80 + 0x78) = g.chance(4) ? 0 : ptrv(fake_font(g.below(30)));
    case_global(0x6be1e0, at_(object(10), 0));
    case_global(0x6be210, ptrv(fonts));
    case_global(0x6be214, nf);
    BYTE *f0 = fake_font(g.below(30));
    at_(f0, 0) = at_(object(11), 0);
    g_case.fixed[4] = ptrv(f0);
    g_args[0] = ptrv(make_window(14, C_CFRAME));
}

const Test TESTS[] = {
    {"Shared Field28", 0x4a5970, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"Shared Field4", 0x592370, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"ImageField Write", 0x4b5c20, CALL_THIS, 2, ~0u, 30, s_image_field, STUBS, NSTUBS},
    {"ImageField Read", 0x4b5c40, CALL_THIS, 2, ~0u, 30, s_image_field, STUBS, NSTUBS},
    {"App Slot5", 0x4ce3d0, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"Credits Slot66", 0x5c0780, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"Polygon Slot25", 0x55dd80, CALL_THIS, 2, ~0u, 20, s_values2, STUBS, NSTUBS},
    {"ButtonLook FitTitle", 0x4e2590, CALL_THIS, 0, ~0u, 60, s_button_look, STUBS, NSTUBS},
    {"Text AppendCallback", 0x506f60, CALL_FAST, 3, ~0u, 40, s_append, STUBS, NSTUBS},
    {"ResourceList Record", 0x598750, CALL_THIS, 1, ~0u, 300, s_record, STUBS, NSTUBS},
    {"TextureList Load", 0x4b47a0, CALL_THIS, 3, ~0u, 40, s_texture_load, STUBS, NSTUBS},
    {"BrowsableImage Update", 0x599440, CALL_THIS, 1, 0, 40, s_browsable, STUBS, NSTUBS},
    {"BrowsableImage Draw", 0x599460, CALL_THIS, 1, 0, 40, s_browsable, STUBS, NSTUBS},
    {"BrowsableImage MouseDown", 0x599480, CALL_THIS, 2, ~0u, 60, s_browsable, STUBS, NSTUBS},
    {"PopupMenu Notify", 0x4de870, CALL_THIS, 2, 0, 200, s_popup_notify, STUBS, NSTUBS},
    {"PopupMenu Key", 0x4de900, CALL_THIS, 1, ~0u, 100, s_popup_key, STUBS, NSTUBS},
    {"PopupMenu Close", 0x4dea40, CALL_THIS, 0, 0xff, 30, s_popup_close, STUBS, NSTUBS},
    {"PopupMenu SetRect", 0x4de950, CALL_THIS, 1, 0, 150, s_popup_rect, STUBS, NSTUBS},
    {"ServerBrowser Key", 0x519470, CALL_THIS, 1, ~0u, 60, s_server_key, STUBS, NSTUBS},
    {"SaveLoad Draw", 0x564080, CALL_THIS, 1, ~0u, 20, s_value, STUBS, NSTUBS},
    {"SelectMap Draw", 0x5bdd80, CALL_THIS, 1, ~0u, 40, s_flag134, STUBS, NSTUBS},
    {"SelectMap BorderHeight", 0x5bddb0, CALL_THIS, 0, ~0u, 40, s_flag134, STUBS, NSTUBS},
    {"Credits Update", 0x5bf000, CALL_THIS, 1, ~0u, 60, s_credits, STUBS, NSTUBS},
    {"Credits MouseUp", 0x5bf020, CALL_THIS, 2, ~0u, 60, s_credits, STUBS, NSTUBS},
    {"Credits Key", 0x5bf050, CALL_THIS, 1, ~0u, 60, s_credits, STUBS, NSTUBS},
    {"CreditsMenu Notify", 0x5c0970, CALL_THIS, 2, ~0u, 60, s_menu_notify, STUBS, NSTUBS},
    {"DiedMenu Notify", 0x5c0a70, CALL_THIS, 2, ~0u, 60, s_menu_notify, STUBS, NSTUBS},
    {"Polygon VectorDeletingDtor", 0x55d370, CALL_THIS, 1, ~0u, 100, s_polygon_dtor, STUBS, NSTUBS},
    {"Polygon UpdateFlags", 0x55d9a0, CALL_THIS, 1, 0, 60, s_polygon, STUBS, NSTUBS},
    {"Polygon SetRenderType", 0x55d9f0, CALL_THIS, 1, ~0u, 60, s_polygon, STUBS, NSTUBS},
    {"Polygon Slot33", 0x55da60, CALL_THIS, 0, ~0u, 60, s_polygon, STUBS, NSTUBS},
    {"ControlRow Key", 0x471160, CALL_THIS, 1, ~0u, 100, s_control_row, STUBS, NSTUBS},
    {"ControlRow Update", 0x471220, CALL_THIS, 0, 0, 100, s_control_row, STUBS, NSTUBS},
    {"TextLook Draw", 0x4e2be0, CALL_THIS, 1, 0, 200, s_text_look, STUBS, NSTUBS},
    {"SimpleWarp Blit8", 0x4e2e50, CALL_FAST, 5, 0, 100, s_warp8, STUBS, NSTUBS},
    {"ModelPreview Draw", 0x517c60, CALL_THIS, 2, 0, 150, s_model_preview, STUBS, NSTUBS},
    {"ServerBrowser Notify", 0x5194e0, CALL_THIS, 2, 0, 300, s_server_notify, STUBS, NSTUBS},
    {"Polygon Animate", 0x55dab0, CALL_THIS, 1, 0, 300, s_polygon_anim, STUBS, NSTUBS},
    {"Polygon Draw", 0x55dcd0, CALL_THIS, 1, ~0u, 100, s_polygon_draw, STUBS, NSTUBS},
    {"SaveLoad SetRect", 0x563d40, CALL_THIS, 1, 0, 200, s_save_load_rect, STUBS, NSTUBS},
    {"SaveLoadDetails Draw", 0x564b90, CALL_THIS, 1, ~0u, 100, s_details, STUBS, NSTUBS},
    {"CategoryOrder Compare", 0x5b29a0, CALL_CDECL, 2, ~0u, 200, s_category_order, STUBS, NSTUBS},
    {"DifficultyOrder Compare", 0x5bf690, CALL_CDECL, 2, ~0u, 200, s_difficulty_order, STUBS, NSTUBS},
    {"ServerList Callback", 0x519870, CALL_CDECL, 5, 0, 300, s_server_list, STUBS, NSTUBS},
    {"SaveLoad Notify", 0x563890, CALL_THIS, 2, 0, 400, s_save_load_notify, STUBS, NSTUBS},
    {"InventoryPage Draw", 0x4dbd50, CALL_THIS, 1, ~0u, 300, s_inventory_page, INVENTORY_STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_c3_misc)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        const DWORD g4[] = {0x6c2080, 0x6c1a78, 0x6c1ab0, 0x6cff20, 0x6cff30, 0x6c1ee8, 0x6c1f18, 0x6c1f1c, 0x713f14,
                            0x6c0830, 0x6c0834, 0x6c2a00, 0x6c2a30, 0x6c2a34, 0x6c0978, 0x6c097c, 0x6be1e0,
                            0x6be210, 0x6be214};
        for (DWORD a : g4)
            g_globals.add(a, 4);
        const DWORD g1[] = {0x6c1aac, 0x6cff38, 0x7130c8};
        for (DWORD a : g1)
            g_globals.add(a, 1);
    }
    Tally t{"ui c3 misc: getters, the image field, the button and text looks, the lists' loaders, the browsable image, the popup menu, the menus, the "
            "credits, the controls rows, the model preview, the server browser, the renderable polygon"};
    for (int k = 0; k < NSTUBS; k++) {
        INVENTORY_STUBS[k] = STUBS[k];
        if (STUBS[k].address == 0x416790)
            INVENTORY_STUBS[k].kind = K_ONE;
    }
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 65500);
    t.report();
}

}  // namespace zax
