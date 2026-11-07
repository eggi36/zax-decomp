// The frame windows of the window toolkit (docs/DECOMPILATION.md 9, "The window toolkit"): the frame window
// (vtable 0x5ea5f8, 0x118 bytes; a title bar, an optional close button, transparency toggle and resize grip,
// moved and resized with the mouse), its base (0x5ea72c, only the forwarding of slots 22-25 to the focused
// child), the dialogs (0x5ea85c and 0x5eaac4, 0x134 bytes: they replace another window of the parent and give it
// back when they close) and the message box (0x5ea990, 0x11c bytes: a text and an OK button).
//
// Layout [confirmed: the constructors 0x402d80/0x403ab0/0x403d00/0x403b30 and the slots below]: the CJWindow
// part, then
//   +0x100 the close button (a button, 0x138 bytes), +0x104 the resize grip (0x104 bytes; its +0x100 holds the
//   edge being dragged: 1 left, 2 top, 4 right, 8 bottom, or-ed), +0x108 the transparency toggle, +0x10c movable
//   (1 by default), +0x110/+0x114 the minimum width and height (the size at construction, at least 10)
// Dialogs: +0x118 the window the dialog replaced (hidden while it shows; deleted with the dialog when the
// dialog has no parent), +0x11c the default button (Enter), +0x120 the cancel button (Escape), +0x124 the
// replaced window's rect. Message box: +0x118 the OK button.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD F_CLOSE = 0x100, F_RESIZE = 0x104, F_TOGGLE = 0x108, F_MOVABLE = 0x10c, F_MIN_W = 0x110, F_MIN_H = 0x114;
constexpr DWORD GRIP_EDGE = 0x100;
constexpr DWORD D_REPLACED = 0x118, D_DEFAULT = 0x11c, D_CANCEL = 0x120, D_RECT = 0x124;
constexpr DWORD MB_OK = 0x118;
constexpr int PLACE_DEFAULT = -1000;

BYTE *screen_of(BYTE *self) { return fn<BYTE *(THISCALL *)(BYTE *)>(0x40ccd0)(self); }
// The frame window's destructor part: the screen's tool tip cleared (0x4098d0), the base's vtable, CJWindow's
// destructor
void frame_dtor(BYTE *self)
{
    m(self, 0) = vt(0x5ea5f8);
    if (BYTE *screen = screen_of(self))
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, int, int)>(0x4098d0)(screen, 0, 0, -1, -1);
    m(self, 0) = vt(0x5ea72c);
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
}
void pool_delete(BYTE *p, DWORD size) { fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(p, size); }
DWORD cursor(int i) { return fn<DWORD(__fastcall *)(int)>(0x40e9e0)(i); }
bool inside(const int *p, BYTE *self)
{
    return p[0] >= mi(self, W_SCREEN) && p[1] >= mi(self, W_SCREEN + 4) && p[0] < mi(self, W_SCREEN + 8) &&
           p[1] < mi(self, W_SCREEN + 12);
}
DWORD edge_hit(BYTE *self, const int *p) { return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x4039e0)(self, p); }
// The cursor for a resize edge: horizontal, vertical and the two diagonals (the base of the group in the
// table; 0 none)
int edge_cursor(DWORD edge, int base)
{
    switch (edge) {
    case 1: case 4: return base;
    case 2: case 8: return base + 2;
    case 3: case 12: return base + 4;
    case 6: case 9: return base + 6;
    default: return 0;
    }
}
// The replaced window's rect kept, the window hidden (slot 26)
void keep_replaced(BYTE *dialog, BYTE *w)
{
    const int *r = vc<const int *>(w, V_GET_RECT);
    for (int k = 0; k < 4; k++)
        mi(dialog, D_RECT + 4 * k) = r[k];
    vc(bp(m(dialog, D_REPLACED)), 26, 0, 0);
}
void drop_replaced(BYTE *dialog)
{
    if (BYTE *old = bp(m(dialog, D_REPLACED))) {
        vc(old, V_DELETE, 1);
        m(dialog, D_REPLACED) = 0;
    }
}

}  // namespace

// ---- The frame window -------------------------------------------------------------------------------------------

// Slot 70: the parent's cursor reset (0x40cd00 with 0, -1, -1); true
bool THISCALL Frame_ParentCursor(BYTE *self, DWORD)   // 0x402cf0
{
    fn<void(THISCALL *)(BYTE *, DWORD, int, int)>(0x40cd00)(self, 0, -1, -1);
    return true;
}

// Slots 22-25 of the frame window base: to the child with the focus (false without one)
#define FRAME_FORWARD(name, address, slot) \
    DWORD THISCALL name(BYTE *self, DWORD a) /* address */ \
    { \
        BYTE *c = bp(m(self, W_FIRST)); \
        if (!c) \
            return 0; \
        return vc(c, slot, a); \
    }
FRAME_FORWARD(FrameBase_Slot22, 0x402d00, 22)
FRAME_FORWARD(FrameBase_Slot23, 0x402d20, 23)
FRAME_FORWARD(FrameBase_Slot24, 0x402d40, 24)
FRAME_FORWARD(FrameBase_Slot25, 0x402d60, 25)
#undef FRAME_FORWARD

// The constructor: CJWindow's on the parent with the title, movable, no buttons; wide enough for the title
// (its width + 3, 0x40f5b0) and as high as the font (0x40f5d0), border 2 (0x40d0c0); the minimum size is the
// size then, at least 10 by 10
BYTE *THISCALL Frame_Ctor(BYTE *self, BYTE *parent, const char *title)   // 0x402d80
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, title);
    m(self, 0) = vt(0x5ea5f8);
    mb(self, F_MOVABLE) = 1;
    m(self, F_CLOSE) = 0;
    m(self, F_RESIZE) = 0;
    m(self, F_TOGGLE) = 0;
    if (fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + W_TITLE)) {
        const char *t = fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(self + W_TITLE);
        const DWORD w = fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, W_FONT)), t, -1) + 3;
        if ((DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT)) < w)
            fn<void(THISCALL *)(BYTE *, DWORD)>(0x40f5b0)(self, w);
        const DWORD h = m(bp(m(self, W_FONT)), 0x80);
        if ((DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4)) < h)
            fn<void(THISCALL *)(BYTE *, DWORD)>(0x40f5d0)(self, h);
    }
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40d0c0)(self, 2);
    const DWORD w = (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT));
    m(self, F_MIN_W) = w < 10 ? 10 : w;
    const DWORD h = (DWORD)(mi(self, W_RECT + 12) - mi(self, W_RECT + 4));
    m(self, F_MIN_H) = h < 10 ? 10 : h;
    return self;
}

// Slot 30: whether it has the focus
DWORD THISCALL Frame_IsActive(BYTE *self)   // 0x402e70
{
    return mb(self, W_ACTIVE) != 0;
}

// Slot 0 (and the destructor 0x402f00): with bit 0 the pool block (0x118)
BYTE *THISCALL Frame_DeletingDtor(BYTE *self, DWORD flags)   // 0x402e80
{
    frame_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x118);
    return self;
}

BYTE *THISCALL FrameBase_DeletingDtor(BYTE *self, DWORD flags)   // 0x402ed0
{
    m(self, 0) = vt(0x5ea72c);
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_delete(self, 0x100);
    return self;
}

void THISCALL Frame_Dtor(BYTE *self)   // 0x402f00
{
    frame_dtor(self);
}

// Slot 4: the frame (slot 76) and the children (0x40b620), the frame last when textures are on (0x40e9b0)
void THISCALL Frame_Draw(BYTE *self, BYTE *frame)   // 0x402f30
{
    if (!fn<DWORD(THISCALL *)(BYTE *)>(0x40e9b0)(self)) {
        vc(self, 76, frame);
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
    } else {
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
        vc(self, 76, frame);
    }
}

// Slot 14, a mouse drag (the point, the movement, the buttons, the cursor out): with the left button down,
// the press inside the window and a resize grip, the edge under the press is taken (EdgeHit in content
// coordinates, only when the button was not captured yet); while an edge is dragged the cursor shows it and
// the rect grows by the movement on that edge (0x403720); else a movable window moves by it (0x40d630); else
// the base's (0x40c0a0). False when the press is outside the screen rect.
bool THISCALL Frame_MouseDrag(BYTE *self, const int *point, const int *delta, DWORD buttons, DWORD *cursor_out)   // 0x403260
{
    if (!(buttons & 1))
        return fn<bool(THISCALL *)(BYTE *, const int *, const int *, DWORD, DWORD *)>(0x40c0a0)(self, point, delta, buttons, cursor_out);
    if (!(mb(self, W_CAPTURE) & 1)) {
        if (!inside(point, self))
            return false;
        if (m(self, F_RESIZE)) {
            BYTE *s = bp(m(self, W_SCROLL));
            int local[2];
            local[0] = point[0] - mi(self, W_SCREEN) + (s ? mi(s, S_X) : 0);
            local[1] = point[1] - mi(self, W_SCREEN + 4) + (s ? mi(s, S_Y) : 0);
            m(bp(m(self, F_RESIZE)), GRIP_EDGE) = edge_hit(self, local);
        }
    }
    BYTE *grip = bp(m(self, F_RESIZE));
    if (grip && m(grip, GRIP_EDGE)) {
        const DWORD edge = m(grip, GRIP_EDGE);
        if (const int c = edge_cursor(edge, 8))
            *cursor_out = cursor(c);
        int r[4] = {mi(self, W_RECT), mi(self, W_RECT + 4), mi(self, W_RECT + 8), mi(self, W_RECT + 12)};
        if (m(grip, GRIP_EDGE) & 1)
            r[0] += delta[0];
        if (m(grip, GRIP_EDGE) & 2)
            r[1] += delta[1];
        if (m(grip, GRIP_EDGE) & 4)
            r[2] += delta[0];
        if (m(grip, GRIP_EDGE) & 8)
            r[3] += delta[1];
        fn<void(THISCALL *)(BYTE *, int *, DWORD, const int *)>(0x403720)(self, r, m(grip, GRIP_EDGE), point);
        return true;
    }
    if (mb(self, F_MOVABLE)) {
        fn<void(THISCALL *)(BYTE *, int, int)>(0x40d630)(self, mi(self, W_RECT) + delta[0], mi(self, W_RECT + 4) + delta[1]);
        return true;
    }
    return fn<bool(THISCALL *)(BYTE *, const int *, const int *, DWORD, DWORD *)>(0x40c0a0)(self, point, delta, buttons, cursor_out);
}

// Slot 10, a button press: the base's (0x40bcc0); else whether the point lies in the screen rect
DWORD THISCALL Frame_MouseDown(BYTE *self, DWORD button, const int *point)   // 0x403470
{
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, point))
        return 1;
    return inside(point, self) ? 1 : 0;
}

// Slot 9: the base's (0x40bc30); else inside the screen rect the press (0x40bcc0, which sets *handled), true
bool THISCALL Frame_Mouse9(BYTE *self, DWORD button, const int *point, BYTE *handled)   // 0x4034c0
{
    if (fn<bool(THISCALL *)(BYTE *, DWORD, const int *, BYTE *)>(0x40bc30)(self, button, point, handled))
        return true;
    if (!inside(point, self))
        return false;
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, point))
        *handled = 1;
    return true;
}

// Slot 11, a button release: the base's (0x40bde0) as 0/1
DWORD THISCALL Frame_MouseUp(BYTE *self, DWORD button, const int *point)   // 0x403530
{
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bde0)(self, button, point) ? 1 : 0;
}

// Slot 69, the cursor at a point: over a resize edge the edge's cursor; else the base's (0x40ea00)
DWORD THISCALL Frame_Cursor(BYTE *self, const int *point)   // 0x403550
{
    if (m(self, F_RESIZE))
        if (const int c = edge_cursor(edge_hit(self, point), 7))
            return cursor(c);
    return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x40ea00)(self, point);
}

// Slot 21, a child's notification (the child, the code): code 0 from the close button closes (slot 5), from
// the toggle switches the transparency (0x40c600); else the base's (0x40c400). No result (eax is what the
// callee left).
void THISCALL Frame_Notify(BYTE *self, BYTE *child, DWORD code)   // 0x4035f0
{
    if (!code) {
        if (m(self, F_CLOSE) && child == bp(m(self, F_CLOSE))) {
            vc(self, V_CLOSE);
            return;
        }
        if (m(self, F_TOGGLE) && child == bp(m(self, F_TOGGLE))) {
            fn<void(THISCALL *)(BYTE *, bool)>(0x40c600)(self, mb(self, W_TRANSPARENT) == 0);
            return;
        }
    }
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40c400)(self, child, code);
}

// Slot 57, the rect: with a resize grip not below the minimum size (the rect argument is widened in place);
// CJWindow's (0x40d2a0); the close button and the toggle placed top right (slot 62 with 3, 0, 2, 2, 1), the
// toggle left of the close button (0x40d680) and shown in the transparency state (slot 78); the grip bottom
// right (4, 0, 1, 0, 1)
void THISCALL Frame_SetRect(BYTE *self, int *rect)   // 0x403640
{
    if (m(self, F_RESIZE)) {
        if ((DWORD)(rect[2] - rect[0]) < m(self, F_MIN_W))
            rect[2] = (int)m(self, F_MIN_W) + rect[0];
        if ((DWORD)(rect[3] - rect[1]) < m(self, F_MIN_H))
            rect[3] = (int)m(self, F_MIN_H) + rect[1];
    }
    fn<void(THISCALL *)(BYTE *, int *)>(0x40d2a0)(self, rect);
    if (BYTE *c = bp(m(self, F_CLOSE)))
        vc(c, V_PLACE, 3, 0, 2, 2, 1);
    if (BYTE *t = bp(m(self, F_TOGGLE))) {
        vc(t, V_PLACE, 3, 0, 2, 2, 1);
        if (BYTE *c = bp(m(self, F_CLOSE)))
            fn<void(THISCALL *)(BYTE *, int)>(0x40d680)(t, mi(c, W_RECT) - mi(t, W_RECT + 8) + mi(t, W_RECT) - 5);
        vc(bp(m(self, F_TOGGLE)), 78, (DWORD)mb(self, W_TRANSPARENT));
    }
    if (BYTE *g = bp(m(self, F_RESIZE)))
        vc(g, V_PLACE, 4, 0, 1, 0, 1);
}

// A resize to the rect (slot 57, which may enlarge it to the minimum): when it changed, the edges that did not
// follow the mouse move the window back (0x40d680 left, 0x40d6c0 top) and the mouse is put where the edge
// ended (the cursor 0x518570 and *point)
void THISCALL Frame_ResizeTo(BYTE *self, int *rect, DWORD edge, int *point)   // 0x403720
{
    const int l = rect[0], t = rect[1], r = rect[2], b = rect[3];
    vc(self, V_SET_RECT, rect);
    if (l == rect[0] && t == rect[1] && r == rect[2] && b == rect[3])
        return;
    int px = point[0], py = point[1];
    if (edge & 1) {
        fn<void(THISCALL *)(BYTE *, int)>(0x40d680)(self, mi(self, W_RECT) - rect[2] - l + rect[0] + r);
        px += rect[0] - rect[2] - l + r;
    }
    if (edge & 2) {
        fn<void(THISCALL *)(BYTE *, int)>(0x40d6c0)(self, mi(self, W_RECT + 4) - t - rect[3] + rect[1] + b);
        py += rect[1] - t - rect[3] + b;
    }
    if (edge & 4)
        px += rect[2] - rect[0] - r + l;
    if (edge & 8)
        py += rect[3] - rect[1] - b + t;
    fn<void(THISCALL *)(void *, int, int)>(0x518570)(glob<void *>(G_CURSOR), px, py);
    point[0] = px;
    point[1] = py;
}

// The close button (once): a button (0x40f9b0, the text "\x18", 13 by 13, no tab stop, the tool tip of 0x62e518
// through 0x4100c0) added top right (slot 31 with 3, 0, 2, 2, 1)
void THISCALL Frame_AddClose(BYTE *self)   // 0x4038a0
{
    if (m(self, F_CLOSE))
        return;
    const char text[2] = {0x18, 0};
    BYTE *p = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x138);
    if (p)
        p = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(p, self, text, 0);
    m(self, F_CLOSE) = pv(p);
    vc(p, V_SET_SIZE, 13, 13);
    mb(bp(m(self, F_CLOSE)), W_TAB_STOP) = 0;
    const char *tip = fn<const char *(THISCALL *)(DWORD)>(0x4e13a0)(glob_addr(0x62e518));
    fn<void(THISCALL *)(BYTE *, const char *, int)>(0x4100c0)(bp(m(self, F_CLOSE)), tip, -1);
    vc(self, V_ADD_CHILD, bp(m(self, F_CLOSE)), 3, 0, 2, 2, 1);
}

// The resize grip (once, 0x40f870) added bottom right (slot 31 with 4, 0, 1, 0, 1)
void THISCALL Frame_AddResize(BYTE *self)   // 0x403930
{
    if (m(self, F_RESIZE))
        return;
    BYTE *p = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x104);
    if (p)
        p = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x40f870)(p, self);
    m(self, F_RESIZE) = pv(p);
    vc(self, V_ADD_CHILD, p, 4, 0, 1, 0, 1);
}

// Slot 60, the height of the title bar: font height + 4 * (font height / 5) + 2 with a title, else 0
DWORD THISCALL Frame_BorderHeight(BYTE *self)   // 0x403a80
{
    if (!fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + W_TITLE))
        return 0;
    const DWORD h = m(bp(m(self, W_FONT)), 0x80);
    return h + 4 * (h / 5) + 2;
}

// ---- The dialogs ------------------------------------------------------------------------------------------------

// The constructors of the two dialog classes: the frame window's, not movable, nothing replaced, no buttons
BYTE *THISCALL Dialog2_Ctor(BYTE *self, BYTE *parent, const char *title)   // 0x403ab0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x402d80)(self, parent, title);
    m(self, 0) = vt(0x5ea85c);
    mb(self, F_MOVABLE) = 0;
    m(self, D_REPLACED) = 0;
    m(self, D_DEFAULT) = 0;
    m(self, D_CANCEL) = 0;
    return self;
}

BYTE *THISCALL Dialog_Ctor(BYTE *self, BYTE *parent, const char *title)   // 0x403d00
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x402d80)(self, parent, title);
    m(self, 0) = vt(0x5eaac4);
    mb(self, F_MOVABLE) = 0;
    m(self, D_REPLACED) = 0;
    m(self, D_DEFAULT) = 0;
    m(self, D_CANCEL) = 0;
    return self;
}

// Slot 4 of the first dialog class: only the children (0x40b620)
void THISCALL Dialog2_Draw(BYTE *self, BYTE *frame)   // 0x403af0
{
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
}

// The destructor: the replaced window comes back to the parent (slot 31 when the parent's focused child is
// modal, else slot 32, placing 0 at the default position), gets its rect back (slot 57) and shows (slot 26);
// without a parent it is deleted. Then the frame window's.
void THISCALL Dialog_Dtor(BYTE *self)   // 0x403d70
{
    m(self, 0) = vt(0x5eaac4);
    if (BYTE *old = bp(m(self, D_REPLACED))) {
        if (BYTE *parent = bp(m(self, W_PARENT))) {
            BYTE *f = bp(m(parent, W_FIRST));
            if (f && mb(f, W_MODAL))
                vc(parent, V_ADD_CHILD, old, 0, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
            else
                vc(parent, V_ADD_CHILD_FRONT, old, 0, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
            vc(bp(m(self, D_REPLACED)), V_SET_RECT, self + D_RECT);
            vc(bp(m(self, D_REPLACED)), 26, 1, 0);
        } else {
            vc(old, V_DELETE, 1);
        }
        m(self, D_REPLACED) = 0;
    }
    frame_dtor(self);
}

BYTE *THISCALL Dialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x403d40
{
    fn<void(THISCALL *)(BYTE *)>(0x403d70)(self);
    if (flags & 1)
        pool_delete(self, 0x134);
    return self;
}

// Slot 16, a key: the base's (0x40c170) first; keys 6 and 11 move the focus on (0x40cb70), 10 and 12 back
// (0x40cb30); Enter to the focused child (slot 18) or, when it does not take it, the default button; Escape and
// 0xfa to the cancel and the default button (slot 18). Whether the key was taken.
DWORD THISCALL Dialog_Key(BYTE *self, DWORD key)   // 0x403e40
{
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key))
        return 1;
    switch (key) {
    case 6: case 11:
        return fn<BYTE(THISCALL *)(BYTE *)>(0x40cb70)(self);
    case 10: case 12:
        return fn<BYTE(THISCALL *)(BYTE *)>(0x40cb30)(self);
    case 13: {
        BYTE *f = bp(m(self, W_FIRST));
        if (!f)
            return 0;
        if (vc<BYTE>(f, V_ACTIVATE))
            return 1;
        BYTE *d = bp(m(self, D_DEFAULT));
        if (!d)
            return 0;
        vc(d, V_ACTIVATE);
        return 1;
    }
    case 27: {
        BYTE *c = bp(m(self, D_CANCEL));
        if (!c)
            return 0;
        vc(c, V_ACTIVATE);
        return 1;
    }
    case 0xfa: {
        BYTE *d = bp(m(self, D_DEFAULT));
        if (!d)
            return 0;
        vc(d, V_ACTIVATE);
        return 1;
    }
    default:
        return 0;
    }
}

// The dialog hands over to another (the next dialog in a sequence): the other takes this one's modality and
// its replaced window (its own deleted), this one closes (slot 5), the other goes to the front of the parent
// (slot 32, placing 6 when centred, else 0)
void THISCALL Dialog_HandOver(BYTE *self, BYTE *other, char centred)   // 0x404000
{
    mb(other, W_MODAL) = mb(self, W_MODAL) != 0;
    BYTE *mine = bp(m(self, D_REPLACED));
    drop_replaced(other);
    m(other, D_REPLACED) = pv(mine);
    if (mine)
        keep_replaced(other, mine);
    BYTE *parent = bp(m(self, W_PARENT));
    m(self, D_REPLACED) = 0;
    vc(self, V_CLOSE);
    vc(parent, V_ADD_CHILD_FRONT, other, centred ? 6 : 0, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
}

// This window gives way to a dialog: out of its parent (slot 33), the dialog in at the front (slot 32, placing
// 6 when centred), this window becomes the dialog's replaced one (hidden, its rect kept)
void THISCALL Window_ReplaceWith(BYTE *self, BYTE *dialog, char centred)   // 0x4040c0
{
    mb(dialog, W_MODAL) = mb(self, W_MODAL) != 0;
    BYTE *parent = bp(m(self, W_PARENT));
    vc(parent, V_REMOVE_CHILD, self);
    vc(parent, V_ADD_CHILD_FRONT, dialog, centred ? 6 : 0, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
    drop_replaced(dialog);
    m(dialog, D_REPLACED) = pv(self);
    keep_replaced(dialog, self);
}

// The replaced window set (the old one deleted; the new one hidden, its rect kept)
void THISCALL Dialog_SetReplaced(BYTE *self, BYTE *w)   // 0x404180
{
    drop_replaced(self);
    m(self, D_REPLACED) = pv(w);
    if (w)
        keep_replaced(self, w);
}

// ---- The message box --------------------------------------------------------------------------------------------

// The constructor (the title, the text, the parent): a frame window on the parent; the text (wrapped at 500
// pixels when wider, 0x4a7450) as a label (0x4127d0, 0x128 bytes) and the OK button (0x40f9b0 with the text of
// 0x62e520) added (0x40c7c0 with placing 1 and 12), the button takes Enter (slot 40 with 13); with a parent the
// box goes in modal and centred (slot 31 with 6) and takes the focus (slot 34)
BYTE *THISCALL MessageBox_Ctor(BYTE *self, const char *title, const char *text, BYTE *parent)   // 0x403b30
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x402d80)(self, parent, title);
    m(self, 0) = vt(0x5ea990);
    alignas(4) BYTE wrapped[4];
    fn<void(THISCALL *)(BYTE *)>(0x4dec90)(wrapped);
    BYTE *font = bp(m(self, W_FONT));
    if (fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(font, text, -1) > 500) {
        alignas(4) BYTE tmp[4];
        BYTE *res = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, int)>(0x4a7450)(bp(m(self, W_FONT)), tmp, text, 500);
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x4e18f0)(wrapped, res);
        fn<void(THISCALL *)(BYTE *)>(0x4defd0)(tmp);
        text = fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(wrapped);
    }
    using AddChild = void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, int, int, DWORD);
    BYTE *label = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x128);
    if (label)
        label = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(label, self, text);
    fn<AddChild>(0x40c7c0)(self, label, 1, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
    BYTE *ok = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x138);
    if (ok)
        ok = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(ok, self, fn<const char *(THISCALL *)(DWORD)>(0x4e13a0)(glob_addr(0x62e520)), 0);
    m(self, MB_OK) = pv(ok);
    fn<AddChild>(0x40c7c0)(self, ok, 12, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
    vc(bp(m(self, MB_OK)), 40, 13);
    if (parent) {
        vc(parent, V_ADD_CHILD, self, 6, 0, PLACE_DEFAULT, PLACE_DEFAULT, 0);
        vc(parent, V_FOCUS, self);
    }
    fn<void(THISCALL *)(BYTE *)>(0x4defd0)(wrapped);
    return self;
}

// Slot 0: the frame window's destructor, with bit 0 the pool block (0x11c)
BYTE *THISCALL MessageBox_DeletingDtor(BYTE *self, DWORD flags)   // 0x403c60
{
    frame_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x11c);
    return self;
}

// Slot 21: code 0 from the OK button closes the box (slot 5)
void THISCALL MessageBox_Notify(BYTE *self, BYTE *child, DWORD code)   // 0x403cb0
{
    if (!code && child == bp(m(self, MB_OK)))
        vc(self, V_CLOSE);
}

// Slot 16: Enter presses OK (slot 21 with the button, 0); every other key the base's (0x40c170)
DWORD THISCALL MessageBox_Key(BYTE *self, DWORD key)   // 0x403cd0
{
    if (key != 13)
        return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
    vc(self, V_NOTIFY, bp(m(self, MB_OK)), 0);
    return 1;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_frame)
{
    const Hook list[] = {
#include "ui_frame_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
