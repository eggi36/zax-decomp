// The way point classes of Zax.exe (World): CWayPointPath (0x5365d0-0x536f70), CWayPointMap with its
// quadtree, building, path query and .way reading/writing (0x537440-0x539910), the methods of
// CPathLineSegment (0x53a010-0x53a0f0), CPathWayPoint (0x53a200-0x53a8c0) and CWayPointsPolygon
// (0x598d90-0x5992e0).
//
// The CLayer keeps the level's map at +0x134 (0x4eca80 reads it from the level's .way file or builds it
// from the way point polygons with Calculate and saves it, 0x4ec520). The map's way points
// (CPathWayPoint) are kept in an array and, for the lookups by position, in a quadtree of 0x20-byte
// nodes; the line segments (CPathLineSegment, path_classes.h) join way points within MaxDistToConnect.
// The AI asks the map for a path (FindPath, 0x5386b0, from 0x4305d3): straight when the line of sight
// is free, otherwise the A* search (path_classes.h) between the way points nearest to start and end,
// temporary way points added for them and removed again.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CWayPointPath (0x34, vtable 0x5fc690, class 0x6d065c, version 3, alternative class name
//     "CFindPathParams"): +8 "Start Point", +0x10 "End Point" (integer points), +0x18 "Length", +0x1c
//     "Max Dist To Travel" (floats), +0x20 "Num Failed Paths", +0x24 "Max Failed Paths", +0x28 "Pos To
//     Move Index" (ints), +0x2c "Dist So Far" (float), +0x30 "Path" (CPointList *, from the cache)
//   CWayPointMap (0x34, vtable 0x5fc760, class 0x6d0664, version 8, alternative class name "CPathData"):
//     +8 the way points (pointer array, vtable 0x5fc7c8, owned), +0x18 the segments (vtable 0x5fc7bc,
//     owned), +0x28 "MinDistBetweenWayPoints" (default 30), +0x2c "MaxDistToConnect" (default 59), +0x30
//     the quadtree's root (not a field; slots 13/15 write and read the arrays)
//   CPathWayPoint (0x24, vtable 0x5fc86c, class 0x6d06bc): +8 "Flags", +0xc "Position X", +0x10
//     "Position Y", +0x14 the indices of its segments (array of ints, vtable 0x5fc8c8, pool data)
//   CWayPointsPolygon (0x78, vtable 0x602b2c, class 0x713df4, version 2, parent CFreeRangePoly): +0x74
//     "Insertion Priority" (0 or 1: the two passes of Calculate); RegisterFields hides 15 inherited
//     fields (their name emptied)
//
// Return values are what the original leaves in eax; the comments say where it is undefined or only al.
#pragma once
#include "path_classes.h"

namespace zax {
struct CFileBuf;
}

namespace zax {

// A node of the map's quadtree (operator new, 0x20 bytes; FindPath uses two static ones). The children
// split by the node's position: +0x10 x >= and y >=, +0x14 x < and y >=, +0x18 x < and y <, +0x1c x >=
// and y <.
struct ZQuadNode {
    int x, y;              // 0x00 the way point's position
    int index;             // 0x08 its index in the map
    ZQuadNode *parent;     // 0x0c
    ZQuadNode *child[4];   // 0x10

    void *THISCALL DeletingDtor(DWORD flags);                    // 0x538380 (the whole subtree)
    void THISCALL DeleteChildren();                              // 0x5396e0 (eax undefined)
    void THISCALL Query(ZPtrArray *out, const int *rect);        // 0x539760 (eax undefined)
    ZQuadNode **THISCALL Unlink();                               // 0x539910
};
static_assert(sizeof(ZQuadNode) == 0x20);

// What slot 48 of a polygon returns: its points (float pairs, closed: the last is the first), the
// outward normals of its edges (12 bytes each, x and y used) and the point count
struct ZPolyShape {
    float (*points)[2];
    float (*normals)[3];
    int count;
};

struct CWayPointPath : CObjectBase {
    ZIntPoint start;        // 0x08
    ZIntPoint end;          // 0x10
    float length;           // 0x18
    float max_dist;         // 0x1c
    int num_failed;         // 0x20
    int max_failed;         // 0x24
    DWORD pos_index;        // 0x28
    float dist_so_far;      // 0x2c
    CPointList *path;       // 0x30

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);                                   // 0x5366b0 (slot 1)
    DWORD THISCALL Init(const ZIntPoint *from, const ZIntPoint *to);             // 0x536d20
    DWORD THISCALL InitFloat(const float *from, const float *to);                // 0x536da0
    CWayPointPath *THISCALL Ctor();                                              // 0x536e10
    DWORD THISCALL Dtor();                                                       // 0x536e30
    DWORD THISCALL HasPath();                                                    // 0x536e60
    DWORD THISCALL NextPoint(float *out, void *mover);                           // 0x536e70 (al)
    DWORD THISCALL Advance();                                                    // 0x536f40 (al)
    DWORD THISCALL Draw(void *surface);                                          // 0x536f70 (eax undefined)
};
static_assert(sizeof(CWayPointPath) == 0x34);

struct CWayPointMap : CObjectBase {
    ZPtrArray waypoints;    // 0x08 ZPathWayPoint *
    ZPtrArray segments;     // 0x18 CPathLineSegment *
    int min_dist;           // 0x28
    int max_connect;        // 0x2c
    ZQuadNode *quad;        // 0x30

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);                                    // 0x5375a0 (slot 1)
    DWORD THISCALL WriteExtra(CFileBuf *w);                                      // 0x537a60 (slot 13)
    DWORD THISCALL ReadExtra(CFileBuf *r);                                       // 0x537ae0 (slot 15, al)
    DWORD THISCALL IsPointBlocked(void *layer, float x, float y, float radius);  // 0x537d40 (al)
    void THISCALL OffsetVertex(const ZPolyShape *shape, int i, float *out, float dist);   // 0x537e90
    void THISCALL AddPointsAlongSegment(void *layer, const float *from, const float *to, float radius, float *last,
                                        BYTE *added);                            // 0x5380d0
    void THISCALL AddPolygonOutline(void *poly, float dist, float radius);       // 0x538230
    void THISCALL Calculate(void *layer, float offset, float min_dist_, float max_connect_, float max_offset,
                            float step);                                         // 0x5383f0
    void THISCALL RemoveTemporary(DWORD start_added, const ZIntPoint *start, DWORD start_segments, DWORD end_added,
                                  const ZIntPoint *end, DWORD end_segments);     // 0x5385b0
    DWORD THISCALL FindPath(CWayPointPath *path, void *mover, void *cache, void *checker);   // 0x5386b0 (al)
    DWORD THISCALL ConnectNode(int index);                                       // 0x538ae0
    int THISCALL AddNode(const ZIntPoint *p, ZQuadNode *node);                   // 0x538df0
    int THISCALL AddSegment(CPathLineSegment *s);                                // 0x538f40
    CPathLineSegment *THISCALL Segment(int i);                                   // 0x538f60
    ZPathWayPoint *THISCALL WayPoint(int i);                                     // 0x538f70
    void THISCALL RemoveNodeAt(const ZIntPoint *p, BYTE delete_tree);            // 0x538f80 (eax undefined)
    int THISCALL FindIndex(const ZIntPoint *p);                                  // 0x5392f0
    DWORD THISCALL FindNearest(const ZIntPoint *p, int dist, int *out, void *mover, BYTE reverse);   // 0x539350 (al)
};
static_assert(sizeof(CWayPointMap) == 0x34);

// The methods of CPathLineSegment and CPathWayPoint (their classes are in path_classes.h)
struct ZSegmentMethods : CPathLineSegment {
    CPathLineSegment *THISCALL Ctor();                                           // 0x53a010
    DWORD THISCALL Touches(int i);                                               // 0x53a040 (al)
    DWORD THISCALL Joins(int i, int j);                                          // 0x53a060 (al)
    DWORD THISCALL IsWalkable();                                                 // 0x53a090
    DWORD THISCALL SetChecked(BYTE walkable);                                    // 0x53a0a0
    DWORD THISCALL IsChecked();                                                  // 0x53a0c0
    int THISCALL OtherEnd(int i);                                                // 0x53a0d0
    void THISCALL Init(CWayPointMap *map, const ZIntPoint *a, const ZIntPoint *b);   // 0x53a0f0
};

struct ZWayPointMethods : ZPathWayPoint {
    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);                                    // 0x53a320 (slot 1)
    ZPathWayPoint *THISCALL Ctor();                                              // 0x53a610
    DWORD THISCALL WriteExtra(CFileBuf *w);                                      // 0x53a640 (slot 13)
    DWORD THISCALL ReadExtra(CFileBuf *r);                                       // 0x53a6a0 (slot 15, al)
    DWORD THISCALL AddEdge(CWayPointMap *map, int self, int other);              // 0x53a780 (al)
    void THISCALL RemoveSegmentsTouching(CWayPointMap *map, int index);          // 0x53a8c0 (eax undefined)
};
static_assert(sizeof(ZWayPointMethods) == 0x24);

struct CWayPointsPolygon : CObjectBase {
    BYTE base[0x6c];        // 0x08 CFreeRangePoly
    int insertion_priority; // 0x74

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);                                    // 0x598e70 (slot 1)
    CWayPointsPolygon *THISCALL Ctor();                                          // 0x599290
    DWORD THISCALL ClearFlags();                                                 // 0x5992b0
    DWORD THISCALL ResetFields();                                                // 0x5992c0 (slot 19)
    DWORD THISCALL Died(DWORD unused);                                           // 0x5992e0 (slot 35)
};
static_assert(sizeof(CWayPointsPolygon) == 0x78);

}  // namespace zax
