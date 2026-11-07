// The entity base classes of Zax.exe: CEntityBase (0x487620-0x48bdf0, a layer part with a model and a
// sequence), CEntityDestructable (0x48bdf0-0x48dbb0, an entity with hit points and death) and
// CEntityAnimated (0x48dcc0-0x48ffc0, an entity with angles, a frame, a behaviour stack and attached
// children). CEntityMovable and the classes above 0x490000 derive from CEntityAnimated.
//
// CEntityBase (0x7c bytes, class 0x6bdd44, version 11, parent CLayerPart, vtable 0x5f1c4c with 97 slots;
// alternative class name "CEntity", set by RegisterFields) [confirmed: Create 0x487670, constructor
// 0x487ff0, RegisterFields 0x487aa0, docs/fields.csv]:
//   +0x4c "Position/Position X", +0x50 "Position/Position Y" (floats; setters 0x48a0a0/0x48a0d0)
//   +0x54/+0x58 the position rounded (fistp; slots 52/53), kept by slot 41
//   +0x5c..+0x68 a cached rectangle (four dwords; cleared with every change of position, height, model,
//         also in every parent that is an entity: 0x488cf0)
//   +0x6c "Position/Rendering Height" (int, -500..500, setter 0x48b6b0), +0x70 "Position/Rendering
//         Height Float" (-500..500, setter 0x48b770); slot 54/55
//   +0x74 "Model" (index into the model list 0x6cfdd8, default "!Unknown Model"; slot 93 sets it)
//   +0x78 "Cur Sequence" (index into the model's sequences, setter 0x48b120; -1 none)
// The model is the object of record +0x74 of the model list (records of 0x80 bytes at 0x6cfe08, count
// 0x6cfe0c, loaded on first use through 0x55e090); almost every slot inlines that lookup (0x48b290 is the
// out-of-line copy, 0x48d8f0 the same for any definition list). Several slots forward to the model's
// "Activity" (CModel +0x08, CFlyweightLayerPartAI objects shared by every entity of the model) before
// the layer part's own AIs.
//
// CEntityDestructable (0x90, class 0x6bdd4c, version 13, parent CEntityBase, vtable 0x5f1e4c):
//   +0x7c "Cur Damage" (units "Hit Points"), +0x80 "Destroyed Action", +0x84 "Damaged Action", +0x88
//   "Hit Action" (owned CAction), +0x8c "Default Death Type" (index into the death types 0x6bd8b0,
//   default "!None", alternative name "Death Type"). Hit points are the model's (+0x20); the remaining
//   ones are hit points - damage.
//
// CEntityAnimated (0xe4, class 0x6bdd98, version 20, parent CEntityDestructable, vtable 0x5f2010):
//   +0x90 the embedded behaviour stack (CEntityBehaviorStack, 0x20 bytes: constructor 0x49a3a0,
//         destructor 0x49a430, update 0x49a480, message 0x49a530, clear 0x49a6b0)
//   +0xb0 flags: 1 "Facing Backwards", 2 "Last Facing Backwards", 4 "Upper Body Present", 8 "Synch Aiming
//         And Moving", 0x10 "Frame Index Locked", 0x20 "Waiting For Discharge", 0x40 "Still Shooting",
//         0x80 "Trying To Move", 0x100 "Tracking Updates Since Relocate" (8 after the constructor)
//   +0xb4 the fade (1.0 opaque; slot 92 reads the top parent's), +0xb8 "Play Ratio", +0xbc "Debug/Current
//   Frame" (float), +0xc0 the frame index, +0xc4 "Debug/Aiming Angle", +0xc8 "Position/Movement Angle",
//   +0xcc "Position/Start FacingAngle" (radians), +0xd0 "Debug/Created By" (a part this one listens to)
//   +0xd4 a pool pointer array (vtable 0x5f21b4, base 0x5f21c0): data +0xd8, count +0xdc, capacity +0xe0
//         (objects written and read with the multiplayer block, deleted through slot 1 by the destructor)
// The dynamic field "Debug/Num Updates Since Relocate" (0x6bdd50, an enum int field made by the static
// initializer 0x48dc10) counts the updates while flag 0x100 is set.
//
// Left out (documented in docs/DECOMPILATION.md): the drawing (slot 23 0x488f80, Entity_Draw 0x488d70,
// DrawRotationSequenceLayers 0x4886a0, the drawable pool 0x488bb0/0x488bd0/0x487fa0), the frame
// rectangles of slot 37 (0x48a100), the pixel hit test of slot 38 (0x489cd0) and the dirty rectangles of
// slot 82 (0x489a40), which all go through the frames of the rotation sequences; the multiplayer block
// readers' create functions (0x487080, 0x4872d0, 0x48dac0, 0x48db30, 0x48dbb0) and their static
// initializers.
//
// Return values are what the original leaves in eax (or st(0)); byte results say "al".
#pragma once
#include "layer_parts.h"

namespace zax {

struct CModel;

struct Entity : LayerPart {
    float x, y;                // 0x4c
    int rx, ry;                // 0x54 rounded position
    int bounds[4];             // 0x5c cached rectangle
    int render_height;         // 0x6c
    float render_height_f;     // 0x70
    DWORD model;               // 0x74
    DWORD sequence;            // 0x78

    static Entity *THISCALL Ctor(Entity *self);                                   // 0x487ff0
    static void THISCALL Dtor(Entity *self);                                      // 0x488150
    static Entity *THISCALL DeletingDtor(Entity *self, DWORD flags);              // 0x4879b0 (slot 1)
    static DWORD THISCALL ReadOwnLine(Entity *self, const char *name, DWORD value, DWORD reader);   // 0x488170 (slot 11)
    static void THISCALL Update(Entity *self, DWORD arg);                         // 0x4881b0 (slot 35)
    static void THISCALL Activate(Entity *self);                                  // 0x4882f0 (slot 33)
    static DWORD THISCALL Message(Entity *self, void *message);                   // 0x488420 (slot 63)
    static void THISCALL Slot29(Entity *self, DWORD arg);                         // 0x488560
    static void THISCALL ClearBounds(Entity *self);                               // 0x488cf0
    static void __fastcall WritePosition(void *packet, Entity *self, BYTE height);   // 0x4890c0
    static void __fastcall ReadPosition(void *packet, Entity *self, BYTE height);    // 0x489230
    static DWORD THISCALL WriteBlock(Entity *self, void *packet, BYTE header);    // 0x4893b0 (slot 25)
    static void THISCALL ReadBlock(Entity *self, void *packet);                   // 0x4896e0 (slot 27)
    static void THISCALL Relist(Entity *self);                                    // 0x489fb0 (slot 78)
    static void THISCALL SetPosition(Entity *self, DWORD x, DWORD y);             // 0x489fd0 (slot 41)
    static void THISCALL SetFlags(Entity *self, DWORD bits);                      // 0x48a700 (slot 58)
    static void THISCALL ReplaceFlags(Entity *self, DWORD flags);                 // 0x48a750 (slot 60)
    static const char *THISCALL DisplayName(Entity *self);                        // 0x48a7a0 (slot 61)
    static DWORD THISCALL AllAIs(Entity *self, DWORD a, DWORD b);                 // 0x48a7c0 (slot 65; al)
    static DWORD THISCALL AnyAI(Entity *self, DWORD a, DWORD b);                  // 0x48a920 (slot 66; al)
    static void THISCALL Slot67(Entity *self, DWORD a, DWORD b, DWORD c, DWORD d);   // 0x48aa80
    static int THISCALL SortingY(Entity *self);                                   // 0x48abf0 (slot 56)
    static void THISCALL SetSortingY(Entity *self, int y);                        // 0x48ad50 (slot 57)
    static DWORD THISCALL Sequence(Entity *self);                                 // 0x48aed0
    static const char *THISCALL SequenceName(Entity *self);                       // 0x48aee0
    static ZString *THISCALL SequenceNameString(Entity *self);                    // 0x48b000
    static void THISCALL SetSequence(Entity *self, DWORD sequence);               // 0x48b120
    static CModel *THISCALL Model(Entity *self);                                  // 0x48b290
    static void THISCALL SetModel(Entity *self, DWORD model);                     // 0x48b390 (slot 93)
    static DWORD THISCALL ModelIndex(Entity *self);                               // 0x48b560
    static DWORD THISCALL Slot96(Entity *self, void *sequence);                   // 0x48b570
    static void *THISCALL SequenceObject(Entity *self);                           // 0x48b590
    static int THISCALL SetRenderHeight(Entity *self, int h);                     // 0x48b6b0
    static float THISCALL SetRenderHeightFloat(Entity *self, float h);            // 0x48b770
    static void *THISCALL Polygon(Entity *self);                                  // 0x48b860 (slot 48)
    static void THISCALL SetPolygon(Entity *self, const void *p);                 // 0x48b9c0 (slot 49)
    static DWORD THISCALL HasActivity(Entity *self);                              // 0x48bb20 (slot 83; al)
    static void *THISCALL FindAI(Entity *self, CClassInfo *cls, DWORD query);     // 0x48bc90 (slot 64)
    static int THISCALL RoundX(Entity *self);                                     // 0x487240 (slot 52)
    static int THISCALL RoundY(Entity *self);                                     // 0x487250 (slot 53)
    static int THISCALL RenderHeight(Entity *self);                               // 0x487260 (slot 54)
    static float THISCALL RenderHeightFloat(Entity *self);                        // 0x487270 (slot 55)
    static void THISCALL ClearState(Entity *self, DWORD *out);                    // 0x487280 (slot 94)
};
static_assert(sizeof(Entity) == 0x7c);

struct Destructable : Entity {
    float damage;              // 0x7c
    void *destroyed_action;    // 0x80
    void *damaged_action;      // 0x84
    void *hit_action;          // 0x88
    DWORD death_type;          // 0x8c

    static Destructable *THISCALL Ctor(Destructable *self);                       // 0x48c890
    static void THISCALL Dtor(Destructable *self);                                // 0x48ca00
    static Destructable *THISCALL DeletingDtor(Destructable *self, DWORD flags);  // 0x48c200 (slot 1)
    static void THISCALL Update(Destructable *self, DWORD arg);                   // 0x48ca40 (slot 35)
    static void THISCALL OnCollide(Destructable *self, LayerPart *other, DWORD b, DWORD c, DWORD d);   // 0x48cb90 (slot 67)
    static void THISCALL Die(Destructable *self);                                 // 0x48cdf0 (slot 97)
    static void THISCALL ResetDamage(Destructable *self);                         // 0x48d020
    static long double THISCALL HitPointsLeft(Destructable *self);                // 0x48d030
    static long double THISCALL HitPoints(Destructable *self);                    // 0x48d150
    static long double THISCALL HealthRatio(Destructable *self);                  // 0x48d260
    static void THISCALL Heal(Destructable *self, float amount);                  // 0x48d330 (slot 98)
    static void THISCALL TakeDamage(Destructable *self, float amount, DWORD type, LayerPart *by);   // 0x48d380 (slot 99)
    static float THISCALL DamageModifier(Destructable *self, float amount, DWORD type);   // 0x48d6e0 (slot 100)
    static void THISCALL Activate(Destructable *self);                            // 0x48d6f0 (slot 33)
    static void THISCALL GroundDamage(Destructable *self, float seconds);         // 0x48d7f0
};
static_assert(sizeof(Destructable) == 0x90);

struct Animated : Destructable {
    BYTE behaviors[0x20];      // 0x90 CEntityBehaviorStack
    DWORD aflags;              // 0xb0
    float fade;                // 0xb4
    float play_ratio;          // 0xb8
    float frame;               // 0xbc
    DWORD frame_index;         // 0xc0
    float aiming;              // 0xc4
    float movement;            // 0xc8
    float start_facing;        // 0xcc
    LayerPart *created_by;     // 0xd0
    LPArray attached;          // 0xd4

    static Animated *THISCALL Ctor(Animated *self);                               // 0x48e920
    static void THISCALL Dtor(Animated *self);                                    // 0x48ea40
    static Animated *THISCALL DeletingDtor(Animated *self, DWORD flags);          // 0x48dda0 (slot 1)
    static void THISCALL SetStartFacing(Animated *self, float a);                 // 0x48e8f0
    static float THISCALL Frame(Animated *self);                                  // 0x48e9b0 (slot 84)
    static DWORD THISCALL FrameIndex(Animated *self);                             // 0x48e9c0 (slot 85)
    static DWORD THISCALL Opaque(Animated *self, DWORD unused);                   // 0x48e9d0 (slot 77)
    static void THISCALL Activate(Animated *self);                                // 0x48eb50 (slot 33)
    static DWORD THISCALL WaitingForDischarge(Animated *self);                    // 0x48ebb0
    static DWORD THISCALL SetWaitingForDischarge(Animated *self, BYTE on);        // 0x48ebc0
    static DWORD THISCALL SetFacingBackwards(Animated *self, BYTE on);            // 0x48ebf0
    static DWORD THISCALL FrameIndexLocked(Animated *self);                       // 0x48ec20
    static DWORD THISCALL StillShooting(Animated *self);                          // 0x48ec30
    static DWORD THISCALL SetStillShooting(Animated *self, BYTE on);              // 0x48ec40
    static DWORD THISCALL WriteBlock(Animated *self, void *packet, BYTE header);  // 0x48ec70 (slot 25)
    static void THISCALL ReadBlock(Animated *self, void *packet);                 // 0x48ef00 (slot 27)
    static DWORD THISCALL UpperBodyPresent(Animated *self);                       // 0x48f180 (slot 102)
    static LayerPart *THISCALL CreatedBy(Animated *self);                         // 0x48f190 (slot 68)
    static void THISCALL SetCreatedBy(Animated *self, LayerPart *by);             // 0x48f1a0 (slot 69)
    static DWORD THISCALL OnMessage(Animated *self, LayerPart *sender, const char *message);   // 0x48f1e0 (slot 0)
    static DWORD THISCALL UpdateFacing(Animated *self);                           // 0x48f260 (al)
    static void THISCALL Update(Animated *self, DWORD arg);                       // 0x48f300 (slot 35)
    static void THISCALL Slot29(Animated *self, DWORD arg);                       // 0x48f490
    static void THISCALL SetModel(Animated *self, DWORD model);                   // 0x48f4b0 (slot 93)
    static float THISCALL Fade(Animated *self);                                   // 0x48f510 (slot 92)
    static void THISCALL Die(Animated *self);                                     // 0x48f590 (slot 97)
    static long double THISCALL FramePart(Animated *self);                        // 0x48f600
    static void THISCALL Explode(Animated *self);                                 // 0x48f630 (slot 101)
    static void THISCALL SetPlayRatio(Animated *self, float r);                   // 0x48f6a0
    static DWORD THISCALL PlaySequence(Animated *self, const char *name);         // 0x48f700 (al)
    static void THISCALL SetAiming(Animated *self, float a, BYTE children);       // 0x48f8b0 (slot 103)
    static void THISCALL SetMovement(Animated *self, float a, BYTE children);     // 0x48fa00 (slot 104)
    static void THISCALL SetSynch(Animated *self, BYTE on);                       // 0x48fb50
    static DWORD THISCALL Synch(Animated *self);                                  // 0x48fc40
    static long double THISCALL FacingAngle(Animated *self);                      // 0x48fc50 (slot 46)
    static DWORD THISCALL FrameOf(Animated *self, void *sequence);                // 0x48fd40 (slot 96)
    static int THISCALL FrameNumber(Animated *self, void *sequence);              // 0x48fd80
    static int THISCALL UpdatesSinceRelocate(Animated *self);                     // 0x48fe30
    static void THISCALL SetUpdatesSinceRelocate(Animated *self, DWORD n);        // 0x48fe40
    static int THISCALL Rotation(Animated *self, void *sequence);                 // 0x48fe80
    static void THISCALL SetFrame(Animated *self, float f);                       // 0x48feb0
    static void THISCALL LockFrame(Animated *self, float f);                      // 0x48ff90
    static void THISCALL UnlockFrame(Animated *self);                             // 0x48ffb0
    static void THISCALL SetFrameFraction(Animated *self, float f);               // 0x48ffc0
};
static_assert(sizeof(Animated) == 0xe4);

constexpr DWORD VT_ENTITY_BASE = 0x5f1c4c, VT_ENTITY_DESTRUCTABLE = 0x5f1e4c, VT_ENTITY_ANIMATED = 0x5f2010,
                VT_ENTITY_ATTACHED = 0x5f21b4, VT_ENTITY_ATTACHED_BASE = 0x5f21c0;
constexpr DWORD CLASS_ENTITY_BASE = 0x6bdd44, CLASS_ENTITY_DESTRUCTABLE = 0x6bdd4c, CLASS_ENTITY_ANIMATED = 0x6bdd98;
// The model list's records (the list MODEL_LIST is in models.h), the death type list
constexpr DWORD MODEL_RECORDS = 0x6cfe08, MODEL_COUNT = 0x6cfe0c, DEATH_TYPES = 0x6bd8b0;
// The dynamic field "Debug/Num Updates Since Relocate" and the reader selection of slot 27
constexpr DWORD UPDATES_FIELD = 0x6bdd50, BASE_READ_MODE = 0x6bdd40, ANIMATED_READ_MODE = 0x6bdd94;

CClassInfo *__cdecl EntityBase_ClassInfo();          // 0x487620
CClassInfo *__cdecl EntityDestructable_ClassInfo();  // 0x48bdf0
CClassInfo *__cdecl EntityAnimated_ClassInfo();      // 0x48dcc0

}  // namespace zax
