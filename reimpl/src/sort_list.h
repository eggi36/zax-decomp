// CSortList2D (0x579120-0x57aeb0): the spatial grid of a level's objects (the "Tree List" of the
// layer, CLayer +0x50), with its cell arrays, queries and file slots.
//
// Layout [confirmed: constructor 0x579340, Resize 0x5793e0, every method]:
//   CSortList2D (0x28, vtable 0x600a9c, class 0x713684, version 1.01, no parent class, no fields)
//     +0x08 max x: columns * 64 - 1      +0x0c max y: rows * 64 - 1
//     +0x10 columns                      +0x14 rows
//     +0x18 cells: columns * rows pointer arrays of 16 bytes (vtable 0x600a90, pool flavour, Add
//           0x5c1f00), one new[] block with the count in front
//     +0x1c the index of the first cell of every row (0x4a8440: max(rows, 2) entries i * columns)
//     +0x20 the owner (the CLayer; set by the layer, not by the constructor)
//     +0x24 the id given to the next part read as text (-1 after the constructor)
//   Cells are 64 pixels square. A part is listed in every cell its rectangle (slot 37 with kind 5)
//   touches; the cell rectangle it was listed with is kept in the part (+0x30 x0, +0x32 y0, +0x34 x1,
//   +0x36 y1, words; -9999 in all four when it is in no grid).
//
// The layer part (CLayerPart) members used here: +0x08 child list (0x4fc8f0 recurses into it), +0x18
// name (ZString), +0x1c flags (0x10000 marked for deletion, 0x20000/0x200000/0x800000 together: also
// registered with the layer's +0x7c object on insertion, 0x2000000 excluded from the line of sight),
// +0x20 a second flag word (0x57a6e0), +0x28 id (word), +0x2c the stamp of the last visit, +0x30 the
// cell rectangle, +0x48 the layer. Slots: 1 deleting destructor, 2 class, 7 (vf7 calls it once per
// part), 31/32/35 (the three passes of Process), 37 rectangle (rect *, kind, 0) -> al, 43 position
// (float[2]), 58 set flags, 59 clear flags, 74 after insertion, 75 before removal.
//
// Stamps: a global counter (0x622200, 1 after 0xefffffff) marks the parts visited by one walk, so a
// part listed in several cells is visited once. The result lists of the queries (world class, methods
// 0x59a0f0-0x59a510: {vtable, data, count, capacity, stamp}) keep the stamp of the query.
//
// Return values are what the original leaves in eax; the comments say where that is not defined.
#pragma once
#include "exe_base.h"
#include "ptr_array.h"

namespace zax {

struct CFileBuf;

constexpr DWORD SORT_LIST_VTABLE = 0x600a9c;
constexpr DWORD SORT_CELL_VTABLE = 0x600a90;
constexpr DWORD SORT_STAMP = 0x622200;
constexpr short NOT_LISTED = -9999;   // 0xd8f1

// A layer part as the grid sees it (offsets only; the real classes are much bigger)
struct GridPart {
    DWORD vftable;
    DWORD dynamic_properties;
    void *child_list;   // 0x08
    BYTE unknown0c[0x0c];
    void *name;         // 0x18 ZString
    DWORD flags;        // 0x1c
    DWORD flags2;       // 0x20
    DWORD unknown24;
    WORD id;            // 0x28
    WORD unknown2a;
    DWORD stamp;        // 0x2c
    short cells[4];     // 0x30 x0, y0, x1, y1
};

struct CSortList2D {
    DWORD vftable;
    void *dynamic_properties;
    int max_x;          // 0x08
    int max_y;          // 0x0c
    DWORD columns;      // 0x10
    DWORD rows;         // 0x14
    ZPtrArray *cells;   // 0x18
    DWORD *row_start;   // 0x1c
    void *owner;        // 0x20 CLayer
    int next_id;        // 0x24

    ZPtrArray *cell(DWORD x, DWORD y) { return &cells[row_start[y] + x]; }

    static CSortList2D *THISCALL Ctor(CSortList2D *self);                          // 0x579340
    static DWORD THISCALL Dtor(CSortList2D *self);                                 // 0x579370
    static void *THISCALL DeletingDtor(CSortList2D *self, DWORD flags);            // 0x5791f0 (slot 1)
    static DWORD THISCALL Precache(CSortList2D *self);                             // 0x579850 (slot 7)
    static DWORD THISCALL WriteText(CSortList2D *self, CFileBuf *w);               // 0x579600 (slot 9)
    static DWORD THISCALL ReadOwnLine(CSortList2D *self, const char *name, char *value, CFileBuf *r);   // 0x579560 (slot 11)
    static DWORD THISCALL WriteBinary(CSortList2D *self, CFileBuf *w);             // 0x5797e0 (slot 13)
    static DWORD THISCALL ReadBinary(CSortList2D *self, CFileBuf *r);              // 0x5796d0 (slot 15)
    static DWORD THISCALL ResetFields(CSortList2D *self);                          // 0x5793c0 (slot 19)
    static DWORD *THISCALL Resize(CSortList2D *self, DWORD width, DWORD height);   // 0x5793e0
    static DWORD THISCALL Insert(CSortList2D *self, GridPart *part);               // 0x579900
    static DWORD THISCALL Remove(CSortList2D *self, GridPart *part);               // 0x579a80
    static DWORD THISCALL RemoveFromCells(CSortList2D *self, GridPart *part);      // 0x579ab0
    static DWORD THISCALL Update(CSortList2D *self, GridPart *part);               // 0x579c10
    static short *THISCALL RectToCells(CSortList2D *self, short *r);               // 0x579de0
    static DWORD THISCALL IsListed(CSortList2D *self, GridPart *part, int check_cell);   // 0x579e70
    static DWORD THISCALL RemoveAndDelete(CSortList2D *self, GridPart *part);      // 0x579f10
    static DWORD THISCALL DeleteAll(CSortList2D *self);                            // 0x579f50
    static DWORD THISCALL Process(CSortList2D *self, ZPtrArray *parts, DWORD arg);  // 0x57a030
    static DWORD THISCALL QueryRect(CSortList2D *self, ZPtrArray *list, const int *rect, int kind, DWORD all,
                                    DWORD none, DWORD any, BYTE use_any);          // 0x57a100
    static DWORD THISCALL MarkDied(CSortList2D *self, int bit);                    // 0x57a6e0
    static DWORD THISCALL FindRect(CSortList2D *self, ZPtrArray *list, const int *rect, int kind, DWORD all,
                                   DWORD none, DWORD any);                         // 0x57a770
    static DWORD THISCALL QueryName(CSortList2D *self, ZPtrArray *list, const char *name, DWORD length, DWORD all,
                                    DWORD none, DWORD any, BYTE use_any);          // 0x57a7e0
    static GridPart *THISCALL FindId(CSortList2D *self, const float *point, DWORD id);   // 0x57a9a0
    static DWORD THISCALL FindName(CSortList2D *self, ZPtrArray *list, const char *name, DWORD all, DWORD none,
                                   DWORD any);                                     // 0x57aa80
    static DWORD THISCALL FindClass(CSortList2D *self, ZPtrArray *list, void *cls, DWORD all, DWORD none,
                                    DWORD any);                                    // 0x57aae0
    static DWORD THISCALL QueryClass(CSortList2D *self, ZPtrArray *list, void *cls, DWORD all, DWORD none, DWORD any,
                                     BYTE use_any);                                // 0x57ab20
    static DWORD THISCALL QueryRadius(CSortList2D *self, void *callback, DWORD context, const float *point,
                                      GridPart *except, float radius, float y_scale, int kind, DWORD all,
                                      DWORD none, DWORD any, BYTE line_of_sight, void *cls);   // 0x57ac50
};
static_assert(sizeof(CSortList2D) == 0x28);

// 0x57aeb0 (stdcall; ecx not used): the next stamp, given to the list (if any)
DWORD STDCALL SortList_NewStamp(ZPtrArray *list);

}  // namespace zax
