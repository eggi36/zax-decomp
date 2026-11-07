// The save/load screen's smaller functions (docs/DECOMPILATION.md 9, "The rest of the user interface"): its
// destructors, its update (buttons enabled by the lists' choices), the row of two labels of a saved game's details
// and the list's sort compare. Of its two large builders the details page 0x564090 is in ui_save_load_rest4c.cpp, the
// constructor reading the saved games 0x562970 in ui_save_load_ctor.cpp (section 9, "Phase 4 leftovers"). Every call
// goes through its exe address.
//
// Layout [confirmed: the destructor and the slots below] (0x1a4 bytes, vtable 0x5fef48, a CDialog; 0x71320c while
// it is open): +0x138 the list of saves, +0x13c the name field, +0x140 Load/Save, +0x144 the second list, +0x174
// and +0x178 its buttons, +0x184 the names (a FieldNameList, vtable 0x5feed8, 0x18 bytes: +0x14 the screen),
// +0x188 the saved games' statistics (a heap pointer array, vtable 0x5ff07c), +0x198 an object deleted with it
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr int VALUE = 77, SHOW_ROW = 80;

}  // namespace

// The destructor: the statistics deleted (slot 1 with 1), the names and +0x198 deleted, no screen open any more,
// the reporter's slot 29, the array's data freed (operator delete)
void THISCALL SaveLoadScreen_Dtor(BYTE *self)   // 0x5636e0
{
    m(self, 0) = vt(0x5fef48);
    array_delete_all(self + 0x188, [](DWORD e) { vc(bp(e), 1, 1); });
    const DWORD owned[2] = {0x184, 0x198};
    for (DWORD o : owned)
        if (BYTE *p = bp(m(self, o))) {
            vc(p, 0, 1);
            m(self, o) = 0;
        }
    gd(glob_addr(0x71320c)) = 0;
    vc(bp(gd(glob_addr(G_REPORTER))), 29);
    m(self, 0x188) = vt(0x5ff07c);
    if (DWORD d = m(self, 0x18c)) {
        op_delete(bp(d));
        m(self, 0x18c) = 0;
        m(self, 0x194) = 0;
    }
    m(self, 0x190) = 0;
    dialog_dtor(self);
}
BYTE *THISCALL SaveLoadScreen_DeletingDtor(BYTE *self, DWORD flags)   // 0x563680
{
    fn<void(THISCALL *)(BYTE *)>(0x5636e0)(self);
    if (flags & 1)
        pool_delete(self, 0x1a4);
    return self;
}
// The names' deleting destructor (a FieldNameList of 0x18 bytes)
BYTE *THISCALL SaveLoadNames_DeletingDtor(BYTE *self, DWORD flags)   // 0x5636b0
{
    fn<void(THISCALL *)(BYTE *)>(0x592220)(self);
    if (flags & 1)
        pool_delete(self, 0x18);
    return self;
}
// Slot 2 (the seconds): while the name field is active both lists show no choice (slot 80); Load/Save and the
// second list's buttons are enabled by their list's choice
void THISCALL SaveLoadScreen_Update(BYTE *self, DWORD dt)   // 0x5637d0
{
    BYTE *field = bp(m(self, 0x13c));
    if (field && mb(field, W_ACTIVE)) {
        vc(bp(m(self, 0x138)), SHOW_ROW, 0, 0);
        vc(bp(m(self, 0x144)), SHOW_ROW, -1, 0);
    }
    if (m(self, 0x140)) {
        const DWORD sel = vc(bp(m(self, 0x138)), VALUE);
        mb(bp(m(self, 0x140)), W_ENABLED) = sel != 0xffffffff;
    }
    if (m(self, 0x174)) {
        const DWORD sel = vc(bp(m(self, 0x144)), VALUE);
        mb(bp(m(self, 0x174)), W_ENABLED) = sel != 0xffffffff;
    }
    const DWORD sel = vc(bp(m(self, 0x144)), VALUE);
    mb(bp(m(self, 0x178)), W_ENABLED) = sel != 0xffffffff;
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b5d0)(self, dt);
}
// A details window's deleting destructor (0x104 bytes, the frame window base's vtable)
BYTE *THISCALL SaveLoadDetails_DeletingDtor(BYTE *self, DWORD flags)   // 0x564c70
{
    m(self, 0) = vt(0x5ea72c);
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x104);
    return self;
}
// A row of the details (the page, the first text, its font's name, its x, its alignment, the second text, its
// font's name, its x, its alignment): two labels made on a dummy window (the screen's +0x14c parent) that lends
// them the fonts, placed below the page's last child (+ a third of the gap) with how 5 and 2; the second row's
// y from the first label's height
void THISCALL SaveLoadScreen_DetailRow(BYTE *self, BYTE *page, const char *text1, const char *font1, DWORD x1, DWORD align1,
                                       const char *text2, const char *font2, DWORD x2, DWORD align2)   // 0x564ce0
{
    alignas(4) BYTE dummy[0x100];
    window_ctor(dummy, m(self, 0x14c), glob<const char *>(0x620f30));
    int y = (signed char)mb(self, W_BORDER);
    if (BYTE *last = bp(m(page, W_LAST))) {
        const int *r = vc<const int *>(last, V_GET_RECT);
        y = r[3] + (signed char)mb(self, W_BORDER2) / 3 + 1;
    }
    const DWORD i1 = vc(glob<void *>(FONTS), 3, font1, -1);
    m(dummy, W_FONT) = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(FONTS), i1);
    BYTE *label = alloc(0x128);
    if (label)
        label = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(label, dummy, text1);
    m(label, 0x108) = align1;
    m(label, 0x10c) = 0xffffffff;
    vc(label, V_SET_TITLE, c_str(label + W_TITLE));
    vc(page, V_ADD_CHILD, label, 5, 0, x1, y, 2);
    BYTE *f = vc<BYTE *>(vc<BYTE *>(label, 45), 23, 0);
    const int h = (int)vc(f, 39);
    const int b = (signed char)mb(self, W_BORDER3);
    y += mi(label, W_RECT + 0xc) - (2 * b + 2) - mi(label, W_RECT + 4) - h;
    const DWORD i2 = vc(glob<void *>(FONTS), 3, font2, -1);
    m(dummy, W_FONT) = record_loaded(glob_addr(FONTS), i2);
    BYTE *label2 = alloc(0x128);
    if (label2)
        label2 = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(label2, dummy, text2);
    m(label2, 0x108) = align2;
    m(label2, 0x10c) = 0xffffffff;
    vc(label2, V_SET_TITLE, c_str(label2 + W_TITLE));
    vc(page, V_ADD_CHILD, label2, 5, 0, x2, y, 2);
    window_dtor(dummy);
}
// The list's sort compare (fastcall: the column; a value, the source, the rows i and j): the statistics of the
// two rows (the source's slot 5 object's +0x14 screen's array) compared by CZaxSaveGameStats::Compare 0x5bcc10
int __fastcall SaveLoadScreen_Compare(DWORD column, DWORD, DWORD, BYTE *source, DWORD i, DWORD j)   // 0x564ef0
{
    BYTE *screen = bp(m(vc<BYTE *>(source, 5), 0x14));
    BYTE *data = bp(m(screen, 0x18c));
    return fn<int(THISCALL *)(DWORD, DWORD, DWORD)>(0x5bcc10)(m(data, i * 4), m(data, j * 4), column);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_save_load)
{
    const Hook list[] = {
#include "ui_save_load_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
