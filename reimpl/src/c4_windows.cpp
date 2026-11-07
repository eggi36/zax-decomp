// Batch C4 (phase 5a), third part: the window slots that phase 4 left open (docs/DECOMPILATION.md 10,
// "Batch C4"): the inventory screen's key and notify slots, the item list window's draw, key, mouse-down
// and mouse-drag slots, the inventory display page's images and button notify. Layouts as in
// ui_inventory.cpp; every call into the toolkit or the rest of the game goes through its exe address.
//
// Return values are what the original leaves in eax (where the original sets only al, the check compares al).
#include "ui_screens.h"
#include "x87.h"

namespace zax::ui {

namespace {

BYTE *inventory_display() { return fn<BYTE *(THISCALL *)(DWORD)>(0x5a1d40)(custom_rules()); }
BYTE *player_inventory() { return fn<BYTE *(__cdecl *)()>(0x428b10)(); }
DWORD item_in_order(BYTE *inv, DWORD i) { return fn<DWORD(__fastcall *)(BYTE *, DWORD)>(0x5b2aa0)(inv, i); }
DWORD item_in_focus(BYTE *inv) { return fn<DWORD(THISCALL *)(BYTE *)>(0x426070)(inv); }
DWORD show_item(BYTE *list, DWORD i) { return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4da780)(list, i); }
DWORD close_inventory() { return fn<DWORD(THISCALL *)(DWORD)>(0x5a07e0)(gd(glob_addr(G_APP))); }
DWORD first_character() { return fn<DWORD(THISCALL *)(DWORD)>(0x4f5d80)(gd(glob_addr(G_APP))); }

// The position of the item in focus in category order (0 when none matches); the inventory and the item
// asked again on every step as the original does
DWORD focus_position()
{
    BYTE *inv = player_inventory();
    for (DWORD i = 0; i < m(inv, 0x20); i++) {
        const DWORD item = item_in_order(player_inventory(), i);
        if (item == item_in_focus(inv))
            return i;
    }
    return 0;
}

}  // namespace

// ---- The inventory screen ------------------------------------------------------------------------------------

// 0x429cd0, slot 16 (a key): the window's own keys first; Escape closes the panel while it is open or sliding in
DWORD THISCALL InventoryScreen_Key(BYTE *self, DWORD key)
{
    if (!fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key)) {
        if (key != 0x1b)
            return 0;
        const DWORD state = m(self, 0x100);
        if (state != 2 && state != 1)
            return 0;
        close_inventory();
    }
    return 1;
}

// 0x429d20, slot 21 (notify): the tab buttons select their page, the close button closes the panel
DWORD THISCALL InventoryScreen_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (what)
        return what;
    BYTE *tabs = bp(m(self, 0x124));
    if (from == m(self, 0x118))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40a4b0)(tabs, 0, 1);
    if (from == m(self, 0x11c))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40a4b0)(tabs, 1, 1);
    if (from == m(self, 0x120))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40a4b0)(tabs, 2, 1);
    if (from == m(self, 0x114))
        return close_inventory();
    return from;
}

// ---- The item list window -------------------------------------------------------------------------------------

// 0x4da290, slot 4 (draw): the background, the grid lines (copies of the line image +0x100 through the frame's
// slot 23), every item's image (the item definition list 0x6c0aa8 by the definition's +0x10) row by row, its
// count when above 1 (in the options' font at the options' offset), the item in focus framed twice
DWORD THISCALL ItemListWindow_Draw(BYTE *self, BYTE *frame)
{
    BYTE *opt = inventory_display() + 0x3c;
    fn<void(THISCALL *)(BYTE *, int, int, DWORD, DWORD, DWORD)>(0x4b3a70)(frame, 0, 0, m(self, W_CONTENT_W), m(self, W_CONTENT_H), glob_addr(0x6be368));
    for (int x = 0; x < mi(self, W_CONTENT_W); x += mi(opt, 0x60) + 1) {
        int dst[4] = {x, 0, x + 1, mi(self, W_CONTENT_H) - 1};
        int src[4] = {0, 0, 1, mi(self, W_CONTENT_H) - 1};
        vc(frame, 23, m(self, 0x100), src, dst, glob_addr(0x6bc4e0));
    }
    for (int y = 0; y < mi(self, W_CONTENT_H); y += mi(opt, 0x60) + 1) {
        const int w = (mi(opt, 0x60) + 1) * mi(opt, 0x5c) + 1;
        int src[4] = {0, 0, w, 1};
        int dst[4] = {0, y, w, y + 1};
        vc(frame, 23, m(self, 0x100), src, dst, glob_addr(0x6bc4e0));
    }
    const DWORD focus = focus_position();
    BYTE *inv = player_inventory();
    if (inv) {
        const DWORD n = m(inv, 0x20);
        DWORD k = 0;
        if (item_in_order(player_inventory(), 0) != 0xffffffff && n) {
            int y = 1;
            do {
                for (int x = 1; x < (mi(opt, 0x60) + 1) * mi(opt, 0x5c) + 1; x += mi(opt, 0x60) + 1) {
                    if (k >= n)
                        continue;
                    const DWORD item = vc(inv, 26, item_in_order(player_inventory(), k));
                    const DWORD def = fn<DWORD(THISCALL *)(DWORD)>(0x4dd480)(item);
                    const DWORD image = record_inline(glob_addr(0x6c0aa8), m(bp(def), 0x10));
                    fn<void(THISCALL *)(BYTE *, DWORD, int, int, DWORD)>(0x4b3280)(frame, image, x, y, glob_addr(0x6bcc68));
                    const DWORD props = fn<DWORD(THISCALL *)(DWORD)>(0x482de0)(item);
                    const int count = fn<int(__fastcall *)(BYTE *, DWORD)>(0x4268d0)(inv, props);
                    if (count > 1) {
                        const DWORD font = record_inline(glob_addr(FONTS), m(opt, 0x64));
                        fn<void(THISCALL *)(DWORD, BYTE *, int, int, int, DWORD)>(0x4a6b90)(font, frame, x + mi(opt, 0x68), y + mi(opt, 0x6c), count, 0);
                    }
                    if (k == focus) {
                        const int c = mi(opt, 0x60);
                        fn<void(THISCALL *)(BYTE *, int, int, int, int, BYTE *)>(0x4b33f0)(frame, x, y, x + c, y + c, opt + 0x98);
                        fn<void(THISCALL *)(BYTE *, int, int, int, int, BYTE *)>(0x4b33f0)(frame, x + 1, y + 1, x + c - 1, y + c - 1, opt + 0x88);
                    }
                    k++;
                }
                y += mi(opt, 0x60) + 1;
            } while (k < n);
        }
    }
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
}

// 0x4da840, slot 14 (mouse drag: point, a, buttons, b): with the left button the item under the point (cells
// of the options' size plus a line, the scroll position +0xfc added) comes into focus
DWORD THISCALL ItemListWindow_MouseDrag(BYTE *self, int *point, DWORD, DWORD buttons, DWORD)
{
    BYTE *opt = inventory_display() + 0x3c;
    if (!(buttons & 1))
        return (pv(opt) - 0x3c) & ~0xffu;
    BYTE *scroll = bp(m(self, 0xfc));
    const DWORD sx = scroll ? m(scroll, 0) : 0;
    const DWORD sy = scroll ? m(scroll, 4) : 0;
    const DWORD cell = m(opt, 0x60) + 1;
    const DWORD col = (DWORD)(point[0] - mi(self, 0x24) + (int)sx) / cell;
    const DWORD row = (DWORD)(point[1] - mi(self, 0x28) + (int)sy) / cell;
    return (show_item(self, row * m(opt, 0x5c) + col) & ~0xffu) | 1;
}

// 0x4da8f0, slot 10 (mouse down): the toolkit's
DWORD THISCALL ItemListWindow_MouseDown(BYTE *self, DWORD a, DWORD b)
{
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bcc0)(self, a, b);
}

// 0x4da910, slot 16 (a key): up (6), down (10), left (11) and right (12) move the focus by a row or a cell within
// the grid; down only while the position a row further is below the item definition list's count (0x6c0adc)
DWORD THISCALL ItemListWindow_Key(BYTE *self, DWORD key)
{
    BYTE *opt = inventory_display() + 0x3c;
    const DWORD cols = m(opt, 0x5c);
    switch (key) {
    case 6:
        if (focus_position() - 1 != 0)
            show_item(self, focus_position() - cols);
        return 1;
    case 10:
        if (cols + focus_position() < gd(glob_addr(0x6c0adc)))
            show_item(self, cols + focus_position());
        return 1;
    case 11:
        if (focus_position() % cols != 0)
            show_item(self, focus_position() - 1);
        return 1;
    case 12:
        if (focus_position() % cols != cols - 1)
            show_item(self, focus_position() + 1);
        return 1;
    default:
        return 0;
    }
}

// ---- The inventory display page --------------------------------------------------------------------------------

// 0x4dade0, slot 76: the page's images: its size from the "Inventory Diagram" sequence (+0x134), the list's line
// image ("Inventory"/"Line Fill" with the filter 0xb), the bar's colour, arrows and thumb
DWORD THISCALL InventoryDisplayPage_LoadImages(BYTE *self)
{
    const DWORD diagram = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616f30)));
    m(self, 0x134) = diagram;
    const DWORD h = vc(bp(diagram), 39);
    const DWORD w = vc(bp(diagram), 45);
    vc(self, 59, w, h);
    BYTE filter[0x14];
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b1cc0)(filter, 0);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b1db0)(filter, 0xb);
    BYTE *line = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x40);
    if (line)
        line = fn<BYTE *(THISCALL *)(BYTE *)>(0x4a8a90)(line);
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, BYTE *, DWORD, DWORD)>(0x4a9a30)(line, glob_addr(0x60bebc), glob_addr(0x616f24), filter, 1, 0);
    m(bp(m(self, 0x100)), 0x100) = pv(line);
    fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616f0c)));
    BYTE color[0x10];
    const DWORD c = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 2, 0x8f, 0x65, 0xff);
    BYTE *bar = bp(m(self, 0x104));
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x414560)(bar, c);
    mb(bp(m(self, 0x104)), 0x164) = 1;
    const DWORD up_high = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616ef8)));
    const DWORD up = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616ee4)));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x4145e0)(m(self, 0x104), up, up_high);
    const DWORD down_high = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616ecc)));
    const DWORD down = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616eb8)));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x414600)(m(self, 0x104), down, down_high);
    const DWORD thumb = record_loaded(glob_addr(0x6c22e0), find_index(glob_addr(0x6c22e0), glob_addr(0x616eb0)));
    fn<void(THISCALL *)(DWORD, DWORD)>(0x414620)(m(self, 0x104), thumb);
    return fn<DWORD(THISCALL *)(BYTE *)>(0x4b1d20)(filter);
}

// 0x4db630, slot 21 (notify): a button of the page runs the matching action of the item in focus (when the
// definition is a CZaxInventoryItemDefinition, the action's switch is set and the action exists) for the first
// character in the current layer, the error sound when it fails; then the inventory's slot 24 (the character, 0)
DWORD THISCALL InventoryDisplayPage_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (what)
        return what;
    BYTE *inv = bp(fn<DWORD(THISCALL *)(DWORD)>(0x4267e0)(first_character()));
    DWORD r = vc(inv, 26, item_in_focus(inv));
    if (r) {
        BYTE *def = bp(fn<DWORD(THISCALL *)(DWORD)>(0x4dd480)(r));
        r = pv(def);
        if (def) {
            r = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x416790)(def, fn<DWORD(__cdecl *)()>(0x5b5f10)());
            if ((BYTE)r && from) {
                // (button, switch bit, action)
                static const DWORD BUTTONS[6][3] = {{0x10c, 3, 0x8c}, {0x110, 1, 0x84}, {0x114, 0, 0x80},
                                                    {0x118, 6, 0x94}, {0x11c, 2, 0x88}, {0x120, 4, 0x90}};
                for (const auto &b : BUTTONS) {
                    if (from != m(self, b[0]) || !((m(def, 0xf4) >> b[1]) & 1))
                        continue;
                    const DWORD action = m(def, b[2]);
                    if (!action)
                        continue;
                    const DWORD character = first_character();
                    const DWORD layer = fn<DWORD(THISCALL *)(DWORD)>(0x4f1050)(gd(glob_addr(G_WORLD)));
                    r = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x41a740)(action, layer, character, 0, 0, glob_addr(0x616f44));
                    if (!(BYTE)r)
                        r = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x57e820)(glob_addr(G_SOUND_LIST), glob_addr(SOUND_ERROR));
                    break;
                }
            }
        }
    }
    if (inv)
        r = vc(inv, 24, first_character(), 0);
    return r;
}


// ---- Shared pieces of the pages and dialogs ----------------------------------------------------------------------

namespace {

constexpr DWORD CLOSE = 5, ON_CLOSE = 6, NOTIFY = 21, SET_SIZE = 59, SET_TITLE_ = 43, VALUE = 77, SET_CHECKED = 78,
                CHECKED = 79;
DWORD frame_notify_(BYTE *self, DWORD from, DWORD what) { return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4035f0)(self, from, what); }
DWORD mission_current() { return fn<DWORD(THISCALL *)(DWORD)>(0x5127e0)(vc(bp(gd(glob_addr(G_WORLD))), 98)); }
DWORD field_name(DWORD field) { return fn<DWORD(THISCALL *)(DWORD)>(0x5007a0)(field); }
DWORD draw_text(DWORD font, BYTE *frame, int x, int y, DWORD text)
{
    return fn<DWORD(THISCALL *)(DWORD, BYTE *, int, int, DWORD, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, 0, 0);
}
DWORD ini_write_int(DWORD key, DWORD value)
{
    return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4690)(glob_addr(0x6bd330), glob_addr(0x6bd430), key, value);
}
BYTE *resource_cache() { return fn<BYTE *(__cdecl *)()>(0x57aef0)(); }

// The page's size from its diagram sequence (the list's object of `name`, kept at `keep`): its slot 59 with the
// sequence's slots 45 and 39
DWORD diagram_size(BYTE *self, DWORD name, DWORD keep)
{
    const DWORD diagram = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), name));
    m(self, keep) = diagram;
    const DWORD h = vc(bp(diagram), 39);
    const DWORD w = vc(bp(diagram), 45);
    return vc(self, SET_SIZE, w, h);
}
// The image scroll bar at `bar` of a page: the colour (2, 0x8f, 0x65, 0xff), the arrows' images and the thumb
DWORD bar_images(BYTE *self, DWORD bar)
{
    BYTE color[0x10];
    const DWORD c = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 2, 0x8f, 0x65, 0xff);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x414560)(m(self, bar), c);
    mb(bp(m(self, bar)), 0x164) = 1;
    const DWORD up_high = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616ef8)));
    const DWORD up = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616ee4)));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x4145e0)(m(self, bar), up, up_high);
    const DWORD down_high = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616ecc)));
    const DWORD down = record_loaded(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x616eb8)));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x414600)(m(self, bar), down, down_high);
    const DWORD thumb = record_loaded(glob_addr(0x6c22e0), find_index(glob_addr(0x6c22e0), glob_addr(0x616eb0)));
    return fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x414620)(m(self, bar), thumb);
}
// A page's title: the field name at opt+0x18 in the font `font`, centred in the rectangle opt+8 (x, y, w, h)
DWORD draw_title(BYTE *opt, DWORD font, BYTE *frame)
{
    const DWORD w = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x4a7310)(font, field_name(pv(opt + 0x18)), 0xffffffff);
    const int x = (int)((m(opt, 0x10) - w) >> 1) + mi(opt, 8);
    const DWORD h = fn<DWORD(THISCALL *)(DWORD)>(0x4a6be0)(font);
    const int y = (int)((m(opt, 0x14) - h) >> 1) + mi(opt, 0xc);
    return draw_text(font, frame, x, y, field_name(pv(opt + 0x18)));
}
// MissionObjectives_Draw 0x509910 (fastcall: the mission, the frame; the list's +0x110, +0x10c, +0x108, 1.0, 0,
// the flag +0x104 as a byte in a dword, +0x100)
DWORD objectives_draw(DWORD mission, DWORD frame, BYTE *list, DWORD flag)
{
    return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x509910)(
        mission, frame, m(list, 0x110), m(list, 0x10c), m(list, 0x108), 0x3f800000, 0, flag, m(list, 0x100));
}

// A key button of the controls dialog refreshed (KeyButton::UpdateText 0x4710b0 inlined): its config +0x13c, its
// index +0x140, the title from the config's first INI word, the label +0x138 with the binding's text
void key_button(BYTE *b, DWORD config, DWORD i, bool inline_text)
{
    m(b, 0x13c) = config;
    m(b, 0x140) = i;
    if (config)
        vc(b, SET_TITLE_, fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x470ce0)(config, i));
    else
        vc(b, SET_TITLE_, glob_addr(EMPTY_TEXT));
    if (!inline_text) {
        if (m(b, 0x138))
            fn<DWORD(THISCALL *)(BYTE *)>(0x4710b0)(b);
        return;
    }
    if (!m(b, 0x138))
        return;
    if (!m(b, 0x13c)) {
        vc(bp(m(b, 0x138)), VALUE, glob_addr(EMPTY_TEXT));
        return;
    }
    BYTE z[4];
    zs_ctor(z);
    fn<DWORD(THISCALL *)(DWORD, DWORD, BYTE *)>(0x470b60)(m(b, 0x13c), m(b, 0x140), z);
    BYTE *label = bp(m(b, 0x138));
    vc(label, VALUE, c_str(z));
    zs_dtor(z);
}
void key_buttons(BYTE *self, bool inline_text)
{
    for (DWORD i = 0; i < m(self, 0x13c); i++)
        key_button(bp(m(bp(m(self, 0x138)), i * 4)), m(self, 0x134), i, inline_text);
}
DWORD check_joystick(BYTE *self) { return fn<DWORD(THISCALL *)(DWORD)>(0x46ffa0)(m(self, 0x134)); }

}  // namespace

// ---- The network options dialog, the controls dialogs ----------------------------------------------------------

// 0x46d520, slot 21: Ok closes, the model button clears the preview's model, the two colour sliders apply the
// colours
DWORD THISCALL NetOptionsDialog_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (!what) {
        if (from == m(self, 0x154))
            return vc(self, CLOSE);
        if (from == m(self, 0x13c))
            return fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x517a80)(m(self, 0x140), 0);
    } else if (what == 1 && (from == m(self, 0x148) || from == m(self, 0x150))) {
        return fn<DWORD(THISCALL *)(BYTE *)>(0x46d450)(self);
    }
    return frame_notify_(self, from, what);
}

// 0x471ac0, slot 4 (draw): when the joystick changed, the key buttons refreshed; then the frame window's draw
DWORD THISCALL ControlsDialog_Draw(BYTE *self, DWORD frame)
{
    if ((BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x470ac0)(m(self, 0x134))) {
        check_joystick(self);
        key_buttons(self, true);
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x402f30)(self, frame);
}

// 0x471c30, slot 21: the joystick control panel, cancel, Ok (writes the INI file when the config changed), the
// defaults, the auto switch box; a joystick chosen in the combo box or a key button pressed refreshes the buttons
DWORD THISCALL ControlsDialog_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (!what) {
        if (from == m(self, 0x150))
            return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x4d2570)(glob_addr(0x60fdac), glob_addr(0x60fdb4), 0);
        if (from == m(self, 0x14c))
            return vc(self, CLOSE);
        if (from == m(self, 0x148)) {
            if ((BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x46fb00)(m(self, 0x134)))
                fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x46fb60)(m(self, 0x134), 0);
            return vc(self, ON_CLOSE);
        }
        if (from == m(self, 0x144)) {
            fn<DWORD(THISCALL *)(DWORD)>(0x46fa10)(m(self, 0x134));
            check_joystick(self);
            key_buttons(self, false);
            return 0;
        }
        if (from == m(self, 0x154)) {
            const DWORD on = vc(bp(from), CHECKED) != 0;
            ini_write_int(glob_addr(0x60fa14), on);
            return fn<DWORD(THISCALL *)(DWORD)>(0x5abfe0)(fn<DWORD(__cdecl *)()>(0x5abe30)());
        }
    } else if (what == 1) {
        if (from == m(self, 0x164)) {
            const DWORD k = vc(bp(from), 76);
            fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x470d20)(m(self, 0x134), m(bp(m(self, 0x15c)), k * 4));
            check_joystick(self);
            key_buttons(self, true);
            return 0;
        }
        for (DWORD i = 0; i < m(self, 0x13c); i++)
            if (from == m(bp(m(self, 0x138)), i * 4)) {
                check_joystick(self);
                key_buttons(self, true);
                return 0;
            }
    }
    return frame_notify_(self, from, what);
}

// 0x472330, slot 21 (the dialog that confirms the defaults; +0x13c the controls dialog): yes closes and restores
// the controls dialog's config, then closes that one too (its slot 6); no closes
DWORD THISCALL SaveChangesDialog_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (what)
        return what;
    if (from == m(self, 0x134)) {
        BYTE *dialog = bp(m(self, 0x13c));
        vc(self, CLOSE);
        fn<DWORD(THISCALL *)(DWORD)>(0x46fad0)(m(dialog, 0x134));
        return vc(dialog, ON_CLOSE);
    }
    if (from == m(self, 0x138))
        return vc(self, CLOSE);
    return from;
}

// ---- The dialogue display ----------------------------------------------------------------------------------------

// 0x47c120, slot 16 (a key): Return, Escape, Space and the keys 0xfa/0xfb close the window
DWORD THISCALL DialogDisplayWindow_Key(BYTE *self, DWORD key)
{
    if (key == 0xd || key == 0x1b || key == 0x20 || key == 0xfa || key == 0xfb) {
        vc(self, CLOSE);
        return 1;
    }
    return 0;
}

// 0x47c2b0, slot 11 (mouse up): the left button closes the window when its flag +0x100 is set
DWORD THISCALL DialogDisplayWindow_MouseUp(BYTE *self, DWORD button, DWORD b)
{
    if (!button && mb(self, 0x100)) {
        vc(self, CLOSE);
        return 1;
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bde0)(self, button, b);
}

// 0x47c9b0, slot 21: the button +0x150 closes the window
DWORD THISCALL DialogDisplayWindow_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (what)
        return what;
    if (from == m(self, 0x150))
        return vc(self, CLOSE);
    return from;
}

// ---- The server dialog -------------------------------------------------------------------------------------------

// 0x4d40b0, slot 21: close, the two "unlimited" boxes (+0x164 for the number field +0x160 and the settings' +0x10,
// +0x158 for +0x154 and +0x14), Ok and start; a changed game type or map list refills, a changed number field
// clears its box and takes the value
DWORD THISCALL ServerDialog_Notify(BYTE *self, DWORD from, DWORD what)
{
    BYTE *settings = bp(m(self, 0x19c));
    if (what == 0) {
        if (from == m(self, 0x17c))
            return vc(self, CLOSE);
        if (from == m(self, 0x164) || from == m(self, 0x158)) {
            const bool first = from == m(self, 0x164);
            const DWORD box = first ? 0x164 : 0x158, field = first ? 0x160 : 0x154, setting = first ? 0x10 : 0x14;
            const DWORD on = vc(bp(m(self, box)), CHECKED);
            if (on) {
                const DWORD r = vc(bp(m(self, field)), SET_TITLE_, 0);
                m(bp(m(self, 0x19c)), setting) = 0;
                return first ? pv(settings) : r;
            }
            vc(bp(m(self, field)), 49, 10);
            const int v = x87_ftol(vc<long double>(bp(m(self, field)), 48));
            m(bp(m(self, 0x19c)), setting) = (DWORD)v;
            return (DWORD)v;
        }
        if (from == m(self, 0x144) || from == m(self, 0x178))
            return fn<DWORD(THISCALL *)(BYTE *)>(0x4d3f90)(self);
    } else if (what == 1) {
        if (from == m(self, 0x13c))
            return fn<DWORD(THISCALL *)(BYTE *)>(0x4d3c80)(self);
        if (from == m(self, 0x154) || from == m(self, 0x160)) {
            const bool second = from == m(self, 0x154);
            const DWORD box = second ? 0x158 : 0x164, field = second ? 0x154 : 0x160, setting = second ? 0x14 : 0x10;
            vc(bp(m(self, box)), SET_CHECKED, 0);
            const int v = x87_ftol(vc<long double>(bp(m(self, field)), 48));
            m(bp(m(self, 0x19c)), setting) = (DWORD)v;
            return (DWORD)v;
        }
        if (from == m(self, 0x144))
            return fn<DWORD(THISCALL *)(BYTE *)>(0x4d3e20)(self);
    }
    return frame_notify_(self, from, what);
}

// ---- The mission windows -----------------------------------------------------------------------------------------

// 0x50e9f0, slot 21 (the mission-over window): load a game (a save/load screen in place of the window), restart,
// the main menu, continue or close (once: the continue record written, the continue button disabled), save
DWORD THISCALL WindowMissionOver_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (what)
        return frame_notify_(self, from, what);
    auto replace = [&](DWORD a, DWORD b) {
        BYTE *o = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x1a4);
        const DWORD w = o ? fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x562970)(o, a, b) : 0;
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4040c0)(self, w, 1);
    };
    auto continue_ = [&]() -> DWORD {
        if (mb(self, 0x1b0))
            return mb(self, 0x1b0);
        mb(self, 0x1b0) = 1;
        DWORD r = fn<DWORD(THISCALL *)(DWORD)>(0x5ac500)(fn<DWORD(__cdecl *)()>(0x5ac560)());
        if (BYTE *b = bp(m(self, 0x14c)))
            mb(b, 0x61) = 0;
        return r;
    };
    if (from) {
        if (from == m(self, 0x140))
            return replace(0, 1);
        if (from == m(self, 0x144)) {
            BYTE *x = bp(vc(bp(gd(glob_addr(G_WORLD))), 1));
            vc(x, 10);
            mb(bp(gd(glob_addr(G_WORLD))), 0x312) = 1;
            if (m(self, 0x1ac) == 2)
                return continue_();
            return vc(self, CLOSE);
        }
        if (from == m(self, 0x148)) {
            vc(self, CLOSE);
            BYTE *x = bp(vc(bp(gd(glob_addr(G_WORLD))), 1));
            vc(x, 10);
            BYTE *o = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x158);
            DWORD menu = 0;
            if (o)
                menu = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x5be4a0)(o, fn<DWORD(THISCALL *)(DWORD)>(0x4cdf30)(gd(glob_addr(G_WORLD))));
            vc(bp(gd(glob_addr(G_WORLD))), 23, menu, 1);
            return vc(bp(gd(glob_addr(G_WORLD))), 81, 1);
        }
        if (from == m(self, 0x13c) || from == m(self, 0x14c)) {
            if (m(self, 0x1ac) == 2)
                return continue_();
            return vc(self, CLOSE);
        }
    }
    if (from == m(self, 0x150))
        return replace(pv(self), 0);
    return frame_notify_(self, from, what);
}

// 0x50ff20, slot 4 of the mission details window: the children, then the current mission's objectives (the flag
// +0x104 as a byte; the original's upper bytes are left over from the call before)
DWORD THISCALL WindowMissionDetails_Draw(BYTE *self, DWORD frame)
{
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
    return objectives_draw(mission_current(), frame, self, mb(self, 0x104));
}

// 0x5101d0, slot 57 (set rect) of the mission display page: with its flag +0x10c the objectives list +0x108 sized
// to the options (InventoryDisplay + 0xe4), its content as high as the objectives drawn (at least the options'),
// the bar +0x104 sized and both placed, the bar's range from the list
DWORD THISCALL MissionDisplayPage_SetRect(BYTE *self, DWORD rect)
{
    DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40d2a0)(self, rect);
    BYTE *opt = inventory_display() + 0xe4;
    if (!mb(self, 0x10c))
        return pv(opt);
    vc(bp(m(self, 0x108)), SET_SIZE, m(opt, 0x44), m(opt, 0x48));
    const DWORD mission = mission_current();
    BYTE *list = bp(m(self, 0x108));
    int h = (int)objectives_draw(mission, 0, list, (m(list, 0x100) & ~0xffu) | mb(list, 0x104));
    h += mi(list, 0x10c) * 2;
    if (mi(opt, 0x48) > h)
        h = mi(opt, 0x48);
    fn<DWORD(THISCALL *)(DWORD, DWORD, int)>(0x40d550)(m(self, 0x108), m(opt, 0x44), h);
    vc(bp(m(self, 0x104)), SET_SIZE, m(opt, 0x58), m(opt, 0x5c));
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d630)(m(self, 0x108), m(opt, 0x3c), m(opt, 0x40));
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d630)(m(self, 0x104), m(opt, 0x50), m(opt, 0x54));
    r = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x413ce0)(m(self, 0x104), m(self, 0x108));
    return r;
}

// 0x510310, slot 76 of the mission display page: its size from "Mission Objectives Diagram" (+0x100), the bar's
// images (the frame filter 0xb made and destroyed around them, unused)
DWORD THISCALL MissionDisplayPage_LoadImages(BYTE *self)
{
    diagram_size(self, glob_addr(0x61b2ec), 0x100);
    BYTE filter[0x14];
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b1cc0)(filter, 0);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b1db0)(filter, 0xb);
    bar_images(self, 0x104);
    return fn<DWORD(THISCALL *)(BYTE *)>(0x4b1d20)(filter);
}

// 0x510590, slot 4 of the mission display page: the diagram, the children, the title, the mission's name (in the
// options' font +0x38) or "No Mission Defined" (in the font of the options' +0x4c object's +0x14)
DWORD THISCALL MissionDisplayPage_Draw(BYTE *self, BYTE *frame)
{
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(frame, m(self, 0x100), 0, 0, glob_addr(0x6bcc68));
    fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
    BYTE *opt = inventory_display() + 0xe4;
    draw_title(opt, record_loaded(glob_addr(FONTS), m(opt, 0x20)), frame);
    const DWORD mission = mission_current();
    const DWORD none_font = record_loaded(glob_addr(FONTS), m(bp(m(opt, 0x4c)), 0x14));
    const DWORD font = record_inline(glob_addr(FONTS), m(opt, 0x38));
    const int x = mi(opt, 0x34) + mi(opt, 0x24);
    const DWORD h = fn<DWORD(THISCALL *)(DWORD)>(0x4a6be0)(font);
    const int y = (int)((m(opt, 0x30) - h) >> 1) + mi(opt, 0x28);
    if (!mission)
        return draw_text(none_font, frame, x, y, glob_addr(0x619b8c));
    return draw_text(font, frame, x, y, field_name(mission + 0xc));
}

// 0x515820, slot 21 (a question box with its owner +0x13c): yes tells the owner (its slot 21 with this box, 0) and
// closes, no closes
DWORD THISCALL QuestionDialog_Notify(BYTE *self, DWORD from, DWORD what)
{
    if (what)
        return what;
    if (from == m(self, 0x134)) {
        vc(bp(m(self, 0x13c)), NOTIFY, pv(self), 0);
        return vc(self, CLOSE);
    }
    if (from == m(self, 0x138))
        return vc(self, CLOSE);
    return from;
}

// ---- The sound dialog --------------------------------------------------------------------------------------------

// 0x57f870, slot 21: close; the voice, stereo and channel switches written to the INI file (stereo and channels
// to the resource cache as well); the three mute boxes and the three volume sliders (master: the cache's slot 6
// in steps of 0xd05 below; effects: a scale of (value * 0.1 + c) * c' as a float; music: the reporter's +0x64)
DWORD THISCALL SoundDialog_Notify(BYTE *self, DWORD from, DWORD what)
{
    auto sfx_scale = [&]() {
        volatile float f = fstp((fild64((long long)vc(bp(m(self, 0x148)), VALUE)) * cst(glob_addr(0x600df8)) + cst(glob_addr(0x5ec3f4))) * cst(glob_addr(0x5eee2c)));
        return float_bits(f);
    };
    BYTE *rep = bp(gd(glob_addr(G_REPORTER)));
    if (what == 0) {
        if (from == m(self, 0x17c))
            return vc(self, CLOSE);
        if (from == m(self, 0x15c)) {
            ini_write_int(glob_addr(0x60b05c), 1);
        } else if (from == m(self, 0x160)) {
            ini_write_int(glob_addr(0x60b05c), 0);
        } else if (from == m(self, 0x168)) {
            vc(resource_cache(), 5, 1);
            ini_write_int(glob_addr(0x6223d0), 1);
        } else if (from == m(self, 0x16c)) {
            vc(resource_cache(), 5, 0);
            ini_write_int(glob_addr(0x6223d0), 0);
        } else if (from == m(self, 0x174)) {
            ini_write_int(glob_addr(0x62242c), 0x20);
            fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x57b1b0)(resource_cache(), 0x20);
        } else if (from == m(self, 0x178)) {
            ini_write_int(glob_addr(0x62242c), 5);
            fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x57b1b0)(resource_cache(), 5);
        } else if (from == m(self, 0x138)) {
            ini_write_int(glob_addr(0x622420), vc(bp(m(self, 0x138)), CHECKED) != 0);
            if (vc(bp(m(self, 0x138)), CHECKED)) {
                vc(resource_cache(), 6, 0xfffff2fb);
            } else {
                BYTE *cache = resource_cache();
                vc(cache, 6, vc(bp(m(self, 0x13c)), VALUE) - 0xd05);
            }
        } else if (from == m(self, 0x144)) {
            ini_write_int(glob_addr(0x622404), vc(bp(m(self, 0x144)), CHECKED) != 0);
            if (vc(bp(m(self, 0x144)), CHECKED)) {
                fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x57b1d0)(resource_cache(), 0x3e800000);
            } else {
                const DWORD f = sfx_scale();
                fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x57b1d0)(resource_cache(), f);
            }
        } else if (from == m(self, 0x150)) {
            ini_write_int(glob_addr(0x6223c4), vc(bp(m(self, 0x150)), CHECKED) != 0);
            if (vc(bp(m(self, 0x150)), CHECKED))
                m(bp(gd(glob_addr(G_REPORTER))), 0x64) = 0xfffff2fb;
            else
                m(bp(gd(glob_addr(G_REPORTER))), 0x64) = vc(bp(m(self, 0x154)), VALUE) - 0xd05;
        }
    } else if (what == 1) {
        if (from == m(self, 0x13c)) {
            const DWORD v = vc(bp(from), VALUE) - 0xd05;
            vc(resource_cache(), 6, v);
            ini_write_int(glob_addr(0x622410), v);
            ini_write_int(glob_addr(0x622420), 0);
            vc(bp(m(self, 0x138)), SET_CHECKED, 0);
        } else if (from == m(self, 0x148)) {
            const DWORD f = sfx_scale();
            fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x57b1d0)(resource_cache(), f);
            fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4740)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob_addr(0x6223f8), f);
            ini_write_int(glob_addr(0x622404), 0);
            vc(bp(m(self, 0x144)), SET_CHECKED, 0);
        } else if (from == m(self, 0x154)) {
            m(rep, 0x64) = vc(bp(from), VALUE) - 0xd05;
            ini_write_int(glob_addr(0x6223b4), m(bp(gd(glob_addr(G_REPORTER))), 0x64));
            ini_write_int(glob_addr(0x6223c4), 0);
            vc(bp(m(self, 0x150)), SET_CHECKED, 0);
            vc(bp(gd(glob_addr(G_REPORTER))), 6);
        }
    }
    return frame_notify_(self, from, what);
}

// ---- The voice log panel -----------------------------------------------------------------------------------------

// 0x594e30, slot 76: its size from "Voice Log Diagram" (+0x12c), the "Replay" sequence looked up (unused), the
// bar's images (+0x114)
DWORD THISCALL VoiceLogPanel_LoadImages(BYTE *self)
{
    diagram_size(self, glob_addr(0x62459c), 0x12c);
    fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x615cec)));
    return bar_images(self, 0x114);
}

// 0x5950c0, slot 57 (set rect): with its flag +0x118 the header +0x10c, the list +0x100 and the replay button
// +0x110 sized and placed from the options (InventoryDisplay + 0x144), the list's bar remade; the first time
// (+0x108) the header's three columns (the last one the rest of the header's width when the options give none)
DWORD THISCALL VoiceLogPanel_SetRect(BYTE *self, DWORD rect)
{
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40d2a0)(self, rect);
    BYTE *opt = inventory_display() + 0x144;
    if (!mb(self, 0x118))
        return pv(opt);
    vc(bp(m(self, 0x10c)), SET_SIZE, m(opt, 0x2c), m(opt, 0x30));
    vc(bp(m(self, 0x100)), SET_SIZE, m(opt, 0x68), m(opt, 0x64));
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d630)(m(self, 0x10c), m(opt, 0x24), m(opt, 0x28));
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d630)(m(self, 0x100), m(opt, 0x5c), m(opt, 0x60));
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d630)(m(self, 0x110), m(opt, 0x80), m(opt, 0x84));
    DWORD r = fn<DWORD(THISCALL *)(DWORD)>(0x407630)(m(self, 0x100));
    if (mb(self, 0x108))
        return r;
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x402890)(m(self, 0x10c), 0, m(opt, 0x40));
    fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x402890)(m(self, 0x10c), 1, m(opt, 0x4c));
    DWORD w = m(opt, 0x58);
    if (!w) {
        BYTE *header = bp(m(self, 0x10c));
        const DWORD first = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(header, 0);
        w = m(header, 0xc) - m(header, 4) - first;
        w -= fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4028f0)(m(self, 0x10c), 1);
    }
    r = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x402890)(m(self, 0x10c), 2, w);
    mb(self, 0x108) = 1;
    return r;
}

// 0x595210, slot 4 (draw): the diagram (+0x12c), the title in the options' font (loaded inline), the children
DWORD THISCALL VoiceLogPanel_Draw(BYTE *self, BYTE *frame)
{
    BYTE *opt = inventory_display() + 0x144;
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(frame, m(self, 0x12c), 0, 0, glob_addr(0x6bcc68));
    draw_title(opt, record_inline(glob_addr(FONTS), m(opt, 0x20)), frame);
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
}

// 0x595650, slot 2 (update): the toolkit's update; the replay button +0x110 enabled (+0x61) while the list's
// slot 77 is below the header's slot 22 (asked once before for nothing)
DWORD THISCALL VoiceLogPanel_Update(BYTE *self, DWORD dt)
{
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b5d0)(self, dt);
    vc(bp(m(self, 0x100)), VALUE);
    const DWORD limit = vc(bp(m(self, 0x104)), 22);
    BYTE *list = bp(m(self, 0x100));
    const DWORD v = vc(list, VALUE);
    const DWORD below = v < limit ? 1 : 0;
    mb(bp(m(self, 0x110)), 0x61) = (BYTE)below;
    return below;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(c4_windows)
{
    const Hook list[] = {
#include "c4_windows_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
