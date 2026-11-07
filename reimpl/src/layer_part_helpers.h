// The non-virtual helpers of CLayerPart (0x4fa960-0x4fc8c0) and the multiplayer block functions of
// CLayerPart (0x4faa10/0x4faa40) and CSimpleLayerPart (0x5716f0/0x571780). The class layout and the
// slots are in layer_parts.h; the polygons these helpers test are in polygon.h.
//
// "Centre" below is what most helpers use as the part's place [confirmed: code]: with a polygon (slot 48)
// the polygon's mean (+0x20) plus the part's offset (slot 50), otherwise the position (slot 43); the
// original inlines this every time (LayerPart_Position 0x4fb0a0 is the out-of-line copy).
//
// Angles come from the exe's table functions 0x509100 (angle of (x, y)) and 0x508fe0 (sine and cosine
// into two floats), which stay in the exe; "scale" is the vertical scale of the isometric view (the
// callers pass 1 or the oval ratio 0.712766): y differences are divided by it, directions multiplied.
//
// Return values: the original's eax where the callers can see it; where the original leaves a status
// word or a pointer's upper bytes beside a byte result, the comment says al.
#pragma once
#include "layer_parts.h"

namespace zax {

struct LayerPartHelpers {
    static DWORD THISCALL Team(LayerPart *self);                                                  // 0x4fa960
    static DWORD THISCALL ReadBlock(LayerPart *self, void *packet);                               // 0x4faa10 (eax not reproduced)
    static DWORD THISCALL WriteBlock(LayerPart *self, void *packet, DWORD unused);                // 0x4faa40 (1)
    static long double THISCALL SlotDistance2(LayerPart *self);                                   // 0x4faf50
    static DWORD THISCALL ChildrenRect(LayerPart *self, int *rect, DWORD kind, DWORD offset);     // 0x4faff0 (al 1)
    static void THISCALL Center(LayerPart *self, float *out);                                     // 0x4fb0a0
    static long double THISCALL AngleFrom(LayerPart *self, const float *p, const float *dir, float scale);       // 0x4fb0f0
    static long double THISCALL AngleFromPart(LayerPart *self, LayerPart *other, const float *dir, float scale); // 0x4fb2d0
    static long double THISCALL TurnTo(LayerPart *self, const float *p, const float *dir, float scale);          // 0x4fb340
    static long double THISCALL TurnToPart(LayerPart *self, LayerPart *other, const float *dir, float scale);    // 0x4fb3c0
    static void THISCALL MoveCenterTo(LayerPart *self, const float *p);                           // 0x4fb5e0
    static void THISCALL MoveCenterToXY(LayerPart *self, float x, float y);                       // 0x4fb680
    static void THISCALL AddParentPositions(LayerPart *self, float *p);                           // 0x4fb710
    static void THISCALL AddParentPositions3(LayerPart *self, float *p);                          // 0x4fb750
    static void THISCALL SubParentPositions3(LayerPart *self, float *p);                          // 0x4fb7b0
    static void THISCALL MessageTree(LayerPart *self, DWORD message);                             // 0x4fb810
    static DWORD THISCALL FindInRect(LayerPart *self, void *list, float rx, float ry, DWORD a, DWORD b, DWORD c);   // 0x4fb910
    static DWORD THISCALL FindInOval(LayerPart *self, void *list, DWORD r, DWORD a, DWORD b, DWORD c);  // 0x4fb9e0
    static void *THISCALL AIAt(LayerPart *self, DWORD i);                                         // 0x4fba60
    static void *THISCALL TakeAI(LayerPart *self, DWORD i);                                       // 0x4fba80
    static void *THISCALL ReplaceAI(LayerPart *self, DWORD i, void *ai);                          // 0x4fbaa0
    static void THISCALL AdoptDynamicAIs(LayerPart *self);                                        // 0x4fbba0
    static void THISCALL AddAI(LayerPart *self, void *ai);                                        // 0x4fbc50
    static void THISCALL RemoveAI(LayerPart *self, void *ai);                                     // 0x4fbc90
    static void THISCALL DeleteAIs(LayerPart *self);                                              // 0x4fbd00
    static void THISCALL MarkAIsDone(LayerPart *self);                                            // 0x4fbd60
    static void *THISCALL MarkDone(LayerPart *self, void *ai);                                    // 0x4fbd80
    static void THISCALL RemoveDeleteAI(LayerPart *self, void *ai);                               // 0x4fbd90
    static ZString *THISCALL SetNameText(LayerPart *self, const char *name);                      // 0x4fbf00
    static ZString *THISCALL SetName(LayerPart *self, const ZString *name);                       // 0x4fbf10
    static const char *THISCALL Name(LayerPart *self);                                            // 0x4fbf20
    static DWORD THISCALL Died(LayerPart *self);                                                  // 0x4fbf30
    static void *THISCALL FindNearest(LayerPart *self, int r, DWORD all, DWORD none, DWORD any, float max_angle,
                                      const void *exclude);                                       // 0x4fc390
    static DWORD THISCALL OverlapsPolygon(LayerPart *self, const void *poly, const float *offset);   // 0x4fc620
    static DWORD THISCALL OverlapsPart(LayerPart *self, LayerPart *other);                        // 0x4fc680
    static LayerPart *THISCALL Root(LayerPart *self);                                             // 0x4fc6f0
    static void THISCALL AddChild(LayerPart *self, LayerPart *child);                             // 0x4fc710
    static DWORD THISCALL ChildCount(LayerPart *self);                                            // 0x4fc7c0
    static LayerPart *THISCALL ChildAt(LayerPart *self, DWORD i);                                 // 0x4fc7d0
    static void THISCALL SetChildList(LayerPart *self, LPChildList *list);                        // 0x4fc7e0
    static DWORD THISCALL SetId(LayerPart *self, DWORD id);                                       // 0x4fc860 (ax)
    static DWORD THISCALL SaveFlags(LayerPart *self);                                             // 0x4fc870
    static DWORD THISCALL FlagsUnchanged(LayerPart *self, DWORD mask);                            // 0x4fc880
    static void THISCALL LayerAdd(LayerPart *self, void *what);                                   // 0x4fc8a0
    static void THISCALL LayerAddNamed(LayerPart *self, const char *name);                        // 0x4fc8c0

    // CSimpleLayerPart's multiplayer block
    static DWORD THISCALL SimpleWriteBlock(SimplePart *self, void *packet, DWORD skip);           // 0x5716f0
    static DWORD THISCALL SimpleReadBlock(SimplePart *self, void *packet);                        // 0x571780
};

// fastcall (ecx = this, edx = the other part or point, scale and limits on the stack)
long double __fastcall LayerPart_Distance2ToPart(LayerPart *self, LayerPart *other, float scale);   // 0x4fbf60
long double __fastcall LayerPart_Distance2(LayerPart *self, const float *p, float scale);           // 0x4fc020
BYTE __fastcall LayerPart_Within(LayerPart *self, LayerPart *other, float r, float scale);           // 0x4fc090

}  // namespace zax
