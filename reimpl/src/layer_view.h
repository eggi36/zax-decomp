// CLayer's mini map mapping and view position (0x4ec280, 0x4ec3c0, 0x4ed6e0-0x4ed8f0).
//
// The layer (layer.h) members used [confirmed: code]: +0x48/+0x4c the level's width and height (save
// data +0x0c/+0x10), +0x7c the tile map (CPlasmaTileMap; +0x2cfc/+0x2d00 its size in pixels), +0x80 the
// "View Position" (float point), +0x88 the "Int View Position", +0x1a8/+0x1ac/+0x1b0/+0x1b4 the top,
// bottom, left and right clipping ("Mini-Map/Clip ..."), +0x1b8/+0x1bc/+0x1c0/+0x1c4 "Debug/MiniMap X
// Offset", "X Scale", "Y Offset", "Y Scale". Slot 36 (0x90) of the layer is the visible rectangle
// {x0, y0, x1, y1} of its frame; 0x524fe0 sets the frame's offset (+0x18/+0x1c).
//
// Mini map [confirmed: code; zax-formats/docs/FORMATS.md, the mini map mapping]:
//  - 0x4ec280 (out rect): the level without its clipping, divided by the world's slot 100 (the zoom
//    factor, stored as a float) and truncated, then shrunk by the factor min(1, mini map height / h,
//    mini map width / w) (the world's slot 99 gives the mini map size {w, h}; the width test compares
//    w * factor with the width as floats), the size truncated again and centred (the margins halved
//    towards 0). The width is tested against the factor that the height test left.
//  - 0x4ec3c0 (out rect, in rect): with X or Y scale 0 the scales become image size / clipped level size
//    (unsigned, through fild qword) and the offsets image origin - clip * scale (the x offset with the
//    unrounded x scale, the y offset with the stored float), stored into the save data; then every
//    coordinate is coordinate * scale + offset, truncated (__ftol).
// View [confirmed: code]:
//  - 0x4ed7f0 (float pos[2]): without a tile map nothing. x below 0 becomes 0, else at most
//    ((tile map width - 1) & ~0x7f) - visible width (not below 0); y below 0 becomes 0, else at most
//    ((tile map height - 1) & ~0x7f) - visible height (that limit may be negative and is stored as it is).
//  - 0x4ed6e0 (pos, target x, y, offset x, y, box width, height): moves pos so that the target lies in a
//    box of half the given size around pos + offset / 2 (x through a float, y unrounded), not below 0,
//    then 0x4ed7f0.
//  - 0x4ed8f0 (x, y): with a tile map the view position set and clamped (0x4ed7f0), the integer view
//    position rounded with the FPU's mode (fistp), the frame's offset set (0x524fe0).
//
// Elsewhere: 0x4eda60 (a 3-byte add and a jump to the save data's 0x4e7870, which starts the level's actions: not
// hookable with a trampoline and not callable in a check; short_hooks.cpp), 0x4ed960 (the layer whose ambient
// sounds play: sound objects) and 0x4ed350 (world_rest.cpp), the drawing of the mini map 0x4ec600, 0x4ece20,
// 0x4ed150 (Rendering, world_c5.cpp).
//
// Return values are what the original leaves in eax where callers can see it.
#pragma once
#include "exe_base.h"

namespace zax {

struct LayerView {
    static DWORD THISCALL MiniMapImageRect(BYTE *layer, int *out);                   // 0x4ec280 (eax: the left margin)
    static DWORD THISCALL WorldRectToMiniMap(BYTE *layer, int *out, const int *in);  // 0x4ec3c0 (eax: the last __ftol)
    static DWORD THISCALL ScrollToward(BYTE *layer, float *pos, float tx, float ty, float ox, float oy, float w,
                                       float h);                                     // 0x4ed6e0 (eax not reproduced)
    static DWORD THISCALL ClampView(BYTE *layer, float *pos);                        // 0x4ed7f0 (eax not reproduced)
    static DWORD THISCALL SetView(BYTE *layer, float x, float y);                    // 0x4ed8f0 (eax: 0x524fe0's or 0)
};

}  // namespace zax
