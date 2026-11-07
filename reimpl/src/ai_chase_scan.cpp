// The registration of the chase, companion, scan and same-as-parent AIs of 0x46b7b0-0x46e380 and
// 0x561ce0-0x5663d0 (phase 3, AI; docs/DECOMPILATION.md section 8): their class getters, deleting
// destructors and RegisterFields, and the slots that only pass a call on. Their Create and CreateArray
// are in ai_classes.cpp; their think slots (24, 25), the drawing (41) and the companion AI's helpers stay
// with the original.
//
// Layouts [confirmed: the constructors of ai_classes.cpp, the destructors and RegisterFields below]:
//   CChaseInvestigateAI (0x110, vtable 0x5ef95c, class 0x6bd2d0, version 3, parent CInvestigateAI with
//     its destructor 0x4dd780): +0x10 a CEntityBehaviorMoveToPos ("AFloat", embedded, 0xbc bytes;
//     destructor 0x430350), +0xcc a CEntityBehaviorTurnToAngle ("Turn Behavior", 0x1c bytes; 0x495360),
//     +0xf0 "Debug/State" (a typed resource field over 0x6bd2b8, "Chasing"), +0xf4 "Count To
//     Investigate", +0xf8 "Play Ratio", +0xfc "Next Try To Move", +0x100 "Time To Investigate" (5),
//     +0x104 "Radius to Investigate Point", +0x108 "Run To Investigate" (byte, 1), +0x10c "Investigate AI"
//     (an owned CLayerPartAI, default class CScanInvestigateAI; the descriptor's +0x30 set to
//     CInvestigateAreaAI's class afterwards)
//   CChasePursueAI (0xd4, 0x5efaac, 0x6bd2d8, version 2, parent CPursueAI, destructor 0x54fe70): +0x10 the
//     move behaviour ("AFloat"), +0xcc "Play Ratio", +0xd0 "Next Try To Move"
//   CCompanionAI (0x10c, 0x5efef0, 0x6bd328, version 2, parent CLayerPartAI): +0x10 "Move/Move Behavior",
//     +0xd0 "Attack/Attack AI" (owned, class 0x42e870's, default CNormalAttackAI), +0xd4 "Move/Time
//     Between Rechecks" (1), +0xd8 "Debug/Time To Next Recheck", +0xdc/+0xe0 "Move/Minimum/Maximum
//     Distance" (25, 50), +0xe8 "Attack/Attack Distance" (200), +0xec "Debug/Set Target Behavior" (a
//     CEntityBehaviorStateSetClosestTarget, 0x20 bytes; 0x5867a0)
//   CSameAsParentAI (0x14, 0x5fd080, 0x713178, version 3, parent CLayerPartAI): +0x10 "Force Invisable"
//     (sic), +0x11 "Only Show First Sequence" (1)
//   CScanAreaAI (0x4c, 0x5ff1e0, 0x713214, version 2, parent CPatrolBaseAI with its destructor 0x53bd70):
//     +0x10 "Turn Sequence Name" ("Idle"), +0x14/+0x18 the starting and ending angle (−45, 45 degrees),
//     +0x1c "Turning Speed" (10 degrees/s), +0x20 "Time Between Turning", +0x24 "Start Out Waiting", +0x25
//     "Start Counterclockwise", +0x28 "End Turn Action", +0x2c "Execute Each Turn", +0x2d "Reset Each
//     Begin" (1), +0x2e "Turning Flag", +0x30 "Current Turn Time", +0x34 "Complete Turn Time", +0x38/+0x3c
//     the current start and end angle, +0x40 "Waiting" (1), +0x44 "Current Time To Wait", +0x48
//     "Starting Facing Angle" (−999)
//   CScanInvestigateAI (0x5c, 0x5ff288, 0x71321c, version 1, parent CInvestigateAreaAI with its destructor
//     0x4dd860): +0x10 a CScanAreaAI ("AI", embedded) whose slots 23, 24, 25 and 41 it passes calls to
#include "class_info.h"
#include "exe_base.h"

namespace zax {

namespace {

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
void obj_free_fields(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
void call_dtor(FnKey dtor, BYTE *o) { zax::fn<void(THISCALL *)(void *)>(dtor)(o); }
bool is_kind_of(const void *o, CClassInfo *c) { return zax::fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
CClassInfo *cls(FnKey getter) { return zax::fn<Getter>(getter)(); }
DWORD add_field(CClassInfo *c, void *f) { return zax::fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD EMBEDDED_FIELD_CTOR = 0x54e690;         // 0x30
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44

constexpr DWORD G_INVESTIGATE = 0x4dd6d0, G_INVESTIGATE_AREA = 0x4dd7b0, G_PURSUE = 0x54fdc0, G_LAYER_PART_AI = 0x4fd220,
                G_PATROL_BASE = 0x53bcc0, G_ACTION = 0x41a650, G_ATTACK_AI = 0x42e870, G_NORMAL_ATTACK = 0x522b40,
                G_ENTITY_ANIMATED = 0x48dcc0;

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated)
CClassInfo *class_getter(GlobKey var, GlobKey name, DWORD version, FnKey parent, DWORD size, CodeKey create, CodeKey create_array,
                         CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo))) {
            CClassInfo *const parent_class = cls(parent);
            float vf;
            memcpy(&vf, &version, 4);
            zax::fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), vf, parent_class, size, 0, key_fn_addr(create), key_fn_addr(create_array), key_fn_addr(register_fields));
        }
    return v;
}
template <typename... A> void *new_field(DWORD size, FnKey ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = zax::fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
template <typename... A> void *construct_field(void *f, FnKey ctor, A... args)
{
    return f ? zax::fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...) : nullptr;
}

using Destroy = void (*)(BYTE *);
// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the block,
// bit 0 frees (array: operator delete, single object: the pool)
template <DWORD SIZE, Destroy D> void *THISCALL deleting_dtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                D(self + (i - 1) * SIZE);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

}  // namespace

// ---- CChaseInvestigateAI -------------------------------------------------------------------------------------

// 0x46b7b0 and 0x46b9d0 (slot 2)
static CClassInfo *__cdecl ChaseInvestigate_ClassInfo()
{
    return class_getter(0x6bd2d0, 0x60f924 /* "CChaseInvestigateAI" */, 0x40400000, G_INVESTIGATE, 0x110, 0x46b810, 0x46b870, 0x46ba40);
}
static void ChaseInvestigate_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5ef95c);
    obj_free_fields(o);
    call_dtor(0x495360, o + 0xcc);   // CEntityBehaviorTurnToAngle
    call_dtor(0x430350, o + 0x10);   // CEntityBehaviorMoveToPos
    call_dtor(0x4dd780, o);          // CInvestigateAI
}
// 0x46ba40: the fields, then the "Investigate AI" descriptor's +0x30 set to CInvestigateAreaAI's class.
// eax: that class.
static CClassInfo *__cdecl ChaseInvestigate_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60f9d4) /* "AFloat" */, 0x10, 0xbc, 0x10, 1);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60f9c4) /* "Turn Behavior" */, 0xcc, 0x1c, 0xcc, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c4f8) /* "Play Ratio" */, 0xf8, 4, 0xf8, 0, 0, 0, 0, 0, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60f9b0) /* "Next Try To Move" */, 0xfc, 4, 0xfc, 0, 0, 0, 0, 0, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60f99c) /* "Time To Investigate" */, 0x100, 4, 0x100, 1, 0x40a00000 /* 5.0 */, 0, 0, 0, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60f984) /* "Count To Investigate" */, 0xf4, 4, 0xf4, 0, 0, 0, 0, 0, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60f968) /* "Radius to Investigate Point" */, 0x104, 4, 0x104, 1, 0, 0, 0, 0, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60f954) /* "Run To Investigate" */, 0x108, 1, 0x108, 1, 1, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60f948) /* "Debug/State" */, 0xf0, 4, 0xf0, glob_addr(0x6bd2b8), 3, glob_addr(0x60f91c) /* "Chasing" */, 0, 0);
    add_field(ChaseInvestigate_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f) {
        CClassInfo *const default_class = cls(0x566010);   // CScanInvestigateAI
        f = construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x60f938) /* "Investigate AI" */, 0x10c, 4, 0x10c, cls(G_LAYER_PART_AI), 1, default_class, 0,
                            0);
    }
    add_field(ChaseInvestigate_ClassInfo(), f);
    CClassInfo *const area = cls(G_INVESTIGATE_AREA);
    dw(f, 0x30) = (DWORD)(uintptr_t)area;
    return area;
}
// 0x46c660 (slot 29): passed to the investigate AI's slot 29 (+0x10c); al 0 without one
static DWORD THISCALL ChaseInvestigate_Slot29(BYTE *self, DWORD a, DWORD b)
{
    if (void *ai = pt(self, 0x10c))
        return vcall<DWORD>(ai, 29, a, b);
    return 0;
}

// ---- CChasePursueAI ----------------------------------------------------------------------------------------------

// 0x46c690 and 0x46c880 (slot 2)
static CClassInfo *__cdecl ChasePursue_ClassInfo()
{
    return class_getter(0x6bd2d8, 0x60f9dc /* "CChasePursueAI" */, 0x40000000, G_PURSUE, 0xd4, 0x46c6f0, 0x46c740, 0x46c8f0);
}
static void ChasePursue_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5efaac);
    obj_free_fields(o);
    call_dtor(0x430350, o + 0x10);   // CEntityBehaviorMoveToPos
    call_dtor(0x54fe70, o);          // CPursueAI
}
// 0x46c8f0. eax: the last call's.
static DWORD __cdecl ChasePursue_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60f9d4) /* "AFloat" */, 0x10, 0xbc, 0x10, 1);
    add_field(ChasePursue_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c4f8) /* "Play Ratio" */, 0xcc, 4, 0xcc, 0, 0, 0, 0, 0, 0);
    add_field(ChasePursue_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60f9b0) /* "Next Try To Move" */, 0xd0, 4, 0xd0, 0, 0, 0, 0, 0, 0);
    return add_field(ChasePursue_ClassInfo(), f);
}
// 0x46cce0 (slot 41, the debug drawing): passed to the move behaviour's (+0x10) slot 28
static DWORD THISCALL ChasePursue_Slot41(BYTE *self, DWORD a, DWORD b) { return vcall<DWORD>(self + 0x10, 28, a, b); }
// 0x46cd00: a CFailedTargetMessage (vtable 0x5efbfc): its message id (0x506680 with the name), +8 0,
// +0xc the argument. eax: this.
static BYTE *THISCALL FailedTargetMessage_Ctor(BYTE *self, DWORD target)
{
    dw(self, 4) = zax::fn<DWORD(__fastcall *)(const char *)>(0x506680)(glob<const char *>(0x60f9ec) /* "CFailedTargetMessage" */);
    dw(self, 0xc) = target;
    self[8] = 0;
    dw(self, 0) = vt(0x5efbfc);
    return self;
}

// ---- CCompanionAI ------------------------------------------------------------------------------------------------

// 0x46e0c0 and 0x46e310 (slot 2)
static CClassInfo *__cdecl Companion_ClassInfo()
{
    return class_getter(0x6bd328, 0x60fb34 /* "CCompanionAI" */, 0x40000000, G_LAYER_PART_AI, 0x10c, 0x46e120, 0x46e190, 0x46e380);
}
static void Companion_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5efef0);
    obj_free_fields(o);
    call_dtor(0x5867a0, o + 0xec);   // CEntityBehaviorStateSetClosestTarget
    call_dtor(0x430350, o + 0x10);   // CEntityBehaviorMoveToPos
    dw(o, 0) = vt(0x5ed1d0);             // CLayerPartAI
    obj_free_fields(o);
    call_dtor(0x4fd080, o);          // CAIBase
}
// 0x46e380. eax: the last call's.
static DWORD __cdecl Companion_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60fbf4) /* "Move/Move Behavior" */, 0x10, 0xbc, 0x10, 3);
    add_field(Companion_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60fbdc) /* "Move/Minimum Distance" */, 0xdc, 4, 0xdc, 1, 0x41c80000 /* 25.0 */, 0, 0, 0, 0);
    add_field(Companion_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60fbc4) /* "Move/Maximum Distance" */, 0xe0, 4, 0xe0, 1, 0x42480000 /* 50.0 */, 0, 0, 0, 0);
    add_field(Companion_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60fba8) /* "Move/Time Between Rechecks" */, 0xd4, 4, 0xd4, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    add_field(Companion_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60fb8c) /* "Debug/Time To Next Recheck" */, 0xd8, 4, 0xd8, 3, 0, 0, 0, 0, 0);
    add_field(Companion_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60fb74) /* "Attack/Attack Distance" */, 0xe8, 4, 0xe8, 1, 0x43480000 /* 200.0 */, 0, 0, 0, 0);
    add_field(Companion_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f) {
        CClassInfo *const default_class = cls(G_NORMAL_ATTACK);
        f = construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x60fb60) /* "Attack/Attack AI" */, 0xd0, 4, 0xd0, cls(G_ATTACK_AI), 1, default_class, 0, 0);
    }
    add_field(Companion_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60fb44) /* "Debug/Set Target Behavior" */, 0xec, 0x20, 0xec, 3);
    return add_field(Companion_ClassInfo(), f);
}

// ---- CSameAsParentAI ---------------------------------------------------------------------------------------------

// 0x561ce0 and 0x561e80 (slot 2)
static CClassInfo *__cdecl SameAsParent_ClassInfo()
{
    return class_getter(0x713178, 0x620ce8 /* "CSameAsParentAI" */, 0x40400000, G_LAYER_PART_AI, 0x14, 0x561d30, 0x561d70, 0x561ee0);
}
static void SameAsParent_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5fd080);
    obj_free_fields(o);
    dw(o, 0) = vt(0x5ed1d0);   // CLayerPartAI
    obj_free_fields(o);
    call_dtor(0x4fd080, o);
}
// 0x561ee0. eax: the last call's.
static DWORD __cdecl SameAsParent_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x620d14) /* "Force Invisable" */, 0x10, 1, 0x10, 0, 0, 0);
    add_field(SameAsParent_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x620cf8) /* "Only Show First Sequence" */, 0x11, 1, 0x11, 1, 0, 0);
    return add_field(SameAsParent_ClassInfo(), f);
}
// 0x561ff0 (slot 24, Begin): the entity's slot 59 with 0x200000. eax: its.
static DWORD THISCALL SameAsParent_Begin(BYTE *, BYTE *entity) { return vcall<DWORD>(entity, 59, 0x200000); }

// ---- CScanAreaAI -------------------------------------------------------------------------------------------------

// 0x565010 and 0x5651e0 (slot 2)
static CClassInfo *__cdecl ScanArea_ClassInfo()
{
    return class_getter(0x713214, 0x620f38 /* "CScanAreaAI" */, 0x40000000, G_PATROL_BASE, 0x4c, 0x565060, 0x5650b0, 0x565240);
}
static void ScanArea_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5ff1e0);
    obj_free_fields(o);
    zax::fn<void(THISCALL *)(void *)>(0x4defd0)(o + 0x10);   // "Turn Sequence Name"
    call_dtor(0x53bd70, o);                            // CPatrolBaseAI
}
// 0x565240. eax: the last call's.
static DWORD __cdecl ScanArea_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621098) /* "Starting Angle" */, glob_addr(0x60ff80) /* "Degrees" */, 0x14, 4, 0x14, 1,
                        0xc2340000 /* -45.0 */, 0, 0xc3b40000 /* -360.0 */, 0x43b40000 /* 360.0 */, 0x3e800000 /* 0.25 */);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621088) /* "Ending Angle" */, glob_addr(0x60ff80) /* "Degrees" */, 0x18, 4, 0x18, 1,
                  0x42340000 /* 45.0 */, 0, 0xc3b40000 /* -360.0 */, 0x43b40000 /* 360.0 */, 0x3e800000 /* 0.25 */);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x62106c) /* "Turning Speed" */, glob_addr(0x62107c) /* "Degrees/Sec" */, 0x1c, 4, 0x1c, 1,
                  0x41200000 /* 10.0 */, 0, 0, 0x47c35000 /* 100000.0 */, 0x3f000000 /* 0.5 */);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x621054) /* "Time Between Turning" */, glob_addr(0x60b000) /* "Seconds" */, 0x20, 4, 0x20, 1, 0, 0,
                  0, 0x47c35000 /* 100000.0 */, 0x3f000000 /* 0.5 */);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621040) /* "Start Out Waiting" */, 0x24, 1, 0x24, 1, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x621028) /* "Start Counterclockwise" */, 0x25, 1, 0x25, 1, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x621014) /* "Turn Sequence Name" */, 0x10, 4, 0x10, 1, glob_addr(0x60f8d0) /* "Idle" */, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = heap_alloc(0x3c);
    f = !f ? nullptr : construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x621004) /* "End Turn Action" */, 0x28, 4, 0x28, cls(G_ACTION), 1, 0, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x620ff0) /* "Execute Each Turn" */, 0x2c, 1, 0x2c, 1, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x620fdc) /* "Reset Each Begin" */, 0x2d, 1, 0x2d, 1, 1, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x620fc4) /* "Current Time To Wait" */, glob_addr(0x60b000) /* "Seconds" */, 0x44, 4, 0x44, 0, 0, 0, 0,
                  0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x620fb0) /* "Current Start Angle" */, 0x38, 4, 0x38, 0, 0, 0, 0, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x620f9c) /* "Current End Angle" */, 0x3c, 4, 0x3c, 0, 0, 0, 0, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x620f94) /* "Waiting" */, 0x40, 1, 0x40, 0, 1, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x620f84) /* "Turning Flag" */, 0x2e, 1, 0x2e, 0, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x620f6c) /* "Starting Facing Angle" */, 0x48, 4, 0x48, 0, 0xc479c000 /* -999.0 */, 0, 0, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x620f58) /* "Complete Turn Time" */, 0x34, 4, 0x34, 0, 0, 0, 0, 0, 0);
    add_field(ScanArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x620f44) /* "Current Turn Time" */, 0x30, 4, 0x30, 0, 0, 0, 0, 0, 0);
    return add_field(ScanArea_ClassInfo(), f);
}
// 0x565be0 (slot 23): for a CEntityAnimated, its facing angle (slot 46, st(0)) kept as "Starting Facing
// Angle" (+0x48). Void in use [assumption].
static void THISCALL ScanArea_Slot23(BYTE *self, BYTE *entity)
{
    if (entity && is_kind_of(entity, cls(G_ENTITY_ANIMATED)))
        fl(self, 0x48) = vcall<float>(entity, 46);
}

// ---- CScanInvestigateAI ------------------------------------------------------------------------------------------

// 0x566010 and 0x566280 (slot 2)
static CClassInfo *__cdecl ScanInvestigate_ClassInfo()
{
    return class_getter(0x71321c, 0x6210a8 /* "CScanInvestigateAI" */, 0x3f800000, G_INVESTIGATE_AREA, 0x5c, 0x566060, 0x5660f0, 0x5662e0);
}
static void ScanInvestigate_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5ff288);
    obj_free_fields(o);
    ScanArea_destroy(o + 0x10);   // the embedded CScanAreaAI ("AI")
    call_dtor(0x4dd860, o);       // CInvestigateAreaAI
}
// 0x5662e0. eax: the last call's.
static DWORD __cdecl ScanInvestigate_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x614e3c) /* "AI" */, 0x10, 0x4c, 0x10, 1);
    return add_field(ScanInvestigate_ClassInfo(), f);
}
// 0x566370 (slot 23), 0x566390 (24), 0x5663b0 (25), 0x5663d0 (41): passed to the embedded scan AI (+0x10)
static DWORD THISCALL ScanInvestigate_Slot23(BYTE *self, DWORD a) { return vcall<DWORD>(self + 0x10, 23, a); }
static DWORD THISCALL ScanInvestigate_Slot24(BYTE *self, DWORD a) { return vcall<DWORD>(self + 0x10, 24, a); }
static DWORD THISCALL ScanInvestigate_Slot25(BYTE *self, DWORD a, DWORD b) { return vcall<DWORD>(self + 0x10, 25, a, b); }
static DWORD THISCALL ScanInvestigate_Slot41(BYTE *self, DWORD a, DWORD b) { return vcall<DWORD>(self + 0x10, 41, a, b); }

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_chase_scan)
{
    const Hook list[] = {
#include "ai_chase_scan_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
