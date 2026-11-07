// Building blocks shared by the AI modules ai_patrol.cpp, ai_pickup.cpp and ai_player.cpp: the exe
// functions they call (through the address layer, reimpl.h: in the DLL their exe addresses, so Disable=
// and the checks' stand-ins apply), the constructor, destructor and registration patterns of the AI
// classes, x87 helpers. The helpers whose callers pass addresses known only at run time (class_at,
// find_field, field_color_rgba, vtable_and_defaults/free, ent_play_sequence) look them up at run time.
//
// Constructors are inlined into Create and CreateArray in the exe, one level per class (the parent's
// level, the members, the vtable, Object_InitFields); destructors into the deleting destructors (slot 1:
// bit 1 destroys an array from its last element, bit 0 frees). RegisterFields construct one descriptor
// after the other and append it through the class getter, which they inline (a failed allocation
// appends a null descriptor as in the original).
#pragma once
#include "exe_base.h"
#include "x87.h"

namespace zax::ai {

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);

template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

// ---- CObject, strings, class infos ---------------------------------------------------------------------

inline void obj_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
inline void obj_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
inline void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
inline void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
inline bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
// 0x46cd50: CObject's slot 0 (a message), also CLayerPartAI's slot 25 (the base Update)
inline DWORD obj_on_message(void *o, void *a, DWORD b) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x46cd50)(o, a, b); }
inline void zs_ctor(BYTE *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
inline void zs_dtor(BYTE *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
inline const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
inline void zs_assign_text(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
inline ZString *zstring(void *p, DWORD off) { return reinterpret_cast<ZString *>(static_cast<BYTE *>(p) + off); }
template <DWORD A> inline CClassInfo *__cdecl getter() { return fn<Getter>(A)(); }
inline CClassInfo *class_at(DWORD getter_address) { return fn_dyn<Getter>(getter_address)(); }
inline DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
inline void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
inline DWORD set_help(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, glob<const char *>(text)); }
// 0x4154d0: the class's descriptor of that name (the second argument 0 in every caller)
inline BYTE *find_field(CClassInfo *c, DWORD name, DWORD a) { return fn<BYTE *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, glob_dyn<const char *>(name), a); }

// Class getters of other classes
inline CClassInfo *layer_part_ai_class() { return fn<Getter>(0x4fd220)(); }
inline CClassInfo *entity_animated_class() { return fn<Getter>(0x48dcc0)(); }
inline CClassInfo *entity_movable_class() { return fn<Getter>(0x4903f0)(); }   // CEntityMovable
inline CClassInfo *entity_walking_class() { return fn<Getter>(0x49b810)(); }   // CEntityWalking
inline CClassInfo *action_class() { return fn<Getter>(0x41a650)(); }

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34 bytes
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD OBJECT_REF_FIELD_CTOR = 0x523e30;       // 0x30
constexpr DWORD FLAGS_FIELD_CTOR = 0x414cf0;            // 0x38
constexpr DWORD POINT_FIELD_CTOR = 0x4a7fa0;            // 0x34
constexpr DWORD INT_FIELD_CTOR = 0x54d480;              // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD EMBEDDED_FIELD_CTOR = 0x54e690;         // 0x30
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44
constexpr DWORD DOUBLE_FIELD_CTOR = 0x54f060;           // 0x54

// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> inline void *new_field(DWORD size, FnKey ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
// The same with a subclass's vtable written over the constructor's
template <typename... A> inline void *new_field_vt(DWORD size, VtKey vtable, FnKey ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f) {
        fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
        dw(f, 0) = key_vt(vtable);
    }
    return f;
}

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated, parent null = derived from CObject)
inline CClassInfo *class_getter(GlobKey var, GlobKey name, float version, Getter parent, DWORD size, DWORD abstract, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, abstract,
                                     key_fn_addr(create), key_fn_addr(create_array), key_fn_addr(register_fields));
    return v;
}

// ---- Constructor and destructor patterns ---------------------------------------------------------------

// The end of every constructor level: the class's vtable, then Object_InitFields
inline void vtable_and_defaults(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vt_dyn(vtable);
    obj_init_fields(o);
}
// The start of every destructor level: the class's vtable, then Object_FreeFields
inline void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vt_dyn(vtable);
    obj_free_fields(o);
}
// CAIBase's constructor 0x4fd060 (ai_classes.cpp) and destructor 0x4fd080 (CObject's with the vtable)
inline void ai_base_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x4fd060)(o); }
inline void ai_base_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4fd080)(o); }
// The CAIBase and CLayerPartAI levels most AI constructors start with
inline void layer_part_ai_ctor(BYTE *o)
{
    ai_base_ctor(o);
    vtable_and_defaults(o, 0x5ed1d0);   // CLayerPartAI
}
// CEntityBehaviorMoveToPos embedded in the patrol AIs (0xbc bytes): constructor 0x4302c0 (ai_classes.cpp),
// destructor 0x430350 (ai_rest_a.cpp)
inline void move_to_pos_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x4302c0)(o); }
inline void move_to_pos_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x430350)(o); }

using Construct = void (*)(BYTE *);
using Destroy = void (*)(BYTE *);

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> inline void *create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}
// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> inline void *create_array(int count)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * SIZE + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            C(e + i * SIZE);
    return e;
}
// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the
// block, bit 0 frees (array: operator delete, single object: the pool)
template <DWORD SIZE, Destroy D> inline void *deleting_dtor(BYTE *self, DWORD flags)
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

// ---- Layer parts, entities, behaviours -----------------------------------------------------------------

// 0x4fb0a0: the part's centre (polygon mean + offset, or the position)
inline float *part_position(void *part, float *out) { return fn<float *(THISCALL *)(void *, float *)>(0x4fb0a0)(part, out); }
// Slots of a layer part (CLayerPart): 40 position or offset, 43 position, 48 polygon, 50 offset,
// 58/59 set/clear flags, 71 set the target
inline float *slot_position40(void *part, float *out) { return vcall<float *>(part, 40, out); }
inline float *slot_position(void *part, float *out) { return vcall<float *>(part, 43, out); }
inline BYTE *slot_polygon(void *part) { return vcall<BYTE *>(part, 48); }
inline float *slot_offset(void *part, float *out) { return vcall<float *>(part, 50, out); }
inline void slot_set_flags(void *part, DWORD bits) { vcall<void>(part, 58, bits); }
inline void slot_clear_flags(void *part, DWORD bits) { vcall<void>(part, 59, bits); }
// The polygon's points (float x, y each) and their count
inline float *poly_points(BYTE *poly) { return reinterpret_cast<float *>(static_cast<uintptr_t>(dw(poly, 0))); }
inline DWORD poly_count(BYTE *poly) { return dw(poly, 8); }

// CEntityAnimated (Objects): the sequence and play ratio setters
// (the name: a string of the exe's data or, from a model, of the heap; looked up at run time)
inline void ent_play_sequence(void *e, DWORD name) { fn<void(THISCALL *)(void *, const char *)>(0x48f700)(e, glob_dyn<const char *>(name)); }
inline void ent_set_play_ratio(void *e, DWORD ratio) { fn<void(THISCALL *)(void *, DWORD)>(0x48f6a0)(e, ratio); }
// The entity's behaviour stack (CEntityBehaviorStack at +0x90; ai_behaviors.cpp)
inline BYTE *stack_push(BYTE *stack, void *e, BYTE *b, DWORD priority) { return fn<BYTE *(THISCALL *)(void *, void *, void *, DWORD)>(0x49a560)(stack, e, b, priority); }
inline BYTE *stack_append(BYTE *stack, BYTE *b) { return fn<BYTE *(THISCALL *)(void *, void *)>(0x49a620)(stack, b); }
inline void stack_clear(BYTE *stack, void *e) { fn<DWORD(THISCALL *)(void *, void *)>(0x49a6b0)(stack, e); }

// CEntityBehaviorMoveToPos (AI, 0x42f8e0-0x430900; ai_approach.cpp, ai_rest_a.cpp): 0x4303f0 (entity, target point,
// speed, a flag) aims it at the point (al: a way was found); its slots 23 Begin (entity), 25 Update
// (seconds, entity; al: still moving), 28 debug drawing (entity, view). Floats the exe passes with
// integer moves are passed as their bits (a signalling NaN stays one), those it passes through the FPU
// through fpu_float().
inline DWORD move_to(BYTE *m, void *e, const float *target, DWORD speed, DWORD flag)
{
    return fn<DWORD(THISCALL *)(void *, void *, const float *, DWORD, DWORD)>(0x4303f0)(m, e, target, speed, flag);
}
// A float as the exe passes it after fld/fstp: its bits through the FPU (a signalling NaN quietened)
inline DWORD fpu_float(float f) { return float_bits(fstp(fld(f))); }

// 0x41ad40 (fastcall layer, name; then a2, a4, a3, the caller, 1, 0): the part a name refers to
inline BYTE *find_named_part(void *layer, void *name, void *a2, void *a4, void *caller, DWORD a5, DWORD a6, DWORD a7)
{
    return fn<BYTE *(__fastcall *)(void *, void *, void *, void *, void *, DWORD, DWORD, DWORD)>(0x41ad40)(layer, name, a2, a4, caller, a5,
                                                                                                         a6, a7);
}
// 0x41a720: an action's slot 23 (layer, a2, a3, a4) with the caller and the field (unused by the slots)
inline DWORD action_execute(void *action, void *layer, void *a2, void *a3, void *a4, void *self, void *field)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, void *, void *, void *, void *)>(0x41a720)(action, layer, a2, a3, a4, self, field);
}

// A double in memory loaded (fld qword) / stored (fst qword, the value stays on the FPU unrounded)
inline long double fld64(const void *p)
{
    long double r;
    __asm__("fldl %1" : "=t"(r) : "m"(*static_cast<const double *>(p)));
    return r;
}
inline void fst64(long double v, void *p) { __asm__ volatile("fstl %0" : "=m"(*static_cast<double *>(p)) : "t"(v)); }

// ---- Random numbers and angles --------------------------------------------------------------------------

constexpr DWORD GAME_RANDOM = 0x7124c0, RANDOM2 = 0x6d06c0, RANDOM3 = 0x712cc0;
// 0x55c470: an integer below n; 0x55c550: a float between lo and hi (st0)
inline DWORD random_below(GlobKey gen, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55c470)(glob<void *>(gen), n); }
inline long double random_range(GlobKey gen, DWORD lo, DWORD hi) { return fn<long double(THISCALL *)(void *, DWORD, DWORD)>(0x55c550)(glob<void *>(gen), lo, hi); }
// The exe's table functions (math_tables.cpp): 0x508da0 the cosine table (Table_Cos), 0x508ec0 the sine table
// (Table_Sin), 0x509100 atan2 (y, x), 0x508fe0 both (ecx the sine out, edx the cosine out). Until 2026-11-01
// table_sin and table_cos were swapped here (their callers' code did not change with the rename).
inline long double table_cos(float a) { return fn<long double(__stdcall *)(float)>(0x508da0)(a); }
inline long double table_sin(float a) { return fn<long double(__stdcall *)(float)>(0x508ec0)(a); }
inline long double table_atan2(float y, float x) { return fn<long double(__stdcall *)(float, float)>(0x509100)(y, x); }
inline long double table_atan2_bits(DWORD y, DWORD x) { return fn<long double(__stdcall *)(DWORD, DWORD)>(0x509100)(y, x); }
inline void table_sin_cos(float *c, float *d, float a) { fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(c, d, a); }

// An angle stored as a float into [0, 2 pi): + 2 pi when below 0 (the unrounded value compared),
// - 2 pi when not below 2 pi (the stored one compared)
inline float wrap_two_pi(long double a)
{
    float v = fstp(a);
    if (c0(a, cst(C_ZERO)))
        v = fstp(fld(v) + cst(C_TWO_PI));
    if (!c0(fld(v), cst(C_TWO_PI)))
        v = fstp(fld(v) - cst(C_TWO_PI));
    return v;
}

// ---- Debug drawing helpers (Rendering) --------------------------------------------------------------------

// 0x53f010: FieldColor_CtorRGBA (ai_pickup.cpp)
inline void *field_color_rgba(DWORD color, DWORD r, DWORD g, DWORD b, DWORD a)
{
    return fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob_dyn<void *>(color), r, g, b, a);
}
// The function-local static colours of the debug drawing: the exe's guard bit, object and atexit
// function (an empty destructor), so the reimplementation has no static of its own
inline void *static_color(GlobKey guard, GlobKey color, DWORD r, DWORD g, DWORD b, DWORD a, CodeKey exit_fn)
{
    BYTE &bits = *glob<BYTE *>(guard);
    if (!(bits & 1)) {
        bits |= 1;
        field_color_rgba(key_address(color), r, g, b, a);
        crt_atexit(code<void(__cdecl *)()>(exit_fn));
    }
    return glob<void *>(color);
}
// 0x4fccc0 (view in ecx, centre in edx; radius, y scale, colour): an oval
inline DWORD draw_oval(void *view, const float *center, DWORD radius, DWORD y_scale, void *color)
{
    return fn<DWORD(__fastcall *)(void *, const float *, DWORD, DWORD, void *)>(0x4fccc0)(view, center, radius, y_scale, color);
}
constexpr DWORD Y_SCALE = 0x3f3677d4;   // 0.7128

// The error reporter (slot 13: the time in seconds)
constexpr DWORD G_REPORTER = 0x6c02cc, G_WORLD = 0x6c2080;
inline void *reporter() { return *glob<void **>(G_REPORTER); }
inline void *world() { return *glob<void **>(G_WORLD); }

}  // namespace zax::ai
