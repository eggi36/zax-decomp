// Small self-contained functions from ranges that docs/functions.csv counts as Core through their
// neighbours only, reimplemented with the batch around the Local.txt table:
//   0x5003e0 the 12-byte assignment the layer part's slot 59 (0x5001d0) uses (three dwords, returns the
//            target) [confirmed]; a point or vector of three floats [assumption].
//   0x500570 Reserve(n) of the segment array of CLightningLayerPart (called by its slot 23 0x4ff500,
//            Rendering): {vtable, data, count, capacity} of 0x58-byte elements whose constructor only
//            writes the vtable 0x5f99c8; a new block from operator new of max(n, capacity * 1.2) elements
//            (all get the vtable), every member of the old elements copied except the vtable and the
//            bytes +0x0d-+0x0f and +0x27, the old block deleted.
//   The DirectPlay 8 code of the multiplayer (0x47f000-0x480fff):
//   0x47f350 the text of a DirectPlay result (fastcall, code in ecx): 24 known codes (DPNERR_* and
//            DPNSUCCESS_PENDING 0x15800e) give the exe's own texts, every other one is printed with
//            "Unknown error %08x" into the static buffer 0x6bdae8.
//   0x480c80 clears the 100 player slots of the DirectPlay session object (12 bytes from +0x44c: the
//            bytes +0 and +1 and the dword +8 of each) and the byte +0x8fc inside the critical section
//            0x6bdbf0 [confirmed: code; "player slots" is an assumption].
//   0x480cc0 removes element i of a pointer array {vtable, data, count} after deleting it through its
//            slot 0 (flags 1), the same as the inlined loop in 0x480530.
#pragma once
#include "exe_base.h"

namespace zax {

struct ZVec3 {
    DWORD v[3];
    ZVec3 *THISCALL Assign(const ZVec3 *other);   // 0x5003e0
};

struct LightningSegment {
    DWORD vftable;   // 0x5f99c8
    BYTE data[0x54];
};
static_assert(sizeof(LightningSegment) == 0x58);

struct ZSegmentArray {
    DWORD vftable;
    LightningSegment *data;
    DWORD count;
    DWORD capacity;
    DWORD THISCALL Reserve(DWORD n);   // 0x500570
};

struct ZOwningPtrArray {
    DWORD vftable;
    void **data;
    DWORD count;
    DWORD THISCALL RemoveAndDeleteAt(DWORD index);   // 0x480cc0
};

const char *__fastcall DPlay_ErrorText(DWORD code);   // 0x47f350
DWORD THISCALL DPlaySession_ClearSlots(BYTE *self);   // 0x480c80

constexpr DWORD DPLAY_ERROR_BUFFER = 0x6bdae8;
constexpr DWORD DPLAY_LOCK = 0x6bdbf0;
constexpr DWORD VT_LIGHTNING_SEGMENT = 0x5f99c8;

}  // namespace zax
