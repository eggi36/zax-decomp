// CJSlider (vtable 0x5eb9b4, 0x10c bytes, "~CJSlider"), the slider of the window toolkit (docs/DECOMPILATION.md 9,
// "The window toolkit"). Its slot 0 is the shared deleting destructor 0x56c3d0 (outside the toolkit).
//
// Layout [confirmed: the constructor 0x4101f0 and the slots below]: the CJWindow part, then
//   +0x100 the value (0 .. count - 1), +0x104 the count of values, +0x108 the step of the keys 11/12 ((count + 10)
//   / 20, at least 1, unless given)
// The thumb is as wide as the font's thumb width (0x4a6140) plus one; it sits at
// ((width - thumb - 1) * value + (count - 1) / 2) / (count - 1) (unsigned) from the left, 0 with fewer than two
// values (the width from the rect at +4).
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD SL_VALUE = 0x100, SL_COUNT = 0x104, SL_STEP = 0x108;
constexpr int V_SET_VALUE = 76;

DWORD thumb_width(BYTE *self) { return fn<DWORD(THISCALL *)(BYTE *)>(0x4a6140)(bp(m(self, W_FONT))); }
DWORD thumb_x(BYTE *self)
{
    if (m(self, SL_COUNT) <= 1)
        return 0;
    const DWORD n = m(self, SL_COUNT) - 1;
    const DWORD t = thumb_width(self);
    return (((DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT)) - t - 1) * m(self, SL_VALUE) + n / 2) / n;
}
DWORD default_step(DWORD count)
{
    const DWORD s = (count + 10) / 20;
    return s < 1 ? 1 : s;
}
DWORD palette_index(BYTE *colour)
{
    if (mb(colour, 0xc) & 1)
        return mb(colour, 0xa);
    return fn<BYTE(THISCALL *)(BYTE *)>(0x53f160)(colour);
}
void bevel(BYTE *self, BYTE *frame, int *rect, DWORD c2, DWORD c1, DWORD sunken)
{
    fn<DWORD(__fastcall *)(BYTE *, BYTE *, int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(bp(m(self, W_FONT)), frame, rect, c2, c1, sunken, 0xf, 0);
}

}  // namespace

// The constructor (the parent, the count, the step or 0): "~CJSlider", a tab stop, framed, value 0; as wide as the
// title (or "XXXXXXXXXXXX") and as high as the thumb, each plus 4 * (font height / 5) + 2 (0x40d510)
BYTE *THISCALL Slider_Ctor(BYTE *self, BYTE *parent, DWORD count, DWORD step)   // 0x4101f0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(self, parent, glob<const char *>(0x60a66c));
    m(self, 0) = vt(0x5eb9b4);
    mb(self, W_TAB_STOP) = 1;
    mb(self, W_FRAMED) = 1;
    m(self, SL_VALUE) = 0;
    m(self, SL_COUNT) = count;
    m(self, SL_STEP) = step ? step : default_step(count);
    const char *text = glob<const char *>(0x60a65c);
    if (fn<bool(THISCALL *)(BYTE *)>(0x4e13e0)(self + W_TITLE))
        text = fn<const char *(THISCALL *)(BYTE *)>(0x4e13a0)(self + W_TITLE);
    const DWORD h = m(bp(m(self, W_FONT)), 0x80);
    const DWORD thumb = thumb_width(self);
    const DWORD tw = fn<DWORD(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, W_FONT)), text, -1);
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(self, tw + 4 * (h / 5) + 2, thumb + 4 * (h / 5) + 2);
    return self;
}

// Slot 77: the value
DWORD THISCALL Slider_Value(BYTE *self)   // 0x4102c0
{
    return m(self, SL_VALUE);
}

// Slot 4: the groove (a raised bevel from 2 * (font height / 5) + 1 below the top to as much above the bottom, and
// two lines across the middle in the dark colours 0x62e5a8 / 0x62e590), the thumb (its background from the
// texture, slot 72, unless transparent, through the frame's slot 23; a sunken bevel) in the content rect kept in
// the static rect 0x7161a0
void THISCALL Slider_Draw(BYTE *self, BYTE *frame)   // 0x4102d0
{
    const int cw = mi(self, W_CONTENT_W), ch = mi(self, W_CONTENT_H);
    const int h5 = (int)(m(bp(m(self, W_FONT)), 0x80) / 5);
    int groove[4] = {0, 2 * h5 + 1, cw, ch - 1 - 2 * h5};
    const DWORD c1 = palette_index(self + 0xb0), c2 = palette_index(self + 0xa0);
    bevel(self, frame, groove, c2, c1, 0);
    using Fill = void(THISCALL *)(BYTE *, int, int, int, int, DWORD);
    fn<Fill>(0x4b3ce0)(frame, h5, (int)((DWORD)ch >> 1), cw - h5 + 1, (int)((DWORD)ch >> 1) + 1, glob_addr(0x62e5a8));
    fn<Fill>(0x4b3ce0)(frame, h5, (int)((DWORD)ch >> 1) + 1, cw - h5 + 1, (int)((DWORD)ch >> 1) + 2, glob_addr(0x62e590));
    if (!(gb(glob_addr(0x716190)) & 1)) {
        gb(glob_addr(0x716190)) |= 1;
        fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x401270));
    }
    int *stat = glob<int *>(0x7161a0);
    stat[0] = 0, stat[1] = 0, stat[2] = mi(self, W_CONTENT_W), stat[3] = mi(self, W_CONTENT_H);
    const DWORD x = thumb_x(self);
    int thumb[4] = {stat[0] + (int)x, stat[1], stat[2], stat[3]};
    thumb[2] = (int)thumb_width(self) + thumb[0] + 1;
    int src[4] = {0, 0, thumb[2] - thumb[0], thumb[3] - thumb[1]};
    int dst[4] = {thumb[0], thumb[1], thumb[2], thumb[3]};
    if (!mb(self, W_TRANSPARENT)) {
        const DWORD texture = vc(self, V_TEXTURE);
        vc(frame, 23, texture, src, dst, glob_addr(0x6bc4e0));
    }
    const DWORD d1 = palette_index(self + 0xb0), d2 = palette_index(self + 0xa0);
    bevel(self, frame, thumb, d2, d1, 1);
}

// Slot 8, the focus mark (the frame, the colour): a rectangle (0x4b3430) one pixel around the thumb, in the parent's
// coordinates
void THISCALL Slider_DrawFocus(BYTE *self, BYTE *frame, DWORD colour)   // 0x4105a0
{
    const DWORD x = thumb_x(self);
    const int left = mi(self, W_RECT) + (int)x;
    const int right = (int)thumb_width(self) + left + 1;
    int r[4] = {left - 1, mi(self, W_RECT + 4) - 1, right + 1, mi(self, W_RECT + 12) + 1};
    fn<void(THISCALL *)(BYTE *, int *, DWORD)>(0x4b3430)(frame, r, colour);
}

// Slot 14, a drag with the left button: the value under the point (rounded, clamped), set (slot 76), under cursor 8;
// true. Else the base's (0x40c0a0).
bool THISCALL Slider_MouseDrag(BYTE *self, const int *point, const int *delta, DWORD buttons, DWORD *cursor_out)   // 0x410650
{
    if (!(buttons & 1))
        return fn<bool(THISCALL *)(BYTE *, const int *, const int *, DWORD, DWORD *)>(0x40c0a0)(self, point, delta, buttons, cursor_out);
    *cursor_out = fn<DWORD(__fastcall *)(int)>(0x40e9e0)(8);
    const int x = point[0] - (int)((thumb_width(self) + 1) >> 1) - mi(self, W_SCREEN);
    const DWORD t = thumb_width(self);
    const int n = mi(self, SL_COUNT);
    const int span = mi(self, W_RECT + 8) - mi(self, W_RECT) - (int)t - 1;
    int v = ((n - 1) * x + span / 2) / span;
    if (v < 0)
        v = 0;
    else if (v >= n)
        v = n - 1;
    vc(self, V_SET_VALUE, v, 0);
    return true;
}

// Slot 15, the wheel: up key 12, down key 11 (slot 16); whether it was taken
DWORD THISCALL Slider_Wheel(BYTE *self, int delta, DWORD)   // 0x410700
{
    return vc(self, V_KEY, delta > 0 ? 12 : 11) ? 1 : 0;
}

// Slot 16, a key: the base's (0x40c170); key 5 the first value, 14 the last, 11 and 12 a step down and up
// (clamped); whether it was taken
DWORD THISCALL Slider_Key(BYTE *self, DWORD key)   // 0x410730
{
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key))
        return 1;
    const DWORD v = m(self, SL_VALUE), step = m(self, SL_STEP), n = m(self, SL_COUNT);
    switch (key) {
    case 5:
        vc(self, V_SET_VALUE, 0, 0);
        return 1;
    case 14:
        vc(self, V_SET_VALUE, n - 1, 0);
        return 1;
    case 11:
        vc(self, V_SET_VALUE, v > step ? v - step : 0, 0);
        return 1;
    case 12:
        vc(self, V_SET_VALUE, step + v < n ? step + v : n - 1, 0);
        return 1;
    default:
        return 0;
    }
}

// Slot 76, the value (the value, quiet): a change redraws (slot 38 with 1) unless quiet. eax as the original leaves it.
DWORD THISCALL Slider_SetValue(BYTE *self, DWORD v, DWORD quiet)   // 0x410850
{
    const DWORD old = m(self, SL_VALUE);
    m(self, SL_VALUE) = v;
    if (old == v)
        return old;
    if (quiet)
        return quiet;
    return vc(self, V_INVALIDATE, 1);
}

// The count of values and the step (0: the default)
void THISCALL Slider_SetRange(BYTE *self, DWORD count, DWORD step)   // 0x410880
{
    m(self, SL_COUNT) = count;
    m(self, SL_STEP) = step ? step : default_step(count);
}

// Slot 69, the cursor at a point (content coordinates): 7 over the thumb, 2 elsewhere in the slider, else the base's
// (0x40ea00)
DWORD THISCALL Slider_Cursor(BYTE *self, const int *point)   // 0x4108c0
{
    const int w = mi(self, W_RECT + 8) - mi(self, W_RECT), h = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
    const int x0 = (int)thumb_x(self);
    const int x1 = (int)thumb_width(self) + x0 + 1;
    const int px = point[0], py = point[1];
    if (px >= x0 && py >= 0 && px < x1 && py < h)
        return fn<DWORD(__fastcall *)(int)>(0x40e9e0)(7);
    if (px >= 0 && py >= 0 && px < w && py < h)
        return fn<DWORD(__fastcall *)(int)>(0x40e9e0)(2);
    return fn<DWORD(THISCALL *)(BYTE *, const int *)>(0x40ea00)(self, point);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_slider)
{
    const Hook list[] = {
#include "ui_slider_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
