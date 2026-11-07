// The last Inventory and weapons functions of 0x480000-0x4dffff (phase 3 remains, band B; docs/DECOMPILATION.md
// section 8): CEnergyItemDefinition and its embedded CEnergyMeter 0x486220-0x486cf0, five getters of
// CZaxInventoryItemDefinition 0x486280-0x4862c0, and the firing of CDischargeWeaponThisFrameBaseAction
// 0x4c4e00-0x4c5270 (its registration is in action_classes.cpp, the setter 0x4c4dd0 in action_slots2.cpp).
//
// CEnergyItemDefinition (0x174, vtable 0x5f1b4c, class 0x6bdcd8, version 1, parent CZaxInventoryItemDefinition)
// [confirmed: Create, RegisterFields; checked]: the definition (0x100 bytes) with "Display/Energy Meter"
// (+0x100, a CEnergyMeter, 0x68 bytes) and "Display/Spacing" +0x168 (10), "Display/Width" +0x16c (50),
// "Display/Height" +0x170 (10). CEnergyMeter (0x68, vtable 0x5f1be0, class 0x6bdce0, version 2, parent
// CObject): six colours (field colours 0x10 bytes) "Line Dim Color" +8 (0, 0, 128), "Line Bright Color" +0x18
// (0, 0, 255), "Charged Dim Color" +0x28 (black), "Charged Bright Color" +0x38 (white), "Under Charged Dim
// Color" +0x48 and "Under Charged Bright Color" +0x58 (the constructor copies the charged ones into them, all
// but the colour's bytes 7 and 0xb; RegisterFields gives the defaults of the two under charged ones as black
// and white too). The meter's drawing 0x486d10 is Rendering.
//
// The getters (slots 31-34, 36 of the item definitions): "Display/HUD Image" +0xd4, "Display/HUD Renderer"
// +0xd8, "Display/Detail Image" +0xd0, "Display/Detail Renderer" +0xdc, "Generate/Max Count" +0xf0.
//
// The weapon's firing [confirmed: code; checked] (the action: +8/+0xc the muzzle offset, +0x10 the height
// added, +0x18 the elevation angle, +0x1c "play the shoot sequence"):
//  - Slot 27 0x4c5270 (layer, part, a3, a4; al): the part's root must be a CEntityCharacter and slot 29 must
//    give the weapon's item (else the error reporter complains); slot 28 (layer, part, a3, a4, item,
//    definition) may do it all (CZaxDischargeWeaponThisFrameAction). The angle is the part's facing (slot
//    46), the aiming angle +0xc4 of an animated part. A definition whose slot 23 (-1) is above 0 strikes
//    (0x4c4e00) and returns; otherwise one projectile (no "Projectiles" +0x40) or one per projectile entry
//    (+0x44, 0x14 bytes: +8 the spread in degrees, +0xc/+0x10 the offset) is fired, and then the item's slot
//    23 runs, the definition's sound (+0xc, CLayerPart_LayerAdd) plays and a CWeaponDischargedMessage goes
//    to the parts in 500 pixels (CLayerPart_FindInRect, flag 0x800000).
//  - The strike 0x4c4e00 (layer, root, definition, angle, three unused; al 1): the muzzle offset turned by
//    the angle snapped to the root's rotation (y times 0.712766) at the root's position; the definition's
//    "Strike Damage Type" (+0x38) within its "Strike Damage Radius" (+0x3c), the damage from its slot 23
//    (DamageRadius_Query).
//  - The projectile 0x4c4ee0 (layer, root, definition, angle, spread, dx, dy; al: fired): the definition's
//    projectile model (+0x20, an entity of class CEntityProjectile at least, else the error reporter) made by
//    the root, the shoot sequence when the action wants it (0x49b1a0), the muzzle offset plus (dx, dy)
//    turned like the strike's, the direction angle + spread * pi/180, the height the root's plus +0x10, the
//    projectile centred on the root and aimed at the muzzle, an explosion point broadcast for a definition
//    with an "Explosion" (+0x24), "Idle", the movement angle, flags (0x80000 set, 0x200000 cleared), added to
//    the layer and activated; a projectile of an enemy (flags & 0x6c29a0) aims at the first part in front
//    (CLayer::FindInFront, 30 to 500 pixels) with the root made passable meanwhile. The velocity is the
//    model's "Move/Max Velocity" in the direction (elevation cos/sin), plus the root's velocity (slot 117)
//    for the player's projectiles; then the "New Shot Action" (+0x54) runs with the projectile.
//
// Floating point follows the original instruction by instruction (x87.h); floats the exe passes with
// integer moves are passed as their bits. Return values: the original's eax where callers can see it (the
// getters, Create, CreateArray, the destructors, RegisterFields); al for the firing.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- CEnergyMeter -------------------------------------------------------------------------------------------

void color_ctor(BYTE *c) { fn<void *(THISCALL *)(void *)>(0x53f000)(c); }
// A field colour copied as the exe copies it: the dword, three bytes, the word, one byte, the dword
void copy_color(BYTE *to, const BYTE *from)
{
    dw(to, 0) = dw(const_cast<BYTE *>(from), 0);
    to[4] = from[4];
    to[5] = from[5];
    to[6] = from[6];
    to[8] = from[8];
    to[9] = from[9];
    to[0xa] = from[0xa];
    dw(to, 0xc) = dw(const_cast<BYTE *>(from), 0xc);
}

// 0x486c20: the constructor
BYTE *THISCALL EnergyMeter_Ctor(BYTE *o)
{
    obj_ctor(o);
    for (DWORD off = 8; off <= 0x58; off += 0x10)
        color_ctor(o + off);
    dw(o, 0) = vt(0x5f1be0);
    fn<void(THISCALL *)(void *)>(0x53f060)(o + 0x28);   // FieldColor_SetBlack
    copy_color(o + 0x48, o + 0x28);
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(o + 0x38, 0xff, 0xff, 0xff, 0xff);   // FieldColor_SetRGBA
    copy_color(o + 0x58, o + 0x38);
    obj_init_fields(o);
    return o;
}
void meter_ctor(BYTE *o) { fn<BYTE *(THISCALL *)(void *)>(0x486c20)(o); }
// 0x486cf0: the destructor
void THISCALL EnergyMeter_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1be0));
    obj_dtor(o);
}
void meter_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x486cf0)(o); }

// 0x486840 (slot 2)
CClassInfo *__cdecl EnergyMeter_ClassInfo()
{
    return class_getter(0x6bdce0, 0x612370 /* "CEnergyMeter" */, 2.0f, code<Getter>(0x4163b0) /* CObject */, 0x68, 0, 0x486740, 0x486760, 0x4868a0);
}
// 0x486740: Create (a tail jump to the constructor in the original)
void *__cdecl EnergyMeter_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x68));
    if (!o)
        return nullptr;
    return fn<BYTE *(THISCALL *)(void *)>(0x486c20)(o);
}
// 0x486760: CreateArray (fastcall count; one unused stack argument)
void *__fastcall EnergyMeter_CreateArray(int count, int, int) { return create_array<0x68, meter_ctor>(count); }
// 0x4867b0 (slot 1)
void *THISCALL EnergyMeter_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x68, meter_dtor>(self, flags); }
// 0x4868a0. eax: the last call's.
DWORD __cdecl EnergyMeter_RegisterFields()
{
    BYTE black[0x10], white[0x10], dark_blue[0x10], blue[0x10];
    field_color_rgba(pv(black), 0, 0, 0, 0xff);
    field_color_rgba(pv(white), 0xff, 0xff, 0xff, 0xff);
    field_color_rgba(pv(dark_blue), 0, 0, 0x80, 0xff);
    field_color_rgba(pv(blue), 0, 0, 0xff, 0xff);
    constexpr DWORD COLOR_FIELD_CTOR = 0x46dcc0;   // 0x3c bytes
    struct Field {
        GlobKey name;
        DWORD offset;
        const BYTE *color;
    };
    const Field fields[] = {{0x6123f4 /* "Line Dim Color" */, 8, dark_blue},
                            {0x6123e0 /* "Line Bright Color" */, 0x18, blue},
                            {0x6123cc /* "Charged Dim Color" */, 0x28, black},
                            {0x6123b4 /* "Charged Bright Color" */, 0x38, white},
                            {0x61239c /* "Under Charged Dim Color" */, 0x48, black},
                            {0x612380 /* "Under Charged Bright Color" */, 0x58, white}};
    DWORD r = 0;
    for (const Field &d : fields) {
        void *f = new_field(0x3c, COLOR_FIELD_CTOR, glob<const char *>(d.name), d.offset, 0x10, d.offset, 1, d.color, 0);
        r = add_field(EnergyMeter_ClassInfo(), f);
    }
    return r;
}

// ---- CEnergyItemDefinition ------------------------------------------------------------------------------------

// 0x486220, 0x486440 (slot 2)
CClassInfo *__cdecl EnergyItemDefinition_ClassInfo()
{
    return class_getter(0x6bdcd8, 0x612310 /* "CEnergyItemDefinition" */, 1.0f, code<Getter>(0x5b5f10) /* CZaxInventoryItemDefinition */, 0x174, 0,
                        0x4862d0, 0x486310, 0x4864b0);
}
void energy_def_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x5b7380)(o);   // CZaxInventoryItemDefinition's constructor
    meter_ctor(o + 0x100);
    vtable_and_defaults(o, vt(0x5f1b4c));
}
void energy_def_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1b4c));
    meter_dtor(o + 0x100);
    fn<void(THISCALL *)(void *)>(0x5b73f0)(o);   // CZaxInventoryItemDefinition's destructor
}
// 0x4862d0, 0x486310 (fastcall count), 0x486390 (slot 1)
void *__cdecl EnergyItemDefinition_Create() { return create_one<0x174, energy_def_construct>(); }
void *__fastcall EnergyItemDefinition_CreateArray(int count, int, int) { return create_array<0x174, energy_def_construct>(count); }
void *THISCALL EnergyItemDefinition_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x174, energy_def_destroy>(self, flags); }
// 0x4864b0. eax: the last call's.
DWORD __cdecl EnergyItemDefinition_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x612358) /* "Display/Energy Meter" */, 0x100, 0x68, 0x100, 1);
    add_field(EnergyItemDefinition_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x612348) /* "Display/Spacing" */, 0x168, 4, 0x168, 1, 0xa, 0, 0, 0);
    add_field(EnergyItemDefinition_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x612338) /* "Display/Width" */, 0x16c, 4, 0x16c, 1, 0x32, 0, 0, 0);
    add_field(EnergyItemDefinition_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x612328) /* "Display/Height" */, 0x170, 4, 0x170, 1, 0xa, 0, 0, 0);
    return add_field(EnergyItemDefinition_ClassInfo(), f);
}

// ---- CZaxInventoryItemDefinition's getters ------------------------------------------------------------------

DWORD THISCALL ZaxItemDef_HudImage(BYTE *d) { return dw(d, 0xd4); }         // 0x486280 (slot 31)
DWORD THISCALL ZaxItemDef_HudRenderer(BYTE *d) { return dw(d, 0xd8); }      // 0x486290 (slot 32)
DWORD THISCALL ZaxItemDef_DetailImage(BYTE *d) { return dw(d, 0xd0); }      // 0x4862a0 (slot 33)
DWORD THISCALL ZaxItemDef_DetailRenderer(BYTE *d) { return dw(d, 0xdc); }   // 0x4862b0 (slot 34)
DWORD THISCALL ZaxItemDef_MaxCount(BYTE *d) { return dw(d, 0xf0); }         // 0x4862c0 (slot 36)

// ---- The weapon's firing ----------------------------------------------------------------------------------------

void *reporter_() { return *glob<void **>(G_REPORTER); }
void report(DWORD text) { vcall<DWORD>(reporter_(), 17, at<const char *>(text)); }
BYTE *sequence_object(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
BYTE *entity_model(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
CClassInfo *projectile_class() { return fn<Getter>(0x491930)(); }
// 0x508fe0 (fastcall out, out; angle): the table's two values of the angle
void sin_cos(float *a, float *b, DWORD angle) { fn<void(__fastcall *)(float *, float *, DWORD)>(0x508fe0)(a, b, angle); }
long double table_508ec0(DWORD a) { return fn<long double(__stdcall *)(DWORD)>(0x508ec0)(a); }
long double table_508da0(DWORD a) { return fn<long double(__stdcall *)(DWORD)>(0x508da0)(a); }
constexpr DWORD C_DEG_TO_RAD = 0x5edb3c;   // pi / 180

// The exe's operand order (with two NaNs it decides the result): st0 op a float in memory (fadd/fmul m32),
// and st0 = dest op src (fadd st, st(1)); clang could commute a plain `a + b`
long double add_m(long double st, const float &m)
{
    __asm__("fadds %1" : "+t"(st) : "m"(m));
    return st;
}
long double mul_m(long double st, const float &m)
{
    __asm__("fmuls %1" : "+t"(st) : "m"(m));
    return st;
}
long double add_st(long double dest, long double src)
{
    __asm__("fadd %%st(1), %%st" : "+t"(dest) : "u"(src));
    return dest;
}
const float &cfl(DWORD address) { return *at<const float *>(address); }

// The angle snapped to the rotation of the root's sequence, negated (the argument of the sin/cos table)
DWORD snapped_negated(BYTE *root, DWORD angle)
{
    BYTE *seq = sequence_object(root);
    const DWORD rotation = fn<DWORD(THISCALL *)(void *, DWORD)>(0x55fdd0)(seq, angle);   // CRotationSequence_AngleToRotation
    return float_bits(fstp(-fn<long double(THISCALL *)(void *, DWORD)>(0x55fe90)(seq, rotation)));   // CRotationSequence_RotationToAngle
}

// 0x4c4e00 (layer, root, definition, angle, three unused): the strike. al 1, the rest of eax DamageRadius_Query's.
DWORD THISCALL Weapon_Strike(BYTE *self, BYTE *layer, BYTE *root, BYTE *def, DWORD angle, DWORD, DWORD, DWORD)
{
    float p[4];
    part_position(root, p);
    const DWORD a = snapped_negated(root, angle);
    const float ox = fstp(fld(fl<float>(self, 8))), oy = fstp(fld(fl<float>(self, 0xc)));
    float s1, s2;
    sin_cos(&s1, &s2, a);
    const long double x = add_st(mul_m(fld(s1), oy), mul_m(fld(s2), ox));
    const long double y = add_st(mul_m(-fld(s1), ox), mul_m(fld(s2), oy));
    const DWORD radius = dw(def, 0x3c) /* "Strike Damage Radius" */, damage_type = dw(def, 0x38) /* "Strike Damage Type" */;
    const float y2 = fstp(mul_m(y, cfl(glob_addr(C_OVAL_RATIO))));
    float pt[2];
    pt[0] = fstp(add_st(fld(p[0]), x));
    pt[1] = fstp(add_m(fld(p[1]), y2));
    const float damage = fstp(vcall<long double>(def, 23, 0xbf800000));
    const DWORD r = fn<DWORD(__fastcall *)(void *, float *, void *, DWORD, DWORD, DWORD, void *, DWORD, DWORD)>(0x477640)(
        layer, pt, root, radius, float_bits(damage), damage_type, root, 0, 0);   // DamageRadius_Query
    return (r & ~0xffu) | 1;
}

// 0x4c4ee0 (layer, root, definition, angle, spread, dx, dy): a projectile. al: fired; the rest of eax the
// error report's, the action's or the address of the definition's +0x54.
DWORD THISCALL Weapon_FireProjectile(BYTE *self, BYTE *layer, BYTE *root, BYTE *def, DWORD angle, DWORD spread, DWORD dx, DWORD dy)
{
    BYTE *proj = fn<BYTE *(THISCALL *)(void *, DWORD, CClassInfo *, void *, DWORD, int)>(0x5176f0)(glob<void *>(0x6cfdd8), dw(def, 0x20), projectile_class(),
                                                                                                root, 0, -1);   // Entity_CreateFromModel
    if (!is_kind_of(proj, projectile_class())) {
        // "Could not fire weapon because the projectile model does not have a projectile entity type"
        return vcall<DWORD>(reporter_(), 17, glob<const char *>(0x614ebc)) & ~0xffu;
    }
    float pos[4];
    part_position(root, pos);
    float off[2];
    off[0] = fstp(fld(fl<float>(self, 8)));
    off[1] = fstp(fld(fl<float>(self, 0xc)));
    if (self[0x1c]) {
        const DWORD name = fn<DWORD(THISCALL *)(void *, DWORD)>(0x4d6540)(def, 0);   // CInventoryItemDefinition::ShootSequence
        fn<void(THISCALL *)(void *, float *, DWORD)>(0x49b1a0)(root, off, name);   // EntityCharacter_SetDischarge
    }
    const float fdx = raw_float(dx), fdy = raw_float(dy), fangle = raw_float(angle), fspread = raw_float(spread);
    off[0] = fstp(add_m(fld(off[0]), fdx));
    off[1] = fstp(add_m(fld(off[1]), fdy));
    const DWORD a = snapped_negated(root, angle);
    float s1, s2;
    sin_cos(&s1, &s2, a);
    const long double x = add_st(mul_m(fld(s1), off[1]), mul_m(fld(s2), off[0]));
    const float y = fstp(add_st(mul_m(-fld(s1), off[0]), mul_m(fld(s2), off[1])));
    off[0] = fstp(x);
    off[1] = y;
    float c1, c2;
    sin_cos(&c1, &c2, float_bits(fstp(-fld(fangle))));   // its values are not used
    off[1] = fstp(mul_m(fld(off[1]), cfl(glob_addr(C_OVAL_RATIO))));
    float start[2];
    start[0] = fstp(add_m(fld(pos[0]), off[0]));
    start[1] = fstp(add_m(fld(pos[1]), off[1]));
    const DWORD dir = float_bits(fstp(add_m(mul_m(fld(fspread), cfl(glob_addr(C_DEG_TO_RAD))), fangle)));
    const float lift = fstp(table_508ec0(dw(self, 0x18)));
    const float level = fstp(table_508da0(dw(self, 0x18)));
    float vx = fstp(mul_m(table_508da0(dir), level));
    float vy = fstp(mul_m(table_508ec0(dir), level));
    const float height = fl<float>(self, 0x10);
    const float h = fstp(add_m(fild(vcall<int>(root, 54)), height));
    fn<void(THISCALL *)(void *, int)>(0x48b6b0)(proj, x87_fistp(fld(h)));   // Entity_SetRenderHeight
    fn<void(THISCALL *)(void *, float *)>(0x4fb5e0)(proj, pos);              // CLayerPart_MoveCenterTo
    const float ty = fstp(add_m(fld(off[1]), fl<float>(proj, 0x104)));
    const float tx = fstp(add_m(fld(off[0]), fl<float>(proj, 0x100)));
    fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x491230)(proj, float_bits(tx), float_bits(ty));   // EntityMovable_SetTarget
    if (const DWORD explosion = dw(def, 0x24)) {
        const DWORD ph = vcall<DWORD>(proj, 54);
        fn<void(THISCALL *)(void *, void *, DWORD, float *, DWORD, DWORD, DWORD)>(0x4f61c0)(world(), layer, explosion, start, ph, 1, dir);   // World::BroadcastExplosionPoint
    }
    ent_play_sequence(proj, glob_addr(0x60f8d0) /* "Idle" */);
    vcall<DWORD>(proj, 104, dir, 1);
    slot_set_flags(proj, 0x80000);
    slot_clear_flags(proj, 0x200000);
    fn<void(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, proj);   // CLayer::AddPart
    vcall<DWORD>(proj, 35, 0);
    const DWORD flags = dw(root, 0x1c);
    const bool enemy = (flags & *glob<DWORD *>(0x6c29a0)) != 0;
    if (enemy) {
        const DWORD passable = flags & 0x2000000;
        float from[2] = {pos[0], pos[1]};
        slot_set_flags(root, 0x2000000);
        BYTE *target = fn<BYTE *(THISCALL *)(void *, void *, float *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4eb200)(
            layer, proj, from, dir, 0x41f00000 /* 30 */, 0x43fa0000 /* 500 */, 0x940000, *glob<DWORD *>(0x6c29bc) | 0x2000000);   // CLayer::FindInFront
        if (target) {
            const DWORD heading = float_bits(fstp(fn<long double(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4fb2d0)(proj, target, 0, 0x3f800000)));   // CLayerPart_AngleFromPart
            vx = fstp(mul_m(table_508da0(heading), level));
            vy = fstp(mul_m(table_508ec0(heading), level));
        }
        if (!passable)
            slot_clear_flags(root, 0x2000000);
    }
    const long double speed = fld(fl<float>(entity_model(proj), 0x60));   // "Move/Max Velocity"
    vx = fstp(mul_m(speed, vx));
    vy = fstp(mul_m(speed, vy));
    const float vz = fstp(mul_m(fld(fl<float>(entity_model(proj), 0x60)), lift));
    float rv[2];
    vcall<DWORD>(root, 117, rv);
    float v[3];
    if (!enemy) {
        const long double x2 = add_m(fld(rv[0]), vx);
        vy = fstp(add_m(fld(rv[1]), vy));
        v[0] = fstp(x2);
    } else
        v[0] = vx;
    v[1] = fstp(fld(vy));
    v[2] = vz;
    vcall<DWORD>(proj, 107, v);
    DWORD r = pv(def + 0x54);
    if (dw(def, 0x54))
        r = action_execute(bp(dw(def, 0x54)), layer, proj, nullptr, root, def, def + 0x54);
    return (r & ~0xffu) | 1;
}

// 0x4c5270 (slot 27: layer, part, a3, a4): see above
DWORD THISCALL Weapon_DischargeThisFrame(BYTE *self, BYTE *layer, BYTE *part, DWORD a3, DWORD a4)
{
    BYTE *root = fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(part);   // CLayerPart_Root
    if (!root || !is_kind_of(root, fn<Getter>(0x49a930)() /* CEntityCharacter */)) {
        report(glob_addr(0x614f68));   // "The DischargeWeaponThisFrame Action should only be triggered by a Entity CEntityCharacter or Higher"
        return 0;
    }
    BYTE *item = vcall<BYTE *>(self, 29, part);
    if (!item) {
        report(glob_addr(0x614f18));   // "The object that you tried to use has no inventory, or has no weapon to fire."
        return 0;
    }
    BYTE *def = fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item);   // ItemRef_Definition
    if (vcall<DWORD>(self, 28, layer, part, a3, a4, item, def) & 0xff)
        return 1;
    const DWORD count = dw(def, 0x40);
    DWORD angle = float_bits(fstp(vcall<long double>(part, 46)));
    if (part && is_kind_of(part, entity_animated_class()))
        angle = dw(part, 0xc4);
    if (!c03(vcall<long double>(def, 23, 0xbf800000), cst(glob_addr(C_ZERO))))
        return fn<DWORD(THISCALL *)(void *, void *, void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4c4e00)(self, layer, root, def, angle, 0, 0, 0);
    using Fire = DWORD(THISCALL *)(void *, void *, void *, void *, DWORD, DWORD, DWORD, DWORD);
    if (!count) {
        if (!(fn<Fire>(0x4c4ee0)(self, layer, root, def, angle, 0, 0, 0) & 0xff))
            return 0;
    } else {
        for (DWORD i = 0; i < count; i++) {
            const DWORD shots = dw(def, 0x44);
            BYTE *shot = shots && i < dw(def, 0x40) ? bp(shots + i * 0x14) : nullptr;
            if (!(fn<Fire>(0x4c4ee0)(self, layer, root, def, angle, dw(shot, 8), dw(shot, 0xc), dw(shot, 0x10)) & 0xff))
                return 0;
        }
    }
    vcall<DWORD>(item, 23, part);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fc8a0)(root, dw(def, 0xc));   // CLayerPart_LayerAdd
    const DWORD target = vcall<DWORD>(root, 70);
    DWORD msg[5];
    fn<void *(THISCALL *)(void *, void *, DWORD)>(0x599300)(msg, root, target);   // CWeaponDischargedMessage::Ctor
    fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4fb910)(root, msg, 0x43fa0000 /* 500 */, 0xbf800000, 0x800000, 0, 0);   // CLayerPart_FindInRect
    return 1;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(inventory_rest_b)
{
    const Hook list[] = {
#include "inventory_rest_b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
