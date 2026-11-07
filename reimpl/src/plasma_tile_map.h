// CPlasmaTileMap, the Plasma Ground of a level, without its drawing: constructor and destructor
// (0x53f490, 0x53f6d0), the file slots 7, 9, 11, 13, 15, the grid setup (0x53f680, 0x53fe00), the
// elevation grid and its tile cache bookkeeping (0x540420-0x540640, 0x541fb0-0x542140), and the texture
// list functions the tile map uses (0x4b40f0-0x4b4670).
//
// CPlasmaTileMap (0x2d48, vtable 0x5fcd98, class 0x7110d8; world_data.h has its class functions)
// [confirmed: constructor, destructor, the slots below; zax-formats/docs/FORMATS.md, Plasma Ground]:
//   +0x08 an object of 0x480e20/0x480e80 (the tiles drawn so far; sized by 0x480ee0/0x480f50)
//   +0x60 a byte (0; drawing: shows the grid)   +0x64/+0x464/+0x864/+0xc64 per-band tables (drawing)
//   +0x1064 the texture list (0x160 bytes, vtable 0x5fccf4; owner the texture class 0x4a8760)
//   +0x1068 "Blending"   +0x106c 256 band entries of 0x1c bytes (texture, masks, second texture, blend
//           table; built by 0x541710: drawing)
//   +0x2c6c "Elevations" and +0x2cac "Seeds": frames of 8 bits per grid point (CFrame, 0x40 bytes;
//           slot 54 reads a point, 0x4a8b60 writes one, 0x4aad30 the address of (x, y))
//   +0x2cec a dword (0 by the grid setup), +0x2cf8 a countdown of 0x53ff90/0x541710 (drawing)
//   +0x2cfc/+0x2d00 the size in pixels, +0x2d04/+0x2d08 the cell size (64), +0x2d0c/+0x2d10 the grid's
//   columns and rows, +0x2d14/+0x2d18 the cell masks, +0x2d1c/+0x2d20 the cell shifts (0x509680: the
//   highest set bit)
//   +0x2d24 the tile cache handles: columns * rows * 3 dwords (layer 0 the ground, 1 and 2 the
//   transitions; +0x2d28 their count) in one new[] block with the count in front (0x4d2660 clears a
//   handle, 0x4d2670 releases its tile from the cache)
//   +0x2d2c-+0x2d34 counters of the drawing, +0x2d38 a dword, +0x2d3c/+0x2d40 two owned objects (made by
//   the drawing; deleted through slot 1 and slot 0), +0x2d44 the layer
// The tile cache 0x7110e0 (0x44 bytes, 0x4d27c0 with 0x480000 bytes, 0x4d2880 destroys it) is shared by
// all tile maps: the constructor makes it when the count 0x7110dc is 0, the destructor frees it when the
// count drops to 0. The constructor also refills the 256 plasma tables of 0x400 bytes at 0x6d1030 with
// 0x55c390 and the seeds 0xe6cef * (1 ... 256) (zax-formats/zaxtools/plasma.py).
//
// Slots [confirmed: code]: 7 precache (CObject's, then the texture list's 0x4b4670); 9 writes "Num
// Textures=%d", "Texture %d=%s" per entry, then "Elevations Row %d=" and "Seeds Row %d=" with every row in
// hex (0x587fa0); 11 reads those lines ("Num Textures" sets the count (0x4b4540); "Texture <i>" looks the
// name up in the texture definitions 0x6c1d50 (slot 2; -1 becomes 0) and copies that entry (0x4b4430);
// row 0 of the elevations or seeds sizes the frame to the grid (0x4ab5a0, 8 bits), the elevations' row 0
// also rebuilds the tile cache handles; a row inside the grid is decoded from hex (0x587ff0); other lines
// go to CObject's 0x417500); 13 writes the count, the names (counted texts) and both frames' bytes; 15
// reads them back (both frames sized, the handles rebuilt; al 1).
//
// The elevation grid [confirmed: code]: 0x540440 a point; 0x540460 sets a point and, when it changed,
// releases the tile cache handles of the cells that touch it (layers 0-2 of the cell, of the cell to
// the left, the one above and (layer 0-2) the one above left); 0x5405e0 adds to a point, clamped to
// 0-255; 0x540640 releases the handles of the cells in a pixel rectangle (layer 0: all three, 1: layers 1
// and 2, 2: layer 2), nothing without the tile cache or a cell size; 0x540420 the cell of a pixel
// coordinate rounded to the nearest cell (x + cell / 2 >> shift); 0x542000 the elevation at a pixel,
// bilinear between the four grid points around it (x87, __ftol: the band); 0x541fd0 that band's
// texture; 0x542140 the address of a handle; 0x541fb0 empties the tile cache (0x4d2b50).
//
// The texture list functions [confirmed: code]: 0x4b40f0 the constructor of the texture list (record
// list of 0x80-byte entries, aliases, the +0x140 object of 0x4b1cc0, the owner at +0x154, vtable
// 0x5f4994); 0x4b4160 the deleting destructor of 0x5f4994 (0x15c bytes); 0x4b4220 hands the +0x140
// object to another list (0x4b1d50) and sets the byte +0x158; 0x4b4430 copies an entry of another list
// (loading the source entry through its slot 37 when it has no object yet; a source index past its end
// loads entry 0 but copies the given one) and remembers the source at +0x15c; 0x4b4540 sets the number of
// entries (slot 40 first; one more entry allocated than counted, all cleared); 0x4b4670 precaches every
// entry through the source list's slot 12 with the index of the source entry holding the same object (-1
// when none).
//
// Elsewhere (Rendering): 0x53ff90 and 0x540000 (draw a region), 0x540790-0x541250 (build and draw tiles, the
// plasma mix), 0x541710 (the band table: textures of the bands and their masks per colour depth; all in
// tile_map_draw.cpp), 0x4b4240 (the texture list's slot 37, reads a texture file; frame_rest4a.cpp), the slot 34
// copy 0x4b46e0 (a 5-byte jump; short_hooks.cpp).
//
// Return values are what the original leaves in eax where callers can see it.
#pragma once
#include "world_data.h"

namespace zax {

struct PlasmaMap {
    static void *THISCALL Ctor(BYTE *self);                                        // 0x53f490
    static DWORD THISCALL Dtor(BYTE *self);                                        // 0x53f6d0 (eax: CObject_Dtor's)
    static DWORD THISCALL Precache(BYTE *self);                                    // 0x53f660 (slot 7)
    static DWORD THISCALL WriteOwnLines(BYTE *self, void *f);                      // 0x53f7c0 (slot 9; eax not reproduced)
    static DWORD THISCALL ReadOwnLine(BYTE *self, const char *name, const char *value, void *r);   // 0x53f920 (slot 11)
    static DWORD THISCALL WriteBinary(BYTE *self, void *f);                        // 0x53fbc0 (slot 13; eax: FileBuf_Write's)
    static DWORD THISCALL ReadBinary(BYTE *self, void *f);                         // 0x53fc50 (slot 15; al 1)
    static DWORD THISCALL SetPixelSize(BYTE *self, DWORD w, DWORD h);              // 0x53f680 (eax: 0x53fe00's)
    static DWORD THISCALL SetGrid(BYTE *self, DWORD cols, DWORD rows, DWORD cw, DWORD ch);   // 0x53fe00 (eax: the handles or 0)
    static DWORD THISCALL CellOf(BYTE *self, DWORD v);                             // 0x540420
    static DWORD THISCALL Elevation(BYTE *self, DWORD x, DWORD y);                 // 0x540440
    static void THISCALL SetElevation(BYTE *self, DWORD x, DWORD y, DWORD v);      // 0x540460 (eax not reproduced)
    static void THISCALL AddElevation(BYTE *self, DWORD x, DWORD y, int d);        // 0x5405e0 (eax not reproduced)
    static void THISCALL ReleaseRect(BYTE *self, const int *rect, DWORD layer);    // 0x540640 (eax not reproduced)
    static DWORD THISCALL FlushCache(BYTE *self);                                  // 0x541fb0 (eax: 0x4d2b50's)
    static DWORD THISCALL TextureAt(BYTE *self, DWORD x, DWORD y);                 // 0x541fd0
    static int THISCALL BandAt(BYTE *self, DWORD x, DWORD y);                      // 0x542000
    static DWORD *THISCALL Handle(BYTE *self, DWORD x, DWORD y, DWORD layer);      // 0x542140
};

struct TextureListOps {
    static TextureList *THISCALL Ctor(TextureList *self, void *owner);             // 0x4b40f0
    static TextureList *THISCALL DeletingDtor(TextureList *self, DWORD flags);     // 0x4b4160 (slot 0 of 0x5f4994)
    static DWORD THISCALL ShareExtra(TextureList *self, void *other);              // 0x4b4220 (eax: 0x4b1d50's)
    static TextureList *THISCALL CopyEntry(TextureList *self, DWORD dst, ZDefinitionList *src, DWORD i);   // 0x4b4430
    static DWORD THISCALL SetCount(TextureList *self, DWORD n);                    // 0x4b4540 (eax: RecordList_Set's)
    static DWORD THISCALL PrecacheEntries(TextureList *self);                      // 0x4b4670 (eax not reproduced)
};

constexpr DWORD TILE_CACHE = 0x7110e0, TILE_MAP_COUNT = 0x7110dc, PLASMA_TABLES = 0x6d1030;
constexpr DWORD TEXTURE_DEFINITIONS = 0x6c1d50, VT_TEXTURE_LIST = 0x5f4994;

}  // namespace zax
