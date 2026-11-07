// The buttons of the window toolkit (docs/DECOMPILATION.md 9, "The window toolkit"): the resize grip
// ("Resize Control", vtable 0x5eb72c, 0x104 bytes), the button (0x5eb85c, 0x138 bytes) and its two toggling
// variants (0x5ebaec: a radio button that stays down in its group; 0x5ebc44: a toggle).
//
// Button layout [confirmed: the init 0x40faa0, the destructor 0x40fb80 and the slots below]: the CJWindow part, then
//   +0x100 pressed (the left button went down on it), +0x101 the mouse is over it, +0x102 clicked (taken by slot
//   80), +0x103 clicked while slot 28 answered true, +0x104 checked, +0x108 slot 77's value (1 by default),
//   +0x10c the next button of its radio group (a ring; 0 alone), +0x110 an image (a CFrame), +0x114 slot 0x4100f0's
//   value, +0x118 and +0x11a the image flags (+0x11a: the image is all, no frame and no border), +0x119 the image
//   is its own (deleted with it), +0x11c the tool tip (ZString), +0x120 its accelerator index (-1 none), +0x124
//   repeats while held (0x40be70), +0x128 a callback (slot 1 on a click, slot 2 the enabled state), +0x12c,
//   +0x130 a polygon for the hit test (else the rect), +0x134 the look (made by the look factory at +0x4c; slot 1
//   draws, slot 2 updates)
// The resize grip: +0x100 the edge being dragged (set by the frame window 0x403260).
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD B_PRESSED = 0x100, B_INSIDE = 0x101, B_CLICKED = 0x102, B_CLICKED_28 = 0x103, B_CHECKED = 0x104,
                B_TARGET = 0x108, B_GROUP = 0x10c, B_IMAGE = 0x110, B_114 = 0x114, B_IMAGE_118 = 0x118,
                B_IMAGE_OWNED = 0x119, B_IMAGE_ONLY = 0x11a, B_TIP = 0x11c, B_TIP_ACCEL = 0x120, B_REPEAT = 0x124,
                B_CALLBACK = 0x128, B_12C = 0x12c, B_SHAPE = 0x130, B_LOOK = 0x134;
// The button's slots
constexpr int V_UPDATE_LOOK = 76, V_SET_CHECKED = 78, V_CLICK = 81, V_DO_CLICK = 82, V_HIT = 83, V_CLICK_SOUND = 84, V_HOVER_SOUND = 85;

DWORD cursor(int i) { return fn<DWORD(__fastcall *)(int)>(0x40e9e0)(i); }
void pool_delete(BYTE *p, DWORD size) { fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(p, size); }
void zstring_ctor(BYTE *z) { fn<void(THISCALL *)(BYTE *)>(0x4dec90)(z); }
// A button made: the base constructor (text 0x40a8a0 or ZString 0x40a940), the tool tip, the button's vtable,
// the init
template <typename Title> BYTE *construct(BYTE *self, BYTE *parent, Title title, BYTE *partner, DWORD base, DWORD vtable)
{
    fn_dyn<BYTE *(THISCALL *)(BYTE *, BYTE *, Title)>(base)(self, parent, title);
    zstring_ctor(self + B_TIP);
    m(self, 0) = vt(0x5eb85c);
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40faa0)(self, partner);
    m(self, 0) = vtable;
    return self;
}
// The rest of a click (0x410180's body, inlined by the toggles): clicked, slot 28's answer noted, the callback
// (slot 1), redrawn (slot 38)
void finish_click(BYTE *self)
{
    mb(self, B_CLICKED) = 1;
    if (vc(self, V_28))
        mb(self, B_CLICKED_28) = 1;
    if (BYTE *cb = bp(m(self, B_CALLBACK)))
        vc(cb, 1);
    vc(self, V_INVALIDATE, 0);
}
// A toggle's click: the sound, the checked state flipped (slot 78), the rest of a click
void toggle_click(BYTE *self)
{
    vc(self, V_CLICK_SOUND);
    vc(self, V_SET_CHECKED, (DWORD)(m(self, B_CHECKED) == 0));
    finish_click(self);
}

}  // namespace

// ---- The resize grip ---------------------------------------------------------------------------------------

// The constructor: "Resize Control" on the parent, as wide and high as the font's character 0x19 (0x4a6290)
BYTE *THISCALL Grip_Ctor(BYTE *self, BYTE *parent)   // 0x40f870
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, glob<const char *>(0x60a64c));
    m(self, 0) = vt(0x5eb72c);
    const DWORD w = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4a6290)(bp(m(self, W_FONT)), 0x19);
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, w, w);
    m(self, 0x100) = 0;
    return self;
}

// Slot 0: CJWindow's destructor, with bit 0 the pool block (0x104)
BYTE *THISCALL Grip_DeletingDtor(BYTE *self, DWORD flags)   // 0x40f8b0
{
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        pool_delete(self, 0x104);
    return self;
}

// Slot 4: the font's character 0x19 at 0, 0 (the font's text drawing 0x4a6470)
void THISCALL Grip_Draw(BYTE *self, BYTE *frame)   // 0x40f8e0
{
    const char text[2] = {0x19, 0};
    fn<void(THISCALL *)(BYTE *, BYTE *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(bp(m(self, W_FONT)), frame, 0, 0, text, 0, 0);
}

// Slot 14, a drag with the left button: the parent (a frame window) grows by the movement at its bottom right
// (0x403720 with edge 12) under the diagonal cursor; true. Else false.
bool THISCALL Grip_MouseDrag(BYTE *self, int *point, const int *delta, DWORD buttons, DWORD *cursor_out)   // 0x40f910
{
    if (!(buttons & 1))
        return false;
    *cursor_out = cursor(12);
    const int *r = vc<const int *>(bp(m(self, W_PARENT)), V_GET_RECT);
    int rect[4] = {r[0], r[1], r[2] + delta[0], r[3] + delta[1]};
    fn<void(THISCALL *)(BYTE *, int *, DWORD, int *)>(0x403720)(bp(m(self, W_PARENT)), rect, 12, point);
    return true;
}

// Slot 69: the diagonal cursor
DWORD THISCALL Grip_Cursor(BYTE *, const int *)   // 0x40f9a0
{
    return cursor(11);
}

// ---- The button ----------------------------------------------------------------------------------------------

// The constructors (the parent, the title as text or ZString, a button of the radio group to join or null)
BYTE *THISCALL Button_Ctor(BYTE *self, BYTE *parent, const char *title, BYTE *partner)   // 0x40f9b0
{
    return construct(self, parent, title, partner, fn_addr(0x40a8a0), vt(0x5eb85c));
}

BYTE *THISCALL Button_CtorString(BYTE *self, BYTE *parent, BYTE *title, BYTE *partner)   // 0x40fa60
{
    return construct(self, parent, title, partner, fn_addr(0x40a940), vt(0x5eb85c));
}

// Slot 77
void THISCALL Button_SetTarget(BYTE *self, DWORD v)   // 0x40f9f0
{
    m(self, B_TARGET) = v;
}

// Slot 18 (activated by key): enabled, a click (slot 81)
bool THISCALL Button_Activate(BYTE *self)   // 0x40fa00
{
    if (!mb(self, W_ENABLED))
        return false;
    vc(self, V_CLICK);
    return true;
}

// Slot 79
DWORD THISCALL Button_Checked(BYTE *self)   // 0x40fa20
{
    return m(self, B_CHECKED);
}

// Slot 0: the destructor, with bit 0 the pool block (0x138)
BYTE *THISCALL Button_DeletingDtor(BYTE *self, DWORD flags)   // 0x40fa30
{
    fn<void(THISCALL *)(BYTE *)>(0x40fb80)(self);
    if (flags & 1)
        pool_delete(self, 0x138);
    return self;
}

// The init: nothing pressed or checked, no image, callback, shape or repeat, framed, the look from the look factory
// (slot 1) and updated (slot 76); joining a radio group takes the partner's tab stop (+0x5d, +0x5f), alone the
// button has it; the ring: the button after the partner
void THISCALL Button_Init(BYTE *self, BYTE *partner)   // 0x40faa0
{
    mb(self, B_INSIDE) = 0;
    mb(self, B_IMAGE_118) = 0;
    m(self, B_CALLBACK) = 0;
    mb(self, B_REPEAT) = 0;
    m(self, B_IMAGE) = 0;
    m(self, B_114) = 0;
    mb(self, B_IMAGE_OWNED) = 0;
    mb(self, B_IMAGE_ONLY) = 0;
    m(self, B_TARGET) = 1;
    mb(self, B_12C) = 0;
    mb(self, W_FRAMED) = 1;
    m(self, B_SHAPE) = 0;
    m(self, B_LOOK) = vc(bp(m(self, W_LOOK)), 1, self);
    vc(self, V_UPDATE_LOOK);
    if (partner) {
        mb(partner, W_TAB_STOP) = 0;
        mb(partner, W_5F) = 0;
    } else {
        mb(self, W_TAB_STOP) = 1;
        mb(self, W_5F) = 1;
    }
    mb(self, W_5E) = 1;
    mb(self, B_PRESSED) = 0;
    mb(self, B_CLICKED) = 0;
    mb(self, B_CLICKED_28) = 0;
    m(self, B_CHECKED) = 0;
    if (partner) {
        m(self, B_GROUP) = m(partner, B_GROUP);
        m(partner, B_GROUP) = pv(self);
        if (!m(self, B_GROUP))
            m(self, B_GROUP) = pv(partner);
    } else {
        m(self, B_GROUP) = 0;
    }
}

// The destructor: the callback (slot 0) and the shape (0x545cf0, operator delete), an own image (slot 1), the
// look (slot 0), the tool tip, CJWindow's destructor
void THISCALL Button_Dtor(BYTE *self)   // 0x40fb80
{
    m(self, 0) = vt(0x5eb85c);
    if (BYTE *cb = bp(m(self, B_CALLBACK))) {
        vc(cb, 0, 1);
        m(self, B_CALLBACK) = 0;
    }
    if (BYTE *shape = bp(m(self, B_SHAPE))) {
        fn<void(THISCALL *)(BYTE *)>(0x545cf0)(shape);
        fn<void(__cdecl *)(void *)>(0x5d0320)(shape);
        m(self, B_SHAPE) = 0;
    }
    BYTE *image = bp(m(self, B_IMAGE));
    if (image && mb(self, B_IMAGE_OWNED))
        vc(image, 1, 1);
    m(self, B_IMAGE) = 0;
    mb(self, B_IMAGE_OWNED) = 0;
    if (BYTE *look = bp(m(self, B_LOOK))) {
        vc(look, 0, 1);
        m(self, B_LOOK) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x4defd0)(self + B_TIP);
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
}

// Slot 4: the look draws (slot 1), the click mark +0x103 goes, the children (0x40b620)
void THISCALL Button_Draw(BYTE *self, BYTE *frame)   // 0x40fc20
{
    vc(bp(m(self, B_LOOK)), 1, frame);
    mb(self, B_CLICKED_28) = 0;
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
}

// Slot 76: the look updates (its slot 2; a jump, the look's eax)
DWORD THISCALL Button_UpdateLook(BYTE *self)   // 0x40fc50
{
    return vc(bp(m(self, B_LOOK)), 2);
}

// Slot 10, a press: the left button on the button (slot 83) presses it (starting the repeat when it repeats)
DWORD THISCALL Button_MouseDown(BYTE *self, DWORD button, const int *point)   // 0x40fc60
{
    if (button || !vc<BYTE>(self, V_HIT, point))
        return 0;
    mb(self, B_PRESSED) = 1;
    mb(self, B_INSIDE) = 1;
    if (mb(self, B_REPEAT))
        fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40be70)(self, 0, 0);
    return 1;
}

// Slot 11, a release of the left button after a press: released over it a click (slot 81) unless the repeat
// already fired (0x40bec0)
DWORD THISCALL Button_MouseUp(BYTE *self, DWORD button, const int *)   // 0x40fcb0
{
    if (button || !mb(self, B_PRESSED))
        return 0;
    const BYTE inside = mb(self, B_INSIDE);
    mb(self, B_PRESSED) = 0;
    if (inside && !fn<BYTE(THISCALL *)(BYTE *)>(0x40bec0)(self))
        vc(self, V_CLICK);
    return 1;
}

// Slot 12: a click (slot 81)
DWORD THISCALL Button_Slot12(BYTE *self, DWORD, DWORD)   // 0x40fd00
{
    return vc(self, V_CLICK);
}

// Slot 13, the mouse moved: over it (in the visible rect and slot 83); entering an enabled button plays the hover
// sound (slot 85). 0.
DWORD THISCALL Button_MouseMove(BYTE *self, const int *point, DWORD, DWORD)   // 0x40fd10
{
    const BYTE was = mb(self, B_INSIDE);
    const BYTE now = point[0] >= mi(self, W_VISIBLE) && point[1] >= mi(self, W_VISIBLE + 4) &&
                     point[0] < mi(self, W_VISIBLE + 8) && point[1] < mi(self, W_VISIBLE + 12) && vc<BYTE>(self, V_HIT, point);
    mb(self, B_INSIDE) = now;
    if (!was && now && fn<BYTE(THISCALL *)(BYTE *)>(0x40c630)(self))
        vc(self, V_HOVER_SOUND);
    return 0;
}

// Slot 16, a key: space clicks (slot 81); in a radio group keys 12 and 11 move the focus (parent's slot 34) to the
// next and the previous button and click it; every other key the base's (0x40c170)
DWORD THISCALL Button_Key(BYTE *self, DWORD key)   // 0x40fd80
{
    if (key == 0x20) {
        vc(self, V_CLICK);
        return 1;
    }
    BYTE *g = bp(m(self, B_GROUP));
    if (g && g != self) {
        if (key == 12) {
            vc(bp(m(self, W_PARENT)), V_FOCUS, g);
            vc(bp(m(self, B_GROUP)), V_CLICK);
            return 1;
        }
        if (key == 11) {
            while (bp(m(g, B_GROUP)) != self)
                g = bp(m(g, B_GROUP));
            vc(bp(m(self, W_PARENT)), V_FOCUS, g);
            vc(g, V_CLICK);
            return 1;
        }
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
}

// Slot 70, the tool tip at a point: over the button (slot 83) its tool tip (0x40cd50 at the cursor) when it has
// one, else the base's (0x40ea10); elsewhere the parent's slot 70. al: whether a tip shows (the upper bytes as the
// original leaves them: the callee's, slot 83's without a parent).
DWORD THISCALL Button_Tip(BYTE *self, const int *point)   // 0x40fe30
{
    const DWORD hit = vc(self, V_HIT, point);
    if (!(BYTE)hit) {
        if (BYTE *parent = bp(m(self, W_PARENT)))
            return vc(parent, V_PARENT_CURSOR, point);
        return hit & 0xffffff00;
    }
    if (!fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + B_TIP))
        return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x40ea10)(self, point);
    const char *t = fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(self + B_TIP);
    return (fn<DWORD(THISCALL *)(BYTE *, DWORD, const char *, int, int)>(0x40cd50)(self, m(self, B_TIP_ACCEL), t, -1, -1) & 0xffffff00) | 1;
}

// Slot 78, checked or not: checking a button of a radio group gives it the tab stop and unchecks the others
// (their slot 78 with 0); unchecking takes the tab stop
void THISCALL Button_SetChecked(BYTE *self, DWORD on)   // 0x40feb0
{
    m(self, B_CHECKED) = on;
    BYTE *g = bp(m(self, B_GROUP));
    if (!g)
        return;
    if (!on) {
        mb(self, W_TAB_STOP) = 0;
        return;
    }
    mb(self, W_TAB_STOP) = 1;
    for (; g != self; g = bp(m(g, B_GROUP)))
        if (m(g, B_CHECKED))
            vc(g, V_SET_CHECKED, 0);
}

// Slot 80: whether it was clicked since the last call
DWORD THISCALL Button_TakeClick(BYTE *self)   // 0x40ff10
{
    if (!mb(self, B_CLICKED))
        return 0;
    mb(self, B_CLICKED) = 0;
    return 1;
}

// Slot 29: enabled as the callback says (its slot 2)
void THISCALL Button_UpdateEnabled(BYTE *self)   // 0x40ff30
{
    if (BYTE *cb = bp(m(self, B_CALLBACK)))
        mb(self, W_ENABLED) = vc<BYTE>(cb, 2);
}

// An image file as the button's image (the path, the load's flag, image only): a CFrame (0x4a8a90) loaded
// (0x4a9a30 with the frame filters 0x4a8a30 without filter 6) and set (0x40ffd0) as its own; a failed load deletes
// the frame
void THISCALL Button_LoadImage(BYTE *self, const char *path, DWORD flag, DWORD image_only)   // 0x40ff50
{
    BYTE *frame = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x40);
    if (frame)
        frame = fn<BYTE *(THISCALL *)(BYTE *)>(0x4a8a90)(frame);
    BYTE *filters = fn<BYTE *(*)()>(0x4a8a30)();
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x4b20e0)(filters, 6);
    if (fn<BYTE(THISCALL *)(BYTE *, const char *, DWORD, BYTE *, DWORD, DWORD)>(0x4a9a30)(frame, path, flag, filters, 1, 0)) {
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x40ffd0)(self, frame, image_only, 0);
        mb(self, B_IMAGE_OWNED) = 1;
        return;
    }
    if (frame)
        vc(frame, 1, 1);
}

// The image (the frame, the image-only flag, the flag +0x118): an own image before is deleted; with an image
// the button takes its size (slots 45 and 39 of the frame plus twice the border +0x7a, which image-only clears
// together with the frame), is fitted into the parent (slot 63) and, without a tool tip, takes the title as one
// (the accelerator index +0x59)
void THISCALL Button_SetImage(BYTE *self, BYTE *frame, BYTE image_only, BYTE flag)   // 0x40ffd0
{
    mb(self, B_IMAGE_ONLY) = image_only;
    mb(self, B_IMAGE_118) = flag;
    BYTE *old = bp(m(self, B_IMAGE));
    if (old && mb(self, B_IMAGE_OWNED))
        vc(old, 1, 1);
    mb(self, B_IMAGE_OWNED) = 0;
    m(self, B_IMAGE) = pv(frame);
    if (!frame)
        return;
    if (mb(self, B_IMAGE_ONLY)) {
        mb(self, W_FRAMED) = 0;
        mb(self, W_BORDER3) = 0;
    }
    const int h = vc<int>(frame, 39) + 2 * (int)(signed char)mb(self, W_BORDER3);
    const int w = vc<int>(bp(m(self, B_IMAGE)), 45) + 2 * (int)(signed char)mb(self, W_BORDER3);
    vc(self, V_SET_SIZE, w, h);
    if (BYTE *parent = bp(m(self, W_PARENT)))
        vc(parent, V_FIT_CHILD, self, -1000);
    if (fn<bool(THISCALL *)(BYTE *)>(0x4e13d0)(self + B_TIP) && fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + W_TITLE)) {
        const char *t = fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(self + W_TITLE);
        const DWORD accel = mb(self, W_ACCEL_INDEX);
        fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(self + B_TIP, t);
        m(self, B_TIP_ACCEL) = accel;
    }
}

// The tool tip and its accelerator index
void THISCALL Button_SetTip(BYTE *self, const char *text, DWORD accel)   // 0x4100c0
{
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(self + B_TIP, text);
    m(self, B_TIP_ACCEL) = accel;
}

void THISCALL Button_Set114(BYTE *self, DWORD v)   // 0x4100f0
{
    m(self, B_114) = v;
}

// Slot 83, the hit test: in the shape when it has one (0x546830 in content coordinates as floats), else true
bool THISCALL Button_Hit(BYTE *self, const int *point)   // 0x410100
{
    BYTE *shape = bp(m(self, B_SHAPE));
    if (!shape)
        return true;
    const float x = (float)(point[0] - mi(self, W_SCREEN)), y = (float)(point[1] - mi(self, W_SCREEN + 4));
    return fn<bool(THISCALL *)(BYTE *, float, float)>(0x546830)(shape, x, y);
}

// Slot 84: the action sound
void THISCALL Button_ClickSound(BYTE *)   // 0x410150
{
    play_sound(glob_addr(SOUND_ACTION));
}

// Slot 81, a click: the sound (slot 84), then slot 82
void THISCALL Button_Click(BYTE *self)   // 0x410160
{
    vc(self, V_CLICK_SOUND);
    vc(self, V_DO_CLICK);
}

// Slot 82: clicked, the callback, redrawn
void THISCALL Button_DoClick(BYTE *self)   // 0x410180
{
    finish_click(self);
}

// No tool tip (+0x12c set to the argument)
void THISCALL Button_ClearTip(BYTE *self, BYTE v)   // 0x4101c0
{
    mb(self, B_12C) = v;
    fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(self + B_TIP, glob<const char *>(0x62e558));
    m(self, B_TIP_ACCEL) = (DWORD)-1;
}

// ---- The toggles ---------------------------------------------------------------------------------------------

BYTE *THISCALL Radio_Ctor(BYTE *self, BYTE *parent, const char *title, BYTE *partner)   // 0x4109a0
{
    return construct(self, parent, title, partner, fn_addr(0x40a8a0), vt(0x5ebaec));
}

BYTE *THISCALL Radio_CtorString(BYTE *self, BYTE *parent, BYTE *title, BYTE *partner)   // 0x4109e0
{
    return construct(self, parent, title, partner, fn_addr(0x40a940), vt(0x5ebaec));
}

// Slot 18 of the radio button: an enabled unchecked one clicks (slot 81)
bool THISCALL Radio_Activate(BYTE *self)   // 0x410a20
{
    if (m(self, B_CHECKED) || !mb(self, W_ENABLED))
        return false;
    vc(self, V_CLICK);
    return true;
}

// Slot 81 of the radio button: a checked one in a group stays; else toggled
void THISCALL Radio_Click(BYTE *self)   // 0x410a40
{
    if (m(self, B_CHECKED) && m(self, B_GROUP))
        return;
    toggle_click(self);
}

BYTE *THISCALL Toggle_Ctor(BYTE *self, BYTE *parent, const char *title, BYTE *partner)   // 0x410ab0
{
    return construct(self, parent, title, partner, fn_addr(0x40a8a0), vt(0x5ebc44));
}

// Slot 81 of the toggle: toggled
void THISCALL Toggle_Click(BYTE *self)   // 0x410af0
{
    toggle_click(self);
}

// Slot 84 of the toggles: the switch sound
void THISCALL Toggle_ClickSound(BYTE *)   // 0x412750
{
    play_sound(glob_addr(SOUND_SWITCH));
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_button)
{
    const Hook list[] = {
#include "ui_button_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
