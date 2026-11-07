// The game's small dialogs (docs/DECOMPILATION.md 9, "The rest of the user interface"): the popup menu, the CD
// key dialog, the network options dialog, the clipboard's text wrappers, the server dialog, the server browser
// and the model preview window. Dialogs of the window toolkit (ui_frame.cpp) built from its labels, edit fields,
// sliders and buttons; every call into the toolkit or the rest of the game goes through its exe address.
//
// Layouts [confirmed: the constructors and destructors below]:
//   popup menu (0x150, vtable 0x5f86d0, a frame window 0x402d80): +0x118 the text label, +0x11c running,
//     +0x120 the items (a pool pointer array, vtable 0x5f8810/0x5f881c: blocks of 8 bytes {id, button} from
//     operator new), +0x130 a second array (vtable 0x5f8804/0x5f8828: objects with a deleting destructor, +4 a
//     window), +0x140 the width of the buttons so far, +0x14c the chosen item (-1 none)
//   CD key dialog (0x14c, vtable 0x5ef828, a CDialog): +0x134 the label "Enter CD key" (ZString 0x6bd2a0),
//     +0x138 the error label, +0x13c the label of 0x6bd28c, +0x140 the edit field (at most 50 characters,
//     "XXXX-XXXX-XXXX-XXXX"), +0x144 Ok, +0x148 Cancel
//   network options dialog (0x15c, vtable 0x5efc00): +0x134 the name label, +0x138 the name's edit field (200
//     characters), +0x13c the browsable image (0x599330) with the model preview +0x140, +0x144/+0x14c the labels
//     and +0x148/+0x150 the sliders (0..0x13b) of the two colours, +0x154 Ok, +0x158 the player options 0x6bd2f8
//     (+0 the name, +4/+8 the colours)
#include "ui_screens.h"
#include "file_search.h"

namespace zax::ui {

namespace {

constexpr DWORD INI = 0x6bd330, INI_FILE = 0x6bd430, PLAYER_OPTIONS = 0x6bd2f8;
constexpr int SET_TITLE = 43, TITLE = 44, SET_VALUE = 76, VALUE = 77;

void ini_get(const char *key, void *z) { fn<void(THISCALL *)(DWORD, const char *, void *)>(0x46ec30)(glob_addr(INI), key, z); }
void ini_write(const char *key, const char *value) { fn<void(__fastcall *)(DWORD, DWORD, const char *, const char *)>(0x4a47c0)(glob_addr(INI), glob_addr(INI_FILE), key, value); }
void play_name(DWORD name) { fn<void(THISCALL *)(DWORD, DWORD)>(0x57e820)(glob_addr(G_SOUND_LIST), name); }

}  // namespace

// ---- The popup menu ------------------------------------------------------------------------------------------

// The constructor (the title, the text: ZStrings): a frame window without a parent, the text in a label (cut to
// 500 pixels of the font with 0x4a7450 when wider)
BYTE *THISCALL PopupMenu_Ctor(BYTE *self, DWORD title, DWORD text)   // 0x4de290
{
    frame_window_ctor(self, 0, c_str(title));
    m(self, 0x118) = 0;
    mb(self, 0x11c) = 0;
    m(self, 0x124) = 0, m(self, 0x128) = 0, m(self, 0x12c) = 0;
    m(self, 0x120) = vt(0x5f8810);
    m(self, 0x134) = 0, m(self, 0x138) = 0, m(self, 0x13c) = 0;
    m(self, 0x130) = vt(0x5f8804);
    m(self, 0x140) = 0, m(self, 0x144) = 0, m(self, 0x148) = 0;
    m(self, 0x14c) = 0xffffffff;
    m(self, 0) = vt(0x5f86d0);
    DWORD local;
    zs_copy(&local, text);
    const DWORD width = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x4a7310)(m(self, W_FONT), c_str(text), -1);
    if (width > 500) {
        DWORD cut;
        const DWORD r = fn<DWORD(THISCALL *)(DWORD, void *, const char *, DWORD)>(0x4a7450)(m(self, W_FONT), &cut, c_str(text), 500);
        zs_assign(&local, r);
        zs_dtor(&cut);
    }
    BYTE *label = new_label(self, c_str(&local));
    m(self, 0x118) = pv(label);
    add_child(self, label, 1, 0, NOWHERE, NOWHERE, 0);
    zs_dtor(&local);
    return self;
}

// The destructor: the items freed (operator delete), the second array's objects deleted (slot 0 with 1), both
// arrays' data freed, the frame window's destructor
void THISCALL PopupMenu_Dtor(BYTE *self)   // 0x4de5e0
{
    m(self, 0) = vt(0x5f86d0);
    array_delete_all(self + 0x120, [](DWORD e) { op_delete(bp(e)); });
    array_delete_all(self + 0x130, [](DWORD e) { vc(bp(e), 0, 1); });
    array_free_pool(self + 0x130, vt(0x5f8828));
    array_free_pool(self + 0x120, vt(0x5f881c));
    frame_window_dtor(self);
}
BYTE *THISCALL PopupMenu_DeletingDtor(BYTE *self, DWORD flags)   // 0x4de3c0
{
    PopupMenu_Dtor(self);
    if (flags & 1)
        pool_delete(self, 0x150);
    return self;
}

// Runs the menu modally (0x40f5f0 with the screen's frame: the reporter's screen +0x104) and gives the chosen
// item's id (the original reads the item at +0x14c without a test: -1 reads before the array)
DWORD THISCALL PopupMenu_Run(BYTE *self)   // 0x4de740
{
    mb(self, 0x11c) = 1;
    const DWORD frame = m(bp(m(bp(gd(glob_addr(G_REPORTER))), 0x34)), 0x104);
    fn<void(__fastcall *)(BYTE *, DWORD, DWORD, DWORD)>(0x40f5f0)(self, frame, 0, 0);
    const DWORD i = m(self, 0x14c);
    mb(self, 0x11c) = 0;
    const DWORD id = m(bp(m(bp(m(self, 0x124)), i * 4)), 0);
    m(self, 0x14c) = 0xffffffff;
    return id;
}

// AddItem (the text as a ZString, the id): a button below the label (the first item, how 8) or the last of the
// second array (7), else after the previous item (0); the item block {id, button} appended; the width grows by
// the button's and the gap (+0x79) after the first
void THISCALL PopupMenu_AddItem(BYTE *self, DWORD text, DWORD id)   // 0x4de790
{
    BYTE *button = nullptr;
    if (BYTE *b = alloc(0x138))
        button = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(b, self, c_str(text), 0);
    const DWORD n2 = m(self, 0x138);
    const DWORD rel = n2 ? m(bp(m(bp(m(self, 0x134)), n2 * 4 - 4)), 4) : 0;
    const DWORD how = m(self, 0x128) ? 0 : (n2 ? 7 : 8);
    vc(self, V_ADD_CHILD, button, how, rel, NOWHERE, NOWHERE, 0);
    BYTE *item = op_new(8);
    m(item, 4) = pv(button);
    m(item, 0) = id;
    const int gap = m(self, 0x128) ? (signed char)mb(self, W_BORDER2) : 0;
    m(self, 0x140) = m(self, 0x140) + (mi(button, W_RECT + 8) - mi(button, W_RECT)) + gap;
    vc(self + 0x120, 2, &item);
}

// ---- The CD key dialog ---------------------------------------------------------------------------------------

// The constructor (the parent): the labels, the last error message of the INI file ("Last CD Key Error Message",
// formatted with 0x6bd294), the edit field with the INI file's "CD Key", Ok (centred) and Cancel
BYTE *THISCALL CdKeyDialog_Ctor(BYTE *self, DWORD parent)   // 0x46b050
{
    dialog_ctor(self, parent, c_str(glob_addr(0x6bd298)));
    m(self, 0) = vt(0x5ef828);
    BYTE *w = new_label_z(self, glob_addr(0x6bd2a0));
    m(self, 0x134) = pv(w);
    add_child(self, w, 1, 0, NOWHERE, NOWHERE, 0);
    w = new_label(self, glob<const char *>(EMPTY_TEXT));
    m(self, 0x138) = pv(w);
    add_child(self, w, 8, 0, NOWHERE, NOWHERE, 0);
    BYTE *label = bp(m(self, 0x138));
    BYTE *first = bp(m(self, 0x134));
    m(label, 0x10c) = 0xffffffff;
    m(label, 0x108) = m(first, W_RECT + 8) - m(first, W_RECT);
    vc(label, SET_TITLE, c_str(label + W_TITLE));
    vc(bp(m(self, 0x138)), SET_TITLE, glob_addr(0x60f844));
    vc(bp(m(self, 0x138)), VALUE, glob_addr(EMPTY_TEXT));
    DWORD error, key;
    zs_ctor(&error);
    ini_get(glob<const char *>(0x60f828), &error);
    if (zs_not_empty(&error)) {
        DWORD text;
        zs_ctor(&text);
        const char *fmt = c_str(glob_addr(0x6bd294));
        zs_format(&text, fmt, c_str(&error));
        vc(bp(m(self, 0x138)), SET_TITLE, c_str(&text));
        zs_dtor(&text);
    }
    w = new_label_z(self, glob_addr(0x6bd28c));
    m(self, 0x13c) = pv(w);
    add_child(self, w, 8, 0, NOWHERE, NOWHERE, 0);
    w = new_edit(self, glob<const char *>(0x60f814));
    m(self, 0x140) = pv(w);
    add_child(self, w, 0x10, 0, NOWHERE, NOWHERE, 0);
    mb(bp(m(self, 0x140)), 0x111) = 0x32;
    fn<void(THISCALL *)(DWORD, DWORD)>(0x411ad0)(m(self, 0x140), 0);
    zs_ctor(&key);
    ini_get(glob<const char *>(0x60f80c), &key);
    vc(bp(m(self, 0x140)), SET_TITLE, c_str(&key));
    w = new_button_z(self, glob_addr(0x6bd29c), 0);
    m(self, 0x144) = pv(w);
    add_child(self, w, 0xc, 0, 0, 8, 0);
    BYTE *ok = bp(m(self, 0x144));
    const int left = mi(ok, W_RECT);
    set_left(ok, left - ((signed char)mb(self, W_BORDER2) + mi(ok, W_RECT + 8) - left) / 2);
    w = new_button_z(self, glob_addr(0x6bd284), 0);
    m(self, 0x148) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    m(self, 0x120) = m(self, 0x148);
    m(self, 0x11c) = m(self, 0x144);
    focus(self, bp(m(self, 0x140)));
    zs_dtor(&key);
    zs_dtor(&error);
    return self;
}
BYTE *THISCALL CdKeyDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x46b380
{
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x14c);
    return self;
}
// Slot 21 (the child, what): Ok closes when the key is valid (0x46b420), else the error sound; Cancel closes the
// game (Platform_CloseWindow 0x4d2470); a change of the edit field is taken; the rest the frame window's
DWORD THISCALL CdKeyDialog_Notify(BYTE *self, DWORD from, DWORD what)   // 0x46b3b0
{
    if (what == 0) {
        if (from == m(self, 0x144)) {
            if ((BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x46b420)(self))
                return vc(self, V_CLOSE);
            play_name(glob_addr(0x60a4d8));
        }
        if (from == m(self, 0x148))
            return fn<DWORD(__cdecl *)()>(0x4d2470)();
    } else if (what == 1 && from == m(self, 0x140)) {
        return what;
    }
    frame_notify(self, from, what);
    return 0;
}
// The key check: shorter than 20 characters or rejected by CdKey_Check 0x46b540 shows an error (0x6bd290 /
// 0x6bd288) and gives 0; a valid key goes to the INI file ("CD Key"; "Last CD Key Error Message" emptied); al
BYTE THISCALL CdKeyDialog_Validate(BYTE *self)   // 0x46b420
{
    DWORD key;
    zs_ctor_text(&key, vc<const char *>(bp(m(self, 0x140)), TITLE));
    DWORD message;
    if (zs_length(&key) < 0x14) {
        message = glob_addr(0x6bd290);
    } else {
        if ((BYTE)fn<DWORD(__fastcall *)(void *, DWORD)>(0x46b540)(&key, 0)) {
            ini_write(glob<const char *>(0x60f80c), c_str(&key));
            ini_write(glob<const char *>(0x60f828), glob<const char *>(EMPTY_TEXT));
            zs_dtor(&key);
            return 1;
        }
        message = glob_addr(0x6bd288);
    }
    BYTE *label = bp(m(self, 0x138));
    vc(label, SET_TITLE, c_str(message));
    zs_dtor(&key);
    return 0;
}

// ---- The network options dialog --------------------------------------------------------------------------------

// The constructor (the parent): the player options loaded (0x46ce50), the name's label and edit field, the
// browsable image with the model preview of "Crater Multiplayer", the two colour sliders with their labels
// (the labels as wide as the name's label), the colours applied (0x46d450), Ok
BYTE *THISCALL NetOptionsDialog_Ctor(BYTE *self, DWORD parent)   // 0x46d010
{
    dialog_ctor(self, parent, c_str(glob_addr(0x6bd310)));
    m(self, 0) = vt(0x5efc00);
    m(self, 0x158) = glob_addr(PLAYER_OPTIONS);
    fn<void(THISCALL *)(DWORD)>(0x46ce50)(glob_addr(PLAYER_OPTIONS));
    BYTE *w = new_label_z(self, glob_addr(0x6bd318));
    m(self, 0x134) = pv(w);
    add_child(self, w, 1, 0, NOWHERE, NOWHERE, 0);
    w = new_edit(self, glob<const char *>(EMPTY_TEXT));
    m(self, 0x138) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    mb(bp(m(self, 0x138)), 0x111) = 0xc8;
    BYTE *edit = bp(m(self, 0x138));
    vc(edit, SET_TITLE, c_str(m(self, 0x158)));
    w = alloc(0x10c);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x599330)(w, self);
    m(self, 0x13c) = pv(w);
    BYTE *preview = op_new(0x18);
    if (preview) {
        const DWORD list = gd(glob_addr(0x6cfde8));
        const DWORD index = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(0x6cfdd8), glob<const char *>(0x60faa4), -1);
        preview = fn<BYTE *(THISCALL *)(BYTE *, DWORD, DWORD)>(0x5179b0)(preview, list, index);
    }
    m(self, 0x140) = pv(preview);
    fn<void(THISCALL *)(DWORD, BYTE *)>(0x5993c0)(m(self, 0x13c), preview);
    add_child(self, bp(m(self, 0x13c)), 7, m(self, 0x134), NOWHERE, NOWHERE, 0);
    const DWORD labels[2][3] = {{glob_addr(0x6bd308), 0x144, 0x13c}, {glob_addr(0x6bd30c), 0x14c, 0x144}};
    const DWORD sliders[2][2] = {{0x148, 4}, {0x150, 8}};
    for (int k = 0; k < 2; k++) {
        w = new_label_z(self, labels[k][0]);
        m(self, labels[k][1]) = pv(w);
        add_child(self, w, 7, m(self, labels[k][2]), NOWHERE, NOWHERE, 0);
        const int *r = vc<const int *>(bp(m(self, 0x134)), V_GET_RECT);
        set_width(bp(m(self, labels[k][1])), r[2] - r[0]);
        vc(bp(m(self, labels[k][1])), SET_VALUE, 2);
        w = alloc(0x10c);
        if (w)
            w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x4101f0)(w, self, 0x13b, 1);
        m(self, sliders[k][0]) = pv(w);
        add_child(self, w, 0x10, 0, NOWHERE, NOWHERE, 0);
        vc(bp(m(self, sliders[k][0])), SET_VALUE, m(bp(m(self, 0x158)), sliders[k][1]), 1);
    }
    fn<void(THISCALL *)(BYTE *)>(0x46d450)(self);
    set_width(bp(m(self, 0x138)), 0xe6);
    set_width(bp(m(self, 0x148)), 0xe6);
    set_width(bp(m(self, 0x150)), 0xe6);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40e590)(self, m(self, 0x138), NOWHERE);
    BYTE *image = bp(m(self, 0x13c));
    set_left(image, (mi(image, W_RECT) - mi(image, W_RECT + 8) + 0xe6) / 2 + mi(bp(m(self, 0x138)), W_RECT));
    w = new_button_z(self, glob_addr(0x6bd314), 0);
    m(self, 0x154) = pv(w);
    add_child(self, w, 0xc, 0, 0, 6, 0);
    m(self, 0x11c) = m(self, 0x154);
    m(self, 0x120) = m(self, 0x154);
    focus(self, bp(m(self, 0x138)));
    return self;
}
// The deleting destructor: the name taken from the edit field, the player options saved (0x46ced0) and sent
// (the client options reader's Write)
BYTE *THISCALL NetOptionsDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x46d3e0
{
    m(self, 0) = vt(0x5efc00);
    const char *name = vc<const char *>(bp(m(self, 0x138)), TITLE);
    fn<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(m(self, 0x158), name);
    fn<void(THISCALL *)(DWORD)>(0x46ced0)(m(self, 0x158));
    fn<void(THISCALL *)(DWORD)>(0x5abfe0)(fn<DWORD(__cdecl *)()>(0x5abe30)());
    m(self, 0x158) = 0;
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x15c);
    return self;
}
// The colours from the sliders into the player options and, when the preview's model (its +0xc, or its first
// child) has the dynamic property block of 0x6c0520, as floats into it (+0xc, +0x18)
void THISCALL NetOptionsDialog_ApplyColours(BYTE *self)   // 0x46d450
{
    m(bp(m(self, 0x158)), 4) = vc(bp(m(self, 0x148)), VALUE);
    m(bp(m(self, 0x158)), 8) = vc(bp(m(self, 0x150)), VALUE);
    DWORD part = m(bp(m(self, 0x140)), 0xc);
    if (part && fn<DWORD(__fastcall *)(DWORD)>(0x4fc7c0)(part))
        part = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4fc7d0)(m(bp(m(self, 0x140)), 0xc), 0);
    if (!(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x418fc0)(gd(glob_addr(0x6c0520)), part))
        return;
    BYTE *p = fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x418790)(gd(glob_addr(0x6c0520)), part);
    volatile float a = (float)(long long)m(bp(m(self, 0x158)), 4);
    m<float>(p, 0xc) = a;
    volatile float b = (float)(long long)m(bp(m(self, 0x158)), 8);
    m<float>(p, 0x18) = b;
}

// ---- The clipboard's text wrappers -----------------------------------------------------------------------------

// The text (n bytes) copied with a 0 behind it and given to the clipboard as "Text" (Clipboard_SetText 0x46d590
// with the length including the 0)
void STDCALL Clipboard_CopyText(const char *text, DWORD n)   // 0x46d700
{
    char *copy = reinterpret_cast<char *>(op_new(n + 1));
    for (DWORD i = 0; i < n; i++)
        copy[i] = text[i];
    copy[n] = 0;
    DWORD len = 0;
    while (copy[len])
        len++;
    fn<void(__fastcall *)(DWORD, const char *, DWORD)>(0x46d590)(glob_addr(0x60b078), copy, len + 1);
    if (copy)
        op_delete(copy);
}
// The clipboard's "Text" (Clipboard_GetText 0x46d620, a block of operator new) into *out (the old one freed)
DWORD __fastcall Clipboard_PasteText(DWORD *out)   // 0x46d760
{
    const DWORD text = fn<DWORD(__fastcall *)(DWORD, DWORD)>(0x46d620)(glob_addr(0x60b078), 0);
    if (*out) {
        op_delete(bp(*out));
        *out = 0;
    }
    *out = text;
    return text;
}


// ---- The server dialog ("Muti-Player Configuration": host a game) -------------------------------------------

namespace {

constexpr DWORD GAME_TYPE_LIST = 0x6c02e8, GAME_TYPE_RECORDS = 0x6c0318, GAME_TYPE_COUNT = 0x6c031c;
constexpr DWORD FLOAT_1 = 0x3f800000, FLOAT_2 = 0x40000000, FLOAT_1E9 = 0x4e6e6b28;

// The game type definition i (the inlined record lookup of the definition lists: index 0 when out of range, null
// when the list is empty or the record is being loaded, loaded on first use)
DWORD game_type(DWORD i)
{
    DWORD recs = gd(glob_addr(GAME_TYPE_RECORDS));
    if (!recs)
        return 0;
    if (i >= gd(glob_addr(GAME_TYPE_COUNT)))
        i = 0;
    const DWORD rec = i << 7;
    if (gb(recs + rec + 0x7f))
        return 0;
    if (!m(bp(recs), rec + 0x78)) {
        fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(glob_addr(GAME_TYPE_LIST), i);
        recs = gd(glob_addr(GAME_TYPE_RECORDS));
    }
    return m(bp(recs), rec + 0x78);
}
BYTE *new_number_field(BYTE *parent)   // NumberField::NumberField 0x411b90 (0x11c): "", "19999"
{
    BYTE *w = alloc(0x11c);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x411b90)(w, parent, glob_addr(EMPTY_TEXT), glob_addr(0x61570c)) : nullptr;
}
BYTE *new_check_box(BYTE *parent, DWORD zstring)   // CheckBox::CheckBox 0x412280 (0x144)
{
    BYTE *w = alloc(0x144);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x412280)(w, parent, c_str(zstring), 0) : nullptr;
}
// A number field's range 1 .. 1e9 (+0x10c, +0x110, the step +0x114 and its edit field's +0x118/+0x114)
void unlimited(BYTE *f)
{
    m(f, 0x10c) = FLOAT_1, m(f, 0x110) = FLOAT_1E9, m(f, 0x114) = FLOAT_1;
    BYTE *e = bp(m(f, 0x100));
    m(e, 0x118) = FLOAT_1E9, m(e, 0x114) = FLOAT_1;
}
// A limit's check box (set when there is no limit) and its number field (empty then, else the limit)
void limit(BYTE *self, DWORD box, DWORD field, DWORD value)
{
    BYTE *b = bp(m(self, box));
    if (!value) {
        vc(b, 78, 1);
        vc(bp(m(self, field)), SET_TITLE, glob_addr(EMPTY_TEXT));
    } else {
        vc(b, 78, 0);
        vc(bp(m(self, field)), 49, value);
    }
}

}  // namespace

// The constructor (the parent, joining an existing game): the server options 0x6c0490 (initialised from the
// player's name unless joining), the multiplayer game types into a name list (the combo box +0x13c), the maps of
// "Levels/Multiplayer/*.zax" (each read as a CLayer up to its "Tree List", +0x180) into a list box with a header
// of three columns, the name, the player count, the limits with their check boxes, Ok and Cancel
BYTE *THISCALL ServerDialog_Ctor(BYTE *self, DWORD parent, BYTE joining)   // 0x4d3080
{
    dialog_ctor(self, parent, c_str(glob_addr(0x6c04d8)));
    m(self, 0x184) = 0, m(self, 0x188) = 0, m(self, 0x18c) = 0;
    m(self, 0x180) = vt(0x5f7b88);
    mb(self, 0x1a0) = joining;
    m(self, 0) = vt(0x5f7a54);
    fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self);
    m(self, 0x19c) = glob_addr(0x6c0490);
    if (!mb(self, 0x1a0))
        fn<void(THISCALL *)(DWORD)>(0x4d2c50)(glob_addr(0x6c0490));
    const DWORD lists[2] = {0x194, 0x198};
    for (DWORD off : lists) {
        BYTE *list = alloc(0x14);
        if (list) {
            fn<void(THISCALL *)(BYTE *)>(0x592200)(list);
            m(list, 0) = vt(0x5f0240);
        }
        m(self, off) = pv(list);
    }
    for (DWORD i = 0; i < gd(glob_addr(GAME_TYPE_COUNT)); i++) {
        const DWORD type = game_type(i);
        if (!(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(type, fn<DWORD(__cdecl *)()>(0x51a270)()))
            continue;
        const DWORD again = game_type(i);
        const DWORD name = fn<DWORD(THISCALL *)(DWORD)>(0x5007a0)(again + 8);
        fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x5922b0)(m(self, 0x198), name, i);
    }
    BYTE *w = alloc(0x118);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x408cd0)(w, self, m(self, 0x198), 999999);
    m(self, 0x13c) = pv(w);
    add_child(self, w, 1, 0, NOWHERE, NOWHERE, 0);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x408bc0)(m(self, 0x13c), 2, 1);
    w = alloc(0x140);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x4020d0)(w, self);
    m(self, 0x140) = pv(w);
    set_width(w, 0x1ea);
    const DWORD columns[3][2] = {{glob_addr(0x6c04dc), 0xfa}, {glob_addr(0x6c04cc), 0x96}, {glob_addr(0x6c04e8), 0x46}};
    for (const auto &c : columns)
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x402770)(m(self, 0x140), c[0], c[1], 0, 0, 0);
    BYTE *header = bp(m(self, 0x140));
    m(header, 0x124) = m(header, W_RECT + 8) - m(header, W_RECT);
    fn<void(THISCALL *)(BYTE *)>(0x402bb0)(header);
    add_child(self, bp(m(self, 0x140)), 8, 0, NOWHERE, NOWHERE, 0);
    w = alloc(0x258);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x4041f0)(w, self, m(self, 0x194), 999999, 1);
    m(self, 0x144) = pv(w);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x406aa0)(w, m(self, 0x140));
    header = bp(m(self, 0x140));
    vc(bp(m(self, 0x144)), V_SET_SIZE, m(header, W_RECT + 8) - m(header, W_RECT), 0xaa);
    add_child(self, bp(m(self, 0x144)), 8, 0, NOWHERE, NOWHERE, 0);
    ZFileSpec spec;
    fn<void(THISCALL *)(void *)>(0x4a09f0)(&spec);
    fn<void(THISCALL *)(void *, const char *, const char *)>(0x4a0a60)(&spec, glob<const char *>(0x615d24), glob<const char *>(0x615d38));
    BYTE *files = op_new(0x14);
    if (files)
        files = fn<BYTE *(THISCALL *)(BYTE *)>(0x4a3970)(files);
    m(self, 0x190) = pv(files);
    const DWORD n = fn<DWORD(THISCALL *)(BYTE *, void *)>(0x4a3ae0)(files, &spec);
    for (DWORD i = 0; i < n; i++) {
        DWORD name;
        fn<void(THISCALL *)(DWORD, void *, DWORD)>(0x4a3b60)(m(self, 0x190), &name, i);
        BYTE *layer = alloc(0x1d0);
        if (layer)
            layer = fn<BYTE *(THISCALL *)(BYTE *)>(0x4e9da0)(layer);
        BYTE *object = layer + 0x3c;
        gd(glob_addr(0x62e68c)) = pv(object);
        gd(glob_addr(0x62e690)) = glob_addr(0x615d18);
        fn<void(THISCALL *)(BYTE *, const char *, DWORD)>(0x416a00)(object, c_str(&name), 0);
        gd(glob_addr(0x62e68c)) = 0;
        gd(glob_addr(0x62e690)) = 0;
        vc(self + 0x180, 2, &layer);
        zs_dtor(&name);
    }
    w = new_label_z(self, glob_addr(0x6c04f8));
    m(self, 0x134) = pv(w);
    add_child(self, w, 8, 0, NOWHERE, NOWHERE, 0);
    BYTE *name_label = bp(m(self, 0x134));
    w = new_edit(self, glob<const char *>(EMPTY_TEXT));
    m(self, 0x138) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    mb(bp(m(self, 0x138)), 0x111) = 0xc8;
    BYTE *edit = bp(m(self, 0x138));
    vc(edit, SET_TITLE, c_str(m(self, 0x19c)));
    w = new_label_z(self, glob_addr(0x6c04e0));
    m(self, 0x148) = pv(w);
    add_child(self, w, 7, pv(name_label), NOWHERE, NOWHERE, 0);
    BYTE *players_label = bp(m(self, 0x148));
    w = new_number_field(self);
    m(self, 0x14c) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x14c)), 49, 8);
    BYTE *first = bp(m(self, 0x134));
    int width = mi(first, W_RECT + 8) - mi(first, W_RECT);
    const int *r = vc<const int *>(bp(m(self, 0x148)), V_GET_RECT);
    set_left(bp(m(self, 0x134)), r[2] - width);
    set_left(bp(m(self, 0x138)), mi(bp(m(self, 0x14c)), W_RECT));
    const int left = mi(bp(m(self, 0x138)), W_RECT);
    r = vc<const int *>(bp(m(self, 0x144)), V_GET_RECT);
    set_width(bp(m(self, 0x138)), r[2] - left);
    w = new_label_z(self, glob_addr(0x6c0500));
    m(self, 0x150) = pv(w);
    add_child(self, w, 7, pv(players_label), NOWHERE, NOWHERE, 0);
    BYTE *p = bp(m(self, 0x148));
    set_width(bp(m(self, 0x150)), mi(p, W_RECT + 8) - mi(p, W_RECT));
    vc(bp(m(self, 0x150)), SET_VALUE, 2);
    BYTE *rel = bp(m(self, 0x150));
    w = new_number_field(self);
    m(self, 0x154) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    unlimited(bp(m(self, 0x154)));
    w = new_check_box(self, glob_addr(0x6c04ec));
    m(self, 0x158) = pv(w);
    add_child(self, w, 0x10, 0, NOWHERE, NOWHERE, 0);
    limit(self, 0x158, 0x154, m(bp(m(self, 0x19c)), 0x14));
    w = new_label_z(self, glob_addr(0x6c04b4));
    m(self, 0x15c) = pv(w);
    add_child(self, w, 9, pv(rel), NOWHERE, NOWHERE, 0);
    rel = bp(m(self, 0x15c));
    w = new_number_field(self);
    m(self, 0x160) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    w = new_label_z(self, glob_addr(0x6c04e4));
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    unlimited(bp(m(self, 0x160)));
    w = new_check_box(self, glob_addr(0x6c04ec));
    m(self, 0x164) = pv(w);
    add_child(self, w, 0x10, 0, NOWHERE, NOWHERE, 0);
    limit(self, 0x164, 0x160, m(bp(m(self, 0x19c)), 0x10));
    w = new_label_z(self, glob_addr(0x6c04f4));
    m(self, 0x170) = pv(w);
    add_child(self, w, 9, pv(rel), NOWHERE, NOWHERE, 0);
    rel = bp(m(self, 0x170));
    w = new_check_box(self, glob_addr(0x6c04f0));
    m(self, 0x174) = pv(w);
    add_child(self, w, 0x10, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x174)), 78, (DWORD)mb(bp(m(self, 0x19c)), 0x19));
    w = new_label_z(self, glob_addr(0x6c04c8));
    m(self, 0x168) = pv(w);
    add_child(self, w, 9, pv(rel), NOWHERE, NOWHERE, 0);
    w = new_check_box(self, glob_addr(0x6c04fc));
    m(self, 0x16c) = pv(w);
    add_child(self, w, 0x10, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x16c)), 78, (DWORD)mb(bp(m(self, 0x19c)), 0x18));
    w = new_button_z(self, glob_addr(0x6c04bc), 0);
    m(self, 0x178) = pv(w);
    add_child(self, w, 0xc, 0, (DWORD)-0x14, 8, 0);
    w = new_button_z(self, glob_addr(0x6c04b0), 0);
    m(self, 0x17c) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    m(self, 0x120) = m(self, 0x17c);
    m(self, 0x11c) = m(self, 0x178);
    fn<void(THISCALL *)(BYTE *)>(0x4d3c80)(self);
    fn<void(THISCALL *)(BYTE *)>(0x4d3e20)(self);
    for (int k = 4; k >= 0; k--)
        zs_dtor(&spec.ext[k]);
    return self;
}
// The destructor: the file name list (slot 3), the two name lists, the layers (slot 1 with 1) and their array
void THISCALL ServerDialog_Dtor(BYTE *self)   // 0x4d3b80
{
    m(self, 0) = vt(0x5f7a54);
    if (BYTE *files = bp(m(self, 0x190))) {
        vc(files, 3, 1);
        m(self, 0x190) = 0;
    }
    const DWORD lists[2] = {0x194, 0x198};
    for (DWORD off : lists)
        if (BYTE *list = bp(m(self, off))) {
            vc(list, 0, 1);
            m(self, off) = 0;
        }
    array_delete_all(self + 0x180, [](DWORD e) { vc(bp(e), 1, 1); });
    array_free_pool(self + 0x180, vt(0x5f7b94));
    dialog_dtor(self);
}
BYTE *THISCALL ServerDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x4d3b00
{
    fn<void(THISCALL *)(BYTE *)>(0x4d3b80)(self);
    if (flags & 1)
        pool_delete(self, 0x1a4);
    return self;
}
// The chosen game type (the combo box's id through the name list) into the options and its title into the
// label +0x150 (0x6c04d0); the maps of that type (a bit of the type in +0x9c of the layer) into the map list,
// each "%s (%d-%d)"-like with its name, minimum and maximum players (16 without a maximum); the type's slot 37
// decides whether the check box +0x174 is enabled
void THISCALL ServerDialog_FillGameTypes(BYTE *self)   // 0x4d3c80
{
    const DWORD id = vc(bp(m(self, 0x13c)), SET_VALUE);
    const DWORD type = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x592310)(m(self, 0x198), id);
    const DWORD bit = 1u << (type & 31);
    BYTE *def = nullptr;
    if (DWORD recs = gd(glob_addr(GAME_TYPE_RECORDS))) {
        const DWORD i = type < gd(glob_addr(GAME_TYPE_COUNT)) ? type : 0;
        const DWORD rec = i << 7;
        if (!gb(recs + rec + 0x7f)) {
            if (!m(bp(recs), rec + 0x78)) {
                fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(glob_addr(GAME_TYPE_LIST), i);
                recs = gd(glob_addr(GAME_TYPE_RECORDS));
            }
            def = bp(m(bp(recs), rec + 0x78));
        }
    }
    m(bp(m(self, 0x19c)), 4) = type;
    DWORD title;
    zs_ctor(&title);
    const DWORD name = vc(def, 45);
    const char *fmt = c_str(glob_addr(0x6c04d0));
    zs_format(&title, fmt, c_str(name));
    BYTE *label = bp(m(self, 0x150));
    vc(label, SET_TITLE, c_str(&title));
    vc(bp(m(self, 0x194)), 17);
    DWORD text;
    zs_ctor(&text);
    const DWORD n = m(self, 0x188);
    for (DWORD i = 0; i < n; i++) {
        BYTE *layer = bp(m(bp(m(self, 0x184)), i * 4));
        if (!(m(layer, 0x9c) & bit))
            continue;
        const DWORD most = m(layer, 0x94) ? m(layer, 0x94) : 0x10;
        const DWORD map = fn<DWORD(THISCALL *)(BYTE *)>(0x5007a0)(layer + 0xc0);
        zs_format(&text, glob<const char *>(0x615d3c), map, m(layer, 0x90), most);
        fn<void(THISCALL *)(DWORD, const char *, DWORD)>(0x5922b0)(m(self, 0x194), c_str(&text), i);
    }
    fn<void(THISCALL *)(DWORD, DWORD)>(0x406890)(m(self, 0x144), m(self, 0x194));
    mb(bp(m(self, 0x174)), W_ENABLED) = (BYTE)vc(def, 37);
    zs_dtor(&text);
    zs_dtor(&title);
}
// The chosen map's file (without "Levels/") into the options (+8) and the player count's range 2 .. the map's
// maximum (16 without one; the field's value clamped into it)
void THISCALL ServerDialog_FillMaps(BYTE *self)   // 0x4d3e20
{
    int most = 0x10;
    const DWORD sel = vc(bp(m(self, 0x144)), VALUE);
    if (sel != 0xffffffff) {
        DWORD file;
        const DWORD i = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x592310)(m(self, 0x194), sel);
        const DWORD z = fn<DWORD(THISCALL *)(DWORD, void *, DWORD)>(0x4a3b60)(m(self, 0x190), &file, i);
        const char *path = c_str(z);
        const char *prefix = glob<const char *>(0x615d48);   // "Levels/"
        DWORD len = 0;
        while (prefix[len])
            len++;
        fn<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(m(self, 0x19c) + 8, path + len);
        zs_dtor(&file);
        const DWORD k = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x592310)(m(self, 0x194), sel);
        if (const DWORD max = m(bp(m(bp(m(self, 0x184)), k * 4)), 0x94))
            most = (int)max;
    }
    int v = x87_ftol(vc<long double>(bp(m(self, 0x14c)), 48));
    if (v < 2)
        v = 2;
    else if (v > most)
        v = most;
    vc(bp(m(self, 0x14c)), 49, v);
    volatile float f = (float)most;
    BYTE *field = bp(m(self, 0x14c));
    m<float>(field, 0x110) = f;
    m(field, 0x10c) = FLOAT_2;
    m(field, 0x114) = FLOAT_1;
    BYTE *e = bp(m(field, 0x100));
    m<float>(e, 0x118) = f;
    m(e, 0x114) = FLOAT_2;
}
// Ok: without a chosen map a message box (0x6c04b8, 0x6c04ac); else the name, the two switches and the player
// count into the options, the name saved (0x4d2ca0) unless joining, the world's object's slot 6, the player
// options loaded again and the game started (Game_StartFromMenu 0x5a2940); a failure shows "Could not create
// server"
void THISCALL ServerDialog_OnOk(BYTE *self)   // 0x4d3f90
{
    if (vc(bp(m(self, 0x144)), VALUE) == 0xffffffff) {
        const char *title = c_str(glob_addr(0x6c04b8));
        fn<void(THISCALL *)(BYTE *, const char *, const char *)>(0x40c740)(self, title, c_str(glob_addr(0x6c04ac)));
        return;
    }
    fn<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(m(self, 0x19c), vc<const char *>(bp(m(self, 0x138)), TITLE));
    mb(bp(m(self, 0x19c)), 0x18) = vc(bp(m(self, 0x16c)), 79) != 0;
    mb(bp(m(self, 0x19c)), 0x19) = vc(bp(m(self, 0x174)), 79) != 0;
    m(bp(m(self, 0x19c)), 0xc) = (DWORD)x87_ftol(vc<long double>(bp(m(self, 0x14c)), 48));
    if (!mb(self, 0x1a0))
        fn<void(THISCALL *)(DWORD)>(0x4d2ca0)(m(self, 0x19c));
    vc(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 1), 6, 0);
    if (!mb(self, 0x1a0))
        fn<void(THISCALL *)(DWORD)>(0x46ce50)(glob_addr(PLAYER_OPTIONS));
    if ((BYTE)fn<DWORD(__fastcall *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x5a2940)(self, 0, 0, glob_addr(PLAYER_OPTIONS), m(self, 0x19c)))
        return;
    fn<void(THISCALL *)(BYTE *, const char *, const char *)>(0x40c740)(self, glob<const char *>(0x615da8), glob<const char *>(0x615db8));
}

// ---- The model preview window and the question dialog ----------------------------------------------------------

// The model preview (0x18 bytes, vtable 0x5fae90; a pair 0x4d4e80 of the model list and the index): +0xc the
// entity made from the model, +0x10 the model's index, +0x14 the seconds to the next animation (1 .. 6)
BYTE *THISCALL ModelPreview_Make(BYTE *self, BYTE *list, DWORD index)   // 0x5179b0
{
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4d4e80)(self, list, index);
    m(self, 0) = vt(0x5fae90);
    m(self, 0x10) = index;
    const DWORD models = vc(list, 5);
    const DWORD animated = fn<DWORD(__cdecl *)()>(0x48dcc0)();
    m(self, 0xc) = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, int)>(0x5176f0)(models, m(self, 0x10), animated, 0, 0, -1);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x517a80)(self, 1);
    if ((BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(m(self, 0xc), fn<DWORD(__cdecl *)()>(0x48dcc0)())) {
        BYTE *e = bp(m(self, 0xc));
        volatile float angle = (float)fn<long double(THISCALL *)(DWORD, DWORD, DWORD)>(0x55c550)(glob_addr(0x712cc0), 0, 0x40c90fdb);
        vc(e, 104, angle, 1);
    }
    return self;
}
// The deleting destructor: the entity deleted (slot 1 with 1), the pair's vtable, operator delete
BYTE *THISCALL ModelPreview_DeletingDtor(BYTE *self, DWORD flags)   // 0x517a40
{
    m(self, 0) = vt(0x5fae90);
    if (BYTE *e = bp(m(self, 0xc))) {
        vc(e, 1, 1);
        m(self, 0xc) = 0;
    }
    m(self, 0) = vt(0x5f7c84);
    if (flags & 1)
        op_delete(self);
    return self;
}
// The animation (first: the first of "Walk", "Run Energy", "Switching", "Flame", "Active", "Deploy", "Switch",
// "Opening", "Closing", "Switched" the model has; later up to ten random picks among it, "Run", "Shoot1",
// "Idle" (when it has more than one frame), the last pick and "Walk" until one differs from the current
// sequence); the next change in 1 .. 6 seconds
void THISCALL ModelPreview_SetModel(BYTE *self, BYTE first)   // 0x517a80
{
    volatile float wait = (float)fn<long double(THISCALL *)(DWORD, DWORD, DWORD)>(0x55c550)(glob_addr(0x712cc0), 0x3f800000, 0x40c00000);
    m<float>(self, 0x14) = wait;
    if (!(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(m(self, 0xc), fn<DWORD(__cdecl *)()>(0x48dcc0)()))
        return;
    BYTE *e = bp(m(self, 0xc));
    const DWORD model = fn<DWORD(THISCALL *)(BYTE *)>(0x48b290)(e);
    auto find = [&](DWORD name) { return (int)fn<DWORD(THISCALL *)(DWORD, const char *)>(0x517240)(model, at<const char *>(name)); };
    const DWORD NAMES[] = {glob_addr(0x60c518), glob_addr(0x61bc54), glob_addr(0x61479c), glob_addr(0x61bc4c), glob_addr(0x612d28), glob_addr(0x61bc44), glob_addr(0x6147a8), glob_addr(0x611fa0), glob_addr(0x611fb0), glob_addr(0x6147b0)};
    int seq = -1;
    for (DWORD name : NAMES) {
        seq = find(name);
        if (seq != -1)
            break;
    }
    if (!first) {
        const int current = (int)fn<DWORD(THISCALL *)(BYTE *)>(0x48aed0)(e);
        int pick = -1;
        for (int k = 0; k < 10; k++) {
            switch (fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x55c470)(glob_addr(0x712cc0), 5)) {
            case 0: pick = seq; break;
            case 1: pick = find(glob_addr(0x60c520)); break;   // "Run"
            case 2: pick = find(glob_addr(0x615f8c)); break;   // "Shoot1"
            case 3:
                pick = find(glob_addr(0x60f8d0));   // "Idle"
                if (pick == -1 || vc(fn<BYTE *(THISCALL *)(DWORD, int)>(0x5173f0)(model, pick), 25) <= 1)
                    pick = seq;
                break;
            case 4: break;
            case 5: pick = find(glob_addr(0x60c518)); break;   // "Walk"
            default: break;
            }
            if (pick != -1 && pick != current)
                break;
        }
        if (pick != -1)
            seq = pick;
    }
    if (seq == -1)
        return;
    const DWORD name = fn<DWORD(THISCALL *)(DWORD, int)>(0x517200)(model, seq);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x48f700)(e, name);
}

// A question dialog (0x140, vtable 0x5faba8, a CDialog without a title): the parent, a value kept at +0x13c, the
// text, the two buttons' texts (+0x134 at the left border, +0x138 the default)
BYTE *THISCALL QuestionDialog_Ctor(BYTE *self, DWORD parent, DWORD value, const char *text, const char *yes, const char *no)   // 0x5156e0
{
    dialog_ctor(self, parent, nullptr);
    m(self, 0) = vt(0x5faba8);
    BYTE *w = new_label(self, text);
    add_child(self, w, 1, 0, NOWHERE, NOWHERE, 0);
    w = new_button(self, yes, 0);
    m(self, 0x134) = pv(w);
    add_child(self, w, 0xc, 0, NOWHERE, NOWHERE, 0);
    BYTE *b = bp(m(self, 0x134));
    const DWORD fifths = (m(bp(m(self, W_FONT)), 0x80) / 5) << 1;
    set_left(b, (int)fifths - (mi(b, W_RECT + 8) - mi(b, W_RECT)) / 2 + mi(b, W_RECT));
    w = new_button(self, no, 0);
    m(self, 0x138) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    focus(self, bp(m(self, 0x138)));
    m(self, 0x11c) = m(self, 0x138);
    m(self, 0x120) = m(self, 0x138);
    m(self, 0x13c) = value;
    return self;
}

// ---- The server browser (GameSpy's server list) -----------------------------------------------------------

namespace {

constexpr DWORD SERVER_KEY[2] = {0x6e41334a, 0x00007333};   // "J3An3s"
DWORD server_list_new(BYTE *self)   // ServerListNew 0x5c6060 ("Zax", "zax", the key, 10, the callback 0x519870, 1, self)
{
    DWORD key[2] = {SERVER_KEY[0], SERVER_KEY[1]};
    return fn<DWORD(__cdecl *)(DWORD, DWORD, void *, DWORD, DWORD, DWORD, BYTE *)>(0x5c6060)(glob_addr(0x61be18), glob_addr(0x615d38), key, 10, fn_addr(0x519870), 1, self);
}
void server_list_think(DWORD list) { fn<void(__cdecl *)(DWORD)>(0x5c74c0)(list); }
const char *server_text(DWORD server, DWORD key, DWORD missing) { return fn<const char *(__cdecl *)(DWORD, DWORD, DWORD)>(0x5c8080)(server, key, missing); }
int server_int(DWORD server, DWORD key) { return fn<int(__cdecl *)(DWORD, DWORD, DWORD)>(0x5c8130)(server, key, 0); }
void message_box(BYTE *self, const char *title, const char *text) { fn<void(THISCALL *)(BYTE *, const char *, const char *)>(0x40c740)(self, title, text); }

}  // namespace

// The constructor (the parent): the network type (a combo box over "Internet"/"LAN" names, +0x150; the INI
// file's "Prefered Network Type"), the buttons, the status label "XXXXXXXX", a header of six columns over the
// server list box (+0x17c the names: the server's text and the server), the GameSpy logo, the server list
// (+0x190) refreshed
BYTE *THISCALL ServerBrowser_Ctor(BYTE *self, DWORD parent)   // 0x518e00
{
    dialog_ctor(self, parent, c_str(glob_addr(0x6cff74)));
    fn<void(THISCALL *)(BYTE *)>(0x592200)(self + 0x150);
    m(self, 0x150) = vt(0x5f0240);
    fn<void(THISCALL *)(BYTE *)>(0x592200)(self + 0x17c);
    m(self, 0x17c) = vt(0x5f0240);
    zs_ctor(self + 0x198);
    m(self, 0) = vt(0x5fafd0);
    fn<void(__fastcall *)(DWORD)>(0x4a2650)(glob_addr(0x619510));
    fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self);
    mb(self, 0x195) = 0;
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40c600)(self, 0);
    m(self, 0x19c) = 0;
    zs_assign(self + 0x198, glob_addr(0x6cffb4));
    mb(self, 0x178) = 0;
    m(self, 0x174) = 0;
    mb(self, 0x194) = 0;
    fn<void(THISCALL *)(BYTE *, const char *, DWORD)>(0x5922b0)(self + 0x150, c_str(glob_addr(0x6cffa0)), 0);
    fn<void(THISCALL *)(BYTE *, const char *, DWORD)>(0x5922b0)(self + 0x150, c_str(glob_addr(0x6cffc0)), 1);
    BYTE *w = alloc(0x118);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *, DWORD)>(0x408cd0)(w, self, self + 0x150, 999999);
    m(self, 0x14c) = pv(w);
    add_child(self, w, 1, 0, NOWHERE, NOWHERE, 0);
    const int type = fn<int(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4670)(glob_addr(INI), glob_addr(0x60fa48), glob_addr(0x61be28), 0);
    mb(self, 0x194) = type == 1;
    const DWORD id = fn<DWORD(THISCALL *)(BYTE *, int)>(0x592320)(self + 0x150, type);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x408bc0)(m(self, 0x14c), id, 1);
    w = new_button_z(self, glob_addr(0x6cff88), 0);
    m(self, 0x13c) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x13c)), 40, 2);
    w = new_button_z(self, glob_addr(0x6cff8c), 0);
    m(self, 0x140) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    w = new_label_z(self, glob_addr(0x6cff98));
    m(self, 0x144) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    w = new_label(self, glob<const char *>(0x61be1c));
    m(self, 0x148) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    w = alloc(0x140);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x4020d0)(w, self);
    m(self, 0x134) = pv(w);
    set_width(w, 0x23f);
    const DWORD columns[6][4] = {{glob_addr(0x6cff80), 0x26, 2, 1}, {glob_addr(0x6cffd8), 0xd2, 0, 0}, {glob_addr(0x6cff7c), 0x34, 1, 1},
                                 {glob_addr(0x6cff6c), 0x6e, 0, 0}, {glob_addr(0x6cffa4), 0x6e, 0, 0}, {glob_addr(0x6cffd4), 0x50, 0, 0}};
    for (const auto &c : columns)
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x402770)(m(self, 0x134), c[0], c[1], 0, c[2], c[3]);
    BYTE *header = bp(m(self, 0x134));
    m(header, 0x124) = m(header, W_RECT + 8) - m(header, W_RECT);
    fn<void(THISCALL *)(BYTE *)>(0x402bb0)(header);
    add_child(self, bp(m(self, 0x134)), 7, m(self, 0x14c), NOWHERE, NOWHERE, 0);
    w = alloc(0x258);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *, DWORD, DWORD)>(0x4041f0)(w, self, self + 0x17c, 999999, 1);
    m(self, 0x138) = pv(w);
    header = bp(m(self, 0x134));
    vc(w, V_SET_SIZE, m(header, W_RECT + 8) - m(header, W_RECT), 0x12c);
    add_child(self, bp(m(self, 0x138)), 8, 0, 0, 0, 0);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x406aa0)(m(self, 0x138), m(self, 0x134));
    w = new_button_z(self, glob_addr(0x6cffa8), 0);
    m(self, 0x16c) = pv(w);
    add_child(self, w, 7, m(self, 0x138), NOWHERE, NOWHERE, 0);
    w = new_button_z(self, glob_addr(0x6cff84), 0);
    m(self, 0x168) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    w = new_button_z(self, glob_addr(0x6cff90), 0);
    m(self, 0x164) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    mb(bp(m(self, 0x164)), W_ENABLED) = 0;
    w = new_button_z(self, glob_addr(0x6cff78), 0);
    m(self, 0x170) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    m(self, 0x120) = m(self, 0x100);
    m(self, 0x190) = server_list_new(self);
    w = alloc(0x10c);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x56c400)(w, self, glob<const char *>(0x61be08), 0);
    add_child(self, w, 7, m(self, 0x16c), NOWHERE, NOWHERE, 0);
    m(self, 0x120) = m(self, 0x170);
    m(self, 0x11c) = m(self, 0x164);
    fn<void(THISCALL *)(BYTE *)>(0x519cf0)(self);
    fn<void(__cdecl *)()>(0x4a2670)();
    return self;
}
BYTE *THISCALL ServerBrowser_DeletingDtor(BYTE *self, DWORD flags)   // 0x519360
{
    m(self, 0) = vt(0x5fafd0);
    fn<void(__cdecl *)(DWORD)>(0x5c6180)(m(self, 0x190));
    zs_dtor(self + 0x198);
    fn<void(THISCALL *)(BYTE *)>(0x592220)(self + 0x17c);
    fn<void(THISCALL *)(BYTE *)>(0x592220)(self + 0x150);
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x1a0);
    return self;
}
// Slot 2 (the seconds): the server list's think, the window's update; once a second the list box takes the
// names again when they changed (+0x178)
void THISCALL ServerBrowser_Update(BYTE *self, float dt)   // 0x5193c0
{
    server_list_think(m(self, 0x190));
    fn<void(THISCALL *)(BYTE *, float)>(0x40b5d0)(self, dt);
    const long double left = (long double)m<float>(self, 0x174) - (long double)dt;
    volatile float stored = (float)left;
    m<float>(self, 0x174) = stored;
    if (!x87_lt(left, (long double)*glob<float *>(0x5ebed0)))
        return;
    m(self, 0x174) = 0x3f800000;
    if (!mb(self, 0x178))
        return;
    mb(self, 0x178) = 0;
    fn<void(THISCALL *)(DWORD, BYTE *)>(0x406890)(m(self, 0x138), self + 0x17c);
}
// Slot 4 (the frame): the frame window's drawing between two thinks of the server list
void THISCALL ServerBrowser_Draw(BYTE *self, DWORD frame)   // 0x519430
{
    server_list_think(m(self, 0x190));
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x402f30)(self, frame);
    server_list_think(m(self, 0x190));
}
// Join the chosen server: too old a build ("gamebuild" below Game_MinBuildNumber 0x5908b0) or full gives a
// message box; else the game starts with the server's "DirectPlayAddress_0" (Game_StartFromMenu 0x5a2940)
void THISCALL ServerBrowser_Join(BYTE *self)   // 0x519730
{
    const DWORD sel = vc(bp(m(self, 0x138)), VALUE);
    if (sel == 0xffffffff)
        return;
    BYTE *names = self + 0x17c;
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x592310)(names, sel) == 0xffffffff)
        return;
    const DWORD server = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x592310)(names, sel);
    const int build = server_int(server, glob_addr(0x61be68));
    if (build < fn<int(__cdecl *)()>(0x5908b0)()) {
        const char *title = c_str(glob_addr(0x6cff9c));
        message_box(self, title, c_str(glob_addr(0x6cffbc)));
        return;
    }
    const int players = server_int(server, glob_addr(0x61158c));
    if (players >= server_int(server, glob_addr(0x611580))) {
        const char *title = c_str(glob_addr(0x6cff9c));
        message_box(self, title, c_str(glob_addr(0x6cffc4)));
        return;
    }
    DWORD address;
    zs_ctor_text(&address, server_text(server, glob_addr(0x61be54), glob_addr(EMPTY_TEXT)));
    fn<void(THISCALL *)(DWORD)>(0x46ce50)(glob_addr(PLAYER_OPTIONS));
    if (!(BYTE)fn<DWORD(__fastcall *)(BYTE *, const char *, DWORD, DWORD, DWORD)>(0x5a2940)(self, c_str(&address), 0, glob_addr(PLAYER_OPTIONS), 0)) {
        const char *title = c_str(glob_addr(0x6cff70));
        message_box(self, title, c_str(glob_addr(0x6cff9c)));
    }
    zs_dtor(&address);
}
// A server of the list (the GameSpy callback's): its line "%d %s %d %d %s %s %s" (ping, host name, players,
// maximum, game type, map, version) appended to the names (the placeholder line removed first)
void THISCALL ServerBrowser_AddServer(BYTE *self, DWORD server)   // 0x519b60
{
    DWORD host, map, type, version, line;
    zs_ctor_text(&host, server_text(server, glob_addr(0x611574), glob_addr(0x61bebc)));
    const DWORD ping = fn<DWORD(__cdecl *)(DWORD)>(0x5c8050)(server);
    const int players = server_int(server, glob_addr(0x61158c));
    const int most = server_int(server, glob_addr(0x611580));
    zs_ctor_text(&map, server_text(server, glob_addr(0x61bea8), glob_addr(0x61beb0)));
    zs_ctor_text(&type, server_text(server, glob_addr(0x61be9c), glob_addr(EMPTY_TEXT)));
    zs_ctor_text(&version, server_text(server, glob_addr(0x61be94), glob_addr(EMPTY_TEXT)));
    zs_ctor(&line);
    const char *m_ = c_str(&map);
    const char *t_ = c_str(&type);
    const char *h_ = c_str(&host);
    const char *v_ = c_str(&version);
    zs_format(&line, glob<const char *>(0x61be7c), ping, h_, players, most, t_, m_, v_);
    BYTE *names = self + 0x17c;
    if (vc(names, 22) == 1 && fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x592310)(names, 0) == 0xffffffff)
        vc(names, 17);
    fn<void(THISCALL *)(BYTE *, const char *, DWORD)>(0x5922b0)(names, c_str(&line), server);
    mb(self, 0x178) = 1;
    zs_dtor(&line);
    zs_dtor(&version);
    zs_dtor(&type);
    zs_dtor(&map);
    zs_dtor(&host);
}
// Refresh: Join and the second button off, the selection cleared, the status text (with the progress " (%d%%)"
// between 0 and 100), a new server list, the names emptied but for the placeholder line, the list's update
// (on the LAN: ports 0x68aa .. 0x68b4)
void THISCALL ServerBrowser_Refresh(BYTE *self)   // 0x519cf0
{
    mb(self, 0x195) = 1;
    mb(bp(m(self, 0x13c)), W_ENABLED) = 0;
    mb(bp(m(self, 0x140)), W_ENABLED) = mb(self, 0x195);
    fn<void(THISCALL *)(DWORD, int, DWORD)>(0x407270)(m(self, 0x138), -1, 0);
    m(self, 0x19c) = 0;
    zs_assign(self + 0x198, glob_addr(0x6cff94));
    DWORD status;
    zs_copy(&status, pv(self + 0x198));
    if (m(self, 0x19c) && m(self, 0x19c) != 100)
        fn<void(__cdecl *)(void *, DWORD, DWORD)>(0x4e0a30)(&status, glob_addr(0x61be74), m(self, 0x19c));
    BYTE *label = bp(m(self, 0x148));
    vc(label, SET_TITLE, c_str(&status));
    zs_dtor(&status);
    fn<void(__cdecl *)(DWORD)>(0x5c6180)(m(self, 0x190));
    m(self, 0x190) = server_list_new(self);
    BYTE *names = self + 0x17c;
    vc(names, 17);
    DWORD line;
    zs_ctor(&line);
    zs_format(&line, glob<const char *>(0x61bec4), c_str(glob_addr(0x6cffb8)));
    fn<void(THISCALL *)(BYTE *, const char *, int)>(0x5922b0)(names, c_str(&line), -1);
    zs_dtor(&line);
    fn<void(THISCALL *)(DWORD, BYTE *)>(0x406890)(m(self, 0x138), names);
    fn<DWORD(__cdecl *)(DWORD)>(0x5c7870)(m(self, 0x190));
    if (mb(self, 0x194))
        fn<void(__cdecl *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x5c64b0)(m(self, 0x190), 1, 0x68aa, 0x68b4, 1);
    else
        fn<void(__cdecl *)(DWORD, DWORD)>(0x5c61c0)(m(self, 0x190), 1);
}
// A line of text (a ZString) as a name without a server
void THISCALL ServerBrowser_AddText(BYTE *self, DWORD text)   // 0x519ea0
{
    DWORD line;
    zs_ctor(&line);
    zs_format(&line, glob<const char *>(0x61bec4), c_str(text));
    fn<void(THISCALL *)(BYTE *, const char *, int)>(0x5922b0)(self + 0x17c, c_str(&line), -1);
    zs_dtor(&line);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_dialogs)
{
    const Hook list[] = {
#include "ui_dialogs_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
