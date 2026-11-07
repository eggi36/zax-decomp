// The think slots of the chase and companion AIs 0x46bfe0-0x46ead0, and the AI base classes 0x4fa710-0x4fe5f0
// (phase 3, AI; docs/DECOMPILATION.md section 8): CChaseInvestigateAI and CChasePursueAI (Begin, the move
// retry, Update), CCompanionAI (Begin, Update, the target test, the spot near the leader); CAIBase (class
// getters, RegisterFields, destructor), CLayerPartAI's and CFlyweightLayerPartAI's getters and their small
// slots (the "find the AI of a class" message, the debug drawing passed through user defined class
// references, an action of a field run with the AI as the caller), CLightBeamAI (registration, destructor,
// the damage on collision), the difficulty list's deleting destructor and the light beam's last-made
// global. The classes' registration, Create and CreateArray are in ai_chase_scan.cpp and ai_classes.cpp.
//
// Layouts [confirmed: ai_chase_scan.cpp, the slots below]:
//   CChaseInvestigateAI (0x110): +0x10 a CEntityBehaviorMoveToPos (its +0xa8 flags), +0xcc a
//     CEntityBehaviorTurnToAngle, +0xe8/+0xec the point to investigate, +0xf0 the state (0 move there, 1
//     turn to it, 2 investigate (the investigate AI +0x10c), 3 chase the target, 4 turn to the target's
//     angle, 5 failed, 6 reported), +0xf4 "Count To Investigate", +0xf8 "Play Ratio", +0xfc "Next Try To
//     Move", +0x100 "Time To Investigate", +0x104 "Radius to Investigate Point", +0x108 "Run To
//     Investigate".
//   CChasePursueAI (0xd4): +0x10 the move behaviour (+0xb8 = its flags; bit 6: no way), +0xcc "Play
//     Ratio", +0xd0 "Next Try To Move".
//   CCompanionAI (0x10c): +0x10 the move behaviour, +0xcc the leader (a CEntityWalking), +0xd0 the attack
//     AI, +0xd4 "Move/Time Between Rechecks", +0xd8 the time to the next recheck, +0xdc/+0xe0 the minimum and
//     maximum distance, +0xe4 the entity (a CEntityFlying), +0xec a CEntityBehaviorStateSetClosestTarget
//     (its +0x18 the flags it looks for, +0x1c more flags).
//   CLightBeamAI (0x18, vtable 0x5f9798, class 0x6c2b50, version 2, parent CFlyweightLayerPartAI): +0x10
//     "Player Damage On Collide" (1, 0 to 100), +0x14 "Enemy Damage On Collide" (100).
//
// Slots (names ours): 24 Begin (entity), 25 Update (entity, seconds), 29 message (part, message; al
// handled), 30 a float (st0: the second argument), 33 al 0, 34 collision (part, other, three more), 41
// debug drawing. Floating point follows the original instruction by instruction (x87.h). Return values: al
// where the original sets it; otherwise void in use [assumption]. Where the original calls through a null
// pointer (CCompanionAI's Begin and Update without an entity) the reimplementation does the same.
// Left out (Rendering): the debug drawing 0x46c4b0, 0x46e990 and the view's oval, arc and arrow helpers
// 0x4fccc0-0x4fceb0; CLightBeamAI's slot 44 0x4fe6a0 (makes the beam's CLightningLayerPart).
#include "class_info.h"
#include "ai_common.h"
#include "def_list_slots.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ----------------------------------------------------------------------------------

CClassInfo *entity_base_class() { return fn<Getter>(0x487620)(); }
CClassInfo *entity_destructable_class() { return fn<Getter>(0x48bdf0)(); }
CClassInfo *entity_projectile_class() { return fn<Getter>(0x491930)(); }
CClassInfo *entity_flying_class() { return fn<Getter>(0x49bde0)(); }
BYTE *animated_or_null(BYTE *e) { return e && (is_kind_of(e, entity_animated_class()) & 0xff) ? e : nullptr; }
// 0x4fc020 (fastcall part, point; y scale): the squared distance (st0)
long double distance2(void *part, const float *p, DWORD scale) { return fn<long double(__fastcall *)(void *, const float *, DWORD)>(0x4fc020)(part, p, scale); }
// 0x4fb0f0 (part; point, direction, y scale): the angle to the point (st0)
long double angle_from(void *part, const float *p, DWORD dir, DWORD scale)
{
    return fn<long double(THISCALL *)(void *, const float *, DWORD, DWORD)>(0x4fb0f0)(part, p, dir, scale);
}
constexpr DWORD Y_STRETCH = 0x3fb39504;   // 1.403 (1 / 0.7128)
void turn_set_target(BYTE *turn, DWORD angle) { fn<void(THISCALL *)(void *, DWORD)>(0x4953a0)(turn, angle); }
// 0x4eb050 (layer; part, point, preferred class, all, none): a part blocking the point
BYTE *find_blocker(void *layer, void *part, const float *p, CClassInfo *prefer, DWORD all, DWORD none)
{
    return fn<BYTE *(THISCALL *)(void *, void *, const float *, CClassInfo *, DWORD, DWORD)>(0x4eb050)(layer, part, p, prefer, all, none);
}
constexpr DWORD FLAG_PASSABLE = 0x2000000;

// ---- CChaseInvestigateAI ---------------------------------------------------------------------------------

// 0x46c0a0 (thiscall: entity, seconds): when the retry time is up (again in 1 s), the move behaviour aims
// at the point to investigate ("Play Ratio" taken from the move behaviour's speed (+0xb4) when above 0;
// failing that without the speed and flag); a part blocking the point is made passable (0x2000000) meanwhile.
void THISCALL ChaseInvestigate_TryMove(BYTE *self, BYTE *entity, DWORD seconds)
{
    const long double next = fld(fl<float>(self, 0xfc)) - fld(raw_float(seconds));
    fl<float>(self, 0xfc) = fstp(next);
    if (!c03(next, cst(glob_addr(C_ZERO))))
        return;
    float target[2];
    target[1] = fl<float>(self, 0xec);
    dw(self, 0xfc) = 0x3f800000;
    target[0] = fl<float>(self, 0xe8);
    DWORD was = 0;
    BYTE *blocker = find_blocker(bp(dw(entity, 0x48)), entity, target, nullptr, 0x840000, FLAG_PASSABLE);
    if (blocker) {
        was = dw(blocker, 0x1c) & FLAG_PASSABLE;
        slot_set_flags(blocker, FLAG_PASSABLE);
    }
    if (!c03(fld(fl<float>(self, 0xb4)), cst(glob_addr(C_ZERO))))
        fl<float>(self, 0xf8) = fstp(fld(fl<float>(self, 0xb4)));
    BYTE *a = animated_or_null(entity);
    if (!(move_to(self + 0x10, a, target, dw(self, 0xf8), 1) & 0xff))
        move_to(self + 0x10, a, target, 0, 0);
    if (blocker) {
        slot_clear_flags(blocker, FLAG_PASSABLE);
        slot_set_flags(blocker, was);
    }
}
// 0x46bfe0 (slot 24, Begin): a walking entity runs or walks ("Run To Investigate"); the point to
// investigate is the entity's slot 72; the move behaviour begins, the first move.
void THISCALL ChaseInvestigate_Begin(BYTE *self, BYTE *entity)
{
    if (entity && (is_kind_of(entity, entity_walking_class()) & 0xff))
        dw(entity, 0x12c) = self[0x108] ? 1 : 0;
    dw(self, 0xfc) = 0;
    dw(self, 0xf0) = 0;
    const DWORD *p = vcall<DWORD *>(entity, 72);
    dw(self, 0xe8) = p[0];
    dw(self, 0xec) = p[1];
    float pos[2];
    part_position(entity, pos);
    vcall<DWORD>(self + 0x10, 23, animated_or_null(entity));
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x46c0a0)(self, entity, 0);
}
// 0x46c1c0 (slot 25, Update): the states (see the layout above).
void THISCALL ChaseInvestigate_Update(BYTE *self, BYTE *entity, DWORD seconds)
{
    BYTE *const a = animated_or_null(entity);
    BYTE *const turn = self + 0xcc;
    switch (dw(self, 0xf0)) {
    case 0:
        if (vcall<DWORD>(self + 0x10, 25, seconds, a) & 0xff) {
            const DWORD radius = dw(self, 0x104);
            const long double d2 = distance2(entity, &fl<float>(self, 0xe8), Y_STRETCH);
            if (c03(fld(raw_float(radius)) * fld(raw_float(radius)), d2))
                return;
        }
        {
            dw(self, 0xf4) = dw(self, 0x100);
            dw(self, 0xfc) = 0;
            dw(self, 0xf0) = 1;
            const float angle = wrap_two_pi(angle_from(entity, &fl<float>(self, 0xe8), 0, Y_STRETCH));
            turn_set_target(turn, float_bits(angle));
            vcall<DWORD>(turn, 23, a);
        }
        return;
    case 1:
        if (!c3(fld(fl<float>(self, 0x104)), cst(glob_addr(C_ZERO))) && (vcall<DWORD>(turn, 25, seconds, a) & 0xff))
            return;
        dw(self, 0xf0) = 2;
        if (void *ai = bp(dw(self, 0x10c)))
            vcall<DWORD>(ai, 24, entity);
        return;
    case 2: {
        const long double c = fld(fl<float>(self, 0xf4));
        const long double left = c0(c, fld(raw_float(seconds))) ? cst(glob_addr(C_ZERO)) : c - fld(raw_float(seconds));
        fl<float>(self, 0xf4) = fstp(left);
        if (c3(left, cst(glob_addr(C_ZERO)))) {
            if (entity && (is_kind_of(entity, entity_walking_class()) & 0xff))
                dw(entity, 0x12c) = 0;
            dw(self, 0xf0) = 5;
            return;
        }
        if (void *ai = bp(dw(self, 0x10c)))
            vcall<DWORD>(ai, 25, entity, seconds);
        return;
    }
    case 3:
        if (entity && (is_kind_of(entity, entity_animated_class()) & 0xff)) {
            if (vcall<DWORD>(self + 0x10, 25, seconds, entity) & 0xff)
                return;
            dw(self, 0xfc) = 0;
            dw(self, 0xf0) = 4;
            turn_set_target(turn, float_bits(fstp(fld(fl<float>(entity, 0xcc)))));
            vcall<DWORD>(turn, 23, entity);
            return;
        }
        dw(self, 0xf0) = 5;
        return;
    case 4:
        if (entity && (is_kind_of(entity, entity_animated_class()) & 0xff) && (vcall<DWORD>(turn, 25, seconds, entity) & 0xff))
            return;
        dw(self, 0xf0) = 5;
        return;
    case 5: {
        alignas(4) BYTE message[0x10];
        fn<void *(THISCALL *)(void *, DWORD)>(0x46cd00)(message, 0);   // CFailedTargetMessage without a target
        vcall<DWORD>(entity, 63, message);
        dw(self, 0xf0) = 6;
        return;
    }
    default:
        return;
    }
}

// ---- CChasePursueAI ----------------------------------------------------------------------------------------

// 0x46cb10 (thiscall: entity, seconds): when the retry time is up (again in 1 s), the move behaviour aims at
// the entity's target (slot 70) or, without one, at the entity itself; the target is made passable
// meanwhile and, when it was not before, made impassable again with its flags slot 58 called with 0.
void THISCALL ChasePursue_TryMove(BYTE *self, BYTE *entity, DWORD seconds)
{
    const long double next = fld(fl<float>(self, 0xd0)) - fld(raw_float(seconds));
    fl<float>(self, 0xd0) = fstp(next);
    if (!c03(next, cst(glob_addr(C_ZERO))))
        return;
    BYTE *target = vcall<BYTE *>(entity, 70);
    float pos[2];
    part_position(target ? vcall<BYTE *>(entity, 70) : entity, pos);
    dw(self, 0xd0) = 0x3f800000;
    DWORD was = 0;
    if (target) {
        was = dw(target, 0x1c) & FLAG_PASSABLE;
        slot_set_flags(target, FLAG_PASSABLE);
    }
    if (!c03(fld(fl<float>(self, 0xb4)), cst(glob_addr(C_ZERO))))
        fl<float>(self, 0xcc) = fstp(fld(fl<float>(self, 0xb4)));
    BYTE *a = animated_or_null(entity);
    if (!(move_to(self + 0x10, a, pos, dw(self, 0xcc), 1) & 0xff))
        move_to(self + 0x10, a, pos, 0, 0);
    if (target && !was) {
        slot_clear_flags(target, FLAG_PASSABLE);
        slot_set_flags(target, 0);
    }
}
// 0x46caa0 (slot 24, Begin): a walking entity runs; the move behaviour begins, the first move.
void THISCALL ChasePursue_Begin(BYTE *self, BYTE *entity)
{
    if (entity && (is_kind_of(entity, entity_walking_class()) & 0xff))
        dw(entity, 0x12c) = 1;
    dw(self, 0xd0) = 0;
    vcall<DWORD>(self + 0x10, 23, animated_or_null(entity));
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x46cb10)(self, entity, 0);
}
// 0x46cc40 (slot 25, Update): the move retried and updated; when it stopped without a way (flag 6 of the
// move behaviour) the entity gets a CFailedTargetMessage for its target (slot 63).
void THISCALL ChasePursue_Update(BYTE *self, BYTE *entity, DWORD seconds)
{
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x46cb10)(self, entity, seconds);
    if (vcall<DWORD>(self + 0x10, 25, seconds, animated_or_null(entity)) & 0xff)
        return;
    if (!((dw(self, 0xb8) >> 6) & 1))
        return;
    dw(self, 0xd0) = 0;
    const DWORD target = vcall<DWORD>(entity, 70);
    DWORD message[4];
    message[1] = fn<DWORD(__fastcall *)(const char *)>(0x506680)(glob<const char *>(0x60f9ec) /* "CFailedTargetMessage" */);
    reinterpret_cast<BYTE *>(message)[8] = 0;
    message[3] = target;
    message[0] = vt(0x5efbfc);
    vcall<DWORD>(entity, 63, message);
}

// ---- CCompanionAI -----------------------------------------------------------------------------------------

BYTE *flying_or_null(BYTE *e) { return e && (is_kind_of(e, entity_flying_class()) & 0xff) ? e : nullptr; }

// 0x46ead0 (thiscall: out): a spot at a random angle and a random distance between the minimum and the
// maximum from the leader (Random 0x712cc0); its y is the leader's + 1.0 (0x5efeec) as in the original,
// which computes the sine part and overwrites it. al: 1.
DWORD THISCALL Companion_PickSpot(BYTE *self, float *out)
{
    out[0] = 0;
    out[1] = 0;
    float own[2], leader[2];
    part_position(bp(dw(self, 0xe4)), own);
    part_position(bp(dw(self, 0xcc)), leader);
    const float angle = fstp(random_range(RANDOM3, 0, 0x40c90fdb /* 2 pi */));
    const long double r = random_range(RANDOM3, dw(self, 0xdc), dw(self, 0xe0));
    long double c = fld(angle), s = fld(angle);
    __asm__("fcos" : "+t"(c));
    __asm__("fsin" : "+t"(s));
    const float x = fstp(c * r);
    out[0] = x;
    out[1] = fstp(s * r);
    out[1] = *glob<float *>(0x5efeec);
    out[0] = fstp(fld(x) + fld(leader[0]));
    out[1] = fstp(fld(leader[1]) + fld(out[1]));
    return (*glob<DWORD *>(0x5efeec) & ~0xffu) | 1;
}
// 0x46e800 (slot 24, Begin): the entity (a CEntityFlying; no null test) and its leader (slot 68, a
// CEntityWalking); without a leader the AI marks itself for deletion (+8), otherwise the recheck is due,
// the move behaviour and the attack AI begin.
void THISCALL Companion_Begin(BYTE *self, BYTE *entity)
{
    BYTE *e = flying_or_null(entity);
    dw(self, 0xe4) = (DWORD)(uintptr_t)e;
    BYTE *leader = vcall<BYTE *>(e, 68);
    if (leader && !(is_kind_of(leader, entity_walking_class()) & 0xff))
        leader = nullptr;
    dw(self, 0xcc) = (DWORD)(uintptr_t)leader;
    if (!leader) {
        self[8] = 1;
        return;
    }
    dw(self, 0xd8) = 0;
    vcall<DWORD>(self + 0x10, 23, e);
    if (void *attack = bp(dw(self, 0xd0)))
        vcall<DWORD>(attack, 24, e);
}
// 0x46e890 (slot 25, Update): at each recheck (with a leader) the move behaviour aims at a new spot near the
// leader (speed 1); the move updated; the target behaviour (+0xec) looks for the closest target with the
// flags 0x6c29a8 and 0x40000 added to its own; a target the entity can see (EntityMovable_CanSee) is
// left to the attack AI.
void THISCALL Companion_Update(BYTE *self, BYTE *entity, DWORD seconds)
{
    BYTE *e = flying_or_null(entity);
    if (c03(fld(fl<float>(self, 0xd8)), cst(glob_addr(C_ZERO))) && dw(self, 0xcc)) {
        float spot[2];
        if (fn<DWORD(THISCALL *)(void *, float *)>(0x46ead0)(self, spot) & 0xff)
            move_to(self + 0x10, e, spot, 0x3f800000, 1);
        dw(self, 0xd8) = dw(self, 0xd4);
    } else {
        fl<float>(self, 0xd8) = fstp(fld(fl<float>(self, 0xd8)) - fld(raw_float(seconds)));
    }
    vcall<DWORD>(self + 0x10, 25, seconds, e);
    BYTE *const b = self + 0xec;
    dw(b, 0x1c) = dw(b, 0x1c) | 0x40000;
    dw(b, 0x18) = *glob<DWORD *>(0x6c29a8);
    vcall<DWORD>(b, 23, e);
    if (void *t = vcall<void *>(e, 70))
        if (fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x491380)(e, t, 0, 0, 2, 0) & 0xff)
            if (void *attack = bp(dw(self, 0xd0)))
                vcall<DWORD>(attack, 25, e, seconds);
}
// 0x46ea70 (slot 31: part, other, a3): a projectile of the part's own owner (slot 68) is no target (al
// 0); otherwise CLayerPartAI's answer (al 1).
DWORD THISCALL Companion_IsTarget(BYTE *self, BYTE *part, BYTE *other, DWORD a3)
{
    if (other && (is_kind_of(other, entity_projectile_class()) & 0xff)) {
        const DWORD owner = vcall<DWORD>(part, 68);
        if (vcall<DWORD>(other, 68) == owner)
            return 0;
    }
    return fn<DWORD(THISCALL *)(void *, void *, void *, DWORD)>(0x53e0d0)(self, part, other, a3);
}

// ---- CAIBase, CLayerPartAI, CFlyweightLayerPartAI --------------------------------------------------------------

constexpr DWORD CREATE_NONE = 0x46b760, CREATE_ARRAY_NONE = 0x52dc80, REGISTER_NONE = 0x58ea40;
CClassInfo *object_class() { return fn<Getter>(0x4163b0)(); }
// 0x4fcb00, 0x4fcb50
CClassInfo *__cdecl AIBase_ClassInfo()
{
    return class_getter(0x6c29f0, 0x618bf0 /* "CAIBase" */, 3.0f, object_class, 0x10, 1, CREATE_NONE, CREATE_ARRAY_NONE, 0x4fcbb0);
}
// 0x4fd220, 0x4fd2d0
CClassInfo *__cdecl LayerPartAI_ClassInfo()
{
    return class_getter(0x6c29f4, 0x618c0c /* "CLayerPartAI" */, 2.0f, AIBase_ClassInfo, 0x10, 1, CREATE_NONE, CREATE_ARRAY_NONE, REGISTER_NONE);
}
// 0x4fd390, 0x4fd490
CClassInfo *__cdecl FlyweightAI_ClassInfo()
{
    return class_getter(0x6c29f8, 0x618c1c /* "CFlyweightLayerPartAI" */, 2.0f, LayerPartAI_ClassInfo, 0x10, 1, CREATE_NONE, CREATE_ARRAY_NONE,
                        REGISTER_NONE);
}
// 0x4fcbb0: "Marked For Deletion" (+8), "Debug/Broadcaster" (+0xc, a CBroadcaster). eax: the last call's.
DWORD __cdecl AIBase_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x618bf8) /* "Marked For Deletion" */, 8, 1, 8, 0, 0, 0);
    add_field(AIBase_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x60c0dc) /* "Debug/Broadcaster" */, 0xc, 4, 0xc, class_at(fn_addr(0x467830)) /* CBroadcaster */, 0, 0, 0, 0);
    return add_field(AIBase_ClassInfo(), f);
}
// 0x4fd080: CAIBase's destructor: the broadcaster (+0xc) tells its listeners "Delete", then CObject's.
void THISCALL AIBase_Dtor(BYTE *self)
{
    dw(self, 0) = vt(0x5f9540);
    if (void *b = bp(dw(self, 0xc)))
        fn<void(THISCALL *)(void *, void *, const char *)>(0x467d40)(b, self, glob<const char *>(0x60aef8) /* "Delete" */);
    obj_free_fields(self);
    obj_dtor(self);
}
// 0x4fd0b0 (thiscall: a, b): the debug drawing (slot 41) of the AI a chain of user defined class
// references (CUserDefinedClassRef, 0x590400 the object) stands for.
void THISCALL AI_Draw(BYTE *self, DWORD a, DWORD b)
{
    BYTE *ai = self;
    while (is_kind_of(ai, fn<Getter>(0x590200)()) & 0xff)
        ai = fn<BYTE *(THISCALL *)(void *)>(0x590400)(ai);
    vcall<DWORD>(ai, 41, a, b);
}
// 0x4fd100 (slot 29: part, message): the "find the AI of a class" message (id 0x71226c, the class +0xc)
// answered by an AI of that class: +0x10 this, +8 1. al: handled.
DWORD THISCALL LayerPartAI_Message(BYTE *self, BYTE *, BYTE *message)
{
    if (dw(message, 4) == *glob<DWORD *>(0x71226c) && (is_kind_of(self, at<CClassInfo *>(dw(message, 0xc))) & 0xff)) {
        dw(message, 0x10) = (DWORD)(uintptr_t)self;
        message[8] = 1;
        return 1;
    }
    return 0;
}
// 0x4fd140 (slot 30: three arguments): the second as the float result
float THISCALL LayerPartAI_Slot30(BYTE *, DWORD, float value, DWORD) { return value; }
// 0x4fd150 (slot 33: three arguments): al 0
BYTE THISCALL LayerPartAI_Slot33(BYTE *, DWORD, DWORD, DWORD) { return 0; }
// 0x4fd160 (thiscall: layer, the action field, a3, a4): the field's action executed with the AI as the
// caller. eax: the action's.
DWORD THISCALL AI_RunAction(BYTE *self, void *layer, BYTE **field, void *a3, void *a4)
{
    return action_execute(*field, layer, a3, self, a4, self, field);
}
// 0x4fd180: CObject's empty function (0x58ea40) on the object at +0xc when there is one
void THISCALL AI_Empty(BYTE *self)
{
    if (void *o = bp(dw(self, 0xc)))
        fn<void(THISCALL *)(void *)>(REGISTER_NONE)(o);
}
// 0x4fd5a0: a block reader (vtable 0x5f95e8). eax: this.
BYTE *THISCALL Reader_Ctor(BYTE *self)
{
    fn<void *(THISCALL *)(void *)>(0x51f8d0)(self);
    dw(self, 0) = vt(0x5f95e8);
    return self;
}
// 0x4fd5c0 (ecx): the light beam last made (0x6c29fc); 0x4fd5d0: taken (cleared). eax: 0x4fd5d0's.
void __fastcall LastBeam_Set(DWORD beam) { *glob<DWORD *>(0x6c29fc) = beam; }
DWORD __cdecl LastBeam_Take()
{
    const DWORD b = *glob<DWORD *>(0x6c29fc);
    *glob<DWORD *>(0x6c29fc) = 0;
    return b;
}
// 0x4fa710 (slot 1 of the string field kind 0x5f9314): a copy of the descriptor (heap 0x34 bytes, the ZString
// kind's copy constructor, this kind's vtable). eax: the copy, 0 when operator new gives none.
BYTE *THISCALL StringKind_Clone(BYTE *self)
{
    BYTE *c = static_cast<BYTE *>(heap_alloc(0x34));
    if (!c)
        return nullptr;
    fn<void *(THISCALL *)(void *, void *)>(0x415ef0)(c, self);
    dw(c, 0) = vt(0x5f9314);
    return c;
}

// ---- CLightBeamAI ------------------------------------------------------------------------------------------

// 0x4fe290, 0x4fe460 (slot 2)
CClassInfo *__cdecl LightBeam_ClassInfo()
{
    return class_getter(0x6c2b50, 0x618fc4 /* "CLightBeamAI" */, 2.0f, FlyweightAI_ClassInfo, 0x18, 0, 0x4fe2e0, 0x4fe330, 0x4fe4c0);
}
// 0x4fe4c0. eax: the last call's.
DWORD __cdecl LightBeam_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x618fec) /* "Player Damage On Collide" */, 0x10, 4, 0x10, 1, 0x3f800000 /* 1.0 */, 0, 0,
                        0x42c80000 /* 100.0 */, 0x3f800000 /* 1.0 */);
    add_field(LightBeam_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x618fd4) /* "Enemy Damage On Collide" */, 0x14, 4, 0x14, 1, 0x42c80000 /* 100.0 */, 0, 0,
                  0x42c80000 /* 100.0 */, 0x3f800000 /* 1.0 */);
    return add_field(LightBeam_ClassInfo(), f);
}
void LightBeam_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f9798));
    vtable_and_free(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x4fe3a0 (slot 1)
void *THISCALL LightBeam_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, LightBeam_destroy>(self, flags); }
// 0x4fe5f0 (slot 34, collision: part, other, three more): a destructable other gets damage (slot 99) from
// the part (a CEntityBase, else 0; its model's +0x28 the damage type, its owner by slot 68): "Enemy Damage
// On Collide", or the player's when the other has flags and the flags 0x6c29bc | 0x6c29a0 are not 0.
void THISCALL LightBeam_Collide(BYTE *self, BYTE *part, BYTE *other, DWORD, DWORD, DWORD)
{
    if (!other || !(is_kind_of(other, entity_destructable_class()) & 0xff))
        return;
    BYTE *p = part;
    if (p && !(is_kind_of(p, entity_base_class()) & 0xff))
        p = nullptr;
    DWORD damage = dw(self, 0x14);
    if (dw(other, 0x1c) && (*glob<DWORD *>(0x6c29bc) | *glob<DWORD *>(0x6c29a0)))
        damage = dw(self, 0x10);
    const DWORD type = dw(fn<BYTE *(THISCALL *)(void *)>(0x48b290)(p), 0x28);
    const DWORD owner = vcall<DWORD>(p, 68);
    vcall<DWORD>(other, 99, damage, type, owner);
}

// ---- The difficulty list ---------------------------------------------------------------------------------------

// 0x4fe030 (slot 0 of the difficulty list 0x6c2a00, vtable 0x5f9698): the definition list's destructor
using deflist::Slots;

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_chase)
{
    const Hook list[] = {
#include "ai_chase_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
