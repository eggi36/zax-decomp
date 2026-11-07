// The check box (vtable 0x5ec004, 0x144 bytes, a button) and the text label (0x5ec170, 0x128 bytes) of the window
// toolkit (docs/DECOMPILATION.md 9, "The window toolkit").
//
// Check box layout [confirmed: the constructor 0x412280 and the slots below]: the button (ui_button.cpp), then
//   +0x138 the box right of the title (else left), +0x13c a byte the checked state is mirrored to (or null),
//   +0x140 the box size (font height + 2)
// Text label layout [confirmed: the constructors 0x4127d0/0x412930 and the slots below]: the CJWindow part, then
//   +0x100 an extra tool tip (ZString, below the title in the tip), +0x104 the title has several lines,
//   +0x108 the wrap width (-1: no wrapping), +0x10c the maximum height (-1: none), +0x110 the alignment
//   (0 left, 1 centred, 2 right), +0x114 ... +0x124 the font's wrap and layout parameters (0x4a7010/0x4a7110/0x4a6ae0)
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD CB_RIGHT = 0x138, CB_MIRROR = 0x13c, CB_SIZE = 0x140;
constexpr DWORD L_TIP = 0x100, L_LINES = 0x104, L_WRAP = 0x108, L_MAX_H = 0x10c, L_ALIGN = 0x110;
// Button fields the check box uses
constexpr DWORD B_CLICKED = 0x102, B_CLICKED_28 = 0x103, B_CHECKED = 0x104, B_GROUP = 0x10c, B_IMAGE = 0x110,
                B_CALLBACK = 0x128, B_LOOK = 0x134;
constexpr int V_SET_CHECKED = 78, V_DRAW_TEXT = 74, V_BOX = 86, V_BOX_HOVER = 87, V_BOX_SUNKEN = 88, V_CHECK_MARK = 89,
              V_BOX_TEXT = 90;

BYTE *font_of(BYTE *self) { return bp(m(self, W_FONT)); }
const char *title_of(BYTE *self) { return zax::fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(self + W_TITLE); }
bool has_title(BYTE *self) { return zax::fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + W_TITLE); }
DWORD thumb_width(BYTE *self) { return zax::fn<DWORD(THISCALL *)(BYTE *)>(0x4a6140)(font_of(self)); }
DWORD text_width(BYTE *self, const char *t) { return zax::fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(font_of(self), t, -1); }
int border(BYTE *self) { return (int)(signed char)mb(self, W_BORDER3); }
DWORD palette_index(BYTE *colour)
{
    if (mb(colour, 0xc) & 1)
        return mb(colour, 0xa);
    return zax::fn<BYTE(THISCALL *)(BYTE *)>(0x53f160)(colour);
}
// The check box's size: the title (or "XXX") plus the box plus one, as high as the thumb plus 2 (slot 59)
void checkbox_size(BYTE *self)
{
    const char *t = has_title(self) ? title_of(self) : glob<const char *>(0x60a680);
    const DWORD h = thumb_width(self) + 2;
    const DWORD w = text_width(self, t);
    vc(self, V_SET_SIZE, w + m(self, CB_SIZE) + 1, h);
}
// The box: a bevel (raised or sunken) of the box size at x, y
void box(BYTE *self, BYTE *frame, int x, int y, DWORD sunken)
{
    int r[4] = {x, y, x + mi(self, CB_SIZE), y + mi(self, CB_SIZE)};
    const DWORD c1 = palette_index(self + 0xb0);
    const DWORD c2 = palette_index(self + 0xa0);
    zax::fn<DWORD(__fastcall *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(font_of(self), frame, r, c2, c1, sunken, 0xf, 0);
}
using LayoutFn = int(THISCALL *)(BYTE *, const char *, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
int layout(DWORD fn, BYTE *self, const char *t, int b)
{
    return fn_dyn<LayoutFn>(fn)(font_of(self), t, mi(self, L_WRAP) - (2 * b + 2), b, m(self, 0x110), m(self, 0x114), m(self, 0x118),
                            m(self, 0x11c), m(self, 0x120), m(self, 0x124));
}

}  // namespace

// ---- The check box ---------------------------------------------------------------------------------------------

// The constructor (the parent, the title, a radio partner or null): a button (0x40faa0) with the check box's vtable,
// unframed, the box left of the title and its size, sized; a mirror byte (none yet) is followed (slot 78)
BYTE *THISCALL CheckBox_Ctor(BYTE *self, BYTE *parent, const char *title, BYTE *partner)   // 0x412280
{
    zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, title);
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(self + 0x11c);
    m(self, 0) = vt(0x5eb85c);
    zax::fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40faa0)(self, partner);
    m(self, 0) = vt(0x5ec004);
    m(self, CB_MIRROR) = 0;
    mb(self, W_FRAMED) = 0;
    mb(self, CB_RIGHT) = 0;
    m(self, CB_SIZE) = m(font_of(self), 0x80) + 2;
    checkbox_size(self);
    if (BYTE *mirror = bp(m(self, CB_MIRROR)))
        vc(self, V_SET_CHECKED, (DWORD)*mirror);
    return self;
}

// Slot 0: the button's destructor, with bit 0 the pool block (0x144)
BYTE *THISCALL CheckBox_DeletingDtor(BYTE *self, DWORD flags)   // 0x412350
{
    zax::fn<void(THISCALL *)(BYTE *)>(0x40fb80)(self);
    if (flags & 1)
        zax::fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x144);
    return self;
}

// Slot 86: the box raised at x, y
void THISCALL CheckBox_Box(BYTE *self, BYTE *frame, int x, int y)   // 0x412380
{
    box(self, frame, x, y, 0);
}

// Slot 87, the box under the mouse: as slot 88
DWORD THISCALL CheckBox_BoxHover(BYTE *self, BYTE *frame, int x, int y)   // 0x412420
{
    return vc(self, V_BOX_SUNKEN, frame, x, y);
}

// Slot 88: the box sunken at x, y
void THISCALL CheckBox_BoxSunken(BYTE *self, BYTE *frame, int x, int y)   // 0x412440
{
    box(self, frame, x, y, 1);
}

// Slot 89: the check mark, two diagonals inside the box (the frame's line, slot 51, in the colour +0x90)
void THISCALL CheckBox_CheckMark(BYTE *self, BYTE *frame, int x, int y)   // 0x4124e0
{
    const int s = mi(self, CB_SIZE);
    BYTE *colour = self + 0x90;
    vc(frame, 51, x + 1, y + 1, x + s - 2, y + s - 2, colour);
    vc(frame, 51, x + s - 2, y + 1, x + 1, y + s - 2, colour);
}

// Slot 90: the title (slot 74) after the box, or at -1 when the box is right of it
DWORD THISCALL CheckBox_Text(BYTE *self, BYTE *frame)   // 0x412550
{
    const DWORD x = mb(self, CB_RIGHT) ? (DWORD)-1 : m(self, CB_SIZE);
    return vc(self, V_DRAW_TEXT, frame, x, 1, 0);
}

// Slot 4: with an image as a button; else, disabled at the opacities 0.4 (transparent mask) and 0.6 (black bias)
// of the blend modes (restored after), the title (slot 90), the box after it or before it: pressed and over it
// raised (86), checked raised with the mark (86, 89), over it slot 87, else sunken (88); the children
void THISCALL CheckBox_Draw(BYTE *self, BYTE *frame)   // 0x412580
{
    if (m(self, B_IMAGE)) {
        vc(bp(m(self, B_LOOK)), 1, frame);
        mb(self, B_CLICKED_28) = 0;
        zax::fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
        return;
    }
    const BYTE enabled = zax::fn<BYTE(THISCALL *)(BYTE *)>(0x40c630)(self);
    using GetOpacity = float(THISCALL *)(DWORD);
    using SetOpacity = void(THISCALL *)(DWORD, float);
    float o1 = 1.0f, o2 = 1.0f;
    if (!enabled) {
        o1 = zax::fn<GetOpacity>(0x433c20)(glob_addr(0x6bcc68));
        o2 = zax::fn<GetOpacity>(0x433c20)(glob_addr(0x6bcc68));
        zax::fn<SetOpacity>(0x433be0)(glob_addr(0x6bcc68), 0.4f);
        zax::fn<SetOpacity>(0x433be0)(glob_addr(0x6b9728), 0.6f);
    }
    vc(self, V_BOX_TEXT, frame);
    const DWORD w = text_width(self, title_of(self));
    const int x = mb(self, CB_RIGHT) ? (int)w + 1 : 0;
    const BYTE inside = mb(self, 0x101);
    if (inside && mb(self, 0x100)) {
        vc(self, V_BOX, frame, x, 0);
    } else if (m(self, B_CHECKED)) {
        vc(self, V_BOX, frame, x, 0);
        vc(self, V_CHECK_MARK, frame, x, 0);
    } else if (inside) {
        vc(self, V_BOX_HOVER, frame, x, 0);
    } else {
        vc(self, V_BOX_SUNKEN, frame, x, 0);
    }
    zax::fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
    if (!enabled) {
        zax::fn<SetOpacity>(0x433be0)(glob_addr(0x6bcc68), o1);
        zax::fn<SetOpacity>(0x433be0)(glob_addr(0x6b9728), o2);
    }
}

// Slot 43, the title: CJWindow's (0x40cdc0), sized again
void THISCALL CheckBox_SetTitle(BYTE *self, const char *text)   // 0x4126f0
{
    zax::fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, text);
    checkbox_size(self);
}

// Slot 81, a click: a checked box of a radio group stays; else toggled (mirrored to the byte), slot 78, clicked,
// the callback, redrawn (no sound)
void THISCALL CheckBox_Click(BYTE *self)   // 0x412760
{
    if (m(self, B_CHECKED) && m(self, B_GROUP))
        return;
    const DWORD on = m(self, B_CHECKED) == 0;
    if (BYTE *mirror = bp(m(self, CB_MIRROR)))
        *mirror = on != 0;
    vc(self, V_SET_CHECKED, on);
    mb(self, B_CLICKED) = 1;
    if (vc(self, V_28))
        mb(self, B_CLICKED_28) = 1;
    if (BYTE *cb = bp(m(self, B_CALLBACK)))
        vc(cb, 1);
    vc(self, V_INVALIDATE, 0);
}

// ---- The text label ----------------------------------------------------------------------------------------------

namespace {

// The constructors' common part after the title text is known: the extra tip, one line, no wrap or maximum
// height, left aligned, the layout parameters 0, each set followed by the title again (slot 43; the first
// through the vtable entry itself when `direct`), as high as the thumb plus the borders (0x40f5d0), then the text
// as the title
void label_init(BYTE *self, const char *text, bool direct)
{
    using SetTitle = void(THISCALL *)(BYTE *, const char *);
    mb(self, L_LINES) = 0;
    mb(self, W_5F) = 1;
    m(self, L_WRAP) = (DWORD)-1;
    m(self, L_MAX_H) = (DWORD)-1;
    if (direct)
        (*glob<SetTitle *>(0x5ec21c))(self, title_of(self));
    else
        vc(self, V_SET_TITLE, title_of(self));
    m(self, L_ALIGN) = 0;
    vc(self, V_SET_TITLE, title_of(self));
    m(self, 0x114) = 0;
    m(self, 0x118) = 0;
    vc(self, V_SET_TITLE, title_of(self));
    m(self, 0x11c) = 0;
    vc(self, V_SET_TITLE, title_of(self));
    m(self, 0x120) = 0;
    m(self, 0x124) = 0;
    vc(self, V_SET_TITLE, title_of(self));
    zax::fn<DWORD(THISCALL *)(BYTE *, int)>(0x40f5d0)(self, (int)thumb_width(self) + 2 * border(self) + 2);
    vc(self, V_SET_TITLE, text);
}

}  // namespace

// The constructor (the parent, the text)
BYTE *THISCALL Label_Ctor(BYTE *self, BYTE *parent, const char *text)   // 0x4127d0
{
    zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, nullptr);
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(self + L_TIP);
    m(self, 0) = vt(0x5ec170);
    label_init(self, text, true);
    return self;
}

// The constructor with the text as a ZString
BYTE *THISCALL Label_CtorString(BYTE *self, BYTE *parent, BYTE *text)   // 0x412930
{
    zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, nullptr);
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(self + L_TIP);
    m(self, 0) = vt(0x5ec170);
    label_init(self, zax::fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(text), false);
    return self;
}

// Slot 76: the alignment
void THISCALL Label_SetAlign(BYTE *self, DWORD align)   // 0x4128c0
{
    m(self, L_ALIGN) = align;
}

// Slot 46, the font: sized from nothing again (slot 59 with 0, 0, then the title, slot 43)
void THISCALL Label_SetFont(BYTE *self, BYTE *font)   // 0x4128d0
{
    m(self, W_FONT) = pv(font);
    vc(self, V_SET_SIZE, 0, 0);
    vc(self, V_SET_TITLE, title_of(self));
}

// Slot 0: the extra tip, CJWindow's destructor, with bit 0 the pool block (0x128)
BYTE *THISCALL Label_DeletingDtor(BYTE *self, DWORD flags)   // 0x412900
{
    zax::fn<void(THISCALL *)(BYTE *)>(0x4defd0)(self + L_TIP);
    zax::fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        zax::fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x128);
    return self;
}

// Slot 4: without wrapping several lines (the font's 0x4a66c0) or one line aligned (slot 74: left at the border,
// centred, right); with wrapping the font's wrapped text (0x4a6ae0 with the layout parameters); the children
void THISCALL Label_Draw(BYTE *self, BYTE *frame)   // 0x412a30
{
    const int b = border(self);
    if (m(self, L_WRAP) != (DWORD)-1) {
        const char *t = title_of(self);
        zax::fn<void(THISCALL *)(BYTE *, BYTE *, int, int, const char *, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4a6ae0)(
            font_of(self), frame, b, b + 1, t, mi(self, L_WRAP) - 2 * (b + 1), b, m(self, 0x110), m(self, 0x114), m(self, 0x118),
            m(self, 0x11c), m(self, 0x120), m(self, 0x124));
    } else if (mb(self, L_LINES)) {
        const char *t = title_of(self);
        zax::fn<void(THISCALL *)(BYTE *, BYTE *, int, int, const char *, int, DWORD)>(0x4a66c0)(font_of(self), frame, b, b + 1, t, b, 0);
    } else {
        switch (m(self, L_ALIGN)) {
        case 0:
            vc(self, V_DRAW_TEXT, frame, b, b + 1, 0);
            break;
        case 1:
            vc(self, V_DRAW_TEXT, frame, mi(self, W_CONTENT_W) / 2, b + 1, 1);
            break;
        case 2:
            vc(self, V_DRAW_TEXT, frame, mi(self, W_CONTENT_W) - b - 1, b + 1, 2);
            break;
        default:
            break;
        }
    }
    zax::fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40b620)(self, frame);
}

// Slot 43, the title: CJWindow's (0x40cdc0) when it changed; the size: without wrapping, several lines (a line
// break in it) as their height (0x4a6c80 with spacing 2 * (font height / 5)) and widest line (0x4a7380), one line
// widened to the text (or "XXX") when wider than the rect; with wrapping the font's wrapped size (0x4a7010 height,
// 0x4a7110 width); each plus the borders; then at most the maximum height (0x40f5d0)
void THISCALL Label_SetTitle(BYTE *self, const char *text)   // 0x412b70
{
    if (zax::fn<int(THISCALL *)(BYTE *, const char *)>(0x4e1770)(self + W_TITLE, text))
        zax::fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, text);
    if (m(self, L_WRAP) == (DWORD)-1) {
        if (has_title(self) && zax::fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(title_of(self), '\n')) {
            mb(self, L_LINES) = 1;
            const DWORD fh = m(font_of(self), 0x80);
            const char *t = title_of(self);
            const int h = zax::fn<int(THISCALL *)(BYTE *, const char *, DWORD)>(0x4a6c80)(font_of(self), title_of(self), 2 * (fh / 5));
            const int hh = h + 2 * border(self) + 2;
            const int w = zax::fn<int(THISCALL *)(BYTE *, const char *)>(0x4a7380)(font_of(self), t);
            vc(self, V_SET_SIZE, w + 2 * border(self) + 2, hh);
        } else {
            mb(self, L_LINES) = 0;
            const char *t = has_title(self) ? title_of(self) : glob<const char *>(0x60a680);
            const DWORD w = text_width(self, t) + 2 * border(self);
            if (w > (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT))) {
                const int h = (int)thumb_width(self) + 2 * border(self) + 2;
                vc(self, V_SET_SIZE, w, h);
            }
        }
    } else {
        const char *t1 = title_of(self);
        const char *t2 = title_of(self);
        const int b = border(self);
        const int h = layout(fn_addr(0x4a7010), self, t1, b) + 2 * b + 2;
        const int w = layout(fn_addr(0x4a7110), self, t2, b);
        vc(self, V_SET_SIZE, w + 2 * border(self) + 2, h);
    }
    if (m(self, L_MAX_H) != (DWORD)-1 && mi(self, W_RECT + 12) - mi(self, W_RECT + 4) > mi(self, L_MAX_H))
        zax::fn<DWORD(THISCALL *)(BYTE *, int)>(0x40f5d0)(self, mi(self, L_MAX_H));
}

// Slot 77: CJWindow's title (0x40cdc0) without the sizing
DWORD THISCALL Label_SetTitleOnly(BYTE *self, const char *text)   // 0x412d70
{
    return zax::fn<DWORD(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, text);
}

// Slot 70, the tool tip at a point: a one-line title cut off by the rect shows in full at the label (the screen
// rect plus the border), the extra tip below it (after 0x60a684) or alone at the cursor (0x40cd00); without a
// tip the base's (0x40ea10). al: whether a tip shows.
bool THISCALL Label_Tip(BYTE *self, const int *point)   // 0x412d80
{
    alignas(4) BYTE tip[4];
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(tip);
    bool cut = false;
    if (has_title(self) && !mb(self, L_LINES) && m(self, L_WRAP) == (DWORD)-1) {
        const DWORD w = (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT));
        if (text_width(self, title_of(self)) > w) {
            cut = true;
            zax::fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1be0)(tip, title_of(self));
        }
    }
    if (zax::fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + L_TIP)) {
        if (zax::fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(tip))
            zax::fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1be0)(tip, glob<const char *>(0x60a684));
        zax::fn<void(THISCALL *)(BYTE *, BYTE *)>(0x4e1ab0)(tip, self + L_TIP);
    }
    if (!zax::fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(tip)) {
        zax::fn<void(THISCALL *)(BYTE *, const int *)>(0x40ea10)(self, point);
        zax::fn<void(THISCALL *)(BYTE *)>(0x4defd0)(tip);
        return false;
    }
    using ShowTip = void(THISCALL *)(BYTE *, const char *, int, int);
    const char *t = zax::fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(tip);
    if (cut) {
        const int b = border(self);
        zax::fn<ShowTip>(0x40cd00)(self, t, mi(self, W_SCREEN) + b, mi(self, W_SCREEN + 4) + b + 1);
    } else {
        zax::fn<ShowTip>(0x40cd00)(self, t, -1, -1);
    }
    zax::fn<void(THISCALL *)(BYTE *)>(0x4defd0)(tip);
    return true;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_label)
{
    const Hook list[] = {
#include "ui_label_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
