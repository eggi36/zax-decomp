// The frame and blend core of the software renderer (phase 4, batch R1 of the blitter map,
// docs/DECOMPILATION.md 9): the layouts of CFrame, CBlendMode and the colour and rectangle types, the
// vtable slots of a frame, the exe's tables and globals, and the reimplemented helpers the later
// batches (spans R2-R4, blitters R5, 24/32 bits R6, rasterizers R7) call.
//
// CFrame (0x40, vtable 0x5f4814 with 64 slots, class 0x6be7d4 "CFrame", version 2.0, parent
// CFrameInterface (8 bytes, vtable 0x5eb534, class 0x6bf84c); frame_class.cpp) [confirmed: constructor
// 0x4a8a90, destructor 0x4a8b00, the slots]:
//   +0x00 vtable, +0x04 CObject, +0x08/+0x0a the origin (signed words; slot 36's rectangle is (x, y,
//   x + width, y + height); drawing subtracts it), +0x0c the pixels (or the pixel block of a compressed
//   frame), +0x10/+0x11 floor(log2) of width and height, +0x12/+0x14 width and height, +0x18 the row
//   table (max(height, 2) dwords, row i at pixel i * pitch; or the pixel block's row offsets), +0x1c ..
//   +0x38 four more blocks of a compressed frame with their row tables (+0x1c/+0x20 the extra block,
//   +0x24/+0x28 the third, +0x2c/+0x30 the alpha block, +0x34/+0x38 the edge block; each starts with its
//   size in bytes), +0x3c the flags: 0x02 an 8-bit RLE frame, 0x04 a 16-bit RLE frame, 0x20/0x40/0x80/
//   0x100 the depth 8/16/24/32, 0x2000 kept from the file, 0x4000 a palette was taken (slot 59).
// The vtable slots 21-63 (slots 0-20 are CObject's):
//   21 MemorySize, 22 (CObject), 23 Blit (src, srcRect, dstPoint rect, mode), 24 ClearRect (rect),
//   25 Clear, 26 FillColor (FieldColor), 27 Fill (byte), 28 FillRect (rect, FieldColor), 29 bits per
//   pixel, 30 bytes per pixel, 31 pixels, 32 +0x1c, 33 +0x24, 34 +0x2c, 35 +0x34, 36 ClipRect (out),
//   37 the frame itself, 38 flags, 39 height, 40 pitch (row table entry 1), 41/42 log2 width/height,
//   43/44 origin x/y, 45 width, 46 row table, 47 +0x20, 48 +0x28, 49 +0x30, 50 +0x38, 51 DrawLine (x1,
//   y1, x2, y2, FieldColor), 52 DrawThickLine (the span rasterizer 0x42b160), 53 GetColor (x, y,
//   FieldColor out), 54 GetPixel (x, y), 55 SetPixel (x, y, FieldColor), 56 IsPixelSet (x, y),
//   57 TintRect (rect, table), 58 Palette (the CPaletteHolder's palette), 59 has a palette (flag
//   0x4000), 60/61 (stubs, 0), 62 BlitWithMode (src, srcRect, dstRect, mode: FindBlitter, then the
//   blitter), 63 LoadImage (file, filter list, ...; the TGA loader).
//
// CBlendMode (0x34; blend_core.cpp): +0x00 vtable (base 0x5eede4: 0 deleting destructor, 1 0x53bbc0,
// 2 FindBlitter, 3 0x5007b0, 4 WriteOpacity, 5 ReadOpacity, 6 0x401260), +0x04 the palette (0xc bytes,
// constructor 0x5266d0: +0 data, the mode's 64 KB blend table for 8-bit destinations), +0x10 the
// registry (an Array20: vtable 0x5eee00, +0x14 data, +0x18 count, +0x1c capacity; 20-byte records),
// +0x20 the opacity (float, 1.0), +0x24 the entry of 0x6757b8 for the opacity as 0..255, +0x28 the
// opacity as 0..255 (255), +0x2c/+0x30 the one-entry cache of FindBlitter (key, function).
#pragma once
#include "exe_base.h"
#include "x87.h"
#include "class_info.h"

namespace zax::render {

// ---- Types ---------------------------------------------------------------------------------------------

struct Rect {
    int left, top, right, bottom;
};
struct RectF {
    float left, top, right, bottom;
};
struct Point {
    int x, y;
};

// FieldColor (0x10): +0 b, g, r, a, +4 b, g, r again, +8 the 16-bit pixel, +0xa the palette index,
// +0xc flags (1 the index is valid, 2 made from RGB)
struct FieldColor {
    BYTE b, g, r, a;
    BYTE b2, g2, r2, pad7;
    WORD pixel16;
    BYTE index, padb;
    DWORD flags;
};
static_assert(sizeof(FieldColor) == 0x10);

struct CFrame {
    DWORD vftable;     // 0x00
    DWORD object_04;   // 0x04
    short x, y;        // 0x08
    BYTE *pixels;      // 0x0c
    BYTE log2_width;   // 0x10
    BYTE log2_height;  // 0x11
    WORD width;        // 0x12
    WORD height;       // 0x14
    WORD pad_16;       // 0x16
    DWORD *rows;       // 0x18
    void *blocks[8];   // 0x1c: +0x1c/+0x20, +0x24/+0x28, +0x2c/+0x30, +0x34/+0x38 (data, row table)
    DWORD flags;       // 0x3c
};
static_assert(sizeof(CFrame) == 0x40 && __builtin_offsetof(CFrame, rows) == 0x18 && __builtin_offsetof(CFrame, flags) == 0x3c);

// The proxy frames (frame_proxy.cpp): CProxyFrame (0x0c, vtable 0x5eb62c, class 0x712228) forwards every
// drawing slot to its target; COffsetClippedProxyFrame (0x3c, vtable 0x5f8ca8, class 0x6d0148) embeds an
// offset frame (0x14, vtable 0x5fb928) and a clip frame (0x1c, vtable 0x5efd34), both CProxyFrames
struct ProxyFrame {
    DWORD vftable;
    DWORD object_04;
    void *target;    // 0x08
};
struct OffsetFrame {
    ProxyFrame proxy;
    int dx, dy;      // 0x0c, 0x10
};
struct ClipFrame {
    ProxyFrame proxy;
    Rect clip;       // 0x0c
};
struct OffsetClippedProxyFrame {
    ProxyFrame proxy;     // 0x00 (target: the frame)
    OffsetFrame offset;   // 0x0c (target: the clip frame)
    ClipFrame clip;       // 0x20 (target: the frame)
};
static_assert(sizeof(OffsetClippedProxyFrame) == 0x3c && __builtin_offsetof(OffsetClippedProxyFrame, clip) == 0x20);

// The palette object (0xc; 0x5266d0 constructs it empty, 0x5266e0 frees it): +0 the data (256 x b, g, r
// for a game palette, 64 KB for a blend table), +4/+8 its bookkeeping
struct Palette {
    BYTE *data;
    DWORD d4, d8;
};

// A record of the blitter registry (20 bytes)
struct BlendRecord {
    DWORD src_bpp, src_flags, dst_bpp, dst_flags, fn;
};
struct BlendArray {     // Array20 (vtable 0x5eee00)
    DWORD vftable;
    BlendRecord *data;
    DWORD count, capacity;
};
struct CBlendMode {
    DWORD vftable;          // 0x00
    Palette palette;        // 0x04
    BlendArray registry;    // 0x10
    float opacity;          // 0x20
    DWORD opacity_entry;    // 0x24
    DWORD opacity255;       // 0x28
    DWORD cache_key;        // 0x2c
    DWORD cache_fn;         // 0x30
};
static_assert(sizeof(CBlendMode) == 0x34 && __builtin_offsetof(CBlendMode, opacity) == 0x20);

// fastcall (src, srcRect; dst, dstRect, mode), ret 0xc
using Blitter = void(__fastcall *)(CFrame *src, const Rect *src_rect, CFrame *dst, const Rect *dst_rect, CBlendMode *mode);

// ---- Constants ---------------------------------------------------------------------------------------------

constexpr DWORD F_RLE8 = 0x2, F_RLE16 = 0x4, F_COMPRESSED = 0x6, F_8 = 0x20, F_16 = 0x40, F_24 = 0x80, F_32 = 0x100,
                F_DEPTH = 0x1e0, F_PALETTE = 0x4000;
constexpr DWORD VT_FRAME_INTERFACE = 0x5eb534, VT_CFRAME = 0x5f4814, VT_DIB_FRAME = 0x5f4460, VT_PROXY_FRAME = 0x5eb62c,
                VT_OFFSET_CLIPPED = 0x5f8ca8, VT_OFFSET_FRAME = 0x5fb928, VT_CLIP_FRAME = 0x5efd34, VT_PALETTE_HOLDER = 0x5f47b8;
constexpr DWORD VT_BLEND_MODE = 0x5eede4, VT_BLEND_ARRAY = 0x5eee00;
constexpr DWORD CFRAME_CLASS = 0x6be7d4, DIB_FRAME_CLASS = 0x6be1dc, PALETTE_HOLDER_CLASS = 0x6be7d0;
constexpr DWORD PALETTE_HOLDER_FIELD = 0x6be38c;   // the dynamic field "Palette" of a frame (CPaletteHolder)

// Tables and globals
constexpr DWORD GAME_PALETTE = 0x633f98;           // CPaletteHolder-less palette object (Palette) of the game
constexpr DWORD COPY_MODE = 0x6bb4e0;              // the CBlendCopy object (the default blit)
constexpr DWORD ALPHA20_80 = 0x6767d0, ALPHA40_60 = 0x6767c0;   // palette objects with the 64 KB 8-bit blend tables
constexpr DWORD OPACITY_ENTRIES = 0x6757b8;        // 256 dwords: 5 - (6i >> 8), indexed by the opacity 0..255
constexpr DWORD FADE_TABLES = 0x6347b0, OPACITY_TABLE = 0x6b67e8;
constexpr DWORD PIXEL16_RED = 0x633394, PIXEL16_GREEN = 0x675bb8, PIXEL16_BLUE = 0x630378, PIXEL16_BUILT = 0x630578,
                PIXEL15_RED = 0x633b94, PIXEL15_GREEN = 0x6753b8;
constexpr DWORD PALETTE15 = 0x62fb34, PALETTE16 = 0x6343b4;   // 255 entries each: the game palette as 5-5-5 / 5-6-5

// ---- Exe functions this code calls (through their addresses, so Disable= and the checks apply) ------------------

inline void obj_ctor(void *o) { zax::fn<void *(THISCALL *)(void *)>(0x416610)(o); }
inline void obj_dtor(void *o) { zax::fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
inline void obj_init_fields(void *o) { zax::fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
inline void obj_free_fields(void *o) { zax::fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
inline bool obj_is_kind_of(const void *o, CClassInfo *c) { return zax::fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
// FieldColor: 0x53f160 the palette index (computed), 0x53f080 set from r, g, b, a, 0x53f100 from a palette
// index, 0x53f010 the constructor from r, g, b, a
inline BYTE color_index(FieldColor *c) { return (BYTE)zax::fn<DWORD(THISCALL *)(FieldColor *)>(0x53f160)(c); }
inline void color_set_rgba(FieldColor *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    zax::fn<void(THISCALL *)(FieldColor *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(c, r, g, b, a);
}
inline void color_from_index(FieldColor *c, DWORD index) { zax::fn<void(THISCALL *)(FieldColor *, DWORD)>(0x53f100)(c, index); }
inline void color_ctor_rgba(FieldColor *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    zax::fn<void(THISCALL *)(FieldColor *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(c, r, g, b, a);
}
// The palette object: 0x5266d0 constructor, 0x5266e0 free, 0x526700 assign (copy of another), 0x5268f0
// (count) allocate, 0x526e40 nearest index (r, g, b, first, end)
inline void palette_ctor(Palette *p) { zax::fn<void(THISCALL *)(Palette *)>(0x5266d0)(p); }
inline void palette_free(Palette *p) { zax::fn<void(THISCALL *)(Palette *)>(0x5266e0)(p); }
inline void palette_assign(Palette *p, const Palette *from) { zax::fn<void(THISCALL *)(Palette *, const Palette *)>(0x526700)(p, from); }
inline void palette_alloc(Palette *p, DWORD kind) { zax::fn<void(THISCALL *)(Palette *, DWORD)>(0x5268f0)(p, kind); }
inline DWORD palette_nearest(Palette *p, DWORD r, DWORD g, DWORD b, DWORD first, DWORD end)
{
    return zax::fn<DWORD(THISCALL *)(Palette *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526e40)(p, r, g, b, first, end);
}
// Dynamic fields (field_desc): 0x418fc0 is dynamic in, 0x418fb0 remove, 0x418fa0 add, 0x418790 the pointer,
// 0x4187c0 set the object pointer
inline bool dyn_has(DWORD field, void *o) { return zax::fn<bool(THISCALL *)(void *, void *)>(0x418fc0)(*at<void **>(field), o); }
inline void dyn_remove(DWORD field, void *o) { zax::fn<void(THISCALL *)(void *, void *)>(0x418fb0)(*at<void **>(field), o); }
inline void dyn_add(DWORD field, void *o) { zax::fn<void(THISCALL *)(void *, void *)>(0x418fa0)(*at<void **>(field), o); }
inline void *dyn_pointer(DWORD field, void *o) { return zax::fn<void *(THISCALL *)(void *, void *)>(0x418790)(*at<void **>(field), o); }
inline void dyn_set_object(DWORD field, void *o, void *v) { zax::fn<void(THISCALL *)(void *, void *, void *)>(0x4187c0)(*at<void **>(field), o, v); }

inline bool cpu_has_mmx() { return (BYTE)zax::fn<DWORD(__cdecl *)()>(0x5156b0)() != 0; }

// ---- Frame slots (calls through the vtable) ------------------------------------------------------------------

inline DWORD f_bits(const void *f) { return vcall<DWORD>(f, 29); }
inline DWORD f_bytes(const void *f) { return vcall<DWORD>(f, 30); }
inline BYTE *f_pixels(const void *f) { return vcall<BYTE *>(f, 31); }
inline void f_clip_rect(const void *f, Rect *out) { vcall<DWORD>(f, 36, out); }
inline DWORD f_flags(const void *f) { return vcall<DWORD>(f, 38); }
inline DWORD f_pitch(const void *f) { return vcall<DWORD>(f, 40); }
inline int f_x(const void *f) { return vcall<int>(f, 43); }
inline int f_y(const void *f) { return vcall<int>(f, 44); }
inline DWORD *f_rows(const void *f) { return vcall<DWORD *>(f, 46); }
inline Palette *f_palette(void *f) { return vcall<Palette *>(f, 58); }

// ---- Reimplemented helpers (frame_draw.cpp, frame_class.cpp, blend_core.cpp) ----------------------------------

// 0x432c10 (fastcall dst, value; count): count bytes from dst, the unaligned head and the tail with the low
// byte of value, the aligned middle with the whole dword; eax the value
DWORD __fastcall Mem_FillBytes(BYTE *dst, DWORD value, DWORD count);
// 0x4b2cd0 (fastcall clip, rect): rect cut to clip, top/bottom first; al whether it is not empty
DWORD __fastcall Rect_Intersect(const Rect *clip, Rect *r);
// 0x4b2c20 (fastcall clip, srcRect; dstRect): cuts dstRect to clip and moves srcRect's corners along
DWORD __fastcall Blit_ClipRects(const Rect *clip, Rect *src, Rect *dst);
// 0x4b2d30 (fastcall clip, &x1; &y1, &x2, &y2) and the float variant 0x4b2f50
DWORD __fastcall Line_ClipToRect(const Rect *clip, int *x1, int *y1, int *x2, int *y2);
DWORD __fastcall Line_ClipToRectF(const RectF *clip, float *x1, float *y1, float *x2, float *y2);
// 0x4339a0 (fastcall r, g; b): the 5-5-5 twin of Pixel16_FromRGB (frame_basics.cpp)
DWORD __fastcall Pixel15_FromRGB(DWORD r, DWORD g, DWORD b);

}  // namespace zax::render
