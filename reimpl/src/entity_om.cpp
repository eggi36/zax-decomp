// CEntityOm (0x525c70-0x5266c4), the boss Om, reimplemented: Create, CreateArray, the deleting destructor,
// the class getters (both copies), RegisterFields and its slots: the collision polygon that turns with
// the facing angle, the "hit in the eye" test, the damage scale and the turn that stops at obstacles.
//
// Layout [confirmed: Create 0x525cd0, RegisterFields, docs/fields.csv] (0x16c bytes, class 0x6d016c,
// version 1, vtable 0x5fba6c, parent CEntityWalking): +0x130 the turned polygon (0x28 bytes, Polygon_Ctor
// 0x545cc0), +0x158 the angle it was turned for (0 at first), +0x15c "Angle Of Allowed Collisions"
// (radians, shown in degrees), +0x160 "Angle Plus Or Minus" (0.1309 = 7.5 degrees), +0x164 "Debug/Has
// Been Hit In Eye", +0x168 "Collided While Rotating Action". The facing angle is CEntityAnimated's +0xc8
// (slot 46 gives it). Names ours [assumption on the purpose of the angles from the help texts].
//
// Slots [confirmed: code]:
//  - 48 (the polygon): CEntityBase's polygon turned by the facing angle around its centre (the y axis
//    scaled by 1/0.7128 before and by 0.7128 after, the oval of the view), through the point buffer
//    0x711120 and Polygon_SetFromBuffer; only again when the angle changed.
//  - 49 (set the polygon): the polygon turned back by the angle becomes CEntityBase's.
//  - 67 (collision: a1, the point, a3, a4): "hit in the eye" while the angle from the centre to the point,
//    less the allowed angle, lies within plus or minus the tolerance of the facing angle; the
//    destructable's collision 0x48cb90 then runs with the flag set (slot 100 gives the damage only then).
//  - 100 (damage scale: value, b): the value while hit in the eye, otherwise 0.
//  - 104 (facing: angle, b): a changed angle is set (0x48fa00) and taken back when the turned Om overlaps
//    another part (0x526600: the parts in its rectangle, category bits 0x840000, that neither ignores and
//    that are not characters), which runs "Collided While Rotating Action".
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_OM = 0x5fba6c;
constexpr DWORD POINT_BUFFER = 0x711120;   // the polygon point buffer (Polygon_SetFromBuffer's)
constexpr DWORD C_INV_OVAL = 0x5ee9bc;     // 1 / 0.7128
constexpr DWORD C_MINUS_ONE = 0x5eeccc;    // -1

void poly_ctor(BYTE *p) { fn<void *(THISCALL *)(void *)>(0x545cc0)(p); }
void poly_dtor(BYTE *p) { fn<void(THISCALL *)(void *)>(0x545cf0)(p); }
void poly_assign(BYTE *p, const void *other) { fn<void *(THISCALL *)(void *, const void *)>(0x545e80)(p, other); }
void poly_set_from(BYTE *p, const void *other) { fn<void *(THISCALL *)(void *, const void *)>(0x545d50)(p, other); }
void poly_center(BYTE *p, float *out) { fn<void *(THISCALL *)(void *, float *)>(0x546a70)(p, out); }
long double fpatan_of(long double y, long double x) { return fpatan(y, x); }
// An angle kept on the FPU wrapped into [0, 2 pi) (the unrounded value compared, as fcom st0)
long double wrap_st0(long double a)
{
    if (c0(a, cst(glob_addr(C_ZERO))))
        a = a + cst(glob_addr(C_TWO_PI));
    if (!c0(a, cst(glob_addr(C_TWO_PI))))
        a = a - cst(glob_addr(C_TWO_PI));
    return a;
}
// One point turned around the centre: dx, dy (y already scaled up) and the table's two values of the
// angle (0x508fe0: ecx a, edx b)
void turn_point(float a, float b, float dx, float dy, const float *c, float *out)
{
    const long double x = fld(b) * dx + fld(a) * dy;
    const float y = fstp((-fld(a) * dx + fld(b) * dy) * cst(glob_addr(C_OVAL_RATIO)));
    out[1] = fstp(fld(c[1]) + y);
    out[0] = fstp(x + c[0]);
}

}  // namespace

static void om_ctor(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x49bcb0)(o);   // CEntityWalking's constructor
    poly_ctor(o + 0x130);
    vtable_and_defaults(o, vt(VT_OM));
    dw(o, 0x158) = 0;
}
static void om_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_OM));
    poly_dtor(o + 0x130);
    fn<void(THISCALL *)(void *)>(0x49bd20)(o);   // CEntityWalking's destructor
}
static CClassInfo *__cdecl EntityOm_ClassInfo();
static void *__cdecl EntityOm_Create() { return create_one<0x16c, om_ctor>(); }
static void *__fastcall EntityOm_CreateArray(int count, int, int) { return create_array<0x16c, om_ctor>(count); }
// 0x525c70 (static registration), 0x525e50 (slot 2)
static CClassInfo *__cdecl EntityOm_ClassInfo()
{
    return class_getter(0x6d016c, 0x61d2d8 /* "CEntityOm" */, 1.0f, entity_walking_class, 0x16c, 0, 0x525cd0, 0x525d20, 0x525ec0);
}
// 0x525da0 (slot 1)
static void *THISCALL EntityOm_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x16c, om_destroy>(self, flags); }
// 0x525ec0
static DWORD __cdecl EntityOm_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61d3c8) /* "Angle Of Allowed Collisions" */, glob_addr(0x60ff9c) /* "Radians" */, 0x15c, 4, 0x15c, 1, 0, 0, 0,
                        0x40c90fdb /* 6.28319 */, 0);
    add_field(EntityOm_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    set_help(f, 0x61d3a0 /* "At what angle Om accepts damage from." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61d38c) /* "Angle Plus Or Minus" */, glob_addr(0x60ff9c) /* "Radians" */, 0x160, 4, 0x160, 1,
                  0x3e060a92 /* 0.1309 */, 0, 0, 0x40490fdb /* 3.14159 */, 0);
    add_field(EntityOm_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    set_help(f, 0x61d358 /* "Half the width of the angle Om accepts damage from." */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d33c) /* "Debug/Has Been Hit In Eye" */, 0x164, 1, 0x164, 3, 0, 0);
    add_field(EntityOm_ClassInfo(), f);
    set_help(last_field(EntityOm_ClassInfo()), 0x61d304 /* "If Om has been hit in the eye, and is awaiting damage." */);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61d2e4) /* "Collided While Rotating Action" */, 0x168, 4, 0x168, action_class(), 1, 0, 0, 0);
    return add_field(EntityOm_ClassInfo(), f);
}
// 0x5261c0 (slot 48): the turned polygon (made again when the facing angle +0xc8 differs from +0x158).
// eax: +0x130.
static BYTE *THISCALL EntityOm_Polygon(BYTE *self)
{
    if (c3(fld(fl<float>(self, 0xc8)), fld(fl<float>(self, 0x158))))
        return self + 0x130;
    BYTE *poly = fn<BYTE *(THISCALL *)(void *)>(0x48b860)(self);   // CEntityBase::Polygon
    poly_assign(self + 0x130, poly);
    float c[2];
    poly_center(self + 0x130, c);
    for (DWORD i = 0; i < dw(poly, 8); i++) {
        const float *p = reinterpret_cast<const float *>(static_cast<uintptr_t>(dw(poly, 0))) + i * 2;
        const float dx = fstp(fld(p[0]) - c[0]);
        const float dy = fstp((fld(raw_float(float_bits(p[1]))) - c[1]) * cst(glob_addr(C_INV_OVAL)));
        float a, b;
        const long double ang = vcall<long double>(self, 46);
        fn<void(__fastcall *)(float *, float *, DWORD)>(0x508fe0)(&a, &b, float_bits(fstp(-ang)));
        turn_point(a, b, dx, dy, c, at<float *>(glob_addr(POINT_BUFFER) + i * 8));
    }
    fn<void(THISCALL *)(void *, DWORD)>(0x546580)(self + 0x130, dw(poly, 8));   // Polygon_SetFromBuffer
    fl<float>(self, 0x158) = fstp(fld(fl<float>(self, 0xc8)));
    return self + 0x130;
}
// 0x5262f0 (slot 49: polygon): the polygon kept at +0x130, a copy turned back by the facing angle given to
// CEntityBase (0x48b9c0). Nothing for null.
static void THISCALL EntityOm_SetPolygon(BYTE *self, const BYTE *p)
{
    if (!p)
        return;
    poly_set_from(self + 0x130, p);
    BYTE local[0x28];
    poly_ctor(local);
    poly_set_from(local, self + 0x130);
    float c[2];
    poly_center(local, c);
    for (DWORD i = 0; i < dw(self, 0x138); i++) {
        const float *q = reinterpret_cast<const float *>(static_cast<uintptr_t>(dw(self, 0x130))) + i * 2;
        const float dx = fstp(fld(q[0]) - c[0]);
        const float dy = fstp((fld(raw_float(float_bits(q[1]))) - c[1]) * cst(glob_addr(C_INV_OVAL)));
        float a, b;
        const long double ang = vcall<long double>(self, 46) * cst(glob_addr(C_MINUS_ONE));
        fn<void(__fastcall *)(float *, float *, DWORD)>(0x508fe0)(&a, &b, float_bits(fstp(-ang)));
        float out[2];
        turn_point(a, b, dx, dy, c, out);
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x546630)(local, i, float_bits(out[0]), float_bits(out[1]));   // Polygon_SetPoint
    }
    fn<void(THISCALL *)(void *, const void *)>(0x48b9c0)(self, local);   // CEntityBase::SetPolygon
    poly_dtor(local);
}
// 0x526420 (slot 67: a1, the collision point, a3, a4): see above. eax: 0x48cb90's.
static DWORD THISCALL EntityOm_Collide(BYTE *self, DWORD a1, const float *point, DWORD a3, DWORD a4)
{
    if (point) {
        const float q[2] = {point[0], point[1]};
        float pos[2];
        part_position(self, pos);
        const long double dx = fld(q[0]) - pos[0];
        const long double dy = fld(q[1]) - pos[1];
        long double a = wrap_st0(fpatan_of(dy, dx));
        a = wrap_st0(a - fl<float>(self, 0x15c));
        const float rel = fstp(a);
        long double d = fld(rel) - vcall<long double>(self, 46);
        d = wrap_st0(d + cst(glob_addr(C_PI))) - cst(glob_addr(C_PI));
        const float df = fstp(d);
        fl<BYTE>(self, 0x164) = c03(fld(raw_float(float_bits(df) & 0x7fffffffu)), fld(fl<float>(self, 0x160))) ? 1 : 0;
    } else {
        fl<BYTE>(self, 0x164) = 0;
    }
    const DWORD r = fn<DWORD(THISCALL *)(void *, DWORD, const float *, DWORD, DWORD)>(0x48cb90)(self, a1, point, a3, a4);   // EntityDestructable_OnCollide
    fl<BYTE>(self, 0x164) = 0;
    return r;
}
// 0x526550 (slot 100: value, b): st0 the value while hit in the eye, otherwise 0
static long double THISCALL EntityOm_DamageScale(BYTE *self, float value, DWORD)
{
    if (fl<BYTE>(self, 0x164))
        return fld(value);
    return cst(glob_addr(C_ZERO));
}
// 0x526600: does the Om overlap a part in its rectangle (slot 37 with 3 and 0; the layer's FindRect with
// the category bits 0x840000) that it does not ignore and that does not ignore it (slot 66) and that is
// not a character (0x4fc680)? al.
static DWORD THISCALL EntityOm_Overlaps(BYTE *self)
{
    DWORD rect[4];
    vcall<void>(self, 37, rect, (DWORD)3, (DWORD)0);
    void *tmp;
    fn<void *(THISCALL *)(void *)>(0x59a470)(&tmp);   // PartList_GetTemp
    BYTE *list = static_cast<BYTE *>(tmp);
    fn<void(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4ed5e0)(bp(dw(self, 0x48)), list, rect, 3, 0x840000, 0, 0);   // CLayer::FindRect
    const DWORD n = dw(list, 8);
    BYTE found = 0;
    for (DWORD i = 0; i < n && !found; i++) {
        BYTE *p = bp(dw(bp(dw(list, 4)), i * 4));
        if (p == self)
            continue;
        if (vcall<DWORD>(p, 66, self, (DWORD)0) & 0xff)
            continue;
        if (vcall<DWORD>(self, 66, p, (DWORD)0) & 0xff)
            continue;
        if (is_kind_of(p, entity_character_class()))
            continue;
        found = (BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x4fc680)(self, p);   // CLayerPart_OverlapsPart
    }
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x59a510)(&tmp);   // PartList_ReleaseTemp
    return (r & 0xffffff00u) | found;
}
// 0x526570 (slot 104: angle, b): see above
static void THISCALL EntityOm_SetFacing(BYTE *self, DWORD angle, DWORD b)
{
    using SetMovement = void(THISCALL *)(void *, DWORD, DWORD);
    if (!dw(self, 0x48)) {
        fn<SetMovement>(0x48fa00)(self, angle, b);
        return;
    }
    const DWORD old = dw(self, 0xc8);
    if (c3(fld(raw_float(angle)), fld(raw_float(old))))
        return;
    fn<SetMovement>(0x48fa00)(self, angle, b);
    if (!(fn<DWORD(THISCALL *)(void *)>(0x526600)(self) & 0xff))
        return;
    if (dw(self, 0x168))
        action_execute(bp(dw(self, 0x168)), bp(dw(self, 0x48)), self, nullptr, nullptr, self, self + 0x168);
    fn<SetMovement>(0x48fa00)(self, old, 1);
}

ZAX_HOOKS(entity_om)
{
    const Hook list[] = {
#include "entity_om_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
