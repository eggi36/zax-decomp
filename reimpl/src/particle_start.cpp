// The particle descriptions of 0x52a860-0x52dc1f (phase 3, counted as Rendering in docs/functions.csv,
// but no drawing): CParticleInfo (one kind of particle of a CGenericParticleSystemAI) and the start
// point types CParticleStartPositionType, CPointParticleStartPositionType, CDiskParticleStartPositionType,
// CLineParticleStartPositionType, CCylinderParticleStartPositionType, CSphereParticleStartPositionType:
// class getters (both copies), Create/CreateArray, deleting destructors, RegisterFields and the slots
// that place a particle (start point, the envelopes' update and reset, the old text lines).
//
// Layouts [confirmed: Create, RegisterFields, docs/fields.csv]:
//   CParticleInfo (0x24, class 0x6d04d8, version 3, parent CObject): +8 "Model", +0x10 "AI" (a
//     CParticleAI), +0x14 "Animated", +0x15 "Obey Sequence FPS", +0x18 "Num Particles", +0x1c "Num
//     Particles Variance", +0x20 "Tries To Collide" (alternative name "Particle Collides"), +0x21
//     "Collidable".
//   CParticleStartPositionType (8, 0x6d04dc, abstract, vtable 0x5fc20c); CPointParticleStartPositionType
//     (8, 0x6d04e0): no fields.
//   CDiskParticleStartPositionType (0x58, 0x6d04e4): +8 "Inside Radius", +0x2c "Outside Radius" (embedded
//     CLengthEnvelope, vtable 0x5f3f2c), +0x50 "Use Cone Angle For Start Pos", +0x54 "YX Ratio" (1.0);
//     CSphereParticleStartPositionType (0x58, 0x6d04f0) adds nothing; CCylinderParticleStartPositionType
//     (0x7c, 0x6d04ec) adds +0x58 "Height of Cylinder".
//   CLineParticleStartPositionType (0x2c, 0x6d04e8): +8 "Center Length".
//
// Slots (names ours [assumption]): 11 (an old text line), 19 (reset fields), 25 (out, direction: the
// start point), 26 (the YX ratio, st0), 27 (seconds: the envelopes advance), 28 (-: the envelopes reset),
// 29/30/31 (the first, second and third envelope's value, st0). An envelope's value is its start value
// (+8) while its speed (+0x14) is 0, else its current value (+0xc).
// Slot 24 (the debug drawing: 0x52bd50, 0x52c530, 0x52d470, 0x52dc00 with the helpers 0x52b010, 0x52cfb0
// and 0x52af80) is in particle_debug.cpp. The second hook table of this file (particle_convert, phase 4) has
// the editor's converting wrappers between the types (0x530d70-0x5353f3, docs/DECOMPILATION.md 9, "The last
// effects: …"), the envelope constructor out of line 0x5317b0 and the disk's slot 26 0x52b5d0.
#include "class_info.h"
#include "ai_common.h"
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD CLS_INFO = 0x6d04d8, CLS_TYPE = 0x6d04dc, CLS_POINT = 0x6d04e0, CLS_DISK = 0x6d04e4, CLS_LINE = 0x6d04e8,
                CLS_CYLINDER = 0x6d04ec, CLS_SPHERE = 0x6d04f0;
constexpr DWORD VT_INFO = 0x5fc138, VT_TYPE = 0x5fc20c, VT_POINT = 0x5fc198, VT_DISK = 0x5fc280, VT_LINE = 0x5fc2fc,
                VT_CYLINDER = 0x5fc374, VT_SPHERE = 0x5fc3f4;
constexpr DWORD VT_ENVELOPE = 0x5f3cbc, VT_LENGTH = 0x5f3f2c;
constexpr DWORD NO_FIELDS = 0x58ea40, CREATE_NONE = 0x46b760, CREATE_ARRAY_NONE = 0x52dc80;
constexpr DWORD EMBEDDED_FIELD_CTOR_VALUE = 0x5284c0;
constexpr DWORD C_TWO_PI_BITS = 0x40c90fdb;

CClassInfo *object_class() { return zax::fn<Getter>(0x4163b0)(); }
DWORD reset_fields(void *o) { return zax::fn<DWORD(THISCALL *)(void *)>(0x417230)(o); }
long double trig_da0(DWORD a) { return zax::fn<long double(__stdcall *)(DWORD)>(0x508da0)(a); }
long double trig_ec0(DWORD a) { return zax::fn<long double(__stdcall *)(DWORD)>(0x508ec0)(a); }
DWORD bits_of(long double v) { return float_bits(fstp(v)); }
// CObject_ReadOwnLine and the C runtime's atof (st0)
DWORD read_own_line(void *o, const char *name, const char *value, DWORD a3)
{
    return zax::fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x417500)(o, name, value, a3);
}
long double crt_atof(const char *s) { return zax::fn<long double(__cdecl *)(const char *)>(0x5d03de)(s); }

// ---- Constructors and destructors as Create, CreateArray and slot 1 inline them --------------------------

// An embedded length envelope: CObject, CEnvelope (+0x20 0), CLengthEnvelope
void length_envelope(BYTE *e)
{
    obj_ctor(e);
    vtable_and_defaults(e, vt(VT_ENVELOPE));
    dw(e, 0x20) = 0;
    vtable_and_defaults(e, vt(VT_LENGTH));
}
void length_envelope_destroy(BYTE *e)
{
    vtable_and_free(e, vt(VT_LENGTH));
    vtable_and_free(e, vt(VT_ENVELOPE));
    obj_dtor(e);
}
// The same with CEnvelope's level out of line (0x52cc70, as the cylinder's destructor calls it)
void length_envelope_destroy_call(BYTE *e)
{
    vtable_and_free(e, vt(VT_LENGTH));
    zax::fn<void(THISCALL *)(void *)>(0x52cc70)(e);
}
void type_level(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_TYPE));
}
void type_level_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TYPE));
    obj_dtor(o);
}

void info_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_INFO));
    dw(o, 0x10) = 0;
    dw(o, 8) = 0;
}
void info_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_INFO));
    obj_dtor(o);
}
void point_construct(BYTE *o)
{
    type_level(o);
    vtable_and_defaults(o, vt(VT_POINT));
}
void point_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_POINT));
    type_level_destroy(o);
}
void disk_level(BYTE *o)
{
    type_level(o);
    length_envelope(o + 8);
    length_envelope(o + 0x2c);
    vtable_and_defaults(o, vt(VT_DISK));
}
void disk_construct(BYTE *o) { disk_level(o); }
void disk_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DISK));
    length_envelope_destroy(o + 0x2c);
    length_envelope_destroy(o + 8);
    type_level_destroy(o);
}
void sphere_construct(BYTE *o)
{
    disk_level(o);
    vtable_and_defaults(o, vt(VT_SPHERE));
}
void sphere_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SPHERE));
    disk_destroy(o);
}
void line_construct(BYTE *o)
{
    type_level(o);
    length_envelope(o + 8);
    vtable_and_defaults(o, vt(VT_LINE));
}
void line_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_LINE));
    length_envelope_destroy(o + 8);
    type_level_destroy(o);
}
void cylinder_construct(BYTE *o)
{
    disk_level(o);
    length_envelope(o + 0x58);
    vtable_and_defaults(o, vt(VT_CYLINDER));
}
// The single object: +0x2c's CEnvelope level inline, +8's through 0x52cc70; an array element: both
// through 0x52cc70
void cylinder_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_CYLINDER));
    length_envelope_destroy(o + 0x58);
    vtable_and_free(o, vt(VT_DISK));
    length_envelope_destroy(o + 0x2c);
    length_envelope_destroy_call(o + 8);
    type_level_destroy(o);
}
void cylinder_destroy_element(BYTE *o)
{
    vtable_and_free(o, vt(VT_CYLINDER));
    length_envelope_destroy(o + 0x58);
    vtable_and_free(o, vt(VT_DISK));
    length_envelope_destroy_call(o + 0x2c);
    length_envelope_destroy_call(o + 8);
    type_level_destroy(o);
}

// ---- CParticleInfo -------------------------------------------------------------------------------------------

// 0x52a860, 0x52a9e0 (slot 2)
CClassInfo *__cdecl ParticleInfo_ClassInfo()
{
    return class_getter(CLS_INFO, 0x61d98c /* "CParticleInfo" */, 3.0f, object_class, 0x24, 0, 0x52a8c0, 0x52a900, 0x52aa40);
}
// 0x52a8c0, 0x52a900, 0x52a960 (slot 1)
void *__cdecl ParticleInfo_Create() { return create_one<0x24, info_construct>(); }
void *__fastcall ParticleInfo_CreateArray(int count, int, int) { return create_array<0x24, info_construct>(count); }
void *THISCALL ParticleInfo_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, info_destroy>(self, flags); }
// 0x52a8b0 (slot 19): "Model" and "AI" cleared, then CObject_ResetFields
DWORD THISCALL ParticleInfo_Reset(BYTE *self)
{
    dw(self, 8) = 0;
    dw(self, 0x10) = 0;
    return reset_fields(self);
}
// 0x52aa40
DWORD __cdecl ParticleInfo_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61248c) /* "Model" */, 8, 4, 8, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(ParticleInfo_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x614e3c) /* "AI" */, 0x10, 4, 0x10, class_at(fn_addr(0x528550)) /* CParticleAI */, 1, 0, 0, 0);
    add_field(ParticleInfo_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d818) /* "Animated" */, 0x14, 1, 0x14, 1, 0, 0);
    add_field(ParticleInfo_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d804) /* "Obey Sequence FPS" */, 0x15, 1, 0x15, 1, 0, 0);
    add_field(ParticleInfo_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x612d3c) /* "Tries To Collide" */, 0x20, 1, 0x20, 1, 0, 0);
    add_field(ParticleInfo_ClassInfo(), f);
    set_alt_name(last_field(ParticleInfo_ClassInfo()), glob_addr(0x61d9c4) /* "Particle Collides" */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x614b70) /* "Collidable" */, 0x21, 1, 0x21, 1, 0, 0);
    add_field(ParticleInfo_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x61d9b4) /* "Num Particles" */, 0x18, 4, 0x18, 1, 1, 0, 0, 0x3e8);
    add_field(ParticleInfo_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x61d99c) /* "Num Particles Variance" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0x3e8);
    return add_field(ParticleInfo_ClassInfo(), f);
}

// ---- The start point types ------------------------------------------------------------------------------------

// 0x52aed0, 0x52af20 (slot 2)
CClassInfo *__cdecl StartType_ClassInfo()
{
    return class_getter(CLS_TYPE, 0x61d9d8 /* "CParticleStartPositionType" */, 1.0f, object_class, 8, 1, CREATE_NONE, CREATE_ARRAY_NONE,
                        NO_FIELDS);
}
// 0x52b1d0, 0x52b410
CClassInfo *__cdecl PointStart_ClassInfo()
{
    return class_getter(CLS_POINT, 0x61d9f4 /* "CPointParticleStartPositionType" */, 1.0f, StartType_ClassInfo, 8, 0, 0x52b2d0, 0x52b310,
                        NO_FIELDS);
}
// 0x52b4d0, 0x52b8c0
CClassInfo *__cdecl DiskStart_ClassInfo()
{
    return class_getter(CLS_DISK, 0x61da14 /* "CDiskParticleStartPositionType" */, 1.0f, StartType_ClassInfo, 0x58, 0, 0x52b600, 0x52b6a0,
                        0x52b980);
}
// 0x52c040, 0x52c340
CClassInfo *__cdecl LineStart_ClassInfo()
{
    return class_getter(CLS_LINE, 0x61da84 /* "CLineParticleStartPositionType" */, 1.0f, StartType_ClassInfo, 0x2c, 0, 0x52c140, 0x52c1b0,
                        0x52c400);
}
// 0x52c770, 0x52cc90
CClassInfo *__cdecl CylinderStart_ClassInfo()
{
    return class_getter(CLS_CYLINDER, 0x61dab4 /* "CCylinderParticleStartPositionType" */, 1.0f, DiskStart_ClassInfo, 0x7c, 0, 0x52c8f0,
                        0x52c9c0, 0x52cda0);
}
// 0x52d5c0, 0x52d9c0
CClassInfo *__cdecl SphereStart_ClassInfo()
{
    return class_getter(CLS_SPHERE, 0x61dafc /* "CSphereParticleStartPositionType" */, 1.0f, DiskStart_ClassInfo, 0x58, 0, 0x52d6c0, 0x52d770,
                        NO_FIELDS);
}

// 0x52b280 (slot 1 of the abstract class)
void *THISCALL StartType_DeletingDtor(BYTE *self, DWORD flags)
{
    type_level_destroy(self);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}
// 0x52b2d0, 0x52b310, 0x52b370
void *__cdecl PointStart_Create() { return create_one<8, point_construct>(); }
void *__fastcall PointStart_CreateArray(int count, int, int) { return create_array<8, point_construct>(count); }
void *THISCALL PointStart_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, point_destroy>(self, flags); }
// 0x52b600, 0x52b6a0, 0x52b770
void *__cdecl DiskStart_Create() { return create_one<0x58, disk_construct>(); }
void *__fastcall DiskStart_CreateArray(int count, int, int) { return create_array<0x58, disk_construct>(count); }
void *THISCALL DiskStart_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x58, disk_destroy>(self, flags); }
// 0x52c140, 0x52c1b0, 0x52c250
void *__cdecl LineStart_Create() { return create_one<0x2c, line_construct>(); }
void *__fastcall LineStart_CreateArray(int count, int, int) { return create_array<0x2c, line_construct>(count); }
void *THISCALL LineStart_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x2c, line_destroy>(self, flags); }
// 0x52c8f0, 0x52c9c0, 0x52cae0
void *__cdecl CylinderStart_Create() { return create_one<0x7c, cylinder_construct>(); }
void *__fastcall CylinderStart_CreateArray(int count, int, int) { return create_array<0x7c, cylinder_construct>(count); }
void *THISCALL CylinderStart_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2)
        return deleting_dtor<0x7c, cylinder_destroy_element>(self, flags);
    return deleting_dtor<0x7c, cylinder_destroy>(self, flags);
}
// 0x52d6c0, 0x52d770, 0x52d850
void *__cdecl SphereStart_Create() { return create_one<0x58, sphere_construct>(); }
void *__fastcall SphereStart_CreateArray(int count, int, int) { return create_array<0x58, sphere_construct>(count); }
void *THISCALL SphereStart_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x58, sphere_destroy>(self, flags); }
// 0x52cc70: CEnvelope's and CObject's level of an envelope's destructor
void THISCALL Envelope_DestroyLevel(BYTE *self)
{
    vtable_and_free(self, vt(VT_ENVELOPE));
    obj_dtor(self);
}

// 0x52b980
DWORD __cdecl DiskStart_RegisterFields()
{
    void *f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61da64) /* "Inside Radius" */, 8, 0x24, 8, class_at(fn_addr(0x49f4d0)) /* CLengthEnvelope */,
                        glob_addr(0x613390) /* "Start Value" */, 1);
    add_field(DiskStart_ClassInfo(), f);
    f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61da54) /* "Outside Radius" */, 0x2c, 0x24, 0x2c, class_at(fn_addr(0x49f4d0)), glob_addr(0x613390) /* "Start Value" */,
                  1);
    add_field(DiskStart_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60b340) /* "YX Ratio" */, 0x54, 4, 0x54, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0x3d4ccccd /* 0.05 */);
    add_field(DiskStart_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61da34) /* "Use Cone Angle For Start Pos" */, 0x50, 1, 0x50, 1, 0, 0);
    return add_field(DiskStart_ClassInfo(), f);
}
// 0x52c400
DWORD __cdecl LineStart_RegisterFields()
{
    void *f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61daa4) /* "Center Length" */, 8, 0x24, 8, class_at(fn_addr(0x49f4d0)), glob_addr(0x613390) /* "Start Value" */,
                        1);
    return add_field(LineStart_ClassInfo(), f);
}
// 0x52cda0
DWORD __cdecl CylinderStart_RegisterFields()
{
    void *f = new_field(0x34, EMBEDDED_FIELD_CTOR_VALUE, glob_addr(0x61dad8) /* "Height of Cylinder" */, 0x58, 0x24, 0x58, class_at(fn_addr(0x49f4d0)),
                        glob_addr(0x613390) /* "Start Value" */, 1);
    return add_field(CylinderStart_ClassInfo(), f);
}

// An envelope's value (the start value while its speed is 0 or NaN) on the FPU
long double envelope_value(BYTE *e) { return fld(c3(fld(fl<float>(e, 0x14)), cst(glob_addr(C_ZERO))) ? fl<float>(e, 8) : fl<float>(e, 0xc)); }
// 0x52c120 (slot 29), 0x52b5e0 (slot 30), 0x52c8d0 (slot 31): the envelope at +8, +0x2c, +0x58
long double THISCALL StartType_Value8(BYTE *self) { return envelope_value(self + 8); }
long double THISCALL DiskStart_Value2c(BYTE *self) { return envelope_value(self + 0x2c); }
long double THISCALL CylinderStart_Value58(BYTE *self) { return envelope_value(self + 0x58); }
// 0x52b5d0 (slot 26): "YX Ratio"
long double THISCALL DiskStart_Ratio(BYTE *self) { return fld(fl<float>(self, 0x54)); }
// 0x52b580, 0x52c0f0, 0x52c870 (slot 27: seconds): the envelopes advance (their slot 23)
void THISCALL DiskStart_Advance(BYTE *self, DWORD seconds)
{
    vcall<void>(self + 8, 23, seconds);
    vcall<void>(self + 0x2c, 23, seconds);
}
DWORD THISCALL LineStart_Advance(BYTE *self, DWORD seconds) { return vcall<DWORD>(self + 8, 23, seconds); }
void THISCALL CylinderStart_Advance(BYTE *self, DWORD seconds)
{
    vcall<void>(self + 8, 23, seconds);
    vcall<void>(self + 0x2c, 23, seconds);
    vcall<void>(self + 0x58, 23, seconds);
}
// 0x52b5b0, 0x52c110, 0x52c8a0 (slot 28: one unused argument): the envelopes reset (their slot 24)
void THISCALL DiskStart_Reset(BYTE *self, DWORD)
{
    vcall<void>(self + 8, 24);
    vcall<void>(self + 0x2c, 24);
}
DWORD THISCALL LineStart_Reset(BYTE *self, DWORD) { return vcall<DWORD>(self + 8, 24); }
void THISCALL CylinderStart_Reset(BYTE *self, DWORD)
{
    vcall<void>(self + 8, 24);
    vcall<void>(self + 0x2c, 24);
    vcall<void>(self + 0x58, 24);
}

// 0x52b2b0 (slot 25: out, direction): the point itself
void THISCALL PointStart_Point(BYTE *, DWORD *out, const DWORD *)
{
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
}
// 0x52c720 (slot 25): on the line through the point along z, the centre length (slot 29) times a random
// factor in [-1, 1] (the game generator)
void THISCALL LineStart_Point(BYTE *self, float *out, const float *)
{
    const long double len = vcall<long double>(self, 29);
    dw(out, 0) = 0;
    dw(out, 4) = 0;
    out[2] = fstp(len);
    const long double r = random_range(GAME_RANDOM, 0xbf800000, 0x3f800000);
    out[0] = fstp(r * fld(out[0]));
    out[1] = fstp(r * fld(out[1]));
    out[2] = fstp(r * fld(out[2]));
}
// The radius of the disk and the sphere: random between the inside radius (slot 29) and inside +
// outside (slot 30) (generator 0x712cc0) times the unit vector; y scaled by "YX Ratio"
void scale_by_radius(BYTE *self, float *out)
{
    const float outside = fstp(vcall<long double>(self, 30));
    const DWORD hi = bits_of(vcall<long double>(self, 29) + fld(outside));
    const DWORD lo = bits_of(vcall<long double>(self, 29));
    const long double r = random_range(RANDOM3, lo, hi);
    out[0] = fstp(r * fld(out[0]));
    const float y = fstp(r * fld(out[1]));
    out[1] = y;
    out[2] = fstp(r * fld(out[2]));
    out[1] = fstp(fld(y) * fld(fl<float>(self, 0x54)));
}
// A random unit vector in the y-z plane (generator 0x712cc0): (0, cos a, -sin a) (0x508da0, 0x508ec0)
void random_circle(float *out)
{
    const DWORD a = bits_of(random_range(RANDOM3, 0, C_TWO_PI_BITS));
    const float nc = fstp(-trig_ec0(a));
    const long double s = trig_da0(a);
    dw(out, 0) = 0;
    out[1] = fstp(s);
    out[2] = nc;
}
// 0x52bed0 (slot 25): on the disk around the point: with "Use Cone Angle For Start Pos" the direction's
// y and z normalised (a random one when both are 0; unchanged when their length is 0 otherwise), else a
// random one; then scaled by the radius
void THISCALL DiskStart_Point(BYTE *self, float *out, const DWORD *dir)
{
    if (fl<BYTE>(self, 0x50)) {
        dw(out, 0) = dir[0];
        dw(out, 4) = dir[1];
        dw(out, 8) = dir[2];
        dw(out, 0) = 0;
        if (c3(fld(out[1]), cst(glob_addr(C_ZERO))) && c3(fld(out[2]), cst(glob_addr(C_ZERO)))) {
            random_circle(out);
        } else {
            const long double z = fld(out[2]), y = fld(out[1]);
            const long double len = fsqrt(z * z + y * y);
            if (!c3(len, cst(glob_addr(C_ZERO)))) {
                const long double inv = cst(glob_addr(C_ONE)) / len;
                dw(out, 0) = 0;
                out[1] = fstp(inv * fld(out[1]));
                out[2] = fstp(inv * fld(out[2]));
            }
        }
    } else {
        random_circle(out);
    }
    scale_by_radius(self, out);
}
// 0x52cf70 (slot 25): the disk's point, x random between 0 and the height (slot 31)
void THISCALL CylinderStart_Point(BYTE *self, float *out, const DWORD *dir)
{
    zax::fn<void(THISCALL *)(void *, float *, const DWORD *)>(0x52bed0)(self, out, dir);
    const DWORD h = bits_of(vcall<long double>(self, 31));
    out[0] = fstp(random_range(RANDOM3, 0, h));
}
// 0x52dad0 (slot 25): in the sphere: with "Use Cone Angle For Start Pos" the direction as it is, else a
// random vector in the cube [-1, 1]³ normalised (unless its length is 0); then scaled by the radius
void THISCALL SphereStart_Point(BYTE *self, float *out, const DWORD *dir)
{
    if (fl<BYTE>(self, 0x50)) {
        dw(out, 0) = dir[0];
        dw(out, 4) = dir[1];
        dw(out, 8) = dir[2];
    } else {
        const float a = fstp(random_range(RANDOM3, 0xbf800000, 0x3f800000));
        const float b = fstp(random_range(RANDOM3, 0xbf800000, 0x3f800000));
        const long double c = random_range(RANDOM3, 0xbf800000, 0x3f800000);
        out[1] = b;
        out[0] = fstp(c);
        out[2] = a;
        const long double len = fsqrt(c * c + fld(b) * fld(b) + fld(a) * fld(a));
        if (!c3(len, cst(glob_addr(C_ZERO)))) {
            const long double inv = cst(glob_addr(C_ONE)) / len;
            out[0] = fstp(c * inv);
            out[1] = fstp(inv * fld(b));
            out[2] = fstp(inv * fld(a));
        }
    }
    scale_by_radius(self, out);
}

// 0x52bce0, 0x52c4e0, 0x52ced0 (slot 11: name, value, a3): the old lines (atof), else
// CObject_ReadOwnLine
struct OldLine {
    GlobKey name;
    DWORD offset;
};
constexpr OldLine DISK_LINES[] = {{0x61da74 /* "Inner Radius" */, 0x10}, {0x61d97c /* "Outer Radius" */, 0x34}};
constexpr OldLine LINE_LINES[] = {{0x61d968 /* "Length From Center" */, 0x10}};
constexpr OldLine CYLINDER_LINES[] = {{0x61daec /* "Cylinder Height" */, 0x60}, {0x61da74, 0x10}, {0x61d97c, 0x34}};
template <const OldLine *L, int N> DWORD THISCALL StartType_ReadLine(BYTE *self, const char *name, const char *value, DWORD a3)
{
    for (int i = 0; i < N; i++)
        if (!crt_stricmp(name, glob<const char *>(L[i].name))) {
            fl<float>(self, L[i].offset) = fstp(crt_atof(value));
            return 0;
        }
    return read_own_line(self, name, value, a3);
}

// ---- The editor's converting wrappers between the start point types (0x530d70-0x5353f3) --------------------
//
// Twenty global CActionWrapper objects of the static initialisers 0x52a220-0x52a855 (vtables 0x5fc048-0x5fc12c,
// three slots each: 0 the wrappers' base destructor 0x41a0c0, 1 Describe, 2 Convert; +8 the text, +0xc/+0x10 the
// field names to copy, all 0 in the exe), one per pair of the five concrete types. Their class getters are
// inline in the exe (the same registration as the getters above).

const char *start_describe(BYTE *self, void *o, Getter from, Getter to, CClassInfo **cls)
{
    if (!is_kind_of(o, from()))
        return nullptr;
    if (vcall<CClassInfo *>(o, 2) == to())
        return nullptr;
    if (cls)
        *cls = to();
    if (const char *t = reinterpret_cast<const char *>(dw(self, 8)))
        return t;
    return zax::fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(to());
}

}  // namespace

// Slot 1 (object, class out): an object of the source type that is not of the target type: the wrapper's
// text or the target class's short name (eax 0 otherwise)
#define START_DESCRIBE(fn, from, to)                                                                                         \
    const char *THISCALL fn(BYTE *self, void *o, CClassInfo **cls) { return start_describe(self, o, from, to, cls); }
START_DESCRIBE(PointToLine_Describe, PointStart_ClassInfo, LineStart_ClassInfo)              // 0x530d70
START_DESCRIBE(PointToDisk_Describe, PointStart_ClassInfo, DiskStart_ClassInfo)              // 0x531040
START_DESCRIBE(PointToCylinder_Describe, PointStart_ClassInfo, CylinderStart_ClassInfo)      // 0x531310
START_DESCRIBE(PointToSphere_Describe, PointStart_ClassInfo, SphereStart_ClassInfo)          // 0x5317e0
START_DESCRIBE(CylinderToPoint_Describe, CylinderStart_ClassInfo, PointStart_ClassInfo)      // 0x531ba0
START_DESCRIBE(CylinderToLine_Describe, CylinderStart_ClassInfo, LineStart_ClassInfo)        // 0x531ec0
START_DESCRIBE(CylinderToDisk_Describe, CylinderStart_ClassInfo, DiskStart_ClassInfo)        // 0x5321e0
START_DESCRIBE(CylinderToSphere_Describe, CylinderStart_ClassInfo, SphereStart_ClassInfo)    // 0x532500
START_DESCRIBE(DiskToPoint_Describe, DiskStart_ClassInfo, PointStart_ClassInfo)              // 0x532910
START_DESCRIBE(DiskToLine_Describe, DiskStart_ClassInfo, LineStart_ClassInfo)                // 0x532be0
START_DESCRIBE(DiskToCylinder_Describe, DiskStart_ClassInfo, CylinderStart_ClassInfo)        // 0x532f80
START_DESCRIBE(DiskToSphere_Describe, DiskStart_ClassInfo, SphereStart_ClassInfo)            // 0x533340
START_DESCRIBE(SphereToPoint_Describe, SphereStart_ClassInfo, PointStart_ClassInfo)          // 0x533700
START_DESCRIBE(SphereToLine_Describe, SphereStart_ClassInfo, LineStart_ClassInfo)            // 0x533a20
START_DESCRIBE(SphereToCylinder_Describe, SphereStart_ClassInfo, CylinderStart_ClassInfo)    // 0x533d40
START_DESCRIBE(SphereToDisk_Describe, SphereStart_ClassInfo, DiskStart_ClassInfo)            // 0x534150
START_DESCRIBE(LineToPoint_Describe, LineStart_ClassInfo, PointStart_ClassInfo)              // 0x534470
START_DESCRIBE(LineToSphere_Describe, LineStart_ClassInfo, SphereStart_ClassInfo)            // 0x5347f0
START_DESCRIBE(LineToCylinder_Describe, LineStart_ClassInfo, CylinderStart_ClassInfo)        // 0x534c80
START_DESCRIBE(LineToDisk_Describe, LineStart_ClassInfo, DiskStart_ClassInfo)                // 0x535040
#undef START_DESCRIBE

// Slot 2 (object): a new object of the target type (pool block, the constructor inline; no null test), its
// slot 19, slot 20 from the old one, the field named at +0xc copied into the one named at +0x10 (the
// descriptor's slot 4), the old object deleted (slot 1 with 1). eax: the new object.
template <DWORD SIZE, void (*CONSTRUCT)(BYTE *)> BYTE *THISCALL Start_Convert(BYTE *self, BYTE *old)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (o)
        CONSTRUCT(o);
    vcall<void>(o, 19);
    vcall<void>(o, 20, old);
    if (const char *from = reinterpret_cast<const char *>(dw(self, 0xc))) {
        using Find = BYTE *(THISCALL *)(CClassInfo *, const char *, DWORD);
        BYTE *f = zax::fn<Find>(0x4154d0)(vcall<CClassInfo *>(old, 2), from, 0);
        BYTE *t = zax::fn<Find>(0x4154d0)(vcall<CClassInfo *>(o, 2), reinterpret_cast<const char *>(dw(self, 0x10)), 0);
        if (f && t)
            vcall<void>(t, 4, o, old, f);
    }
    if (old)
        vcall<void *>(old, 1, 1);
    return o;
}

// 0x5317b0 (thiscall): an embedded length envelope's constructor out of line (the converters call it)
BYTE *THISCALL LengthEnvelope_Ctor(BYTE *e)
{
    length_envelope(e);
    return e;
}


// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(particle_start)
{
    const Hook list[] = {
#include "particle_start_hooks.inc"
    };
    return emit_hooks(out, list);
}


ZAX_HOOKS(particle_convert)
{
    const Hook list[] = {
#include "particle_convert_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
