// The rest of the entity behaviours and the AIs built on them (phase 3, game logic; 0x49c000-0x4a7fff and
// 0x578000-0x587fff): CShootEntityBehavior, CPointWeaponAtTargetEntityBehavior,
// CEntityBehaviorStateSetClosestTarget, CEntityBehaviorQueue, the AIs CBroadcastEvilAI, CStandStillAttackAI,
// CGuardPosAI and FollowAI, the fidget settings and data the idle behaviour uses (CFidgetSettings,
// CFidgetData) and CSkeletonUpdatedMessage: class getters (both copies), deleting destructors (slot 1),
// the out-of-line destructors, RegisterFields and the behaviour slots. Their Create, CreateArray and
// constructors are in ai_classes.cpp.
//
// Behaviour slots (CEntityBehavior, ai_behaviors.h): 1 deleting destructor, 2 class getter, 23 Begin
// (entity; al), 25 Update (seconds, entity; al: still running), 28 debug drawing (entity, view), 29 End
// (entity). AI slots (ai_simple.cpp): 24 Begin (entity), 25 Update (entity, seconds), 41 debug drawing
// (entity, view), 0 a message (sender, text).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CShootEntityBehavior (0x14, class 0x713668, vtable 0x60098c): +0xc the owned "Play Sequence"
//    (CEntityBehaviorPlaySequenceOnce while a shoot sequence plays), +0x10 the last angle off the target
//    (2 pi: none). Slot 30 (entity, sequence name): plays the sequence on the entity if its model has it,
//    otherwise fires once through a CDischargeWeaponThisFrameBaseAction made on the stack.
//  - CPointWeaponAtTargetEntityBehavior (0xc, class 0x71366c, vtable 0x600a08): no fields; turns the
//    aiming angle towards the target at the model's turn speed. RegisterFields only renames the class
//    info (+8: "CTurnToTargetEntityBehavior").
//  - CEntityBehaviorStateSetClosestTarget (0x20, class 0x713850, vtable 0x601654): +0xc passed through,
//    +0x10 "Max Distance", +0x14 "Vision Cone" (degrees), +0x18 "Target/Valid Targets" (categories),
//    +0x1c bits 0x40000 "Target/Must Be Collideable" and 0x100000 "Target/Must Have HPs".
//  - CEntityBehaviorQueue (0x24, class 0x713858, vtable 0x6016d0): +0xc the owned behaviours ("Behavior",
//    pointer array vtable 0x5f3228, base 0x5f3234, data +0x10 from the pool, count +0x14, capacity +0x18),
//    +0x1c "Current Behavior", +0x20 "Changed Behavior" (the current one still has to begin).
//  - CBroadcastEvilAI (0x14, class 0x713820, vtable 0x601458, under CFlyweightLayerPartAI): +0x10 "Evil To
//    Broadcast".
//  - CStandStillAttackAI (0x30, class 0x713828, version 4, vtable 0x601500, under CAttackAI): +0x10 the
//    embedded "Shoot/Shoot Behavior", +0x24 "Turn/Turn Behavior" (CPointWeaponAtTargetEntityBehavior).
//  - CGuardPosAI (0x108, class 0x713830, vtable 0x6015a8, under CPatrolBaseAI): +0x10 "Debug/Going Home",
//    +0x11 "Debug/Turning", +0x14 "Move To Behavior" (CEntityBehaviorMoveToPos, its speed at +0xb8 of the
//    AI), +0xd0 "Idle Behavior", +0x100 "Debug/Next Try To Move", +0x104 "Debug/Time Since Last Turn".
//  - FollowAI (0xf8, class 0x6be1b0, vtable 0x5f422c, under CPatrolBaseAI): +0x10 "Leader Name", +0x14
//    "Moving/Moving", +0xd0 "Turn/Turn", +0xec "Debug/Current Leader" (listened to), +0xf0/+0xf4 "Debug/
//    Relative Position X/Y" (in the leader's frame).
//  - CFidgetSettings (0x14, class 0x6bdf3c, vtable 0x5f3f94): +8 "Avg Time Between Fidgets", +0xc
//    "Fidget Variance", +0x10 "Fidget Sequence Order" (enum list 0x6bdf20).
//  - CFidgetData (0x8c, class 0x6bdf40, vtable 0x5f4000, RegisterFields the empty 0x58ea40): +8 the model
//    index, +0xc the "Fidget" sequences of the model (array vtable 0x5ee168, heap data), +0x28 the
//    "FidgetCrouch" ones, +0x44 and +0x68 two more arrays, list views (vtable 0x5f405c, base 0x5ec7cc) at
//    +0x1c, +0x38, +0x54, +0x78; +0x60 / +0x84 the last index taken of the first / second list.
#include "ai_helpers.h"
#include "crt_portable.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_SHOOT = 0x60098c, VT_POINT = 0x600a08, VT_CLOSEST = 0x601654, VT_QUEUE = 0x6016d0, VT_BROADCAST = 0x601458,
                VT_STAND_STILL = 0x601500, VT_GUARD = 0x6015a8, VT_FOLLOW = 0x5f422c, VT_FIDGET_SETTINGS = 0x5f3f94,
                VT_FIDGET_DATA = 0x5f4000;
constexpr DWORD VT_PTR_ARRAY_BASE = 0x5f3234, VT_ARRAY = 0x5ee168, VT_LIST_VIEW_BASE = 0x5ec7cc;
constexpr DWORD EMPTY_REGISTER_FIELDS = 0x58ea40;
constexpr DWORD BIT_FIELD_CTOR = 0x54dfc0, STRING_FIELD_VTABLE = 0x5ed924;
constexpr DWORD G_MODEL_LIST = 0x6cfdd8;

// CEntityBehavior (ai_behaviors.cpp): the class getter 0x494410, the destructor 0x4944e0
CClassInfo *entity_behavior_class() { return class_at(fn_addr(0x494410)); }
void entity_behavior_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4944e0)(o); }
// The level destructors other modules reimplement: CAttackAI 0x42e920, CPatrolBaseAI 0x53bd70,
// CEntityBehaviorIdle 0x498dc0, CEntityBehaviorTurnToAngle 0x495360, CObject 0x4166d0
void attack_ai_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x42e920)(o); }
void patrol_base_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x53bd70)(o); }
void idle_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x498dc0)(o); }
void turn_to_angle_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x495360)(o); }

// Exe functions the slots call (stand-ins in the checks)
long double turn_to_part(void *part, void *target, DWORD arg) { return fn<long double(THISCALL *)(void *, void *, DWORD)>(0x4fb3c0)(part, target, arg); }
long double shoot_angle(void *def) { return fn<long double(THISCALL *)(void *)>(0x4d6520)(def); }
DWORD find_sequence(void *model, const char *name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x517240)(model, name); }
const char *sequence_name(void *model, DWORD i) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x517200)(model, i); }
long double turn_speed(void *model) { return fn<long double(THISCALL *)(void *)>(0x517510)(model); }
long double part_distance2(void *part) { return fn<long double(THISCALL *)(void *)>(0x4faf50)(part); }
DWORD current_index(BYTE *inv, DWORD category) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, category); }
BYTE *current_definition(BYTE *inv, DWORD category) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4252b0)(inv, category); }
// 0x5664b0 (on an item): the weapon behind it (not hookable, 4 bytes long)
BYTE *item_weapon(void *item) { return fn<BYTE *(THISCALL *)(void *)>(0x5664b0)(item); }
DWORD set_turn_target(BYTE *turn, float angle) { return fn<DWORD(THISCALL *)(void *, float)>(0x4953a0)(turn, angle); }
BYTE *find_nearest(void *part, DWORD max_distance, DWORD flags, DWORD a3, DWORD categories, float cone, DWORD a6)
{
    return fn<BYTE *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, float, DWORD)>(0x4fc390)(part, max_distance, flags, a3, categories, cone, a6);
}
// 0x4fcce0 (view in ecx, centre in edx; radius, y scale, colour, from, to): an arc of the view
DWORD draw_arc(void *view, const float *center, DWORD radius, DWORD y_scale, void *color, float from, float to)
{
    return fn<DWORD(__fastcall *)(void *, const float *, DWORD, DWORD, void *, float, float)>(0x4fcce0)(view, center, radius, y_scale, color, from,
                                                                                                    to);
}
// 0x496e20 (fastcall entity, name; ratio, idle sequence, two flags, a count): a new CEntityBehaviorPlaySequenceOnce
BYTE *make_play_once(void *e, void *name, float ratio, void *idle, DWORD a, DWORD b, DWORD c)
{
    return fn<BYTE *(__fastcall *)(void *, void *, float, void *, DWORD, DWORD, DWORD)>(0x496e20)(e, name, ratio, idle, a, b, c);
}
// The game's random generator 0x7124c0: 0x55c550 a float in [lo, hi) (st0), 0x55c4e0 an integer in [lo, hi]
long double random_float(float lo, float hi) { return fn<long double(THISCALL *)(void *, float, float)>(0x55c550)(glob<void *>(GAME_RANDOM), lo, hi); }
DWORD random_between(DWORD lo, DWORD hi) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x55c4e0)(glob<void *>(GAME_RANDOM), lo, hi); }

// strcmp inlined (bytes, 0 when equal)
bool same_text(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (*a != *b)
            return false;
        if (!*a)
            return true;
    }
}

BYTE *as_kind(BYTE *o, CClassInfo *(*getter)())
{
    return o && is_kind_of(o, getter()) ? o : nullptr;
}
CClassInfo *walking_class() { return entity_walking_class(); }
CClassInfo *animated_class() { return entity_animated_class(); }

// A pointer array's element removed (PtrArray_RemoveAt 0x586fc0 inlined): the elements behind it move down
void remove_at(BYTE *arr, DWORD i)
{
    const DWORD n = dw(arr, 8);
    if (i >= n)
        return;
    if (i + 1 < n) {
        DWORD *d = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(arr, 4)));
        fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d + i, d + i + 1, (n - (i + 1)) * 4);   // memmove
    }
    dw(arr, 8)--;
}

// fptan of st0: tan (the 1.0 it pushes popped)
long double fptan_(long double v)
{
    __asm__("fptan\n\tfstp %%st(0)" : "+t"(v));
    return v;
}

}  // namespace

// ---- CShootEntityBehavior ----------------------------------------------------------------------------------

static CClassInfo *__cdecl ShootEntityBehavior_ClassInfo() { return class_at(fn_addr(0x577e50)); }
// 0x578020. eax: ClassInfo_AddField's.
static DWORD __cdecl ShootEntityBehavior_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x622198) /* "Play Sequence" */, 0xc, 4, 0xc, class_at(fn_addr(0x496620)), 0, 0, 0, 0);
    return add_field(ShootEntityBehavior_ClassInfo(), f);
}
// 0x5780d0
static void THISCALL ShootEntityBehavior_Dtor(BYTE *self)
{
    vtable_and_free(self, vt(VT_SHOOT));
    entity_behavior_dtor(self);
}
// 0x5780f0 (slot 23, entity): the playing sequence deleted, no angle yet. al: 1.
static DWORD THISCALL ShootEntityBehavior_Begin(BYTE *self, void *)
{
    DWORD r = 0;
    if (BYTE *b = bp(dw(self, 0xc))) {
        r = vcall<DWORD>(b, 1, 1);
        dw(self, 0xc) = 0;
    }
    dw(self, 0x10) = 0x40c90fdb;   // 2 pi
    return (r & 0xffffff00u) | 1;
}
// 0x578120 (slot 30: entity, sequence name): the sequence plays once (back to the function-local
// static "Idle", guard 0x713666 bit 0, string 0x713660, atexit 0x578230) when the entity's model has it;
// otherwise a CDischargeWeaponThisFrameBaseAction on the stack fires once (Set 0x4c4dd0 with 0, 0, 0,
// 1.0; CAction_ExecuteFrom with the entity's layer and the behaviour as the caller)
static DWORD THISCALL ShootEntityBehavior_PlayOrFire(BYTE *self, BYTE *e, void *name)
{
    const char *text = zs_c_str(name);
    if (find_sequence(entity_model(e), text) != 0xffffffffu) {
        BYTE &guard = *glob<BYTE *>(0x713666);
        if (!(guard & 1)) {
            guard |= 1;
            fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(glob<void *>(0x713660), glob<const char *>(0x60f8d0) /* "Idle" */);
            crt_atexit(code<void(__cdecl *)()>(0x578230));
        }
        BYTE *b = make_play_once(e, name, 1.0f, glob<void *>(0x713660), 0, 0, 0xffffffffu);
        dw(self, 0xc) = ptr_value(b);
        return vcall<DWORD>(b, 23, e);
    }
    alignas(4) BYTE action[0x20];
    action_ctor(action);
    vtable_and_defaults(action, vt(0x5f5ff0));   // CPositionedAction
    vtable_and_defaults(action, vt(0x5f66f4));   // CDischargeWeaponThisFrameBaseAction
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, float)>(0x4c4dd0)(action, 0, 0, 0, 1.0f);
    fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, void *)>(0x41a740)(action, bp(dw(e, 0x48)), e, 0, 0, self);
    vtable_and_free(action, vt(0x5f66f4));
    vtable_and_free(action, vt(0x5f5ff0));
    action_dtor(action);
    return 0;
}
// 0x578240 (slot 25: seconds, entity). While a sequence plays it runs (ended and deleted when it stops).
// Otherwise with a primary weapon (0x5664b0 of the current primary item) that is ready (its slot 39 not
// above 0) and a target (entity slot 70): the angle to the target (CLayerPart_TurnToPart with the
// entity's slot 47 of 1.0), when the weapon's shoot angle is not above it and it shrank since the last
// update (not below the last one in absolute value) the behaviour waits (+0x10 the angle). Otherwise
// +0x10 = 2 pi and, when the entity may shoot (slot 102), a walking entity is told "shoot" (message
// 0x713c3c, vtable 0x5fcfd4) instead, a flying or stationary one fires: the weapon's sequence (slot 35)
// played through slot 30, the weapon told (slot 23). al: 1 while the sequence plays, else 0.
static DWORD THISCALL ShootEntityBehavior_Update(BYTE *self, DWORD seconds, BYTE *e)
{
    if (BYTE *b = bp(dw(self, 0xc))) {
        const DWORD r = vcall<DWORD>(b, 25, seconds, e);
        if (r & 0xff)
            return (r & 0xffffff00u) | 1;
        const DWORD r2 = vcall<DWORD>(bp(dw(self, 0xc)), 29, e);
        dw(self, 0xc) = 0;
        return r2 & 0xffffff00u;
    }
    BYTE *inv = part_inventory(e);
    if (!inv)
        return 0;
    const DWORD cat = category_index(glob_addr(0x60b780) /* "Primary" */);
    BYTE *item = vcall<BYTE *>(inv, 26, current_index(inv, cat));
    BYTE *weapon = item_weapon(item);
    if (!weapon)
        return 0;
    if (!c03(vcall<long double>(weapon, 39), cst(glob_addr(C_ZERO))))
        return 0;
    BYTE *target = vcall<BYTE *>(e, 70);
    if (!target)
        return 0;
    const DWORD a47 = vcall<DWORD>(e, 47, 1.0f);
    const float angle = fstp(turn_to_part(e, target, a47));
    const float off = raw_float(float_bits(angle) & 0x7fffffffu);
    if (c03(shoot_angle(item_definition(weapon)), fld(off))) {
        const float last = raw_float(dw(self, 0x10) & 0x7fffffffu);
        if (!c0(fld(last), fld(off))) {
            dw(self, 0x10) = float_bits(off);
            return 0;
        }
    }
    dw(self, 0x10) = 0x40c90fdb;
    if (vcall<DWORD>(e, 102) & 0xff) {
        BYTE *walking = as_kind(e, walking_class);
        BYTE *flying = as_kind(e, entity_flying_class);
        BYTE *stationary = as_kind(e, getter<0x49b400>);
        if (walking) {
            Message m = {vt(0x5fcfd4), *glob<DWORD *>(0x713c3c), 0};
            vcall<DWORD>(e, 63, &m);
            return 0;
        }
        if (!flying && !stationary)
            return 0;
    }
    void *name = vcall<void *>(weapon, 35, e);
    vcall<DWORD>(self, 30, e, name);
    vcall<DWORD>(weapon, 23, e);
    return 0;
}
// 0x578460 (slot 28: entity, view): the cone of the primary weapon's shoot angle around the aiming
// angle, 999 pixels (function-local static colour 255, 40, 40, 255: guard 0x713664 bit 0, colour
// 0x713650, atexit 0x578540)
static void THISCALL ShootEntityBehavior_Draw(BYTE *, BYTE *e, void *view)
{
    BYTE *inv = part_inventory(e);
    if (!inv)
        return;
    BYTE *def = current_definition(inv, category_index(glob_addr(0x60b780) /* "Primary" */));
    if (!def)
        return;
    const float s = fstp(shoot_angle(def));
    float pos[2];
    part_position(e, pos);
    const float aim = fl<float>(e, 0xc4);
    void *color = static_color(0x713664, 0x713650, 0xff, 0x28, 0x28, 0xff, 0x578540);
    draw_arc(view, pos, 0x4479c000 /* 999 */, 0x3f800000, color, fstp(fld(aim) - fld(s)), fstp(fld(aim) + fld(s)));
}

// ---- CPointWeaponAtTargetEntityBehavior ----------------------------------------------------------------------

// 0x578550 (static registration), 0x5786b0 (slot 2)
static CClassInfo *__cdecl PointWeapon_ClassInfo()
{
    return class_getter(0x71366c, 0x6221a8 /* "CPointWeaponAtTargetEntityBehavior" */, 2.0f, entity_behavior_class, 0xc, 0, 0x5785a0, 0x5785d0,
                        0x578710);
}
static void PointWeapon_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_POINT));
    entity_behavior_dtor(o);
}
// 0x578630 (slot 1)
static void *THISCALL PointWeapon_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, PointWeapon_destroy>(self, flags); }
// 0x5787a0
static void THISCALL PointWeapon_Dtor(BYTE *self) { PointWeapon_destroy(self); }
// 0x578710: the class info's old name (+8) "CTurnToTargetEntityBehavior". eax: the class info.
static CClassInfo *__cdecl PointWeapon_RegisterFields()
{
    CClassInfo *c = *glob<CClassInfo **>(0x71366c);
    if (!c) {
        void *p = pool_alloc(sizeof(CClassInfo));
        if (!p) {
            dw(*glob<CClassInfo **>(0x71366c), 8) = glob_addr(0x6221cc);
            return nullptr;
        }
        fn<RegisterFn>(0x415780)(p, glob<CClassInfo **>(0x71366c), glob<const char *>(0x6221a8), 2.0f, entity_behavior_class(), 0xc, 0, fn_addr(0x5785a0),
                                 0x5785d0, 0x578710);
        c = *glob<CClassInfo **>(0x71366c);
    }
    dw(c, 8) = glob_addr(0x6221cc);   // "CTurnToTargetEntityBehavior"
    return c;
}
// 0x5787c0 (slot 25: seconds, entity): a movable entity with a target (slot 70) turns its aiming angle
// (slot 103 (angle, 1)) towards it when the angle to it (CLayerPart_TurnToPart with slot 47 of 1.0) is
// above the threshold 0x600a80, by at most the model's turn speed times the seconds. al: 1.
static DWORD THISCALL PointWeapon_Update(BYTE *, DWORD seconds, BYTE *e)
{
    BYTE *target = vcall<BYTE *>(e, 70);
    if (!target || !is_kind_of(e, entity_movable_class()))
        return 1;
    const DWORD a47 = vcall<DWORD>(e, 47, 1.0f);
    const float d = fstp(turn_to_part(e, target, a47));
    const float off = raw_float(float_bits(d) & 0x7fffffffu);
    if (c03(fld(off), cst(glob_addr(0x600a80))))
        return 1;
    const long double m = turn_speed(entity_model(e)) * fld(raw_float(seconds));
    const float mf = fstp(m);
    long double step;
    if (c0(fld(d), -m))
        step = -m;
    else if (c03(fld(d), fld(mf)))
        step = fld(d);
    else
        step = fld(mf);
    vcall<DWORD>(e, 103, fstp(fld(fl<float>(e, 0xc4)) + step), 1);
    return 1;
}

// ---- CSkeletonUpdatedMessage ---------------------------------------------------------------------------------

// 0x5788a0: the message object (vtable 0x600a84, the id registered for "CSkeletonUpdatedMessage" at
// 0x713670, not handled)
static BYTE *THISCALL SkeletonUpdatedMessage_Ctor(BYTE *self)
{
    dw(self, 4) = *glob<DWORD *>(0x713670);
    self[8] = 0;
    dw(self, 0) = vt(0x600a84);
    return self;
}
// 0x5788c0: the base message's destructor (its vtable 0x600a88). eax unchanged (not reproduced).
static void THISCALL Message_Dtor(BYTE *self) { dw(self, 0) = vt(0x600a88); }
// 0x5788d0 (the base message's slot: flags): bit 0 frees (operator delete)
static BYTE *THISCALL Message_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x600a88);
    if (flags & 1)
        heap_free(self);
    return self;
}

// ---- CBroadcastEvilAI ---------------------------------------------------------------------------------------

// 0x584e50 (static registration), 0x585020 (slot 2); version 0
static CClassInfo *__cdecl BroadcastEvil_ClassInfo()
{
    return class_getter(0x713820, 0x622bdc /* "CBroadcastEvilAI" */, 0.0f, flyweight_ai_class, 0x14, 0, 0x584ea0, 0x584ef0, 0x585080);
}
static void BroadcastEvil_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_BROADCAST));
    flyweight_ai_dtor(o);
}
// 0x584f60 (slot 1)
static void *THISCALL BroadcastEvil_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, BroadcastEvil_destroy>(self, flags); }
// 0x585080: "Evil To Broadcast" (PerWhole, default 0.2, maximum 1.0), its edit list 0x6be348 and help.
// eax: FieldDesc_SetHelp's.
static DWORD __cdecl BroadcastEvil_RegisterFields()
{
    BYTE *f = static_cast<BYTE *>(new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622c34) /* "Evil To Broadcast" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0x10, 4,
                                            0x10, 1, 0x3e4ccccd, 0, 0, 0x3f800000, 0));
    add_field(BroadcastEvil_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60f5d8));
    return set_help(f, 0x622bf0 /* "Amount of Evil this thing should broadcast to the sphere of zin." */);
}
// 0x585140 (slot 25: entity, seconds): the evil goes to the world (slot 86: entity, amount) unless the
// entity is destructable with hit points above 0 and a health ratio of 0
static DWORD THISCALL BroadcastEvil_Update(BYTE *self, BYTE *e, DWORD)
{
    if (e && is_kind_of(e, entity_destructable_class()) && !c03(hit_points(e), cst(glob_addr(C_ZERO))) &&
        c3(fn<long double(THISCALL *)(void *)>(0x48d260)(e), cst(glob_addr(C_ZERO))))
        return 0;
    return vcall<DWORD>(world(), 86, e, dw(self, 0x10));
}

// ---- CStandStillAttackAI -------------------------------------------------------------------------------------

// 0x5856a0 (static registration), 0x585890 (slot 2)
static CClassInfo *__cdecl StandStill_ClassInfo()
{
    return class_getter(0x713828, 0x622c48 /* "CStandStillAttackAI" */, 4.0f, getter<0x42e870>, 0x30, 0, 0x5856f0, 0x585750, 0x5858f0);
}
static void StandStill_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_STAND_STILL));
    fn<void(THISCALL *)(void *)>(0x5787a0)(o + 0x24);   // CPointWeaponAtTargetEntityBehavior
    fn<void(THISCALL *)(void *)>(0x5780d0)(o + 0x10);   // CShootEntityBehavior
    attack_ai_dtor(o);
}
// 0x5857e0 (slot 1)
static void *THISCALL StandStill_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, StandStill_destroy>(self, flags); }
// 0x5858f0. eax: ClassInfo_AddField's.
static DWORD __cdecl StandStill_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61d25c) /* "Shoot/Shoot Behavior" */, 0x10, 0x14, 0x10, 1);
    add_field(StandStill_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61d248) /* "Turn/Turn Behavior" */, 0x24, 0xc, 0x24, 1);
    return add_field(StandStill_ClassInfo(), f);
}
// 0x5859f0 (slot 24, entity): a walking entity stands (+0x12c "Movement Speed" 1), both behaviours begin
static DWORD THISCALL StandStill_Begin(BYTE *self, BYTE *e)
{
    if (as_kind(e, walking_class))
        dw(e, 0x12c) = 1;
    vcall<DWORD>(self + 0x10, 23, e);
    return vcall<DWORD>(self + 0x24, 23, e);
}
// 0x585a30 (slot 25: entity, seconds): both behaviours update
static DWORD THISCALL StandStill_Update(BYTE *self, DWORD e, DWORD seconds)
{
    vcall<DWORD>(self + 0x10, 25, seconds, e);
    return vcall<DWORD>(self + 0x24, 25, seconds, e);
}
// 0x585a60 (slot 41: entity, view): both behaviours draw
static DWORD THISCALL StandStill_Draw(BYTE *self, DWORD e, DWORD view)
{
    vcall<DWORD>(self + 0x10, 28, e, view);
    return vcall<DWORD>(self + 0x24, 28, e, view);
}

// ---- CGuardPosAI -------------------------------------------------------------------------------------------

// 0x585a90 (static registration), 0x585cb0 (slot 2)
static CClassInfo *__cdecl GuardPos_ClassInfo()
{
    return class_getter(0x713830, 0x622c5c /* "CGuardPosAI" */, 1.0f, getter<0x53bcc0>, 0x108, 0, 0x585af0, 0x585b50, 0x585d20);
}
static void GuardPos_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_GUARD));
    idle_dtor(o + 0xd0);
    move_to_pos_dtor(o + 0x14);
    patrol_base_dtor(o);
}
// 0x585bf0 (slot 1)
static void *THISCALL GuardPos_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x108, GuardPos_destroy>(self, flags); }
// 0x585d20. eax: ClassInfo_AddField's.
static DWORD __cdecl GuardPos_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x622cd4) /* "Idle Behavior" */, 0xd0, 0x30, 0xd0, 1);
    add_field(GuardPos_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x622cc0) /* "Move To Behavior" */, 0x14, 0xbc, 0x14, 1);
    add_field(GuardPos_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x622cac) /* "Debug/Going Home" */, 0x10, 1, 0x10, 3, 0, 0);
    add_field(GuardPos_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622c94) /* "Debug/Next Try To Move" */, 0x100, 4, 0x100, 3, 0, 0, 0, 0, 0);
    add_field(GuardPos_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622c78) /* "Debug/Time Since Last Turn" */, 0x104, 4, 0x104, 3, 0, 0, 0, 0, 0);
    add_field(GuardPos_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x622c68) /* "Debug/Turning" */, 0x11, 1, 0x11, 3, 0, 0);
    return add_field(GuardPos_ClassInfo(), f);
}
static DWORD guard_idle(BYTE *self, BYTE *anim)
{
    ent_set_play_ratio(anim, 0x3f800000);
    ent_play_sequence(anim, glob_addr(0x60f8d0) /* "Idle" */);
    const DWORD r = vcall<DWORD>(self + 0xd0, 23, anim);
    dw(self, 0x104) = 0;
    return r;
}
// 0x586050 (slot 24, entity): "Going Home" when the entity is farther than 0x601650 from its post
// (CLayerPart_SlotDistance2 0x4faf50): a walking one walks (+0x12c 0) back to its position (slot 40)
// with the move-to behaviour; otherwise it idles ("Idle" at play ratio 1)
static DWORD THISCALL GuardPos_Begin(BYTE *self, BYTE *e)
{
    self[0x11] = 0;
    self[0x10] = c03(part_distance2(e), cst(glob_addr(0x601650))) ? 0 : 1;
    BYTE *anim = as_kind(e, animated_class);
    if (!self[0x10]) {
        if (!anim)
            return 0;
        return guard_idle(self, anim);
    }
    if (!anim)
        return 0;
    if (as_kind(e, walking_class))
        dw(e, 0x12c) = 0;
    DWORD r = vcall<DWORD>(self + 0x14, 23, anim);
    dw(self, 0x100) = 0;
    if (as_kind(e, animated_class)) {
        float home[2];
        vcall<DWORD>(e, 40, home);
        r = move_to(self + 0x14, e, home, fpu_float(fl<float>(self, 0xb8)), 1);
    }
    return r;
}
// 0x586170 (slot 25: entity, seconds): counts the time since the last turn; a pending retry (+0x100 above
// 0) counts down and begins again when it runs out; going home walks until the move-to behaviour ends,
// then idles; otherwise the idle behaviour updates
static DWORD THISCALL GuardPos_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    fl<float>(self, 0x104) = fstp(fld(raw_float(seconds)) + fld(fl<float>(self, 0x104)));
    BYTE *anim = as_kind(e, animated_class);
    if (!c03(fld(fl<float>(self, 0x100)), cst(glob_addr(C_ZERO)))) {
        const long double v = fld(fl<float>(self, 0x100)) - fld(raw_float(seconds));
        fl<float>(self, 0x100) = fstp(v);
        if (c03(v, cst(glob_addr(C_ZERO))))
            return vcall<DWORD>(self, 24, e);
        if (!anim)
            return 0;
    } else if (self[0x10]) {
        if (!anim)
            return 0;
        const DWORD r = vcall<DWORD>(self + 0x14, 25, seconds, anim);
        self[0x10] = (BYTE)r;
        if (r & 0xff)
            return r;
        return guard_idle(self, anim);
    } else if (!anim) {
        return 0;
    }
    return vcall<DWORD>(self + 0xd0, 25, seconds, anim);
}

// ---- CEntityBehaviorStateSetClosestTarget ------------------------------------------------------------------------

// 0x5862c0 (static registration), 0x586440 (slot 2)
static CClassInfo *__cdecl ClosestTarget_ClassInfo()
{
    return class_getter(0x713850, 0x622ce4 /* "CEntityBehaviorStateSetClosestTarget" */, 5.0f, entity_behavior_class, 0x20, 0, 0x586310, 0x586350,
                        0x5864a0);
}
static void ClosestTarget_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_CLOSEST));
    entity_behavior_dtor(o);
}
// 0x5863c0 (slot 1)
static void *THISCALL ClosestTarget_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, ClosestTarget_destroy>(self, flags); }
// 0x5867a0
static void THISCALL ClosestTarget_Dtor(BYTE *self) { ClosestTarget_destroy(self); }
// 0x5864a0. eax: ClassInfo_AddField's.
static DWORD __cdecl ClosestTarget_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x60c2a8) /* "Max Distance" */, 0x10, 4, 0x10, 1, 0x12c, 0, 0, 0);
    add_field(ClosestTarget_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622d58) /* "Vision Cone" */, glob_addr(0x60ff80) /* "Degrees" */, 0x14, 4, 0x14, 1, 0x42b40000, 0, 0x3f800000,
                  0x43b40000, 0x40a00000);
    add_field(ClosestTarget_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x622d3c) /* "Target/Must Be Collideable" */, 0x1c, 4, 0x1c, 0x40000, 1, 1, 0);
    add_field(ClosestTarget_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x622d24) /* "Target/Must Have HPs" */, 0x1c, 4, 0x1c, 0x100000, 1, 1, 0);
    add_field(ClosestTarget_ClassInfo(), f);
    f = new_field(0x38, FLAGS_FIELD_CTOR, glob_addr(0x622d0c) /* "Target/Valid Targets" */, 0x18, 4, 0x18, glob_addr(0x6bd140), 2, glob_addr(0x61807c) /* "Player" */, 0);
    return add_field(ClosestTarget_ClassInfo(), f);
}
// 0x586760: the vision cone in radians (st0)
static long double THISCALL ClosestTarget_ConeRadians(BYTE *self) { return fld(fl<float>(self, 0x14)) * cst(glob_addr(0x5edb3c)); }
// 0x5867c0 (slot 23, entity): the entity's target (slot 71) becomes the nearest part within "Max
// Distance" and half the vision cone (CLayerPart_FindNearest with the bits | 0x800000). eax: 1 when the
// entity has a target now (slot 70), else 0.
static DWORD THISCALL ClosestTarget_Begin(BYTE *self, BYTE *e)
{
    const float half = fstp(fld(fl<float>(self, 0x14)) * cst(glob_addr(0x5edb3c)) * cst(glob_addr(C_HALF)));
    BYTE *t = find_nearest(e, dw(self, 0x10), dw(self, 0x1c) | 0x800000, 0, dw(self, 0x18), half, dw(self, 0xc));
    vcall<DWORD>(e, 71, t);
    return vcall<DWORD>(e, 70) ? 1 : 0;
}
// 0x586820 (slot 28: entity, view): the search range (function-local static colour 255, 100, 25, 255:
// guard 0x713838 bit 0, colour 0x713840, atexit 0x586920): an oval of "Max Distance" when the cone is not
// below 0x6016cc, else the cone's arc around the entity's angle (slot 46)
static DWORD THISCALL ClosestTarget_Draw(BYTE *self, BYTE *e, void *view)
{
    void *color = static_color(0x713838, 0x713840, 0xff, 0x64, 0x19, 0xff, 0x586920);
    float pos[2];
    part_position(e, pos);
    const DWORD radius = float_bits(fstp((long double)(unsigned long long)dw(self, 0x10)));
    if (!c0(fld(fl<float>(self, 0x14)) * cst(glob_addr(0x5edb3c)), cst(glob_addr(0x6016cc))))
        return draw_oval(view, pos, radius, Y_SCALE, color);
    const long double a = vcall<long double>(e, 46);
    const long double half = fld(fl<float>(self, 0x14)) * cst(glob_addr(0x5edb3c)) * cst(glob_addr(C_HALF));
    const float to = fstp(half + a);
    const float from = fstp(a - half);
    return draw_arc(view, pos, radius, Y_SCALE, color, from, to);
}

// ---- CEntityBehaviorQueue --------------------------------------------------------------------------------------

// 0x586930 (static registration), 0x586be0 (slot 2); version 0
static CClassInfo *__cdecl Queue_ClassInfo()
{
    return class_getter(0x713858, 0x622d64 /* "CEntityBehaviorQueue" */, 0.0f, entity_behavior_class, 0x24, 0, 0x586980, 0x5869d0, 0x586c40);
}
// The level's destructor: the behaviours deleted from the last (slot 1 with 1) and removed (inlined, or
// through PtrArray_RemoveAt 0x586fc0 in the array destructor), the fields freed, the array's data back to
// the pool
static void queue_destroy(BYTE *o, bool by_call)
{
    dw(o, 0) = vt(VT_QUEUE);
    BYTE *arr = o + 0xc;
    while (dw(arr, 8)) {
        const DWORD i = dw(arr, 8) - 1;
        if (BYTE *b = bp(dw(bp(dw(arr, 4)), i * 4)))
            vcall<void *>(b, 1, 1);
        if (by_call)
            fn<void(THISCALL *)(void *, DWORD)>(0x586fc0)(arr, i);
        else
            remove_at(arr, i);
    }
    obj_free_fields(o);
    pool_array_dtor(arr, vt(VT_PTR_ARRAY_BASE));
    entity_behavior_dtor(o);
}
static void Queue_destroy(BYTE *o) { queue_destroy(o, false); }
static void Queue_destroy_element(BYTE *o) { queue_destroy(o, true); }
// 0x586a50 (slot 1)
static void *THISCALL Queue_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2)
        return deleting_dtor<0x24, Queue_destroy_element>(self, flags);
    return deleting_dtor<0x24, Queue_destroy>(self, flags);
}
// 0x586e40
static void THISCALL Queue_Dtor(BYTE *self) { queue_destroy(self, false); }
// 0x586c40. eax: ClassInfo_AddField's.
static DWORD __cdecl Queue_RegisterFields()
{
    void *element = heap_alloc(0x3c);
    if (element)
        element = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            element, glob_addr(0x6153f8) /* "Behavior" */, 0, 4, 0, entity_behavior_class(), 1, 0, 0, 0);
    void *f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x6153f8) /* "Behavior" */, 0x14, 4, 0x14, 0x18, 4, 0x18, 0x10, 4, 0x10, 1, element, 1, 0);
    add_field(Queue_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x622d90) /* "Current Behavior" */, 0x1c, 4, 0x1c, 0, 0x7fffffff, 0, 0, 0);
    add_field(Queue_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x622d7c) /* "Changed Behavior" */, 0x20, 1, 0x20, 0, 1, 0);
    return add_field(Queue_ClassInfo(), f);
}
// 0x586ef0 (slot 23, entity): a queue past its end starts again at the first behaviour. eax: the old index's
// upper bytes, al 1.
static DWORD THISCALL Queue_Begin(BYTE *self, void *)
{
    const DWORD old = dw(self, 0x1c);
    if (old >= dw(self, 0x14)) {
        dw(self, 0x1c) = 0;
        self[0x20] = 1;
    }
    return (old & 0xffffff00u) | 1;
}
// 0x586f10 (slot 25: seconds, entity): the current behaviour begins when it changed and updates; when it
// ends (or is null) the next one is current. al: 1 while one runs, 0 past the end.
static DWORD THISCALL Queue_Update(BYTE *self, DWORD seconds, void *e)
{
    DWORD i = dw(self, 0x1c);
    while (i < dw(self, 0x14)) {
        if (BYTE *b = bp(dw(bp(dw(self, 0x10)), dw(self, 0x1c) * 4))) {
            if (self[0x20]) {
                vcall<DWORD>(b, 23, e);
                self[0x20] = 0;
            }
            const DWORD r = vcall<DWORD>(b, 25, seconds, e);
            if (r & 0xff)
                return (r & 0xffffff00u) | 1;
        }
        self[0x20] = 1;
        i = ++dw(self, 0x1c);
    }
    return i & 0xffffff00u;
}
// 0x586f80 (a behaviour): appended (array slot 2); the first one of an empty queue has to begin
static DWORD THISCALL Queue_Append(BYTE *self, DWORD b)
{
    if (!b)
        return 0;
    if (!dw(self, 0x14))
        self[0x20] = 1;
    return vcall<DWORD>(self + 0xc, 2, &b);
}
// 0x586fb0: 1 when the queue is past its end
static DWORD THISCALL Queue_Done(BYTE *self) { return dw(self, 0x1c) < dw(self, 0x14) ? 0 : 1; }

// ---- FollowAI ---------------------------------------------------------------------------------------------------

// 0x4a4f40 (static registration), 0x4a51c0 (slot 2)
static CClassInfo *__cdecl Follow_ClassInfo()
{
    return class_getter(0x6be1b0, 0x613910 /* "FollowAI" */, 1.0f, getter<0x53bcc0>, 0xf8, 0, 0x4a4fa0, 0x4a5010, 0x4a5230);
}
static void follow_unlisten(BYTE *self)
{
    remove_listener(bp(dw(self, 0xec)), self, -1, glob_addr(TEXT_EMPTY));
    dw(self, 0xec) = 0;
}
static void Follow_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_FOLLOW);
    if (dw(o, 0xec))
        follow_unlisten(o);
    obj_free_fields(o);
    turn_to_angle_dtor(o + 0xd0);
    move_to_pos_dtor(o + 0x14);
    zs_dtor(o + 0x10);
    patrol_base_dtor(o);
}
// 0x4a50c0 (slot 1)
static void *THISCALL Follow_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xf8, Follow_destroy>(self, flags); }
// 0x4a5230. eax: ClassInfo_AddField's.
static DWORD __cdecl Follow_RegisterFields()
{
    BYTE *f = static_cast<BYTE *>(heap_alloc(0x34));
    if (f) {
        fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(STRING_FIELD_CTOR)(f, glob_addr(0x613988) /* "Leader Name" */, 0x10, 4,
                                                                                                            0x10, 1, glob_addr(TEXT_EMPTY), 0);
        dw(f, 0) = vt(STRING_FIELD_VTABLE);
    }
    add_field(Follow_ClassInfo(), f);
    void *g = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x613978) /* "Moving/Moving" */, 0x14, 0xbc, 0x14, 1);
    add_field(Follow_ClassInfo(), g);
    g = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61396c) /* "Turn/Turn" */, 0xd0, 0x1c, 0xd0, 1);
    add_field(Follow_ClassInfo(), g);
    g = heap_alloc(0x30);
    if (g)
        g = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD_CTOR)(
            g, glob_addr(0x613954) /* "Debug/Current Leader" */, 0xec, 4, 0xec, layer_part_class(), 3, 0);
    add_field(Follow_ClassInfo(), g);
    g = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x613938) /* "Debug/Relative Position X" */, 0xf0, 4, 0xf0, 3, 0, 0, 0, 0, 0);
    add_field(Follow_ClassInfo(), g);
    g = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61391c) /* "Debug/Relative Position Y" */, 0xf4, 4, 0xf4, 3, 0, 0, 0, 0, 0);
    return add_field(Follow_ClassInfo(), g);
}
// 0x4a5580 (slot 41: entity, view): the move-to behaviour draws
static DWORD THISCALL Follow_Draw(BYTE *self, DWORD e, DWORD view) { return vcall<DWORD>(self + 0x14, 28, e, view); }
// The point the follower keeps to: the leader's position (slot 43) plus the relative position turned by
// the leader's angle (slot 46, table sine and cosine 0x508fe0 of minus it); the move-to behaviour aims at it.
// In the exe's operand order (with two NaNs it decides the result), which differs between Begin and
// Update (update_order).
static void follow_aim(BYTE *self, BYTE *e, bool update_order)
{
    BYTE *leader = bp(dw(self, 0xec));
    float lp[2];
    vcall<DWORD>(leader, 43, lp);
    const float rx = fl<float>(self, 0xf0), ry = fl<float>(self, 0xf4);
    const float a = fstp(-vcall<long double>(leader, 46));
    float c1, c2, y;
    table_sin_cos(&c1, &c2, a);
    if (update_order)   // X = rx c2 + c1 ry, Y = -c1 rx + c2 ry
        __asm__("flds %[rx]\n\tfmuls %[c2]\n\tflds %[c1]\n\tfmuls %[ry]\n\tfaddp\n\t"
                "flds %[c1]\n\tfchs\n\tfmuls %[rx]\n\tflds %[c2]\n\tfmuls %[ry]\n\tfaddp\n\t"
                "fstps %[y]\n\tflds %[lx]\n\tfadd %%st(1), %%st\n\tfstps %[lx]\n\tfstp %%st(0)\n\t"
                "flds %[ly]\n\tfadds %[y]\n\tfstps %[ly]"
                : [y] "=m"(y), [lx] "+m"(lp[0]), [ly] "+m"(lp[1])
                : [c1] "m"(c1), [c2] "m"(c2), [rx] "m"(rx), [ry] "m"(ry)
                : "st", "st(1)", "st(2)");
    else   // X = ry c1 + rx c2, Y = -c1 rx + ry c2
        __asm__("flds %[ry]\n\tfmuls %[c1]\n\tflds %[rx]\n\tfmuls %[c2]\n\tfaddp\n\t"
                "flds %[c1]\n\tfchs\n\tfmuls %[rx]\n\tflds %[ry]\n\tfmuls %[c2]\n\tfaddp\n\t"
                "fstps %[y]\n\tflds %[lx]\n\tfadd %%st(1), %%st\n\tfstps %[lx]\n\tfstp %%st(0)\n\t"
                "flds %[ly]\n\tfadds %[y]\n\tfstps %[ly]"
                : [y] "=m"(y), [lx] "+m"(lp[0]), [ly] "+m"(lp[1])
                : [c1] "m"(c1), [c2] "m"(c2), [rx] "m"(rx), [ry] "m"(ry)
                : "st", "st(1)", "st(2)");
    move_to(self + 0x14, e, lp, fpu_float(fl<float>(self, 0xb8)), 1);
}
// 0x4a55a0 (slot 24, entity): the leader ("Leader Name", resolved from the entity's layer) is listened to;
// without a relative position yet it is the entity's offset from the leader in the leader's frame; the
// move-to behaviour aims at the point beside the leader and begins
static DWORD THISCALL Follow_Begin(BYTE *self, BYTE *e)
{
    BYTE *leader = find_named_part(bp(dw(e, 0x48)), self + 0x10, nullptr, nullptr, self, 0, 1, 0);
    if (dw(self, 0xec))
        remove_listener(bp(dw(self, 0xec)), self, -1, glob_addr(TEXT_EMPTY));
    dw(self, 0xec) = ptr_value(leader);
    if (leader)
        add_listener(leader, self, -1, glob_addr(TEXT_EMPTY));
    if (c3(fld(fl<float>(self, 0xf0)), cst(glob_addr(C_ZERO))) && c3(fld(fl<float>(self, 0xf4)), cst(glob_addr(C_ZERO))) && dw(self, 0xec)) {
        BYTE *l = bp(dw(self, 0xec));
        float lp[2], ep[2];
        vcall<DWORD>(l, 43, lp);
        vcall<DWORD>(e, 43, ep);
        fl<float>(self, 0xf0) = fstp(fld(ep[0]) - fld(lp[0]));
        fl<float>(self, 0xf4) = fstp(fld(ep[1]) - fld(lp[1]));
        const float a = fstp(vcall<long double>(bp(dw(self, 0xec)), 46));
        float c1, c2, nx, ny;
        table_sin_cos(&c1, &c2, a);
        // nx = c2 x + c1 y, ny = -c1 x + c2 y in the exe's operand order
        __asm__("flds %[c2]\n\tfmuls %[x0]\n\tflds %[c1]\n\tfmuls %[y0]\n\tfaddp\n\t"
                "flds %[c1]\n\tfchs\n\tfmuls %[x0]\n\tflds %[c2]\n\tfmuls %[y0]\n\tfaddp\n\t"
                "fstps %[y]\n\tfstps %[x]"
                : [x] "=m"(nx), [y] "=m"(ny)
                : [c1] "m"(c1), [c2] "m"(c2), [x0] "m"(fl<float>(self, 0xf0)), [y0] "m"(fl<float>(self, 0xf4))
                : "st", "st(1)", "st(2)");
        fl<float>(self, 0xf0) = nx;
        dw(self, 0xf4) = float_bits(ny);
    }
    if (dw(self, 0xec))
        follow_aim(self, e, false);
    return vcall<DWORD>(self + 0x14, 23, e);
}
// 0x4a5790 (slot 25: entity, seconds): with a leader the move-to behaviour aims at the point beside it and
// updates; when it has arrived the turn behaviour turns to the leader's angle
static DWORD THISCALL Follow_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    if (!dw(self, 0xec))
        return 0;
    follow_aim(self, e, true);
    const DWORD r = vcall<DWORD>(self + 0x14, 25, seconds, e);
    if (r & 0xff)
        return r;
    set_turn_target(self + 0xd0, fstp(vcall<long double>(bp(dw(self, 0xec)), 46)));
    return vcall<DWORD>(self + 0xd0, 25, seconds, e);
}
// 0x4a5890 (slot 0: sender, message text): "Died" or "Delete" of the leader ends listening to it
static void THISCALL Follow_OnMessage(BYTE *self, void *, const char *text)
{
    if (!crt_stricmp(text, glob<const char *>(TEXT_DIED)) || !crt_stricmp(text, glob<const char *>(TEXT_DELETE)))
        follow_unlisten(self);
}

// ---- CFidgetSettings and CFidgetData ---------------------------------------------------------------------------

// 0x49ff50 (static registration), 0x4a00b0 (slot 2)
static CClassInfo *__cdecl FidgetSettings_ClassInfo()
{
    return class_getter(0x6bdf3c, 0x6134e4 /* "CFidgetSettings" */, 1.0f, getter<0x4163b0>, 0x14, 0, 0x49ffa0, 0x49ffd0, 0x4a0110);
}
static void FidgetSettings_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_FIDGET_SETTINGS));
    obj_dtor(o);
}
// 0x4a0030 (slot 1)
static void *THISCALL FidgetSettings_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, FidgetSettings_destroy>(self, flags); }
// 0x4a0110. eax: ClassInfo_AddField's.
static DWORD __cdecl FidgetSettings_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61351c) /* "Avg Time Between Fidgets" */, glob_addr(0x613538) /* "seconds" */, 8, 4, 8, 1, 0x41a00000,
                        0, 0, 0x4cbebc20, 0);
    add_field(FidgetSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61350c) /* "Fidget Variance" */, 0xc, 4, 0xc, 1, 0x40800000, 0, 0, 0x4cbebc20, 0);
    add_field(FidgetSettings_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6134f4) /* "Fidget Sequence Order" */, 0x10, 4, 0x10, glob_addr(0x6bdf20), 1, glob_addr(0x6134c0) /* "Random Order" */,
                  0, 0);
    return add_field(FidgetSettings_ClassInfo(), f);
}
// 0x4a02c0: the time to the next fidget (st0): the average, spread by the variance through a tangent:
// avg + var * tan((t + k) * r - t) with t = atan(avg / var), k the double at 0x5f3ff8 and r a random
// number in [0, 1); without variance the average
static long double THISCALL FidgetSettings_NextTime(BYTE *self)
{
    const long double r = random_float(0.0f, 1.0f);
    if (c3(fld(fl<float>(self, 0xc)), cst(glob_addr(C_ZERO))))
        return cst(glob_addr(C_ZERO)) + fld(fl<float>(self, 8));
    const long double t = fpatan(fld(fl<float>(self, 8)) / fld(fl<float>(self, 0xc)), 1.0L);
    const long double x = (t + fld64(glob<void *>(0x5f3ff8))) * r - t;
    return fptan_(x) * fld(fl<float>(self, 0xc)) + fld(fl<float>(self, 8));
}

// 0x4a0320 (static registration), 0x4a0470 (slot 2)
static CClassInfo *__cdecl FidgetData_ClassInfo()
{
    return class_getter(0x6bdf40, 0x613540 /* "CFidgetData" */, 1.0f, getter<0x4163b0>, 0x8c, 0, 0x4a0380, 0x4a03a0, EMPTY_REGISTER_FIELDS);
}
// An array of the fidget data (vtable 0x5ee168, heap data) emptied
static void fidget_array_free(BYTE *a)
{
    dw(a, 0) = vt(VT_ARRAY);
    if (dw(a, 4)) {
        heap_free(bp(dw(a, 4)));
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
}
// 0x4a0590: the four list views and arrays from the last
static void THISCALL FidgetData_Dtor(BYTE *self)
{
    vtable_and_free(self, vt(VT_FIDGET_DATA));
    const DWORD views[] = {0x78, 0x54, 0x38, 0x1c}, arrays[] = {0x68, 0x44, 0x28, 0xc};
    for (int k = 0; k < 4; k++) {
        dw(self, views[k]) = vt(VT_LIST_VIEW_BASE);
        fidget_array_free(self + arrays[k]);
    }
    obj_dtor(self);
}
static void FidgetData_destroy(BYTE *o) { FidgetData_Dtor(o); }
// 0x4a0400 (slot 1)
static void *THISCALL FidgetData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8c, FidgetData_destroy>(self, flags); }
// 0x4a0860 (model index): the model's "Fidget" sequences (CModel::CollectSequences 0x5172b0), the
// "FidgetCrouch" ones moved to the second list. The same model again does nothing; 0 or a model the list
// does not have empties the lists; the list views point at the model's sequences (+0xb4).
static void THISCALL FidgetData_SetModel(BYTE *self, DWORD m)
{
    if (dw(self, 8) == m && m)
        return;
    dw(self, 8) = m;
    BYTE *model = nullptr;
    DWORD seqs = 0;
    if (m) {
        model = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(G_MODEL_LIST), m);
        if (model)
            seqs = ptr_value(model) + 0xb4;
    }
    BYTE *a = self + 0xc, *b = self + 0x28;
    dw(self, 0x20) = seqs;
    dw(self, 0x3c) = seqs;
    if (dw(a, 4)) {
        heap_free(bp(dw(a, 4)));
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
    if (dw(b, 4)) {
        heap_free(bp(dw(b, 4)));
        dw(b, 4) = 0;
        dw(b, 0xc) = 0;
    }
    dw(b, 8) = 0;
    if (!m)
        return;
    fn<DWORD(THISCALL *)(void *, void *, const char *, DWORD)>(0x5172b0)(model, a, glob<const char *>(0x6135e4) /* "Fidget" */, 1);
    for (int i = (int)dw(a, 8) - 1; i >= 0; i--) {
        const char *crouch = glob<const char *>(0x6135d4);   // "FidgetCrouch"
        const DWORD n = (DWORD)lstrlenA(crouch);
        const DWORD seq = dw(bp(dw(a, 4)), (DWORD)i * 4);
        if (fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(crouch, sequence_name(model, seq), n))
            continue;
        DWORD v = dw(bp(dw(a, 4)), (DWORD)i * 4);
        vcall<DWORD>(b, 2, &v);
        remove_at(a, (DWORD)i);
    }
}
// 0x4a0640 (slot 19): the fields reset, the lists emptied
static void THISCALL FidgetData_Reset(BYTE *self)
{
    fn<DWORD(THISCALL *)(void *)>(0x417230)(self);
    FidgetData_SetModel(self, 0);
}
// 0x4a0660 (slot 18): the lists made again for the current model
static void THISCALL FidgetData_Reload(BYTE *self, DWORD)
{
    const DWORD m = dw(self, 8);
    dw(self, 8) = 0;
    FidgetData_SetModel(self, m);
}
// 0x4a0680 (settings, entity, current sequence name): the fidget sequence to play instead of "Idle" (from
// the first list) or "IdleCrouch" (the second) after the settings' "Fidget Sequence Order": 0 the first,
// 1 random, 2 random but never the same twice, 5 the one after the last (never advanced), 3 and 4 not
// implemented (reported). eax: its name (CModel::SequenceName), or the current name unchanged.
static const char *THISCALL FidgetData_Pick(BYTE *self, BYTE *settings, BYTE *e, const char *current)
{
    FidgetData_SetModel(self, fn<DWORD(THISCALL *)(void *)>(0x48b560)(e));
    if (!dw(self, 0x14) && !dw(self, 0x30)) {
        vcall<DWORD>(reporter(), 17, glob<const char *>(0x6135b8) /* "No Fidget Sequences Found" */);
        return current;
    }
    BYTE *list;
    DWORD *last;
    if (same_text(current, glob<const char *>(0x612e70) /* "IdleCrouch" */) && dw(self, 0x30)) {
        list = self + 0x28;
        last = &dw(self, 0x84);
    } else if (same_text(current, glob<const char *>(0x60f8d0) /* "Idle" */) && dw(self, 0x14)) {
        list = self + 0xc;
        last = &dw(self, 0x60);
    } else {
        return current;
    }
    const DWORD *seqs = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(dw(list, 4)));
    DWORD seq;
    switch (dw(settings, 0x10)) {
    case 0:
        *last = 0;
        seq = seqs[0];
        break;
    case 1:
        *last = random_between(0, dw(list, 8) - 1);
        seq = seqs[*last];
        break;
    case 2:
        if (dw(list, 8) > 1) {
            const DWORD old = *last;
            *last = random_between(0, dw(list, 8) - 2);
            if (*last >= old)
                *last = *last + 1;
            seq = seqs[*last];
        } else {
            vcall<DWORD>(reporter(), 17,
                         glob<const char *>(0x613564) /* "Don't use \"Never Same Twice Consecutively\" on model with only one Fidget sequence" */);
            *last = 0;
            seq = seqs[0];
        }
        break;
    case 3:
    case 4:
        vcall<DWORD>(reporter(), 17, glob<const char *>(0x61354c) /* "Not Implemented Yet!!!" */);
        return current;
    case 5:
        if (last[1] >= dw(list, 8))
            last[1] = 0;
        seq = seqs[last[1]];
        break;
    default:
        return current;
    }
    return sequence_name(entity_model(e), seq);
}

// ---- Hooks ---------------------------------------------------------------------------------------------------

ZAX_HOOKS(behavior_rest)
{
    const Hook list[] = {
#include "behavior_rest_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
