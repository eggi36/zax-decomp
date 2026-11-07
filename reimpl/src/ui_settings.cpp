// The sound and video configuration dialogs (docs/DECOMPILATION.md 9, "The rest of the user interface"): the
// sound dialog's volume sliders, mute boxes and the voice-over, quality and channel choices from the INI file;
// the video dialog's two pages (brightness, colour depth, resolution; the 16-bit pixel format and the video card)
// and, when it closes, the new resolution set (0x567f20) and written to the INI file. Every call into the toolkit
// or the rest of the game goes through its exe address.
//
// Layouts [confirmed: the constructors, destructors and slots below]:
//   sound dialog (0x180, vtable 0x600cc4, a CDialog): +0x134/+0x140/+0x14c/+0x158/+0x164/+0x170 the labels,
//     +0x138/+0x144/+0x150 the mute boxes, +0x13c/+0x148/+0x154 the master, effects and music sliders (0 ..
//     0xd05), +0x15c/+0x160, +0x168/+0x16c, +0x174/+0x178 the radio pairs, +0x17c Ok
//   video dialog (0x180, vtable 0x6022a8): +0x134 the brightness label, +0x138 its slider (0 .. 100), +0x13c the
//     tabs, +0x140/+0x144 the pages, +0x148 the resolution label, +0x14c its slider, +0x150 its text, +0x154 …
//     +0x168 the second page's labels, +0x15c the depth label, +0x160 the depth combo box (names 0x713bf0),
//     +0x16c Ok, +0x170 the resolutions, +0x174 the current one, +0x178 the chosen one, +0x17c the depth at the
//     start (0x713bd0 the depth chosen)
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr DWORD INI = 0x6bd330, INI_FILE = 0x6bd430;
constexpr DWORD G_DEPTH = 0x713bd0, DEPTH_NAMES = 0x713bf0;
constexpr int SET_TITLE = 43, SET_VALUE = 76, VALUE = 77, CHECK = 78;

int ini_int(DWORD key, int def) { return fn<int(__fastcall *)(DWORD, DWORD, const char *, int)>(0x4a4670)(glob_addr(INI), glob_addr(INI_FILE), at<const char *>(key), def); }
void ini_write_int(DWORD key, int v) { fn<void(__fastcall *)(DWORD, DWORD, const char *, int)>(0x4a4690)(glob_addr(INI), glob_addr(INI_FILE), at<const char *>(key), v); }
BYTE *screen() { return bp(m(bp(gd(glob_addr(G_REPORTER))), 4)); }
BYTE *new_check(BYTE *self, DWORD text)   // CheckBox::CheckBox 0x412280 with the "Interface/Mute" image
{
    BYTE *w = alloc(0x144);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x412280)(w, self, c_str(text), 0);
    fn<void(THISCALL *)(BYTE *, const char *, const char *, DWORD)>(0x40ff50)(w, glob<const char *>(0x60a630), glob<const char *>(0x622258), 0);
    return w;
}
BYTE *new_slider(BYTE *self, DWORD most)   // CJSlider::CJSlider 0x4101f0 (0 .. most)
{
    BYTE *w = alloc(0x10c);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x4101f0)(w, self, most, 0) : nullptr;
}
BYTE *new_radio(BYTE *self, DWORD text, DWORD partner)   // RadioButton::RadioButton 0x4109a0
{
    BYTE *w = alloc(0x138);
    if (!w)
        return nullptr;
    const char *t = c_str(text);
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x4109a0)(w, self, t, partner);
}
BYTE *new_text_label(BYTE *self, DWORD text)   // TextLabel::TextLabel 0x4127d0 with a ZString's text
{
    BYTE *w = alloc(0x128);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(w, self, c_str(text)) : nullptr;
}
void set_value(DWORD w, DWORD v, DWORD a) { vc(bp(w), SET_VALUE, v, a); }
long double fat(DWORD address) { return (long double)*at<float *>(address); }
// A row of the sound dialog: the label (below the previous one), the mute box, the slider
void sound_row(BYTE *self, DWORD label, DWORD text, DWORD rel, DWORD box, DWORD slider)
{
    BYTE *w = new_text_label(self, text);
    m(self, label) = pv(w);
    add_child(self, w, rel ? 9 : 1, rel ? m(self, rel) : 0, NOWHERE, NOWHERE, 0);
    w = new_check(self, glob_addr(0x713764));
    m(self, box) = pv(w);
    add_child(self, bp(m(self, box)), 0xe, 0, NOWHERE, NOWHERE, 0);
    w = new_slider(self, 0xd05);
    m(self, slider) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
}
void check_if(BYTE *self, DWORD box, int v) { vc(bp(m(self, box)), CHECK, v ? 1u : 0u); }
// A radio pair: the label (below `rel`), two radio buttons; the first or second checked
void radio_pair(BYTE *self, DWORD label, DWORD text, DWORD rel, DWORD first, DWORD t1, DWORD second, DWORD t2)
{
    BYTE *w = new_text_label(self, text);
    m(self, label) = pv(w);
    add_child(self, w, 9, m(self, rel), NOWHERE, NOWHERE, 0);
    w = new_radio(self, t1, 0);
    m(self, first) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    w = new_radio(self, t2, m(self, first));
    m(self, second) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
}

}  // namespace

// ---- The sound configuration dialog ----------------------------------------------------------------------------

// The constructor (the parent): the master, effects and music rows from the INI file ("Master Voume", "Master
// Mute", "SFx Voume" as a float, "SFx Mute", "Music Volume" (-800), "Music Mute"), the voice-over choice ("Play
// Voiceovers"), the quality and channel choices (the resource cache's +0x30 and +8 == 5), the sliders 125 wide or
// fitted, the radio pairs spread right of the boxes, Ok
BYTE *THISCALL SoundDialog_Ctor(BYTE *self, DWORD parent)   // 0x57efd0
{
    dialog_ctor(self, parent, c_str(glob_addr(0x713754)));
    m(self, 0) = vt(0x600cc4);
    fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self);
    sound_row(self, 0x134, glob_addr(0x713768), 0, 0x138, 0x13c);
    set_value(m(self, 0x13c), (DWORD)ini_int(glob_addr(0x622410), 0) + 0xd05, 1);
    check_if(self, 0x138, ini_int(glob_addr(0x622420), 0));
    sound_row(self, 0x140, glob_addr(0x71377c), 0x134, 0x144, 0x148);
    const long double sfx = fn<long double(__fastcall *)(DWORD, DWORD, const char *, float)>(0x4a46e0)(glob_addr(INI), glob_addr(INI_FILE), glob<const char *>(0x6223f8), 1.0f);
    set_value(m(self, 0x148), (DWORD)x87_ftol((sfx - fat(glob_addr(0x5eee2c))) * fat(glob_addr(0x600cc0))), 1);
    check_if(self, 0x144, ini_int(glob_addr(0x622404), 0));
    sound_row(self, 0x14c, glob_addr(0x713778), 0x140, 0x150, 0x154);
    set_value(m(self, 0x154), (DWORD)ini_int(glob_addr(0x6223b4), -800) + 0xd05, 1);
    check_if(self, 0x150, ini_int(glob_addr(0x6223c4), 0));
    radio_pair(self, 0x158, glob_addr(0x713774), 0x14c, 0x15c, glob_addr(0x713760), 0x160, glob_addr(0x713780));
    vc(bp(m(self, ini_int(glob_addr(0x60b05c), 1) ? 0x15c : 0x160)), CHECK, 1);
    radio_pair(self, 0x164, glob_addr(0x713758), 0x158, 0x168, glob_addr(0x71376c), 0x16c, glob_addr(0x713750));
    vc(bp(m(self, mb(fn<BYTE *(__cdecl *)()>(0x57aef0)(), 0x30) ? 0x168 : 0x16c)), CHECK, 1);
    radio_pair(self, 0x170, glob_addr(0x713748), 0x164, 0x174, glob_addr(0x71374c), 0x178, glob_addr(0x71375c));
    vc(bp(m(self, m(fn<BYTE *(__cdecl *)()>(0x57aef0)(), 8) == 5 ? 0x178 : 0x174)), CHECK, 1);
    set_width(bp(m(self, 0x13c)), 0x7d);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40e590)(self, m(self, 0x13c), NOWHERE);
    BYTE *s = bp(m(self, 0x13c));
    set_width(s, mi(s, W_RECT + 8) - mi(s, W_RECT));
    s = bp(m(self, 0x13c));
    set_width(bp(m(self, 0x148)), mi(s, W_RECT + 8) - mi(s, W_RECT));
    s = bp(m(self, 0x13c));
    set_width(bp(m(self, 0x154)), mi(s, W_RECT + 8) - mi(s, W_RECT));
    const DWORD right = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x40d0f0)(m(self, 0x138), m(self, 0x13c));
    using Spread = void(THISCALL *)(DWORD, DWORD, DWORD);
    fn<Spread>(0x40d100)(m(self, 0x174), m(self, 0x178), right);
    fn<Spread>(0x40d100)(m(self, 0x15c), m(self, 0x160), right);
    fn<Spread>(0x40d100)(m(self, 0x168), m(self, 0x16c), right);
    BYTE *ok = new_button(self, c_str(glob_addr(0x713770)), 0);
    m(self, 0x17c) = pv(ok);
    add_child(self, ok, 0xc, 0, 0, 8, 0);
    m(self, 0x11c) = m(self, 0x17c);
    m(self, 0x120) = m(self, 0x17c);
    return self;
}
BYTE *THISCALL SoundDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x57f840
{
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x180);
    return self;
}

// ---- The video configuration dialog ----------------------------------------------------------------------------

namespace {

BYTE *new_page(BYTE *self, DWORD name)   // CJWindow(ZString) 0x40a940 with the frame window base's vtable
{
    BYTE *w = alloc(0x100);
    if (!w)
        return nullptr;
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40a940)(w, self, name);
    m(w, 0) = vt(0x5ea72c);
    return w;
}
void page_add(BYTE *self, DWORD page, DWORD child, DWORD how, DWORD rel, DWORD x = NOWHERE, DWORD y = NOWHERE)
{
    vc(bp(m(self, page)), V_ADD_CHILD, child, how, rel, x, y, 0);
}

}  // namespace

// The constructor (the parent): the first page with the brightness (the screen's +0xd0), the colour depth names
// (added to 0x713bf0 the first time) and the resolution of the screen's list (+0x4c entries of 0xc bytes from
// +0x54) with its slider (off when the world's object says so) and text; on 16 bits the second page shows the
// pixel format ("555"/"565"), then the video card ("%d %s"); the tabs, Ok
BYTE *THISCALL VideoDialog_Ctor(BYTE *self, DWORD parent)   // 0x591020
{
    dialog_ctor(self, parent, c_str(glob_addr(0x713bb8)));
    m(self, 0) = vt(0x6022a8);
    fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self);
    m(self, 0x140) = pv(new_page(self, glob_addr(0x713bcc)));
    m(self, 0x144) = pv(new_page(self, glob_addr(0x713c08)));
    BYTE *w = new_label_z(self, glob_addr(0x713be0));
    m(self, 0x134) = pv(w);
    page_add(self, 0x140, pv(w), 1, 0);
    vc(bp(m(self, 0x134)), SET_VALUE, 2);
    w = new_slider(self, 0x64);
    m(self, 0x138) = pv(w);
    page_add(self, 0x140, pv(w), 0xe, 0);
    BYTE *slider = bp(m(self, 0x138));
    const long double b = (fat(pv(screen()) + 0xd0) - fat(glob_addr(0x5ec3f4))) * fat(glob_addr(0x5effa0));
    vc(slider, SET_VALUE, x87_ftol(b), 0);
    DWORD format_label = 0;
    w = new_label_z(self, glob_addr(0x713be4));
    m(self, 0x15c) = pv(w);
    page_add(self, 0x140, pv(w), 9, m(self, 0x134));
    vc(bp(m(self, 0x15c)), SET_VALUE, 2);
    if (!gd(glob_addr(DEPTH_NAMES) + 0xc)) {
        fn<void(THISCALL *)(DWORD, const char *)>(0x54f470)(glob_addr(DEPTH_NAMES), c_str(glob_addr(0x713bc0)));
        fn<void(THISCALL *)(DWORD, const char *)>(0x54f470)(glob_addr(DEPTH_NAMES), c_str(glob_addr(0x713bc8)));
    }
    w = alloc(0x118);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x408cd0)(w, self, glob_addr(DEPTH_NAMES), 999999);
    m(self, 0x160) = pv(w);
    page_add(self, 0x140, pv(w), 0xe, 0);
    const DWORD depth = gd(glob_addr(G_DEPTH));
    if (depth == 8 || depth == 0x10 || depth == 0x18)
        fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x408bc0)(m(self, 0x160), depth == 8 ? 0 : depth == 0x10 ? 1 : 2, 0);
    m(self, 0x17c) = gd(glob_addr(G_DEPTH));
    const DWORD rel = m(self, 0x15c);
    BYTE *combo = bp(m(self, 0x160));
    const int width = mi(combo, W_RECT + 8) - mi(combo, W_RECT);
    if (vc(screen(), 29) == 0x10) {
        const bool five = !mb(screen(), 0xcc);
        w = new_label_z(self, glob_addr(0x713c04));
        m(self, 0x164) = pv(w);
        page_add(self, 0x144, pv(w), 1, 0);
        vc(bp(m(self, 0x164)), SET_VALUE, 2);
        w = alloc(0x128);
        if (w)
            w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4127d0)(w, self, five ? glob_addr(0x623898) : glob_addr(0x623894));
        m(self, 0x168) = pv(w);
        page_add(self, 0x144, pv(w), 0xe, 0);
        format_label = m(self, 0x164);
    }
    set_width(bp(m(self, 0x138)), width);
    w = new_label_z(self, glob_addr(0x713bd4));
    m(self, 0x148) = pv(w);
    page_add(self, 0x140, pv(w), 9, rel);
    vc(bp(m(self, 0x148)), SET_VALUE, 2);
    m(self, 0x174) = 0;
    m(self, 0x170) = m(screen(), 0x4c);
    for (DWORD i = 0; i < m(self, 0x170); i++) {
        BYTE *s = screen();
        if (m(s, 0x54 + i * 0xc) != vc(s, 45))
            continue;
        const DWORD h = vc(s, 39);
        if (m(screen(), 0x58 + i * 0xc) == h)
            m(self, 0x174) = i;
    }
    m(self, 0x178) = m(self, 0x174);
    w = new_slider(self, 0x64);
    m(self, 0x14c) = pv(w);
    page_add(self, 0x140, pv(w), 0xe, 0);
    const BYTE locked = (BYTE)vc(vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 1), 13);
    mb(bp(m(self, 0x14c)), W_ENABLED) = locked == 0;
    set_width(bp(m(self, 0x14c)), width);
    w = alloc(0x128);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4127d0)(w, self, glob_addr(0x623890));
    m(self, 0x150) = pv(w);
    page_add(self, 0x140, pv(w), 8, 0, (DWORD)-2, NOWHERE);
    vc(bp(m(self, 0x150)), SET_VALUE, 1);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x410880)(m(self, 0x14c), m(self, 0x170), 1);
    vc(bp(m(self, 0x14c)), SET_VALUE, m(self, 0x178), 1);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x5918c0)(self, m(self, 0x14c), 1);
    w = new_label_z(self, glob_addr(0x713bbc));
    m(self, 0x154) = pv(w);
    if (format_label)
        page_add(self, 0x144, pv(w), 9, format_label);
    else
        page_add(self, 0x144, pv(w), 1, 0);
    vc(bp(m(self, 0x154)), SET_VALUE, 2);
    DWORD card;
    zs_ctor(&card);
    const char *kind = c_str(mb(screen(), 0x50) ? glob_addr(0x713bc4) : glob_addr(0x713c0c));
    zs_format(&card, glob<const char *>(0x623888), m(screen(), 0x44), kind);
    w = alloc(0x128);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(w, self, c_str(&card));
    m(self, 0x158) = pv(w);
    page_add(self, 0x144, pv(w), 0xe, 0);
    w = alloc(0x11c);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x409be0)(w, self);
    m(self, 0x13c) = pv(w);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40a2f0)(w, m(self, 0x140));
    fn<void(THISCALL *)(DWORD, DWORD)>(0x40a2f0)(m(self, 0x13c), m(self, 0x144));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40a4b0)(m(self, 0x13c), 0, 1);
    add_child(self, bp(m(self, 0x13c)), 1, 0, NOWHERE, NOWHERE, 0);
    BYTE *ok = new_button_z(self, glob_addr(0x713c10), 0);
    m(self, 0x16c) = pv(ok);
    add_child(self, ok, 0xc, 0, 0, 6, 0);
    m(self, 0x11c) = m(self, 0x16c);
    m(self, 0x120) = m(self, 0x16c);
    zs_dtor(&card);
    return self;
}
// The destructor: a depth other than at the start and the screen's asks for a restart (a message box); a new
// resolution is set (0x567f20, 24 bits falling back to 32; failing: "Couldn't set resolution" and the game ends),
// the reporter's slot 8, the new size written to the INI file
void THISCALL VideoDialog_Dtor(BYTE *self)   // 0x591750
{
    m(self, 0) = vt(0x6022a8);
    DWORD depth = gd(glob_addr(G_DEPTH));
    if (depth != m(self, 0x17c) && depth != m(bp(gd(glob_addr(G_REPORTER))), 0x24)) {
        BYTE *box = alloc(0x11c);
        if (box) {
            const char *title = c_str(glob_addr(0x713bd8));
            box = fn<BYTE *(THISCALL *)(BYTE *, const char *, const char *, DWORD)>(0x403b30)(box, title, c_str(glob_addr(0x713be8)), 0);
        }
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40c6e0)(self, box, 6);
        depth = gd(glob_addr(G_DEPTH));
    }
    const DWORD chosen = m(self, 0x178);
    if (chosen != m(self, 0x174)) {
        BYTE *s = screen();
        DWORD w = m(s, 0x54 + chosen * 0xc), h = m(s, 0x58 + chosen * 0xc);
        const DWORD w0 = w, h0 = h;
        using SetMode = void(THISCALL *)(BYTE *, DWORD *, DWORD *, DWORD, DWORD);
        fn<SetMode>(0x567f20)(screen(), &w, &h, depth, depth);
        if (!w) {
            if (depth == 0x18) {
                w = w0, h = h0;
                fn<SetMode>(0x567f20)(screen(), &w, &h, 0x20, depth);
            }
            if (!w) {
                fn<void(__fastcall *)(DWORD, DWORD)>(0x4d24c0)(glob_addr(0x610490), glob_addr(0x623830));
                fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
            }
        }
        vc(bp(gd(glob_addr(G_REPORTER))), 8);
        ini_write_int(glob_addr(0x623878), (int)vc(screen(), 45));
        ini_write_int(glob_addr(0x623868), (int)vc(screen(), 39));
    }
    dialog_dtor(self);
}
BYTE *THISCALL VideoDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x591720
{
    fn<void(THISCALL *)(BYTE *)>(0x591750)(self);
    if (flags & 1)
        pool_delete(self, 0x180);
    return self;
}
// Slot 21 (the child, what): Ok closes; a change of the brightness slider writes "Screen Brightness" (the value
// × 0x5eee2c × 0x5f1e38 + 1) and sets it (0x569520); of the resolution slider shows "%d X %d"; of the depth combo
// box sets 0x713bd0 (8, 16, 24) and writes "Screen Bit Depth"; the frame window's notify after each
void THISCALL VideoDialog_Notify(BYTE *self, DWORD from, DWORD what)   // 0x5918c0
{
    if (what == 0) {
        if (from == m(self, 0x16c)) {
            vc(self, V_CLOSE);
            return;
        }
    } else if (what == 1) {
        if (from == m(self, 0x138)) {
            const DWORD v = vc(bp(from), VALUE);
            volatile float f = (float)((long double)(long long)v * fat(glob_addr(0x5eee2c)) * fat(glob_addr(0x5f1e38)) + fat(glob_addr(0x5ec3f4)));
            fn<void(__fastcall *)(DWORD, DWORD, const char *, float)>(0x4a4740)(glob_addr(INI), glob_addr(INI_FILE), glob<const char *>(0x62381c), f);
            fn<void(THISCALL *)(BYTE *, float)>(0x569520)(screen(), f);
        } else if (from == m(self, 0x14c)) {
            const DWORD i = vc(bp(from), VALUE);
            m(self, 0x178) = i;
            BYTE *s = screen();
            char text[0x6c];
            fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x62389c), m(s, 0x54 + i * 0xc), m(s, 0x58 + i * 0xc));
            vc(bp(m(self, 0x150)), SET_TITLE, text);
            BYTE *sl = bp(m(self, 0x14c));
            set_width(bp(m(self, 0x150)), mi(sl, W_RECT + 8) - mi(sl, W_RECT));
        } else if (from == m(self, 0x160)) {
            switch (vc(bp(from), SET_VALUE)) {
            case 0: gd(G_DEPTH) = 8; break;
            case 1: gd(G_DEPTH) = 0x10; break;
            case 2: gd(G_DEPTH) = 0x18; break;
            default: break;
            }
            ini_write_int(glob_addr(0x615600), (int)gd(glob_addr(G_DEPTH)));
        }
    }
    frame_notify(self, from, what);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_settings)
{
    const Hook list[] = {
#include "ui_settings_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
