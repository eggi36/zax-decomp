// The entity behaviours and the behaviour stack reimplemented (see ai_behaviors.h).
//
// The destructors are inlined into the deleting destructors in the exe, one level per class (the
// level's vtable, Object_FreeFields, the members), down to CEntityBehavior's out-of-line level
// 0x4944e0; slot 1 of the concrete classes destroys an array from its last element when bit 1 is set,
// bit 0 frees. RegisterFields make one descriptor after the other and append it through the class
// getter, which they inline (a failed allocation appends a null descriptor as in the original). The
// angles are computed as the exe does on the FPU: long double for the values it keeps on the stack,
// float wherever it stores one (to_float), comparisons as fcom + fnstsw (x87_lt: less or unordered).
// Strings, float constants and other values are the exe's, by address.
#include "ai_behaviors.h"
#include "exe_base.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static void obj_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_reset_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x417230)(o); }
static bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
// 0x46cd50: CObject's slot 0 (a message from a sender)
static DWORD obj_on_message(void *o, void *sender, const char *text)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x46cd50)(o, sender, text);
}
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void zs_ctor(BYTE *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(BYTE *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static bool zs_is_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13d0)(z); }
static void zs_clear(void *z) { fn<void(THISCALL *)(void *)>(0x4df2b0)(z); }
static void zs_assign(void *z, const void *v) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
static void zs_assign_text(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
static int crt_memmove(void *dst, const void *src, DWORD n) { return fn<int(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(dst, src, n); }

// Class getters of other classes
static CClassInfo *entity_animated_class() { return fn<Getter>(0x48dcc0)(); }
static CClassInfo *entity_movable_class() { return fn<Getter>(0x4903f0)(); }   // CEntityMovable
static CClassInfo *entity_walking_class() { return fn<Getter>(0x49b810)(); }   // CEntityWalking

// CEntityBehavior's constructor (0x494470, ai_classes.cpp) and CFidgetData's (0x4a04e0)
static void behavior_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x494470)(o); }
static void *fidget_data_ctor(void *o) { return fn<void *(THISCALL *)(void *)>(0x4a04e0)(o); }

// The entity (CEntityAnimated, Objects): its model (0x48b290), its sequence player (0x48b590) and the
// functions the behaviours call
static BYTE *ent_model(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
static BYTE *ent_player(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
static void ent_set_sequence(void *e, const char *name) { fn<void(THISCALL *)(void *, const char *)>(0x48f700)(e, name); }
static void ent_set_frame_index(void *e, DWORD frame) { fn<void(THISCALL *)(void *, DWORD)>(0x490010)(e, frame); }
static void ent_set_frame(void *e, float frame) { fn<void(THISCALL *)(void *, float)>(0x48feb0)(e, frame); }
static void ent_set_play_ratio_bits(void *e, DWORD ratio) { fn<void(THISCALL *)(void *, DWORD)>(0x48f6a0)(e, ratio); }
static bool ent_backwards(void *e) { return fn<bool(THISCALL *)(void *)>(0x48f260)(e); }
static void *ent_motion(void *e, void *player) { return fn<void *(THISCALL *)(void *, void *)>(0x48fe80)(e, player); }
static void ent_move(void *e, float x, float y) { fn<void(THISCALL *)(void *, float, float)>(0x491230)(e, x, y); }
static void ent_died(void *e) { fn<void(THISCALL *)(void *)>(0x4fbf30)(e); }
// Entity slots
static void ent_set_flags(void *e, DWORD mask) { vcall<void>(e, 58, mask); }
static void ent_clear_flags(void *e, DWORD mask) { vcall<void>(e, 59, mask); }
static long double ent_cur_frame(void *e) { return vcall<long double>(e, 84); }
static DWORD ent_slot47(void *e) { return vcall<DWORD>(e, 47); }
// The model (CModel) and the sequence player
static int model_find_sequence(BYTE *m, const char *name) { return fn<int(THISCALL *)(void *, const char *)>(0x517240)(m, name); }
static long double model_turn_speed(BYTE *m) { return fn<long double(THISCALL *)(void *)>(0x517510)(m); }
static long double player_fps(BYTE *p) { return vcall<long double>(p, 26); }
static DWORD player_frames(BYTE *p) { return *reinterpret_cast<DWORD *>(p + 0x48); }
static void player_play_forward(BYTE *p, float from, float to, void *e) { fn<void(THISCALL *)(void *, float, float, void *)>(0x560300)(p, from, to, e); }
static void player_play_backward(BYTE *p, float from, float to, void *e) { fn<void(THISCALL *)(void *, float, float, void *)>(0x560480)(p, from, to, e); }
static void player_motion(BYTE *p, float *out, void *motion, float from, float to, DWORD forward)
{
    fn<void(THISCALL *)(void *, float *, void *, float, float, DWORD)>(0x55fc70)(p, out, motion, from, to, forward);
}
// 0x55e4b0: the synchronised frame (ecx the frame count, edx 0, the frames per second)
static DWORD sync_frame(DWORD frames, float fps) { return fn<DWORD(__fastcall *)(DWORD, DWORD, float)>(0x55e4b0)(frames, 0, fps); }
// Layer parts (CLayerPart): squared distance and angle to another part, listeners
static long double part_distance2(void *a, void *b, float y_scale)
{
    return fn<long double(__fastcall *)(void *, void *, float)>(0x4fbf60)(a, b, y_scale);
}
static long double part_angle(void *a, void *b, DWORD x, float scale) { return fn<long double(THISCALL *)(void *, void *, DWORD, float)>(0x4fb2d0)(a, b, x, scale); }
static void part_add_listener(void *part, void *l) { fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, l, -1, glob<const char *>(0x62e558)); }
static void part_remove_listener(void *part, void *l) { fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, l, -1, glob<const char *>(0x62e558)); }
static void *part_position(void *part, float *out) { return fn<void *(THISCALL *)(void *, float *)>(0x4fb0a0)(part, out); }
// 0x4fceb0 (Rendering): a debug line from a position at an angle (view in ecx, the position in edx)
static DWORD debug_draw_direction(void *view, const float *pos, float angle, DWORD length, DWORD r, DWORD g, DWORD b)
{
    return fn<DWORD(__fastcall *)(void *, const float *, float, DWORD, DWORD, DWORD, DWORD)>(0x4fceb0)(view, pos, angle, length, r, g, b);
}
// The game's random generator 0x7124c0: a float between lo and hi (0x55c550)
static long double random_range(float lo, float hi) { return fn<long double(THISCALL *)(void *, float, float)>(0x55c550)(glob<void *>(0x7124c0), lo, hi); }
// Fidgets (CFidgetSettings, CFidgetData; AI, 0x49ff50-0x4a0860, behavior_rest.cpp; called through their exe addresses)
static const char *fidget_choose(void *data, void *settings, void *e, const char *idle)
{
    return fn<const char *(THISCALL *)(void *, void *, void *, const char *)>(0x4a0680)(data, settings, e, idle);
}
static long double fidget_delay(void *settings) { return fn<long double(THISCALL *)(void *)>(0x4a02c0)(settings); }
// 0x506620: whether a message has this name
static bool message_is(void *msg, const char *name) { return fn<bool(THISCALL *)(void *, const char *)>(0x506620)(msg, name); }
// 0x41a720: an action's slot 23 (CAction)
static void action_execute(void *action, void *layer, void *e, DWORD a, void *instigator, void *self, void *field)
{
    fn<void(THISCALL *)(void *, void *, void *, DWORD, void *, void *, void *)>(0x41a720)(action, layer, e, a, instigator, self, field);
}

// Float constants of the exe
static long double kf(GlobKey a) { return *glob<const float *>(a); }
constexpr DWORD K_PI = 0x5eecc8, K_NEG_PI = 0x5eecc4, K_TWO_PI = 0x5edffc, K_ZERO = 0x5ebed0, K_ONE = 0x5ec3f4,
                K_MINUS_ONE = 0x5eeccc, K_FRAME_EPSILON = 0x5f1e48, K_FIDGET_MAX = 0x5f3060, K_FIDGET_MIN = 0x5f05b8;
constexpr DWORD Y_SCALE = 0x3f3677d4;   // 0.7128, passed to the distance and the debug drawing (meaning not known)
constexpr DWORD STATIONARY = 0x200000;
constexpr DWORD TEXT_IDLE = 0x60f8d0, TEXT_IDLE_CROUCH = 0x612e70;
static float fbits(DWORD v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}
static DWORD dbits(float f)
{
    DWORD v;
    memcpy(&v, &f, 4);
    return v;
}
// fabs of a float through its bits (and eax, 0x7fffffff)
static float abs_bits(float f) { return fbits(dbits(f) & 0x7fffffff); }
// x87 comparisons by the status flags: C3 (equal or unordered)
static bool x87_eq(long double a, long double b) { return !(a < b) && !(a > b); }

// ---- The shared pattern ---------------------------------------------------------------------------------

template <typename T> static T &fld(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
static DWORD &dw(void *p, DWORD off) { return fld<DWORD>(p, off); }

static void vtable_and_defaults(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_init_fields(o);
}
static void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}

using Destroy = void (*)(BYTE *);

// Deleting destructor of the concrete classes (slot 1): bit 1 destroys the array from the last element
// and returns the block, bit 0 frees (array: operator delete, single object: the pool)
template <DWORD SIZE, Destroy D> static void *deleting_dtor(BYTE *self, DWORD flags)
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
// The abstract classes' slot 1: no array path
template <DWORD SIZE, Destroy D> static void *scalar_deleting_dtor(BYTE *self, DWORD flags)
{
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated, parent null = derived from CObject)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, Getter parent, DWORD size, DWORD abstract, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, abstract, key_fn_addr(create), key_fn_addr(create_array),
                                     key_fn_addr(register_fields));
    return v;
}

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD ENUM_INT_FIELD = 0x54d6e0;   // 0x44 bytes
constexpr DWORD FLOAT_FIELD_UNITS = 0x54ec40;   // 0x44
constexpr DWORD FLOAT_FIELD = 0x54ecd0;   // 0x44
constexpr DWORD BYTE_FIELD = 0x54ddf0;   // 0x30
constexpr DWORD OBJECT_REF_FIELD = 0x523e30;   // 0x30
constexpr DWORD STRING_FIELD = 0x415dd0;   // 0x34
constexpr DWORD BIT_FIELD = 0x54dfc0;   // 0x34
constexpr DWORD TYPED_RESOURCE_FIELD = 0x54e560;   // 0x38
constexpr DWORD OBJECT_FIELD = 0x4186f0;   // 0x3c
constexpr DWORD OBJECT_ARRAY_FIELD2 = 0x419230;   // 0x48

// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> static void *new_field(DWORD size, FnKey ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = zax::fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
// An object descriptor whose class getter runs only when operator new gave a block
static void *new_object_field(DWORD name, DWORD offset, Getter cls, DWORD u18)
{
    void *f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD)(
            f, name, offset, 4, offset, cls(), u18, 0, 0, 0);
    return f;
}


static void *byte_field(DWORD name, DWORD offset, DWORD u18) { return new_field(0x30, BYTE_FIELD, name, offset, 1, offset, u18, 0, 0); }
static void *float_field(DWORD name, DWORD offset, DWORD u18, DWORD def = 0, DWORD min = 0, DWORD max = 0)
{
    return new_field(0x44, FLOAT_FIELD, name, offset, 4, offset, u18, def, 0, min, max, 0);
}
static void *bit_field(DWORD name, DWORD mask, DWORD u18, DWORD def)
{
    return new_field(0x34, BIT_FIELD, name, 0xc, 4, 0xc, mask, u18, def, 0);
}

// ---- CEntityBehavior ------------------------------------------------------------------------------------

constexpr DWORD VT_BEHAVIOR = 0x5f2930;

// 0x494410 (slot 2)
static CClassInfo *__cdecl Behavior_ClassInfo()
{
    return class_getter(0x6bddcc, 0x612a9c /* "CEntityBehavior" */, 3.0f, nullptr, 0xc, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x4944e0: the destructor (CEntityBehavior's level, then CObject's)
static void THISCALL Behavior_Dtor(BYTE *self)
{
    vtable_and_free(self, vt(VT_BEHAVIOR));
    obj_dtor(self);
}
static void Behavior_destroy(BYTE *o) { Behavior_Dtor(o); }
// 0x4944b0 (slot 1)
static void *THISCALL Behavior_DeletingDtor(BYTE *self, DWORD flags) { return scalar_deleting_dtor<0xc, Behavior_destroy>(self, flags); }
// 0x494490 (slot 23, Begin): marks the behaviour begun. eax 1.
static DWORD THISCALL Behavior_Begin(BYTE *self, void *)
{
    dw(self, 8) |= 1;
    return 1;
}
// 0x4944a0 (slots 25 and 26): nothing to do
static bool THISCALL Behavior_Update(BYTE *, float, void *) { return false; }
// 0x494500 (slot 29, End): deletes the behaviour. eax: the destructor's.
static void *THISCALL Behavior_End(BYTE *self, void *)
{
    if (!self)
        return nullptr;
    return vcall<void *>(self, 1, 1);
}

// ---- CEntityBehaviorTurnToAngleBase, ...TurnMovingAngle, ...TurnAimingAngle, ...TurnToAngle ---------------
//
// TurnToAngleBase (abstract, 0x14 bytes): +0xc "Old Stationary Flag" (the entity's flag 0x200000 from
// before Begin), +0x10 the tolerance (0.1 rad from the constructors). TurnMovingAngle/TurnAimingAngle
// turn the moving/aiming angle (slots 30 and 31). TurnToAngle (0x1c): +0x14 "Facing Backwards" (not
// used here), +0x18 "Angle To Turn To". TurnToTarget (0x24, aiming): +0x14 "Point Weapon At Target",
// +0x18 "Turn to Target" (a part it listens to), +0x1c "Max Dist To Target", +0x20 "Target Must Be
// Visible" (not used here).

constexpr DWORD VT_TURN_BASE = 0x5f29a8, VT_TURN_MOVING = 0x5f2a28, VT_TURN_AIMING = 0x5f2aa8, VT_TURN_TO_ANGLE = 0x5f2b28,
                VT_TURN_TO_TARGET = 0x5f2ba8;

// 0x494560 (slot 2), 0x494510 (the same code)
static CClassInfo *__cdecl TurnBase_ClassInfo()
{
    return class_getter(0x6bde04, 0x612af4 /* "CEntityBehaviorTurnToAngleBase" */, 2.0f, Behavior_ClassInfo, 0x14, 1, 0x46b760, 0x52dc80,
                        0x4945c0);
}
// 0x4948f0 (slot 2), 0x4946e0
static CClassInfo *__cdecl TurnMoving_ClassInfo()
{
    return class_getter(0x6bde08, 0x612b28 /* "CEntityBehaviorTurnMovingAngle" */, 1.0f, TurnBase_ClassInfo, 0x14, 0, 0x494790, 0x4947e0,
                        0x58ea40);
}
// 0x494bf0 (slot 2), 0x4949e0
static CClassInfo *__cdecl TurnAiming_ClassInfo()
{
    return class_getter(0x6bde0c, 0x612b48 /* "CEntityBehaviorTurnAimingAngle" */, 1.0f, TurnBase_ClassInfo, 0x14, 0, 0x494a90, 0x494ae0,
                        0x58ea40);
}
// 0x494f90 (slot 2), 0x494ce0
static CClassInfo *__cdecl TurnToAngle_ClassInfo()
{
    return class_getter(0x6bde10, 0x612b68 /* "CEntityBehaviorTurnToAngle" */, 2.0f, TurnMoving_ClassInfo, 0x1c, 0, 0x494de0, 0x494e40,
                        0x4950a0);
}
// 0x495920 (slot 2), 0x495660
static CClassInfo *__cdecl TurnToTarget_ClassInfo()
{
    return class_getter(0x6bde14, 0x612bac /* "CEntityBehaviorTurnToTarget" */, 3.0f, TurnAiming_ClassInfo, 0x24, 0, 0x495760, 0x4957b0,
                        0x495a30);
}

static void TurnBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TURN_BASE));
    Behavior_Dtor(o);
}
static void TurnMoving_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TURN_MOVING));
    TurnBase_destroy(o);
}
static void TurnAiming_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TURN_AIMING));
    TurnBase_destroy(o);
}
// 0x495360: CEntityBehaviorTurnToAngle's destructor
static void THISCALL TurnToAngle_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(VT_TURN_TO_ANGLE));
    TurnMoving_destroy(o);
}
static void TurnToAngle_destroy(BYTE *o) { TurnToAngle_Dtor(o); }
static void TurnToTarget_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_TURN_TO_TARGET);
    if (void *target = fld<void *>(o, 0x18))
        part_remove_listener(target, o);
    dw(o, 0x18) = 0;
    obj_free_fields(o);
    TurnAiming_destroy(o);
}
// 0x494650, 0x494850, 0x494b50, 0x494ed0, 0x495830 (slot 1)
static void *THISCALL TurnBase_DeletingDtor(BYTE *self, DWORD flags) { return scalar_deleting_dtor<0x14, TurnBase_destroy>(self, flags); }
static void *THISCALL TurnMoving_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, TurnMoving_destroy>(self, flags); }
static void *THISCALL TurnAiming_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, TurnAiming_destroy>(self, flags); }
static void *THISCALL TurnToAngle_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, TurnToAngle_destroy>(self, flags); }
static void *THISCALL TurnToTarget_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, TurnToTarget_destroy>(self, flags); }

// 0x4945c0. eax: the last call's.
static DWORD __cdecl TurnBase_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD, glob_addr(0x612b14) /* "Old Stationary Flag" */, 0xc, 4, 0xc, 0, 0, 0, 0, 0);
    return add_field(TurnBase_ClassInfo(), f);
}
// 0x4950a0: "Angle To Turn To" in radians (shown in degrees: the converter 0x6be328), "Facing Backwards"
static DWORD __cdecl TurnToAngle_RegisterFields()
{
    BYTE *f = static_cast<BYTE *>(new_field(0x44, FLOAT_FIELD_UNITS, glob_addr(0x612b98) /* "Angle To Turn To" */, glob_addr(0x60ff9c) /* "Radians" */, 0x18, 4, 0x18,
                                            1, 0, 0, 0, 0x40c90fdb /* 2 pi */, 0x3f800000));
    add_field(TurnToAngle_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(f + 0x20, glob<const char *>(0x60ff80));   // "Degrees"
    f = static_cast<BYTE *>(byte_field(glob_addr(0x612b84) /* "Facing Backwards" */, 0x14, 0));
    return add_field(TurnToAngle_ClassInfo(), f);
}
// 0x495a30
static DWORD __cdecl TurnToTarget_RegisterFields()
{
    void *f = byte_field(glob_addr(0x612c04) /* "Point Weapon At Target" */, 0x14, 1);
    add_field(TurnToTarget_ClassInfo(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD)(
            f, glob_addr(0x612bf4) /* "Turn to Target" */, 0x18, 4, 0x18, fn<Getter>(0x4f9bf0)() /* CLayerPart */, 0, 0);
    add_field(TurnToTarget_ClassInfo(), f);
    f = float_field(glob_addr(0x612be0) /* "Max Dist To Target" */, 0x1c, 1);
    add_field(TurnToTarget_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD, glob_addr(0x612bc8) /* "Target Must Be Visible" */, 0x20, 4, 0x20, 1, 0, 0, 0, 0);
    return add_field(TurnToTarget_ClassInfo(), f);
}

// 0x494680 (slot 23, Begin): keeps the entity's "Stationary" flag and clears it. al 1.
static bool THISCALL TurnBase_Begin(BYTE *self, BYTE *e)
{
    dw(self, 0xc) = dw(e, 0x1c) & STATIONARY;
    ent_clear_flags(e, STATIONARY);
    return true;
}
// 0x4946b0 (slot 29, End): gives the entity its "Stationary" flag back, then CEntityBehavior's End
static void *THISCALL TurnBase_End(BYTE *self, BYTE *e)
{
    ent_set_flags(e, dw(self, 0xc));
    return Behavior_End(self, e);
}
// 0x4949b0, 0x494cb0 (slot 30): the moving/aiming angle
static float THISCALL TurnMoving_Angle(BYTE *, BYTE *e) { return fld<float>(e, 0xc8); }
static float THISCALL TurnAiming_Angle(BYTE *, BYTE *e) { return fld<float>(e, 0xc4); }
// 0x4949c0, 0x494cc0 (slot 31): sets it (entity slots 104 and 103). eax: the entity's.
static DWORD THISCALL TurnMoving_SetAngle(BYTE *, BYTE *e, DWORD angle) { return vcall<DWORD>(e, 104, angle, 1); }
static DWORD THISCALL TurnAiming_SetAngle(BYTE *, BYTE *e, DWORD angle) { return vcall<DWORD>(e, 103, angle, 1); }

static long double turn_angle(BYTE *self, BYTE *e) { return vcall<long double>(self, 30, e); }
static void turn_set_angle(BYTE *self, BYTE *e, float a) { vcall<void>(self, 31, e, dbits(a)); }

// A difference of two angles into [-pi, pi] (once each way), as the exe keeps it on the FPU
static long double wrap_pi(long double d)
{
    if (!x87_le(d, kf(K_PI)))
        d -= kf(K_TWO_PI);
    if (x87_lt(d, kf(K_NEG_PI)))
        d += kf(K_TWO_PI);
    return d;
}
// An angle into [0, 2 pi) (once each way)
static long double wrap_2pi(long double a)
{
    if (x87_lt(a, kf(K_ZERO)))
        a += kf(K_TWO_PI);
    if (!x87_lt(a, kf(K_TWO_PI)))
        a -= kf(K_TWO_PI);
    return a;
}

// One step of the turn towards the target angle (the second half of slot 25 of TurnToAngle and
// TurnToTarget): at most the model's turn speed times the seconds, then the remaining difference
// against the tolerance. True while still turning; when done, the entity's "Stationary" flag back.
static bool turn_step(BYTE *self, float seconds, BYTE *e, float target, float current)
{
    const long double t = wrap_2pi(target);
    long double d = t - current;
    float diff = to_float(d);
    if (!x87_le(d, kf(K_PI))) {
        d -= kf(K_TWO_PI);
        diff = to_float(d);
    }
    if (x87_lt(d, kf(K_NEG_PI))) {
        d += kf(K_TWO_PI);
        diff = to_float(d);
    }
    const long double max_ext = model_turn_speed(ent_model(e)) * (long double)seconds;
    const float max = to_float(max_ext);
    long double step;
    if (x87_lt(diff, -max_ext))
        step = -max_ext;
    else if (!x87_le(diff, max))
        step = max;
    else
        step = diff;
    turn_set_angle(self, e, to_float(wrap_2pi(step + current)));
    const float left = abs_bits(to_float(wrap_pi(turn_angle(self, e) - target)));
    if (x87_lt(fld<float>(self, 0x10), left))
        return true;
    ent_set_flags(e, dw(self, 0xc));
    return false;
}

// 0x495490 (slot 25, Update): turns an animated entity towards "Angle To Turn To"; nothing when it
// already has the angle
static bool THISCALL TurnToAngle_Update(BYTE *self, float seconds, BYTE *e)
{
    if (!is_kind_of(e, entity_animated_class()))
        return false;
    const float target = fld<float>(self, 0x18);
    if (x87_eq(turn_angle(self, e), target))
        return false;
    const float current = to_float(turn_angle(self, e));
    return turn_step(self, seconds, e, target, current);
}

// 0x495620 (slot 28): debug drawing of the target angle from the entity's position (0x4fceb0,
// Rendering). eax: the drawing's.
static DWORD THISCALL TurnToAngle_Draw(BYTE *self, BYTE *e, void *view)
{
    float pos[2];
    part_position(e, pos);
    return debug_draw_direction(view, pos, fld<float>(self, 0x18), Y_SCALE, 0xc8, 0xcd, 0x19);
}

// 0x4953a0: sets "Angle To Turn To"
static void THISCALL TurnToAngle_SetTarget(BYTE *self, DWORD angle) { dw(self, 0x18) = angle; }

static BYTE *new_turn_to_angle()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x1c));
    if (o) {
        behavior_ctor(o);
        fld<float>(o, 0x10) = 0.1f;
        vtable_and_defaults(o, vt(VT_TURN_BASE));
        vtable_and_defaults(o, vt(VT_TURN_MOVING));
        dw(o, 0) = vt(VT_TURN_TO_ANGLE);
        dw(o, 0x18) = 0;
        obj_init_fields(o);
    }
    return o;
}
static BYTE *new_turn_to_target()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x24));
    if (o) {
        behavior_ctor(o);
        fld<float>(o, 0x10) = 0.1f;
        vtable_and_defaults(o, vt(VT_TURN_BASE));
        vtable_and_defaults(o, vt(VT_TURN_AIMING));
        vtable_and_defaults(o, vt(VT_TURN_TO_TARGET));
    }
    return o;
}

// 0x4953b0 (entity in ecx, the angle; the second argument is not used): a new TurnToAngle with its
// fields reset when the entity's moving angle is further than the tolerance from the angle (its
// "Angle To Turn To" is not set here), otherwise none (the new one deleted again)
static BYTE *THISCALL TurnToAngle_Make(BYTE *e, float angle, DWORD)
{
    BYTE *o = new_turn_to_angle();
    vcall<void>(o, 19);
    const float d = abs_bits(to_float(wrap_pi((long double)angle - turn_angle(o, e))));
    if (x87_lt(fld<float>(o, 0x10), d)) {
        dw(o, 8) |= 2;
        return o;
    }
    if (o)
        vcall<void *>(o, 1, 1);
    return nullptr;
}

// The angle from the entity to the target in [0, 2 pi), as a float
static float target_angle(BYTE *e, void *target, bool point_weapon)
{
    const DWORD x = point_weapon ? ent_slot47(e) : 0;
    const long double a = part_angle(e, target, x, 1.0f);
    float v = to_float(a);
    if (x87_lt(a, kf(K_ZERO)))
        v = float_in_memory(a + kf(K_TWO_PI));
    if (!x87_lt(v, kf(K_TWO_PI)))
        v = float_in_memory(v - kf(K_TWO_PI));
    return v;
}

// 0x495ec0 (entity in ecx, target in edx; the maximum distance, "Point Weapon At Target", "Target Must
// Be Visible", two unused): a new TurnToTarget listening to the target when the target is nearer than
// the distance and the aiming angle further than the tolerance from it; otherwise none
static BYTE *__fastcall TurnToTarget_Make(BYTE *e, void *target, float max_dist, DWORD point_weapon, DWORD visible, DWORD, DWORD)
{
    const float d2 = to_float(part_distance2(e, target, fbits(Y_SCALE)));
    const float angle = target_angle(e, target, (BYTE)point_weapon != 0);
    if (x87_le((long double)max_dist * max_dist, d2))
        return nullptr;
    BYTE *o = new_turn_to_target();
    const float d = abs_bits(to_float(wrap_pi(turn_angle(o, e) - angle)));
    if (!x87_lt(fld<float>(o, 0x10), d)) {
        if (o)
            vcall<void *>(o, 1, 1);
        return nullptr;
    }
    vcall<void>(o, 19);
    const DWORD flags = dw(o, 8) | 2;
    fld<float>(o, 0x1c) = max_dist;
    dw(o, 8) = flags;
    fld<void *>(o, 0x18) = target;
    dw(o, 0x20) = visible;
    fld<BYTE>(o, 0x14) = (BYTE)point_weapon;
    part_add_listener(target, o);
    return o;
}

static void forget_target(BYTE *self)
{
    if (void *target = fld<void *>(self, 0x18))
        part_remove_listener(target, self);
    dw(self, 0x18) = 0;
}

// 0x496040 (slot 0, message): the target deleted, died or out of scope: forget it (and stop
// listening, except on "Delete"); everything else to CObject. eax: CObject's (0 for "Delete").
static DWORD THISCALL TurnToTarget_OnMessage(BYTE *self, void *sender, const char *text)
{
    if (sender != fld<void *>(self, 0x18))
        return obj_on_message(self, sender, text);
    if (!crt_stricmp(text, glob<const char *>(0x60aef8))) {   // "Delete"
        dw(self, 0x18) = 0;
        return 0;
    }
    const char *died = glob<const char *>(0x610178);   // "Died", compared case-sensitively
    DWORD i = 0;
    while (text[i] == died[i] && text[i])
        i++;
    if (text[i] == died[i] || !crt_stricmp(text, glob<const char *>(0x60aee8))) {   // "Leaving Scope"
        forget_target(self);
        return 0;
    }
    return obj_on_message(self, sender, text);
}

// 0x4960f0 (slot 25, Update): turns an animated entity's aiming angle towards the target while it is
// within "Max Dist To Target"; forgets the target when done or out of reach
static bool THISCALL TurnToTarget_Update(BYTE *self, float seconds, BYTE *e)
{
    void *target = fld<void *>(self, 0x18);
    if (!target || !is_kind_of(e, entity_animated_class()))
        return false;
    const float max_dist = fld<float>(self, 0x1c);
    const long double d2 = part_distance2(e, target, fbits(Y_SCALE));
    if (!x87_le((long double)max_dist * max_dist, d2)) {
        const float angle = target_angle(e, fld<void *>(self, 0x18), fld<BYTE>(self, 0x14) != 0);
        const float current = to_float(turn_angle(self, e));
        if (turn_step(self, seconds, e, angle, current))
            return true;
    }
    forget_target(self);
    return false;
}

// ---- CEntityBehaviorSetRotationSequence, ...PlaySequenceOnce ------------------------------------------------
//
// SetRotationSequence (0x10 bytes): +0xc "Sequence" (default "Idle"). PlaySequenceOnce (0x28): +0x10
// "Old Play Ratio", +0x14 "Speed To Play" (frames per second factor, -100..100, default 1; negative
// plays backwards), +0x18 the speed while playing (0 = done), +0x1c "Done Sequence", +0x20 "Done Frame"
// (-1 = the last), +0x24 "Was Stationary", +0x25 "Move With Sequence" (default 1).

constexpr DWORD VT_SET_ROTATION = 0x5f2c28, VT_PLAY_ONCE = 0x5f2ca0;

// 0x4964c0 (slot 2), 0x496320
static CClassInfo *__cdecl SetRotation_ClassInfo()
{
    return class_getter(0x6bde18, 0x612c1c /* "CEntityBehaviorSetRotationSequence" */, 3.0f, Behavior_ClassInfo, 0x10, 0, 0x496370,
                        0x4963b0, 0x496520);
}
// 0x496860 (slot 2), 0x496620
static CClassInfo *__cdecl PlayOnce_ClassInfo()
{
    return class_getter(0x6bde1c, 0x612c40 /* "CEntityBehaviorPlaySequenceOnce" */, 3.0f, SetRotation_ClassInfo, 0x28, 0, 0x4966d0,
                        0x496720, 0x496920);
}
static void SetRotation_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SET_ROTATION));
    zs_dtor(o + 0xc);
    Behavior_Dtor(o);
}
static void PlayOnce_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_PLAY_ONCE));
    zs_dtor(o + 0x1c);
    SetRotation_destroy(o);
}
// 0x496420, 0x4967a0 (slot 1)
static void *THISCALL SetRotation_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, SetRotation_destroy>(self, flags); }
static void *THISCALL PlayOnce_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, PlayOnce_destroy>(self, flags); }

// 0x496520
static DWORD __cdecl SetRotation_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD, glob_addr(0x60c3d4) /* "Sequence" */, 0xc, 4, 0xc, 1, glob_addr(TEXT_IDLE), 0);
    return add_field(SetRotation_ClassInfo(), f);
}
// 0x496920
static DWORD __cdecl PlayOnce_RegisterFields()
{
    void *f = float_field(glob_addr(0x612cb0) /* "Speed To Play" */, 0x14, 1, 0x3f800000, 0xc2c80000 /* -100 */, 0x42c80000 /* 100 */);
    add_field(PlayOnce_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD, glob_addr(0x612ca0) /* "Done Sequence" */, 0x1c, 4, 0x1c, 1, glob_addr(TEXT_IDLE), 0);
    add_field(PlayOnce_ClassInfo(), f);
    f = float_field(glob_addr(0x612c94) /* "Done Frame" */, 0x20, 0);
    add_field(PlayOnce_ClassInfo(), f);
    f = byte_field(glob_addr(0x612c84) /* "Was Stationary" */, 0x24, 0);
    add_field(PlayOnce_ClassInfo(), f);
    f = float_field(glob_addr(0x612c74) /* "Old Play Ratio" */, 0x10, 0);
    add_field(PlayOnce_ClassInfo(), f);
    f = byte_field(glob_addr(0x612c60) /* "Move With Sequence" */, 0x25, 1);
    return add_field(PlayOnce_ClassInfo(), f);
}

// The entity's behaviour stack (CEntityBehaviorStack at +0x90)
static BYTE *stack_of(BYTE *e) { return e + 0x90; }
static bool THISCALL Stack_Contains(BYTE *self, void *b);
static DWORD THISCALL Stack_RemoveTop(BYTE *self, BYTE *e, void *unused);

// 0x4965b0 (slot 23, Begin): clears "Stationary", sets "Sequence" (when the model has it) and takes
// itself off the stack. al 0 when the model has no such sequence.
static bool THISCALL SetRotation_Begin(BYTE *self, BYTE *e)
{
    ent_clear_flags(e, STATIONARY);
    if (zs_not_empty(self + 0xc)) {
        if (model_find_sequence(ent_model(e), zs_c_str(self + 0xc)) == -1)
            return false;
        ent_set_sequence(e, zs_c_str(self + 0xc));
    }
    Stack_RemoveTop(stack_of(e), e, self);
    return true;
}

// 0x496e20 (entity in ecx, the sequence in edx; the speed, the done sequence, the done frame, two
// unused): a new PlaySequenceOnce with its fields reset when the model has the sequence, otherwise none
static BYTE *__fastcall PlayOnce_Make(BYTE *e, const ZString *sequence, DWORD speed, const ZString *done_sequence, DWORD done_frame, DWORD,
                                      DWORD)
{
    if (!sequence || model_find_sequence(ent_model(e), zs_c_str(sequence)) == -1)
        return nullptr;
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x28));
    if (o) {
        behavior_ctor(o);
        zs_ctor(o + 0xc);
        vtable_and_defaults(o, vt(VT_SET_ROTATION));
        zs_ctor(o + 0x1c);
        vtable_and_defaults(o, vt(VT_PLAY_ONCE));
    }
    vcall<void>(o, 19);
    dw(o, 8) |= 2;
    zs_assign(o + 0xc, sequence);
    zs_assign(o + 0x1c, done_sequence);
    dw(o, 0x14) = speed;
    dw(o, 0x20) = done_frame;
    return o;
}

// 0x496ee0 (slot 23, Begin): keeps and clears "Stationary", starts the sequence at its first frame
// (forwards) or its last (backwards), keeps the play ratio and sets it to 0 (the behaviour moves the
// frames itself). al 0 when there is no sequence or the model does not have it.
static bool THISCALL PlayOnce_Begin(BYTE *self, BYTE *e)
{
    fld<BYTE>(self, 0x24) = (BYTE)((dw(e, 0x1c) >> 21) & 1);
    ent_clear_flags(e, STATIONARY);
    if (!zs_not_empty(self + 0xc) || model_find_sequence(ent_model(e), zs_c_str(self + 0xc)) == -1)
        return false;
    ent_set_sequence(e, zs_c_str(self + 0xc));
    ent_set_frame_index(e, 0);
    const float speed = fld<float>(self, 0x14);
    fld<float>(self, 0x18) = speed;
    if (x87_lt(speed, kf(K_ZERO))) {
        const DWORD n = player_frames(ent_player(e));
        const float frames = to_float((long double)n);
        ent_set_frame(e, to_float((long double)n - kf(K_FRAME_EPSILON)));
        player_play_backward(ent_player(e), to_float(frames + kf(K_ONE)), frames, e);
    } else {
        player_play_forward(ent_player(e), -1.0f, 0.0f, e);
    }
    const long double factor = ent_backwards(e) ? kf(K_MINUS_ONE) : kf(K_ONE);
    fld<float>(self, 0x10) = to_float(factor * fld<float>(e, 0xb8));
    ent_set_play_ratio_bits(e, 0);
    return true;
}

// 0x497010 (slot 25, Update): moves the frame by the player's frames per second times the speed and
// the seconds (and the entity with the sequence's motion when "Move With Sequence"); at the end the
// "Done Sequence" at the "Done Frame", "Stationary" back if it was. Counts its calls (0x6bddfc).
static bool THISCALL PlayOnce_Update(BYTE *self, float seconds, BYTE *e)
{
    (*glob<DWORD *>(0x6bddfc))++;
    if (x87_eq(fld<float>(self, 0x18), kf(K_ZERO)))
        return false;
    if (!ent_player(e))
        return false;
    const float cur = to_float(ent_cur_frame(e));
    float frame = to_float(player_fps(ent_player(e)) * fld<float>(self, 0x18) * seconds + cur);
    const DWORD frames_int = player_frames(ent_player(e));
    const float frames = to_float((long double)frames_int);
    bool done = false;
    if (!x87_le(fld<float>(self, 0x18), kf(K_ZERO))) {
        if (!x87_lt(frame, frames)) {
            frame = to_float(frames - kf(K_FRAME_EPSILON));
            done = true;
        }
        player_play_forward(ent_player(e), cur, frame, e);
    } else {
        if (x87_le(frame, kf(K_ZERO))) {
            frame = 0.0f;
            done = true;
        }
        player_play_backward(ent_player(e), cur, frame, e);
    }
    if (fld<BYTE>(self, 0x25) && is_kind_of(e, entity_movable_class())) {
        const DWORD forward = x87_le(fld<float>(self, 0x18), kf(K_ZERO)) ? 0 : 1;
        // the motion (x, y) comes back in the 8 bytes the exe used for the frame count as a qword
        float out[2];
        memcpy(&out[0], &frames_int, 4);
        out[1] = 0.0f;
        BYTE *p = ent_player(e);
        void *motion = ent_motion(e, p);
        player_motion(ent_player(e), out, motion, cur, frame, forward);
        const float y = to_float((long double)out[1] + fld<float>(e, 0x104));
        ent_move(e, to_float((long double)out[0] + fld<float>(e, 0x100)), y);
    }
    if (!done) {
        ent_set_frame(e, frame);
        return true;
    }
    dw(self, 0x18) = 0;
    if (zs_not_empty(self + 0x1c)) {
        ent_set_sequence(e, zs_c_str(self + 0x1c));
        if (dw(self, 0x20) == 0xbf800000) {
            frame = to_float((long double)player_frames(ent_player(e)) - kf(K_FRAME_EPSILON));
        } else {
            frame = fld<float>(self, 0x20);
            if (x87_le((long double)player_frames(ent_player(e)), frame))
                frame = to_float((long double)player_frames(ent_player(e)));
        }
    }
    ent_set_frame(e, frame);
    if (fld<BYTE>(self, 0x24))
        ent_set_flags(e, STATIONARY);
    return false;
}

// 0x497270 (slot 29, End): the play ratio back, then CEntityBehavior's End
static void *THISCALL PlayOnce_End(BYTE *self, BYTE *e)
{
    ent_set_play_ratio_bits(e, dw(self, 0x10));
    return Behavior_End(self, e);
}

// ---- CEntityBehaviorSetPlayRatio, ...MarkForDeletion, ...ClearFlags, ...SetCurFrame, ...ExecuteAction ----------
//
// Each does its work in Begin and takes itself off the stack (SetCurFrame by returning al 0).
// SetPlayRatio (0x10): +0xc "New Play Ratio". ClearFlags (0x10): +0xc the entity flags to clear
// ("Visible" 0x20000, "Collideable" 0x40000, "Tries To Collide" 0x80000, "Stationary" 0x200000,
// "Active" 0x800000). SetCurFrame (0x10): +0xc the frame. ExecuteAction (0x10): +0xc "Action" (owned).

constexpr DWORD VT_SET_PLAY_RATIO = 0x5f2d18, VT_MARK_FOR_DELETION = 0x5f2d90, VT_CLEAR_FLAGS = 0x5f2e08, VT_SET_CUR_FRAME = 0x5f3064,
                VT_EXECUTE_ACTION = 0x5f30dc;

// 0x4973f0, 0x4976c0, 0x497910, 0x499470, 0x4996a0 (slot 2) and 0x497290, 0x497560, 0x4977a0, 0x499310, 0x499540
static CClassInfo *__cdecl SetPlayRatio_ClassInfo()
{
    return class_getter(0x6bde20, 0x612cc0 /* "CEntityBehaviorSetPlayRatio" */, 1.0f, Behavior_ClassInfo, 0x10, 0, 0x4972e0, 0x497310,
                        0x497450);
}
static CClassInfo *__cdecl MarkForDeletion_ClassInfo()
{
    return class_getter(0x6bde24, 0x612cec /* "CEntityBehaviorMarkForDeletion" */, 1.0f, Behavior_ClassInfo, 0xc, 0, 0x4975b0, 0x4975e0,
                        0x58ea40);
}
static CClassInfo *__cdecl ClearFlags_ClassInfo()
{
    return class_getter(0x6bde28, 0x612d0c /* "CEntityBehaviorClearFlags" */, 1.0f, Behavior_ClassInfo, 0x10, 0, 0x4977f0, 0x497830,
                        0x497970);
}
static CClassInfo *__cdecl SetCurFrame_ClassInfo()
{
    return class_getter(0x6bde3c, 0x612e7c /* "CEntityBehaviorSetCurFrame" */, 1.0f, Behavior_ClassInfo, 0x10, 0, 0x499360, 0x499390,
                        0x58ea40);
}
static CClassInfo *__cdecl ExecuteAction_ClassInfo()
{
    return class_getter(0x6bde40, 0x612e98 /* "CEntityBehaviorExecuteAction" */, 2.0f, Behavior_ClassInfo, 0x10, 0, 0x499590, 0x4995c0,
                        0x499700);
}

template <DWORD VT> static void simple_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    Behavior_Dtor(o);
}
// 0x497370, 0x497640, 0x497890, 0x4993f0, 0x499620 (slot 1)
static void *THISCALL SetPlayRatio_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, simple_destroy<VT_SET_PLAY_RATIO>>(self, flags); }
static void *THISCALL MarkForDeletion_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0xc, simple_destroy<VT_MARK_FOR_DELETION>>(self, flags);
}
static void *THISCALL ClearFlags_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, simple_destroy<VT_CLEAR_FLAGS>>(self, flags); }
static void *THISCALL SetCurFrame_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, simple_destroy<VT_SET_CUR_FRAME>>(self, flags); }
static void *THISCALL ExecuteAction_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x10, simple_destroy<VT_EXECUTE_ACTION>>(self, flags);
}

// 0x497450
static DWORD __cdecl SetPlayRatio_RegisterFields()
{
    void *f = float_field(glob_addr(0x612cdc) /* "New Play Ratio" */, 0xc, 0);
    return add_field(SetPlayRatio_ClassInfo(), f);
}
// 0x497970
static DWORD __cdecl ClearFlags_RegisterFields()
{
    void *f = bit_field(glob_addr(0x612d5c) /* "Visible" */, 0x20000, 1, 0);
    add_field(ClearFlags_ClassInfo(), f);
    f = bit_field(glob_addr(0x612d50) /* "Collideable" */, 0x40000, 1, 0);
    add_field(ClearFlags_ClassInfo(), f);
    f = bit_field(glob_addr(0x612d3c) /* "Tries To Collide" */, 0x80000, 1, 0);
    add_field(ClearFlags_ClassInfo(), f);
    f = bit_field(glob_addr(0x612d30) /* "Stationary" */, 0x200000, 1, 0);
    add_field(ClearFlags_ClassInfo(), f);
    f = bit_field(glob_addr(0x612d28) /* "Active" */, 0x800000, 1, 0);
    return add_field(ClearFlags_ClassInfo(), f);
}
// 0x499700
static DWORD __cdecl ExecuteAction_RegisterFields()
{
    void *f = new_object_field(glob_addr(0x60aed0) /* "Action" */, 0xc, code<Getter>(0x41a650) /* CAction */, 1);
    return add_field(ExecuteAction_ClassInfo(), f);
}

// 0x4974e0 (the ratio): a new SetPlayRatio with its fields reset
static BYTE *__stdcall SetPlayRatio_Make(DWORD ratio)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x10));
    if (o) {
        behavior_ctor(o);
        vtable_and_defaults(o, vt(VT_SET_PLAY_RATIO));
    }
    vcall<void>(o, 19);
    dw(o, 0xc) = ratio;
    return o;
}
// 0x497530 (slot 23, Begin)
static bool THISCALL SetPlayRatio_Begin(BYTE *self, BYTE *e)
{
    ent_set_play_ratio_bits(e, dw(self, 0xc));
    Stack_RemoveTop(stack_of(e), e, self);
    return true;
}

// 0x497720: a new MarkForDeletion with its fields reset
static BYTE *__cdecl MarkForDeletion_Make()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0xc));
    if (o) {
        behavior_ctor(o);
        vtable_and_defaults(o, vt(VT_MARK_FOR_DELETION));
    }
    vcall<void>(o, 19);
    return o;
}
// 0x497760 (slot 23, Begin): the entity dies (LayerPart_Died 0x4fbf30)
static bool THISCALL MarkForDeletion_Begin(BYTE *self, BYTE *e)
{
    ent_died(e);
    if (Stack_Contains(stack_of(e), self))
        Stack_RemoveTop(stack_of(e), e, self);
    return true;
}
// 0x497c10 (slot 23, Begin)
static bool THISCALL ClearFlags_Begin(BYTE *self, BYTE *e)
{
    ent_clear_flags(e, dw(self, 0xc));
    if (Stack_Contains(stack_of(e), self))
        Stack_RemoveTop(stack_of(e), e, self);
    return true;
}

// 0x4994d0 (the frame in ecx; the stack argument is not used): a new SetCurFrame with its fields reset
static BYTE *__fastcall SetCurFrame_Make(DWORD frame, DWORD, DWORD)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x10));
    if (o) {
        behavior_ctor(o);
        vtable_and_defaults(o, vt(VT_SET_CUR_FRAME));
    }
    vcall<void>(o, 19);
    const DWORD flags = dw(o, 8);
    dw(o, 0xc) = frame;
    dw(o, 8) = flags | 2;
    return o;
}
// 0x499520 (slot 23, Begin): sets the frame (0x490010). al 0: ends at once.
static bool THISCALL SetCurFrame_Begin(BYTE *self, BYTE *e)
{
    ent_set_frame_index(e, dw(self, 0xc));
    return false;
}

// 0x499790 (slot 23, Begin): executes "Action" (layer: the entity's +0x48; instigator: the entity).
// al 1: stays on the stack.
static bool THISCALL ExecuteAction_Begin(BYTE *self, BYTE *e)
{
    action_execute(fld<void *>(self, 0xc), fld<void *>(e, 0x48), e, 0, e, self, self + 0xc);
    return true;
}

// ---- CEntityBehaviorWait, ...Transition, ...Fade --------------------------------------------------------------
//
// Wait (0x14): +0xc "Num Seconds" (default 1), +0x10 "Seconds Have Transitioned". Transition
// (abstract, 0x18): +0x14 "Transition In Or Out" (the enum 0x6bdde0; 1 = out). Fade (0x18) sets the
// entity's fade +0xb4 from the elapsed share.

constexpr DWORD VT_WAIT = 0x5f2e80, VT_TRANSITION = 0x5f2ef8, VT_FADE = 0x5f2f70;

// 0x497db0, 0x498090, 0x4984f0 (slot 2) and 0x497c50, 0x497fe0, 0x498270
static CClassInfo *__cdecl Wait_ClassInfo()
{
    return class_getter(0x6bde2c, 0x612d64 /* "CEntityBehaviorWait" */, 1.0f, Behavior_ClassInfo, 0x14, 0, 0x497ca0, 0x497cd0, 0x497e10);
}
static CClassInfo *__cdecl Transition_ClassInfo()
{
    return class_getter(0x6bde30, 0x612da0 /* "CEntityBehaviorTransition" */, 1.0f, Wait_ClassInfo, 0x18, 1, 0x46b760, 0x52dc80, 0x498150);
}
static CClassInfo *__cdecl Fade_ClassInfo()
{
    return class_getter(0x6bde34, 0x612dd4 /* "CEntityBehaviorFade" */, 2.0f, Transition_ClassInfo, 0x18, 0, 0x498370, 0x4983c0, 0x58ea40);
}
static void Wait_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_WAIT));
    Behavior_Dtor(o);
}
static void Transition_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TRANSITION));
    Wait_destroy(o);
}
static void Fade_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_FADE));
    Transition_destroy(o);
}
// 0x497d30, 0x498230, 0x498430 (slot 1)
static void *THISCALL Wait_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, Wait_destroy>(self, flags); }
static void *THISCALL Transition_DeletingDtor(BYTE *self, DWORD flags) { return scalar_deleting_dtor<0x18, Transition_destroy>(self, flags); }
static void *THISCALL Fade_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, Fade_destroy>(self, flags); }

// 0x497e10
static DWORD __cdecl Wait_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_UNITS, glob_addr(0x612d94) /* "Num Seconds" */, glob_addr(0x60b000) /* "Seconds" */, 0xc, 4, 0xc, 1, 0x3f800000, 0, 0, 0, 0);
    add_field(Wait_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_UNITS, glob_addr(0x612d78) /* "Seconds Have Transitioned" */, glob_addr(0x60b000), 0x10, 4, 0x10, 0, 0, 0, 0, 0, 0);
    return add_field(Wait_ClassInfo(), f);
}
// 0x498150
static DWORD __cdecl Transition_RegisterFields()
{
    void *f = new_field(0x38, TYPED_RESOURCE_FIELD, glob_addr(0x612dbc) /* "Transition In Or Out" */, 0x14, 4, 0x14, glob_addr(0x6bdde0), 1, 0, 0, 0);
    return add_field(Transition_ClassInfo(), f);
}

// 0x497f30 (the seconds): a new Wait
static BYTE *__stdcall Wait_Make(DWORD seconds)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x14));
    if (o) {
        behavior_ctor(o);
        vtable_and_defaults(o, vt(VT_WAIT));
    }
    const DWORD flags = dw(o, 8);
    dw(o, 0x10) = 0;
    dw(o, 8) = flags | 2;
    dw(o, 0xc) = seconds;
    return o;
}
// 0x497f80 (slot 25, Update): counts the seconds up to "Num Seconds"
static bool THISCALL Wait_Update(BYTE *self, float seconds, void *)
{
    if (!x87_lt(fld<float>(self, 0x10), fld<float>(self, 0xc)))
        return false;
    fld<float>(self, 0x10) = to_float((long double)seconds + fld<float>(self, 0x10));
    return true;
}
// 0x498600 (slot 25, Update): like Wait, and the entity's fade the elapsed share (at most 1; 1 minus it
// when transitioning out). eax 1 while running.
static DWORD THISCALL Fade_Update(BYTE *self, float seconds, BYTE *e)
{
    if (!x87_lt(fld<float>(self, 0x10), fld<float>(self, 0xc)))
        return 0;
    const long double sum = (long double)seconds + fld<float>(self, 0x10);
    fld<float>(self, 0x10) = to_float(sum);
    long double share = sum / fld<float>(self, 0xc);
    if (!x87_le(share, kf(K_ONE)))
        share = kf(K_ONE);
    if (dw(self, 0x14) == 1)
        share = kf(K_ONE) - share;
    fld<float>(e, 0xb4) = to_float(share);
    return 1;
}

// ---- CEntityBehaviorIdle ------------------------------------------------------------------------------------
//
// Idle (0x30, a Wait): +0x14 the walking entity's "Movement Speed" when it started, +0x18 the fidget sequence (empty =
// the idle sequence), +0x1c "Debug/Fidgeting", +0x20 "Debug/Time Until Next Fidget", +0x24 "Override
// Fidget Options" (CFidgetSettings; else the model's +0xbc), +0x28 "Current Fidget Data" (CFidgetData),
// +0x2c "Debug/FPS Play Ratio" (0.9-1.1, random per start).

constexpr DWORD VT_IDLE = 0x5f2fe8;

// 0x4988a0 (slot 2), 0x498660
static CClassInfo *__cdecl Idle_ClassInfo()
{
    return class_getter(0x6bde38, 0x612de8 /* "CEntityBehaviorIdle" */, 1.0f, Wait_ClassInfo, 0x30, 0, 0x498720, 0x498770, 0x498960);
}
// 0x498dc0: the destructor
static void THISCALL Idle_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(VT_IDLE));
    zs_dtor(o + 0x18);
    Wait_destroy(o);
}
static void Idle_destroy(BYTE *o) { Idle_Dtor(o); }
// 0x4987e0 (slot 1)
static void *THISCALL Idle_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, Idle_destroy>(self, flags); }

// 0x498960
static DWORD __cdecl Idle_RegisterFields()
{
    void *f = new_object_field(glob_addr(0x612e58) /* "Override Fidget Options" */, 0x24, code<Getter>(0x49ff50) /* CFidgetSettings */, 1);
    add_field(Idle_ClassInfo(), f);
    f = new_object_field(glob_addr(0x612e44) /* "Current Fidget Data" */, 0x28, code<Getter>(0x4a0320) /* CFidgetData */, 3);
    add_field(Idle_ClassInfo(), f);
    f = float_field(glob_addr(0x612e24) /* "Debug/Time Until Next Fidget" */, 0x20, 3);
    add_field(Idle_ClassInfo(), f);
    f = float_field(glob_addr(0x612e0c) /* "Debug/FPS Play Ratio" */, 0x2c, 3, 0x3f800000, 0x3f666666 /* 0.9 */, 0x3f8ccccd /* 1.1 */);
    add_field(Idle_ClassInfo(), f);
    f = byte_field(glob_addr(0x612dfc) /* "Debug/Fidgeting" */, 0x1c, 3);
    return add_field(Idle_ClassInfo(), f);
}

// 0x498df0 (slot 19): the fields reset and the fidget sequence emptied
static void THISCALL Idle_ResetFields(BYTE *self)
{
    obj_reset_fields(self);
    zs_clear(self + 0x18);
}
// 0x498e10 (slot 18): the fidget sequence emptied
static void THISCALL Idle_Slot18(BYTE *self, DWORD) { zs_clear(self + 0x18); }

static void *fidget_settings(BYTE *self, BYTE *e)
{
    void *s = fld<void *>(self, 0x24);
    return s ? s : fld<void *>(ent_model(e), 0xbc);
}

// 0x498e20 (entity in ecx, the idle behaviour or null in edx): the sequence to idle with: "IdleCrouch"
// for a walking entity crouching ("Movement Speed" 2) whose model has it, otherwise "Idle"; while fidgeting (the model has
// fidget settings) the fidget sequence, chosen by the fidget data once (0x4a0680)
static const char *__fastcall Idle_SequenceName(BYTE *e, BYTE *idle)
{
    BYTE *walking = e && is_kind_of(e, entity_walking_class()) ? e : nullptr;
    const char *name = glob<const char *>(TEXT_IDLE);
    if (walking && dw(walking, 0x12c) == 2)
        name = model_find_sequence(ent_model(e), glob<const char *>(TEXT_IDLE_CROUCH)) != -1 ? glob<const char *>(TEXT_IDLE_CROUCH)
                                                                                            : glob<const char *>(TEXT_IDLE);
    if (!idle || !fld<void *>(ent_model(e), 0xbc) || !fld<BYTE>(idle, 0x1c))
        return name;
    if (!fld<void *>(idle, 0x28)) {
        void *d = pool_alloc(0x8c);
        fld<void *>(idle, 0x28) = d ? fidget_data_ctor(d) : nullptr;
    }
    void *settings = fidget_settings(idle, e);
    if (zs_is_empty(idle + 0x18))
        zs_assign_text(idle + 0x18, fidget_choose(fld<void *>(idle, 0x28), settings, e, name));
    return zs_c_str(idle + 0x18);
}

// 0x498f10 (entity in ecx; the seconds, one unused): a new Idle with its fields reset when the model
// has the idle sequence, otherwise none
static BYTE *THISCALL Idle_Make(BYTE *e, DWORD seconds, DWORD)
{
    const char *name = Idle_SequenceName(e, nullptr);
    if (model_find_sequence(ent_model(e), name) == -1)
        return nullptr;
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x30));
    if (o) {
        behavior_ctor(o);
        vtable_and_defaults(o, vt(VT_WAIT));
        zs_ctor(o + 0x18);
        vtable_and_defaults(o, vt(VT_IDLE));
    }
    vcall<void>(o, 19);
    const DWORD flags = dw(o, 8);
    dw(o, 0xc) = seconds;
    dw(o, 8) = flags | 2;
    dw(o, 0x10) = 0;
    return o;
}

// 0x498fa0 (entity, restart): starts the idle (or fidget) sequence: clears "Stationary", play ratio
// 1, remembers the "Movement Speed"; on a restart also stops fidgeting, draws the time until the next fidget
// (fidget delay times 0.25-1.75), a random start frame and the frames-per-second ratio (0.9-1.1). al 0
// when the model has no such sequence.
static bool THISCALL Idle_Start(BYTE *self, BYTE *e, DWORD restart)
{
    const char *name = Idle_SequenceName(e, self);
    if ((BYTE)restart) {
        fld<BYTE>(self, 0x1c) = 0;
        zs_clear(self + 0x18);
        if (fld<void *>(ent_model(e), 0xbc)) {
            void *s = fld<void *>(self, 0x24);
            if (!s)
                s = fld<void *>(ent_model(e), 0xbc);
            const long double delay = fld<float>(s, 8);
            const float hi = to_float(delay * kf(K_FIDGET_MAX));
            fld<float>(self, 0x20) = to_float(random_range(to_float(delay * kf(K_FIDGET_MIN)), hi));
        } else {
            dw(self, 0x20) = 0;
        }
    }
    if (model_find_sequence(ent_model(e), name) == -1)
        return false;
    ent_clear_flags(e, STATIONARY);
    ent_set_sequence(e, name);
    ent_set_play_ratio_bits(e, 0x3f800000);
    if (e && is_kind_of(e, entity_walking_class()))
        dw(self, 0x14) = dw(e, 0x12c);
    else
        dw(self, 0x14) = 0;
    if ((BYTE)restart) {
        const float frames = to_float((long double)player_frames(ent_player(e)));
        ent_set_frame(e, to_float(random_range(0.0f, frames)));
        fld<float>(self, 0x2c) = to_float(random_range(fbits(0x3f666666), fbits(0x3f8ccccd)));
    }
    return true;
}

// 0x498710 (slot 23, Begin)
static bool THISCALL Idle_Begin(BYTE *self, BYTE *e) { return Idle_Start(self, e, 1); }

// 0x4990e0 (slot 25, Update): restarts the sequence for a changed "Movement Speed" or an available idle
// sequence, begins a fidget when the time has run out, moves the frame (the player's frames per second
// times the play ratio and the fidget ratio), ends a fidget at the last frame; counts the seconds like
// Wait
static bool THISCALL Idle_Update(BYTE *self, float seconds, BYTE *e)
{
    BYTE *walking = e && is_kind_of(e, entity_walking_class()) ? e : nullptr;
    fld<float>(self, 0x20) = to_float((long double)fld<float>(self, 0x20) - seconds);
    if (walking) {
        if (dw(walking, 0x12c) != dw(self, 0x14))
            Idle_Start(self, e, 0);
        const char *name = Idle_SequenceName(e, self);
        if (model_find_sequence(ent_model(e), name) != -1)
            Idle_Start(self, e, 0);
    }
    if (fld<void *>(ent_model(e), 0xbc) && !fld<BYTE>(self, 0x1c) && x87_le(fld<float>(self, 0x20), kf(K_ZERO))) {
        fld<BYTE>(self, 0x1c) = 1;
        zs_clear(self + 0x18);
        Idle_Start(self, e, 0);
        ent_set_frame(e, 0.0f);
    }
    BYTE *player = ent_player(e);
    if (!player)
        return false;
    const long double factor = ent_backwards(e) ? kf(K_MINUS_ONE) : kf(K_ONE);
    const float ratio = abs_bits(to_float(factor * fld<float>(e, 0xb8)));
    const float speed = to_float(player_fps(player) * fld<float>(self, 0x2c) * ratio);
    const float cur = to_float(ent_cur_frame(e));
    float frame = to_float((long double)speed * seconds + cur);
    if (fld<BYTE>(self, 0x1c) && x87_le((long double)player_frames(ent_player(e)), frame)) {
        frame = to_float((long double)frame - (long double)player_frames(ent_player(e)));
        const float frames = to_float((long double)player_frames(ent_player(e)));
        player_play_forward(ent_player(e), cur, frames, e);
        fld<BYTE>(self, 0x1c) = 0;
        zs_clear(self + 0x18);
        fld<float>(self, 0x20) = to_float(fidget_delay(fidget_settings(self, e)));
        Idle_Start(self, e, 0);
        player_play_forward(ent_player(e), 0.0f, frame, e);
        ent_set_frame(e, frame);
    } else {
        ent_set_frame(e, frame);
        player_play_forward(ent_player(e), cur, frame, e);
    }
    return Wait_Update(self, seconds, e);
}

// ---- CEntityBehaviorPlaySequenceLooped -------------------------------------------------------------------------
//
// 0x14 bytes: +0xc flags ("Debug/Playing" 1, "Playing Backwards" 2, "Ping Pong" 4, "Debug/Stoping" 8,
// "Synchronize" 0x10), +0x10 "Sequence".

constexpr DWORD VT_PLAY_LOOPED = 0x5f3154;

// 0x499950 (slot 2), 0x4997b0
static CClassInfo *__cdecl PlayLooped_ClassInfo()
{
    return class_getter(0x6bde44, 0x612eb8 /* "CEntityBehaviorPlaySequenceLooped" */, 2.0f, Behavior_ClassInfo, 0x14, 0, 0x499800,
                        0x499840, 0x4999b0);
}
// 0x499cf0: the destructor
static void THISCALL PlayLooped_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(VT_PLAY_LOOPED));
    zs_dtor(o + 0x10);
    Behavior_Dtor(o);
}
static void PlayLooped_destroy(BYTE *o) { PlayLooped_Dtor(o); }
// 0x4998b0 (slot 1)
static void *THISCALL PlayLooped_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, PlayLooped_destroy>(self, flags); }

// 0x4999b0
static DWORD __cdecl PlayLooped_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD, glob_addr(0x60c3d4) /* "Sequence" */, 0x10, 4, 0x10, 1, 0, 0);
    add_field(PlayLooped_ClassInfo(), f);
    f = bit_field(glob_addr(0x612f14) /* "Playing Backwards" */, 2, 1, 0);
    add_field(PlayLooped_ClassInfo(), f);
    f = bit_field(glob_addr(0x612f08) /* "Ping Pong" */, 4, 1, 0);
    add_field(PlayLooped_ClassInfo(), f);
    f = bit_field(glob_addr(0x612efc) /* "Synchronize" */, 0x10, 1, 0);
    add_field(PlayLooped_ClassInfo(), f);
    f = bit_field(glob_addr(0x612eec) /* "Debug/Playing" */, 1, 3, 1);
    add_field(PlayLooped_ClassInfo(), f);
    f = bit_field(glob_addr(0x612edc) /* "Debug/Stoping" */, 8, 3, 0);
    return add_field(PlayLooped_ClassInfo(), f);
}

// 0x499d10 (slot 23, Begin): sets the sequence at frame 0 and plays. al 1.
static bool THISCALL PlayLooped_Begin(BYTE *self, BYTE *e)
{
    if (zs_not_empty(self + 0x10)) {
        ent_set_sequence(e, zs_c_str(self + 0x10));
        ent_set_frame(e, 0.0f);
        dw(self, 0xc) = (dw(self, 0xc) & ~8u) | 1;
    }
    return true;
}

// 0x499d50 (slot 25, Update): while playing, moves the frame forwards or backwards by the player's
// frames per second (or to the synchronised frame), wraps around or turns at the ends (ping pong);
// "Debug/Stoping" stops at the next end (al 0). al 1 otherwise.
static bool THISCALL PlayLooped_Update(BYTE *self, float seconds, BYTE *e)
{
    if (!(dw(self, 0xc) & 1) || x87_le(seconds, kf(K_ZERO)))
        return true;
    BYTE *player = ent_player(e);
    if (!player)
        return true;
    ent_clear_flags(e, STATIONARY);
    const float cur = to_float(ent_cur_frame(e));
    const DWORD frames_int = player_frames(player);
    const float frames = to_float((long double)frames_int);
    float frame;
    DWORD flags = dw(self, 0xc);
    if (flags & 0x10) {
        frame = to_float((long double)sync_frame(frames_int, to_float(player_fps(player))));
        player_play_forward(player, cur, frame, e);
    } else if (flags & 2) {
        frame = to_float(cur - player_fps(player) * seconds);
        player_play_backward(player, cur, frame, e);
        if (x87_lt(frame, kf(K_ZERO))) {
            flags = dw(self, 0xc);
            if (flags & 8) {
                dw(self, 0xc) = flags & ~9u;
                return false;
            }
            if (flags & 4) {
                frame = float_in_memory(frame * kf(K_MINUS_ONE));
                dw(self, 0xc) = flags & ~2u;
            } else {
                frame = float_in_memory((long double)frames + frame);
            }
        } else {
            if (!x87_lt(frame, frames))
                frame = 0.0f;
            ent_set_frame(e, frame);
            return true;
        }
    } else {
        frame = to_float(player_fps(player) * seconds + cur);
        player_play_forward(player, cur, frame, e);
        if (!x87_lt(frame, frames)) {
            flags = dw(self, 0xc);
            if (flags & 8) {
                dw(self, 0xc) = flags & ~9u;
                return false;
            }
            const long double over = (long double)frame - frames;
            if (flags & 4) {
                dw(self, 0xc) = flags | 2;
                frame = float_in_memory(frames - over);
            } else {
                frame = float_in_memory(over);
            }
        }
    }
    if (x87_lt(frame, kf(K_ZERO)) || !x87_lt(frame, frames))
        frame = 0.0f;
    ent_set_frame(e, frame);
    return true;
}

// 0x499f30 (a message with the entity; the first argument is not used): "Start Animation" plays,
// "Stop Animation" stops at the next end. al 1 when it was one of them.
static bool THISCALL PlayLooped_OnMessage(BYTE *self, DWORD, void *msg)
{
    if (message_is(msg, glob<const char *>(0x612f38))) {   // "Start Animation"
        dw(self, 0xc) = (dw(self, 0xc) & ~8u) | 1;
        return true;
    }
    if (message_is(msg, glob<const char *>(0x612f28))) {   // "Stop Animation"
        dw(self, 0xc) |= 8;
        return true;
    }
    return false;
}

// ---- CEntityBehaviorStack -----------------------------------------------------------------------------------------
//
// 0x20 bytes, derived from CObject: +8 "Debug/InProcess" (the top behaviour has begun), +0xc
// "Misc./Blocking Priority" (the enum 0x6bde48: the priority of what was pushed last; 3 and above
// accept nothing new), +0x10 "Debug/BehaviorList" (pointer array: vtable 0x5f3228, +0x14 data, +0x18
// count, +0x1c capacity; the top is the last). The behaviours on it are owned: ending one (slot 29)
// deletes it.

constexpr DWORD VT_STACK = 0x5f31cc, VT_STACK_ARRAY = 0x5f3228, VT_STACK_ARRAY_BASE = 0x5f3234;

static BYTE **stack_data(BYTE *s) { return fld<BYTE **>(s, 0x14); }
static DWORD &stack_count(BYTE *s) { return dw(s, 0x18); }

// 0x499fb0 (slot 2)
static CClassInfo *__cdecl Stack_ClassInfo()
{
    return class_getter(0x6bde64, 0x612fb4 /* "CEntityBehaviorStack" */, 1.0f, nullptr, 0x20, 0, 0x49a000, 0x49a050, 0x49a1e0);
}
// 0x49a3a0: the constructor
static BYTE *THISCALL Stack_Ctor(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0x10) = vt(VT_STACK_ARRAY);
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    dw(o, 0x1c) = 0;
    dw(o, 0) = vt(VT_STACK);
    fld<BYTE>(o, 8) = 0;
    dw(o, 0xc) = 0;
    obj_init_fields(o);
    return o;
}
// 0x49a430: the destructor (the behaviours left on it are not deleted)
static void THISCALL Stack_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(VT_STACK));
    dw(o, 0x10) = vt(VT_STACK_ARRAY_BASE);
    if (void *data = fld<void *>(o, 0x14)) {
        pool_get_free(data, dw(o, 0x1c) << 2);
        dw(o, 0x14) = 0;
        dw(o, 0x1c) = 0;
    }
    dw(o, 0x18) = 0;
    obj_dtor(o);
}
static void Stack_destroy(BYTE *o) { Stack_Dtor(o); }
// 0x49a0d0 (slot 1)
static void *THISCALL Stack_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, Stack_destroy>(self, flags); }

// 0x49a1e0
static DWORD __cdecl Stack_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD, glob_addr(0x613008) /* "Debug/InProcess" */, 8, 1, 8, 0, 0, 0);
    add_field(Stack_ClassInfo(), f);
    f = new_field(0x38, TYPED_RESOURCE_FIELD, glob_addr(0x612ff0) /* "Misc./Blocking Priority" */, 0xc, 4, 0xc, glob_addr(0x6bde48), 1, glob_addr(TEXT_IDLE), 0, 0);
    add_field(Stack_ClassInfo(), f);
    void *element = new_object_field(glob_addr(0x612fe0) /* "BehaviorItem" */, 0, Behavior_ClassInfo, 3);
    f = new_field(0x48, OBJECT_ARRAY_FIELD2, glob_addr(0x612fcc) /* "Debug/BehaviorList" */, 0x18, 4, 0x18, 0x1c, 4, 0x1c, 0x14, 4, 0x14, 1, element, 3, 0);
    return add_field(Stack_ClassInfo(), f);
}

// Ends the behaviour at index i (slot 29, which deletes it) and takes it out of the list (the count
// read again after the End)
static void stack_end_at(BYTE *s, BYTE *e, DWORD i)
{
    vcall<void *>(stack_data(s)[i], 29, e);
    const DWORD n = stack_count(s);
    if (i < n) {
        if (i + 1 < n)
            crt_memmove(stack_data(s) + i, stack_data(s) + i + 1, (n - (i + 1)) * 4);
        stack_count(s)--;
    }
}
// The top ended and removed, InProcess cleared, the priority 0 when the stack is empty
static void stack_pop(BYTE *s, BYTE *e)
{
    stack_end_at(s, e, stack_count(s) - 1);
    fld<BYTE>(s, 8) = 0;
    if (!stack_count(s))
        dw(s, 0xc) = 0;
}
static void stack_append(BYTE *s, BYTE *b)
{
    BYTE *item = b;
    vcall<void>(s + 0x10, 2, &item);
    dw(item, 8) |= 1;
}

// 0x49a760 (entity): begins the top behaviour (slot 23) until one stays: one that returns al 0 is
// ended and removed, then the next. eax: the count when the begun one is still the top, else 0.
static DWORD THISCALL Stack_BeginNext(BYTE *self, BYTE *e)
{
    DWORD n = stack_count(self);
    while (n) {
        BYTE *top = stack_data(self)[n - 1];
        fld<BYTE>(self, 8) = 1;
        if (vcall<bool>(top, 23, e)) {
            n = stack_count(self);
            if (!n)
                return 0;
            if (top == stack_data(self)[n - 1])
                return n;
        } else {
            fld<BYTE>(self, 8) = 0;
            stack_pop(self, e);
        }
        n = stack_count(self);
    }
    return 0;
}
// 0x49a480 (seconds, entity): begins the top when it has not begun, else updates it (slot 25); one
// that is done is ended and removed and the next begun. eax: the update's while running, else
// BeginNext's (0 for an empty stack).
static DWORD THISCALL Stack_Update(BYTE *self, float seconds, BYTE *e)
{
    const DWORD n = stack_count(self);
    if (!n)
        return 0;
    if (!fld<BYTE>(self, 8))
        return Stack_BeginNext(self, e);
    const DWORD r = vcall<DWORD>(stack_data(self)[n - 1], 25, seconds, e);
    if ((BYTE)r)
        return r;
    stack_end_at(self, e, stack_count(self) - 1);
    fld<BYTE>(self, 8) = 0;
    if (!stack_count(self))
        dw(self, 0xc) = 0;
    return Stack_BeginNext(self, e);
}
// 0x49a530 (entity, view): the begun top's debug drawing (slot 28)
static void THISCALL Stack_Draw(BYTE *self, void *a, void *b)
{
    if (fld<BYTE>(self, 8) && stack_count(self))
        vcall<void>(stack_data(self)[stack_count(self) - 1], 28, a, b);
}
// 0x49a560 (entity, behaviour, priority): with a priority at least the current one and below 3, ends
// and removes everything on the stack and pushes the behaviour (not begun yet). eax: the behaviour (also
// when refused; it is not deleted then).
static BYTE *THISCALL Stack_Push(BYTE *self, BYTE *e, BYTE *b, DWORD priority)
{
    if (!b)
        return nullptr;
    const DWORD current = dw(self, 0xc);
    if (priority < current || current >= 3)
        return b;
    fld<BYTE>(self, 8) = 0;
    while (stack_count(self))
        stack_pop(self, e);
    dw(self, 0xc) = priority;
    stack_append(self, b);
    return b;
}
// 0x49a620 (behaviour): puts it on top without ending the others. eax: it.
static BYTE *THISCALL Stack_Append(BYTE *self, BYTE *b)
{
    if (!b)
        return nullptr;
    stack_append(self, b);
    return b;
}
// 0x49a650 (entity, behaviour): ends and removes the top (whatever the behaviour). eax 0.
static DWORD THISCALL Stack_RemoveTop(BYTE *self, BYTE *e, void *)
{
    stack_pop(self, e);
    return 0;
}
// 0x49a6b0 (entity): ends and removes everything. eax 0.
static DWORD THISCALL Stack_Clear(BYTE *self, BYTE *e)
{
    fld<BYTE>(self, 8) = 0;
    while (stack_count(self))
        stack_pop(self, e);
    return 0;
}
// 0x49a730 (behaviour): whether it is on the stack
static bool THISCALL Stack_Contains(BYTE *self, void *b)
{
    for (DWORD i = 0; i < stack_count(self); i++)
        if (stack_data(self)[i] == b)
            return true;
    return false;
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_behaviors)
{
    const Hook list[] = {
        {0x494410, (void *)&Behavior_ClassInfo, "CEntityBehavior::ClassInfo", 5, {0xa1, 0xcc, 0xdd, 0x6b, 0x00, 0x85, 0xc0, 0x75}},
        {0x4944e0, (void *)&Behavior_Dtor, "CEntityBehavior::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x30, 0x29, 0x5f}},
        {0x4944b0, (void *)&Behavior_DeletingDtor, "CEntityBehavior::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x30, 0x29, 0x5f}},
        {0x494490, (void *)&Behavior_Begin, "CEntityBehavior::Begin", 8, {0x8b, 0x51, 0x08, 0xb8, 0x01, 0x00, 0x00, 0x00}},
        {0x4944a0, (void *)&Behavior_Update, "CEntityBehavior::Update", 5, {0x32, 0xc0, 0xc2, 0x08, 0x00, 0x90, 0x90, 0x90}},
        {0x494500, (void *)&Behavior_End, "CEntityBehavior::End", 0, {0x85, 0xc9, 0x74, 0x07, 0x8b, 0x01, 0x6a, 0x01}},
        {0x494560, (void *)&TurnBase_ClassInfo, "CEntityBehaviorTurnToAngleBase::ClassInfo", 5, {0xa1, 0x04, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x494510, (void *)&TurnBase_ClassInfo, "CEntityBehaviorTurnToAngleBase::ClassInfo2", 5, {0xa1, 0x04, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4948f0, (void *)&TurnMoving_ClassInfo, "CEntityBehaviorTurnMovingAngle::ClassInfo", 5, {0xa1, 0x08, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4946e0, (void *)&TurnMoving_ClassInfo, "CEntityBehaviorTurnMovingAngle::ClassInfo2", 5, {0xa1, 0x08, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x494bf0, (void *)&TurnAiming_ClassInfo, "CEntityBehaviorTurnAimingAngle::ClassInfo", 5, {0xa1, 0x0c, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4949e0, (void *)&TurnAiming_ClassInfo, "CEntityBehaviorTurnAimingAngle::ClassInfo2", 5, {0xa1, 0x0c, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x494f90, (void *)&TurnToAngle_ClassInfo, "CEntityBehaviorTurnToAngle::ClassInfo", 5, {0xa1, 0x10, 0xde, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x494ce0, (void *)&TurnToAngle_ClassInfo, "CEntityBehaviorTurnToAngle::ClassInfo2", 5, {0xa1, 0x10, 0xde, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x495920, (void *)&TurnToTarget_ClassInfo, "CEntityBehaviorTurnToTarget::ClassInfo", 5, {0xa1, 0x14, 0xde, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x495660, (void *)&TurnToTarget_ClassInfo, "CEntityBehaviorTurnToTarget::ClassInfo2", 5, {0xa1, 0x14, 0xde, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x495360, (void *)&TurnToAngle_Dtor, "CEntityBehaviorTurnToAngle::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x28, 0x2b, 0x5f}},
        {0x494650, (void *)&TurnBase_DeletingDtor, "CEntityBehaviorTurnToAngleBase::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xa8, 0x29, 0x5f}},
        {0x494850, (void *)&TurnMoving_DeletingDtor, "CEntityBehaviorTurnMovingAngle::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x494b50, (void *)&TurnAiming_DeletingDtor, "CEntityBehaviorTurnAimingAngle::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x494ed0, (void *)&TurnToAngle_DeletingDtor, "CEntityBehaviorTurnToAngle::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x495830, (void *)&TurnToTarget_DeletingDtor, "CEntityBehaviorTurnToTarget::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4945c0, (void *)&TurnBase_RegisterFields, "CEntityBehaviorTurnToAngleBase::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x71, 0xbd, 0x13}, 5},
        {0x4950a0, (void *)&TurnToAngle_RegisterFields, "CEntityBehaviorTurnToAngle::RegisterFields", 6, {0x53, 0x55, 0x56, 0x57, 0x6a, 0x44, 0xe8, 0x8f}},
        {0x495a30, (void *)&TurnToTarget_RegisterFields, "CEntityBehaviorTurnToTarget::RegisterFields", 6, {0x53, 0x55, 0x56, 0x57, 0x6a, 0x30, 0xe8, 0xff}},
        {0x494680, (void *)&TurnBase_Begin, "CEntityBehaviorTurnToAngleBase::Begin", 9, {0x8b, 0x44, 0x24, 0x04, 0x68, 0x00, 0x00, 0x20}},
        {0x4946b0, (void *)&TurnBase_End, "CEntityBehaviorTurnToAngleBase::End", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x4949b0, (void *)&TurnMoving_Angle, "CEntityBehaviorTurnMovingAngle::Angle", 10, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x80, 0xc8, 0x00}},
        {0x494cb0, (void *)&TurnAiming_Angle, "CEntityBehaviorTurnAimingAngle::Angle", 10, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x80, 0xc4, 0x00}},
        {0x4949c0, (void *)&TurnMoving_SetAngle, "CEntityBehaviorTurnMovingAngle::SetAngle", 8, {0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x494cc0, (void *)&TurnAiming_SetAngle, "CEntityBehaviorTurnAimingAngle::SetAngle", 8, {0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x495490, (void *)&TurnToAngle_Update, "CEntityBehaviorTurnToAngle::Update", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x56, 0x57}},
        {0x495620, (void *)&TurnToAngle_Draw, "CEntityBehaviorTurnToAngle::Draw", 7, {0x83, 0xec, 0x08, 0x8d, 0x44, 0x24, 0x00, 0x56}},
        {0x4953a0, (void *)&TurnToAngle_SetTarget, "CEntityBehaviorTurnToAngle::SetTarget", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x18, 0xc2}},
        {0x4953b0, (void *)&TurnToAngle_Make, "EntityBehaviorTurnToAngle_Make", 5, {0x55, 0x8b, 0xec, 0x56, 0x57, 0x8b, 0xf9, 0xb9}},
        {0x495ec0, (void *)&TurnToTarget_Make, "EntityBehaviorTurnToTarget_Make", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x496040, (void *)&TurnToTarget_OnMessage, "CEntityBehaviorTurnToTarget::OnMessage", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x8b, 0x6c}},
        {0x4960f0, (void *)&TurnToTarget_Update, "CEntityBehaviorTurnToTarget::Update", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x56, 0x8b}},
        {0x4964c0, (void *)&SetRotation_ClassInfo, "CEntityBehaviorSetRotationSequence::ClassInfo", 5, {0xa1, 0x18, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x496320, (void *)&SetRotation_ClassInfo, "CEntityBehaviorSetRotationSequence::ClassInfo2", 5, {0xa1, 0x18, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x496860, (void *)&PlayOnce_ClassInfo, "CEntityBehaviorPlaySequenceOnce::ClassInfo", 5, {0xa1, 0x1c, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x496620, (void *)&PlayOnce_ClassInfo, "CEntityBehaviorPlaySequenceOnce::ClassInfo2", 5, {0xa1, 0x1c, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x496420, (void *)&SetRotation_DeletingDtor, "CEntityBehaviorSetRotationSequence::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4967a0, (void *)&PlayOnce_DeletingDtor, "CEntityBehaviorPlaySequenceOnce::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x496520, (void *)&SetRotation_RegisterFields, "CEntityBehaviorSetRotationSequence::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x11, 0x9e, 0x13}, 5},
        {0x496920, (void *)&PlayOnce_RegisterFields, "CEntityBehaviorPlaySequenceOnce::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x44, 0xe8, 0x10, 0x9a}},
        {0x4965b0, (void *)&SetRotation_Begin, "CEntityBehaviorSetRotationSequence::Begin", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0xd9}},
        {0x496e20, (void *)&PlayOnce_Make, "EntityBehaviorPlaySequenceOnce_Make", 6, {0x56, 0x57, 0x8b, 0xfa, 0x8b, 0xf1, 0x85, 0xff}},
        {0x496ee0, (void *)&PlayOnce_Begin, "CEntityBehaviorPlaySequenceOnce::Begin", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x497010, (void *)&PlayOnce_Update, "CEntityBehaviorPlaySequenceOnce::Update", 5, {0xa1, 0xfc, 0xdd, 0x6b, 0x00, 0x83, 0xec, 0x0c}},
        {0x497270, (void *)&PlayOnce_End, "CEntityBehaviorPlaySequenceOnce::End", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x4973f0, (void *)&SetPlayRatio_ClassInfo, "CEntityBehaviorSetPlayRatio::ClassInfo", 5, {0xa1, 0x20, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x497290, (void *)&SetPlayRatio_ClassInfo, "CEntityBehaviorSetPlayRatio::ClassInfo2", 5, {0xa1, 0x20, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4976c0, (void *)&MarkForDeletion_ClassInfo, "CEntityBehaviorMarkForDeletion::ClassInfo", 5, {0xa1, 0x24, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x497560, (void *)&MarkForDeletion_ClassInfo, "CEntityBehaviorMarkForDeletion::ClassInfo2", 5, {0xa1, 0x24, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x497910, (void *)&ClearFlags_ClassInfo, "CEntityBehaviorClearFlags::ClassInfo", 5, {0xa1, 0x28, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4977a0, (void *)&ClearFlags_ClassInfo, "CEntityBehaviorClearFlags::ClassInfo2", 5, {0xa1, 0x28, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x499470, (void *)&SetCurFrame_ClassInfo, "CEntityBehaviorSetCurFrame::ClassInfo", 5, {0xa1, 0x3c, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x499310, (void *)&SetCurFrame_ClassInfo, "CEntityBehaviorSetCurFrame::ClassInfo2", 5, {0xa1, 0x3c, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4996a0, (void *)&ExecuteAction_ClassInfo, "CEntityBehaviorExecuteAction::ClassInfo", 5, {0xa1, 0x40, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x499540, (void *)&ExecuteAction_ClassInfo, "CEntityBehaviorExecuteAction::ClassInfo2", 5, {0xa1, 0x40, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x497370, (void *)&SetPlayRatio_DeletingDtor, "CEntityBehaviorSetPlayRatio::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x497640, (void *)&MarkForDeletion_DeletingDtor, "CEntityBehaviorMarkForDeletion::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x497890, (void *)&ClearFlags_DeletingDtor, "CEntityBehaviorClearFlags::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4993f0, (void *)&SetCurFrame_DeletingDtor, "CEntityBehaviorSetCurFrame::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x499620, (void *)&ExecuteAction_DeletingDtor, "CEntityBehaviorExecuteAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x497450, (void *)&SetPlayRatio_RegisterFields, "CEntityBehaviorSetPlayRatio::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xe1, 0x8e, 0x13}, 5},
        {0x497970, (void *)&ClearFlags_RegisterFields, "CEntityBehaviorClearFlags::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xc1, 0x89, 0x13}, 5},
        {0x499700, (void *)&ExecuteAction_RegisterFields, "CEntityBehaviorExecuteAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x31, 0x6c, 0x13}, 5},
        {0x4974e0, (void *)&SetPlayRatio_Make, "EntityBehaviorSetPlayRatio_Make", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x497530, (void *)&SetPlayRatio_Begin, "CEntityBehaviorSetPlayRatio::Begin", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x497720, (void *)&MarkForDeletion_Make, "EntityBehaviorMarkForDeletion_Make", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x497760, (void *)&MarkForDeletion_Begin, "CEntityBehaviorMarkForDeletion::Begin", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x497c10, (void *)&ClearFlags_Begin, "CEntityBehaviorClearFlags::Begin", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x4994d0, (void *)&SetCurFrame_Make, "EntityBehaviorSetCurFrame_Make", 9, {0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x10, 0x00, 0x00}},
        {0x499520, (void *)&SetCurFrame_Begin, "CEntityBehaviorSetCurFrame::Begin", 7, {0x8b, 0x41, 0x0c, 0x8b, 0x4c, 0x24, 0x04, 0x50}},
        {0x499790, (void *)&ExecuteAction_Begin, "CEntityBehaviorExecuteAction::Begin", 5, {0x8d, 0x41, 0x0c, 0x50, 0x51, 0x8b, 0x4c, 0x24}},
        {0x497db0, (void *)&Wait_ClassInfo, "CEntityBehaviorWait::ClassInfo", 5, {0xa1, 0x2c, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x497c50, (void *)&Wait_ClassInfo, "CEntityBehaviorWait::ClassInfo2", 5, {0xa1, 0x2c, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x498090, (void *)&Transition_ClassInfo, "CEntityBehaviorTransition::ClassInfo", 5, {0xa1, 0x30, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x497fe0, (void *)&Transition_ClassInfo, "CEntityBehaviorTransition::ClassInfo2", 5, {0xa1, 0x30, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4984f0, (void *)&Fade_ClassInfo, "CEntityBehaviorFade::ClassInfo", 5, {0xa1, 0x34, 0xde, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x498270, (void *)&Fade_ClassInfo, "CEntityBehaviorFade::ClassInfo2", 5, {0xa1, 0x34, 0xde, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x497d30, (void *)&Wait_DeletingDtor, "CEntityBehaviorWait::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x498230, (void *)&Transition_DeletingDtor, "CEntityBehaviorTransition::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xf8, 0x2e, 0x5f}},
        {0x498430, (void *)&Fade_DeletingDtor, "CEntityBehaviorFade::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x497e10, (void *)&Wait_RegisterFields, "CEntityBehaviorWait::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x21, 0x85, 0x13}, 5},
        {0x498150, (void *)&Transition_RegisterFields, "CEntityBehaviorTransition::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x38, 0xe8, 0xe1, 0x81, 0x13}, 5},
        {0x497f30, (void *)&Wait_Make, "EntityBehaviorWait_Make", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x497f80, (void *)&Wait_Update, "CEntityBehaviorWait::Update", 6, {0xd9, 0x41, 0x10, 0xd8, 0x59, 0x0c, 0xdf, 0xe0}},
        {0x498600, (void *)&Fade_Update, "CEntityBehaviorFade::Update", 6, {0xd9, 0x41, 0x10, 0xd8, 0x59, 0x0c, 0xdf, 0xe0}},
        {0x4988a0, (void *)&Idle_ClassInfo, "CEntityBehaviorIdle::ClassInfo", 5, {0xa1, 0x38, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x498660, (void *)&Idle_ClassInfo, "CEntityBehaviorIdle::ClassInfo2", 5, {0xa1, 0x38, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x498dc0, (void *)&Idle_Dtor, "CEntityBehaviorIdle::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xe8, 0x2f, 0x5f}},
        {0x4987e0, (void *)&Idle_DeletingDtor, "CEntityBehaviorIdle::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x498960, (void *)&Idle_RegisterFields, "CEntityBehaviorIdle::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xd0, 0x79}},
        {0x498df0, (void *)&Idle_ResetFields, "CEntityBehaviorIdle::ResetFields", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x38, 0xe4, 0xf7, 0xff}, 4},
        {0x498e10, (void *)&Idle_Slot18, "CEntityBehaviorIdle::Slot18", 8, {0x83, 0xc1, 0x18, 0xe8, 0x98, 0x64, 0x04, 0x00}, 4},
        {0x498e20, (void *)&Idle_SequenceName, "EntityBehaviorIdle_SequenceName", 6, {0x51, 0x53, 0x55, 0x57, 0x8b, 0xf9, 0x85, 0xff}},
        {0x498f10, (void *)&Idle_Make, "EntityBehaviorIdle_Make", 5, {0x56, 0x8b, 0xf1, 0x33, 0xd2, 0xe8, 0x06, 0xff}},
        {0x498fa0, (void *)&Idle_Start, "CEntityBehaviorIdle::Start", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x498710, (void *)&Idle_Begin, "CEntityBehaviorIdle::Begin", 6, {0x8b, 0x44, 0x24, 0x04, 0x6a, 0x01, 0x50, 0xe8}},
        {0x4990e0, (void *)&Idle_Update, "CEntityBehaviorIdle::Update", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53, 0x56}},
        {0x499950, (void *)&PlayLooped_ClassInfo, "CEntityBehaviorPlaySequenceLooped::ClassInfo", 5, {0xa1, 0x44, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4997b0, (void *)&PlayLooped_ClassInfo, "CEntityBehaviorPlaySequenceLooped::ClassInfo2", 5, {0xa1, 0x44, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x499cf0, (void *)&PlayLooped_Dtor, "CEntityBehaviorPlaySequenceLooped::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x54, 0x31, 0x5f}},
        {0x4998b0, (void *)&PlayLooped_DeletingDtor, "CEntityBehaviorPlaySequenceLooped::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4999b0, (void *)&PlayLooped_RegisterFields, "CEntityBehaviorPlaySequenceLooped::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x81, 0x69, 0x13}, 5},
        {0x499d10, (void *)&PlayLooped_Begin, "CEntityBehaviorPlaySequenceLooped::Begin", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8d, 0x7e, 0x10, 0x8b}},
        {0x499d50, (void *)&PlayLooped_Update, "CEntityBehaviorPlaySequenceLooped::Update", 6, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x499f30, (void *)&PlayLooped_OnMessage, "CEntityBehaviorPlaySequenceLooped::OnMessage", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0xf1}},
        {0x499fb0, (void *)&Stack_ClassInfo, "CEntityBehaviorStack::ClassInfo", 5, {0xa1, 0x64, 0xde, 0x6b, 0x00, 0x85, 0xc0, 0x75}},
        {0x49a3a0, (void *)&Stack_Ctor, "CEntityBehaviorStack::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x68, 0xc2, 0xf7, 0xff}, 4},
        {0x49a430, (void *)&Stack_Dtor, "CEntityBehaviorStack::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xcc, 0x31, 0x5f}},
        {0x49a0d0, (void *)&Stack_DeletingDtor, "CEntityBehaviorStack::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x49a1e0, (void *)&Stack_RegisterFields, "CEntityBehaviorStack::RegisterFields", 8, {0x56, 0x6a, 0x30, 0xe8, 0x52, 0x61, 0x13, 0x00}, 4},
        {0x49a760, (void *)&Stack_BeginNext, "CEntityBehaviorStack::BeginNext", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57}},
        {0x49a480, (void *)&Stack_Update, "CEntityBehaviorStack::Update", 7, {0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x18, 0x85}},
        {0x49a530, (void *)&Stack_Draw, "CEntityBehaviorStack::Draw", 5, {0x8a, 0x41, 0x08, 0x84, 0xc0, 0x74, 0x1d, 0x8b}},
        {0x49a560, (void *)&Stack_Push, "CEntityBehaviorStack::Push", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x49a620, (void *)&Stack_Append, "CEntityBehaviorStack::Append", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x74, 0x1f}},
        {0x49a650, (void *)&Stack_RemoveTop, "CEntityBehaviorStack::RemoveTop", 7, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x77, 0x18, 0x8b}},
        {0x49a6b0, (void *)&Stack_Clear, "CEntityBehaviorStack::Clear", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x88, 0x5e}},
        {0x49a730, (void *)&Stack_Contains, "CEntityBehaviorStack::Contains", 5, {0x8b, 0x51, 0x18, 0x33, 0xc0, 0x56, 0x85, 0xd2}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
