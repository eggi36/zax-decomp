// The frame load filters of batch R7 (docs/DECOMPILATION.md 9): the filter objects CSequence (0x56b440)
// and the TGA loader (CFrame slot 63) chain in a list and run over a frame after loading it.
//
// A filter (0x14 bytes, more for some kinds) [confirmed: constructors, FrameFilterList_Add 0x4b1db0]:
//   +0x00 vtable (slot 0 deleting destructor, 1 Clone, 2 Apply (frame, rect); al the result),
//   +0x04 the next filter, +0x08 the kind (2-12), +0x0c a bit depth (-1: none; the list's own target
//   depth), +0x10 the lock byte (the list refuses changes while set), +0x14 the kind's parameter.
// The list is itself a filter (the head, kind and Apply of the base vtable 0x5f4934); the filters after it
// are sorted by kind and a kind appears once (FrameFilterList_Insert 0x4b2060 replaces it). The
// destructors delete the rest of the chain while the byte 0x613d78 is set; Insert clears it around
// deleting the filter it replaces.
// The kinds: 2 move the origin to the centre, 3 alpha below a threshold to 0 (32 bits), 4 dark pixels
// transparent ("Blend Black Bias"), 5 dark pixels transparent below a threshold ("Blend Amplify Light"),
// 6 crop to the drawn pixels, 7 un-premultiply the alpha (32 bits), 8/9 the RLE encoder (8 or 16 bits),
// 10 build the palette of an 8-bit frame, 11 the depth conversion (+0x14 the depth, +0x18 dither; "12"
// in FrameFilterList_Add is kind 11 with dither).
#pragma once
#include "frame.h"

namespace zax::render {

struct FrameFilter {
    DWORD vftable;
    FrameFilter *next;   // 0x04
    DWORD kind;          // 0x08
    int depth;           // 0x0c
    BYTE locked;         // 0x10
    BYTE pad_11[3];
    DWORD param;         // 0x14
};
static_assert(sizeof(FrameFilter) == 0x18);

constexpr DWORD VT_FILTER = 0x5f4934, VT_FILTER_CONVERT = 0x5f4914, VT_FILTER2 = 0x5f497c, VT_FILTER3 = 0x5f4970,
                VT_FILTER4 = 0x5f494c, VT_FILTER5 = 0x5f4940, VT_FILTER6 = 0x5f4964, VT_FILTER7 = 0x5f4958, VT_FILTER10 = 0x5f4988;
constexpr DWORD FILTER_DELETE_CHAIN = 0x613d78;   // byte: the destructors delete the rest of the chain
constexpr DWORD INV_COLOR15_HOLDER = 0x633388;    // the palette object of the 5-5-5 inverse colour table

// The pool (operator_new_ecx 0x417710, pool_delete 0x54d130 (block, size)) and the colour depth of the display
inline void *pool_new(DWORD n) { return fn<void *(__fastcall *)(DWORD)>(0x417710)(n); }
inline void pool_delete(void *p, DWORD n) { fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(p, n); }
inline int color_depth() { return fn<int(__cdecl *)()>(0x55d030)(); }
inline CClassInfo *frame_class() { return fn<CClassInfo *(__cdecl *)()>(0x4a8760)(); }
// The frame when it is a CFrame (Object_IsKindOf), else null, as the filters test their argument
inline CFrame *as_frame(void *f) { return f && obj_is_kind_of(f, frame_class()) ? static_cast<CFrame *>(f) : nullptr; }
inline void frame_set_depth(void *f, DWORD bits) { fn<void(THISCALL *)(void *, DWORD)>(0x4aa240)(f, bits); }
inline DWORD pixel16(DWORD r, DWORD g, DWORD b) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(r, g, b); }

}  // namespace zax::render
