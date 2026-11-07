// The AI classes of 0x4cbf80-0x4d1f20 (phase 3, game logic) and their neighbours: CWaitAI, CIdleAI,
// CForceUpdatingAI, CLimitedTimeAI, CHandleMessageAI, CGeneratorAI, CGoToAI (class getters, RegisterFields,
// deleting destructors, behaviour slots and helpers), the spawnable items CSpawnableItem, CSpawnableEntity,
// CSpawnableCloneByName (registration, Create, CreateArray, destructors, slot 23 "spawn"), CGameType's
// registration, constructor and destructors with the game type list's slots, and CGroundTextureFrame's
// registration. Their Create and CreateArray (except the spawnables' and the frame's) are in
// ai_classes.cpp.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CWaitAI (0x20, class 0x6bfe68): +0x10 "Time To Wait", +0x14 "Time Left", +0x18 "Done Action" (owned
//     action), +0x1c "Behavior to run" (owned CEntityBehavior).
//   CIdleAI (0x40, 0x6bfe6c, alternative name "CStandGuardPatrolAI"): +0x10 an embedded
//     CEntityBehaviorIdle ("Debug/Idle Behavior", 0x30 bytes).
//   CForceUpdatingAI (0x18, 0x6bfe70): +0x10 "AI To Force" (owned AI), +0x14 "Depricated/Updated Radius".
//   CLimitedTimeAI (0x20, 0x6bfe74): +0x10 "Max Time", +0x14 "Time Left", +0x18 "What To Do" (owned AI),
//     +0x1c "Done Action".
//   CHandleMessageAI (0x18, 0x6bfe78, parent CFlyweightLayerPartAI): +0x10 "Message To Handle" (ZString),
//     +0x14 "Action".
//   CGeneratorAI (0x48, 0x6c0450): +0x10 "When/Max Simultaneous", +0x14 "When/Delay Between Spawns", +0x18
//     "When/Warp In" (byte), +0x1c/+0x20 "What/Things to Generate" (CSpawnableItem array: data, count),
//     +0x24 "Customization/New Name", +0x28 "Customization/Remove Default AIs", +0x2c/+0x30
//     "Customization/AIs to Add", +0x34 "Customization/After Action", +0x38 "Debug/Generations Pending",
//     +0x3c "Time Till Next Generate", +0x40 "New Facing Angle", +0x44 "Angle Variation".
//   CGoToAI (0x118, 0x6c0458): +0x10 "Need to Call Start", +0x11 "Debug/Failed Path", +0x14 "Destination",
//     +0x18 "Debug/Finished", +0x19 "Debug/Target Was Set By Goto AI", +0x1a "Debug/Move Finished", +0x1c
//     the embedded CEntityBehaviorMoveToPos ("Move/Move Behavior", 0xbc bytes; its +0xa4 the speed, +0xa8
//     flags, bit 6 "no way"), +0xd8 "Facing Angle/Set Facing Angle", +0xdc the embedded
//     CEntityBehaviorTurnToAngle ("Facing Angle/Facing Angle", 0x1c bytes), +0xf8 "Starting Action", +0xfc
//     "Done action", +0x100 "Offset" (x, y), +0x108 "Make New Home", +0x10c "Play Ratio", +0x110 "Time To
//     Try To Move", +0x114 "Facing Angle/Turn Sequence Name".
//   CSpawnableItem (0xc, 0x6c0440, abstract): +8 "Weight". CSpawnableEntity (0x10, 0x6c0448): +0xc
//     "CustomEntity" (owned CEntityBase). CSpawnableCloneByName (0x10, 0x6c044c): +0xc "Name".
//   CGameType (0x18, 0x6c0430, abstract): +8 "Game Name" (a Local.txt entry: two ZStrings), +0x10 "Players
//     Initial Inventory", +0x14 "Default Player Model".
//
// The slots of CLayerPartAI [confirmed: callers and layouts; names ours]: 11 read an old line (name,
// value, a3), 24 Begin (part), 25 Update (part, seconds), 29 message (part, message; al handled), 34
// collision (five arguments), 41 debug drawing (two arguments); CGoToAI adds 42 done, 43 its drawing, 44
// check the target, 45 start the move, 46 finish, 47 start turning.
//
// Floating point follows the original instruction by instruction (x87.h). Left out: CGoToAI's drawing
// (slot 43 0x4d18b0: lines and an oval through the view; Rendering).
#include "class_info.h"
#include "ai_common.h"
#include "def_list_slots.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ----------------------------------------------------------------------------------

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;          // 0x34 (a Local.txt entry, 8 bytes)
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR = 0x419110;     // 0x48
constexpr DWORD SET_ALT_NAME = 0x419030;
DWORD set_alt_name(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(SET_ALT_NAME)(f, glob<const char *>(text)); }

CClassInfo *flyweight_ai_class() { return fn<Getter>(0x4fd390)(); }   // CFlyweightLayerPartAI
CClassInfo *entity_base_class() { return fn<Getter>(0x487620)(); }
CClassInfo *entity_destructable_class() { return fn<Getter>(0x48bdf0)(); }

// 0x4fd0b0: an AI's debug drawing (slot 41 of the AI it stands for)
void ai_draw(void *ai, DWORD a, DWORD b) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4fd0b0)(ai, a, b); }
// 0x4e72a0 / 0x4e73c0: the layer's update monitor list (the layer's save data +0x3c)
void monitor_add(BYTE *layer, void *part, DWORD w, DWORD h) { fn<void(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4e72a0)(layer + 0x3c, part, w, h); }
void monitor_remove(BYTE *layer, void *part) { fn<void(THISCALL *)(void *, void *)>(0x4e73c0)(layer + 0x3c, part); }
// 0x506620: the message (ecx) is the one of that name (al)
DWORD message_is(void *msg, const char *name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x506620)(msg, name); }
// 0x4eb1c0: a part of the layer blocking the part here (preferred class, all of, none of these flags)
BYTE *find_blocker(BYTE *layer, void *part, CClassInfo *prefer, DWORD all, DWORD none)
{
    return fn<BYTE *(THISCALL *)(void *, void *, CClassInfo *, DWORD, DWORD)>(0x4eb1c0)(layer, part, prefer, all, none);
}
BYTE *world_first_character() { return fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world()); }
// CLayerPart helpers (World)
void part_set_name(void *part, void *name) { fn<void *(THISCALL *)(void *, void *)>(0x4fbf10)(part, name); }
void part_delete_ais(void *part) { fn<void(THISCALL *)(void *)>(0x4fbd00)(part); }
void part_move_center_to(void *part, const float *p) { fn<void(THISCALL *)(void *, const float *)>(0x4fb5e0)(part, p); }
void part_add_ai(void *part, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(part, ai); }
void layer_add_part(void *layer, void *part) { fn<void(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, part); }
void *object_clone(void *o, DWORD a, DWORD b) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, a, b); }
// 0x48d030: the hit points left (st0)
long double hit_points_left(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d030)(e); }
// The temporary part lists (0x59a470/0x59a510) and the grid query by name (CSortList2D 0x57aa80)
BYTE **temp_list(BYTE **slot) { return fn<BYTE **(THISCALL *)(BYTE **)>(0x59a470)(slot); }
DWORD release_temp_list(BYTE **slot) { return fn<DWORD(THISCALL *)(BYTE **)>(0x59a510)(slot); }
void sort_list_find_name(void *grid, BYTE *list, const char *name, DWORD flags, DWORD a, DWORD b)
{
    fn<void(THISCALL *)(void *, void *, const char *, DWORD, DWORD, DWORD)>(0x57aa80)(grid, list, name, flags, a, b);
}
// 0x48b590: the entity's current CRotationSequence; 0x48feb0 / 0x48f700: the frame and sequence setters
BYTE *entity_sequence(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
void entity_set_frame(void *e, DWORD frame) { fn<void(THISCALL *)(void *, DWORD)>(0x48feb0)(e, frame); }
void entity_play_sequence(void *e, const char *name) { fn<void(THISCALL *)(void *, const char *)>(0x48f700)(e, name); }
// 0x560300: the sequence's motion between two frames (sequence; from, to, entity)
void sequence_motion(BYTE *seq, DWORD from, DWORD to, void *e) { fn<void(THISCALL *)(void *, DWORD, DWORD, void *)>(0x560300)(seq, from, to, e); }

// ---- Registration helpers -----------------------------------------------------------------------------

constexpr DWORD CREATE_NONE = 0x46b760, CREATE_ARRAY_NONE = 0x52dc80;   // the abstract classes' factories

// ---- CWaitAI ---------------------------------------------------------------------------------------------

// 0x4cbf80, 0x4cc110 (slot 2)
CClassInfo *__cdecl WaitAI_ClassInfo()
{
    return class_getter(0x6bfe68, 0x615474 /* "CWaitAI" */, 1.0f, layer_part_ai_class, 0x20, 0, 0x4cbfd0, 0x4cc010, 0x4cc170);
}
// 0x4cc170
DWORD __cdecl WaitAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x615498) /* "Time To Wait" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0x3f800000 /* 1.0 */, 0, 0,
                        0x47c34f80 /* 99999.0 */, 0);
    add_field(WaitAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61548c) /* "Time Left" */, glob_addr(0x60b000) /* "Seconds" */, 0x14, 4, 0x14, 0, 0, 0, 0, 0, 0);
    add_field(WaitAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615438) /* "Done Action" */, 0x18, 4, 0x18, action_class(), 1, class_at(fn_addr(0x56d190)) /* CSendAIDoneMessage */, 0, 0);
    add_field(WaitAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61547c) /* "Behavior to run" */, 0x1c, 4, 0x1c, class_at(fn_addr(0x494410)) /* CEntityBehavior */, 1, 0, 0, 0);
    return add_field(WaitAI_ClassInfo(), f);
}

// The CAIBase and CLayerPartAI levels of every AI's destructor
void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed1d0));
    ai_base_dtor(o);
}
template <DWORD VT> void ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    layer_part_ai_destroy(o);
}
// 0x4cc070 (slot 1)
void *THISCALL WaitAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, ai_destroy<0x5f70a0>>(self, flags); }

// 0x4cc3b0 (slot 24): the wait starts ("Time Left" = "Time To Wait"); the behaviour begins (its slot 23)
// with the part when it is an animated entity, with 0 otherwise
void THISCALL WaitAI_Begin(BYTE *self, BYTE *part)
{
    dw(self, 0x14) = dw(self, 0x10);
    if (void *b = bp(dw(self, 0x1c)))
        vcall<void>(b, 23, part && is_kind_of(part, entity_animated_class()) ? part : nullptr);
}
// The count-down of "Time Left" (+OFF): from above 0 the seconds off; reaching 0 (the unrounded
// difference, NaN too) it is 0 and the action at +ACTION runs (0x41a720 with the part, the AI, the part)
template <DWORD OFF, DWORD ACTION> void count_down(BYTE *self, BYTE *part, float dt)
{
    if (c03(fld(fl<float>(self, OFF)), cst(glob_addr(C_ZERO))))
        return;
    const long double left = fld(fl<float>(self, OFF)) - fld(dt);
    fl<float>(self, OFF) = fstp(left);
    if (!c03(left, cst(glob_addr(C_ZERO))))
        return;
    dw(self, OFF) = 0;
    if (void *a = bp(dw(self, ACTION)))
        action_execute(a, bp(dw(part, 0x48)), part, self, part, self, self + ACTION);
}
// 0x4cc400 (slot 25): the behaviour's frame (its slot 25 (seconds, the part or 0 as in Begin)), then the
// count-down with "Done Action"
void THISCALL WaitAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (void *b = bp(dw(self, 0x1c)))
        vcall<BYTE>(b, 25, dt, part && is_kind_of(part, entity_animated_class()) ? part : nullptr);
    count_down<0x14, 0x18>(self, part, dt);
}

// ---- CIdleAI ---------------------------------------------------------------------------------------------

// 0x4cc490, 0x4cc650 (slot 2)
CClassInfo *__cdecl IdleAI_ClassInfo()
{
    return class_getter(0x6bfe6c, 0x6154a8 /* "CIdleAI" */, 1.0f, layer_part_ai_class, 0x40, 0, 0x4cc4e0, 0x4cc530, 0x4cc6b0);
}
// 0x4cc6b0: "Debug/Idle Behavior" and the alternative class name "CStandGuardPatrolAI". eax: the class
// (0 when the getter's allocation failed)
DWORD __cdecl IdleAI_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60c504) /* "Debug/Idle Behavior" */, 0x10, 0x30, 0x10, 3);
    add_field(IdleAI_ClassInfo(), f);
    CClassInfo *c = IdleAI_ClassInfo();
    if (c)
        c->alt_name = glob<const char *>(0x6154b0);   // "CStandGuardPatrolAI"
    return (DWORD)(uintptr_t)c;
}
// 0x4cc7a0: the constructor (the idle behaviour 0x498d90)
BYTE *THISCALL IdleAI_Ctor(BYTE *self)
{
    layer_part_ai_ctor(self);
    fn<void *(THISCALL *)(void *)>(0x498d90)(self + 0x10);
    vtable_and_defaults(self, vt(0x5f7148));
    return self;
}
void IdleAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f7148));
    fn<void(THISCALL *)(void *)>(0x498dc0)(o + 0x10);   // CEntityBehaviorIdle's destructor
    layer_part_ai_destroy(o);
}
// 0x4cc5a0 (slot 1)
void *THISCALL IdleAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x40, IdleAI_destroy>(self, flags); }
// 0x4cc7d0 (slot 24): an animated entity begins the idle behaviour (its slot 23)
void THISCALL IdleAI_Begin(BYTE *self, BYTE *part)
{
    if (part && is_kind_of(part, entity_animated_class()))
        vcall<BYTE>(self + 0x10, 23, part);
}
// 0x4cc800 (slot 25): the behaviour's frame for an animated entity, then CLayerPartAI's slot 25 (0x46cd50)
void THISCALL IdleAI_Update(BYTE *self, BYTE *part, DWORD dt)
{
    if (part && is_kind_of(part, entity_animated_class()))
        vcall<BYTE>(self + 0x10, 25, dt, part);
    obj_on_message(self, part, dt);
}

// ---- CForceUpdatingAI ----------------------------------------------------------------------------------------

// 0x4cc840, 0x4cc9e0 (slot 2)
CClassInfo *__cdecl ForceUpdatingAI_ClassInfo()
{
    return class_getter(0x6bfe70, 0x6154c4 /* "CForceUpdatingAI" */, 1.0f, layer_part_ai_class, 0x18, 0, 0x4cc890, 0x4cc8d0, 0x4cca40);
}
// 0x4cca40
DWORD __cdecl ForceUpdatingAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6154f4) /* "AI To Force" */, 0x10, 4, 0x10, layer_part_ai_class(), 1, 0, 0, 0);
    add_field(ForceUpdatingAI_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6154d8) /* "Depricated/Updated Radius" */, 0x14, 4, 0x14, 1, 0xa, 0, 0, 0);
    return add_field(ForceUpdatingAI_ClassInfo(), f);
}
// 0x4cc940 (slot 1)
void *THISCALL ForceUpdatingAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, ai_destroy<0x5f71f0>>(self, flags); }
// 0x4ccb60 (slot 24): the part goes into its layer's update monitor list (half sizes "Updated Radius"),
// then the AI begins
void THISCALL ForceUpdatingAI_Begin(BYTE *self, BYTE *part)
{
    monitor_add(bp(dw(part, 0x48)), part, dw(self, 0x14), dw(self, 0x14));
    if (void *ai = bp(dw(self, 0x10)))
        vcall<void>(ai, 24, part);
}
// 0x4ccb90 (slot 29): "deactivate AI" (the message id 0x6c29e8) takes the part out of the monitor list;
// the message goes to the AI. al: its, 0 without one.
DWORD THISCALL ForceUpdatingAI_Message(BYTE *self, BYTE *part, BYTE *msg)
{
    if (dw(msg, 4) == *glob<DWORD *>(0x6c29e8))
        monitor_remove(bp(dw(part, 0x48)), part);
    if (void *ai = bp(dw(self, 0x10)))
        return vcall<DWORD>(ai, 29, part, msg);
    return 0;
}
// 0x4ccbe0 (slot 34), 0x4ccc10 (slot 25), 0x4ccc30 (slot 41): to the AI
void THISCALL ForceUpdatingAI_Collide(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    if (void *ai = bp(dw(self, 0x10)))
        vcall<void>(ai, 34, a, b, c, d, e);
}
void THISCALL ForceUpdatingAI_Update(BYTE *self, DWORD part, DWORD dt)
{
    if (void *ai = bp(dw(self, 0x10)))
        vcall<void>(ai, 25, part, dt);
}
void THISCALL ForceUpdatingAI_Draw(BYTE *self, DWORD a, DWORD b)
{
    if (void *ai = bp(dw(self, 0x10)))
        ai_draw(ai, a, b);
}

// ---- CLimitedTimeAI ------------------------------------------------------------------------------------------

// 0x4ccc50, 0x4ccde0 (slot 2)
CClassInfo *__cdecl LimitedTimeAI_ClassInfo()
{
    return class_getter(0x6bfe74, 0x615500 /* "CLimitedTimeAI" */, 1.0f, layer_part_ai_class, 0x20, 0, 0x4ccca0, 0x4ccce0, 0x4cce40);
}
// 0x4cce40
DWORD __cdecl LimitedTimeAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61551c) /* "What To Do" */, 0x18, 4, 0x18, layer_part_ai_class(), 1, 0, 0, 0);
    add_field(LimitedTimeAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x615510) /* "Max Time" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0x3f800000 /* 1.0 */, 0, 0,
                  0x47c34f80 /* 99999.0 */, 0);
    add_field(LimitedTimeAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61548c) /* "Time Left" */, glob_addr(0x60b000) /* "Seconds" */, 0x14, 4, 0x14, 0, 0, 0, 0, 0, 0);
    add_field(LimitedTimeAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615438) /* "Done Action" */, 0x1c, 4, 0x1c, action_class(), 1, class_at(fn_addr(0x56d190)) /* CSendAIDoneMessage */, 0, 0);
    return add_field(LimitedTimeAI_ClassInfo(), f);
}
// 0x4ccd40 (slot 1)
void *THISCALL LimitedTimeAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, ai_destroy<0x5f7298>>(self, flags); }
// 0x4cd080 (slot 24): "Time Left" = "Max Time"; the AI begins (no null test, as the exe)
void THISCALL LimitedTimeAI_Begin(BYTE *self, BYTE *part)
{
    dw(self, 0x14) = dw(self, 0x10);
    vcall<void>(bp(dw(self, 0x18)), 24, part);
}
// 0x4cd0a0 (slot 25): while time is left the AI's frame, then the count-down with "Done Action"
void THISCALL LimitedTimeAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (c03(fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO))))
        return;
    if (void *ai = bp(dw(self, 0x18)))
        vcall<void>(ai, 25, part, dt);
    const long double left = fld(fl<float>(self, 0x14)) - fld(dt);
    fl<float>(self, 0x14) = fstp(left);
    if (!c03(left, cst(glob_addr(C_ZERO))))
        return;
    dw(self, 0x14) = 0;
    if (void *a = bp(dw(self, 0x1c)))
        action_execute(a, bp(dw(part, 0x48)), part, self, part, self, self + 0x1c);
}
// 0x4cd110 (slot 34), 0x4cd150 (slot 29): to the AI while time is left. al of slot 29: the AI's, else 0.
void THISCALL LimitedTimeAI_Collide(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    void *ai = bp(dw(self, 0x18));
    if (ai && !c03(fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO))))
        vcall<void>(ai, 34, a, b, c, d, e);
}
DWORD THISCALL LimitedTimeAI_Message(BYTE *self, DWORD part, DWORD msg)
{
    void *ai = bp(dw(self, 0x18));
    if (ai && !c03(fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO))))
        return vcall<DWORD>(ai, 29, part, msg);
    return 0;
}

// ---- CHandleMessageAI -----------------------------------------------------------------------------------------

// 0x4cd180, 0x4cd370 (slot 2)
CClassInfo *__cdecl HandleMessageAI_ClassInfo()
{
    return class_getter(0x6bfe78, 0x615528 /* "CHandleMessageAI" */, 3.0f, flyweight_ai_class, 0x18, 0, 0x4cd1d0, 0x4cd220, 0x4cd3d0);
}
// 0x4cd3d0
DWORD __cdecl HandleMessageAI_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x61553c) /* "Message To Handle" */, 0x10, 4, 0x10, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(HandleMessageAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60aed0) /* "Action" */, 0x14, 4, 0x14, action_class(), 1, 0, 0, 0);
    return add_field(HandleMessageAI_ClassInfo(), f);
}
void HandleMessageAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f7340));
    zs_dtor(o + 0x10);
    vtable_and_free(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    layer_part_ai_destroy(o);
}
// 0x4cd2a0 (slot 1)
void *THISCALL HandleMessageAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, HandleMessageAI_destroy>(self, flags); }
// 0x4cd4f0 (slot 29): the message of that name (0x506620) runs "Action" (0x41a720 with the part, the AI,
// the part). al: handled.
DWORD THISCALL HandleMessageAI_Message(BYTE *self, BYTE *part, void *msg)
{
    if (!(BYTE)message_is(msg, zs_c_str(self + 0x10)))
        return 0;
    if (void *a = bp(dw(self, 0x14)))
        action_execute(a, bp(dw(part, 0x48)), part, self, part, self, self + 0x14);
    return 1;
}
// 0x4cd540 (slot 41): "Action"'s slot 25 with both arguments
void THISCALL HandleMessageAI_Draw(BYTE *self, DWORD a, DWORD b)
{
    if (void *act = bp(dw(self, 0x14)))
        vcall<void>(act, 25, a, b);
}

// ---- CGameType and the game type list --------------------------------------------------------------------------

// 0x4ce850, 0x4ce8a0 (slot 2), 0x4cec40 (slot 34 of the game type list: its element class)
CClassInfo *__cdecl GameType_ClassInfo()
{
    return class_getter(0x6c0430, 0x6156a4 /* "CGameType" */, 1.0f, getter<0x4163b0>, 0x18, 1, CREATE_NONE, CREATE_ARRAY_NONE, 0x4ce900);
}
// 0x4ce900
DWORD __cdecl GameType_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x6156e4) /* "Game Name" */, 8, 8, 8, 1, glob_addr(0x6156f0) /* "Put Name Here" */, 0);
    add_field(GameType_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6156c8) /* "Players Initial Inventory" */, 0x10, 4, 0x10, class_at(fn_addr(0x424560)) /* CInventory */, 1, 0, 0, 0);
    add_field(GameType_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6156b0) /* "Default Player Model" */, 0x14, 4, 0x14, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    return add_field(GameType_ClassInfo(), f);
}
// 0x4ceaa0: the constructor (the Local.txt entry 0x500740 at +8)
BYTE *THISCALL GameType_Ctor(BYTE *self)
{
    obj_ctor(self);
    fn<void *(THISCALL *)(void *)>(0x500740)(self + 8);
    vtable_and_defaults(self, vt(0x5f75b4));
    return self;
}
// 0x4ceb20: the destructor (eax: CObject's destructor's)
DWORD THISCALL GameType_Dtor(BYTE *self)
{
    vtable_and_free(self, vt(0x5f75b4));
    zs_dtor(self + 0xc);
    zs_dtor(self + 8);
    return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(self);
}
// 0x4cead0 (slot 1; no array path)
BYTE *THISCALL GameType_DeletingDtor(BYTE *self, DWORD flags)
{
    GameType_Dtor(self);
    if (flags & 1)
        pool_free(self, 0x18);
    return self;
}

// 0x4cebb0 (the base of the game types' slot 23, start): the game data's (application slot 97) +0x30
// object deleted (its slot 1 with 1) and cleared, then every player of the net game (the world's slot
// 1, 0x51f450 / 0x51f440) told its slot 2
void __cdecl GameType_Start()
{
    void *app = *glob<void **>(0x713f14);
    if (dw(vcall<BYTE *>(app, 97), 0x30)) {
        if (void *o = bp(dw(vcall<BYTE *>(app, 97), 0x30)))
            vcall<void *>(o, 1, 1);
        dw(vcall<BYTE *>(app, 97), 0x30) = 0;
    }
    void *net = vcall<void *>(world(), 1);
    const DWORD n = fn<DWORD(THISCALL *)(void *)>(0x51f450)(net);
    for (DWORD i = 0; i < n; i++) {
        void *p = fn<void *(THISCALL *)(void *, DWORD)>(0x51f440)(vcall<void *>(world(), 1), i);
        vcall<void>(p, 2);
    }
}
// 0x4ceba0 (CQuestGameType's slot 45): the Local.txt string "Score" (0x6c0428)
DWORD __cdecl QuestGameType_ScoreName() { return glob_addr(0x6c0428); }
// 0x4cec30 (CQuestGameType's slot 28): the object's +0x14 cleared. eax: the object.
BYTE *THISCALL QuestGameType_Slot28(void *, BYTE *o)
{
    dw(o, 0x14) = 0;
    return o;
}
// 0x4ceb60 (a CObject slot of the user interface classes, 31-36): the application's slot 14
DWORD THISCALL Object_AppSlot14(void *)
{
    void *app = *glob<void **>(0x713f14);
    return vcall<DWORD>(app, 14);
}

// The game type list (".ZGT", vtable 0x5f746c, its base 0x5f7510): 0x4ce7a0 (slot 0), 0x4cec90 (slot
// 37), 0x4ced10 (slot 35), 0x4cedc0 (slot 39); def_list_slots.h
constexpr DWORD TEXT_ZGT = 0x615708;

// ---- The spawnable items --------------------------------------------------------------------------------------

// 0x4cee40 (CSpawnableItem's getter; abstract, no fields of a parent)
CClassInfo *__cdecl SpawnableItem_ClassInfo()
{
    return class_getter(0x6c0440, 0x615714 /* "CSpawnableItem" */, 1.0f, nullptr, 0xc, 1, CREATE_NONE, CREATE_ARRAY_NONE, 0x4ceea0);
}
// 0x4ceea0
DWORD __cdecl SpawnableItem_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x615724) /* "Weight" */, 8, 4, 8, 1, 1, 0, 1, 0xf423f);
    return add_field(SpawnableItem_ClassInfo(), f);
}
// 0x4cf440 (slot 2)
CClassInfo *__cdecl SpawnableEntity_ClassInfo()
{
    return class_getter(0x6c0448, 0x615750 /* "CSpawnableEntity" */, 1.0f, SpawnableItem_ClassInfo, 0x10, 0, 0x4cf300, 0x4cf340, 0x4cf4f0);
}
// 0x4cf4f0
DWORD __cdecl SpawnableEntity_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615764) /* "CustomEntity" */, 0xc, 4, 0xc, entity_base_class(), 1, 0, 0, 0);
    return add_field(SpawnableEntity_ClassInfo(), f);
}
// 0x4cf770 (slot 2)
CClassInfo *__cdecl SpawnableCloneByName_ClassInfo()
{
    return class_getter(0x6c044c, 0x615774 /* "CSpawnableCloneByName" */, 1.0f, SpawnableItem_ClassInfo, 0x10, 0, 0x4cf600, 0x4cf650, 0x4cf820);
}
// 0x4cf820: "Name" (a name field, vtable 0x5ed924) with its help
DWORD __cdecl SpawnableCloneByName_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x60b080) /* "Name" */, 0xc, 4, 0xc, 1, 0, 0);
    add_field(SpawnableCloneByName_ClassInfo(), f);
    return set_help(last_field(SpawnableCloneByName_ClassInfo()), 0x61578c /* "The name of the source entity to be cloned." */);
}
// The CSpawnableItem level (vtable 0x5f76cc) of the constructors and destructors
void spawnable_ctor(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f76cc));
}
void SpawnableEntity_construct(BYTE *o)
{
    spawnable_ctor(o);
    vtable_and_defaults(o, vt(0x5f772c));
}
void SpawnableCloneByName_construct(BYTE *o)
{
    spawnable_ctor(o);
    zs_ctor(o + 0xc);
    vtable_and_defaults(o, vt(0x5f778c));
}
void SpawnableEntity_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f772c));
    vtable_and_free(o, vt(0x5f76cc));
    obj_dtor(o);
}
void SpawnableCloneByName_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f778c));
    zs_dtor(o + 0xc);
    vtable_and_free(o, vt(0x5f76cc));
    obj_dtor(o);
}
// 0x4cf300, 0x4cf340, 0x4cf600, 0x4cf650 (Create, CreateArray)
void *__cdecl SpawnableEntity_Create() { return create_one<0x10, SpawnableEntity_construct>(); }
void *__fastcall SpawnableEntity_CreateArray(int count, int, int) { return create_array<0x10, SpawnableEntity_construct>(count); }
void *__cdecl SpawnableCloneByName_Create() { return create_one<0x10, SpawnableCloneByName_construct>(); }
void *__fastcall SpawnableCloneByName_CreateArray(int count, int, int) { return create_array<0x10, SpawnableCloneByName_construct>(count); }
// 0x4cf3a0, 0x4cf6c0 (slot 1)
void *THISCALL SpawnableEntity_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, SpawnableEntity_destroy>(self, flags); }
void *THISCALL SpawnableCloneByName_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, SpawnableCloneByName_destroy>(self, flags); }

// 0x4cf5d0 (slot 23, (part, generator)): a copy of "CustomEntity" (Object_Clone 0, -1) told the part
// through its slot 69; 0 without one
BYTE *THISCALL SpawnableEntity_Spawn(BYTE *self, void *part, DWORD)
{
    void *e = bp(dw(self, 0xc));
    if (!e)
        return nullptr;
    BYTE *c = static_cast<BYTE *>(object_clone(e, 0, 0xffffffff));
    vcall<void>(c, 69, part);
    return c;
}
// 0x4cf9a0 (slot 23): a copy of the CEntityBase "Name" refers to (0x41ad40 in the part's layer with the
// part as a2 and the generator as a4, class CEntityBase), told the part through its slot 69
BYTE *THISCALL SpawnableCloneByName_Spawn(BYTE *self, BYTE *part, void *generator)
{
    void *layer = bp(dw(part, 0x48));
    BYTE *src = fn<BYTE *(__fastcall *)(void *, void *, void *, void *, void *, DWORD, DWORD, CClassInfo *)>(0x41ad40)(
        layer, self + 0xc, nullptr, part, generator, 0, 1, entity_base_class());
    if (!src || !is_kind_of(src, entity_base_class()))
        return nullptr;
    BYTE *c = static_cast<BYTE *>(object_clone(src, 0, 0xffffffff));
    vcall<void>(c, 69, part);
    return c;
}

// ---- CGeneratorAI ---------------------------------------------------------------------------------------------

// 0x4cfa10, 0x4cfbe0 (slot 2)
CClassInfo *__cdecl GeneratorAI_ClassInfo()
{
    return class_getter(0x6c0450, 0x6157b8 /* "CGeneratorAI" */, 2.0f, layer_part_ai_class, 0x48, 0, 0x4cfa60, 0x4cfab0, 0x4cfc40);
}
// 0x4cfc40: "New Facing Angle" and "Angle Variation" are shown in degrees (+0x3c the conversion 0x6be328,
// the units' text "Degrees")
DWORD __cdecl GeneratorAI_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6159d4) /* "When/Max Simultaneous" */, glob_addr(0x6159ec) /* "entities" */, 0x10, 4, 0x10, 1, 1, 0, 1, 0xf423f);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6159b8) /* "When/Delay Between Spawns" */, glob_addr(0x613538) /* "seconds" */, 0x14, 4, 0x14, 1, 0x41200000 /* 10.0 */, 0, 0,
                  0x497423f0 /* 999999.0 */, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6159a8) /* "When/Warp In" */, 0x18, 1, 0x18, 1, 1, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    void *element = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615994) /* "Thing to Generate" */, 0, 4, 0, SpawnableItem_ClassInfo(), 1, 0, 0, 0);
    set_alt_name(element, 0x60a900 /* "Array Item" */);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR, glob_addr(0x61597c) /* "What/Things to Generate" */, 0x20, 4, 0x20, 0x1c, 4, 0x1c, element, 1, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x615964) /* "Customization/New Name" */, 0x24, 4, 0x24, 1, 0, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615940) /* "Customization/Remove Default AIs" */, 0x28, 1, 0x28, 1, 0, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    void *element2 = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x614e3c) /* "AI" */, 0, 4, 0, class_at(fn_addr(0x4fcb00)) /* CAIBase */, 1, 0, 0, 0);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR, glob_addr(0x615924) /* "Customization/AIs to Add" */, 0x30, 4, 0x30, 0x2c, 4, 0x2c, element2, 1, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615908) /* "Customization/After Action" */, 0x34, 4, 0x34, action_class(), 1, 0, 0, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    set_help(last_field(GeneratorAI_ClassInfo()), 0x615868 /* "Action to execute after the new entity is generated. ..." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x615850) /* "Time Till Next Generate" */, glob_addr(0x613538) /* "seconds" */, 0x3c, 4, 0x3c, 0, 0, 0, 0, 0, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x615834) /* "Debug/Generations Pending" */, glob_addr(0x6159ec) /* "entities" */, 0x38, 4, 0x38, 3, 0, 0, 0, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x614558) /* "New Facing Angle" */, glob_addr(0x60ff9c) /* "Radians" */, 0x40, 4, 0x40, 1, 0, 0, 0, 0x40c90f09 /* 6.2830853 */, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    set_help(f, 0x6157d8 /* "The new entity will spawn in at \"New Facing Angle\" plus or minus ..." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6157c8) /* "Angle Variation" */, glob_addr(0x60ff9c) /* "Radians" */, 0x44, 4, 0x44, 1, 0x40490fdb /* 3.1415927 */, 0, 0,
                  0x40490fdb /* 3.1415927 */, 0);
    add_field(GeneratorAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    return set_help(f, 0x6157d8);
}
template <DWORD VT, DWORD NAME> void named_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + NAME);
    layer_part_ai_destroy(o);
}
// 0x4cfb30 (slot 1)
void *THISCALL GeneratorAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x48, named_ai_destroy<0x5f77ec, 0x24>>(self, flags); }
// 0x4d0410 (slot 24): "Time Till Next Generate" = "Delay Between Spawns". eax: the value.
DWORD THISCALL GeneratorAI_Begin(BYTE *self, DWORD)
{
    dw(self, 0x3c) = dw(self, 0x14);
    return dw(self, 0x3c);
}

// 0x4d05a0 (part, index): item i spawns an entity (its slot 23 (part, the AI)). An animated one is
// turned to a random angle in ["New Facing Angle" - "Angle Variation", + "Angle Variation"] (the game's
// generator 0x7124c0; brought into [0, 2 pi) once) through slots 103 and 104; then it gets "New Name",
// loses its AIs with "Remove Default AIs", is centred on the generator's part, told slot 39, added to
// the part's layer, gets copies of "AIs to Add" (Object_Clone 0, -1); an active entity (flag 0x800000)
// is told slot 33, an inactive one of a CSpawnableCloneByName becomes active (slot 58). "After Action"
// runs with (part, the AI, the new entity). With "Warp In" an animated entity gets the warp effect: a
// CAddEffectAction on the stack (vtable 0x5f6bcc, target "$Instigator" (the exe's function-local static
// 0x6c0438, guard 0x6c043d), effect a 0x30-byte behaviour (0x571b70) with the sequence "Warp Effect 1"
// at +0x2c made into a warp behaviour by 0x5aabe0) run through 0x41a740 at the new entity.
void THISCALL GeneratorAI_Spawn(BYTE *self, BYTE *part, DWORD index)
{
    void *item = reinterpret_cast<void **>(static_cast<uintptr_t>(dw(self, 0x1c)))[index];
    if (!item)
        return;
    BYTE *e = vcall<BYTE *>(item, 23, part, self);
    if (!e)
        return;
    if (is_kind_of(e, entity_animated_class())) {
        const DWORD hi = float_bits(fstp(fld(fl<float>(self, 0x40)) + fld(fl<float>(self, 0x44))));
        const DWORD lo = float_bits(fstp(fld(fl<float>(self, 0x40)) - fld(fl<float>(self, 0x44))));
        const long double r = random_range(GAME_RANDOM, lo, hi);
        float a = fstp(r);
        if (!c03(r, cst(glob_addr(C_TWO_PI))))
            a = fstp(fld(a) - cst(glob_addr(C_TWO_PI)));
        if (c0(fld(a), cst(glob_addr(C_ZERO))))
            a = fstp(fld(a) + cst(glob_addr(C_TWO_PI)));
        vcall<void>(e, 103, float_bits(a), 1);
        vcall<void>(e, 104, float_bits(a), 1);
    }
    part_set_name(e, self + 0x24);
    if (fl<BYTE>(self, 0x28))
        part_delete_ais(e);
    float pos[2];
    part_position(part, pos);
    part_move_center_to(e, pos);
    vcall<void>(e, 39);
    BYTE *layer = bp(dw(part, 0x48));
    layer_add_part(layer, e);
    for (DWORD i = 0; i < dw(self, 0x30); i++)
        if (void *ai = reinterpret_cast<void **>(static_cast<uintptr_t>(dw(self, 0x2c)))[i])
            part_add_ai(e, object_clone(ai, 0, 0xffffffff));
    if (dw(e, 0x1c) & 0x800000) {
        vcall<void>(e, 33);
    } else {
        CClassInfo *clone_class = SpawnableCloneByName_ClassInfo();
        if (is_kind_of(reinterpret_cast<void **>(static_cast<uintptr_t>(dw(self, 0x1c)))[index], clone_class))
            vcall<void>(e, 58, 0x800000);
    }
    if (void *a = bp(dw(self, 0x34)))
        action_execute(a, layer, part, self, e, self, self + 0x34);
    if (!fl<BYTE>(self, 0x18) || !is_kind_of(e, entity_animated_class()))
        return;
    alignas(4) BYTE effect[0x14];
    fn<void *(THISCALL *)(void *)>(0x41a6b0)(effect);   // CAction's constructor
    zs_ctor(effect + 8);
    vtable_and_defaults(effect, vt(0x5f6bcc));   // CAddEffectAction
    BYTE *b = static_cast<BYTE *>(pool_alloc(0x30));
    BYTE *warp = b ? fn<BYTE *(THISCALL *)(void *)>(0x571b70)(b) : nullptr;
    const DWORD seq = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c2788), glob<const char *>(0x6159f8), -1);
    dw(warp, 0x2c) = seq;   // no null test, as the exe
    BYTE *behaviour = fn<BYTE *(__fastcall *)(DWORD, void *, DWORD, DWORD, DWORD)>(0x5aabe0)(1, warp, 0x3f800000, 0, 0xffffffff);
    BYTE &guard = *glob<BYTE *>(0x6c043d);
    if (!(guard & 1)) {
        guard |= 1;
        fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(glob<void *>(0x6c0438), glob<const char *>(0x6142cc));   // "$Instigator"
        crt_atexit(code<void(__cdecl *)()>(0x4d08d0));
    }
    fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(effect + 8, glob<void *>(0x6c0438));
    dw(effect, 0xc) = 0;
    dw(effect, 0x10) = (DWORD)(uintptr_t)behaviour;
    fn<DWORD(THISCALL *)(void *, void *, void *, void *, void *, void *)>(0x41a740)(effect, layer, e, nullptr, e, nullptr);
    vtable_and_free(effect, vt(0x5f6bcc));
    zs_dtor(effect + 8);
    fn<void(THISCALL *)(void *)>(0x41a700)(effect);   // CAction's destructor
}
// 0x4d0520 (part): one of "Things to Generate" by weight: r in [1, sum of the weights] from the
// generator 0x712cc0, the first item whose running sum reaches r spawns (0x4d05a0); null items count 0
void THISCALL GeneratorAI_Generate(BYTE *self, BYTE *part)
{
    const DWORD n = dw(self, 0x20);
    if (!n)
        return;
    void **items = reinterpret_cast<void **>(static_cast<uintptr_t>(dw(self, 0x1c)));
    DWORD sum = 0;
    for (DWORD i = 0; i < n; i++)
        if (items[i])
            sum += dw(items[i], 8);
    if (!sum)
        return;
    const DWORD r = random_below(RANDOM3, sum) + 1;
    items = reinterpret_cast<void **>(static_cast<uintptr_t>(dw(self, 0x1c)));
    const DWORD m = dw(self, 0x20);
    DWORD acc = 0;
    for (DWORD i = 0; i < m; i++) {
        if (!items[i])
            continue;
        acc += dw(items[i], 8);
        if (r <= acc) {
            GeneratorAI_Spawn(self, part, i);
            return;
        }
    }
}
// 0x4d08e0 (part): al: "Max Simultaneous" is reached: the pending generations plus the parts of the
// layer named "New Name" (the grid's query 0x57aa80 with flag 0x800000) that are not destructable or
// have hit points left (above 0). Without a name the reporter's slot 17 says "A CGeneratorAI tried to
// generate something without giving it a name" and the answer is 1.
DWORD THISCALL GeneratorAI_Full(BYTE *self, BYTE *part)
{
    if (fn<bool(THISCALL *)(void *)>(0x4e13d0)(self + 0x24)) {
        vcall<void>(reporter(), 17, glob<const char *>(0x615a08));
        return 1;
    }
    BYTE *list = nullptr;
    temp_list(&list);
    BYTE *layer = bp(dw(part, 0x48));
    sort_list_find_name(bp(dw(layer, 0x78)), list, zs_c_str(self + 0x24), 0x800000, 0, 0);
    DWORD count = 0;
    for (DWORD i = 0; i < dw(list, 8); i++) {
        BYTE *p = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(list, 4)))[i];
        if (!is_kind_of(p, entity_destructable_class()) || !c03(hit_points_left(p), cst(glob_addr(C_ZERO))))
            count++;
    }
    const bool full = dw(self, 0x38) + count >= dw(self, 0x10);
    return (release_temp_list(&list) & ~0xffu) | (full ? 1 : 0);
}
// A generation: the part's layer's blocker here (a CEntityMovable preferred, flags 0x840000 but not
// 0x2000000) that does not let the first character through (its slot 66) makes it pending again,
// otherwise 0x4d0520. Slot 66 takes (the character, 0).
void generate_or_wait(BYTE *self, BYTE *part)
{
    BYTE *blocker = find_blocker(bp(dw(part, 0x48)), part, entity_movable_class(), 0x840000, 0x2000000);
    if (blocker && !vcall<BYTE>(blocker, 66, world_first_character(), 0))
        dw(self, 0x38)++;
    else
        GeneratorAI_Generate(self, part);
}
// 0x4d0420 (slot 25): a pending generation first; then the count-down: when "Time Till Next Generate"
// reaches 0 (unrounded) and while "Delay Between Spawns" is above 0, the time goes up by the delay and,
// unless "Max Simultaneous" is reached (0x4d08e0), one generation, until the time is above 0 again
void THISCALL GeneratorAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (dw(self, 0x38)) {
        dw(self, 0x38)--;
        generate_or_wait(self, part);
    }
    const long double t = fld(fl<float>(self, 0x3c)) - fld(dt);
    fl<float>(self, 0x3c) = fstp(t);
    if (!c03(t, cst(glob_addr(C_ZERO))))
        return;
    do {
        if (c03(fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO))))
            return;
        fl<float>(self, 0x3c) = fstp(fld(fl<float>(self, 0x14)) + fld(fl<float>(self, 0x3c)));
        if (!(BYTE)GeneratorAI_Full(self, part))
            generate_or_wait(self, part);
    } while (c03(fld(fl<float>(self, 0x3c)), cst(glob_addr(C_ZERO))));
}

// ---- CGoToAI ----------------------------------------------------------------------------------------------------

// 0x4d09a0, 0x4d0c10 (slot 2)
CClassInfo *__cdecl GoToAI_ClassInfo()
{
    return class_getter(0x6c0458, 0x615a4c /* "CGoToAI" */, 5.0f, layer_part_ai_class, 0x118, 0, 0x4d0a00, 0x4d0a60, 0x4d0c80);
}
// 0x4d0c80
DWORD __cdecl GoToAI_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x615b54) /* "Destination" */, 0x14, 4, 0x14, 1, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR, glob_addr(0x615b4c) /* "Offset" */, 0x100, 8, 0x100, 1, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615b3c) /* "Starting Action" */, 0xf8, 4, 0xf8, action_class(), 1, 0, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60c4c8) /* "Debug/Finished" */, 0x18, 1, 0x18, 3, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615b28) /* "Debug/Move Finished" */, 0x1a, 1, 0x1a, 3, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615b14) /* "Debug/Failed Path" */, 0x11, 1, 0x11, 3, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615af4) /* "Debug/Target Was Set By Goto AI" */, 0x19, 1, 0x19, 3, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60fbf4) /* "Move/Move Behavior" */, 0x1c, 0xbc, 0x1c, 1);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x615ae8) /* "Done action" */, 0xfc, 4, 0xfc, action_class(), 1, class_at(fn_addr(0x56d190)) /* CSendAIDoneMessage */, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615ad8) /* "Make New Home" */, 0x108, 1, 0x108, 1, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c4f8) /* "Play Ratio" */, 0x10c, 4, 0x10c, 0, 0, 0, 0, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x615ac4) /* "Time To Try To Move" */, 0x110, 4, 0x110, 0, 0, 0, 0, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615aa4) /* "Facing Angle/Set Facing Angle" */, 0xd8, 1, 0xd8, 1, 0, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x615a84) /* "Facing Angle/Turn Sequence Name" */, 0x114, 4, 0x114, 1, glob_addr(0x60f8d0) /* "Idle" */, 0);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x615a68) /* "Facing Angle/Facing Angle" */, 0xdc, 0x1c, 0xdc, 1);
    add_field(GoToAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x615a54) /* "Need to Call Start" */, 0x10, 1, 0x10, 0, 1, 0);
    return add_field(GoToAI_ClassInfo(), f);
}
void GoToAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f7894));
    zs_dtor(o + 0x114);
    fn<void(THISCALL *)(void *)>(0x495360)(o + 0xdc);   // CEntityBehaviorTurnToAngle's destructor
    move_to_pos_dtor(o + 0x1c);
    zs_dtor(o + 0x14);
    layer_part_ai_destroy(o);
}
// 0x4d0b00 (slot 1)
void *THISCALL GoToAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x118, GoToAI_destroy>(self, flags); }

// 0x4d1520 (slot 11): the old lines "X Offset" and "Y Offset" (atof into "Offset"), others to CObject's
// 0x417500. eax: its; after atof what atof left.
DWORD THISCALL GoToAI_ReadLine(BYTE *self, const char *name, const char *value, DWORD a3)
{
    if (!crt_stricmp(name, glob<const char *>(0x614d38))) {   // "X Offset"
        fl<float>(self, 0x100) = fstp(fn<long double(__cdecl *)(const char *)>(0x5d03de)(value));
        return 0;
    }
    if (!crt_stricmp(name, glob<const char *>(0x614d2c))) {   // "Y Offset"
        fl<float>(self, 0x104) = fstp(fn<long double(__cdecl *)(const char *)>(0x5d03de)(value));
        return 0;
    }
    return fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x417500)(self, name, value, a3);
}
// 0x4d15a0 (slot 34): touching its target (the part's slot 70) while not finished ends the walk (slot 46)
void THISCALL GoToAI_Collide(BYTE *self, BYTE *part, void *other, DWORD, DWORD, DWORD)
{
    if (!fl<BYTE>(self, 0x18) && other && other == vcall<void *>(part, 70))
        vcall<void>(self, 46, part);
}
// 0x4d15e0 (slot 29): "deactivate AI" (0x6c29e8) clears the part's target if this AI set it; "target"
// (0x6c29e0) forgets that it did; both handled (al 1), others to CLayerPartAI's 0x4fd100
DWORD THISCALL GoToAI_Message(BYTE *self, BYTE *part, BYTE *msg)
{
    const DWORD id = dw(msg, 4);
    if (id == *glob<DWORD *>(0x6c29e8)) {
        if (fl<BYTE>(self, 0x19)) {
            vcall<void>(part, 71, 0);
            fl<BYTE>(self, 0x19) = 0;
        }
        return 1;
    }
    if (id == *glob<DWORD *>(0x6c29e0)) {
        fl<BYTE>(self, 0x19) = 0;
        return 1;
    }
    return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4fd100)(self, part, msg);
}
// 0x4d1640 (slot 24): the flags reset ("Need to Call Start" 1), the move started (slot 45); the move
// behaviour begins unless it found no way (its +0xa8 bit 6)
void THISCALL GoToAI_Begin(BYTE *self, BYTE *part)
{
    fl<BYTE>(self, 0x18) = 0;
    fl<BYTE>(self, 0x1a) = 0;
    fl<BYTE>(self, 0x11) = 0;
    fl<BYTE>(self, 0x10) = 1;
    vcall<void>(self, 45, part);
    if (!((dw(self, 0xc4) >> 6) & 1))
        vcall<BYTE>(self + 0x1c, 23, part);
}
// 0x4d1680 (slot 44): without a target the move starts again
void THISCALL GoToAI_CheckTarget(BYTE *self, BYTE *part)
{
    if (!vcall<void *>(part, 70))
        vcall<void>(self, 45, part);
}
// 0x4d1ce0 (slot 42): a failed path sends the message 0x6c29d8 (a 12-byte message, vtable 0x5eeb64) to the
// part's slot 63; otherwise "Done action" runs
void THISCALL GoToAI_Done(BYTE *self, BYTE *part)
{
    if (fl<BYTE>(self, 0x11)) {
        DWORD msg[3] = {vt(0x5eeb64), *glob<DWORD *>(0x6c29d8), 0};
        vcall<void>(part, 63, msg);
        return;
    }
    if (void *a = bp(dw(self, 0xfc)))
        action_execute(a, bp(dw(part, 0x48)), part, self, part, self, self + 0xfc);
}
// 0x4d1c90 (slot 46): finished: the target this AI set cleared, "Make New Home" (the part's slot 39),
// then slot 42
void THISCALL GoToAI_Finish(BYTE *self, BYTE *part)
{
    if (fl<BYTE>(self, 0x19)) {
        vcall<void>(part, 71, 0);
        fl<BYTE>(self, 0x19) = 0;
    }
    fl<BYTE>(self, 0x18) = 1;
    fl<BYTE>(self, 0x1a) = 1;
    if (fl<BYTE>(self, 0x108))
        vcall<void>(part, 39);
    vcall<void>(self, 42, part);
}
// 0x4d1c30 (slot 47): the move is done, the turn begins; an animated entity plays "Turn Sequence Name"
// from frame 0
void THISCALL GoToAI_StartTurn(BYTE *self, BYTE *part)
{
    fl<BYTE>(self, 0x1a) = 1;
    vcall<BYTE>(self + 0xdc, 23, part);
    if (part && is_kind_of(part, entity_animated_class())) {
        entity_play_sequence(part, zs_c_str(self + 0x114));
        entity_set_frame(part, 0);
    }
}
// 0x4d1b20 (slot 45): the move to "Destination" (0x41ad40, the part as a2 and a3, the AI as caller)
// plus "Offset"; the destination becomes the part's target (slot 71). A collideable destination (flag
// 0x40000) is made "no collision" (0x2000000) meanwhile and gets its old bit back. A move speed above 0
// becomes "Play Ratio"; when the move behaviour finds no way (0x4303f0) the path failed and it is aimed
// at the point once more with speed 0 and flag 0.
void THISCALL GoToAI_StartMove(BYTE *self, BYTE *part)
{
    fl<BYTE>(self, 0x11) = 0;
    BYTE *target = find_named_part(bp(dw(part, 0x48)), self + 0x14, part, part, self, 0, 1, 0);
    if (!target)
        return;
    vcall<void>(part, 71, target);
    fl<BYTE>(self, 0x19) = 1;
    float pos[2];
    part_position(target, pos);
    pos[0] = fstp(fld(pos[0]) + fld(fl<float>(self, 0x100)));
    pos[1] = fstp(fld(pos[1]) + fld(fl<float>(self, 0x104)));
    const DWORD flags = dw(target, 0x1c);
    const DWORD saved = flags & 0x2000000;
    const bool collideable = (flags >> 18) & 1;
    if (collideable)
        vcall<void>(target, 58, 0x2000000);
    if (!c03(fld(fl<float>(self, 0xc0)), cst(glob_addr(C_ZERO))))
        dw(self, 0x10c) = dw(self, 0xc0);
    if (!(BYTE)move_to(self + 0x1c, part, pos, dw(self, 0x10c), 1)) {
        fl<BYTE>(self, 0x11) = 1;
        move_to(self + 0x1c, part, pos, 0, 0);
    }
    if (collideable) {
        vcall<void>(target, 59, 0x2000000);
        vcall<void>(target, 58, saved);
    }
}
// 0x4d16b0 (slot 25): the first frame runs "Starting Action" and nothing else. Until finished: slot 44;
// while turning, the turn behaviour's frame (done: slot 46); an animated entity then moves its frame on
// by the sequence's frames per second (its slot 26) times the seconds from its current frame (slot 84),
// with the sequence's motion between the two (0x560300); a frame not below the frame count wraps by the
// count (and becomes 0 below 0 or still not below the count). While moving, the move behaviour's frame:
// done, the turn starts with "Set Facing Angle" (slot 47) unless the path failed, else slot 46; still
// moving, "Time To Try To Move" counts down and at 0 (unrounded) is 1 again, and a stopped move behaviour
// (speed 0) starts the move again (slot 45).
void THISCALL GoToAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (fl<BYTE>(self, 0x10)) {
        if (void *a = bp(dw(self, 0xf8)))
            action_execute(a, bp(dw(part, 0x48)), part, self, part, self, self + 0xf8);
        fl<BYTE>(self, 0x10) = 0;
        return;
    }
    if (fl<BYTE>(self, 0x18))
        return;
    vcall<void>(self, 44, part);
    if (fl<BYTE>(self, 0x1a)) {
        if (!vcall<BYTE>(self + 0xdc, 25, dt, part)) {
            vcall<void>(self, 46, part);
            return;
        }
        if (!part || !is_kind_of(part, entity_animated_class()))
            return;
        const float from = fstp(vcall<long double>(part, 84));
        const long double fps = vcall<long double>(entity_sequence(part), 26);
        float to = fstp(fps * fld(dt) + fld(from));
        const float count = fstp(fild64((long long)dw(entity_sequence(part), 0x48)));
        sequence_motion(entity_sequence(part), float_bits(from), float_bits(to), part);
        if (!c0(fld(to), fld(count))) {
            const long double wrapped = fld(to) - fld(count);
            to = fstp(wrapped);
            if (c0(wrapped, cst(glob_addr(C_ZERO))) || !c0(fld(to), fld(count)))
                to = 0.0f;
        }
        entity_set_frame(part, float_bits(to));
        return;
    }
    if (!vcall<BYTE>(self + 0x1c, 25, dt, part)) {
        const BYTE failed = (BYTE)((dw(self, 0xc4) >> 6) & 1);
        fl<BYTE>(self, 0x11) = failed;
        if (!failed && fl<BYTE>(self, 0xd8))
            vcall<void>(self, 47, part);
        else
            vcall<void>(self, 46, part);
        return;
    }
    const long double t = fld(fl<float>(self, 0x110)) - fld(dt);
    fl<float>(self, 0x110) = fstp(t);
    if (!c03(t, cst(glob_addr(C_ZERO))))
        return;
    dw(self, 0x110) = 0x3f800000;
    if (c3(fld(fl<float>(self, 0xc0)), cst(glob_addr(C_ZERO))))
        vcall<void>(self, 45, part);
}
// 0x4d1af0 (slot 41): until finished its own drawing (slot 43) and the move behaviour's (its slot 28)
void THISCALL GoToAI_Draw(BYTE *self, DWORD a, DWORD b)
{
    if (fl<BYTE>(self, 0x18))
        return;
    vcall<void>(self, 43, a, b);
    vcall<void>(self + 0x1c, 28, a, b);
}

// ---- CGroundTextureFrame (Rendering's class, registration only) ---------------------------------------------------

// 0x4d1d50, 0x4d1ec0 (slot 2): parent CFrame (0x4a8760)
CClassInfo *__cdecl GroundTextureFrame_ClassInfo()
{
    return class_getter(0x6c0460, 0x615b60 /* "CGroundTextureFrame" */, 1.0f, getter<0x4a8760>, 0x4c, 0, 0x4d1da0, 0x4d1dd0, 0x4d1f20);
}
// 0x4d1f20
DWORD __cdecl GroundTextureFrame_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x610330) /* "Damage" */, glob_addr(0x615b74) /* "Hit Points per second" */, 0x44, 4, 0x44, 1, 0, 0, 0,
                        0x461c3c00 /* 9999.0 */, 0);
    add_field(GroundTextureFrame_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610318) /* "Damage Type" */, 0x40, 4, 0x40, glob_addr(0x6bd760), 1, 0, 0, 0);
    add_field(GroundTextureFrame_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6149d0) /* "Surface Type" */, 0x48, 4, 0x48, glob_addr(0x713868), 1, glob_addr(0x614170) /* "Generic" */, 0, 0);
    return add_field(GroundTextureFrame_ClassInfo(), f);
}
void GroundTextureFrame_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x4a8a90)(o);   // CFrame's constructor
    vtable_and_defaults(o, vt(0x5f7954));
}
void GroundTextureFrame_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f7954));
    fn<void(THISCALL *)(void *)>(0x4a8b00)(o);   // CFrame's destructor
}
// 0x4d1da0, 0x4d1dd0, 0x4d1e30
void *__cdecl GroundTextureFrame_Create() { return create_one<0x4c, GroundTextureFrame_construct>(); }
void *__fastcall GroundTextureFrame_CreateArray(int count, int, int) { return create_array<0x4c, GroundTextureFrame_construct>(count); }
void *THISCALL GroundTextureFrame_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x4c, GroundTextureFrame_destroy>(self, flags); }

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_tasks)
{
    const Hook list[] = {
#include "ai_tasks_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
