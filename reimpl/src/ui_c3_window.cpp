// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the slots of the window base class CJWindow (vtable 0x5eb404)
// that no table listed, because the toolkit's own vtables are not those of a registered class: the getters and
// setters the compiler shared between the toolkit's classes (0x401110-0x4011d0, the same function in up to 67
// vtables), the dispatch of the mouse move and of slots 22-25 to the children, showing and hiding, the focus
// order, the colour setters, the placement of a window relative to its parent or a sibling (slot 62), the INI
// position (slot 67), the window textures (slot 71) and the title drawing with the arrow titles (slot 74); the
// toolkit's member function callback (vtable 0x5ea350) and its base (0x5ea4a0).
//
// Every call goes through its exe address (the original's side of a check runs the original all the way down).
// eax is what the original leaves where it defines it; functions that set al only leave the upper bytes of
// whatever came before and say so.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD FONT_HEIGHT = 0x80;
constexpr DWORD NOWHERE = 0xfffffc18;   // -1000: "keep" / "the default"
constexpr DWORD G_INI = 0x62e640, G_REPORTER = 0x6c02cc, G_MASK = 0x6bcc68, G_BLACK_BIAS = 0x6b9728;

inline DWORD c_str(const void *z) { return zax::fn<DWORD(THISCALL *)(const void *)>(0x4e13a0)(z); }
inline bool not_empty(const void *z) { return zax::fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
inline int ini_get(DWORD section, const char *key, int def)
{
    return zax::fn<int(__fastcall *)(DWORD, DWORD, const char *, int)>(0x4a4670)(gd(glob_addr(G_INI)), section, key, def);
}
inline int font_unit(BYTE *w) { return (int)(m(bp(m(w, W_FONT)), FONT_HEIGHT) / 5); }
inline int width(BYTE *w) { return mi(w, W_RECT + 8) - mi(w, W_RECT); }
inline int height(BYTE *w) { return mi(w, W_RECT + 12) - mi(w, W_RECT + 4); }
inline DWORD set_rect(BYTE *w, int l, int t, int r, int b)
{
    int rc[4] = {l, t, r, b};
    return vc(w, V_SET_RECT, rc);
}
inline bool in_visible(const int *p, BYTE *w)
{
    return p[0] >= mi(w, W_VISIBLE) && p[1] >= mi(w, W_VISIBLE + 4) && p[0] < mi(w, W_VISIBLE + 8) &&
           p[1] < mi(w, W_VISIBLE + 12);
}
// The root window as a screen window: its slot 39 answers whether it is one (neg al; sbb; and: null if not)
BYTE *screen_root(BYTE *w)
{
    BYTE *r = w;
    for (BYTE *p = bp(m(w, W_PARENT)); p; p = bp(m(p, W_PARENT)))
        r = p;
    return (vc(r, V_IS_SCREEN) & 0xff) ? r : nullptr;
}
// The display object of the error reporter (+4): slot 45 the screen width, slot 39 its height
inline BYTE *display() { return bp(m(bp(gd(glob_addr(G_REPORTER))), 4)); }
inline float opacity(DWORD mode) { return zax::fn<float(THISCALL *)(DWORD)>(0x433c20)(mode); }
inline void set_opacity(DWORD mode, DWORD bits) { zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x433be0)(mode, bits); }
inline DWORD float_bits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

}  // namespace

// ---- The getters and setters the toolkit's classes share ------------------------------------------------------

// Slot 20 and dozens of other "(one argument) nothing" slots: ret 4 (eax the caller's)
void THISCALL Shared_Nop4(BYTE *, DWORD)   // 0x401110
{
}

// Slot 27: shown (+0x60)
DWORD THISCALL Window_IsShown(BYTE *self)   // 0x401120
{
    return mb(self, W_SHOWN);
}

// Slot 30: the parent's slot 30 (a tail jump), the root whether it is active (+0x5c)
DWORD THISCALL Window_TreeActive(BYTE *self)   // 0x401130
{
    if (BYTE *p = bp(m(self, W_PARENT)))
        return vc(p, V_30);
    return mb(self, W_ACTIVE) != 0;
}

// Slot 40: the accelerator (+0x5b; al the byte, the upper bytes the caller's)
DWORD THISCALL Window_SetAccel(BYTE *self, DWORD key)   // 0x401150
{
    mb(self, W_ACCEL) = (BYTE)key;
    return key;
}

// Slot 41: the accelerator (al; the upper bytes the caller's)
DWORD THISCALL Window_Accel(BYTE *self)   // 0x401160
{
    return mb(self, W_ACCEL);
}

// Slot 44: the title as a C string (a tail jump to ZString_c_str)
DWORD THISCALL Window_TitleText(BYTE *self)   // 0x401170
{
    return c_str(self + W_TITLE);
}

// Slot 45: the font
DWORD THISCALL Window_Font(BYTE *self)   // 0x401180
{
    return m(self, W_FONT);
}

// Slot 46: the font set (eax the font)
DWORD THISCALL Window_SetFont(BYTE *self, DWORD font)   // 0x401190
{
    m(self, W_FONT) = font;
    return font;
}

// Slot 47: the title as a number (atol of slot 44's text)
DWORD THISCALL Window_TitleInt(BYTE *self)   // 0x4011a0
{
    const char *t = vc<const char *>(self, 44);
    return zax::fn<DWORD(__cdecl *)(const char *)>(0x5d0348)(t);
}

// Slot 48: the title as a floating point number (atof of slot 44's text, in st0)
double THISCALL Window_TitleFloat(BYTE *self)   // 0x4011c0
{
    const char *t = vc<const char *>(self, 44);
    return zax::fn<double(__cdecl *)(const char *)>(0x5d03de)(t);
}

// Slot 58: the rectangle (+4)
DWORD THISCALL Window_Rect(BYTE *self)   // 0x4011d0
{
    return pv(self + W_RECT);
}

// Slot 18 and slots 39, 73 and others of many classes: false (xor al, al; the upper bytes the caller's)
DWORD THISCALL Shared_False(BYTE *)   // 0x401200
{
    return 0;
}

// The base of the member function callbacks (vtable 0x5ea4a0) slot 2 and others: true (mov al, 1)
DWORD THISCALL Shared_True(BYTE *)   // 0x401260
{
    return 1;
}

// ---- The toolkit's member function callback (vtable 0x5ea350, 0x18 bytes: +4 the object, +8 the function of slot 1
// and +0xc its this adjustment, +0x10 the function of slot 2 and +0x14 its adjustment)

// Slot 1: the function on the adjusted object (a tail jump; thiscall without arguments)
DWORD THISCALL Callback_Call(BYTE *self)   // 0x401210
{
    using Fn = DWORD(THISCALL *)(BYTE *);
    return at<Fn>(m(self, 8))(bp(m(self, 0xc) + m(self, 4)));
}

// Slot 2: the second function when there is one; true otherwise (mov al, 1 over eax = this)
DWORD THISCALL Callback_Test(BYTE *self)   // 0x401220
{
    if (DWORD fn = m(self, 0x10)) {
        using Fn = DWORD(THISCALL *)(BYTE *);
        return fn_dyn<Fn>(fn)(bp(m(self, 0x14) + m(self, 4)));
    }
    return (pv(self) & 0xffffff00u) | 1;
}

// Slot 0: the deleting destructor (back to the base's vtable 0x5ea4a0; operator delete)
BYTE *THISCALL Callback_DeletingDtor(BYTE *self, DWORD flags)   // 0x401240
{
    m(self, 0) = vt(0x5ea4a0);
    if (flags & 1)
        zax::fn<void(__cdecl *)(void *)>(0x5d0320)(self);
    return self;
}

// ---- The slots of the base class ------------------------------------------------------------------------------

// Slot 6: deletes the window (slot 0 with 1) when there is one (eax the caller's otherwise)
DWORD THISCALL Window_Delete(BYTE *self)   // 0x40bba0
{
    if (self)
        return vc(self, V_DELETE, 1);
    return 0;
}

// Slot 8: (frame, colour) a rectangle one pixel outside the window's rectangle (Frame_DrawRect 0x4b3430)
DWORD THISCALL Window_DrawOutline(BYTE *self, DWORD frame, DWORD color)   // 0x40bbe0
{
    int r[4] = {mi(self, W_RECT) - 1, mi(self, W_RECT + 4) - 1, mi(self, W_RECT + 8) + 1, mi(self, W_RECT + 12) + 1};
    return zax::fn<DWORD(THISCALL *)(DWORD, int *, DWORD)>(0x4b3430)(frame, r, color);
}

// Slot 12: slot 10 (the button press) with the same arguments
DWORD THISCALL Window_Slot12(BYTE *self, DWORD a, DWORD b)   // 0x40be50
{
    return vc(self, V_MOUSE_DOWN, a, b);
}

// Slot 13, the mouse move: (point, a, cursor out) with a capture slot 14 (the drag) first; then the children in
// front to back order (the shown ones that are enabled or +0x62) until a modal one: the first under the point
// gets the mouse (slot 65 asked, slot 70 when it says no) and names the cursor (slot 69, the point in its screen
// coordinates); each child that is not under the point, or every one after the first, gets slot 13 and ends the
// dispatch when it takes it. Then the window itself: slot 70 when slot 65 says so, no child took the point and it
// is shown, enabled and under the point. 1 when taken, 0 otherwise.
DWORD THISCALL Window_MouseMove(BYTE *self, int *pt, DWORD a, DWORD *cursor)   // 0x40bf10
{
    if (BYTE cap = mb(self, W_CAPTURE)) {
        if (vc(self, V_MOUSE_DRAG, pt, a, (DWORD)cap, cursor) & 0xff)
            return 1;
    }
    bool taken = false;   // bl
    bool first = false;   // the first child under the point had the mouse
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (!mb(c, W_SHOWN))
            goto next;
        if (!mb(c, W_ENABLED) && !mb(c, 0x62))
            goto next;
        {
            const bool in = in_visible(pt, c);
            if (in) {
                if (first)
                    goto again;
                first = true;
                if (!(vc(c, 65) & 0xff))
                    vc(c, V_PARENT_CURSOR, pt);
                int local[2] = {pt[0] - mi(c, W_SCREEN), pt[1] - mi(c, W_SCREEN + 4)};
                *cursor = vc(c, V_CURSOR, local);
            }
            if (!taken)
                goto dispatch;
        }
    again:
        if (in_visible(pt, c))
            goto done;
    dispatch:
        if (vc(c, V_MOUSE_MOVE, pt, a, cursor))
            return 1;
    done:
        taken = first;
    next:
        if (mb(c, W_MODAL))
            break;
    }
    if ((vc(self, 65) & 0xff) && !taken && mb(self, W_SHOWN) && mb(self, W_ENABLED) && in_visible(pt, self))
        vc(self, V_PARENT_CURSOR, pt);
    return 0;
}

// Slots 22-25: the same slot of the shown children, front to back, until one says true (al 1) or a modal one;
// false otherwise (al 0; the upper bytes the last call's or the caller's)
namespace {
DWORD to_children(BYTE *self, int slot, DWORD a)
{
    DWORD r = 0;
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (mb(c, W_SHOWN)) {
            r = vc(c, slot, a);
            if (r & 0xff)
                return (r & 0xffffff00u) | 1;
        }
        if (mb(c, W_MODAL))
            break;
    }
    return r & 0xffffff00u;
}
}  // namespace
DWORD THISCALL Window_ChildrenSlot22(BYTE *self, DWORD a) { return to_children(self, 22, a); }   // 0x40c430
DWORD THISCALL Window_ChildrenSlot23(BYTE *self, DWORD a) { return to_children(self, 23, a); }   // 0x40c480
DWORD THISCALL Window_ChildrenSlot24(BYTE *self, DWORD a) { return to_children(self, 24, a); }   // 0x40c4d0
DWORD THISCALL Window_ChildrenSlot25(BYTE *self, DWORD a) { return to_children(self, 25, a); }   // 0x40c520

// Slot 26: (show, focus) shown or hidden (hiding sets the toolkit's "redraw" byte 0x62e5c4); slot 20 with the
// new state (the argument's upper bytes and al = show != 0); with focus 1 the parent focuses the window when it
// shows, or its next sibling when it hides
void THISCALL Window_Show(BYTE *self, DWORD show, DWORD focus)   // 0x40c570
{
    if (mb(self, W_SHOWN) == (BYTE)show)
        return;
    mb(self, W_SHOWN) = (BYTE)show;
    if (!(BYTE)show)
        gb(glob_addr(0x62e5c4)) = 1;
    vc(self, 20, (show & 0xffffff00u) | (show != 0));
    if (focus != 1)
        return;
    BYTE *p = bp(m(self, W_PARENT));
    if (mb(self, W_SHOWN)) {
        if (p)
            vc(p, V_FOCUS, self);
        return;
    }
    if (p) {
        if (BYTE *n = bp(m(self, W_NEXT)))
            vc(p, V_FOCUS, n);
    }
}

// Slot 28: shown here and in every parent (the parent's slot 28 when shown and there is one)
DWORD THISCALL Window_IsVisible(BYTE *self)   // 0x40c5e0
{
    BYTE *p = bp(m(self, W_PARENT));
    if (p && mb(self, W_SHOWN))
        return vc(p, V_28);
    return mb(self, W_SHOWN);
}

// Slot 29: slot 29 of every child (eax the last call's or the caller's)
void THISCALL Window_ChildrenSlot29(BYTE *self)   // 0x40c660
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        vc(c, 29);
}

// Slot 36: the next sibling that takes the focus (+0x5d) and is shown, from the parent's first child again
// after the last; null when there is none before the window itself
DWORD THISCALL Window_NextTabStop(BYTE *self)   // 0x40cbb0
{
    for (BYTE *c = bp(m(self, W_NEXT)); c; c = bp(m(c, W_NEXT)))
        if (mb(c, W_TAB_STOP) && mb(c, W_SHOWN))
            return pv(c);
    for (BYTE *c = bp(m(bp(m(self, W_PARENT)), W_FIRST)); c != self; c = bp(m(c, W_NEXT)))
        if (mb(c, W_TAB_STOP) && mb(c, W_SHOWN))
            return pv(c);
    return 0;
}

// Slot 37: the same backwards (previous siblings, then from the parent's last child)
DWORD THISCALL Window_PrevTabStop(BYTE *self)   // 0x40cc00
{
    for (BYTE *c = bp(m(self, W_PREV)); c; c = bp(m(c, W_PREV)))
        if (mb(c, W_TAB_STOP) && mb(c, W_SHOWN))
            return pv(c);
    for (BYTE *c = bp(m(bp(m(self, W_PARENT)), W_LAST)); c != self; c = bp(m(c, W_PREV)))
        if (mb(c, W_TAB_STOP) && mb(c, W_SHOWN))
            return pv(c);
    return 0;
}

// Slot 38: (a) the parent's slot 21 (the notification) with the window and a; 0 without a parent
DWORD THISCALL Window_NotifyParent(BYTE *self, DWORD a)   // 0x40cc50
{
    if (BYTE *p = bp(m(self, W_PARENT)))
        return vc(p, V_NOTIFY, self, a);
    return 0;
}

// Slot 49: (number) the title set to its decimal text (slot 43)
DWORD THISCALL Window_SetTitleInt(BYTE *self, int v)   // 0x40cf00
{
    char text[0x14];
    zax::fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x60a5a0), v);
    return vc(self, V_SET_TITLE, text);
}

// Slot 50: (float) the title set to its "%g" text (slot 43)
DWORD THISCALL Window_SetTitleFloat(BYTE *self, float v)   // 0x40cf40
{
    char text[0x14];
    zax::fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x60a5a4), (double)v);
    return vc(self, V_SET_TITLE, text);
}

// Slots 51, 52, 54, 55, 56: (r, g, b) one of the six colours (FieldColor_SetRGBA 0x53f080, alpha 0xff)
namespace {
DWORD set_color(BYTE *self, DWORD off, DWORD r, DWORD g, DWORD b)
{
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(self + off, r, g, b, 0xff);
}
}  // namespace
DWORD THISCALL Window_SetColor80(BYTE *self, DWORD r, DWORD g, DWORD b) { return set_color(self, 0x80, r, g, b); }   // 0x40cf80
DWORD THISCALL Window_SetColor90(BYTE *self, DWORD r, DWORD g, DWORD b) { return set_color(self, 0x90, r, g, b); }   // 0x40cfb0
DWORD THISCALL Window_SetColorB0(BYTE *self, DWORD r, DWORD g, DWORD b) { return set_color(self, 0xb0, r, g, b); }   // 0x40d030
DWORD THISCALL Window_SetColorC0(BYTE *self, DWORD r, DWORD g, DWORD b) { return set_color(self, 0xc0, r, g, b); }   // 0x40d060
DWORD THISCALL Window_SetColorD0(BYTE *self, DWORD r, DWORD g, DWORD b) { return set_color(self, 0xd0, r, g, b); }   // 0x40d090

// Slot 62, the placement: (how, other, dx, dy, flags) the window moved (slot 57, its size kept) to a place in
// its parent or next to another window. how 0 does nothing; 8 without a previous sibling counts as 1. dx and dy
// are in fifths of the font height unless flags bit 1; -1000 takes the parent's border (+0x78 for 1-4 and 23,
// none for 9, 10, 19 and 22, +0x79 else) or dx for dy (9 and 10: +0x79, 22: 1). Places: 1, 5 and 8 the top
// left inside the borders (slots 60/61), 2 the top right, 3 right at the top, 4 the bottom right, 6 the middle
// (vertically at the bottom when it does not fit), 7/8 below another window / the previous sibling, 9/10 its
// right edges aligned, 11/12 centred below, 13/14 right of it, 15/16 right of it centred vertically, 17/18
// right of it bottom aligned, 19 the parent's width (slot 59), 20/21 left of it, 22 at the mouse (kept on the
// screen and in the parent; the cursor moved by the correction), 23 at the bottom. Then, unless flags bit 0,
// the parent's slot 63 (fit to the child) with -1000 or, with flags bit 2, 0.
void THISCALL Window_Place(BYTE *self, DWORD how, BYTE *other, int dx, int dy, DWORD flags)   // 0x40db20
{
    if (!how)
        return;
    if (how == 8 && !m(self, W_PREV))
        how = 1;
    BYTE *parent = bp(m(self, W_PARENT));
    int x, y;
    if ((DWORD)dx == NOWHERE) {
        switch (how) {
        case 1: case 2: case 3: case 4: case 0x17: x = (signed char)mb(parent, W_BORDER); break;
        case 9: case 10: case 0x13: case 0x16: x = 0; break;
        default: x = (signed char)mb(parent, W_BORDER2); break;
        }
    } else {
        x = dx;
        if (!(flags & 2))
            x = dx * font_unit(self);
    }
    if ((DWORD)dy == NOWHERE) {
        y = x;
        if ((int)how >= 9) {
            if ((int)how <= 10)
                y = (signed char)mb(parent, W_BORDER2);
            else if (how == 0x16)
                y = 1;
        }
    } else {
        y = dy;
        if (!(flags & 2))
            y = dy * font_unit(self);
    }
    const int w = width(self), h = height(self);
    BYTE *o = other;
    switch (how) {
    case 1:
    case 5:
    top_left: {
        const int t = vc(parent, V_BORDER_H);
        const int l = vc(parent, V_BORDER_W);
        set_rect(self, l + x, t + y, width(self) + l + x, height(self) + t + y);
        break;
    }
    case 2: {
        const int pr = mi(parent, W_RECT + 8), pl = mi(parent, W_RECT);
        const int ww = width(self);
        const int t = vc(parent, V_BORDER_H);
        const int l = pr - pl - ww - x;
        set_rect(self, l, t + y, width(self) + l, height(self) + t + y);
        break;
    }
    case 3: {
        const int l = mi(parent, W_RECT + 8) - mi(self, W_RECT + 8) - mi(parent, W_RECT) - x + mi(self, W_RECT);
        set_rect(self, l, y, w + l, h + y);
        break;
    }
    case 4: {
        const int t = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) - mi(parent, W_RECT + 4) + mi(parent, W_RECT + 12) - y;
        const int l = mi(parent, W_RECT + 8) - mi(self, W_RECT + 8) - mi(parent, W_RECT) - x + mi(self, W_RECT);
        set_rect(self, l, t, w + l, h + t);
        break;
    }
    case 6: {
        int l = (mi(parent, W_VISIBLE + 8) - mi(self, W_RECT + 8) - mi(parent, W_VISIBLE) + mi(self, W_RECT)) / 2;
        int t = (mi(self, W_RECT + 4) - mi(self, W_RECT + 12) - mi(parent, W_VISIBLE + 4) + mi(parent, W_VISIBLE + 12)) / 2;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        if (mi(self, W_RECT + 12) <= mi(parent, W_VISIBLE + 12) - mi(parent, W_VISIBLE + 4))
            break;
        t = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) - mi(parent, W_VISIBLE + 4) + mi(parent, W_VISIBLE + 12);
        l = (mi(parent, W_VISIBLE + 8) - mi(self, W_RECT + 8) - mi(parent, W_VISIBLE) + mi(self, W_RECT)) / 2;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 8:
        o = bp(m(self, W_PREV));
        if (!o)
            goto top_left;
        [[fallthrough]];
    case 7: {
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 0xc) + y;
        const int l = mi(vc<BYTE *>(o, V_GET_RECT), 0);
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 10:
        o = bp(m(self, W_PREV));
        [[fallthrough]];
    case 9: {
        const int sr = mi(self, W_RECT + 8), sl = mi(self, W_RECT);
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 0xc) + y;
        const int l = mi(vc<BYTE *>(o, V_GET_RECT), 8) - (sr - sl) + x;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 12:
        o = bp(m(self, W_PREV));
        [[fallthrough]];
    case 11: {
        const int sl = mi(self, W_RECT), pr = mi(parent, W_VISIBLE + 8), pl = mi(parent, W_VISIBLE), sr = mi(self, W_RECT + 8);
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 0xc) + y;
        const int l = ((pr - pl) - (sr - sl)) / 2;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 14:
        o = bp(m(self, W_PREV));
        [[fallthrough]];
    case 13: {
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 4);
        const int l = mi(vc<BYTE *>(o, V_GET_RECT), 8) + x;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 16:
        o = bp(m(self, W_PREV));
        [[fallthrough]];
    case 15: {
        const int sb = mi(self, W_RECT + 12), st = mi(self, W_RECT + 4);
        BYTE *r = vc<BYTE *>(o, V_GET_RECT);
        const int oh = mi(r, 0xc) - mi(r, 4);
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 4) - ((sb - st) - oh) / 2;
        const int l = mi(vc<BYTE *>(o, V_GET_RECT), 8) + x;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 18:
        o = bp(m(self, W_PREV));
        [[fallthrough]];
    case 17: {
        const int hh = height(self);
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 0xc) - hh;
        const int l = mi(vc<BYTE *>(o, V_GET_RECT), 8) + x;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 0x13: {
        vc(self, V_SET_SIZE, width(self), mi(parent, W_RECT + 12) - mi(parent, W_RECT + 4));
        const int l = mi(parent, W_RECT + 8) - mi(parent, W_RECT);
        set_rect(self, l, 0, width(self) + l, height(self));
        break;
    }
    case 0x15:
        o = bp(m(self, W_PREV));
        [[fallthrough]];
    case 0x14: {
        const int sr = mi(self, W_RECT + 8), sl = mi(self, W_RECT);
        const int t = mi(vc<BYTE *>(o, V_GET_RECT), 4);
        const int l = mi(vc<BYTE *>(o, V_GET_RECT), 0) - (sr - sl) - x;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        break;
    }
    case 0x16: {
        int cx, cy;
        zax::fn<void(THISCALL *)(DWORD, int *, int *)>(0x5184e0)(glob_addr(0x6cff40), &cx, &cy);
        BYTE *s = screen_root(self);
        if ((mi(s, W_RECT + 8) - mi(s, W_RECT)) - cx < width(self)) {
            const int nx = cx - width(self);
            if (nx >= 0)
                cx = nx;
        }
        s = screen_root(self);
        if ((mi(s, W_RECT + 12) - mi(s, W_RECT + 4)) - cy < height(self)) {
            const int ny = cy - height(self);
            if (ny >= 0)
                cy = ny;
        }
        const int want_y = y + cy, want_x = cx + x;
        int l = want_x, t = want_y;
        if (BYTE *p = bp(m(self, W_PARENT))) {
            if (mi(p, W_RECT + 8) - mi(p, W_RECT) < width(self) + want_x)
                l = mi(p, W_RECT + 8) - mi(self, W_RECT + 8) - mi(p, W_RECT) + mi(self, W_RECT);
            if (mi(p, W_RECT + 12) - mi(p, W_RECT + 4) < height(self) + want_y)
                t = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) - mi(p, W_RECT + 4) + mi(p, W_RECT + 12);
        }
        if (l < 0)
            l = 0;
        if (t < 0)
            t = 0;
        set_rect(self, l, t, width(self) + l, height(self) + t);
        if (l != want_x || t != want_y)
            zax::fn<void(THISCALL *)(DWORD, int, int)>(0x518570)(glob_addr(0x6cff40), l - want_x + cx, t - want_y + cy);
        break;
    }
    case 0x17: {
        const int t = mi(parent, W_RECT + 12) - mi(parent, W_RECT + 4) - mi(self, W_RECT + 12) + mi(self, W_RECT + 4) - y;
        set_rect(self, x, t, width(self) + x, height(self) + t);
        break;
    }
    default:
        break;
    }
    if (flags & 1)
        return;
    vc(bp(m(self, W_PARENT)), V_FIT_CHILD, self, (flags & 4) ? 0 : NOWHERE);
}

// Slot 64: (rect, except) whether a child other than `except` lies exactly on the rectangle (its screen rect),
// asked recursively of the children that have children of their own
DWORD THISCALL Window_ChildOnRect(BYTE *self, const int *r, BYTE *except)   // 0x40e510
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (c == except)
            continue;
        if (m(c, W_FIRST) && vc(c, 64, r, except))
            return 1;
        if (mi(c, W_SCREEN) == r[0] && mi(c, W_SCREEN + 4) == r[1] && mi(c, W_SCREEN + 8) == r[2] && mi(c, W_SCREEN + 12) == r[3])
            return 1;
    }
    return 0;
}

// Slot 67: (section, with size) the INI section (the title's text when null) and whether the size goes with it
// (+0x54/+0x58); the size (slot 59, both positive), the position (kept in the parent's screen rect, or on the
// display without one), "Show Window" (slot 26 with focus) and "Transparent Window" (also for every child,
// SetTransparent 0x40c600 with the argument's upper bytes) read from the game's INI file 0x62e640; then the
// window moved down and right by 10 while the parent has another child on its screen rect (slot 64)
void THISCALL Window_LoadIni(BYTE *self, DWORD section, DWORD with_size)   // 0x40e620
{
    if (!section)
        section = c_str(self + W_TITLE);
    m(self, W_INI) = section;
    mb(self, W_INI_SIZE) = (BYTE)with_size;
    if ((BYTE)with_size) {
        const int w = ini_get(section, glob<const char *>(0x60a5e8), width(self));
        const int h = ini_get(section, glob<const char *>(0x60a5e0), height(self));
        if (w > 0 && h > 0)
            vc(self, V_SET_SIZE, w, h);
    }
    int x = ini_get(section, glob<const char *>(0x60a5d4), mi(self, W_RECT));
    int y = ini_get(section, glob<const char *>(0x60a5c8), mi(self, W_RECT + 4));
    const DWORD show = (DWORD)ini_get(section, glob<const char *>(0x60a5bc), mb(self, W_SHOWN));
    const BYTE transparent = ini_get(section, glob<const char *>(0x60a5a8), mb(self, W_TRANSPARENT)) != 0;
    mb(self, W_TRANSPARENT) = transparent;
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x40c600)(c, (with_size & 0xffffff00u) | transparent);
    if (BYTE *p = bp(m(self, W_PARENT))) {
        if (mi(p, W_SCREEN) + x > mi(p, W_VISIBLE + 8))
            x = mi(p, W_VISIBLE + 8) - mi(self, W_RECT + 8) - mi(p, W_SCREEN) + mi(self, W_RECT);
        const int py = mi(p, W_SCREEN + 4);
        if (py + y > mi(p, W_VISIBLE + 12))
            y = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) - py + mi(p, W_VISIBLE + 12);
        if (width(self) + mi(p, W_SCREEN) + x < mi(p, W_VISIBLE))
            x = mi(p, W_VISIBLE);
        if (height(self) + py + y < mi(p, W_VISIBLE + 4))
            y = mi(p, W_VISIBLE + 4);
    } else {
        if (width(self) + x < 0)
            x = 0;
        if (x > vc<int>(display(), 45) - 1) {
            const int ww = width(self);
            x = vc<int>(display(), 45) - ww;
        }
        if (height(self) + y < 0)
            y = 0;
        if (y > vc<int>(display(), 39) - 1) {
            const int hh = height(self);
            y = vc<int>(display(), 39) - hh;
        }
    }
    if (x != mi(self, W_RECT) || y != mi(self, W_RECT + 4))
        set_rect(self, x, y, width(self) + x, height(self) + y);
    if (show != mb(self, W_SHOWN))
        vc(self, 26, show, 1);
    if (BYTE *p = bp(m(self, W_PARENT))) {
        while (vc(p, 64, self + W_SCREEN, self)) {
            const int l = mi(self, W_RECT) + 10, t = mi(self, W_RECT + 4) + 10;
            set_rect(self, l, t, width(self) + l, height(self) + t);
            p = bp(m(self, W_PARENT));
        }
    }
}

// Slot 71: (name, active) the active (+0xe0) or inactive (+0xe4) texture loaded anew: the old one deleted (slot 1),
// a new CFrame (0x40 bytes), loaded from "Interface" with a frame filter of kind 0xb (8-bit display) or 0xc; the
// game ends with a message when the file is missing
void THISCALL Window_LoadTexture(BYTE *self, DWORD name, DWORD active)   // 0x40ea30
{
    DWORD *slot = reinterpret_cast<DWORD *>(self + ((BYTE)active ? W_TEXTURE : W_TEXTURE_INACTIVE));
    if (BYTE *old = bp(*slot)) {
        vc(old, 1, 1);
        *slot = 0;
    }
    BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
    *slot = f ? zax::fn<DWORD(THISCALL *)(BYTE *)>(0x4a8a90)(f) : 0;
    BYTE filter[0x14];
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b1cc0)(filter, 0);
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b1db0)(filter, m(bp(gd(glob_addr(G_REPORTER))), 0x24) == 8 ? 0xb : 0xc);
    using Load = bool(THISCALL *)(DWORD, const char *, DWORD, BYTE *, DWORD, DWORD);
    if (!zax::fn<Load>(0x4a9a30)(*slot, glob<const char *>(0x60a630), name, filter, 1, 0)) {
        char text[0x80];
        zax::fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x60a608), name);
        zax::fn<void(__fastcall *)(const char *, char *)>(0x4d24c0)(glob<const char *>(0x60a5f0), text);
        zax::fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    zax::fn<void(THISCALL *)(BYTE *)>(0x4b1d20)(filter);
}

// Slot 74, the title: (frame, x, y, align) the title in the window's font at (x, y), centred (1) or right
// aligned (2), the accelerator underlined (frame slot 28 in colour +0xa0 at the opacity of Transparent Mask)
// when the window is enabled; a disabled window (or one in a disabled parent) is drawn at the opacities 0.4
// (Transparent Mask) and 0.6 (Black Bias), put back after. The titles "/\", "\/" and "><" are drawn as an up
// arrow, a down arrow (frame slot 55 points in colour +0xa0) and a cross (slot 51 lines).
void THISCALL Window_DrawTitle(BYTE *self, BYTE *frame, int x, int y, int align)   // 0x40eb40
{
    BYTE *p = bp(m(self, W_PARENT));
    const bool enabled = (p && mb(self, W_ENABLED)) ? mb(p, W_ENABLED) != 0 : mb(self, W_ENABLED) != 0;
    float mask = 1.0f, bias = 1.0f;
    if (!enabled) {
        mask = opacity(glob_addr(G_MASK));
        bias = opacity(glob_addr(G_MASK));
        set_opacity(glob_addr(G_MASK), 0x3ecccccd);
        set_opacity(glob_addr(G_BLACK_BIAS), 0x3f19999a);
    }
    BYTE *title = self + W_TITLE;
    auto same = [](DWORD text, DWORD lit) {
        const BYTE *a = bp(text), *b = bp(lit);
        for (;; a += 2, b += 2) {
            if (a[0] != b[0])
                return false;
            if (!a[0])
                return true;
            if (a[1] != b[1])
                return false;
            if (!a[1])
                return true;
        }
    };
    if (not_empty(title)) {
        BYTE *color = self + 0xa0;
        if (same(c_str(title), glob_addr(0x60a644))) {   // "/\"
            for (int k = -2; k <= 1; k++)
                vc(frame, 55, x, y + k, color);
            vc(frame, 55, x - 1, y - 1, color);
            vc(frame, 55, x + 1, y - 1, color);
        } else if (same(c_str(title), glob_addr(0x60a640))) {   // "\/"
            for (int k = -2; k <= 1; k++)
                vc(frame, 55, x, y + k, color);
            vc(frame, 55, x - 1, y, color);
            vc(frame, 55, x + 1, y, color);
        } else if (same(c_str(title), glob_addr(0x60a63c))) {   // "><"
            vc(frame, 51, x - 3, y - 2, x + 1, y + 2, color);
            vc(frame, 51, x + 1, y - 2, x - 3, y + 2, color);
        } else {
            using Width = DWORD(THISCALL *)(DWORD, DWORD, int);
            BYTE *font = bp(m(self, W_FONT));
            int left = x;
            if (align == 1)
                left = x - (int)(zax::fn<Width>(0x4a7310)(pv(font), c_str(title), -1) >> 1);
            else if (align == 2)
                left = x - (int)zax::fn<Width>(0x4a7310)(pv(font), c_str(title), -1);
            if (mb(self, W_ACCEL_INDEX) != 0xff && enabled) {
                const int l = (int)zax::fn<Width>(0x4a7310)(pv(font), c_str(title), mb(self, W_ACCEL_INDEX)) + left + 1;
                const int lh = zax::fn<int(THISCALL *)(BYTE *)>(0x4a6140)(font);
                DWORD t = (DWORD)((signed char)mb(self, W_BORDER3) / 2 + y + lh);
                const DWORD w = zax::fn<Width>(0x4a7310)(m(self, W_FONT), c_str(title) + mb(self, W_ACCEL_INDEX), mb(self, W_ACCEL_WIDTH));
                const DWORD hh = (DWORD)height(self);
                if (t >= hh)
                    t = hh - 1;
                int r[4] = {l, (int)t, (int)(w - 2) + l, (int)t + 1};
                const DWORD c = m(self, 0xa0);
                BYTE fc[0x10];
                zax::fn<void(THISCALL *)(BYTE *)>(0x53f000)(fc);
                const BYTE alpha = (BYTE)x87_ftol((long double)opacity(glob_addr(G_MASK)) * (long double)*glob<float *>(0x5eb728));
                zax::fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(fc, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, alpha);
                vc(frame, 28, r, fc);
            }
            using Draw = void(THISCALL *)(DWORD, BYTE *, int, int, DWORD, DWORD, DWORD);
            zax::fn<Draw>(0x4a6470)(m(self, W_FONT), frame, left, y, c_str(title), 0, 0);
        }
    }
    if (!enabled) {
        set_opacity(glob_addr(G_MASK), float_bits(mask));
        set_opacity(glob_addr(G_BLACK_BIAS), float_bits(bias));
    }
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_c3_window)
{
    const Hook list[] = {
#include "ui_c3_window_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
