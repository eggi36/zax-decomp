// The game's menus outside and inside a level (docs/DECOMPILATION.md 9, "The game's screens and menus"):
// the menu dialog base, the main menu ("~CMainMenu"), "Select Difficulty", the game menu ("Zax Menu"), the
// "Configure Menu" (QuitMenu in the tables, a misnomer: batch gaps-menu), the credits, "You Have Died", the chat
// window and the "Select Map" dialog's destructors and Ok.
// They are dialogs of the window toolkit (ui_frame.cpp: CDialog 0x403d00, CDialog2 0x403ab0) built from its
// buttons, labels and sequence windows; every call into the toolkit or the rest of the game goes through
// its exe address.
//
// Layouts [confirmed: the constructors and destructors below]:
//   menu dialog (0x138 bytes, vtable 0x604fc0, a CDialog): +0x134 a "Background"-like sequence of the title
//     exists ("<title>" in the resource list 0x713380), shown behind the dialog
//   main menu (0x158, vtable 0x6055d4, a CDialog2; g_MainMenu 0x714558): +0x134 the background window
//     (0x14c: a sequence window with a CFrame at +0x10c, vtable 0x6054a4), +0x138 … +0x14c the six buttons
//     (Start Game 0x7144f8, 0x71451c, 0x714514, 0x714520, 0x71453c, 0x714554: registered Local.txt strings),
//     +0x150 the CD key was checked, +0x154 the version label
//   difficulty menu (0x150, vtable 0x60596c): +0x138 a pointer array of the difficulty buttons (vtable
//     0x605aa0: +0x13c data, +0x140 count, +0x144 capacity), +0x148 the label, +0x14c Back
//   Zax menu (0x154, vtable 0x605ab8): +0x138 … +0x150 the buttons (0x148 the multiplayer one or null)
//   "Configure Menu" (QuitMenu, 0x14c, vtable 0x605bec): +0x138 … +0x148 the buttons (+0x144 or null)
//   credits (0x140, vtable 0x605d20): +0x138 the credits control (0x108, "~CJCreditsControl", vtable
//     0x605708: +0x100 the credits text (0x20, 0x475eb0), +0x104 scrolls), +0x13c the button
//   "You Have Died" (0x13c, vtable 0x605e54): +0x138 "Restart Map"
//   chat window (0x104, "~ChatWindow", vtable 0x605f88; open flag 0x714560): +0x100 the edit field
//   "Select Map" (0x14c, vtable 0x605234, a file dialog 0x414910 with vtable 0x6050f4 in between): +0x144 the
//     game type list, +0x148 its name list (deleted by the destructor)
#include "ui_window.h"
#include "standalone_launcher.h"

namespace zax::ui {

namespace {

// Globals and objects of the game
constexpr DWORD G_ERROR_REPORTER = 0x6c02cc;   // the application's reporter: slots 28/29 open/close a menu, +4 the screen
constexpr DWORD G_APP = 0x713f14, G_WORLD = 0x6c2080, G_MAIN_MENU = 0x714558, G_DEMO = 0x62485c;
constexpr DWORD G_CHAT_OPEN = 0x714560;
constexpr DWORD FONT_LIST = 0x6be1e0, FONT_RECORDS = 0x6be210, FONT_COUNT = 0x6be214;
constexpr DWORD DIFFICULTY_LIST = 0x6c2a00, DIFFICULTY_COUNT = 0x6c2a14;
constexpr int R_OPEN = 28, R_CLOSE = 29, SCREEN_HEIGHT = 39, SCREEN_WIDTH = 45;
constexpr int LABEL_SET_TEXT = 43, LABEL_SET_FONT = 46, BUTTON_SET_ACCEL = 40;
constexpr DWORD ZAX_MENU_LAUNCHER_SITE = 0x5bfe14, MAIN_MENU_CALL = 0x5bff30;   // patched by the launcher
constexpr DWORD NOWHERE = 0xfffffc18;   // -1000: AddChild's "keep the coordinate"

BYTE *reporter() { return bp(gd(glob_addr(G_ERROR_REPORTER))); }
BYTE *screen() { return bp(m(reporter(), 4)); }
BYTE *alloc(DWORD n) { return static_cast<BYTE *>(pool_alloc(n)); }
const char *c_str(DWORD zstring) { return fn<const char *(THISCALL *)(DWORD)>(0x4e13a0)(glob_addr_dyn(zstring)); }

void add_child_front(BYTE *self, BYTE *child, DWORD how, DWORD a, DWORD x, DWORD y, DWORD b)
{
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40c8d0)(self, child, how, a, x, y, b);
}
void add_child(BYTE *self, BYTE *child, DWORD how, DWORD a, DWORD x, DWORD y, DWORD b)
{
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40c7c0)(self, child, how, a, x, y, b);
}
void set_width(BYTE *w, DWORD v) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x40f5b0)(w, v); }
void set_height(BYTE *w, DWORD v) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x40f5d0)(w, v); }
void set_left(BYTE *w, int v) { fn<void(THISCALL *)(BYTE *, int)>(0x40d680)(w, v); }
void move_to(BYTE *w, int x, int y) { fn<void(THISCALL *)(BYTE *, int, int)>(0x40d630)(w, x, y); }
void same_width(BYTE *first, BYTE *last) { fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40d230)(first, last); }
void focus(BYTE *self, BYTE *child) { fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40ca40)(self, child); }
void dialog_dtor(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x403d70)(self); }
void window_dtor(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x40b190)(self); }
void add_close_button(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self); }
void notify(BYTE *self, BYTE *from, DWORD what) { fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4035f0)(self, from, what); }
void hand_over(BYTE *self, BYTE *to, DWORD a) { fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x404000)(self, to, a); }
void replace_window(BYTE *self, BYTE *to, DWORD a) { fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4040c0)(self, to, a); }
void set_replaced(BYTE *self, DWORD v) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x404180)(self, v); }
void pool_delete(BYTE *p, DWORD size) { fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(p, size); }

// The constructors of the toolkit's windows (null when the pool gives no block)
BYTE *new_button(DWORD size, BYTE *parent, DWORD zstring)   // Button_ctor 0x40fa60 (text a ZString)
{
    BYTE *b = alloc(size);
    return b ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x40fa60)(b, parent, zstring, 0) : nullptr;
}
BYTE *new_button_text(BYTE *parent, const char *text)   // Button::Button 0x40f9b0 (a C string)
{
    BYTE *b = alloc(0x138);
    return b ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(b, parent, text, 0) : nullptr;
}
BYTE *sequence_window(BYTE *w, BYTE *parent, const char *name)   // SequenceWindow_Ctor 0x56c400
{
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x56c400)(w, parent, name, 0);
}
BYTE *new_sequence_window(BYTE *parent, const char *name)
{
    BYTE *w = alloc(0x10c);
    return w ? sequence_window(w, parent, name) : nullptr;
}
BYTE *text_label(BYTE *w, BYTE *parent, const char *text) { return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(w, parent, text); }
BYTE *window_ctor(BYTE *w, BYTE *parent, const char *name) { return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(w, parent, name); }

// The font of a font list index (the inlined record lookup of the resource lists: index 0 when out of range,
// null when the list is empty or the record is being loaded, loaded on first use)
DWORD font_at(DWORD index)
{
    if (!gd(glob_addr(FONT_RECORDS)))
        return 0;
    if (index >= gd(glob_addr(FONT_COUNT)))
        index = 0;
    const DWORD rec = index << 7;
    if (gb(gd(glob_addr(FONT_RECORDS)) + rec + 0x7f))
        return 0;
    if (!m(bp(gd(glob_addr(FONT_RECORDS))), rec + 0x78))
        fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(glob_addr(FONT_LIST), index);
    return m(bp(gd(glob_addr(FONT_RECORDS))), rec + 0x78);
}

}  // namespace

// ---- The menu dialog base -------------------------------------------------------------------------------------

// The constructor (the parent, the title): a CDialog with the vtable 0x604fc0, the debug section "Open Menu" and
// the reporter's slot 28; when the resource list 0x713380 has a sequence of the title's name, +0x134 is set and a
// sequence window of it goes in front of the children (AddChildFront 1, 0, -1000, -1000, 0)
BYTE *THISCALL MenuDialog_Ctor(BYTE *self, BYTE *parent, const char *title)   // 0x5bdc90
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x403d00)(self, parent, title);
    m(self, 0) = vt(0x604fc0);
    fn<void(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(0x627598));   // "Open Menu"
    vc(reporter(), R_OPEN);
    mb(self, 0x134) = 0;
    const char *name = c_str(pv(self + W_TITLE));
    if (fn<int(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(0x713380), name, -1) == -1)
        return self;
    mb(self, 0x134) = 1;
    BYTE *w = alloc(0x10c);
    BYTE *seq = nullptr;
    if (w)
        seq = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x56c400)(
            w, self, c_str(pv(self + W_TITLE)), 0);
    add_child_front(self, seq, 1, 0, NOWHERE, NOWHERE, 0);
    return self;
}

// The deleting destructors of the menu dialogs (the base's vtable, the reporter's slot 29, the debug section's
// end, CDialog's destructor; flag 1 frees the size of the class)
template <DWORD SIZE> BYTE *THISCALL MenuDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bdd40 and the menus'
{
    m(self, 0) = vt(0x604fc0);
    vc(reporter(), R_CLOSE);
    fn<void(__cdecl *)()>(0x4a2670)();
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, SIZE);
    return self;
}

// ---- "Select Map" -------------------------------------------------------------------------------------------

// The file dialog part's deleting destructor (vtable 0x6050f4, the reporter's slot 29, the file dialog's
// destructor 0x414bc0)
BYTE *THISCALL SelectMapBase_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bddd0
{
    m(self, 0) = vt(0x6050f4);
    vc(reporter(), R_CLOSE);
    fn<void(THISCALL *)(BYTE *)>(0x414bc0)(self);
    if (flags & 1)
        pool_delete(self, 0x144);
    return self;
}
// "Select Map" itself: its name list +0x148 deleted (slot 0 with 1) first
BYTE *THISCALL SelectMap_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bde10
{
    m(self, 0) = vt(0x605234);
    if (BYTE *list = bp(m(self, 0x148))) {
        vc(list, 0, 1);
        m(self, 0x148) = 0;
    }
    m(self, 0) = vt(0x6050f4);
    vc(reporter(), R_CLOSE);
    fn<void(THISCALL *)(BYTE *)>(0x414bc0)(self);
    if (flags & 1)
        pool_delete(self, 0x14c);
    return self;
}
// Ok (the chosen map or 0): the dialog is no longer replaced, the world's object's slot 10, the game type of
// the list's choice (slot 76 of +0x144, the name list's value), the world's slots 80 and 79 (1, the map, 0, 0)
// start it; then Close (slot 5)
void THISCALL SelectMap_OnOk(BYTE *self, DWORD map)   // 0x5be050
{
    if (map) {
        set_replaced(self, 0);
        vc(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 1), 10);
        const DWORD choice = vc(bp(m(self, 0x144)), 76);
        const DWORD type = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x592310)(m(self, 0x148), choice);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x5a32b0)(gd(glob_addr(G_APP)), type);
        vc(bp(gd(glob_addr(G_WORLD))), 80);
        vc(bp(gd(glob_addr(G_WORLD))), 79, 1, map, 0, 0);
    }
    vc(self, V_CLOSE);
}

// The main menu's buttons after the first: member, text
struct MainMenuButton {
    DWORD member;
    GlobKey text;
};
constexpr MainMenuButton MAIN_MENU_BUTTONS[4] = {{0x13c, 0x71451c}, {0x140, 0x714514}, {0x144, 0x714520}, {0x148, 0x71453c}};
// The base constructor through its exe address
static BYTE *menu_dialog(BYTE *self, BYTE *parent, const char *title)
{
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x5bdc90)(self, parent, title);
}

// ---- The main menu --------------------------------------------------------------------------------------------

// The background window of the main menu: a sequence window "Background" with a CFrame at +0x10c (vtable
// 0x6054a4), 800 × 600 at (0, 0) when the screen is 800 wide
static BYTE *main_menu_background(BYTE *self)
{
    BYTE *w = alloc(0x14c);
    if (!w)
        return nullptr;
    sequence_window(w, self, glob<const char *>(0x624d14));
    fn<void(THISCALL *)(BYTE *)>(0x4a8a90)(w + 0x10c);
    m(w, 0) = vt(0x6054a4);
    const DWORD width = vc(screen(), SCREEN_WIDTH);
    if (width == 800) {
        set_width(w, width);
        set_height(w, 600);
        move_to(w, 0, 0);
    }
    return w;
}
// A fire of the main menu: a sequence window with its position (+0x10c/+0x110, the option's point + (80, 60))
static BYTE *main_menu_fire(BYTE *self, const char *name, int x, int y)
{
    BYTE *w = alloc(0x114);
    if (!w)
        return nullptr;
    sequence_window(w, self, name);
    m(w, 0x10c) = (DWORD)(x + 0x50);
    m(w, 0) = vt(0x605374);
    m(w, 0x110) = (DWORD)(y + 0x3c);
    return w;
}

// The constructor (the parent): "~CMainMenu" over the Rules.txt options (Main Menu Display): the background (at
// (80, 60) when the screen is 640 wide, else (0, 0)), the two fires, the version label (the version font, the
// full version text), "Charms" and "Skull", a 640 × 480 window "~C Z Order Hack" at (80, 60) holding the six
// buttons (the first at the option's First Button Pos y, the rest below, all as wide as the widest and centred on
// its x; the last centred under the fifth), the font, the look 0x4e3680; the first button has the focus
BYTE *THISCALL MainMenu_Ctor(BYTE *self, BYTE *parent)   // 0x5be4a0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x403ab0)(self, parent, glob<const char *>(0x627650));
    m(self, 0) = vt(0x6055d4);
    mb(self, 0x150) = 0;
    gd(glob_addr(G_MAIN_MENU)) = pv(self);
    BYTE *holder = bp(m(bp(gd(glob_addr(G_APP))), 0x2e0));
    BYTE *rules = nullptr;
    if (m(holder, 0x38)) {
        const DWORD c = fn<DWORD(__cdecl *)()>(0x5a1350)();
        if (fn<bool(THISCALL *)(DWORD, DWORD)>(0x416790)(m(holder, 0x38), c))
            rules = bp(m(holder, 0x38));
    }
    BYTE *opt = fn<BYTE *(THISCALL *)(BYTE *)>(0x5a1ee0)(rules);
    m(self, W_FONT) = font_at(m(opt, 0x18));
    vc(reporter(), R_OPEN);
    mb(self, W_BORDER) = 0;
    const bool small = vc(screen(), SCREEN_WIDTH) == 0x280;
    BYTE *bg = main_menu_background(self);
    m(self, 0x134) = pv(bg);
    add_child_front(self, bg, 5, 0, small ? 0x50 : 0, small ? 0x3c : 0, 2);
    BYTE *fire = main_menu_fire(self, glob<const char *>(0x624b80), mi(opt, 8), mi(opt, 0xc));
    add_child_front(self, fire, 5, 0, m(opt, 8) + 0x50, m(opt, 0xc) + 0x3c, 2);
    fire = main_menu_fire(self, glob<const char *>(0x624b8c), mi(opt, 0x10), mi(opt, 0x14));
    add_child_front(self, fire, 5, 0, m(opt, 0x10) + 0x50, m(opt, 0x14) + 0x3c, 2);
    BYTE *label = alloc(0x128);
    if (label)
        label = text_label(label, nullptr, glob<const char *>(0x623890));
    m(self, 0x154) = pv(label);
    vc(bp(m(self, 0x154)), LABEL_SET_FONT, font_at(m(opt, 0x30)));
    DWORD version;
    fn<void(THISCALL *)(DWORD *)>(0x4dec90)(&version);
    fn<void(THISCALL *)(DWORD *)>(0x5908f0)(&version);
    vc(bp(m(self, 0x154)), LABEL_SET_TEXT, c_str(pv(&version)));
    add_child_front(self, bp(m(self, 0x154)), 4, 0, NOWHERE, NOWHERE, 0);
    add_child_front(self, new_sequence_window(self, glob<const char *>(0x624d0c)), 5, 0, 0x50, 0x3c, 2);   // "Charms"
    add_child_front(self, new_sequence_window(self, glob<const char *>(0x624d04)), 5, 0, 0x50, 0x3c, 2);   // "Skull"
    BYTE *hack = alloc(0x100);
    if (hack)
        hack = window_ctor(hack, self, glob<const char *>(0x627640));
    set_width(hack, 0x280);
    set_height(hack, 0x1e0);
    mb(hack, W_BORDER2) = 0;
    mb(hack, W_BORDER) = 0;
    mb(self, W_BORDER2) = 0;
    add_child_front(self, hack, 5, 0, 0x50, 0x3c, 2);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, 800, 600);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b5c0)(self, fn<DWORD(__cdecl *)()>(0x4e3680)());
    BYTE *b = new_button(0x138, self, glob_addr(0x7144f8));
    m(self, 0x138) = pv(b);
    vc(hack, V_ADD_CHILD, b, 5, 0, 0, m(opt, 0x2c), 2);
    for (const auto &e : MAIN_MENU_BUTTONS) {
        b = new_button(0x138, self, key_glob_addr(e.text));
        m(self, e.member) = pv(b);
        vc(hack, V_ADD_CHILD, b, 8, 0, NOWHERE, NOWHERE, 0);
    }
    same_width(bp(m(self, 0x138)), bp(m(self, 0x148)));
    for (BYTE *w = bp(m(self, 0x138)); w; w = bp(m(w, W_NEXT)))
        set_left(w, mi(opt, 0x28) - (mi(w, W_RECT + 8) - mi(w, W_RECT)) / 2);
    b = new_button(0x138, self, glob_addr(0x714554));
    m(self, 0x14c) = pv(b);
    vc(hack, V_ADD_CHILD, b, 8, 0, 0, 4, 0);
    BYTE *fifth = bp(m(self, 0x148)), *last = bp(m(self, 0x14c));
    const int l5 = mi(fifth, W_RECT);
    set_left(last, (mi(fifth, W_RECT + 8) - mi(last, W_RECT + 8) - l5 + mi(last, W_RECT)) / 2 + l5);
    m(self, 0x11c) = m(self, 0x138);
    vc(hack, V_FOCUS, m(self, 0x138));
    fn<void(THISCALL *)(DWORD *)>(0x4defd0)(&version);
    return self;
}

// The background window's deleting destructor (its CFrame, CJWindow's destructor)
BYTE *THISCALL MainMenuBackground_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bec40
{
    fn<void(THISCALL *)(BYTE *)>(0x4a8b00)(self + 0x10c);
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x14c);
    return self;
}
// The fire's deleting destructor
BYTE *THISCALL MainMenuFire_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bed70
{
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x114);
    return self;
}
// The main menu's deleting destructor: the reporter's slot 29, g_MainMenu cleared
BYTE *THISCALL MainMenu_DeletingDtor(BYTE *self, DWORD flags)   // 0x5beda0
{
    m(self, 0) = vt(0x6055d4);
    vc(reporter(), R_CLOSE);
    gd(glob_addr(G_MAIN_MENU)) = 0;
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x158);
    return self;
}
// The end game credits dialog's deleting destructor (0x13c bytes)
BYTE *THISCALL EndCredits_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bf200
{
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x13c);
    return self;
}
// Slot 2, the update (the argument): the first time, unless it is the demo (0x62485c) or the key is
// valid, the CD key dialog goes modal over the menu (AddModalToParent 6); then CJWindow's update
void THISCALL MainMenu_Update(BYTE *self, DWORD a)   // 0x5bf230
{
    if (!mb(self, 0x150)) {
        mb(self, 0x150) = 1;
        if (!gb(glob_addr(G_DEMO)) && !fn<bool(__cdecl *)()>(0x46b6d0)()) {
            BYTE *d = alloc(0x14c);
            if (d)
                d = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x46b050)(d, 0);
            fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40c6e0)(self, d, 6);
        }
    }
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b5d0)(self, a);
}

// ---- "Select Difficulty" --------------------------------------------------------------------------------------

namespace {
// The local index array of the constructor (vtable 0x5ee138 while alive, 0x5ee15c when destroyed)
struct IndexArray {
    DWORD vtable;
    DWORD *data;
    DWORD count, capacity;
};
}  // namespace

// The constructor (the parent): a menu dialog titled 0x7144fc with a close button, the label 0x7144dc, one button
// per difficulty (its definition's name), added in the order of the comparator 0x5bf690 (qsort of their
// indices), all as wide as the first and the last, centred; "Back" 0x7144d8 at the bottom (AddChild 12, 0, 0, 12);
// the middle one of the sorted buttons has the focus. With no difficulties the original reads before its array.
BYTE *THISCALL DifficultyMenu_Ctor(BYTE *self, BYTE *parent)   // 0x5bf820
{
    menu_dialog(self, parent, c_str(glob_addr(0x7144fc)));
    m(self, 0x138) = vt(0x605aa0);
    m(self, 0x13c) = 0, m(self, 0x140) = 0, m(self, 0x144) = 0;
    m(self, 0) = vt(0x60596c);
    add_close_button(self);
    BYTE *label = alloc(0x128);
    if (label)
        label = text_label(label, self, c_str(glob_addr(0x7144dc)));
    m(self, 0x148) = pv(label);
    add_child(self, label, 1, 0, NOWHERE, NOWHERE, 0);
    const DWORD n = gd(glob_addr(DIFFICULTY_COUNT));
    IndexArray order{vt(0x5ee138), nullptr, 0, 0};
    if (n) {
        for (DWORD i = 0; i < n; i++) {
            BYTE *b = alloc(0x138);
            if (b) {
                BYTE *def = fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(DIFFICULTY_LIST), i);
                const char *name = fn<const char *(THISCALL *)(BYTE *)>(0x5007a0)(def + 8);
                b = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(b, self, name, 0);
            }
            vc(self + 0x138, 2, &b);
            fn<void(THISCALL *)(IndexArray *, DWORD)>(0x5c11c0)(&order, order.count + 1);
            order.data[order.count] = i;
            order.count++;
        }
        if (order.count)
            fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(order.data, order.count, 4, fn_addr(0x5bf690));
    }
    BYTE **buttons = reinterpret_cast<BYTE **>(bp(m(self, 0x13c)));
    for (DWORD i = 0; i < n; i++)
        add_child(self, buttons[order.data[i]], 8, 0, NOWHERE, NOWHERE, 0);
    buttons = reinterpret_cast<BYTE **>(bp(m(self, 0x13c)));
    same_width(buttons[order.data[0]], buttons[order.data[m(self, 0x140) - 1]]);
    buttons = reinterpret_cast<BYTE **>(bp(m(self, 0x13c)));
    BYTE *first = buttons[0];
    const int x = (mi(self, W_RECT + 8) - mi(first, W_RECT + 8) - mi(self, W_RECT) + mi(first, W_RECT)) / 2;
    for (DWORD i = 0; i < n; i++)
        set_left(reinterpret_cast<BYTE **>(bp(m(self, 0x13c)))[i], x);
    BYTE *back = new_button(0x138, self, glob_addr(0x7144d8));
    m(self, 0x14c) = pv(back);
    add_child(self, back, 0xc, 0, 0, 0xc, 0);
    m(self, 0x120) = m(self, 0x14c);
    BYTE *mid = reinterpret_cast<BYTE **>(bp(m(self, 0x13c)))[order.data[(n - 1) >> 1]];
    m(self, 0x11c) = pv(mid);
    focus(self, mid);
    order.vtable = vt(0x5ee15c);
    if (order.data)
        pool_get_free(order.data, order.capacity << 2);
    return self;
}

// The deleting destructor: the button array's block back to the pool (the array's own deleting destructor
// 0x5bfb20 is ZPtrArray's, ptr_array.h), then the menu dialog's
BYTE *THISCALL DifficultyMenu_DeletingDtor(BYTE *self, DWORD flags)   // 0x5bfa90
{
    m(self, 0x138) = vt(0x605aac);
    if (m(self, 0x13c)) {
        pool_get_free(bp(m(self, 0x13c)), m(self, 0x144) << 2);
        m(self, 0x13c) = 0;
        m(self, 0x144) = 0;
    }
    m(self, 0x140) = 0;
    return MenuDialog_DeletingDtor<0x150>(self, flags);
}
// Slot 21, a child's notification (the child, what): a click (0) on Back closes; on a difficulty button the save
// data's difficulty, the quest game type "QuestMode", the music stops, the movie "Intro", the world's slot 79
// (1, 0, 0, 0) starts the game, the dialog closes; else the frame window's
void THISCALL DifficultyMenu_Notify(BYTE *self, BYTE *from, DWORD what)   // 0x5bfb70
{
    if (!what) {
        if (pv(from) == m(self, 0x14c)) {
            vc(self, V_CLOSE);
            return;
        }
        const DWORD n = gd(glob_addr(DIFFICULTY_COUNT));
        const DWORD *buttons = reinterpret_cast<const DWORD *>(bp(m(self, 0x13c)));
        for (DWORD i = 0; i < n; i++)
            if (pv(from) == buttons[i]) {
                fn<void(THISCALL *)(DWORD, DWORD)>(0x5bb780)(m(bp(gd(glob_addr(G_APP))), 0x34c), i);
                fn<void(THISCALL *)(DWORD, const char *)>(0x5a3180)(gd(glob_addr(G_APP)), glob<const char *>(0x6179f0));
                fn<void(THISCALL *)(DWORD)>(0x51e450)(m(bp(gd(glob_addr(G_WORLD))), 0x334));
                fn<void(THISCALL *)(DWORD, const char *, DWORD, DWORD)>(0x4cd5a0)(gd(glob_addr(G_WORLD)), glob<const char *>(0x6276c4), 0, 1);
                vc(bp(gd(glob_addr(G_WORLD))), 79, 1, 0, 0, 0);
                set_replaced(self, 0);
                vc(self, V_CLOSE);
                return;
            }
    }
    notify(self, from, what);
}

// ---- The game menu ("Zax Menu") -------------------------------------------------------------------------------

// The constructor (the parent): a menu dialog titled 0x7144e8 with a close button; the buttons 0x71453c (centred
// when the menu has its background sequence), 0x714548, 0x71452c unless the world's object's slot 13 says so,
// the multiplayer button (the game type's slot 37: the text 0x7144f0 formatted with 0x7144e0), 0x714500 (y 6),
// all as wide as the first and that one; 0x7144ec at the bottom (12, 0, 8) with the focus
BYTE *THISCALL ZaxMenu_Ctor(BYTE *self, BYTE *parent)   // 0x5bfc40
{
    // Zax-Launcher.dll puts a jump at 0x5bfe14 (the "End Game" button's allocation) to add its "Load Map"
    // button (zax-launcher/zax_launcher_dll.c): the game then keeps the original with that patch
#ifndef ZAX_STANDALONE
    if (gb(ZAX_MENU_LAUNCHER_SITE) != 0xb9)
        return original<BYTE *(THISCALL *)(BYTE *, BYTE *)>(fn_exe(0x5bfc40))(self, parent);
#endif
    menu_dialog(self, parent, c_str(glob_addr(0x7144e8)));
    m(self, 0) = vt(0x605ab8);
    add_close_button(self);
    m(self, 0x148) = 0;
    BYTE *b = new_button(0x138, self, glob_addr(0x71453c));
    m(self, 0x138) = pv(b);
    add_child(self, b, 1, 0, NOWHERE, NOWHERE, 0);
    if (mb(self, 0x134)) {
        BYTE *f = bp(m(self, 0x138));
        set_left(f, (mi(self, W_RECT + 8) - mi(f, W_RECT + 8) - mi(self, W_RECT) + mi(f, W_RECT)) / 2);
    }
    b = new_button(0x138, self, glob_addr(0x714548));
    m(self, 0x140) = pv(b);
    add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
    if (!(vc(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 1), 13) & 0xff)) {
        b = new_button(0x138, self, glob_addr(0x71452c));
        m(self, 0x144) = pv(b);
        add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
    }
    if (fn<DWORD(THISCALL *)(DWORD)>(0x59ff80)(gd(glob_addr(G_APP)))) {
        BYTE *type = fn<BYTE *(THISCALL *)(DWORD)>(0x59ff80)(gd(glob_addr(G_APP)));
        if (vc(type, 37) & 0xff) {
            DWORD text;
            fn<void(THISCALL *)(DWORD *)>(0x4dec90)(&text);
            const char *format = c_str(glob_addr(0x7144f0));
            fn<void(__cdecl *)(DWORD *, const char *, const char *)>(0x4e09b0)(&text, format, c_str(glob_addr(0x7144e0)));
            b = alloc(0x138);
            if (b)
                b = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD *, DWORD)>(0x40fa60)(b, self, &text, 0);
            m(self, 0x148) = pv(b);
            add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
            fn<void(THISCALL *)(DWORD *)>(0x4defd0)(&text);
        }
    }
#ifdef ZAX_STANDALONE
    launcher::add_load_map(self);   // where Zax-Launcher.dll jumps in at 0x5bfe14 (standalone/launcher.cpp, batch 6-10c)
#endif
    b = new_button(0x138, self, glob_addr(0x714500));
    m(self, 0x14c) = pv(b);
    add_child(self, b, 8, 0, 6, NOWHERE, 0);
    same_width(bp(m(self, 0x138)), bp(m(self, 0x14c)));
    b = new_button(0x138, self, glob_addr(0x7144ec));
    m(self, 0x150) = pv(b);
    add_child(self, b, 0xc, 0, 8, NOWHERE, 0);
    m(self, 0x11c) = m(self, 0x150);
    m(self, 0x120) = m(self, 0x150);
    focus(self, bp(m(self, 0x150)));
    return self;
}

// Slot 21 (the child, what): on a click, +0x14c hands over to a new main menu and leaves the level (the world's
// object's slot 10, the world's slot 81 with 1); +0x148 sends "continue" (0x5ac4a0, 0x5ac500) and closes; +0x150
// closes; +0x13c opens "Select Map" over "Levels/*.zax" (its slot 77 with 1, the file spec, 0); +0x140 and +0x144
// replace the menu by the save (1) or load (0) screen 0x562970; +0x138 hands over to the Configure
// Menu (its last button 0x7144ec); else the frame window's
void THISCALL ZaxMenu_Notify(BYTE *self, BYTE *from, DWORD what)   // 0x5bff00
{
    if (what) {
        notify(self, from, what);
        return;
    }
    const DWORD f = pv(from);
    if (f == m(self, 0x14c)) {
        BYTE *menu = alloc(0x158);
#ifdef ZAX_STANDALONE
        if (menu)   // through the function table, where the built-in launcher's redirection is (batch 6-10c)
            menu = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x5be4a0)(menu, self);
#else
        if (menu)   // through the target of the exe's call 0x5bff30, which the launcher redirects
            menu = at<BYTE *(THISCALL *)(BYTE *, BYTE *)>(MAIN_MENU_CALL + 5 + m(bp(MAIN_MENU_CALL), 1))(menu, self);
#endif
        hand_over(self, menu, 1);
        vc(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 1), 10);
        vc(bp(gd(glob_addr(G_WORLD))), 81, 1);
        return;
    }
    if (f == m(self, 0x148) && m(self, 0x148)) {
        fn<void(THISCALL *)(DWORD)>(0x5ac500)(fn<DWORD(__cdecl *)()>(0x5ac4a0)());
        vc(self, V_CLOSE);
        return;
    }
    if (f == m(self, 0x150)) {
        vc(self, V_CLOSE);
        return;
    }
    if (f == m(self, 0x13c)) {
        BYTE spec[0x2c];
        fn<void(THISCALL *)(BYTE *)>(0x4a09f0)(spec);
        fn<void(THISCALL *)(BYTE *, const char *, const char *)>(0x4a0a60)(spec, glob<const char *>(0x615d48), glob<const char *>(0x615d38));
        BYTE *d = alloc(0x14c);
        if (d) {
            fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x414910)(d, self, glob<const char *>(0x6275a4), 0);
            m(d, 0) = vt(0x6050f4);
            vc(reporter(), R_OPEN);
            m(d, 0) = vt(0x605234);
        }
        vc(d, 77, 1, spec, 0);
        replace_window(self, d, 1);
        for (int i = 4; i >= 0; i--)
            fn<void(THISCALL *)(BYTE *)>(0x4defd0)(spec + 0x18 + 4 * i);
        return;
    }
    if (f == m(self, 0x140) || f == m(self, 0x144)) {
        BYTE *screen = alloc(0x1a4);
        if (screen)
            screen = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x562970)(screen, self, f == m(self, 0x140) ? 1 : 0);
        replace_window(self, screen, 1);
        return;
    }
    if (f == m(self, 0x138)) {
        BYTE *quit = alloc(0x14c);
        if (quit)
            quit = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x5c0220)(quit, self, c_str(glob_addr(0x7144ec)));
        hand_over(self, quit, 1);
        return;
    }
    notify(self, from, what);
}

// ---- "Configure Menu" (QuitMenu) ------------------------------------------------------------------------------

// The constructor (the parent, the text of the last button): a menu dialog titled 0x714530 ("Configure Menu"; the
// game has no quit menu: the main menu's E&xit closes the window, ui_c3_menus.cpp) with a close button; the buttons
// 0x7144d0 (&Controls), 0x7144f4 (&Video), 0x714544 (Multi-&Player; not in the demo, and only from the main menu or
// when the application's object's slot 13 says so), 0x71450c (&Sound), all as wide as the first and that one; the
// last button (the text) at the bottom (12, 0, 6) with the focus
BYTE *THISCALL QuitMenu_Ctor(BYTE *self, BYTE *parent, const char *last)   // 0x5c0220
{
    menu_dialog(self, parent, c_str(glob_addr(0x714530)));
    m(self, 0) = vt(0x605bec);
    m(self, 0x144) = 0;
    add_close_button(self);
    BYTE *b = new_button(0x138, self, glob_addr(0x7144d0));
    m(self, 0x13c) = pv(b);
    add_child(self, b, 1, 0, NOWHERE, NOWHERE, 0);
    b = new_button(0x138, self, glob_addr(0x7144f4));
    m(self, 0x138) = pv(b);
    add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
    if (!gb(glob_addr(G_DEMO)) && (gd(glob_addr(G_MAIN_MENU)) || (vc(vc<BYTE *>(bp(gd(glob_addr(G_APP))), 1), 13) & 0xff))) {
        b = new_button(0x138, self, glob_addr(0x714544));
        m(self, 0x144) = pv(b);
        add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
    }
    b = new_button(0x138, self, glob_addr(0x71450c));
    m(self, 0x140) = pv(b);
    add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
    same_width(bp(m(self, 0x13c)), bp(m(self, 0x140)));
    b = new_button_text(self, last);
    m(self, 0x148) = pv(b);
    add_child(self, b, 0xc, 0, 6, NOWHERE, 0);
    m(self, 0x11c) = m(self, 0x148);
    m(self, 0x120) = m(self, 0x148);
    focus(self, bp(m(self, 0x148)));
    return self;
}

// Slot 4 of a centred menu (the frame): centred on the screen (unsigned halves of the screen's size minus the
// window's), then the frame window's drawing
void THISCALL CentredMenu_Draw(BYTE *self, BYTE *frame)   // 0x5c0430
{
    const int w = mi(self, W_RECT + 8) - mi(self, W_RECT);
    set_left(self, (int)((vc(screen(), SCREEN_WIDTH) - (DWORD)w) >> 1));
    const int h = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
    fn<void(THISCALL *)(BYTE *, int)>(0x40d6c0)(self, (int)((vc(screen(), SCREEN_HEIGHT) - (DWORD)h) >> 1));
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x402f30)(self, frame);
}

// ---- The credits ----------------------------------------------------------------------------------------------

// The credits control's deleting destructor: its text (0x475fd0, operator delete)
BYTE *THISCALL CreditsControl_DeletingDtor(BYTE *self, DWORD flags)   // 0x5c05c0
{
    BYTE *text = bp(m(self, 0x100));
    m(self, 0) = vt(0x605708);
    if (text) {
        fn<void(THISCALL *)(BYTE *)>(0x475fd0)(text);
        heap_free(text);
        m(self, 0x100) = 0;
    }
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x108);
    return self;
}

// The constructor (the parent): a menu dialog titled 0x714504 with a close button; the credits control (the text
// of "ZaxCredits.txt" in its font, 200 × 100 font fifths, scrolling) and the button 0x714534 at the bottom (12, 0,
// 0, 8) with the accelerator Escape (slot 40 with 0x1b) and the focus; the keyboard focus flag 0x62e5e0 cleared
BYTE *THISCALL CreditsMenu_Ctor(BYTE *self, BYTE *parent)   // 0x5c07f0
{
    menu_dialog(self, parent, c_str(glob_addr(0x714504)));
    m(self, 0) = vt(0x605d20);
    add_close_button(self);
    BYTE *c = alloc(0x108);
    if (c) {
        window_ctor(c, self, glob<const char *>(0x627680));
        m(c, 0) = vt(0x605708);
        BYTE *text = static_cast<BYTE *>(heap_alloc(0x20));
        if (text)
            text = fn<BYTE *(THISCALL *)(BYTE *, const char *, DWORD)>(0x475eb0)(text, glob<const char *>(0x627670), m(c, W_FONT));
        m(c, 0x100) = pv(text);
        const DWORD f = m(bp(m(c, W_FONT)), 0x80) / 5;
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(c, f * 200, f * 100);
        mb(c, 0x104) = 1;
    }
    m(self, 0x138) = pv(c);
    add_child(self, c, 1, 0, NOWHERE, NOWHERE, 0);
    BYTE *b = new_button_text(self, c_str(glob_addr(0x714534)));
    m(self, 0x13c) = pv(b);
    add_child(self, b, 0xc, 0, 0, 8, 0);
    vc(bp(m(self, 0x13c)), BUTTON_SET_ACCEL, 0x1b);
    focus(self, bp(m(self, 0x13c)));
    gb(glob_addr(G_FOCUS_BY_KEY)) = 0;
    return self;
}

// ---- "You Have Died" ------------------------------------------------------------------------------------------

// The constructor (the parent): a menu dialog "You Have Died" with the button "Restart Map", enabled when the
// application's save data (+0x33c) has a restart point (+0x1c), and the default button
BYTE *THISCALL DiedMenu_Ctor(BYTE *self, BYTE *parent)   // 0x5c09a0
{
    menu_dialog(self, parent, glob<const char *>(0x62775c));
    m(self, 0) = vt(0x605e54);
    BYTE *b = new_button_text(self, glob<const char *>(0x627750));
    m(self, 0x138) = pv(b);
    add_child(self, b, 1, 0, NOWHERE, NOWHERE, 0);
    mb(bp(m(self, 0x138)), W_ENABLED) = m(bp(m(bp(gd(glob_addr(G_APP))), 0x33c)), 0x1c) != 0;
    m(self, 0x11c) = m(self, 0x138);
    return self;
}

// ---- The chat window ------------------------------------------------------------------------------------------

// Opens the chat window unless it is open: the application's slot 23 (the window or null, 0)
void __cdecl ChatWindow_Open()   // 0x5c1040
{
    if (gb(glob_addr(G_CHAT_OPEN)))
        return;
    BYTE *w = alloc(0x104);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *)>(0x5c1080)(w);
    vc(bp(gd(glob_addr(G_APP))), 23, w, 0);
}
// The constructor: "~ChatWindow" without a parent, the controls off (the world's slot 88's 0x46ff90), the open
// flag, an edit field (0x410b50, empty) 500 wide, at (10, 10); the typed characters cleared
BYTE *THISCALL ChatWindow_Ctor(BYTE *self)   // 0x5c1080
{
    window_ctor(self, nullptr, glob<const char *>(0x627808));
    m(self, 0) = vt(0x605f88);
    fn<void(THISCALL *)(BYTE *)>(0x46ff90)(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 88));
    gb(glob_addr(G_CHAT_OPEN)) = 1;
    BYTE *e = alloc(0x120);
    if (e)
        e = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x410b50)(e, self, glob<const char *>(0x62e558));
    m(self, 0x100) = pv(e);
    set_width(e, 0x1f4);
    add_child(self, bp(m(self, 0x100)), 1, 0, NOWHERE, NOWHERE, 0);
    move_to(self, 0xa, 0xa);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x4e2760)(glob_addr(G_KEYS), 0);
    return self;
}
// The deleting destructor: the controls on again (0x46ff70), the open flag cleared
BYTE *THISCALL ChatWindow_DeletingDtor(BYTE *self, DWORD flags)   // 0x5c1120
{
    m(self, 0) = vt(0x605f88);
    fn<void(THISCALL *)(BYTE *)>(0x46ff70)(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 88));
    gb(glob_addr(G_CHAT_OPEN)) = 0;
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x104);
    return self;
}

// ---- The file dialog -------------------------------------------------------------------------------------------

// The file dialog of the toolkit (0x144 bytes, vtable 0x5ec544, a CDialog; slot 78 the derived class's Ok with the
// chosen name or 0, slot 79 the window beside the list): +0x134 the list box of the names, +0x138 " &Open ",
// +0x13c "&Cancel", +0x140 the directory list (0x12c bytes, DirList 0x4a2c50)

// The constructor (the parent, the title, a font or null)
BYTE *THISCALL FileDialog_Ctor(BYTE *self, BYTE *parent, const char *title, DWORD font)   // 0x414910
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x403d00)(self, parent, title);
    m(self, 0) = vt(0x5ec544);
    if (font)
        m(self, W_FONT) = font;
    return self;
}
// The destructor: the directory list deleted (slot 0 with 1), CDialog's destructor (its eax)
DWORD THISCALL FileDialog_Dtor(BYTE *self)   // 0x414bc0
{
    m(self, 0) = vt(0x5ec544);
    if (BYTE *list = bp(m(self, 0x140))) {
        vc(list, 0, 1);
        m(self, 0x140) = 0;
    }
    return fn<DWORD(THISCALL *)(BYTE *)>(0x403d70)(self);
}
BYTE *THISCALL FileDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x414940
{
    m(self, 0) = vt(0x5ec544);
    if (BYTE *list = bp(m(self, 0x140))) {
        vc(list, 0, 1);
        m(self, 0x140) = 0;
    }
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x144);
    return self;
}

// Slot 77, the contents (by folders, the file spec, the third argument of the directory read): a close button;
// the directory list read (0x4a2ea0 (spec, 0, 0, the argument)); with names a list box of them (its own list
// source when by folders and the list's slot 11 gives one, else the directory list; 0xf423f rows, 1), 10 rows
// high (0x407870), beside the window of slot 79 (AddChild 7) or first (1); without "No Files Found"; " &Open "
// (enabled with names) at the bottom, two font fifths in from the left, and "&Cancel" right of it; Open is the
// default and Cancel the cancel button; the list has the focus when it is there
void THISCALL FileDialog_Run(BYTE *self, DWORD by_folders, DWORD spec, DWORD arg)   // 0x414990
{
    add_close_button(self);
    const DWORD beside = vc(self, 79);
    BYTE *dl = alloc(0x12c);
    if (dl)
        dl = fn<BYTE *(THISCALL *)(BYTE *)>(0x4a2c50)(dl);
    m(self, 0x140) = pv(dl);
    const DWORD n = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x4a2ea0)(dl, spec, 0, 0, arg);
    if (n) {
        BYTE *lb;
        if ((BYTE)by_folders && vc(bp(m(self, 0x140)), 11)) {
            lb = alloc(0x258);
            if (lb)
                lb = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x404520)(lb, self, vc(bp(m(self, 0x140)), 11), 0xf423f, 1);
        } else {
            lb = alloc(0x258);
            if (lb)
                lb = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x4041f0)(lb, self, m(self, 0x140), 0xf423f, 1);
        }
        m(self, 0x134) = pv(lb);
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x407870)(lb, 0xa);
        if (beside)
            vc(self, V_ADD_CHILD, m(self, 0x134), 7, beside, NOWHERE, NOWHERE, 0);
        else
            vc(self, V_ADD_CHILD, m(self, 0x134), 1, 0, NOWHERE, NOWHERE, 0);
    } else {
        BYTE *label = alloc(0x128);
        if (label)
            label = text_label(label, self, glob<const char *>(0x60a6a8));
        vc(self, V_ADD_CHILD, label, 1, 0, NOWHERE, NOWHERE, 0);
    }
    BYTE *open = new_button_text(self, glob<const char *>(0x60a6a0));
    m(self, 0x138) = pv(open);
    vc(self, V_ADD_CHILD, open, 0xc, 0, 6, NOWHERE, 0);
    open = bp(m(self, 0x138));
    const int fifths = (int)((m(bp(m(self, W_FONT)), 0x80) / 5) << 1);
    set_left(open, fifths - (mi(open, W_RECT + 8) - mi(open, W_RECT)) / 2 + mi(open, W_RECT));
    mb(bp(m(self, 0x138)), W_ENABLED) = n != 0;
    BYTE *cancel = new_button_text(self, glob<const char *>(0x60a698));
    m(self, 0x13c) = pv(cancel);
    vc(self, V_ADD_CHILD, cancel, 0xe, 0, NOWHERE, NOWHERE, 0);
    m(self, 0x120) = m(self, 0x13c);
    m(self, 0x11c) = m(self, 0x138);
    if (n)
        vc(self, V_FOCUS, m(self, 0x134));
}

// Slot 21 (the child, what): a click on Open or in the list gives the chosen name to slot 78 when the list's
// choice (slot 77) is below the directory list's count (slot 22); Cancel gives 0; else the frame window's
void THISCALL FileDialog_Notify(BYTE *self, BYTE *from, DWORD what)   // 0x414bf0
{
    if (!what) {
        if (pv(from) == m(self, 0x138) || pv(from) == m(self, 0x134)) {
            BYTE *lb = bp(m(self, 0x134));
            const DWORD count = vc(bp(m(self, 0x140)), 22);
            if (vc(lb, 77) < count) {
                const DWORD i = vc(bp(m(self, 0x134)), 77);
                vc(self, 78, fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4a3310)(m(self, 0x140), i));
            }
            return;
        }
        if (pv(from) == m(self, 0x13c)) {
            vc(self, 78, 0);
            return;
        }
    }
    notify(self, from, what);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_menus)
{
    const Hook list[] = {
#include "ui_menus_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
