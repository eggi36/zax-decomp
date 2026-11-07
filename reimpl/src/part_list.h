// The part lists of the world (0x59a0f0-0x59a520): result lists of the grid queries and the pool of
// temporary lists that the queries, the collision sweep and the draw code borrow.
//
// A part list (0x14 bytes) [confirmed: code, the callers in sort_list.cpp, layer.cpp, polygon.cpp]: the
// pool pointer array of ptr_array.h {vtable, data, count, capacity} plus +0x10 a stamp. A part listed by
// AddNew gets the list's stamp in its +0x2c, so a walk that sees it again (it is listed in several grid
// cells) skips it while the stamp is the same. Growth as in every array template: max(count + 1,
// capacity * 1.2) through __ftol, the data from the global pool.
//
// The temporary lists (0x59a470/0x59a510): nodes of 0x20 bytes {next, prev, list (vtable 0x602dbc, its
// slot 1 the shared RemoveAll), +0x1c "free" byte} in a circular chain whose sentinel is the static node
// 0x713e20 (its own list at 0x713e28 is never handed out; the static initializer 0x59a370 sets it up,
// 0x59a3c0 frees every node at exit). GetTemp takes the first free node or appends a new one (operator
// new; a list stamp of -1) behind the last node, marks it used and stores the list into *slot; Release
// marks it free again. A list keeps its parts and capacity while free: users clear it themselves.
//
// The sort 0x59a2d0 (qsort 0x5d097c with the comparator 0x59a2f0) [confirmed: code]: the comparator
// means to order by slot 56 + slot 53 (the bottom of a part: sort offset plus rounded y) and then by
// slot 52 (rounded x), but it reads the first part for both sides of the first comparison (an error of
// the original), so that comparison is always equal and the parts end up ordered by x alone; it never
// returns 0 (-1 when a's x is less, else 1). The slots are still called in the original's order.
//
// Return values are what the original leaves in eax where callers can see it.
#pragma once
#include "ptr_array.h"

namespace zax {

struct PartList : ZPtrArray {
    DWORD stamp;   // 0x10
    // the "free" byte of a temporary list's node, right behind the list
    BYTE &free_byte() { return reinterpret_cast<BYTE *>(this)[0x14]; }

    static DWORD THISCALL SetStamp(PartList *self, DWORD stamp);   // 0x59a0f0 (eax: the count)
    static DWORD THISCALL Clear(PartList *self);                   // 0x59a120 (eax: slot 1's)
    static DWORD THISCALL AddNew(PartList *self, BYTE *part);      // 0x59a140 (eax: the count after)
    static DWORD THISCALL Append(PartList *self, void *part);       // 0x59a220 (eax: the count after)
    static DWORD THISCALL SortByX(PartList *self);                 // 0x59a2d0 (eax: qsort's, 0 when empty)
    static PartList **THISCALL GetTemp(PartList **slot);           // 0x59a470
    static PartList *THISCALL ReleaseTemp(PartList **slot);        // 0x59a510
    static ZPtrArray *THISCALL InitArray(ZPtrArray *self);         // 0x59a520 (vtable 0x5f8b98, all 0)
};
static_assert(sizeof(PartList) == 0x14);

struct TempListNode {
    TempListNode *next, *prev;
    PartList list;   // 0x08
    BYTE free;       // 0x1c
    BYTE pad[3];
};
static_assert(sizeof(TempListNode) == 0x20);

int __cdecl PartList_CompareBottomX(void *const *a, void *const *b);   // 0x59a2f0

constexpr DWORD TEMP_LIST_SENTINEL = 0x713e20;
constexpr DWORD VT_TEMP_PART_LIST = 0x602dbc, VT_PLAIN_PTR_ARRAY = 0x5f8b98;

}  // namespace zax
