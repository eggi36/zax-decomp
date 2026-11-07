// CJScrollBar (vtable 0x5ec2ac, 0x168 bytes, "~CJScrollBar") and its variant drawn from images (0x5ec3fc, 0x190
// bytes), the scroll bars of the window toolkit (docs/DECOMPILATION.md 9, "The window toolkit").
//
// Layout [confirmed: the constructors 0x412ea0/0x4144b0 and the slots below]: the CJWindow part, then
//   +0x100 the track (the whole bar between the arrows), +0x110 the up (left) arrow, +0x120 the down (right) arrow,
//   +0x130 the thumb (rects in the bar's coordinates), +0x140 vertical (1) or horizontal (0),
//   +0x144 / +0x148 / +0x14c the range's minimum, maximum and the position (floats; 0, 1000, 0 at first),
//   +0x150 the arrows' step (1, then 10), +0x154 the page (10, then the owner's visible size),
//   +0x158 track pixels per unit, +0x15c the part pressed (0 none, 1 thumb, 2 up, 3 down, 4 page up, 5 page down),
//   +0x160 the window it scrolls (its scroll position follows the bar's; the parent itself for a window's own bars),
//   +0x164 hidden while the range is below 1
// The variant: +0x168 / +0x16c the up and down arrow images, +0x170 / +0x174 the same pressed, +0x178 the track
// image, +0x17c the track has a colour, +0x180 that colour (a FieldColor).
// The float arithmetic is the x87's at the game's precision (control word 0x27f: 53 bits), as in the original.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD SB_TRACK = 0x100, SB_UP = 0x110, SB_DOWN = 0x120, SB_THUMB = 0x130, SB_VERTICAL = 0x140, SB_MIN = 0x144,
                SB_MAX = 0x148, SB_POS = 0x14c, SB_STEP = 0x150, SB_PAGE = 0x154, SB_SCALE = 0x158, SB_PRESSED = 0x15c,
                SB_OWNER = 0x160, SB_AUTO_HIDE = 0x164;
constexpr DWORD IB_UP = 0x168, IB_DOWN = 0x16c, IB_UP_PRESSED = 0x170, IB_DOWN_PRESSED = 0x174, IB_TRACK = 0x178,
                IB_HAS_COLOUR = 0x17c, IB_COLOUR = 0x180;
constexpr int V_SB_PART = 76, V_SB_UP = 77, V_SB_DOWN = 78, V_SB_THUMB = 79, V_SB_LAYOUT = 80, V_SB_UPDATE = 81;

float &fl(BYTE *self, DWORD off) { return m<float>(self, off); }
// A value rounded to float as a store does (clang keeps x87 temporaries unrounded otherwise)
float round_float(long double v)
{
    volatile float f = (float)v;
    return f;
}
bool vertical(BYTE *self) { return m(self, SB_VERTICAL) == 1; }
bool in_rect(BYTE *self, DWORD r, int x, int y)
{
    return x >= mi(self, r) && y >= mi(self, r + 4) && x < mi(self, r + 8) && y < mi(self, r + 12);
}
// The position clamped to the maximum, then to the minimum (unordered counts as below)
void clamp_inline(BYTE *self)
{
    if (!x87_le(fl(self, SB_POS), fl(self, SB_MAX)))
        fl(self, SB_POS) = fl(self, SB_MAX);
    if (x87_lt(fl(self, SB_POS), fl(self, SB_MIN)))
        fl(self, SB_POS) = fl(self, SB_MIN);
}
// A move to nv (stored as the float tmp): nothing when nv equals the position (or is unordered with it), else the
// position becomes tmp, clamped
bool move_to(BYTE *self, long double nv, float &tmp)
{
    tmp = round_float(nv);
    if (!(nv < (long double)fl(self, SB_POS) || nv > (long double)fl(self, SB_POS)))
        return false;
    fl(self, SB_POS) = tmp;
    clamp_inline(self);
    return true;
}
// The window scrolled along: its scroll position to the truncated value
void scroll_owner(BYTE *self, float v)
{
    BYTE *owner = bp(m(self, SB_OWNER));
    if (!owner)
        return;
    if (vertical(self))
        fn<void(THISCALL *)(BYTE *, int)>(0x40d900)(owner, x87_ftol(v));
    else
        fn<void(THISCALL *)(BYTE *, int)>(0x40d8c0)(owner, x87_ftol(v));
}
// After a move by the mouse or the wheel: the thumb (slot 81), redrawn (slot 38), the owner scrolled
void moved(BYTE *self, float tmp)
{
    vc(self, V_SB_UPDATE);
    vc(self, V_INVALIDATE, 1);
    scroll_owner(self, tmp);
}
void start_repeat(BYTE *self) { fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40be70)(self, 0, 0); }
void clamp(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x414470)(self); }
DWORD palette_index(BYTE *colour)
{
    if (mb(colour, 0xc) & 1)
        return mb(colour, 0xa);
    return fn<BYTE(THISCALL *)(BYTE *)>(0x53f160)(colour);
}
void draw_button(BYTE *self, BYTE *frame, int *rect, DWORD flag, DWORD code, DWORD raised)
{
    fn<void(THISCALL *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD)>(0x4140a0)(self, frame, rect, flag, code, raised);
}
DWORD own_bar_flag(BYTE *self) { return m(self, SB_OWNER) != m(self, W_PARENT); }

}  // namespace

// The constructor (the parent, vertical): "~CJScrollBar", the range 0 .. 1000 at 0, step 1, page 10; 15 by 415
// (vertical) or 415 by 15 (0x40d510), laid out (slot 80)
BYTE *THISCALL ScrollBar_Ctor(BYTE *self, BYTE *parent, DWORD vert)   // 0x412ea0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, glob<const char *>(0x60a688));
    m(self, 0) = vt(0x5ec2ac);
    mb(self, SB_AUTO_HIDE) = 0;
    m(self, SB_PRESSED) = 0;
    m(self, SB_MIN) = 0;
    m(self, SB_POS) = 0;
    m(self, SB_OWNER) = 0;
    m(self, SB_MAX) = 0x447a0000;    // 1000.0f
    m(self, SB_STEP) = 0x3f800000;   // 1.0f
    m(self, SB_PAGE) = 0x41200000;   // 10.0f
    m(self, SB_VERTICAL) = vert;
    if (vert == 1)
        fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x40d510)(self, BAR, 0x19f);
    else
        fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x40d510)(self, 0x19f, BAR);
    vc(self, V_SB_LAYOUT);
    return self;
}

// Slot 0: CJWindow's destructor, with bit 0 the pool block (0x168)
BYTE *THISCALL ScrollBar_DeletingDtor(BYTE *self, DWORD flags)   // 0x412f50
{
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x168);
    return self;
}

// Slot 59, the size: CJWindow's (0x40d510), laid out again (slot 80)
DWORD THISCALL ScrollBar_SetSize(BYTE *self, int w, int h)   // 0x412f80
{
    fn<DWORD(THISCALL *)(BYTE *, int, int)>(0x40d510)(self, w, h);
    return vc(self, V_SB_LAYOUT);
}

// A rect (the rect, an offset) cut where the bar covers it: the bar's screen rect moved back by the offset; when they
// overlap, a vertical bar moves the rect's left edge past it (when the edge lies on the bar) or its right edge before
// it, a horizontal bar the top edge (when it is the bar's top) or the bottom edge (when that is the bar's top); each
// at most to the opposite edge. Whether it cut.
bool THISCALL ScrollBar_ClipRect(BYTE *self, int *r, const int *offset)   // 0x412fb0
{
    const int sl = mi(self, W_SCREEN) - offset[0], st = mi(self, W_SCREEN + 4) - offset[1];
    const int sr = mi(self, W_SCREEN + 8) - offset[0], sb = mi(self, W_SCREEN + 12) - offset[1];
    const int l = sl > r[0] ? sl : r[0], t = st > r[1] ? st : r[1];
    const int rr = sr < r[2] ? sr : r[2], b = sb < r[3] ? sb : r[3];
    if (l >= rr || t >= b)
        return false;
    if (!m(self, SB_VERTICAL)) {
        if (r[1] == st) {
            r[1] = sb + 1;
            if (r[1] > r[3])
                r[1] = r[3];
            return true;
        }
        if (r[3] == st) {
            r[3] = st - 1;
            if (r[3] < r[1])
                r[3] = r[1];
            return true;
        }
        return false;
    }
    if (r[0] >= sl && r[0] <= sr) {
        r[0] = sr + 1;
        if (r[0] > r[2])
            r[0] = r[2];
        return true;
    }
    if (r[2] >= sl && r[2] <= sr) {
        r[2] = sl - 1;
        if (r[2] < r[0])
            r[2] = r[0];
        return true;
    }
    return false;
}

// Slot 4: the track (slot 76 with the two colours +0xb0/+0xa0, not raised), the arrows (slots 77 and 78 with the
// font's arrow characters 0x1c/0x1f vertical, 0x1d/0x1e horizontal, raised unless pressed), the thumb (slot 79)
void THISCALL ScrollBar_Draw(BYTE *self, BYTE *frame)   // 0x4130f0
{
    int track[4] = {mi(self, SB_TRACK), mi(self, SB_TRACK + 4), mi(self, SB_TRACK + 8), mi(self, SB_TRACK + 12)};
    vc(self, V_SB_PART, frame, track, self + 0xb0, self + 0xa0, 0);
    vc(self, V_SB_UP, frame, self + SB_UP, vertical(self) ? 0x1c : 0x1f, (DWORD)(m(self, SB_PRESSED) != 2));
    vc(self, V_SB_DOWN, frame, self + SB_DOWN, m(self, SB_VERTICAL) != 1 ? 0x1e : 0x1d, (DWORD)(m(self, SB_PRESSED) != 3));
    vc(self, V_SB_THUMB, frame, self + SB_THUMB);
}

// Slot 76, a part of the bar (the frame, the rect, two colours, raised): the background as a button face
// (0x4140a0 with a space), the frame's rect outline in 0x62e5a8 (slot 57), a bevel
void THISCALL ScrollBar_DrawPart(BYTE *self, BYTE *frame, int *rect, BYTE *colour1, BYTE *colour2, DWORD raised)   // 0x4131c0
{
    draw_button(self, frame, rect, own_bar_flag(self), 0x20, raised);
    vc(frame, V_SET_RECT, rect, glob_addr(0x62e5a8));
    const DWORD c2 = palette_index(colour2);
    const DWORD c1 = palette_index(colour1);
    fn<DWORD(__fastcall *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(bp(m(self, W_FONT)), frame, rect, c1, c2, raised, 0xf, 0);
}

// Slots 77 and 78, an arrow (the frame, the rect, the character, raised): a button face with the character
DWORD THISCALL ScrollBar_DrawArrow(BYTE *self, BYTE *frame, int *rect, DWORD code, DWORD raised)   // 0x413260
{
    draw_button(self, frame, rect, own_bar_flag(self), code, raised);
    return 0;
}

// Slot 79, the thumb: a raised button face without a character
void THISCALL ScrollBar_DrawThumb(BYTE *self, BYTE *frame, int *rect)   // 0x413290
{
    draw_button(self, frame, rect, 0, 0, 1);
}

// Slot 10, a press (only the left button counts; true): on an arrow a step back or on (the repeat starts), on
// the thumb the drag starts, on the track a page back (before the thumb) or on (0x413c20)
DWORD THISCALL ScrollBar_MouseDown(BYTE *self, DWORD button, const int *point)   // 0x4132b0
{
    if (button)
        return 1;
    BYTE *s = bp(m(self, W_SCROLL));
    const int x = point[0] - mi(self, W_SCREEN) + (s ? mi(s, S_X) : 0);
    const int y = point[1] - mi(self, W_SCREEN + 4) + (s ? mi(s, S_Y) : 0);
    float tmp;
    if (in_rect(self, SB_UP, x, y)) {
        m(self, SB_PRESSED) = 2;
        if (move_to(self, (long double)fl(self, SB_POS) - fl(self, SB_STEP), tmp))
            moved(self, tmp);
        start_repeat(self);
        return 1;
    }
    if (in_rect(self, SB_DOWN, x, y)) {
        m(self, SB_PRESSED) = 3;
        if (move_to(self, (long double)fl(self, SB_STEP) + fl(self, SB_POS), tmp))
            moved(self, tmp);
        start_repeat(self);
        return 1;
    }
    if (in_rect(self, SB_THUMB, x, y)) {
        m(self, SB_PRESSED) = 1;
        return 1;
    }
    if (!in_rect(self, SB_TRACK, x, y))
        return 1;
    const DWORD dir = m(self, SB_VERTICAL);
    if ((dir == 1 && y > mi(self, SB_THUMB + 4)) || (dir == 0 && x > mi(self, SB_THUMB))) {
        m(self, SB_PRESSED) = 5;
        fn<void(THISCALL *)(BYTE *, float, DWORD)>(0x413c20)(self, (float)((long double)fl(self, SB_PAGE) + fl(self, SB_POS)), 0);
        start_repeat(self);
        return 1;
    }
    const long double nv = (long double)fl(self, SB_POS) - fl(self, SB_PAGE);
    m(self, SB_PRESSED) = 4;
    tmp = round_float(nv);
    if (nv < (long double)fl(self, SB_POS) || nv > (long double)fl(self, SB_POS)) {
        fl(self, SB_POS) = tmp;
        clamp(self);
        moved(self, tmp);
    }
    start_repeat(self);
    return 1;
}

// Slot 11, a release of the left button: the press ends (true when something was pressed)
DWORD THISCALL ScrollBar_MouseUp(BYTE *self, DWORD button, const int *)   // 0x413650
{
    if (button || !m(self, SB_PRESSED))
        return 0;
    m(self, SB_PRESSED) = 0;
    return 1;
}

// Slot 13, the mouse moved: dragging the thumb moves the position by the movement along the bar over the scale,
// under the bar's drag cursor (10 vertical, 8 horizontal); false when not dragging
DWORD THISCALL ScrollBar_MouseMove(BYTE *self, const int *, const int *delta, DWORD *cursor_out)   // 0x413680
{
    if (m(self, SB_PRESSED) != 1)
        return 0;
    const int d = m(self, SB_VERTICAL) == 1 ? delta[1] : delta[0];
    const long double nv = (long double)*glob<float *>(0x5ec3f4) / fl(self, SB_SCALE) * d + fl(self, SB_POS);
    float tmp;
    if (move_to(self, nv, tmp))
        moved(self, tmp);
    *cursor_out = fn<DWORD(__fastcall *)(int)>(0x40e9e0)(m(self, SB_VERTICAL) == 1 ? 10 : 8);
    return 1;
}

// Slot 15, the wheel: the position moves back by delta * step * 12 (0x5ec3f8); al 1
bool THISCALL ScrollBar_Wheel(BYTE *self, int delta, DWORD)   // 0x4137a0
{
    const long double nv = (long double)fl(self, SB_POS) - (long double)delta * fl(self, SB_STEP) * *glob<float *>(0x5ec3f8);
    float tmp;
    if (move_to(self, nv, tmp))
        moved(self, tmp);
    return true;
}

// Slot 69, the cursor at a point: the part's cursor (thumb 9/7, up 15/19, down 17/21, page up 16/20, page down
// 18/22 for vertical/horizontal), elsewhere the base's (0x40ea00)
DWORD THISCALL ScrollBar_Cursor(BYTE *self, const int *point)   // 0x413870
{
    const bool v = m(self, SB_VERTICAL) == 1;
    int c;
    switch (fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x413fb0)(self, point)) {
    case 1: c = v ? 9 : 7; break;
    case 2: c = v ? 0xf : 0x13; break;
    case 3: c = v ? 0x11 : 0x15; break;
    case 4: c = v ? 0x10 : 0x14; break;
    case 5: c = v ? 0x12 : 0x16; break;
    default: return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x40ea00)(self, point);
    }
    return fn<DWORD(__fastcall *)(int)>(0x40e9e0)(c);
}

// Slot 57, the rect: CJWindow's (0x40d2a0), laid out (slot 80); a window's own bar keeps to its parent's edge:
// a vertical one below the title bar (slot 60) and right of the client width (0x40d490; the left edge moved by the
// width difference and slot 61), as high as the client height (0x40d4d0); a horizontal one right of the border
// (slot 61) and below the client height, its height set to the client width (as the original does); then the range
// from the parent (0x413ce0)
void THISCALL ScrollBar_SetRect(BYTE *self, int *rect)   // 0x413940
{
    fn<void(THISCALL *)(BYTE *, int *)>(0x40d2a0)(self, rect);
    vc(self, V_SB_LAYOUT);
    BYTE *owner = bp(m(self, SB_OWNER));
    if (!owner || owner != bp(m(self, W_PARENT)))
        return;
    auto sx = [&]() { BYTE *s = bp(m(bp(m(self, W_PARENT)), W_SCROLL)); return s ? mi(s, S_X) : 0; };
    auto sy = [&]() { BYTE *s = bp(m(bp(m(self, W_PARENT)), W_SCROLL)); return s ? mi(s, S_Y) : 0; };
    using IntFn = int(THISCALL *)(BYTE *);
    using SetFn = DWORD(THISCALL *)(BYTE *, int);
    DWORD size;
    if (m(self, SB_VERTICAL) == 1) {
        int y0 = sy();
        if (mi(self, W_RECT + 4) != vc<int>(self, V_BORDER_H) + y0) {
            const int y1 = sy();
            fn<SetFn>(0x40d6c0)(self, vc<int>(self, V_BORDER_H) + y1);
        }
        int x0 = sx();
        if (mi(self, W_RECT + 8) != fn<IntFn>(0x40d490)(bp(m(self, W_PARENT))) + x0) {
            const int x1 = sx();
            if (mi(self, W_RECT) != fn<IntFn>(0x40d490)(bp(m(self, W_PARENT))) + x1) {
                BYTE *parent = bp(m(self, W_PARENT));
                const int x2 = sx();
                const int width = mi(self, W_RECT + 8) - mi(self, W_RECT);
                const int bw = vc<int>(self, V_BORDER_W);
                const int cw = fn<IntFn>(0x40d490)(parent);
                fn<SetFn>(0x40d680)(self, bw + cw + (x2 - width));
            }
        }
        if (mi(self, W_RECT + 12) - mi(self, W_RECT + 4) == fn<IntFn>(0x40d4d0)(bp(m(self, W_PARENT))))
            return;
        size = (DWORD)fn<IntFn>(0x40d4d0)(bp(m(self, W_PARENT)));
    } else {
        int x0 = sx();
        if (mi(self, W_RECT) != vc<int>(self, V_BORDER_W) + x0) {
            const int x1 = sx();
            fn<SetFn>(0x40d680)(self, vc<int>(self, V_BORDER_W) + x1);
        }
        int y0 = sy();
        if (mi(self, W_RECT + 12) != fn<IntFn>(0x40d4d0)(bp(m(self, W_PARENT))) + y0) {
            const int y1 = sy();
            if (mi(self, W_RECT + 4) != fn<IntFn>(0x40d4d0)(bp(m(self, W_PARENT))) + y1) {
                BYTE *parent = bp(m(self, W_PARENT));
                const int y2 = sy();
                const int width = mi(self, W_RECT + 8) - mi(self, W_RECT);
                const int bh = vc<int>(self, V_BORDER_H);
                const int ch = fn<IntFn>(0x40d4d0)(parent);
                fn<SetFn>(0x40d6c0)(self, bh + ch + (y2 - width));
            }
        }
        if (mi(self, W_RECT + 8) - mi(self, W_RECT) == fn<IntFn>(0x40d490)(bp(m(self, W_PARENT))))
            return;
        size = (DWORD)fn<IntFn>(0x40d490)(bp(m(self, W_PARENT)));
    }
    fn<SetFn>(0x40f5d0)(self, (int)size);
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x413ce0)(self, bp(m(self, SB_OWNER)));
}

// The range (minimum, maximum as floats): the thumb (slot 81), the position clamped
void THISCALL ScrollBar_SetRange(BYTE *self, float lo, float hi)   // 0x413bb0
{
    fl(self, SB_MAX) = hi;
    fl(self, SB_MIN) = lo;
    vc(self, V_SB_UPDATE);
    clamp_inline(self);
}

// The position (a float, quiet): when it changes, clamped, the thumb (slot 81); unless quiet redrawn and the owner
// scrolled
void THISCALL ScrollBar_SetPos(BYTE *self, float v, DWORD quiet)   // 0x413c20
{
    if (!(v < fl(self, SB_POS) || v > fl(self, SB_POS)))
        return;
    fl(self, SB_POS) = v;
    clamp_inline(self);
    vc(self, V_SB_UPDATE);
    if (quiet)
        return;
    vc(self, V_INVALIDATE, 1);
    scroll_owner(self, v);
}

// The range from a window (the owner): its content size beyond its height (vertical; width horizontal) from 0, the
// position its scroll position (a static zero point 0x716188 without a scroll info), the page its height, the step
// 10. A vertical bar first pulls a scroll position beyond the end back (0x40d900); without content beyond the
// height its range is 0 and the position goes to 0 (redrawn, the bar's own owner scrolled).
void THISCALL ScrollBar_SetRangeFrom(BYTE *self, BYTE *owner)   // 0x413ce0
{
    using SetFn = void(THISCALL *)(BYTE *, int);
    auto zero_point = [&]() -> int * {
        if (!(gb(glob_addr(0x716180)) & 1)) {
            gd(glob_addr(0x716188)) = 0;
            gb(glob_addr(0x716180)) |= 1;
            gd(glob_addr(0x71618c)) = 0;
            fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x401270));
        }
        BYTE *s = bp(m(owner, W_SCROLL));
        return s ? reinterpret_cast<int *>(s) : glob<int *>(0x716188);
    };
    long double page;
    if (m(self, SB_VERTICAL) == 1) {
        BYTE *s = bp(m(owner, W_SCROLL));
        if (s && m(s, S_H)) {
            const int ch = mi(s, S_H);
            if (mi(s, S_Y) - mi(owner, W_RECT + 4) + mi(owner, W_RECT + 12) > ch)
                fn<SetFn>(0x40d900)(owner, mi(owner, W_RECT + 4) - mi(owner, W_RECT + 12) + ch);
        }
        s = bp(m(owner, W_SCROLL));
        const DWORD ch = s ? m(s, S_H) : 0;
        const int h = mi(owner, W_RECT + 12) - mi(owner, W_RECT + 4);
        if (ch <= (DWORD)h) {
            m(self, SB_MIN) = 0;
            m(self, SB_MAX) = 0;
            vc(self, V_SB_UPDATE);
            clamp(self);
            if (!(*glob<float *>(0x5ebed0) < fl(self, SB_POS) || *glob<float *>(0x5ebed0) > fl(self, SB_POS)))
                return;
            m(self, SB_POS) = 0;
            clamp(self);
            vc(self, V_SB_UPDATE);
            vc(self, V_INVALIDATE, 1);
            if (BYTE *own = bp(m(self, SB_OWNER))) {
                if (m(self, SB_VERTICAL) == 1)
                    fn<SetFn>(0x40d900)(own, 0);
                else
                    fn<SetFn>(0x40d8c0)(own, 0);
            }
            return;
        }
        m(self, SB_MIN) = 0;
        fl(self, SB_MAX) = (float)((long double)(unsigned long long)ch - h);
        vc(self, V_SB_UPDATE);
        clamp(self);
        const long double v = (long double)zero_point()[1];
        if (v < (long double)fl(self, SB_POS) || v > (long double)fl(self, SB_POS)) {
            fl(self, SB_POS) = (float)v;
            clamp(self);
            vc(self, V_SB_UPDATE);
        }
        page = (long double)(mi(owner, W_RECT + 12) - mi(owner, W_RECT + 4));
    } else {
        BYTE *s = bp(m(owner, W_SCROLL));
        const DWORD cw = s ? m(s, S_W) : 0;
        const int w = mi(owner, W_RECT + 8) - mi(owner, W_RECT);
        m(self, SB_MIN) = 0;
        fl(self, SB_MAX) = (float)((long double)(unsigned long long)cw - w);
        vc(self, V_SB_UPDATE);
        clamp_inline(self);
        const long double v = (long double)zero_point()[0];
        if (v < (long double)fl(self, SB_POS) || v > (long double)fl(self, SB_POS)) {
            fl(self, SB_POS) = (float)v;
            clamp(self);
            vc(self, V_SB_UPDATE);
        }
        page = (long double)(mi(owner, W_RECT + 8) - mi(owner, W_RECT));
    }
    fl(self, SB_PAGE) = (float)page;
    m(self, SB_STEP) = 0x41200000;   // 10.0f
}

// The part at a point (bar coordinates): 2 up, 3 down, 1 thumb, on the track 4 before the thumb or 5 after it, 0
DWORD THISCALL ScrollBar_PartAt(BYTE *self, const int *point)   // 0x413fb0
{
    const int x = point[0], y = point[1];
    if (in_rect(self, SB_UP, x, y))
        return 2;
    if (in_rect(self, SB_DOWN, x, y))
        return 3;
    if (in_rect(self, SB_THUMB, x, y))
        return 1;
    if (!in_rect(self, SB_TRACK, x, y))
        return 0;
    const DWORD dir = m(self, SB_VERTICAL);
    if (dir == 1 && y > mi(self, SB_THUMB + 4))
        return 5;
    if (dir == 0 && x > mi(self, SB_THUMB))
        return 5;
    return 4;
}

// A button face (the frame, the rect, from the parent's texture, a character, raised): unless transparent the
// texture (slot 72) through the frame's slot 23, from the window's own position when `from_parent`; the bevel
// (0x40a740, the raised flag as its sunken argument); the character centred (0x4a6470), one pixel further when
// pressed. Nothing for an empty rect.
void THISCALL ScrollBar_DrawButton(BYTE *self, BYTE *frame, const int *rect, BYTE from_parent, DWORD code, DWORD raised)   // 0x4140a0
{
    const int off = raised ? 0 : 1;
    int r[4] = {rect[0], rect[1], rect[2], rect[3]};
    if (r[2] <= r[0] || r[3] <= r[1])
        return;
    int dst[4] = {rect[0] + off, rect[1] + off, rect[2], rect[3]};
    int src[4] = {0, 0, rect[2] - (rect[0] + off), rect[3] - (rect[1] + off)};
    if (from_parent) {
        src[1] = rect[1] + mi(self, W_RECT + 4);
        src[0] = mi(self, W_RECT) + rect[0];
        src[2] += src[0];
        src[3] += src[1];
    }
    if (!mb(self, W_TRANSPARENT)) {
        const DWORD texture = vc(self, V_TEXTURE);
        vc(frame, 23, texture, src, dst, glob_addr(0x6bc4e0));
    }
    const DWORD c1 = palette_index(self + 0xb0);
    const DWORD c2 = palette_index(self + 0xa0);
    fn<DWORD(__fastcall *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(bp(m(self, W_FONT)), frame, r, c2, c1, raised, 0xf, 0);
    if (!code)
        return;
    const char text[2] = {(char)code, 0};
    const int x = (int)((DWORD)(r[2] - r[0]) >> 1) + r[0] + off;
    fn<void(THISCALL *)(BYTE *, BYTE *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(bp(m(self, W_FONT)), frame, x, r[1] + off + 2, text, 1, 0);
}

// Slot 80, the layout: the track the whole bar less a square arrow (the bar's thickness) at each end, the arrows in
// those squares; the thumb (slot 81)
void THISCALL ScrollBar_Layout(BYTE *self)   // 0x414240
{
    const int w = mi(self, W_RECT + 8) - mi(self, W_RECT), h = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
    mi(self, SB_TRACK + 8) = w;
    mi(self, SB_TRACK) = 0;
    mi(self, SB_TRACK + 4) = 0;
    mi(self, SB_TRACK + 12) = h;
    int s;
    if (m(self, SB_VERTICAL) == 1) {
        s = mi(self, W_RECT + 8) - mi(self, W_RECT);
        mi(self, SB_TRACK + 4) = s;
        mi(self, SB_TRACK + 12) += mi(self, W_RECT) - mi(self, W_RECT + 8);
    } else {
        s = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
        mi(self, SB_TRACK) += s;
        mi(self, SB_TRACK + 8) += mi(self, W_RECT + 4) - mi(self, W_RECT + 12);
    }
    mi(self, SB_UP) = 0, mi(self, SB_UP + 4) = 0, mi(self, SB_UP + 8) = s, mi(self, SB_UP + 12) = s;
    const int ww = mi(self, W_RECT + 8) - mi(self, W_RECT), hh = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
    mi(self, SB_DOWN + 4) = hh - s;
    mi(self, SB_DOWN) = ww - s;
    mi(self, SB_DOWN + 8) = ww;
    mi(self, SB_DOWN + 12) = hh;
    vc(self, V_SB_UPDATE);
}

// Slot 81, the thumb: over the track, len * len / (len + range) long (truncated, at least 4), at
// (position - minimum) * scale with scale = (len - thumb) / range (kept at +0x158); an auto-hiding bar shows
// (slot 26) when the range is at least 1
void THISCALL ScrollBar_UpdateThumb(BYTE *self)   // 0x414340
{
    for (DWORD o = 0; o < 16; o += 4)
        m(self, SB_THUMB + o) = m(self, SB_TRACK + o);
    const float range = round_float((long double)fl(self, SB_MAX) - fl(self, SB_MIN));
    if (mb(self, SB_AUTO_HIDE))
        vc(self, 26, (DWORD)(x87_lt(range, *glob<float *>(0x5ec3f4)) ? 0 : 1), 0);
    const DWORD len = m(self, SB_VERTICAL) == 1 ? m(self, SB_TRACK + 12) - m(self, SB_TRACK + 4) : m(self, SB_TRACK + 8) - m(self, SB_TRACK);
    const long double l = (long double)(unsigned long long)len;
    int thumb = x87_ftol(l / (l + range) * l);
    if (thumb < 4)
        thumb = 4;
    const long double scale = (long double)(unsigned long long)(DWORD)(len - (DWORD)thumb) / range;
    fl(self, SB_SCALE) = (float)scale;
    const int off = x87_ftol(((long double)fl(self, SB_POS) - fl(self, SB_MIN)) * scale);
    if (m(self, SB_VERTICAL) == 1) {
        mi(self, SB_THUMB + 4) += off;
        mi(self, SB_THUMB + 12) = mi(self, SB_THUMB + 4) + thumb;
    } else {
        mi(self, SB_THUMB) += off;
        mi(self, SB_THUMB + 8) = mi(self, SB_THUMB) + thumb;
    }
}

// The position clamped to the maximum, then the minimum
void THISCALL ScrollBar_Clamp(BYTE *self)   // 0x414470
{
    clamp_inline(self);
}

// ---- The image scroll bar ----------------------------------------------------------------------------------------

// The constructor: the scroll bar's, a colour (0x53f000), no images, no track colour
BYTE *THISCALL ImageScrollBar_Ctor(BYTE *self, BYTE *parent, DWORD vert)   // 0x4144b0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x412ea0)(self, parent, vert);
    fn<void(THISCALL *)(BYTE *)>(0x53f000)(self + IB_COLOUR);
    m(self, 0) = vt(0x5ec3fc);
    mb(self, IB_HAS_COLOUR) = 0;
    for (DWORD o = IB_UP; o <= IB_TRACK; o += 4)
        m(self, o) = 0;
    return self;
}

// Slot 0: the images forgotten, CJWindow's destructor, with bit 0 the pool block (0x190)
BYTE *THISCALL ImageScrollBar_DeletingDtor(BYTE *self, DWORD flags)   // 0x414510
{
    m(self, 0) = vt(0x5ec3fc);
    for (DWORD o = IB_UP; o <= IB_TRACK; o += 4)
        m(self, o) = 0;
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x190);
    return self;
}

// The track colour (copied field by field: bytes 7 and 11 stay)
void THISCALL ImageScrollBar_SetColour(BYTE *self, const BYTE *colour)   // 0x414560
{
    mb(self, IB_HAS_COLOUR) = 1;
    m(self, IB_COLOUR) = *reinterpret_cast<const DWORD *>(colour);
    m<WORD>(self, IB_COLOUR + 4) = *reinterpret_cast<const WORD *>(colour + 4);
    mb(self, IB_COLOUR + 6) = colour[6];
    m<WORD>(self, IB_COLOUR + 8) = *reinterpret_cast<const WORD *>(colour + 8);
    mb(self, IB_COLOUR + 10) = colour[10];
    m(self, IB_COLOUR + 12) = *reinterpret_cast<const DWORD *>(colour + 12);
}

// Slot 76: with a track colour the frame's rect in it (0x4b3430); else nothing
void THISCALL ImageScrollBar_DrawPart(BYTE *self, BYTE *frame, int *rect, BYTE *, BYTE *, DWORD)   // 0x4145b0
{
    if (mb(self, IB_HAS_COLOUR))
        fn<void(THISCALL *)(BYTE *, int *, BYTE *)>(0x4b3430)(frame, rect, self + IB_COLOUR);
}

void THISCALL ImageScrollBar_SetUpImages(BYTE *self, DWORD normal, DWORD pressed)   // 0x4145e0
{
    m(self, IB_UP) = normal;
    m(self, IB_UP_PRESSED) = pressed;
}

void THISCALL ImageScrollBar_SetDownImages(BYTE *self, DWORD normal, DWORD pressed)   // 0x414600
{
    m(self, IB_DOWN) = normal;
    m(self, IB_DOWN_PRESSED) = pressed;
}

void THISCALL ImageScrollBar_SetTrackImage(BYTE *self, DWORD image)   // 0x414620
{
    m(self, IB_TRACK) = image;
}

namespace {

// An arrow from its image (pressed: the pressed one if there is one) at the rect's corner (0x4b3280 with the
// transparent mask 0x6bcc68), else drawn as the scroll bar's
DWORD image_arrow(BYTE *self, BYTE *frame, int *rect, DWORD code, DWORD raised, DWORD normal, DWORD pressed)
{
    DWORD img = raised ? m(self, normal) : m(self, pressed);
    if (!img)
        img = m(self, normal);
    if (img)
        return fn<DWORD(THISCALL *)(BYTE *, DWORD, int, int, DWORD)>(0x4b3280)(frame, img, rect[0], rect[1], glob_addr(0x6bcc68));
    draw_button(self, frame, rect, own_bar_flag(self), code, raised);
    return 0;
}

}  // namespace

// Slots 77 and 78 of the image scroll bar
DWORD THISCALL ImageScrollBar_DrawUp(BYTE *self, BYTE *frame, int *rect, DWORD code, DWORD raised)   // 0x414630
{
    return image_arrow(self, frame, rect, code, raised, IB_UP, IB_UP_PRESSED);
}

DWORD THISCALL ImageScrollBar_DrawDown(BYTE *self, BYTE *frame, int *rect, DWORD code, DWORD raised)   // 0x4146a0
{
    return image_arrow(self, frame, rect, code, raised, IB_DOWN, IB_DOWN_PRESSED);
}

// Slot 79, the thumb: the track image stretched over it (the frame's slot 23 with 0x6bc4a8), else the scroll bar's
void THISCALL ImageScrollBar_DrawThumb(BYTE *self, BYTE *frame, int *rect)   // 0x414710
{
    const DWORD img = m(self, IB_TRACK);
    if (!img) {
        draw_button(self, frame, rect, 0, 0, 1);
        return;
    }
    int r[4] = {rect[0], rect[1], rect[2], rect[3]};
    if (r[2] <= r[0] || r[3] <= r[1])
        return;
    int src[4] = {0, 0, r[2] - r[0], r[3] - r[1]};
    vc(frame, 23, img, src, r, glob_addr(0x6bc4a8));
}

// Slot 80, the layout: with both arrow images the arrows as large as their images (the frame's slots 45 width and
// 39 height) at the ends and the track between them; else the scroll bar's (0x414240)
void THISCALL ImageScrollBar_Layout(BYTE *self)   // 0x4147a0
{
    if (!m(self, IB_UP) || !m(self, IB_DOWN)) {
        fn<void(THISCALL *)(BYTE *)>(0x414240)(self);
        return;
    }
    const int h = mi(self, W_RECT + 12) - mi(self, W_RECT + 4), w = mi(self, W_RECT + 8) - mi(self, W_RECT);
    m(self, SB_TRACK) = 0;
    m(self, SB_TRACK + 4) = 0;
    mi(self, SB_TRACK + 8) = w;
    mi(self, SB_TRACK + 12) = h;
    BYTE *up = bp(m(self, IB_UP));
    if (m(self, SB_VERTICAL) == 1) {
        mi(self, SB_TRACK + 4) += vc<int>(up, 39);
        mi(self, SB_TRACK + 12) -= vc<int>(bp(m(self, IB_DOWN)), 39);
    } else {
        mi(self, SB_TRACK) += vc<int>(up, 45);
        mi(self, SB_TRACK + 8) -= vc<int>(bp(m(self, IB_DOWN)), 45);
    }
    const int uh = vc<int>(bp(m(self, IB_UP)), 39);
    const int uw = vc<int>(bp(m(self, IB_UP)), 45);
    mi(self, SB_UP) = 0, mi(self, SB_UP + 4) = 0, mi(self, SB_UP + 8) = uw, mi(self, SB_UP + 12) = uh;
    const int ww = mi(self, W_RECT + 8) - mi(self, W_RECT), hh = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
    const int dh = vc<int>(bp(m(self, IB_DOWN)), 39);
    const int dw = vc<int>(bp(m(self, IB_DOWN)), 45);
    mi(self, SB_DOWN) = ww - dw;
    mi(self, SB_DOWN + 4) = hh - dh;
    mi(self, SB_DOWN + 8) = ww;
    mi(self, SB_DOWN + 12) = hh;
    vc(self, V_SB_UPDATE);
}

// Slot 81: the scroll bar's (a jump to 0x414340)
void THISCALL ImageScrollBar_UpdateThumb(BYTE *self)   // 0x414900
{
    fn<void(THISCALL *)(BYTE *)>(0x414340)(self);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_scrollbar)
{
    const Hook list[] = {
#include "ui_scrollbar_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
