// The skeleton AI of 0x573620-0x577fff reimplemented: CSkeletonAI (the state machine of most enemies: patrol,
// pursue, attack, investigate, retreat), CSkeletonBaseAI, CSkeletonTransition and its list, the actions
// that change a skeleton AI (CSetPatrolAIAction, CSetPursueAIAction, CSetAttackAIAction,
// CSetTargetTypeAction, CSetTargetAction, CCallForReinforcementsAction) and CShootEntityBehavior's getter and
// destructor: class getters (both copies), Create/CreateArray where ai_classes.cpp has none, deleting
// destructors, RegisterFields, the slots and their helpers. Left out: CSkeletonAI's debug drawing (slot 41
// 0x577030: the home and attack ovals, the state's name; Rendering) and the static initialisers of the
// three enum tables (0x574b80-0x574c00, before the hooks).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CSkeletonBaseAI (0x10, abstract, class 0x713644, vtable 0x6008e0, parent CLayerPartAI).
//  - CSkeletonAI (0xac, class 0x71363c, version 4, vtable 0x600820): +0x10 the parts that failed as targets
//    (pointer array vtable 0x5f8b8c, base 0x5f8b98, from the pool; data +0x14, count +0x18), +0x20 the
//    seconds until the oldest of them is forgotten (1.5 each), +0x24 the entity (set by Begin), +0x28
//    "Patrol/Patrol AI" (CPatrolBaseAI, default CPatrolAreaAI), +0x2c/+0x30 "Patrol Start/End Action", +0x34
//    "Respond to calls for reinforcements" (1), +0x35 "Target shooter if hit" (1), +0x38 the embedded
//    CEntityBehaviorStateSetClosestTarget "Target/Target Type" (0x20; its +0x10 = +0x48 the sight radius
//    in pixels, +0x18 = +0x50 the category bits of the targets, +0xc = +0x44 the parts to skip), +0x58
//    "Debug/Has Seen Target", +0x59 "Never Unacquire", +0x5c "Pursue/Max dist from home" (500), +0x60
//    "Pursue AI" (CPursueAI), +0x64/+0x68 its start/end actions, +0x6c "Investigate/Max distance from home"
//    (500, not read by the reimplemented slots), +0x70 "Investigate AI" (CInvestigateAI, default
//    CChaseInvestigateAI), +0x74/+0x78 its start/end actions, +0x7c "Attack/Max Dist" (200), +0x80 "Attack
//    AI" (default CNormalAttackAI), +0x84/+0x88 its start/end actions, +0x8c "Retreat/Retreat when" (40 %
//    health or less), +0x90 "Retreat Start Action", +0x94 "Debug/Has Started Retreat", +0x98 the
//    "Transition/Transition" list (vtable 0x6008d0; a heap array of CSkeletonTransition with the count
//    in front, data +0x9c, count +0xa0, capacity +0xa4), +0xa8 "Debug/State" (enum 0x7135d0: 0 "Patrol", 1
//    "Pursue", 2 "Attack", 3 "Retreat", 4 "Investigate", 5 "Any"; 5 until the first update).
//  - CSkeletonTransition (0x1c, class 0x713638, vtable 0x6007c4, no parent): +8 "Trigger" (enum 0x713620: 0
//    "First Time", 1 "Every Time" (the default), 2 "Once Per New Target"), +0xc "From State" (the states,
//    5 "Any"), +0x10 "To State" (the states and 6 "Target Acquired", 7 "Target Lost"; setter 0x574f90),
//    +0x14 "Action", +0x18 "IsDone".
//  - The actions (0x10 each, parent CAction): CSetPatrolAIAction/CSetPursueAIAction/CSetAttackAIAction (+8
//    "Target" ("$Trigger"), +0xc "Patrol AI"/"Pursue AI"/"Attack AI", an owned template),
//    CCallForReinforcementsAction (version 2: +8 "Requester Name", +0xc "Max Distance" (500)),
//    CSetTargetTypeAction (+8 "Entity Name", +0xc "Valid Targets"), CSetTargetAction (+8 "Attacker", +0xc
//    "Target to attack").
//
// The messages the skeleton AI reads (12-byte objects and larger, id at +4): 0x713df8 a part came into
// sight (+0xc the part) [assumption on its meaning], 0x6bd708 damage (+0x18 the shooter), 0x7130c0 a call
// for reinforcements (+0xc the caller, +0x10 its target), a "CFailedTargetMessage" (+0xc the part).
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_SKELETON_BASE = 0x6008e0, VT_SKELETON = 0x600820, VT_TRANSITION = 0x6007c4, VT_TRANSITION_LIST = 0x6008d0,
                VT_SHOOT_BEHAVIOR = 0x60098c, VT_SET_PATROL = 0x600524, VT_SET_PURSUE = 0x600594, VT_SET_ATTACK = 0x600604,
                VT_REINFORCE = 0x600674, VT_SET_TARGET_TYPE = 0x6006e4, VT_SET_TARGET = 0x600754;
constexpr DWORD C_DY_SCALE = 0x5ee9bc;    // 1.403 (1 / 0.7128: screen y to ground distance)
constexpr DWORD MSG_SEEN = 0x713df8, MSG_DAMAGE = 0x6bd708, MSG_REINFORCE = 0x7130c0;
enum State : DWORD { PATROL, PURSUE, ATTACK, RETREAT, INVESTIGATE, ANY, TARGET_ACQUIRED, TARGET_LOST };

// Slots of a layer part: 70 its target, 71 set the target, 73 the point it looks at (a part's centre)
BYTE *part_target(void *part) { return vcall<BYTE *>(part, 70); }
void set_target(void *part, void *target) { vcall<void>(part, 71, target); }
// The AI of a class on a part (slot 64: class, a flag)
BYTE *part_ai(void *part, CClassInfo *c, DWORD flag) { return vcall<BYTE *>(part, 64, c, flag); }
// 0x491380 (on a CEntityMovable or 0): may it see the other part (1 or 0)
DWORD can_see(void *mover, void *other) { return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x491380)(mover, other, 0, 0, 2, 0); }
void *as_movable(BYTE *e) { return is_kind_of(e, entity_movable_class()) ? e : nullptr; }
// 0x586760 (on the embedded behaviour): its angle of view, +0x14 times a constant (st0)
long double view_angle(BYTE *behavior) { return fn<long double(THISCALL *)(void *)>(0x586760)(behavior); }
// The pointer array's linear search as the exe inlines it (its test of the element before the data for
// the start index -1 is always skipped): the index, -1 if none
DWORD find_inline_dword(BYTE *data, DWORD count, DWORD v)
{
    for (DWORD i = 0; i < count; i++)
        if (dw(data, i * 4) == v)
            return i;
    return 0xffffffffu;
}

}  // namespace

// ---- CSkeletonTransition and its list ------------------------------------------------------------------------

// 0x574c10 (slot 2 and the static registration)
static CClassInfo *__cdecl SkeletonTransition_ClassInfo()
{
    return class_getter(0x713638, 0x621dc0 /* "CSkeletonTransition" */, 1.0f, nullptr, 0x1c, 0, 0x574c60, 0x574c90, 0x574d00);
}
static void SkeletonTransition_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TRANSITION));
    obj_dtor(o);
}
// 0x574af0 (slot 1)
static void *THISCALL SkeletonTransition_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, SkeletonTransition_destroy>(self, flags); }
// 0x574d00. eax: ClassInfo_AddField's.
static DWORD __cdecl SkeletonTransition_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6142d8) /* "Trigger" */, 8, 4, 8, glob_addr(0x713620), 1, glob_addr(0x621d5c) /* "Every Time" */, 0, 0);
    add_field(SkeletonTransition_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x621de8) /* "From State" */, 0xc, 4, 0xc, glob_addr(0x7135d0), 1, glob_addr(0x621d90) /* "Any" */, 0, 0);
    add_field(SkeletonTransition_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x621ddc) /* "To State" */, 0x10, 4, 0x10, glob_addr(0x713600), 1, glob_addr(0x621d90) /* "Any" */, fn_addr(0x574f90), 0);
    add_field(SkeletonTransition_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60aed0) /* "Action" */, 0x14, 4, 0x14, action_class(), 1, 0, 0, 0);
    add_field(SkeletonTransition_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621dd4) /* "IsDone" */, 0x18, 1, 0x18, 0, 0, 0);
    return add_field(SkeletonTransition_ClassInfo(), f);
}
// 0x574f90: the setter of "To State" (+0x10)
static void THISCALL SkeletonTransition_SetToState(BYTE *self, DWORD v) { dw(self, 0x10) = v; }

// 0x577b60 (the transition list's append: element): a full list grows to the larger of count + 1 and
// capacity × 1.2 (truncated); the new heap array (the count in front, CSkeletonTransition constructed)
// takes the old elements (0x522b30 copies the CObject part, then +8 to +0x18), the old array is deleted
// (slot 1 with 3). The element's +4 to +0x18 are copied into the new slot. eax: the old count.
static DWORD THISCALL TransitionList_Append(BYTE *list, const BYTE *e)
{
    DWORD need = dw(list, 8) + 1;
    if (dw(list, 0xc) < need) {
        const DWORD grown = (DWORD)(long long)(fild64((long long)dw(list, 0xc)) * cst(glob_addr(0x5ea5f4)));   // __ftol
        if (grown > need)
            need = grown;
        if (dw(list, 0xc) < need) {
            dw(list, 0xc) = need;
            BYTE *data = nullptr;
            if (DWORD *block = static_cast<DWORD *>(heap_alloc(need * 0x1c + 4))) {
                block[0] = need;
                data = reinterpret_cast<BYTE *>(block + 1);
                for (DWORD i = 0; i < need; i++) {
                    obj_ctor(data + i * 0x1c);
                    vtable_and_defaults(data + i * 0x1c, vt(VT_TRANSITION));
                }
            }
            if (dw(list, 8)) {
                for (DWORD i = 0; i < dw(list, 8); i++) {
                    BYTE *to = data + i * 0x1c;
                    const BYTE *from = bp(dw(list, 4)) + i * 0x1c;
                    fn<void(THISCALL *)(void *, const void *)>(0x522b30)(to, from);
                    dw(to, 8) = dw(const_cast<BYTE *>(from), 8);
                    dw(to, 0xc) = dw(const_cast<BYTE *>(from), 0xc);
                    dw(to, 0x10) = dw(const_cast<BYTE *>(from), 0x10);
                    dw(to, 0x14) = dw(const_cast<BYTE *>(from), 0x14);
                    to[0x18] = from[0x18];
                }
            }
            if (dw(list, 4))
                vcall<void *>(bp(dw(list, 4)), 1, 3);
            dw(list, 4) = ptr_value(data);
        }
    }
    const DWORD n = dw(list, 8);
    BYTE *slot = bp(dw(list, 4)) + n * 0x1c;
    for (DWORD off = 4; off < 0x18; off += 4)
        dw(slot, off) = dw(const_cast<BYTE *>(e), off);
    slot[0x18] = e[0x18];
    dw(list, 8) = n + 1;
    return n;
}
// 0x577cb0 (the transition list's deleting destructor): the array deleted (slot 1 with 3), the fields 0,
// the list itself freed (operator delete) with bit 0. eax: the list.
static BYTE *THISCALL TransitionList_DeletingDtor(BYTE *list, DWORD flags)
{
    dw(list, 0) = vt(VT_TRANSITION_LIST);
    if (dw(list, 4)) {
        vcall<void *>(bp(dw(list, 4)), 1, 3);
        dw(list, 4) = 0;
        dw(list, 0xc) = 0;
    }
    dw(list, 8) = 0;
    if (flags & 1)
        heap_free(list);
    return list;
}

// ---- CSkeletonBaseAI ----------------------------------------------------------------------------------------

// 0x577d00 (static registration's), 0x577d50
static CClassInfo *__cdecl SkeletonBase_ClassInfo()
{
    return class_getter(0x713644, 0x622170 /* "CSkeletonBaseAI" */, 1.0f, layer_part_ai_class, 0x10, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x577e20: the CSkeletonBaseAI level of the destructors (its vtable and fields, CLayerPartAI, CAIBase)
static void THISCALL SkeletonBase_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(VT_SKELETON_BASE));
    layer_part_ai_dtor(o);
}
// 0x577de0 (slot 1; no array path)
static BYTE *THISCALL SkeletonBase_DeletingDtor(BYTE *o, DWORD flags)
{
    vtable_and_free(o, vt(VT_SKELETON_BASE));
    layer_part_ai_dtor(o);
    if (flags & 1)
        pool_free(o, 0x10);
    return o;
}

// ---- CSkeletonAI ---------------------------------------------------------------------------------------------

// 0x574fa0 (static registration), 0x5752c0 (slot 2)
static CClassInfo *__cdecl SkeletonAI_ClassInfo()
{
    return class_getter(0x71363c, 0x621df4 /* "CSkeletonAI" */, 4.0f, getter<0x577d00>, 0xac, 0, 0x575000, 0x575070, 0x575330);
}
static void SkeletonAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SKELETON));
    dw(o, 0x98) = vt(VT_TRANSITION_LIST);
    if (dw(o, 0x9c)) {
        vcall<void *>(bp(dw(o, 0x9c)), 1, 3);
        dw(o, 0x9c) = 0;
        dw(o, 0xa4) = 0;
    }
    dw(o, 0xa0) = 0;
    fn<void(THISCALL *)(void *)>(0x5867a0)(o + 0x38);   // the embedded behaviour's destructor
    pool_array_dtor(o + 0x10, vt(0x5f8b98));
    fn<void(THISCALL *)(void *)>(0x577e20)(o);
}
// 0x575130 (slot 1)
static void *THISCALL SkeletonAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xac, SkeletonAI_destroy>(self, flags); }
// 0x575330. eax: ClassInfo_AddField's.
static DWORD __cdecl SkeletonAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x622134) /* "Patrol/Patrol AI" */, 0x28, 4, 0x28, layer_part_ai_class(), 1,
                        class_at(fn_addr(0x53b3a0)) /* CPatrolAreaAI */, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    dw(f, 0x30) = ptr_value(class_at(fn_addr(0x53bcc0))) /* CPatrolBaseAI */;
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x622118) /* "Patrol/Patrol Start Action" */, 0x2c, 4, 0x2c, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6220fc) /* "Patrol/Patrol End Action" */, 0x30, 4, 0x30, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6220d0) /* "Patrol/Respond to calls for reinforcements" */, 0x34, 1, 0x34, 1, 1, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    set_help(last_field(SkeletonAI_ClassInfo()), 0x622020 /* "Checking this box will make this character aquire a target ..." */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x622000) /* "Target/Target shooter if hit" */, 0x35, 1, 0x35, 1, 1, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621fe8) /* "Target/Never Unacquire" */, 0x59, 1, 0x59, 1, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x621fd4) /* "Target/Target Type" */, 0x38, 0x20, 0x38, 1);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621fbc) /* "Debug/Has Seen Target" */, 0x58, 1, 0x58, 3, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621fa0) /* "Pursue/Max dist from home" */, glob_addr(0x60b360) /* "Pixels" */, 0x5c, 4, 0x5c, 1,
                  0x43fa0000 /* 500.0 */, 0, 0, 0x47c34f80 /* 99999.0 */, 0x41200000 /* 10.0 */);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621f8c) /* "Pursue/Pursue AI" */, 0x60, 4, 0x60, layer_part_ai_class(), 1, class_at(fn_addr(0x46c690)), 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    dw(f, 0x30) = ptr_value(class_at(fn_addr(0x54fdc0))) /* CPursueAI */;
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621f70) /* "Pursue/Pursue Start Action" */, 0x64, 4, 0x64, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621f54) /* "Pursue/Pursue End Action" */, 0x68, 4, 0x68, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621f30) /* "Investigate/Max distance from home" */, glob_addr(0x60b360) /* "Pixels" */, 0x6c, 4, 0x6c, 1,
                  0x43fa0000 /* 500.0 */, 0, 0, 0x47c34f80 /* 99999.0 */, 0x41200000 /* 10.0 */);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621f0c) /* "Investigate/Investigate End Action" */, 0x78, 4, 0x78, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621ee4) /* "Investigate/Investigate Start Action" */, 0x74, 4, 0x74, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621ec8) /* "Investigate/Investigate AI" */, 0x70, 4, 0x70, layer_part_ai_class(), 1,
                  class_at(fn_addr(0x46b7b0)) /* CChaseInvestigateAI */, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    dw(f, 0x30) = ptr_value(class_at(fn_addr(0x4dd6d0))) /* CInvestigateAI */;
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621eb8) /* "Attack/Max Dist" */, glob_addr(0x60b360) /* "Pixels" */, 0x7c, 4, 0x7c, 1, 0x43480000 /* 200.0 */,
                  0, 0, 0x47c34f80 /* 99999.0 */, 0x40a00000 /* 5.0 */);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60fb60) /* "Attack/Attack AI" */, 0x80, 4, 0x80, layer_part_ai_class(), 1,
                  class_at(fn_addr(0x522b40)) /* CNormalAttackAI */, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    dw(f, 0x30) = ptr_value(class_at(fn_addr(0x42e870)));
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621e9c) /* "Attack/Attack Start Action" */, 0x84, 4, 0x84, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621e80) /* "Attack/Attack End Action" */, 0x88, 4, 0x88, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621e54) /* "Retreat/Retreat when" */, glob_addr(0x621e6c) /* "% Health or less" */, 0x8c, 4, 0x8c, 1,
                  0x42200000 /* 40.0 */, 0, 0, 0x42c80000 /* 100.0 */, 0x3f800000 /* 1.0 */);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x621e34) /* "Retreat/Retreat Start Action" */, 0x90, 4, 0x90, action_class(), 1, 0, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621e18) /* "Debug/Has Started Retreat" */, 0x94, 1, 0x94, 3, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60f948) /* "Debug/State" */, 0xa8, 4, 0xa8, glob_addr(0x7135d0), 3, glob_addr(0x611f90) /* "Unknown" */, 0, 0);
    add_field(SkeletonAI_ClassInfo(), f);
    f = new_field(0x44, LIST_FIELD_CTOR2, glob_addr(0x621e00) /* "Transition/Transition" */, 0xa0, 4, 0xa0, 0xa4, 4, 0xa4, 0x9c, 4, 0x9c, 0,
                  SkeletonTransition_ClassInfo(), 1, 0);
    return add_field(SkeletonAI_ClassInfo(), f);
}

// 0x577a90 (entity, from, to, instigator): every transition not done whose "From State" is `from` or "Any"
// and whose "To State" is `to` or "Any" is marked done unless its trigger is "Every Time", and runs its
// action (layer, the entity as a2, the AI as a3, the instigator (the entity when 0) as a4, the
// transition's index as the field)
static void THISCALL SkeletonAI_RunTransitions(BYTE *self, BYTE *e, DWORD from, DWORD to, BYTE *instigator)
{
    if (!instigator)
        instigator = e;
    const DWORD n = dw(self, 0xa0);
    for (DWORD i = 0; i < n; i++) {
        BYTE *t = bp(dw(self, 0x9c)) + i * 0x1c;
        if (t[0x18])
            continue;
        if (dw(t, 0xc) != from && dw(t, 0xc) != ANY)
            continue;
        if (dw(t, 0x10) != to && dw(t, 0x10) != ANY)
            continue;
        if (dw(t, 8) != 1)
            t[0x18] = 1;
        if (BYTE *action = bp(dw(bp(dw(self, 0x9c)) + i * 0x1c, 0x14)))
            action_execute(action, bp(dw(e, 0x48)), e, self, instigator, self, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
    }
}
// 0x577970 (entity, field): an action field's action (layer, the entity as a2 and a4, the AI as a3)
static void THISCALL SkeletonAI_RunAction(BYTE *self, BYTE *e, BYTE *field)
{
    if (BYTE *action = bp(dw(field, 0)))
        action_execute(action, bp(dw(e, 0x48)), e, self, e, self, field);
}
// 0x577950 (id): a message object (vtable 0x5eeb64, the id, 0). eax: this.
static BYTE *THISCALL Message_Ctor(BYTE *m, DWORD id)
{
    dw(m, 4) = id;
    dw(m, 0) = vt(0x5eeb64);
    m[8] = 0;
    return m;
}
// 0x5776d0 (entity, state): a CEntityFlying stops (slot 120). A new state ends the old one (its AI told
// "deactivate", its end action: patrol +0x30, pursue +0x68, attack +0x88, investigate +0x78) and begins the
// new one (its start action and its AI's Begin: patrol +0x2c/+0x28, pursue +0x64/+0x60, investigate
// +0x74/+0x70; attack first forgets the failed targets, +0x84/+0x80; retreat only runs +0x90); then the
// transitions from the old to the new state run with the entity's target.
static void THISCALL SkeletonAI_SetState(BYTE *self, BYTE *e, DWORD state)
{
    if (e && is_kind_of(e, entity_flying_class()))
        vcall<void>(e, 120, 0, 0);
    const DWORD old = dw(self, 0xa8);
    if (state == old)
        return;
    Message m;
    switch (old) {
    case PATROL:
    case INVESTIGATE: {
        BYTE *ai = bp(dw(self, old == PATROL ? 0x28 : 0x70));
        if (ai) {
            m = message(glob_addr(MSG_DEACTIVATE_AI));
            send_to_ai(ai, e, &m);
        }
        BYTE *field = self + (old == PATROL ? 0x30 : 0x78);
        if (dw(field, 0))
            action_execute(bp(dw(field, 0)), bp(dw(e, 0x48)), e, self, e, self, field);
        break;
    }
    case PURSUE:
    case ATTACK: {
        BYTE *ai = bp(dw(self, old == PURSUE ? 0x60 : 0x80));
        if (ai) {
            Message_Ctor(reinterpret_cast<BYTE *>(&m), *glob<DWORD *>(MSG_DEACTIVATE_AI));
            send_to_ai(ai, e, &m);
        }
        BYTE *field = self + (old == PURSUE ? 0x68 : 0x88);
        if (dw(field, 0))
            action_execute(bp(dw(field, 0)), bp(dw(e, 0x48)), e, self, e, self, field);
        break;
    }
    }
    dw(self, 0xa8) = state;
    switch (state) {
    case PATROL:
    case INVESTIGATE:
    case PURSUE: {
        const DWORD start = state == PATROL ? 0x2c : state == PURSUE ? 0x64 : 0x74;
        if (dw(self, start))
            action_execute(bp(dw(self, start)), bp(dw(e, 0x48)), e, self, e, self, self + start);
        if (BYTE *ai = bp(dw(self, state == PATROL ? 0x28 : state == PURSUE ? 0x60 : 0x70)))
            vcall<void>(ai, 24, e);
        break;
    }
    case ATTACK:
        if (dw(self, 0x14)) {
            pool_get_free(bp(dw(self, 0x14)), dw(self, 0x1c) << 2);
            dw(self, 0x14) = 0;
            dw(self, 0x1c) = 0;
        }
        dw(self, 0x18) = 0;
        dw(self, 0x20) = 0;
        fn<void(THISCALL *)(void *, void *, void *)>(0x577970)(self, e, self + 0x84);
        if (BYTE *ai = bp(dw(self, 0x80)))
            vcall<void>(ai, 24, e);
        break;
    case RETREAT:
        fn<void(THISCALL *)(void *, void *, void *)>(0x577970)(self, e, self + 0x90);
        break;
    }
    BYTE *target = part_target(e);
    fn<void(THISCALL *)(void *, void *, DWORD, DWORD, void *)>(0x577a90)(self, e, old, state, target);
}
// The transitions triggered "Once Per New Target" may run again (IsDone 0)
static void rearm_new_target_transitions(BYTE *self)
{
    const DWORD n = dw(self, 0xa0);
    for (DWORD i = 0; i < n; i++) {
        BYTE *t = bp(dw(self, 0x9c)) + i * 0x1c;
        if (dw(t, 8) == 2)
            t[0x18] = 0;
    }
}
// The squared ground distance (y scaled by 1.403) from the target's centre to the entity's, as the exe
// keeps it on the FPU; 1e12 without a target
static long double target_distance2(BYTE *e)
{
    BYTE *t = part_target(e);
    if (!t)
        return cst(glob_addr(0x6008dc));
    float tp[2], ep[2];
    part_position(t, tp);
    part_position(e, ep);
    const long double dx = fld(ep[0]) - fld(tp[0]);
    const long double dy = (fld(ep[1]) - fld(tp[1])) * cst(glob_addr(C_DY_SCALE));
    return dx * dx + dy * dy;
}
// The end of seeing a target (0x5779a0, 0x576e50, slot 43): the first time the model's +0xa4 is added at
// the entity ("Has Seen Target"); the transitions to "Target Acquired"; attack within "Attack/Max Dist",
// otherwise pursue
static void acquired(BYTE *self, BYTE *e)
{
    if (!fl<BYTE>(self, 0x58)) {
        fl<BYTE>(self, 0x58) = 1;
        if (e && is_kind_of(e, entity_base_class()))
            part_layer_add(e, dw(entity_model(e), 0xa4));
    }
    fn<void(THISCALL *)(void *, void *, DWORD, DWORD, void *)>(0x577a90)(self, e, ANY, TARGET_ACQUIRED, part_target(e));
    const long double m = fld(fl<float>(self, 0x7c));
    const float max2 = fstp(m * m);
    const long double d2 = target_distance2(e);
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, c03(d2, max2) ? ATTACK : PURSUE);
}
// 0x5779a0 (entity): a target was set (see acquired)
static void THISCALL SkeletonAI_TargetSet(BYTE *self, BYTE *e) { acquired(self, e); }
// 0x576e50 (entity, target): the entity's target becomes `target` (the "Once Per New Target" transitions
// re-armed for a new one), then as 0x5779a0
static void THISCALL SkeletonAI_SetTarget(BYTE *self, BYTE *e, BYTE *target)
{
    if (part_target(e) != target) {
        rearm_new_target_transitions(self);
        set_target(e, target);
    }
    acquired(self, e);
}
// 0x576f80 (entity, other): al 1 when the entity is further than "Pursue/Max dist from home" from where
// slot 40 puts it, or the other part (slot 43) is further than that plus "Attack/Max Dist" from that point
static DWORD THISCALL SkeletonAI_TooFar(BYTE *self, BYTE *e, BYTE *other)
{
    const long double d2 = fn<long double(THISCALL *)(void *)>(0x4faf50)(e);   // CLayerPart_SlotDistance2
    const long double r = fld(fl<float>(self, 0x5c));
    if (!c03(d2, r * r))
        return 1;
    if (!other)
        return 0;
    float a[2], b[2];
    slot_position(other, a);
    slot_position40(e, b);
    const long double dx = fld(a[0]) - fld(b[0]);
    const long double dy = (fld(a[1]) - fld(b[1])) * cst(glob_addr(C_DY_SCALE));
    const long double s = fld(fl<float>(self, 0x7c)) + fld(fl<float>(self, 0x5c));
    return c0(s * s, dx * dx + dy * dy) ? 1 : 0;
}
// 0x576c50 (slot 43: entity, candidate): a part of the target categories (+0x50) within the sight radius
// (+0x48; the ground distance) that is not too far from home and that the entity can see becomes its
// target (see acquired). al: it became the target.
static DWORD THISCALL SkeletonAI_Notice(BYTE *self, BYTE *e, BYTE *candidate)
{
    if (!candidate || candidate == e || !(dw(candidate, 0x1c) & dw(self, 0x50)))
        return 0;
    float cp[2], ep[2];
    part_position(candidate, cp);
    part_position(e, ep);
    const long double dx = fld(ep[0]) - fld(cp[0]);
    const long double dy = (fld(ep[1]) - fld(cp[1])) * cst(glob_addr(C_DY_SCALE));
    const long double d2 = dx * dx + dy * dy;
    const DWORD r = dw(self, 0x48);
    if (!c03(d2, fild64((long long)(DWORD)(r * r))))
        return 0;
    if (fn<DWORD(THISCALL *)(void *, void *, void *)>(0x576f80)(self, e, candidate) & 0xff)
        return 0;
    void *mover = e && is_kind_of(e, entity_movable_class()) ? e : nullptr;
    if (!(can_see(mover, candidate) & 0xff))
        return 0;
    if (part_target(e) != candidate) {
        rearm_new_target_transitions(self);
        set_target(e, candidate);
    }
    acquired(self, e);
    return 1;
}
// 0x576200 (slot 24, Begin: entity): "Tries To Collide" and "Collideable" set, "Stationary" cleared, no
// target; the state "Any", then "Patrol"; the entity kept (+0x24)
static void THISCALL SkeletonAI_Begin(BYTE *self, BYTE *e)
{
    slot_set_flags(e, 0x80000);
    slot_set_flags(e, 0x40000);
    slot_clear_flags(e, 0x200000);
    set_target(e, nullptr);
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, ANY);
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, PATROL);
    dw(self, 0x24) = ptr_value(e);
}
// The AI of the current state (patrol +0x28, pursue +0x60, attack +0x80, investigate +0x70), 0 for the
// others
static BYTE *state_ai(BYTE *self)
{
    switch (dw(self, 0xa8)) {
    case PATROL: return bp(dw(self, 0x28));
    case PURSUE: return bp(dw(self, 0x60));
    case ATTACK: return bp(dw(self, 0x80));
    case INVESTIGATE: return bp(dw(self, 0x70));
    }
    return nullptr;
}
// 0x576260 (slot 34: part, other, three more): the state's AI's slot 34, then slot 43 with the part and the
// other. eax: slot 43's.
static DWORD THISCALL SkeletonAI_Collide(BYTE *self, BYTE *part, BYTE *other, DWORD a3, DWORD a4, DWORD a5)
{
    if (BYTE *ai = state_ai(self))
        vcall<void>(ai, 34, part, other, a3, a4, a5);
    return vcall<DWORD>(self, 43, part, other);
}
// 0x5762e0 (slot 29: entity, message): the state's AI answers first. While patrolling: a part in sight is
// noticed (slot 43); damage makes the shooter the target ("Target shooter if hit", no target yet); a call
// for reinforcements makes the caller's target ours ("Respond to calls for reinforcements", the target of
// our categories, not too far from home). A "CFailedTargetMessage" (unless "Never Unacquire") puts the
// part on the failed list (forgotten after 1.5 s), drops the target, runs the "Target Lost" transitions and
// patrols. Damage at or below "Retreat when" % health starts the retreat action once. Not handled:
// CLayerPartAI's slot 29. al: handled.
static DWORD THISCALL SkeletonAI_Message(BYTE *self, BYTE *e, BYTE *m)
{
    BYTE handled = 0;
    bool by_ai = false;
    if (BYTE *ai = state_ai(self))
        if (send_to_ai(ai, e, reinterpret_cast<Message *>(m)) & 0xff)
            handled = 1, by_ai = true;
    if (!by_ai) {
        if (dw(self, 0xa8) == PATROL) {
            const DWORD id = dw(m, 4);
            if (id == *glob<DWORD *>(MSG_SEEN)) {
                vcall<DWORD>(self, 43, e, bp(dw(m, 0xc)));
                handled = 1;
            } else if (id == *glob<DWORD *>(MSG_DAMAGE)) {
                if (fl<BYTE>(self, 0x35) && !part_target(e)) {
                    if (BYTE *shooter = bp(dw(m, 0x18))) {
                        if (part_target(e) != shooter) {
                            rearm_new_target_transitions(self);
                            set_target(e, shooter);
                        }
                        fn<void(THISCALL *)(void *, void *)>(0x5779a0)(self, e);
                    }
                }
                handled = 1;
            } else if (id == *glob<DWORD *>(MSG_REINFORCE) && fl<BYTE>(self, 0x34)) {
                BYTE *t = bp(dw(m, 0x10));
                if (t && bp(dw(m, 0xc)) != e && (dw(t, 0x1c) & dw(self, 0x50)) &&
                    !(fn<DWORD(THISCALL *)(void *, void *, void *)>(0x576f80)(self, e, t) & 0xff)) {
                    if (part_target(e) != t) {
                        rearm_new_target_transitions(self);
                        set_target(e, t);
                    }
                    fn<void(THISCALL *)(void *, void *)>(0x5779a0)(self, e);
                }
                handled = 1;
            }
        }
        // (also after a message the patrol branch handled, as in the exe)
        if ((fn<DWORD(THISCALL *)(void *, const char *)>(0x506620)(m, glob<const char *>(0x60f9ec) /* "CFailedTargetMessage" */) & 0xff) &&
            !fl<BYTE>(self, 0x59)) {
            if (dw(m, 0xc)) {
                if (find_inline_dword(bp(dw(self, 0x14)), dw(self, 0x18), dw(m, 0xc)) == 0xffffffffu) {
                    vcall<void>(self + 0x10, 2, m + 0xc);
                    if (c03(fld(fl<float>(self, 0x20)), cst(glob_addr(C_ZERO))))
                        dw(self, 0x20) = 0x3fc00000;   // 1.5
                }
            }
            set_target(e, nullptr);
            fn<void(THISCALL *)(void *, void *, DWORD, DWORD, void *)>(0x577a90)(self, e, ANY, TARGET_LOST, nullptr);
            fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, PATROL);
        }
    }
    if (dw(m, 4) == *glob<DWORD *>(MSG_DAMAGE)) {
        BYTE *d = e && is_kind_of(e, entity_destructable_class()) ? e : nullptr;
        if (!fl<BYTE>(self, 0x94) && d) {
            const long double pct = fn<long double(THISCALL *)(void *)>(0x48d260)(d) * cst(glob_addr(0x5ee864));   // health × 100
            if (c03(pct, fld(fl<float>(self, 0x8c)))) {
                fl<BYTE>(self, 0x94) = 1;
                if (dw(self, 0x90))
                    action_execute(bp(dw(self, 0x90)), bp(dw(e, 0x48)), e, self, e, self, self + 0x90);
            }
        }
    }
    if (!handled)
        return layer_part_ai_message(self, e, m);
    return 1;
}
// 0x5765c0 (slot 0, a message: sender, text): "Delete" of an animated entity on the failed list takes it
// off (and stops listening to it); "Leaving Scope" or "Died" of the entity's target: patrol (no longer
// listening to a part that leaves); then CObject's slot 0 (its eax)
static DWORD THISCALL SkeletonAI_OnMessage(BYTE *self, BYTE *sender, const char *text)
{
    if (!crt_stricmp(text, glob<const char *>(TEXT_DELETE)) && sender && is_kind_of(sender, entity_animated_class())) {
        const DWORD n = dw(self, 0x18);
        const DWORD i = find_inline_dword(bp(dw(self, 0x14)), n, ptr_value(sender));
        if (i != 0xffffffffu) {
            if (i < n) {
                if (i + 1 < n)
                    fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(bp(dw(self, 0x14)) + i * 4, bp(dw(self, 0x14)) + (i + 1) * 4,
                                                                              (n - (i + 1)) * 4);   // memmove
                dw(self, 0x18)--;
            }
            if (is_kind_of(sender, layer_part_class()))
                remove_listener(sender, self, 0, glob_addr(TEXT_EMPTY));
        }
    }
    if (!crt_stricmp(text, glob<const char *>(0x60aee8) /* "Leaving Scope" */)) {
        if (sender == part_target(bp(dw(self, 0x24)))) {
            fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, bp(dw(self, 0x24)), PATROL);
            if (sender && is_kind_of(sender, layer_part_class())) {
                remove_listener(sender, self, 0, glob_addr(TEXT_EMPTY));
                return obj_on_message(self, sender, ptr_value(text));
            }
        }
    } else if (!crt_stricmp(text, glob<const char *>(TEXT_DIED))) {
        if (sender == part_target(bp(dw(self, 0x24))))
            fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, bp(dw(self, 0x24)), PATROL);
    }
    return obj_on_message(self, sender, ptr_value(text));
}
// The embedded behaviour searches for the closest target (its +0xc the failed list, +0x1c flag 0x40000)
static void search_target(BYTE *self, BYTE *e)
{
    BYTE *b = self + 0x38;
    dw(b, 0xc) = ptr_value(self + 0x10);
    dw(b, 0x1c) |= 0x40000;
    vcall<DWORD>(b, 23, e);
}
// The target visible and within the view angle (the embedded behaviour's, the turn to its centre)
static bool target_in_view(BYTE *self, BYTE *e, BYTE *t)
{
    if (!t || !(can_see(as_movable(e), t) & 0xff))
        return false;
    const float a = fstp(view_angle(self + 0x38));
    float p[2];
    part_position(t, p);
    const float turn = fstp(fn<long double(THISCALL *)(void *, const float *, DWORD, DWORD)>(0x4fb340)(e, p, 0, Y_SCALE));   // CLayerPart_TurnTo
    return c03(fld(raw_float(float_bits(turn) & 0x7fffffffu)), fld(a));
}
// 0x576730 (slot 25, Update: entity, seconds): an enemy entity sends the 0x5788a0 message to the reporter
// (slot 12); not a CEntityAnimated: an error. The failed list forgets its oldest part every 1.5 s. Then up to
// 99 rounds while the state changes: "Any" patrols; patrol searches for a target (the embedded behaviour)
// and notices it (slot 43), otherwise drops it (unless "Never Unacquire") and updates the patrol AI; pursue
// gives up when too far from home (patrol, "Target Lost"), investigates a target it cannot see, attacks
// within "Attack/Max Dist", otherwise updates the pursue AI; investigate pursues a target it sees within
// the view angle, searches again, updates the investigate AI and keeps the first target; attack without a
// target patrols (investigates with "Never Unacquire"; "Target Lost"), investigates a target it cannot see,
// updates the attack AI within "Attack/Max Dist", otherwise pursues a target within the sight radius and
// investigates one beyond.
static void THISCALL SkeletonAI_Update(BYTE *self, BYTE *e, float seconds)
{
    if (dw(e, 0x1c) & *glob<DWORD *>(CATEGORY_ENEMY)) {
        alignas(4) BYTE local[0x60];
        fn<void *(THISCALL *)(void *)>(0x5788a0)(local);
        vcall<void>(reporter(), 12, local);
    }
    if (!e || !is_kind_of(e, entity_animated_class())) {
        vcall<void>(reporter(), 17, glob<const char *>(0x622148) /* "CSkeletonAI used on non-CEntityAnimated" */);
        return;
    }
    if (!c03(fld(fl<float>(self, 0x20)), cst(glob_addr(C_ZERO)))) {
        const long double left = fld(fl<float>(self, 0x20)) - fld(seconds);
        fl<float>(self, 0x20) = fstp(left);
        if (c03(left, cst(glob_addr(C_ZERO)))) {
            dw(self, 0x20) = 0;
            if (dw(self, 0x18)) {
                fn<void(THISCALL *)(void *, DWORD)>(0x586fc0)(self + 0x10, 0);   // PtrArray_RemoveAt
                if (dw(self, 0x18))
                    dw(self, 0x20) = 0x3fc00000;   // 1.5
            }
        }
    }
    auto set_state = [&](DWORD s) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, s); };
    auto update_ai = [&](DWORD off) {
        if (BYTE *ai = bp(dw(self, off)))
            vcall<void>(ai, 25, e, float_bits(seconds));
    };
    for (int rounds = 99; rounds; rounds--) {
        const DWORD s = dw(self, 0xa8);
        switch (s) {
        case ANY:
            set_state(PATROL);
            break;
        case PATROL: {
            search_target(self, e);
            BYTE *t = part_target(e);
            if (vcall<DWORD>(self, 43, e, t) & 0xff) {
                float p[2];
                part_position(t, p);
                vcall<void>(e, 73, p);
            } else {
                if (!fl<BYTE>(self, 0x59))
                    set_target(e, nullptr);
                update_ai(0x28);
            }
            break;
        }
        case PURSUE: {
            BYTE *t = part_target(e);
            if ((fn<DWORD(THISCALL *)(void *, void *, void *)>(0x576f80)(self, e, t) & 0xff) && !fl<BYTE>(self, 0x59)) {
                set_state(PATROL);
                fn<void(THISCALL *)(void *, void *, DWORD, DWORD, void *)>(0x577a90)(self, e, ANY, TARGET_LOST, nullptr);
                break;
            }
            if (!t || !(can_see(as_movable(e), t) & 0xff)) {
                set_state(INVESTIGATE);
                break;
            }
            float tp[2], ep[2];
            part_position(t, tp);
            vcall<void>(e, 73, tp);
            part_position(e, ep);
            const float dx = fstp(fld(ep[0]) - fld(tp[0]));
            const long double dy = (fld(ep[1]) - fld(tp[1])) * cst(glob_addr(C_DY_SCALE));
            const long double m = fld(fl<float>(self, 0x7c));
            if (!c0(m * m, dy * dy + fld(dx) * fld(dx)))
                set_state(ATTACK);
            else
                update_ai(0x60);
            break;
        }
        case INVESTIGATE: {
            BYTE *first = part_target(e);
            if (target_in_view(self, e, first)) {
                set_state(PURSUE);
                break;
            }
            if (!fl<BYTE>(self, 0x59))
                search_target(self, e);
            if (target_in_view(self, e, part_target(e))) {
                set_state(PURSUE);
                break;
            }
            update_ai(0x70);
            set_target(e, first);
            break;
        }
        case ATTACK: {
            BYTE *t = part_target(e);
            if (!t) {
                if (!fl<BYTE>(self, 0x59))
                    set_target(e, nullptr);
                fn<void(THISCALL *)(void *, void *, DWORD, DWORD, void *)>(0x577a90)(self, e, ANY, TARGET_LOST, nullptr);
                set_state(fl<BYTE>(self, 0x59) ? INVESTIGATE : PATROL);
                break;
            }
            float tp[2];
            part_position(t, tp);
            vcall<void>(e, 73, tp);
            float d2;
            if (BYTE *t2 = part_target(e)) {
                float p2[2], ep[2];
                part_position(t2, p2);
                part_position(e, ep);
                const float dx = fstp(fld(ep[0]) - fld(p2[0]));
                const float dy = fstp((fld(ep[1]) - fld(p2[1])) * cst(glob_addr(C_DY_SCALE)));
                d2 = fstp(fld(dx) * fld(dx) + fld(dy) * fld(dy));
            } else {
                d2 = raw_float(0x5368d4a5);   // 1e12
            }
            if (!(can_see(as_movable(e), t) & 0xff)) {
                set_state(INVESTIGATE);
                break;
            }
            const long double m = fld(fl<float>(self, 0x7c));
            if (c03(fld(d2), m * m)) {
                update_ai(0x80);
            } else {
                const DWORD r = dw(self, 0x48);
                if (c0(fild64((long long)(DWORD)(r * r)), fld(d2)))
                    set_state(INVESTIGATE);
                else
                    set_state(PURSUE);
            }
            break;
        }
        }
        if (dw(self, 0xa8) == s)
            break;
    }
}
// 0x577b30 (slot 42: -, radius out, angle out): the sight radius (+0x48) as a float and the view angle
static void THISCALL SkeletonAI_SightOf(BYTE *self, DWORD, float *radius, float *angle)
{
    *radius = fstp(fild64((long long)dw(self, 0x48)));
    *angle = fstp(view_angle(self + 0x38));
}
// 0x5775e0 / 0x577620 / 0x577660 (entity, AI): a new patrol / pursue / attack AI (the old one deleted);
// in that state the state is set to "Any" first, so the next update begins it again
template <DWORD OFF, DWORD STATE> static void THISCALL SkeletonAI_SetAI(BYTE *self, BYTE *e, BYTE *ai)
{
    if (BYTE *old = bp(dw(self, OFF))) {
        vcall<void *>(old, 1, 1);
        dw(self, OFF) = 0;
    }
    dw(self, OFF) = ptr_value(ai);
    if (dw(self, 0xa8) == STATE)
        fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, ANY);
}
// 0x5776b0 (entity, categories): the target categories (+0x50), then the state "Any"
static void THISCALL SkeletonAI_SetTargetTypes(BYTE *self, BYTE *e, DWORD mask)
{
    dw(self, 0x50) = mask;
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776d0)(self, e, ANY);
}

// ---- CShootEntityBehavior -------------------------------------------------------------------------------------

// 0x577e50 (static registration), 0x577fc0 (slot 2)
static CClassInfo *__cdecl ShootEntityBehavior_ClassInfo()
{
    return class_getter(0x713668, 0x622180 /* "CShootEntityBehavior" */, 1.0f, getter<0x494410>, 0x14, 0, 0x577ea0, 0x577ee0, 0x578020);
}
static void ShootEntityBehavior_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SHOOT_BEHAVIOR));
    fn<void(THISCALL *)(void *)>(0x4944e0)(o);   // CEntityBehavior's destructor
}
// 0x577f40 (slot 1)
static void *THISCALL ShootEntityBehavior_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, ShootEntityBehavior_destroy>(self, flags); }

// ---- The actions ---------------------------------------------------------------------------------------------

// 0x573620 / 0x5737c0, 0x5739a0 / 0x573b40, 0x573d20 / 0x573ec0 (static registration / slot 2)
static CClassInfo *__cdecl SetPatrolAI_ClassInfo()
{
    return class_getter(0x713580, 0x621b44 /* "CSetPatrolAIAction" */, 1.0f, action_class, 0x10, 0, 0x573670, 0x5736b0, 0x573820);
}
static CClassInfo *__cdecl SetPursueAI_ClassInfo()
{
    return class_getter(0x713584, 0x621b64 /* "CSetPursueAIAction" */, 1.0f, action_class, 0x10, 0, 0x5739f0, 0x573a30, 0x573ba0);
}
static CClassInfo *__cdecl SetAttackAI_ClassInfo()
{
    return class_getter(0x713588, 0x621b84 /* "CSetAttackAIAction" */, 1.0f, action_class, 0x10, 0, 0x573d70, 0x573db0, 0x573f20);
}
// 0x5740a0 / 0x574240, 0x5743f0 / 0x574590, 0x574750 / 0x574900
static CClassInfo *__cdecl CallForReinforcements_ClassInfo()
{
    return class_getter(0x71358c, 0x621ba4 /* "CCallForReinforcementsAction" */, 2.0f, action_class, 0x10, 0, 0x5740f0, 0x574130, 0x5742a0);
}
static CClassInfo *__cdecl SetTargetType_ClassInfo()
{
    return class_getter(0x713590, 0x621bd4 /* "CSetTargetTypeAction" */, 1.0f, action_class, 0x10, 0, 0x574440, 0x574480, 0x5745f0);
}
static CClassInfo *__cdecl SetTarget_ClassInfo()
{
    return class_getter(0x713594, 0x621c08 /* "CSetTargetAction" */, 1.0f, action_class, 0x10, 0, 0x5747a0, 0x5747e0, 0x574960);
}
// The actions with one string at +8: their level and CAction's
template <DWORD VT> static void action_string_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x573720, 0x573aa0, 0x573e20, 0x5741a0, 0x5744f0 (slot 1)
template <DWORD VT> static void *THISCALL ActionString_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, action_string_destroy<VT>>(self, flags); }
static void SetTarget_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SET_TARGET));
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x574850 (slot 1)
static void *THISCALL SetTarget_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, SetTarget_destroy>(self, flags); }
// 0x5740f0, 0x574130, 0x574440, 0x574480, 0x5747a0, 0x5747e0
static void CallForReinforcements_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);   // "Requester Name"
    vtable_and_defaults(o, vt(VT_REINFORCE));
}
static void SetTargetType_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);   // "Entity Name"
    vtable_and_defaults(o, vt(VT_SET_TARGET_TYPE));
    dw(o, 0xc) = 0;
}
static void SetTarget_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);     // "Attacker"
    zs_ctor(o + 0xc);   // "Target to attack"
    vtable_and_defaults(o, vt(VT_SET_TARGET));
}
static void *__cdecl CallForReinforcements_Create() { return create_one<0x10, CallForReinforcements_construct>(); }
static void *__fastcall CallForReinforcements_CreateArray(int count, int, int) { return create_array<0x10, CallForReinforcements_construct>(count); }
static void *__cdecl SetTargetType_Create() { return create_one<0x10, SetTargetType_construct>(); }
static void *__fastcall SetTargetType_CreateArray(int count, int, int) { return create_array<0x10, SetTargetType_construct>(count); }
static void *__cdecl SetTarget_Create() { return create_one<0x10, SetTarget_construct>(); }
static void *__fastcall SetTarget_CreateArray(int count, int, int) { return create_array<0x10, SetTarget_construct>(count); }

// 0x573820 / 0x573ba0 / 0x573f20: the AI template (an owned CLayerPartAI, +0x30 the class shown: the
// given getter's) and "Target". eax: the shown class.
template <Getter CLS, DWORD NAME, DWORD SHOWN> static CClassInfo *__cdecl SetAI_RegisterFields()
{
    void *ai = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(NAME), 0xc, 4, 0xc, layer_part_ai_class(), 1, 0, 0, 0);
    add_field(CLS(), ai);
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(CLS(), f);
    CClassInfo *c = class_at(SHOWN);
    dw(ai, 0x30) = ptr_value(c);
    return c;
}
// 0x5742a0. eax: ClassInfo_AddField's.
static DWORD __cdecl CallForReinforcements_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x621bc4) /* "Requester Name" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(CallForReinforcements_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c2a8) /* "Max Distance" */, 0xc, 4, 0xc, 1, 0x43fa0000 /* 500.0 */, 0, 0, 0x4cbebc20 /* 100000000.0 */,
                  0x41200000 /* 10.0 */);
    return add_field(CallForReinforcements_ClassInfo(), f);
}
// 0x5745f0. eax: ClassInfo_AddField's.
static DWORD __cdecl SetTargetType_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x621bfc) /* "Entity Name" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(SetTargetType_ClassInfo(), f);
    f = new_field(0x38, FLAGS_FIELD_CTOR, glob_addr(0x621bec) /* "Valid Targets" */, 0xc, 4, 0xc, glob_addr(0x6bd140), 2, 0, 0);
    return add_field(SetTargetType_ClassInfo(), f);
}
// 0x574960. eax: ClassInfo_AddField's.
static DWORD __cdecl SetTarget_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x621c28) /* "Target to attack" */, 0xc, 4, 0xc, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(SetTarget_ClassInfo(), f);
    f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x621c1c) /* "Attacker" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    return add_field(SetTarget_ClassInfo(), f);
}
// 0x573950 / 0x573cd0 / 0x574050 (slot 27: a1, a2, a3, target): the target's CSkeletonAI (slot 64) gets a
// copy of the AI template (0x5775e0 / 0x577620 / 0x577660). al: there was a template and a skeleton AI.
template <DWORD SETTER> static DWORD THISCALL SetAI_Target(BYTE *self, DWORD, DWORD, DWORD, BYTE *target)
{
    if (!dw(self, 0xc))
        return 0;
    CClassInfo *c = class_at(fn_addr(0x574fa0));   // CSkeletonAI
    BYTE *sk = part_ai(target, c, 1);
    if (!sk)
        return 0;
    void *copy = fn<void *(THISCALL *)(void *, DWORD, int)>(0x416760)(bp(dw(self, 0xc)), 0, -1);   // Object_Clone
    fn<void(THISCALL *)(void *, void *, void *)>(SETTER)(sk, target, copy);
    return 1;
}
// 0x5743c0 (slot 27: a1, a2, a3, target): a1 (not the target) calls for reinforcements against its target
// within "Max Distance" (0x55d070: the parts in that oval get message 0x7130c0). al 1.
static DWORD THISCALL CallForReinforcements_Target(BYTE *self, BYTE *a1, DWORD, DWORD, DWORD)
{
    const DWORD radius = dw(self, 0xc);
    BYTE *t = part_target(a1);
    fn<void(__fastcall *)(void *, void *, DWORD)>(0x55d070)(a1, t, radius);
    return 1;
}
// 0x574710 (slot 27: a1, a2, a3, target): the target's CSkeletonAI gets "Valid Targets" (0x5776b0). al:
// it has one.
static DWORD THISCALL SetTargetType_Target(BYTE *self, DWORD, DWORD, DWORD, BYTE *target)
{
    CClassInfo *c = class_at(fn_addr(0x574fa0));   // CSkeletonAI
    BYTE *sk = part_ai(target, c, 1);
    if (!sk)
        return 0;
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x5776b0)(sk, target, dw(self, 0xc));
    return 1;
}
// 0x574a80 (slot 27: a1, a2, a3, attacker): "Target to attack" resolved in the attacker's layer
// (0x41ad40) becomes the target of the attacker's CSkeletonAI (0x576e50) when it is "Collideable". al.
static DWORD THISCALL SetTarget_Target(BYTE *self, BYTE *a1, BYTE *a2, BYTE *a3, BYTE *attacker)
{
    if (!attacker)
        return 0;
    BYTE *t = fn<BYTE *(__fastcall *)(void *, void *, void *, void *, void *, void *, DWORD, DWORD)>(0x41ad40)(bp(dw(attacker, 0x48)), self + 0xc, a1, a3,
                                                                                                          a2, self, 1, 0);
    CClassInfo *c = class_at(fn_addr(0x574fa0));   // CSkeletonAI
    BYTE *sk = part_ai(attacker, c, 1);
    if (!sk || !t || !(dw(t, 0x1c) & 0x40000))
        return 0;
    fn<void(THISCALL *)(void *, void *, void *)>(0x576e50)(sk, attacker, t);
    return 1;
}

ZAX_HOOKS(ai_skeleton)
{
    const Hook list[] = {
#include "ai_skeleton_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
