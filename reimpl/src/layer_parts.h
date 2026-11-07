// The layer part classes of Zax.exe: the base class CLayerPart (0x4fa770-0x4fc940 and its slots in
// other ranges), CSimpleLayerPart (a part with a position), CFreeRangePoly (a part with a polygon,
// 0x4b5cb0-0x4b61e0) and CUnderConstructionLayerPart (a sign with a text, 0x58f340-0x58fc90).
//
// CLayerPart (0x4c bytes, abstract, class 0x6c29cc, version 26, no parent; vtable 0x5f93d8 with 81 slots,
// written by the constructor 0x4fa770 and the destructor 0x4fa810) [confirmed: constructor, destructor,
// RegisterFields 0x4f9c50, docs/fields.csv]:
//   +0x08 "Child List" (owned CChildList: +0xc data, +0x10 count of the parts)
//   +0x0c the parent part (the part whose child list holds this one; set lazily by several slots)
//   +0x10 "Debug/Broadcaster" (owned CBroadcaster, created by the first listener: +0x10 listener count)
//   +0x14 "Debug/Current Target" (reference to a part; this part listens to the target's broadcaster)
//   +0x18 "Name" (ZString)
//   +0x1c flags: 0x10000 marked for deletion, 0x20000 visible, 0x40000 collideable, 0x80000 tries to
//         collide, 0x100000 has hit points, 0x200000 stationary, 0x400000 (redraw on change, used by
//         CFreeRangePoly), 0x800000 active, 0x2000000 temporarily excluded, 0x4000000 half height,
//         0x8000000 full height (half and full height exclude each other); "Categories/Category" are
//         flags of the same dword (offset 0x1c, the category list 0x6bd140)
//   +0x20 "Game Type/Used In" (flags over 0x6c02e8, default "$All")
//   +0x24 "Game Type/Team Number" (-1 "Nutral", 1 blue, 2 red)
//   +0x28 id (word), +0x2a a saved copy of the upper flag word (0x4fc870)
//   +0x2c the grid stamp, +0x30 the grid cell rectangle (four words, -9999 when not listed)
//   +0x38 "Activity/Activity": embedded pool pointer array (vtable 0x5f951c; destructor writes
//         0x5f9528) of AI objects (CAIBase; +8 a byte "done": slot 35 removes and deletes those)
//   +0x48 the layer (CLayer: +0x3c, +0x50 the grid CSortList2D, +0x7c the object of 0x540640)
// The constructor sets the flags to 0xa60000 (visible, collideable, stationary, active), the cells to
// -9999 and everything else to 0.
//
// Slots of the base class (names are ours where the code does not show more) [confirmed: code]:
//   0 OnMessage(sender, message): "Died", "Delete" or "Leaving Scope" from the current target clears
//     the target (slot 71 with 0), everything else goes to CObject_OnMessage
//   23 (5 arguments) passed to every visible child; a child without a parent gets this one
//   26 sum of slot 25 (arg, 1) of the visible active children
//   30/31/32 pass an argument to the AIs (slots 28/26/27 with this) and to the children (30/31)
//   33 activate: AIs slot 24 when this part and all parents are active, children slot 33, then slot 63
//      with the message "Begin" (id 0x6c2998); 34 deactivate (not when marked for deletion): AIs slot 29
//      with message 0x6c29e8 unless the chain is active, children slot 34, slot 63 with "End" (0x6c29b0)
//   35 update (when active): AIs slot 25, AIs marked done removed and deleted (from the last); children
//      slot 35, children marked for deletion removed and deleted (from the last)
//   36 parent's slot 36 (0 without a parent); 42 position plus the positions of all parents (slot 43)
//   57 children slot 57 with arg + 1; 58/59 set/clear flags (activation through slots 33/34, the layer
//      told through 0x4ee730); 60 replace the flags (the layer's +0x7c object told about a visibility or
//      stationary change, 0x4fa8c0)
//   63 message to the AIs (slot 29) and the visible children (slot 63): true if one took it
//   64 first AI not done of a class, or (with the flag) the answer of an AI to "Query AI Type"
//   65/66 AND/OR of AI slots 31/33; 67 AI slot 34 (four arguments)
//   70 the current target; 71 set the current target (moves the listening, then slot 63 with 0x6c29e0)
//   72/73 the dynamic point property 0x6c29b8 (0x4a8080/0x4a80b0); 74/75 the world's slots 46/47
//   76 the broadcaster's empty slot and 0x4fd180 for every AI; 78 (CSimpleLayerPart) re-list in the
//      grid and tell the layer's +0x7c object
// Slots 37-55 are pure virtual in the base (0x5d0331): 37 rectangle (rect *, kind, offset), 38 hit
// test (x, y, kind), 41 set position, 43 position, 44/45 x/y, 48 polygon, 49 set polygon, 50 offset,
// 52/53 x/y rounded, 61 display name.
//
// CSimpleLayerPart (0x54, abstract, class 0x713560, version 1, parent CLayerPart, vtable 0x600344):
//   +0x4c "Position/X", +0x50 "Position/Y" (floats; setters 0x571800/0x571810 call slots 81/82)
// CFreeRangePoly (0x74, class 0x6bfb08, version 3, parent CLayerPart, vtable 0x5f4da4):
//   +0x4c "Polygon" (ZPolygon of field_slots.h: points, edges, count, ...)
// CUnderConstructionLayerPart (0x9c, class 0x713a3c, version 1, parent CSimpleLayerPart, vtable
//   0x601f94): +0x54 "Text" (default "Under Construction"), +0x58 the wrapped text, +0x5c the font
//   (record object of "Med Gold" in the font list 0x6be1e0, else the error reporter's default font),
//   +0x60 "Border Texture" (index in the texture list 0x6c1d50), +0x64 "Max Width" (pixels), +0x68/+0x6c
//   the text's width and height, +0x70 an embedded drawable (vtable 0x6020e0; +0x74, +0x78, +0x80 the
//   part, +0x84), +0x88 its sort key, +0x94/+0x95 two bytes, +0x98 the part itself
//
// Return values are what the original leaves in eax; where that is not reproduced (void in use) the
// comment says so.
#pragma once
#include "class_info.h"
#include "field_slots.h"

namespace zax {

struct LPChildList {
    DWORD vftable;
    void *dynamic;
    DWORD array_vftable;   // 0x08
    struct LayerPart **data;   // 0x0c
    DWORD count;           // 0x10
    DWORD capacity;        // 0x14
};

struct LPArray {
    DWORD vftable;
    void **data;
    DWORD count;
    DWORD capacity;
};

struct LayerPart {
    DWORD vftable;
    void *dynamic;
    LPChildList *children;   // 0x08
    LayerPart *parent;       // 0x0c
    BYTE *broadcaster;       // 0x10
    LayerPart *target;       // 0x14
    ZString name;            // 0x18
    DWORD flags;             // 0x1c
    DWORD used_in;           // 0x20
    DWORD team;              // 0x24
    WORD id;                 // 0x28
    WORD saved_flags;        // 0x2a
    DWORD stamp;             // 0x2c
    short cells[4];          // 0x30
    LPArray activity;        // 0x38
    BYTE *layer;             // 0x48

    // Base class
    static LayerPart *THISCALL Ctor(LayerPart *self);                        // 0x4fa770
    static LayerPart *THISCALL DeletingDtor(LayerPart *self, DWORD flags);   // 0x4fa7e0 (slot 1)
    static DWORD THISCALL Dtor(LayerPart *self);                             // 0x4fa810
    static DWORD THISCALL TellLayerRect(LayerPart *self);                    // 0x4fa8c0 (eax not reproduced)
    static DWORD THISCALL SetHalfHeight(LayerPart *self, DWORD on);          // 0x4fa900
    static DWORD THISCALL SetCollideable(LayerPart *self, DWORD on);         // 0x4fa930
    static DWORD THISCALL SetFullHeight(LayerPart *self, DWORD on);          // 0x4fa970
    static DWORD THISCALL Slot23(LayerPart *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e);   // 0x4fa9a0
    static DWORD THISCALL Slot26(LayerPart *self, DWORD arg);                // 0x4faa90
    static void THISCALL Slot30(LayerPart *self, DWORD arg);                 // 0x4fab00
    static void THISCALL Slot31(LayerPart *self, DWORD arg);                 // 0x4fab60
    static void THISCALL Slot32(LayerPart *self, DWORD arg);                 // 0x4fabd0
    static void THISCALL Activate(LayerPart *self);                          // 0x4fac20 (slot 33)
    static void THISCALL Deactivate(LayerPart *self);                        // 0x4face0 (slot 34)
    static void THISCALL Update(LayerPart *self, DWORD arg);                 // 0x4fadc0 (slot 35)
    static DWORD THISCALL ParentSlot36(LayerPart *self);                     // 0x4faf30 (slot 36)
    static void THISCALL WorldPosition(LayerPart *self, float *out);         // 0x4fafa0 (slot 42)
    static void THISCALL Slot57(LayerPart *self, DWORD n);                   // 0x4fb490
    static void THISCALL SetFlags(LayerPart *self, DWORD bits);              // 0x4fb540 (slot 58)
    static void THISCALL ClearFlags(LayerPart *self, DWORD bits);            // 0x4fb590 (slot 59)
    static void THISCALL ReplaceFlags(LayerPart *self, DWORD flags);         // 0x4fb4d0 (slot 60)
    static DWORD THISCALL Message(LayerPart *self, void *message);           // 0x4fb880 (slot 63; al)
    static void *THISCALL FindAI(LayerPart *self, CClassInfo *cls, DWORD query);   // 0x4fbae0 (slot 64)
    static DWORD THISCALL AllAIs(LayerPart *self, DWORD a, DWORD b);         // 0x4fbe10 (slot 65)
    static DWORD THISCALL AnyAI(LayerPart *self, DWORD a, DWORD b);          // 0x4fbe60 (slot 66)
    static DWORD THISCALL Slot67(LayerPart *self, DWORD a, DWORD b, DWORD c, DWORD d);   // 0x4fbeb0
    static LayerPart *THISCALL Target(LayerPart *self);                      // 0x4fc260 (slot 70)
    static DWORD THISCALL OnMessage(LayerPart *self, LayerPart *sender, const char *message);   // 0x4fc270 (slot 0)
    static DWORD THISCALL SetTarget(LayerPart *self, LayerPart *target);     // 0x4fc2e0 (slot 71)
    static DWORD THISCALL Slot72(LayerPart *self);                           // 0x4fc930
    static DWORD THISCALL Slot73(LayerPart *self, DWORD point);              // 0x4fc940
    static DWORD THISCALL Slot74(LayerPart *self);                           // 0x4fc5e0
    static DWORD THISCALL Slot75(LayerPart *self);                           // 0x4fc600
    static DWORD THISCALL Slot76(LayerPart *self);                           // 0x4fc170
    static DWORD THISCALL Relist(LayerPart *self);                           // 0x5717e0 (slot 78; eax not reproduced)
    static void THISCALL ClearFlagsRelist(LayerPart *self, DWORD bits);      // 0x5001d0 (slot 59 of the sign)
    static float *THISCALL OffsetOrPosition(LayerPart *self, float *out);    // 0x487210 (slots 40/50)

    // CSimpleLayerPart
    static LayerPart *THISCALL SimpleCtor(LayerPart *self);                       // 0x571680
    static LayerPart *THISCALL SimpleDeletingDtor(LayerPart *self, DWORD flags);  // 0x5716a0 (slot 1)
    static DWORD THISCALL SimpleDtor(LayerPart *self);                            // 0x5716d0
    static DWORD THISCALL SimpleSetPosition(LayerPart *self, DWORD x, DWORD y);   // 0x4fe730 (slot 41)
    static float *THISCALL SimplePosition(LayerPart *self, float *out);           // 0x4fe760 (slot 43)
    static float THISCALL SimpleX(LayerPart *self);                               // 0x487220 (slot 44)
    static float THISCALL SimpleY(LayerPart *self);                               // 0x487230 (slot 45)
    static int THISCALL SimpleRoundX(LayerPart *self);                            // 0x4fe780 (slot 52)
    static int THISCALL SimpleRoundY(LayerPart *self);                            // 0x4fe7a0 (slot 53)
    static float THISCALL SimpleSetX(LayerPart *self, DWORD x);                   // 0x48a0a0 (slot 81)
    static float THISCALL SimpleSetY(LayerPart *self, DWORD y);                   // 0x48a0d0 (slot 82)
    static long double THISCALL FieldSetX(LayerPart *self, DWORD x);                    // 0x571800
    static long double THISCALL FieldSetY(LayerPart *self, DWORD y);                    // 0x571810
    static void THISCALL SimpleSetFlags(LayerPart *self, DWORD bits);             // 0x58fc90 (slot 58 of the sign)
};
static_assert(sizeof(LayerPart) == 0x4c);
static_assert(offsetof(LayerPart, activity) == 0x38 && offsetof(LayerPart, layer) == 0x48);

struct SimplePart : LayerPart {
    float x, y;   // 0x4c
};
static_assert(sizeof(SimplePart) == 0x54);

struct FreeRangePoly : LayerPart {
    ZPolygon polygon;   // 0x4c

    static FreeRangePoly *THISCALL Ctor(FreeRangePoly *self);                     // 0x4b5fa0
    static DWORD THISCALL Dtor(FreeRangePoly *self);                              // 0x4b5fd0
    static FreeRangePoly *THISCALL DeletingDtor(FreeRangePoly *self, DWORD flags);   // 0x4b5e00 (slot 1)
    static float THISCALL Zero(FreeRangePoly *self);                              // 0x4b5cb0 (slots 46, 55)
    static void THISCALL Slot23(FreeRangePoly *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e);   // 0x4b5cc0
    static const char *THISCALL DisplayName(FreeRangePoly *self);                 // 0x4b5cd0 (slot 61)
    static ZPolygon *THISCALL Polygon(FreeRangePoly *self);                       // 0x4b5ce0 (slot 48)
    static DWORD THISCALL SetPolygon(FreeRangePoly *self, const ZPolygon *p);     // 0x4b5cf0 (slot 49)
    static int THISCALL RoundX(FreeRangePoly *self);                              // 0x4b5d10 (slot 52)
    static int THISCALL RoundY(FreeRangePoly *self);                              // 0x4b5d30 (slot 53)
    static void THISCALL Slot29(FreeRangePoly *self, DWORD arg);                  // 0x4b5ff0
    static void THISCALL Update(FreeRangePoly *self, DWORD arg);                  // 0x4b6020 (slot 35)
    static void THISCALL SetFlags(FreeRangePoly *self, DWORD bits);               // 0x4b6040 (slot 58)
    static void THISCALL ClearFlags(FreeRangePoly *self, DWORD bits);             // 0x4b6070 (slot 59)
    static DWORD THISCALL SetPosition(FreeRangePoly *self, DWORD x, DWORD y);     // 0x4b60a0 (slot 41)
    static float *THISCALL Offset(FreeRangePoly *self, float *out);               // 0x4b60e0 (slot 50)
    static float *THISCALL Position(FreeRangePoly *self, float *out);             // 0x4b6100 (slot 43)
    static float THISCALL X(FreeRangePoly *self);                                 // 0x4b6140 (slot 44)
    static float THISCALL Y(FreeRangePoly *self);                                 // 0x4b6160 (slot 45)
    static DWORD THISCALL HitTest(FreeRangePoly *self, int x, int y, DWORD kind);   // 0x4b6180 (slot 38)
    static DWORD THISCALL Rect(FreeRangePoly *self, int *rect, DWORD kind, DWORD offset);   // 0x4b61e0 (slot 37)
};
static_assert(sizeof(FreeRangePoly) == 0x74);

struct SignPart : SimplePart {
    ZString text;            // 0x54
    ZString wrapped;         // 0x58
    void *font;              // 0x5c
    DWORD border_texture;    // 0x60
    int max_width;           // 0x64
    int text_width;          // 0x68
    int text_height;         // 0x6c
    DWORD drawable[6];       // 0x70 vtable 0x6020e0, +0x74, +0x78, +0x7c, +0x80 part, +0x84
    DWORD sort_key;          // 0x88
    DWORD unknown8c[2];      // 0x8c
    BYTE flag94, flag95;     // 0x94
    BYTE pad96[2];
    SignPart *self_ref;      // 0x98

    static SignPart *THISCALL Ctor(SignPart *self);                              // 0x58f8a0
    static SignPart *THISCALL DeletingDtor(SignPart *self, DWORD flags);         // 0x58f410 (slot 1)
    static const char *THISCALL DisplayName(SignPart *self);                     // 0x58f4c0 (slot 61)
    static DWORD THISCALL Slot23(SignPart *self, DWORD a, void *b, DWORD c, DWORD d, DWORD e);   // 0x58fa30
    static DWORD THISCALL Rect(SignPart *self, int *rect, DWORD kind, const int *offset);   // 0x58fab0 (slot 37; al)
    static DWORD THISCALL HitTest(SignPart *self, int x, int y, DWORD kind);     // 0x58fb60 (slot 38; al)
    static DWORD THISCALL Relayout(SignPart *self);                              // 0x58fbb0 (slot 78)
    static int THISCALL SetMaxWidth(SignPart *self, int w);                      // 0x58fc30
    static ZString *THISCALL SetText(SignPart *self, const ZString *t);          // 0x58fc60
};
static_assert(sizeof(SignPart) == 0x9c);

constexpr DWORD VT_LAYER_PART = 0x5f93d8, VT_SIMPLE_PART = 0x600344, VT_FREE_RANGE_POLY = 0x5f4da4,
                VT_SIGN_PART = 0x601f94, VT_ACTIVITY = 0x5f951c, VT_ACTIVITY_BASE = 0x5f9528,
                VT_SIGN_DRAWABLE = 0x6020e0, VT_MESSAGE = 0x5eeb64, VT_QUERY_MESSAGE = 0x5f9534;
constexpr DWORD CLASS_LAYER_PART = 0x6c29cc, CLASS_SIMPLE_PART = 0x713560, CLASS_FREE_RANGE_POLY = 0x6bfb08,
                CLASS_SIGN_PART = 0x713a3c;
// Message ids (set by static initializers from 0x506680)
constexpr DWORD MSG_BEGIN = 0x6c2998, MSG_END = 0x6c29b0, MSG_DEACTIVATE_AI = 0x6c29e8, MSG_TARGET = 0x6c29e0;

}  // namespace zax
