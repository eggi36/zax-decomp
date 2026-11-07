// The combo boxes (vtables 0x5eaf18 and 0x5eb058, 0x118 bytes), the edit field (0x5ebd9c, 0x120 bytes) and the
// number field (0x5ebed4, 0x11c bytes) of the window toolkit (docs/DECOMPILATION.md 9, "List boxes, combo boxes,
// edit and number fields").
//
// Combo box layout [confirmed: the constructors 0x408040/0x408cd0, the init 0x408110 and the slots below]: the
// CJWindow part, then
//   +0x100 the closed list (a CJListBox of one row without frame and tree lines), +0x104 the drop list while it is
//   open (ui_listbox.cpp), +0x108 the button (a button with the arrow 0x1d), +0x10c the width ((font height / 5) *
//   the width in characters), +0x110 a click on the closed list opens it (the second class), +0x111 enter goes to
//   slot 38, +0x114 the id selected when the list opened (escape and a click outside select it again)
// Edit field layout [confirmed: the constructor 0x410b50 and the slots below]: the CJWindow part (the text is the
// title), then
//   +0x100 the caret, +0x104 the selection's anchor, +0x108 its end (the caret's index as well), +0x10c the
//   horizontal scroll in pixels, +0x110 digits only (and '.', '-'), +0x111 the maximum length (0 none),
//   +0x114/+0x118 the range a number must lie in (both 0: none; slot 76), +0x11c upper case, +0x11d copy and paste
// Number field layout [confirmed: the constructor 0x411b90 and the slots below]: the CJWindow part, then
//   +0x100 its edit field (digits only), +0x104/+0x108 the up and down buttons (0x1a, 0x1b), +0x10c/+0x110 the
//   range (both 0: none), +0x114 the step (1.0), +0x118 a callback object told about every change (slot 1)
// The caret is a line when 0x60a648 is set (key 16 toggles it), else a block over the character after it.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD CB_LIST = 0x100, CB_DROP = 0x104, CB_BUTTON = 0x108, CB_WIDTH = 0x10c, CB_CLICK_OPENS = 0x110, CB_ENTER = 0x111,
                CB_SEL_ID = 0x114;
constexpr DWORD ED_CARET = 0x100, ED_ANCHOR = 0x104, ED_END = 0x108, ED_SCROLL = 0x10c, ED_DIGITS = 0x110, ED_MAX = 0x111,
                ED_MIN_V = 0x114, ED_MAX_V = 0x118, ED_UPPER = 0x11c, ED_CLIPBOARD = 0x11d;
constexpr DWORD NF_EDIT = 0x100, NF_UP = 0x104, NF_DOWN = 0x108, NF_MIN = 0x10c, NF_MAX = 0x110, NF_STEP = 0x114, NF_CALLBACK = 0x118;
constexpr DWORD LB_SEL = 0x110, LB_COUNT = 0x108, LB_DATA = 0x104, LB_SCROLL = 0x114, LB_TARGET = 0x118, LB_SHOWN = 0x11c,
                LB_SOURCE = 0x128, LB_BAR = 0x248, LB_PLAIN = 0x230, LB_PLAIN2 = 0x231, LB_TEXT_W = 0x12c, LB_MAX_W = 0x22c,
                LB_OWN_SOURCE = 0x24c, LB_OWN_LIST = 0x250;
constexpr DWORD G_LINE_CARET = 0x60a648;
constexpr DWORD CLIPBOARD = 0x6bd31c;
constexpr int V_SET_TEXT_VALIDATE = 76, V_SEL_ID = 77, V_SEL_TEXT = 78, V_ROW_H = 84, V_COPY = 23, V_DELETE_SEL = 25, V_GET_FLOAT = 48,
              V_SET_FLOAT = 50, V_CLICK = 18;

// ZString through the exe
const char *zs_cstr(BYTE *z) { return fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(z); }
DWORD zs_len(BYTE *z) { return fn<DWORD(THISCALL *)(BYTE *)>(0x4e13b0)(z); }
char *zs_buffer(BYTE *z, DWORD n) { return fn<char *(THISCALL *)(BYTE *, DWORD)>(0x4e1590)(z, n); }
void zs_release(BYTE *z, int n) { fn<void(THISCALL *)(BYTE *, int)>(0x4e16d0)(z, n); }
DWORD slen(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
// strcpy as the original inlines it (the source's length first, then forward)
void scpy(char *d, const char *s)
{
    const DWORD n = slen(s) + 1;
    for (DWORD i = 0; i < n; i++)
        d[i] = s[i];
}
DWORD text_width(BYTE *self, const char *t, int n) { return fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, W_FONT)), t, n); }
DWORD h5(BYTE *w) { return m(bp(m(w, W_FONT)), 0x80) / 5; }
bool shift_down() { return key_down(0x2a) || key_down(0x36); }
DWORD palette_index(BYTE *colour)
{
    if (mb(colour, 0xc) & 1)
        return mb(colour, 0xa);
    return fn<BYTE(THISCALL *)(BYTE *)>(0x53f160)(colour);
}
// Rounding to float as the original's fstp dword does
float round_float(long double v)
{
    volatile float f = (float)v;
    return f;
}
// The number checks treat a range bound that compares equal to 0.0 (or unordered: NaN) as none
bool zero_or_nan(float x) { return !(x < 0.0f) && !(x > 0.0f); }

// The list box's functions (ui_listbox.cpp) through their addresses
void lb_select(BYTE *box, DWORD r, DWORD quiet) { fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4076c0)(box, r, quiet); }
DWORD lb_find(BYTE *box, DWORD id) { return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4067b0)(box, id); }
void lb_show(BYTE *box, DWORD id) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x406920)(box, id); }
void lb_expand(BYTE *box, DWORD r) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x4068b0)(box, r); }
BYTE *lb_row(BYTE *box, DWORD i) { return bp(m(box, LB_DATA)) + i * 0x14; }
DWORD lb_row_of(BYTE *box, DWORD id)
{
    for (DWORD i = 0; i < m(box, LB_COUNT); i++)
        if (m(lb_row(box, i), 0) == id)
            return i;
    return (DWORD)-1;
}
DWORD lb_most(BYTE *box)
{
    if (m(box, LB_SHOWN) >= m(box, LB_COUNT))
        return 0;
    return vc(box, V_ROW_H) * (m(box, LB_COUNT) - m(box, LB_SHOWN));
}

// The drop list made for the combo box over its list's source (0x407a50)
BYTE *open_drop(BYTE *self)
{
    m(self, CB_SEL_ID) = vc(bp(m(self, CB_LIST)), V_SEL_ID);
    BYTE *dl = static_cast<BYTE *>(pool_alloc(0x25c));
    if (dl)
        dl = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *, DWORD)>(0x407a50)(dl, self, self, m(bp(m(self, CB_LIST)), LB_SOURCE));
    m(self, CB_DROP) = pv(dl);
    return dl;
}
// The id selected in the drop list with its parents expanded (the parent's row through FindRow 0x4067b0)
void select_in_drop(BYTE *dl, DWORD id)
{
    if (id == (DWORD)-1) {
        lb_select(dl, (DWORD)-1, 1);
        return;
    }
    const DWORD r = lb_row_of(dl, id);
    if (r != (DWORD)-1) {
        lb_select(dl, r, 1);
        return;
    }
    BYTE *src = bp(m(dl, LB_SOURCE));
    if (id == vc(src, 0) || vc(src, 3, vc(src, 0))) {
        const DWORD parent = vc(bp(m(dl, LB_SOURCE)), 1, id);
        DWORD pr = lb_find(dl, parent);
        if (pr == (DWORD)-1) {
            lb_show(dl, parent);
            pr = lb_find(dl, parent);
        }
        lb_expand(dl, pr);
    }
    lb_select(dl, lb_row_of(dl, id), 1);
}
// The drop list's selection in the middle of its rows (as CenterSelection 0x407380, the target through Clamp)
void center_drop(BYTE *dl)
{
    const DWORD sel = m(dl, LB_SEL);
    if (sel == (DWORD)-1)
        return;
    const DWORD shown = m(dl, LB_SHOWN);
    int first = (int)(sel - (shown >> 1));
    if (first < 0)
        first = 0;
    if (shown + (DWORD)first > m(dl, LB_COUNT))
        first = (int)(m(dl, LB_COUNT) - shown);
    const int s = (int)vc(dl, V_ROW_H) * first;
    const int most = (int)lb_most(dl);
    m(dl, LB_TARGET) = fn<DWORD(__fastcall *)(int, int, int)>(0x402070)(s, 0, most);
    m(dl, LB_SCROLL) = (DWORD)s;
    BYTE *bar = bp(m(dl, LB_BAR));
    if (bar)
        fn<void(THISCALL *)(BYTE *, float, DWORD)>(0x413c20)(bar, (float)(long double)(unsigned long long)(DWORD)s, 1);
}
// The drop list opened by a key (0x4087c0): the id's row, else the id shown first
BYTE *open_drop_by_key(BYTE *self)
{
    BYTE *dl = open_drop(self);
    const DWORD id = m(self, CB_SEL_ID);
    DWORD r = lb_find(dl, id);
    if (r == (DWORD)-1 && id != r) {
        lb_show(dl, id);
        r = lb_find(dl, id);
    }
    lb_select(dl, r, 1);
    fn<void(THISCALL *)(BYTE *)>(0x407380)(bp(m(self, CB_DROP)));
    return bp(m(self, CB_DROP));
}
// The list box's selection of an id with its parents (0x404390's pattern)
void select_id_in(BYTE *box, DWORD id, DWORD quiet)
{
    if (id == (DWORD)-1) {
        lb_select(box, (DWORD)-1, quiet);
        return;
    }
    const DWORD r = lb_row_of(box, id);
    if (r != (DWORD)-1) {
        lb_select(box, r, quiet);
        return;
    }
    BYTE *src = bp(m(box, LB_SOURCE));
    if (id == vc(src, 0) || vc(src, 3, vc(src, 0))) {
        const DWORD parent = vc(bp(m(box, LB_SOURCE)), 1, id);
        DWORD pr = parent == (DWORD)-1 ? (DWORD)-1 : lb_row_of(box, parent);
        if (pr == (DWORD)-1) {
            lb_show(box, parent);
            pr = parent == (DWORD)-1 ? (DWORD)-1 : lb_row_of(box, parent);
        }
        lb_expand(box, pr);
    }
    lb_select(box, lb_row_of(box, id), quiet);
}

}  // namespace

// ---- The combo box --------------------------------------------------------------------------------------------

// The constructor (the parent, a list for the box's own source, the width in characters): a button with the arrow
// 0x1d, then the init (0x408110)
BYTE *THISCALL Combo_Ctor(BYTE *self, BYTE *parent, DWORD list, DWORD width)   // 0x408040
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, nullptr);
    m(self, 0) = vt(0x5eaf18);
    char arrow[2] = {0x1d, 0};
    BYTE *btn = static_cast<BYTE *>(pool_alloc(0x138));
    if (btn)
        btn = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(btn, self, arrow, 0);
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x408110)(self, btn, list, width);
    return self;
}

// Slot 44: the closed list's
DWORD THISCALL Combo_Slot44(BYTE *self)   // 0x4080a0
{
    return vc(bp(m(self, CB_LIST)), 44);
}

// The deleting destructor: the list and the drop list deleted
BYTE *THISCALL Combo_DeletingDtor(BYTE *self, DWORD flags)   // 0x4080b0
{
    m(self, 0) = vt(0x5eaf18);
    if (m(self, CB_LIST)) {
        vc(bp(m(self, CB_LIST)), V_DELETE, 1);
        m(self, CB_LIST) = 0;
    }
    if (m(self, CB_DROP)) {
        vc(bp(m(self, CB_DROP)), V_DELETE, 1);
        m(self, CB_DROP) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_free(self, 0x118);
    return self;
}

// The init (the button, a list, the width in characters): a tab stop, framed; the closed list (one list box of the
// width less the button, no frame, no tree lines) and the button right of it (0xd), the list as wide as its
// widest text (up to the limit) and focused
void THISCALL Combo_Init(BYTE *self, BYTE *btn, DWORD list, DWORD width)   // 0x408110
{
    m(self, CB_BUTTON) = 0;
    m(self, CB_LIST) = 0;
    mb(self, CB_ENTER) = 0;
    mb(self, CB_CLICK_OPENS) = 0;
    m(self, CB_WIDTH) = h5(self) * width;
    mb(self, W_BORDER) = 0;
    mb(self, W_TAB_STOP) = 1;
    mb(self, W_FRAMED) = 1;
    m(self, CB_BUTTON) = pv(btn);
    mb(btn, W_TAB_STOP) = 0;
    mb(bp(m(self, CB_BUTTON)), W_FRAMED) = 0;
    BYTE *lb = static_cast<BYTE *>(pool_alloc(0x258));
    if (lb) {
        BYTE *b = bp(m(self, CB_BUTTON));
        const DWORD w = m(self, CB_WIDTH) - (DWORD)mi(b, W_RECT + 8) + (DWORD)mi(b, W_RECT);
        lb = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x4041f0)(lb, self, list, w / h5(self), 1);
    }
    m(self, CB_LIST) = pv(lb);
    mb(lb, W_TAB_STOP) = 0;
    mb(bp(m(self, CB_LIST)), W_FRAMED) = 0;
    mb(bp(m(self, CB_LIST)), LB_PLAIN) = 1;
    mb(bp(m(self, CB_LIST)), LB_PLAIN2) = 1;
    vc(self, V_ADD_CHILD, bp(m(self, CB_LIST)), 0, 0, -1000, -1000, 0);
    m(self, CB_DROP) = 0;
    vc(self, V_ADD_CHILD, bp(m(self, CB_BUTTON)), 0xd, bp(m(self, CB_LIST)), -1, 0, 0);
    lb = bp(m(self, CB_LIST));
    const DWORD tw = m(lb, LB_TEXT_W) < m(lb, LB_MAX_W) ? m(lb, LB_TEXT_W) : m(lb, LB_MAX_W);
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40f5b0)(lb, tw + h5(lb) * 2 + 3);
    vc(bp(m(self, CB_BUTTON)), V_PLACE, 0xd, bp(m(self, CB_LIST)), -1, 0, 0);
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40ca40)(self, bp(m(self, CB_LIST)));
}

// Slot 79: the button (the arrow 0x1d on the parent)
BYTE *THISCALL Combo_MakeButton(BYTE *, BYTE *parent)   // 0x408290
{
    char arrow[2] = {0x1d, 0};
    BYTE *btn = static_cast<BYTE *>(pool_alloc(0x138));
    if (!btn)
        return nullptr;
    return fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(btn, parent, arrow, 0);
}

// Slot 57, the rectangle: the base's; the button kept at the right edge, the list as wide as the rest (more than
// 4) and one row high (slot 59)
void THISCALL Combo_SetRect(BYTE *self, const int *r)   // 0x4082d0
{
    fn<void(THISCALL *)(BYTE *, const int *)>(0x40d2a0)(self, r);
    BYTE *btn = bp(m(self, CB_BUTTON));
    if (!btn || !m(self, CB_LIST))
        return;
    const int x = mi(self, W_RECT + 8) - mi(btn, W_RECT + 8) - mi(self, W_RECT) + mi(btn, W_RECT);
    if (mi(btn, W_RECT) != x)
        fn<DWORD(THISCALL *)(BYTE *, int)>(0x40d680)(btn, x);
    btn = bp(m(self, CB_BUTTON));
    const int w = mi(self, W_RECT + 8) - mi(btn, W_RECT + 8) - mi(self, W_RECT) + mi(btn, W_RECT) + 1;
    if (w <= 4)
        return;
    BYTE *lb = bp(m(self, CB_LIST));
    if (mi(lb, W_RECT + 8) - mi(lb, W_RECT) == w)
        return;
    const DWORD fh = m(bp(m(lb, W_FONT)), 0x80);
    const DWORD rh = vc(lb, V_ROW_H);
    vc(bp(m(self, CB_LIST)), V_SET_SIZE, w, rh + fh / 5 + fh / 5 + 2);
}

// Slot 10, a button: the left button on the closed list of a combo box that opens on a click opens the drop list
// (the selected id selected and centred in it); 1. Else the base's (0x40bcc0).
DWORD THISCALL Combo_MouseDown(BYTE *self, DWORD button, const int *p)   // 0x408390
{
    BYTE *lb = bp(m(self, CB_LIST));
    if (!mb(self, CB_CLICK_OPENS) || button || !(p[0] >= mi(lb, W_VISIBLE) && p[1] >= mi(lb, W_VISIBLE + 4) && p[0] < mi(lb, W_VISIBLE + 8) &&
                                                p[1] < mi(lb, W_VISIBLE + 12)))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, p);
    if (m(self, CB_DROP))
        return 1;
    BYTE *dl = open_drop(self);
    select_in_drop(dl, m(self, CB_SEL_ID));
    center_drop(bp(m(self, CB_DROP)));
    return 1;
}

// Slot 11, a button released: the base's (0x40bde0); the button clicked (its slot 80) opens the drop list as a
// click does. The base's result.
DWORD THISCALL Combo_MouseUp(BYTE *self, DWORD button, const int *p)   // 0x4085b0
{
    const DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bde0)(self, button, p);
    if (!vc(bp(m(self, CB_BUTTON)), 80) || m(self, CB_DROP))
        return r;
    BYTE *dl = open_drop(self);
    select_in_drop(dl, m(self, CB_SEL_ID));
    center_drop(bp(m(self, CB_DROP)));
    return r;
}

// Slot 16, a key: with the drop list open enter closes it (the list focused, slot 34); 0x14 opens it, 6 and 10 open
// it and move in it (its slot 16); enter goes to slot 38 with 0 when +0x111 is set; others the base's (0x40c170)
DWORD THISCALL Combo_Key(BYTE *self, DWORD key)   // 0x4087c0
{
    if (m(self, CB_DROP)) {
        if (key != 0xd)
            return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
        vc(bp(m(self, CB_DROP)), V_DELETE, 1);
        m(self, CB_DROP) = 0;
        vc(self, V_FOCUS, bp(m(self, CB_LIST)));
        return 1;
    }
    if (key == 0x14) {
        open_drop_by_key(self);
        return 1;
    }
    if (key == 6 || key == 0xa) {
        BYTE *dl = open_drop_by_key(self);
        (void)dl;
        return vc(bp(m(self, CB_DROP)), V_KEY, key);
    }
    if (key == 0xd && mb(self, CB_ENTER)) {
        vc(self, V_INVALIDATE, 0);
        return 1;
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
}

// Slot 21, a notification (from, code): from the closed list to slot 38; from the drop list its selected id
// selected in the closed list
void THISCALL Combo_Notify(BYTE *self, BYTE *from, DWORD code)   // 0x408a10
{
    if (from == bp(m(self, CB_LIST)))
        vc(self, V_INVALIDATE, code);
    BYTE *dl = bp(m(self, CB_DROP));
    if (!dl || from != dl)
        return;
    const DWORD id = vc(dl, V_SEL_ID);
    select_id_in(bp(m(self, CB_LIST)), id, 0);
}

// The list of the boxes' own sources (the closed list's and the open drop list's)
void THISCALL Combo_SetList(BYTE *self, DWORD list)   // 0x408b50
{
    BYTE *lb = bp(m(self, CB_LIST));
    m(lb, LB_OWN_LIST) = list;
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x406fc0)(lb, lb + LB_OWN_SOURCE, 0);
    BYTE *dl = bp(m(self, CB_DROP));
    if (dl) {
        m(dl, LB_OWN_LIST) = list;
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x406fc0)(dl, dl + LB_OWN_SOURCE, 0);
    }
}

// Slots 76 and 77: the closed list's selected id and text (its slots 77, 78)
DWORD THISCALL Combo_SelectedId(BYTE *self)   // 0x408ba0
{
    return vc(bp(m(self, CB_LIST)), V_SEL_ID);
}
DWORD THISCALL Combo_SelectedText(BYTE *self)   // 0x408bb0
{
    return vc(bp(m(self, CB_LIST)), V_SEL_TEXT);
}

// An id selected in the closed list (the id, quiet)
void THISCALL Combo_SelectId(BYTE *self, DWORD id, DWORD quiet)   // 0x408bc0
{
    select_id_in(bp(m(self, CB_LIST)), id, quiet);
}

// The second combo box's constructor: a click on the closed list opens it, the closed list with a frame
BYTE *THISCALL Combo2_Ctor(BYTE *self, BYTE *parent, DWORD list, DWORD width)   // 0x408cd0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, nullptr);
    m(self, 0) = vt(0x5eaf18);
    char arrow[2] = {0x1d, 0};
    BYTE *btn = static_cast<BYTE *>(pool_alloc(0x138));
    if (btn)
        btn = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(btn, self, arrow, 0);
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x408110)(self, btn, list, width);
    m(self, 0) = vt(0x5eb058);
    mb(self, CB_CLICK_OPENS) = 1;
    mb(bp(m(self, CB_LIST)), LB_PLAIN) = 0;
    return self;
}

// ---- The edit field -------------------------------------------------------------------------------------------

namespace {

// The character index under x (window coordinates less the scroll): the last n whose text width less 2 lies left
// of it, 0 outside the field
DWORD index_at(BYTE *self, int px)
{
    DWORD x = (DWORD)(px - mi(self, W_SCREEN));
    if ((int)x < 0 || (int)x >= mi(self, W_RECT + 8) - mi(self, W_RECT))
        return 0;
    x += m(self, ED_SCROLL);
    for (DWORD n = zs_len(self + W_TITLE); n; n--)
        if (text_width(self, zs_cstr(self + W_TITLE), (int)n) - 2 < x)
            return n;
    return 0;
}
// The caret moved (with the anchor unless shift is down)
DWORD move_caret(BYTE *self, DWORD to)
{
    const bool shift = shift_down();
    m(self, ED_CARET) = to;
    m(self, ED_END) = to;
    if (!shift)
        m(self, ED_ANCHOR) = to;
    return 1;
}

}  // namespace

// The constructor (the parent, the text): a framed tab stop with copy and paste; without a width as wide as the
// text and "XXXX" (or "XXXXXXXXXXXX") and as high as the font's thumb, the caret at the end
BYTE *THISCALL Edit_Ctor(BYTE *self, BYTE *parent, const char *text)   // 0x410b50
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, text);
    m(self, 0) = vt(0x5ebd9c);
    mb(self, ED_CLIPBOARD) = 1;
    mb(self, W_TAB_STOP) = 1;
    mb(self, W_FRAMED) = 1;
    DWORD w = (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT));
    m(self, ED_SCROLL) = 0;
    m(self, ED_ANCHOR) = 0;
    m(self, ED_END) = 0;
    mb(self, ED_MAX) = 0;
    m(self, ED_MIN_V) = 0;
    m(self, ED_MAX_V) = 0;
    mb(self, W_ACCEL_ON) = 0;
    if (!w) {
        if (text && *text) {
            const DWORD tw = text_width(self, text, -1);
            const int b = (signed char)mb(self, W_BORDER3);
            w = tw + (DWORD)b + 1 + text_width(self, glob<const char *>(0x60a678), -1);
        } else {
            w = text_width(self, glob<const char *>(0x60a65c), -1);
        }
    }
    const DWORD thumb = fn<DWORD(THISCALL *)(BYTE *)>(0x4a6140)(bp(m(self, W_FONT)));
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, w, thumb + 2 * (DWORD)(int)(signed char)mb(self, W_BORDER3) + 2);
    const DWORD len = slen(zs_cstr(self + W_TITLE));
    m(self, ED_ANCHOR) = 0;
    m(self, ED_CARET) = len;
    m(self, ED_END) = len;
    mb(self, ED_DIGITS) = 0;
    mb(self, ED_UPPER) = 0;
    return self;
}

// The deleting destructor (0x120 bytes)
BYTE *THISCALL Edit_DeletingDtor(BYTE *self, DWORD flags)   // 0x410c50
{
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_free(self, 0x120);
    return self;
}

// Slot 4: the sunken bevel in the static rect 0x7161a0; in the visible client area (0x40ef90; nothing more, the
// opacities not put back, when none) the selection (colour +0xc0), the text, and when active the caret (scrolled
// into view; blinking with bit 8 of 0x4d2460: a line, or a block over the next character, colour +0xa0); the
// children (0x40b620). Disabled at reduced opacities.
void THISCALL Edit_Draw(BYTE *self, BYTE *frame)   // 0x410c80
{
    const bool enabled = fn<bool(THISCALL *)(BYTE *)>(0x40c630)(self);
    if (!enabled) {
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6bcc68), 0.4f);
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6b9728), 0.6f);
    }
    const DWORD cb0 = palette_index(self + 0xb0) & 0xff;
    const DWORD ca0 = palette_index(self + 0xa0) & 0xff;
    if (!(gb(glob_addr(0x716190)) & 1)) {
        gb(glob_addr(0x716190)) |= 1;
        fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x401270));
    }
    int *stat = glob<int *>(0x7161a0);
    stat[0] = 0, stat[1] = 0;
    stat[3] = mi(self, W_CONTENT_H);
    stat[2] = mi(self, W_CONTENT_W);
    fn<DWORD(__fastcall *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(bp(m(self, W_FONT)), frame, stat, ca0, cb0, 0, 0xf, 0);
    int r[4];
    if (!(fn<DWORD(THISCALL *)(BYTE *, int, int, int *, BYTE *)>(0x40ef90)(self, 1, 1, r, frame) & 0xff))
        return;
    BYTE *title = self + W_TITLE;
    using Fill = void(THISCALL *)(BYTE *, int, int, int, int, BYTE *);
    if (enabled && mb(self, W_ACTIVE) && m(self, ED_ANCHOR) != m(self, ED_END)) {
        const DWORD a = m(self, ED_ANCHOR), e = m(self, ED_END);
        const int w_lo = (int)text_width(self, zs_cstr(title), (int)(a < e ? a : e)) - mi(self, ED_SCROLL);
        const int w_hi = (int)text_width(self, zs_cstr(title), (int)(a > e ? a : e)) - mi(self, ED_SCROLL);
        fn<Fill>(0x4b3a70)(frame, w_lo + r[0], r[1], w_hi + r[0], r[3], self + 0xc0);
    }
    fn<void(THISCALL *)(BYTE *, BYTE *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(
        bp(m(self, W_FONT)), frame, r[0] - mi(self, ED_SCROLL), (int)(signed char)mb(self, W_BORDER3) + r[1], zs_cstr(title), 0, 0);
    const int caret_x = (int)text_width(self, zs_cstr(title), (int)m(self, ED_CARET));
    if (mb(self, W_ACTIVE) && enabled) {
        int tw = (int)text_width(self, zs_cstr(title), -1);
        const int w = r[2] - r[0];
        int right = caret_x + 1 <= tw ? tw : caret_x + 1;
        if (mi(self, ED_SCROLL) && right - mi(self, ED_SCROLL) < w)
            mi(self, ED_SCROLL) = right - w;
        if (caret_x < mi(self, ED_SCROLL))
            mi(self, ED_SCROLL) = caret_x;
        if (caret_x + 1 - mi(self, ED_SCROLL) >= w)
            mi(self, ED_SCROLL) = caret_x + 1 - w + 1;
        if (fn<DWORD(__cdecl *)()>(0x4d2460)() & 0x100) {
            if (!gb(glob_addr(G_LINE_CARET)) && m(self, ED_CARET) != zs_len(title)) {
                const char c = fn<char(THISCALL *)(BYTE *, DWORD)>(0x4e1aa0)(title, m(self, ED_CARET));
                const int cw = (int)fn<DWORD(THISCALL *)(BYTE *, int)>(0x4a6290)(bp(m(self, W_FONT)), (int)c);
                const int end = cw + caret_x;
                if (end > tw)
                    tw = end;
                if (mi(self, ED_SCROLL) && tw - mi(self, ED_SCROLL) < w)
                    mi(self, ED_SCROLL) = tw - w;
                if (caret_x < mi(self, ED_SCROLL))
                    mi(self, ED_SCROLL) = caret_x;
                if (end - mi(self, ED_SCROLL) >= w)
                    mi(self, ED_SCROLL) = end - w + 1;
                const int s = mi(self, ED_SCROLL);
                fn<Fill>(0x4b3a70)(frame, caret_x - s + r[0], r[1], cw - s + caret_x + r[0], r[3], self + 0xa0);
            } else {
                const int x = caret_x - mi(self, ED_SCROLL) + r[0];
                vc(frame, 51, x, r[1], x, r[3], self + 0xa0);
            }
        }
    }
    if (!enabled) {
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6bcc68), 1.0f);
        fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(0x6b9728), 1.0f);
    }
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
}

// Slot 10, a button: the base's; the left button puts the caret under the point (the anchor too unless shift is
// down); 1, 0 for other buttons
DWORD THISCALL Edit_MouseDown(BYTE *self, DWORD button, const int *p)   // 0x411020
{
    fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, p);
    if (button)
        return 0;
    const DWORD at_x = index_at(self, p[0]);
    return move_caret(self, at_x);
}

// Slot 14, a drag: with the left button the caret (not the anchor) under the point; al 1. Else the base's.
DWORD THISCALL Edit_MouseDrag(BYTE *self, const int *p, const int *delta, DWORD buttons, DWORD *cursor)   // 0x4110e0
{
    if (!(buttons & 1))
        return fn<DWORD(THISCALL *)(BYTE *, const int *, const int *, DWORD, DWORD *)>(0x40c0a0)(self, p, delta, buttons, cursor);
    const DWORD n = index_at(self, p[0]);
    m(self, ED_CARET) = n;
    m(self, ED_END) = n;
    return 1;
}

// Slot 16, a key (characters too): a character from 0x20 (upper case with +0x11c; digits, '.' and '-' only with
// +0x110) replaces the selection, overwrites the character at the caret (without the line caret) or is inserted
// (up to the maximum length, else the error sound); the text then checked (slot 76: the old text back through
// slot 43) or slot 38 with 1. Ctrl-A (1) selects all, 16 toggles the line caret (without shift and ctrl), back (8)
// deletes the selection or the character before the caret (slot 25), 11/12/5/14 move the caret (shift keeps the
// anchor). Others are not taken.
DWORD THISCALL Edit_Key(BYTE *self, DWORD key)   // 0x411170
{
    char old[0x100];
    BYTE *title = self + W_TITLE;
    old[0xff] = 0;
    fn<char *(__cdecl *)(char *, const char *, DWORD)>(0x5d0b50)(old, zs_cstr(title), 0xff);
    const DWORD len = slen(zs_cstr(title));
    if (key >= 0x20) {
        if (mb(self, ED_UPPER) && fn<int(__cdecl *)(int)>(0x5d0f51)((int)key))
            key = (DWORD)fn<int(__cdecl *)(int)>(0x5d0c4e)((int)key);
        if (mb(self, ED_DIGITS) && key != '.' && key != '-' && (key < '0' || key > '9'))
            return 0;
        const DWORD a = m(self, ED_ANCHOR), e = m(self, ED_END);
        if (a != e) {
            const DWORD lo = a < e ? a : e;
            const int d = (int)a - (int)e;
            const DWORD n = (DWORD)(d < 0 ? -d : d);
            zs_buffer(title, 0)[lo] = (char)key;
            char *src = zs_buffer(title, 0) + n + lo;
            char *dst = zs_buffer(title, 0) + lo + 1;
            scpy(dst, src);
            zs_release(title, -1);
            m(self, ED_CARET) = lo + 1;
            m(self, ED_END) = lo + 1;
            m(self, ED_ANCHOR) = lo + 1;
        } else if (!gb(glob_addr(G_LINE_CARET)) && m(self, ED_CARET) != len) {
            zs_buffer(title, 0)[m(self, ED_CARET)] = (char)key;
            const DWORD c = m(self, ED_CARET) + 1;
            m(self, ED_CARET) = c;
            m(self, ED_END) = c;
            m(self, ED_ANCHOR) = c;
            zs_release(title, -1);
        } else if (mb(self, ED_MAX) && len >= mb(self, ED_MAX)) {
            play_sound(glob_addr(SOUND_ERROR));
        } else {
            BYTE tmp[4];
            fn<void(THISCALL *)(BYTE *)>(0x4dec90)(tmp);
            fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(tmp, zs_cstr(title));
            char *buf = zs_buffer(tmp, zs_len(title) + 2);
            buf[m(self, ED_CARET)] = (char)key;
            const DWORD caret = m(self, ED_CARET);
            scpy(buf + caret + 1, zs_cstr(title) + caret);
            zs_release(tmp, -1);
            fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, zs_cstr(tmp));
            const DWORD c = m(self, ED_CARET) + 1;
            m(self, ED_CARET) = c;
            m(self, ED_END) = c;
            m(self, ED_ANCHOR) = c;
            fn<void(THISCALL *)(BYTE *)>(0x4defd0)(tmp);
        }
        if (vc<BYTE>(self, V_SET_TEXT_VALIDATE, zs_cstr(title)))
            vc(self, V_INVALIDATE, 1);
        else
            vc(self, V_SET_TITLE, old);
        return 1;
    }
    switch (key) {
    case 1:
        m(self, ED_CARET) = len;
        m(self, ED_END) = len;
        m(self, ED_ANCHOR) = 0;
        return 1;
    case 16:
        if (key_down(0x2a) || key_down(0x36) || key_down(0x1d) || key_down(0x11d))
            return 0;
        gb(glob_addr(G_LINE_CARET)) = gb(glob_addr(G_LINE_CARET)) == 0;
        return 1;
    case 8: {
        const DWORD c = m(self, ED_CARET);
        if (c == 0)
            return 1;
        if (m(self, ED_ANCHOR) == m(self, ED_END))
            move_caret(self, c - 1);
        vc(self, V_DELETE_SEL, 1);
        if (vc<BYTE>(self, V_SET_TEXT_VALIDATE, zs_cstr(title)))
            vc(self, V_INVALIDATE, 1);
        else
            vc(self, V_SET_TITLE, old);
        return 1;
    }
    case 11: {
        const DWORD c = m(self, ED_CARET);
        return move_caret(self, c > 0 ? c - 1 : c);
    }
    case 12: {
        const DWORD c = m(self, ED_CARET);
        return move_caret(self, c < len ? c + 1 : c);
    }
    case 5:
        return move_caret(self, 0);
    case 14:
        return move_caret(self, len);
    default:
        return 0;
    }
}

// Slot 23, copy (1: to the clipboard, 0x46d700): with copy and paste and a selection; al
DWORD THISCALL Edit_Copy(BYTE *self, DWORD doit)   // 0x411820
{
    const DWORD a = m(self, ED_ANCHOR), e = m(self, ED_END);
    if (!mb(self, ED_CLIPBOARD) || a == e)
        return 0;
    if (doit == 1) {
        const DWORD lo = a < e ? a : e;
        const int d = (int)a - (int)e;
        const DWORD n = (DWORD)(d < 0 ? -d : d);
        fn<void(THISCALL *)(DWORD, const char *, DWORD)>(0x46d700)(glob_addr(CLIPBOARD), zs_cstr(self + W_TITLE) + lo, n);
    }
    return 1;
}

// Slot 25, delete (1: do it): the selection, or the character after the caret; slot 38 with 1. al 1.
DWORD THISCALL Edit_DeleteSelection(BYTE *self, DWORD doit)   // 0x411880
{
    BYTE *title = self + W_TITLE;
    const DWORD a = m(self, ED_ANCHOR), e = m(self, ED_END);
    if (doit != 1)
        return 1;
    if (a != e) {
        const DWORD lo = a < e ? a : e;
        const int d = (int)a - (int)e;
        const DWORD n = (DWORD)(d < 0 ? -d : d);
        const char *src = zs_cstr(title) + n + lo;
        char *dst = zs_buffer(title, 0) + lo;
        scpy(dst, src);
        zs_release(title, -1);
        m(self, ED_CARET) = lo;
        m(self, ED_END) = lo;
        m(self, ED_ANCHOR) = lo;
    } else {
        if (m(self, ED_CARET) >= slen(zs_cstr(title)))
            return 1;
        const char *src = zs_cstr(title) + m(self, ED_CARET) + 1;
        char *dst = zs_buffer(title, 0) + m(self, ED_CARET);
        scpy(dst, src);
        zs_release(title, -1);
    }
    vc(self, V_INVALIDATE, 1);
    return 1;
}

// Slot 22, cut: copy (slot 23), then delete (slot 25) when asked; al
DWORD THISCALL Edit_Cut(BYTE *self, DWORD doit)   // 0x4119a0
{
    if (!mb(self, ED_CLIPBOARD) || !vc<BYTE>(self, V_COPY, doit))
        return 0;
    if (doit == 1)
        vc(self, V_DELETE_SEL, doit);
    return 1;
}

// Slot 24, paste: the clipboard's text (0x46d760) typed (slot 16) over the deleted selection; al
DWORD THISCALL Edit_Paste(BYTE *self, DWORD doit)   // 0x4119e0
{
    if (!mb(self, ED_CLIPBOARD))
        return 0;
    const char *t = fn<const char *(THISCALL *)(DWORD)>(0x46d760)(glob_addr(CLIPBOARD));
    if (!t)
        return 0;
    if (doit == 1) {
        vc(self, V_DELETE_SEL, 1);
        for (; *t; t++)
            vc(self, V_KEY, (int)*t);
    }
    return 1;
}

// Slot 19, active: becoming active selects all; the base's
DWORD THISCALL Edit_SetActive(BYTE *self, DWORD on)   // 0x411a40
{
    if (on) {
        const DWORD n = zs_len(self + W_TITLE);
        m(self, ED_CARET) = n;
        m(self, ED_END) = n;
        m(self, ED_ANCHOR) = 0;
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c3e0)(self, on);
}

// Slot 43, the text (null: empty): no scroll, all selected; the length
DWORD THISCALL Edit_SetTitle(BYTE *self, const char *text)   // 0x411a80
{
    m(self, ED_SCROLL) = 0;
    if (!text)
        text = glob<const char *>(0x62e558);
    fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, text);
    const DWORD n = zs_len(self + W_TITLE);
    m(self, ED_CARET) = n;
    m(self, ED_END) = n;
    m(self, ED_ANCHOR) = 0;
    return n;
}

// Copy and paste allowed
DWORD THISCALL Edit_SetClipboard(BYTE *self, DWORD on)   // 0x411ad0
{
    mb(self, ED_CLIPBOARD) = (BYTE)on;
    return on;
}

// Slot 69, the cursor: the text cursor (entry 1) when enabled, else the base's
DWORD THISCALL Edit_Cursor(BYTE *self, const int *p)   // 0x411ae0
{
    if (fn<bool(THISCALL *)(BYTE *)>(0x40c630)(self))
        return CursorEntry(1);
    return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x40ea00)(self, p);
}

// Slot 76, a text checked: with a range (+0x114/+0x118) a number (atof) outside it gives the error sound and
// false; empty texts pass; al
DWORD THISCALL Edit_Validate(BYTE *self, const char *text)   // 0x411b10
{
    const float lo = m<float>(self, ED_MIN_V), hi = m<float>(self, ED_MAX_V);
    if (zero_or_nan(lo) && zero_or_nan(hi))
        return 1;
    if (!*text)
        return 1;
    const long double v = fn<long double(__cdecl *)(const char *)>(0x5d03de)(text);
    if (x87_lt(v, lo) || v > (long double)hi) {
        play_sound(glob_addr(SOUND_ERROR));
        return 0;
    }
    return 1;
}

// ---- The number field -----------------------------------------------------------------------------------------

namespace {

// A value kept in the range (the error sound when it was outside)
float clamp_value(BYTE *self, float v)
{
    const float lo = m<float>(self, NF_MIN), hi = m<float>(self, NF_MAX);
    if ((!zero_or_nan(lo) || !zero_or_nan(hi)) && x87_lt(v, lo)) {
        play_sound(glob_addr(SOUND_ERROR));
        v = m<float>(self, NF_MIN);
    }
    if ((!zero_or_nan(m<float>(self, NF_MIN)) || !zero_or_nan(m<float>(self, NF_MAX))) && v > m<float>(self, NF_MAX)) {
        play_sound(glob_addr(SOUND_ERROR));
        v = m<float>(self, NF_MAX);
    }
    return v;
}
// The edit field's value moved by a float step, kept in range, set; the callback told
DWORD step_value(BYTE *self, float step)
{
    const long double cur = vc<long double>(bp(m(self, NF_EDIT)), V_GET_FLOAT);
    float v = round_float(cur + (long double)step);
    v = clamp_value(self, v);
    vc(bp(m(self, NF_EDIT)), V_SET_FLOAT, v);
    if (m(self, NF_CALLBACK))
        vc(bp(m(self, NF_CALLBACK)), 1);
    return vc(self, V_INVALIDATE, 1);
}
BYTE *new_button(BYTE *self, char c, int w, int h)
{
    BYTE *b = static_cast<BYTE *>(pool_alloc(0x138));
    if (b) {
        fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(b, self, glob<const char *>(0x62e558));
        fn<void(THISCALL *)(BYTE *)>(0x4dec90)(b + 0x11c);
        m(b, 0) = vt(0x5eb85c);
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x40faa0)(b, 0);
    }
    (void)w, (void)h;
    fn<DWORD(THISCALL *)(BYTE *, char)>(0x40cee0)(b, c);
    return b;
}

}  // namespace

// The constructor (the parent, the title, a text as wide as the field): a framed tab stop; a digits-only edit field
// as wide as the text plus 4, the up and down buttons (0x1a, 0x1b; repeating, 3/4 of the height wide and half as
// high) right of it, top and bottom; the range none, the step 1, the edit field focused
BYTE *THISCALL Number_Ctor(BYTE *self, BYTE *parent, const char *title, const char *width_text)   // 0x411b90
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, title);
    m(self, 0) = vt(0x5ebed4);
    m(self, NF_CALLBACK) = 0;
    mb(self, W_BORDER) = 0;
    mb(self, W_TAB_STOP) = 1;
    mb(self, W_FRAMED) = 1;
    m<float>(self, NF_STEP) = 1.0f;
    BYTE *ed = static_cast<BYTE *>(pool_alloc(0x120));
    if (ed)
        ed = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x410b50)(ed, self, glob<const char *>(0x62e558));
    m(self, NF_EDIT) = pv(ed);
    mb(ed, W_TAB_STOP) = 0;
    mb(bp(m(self, NF_EDIT)), W_FRAMED) = 0;
    mb(bp(m(self, NF_EDIT)), ED_DIGITS) = 1;
    const DWORD tw = text_width(self, width_text, -1);
    fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40f5b0)(bp(m(self, NF_EDIT)), tw + 4);
    using AddChild = DWORD(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD);
    fn<AddChild>(0x40c7c0)(self, bp(m(self, NF_EDIT)), 0, 0, (DWORD)-1000, (DWORD)-1000, 0);
    const DWORD h = (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4) + 2) >> 1;
    const DWORD w = (DWORD)(mi(self, W_RECT + 12) * 3 - mi(self, W_RECT + 4) * 3) >> 2;
    BYTE *up = new_button(self, 0x1a, (int)w, (int)h);
    m(self, NF_UP) = pv(up);
    mb(bp(m(self, NF_UP)), W_TAB_STOP) = 0;
    mb(bp(m(self, NF_UP)), W_FRAMED) = 0;
    vc(bp(m(self, NF_UP)), V_SET_SIZE, w, h);
    mb(bp(m(self, NF_UP)), 0x124) = 1;
    fn<AddChild>(0x40c7c0)(self, bp(m(self, NF_UP)), 0xe, 0, 0, 0, 0);
    BYTE *down = new_button(self, 0x1b, (int)w, (int)h);
    m(self, NF_DOWN) = pv(down);
    mb(bp(m(self, NF_DOWN)), W_TAB_STOP) = 0;
    mb(bp(m(self, NF_DOWN)), W_FRAMED) = 0;
    vc(bp(m(self, NF_DOWN)), V_SET_SIZE, w, h);
    mb(bp(m(self, NF_DOWN)), 0x124) = 1;
    fn<AddChild>(0x40c7c0)(self, bp(m(self, NF_DOWN)), 8, 0, 0, (DWORD)-1000, 0);
    m(self, NF_MIN) = 0;
    m(self, NF_MAX) = 0;
    m<float>(self, NF_STEP) = 1.0f;
    m(bp(m(self, NF_EDIT)), ED_MIN_V) = 0;
    m(bp(m(self, NF_EDIT)), ED_MAX_V) = 0;
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40ca40)(self, bp(m(self, NF_EDIT)));
    return self;
}

// Slot 43 of the number field and the combo boxes: the child's at +0x100
DWORD THISCALL Number_SetTitle(BYTE *self, const char *text)   // 0x411dd0
{
    return vc(bp(m(self, NF_EDIT)), V_SET_TITLE, text);
}

// The number field's deleting destructor: the edit field and the callback deleted
BYTE *THISCALL Number_DeletingDtor(BYTE *self, DWORD flags)   // 0x411df0
{
    m(self, 0) = vt(0x5ebed4);
    if (m(self, NF_EDIT)) {
        vc(bp(m(self, NF_EDIT)), V_DELETE, 1);
        m(self, NF_EDIT) = 0;
    }
    if (m(self, NF_CALLBACK)) {
        vc(bp(m(self, NF_CALLBACK)), 0, 1);
        m(self, NF_CALLBACK) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_free(self, 0x11c);
    return self;
}

// Slot 11: the base's
DWORD THISCALL Number_MouseUp(BYTE *self, DWORD button, const int *p)   // 0x411e50
{
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bde0)(self, button, p);
}

// Slot 15, the wheel: up or down (the buttons' slot 18); al 1
DWORD THISCALL Number_Wheel(BYTE *self, int delta, DWORD)   // 0x411e70
{
    vc(bp(m(self, delta > 0 ? NF_UP : NF_DOWN)), V_CLICK);
    return 1;
}

// Slot 14, a drag: the base's first (al 1 when taken); with the right button the size cursor (10) and the value
// moved by the step per pixel up (kept in range), the callback told, slot 38 with 1; al 1. Else al 0.
DWORD THISCALL Number_MouseDrag(BYTE *self, const int *p, const int *delta, DWORD buttons, DWORD *cursor)   // 0x411ea0
{
    if (fn<DWORD(THISCALL *)(BYTE *, const int *, const int *, DWORD, DWORD *)>(0x40c0a0)(self, p, delta, buttons, cursor) & 0xff)
        return 1;
    if (!(buttons & 2))
        return 0;
    *cursor = CursorEntry(10);
    const int dy = delta[1];
    if (!dy)
        return 1;
    const float t = round_float((long double)(-dy) * (long double)m<float>(self, NF_STEP));
    step_value(self, t);
    return 1;
}

// Slot 16, a key: 6 up, 10 down (the buttons' slot 18); others the base's
DWORD THISCALL Number_Key(BYTE *self, DWORD key)   // 0x412000
{
    if (key == 6) {
        vc(bp(m(self, NF_UP)), V_CLICK);
        return 1;
    }
    if (key == 10) {
        vc(bp(m(self, NF_DOWN)), V_CLICK);
        return 1;
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
}

// Slot 34, the focus: always the edit field
DWORD THISCALL Number_Focus(BYTE *self, BYTE *)   // 0x412040
{
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x40ca40)(self, bp(m(self, NF_EDIT)));
}

// Slot 21, a notification (from, code): from the edit field to slot 38 (code 1: the callback told first); from the
// buttons the value a step up or down (kept in range), the callback told, slot 38 with 1
DWORD THISCALL Number_Notify(BYTE *self, BYTE *from, DWORD code)   // 0x412050
{
    if (from == bp(m(self, NF_EDIT))) {
        if (code == 1) {
            if (m(self, NF_CALLBACK))
                vc(bp(m(self, NF_CALLBACK)), 1);
            return vc(self, V_INVALIDATE, 1);
        }
        return vc(self, V_INVALIDATE, code);
    }
    if (from == bp(m(self, NF_UP)))
        return step_value(self, m<float>(self, NF_STEP));
    if (from == bp(m(self, NF_DOWN)))
        return step_value(self, round_float(-(long double)m<float>(self, NF_STEP)));
    return pv(from);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_edit)
{
    const Hook list[] = {
#include "ui_edit_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
