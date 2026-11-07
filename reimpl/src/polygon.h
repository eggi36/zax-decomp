// The polygons of Zax.exe ("POLY2D", 0x545620-0x547d60) and the collision core built on them: the
// straight movement ("sweep", vtable 0x5f8ba8), point, circle, edge and segment tests, the sweep of a
// point or a polygon against a polygon, and the sweep through the layer's grid (0x4e8d40, 0x4e8e00), plus
// the segment intersection 0x5093d0.
//
// Poly2D (0x28 bytes, the value of the polygon field kind, ZPolygon in field_slots.h) [confirmed: code]:
//   +0x00 the points (operator new, count * {x, y}; a closed polygon repeats its first point at the end)
//   +0x04 one edge per pair of points (operator new, count - 1): the unit normal (dy, -dx) / length
//         ((0, 1) for a zero-length edge) and the length
//   +0x08 count
//   +0x0c left, top, right, bottom: the bounding rectangle in whole pixels (floor of the minimum, ceiling
//         of the maximum, then rounded; right/bottom one more when equal to left/top)
//   +0x1c flags: bit 0 the outline turns both ways (concave), bit 1 the turns add up to about +2 pi
//         (|sum - 2 pi| < 2)
//   +0x20/+0x24 the mean of the points without the last one (x snapped to 0 when |x| < 0.05)
// 0x5460b0 recomputes the rectangle and the mean, 0x546240 the edges and the flags; every function that
// changes points calls both (moving 0x546070 only the first).
//
// Sweep (0x38 bytes, vtable 0x5f8ba8 whose slots 0x4e96b0-0x4e9c80 belong to CLayer) [confirmed: code]:
//   +0x04 a kind (1 and 2 select part categories in 0x4e8e00), +0x08 the movement, +0x10 its direction
//   (unit), +0x18 its length, +0x1c a byte "notify" (the parts hit are told, 0x4e8e00), +0x1d a byte,
//   +0x20-+0x34 six dwords set by 0x545b90. Slot 0 (mover, part) is the sweep's own filter.
// SweepHit (9 dwords): +0 the share of the movement until the contact (starts at 1, 0x545c90), +4 the
// movement until then, +0xc the contact normal, +0x14 the contact position, +0x1c two dwords copied along.
//
// Floating point follows the original instruction by instruction (x87.h): where it keeps a value on the
// FPU stack it stays long double, where it stores a float it is rounded. Elsewhere: the drawing
// 0x546d40 (draws the outline and point numbers through the canvas and the error reporter's font; view_draw.cpp) and
// the static initializer 0x5470f0 of the ten polygon pairs at 0x7111e8 (static_c2_manual.cpp).
#pragma once
#include "exe_base.h"

namespace zax {

struct PolyEdge {
    float nx, ny;   // unit normal
    float length;
};

struct Poly2D {
    float *points;     // 0x00
    PolyEdge *edges;   // 0x04
    DWORD count;       // 0x08
    int left, top, right, bottom;   // 0x0c
    DWORD flags;       // 0x1c
    float cx, cy;      // 0x20
};
static_assert(sizeof(Poly2D) == 0x28);

struct Sweep {
    DWORD vftable;   // 0x00 0x5f8ba8
    DWORD kind;      // 0x04
    float dx, dy;    // 0x08
    float ux, uy;    // 0x10
    float length;    // 0x18
    BYTE notify;     // 0x1c
    BYTE b1d;        // 0x1d
    BYTE pad[2];
    float hx, hy;    // 0x20
    DWORD h28, h2c, h30, h34;
};
static_assert(sizeof(Sweep) == 0x38);

struct SweepHit {
    float t;
    float vx, vy;
    float nx, ny;
    float px, py;
    DWORD f1c, f20;
};
static_assert(sizeof(SweepHit) == 0x24);

constexpr DWORD VT_SWEEP = 0x5f8ba8;
constexpr DWORD POINT_BUFFER = 0x711120;         // 25 points the polygon setters copy through
constexpr DWORD STATIC_POLY_PAIRS = 0x7111e8;    // 10 * 0x88 bytes, polygons at +0 and +0x38
constexpr DWORD SINGLE_POINT_POLY = 0x6c1b38;    // the one-point polygon of 0x4e8d40 (guard 0x6c1bf0)
constexpr DWORD SWEEP_DEPTH = 0x6c1d00;          // nesting counter of 0x4e8e00

// fastcall (ecx = start, edx = unit direction): does the segment start + dir * [0, length] come closer
// than r to c? normal/dist (optional) get the direction from the nearest point to c and its distance.
BYTE __fastcall Seg_HitCircle(const float *p, const float *dir, float length, const float *c, float r, float *normal,
                              float *dist);                                                       // 0x545620
// fastcall (ecx = out x, edx = out y): intersection of the segments (x1,y1)-(x2,y2) and (x3,y3)-(x4,y4);
// 0 none, 1 the point, 2 parallel; check = 0 skips the range tests
DWORD __fastcall Seg_Intersect(float *ox, float *oy, float x1, float y1, float x2, float y2, float x3, float y3,
                               float x4, float y4, DWORD check);                                 // 0x5093d0
// thiscall on the output: the same with the share of the first segment in *t
DWORD THISCALL Seg_IntersectT(float *t, float x1, float y1, float x2, float y2, float x3, float y3, float x4,
                              float y4);                                                         // 0x5476b0
// fastcall (ecx = circle centre, edx = start): the first contact of start + u * s with the circle (the
// smaller root, 0 without a root); eax (the number of roots) is not reproduced, the caller ignores it
long double __fastcall Circle_FirstRoot(const float *center, const float *start, float r, const float *u);   // 0x547920

struct SweepOps {
    static Sweep *THISCALL Ctor(Sweep *s, DWORD kind, DWORD notify);                              // 0x545a00
    static Sweep *THISCALL CtorFromTo(Sweep *s, const float *from, const float *to, DWORD kind, DWORD notify);   // 0x545a20
    static DWORD THISCALL SetFromTo(Sweep *s, const float *from, const float *to);                // 0x545ab0 (al)
    static DWORD THISCALL SetMove(Sweep *s, const float *move);                                   // 0x545b30 (al)
    static DWORD THISCALL SetHitData(Sweep *s, const float *p, DWORD a, DWORD b, DWORD c, DWORD d);   // 0x545b90
    static int THISCALL ExpandRect(const Sweep *s, int *rect);                                    // 0x545bc0
    static float *THISCALL HitInit(float *hit);                                                   // 0x545c90 (t = 1)
};

struct Polygon {
    static DWORD THISCALL Clear0(Poly2D *s);                                       // 0x545ca0 (eax 0)
    static Poly2D *THISCALL Ctor(Poly2D *s);                                       // 0x545cc0
    static DWORD THISCALL Dtor(Poly2D *s);                                         // 0x545cf0 (eax 0)
    static DWORD THISCALL SetFrom(Poly2D *s, const Poly2D *src);                   // 0x545d50 (flags)
    static void THISCALL Clear(Poly2D *s);                                         // 0x545e30
    static DWORD THISCALL Read(Poly2D *s, void *file);                             // 0x545fa0 (al)
    static void THISCALL Move(Poly2D *s, float dx, float dy);                      // 0x546070
    static void THISCALL UpdateBounds(Poly2D *s);                                  // 0x5460b0
    static DWORD THISCALL UpdateEdges(Poly2D *s);                                  // 0x546240 (flags)
    static DWORD THISCALL IsClosed(const Poly2D *s);                               // 0x546550 (al)
    static DWORD THISCALL SetFromBuffer(Poly2D *s, DWORD n);                       // 0x546580 (flags)
    static DWORD THISCALL SetPoint(Poly2D *s, DWORD i, float x, float y);          // 0x546630 (flags)
    static DWORD THISCALL Insert(Poly2D *s, float x, float y, DWORD after);        // 0x5466c0 (flags)
    static DWORD THISCALL Close(Poly2D *s);                                        // 0x546810
    static DWORD THISCALL Contains(const Poly2D *s, float x, float y);             // 0x546830 (al)
    static DWORD THISCALL HitsCircle(const Poly2D *s, float x, float y, float r);  // 0x5469c0 (al)
    static DWORD THISCALL Center(const Poly2D *s, float *out);                     // 0x546a70 (al 1)
    static long double THISCALL Size(const Poly2D *s);                             // 0x546a90
    static int *THISCALL Rect(const Poly2D *s, const float *offset, int *out);     // 0x546ad0
    static int THISCALL RectSwept(const Poly2D *s, const float *offset, const Sweep *sw, int *out);   // 0x546b40
    static int THISCALL NearestVertex(const Poly2D *s, const float *p, float *dist2, DWORD skip);     // 0x546bc0
    static int THISCALL NearestEdge(const Poly2D *s, const float *p, float r, float *dist);          // 0x546c70
    static DWORD THISCALL RectsMeet(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2,
                                    const float *move);                            // 0x546f50 (al)
    static DWORD THISCALL SweepPoint(const Poly2D *s, const float *offset, const float *p, const Sweep *sw,
                                     SweepHit *hit);                               // 0x547230 (al)
    static DWORD THISCALL SweepVertices(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2,
                                        const Sweep *sw, SweepHit *hit);           // 0x547a30 (al)
    static DWORD THISCALL SweepPoly(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2,
                                    const Sweep *sw, SweepHit *hit);               // 0x547bb0 (al)
    static DWORD THISCALL Overlaps(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2);   // 0x547d60 (al)
};

// stdcall angle helpers (results in st(0))
long double STDCALL Angle_Normalize(float a);            // 0x5464e0: into [0, 2 pi)
long double STDCALL Angle_Diff(float from, float to);    // 0x546510: to - from into [-pi, pi]
DWORD __cdecl Poly_DestroyStaticPairs();                 // 0x547130 (atexit of the pairs at 0x7111e8; eax 0)

// The sweep through the layer's grid: a method of the layer's +0x3c object (its +0x14 is the grid
// CSortList2D the layer keeps at +0x50)
struct LayerSweep {
    // 0x4e8e00: every part of the grid in the swept rectangle (kind 3, all 0x840000 (+0x8000000 for sweep
    // kind 2), none `none` | 0x2000000, any `any` (+0xc000000 for kind 1)) that the mover, the filter, both
    // parts' slot 65 and the sweep's slot 0 accept is swept against (0x547bb0, the part's polygon slot 48 at
    // its offset slot 50); the nearest contact goes to result/out_part unless the part's slot 66 lets the
    // mover pass (then slot 67 tells it, when the sweep notifies). At the end the part hit is told through
    // its slot 67 with the normal turned around. al: a contact was found.
    static DWORD THISCALL Run(BYTE *self, const Poly2D *poly, const float *offset, const ::zax::Sweep *sw,
                                SweepHit *result, void **out_part, void *mover, DWORD any, DWORD none,
                                BYTE(__fastcall *filter)(void *mover, void *part));
    // 0x4e8d40: the same for a single point (the polygon at 0x6c1b38 with the point (0, 0), built on first
    // use) at *point; al = 0x4e8e00 != 0
    static DWORD THISCALL RunPoint(BYTE *self, const float *point, const ::zax::Sweep *sw, SweepHit *result,
                                     void **out_part, DWORD any, DWORD none, void *mover,
                                     BYTE(__fastcall *filter)(void *, void *));
};

}  // namespace zax
