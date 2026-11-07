// The simple AIs of 0x56a890-0x5714a4 reimplemented: CSelectiveCollideAI, CSeriesAI, CActionAI, the action
// CSendAIDoneMessage, the shot AIs (CShotAI, CShootOverAI, CTurnToBounceDirAI, CPassThroughShooterAI,
// CExplodeOnImpact) and CSimpleCollideTriggerAI: class getters (both copies: the static registration's and
// slot 2's), deleting destructors (slot 1), RegisterFields and the behaviour slots. Their Create and
// CreateArray are in ai_classes.cpp (CSendAIDoneMessage's here).
//
// AI slots (CLayerPartAI's vtable 0x5ed1d0; names ours [assumption]): 19 reset the fields, 24 Begin
// (entity), 25 Update (entity, seconds), 29 a message (entity, message; al: handled), 31 may the other
// part touch it (part, other, -; al), 33 the same for collisions (part, other, -; eax), 34 collision
// (part, other, three more), 41 debug drawing (entity, view). The actions' slot 27 acts on one target
// (a1, a2, a3, target; al).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CSelectiveCollideAI (0x14, class 0x71335c, vtable 0x5ff5a4): +0x10 "Collides with" (category bits).
//  - CSeriesAI (0x2c, class 0x7134fc, vtable 0x5ff8c4): +0x10 "Restart Series Next Begin", +0x14 "Debug/
//    Current AI", +0x18 "When Done" (enum 0x7134e0: 0 "Repeat Series" starts again, 1 "Do Nothing" (the
//    default) tells the entity that the AI is done, others stop), +0x1c the owned "AIs" (pointer array
//    vtable 0x5ff96c, base 0x5ff978;
//    data +0x20, count +0x24, capacity +0x28, from the pool).
//  - CActionAI (0x18, class 0x713500, version 2, vtable 0x5ff984): +0x10 "Action", +0x14 "Executed Action".
//  - CSendAIDoneMessage (0xc, class 0x713504, vtable 0x5ffa2c, parent CAction): +8 "Target" ("$Trigger").
//  - The shot AIs (0x10 each, no fields; CShotAI and CShootOverAI version 2 under CFlyweightLayerPartAI
//    (vtable 0x5ef1a0), CTurnToBounceDirAI and CPassThroughShooterAI under CShotAI, CExplodeOnImpact
//    under CPassThroughShooterAI; RegisterFields is the empty 0x58ea40).
//  - CSimpleCollideTriggerAI (0x18, class 0x713558, vtable 0x60029c, parent CFlyweightLayerPartAI): +0x10
//    to +0x13 "Triggered By Anything/Players (1)/Enemies/Projectiles", +0x14 "Action" (owned, not deleted
//    by the destructor [confirmed: 0x571080]).
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_SELECTIVE = 0x5ff5a4, VT_SERIES = 0x5ff8c4, VT_ACTION_AI = 0x5ff984, VT_SEND_DONE = 0x5ffa2c, VT_SHOT = 0x5fff54,
                VT_SHOOT_OVER = 0x5ffffc, VT_BOUNCE = 0x6000a4, VT_PASS_THROUGH = 0x60014c, VT_EXPLODE = 0x6001f4,
                VT_SIMPLE_COLLIDE = 0x60029c;
constexpr DWORD EMPTY_REGISTER_FIELDS = 0x58ea40;   // the empty RegisterFields of the classes without fields

// 0x4fd160 (on the AI: layer, the action field, a2, a4): runs the action of a field with the AI as the caller
DWORD run_action_field(void *ai, void *layer, void *field, void *a2, void *a4)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, void *, void *)>(0x4fd160)(ai, layer, field, a2, a4);
}

}  // namespace

// ---- CSelectiveCollideAI ---------------------------------------------------------------------------------

// 0x56a890 (static registration), 0x56aa40 (slot 2)
static CClassInfo *__cdecl SelectiveCollideAI_ClassInfo()
{
    return class_getter(0x71335c, 0x621614 /* "CSelectiveCollideAI" */, 1.0f, layer_part_ai_class, 0x14, 0, 0x56a8e0, 0x56a930, 0x56aaa0);
}
static void SelectiveCollideAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SELECTIVE));
    layer_part_ai_dtor(o);
}
// 0x56a9a0 (slot 1)
static void *THISCALL SelectiveCollideAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, SelectiveCollideAI_destroy>(self, flags); }
// 0x56aaa0. eax: ClassInfo_AddField's.
static DWORD __cdecl SelectiveCollideAI_RegisterFields()
{
    void *f = new_field(0x38, FLAGS_FIELD_CTOR, glob_addr(0x621628) /* "Collides with" */, 0x10, 4, 0x10, glob_addr(0x6bd140), 2, 0, 0);
    return add_field(SelectiveCollideAI_ClassInfo(), f);
}
// 0x56ab30 (slot 33: part, other, -): 0 when the other part is in one of the "Collides with" categories,
// otherwise 1
static DWORD THISCALL SelectiveCollideAI_Slot33(BYTE *self, DWORD, BYTE *other, DWORD)
{
    if (other && (dw(other, 0x1c) & dw(self, 0x10)))
        return 0;
    return 1;
}

// ---- CSeriesAI -------------------------------------------------------------------------------------------

// 0x56c680 (static registration), 0x56c930 (slot 2)
static CClassInfo *__cdecl SeriesAI_ClassInfo()
{
    return class_getter(0x7134fc, 0x62188c /* "CSeriesAI" */, 1.0f, layer_part_ai_class, 0x2c, 0, 0x56c730, 0x56c790, 0x56c990);
}
static void SeriesAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SERIES));
    pool_array_dtor(o + 0x1c, vt(0x5ff978));
    layer_part_ai_dtor(o);
}
// 0x56c820 (slot 1)
static void *THISCALL SeriesAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x2c, SeriesAI_destroy>(self, flags); }
// 0x56c990. eax: ClassInfo_AddField's.
static DWORD __cdecl SeriesAI_RegisterFields()
{
    void *element = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x614e3c) /* "AI" */, 0, 4, 0, layer_part_ai_class(), 1, 0, 0, 0);
    void *f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x615428) /* "AIs" */, 0x24, 4, 0x24, 0x28, 4, 0x28, 0x20, 4, 0x20, 1, element, 1, 0);
    add_field(SeriesAI_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6218b4) /* "Debug/Current AI" */, glob_addr(0x6218c8) /* "+1" */, 0x14, 4, 0x14, 3, 0, 0, 0, 0);
    add_field(SeriesAI_ClassInfo(), f);
    dw(f, 0x38) = glob_addr(0x6be330);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(TEXT_EMPTY));
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x614494) /* "When Done" */, 0x18, 4, 0x18, glob_addr(0x7134e0), 1, glob_addr(0x6141c8) /* "Do Nothing" */, 0, 0);
    add_field(SeriesAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621898) /* "Restart Series Next Begin" */, 0x10, 1, 0x10, 1, 0, 0);
    return add_field(SeriesAI_ClassInfo(), f);
}
// The current AI of the series, 0 past the end
static BYTE *series_current(BYTE *self) { return dw(self, 0x14) < dw(self, 0x24) ? bp(dw(bp(dw(self, 0x20)), dw(self, 0x14) * 4)) : nullptr; }
// 0x56c6d0 (slot 19): "Debug/Current AI" 0 and "When Done" 0 ("Repeat Series"), then CObject_ResetFields
// (its eax)
static DWORD THISCALL SeriesAI_Reset(BYTE *self)
{
    dw(self, 0x14) = 0;
    dw(self, 0x18) = 0;
    return fn<DWORD(THISCALL *)(void *)>(0x417230)(self);
}
// 0x56cc00 (slot 34): the current AI's slot 34. eax: its, or the current index.
static DWORD THISCALL SeriesAI_Collide(BYTE *self, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5)
{
    if (BYTE *ai = series_current(self))
        return vcall<DWORD>(ai, 34, a1, a2, a3, a4, a5);
    return dw(self, 0x14);
}
// 0x56cd50 (entity): the next AI. The current one is told "deactivate" (its slot 29); past the end "When
// Done" 0 starts again at the first, 1 tells the entity (slot 63) that the AI is done; the new current
// AI begins (slot 24). Nothing when the series is already past its end.
// (Past the end "Repeat Series" (0) starts again, "Do Nothing" (1) tells the entity.)
static void THISCALL SeriesAI_Advance(BYTE *self, BYTE *e)
{
    if (dw(self, 0x14) >= dw(self, 0x24))
        return;
    Message m;
    if (BYTE *ai = bp(dw(bp(dw(self, 0x20)), dw(self, 0x14) * 4))) {
        m = message(glob_addr(MSG_DEACTIVATE_AI));
        send_to_ai(ai, e, &m);
    }
    dw(self, 0x14)++;
    if (dw(self, 0x14) >= dw(self, 0x24)) {
        if (dw(self, 0x18) == 0) {
            dw(self, 0x14) = 0;
        } else if (dw(self, 0x18) == 1) {
            m = message(glob_addr(MSG_AI_DONE));
            send_to_part(e, &m);
        }
    }
    if (BYTE *ai = series_current(self))
        vcall<void>(ai, 24, e);
}
// 0x56cc40 (slot 29: entity, message): the current AI's answer; otherwise "AI done" advances the series
// (when it has a current AI); otherwise CLayerPartAI's slot 29. al.
static DWORD THISCALL SeriesAI_Message(BYTE *self, BYTE *e, BYTE *m)
{
    if (BYTE *ai = series_current(self)) {
        const DWORD r = send_to_ai(ai, e, reinterpret_cast<Message *>(m));
        if (r & 0xff)
            return (r & 0xffffff00u) | 1;
    }
    if (dw(m, 4) == *glob<DWORD *>(MSG_AI_DONE) && dw(self, 0x14) < dw(self, 0x24)) {
        SeriesAI_Advance(self, e);
        return 1;
    }
    return layer_part_ai_message(self, e, m);
}
// 0x56ccb0 (slot 24, Begin: entity): past the end or with "Restart Series Next Begin" the first AI again;
// the current AI begins
static void THISCALL SeriesAI_Begin(BYTE *self, BYTE *e)
{
    if (dw(self, 0x14) >= dw(self, 0x24) || fl<BYTE>(self, 0x10))
        dw(self, 0x14) = 0;
    if (dw(self, 0x24))
        if (BYTE *ai = bp(dw(bp(dw(self, 0x20)), dw(self, 0x14) * 4)))
            vcall<void>(ai, 24, e);
}
// 0x56ccf0 (slot 25, Update: entity, seconds): the current AI's update
static void THISCALL SeriesAI_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    if (BYTE *ai = series_current(self))
        vcall<void>(ai, 25, e, seconds);
}
// 0x56cd20 (slot 41: entity, view): the current AI's debug drawing
static void THISCALL SeriesAI_Draw(BYTE *self, BYTE *e, void *view)
{
    if (BYTE *ai = series_current(self))
        ai_debug_draw(ai, e, view);
}

// ---- CActionAI -------------------------------------------------------------------------------------------

// 0x56ce10 (static registration), 0x56cfc0 (slot 2)
static CClassInfo *__cdecl ActionAI_ClassInfo()
{
    return class_getter(0x713500, 0x6218cc /* "CActionAI" */, 2.0f, layer_part_ai_class, 0x18, 0, 0x56ce70, 0x56ceb0, 0x56d020);
}
static void ActionAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_ACTION_AI));
    layer_part_ai_dtor(o);
}
// 0x56cf20 (slot 1)
static void *THISCALL ActionAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, ActionAI_destroy>(self, flags); }
// 0x56d020. eax: ClassInfo_AddField's.
static DWORD __cdecl ActionAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60aed0) /* "Action" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(ActionAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6218d8) /* "Executed Action" */, 0x14, 1, 0x14, 0, 0, 0);
    return add_field(ActionAI_ClassInfo(), f);
}
// 0x56ce60 (slot 19): CObject_ResetFields, then "Executed Action" 0. eax: ResetFields'.
static DWORD THISCALL ActionAI_Reset(BYTE *self)
{
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x417230)(self);
    fl<BYTE>(self, 0x14) = 0;
    return r;
}
// 0x56d130 (slot 25, Update: entity, seconds): the first update runs "Action" (layer, the entity as a2
// and a4, the AI as a3; its eax), every later one tells the entity that the AI is done (slot 63; its eax).
// eax: the AI when there is no action.
static DWORD THISCALL ActionAI_Update(BYTE *self, BYTE *e, DWORD)
{
    if (!fl<BYTE>(self, 0x14)) {
        fl<BYTE>(self, 0x14) = 1;
        if (BYTE *action = bp(dw(self, 0x10)))
            return action_execute(action, bp(dw(e, 0x48)), e, self, e, self, self + 0x10);
        return ptr_value(self);
    }
    Message m = message(glob_addr(MSG_AI_DONE));
    return send_to_part(e, &m);
}

// ---- CSendAIDoneMessage ------------------------------------------------------------------------------------

// 0x56d190 (static registration), 0x56d330 (slot 2)
static CClassInfo *__cdecl SendAIDoneMessage_ClassInfo()
{
    return class_getter(0x713504, 0x6218e8 /* "CSendAIDoneMessage" */, 1.0f, action_class, 0xc, 0, 0x56d1e0, 0x56d220, 0x56d390);
}
static void SendAIDoneMessage_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);   // "Target"
    vtable_and_defaults(o, vt(VT_SEND_DONE));
}
static void SendAIDoneMessage_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SEND_DONE));
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x56d1e0
static void *__cdecl SendAIDoneMessage_Create() { return create_one<0xc, SendAIDoneMessage_construct>(); }
// 0x56d220
static void *__fastcall SendAIDoneMessage_CreateArray(int count, int, int) { return create_array<0xc, SendAIDoneMessage_construct>(count); }
// 0x56d290 (slot 1)
static void *THISCALL SendAIDoneMessage_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, SendAIDoneMessage_destroy>(self, flags); }
// 0x56d390. eax: ClassInfo_AddField's.
static DWORD __cdecl SendAIDoneMessage_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    return add_field(SendAIDoneMessage_ClassInfo(), f);
}
// 0x56d420 (slot 27: a1, a2, a3, target): tells the target that its AI is done (slot 63). al: its answer
// (0 without a target).
static DWORD THISCALL SendAIDoneMessage_Target(BYTE *, DWORD, DWORD, DWORD, BYTE *target)
{
    if (!target)
        return 0;
    Message m = message(glob_addr(MSG_AI_DONE));
    return send_to_part(target, &m);
}

// ---- The shot AIs ------------------------------------------------------------------------------------------

// 0x56fea0 (static registration), 0x570070 (slot 2)
static CClassInfo *__cdecl ShotAI_ClassInfo()
{
    return class_getter(0x713540, 0x621a64 /* "CShotAI" */, 2.0f, flyweight_ai_class, 0x10, 0, 0x56fef0, 0x56ff40, EMPTY_REGISTER_FIELDS);
}
// 0x5700d0 (static registration), 0x5702a0 (slot 2)
static CClassInfo *__cdecl ShootOverAI_ClassInfo()
{
    return class_getter(0x713544, 0x621a6c /* "CShootOverAI" */, 2.0f, flyweight_ai_class, 0x10, 0, 0x570120, 0x570170, EMPTY_REGISTER_FIELDS);
}
// 0x570380 (static registration), 0x5705e0 (slot 2); CShotAI's getter inlined
static CClassInfo *__cdecl TurnToBounceDirAI_ClassInfo()
{
    return class_getter(0x713548, 0x621a7c /* "CTurnToBounceDirAI" */, 1.0f, ShotAI_ClassInfo, 0x10, 0, 0x570430, 0x570490, EMPTY_REGISTER_FIELDS);
}
// 0x570760 (static registration), 0x5709c0 (slot 2)
static CClassInfo *__cdecl PassThroughShooterAI_ClassInfo()
{
    return class_getter(0x71354c, 0x621a90 /* "CPassThroughShooterAI" */, 1.0f, ShotAI_ClassInfo, 0x10, 0, 0x570810, 0x570870,
                        EMPTY_REGISTER_FIELDS);
}
// 0x570ac0 (static registration), 0x570db0 (slot 2); the parents' getters inlined
static CClassInfo *__cdecl ExplodeOnImpact_ClassInfo()
{
    return class_getter(0x713550, 0x621aa8 /* "CExplodeOnImpact" */, 1.0f, PassThroughShooterAI_ClassInfo, 0x10, 0, 0x570bc0, 0x570c30,
                        EMPTY_REGISTER_FIELDS);
}
static void ShotAI_destroy_level(BYTE *o)
{
    vtable_and_free(o, vt(VT_SHOT));
    flyweight_ai_dtor(o);
}
static void ShotAI_destroy(BYTE *o) { ShotAI_destroy_level(o); }
static void ShootOverAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SHOOT_OVER));
    flyweight_ai_dtor(o);
}
static void TurnToBounceDirAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_BOUNCE));
    ShotAI_destroy_level(o);
}
static void PassThroughShooterAI_destroy_level(BYTE *o)
{
    vtable_and_free(o, vt(VT_PASS_THROUGH));
    ShotAI_destroy_level(o);
}
static void PassThroughShooterAI_destroy(BYTE *o) { PassThroughShooterAI_destroy_level(o); }
static void ExplodeOnImpact_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_EXPLODE));
    PassThroughShooterAI_destroy_level(o);
}
// 0x56ffb0, 0x5701e0, 0x570510, 0x5708f0, 0x570cc0 (slot 1)
static void *THISCALL ShotAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, ShotAI_destroy>(self, flags); }
static void *THISCALL ShootOverAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, ShootOverAI_destroy>(self, flags); }
static void *THISCALL TurnToBounceDirAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, TurnToBounceDirAI_destroy>(self, flags); }
static void *THISCALL PassThroughShooterAI_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x10, PassThroughShooterAI_destroy>(self, flags);
}
static void *THISCALL ExplodeOnImpact_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, ExplodeOnImpact_destroy>(self, flags); }

// 0x570300 (CShootOverAI's slot 31: part, other, -): shots (parts with a CShotAI, the other's slot 64)
// pass over it. al (the upper bytes: the AI's; undefined without another part).
static DWORD THISCALL ShootOverAI_MayTouch(BYTE *, BYTE *, BYTE *other, DWORD)
{
    if (other) {
        CClassInfo *c = ShotAI_ClassInfo();
        if (const DWORD ai = vcall<DWORD>(other, 64, c, 0))
            return ai & 0xffffff00u;
        return 1;
    }
    return 1;
}
// 0x5706a0 (CTurnToBounceDirAI's slot 31: part, other, -): everything but the part's shooter (slot 68).
// al (the upper bytes: the shooter's).
static DWORD THISCALL TurnToBounceDirAI_MayTouch(BYTE *, BYTE *part, BYTE *other, DWORD)
{
    const DWORD shooter = vcall<DWORD>(part, 68);
    return (shooter & 0xffffff00u) | (ptr_value(other) != shooter ? 1 : 0);
}
// 0x5706c0 (CTurnToBounceDirAI's slot 34: part, other, three more): a shot whose shooter is an entity
// plays "Laser Bounce.wav" and turns to its direction of motion (slot 116 gives the velocity; slot 104 the
// angle atan2(vy, vx)); then CLayerPartAI's slot 34.
static void THISCALL TurnToBounceDirAI_Collide(BYTE *self, BYTE *part, DWORD other, DWORD a3, DWORD a4, DWORD a5)
{
    BYTE *shooter = vcall<BYTE *>(part, 68);
    if (shooter && is_kind_of(shooter, entity_base_class())) {
        part_layer_add_named(part, glob_addr(0x61ea60) /* "Laser Bounce.wav" */);
        DWORD v[2];
        vcall<void>(part, 116, &v[0], &v[1]);
        const float angle = fstp(table_atan2_bits(v[1], v[0]));
        vcall<void>(part, 104, float_bits(angle), 1);
    }
    fn<void(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x53d140)(self, part, other, a3, a4, a5);
}
// 0x570a80 (CPassThroughShooterAI's slot 31: part, other, -): not the part's shooter (slot 68) and not
// another shot of the same shooter. al (the upper bytes as the exe leaves them).
static DWORD THISCALL PassThroughShooterAI_MayTouch(BYTE *, BYTE *part, BYTE *other, DWORD)
{
    const DWORD shooter = vcall<DWORD>(part, 68);
    if (shooter) {
        if (ptr_value(other) == shooter)
            return shooter & 0xffffff00u;
        if (other) {
            const DWORD s = vcall<DWORD>(other, 68);
            if (s == shooter)
                return s & 0xffffff00u;
            return (s & 0xffffff00u) | 1;
        }
    }
    return (shooter & 0xffffff00u) | 1;
}
// 0x570ec0 (CExplodeOnImpact's slot 34: part, other, point, two more): a destructable part is put at the
// point (slot 41); with hit points left it takes them all as "Unavoidable" damage (slot 99), otherwise the
// world removes it (slot 91) and it dies (slot 97); then CLayerPartAI's slot 34.
static void THISCALL ExplodeOnImpact_Collide(BYTE *self, BYTE *part, DWORD other, DWORD *point, DWORD a4, DWORD a5)
{
    if (is_kind_of(part, entity_destructable_class())) {
        vcall<void>(part, 41, point[0], point[1]);
        if (!c3(hit_points(part), cst(glob_addr(C_ZERO)))) {
            const DWORD type = damage_type_index(glob_addr(0x610324) /* "Unavoidable" */);
            const float hp = fstp(hit_points(part));
            vcall<void>(part, 99, float_bits(hp), type, 0);
        } else {
            vcall<void>(world(), 91, 0, part);
            vcall<void>(part, 97);
        }
    }
    fn<void(THISCALL *)(void *, void *, DWORD, DWORD *, DWORD, DWORD)>(0x53d140)(self, part, other, point, a4, a5);
}

// ---- CSimpleCollideTriggerAI ---------------------------------------------------------------------------------

// 0x570f70 (static registration), 0x571140 (slot 2)
static CClassInfo *__cdecl SimpleCollideTriggerAI_ClassInfo()
{
    return class_getter(0x713558, 0x621abc /* "CSimpleCollideTriggerAI" */, 1.0f, flyweight_ai_class, 0x18, 0, 0x570fc0, 0x571010, 0x5711a0);
}
static void SimpleCollideTriggerAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SIMPLE_COLLIDE));
    flyweight_ai_dtor(o);
}
// 0x571080 (slot 1)
static void *THISCALL SimpleCollideTriggerAI_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x18, SimpleCollideTriggerAI_destroy>(self, flags);
}
// 0x5711a0. eax: ClassInfo_AddField's.
static DWORD __cdecl SimpleCollideTriggerAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60aed0) /* "Action" */, 0x14, 4, 0x14, action_class(), 1, 0, 0, 0);
    add_field(SimpleCollideTriggerAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b4c8) /* "Triggered By/Triggered By Anything" */, 0x10, 1, 0x10, 1, 0, 0);
    add_field(SimpleCollideTriggerAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b4a4) /* "Triggered By/Triggered By Players" */, 0x11, 1, 0x11, 1, 1, 0);
    add_field(SimpleCollideTriggerAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b480) /* "Triggered By/Triggered By Enemies" */, 0x12, 1, 0x12, 1, 0, 0);
    add_field(SimpleCollideTriggerAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b458) /* "Triggered By/Triggered By Projectiles" */, 0x13, 1, 0x13, 1, 0, 0);
    return add_field(SimpleCollideTriggerAI_ClassInfo(), f);
}
// 0x571430 (slot 34: part, other, two more, a flag): unless the flag (low byte) is set, a touch by
// anything, a player, an enemy or a shot (a part with a CShotAI, its slot 64), as the switches say, runs
// "Action" (0x4fd160 with the part's layer, the part as a2 and the other as a4)
static void THISCALL SimpleCollideTriggerAI_Collide(BYTE *self, BYTE *part, BYTE *other, DWORD, DWORD, DWORD flag)
{
    if (flag & 0xff)
        return;
    bool hit = fl<BYTE>(self, 0x10) || (fl<BYTE>(self, 0x11) && (*glob<DWORD *>(CATEGORY_PLAYER) & dw(other, 0x1c))) ||
               (fl<BYTE>(self, 0x12) && (*glob<DWORD *>(CATEGORY_ENEMY) & dw(other, 0x1c)));
    if (!hit && fl<BYTE>(self, 0x13)) {
        CClassInfo *c = class_at(fn_addr(0x56fea0));   // CShotAI
        hit = vcall<DWORD>(other, 64, c, 0) != 0;
    }
    if (hit)
        run_action_field(self, bp(dw(part, 0x48)), self + 0x14, part, other);
}

ZAX_HOOKS(ai_simple)
{
    const Hook list[] = {
#include "ai_simple_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
