// The save/load screen's constructor 0x562970 (docs/DECOMPILATION.md 9, "Phase 4 leftovers"; the screen's other
// functions are in ui_save_load.cpp and ui_save_load_rest4c.cpp, its layout there). Every call goes through its exe
// address.
//
// The constructor [confirmed: the code below] (parent, load): a CDialog titled by the localised ZString 0x7131a0
// (load) or 0x7131d4 (save); +0x188 the statistics (a heap pointer array, vtable 0x5ff07c), +0x1a0 built, +0x1a1
// filling the current statistics, +0x180 0. The reporter's slot 28 is told, an open screen (0x71320c) is closed
// (slot 5) and this one is it. The saves are the files "SaveGames/*.sav" (a FileSpec and a DirList on the stack,
// flags 0x20): each read into a new CZaxSaveGameStats (0x80; its +0x68 the path) through an ObjectStream (flags
// 0x24); one that does not open or read is deleted again (the last element of the array), "QuickSave.sav" gets the
// name pattern "*** <0x7131f4> ***"; every description goes into the names (+0x184, a FieldNameList, vtable
// 0x5feed8, +0x14 the screen); without saves the names get "<0x7131a4>" formatted with 0x620ee0. Then the header
// (+0x134, 380 wide, three columns 0x713184/0x7131dc/0x7131ec of 30, 190, 160), for saving the current game's
// statistics (the application's +0x350, filled) in a list of their own (+0x198 the names, +0x138 a list box with
// the description without its first field, +0x13c the name field inside it, +0x140 "Save" 0x7131d8), the saves' list
// box (+0x144, 300 high, sorted by 0x564ef0), the preview button (+0x148 "Small Gold" font, 200 x 150, disabled,
// the current game's image), the tab control (+0x14c) with an empty page per member of the null-terminated list at
// +0x160 (+0x150-+0x15c, the names from 0x620d24 on, the label 0x7131ac), the buttons +0x174 (0x7131d0, deleted
// again when saving), +0x178 (0x7131e0) and +0x17c (0x7131fc), the size limits +0x110/+0x114, the default and
// cancel buttons +0x11c/+0x120, the list sorted down by the header's column 2, the focus, the details page, the size
// and the first update.
#include "ui_screens.h"

namespace zax::uisl {

using namespace ui;

constexpr DWORD G_SCREEN = 0x71320c;

inline DWORD list_box(BYTE *self, DWORD source)
{
    BYTE *b = alloc(0x258);
    return b ? pv(fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x4041f0)(b, self, source, 999999, 1)) : 0;
}
inline BYTE *button_text(BYTE *self, DWORD label)
{
    BYTE *b = alloc(0x138);
    if (b) {
        const char *t = c_str(label);
        b = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(b, self, t, 0);
    }
    return b;
}
inline int width(DWORD w) { return mi(bp(w), 0xc) - mi(bp(w), 4); }
inline int height(DWORD w) { return mi(bp(w), 0x10) - mi(bp(w), 8); }
inline DWORD stats_description(DWORD stats, void *out) { return fn<DWORD(THISCALL *)(DWORD, void *)>(0x5bcb60)(stats, out); }
inline DWORD column_width(DWORD header, int c) { return fn<DWORD(THISCALL *)(DWORD, int)>(0x4028f0)(header, c); }
inline void add_column(DWORD header, DWORD name, DWORD w) { fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x402770)(header, name, w, 0, 0, 0); }
inline DWORD dir_name(void *dl, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a3310)(dl, i); }
inline DWORD dir_path(void *dl, DWORD name) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a33b0)(dl, name); }

// The last element of the statistics deleted and taken out (the template's RemoveAt inlined)
void drop_last(BYTE *self)
{
    BYTE *arr = self + 0x188;
    const DWORD i = m(arr, 8) - 1;
    if (BYTE *e = bp(m(bp(m(arr, 4)), i * 4)))
        vc(e, 1, 1);
    const DWORD count = m(arr, 8);
    if (i >= count)
        return;
    if (i + 1 < count)
        memmove_(bp(m(arr, 4)) + i * 4, bp(m(arr, 4)) + (i + 1) * 4, (count - (i + 1)) * 4);
    m(arr, 8) = m(arr, 8) - 1;
}

// The edit field placed over the list box's first row: the first two columns, the box's height
void place_field(BYTE *self)
{
    int r[4];
    r[1] = 0;
    r[3] = height(m(self, 0x138));
    r[0] = 0;
    r[0] += (int)column_width(m(self, 0x134), 0);
    r[2] = (int)column_width(m(self, 0x134), 1) + r[0];
    vc(bp(m(self, 0x13c)), 57, r);
}

BYTE *THISCALL SaveLoadScreen_Ctor(BYTE *self, DWORD parent, DWORD load)   // 0x562970
{
    const char *title = c_str(load ? glob_addr(0x7131a0) : glob_addr(0x7131d4));
    dialog_ctor(self, parent, title);
    BYTE *arr = self + 0x188;
    m(arr, 0) = vt(0x5ff07c);
    m(arr, 4) = 0;
    m(arr, 8) = 0;
    m(arr, 0xc) = 0;
    m(self, 0) = vt(0x5fef48);
    mb(self, 0x1a1) = 0;
    vc(bp(gd(glob_addr(G_REPORTER))), 28);
    if (BYTE *old = bp(gd(glob_addr(G_SCREEN))))
        vc(old, 5);
    gd(glob_addr(G_SCREEN)) = pv(self);
    mb(self, 0x1a0) = 0;
    m(self, 0x180) = 0;
    BYTE *names = alloc(0x18);
    if (names) {
        fn<void(THISCALL *)(BYTE *)>(0x592200)(names);
        m(names, 0) = vt(0x5feed8);
    }
    m(self, 0x184) = pv(names);
    m(names, 0x14) = pv(self);
    fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40c600)(self, 0);
    fn<void(THISCALL *)(BYTE *)>(0x403930)(self);
    // the saves
    alignas(4) BYTE spec[0x2c];
    alignas(4) BYTE dl[0x12c];
    fn<void(THISCALL *)(void *)>(0x4a09f0)(spec);
    fn<void(THISCALL *)(void *, const char *, const char *)>(0x4a0a60)(spec, glob<const char *>(0x620ea8), glob<const char *>(0x620f1c));
    fn<void(THISCALL *)(void *)>(0x4a2c50)(dl);
    const DWORD count = fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4a2ea0)(dl, spec, 0, 0, 0x20);
    if (count) {
        fn<void(__fastcall *)(DWORD)>(0x4a2650)(glob_addr(0x620f04));
        for (DWORD i = 0; i < count; i++) {
            DWORD stats = 0;
            BYTE *b = alloc(0x80);
            stats = b ? pv(fn<BYTE *(THISCALL *)(BYTE *)>(0x5bc230)(b)) : 0;
            vc(arr, 2, &stats);
            alignas(4) BYTE os[0x24];
            fn<void(THISCALL *)(void *)>(0x5240b0)(os);
            const DWORD path = dir_path(dl, dir_name(dl, i));
            if (!(BYTE)fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x5242b0)(os, path, 0x24, 0)) {
                drop_last(self);
            } else {
                const BYTE ok = (BYTE)fn<DWORD(THISCALL *)(DWORD, void *)>(0x416af0)(stats, os);
                fn<void(THISCALL *)(void *)>(0x524340)(os);
                if (!ok) {
                    drop_last(self);
                } else {
                    fn<void(THISCALL *)(DWORD, DWORD)>(0x4df010)(stats + 0x68, dir_path(dl, dir_name(dl, i)));
                    if (!fn<int(__cdecl *)(DWORD, const char *)>(0x5d3640)(dir_name(dl, i), glob<const char *>(0x620ef4))) {
                        DWORD z;
                        zs_ctor(&z);
                        zs_format(&z, glob<const char *>(0x620ee8), c_str(glob_addr(0x7131f4)));
                        fn<void(THISCALL *)(DWORD, void *)>(0x5bc910)(stats, &z);
                        zs_dtor(&z);
                    }
                    DWORD desc;
                    const char *t = c_str(bp(stats_description(stats, &desc)));
                    vc(bp(m(self, 0x184)), 18, t);
                    zs_dtor(&desc);
                }
            }
            fn<void(THISCALL *)(void *)>(0x5241e0)(os);
        }
        fn<void(__cdecl *)()>(0x4a2670)();
    } else {
        DWORD z;
        zs_ctor(&z);
        zs_format(&z, glob<const char *>(0x620ee0), c_str(glob_addr(0x7131a4)));
        vc(bp(m(self, 0x184)), 18, c_str(&z));
        zs_dtor(&z);
    }
    // the header
    BYTE *h = alloc(0x140);
    h = h ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x4020d0)(h, self) : nullptr;
    m(self, 0x134) = pv(h);
    set_width(h, 0x17c);
    add_column(m(self, 0x134), glob_addr(0x713184), 0x1e);
    add_column(m(self, 0x134), glob_addr(0x7131dc), 0xbe);
    add_column(m(self, 0x134), glob_addr(0x7131ec), 0xa0);
    h = bp(m(self, 0x134));
    m(h, 0x124) = m(h, 0xc) - m(h, 4);
    fn<void(THISCALL *)(BYTE *)>(0x402bb0)(h);
    add_child(self, bp(m(self, 0x134)), 1, 0, NOWHERE, NOWHERE, 0);
    // saving: the current game's line with the name field and "Save"
    if (!load) {
        const DWORD current = m(bp(gd(glob_addr(G_APP))), 0x350);
        mb(self, 0x1a1) = 1;
        m(self, 0x19c) = current;
        fn<void(THISCALL *)(DWORD)>(0x5bc2a0)(current);
        mb(self, 0x1a1) = 0;
        BYTE *own = alloc(0x14);
        if (own) {
            fn<void(THISCALL *)(BYTE *)>(0x592200)(own);
            m(own, 0) = vt(0x5f0240);
        }
        m(self, 0x198) = pv(own);
        DWORD desc;
        stats_description(m(self, 0x19c), &desc);
        char *buf = fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(&desc, 0);
        using Strchr = char *(__cdecl *)(const char *, int);
        char *p = fn<Strchr>(0x5d08c0)(buf, 9) + 1;
        char *next = fn<Strchr>(0x5d08c0)(p, 9);
        if (p[-1]) {
            const int skip = (int)(next - p);
            char c;
            do {
                *p = p[skip];
                c = *p++;
            } while (c);
        }
        fn<void(THISCALL *)(void *, int)>(0x4e16d0)(&desc, -1);
        vc(bp(m(self, 0x198)), 18, c_str(&desc));
        m(self, 0x138) = list_box(self, m(self, 0x198));
        set_width(bp(m(self, 0x138)), (DWORD)width(m(self, 0x134)));
        fn<void(THISCALL *)(DWORD, DWORD)>(0x407870)(m(self, 0x138), 1);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x406aa0)(m(self, 0x138), m(self, 0x134));
        add_child(self, bp(m(self, 0x138)), 8, 0, NOWHERE, NOWHERE, 0);
        vc(bp(m(self, 0x138)), 80, 0, 1);
        BYTE *field = alloc(0x120);
        if (field) {
            DWORD name;
            const char *t = c_str(bp(fn<DWORD(THISCALL *)(DWORD, void *)>(0x5bc810)(m(self, 0x19c), &name)));
            field = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x410b50)(field, self, t);
            m(self, 0x13c) = pv(field);
            zs_dtor(&name);
        } else {
            m(self, 0x13c) = 0;
        }
        place_field(self);
        vc(bp(m(self, 0x138)), 31, m(self, 0x13c), 1, 0, NOWHERE, NOWHERE, 0);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x407870)(m(self, 0x138), 1);
        place_field(self);
        m(self, 0x140) = pv(button_text(self, glob_addr(0x7131d8)));
        set_width(bp(m(self, 0x140)), 200);
        add_child(self, bp(m(self, 0x140)), 0xe, 0, NOWHERE, NOWHERE, 0);
        zs_dtor(&desc);
    } else {
        m(self, 0x19c) = 0;
        m(self, 0x198) = 0;
        m(self, 0x138) = 0;
        m(self, 0x13c) = 0;
        m(self, 0x140) = 0;
    }
    // the saves' list box
    m(self, 0x144) = list_box(self, m(self, 0x184));
    vc(bp(m(self, 0x144)), 59, width(m(self, 0x134)), 300);
    m(bp(m(self, 0x144)), 0x240) = fn_addr(0x564ef0);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x406aa0)(m(self, 0x144), m(self, 0x134));
    add_child(self, bp(m(self, 0x144)), 7, m(self, 0x138) ? m(self, 0x138) : m(self, 0x134), NOWHERE, NOWHERE, 0);
    // the preview button
    m(self, 0x148) = pv(button_text(self, glob_addr(0x7131cc)));
    const DWORD font = record_loaded(glob_addr(FONTS), find_index(glob_addr(FONTS), glob_addr(0x612264)));
    vc(bp(m(self, 0x148)), 46, font);
    vc(bp(m(self, 0x148)), 59, 200, 0x96);
    mb(bp(m(self, 0x148)), 0x61) = 0;
    if (const DWORD cur = m(self, 0x19c)) {
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(m(self, 0x148), cur + 0x14, 1, 0);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4100f0)(m(self, 0x148), m(self, 0x19c) + 0x14);
    }
    add_child(self, bp(m(self, 0x148)), 0xe, 0, NOWHERE, NOWHERE, 0);
    // the tab control with an empty page per member
    BYTE *tabs = alloc(0x11c);
    tabs = tabs ? fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x409be0)(tabs, m(self, 0x148)) : nullptr;
    m(self, 0x14c) = pv(tabs);
    vc(tabs, 59, 200, height(m(self, 0x144)) - (int)(signed char)mb(self, 0x79) - 0x96);
    add_child(self, bp(m(self, 0x14c)), 8, 0, NOWHERE, NOWHERE, 0);
    m(self, 0x160) = pv(self + 0x150);
    m(self, 0x164) = pv(self + 0x154);
    m(self, 0x168) = pv(self + 0x158);
    m(self, 0x16c) = pv(self + 0x15c);
    m(self, 0x170) = 0;
    DWORD name = glob_addr(0x620d24);
    for (DWORD *member = &m(self, 0x160); *member; member++, name += 4) {
        BYTE *page = alloc(0x100);
        if (page) {
            window_ctor(page, pv(self), c_str(gd(name)));
            m(page, 0) = vt(0x5ea72c);
        }
        *at<DWORD *>(*member) = pv(page);
        BYTE *label = new_label_z(self, glob_addr(0x7131ac));
        vc(bp(*at<DWORD *>(*member)), 31, pv(label), 1, 0, NOWHERE, NOWHERE, 0);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x40a2f0)(m(self, 0x14c), *at<DWORD *>(*member));
    }
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40a4b0)(m(self, 0x14c), 0, 1);
    mb(bp(m(self, 0x14c)), 0x61) = 0;
    // the three buttons below
    BYTE *b = new_button_z(self, glob_addr(0x7131d0));
    m(self, 0x174) = pv(b);
    set_width(b, (DWORD)((mi(self, 0xc) - 2 * ((signed char)mb(self, 0x78) + 2 * (signed char)mb(self, 0x79)) - mi(self, 4)) / 4));
    add_child(self, bp(m(self, 0x174)), 7, m(self, 0x144), NOWHERE, NOWHERE, 0);
    b = bp(m(self, 0x174));
    set_left(b, width(pv(b)) / 2 + (signed char)mb(self, 0x78) + (signed char)mb(self, 0x79));
    b = new_button_z(self, glob_addr(0x7131e0));
    m(self, 0x178) = pv(b);
    set_width(b, (DWORD)width(m(self, 0x174)));
    add_child(self, bp(m(self, 0x178)), 0xe, 0, NOWHERE, NOWHERE, 0);
    b = new_button_z(self, glob_addr(0x7131fc));
    m(self, 0x17c) = pv(b);
    set_width(b, (DWORD)width(m(self, 0x174)));
    add_child(self, bp(m(self, 0x17c)), 0xe, 0, NOWHERE, NOWHERE, 0);
    if (m(self, 0x140)) {
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x40c9b0)(self, m(self, 0x174));
        if (BYTE *o = bp(m(self, 0x174))) {
            vc(o, 0, 1);
            m(self, 0x174) = 0;
        }
    }
    // the limits, the default buttons, the order, the focus, the details, the size
    BYTE *l2 = bp(m(self, 0x144));
    m(self, 0x110) = 300;
    m(self, 0x114) = m(l2, 8) - m(l2, 0x10) - m(self, 8) + m(self, 0x10) + m(bp(m(self, 0x14c)), 0x110) + 0xc3;
    m(self, 0x11c) = m(self, 0x140) ? m(self, 0x140) : m(self, 0x174);
    m(self, 0x120) = m(self, 0x17c);
    m(bp(m(self, 0x134)), 0x12c) = 2;
    fn<void(THISCALL *)(DWORD, DWORD)>(0x406b60)(m(self, 0x144), 1);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x406b70)(m(self, 0x144), 0, m(bp(m(self, 0x144)), 0x108));
    vc(bp(m(self, 0x144)), 80, -1, 0);
    mb(bp(m(self, 0x144)), 0x61) = count != 0;
    DWORD f = m(self, 0x138);
    if (!f)
        f = count ? m(self, 0x144) : m(self, 0x17c);
    focus(self, bp(f));
    fn<void(THISCALL *)(BYTE *)>(0x564090)(self);
    mb(self, 0x1a0) = 1;
    fn<void(THISCALL *)(BYTE *, int, int)>(0x40d510)(self, mi(self, 0xc) - mi(self, 4), mi(self, 0x10) - mi(self, 8));
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x5637d0)(self, 0);
    fn<void(THISCALL *)(void *)>(0x4a2db0)(dl);
    for (DWORD o = 0x28; o >= 0x18; o -= 4)
        zs_dtor(spec + o);
    return self;
}

}  // namespace zax::uisl

namespace zax {

using namespace uisl;

ZAX_HOOKS(ui_save_load_ctor)
{
    const Hook list[] = {
        {0x562970, ptr(&SaveLoadScreen_Ctor), "SaveLoadScreen::SaveLoadScreen", 6, {0x81, 0xec, 0xa4, 0x01, 0x00, 0x00, 0x8b, 0x84}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
