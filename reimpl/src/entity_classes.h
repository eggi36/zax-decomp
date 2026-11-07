// The entity classes above the base classes (0x490010-0x494410 and 0x49a8c0-0x49c272): CEntityMovable (an
// animated entity with velocity, acceleration and friction), CEntityProjectile, CEntityAnimatedLine (a
// CEntityBase drawn as a line of strips), CEntityAnimatedUpperBody, the abstract CEntityCharacter (an
// inventory, a home position and the last discharge) and its CEntityStationaryCharacter, CEntityWalking
// and CEntityFlying, plus CEntityAnimated's attached-object slots 81 and 86-91 and two helpers of it.
//
// CEntityMovable (0x108 bytes, class 0x6bdd9c, version 9, parent CEntityAnimated, vtable 0x5f21d8 with 127
// slots) [confirmed: Create 0x490700, RegisterFields 0x490960, docs/fields.csv]:
//   +0xe4 "Friction" (the share of the velocity taken away per update)
//   +0xe8 "Velocity X", +0xec "Velocity Y" ("Pixels Per Second"), +0xf0 the height velocity (no field)
//   +0xf4 "Acceleration X", +0xf8 "Acceleration Y", +0xfc the height acceleration (no field)
//   +0x100/+0x104 the position the next update moves to (two hidden float fields without a name)
// Slot 31 (seconds) adds acceleration * seconds to the velocity, clears the acceleration, takes friction *
// velocity away and moves the target position (and the rendering height) by velocity * seconds; slot 35
// moves the entity there: through CLayerSaveData::Bounce (0x4e96c0, the filter 0x6bdda0) when it "Tries
// To Collide" (0x80000), otherwise through slot 41. Slot 94 sets up the bounce motion from the model's
// bounce values and ends the game for a bounce factor outside 0..1 [all: confirmed, code].
//
// CEntityProjectile (0x114, class 0x6bdda4, version 2, parent CEntityMovable, vtable 0x5f23d4):
//   +0x108 "Max Acceleration" ("Pixels/Sec/Sec"), +0x10c "Shot Duration" (seconds left; default 2),
//   +0x110 a minimum the duration getter 0x492080 keeps it at (0.01 after the constructor)
// Slot 35 counts the duration down; at its end the projectile takes its remaining hit points as damage
// of type "Unavoidable" (or, without hit points, is removed through world slot 91 and LayerPart_Died).
//
// CEntityAnimatedLine (0xe0, class 0x6bddb8, version 3, parent CEntityBase, vtable 0x5f25e8):
//   +0x7c "Outside Color", +0x8c "Middle Color" (FieldColor, 0x10 bytes each)
//   +0x9c the strips (an array of 0x60-byte drawables, vtable 0x5f25dc; data +0xa0, count +0xa4,
//         capacity +0xa8; element vtable 0x5f2774)
//   +0xac..+0xb8 "BoundLeft", "BoundTop", "BoundRight", "BoundBottom" (ints, the rectangle of slot 37)
//   +0xbc "Start", +0xc4 "End", +0xcc "Normal", +0xd4 "PerpenNormal" (points), +0xdc "Width" (4)
// Slot 97 (start, end) sorts the end points by y, moves the entity to the start, sets the unit normal and
// its perpendicular and builds the strips (0x493850): a band of "Width" times the perpendicular around the
// line cut into rows of 16 pixels, one strip (0x4936b0) per row with its x range, the bounds the rows' extent.
//
// CEntityAnimatedUpperBody (0xe8, class 0x6bddc4, version 1, parent CEntityAnimated, vtable 0x5f278c):
//   +0xe4 a byte set while its slot 35 runs (slot 46 then answers the root's facing angle)
//
// CEntityCharacter (0x12c, abstract, class 0x6bdea0, version 7, parent CEntityMovable, vtable 0x5f324c):
//   +0x108 "Inventory/Inventory" (an owned CInventoryBase), +0x10c "Home Pos X", +0x110 "Home Pos Y",
//   +0x114 "LastDischargePosX", +0x118 "LastDischargePosY", +0x11c/+0x120 that position turned by the
//   aiming angle (slot 47; y times 0.7128), +0x124 "LastDischargeSequenceName" (ZString), +0x128 a byte the
//   multiplayer block carries: the character is the one of 0x6bde9c (the local player's, cleared by the
//   destructor). "Inventory/Weapons Use Amo" is bit 0x10000000 of the layer part's flags.
// Every character constructor sets the bounce filter 0x6bdda0 to 0x49adf0: characters pass through each
// other unless the other one is a CEntityStationaryCharacter.
// CEntityStationaryCharacter (0x12c, 0x6bdea4, vtable 0x5f3458), CEntityWalking (0x130, 0x6bdea8, version
// 3, vtable 0x5f3664; +0x12c "Debug/Movement Speed", an index into the names 0x6bde80, default "Walk") and
// CEntityFlying (0x130, 0x6bdeac, vtable 0x5f3874; +0x12c "Max Acceleration").
//
// Left out (documented in docs/DECOMPILATION.md): the drawing of CEntityAnimatedLine (slot 23 0x493400,
// which queues the strips; the debug drawing slot 29 0x492fc0; the strips' own drawing, their slot 1
// 0x4923e0); the static registrations (0x490950, 0x491da0,
// 0x4928f0, 0x494380, 0x49a9f0, 0x49b7a0, 0x49bbb0, 0x49c170), the block readers' static initializers
// (0x492650, 0x4926b0, 0x49a920) and the names of "Movement Speed" (0x49b7e0).
//
// Return values are what the original leaves in eax (or st(0)); byte results say "al".
#pragma once
#include "entities.h"

namespace zax {

struct Movable : Animated {
    float friction;            // 0xe4
    float velocity[3];         // 0xe8 x, y, height
    float acceleration[3];     // 0xf4
    float to[2];               // 0x100

    static Movable *THISCALL Ctor(Movable *self);                                  // 0x490f40
    static void THISCALL Dtor(Movable *self);                                      // 0x490fa0
    static Movable *THISCALL DeletingDtor(Movable *self, DWORD flags);             // 0x490800 (slot 1)
    static void THISCALL SetVelocityFrom(Movable *self, const BYTE *result);       // 0x4904a0 (slot 95)
    static void THISCALL SetFriction(Movable *self, DWORD f);                      // 0x4904c0 (slot 105)
    static float THISCALL Friction(Movable *self);                                 // 0x4904d0 (slot 106)
    static void THISCALL SetVelocity3(Movable *self, const DWORD *v);              // 0x490500 (slot 107)
    static void THISCALL SetVelocity2(Movable *self, const DWORD *v);              // 0x4904e0 (slot 108)
    static void THISCALL SetVelocityXY(Movable *self, DWORD x, DWORD y);           // 0x491890 (slot 109)
    static void THISCALL SetSpeed(Movable *self, float speed);                     // 0x491840 (slot 110)
    static void THISCALL AddVelocity3(Movable *self, const float *v);              // 0x490560 (slot 111)
    static void THISCALL AddVelocity2(Movable *self, const float *v);              // 0x490530 (slot 112)
    static void THISCALL AddVelocityXY(Movable *self, float x, float y);           // 0x4918b0 (slot 113)
    static float *THISCALL VelocityPtr(Movable *self);                             // 0x4905e0 (slot 114)
    static DWORD THISCALL VelocityTo(Movable *self, float *out);                   // 0x4905c0 (slot 115)
    static void THISCALL VelocityXY(Movable *self, DWORD *x, DWORD *y);            // 0x4905a0 (slot 116)
    static void THISCALL Forward(Movable *self, float *out);                       // 0x491610 (slot 117)
    static void THISCALL SetAcceleration3(Movable *self, const DWORD *v);          // 0x490610 (slot 118)
    static void THISCALL SetAcceleration2(Movable *self, const DWORD *v);          // 0x4905f0 (slot 119)
    static void THISCALL SetAccelerationXY(Movable *self, DWORD x, DWORD y);       // 0x4918e0 (slot 120)
    static void THISCALL AddAcceleration3(Movable *self, const float *v);          // 0x490670 (slot 121)
    static void THISCALL AddAcceleration2(Movable *self, const float *v);          // 0x490640 (slot 122)
    static void THISCALL AddAccelerationXY(Movable *self, float x, float y);       // 0x491900 (slot 123)
    static float *THISCALL AccelerationPtr(Movable *self);                         // 0x4906f0 (slot 124)
    static DWORD THISCALL AccelerationTo(Movable *self, float *out);               // 0x4906d0 (slot 125)
    static void THISCALL AccelerationXY(Movable *self, DWORD *x, DWORD *y);        // 0x4906b0 (slot 126)
    static void THISCALL Integrate(Movable *self, DWORD seconds);                  // 0x490fc0 (slot 31)
    static void THISCALL Update(Movable *self, DWORD arg);                         // 0x491160 (slot 35)
    static void THISCALL SetTarget(Movable *self, DWORD x, DWORD y);               // 0x491230
    static DWORD THISCALL CanMoveTo(Movable *self, const float *to);               // 0x491250
    static DWORD THISCALL PointFree(Movable *self, const float *to, DWORD filter, DWORD kind, const float *offset);   // 0x4912b0
    static DWORD THISCALL CanSee(Movable *self, LayerPart *other, DWORD filter, const float *other_offset, DWORD kind,
                                 const float *offset);                             // 0x491380
    static DWORD THISCALL TryingToMove(Movable *self);                             // 0x4914d0
    static void THISCALL SetPosition(Movable *self, DWORD x, DWORD y);             // 0x4914e0 (slot 41)
    static int THISCALL ExpandRect(Movable *self, void *sweep, int *rect);         // 0x4916f0
    static DWORD THISCALL BounceMotion(Movable *self, void *motion);               // 0x491710 (slot 94)
};
static_assert(sizeof(Movable) == 0x108);

struct Projectile : Movable {
    float max_acceleration;    // 0x108
    float duration;            // 0x10c
    float min_duration;        // 0x110

    static Projectile *THISCALL Ctor(Projectile *self);                            // 0x4920b0
    static void THISCALL Dtor(Projectile *self);                                   // 0x492030
    static Projectile *THISCALL DeletingDtor(Projectile *self, DWORD flags);       // 0x491bd0 (slot 1)
    static void THISCALL BounceMotion(Projectile *self, BYTE *motion);             // 0x492060 (slot 94)
    static float THISCALL Duration(Projectile *self);                              // 0x492080
    static void THISCALL Update(Projectile *self, DWORD seconds);                  // 0x492150 (slot 35)
};
static_assert(sizeof(Projectile) == 0x114);

struct AnimatedLine : Entity {
    BYTE outside_color[0x10];  // 0x7c
    BYTE middle_color[0x10];   // 0x8c
    struct {
        DWORD vftable;
        BYTE *data;
        DWORD count;
        DWORD capacity;
    } strips;                  // 0x9c
    int bounds[4];             // 0xac
    float start[2];            // 0xbc
    float end[2];              // 0xc4
    float normal[2];           // 0xcc
    float perpendicular[2];    // 0xd4
    float width;               // 0xdc

    static AnimatedLine *THISCALL Ctor(AnimatedLine *self);                        // 0x492f50
    static AnimatedLine *THISCALL DeletingDtor(AnimatedLine *self, DWORD flags);   // 0x492790 (slot 1)
    static DWORD THISCALL Rect(AnimatedLine *self, int *out, DWORD kind, DWORD unused);   // 0x4931b0 (slot 37; al)
    static DWORD THISCALL WriteBlock(AnimatedLine *self, void *packet, BYTE header);   // 0x493230 (slot 25)
    static void THISCALL ReadBlock(AnimatedLine *self, void *packet);              // 0x4932e0 (slot 27)
    static void THISCALL SetLine(AnimatedLine *self, const float *a, const float *b);   // 0x493520 (slot 97)
    static void THISCALL SetLineXY(AnimatedLine *self, DWORD x0, DWORD y0, DWORD x1, DWORD y1);   // 0x493670 (slot 98)
};
static_assert(sizeof(AnimatedLine) == 0xe0);

struct UpperBody : Animated {
    BYTE updating;             // 0xe4
    BYTE pad[3];

    static UpperBody *THISCALL DeletingDtor(UpperBody *self, DWORD flags);         // 0x494290 (slot 1)
    static void THISCALL Update(UpperBody *self, DWORD arg);                       // 0x494390 (slot 35)
    static long double THISCALL FacingAngle(UpperBody *self);                      // 0x4943b0 (slot 46)
};
static_assert(sizeof(UpperBody) == 0xe8);

struct Character : Movable {
    void *inventory;           // 0x108
    float home[2];             // 0x10c
    float discharge[2];        // 0x114
    float discharge_turned[2]; // 0x11c
    ZString discharge_sequence;   // 0x124
    BYTE local;                // 0x128
    BYTE pad[3];

    static Character *THISCALL DeletingDtor(Character *self, DWORD flags);         // 0x49af70 (slot 1)
    static void THISCALL ResetFields(Character *self);                             // 0x49afd0 (slot 19)
    static DWORD THISCALL WriteBlock(Character *self, void *packet, BYTE header);  // 0x49b2a0 (slot 25)
    static void THISCALL ReadBlock(Character *self, void *packet);                 // 0x49b300 (slot 27)
    static void THISCALL Integrate(Character *self, DWORD seconds);                // 0x49b040 (slot 31)
    static void *THISCALL Inventory(Character *self);                              // 0x49a8c0 (slot 36)
    static DWORD THISCALL HomePosition(Character *self);                           // 0x49a8d0 (slot 39)
    static void THISCALL HomeTo(Character *self, DWORD *out);                      // 0x49a900 (slot 40)
    static long double THISCALL FacingAngle(Character *self);                      // 0x49b4b0 (slot 46)
    static float *THISCALL TurnDischarge(Character *self);                         // 0x49b1d0 (slot 47)
    static void THISCALL Broadcast(Character *self, DWORD arg);                    // 0x49b3a0 (slot 62)
    static void *THISCALL TakeInventory(Character *self);                          // 0x49b330 (slot 127)
    static void THISCALL SetInventory(Character *self, void *inventory);           // 0x49b360 (slot 128)
    static void THISCALL SetHome(Character *self, const DWORD *p);                 // 0x49a8e0 (slot 129)
    static void THISCALL InventoryUpdate(Character *self, DWORD seconds);          // 0x49b000 (slot 130)
    static DWORD THISCALL SetDischarge(Character *self, const DWORD *p, const ZString *sequence);   // 0x49b1a0
    static void THISCALL FlyingUpdate(Character *self, DWORD arg);                 // 0x49b090 (CEntityFlying's slot 35)
    static void THISCALL StationaryUpdate(Character *self, DWORD arg);             // 0x49b7b0 (slot 35)
    static void THISCALL WalkingUpdate(Character *self, DWORD arg);                // 0x49bd70 (slot 35)
    static void THISCALL WalkingDie(Character *self);                              // 0x49bd90 (slot 97)
};
static_assert(sizeof(Character) == 0x12c);

struct Walking : Character {
    DWORD movement_speed;      // 0x12c
    static Walking *THISCALL Ctor(Walking *self);                                  // 0x49bcb0
    static void THISCALL Dtor(Walking *self);                                      // 0x49bd20
};
static_assert(sizeof(Walking) == 0x130);

constexpr DWORD VT_MOVABLE = 0x5f21d8, VT_PROJECTILE = 0x5f23d4, VT_ANIMATED_LINE = 0x5f25e8, VT_LINE_STRIPS = 0x5f25dc,
                VT_UPPER_BODY = 0x5f278c, VT_CHARACTER = 0x5f324c, VT_STATIONARY = 0x5f3458, VT_WALKING = 0x5f3664,
                VT_FLYING = 0x5f3874;
constexpr DWORD CLASS_MOVABLE = 0x6bdd9c, CLASS_PROJECTILE = 0x6bdda4, CLASS_ANIMATED_LINE = 0x6bddb8,
                CLASS_UPPER_BODY = 0x6bddc4, CLASS_CHARACTER = 0x6bdea0, CLASS_STATIONARY = 0x6bdea4,
                CLASS_WALKING = 0x6bdea8, CLASS_FLYING = 0x6bdeac;
// The bounce filter of CEntityMovable's slot 35 (set by every character constructor to 0x49adf0), the
// local player's character, the CEntityAnimated converter object (vtable 0x5f1fe0)
constexpr DWORD BOUNCE_FILTER = 0x6bdda0, LOCAL_CHARACTER = 0x6bde9c, ANIMATED_CONVERTER = 0x6bdd70;
// The block readers of CEntityAnimatedLine and CEntityCharacter; the object the readers' create functions
// leave in 0x6c29fc
constexpr DWORD READER_ANIMATED_LINE = 0x6bddb0, READER_CHARACTER = 0x6bde70, LAST_READ_OBJECT = 0x6c29fc;

CClassInfo *__cdecl EntityMovable_ClassInfo();       // 0x4903f0, 0x4908a0
CClassInfo *__cdecl EntityCharacter_ClassInfo();     // 0x49a930, 0x49a990

}  // namespace zax
