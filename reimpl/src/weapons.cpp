// The continuous fire weapons of 0x472440-0x4776b0 (phase 3, inventory and weapons): their data object
// CContinuousFireWeaponData, the item CContinuousFireWeaponItem, the AIs that fire them
// (CFireContinuousFireWeaponBaseAI and its lightning and particle variants CFireElectricalWeaponAI and
// CFireParticleWeaponAI), the energy ammunition CEnergyAmmoItem, the expanding damage radius
// CDamageExpandingRadiusAI, the damage message 0x476450 and the damage in a radius 0x477640 with its
// callback 0x477510 (docs/DECOMPILATION.md section 8).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CContinuousFireWeaponData (0x20, vtable 0x5f03e4, class 0x6bd6f0, no parent; the item definition's
//     +0xcc): +8 "Start Shooting Continuous Action", +0xc "Finish Shooting Continuous Action", +0x10
//     "Impact Exposion" (sic; the projectile list 0x6cfdd8), +0x14 "Shoot Spread Angle" (radians), +0x18
//     "Extra Close Range Strike Damage", +0x1c "Looping Discharge Sound" (the sound list 0x713708)
//   CContinuousFireWeaponItem (0x20, vtable 0x5f0440, class 0x6bd6f4, parent CInventoryItem): +0x18
//     "Debug/Muzzel Flash Entity" (the impact entity while it fires), +0x1c "Debug/Gun Owner"
//   CFireContinuousFireWeaponBaseAI (0x34, vtable 0x5f050c, class 0x6bd6f8, parent CLayerPartAI): +0x10
//     "Debug/Gun Item", +0x14 "Debug/Muzzel Flash Entity" (the part it listens to), +0x18 "Weapon
//     Definition Index" (the item catalogue 0x6c0c08), +0x1c "Debug/Impact Explosion", +0x20 "Damaged
//     Message Name", +0x24 "Score Every" (seconds), +0x28 the looping sound (-1: none), +0x2c its channel
//     (0xff), +0x30 "Debug/Updates Since I Got IMoved Message"
//   CFireElectricalWeaponAI (0x40, vtable 0x5f05bc, class 0x6bd6fc): +0x34 "Lightning To Use" (a
//     CLightningLayerPart template), +0x38 "Debug/Lightning Path" (its clone, a child of the part), +0x3c
//     "Clipped Message Name"
//   CFireParticleWeaponAI (0x40, vtable 0x5f0668, class 0x6bd700): +0x34 "Particle System" (an AI
//     template), +0x38 "Debug/Particle System On Muzzel Flash" (its clone, an AI of the muzzle flash),
//     +0x3c "Clipped Message Name"
//   CEnergyAmmoItem (0x1c, vtable 0x5f0714, class 0x6bd704, parent CEnergyItem, no fields of its own)
//   CDamageExpandingRadiusAI (0x34, vtable 0x5f08c8, class 0x6bd74c, version 5): +0x10 "Damage Creator",
//     +0x11 "Use Animation For Radius", +0x14 "Damage" (units/sec), +0x18/+0x1c "Start Radius"/"End
//     Radius", +0x20 "YX Ratio", +0x24 "Start To End Time", +0x28 the radius now, +0x2c the time so far,
//     +0x30 "Damage Type" (the damage type list 0x6bd760)
//   the damage message 0x476450 (0x1c, vtable 0x5f08c4): +4 the message id 0x6bd708, +8 0, +0xc..+0x14
//     the three arguments, +0x18 the third as a CEntityCharacter or 0
//
// Constructors are inlined into Create and CreateArray one level per class, destructors into the
// deleting destructors (slot 1); the AIs' common destructor level is the function 0x4734e0. The class
// getters are inlined into RegisterFields (the reimplementation calls the getter, which does the same);
// where the original fetches a descriptor's argument after operator new, so does the reimplementation.
// Floating point follows the original instruction by instruction (x87.h). Return values: eax where the
// slot returns one, al for the bool slots (the checks compare al).
// Left out: CFireContinuousFireWeaponBaseAI's debug drawing (slot 41 0x474320: the beam polygon in a
// colour; Rendering), CDamageExpandingRadiusAI's (slot 41 0x476d00: three ovals; Rendering), the beam
// helpers' unhookable slot 42 of the base AI (0x472f20, `ret 0x1c`, 3 bytes) and the HUD and detail
// drawing 0x475eb0-0x475ff0 (User interface).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe; through their exe addresses so Disable= and the checks' stand-ins apply ----

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD CATALOGUE = 0x6c0c08;                        // the item catalogue (definition list)
constexpr DWORD CAT_RECORDS = 0x6c0c38, CAT_COUNT = 0x6c0c3c;   // its records (0x80 bytes each) and count
constexpr DWORD G_APP = 0x713f14, G_CATEGORY_PLAYER = 0x6c29a0;
constexpr DWORD MSG_DEACTIVATE_AI = 0x6c29e8, MSG_6C29D4 = 0x6c29d4;
constexpr DWORD C_MINUS_ONE = 0x5eeccc, C_YX = 0x5ee9b8;

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
// fchs where the original has it (the compiler may otherwise move a negation onto another factor,
// which gives a NaN the other sign)
long double fchs(long double v)
{
    __asm__("fchs" : "+t"(v));
    return v;
}
// a * b + c * d with the original's instructions (fld a, fmul b, fld c, fmul d, faddp): with two NaNs of
// the same significand the operand order decides the sign
long double dot2(const float &a, const float &b, const float &c, const float &d)
{
    long double r;
    __asm__("flds %1\n\tfmuls %2\n\tflds %3\n\tfmuls %4\n\tfaddp" : "=t"(r) : "m"(a), "m"(b), "m"(c), "m"(d));
    return r;
}
// (-a * b + c * d) * e the same way (fld a, fchs, fmul b, fld c, fmul d, faddp, fmul e)
long double ndot2_scaled(const float &a, const float &b, const float &c, const float &d, const float &e)
{
    long double r;
    __asm__("flds %1\n\tfchs\n\tfmuls %2\n\tflds %3\n\tfmuls %4\n\tfaddp\n\tfmuls %5" : "=t"(r) : "m"(a), "m"(b), "m"(c), "m"(d), "m"(e));
    return r;
}
BYTE *pt(void *p, DWORD off) { return bp(dw(p, off)); }
CClassInfo *character_class() { return zax::fn<Getter>(0x49a930)(); }
CClassInfo *destructable_class() { return zax::fn<Getter>(0x48bdf0)(); }
CClassInfo *zax_def_class() { return zax::fn<Getter>(0x5b5f10)(); }
CClassInfo *inventory_item_class() { return zax::fn<Getter>(0x429e50)(); }
CClassInfo *energy_item_class() { return zax::fn<Getter>(0x5afbd0)(); }
BYTE *root(void *part) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(part); }
BYTE *inventory_of(void *part) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(part); }
BYTE *definition_of(void *item) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item); }
BYTE *definition_of2(void *item) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x42a330)(item); }   // the jump to 0x4dd480
BYTE *def_list_object(DWORD index) { return zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(CATALOGUE), index); }
BYTE *energy_source(void *def, void *inv) { return zax::fn<BYTE *(THISCALL *)(void *, void *)>(0x5b7920)(def, inv); }
DWORD def_start_action(void *def) { return zax::fn<DWORD(THISCALL *)(void *)>(0x5b7670)(def); }
DWORD def_finish_action(void *def) { return zax::fn<DWORD(THISCALL *)(void *)>(0x5b76a0)(def); }
DWORD def_impact(void *def) { return zax::fn<DWORD(THISCALL *)(void *)>(0x5b77a0)(def); }
long double def_spread(void *def) { return zax::fn<long double(THISCALL *)(void *)>(0x5b78f0)(def); }
DWORD action_execute_from(DWORD action, void *layer, void *part, DWORD a3, DWORD a4, void *caller)
{
    return zax::fn<DWORD(THISCALL *)(DWORD, void *, void *, DWORD, DWORD, void *)>(0x41a740)(action, layer, part, a3, a4, caller);
}
void *world() { return *glob<void **>(G_WORLD); }
void *reporter() { return *glob<void **>(G_REPORTER); }
void report(DWORD static_text) { vcall<void>(reporter(), 17, zs_c_str(at<void *>(static_text))); }
BYTE *current_layer() { return zax::fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(world()); }
DWORD send_sound_at(void *layer, DWORD sound, void *part, DWORD mode, DWORD keep, DWORD channel)
{
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD, DWORD)>(0x4eaa60)(layer, sound, part, mode, keep, channel);
}
DWORD send_sound(void *layer, DWORD sound, const float *pos, DWORD mode, DWORD keep, DWORD channel)
{
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, const float *, DWORD, DWORD, DWORD)>(0x4eaab0)(layer, sound, pos, mode, keep, channel);
}
void add_listener(void *part, void *listener) { zax::fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, listener, -1, glob<const char *>(TEXT_EMPTY)); }
void remove_listener(void *part, void *listener) { zax::fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, listener, -1, glob<const char *>(TEXT_EMPTY)); }
void ai_add_listener(void *ai, void *listener) { zax::fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fd190)(ai, listener, -1, glob<const char *>(TEXT_EMPTY)); }
void ai_remove_listener(void *ai, void *listener) { zax::fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fd1e0)(ai, listener, -1, glob<const char *>(TEXT_EMPTY)); }
DWORD died(void *part) { return zax::fn<DWORD(THISCALL *)(void *)>(0x4fbf30)(part); }
void add_child(void *part, void *child) { zax::fn<void(THISCALL *)(void *, void *)>(0x4fc710)(part, child); }
void add_ai(void *part, void *ai) { zax::fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(part, ai); }
void remove_part(void *layer, void *part) { zax::fn<void(THISCALL *)(void *, void *)>(0x4ebcb0)(layer, part); }
void *object_clone(void *o, DWORD a, DWORD b) { return zax::fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, a, b); }
BYTE *spawn_explosion(void *layer, DWORD what, DWORD x, DWORD y, DWORD a5, DWORD sound, DWORD a7, DWORD with_ai, void *creator)
{
    return zax::fn<BYTE *(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void *)>(0x4f6530)(
        world(), layer, what, x, y, a5, sound, a7, with_ai, creator);
}
DWORD message_id(const char *name) { return zax::fn<DWORD(__fastcall *)(const char *)>(0x506680)(name); }
DWORD part_ai_message(void *ai, void *part, void *msg) { return zax::fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4fd100)(ai, part, msg); }
DWORD obj_message(void *o, void *other, const char *name) { return zax::fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x46cd50)(o, other, name); }
long double table_sqrt(float v) { return zax::fn<long double(__stdcall *)(float)>(0x508c50)(v); }
// 0x508fe0 (ecx, edx: the two outputs; the angle)
void sin_cos(float *c, float *d, float a) { zax::fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(c, d, a); }
long double random_between(FnKey f, DWORD lo, DWORD hi) { return fn<long double(THISCALL *)(void *, DWORD, DWORD)>(f)(glob<void *>(GAME_RANDOM), lo, hi); }
void poly_ctor(void *p) { zax::fn<void *(THISCALL *)(void *)>(0x545cc0)(p); }
void poly_dtor(void *p) { zax::fn<void(THISCALL *)(void *)>(0x545cf0)(p); }
void poly_clear(void *p) { zax::fn<void(THISCALL *)(void *)>(0x545e30)(p); }
void poly_insert(void *p, DWORD x, DWORD y, int after) { zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, int)>(0x5466c0)(p, x, y, after); }
void poly_close(void *p) { zax::fn<void(THISCALL *)(void *)>(0x546810)(p); }
// A message on the stack (vtable 0x5eeb64, the id, a byte 0), as the slots send them (slot 63)
struct StackMessage {
    DWORD vtable, id;
    BYTE flag;
};
// The game type of the application (0x59ff80) and the quest statistic 0xe (the accuracy: shots and hits)
BYTE *quest_accuracy_stat()
{
    BYTE *gt = zax::fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(G_APP));
    if (!gt || !is_kind_of(gt, zax::fn<Getter>(0x551c20)()))
        return nullptr;
    BYTE *stat = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x552940)(gt, 0xe);
    if (!stat || !is_kind_of(stat, zax::fn<Getter>(0x559e10)()))
        return nullptr;
    return stat;
}
DWORD quest_key() { return dw(pt(zax::fn<BYTE *(__cdecl *)()>(0x5525b0)(), 0xc), 0x38); }
void stat_add_denominator(void *stat, DWORD key, DWORD v) { zax::fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x55ab90)(stat, key, v); }
void stat_add_numerator(void *stat, DWORD key, DWORD v) { zax::fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x55aad0)(stat, key, v); }

// The catalogue's definition of an index as most slots read it (0x48d8f0), when it is a
// CZaxInventoryItemDefinition
BYTE *zax_def(DWORD index)
{
    BYTE *d = def_list_object(index);
    return d && is_kind_of(d, zax_def_class()) ? d : nullptr;
}
// The same with the record lookup inlined: no records, a record being loaded or no object give 0; a
// record without an object is loaded by 0x55e090 first
BYTE *zax_def_inline(DWORD index)
{
    if (!dw(glob<void *>(CAT_RECORDS), 0))
        return nullptr;
    DWORD i = index;
    if (i >= *glob<DWORD *>(CAT_COUNT))
        i = 0;
    const DWORD off = i * 0x80;
    if (*(pt(glob<void *>(CAT_RECORDS), 0) + off + 0x7f))
        return nullptr;
    if (!dw(pt(glob<void *>(CAT_RECORDS), 0) + off, 0x78))
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(CATALOGUE), i);
    BYTE *d = pt(pt(glob<void *>(CAT_RECORDS), 0) + off, 0x78);
    return d && is_kind_of(d, zax_def_class()) ? d : nullptr;
}
// The same with the record's loading inlined too (0x55e090's body): the record's file name (slot 1)
// joined to the list's folder (+0x38), made by slot 37 from the folder part and the name part, the
// loading flag +0x7f set meanwhile
BYTE *zax_def_inline_load(DWORD index)
{
    BYTE *records = pt(glob<void *>(CAT_RECORDS), 0);
    if (!records)
        return nullptr;
    DWORD i = index;
    if (i >= *glob<DWORD *>(CAT_COUNT))
        i = 0;
    const DWORD off = i * 0x80;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78) && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        BYTE *l = glob<BYTE *>(CATALOGUE);
        char path[0x100];
        zax::fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, reinterpret_cast<const char *>(l + 0x38),
                                                                           vcall<const char *>(l, 1, i));
        char *name = zax::fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        void *made;
        if (name == path) {
            made = vcall<void *>(l, 37, glob<const char *>(TEXT_EMPTY), name, pt(glob<void *>(CAT_RECORDS), 0) + off + 0x78);
        } else {
            name[-1] = 0;
            made = vcall<void *>(l, 37, path, name, pt(glob<void *>(CAT_RECORDS), 0) + off + 0x78);
        }
        dw(pt(glob<void *>(CAT_RECORDS), 0) + off, 0x78) = ptrv(made);
        pt(glob<void *>(CAT_RECORDS), 0)[off + 0x7f] = 0;
        records = pt(glob<void *>(CAT_RECORDS), 0);
    }
    BYTE *d = pt(records + off, 0x78);
    return d && is_kind_of(d, zax_def_class()) ? d : nullptr;
}

// A descriptor's constructor on a block the caller got from operator new (for descriptors whose
// arguments the original fetches after operator new: a class getter)
template <typename... A> void *construct_field(void *f, FnKey ctor, A... args)
{
    return f ? zax::fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...) : nullptr;
}
void zs_assign(void *f, DWORD off, GlobKey text) { zs_assign_text(zstring(f, off), glob<const char *>(text)); }
void item_ctor(BYTE *o) { zax::fn<void *(THISCALL *)(void *)>(0x42a2b0)(o); }
void item_dtor(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x42a300)(o); }
// CEnergyItem's destructor 0x5afe30 (zax_items.cpp)
void energy_item_dtor(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x5afe30)(o); }

// The module's class getters through their exe addresses (the reimplementation when hooked)
CClassInfo *data_class() { return zax::fn<Getter>(0x472e80)(); }
CClassInfo *item_class() { return zax::fn<Getter>(0x472880)(); }

// ---- CContinuousFireWeaponData ---------------------------------------------------------------------------------

constexpr DWORD VT_DATA = 0x5f03e4, VT_ITEM = 0x5f0440, VT_BASE_AI = 0x5f050c, VT_ELECTRICAL = 0x5f05bc, VT_PARTICLE = 0x5f0668,
                VT_ENERGY_AMMO = 0x5f0714, VT_ENERGY_ITEM = 0x5f07e0, VT_RADIUS_AI = 0x5f08c8, VT_LAYER_PART_AI = 0x5ed1d0;

// 0x472e80 (also slot 24 of the item: the class of the definition's continuous fire data)
CClassInfo *__cdecl ContinuousFireWeaponData_ClassInfo()
{
    return class_getter(0x6bd6f0, 0x60ff2c /* "CContinuousFireWeaponData" */, 0.0f, nullptr, 0x20, 0, 0x472440, 0x472470, 0x472560);
}
void Data_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_DATA));
}
void Data_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DATA));
    obj_dtor(o);
}
// 0x472440, 0x472470, 0x4724d0 (slot 1)
void *__cdecl Data_Create() { return create_one<0x20, Data_construct>(); }
void *__fastcall Data_CreateArray(int count, int, int) { return create_array<0x20, Data_construct>(count); }
void *THISCALL Data_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, Data_destroy>(self, flags); }
// 0x472560. eax: the last call's.
DWORD __cdecl Data_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    f = construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x60ffd8) /* "Start Shooting Continuous Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    add_field(ContinuousFireWeaponData_ClassInfo(), f);
    f = heap_alloc(0x3c);
    f = construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x60ffb4) /* "Finish Shooting Continuous Action" */, 0xc, 4, 0xc, action_class(), 1, 0, 0, 0);
    add_field(ContinuousFireWeaponData_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60ffa4) /* "Impact Exposion" */, 0x10, 4, 0x10, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(ContinuousFireWeaponData_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x60ff88) /* "Shoot Spread Angle" */, glob_addr(0x60ff9c) /* "Radians" */, 0x14, 4, 0x14, 1,
                  0x3e860a92 /* 0.2617994 */, 0, 0, 0x40490fdb /* 3.1415927 */, 0x3c8efa35 /* 0.017453292 */);
    add_field(ContinuousFireWeaponData_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign(f, 0x20, 0x60ff80 /* "Degrees" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60ff60) /* "Extra Close Range Strike Damage" */, 0x18, 4, 0x18, 1, 0x41700000 /* 15.0 */, 0, 0, 0, 0);
    add_field(ContinuousFireWeaponData_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60ff48) /* "Looping Discharge Sound" */, 0x1c, 4, 0x1c, glob_addr(0x713708), 1, 0, 0, 0);
    return add_field(ContinuousFireWeaponData_ClassInfo(), f);
}

// ---- CContinuousFireWeaponItem ---------------------------------------------------------------------------------

// 0x472880, 0x4729e0 (slot 2)
CClassInfo *__cdecl ContinuousFireWeaponItem_ClassInfo()
{
    return class_getter(0x6bd6f4, 0x60fffc /* "CContinuousFireWeaponItem" */, 0.0f, inventory_item_class, 0x20, 0, 0x4728d0, 0x472900,
                        0x472a40);
}
void Item_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_ITEM));
}
void Item_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_ITEM));
    item_dtor(o);
}
// 0x4728d0, 0x472900, 0x472960 (slot 1)
void *__cdecl Item_Create() { return create_one<0x20, Item_construct>(); }
void *__fastcall Item_CreateArray(int count, int, int) { return create_array<0x20, Item_construct>(count); }
void *THISCALL Item_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, Item_destroy>(self, flags); }
// 0x472a40. eax: the last call's.
DWORD __cdecl Item_RegisterFields()
{
    void *f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x610028) /* "Debug/Muzzel Flash Entity" */, 0x18, 4, 0x18, class_at(fn_addr(0x487620)), 3, 0);
    add_field(ContinuousFireWeaponItem_ClassInfo(), f);
    f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x610018) /* "Debug/Gun Owner" */, 0x1c, 4, 0x1c, entity_animated_class(), 3, 0);
    return add_field(ContinuousFireWeaponItem_ClassInfo(), f);
}
// 0x472b50 (slot 38, may the part fire it): a character for which slot 37 allows it; one with the flag
// 0x10000000 also needs an energy source (the definition's 0x5b7920 with its inventory) with charge above
// 0. al.
DWORD THISCALL Item_MayFire(BYTE *self, BYTE *part)
{
    if (!part || !is_kind_of(part, character_class()))
        return 0;
    if (!(vcall<DWORD>(self, 37, part) & 0xff))
        return 0;
    if (!(dw(part, 0x1c) & 0x10000000))
        return 1;
    BYTE *def = definition_of(self);
    if (def && !is_kind_of(def, zax_def_class()))
        def = nullptr;
    BYTE *inv = inventory_of(part);
    if (!def || !energy_source(def, inv))
        return 0;
    return c03(fld(fl<float>(energy_source(def, inv), 0x18)), cst(glob_addr(C_ZERO))) ? 0 : 1;
}
// 0x472bf0 (slot 44): CInventoryItem's select. eax: its.
DWORD THISCALL Item_Select(BYTE *self, DWORD a) { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x42a370)(self, a); }
// 0x472c00 (slot 45): a firing weapon stops (slot 50 with its owner), then CInventoryItem's deselect
DWORD THISCALL Item_Deselect(BYTE *self, DWORD a)
{
    if (dw(self, 0x18))
        vcall<DWORD>(self, 50, dw(self, 0x1c));
    return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x42a380)(self, a);
}
// 0x472c30 (slot 34, a message): the message 0x6bde98 about its owner's root part stops a firing
// weapon (slot 50; al 1); other messages go to CInventoryItem's slot 34
DWORD THISCALL Item_Message(BYTE *self, DWORD a, BYTE *msg)
{
    if (dw(msg, 4) == *glob<DWORD *>(0x6bde98) && dw(self, 0x18) && dw(msg, 0xc) == ptrv(root(pt(self, 0x1c)))) {
        if (dw(self, 0x18))
            vcall<DWORD>(self, 50, dw(self, 0x1c));
        return 1;
    }
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x42a720)(self, a, msg);
}
// 0x472c90 (slot 49, start firing (part, a2)): unless it fires already, the part's frame is locked
// (its slot 85), the "weapon discharged" message (0x599300 with the root character and its slot 70)
// goes to the parts within 500 pixels (CLayerPart_FindInRect with flag 0x800000); a definition with a
// "+0x24" model spawns the muzzle flash entity there (World::SpawnExplosionEntity at the facing angle,
// taken out of the layer and made a child of the root); the data's start action runs (CAction::
// ExecuteFrom). eax: the action's, the muzzle flash entity when it fires already, else 0.
DWORD THISCALL Item_StartFiring(BYTE *self, BYTE *part, DWORD a2)
{
    if (dw(self, 0x18))
        return dw(self, 0x18);
    dw(self, 0x1c) = ptrv(part);
    if (!(zax::fn<DWORD(THISCALL *)(void *)>(0x48ec20)(part) & 0xff)) {
        const DWORD frame = vcall<DWORD>(part, 85);
        zax::fn<void(THISCALL *)(void *, float)>(0x48ff90)(part, fstp(fild((int)frame)));
    }
    BYTE *r = root(part);
    if (r && !is_kind_of(r, character_class()))
        r = nullptr;
    const DWORD v = vcall<DWORD>(r, 70);
    BYTE msg[0x14];
    zax::fn<void *(THISCALL *)(void *, void *, DWORD)>(0x599300)(msg, r, v);
    zax::fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4fb910)(r, msg, 0x43fa0000, 0xbf800000, 0x800000, 0, 0);
    if (dw(definition_of(self), 0x24)) {
        const DWORD angle = dw(r, 0xc4);
        BYTE *e = spawn_explosion(pt(r, 0x48), dw(definition_of(self), 0x24), 0, 0, a2, 1, angle, 1, nullptr);
        dw(self, 0x18) = ptrv(e);
        remove_part(pt(r, 0x48), e);
        add_child(r, pt(self, 0x18));
    }
    if (!def_start_action(definition_of2(self)))
        return 0;
    return action_execute_from(def_start_action(definition_of2(self)), pt(r, 0x48), r, 0, 0, self);
}
// 0x472dc0 (slot 50, stop firing (part)): the data's finish action runs for the root character, the
// muzzle flash entity dies, the frame of the part (or of the owner when both have the same root) is
// unlocked and the character no longer waits for a discharge (0x48ebc0 with 0). eax: that call's, 0
// when it was not firing.
DWORD THISCALL Item_StopFiring(BYTE *self, BYTE *part)
{
    if (!dw(self, 0x18))
        return 0;
    BYTE *frame_part = part;
    BYTE *r = root(part);
    if (dw(self, 0x1c) && r == root(pt(self, 0x1c)))
        frame_part = pt(self, 0x1c);
    if (r && !is_kind_of(r, character_class()))
        r = nullptr;
    if (def_finish_action(definition_of2(self)))
        action_execute_from(def_finish_action(definition_of2(self)), pt(r, 0x48), r, 0, 0, self);
    if (dw(self, 0x18)) {
        died(pt(self, 0x18));
        dw(self, 0x18) = 0;
    }
    if (zax::fn<DWORD(THISCALL *)(void *)>(0x48ec20)(frame_part) & 0xff)
        zax::fn<void(THISCALL *)(void *)>(0x48ffb0)(frame_part);
    const DWORD r_eax = zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x48ebc0)(r, 0);
    dw(self, 0x1c) = 0;
    return r_eax;
}

// ---- CFireContinuousFireWeaponBaseAI ----------------------------------------------------------------------------

// 0x472ed0, 0x473070 (slot 2)
CClassInfo *__cdecl FireBaseAI_ClassInfo()
{
    return class_getter(0x6bd6f8, 0x610044 /* "CFireContinuousFireWeaponBaseAI" */, 0.0f, layer_part_ai_class, 0x34, 0, 0x472f30, 0x472f80,
                        0x4730d0);
}
// 0x4747e0, 0x474a40 (slot 2); 0x4751a0, 0x475420 (slot 2)
CClassInfo *__cdecl FireElectricalAI_ClassInfo()
{
    return class_getter(0x6bd6fc, 0x610188 /* "CFireElectricalWeaponAI" */, 0.0f, FireBaseAI_ClassInfo, 0x40, 0, 0x474880, 0x4748f0, 0x474af0);
}
CClassInfo *__cdecl FireParticleAI_ClassInfo()
{
    return class_getter(0x6bd700, 0x61020c /* "CFireParticleWeaponAI" */, 0.0f, FireBaseAI_ClassInfo, 0x40, 0, 0x475240, 0x4752b0, 0x4754d0);
}
// 0x4730d0. eax: the last call's.
DWORD __cdecl FireBaseAI_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610160) /* "Weapon Definition Index" */, 0x18, 4, 0x18, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(FireBaseAI_ClassInfo(), f);
    f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x610148) /* "Debug/Impact Explosion" */, 0x1c, 4, 0x1c, class_at(fn_addr(0x487620)), 3, 0);
    add_field(FireBaseAI_ClassInfo(), f);
    f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x610138) /* "Debug/Gun Item" */, 0x10, 4, 0x10, item_class(), 3, 0);
    add_field(FireBaseAI_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x6100f0) /* "Damaged Message Name" */, 0x20, 4, 0x20, 1,
                  glob_addr(0x610108) /* "You Were Damaged By A Continuous Fire Weapon" */, 0);
    add_field(FireBaseAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6100d8) /* "Score Every" */, glob_addr(0x6100e4) /* "per Second" */, 0x24, 4, 0x24, 1,
                  0x3f800000 /* 1.0 */, 0, 0x3727c5ac /* 1e-05 */, 0x497423fe /* 999999.9 */, 0);
    add_field(FireBaseAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be358);
    zs_assign(f, 0x20, 0x60b000 /* "Seconds" */);
    set_help(f, 0x610090 /* "How often the Continuous fire weapon updates the Accuracy statistic." */);
    f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x610028) /* "Debug/Muzzel Flash Entity" */, 0x14, 4, 0x14, class_at(fn_addr(0x487620)), 3, 0);
    add_field(FireBaseAI_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x610064) /* "Debug/Updates Since I Got IMoved Message" */, 0x30, 4, 0x30, 3, 0, 0, 0, 0);
    return add_field(FireBaseAI_ClassInfo(), f);
}
// 0x474af0. eax: the last call's.
DWORD __cdecl FireElectricalAI_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    f = construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x6101f8) /* "Lightning To Use" */, 0x34, 4, 0x34, class_at(fn_addr(0x4fec90)), 1, class_at(fn_addr(0x4fec90)), 0, 0);
    add_field(FireElectricalAI_ClassInfo(), f);
    f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x6101e0) /* "Debug/Lightning Path" */, 0x38, 4, 0x38, class_at(fn_addr(0x4fec90)), 3, 0);
    add_field(FireElectricalAI_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x6101a0) /* "Clipped Message Name" */, 0x3c, 4, 0x3c, 1, glob_addr(0x6101b8) /* "You Clipped The Lightning Gun's Bolt" */, 0);
    return add_field(FireElectricalAI_ClassInfo(), f);
}
// 0x4754d0. eax: the last call's.
DWORD __cdecl FireParticleAI_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    f = construct_field(f, OBJECT_FIELD_CTOR, glob_addr(0x610270) /* "Particle System" */, 0x34, 4, 0x34, class_at(fn_addr(0x52ef30)), 1, class_at(fn_addr(0x52ef30)), 0, 0);
    add_field(FireParticleAI_ClassInfo(), f);
    f = heap_alloc(0x30);
    f = construct_field(f, OBJECT_REF_FIELD_CTOR, glob_addr(0x610248) /* "Debug/Particle System On Muzzel Flash" */, 0x38, 4, 0x38, class_at(fn_addr(0x52ef30)), 3, 0);
    add_field(FireParticleAI_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x6101a0) /* "Clipped Message Name" */, 0x3c, 4, 0x3c, 1, glob_addr(0x610224) /* "You Clipped The Particle Gun's Bolt" */, 0);
    return add_field(FireParticleAI_ClassInfo(), f);
}

// The looping sound stopped (+0x28, -1 when none): at the muzzle flash's root part, or at (0, 0) of the
// current layer without one; mode 2, the channel +0x2c
void stop_sound(BYTE *self)
{
    if (dw(self, 0x28) == 0xffffffffu)
        return;
    BYTE *r;
    if (dw(self, 0x14) && (r = root(pt(self, 0x14)))) {
        send_sound_at(current_layer(), dw(self, 0x28), r, 2, 0, fl<BYTE>(self, 0x2c));
    } else {
        float pos[2] = {0, 0};
        send_sound(current_layer(), dw(self, 0x28), pos, 2, 0, fl<BYTE>(self, 0x2c));
    }
    dw(self, 0x28) = 0xffffffffu;
    fl<BYTE>(self, 0x2c) = 0xff;
}
// 0x4734e0: the destructor level of the base AI: the sound stopped, the muzzle flash no longer
// listened to, the impact explosion dies; then the CLayerPartAI and CAIBase levels
void THISCALL FireBaseAI_Dtor(BYTE *self)
{
    dw(self, 0) = vt(VT_BASE_AI);
    stop_sound(self);
    if (dw(self, 0x14)) {
        remove_listener(pt(self, 0x14), self);
        dw(self, 0x14) = 0;
        dw(self, 0x10) = 0;
    }
    if (dw(self, 0x1c)) {
        died(pt(self, 0x1c));
        dw(self, 0x1c) = 0;
    }
    obj_free_fields(self);
    zs_dtor(self + 0x20);
    vtable_and_free(self, vt(VT_LAYER_PART_AI));
    ai_base_dtor(self);
}
void FireBaseAI_destroy(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x4734e0)(o); }
void FireElectricalAI_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_ELECTRICAL);
    if (dw(o, 0x38)) {
        died(pt(o, 0x38));
        dw(o, 0x38) = 0;
    }
    obj_free_fields(o);
    zs_dtor(o + 0x3c);
    FireBaseAI_destroy(o);
}
void FireParticleAI_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_PARTICLE);
    if (dw(o, 0x38)) {
        ai_remove_listener(pt(o, 0x38), o);
        fl<BYTE>(pt(o, 0x38), 8) = 1;
        dw(o, 0x38) = 0;
    }
    obj_free_fields(o);
    zs_dtor(o + 0x3c);
    FireBaseAI_destroy(o);
}
// 0x473000, 0x474980, 0x475340 (slot 1)
void *THISCALL FireBaseAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, FireBaseAI_destroy>(self, flags); }
void *THISCALL FireElectricalAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x40, FireElectricalAI_destroy>(self, flags); }
void *THISCALL FireParticleAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x40, FireParticleAI_destroy>(self, flags); }

// 0x4735c0 (slot 24, begin (part)): the part's CContinuousFireWeaponItem of "Weapon Definition Index"
// becomes "Gun Item" and its muzzle flash entity the part listened to (its looping sound started, mode
// 0, or kept going); without one the reporter gets "No muzzel flash was found... Continuous fire
// impact being set to ground height" (the static 0x6bd5d0). Unless there is an impact explosion
// already, one is spawned (the definition's 0x5b77a0) at the part's facing with the muzzle flash's slot
// 54 value, taken out of the layer, its flag 0x800000 cleared and made a child of the part.
void THISCALL FireBaseAI_Begin(BYTE *self, BYTE *part)
{
    if (fl<BYTE>(self, 8))
        return;
    dw(self, 0x30) = 0;
    BYTE *inv = inventory_of(part);
    BYTE *def = zax_def(dw(self, 0x18));
    const DWORD index = zax::fn<DWORD(THISCALL *)(void *, void *)>(0x425900)(inv, def);
    BYTE *item = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x424f60)(inv, index);
    if (item && !is_kind_of(item, item_class()))
        item = nullptr;
    DWORD flash_value = 0;
    if (item && dw(item, 0x18)) {
        flash_value = vcall<DWORD>(pt(item, 0x18), 54);
        BYTE *r = root(pt(item, 0x18));
        if (dw(self, 0x28) == 0xffffffffu) {
            BYTE *data = pt(definition_of(item), 0xcc);
            if (data && !is_kind_of(data, data_class()))
                data = nullptr;
            const DWORD sound = dw(data, 0x1c);
            if (sound) {
                dw(self, 0x28) = sound;
                fl<BYTE>(self, 0x2c) = (BYTE)send_sound_at(pt(r, 0x48), sound, r, 0, 1, 0);
            }
        } else {
            send_sound_at(pt(r, 0x48), dw(self, 0x28), r, 1, 1, fl<BYTE>(self, 0x2c));
        }
        dw(self, 0x10) = ptrv(item);
        BYTE *flash = pt(item, 0x18);
        dw(self, 0x14) = ptrv(flash);
        add_listener(flash, self);
    } else {
        report(glob_addr(0x6bd5d0));
    }
    BYTE *anim = part && is_kind_of(part, entity_animated_class()) ? part : nullptr;
    const DWORD angle = anim ? dw(anim, 0xc4) : 0;
    if (dw(self, 0x1c))
        return;
    BYTE *d = zax_def(dw(self, 0x18));
    BYTE *layer = pt(part, 0x48);
    BYTE *e = spawn_explosion(layer, def_impact(d), 0, 0, flash_value, 0, angle, 1, part);
    dw(self, 0x1c) = ptrv(e);
    if (!e)
        return;
    remove_part(pt(part, 0x48), e);
    vcall<void>(pt(self, 0x1c), 59, 0x800000);
    add_child(part, pt(self, 0x1c));
}

// 0x474510 (part, polygon): the beam polygon: the part's origin, then five points at "range" (the
// definition's +0x3c) in the part's facing (its +0xc4) turned by -2..2 quarters of the spread angle
// (the middle one straight), y scaled by 0.712766; closed
void THISCALL FireBaseAI_BeamPolygon(BYTE *self, BYTE *part, BYTE *poly)
{
    BYTE *def = zax_def(dw(self, 0x18));
    float range = fl<float>(def, 0x3c), side = 0;
    float o1, o2;
    if (part && is_kind_of(part, entity_animated_class())) {
        sin_cos(&o1, &o2, fstp(fchs(fld(fl<float>(part, 0xc4)))));
        const long double a = fld(o2) * fld(range);
        side = fstp(fchs(fld(o1)) * fld(range));
        range = fstp(a);
    }
    poly_clear(poly);
    poly_insert(poly, 0, 0, -1);
    for (int i = -2; i <= 2; i++) {
        if (i == 0) {
            poly_insert(poly, float_bits(range), float_bits(fstp(fld(side) * cst(glob_addr(C_YX)))), -1);
            continue;
        }
        BYTE *d = zax_def_inline(dw(self, 0x18));
        float c, s;
        sin_cos(&c, &s, fstp(fchs(def_spread(d) * fild(i) * cst(glob_addr(C_QUARTER)))));
        const float x = fstp(dot2(s, range, c, side));
        const float y = fstp(ndot2_scaled(c, range, s, side, *glob<const float *>(C_YX)));
        poly_insert(poly, float_bits(x), float_bits(y), -1);
    }
    poly_close(poly);
}

// 0x4737c0 (slot 25, update (part, seconds)): every frame the beam: after more than 6 updates without
// the "I Moved" message (counted while time passes) the AI is done and the gun item stops (slot 50 with
// the part as an animated entity or 0). Otherwise (a gun item with a muzzle flash; else the reporter's
// "... without a CContinuousFireWeaponItem" or "... that's not firing", static 0x6bd5c4/0x6bd5c8, and
// done) the looping sound is kept going; the beam polygon (0x474510) at the part's position plus the muzzle
// flash's is searched for parts (CLayer::FindRect, kind 3, flags 0x940000) that a projectile hits (their
// slot 65 with the function-local static projectile 0x6bd5d8), whose polygon touches the beam
// (Polygon_HitsCircle with its size), and that a movable part can see; the nearest (squared distance
// below the range squared) is the target. The impact explosion moves to the sweep's hit point on the
// target's polygon (or its centre), its render height the muzzle flash's. A destructable target (flag
// 0x100000, not the part itself) takes the definition's damage at the distance (its slot 23) times the
// seconds (shortened to what the energy source still holds) as "damage type" +0x38 (slot 99), gets the
// "Damaged Message Name" message (slot 63) and the impact explosion shows (slot 58 0x800000), and the
// quest's accuracy statistic (stat 0xe) counts a hit and a shot; without one the explosion hides (slot
// 59) and a shot counts. Then the energy source pays "+0x4c" times the seconds times -1 (times the world
// setting +0x18 for a player's part; slot 50, emptied through slot 49 with 0 when that fails; "No Valid
// Ammo Availible ..." (static 0x6bd5d4) without one) and slot 42 draws the beam (part, target, polygon,
// muzzle position, hit offset, render height, and again when there is a target).
void THISCALL FireBaseAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (fl<BYTE>(self, 8))
        return;
    const bool time_passes = !c03(fld(dt), cst(glob_addr(C_ZERO)));
    const DWORD updates = dw(self, 0x30);
    if (updates > 6) {
        if (time_passes) {
            fl<BYTE>(self, 8) = 1;
            if (!dw(self, 0x10))
                return;
            vcall<DWORD>(pt(self, 0x10), 50, part && is_kind_of(part, entity_animated_class()) ? part : nullptr);
            return;
        }
    } else if (time_passes) {
        dw(self, 0x30) = updates + 1;
    }
    float muzzle[2] = {0, 0};
    BYTE *inv = inventory_of(part);
    BYTE *item = pt(self, 0x10);
    if (!item || !dw(self, 0x14)) {
        fl<BYTE>(self, 8) = 1;
        report(item ? glob_addr(0x6bd5c8) : glob_addr(0x6bd5c4));
        return;
    }
    BYTE *r = root(pt(self, 0x14));
    if (dw(self, 0x28) == 0xffffffffu) {
        BYTE *data = pt(definition_of(item), 0xcc);
        if (data && !is_kind_of(data, data_class()))
            data = nullptr;
        const DWORD sound = dw(data, 0x1c);
        if (sound) {
            dw(self, 0x28) = sound;
            fl<BYTE>(self, 0x2c) = (BYTE)send_sound_at(pt(r, 0x48), sound, r, 0, 1, 0);
        }
    } else {
        send_sound_at(pt(r, 0x48), dw(self, 0x28), r, 1, 1, fl<BYTE>(self, 0x2c));
    }
    DWORD height = 0, height2 = 0;
    if (dw(self, 0x14)) {
        vcall<void>(pt(self, 0x14), 43, muzzle);
        height = vcall<DWORD>(pt(self, 0x14), 54);
    }
    float from[2] = {muzzle[0], muzzle[1]};
    float pos[2];
    vcall<void>(part, 43, pos);
    from[0] = fstp(fld(pos[0]) + fld(from[0]));
    from[1] = fstp(fld(from[1]) + fld(pos[1]));
    BYTE *movable = part && is_kind_of(part, entity_movable_class()) ? part : nullptr;
    BYTE beam[0x28];
    poly_ctor(beam);
    zax::fn<void(THISCALL *)(void *, void *, void *)>(0x474510)(self, part, beam);
    BYTE *found = nullptr, *candidates = nullptr;
    zax::fn<void *(THISCALL *)(void *)>(0x59a470)(&found);
    zax::fn<void *(THISCALL *)(void *)>(0x59a470)(&candidates);
    BYTE *target = nullptr;
    float best = fl<float>(zax_def(dw(self, 0x18)), 0x3c);
    best = fstp(fld(fl<float>(zax_def(dw(self, 0x18)), 0x3c)) * fld(best));
    BYTE rect[0x10];
    zax::fn<void *(THISCALL *)(void *, const float *, void *)>(0x546ad0)(beam, from, rect);
    zax::fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4ed5e0)(pt(part, 0x48), found, rect, 3, 0x940000, 0, 0);
    zax::fn<DWORD(THISCALL *)(void *)>(0x59a120)(candidates);
    zax::fn<DWORD(THISCALL *)(void *, void *)>(0x57aeb0)(pt(part, 0x48) + 0x50, candidates);
    const DWORD n = dw(found, 8);
    float centre[2];
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = pt(pt(found, 4), i * 4);
        BYTE &guard = *glob<BYTE *>(0x6bd5a9);
        if (!(guard & 1)) {
            guard |= 1;
            zax::fn<void *(THISCALL *)(void *)>(0x4920b0)(glob<void *>(0x6bd5d8));
            crt_atexit(code<void(__cdecl *)()>(0x474310));
        }
        if (e == part)
            continue;
        if (!(vcall<DWORD>(e, 65, glob<void *>(0x6bd5d8), 0) & 0xff))
            continue;
        float epos[2];
        vcall<void>(e, 43, epos);
        const float dx = fstp(fld(epos[0]) - fld(from[0]));
        const float dy = fstp(fld(epos[1]) - fld(from[1]));
        const long double size = zax::fn<long double(THISCALL *)(void *)>(0x546a90)(vcall<void *>(e, 48));
        if (!(zax::fn<DWORD(THISCALL *)(void *, float, float, float)>(0x5469c0)(beam, dx, dy, fstp(size)) & 0xff))
            continue;
        part_position(e, centre);
        if (target) {
            const long double a = fld(from[1]) - fld(centre[1]), b = fld(from[0]) - fld(centre[0]);
            if (!c0(b * b + a * a, fld(best)))
                continue;
        }
        if (movable && !(zax::fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x491380)(movable, e, 0, 0, 2, 0) & 0xff))
            continue;
        const long double a = fld(from[1]) - fld(centre[1]), b = fld(from[0]) - fld(centre[0]);
        target = e;
        best = fstp(b * b + a * a);
    }
    float hit_offset[2];
    if (target) {
        float tpos[2];
        vcall<void>(target, 43, tpos);
        part_position(target, centre);
        BYTE sweep[0x38], hit[0x24];
        zax::fn<void *(THISCALL *)(void *, const float *, const float *, DWORD, DWORD)>(0x545a20)(sweep, from, centre, 0, 1);
        zax::fn<void *(THISCALL *)(void *)>(0x545c90)(hit);
        float hy, hx_bits;
        if (zax::fn<DWORD(THISCALL *)(void *, const float *, const float *, void *, void *)>(0x547230)(vcall<void *>(target, 48), tpos, from, sweep,
                                                                                          hit) &
            0xff) {
            hy = fl<float>(hit, 0x18);
            hx_bits = fl<float>(hit, 0x14);
        } else {
            hy = centre[1];
            hx_bits = centre[0];
        }
        height2 = height;
        hit_offset[0] = fstp(fld(hx_bits) - fld(pos[0]));
        hit_offset[1] = fstp(fld(hy) - fld(pos[1]));
        vcall<void>(pt(self, 0x1c), 41, float_bits(hit_offset[0]), float_bits(hit_offset[1]));
        zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x48b6b0)(pt(self, 0x1c), height);
    }
    bool hit_done = false;
    if (target && (dw(target, 0x1c) & 0x100000) && part != target && is_kind_of(target, destructable_class())) {
        BYTE *victim = is_kind_of(target, destructable_class()) ? target : nullptr;
        float seconds = dt;
        float need = fl<float>(zax_def(dw(self, 0x18)), 0x4c);
        BYTE *d = zax_def(dw(self, 0x18));
        need = fstp(fld(need) * fld(dt) * cst(glob_addr(C_MINUS_ONE)));
        if (c0(fld(fl<float>(energy_source(d, inv), 0x18)), fld(need))) {
            const float charge = fl<float>(energy_source(zax_def(dw(self, 0x18)), inv), 0x18);
            need = charge;
            BYTE *d2 = zax_def(dw(self, 0x18));
            seconds = fstp(fld(need) / (fld(fl<float>(d2, 0x4c)) * cst(glob_addr(C_MINUS_ONE))));
        }
        const DWORD damage_type = dw(zax_def(dw(self, 0x18)), 0x38);
        BYTE *d3 = zax_def(dw(self, 0x18));
        const long double dist = table_sqrt(best);
        const long double amount = vcall<long double>(d3, 23, fstp(dist)) * fld(seconds);
        vcall<void>(victim, 99, fstp(amount), damage_type, part);
        StackMessage msg{vt(0x5eeb64), message_id(zs_c_str(self + 0x20)), 0};
        vcall<DWORD>(victim, 63, &msg);
        vcall<void>(pt(self, 0x1c), 58, 0x800000);
        if (BYTE *stat = quest_accuracy_stat()) {
            const DWORD key = quest_key();
            const DWORD v = float_bits(fstp(fld(dt) * fld(fl<float>(self, 0x24))));
            stat_add_denominator(stat, key, v);
            stat_add_numerator(stat, key, v);
        }
        hit_done = true;
    }
    if (!hit_done) {
        vcall<void>(pt(self, 0x1c), 59, 0x800000);
        if (BYTE *stat = quest_accuracy_stat()) {
            const DWORD key = quest_key();
            stat_add_denominator(stat, key, float_bits(fstp(fld(dt) * fld(fl<float>(self, 0x24)))));
        }
    }
    BYTE *d = zax_def(dw(self, 0x18));
    float cost = fstp(fld(fl<float>(d, 0x4c)) * fld(dt) * cst(glob_addr(C_MINUS_ONE)));
    if (*glob<DWORD *>(G_CATEGORY_PLAYER) & dw(part, 0x1c))
        cost = fstp(fld(cost) * fld(fl<float>(vcall<BYTE *>(world(), 58), 0x18)));
    if (zax_def_inline(dw(self, 0x18)) && energy_source(zax_def_inline(dw(self, 0x18)), inv)) {
        if (!(vcall<DWORD>(energy_source(zax_def_inline(dw(self, 0x18)), inv), 50, cost) & 0xff))
            vcall<void>(energy_source(zax_def_inline_load(dw(self, 0x18)), inv), 49, 0);
    } else {
        report(glob_addr(0x6bd5d4));
    }
    vcall<void>(self, 42, part, target, beam, muzzle, hit_offset, height, height2);
    zax::fn<void(THISCALL *)(void *)>(0x59a510)(&candidates);
    zax::fn<void(THISCALL *)(void *)>(0x59a510)(&found);
    poly_dtor(beam);
}

// 0x474460 (slot 0, a message (other, name)): "I Moved" from the muzzle flash restarts the count and
// updates at once (slot 25 with its root part and 0 seconds); "Died" or "Delete" from it ends the
// listening and the AI is done; then CObject's slot 0. eax: its.
DWORD THISCALL FireBaseAI_OnMessage(BYTE *self, BYTE *other, const char *name)
{
    if (dw(self, 0x14) && pt(self, 0x14) == other && !crt_stricmp(name, glob<const char *>(0x610180))) {
        dw(self, 0x30) = 0;
        vcall<void>(self, 25, root(pt(self, 0x14)), 0);
        return obj_message(self, other, name);
    }
    if (dw(self, 0x14) && pt(self, 0x14) == other &&
        (!crt_stricmp(name, glob<const char *>(0x610178)) || !crt_stricmp(name, glob<const char *>(0x60aef8)))) {
        remove_listener(pt(self, 0x14), self);
        dw(self, 0x14) = 0;
        fl<BYTE>(self, 8) = 1;
    }
    return obj_message(self, other, name);
}
// 0x4746c0 (slot 29, a message to the AI (part, message)): "deactivate AI" (0x6c29e8) or 0x6c29d4 with
// a gun item stops the sound, the gun item (slot 50 with the part as a character or 0), the listening
// and the impact explosion; then CLayerPartAI's slot 29. eax: its.
DWORD THISCALL FireBaseAI_AIMessage(BYTE *self, BYTE *part, BYTE *msg)
{
    if (dw(self, 0x10) && (dw(msg, 4) == *glob<DWORD *>(MSG_DEACTIVATE_AI) || dw(msg, 4) == *glob<DWORD *>(MSG_6C29D4))) {
        BYTE *ch = part && is_kind_of(part, character_class()) ? part : nullptr;
        stop_sound(self);
        vcall<DWORD>(pt(self, 0x10), 50, ch);
        if (dw(self, 0x14)) {
            remove_listener(pt(self, 0x14), self);
            dw(self, 0x14) = 0;
            dw(self, 0x10) = 0;
        }
        if (dw(self, 0x1c)) {
            died(pt(self, 0x1c));
            dw(self, 0x1c) = 0;
        }
    }
    return part_ai_message(self, part, msg);
}

// ---- CFireElectricalWeaponAI ---------------------------------------------------------------------------------------

bool deactivating(BYTE *msg) { return dw(msg, 4) == *glob<DWORD *>(MSG_DEACTIVATE_AI) || dw(msg, 4) == *glob<DWORD *>(MSG_6C29D4); }
void base_begin(BYTE *self, BYTE *part) { zax::fn<void(THISCALL *)(void *, void *)>(0x4735c0)(self, part); }
DWORD base_ai_message(BYTE *self, BYTE *part, BYTE *msg) { return zax::fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4746c0)(self, part, msg); }

// 0x474d70 (slot 24): a clone of "Lightning To Use" becomes the lightning path, a child of the part with
// its flag 0x800000 cleared; then the base AI's begin
void THISCALL FireElectricalAI_Begin(BYTE *self, BYTE *part)
{
    if (fl<BYTE>(self, 8))
        return;
    if (!dw(self, 0x38)) {
        void *c = object_clone(pt(self, 0x34), 0, 0xffffffffu);
        dw(self, 0x38) = ptrv(c);
        add_child(part, c);
        vcall<void>(pt(self, 0x38), 59, 0x800000);
    }
    base_begin(self, part);
}
// 0x475150 (slot 29): "deactivate AI" makes the lightning path die; then the base AI's slot 29
DWORD THISCALL FireElectricalAI_AIMessage(BYTE *self, BYTE *part, BYTE *msg)
{
    if (dw(self, 0x38) && deactivating(msg)) {
        died(pt(self, 0x38));
        dw(self, 0x38) = 0;
    }
    return base_ai_message(self, part, msg);
}
// The lightning's end point (its slots 84, 85: x, y) and its "height" span (slot 86); each returns a
// float in st(0), dropped
void lightning_set(BYTE *l, int slot, float v) { vcall<long double>(l, slot, v); }
// 0x474dc0 (slot 42, draw the beam (part, target, polygon, muzzle position, hit offset, height, height
// 2)): the lightning path shows (slot 58 0x800000) at the muzzle (slot 41) at the height (slot 83).
// With a target it reaches the hit offset (slots 84, 85; 86 the height difference). Without one it
// reaches a random length (0.5 to 1 of the range) in a random direction within the spread around the
// part's facing and a random height span (-0.25 to 0.25 times the height); a lightning that a sweep from
// the part's centre to its start (CLayer_SweepPoint) already finds blocked has no length (0, 0, 0);
// one whose sweep along it hits a part ends there, and that part gets "Clipped Message Name" (slot 63).
void THISCALL FireElectricalAI_DrawBeam(BYTE *self, BYTE *part, BYTE *target, void *, float *muzzle, float *hit_offset, int height, int height2)
{
    BYTE *l = pt(self, 0x38);
    vcall<void>(l, 58, 0x800000);
    vcall<void>(pt(self, 0x38), 41, float_bits(muzzle[0]), float_bits(muzzle[1]));
    float h = fstp(fild(height));
    vcall<long double>(pt(self, 0x38), 83, h);
    if (target) {
        lightning_set(pt(self, 0x38), 84, fstp(fld(hit_offset[0]) - fld(muzzle[0])));
        lightning_set(pt(self, 0x38), 85, fstp(fld(hit_offset[1]) - fld(muzzle[1])));
        vcall<long double>(pt(self, 0x38), 86, fstp(fild(height2 - height)));
        return;
    }
    float c = 0;
    if (part && is_kind_of(part, entity_animated_class()))
        c = fl<float>(part, 0xc4);
    BYTE *d = zax_def(dw(self, 0x18));
    float range = fl<float>(d, 0x3c);
    const float length = fstp(random_between(0x55c650, 0x3f000000, 0x3f800000) * fld(range));
    BYTE *d2 = zax_def(dw(self, 0x18));
    float s = fstp(random_between(0x55c650, 0xbf000000, 0x3f000000));
    sin_cos(&c, &s, fstp(fchs(def_spread(d2) * fld(s) + fld(c))));
    const float x = fstp(fld(s) * fld(length));
    const float y = fstp(fchs(fld(c)) * fld(length) * cst(glob_addr(C_YX)));
    lightning_set(pt(self, 0x38), 84, x);
    lightning_set(pt(self, 0x38), 85, y);
    BYTE *l2 = pt(self, 0x38);
    lightning_set(l2, 86, fstp(random_between(0x55c5b0, 0xbe800000, 0x3e800000) * fld(h)));
    float centre[2], start[2];
    part_position(part, centre);
    vcall<void>(part, 43, start);
    start[0] = fstp(fld(start[0]) + fld(muzzle[0]));
    start[1] = fstp(fld(start[1]) + fld(muzzle[1]));
    BYTE sweep[0x38], hit[0x24];
    zax::fn<void *(THISCALL *)(void *, const float *, const float *, DWORD, DWORD)>(0x545a20)(sweep, centre, start, 0, 1);
    using SweepPoint = DWORD(THISCALL *)(void *, float *, void *, void *, void *, DWORD, DWORD, DWORD, DWORD);
    if (zax::fn<SweepPoint>(0x4e8d40)(pt(part, 0x48) + 0x3c, centre, sweep, nullptr, nullptr, 0, 0, 0, 0) & 0xff) {
        lightning_set(pt(self, 0x38), 84, 0);
        lightning_set(pt(self, 0x38), 85, 0);
        vcall<long double>(pt(self, 0x38), 86, 0.0f);
        return;
    }
    float end[2];
    end[0] = fstp(fld(x) + fld(start[0]));
    end[1] = fstp(fld(start[1]) + fld(y));
    zax::fn<DWORD(THISCALL *)(void *, const float *, const float *)>(0x545ab0)(sweep, start, end);
    zax::fn<void *(THISCALL *)(void *)>(0x545c90)(hit);
    BYTE *clipper = nullptr;
    const DWORD r = zax::fn<SweepPoint>(0x4e8d40)(pt(part, 0x48) + 0x3c, centre, sweep, hit, &clipper, 0, 0, 0, 0);
    if (!(r & 0xff))
        return;
    lightning_set(pt(self, 0x38), 84, fstp(fld(fl<float>(hit, 0x14)) - fld(start[0])));
    lightning_set(pt(self, 0x38), 85, fstp(fld(fl<float>(hit, 0x18)) - fld(start[1])));
    StackMessage msg{vt(0x5eeb64), message_id(zs_c_str(self + 0x3c)), 0};
    vcall<DWORD>(clipper, 63, &msg);
}

// ---- CFireParticleWeaponAI -----------------------------------------------------------------------------------------

// 0x475750 (slot 24): the base AI's begin; then a clone of "Particle System" becomes an AI of the muzzle
// flash (CLayerPart_AddAI) this AI listens to
void THISCALL FireParticleAI_Begin(BYTE *self, BYTE *part)
{
    if (fl<BYTE>(self, 8))
        return;
    base_begin(self, part);
    if (dw(self, 0x38))
        return;
    void *c = object_clone(pt(self, 0x34), 0, 0xffffffffu);
    dw(self, 0x38) = ptrv(c);
    add_ai(pt(self, 0x14), c);
    ai_add_listener(pt(self, 0x38), self);
}
// 0x475a50 (slot 0): "Delete" from the particle system ends the listening and the AI is done; then the
// base AI's slot 0
DWORD THISCALL FireParticleAI_OnMessage(BYTE *self, BYTE *other, const char *name)
{
    if (other == pt(self, 0x38) && !crt_stricmp(name, glob<const char *>(0x60aef8))) {
        ai_remove_listener(pt(self, 0x38), self);
        dw(self, 0x38) = 0;
        fl<BYTE>(self, 8) = 1;
    }
    return zax::fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x474460)(self, other, name);
}
// 0x475aa0 (slot 29): "deactivate AI" ends the listening to the particle system and marks it done; then
// the base AI's slot 29
DWORD THISCALL FireParticleAI_AIMessage(BYTE *self, BYTE *part, BYTE *msg)
{
    if (dw(self, 0x38) && deactivating(msg)) {
        ai_remove_listener(pt(self, 0x38), self);
        fl<BYTE>(pt(self, 0x38), 8) = 1;
        dw(self, 0x38) = 0;
    }
    return base_ai_message(self, part, msg);
}
// 0x4757a0 (slot 42, draw the beam): the particle system's reach: with a target the distance to the hit
// offset (table square root of the squared offset); without one the range, cut where a sweep along the
// facing finds a part (that part gets "Clipped Message Name") or 0 when the sweep from the part's centre
// to the start is blocked. The particle system's speed (its +0xb4, or +0xb0 when +0xbc is 0) divides it
// into its life +0xd8 (then its emitter's slot 24); +0xf8 the reach.
void THISCALL FireParticleAI_DrawBeam(BYTE *self, BYTE *part, BYTE *target, void *, float *muzzle, float *hit_offset, int, int)
{
    BYTE *d = zax_def(dw(self, 0x18));
    float reach = fl<float>(d, 0x3c);
    if (target) {
        const long double dy = fld(hit_offset[1]) - fld(muzzle[1]), dx = fld(hit_offset[0]) - fld(muzzle[0]);
        reach = fstp(table_sqrt(fstp(dx * dx + dy * dy)));
    } else {
        float angle = 0;
        if (part && is_kind_of(part, entity_animated_class()))
            angle = fl<float>(part, 0xc4);
        BYTE *d2 = zax_def(dw(self, 0x18));
        const float range = fl<float>(d2, 0x3c);
        float c, s;
        sin_cos(&c, &angle, fstp(fchs(fld(angle))));
        s = angle;
        const float x = fstp(fld(range) * fld(s));
        const float y = fstp(fchs(fld(c)) * fld(range) * cst(glob_addr(C_YX)));
        float centre[2], start[2];
        part_position(part, centre);
        vcall<void>(part, 43, start);
        start[0] = fstp(fld(start[0]) + fld(muzzle[0]));
        start[1] = fstp(fld(start[1]) + fld(muzzle[1]));
        BYTE sweep[0x38], hit[0x24];
        zax::fn<void *(THISCALL *)(void *, const float *, const float *, DWORD, DWORD)>(0x545a20)(sweep, centre, start, 0, 1);
        using SweepPoint = DWORD(THISCALL *)(void *, float *, void *, void *, void *, DWORD, DWORD, DWORD, DWORD);
        if (zax::fn<SweepPoint>(0x4e8d40)(pt(part, 0x48) + 0x3c, centre, sweep, nullptr, nullptr, 0, 0, 0, 0) & 0xff) {
            reach = 0;
        } else {
            float end[2];
            end[0] = fstp(fld(start[0]) + fld(x));
            end[1] = fstp(fld(start[1]) + fld(y));
            zax::fn<DWORD(THISCALL *)(void *, const float *, const float *)>(0x545ab0)(sweep, start, end);
            zax::fn<void *(THISCALL *)(void *)>(0x545c90)(hit);
            BYTE *clipper = nullptr;
            if (zax::fn<SweepPoint>(0x4e8d40)(pt(part, 0x48) + 0x3c, centre, sweep, hit, &clipper, 0, 0, 0, 0) & 0xff) {
                const long double dy = fld(fl<float>(hit, 0x18)) - fld(start[1]), dx = fld(fl<float>(hit, 0x14)) - fld(start[0]);
                reach = fstp(table_sqrt(fstp(dx * dx + dy * dy)));
                StackMessage msg{vt(0x5eeb64), message_id(zs_c_str(self + 0x3c)), 0};
                vcall<DWORD>(clipper, 63, &msg);
            }
        }
    }
    BYTE *ps = pt(self, 0x38);
    const long double speed = c3(fld(fl<float>(ps, 0xbc)), cst(glob_addr(C_ZERO))) ? fld(fl<float>(ps, 0xb0)) : fld(fl<float>(ps, 0xb4));
    fl<float>(ps, 0xd8) = fstp(fld(reach) / speed);
    vcall<void>(ps + 0xd0, 24);
    dw(pt(self, 0x38), 0xf8) = float_bits(reach);
}

// ---- CEnergyAmmoItem ---------------------------------------------------------------------------------------------

// 0x475b00 (also inlined into slot 33), 0x475c90 (slot 2)
CClassInfo *__cdecl EnergyAmmoItem_ClassInfo()
{
    return class_getter(0x6bd704, 0x610280 /* "CEnergyAmmoItem" */, 0.0f, energy_item_class, 0x1c, 0, 0x475b50, 0x475b90, 0x58ea40);
}
void EnergyAmmo_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_ENERGY_ITEM));
    vtable_and_defaults(o, vt(VT_ENERGY_AMMO));
}
void EnergyAmmo_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_ENERGY_AMMO));
    energy_item_dtor(o);
}
// 0x475b50, 0x475b90, 0x475c00 (slot 1)
void *__cdecl EnergyAmmo_Create() { return create_one<0x1c, EnergyAmmo_construct>(); }
void *__fastcall EnergyAmmo_CreateArray(int count, int, int) { return create_array<0x1c, EnergyAmmo_construct>(count); }
void *THISCALL EnergyAmmo_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, EnergyAmmo_destroy>(self, flags); }
long double max_charge(BYTE *self) { return zax::fn<long double(THISCALL *)(void *)>(0x5afe50)(self); }
// 0x475cf0 (slot 49, set the amount): the charge, at most CEnergyItem's maximum; nothing left (not above
// 0) marks the item for deletion with charge 0
void THISCALL EnergyAmmo_SetAmount(BYTE *self, float amount)
{
    if (c03(fld(amount), cst(glob_addr(C_ZERO)))) {
        dw(self, 0x18) = 0;
        fl<BYTE>(self, 0x11) = 1;
        return;
    }
    if (c0(max_charge(self), fld(amount)))
        fl<float>(self, 0x18) = fstp(max_charge(self));
    else
        fl<float>(self, 0x18) = fstp(fld(amount));
}
// 0x475d40 (slot 33, taken by a part (a, part)): without an energy ammunition of the same definition in
// the part's inventory as CInventoryItem's slot 33; otherwise that item gets the charge (CEnergyItem::Add,
// times the world setting +0x1c for a player's part) when it is one. eax: the clone, the item, 0 without
// an inventory.
DWORD THISCALL EnergyAmmo_Take(BYTE *self, DWORD a, BYTE *part)
{
    BYTE *inv = inventory_of(part);
    if (!inv)
        return 0;
    const int i = zax::fn<int(THISCALL *)(void *, void *)>(0x425740)(inv, self);
    if (i == -1)
        return zax::fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x42a550)(self, a, part);
    BYTE *item = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x424f60)(inv, (DWORD)i);
    if (is_kind_of(item, zax::fn<Getter>(0x475b00)())) {
        float charge = fl<float>(self, 0x18);
        if (a && (*glob<DWORD *>(G_CATEGORY_PLAYER) & dw(bp(a), 0x1c)))
            charge = fstp(fld(fl<float>(vcall<BYTE *>(world(), 58), 0x1c)) * fld(charge));
        zax::fn<void(THISCALL *)(void *, float)>(0x5afe90)(item, charge);
    }
    return ptrv(item);
}
// 0x475e40 (slot 32, may the part take it): without an inventory al 0; without such an item
// CInventoryItem's slot 32; otherwise eax 1 while the item there (the inventory's slot 26) is not full
DWORD THISCALL EnergyAmmo_MayTake(BYTE *self, BYTE *part)
{
    BYTE *inv = inventory_of(part);
    if (!inv)
        return 0;
    const int i = zax::fn<int(THISCALL *)(void *, void *)>(0x425740)(inv, self);
    if (i == -1)
        return zax::fn<DWORD(THISCALL *)(void *, void *)>(0x42a500)(self, part);
    BYTE *item = vcall<BYTE *>(inv, 26, i);
    const float charge = fl<float>(item, 0x18);
    return c03(max_charge(item), fld(charge)) ? 0 : 1;
}

// ---- The damage message and CDamageExpandingRadiusAI -------------------------------------------------------------

// 0x476450 (a1, a2, instigator): the damage message (vtable 0x5f08c4, id 0x6bd708) with the
// instigator also as a character (+0x18) when it is one. eax: this.
BYTE *THISCALL DamageMessage_Ctor(BYTE *self, DWORD a1, DWORD a2, BYTE *instigator)
{
    dw(self, 4) = *glob<DWORD *>(0x6bd708);
    dw(self, 0xc) = a1;
    fl<BYTE>(self, 8) = 0;
    dw(self, 0x10) = a2;
    dw(self, 0x14) = ptrv(instigator);
    dw(self, 0x18) = 0;
    dw(self, 0) = vt(0x5f08c4);
    if (instigator)
        dw(self, 0x18) = is_kind_of(instigator, character_class()) ? ptrv(instigator) : 0;
    return self;
}

// 0x4764a0, 0x476650 (slot 2)
CClassInfo *__cdecl DamageRadiusAI_ClassInfo()
{
    return class_getter(0x6bd74c, 0x6102a0 /* "CDamageExpandingRadiusAI" */, 5.0f, layer_part_ai_class, 0x34, 0, 0x4764f0, 0x476540, 0x4766b0);
}
// 0x4766b0. eax: the last call's.
DWORD __cdecl DamageRadiusAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x610330) /* "Damage" */, glob_addr(0x610338) /* "Units/Sec" */, 0x14, 4, 0x14, 1, 0x42c80000 /* 100.0 */, 0, 0, 0, 0);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610318) /* "Damage Type" */, 0x30, 4, 0x30, glob_addr(0x6bd760), 1, glob_addr(0x610324) /* "Unavoidable" */, 0, 0);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x610308) /* "Damage Creator" */, 0x10, 1, 0x10, 1, 0, 0);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6102ec) /* "Use Animation For Radius" */, 0x11, 1, 0x11, 1, 1, 0);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6102dc) /* "Start Radius" */, glob_addr(0x60b360) /* "Pixels" */, 0x18, 4, 0x18, 1, 0x41200000 /* 10.0 */, 0, 0,
                  0x447a0000 /* 1000.0 */, 0x41200000 /* 10.0 */);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6102d0) /* "End Radius" */, glob_addr(0x60b360) /* "Pixels" */, 0x1c, 4, 0x1c, 1, 0x42c80000 /* 100.0 */, 0, 0,
                  0x447a0000 /* 1000.0 */, 0x41200000 /* 10.0 */);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60b340) /* "YX Ratio" */, 0x20, 4, 0x20, 1, 0x3f3677d4 /* 0.71276593 */, 0, 0x3a83126f /* 0.001 */,
                  0x41200000 /* 10.0 */, 0x3dcccccd /* 0.1 */);
    add_field(DamageRadiusAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6102bc) /* "Start To End Time" */, glob_addr(0x60b000) /* "Seconds" */, 0x24, 4, 0x24, 1, 0x40000000 /* 2.0 */, 0,
                  0x3c23d70a /* 0.01 */, 0x42c80000 /* 100.0 */, 0x3dcccccd /* 0.1 */);
    return add_field(DamageRadiusAI_ClassInfo(), f);
}
void DamageRadiusAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_RADIUS_AI));
    vtable_and_free(o, vt(VT_LAYER_PART_AI));
    ai_base_dtor(o);
}
// 0x4765b0 (slot 1)
void *THISCALL DamageRadiusAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, DamageRadiusAI_destroy>(self, flags); }
// 0x476b20 (part; edx: the context {vtable 0x5f0970, the part's layer +4}): a destructable part takes the
// damage of this frame (slot 99 with the globals 0x6bd720 (the type) and 0x6bd750 (the creator))
void __fastcall DamageRadius_Callback(BYTE *part, BYTE *context, DWORD)
{
    if (is_kind_of(part, destructable_class()))
        vcall<void>(part, 99, dw(context, 4), *glob<DWORD *>(0x6bd720), *glob<DWORD *>(0x6bd750));
}
// 0x476b60 (slot 25, update (part, seconds)): an animated entity's radius grows with the time (+0x2c): with
// "Use Animation For Radius" half the width of the current frame (the rotation sequence's frame of slot
// 85, its slots 45 and 39: width and height; "YX Ratio" the height over the width), else from "Start
// Radius" to "End Radius" over "Start To End Time" (held at the end radius outside the two); every
// destructable part within it (CSortList2D_QueryRadius, kind 3, flags 0x840000) takes "Damage" times the
// seconds of the damage type, from the part's creator (slot 68) or the part itself with "Damage Creator"
// unset (both read slot 68).
void THISCALL DamageRadiusAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (!is_kind_of(part, entity_animated_class()))
        return;
    fl<float>(self, 0x2c) = fstp(fld(dt) + fld(fl<float>(self, 0x2c)));
    BYTE *seq = zax::fn<BYTE *(THISCALL *)(void *)>(0x48b590)(part);
    BYTE *frame = vcall<BYTE *>(seq, 23, vcall<DWORD>(part, 85));
    if (!frame)
        return;
    *glob<DWORD *>(0x6bd750) = fl<BYTE>(self, 0x10) ? vcall<DWORD>(part, 68) : vcall<DWORD>(part, 68);
    *glob<DWORD *>(0x6bd720) = dw(self, 0x30);
    // the context {vtable 0x5f0970, the damage} and the position lie next to each other, as in the original
    DWORD context[4] = {vt(0x5f0970), float_bits(fstp(fld(dt) * fld(fl<float>(self, 0x14)))), 0, 0};
    float *pos = reinterpret_cast<float *>(context + 2);
    part_position(part, pos);
    if (fl<BYTE>(self, 0x11)) {
        fl<float>(self, 0x28) = fstp(fild64((long long)vcall<DWORD>(frame, 45)) * cst(glob_addr(C_HALF)));
        const float height = fstp(fild64((long long)vcall<DWORD>(frame, 39)));
        fl<float>(self, 0x20) = fstp(fld(height) / fild64((long long)vcall<DWORD>(frame, 45)));
    } else {
        const long double r = fld(fl<float>(self, 0x2c)) / fld(fl<float>(self, 0x24)) * (fld(fl<float>(self, 0x1c)) - fld(fl<float>(self, 0x18))) +
                              fld(fl<float>(self, 0x18));
        const long double lo = fld(fl<float>(self, 0x18)), hi = fld(fl<float>(self, 0x1c));
        if ((c0(r, lo) && !c03(r, hi)) || (!c03(r, lo) && c0(r, hi)))
            fl<float>(self, 0x28) = fstp(r);
        else
            dw(self, 0x28) = dw(self, 0x1c);
    }
    using Query = DWORD(THISCALL *)(void *, void *, void *, float *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    zax::fn<Query>(0x57ac50)(pt(pt(part, 0x48), 0x78), code<void *>(0x476b20), context, pos, *glob<DWORD *>(0x6bd750), dw(self, 0x28),
                        dw(self, 0x20), 3, 0x840000, 0, 0, 0, 0);
}

// 0x477510 (part; edx: the context {vtable 0x5f0ba0, damage +4, falloff +8, damage type +0xc,
// instigator +0x10}; the squared distance): a destructable part gets a CPainAction of the damage less
// the distance (table square root) times the falloff, queued in its layer's action queue (+0xa0) with a
// delay of the distance at 344 m/s (1 / (344 * 3.2808399 * 10), made once into 0x6bd75c, guard
// 0x6bd8a8 bit 0) times 5, the part as trigger and the instigator; the target "$trigger" (the static
// ZString 0x6bd8a4, bit 1)
void __fastcall DamageRadius_PainCallback(BYTE *part, BYTE *context, float d2)
{
    BYTE &guard = *glob<BYTE *>(0x6bd8a8);
    if (!(guard & 1)) {
        const long double k = fld(*glob<float *>(0x5f0978)) * cst(glob_addr(0x5f097c)) * fld64(glob<void *>(0x5f0b98));
        guard |= 1;
        *glob<float *>(0x6bd75c) = fstp(fld64(glob<void *>(0x5ee678)) / k);
    }
    if (!is_kind_of(part, destructable_class()))
        return;
    const float dist = fstp(table_sqrt(d2));
    const float delay = fstp(fld(*glob<float *>(0x6bd75c)) * fld(dist) * cst(glob_addr(C_FIVE)));
    if (!(guard & 2)) {
        guard |= 2;
        zax::fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(glob<void *>(0x6bd8a4), glob<const char *>(0x60ae18));
        crt_atexit(code<void(__cdecl *)()>(0x477630));
    }
    BYTE pain[0x18];
    zax::fn<void *(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4c3c20)(pain, float_bits(fstp(fld(fl<float>(context, 4)) - fld(dist) * fld(fl<float>(context, 8)))),
                                                                   glob<void *>(0x6bd8a4), dw(context, 0xc));
    BYTE *layer = pt(part, 0x48);
    void *clone = object_clone(pain, 0, 0xffffffffu);
    zax::fn<void(THISCALL *)(void *, DWORD, DWORD, void *, DWORD, DWORD, void *)>(0x41c7a0)(layer + 0xa0, float_bits(delay), dw(context, 0x10), part, 0, 0,
                                                                                      clone);
    dw(pain, 0) = vt(0x5f0b24);
    obj_free_fields(pain);
    zs_dtor(pain + 0x10);
    zax::fn<void(THISCALL *)(void *)>(0x41a700)(pain);
}
// 0x477640 (ecx: a layer, edx: the centre; the part left out, radius, damage, damage type, instigator,
// a6, a7): every
// destructable part within the radius (CSortList2D_QueryRadius of the layer's grid +0x50, y scale
// 1.4029851, kind 4, flags 0x800000) gets pain falling off from the damage at the centre to 0 at the
// radius (0x477510); a6/a7 passed on (line of sight, class). eax: the query's.
DWORD __fastcall DamageRadius_Query(BYTE *layer, float *centre, DWORD except, float radius, float damage, DWORD damage_type, DWORD instigator,
                                    DWORD a6, DWORD a7)
{
    DWORD context[5] = {vt(0x5f0ba0), float_bits(damage), float_bits(fstp(fld(damage) / fld(radius))), damage_type, instigator};
    using Query = DWORD(THISCALL *)(void *, void *, void *, float *, DWORD, float, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    return zax::fn<Query>(0x57ac50)(layer + 0x50, code<void *>(0x477510), context, centre, except, radius, 0x3fb39504, 4, 0x800000, 0, 0, a6, a7);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(weapons)
{
    const Hook list[] = {
#include "weapons_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
