// The inventory screen (docs/DECOMPILATION.md 9, "The rest of the user interface"): the panel at the bottom
// with its three tab pages (the inventory display page, the mission details page, the voice log panel) that
// slides in over the HUD bar, the inventory display page with its item list and buttons, the multiplayer item
// records of a character, CZaxWorld's opening and closing of the panel. Every call into the toolkit or the rest
// of the game goes through its exe address.
//
// Layouts [confirmed: the constructors and the slots below]:
//   inventory screen ("~CZaxInventoryScreen", 0x134, vtable 0x5ee340; the application's +0x340): +0x100 the
//     state (0 closed, 1 sliding in, 2 open, 3 sliding out), +0x104 the height shown (float), +0x108 the same
//     rounded, +0x10c the "Inventory Window" sequence, +0x110 the frame of the "Health Indicator Icon", +0x114
//     the close button, +0x118/+0x11c/+0x120 the tab buttons (inventory, mission, voice log), +0x124 the tab
//     control, +0x128/+0x12c/+0x130 the pages
//   page window (0x42d3d0: a CJWindow with the vtable 0x5ee688)
//   inventory display page ("~CInventoryDisplayPage", 0x138, vtable 0x5f8200): +0x100 the item list window
//     ("~CItemListWindow", 0x104, vtable 0x5f80d0), +0x104 the image scroll bar, +0x108 … +0x120 the buttons
//     (use, place, …), +0x124 the first and +0x128 the last button shown, +0x12c the item in focus (-1),
//     +0x130 the time since the last change of the buttons (1 when settled)
//   the inventory display options (CZaxCustomRules::InventoryDisplay 0x5a1d40 + 0x3c): +0x58 rows, +0x5c
//     columns, +0x60 the cell size, +0x70/+0x74 the list's position, +0x78/+0x7c the bar's, +0x80/+0x84 its
//     size, +0x44/+0x48 the first button's position, +0x4c the gap between buttons
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr DWORD FLOAT_1 = 0x3f800000;
constexpr int CHECK = 78;

BYTE *inventory_display()   // CZaxCustomRules::InventoryDisplay 0x5a1d40 (the rules or null in ecx)
{
    return fn<BYTE *(THISCALL *)(DWORD)>(0x5a1d40)(custom_rules());
}
BYTE *player_inventory() { return fn<BYTE *(__cdecl *)()>(0x428b10)(); }
DWORD first_character(DWORD world) { return fn<DWORD(THISCALL *)(DWORD)>(0x4f5d80)(world); }
void move_to_(DWORD w, int x, int y) { fn<void(THISCALL *)(DWORD, int, int)>(0x40d630)(w, x, y); }
void set_image(DWORD button, DWORD image, DWORD a, DWORD b) { fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(button, image, a, b); }
void clear_tip(DWORD button, DWORD v) { fn<void(THISCALL *)(DWORD, DWORD)>(0x4101c0)(button, v); }
BYTE *new_radio(BYTE *parent, DWORD text, DWORD partner)   // RadioButton::RadioButton 0x4109a0 (0x138)
{
    BYTE *w = alloc(0x138);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x4109a0)(w, parent, c_str(text), partner) : nullptr;
}
void play_name(DWORD name) { fn<void(THISCALL *)(DWORD, DWORD)>(0x57e820)(glob_addr(G_SOUND_LIST), name); }
long double f32(DWORD bits) { float f; memcpy(&f, &bits, 4); return f; }
long double fat(DWORD address) { return (long double)*at<float *>(address); }

}  // namespace

// ---- The inventory screen -------------------------------------------------------------------------------------

// The constructor (the parent): 640 × 200, the font of the display options (+0x20), the tab control with the
// three pages, the three tab buttons ("Inventory Tab", "Mission Tab", "Voice Log Tab": a radio group) and the
// close button at the options' positions, the tab control in front at the options' +8/+0xc
BYTE *THISCALL InventoryScreen_Ctor(BYTE *self, DWORD parent)   // 0x428c00
{
    window_ctor(self, parent, glob<const char *>(0x60bf48));
    m(self, 0) = vt(0x5ee340);
    mb(self, W_BORDER) = 0;
    m(self, 0x110) = 0;
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, 0x280, 0xc8);
    m(self, 0x100) = 0, m(self, 0x104) = 0, m(self, 0x108) = 0;
    BYTE *opt = inventory_display();
    m(self, W_FONT) = record_loaded(glob_addr(FONTS), m(opt, 0x20));
    BYTE *w = alloc(0x11c);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x409be0)(w, self);
    m(self, 0x124) = pv(w);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40a2b0)(w, 0);
    w = alloc(0x138);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x4dabd0)(w, m(self, 0x124));
    m(self, 0x128) = pv(w);
    w = alloc(0x110);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x50ffe0)(w, m(self, 0x124));
    m(self, 0x12c) = pv(w);
    w = alloc(0x130);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x5948b0)(w, m(self, 0x124));
    m(self, 0x130) = pv(w);
    for (DWORD page = 0x128; page <= 0x130; page += 4)
        vc(bp(m(self, page)), V_SET_SIZE, m(opt, 0x10), m(opt, 0x14));
    // the tab buttons: member, text, sequence, position in the options, the record loaded inline
    const DWORD tabs[3][4] = {{0x118, 0x62ea14, 0x60bf38, 0x24}, {0x11c, 0x62ea04, 0x60bf2c, 0x2c}, {0x120, 0x62ea10, 0x60bf1c, 0x34}};
    for (int k = 0; k < 3; k++) {
        w = new_radio(self, tabs[k][1], k ? m(self, tabs[k - 1][0]) : 0);
        m(self, tabs[k][0]) = pv(w);
        const DWORD index = find_index(glob_addr(SEQUENCES), tabs[k][2]);
        const DWORD image = k ? record_inline(glob_addr(SEQUENCES), index) : record_loaded(glob_addr(SEQUENCES), index);
        set_image(m(self, tabs[k][0]), image, 1, 0);
        move_to_(m(self, tabs[k][0]), mi(opt, tabs[k][3]), mi(opt, tabs[k][3] + 4));
        clear_tip(m(self, tabs[k][0]), 1);
        vc(bp(m(self, tabs[k][0])), 42);
        add_child(self, bp(m(self, tabs[k][0])), 0, 0, NOWHERE, NOWHERE, 0);
    }
    vc(bp(m(self, 0x118)), CHECK, 1);
    w = new_button(self, c_str(glob_addr(0x62ea0c)), 0);
    m(self, 0x114) = pv(w);
    const DWORD index = find_index(glob_addr(SEQUENCES), glob_addr(0x60bf00));   // "Inventory Close Button Down"
    set_image(m(self, 0x114), record_inline(glob_addr(SEQUENCES), index), 1, 0);
    move_to_(m(self, 0x114), mi(opt, 0x18), mi(opt, 0x1c));
    vc(bp(m(self, 0x114)), 42);
    add_child(self, bp(m(self, 0x114)), 0, 0, NOWHERE, NOWHERE, 0);
    for (DWORD page = 0x128; page <= 0x130; page += 4)
        fn<void(THISCALL *)(DWORD, DWORD)>(0x40a2f0)(m(self, 0x124), m(self, page));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40a4b0)(m(self, 0x124), 0, 1);
    add_child_front(self, bp(m(self, 0x124)), 5, 0, m(opt, 8), m(opt, 0xc), 6);
    return self;
}
// The deleting destructor: the icon's frame deleted (slot 1 with 1)
BYTE *THISCALL InventoryScreen_DeletingDtor(BYTE *self, DWORD flags)   // 0x429370
{
    m(self, 0) = vt(0x5ee340);
    if (BYTE *f = bp(m(self, 0x110))) {
        vc(f, 1, 1);
        m(self, 0x110) = 0;
    }
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x134);
    return self;
}
// The init (after loading): the "Inventory Window" sequence (+0x10c) sizes the screen, the "Health Indicator
// Icon" of "Interface" into a frame (+0x110), the screen's sounds precached, every page's slot 76
void THISCALL InventoryScreen_Init(BYTE *self)   // 0x4293c0
{
    const DWORD index = find_index(glob_addr(SEQUENCES), glob_addr(0x60bfb8));
    const DWORD seq = record_loaded(glob_addr(SEQUENCES), index);
    m(self, 0x10c) = seq;
    const DWORD h = vc(bp(seq), 39);
    vc(self, V_SET_SIZE, vc(bp(seq), 45), h);
    BYTE *frame = alloc(0x40);
    if (frame)
        frame = fn<BYTE *(THISCALL *)(BYTE *)>(0x4a8a90)(frame);
    m(self, 0x110) = pv(frame);
    fn<void(THISCALL *)(BYTE *, const char *, const char *, DWORD, DWORD, DWORD)>(0x4a9a30)(
        frame, glob<const char *>(0x60a630), glob<const char *>(0x60bfa0), 0, 1, 0);
    const GlobKey SOUNDS[4] = {0x60bf8c, 0x60bf78, 0x60b768, 0x60bf60};
    for (GlobKey s : SOUNDS)
        fn<void(THISCALL *)(DWORD, const char *)>(0x57e7b0)(glob_addr(G_SOUND_LIST), glob<const char *>(s));
    for (DWORD i = 0; i < m(bp(m(self, 0x124)), 0x104); i++)
        vc(bp(m(bp(m(bp(m(self, 0x124)), 0x108)), i * 4)), 76);
}
// Open: the first character's slot 36 object takes the character (slot 24), the tabs' slot 20; closed: "Inventory
// Up.wav", sliding in, the reporter's slot 23 with the screen, placed at the bottom of the parent (80 pixels to
// the left on a 640 wide screen), the reporter's slot 28; sliding out turns back
void THISCALL InventoryScreen_Open(BYTE *self)   // 0x4294f0
{
    BYTE *o = vc<BYTE *>(bp(first_character(gd(glob_addr(G_WORLD)))), 36);
    vc(o, 24, first_character(gd(glob_addr(G_WORLD))), 0);
    vc(bp(m(self, 0x124)), 20, 1);
    const DWORD state = m(self, 0x100);
    if (state == 0) {
        play_name(glob_addr(0x60bf8c));
        m(self, 0x100) = 1;
        vc(bp(gd(glob_addr(G_REPORTER))), 23, self, 1);
        const DWORD w = vc(bp(m(bp(gd(glob_addr(G_REPORTER))), 4)), 45);
        BYTE *parent = bp(m(self, W_PARENT));
        const int y = mi(parent, W_RECT + 0xc) - mi(self, 0x108) - mi(parent, W_RECT + 4);
        move_to(self, w == 0x280 ? -0x50 : 0, y);
        vc(bp(gd(glob_addr(G_REPORTER))), 28);
    } else if (state == 3) {
        m(self, 0x100) = 1;
    }
}
// Close: the HUD bars' slot (0x5085f0 on the application's +0x344); sliding in or open: sliding out (with
// "Sounds/Inventory Down.wav" when it was open)
void THISCALL InventoryScreen_Close(BYTE *self)   // 0x4295e0
{
    fn<void(THISCALL *)(DWORD)>(0x5085f0)(m(bp(gd(glob_addr(G_APP))), 0x344));
    const DWORD state = m(self, 0x100);
    if (state == 2)
        play_name(glob_addr(0x60bfcc));
    else if (state != 1)
        return;
    m(self, 0x100) = 3;
}
// Slot 2 (the seconds): while sliding in or open, the world's input bits 9, 10, 11 ask the tab buttons (slot
// 18; the last asked decides) and close the screen when it says no; without a character or its inventory
// nothing more; sliding in grows the height by the seconds × 0x5ee470 up to the sequence's (then open), sliding
// out shrinks it to 0 (then closed: the tabs' slot 20, out of the parent, the reporter's slot 29); the window's
// update while not closed
void THISCALL InventoryScreen_Update(BYTE *self, float dt)   // 0x429620
{
    DWORD state = m(self, 0x100);
    if (state == 2 || state == 1) {
        BYTE ok = 1, asked = 0;
        const DWORD bits = vc(bp(gd(glob_addr(G_WORLD))), 88);
        auto test = [&](DWORD bit) { return (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x46ed70)(bits, bit); };
        auto ask = [&](DWORD tab) { return (BYTE)vc(bp(m(bp(m(bp(gd(glob_addr(G_APP))), 0x340)), tab)), 18); };
        bool decide;
        if (test(9))
            ok = ask(0x118), asked = 1;
        if (test(0xa))
            ok = ask(0x11c), asked = 1;
        if (test(0xb)) {
            ok = ask(0x120);
            decide = true;
        } else {
            decide = asked != 0;
        }
        if (decide && !ok)
            fn<void(THISCALL *)(DWORD)>(0x5a07e0)(gd(glob_addr(G_APP)));
    }
    if (!first_character(gd(glob_addr(G_APP))))
        return;
    if (!fn<DWORD(THISCALL *)(DWORD)>(0x4267e0)(first_character(gd(glob_addr(G_APP)))))
        return;
    state = m(self, 0x100);
    int shown = 0;
    bool place = false;
    if (state == 1) {
        if (mi(self, 0x108) >= (int)vc(bp(m(self, 0x10c)), 39)) {
            m(self, 0x100) = 2;
        } else {
            volatile float grown = (float)((long double)dt * fat(glob_addr(0x5ee470)) + f32(m(self, 0x104)));
            const int most = (int)vc(bp(m(self, 0x10c)), 39);
            if (x87_ftol(grown) > most) {
                volatile float full = (float)(long long)vc(bp(m(self, 0x10c)), 39);
                m<float>(self, 0x104) = full;
            } else {
                m<float>(self, 0x104) = grown;
            }
            shown = x87_fistp(f32(m(self, 0x104)));
            place = true;
        }
    } else if (state == 3) {
        const long double h = f32(m(self, 0x104));
        if (!(h < fat(glob_addr(0x5ebed0))) && !(h > fat(glob_addr(0x5ebed0)))) {   // 0 (or NaN: fcomp's C3)
            m(self, 0x100) = 0;
            vc(bp(m(self, 0x124)), 20, 0);
            vc(bp(m(self, W_PARENT)), 33, self);
            vc(bp(gd(glob_addr(G_REPORTER))), 29);
        } else {
            const long double v = h - (long double)dt * fat(glob_addr(0x5ee470));
            if (x87_lt(v, fat(glob_addr(0x5ebed0)))) {
                m(self, 0x104) = 0;
            } else {
                volatile float s = (float)v;
                m<float>(self, 0x104) = s;
            }
            shown = x87_fistp(f32(m(self, 0x104)));
            place = true;
        }
    }
    if (place) {
        m(self, 0x108) = (DWORD)shown;
        BYTE *parent = bp(m(self, W_PARENT));
        set_top(self, mi(parent, W_RECT + 0xc) - mi(parent, W_RECT + 4) - shown);
    }
    if (m(self, 0x100))
        fn<void(THISCALL *)(BYTE *, float)>(0x40b5d0)(self, dt);
}
// The room above the panel in a frame: its height (slot 39) less the panel's height when the application's
// window says the panel is in front (slot 27)
int THISCALL InventoryScreen_RoomAbove(BYTE *self, BYTE *frame)   // 0x429880
{
    BYTE *window = fn<BYTE *(THISCALL *)(DWORD)>(0x4cdf30)(gd(glob_addr(G_REPORTER)));
    if (!vc(window, 27))
        return (int)vc(frame, 39);
    return (int)vc(frame, 39) - mi(self, 0x108);
}
// The multiplayer item record of a category (the character, the type's +0x24, the category's index, a player
// value, the inventory): the item in use and its definition; while it changes to another item, both items with
// the parts of the change (1 - seconds into / seconds to, and the rest), else the one with 1
void THISCALL InventoryScreen_ItemRecord(BYTE *self, DWORD a1, DWORD a2, DWORD category, DWORD a4, BYTE *inventory)   // 0x429980
{
    if (!fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4251c0)(inventory, category))
        return;
    const DWORD index = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x425290)(inventory, category);
    if (index == 0xffffffff)
        return;
    DWORD item = vc(inventory, 26, index);
    if (item && !(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(item, fn<DWORD(__cdecl *)()>(0x429e50)()))
        item = 0;
    DWORD def = fn<DWORD(THISCALL *)(DWORD)>(0x4dd480)(item);
    if (def && !(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(def, fn<DWORD(__cdecl *)()>(0x5b5f10)()))
        def = 0;
    const DWORD kind = fn<DWORD(THISCALL *)(DWORD)>(0x4dd580)(item);
    const DWORD changing = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4252e0)(inventory, category);
    DWORD type = 0;
    if (kind && (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(kind, fn<DWORD(__cdecl *)()>(0x5b9400)()))
        type = kind;
    DWORD out1, out2;
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD *, DWORD *)>(0x4298d0)(self, type, a4, &out2, &out1);
    using Record = void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, float, DWORD);
    if (!changing) {
        fn<Record>(0x5b7f00)(def, item, a1, a4, 1.0f, a2);
        return;
    }
    volatile float into = (float)fn<long double(THISCALL *)(BYTE *, DWORD)>(0x425320)(inventory, category);
    const long double to = fn<long double(THISCALL *)(BYTE *, DWORD)>(0x425300)(inventory, category);
    volatile float part = (float)(fat(glob_addr(0x5ec3f4)) - (long double)into / to);
    fn<Record>(0x5b7f00)(def, item, a1, a4, part, a2);
    volatile float rest = (float)(fat(glob_addr(0x5ec3f4)) - (long double)part);
    fn<Record>(0x5b7f00)(def, changing, a1, a4, rest, a2);
}
// The multiplayer item records of a character (the character, a player value, the inventory, the second
// switch): its hit points (at least 1 when above 0) as its energy, the game type's slot 40, a record of every
// item category whose switches match (bit 0 without, bit 2 with the switch), the record count; a category
// that is no CZaxInventoryTypeDefinition stops the game ("FATAL ERROR!")
void THISCALL InventoryScreen_ItemRecords(BYTE *self, DWORD character, DWORD a2, DWORD inventory, BYTE second)   // 0x429ae0
{
    using PlayerOf = BYTE *(THISCALL *)(DWORD);
    if (character) {
        long double hp = fn<long double(THISCALL *)(DWORD)>(0x48d030)(character);
        if (!x87_le(hp, fat(glob_addr(0x5ebed0))) && x87_lt(hp, fat(glob_addr(0x5ec3f4))))
            hp = fat(glob_addr(0x5ec3f4));
        fn<void(THISCALL *)(BYTE *, int)>(0x5ba340)(fn<PlayerOf>(0x5bac80)(character), x87_ftol(hp));
    }
    if (fn<int(THISCALL *)(DWORD)>(0x5a01a0)(gd(glob_addr(G_APP))) != -1 && fn<DWORD(THISCALL *)(DWORD)>(0x59ff80)(gd(glob_addr(G_APP)))) {
        BYTE *type = fn<BYTE *(THISCALL *)(DWORD)>(0x59ff80)(gd(glob_addr(G_APP)));
        vc(type, 40, character, inventory);
    }
    m(fn<PlayerOf>(0x5bac80)(character), 0x114) = 0;
    constexpr DWORD CATEGORIES = 0x6c0800;
    // a category that is no item type stops the game (the original faults on a missing one: its class is read
    // through null)
    auto fatal = [](DWORD cat, DWORD i) {
        DWORD text;
        zs_ctor(&text);
        const DWORD name = m(bp(vc(bp(cat), 2)), 0xc);
        using Name = DWORD(THISCALL *)(DWORD, DWORD);
        const DWORD first = fn<Name>(0x591d00)(glob_addr(CATEGORIES), i);
        zs_format(&text, glob<const char *>(0x60bfe8), fn<Name>(0x591d00)(glob_addr(CATEGORIES), i), name, first);
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x60a700), c_str(&text));
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    };
    for (DWORD i = 0; i < gd(glob_addr(CATEGORIES) + 0x34); i++) {
        DWORD recs = gd(glob_addr(CATEGORIES) + 0x30);
        if (!m(bp(recs), i * 0x80 + 0x78))
            continue;
        if (!recs)
            fatal(0, i);
        const DWORD k = i < gd(glob_addr(CATEGORIES) + 0x34) ? i : 0;
        if (gb(recs + (k << 7) + 0x7f))
            fatal(0, i);
        if (!m(bp(recs), (k << 7) + 0x78)) {
            fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(glob_addr(CATEGORIES), k);
            recs = gd(glob_addr(CATEGORIES) + 0x30);
        }
        const DWORD cat = m(bp(recs), (k << 7) + 0x78);
        if (!cat || !(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(cat, fn<DWORD(__cdecl *)()>(0x5b9400)()))
            fatal(cat, i);
        const DWORD flags = m(bp(cat), 0x18);
        if (((flags & 1) && !second) || (((flags >> 2) & 1) && second))
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x429980)(self, character, m(bp(cat), 0x24), i, a2, inventory);
    }
    const DWORD count = m(fn<PlayerOf>(0x5bac80)(character), 0x114);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x5ba450)(fn<PlayerOf>(0x5bac80)(character), count);
}
// Slot 4 (the frame): the sequence +0x10c at (0, 0) through the transparent mask while not closed, then the
// children (0x40b620)
void THISCALL InventoryScreen_Draw(BYTE *self, DWORD frame)   // 0x429da0
{
    if (m(self, 0x100))
        fn<void(THISCALL *)(DWORD, DWORD, int, int, DWORD)>(0x4b3280)(frame, m(self, 0x10c), 0, 0, glob_addr(0x6bcc68));
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}
// The tab buttons' slot 18 (the voice log, the mission, the inventory tab)
DWORD THISCALL InventoryScreen_ShowVoiceLog(BYTE *self) { return vc(bp(m(self, 0x120)), 18); }   // 0x429e20
DWORD THISCALL InventoryScreen_ShowMission(BYTE *self) { return vc(bp(m(self, 0x11c)), 18); }    // 0x429e30
DWORD THISCALL InventoryScreen_ShowInventory(BYTE *self) { return vc(bp(m(self, 0x118)), 18); }  // 0x429e40

// ---- The page window -------------------------------------------------------------------------------------------

BYTE *THISCALL PageWindow_Ctor(BYTE *self, DWORD parent, const char *name)   // 0x42d3d0
{
    window_ctor(self, parent, name);
    m(self, 0) = vt(0x5ee688);
    return self;
}
void THISCALL PageWindow_Dtor(BYTE *self)   // 0x42d3f0
{
    m(self, 0) = vt(0x5ea72c);
    window_dtor(self);
}

// ---- The inventory display page ---------------------------------------------------------------------------------

// The item list window's deleting destructor (its +0x100 deleted with slot 1)
BYTE *THISCALL ItemListWindow_DeletingDtor(BYTE *self, DWORD flags)   // 0x4da240
{
    m(self, 0) = vt(0x5f80d0);
    if (BYTE *o = bp(m(self, 0x100))) {
        vc(o, 1, 1);
        m(self, 0x100) = 0;
    }
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x104);
    return self;
}
// The item list window shows item i (clamped to the inventory's count; its +0xc the i-th item in category
// order): the cell's row scrolled into view
void THISCALL ItemListWindow_ShowItem(BYTE *self, int i)   // 0x4da780
{
    BYTE *opt = inventory_display() + 0x3c;
    BYTE *inv = player_inventory();
    const DWORD n = m(inv, 0x20);
    if (!n)
        return;
    const int last = (int)n - 1;
    if (i < 0)
        i = 0;
    else if (i > last)
        i = last;
    m(inv, 0xc) = fn<DWORD(__fastcall *)(BYTE *, int)>(0x5b2aa0)(player_inventory(), i);
    const DWORD size = m(opt, 0x60);
    const DWORD row = (DWORD)i / m(opt, 0x5c);
    const int top = (int)(row * (size + 1));
    int r[4] = {0, top, 1, (int)size + top + 1};
    fn<void(THISCALL *)(BYTE *, int *)>(0x40d940)(self, r);
}
// The constructor (the parent): the item list window (cells of the options' size, as many rows as the inventory
// needs), the image scroll bar beside it, no buttons yet
BYTE *THISCALL InventoryDisplayPage_Ctor(BYTE *self, DWORD parent)   // 0x4dabd0
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x42d3d0)(self, parent, glob_addr(0x616e98));
    m(self, 0x12c) = 0xffffffff;
    m(self, 0x124) = 0, m(self, 0x128) = 0;
    m(self, 0x130) = FLOAT_1;
    m(self, 0) = vt(0x5f8200);
    BYTE *opt = inventory_display() + 0x3c;
    BYTE *list = alloc(0x104);
    if (list) {
        window_ctor(list, pv(self), glob<const char *>(0x616e84));
        m(list, 0x100) = 0;
        m(list, 0) = vt(0x5f80d0);
    }
    m(self, 0x100) = pv(list);
    add_child(self, list, 0, 0, NOWHERE, NOWHERE, 0);
    const DWORD cell = m(opt, 0x60) + 1;
    vc(bp(m(self, 0x100)), V_SET_SIZE, m(opt, 0x5c) * cell + 1, m(opt, 0x58) * cell + 2);
    BYTE *o2 = fn<BYTE *(__cdecl *)()>(0x429de0)() + 0x3c;
    BYTE *inv = player_inventory();
    const DWORD n = inv ? m(inv, 0x20) : 0;
    const DWORD cols = m(o2, 0x5c);
    int rows = (int)((n + cols - 1) / cols);
    if (rows < 1)
        rows = 1;
    const DWORD c2 = m(o2, 0x60) + 1;
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d550)(m(self, 0x100), c2 * cols + 1, c2 * (DWORD)rows + 2);
    move_to_(m(self, 0x100), mi(opt, 0x70), mi(opt, 0x74));
    BYTE *bar = alloc(0x190);
    if (bar)
        bar = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4144b0)(bar, self, 1);
    m(self, 0x104) = pv(bar);
    add_child(self, bar, 0, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x104)), V_SET_SIZE, m(opt, 0x80), m(opt, 0x84));
    move_to_(m(self, 0x104), mi(opt, 0x78), mi(opt, 0x7c));
    m(bp(m(self, 0x104)), 0x160) = m(self, 0x100);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x413ce0)(m(self, 0x104), m(self, 0x100));
    for (DWORD o = 0x108; o < 0x124; o += 4)
        m(self, o) = 0;
    return self;
}
BYTE *THISCALL InventoryDisplayPage_DeletingDtor(BYTE *self, DWORD flags)   // 0x4dadb0
{
    fn<void(THISCALL *)(BYTE *)>(0x42d3f0)(self);
    if (flags & 1)
        pool_delete(self, 0x138);
    return self;
}
// Slot 2 (the seconds): the settle time grows by the seconds × 0x5eee2c up to 1; the list's content size for the
// inventory's rows, the scroll bar's range, the buttons
void THISCALL InventoryDisplayPage_Update(BYTE *self, float dt)   // 0x4db0c0
{
    if (x87_lt(f32(m(self, 0x130)), fat(glob_addr(0x5ec3f4)))) {
        const long double v = (long double)dt * fat(glob_addr(0x5eee2c)) + f32(m(self, 0x130));
        volatile float s = (float)v;
        m<float>(self, 0x130) = s;
        if (!x87_le(v, fat(glob_addr(0x5ec3f4))))
            m(self, 0x130) = FLOAT_1;
    }
    BYTE *opt = fn<BYTE *(__cdecl *)()>(0x429de0)() + 0x3c;
    BYTE *inv = player_inventory();
    const DWORD n = inv ? m(inv, 0x20) : 0;
    const DWORD cols = m(opt, 0x5c);
    int rows = (int)((cols + n - 1) / cols);
    if (rows < 1)
        rows = 1;
    const DWORD cell = m(opt, 0x60) + 1;
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d550)(m(self, 0x100), cell * cols + 1, cell * (DWORD)rows + 2);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x413ce0)(m(self, 0x104), m(self, 0x100));
    fn<void(THISCALL *)(BYTE *)>(0x4db430)(self);
}
// A button of the page (shown, the member, the text): none when not shown (al 0); the text 0x6c0d5c makes a
// toggle button (unchecked), others a button; the "Info_Use_Deploy_Button" image, no tip; the first goes to the
// options' position, the others below the previous one (how 8, the gap +0x4c); al 1
BYTE THISCALL InventoryDisplayPage_AddButton(BYTE *self, BYTE shown, DWORD *out, const char *text)   // 0x4db170
{
    BYTE *opt = inventory_display() + 0x3c;
    if (!shown)
        return 0;
    const char *toggle = c_str(glob_addr(0x6c0d5c));
    int k = 0;
    while (text[k] == toggle[k] && text[k])
        k++;
    if (text[k] == toggle[k]) {
        BYTE *b = alloc(0x138);
        if (b)
            b = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x410ab0)(b, self, text, 0);
        *out = pv(b);
        vc(b, CHECK, 0);
    } else {
        *out = pv(new_button(self, text, 0));
    }
    const DWORD button = *out;
    const DWORD index = find_index(glob_addr(SEQUENCES), glob_addr(0x616f0c));
    set_image(button, record_inline(glob_addr(SEQUENCES), index), 1, 0);
    clear_tip(button, 1);
    fn<void(THISCALL *)(DWORD, DWORD, int)>(0x4100c0)(button, glob_addr(EMPTY_TEXT), -1);
    bool below = true;
    if (!m(self, 0x124)) {
        m(self, 0x124) = *out;
        below = false;
    }
    m(self, 0x128) = *out;
    if (below) {
        vc(self, V_ADD_CHILD, *out, 8, 0, NOWHERE, m(opt, 0x4c), 2);
    } else {
        vc(self, V_ADD_CHILD, *out, 0, 0, NOWHERE, NOWHERE, 0);
        move_to_(*out, mi(opt, 0x44), mi(opt, 0x48));
    }
    vc(bp(button), 42);
    return 1;
}
// The buttons for the item in focus (rebuilt when the item or its definition changed; the definition is kept in
// the function-local static 0x6c0d64): the toggle button, then use, place, the undefined one and disable as the
// definition's switches (+0xf4 bits 3, 0, 6, 2) allow, all as wide as the widest; none without items
void THISCALL InventoryDisplayPage_Buttons(BYTE *self)   // 0x4db430
{
    BYTE *inv = player_inventory();
    auto delete_buttons = [&]() {
        for (DWORD o = 0x108; o < 0x124; o += 4)
            if (BYTE *b = bp(m(self, o))) {
                vc(b, 0, 1);
                m(self, o) = 0;
            }
    };
    if (!m(inv, 0x20)) {
        delete_buttons();
        m(self, 0x124) = 0, m(self, 0x128) = 0;
        m(self, 0x12c) = 0xffffffff;
        return;
    }
    const DWORD item = vc(inv, 26, fn<DWORD(THISCALL *)(BYTE *)>(0x426070)(inv));
    const DWORD def = item ? fn<DWORD(THISCALL *)(DWORD)>(0x4dd480)(item) : 0;
    if (!(gb(glob_addr(0x6c0d68)) & 1)) {
        gd(glob_addr(0x6c0d64)) = def;
        gb(glob_addr(0x6c0d68)) |= 1;
    }
    if (m(self, 0x12c) == fn<DWORD(THISCALL *)(BYTE *)>(0x426070)(inv) && def == gd(glob_addr(0x6c0d64)))
        return;
    gd(glob_addr(0x6c0d64)) = def;
    m(self, 0x12c) = fn<DWORD(THISCALL *)(BYTE *)>(0x426070)(inv);
    if (!def || !(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(def, fn<DWORD(__cdecl *)()>(0x5b5f10)()))
        return;
    delete_buttons();
    m(self, 0x124) = 0, m(self, 0x128) = 0;
    using Add = BYTE(THISCALL *)(BYTE *, BYTE, BYTE *, const char *);
    using Label = const char *(THISCALL *)(DWORD);
    fn<Add>(0x4db170)(self, 1, self + 0x108, c_str(glob_addr(0x6c0d5c)));
    const DWORD f = m(bp(def), 0xf4);
    fn<Add>(0x4db170)(self, (BYTE)((f >> 3) & 1), self + 0x10c, fn<Label>(0x5b7740)(def));
    fn<Add>(0x4db170)(self, (BYTE)(mb(bp(def), 0xf4) & 1), self + 0x114, fn<Label>(0x5b76e0)(def));
    fn<Add>(0x4db170)(self, (BYTE)((m(bp(def), 0xf4) >> 6) & 1), self + 0x118, fn<Label>(0x5b7770)(def));
    fn<Add>(0x4db170)(self, (BYTE)((m(bp(def), 0xf4) >> 2) & 1), self + 0x11c, fn<Label>(0x5b7710)(def));
    if (m(self, 0x124))
        same_width(bp(m(self, 0x124)), bp(m(self, 0x128)));
}

// ---- CZaxWorld: the inventory screen opened and closed ---------------------------------------------------------

// Open (on the application): unless already sliding in or open, the cursor's position kept (+0x278/+0x27c) and
// the cursor put in the middle of the window +0x2a8, the HUD's slot 0x508570, the screen's Open
void THISCALL World_OpenInventory(BYTE *self)   // 0x5a0760
{
    const DWORD state = m(bp(m(self, 0x340)), 0x100);
    if (state == 2 || state == 1)
        return;
    fn<void(THISCALL *)(DWORD, BYTE *, BYTE *)>(0x5184e0)(glob_addr(G_CURSOR), self + 0x278, self + 0x27c);
    const DWORD h = vc(bp(m(self, 0x2a8)), 39);
    const DWORD w = vc(bp(m(self, 0x2a8)), 45);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x518570)(glob_addr(G_CURSOR), w >> 1, h >> 1);
    fn<void(THISCALL *)(DWORD)>(0x508570)(m(self, 0x344));
    fn<void(THISCALL *)(DWORD)>(0x4294f0)(m(self, 0x340));
}
// Close: while sliding in or open, 0x505370, the cursor back where it was, the screen's Close, the cursor hidden
// until it says it is not shown (0x518690)
void THISCALL World_CloseInventory(BYTE *self)   // 0x5a07e0
{
    BYTE *screen = bp(m(self, 0x340));
    if (!screen)
        return;
    const DWORD state = m(screen, 0x100);
    if (state != 2 && state != 1)
        return;
    fn<void(__cdecl *)()>(0x505370)();
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x518570)(glob_addr(G_CURSOR), m(self, 0x278), m(self, 0x27c));
    fn<void(THISCALL *)(DWORD)>(0x4295e0)(m(self, 0x340));
    using Shown = DWORD(THISCALL *)(DWORD);
    if (!(BYTE)fn<Shown>(0x518690)(glob_addr(G_CURSOR)))
        return;
    do
        fn<void(THISCALL *)(DWORD)>(0x5185f0)(glob_addr(G_CURSOR));
    while ((BYTE)fn<Shown>(0x518690)(glob_addr(G_CURSOR)));
}


// ---- The HUD bar's slide (the application's +0x344; 0x18 bytes) ----------------------------------------------
// +0 the state (0 hidden, 1 sliding in, 2 shown, 3 sliding out), +4 the seconds of the slide, +0x14 hidden by
// the inventory screen. The slide's length and sounds come from CZaxCustomRules::HudOptions 0x5a1fa0 (+0x18 the
// seconds, +0x1c the sound index in, +0x20 out). [confirmed: the functions below] Turning back mid-slide
// mirrors the time (length - time); starting from hidden or shown sets it to the length.

namespace {

BYTE *hud_options() { return fn<BYTE *(THISCALL *)(DWORD)>(0x5a1fa0)(custom_rules()); }
void play_index(DWORD i) { fn<void(THISCALL *)(DWORD, DWORD)>(0x57e890)(glob_addr(G_SOUND_LIST), i); }
// The slide turned towards `to` (1 in, 3 out): from the opposite slide the time mirrored, else the length
void slide(BYTE *self, BYTE *opt, DWORD from, DWORD to, DWORD sound)
{
    const long double length = f32(m(opt, 0x18));
    volatile float t = (float)(m(self, 0) == from ? length - f32(m(self, 4)) : length);
    m<float>(self, 4) = t;
    m(self, 0) = to;
    play_index(m(opt, sound));
}

}  // namespace

void THISCALL HudSlide_Ctor(BYTE *self)   // 0x507f20
{
    m(self, 0) = 0, m(self, 4) = 0, m(self, 8) = 0, m(self, 0xc) = 0, m(self, 0x10) = 0;
    mb(self, 0x14) = 0;
}
// How far the bar is in (st0): 0 hidden or when the application's window has no bar (slot 27), the part of the
// slide, 1 shown
long double THISCALL HudSlide_Fraction(BYTE *self)   // 0x507f40
{
    const DWORD length = m(hud_options(), 0x18);
    BYTE *window = fn<BYTE *(THISCALL *)(DWORD)>(0x4cdf30)(gd(glob_addr(G_REPORTER)));
    if (!vc(window, 27))
        return fat(glob_addr(0x5ebed0));
    switch (m(self, 0)) {
    case 0: return fat(0x5ebed0);
    case 1: return fat(0x5ec3f4) - f32(m(self, 4)) / f32(length);
    case 2: return fat(0x5ec3f4);
    case 3: return f32(m(self, 4)) / f32(length);
    default: return 0.0L;
    }
}
// Toggle: in or shown slides out (no longer hidden by the inventory), else slides in
void THISCALL HudSlide_Toggle(BYTE *self)   // 0x5084a0
{
    const DWORD state = m(self, 0);
    if (state == 1 || state == 2) {
        mb(self, 0x14) = 0;
        slide(self, hud_options(), 1, 3, 0x20);
        return;
    }
    mb(self, 0x14) = 1;
    slide(self, hud_options(), 3, 1, 0x1c);
}
// Hidden by the inventory screen: in or shown slides out and remembers it (+0x14)
void THISCALL HudSlide_HideForInventory(BYTE *self)   // 0x508570
{
    const DWORD state = m(self, 0);
    const BYTE was = state == 1 || state == 2;
    mb(self, 0x14) = 0;
    if (state != 3 && state != 0)
        slide(self, hud_options(), 1, 3, 0x20);
    mb(self, 0x14) = was;
}
// Back after the inventory screen: slides in again when it was hidden by it, else out unless already out
void THISCALL HudSlide_ShowAfterInventory(BYTE *self)   // 0x5085f0
{
    const DWORD state = m(self, 0);
    if (mb(self, 0x14)) {
        mb(self, 0x14) = 1;
        if (state == 1 || state == 2)
            return;
        slide(self, hud_options(), 3, 1, 0x1c);
        return;
    }
    mb(self, 0x14) = 0;
    if (state == 3 || state == 0)
        return;
    slide(self, hud_options(), 1, 3, 0x20);
}
// The materials generator's window (0x6c2cfc) told to close (+0x13d)
void __cdecl MaterialsGenerator_RequestClose()   // 0x505370
{
    if (DWORD w = gd(glob_addr(0x6c2cfc)))
        mb(bp(w), 0x13d) = 1;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_inventory)
{
    const Hook list[] = {
#include "ui_inventory_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
