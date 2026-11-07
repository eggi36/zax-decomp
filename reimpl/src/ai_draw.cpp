// The debug drawing slot of four AIs (phase 4, rendering; the AIs' draw slot (part, frame), called by
// 0x4fd0b0 from the parts' slot 23 while the world draws): CLandscapeAI 0x4e4280, CScanAreaAI 0x565f40,
// CDrawTimeoutOvalAI 0x4cad40 and CDrawTimeoutRectAI 0x4cb110. They draw with the view helpers of
// view_draw.cpp and the frame helpers.
//
// [confirmed: code; VM]
//  - CLandscapeAI (+0x10/+0x14 the shape's width and height, +0x18 a value that must be above 0, +0x24 the
//    shape: 0 an oval, 1 a rectangle): in (60, 205, 60) at the part's position (slot 43, truncated with
//    __ftol for the rectangle): the oval with the radius width * 0x5eee2c and the ratio height / width
//    (both as 64-bit integers), the rectangle (Frame_DrawRectXY) width / 2 and height / 2 around it.
//  - CScanAreaAI: three arrows (DrawView_Arrow, y ratio 0x3f3677d4) at the part's position
//    (LayerPart_Position 0x4fb0a0): the scan's two limits +0x14 and +0x18 (times 0x5edb3c, plus the base
//    angle +0x48) in (0, 255, 255) and (255, 255, 0), and the part's facing (slot 46) in blue while +0x40
//    is set, else in (255, 128, 0).
//  - CDrawTimeoutOvalAI: an oval (radius +0x10, ratio +0x14) in the function-local static colour 0x6bfe38
//    (255, 100, 100; guard 0x6bfe4b, atexit 0x4cadf0) at the part's position, and its text +0x1c (when not
//    empty) centred there in the font +0x20 (the position truncated, y first).
//  - CDrawTimeoutRectAI: the rectangle +0x10 (Frame_DrawRect) in the static colour 0x6bfe28 (guard
//    0x6bfe21, atexit 0x4cb1e0) and its text +0x24 centred at the part's position and again left
//    justified at the rectangle's corner, in the font +0x28.
//
// Return values: void for the callers (eax not reproduced).
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

using LD = long double;

void oval(void *frame, const float *c, float r, float ratio, const void *colour)
{
    fn<void(__fastcall *)(void *, const float *, float, float, const void *)>(0x4fccc0)(frame, c, r, ratio, colour);
}
void arrow(void *frame, const float *p, float angle, DWORD r, DWORD g, DWORD b)
{
    fn<void(__fastcall *)(void *, const float *, float, float, DWORD, DWORD, DWORD)>(0x4fceb0)(frame, p, angle, raw_float(0x3f3677d4), r, g, b);
}
void colour_rgba(void *c, DWORD r, DWORD g, DWORD b, DWORD a) { fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(c, r, g, b, a); }
DWORD draw_text(void *f, void *frame, int x, int y, const char *s, DWORD just)
{
    return fn<DWORD(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(f, frame, x, y, s, just, 0);
}
bool not_empty(void *z) { return (BYTE)fn<DWORD(THISCALL *)(void *)>(0x4e13e0)(z) != 0; }
void static_colour(GlobKey colour, GlobKey guard, CodeKey dtor)
{
    BYTE &g = *glob<BYTE *>(guard);
    if (g & 1)
        return;
    g |= 1;
    colour_rgba(glob<void *>(colour), 0xff, 0x64, 0x64, 0xff);
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(key_fn_addr(dtor));
}

}  // namespace

// 0x4e4280 (part, frame)
void THISCALL LandscapeAI_Draw(BYTE *ai, void *part, void *frame)
{
    if (x87_le(fl<float>(ai, 0x18), cst(glob_addr(0x5ebed0))))
        return;
    float pos[2];
    vcall<DWORD>(part, 43, pos);
    BYTE colour[16];
    colour_rgba(colour, 0x3c, 0xcd, 0x3c, 0xff);
    const int x = x87_ftol(pos[0]);
    const int y = x87_ftol(pos[1]);
    switch (dw(ai, 0x24)) {
    case 0: {
        const LD w = fild64(dw(ai, 0x10)), h = fild64(dw(ai, 0x14));
        const float ratio = fstp(h / w);
        oval(frame, pos, fstp(w * cst(glob_addr(0x5eee2c))), ratio, colour);
        break;
    }
    case 1: {
        const int hh = (int)(dw(ai, 0x14) >> 1), hw = (int)(dw(ai, 0x10) >> 1);
        fn<DWORD(THISCALL *)(void *, int, int, int, int, const void *)>(0x4b33f0)(frame, x - hw, y - hh, hw + x, hh + y, colour);
        break;
    }
    }
}

// 0x565f40 (part, frame)
void THISCALL ScanAreaAI_Draw(BYTE *ai, void *part, void *frame)
{
    float pos[2];
    part_position(part, pos);
    arrow(frame, pos, fstp(fld(fl<float>(ai, 0x14)) * cst(glob_addr(0x5edb3c)) + fl<float>(ai, 0x48)), 0, 0xff, 0xff);
    arrow(frame, pos, fstp(fld(fl<float>(ai, 0x18)) * cst(glob_addr(0x5edb3c)) + fl<float>(ai, 0x48)), 0xff, 0xff, 0);
    if (ai[0x40])
        arrow(frame, pos, fstp(vcall<LD>(part, 46)), 0, 0, 0xff);
    else
        arrow(frame, pos, fstp(vcall<LD>(part, 46)), 0xff, 0x80, 0);
}

// 0x4cad40 (part, frame)
void THISCALL DrawTimeoutOvalAI_Draw(BYTE *ai, void *part, void *frame)
{
    static_colour(0x6bfe38, 0x6bfe4b, 0x4cadf0);
    float pos[2];
    vcall<DWORD>(part, 43, pos);
    oval(frame, pos, fl<float>(ai, 0x10), fl<float>(ai, 0x14), glob<void *>(0x6bfe38));
    if (not_empty(ai + 0x1c)) {
        const char *s = zs_c_str(ai + 0x1c);
        const int y = x87_ftol(pos[1]);
        const int x = x87_ftol(pos[0]);
        draw_text(bp(dw(ai, 0x20)), frame, x, y, s, 1);
    }
}

// 0x4cb110 (part, frame)
void THISCALL DrawTimeoutRectAI_Draw(BYTE *ai, void *part, void *frame)
{
    static_colour(0x6bfe28, 0x6bfe21, 0x4cb1e0);
    float pos[2];
    vcall<DWORD>(part, 43, pos);
    fn<DWORD(THISCALL *)(void *, const void *, const void *)>(0x4b3430)(frame, ai + 0x10, glob<void *>(0x6bfe28));
    if (not_empty(ai + 0x24)) {
        const char *s = zs_c_str(ai + 0x24);
        const int y = x87_ftol(pos[1]);
        const int x = x87_ftol(pos[0]);
        draw_text(bp(dw(ai, 0x28)), frame, x, y, s, 1);
        draw_text(bp(dw(ai, 0x28)), frame, (int)dw(ai, 0x10), (int)dw(ai, 0x14), zs_c_str(ai + 0x24), 0);
    }
}

ZAX_HOOKS(ai_draw)
{
    const Hook list[] = {
#include "ai_draw_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
