// The AI classes of 0x42d720-0x42f8e0 (phase 3, game logic): CAmbientSoundAI (its logic: the sound
// layer's handle, the radii handed to the layer; not the sound output itself), CAttackAI (abstract),
// CApproachTargetAI (walks towards its target and stops at "Stop Distance"), CBehaviorActivity (runs a
// behaviour queue and says "AI done" when it is empty), the registration of CEntityBehaviorMoveToPos
// (the class getter and RegisterFields; its Create and CreateArray are in ai_classes.cpp), the AI message's
// deleting destructor and two helpers (Array_MoveElement 0x42e7c0, the oval angle 0x42e820). Create and
// CreateArray of the AI classes are in ai_classes.cpp.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CAmbientSoundAI (0x28, class 0x62ea7c): +0x10 "Ambient Inner Radius", +0x14 "Ambient Outer Radius"
//     (alternative name "Ambient Radius"), +0x18 "Max Distance", +0x1c "Sound" (a resource of the list
//     0x713708), +0x20 "Layer Sound Handle" (-1: none), +0x24 the layer (not a field).
//   CAttackAI (0x10, 0x62ea8c, abstract, no fields).
//   CApproachTargetAI (0xd8, 0x62ea84, parent CAttackAI): +0x10 the embedded CEntityBehaviorMoveToPos
//     ("Move Behavior", 0xbc bytes; its +0xa8 the flags), +0xcc "Stop Distance", +0xd0 "Move Revision
//     Frequency", +0xd4 "Debug/Time To Next Move Revision".
//   CBehaviorActivity (0x34, 0x62ead0): +0x10 the embedded CEntityBehaviorQueue ("Behavior Queue", 0x24).
//
// The sound layer of the level (the layer's +0x3c object, section 7; called through the exe): 0x4ee4e0
// (resource, part) adds a sound and gives its handle or -1, 0x4ee6e0 (handle, inner, inner + outer, inner
// + outer + max) sets its distances, 0x4ee650 (handle) starts it, 0x4ee610 (handle) removes it.
//
// Floating point follows the original instruction by instruction (x87.h). Left out (Rendering): the debug
// drawing slots 41 of CAmbientSoundAI (0x42dd30: the radii and the sound's name as text) and
// CApproachTargetAI (0x42e730: the move behaviour's drawing and the stop distance's oval); the frame
// class of 0x42e950-0x42f2f0 (vtable 0x5ee9c0, a CProxyFrame recording draw calls; Rendering).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

inline void *ptr_at(void *p, DWORD off) { return bp(dw(p, off)); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
constexpr DWORD SET_ALT_NAME = 0x419030;
inline DWORD set_alt_name(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(SET_ALT_NAME)(f, glob<const char *>(text)); }
constexpr DWORD BIT_FIELD_CTOR = 0x54dfc0;   // 0x34

inline long double x87_fcos(long double v)
{
    __asm__("fcos" : "+t"(v));
    return v;
}
inline long double x87_fsin(long double v)
{
    __asm__("fsin" : "+t"(v));
    return v;
}

// The level's sound layer (layer + 0x3c is not the receiver: these are CLayer methods on the layer)
inline DWORD sound_add(void *layer, DWORD sound, void *part) { return fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x4ee4e0)(layer, sound, part); }
inline void sound_set_distances(void *layer, DWORD handle, DWORD inner, float near_, float far_)
{
    fn<void(THISCALL *)(void *, DWORD, DWORD, float, float)>(0x4ee6e0)(layer, handle, inner, near_, far_);
}
inline void sound_start(void *layer, DWORD handle) { fn<void(THISCALL *)(void *, DWORD)>(0x4ee650)(layer, handle); }
inline void sound_remove(void *layer, DWORD handle) { fn<void(THISCALL *)(void *, DWORD)>(0x4ee610)(layer, handle); }

// ---- Class getters and RegisterFields -----------------------------------------------------------------------

// 0x42d610 (static copy), 0x42d7f0 (slot 2) (CAmbientSoundAI, version 1, 0x28)
CClassInfo *__cdecl AmbientSoundAI_ClassInfo()
{
    return class_getter(0x62ea7c, 0x60c284 /* "CAmbientSoundAI" */, 1.0f, layer_part_ai_class, 0x28, 0, 0x42d660, 0x42d6b0, 0x42d850);
}
// 0x42e870, 0x42e8c0 (slot 2) (CAttackAI, version 1, 0x10, abstract)
CClassInfo *__cdecl AttackAI_ClassInfo()
{
    return class_getter(0x62ea8c, 0x60c370 /* "CAttackAI" */, 1.0f, layer_part_ai_class, 0x10, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x42e0f0 (static copy), 0x42e2d0 (slot 2) (CApproachTargetAI, version 3, 0xd8)
CClassInfo *__cdecl ApproachTargetAI_ClassInfo()
{
    return class_getter(0x62ea84, 0x60c300 /* "CApproachTargetAI" */, 3.0f, getter<0x42e870>, 0xd8, 0, 0x42e150, 0x42e1a0, 0x42e340);
}
// 0x42f310, 0x42f4f0 (slot 2) (CBehaviorActivity, version 1, 0x34)
CClassInfo *__cdecl BehaviorActivity_ClassInfo()
{
    return class_getter(0x62ead0, 0x60c37c /* "CBehaviorActivity" */, 1.0f, layer_part_ai_class, 0x34, 0, 0x42f360, 0x42f3b0, 0x42f550);
}
// 0x42f6d0, 0x42f870 (slot 2) (CEntityBehaviorMoveToPos, version 17, 0xbc, parent CEntityBehaviorTurnToAngle)
CClassInfo *__cdecl EntityBehaviorMoveToPos_ClassInfo()
{
    return class_getter(0x62eae8, 0x60c3b8 /* "CEntityBehaviorMoveToPos" */, 17.0f, getter<0x494ce0>, 0xbc, 0, 0x42f730, 0x42f750, 0x42f8e0);
}

// 0x42d850. eax: the last call's.
DWORD __cdecl AmbientSoundAI_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60c2f8) /* "Sound" */, 0x1c, 4, 0x1c, glob_addr(0x713708), 1, 0, fn_addr(0x42dc60), 0);
    add_field(AmbientSoundAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x60c2e0) /* "Ambient Inner Radius" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0, fn_addr(0x42db80) /* setter */, 0,
                  0x459c4000 /* 5000.0 */, 0x3f800000 /* 1.0 */);
    add_field(AmbientSoundAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x60c2c8) /* "Ambient Outer Radius" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0, fn_addr(0x42dbc0) /* setter */, 0,
                  0x459c4000 /* 5000.0 */, 0x3f800000 /* 1.0 */);
    add_field(AmbientSoundAI_ClassInfo(), f);
    set_alt_name(last_field(AmbientSoundAI_ClassInfo()), 0x60c2b8 /* "Ambient Radius" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x60c2a8) /* "Max Distance" */, glob_addr(0x60b360) /* "Pixels" */, 0x18, 4, 0x18, 1, 0, fn_addr(0x42dc10) /* setter */, 0,
                  0x48f42400 /* 500000.0 */, 0x3f800000 /* 1.0 */);
    add_field(AmbientSoundAI_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x60c294) /* "Layer Sound Handle" */, 0x20, 4, 0x20, 0, 0xffffffff, 0, 0, 0);
    return add_field(AmbientSoundAI_ClassInfo(), f);
}
// 0x42e340. eax: the last call's.
DWORD __cdecl ApproachTargetAI_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60c360) /* "Move Behavior" */, 0x10, 0xbc, 0x10, 1);
    add_field(ApproachTargetAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c350) /* "Stop Distance" */, 0xcc, 4, 0xcc, 1, 0x43160000 /* 150.0 */, 0, 0, 0, 0);
    add_field(ApproachTargetAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c338) /* "Move Revision Frequency" */, 0xd0, 4, 0xd0, 1, 0x40000000 /* 2.0 */, 0, 0, 0, 0);
    add_field(ApproachTargetAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c314) /* "Debug/Time To Next Move Revision" */, 0xd4, 4, 0xd4, 3, 0, 0, 0, 0, 0);
    return add_field(ApproachTargetAI_ClassInfo(), f);
}
// 0x42f550. eax: the last call's.
DWORD __cdecl BehaviorActivity_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60c390) /* "Behavior Queue" */, 0x10, 0x24, 0x10, 1);
    return add_field(BehaviorActivity_ClassInfo(), f);
}
// 0x42f8e0: the idle behaviour, play ratio, the flags at +0xa8 (bits 8, 0x10, 0x20, 0x40, 0x200), "Move To
// Rate", the target normal, the path finding parameters, the three points, "Last Neg Dist",
// "NumTimesLostGound", the path checker (CPathCollideChecker, owned) and the sequence name. eax: the last
// call's.
DWORD __cdecl EntityBehaviorMoveToPos_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60c504) /* "Debug/Idle Behavior" */, 0x1c, 0x30, 0x1c, 3);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c4f8) /* "Play Ratio" */, 0xa4, 4, 0xa4, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x60c4e8) /* "Smooth Turning" */, 0xa8, 4, 0xa8, 8, 1, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x60c4d8) /* "Do Path Finding" */, 0xa8, 4, 0xa8, 0x10, 1, 1, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x60c4c8) /* "Debug/Finished" */, 0xa8, 4, 0xa8, 0x20, 3, 1, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x60c4b0) /* "Debug/Failed Last Path" */, 0xa8, 4, 0xa8, 0x40, 3, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x60c498) /* "Debug/Idle Begin Called" */, 0xa8, 4, 0xa8, 0x200, 3, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x60c488) /* "Move To Rate" */, 0x9c, 4, 0x9c, 0, 0, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c474) /* "CurTargetNormalX" */, 0xac, 4, 0xac, 0, 0, 0, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c460) /* "CurTargetNormalY" */, 0xb0, 4, 0xb0, 0, 0, 0, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60c44c) /* "Path Finding Params" */, 0x4c, 0x34, 0x4c, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR, glob_addr(0x60c43c) /* "Pos Moving From" */, 0x80, 8, 0x80, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR, glob_addr(0x60c430) /* "Final Dest" */, 0x90, 8, 0x90, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR, glob_addr(0x60c41c) /* "Cur Dest Move To" */, 0x88, 8, 0x88, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c40c) /* "Last Neg Dist" */, 0x98, 4, 0x98, 0, 0, 0, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x60c3f8) /* "NumTimesLostGound" */, 0xa0, 4, 0xa0, 0, 0, 0, 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60c3e0) /* "Path Collide Checker" */, 0xb4, 4, 0xb4, class_at(fn_addr(0x535c00)) /* CPathCollideChecker */, 0,
                  class_at(fn_addr(0x535c00)), 0, 0);
    add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x60c3d4) /* "Sequence" */, 0xb8, 4, 0xb8, 1, 0, 0);
    return add_field(EntityBehaviorMoveToPos_ClassInfo(), f);
}

// ---- Deleting destructors and constructors -------------------------------------------------------------------

void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed1d0));
    ai_base_dtor(o);
}
// 0x42e920: CAttackAI's destructor (its level, then CLayerPartAI's and CAIBase's). eax: CAIBase's.
DWORD THISCALL AttackAI_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(0x5ee910));
    vtable_and_free(o, vt(0x5ed1d0));
    return fn<DWORD(THISCALL *)(void *)>(0x4fd080)(o);
}
// CAmbientSoundAI: the sound taken out of the layer first
void ambient_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5ee7bc);
    if (dw(o, 0x20) != 0xffffffff)
        if (void *layer = ptr_at(o, 0x24)) {
            sound_remove(layer, dw(o, 0x20));
            dw(o, 0x20) = 0xffffffff;
        }
    obj_free_fields(o);
    layer_part_ai_destroy(o);
}
void approach_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ee868));
    move_to_pos_dtor(o + 0x10);
    fn<DWORD(THISCALL *)(void *)>(0x42e920)(o);
}
void behavior_activity_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5eeabc));
    fn<void(THISCALL *)(void *)>(0x586e40)(o + 0x10);   // CEntityBehaviorQueue's destructor
    layer_part_ai_destroy(o);
}
// 0x42d720, 0x42e230, 0x42f430 (slot 1)
void *THISCALL AmbientSound_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, ambient_destroy>(self, flags); }
void *THISCALL ApproachTarget_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xd8, approach_destroy>(self, flags); }
void *THISCALL BehaviorActivity_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, behavior_activity_destroy>(self, flags); }
// 0x42f680: the AI message (0xc bytes on the stack or the heap, vtable 0x5eeb64; operator delete)
void *THISCALL AIMessage_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5eeb64);
    if (flags & 1)
        heap_free(self);
    return self;
}
// 0x42f5e0: CBehaviorActivity's out-of-line constructor (the queue 0x586e10 and the defaults). eax: this.
BYTE *THISCALL BehaviorActivity_Ctor(BYTE *o)
{
    layer_part_ai_ctor(o);
    fn<void *(THISCALL *)(void *)>(0x586e10)(o + 0x10);
    vtable_and_defaults(o, vt(0x5eeabc));
    return o;
}

// ---- CAmbientSoundAI ----------------------------------------------------------------------------------------

// The distances handed to the layer: inner, inner + outer, the far end as each setter adds it
void set_distances(BYTE *self, DWORD inner, long double near_, long double far_)
{
    sound_set_distances(ptr_at(self, 0x24), dw(self, 0x20), inner, fstp(near_), fstp(far_));
}
// 0x42db80, 0x42dbc0, 0x42dc10 (the radii's setters): the value stored and, with a sound in the layer, the
// distances updated (the sums in the order the original adds them). st(0): the value.
long double THISCALL AmbientSound_SetInner(BYTE *self, float v)
{
    dw(self, 0x10) = float_bits(v);
    if (dw(self, 0x20) != 0xffffffff) {
        const long double far_ = fld(fl<float>(self, 0x18)) + fld(fl<float>(self, 0x14)) + fld(v);
        set_distances(self, float_bits(v), fld(v) + fld(fl<float>(self, 0x14)), far_);
    }
    return fld(v);
}
long double THISCALL AmbientSound_SetOuter(BYTE *self, float v)
{
    dw(self, 0x14) = float_bits(v);
    if (dw(self, 0x20) != 0xffffffff) {
        const DWORD inner = dw(self, 0x10);
        const long double far_ = fld(fl<float>(self, 0x18)) + fld(fl<float>(self, 0x10)) + fld(v);
        set_distances(self, inner, fld(raw_float(inner)) + fld(v), far_);
    }
    return fld(v);
}
long double THISCALL AmbientSound_SetMax(BYTE *self, float v)
{
    dw(self, 0x18) = float_bits(v);
    if (dw(self, 0x20) != 0xffffffff) {
        const DWORD inner = dw(self, 0x10);
        const long double far_ = fld(fl<float>(self, 0x10)) + fld(fl<float>(self, 0x14)) + fld(v);
        set_distances(self, inner, fld(raw_float(inner)) + fld(fl<float>(self, 0x14)), far_);
    }
    return fld(v);
}
// 0x42dc60 ("Sound"'s setter): a sound in the layer is removed first. eax: the value.
DWORD THISCALL AmbientSound_SetSound(BYTE *self, DWORD v)
{
    if (dw(self, 0x20) != 0xffffffff) {
        sound_remove(ptr_at(self, 0x24), dw(self, 0x20));
        dw(self, 0x20) = 0xffffffff;
    }
    dw(self, 0x1c) = v;
    return v;
}
// 0x42dc90 (slot 24): the part's layer kept and the part put into the layer's update monitor list
// (0x4e7330 on the layer's +0x3c). eax: its.
DWORD THISCALL AmbientSound_Begin(BYTE *self, BYTE *part)
{
    BYTE *layer = static_cast<BYTE *>(ptr_at(part, 0x48));
    dw(self, 0x24) = pv(layer);
    return fn<DWORD(THISCALL *)(void *, void *)>(0x4e7330)(layer + 0x3c, part);
}
// 0x42dcb0 (slot 25, part, seconds): the layer taken from the part if none is kept; with a sound and no
// handle yet the sound is added to the layer (none: "Sound" cleared), its distances set and started
void THISCALL AmbientSound_Update(BYTE *self, BYTE *part, DWORD)
{
    if (!dw(self, 0x24))
        dw(self, 0x24) = dw(part, 0x48);
    if (!dw(self, 0x1c) || dw(self, 0x20) != 0xffffffff)
        return;
    const DWORD h = sound_add(ptr_at(self, 0x24), dw(self, 0x1c), part);
    dw(self, 0x20) = h;
    if (h == 0xffffffff) {
        dw(self, 0x1c) = 0;
        return;
    }
    const DWORD inner = dw(self, 0x10);
    const long double far_ = fld(fl<float>(self, 0x18)) + fld(fl<float>(self, 0x10)) + fld(fl<float>(self, 0x14));
    set_distances(self, inner, fld(raw_float(inner)) + fld(fl<float>(self, 0x14)), far_);
    sound_start(ptr_at(self, 0x24), dw(self, 0x20));
}

// ---- CApproachTargetAI ----------------------------------------------------------------------------------------

BYTE *walking_or_null(BYTE *part) { return part && is_kind_of(part, entity_walking_class()) ? part : nullptr; }
// 0x42e580 (slot 24): the move behaviour's flags get 9, it begins with the part when that is a walking
// entity (else null), the revision time is 0. eax: the behaviour's slot 23's.
DWORD THISCALL ApproachTarget_Begin(BYTE *self, BYTE *part)
{
    BYTE *w = walking_or_null(part);
    dw(self, 0xb8) |= 9;
    const DWORD r = vcall<DWORD>(self + 0x10, 23, w);
    dw(self, 0xd4) = 0;
    return r;
}
// 0x42e5d0 (slot 25, part, seconds): the walker's target (slot 70; the walker read without a null test)
// when it is an animated entity: further than "Stop Distance" and with the revision time run out, the move
// behaviour is aimed (0x4303f0, speed 1) at the point on the line from the walker towards the target
// (the angle 0x4fb2d0 with the oval ratio, fcos/fsin) at the distance (0x508c50 of the squared distance)
// less the stop distance, the walker's +0x12c cleared and the time set to "Move Revision Frequency";
// otherwise the time counts down. Then the move behaviour's frame (its slot 25).
void THISCALL ApproachTarget_Update(BYTE *self, BYTE *part, float dt)
{
    BYTE *w = walking_or_null(part);
    BYTE *target = vcall<BYTE *>(w, 70);
    if (!target || !is_kind_of(target, entity_animated_class()))
        return;
    float p1[2], p2[2];
    part_position(w, p1);
    part_position(target, p2);
    const long double dy = fld(p1[1]) - fld(p2[1]);
    const long double dx = fld(p1[0]) - fld(p2[0]);
    const long double dist2 = dy * dy + dx * dx;
    const float dist2f = fstp(dist2);
    const long double stop = fld(fl<float>(self, 0xcc));
    if (!c03(dist2, stop * stop) && c03(fld(fl<float>(self, 0xd4)), cst(glob_addr(C_ZERO)))) {
        const float angle = fstp(fn<long double(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4fb2d0)(w, target, 0, Y_SCALE));
        const long double d = fn<long double(__stdcall *)(DWORD)>(0x508c50)(float_bits(dist2f)) - fld(fl<float>(self, 0xcc));
        float pt[2];
        pt[0] = fstp(x87_fcos(fld(angle)) * d + fld(p1[0]));
        pt[1] = fstp(x87_fsin(fld(angle)) * d + fld(p1[1]));
        move_to(self + 0x10, w, pt, 0x3f800000, 0);
        dw(w, 0x12c) = 0;
        dw(self, 0xd4) = dw(self, 0xd0);
    } else {
        fl<float>(self, 0xd4) = fstp(fld(fl<float>(self, 0xd4)) - fld(dt));
    }
    vcall<void>(self + 0x10, 25, float_bits(dt), w);
}

// ---- CBehaviorActivity ------------------------------------------------------------------------------------------

// 0x42f610 (slot 25, part, seconds): an animated entity runs the queue's frame (its slot 25); an empty
// queue (0x586fb0) sends the part the message "AI done" (0x6c29d8, a 12-byte message on the stack, its
// byte 0; the part's slot 63)
void THISCALL BehaviorActivity_Update(BYTE *self, BYTE *part, DWORD dt)
{
    if (!part || !is_kind_of(part, entity_animated_class()))
        return;
    vcall<void>(self + 0x10, 25, dt, part);
    if (!fn<bool(THISCALL *)(void *)>(0x586fb0)(self + 0x10))
        return;
    struct {
        DWORD vftable, id;
        BYTE flag;
    } m;
    m.id = *glob<DWORD *>(0x6c29d8);
    m.vftable = vt(0x5eeb64);
    m.flag = 0;
    vcall<void>(part, 63, &m);
}

// ---- Helpers ------------------------------------------------------------------------------------------------------

// 0x42e7c0 (fastcall base, element size; to, from): the element at "from" moved to "to", the ones between
// shifted (memmove 0x5d04b0); only its first dword is kept meanwhile. eax: memmove's, or "from" when both
// are the same.
DWORD __fastcall Array_MoveElement(BYTE *base, DWORD size, DWORD to, DWORD from)
{
    if (to == from)
        return from;
    const DWORD kept = *reinterpret_cast<DWORD *>(base + from * size);
    using Memmove = DWORD(__cdecl *)(void *, const void *, DWORD);
    DWORD r;
    if (from < to)
        r = fn<Memmove>(0x5d04b0)(base + from * size, base + (from + 1) * size, (to - from) * size);
    else
        r = fn<Memmove>(0x5d04b0)(base + (to + 1) * size, base + to * size, (from - to) * size);
    *reinterpret_cast<DWORD *>(base + to * size) = kept;
    return r;
}
// 0x42e820 (stdcall, an angle; called by CDischargeWeaponThisFrameAction's slot 28): the angle squeezed by
// the oval ratio: the table sine and cosine (0x508fe0) of -a, then atan2(-first · 0.712766, second)
// (0x509100). st(0).
long double __stdcall Angle_ToOval(float a)
{
    float first, second;
    fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(&first, &second, fstp(-fld(a)));
    const float x = fstp(fld(second));
    const float y = fstp(-fld(first) * cst(glob_addr(C_OVAL_RATIO)));
    return table_atan2_bits(float_bits(y), float_bits(x));
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_approach)
{
    const Hook list[] = {
#include "ai_approach_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
