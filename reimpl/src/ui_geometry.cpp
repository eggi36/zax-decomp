// Three self-contained helpers of the window toolkit (0x403000-0x408fff: the frame windows, buttons,
// CJListBox with its tree rows, the dropdown list; all of it is UI code and stays with that subsystem):
//   0x403240 the CRect constructor (left, top, right, bottom),
//   0x4039e0 which edges of a resizable window a point (relative to the window) is on,
//   0x407440 the scroll step of the list box (fastcall).
#include "reimpl.h"

namespace zax {

struct CRect {
    int left, top, right, bottom;
    CRect *THISCALL Ctor(int l, int t, int r, int b);   // 0x403240
};

CRect *THISCALL CRect::Ctor(int l, int t, int r, int b)
{
    left = l;
    top = t;
    right = r;
    bottom = b;
    return this;
}

// The window's rectangle is at +4 (left, top, right, bottom), its font at +0x7c (height at +0x80).
// Bits: 1 left, 2 top, 4 right, 8 bottom. The border is 2 * (height / 5) wide; within 6 * (height / 5)
// of a corner the neighbouring edge counts too (the resize corners).
struct ResizableWindow {
    DWORD vftable;
    int left, top, right, bottom;     // 0x04
    BYTE unknown14[0x7c - 0x14];
    const BYTE *font;                 // 0x7c
    DWORD THISCALL EdgeHit(const int *point);   // 0x4039e0
};
static_assert(offsetof(ResizableWindow, font) == 0x7c);

DWORD THISCALL ResizableWindow::EdgeHit(const int *point)
{
    const int e = (int)(*(const DWORD *)(font + 0x80) / 5 * 2);
    const int x = point[0], y = point[1];
    const int w = (int)((DWORD)right - (DWORD)left), h = (int)((DWORD)bottom - (DWORD)top);
    DWORD bits = x <= e ? 1 : 0;
    if (y <= e)
        bits |= 2;
    if (x >= (int)((DWORD)w - (DWORD)e))
        bits |= 4;
    if (y >= (int)((DWORD)h - (DWORD)e))
        bits |= 8;
    const int c = (int)((DWORD)e * 3);
    if (x <= c && (bits & 10))
        bits |= 1;
    if (y <= c && (bits & 5))
        bits |= 2;
    if (x >= (int)((DWORD)w - (DWORD)c) && (bits & 10))
        bits |= 4;
    if (y >= (int)((DWORD)h - (DWORD)c) && (bits & 5))
        bits |= 8;
    return bits;
}

// From `from` towards `to`: at most `far` away from `to`, and if farther than `near`, `near` closer to
// `to` than `from` (sign of the wrapped difference, as the original's jns)
int __fastcall ScrollStep(int from, int to, int near_, int far_)
{
    int d = (int)((DWORD)to - (DWORD)from), s = 1;
    if (d < 0) {
        d = (int)(0u - (DWORD)d);
        s = -1;
    }
    if (d > far_)
        return (int)((DWORD)to - (DWORD)s * (DWORD)far_);
    if (d > near_)
        return (int)((DWORD)s * (DWORD)near_ + (DWORD)from);
    return to;
}

ZAX_HOOKS(ui_geometry)
{
    const Hook list[] = {
        {0x403240, ptr(&CRect::Ctor), "CRect::CRect", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x4039e0, ptr(&ResizableWindow::EdgeHit), "ResizableWindow::EdgeHit", 8, {0x8b, 0x51, 0x7c, 0xb8, 0xcd, 0xcc, 0xcc, 0xcc}},
        {0x407440, (void *)&ScrollStep, "ScrollStep", 6, {0x8b, 0xc2, 0x56, 0x8b, 0xf0, 0x57, 0x2b, 0xf1}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
