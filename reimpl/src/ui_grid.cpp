// The "Grid Control" (vtable 0x5ea35c, 0x12c bytes) and its column header CJHeader (vtable 0x5ea4ac, 0x140 bytes) of
// the window toolkit (docs/DECOMPILATION.md 9, "The window toolkit").
//
// Grid [confirmed: constructor 0x401000, the functions below]: the CJWindow part, then +0x100 its CJHeader, +0x104
//   a FieldColor (the header's, copied), +0x114 the scroll position the selection keeps in view, +0x118 the number
//   of rows, +0x11c the vertical CJScrollBar, +0x120 the row height (font line height + 2), +0x124 the selected row
//   (-1 none), +0x128 the scroll bar is the caller's (not deleted), +0x129 a guard while the bar is made.
// CJHeader [confirmed: constructor 0x4020d0]: +0x100 a pointer array of the columns {vtable 0x5ea5dc, data +0x104,
//   count +0x108, capacity +0x10c}, +0x110 a FieldColor (0xaf, 0x92, 0x36), +0x120 -1, +0x124 the width the last
//   column fills up to, +0x128 3, +0x12c, +0x132, +0x133, +0x134 -1, +0x138 and +0x13c two callbacks (objects with
//   a deleting destructor in slot 0; +0x13c's slot 1 is called when the columns change).
//   A column (0x24 bytes, pool): +0 its left, +4 its width, +8 its width as a fraction of the header's (float),
//   +0xc the title (ZString), +0x10 the font, +0x14, +0x18 a flag (1), +0x1c a value, +0x20 a flag.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD G_HEADER = 0x100, G_COLOR = 0x104, G_SCROLL = 0x114, G_ROWS = 0x118, G_BAR = 0x11c, G_ROW_H = 0x120,
                G_SELECTED = 0x124, G_OWN_BAR = 0x128, G_GUARD = 0x129;
constexpr DWORD H_COLUMNS = 0x104, H_COUNT = 0x108, H_COLOR = 0x110, H_FILL = 0x124, H_SORT = 0x138, H_CHANGED = 0x13c;

BYTE *column(BYTE *h, DWORD i) { return bp(at<DWORD *>(m(h, H_COLUMNS))[i]); }
int content_h(BYTE *w) { BYTE *s = bp(m(w, W_SCROLL)); return s ? mi(s, S_H) : 0; }
int height(BYTE *w) { return mi(w, W_RECT + 12) - mi(w, W_RECT + 4); }
void copy_color(BYTE *to, const BYTE *from)
{
    m(to, 0) = m(const_cast<BYTE *>(from), 0);
    m<WORD>(to, 4) = m<WORD>(const_cast<BYTE *>(from), 4);
    to[6] = from[6];
    m<WORD>(to, 8) = m<WORD>(const_cast<BYTE *>(from), 8);
    to[0xa] = from[0xa];
    m(to, 0xc) = m(const_cast<BYTE *>(from), 0xc);
}
// A callback object replaced: the old one deleted (slot 0 with 1)
void set_callback(BYTE *self, DWORD off, DWORD cb)
{
    if (BYTE *old = bp(m(self, off))) {
        vc(old, 0, 1);
        m(self, off) = 0;
    }
    m(self, off) = cb;
}
// The scroll bar's value (0x413c20, a float and "silent")
void bar_set_value(BYTE *bar, float v, DWORD silent) { fn<void(THISCALL *)(BYTE *, float, DWORD)>(0x413c20)(bar, v, silent); }

}  // namespace

// Clamp (fastcall: the value, the lower bound, the upper bound on the stack): below the lower bound the lower,
// above the upper the upper
int __fastcall Clamp(int v, int lo, int hi)   // 0x402070
{
    if (v < lo)
        return lo;
    return v > hi ? hi : v;
}

// ---- The grid -----------------------------------------------------------------------------------------------

// The constructor on a parent and a header: "Grid Control", takes the focus; the header gets a callback to the
// grid (0x18 bytes {vtable 0x5ea350, the grid, 0x4020c0, 0, 0, 0}) and fills its width + 1; the row height from
// the font; the header's colour
BYTE *THISCALL Grid_Ctor(BYTE *self, BYTE *parent, BYTE *header)   // 0x401000
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40a8a0)(self, parent, glob_addr(0x60a430));
    m(self, G_HEADER) = pv(header);
    fn<void(THISCALL *)(BYTE *)>(0x53f000)(self + G_COLOR);
    m(self, G_ROWS) = 0;
    m(self, G_BAR) = 0;
    m(self, G_SELECTED) = (DWORD)-1;
    mb(self, G_OWN_BAR) = 0;
    mb(self, G_GUARD) = 0;
    m(self, 0) = vt(0x5ea35c);
    mb(self, W_TAB_STOP) = 1;
    BYTE *cb = static_cast<BYTE *>(fn<void *(__cdecl *)(DWORD)>(0x5d033a)(0x18));
    if (cb) {
        m(cb, 8) = fn_addr(0x4020c0);
        m(cb, 0x10) = 0;
        m(cb, 0) = vt(0x5ea350);
        m(cb, 4) = pv(self);
        m(cb, 0xc) = 0;
        m(cb, 0x14) = 0;
    }
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x4029d0)(bp(m(self, G_HEADER)), cb);
    m(bp(m(self, G_HEADER)), H_FILL) = (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT) + 1);
    fn<void(THISCALL *)(BYTE *)>(0x402bb0)(bp(m(self, G_HEADER)));
    m(self, G_ROW_H) = fn<DWORD(THISCALL *)(BYTE *)>(0x4a6be0)(bp(m(self, W_FONT))) + 2;
    copy_color(self + G_COLOR, header + H_COLOR);
    mb(self, G_GUARD) = 1;
    return self;
}

// Whether the window has children (al; the upper bytes 0)
DWORD THISCALL Grid_HasChildren(BYTE *self)   // 0x4011f0
{
    return m(self, W_FIRST) ? 1 : 0;
}

// Slot 0: the base destructor and the pool block (0x12c)
BYTE *THISCALL Grid_DeletingDtor(BYTE *self, DWORD flags)   // 0x401290
{
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x12c);
    return self;
}

// The selected row (beyond the rows: the last; -1 none): a change redraws (slot 38) unless quiet and scrolls the
// row into view (the position +0x114 kept within the content); the scroll bar follows (silent)
void THISCALL Grid_Select(BYTE *self, DWORD row, DWORD quiet)   // 0x401930
{
    if (row != (DWORD)-1 && row >= m(self, G_ROWS))
        row = m(self, G_ROWS) - 1;
    if (m(self, G_SELECTED) != row) {
        m(self, G_SELECTED) = row;
        if (!quiet)
            vc(self, V_INVALIDATE, 1);
        const DWORD sel = m(self, G_SELECTED);
        if (sel != (DWORD)-1) {
            const int top = (int)(m(self, G_ROW_H) * sel);
            if (mi(self, G_SCROLL) > top) {
                const int range = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + content_h(self);
                if (range <= 0)
                    m(self, G_SCROLL) = 0;
                else
                    mi(self, G_SCROLL) = fn<int(__fastcall *)(int, int, int)>(0x402070)(top, 0, range);
            }
            const DWORD bottom = (sel + 1) * m(self, G_ROW_H) - 1;
            if (bottom > (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4) + mi(self, G_SCROLL))) {
                const int want = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + (int)bottom;
                const int range = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + content_h(self);
                if (range <= 0)
                    m(self, G_SCROLL) = 0;
                else if (want < 0)
                    m(self, G_SCROLL) = 0;
                else
                    mi(self, G_SCROLL) = want > range ? range : want;
            }
        }
    }
    if (BYTE *bar = bp(m(self, G_BAR)))
        bar_set_value(bar, (float)(long double)(unsigned long long)(m(self, G_ROW_H) * m(self, G_SELECTED)), 1);
}

// The number of rows: the content height (rows * row height - 1, at least 1; the width the scroll info's), a
// selection beyond the rows moves to the last one, the scroll position kept within the content, the scroll bar
// made or removed (0x401e30)
void THISCALL Grid_SetRows(BYTE *self, DWORD rows)   // 0x401bb0
{
    m(self, G_ROWS) = rows;
    BYTE *s = bp(m(self, W_SCROLL));
    const DWORD w = s ? m(s, S_W) : 0;
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d550)(self, w, rows ? m(self, G_ROW_H) * rows - 1 : 1);
    if (m(self, G_SELECTED) >= rows && m(self, G_SELECTED) != (DWORD)-1)
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x401930)(self, rows - 1, 0);
    s = bp(m(self, W_SCROLL));
    const int sy = s ? mi(s, S_Y) : 0, ch = s ? mi(s, S_H) : 0;
    if (height(self) + sy > ch) {
        if (ch < height(self))
            fn<void(THISCALL *)(BYTE *, int)>(0x40d900)(self, 0);
        else
            fn<void(THISCALL *)(BYTE *, int)>(0x40d900)(self, mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + ch);
    }
    fn<void(THISCALL *)(BYTE *)>(0x401e30)(self);
}

// The colour at +0x104 (a FieldColor; bytes 7 and 0xb are not copied)
DWORD THISCALL Grid_SetColor(BYTE *self, const BYTE *color)   // 0x401ca0
{
    copy_color(self + G_COLOR, color);
    return m(self, G_COLOR + 0xc);
}

// The scroll bar: the grid's own is made when missing (slot 79; the grid narrower by its width, added as a child
// with placing 0x13 while the guard is off) or fitted to the grid's height; its range is the content beyond the
// grid's height, its step the row height, its page the grid's height (floats)
void THISCALL Grid_UpdateBar(BYTE *self)   // 0x401cf0
{
    if (!mb(self, G_OWN_BAR)) {
        if (BYTE *bar = bp(m(self, G_BAR))) {
            if (mi(bar, W_RECT + 12) - mi(bar, W_RECT + 4) != height(self))
                fn<void(THISCALL *)(BYTE *, int)>(0x40f5d0)(bar, height(self));
        } else {
            const BYTE guard = mb(self, G_GUARD);
            mb(self, G_GUARD) = 0;
            mb(self, W_BORDER) = 0;
            BYTE *nb = bp(vc(self, 79));
            if (BYTE *old = bp(m(self, G_BAR))) {
                vc(old, V_DELETE, 1);
                m(self, G_BAR) = 0;
            }
            m(self, G_BAR) = pv(nb);
            mb(self, G_OWN_BAR) = 0;
            fn<void(THISCALL *)(BYTE *, int)>(0x40f5b0)(self, mi(self, W_RECT + 8) - mi(nb, W_RECT + 8) - mi(self, W_RECT) +
                                                                 mi(nb, W_RECT));
            vc(self, V_ADD_CHILD, nb, 0x13, 0, (DWORD)-1000, (DWORD)-1000, 0);
            mb(self, G_GUARD) = guard;
        }
    }
    BYTE *bar = bp(m(self, G_BAR));
    const int range = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + content_h(self);
    fn<void(THISCALL *)(BYTE *, DWORD, float)>(0x413bb0)(bar, 0, (float)range);
    const DWORD h = (DWORD)height(self);
    m<float>(bar, 0x150) = (float)(long double)(unsigned long long)m(self, G_ROW_H);
    m<float>(bar, 0x154) = (float)(long double)(unsigned long long)h;
}

// The scroll bar is needed when fewer rows show than there are (and more than one shows): made or updated; else
// the grid's own bar is deleted, the caller's emptied (range 0, step and page 0). Nothing for an empty rect.
void THISCALL Grid_UpdateScroll(BYTE *self)   // 0x401e30
{
    if (mi(self, W_RECT + 12) == mi(self, W_RECT + 4) || mi(self, W_RECT + 8) == mi(self, W_RECT))
        return;
    const DWORD rows = m(self, G_ROWS);
    DWORD shown = (DWORD)height(self) / m(self, G_ROW_H);
    if (shown > rows)
        shown = rows;
    if (shown < rows && rows > 1) {
        shown = (DWORD)height(self) / m(self, G_ROW_H);
        if (rows < shown)
            shown = rows;
        if (shown > 1) {
            fn<void(THISCALL *)(BYTE *)>(0x401cf0)(self);
            return;
        }
    }
    BYTE *bar = bp(m(self, G_BAR));
    if (!mb(self, G_OWN_BAR)) {
        if (bar) {
            vc(bar, V_DELETE, 1);
            m(self, G_BAR) = 0;
        }
    } else if (bar) {
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x413bb0)(bar, 0, 0);
        BYTE *b = bp(m(self, G_BAR));
        m(b, 0x150) = 0;
        m(b, 0x154) = 0;
    }
}

// Slot 79: a vertical CJScrollBar (0x168 bytes) on the grid
BYTE *THISCALL Grid_MakeBar(BYTE *self)   // 0x401ef0
{
    BYTE *bar = static_cast<BYTE *>(fn<void *(__fastcall *)(DWORD)>(0x417710)(0x168));
    if (!bar)
        return nullptr;
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x412ea0)(bar, self, 1);
}

// A scroll bar of the caller's (the old one deleted)
void THISCALL Grid_SetBar(BYTE *self, DWORD bar, char own)   // 0x401f20
{
    if (BYTE *old = bp(m(self, G_BAR))) {
        vc(old, V_DELETE, 1);
        m(self, G_BAR) = 0;
    }
    m(self, G_BAR) = bar;
    mb(self, G_OWN_BAR) = (BYTE)own;
}

// The row height, then the scroll bar
void THISCALL Grid_SetRowHeight(BYTE *self, DWORD h)   // 0x401f70
{
    m(self, G_ROW_H) = h;
    fn<void(THISCALL *)(BYTE *)>(0x401e30)(self);
}

// Slot 21: the scroll bar moved (code 1 from the grid's bar): the vertical scroll position follows its value
// (+0x114 clamped to the content meanwhile, restored after), the bar is set to it again (silent); other codes go
// to the base class
void THISCALL Grid_Notify(BYTE *self, BYTE *from, DWORD code)   // 0x401f90
{
    BYTE *bar = bp(m(self, G_BAR));
    if (code != 1) {
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40c400)(self, from, code);
        return;
    }
    if (!bar || from != bar)
        return;
    const int v = x87_ftol((long double)m<float>(bar, 0x14c));
    const int range = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + content_h(self);
    if (range <= 0)
        m(self, G_SCROLL) = 0;
    else if (v < 0)
        m(self, G_SCROLL) = 0;
    else
        mi(self, G_SCROLL) = v > range ? range : v;
    const DWORD kept = m(self, G_SCROLL);
    fn<void(THISCALL *)(BYTE *, int)>(0x40d900)(self, v);
    if (BYTE *b = bp(m(self, G_BAR)))
        bar_set_value(b, (float)v, 0);
    m(self, G_SCROLL) = kept;
}

// ---- The header ---------------------------------------------------------------------------------------------

// The constructor: "~CJHeader", no columns, the font's line height high and 100 wide, colour (0xaf, 0x92, 0x36)
BYTE *THISCALL Header_Ctor(BYTE *self, BYTE *parent)   // 0x4020d0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40a8a0)(self, parent, glob_addr(0x60a440));
    m(self, H_COLUMNS) = 0;
    m(self, H_COUNT) = 0;
    m(self, 0x10c) = 0;
    m(self, 0x100) = vt(0x5ea5dc);
    fn<void(THISCALL *)(BYTE *)>(0x53f000)(self + H_COLOR);
    m(self, 0x12c) = 0;
    mb(self, 0x132) = 0;
    m(self, 0x134) = (DWORD)-1;
    m(self, 0) = vt(0x5ea4ac);
    mb(self, 0x133) = 0;
    fn<void(THISCALL *)(BYTE *, int)>(0x40f5d0)(self, fn<int(THISCALL *)(BYTE *)>(0x4a6be0)(bp(m(self, W_FONT))));
    fn<void(THISCALL *)(BYTE *, int)>(0x40f5b0)(self, 100);
    m(self, 0x128) = 3;
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(self + H_COLOR, 0xaf, 0x92, 0x36, 0xff);
    m(self, 0x120) = (DWORD)-1;
    m(self, H_FILL) = 0;
    m(self, H_SORT) = 0;
    m(self, H_CHANGED) = 0;
    return self;
}

// A column: width, font (0: the header's), a value and a flag; its fraction of the header's width (float), its
// left after the column before, the title (ZString assign; eax its result)
DWORD THISCALL Header_AddColumn(BYTE *self, BYTE *title, DWORD width, DWORD font, DWORD value, char flag)   // 0x402770
{
    const DWORD i = m(self, H_COUNT);
    BYTE *col = static_cast<BYTE *>(fn<void *(__fastcall *)(DWORD)>(0x417710)(0x24));
    if (col) {
        fn<void(THISCALL *)(BYTE *)>(0x4dec90)(col + 0xc);
        m(col, 0x14) = 0;
        mb(col, 0x18) = 1;
    }
    BYTE *added = col;
    vc(self + 0x100, 2, &added);
    m(column(self, i), 4) = width;
    if (!font)
        font = m(self, W_FONT);
    m(column(self, i), 0x10) = font;
    m(column(self, i), 0x1c) = value;
    mb(column(self, i), 0x20) = (BYTE)flag;
    const int hw = mi(self, W_RECT + 8) - mi(self, W_RECT);
    m<float>(column(self, i), 8) = (float)((long double)(unsigned long long)width / (long double)hw);
    if (i) {
        BYTE *prev = bp(at<DWORD *>(m(self, H_COLUMNS))[i - 1]);
        m(column(self, i), 0) = m(prev, 4) + m(prev, 0);
    } else {
        m(column(self, i), 0) = 0;
    }
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x4e18f0)(column(self, i) + 0xc, title);
}

namespace {

// The columns changed: redraw, tell the parent (slot 21 code 1), the change callback (slot 1)
void columns_changed(BYTE *self)
{
    fn<void(THISCALL *)(BYTE *)>(0x402bb0)(self);
    vc(self, V_INVALIDATE, 1);
    if (BYTE *p = bp(m(self, W_PARENT)))
        vc(p, V_NOTIFY, self, 1);
    if (BYTE *cb = bp(m(self, H_CHANGED)))
        vc(cb, 1);
}

}  // namespace

// A column's width (at least 1), then the lefts again and the notifications
void THISCALL Header_SetColumnWidth(BYTE *self, DWORD i, int width)   // 0x402890
{
    m(column(self, i), 4) = (DWORD)(width < 1 ? 1 : width);
    columns_changed(self);
}

// A column's width, its flag, its left (no bounds check for the left, as the original)
DWORD THISCALL Header_ColumnWidth(BYTE *self, DWORD i)   // 0x4028f0
{
    return i < m(self, H_COUNT) ? m(column(self, i), 4) : 0;
}

DWORD THISCALL Header_ColumnFlag(BYTE *self, DWORD i)   // 0x402920 (al; the upper bytes are the index's)
{
    return (i & ~0xffu) | (i < m(self, H_COUNT) ? mb(column(self, i), 0x20) : 0u);
}

DWORD THISCALL Header_ColumnLeft(BYTE *self, DWORD i)   // 0x402950
{
    return m(column(self, i), 0);
}

// A column's title (ZString assign)
DWORD THISCALL Header_SetColumnTitle(BYTE *self, DWORD i, BYTE *title)   // 0x402970
{
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x4e18f0)(column(self, i) + 0xc, title);
}

// The two callbacks (the old one deleted)
void THISCALL Header_SetSortCallback(BYTE *self, DWORD cb)   // 0x402990
{
    set_callback(self, H_SORT, cb);
}

void THISCALL Header_SetChangeCallback(BYTE *self, DWORD cb)   // 0x4029d0
{
    set_callback(self, H_CHANGED, cb);
}

// A new width: every column whose width no longer matches its fraction of the old width gets a new fraction;
// then its width becomes the fraction of the new width (at least 1) with the notifications; the new width is
// the one the last column fills up to
void THISCALL Header_SetWidth(BYTE *self, DWORD width)   // 0x402a10
{
    DWORD count = m(self, H_COUNT);
    if (count) {
        const float old_w = (float)(long double)(unsigned long long)(DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT));
        const float new_w = (float)(long double)(unsigned long long)width;
        for (DWORD i = 0; i < count; i++) {
            const DWORD cur = i < count ? m(column(self, i), 4) : 0;
            BYTE *col = column(self, i);
            if ((DWORD)x87_ftol((long double)old_w * (long double)m<float>(col, 8)) != cur) {
                const DWORD w = i < count ? m(col, 4) : 0;
                m<float>(col, 8) = (float)((long double)(unsigned long long)w / (long double)old_w);
            }
            col = column(self, i);
            const int nw = x87_ftol((long double)new_w * (long double)m<float>(col, 8));
            m(col, 4) = (DWORD)(nw < 1 ? 1 : nw);
            columns_changed(self);
            count = m(self, H_COUNT);
        }
    }
    m(self, H_FILL) = width;
    fn<void(THISCALL *)(BYTE *)>(0x402bb0)(self);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40f5b0)(self, width);
}

// The colour at +0x110
DWORD THISCALL Header_SetColor(BYTE *self, const BYTE *color)   // 0x402b40
{
    copy_color(self + H_COLOR, color);
    return m(self, H_COLOR + 0xc);
}

// A column's flag at +0x18
void THISCALL Header_SetColumnFlag18(BYTE *self, DWORD i, char flag)   // 0x402b90
{
    mb(column(self, i), 0x18) = (BYTE)flag;
}

// The lefts one after another; the last column widened to reach the fill width
DWORD THISCALL Header_Recalc(BYTE *self)   // 0x402bb0
{
    DWORD x = 0;
    for (DWORD i = 0; i < m(self, H_COUNT); i++) {
        m(column(self, i), 0) = x;
        x += m(column(self, i), 4);
    }
    const DWORD fill = m(self, H_FILL);
    if (fill && m(self, H_COUNT)) {
        BYTE *last = column(self, m(self, H_COUNT) - 1);
        if (m(last, 0) < fill) {
            m(last, 4) = fill - m(last, 0);
            return fill - m(last, 0);
        }
    }
    return fill;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_grid)
{
    const Hook list[] = {
#include "ui_grid_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
