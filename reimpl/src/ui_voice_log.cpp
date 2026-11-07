// The voice log panel ("~CVoiceLogPanel", docs/DECOMPILATION.md 9, "The rest of the user interface"): the third
// page of the inventory screen, a list box over the voice log's tree (the application's slot 97, 0x47e910) with
// a header of three columns, the image scroll bar and the "Replay" button; replaying a line queues the lines
// recorded at the same time after it, which the dialogue window plays one after another (a callback object it
// deletes with itself). Every call into the toolkit or the rest of the game goes through its exe address.
//
// Layout [confirmed: the constructor, the destructor and the slots below] (0x130 bytes, vtable 0x602608, a page
// window 0x42d3d0): +0x100 the list box, +0x104 the tree (+4 the save data with the elements: +0xc the time
// (double), +0x14 the order, +8 the action), +0x10c the header, +0x110 Replay, +0x114 the bar, +0x118 shown,
// +0x11c the queue of rows to replay (an int array, vtable 0x5ee168: +0x120 data from operator new, +0x124
// count, +0x128 capacity)
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr int VALUE = 77, SHOW_ROW = 80, COUNT = 22;

BYTE *elements(BYTE *self) { return fn<BYTE *(THISCALL *)(DWORD)>(0x5bb9a0)(m(bp(m(self, 0x104)), 4)); }
BYTE *element(BYTE *elements, DWORD i) { return bp(m(bp(m(elements, 4)), i * 4)); }
DWORD tree_count(BYTE *self) { return vc(bp(m(self, 0x104)), COUNT); }
// Two elements recorded at the same time (the doubles compared as fcomp does: unordered counts as equal)
bool same_time(BYTE *a, BYTE *b)
{
    const double x = m<double>(a, 0xc), y = m<double>(b, 0xc);
    return !(x < y) && !(x > y);
}
// The replay queue emptied (its data freed)
void clear_queue(BYTE *self)
{
    if (DWORD d = m(self, 0x120)) {
        op_delete(bp(d));
        m(self, 0x120) = 0;
    }
    m(self, 0x128) = 0;
    m(self, 0x124) = 0;
}

}  // namespace

// The constructor (the parent): the header with the display options' columns (+0x144 of the inventory display:
// texts +0x38/+0x44/+0x50, widths +0x40/+0x4c/+0x58) and the materials generator's colour, the list box over
// the voice log's tree (its own sort compare 0x5959d0), the font (inline loaded), the image scroll bar, Replay
BYTE *THISCALL VoiceLogPanel_Ctor(BYTE *self, DWORD parent)   // 0x5948b0
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x42d3d0)(self, parent, glob_addr(0x62458c));
    m(self, 0x100) = 0, m(self, 0x104) = 0;
    mb(self, 0x108) = 0;
    m(self, 0x10c) = 0, m(self, 0x110) = 0, m(self, 0x114) = 0;
    mb(self, 0x118) = 0;
    m(self, 0x11c) = vt(0x5ee168);
    m(self, 0x120) = 0, m(self, 0x124) = 0, m(self, 0x128) = 0, m(self, 0x12c) = 0;
    m(self, 0) = vt(0x602608);
    const DWORD rules = custom_rules();
    BYTE *opt = fn<BYTE *(THISCALL *)(DWORD)>(0x5a1d40)(rules) + 0x144;
    BYTE *header = alloc(0x140);
    if (header)
        header = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x4020d0)(header, self);
    m(self, 0x10c) = pv(header);
    const DWORD colour = fn<DWORD(THISCALL *)(DWORD)>(0x5a1cd0)(rules) + 0x88;
    fn<void(THISCALL *)(DWORD, DWORD)>(0x402b40)(m(self, 0x10c), colour);
    const DWORD columns[3][2] = {{0x38, 1}, {0x44, 1}, {0x50, 0}};
    for (const auto &c : columns) {
        const DWORD text = fn<DWORD(THISCALL *)(BYTE *)>(0x5007b0)(opt + c[0]);
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x402770)(m(self, 0x10c), text, m(opt, c[0] + 8), 0, c[1], 0);
    }
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x402b90)(m(self, 0x10c), 2, 0);
    vc(bp(m(self, 0x10c)), 42);
    add_child(self, bp(m(self, 0x10c)), 1, 0, NOWHERE, NOWHERE, 0);
    const DWORD tree = vc(bp(gd(glob_addr(G_APP))), 97);
    m(self, 0x104) = fn<DWORD(THISCALL *)(DWORD)>(0x47e910)(tree);
    BYTE *list = alloc(0x258);
    if (list)
        list = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x4041f0)(list, self, m(self, 0x104), 999999, 1);
    m(self, 0x100) = pv(list);
    add_child(self, list, 8, 0, NOWHERE, NOWHERE, 0);
    mb(bp(m(self, 0x100)), 0x257) = 0;
    fn<void(THISCALL *)(DWORD, DWORD)>(0x406aa0)(m(self, 0x100), m(self, 0x10c));
    m(bp(m(self, 0x100)), 0x240) = fn_addr(0x5959d0);
    mb(bp(m(self, 0x100)), 0x254) = 0;
    m(bp(m(self, 0x104)), 8) = m(self, 0x100);
    vc(bp(m(self, 0x100)), 46, record_inline(glob_addr(FONTS), m(opt, 0x6c)));
    vc(bp(m(self, 0x100)), 42);
    BYTE *bar = alloc(0x190);
    if (bar)
        bar = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4144b0)(bar, self, 1);
    m(self, 0x114) = pv(bar);
    add_child(self, bar, 0, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x114)), V_SET_SIZE, m(opt, 0x78), m(opt, 0x7c));
    move_to(bp(m(self, 0x114)), mi(opt, 0x70), mi(opt, 0x74));
    vc(bp(m(self, 0x114)), 42);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x407a00)(m(self, 0x100), m(self, 0x114), 1);
    BYTE *b = alloc(0x138);
    if (b)
        b = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x40f9b0)(b, self, fn<DWORD(THISCALL *)(BYTE *)>(0x5007a0)(opt + 0x90), 0);
    m(self, 0x110) = pv(b);
    add_child(self, b, 8, 0, NOWHERE, NOWHERE, 0);
    vc(bp(m(self, 0x110)), 40, 0xd);
    const DWORD replay = record_inline(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x615cec)));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(m(self, 0x110), replay, 1, 0);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x4101c0)(m(self, 0x110), 1);
    mb(self, 0x118) = 1;
    return self;
}
BYTE *THISCALL VoiceLogPanel_DeletingDtor(BYTE *self, DWORD flags)   // 0x594dc0
{
    m(self, 0) = vt(0x602608);
    m(self, 0x104) = 0;
    m(self, 0x11c) = vt(0x5ee168);
    if (DWORD d = m(self, 0x120)) {
        op_delete(bp(d));
        m(self, 0x120) = 0;
    }
    m(self, 0x128) = 0;
    m(self, 0x124) = 0;
    fn<void(THISCALL *)(BYTE *)>(0x42d3f0)(self);
    if (flags & 1)
        pool_delete(self, 0x130);
    return self;
}
// Slot 21 (the child, what): the bar's to the list box; a click on the list or Replay replays the chosen line
// (its action's slot 29 with 0, 0, 0, 0, 1) and, when the next lines were recorded at the same time, the
// dialogue window gets a callback (0x5957d0) and the queue those lines (each one's action's slot 29 with zeros)
DWORD THISCALL VoiceLogPanel_Notify(BYTE *self, BYTE *from, DWORD what)   // 0x5953d0
{
    if (from && pv(from) == m(self, 0x114))
        return vc(bp(m(self, 0x100)), V_NOTIFY, from, what);
    if (what || !from)
        return 0;
    BYTE *list = bp(m(self, 0x100));
    if (from != list && pv(from) != m(self, 0x110))
        return 0;
    vc(list, VALUE);   // (asked twice, the first answer unused)
    const DWORD n = tree_count(self);
    if (vc(bp(m(self, 0x100)), VALUE) >= n)
        return 0;
    clear_queue(self);
    DWORD row = vc(bp(m(self, 0x100)), VALUE);
    vc(bp(m(element(elements(self), row), 8)), 29, 0, 0, 0, 0, 1);
    row = vc(bp(m(self, 0x100)), VALUE);
    if (row + 1 >= tree_count(self))
        return 0;
    BYTE *a = elements(self);
    BYTE *b = elements(self);
    if (!same_time(element(a, row), element(b, row + 1)))
        return 0;
    BYTE *cb = op_new(0x18);
    if (cb) {
        m(cb, 8) = fn_addr(0x5957d0);
        m(cb, 0x10) = 0;
        m(cb, 0) = vt(0x5ea350);
        m(cb, 4) = pv(self);
        m(cb, 0xc) = 0;
        m(cb, 0x14) = 0;
    }
    fn<void(THISCALL *)(DWORD, BYTE *)>(0x47c9d0)(fn<DWORD(__cdecl *)()>(0x47ca90)(), cb);
    if (row + 1 >= tree_count(self))
        return 0;
    do {
        BYTE *x = elements(self);
        BYTE *y = elements(self);
        if (!same_time(element(x, row), element(y, row + 1)))
            return 0;
        row++;
        vc(bp(m(element(elements(self), row), 8)), 29, 0, 0, 0, 0, 0);
        vc(self + 0x11c, 2, &row);
    } while (row + 1 < tree_count(self));
    return 0;
}
// Shown again (a bool): the queue emptied; when shown, the list box over the tree sorted by time with the
// first of the last lines recorded together shown and the bar at the top; Replay enabled when a line is chosen
void THISCALL VoiceLogPanel_Show(BYTE *self, BYTE shown)   // 0x5956a0
{
    clear_queue(self);
    if (shown) {
        fn<void(THISCALL *)(DWORD, DWORD)>(0x406890)(m(self, 0x100), m(self, 0x104));
        int i = (int)tree_count(self) - 1;
        while (i > 0) {
            BYTE *a = elements(self);
            BYTE *b = elements(self);
            if (!same_time(element(a, (DWORD)i - 1), element(b, (DWORD)i)))
                break;
            i--;
        }
        if (i >= 0) {
            m(bp(m(self, 0x10c)), 0x12c) = 0;
            fn<void(THISCALL *)(DWORD, DWORD)>(0x406b60)(m(self, 0x100), 0);
            BYTE *list = bp(m(self, 0x100));
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x406b70)(list, 0, m(list, 0x108));
            vc(bp(m(self, 0x100)), SHOW_ROW, i, 0);
            fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x413c20)(m(self, 0x114), 0, 0);
        }
    }
    vc(bp(m(self, 0x100)), VALUE);   // (asked twice, the first answer unused)
    const DWORD n = tree_count(self);
    const DWORD row = vc(bp(m(self, 0x100)), VALUE);
    mb(bp(m(self, 0x110)), W_ENABLED) = row < n;
}
// The dialogue window's callback: the next queued row shown in the list box
void THISCALL VoiceLogPanel_NextQueued(BYTE *self)   // 0x5957d0
{
    if (!m(self, 0x124))
        return;
    vc(bp(m(self, 0x100)), SHOW_ROW, m(bp(m(self, 0x120)), 0), 0);
    const DWORD n = m(self, 0x124);
    if (!n)
        return;
    if (n > 1)
        memmove_(bp(m(self, 0x120)), bp(m(self, 0x120)) + 4, n * 4 - 4);
    m(self, 0x124) = m(self, 0x124) - 1;
}
// The input slots: any mouse or key input empties the queue first, then the window's own slot
DWORD THISCALL VoiceLogPanel_Mouse9(BYTE *self, DWORD a, DWORD b, DWORD c)   // 0x595830
{
    clear_queue(self);
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x40bc30)(self, a, b, c);
}
DWORD THISCALL VoiceLogPanel_MouseDown(BYTE *self, DWORD a, DWORD b)   // 0x595880
{
    clear_queue(self);
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bcc0)(self, a, b);
}
DWORD THISCALL VoiceLogPanel_Mouse12(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d)   // 0x5958d0
{
    clear_queue(self);
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x40c0a0)(self, a, b, c, d);
}
DWORD THISCALL VoiceLogPanel_Mouse15(BYTE *self, DWORD a, DWORD b)   // 0x595930
{
    clear_queue(self);
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40c0b0)(self, a, b);
}
DWORD THISCALL VoiceLogPanel_Key(BYTE *self, DWORD key)   // 0x595980
{
    clear_queue(self);
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
}
// The list box's sort compare (fastcall: the column's text or null, the first text; the second text, the
// source, the rows i and j): by text when a column is given, else (or when equal) by time, then by order
int __fastcall VoiceLogPanel_Compare(const char *column, const char *a, const char *b, BYTE *source, DWORD i, DWORD j)   // 0x5959d0
{
    if (column) {
        const int r = crt_stricmp(a, b);
        if (r)
            return r;
    }
    using Elements = BYTE *(THISCALL *)(DWORD);
    BYTE *x = fn<Elements>(0x5bb9a0)(m(vc<BYTE *>(source, 5), 4));
    BYTE *y = fn<Elements>(0x5bb9a0)(m(vc<BYTE *>(source, 5), 4));
    if (same_time(element(x, i), element(y, j))) {
        BYTE *p = fn<Elements>(0x5bb9a0)(m(vc<BYTE *>(source, 5), 4));
        BYTE *q = fn<Elements>(0x5bb9a0)(m(vc<BYTE *>(source, 5), 4));
        return m(element(p, i), 0x14) < m(element(q, j), 0x14) ? 1 : -1;
    }
    BYTE *p = fn<Elements>(0x5bb9a0)(m(vc<BYTE *>(source, 5), 4));
    BYTE *q = fn<Elements>(0x5bb9a0)(m(vc<BYTE *>(source, 5), 4));
    return !(m<double>(element(p, i), 0xc) >= m<double>(element(q, j), 0xc)) ? 1 : -1;   // (unordered counts as less)
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_voice_log)
{
    const Hook list[] = {
#include "ui_voice_log_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
