// CJListBox (vtable 0x5eabf8, 0x258 bytes, "~CJListBox"), the drop list it opens for a combo box (0x5ead94, 0x25c
// bytes) and their helpers, the list boxes of the window toolkit (docs/DECOMPILATION.md 9, "List boxes, combo
// boxes, edit and number fields").
//
// Layout [confirmed: the constructors 0x4041f0/0x404520, the defaults 0x404650 and the slots below]: the CJWindow
// part, then
//   +0x100 the rows (an array {vtable 0x5ead7c, data, count, capacity} of 0x14-byte rows from the pool: +0 the
//          item's id in the source, +4 expanded, +5 has children, +8 the depth, +0xc the parent's id, +0x10 the
//          tree lines still open, a bit per depth)
//   +0x110 the selected row (-1 none), +0x114 the scroll position in pixels, +0x118 the position it moves to (four
//          pixels or a row per frame while drawing), +0x11c the rows shown, +0x120 the rows that fit,
//   +0x124 sorted (1: the children are sorted by 0x406b70), +0x128 the source (an object with the slots 0 root,
//          1 parent, 2 text, 3 child count, 4 child, 5 a file list, 6 selectable row), +0x12c the widest text,
//   +0x130 the typed text (up to 0xf0 characters, cleared a second after the last key), +0x224 where it matched in
//          the row's text, +0x228 the time of the last key (0x4d2460), +0x22c the widest width allowed
//          ((font height / 5) * the width in characters), +0x230/+0x231 no frame, no row lines (a combo box's
//          closed list), +0x232 the layout is on (the constructors clear it while they size the box), +0x233 a tree
//          (some row has children: icons and tree lines), +0x234 the root's row is shown, +0x238 the row gap,
//          +0x23c a first child gets the top line, +0x23d files drawn with the open folder, +0x240 the compare
//          function of the sort (fastcall, or 0), +0x244 the column header (CJHeader), +0x248 the scroll bar,
//          +0x24c the box's own source over a list at +0x250 (ListBoxListSource 0x592350), +0x254 tool tips for
//          cut texts, +0x255 a bevel, +0x256 the scroll bar belongs to someone else, +0x257 the error sound when
//          typing finds nothing
// The drop list: the list box, then +0x258 the combo box it belongs to.
#include "ui_window.h"
#include "ptr_array.h"

namespace zax::ui {

namespace {

constexpr DWORD LB_ROWS = 0x100, LB_DATA = 0x104, LB_COUNT = 0x108, LB_CAP = 0x10c, LB_SEL = 0x110, LB_SCROLL = 0x114,
                LB_TARGET = 0x118, LB_SHOWN = 0x11c, LB_FIT = 0x120, LB_SORTED = 0x124, LB_SOURCE = 0x128,
                LB_TEXT_W = 0x12c, LB_TYPED = 0x130, LB_TYPED_POS = 0x224, LB_TYPED_TIME = 0x228, LB_MAX_W = 0x22c,
                LB_PLAIN = 0x230, LB_PLAIN2 = 0x231, LB_LAYOUT = 0x232, LB_TREE = 0x233, LB_ROOT_SHOWN = 0x234,
                LB_GAP = 0x238, LB_FIRST_LINE = 0x23c, LB_FILE_OPEN = 0x23d, LB_COMPARE = 0x240, LB_HEADER = 0x244,
                LB_BAR = 0x248, LB_OWN_SOURCE = 0x24c, LB_OWN_LIST = 0x250, LB_TIPS = 0x254, LB_BEVEL = 0x255,
                LB_BAR_FOREIGN = 0x256, LB_ERROR_SOUND = 0x257, DL_OWNER = 0x258;
constexpr DWORD ROW = 0x14, R_ID = 0, R_OPEN = 4, R_KIDS = 5, R_DEPTH = 8, R_PARENT = 0xc, R_LINES = 0x10;
constexpr DWORD TYPED_MAX = 0xf0;
// The combo box's members the drop list uses (ui_combo.cpp)
constexpr DWORD CB_LIST = 0x100, CB_DROP = 0x104, CB_SEL_ID = 0x114;
// Virtual slots of the list box
enum : int {
    S_DRAW_LIST = 76, S_SEL_ID = 77, S_SEL_TEXT = 78, S_SELECT_ID = 80, S_DRAW_ROW = 81, S_DRAW_TEXT = 82, S_ROW_TEXT = 83,
    S_ROW_H = 84, S_GAP = 85, S_INDENT = 86, S_LINE_LAST = 87, S_LINE_FIRST = 88, S_LINE_MID = 89, S_LINE_PASS = 90,
    S_DRAW_SEL = 91, S_ICON_OPEN = 92, S_ICON_CLOSED = 93, S_ICON_FILE = 94, S_ICON_W = 95, S_MAKE_BAR = 96
};
// The sort's globals (the list box being sorted, its source and compare function, the column, descending, numeric)
constexpr DWORD G_SORT_BOX = 0x62e550, G_SORT_SOURCE = 0x62e544, G_SORT_FN = 0x62e548, G_SORT_COLUMN = 0x60a494,
                G_SORT_HAS_COLUMN = 0x62e554, G_SORT_NUMERIC = 0x62e555, G_SORT_DOWN = 0x62e556;
constexpr DWORD G_IN_DOUBLE_CLICK = 0x62e54d;   // set while slot 9 runs the base's mouse down
constexpr DWORD G_ICONS = 0x62e5c8;              // the toolkit's resource list (icons)

DWORD h5(BYTE *self) { return m(bp(m(self, W_FONT)), 0x80) / 5; }
BYTE *row(BYTE *self, DWORD i) { return bp(m(self, LB_DATA)) + i * ROW; }
BYTE *source(BYTE *self) { return bp(m(self, LB_SOURCE)); }
DWORD row_h(BYTE *self) { return vc(self, S_ROW_H); }
// The row of an id (the loops the original inlines everywhere), -1 when it is not shown
DWORD row_of(BYTE *self, DWORD id)
{
    for (DWORD i = 0; i < m(self, LB_COUNT); i++)
        if (m(row(self, i), R_ID) == id)
            return i;
    return (DWORD)-1;
}
DWORD slen(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
void scopy(char *d, const char *s, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        d[i] = s[i];
}
// The exe's C runtime and the functions of this file through their addresses
char *crt_strchr(char *s, int c) { return fn<char *(__cdecl *)(char *, int)>(0x5d08c0)(s, c); }
void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
void *new_heap(DWORD n) { return fn<void *(__cdecl *)(DWORD)>(0x5d033a)(n); }
void delete_heap(void *p) { fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
DWORD now_ms() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }
char *find_no_case(const char *text, const char *what) { return fn<char *(__fastcall *)(const char *, const char *)>(0x587c70)(text, what); }
DWORD text_width(BYTE *self, const char *t, int n) { return fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, W_FONT)), t, n); }
void expand(BYTE *self, DWORD r) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x4068b0)(self, r); }
void collapse(BYTE *self, DWORD r) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x4069d0)(self, r); }
void show_id(BYTE *self, DWORD id) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x406920)(self, id); }
DWORD find_row(BYTE *self, DWORD id) { return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4067b0)(self, id); }
void select(BYTE *self, DWORD r, DWORD quiet) { fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4076c0)(self, r, quiet); }
void check_bar(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x407660)(self); }
DWORD bar_pos(BYTE *bar, DWORD v)
{
    return fn<DWORD(THISCALL *)(BYTE *, float, DWORD)>(0x413c20)(bar, (float)(long double)(unsigned long long)v, 1);
}
DWORD clamp(int v, int lo, int hi) { return fn<DWORD(__fastcall *)(int, int, int)>(0x402070)(v, lo, hi); }
DWORD column_width(BYTE *header, DWORD i) { return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(header, i); }
void set_clip(BYTE *frame, const int *r) { fn<void(THISCALL *)(BYTE *, const int *)>(0x524f50)(frame, r); }
void draw_text(BYTE *self, BYTE *frame, int x, int y, const char *t, DWORD align)
{
    fn<void(THISCALL *)(BYTE *, BYTE *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(bp(m(self, W_FONT)), frame, x, y, t, align, 0);
}
DWORD palette_index(BYTE *colour)
{
    if (mb(colour, 0xc) & 1)
        return mb(colour, 0xa);
    return fn<BYTE(THISCALL *)(BYTE *)>(0x53f160)(colour);
}
DWORD most_scroll(BYTE *self)
{
    if (m(self, LB_SHOWN) >= m(self, LB_COUNT))
        return 0;
    return row_h(self) * (m(self, LB_COUNT) - m(self, LB_SHOWN));
}
bool inside_visible(BYTE *self, const int *p)
{
    return p[0] >= mi(self, W_VISIBLE) && p[1] >= mi(self, W_VISIBLE + 4) && p[0] < mi(self, W_VISIBLE + 8) && p[1] < mi(self, W_VISIBLE + 12);
}
// The parent rows down to an id are expanded and the id selected (the pattern of 0x404390, 0x407270 and the
// combo box's 0x408a10/0x408bc0): the row of the id, else its parent shown and expanded first when the id is the
// root or the root has children
void select_id_in(BYTE *box, DWORD id, DWORD quiet)
{
    if (id == (DWORD)-1) {
        select(box, (DWORD)-1, quiet);
        return;
    }
    DWORD r = row_of(box, id);
    if (r != (DWORD)-1) {
        select(box, r, quiet);
        return;
    }
    BYTE *src = source(box);
    if (id == vc(src, 0) || vc(src, 3, vc(src, 0))) {
        const DWORD parent = vc(source(box), 1, id);
        DWORD pr = parent == (DWORD)-1 ? (DWORD)-1 : row_of(box, parent);
        if (pr == (DWORD)-1) {
            show_id(box, parent);
            pr = parent == (DWORD)-1 ? (DWORD)-1 : row_of(box, parent);
        }
        expand(box, pr);
    }
    select(box, row_of(box, id), quiet);
}
// The same with the parent through FindRow 0x4067b0 (the drop list's and SetSource's version)
void select_id_found(BYTE *box, DWORD id, DWORD quiet)
{
    if (id == (DWORD)-1) {
        select(box, (DWORD)-1, quiet);
        return;
    }
    const DWORD r = row_of(box, id);
    if (r != (DWORD)-1) {
        select(box, r, quiet);
        return;
    }
    BYTE *src = source(box);
    if (id == vc(src, 0) || vc(src, 3, vc(src, 0))) {
        const DWORD parent = vc(source(box), 1, id);
        DWORD pr = find_row(box, parent);
        if (pr == (DWORD)-1) {
            show_id(box, parent);
            pr = find_row(box, parent);
        }
        expand(box, pr);
    }
    select(box, find_row(box, id), quiet);
}
// One of the toolkit's icons (a sequence of the resource list 0x62e5c8, named once into a static handle): marked
// used (slot 33), the record loaded on first use (+0x78 the object, +0x7f loading; the file through slot 1,
// Path_Join 0x4a4220 and slot 37), as the resource lists' inline lookup
BYTE *icon(DWORD guard, BYTE bit, DWORD handle, DWORD name)
{
    if (!(gb(guard) & bit)) {
        gb(guard) |= bit;
        gd(handle) = vc(bp(gd(glob_addr(G_ICONS))), 2, at<const char *>(name), (DWORD)-1);
    }
    vc(bp(gd(glob_addr(G_ICONS))), 33, gd(handle), 1);
    BYTE *list = bp(gd(glob_addr(G_ICONS)));
    DWORD idx = gd(handle);
    if (!m(list, 0x30))
        return nullptr;
    if (idx >= m(list, 0x34))
        idx = 0;
    const DWORD off = idx * 0x80;
    BYTE *rec = bp(m(list, 0x30));
    if (mb(rec, off + 0x7f))
        return nullptr;
    if (!m(rec, off + 0x78) && !mb(rec, off + 0x7f)) {
        mb(rec, off + 0x7f) = 1;
        const char *file = reinterpret_cast<const char *>(vc(list, 1, idx));
        char buf[0x100];
        fn<void(__fastcall *)(char *, BYTE *, const char *)>(0x4a4220)(buf, list + 0x38, file);
        char *name_part = const_cast<char *>(path_name(buf));
        DWORD obj;
        if (name_part == buf) {
            obj = vc(list, 37, glob<const char *>(0x62e558), name_part, bp(m(list, 0x30)) + off + 0x78);
        } else {
            name_part[-1] = 0;
            obj = vc(list, 37, buf, name_part, bp(m(list, 0x30)) + off + 0x78);
        }
        m(bp(m(list, 0x30)), off + 0x78) = obj;
        mb(bp(m(list, 0x30)), off + 0x7f) = 0;
    }
    return bp(m(bp(m(list, 0x30)), off + 0x78));
}
// An icon centred at x, y (its height slot 39, width slot 45) through Frame_BlitAt with g_BlendTransparentMask
void draw_icon(BYTE *frame, BYTE *obj, int x, int y)
{
    const DWORD h = vc(obj, 39);
    const DWORD w = vc(obj, 45);
    fn<void(THISCALL *)(BYTE *, BYTE *, int, int, DWORD)>(0x4b3280)(frame, obj, x - (int)(w >> 1), y - (int)(h >> 1), glob_addr(0x6bcc68));
}
// The record of a handle of the resource list (loaded through DefinitionList_LoadRecord 0x55e090), for the widths
BYTE *icon_loaded(DWORD handle)
{
    BYTE *list = bp(gd(glob_addr(G_ICONS)));
    if (!m(list, 0x30))
        return nullptr;
    DWORD idx = gd(handle);
    if (idx >= m(list, 0x34))
        idx = 0;
    const DWORD off = idx * 0x80;
    if (mb(bp(m(list, 0x30)), off + 0x7f))
        return nullptr;
    if (!m(bp(m(list, 0x30)), off + 0x78))
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x55e090)(list, idx);
    return bp(m(bp(m(list, 0x30)), off + 0x78));
}

// The constructors' common part (the two differ in where the source goes and in +0x257)
void construct_tail(BYTE *self)
{
    const DWORD h = m(bp(m(self, W_FONT)), 0x80);
    m(self, LB_GAP) = (h / 5) * 2;
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const DWORD gap = vc(self, S_GAP);
    const DWORD w = m(self, LB_TEXT_W) < m(self, LB_MAX_W) ? m(self, LB_TEXT_W) : m(self, LB_MAX_W);
    const DWORD h2 = m(bp(m(self, W_FONT)), 0x80);
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, w + 3 + (h2 / 5) * 2, m(self, LB_SHOWN) * (gap + fh) + 2 + (h2 / 5) * 2);
    mb(self, LB_LAYOUT) = 1;
    m(self, LB_ROOT_SHOWN) = 0;
    mb(self, LB_FIRST_LINE) = 0;
}
void construct_head(BYTE *self, BYTE *parent)
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, glob<const char *>(0x60a498));
    m(self, LB_DATA) = 0;
    m(self, LB_COUNT) = 0;
    m(self, LB_CAP) = 0;
    m(self, LB_ROWS) = vt(0x5ead7c);
    m(self, LB_COMPARE) = 0;
    m(self, LB_HEADER) = 0;
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x592350)(self + LB_OWN_SOURCE, 0);
    m(self, 0) = vt(0x5eabf8);
}

}  // namespace

// The constructor (the parent, a list for the box's own source or 0, the widest width in characters, sorted)
BYTE *THISCALL ListBox_Ctor(BYTE *self, BYTE *parent, DWORD list, DWORD width, DWORD sorted)   // 0x4041f0
{
    construct_head(self, parent);
    mb(self, LB_ERROR_SOUND) = 1;
    mb(self, LB_LAYOUT) = 0;
    m(self, LB_MAX_W) = h5(self) * width;
    fn<void(THISCALL *)(BYTE *)>(0x404650)(self);
    m(self, LB_SORTED) = sorted;
    if (list) {
        m(self, LB_OWN_LIST) = list;
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x406fc0)(self, self + LB_OWN_SOURCE, 0);
    }
    construct_tail(self);
    return self;
}

// Slot 77: the selected row's id (-1 none)
DWORD THISCALL ListBox_SelectedId(BYTE *self)   // 0x404330
{
    const DWORD s = m(self, LB_SEL);
    if (s == (DWORD)-1)
        return s;
    return m(row(self, s), R_ID);
}

// Slot 78: the selected row's text (the source's slot 2), 0 without a selection
DWORD THISCALL ListBox_SelectedText(BYTE *self)   // 0x404350
{
    const DWORD s = m(self, LB_SEL);
    if (s == (DWORD)-1)
        return 0;
    return vc(source(self), 2, m(row(self, s), R_ID));
}

// Slot 79: slot 77
DWORD THISCALL ListBox_Value(BYTE *self)   // 0x404380
{
    return vc(self, S_SEL_ID);
}

// Slot 80, select an id (the id, quiet): its parents expanded first (select_id_in); a row not found leaves no
// selection. 0x407270 is a copy of it outside the vtable.
void THISCALL ListBox_SelectId(BYTE *self, DWORD id, DWORD quiet)   // 0x404390
{
    select_id_in(self, id, quiet);
}
void THISCALL ListBox_SelectId2(BYTE *self, DWORD id, DWORD quiet)   // 0x407270
{
    select_id_in(self, id, quiet);
}

// The deleting destructor: no longer the box the sort remembers, the rows back to the pool
BYTE *THISCALL ListBox_DeletingDtor(BYTE *self, DWORD flags)   // 0x4044a0
{
    m(self, 0) = vt(0x5eabf8);
    if (gd(glob_addr(G_SORT_BOX)) == pv(self))
        gd(glob_addr(G_SORT_BOX)) = 0;
    m(self, LB_ROWS) = vt(0x5ead88);
    if (m(self, LB_DATA)) {
        pool_get_free(bp(m(self, LB_DATA)), m(self, LB_CAP) * ROW);
        m(self, LB_DATA) = 0;
        m(self, LB_CAP) = 0;
    }
    m(self, LB_COUNT) = 0;
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_free(self, 0x258);
    return self;
}

// The second constructor (the parent, a source object or 0, the widest width in characters, sorted): the source
// as given (SetSource), +0x257 as the base left it
BYTE *THISCALL ListBox_Ctor2(BYTE *self, BYTE *parent, BYTE *src, DWORD width, DWORD sorted)   // 0x404520
{
    construct_head(self, parent);
    mb(self, LB_LAYOUT) = 0;
    m(self, LB_MAX_W) = h5(self) * width;
    fn<void(THISCALL *)(BYTE *)>(0x404650)(self);
    m(self, LB_SORTED) = sorted;
    if (src)
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x406fc0)(self, src, 0);
    construct_tail(self);
    return self;
}

// The defaults: a framed tab stop, one row, no selection, sorted, the colours 0x33/0x29/0x23 (slot 55), a bevel
void THISCALL ListBox_InitDefaults(BYTE *self)   // 0x404650
{
    m(self, LB_SORTED) = 1;
    mb(self, W_TAB_STOP) = 1;
    mb(self, W_FRAMED) = 1;
    mb(self, LB_TIPS) = 1;
    m(self, LB_FIT) = 1;
    m(self, LB_SHOWN) = 1;
    mb(self, LB_FILE_OPEN) = 0;
    mb(self, LB_PLAIN) = 0;
    mb(self, LB_PLAIN2) = 0;
    m(self, LB_BAR) = 0;
    m(self, LB_SEL) = (DWORD)-1;
    m(self, LB_TARGET) = 0;
    m(self, LB_SCROLL) = 0;
    m(self, LB_TEXT_W) = 0;
    m(self, LB_SOURCE) = 0;
    mb(self, LB_TYPED) = 0;
    m(self, LB_TYPED_TIME) = 0;
    m(self, LB_TYPED_POS) = 0;
    vc(self, 55, 0x33, 0x29, 0x23);
    mb(self, LB_BEVEL) = 1;
    mb(self, LB_BAR_FOREIGN) = 0;
}

// Slot 57, the rectangle: the base's (0x40d2a0); with the layout on the rows that fit, the scroll bar kept at the
// right edge (0x40d680), the bar checked (0x407660)
void THISCALL ListBox_SetRect(BYTE *self, const int *r)   // 0x4046f0
{
    fn<void(THISCALL *)(BYTE *, const int *)>(0x40d2a0)(self, r);
    if (!mb(self, LB_LAYOUT))
        return;
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const DWORD rh = row_h(self);
    const DWORD n = ((DWORD)mi(self, W_RECT + 12) - fh / 5 - fh / 5 - (DWORD)mi(self, W_RECT + 4) - 2) / rh;
    m(self, LB_FIT) = n;
    m(self, LB_SHOWN) = n;
    if (n > m(self, LB_COUNT))
        m(self, LB_SHOWN) = m(self, LB_COUNT);
    BYTE *bar = bp(m(self, LB_BAR));
    if (bar && !mb(self, LB_BAR_FOREIGN)) {
        const int w = mi(self, W_RECT + 8) - mi(self, W_RECT);
        if (mi(bp(vc(bar, V_GET_RECT)), 8) != w)
            fn<DWORD(THISCALL *)(BYTE *, int)>(0x40d680)(bar, mi(self, W_RECT + 8) - mi(bar, W_RECT + 8) - mi(self, W_RECT) + mi(bar, W_RECT));
    }
    check_bar(self);
}

// Slots 87-90, the tree lines of a row at x, y (the frame, x, y, the depth, the row's last flag; the colour +0x90;
// half the indent a, half the row height b): 87 └ the last child, 88 ┌ a first child, 89 ├ a middle child,
// 90 │ a line passing a deeper row
void THISCALL ListBox_LineLast(BYTE *self, BYTE *frame, int x, int y, DWORD, DWORD)   // 0x4047c0
{
    const int a = (int)(vc(self, S_INDENT) >> 1), b = (int)(row_h(self) >> 1);
    BYTE *c = self + 0x90;
    vc(frame, 51, x + a, y, x + a, y + b, c);
    vc(frame, 51, x + a, y + b, x + 2 * a, y + b, c);
}
void THISCALL ListBox_LineFirst(BYTE *self, BYTE *frame, int x, int y, DWORD, DWORD)   // 0x404830
{
    const int a = (int)(vc(self, S_INDENT) >> 1), b = (int)(row_h(self) >> 1);
    BYTE *c = self + 0x90;
    vc(frame, 51, x + a, y + b, x + a, y + 2 * b, c);
    vc(frame, 51, x + a, y + b, x + 2 * a, y + b, c);
}
void THISCALL ListBox_LineMiddle(BYTE *self, BYTE *frame, int x, int y, DWORD, DWORD)   // 0x4048b0
{
    const int a = (int)(vc(self, S_INDENT) >> 1), b = (int)(row_h(self) >> 1);
    BYTE *c = self + 0x90;
    vc(frame, 51, x + a, y, x + a, y + 2 * b, c);
    vc(frame, 51, x + a, y + b, x + 2 * a, y + b, c);
}
void THISCALL ListBox_LinePass(BYTE *self, BYTE *frame, int x, int y, DWORD, DWORD)   // 0x404930
{
    const int a = (int)(vc(self, S_INDENT) >> 1), h = (int)row_h(self);
    vc(frame, 51, x + a, y, x + a, y + h, self + 0x90);
}

// Slots 92-94, the icons of a row centred at x, y: "Folder Open Icon", "Folder Closed Icon", "File Icon" (with
// +0x23d the open folder)
void THISCALL ListBox_IconOpen(BYTE *self, BYTE *frame, int x, int y)   // 0x404980
{
    (void)self;
    draw_icon(frame, icon(glob_addr(0x62e534), 1, glob_addr(0x62e528), glob_addr(0x60a4a4)), x, y);
}
void THISCALL ListBox_IconClosed(BYTE *self, BYTE *frame, int x, int y)   // 0x404af0
{
    (void)self;
    draw_icon(frame, icon(glob_addr(0x62e541), 1, glob_addr(0x62e524), glob_addr(0x60a4b8)), x, y);
}
void THISCALL ListBox_IconFile(BYTE *self, BYTE *frame, int x, int y)   // 0x404c60
{
    if (mb(self, LB_FILE_OPEN)) {
        vc(self, S_ICON_OPEN, frame, x, y);
        return;
    }
    draw_icon(frame, icon(glob_addr(0x62e54c), 1, glob_addr(0x62e52c), glob_addr(0x60a4cc)), x, y);
}

// Slot 21, a notification (from, code): code 1 from the box's scroll bar moves to its position (+0x14c, the
// target clamped, the position raw) and sets the bar back; others go to the base (0x40c400)
DWORD THISCALL ListBox_Notify(BYTE *self, BYTE *from, DWORD code)   // 0x404e00
{
    BYTE *bar = bp(m(self, LB_BAR));
    if (code != 1 || !bar || from != bar)
        return fn<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40c400)(self, from, code);
    const int v = x87_ftol(m<float>(bar, 0x14c));
    const int most = (int)most_scroll(self);
    m(self, LB_TARGET) = (DWORD)(v < 0 ? 0 : v > most ? most : v);
    m(self, LB_SCROLL) = (DWORD)v;
    return bar_pos(bp(m(self, LB_BAR)), (DWORD)v);
}

// Slot 91, the selected row's background at y (the content width less the scroll bar, 0x412fb0), colour +0xc0
void THISCALL ListBox_DrawSelection(BYTE *self, BYTE *frame, int y)   // 0x404ec0
{
    int r[4] = {0, 0, mi(self, W_CONTENT_W), mi(self, W_CONTENT_H)};
    BYTE *bar = bp(m(self, LB_BAR));
    if (bar) {
        int p[2] = {mi(self, W_SCREEN), mi(self, W_SCREEN + 4)};
        fn<void(THISCALL *)(BYTE *, int *, int *)>(0x412fb0)(bar, r, p);
    }
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const int rh = (int)row_h(self);
    fn<void(THISCALL *)(BYTE *, int, int, int, int, BYTE *)>(0x4b3a70)(frame, (int)(fh / 5) + r[0] + 1, y, r[2] - (int)(fh / 5) - 1, rh + y,
                                                                    self + 0xc0);
}

// Slot 4: the list (slot 76) raised
void THISCALL ListBox_Draw(BYTE *self, BYTE *frame)   // 0x404f60
{
    vc(self, S_DRAW_LIST, frame, 0);
}

// Slot 82, a row's text at x, y: with a header of more than one column each tab-separated part in its column (cut
// to it, aligned by the column's +0x1c: 0 left after the header's padding +0x128, 2 right before it, else
// centred), else the whole text; the clip less the scroll bar (0x524f50)
void THISCALL ListBox_DrawText(BYTE *self, BYTE *frame, int x, int y, const char *text)   // 0x404f80
{
    BYTE *header = bp(m(self, LB_HEADER));
    if (!header || m(header, 0x108) <= 1) {
        int clip[4];
        vc(frame, 36, clip);
        int cut[4] = {clip[0], clip[1], clip[2], clip[3]};
        BYTE *bar = bp(m(self, LB_BAR));
        if (bar && cut[2] >= mi(bar, W_RECT)) {
            cut[2] = mi(bar, W_RECT) - 1;
            set_clip(frame, cut);
        }
        draw_text(self, frame, x, y, text, 0);
        if (m(self, LB_BAR))
            set_clip(frame, clip);
        return;
    }
    char *copy = static_cast<char *>(new_heap(slen(text) + 1));
    scopy(copy, text, slen(text) + 1);
    char *tab = crt_strchr(copy, 9);
    if (tab)
        *tab = 0;
    int clip[4];
    vc(frame, 36, clip);
    int saved[4] = {clip[0], clip[1], clip[2], clip[3]};
    BYTE *bar = bp(m(self, LB_BAR));
    if (bar && saved[2] >= mi(bar, W_RECT))
        saved[2] = mi(bar, W_RECT) - 1;
    char *part = copy;
    int col_x = 0;
    for (DWORD col = 0; part && col < m(bp(m(self, LB_HEADER)), 0x108); col++) {
        int r[4] = {saved[0], saved[1], saved[2], saved[3]};
        header = bp(m(self, LB_HEADER));
        int right = r[2];
        if (r[2] >= (int)column_width(header, col) + col_x) {
            const int end = (int)column_width(bp(m(self, LB_HEADER)), col) + col_x;
            right = r[0] > end ? r[0] : (int)column_width(bp(m(self, LB_HEADER)), col) + col_x;
            r[2] = right;
        }
        if (r[0] < col_x)
            r[0] = right < col_x ? right : col_x;
        set_clip(frame, r);
        header = bp(m(self, LB_HEADER));
        DWORD align = header ? m(bp(m(bp(m(header, 0x104)), col * 4)), 0x1c) : 0;
        int off = mi(header, 0x128);
        if (align) {
            off = (int)column_width(header, col) - off;
            if (align != 2)
                off = (int)(column_width(bp(m(self, LB_HEADER)), col) >> 1);
        }
        draw_text(self, frame, off + col_x + x, y, part, align);
        col_x += (int)column_width(bp(m(self, LB_HEADER)), col);
        if (tab) {
            part = tab + 1;
            tab = crt_strchr(part, 9);
            if (tab)
                *tab = 0;
        } else {
            part = nullptr;
        }
    }
    delete_heap(copy);
    set_clip(frame, clip);
}

// Slot 81, a row (the frame, x, y, dx, dy, the row): its text (slot 83's way; none for -1) through slot 82 at
// x + dx, y + dy
void THISCALL ListBox_DrawRow(BYTE *self, BYTE *frame, int x, int y, int dx, int dy, DWORD r)   // 0x405260
{
    const DWORD t = r == (DWORD)-1 ? 0 : vc(source(self), 2, m(row(self, r), R_ID));
    vc(self, S_DRAW_TEXT, frame, x + dx, y + dy, t);
}

// Slot 95: the widest of the three icons (slot 45)
DWORD THISCALL ListBox_IconWidth(BYTE *self)   // 0x4052c0
{
    (void)self;
    if (!(gb(glob_addr(0x62e540)) & 1)) {
        gb(glob_addr(0x62e540)) |= 1;
        gd(glob_addr(0x62e538)) = vc(bp(gd(glob_addr(G_ICONS))), 2, glob<const char *>(0x60a4b8), (DWORD)-1);
    }
    if (!(gb(glob_addr(0x62e540)) & 2)) {
        gb(glob_addr(0x62e540)) |= 2;
        gd(glob_addr(0x62e53c)) = vc(bp(gd(glob_addr(G_ICONS))), 2, glob<const char *>(0x60a4a4), (DWORD)-1);
    }
    if (!(gb(glob_addr(0x62e540)) & 4)) {
        gb(glob_addr(0x62e540)) |= 4;
        gd(glob_addr(0x62e530)) = vc(bp(gd(glob_addr(G_ICONS))), 2, glob<const char *>(0x60a4cc), (DWORD)-1);
    }
    vc(bp(gd(glob_addr(G_ICONS))), 33, gd(glob_addr(0x62e538)), 1);
    vc(bp(gd(glob_addr(G_ICONS))), 33, gd(glob_addr(0x62e53c)), 1);
    vc(bp(gd(glob_addr(G_ICONS))), 33, gd(glob_addr(0x62e530)), 1);
    BYTE *closed = icon_loaded(glob_addr(0x62e538));
    BYTE *open = icon_loaded(glob_addr(0x62e53c));
    BYTE *file = icon_loaded(glob_addr(0x62e530));
    DWORD w = vc(open, 45);
    const DWORD wc = vc(closed, 45);
    if (w <= wc)
        w = wc;
    const DWORD wf = vc(file, 45);
    return w > wf ? w : wf;
}

// Slot 76, the list (the frame, sunken): disabled at reduced opacities (0.4 and 0.6, put back to 1); the bevel
// (+0x255) in the static rect 0x7161a0; the selected row's background (slot 91, when more than one row shows or
// there is one row) and the typed text marked in it (+0x130, cleared after a second; a caret in a combo box's
// closed list when the reporter's flag 8 is set); the visible rows (slot 81), their icons (slots 92-94) and tree
// lines (slots 87-90); the children (0x40b620); the scroll position moved towards the target (a row or four
// pixels) and the bar set to it
void THISCALL ListBox_DrawList(BYTE *self, BYTE *frame, DWORD sunken)   // 0x405480
{
    const bool enabled = fn<bool(THISCALL *)(BYTE *)>(0x40c630)(self);
    if (!enabled) {
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6bcc68), 0.4f);
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6b9728), 0.6f);
    }
    DWORD icon_w = 0;
    if (mb(self, LB_TREE))
        icon_w = vc(self, S_ICON_W);
    const DWORD rh = row_h(self);
    if (mb(self, LB_BEVEL)) {
        const DWORD c1 = palette_index(self + 0xb0) & 0xff;
        const DWORD c2 = palette_index(self + 0xa0) & 0xff;
        if (!(gb(glob_addr(0x716190)) & 1)) {
            gb(glob_addr(0x716190)) |= 1;
            fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x401270));
        }
        int *stat = glob<int *>(0x7161a0);
        stat[3] = mi(self, W_CONTENT_H);
        stat[2] = mi(self, W_CONTENT_W);
        stat[0] = 0, stat[1] = 0;
        fn<DWORD(__fastcall *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(bp(m(self, W_FONT)), frame, stat, c2, c1, sunken,
                                                                                              0xf, 0);
    }
    DWORD indent = vc(self, S_INDENT);
    if (mb(self, LB_PLAIN) || mb(self, LB_PLAIN2))
        indent = 0, icon_w = 0;
    const DWORD sel = m(self, LB_SEL);
    if (sel != (DWORD)-1) {
        const DWORD fh5 = m(bp(m(self, W_FONT)), 0x80) / 5;
        const int x = (int)(m(row(self, sel), R_DEPTH) * indent + fh5 + icon_w + 1);
        const int y = (int)(sel * rh - m(self, LB_SCROLL) + fh5 + 1);
        if (m(self, LB_COUNT)) {
            if (m(self, LB_SHOWN) > 1 || m(self, LB_COUNT) == 1)
                vc(self, S_DRAW_SEL, frame, y);
            int marked = 0;
            if (mb(self, LB_TYPED)) {
                if (now_ms() > m(self, LB_TYPED_TIME) + 1000) {
                    mb(self, LB_TYPED) = 0;
                    m(self, LB_TYPED_TIME) = 0;
                    m(self, LB_TYPED_POS) = 0;
                } else {
                    const char *t = reinterpret_cast<const char *>(vc(self, S_ROW_TEXT, m(self, LB_SEL)));
                    const DWORD n = slen(reinterpret_cast<const char *>(self + LB_TYPED));
                    const int w1 = (int)text_width(self, t + m(self, LB_TYPED_POS), (int)n);
                    const int w2 = (int)text_width(self, t, (int)m(self, LB_TYPED_POS));
                    fn<void(THISCALL *)(BYTE *, int, int, int, int, BYTE *)>(0x4b3a70)(frame, x + w2, y, x + w1 + w2, y + (int)rh, self + 0xa0);
                    marked = w1 + w2;
                }
            }
            if (mb(self, W_ACTIVE) && m(self, LB_SHOWN) == 1 && mb(self, LB_PLAIN) && enabled && (gb(gd(glob_addr(0x6c02cc)) + 0x38) & 8)) {
                const int cx = x + marked;
                vc(frame, 51, cx, 1, cx, mi(self, W_CONTENT_H) - 2, self + 0xa0);
            }
        }
    }
    const DWORD scroll = m(self, LB_SCROLL);
    const DWORD left = m(bp(m(self, W_FONT)), 0x80) / 5 + 1;
    const DWORD first = scroll / rh;
    const DWORD height = (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4));
    const DWORD inner = height < 2 * left ? 0 : height - 2 * left;
    DWORD last = (scroll + inner + rh - 1) / rh;
    if (last > m(self, LB_COUNT))
        last = m(self, LB_COUNT);
    int prev_depth = -1;
    int y = (int)(left + first * rh - scroll);
    for (DWORD i = first; i < last; i++, y += (int)rh) {
        BYTE *r = row(self, i);
        const int dx = (int)((m(r, R_DEPTH) + m(self, LB_ROOT_SHOWN)) * indent);
        vc(self, S_DRAW_ROW, frame, left, y, dx + (int)icon_w, m(self, LB_GAP) >> 1, i);
        if (!mb(self, LB_TREE) || mb(self, LB_PLAIN) || mb(self, LB_PLAIN2))
            continue;
        if (icon_w) {
            r = row(self, i);
            const int ix = (int)(icon_w >> 1) + dx + (int)left, iy = (int)(rh >> 1) + y;
            if (mb(r, R_KIDS))
                vc(self, mb(r, R_OPEN) ? S_ICON_OPEN : S_ICON_CLOSED, frame, ix, iy);
            else
                vc(self, S_ICON_FILE, frame, ix, iy);
        }
        const DWORD depth = m(row(self, i), R_DEPTH);
        int x = (int)left;
        for (DWORD j = m(self, LB_ROOT_SHOWN) == 0 ? 1 : 0; j <= depth; j++, x += (int)indent) {
            r = row(self, i);
            if (j == depth) {
                // the last flag goes in al over the rest of the row's address, as the original pushes it
                const DWORD last_flag = (pv(r) & 0xffffff00u) | (mb(r, R_KIDS) == 0 ? 1u : 0u);
                if (m(r, R_LINES) & (1u << j))
                    vc(self, S_LINE_LAST, frame, x, y, j, last_flag);
                else if (mb(self, LB_FIRST_LINE) && prev_depth < (int)depth)
                    vc(self, S_LINE_FIRST, frame, x, y, j, last_flag);
                else
                    vc(self, S_LINE_MID, frame, x, y, j, last_flag);
            } else if (!(m(r, R_LINES) & (1u << j))) {
                vc(self, S_LINE_PASS, frame, x, y, j, 0);
            }
        }
        prev_depth = (int)depth;
    }
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
    if (m(self, LB_SCROLL) != m(self, LB_TARGET)) {
        const int rh2 = (int)row_h(self);
        int target = mi(self, LB_TARGET);
        const int pos = mi(self, LB_SCROLL);
        int d = target - pos, dir = 1;
        if (d < 0)
            d = -d, dir = -1;
        if (d > rh2)
            target -= dir * rh2;
        else if (d > 4)
            target = pos + dir * 4;
        m(self, LB_SCROLL) = (DWORD)target;
        if (m(self, LB_BAR))
            bar_pos(bp(m(self, LB_BAR)), (DWORD)target);
    }
    if (!enabled) {
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6bcc68), 1.0f);
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6b9728), 1.0f);
    }
}

// Slot 9, a double click (the button, the point, out: taken): the left button first as a mouse down of the base
// (with 0x62e54d set; taken when it is); else the selected row expanded or collapsed and slot 38 with 0. al: 1, 0
// for other buttons.
DWORD THISCALL ListBox_DoubleClick(BYTE *self, DWORD button, const int *p, BYTE *taken)   // 0x405a90
{
    if (button)
        return 0;
    gb(glob_addr(G_IN_DOUBLE_CLICK)) = 1;
    const DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, 0, p);
    gb(glob_addr(G_IN_DOUBLE_CLICK)) = 0;
    if (r) {
        *taken = 1;
        return 1;
    }
    const DWORD sel = m(self, LB_SEL);
    if (sel == (DWORD)-1)
        return 1;
    BYTE *rw = row(self, sel);
    if (mb(rw, R_KIDS)) {
        if (mb(rw, R_OPEN)) {
            collapse(self, sel);
            vc(self, V_INVALIDATE, 0);
            return 1;
        }
        expand(self, sel);
    }
    vc(self, V_INVALIDATE, 0);
    return 1;
}

// Slot 10, a mouse button: the base's (0x40bcc0) as 0 or 1
DWORD THISCALL ListBox_MouseDown(BYTE *self, DWORD button, const int *p)   // 0x405b30
{
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, p) ? 1 : 0;
}

// Slot 14, a drag (the point, the move, the buttons, out: the cursor): with the left button inside the box (not
// during a double click) the row under the point selected when more than one row shows or none is selected;
// else the base's (0x40c0a0). al: 1.
DWORD THISCALL ListBox_MouseDrag(BYTE *self, const int *p, const int *delta, DWORD buttons, DWORD *cursor)   // 0x405b50
{
    if (gb(glob_addr(G_IN_DOUBLE_CLICK)) || !(buttons & 1) || !inside_visible(self, p))
        return fn<DWORD(THISCALL *)(BYTE *, const int *, const int *, DWORD, DWORD *)>(0x40c0a0)(self, p, delta, buttons, cursor);
    const DWORD shown = m(self, LB_SHOWN);
    if (shown <= 1 && (shown == 0 || m(self, LB_SEL) != (DWORD)-1))
        return 1;
    BYTE *scroll = bp(m(self, W_SCROLL));
    const int sy = scroll ? mi(scroll, S_Y) : 0;
    const DWORD y = (DWORD)(p[1] - mi(self, W_SCREEN + 4) + sy);
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const DWORD rh = row_h(self);
    const DWORD r = (m(self, LB_SCROLL) - fh / 5 + y) / rh;
    if (r < m(self, LB_COUNT))
        select(self, r, 0);
    return 1;
}

// Slot 15, the wheel (the steps): with more than one row shown the target moved by rows (clamped; al 1), else the
// keys 6 (up) or 10 (down) through slot 16
DWORD THISCALL ListBox_Wheel(BYTE *self, int delta, DWORD)   // 0x405c40
{
    if (m(self, LB_SHOWN) <= 1)
        return vc(self, V_KEY, delta > 0 ? 6 : 10) ? 1 : 0;
    const int t = mi(self, LB_TARGET) - (int)row_h(self) * delta;
    const int most = (int)most_scroll(self);
    if (t < 0) {
        m(self, LB_TARGET) = 0;
        return 1;
    }
    const int v = t > most ? most : t;
    m(self, LB_TARGET) = (DWORD)v;
    return ((DWORD)v & 0xffffff00u) | 1;
}

namespace {

// The first selectable row (the source's slot 6) from i up, from i down; -1 none
DWORD selectable_up(BYTE *self, DWORD i)
{
    for (; i < m(self, LB_COUNT); i++)
        if (vc<BYTE>(source(self), 6, i))
            return i;
    return (DWORD)-1;
}
DWORD selectable_down(BYTE *self, int i)
{
    for (; i >= 0; i--)
        if (vc<BYTE>(source(self), 6, (DWORD)i))
            return (DWORD)i;
    return (DWORD)-1;
}
void clear_typed(BYTE *self)
{
    mb(self, LB_TYPED) = 0;
    m(self, LB_TYPED_TIME) = 0;
    m(self, LB_TYPED_POS) = 0;
}

}  // namespace

// Slot 16, a key: the base's (0x40c170) first. In a tree 11 collapses or goes to the parent, 12 and '+' expand,
// '-' collapses, '*' expands and '/' collapses every row. Then 5/14 the first/last selectable row (source slot 6),
// 6/10 the one above/below, 7/15 a page; these clear the typed text. '/' takes the selected text up to its next
// '/' as the typed text. Printable keys and 8 (back) edit the typed text (cleared a second after the last key, at
// most 0xf0) and select the first leaf starting with it, else the first leaf containing it (+0x224 where), else
// any row (0x406810), else an entry of the source's file list (slot 5) as an id; nothing found takes the
// character back with the error sound (+0x257). Others clear the typed text and are not taken.
DWORD THISCALL ListBox_Key(BYTE *self, DWORD key)   // 0x405ce0
{
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key))
        return 1;
    if (mb(self, LB_TREE)) {
        const DWORD sel = m(self, LB_SEL);
        switch (key) {
        case 11: {
            if (sel == (DWORD)-1)
                return 1;
            BYTE *r = row(self, sel);
            if (mb(r, R_OPEN)) {
                collapse(self, sel);
                return 1;
            }
            if (m(r, R_DEPTH) == 0)
                return 1;
            const DWORD parent = vc(source(self), 1, m(r, R_ID));
            if (parent == (DWORD)-1) {
                select(self, parent, 0);
                return 1;
            }
            DWORD pr = row_of(self, parent);
            if (pr == (DWORD)-1) {
                show_id(self, parent);
                pr = find_row(self, parent);
            }
            select(self, pr, 0);
            return 1;
        }
        case 12:
        case '+':
            if (sel != (DWORD)-1)
                expand(self, sel);
            return 1;
        case '-':
            if (sel != (DWORD)-1)
                collapse(self, sel);
            return 1;
        case '*':
            for (DWORD i = 0; i < m(self, LB_COUNT); i++)
                if (mb(row(self, i), R_KIDS) && !mb(row(self, i), R_OPEN))
                    expand(self, i);
            return 1;
        case '/':
            for (DWORD i = 0; i < m(self, LB_COUNT); i++)
                if (mb(row(self, i), R_KIDS) && mb(row(self, i), R_OPEN))
                    collapse(self, i);
            return 1;
        default:
            break;
        }
    }
    DWORD target = (DWORD)-1;
    const DWORD sel = m(self, LB_SEL);
    switch (key) {
    case 5:
        target = selectable_up(self, 0);
        break;
    case 6:
        if (sel == (DWORD)-1)
            target = selectable_up(self, 0);
        else if (sel == 0) {
            clear_typed(self);
            return 1;
        } else
            target = selectable_down(self, (int)sel - 1);
        break;
    case 7:
        if (sel < m(self, LB_SHOWN) || sel == (DWORD)-1)
            target = selectable_up(self, 0);
        else
            target = selectable_up(self, sel - m(self, LB_SHOWN));
        break;
    case 10:
        if (sel == (DWORD)-1)
            target = selectable_up(self, 0);
        else if (sel + 1 >= m(self, LB_COUNT)) {
            clear_typed(self);
            return 1;
        } else
            target = selectable_up(self, sel + 1);
        break;
    case 14:
        target = selectable_down(self, (int)m(self, LB_COUNT) - 1);
        break;
    case 15: {
        const DWORD i = m(self, LB_SHOWN) + sel;
        if (i >= m(self, LB_COUNT))
            target = selectable_down(self, (int)m(self, LB_COUNT) - 1);
        else if (sel == (DWORD)-1)
            target = selectable_up(self, 0);
        else
            target = selectable_down(self, (int)i);
        break;
    }
    case '/': {
        const char *t = reinterpret_cast<const char *>(vc(self, S_SEL_TEXT));
        char *typed = reinterpret_cast<char *>(self + LB_TYPED);
        const DWORD n = slen(typed);
        if (n < slen(t)) {
            const char *slash = crt_strchr(const_cast<char *>(t + n), '/');
            if (slash && (DWORD)(slash - t) < TYPED_MAX) {
                const DWORD k = (DWORD)(slash - t);
                scopy(typed, t, k);
                typed[k] = 0;
                m(self, LB_TYPED_TIME) = now_ms();
            }
        }
        goto typing;
    }
    default:
        goto typing;
    }
    select(self, target, 0);
    clear_typed(self);
    return 1;

typing:
    if (!((key >= 0x20 && key <= 0x7e) || key == 8)) {
        clear_typed(self);
        return 0;
    }
    {
        const DWORD now = now_ms();
        if (now > m(self, LB_TYPED_TIME) + 1000)
            clear_typed(self);
        m(self, LB_TYPED_TIME) = now;
        char *typed = reinterpret_cast<char *>(self + LB_TYPED);
        DWORD n = slen(typed);
        if (n >= TYPED_MAX)
            return 1;
        if (key == 8) {
            if (n == 0)
                return 0;
            n--;
        } else {
            typed[n++] = (char)key;
        }
        typed[n] = 0;
        const DWORD len = slen(typed);
        for (DWORD i = 0; i < m(self, LB_COUNT); i++) {
            if (mb(row(self, i), R_KIDS))
                continue;
            const char *t = reinterpret_cast<const char *>(vc(self, S_ROW_TEXT, i));
            if (!fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(t, typed, len)) {
                select(self, i, 0);
                m(self, LB_TYPED_POS) = 0;
                return 1;
            }
        }
        DWORD pos = 0;
        for (DWORD i = 0; i < m(self, LB_COUNT); i++) {
            if (mb(row(self, i), R_KIDS))
                continue;
            const char *t = reinterpret_cast<const char *>(vc(self, S_ROW_TEXT, i));
            const char *p = find_no_case(t, typed);
            if (p) {
                pos = (DWORD)(p - t);
                select(self, i, 0);
                m(self, LB_TYPED_POS) = pos;
                return 1;
            }
        }
        const DWORD any = fn<DWORD(THISCALL *)(BYTE *, const char *, DWORD *, DWORD)>(0x406810)(self, typed, &pos, 0);
        if (any != (DWORD)-1) {
            select(self, any, 0);
            m(self, LB_TYPED_POS) = pos;
            return 1;
        }
        BYTE *files = bp(vc(source(self), 5));
        if (files) {
            const DWORD nf = vc(files, 22);
            for (DWORD j = 0; j < nf; j++) {
                const char *name = path_name(reinterpret_cast<const char *>(vc(files, 1, j)));
                const char *p = find_no_case(name, typed);
                if (!p)
                    continue;
                pos = (DWORD)(p - name);
                DWORD r = row_of(self, j);
                if (r == (DWORD)-1) {
                    show_id(self, j);
                    r = row_of(self, j);
                }
                select(self, r, 0);
                m(self, LB_TYPED_POS) = pos;
                return 1;
            }
        }
        if (mb(self, LB_ERROR_SOUND) && n) {
            typed[n - 1] = 0;
            play_sound(glob_addr(SOUND_ERROR));
        }
        return 1;
    }
}

// Slot 70, the tool tip at a point (content coordinates): with +0x254 the text of the row under it (or the
// selected one when one row shows) in the column under it, shown (0x40cd00) when it is wider than the column (or
// the box) less the borders; al 1. Else the base's (0x40ea10).
DWORD THISCALL ListBox_Tip(BYTE *self, const int *p)   // 0x406550
{
    if (mb(self, LB_TIPS)) {
        DWORD r = m(self, LB_SEL);
        int col_x = 0;
        const DWORD col = fn<DWORD(THISCALL *)(BYTE *, const int *, int *)>(0x407930)(self, p, &col_x);
        if (m(self, LB_SHOWN) > 1) {
            BYTE *scroll = bp(m(self, W_SCROLL));
            const int sy = scroll ? mi(scroll, S_Y) : 0;
            const DWORD y = (DWORD)(p[1] - mi(self, W_SCREEN + 4) + sy);
            const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
            const DWORD rh = row_h(self);
            r = (m(self, LB_SCROLL) - fh / 5 + y) / rh;
        }
        if (r < m(self, LB_COUNT) && col != (DWORD)-1) {
            DWORD room = (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT));
            const char *text = r == (DWORD)-1 ? nullptr : reinterpret_cast<const char *>(vc(source(self), 2, m(row(self, r), R_ID)));
            char *copy = static_cast<char *>(new_heap(slen(text) + 1));
            scopy(copy, text, slen(text) + 1);
            const char *part = text;
            if (m(self, LB_HEADER)) {
                room = column_width(bp(m(self, LB_HEADER)), col);
                part = copy;
                char *tab = crt_strchr(copy, 9);
                if (tab)
                    *tab = 0;
                for (DWORD i = 0; i < col && tab; i++) {
                    part = tab + 1;
                    tab = crt_strchr(const_cast<char *>(part), 9);
                    if (tab)
                        *tab = 0;
                }
            }
            if (part) {
                const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
                const DWORD tw = text_width(self, part, -1);
                if (tw + fh / 5 + fh / 5 + 2 > room) {
                    const DWORD rh = row_h(self);
                    const DWORD fh2 = m(bp(m(self, W_FONT)), 0x80);
                    const int y = (int)(rh * r + fh2 / 5 - m(self, LB_SCROLL) + 1) + mi(self, W_SCREEN + 4) + 2;
                    const int x = (int)(fh2 / 5) + mi(self, W_SCREEN) + col_x + 1;
                    fn<void(THISCALL *)(BYTE *, const char *, int, int)>(0x40cd00)(self, part, x, y);
                    if (copy)
                        delete_heap(copy);
                    return 1;
                }
            }
            if (copy)
                delete_heap(copy);
        }
    }
    return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x40ea10)(self, p);
}

// The row of an id, -1 when not shown
DWORD THISCALL ListBox_FindRow(BYTE *self, DWORD id)   // 0x4067b0
{
    if (id == (DWORD)-1)
        return id;
    return row_of(self, id);
}

// Slot 83: a row's text (0 for -1)
DWORD THISCALL ListBox_RowText(BYTE *self, DWORD r)   // 0x4067e0
{
    if (r == (DWORD)-1)
        return 0;
    return vc(source(self), 2, m(row(self, r), R_ID));
}

// The first row whose text contains a text (Text_FindNoCase 0x587c70; only leaves when asked), where in out; -1
DWORD THISCALL ListBox_FindText(BYTE *self, const char *what, DWORD *where, BYTE leaves_only)   // 0x406810
{
    for (DWORD i = 0; i < m(self, LB_COUNT); i++) {
        if (leaves_only && mb(row(self, i), R_KIDS))
            continue;
        const char *t = reinterpret_cast<const char *>(vc(self, S_ROW_TEXT, i));
        const char *p = find_no_case(t, what);
        if (p) {
            if (where)
                *where = (DWORD)(p - t);
            return i;
        }
    }
    return (DWORD)-1;
}

// The list of the box's own source (+0x250) and the source set to it
void THISCALL ListBox_SetList(BYTE *self, DWORD list)   // 0x406890
{
    m(self, LB_OWN_LIST) = list;
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x406fc0)(self, self + LB_OWN_SOURCE, 0);
}

// A row expanded: its children inserted below it (0x406e00), the rows shown, the bar checked
void THISCALL ListBox_Expand(BYTE *self, DWORD r)   // 0x4068b0
{
    BYTE *rw = row(self, r);
    if (mb(rw, R_OPEN) || !mb(rw, R_KIDS))
        return;
    mb(rw, R_OPEN) = 1;
    rw = row(self, r);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x406e00)(self, m(rw, R_ID), m(rw, R_DEPTH) + 1, r + 1, m(rw, R_LINES));
    const DWORD fit = m(self, LB_FIT);
    m(self, LB_SHOWN) = fit;
    if (fit > m(self, LB_COUNT))
        m(self, LB_SHOWN) = m(self, LB_COUNT);
    check_bar(self);
}

// An id's row made visible: its parent shown and expanded (when the id is the root or the root has children;
// the parent's row -1 when it has none, as the original passes it on)
void THISCALL ListBox_ShowId(BYTE *self, DWORD id)   // 0x406920
{
    BYTE *src = source(self);
    if (id != vc(src, 0) && !vc(src, 3, vc(src, 0)))
        return;
    const DWORD parent = vc(source(self), 1, id);
    DWORD pr = parent == (DWORD)-1 ? (DWORD)-1 : row_of(self, parent);
    if (pr == (DWORD)-1) {
        show_id(self, parent);
        pr = parent == (DWORD)-1 ? (DWORD)-1 : row_of(self, parent);
    }
    expand(self, pr);
}

// A row collapsed: the rows below it with it as their parent removed (expanded ones collapsed first), the rows
// shown; the bar checked in any case
void THISCALL ListBox_Collapse(BYTE *self, DWORD r)   // 0x4069d0
{
    if (mb(row(self, r), R_OPEN)) {
        mb(row(self, r), R_OPEN) = 0;
        const DWORD i = r + 1;
        while (i < m(self, LB_COUNT)) {
            BYTE *next = row(self, i);
            if (m(next, R_PARENT) != m(row(self, r), R_ID))
                break;
            if (mb(next, R_OPEN))
                collapse(self, i);
            const DWORD n = m(self, LB_COUNT);
            if (i < n) {
                if (i + 1 < n)
                    crt_memmove(row(self, i), row(self, i + 1), n * ROW - (i + 1) * ROW);
                m(self, LB_COUNT) = n - 1;
            }
        }
        const DWORD fit = m(self, LB_FIT);
        m(self, LB_SHOWN) = fit;
        if (fit > m(self, LB_COUNT))
            m(self, LB_SHOWN) = m(self, LB_COUNT);
    }
    check_bar(self);
}

// The column header (+0x244) and its sort callback (a callback object 0x5ea350 of 0x18 bytes bound to 0x406b00)
void THISCALL ListBox_SetHeader(BYTE *self, BYTE *header)   // 0x406aa0
{
    m(self, LB_HEADER) = pv(header);
    BYTE *cb = static_cast<BYTE *>(new_heap(0x18));
    if (cb) {
        m(cb, 8) = fn_addr(0x406b00);
        m(cb, 0xc) = 0;
        m(cb, 0x10) = 0;
        m(cb, 0) = vt(0x5ea350);
        m(cb, 4) = pv(self);
        m(cb, 0x14) = 0;
    }
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x402990)(header, cb);
}

// The header's click: the same column of the same box again sorts the other way, else ascending; all rows sorted
DWORD THISCALL ListBox_HeaderClick(BYTE *self)   // 0x406b00
{
    BYTE *header = bp(m(self, LB_HEADER));
    const DWORD col = header ? m(header, 0x12c) : 0;
    if (gd(glob_addr(G_SORT_COLUMN)) == col && gd(glob_addr(G_SORT_BOX)) == pv(self))
        gb(glob_addr(G_SORT_DOWN)) = gb(glob_addr(G_SORT_DOWN)) == 0;
    else
        gb(glob_addr(G_SORT_DOWN)) = 0;
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x406b70)(self, 0, m(self, LB_COUNT));
}

// The sort direction (true: descending); ecx unused
DWORD THISCALL ListBox_SetSortDown(BYTE *, DWORD down)   // 0x406b60
{
    gb(glob_addr(G_SORT_DOWN)) = (BYTE)down;
    return down;
}

// Rows sorted (the first, the count) when the box sorts: qsort 0x5d097c with 0x406c60 over the source's texts, in
// the header's column (+0x12c; its flag 0x402920: numeric) and the box's compare function; the direction kept for
// the same column of the same box
void THISCALL ListBox_Sort(BYTE *self, DWORD first, DWORD n)   // 0x406b70
{
    if (m(self, LB_SORTED) != 1 || !n)
        return;
    const DWORD saved = gd(glob_addr(G_SORT_FN));
    gd(glob_addr(G_SORT_SOURCE)) = m(self, LB_SOURCE);
    gd(glob_addr(G_SORT_FN)) = m(self, LB_COMPARE);
    BYTE *header = bp(m(self, LB_HEADER));
    const DWORD col = header ? m(header, 0x12c) : 0;
    if (gd(glob_addr(G_SORT_COLUMN)) != col || gd(glob_addr(G_SORT_BOX)) != pv(self))
        gb(glob_addr(G_SORT_DOWN)) = 0;
    gd(glob_addr(G_SORT_BOX)) = pv(self);
    header = bp(m(self, LB_HEADER));
    const DWORD col2 = header ? m(header, 0x12c) : 0;
    gd(glob_addr(G_SORT_COLUMN)) = col2;
    gb(glob_addr(G_SORT_NUMERIC)) = 0;
    gb(glob_addr(G_SORT_HAS_COLUMN)) = 0;
    header = bp(m(self, LB_HEADER));
    if (header && col2 < m(header, 0x108)) {
        gb(glob_addr(G_SORT_HAS_COLUMN)) = 1;
        gb(glob_addr(G_SORT_NUMERIC)) = fn<BYTE(THISCALL *)(BYTE *, DWORD)>(0x402920)(bp(m(self, LB_HEADER)), col2);
    }
    fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(row(self, first), n, ROW, fn_addr(0x406c60));
    gd(glob_addr(G_SORT_FN)) = saved;
}

namespace {

// The part of a text in the sort's column (the tabs before it skipped), copied into a buffer of 0x100
const char *column_part(const char *t, char *buf)
{
    const char *s = t;
    for (DWORD i = 0; i < gd(glob_addr(G_SORT_COLUMN)); i++)
        s = crt_strchr(const_cast<char *>(s), 9) + 1;
    char *tab = crt_strchr(const_cast<char *>(s), 9);
    if (tab)
        *tab = 0;
    if (!s)
        s = t;
    fn<char *(__cdecl *)(char *, const char *, DWORD)>(0x5d0b50)(buf, s, 0xff);
    buf[0xff] = 0;
    if (tab)
        *tab = 9;
    return buf;
}

}  // namespace

// The sort's compare (two rows): the texts (or their column's parts) without leading white space by the box's
// function, as numbers (atoi) or case-insensitively (_stricmp); negated when descending
int __cdecl ListBox_Compare(const DWORD *a, const DWORD *b)   // 0x406c60
{
    BYTE *src = bp(gd(glob_addr(G_SORT_SOURCE)));
    const char *ta = reinterpret_cast<const char *>(vc(src, 2, *a));
    const char *tb = reinterpret_cast<const char *>(vc(bp(gd(glob_addr(G_SORT_SOURCE))), 2, *b));
    char buf_a[0x100], buf_b[0x100];
    if (gb(glob_addr(G_SORT_HAS_COLUMN))) {
        ta = column_part(ta, buf_a);
        tb = column_part(tb, buf_b);
    }
    const char *sa = fn<const char *(__fastcall *)(const char *)>(0x587aa0)(ta);
    const char *sb = fn<const char *(__fastcall *)(const char *)>(0x587aa0)(tb);
    int r;
    if (gd(glob_addr(G_SORT_FN)))
        r = at<int(__fastcall *)(DWORD, const char *, const char *, DWORD, DWORD, DWORD)>(gd(glob_addr(G_SORT_FN)))(gd(glob_addr(G_SORT_COLUMN)), sa, sb, gd(glob_addr(G_SORT_SOURCE)), *a,
                                                                                                    *b);
    else if (gb(glob_addr(G_SORT_NUMERIC))) {
        const int nb = crt_atoi(sb);
        r = crt_atoi(sa) - nb;
    }
    else
        r = crt_stricmp(sa, sb);
    if (gb(glob_addr(G_SORT_DOWN)))
        r = -r;
    return r;
}

// An id's children inserted as rows (the id, their depth, the row they go to, the parent's tree lines): the rows
// grown as the array template does, each child's row (its child count through slot 3), the new rows sorted, the
// last one closing its line
void THISCALL ListBox_InsertChildren(BYTE *self, DWORD id, DWORD depth, DWORD at_row, DWORD lines)   // 0x406e00
{
    const DWORD n = vc(source(self), 3, id);
    DWORD to = at_row;
    for (DWORD k = 0; k < n; k++, to++) {
        const DWORD off = to * ROW;
        if (m(self, LB_CAP) < m(self, LB_COUNT) + 1) {
            const DWORD grown = array_grown_capacity(m(self, LB_COUNT), m(self, LB_CAP));
            const DWORD old = m(self, LB_CAP);
            if (old < grown) {
                m(self, LB_CAP) = grown;
                BYTE *p = static_cast<BYTE *>(pool_get_alloc(grown * ROW));
                if (m(self, LB_COUNT))
                    crt_memmove(p, bp(m(self, LB_DATA)), m(self, LB_COUNT) * ROW);
                if (m(self, LB_DATA)) {
                    pool_get_free(bp(m(self, LB_DATA)), old * ROW);
                    m(self, LB_DATA) = 0;
                }
                m(self, LB_DATA) = pv(p);
            }
        }
        BYTE *r = bp(m(self, LB_DATA)) + off;
        if (to < m(self, LB_COUNT))
            crt_memmove(r + ROW, r, m(self, LB_COUNT) * ROW - off);
        m(self, LB_COUNT) += 1;
        const DWORD child = vc(source(self), 4, id, k);
        m(r, R_ID) = child;
        mb(r, R_OPEN) = 0;
        m(r, R_DEPTH) = depth;
        const DWORD kids = vc(source(self), 3, child);
        m(r, R_PARENT) = id;
        mb(r, R_KIDS) = kids > 0;
        m(r, R_LINES) = lines;
    }
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x406b70)(self, at_row, n);
    if (n)
        m(row(self, n + at_row - 1), R_LINES) |= 1u << (depth & 31);
}

// The source (the source object, the root as a row): no selection, scroll 0, the rows rebuilt (the root expanded,
// or its children), the widest text (with the indents and icons of a tree), the rows shown; the id selected before
// selected again when it is less than the row count (as the original compares it), the bar checked
void THISCALL ListBox_SetSource(BYTE *self, BYTE *src, BYTE root_row)   // 0x406fc0
{
    const DWORD old = m(self, LB_SEL) == (DWORD)-1 ? (DWORD)-1 : m(row(self, m(self, LB_SEL)), R_ID);
    m(self, LB_SOURCE) = pv(src);
    const DWORD most = fn<DWORD(THISCALL *)(BYTE *)>(0x407480)(self);
    m(self, LB_TARGET) = clamp(0, 0, (int)most);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x4074b0)(self, 0);
    m(self, LB_TEXT_W) = 0;
    m(self, LB_SEL) = (DWORD)-1;
    mb(self, LB_TREE) = 0;
    if (m(self, LB_DATA)) {
        pool_get_free(bp(m(self, LB_DATA)), m(self, LB_CAP) * ROW);
        m(self, LB_DATA) = 0;
        m(self, LB_CAP) = 0;
    }
    m(self, LB_COUNT) = 0;
    if (m(self, LB_SOURCE)) {
        if (root_row) {
            if (m(self, LB_CAP) < 1)
                fn<void(THISCALL *)(BYTE *, DWORD)>(0x408eb0)(self + LB_ROWS, array_grown_capacity(0, m(self, LB_CAP)));
            BYTE *r = row(self, m(self, LB_COUNT));
            m(self, LB_COUNT) += 1;
            const DWORD root = vc(source(self), 0);
            m(r, R_ID) = root;
            mb(r, R_OPEN) = 0;
            m(r, R_DEPTH) = 0;
            mb(r, R_KIDS) = 1;
            m(r, R_PARENT) = root;
            m(r, R_LINES) = 1;
            expand(self, 0);
        } else {
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x406e00)(self, vc(source(self), 0), 0, 0, 0);
        }
        for (DWORD i = 0; i < m(self, LB_COUNT); i++) {
            const char *t = i == (DWORD)-1 ? nullptr : reinterpret_cast<const char *>(vc(source(self), 2, m(row(self, i), R_ID)));
            const DWORD w = text_width(self, t, -1);
            if (w > m(self, LB_TEXT_W))
                m(self, LB_TEXT_W) = w;
            if (mb(row(self, i), R_KIDS))
                mb(self, LB_TREE) = 1;
        }
    }
    if (mb(self, LB_TREE)) {
        const DWORD w = m(self, LB_TEXT_W);
        const DWORD indent = vc(self, S_INDENT);
        const DWORD iw = vc(self, S_ICON_W);
        m(self, LB_TEXT_W) = (w >> 1) + iw + w + indent * 4;
    }
    const DWORD fit = m(self, LB_FIT);
    m(self, LB_SHOWN) = fit;
    if (fit > m(self, LB_COUNT))
        m(self, LB_SHOWN) = m(self, LB_COUNT);
    if (old >= m(self, LB_COUNT))
        select(self, (DWORD)-1, 1);
    else
        select_id_found(self, old, 1);
    check_bar(self);
}

// The selected row moved to the middle of the rows shown (the target clamped, the position raw, the bar set)
void THISCALL ListBox_CenterSelection(BYTE *self)   // 0x407380
{
    const DWORD sel = m(self, LB_SEL);
    if (sel == (DWORD)-1)
        return;
    const DWORD shown = m(self, LB_SHOWN);
    int first = (int)(sel - (shown >> 1));
    if (first < 0)
        first = 0;
    if (shown + (DWORD)first > m(self, LB_COUNT))
        first = (int)(m(self, LB_COUNT) - shown);
    const int s = (int)row_h(self) * first;
    const int most = (int)most_scroll(self);
    m(self, LB_TARGET) = (DWORD)(s < 0 ? 0 : s > most ? most : s);
    m(self, LB_SCROLL) = (DWORD)s;
    if (m(self, LB_BAR))
        bar_pos(bp(m(self, LB_BAR)), (DWORD)s);
}

// The scroll range: the rows not shown times the row height
DWORD THISCALL ListBox_MostScroll(BYTE *self)   // 0x407480
{
    return most_scroll(self);
}

// The scroll position (and the bar's)
void THISCALL ListBox_SetScroll(BYTE *self, DWORD v)   // 0x4074b0
{
    m(self, LB_SCROLL) = v;
    if (m(self, LB_BAR))
        bar_pos(bp(m(self, LB_BAR)), v);
}

// The scroll bar: the own one made (slot 96, at the right edge: the box narrowed, a child 0x13) or kept as high
// as the box; its range (0 .. the scroll range), its step (+0x150, the row height) and page (+0x154, the height)
void THISCALL ListBox_UpdateBar(BYTE *self)   // 0x4074f0
{
    if (!mb(self, LB_BAR_FOREIGN)) {
        BYTE *bar = bp(m(self, LB_BAR));
        if (bar) {
            const int h = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
            if (mi(bar, W_RECT + 12) - mi(bar, W_RECT + 4) != h)
                fn<DWORD(THISCALL *)(BYTE *, int)>(0x40f5d0)(bar, h);
        } else {
            const BYTE layout = mb(self, LB_LAYOUT);
            mb(self, LB_LAYOUT) = 0;
            mb(self, W_BORDER) = 0;
            bar = bp(vc(self, S_MAKE_BAR));
            m(self, LB_BAR) = pv(bar);
            fn<DWORD(THISCALL *)(BYTE *, int)>(0x40f5b0)(self, mi(self, W_RECT + 8) - mi(bar, W_RECT + 8) - mi(self, W_RECT) + mi(bar, W_RECT));
            vc(self, V_ADD_CHILD, bp(m(self, LB_BAR)), 0x13, 0, -1000, -1000, 0);
            mb(self, LB_LAYOUT) = layout;
        }
    }
    const DWORD most = most_scroll(self);
    fn<void(THISCALL *)(BYTE *, DWORD, float)>(0x413bb0)(bp(m(self, LB_BAR)), 0, (float)(long double)(unsigned long long)most);
    const DWORD rh = row_h(self);
    const DWORD h = (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4));
    BYTE *bar = bp(m(self, LB_BAR));
    m<float>(bar, 0x150) = (float)(long double)(unsigned long long)rh;
    m<float>(bar, 0x154) = (float)(long double)(unsigned long long)h;
}

// The own scroll bar deleted and made again
void THISCALL ListBox_RemakeBar(BYTE *self)   // 0x407630
{
    if (!mb(self, LB_BAR_FOREIGN) && m(self, LB_BAR)) {
        vc(bp(m(self, LB_BAR)), V_DELETE, 1);
        m(self, LB_BAR) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x4074f0)(self);
}

// The scroll bar needed (more than one row, not all shown, more than one shown): updated; else the own one
// deleted. Nothing for an empty box.
void THISCALL ListBox_CheckBar(BYTE *self)   // 0x407660
{
    if (mi(self, W_RECT + 12) == mi(self, W_RECT + 4) || mi(self, W_RECT + 8) == mi(self, W_RECT))
        return;
    const DWORD shown = m(self, LB_SHOWN), n = m(self, LB_COUNT);
    if (shown < n && n > 1 && shown > 1) {
        fn<void(THISCALL *)(BYTE *)>(0x4074f0)(self);
        return;
    }
    if (!mb(self, LB_BAR_FOREIGN) && m(self, LB_BAR)) {
        vc(bp(m(self, LB_BAR)), V_DELETE, 1);
        m(self, LB_BAR) = 0;
    }
}

// A row selected (the row, quiet): only a selectable one (the source's slot 6; an empty box selects -1); the
// target moved so the row shows (the height less the borders), the position with it when one row shows; slot 38
// with 1 on a change unless quiet
void THISCALL ListBox_Select(BYTE *self, DWORD r, DWORD quiet)   // 0x4076c0
{
    if (m(self, LB_COUNT) == 0 && r != 0)
        r = (DWORD)-1;
    if (!vc<BYTE>(source(self), 6, r))
        return;
    const DWORD old = m(self, LB_SEL);
    m(self, LB_SEL) = r;
    if (r != (DWORD)-1) {
        const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
        const DWORD h = (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4));
        const DWORD border = (fh / 5) * 2 + 2;
        const int avail = h < border ? 0 : (int)(h - border);
        const int rh = (int)row_h(self);
        int off = (int)(m(self, LB_SEL) * (DWORD)rh) - mi(self, LB_SCROLL);
        if (off < 0) {
            const int t = mi(self, LB_SCROLL) + off;
            const int most = (int)most_scroll(self);
            m(self, LB_TARGET) = (DWORD)(t < 0 ? 0 : t > most ? most : t);
        }
        if (avail) {
            const int d = off - avail + rh;
            if (d > 0) {
                const int t = mi(self, LB_SCROLL) + d;
                const int most = (int)most_scroll(self);
                m(self, LB_TARGET) = (DWORD)(t < 0 ? 0 : t > most ? most : t);
            }
        }
    }
    if (m(self, LB_SHOWN) <= 1) {
        m(self, LB_SCROLL) = m(self, LB_TARGET);
        if (m(self, LB_BAR))
            bar_pos(bp(m(self, LB_BAR)), m(self, LB_SCROLL));
    }
    if (old != r && !quiet)
        vc(self, V_INVALIDATE, 1);
}

// The rows that fit (and the height for them), the bar checked
void THISCALL ListBox_SetRows(BYTE *self, DWORD n)   // 0x407870
{
    m(self, LB_FIT) = n;
    m(self, LB_SHOWN) = n;
    if (n > m(self, LB_COUNT))
        m(self, LB_SHOWN) = m(self, LB_COUNT);
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const DWORD rh = row_h(self);
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40f5d0)(self, rh * n + fh / 5 + fh / 5 + 2);
    check_bar(self);
}

// Slot 84: the row height (the gap, slot 85, and the font height); slot 85: the gap; slot 86: the indent
// (4 * (font height / 5))
DWORD THISCALL ListBox_RowHeight(BYTE *self)   // 0x4078e0
{
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    return vc(self, S_GAP) + fh;
}
DWORD THISCALL ListBox_Gap(BYTE *self)   // 0x407900
{
    return m(self, LB_GAP);
}
DWORD THISCALL ListBox_Indent(BYTE *self)   // 0x407910
{
    return (m(bp(m(self, W_FONT)), 0x80) / 5) * 4;
}

// The header's column under a point (screen coordinates; out: the column's left) with more than one column; 0
// without, -1 right of the last
DWORD THISCALL ListBox_ColumnAt(BYTE *self, const int *p, int *col_x)   // 0x407930
{
    if (col_x)
        *col_x = 0;
    BYTE *header = bp(m(self, LB_HEADER));
    if (!header || m(header, 0x108) <= 1)
        return 0;
    BYTE *scroll = bp(m(self, W_SCROLL));
    const int sx = scroll ? mi(scroll, S_X) : 0;
    DWORD x = (DWORD)(p[0] - mi(self, W_SCREEN) + sx);
    DWORD i = 0;
    for (; i < m(bp(m(self, LB_HEADER)), 0x108); i++) {
        if (x <= column_width(bp(m(self, LB_HEADER)), i))
            break;
        x -= column_width(bp(m(self, LB_HEADER)), i);
        if (col_x)
            *col_x += (int)column_width(bp(m(self, LB_HEADER)), i);
    }
    if (i < m(bp(m(self, LB_HEADER)), 0x108))
        return i;
    if (col_x)
        *col_x = 0;
    return (DWORD)-1;
}

// A scroll bar given (the bar, someone else's): the old one deleted
void THISCALL ListBox_SetBar(BYTE *self, DWORD bar, BYTE foreign)   // 0x407a00
{
    if (m(self, LB_BAR))
        vc(bp(m(self, LB_BAR)), V_DELETE, 1);
    m(self, LB_BAR) = bar;
    mb(self, LB_BAR_FOREIGN) = foreign;
}

// The drop list's constructor (the parent, the combo box, the source): a modal list box of up to 14 rows (10 more
// rows than the items for a tree) on the root window, below the combo box or above it when it would leave the
// parent, focused
BYTE *THISCALL DropList_Ctor(BYTE *self, BYTE *parent, BYTE *owner, BYTE *src)   // 0x407a50
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *, DWORD, DWORD)>(0x404520)(self, parent, src, 0xf423f, 1);
    m(self, 0) = vt(0x5ead94);
    mb(self, W_TRANSPARENT) = 0;
    mb(self, W_MODAL) = 1;
    m(self, DL_OWNER) = pv(owner);
    DWORD n = m(self, LB_COUNT);
    if (mb(self, LB_TREE))
        n += 10;
    if (n >= 0xe)
        n = 0xe;
    m(self, LB_FIT) = n;
    m(self, LB_SHOWN) = n;
    if (n > m(self, LB_COUNT))
        m(self, LB_SHOWN) = m(self, LB_COUNT);
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const DWORD rh = at<DWORD(THISCALL *)(BYTE *)>(gd(glob_addr(0x5eaee4)))(self);
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40f5d0)(self, rh * n + fh / 5 + fh / 5 + 2);
    check_bar(self);
    BYTE *root = fn<BYTE *(THISCALL *)(BYTE *)>(0x40cc70)(bp(m(self, DL_OWNER)));
    vc(root, V_ADD_CHILD, self, 0, 0, -1000, -1000, 0);
    BYTE *o = bp(m(self, DL_OWNER));
    fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x40d630)(self, mi(o, W_SCREEN), mi(o, W_SCREEN + 12));
    if (mi(self, W_SCREEN + 12) > mi(bp(m(self, W_PARENT)), W_SCREEN + 12)) {
        o = bp(m(self, DL_OWNER));
        fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x40d630)(self, mi(o, W_SCREEN), mi(o, W_SCREEN + 4) - mi(self, W_RECT + 12) + mi(self, W_RECT + 4));
    }
    vc(bp(m(self, W_PARENT)), V_FOCUS, self);
    return self;
}

// The drop list's deleting destructor (the list box's, with the list box's vtable and 0x25c bytes)
BYTE *THISCALL DropList_DeletingDtor(BYTE *self, DWORD flags)   // 0x407b80
{
    m(self, 0) = vt(0x5eabf8);
    if (gd(glob_addr(G_SORT_BOX)) == pv(self))
        gd(glob_addr(G_SORT_BOX)) = 0;
    m(self, LB_ROWS) = vt(0x5ead88);
    if (m(self, LB_DATA)) {
        pool_get_free(bp(m(self, LB_DATA)), m(self, LB_CAP) * ROW);
        m(self, LB_DATA) = 0;
        m(self, LB_CAP) = 0;
    }
    m(self, LB_COUNT) = 0;
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_free(self, 0x25c);
    return self;
}

namespace {

// The drop list closed: deleted by its combo box's pointer
bool close_drop(BYTE *owner)
{
    BYTE *dl = bp(m(owner, CB_DROP));
    if (!dl)
        return false;
    vc(dl, V_DELETE, 1);
    m(owner, CB_DROP) = 0;
    return true;
}

}  // namespace

// The drop list's slot 38: to the combo box as a notification (slot 21) from it
DWORD THISCALL DropList_Changed(BYTE *self, DWORD code)   // 0x407c00
{
    BYTE *o = bp(m(self, DL_OWNER));
    if (!o)
        return 0;
    return vc(o, V_NOTIFY, self, code);
}

// The drop list's slot 19 (active): the base's; losing it closes the list
DWORD THISCALL DropList_SetActive(BYTE *self, DWORD on)   // 0x407c20
{
    DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c3e0)(self, on);
    if (!on) {
        BYTE *o = bp(m(self, DL_OWNER));
        BYTE *dl = bp(m(o, CB_DROP));
        if (dl) {
            r = vc(dl, V_DELETE, 1);
            m(o, CB_DROP) = 0;
        }
    }
    return r;
}

// The drop list's slot 10, a button: inside the list the base's (0 or 1); outside the list closed, the combo
// box's id selected again in its box, the combo box focused; 1
DWORD THISCALL DropList_MouseDown(BYTE *self, DWORD button, const int *p)   // 0x407c60
{
    if (inside_visible(self, p))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, p) ? 1 : 0;
    BYTE *o = bp(m(self, DL_OWNER));
    if (close_drop(o))
        select_id_found(bp(m(o, CB_LIST)), m(o, CB_SEL_ID), 0);
    vc(bp(m(o, W_PARENT)), V_FOCUS, o);
    return 1;
}

// The drop list's slot 11, a button released: the base's (0x40bde0); the left button released over the selected
// row (pressed on the list) expands or collapses it, or, for a leaf, closes the list (the combo box focused, its
// slot 38 with 0). The base's result.
DWORD THISCALL DropList_MouseUp(BYTE *self, DWORD button, const int *p)   // 0x407d90
{
    const BYTE pressed = mb(self, W_CAPTURE) & 1;
    const DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bde0)(self, button, p);
    if (button || !inside_visible(self, p) || !pressed || m(self, LB_SEL) == (DWORD)-1)
        return r;
    BYTE *rw = row(self, m(self, LB_SEL));
    if (mb(rw, R_KIDS)) {
        if (mb(rw, R_OPEN))
            collapse(self, m(self, LB_SEL));
        else
            expand(self, m(self, LB_SEL));
        return r;
    }
    BYTE *o = bp(m(self, DL_OWNER));
    close_drop(o);
    vc(bp(m(o, W_PARENT)), V_FOCUS, o);
    vc(o, V_INVALIDATE, 0);
    return r;
}

// The drop list's slot 16, a key: tab, and enter (0xd, 0x14) with a selection, close the list (the combo box
// focused, its slot 38 with 0); escape closes it with the combo box's id selected again; others the list box's.
DWORD THISCALL DropList_Key(BYTE *self, DWORD key)   // 0x407e80
{
    if (key == 9 || ((key == 0x14 || key == 0xd) && vc(self, S_SEL_ID) != (DWORD)-1)) {
        BYTE *o = bp(m(self, DL_OWNER));
        close_drop(o);
        vc(bp(m(o, W_PARENT)), V_FOCUS, o);
        vc(o, V_INVALIDATE, 0);
        return 1;
    }
    if (key == 0x1b) {
        BYTE *o = bp(m(self, DL_OWNER));
        BYTE *dl = bp(m(o, CB_DROP));
        if (dl) {
            vc(dl, V_DELETE, 1);
            const DWORD id = m(o, CB_SEL_ID);
            BYTE *box = bp(m(o, CB_LIST));
            m(o, CB_DROP) = 0;
            DWORD r = (DWORD)-1;
            if (id != (DWORD)-1) {
                r = row_of(box, id);
                if (r == (DWORD)-1) {
                    show_id(box, id);
                    r = find_row(box, id);
                }
            }
            select(box, r, 0);
        }
        vc(bp(m(o, W_PARENT)), V_FOCUS, o);
        return 1;
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x405ce0)(self, key);
}

// The drop list's slot 4: the background from the texture (slot 72, at the scroll position; unless transparent)
// and the list (slot 76) sunken
void THISCALL DropList_Draw(BYTE *self, BYTE *frame)   // 0x407fb0
{
    int dst[4] = {0, 0, mi(self, W_CONTENT_W), mi(self, W_CONTENT_H)};
    int src[4] = {0, mi(self, LB_SCROLL), mi(self, W_CONTENT_W), mi(self, W_CONTENT_H) + mi(self, LB_SCROLL)};
    if (!mb(self, W_TRANSPARENT)) {
        const DWORD texture = vc(self, V_TEXTURE);
        vc(frame, 23, texture, src, dst, glob_addr(0x6bc4e0));
    }
    vc(self, S_DRAW_LIST, frame, 1);
}

// The compiler's vector destructor iterator (the first element, the size, the count, the destructor): backwards
void __stdcall VectorDtorIterator(BYTE *p, DWORD size, int n, void(THISCALL *dtor)(BYTE *))   // 0x408e70
{
    BYTE *q = p + size * (DWORD)n;
    for (int i = n; i > 0; i--) {
        q -= size;
        dtor(q);
    }
}

// The rows' capacity at least n (pool blocks)
void THISCALL Rows_Reserve(BYTE *self, DWORD n)   // 0x408eb0
{
    const DWORD old = m(self, 0xc);
    if (old >= n)
        return;
    m(self, 0xc) = n;
    BYTE *p = static_cast<BYTE *>(pool_get_alloc(n * ROW));
    if (m(self, 8))
        crt_memmove(p, bp(m(self, 4)), m(self, 8) * ROW);
    if (m(self, 4)) {
        pool_get_free(bp(m(self, 4)), old * ROW);
        m(self, 4) = 0;
    }
    m(self, 4) = pv(p);
}

// ListBoxListSource (vtable 0x602590, 8 bytes: +4 a list), the source over a flat list (a named list's slots: 1 the
// text of an entry, 4 the text of none, 22 the count, 23 selectable): the root -1 (slot 0, 0x592380), every
// entry a child of it, the list itself as the file list (slot 5, 0x592370; both not hookable)
BYTE *THISCALL ListSource_Ctor(BYTE *self, DWORD list)   // 0x592350
{
    m(self, 0) = vt(0x602590);
    m(self, 4) = list;
    return self;
}
// Slot 1: the parent (the root, slot 0)
DWORD THISCALL ListSource_Parent(BYTE *self, DWORD)   // 0x592390
{
    return vc(self, 0);
}
// Slot 2: the text of an entry (-1: the list's slot 4)
DWORD THISCALL ListSource_Text(BYTE *self, DWORD id)   // 0x5923a0
{
    BYTE *list = bp(m(self, 4));
    if (id == (DWORD)-1)
        return vc(list, 4);
    return vc(list, 1, id);
}
// Slot 3: the child count (the list's count for the root, else 0)
DWORD THISCALL ListSource_Count(BYTE *self, DWORD id)   // 0x5923c0
{
    if (id == (DWORD)-1)
        return vc(bp(m(self, 4)), 22);
    return 0;
}
// Slot 4: a child (the root's k-th is entry k, -1 for others); ecx unused
DWORD THISCALL ListSource_Child(BYTE *, DWORD id, DWORD k)   // 0x5923e0
{
    return id == (DWORD)-1 ? k : (DWORD)-1;
}
// Slot 6: a row selectable (the list's slot 23)
DWORD THISCALL ListSource_Selectable(BYTE *self, DWORD r)   // 0x592400
{
    return vc(bp(m(self, 4)), 23, r);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_listbox)
{
    const Hook list[] = {
#include "ui_listbox_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
