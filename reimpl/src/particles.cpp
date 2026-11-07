// The particle AIs of 0x528000-0x530fff (phase 3): CParticleAI and its classes CGravityParticleAI,
// CFloatParticleAI and CSwirlParticleAI (the AI a single particle entity carries), CParticleSystemAI and
// CGenericParticleSystemAI (the emitter: bursts, the particles' start position, direction, speed and
// age from the envelopes, the particle entities made from models): class getters (both copies),
// deleting destructors, RegisterFields, the behaviour slots and their helpers, and the vector helpers
// 0x52a200, 0x535400, 0x535420, the pointer array constructor 0x535440.
//
// Layouts [confirmed: constructors (ai_classes.cpp), RegisterFields, docs/fields.csv]:
//   CParticleAI (0x24, class 0x6d0174, version 2, abstract, parent CLayerPartAI): +0x10/+0x14/+0x18 "X/Y/Z
//     Center Pos", +0x1c (no field) follow the parent's position, +0x1d "Animated", +0x1e "Obey Sequence
//     FPS", +0x20 "Y To X Ratio".
//   CGravityParticleAI (0x34, 0x6d0178, version 3): +0x24 "Gravity" (9.8), +0x28 "Bounce Sound", +0x2c
//     "Use Velocity to Advance Frames", +0x2d "Bounced Last Frame", +0x30 "Gravity Constant".
//   CFloatParticleAI (0x24, 0x6d017c): nothing of its own (RegisterFields is CAction's empty 0x58ea40).
//   CSwirlParticleAI (0x34, 0x6d0180): +0x24 "Angular Velocity", +0x28 "Angular Velocity Variance", +0x2c
//     "Rotate Velocity", +0x30 "Debug/Current Angular Velocity".
//   CParticleSystemAI (0x134, 0x6d04f4, version 4, abstract): six embedded envelopes (CEnvelope: +8
//     start, +0xc current, +0x10 end, +0x14 speed; the value used is the start value when the speed is
//     0, else the current one): +0x10 "Outer Cone Angle", +0x34 "Inner Cone Angle", +0x58 "Face Angle",
//     +0x7c "Cone Trajectory", +0xa8 "Speed", +0xd0 "Age"; +0xa0/+0xa4 "Burst Frequency" and its
//     variance, +0xcc/+0xf4 the speed and age variances, +0xf8/+0xfc "Max/Min Travel Distance", +0x100
//     "NumBursts" (the fraction of a burst due), +0x104 "CurFrequency", +0x108 "Make Child of
//     Generator", +0x109 "Limit Duration To First Collision", +0x10a "Enabled", +0x10b "Disable in
//     multi-player", +0x10c the bursts so far, +0x110 "Start Point Type", +0x114 "Particles" (pointer
//     array: +0x118 data, +0x11c count, +0x120 capacity), +0x124 "Sounds" (+0x128, +0x12c, +0x130).
//   CGenericParticleSystemAI (0x148, 0x6d04fc, version 13): +0x134/+0x138/+0x13c "Position/X Offset, Y
//     Offset, Height", +0x140 "Num Bursts To Stop After", +0x144 "Use Particle As Shot", +0x145 "Orient
//     To Parent's Rotation", +0x146 "Choose Sound Based On Burst".
//
// Slots (names ours [assumption]): CLayerPartAI's 19 (reset fields), 23 (entity: begin of the particle),
// 24 (begin of the system), 25/26 (entity, seconds: updates), 29 (message), 31 (whether two parts may
// touch); the particle AI's 42 (centre), 43 (animated, obey FPS, follow the parent), 44 (Y to X ratio),
// 45 (entity, seconds: the frame), the gravity AI's 46 (the bounce); the system's 42/44/45 (x offset,
// height, bursts to stop after), 43 (y offset, 0x46acb0), 46/47/48 (the three switches). Floating point
// follows the original instruction by instruction (x87.h).
// Left out (Rendering): the system's debug drawing (slot 41 0x530830 with 0x52ecd0 and 0x52b010), the
// sprite blitters 0x530d70-0x535310 (the table 0x5fc048 the static initialisers 0x52a220- fill) and the
// palette mapping 0x5280e0.
#include "class_info.h"
#include "ai_common.h"
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ---------------------------------------------------------------------------------

constexpr DWORD CLS_PARTICLE_AI = 0x6d0174, CLS_GRAVITY = 0x6d0178, CLS_FLOAT = 0x6d017c, CLS_SWIRL = 0x6d0180,
                CLS_SYSTEM = 0x6d04f4, CLS_GENERIC = 0x6d04fc;
constexpr DWORD CREATE_NONE = 0x46b760, CREATE_ARRAY_NONE = 0x52dc80, NO_FIELDS = 0x58ea40;
constexpr DWORD C_DEG = 0x5fc54c;     // the factor of the angle envelopes (degrees to radians)
constexpr DWORD C_DEG2 = 0x5fc550;    // the same for the trajectory
constexpr DWORD MSG_ON = 0x6d0210, MSG_OFF = 0x6d04d4;   // "Particle System On" / "Particle System Off"
constexpr DWORD G_SYSTEMS_OFF = 0x6d04f8;                // a byte: all particle systems stopped
constexpr DWORD MODEL_LIST = 0x6cfdd8;
constexpr DWORD ENVELOPES[6] = {0x10, 0x34, 0x58, 0x7c, 0xa8, 0xd0};   // the embedded envelopes in order

CClassInfo *projectile_class() { return zax::fn<Getter>(0x491930)(); }
CClassInfo *entity_base_class() { return zax::fn<Getter>(0x487620)(); }
CClassInfo *particle_ai_class() { return zax::fn<Getter>(0x528550)(); }
BYTE *sequence_object(void *e) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
long double projectile_duration(void *e) { return zax::fn<long double(THISCALL *)(void *)>(0x492080)(e); }
void set_frame(void *e, float f) { zax::fn<void(THISCALL *)(void *, float)>(0x48feb0)(e, f); }
// 0x48b770 with the float's bits pushed as they are (no copy through the FPU)
long double set_render_height_bits(void *e, DWORD h) { return zax::fn<long double(THISCALL *)(void *, DWORD)>(0x48b770)(e, h); }
void set_render_height(void *e, int h) { zax::fn<void(THISCALL *)(void *, int)>(0x48b6b0)(e, h); }
void layer_add_sound(void *part, DWORD sound) { zax::fn<void(THISCALL *)(void *, DWORD)>(0x4fc8a0)(part, sound); }
void add_parent_positions(void *part, float *p) { zax::fn<void(THISCALL *)(void *, float *)>(0x4fb710)(part, p); }
void add_parent_positions3(void *part, float *p) { zax::fn<void(THISCALL *)(void *, float *)>(0x4fb750)(part, p); }
void sub_parent_positions3(void *part, float *p) { zax::fn<void(THISCALL *)(void *, float *)>(0x4fb7b0)(part, p); }
BYTE *part_root(void *part) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(part); }
void part_add_child(void *part, void *child) { zax::fn<void(THISCALL *)(void *, void *)>(0x4fc710)(part, child); }
void part_add_ai(void *part, void *ai_) { zax::fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(part, ai_); }
void layer_add_part(void *layer, void *part) { zax::fn<void(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, part); }
void layer_send_sound(void *layer, DWORD sound, const float *pos) { zax::fn<void(THISCALL *)(void *, DWORD, const float *)>(0x4ea8c0)(layer, sound, pos); }
void set_target(void *e, DWORD x, DWORD y) { zax::fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x491230)(e, x, y); }
BYTE *create_from_model(DWORD model, CClassInfo *c, void *root)
{
    return zax::fn<BYTE *(THISCALL *)(void *, DWORD, CClassInfo *, void *, DWORD, DWORD)>(0x5176f0)(glob<void *>(MODEL_LIST), model, c, root, 0,
                                                                                            0xffffffff);
}
void *object_clone(DWORD o) { return zax::fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(bp(o), 0, 0xffffffff); }
// 0x55c4e0: an integer in [lo, hi] of a generator
int random_between(GlobKey gen, int lo, int hi) { return zax::fn<int(THISCALL *)(void *, int, int)>(0x55c4e0)(glob<void *>(gen), lo, hi); }
void *game_type() { return zax::fn<void *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14)); }
DWORD reset_fields(void *o) { return zax::fn<DWORD(THISCALL *)(void *)>(0x417230)(o); }
// The table functions 0x508da0 (cosine) and 0x508ec0 (sine) (stdcall, the float's bits; ai_common.h has
// the two names the other way round)
long double trig_da0(DWORD a) { return zax::fn<long double(__stdcall *)(DWORD)>(0x508da0)(a); }
long double trig_ec0(DWORD a) { return zax::fn<long double(__stdcall *)(DWORD)>(0x508ec0)(a); }
// _CIfmod 0x5d260a: st(1) modulo st(0), both popped
long double ci_fmod(long double x, long double y)
{
    long double r;
    const DWORD target = fn_addr(0x5d260a);
    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "m"(target) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}
DWORD bits_of(long double v) { return float_bits(fstp(v)); }
// An envelope's value: its start value while its speed is 0 (or NaN), else its current value (raw)
DWORD env_value(BYTE *e) { return c3(fld(fl<float>(e, 0x14)), cst(glob_addr(C_ZERO))) ? dw(e, 8) : dw(e, 0xc); }
float *velocity(void *e) { return vcall<float *>(e, 114); }

// The CAIBase and CLayerPartAI levels of every particle AI
void particle_levels_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5fbd58));   // CParticleAI
    vtable_and_free(o, zax::vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
template <DWORD VT> void particle_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    particle_levels_destroy(o);
}

// ---- CParticleAI ------------------------------------------------------------------------------------------

// 0x528550, 0x5285a0 (slot 2)
CClassInfo *__cdecl ParticleAI_ClassInfo()
{
    return class_getter(CLS_PARTICLE_AI, 0x61d7e8 /* "CParticleAI" */, 2.0f, layer_part_ai_class, 0x24, 1, CREATE_NONE, CREATE_ARRAY_NONE,
                        0x528600);
}
// 0x528600
DWORD __cdecl ParticleAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d844) /* "X Center Pos" */, 0x10, 4, 0x10, 0, 0, 0, 0, 0, 0);
    add_field(ParticleAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d834) /* "Y Center Pos" */, 0x14, 4, 0x14, 0, 0, 0, 0, 0, 0);
    add_field(ParticleAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d824) /* "Z Center Pos" */, 0x18, 4, 0x18, 0, 0, 0, 0, 0, 0);
    add_field(ParticleAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d818) /* "Animated" */, 0x1d, 1, 0x1d, 0, 0, 0);
    add_field(ParticleAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d804) /* "Obey Sequence FPS" */, 0x1e, 1, 0x1e, 0, 0, 0);
    add_field(ParticleAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d7f4) /* "Y To X Ratio" */, 0x20, 4, 0x20, 0, 0, 0, 0, 0, 0);
    return add_field(ParticleAI_ClassInfo(), f);
}
// 0x528920 (slot 1 of the abstract class: no array branch)
void *THISCALL ParticleAI_DeletingDtor(BYTE *self, DWORD flags)
{
    particle_levels_destroy(self);
    if (flags & 1)
        pool_free(self, 0x24);
    return self;
}
// 0x528960 (slot 31: part, other, -): particles of the same parent (slot 68) do not touch. al.
DWORD THISCALL ParticleAI_MayTouch(BYTE *, void *part, void *other, DWORD)
{
    if (!other)
        return 1;
    const DWORD a = vcall<DWORD>(part, 68);
    const DWORD b = vcall<DWORD>(other, 68);
    return a == b ? (b & 0xffffff00u) : 1;
}
// 0x5289a0 (slot 42): the centre. eax: its z.
DWORD THISCALL ParticleAI_SetCenter(BYTE *self, const DWORD *v)
{
    dw(self, 0x10) = v[0];
    dw(self, 0x14) = v[1];
    dw(self, 0x18) = v[2];
    return v[2];
}
// 0x5289c0 (slot 43): "Animated", "Obey Sequence FPS", follow the parent
DWORD THISCALL ParticleAI_SetFlags(BYTE *self, DWORD animated, DWORD obey, DWORD follow)
{
    fl<BYTE>(self, 0x1d) = (BYTE)animated;
    fl<BYTE>(self, 0x1e) = (BYTE)obey;
    fl<BYTE>(self, 0x1c) = (BYTE)follow;
    return follow;
}
// 0x5289e0 (slot 44): "Y To X Ratio"
DWORD THISCALL ParticleAI_SetRatio(BYTE *self, DWORD v)
{
    dw(self, 0x20) = v;
    return v;
}
// 0x5289f0 (slot 26 of CParticleAI and CFloatParticleAI): a movable entity's frame (slot 45)
DWORD THISCALL ParticleAI_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    const DWORD k = is_kind_of(e, entity_movable_class());
    if (!(k & 0xff))
        return k;
    BYTE *m = e && is_kind_of(e, entity_movable_class()) ? e : nullptr;
    return vcall<DWORD>(self, 45, m, seconds);
}
// 0x528a40 (slot 45: entity, seconds): with "Animated" a projectile's frame from its age (with "Obey
// Sequence FPS" the age times the sequence's fps modulo its frame count, else the age's share of the
// projectile's duration times the frame count); with +0x1c the centre follows the parent (slot 68): its
// position and height (slot 55). al 1.
DWORD THISCALL ParticleAI_Frame(BYTE *self, BYTE *e, DWORD)
{
    if (fl<BYTE>(self, 0x1d) && e && is_kind_of(e, projectile_class())) {
        BYTE *seq = sequence_object(e);
        const float t = fl<float>(e, 0x10c);
        const float age = fstp(projectile_duration(e) - fld(t));
        float frame;
        if (fl<BYTE>(self, 0x1e)) {
            const DWORD count = dw(seq, 0x48);
            const long double fps = vcall<long double>(seq, 26);
            frame = fstp(ci_fmod(fps * fld(age), fild64((long long)count)));
        } else {
            const DWORD count = dw(seq, 0x48);
            const float r = fstp(fild64((long long)count) * fld(age));
            frame = fstp(fld(r) / projectile_duration(e));
        }
        set_frame(e, frame);
    }
    if (fl<BYTE>(self, 0x1c))
        if (BYTE *parent = vcall<BYTE *>(e, 68)) {
            float p[2];
            part_position(parent, p);
            const long double h = vcall<long double>(parent, 55);
            float v[3] = {fstp(fld(p[0])), fstp(fld(p[1])), fstp(h)};
            vcall<void>(self, 42, v);
        }
    return 1;
}

// ---- CGravityParticleAI ------------------------------------------------------------------------------------

// 0x528b60, 0x528dd0 (slot 2)
CClassInfo *__cdecl GravityParticleAI_ClassInfo()
{
    return class_getter(CLS_GRAVITY, 0x61d854 /* "CGravityParticleAI" */, 3.0f, ParticleAI_ClassInfo, 0x34, 0, 0x528c10, 0x528c70, 0x528e90);
}
// 0x528e90
DWORD __cdecl GravityParticleAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d8c0) /* "Gravity" */, 0x24, 4, 0x24, 1, 0x411ccccd /* 9.8 */, 0, 0xc2c80000 /* -100.0 */,
                        0x42c80000 /* 100.0 */, 0);
    add_field(GravityParticleAI_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61d8b0) /* "Bounce Sound" */, 0x28, 4, 0x28, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(GravityParticleAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d890) /* "Use Velocity to Advance Frames" */, 0x2c, 1, 0x2c, 1, 0, 0);
    add_field(GravityParticleAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d87c) /* "Bounced Last Frame" */, 0x2d, 1, 0x2d, 0, 0, 0);
    add_field(GravityParticleAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d868) /* "Gravity Constant" */, 0x30, 4, 0x30, 0, 0, 0, 0, 0, 0);
    return add_field(GravityParticleAI_ClassInfo(), f);
}
// 0x528d10 (slot 1)
void *THISCALL GravityParticleAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, particle_destroy<0x5fbe10>>(self, flags); }
// 0x5292c0 (slot 23: entity): "Gravity Constant" = "Gravity" times the double 0x5fbed0; a movable entity
// accelerates by (0, 0, -constant) (slot 118)
void THISCALL GravityParticleAI_Begin(BYTE *self, BYTE *e)
{
    if (!e || !is_kind_of(e, entity_movable_class()))
        return;
    const long double g = fld(fl<float>(self, 0x24)) * fld64(glob<const void *>(0x5fbed0));
    fl<float>(self, 0x30) = fstp(g);
    float a[3] = {raw_float(0), raw_float(0), fstp(-g)};
    vcall<void>(e, 118, a);
}
// 0x529320 (slot 45): with "Use Velocity to Advance Frames" a projectile advances its frame by its
// speed (the length of the velocity's x and y) times the seconds over the sequence's fps; else
// CParticleAI's slot 45. al 1.
DWORD THISCALL GravityParticleAI_Frame(BYTE *self, BYTE *e, DWORD seconds)
{
    if (!fl<BYTE>(self, 0x2c))
        return zax::fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x528a40)(self, e, seconds);
    if (e && is_kind_of(e, projectile_class())) {
        BYTE *seq = sequence_object(e);
        const float *v1 = velocity(e);
        const float *v2 = velocity(e);
        const long double y = fld(v1[1]), x = fld(v2[0]);
        const float d = fstp(fsqrt(y * y + x * x) * fld(raw_float(seconds)));
        const float step = fstp(fld(d) / vcall<long double>(seq, 26));
        set_frame(e, fstp(vcall<long double>(e, 84) + fld(step)));
    }
    return 1;
}
// 0x5293d0 (slot 26: entity, seconds): a movable entity accelerates by (0, 0, -"Gravity Constant")
void THISCALL GravityParticleAI_Update(BYTE *self, BYTE *e, DWORD)
{
    if (!e || !is_kind_of(e, entity_movable_class()))
        return;
    float a[3] = {raw_float(0), raw_float(0), fstp(-fld(fl<float>(self, 0x30)))};
    vcall<void>(e, 118, a);
}
// 0x52a200 (thiscall on a vector): minus another
void THISCALL Vec3_Sub(float *a, const float *b)
{
    a[0] = fstp(fld(a[0]) - fld(b[0]));
    a[1] = fstp(fld(a[1]) - fld(b[1]));
    a[2] = fstp(fld(a[2]) - fld(b[2]));
}
// 0x529420 (slot 46: seconds, entity, height below the ground, a4): the bounce. Falling (velocity z
// below 0), the velocity loses its part along the normal (0, 0, 1) and is scaled by 1 - the model's
// +0x38 (bounciness); below the model's +0x3c (squared length) it stops; when the normal part is longer
// than the model's +0x40, the normal part times the model's +0x34 is taken off once more (0x52a200). The
// height becomes 2 · a4 - the depth, and the rise is limited to the velocity's z times the seconds.
// "Bounced Last Frame" = 1.
//
// Every operation in the original's form and operand roles, every float in memory written by fst or a
// mov as there (the m32 helpers below): with a NaN operand the x87 emulation of the VMs (Windows on ARM)
// decides by the form which payload comes out (docs/DECOMPILATION.md 12, "The three flaky checks"); with
// plain C++ clang took register forms with other destinations and copied floats through the FPU, and a
// NaN payload of the original came out as the default NaN 0x7fc00000.
template <class T> long double ld_m(const T &m)   // fld m32
{
    long double r;
    __asm__("flds %1" : "=t"(r) : "m"(m));
    return r;
}
template <class T> void fst_m(T &m, long double v)   // fst m32 (fstp m32: the value is not used again)
{
    __asm__ volatile("fsts %0" : "=m"(m) : "t"(v));
}
template <class T> long double mul_m(long double st, const T &m)   // fmul m32
{
    __asm__("fmuls %1" : "+t"(st) : "m"(m));
    return st;
}
template <class T> long double sub_m(long double st, const T &m)   // fsub m32: st - m
{
    __asm__("fsubs %1" : "+t"(st) : "m"(m));
    return st;
}
template <class T> long double subr_m(long double st, const T &m)   // fsubr m32: m - st
{
    __asm__("fsubrs %1" : "+t"(st) : "m"(m));
    return st;
}
template <class T> long double add_m(long double st, const T &m)   // fadd m32
{
    __asm__("fadds %1" : "+t"(st) : "m"(m));
    return st;
}
long double sub_r(long double d, long double s)   // fsub st, st(1): d - s, d the destination
{
    __asm__("fsub %%st(1), %%st" : "+t"(d) : "u"(s));
    return d;
}
long double mul_r(long double d, long double s)   // fld st(i) and fmul st, st(j): d · s, d the destination
{
    __asm__("fmul %%st(1), %%st" : "+t"(d) : "u"(s));
    return d;
}
long double addp(long double d, long double s)   // faddp st(1), st: d + s, d (st(1)) the destination
{
    long double r;
    __asm__("faddp %%st, %%st(1)" : "=t"(r) : "0"(s), "u"(d) : "st(1)");
    return r;
}
long double twice(long double v)   // fadd st(0), st(0)
{
    __asm__("fadd %%st(0), %%st" : "+t"(v));
    return v;
}
void THISCALL GravityParticleAI_Bounce(BYTE *self, DWORD seconds, BYTE *e, DWORD depth, DWORD a4)
{
    DWORD h0, before;
    fst_m(h0, vcall<long double>(e, 55));
    fst_m(before, subr_m(mul_m(ld_m(velocity(e)[2]), seconds), h0));
    const DWORD normal[3] = {0, 0, 0x3f800000};
    BYTE *m = entity_model(e);
    const DWORD m3c = dw(m, 0x3c), m38 = dw(m, 0x38), m40 = dw(m, 0x40), m34 = dw(m, 0x34);
    float *v = velocity(e);
    DWORD vz;
    const long double vz_st = ld_m(v[2]);
    fst_m(vz, vz_st);
    if (c0(vz_st, cst(glob_addr(C_ZERO)))) {
        const DWORD n1 = normal[1], n2 = normal[2];
        const long double nx = ld_m(*glob<const float *>(C_ZERO));
        DWORD ny, nz, y, z, keep, b[3];
        fst_m(ny, mul_m(ld_m(n1), vz));
        fst_m(nz, mul_m(ld_m(n2), vz));
        const long double x = sub_r(ld_m(v[0]), nx);
        fst_m(v[0], x);
        const long double y_st = sub_m(ld_m(v[1]), ny);
        fst_m(y, y_st);
        fst_m(v[1], y_st);
        const long double z_st = sub_m(ld_m(v[2]), nz);
        fst_m(z, z_st);
        fst_m(v[2], z_st);
        fst_m(keep, sub_m(ld_m(*glob<const float *>(C_ONE)), m38));
        const long double x2 = mul_m(x, keep);
        fst_m(v[0], x2);
        const long double y2 = mul_m(ld_m(y), keep);
        fst_m(v[1], y2);
        const long double z2 = mul_m(ld_m(z), keep);
        fst_m(v[2], z2);
        const long double len2 = addp(addp(mul_r(z2, z2), mul_r(y2, y2)), mul_r(x2, x2));
        if (!c03(mul_m(ld_m(m3c), m3c), len2)) {
            dw(v, 0) = 0;
            dw(v, 4) = 0;
            dw(v, 8) = 0;
        }
        fst_m(b[0], mul_m(nx, m34));
        fst_m(b[1], mul_m(ld_m(ny), m34));
        const long double bz = mul_m(ld_m(nz), m34);
        fst_m(b[2], bz);
        const long double blen2 = addp(addp(mul_m(bz, b[2]), mul_m(ld_m(b[1]), b[1])), mul_m(ld_m(b[0]), b[0]));
        if (c0(mul_m(ld_m(m40), m40), blen2))
            zax::fn<void(THISCALL *)(float *, const DWORD *)>(0x52a200)(v, b);
    }
    DWORD h;
    fst_m(h, sub_m(twice(ld_m(a4)), depth));
    set_render_height_bits(e, h);
    DWORD fall, rise;
    fst_m(fall, subr_m(vcall<long double>(e, 55), before));
    fall &= 0x7fffffffu;
    fst_m(rise, ld_m(fall));
    if (c0(mul_m(ld_m(velocity(e)[2]), seconds), ld_m(rise))) {
        fst_m(h, add_m(mul_m(ld_m(velocity(e)[2]), seconds), before));
        set_render_height_bits(e, h);
    }
    fl<BYTE>(self, 0x2d) = 1;
}
// 0x529600 (slot 25: entity, seconds): an active (flag 0x80000) movable entity whose frame step (slot 45)
// succeeds bounces (slot 46) while below the ground (slot 55 below 0); above it, after a bounce the
// "Bounce Sound" plays (CLayerPart_LayerAdd) and "Bounced Last Frame" is cleared
void THISCALL GravityParticleAI_Collide(BYTE *self, BYTE *e, DWORD seconds)
{
    if (!e || !is_kind_of(e, entity_movable_class()) || !(dw(e, 0x1c) & 0x80000))
        return;
    if (!(vcall<DWORD>(self, 45, e, seconds) & 0xff))
        return;
    const long double h = vcall<long double>(e, 55);
    const float hf = fstp(h);
    if (c0(h, cst(glob_addr(C_ZERO)))) {
        vcall<void>(self, 46, seconds, e, float_bits(hf), 0);
        return;
    }
    if (fl<BYTE>(self, 0x2d)) {
        if (dw(self, 0x28))
            layer_add_sound(e, dw(self, 0x28));
        fl<BYTE>(self, 0x2d) = 0;
    }
}

// ---- CFloatParticleAI --------------------------------------------------------------------------------------

// 0x529690, 0x529900 (slot 2)
CClassInfo *__cdecl FloatParticleAI_ClassInfo()
{
    return class_getter(CLS_FLOAT, 0x61d8c8 /* "CFloatParticleAI" */, 2.0f, ParticleAI_ClassInfo, 0x24, 0, 0x529740, 0x5297a0, NO_FIELDS);
}
// 0x529840 (slot 1)
void *THISCALL FloatParticleAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, particle_destroy<0x5fbed8>>(self, flags); }

// ---- CSwirlParticleAI --------------------------------------------------------------------------------------

// 0x5299c0, 0x529c50 (slot 2)
CClassInfo *__cdecl SwirlParticleAI_ClassInfo()
{
    return class_getter(CLS_SWIRL, 0x61d8dc /* "CSwirlParticleAI" */, 1.0f, ParticleAI_ClassInfo, 0x34, 0, 0x529a90, 0x529af0, 0x529d10);
}
// 0x529d10
DWORD __cdecl SwirlParticleAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61d948) /* "Angular Velocity" */, glob_addr(0x61d95c) /* "Radians/Sec" */, 0x24, 4, 0x24, 1,
                        0x3f860a92 /* 1.0471976 */, 0, 0xc0c90fdb /* -6.2831855 */, 0x40c90fdb /* 6.2831855 */, 0);
    add_field(SwirlParticleAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x61d93c) /* "Degrees/Seg" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61d920) /* "Angular Velocity Variance" */, glob_addr(0x61d95c) /* "Radians/Sec" */, 0x28, 4, 0x28, 1, 0,
                  0, 0xc0c90fdb /* -6.2831855 */, 0x40c90fdb /* 6.2831855 */, 0);
    add_field(SwirlParticleAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x61d93c) /* "Degrees/Seg" */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d910) /* "Rotate Velocity" */, 0x2c, 1, 0x2c, 1, 0, 0);
    add_field(SwirlParticleAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61d8f0) /* "Debug/Current Angular Velocity" */, glob_addr(0x61d95c) /* "Radians/Sec" */, 0x30, 4, 0x30, 3,
                  0, 0, 0, 0, 0);
    add_field(SwirlParticleAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    return zax::fn<DWORD(THISCALL *)(void *, const char *)>(0x4e1930)(zstring(f, 0x20), glob<const char *>(0x61d93c) /* "Degrees/Seg" */);
}
// 0x529b90 (slot 1)
void *THISCALL SwirlParticleAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, particle_destroy<0x5fbf90>>(self, flags); }
// 0x529a70 (slot 19): the four fields cleared, then CObject_ResetFields
DWORD THISCALL SwirlParticleAI_Reset(BYTE *self)
{
    dw(self, 0x24) = 0;
    dw(self, 0x28) = 0;
    fl<BYTE>(self, 0x2c) = 0;
    dw(self, 0x30) = 0;
    return reset_fields(self);
}
// 0x52a070 (slot 23: entity): "Current Angular Velocity" = "Angular Velocity" ± a random part of the
// variance (generator 0x712cc0)
void THISCALL SwirlParticleAI_Begin(BYTE *self, BYTE *)
{
    const DWORD a = dw(self, 0x28) & 0x7fffffffu, b = dw(self, 0x28) & 0x7fffffffu;
    const DWORD hi = fpu_float(raw_float(a));
    const DWORD lo = bits_of(-fld(raw_float(b)));
    fl<float>(self, 0x30) = fstp(random_range(RANDOM3, lo, hi) + fld(fl<float>(self, 0x24)));
}
// 0x52a0d0 (slot 26: entity, seconds): a movable entity whose frame step (slot 45) succeeds turns around
// the centre by "Current Angular Velocity" times the seconds in the plane with y stretched by 1 / (ratio ·
// 0.712766) (slot 41); with "Rotate Velocity" its velocity is set again (slot 107)
void THISCALL SwirlParticleAI_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    if (!e || !is_kind_of(e, entity_movable_class()))
        return;
    if (!(vcall<DWORD>(self, 45, e, seconds) & 0xff))
        return;
    float p[5];
    vcall<void>(e, 43, p);
    const float cx = fl<float>(self, 0x10), cy = fl<float>(self, 0x14), ratio = fl<float>(self, 0x20);
    p[1] = fstp((fld(p[1]) - fld(cy)) / (fld(ratio) * cst(glob_addr(C_OVAL_RATIO))) + fld(cy));
    const DWORD angle = bits_of(fld(raw_float(seconds)) * fld(fl<float>(self, 0x30)));
    const float c = fstp(trig_ec0(angle));
    const long double s = trig_da0(angle);
    const long double dx = fld(p[0]) - fld(cx), dy = fld(p[1]) - fld(cy);
    const float ny = fstp(dx * fld(c) + dy * s);
    p[0] = fstp(dx * s - dy * fld(c) + fld(cx));
    p[1] = fstp(fld(ny) + fld(cy));
    const float y = fstp((fld(p[1]) - fld(cy)) * fld(ratio) * cst(glob_addr(C_OVAL_RATIO)) + fld(cy));
    vcall<void>(e, 41, float_bits(p[0]), float_bits(y));
    if (fl<BYTE>(self, 0x2c)) {
        const DWORD *v = reinterpret_cast<const DWORD *>(velocity(e));
        DWORD copy[3] = {v[0], v[1], v[2]};
        vcall<void>(e, 107, copy);
    }
}

// ---- CParticleSystemAI -------------------------------------------------------------------------------------

// 0x52dc20, 0x52dc90 (slot 2)
CClassInfo *__cdecl ParticleSystemAI_ClassInfo()
{
    return class_getter(CLS_SYSTEM, 0x61db20 /* "CParticleSystemAI" */, 4.0f, layer_part_ai_class, 0x134, 1, CREATE_NONE,
                        CREATE_ARRAY_NONE, NO_FIELDS);
}
// One embedded envelope's destructor: its class's level, CEnvelope's, CObject's
void envelope_destroy(BYTE *e, DWORD vt)
{
    vtable_and_free(e, vt);
    vtable_and_free(e, zax::vt(0x5f3cbc));
    obj_dtor(e);
}
// A pointer array's destructor (the exe's inline ZPtrArray: data back to the pool, then the count)
void ptr_array_destroy(BYTE *a, DWORD vt)
{
    dw(a, 0) = vt;
    if (dw(a, 4)) {
        pool_get_free(bp(dw(a, 4)), dw(a, 0xc) << 2);
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
}
// 0x52ded0: the destructor below CGenericParticleSystemAI (the arrays freed, not their objects; the
// envelopes from the last; CLayerPartAI, CAIBase)
void THISCALL ParticleSystemAI_Dtor(BYTE *self)
{
    dw(self, 0) = zax::vt(0x5fc470);
    obj_free_fields(self);
    ptr_array_destroy(self + 0x124, zax::vt(0x5ee15c));
    ptr_array_destroy(self + 0x114, zax::vt(0x5fc540));
    envelope_destroy(self + 0xd0, zax::vt(0x5f3d24));
    envelope_destroy(self + 0xa8, zax::vt(0x5f3df4));
    envelope_destroy(self + 0x7c, zax::vt(0x5f3e5c));
    envelope_destroy(self + 0x58, zax::vt(0x5f3ec4));
    envelope_destroy(self + 0x34, zax::vt(0x5f3d8c));
    envelope_destroy(self + 0x10, zax::vt(0x5f3d8c));
    vtable_and_free(self, zax::vt(0x5ed1d0));
    ai_base_dtor(self);
}
// 0x52dea0 (slot 1 of the abstract class)
void *THISCALL ParticleSystemAI_DeletingDtor(BYTE *self, DWORD flags)
{
    zax::fn<void(THISCALL *)(void *)>(0x52ded0)(self);
    if (flags & 1)
        pool_free(self, 0x134);
    return self;
}
// 0x52e050 (slot 19): CObject_ResetFields, then "NumBursts" 1, "Enabled" 1, no bursts yet
DWORD THISCALL ParticleSystemAI_Reset(BYTE *self)
{
    const DWORD r = reset_fields(self);
    dw(self, 0x100) = 0x3f800000;
    fl<BYTE>(self, 0x10a) = 1;
    dw(self, 0x10c) = 0;
    return r;
}
// 0x52e080 (part, out, angle, own): the emitter's position: the part's position with its parents'
// (CLayerPart_AddParentPositions); as a child of the generator ("Make Child of Generator" and not
// `own`) only the offset to the part's slot 43 position. Plus the offsets (slots 42, 43, 44, as
// integers); without the child case an entity base adds its height (slot 54). With "Orient To Parent's
// Rotation" (slot 47) x and y turn by the part's facing (slot 46) + the angle.
void THISCALL ParticleSystemAI_Origin(BYTE *self, BYTE *part, float *out, DWORD angle, DWORD own)
{
    float base[2];
    const bool child = fl<BYTE>(self, 0x108) && !(BYTE)own;
    if (child) {
        base[0] = raw_float(0);
        base[1] = raw_float(0);
        float p[2], q[2];
        part_position(part, p);
        add_parent_positions(part, p);
        vcall<void>(part, 43, q);
        base[0] = fstp(fld(p[0]) - fld(q[0]));
        base[1] = fstp(fld(p[1]) - fld(q[1]));
    } else {
        part_position(part, base);
        add_parent_positions(part, base);
    }
    out[0] = fstp(fild(vcall<int>(self, 42)));
    out[1] = fstp(fild(vcall<int>(self, 43)));
    out[2] = fstp(fild(vcall<int>(self, 44)));
    if (!child && is_kind_of(part, entity_base_class()))
        out[2] = fstp(fild(vcall<int>(part, 54)) + fld(out[2]));
    if (vcall<DWORD>(self, 47) & 0xff) {
        const long double facing = vcall<long double>(part, 46);
        const DWORD a = bits_of(facing + fld(raw_float(angle)) * cst(glob_addr(C_DEG)));
        const float x = out[0], y = out[1];
        const float t = fstp(trig_da0(a) * fld(x));
        out[0] = fstp(fld(t) - trig_ec0(a) * fld(y));
        const float u = fstp(trig_da0(a) * fld(y));
        out[1] = fstp(trig_ec0(a) * fld(x) + fld(u));
    }
    out[0] = fstp(fld(base[0]) + fld(out[0]));
    out[1] = fstp(fld(base[1]) + fld(out[1]));
}
// 0x52eb40 (angle, out): a direction in the cone of the angle: a random turn r in [-pi, pi]
// (generator 0x712cc0) around the axis; out = (cos a, sin a · sin r, -sin a · cos r) with the table
// functions 0x508da0 (cosine) and 0x508ec0 (sine) (every table call of the exe made, also the unused ones)
void THISCALL ParticleSystemAI_Direction(BYTE *, DWORD angle, float *out)
{
    trig_ec0(angle);
    const float fa = fstp(trig_da0(angle));
    trig_da0(angle);
    const float ga = fstp(-trig_ec0(angle));
    const DWORD r = bits_of(random_range(RANDOM3, 0xc0490fdb, 0x40490fdb));
    trig_da0(r);
    const float y = fstp(-(trig_ec0(r) * fld(ga)));
    trig_ec0(r);
    const long double z = trig_da0(r) * fld(ga);
    out[0] = fa;
    dw(out, 4) = float_bits(y);
    out[2] = fstp(z);
}
// 0x52ebd0 (part, in, out, face angle, trajectory): `in` turned by the trajectory (· 0x5fc550) around
// one axis and by the face angle (· 0x5fc54c; with "Orient To Parent's Rotation" plus the part's facing,
// slot 46) around the other
void THISCALL ParticleSystemAI_Turn(BYTE *self, BYTE *part, const float *in, float *out, DWORD face, DWORD trajectory)
{
    const DWORD pitch = bits_of(fld(raw_float(trajectory)) * cst(glob_addr(C_DEG2)));
    float yaw = fstp(fld(raw_float(face)) * cst(glob_addr(C_DEG)));
    if (vcall<DWORD>(self, 47) & 0xff)
        yaw = fstp(vcall<long double>(part, 46) + fld(yaw));
    const DWORD yb = float_bits(yaw);
    const float vx = in[0], vy = in[1], vz = in[2];
    const float t1 = fstp(trig_ec0(pitch) * fld(vz));
    const float f1 = fstp(trig_da0(pitch) * fld(vx) + fld(t1));
    const float t3 = fstp(trig_da0(pitch) * fld(vz));
    const float z = fstp(fld(t3) - trig_ec0(pitch) * fld(vx));
    const float t4 = fstp(trig_da0(yb) * fld(f1));
    const float x = fstp(fld(t4) - trig_ec0(yb) * fld(vy));
    const float t5 = fstp(trig_ec0(yb) * fld(f1));
    const long double y = trig_da0(yb) * fld(vy) + fld(t5);
    out[0] = fstp(fld(x));
    out[1] = fstp(y);
    out[2] = fstp(fld(z));
}

// ---- CGenericParticleSystemAI -------------------------------------------------------------------------------

// 0x52ef30, 0x52f160 (slot 2)
CClassInfo *__cdecl GenericParticleSystemAI_ClassInfo()
{
    return class_getter(CLS_GENERIC, 0x61db5c /* "CGenericParticleSystemAI" */, 13.0f, ParticleSystemAI_ClassInfo, 0x148, 0, 0x52f040,
                        0x52f070, 0x52f220);
}
// 0x52f220
DWORD __cdecl GenericParticleSystemAI_RegisterFields()
{
    constexpr DWORD EMBEDDED_FIELD_CTOR_VALUE = 0x5284c0;
    void *f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61de44) /* "Outer Cone Angle" */, 0x10, 0x24, 0x10, class_at(fn_addr(0x49e130)),
                        glob_addr(0x613390) /* "Start Value" */, 1);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61de30) /* "Inner Cone Angle" */, 0x34, 0x24, 0x34, class_at(fn_addr(0x49e130)),
                  glob_addr(0x613390) /* "Start Value" */, 1);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61de24) /* "Face Angle" */, 0x58, 0x24, 0x58, class_at(fn_addr(0x49efb0)), glob_addr(0x613390) /* "Start Value" */,
                  1);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61de14) /* "Cone Trajectory" */, 0x7c, 0x24, 0x7c, class_at(fn_addr(0x49ea90)),
                  glob_addr(0x613390) /* "Start Value" */, 1);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61ddf8) /* "Burst Frequency" */, glob_addr(0x61de08) /* "Bursts/Sec" */, 0xa0, 4, 0xa0, 1,
                  0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61dddc) /* "Burst Frequency Variance" */, glob_addr(0x61de08) /* "Bursts/Sec" */, 0xa4, 4, 0xa4, 1, 0, 0,
                  0, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    set_alt_name(last_field(GenericParticleSystemAI_ClassInfo()), glob_addr(0x60b28c) /* "Plus or Minus" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ddb8) /* "Num Bursts To Stop After" */, glob_addr(0x61ddd4) /* "Bursts" */, 0x140, 4, 0x140, 1, 0, 0,
                  1, 0xc350);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    zax::fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54da60)(f, 0, glob<const char *>(0x61ddac) /* "Don't Stop" */);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61dd98) /* "Start Point Type" */, 0x110, 4, 0x110, class_at(fn_addr(0x52aed0)), 1, class_at(fn_addr(0x52b1d0)), 0,
                  0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61dd7c) /* "Orient To Parent's Rotation" */, 0x145, 1, 0x145, 1, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x61dd68) /* "Position/X Offset" */, 0x134, 4, 0x134, 1, 0, 0, 0xfffffe0c, 0x1f4);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x61dd54) /* "Position/Y Offset" */, 0x138, 4, 0x138, 1, 0, 0, 0xfffffe0c, 0x1f4);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x6131c0) /* "Position/Height" */, 0x13c, 4, 0x13c, 1, 0, 0, 0xfffffe0c, 0x1f4);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61dd3c) /* "Particles Params/Speed" */, 0xa8, 0x24, 0xa8, class_at(fn_addr(0x49e650)),
                  glob_addr(0x613390) /* "Start Value" */, 1);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61dd10) /* "Particles Params/Speed Variance" */, glob_addr(0x61dd30) /* "Pixels/Sec" */, 0xcc, 4, 0xcc, 1,
                  0, 0, 0, 0x447a0000 /* 1000.0 */, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61dcf8) /* "Particles Params/Age" */, 0xd0, 0x24, 0xd0, class_at(fn_addr(0x49dcf0)),
                  glob_addr(0x613390) /* "Start Value" */, 1);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61dcd8) /* "Particles Params/Age Variance" */, glob_addr(0x60b000) /* "Seconds" */, 0xf4, 4, 0xf4, 1, 0, 0,
                  0, 0x42c80000 /* 100.0 */, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61dcb0) /* "Particles Params/Max Travel Distance" */, glob_addr(0x60b360) /* "Pixels" */, 0xf8, 4, 0xf8, 1,
                  0x497423f0 /* 999999.0 */, 0, 0, 0x497423f0 /* 999999.0 */, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61dc88) /* "Particles Params/Min Travel Distance" */, glob_addr(0x60b360) /* "Pixels" */, 0xfc, 4, 0xfc, 1,
                  0, 0, 0, 0x497423f0 /* 999999.0 */, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61dc68) /* "Particles/Use Particle As Shot" */, 0x144, 1, 0x144, 1, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61dc44) /* "Particles/Make Child of Generator" */, 0x108, 1, 0x108, 1, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61dc18) /* "Particles/Limit Duration To First Collision" */, 0x109, 1, 0x109, 1, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    void *element = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61dc0c) /* "Particles" */, 0, 4, 0, class_at(fn_addr(0x52a860)), 1, class_at(fn_addr(0x52a860)), 0, 0);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x61dbf8) /* "Particles/Particles" */, 0x11c, 4, 0x11c, 0x120, 4, 0x120, 0x118, 4, 0x118, 1,
                  element, 1, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61dbec) /* "NumBursts" */, 0x100, 4, 0x100, 0, 0, 0, 0, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61dbcc) /* "CurFrequency" */, glob_addr(0x61dbdc) /* "Particles/Sec" */, 0x104, 4, 0x104, 0, 0, 0, 0, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61dbc4) /* "Enabled" */, 0x10a, 1, 0x10a, 1, 1, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61dbac) /* "Disable in multi-player" */, 0x10b, 1, 0x10b, 1, 1, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61db88) /* "Sounds/Choose Sound Based On Burst" */, 0x146, 1, 0x146, 1, 0, 0);
    add_field(GenericParticleSystemAI_ClassInfo(), f);
    void *element2 = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x615650) /* "Sounds" */, 0, 4, 0, glob_addr(0x713708), 1, 0, 0, 0);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x61db78) /* "Sounds/Sounds" */, 0x12c, 4, 0x12c, 0x130, 4, 0x130, 0x128, 4, 0x128, 1, element2,
                  1, 0);
    return add_field(GenericParticleSystemAI_ClassInfo(), f);
}
// 0x52f0d0 (slot 1)
void Generic_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5fc554));
    zax::fn<void(THISCALL *)(void *)>(0x52ded0)(o);
}
void *THISCALL GenericParticleSystemAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x148, Generic_destroy>(self, flags); }
// 0x52efe0, 0x52eff0, 0x52f000 (slots 42, 44, 45), 0x52f010, 0x52f020, 0x52f030 (slots 46, 47, 48)
DWORD THISCALL Generic_XOffset(BYTE *self) { return dw(self, 0x134); }
DWORD THISCALL Generic_Height(BYTE *self) { return dw(self, 0x13c); }
DWORD THISCALL Generic_StopAfter(BYTE *self) { return dw(self, 0x140); }
DWORD THISCALL Generic_AsShot(BYTE *self) { return fl<BYTE>(self, 0x144); }
DWORD THISCALL Generic_Orient(BYTE *self) { return fl<BYTE>(self, 0x145); }
DWORD THISCALL Generic_SoundByBurst(BYTE *self) { return fl<BYTE>(self, 0x146); }
// 0x530800 (slot 19): the start point type deleted (slot 1), "Use Particle As Shot" cleared, then
// CObject_ResetFields
DWORD THISCALL Generic_Reset(BYTE *self)
{
    if (BYTE *t = bp(dw(self, 0x110))) {
        vcall<void *>(t, 1, 1);
        dw(self, 0x110) = 0;
    }
    fl<BYTE>(self, 0x144) = 0;
    return reset_fields(self);
}
constexpr DWORD OLD_LINES[8][2] = {{0x61dec8, 0x18}, {0x61deb4, 0x3c}, {0x60b2dc, 0x60}, {0x61dea8, 0x84},
                                   {0x61de98, 0xb0}, {0x61de88, 0xd8}, {0x61de70, 0xcc}, {0x61de58, 0xf4}};
// 0x530690 (slot 11: name, value, a3): the old lines "Cone Angle", "Cone Angle Spread", "Facing Angle",
// "Trajectory", "Particle Speed", "Particle Age" (the envelopes' start values), "Particle Speed
// Variance", "Particle Age Variance" (atof), else CObject_ReadOwnLine
DWORD THISCALL Generic_ReadLine(BYTE *self, const char *name, const char *value, DWORD a3)
{
    for (const auto &n : OLD_LINES)
        if (!crt_stricmp(name, glob_dyn<const char *>(n[0]))) {
            fl<float>(self, n[1]) = fstp(zax::fn<long double(__cdecl *)(const char *)>(0x5d03de)(value));
            return 0;
        }
    return zax::fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x417500)(self, name, value, a3);
}
// 0x52e220 (slot 24: part): "CurFrequency" = "Burst Frequency" ± a random part of its variance,
// "NumBursts" random in [0, 1) (generator 0x712cc0), no bursts yet, the envelopes reset (slot 24) and the
// start point type begun (slot 28)
void THISCALL Generic_Begin(BYTE *self, void *part)
{
    const DWORD var = dw(self, 0xa4);
    const DWORD lo = bits_of(-fld(fl<float>(self, 0xa4)));
    const long double f = random_range(RANDOM3, lo, var) + fld(fl<float>(self, 0xa0));
    fl<float>(self, 0x104) = fstp(f);
    fl<float>(self, 0x100) = fstp(random_range(RANDOM3, 0, 0x3f800000));
    dw(self, 0x10c) = 0;
    for (DWORD e : ENVELOPES)
        vcall<void>(self + e, 24);
    if (BYTE *t = bp(dw(self, 0x110)))
        vcall<void>(t, 28, part);
}
// 0x52eef0 (slot 29: part, message): "Particle System On" enables and begins again (slot 24), "Particle
// System Off" disables. al: handled.
DWORD THISCALL Generic_Message(BYTE *self, void *part, const DWORD *msg)
{
    const DWORD id = msg[1];
    if (id == *glob<DWORD *>(MSG_ON)) {
        fl<BYTE>(self, 0x10a) = 1;
        vcall<void>(self, 24, part);
        return 1;
    }
    if (id == *glob<DWORD *>(MSG_OFF)) {
        fl<BYTE>(self, 0x10a) = 0;
        return (id & 0xffffff00u) | 1;
    }
    return id & 0xffffff00u;
}
// 0x52e2d0 (slot 26: part, seconds): the emission. Unless disabled (also all systems, 0x6d04f8, or in
// multi-player with "Disable in multi-player" when the game type's slot 33 says so) the envelopes and
// the start point type advance (slots 23, 27); "NumBursts" grows by "CurFrequency" times the seconds (a
// zero frequency is drawn anew first). From 1 on, every whole burst (at most up to "Num Bursts To Stop
// After", which then disables) emits per particle info "Num Particles" ± variance (game generator)
// projectiles of the info's model (the model list's 0x5176f0, as children of the part's root with "Use
// Particle As Shot"): the speed (envelope ± variance), the age (envelope ± variance, kept between the min
// and max travel distance over the speed, at least 0.01), a direction in the cone (0x52eb40), the start
// point (slot 25 of the start point type) turned by face angle and trajectory (0x52ebd0), placed at the
// emitter (0x52e080, y scaled by 0.712766), flags, collidable/tries to collide, added to the part or the
// layer, the velocity set, with "Limit Duration To First Collision" a sweep through the layer and the age
// 0.1; the info's AI is cloned onto the particle (centre, flags, the start point type's ratio, slot 26).
// After the bursts a new frequency is drawn and one of "Sounds" plays (by the share of particles emitted
// or at random).
void THISCALL Generic_Update(BYTE *self, BYTE *part, DWORD seconds)
{
    if (!fl<BYTE>(self, 0x10a) || *glob<BYTE *>(G_SYSTEMS_OFF))
        return;
    if (fl<BYTE>(self, 0x10b) && game_type())
        if (vcall<DWORD>(game_type(), 33) & 0xff)
            return;
    for (DWORD e : ENVELOPES)
        vcall<void>(self + e, 23, seconds);
    if (BYTE *t = bp(dw(self, 0x110)))
        vcall<void>(t, 27, seconds);
    const float sec = raw_float(seconds);
    if (c3(fld(fl<float>(self, 0x104)), cst(glob_addr(C_ZERO)))) {
        const DWORD var = dw(self, 0xa4);
        const DWORD lo = bits_of(-fld(fl<float>(self, 0xa4)));
        const long double f = random_range(RANDOM3, lo, var) + fld(fl<float>(self, 0xa0));
        fl<float>(self, 0x104) = fstp(f);
        fl<float>(self, 0x100) = fstp(cst(glob_addr(C_ONE)) - f * fld(sec));
    }
    const long double due = fld(sec) * fld(fl<float>(self, 0x104)) + fld(fl<float>(self, 0x100));
    fl<float>(self, 0x100) = fstp(due);
    if (c0(due, cst(glob_addr(C_ONE))) || !dw(self, 0x11c))
        return;
    DWORD spawned = 0, possible = 0;
    float pos[2], base[2];
    part_position(part, pos);
    dw(base, 0) = dw(pos, 0);
    dw(base, 4) = dw(pos, 4);
    add_parent_positions(part, base);
    float origin[3];
    zax::fn<void(THISCALL *)(void *, void *, float *, DWORD, DWORD)>(0x52e080)(self, part, origin, env_value(self + 0x58), 0);
    sub_parent_positions3(part, origin);
    const int n = x87_fistp(fld(fl<float>(self, 0x100)));
    dw(self, 0x10c) += (DWORD)n;
    fl<float>(self, 0x100) = fstp(fld(fl<float>(self, 0x100)) - fild(n));
    int bursts = n;
    if (vcall<int>(self, 45) && vcall<int>(self, 45) <= (int)dw(self, 0x10c)) {
        const int stop = vcall<int>(self, 45);
        fl<BYTE>(self, 0x10a) = 0;
        bursts = stop + (n - (int)dw(self, 0x10c));
    }
    if (bursts > 0)
        for (int b = bursts; b; b--)
            for (DWORD i = 0; i < dw(self, 0x11c); i++) {
                BYTE *info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                const int k = random_between(GAME_RANDOM, -(int)dw(info, 0x1c), (int)dw(info, 0x1c));
                int count = k + (int)dw(info, 0x18);
                if (count < 0)
                    count = 0;
                spawned += (DWORD)count;
                info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                possible += dw(info, 0x1c) + dw(info, 0x18);
                for (int j = count; j > 0; j--) {
                    BYTE *root = (vcall<DWORD>(self, 46) & 0xff) ? part_root(part) : nullptr;
                    info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                    BYTE *e = create_from_model(dw(info, 8), projectile_class(), root);
                    if (e && !is_kind_of(e, projectile_class())) {
                        vcall<void *>(e, 1, 1);
                        e = nullptr;
                    }
                    const DWORD inner = env_value(self + 0x34), outer = env_value(self + 0x10);
                    const DWORD cone = bits_of((random_range(RANDOM3, 0, outer) + fld(raw_float(inner))) * cst(glob_addr(C_DEG)));
                    const DWORD speed_value = env_value(self + 0xa8);
                    const DWORD sv = dw(self, 0xcc);
                    const float speed = fstp(random_range(RANDOM3, bits_of(-fld(fl<float>(self, 0xcc))), sv) + fld(raw_float(speed_value)));
                    const DWORD age_value = env_value(self + 0xd0);
                    const DWORD av = dw(self, 0xf4);
                    const long double age_unr = random_range(RANDOM3, bits_of(-fld(fl<float>(self, 0xf4))), av) + fld(raw_float(age_value));
                    float age = fstp(age_unr);
                    if (!c03(age_unr * fld(speed), fld(fl<float>(self, 0xf8))))
                        age = fstp(fld(fl<float>(self, 0xf8)) / fld(speed));
                    if (c0(fld(age) * fld(speed), fld(fl<float>(self, 0xfc))))
                        age = fstp(fld(fl<float>(self, 0xfc)) / fld(speed));
                    if (c03(fld(age), cst(glob_addr(C_ZERO))))
                        age = raw_float(0x3c23d70a);
                    float dir[3], start[3];
                    zax::fn<void(THISCALL *)(void *, DWORD, float *)>(0x52eb40)(self, cone, dir);
                    if (BYTE *t = bp(dw(self, 0x110))) {
                        vcall<void>(t, 25, start, dir);
                    } else {
                        dw(start, 0) = 0;
                        dw(start, 4) = 0;
                        dw(start, 8) = 0;
                    }
                    using TurnFn = void(THISCALL *)(void *, void *, const float *, float *, DWORD, DWORD);
                    float vel[3], at_[3];
                    DWORD traj = env_value(self + 0x7c), face = env_value(self + 0x58);
                    zax::fn<TurnFn>(0x52ebd0)(self, part, dir, vel, face, traj);
                    traj = env_value(self + 0x7c);
                    face = env_value(self + 0x58);
                    zax::fn<TurnFn>(0x52ebd0)(self, part, start, at_, face, traj);
                    at_[0] = fstp(fld(origin[0]) + fld(at_[0]));
                    const long double y = fld(at_[1]) + fld(origin[1]);
                    at_[2] = fstp(fld(origin[2]) + fld(at_[2]));
                    at_[1] = fstp((y - fld(origin[1])) * cst(glob_addr(C_OVAL_RATIO)) + fld(origin[1]));
                    add_parent_positions3(part, at_);
                    vel[0] = fstp(fld(vel[0]) * fld(speed));
                    vel[1] = fstp(fld(vel[1]) * fld(speed));
                    vel[2] = fstp(fld(vel[2]) * fld(speed));
                    dw(e, 0x10c) = float_bits(age);
                    vcall<void>(e, 58, 0x820000);
                    vcall<void>(e, 59, 0x200000);
                    if (vcall<DWORD>(self, 47) & 0xff) {
                        const long double facing = vcall<long double>(part, 46);
                        vcall<void>(e, 104, float_bits(fstp(facing)), 1);
                    }
                    info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                    if (fl<BYTE>(info, 0x21))
                        vcall<void>(e, 58, 0x40000);
                    else
                        vcall<void>(e, 59, 0x40000);
                    info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                    if (fl<BYTE>(info, 0x20))
                        vcall<void>(e, 58, 0x80000);
                    if (fl<BYTE>(self, 0x108))
                        part_add_child(part, e);
                    else
                        layer_add_part(bp(dw(part, 0x48)), e);
                    vcall<void>(e, 41, dw(base, 0), dw(base, 4));
                    set_target(e, dw(at_, 0), dw(at_, 4));
                    set_render_height(e, x87_fistp(fld(at_[2])));
                    vcall<void>(e, 35, 0);
                    vcall<void>(e, 107, vel);
                    if (fl<BYTE>(self, 0x109)) {
                        alignas(4) BYTE sweep[0x38], hit[0x24], out[8];
                        zax::fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(sweep, 0, 1);
                        zax::fn<void(THISCALL *)(void *)>(0x545c90)(hit);
                        zax::fn<void(THISCALL *)(void *, void *, void *, void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x4e8d40)(
                            bp(dw(e, 0x48) + 0x3c), out, sweep, hit, 0, 0, 0, e, 0);
                        dw(e, 0x10c) = 0x3dcccccd;
                    }
                    info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                    if (dw(info, 0x10)) {
                        BYTE *a = static_cast<BYTE *>(object_clone(dw(info, 0x10)));
                        if (a && !is_kind_of(a, particle_ai_class()))
                            a = nullptr;
                        vcall<void>(a, 42, origin);
                        info = reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dw(self, 0x118)))[i];
                        vcall<void>(a, 43, (ptr_value(info) & ~0xffu) | fl<BYTE>(info, 0x14), (ptr_value(a) & ~0xffu) | fl<BYTE>(info, 0x15), 0);
                        if (BYTE *t = bp(dw(self, 0x110))) {
                            const long double r = vcall<long double>(t, 26);
                            vcall<void>(a, 44, float_bits(fstp(r)));
                        } else {
                            vcall<void>(a, 44, 0x3f800000);
                        }
                        part_add_ai(e, a);
                    }
                }
            }
    {
        const DWORD var = dw(self, 0xa4);
        const DWORD lo = bits_of(-fld(fl<float>(self, 0xa4)));
        fl<float>(self, 0x104) = fstp(random_range(RANDOM3, lo, var) + fld(fl<float>(self, 0xa0)));
    }
    if (dw(self, 0x12c)) {
        DWORD idx;
        if (vcall<DWORD>(self, 48) & 0xff)
            idx = dw(self, 0x12c) * spawned / (possible + 1);
        else
            idx = (DWORD)random_between(GAME_RANDOM, 0, (int)dw(self, 0x12c) - 1);
        const float p[2] = {fstp(fld(origin[0])), fstp(fld(origin[1]))};
        layer_send_sound(bp(dw(part, 0x48)), reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(dw(self, 0x128)))[idx], p);
    }
}

// ---- Vector helpers and the pointer array 0x5ee15c ------------------------------------------------------------

// 0x535400 (thiscall on a vector: a float): scaled
void THISCALL Vec3_Scale(float *a, float k)
{
    a[0] = fstp(fld(k) * fld(a[0]));
    a[1] = fstp(fld(k) * fld(a[1]));
    a[2] = fstp(fld(k) * fld(a[2]));
}
// 0x535420 (thiscall on a vector: another): plus it
void THISCALL Vec3_Add(float *a, const float *b)
{
    a[0] = fstp(fld(b[0]) + fld(a[0]));
    a[1] = fstp(fld(b[1]) + fld(a[1]));
    a[2] = fstp(fld(b[2]) + fld(a[2]));
}
// 0x535440: an empty pointer array {vtable 0x5ee15c, 0, 0, 0}. eax: it.
DWORD *THISCALL PtrArray5ee15c_Ctor(DWORD *a)
{
    a[0] = zax::vt(0x5ee15c);
    a[1] = 0;
    a[2] = 0;
    a[3] = 0;
    return a;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(particles)
{
    const Hook list[] = {
#include "particles_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
