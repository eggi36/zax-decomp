// The window base class of the game's own window toolkit, CJWindow (vtable 0x5eb404, 0x100 bytes; layout in
// ui_window.h): the child list, the focus, the dispatch of mouse and keyboard to the children, the
// accelerators of the titles, the rectangles and the layout helpers, the scroll info, the INI position and
// the cursor table (docs/DECOMPILATION.md 9, "The window toolkit").
//
// Every call to another function of the toolkit goes through its exe address, so the original's side of a
// check runs the original all the way down. Return values are what the original leaves in eax where it
// defines it; functions that set al only (a bool) leave the upper bytes of whatever the last call returned
// and say so.
//
// Left out here: the drawing of the children (slot 4, 0x40b620; its dirty-rect cache and the frame
// helpers, Rendering), the first-time loading of the window textures and sounds in the init 0x40a9b0 (done
// once at start), the modal loop 0x40f5f0 (it pumps the Windows messages: phase 5).
#include "ui_window.h"

namespace zax::ui {

namespace {

BYTE *root_of(BYTE *w)
{
    while (BYTE *p = bp(m(w, W_PARENT)))
        w = p;
    return w;
}
// The screen window of the tree: the root when its slot 39 says it is one, else null (neg al; sbb; and)
BYTE *screen_of(BYTE *w)
{
    BYTE *r = root_of(w);
    return (vc(r, V_IS_SCREEN) & 0xff) ? r : nullptr;
}
bool in_rect(const int *p, const BYTE *w, DWORD off)
{
    const int *r = reinterpret_cast<const int *>(w + off);
    return p[0] >= r[0] && p[1] >= r[1] && p[0] < r[2] && p[1] < r[3];
}

}  // namespace

// ---- Small slots ------------------------------------------------------------------------------------------------

// Slot 1: the look
DWORD THISCALL Window_SetLook(BYTE *self, DWORD look)   // 0x40b5c0
{
    m(self, W_LOOK) = look;
    return look;
}

// Slot 2: every child's update, then the scroll bars' when the scroll info shows them (slot 75)
DWORD THISCALL Window_Update(BYTE *self, DWORD dt)   // 0x40b5d0
{
    DWORD r = pv(self);
    for (BYTE *c = bp(m(self, W_FIRST)); c;) {
        BYTE *next = bp(m(c, W_NEXT));
        r = vc(c, V_UPDATE, dt);
        c = next;
    }
    BYTE *s = bp(m(self, W_SCROLL));
    if (s) {
        r = pv(s);
        if (mb(s, S_SHOW))
            r = vc(self, V_UPDATE_SCROLL);
    }
    return r;
}

// The dirty-rect cache (+0x74 of a window): +0x20 the dirty rectangle (all zero: none), +0x30 its window. A
// rectangle that shows in the window's visible rect is added to it (the union) and the toolkit's redraw
// counter 0x60a528 set to 4.
DWORD THISCALL WindowCache_AddDirty(BYTE *self, const int *r)   // 0x40bac0
{
    const BYTE *w = bp(m(self, 0x30));
    const int *v = reinterpret_cast<const int *>(w + W_VISIBLE);
    const int l = r[0] > v[0] ? r[0] : v[0], t = r[1] > v[1] ? r[1] : v[1];
    const int rt = r[2] < v[2] ? r[2] : v[2], b = r[3] < v[3] ? r[3] : v[3];
    if (l >= rt || t >= b)
        return (DWORD)b;
    gd(glob_addr(G_DIRTY)) = 4;
    int *d = &mi(self, 0x20);
    if (!d[0] && !d[2] && !d[1] && !d[3]) {
        d[0] = r[0], d[1] = r[1], d[2] = r[2], d[3] = r[3];
        return (DWORD)r[3];
    }
    d[0] = d[0] < r[0] ? d[0] : r[0];
    d[1] = d[1] < r[1] ? d[1] : r[1];
    d[2] = d[2] > r[2] ? d[2] : r[2];
    d[3] = d[3] > r[3] ? d[3] : r[3];
    return (DWORD)d[3];
}

// Slot 5: slot 6, then true (al; the upper bytes from slot 6)
DWORD THISCALL Window_Close(BYTE *self)   // 0x40bb90
{
    return (vc(self, V_ON_CLOSE) & ~0xffu) | 1;
}

// Slot 7: closes the last child until there is none (true) or one refuses (false); al only
DWORD THISCALL Window_CloseChildren(BYTE *self)   // 0x40bbb0
{
    while (BYTE *c = bp(m(self, W_LAST)))
        if (!(vc(c, V_CLOSE) & 0xff))
            return 0;
    return 1;
}

// Slot 9: the children in order: one that shows and is enabled gets the call when the point is in its
// visible rect, or when it is the first child and modal; a modal child stops the search. al only.
DWORD THISCALL Window_Slot9(BYTE *self, DWORD a1, const int *p, DWORD a3)   // 0x40bc30
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (mb(c, W_SHOWN) && mb(c, W_ENABLED)) {
            bool hit = in_rect(p, c, W_VISIBLE);
            if (!hit && c == bp(m(self, W_FIRST)) && mb(c, W_MODAL))
                hit = true;
            else if (!hit && c == bp(m(self, W_FIRST)))
                continue;   // the first child, not modal: next (the original jumps past the modal test)
            if (hit && (vc(c, V_MOUSE_9, a1, p, a3) & 0xff))
                return 1;
        }
        if (mb(c, W_MODAL))
            return 0;
    }
    return 0;
}

// Slot 10, a button pressed: the children as in slot 9 (the hit one becomes the focus once, slot 34, unless it
// is the first child already); none took it: the window keeps the press position, asks slot 14 (the point,
// an offset {0, 0}, the button's bit, the cursor out) whether it starts a drag and then captures the button
// (bit in +0x67) and sets the cursor slot 14 named.
DWORD THISCALL Window_MouseDown(BYTE *self, DWORD button, const int *p)   // 0x40bcc0
{
    bool focused = false;
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (mb(c, W_SHOWN) && mb(c, W_ENABLED)) {
            bool hit = in_rect(p, c, W_VISIBLE);
            if (!hit) {
                if (c != bp(m(self, W_FIRST)))
                    goto next_modal;
                if (!mb(c, W_MODAL))
                    continue;
                hit = true;
            }
            if (c != bp(m(self, W_FIRST)) && !focused) {
                vc(self, V_FOCUS, c);
                focused = true;
            }
            if (vc(c, V_MOUSE_DOWN, button, p))
                return 1;
        }
    next_modal:
        if (mb(c, W_MODAL))
            break;
    }
    mi(self, W_PRESS_X) = p[0];
    mi(self, W_PRESS_Y) = p[1];
    DWORD offset[2] = {0, 0};
    DWORD cursor = 0;
    m(self, W_70) = 0;
    if (!(vc(self, V_MOUSE_DRAG, p, offset, 1u << (button & 31), &cursor) & 0xff))
        return 0;
    mb(self, W_CAPTURE) |= (BYTE)(1u << (button & 31));
    if (cursor && cursor != gd(glob_addr(G_CURSOR) + 4))
        fn<void(THISCALL *)(void *, DWORD)>(0x518590)(glob<void *>(G_CURSOR), cursor);
    return 1;
}

// Slot 11, a button released: a captured button is released (true); else the children in order until one
// takes it or a modal one stops the search
DWORD THISCALL Window_MouseUp(BYTE *self, DWORD button, DWORD a2)   // 0x40bde0
{
    const BYTE bit = (BYTE)(1u << (button & 31));
    if (mb(self, W_CAPTURE) & bit) {
        mb(self, W_CAPTURE) &= (BYTE)~bit;
        return 1;
    }
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (vc(c, V_MOUSE_UP, button, a2))
            return 1;
        if (mb(c, W_MODAL))
            break;
    }
    return 0;
}

// The key repeat of the screen window (0x409330) for a key of this window
DWORD THISCALL Window_StartKeyRepeat(BYTE *self, DWORD delay, DWORD repeat)   // 0x40be70
{
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x409330)(screen_of(self), self, delay, repeat);
}

// Whether the screen window's key repeat is due (0x4093c0)
DWORD THISCALL Window_KeyRepeatDue(BYTE *self)   // 0x40bec0
{
    return fn<DWORD(THISCALL *)(BYTE *)>(0x4093c0)(screen_of(self));
}

// The texture list and its owner the windows take "Window Texture" etc. from (fastcall)
void __fastcall Window_SetTextureList(DWORD owner, DWORD list)   // 0x40bf00
{
    gd(glob_addr(0x62e5c8)) = owner;
    gd(glob_addr(0x62e5cc)) = list;
}

// Slot 15: the children as in slot 9, but a child also counts when it is active; then the scroll bars of the
// scroll info (when shown). al only.
DWORD THISCALL Window_Slot15(BYTE *self, DWORD a1, const int *p)   // 0x40c0b0
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (mb(c, W_SHOWN) && mb(c, W_ENABLED) && (in_rect(p, c, W_VISIBLE) || mb(c, W_ACTIVE)) &&
            (vc(c, V_MOUSE_15, a1, p) & 0xff))
            return 1;
        if (mb(c, W_MODAL))
            break;
    }
    BYTE *s = bp(m(self, W_SCROLL));
    if (!s || !mb(s, S_SHOW))
        return 0;
    if (BYTE *b = bp(m(s, S_VBAR)))
        if (vc(b, V_MOUSE_15, a1, p) & 0xff)
            return 1;
    if (BYTE *b = bp(m(bp(m(self, W_SCROLL)), S_HBAR)))
        if (vc(b, V_MOUSE_15, a1, p) & 0xff)
            return 1;
    return 0;
}

// Slot 16, a key: the active child first; Tab (9) moves the focus to the next (Shift: previous) child that
// takes it, Escape (0x1b) gives up a mouse capture (the cursor back to the press position, the screen
// window's mouse update)
DWORD THISCALL Window_Key(BYTE *self, DWORD key)   // 0x40c170
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (mb(c, W_ACTIVE) && mb(c, W_SHOWN) && mb(c, W_ENABLED) && vc(c, V_KEY, key))
            return 1;
        if (mb(c, W_MODAL))
            break;
    }
    if (key == 9) {
        if (m(self, W_FIRST) == m(self, W_LAST))
            return 0;
        const bool back = key_down(0x2a) || key_down(0x36);
        if (!m(self, W_FIRST))
            return 0;
        gb(glob_addr(G_FOCUS_BY_KEY)) = 1;
        BYTE *f = bp(vc(bp(m(self, W_FIRST)), back ? V_PREV_FOCUS : V_NEXT_FOCUS));
        if (!f)
            return 0;
        vc(self, V_FOCUS, f);
        return 1;
    }
    if (key == 0x1b && mb(self, W_CAPTURE)) {
        fn<void(THISCALL *)(void *, int, int)>(0x518570)(glob<void *>(G_CURSOR), mi(self, W_PRESS_X), mi(self, W_PRESS_Y));
        fn<void(THISCALL *)(BYTE *)>(0x409b30)(screen_of(self));
        mb(self, W_CAPTURE) = 0;
        return 1;
    }
    return 0;
}

// Slot 17, a character: a child with children of its own first; then the child whose accelerator it is (a
// child that does not take the focus itself passes it to the next one that does); an enabled one gets the
// focus (when slot 28 allows) and its slot 18; a disabled one plays "Player Error.wav"
DWORD THISCALL Window_Char(BYTE *self, DWORD ch)   // 0x40c310
{
    bool found = false;
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT))) {
        if (m(c, W_FIRST) && vc(c, V_CHAR, ch))
            return 1;
        if (mb(c, W_ACCEL) && mb(c, W_ACCEL) == (BYTE)fn<int(__cdecl *)(int)>(0x5d0c4e)((int)ch)) {
            if (!mb(c, W_TAB_STOP) && !mb(c, W_5E))
                if (BYTE *n = bp(vc(c, V_NEXT_FOCUS)))
                    c = n;
            found = true;
            if (mb(c, W_ENABLED)) {
                if (vc(self, V_28))
                    vc(self, V_FOCUS, c);
                vc(c, V_ACTIVATE);
                return 1;
            }
        }
        if (mb(c, W_MODAL))
            break;
    }
    if (!found)
        return 0;
    play_sound(glob_addr(SOUND_ERROR));
    return 1;
}

// Slot 19: active or not, and the same for the first child (slot 19)
DWORD THISCALL Window_SetActive(BYTE *self, DWORD on)   // 0x40c3e0
{
    mb(self, W_ACTIVE) = (BYTE)on;
    if (BYTE *c = bp(m(self, W_FIRST)))
        return vc(c, V_SET_ACTIVE, on);
    return on;
}

// Slot 21: a notification goes up to the parent's slot 21, except code 2
DWORD THISCALL Window_Notify(BYTE *self, DWORD from, DWORD code)   // 0x40c400
{
    BYTE *p = bp(m(self, W_PARENT));
    if (!p || code == 2)
        return pv(p) ? code : 0;
    return vc(p, V_NOTIFY, from, code);
}

// Transparent or not, the whole subtree
void THISCALL Window_SetTransparent(BYTE *self, DWORD on)   // 0x40c600
{
    mb(self, W_TRANSPARENT) = (BYTE)on;
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x40c600)(c, on);
}

// Enabled, and its parent too (only the direct parent counts); with a parent the upper bytes of eax are the
// parent's address
DWORD THISCALL Window_IsEnabled(BYTE *self)   // 0x40c630
{
    BYTE *p = bp(m(self, W_PARENT));
    if (p && mb(self, W_ENABLED))
        return (pv(p) & ~0xffu) | (mb(p, W_ENABLED) ? 1u : 0u);
    return mb(self, W_ENABLED) ? 1 : 0;
}

// A window to the parent's (else this window's) front (slot 32) with a placing code
DWORD THISCALL Window_AddToParentFront(BYTE *self, BYTE *w, DWORD place)   // 0x40c680
{
    BYTE *target = bp(m(self, W_PARENT));
    return vc(target ? target : self, V_ADD_CHILD_FRONT, w, place, 0, (DWORD)-1000, (DWORD)-1000, 0);
}

// The same for a modal window (its +0x64 set first)
DWORD THISCALL Window_AddModalToParent(BYTE *self, BYTE *w, DWORD place)   // 0x40c6e0
{
    mb(w, W_MODAL) = 1;
    BYTE *target = bp(m(self, W_PARENT));
    return vc(target ? target : self, V_ADD_CHILD_FRONT, w, place, 0, (DWORD)-1000, (DWORD)-1000, 0);
}

// A message box (0x403b30, 0x11c bytes) with a text and a caption, modal, centred in front of the parent
DWORD THISCALL Window_MessageBox(BYTE *self, DWORD text, DWORD caption)   // 0x40c740
{
    BYTE *box = static_cast<BYTE *>(fn<void *(__fastcall *)(DWORD)>(0x417710)(0x11c));
    if (box)
        box = fn<BYTE *(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x403b30)(box, text, caption, 0);
    mb(box, W_MODAL) = 1;
    BYTE *target = bp(m(self, W_PARENT));
    return vc(target ? target : self, V_ADD_CHILD_FRONT, box, 6, 0, (DWORD)-1000, (DWORD)-1000, 0);
}

// ---- The child list -----------------------------------------------------------------------------------------

namespace {

// A child that takes the focus but is not shown, or does not take it, gives the focus away to the next one
// (slot 36 with the "by key" flag set for the call)
void pass_focus(BYTE *child)
{
    BYTE *f = bp(m(child, W_FIRST));
    if (!f || (mb(f, W_TAB_STOP) && mb(f, W_SHOWN)))
        return;
    const BYTE saved = gb(glob_addr(G_FOCUS_BY_KEY));
    gb(glob_addr(G_FOCUS_BY_KEY)) = 1;
    if (BYTE *n = bp(vc(bp(m(child, W_FIRST)), V_NEXT_FOCUS)))
        vc(child, V_FOCUS, n);
    gb(glob_addr(G_FOCUS_BY_KEY)) = saved;
}

}  // namespace

// Slot 31: a child at the end of the list; with a2 its slot 62 places it (a2..a5 and the flags), else slot
// 57 takes its own rect; the parent's slot 63 fits it in (flag 1: not; flag 4: 0 instead of -1000); the
// first child of an active window becomes active
DWORD THISCALL Window_AddChild(BYTE *self, BYTE *child, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD flags)   // 0x40c7c0
{
    pass_focus(child);
    const bool first = m(self, W_FIRST) == 0;
    m(child, W_PARENT) = pv(self);
    if (BYTE *last = bp(m(self, W_LAST)))
        m(last, W_NEXT) = pv(child);
    else
        m(self, W_FIRST) = pv(child);
    m(child, W_PREV) = m(self, W_LAST);
    m(self, W_LAST) = pv(child);
    DWORD r = a2 ? vc(child, V_PLACE, a2, a3, a4, a5, flags) : vc(child, V_SET_RECT, child + W_RECT);
    if (!(flags & 1))
        r = vc(self, V_FIT_CHILD, child, (flags & 4) ? 0u : (DWORD)-1000);
    if (first && mb(self, W_ACTIVE))
        r = vc(child, V_SET_ACTIVE, 1);
    return r;
}

// Slot 32: the same at the front of the list (no fitting; an active window passes the focus to it)
DWORD THISCALL Window_AddChildFront(BYTE *self, BYTE *child, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6)   // 0x40c8d0
{
    pass_focus(child);
    m(child, W_PARENT) = pv(self);
    if (BYTE *first = bp(m(self, W_FIRST)))
        m(first, W_PREV) = pv(child);
    else
        m(self, W_LAST) = pv(child);
    m(child, W_NEXT) = m(self, W_FIRST);
    m(self, W_FIRST) = pv(child);
    DWORD r = a2 ? vc(child, V_PLACE, a2, a3, a4, a5, a6) : vc(child, V_SET_RECT, child + W_RECT);
    if (mb(self, W_ACTIVE)) {
        if (BYTE *n = bp(m(child, W_NEXT)))
            vc(n, V_SET_ACTIVE, 0);
        r = vc(child, V_SET_ACTIVE, 1);
    }
    return r;
}

// Slot 33: a child out of the list (an active one gives the focus to the new first child)
DWORD THISCALL Window_RemoveChild(BYTE *self, BYTE *child)   // 0x40c9b0
{
    if (BYTE *prev = bp(m(child, W_PREV)))
        m(prev, W_NEXT) = m(child, W_NEXT);
    else
        m(self, W_FIRST) = m(child, W_NEXT);
    if (BYTE *next = bp(m(child, W_NEXT)))
        m(next, W_PREV) = m(child, W_PREV);
    else
        m(self, W_LAST) = m(child, W_PREV);
    const BYTE active = mb(child, W_ACTIVE);
    m(child, W_NEXT) = 0;
    m(child, W_PREV) = 0;
    m(child, W_PARENT) = 0;
    if (active)
        if (BYTE *f = bp(m(self, W_FIRST)))
            return vc(f, V_SET_ACTIVE, 1);
    return pv(child);
}

// Slot 34: a child gets the focus (unless it is active already): an inactive window first asks its parent
// for the focus itself; the child goes to the front of the list (the list is rotated: the children before it
// go to the end), the old first child loses its activity, the new one gets it. eax undefined (void).
void THISCALL Window_Focus(BYTE *self, BYTE *child)   // 0x40ca40
{
    if (mb(child, W_ACTIVE))
        return;
    if (!mb(self, W_ACTIVE))
        if (BYTE *p = bp(m(self, W_PARENT)))
            vc(p, V_FOCUS, self);
    BYTE *first = bp(m(self, W_FIRST));
    if (first == child)
        return;
    if (mb(self, W_ACTIVE))
        vc(first, V_SET_ACTIVE, 0);
    m(bp(m(self, W_FIRST)), W_PREV) = m(self, W_LAST);
    m(bp(m(self, W_LAST)), W_NEXT) = m(self, W_FIRST);
    BYTE *before = bp(m(child, W_PREV));
    m(self, W_LAST) = pv(before);
    m(before, W_NEXT) = 0;
    m(self, W_FIRST) = pv(child);
    m(child, W_PREV) = 0;
    if (mb(self, W_ACTIVE))
        vc(bp(m(self, W_FIRST)), V_SET_ACTIVE, 1);
}

// Slot 35: a child to the end of the list (out with slot 33, in with slot 31 unplaced and unfitted)
DWORD THISCALL Window_ToBack(BYTE *self, BYTE *child)   // 0x40caf0
{
    if (m(self, W_FIRST) == m(self, W_LAST))
        return m(self, W_FIRST);
    vc(self, V_REMOVE_CHILD, child);
    return vc(self, V_ADD_CHILD, child, 0, 0, (DWORD)-1000, (DWORD)-1000, 1);
}

// The focus to the next (previous) child that takes it; al
DWORD THISCALL Window_FocusNext(BYTE *self)   // 0x40cb30
{
    DWORD r = m(self, W_FIRST);
    if (!r)
        return 0;
    gb(glob_addr(G_FOCUS_BY_KEY)) = 1;
    r = vc(bp(m(self, W_FIRST)), V_NEXT_FOCUS);
    if (!r)
        return 0;
    return (vc(self, V_FOCUS, bp(r)) & ~0xffu) | 1;
}

DWORD THISCALL Window_FocusPrev(BYTE *self)   // 0x40cb70
{
    DWORD r = m(self, W_FIRST);
    if (!r)
        return 0;
    gb(glob_addr(G_FOCUS_BY_KEY)) = 1;
    r = vc(bp(m(self, W_FIRST)), V_PREV_FOCUS);
    if (!r)
        return 0;
    return (vc(self, V_FOCUS, bp(r)) & ~0xffu) | 1;
}

// The root of the tree
BYTE *THISCALL Window_Root(BYTE *self)   // 0x40cc70
{
    return root_of(self);
}

// The child with this title (ZString compare), or null
BYTE *THISCALL Window_FindChild(BYTE *self, const char *title)   // 0x40cc90
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        if (!fn<int(THISCALL *)(void *, const char *)>(0x4e1770)(c + W_TITLE, title))
            return c;
    return nullptr;
}

// The screen window of the tree, or null
BYTE *THISCALL Window_Screen(BYTE *self)   // 0x40ccd0
{
    return screen_of(self);
}

// A tool tip through the screen window (0x4098d0 / 0x4098f0) in this window's font
void THISCALL Window_ShowTip(BYTE *self, DWORD text, DWORD x, DWORD y)   // 0x40cd00
{
    BYTE *r = root_of(self);
    if ((vc(r, V_IS_SCREEN) & 0xff) && r)
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x4098d0)(r, m(self, W_FONT), text, x, y);
}

void THISCALL Window_ShowTipAccel(BYTE *self, DWORD accel, DWORD text, DWORD x, DWORD y)   // 0x40cd50
{
    BYTE *r = root_of(self);
    if ((vc(r, V_IS_SCREEN) & 0xff) && r)
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4098f0)(r, m(self, W_FONT), accel, text, x, y);
}

// Slot 42: no accelerator; al 0
DWORD THISCALL Window_ClearAccel(BYTE *self)   // 0x40cdb0
{
    mb(self, W_ACCEL_INDEX) = 0xff;
    mb(self, W_ACCEL_WIDTH) = 0;
    mb(self, W_ACCEL) = 0;
    return 0;
}


// ---- The title and its accelerator ----------------------------------------------------------------------------

namespace {

const char *zs_cstr(BYTE *z) { return fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(z); }
// strcmp as the original inlines it (unsigned bytes; 0 when equal)
int lstrcmpA_exact(const char *a, const char *b)
{
    for (;; a++, b++) {
        const BYTE x = (BYTE)*a, y = (BYTE)*b;
        if (x != y)
            return x < y ? -1 : 1;
        if (!x)
            return 0;
    }
}
// The text from p on moves one character to the left (rep movs forward, as the original)
void remove_char(char *p)
{
    do
        p[0] = p[1];
    while (*p++);
}

}  // namespace

// Slot 43: the title; with accelerators on, the first '&' marks the accelerator (removed; a following
// character 1 is removed too), the index of the '&' in +0x59; a second '&' (the last one) closes an
// underlined range: its distance in +0x5a, removed, no accelerator key; else +0x5a is 1 and the key the
// upper-case character at the index
void THISCALL Window_SetTitle(BYTE *self, const char *text)   // 0x40cdc0
{
    BYTE *title = self + W_TITLE;
    fn<void(THISCALL *)(BYTE *)>(0x4df2b0)(title);
    mb(self, W_ACCEL_INDEX) = 0xff;
    mb(self, W_ACCEL_WIDTH) = 0;
    mb(self, W_ACCEL) = 0;
    if (!text)
        return;
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(title, text);
    if (!mb(self, W_ACCEL_ON))
        return;
    char *amp = fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(zs_cstr(title), '&');
    if (!amp)
        return;
    mb(self, W_ACCEL_INDEX) = (BYTE)((BYTE)pv(amp) - (BYTE)pv(zs_cstr(title)));
    remove_char(amp);
    if (*amp == 1)
        remove_char(amp);
    char *last = fn<char *(__cdecl *)(const char *, int)>(0x5d0d90)(zs_cstr(title), '&');
    if (last) {
        mb(self, W_ACCEL_WIDTH) = (BYTE)((BYTE)pv(last) - (BYTE)pv(amp));
        remove_char(last);
        mb(self, W_ACCEL) = 0;
        return;
    }
    mb(self, W_ACCEL_WIDTH) = 1;
    const char c = fn<char(THISCALL *)(BYTE *, DWORD)>(0x4e1aa0)(title, mb(self, W_ACCEL_INDEX));
    mb(self, W_ACCEL) = (BYTE)fn<int(__cdecl *)(int)>(0x5d0c4e)((int)c);
}

// A one-character title (slot 43 with the character and a terminator)
DWORD THISCALL Window_SetTitleChar(BYTE *self, char c)   // 0x40cee0
{
    char text[2] = {c, 0};
    return vc(self, V_SET_TITLE, text);
}

// Slot 53: the colour at +0xa0 (a FieldColor; bytes 7 and 0xb are not copied)
DWORD THISCALL Window_SetColorA0(BYTE *self, const BYTE *color)   // 0x40cfe0
{
    m(self, 0xa0) = m(const_cast<BYTE *>(color), 0);
    m<WORD>(self, 0xa4) = m<WORD>(const_cast<BYTE *>(color), 4);
    mb(self, 0xa6) = color[6];
    m<WORD>(self, 0xa8) = m<WORD>(const_cast<BYTE *>(color), 8);
    mb(self, 0xaa) = color[0xa];
    return m(self, 0xac) = m(const_cast<BYTE *>(color), 0xc);
}

// The shared border (+0x7a and 0x60a52c): font height / 5 times n (a byte multiplication, as imul byte)
DWORD THISCALL Window_SetBorder(BYTE *self, char n)   // 0x40d0c0
{
    const DWORD fifth = m(bp(m(self, W_FONT)), 0x80) / 5;
    const WORD product = (WORD)((short)(signed char)(BYTE)fifth * (short)n);
    mb(self, W_BORDER3) = (BYTE)product;
    gb(glob_addr(G_BORDER)) = (BYTE)product;
    return (fifth & 0xffff0000u) | product;
}

// The right edge of another window relative to this one's left
int THISCALL Window_RightOf(BYTE *self, BYTE *other)   // 0x40d0f0
{
    return mi(other, W_RECT + 8) - mi(self, W_RECT);
}

// The siblings from this window to `last` share `extra` more width (each gets total / count, the last the
// rest), moved right one after another (slot 57, slot 59); nothing when the total is not positive
DWORD THISCALL Window_SpreadWidth(BYTE *self, BYTE *last, int extra)   // 0x40d100
{
    const int total = mi(self, W_RECT) - mi(last, W_RECT + 8) + extra;
    if (total <= 0)
        return (DWORD)total;
    int count = 1;
    for (BYTE *w = self; w != last; w = bp(m(w, W_NEXT)))
        count++;
    const int each = total / count;
    int offset = 0;
    BYTE *w = self;
    for (; w != last; w = bp(m(w, W_NEXT)), offset += each) {
        if (offset) {
            int r[4];
            r[0] = offset + mi(w, W_RECT);
            r[1] = mi(w, W_RECT + 4);
            r[2] = mi(w, W_RECT + 8) - mi(w, W_RECT) + r[0];
            r[3] = mi(w, W_RECT + 12) - r[1] + r[1];
            vc(w, V_SET_RECT, r);
        }
        vc(w, V_SET_SIZE, mi(w, W_RECT + 8) - mi(w, W_RECT) + each, mi(w, W_RECT + 12) - mi(w, W_RECT + 4));
    }
    int r[4];
    r[0] = offset + mi(w, W_RECT);
    r[1] = mi(w, W_RECT + 4);
    r[2] = mi(w, W_RECT + 8) - mi(w, W_RECT) + r[0];
    r[3] = mi(w, W_RECT + 12) - r[1] + r[1];
    vc(w, V_SET_RECT, r);
    return vc(w, V_SET_SIZE, mi(w, W_RECT + 8) - (count - 1) * each - mi(w, W_RECT) + total,
              mi(w, W_RECT + 12) - mi(w, W_RECT + 4));
}

// The siblings from this window to `last` all get the widest one's width (unsigned compare)
DWORD THISCALL Window_SameWidth(BYTE *self, BYTE *last)   // 0x40d230
{
    DWORD widest = m(last, W_RECT + 8) - m(last, W_RECT);
    for (BYTE *w = self; w != last; w = bp(m(w, W_NEXT)))
        if (m(w, W_RECT + 8) - m(w, W_RECT) > widest)
            widest = m(w, W_RECT + 8) - m(w, W_RECT);
    DWORD r = vc(last, V_SET_SIZE, widest, mi(last, W_RECT + 12) - mi(last, W_RECT + 4));
    for (BYTE *w = self; w != last; w = bp(m(w, W_NEXT)))
        r = vc(w, V_SET_SIZE, widest, mi(w, W_RECT + 12) - mi(w, W_RECT + 4));
    return r;
}

// ---- Rectangles -----------------------------------------------------------------------------------------------

// Slot 57: the rect; the content size (the scroll info's when set, else the rect's); the screen rect (moved by
// the parent's screen origin minus its scroll position) and the visible rect (cut to the parent's visible
// rect, all zero when empty); the clip rect (the visible rect in content coordinates); the scroll bars
// (slot 75) and every child's rect again (slot 57)
void THISCALL Window_SetRect(BYTE *self, const int *r)   // 0x40d2a0
{
    int *rect = &mi(self, W_RECT);
    rect[0] = r[0], rect[1] = r[1], rect[2] = r[2], rect[3] = r[3];
    BYTE *s = bp(m(self, W_SCROLL));
    int w = s ? mi(s, S_W) : 0;
    if (!w)
        w = rect[2] - rect[0];
    m(self, W_CONTENT_W) = (DWORD)w;
    int h = s ? mi(s, S_H) : 0;
    if (!h)
        h = rect[3] - rect[1];
    m(self, W_CONTENT_H) = (DWORD)h;
    int *scr = &mi(self, W_SCREEN), *vis = &mi(self, W_VISIBLE);
    for (int i = 0; i < 4; i++)
        scr[i] = rect[i], vis[i] = rect[i];
    if (BYTE *p = bp(m(self, W_PARENT))) {
        BYTE *ps = bp(m(p, W_SCROLL));
        const int sy = ps ? mi(ps, S_Y) : 0, sx = ps ? mi(ps, S_X) : 0;
        const int dy = mi(p, W_SCREEN + 4) - sy, dx = mi(p, W_SCREEN) - sx;
        scr[0] += dx, scr[1] += dy, scr[2] += dx, scr[3] += dy;
        const int *pv_ = &mi(p, W_VISIBLE);
        const int l = pv_[0] > scr[0] ? pv_[0] : scr[0];
        vis[0] = l;
        const int t = pv_[1] > scr[1] ? pv_[1] : scr[1];
        vis[1] = t;
        const int rt = pv_[2] < scr[2] ? pv_[2] : scr[2];
        vis[2] = rt;
        const int b = pv_[3] < scr[3] ? pv_[3] : scr[3];
        vis[3] = b;
        if (l >= rt || t >= b)
            vis[0] = vis[1] = vis[2] = vis[3] = 0;
    }
    int *clip = &mi(self, W_CLIP);
    for (int i = 0; i < 4; i++)
        clip[i] = vis[i];
    BYTE *s2 = bp(m(self, W_SCROLL));
    const int cy = s2 ? mi(s2, S_Y) : 0, cx = s2 ? mi(s2, S_X) : 0;
    const int ox = cx - scr[0], oy = cy - mi(self, W_SCREEN + 4);
    clip[0] += ox, clip[1] += oy, clip[2] += ox, clip[3] += oy;
    if (BYTE *s3 = bp(m(self, W_SCROLL)))
        if (mb(s3, S_SHOW))
            vc(self, V_UPDATE_SCROLL);
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        vc(c, V_SET_RECT, c + W_RECT);
}

// The client width and height: the rect without the scroll bar (15) and the borders (slots 61, 60)
int THISCALL Window_ClientWidth(BYTE *self)   // 0x40d490
{
    BYTE *s = bp(m(self, W_SCROLL));
    const int bar = s && m(s, S_VBAR) ? BAR : 0;
    const int b = (int)vc(self, V_BORDER_W);
    return mi(self, W_RECT + 8) - mi(self, W_RECT) - bar - b;
}

int THISCALL Window_ClientHeight(BYTE *self)   // 0x40d4d0
{
    BYTE *s = bp(m(self, W_SCROLL));
    const int bar = s && m(s, S_HBAR) ? BAR : 0;
    const int b = (int)vc(self, V_BORDER_H);
    return mi(self, W_RECT + 12) - mi(self, W_RECT + 4) - bar - b;
}

// Slot 59: the size (slot 57 with the same left and top)
DWORD THISCALL Window_SetSize(BYTE *self, int w, int h)   // 0x40d510
{
    int r[4] = {mi(self, W_RECT), mi(self, W_RECT + 4), 0, 0};
    r[2] = r[0] + w;
    r[3] = r[1] + h;
    return vc(self, V_SET_RECT, r);
}

namespace {

BYTE *new_scroll(BYTE *self)
{
    BYTE *s = static_cast<BYTE *>(fn<void *(__cdecl *)(DWORD)>(0x5d033a)(SCROLL_SIZE));
    m(self, W_SCROLL) = pv(s);
    m(s, S_X) = 0, m(s, S_Y) = 0;
    s = bp(m(self, W_SCROLL));
    m(s, S_W) = 0, m(s, S_H) = 0;
    mb(bp(m(self, W_SCROLL)), S_SHOW) = 0;
    m(bp(m(self, W_SCROLL)), S_VBAR) = 0;
    m(bp(m(self, W_SCROLL)), S_HBAR) = 0;
    return bp(m(self, W_SCROLL));
}
// The scroll info freed when it holds nothing; whether it was
bool drop_empty_scroll(BYTE *self)
{
    BYTE *s = bp(m(self, W_SCROLL));
    if (m(s, S_X) || m(s, S_Y) || m(s, S_W) || m(s, S_H) || mb(s, S_SHOW) || m(s, S_VBAR) || m(s, S_HBAR) || !s)
        return false;
    fn<void(__cdecl *)(void *)>(0x5d0320)(s);
    m(self, W_SCROLL) = 0;
    return true;
}

}  // namespace

// The content size (the scroll info, made when missing and freed when empty); a change sets the rect again
void THISCALL Window_SetContentSize(BYTE *self, int w, int h)   // 0x40d550
{
    bool changed = false;
    if (!m(self, W_SCROLL))
        new_scroll(self);
    BYTE *s = bp(m(self, W_SCROLL));
    if (mi(s, S_W) != w || mi(s, S_H) != h) {
        mi(s, S_W) = w;
        mi(s, S_H) = h;
        changed = true;
    }
    drop_empty_scroll(self);
    if (changed)
        vc(self, V_SET_RECT, self + W_RECT);
}

// Moving: to (x, y) with the same size, the left only, the top only (slot 57)
DWORD THISCALL Window_MoveTo(BYTE *self, int x, int y)   // 0x40d630
{
    int r[4] = {x, y, mi(self, W_RECT + 8) - mi(self, W_RECT) + x, mi(self, W_RECT + 12) - mi(self, W_RECT + 4) + y};
    return vc(self, V_SET_RECT, r);
}

DWORD THISCALL Window_SetLeft(BYTE *self, int x)   // 0x40d680
{
    int r[4] = {x, mi(self, W_RECT + 4), mi(self, W_RECT + 8) - mi(self, W_RECT) + x, mi(self, W_RECT + 12)};
    return vc(self, V_SET_RECT, r);
}

DWORD THISCALL Window_SetTop(BYTE *self, int y)   // 0x40d6c0
{
    int r[4] = {mi(self, W_RECT), y, mi(self, W_RECT + 8), mi(self, W_RECT + 12) - mi(self, W_RECT + 4) + y};
    return vc(self, V_SET_RECT, r);
}

// The scroll position, kept within the content (when a content size is set) and at least 0; the scroll info
// freed when empty; the rect again (slot 57)
DWORD THISCALL Window_SetScrollPos(BYTE *self, int x, int y)   // 0x40d700
{
    if (!m(self, W_SCROLL))
        new_scroll(self);
    BYTE *s = bp(m(self, W_SCROLL));
    mi(s, S_X) = x;
    mi(s, S_Y) = y;
    if (mi(s, S_W)) {
        BYTE *si = bp(m(self, W_SCROLL));
        int bar = si && m(si, S_VBAR) ? BAR : 0;
        int b = (int)vc(self, V_BORDER_W);
        if (mi(s, S_X) + (mi(self, W_RECT + 8) - mi(self, W_RECT) - bar - b) > mi(s, S_W)) {
            si = bp(m(self, W_SCROLL));
            bar = si && m(si, S_VBAR) ? BAR : 0;
            b = (int)vc(self, V_BORDER_W);
            mi(s, S_X) = mi(s, S_W) - (mi(self, W_RECT + 8) - mi(self, W_RECT) - bar - b);
        }
    }
    if (mi(s, S_H)) {
        BYTE *si = bp(m(self, W_SCROLL));
        int bar = si && m(si, S_HBAR) ? BAR : 0;
        int b = (int)vc(self, V_BORDER_H);
        if (mi(s, S_Y) + (mi(self, W_RECT + 12) - mi(self, W_RECT + 4) - bar - b) > mi(s, S_H)) {
            si = bp(m(self, W_SCROLL));
            bar = si && m(si, S_HBAR) ? BAR : 0;
            b = (int)vc(self, V_BORDER_H);
            mi(s, S_Y) = mi(s, S_H) - (mi(self, W_RECT + 12) - mi(self, W_RECT + 4) - bar - b);
        }
    }
    if (mi(s, S_X) < 0)
        mi(s, S_X) = 0;
    if (mi(s, S_Y) < 0)
        mi(s, S_Y) = 0;
    drop_empty_scroll(self);
    return vc(self, V_SET_RECT, self + W_RECT);
}

namespace {

void set_scroll(BYTE *self, int x, int y) { fn<void(THISCALL *)(BYTE *, int, int)>(0x40d700)(self, x, y); }
int scroll_x(BYTE *self) { BYTE *s = bp(m(self, W_SCROLL)); return s ? mi(s, S_X) : 0; }
int scroll_y(BYTE *self) { BYTE *s = bp(m(self, W_SCROLL)); return s ? mi(s, S_Y) : 0; }

}  // namespace

// One coordinate of the scroll position (unchanged: nothing)
void THISCALL Window_SetScrollX(BYTE *self, int x)   // 0x40d8c0
{
    if (x != scroll_x(self))
        set_scroll(self, x, scroll_y(self));
}

void THISCALL Window_SetScrollY(BYTE *self, int y)   // 0x40d900
{
    if (y != scroll_y(self))
        set_scroll(self, scroll_x(self), y);
}

// Vertical scrolling so that the rect's top and then its bottom show
void THISCALL Window_ScrollToShow(BYTE *self, const int *r)   // 0x40d940
{
    if (r[1] < scroll_y(self) && r[1] != scroll_y(self))
        set_scroll(self, scroll_x(self), r[1]);
    const int y = scroll_y(self);
    if (r[3] > mi(self, W_RECT + 12) - mi(self, W_RECT + 4) + y) {
        const int ny = mi(self, W_RECT + 4) - mi(self, W_RECT + 12) + r[3];
        if (ny != scroll_y(self))
            set_scroll(self, scroll_x(self), ny);
    }
}

// The scroll position moved by (dx, dy)
void THISCALL Window_ScrollBy(BYTE *self, int dx, int dy)   // 0x40d9f0
{
    set_scroll(self, scroll_x(self) + dx, scroll_y(self) + dy);
}

// Scroll bars on or off (the scroll info made when needed; off deletes both bars; freed when empty)
void THISCALL Window_ShowScrollBars(BYTE *self, char on)   // 0x40da30
{
    BYTE *s = bp(m(self, W_SCROLL));
    if (on && s) {
        mb(s, S_SHOW) = (BYTE)on;
        return;
    }
    if (!on && !s)
        return;
    if (!s)
        new_scroll(self);
    s = bp(m(self, W_SCROLL));
    mb(s, S_SHOW) = (BYTE)on;
    if (!on) {
        if (BYTE *b = bp(m(s, S_HBAR))) {
            vc(b, V_DELETE, 1);
            m(s, S_HBAR) = 0;
        }
        if (BYTE *b = bp(m(s, S_VBAR))) {
            vc(b, V_DELETE, 1);
            m(s, S_VBAR) = 0;
        }
    }
    drop_empty_scroll(self);
}

// Slot 63: the window grows to hold a child (its right and bottom plus a border: -1000 the window's own
// +0x78, else n * font height / 5; unsigned compares); when it grew, its parent fits it in turn
void THISCALL Window_FitChild(BYTE *self, BYTE *child, int border)   // 0x40e590
{
    const DWORD b = border == -1000 ? (DWORD)(int)(signed char)mb(self, W_BORDER)
                                    : m(bp(m(self, W_FONT)), 0x80) / 5 * (DWORD)border;
    DWORD h = m(self, W_RECT + 12) - m(self, W_RECT + 4), w = m(self, W_RECT + 8) - m(self, W_RECT);
    bool grow = false;
    const DWORD right = m(child, W_RECT + 8) + b;
    if (right > w) {
        grow = true;
        w = right;
    }
    const DWORD bottom = b + m(child, W_RECT + 12);
    if (bottom > h)
        h = bottom;
    else if (!grow)
        return;
    vc(self, V_SET_SIZE, w, h);
    if (BYTE *p = bp(m(self, W_PARENT)))
        vc(p, V_FIT_CHILD, self, (DWORD)-1000);
}

// Slot 68: position (and with `size` the size), shown and transparent into the game's INI file (0x62e640),
// section `section` or the title
void THISCALL Window_SaveIni(BYTE *self, const char *section, char size)   // 0x40e8f0
{
    if (!section)
        section = zs_cstr(self + W_TITLE);
    auto write = [&](DWORD key, int v) {
        fn<void(__fastcall *)(DWORD, const char *, const char *, int)>(0x4a4690)(gd(glob_addr(0x62e640)), section, at<const char *>(key), v);
    };
    if (size) {
        write(glob_addr(0x60a5e8), mi(self, W_RECT + 8) - mi(self, W_RECT));
        write(glob_addr(0x60a5e0), mi(self, W_RECT + 12) - mi(self, W_RECT + 4));
    }
    write(glob_addr(0x60a5d4), mi(self, W_RECT));
    write(glob_addr(0x60a5c8), mi(self, W_RECT + 4));
    write(glob_addr(0x60a5bc), mb(self, W_SHOWN));
    write(glob_addr(0x60a5a8), mb(self, W_TRANSPARENT));
}

// Whether the windows draw with their textures (byte 0x60a52d clear)
DWORD Window_TexturesOn()   // 0x40e9b0
{
    return gb(glob_addr(0x60a52d)) == 0 ? 1 : 0;
}

// Three globals of the toolkit (fastcall: ecx, edx, one on the stack)
DWORD __fastcall Window_SetGlobals3(DWORD a, DWORD b, DWORD c)   // 0x40e9c0
{
    gd(glob_addr(0x62e5d4)) = a;
    gd(glob_addr(0x62e5d8)) = b;
    gd(glob_addr(0x62e5dc)) = c;
    return c;
}

// The cursor table 0x62e5e4: entry i, or entry 0 when it is empty (fastcall)
DWORD __fastcall CursorEntry(int i)   // 0x40e9e0
{
    const DWORD c = glob<DWORD *>(0x62e5e4)[i];
    return c ? c : gd(glob_addr(0x62e5e4));
}

// Slot 69: the cursor for a point: the default one
DWORD THISCALL Window_Cursor(BYTE *, const int *)   // 0x40ea00
{
    return gd(glob_addr(0x62e5e4));
}

// Slot 70: the parent's (al 0 without a parent)
DWORD THISCALL Window_ParentSlot70(BYTE *self, DWORD a)   // 0x40ea10
{
    BYTE *p = bp(m(self, W_PARENT));
    return p ? vc(p, V_PARENT_CURSOR, a) : 0;
}

// Slot 72: the texture: the window's own (the inactive one when slot 30 says inactive and there is one),
// else the toolkit's "Window Texture" (0x62e5a0) or "Inactive Window Texture" (0x62e59c)
DWORD THISCALL Window_Texture(BYTE *self)   // 0x40eb00
{
    if (m(self, W_TEXTURE)) {
        if (!(vc(self, V_30) & 0xff) && m(self, W_TEXTURE_INACTIVE))
            return m(self, W_TEXTURE_INACTIVE);
        return m(self, W_TEXTURE);
    }
    return (vc(self, V_30) & 0xff) ? gd(glob_addr(0x62e5a0)) : gd(glob_addr(0x62e59c));
}

// The part of the client area at (x, y) that shows, in content coordinates: the client rect on the screen
// (inside the borders and scroll bars) moved by (x, y) is cut to the visible rect (Rect_Intersect
// 0x4b2cd0); the result moved back and by the scroll position into `out`. al: whether anything shows.
DWORD THISCALL Window_VisibleClient(BYTE *self, int x, int y, int *out, DWORD)   // 0x40ef90
{
    int left = mi(self, W_SCREEN), top = mi(self, W_SCREEN + 4);
    const int right = mi(self, W_SCREEN + 8), bottom = mi(self, W_SCREEN + 12);
    (void)right, (void)bottom;
    const int bh = (int)vc(self, V_BORDER_H);
    left += (int)vc(self, V_BORDER_W);
    top += bh;
    BYTE *s = bp(m(self, W_SCROLL));
    int bar = s && m(s, S_HBAR) ? BAR : 0;
    const int ch = mi(self, W_RECT + 12) - mi(self, W_RECT + 4) - bar - (int)vc(self, V_BORDER_H);
    s = bp(m(self, W_SCROLL));
    bar = s && m(s, S_VBAR) ? BAR : 0;
    int cw = mi(self, W_RECT + 8) - mi(self, W_RECT) - bar + left - (int)vc(self, V_BORDER_W);
    const int bottom2 = ch + top - y;
    left += x;
    top += y;
    cw -= x;
    int r[4] = {left, top, cw, bottom2};
    const DWORD ok = fn<DWORD(__fastcall *)(int *, int *)>(0x4b2cd0)(&mi(self, W_VISIBLE), r);
    if (!(ok & 0xff))
        return ok;
    int ox = r[0], oy = r[1];
    const int dl = left - ox, dt = top - oy;
    out[0] = x + dl;
    out[1] = y + dt;
    out[2] = cw - left + x + dl;
    out[3] = bottom2 - top + y + dt;
    BYTE *s2 = bp(m(self, W_SCROLL));
    const int sy = s2 ? mi(s2, S_Y) : 0, sx = s2 ? mi(s2, S_X) : 0;
    out[0] += sx, out[1] += sy, out[2] += sx, out[3] += sy;
    return (pv(out) & ~0xffu) | 1;
}

// Width or height alone (slot 59)
DWORD THISCALL Window_SetWidth(BYTE *self, int w)   // 0x40f5b0
{
    return vc(self, V_SET_SIZE, w, mi(self, W_RECT + 12) - mi(self, W_RECT + 4));
}

DWORD THISCALL Window_SetHeight(BYTE *self, int h)   // 0x40f5d0
{
    return vc(self, V_SET_SIZE, mi(self, W_RECT + 8) - mi(self, W_RECT), h);
}

// Slot 66: every child's slot 66 while slot 27 of this window answers non-zero
void THISCALL Window_Slot66(BYTE *self)   // 0x40f840
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        if (vc(self, 27))
            vc(c, 66);
}


// ---- Construction and destruction -------------------------------------------------------------------------------

namespace {

// An object of the window texture list (0x62e5cc) by name: the list's slot 33 marks it used, then the record
// (+0x30 records of 0x80 bytes, +0x34 their count) is loaded on first use (+0x7f "loading", +0x78 the
// object; the file through slot 1, Path_Join 0x4a4220 and slot 37), as the resource lists' inline lookup
DWORD texture_object(DWORD find_name, DWORD load_name)
{
    BYTE *list = bp(gd(glob_addr(0x62e5cc)));
    vc(list, 33, vc(list, 2, at<const char *>(load_name), (DWORD)-1), 1);
    list = bp(gd(glob_addr(0x62e5cc)));
    DWORD idx = vc(list, 3, at<const char *>(find_name), (DWORD)-1);
    if (!m(list, 0x30))
        return 0;
    if (idx >= m(list, 0x34))
        idx = 0;
    const DWORD off = idx * 0x80;
    BYTE *rec = bp(m(list, 0x30));
    if (mb(rec, off + 0x7f))
        return 0;
    if (!m(rec, off + 0x78) && !mb(rec, off + 0x7f)) {
        mb(rec, off + 0x7f) = 1;
        const char *file = reinterpret_cast<const char *>(vc(list, 1, idx));
        char buf[0x100];
        fn<void(__fastcall *)(char *, BYTE *, const char *)>(0x4a4220)(buf, list + 0x38, file);
        char *name = const_cast<char *>(path_name(buf));
        DWORD obj;
        if (name == buf) {
            obj = vc(list, 37, glob<const char *>(0x62e558), name, bp(m(list, 0x30)) + off + 0x78);
        } else {
            name[-1] = 0;
            obj = vc(list, 37, buf, name, bp(m(list, 0x30)) + off + 0x78);
        }
        m(bp(m(list, 0x30)), off + 0x78) = obj;
        mb(bp(m(list, 0x30)), off + 0x7f) = 0;
    }
    return m(bp(m(list, 0x30)), off + 0x78);
}

bool texture_exists(DWORD name)
{
    BYTE *list = bp(gd(glob_addr(0x62e5cc)));
    return list && vc(list, 2, at<const char *>(name), (DWORD)-1) != (DWORD)-1;
}

// One of the toolkit's four bevel colours: the nearest palette entry (GamePalette_Get 0x433a80, cdecl) and the
// FieldColor's setter 0x527040 with the same arguments
void bevel_colour(DWORD color, DWORD v)
{
    const DWORD idx = fn<DWORD(__cdecl *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x433a80)(v, v, v, 1, 0xff, 1, 0xff, 0);
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x527040)(at<void *>(color), idx, v, v, v,
                                                                                                    1, 0xff, 1, 0xff, 0);
}

}  // namespace

// The common part of the constructors: the look (the parent's, else the factory's), the members, the parent's
// transparency and font, the border sizes; on the first window the toolkit's sounds, textures ("Window
// Texture", "Window Title Tile" (loaded under the name "Window Texture", as the original), "Inactive ..."; a
// missing inactive one is the active one) and its four bevel colours (palette entries 0x28, 0x50, 0xc8, 0xf5
// through a frame filter); then the default colours (slots 52, 51, 53, 54, 55, 56) and the title (slot 43)
void THISCALL Window_Init(BYTE *self, BYTE *parent, const char *title)   // 0x40a9b0
{
    m(self, W_LOOK) = 0;
    if (parent)
        vc(self, V_SET_LOOK, m(parent, W_LOOK));
    if (!m(self, W_LOOK))
        vc(self, V_SET_LOOK, fn<DWORD(__cdecl *)()>(0x4e2610)());
    m(self, W_CACHE) = 0;
    mb(self, W_ACCEL_ON) = 1;
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(self + W_TITLE, nullptr);
    m(self, W_INI) = 0;
    mb(self, W_INI_SIZE) = 0;
    int *rect = &mi(self, W_RECT);
    rect[0] = rect[1] = rect[2] = rect[3] = 0;
    m(self, W_SCROLL) = 0;
    m(self, W_PARENT) = m(self, W_FIRST) = m(self, W_LAST) = m(self, W_NEXT) = m(self, W_PREV) = 0;
    for (int i = 0; i < 4; i++)
        mi(self, W_SCREEN + 4 * i) = rect[i], mi(self, W_VISIBLE + 4 * i) = rect[i];
    mb(self, W_ACTIVE) = 0;
    mb(self, W_SHOWN) = 1;
    mb(self, W_ENABLED) = 1;
    mb(self, W_TAB_STOP) = 0;
    mb(self, W_FRAMED) = 0;
    mb(self, W_5E) = 0;
    mb(self, W_5F) = 0;
    mb(self, W_MODAL) = 0;
    mb(self, W_TRANSPARENT) = 0;
    m(self, W_TEXTURE) = 0;
    m(self, W_TEXTURE_INACTIVE) = 0;
    if (parent)
        mb(self, W_TRANSPARENT) = mb(parent, W_TRANSPARENT);
    mb(self, W_DRAWN) = 0;
    mb(self, W_CAPTURE) = 0;
    m(self, W_FONT) = gd(glob_addr(G_DEFAULT_FONT));
    if (parent)
        m(self, W_FONT) = m(parent, W_FONT);
    const DWORD fifth = m(bp(m(self, W_FONT)), 0x80) / 5;
    mb(self, W_BORDER) = (BYTE)((BYTE)fifth << 2);
    mb(self, W_BORDER2) = (BYTE)((BYTE)fifth << 2);
    BYTE b = gb(glob_addr(G_BORDER));
    if (b == 0xff) {
        b = (BYTE)((BYTE)fifth << 1);
        mb(self, W_BORDER3) = b;
        gb(glob_addr(G_BORDER)) = b;
    }
    mb(self, W_BORDER3) = b;
    if (!gb(glob_addr(0x62e644))) {
        const DWORD sounds[3] = {glob_addr(SOUND_ACTION), glob_addr(SOUND_SWITCH), glob_addr(SOUND_ERROR)};
        for (DWORD name : sounds)
            fn<void(THISCALL *)(void *, const char *)>(0x57e7b0)(glob<void *>(G_SOUNDS), at<const char *>(name));
        alignas(16) BYTE filter[0x40];
        gb(glob_addr(0x62e644)) = 1;
        fn<void(THISCALL *)(void *, DWORD)>(0x4b1cc0)(filter, 0);
        fn<void(THISCALL *)(void *, DWORD)>(0x4b1db0)(filter, m(bp(gd(glob_addr(0x6c02cc))), 0x24) == 8 ? 0xb : 0xc);
        constexpr DWORD WINDOW_TEXTURE = 0x60a578, TITLE_TILE = 0x60a564, INACTIVE = 0x60a54c, INACTIVE_TILE = 0x60a530;
        gd(glob_addr(0x62e5a0)) = 0;
        if (texture_exists(glob_addr(WINDOW_TEXTURE)))
            gd(glob_addr(0x62e5a0)) = texture_object(glob_addr(WINDOW_TEXTURE), glob_addr(WINDOW_TEXTURE));
        gd(glob_addr(0x62e588)) = 0;
        if (!gd(glob_addr(0x62e5cc))) {
            gd(glob_addr(0x62e59c)) = gd(glob_addr(0x62e5a0));
        } else {
            if (texture_exists(glob_addr(TITLE_TILE)))
                gd(glob_addr(0x62e588)) = texture_object(glob_addr(TITLE_TILE), glob_addr(WINDOW_TEXTURE));
            if (gd(glob_addr(0x62e5cc)) && texture_exists(glob_addr(INACTIVE)))
                gd(glob_addr(0x62e59c)) = texture_object(glob_addr(INACTIVE), glob_addr(INACTIVE));
            else
                gd(glob_addr(0x62e59c)) = gd(glob_addr(0x62e5a0));
        }
        if (gd(glob_addr(0x62e5cc)) && texture_exists(glob_addr(INACTIVE_TILE)))
            gd(glob_addr(0x62e58c)) = texture_object(glob_addr(INACTIVE_TILE), glob_addr(INACTIVE_TILE));
        else
            gd(glob_addr(0x62e58c)) = gd(glob_addr(0x62e588));
        bevel_colour(glob_addr(0x62e5b8), 0x28);
        bevel_colour(glob_addr(0x62e5a8), 0x50);
        bevel_colour(glob_addr(0x62e590), 0xc8);
        bevel_colour(glob_addr(0x62e568), 0xf5);
        fn<void(THISCALL *)(void *)>(0x4b1d20)(filter);
    }
    vc(self, 52, 0xba, 0xba, 0xba);
    vc(self, 51, 0, 0, 0);
    alignas(4) BYTE color[0x10];
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 0x3f, 0x9a, 0xff, 0xff);
    vc(self, 53, color);
    vc(self, 54, 0x60, 0x56, 0x4f);
    vc(self, 55, 0x4d, 0x3e, 0x35);
    vc(self, 56, 0, 0, 0);
    if (title)
        vc(self, V_SET_TITLE, title);
}

namespace {

void construct_members(BYTE *self)
{
    fn<void(THISCALL *)(BYTE *)>(0x4dec90)(self + W_TITLE);
    for (DWORD off = 0x80; off <= 0xd0; off += 0x10)
        fn<void(THISCALL *)(BYTE *)>(0x53f000)(self + off);
}

}  // namespace

// The constructors: on a parent with a title (text or ZString)
BYTE *THISCALL Window_Ctor(BYTE *self, BYTE *parent, const char *title)   // 0x40a8a0
{
    construct_members(self);
    m(self, 0) = vt(0x5eb404);
    fn<void(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a9b0)(self, parent, title);
    return self;
}

BYTE *THISCALL Window_CtorString(BYTE *self, BYTE *parent, BYTE *title)   // 0x40a940
{
    construct_members(self);
    m(self, 0) = vt(0x5eb404);
    fn<void(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a9b0)(self, parent, zs_cstr(title));
    return self;
}

// The destructor: the INI position saved when a section is set, the children deleted (slot 0 of the first
// until none is left), out of the parent (slot 33), the scroll info, the textures (slot 1), the dirty-rect
// cache (its proxy frame and memory stream) and the title freed; 0x62e5c4 set ("in a destructor")
void THISCALL Window_Dtor(BYTE *self)   // 0x40b190
{
    m(self, 0) = vt(0x5eb404);
    gb(glob_addr(G_IN_DESTRUCTOR)) = 1;
    if (m(self, W_INI))
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40e8f0)(self, m(self, W_INI), mb(self, W_INI_SIZE));
    while (BYTE *c = bp(m(self, W_FIRST)))
        vc(c, V_DELETE, 1);
    if (BYTE *p = bp(m(self, W_PARENT)))
        vc(p, V_REMOVE_CHILD, self);
    if (m(self, W_SCROLL)) {
        fn<void(__cdecl *)(void *)>(0x5d0320)(bp(m(self, W_SCROLL)));
        m(self, W_SCROLL) = 0;
    }
    if (BYTE *t = bp(m(self, W_TEXTURE))) {
        vc(t, 1, 1);
        m(self, W_TEXTURE) = 0;
    }
    if (BYTE *t = bp(m(self, W_TEXTURE_INACTIVE))) {
        vc(t, 1, 1);
        m(self, W_TEXTURE_INACTIVE) = 0;
    }
    if (BYTE *c = bp(m(self, W_CACHE))) {
        fn<void(THISCALL *)(BYTE *)>(0x42e9b0)(c + 0x54);
        BYTE *proxy = c + 0x38;
        m(proxy, 0) = vt(0x5eb62c);
        fn<void(THISCALL *)(BYTE *)>(0x4173f0)(proxy);
        m(proxy, 0) = vt(0x5eb534);
        fn<void(THISCALL *)(BYTE *)>(0x4173f0)(proxy);
        fn<void(THISCALL *)(BYTE *)>(0x4166d0)(proxy);
        fn<void(THISCALL *)(BYTE *)>(0x506000)(c);
        fn<void(__cdecl *)(void *)>(0x5d0320)(c);
        m(self, W_CACHE) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x4defd0)(self + W_TITLE);
}

// Slot 0 of CJWindow: the destructor and, with bit 0, the pool block (0x100 bytes)
BYTE *THISCALL Window_DeletingDtor(BYTE *self, DWORD flags)   // 0x40a910
{
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x100);
    return self;
}

// ---- CScreenWindow (vtable 0x5eb198, 0x17c bytes): the root of a window tree on a screen frame ----------------
// +0x100 a COffsetClippedProxyFrame on the frame, +0x104 the frame, +0x108 the mouse cursor shown by the
// window, +0x10c the time of the last mouse move, +0x110 the cursor's state before, +0x114 the tool tip's font,
// +0x118 its text, +0x11c the text wrapped, +0x120 its rect, +0x130 an accelerator underlined (+0x134 its
// height, +0x138/+0x13c the text width before and after it), +0x140/+0x144 the last mouse position, +0x148 three
// key repeat entries of 0x10 bytes {window, due time, interval, due flag}, +0x178 the current entry

// The tool tip's rect grown by 1 (al: whether a tip shows; with one the upper bytes are the out pointer's)
DWORD THISCALL Screen_TipRect(BYTE *self, int *out)   // 0x409290
{
    const DWORD r = fn<DWORD(THISCALL *)(BYTE *)>(0x4e13e0)(self + 0x118);
    if (!(r & 0xff))
        return r & ~0xffu;
    const int *t = &mi(self, 0x120);
    out[0] = t[0] - 1, out[1] = t[1] - 1, out[2] = t[2] + 1, out[3] = t[3] + 1;
    return (pv(out) & ~0xffu) | 1;
}

// A window's key repeat: the delay and interval (0: the Windows keyboard settings in seconds times
// 0x5eb2cc, truncated) and the due time
DWORD THISCALL Screen_StartKeyRepeat(BYTE *self, DWORD window, int delay, int repeat)   // 0x409330
{
    BYTE *e = self + 0x148 + (m(self, 0x178) << 4);
    if (m(e, 0) == window)
        return m(self, 0x178) << 4;
    if (!delay)
        delay = x87_ftol(fn<long double(__cdecl *)()>(0x4e2ae0)() * (long double)*glob<float *>(0x5eb2cc));
    if (!repeat)
        repeat = x87_ftol(fn<long double(__cdecl *)()>(0x4e2b30)() * (long double)*glob<float *>(0x5eb2cc));
    m(self, 0x148 + (m(self, 0x178) << 4)) = window;
    mi(self, ((m(self, 0x178) + 0x15) << 4)) = repeat;
    const DWORD due = fn<DWORD(__cdecl *)()>(0x4d2460)() + (DWORD)delay;
    m(self, 0x14c + (m(self, 0x178) << 4)) = due;
    return due;
}

// Whether the current key repeat is due (al; the upper bytes the entry's offset)
DWORD THISCALL Screen_KeyRepeatDue(BYTE *self)   // 0x4093c0
{
    const DWORD o = m(self, 0x178) << 4;
    return with_low_byte(o, mb(self, o + 0x154));   // the plain merge lost the upper bytes (reimpl.h)
}

// The mouse cursor shown (the cursor object's 0x518620, the keyboard state cleared) or hidden (0x5185f0)
void THISCALL Screen_ShowCursor(BYTE *self)   // 0x409870
{
    if (mb(self, 0x108))
        return;
    mb(self, 0x108) = 1;
    fn<void(THISCALL *)(void *)>(0x518620)(glob<void *>(G_CURSOR));
    fn<void(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(G_KEYS), 0);
    fn<void(THISCALL *)(void *, DWORD)>(0x4e2760)(glob<void *>(G_KEYS), 0);
}

DWORD THISCALL Screen_HideCursor(BYTE *self)   // 0x4098b0
{
    if (!mb(self, 0x108))
        return mb(self, 0x108);
    mb(self, 0x108) = 0;
    return fn<DWORD(THISCALL *)(void *)>(0x5185f0)(glob<void *>(G_CURSOR));
}

// The tool tip without an accelerator
DWORD THISCALL Screen_ShowTip(BYTE *self, DWORD font, const char *text, int x, int y)   // 0x4098d0
{
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, int, const char *, int, int)>(0x4098f0)(self, font, -1, text, x, y);
}

// The tool tip: a new text (an empty one: none) replaces the old one (the same text: nothing); at (x, y) or,
// with x = -1, below the mouse (the cursor sequence's height, else 20); wrapped to the screen's width times
// 0x5eb2d0 (the font's 0x4a7450), its size from the font (0x4a6c80 height with spacing 2, 0x4a7380 width);
// an accelerator's underline (the text widths up to it and after it); kept on the screen
void THISCALL Screen_ShowTipAccel(BYTE *self, DWORD font, int accel, const char *text, int x, int y)   // 0x4098f0
{
    if (text) {
        if (!*text)
            text = nullptr;
        else if (!lstrcmpA_exact(text, zs_cstr(self + 0x118)))
            return;
    }
    mb(self, 0x130) = 0;
    BYTE *tip = self + 0x118, *wrapped = self + 0x11c;
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(tip, nullptr);
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(wrapped, nullptr);
    if (!text)
        return;
    m(self, 0x114) = font;
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(tip, text);
    int *r = &mi(self, 0x120);
    if (x == -1) {
        fn<void(THISCALL *)(void *, int *, int *)>(0x5184e0)(glob<void *>(G_CURSOR), &r[0], &r[1]);
        if (BYTE *seq = bp(gd(glob_addr(G_CURSOR) + 4)))
            r[1] += mi(seq, 0x18) - mi(seq, 0x10);
        else
            r[1] += 0x14;
    } else {
        r[1] = y;
        r[0] = x;
    }
    const int width = x87_ftol((long double)(mi(self, W_RECT + 8) - mi(self, W_RECT)) * (long double)*glob<float *>(0x5eb2d0));
    alignas(4) BYTE tmp[4];
    BYTE *f = bp(m(self, 0x114));
    BYTE *res = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, int)>(0x4a7450)(f, tmp, zs_cstr(tip), width);
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x4e18f0)(wrapped, res);
    fn<void(THISCALL *)(BYTE *)>(0x4defd0)(tmp);
    const char *w = zs_cstr(wrapped);
    const int h = fn<int(THISCALL *)(BYTE *, const char *, int)>(0x4a6c80)(bp(m(self, 0x114)), zs_cstr(wrapped), 2);
    const int tw = fn<int(THISCALL *)(BYTE *, const char *)>(0x4a7380)(bp(m(self, 0x114)), w);
    r[2] = r[0] + tw;
    r[3] = r[1] + h;
    if (accel != -1) {
        mb(self, 0x130) = 1;
        m(self, 0x134) = m(bp(m(self, 0x114)), 0x80) + 1;
        m(self, 0x138) = fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, 0x114)), zs_cstr(tip), accel);
        m(self, 0x13c) = fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, 0x114)), zs_cstr(tip), accel + 1);
        if ((DWORD)(r[3] - r[1]) < m(self, 0x134) + 2)
            r[3] = r[1] + (int)m(self, 0x134) + 2;
    }
    if (mi(self, 0x128) > mi(self, W_RECT + 8) - 1) {
        const int d = mi(self, W_RECT + 8) - mi(self, 0x128) - 1;
        r[0] += d, r[2] += d;
    }
    if (mi(self, 0x12c) > mi(self, W_RECT + 12) - 1) {
        const int d = mi(self, W_RECT + 12) - mi(self, 0x12c) - 1;
        r[1] += d, r[3] += d;
    }
}

// The mouse moved since the last call: slot 13 (position, delta, cursor out), the cursor it named, the new
// position and the time
void THISCALL Screen_MouseUpdate(BYTE *self)   // 0x409b30
{
    int pos[2];
    fn<void(THISCALL *)(void *, int *, int *)>(0x5184e0)(glob<void *>(G_CURSOR), &pos[0], &pos[1]);
    if (pos[0] == mi(self, 0x140) && pos[1] == mi(self, 0x144))
        return;
    int delta[2] = {pos[0] - mi(self, 0x140), pos[1] - mi(self, 0x144)};
    DWORD cursor = 0;
    vc(self, V_MOUSE_MOVE, pos, delta, &cursor);
    if (cursor && cursor != gd(glob_addr(G_CURSOR) + 4))
        fn<void(THISCALL *)(void *, DWORD)>(0x518590)(glob<void *>(G_CURSOR), cursor);
    mi(self, 0x140) = pos[0];
    mi(self, 0x144) = pos[1];
    m(self, 0x10c) = fn<DWORD(__cdecl *)()>(0x4d2460)();
}

// The constructor on a screen frame (and the default font): "~CScreenWindow", active, the cursor's state
// kept, the keyboard state cleared, the proxy frame (0x3c bytes, 0x5247c0) on the frame, the rect from the
// frame's (slot 36), the proxy aimed at the frame (slot 62), the time
BYTE *THISCALL Screen_Ctor(BYTE *self, BYTE *frame, DWORD font)   // 0x408f20
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40a8a0)(self, nullptr, glob_addr(0x60a4ec));
    fn<void(THISCALL *)(BYTE *)>(0x4dec90)(self + 0x118);
    fn<void(THISCALL *)(BYTE *)>(0x4dec90)(self + 0x11c);
    m(self, 0) = vt(0x5eb198);
    mb(self, 0x108) = 1;
    mb(self, 0x110) = fn<BYTE(THISCALL *)(void *)>(0x518620)(glob<void *>(G_CURSOR));
    m(self, 0x104) = 0;
    for (int i = 0; i < 3; i++)
        m(self, 0x148 + 0x10 * (DWORD)i) = 0;
    if (font)
        gd(glob_addr(G_DEFAULT_FONT)) = font;
    mb(self, W_ACTIVE) = 1;
    fn<void(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(G_KEYS), 0);
    fn<void(THISCALL *)(void *, DWORD)>(0x4e2760)(glob<void *>(G_KEYS), 0);
    BYTE *proxy = static_cast<BYTE *>(fn<void *(__fastcall *)(DWORD)>(0x417710)(0x3c));
    if (proxy)
        proxy = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x5247c0)(proxy, frame);
    m(self, 0x100) = pv(proxy);
    int fr[4];
    vc(frame, 36, fr);
    mi(self, W_RECT + 8) = fr[2];
    mi(self, W_RECT + 12) = fr[3];
    m(self, 0x104) = pv(frame);
    mi(self, W_RECT) = 0;
    mi(self, W_RECT + 4) = 0;
    vc(self, V_SET_RECT, self + W_RECT);
    vc(bp(m(self, 0x100)), 62, bp(m(self, 0x104)));
    m(self, 0x10c) = fn<DWORD(__cdecl *)()>(0x4d2460)();
    return self;
}

// Slot 0: the cursor's state back (0x5186a0), the keyboard state cleared, the proxy frame deleted (slot 1),
// the tip texts, the base destructor; with bit 0 the pool block (0x17c)
BYTE *THISCALL Screen_DeletingDtor(BYTE *self, DWORD flags)   // 0x409040
{
    m(self, 0) = vt(0x5eb198);
    fn<void(THISCALL *)(void *, DWORD)>(0x5186a0)(glob<void *>(G_CURSOR), mb(self, 0x110));
    fn<void(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(G_KEYS), 0);
    fn<void(THISCALL *)(void *, DWORD)>(0x4e2760)(glob<void *>(G_KEYS), 0);
    if (BYTE *p = bp(m(self, 0x100))) {
        vc(p, 1, 1);
        m(self, 0x100) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x4defd0)(self + 0x11c);
    fn<void(THISCALL *)(BYTE *)>(0x4defd0)(self + 0x118);
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x17c);
    return self;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_window)
{
    const Hook list[] = {
        {0x40b5c0, (void*)&Window_SetLook, "CJWindow::SetLook", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x4c, 0xc2}},
        {0x40b5d0, (void*)&Window_Update, "CJWindow::Update", 9, {0x53, 0x8b, 0xd9, 0x8b, 0x8b, 0xec, 0x00, 0x00}},
        {0x40bac0, (void*)&WindowCache_AddDirty, "CJWindow::WindowCache_AddDirty", 7, {0x8b, 0x41, 0x30, 0x8b, 0x54, 0x24, 0x04, 0x83}},
        {0x40bb90, (void*)&Window_Close, "CJWindow::Close", 5, {0x8b, 0x01, 0xff, 0x50, 0x18, 0xb0, 0x01, 0xc3}},
        {0x40bbb0, (void*)&Window_CloseChildren, "CJWindow::CloseChildren", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0xf0, 0x00, 0x00}},
        {0x40bc30, (void*)&Window_Slot9, "CJWindow::Slot9", 5, {0x51, 0x53, 0x8b, 0xd1, 0x55, 0x56, 0x8b, 0xb2}},
        {0x40bcc0, (void*)&Window_MouseDown, "CJWindow::MouseDown", 5, {0x83, 0xec, 0x08, 0x33, 0xd2, 0x53, 0x55, 0x56}},
        {0x40bde0, (void*)&Window_MouseUp, "CJWindow::MouseUp", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x40be70, (void*)&Window_StartKeyRepeat, "CJWindow::StartKeyRepeat", 6, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xf7, 0x8b, 0x87}},
        {0x40bec0, (void*)&Window_KeyRepeatDue, "CJWindow::KeyRepeatDue", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xe8, 0x00, 0x00}},
        {0x40bf00, (void*)&Window_SetTextureList, "CJWindow::SetTextureList", 6, {0x89, 0x0d, 0xc8, 0xe5, 0x62, 0x00, 0x89, 0x15}},
        {0x40c0b0, (void*)&Window_Slot15, "CJWindow::Slot15", 8, {0x53, 0x8b, 0xd9, 0x55, 0x8b, 0x6c, 0x24, 0x0c}},
        {0x40c170, (void*)&Window_Key, "CJWindow::Key", 6, {0x53, 0x56, 0x8b, 0x5c, 0x24, 0x0c, 0x57, 0x8b}},
        {0x40c310, (void*)&Window_Char, "CJWindow::Char", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x32, 0xdb}},
        {0x40c3e0, (void*)&Window_SetActive, "CJWindow::SetActive", 7, {0x8b, 0x44, 0x24, 0x04, 0x88, 0x41, 0x5c, 0x8b}},
        {0x40c400, (void*)&Window_Notify, "CJWindow::Notify", 6, {0x8b, 0x89, 0xe8, 0x00, 0x00, 0x00, 0x85, 0xc9}},
        {0x40c600, (void*)&Window_SetTransparent, "CJWindow::SetTransparent", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xb1}},
        {0x40c630, (void*)&Window_IsEnabled, "CJWindow::IsEnabled", 6, {0x8b, 0x81, 0xe8, 0x00, 0x00, 0x00, 0x85, 0xc0}},
        {0x40c680, (void*)&Window_AddToParentFront, "CJWindow::AddToParentFront", 6, {0x8b, 0x81, 0xe8, 0x00, 0x00, 0x00, 0x85, 0xc0}},
        {0x40c6e0, (void*)&Window_AddModalToParent, "CJWindow::AddModalToParent", 5, {0x8b, 0x54, 0x24, 0x04, 0x56, 0xc6, 0x42, 0x64}},
        {0x40c740, (void*)&Window_MessageBox, "CJWindow::MessageBox", 8, {0x56, 0x8b, 0xf1, 0xb9, 0x1c, 0x01, 0x00, 0x00}},
        {0x40c7c0, (void*)&Window_AddChild, "CJWindow::AddChild", 7, {0x53, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x10, 0x57}},
        {0x40c8d0, (void*)&Window_AddChildFront, "CJWindow::AddChildFront", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x40c9b0, (void*)&Window_RemoveChild, "CJWindow::RemoveChild", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0x90, 0xf8}},
        {0x40ca40, (void*)&Window_Focus, "CJWindow::Focus", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x40caf0, (void*)&Window_ToBack, "CJWindow::ToBack", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xec, 0x00, 0x00}},
        {0x40cb30, (void*)&Window_FocusNext, "CJWindow::FocusNext", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xec, 0x00, 0x00}},
        {0x40cb70, (void*)&Window_FocusPrev, "CJWindow::FocusPrev", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xec, 0x00, 0x00}},
        {0x40cc70, (void*)&Window_Root, "CJWindow::Root", 8, {0x8b, 0xc1, 0x8b, 0x88, 0xe8, 0x00, 0x00, 0x00}},
        {0x40cc90, (void*)&Window_FindChild, "CJWindow::FindChild", 7, {0x56, 0x8b, 0xb1, 0xec, 0x00, 0x00, 0x00, 0x85}},
        {0x40ccd0, (void*)&Window_Screen, "CJWindow::Screen", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xe8, 0x00, 0x00}},
        {0x40cd00, (void*)&Window_ShowTip, "CJWindow::ShowTip", 6, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xf7, 0x8b, 0x87}},
        {0x40cd50, (void*)&Window_ShowTipAccel, "CJWindow::ShowTipAccel", 6, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xf7, 0x8b, 0x87}},
        {0x40cdb0, (void*)&Window_ClearAccel, "CJWindow::ClearAccel", 6, {0x32, 0xc0, 0xc6, 0x41, 0x59, 0xff, 0x88, 0x41}},
        {0x40cdc0, (void*)&Window_SetTitle, "CJWindow::SetTitle", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0x8d, 0x75}},
        {0x40cee0, (void*)&Window_SetTitleChar, "CJWindow::SetTitleChar", 6, {0x8a, 0x44, 0x24, 0x04, 0x8b, 0x11, 0x88, 0x44}},
        {0x40cfe0, (void*)&Window_SetColorA0, "CJWindow::SetColorA0", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8d, 0xb1}},
        {0x40d0c0, (void*)&Window_SetBorder, "CJWindow::SetBorder", 8, {0x8b, 0x51, 0x7c, 0xb8, 0xcd, 0xcc, 0xcc, 0xcc}},
        {0x40d0f0, (void*)&Window_RightOf, "CJWindow::RightOf", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x04, 0x8b}},
        {0x40d100, (void*)&Window_SpreadWidth, "CJWindow::SpreadWidth", 6, {0x83, 0xec, 0x10, 0x8b, 0x41, 0x04, 0x55, 0x8b}},
        {0x40d230, (void*)&Window_SameWidth, "CJWindow::SameWidth", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x40d2a0, (void*)&Window_SetRect, "CJWindow::SetRect", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x57}},
        {0x40d490, (void*)&Window_ClientWidth, "CJWindow::ClientWidth", 10, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x86, 0xfc, 0x00}},
        {0x40d4d0, (void*)&Window_ClientHeight, "CJWindow::ClientHeight", 10, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x86, 0xfc, 0x00}},
        {0x40d510, (void*)&Window_SetSize, "CJWindow::SetSize", 6, {0x83, 0xec, 0x10, 0x8b, 0x41, 0x04, 0x8b, 0x51}},
        {0x40d550, (void*)&Window_SetContentSize, "CJWindow::SetContentSize", 5, {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x8b, 0xf1}},
        {0x40d630, (void*)&Window_MoveTo, "CJWindow::MoveTo", 7, {0x83, 0xec, 0x10, 0x8b, 0x44, 0x24, 0x14, 0x56}},
        {0x40d680, (void*)&Window_SetLeft, "CJWindow::SetLeft", 6, {0x83, 0xec, 0x10, 0x8b, 0x51, 0x08, 0x8b, 0x44}},
        {0x40d6c0, (void*)&Window_SetTop, "CJWindow::SetTop", 6, {0x83, 0xec, 0x10, 0x8b, 0x51, 0x0c, 0x8b, 0x41}},
        {0x40d700, (void*)&Window_SetScrollPos, "CJWindow::SetScrollPos", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x8b, 0xf1, 0x33}},
        {0x40d8c0, (void*)&Window_SetScrollX, "CJWindow::SetScrollX", 6, {0x8b, 0x81, 0xfc, 0x00, 0x00, 0x00, 0x85, 0xc0}},
        {0x40d900, (void*)&Window_SetScrollY, "CJWindow::SetScrollY", 6, {0x8b, 0x81, 0xfc, 0x00, 0x00, 0x00, 0x85, 0xc0}},
        {0x40d940, (void*)&Window_ScrollToShow, "CJWindow::ScrollToShow", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x86, 0xfc}},
        {0x40d9f0, (void*)&Window_ScrollBy, "CJWindow::ScrollBy", 6, {0x8b, 0x81, 0xfc, 0x00, 0x00, 0x00, 0x85, 0xc0}},
        {0x40da30, (void*)&Window_ShowScrollBars, "CJWindow::ShowScrollBars", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x8b, 0xf1, 0x33}},
        {0x40e590, (void*)&Window_FitChild, "CJWindow::FitChild", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24}},
        {0x40e8f0, (void*)&Window_SaveIni, "CJWindow::SaveIni", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x40e9b0, (void*)&Window_TexturesOn, "CJWindow::TexturesOn", 6, {0x8a, 0x0d, 0x2d, 0xa5, 0x60, 0x00, 0x33, 0xc0}},
        {0x40e9c0, (void*)&Window_SetGlobals3, "CJWindow::SetGlobals3", 10, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x0d, 0xd4, 0xe5}},
        {0x40e9e0, (void*)&CursorEntry, "CJWindow::CursorEntry", 7, {0x8b, 0x04, 0x8d, 0xe4, 0xe5, 0x62, 0x00, 0x85}},
        {0x40ea00, (void*)&Window_Cursor, "CJWindow::Cursor", 5, {0xa1, 0xe4, 0xe5, 0x62, 0x00, 0xc2, 0x04, 0x00}},
        {0x40ea10, (void*)&Window_ParentSlot70, "CJWindow::ParentSlot70", 6, {0x8b, 0x89, 0xe8, 0x00, 0x00, 0x00, 0x85, 0xc9}},
        {0x40eb00, (void*)&Window_Texture, "CJWindow::Texture", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xe0, 0x00, 0x00}},
        {0x40ef90, (void*)&Window_VisibleClient, "CJWindow::VisibleClient", 5, {0x83, 0xec, 0x28, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x40f5b0, (void*)&Window_SetWidth, "CJWindow::SetWidth", 5, {0x8b, 0x51, 0x10, 0x8b, 0x01, 0x56, 0x8b, 0x71}},
        {0x40f5d0, (void*)&Window_SetHeight, "CJWindow::SetHeight", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x56, 0x8b}},
        {0x40f840, (void*)&Window_Slot66, "CJWindow::Slot66", 10, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xb7, 0xec, 0x00}},
        {0x40a9b0, (void*)&Window_Init, "CJWindow::Init", 6, {0x81, 0xec, 0x24, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x40a8a0, (void*)&Window_Ctor, "CJWindow::CJWindow", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x50, 0xe8, 0xe5}},
        {0x40a940, (void*)&Window_CtorString, "CJWindow::CJWindow(ZString)", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x50, 0xe8, 0x45}},
        {0x40b190, (void*)&Window_Dtor, "CJWindow::~CJWindow", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0x57, 0xc7}},
        {0x40a910, (void*)&Window_DeletingDtor, "CJWindow::DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x78, 0x08, 0x00, 0x00}, 4},
        {0x409290, (void*)&Screen_TipRect, "CScreenWindow::TipRect", 9, {0x56, 0x8b, 0xf1, 0x8d, 0x8e, 0x18, 0x01, 0x00}},
        {0x409330, (void*)&Screen_StartKeyRepeat, "CScreenWindow::StartKeyRepeat", 8, {0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x5c, 0x24, 0x0c}},
        {0x4093c0, (void*)&Screen_KeyRepeatDue, "CScreenWindow::KeyRepeatDue", 6, {0x8b, 0x81, 0x78, 0x01, 0x00, 0x00, 0xc1, 0xe0}},
        {0x409870, (void*)&Screen_ShowCursor, "CScreenWindow::ShowCursor", 6, {0x8a, 0x81, 0x08, 0x01, 0x00, 0x00, 0x84, 0xc0}},
        {0x4098b0, (void*)&Screen_HideCursor, "CScreenWindow::HideCursor", 6, {0x8a, 0x81, 0x08, 0x01, 0x00, 0x00, 0x84, 0xc0}},
        {0x4098d0, (void*)&Screen_ShowTip, "CScreenWindow::ShowTip", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4098f0, (void*)&Screen_ShowTipAccel, "CScreenWindow::ShowTipAccel", 8, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x1c}},
        {0x409b30, (void*)&Screen_MouseUpdate, "CScreenWindow::MouseUpdate", 7, {0x83, 0xec, 0x14, 0x8d, 0x44, 0x24, 0x08, 0x56}},
        {0x408f20, (void*)&Screen_Ctor, "CScreenWindow::CScreenWindow", 5, {0x83, 0xec, 0x10, 0x56, 0x57, 0x68, 0xec, 0xa4}},
        {0x409040, (void*)&Screen_DeletingDtor, "CScreenWindow::DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xb9, 0x40, 0xff, 0x6c, 0x00}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
