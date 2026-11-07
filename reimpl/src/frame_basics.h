// The proving module of the frame check harness (checks_frame_common.h): five small, self-contained
// drawing functions reimplemented and checked in all frame formats and both MMX paths.
//
//  - Pixel16_FromRGB (0x433a10, fastcall r, g, b on the stack): the 5-6-5 pixel from the tables 0x633394
//    (red << 11), 0x675bb8 (green << 5) and 0x630378 (blue) when InitBlendTables has built them (entry
//    0x80 of the blue table, 0x630578, not 0), else min((r + 4) >> 3, 31) << 11 | min((g + 2) >> 2, 63) << 5
//    | min((b + 4) >> 3, 31). The tables are indexed with the arguments unclipped.
//  - Frame_FillRect (0x4abe90, CFrame slot 28, (rect, FieldColor)): the rectangle cut to the frame's
//    rectangle (slot 36, Rect_Intersect 0x4b2cd0; the intersect's eax when it is empty), moved by the
//    origin, then by depth: 8 bits the colour's palette index (+0xa when flag 1 is set, else
//    FieldColor_PaletteIndex 0x53f160) row by row with the byte fill 0x432c10; 16 bits with alpha 0xff
//    the pixel (+8) stored, below that blended: with MMX (Cpu_HasMmx) groups of four pixels per row, the
//    rest (width mod 4, at most three) with the scalar formula. 24 bits the bytes +4..+6, the first pixel
//    copied along the row; 32 bits and any other depth: nothing (eax the depth - 0x80). The pitch comes
//    from entry 1 of the row table.
//  - CFrame::Clear (0x4a9010, slot 25): the whole buffer (slot 40 the pitch * height * bytes per pixel)
//    zeroed with 0x432c10; nothing without pixels or for another depth.
//  - CFrame::Fill (0x4ac490, slot 27, (value)): slot 30 (bytes per pixel) * height * width bytes from the
//    start of the buffer set to the low byte of value (the pitch ignored: with padding the fill runs into
//    the padding and stops short of the last rows).
//  - CFrame::ClipRect (0x4aa520, slot 36, (out)): (x, y, x + width, y + height) of the origin and size.
//
// Quirk kept (Frame_FillRect, 16 bits, alpha below 0xff): without MMX the code still splits each row into
// groups of four and a rest, but only the rest (width mod 4 pixels) is drawn, at the position where the
// previous row's rest ended plus the row skip (pitch - width): the groups of four are left unchanged and the
// rows drift towards the start of the buffer. On an MMX CPU (every CPU the game runs on today) the whole
// rectangle is blended. [confirmed: code 0x4ac082-0x4ac364; checked with the MMX flag off]
//
// [Reimpl] FrameMutation= (checks only, 0 normally): 1 rounds the MMX blend's blue channel with 0x11 instead
// of 0x10, 2 lets the 16-bit solid fill write one pixel past the right edge of its first row; the frame
// harness must report both (docs/DECOMPILATION.md 9).
#pragma once
#include "reimpl.h"

namespace zax {

DWORD __fastcall Pixel16_FromRGB(DWORD r, DWORD g, DWORD b);   // 0x433a10

inline int g_frame_mutation;

}  // namespace zax
