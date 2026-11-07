// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the slots of the "Grid Control" (vtable 0x5ea35c, also the
// materials generator's grid 0x5f9a4c) and of its column header CJHeader (vtable 0x5ea4ac) that no table listed:
// the grid's drawing, wheel, drag, click, layout, key and row-drawing slots; the header's deleting destructor,
// drawing, cursor, drag (column widths) and button release (sorting). Layouts in ui_grid.cpp.
//
// Every call goes through its exe address (the original's side of a check runs the original all the way down).
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD G_HEADER = 0x100, G_COLOR = 0x104, G_SCROLL = 0x114, G_ROWS = 0x118, G_BAR = 0x11c, G_ROW_H = 0x120,
                G_SELECTED = 0x124, G_OWN_BAR = 0x128;
constexpr DWORD H_ARRAY = 0x100, H_COLUMNS = 0x104, H_COUNT = 0x108, H_CAPACITY = 0x10c, H_COLOR = 0x110, H_DRAG = 0x120,
                H_FILL = 0x124, H_PAD = 0x128, H_SORTED = 0x12c, H_SAME = 0x131, H_PRESSED = 0x132, H_FIXED = 0x133,
                H_PRESS_COLUMN = 0x134, H_SORT = 0x138, H_CHANGED = 0x13c;
constexpr DWORD G_IN_GRID_CLICK = 0x62e510;   // byte: the grid's slot 9 runs the button press

BYTE *column(BYTE *h, DWORD i) { return bp(at<DWORD *>(m(h, H_COLUMNS))[i]); }
inline DWORD c_str(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13a0)(z); }
inline int scroll_x(BYTE *w) { BYTE *s = bp(m(w, W_SCROLL)); return s ? mi(s, S_X) : 0; }
inline int scroll_y(BYTE *w) { BYTE *s = bp(m(w, W_SCROLL)); return s ? mi(s, S_Y) : 0; }
inline int col_width(BYTE *h, DWORD i) { return fn<int(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(h, i); }
inline int col_left(BYTE *h, DWORD i) { return fn<int(THISCALL *)(BYTE *, DWORD)>(0x402950)(h, i); }
inline void set_clip(BYTE *frame, int *r) { fn<void(THISCALL *)(BYTE *, int *)>(0x524f50)(frame, r); }
inline void select(BYTE *self, DWORD row, DWORD quiet) { fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x401930)(self, row, quiet); }
inline DWORD cursor_entry(int i) { return fn<DWORD(__fastcall *)(int)>(0x40e9e0)(i); }
inline DWORD slot_false4(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d)
{
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x40c0a0)(self, a, b, c, d);
}
inline void header_recalc(BYTE *h) { fn<DWORD(THISCALL *)(BYTE *)>(0x402bb0)(h); }
// |v| as cdq; xor; sub (INT_MIN stays negative)
inline int abs_(int v)
{
    const int s = v >> 31;
    return (v ^ s) - s;
}
// The column whose divider (its right edge) lies within 3 pixels of x and that may be resized (+0x18); -1 none
DWORD divider_at(BYTE *h, int x)
{
    for (DWORD i = 0; i < m(h, H_COUNT); i++) {
        BYTE *c = column(h, i);
        if (mb(c, 0x18) && abs_(x - mi(c, 4) - mi(c, 0) + 1) < 4)
            return i;
    }
    return 0xffffffff;
}
// The column x lies in (the first whose summed widths reach x); -1 none
DWORD column_at(BYTE *h, int x)
{
    DWORD found = 0xffffffff;
    int sum = 0;
    const DWORD n = m(h, H_COUNT);
    for (DWORD k = 0; k < n && found == 0xffffffff; k++) {
        sum += k < n ? mi(column(h, k), 4) : 0;
        if (x <= sum)
            found = k;
    }
    return found;
}

}  // namespace

// ---- The grid ---------------------------------------------------------------------------------------------------

// Slot 4: (frame) the visible rows (from the frame's clip rect, slot 36): the selected row's background (slot 78),
// each column's cell clipped to the column and the clip rect (slot 77), a line under each row (frame slot 51 in
// +0x104); a line right of every column over the full content height; the children (0x40b620); then the scroll
// position moved a step (ScrollStep 0x407440, at most a row) towards the one the selection keeps in view (+0x114
// kept), the scroll bar following
void THISCALL Grid_Draw(BYTE *self, BYTE *frame)   // 0x4012c0
{
    int clip[4];
    vc(frame, 36, clip);
    const int ch = mi(self, W_CONTENT_H);
    if (DWORD rows = m(self, G_ROWS)) {
        const DWORD rh = m(self, G_ROW_H);
        DWORD row = (DWORD)clip[1] / rh;
        DWORD last = (DWORD)clip[3] / rh;
        if (last >= rows - 1)
            last = rows - 1;
        DWORD y0 = rh * row;
        int cell[4] = {0, 0, mi(self, W_CONTENT_W), mi(self, W_CONTENT_H)};
        if (BYTE *bar = bp(m(self, G_BAR))) {
            int pt[2] = {mi(self, W_SCREEN), mi(self, W_SCREEN + 4)};
            fn<DWORD(THISCALL *)(BYTE *, int *, int *)>(0x412fb0)(bar, cell, pt);
        }
        cell[1] = (int)y0;
        cell[3] = (int)(m(self, G_ROW_H) + y0);
        for (; row <= last; row++) {
            if (row == m(self, G_SELECTED))
                vc(self, 78, row, frame, cell);
            for (DWORD col = 0; col < m(bp(m(self, G_HEADER)), H_COUNT); col++) {
                BYTE *h = bp(m(self, G_HEADER));
                const int w = col_width(bp(m(self, G_HEADER)), col);
                int right = col_left(h, col) + cell[0] + w;
                if (right >= cell[2])
                    right = cell[2];
                if (col_left(bp(m(self, G_HEADER)), col) + cell[0] >= right)
                    continue;
                int cb = cell[3];
                if (cb >= clip[3])
                    cb = clip[3];
                const int l = col_left(bp(m(self, G_HEADER)), col);
                int cr = col_width(bp(m(self, G_HEADER)), col) + cell[0] + l;
                if (cr >= cell[2])
                    cr = cell[2];
                if (cr >= clip[2])
                    cr = clip[2];
                int ct = clip[1];
                if (cell[1] > ct)
                    ct = cell[1];
                int cl = col_left(bp(m(self, G_HEADER)), col) + cell[0];
                if (cl <= clip[0])
                    cl = clip[0];
                int r[4] = {cl, ct, cr, cb};
                set_clip(frame, r);
                vc(self, 77, row, col, frame, cell);
                set_clip(frame, clip);
            }
            const int y = (int)(m(self, G_ROW_H) + y0 - 1);
            vc(frame, 51, 0, y, mi(self, W_RECT + 8) - mi(self, W_RECT) - 1, y, self + G_COLOR);
            const DWORD step = m(self, G_ROW_H);
            cell[1] += (int)step;
            cell[3] += (int)step;
            y0 += step;
        }
    }
    int x = 0;
    for (DWORD col = 0; col < m(bp(m(self, G_HEADER)), H_COUNT); col++) {
        x += col_width(bp(m(self, G_HEADER)), col);
        vc(frame, 51, x - 1, 0, x - 1, ch, self + G_COLOR);
    }
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
    const int sy = scroll_y(self);
    if (sy != mi(self, G_SCROLL)) {
        const int step = fn<int(__fastcall *)(int, int, int, int)>(0x407440)(sy, mi(self, G_SCROLL), 4, mi(self, G_ROW_H));
        const DWORD kept = m(self, G_SCROLL);
        fn<void(THISCALL *)(BYTE *, int)>(0x40d900)(self, step);
        if (BYTE *bar = bp(m(self, G_BAR)))
            fn<void(THISCALL *)(BYTE *, float, DWORD)>(0x413c20)(bar, (float)step, 0);
        m(self, G_SCROLL) = kept;
    }
}

// Slot 15, the wheel: (notches) the scroll position the selection keeps in view (+0x114) moved by 25 pixels a
// notch, kept within the content; true (al 1; the upper bytes those of the value written or of the scroll info)
DWORD THISCALL Grid_Wheel(BYTE *self, int notches, DWORD)   // 0x401600
{
    const int target = mi(self, G_SCROLL) - notches * 25;
    BYTE *s = bp(m(self, W_SCROLL));
    const int room = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + (s ? mi(s, S_H) : 0);
    if (room <= 0) {
        m(self, G_SCROLL) = 0;
        return (pv(s) & 0xffffff00u) | 1;
    }
    const int max = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + (s ? mi(s, S_H) : 0);
    if (target < 0) {
        m(self, G_SCROLL) = 0;
        return 1;
    }
    const int v = target > max ? max : target;
    mi(self, G_SCROLL) = v;
    return ((DWORD)v & 0xffffff00u) | 1;
}

// Slot 14, the drag: (point, delta, buttons, cursor out) the middle button scrolls by the vertical movement (the
// cursor 10); the left button selects the row under the point (inside the visible rect, with rows); during the
// grid's own click (0x62e510) and otherwise the base's slot 14 (0x40c0a0)
DWORD THISCALL Grid_Drag(BYTE *self, int *pt, int *delta, DWORD buttons, DWORD *cursor)   // 0x401690
{
    if (!gb(glob_addr(G_IN_GRID_CLICK))) {
        if ((BYTE)buttons & 4) {
            *cursor = cursor_entry(10);
            const DWORD r = fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x40d9f0)(self, 0, -delta[1]);
            return (r & 0xffffff00u) | 1;
        }
        if ((BYTE)buttons & 1) {
            if (pt[0] >= mi(self, W_VISIBLE) && pt[1] >= mi(self, W_VISIBLE + 4) && pt[0] < mi(self, W_VISIBLE + 8) &&
                pt[1] < mi(self, W_VISIBLE + 12)) {
                DWORD r = (DWORD)pt[1];   // (eax: the point's y, the row or Select's)
                const DWORD rows = m(self, G_ROWS);
                if (rows >= 1 && rows) {
                    r = (DWORD)(pt[1] - mi(self, W_SCREEN + 4) + scroll_y(self)) / m(self, G_ROW_H);
                    if (r != 0xffffffff)
                        r = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x401930)(self, r, 0);
                }
                return (r & 0xffffff00u) | 1;
            }
        }
    }
    return slot_false4(self, pv(pt), pv(delta), buttons, pv(cursor));
}

// Slot 9: (button, point, handled out) the left button runs the button press (0x40bcc0, with the grid's click
// flag set) and says "handled" when it took it, else invalidates (slot 38 with 0); true for the left button,
// false for the others (al; the upper bytes left over)
DWORD THISCALL Grid_Click(BYTE *self, DWORD button, DWORD pt, BYTE *handled)   // 0x401760
{
    if (button)
        return button & 0xffffff00u;
    gb(glob_addr(G_IN_GRID_CLICK)) = 1;
    const DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bcc0)(self, 0, pt);
    gb(glob_addr(G_IN_GRID_CLICK)) = 0;
    if (r) {
        *handled = 1;
        return (r & 0xffffff00u) | 1;
    }
    return (vc(self, V_INVALIDATE, 0) & 0xffffff00u) | 1;
}

// Slot 57, the rectangle: the base's (0x40d2a0); the header's fill width (+0x124) from the screen rect (cut by the
// scroll bar, 0x412fb0) and its columns recalculated; an own scroll bar moved to the right edge and the scroll
// position; the position the selection keeps in view clamped to the content; the scroll state updated (0x401e30)
void THISCALL Grid_SetRect(BYTE *self, int *rect)   // 0x4017b0
{
    fn<void(THISCALL *)(BYTE *, int *)>(0x40d2a0)(self, rect);
    BYTE *bar = bp(m(self, G_BAR));
    if (BYTE *h = bp(m(self, G_HEADER))) {
        (void)h;
        int r[4] = {mi(self, W_SCREEN), mi(self, W_SCREEN + 4), mi(self, W_SCREEN + 8), mi(self, W_SCREEN + 12)};
        if (bar) {
            int pt[2] = {0, 0};
            fn<DWORD(THISCALL *)(BYTE *, int *, int *)>(0x412fb0)(bar, r, pt);
        }
        mi(bp(m(self, G_HEADER)), H_FILL) = r[2] - r[0] + 1;
        header_recalc(bp(m(self, G_HEADER)));
    }
    if (bar && !mb(self, G_OWN_BAR)) {
        const int w = mi(self, W_RECT + 8) - mi(self, W_RECT);
        if (mi(vc<BYTE *>(bar, V_GET_RECT), 8) != w)
            fn<void(THISCALL *)(BYTE *, int)>(0x40d680)(bar, mi(self, W_RECT + 8) - mi(bar, W_RECT + 8) - mi(self, W_RECT) + mi(bar, W_RECT));
        fn<void(THISCALL *)(BYTE *, int)>(0x40d6c0)(bar, scroll_y(self));
        BYTE *s = bp(m(self, W_SCROLL));
        const int y = s ? mi(s, S_Y) : 0;
        if (mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + (s ? mi(s, S_H) : 0) > 0) {
            const int max = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + (s ? mi(s, S_H) : 0);
            mi(self, G_SCROLL) = y < 0 ? 0 : (y > max ? max : y);
        } else {
            m(self, G_SCROLL) = 0;
        }
    }
    fn<void(THISCALL *)(BYTE *)>(0x401e30)(self);
}

// Slot 78, the selected row's background: (row, frame, rect) a filled rectangle (frame slot 28) in colour +0xa0
DWORD THISCALL Grid_DrawSelection(BYTE *self, DWORD, BYTE *frame, int *rect)   // 0x401ab0
{
    return vc(frame, 28, rect, self + 0xa0);
}

// Slot 16, the keys: Home (5) the first row, Up (6) the previous, Down (10) the next (the first without a
// selection), End (14) the last, Enter (13) invalidates (slot 38 with 0); the others the base's (0x40c170)
DWORD THISCALL Grid_Key(BYTE *self, DWORD key)   // 0x401ad0
{
    switch (key) {
    case 5:
        select(self, 0, 0);
        return 1;
    case 6: {
        const DWORD s = m(self, G_SELECTED);
        if (s == 0xffffffff) {
            select(self, 0, 0);
            return 1;
        }
        if (s)
            select(self, s - 1, 0);
        return 1;
    }
    case 10: {
        const DWORD s = m(self, G_SELECTED);
        select(self, s == 0xffffffff ? 0 : s + 1, 0);
        return 1;
    }
    case 13:
        vc(self, V_INVALIDATE, 0);
        return 1;
    case 14:
        select(self, m(self, G_ROWS) - 1, 0);
        return 1;
    default:
        return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
    }
}

// ---- CJHeader ---------------------------------------------------------------------------------------------------

// Slot 0, the deleting destructor: the two callbacks deleted (slot 0 with 1), every column (its image through slot
// 1, its title, the column's 0x24 bytes) from the last, the column array freed through the pool, the window's
// destructor (0x40b190), the header's 0x140 bytes
BYTE *THISCALL Header_DeletingDtor(BYTE *self, DWORD flags)   // 0x402190
{
    m(self, 0) = vt(0x5ea4ac);
    if (BYTE *cb = bp(m(self, H_SORT))) {
        vc(cb, 0, 1);
        m(self, H_SORT) = 0;
    }
    if (BYTE *cb = bp(m(self, H_CHANGED))) {
        vc(cb, 0, 1);
        m(self, H_CHANGED) = 0;
    }
    while (DWORD n = m(self, H_COUNT)) {
        const DWORD i = n - 1;
        if (BYTE *c = bp(m(bp(m(self, H_COLUMNS)), i * 4))) {
            if (BYTE *img = bp(m(c, 0x14))) {
                vc(img, 1, 1);
                m(c, 0x14) = 0;
            }
            fn<void(THISCALL *)(BYTE *)>(0x4defd0)(c + 0xc);
            pool_free(c, 0x24);
        }
        const DWORD count = m(self, H_COUNT);
        if (i < count) {
            if (n < count)
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(bp(m(self, H_COLUMNS)) + i * 4,
                                                                             bp(m(self, H_COLUMNS)) + n * 4, (count - n) * 4);
            m(self, H_COUNT) = m(self, H_COUNT) - 1;
        }
    }
    m(self, H_ARRAY) = vt(0x5ea5e8);
    if (DWORD d = m(self, H_COLUMNS)) {
        pool_get_free(bp(d), m(self, H_CAPACITY) << 2);
        m(self, H_COLUMNS) = 0;
        m(self, H_CAPACITY) = 0;
    }
    m(self, H_COUNT) = 0;
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_free(self, 0x140);
    return self;
}

// Slot 4: (frame) every column: its image centred (Frame_BlitAt 0x4b3280 with the default blend 0x6bb4e0) or its
// title clipped to the column (left with the padding +0x128, centred, right with the padding) in the column's
// font, vertically centred by the first column's font; a divider line for resizable columns; a line under all
DWORD THISCALL Header_Draw(BYTE *self, BYTE *frame)   // 0x4022b0
{
    if (m(self, H_COUNT)) {
        const DWORD ty = (DWORD)(mi(self, W_RECT + 12) - mi(bp(m(column(self, 0), 0x10)), 0x80) - mi(self, W_RECT + 4)) >> 1;
        for (DWORD i = 0; i < m(self, H_COUNT); i++) {
            BYTE *c = column(self, i);
            if (BYTE *img = bp(m(c, 0x14))) {
                const DWORD h = vc(img, 39);
                const DWORD iy = (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4) - (int)h) >> 1;
                const DWORD w = vc(img, 45);
                BYTE *cc = column(self, i);
                const int x = (int)((DWORD)(mi(cc, 4) - (int)w) >> 1) + mi(cc, 0);
                fn<void(THISCALL *)(BYTE *, DWORD, int, DWORD, DWORD)>(0x4b3280)(frame, m(cc, 0x14), x, iy, glob_addr(0x6bb4e0));
            } else {
                int clip[4];
                vc(frame, 36, clip);
                int r[4] = {clip[0], clip[1], clip[2], clip[3]};
                BYTE *cc = column(self, i);
                const int cright = mi(cc, 0) + mi(cc, 4);
                if (clip[2] >= cright)
                    r[2] = clip[0] > cright ? clip[0] : cright;
                const int cleft = mi(column(self, i), 0);
                if (clip[0] < cleft)
                    r[0] = r[2] < cleft ? r[2] : cleft;
                set_clip(frame, r);
                BYTE *col = column(self, i);
                using Draw = void(THISCALL *)(DWORD, BYTE *, int, DWORD, DWORD, DWORD, DWORD);
                switch (m(col, 0x1c)) {
                case 0:
                    fn<Draw>(0x4a6470)(m(col, 0x10), frame, mi(self, H_PAD) + mi(col, 0), ty, c_str(col + 0xc), 0, 0);
                    break;
                case 1:
                    fn<Draw>(0x4a6470)(m(col, 0x10), frame, (int)(m(col, 4) >> 1) + mi(col, 0), ty, c_str(col + 0xc), 1, 0);
                    break;
                case 2:
                    fn<Draw>(0x4a6470)(m(col, 0x10), frame, mi(col, 4) - mi(self, H_PAD) + mi(col, 0), ty, c_str(col + 0xc), 2, 0);
                    break;
                default:
                    break;
                }
                set_clip(frame, clip);
            }
            BYTE *col = column(self, i);
            if (mb(col, 0x18)) {
                const int x = mi(col, 4) + mi(col, 0) - 1;
                vc(frame, 51, x, 0, x, mi(self, W_CONTENT_H) - 1, self + H_COLOR);
            }
        }
    }
    const int y = mi(self, W_CONTENT_H) - 1;
    return vc(frame, 51, 0, y, mi(self, W_CONTENT_W) - 1, y, self + H_COLOR);
}

// Slot 69, the cursor: the resize cursor (7) over a resizable column's divider (unless the header is fixed,
// +0x133); the base's (0x40ea00) otherwise
DWORD THISCALL Header_Cursor(BYTE *self, int *pt)   // 0x4024f0
{
    if (!mb(self, H_FIXED) && divider_at(self, pt[0]) != 0xffffffff)
        return cursor_entry(7);
    return fn<DWORD(THISCALL *)(BYTE *, int *)>(0x40ea00)(self, pt);
}

// Slot 14, the drag: (point, delta, buttons, cursor out) with the left button (and not fixed): a press on a divider
// remembers the column (+0x120), a press elsewhere the column pressed (+0x134, +0x132 set); dragging a divider
// changes the column's width by the movement (at least 1), recalculates (0x402bb0), invalidates (slot 38 with 1),
// tells the parent (slot 21 with 1) and the change callback (slot 1) and names the cursor 8; dragging after a press
// on a column says whether the point is still on that column (+0x131). The base's slot 14 (0x40c0a0) otherwise.
DWORD THISCALL Header_Drag(BYTE *self, int *pt, int *delta, DWORD buttons, DWORD *cursor)   // 0x402560
{
    if (mb(self, H_FIXED) || !(buttons & 1))
        return slot_false4(self, pv(pt), pv(delta), buttons, pv(cursor));
    const int x = pt[0] - mi(self, W_SCREEN) + scroll_x(self);
    if (!(mb(self, W_CAPTURE) & 1)) {
        const DWORD d = divider_at(self, x);
        m(self, H_DRAG) = d;
        if (d == 0xffffffff) {
            const DWORD c = column_at(self, x);
            m(self, H_PRESS_COLUMN) = c;
            if (c != 0xffffffff)
                mb(self, H_PRESSED) = 1;
        }
    }
    const DWORD d = m(self, H_DRAG);
    if (d != 0xffffffff) {
        const int dx = delta[0];
        int w = (d < m(self, H_COUNT) ? mi(column(self, d), 4) : 0) + dx;
        if (w < 1)
            w = 1;
        mi(column(self, d), 4) = w;
        header_recalc(self);
        vc(self, V_INVALIDATE, 1);
        if (BYTE *p = bp(m(self, W_PARENT)))
            vc(p, V_NOTIFY, self, 1);
        if (BYTE *cb = bp(m(self, H_CHANGED)))
            vc(cb, 1);
        const DWORD c = cursor_entry(8);
        *cursor = c;
        return (c & 0xffffff00u) | 1;
    }
    const DWORD pressed = m(self, H_PRESS_COLUMN);
    if (pressed == 0xffffffff)
        return slot_false4(self, pv(pt), pv(delta), buttons, pv(cursor));
    mb(self, H_SAME) = pressed == column_at(self, x);
    return 1;
}

// Slot 11, the button release: (button, point) a left click on a column (pressed and released on the same one)
// sorts by it: +0x12c the column, the parent told (slot 21 with 0), the sort callback (slot 1); then the base's
// (0x40bde0)
DWORD THISCALL Header_MouseUp(BYTE *self, DWORD button, DWORD pt)   // 0x402c20
{
    if (!button && mb(self, H_PRESSED)) {
        mb(self, H_PRESSED) = 0;
        if (mb(self, H_SAME)) {
            m(self, H_SORTED) = m(self, H_PRESS_COLUMN);
            m(self, H_PRESS_COLUMN) = 0xffffffff;
            if (BYTE *p = bp(m(self, W_PARENT)))
                vc(p, V_NOTIFY, self, button);
            if (BYTE *cb = bp(m(self, H_SORT)))
                vc(cb, 1);
        }
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bde0)(self, button, pt);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_c3_grid)
{
    const Hook list[] = {
#include "ui_c3_grid_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
