// The path classes of Zax.exe (0x535000-0x539fff) and the A* search over the way point map:
// CPathCollideChecker (0x535c00-0x535d60), CPointList (0x536090-0x536450), CPathPointListListCache
// (0x536f90-0x537370), CPathLineSegment (0x539af0-0x539ce0), the path search (0x5354b0-0x535b90) and
// three small helpers (0x536080, 0x5396c0, 0x539ac0).
//
// They are the data of the AI's path finding on the way point map (CWayPointMap, World; the CLayer
// keeps its map at +0x134 and reads it from the level's .way file): the map's way points (CPathWayPoint)
// are joined by line segments (CPathLineSegment), a path is a CPointList of way point positions, the
// cache hands point lists to the paths of the moving objects, and a CPathCollideChecker answers for
// one moving object whether a segment can be walked and what it costs. functions.csv counted the
// classes as Core; by their use they belong to the AI and the world.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CPathCollideChecker (8, vtable 0x5eebe8, class 0x6d052c, version 1, parent CObject, no fields): slots
//     23 (whether the line between two points is blocked, 0x535dc0), 24 (its cost factor, 0x535eb0) and 25 (0x536030) test the
//     line through the layer's collision (0x4e8d40, 0x4e8e00; World, polygon.cpp); a derived
//     checker's vtable follows at 0x5eec54 (slots 0x430860, 0x430890).
//   CPointList (0x1c, vtable 0x5fc634, class 0x6d0658, version 1): +8 "Is Available" (byte), +0xc
//     "Point" (array of integer points: vtable 0x5f8dc4, data +0x10 from the pool, count +0x14,
//     capacity +0x18)
//   CPathPointListListCache (0x18, vtable 0x5fc6ec, class 0x6d0660, version 7, alternative class name
//     "CActivePathData"): +8 "Point List" (pointer array of CPointList: vtable 0x5fc748, data +0xc, count
//     +0x10, capacity +0x14; FreeFields deletes the lists)
//   CPathLineSegment (0x20, vtable 0x5fc810, class 0x6d06a4, version 1): +8 "Flags" (bit 1 may be
//     walked, bit 2 checked; set by the search), +0xc "MaxPassWidth", +0x10 "Length", +0x14
//     "AngleAToB" (floats), +0x18 "AIndex", +0x1c "BIndex" (way point indices, -1 after Create)
//
// The path search (ZPathSearch, 0x54 bytes; the game's one instance is the function-local static
// 0x6d05b0 of 0x5386b0, the map's path query (World), built by 0x5354e0, destroyed at exit through
// 0x5389b0; the same query takes the path's point list from the cache first, 0x537370) keeps three lists of
// 0x1c-byte nodes behind sentinel nodes: +0 the free nodes (300 from operator new), +0x1c the open
// list sorted by f, +0x38 the closed list. Search (0x535610) is A* from way point index `start` to
// `goal`: g sums segment Length * cost (slot 24), h is the distance to the goal (0x508c50, the exe's
// table square root of the squared integer distance), and the way points of the path go into a
// CPointList from start to goal (through the static pointer array 0x6d0500, built on first use).
//
// Return values are what the original leaves in eax; where it is undefined or only al, the comments say so.
#pragma once
#include "object_base.h"
#include "ptr_array.h"

namespace zax {

struct ZIntPoint {
    int x, y;
};

// CPathWayPoint (0x24, vtable 0x5fc86c): +8 flags, +0xc position, +0x14 the indices of its segments
struct ZPathWayPoint : CObjectBase {
    DWORD flags;           // 0x08
    ZIntPoint pos;         // 0x0c
    DWORD seg_vftable;     // 0x14
    int *segments;         // 0x18
    DWORD num_segments;    // 0x1c
    DWORD seg_capacity;    // 0x20
};
static_assert(sizeof(ZPathWayPoint) == 0x24);

struct CPathCollideChecker : CObjectBase {
    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x535ce0 (slot 1)
};
static_assert(sizeof(CPathCollideChecker) == 8);

struct CPointList : CObjectBase {
    BYTE available;   // 0x08 "Is Available"
    BYTE pad[3];
    ZPtrArray points;   // 0x0c 8-byte elements

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);          // 0x5361b0 (slot 1)
    DWORD THISCALL AddPoint(const ZIntPoint *point);    // 0x536450
};
static_assert(sizeof(CPointList) == 0x1c);

struct CPathPointListListCache : CObjectBase {
    ZPtrArray lists;   // 0x08 CPointList *

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x5370b0 (slot 1)
    DWORD THISCALL Acquire(BYTE *path);         // 0x537370
};
static_assert(sizeof(CPathPointListListCache) == 0x18);

struct CPathLineSegment : CObjectBase {
    DWORD flags;            // 0x08
    float max_pass_width;   // 0x0c
    float length;           // 0x10
    float angle_a_to_b;     // 0x14
    int a_index, b_index;   // 0x18

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x539c00 (slot 1)
};
static_assert(sizeof(CPathLineSegment) == 0x20);

struct ZPathNode {
    float g, h, f;               // cost so far, estimate to the goal, their sum
    ZPathWayPoint *waypoint;     // 0x0c
    int index;                   // 0x10 the way point's index in the map
    ZPathNode *parent;           // 0x14
    ZPathNode *next;             // 0x18

    void THISCALL FreeChain();   // 0x5354b0: deletes the nodes behind this one
};
static_assert(sizeof(ZPathNode) == 0x1c);

struct ZPathSearch {
    ZPathNode free_nodes;   // 0x00 sentinels: only their next (+0x18) is used
    ZPathNode open;         // 0x1c
    ZPathNode closed;       // 0x38

    ZPathSearch *THISCALL Ctor();                  // 0x5354e0
    void THISCALL Dtor();                          // 0x535560 (eax undefined)
    DWORD THISCALL Search(void *mover, void *map, int start, int goal, CPointList *out, void *checker);   // 0x535610 (al)
    ZPathNode *THISCALL Unlink(ZPathNode *node);   // 0x535820: from the open list, by index
    DWORD THISCALL Expand(void *mover, void *map, ZPathNode *cur, ZPathWayPoint *goal, void *checker);  // 0x535860 (al)
};
static_assert(sizeof(ZPathSearch) == 0x54);

}  // namespace zax
