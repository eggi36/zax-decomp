// Checks of the small dialogs (ui_dialogs.cpp) against the original with the toolkit's harness and the screens'
// setup (checks_ui_screens.h). The constructors run on real objects in the extra area with the toolkit's
// constructors as stand-ins; Notify, the destructors and the helpers run on a fake dialog (window 0) with fake
// members. Never touched: the INI file (GetDefault and WriteString stand-ins), the clipboard (Clipboard_SetText
// and GetText stand-ins), the player options (+4/+8 of 0x6bd2f8 are case values, its name is only read; the
// destructor's save and send are stand-ins), the game window (Platform_CloseWindow a stand-in).
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_dialogs(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_dialogs", &hooks_ui_dialogs};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "ass"},
    {0x40f5f0, CV_FAST, 2, K_VALUE, "CJWindow::RunModal"},
    {0x411ad0, CV_THIS, 1, K_VALUE, "EditField::SetClipboard"},
    {0x46b540, CV_FAST, 0, K_BOOL, "CdKey_Check", "ax"},
    {0x4a47c0, CV_FAST, 2, K_VALUE, "Ini_WriteString", "vvss"},
    {0x4d2470, CV_CDECL, 0, K_VALUE, "Platform_CloseWindow"},
    {0x46ce50, CV_THIS, 0, K_VALUE, "PlayerOptions::Load"},
    {0x46ced0, CV_THIS, 0, K_VALUE, "PlayerOptions::Save"},
    {0x5abe30, CV_CDECL, 0, K_VALUE, "ClientOptionsReader_Get"},
    {0x5abfe0, CV_THIS, 0, K_VALUE, "ClientOptionsReader::Write"},
    {0x599330, CV_THIS, 1, 'e', "CWindowedBrowsableImage::Ctor"},
    {0x5179b0, CV_THIS, 2, 'e', "WindowModelPreview_Make"},
    {0x5993c0, CV_THIS, 1, K_VALUE, "CWindowedBrowsableImage::SetImage"},
    {0x4fc7c0, CV_FAST, 0, K_SMALL, "CLayerPart_ChildCount", "vx"},
    {0x4fc7d0, CV_THIS, 1, K_VALUE, "CLayerPart_ChildAt"},
    {0x418fc0, CV_THIS, 1, K_BOOL, "FieldDesc_IsDynamicIn"},
    {0x418790, CV_THIS, 1, K_FIXED0, "FieldDesc_DynamicPointer"},
    {0x46d590, CV_FAST, 1, K_VALUE, "Clipboard_SetText", "ssv"},
    {0x46d620, CV_FAST, 0, K_VALUE, "Clipboard_GetText", "sv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// The server dialog, the server browser, the model preview and the question dialog
const Stub STUBS2[] = {
    UIC_SCREEN_STUBS,
    {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "asa"},
    {0x4e0a30, CV_CDECL, 3, K_VALUE, "ZString_AppendFormat", "asv"},
    {0x4038a0, CV_THIS, 0, K_VALUE, "CFrameWindow::AddCloseButton"},
    {0x4d2c50, CV_THIS, 0, K_VALUE, "ServerOptions::Init"},
    {0x4d2ca0, CV_THIS, 0, K_VALUE, "ServerOptions::SaveName"},
    {0x592200, CV_THIS, 0, K_VALUE, "FieldNameList::Ctor"},
    {0x592220, CV_THIS, 0, K_VALUE, "FieldNameList::Dtor"},
    {0x5922b0, CV_THIS, 2, K_VALUE, "FieldNameList::AddValue", "sv"},
    {0x592310, CV_THIS, 1, K_SMALL, "FieldNameList::Value"},
    {0x592320, CV_THIS, 1, K_SMALL, "FieldNameList_FindValue"},
    {0x51a270, CV_CDECL, 0, K_VALUE, "CMultiPlayerGameType::ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x408cd0, CV_THIS, 3, 'e', "ComboBox2::ComboBox2"},
    {0x408bc0, CV_THIS, 2, K_VALUE, "ComboBox::SelectId"},
    {0x4020d0, CV_THIS, 1, 'e', "CJHeader::CJHeader"},
    {0x402770, CV_THIS, 5, K_VALUE, "CJHeader::AddColumn"},
    {0x402bb0, CV_THIS, 0, K_VALUE, "CJHeader::Recalc"},
    {0x4041f0, CV_THIS, 4, 'e', "CJListBox::CJListBox"},
    {0x406aa0, CV_THIS, 1, K_VALUE, "CJListBox::SetHeader"},
    {0x406890, CV_THIS, 1, K_VALUE, "CJListBox::SetList"},
    {0x407270, CV_THIS, 2, K_VALUE, "CJListBox::SelectId2"},
    {0x4a3970, CV_THIS, 0, 'e', "FileNameList_Ctor"},
    {0x4a3ae0, CV_THIS, 1, K_SMALL, "FileNameList_Search"},
    {0x4a3b60, CV_THIS, 2, 'p', "FileNameList_GetName", "xv"},
    {0x4e9da0, CV_THIS, 0, 'e', "CLayer::Ctor"},
    {0x416a00, CV_THIS, 2, K_VALUE, "Object_ReadFile", "sv"},
    {0x411b90, CV_THIS, 3, 'e', "NumberField::NumberField"},
    {0x412280, CV_THIS, 3, 'e', "CheckBox::CheckBox", "vsv"},
    {0x4d3c80, CV_THIS, 0, K_VALUE, "ServerDialog_FillGameTypes"},
    {0x4d3e20, CV_THIS, 0, K_VALUE, "ServerDialog_FillMaps"},
    {0x40c740, CV_THIS, 2, K_VALUE, "CJWindow::MessageBox", "ss"},
    {0x5a2940, CV_FAST, 3, K_BOOL, "Game_StartFromMenu", "vsvvv"},
    {0x46ce50, CV_THIS, 0, K_VALUE, "PlayerOptions::Load"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x40c600, CV_THIS, 1, K_VALUE, "CJWindow::SetTransparent"},
    {0x4a4670, CV_FAST, 2, K_SMALL, "Ini_GetInt", "vvsv"},
    {0x56c400, CV_THIS, 3, 'e', "SequenceWindow_Ctor", "vsv"},
    {0x5c6060, CV_CDECL, 7, K_VALUE, "ServerListNew", "sssvvv"},
    {0x5c6180, CV_CDECL, 1, K_VALUE, "ServerListFree"},
    {0x5c74c0, CV_CDECL, 1, K_VALUE, "ServerListThink"},
    {0x5c7870, CV_CDECL, 1, K_VALUE, "ServerListLength"},
    {0x5c64b0, CV_CDECL, 5, K_VALUE, "ServerListLANUpdate"},
    {0x5c61c0, CV_CDECL, 2, K_VALUE, "ServerListUpdate"},
    {0x5c8130, CV_CDECL, 3, K_SMALL, "ServerGetIntValue", "vsv"},
    {0x5c8080, CV_CDECL, 3, K_TEXT, "ServerGetStringValue", "vss"},
    {0x5c8050, CV_CDECL, 1, K_SMALL, "ServerGetPing"},
    {0x5908b0, CV_CDECL, 0, K_SMALL, "Game_MinBuildNumber"},
    {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"},
    {0x402f30, CV_THIS, 1, K_VALUE, "CFrameWindow::Draw"},
    {0x519cf0, CV_THIS, 0, K_VALUE, "ServerBrowser_Refresh"},
    {0x4d4e80, CV_THIS, 2, K_VALUE, "Pair_Ctor"},
    {0x48dcc0, CV_CDECL, 0, K_VALUE, "CEntityAnimated_ClassInfo"},
    {0x5176f0, CV_THIS, 5, K_FIXED3, "Entity_CreateFromModel"},
    {0x517a80, CV_THIS, 1, K_VALUE, "WindowModelPreview_SetModel"},
    {0x55c550, CV_THIS, 2, K_FLOAT, "Random_Range"},
    {0x55c470, CV_THIS, 1, K_SMALL, "Random_Below"},
    {0x48b290, CV_THIS, 0, K_VALUE, "Entity_Model"},
    {0x48aed0, CV_THIS, 0, K_SMALL, "Entity_Sequence"},
    {0x517240, CV_THIS, 1, K_FIXED4, "CModel::FindSequence", "s"},
    {0x5173f0, CV_THIS, 1, K_FIXED5, "CModel::Sequence"},
    {0x517200, CV_THIS, 1, K_TEXT, "CModel::SequenceName"},
    {0x48f700, CV_THIS, 1, K_VALUE, "EntityAnimated_PlaySequence", "s"},
};
constexpr int NSTUBS2 = sizeof STUBS2 / sizeof STUBS2[0];
static_assert(NSTUBS2 <= NSTUB);

// The templates of the number fields and check boxes (the server dialog's)
constexpr int W_NUMBER_T = 14, W_CHECK_T = 15;
const char *const MAPS[4] = {"Levels/Multiplayer/Arena.zax", "Levels/x", "Levels/", "Levels/Multiplayer/A longer name.zax"};

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls != -1)
        return;
    if ((DWORD)slot == 0x46ec30) {
        ini_effect(args, answer);
        return;
    }
    switch ((DWORD)slot) {
    case 0x5179b0:
    case 0x4020d0: made(answer, W_PLAIN_T); return;
    case 0x411b90:   // a number field: the template's vtable and its edit field
        made(answer, W_NUMBER_T);
        if (in_block(answer))
            at_(at<BYTE *>(answer), 0x100) = ptrv(xalloc(0x120));
        return;
    case 0x412280: made(answer, W_CHECK_T); return;
    case 0x4a3b60:   // FileNameList_GetName (out, i): a path into out
        at<void(THISCALL *)(DWORD, const char *)>(0x4dee00)(args[0], MAPS[args[1] & 3]);
        return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

BYTE *g_dynamic;   // the dynamic block FieldDesc_DynamicPointer gives

void base(Gen &g)
{
    screen_base(g, &effect);
    g_dynamic = xalloc(0x20);
    g_case.fixed[0] = ptrv(g_dynamic);
    set_kind(C_SLIDER, 77, K_VALUE);
    case_global(0x6bd2fc, g.next());
    case_global(0x6bd300, g.next());
    case_global(0x6c0520, g.next());
    case_global(0x6cfde8, g.next());
}

DWORD zstring(const char *text)
{
    BYTE *z = xalloc(4);
    make_text(z, text);
    return ptrv(z);
}

void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(6) ? 0 : ptrv(win(W_PARENT));
}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }

// The popup menu: a fake frame window with items {id, button} and a second array of fake windows
void popup(Gen &g)
{
    BYTE *w = make_plain_window(0, C_FRAME, g);
    const DWORD n1 = g.below(5), n2 = g.below(4);
    BYTE *d1 = xalloc(4 * (n1 + 2)), *d2 = xalloc(4 * (n2 + 2));
    for (DWORD i = 0; i < n1; i++) {
        BYTE *item = xalloc(8);
        at_(item, 0) = g.next();
        at_(item, 4) = ptrv(win(14));
        at_(d1, 4 * i) = g.chance(6) ? 0 : ptrv(item);
    }
    for (DWORD i = 0; i < n2; i++)
        at_(d2, 4 * i) = g.chance(6) ? 0 : ptrv(win(14 + (int)(i & 1)));
    at_(w, 0x120) = 0x5f8810, at_(w, 0x124) = ptrv(d1), at_(w, 0x128) = n1, at_(w, 0x12c) = n1 + g.below(3);
    at_(w, 0x130) = 0x5f8804, at_(w, 0x134) = ptrv(d2), at_(w, 0x138) = n2, at_(w, 0x13c) = n2 + g.below(3);
    if (g.chance(5))
        at_(w, 0x124) = 0, at_(w, 0x128) = 0, at_(w, 0x12c) = 0;
    at_(w, 0x140) = g.below(300);
    at_(w, 0x14c) = n1 ? g.below(n1) : 0;
    g_self = w;
}
void s_popup_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = zstring(TEXTS[g.below(8)]);
    g_args[1] = zstring(g.chance(3) ? "A very long text that could well be wider than five hundred pixels in the game's font, "
                                      "so it has to be cut by the font's own function"
                                    : TEXTS[g.below(8)]);
}
void s_popup_dtor(Gen &g)
{
    popup(g);
    // the second array's elements are deleted through slot 0: the fakes 14 and 15
    make_plain_window(14, C_WINDOW, g);
    make_plain_window(15, C_BUTTON, g);
    s_flags(g);
}
void s_popup_run(Gen &g)
{
    popup(g);
    // (the original reads the chosen item without a test)
    if (at_(g_self, 0x128) && at_(g_self, 0x124) && !at_(at_<BYTE *>(g_self, 0x124), 4 * at_(g_self, 0x14c))) {
        BYTE *item = xalloc(8);
        at_(item, 0) = g.next();
        at_(at_<BYTE *>(g_self, 0x124), 4 * at_(g_self, 0x14c)) = ptrv(item);
    }
    if (!at_(g_self, 0x128) || !at_(g_self, 0x124)) {
        BYTE *d = xalloc(8);
        BYTE *item = xalloc(8);
        at_(item, 0) = g.next();
        at_(d, 0) = ptrv(item);
        at_(g_self, 0x124) = ptrv(d), at_(g_self, 0x128) = 1, at_(g_self, 0x12c) = 2, at_(g_self, 0x14c) = 0;
    }
    BYTE *screen = xalloc(0x110);
    at_(screen, 0x104) = g.next();
    at_(win(W_REPORTER), 0x34) = ptrv(screen);
}
void s_popup_add(Gen &g)
{
    popup(g);
    for (DWORD i = 0; i < at_(g_self, 0x138); i++)
        if (!at_(at_<BYTE *>(g_self, 0x134), 4 * i))
            at_(at_<BYTE *>(g_self, 0x134), 4 * i) = ptrv(win(14));
    g_args[0] = zstring(TEXTS[g.below(8)]);
    g_args[1] = g.next();
}

// The CD key dialog under test
void cd_key(Gen &g)
{
    members(g, win(0), 0x134, 0x148);
    make_plain_window(W_OTHER, C_EDIT, g);
    make_plain_window(W_OTHER2, C_LABEL, g);
    at_(win(0), 0x140) = ptrv(win(W_OTHER));
    at_(win(0), 0x138) = ptrv(win(W_OTHER2));
}
void s_cd_notify(Gen &g)
{
    cd_key(g);
    const DWORD k = g.below(6);
    g_args[0] = k < 5 ? at_(win(0), 0x134 + 4 * k) : ptrv(win(W_OTHER2 - 1));
    g_args[1] = g.chance(3) ? g.below(4) : 0;
    if (g.chance(2))
        g_args[0] = at_(win(0), g.chance(2) ? 0x144 : 0x148);
}
void s_cd_validate(Gen &g) { cd_key(g); }

// The network options dialog under test: the sliders, the preview, the player options
void net_options(Gen &g)
{
    members(g, win(0), 0x134, 0x154);
    make_plain_window(W_OTHER, C_SLIDER, g);
    make_plain_window(W_OTHER2, C_SLIDER, g);
    make_plain_window(13, C_EDIT, g);
    at_(win(0), 0x148) = ptrv(win(W_OTHER));
    at_(win(0), 0x150) = ptrv(win(W_OTHER2));
    at_(win(0), 0x138) = ptrv(win(13));
    BYTE *preview = xalloc(0x18);
    at_(preview, 0xc) = g.chance(3) ? 0 : g.next();
    at_(win(0), 0x140) = ptrv(preview);
    BYTE *options = xalloc(0x10);
    make_text(options, TEXTS[g.below(8)]);
    at_(options, 4) = g.next(), at_(options, 8) = g.below(0x13c);
    at_(win(0), 0x158) = ptrv(options);
}
void s_net_dtor(Gen &g)
{
    net_options(g);
    s_flags(g);
}

void s_copy_text(Gen &g)
{
    g_args[0] = g.next(), g_args[1] = g.next();   // ecx, edx: unused
    const char *t = TEXTS[g.below(8)];
    DWORD n = 0;
    while (t[n])
        n++;
    g_args[2] = ptrv(t);
    g_args[3] = g.chance(3) ? g.below(n + 1) : n;
}
void s_paste_text(Gen &g)
{
    DWORD *out = reinterpret_cast<DWORD *>(xalloc(4));
    *out = g.chance(2) ? 0 : ptrv(xalloc(8));
    g_args[0] = ptrv(out);
    g_args[1] = g.next();
}


// ---- The server dialog, the server browser, the model preview, the question dialog -----------------------

char g_object_args[MAXSLOT + 1];

// The templates of the number fields and check boxes, the game type records (0x6c0318: the record's object a
// game object: slot 45 a ZString of the case, slot 37 a bool)
void base2(Gen &g)
{
    base(g);
    make_window(W_NUMBER_T, C_NUMBER);
    make_window(W_CHECK_T, C_CHECKBOX);
    set_kind(C_NUMBER, 48, K_FLOAT);
    set_kind(C_CHECKBOX, 79, K_VALUE);
    const DWORD n = g.below(5);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(4) ? 0 : ptrv(win(W_WORLD));
        recs[i * 0x80 + 0x7f] = g.chance(5) ? 1 : 0;
    }
    case_global(0x6c0318, g.chance(6) ? 0 : ptrv(recs));
    case_global(0x6c031c, n);
    case_global(0x62e68c, 0);
    case_global(0x62e690, 0);
    BYTE *name = xalloc(4);
    make_text(name, TEXTS[g.below(8)]);
    g_case.fixed[1] = ptrv(name);
    set_kind(C_OBJECT, 45, K_FIXED1);
    set_kind(C_OBJECT, 37, K_BOOL);
    set_kind(C_OBJECT, 22, K_SMALL);
    set_kind(C_LISTBOX, 77, K_FIXED2);
    set_kind(C_COMBO, 76, K_SMALL);
    g_case.fixed[2] = g.chance(4) ? 0xffffffff : g.below(6);
}
// The server options of a case (+0 the name, +8 the map: ZStrings; +0xc .. +0x19 numbers and switches)
BYTE *server_options(Gen &g)
{
    BYTE *o = xalloc(0x20);
    make_text(o, TEXTS[g.below(8)]);
    make_text(o + 8, TEXTS[g.below(8)]);
    at_(o, 0x10) = g.chance(2) ? 0 : g.below(50);
    at_(o, 0x14) = g.chance(2) ? 0 : g.below(50);
    o[0x18] = g.chance(2), o[0x19] = g.chance(2);
    return o;
}
// The layers (maps) of the server dialog: +0x90 the minimum, +0x94 the maximum players, +0x9c the game types
void layers(Gen &g, BYTE *self, DWORD n)
{
    BYTE *d = xalloc(4 * (n + 1));
    for (DWORD i = 0; i < n; i++) {
        BYTE *l = xalloc(0x100);
        at_(l, 0x90) = g.below(8);
        at_(l, 0x94) = g.chance(3) ? 0 : g.below(40);
        at_(l, 0x9c) = g.next();
        at_(d, 4 * i) = ptrv(l);
    }
    at_(self, 0x180) = 0x5f7b88, at_(self, 0x184) = ptrv(d), at_(self, 0x188) = n, at_(self, 0x18c) = n + 1;
}
void server_dialog(Gen &g)
{
    BYTE *w = win(0);
    members(g, w, 0x134, 0x17c);
    at_(w, 0x13c) = ptrv(win(W_COMBO_T));
    at_(w, 0x144) = ptrv(win(W_LIST_T));
    at_(w, 0x150) = ptrv(win(W_LABEL_T));
    at_(w, 0x138) = ptrv(win(W_EDIT_T));
    at_(w, 0x14c) = ptrv(win(W_NUMBER_T));
    at_(win(W_NUMBER_T), 0x100) = ptrv(xalloc(0x120));
    at_(w, 0x16c) = ptrv(win(W_CHECK_T));
    at_(w, 0x174) = ptrv(win(W_CHECK_T));
    at_(w, 0x194) = ptrv(win(W_OBJECT));
    at_(w, 0x198) = ptrv(xalloc(0x14));
    at_(w, 0x190) = ptrv(xalloc(0x14));
    at_(w, 0x19c) = ptrv(server_options(g));
    w[0x1a0] = g.chance(2);
    layers(g, w, 21);
}
void s_sd_ctor(Gen &g)
{
    s_ctor(g);
    g_args[1] = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
    set_kind(C_OBJECT, 1, K_WINDOW_NN);
}
void s_sd_dtor(Gen &g)
{
    BYTE *w = win(0);
    object_args(g_object_args, "0:1,1:1,3:1");
    at_(w, 0x190) = g.chance(4) ? 0 : ptrv(win(W_OBJECT));
    at_(w, 0x194) = g.chance(4) ? 0 : ptrv(win(W_APP));
    at_(w, 0x198) = g.chance(4) ? 0 : ptrv(win(W_WORLD));
    const DWORD n = g.below(4);
    BYTE *d = xalloc(4 * (n + 1));
    for (DWORD i = 0; i < n; i++)
        at_(d, 4 * i) = g.chance(5) ? 0 : ptrv(win(W_REPORTER));
    at_(w, 0x180) = 0x5f7b88, at_(w, 0x184) = g.chance(5) ? 0 : ptrv(d), at_(w, 0x188) = at_(w, 0x184) ? n : 0;
    at_(w, 0x18c) = n + 1;
    s_flags(g);
}
void s_sd(Gen &g)
{
    server_dialog(g);
    object_args(g_object_args, "6:1");
    g_case.fixed[3] = ptrv(win(W_OBJECT));
    set_kind(C_OBJECT, 1, K_FIXED3);
}

// The server browser under test (window 0): the names a FieldNameList at +0x17c (a game object's vtable), the
// status ZString +0x198
void browser(Gen &g)
{
    BYTE *w = win(0);
    members(g, w, 0x134, 0x170);
    at_(w, 0x138) = ptrv(win(W_LIST_T));
    at_(w, 0x148) = ptrv(win(W_LABEL_T));
    at_(w, 0x13c) = ptrv(win(W_BUTTON_T));
    at_(w, 0x140) = ptrv(win(W_BUTTON_T));
    at_(w, 0x17c) = at_(win(W_OBJECT), 0);
    make_text(w + 0x198, TEXTS[g.below(8)]);
    at_(w, 0x190) = g.next();
    w[0x194] = g.chance(2), w[0x178] = g.chance(2);
    at_(w, 0x19c) = g.chance(3) ? 0 : g.below(120);
    const float left = g.chance(3) ? 0.0f : (float)g.range(-20, 40) / 16.0f;
    memcpy(w + 0x174, &left, 4);
}
void s_sb_ctor(Gen &g) { s_ctor(g); }
void s_sb_dtor(Gen &g)
{
    browser(g);
    s_flags(g);
}
void s_sb_update(Gen &g)
{
    browser(g);
    const float dt = g.chance(4) ? 0.0f : (float)g.range(-5, 60) / 32.0f;
    memcpy(&g_args[0], &dt, 4);
}
void s_sb_frame(Gen &g)
{
    browser(g);
    g_args[0] = g.next();
}
void s_sb_server(Gen &g)
{
    browser(g);
    g_args[0] = g.next();
}
void s_sb_text(Gen &g)
{
    browser(g);
    g_args[0] = zstring(TEXTS[g.below(8)]);
}

// The model preview: the entity a game object (slots 1 and 104 with their arguments)
void preview(Gen &g)
{
    object_args(g_object_args, "1:1,104:2");
    BYTE *p = xalloc(0x20);
    at_(p, 0xc) = g.chance(5) ? 0 : ptrv(win(W_OBJECT));
    at_(p, 0x10) = g.below(10);
    g_self = p;
    g_case.fixed[3] = ptrv(win(W_OBJECT));
    g_case.fixed[4] = g.chance(3) ? 0xffffffff : g.below(9);
    g_case.fixed[5] = ptrv(win(W_APP));
    set_kind(C_OBJECT, 25, K_SMALL);
}
void s_pv_make(Gen &g)
{
    preview(g);
    g_self = xalloc(0x20);
    g_args[0] = ptrv(win(W_WORLD));
    g_args[1] = g.below(10);
}
void s_pv_dtor(Gen &g)
{
    preview(g);
    s_flags(g);
}
void s_pv_set(Gen &g)
{
    preview(g);
    g_args[0] = g.chance(3) ? 1 : 0;
}
void s_question(Gen &g)
{
    s_ctor(g);
    g_args[1] = g.next();
    g_args[2] = ptrv(TEXTS[g.below(8)]);
    g_args[3] = ptrv(TEXTS[g.below(8)]);
    g_args[4] = ptrv(TEXTS[g.below(8)]);
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6bd2fc, 0x6bd300, 0x6c0520, 0x6cfde8, 0x6c0318, 0x6c031c,
                              0x62e68c, 0x62e690};

const Test TESTS[] = {
    {"PopupMenu ctor", 0x4de290, CALL_THIS, 2, ~0u, 200, s_popup_ctor, STUBS, NSTUBS},
    {"PopupMenu deleting dtor", 0x4de3c0, CALL_THIS, 1, ~0u, 150, s_popup_dtor, STUBS, NSTUBS},
    {"PopupMenu dtor", 0x4de5e0, CALL_THIS, 0, 0, 150, s_popup_dtor, STUBS, NSTUBS},
    {"PopupMenu Run", 0x4de740, CALL_THIS, 0, ~0u, 100, s_popup_run, STUBS, NSTUBS},
    {"PopupMenu AddItem", 0x4de790, CALL_THIS, 2, 0, 300, s_popup_add, STUBS, NSTUBS},
    {"CdKeyDialog ctor", 0x46b050, CALL_THIS, 1, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"CdKeyDialog deleting dtor", 0x46b380, CALL_THIS, 1, ~0u, 50, s_flags, STUBS, NSTUBS},
    {"CdKeyDialog Notify", 0x46b3b0, CALL_THIS, 2, 0, 400, s_cd_notify, STUBS, NSTUBS},
    {"CdKeyDialog Validate", 0x46b420, CALL_THIS, 0, 0xff, 300, s_cd_validate, STUBS, NSTUBS},
    {"NetOptionsDialog ctor", 0x46d010, CALL_THIS, 1, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"NetOptionsDialog deleting dtor", 0x46d3e0, CALL_THIS, 1, ~0u, 150, s_net_dtor, STUBS, NSTUBS},
    {"NetOptionsDialog ApplyColours", 0x46d450, CALL_THIS, 0, 0, 300, net_options, STUBS, NSTUBS},
    {"Clipboard CopyText", 0x46d700, CALL_FAST, 4, 0, 60, s_copy_text, STUBS, NSTUBS},
    {"Clipboard PasteText", 0x46d760, CALL_FAST, 2, ~0u, 40, s_paste_text, STUBS, NSTUBS},
};
const Test TESTS2[] = {
    {"ServerDialog ctor", 0x4d3080, CALL_THIS, 2, ~0u, 300, s_sd_ctor, STUBS2, NSTUBS2},
    {"ServerDialog deleting dtor", 0x4d3b00, CALL_THIS, 1, ~0u, 100, s_sd_dtor, STUBS2, NSTUBS2},
    {"ServerDialog dtor", 0x4d3b80, CALL_THIS, 0, 0, 100, s_sd_dtor, STUBS2, NSTUBS2},
    {"ServerDialog FillGameTypes", 0x4d3c80, CALL_THIS, 0, 0, 300, s_sd, STUBS2, NSTUBS2},
    {"ServerDialog FillMaps", 0x4d3e20, CALL_THIS, 0, 0, 300, s_sd, STUBS2, NSTUBS2},
    {"ServerDialog OnOk", 0x4d3f90, CALL_THIS, 0, 0, 300, s_sd, STUBS2, NSTUBS2},
    {"ServerBrowser ctor", 0x518e00, CALL_THIS, 1, ~0u, 200, s_sb_ctor, STUBS2, NSTUBS2},
    {"ServerBrowser deleting dtor", 0x519360, CALL_THIS, 1, ~0u, 60, s_sb_dtor, STUBS2, NSTUBS2},
    {"ServerBrowser Update", 0x5193c0, CALL_THIS, 1, 0, 200, s_sb_update, STUBS2, NSTUBS2},
    {"ServerBrowser Draw", 0x519430, CALL_THIS, 1, 0, 30, s_sb_frame, STUBS2, NSTUBS2},
    {"ServerBrowser Join", 0x519730, CALL_THIS, 0, 0, 300, browser, STUBS2, NSTUBS2},
    {"ServerBrowser AddServer", 0x519b60, CALL_THIS, 1, 0, 200, s_sb_server, STUBS2, NSTUBS2},
    {"ServerBrowser Refresh", 0x519cf0, CALL_THIS, 0, 0, 200, browser, STUBS2, NSTUBS2},
    {"ServerBrowser AddText", 0x519ea0, CALL_THIS, 1, 0, 60, s_sb_text, STUBS2, NSTUBS2},
    {"ModelPreview Make", 0x5179b0, CALL_THIS, 2, ~0u, 200, s_pv_make, STUBS2, NSTUBS2},
    {"ModelPreview deleting dtor", 0x517a40, CALL_THIS, 1, ~0u, 60, s_pv_dtor, STUBS2, NSTUBS2},
    {"ModelPreview SetModel", 0x517a80, CALL_THIS, 1, 0, 600, s_pv_set, STUBS2, NSTUBS2},
    {"QuestionDialog ctor", 0x5156e0, CALL_THIS, 5, ~0u, 150, s_question, STUBS2, NSTUBS2},
};

}  // namespace

ZAX_CHECKS(ui_dialogs)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui dialogs: the popup menu, the CD key, network options and server dialogs, the server browser, the model preview"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 22000);
    run_tests(t, g_hooks, &g_globals, TESTS2, sizeof TESTS2 / sizeof TESTS2[0], &base2, 23000);
    t.report();
}

}  // namespace zax
