// The game's own entity behaviours and CCameraTrakerAI 0x5aa020-0x5ac570 (phase 3, AI; docs/DECOMPILATION.md
// section 8): CCameraTrakerAI, CEntityBehaviorWarpEffect, CEntityBehaviorFireWeapon with
// CEntityBehaviorFirePrimary and CEntityBehaviorFireSecondary (the fire and the drop of a secondary item
// 0x5ab390/0x5ab9a0), CEntityBehaviorRestartMap; between them the six block readers of the messages a
// client sends the server (registration, accessors, reads, write functions). Their Create and CreateArray
// are in ai_classes.cpp.
//
// Layouts [confirmed: constructors, RegisterFields, the slots below]:
//   CCameraTrakerAI (0x20, vtable 0x603c98, class 0x714030, version 1, parent CLayerPartAI): +0x10 "Camera
//     Tracking Mode" (a typed resource field over the enum list 0x714018, "Gradual"; the constructor writes
//     1), +0x14 "Active" (byte, 1), +0x18 "Camera Speed" (pixels per second, 1 to 1e8, the enum value 0
//     "Immediately"), +0x1c "Debug/Got Caught Up" (alternative name "Got Cought Up").
//   CEntityBehaviorWarpEffect (0x20, 0x603d40, 0x71403c, version 2, parent CRenderEffectBehavior): +0xc "Warp
//     In" (byte, 1), +0xd "Debug/Was Stationary", +0x10 "Warp Time" (1), +0x14 "Cur Warp Time", +0x18 "Warp
//     Effect" (owned CWarpEffect, default CSimpleWarpEffect; its +8 the effect's strength), +0x1c
//     "Debug/WhoWereConnectedTo" (a CEntityAnimated reference).
//   CEntityBehaviorFireWeapon (0x10, 0x603dc0, 0x714040, version 1, parent CEntityBehavior): +0xc "Angle To
//     Fire" (-1). CEntityBehaviorFirePrimary (0x603e38, 0x714044, version 2) and
//     CEntityBehaviorFireSecondary (0x603eb0, 0x714048, version 1) add nothing.
//   CEntityBehaviorRestartMap (0xc, 0x603f28, 0x71404c, version 1, parent CEntityBehavior).
//   The block readers are statics {vtable, BlockReader} made by the static initialisers (not in
//     docs/functions.csv): 0x714080 the client's options (vtable 0x603fa0), 0x714068 the CD key challenge
//     response (0x603fac), 0x714058 ready to spawn (0x603fb8), 0x714070 a chat message from the client
//     (0x603fc4), 0x714060 a team change request (0x603fd0), 0x714050 the client wants to continue past the
//     match over window (0x603fdc). Slot 2 reads; docs/functions.csv counts the reads as
//     CEntityBehaviorRestartMap's slots 32, 35, 38, 41, 44, 47 (its vtable dump runs into the reader
//     vtables).
//
// Slots (names ours): the behaviours' 23 Begin (entity; al: stays on the stack), 25 Update (seconds,
// entity); the AI's 25 Update (entity, seconds). Floating point follows the original instruction by
// instruction (x87.h). Return values: al where the original sets it, otherwise eax of the last call
// [assumption: void in use]. Where the original calls through a null pointer (no weapon item in
// DropSecondary, no projectile made) the reimplementation does the same.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ----------------------------------------------------------------------------------

constexpr DWORD G_APP = 0x713f14;
inline BYTE *app() { return *glob<BYTE **>(G_APP); }
CClassInfo *entity_base_class() { return fn<Getter>(0x487620)(); }
CClassInfo *entity_character_class() { return fn<Getter>(0x49a930)(); }
CClassInfo *entity_projectile_class() { return fn<Getter>(0x491930)(); }
CClassInfo *entity_behavior_class() { return fn<Getter>(0x494410)(); }
CClassInfo *render_effect_behavior_class() { return fn<Getter>(0x55e350)(); }

DWORD set_alt_name(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, glob<const char *>(text)); }
DWORD add_enum_float(void *f, DWORD value, GlobKey text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54ed60)(f, value, glob<const char *>(text)); }

// The item categories (0x6c0800) and the inventory
DWORD category(GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c0800), glob<const char *>(name), -1); }
BYTE *inventory_of(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(part); }
DWORD cur_index(void *inv, DWORD cat) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, cat); }
BYTE *cur_definition(void *inv, DWORD cat) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4252b0)(inv, cat); }
DWORD has_ammunition(void *def) { return fn<DWORD(THISCALL *)(void *)>(0x4d77c0)(def); }
// 0x5664b0 (mov eax, ecx; ret): an inventory item as the weapon item (not hookable)
BYTE *as_item(void *item) { return fn<BYTE *(THISCALL *)(void *)>(0x5664b0)(item); }
BYTE *item_definition(void *item) { return fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item); }
// 0x4ef9e0 (the world's player list +0x284; part): its index, -1 when it is none
DWORD player_index(void *part) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4ef9e0)(static_cast<BYTE *>(world()) + 0x284, part); }
DWORD play_named_sound(void *part, GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4fc8c0)(part, glob<const char *>(name)); }
constexpr DWORD DRYFIRE = 0x61e9f0, PRIMARY = 0x60b780, SECONDARY = 0x60b788, IDLE = 0x60f8d0;

// ---- CCameraTrakerAI -------------------------------------------------------------------------------------

constexpr DWORD CAMERA_VAR = 0x714030;
// 0x5aa020, 0x5aa1e0 (slot 2)
CClassInfo *__cdecl Camera_ClassInfo()
{
    return class_getter(CAMERA_VAR, 0x625d74 /* "CCameraTrakerAI" */, 1.0f, layer_part_ai_class, 0x20, 0, 0x5aa070, 0x5aa0c0, 0x5aa240);
}
// 0x5aa240. eax: the last call's.
DWORD __cdecl Camera_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x612d28) /* "Active" */, 0x14, 1, 0x14, 1, 1, 0);
    add_field(Camera_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61ccb0) /* "Camera Speed" */, glob_addr(0x61ccc0) /* "Pixel per sec" */, 0x18, 4, 0x18, 1, 0, 0,
                  0x3f800000 /* 1.0 */, 0x4cbebc20 /* 1e8 */, 0);
    add_field(Camera_ClassInfo(), f);
    add_enum_float(f, 0, 0x61cca4 /* "Immediately" */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x625dac) /* "Debug/Got Caught Up" */, 0x1c, 1, 0x1c, 3, 1, 0);
    add_field(Camera_ClassInfo(), f);
    set_alt_name(last_field(Camera_ClassInfo()), 0x625d9c /* "Got Cought Up" */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x625d84) /* "Camera Tracking Mode" */, 0x10, 4, 0x10, glob_addr(0x714018), 3,
                  glob_addr(0x625d64) /* "Gradual" */, 0, 0);
    return add_field(Camera_ClassInfo(), f);
}
void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
void Camera_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x603c98));
    layer_part_ai_destroy(o);
}
// 0x5aa140 (slot 1)
void *THISCALL Camera_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, Camera_destroy>(self, flags); }
// 0x5aa4d0: the out-of-line constructor (as Create's). eax: this.
BYTE *THISCALL Camera_Ctor(BYTE *self)
{
    layer_part_ai_ctor(self);
    vtable_and_defaults(self, vt(0x603c98));
    dw(self, 0x18) = 0;
    self[0x14] = 1;
    dw(self, 0x10) = 1;
    return self;
}
// 0x5aa510 (slot 25, Update (entity, seconds)): when active, the view follows the entity's position (its
// height (slot 54) taken off for a CEntityBase) for the entity's player in a network game (0 otherwise):
// at most "Camera Speed" * seconds pixels from the current view target, or straight there ("Debug/Got
// Caught Up" set) when the speed is not above 0 or the distance is within reach. eax: SetViewTarget's.
DWORD THISCALL Camera_Update(BYTE *self, BYTE *entity, DWORD seconds)
{
    if (!self[0x14])
        return 0;
    float pos[2];
    vcall<float *>(entity, 43, pos);
    if (entity && is_kind_of(entity, entity_base_class())) {
        const int h = vcall<int>(entity, 54);
        pos[1] = fstp(fld(pos[1]) - fild(h));
    }
    DWORD player = 0;
    if (vcall<DWORD>(vcall<void *>(world(), 1), 13) & 0xff) {
        player = fn<DWORD(THISCALL *)(void *)>(0x5bacd0)(entity);
        if (player == 0xffffffffu)
            player = 0;
    }
    if (!c03(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO)))) {
        const float t = fstp(fld(raw_float(seconds)) * fld(fl<float>(self, 0x18)));
        float cur[2];
        fn<void(THISCALL *)(void *, float *, DWORD)>(0x4f5da0)(world(), cur, player);
        const long double dx = fld(pos[0]) - fld(cur[0]);
        const long double dy = fld(pos[1]) - fld(cur[1]);
        float dyf = fstp(dy);
        const long double dydy = dy * fld(dyf);
        const long double dxdx = dx * dx;
        if (c0(fld(t) * fld(t), dydy + dxdx)) {
            const long double len = fsqrt(dydy + dxdx);
            long double nx = dx;
            if (!c3(len, cst(glob_addr(C_ZERO)))) {
                const long double inv = cst(glob_addr(C_ONE)) / len;
                nx = dx * inv;
                dyf = fstp(fld(dyf) * inv);
            }
            const long double mx = nx * fld(t);
            dyf = fstp(fld(dyf) * fld(t));
            pos[0] = fstp(mx + fld(cur[0]));
            pos[1] = fstp(fld(dyf) + fld(cur[1]));
            return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4f5dd0)(world(), float_bits(pos[0]), float_bits(pos[1]),
                                                                                        dw(self, 0x10), player);
        }
    }
    self[0x1c] = 1;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4f5dd0)(world(), float_bits(pos[0]), float_bits(pos[1]),
                                                                                dw(self, 0x10), player);
}

// ---- CEntityBehaviorWarpEffect ------------------------------------------------------------------------------

constexpr DWORD WARP_VAR = 0x71403c;
constexpr DWORD K_THIRD = 0x603db8, K_TWO_THIRDS = 0x603dbc, K_ONE_HALF = 0x5fcc30, K_THREE = 0x5ec838;   // 1/3, 2/3, 1.5, 3
// 0x5aa670, 0x5aa820 (slot 2)
CClassInfo *__cdecl Warp_ClassInfo()
{
    return class_getter(WARP_VAR, 0x625dc0 /* "CEntityBehaviorWarpEffect" */, 2.0f, render_effect_behavior_class, 0x20, 0, 0x5aa6c0, 0x5aa700,
                        0x5aa880);
}
// 0x5aa880. eax: the last call's.
DWORD __cdecl Warp_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x614744) /* "Warp In" */, 0xc, 1, 0xc, 1, 1, 0);
    add_field(Warp_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x625e20) /* "Warp Time" */, 0x10, 4, 0x10, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    add_field(Warp_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f) {
        CClassInfo *const default_class = class_at(fn_addr(0x5718d0));   // CSimpleWarpEffect
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x621b14) /* "Warp Effect" */, 0x18, 4, 0x18, class_at(fn_addr(0x5963e0)) /* CWarpEffect */, 1, default_class, 0, 0);
    }
    add_field(Warp_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x625e10) /* "Cur Warp Time" */, 0x14, 4, 0x14, 0, 0, 0, 0, 0, 0);
    add_field(Warp_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x625df8) /* "Debug/Was Stationary" */, 0xd, 1, 0xd, 0, 0, 0);
    add_field(Warp_ClassInfo(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD_CTOR)(
            f, glob_addr(0x625ddc) /* "Debug/WhoWereConnectedTo" */, 0x1c, 4, 0x1c, entity_animated_class(), 3, 0);
    return add_field(Warp_ClassInfo(), f);
}
// The destructor's body: the entity it is connected to gets the effect taken off (slot 91)
void Warp_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x603d40);
    if (void *e = bp(dw(o, 0x1c))) {
        vcall<DWORD>(e, 91, dw(o, 0x18));
        dw(o, 0x1c) = 0;
    }
    obj_free_fields(o);
    fn<void(THISCALL *)(void *)>(0x55e450)(o);   // CRenderEffectBehavior
}
// 0x5aa760 (slot 1)
void *THISCALL Warp_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, Warp_destroy>(self, flags); }
void warp_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x55e400)(o);   // CRenderEffectBehavior
    vtable_and_defaults(o, vt(0x603d40));
    dw(o, 0x14) = 0;
    dw(o, 0x1c) = 0;
}
// 0x5aabb0: the out-of-line constructor. eax: this.
BYTE *THISCALL Warp_Ctor(BYTE *self)
{
    warp_construct(self);
    return self;
}
// 0x5aabe0 (fastcall: "Warp In", the effect; the warp time, two unused): a new behaviour from the pool
// with its fields reset (slot 19), the effect replacing the one the reset made. eax: the behaviour.
BYTE *__fastcall Warp_Make(DWORD warp_in, DWORD effect, DWORD time, DWORD, DWORD)
{
    BYTE *b = static_cast<BYTE *>(pool_alloc(0x20));
    if (b)
        warp_construct(b);
    vcall<DWORD>(b, 19);
    b[0xc] = (BYTE)warp_in;
    dw(b, 0x10) = time;
    if (void *old = bp(dw(b, 0x18))) {
        vcall<void *>(old, 1, 1);
        dw(b, 0x18) = 0;
    }
    dw(b, 0x18) = effect;
    return b;
}
// 0x5aac50 (slot 23, Begin): whether the entity was stationary (flag 0x200000) kept, the flag cleared
// (slot 59), the effect put on the entity (slot 81). al: 1.
DWORD THISCALL Warp_Begin(BYTE *self, BYTE *entity)
{
    self[0xd] = (BYTE)((dw(entity, 0x1c) >> 21) & 1);
    vcall<DWORD>(entity, 59, 0x200000);
    vcall<DWORD>(entity, 81, dw(self, 0x18));
    dw(self, 0x1c) = (DWORD)(uintptr_t)entity;
    return 1;
}
// 0x5aac90 (slot 25, Update (seconds, entity)): the fraction r of "Warp Time" passed (at most 1); warping
// in, the entity fades in over the first third (3 r) while the effect's strength falls over the last
// third (1 - 1.5 (r - 1/3)) ... out, the effect rises over the first two thirds (1.5 r) and the entity
// fades out over the last (1 - 3 (r - 2/3)). Done: the entity fully in or out, the effect taken off (slot
// 91), the stationary flag set again (slot 58) if it was. al: still running.
DWORD THISCALL Warp_Update(BYTE *self, DWORD seconds, BYTE *entity)
{
    if (c0(fld(fl<float>(self, 0x14)), fld(fl<float>(self, 0x10)))) {
        const long double t = fld(raw_float(seconds)) + fld(fl<float>(self, 0x14));
        fl<float>(self, 0x14) = fstp(t);
        const long double q = t / fld(fl<float>(self, 0x10));
        float r = fstp(q);
        if (!c03(q, cst(glob_addr(C_ONE))))
            r = 1.0f;
        BYTE *const effect = bp(dw(self, 0x18));
        if (!self[0xc]) {
            long double a = cst(glob_addr(C_ONE));
            if (c0(fld(r), cst(glob_addr(K_TWO_THIRDS))))
                a = fld(r) * cst(glob_addr(K_ONE_HALF));
            if (effect)
                fl<float>(effect, 8) = fstp(a);
            long double b = cst(glob_addr(C_ONE));
            if (!c03(fld(r), cst(glob_addr(K_TWO_THIRDS))))
                b = cst(glob_addr(C_ONE)) - (fld(r) - cst(glob_addr(K_TWO_THIRDS))) * cst(glob_addr(K_THREE));
            fl<float>(entity, 0xb4) = fstp(b);
            return ((DWORD)(uintptr_t)entity & ~0xffu) | 1;
        }
        long double a = cst(glob_addr(C_ONE));
        if (c0(fld(r), cst(glob_addr(K_THIRD))))
            a = fld(r) * cst(glob_addr(K_THREE));
        fl<float>(entity, 0xb4) = fstp(a);
        long double b = cst(glob_addr(C_ONE));
        if (!c03(fld(r), cst(glob_addr(K_THIRD))))
            b = cst(glob_addr(C_ONE)) - (fld(r) - cst(glob_addr(K_THIRD))) * cst(glob_addr(K_ONE_HALF));
        if (effect)
            fl<float>(effect, 8) = fstp(b);
        return 1;
    }
    dw(entity, 0xb4) = self[0xc] ? 0x3f800000u : 0;
    vcall<DWORD>(entity, 91, dw(self, 0x18));
    if (self[0xd])
        vcall<DWORD>(entity, 58, 0x200000);
    return 0;
}

// ---- CEntityBehaviorFireWeapon, FirePrimary, FireSecondary ----------------------------------------------------

constexpr DWORD FIRE_VAR = 0x714040, PRIMARY_VAR = 0x714044, SECONDARY_VAR = 0x714048;
// 0x5aae00, 0x5aaf60 (slot 2)
CClassInfo *__cdecl FireWeapon_ClassInfo()
{
    return class_getter(FIRE_VAR, 0x625e2c /* "CEntityBehaviorFireWeapon" */, 1.0f, entity_behavior_class, 0x10, 0, 0x5aae50, 0x5aae80, 0x5aafc0);
}
// 0x5ab050, 0x5ab240 (slot 2)
CClassInfo *__cdecl FirePrimary_ClassInfo()
{
    return class_getter(PRIMARY_VAR, 0x625e58 /* "CEntityBehaviorFirePrimary" */, 2.0f, FireWeapon_ClassInfo, 0x10, 0, 0x5ab100, 0x5ab140,
                        0x58ea40);
}
// 0x5ab650, 0x5ab840 (slot 2)
CClassInfo *__cdecl FireSecondary_ClassInfo()
{
    return class_getter(SECONDARY_VAR, 0x625e74 /* "CEntityBehaviorFireSecondary" */, 1.0f, FireWeapon_ClassInfo, 0x10, 0, 0x5ab700, 0x5ab740,
                        0x58ea40);
}
// 0x5aafc0. eax: AddField's.
DWORD __cdecl FireWeapon_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x625e48) /* "Angle To Fire" */, 0xc, 4, 0xc, 0, 0xbf800000 /* -1.0 */, 0, 0, 0, 0);
    return add_field(FireWeapon_ClassInfo(), f);
}
void behavior_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4944e0)(o); }   // CEntityBehavior
void FireWeapon_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x603dc0));
    behavior_dtor(o);
}
template <DWORD VT> void fire_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    FireWeapon_destroy(o);
}
// 0x5aaee0, 0x5ab1a0, 0x5ab7a0 (slot 1)
void *THISCALL FireWeapon_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, FireWeapon_destroy>(self, flags); }
void *THISCALL FirePrimary_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, fire_destroy<0x603e38>>(self, flags); }
void *THISCALL FireSecondary_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, fire_destroy<0x603eb0>>(self, flags); }
// 0x5ab300, 0x5ab900 (the angle; ecx unused): a new behaviour from the pool, its fields reset (slot 19),
// "Angle To Fire" set. eax: the behaviour.
template <DWORD VT> BYTE *__stdcall Fire_Make(DWORD angle)
{
    BYTE *b = static_cast<BYTE *>(pool_alloc(0x10));
    if (b) {
        fn<void *(THISCALL *)(void *)>(0x494470)(b);   // CEntityBehavior
        vtable_and_defaults(b, vt(0x603dc0));
        vtable_and_defaults(b, vt(VT));
    }
    vcall<DWORD>(b, 19);
    dw(b, 0xc) = angle;
    return b;
}

// The weapon of a category: the inventory's current item (slot 26) as the weapon item
BYTE *current_weapon(BYTE *inv, DWORD c) { return as_item(vcall<BYTE *>(inv, 26, cur_index(inv, c))); }
// The discharge action executed on the stack (CDischargeWeaponThisFrameBaseAction, 0x20 bytes: CAction,
// vtables 0x5f5ff0 and 0x5f66f4, Set (0, 0, 0, 1.0)) for the character, the behaviour as the caller
void discharge(BYTE *self, BYTE *entity, BYTE *character)
{
    alignas(4) BYTE action[0x20];
    fn<void *(THISCALL *)(void *)>(0x41a6b0)(action);
    vtable_and_defaults(action, vt(0x5f5ff0));
    vtable_and_defaults(action, vt(0x5f66f4));
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4c4dd0)(action, 0, 0, 0, 0x3f800000);
    fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, void *)>(0x41a740)(action, bp(dw(entity, 0x48)), character, 0, 0, self);
    vtable_and_free(action, vt(0x5f66f4));
    vtable_and_free(action, vt(0x5f5ff0));
    fn<void(THISCALL *)(void *)>(0x41a700)(action);
}
// 0x5ab390 (thiscall: entity, the stack's count before Begin): the root character's primary weapon (slot
// 38 asked with the character): its fire (slot 35, the sequence; slot 23) as a PlaySequenceOnce back to
// the current sequence, pushed in front (with the old count as the priority) when the stack was empty,
// appended otherwise; when that was not taken the discharge action runs at once. Without a weapon that can
// fire: no current weapon at all plays "Dryfire.wav" for a player, otherwise the definition's ammunition
// is asked (and ignored). al: 0.
DWORD THISCALL FirePrimary_Fire(BYTE *self, BYTE *entity, DWORD count)
{
    BYTE *ch = fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(entity);
    if (ch && !(is_kind_of(ch, entity_character_class()) & 0xff))
        ch = nullptr;
    const DWORD c0 = category(PRIMARY);
    const DWORD index = cur_index(inventory_of(ch), c0);
    const DWORD c1 = category(PRIMARY);
    BYTE *weapon = current_weapon(inventory_of(ch), c1);
    if (weapon && (vcall<DWORD>(weapon, 38, ch) & 0xff)) {
        const DWORD sequence = vcall<DWORD>(weapon, 35, entity);
        vcall<DWORD>(weapon, 23, entity);
        using Make = BYTE *(__fastcall *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
        BYTE *stack = entity + 0x90;
        if (!dw(entity, 0x9c)) {
            const DWORD back = fn<DWORD(THISCALL *)(void *)>(0x48b000)(entity);
            BYTE *b = fn<Make>(0x496e20)(entity, sequence, 0x3f800000, back, 0xbf800000, 0, 0xffffffff);
            if (stack_push(stack, entity, b, count))
                return 0;
        } else {
            const DWORD back = fn<DWORD(THISCALL *)(void *)>(0x48b000)(entity);
            BYTE *b = fn<Make>(0x496e20)(entity, sequence, 0x3f800000, back, 0xbf800000, 0, 0xffffffff);
            if (stack_append(stack, b))
                return 0;
        }
        discharge(self, entity, ch);
        return 0;
    }
    if (index == 0xffffffffu) {
        if (player_index(ch) != 0xffffffffu)
            play_named_sound(ch, DRYFIRE);
        return 0;
    }
    BYTE *inv = inventory_of(ch);
    has_ammunition(cur_definition(inv, category(PRIMARY)));
    return 0;
}
// 0x5ab360 (slot 23, Begin): taken off the stack (0x49a650), then fired. al: 1.
DWORD THISCALL FirePrimary_Begin(BYTE *self, BYTE *entity)
{
    const DWORD count = dw(entity, 0x9c);
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x49a650)(entity + 0x90, entity, self);
    return (fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5ab390)(self, entity, count) & ~0xffu) | 1;
}
// 0x5ab9a0 (thiscall: the character): the current secondary item (no null test) that can be used (slot
// 38): its definition's ammunition used up through the part (Part_UseItems), or its negative "Energy"
// taken from the battery (slot 50); a new projectile of the definition's model (CEntityProjectile, the
// character as the creator) at the character's position (slot 41), for a projectile "Idle" and the
// character's aim (slot 114 into slot 108, as x, y, x); flags 0x80000 set (slot 58), the stationary flag cleared (slot
// 59), added to the character's layer; the definition's action (+0x54) executed with the projectile. Not
// usable: as the primary fire. al: 0.
DWORD THISCALL DropSecondary(BYTE *, BYTE *ch)
{
    BYTE *inv = inventory_of(ch);
    const DWORD index = cur_index(inv, category(SECONDARY));
    BYTE *item = current_weapon(inv, category(SECONDARY));
    if (!(vcall<DWORD>(item, 38, ch) & 0xff)) {
        if (index == 0xffffffffu) {
            if (player_index(ch) != index)
                play_named_sound(ch, DRYFIRE);
            return 0;
        }
        BYTE *def = cur_definition(inv, category(SECONDARY));
        if (dw(def, 0x50))
            has_ammunition(def);
        return 0;
    }
    BYTE *def = item_definition(item);
    vcall<DWORD>(item, 23, ch);
    if (has_ammunition(def) & 0xff) {
        fn<void(__fastcall *)(void *, DWORD, DWORD)>(0x426930)(ch, dw(def, 0x2c), dw(def, 0x50));
    } else if (c0(fld(fl<float>(def, 0x4c)), cst(glob_addr(C_ZERO)))) {
        if (BYTE *battery = fn<BYTE *(THISCALL *)(void *)>(0x5b0da0)(inv))
            vcall<DWORD>(battery, 50, float_bits(fstp(-fld(fl<float>(def, 0x4c)))));
    }
    const DWORD model = dw(def, 0x20);
    BYTE *p = fn<BYTE *(THISCALL *)(void *, DWORD, CClassInfo *, void *, DWORD, DWORD)>(0x5176f0)(glob<void *>(0x6cfdd8), model,
                                                                                                    entity_projectile_class(), ch, 0, 0xffffffff);
    float pos[2];
    part_position(ch, pos);
    vcall<DWORD>(p, 41, float_bits(pos[0]), float_bits(pos[1]));
    if (p && (is_kind_of(p, entity_projectile_class()) & 0xff)) {
        ent_play_sequence(p, glob_addr(IDLE));
        const DWORD *aim = vcall<DWORD *>(ch, 114);
        DWORD v[3] = {aim[0], aim[1], aim[0]};   // the original's third dword is the first again (its copy's stack slot)
        vcall<DWORD>(p, 108, v);
    }
    vcall<DWORD>(p, 58, 0x80000);
    vcall<DWORD>(p, 59, 0x200000);
    BYTE *layer = bp(dw(ch, 0x48));
    fn<void(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, p);
    if (void *action = bp(dw(def, 0x54)))
        action_execute(action, layer, p, nullptr, ch, def, def + 0x54);
    return 0;
}
// 0x5ab960 (slot 23, Begin): for a character: taken off the stack, the secondary item dropped. al: 1 then,
// 0 otherwise.
DWORD THISCALL FireSecondary_Begin(BYTE *self, BYTE *entity)
{
    if (entity && (is_kind_of(entity, entity_character_class()) & 0xff)) {
        fn<DWORD(THISCALL *)(void *, void *, void *)>(0x49a650)(entity + 0x90, entity, self);
        return (fn<DWORD(THISCALL *)(void *, void *)>(0x5ab9a0)(self, entity) & ~0xffu) | 1;
    }
    return 0;
}

// ---- CEntityBehaviorRestartMap -------------------------------------------------------------------------------

constexpr DWORD RESTART_VAR = 0x71404c;
// 0x5abbb0, 0x5abd10 (slot 2)
CClassInfo *__cdecl Restart_ClassInfo()
{
    return class_getter(RESTART_VAR, 0x625e94 /* "CEntityBehaviorRestartMap" */, 1.0f, entity_behavior_class, 0xc, 0, 0x5abc00, 0x5abc30,
                        0x58ea40);
}
void Restart_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x603f28));
    behavior_dtor(o);
}
// 0x5abc90 (slot 1)
void *THISCALL Restart_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, Restart_destroy>(self, flags); }
// 0x5abd70 (slot 23, Begin): the game object's "restart the map" flag (+0x338). al: 1.
DWORD THISCALL Restart_Begin(BYTE *, BYTE *)
{
    app()[0x338] = 1;
    return 1;
}

// ---- The block readers of the client's messages ----------------------------------------------------------------

DWORD pk_block_type(void *p, void *reader, DWORD kind) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(p, reader, kind); }
DWORD pk_write_dword(void *p, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x520940)(p, v); }
DWORD pk_write_bool(void *p, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x520a50)(p, v); }
DWORD pk_write_text(void *p, const char *t) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x520770)(p, t); }
DWORD pk_read_dword(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520550)(p, out); }
const char *pk_read_text(void *p) { return fn<const char *(THISCALL *)(void *)>(0x5206c0)(p); }
// The world's net game (slot 1), its net layer (slot 44): the packet (slot 25), the sender's index (slot
// 28) and player (slot 29)
BYTE *net_game() { return vcall<BYTE *>(world(), 1); }
BYTE *net_layer() { return vcall<BYTE *>(world(), 44); }
BYTE *player_at(DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x51f440)(net_game(), i); }
// 0x59ff80: the game object's game type
BYTE *game_type() { return fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(app()); }
// 0x5b7aa0 (mov eax, [ecx+0x1c]; ret): a net player's name (not hookable)
ZString *player_name(void *p) { return fn<ZString *(THISCALL *)(void *)>(0x5b7aa0)(p); }
// 0x59b260 (game object; text, font): a message on the screen
DWORD show_text(const char *text, DWORD font) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x59b260)(app(), text, font); }
void zs_format1(ZString *z, const char *fmt, const char *a) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, fmt, a); }
void zs_format2(ZString *z, const char *fmt, const char *a, const char *b) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, fmt, a, b); }

template <DWORD READER> DWORD __cdecl Reader_Get() { return glob_addr(READER); }

// 0x5abd90 (thiscall on the packet registry): the six readers registered by name. eax: the last call's.
DWORD THISCALL Readers_Register(void *self)
{
    const DWORD readers[][2] = {{0x714080, 0x625f78}, {0x714068, 0x625f48}, {0x714058, 0x625f28},
                                           {0x714070, 0x625f04}, {0x714060, 0x625ee0}, {0x714050, 0x625eb0}};
    DWORD r = 0;
    for (const auto &e : readers)
        r = fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(self, glob_dyn<void *>(e[0]), glob_dyn<const char *>(e[1]));
    return r;
}

// 0x5abe70 (the client's options): read into the sender's options (+0x1c of the net player: name, two
// colours, a byte); its character (or the first child) gets the colours as floats in its dynamic value
// (+0xc after the game type's slot 39 may change the first, +0x18), then the client's build: an older one
// is disconnected (net game slot 20, "Version", the message 0x714078).
DWORD THISCALL Options_Read(void *, void *packet)
{
    const DWORD sender = vcall<DWORD>(net_layer(), 28);
    BYTE *player = player_at(sender);
    BYTE *options = bp(dw(player, 0x1c));
    fn<DWORD(THISCALL *)(void *, void *)>(0x520700)(packet, options);
    pk_read_dword(packet, options + 4);
    pk_read_dword(packet, options + 8);
    fn<DWORD(THISCALL *)(void *, void *)>(0x520580)(packet, options + 0xc);
    BYTE *part = *at<BYTE **>((DWORD)(uintptr_t)fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4ef8e0)(static_cast<BYTE *>(world()) + 0x284, sender));
    if (part) {
        if (fn<DWORD(THISCALL *)(void *)>(0x4fc7c0)(part))
            part = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4fc7d0)(part, 0);
        void *desc = *glob<void **>(0x6c0520);
        if (part && (fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(desc, part) & 0xff)) {
            BYTE *dyn = fn<BYTE *(THISCALL *)(void *, void *)>(0x418790)(desc, part);
            float c1 = fstp(fild64((long long)dw(options, 4)));
            if (game_type())
                vcall<DWORD>(game_type(), 39, player, &c1);
            fl<float>(dyn, 0xc) = fstp(fld(c1));
            fl<float>(dyn, 0x18) = fstp(fild64((long long)dw(options, 8)));
        }
    }
    DWORD build = (DWORD)(uintptr_t)packet;   // the original reads into a stack slot it used before
    pk_read_dword(packet, &build);
    if (build < fn<DWORD(__cdecl *)()>(0x5908b0)()) {
        BYTE *net = net_game();
        const char *text = zs_c_str(glob<void *>(0x714078));
        return vcall<DWORD>(net, 20, sender, glob<const char *>(0x60c21c) /* "Version" */, text);
    }
    return build;
}
// 0x5abfe0 (thiscall on the reader): in a network game, the local options (+0xc of the net game) into the
// net layer's packet with the build (0x5908a0).
void THISCALL Options_Write(void *self)
{
    if (!(vcall<DWORD>(net_game(), 13) & 0xff))
        return;
    BYTE *p = vcall<BYTE *>(net_layer(), 25);
    BYTE *options = bp(dw(net_game(), 0xc));
    pk_block_type(p, self, 0);
    pk_write_text(p, zs_c_str(options));
    pk_write_dword(p, dw(options, 4));
    pk_write_dword(p, dw(options, 8));
    pk_write_bool(p, options[0xc]);
    pk_write_dword(p, fn<DWORD(__cdecl *)()>(0x5908a0)());
}
// 0x5ac0b0 (cdecl: the player's id, the result, the error text, unused): GameSpy's answer to a CD key
// check; a failed one disconnects the player (net game slot 20, "CD Key").
void __cdecl CdKey_Answer(DWORD id, DWORD result, DWORD text, DWORD)
{
    if (result)
        return;
    BYTE *net = net_game();
    const DWORD player = fn<DWORD(THISCALL *)(void *, DWORD)>(0x51f530)(net_game(), id);
    vcall<DWORD>(net, 20, player, glob<const char *>(0x60f80c) /* "CD Key" */, text);
}
// 0x5ac100 (the CD key challenge response): the sender's response (+0x28 "answered" set) to GameSpy's
// gcd_authenticate_user with its id (+0xc), address (net game slot 21) and challenge (+0x24). eax: its.
DWORD THISCALL CdKey_Read(void *, void *packet)
{
    BYTE *player = player_at(vcall<DWORD>(net_layer(), 28));
    const char *response = pk_read_text(packet);
    player[0x28] = 1;
    const DWORD address = vcall<DWORD>(net_game(), 21, player);
    return fn<DWORD(__cdecl *)(DWORD, DWORD, const char *, const char *, DWORD, DWORD)>(0x5c5540)(
        dw(player, 0xc), address, zs_c_str(player + 0x24), response, fn_addr(0x5ac0b0), 0);
}
// 0x5ac180 (thiscall on the reader: packet, response). al: 1.
DWORD THISCALL CdKey_Write(void *self, void *packet, const char *response)
{
    pk_block_type(packet, self, 0);
    return (pk_write_text(packet, response) & ~0xffu) | 1;
}
// 0x5ac220 (ready to spawn): a sender that did not answer the CD key challenge is disconnected ("Protocol
// Breach"); otherwise it spawns in the current layer (game object slot 113), its character's slot 33
// runs, the layer's part flags are sent to it and "%s joined the game" (0x71407c) is shown.
void THISCALL Ready_Read(void *, void *)
{
    BYTE *nl = net_layer();
    const DWORD sender = vcall<DWORD>(nl, 28);
    BYTE *who = vcall<BYTE *>(nl, 29);
    if (!who[0x28]) {
        vcall<DWORD>(net_game(), 20, sender, glob<const char *>(0x626014) /* "Protocol Breach" */,
                     glob<const char *>(0x626024) /* "Failed to respond to CD Key challenge." */);
        return;
    }
    BYTE *const g = app();
    vcall<DWORD>(g, 113, fn<void *(THISCALL *)(void *)>(0x4f1050)(g), sender, 0, 0);
    vcall<DWORD>(fn<void *(THISCALL *)(void *, DWORD)>(0x4f5d60)(app(), sender), 33);
    BYTE *layer = fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(app());
    fn<void(THISCALL *)(void *, void *)>(0x4e77e0)(layer + 0x3c, who);
    BYTE *player = player_at(sender);
    ZString text;
    zs_ctor(reinterpret_cast<BYTE *>(&text));
    ZString *name = player_name(player);
    const char *fmt = zs_c_str(glob<void *>(0x71407c));
    zs_format1(&text, fmt, zs_c_str(name));
    show_text(zs_c_str(&text), 0xffffffff);
    zs_dtor(reinterpret_cast<BYTE *>(&text));
}
// 0x5ac330 (thiscall on the ready-to-spawn reader: the packet). al: 1.
DWORD THISCALL Ready_Write(void *self, void *packet) { return (pk_block_type(packet, self, 0) & ~0xffu) | 1; }
// 0x5ac390 (a chat message from the client): "%s: %s" with the sender's name, shown in the font of the
// custom rules' dialogue display (the rules +0x38 of the game object's +0x2e0, when they are
// CZaxCustomRules; the display's +0xc).
void THISCALL Chat_Read(void *, void *packet)
{
    BYTE *who = vcall<BYTE *>(net_layer(), 29);
    const char *message = pk_read_text(packet);
    ZString text;
    zs_ctor(reinterpret_cast<BYTE *>(&text));
    ZString *name = player_name(who);
    zs_format2(&text, glob<const char *>(0x61107c) /* "%s: %s" */, zs_c_str(name), message);
    BYTE *holder = bp(dw(app(), 0x2e0));
    BYTE *rules = bp(dw(holder, 0x38));
    if (rules && !(is_kind_of(rules, fn<Getter>(0x5a1350)()) & 0xff))
        rules = nullptr;
    BYTE *display = fn<BYTE *(THISCALL *)(void *)>(0x5a1e80)(rules);
    show_text(zs_c_str(&text), dw(display, 0xc));
    zs_dtor(reinterpret_cast<BYTE *>(&text));
}
// 0x5ac440 (thiscall on the chat reader: packet, text). al: 1.
DWORD THISCALL Chat_Write(void *self, void *packet, const char *text)
{
    pk_block_type(packet, self, 3);
    return (pk_write_text(packet, text) & ~0xffu) | 1;
}
// 0x5ac4b0 (a team change request): passed to the game type's slot 47 with the sender.
void THISCALL Team_Read(void *, void *)
{
    const DWORD sender = vcall<DWORD>(net_layer(), 28);
    if (game_type())
        vcall<DWORD>(game_type(), 47, sender);
}
// 0x5ac500 (thiscall on the continue reader): into the net layer's packet. al: 1.
DWORD THISCALL Continue_Write(void *self)
{
    BYTE *p = vcall<BYTE *>(net_layer(), 25);
    return (pk_block_type(p, self, 0) & ~0xffu) | 1;
}
// 0x5ac570 (the client wants to continue past the match over window): a multiplayer game type counts it
// (PlayerReady).
void THISCALL Continue_Read(void *, void *)
{
    BYTE *t = game_type();
    if (t && (is_kind_of(t, fn<Getter>(0x51a270)()) & 0xff))
        fn<DWORD(THISCALL *)(void *)>(0x51c800)(t);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(zax_behaviors)
{
    const Hook list[] = {
#include "zax_behaviors_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
