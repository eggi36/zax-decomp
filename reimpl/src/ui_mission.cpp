// The mission over window and the mission details (docs/DECOMPILATION.md 9, "The rest of the user interface"):
// the window at the end of a mission (won, lost, multiplayer: 0x6cfd34 while it is open) with the mission
// details page in a scrolled window, its image scroll bar, a title, the buttons of its kind; the mission details
// window and the inventory screen's mission page. Every call into the toolkit or the rest of the game goes
// through its exe address.
//
// Layouts [confirmed: the constructors, destructors and slots below]:
//   mission over window (0x1b4, vtable 0x5fa064, a CDialog): +0x134 the text (ZString), +0x138 a value, +0x13c the
//     close button, +0x140 … +0x150 the kind's buttons, +0x154 … +0x160 the buttons laid out in a row, +0x164 the
//     options (CZaxCustomRules::MissionOverOptions / MultiPlayerMissionOverOptions), +0x168 the frame sequence,
//     +0x16c its rectangle, +0x17c the text area, +0x18c the title's font, +0x190 the title, +0x194/+0x198 its
//     position, +0x19c the details window, +0x1a0 the bar, +0x1a4/+0x1a8 the button images, +0x1ac the kind
//     (0, 1, 2 multiplayer), +0x1b0 continue sent
//   mission details window ("~CMissionDetailsWindow", 0x114, vtable 0x5fa198): +0x100 … +0x110 its parameters
//   mission display page ("~CMissionDisplayPage", 0x110, vtable 0x5fa2c8, a page window): +0x104 the bar,
//     +0x108 the details window, +0x10c shown
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr DWORD G_MISSION_OVER = 0x6cfd34;

BYTE *new_named_button(BYTE *self, BYTE *field)   // Button::Button with the name of a field descriptor
{
    BYTE *b = alloc(0x138);
    if (!b)
        return nullptr;
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x40f9b0)(b, self, fn<DWORD(THISCALL *)(BYTE *)>(0x5007a0)(field), 0);
}
BYTE *details_make(BYTE *w, BYTE *parent, DWORD a, BYTE b, DWORD c, DWORD d, DWORD e)
{
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x50fea0)(w, parent, a, b, c, d, e);
}

}  // namespace

// The constructor (the parent, a value, the kind, the text): the options of the kind, the frame sequence and its
// rectangle, the details window (scrolled by the image scroll bar with its images and colour), the window's size
// from the frame, the close button, the kind's buttons (0: two, 1: three, the second only with a save game,
// 2: two), the title and its font, the buttons centred in a row
BYTE *THISCALL MissionOver_Ctor(BYTE *self, DWORD parent, DWORD value, DWORD kind, DWORD text)   // 0x50cab0
{
    dialog_ctor(self, parent, nullptr);
    zs_ctor(self + 0x134);
    zs_ctor(self + 0x190);
    m(self, 0) = vt(0x5fa064);
    gd(glob_addr(G_MISSION_OVER)) = pv(self);
    vc(bp(gd(glob_addr(G_REPORTER))), 28);
    mb(self, 0x1b0) = 0;
    m(self, 0x1ac) = kind;
    const DWORD rules = custom_rules();
    m(self, 0x164) = fn<DWORD(THISCALL *)(DWORD)>(0x5a2010)(rules);
    if (m(self, 0x1ac) == 2)
        m(self, 0x164) = fn<DWORD(THISCALL *)(DWORD)>(0x5a20d0)(rules);
    BYTE *opt = bp(m(self, 0x164));
    m(self, 0x168) = record_loaded(glob_addr(SEQUENCES), m(opt, 8));
    vc(bp(m(self, 0x168)), 36, self + 0x16c);
    opt = bp(m(self, 0x164));
    const int left = mi(self, 0x16c), top = mi(self, 0x170);
    mi(self, 0x180) = mi(opt, 0x34) - top;
    mi(self, 0x17c) = mi(opt, 0x30) - left;
    mi(self, 0x184) = mi(opt, 0x38) - left + mi(opt, 0x30);
    mi(self, 0x188) = mi(opt, 0x3c) - top + mi(opt, 0x34);
    BYTE *details = alloc(0x114);
    if (details) {
        opt = bp(m(self, 0x164));
        details = details_make(details, self, m(opt, 0x80), 1, m(opt, 0x70) - 2 * m(opt, 0x94), m(opt, 0x98), m(opt, 0x94));
    }
    m(self, 0x19c) = pv(details);
    add_child(self, details, 0, 0, NOWHERE, NOWHERE, 0);
    opt = bp(m(self, 0x164));
    vc(bp(m(self, 0x19c)), V_SET_SIZE, m(opt, 0x70), m(opt, 0x5c));
    opt = bp(m(self, 0x164));
    move_to(bp(m(self, 0x19c)), mi(opt, 0x58) - mi(opt, 0x70) + mi(opt, 0x50) - mi(self, 0x16c), mi(opt, 0x54) - mi(self, 0x170));
    BYTE *d = bp(m(self, 0x19c));
    const int w = mi(d, W_RECT + 8) - mi(d, W_RECT);
    const DWORD h = fn<DWORD(THISCALL *)(BYTE *)>(0x50ff80)(d);
    fn<void(THISCALL *)(DWORD, int, DWORD)>(0x40d550)(m(self, 0x19c), w, h);
    BYTE *bar = alloc(0x190);
    if (bar)
        bar = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4144b0)(bar, self, 1);
    m(self, 0x1a0) = pv(bar);
    add_child(self, bar, 0, 0, NOWHERE, NOWHERE, 0);
    opt = bp(m(self, 0x164));
    vc(bp(m(self, 0x1a0)), V_SET_SIZE, m(opt, 0x68), m(opt, 0x6c));
    opt = bp(m(self, 0x164));
    move_to(bp(m(self, 0x1a0)), mi(opt, 0x60) - mi(self, 0x16c), mi(opt, 0x64) - mi(self, 0x170));
    m(bp(m(self, 0x1a0)), 0x160) = m(self, 0x19c);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x413ce0)(m(self, 0x1a0), m(self, 0x19c));
    BYTE colour[0x10];
    const DWORD c = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, 2, 0x8f, 0x65, 0xff);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x414560)(m(self, 0x1a0), c);
    mb(bp(m(self, 0x1a0)), 0x164) = 1;
    using Images = void(THISCALL *)(DWORD, DWORD, DWORD);
    DWORD second = record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x24));
    fn<Images>(0x4145e0)(m(self, 0x1a0), record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x1c)), second);
    second = record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x28));
    fn<Images>(0x414600)(m(self, 0x1a0), record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x20)), second);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x414620)(m(self, 0x1a0), record_inline(glob_addr(0x6c22e0), m(bp(m(self, 0x164)), 0x2c)));
    zs_assign(self + 0x134, text);
    m(self, 0x138) = value;
    BYTE *frame = bp(m(self, 0x168));
    const DWORD fh = vc(frame, 39);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, vc(frame, 45), fh);
    BYTE *b = new_button_z(self, glob_addr(0x6cfd00), 0);
    m(self, 0x13c) = pv(b);
    add_child(self, b, 0, 0, NOWHERE, NOWHERE, 0);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(m(self, 0x13c), record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x14)), 1, 0);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x4100f0)(m(self, 0x13c), record_inline(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x18)));
    opt = bp(m(self, 0x164));
    move_to(bp(m(self, 0x13c)), mi(opt, 0x9c) - mi(self, 0x16c), mi(opt, 0xa0) - mi(self, 0x170));
    DWORD count = 0;
    m(self, 0x1a4) = record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0xc));
    m(self, 0x1a8) = record_loaded(glob_addr(SEQUENCES), m(bp(m(self, 0x164)), 0x10));
    const DWORD wide = vc(bp(m(self, 0x1a4)), 45);
    BYTE *image = wide > vc(bp(m(self, 0x1a8)), 45) ? bp(m(self, 0x1a4)) : bp(m(self, 0x1a8));
    DWORD title;
    switch (m(self, 0x1ac)) {
    case 2:
        m(self, 0x144) = 0, m(self, 0x140) = 0, m(self, 0x150) = 0;
        m(self, 0x14c) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xb8));
        add_child(self, bp(m(self, 0x14c)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x148) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xe4));
        add_child(self, bp(m(self, 0x148)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x154) = m(self, 0x14c), m(self, 0x158) = m(self, 0x148), m(self, 0x15c) = 0;
        mb(bp(m(self, 0x13c)), W_ENABLED) = 1;
        count = 2;
        m(self, 0x18c) = record_inline(glob_addr(FONTS), m(bp(m(self, 0x164)), 0xac));
        title = text;
        break;
    case 1:
        m(self, 0x150) = 0, m(self, 0x14c) = 0;
        m(self, 0x140) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xd4));
        add_child(self, bp(m(self, 0x140)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x144) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xdc));
        add_child(self, bp(m(self, 0x144)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x148) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xe4));
        add_child(self, bp(m(self, 0x148)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x154) = m(self, 0x140), m(self, 0x158) = m(self, 0x144), m(self, 0x15c) = m(self, 0x148), m(self, 0x160) = 0;
        mb(bp(m(self, 0x13c)), W_ENABLED) = 0;
        count = 3;
        mb(bp(m(self, 0x144)), W_ENABLED) = m(bp(m(bp(gd(glob_addr(G_APP))), 0x33c)), 0x1c) != 0;
        m(self, 0x18c) = record_inline(glob_addr(FONTS), m(bp(m(self, 0x164)), 0xc8));
        title = fn<DWORD(THISCALL *)(BYTE *)>(0x5007b0)(bp(m(self, 0x164)) + 0xcc);
        break;
    case 0:
        m(self, 0x148) = 0, m(self, 0x144) = 0, m(self, 0x140) = 0;
        m(self, 0x14c) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xb8));
        add_child(self, bp(m(self, 0x14c)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x150) = pv(new_named_button(self, bp(m(self, 0x164)) + 0xc0));
        add_child(self, bp(m(self, 0x150)), 0, 0, NOWHERE, NOWHERE, 0);
        m(self, 0x154) = m(self, 0x14c), m(self, 0x158) = m(self, 0x150), m(self, 0x15c) = 0;
        mb(bp(m(self, 0x13c)), W_ENABLED) = 1;
        count = 2;
        m(self, 0x18c) = record_inline(glob_addr(FONTS), m(bp(m(self, 0x164)), 0xac));
        title = fn<DWORD(THISCALL *)(BYTE *)>(0x5007b0)(bp(m(self, 0x164)) + 0xb0);
        break;
    default:
        title = 0;
        break;
    }
    if (m(self, 0x1ac) <= 2)
        zs_assign(self + 0x190, title);
    // the title centred on the options' point (+0x40, +0x44)
    const DWORD fh2 = m(bp(m(self, 0x18c)), 0x80);
    const char *t = c_str(self + 0x190);
    opt = bp(m(self, 0x164));
    const int ty = mi(opt, 0x44) - (int)(fh2 >> 1) - mi(self, 0x170);
    const DWORD tw = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x4a7310)(m(self, 0x18c), t, -1);
    m(self, 0x198) = (DWORD)ty;
    mi(self, 0x194) = mi(bp(m(self, 0x164)), 0x40) - (int)(tw >> 1) - mi(self, 0x16c);
    // the buttons in a row, centred on (+0x48, +0x4c)
    BYTE *pressed = bp(m(self, 0x1a8));
    const int border = (signed char)mb(self, W_BORDER);
    DWORD total = vc(image, 45) * count + (count - 1) * (DWORD)border;
    total -= vc(image, 45);
    total += vc(pressed, 45);
    int x = mi(bp(m(self, 0x164)), 0x48) - (int)(total >> 1) - mi(self, 0x16c);
    const int step = (int)vc(image, 45) + border;
    const int y = mi(bp(m(self, 0x164)), 0x4c) - (int)(vc(bp(m(self, 0x1a8)), 39) >> 1) - mi(self, 0x170);
    for (DWORD k = 0; k < count; k++) {
        const DWORD button = m(self, 0x154 + 4 * k);
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(button, m(self, 0x1a8), 1, 0);
        vc(bp(m(self, 0x154 + 4 * k)), 46, record_inline(glob_addr(FONTS), m(bp(m(self, 0x164)), 0x7c)));
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4101c0)(m(self, 0x154 + 4 * k), 1);
        move_to(bp(m(self, 0x154 + 4 * k)), x, y);
        x += step;
    }
    return self;
}
BYTE *THISCALL MissionOver_DeletingDtor(BYTE *self, DWORD flags)   // 0x50db00
{
    m(self, 0) = vt(0x5fa064);
    vc(bp(gd(glob_addr(G_REPORTER))), 29);
    gd(glob_addr(G_MISSION_OVER)) = 0;
    zs_dtor(self + 0x190);
    zs_dtor(self + 0x134);
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x1b4);
    return self;
}
DWORD __cdecl MissionOver_IsOpen() { return gd(glob_addr(G_MISSION_OVER)) != 0; }   // 0x50db60
// Closes the open mission over window (its slot 5)
DWORD __cdecl MissionOver_Close()   // 0x50db70
{
    if (BYTE *w = bp(gd(glob_addr(G_MISSION_OVER))))
        return vc(w, V_CLOSE);
    return 0;
}
// Slot 16 (the key): without the kind's first button +0x140 Enter, Escape, space and 250/251 close the window
// (multiplayer: send "continue" once and disable +0x14c); else the dialog's keys
DWORD THISCALL MissionOver_Key(BYTE *self, DWORD key)   // 0x50e860
{
    if (m(self, 0x140))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x403e40)(self, key);
    if (key != 0xd && key != 0x1b && key != 0x20 && key != 0xfa && key != 0xfb)
        return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x403e40)(self, key);
    if (m(self, 0x1ac) != 2) {
        vc(self, V_CLOSE);
        return 1;
    }
    if (!mb(self, 0x1b0)) {
        mb(self, 0x1b0) = 1;
        fn<void(THISCALL *)(DWORD)>(0x5ac500)(fn<DWORD(__cdecl *)()>(0x5ac560)());
        if (BYTE *b = bp(m(self, 0x14c)))
            mb(b, W_ENABLED) = 0;
    }
    return 1;
}

// ---- The mission details window and page -------------------------------------------------------------------

BYTE *THISCALL MissionDetails_Make(BYTE *self, BYTE *parent, DWORD a, BYTE b, DWORD c, DWORD d, DWORD e)   // 0x50fea0
{
    window_ctor(self, pv(parent), glob<const char *>(0x61b2bc));
    m(self, 0x100) = a;
    mb(self, 0x104) = b;
    m(self, 0x108) = c;
    m(self, 0) = vt(0x5fa198);
    m(self, 0x10c) = d;
    m(self, 0x110) = e;
    return self;
}
BYTE *THISCALL MissionDetails_DeletingDtor(BYTE *self, DWORD flags)   // 0x50fef0
{
    m(self, 0) = vt(0x5ea72c);
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x114);
    return self;
}
// The inventory screen's mission page (the parent): the details window (the inline Make with the display
// options +0xe4: +0x4c, the width +0x44 less 10, 2, 5) sized and placed, the image scroll bar
BYTE *THISCALL MissionPage_Ctor(BYTE *self, DWORD parent)   // 0x50ffe0
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x42d3d0)(self, parent, glob_addr(0x61b2d4));
    m(self, 0) = vt(0x5fa2c8);
    m(self, 0x100) = 0, m(self, 0x104) = 0, m(self, 0x108) = 0;
    mb(self, 0x10c) = 0;
    BYTE *opt = fn<BYTE *(THISCALL *)(DWORD)>(0x5a1d40)(custom_rules()) + 0xe4;
    BYTE *d = alloc(0x114);
    if (d) {
        const DWORD width = m(opt, 0x44) - 10;
        const DWORD a = m(opt, 0x4c);
        window_ctor(d, pv(self), glob<const char *>(0x61b2bc));
        m(d, 0x108) = width;
        m(d, 0) = vt(0x5fa198);
        m(d, 0x100) = a;
        mb(d, 0x104) = 0;
        m(d, 0x10c) = 2;
        m(d, 0x110) = 5;
    }
    m(self, 0x108) = pv(d);
    add_child(self, d, 0, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x108)), V_SET_SIZE, m(opt, 0x44), m(opt, 0x48));
    move_to(bp(m(self, 0x108)), mi(opt, 0x3c), mi(opt, 0x40));
    vc(bp(m(self, 0x108)), 42);
    BYTE *bar = alloc(0x190);
    if (bar)
        bar = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4144b0)(bar, self, 1);
    m(self, 0x104) = pv(bar);
    add_child(self, bar, 0, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x104)), V_SET_SIZE, m(opt, 0x58), m(opt, 0x5c));
    move_to(bp(m(self, 0x104)), mi(opt, 0x50), mi(opt, 0x54));
    m(bp(m(self, 0x104)), 0x160) = m(self, 0x108);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x413ce0)(m(self, 0x104), m(self, 0x108));
    vc(bp(m(self, 0x104)), 42);
    mb(self, 0x10c) = 1;
    return self;
}
BYTE *THISCALL MissionPage_DeletingDtor(BYTE *self, DWORD flags)   // 0x5101a0
{
    fn<void(THISCALL *)(BYTE *)>(0x42d3f0)(self);
    if (flags & 1)
        pool_delete(self, 0x110);
    return self;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_mission)
{
    const Hook list[] = {
#include "ui_mission_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
