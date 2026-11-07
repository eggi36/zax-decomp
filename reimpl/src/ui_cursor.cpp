// The mouse cursor object of the game (0x6cff40, docs/DECOMPILATION.md 9, "List boxes, combo boxes, edit and number
// fields"): its position, buttons, wheel, image and whether the game draws it or Windows shows its arrow.
//
// Layout [confirmed: the functions below and their callers in the window toolkit]:
//   +0x04 the image (a sequence: slot 23 the frame of an index, 25 the frame count, 26 the frames per second),
//   +0x08 the frame (float), +0x0c Windows' arrow instead of the image, +0x0d shown, +0x10/+0x14 the position,
//   +0x18 the buttons down, +0x1c the buttons pressed and not yet taken (a bit each), +0x20 the wheel (float)
// Raw Windows stays for phase 5: 0x518820 (ClientToScreen and SetCursorPos of the game window 0x713e18),
// 0x518860 (Windows' arrow: LoadCursor 0x7f00 and SetCursor) and 0x518880 (SetCursor(0): none) are not
// reimplemented; the functions here call them through their addresses.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD CU_IMAGE = 4, CU_FRAME = 8, CU_ARROW = 0xc, CU_SHOWN = 0xd, CU_X = 0x10, CU_Y = 0x14, CU_DOWN = 0x18,
                CU_PRESSED = 0x1c, CU_WHEEL = 0x20;

void arrow() { fn<void(__cdecl *)()>(0x518860)(); }
void no_cursor() { fn<void(__cdecl *)()>(0x518880)(); }
// The frame count of the image (its slot 25) as the original compares it: as a float with the frame, back to 0
// unless the count is greater (so also for a NaN frame)
void wrap_frame(BYTE *self)
{
    const DWORD n = vc(bp(m(self, CU_IMAGE)), 25);
    if (!((long double)(unsigned long long)n > (long double)m<float>(self, CU_FRAME)))
        m(self, CU_FRAME) = 0;
}
BYTE *current_frame(BYTE *self)
{
    BYTE *img = bp(m(self, CU_IMAGE));
    const int i = fn<int(__stdcall *)(float)>(0x509310)(m<float>(self, CU_FRAME));
    return bp(vc(img, 23, i));
}

}  // namespace

// The position into two ints
void THISCALL Cursor_GetXY(BYTE *self, int *x, int *y)   // 0x5184e0
{
    *x = mi(self, CU_X);
    *y = mi(self, CU_Y);
}

// The position into a point
void THISCALL Cursor_GetPos(BYTE *self, int *p)   // 0x518500
{
    p[0] = mi(self, CU_X);
    p[1] = mi(self, CU_Y);
}

// The wheel's whole steps taken: its fraction kept; eax the whole steps (the original's __ftol result stays in eax:
// the three callers 0x409598, 0x46f779, 0x47020c test or compare it; batch 6-13, as void it was a leftover)
int THISCALL Cursor_KeepWheelFraction(BYTE *self)   // 0x518520
{
    const int whole = x87_ftol(m<float>(self, CU_WHEEL));
    m<float>(self, CU_WHEEL) = (float)((long double)m<float>(self, CU_WHEEL) - (long double)whole);
    return whole;
}

// A press of the buttons in a mask taken (cleared from both sets); al whether there was one
DWORD THISCALL Cursor_TakePress(BYTE *self, DWORD mask)   // 0x518540
{
    if (!(mask & m(self, CU_PRESSED)))
        return 0;
    m(self, CU_PRESSED) &= ~mask;
    m(self, CU_DOWN) &= ~mask;
    return 1;
}

// The position set, and Windows' cursor put there (0x518820)
void THISCALL Cursor_SetPos(BYTE *self, int x, int y)   // 0x518570
{
    mi(self, CU_X) = x;
    mi(self, CU_Y) = y;
    fn<void(__fastcall *)(int, int)>(0x518820)(x, y);
}

// The image set (the frame back to 0 when it has no such frame); the old one
DWORD THISCALL Cursor_SetImage(BYTE *self, BYTE *image)   // 0x518590
{
    const DWORD old = m(self, CU_IMAGE);
    m(self, CU_IMAGE) = pv(image);
    wrap_frame(self);
    return old;
}

// Windows' arrow on and off
void THISCALL Cursor_ArrowOn(BYTE *self)   // 0x5185d0
{
    mb(self, CU_ARROW) = 1;
}
void THISCALL Cursor_ArrowOff(BYTE *self)   // 0x5185e0
{
    mb(self, CU_ARROW) = 0;
}

// Hidden: Windows' arrow when it is on, else no cursor; al whether it was shown
DWORD THISCALL Cursor_Hide(BYTE *self)   // 0x5185f0
{
    const BYTE was = mb(self, CU_SHOWN);
    mb(self, CU_SHOWN) = 0;
    if (mb(self, CU_ARROW))
        arrow();
    else
        no_cursor();
    return was;
}

// Shown: the game's image (no Windows cursor) when there is one and the arrow is off, else Windows' arrow; al
// whether it was shown
DWORD THISCALL Cursor_Show(BYTE *self)   // 0x518620
{
    const BYTE was = mb(self, CU_SHOWN);
    mb(self, CU_SHOWN) = 1;
    if (!mb(self, CU_ARROW) && m(self, CU_IMAGE))
        no_cursor();
    else
        arrow();
    return was;
}

// Windows' cursor as the state asks
void THISCALL Cursor_Refresh(BYTE *self)   // 0x518650
{
    if (mb(self, CU_SHOWN)) {
        if (!mb(self, CU_ARROW) && m(self, CU_IMAGE))
            no_cursor();
        else
            arrow();
    } else if (mb(self, CU_ARROW)) {
        arrow();
    } else {
        no_cursor();
    }
}

// Shown or hidden (the state given): Windows' cursor only on a change; al whether it was shown
DWORD THISCALL Cursor_SetShown(BYTE *self, DWORD on)   // 0x5186a0
{
    const BYTE was = mb(self, CU_SHOWN);
    if ((BYTE)on == was)
        return was;
    mb(self, CU_SHOWN) = (BYTE)on;
    if ((BYTE)on) {
        if (mb(self, CU_ARROW) || !m(self, CU_IMAGE))
            arrow();
        else
            no_cursor();
    } else if (mb(self, CU_ARROW)) {
        arrow();
    } else {
        no_cursor();
    }
    return was;
}

// The image's animation advanced by a time (seconds; its frames per second, slot 26) when it has more than one frame
void THISCALL Cursor_Advance(BYTE *self, float dt)   // 0x518700
{
    BYTE *img = bp(m(self, CU_IMAGE));
    if (!img || vc(img, 25) <= 1)
        return;
    const long double fps = vc<long double>(bp(m(self, CU_IMAGE)), 26);
    m<float>(self, CU_FRAME) = (float)(fps * (long double)dt + (long double)m<float>(self, CU_FRAME));
    wrap_frame(self);
}

// Drawn when shown: the image's frame at the position through Frame_BlitAt with the sequence's blend mode (0x56bf00)
void THISCALL Cursor_Draw(BYTE *self, BYTE *frame)   // 0x518760
{
    if (!m(self, CU_IMAGE) || !mb(self, CU_SHOWN))
        return;
    const int x = mi(self, CU_X), y = mi(self, CU_Y);
    BYTE *f = current_frame(self);
    if (!f)
        return;
    const DWORD blend = fn<DWORD(THISCALL *)(BYTE *)>(0x56bf00)(bp(m(self, CU_IMAGE)));
    fn<void(THISCALL *)(BYTE *, BYTE *, int, int, DWORD)>(0x4b3280)(frame, f, x, y, blend);
}

// The rectangle of the shown image's frame (its slot 36) at the position; al whether there is one
DWORD THISCALL Cursor_Rect(BYTE *self, int *r)   // 0x5187b0
{
    if (!m(self, CU_IMAGE) || !mb(self, CU_SHOWN))
        return 0;
    const int x = mi(self, CU_X), y = mi(self, CU_Y);
    BYTE *f = current_frame(self);
    if (!f)
        return 0;
    vc(f, 36, r);
    r[0] += x, r[1] += y, r[2] += x, r[3] += y;
    return 1;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_cursor)
{
    const Hook list[] = {
#include "ui_cursor_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
