// Create and CreateArray of the AI classes and the out-of-line constructors they call, reimplemented
// (see ai_classes.h).
#include "ai_classes.h"
#include "broadcast.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

static void zstring_ctor(BYTE *z) { fn<void(THISCALL *)(void *)>(0x4dec90)(z); }
static void object_reset_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x417230)(o); }
// CAction's constructor (0x41a6b0, Actions): CObject, vtable 0x5ece50, InitFields
static void action_base_ctor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a6b0)(o); }
// PickupAI_Ctor (0x53d8f0; range 0x53c000-0x53ffff, not part of this module)
static void pickup_ai_ctor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x53d8f0)(o); }
static void waypoint_path_ctor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x536e10)(o); }
// 0x535440 (not in docs/functions.csv): a pointer array {vtable 0x5ee15c, 0, 0, 0}
static void ptr_array_5ee15c_ctor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x535440)(o); }
// 0x4a0860: CFidgetData's update from a model (0 empties its lists; AI, behavior_rest.cpp)
static void fidget_data_set_model(BYTE *o, DWORD model) { fn<void(THISCALL *)(void *, DWORD)>(0x4a0860)(o, model); }

// 0x55c390: seeds a random generator object (0x400 bytes: LCG 0x41c64e6d/0x3039 fills 250 dwords,
// then a mask pass); the exe's generators are 0x7124c0 and 0x6d06c0
static void random_seed(void *generator, DWORD seed) { fn<void(THISCALL *)(void *, DWORD)>(0x55c390)(generator, seed); }

template <typename T> static void put(BYTE *o, DWORD offset, T v) { *reinterpret_cast<T *>(o + offset) = v; }
template <typename T> static T get(const BYTE *o, DWORD offset) { return *reinterpret_cast<const T *>(o + offset); }

// One class level of a constructor: its vtable, then Object_InitFields (which sets no values)
static void set_level(BYTE *o, DWORD vftable)
{
    put<DWORD>(o, 0, vftable);
    object_init_fields(o);
}

// Create: a pool block of the class's size, constructed; 0 when the pool has none
static void *create(DWORD size, BYTE *(__fastcall *construct)(BYTE *))
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(size));
    return o ? construct(o) : nullptr;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
static void *create_array(DWORD count, DWORD size, BYTE *(__fastcall *construct)(BYTE *))
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(count * size + 4));
    if (!block)
        return nullptr;
    block[0] = count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)(count - 1) >= 0)
        for (DWORD i = 0; i < count; i++)
            construct(e + i * size);
    return e;
}

static BYTE *__fastcall AIBase_Ctor(BYTE *o);

// The CAIBase and CLayerPartAI levels that most AI constructors start with
static void LayerPartAI_Ctor(BYTE *o)
{
    AIBase_Ctor(o);
    set_level(o, vt(0x5ed1d0));   // CLayerPartAI
}

#define AI_CLASS(name, size)                                                                                         \
    static void *__cdecl name##_Create() { return create(size, name##_Construct); }                                  \
    static void *__fastcall name##_CreateArray(int count, int, int) { return create_array((DWORD)count, size, name##_Construct); }

// ---- Out-of-line constructors -----------------------------------------------------------------------

// 0x494470: CEntityBehavior
static BYTE *__fastcall EntityBehavior_Ctor(BYTE *o)
{
    object_ctor(o);
    set_level(o, vt(0x5f2930));   // CEntityBehavior
    put<DWORD>(o, 0x8, 0);
    return o;
}

// 0x4fd060: CAIBase
static BYTE *__fastcall AIBase_Ctor(BYTE *o)
{
    object_ctor(o);
    set_level(o, vt(0x5f9540));   // CAIBase
    put<BYTE>(o, 0x8, 0);   // "Marked For Deletion"
    return o;
}

// 0x4223f0: CTouchingTriggerAI
static BYTE *__fastcall TouchingTriggerAI_Ctor(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x18);
    set_level(o, vt(0x5ed128));   // CTriggerAI
    set_level(o, vt(0x5edcd0));   // CTouchingTriggerAI
    put<DWORD>(o, 0x30, 0);   // "Touching LayerPart"
    return o;
}

// 0x495310: CEntityBehaviorTurnToAngle
static BYTE *__fastcall EntityBehaviorTurnToAngle_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<float>(o, 0x10, 0.1f);
    set_level(o, vt(0x5f29a8));   // CEntityBehaviorTurnToAngleBase
    set_level(o, vt(0x5f2a28));   // CEntityBehaviorTurnMovingAngle
    put<DWORD>(o, 0x0, vt(0x5f2b28));   // CEntityBehaviorTurnToAngle
    put<DWORD>(o, 0x18, 0);   // "Angle To Turn To"
    object_init_fields(o);
    return o;
}

// 0x498d90: CEntityBehaviorIdle
static BYTE *__fastcall EntityBehaviorIdle_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2e80));   // CEntityBehaviorWait
    zstring_ctor(o + 0x18);
    set_level(o, vt(0x5f2fe8));   // CEntityBehaviorIdle
    return o;
}

// 0x499cc0: CEntityBehaviorPlaySequenceLooped
static BYTE *__fastcall EntityBehaviorPlaySequenceLooped_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    zstring_ctor(o + 0x10);
    set_level(o, vt(0x5f3154));   // CEntityBehaviorPlaySequenceLooped
    put<DWORD>(o, 0xc, 1);   // "Stoping"
    return o;
}

// 0x55e400: CRenderEffectBehavior
static BYTE *__fastcall RenderEffectBehavior_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5fead4));   // CRenderEffectBehavior
    return o;
}

// 0x577db0: CSkeletonBaseAI
static BYTE *__fastcall SkeletonBaseAI_Ctor(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x6008e0));   // CSkeletonBaseAI
    return o;
}

// 0x5780b0: CShootEntityBehavior
static BYTE *__fastcall ShootEntityBehavior_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x60098c));   // CShootEntityBehavior
    put<float>(o, 0x10, 6.2831855f);
    return o;
}

// 0x578780: CPointWeaponAtTargetEntityBehavior
static BYTE *__fastcall PointWeaponAtTargetEntityBehavior_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x600a08));   // CPointWeaponAtTargetEntityBehavior
    return o;
}

// 0x586770: CEntityBehaviorStateSetClosestTarget
static BYTE *__fastcall EntityBehaviorStateSetClosestTarget_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x601654));   // CEntityBehaviorStateSetClosestTarget
    put<DWORD>(o, 0x10, 0);   // "Max Distance"
    put<DWORD>(o, 0x18, 0);   // "Valid Targets"
    put<DWORD>(o, 0x1c, 0);   // "Must Have HPs"
    put<DWORD>(o, 0xc, 0);
    return o;
}

// 0x586e10: CEntityBehaviorQueue
static BYTE *__fastcall EntityBehaviorQueue_Ctor(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<DWORD>(o, 0xc, vt(0x5f3228));   // vtable
    put<DWORD>(o, 0x10, 0);
    put<DWORD>(o, 0x14, 0);   // "Behavior"
    put<DWORD>(o, 0x18, 0);
    set_level(o, vt(0x6016d0));   // CEntityBehaviorQueue
    return o;
}

// ---- Constructors with more than stores and calls ----------------------------------------------------

// 0x4302c0 (26 callers; also CEntityBehaviorMoveToPos's Create, which jumps here): the turn-to-angle
// part, an idle behaviour at +0x1c, the way point path at +0x4c, a string at +0xb8; then the "Path
// Collide Checker" (+0xb4) made if InitFields left none: a pool block of 8 bytes, CObject with the
// vtable 0x5eebe8 (CPathCollideChecker), 0 when the pool has none
static BYTE *__fastcall EntityBehaviorMoveToPos_Ctor(BYTE *o)
{
    EntityBehaviorTurnToAngle_Ctor(o);
    EntityBehaviorIdle_Ctor(o + 0x1c);
    waypoint_path_ctor(o + 0x4c);
    zstring_ctor(o + 0xb8);
    set_level(o, vt(0x5eeb68));   // CEntityBehaviorMoveToPos
    if (!get<DWORD>(o, 0xb4)) {
        BYTE *checker = static_cast<BYTE *>(pool_alloc(8));
        if (checker) {
            object_ctor(checker);
            set_level(checker, vt(0x5eebe8));   // CPathCollideChecker
        }
        put<BYTE *>(o, 0xb4, checker);   // "Path Collide Checker"
    }
    put<DWORD>(o, 0xa8, 0);   // "Idle Begin Called"
    put<DWORD>(o, 0xa0, 0);   // "NumTimesLostGound"
    return o;
}

// 0x4a04e0 (3 callers; also CFidgetData's Create, which jumps here): three pointer arrays (vtable
// 0x5ee168) and four list views (vtable 0x5f405c: +4 list, +8 index array) over them, then
// 0x4a0860 with model 0
static BYTE *__fastcall FidgetData_Ctor(BYTE *o)
{
    object_ctor(o);
    BYTE *a = o + 0xc, *v1 = o + 0x1c, *b = o + 0x28, *v2 = o + 0x38;
    put<DWORD>(a, 0, vt(0x5ee168));
    put<DWORD>(a, 4, 0);
    put<DWORD>(a, 8, 0);
    put<DWORD>(a, 0xc, 0);
    put<DWORD>(v1, 0, vt(0x5f405c));
    put<DWORD>(v1, 4, 0);
    put<BYTE *>(v1, 8, a);
    put<DWORD>(b, 0, vt(0x5ee168));
    put<DWORD>(b, 4, 0);
    put<DWORD>(b, 8, 0);
    put<DWORD>(b, 0xc, 0);
    put<DWORD>(v2, 0, vt(0x5f405c));
    put<DWORD>(v2, 4, 0);
    put<BYTE *>(v2, 8, b);
    put<DWORD>(o, 0x44, vt(0x5ee168));
    put<DWORD>(o, 0x48, 0);
    put<DWORD>(o, 0x4c, 0);
    put<DWORD>(o, 0x50, 0);
    put<DWORD>(o, 0x54, vt(0x5f405c));
    put<BYTE *>(o, 0x58, v1);
    put<BYTE *>(o, 0x5c, a);
    put<DWORD>(o, 0x68, vt(0x5ee168));
    put<DWORD>(o, 0x6c, 0);
    put<DWORD>(o, 0x70, 0);
    put<DWORD>(o, 0x74, 0);
    put<BYTE *>(o, 0x80, b);
    put<DWORD>(o, 0x78, vt(0x5f405c));
    put<BYTE *>(o, 0x7c, v2);
    set_level(o, vt(0x5f4000));   // CFidgetData
    fidget_data_set_model(o, 0);
    return o;
}

// 0x52de80 (4 callers): CEnvelope, "+0x20" 0
static BYTE *__fastcall Envelope_Ctor(BYTE *o)
{
    object_ctor(o);
    set_level(o, vt(0x5f3cbc));   // CEnvelope
    put<DWORD>(o, 0x20, 0);
    return o;
}

// An envelope of a particle system: CEnvelope, +0x20 0, then the derived class's level
static void envelope(BYTE *e, DWORD vftable)
{
    object_ctor(e);
    set_level(e, vt(0x5f3cbc));   // CEnvelope
    put<DWORD>(e, 0x20, 0);
    set_level(e, vftable);
}

// 0x52dd20 (2 callers: CGenericParticleSystemAI's Create and CreateArray): the CLayerPartAI part, five
// envelopes (+0x10 cone angle, +0x34 cone angle, +0x58 facing angle, +0x7c trajectory, +0xa8 speed)
// and a time envelope (+0xd0), the pointer array at +0x114 (vtable 0x5fc534), the one at +0x124
// (0x535440, then vtable 0x5ee138); after InitFields +0x100 1.0, +0x10a 1, +0x10c 0
static BYTE *__fastcall ParticleSystemAI_Ctor(BYTE *o)
{
    LayerPartAI_Ctor(o);
    envelope(o + 0x10, vt(0x5f3d8c));   // CConeAngleEnvelope
    envelope(o + 0x34, vt(0x5f3d8c));   // CConeAngleEnvelope
    envelope(o + 0x58, vt(0x5f3ec4));   // CFacingAngleEnvelope
    envelope(o + 0x7c, vt(0x5f3e5c));   // CTrajectoryEnvelope
    envelope(o + 0xa8, vt(0x5f3df4));   // CSpeedEnvelope
    Envelope_Ctor(o + 0xd0);
    set_level(o + 0xd0, vt(0x5f3d24));   // CTimeEnvelope
    put<DWORD>(o, 0x118, 0);
    put<DWORD>(o, 0x11c, 0);
    put<DWORD>(o, 0x120, 0);
    put<DWORD>(o, 0x114, vt(0x5fc534));
    ptr_array_5ee15c_ctor(o + 0x124);
    put<DWORD>(o, 0x124, vt(0x5ee138));
    set_level(o, vt(0x5fc470));   // CParticleSystemAI
    put<float>(o, 0x100, 1.0f);   // "NumBursts"
    put<BYTE>(o, 0x10a, 1);   // "Enabled"
    put<DWORD>(o, 0x10c, 0);
    return o;
}

// ---- The classes: the constructor as Create and CreateArray inline it --------------------------------

// 0x41c9d0 Create, 0x41ca30 CreateArray: CollideTriggerAI, 0x38 bytes (last vtable 0x5ed080)
static BYTE *__fastcall CollideTriggerAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x18);
    set_level(o, vt(0x5ed128));   // CTriggerAI
    set_level(o, vt(0x5ed080));   // CollideTriggerAI
    put<BYTE>(o, 0x34, 0);
    return o;
}
AI_CLASS(CollideTriggerAI, 0x38)

// 0x41d120 Create, 0x41d170 CreateArray: CPassThroughTriggerAI, 0x24 bytes (last vtable 0x5ed278)
static BYTE *__fastcall PassThroughTriggerAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x18);
    set_level(o, vt(0x5ed128));   // CTriggerAI
    set_level(o, vt(0x5ed278));   // CPassThroughTriggerAI
    return o;
}
AI_CLASS(PassThroughTriggerAI, 0x24)

// 0x41fc20 Create, 0x41fc60 CreateArray: CRelayAI, 0x18 bytes (last vtable 0x5ed80c)
static BYTE *__fastcall RelayAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ed80c));   // CRelayAI
    return o;
}
AI_CLASS(RelayAI, 0x18)

// 0x4203a0 Create, 0x4203e0 CreateArray: CRepeatTimerTriggerAI, 0x20 bytes (last vtable 0x5ed9e8)
static BYTE *__fastcall RepeatTimerTriggerAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ed9e8));   // CRepeatTimerTriggerAI
    return o;
}
AI_CLASS(RepeatTimerTriggerAI, 0x20)

// 0x4209a0 Create, 0x4209e0 CreateArray: CSpawnPointAI, 0x1c bytes (last vtable 0x5eda94)
static BYTE *__fastcall SpawnPointAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5eda94));   // CSpawnPointAI
    return o;
}
AI_CLASS(SpawnPointAI, 0x1c)

// 0x420f30 Create, 0x420f60 CreateArray: CTouchingOvalTriggerAI, 0x44 bytes (last vtable 0x5edb58)
static BYTE *__fastcall TouchingOvalTriggerAI_Construct(BYTE *o)
{
    TouchingTriggerAI_Ctor(o);
    set_level(o, vt(0x5edb58));   // CTouchingOvalTriggerAI
    return o;
}
AI_CLASS(TouchingOvalTriggerAI, 0x44)

// 0x4218d0 Create, 0x421900 CreateArray: CTouchingPolygonTriggerAI, 0x38 bytes (last vtable 0x5edc20)
static BYTE *__fastcall TouchingPolygonTriggerAI_Construct(BYTE *o)
{
    TouchingTriggerAI_Ctor(o);
    set_level(o, vt(0x5edc20));   // CTouchingPolygonTriggerAI
    return o;
}
AI_CLASS(TouchingPolygonTriggerAI, 0x38)

// 0x422f10 Create, 0x422f50 CreateArray: CAttractCompassAI, 0x10 bytes (last vtable 0x5edd80)
static BYTE *__fastcall AttractCompassAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5edd80));   // CAttractCompassAI
    return o;
}
AI_CLASS(AttractCompassAI, 0x10)

// 0x42d660 Create, 0x42d6b0 CreateArray: CAmbientSoundAI, 0x28 bytes (last vtable 0x5ee7bc)
static BYTE *__fastcall AmbientSoundAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    put<DWORD>(o, 0x20, 0);   // "Layer Sound Handle"
    set_level(o, vt(0x5ee7bc));   // CAmbientSoundAI
    put<DWORD>(o, 0x24, 0);
    return o;
}
AI_CLASS(AmbientSoundAI, 0x28)

// 0x42e150 Create, 0x42e1a0 CreateArray: CApproachTargetAI, 0xd8 bytes (last vtable 0x5ee868)
static BYTE *__fastcall ApproachTargetAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ee910));   // CAttackAI
    EntityBehaviorMoveToPos_Ctor(o + 0x10);
    set_level(o, vt(0x5ee868));   // CApproachTargetAI
    return o;
}
AI_CLASS(ApproachTargetAI, 0xd8)

// 0x42f360 Create, 0x42f3b0 CreateArray: CBehaviorActivity, 0x34 bytes (last vtable 0x5eeabc)
static BYTE *__fastcall BehaviorActivity_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    EntityBehaviorQueue_Ctor(o + 0x10);
    set_level(o, vt(0x5eeabc));   // CBehaviorActivity
    return o;
}
AI_CLASS(BehaviorActivity, 0x34)

// 0x42f730 Create, 0x42f750 CreateArray: CEntityBehaviorMoveToPos, 0xbc bytes
static BYTE *__fastcall EntityBehaviorMoveToPos_Construct(BYTE *o)
{
    EntityBehaviorMoveToPos_Ctor(o);
    return o;
}
AI_CLASS(EntityBehaviorMoveToPos, 0xbc)

// 0x467180 Create, 0x4671d0 CreateArray: CBounceAI, 0x14 bytes (last vtable 0x5ef0f8)
static BYTE *__fastcall BounceAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5ef0f8));   // CBounceAI
    return o;
}
AI_CLASS(BounceAI, 0x14)

// 0x46b810 Create, 0x46b870 CreateArray: CChaseInvestigateAI, 0x110 bytes (last vtable 0x5ef95c)
static BYTE *__fastcall ChaseInvestigateAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5efa04));   // CInvestigateAI
    EntityBehaviorMoveToPos_Ctor(o + 0x10);
    EntityBehaviorTurnToAngle_Ctor(o + 0xcc);
    set_level(o, vt(0x5ef95c));   // CChaseInvestigateAI
    return o;
}
AI_CLASS(ChaseInvestigateAI, 0x110)

// 0x46c6f0 Create, 0x46c740 CreateArray: CChasePursueAI, 0xd4 bytes (last vtable 0x5efaac)
static BYTE *__fastcall ChasePursueAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5efb54));   // CPursueAI
    EntityBehaviorMoveToPos_Ctor(o + 0x10);
    set_level(o, vt(0x5efaac));   // CChasePursueAI
    return o;
}
AI_CLASS(ChasePursueAI, 0xd4)

// 0x46e120 Create, 0x46e190 CreateArray: CCompanionAI, 0x10c bytes (last vtable 0x5efef0)
static BYTE *__fastcall CompanionAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    EntityBehaviorMoveToPos_Ctor(o + 0x10);
    put<DWORD>(o, 0xcc, 0);
    put<DWORD>(o, 0xd0, 0);   // "Attack AI"
    put<DWORD>(o, 0xe4, 0);
    EntityBehaviorStateSetClosestTarget_Ctor(o + 0xec);
    set_level(o, vt(0x5efef0));   // CCompanionAI
    return o;
}
AI_CLASS(CompanionAI, 0x10c)

// 0x472f30 Create, 0x472f80 CreateArray: CFireContinuousFireWeaponBaseAI, 0x34 bytes (last vtable 0x5f050c)
static BYTE *__fastcall FireContinuousFireWeaponBaseAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x20);
    set_level(o, vt(0x5f050c));   // CFireContinuousFireWeaponBaseAI
    put<DWORD>(o, 0x28, 0xffffffff);
    put<BYTE>(o, 0x2c, 255);
    return o;
}
AI_CLASS(FireContinuousFireWeaponBaseAI, 0x34)

// 0x474880 Create, 0x4748f0 CreateArray: CFireElectricalWeaponAI, 0x40 bytes (last vtable 0x5f05bc)
static BYTE *__fastcall FireElectricalWeaponAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x20);
    set_level(o, vt(0x5f050c));   // CFireContinuousFireWeaponBaseAI
    put<DWORD>(o, 0x28, 0xffffffff);
    put<BYTE>(o, 0x2c, 255);
    zstring_ctor(o + 0x3c);
    set_level(o, vt(0x5f05bc));   // CFireElectricalWeaponAI
    return o;
}
AI_CLASS(FireElectricalWeaponAI, 0x40)

// 0x475240 Create, 0x4752b0 CreateArray: CFireParticleWeaponAI, 0x40 bytes (last vtable 0x5f0668)
static BYTE *__fastcall FireParticleWeaponAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x20);
    set_level(o, vt(0x5f050c));   // CFireContinuousFireWeaponBaseAI
    put<DWORD>(o, 0x28, 0xffffffff);
    put<BYTE>(o, 0x2c, 255);
    zstring_ctor(o + 0x3c);
    set_level(o, vt(0x5f0668));   // CFireParticleWeaponAI
    return o;
}
AI_CLASS(FireParticleWeaponAI, 0x40)

// 0x4764f0 Create, 0x476540 CreateArray: CDamageExpandingRadiusAI, 0x34 bytes (last vtable 0x5f08c8)
static BYTE *__fastcall DamageExpandingRadiusAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f08c8));   // CDamageExpandingRadiusAI
    put<DWORD>(o, 0x28, 0);
    put<DWORD>(o, 0x2c, 0);
    return o;
}
AI_CLASS(DamageExpandingRadiusAI, 0x34)

// 0x477d30 Create, 0x477d80 CreateArray: CDeathBehaviorListDefinition, 0x18 bytes (last vtable 0x5f0ba4)
static BYTE *__fastcall DeathBehaviorListDefinition_Construct(BYTE *o)
{
    object_ctor(o);
    put<DWORD>(o, 0x8, vt(0x5f0c00));   // vtable
    put<DWORD>(o, 0xc, 0);
    put<DWORD>(o, 0x10, 0);   // "Behaviors"
    put<DWORD>(o, 0x14, 0);
    set_level(o, vt(0x5f0ba4));   // CDeathBehaviorListDefinition
    return o;
}
AI_CLASS(DeathBehaviorListDefinition, 0x18)

// 0x47b910 Create, 0x47b940 CreateArray: CPickupDeployedItemAI, 0x30 bytes (last vtable 0x5f12f8)
static BYTE *__fastcall PickupDeployedItemAI_Construct(BYTE *o)
{
    pickup_ai_ctor(o);
    set_level(o, vt(0x5f12f8));   // CPickupDeployedItemAI
    return o;
}
AI_CLASS(PickupDeployedItemAI, 0x30)

// 0x481760 Create, 0x4817b0 CreateArray: CDoorAI, 0x48 bytes (last vtable 0x5f1740)
static BYTE *__fastcall DoorAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f1740));   // CDoorAI
    put<DWORD>(o, 0x40, 0);   // "Timeout Time Left"
    return o;
}
AI_CLASS(DoorAI, 0x48)

// 0x494790 Create, 0x4947e0 CreateArray: CEntityBehaviorTurnMovingAngle, 0x14 bytes (last vtable 0x5f2a28)
static BYTE *__fastcall EntityBehaviorTurnMovingAngle_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<float>(o, 0x10, 0.1f);
    set_level(o, vt(0x5f29a8));   // CEntityBehaviorTurnToAngleBase
    set_level(o, vt(0x5f2a28));   // CEntityBehaviorTurnMovingAngle
    return o;
}
AI_CLASS(EntityBehaviorTurnMovingAngle, 0x14)

// 0x494a90 Create, 0x494ae0 CreateArray: CEntityBehaviorTurnAimingAngle, 0x14 bytes (last vtable 0x5f2aa8)
static BYTE *__fastcall EntityBehaviorTurnAimingAngle_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<float>(o, 0x10, 0.1f);
    set_level(o, vt(0x5f29a8));   // CEntityBehaviorTurnToAngleBase
    set_level(o, vt(0x5f2aa8));   // CEntityBehaviorTurnAimingAngle
    return o;
}
AI_CLASS(EntityBehaviorTurnAimingAngle, 0x14)

// 0x494de0 Create, 0x494e40 CreateArray: CEntityBehaviorTurnToAngle, 0x1c bytes (last vtable 0x5f2b28)
static BYTE *__fastcall EntityBehaviorTurnToAngle_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<float>(o, 0x10, 0.1f);
    set_level(o, vt(0x5f29a8));   // CEntityBehaviorTurnToAngleBase
    set_level(o, vt(0x5f2a28));   // CEntityBehaviorTurnMovingAngle
    put<DWORD>(o, 0x0, vt(0x5f2b28));   // CEntityBehaviorTurnToAngle
    put<DWORD>(o, 0x18, 0);   // "Angle To Turn To"
    object_init_fields(o);
    return o;
}
AI_CLASS(EntityBehaviorTurnToAngle, 0x1c)

// 0x495760 Create, 0x4957b0 CreateArray: CEntityBehaviorTurnToTarget, 0x24 bytes (last vtable 0x5f2ba8)
static BYTE *__fastcall EntityBehaviorTurnToTarget_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<float>(o, 0x10, 0.1f);
    set_level(o, vt(0x5f29a8));   // CEntityBehaviorTurnToAngleBase
    set_level(o, vt(0x5f2aa8));   // CEntityBehaviorTurnAimingAngle
    set_level(o, vt(0x5f2ba8));   // CEntityBehaviorTurnToTarget
    return o;
}
AI_CLASS(EntityBehaviorTurnToTarget, 0x24)

// 0x496370 Create, 0x4963b0 CreateArray: CEntityBehaviorSetRotationSequence, 0x10 bytes (last vtable 0x5f2c28)
static BYTE *__fastcall EntityBehaviorSetRotationSequence_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    zstring_ctor(o + 0xc);
    set_level(o, vt(0x5f2c28));   // CEntityBehaviorSetRotationSequence
    return o;
}
AI_CLASS(EntityBehaviorSetRotationSequence, 0x10)

// 0x4966d0 Create, 0x496720 CreateArray: CEntityBehaviorPlaySequenceOnce, 0x28 bytes (last vtable 0x5f2ca0)
static BYTE *__fastcall EntityBehaviorPlaySequenceOnce_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    zstring_ctor(o + 0xc);
    set_level(o, vt(0x5f2c28));   // CEntityBehaviorSetRotationSequence
    zstring_ctor(o + 0x1c);
    set_level(o, vt(0x5f2ca0));   // CEntityBehaviorPlaySequenceOnce
    return o;
}
AI_CLASS(EntityBehaviorPlaySequenceOnce, 0x28)

// 0x4972e0 Create, 0x497310 CreateArray: CEntityBehaviorSetPlayRatio, 0x10 bytes (last vtable 0x5f2d18)
static BYTE *__fastcall EntityBehaviorSetPlayRatio_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2d18));   // CEntityBehaviorSetPlayRatio
    return o;
}
AI_CLASS(EntityBehaviorSetPlayRatio, 0x10)

// 0x4975b0 Create, 0x4975e0 CreateArray: CEntityBehaviorMarkForDeletion, 0xc bytes (last vtable 0x5f2d90)
static BYTE *__fastcall EntityBehaviorMarkForDeletion_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2d90));   // CEntityBehaviorMarkForDeletion
    return o;
}
AI_CLASS(EntityBehaviorMarkForDeletion, 0xc)

// 0x4977f0 Create, 0x497830 CreateArray: CEntityBehaviorClearFlags, 0x10 bytes (last vtable 0x5f2e08)
static BYTE *__fastcall EntityBehaviorClearFlags_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2e08));   // CEntityBehaviorClearFlags
    put<DWORD>(o, 0xc, 0);   // "Active"
    return o;
}
AI_CLASS(EntityBehaviorClearFlags, 0x10)

// 0x497ca0 Create, 0x497cd0 CreateArray: CEntityBehaviorWait, 0x14 bytes (last vtable 0x5f2e80)
static BYTE *__fastcall EntityBehaviorWait_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2e80));   // CEntityBehaviorWait
    return o;
}
AI_CLASS(EntityBehaviorWait, 0x14)

// 0x498370 Create, 0x4983c0 CreateArray: CEntityBehaviorFade, 0x18 bytes (last vtable 0x5f2f70)
static BYTE *__fastcall EntityBehaviorFade_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2e80));   // CEntityBehaviorWait
    set_level(o, vt(0x5f2ef8));   // CEntityBehaviorTransition
    set_level(o, vt(0x5f2f70));   // CEntityBehaviorFade
    return o;
}
AI_CLASS(EntityBehaviorFade, 0x18)

// 0x498720 Create, 0x498770 CreateArray: CEntityBehaviorIdle, 0x30 bytes (last vtable 0x5f2fe8)
static BYTE *__fastcall EntityBehaviorIdle_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f2e80));   // CEntityBehaviorWait
    zstring_ctor(o + 0x18);
    set_level(o, vt(0x5f2fe8));   // CEntityBehaviorIdle
    return o;
}
AI_CLASS(EntityBehaviorIdle, 0x30)

// 0x499360 Create, 0x499390 CreateArray: CEntityBehaviorSetCurFrame, 0x10 bytes (last vtable 0x5f3064)
static BYTE *__fastcall EntityBehaviorSetCurFrame_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f3064));   // CEntityBehaviorSetCurFrame
    return o;
}
AI_CLASS(EntityBehaviorSetCurFrame, 0x10)

// 0x499590 Create, 0x4995c0 CreateArray: CEntityBehaviorExecuteAction, 0x10 bytes (last vtable 0x5f30dc)
static BYTE *__fastcall EntityBehaviorExecuteAction_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x5f30dc));   // CEntityBehaviorExecuteAction
    return o;
}
AI_CLASS(EntityBehaviorExecuteAction, 0x10)

// 0x499800 Create, 0x499840 CreateArray: CEntityBehaviorPlaySequenceLooped, 0x14 bytes (last vtable 0x5f3154)
static BYTE *__fastcall EntityBehaviorPlaySequenceLooped_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    zstring_ctor(o + 0x10);
    set_level(o, vt(0x5f3154));   // CEntityBehaviorPlaySequenceLooped
    put<DWORD>(o, 0xc, 1);   // "Stoping"
    return o;
}
AI_CLASS(EntityBehaviorPlaySequenceLooped, 0x14)

// 0x49a000 Create, 0x49a050 CreateArray: CEntityBehaviorStack, 0x20 bytes (last vtable 0x5f31cc)
static BYTE *__fastcall EntityBehaviorStack_Construct(BYTE *o)
{
    object_ctor(o);
    put<DWORD>(o, 0x14, 0);
    put<DWORD>(o, 0x18, 0);   // "BehaviorList"
    put<DWORD>(o, 0x1c, 0);
    put<DWORD>(o, 0x10, vt(0x5f3228));   // vtable
    put<DWORD>(o, 0x0, vt(0x5f31cc));   // CEntityBehaviorStack
    put<BYTE>(o, 0x8, 0);   // "InProcess"
    put<DWORD>(o, 0xc, 0);   // "Blocking Priority"
    object_init_fields(o);
    return o;
}
AI_CLASS(EntityBehaviorStack, 0x20)

// 0x49ffa0 Create, 0x49ffd0 CreateArray: CFidgetSettings, 0x14 bytes (last vtable 0x5f3f94)
static BYTE *__fastcall FidgetSettings_Construct(BYTE *o)
{
    object_ctor(o);
    set_level(o, vt(0x5f3f94));   // CFidgetSettings
    return o;
}
AI_CLASS(FidgetSettings, 0x14)

// 0x4a0380 Create, 0x4a03a0 CreateArray: CFidgetData, 0x8c bytes
static BYTE *__fastcall FidgetData_Construct(BYTE *o)
{
    FidgetData_Ctor(o);
    return o;
}
AI_CLASS(FidgetData, 0x8c)

// 0x4a4fa0 Create, 0x4a5010 CreateArray: FollowAI, 0xf8 bytes (last vtable 0x5f422c)
static BYTE *__fastcall FollowAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f42d4));   // CPatrolBaseAI
    zstring_ctor(o + 0x10);
    EntityBehaviorMoveToPos_Ctor(o + 0x14);
    EntityBehaviorTurnToAngle_Ctor(o + 0xd0);
    set_level(o, vt(0x5f422c));   // FollowAI
    put<DWORD>(o, 0xec, 0);   // "Current Leader"
    return o;
}
AI_CLASS(FollowAI, 0xf8)

// 0x4ca4e0 Create, 0x4ca520 CreateArray: CGravityAI, 0x18 bytes (last vtable 0x5f6cac)
static BYTE *__fastcall GravityAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f6cac));   // CGravityAI
    return o;
}
AI_CLASS(GravityAI, 0x18)

// 0x4ca9c0 Create, 0x4caa10 CreateArray: CDrawTimeoutOvalAI, 0x24 bytes (last vtable 0x5f6d58)
static BYTE *__fastcall DrawTimeoutOvalAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x1c);
    set_level(o, vt(0x5f6d58));   // CDrawTimeoutOvalAI
    return o;
}
AI_CLASS(DrawTimeoutOvalAI, 0x24)

// 0x4cae90 Create, 0x4caee0 CreateArray: CDrawTimeoutRectAI, 0x2c bytes (last vtable 0x5f6e00)
static BYTE *__fastcall DrawTimeoutRectAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x24);
    set_level(o, vt(0x5f6e00));   // CDrawTimeoutRectAI
    return o;
}
AI_CLASS(DrawTimeoutRectAI, 0x2c)

// 0x4cb280 Create, 0x4cb2d0 CreateArray: CPlaySequenceLoopedAI, 0x28 bytes (last vtable 0x5f6ea8)
static BYTE *__fastcall PlaySequenceLoopedAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    EntityBehaviorPlaySequenceLooped_Ctor(o + 0x10);
    set_level(o, vt(0x5f6ea8));   // CPlaySequenceLoopedAI
    return o;
}
AI_CLASS(PlaySequenceLoopedAI, 0x28)

// 0x4cb6b0 Create, 0x4cb6f0 CreateArray: CSimultaneousAIsAI, 0x18 bytes (last vtable 0x5f6f50)
static BYTE *__fastcall SimultaneousAIsAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f6f50));   // CSimultaneousAIsAI
    return o;
}
AI_CLASS(SimultaneousAIsAI, 0x18)

// 0x4cbb30 Create, 0x4cbb70 CreateArray: CWhileAI, 0x1c bytes (last vtable 0x5f6ff8)
static BYTE *__fastcall WhileAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f6ff8));   // CWhileAI
    return o;
}
AI_CLASS(WhileAI, 0x1c)

// 0x4cbfd0 Create, 0x4cc010 CreateArray: CWaitAI, 0x20 bytes (last vtable 0x5f70a0)
static BYTE *__fastcall WaitAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f70a0));   // CWaitAI
    return o;
}
AI_CLASS(WaitAI, 0x20)

// 0x4cc4e0 Create, 0x4cc530 CreateArray: CIdleAI, 0x40 bytes (last vtable 0x5f7148)
static BYTE *__fastcall IdleAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    EntityBehaviorIdle_Ctor(o + 0x10);
    set_level(o, vt(0x5f7148));   // CIdleAI
    return o;
}
AI_CLASS(IdleAI, 0x40)

// 0x4cc890 Create, 0x4cc8d0 CreateArray: CForceUpdatingAI, 0x18 bytes (last vtable 0x5f71f0)
static BYTE *__fastcall ForceUpdatingAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f71f0));   // CForceUpdatingAI
    return o;
}
AI_CLASS(ForceUpdatingAI, 0x18)

// 0x4ccca0 Create, 0x4ccce0 CreateArray: CLimitedTimeAI, 0x20 bytes (last vtable 0x5f7298)
static BYTE *__fastcall LimitedTimeAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f7298));   // CLimitedTimeAI
    return o;
}
AI_CLASS(LimitedTimeAI, 0x20)

// 0x4cd1d0 Create, 0x4cd220 CreateArray: CHandleMessageAI, 0x18 bytes (last vtable 0x5f7340)
static BYTE *__fastcall HandleMessageAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    zstring_ctor(o + 0x10);
    set_level(o, vt(0x5f7340));   // CHandleMessageAI
    return o;
}
AI_CLASS(HandleMessageAI, 0x18)

// 0x4cfa60 Create, 0x4cfab0 CreateArray: CGeneratorAI, 0x48 bytes (last vtable 0x5f77ec)
static BYTE *__fastcall GeneratorAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x24);
    set_level(o, vt(0x5f77ec));   // CGeneratorAI
    return o;
}
AI_CLASS(GeneratorAI, 0x48)

// 0x4d0a00 Create, 0x4d0a60 CreateArray: CGoToAI, 0x118 bytes (last vtable 0x5f7894)
static BYTE *__fastcall GoToAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    zstring_ctor(o + 0x14);
    EntityBehaviorMoveToPos_Ctor(o + 0x1c);
    EntityBehaviorTurnToAngle_Ctor(o + 0xdc);
    zstring_ctor(o + 0x114);
    set_level(o, vt(0x5f7894));   // CGoToAI
    return o;
}
AI_CLASS(GoToAI, 0x118)

// 0x4e39c0 Create, 0x4e3a00 CreateArray: CLandscapeAI, 0x34 bytes (last vtable 0x5f8904)
static BYTE *__fastcall LandscapeAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f8904));   // CLandscapeAI
    return o;
}
AI_CLASS(LandscapeAI, 0x34)

// 0x4fe2e0 Create, 0x4fe330 CreateArray: CLightBeamAI, 0x18 bytes (last vtable 0x5f9798)
static BYTE *__fastcall LightBeamAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5f9798));   // CLightBeamAI
    return o;
}
AI_CLASS(LightBeamAI, 0x18)

// 0x508730 Create, 0x508780 CreateArray: CPointOfIntrestAI, 0x1c bytes (last vtable 0x5f9ef4)
static BYTE *__fastcall PointOfIntrestAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f9ef4));   // CPointOfIntrestAI
    object_reset_fields(o);
    return o;
}
AI_CLASS(PointOfIntrestAI, 0x1c)

// 0x522b90 Create, 0x522bf0 CreateArray: CNormalAttackAI, 0x4c bytes (last vtable 0x5fb6e0)
static BYTE *__fastcall NormalAttackAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ee910));   // CAttackAI
    ShootEntityBehavior_Ctor(o + 0x10);
    PointWeaponAtTargetEntityBehavior_Ctor(o + 0x24);
    put<DWORD>(o, 0x30, 0);   // "MoveBehavior"
    set_level(o, vt(0x5fb6e0));   // CNormalAttackAI
    return o;
}
AI_CLASS(NormalAttackAI, 0x4c)

// 0x528c10 Create, 0x528c70 CreateArray: CGravityParticleAI, 0x34 bytes (last vtable 0x5fbe10)
static BYTE *__fastcall GravityParticleAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fbd58));   // CParticleAI
    put<DWORD>(o, 0x10, 0);   // "X Center Pos"
    put<DWORD>(o, 0x14, 0);   // "Y Center Pos"
    put<DWORD>(o, 0x18, 0);   // "Z Center Pos"
    put<BYTE>(o, 0x1d, 0);   // "Animated"
    put<BYTE>(o, 0x1e, 0);   // "Obey Sequence FPS"
    set_level(o, vt(0x5fbe10));   // CGravityParticleAI
    return o;
}
AI_CLASS(GravityParticleAI, 0x34)

// 0x529740 Create, 0x5297a0 CreateArray: CFloatParticleAI, 0x24 bytes (last vtable 0x5fbed8)
static BYTE *__fastcall FloatParticleAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fbd58));   // CParticleAI
    put<DWORD>(o, 0x10, 0);   // "X Center Pos"
    put<DWORD>(o, 0x14, 0);   // "Y Center Pos"
    put<DWORD>(o, 0x18, 0);   // "Z Center Pos"
    put<BYTE>(o, 0x1d, 0);   // "Animated"
    put<BYTE>(o, 0x1e, 0);   // "Obey Sequence FPS"
    set_level(o, vt(0x5fbed8));   // CFloatParticleAI
    return o;
}
AI_CLASS(FloatParticleAI, 0x24)

// 0x529a90 Create, 0x529af0 CreateArray: CSwirlParticleAI, 0x34 bytes (last vtable 0x5fbf90)
static BYTE *__fastcall SwirlParticleAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fbd58));   // CParticleAI
    put<DWORD>(o, 0x10, 0);   // "X Center Pos"
    put<DWORD>(o, 0x14, 0);   // "Y Center Pos"
    put<DWORD>(o, 0x18, 0);   // "Z Center Pos"
    put<BYTE>(o, 0x1d, 0);   // "Animated"
    put<BYTE>(o, 0x1e, 0);   // "Obey Sequence FPS"
    set_level(o, vt(0x5fbf90));   // CSwirlParticleAI
    return o;
}
AI_CLASS(SwirlParticleAI, 0x34)

// 0x52f040 Create, 0x52f070 CreateArray: CGenericParticleSystemAI, 0x148 bytes (last vtable 0x5fc554)
static BYTE *__fastcall GenericParticleSystemAI_Construct(BYTE *o)
{
    ParticleSystemAI_Ctor(o);
    set_level(o, vt(0x5fc554));   // CGenericParticleSystemAI
    return o;
}
AI_CLASS(GenericParticleSystemAI, 0x148)

// 0x53a990 Create, 0x53aa00 CreateArray: COldBadPatrolAI, 0x34 bytes (last vtable 0x5fc8d4)
static BYTE *__fastcall OldBadPatrolAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fc8d4));   // COldBadPatrolAI
    put<DWORD>(o, 0x14, 0x78);
    put<DWORD>(o, 0x18, 0xc8);
    random_seed(glob<void *>(0x6d06c0), 0x23bdc6);   // reseeds the generator 0x6d06c0 (2342342)
    put<BYTE>(o, 0x2c, 0);
    put<float>(o, 0x30, -1.0f);   // "Return Home Dist"
    return o;
}
AI_CLASS(OldBadPatrolAI, 0x34)

// 0x53b400 Create, 0x53b450 CreateArray: CPatrolAreaAI, 0xd8 bytes (last vtable 0x5fc984)
static BYTE *__fastcall PatrolAreaAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f42d4));   // CPatrolBaseAI
    EntityBehaviorMoveToPos_Ctor(o + 0x14);
    set_level(o, vt(0x5fc984));   // CPatrolAreaAI
    return o;
}
AI_CLASS(PatrolAreaAI, 0xd8)

// 0x53be30 Create, 0x53be90 CreateArray: CPatrolPathAI, 0xf4 bytes (last vtable 0x5fca38)
static BYTE *__fastcall PatrolPathAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f42d4));   // CPatrolBaseAI
    zstring_ctor(o + 0x20);
    EntityBehaviorMoveToPos_Ctor(o + 0x34);
    set_level(o, vt(0x5fca38));   // CPatrolPathAI
    put<DWORD>(o, 0x24, 0);   // "PathPolygonLayerPart"
    return o;
}
AI_CLASS(PatrolPathAI, 0xf4)

// 0x544df0 Create, 0x544e40 CreateArray: CPlayerWalkingWeaponAI, 0x14 bytes (last vtable 0x5fcfd8)
static BYTE *__fastcall PlayerWalkingWeaponAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fd080));   // CSameAsParentAI
    set_level(o, vt(0x5fcfd8));   // CPlayerWalkingWeaponAI
    return o;
}
AI_CLASS(PlayerWalkingWeaponAI, 0x14)

// 0x5450b0 Create, 0x545100 CreateArray: CPlayerWalkingItemAI, 0x1c bytes (last vtable 0x5fd128)
static BYTE *__fastcall PlayerWalkingItemAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fd080));   // CSameAsParentAI
    set_level(o, vt(0x5fd128));   // CPlayerWalkingItemAI
    return o;
}
AI_CLASS(PlayerWalkingItemAI, 0x1c)

// 0x561d30 Create, 0x561d70 CreateArray: CSameAsParentAI, 0x14 bytes (last vtable 0x5fd080)
static BYTE *__fastcall SameAsParentAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fd080));   // CSameAsParentAI
    return o;
}
AI_CLASS(SameAsParentAI, 0x14)

// 0x565060 Create, 0x5650b0 CreateArray: CScanAreaAI, 0x4c bytes (last vtable 0x5ff1e0)
static BYTE *__fastcall ScanAreaAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f42d4));   // CPatrolBaseAI
    zstring_ctor(o + 0x10);
    set_level(o, vt(0x5ff1e0));   // CScanAreaAI
    return o;
}
AI_CLASS(ScanAreaAI, 0x4c)

// 0x566060 Create, 0x5660f0 CreateArray: CScanInvestigateAI, 0x5c bytes (last vtable 0x5ff288)
static BYTE *__fastcall ScanInvestigateAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f861c));   // CInvestigateAreaAI
    LayerPartAI_Ctor(o + 0x10);
    set_level(o + 0x10, vt(0x5f42d4));   // CPatrolBaseAI
    zstring_ctor(o + 0x20);
    set_level(o + 0x10, vt(0x5ff1e0));   // CScanAreaAI
    set_level(o, vt(0x5ff288));   // CScanInvestigateAI
    return o;
}
AI_CLASS(ScanInvestigateAI, 0x5c)

// 0x56a8e0 Create, 0x56a930 CreateArray: CSelectiveCollideAI, 0x14 bytes (last vtable 0x5ff5a4)
static BYTE *__fastcall SelectiveCollideAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    put<DWORD>(o, 0x10, 0);   // "Collides with"
    set_level(o, vt(0x5ff5a4));   // CSelectiveCollideAI
    return o;
}
AI_CLASS(SelectiveCollideAI, 0x14)

// 0x56c730 Create, 0x56c790 CreateArray: CSeriesAI, 0x2c bytes (last vtable 0x5ff8c4)
static BYTE *__fastcall SeriesAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    put<DWORD>(o, 0x20, 0);
    put<DWORD>(o, 0x24, 0);   // "AIs"
    put<DWORD>(o, 0x28, 0);
    put<DWORD>(o, 0x1c, vt(0x5ff96c));   // vtable
    set_level(o, vt(0x5ff8c4));   // CSeriesAI
    return o;
}
AI_CLASS(SeriesAI, 0x2c)

// 0x56ce70 Create, 0x56ceb0 CreateArray: CActionAI, 0x18 bytes (last vtable 0x5ff984)
static BYTE *__fastcall ActionAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ff984));   // CActionAI
    put<BYTE>(o, 0x14, 0);   // "Executed Action"
    return o;
}
AI_CLASS(ActionAI, 0x18)

// 0x56d4b0 Create, 0x56d500 CreateArray: CShieldAI, 0x18 bytes (last vtable 0x5ffa9c)
static BYTE *__fastcall ShieldAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ffa9c));   // CShieldAI
    put<DWORD>(o, 0x10, 0);   // "ShieldEntity"
    return o;
}
AI_CLASS(ShieldAI, 0x18)

// 0x56dcf0 Create, 0x56dd30 CreateArray: CPercentInvulnerableAI, 0x18 bytes (last vtable 0x5ffb44)
static BYTE *__fastcall PercentInvulnerableAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ffb44));   // CPercentInvulnerableAI
    return o;
}
AI_CLASS(PercentInvulnerableAI, 0x18)

// 0x56fef0 Create, 0x56ff40 CreateArray: CShotAI, 0x10 bytes (last vtable 0x5fff54)
static BYTE *__fastcall ShotAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5fff54));   // CShotAI
    return o;
}
AI_CLASS(ShotAI, 0x10)

// 0x570120 Create, 0x570170 CreateArray: CShootOverAI, 0x10 bytes (last vtable 0x5ffffc)
static BYTE *__fastcall ShootOverAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5ffffc));   // CShootOverAI
    return o;
}
AI_CLASS(ShootOverAI, 0x10)

// 0x570430 Create, 0x570490 CreateArray: CTurnToBounceDirAI, 0x10 bytes (last vtable 0x6000a4)
static BYTE *__fastcall TurnToBounceDirAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5fff54));   // CShotAI
    set_level(o, vt(0x6000a4));   // CTurnToBounceDirAI
    return o;
}
AI_CLASS(TurnToBounceDirAI, 0x10)

// 0x570810 Create, 0x570870 CreateArray: CPassThroughShooterAI, 0x10 bytes (last vtable 0x60014c)
static BYTE *__fastcall PassThroughShooterAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5fff54));   // CShotAI
    set_level(o, vt(0x60014c));   // CPassThroughShooterAI
    return o;
}
AI_CLASS(PassThroughShooterAI, 0x10)

// 0x570bc0 Create, 0x570c30 CreateArray: CExplodeOnImpact, 0x10 bytes (last vtable 0x6001f4)
static BYTE *__fastcall ExplodeOnImpact_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x5fff54));   // CShotAI
    set_level(o, vt(0x60014c));   // CPassThroughShooterAI
    set_level(o, vt(0x6001f4));   // CExplodeOnImpact
    return o;
}
AI_CLASS(ExplodeOnImpact, 0x10)

// 0x570fc0 Create, 0x571010 CreateArray: CSimpleCollideTriggerAI, 0x18 bytes (last vtable 0x60029c)
static BYTE *__fastcall SimpleCollideTriggerAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x60029c));   // CSimpleCollideTriggerAI
    return o;
}
AI_CLASS(SimpleCollideTriggerAI, 0x18)

// 0x573670 Create, 0x5736b0 CreateArray: CSetPatrolAIAction, 0x10 bytes (last vtable 0x600524)
static BYTE *__fastcall SetPatrolAIAction_Construct(BYTE *o)
{
    action_base_ctor(o);
    zstring_ctor(o + 0x8);
    set_level(o, vt(0x600524));   // CSetPatrolAIAction
    return o;
}
AI_CLASS(SetPatrolAIAction, 0x10)

// 0x5739f0 Create, 0x573a30 CreateArray: CSetPursueAIAction, 0x10 bytes (last vtable 0x600594)
static BYTE *__fastcall SetPursueAIAction_Construct(BYTE *o)
{
    action_base_ctor(o);
    zstring_ctor(o + 0x8);
    set_level(o, vt(0x600594));   // CSetPursueAIAction
    return o;
}
AI_CLASS(SetPursueAIAction, 0x10)

// 0x573d70 Create, 0x573db0 CreateArray: CSetAttackAIAction, 0x10 bytes (last vtable 0x600604)
static BYTE *__fastcall SetAttackAIAction_Construct(BYTE *o)
{
    action_base_ctor(o);
    zstring_ctor(o + 0x8);
    set_level(o, vt(0x600604));   // CSetAttackAIAction
    return o;
}
AI_CLASS(SetAttackAIAction, 0x10)

// 0x574c60 Create, 0x574c90 CreateArray: CSkeletonTransition, 0x1c bytes (last vtable 0x6007c4)
static BYTE *__fastcall SkeletonTransition_Construct(BYTE *o)
{
    object_ctor(o);
    set_level(o, vt(0x6007c4));   // CSkeletonTransition
    return o;
}
AI_CLASS(SkeletonTransition, 0x1c)

// 0x575000 Create, 0x575070 CreateArray: CSkeletonAI, 0xac bytes (last vtable 0x600820)
static BYTE *__fastcall SkeletonAI_Construct(BYTE *o)
{
    SkeletonBaseAI_Ctor(o);
    put<DWORD>(o, 0x14, 0);
    put<DWORD>(o, 0x18, 0);
    put<DWORD>(o, 0x1c, 0);
    put<DWORD>(o, 0x10, vt(0x5f8b8c));   // vtable
    EntityBehaviorStateSetClosestTarget_Ctor(o + 0x38);
    put<DWORD>(o, 0x98, vt(0x6008d0));   // vtable
    put<DWORD>(o, 0x9c, 0);
    put<DWORD>(o, 0xa0, 0);   // "Transition"
    put<DWORD>(o, 0xa4, 0);
    set_level(o, vt(0x600820));   // CSkeletonAI
    put<DWORD>(o, 0x20, 0);
    return o;
}
AI_CLASS(SkeletonAI, 0xac)

// 0x577ea0 Create, 0x577ee0 CreateArray: CShootEntityBehavior, 0x14 bytes (last vtable 0x60098c)
static BYTE *__fastcall ShootEntityBehavior_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x60098c));   // CShootEntityBehavior
    put<float>(o, 0x10, 6.2831855f);
    return o;
}
AI_CLASS(ShootEntityBehavior, 0x14)

// 0x5785a0 Create, 0x5785d0 CreateArray: CPointWeaponAtTargetEntityBehavior, 0xc bytes (last vtable 0x600a08)
static BYTE *__fastcall PointWeaponAtTargetEntityBehavior_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x600a08));   // CPointWeaponAtTargetEntityBehavior
    return o;
}
AI_CLASS(PointWeaponAtTargetEntityBehavior, 0xc)

// 0x584ea0 Create, 0x584ef0 CreateArray: CBroadcastEvilAI, 0x14 bytes (last vtable 0x601458)
static BYTE *__fastcall BroadcastEvilAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    set_level(o, vt(0x601458));   // CBroadcastEvilAI
    return o;
}
AI_CLASS(BroadcastEvilAI, 0x14)

// 0x5856f0 Create, 0x585750 CreateArray: CStandStillAttackAI, 0x30 bytes (last vtable 0x601500)
static BYTE *__fastcall StandStillAttackAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5ee910));   // CAttackAI
    ShootEntityBehavior_Ctor(o + 0x10);
    PointWeaponAtTargetEntityBehavior_Ctor(o + 0x24);
    set_level(o, vt(0x601500));   // CStandStillAttackAI
    return o;
}
AI_CLASS(StandStillAttackAI, 0x30)

// 0x585af0 Create, 0x585b50 CreateArray: CGuardPosAI, 0x108 bytes (last vtable 0x6015a8)
static BYTE *__fastcall GuardPosAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5f42d4));   // CPatrolBaseAI
    EntityBehaviorMoveToPos_Ctor(o + 0x14);
    EntityBehaviorIdle_Ctor(o + 0xd0);
    set_level(o, vt(0x6015a8));   // CGuardPosAI
    return o;
}
AI_CLASS(GuardPosAI, 0x108)

// 0x586310 Create, 0x586350 CreateArray: CEntityBehaviorStateSetClosestTarget, 0x20 bytes (last vtable 0x601654)
static BYTE *__fastcall EntityBehaviorStateSetClosestTarget_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x601654));   // CEntityBehaviorStateSetClosestTarget
    put<DWORD>(o, 0x10, 0);   // "Max Distance"
    put<DWORD>(o, 0x18, 0);   // "Valid Targets"
    put<DWORD>(o, 0x1c, 0);   // "Must Have HPs"
    put<DWORD>(o, 0xc, 0);
    return o;
}
AI_CLASS(EntityBehaviorStateSetClosestTarget, 0x20)

// 0x586980 Create, 0x5869d0 CreateArray: CEntityBehaviorQueue, 0x24 bytes (last vtable 0x6016d0)
static BYTE *__fastcall EntityBehaviorQueue_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    put<DWORD>(o, 0x10, 0);
    put<DWORD>(o, 0x14, 0);   // "Behavior"
    put<DWORD>(o, 0x18, 0);
    put<DWORD>(o, 0xc, vt(0x5f3228));   // vtable
    set_level(o, vt(0x6016d0));   // CEntityBehaviorQueue
    return o;
}
AI_CLASS(EntityBehaviorQueue, 0x24)

// 0x58af50 Create, 0x58af90 CreateArray: CTemporaryTaskAI, 0x1c bytes (last vtable 0x601b18)
static BYTE *__fastcall TemporaryTaskAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x601b18));   // CTemporaryTaskAI
    return o;
}
AI_CLASS(TemporaryTaskAI, 0x1c)

// 0x58c220 Create, 0x58c270 CreateArray: CTenaciousInvestigateAI, 0xd4 bytes (last vtable 0x601cf0)
static BYTE *__fastcall TenaciousInvestigateAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5efa04));   // CInvestigateAI
    EntityBehaviorMoveToPos_Ctor(o + 0x14);
    set_level(o, vt(0x601cf0));   // CTenaciousInvestigateAI
    return o;
}
AI_CLASS(TenaciousInvestigateAI, 0xd4)

// 0x58d890 Create, 0x58d900 CreateArray: CTurretAI, 0x30 bytes (last vtable 0x601d98)
static BYTE *__fastcall TurretAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x601d98));   // CTurretAI
    put<DWORD>(o, 0x10, 0xc8);   // "Start Attack"
    put<DWORD>(o, 0x14, 0x28);   // "Stop Attack Extra"
    put<DWORD>(o, 0x1c, 3);   // "Num Shots In Burst"
    put<float>(o, 0x24, 2.0f);   // "Burst Delay"
    put<BYTE>(o, 0x2c, 0);
    put<DWORD>(o, 0x20, *glob<DWORD *>(0x6c29a0));   // "Valid Targets"
    put<DWORD>(o, 0x18, 0);   // "Retry Time"
    return o;
}
AI_CLASS(TurretAI, 0x30)

// 0x58e790 Create, 0x58e810 CreateArray: CSentinonAI, 0x30 bytes (last vtable 0x601e44)
static BYTE *__fastcall SentinonAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x601d98));   // CTurretAI
    put<DWORD>(o, 0x10, 0xc8);   // "Start Attack"
    put<DWORD>(o, 0x14, 0x28);   // "Stop Attack Extra"
    put<DWORD>(o, 0x1c, 3);   // "Num Shots In Burst"
    put<float>(o, 0x24, 2.0f);   // "Burst Delay"
    put<DWORD>(o, 0x20, *glob<DWORD *>(0x6c29a0));   // "Valid Targets"
    put<BYTE>(o, 0x2c, 0);
    put<DWORD>(o, 0x18, 0);   // "Retry Time"
    set_level(o, vt(0x601e44));   // CSentinonAI
    return o;
}
AI_CLASS(SentinonAI, 0x30)

// 0x58eb60 Create, 0x58ebf0 CreateArray: CAutoGunAI, 0x64 bytes (last vtable 0x601eec)
static BYTE *__fastcall AutoGunAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x601d98));   // CTurretAI
    put<DWORD>(o, 0x10, 0xc8);   // "Start Attack"
    put<DWORD>(o, 0x14, 0x28);   // "Stop Attack Extra"
    put<DWORD>(o, 0x1c, 3);   // "Num Shots In Burst"
    put<float>(o, 0x24, 2.0f);   // "Burst Delay"
    put<DWORD>(o, 0x20, *glob<DWORD *>(0x6c29a0));   // "Valid Targets"
    put<BYTE>(o, 0x2c, 0);
    put<DWORD>(o, 0x18, 0);   // "Retry Time"
    pickup_ai_ctor(o + 0x30);
    set_level(o, vt(0x601eec));   // CAutoGunAI
    put<float>(o, 0x60, 3.0f);   // "PickupDelay"
    return o;
}
AI_CLASS(AutoGunAI, 0x64)

// 0x595cd0 Create, 0x595d20 CreateArray: CWalkingUpperBodyAI, 0x1c bytes (last vtable 0x60273c)
static BYTE *__fastcall WalkingUpperBodyAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x5fd080));   // CSameAsParentAI
    zstring_ctor(o + 0x18);
    set_level(o, vt(0x60273c));   // CWalkingUpperBodyAI
    return o;
}
AI_CLASS(WalkingUpperBodyAI, 0x1c)

// 0x5aa070 Create, 0x5aa0c0 CreateArray: CCameraTrakerAI, 0x20 bytes (last vtable 0x603c98)
static BYTE *__fastcall CameraTrakerAI_Construct(BYTE *o)
{
    LayerPartAI_Ctor(o);
    set_level(o, vt(0x603c98));   // CCameraTrakerAI
    put<DWORD>(o, 0x18, 0);   // "Camera Speed"
    put<BYTE>(o, 0x14, 1);   // "Active"
    put<DWORD>(o, 0x10, 1);   // "Camera Tracking Mode"
    return o;
}
AI_CLASS(CameraTrakerAI, 0x20)

// 0x5aa6c0 Create, 0x5aa700 CreateArray: CEntityBehaviorWarpEffect, 0x20 bytes (last vtable 0x603d40)
static BYTE *__fastcall EntityBehaviorWarpEffect_Construct(BYTE *o)
{
    RenderEffectBehavior_Ctor(o);
    set_level(o, vt(0x603d40));   // CEntityBehaviorWarpEffect
    put<DWORD>(o, 0x14, 0);   // "Cur Warp Time"
    put<DWORD>(o, 0x1c, 0);   // "WhoWereConnectedTo"
    return o;
}
AI_CLASS(EntityBehaviorWarpEffect, 0x20)

// 0x5aae50 Create, 0x5aae80 CreateArray: CEntityBehaviorFireWeapon, 0x10 bytes (last vtable 0x603dc0)
static BYTE *__fastcall EntityBehaviorFireWeapon_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x603dc0));   // CEntityBehaviorFireWeapon
    return o;
}
AI_CLASS(EntityBehaviorFireWeapon, 0x10)

// 0x5ab100 Create, 0x5ab140 CreateArray: CEntityBehaviorFirePrimary, 0x10 bytes (last vtable 0x603e38)
static BYTE *__fastcall EntityBehaviorFirePrimary_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x603dc0));   // CEntityBehaviorFireWeapon
    set_level(o, vt(0x603e38));   // CEntityBehaviorFirePrimary
    return o;
}
AI_CLASS(EntityBehaviorFirePrimary, 0x10)

// 0x5ab700 Create, 0x5ab740 CreateArray: CEntityBehaviorFireSecondary, 0x10 bytes (last vtable 0x603eb0)
static BYTE *__fastcall EntityBehaviorFireSecondary_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x603dc0));   // CEntityBehaviorFireWeapon
    set_level(o, vt(0x603eb0));   // CEntityBehaviorFireSecondary
    return o;
}
AI_CLASS(EntityBehaviorFireSecondary, 0x10)

// 0x5abc00 Create, 0x5abc30 CreateArray: CEntityBehaviorRestartMap, 0xc bytes (last vtable 0x603f28)
static BYTE *__fastcall EntityBehaviorRestartMap_Construct(BYTE *o)
{
    EntityBehavior_Ctor(o);
    set_level(o, vt(0x603f28));   // CEntityBehaviorRestartMap
    return o;
}
AI_CLASS(EntityBehaviorRestartMap, 0xc)

// ---- Class getters and RegisterFields of the trigger AIs --------------------------------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
using ByteFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using StringFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ObjectFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
using ObjectRefFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD);
using FloatFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using FloatFieldCtorUnits = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                               DWORD, DWORD, DWORD);
using ResourceFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);

static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD add_field(CClassInfo *c, CFieldDesc *f) { return fn<DWORD(THISCALL *)(CClassInfo *, CFieldDesc *)>(0x415700)(c, f); }
static CFieldDesc *last_field(CClassInfo *c) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_alt_name(CFieldDesc *f, const char *text) { return fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419030)(f, text); }
static CClassInfo *action_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x41a650)(); }            // CAction
static CClassInfo *layer_part_ai_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4fd220)(); }     // CLayerPartAI

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated). The class info gets the exe addresses of Create, CreateArray and
// RegisterFields, as in the original (they are hooked).
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, DWORD abstract,
                                void *create, void *create_array, void *register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, abstract,
                                     create, create_array, register_fields);
    return v;
}

// The getters below are the class getters of the exe (slot 2 and the copies, see the hook table) and
// the ones inlined into every RegisterFields before each ClassInfo_AddField. Each RegisterFields
// returns the eax of its last call: ClassInfo_AddField's (the new field count), FieldDesc_SetAltName's
// or the class info whose alternative class name it sets.

// class CTriggerAI (variable 0x62e9cc)
static CClassInfo *__cdecl TriggerAI_ClassInfo()
{
    return class_getter(0x62e9cc, 0x60b42c /* "CTriggerAI" */, 7.0f, layer_part_ai_class_info, 0x20, 1, code<void *>(0x46b760), code<void *>(0x52dc80), code<void *>(0x4227e0));
}

// class CTouchingTriggerAI (variable 0x62e9c4)
static CClassInfo *__cdecl TouchingTriggerAI_ClassInfo()
{
    return class_getter(0x62e9c4, 0x60b3a0 /* "CTouchingTriggerAI" */, 1.0f, TriggerAI_ClassInfo, 0x38, 1, code<void *>(0x46b760), code<void *>(0x52dc80), code<void *>(0x421ff0));
}

// class CollideTriggerAI (variable 0x62e7c0)
static CClassInfo *__cdecl CollideTriggerAI_ClassInfo()
{
    return class_getter(0x62e7c0, 0x60af6c /* "CollideTriggerAI" */, 1.0f, TriggerAI_ClassInfo, 0x38, 0, code<void *>(0x41c9d0), code<void *>(0x41ca30), code<void *>(0x41cbf0));
}

// class CPassThroughTriggerAI (variable 0x62e7c4)
static CClassInfo *__cdecl PassThroughTriggerAI_ClassInfo()
{
    return class_getter(0x62e7c4, 0x60b008 /* "CPassThroughTriggerAI" */, 1.0f, TriggerAI_ClassInfo, 0x24, 0, code<void *>(0x41d120), code<void *>(0x41d170), code<void *>(0x41d320));
}

// class CRelayAI (variable 0x62e940)
static CClassInfo *__cdecl RelayAI_ClassInfo()
{
    return class_getter(0x62e940, 0x60b1d8 /* "CRelayAI" */, 1.0f, layer_part_ai_class_info, 0x18, 0, code<void *>(0x41fc20), code<void *>(0x41fc60), code<void *>(0x41fdd0));
}

// class CRepeatTimerTriggerAI (variable 0x62e94c)
static CClassInfo *__cdecl RepeatTimerTriggerAI_ClassInfo()
{
    return class_getter(0x62e94c, 0x60b240 /* "CRepeatTimerTriggerAI" */, 3.0f, layer_part_ai_class_info, 0x20, 0, code<void *>(0x4203a0), code<void *>(0x4203e0), code<void *>(0x420540));
}

// class CSpawnPointAI (variable 0x62e954)
static CClassInfo *__cdecl SpawnPointAI_ClassInfo()
{
    return class_getter(0x62e954, 0x60b2cc /* "CSpawnPointAI" */, 1.0f, layer_part_ai_class_info, 0x1c, 0, code<void *>(0x4209a0), code<void *>(0x4209e0), code<void *>(0x420b50));
}

// class CTouchingOvalTriggerAI (variable 0x62e98c)
static CClassInfo *__cdecl TouchingOvalTriggerAI_ClassInfo()
{
    return class_getter(0x62e98c, 0x60b314 /* "CTouchingOvalTriggerAI" */, 1.0f, TouchingTriggerAI_ClassInfo, 0x44, 0, code<void *>(0x420f30), code<void *>(0x420f60), code<void *>(0x4210b0));
}

// class CTouchingPolygonTriggerAI (variable 0x62e9bc)
static CClassInfo *__cdecl TouchingPolygonTriggerAI_ClassInfo()
{
    return class_getter(0x62e9bc, 0x60b368 /* "CTouchingPolygonTriggerAI" */, 1.0f, TouchingTriggerAI_ClassInfo, 0x38, 0, code<void *>(0x4218d0), code<void *>(0x421900), code<void *>(0x421a50));
}

// 0x4227e0: RegisterFields of CTriggerAI
static DWORD __cdecl TriggerAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b1f4) /* "Trigger Only Once" */, 0x1c, 1, 0x1c, 1, 0, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b524) /* "Debug/Was Triggered" */, 0x1d, 1, 0x1d, 3, 0, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b510) /* "Auto-Flip Switch" */, 0x1e, 1, 0x1e, 1, 1, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b4ec) /* "Triggered By/Trigge Must Have HP" */, 0x10, 1, 0x10, 1, 0, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b4c8) /* "Triggered By/Triggered By Anything" */, 0x11, 1, 0x11, 1, 0, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b4a4) /* "Triggered By/Triggered By Players" */, 0x12, 1, 0x12, 1, 1, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b480) /* "Triggered By/Triggered By Enemies" */, 0x13, 1, 0x13, 1, 0, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b458) /* "Triggered By/Triggered By Projectiles" */, 0x14, 1, 0x14, 1, 0, 0);
    add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x34);
    if (f) {
        fn<StringFieldCtor>(0x415dd0)(f, glob<const char *>(0x60b438) /* "Triggered By/Triggered By Name" */, 0x18, 4, 0x18, 1, 0, 0);
        *static_cast<DWORD *>(f) = vt(0x5ed924);
    }
    return add_field(TriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
}

// 0x421ff0: RegisterFields of CTouchingTriggerAI
static DWORD __cdecl TouchingTriggerAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60af5c) /* "Enter Action" */, 0x20, 4, 0x20, action_class_info(), 1, 0, 0, 0);
    add_field(TouchingTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60b420) /* "Exit Action" */, 0x24, 4, 0x24, action_class_info(), 1, 0, 0, 0);
    add_field(TouchingTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60b410) /* "Repeat Action" */, 0x28, 4, 0x28, action_class_info(), 1, 0, 0, 0);
    add_field(TouchingTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x60b400) /* "Repeat Every" */, glob<const char *>(0x60b000) /* "Seconds" */, 0x2c, 4, 0x2c, 1, 0, 0, 0, 0x47c34f80 /* 99999.0 */, 0);
    add_field(TouchingTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    set_alt_name(last_field(TouchingTriggerAI_ClassInfo()), glob<const char *>(0x60b3f0) /* "Repeat Seconds" */);
    f = heap_alloc(0x30);
    if (f)
        f = fn<ObjectRefFieldCtor>(0x523e30)(f, glob<const char *>(0x60b3d4) /* "Debug/Touching LayerPart" */, 0x30, 4, 0x30, fn<CClassInfo *(__cdecl *)()>(0x4f9bf0)()   /* CLayerPart_ClassInfo */, 3, 0);
    add_field(TouchingTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x60b3bc) /* "Debug/Next Repeat Time" */, 0x34, 4, 0x34, 3, 0, 0, 0, 0, 0);
    add_field(TouchingTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    return set_alt_name(last_field(TouchingTriggerAI_ClassInfo()), glob<const char *>(0x60b3b4) /* "IsClose" */);
}

// 0x41cbf0: RegisterFields of CollideTriggerAI
static DWORD __cdecl CollideTriggerAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60aed0) /* "Action" */, 0x20, 4, 0x20, action_class_info(), 1, 0, 0, 0);
    add_field(CollideTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x60afec) /* "Disallow Retrigger" */, glob<const char *>(0x60b000) /* "Seconds" */, 0x24, 4, 0x24, 1, 0x3f000000 /* 0.5 */, 0, 0, 0x497423f0 /* 999999.0 */, 0);
    add_field(CollideTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    set_alt_name(last_field(CollideTriggerAI_ClassInfo()), glob<const char *>(0x60afcc) /* "Seconds To Disallow Retrigger" */);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x60afb8) /* "Last Trigger Time" */, 0x28, 4, 0x28, 0, 0, 0, 0, 0x497423f0 /* 999999.0 */, 0);
    add_field(CollideTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceFieldCtor>(0x54e560)(f, glob<const char *>(0x60af9c) /* "Retrigger Disallow Time For" */, 0x2c, 4, 0x2c, glob_addr(0x62e7a8), 1, glob_addr(0x60af44) /* "All Objects" */, 0, 0);
    add_field(CollideTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60af80) /* "Action for Failed Retrigger" */, 0x30, 4, 0x30, action_class_info(), 1, 0, 0, 0);
    return add_field(CollideTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
}

// 0x41d320: RegisterFields of CPassThroughTriggerAI
static DWORD __cdecl PassThroughTriggerAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60aed0) /* "Action" */, 0x20, 4, 0x20, action_class_info(), 1, 0, 0, 0);
    return add_field(PassThroughTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
}

// 0x41fdd0: RegisterFields of CRelayAI
static DWORD __cdecl RelayAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60aed0) /* "Action" */, 0x10, 4, 0x10, action_class_info(), 1, 0, 0, 0);
    add_field(RelayAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b1f4) /* "Trigger Only Once" */, 0x14, 1, 0x14, 1, 0, 0);
    add_field(RelayAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b1e4) /* "Was Triggered" */, 0x15, 1, 0x15, 0, 0, 0);
    return add_field(RelayAI_ClassInfo(), static_cast<CFieldDesc *>(f));
}

// 0x420540: RegisterFields of CRepeatTimerTriggerAI
static DWORD __cdecl RepeatTimerTriggerAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60aed0) /* "Action" */, 0x10, 4, 0x10, action_class_info(), 1, 0, 0, 0);
    add_field(RepeatTimerTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x60b2b4) /* "Delay Between Actions" */, glob<const char *>(0x60b000) /* "Seconds" */, 0x14, 4, 0x14, 1, 0x3f800000 /* 1.0 */, 0, 0x3c888889 /* 0.016666668 */, 0x497423f0 /* 999999.0 */, 0);
    add_field(RepeatTimerTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    set_alt_name(last_field(RepeatTimerTriggerAI_ClassInfo()), glob<const char *>(0x60b29c) /* "Seconds Between Actions" */);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x60b28c) /* "Plus or Minus" */, glob<const char *>(0x60b000) /* "Seconds" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0x497423f0 /* 999999.0 */, 0x3dcccccd /* 0.1 */);
    add_field(RepeatTimerTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    set_alt_name(last_field(RepeatTimerTriggerAI_ClassInfo()), glob<const char *>(0x60b274) /* "Plus or Minus Seconds" */);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x60b258) /* "Seconds Till Next Execute" */, 0x1c, 4, 0x1c, 0, 0, 0, 0, 0, 0);
    return add_field(RepeatTimerTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
}

// 0x420b50: RegisterFields of CSpawnPointAI
static DWORD __cdecl SpawnPointAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60b304) /* "Spawn Action" */, 0x10, 4, 0x10, action_class_info(), 1, 0, 0, 0);
    add_field(SpawnPointAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x30);
    if (f)
        f = fn<ByteFieldCtor>(0x54ddf0)(f, glob<const char *>(0x60b2f0) /* "Set Facing Angle" */, 0x14, 1, 0x14, 1, 0, 0);
    add_field(SpawnPointAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x60b2dc) /* "Facing Angle" */, glob<const char *>(0x60b2ec) /* "Deg" */, 0x18, 4, 0x18, 1, 0x43340000 /* 180.0 */, 0, 0, 0x43b40000 /* 360.0 */, 0x40a00000 /* 5.0 */);
    return add_field(SpawnPointAI_ClassInfo(), static_cast<CFieldDesc *>(f));
}

// 0x4210b0: RegisterFields of CTouchingOvalTriggerAI
static DWORD __cdecl TouchingOvalTriggerAI_RegisterFields()
{
    void *f;
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x60b354) /* "X Radius" */, glob<const char *>(0x60b360) /* "Pixels" */, 0x38, 4, 0x38, 1, 0x41f00000 /* 30.0 */, 0, 0, 0, 0);
    add_field(TouchingOvalTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    set_alt_name(last_field(TouchingOvalTriggerAI_ClassInfo()), glob<const char *>(0x60b34c) /* "Radius" */);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x60b340) /* "YX Ratio" */, 0x3c, 4, 0x3c, 1, 0x3f3677d4 /* 0.71276593 */, fn_addr(0x421410) /* function */, 0, 0, 0x3d4ccccd /* 0.05 */);
    add_field(TouchingOvalTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x62e558), 0x40, 4, 0x40, 0, 0, 0, 0, 0, 0);
    add_field(TouchingOvalTriggerAI_ClassInfo(), static_cast<CFieldDesc *>(f));
    CClassInfo *c = TouchingOvalTriggerAI_ClassInfo();
    c->alt_name = glob<const char *>(0x60b32c) /* "CProximitySwitchAI" */;
    return ptrv(c);
}

// 0x421a50: RegisterFields of CTouchingPolygonTriggerAI
static DWORD __cdecl TouchingPolygonTriggerAI_RegisterFields()
{
    CClassInfo *c = TouchingPolygonTriggerAI_ClassInfo();
    c->alt_name = glob<const char *>(0x60b384) /* "CTouchingPolyongTriggerAI" */;
    return ptrv(c);
}


// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h); lengths and bytes read from
// gamedata/Zax - German/Zax.exe with capstone
ZAX_HOOKS(ai_classes)
{
    const Hook list[] = {
        {0x41c9d0, (void *)&CollideTriggerAI_Create, "CollideTriggerAI::Create", 6, {0x56, 0xb9, 0x38, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x41ca30, (void *)&CollideTriggerAI_CreateArray, "CollideTriggerAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x41d120, (void *)&PassThroughTriggerAI_Create, "CPassThroughTriggerAI::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x41d170, (void *)&PassThroughTriggerAI_CreateArray, "CPassThroughTriggerAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x41fc20, (void *)&RelayAI_Create, "CRelayAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x41fc60, (void *)&RelayAI_CreateArray, "CRelayAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4203a0, (void *)&RepeatTimerTriggerAI_Create, "CRepeatTimerTriggerAI::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4203e0, (void *)&RepeatTimerTriggerAI_CreateArray, "CRepeatTimerTriggerAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4209a0, (void *)&SpawnPointAI_Create, "CSpawnPointAI::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4209e0, (void *)&SpawnPointAI_CreateArray, "CSpawnPointAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x420f30, (void *)&TouchingOvalTriggerAI_Create, "CTouchingOvalTriggerAI::Create", 6, {0x56, 0xb9, 0x44, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x420f60, (void *)&TouchingOvalTriggerAI_CreateArray, "CTouchingOvalTriggerAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4218d0, (void *)&TouchingPolygonTriggerAI_Create, "CTouchingPolygonTriggerAI::Create", 6, {0x56, 0xb9, 0x38, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x421900, (void *)&TouchingPolygonTriggerAI_CreateArray, "CTouchingPolygonTriggerAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x4223f0, (void *)&TouchingTriggerAI_Ctor, "CTouchingTriggerAI::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x68, 0xac, 0x0d, 0x00}, 4},
        {0x422f10, (void *)&AttractCompassAI_Create, "CAttractCompassAI::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x422f50, (void *)&AttractCompassAI_CreateArray, "CAttractCompassAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x42d660, (void *)&AmbientSoundAI_Create, "CAmbientSoundAI::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x42d6b0, (void *)&AmbientSoundAI_CreateArray, "CAmbientSoundAI::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d}},
        {0x42e150, (void *)&ApproachTargetAI_Create, "CApproachTargetAI::Create", 6, {0x56, 0xb9, 0xd8, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x42e1a0, (void *)&ApproachTargetAI_CreateArray, "CApproachTargetAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x04}},
        {0x42f360, (void *)&BehaviorActivity_Create, "CBehaviorActivity::Create", 6, {0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x42f3b0, (void *)&BehaviorActivity_CreateArray, "CBehaviorActivity::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x42f730, (void *)&EntityBehaviorMoveToPos_Create, "CEntityBehaviorMoveToPos::Create", 5, {0xb9, 0xbc, 0x00, 0x00, 0x00, 0xe8, 0xd6, 0x7f}},
        {0x42f750, (void *)&EntityBehaviorMoveToPos_CreateArray, "CEntityBehaviorMoveToPos::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76, 0xc1, 0xe0}},
        {0x4302c0, (void *)&EntityBehaviorMoveToPos_Ctor, "CEntityBehaviorMoveToPos::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x48, 0x50, 0x06, 0x00}, 4},
        {0x467180, (void *)&BounceAI_Create, "CBounceAI::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x4671d0, (void *)&BounceAI_CreateArray, "CBounceAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x46b810, (void *)&ChaseInvestigateAI_Create, "CChaseInvestigateAI::Create", 6, {0x56, 0xb9, 0x10, 0x01, 0x00, 0x00, 0xe8, 0xf5}},
        {0x46b870, (void *)&ChaseInvestigateAI_CreateArray, "CChaseInvestigateAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x46c6f0, (void *)&ChasePursueAI_Create, "CChasePursueAI::Create", 6, {0x56, 0xb9, 0xd4, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x46c740, (void *)&ChasePursueAI_CreateArray, "CChasePursueAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x04}},
        {0x46e120, (void *)&CompanionAI_Create, "CCompanionAI::Create", 6, {0x56, 0xb9, 0x0c, 0x01, 0x00, 0x00, 0xe8, 0xe5}},
        {0x46e190, (void *)&CompanionAI_CreateArray, "CCompanionAI::CreateArray", 6, {0x53, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x472f30, (void *)&FireContinuousFireWeaponBaseAI_Create, "CFireContinuousFireWeaponBaseAI::Create", 6, {0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x472f80, (void *)&FireContinuousFireWeaponBaseAI_CreateArray, "CFireContinuousFireWeaponBaseAI::CreateArray", 7, {0x51, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x474880, (void *)&FireElectricalWeaponAI_Create, "CFireElectricalWeaponAI::Create", 6, {0x56, 0xb9, 0x40, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x4748f0, (void *)&FireElectricalWeaponAI_CreateArray, "CFireElectricalWeaponAI::CreateArray", 6, {0x51, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x475240, (void *)&FireParticleWeaponAI_Create, "CFireParticleWeaponAI::Create", 6, {0x56, 0xb9, 0x40, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x4752b0, (void *)&FireParticleWeaponAI_CreateArray, "CFireParticleWeaponAI::CreateArray", 6, {0x51, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x4764f0, (void *)&DamageExpandingRadiusAI_Create, "CDamageExpandingRadiusAI::Create", 6, {0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x476540, (void *)&DamageExpandingRadiusAI_CreateArray, "CDamageExpandingRadiusAI::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x477d30, (void *)&DeathBehaviorListDefinition_Create, "CDeathBehaviorListDefinition::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x477d80, (void *)&DeathBehaviorListDefinition_CreateArray, "CDeathBehaviorListDefinition::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x47b910, (void *)&PickupDeployedItemAI_Create, "CPickupDeployedItemAI::Create", 6, {0x56, 0xb9, 0x30, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x47b940, (void *)&PickupDeployedItemAI_CreateArray, "CPickupDeployedItemAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0xc1, 0xe0}},
        {0x481760, (void *)&DoorAI_Create, "CDoorAI::Create", 6, {0x56, 0xb9, 0x48, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4817b0, (void *)&DoorAI_CreateArray, "CDoorAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x494470, (void *)&EntityBehavior_Ctor, "CEntityBehavior::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x98, 0x21, 0xf8, 0xff}, 4},
        {0x494790, (void *)&EntityBehaviorTurnMovingAngle_Create, "CEntityBehaviorTurnMovingAngle::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4947e0, (void *)&EntityBehaviorTurnMovingAngle_CreateArray, "CEntityBehaviorTurnMovingAngle::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x494a90, (void *)&EntityBehaviorTurnAimingAngle_Create, "CEntityBehaviorTurnAimingAngle::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x494ae0, (void *)&EntityBehaviorTurnAimingAngle_CreateArray, "CEntityBehaviorTurnAimingAngle::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x494de0, (void *)&EntityBehaviorTurnToAngle_Create, "CEntityBehaviorTurnToAngle::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x494e40, (void *)&EntityBehaviorTurnToAngle_CreateArray, "CEntityBehaviorTurnToAngle::CreateArray", 11, {0x51, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00}},
        {0x495310, (void *)&EntityBehaviorTurnToAngle_Ctor, "CEntityBehaviorTurnToAngle::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x58, 0xf1, 0xff, 0xff}, 4},
        {0x495760, (void *)&EntityBehaviorTurnToTarget_Create, "CEntityBehaviorTurnToTarget::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4957b0, (void *)&EntityBehaviorTurnToTarget_CreateArray, "CEntityBehaviorTurnToTarget::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x496370, (void *)&EntityBehaviorSetRotationSequence_Create, "CEntityBehaviorSetRotationSequence::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x4963b0, (void *)&EntityBehaviorSetRotationSequence_CreateArray, "CEntityBehaviorSetRotationSequence::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4966d0, (void *)&EntityBehaviorPlaySequenceOnce_Create, "CEntityBehaviorPlaySequenceOnce::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x496720, (void *)&EntityBehaviorPlaySequenceOnce_CreateArray, "CEntityBehaviorPlaySequenceOnce::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4972e0, (void *)&EntityBehaviorSetPlayRatio_Create, "CEntityBehaviorSetPlayRatio::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x497310, (void *)&EntityBehaviorSetPlayRatio_CreateArray, "CEntityBehaviorSetPlayRatio::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4975b0, (void *)&EntityBehaviorMarkForDeletion_Create, "CEntityBehaviorMarkForDeletion::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x4975e0, (void *)&EntityBehaviorMarkForDeletion_CreateArray, "CEntityBehaviorMarkForDeletion::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4977f0, (void *)&EntityBehaviorClearFlags_Create, "CEntityBehaviorClearFlags::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x497830, (void *)&EntityBehaviorClearFlags_CreateArray, "CEntityBehaviorClearFlags::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x497ca0, (void *)&EntityBehaviorWait_Create, "CEntityBehaviorWait::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x497cd0, (void *)&EntityBehaviorWait_CreateArray, "CEntityBehaviorWait::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x498370, (void *)&EntityBehaviorFade_Create, "CEntityBehaviorFade::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x4983c0, (void *)&EntityBehaviorFade_CreateArray, "CEntityBehaviorFade::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x498720, (void *)&EntityBehaviorIdle_Create, "CEntityBehaviorIdle::Create", 6, {0x56, 0xb9, 0x30, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x498770, (void *)&EntityBehaviorIdle_CreateArray, "CEntityBehaviorIdle::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0xc1, 0xe0}},
        {0x498d90, (void *)&EntityBehaviorIdle_Ctor, "CEntityBehaviorIdle::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xd8, 0xb6, 0xff, 0xff}, 4},
        {0x499360, (void *)&EntityBehaviorSetCurFrame_Create, "CEntityBehaviorSetCurFrame::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x499390, (void *)&EntityBehaviorSetCurFrame_CreateArray, "CEntityBehaviorSetCurFrame::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x499590, (void *)&EntityBehaviorExecuteAction_Create, "CEntityBehaviorExecuteAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4995c0, (void *)&EntityBehaviorExecuteAction_CreateArray, "CEntityBehaviorExecuteAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x499800, (void *)&EntityBehaviorPlaySequenceLooped_Create, "CEntityBehaviorPlaySequenceLooped::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x499840, (void *)&EntityBehaviorPlaySequenceLooped_CreateArray, "CEntityBehaviorPlaySequenceLooped::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x499cc0, (void *)&EntityBehaviorPlaySequenceLooped_Ctor, "CEntityBehaviorPlaySequenceLooped::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xa8, 0xa7, 0xff, 0xff}, 4},
        {0x49a000, (void *)&EntityBehaviorStack_Create, "CEntityBehaviorStack::Create", 7, {0x53, 0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8}},
        {0x49a050, (void *)&EntityBehaviorStack_CreateArray, "CEntityBehaviorStack::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8b, 0xc6, 0xc1}},
        {0x49ffa0, (void *)&FidgetSettings_Create, "CFidgetSettings::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x49ffd0, (void *)&FidgetSettings_CreateArray, "CFidgetSettings::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4a0380, (void *)&FidgetData_Create, "CFidgetData::Create", 5, {0xb9, 0x8c, 0x00, 0x00, 0x00, 0xe8, 0x86, 0x73}},
        {0x4a03a0, (void *)&FidgetData_CreateArray, "CFidgetData::CreateArray", 10, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5, 0x00, 0x00}},
        {0x4a04e0, (void *)&FidgetData_Ctor, "CFidgetData::Ctor", 5, {0x53, 0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x26, 0x61}},
        {0x4a4fa0, (void *)&FollowAI_Create, "FollowAI::Create", 6, {0x56, 0xb9, 0xf8, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4a5010, (void *)&FollowAI_CreateArray, "FollowAI::CreateArray", 6, {0x51, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x4ca4e0, (void *)&GravityAI_Create, "CGravityAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4ca520, (void *)&GravityAI_CreateArray, "CGravityAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4ca9c0, (void *)&DrawTimeoutOvalAI_Create, "CDrawTimeoutOvalAI::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x4caa10, (void *)&DrawTimeoutOvalAI_CreateArray, "CDrawTimeoutOvalAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x4cae90, (void *)&DrawTimeoutRectAI_Create, "CDrawTimeoutRectAI::Create", 6, {0x56, 0xb9, 0x2c, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4caee0, (void *)&DrawTimeoutRectAI_CreateArray, "CDrawTimeoutRectAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4cb280, (void *)&PlaySequenceLoopedAI_Create, "CPlaySequenceLoopedAI::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x4cb2d0, (void *)&PlaySequenceLoopedAI_CreateArray, "CPlaySequenceLoopedAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4cb6b0, (void *)&SimultaneousAIsAI_Create, "CSimultaneousAIsAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x4cb6f0, (void *)&SimultaneousAIsAI_CreateArray, "CSimultaneousAIsAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4cbb30, (void *)&WhileAI_Create, "CWhileAI::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4cbb70, (void *)&WhileAI_CreateArray, "CWhileAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x4cbfd0, (void *)&WaitAI_Create, "CWaitAI::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4cc010, (void *)&WaitAI_CreateArray, "CWaitAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4cc4e0, (void *)&IdleAI_Create, "CIdleAI::Create", 6, {0x56, 0xb9, 0x40, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4cc530, (void *)&IdleAI_CreateArray, "CIdleAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x06}},
        {0x4cc890, (void *)&ForceUpdatingAI_Create, "CForceUpdatingAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4cc8d0, (void *)&ForceUpdatingAI_CreateArray, "CForceUpdatingAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4ccca0, (void *)&LimitedTimeAI_Create, "CLimitedTimeAI::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4ccce0, (void *)&LimitedTimeAI_CreateArray, "CLimitedTimeAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4cd1d0, (void *)&HandleMessageAI_Create, "CHandleMessageAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4cd220, (void *)&HandleMessageAI_CreateArray, "CHandleMessageAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4cfa60, (void *)&GeneratorAI_Create, "CGeneratorAI::Create", 6, {0x56, 0xb9, 0x48, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4cfab0, (void *)&GeneratorAI_CreateArray, "CGeneratorAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x4d0a00, (void *)&GoToAI_Create, "CGoToAI::Create", 6, {0x56, 0xb9, 0x18, 0x01, 0x00, 0x00, 0xe8, 0x05}},
        {0x4d0a60, (void *)&GoToAI_CreateArray, "CGoToAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x4e39c0, (void *)&LandscapeAI_Create, "CLandscapeAI::Create", 6, {0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x4e3a00, (void *)&LandscapeAI_CreateArray, "CLandscapeAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4fd060, (void *)&AIBase_Ctor, "CAIBase::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xa8, 0x95, 0xf1, 0xff}, 4},
        {0x4fe2e0, (void *)&LightBeamAI_Create, "CLightBeamAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4fe330, (void *)&LightBeamAI_CreateArray, "CLightBeamAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x508730, (void *)&PointOfIntrestAI_Create, "CPointOfIntrestAI::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x508780, (void *)&PointOfIntrestAI_CreateArray, "CPointOfIntrestAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x522b90, (void *)&NormalAttackAI_Create, "CNormalAttackAI::Create", 6, {0x56, 0xb9, 0x4c, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x522bf0, (void *)&NormalAttackAI_CreateArray, "CNormalAttackAI::CreateArray", 7, {0x51, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d}},
        {0x528c10, (void *)&GravityParticleAI_Create, "CGravityParticleAI::Create", 7, {0x53, 0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8}},
        {0x528c70, (void *)&GravityParticleAI_CreateArray, "CGravityParticleAI::CreateArray", 5, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f}},
        {0x529740, (void *)&FloatParticleAI_Create, "CFloatParticleAI::Create", 7, {0x53, 0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8}},
        {0x5297a0, (void *)&FloatParticleAI_CreateArray, "CFloatParticleAI::CreateArray", 5, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff}},
        {0x529a90, (void *)&SwirlParticleAI_Create, "CSwirlParticleAI::Create", 7, {0x53, 0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8}},
        {0x529af0, (void *)&SwirlParticleAI_CreateArray, "CSwirlParticleAI::CreateArray", 5, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f}},
        {0x52dd20, (void *)&ParticleSystemAI_Ctor, "CParticleSystemAI::Ctor", 5, {0x53, 0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x36, 0xf3}},
        {0x52de80, (void *)&Envelope_Ctor, "CEnvelope::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x88, 0x87, 0xee, 0xff}, 4},
        {0x52f040, (void *)&GenericParticleSystemAI_Create, "CGenericParticleSystemAI::Create", 6, {0x56, 0xb9, 0x48, 0x01, 0x00, 0x00, 0xe8, 0xc5}},
        {0x52f070, (void *)&GenericParticleSystemAI_CreateArray, "CGenericParticleSystemAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x53a990, (void *)&OldBadPatrolAI_Create, "COldBadPatrolAI::Create", 6, {0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x53aa00, (void *)&OldBadPatrolAI_CreateArray, "COldBadPatrolAI::CreateArray", 7, {0x51, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x53b400, (void *)&PatrolAreaAI_Create, "CPatrolAreaAI::Create", 6, {0x56, 0xb9, 0xd8, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x53b450, (void *)&PatrolAreaAI_CreateArray, "CPatrolAreaAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x04}},
        {0x53be30, (void *)&PatrolPathAI_Create, "CPatrolPathAI::Create", 6, {0x56, 0xb9, 0xf4, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x53be90, (void *)&PatrolPathAI_CreateArray, "CPatrolPathAI::CreateArray", 7, {0x51, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x544df0, (void *)&PlayerWalkingWeaponAI_Create, "CPlayerWalkingWeaponAI::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x544e40, (void *)&PlayerWalkingWeaponAI_CreateArray, "CPlayerWalkingWeaponAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5450b0, (void *)&PlayerWalkingItemAI_Create, "CPlayerWalkingItemAI::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x545100, (void *)&PlayerWalkingItemAI_CreateArray, "CPlayerWalkingItemAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x55e400, (void *)&RenderEffectBehavior_Ctor, "CRenderEffectBehavior::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x68, 0x60, 0xf3, 0xff}, 4},
        {0x561d30, (void *)&SameAsParentAI_Create, "CSameAsParentAI::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x561d70, (void *)&SameAsParentAI_CreateArray, "CSameAsParentAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x565060, (void *)&ScanAreaAI_Create, "CScanAreaAI::Create", 6, {0x56, 0xb9, 0x4c, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x5650b0, (void *)&ScanAreaAI_CreateArray, "CScanAreaAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x566060, (void *)&ScanInvestigateAI_Create, "CScanInvestigateAI::Create", 6, {0x56, 0xb9, 0x5c, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x5660f0, (void *)&ScanInvestigateAI_CreateArray, "CScanInvestigateAI::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76, 0xc1, 0xe0}},
        {0x56a8e0, (void *)&SelectiveCollideAI_Create, "CSelectiveCollideAI::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x56a930, (void *)&SelectiveCollideAI_CreateArray, "CSelectiveCollideAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x56c730, (void *)&SeriesAI_Create, "CSeriesAI::Create", 6, {0x56, 0xb9, 0x2c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x56c790, (void *)&SeriesAI_CreateArray, "CSeriesAI::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf}},
        {0x56ce70, (void *)&ActionAI_Create, "CActionAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x56ceb0, (void *)&ActionAI_CreateArray, "CActionAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x56d4b0, (void *)&ShieldAI_Create, "CShieldAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x56d500, (void *)&ShieldAI_CreateArray, "CShieldAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x56dcf0, (void *)&PercentInvulnerableAI_Create, "CPercentInvulnerableAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x56dd30, (void *)&PercentInvulnerableAI_CreateArray, "CPercentInvulnerableAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x56fef0, (void *)&ShotAI_Create, "CShotAI::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x56ff40, (void *)&ShotAI_CreateArray, "CShotAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x570120, (void *)&ShootOverAI_Create, "CShootOverAI::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x570170, (void *)&ShootOverAI_CreateArray, "CShootOverAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x570430, (void *)&TurnToBounceDirAI_Create, "CTurnToBounceDirAI::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x570490, (void *)&TurnToBounceDirAI_CreateArray, "CTurnToBounceDirAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x570810, (void *)&PassThroughShooterAI_Create, "CPassThroughShooterAI::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x570870, (void *)&PassThroughShooterAI_CreateArray, "CPassThroughShooterAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x570bc0, (void *)&ExplodeOnImpact_Create, "CExplodeOnImpact::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x570c30, (void *)&ExplodeOnImpact_CreateArray, "CExplodeOnImpact::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x570fc0, (void *)&SimpleCollideTriggerAI_Create, "CSimpleCollideTriggerAI::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x571010, (void *)&SimpleCollideTriggerAI_CreateArray, "CSimpleCollideTriggerAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x573670, (void *)&SetPatrolAIAction_Create, "CSetPatrolAIAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x5736b0, (void *)&SetPatrolAIAction_CreateArray, "CSetPatrolAIAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x5739f0, (void *)&SetPursueAIAction_Create, "CSetPursueAIAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x573a30, (void *)&SetPursueAIAction_CreateArray, "CSetPursueAIAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x573d70, (void *)&SetAttackAIAction_Create, "CSetAttackAIAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x573db0, (void *)&SetAttackAIAction_CreateArray, "CSetAttackAIAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x574c60, (void *)&SkeletonTransition_Create, "CSkeletonTransition::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x574c90, (void *)&SkeletonTransition_CreateArray, "CSkeletonTransition::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x575000, (void *)&SkeletonAI_Create, "CSkeletonAI::Create", 7, {0x56, 0x57, 0xb9, 0xac, 0x00, 0x00, 0x00, 0xe8}},
        {0x575070, (void *)&SkeletonAI_CreateArray, "CSkeletonAI::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5}},
        {0x577db0, (void *)&SkeletonBaseAI_Ctor, "CSkeletonBaseAI::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xa8, 0x52, 0xf8, 0xff}, 4},
        {0x577ea0, (void *)&ShootEntityBehavior_Create, "CShootEntityBehavior::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x577ee0, (void *)&ShootEntityBehavior_CreateArray, "CShootEntityBehavior::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5780b0, (void *)&ShootEntityBehavior_Ctor, "CShootEntityBehavior::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xb8, 0xc3, 0xf1, 0xff}, 4},
        {0x5785a0, (void *)&PointWeaponAtTargetEntityBehavior_Create, "CPointWeaponAtTargetEntityBehavior::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x5785d0, (void *)&PointWeaponAtTargetEntityBehavior_CreateArray, "CPointWeaponAtTargetEntityBehavior::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x578780, (void *)&PointWeaponAtTargetEntityBehavior_Ctor, "CPointWeaponAtTargetEntityBehavior::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xe8, 0xbc, 0xf1, 0xff}, 4},
        {0x584ea0, (void *)&BroadcastEvilAI_Create, "CBroadcastEvilAI::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x584ef0, (void *)&BroadcastEvilAI_CreateArray, "CBroadcastEvilAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5856f0, (void *)&StandStillAttackAI_Create, "CStandStillAttackAI::Create", 6, {0x56, 0xb9, 0x30, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x585750, (void *)&StandStillAttackAI_CreateArray, "CStandStillAttackAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0xc1, 0xe0}},
        {0x585af0, (void *)&GuardPosAI_Create, "CGuardPosAI::Create", 6, {0x56, 0xb9, 0x08, 0x01, 0x00, 0x00, 0xe8, 0x15}},
        {0x585b50, (void *)&GuardPosAI_CreateArray, "CGuardPosAI::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x586310, (void *)&EntityBehaviorStateSetClosestTarget_Create, "CEntityBehaviorStateSetClosestTarget::Create", 7, {0x56, 0x57, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8}},
        {0x586350, (void *)&EntityBehaviorStateSetClosestTarget_CreateArray, "CEntityBehaviorStateSetClosestTarget::CreateArray", 6, {0x53, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x586770, (void *)&EntityBehaviorStateSetClosestTarget_Ctor, "CEntityBehaviorStateSetClosestTarget::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0xdc, 0xf0, 0xff}, 4},
        {0x586980, (void *)&EntityBehaviorQueue_Create, "CEntityBehaviorQueue::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x5869d0, (void *)&EntityBehaviorQueue_CreateArray, "CEntityBehaviorQueue::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf6}},
        {0x586e10, (void *)&EntityBehaviorQueue_Ctor, "CEntityBehaviorQueue::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x58, 0xd6, 0xf0, 0xff}, 4},
        {0x58af50, (void *)&TemporaryTaskAI_Create, "CTemporaryTaskAI::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x58af90, (void *)&TemporaryTaskAI_CreateArray, "CTemporaryTaskAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x58c220, (void *)&TenaciousInvestigateAI_Create, "CTenaciousInvestigateAI::Create", 6, {0x56, 0xb9, 0xd4, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x58c270, (void *)&TenaciousInvestigateAI_CreateArray, "CTenaciousInvestigateAI::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x04}},
        {0x58d890, (void *)&TurretAI_Create, "CTurretAI::Create", 6, {0x56, 0xb9, 0x30, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x58d900, (void *)&TurretAI_CreateArray, "CTurretAI::CreateArray", 5, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f}},
        {0x58e790, (void *)&SentinonAI_Create, "CSentinonAI::Create", 6, {0x56, 0xb9, 0x30, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x58e810, (void *)&SentinonAI_CreateArray, "CSentinonAI::CreateArray", 5, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f}},
        {0x58eb60, (void *)&AutoGunAI_Create, "CAutoGunAI::Create", 6, {0x56, 0xb9, 0x64, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x58ebf0, (void *)&AutoGunAI_CreateArray, "CAutoGunAI::CreateArray", 5, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf}},
        {0x595cd0, (void *)&WalkingUpperBodyAI_Create, "CWalkingUpperBodyAI::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x595d20, (void *)&WalkingUpperBodyAI_CreateArray, "CWalkingUpperBodyAI::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x5aa070, (void *)&CameraTrakerAI_Create, "CCameraTrakerAI::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x5aa0c0, (void *)&CameraTrakerAI_CreateArray, "CCameraTrakerAI::CreateArray", 6, {0x51, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x5aa6c0, (void *)&EntityBehaviorWarpEffect_Create, "CEntityBehaviorWarpEffect::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x5aa700, (void *)&EntityBehaviorWarpEffect_CreateArray, "CEntityBehaviorWarpEffect::CreateArray", 6, {0x53, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x5aae50, (void *)&EntityBehaviorFireWeapon_Create, "CEntityBehaviorFireWeapon::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x5aae80, (void *)&EntityBehaviorFireWeapon_CreateArray, "CEntityBehaviorFireWeapon::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x5ab100, (void *)&EntityBehaviorFirePrimary_Create, "CEntityBehaviorFirePrimary::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x5ab140, (void *)&EntityBehaviorFirePrimary_CreateArray, "CEntityBehaviorFirePrimary::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x5ab700, (void *)&EntityBehaviorFireSecondary_Create, "CEntityBehaviorFireSecondary::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x5ab740, (void *)&EntityBehaviorFireSecondary_CreateArray, "CEntityBehaviorFireSecondary::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x5abc00, (void *)&EntityBehaviorRestartMap_Create, "CEntityBehaviorRestartMap::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x5abc30, (void *)&EntityBehaviorRestartMap_CreateArray, "CEntityBehaviorRestartMap::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x41c900, (void *)&CollideTriggerAI_ClassInfo, "CollideTriggerAI_ClassInfo", 5, {0xa1, 0xc0, 0xe7, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41cb90, (void *)&CollideTriggerAI_ClassInfo, "CollideTriggerAI::ClassInfo", 5, {0xa1, 0xc0, 0xe7, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41d2c0, (void *)&PassThroughTriggerAI_ClassInfo, "CPassThroughTriggerAI::ClassInfo", 5, {0xa1, 0xc4, 0xe7, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41fd70, (void *)&RelayAI_ClassInfo, "CRelayAI::ClassInfo", 5, {0xa1, 0x40, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x420350, (void *)&RepeatTimerTriggerAI_ClassInfo, "CRepeatTimerTriggerAI_ClassInfo", 5, {0xa1, 0x4c, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x4204e0, (void *)&RepeatTimerTriggerAI_ClassInfo, "CRepeatTimerTriggerAI::ClassInfo", 5, {0xa1, 0x4c, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x420950, (void *)&SpawnPointAI_ClassInfo, "CSpawnPointAI::ClassInfo2", 5, {0xa1, 0x54, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x420af0, (void *)&SpawnPointAI_ClassInfo, "CSpawnPointAI::ClassInfo", 5, {0xa1, 0x54, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x421050, (void *)&TouchingOvalTriggerAI_ClassInfo, "CTouchingOvalTriggerAI::ClassInfo", 5, {0xa1, 0x8c, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x4219f0, (void *)&TouchingPolygonTriggerAI_ClassInfo, "CTouchingPolygonTriggerAI::ClassInfo", 5, {0xa1, 0xbc, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x421f40, (void *)&TouchingTriggerAI_ClassInfo, "CTouchingTriggerAI::ClassInfo", 5, {0xa1, 0xc4, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x422730, (void *)&TriggerAI_ClassInfo, "CTriggerAI::ClassInfo", 5, {0xa1, 0xcc, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x4227e0, (void *)&TriggerAI_RegisterFields, "CTriggerAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x51, 0xdb, 0x1a}, 5},
        {0x421ff0, (void *)&TouchingTriggerAI_RegisterFields, "CTouchingTriggerAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x41, 0xe3, 0x1a}, 5},
        {0x41cbf0, (void *)&CollideTriggerAI_RegisterFields, "CollideTriggerAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x41, 0x37, 0x1b}, 5},
        {0x41d320, (void *)&PassThroughTriggerAI_RegisterFields, "CPassThroughTriggerAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x11, 0x30, 0x1b}, 5},
        {0x41fdd0, (void *)&RelayAI_RegisterFields, "CRelayAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x61, 0x05, 0x1b}, 5},
        {0x420540, (void *)&RepeatTimerTriggerAI_RegisterFields, "CRepeatTimerTriggerAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xf1, 0xfd, 0x1a}, 5},
        {0x420b50, (void *)&SpawnPointAI_RegisterFields, "CSpawnPointAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xe1, 0xf7, 0x1a}, 5},
        {0x4210b0, (void *)&TouchingOvalTriggerAI_RegisterFields, "CTouchingOvalTriggerAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x81, 0xf2, 0x1a}, 5},
        {0x421a50, (void *)&TouchingPolygonTriggerAI_RegisterFields, "CTouchingPolygonTriggerAI::RegisterFields", 5, {0xa1, 0xbc, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
