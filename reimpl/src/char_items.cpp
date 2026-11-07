// The characters' inventory items of Zax.exe (phase 3, inventory and weapons; 0x57fd00-0x581370 and
// 0x584510-0x584e45): the special ability item CCharacterInventoryItem with its definition, its level
// info and its type definition, and the Sphere of Zin's energy item: class getters (both copies), Create,
// CreateArray, deleting destructors (slot 1), RegisterFields and the item slots.
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CCharacterInventoryItem (0x24, class 0x7137b0, vtable 0x600dfc, under CEnergyItem): +0x1c "Special
//    Ability Level", +0x20 "Is Discharging". Its RegisterFields hides CEnergyItem's "Current Charge"
//    (flags +0x18 = 0, +0x34 = 1.0).
//  - CSpecialAbilitylevelInfo (0x20, class 0x7137b4, version 2, vtable 0x600ecc, no parent class): +8
//    "Description", +0xc "Charge Time", +0x10 "Duration" (seconds), +0x14 "Activate Sound", +0x18
//    "Activate Action", +0x1c "Deactivate Action".
//  - CCharacterInventoryItemDefinition (0x180, class 0x7137b8, version 2, vtable 0x600f28, under the class
//    of 0x486220; CZaxInventoryItemDefinition with a CEnergyMeter at +0x100): +0x174 "Special/Special
//    Ability" (enum list 0x713790), +0x178/+0x17c "Special/Level" (count, new[] array of
//    CSpecialAbilitylevelInfo).
//  - CCharacterInventoryTypeDefinition (0x34, class 0x7137c0, vtable 0x601024, under
//    CZaxInventoryTypeDefinition): +0x28 "Advance/Item Needed To Advance" (item catalogue 0x6c0c08),
//    +0x2c/+0x30 "Advance/Level" (data, count: the number of items needed for each level).
//  - CSphereOfZinEnergyInventoryItem (0x34, class 0x71381c, version 2, vtable 0x601380, under CEnergyItem):
//    +0x18 the charge, +0x1c "Debug/Charge Next Update", +0x20 "Debug/Charge Last Update", +0x24 the
//    "Debug/List of ReTriggerable Thresholds" (byte array vtable 0x60144c: data +0x28, count +0x2c,
//    capacity +0x30, heap), one byte per threshold of the Sphere of Zin data ("Retrigerable Now").
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_CHAR_ITEM = 0x600dfc, VT_ENERGY_ITEM = 0x5f07e0, VT_LEVEL_INFO = 0x600ecc, VT_CHAR_DEF = 0x600f28,
                VT_ZAX_ITEM_DEF_LEVEL = 0x5f1b4c, VT_CHAR_TYPE = 0x601024, VT_SPHERE_ITEM = 0x601380, VT_BYTE_ARRAY = 0x60144c;
constexpr DWORD BYTE_FIELD = 0x54ddf0, LIST_FIELD_CTOR = 0x417930, OBJECT_ARRAY_FIELD_CTOR = 0x419110;
constexpr DWORD G_ITEM_CATALOGUE = 0x6c0c08;

// CEnergyItem (zax_items.cpp): destructor 0x5afe30, MaxCharge 0x5afe50 (st0), Add 0x5afe90
void energy_item_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x5afe30)(o); }
long double max_charge(BYTE *o) { return fn<long double(THISCALL *)(void *)>(0x5afe50)(o); }
DWORD energy_add(BYTE *o, float v) { return fn<DWORD(THISCALL *)(void *, float)>(0x5afe90)(o, v); }
// The definition list object of the item catalogue (DefinitionList_Object 0x48d8f0)
BYTE *catalogue_object(DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(G_ITEM_CATALOGUE), i); }
// 0x585380 (fastcall object): the object when it is a CSphereOfZinData, else 0
BYTE *sphere_data_cast(BYTE *o) { return fn<BYTE *(__fastcall *)(void *, DWORD)>(0x585380)(o, 0); }
// The Sphere of Zin data of the item's definition (+0xcc of the catalogue object), as the exe looks it up
// each time
BYTE *item_sphere_data(BYTE *self) { return sphere_data_cast(bp(dw(catalogue_object(dw(self, 8)), 0xcc))); }
BYTE *item_sphere_data_kind(BYTE *self)
{
    BYTE *o = bp(dw(catalogue_object(dw(self, 8)), 0xcc));
    return o && is_kind_of(o, class_at(fn_addr(0x5841c0))) ? o : nullptr;
}

}  // namespace

// ---- CCharacterInventoryItem -----------------------------------------------------------------------------------

// 0x57fd00 (static registration), 0x57fe80 (slot 2)
static CClassInfo *__cdecl CharItem_ClassInfo()
{
    return class_getter(0x7137b0, 0x622490 /* "CCharacterInventoryItem" */, 1.0f, energy_item_class, 0x24, 0, 0x57fd50, 0x57fd90, 0x57fee0);
}
static void CharItem_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_ENERGY_ITEM));
    vtable_and_defaults(o, vt(VT_CHAR_ITEM));
}
static void CharItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_CHAR_ITEM));
    energy_item_dtor(o);
}
// 0x57fd50
static void *__cdecl CharItem_Create() { return create_one<0x24, CharItem_construct>(); }
// 0x57fd90 (count in ecx)
static void *__fastcall CharItem_CreateArray(int count, int, int) { return create_array<0x24, CharItem_construct>(count); }
// 0x57fe00 (slot 1)
static void *THISCALL CharItem_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, CharItem_destroy>(self, flags); }
// 0x57fee0. eax: the "Current Charge" descriptor.
static BYTE *__cdecl CharItem_RegisterFields()
{
    BYTE *f = static_cast<BYTE *>(new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6224b8) /* "Special Ability Level" */, glob_addr(0x6218c8) /* "+1" */, 0x1c, 4,
                                            0x1c, 1, 0, 0, 0, 0));
    add_field(CharItem_ClassInfo(), f);
    dw(f, 0x38) = glob_addr(0x6be330);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(TEXT_EMPTY));
    void *g = new_field(0x30, BYTE_FIELD, glob_addr(0x6224a8) /* "Is Discharging" */, 0x20, 1, 0x20, 0, 0, 0);
    add_field(CharItem_ClassInfo(), g);
    BYTE *charge = find_field(CharItem_ClassInfo(), glob_addr(0x621a18) /* "Current Charge" */, 0);
    dw(charge, 0x18) = 0;
    dw(charge, 0x34) = 0x3f800000;
    return charge;
}
// 0x580bd0 (on the definition: level): the level's CSpecialAbilitylevelInfo, 0 past the last
static BYTE *THISCALL CharDef_Level(BYTE *self, DWORD i) { return i < dw(self, 0x178) ? bp(dw(self, 0x17c) + i * 0x20) : nullptr; }
// 0x580070 (slot 51: inventory, seconds). While discharging the charge runs down over the level's
// "Duration" (slot 50); when it is empty the item stops (slot 49 with 0) and the level's "Deactivate
// Action" runs with the first character. Otherwise it charges over the level's "Charge Time" (CEnergyItem
// Add). Then, when the character has the items its type definition needs for the next level ("Advance/
// Item Needed To Advance", "Advance/Level"), they are used up (Part_UseItems), the level rises, the item
// becomes the inventory's current one and the dialogue window (0x47ca10) says "Character's %s special
// ability has advanced to level %d resulting in %s".
static void THISCALL CharItem_Update(BYTE *self, BYTE *inv, DWORD seconds)
{
    BYTE *def = item_definition(self);
    BYTE *cdef = def && is_kind_of(def, class_at(fn_addr(0x5807f0))) ? def : nullptr;
    DWORD ti = dw(cdef, 0x18);
    BYTE *type = nullptr;
    BYTE *records = *glob<BYTE **>(0x6c0830);
    if (records) {
        if (ti >= *glob<DWORD *>(0x6c0834))
            ti = 0;
        const DWORD off = ti << 7;
        if (!records[off + 0x7f]) {
            if (!dw(records + off, 0x78)) {
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(0x6c0800), ti);
                records = *glob<BYTE **>(0x6c0830);
            }
            BYTE *o = bp(dw(records + off, 0x78));
            if (o) {
                CClassInfo *c = *glob<CClassInfo **>(0x7137c0);
                if (!c) {
                    if (void *p = pool_alloc(sizeof(CClassInfo)))
                        fn<RegisterFn>(0x415780)(p, glob<CClassInfo **>(0x7137c0), glob<const char *>(0x62251c), 1.0f, class_at(fn_addr(0x5b9400)), 0x34, 0,
                                                 0x5810b0, 0x5810e0, 0x581230);
                    c = *glob<CClassInfo **>(0x7137c0);
                }
                if (is_kind_of(o, c))
                    type = o;
            }
        }
    }
    if (dw(self, 0x1c) >= dw(cdef, 0x178))
        return;
    BYTE *level = bp(dw(cdef, 0x17c) + (dw(self, 0x1c) << 5));
    if (!level)
        return;
    if (self[0x20]) {
        const float v = fstp(max_charge(self) / fld(fl<float>(level, 0x10)) * fld(raw_float(seconds)));
        if (!(vcall<DWORD>(self, 50, v) & 0xff)) {
            vcall<DWORD>(self, 49, 0);
            BYTE *ch = fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world());
            BYTE *d2 = item_definition(self);
            BYTE *l2 = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x580bd0)(d2 && is_kind_of(d2, class_at(fn_addr(0x5807f0))) ? d2 : nullptr, dw(self, 0x1c));
            self[0x20] = 0;
            if (l2 && dw(l2, 0x1c))
                action_execute(bp(dw(l2, 0x1c)), bp(dw(ch, 0x48)), ch, nullptr, ch, self, l2 + 0x1c);
        }
    } else {
        energy_add(self, fstp(max_charge(self) / fld(fl<float>(level, 0xc)) * fld(raw_float(seconds))));
    }
    if (dw(self, 0x1c) >= dw(type, 0x30))
        return;
    const DWORD item = dw(type, 0x28), needed = dw(bp(dw(type, 0x2c)), dw(self, 0x1c) * 4);
    const DWORD have = fn<DWORD(__fastcall *)(void *, DWORD)>(0x4268d0)(inv, item);
    if (!needed || have < needed)
        return;
    fn<DWORD(__fastcall *)(void *, DWORD, DWORD)>(0x426930)(fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world()), item, needed);
    dw(self, 0x1c)++;
    dw(inv, 0xc) = fn<DWORD(THISCALL *)(void *, void *)>(0x4262a0)(inv, self);
    const char *ability = fn<const char *(THISCALL *)(void *, DWORD)>(0x591e10)(glob<void *>(0x713790), dw(cdef, 0x174));
    BYTE speaker[4], text[4];
    zs_ctor(text);
    zs_ctor(speaker);
    const char *result;
    if (dw(self, 0x1c) < dw(cdef, 0x178) && dw(cdef, 0x17c) + (dw(self, 0x1c) << 5))
        result = zs_c_str(bp(dw(cdef, 0x17c) + (dw(self, 0x1c) << 5) + 8));
    else
        result = glob<const char *>(0x622518);
    fn<int(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(text,
                                                            glob<const char *>(0x6224d0) /* "Character's %s special ability has advanced to level "
                                                                                          "%d resulting in %s" */,
                                                            ability, dw(self, 0x1c) + 1, result);
    fn<DWORD(__fastcall *)(void *, void *, DWORD, DWORD)>(0x47ca10)(speaker, text, 0, 0);
    zs_dtor(speaker);
    zs_dtor(text);
}

// ---- CSpecialAbilitylevelInfo ----------------------------------------------------------------------------------

// 0x580470 (slot 2 and the static registration); no parent class
static CClassInfo *__cdecl LevelInfo_ClassInfo()
{
    return class_getter(0x7137b4, 0x622564 /* "CSpecialAbilitylevelInfo" */, 2.0f, nullptr, 0x20, 0, 0x580320, 0x580360, 0x5804d0);
}
static void LevelInfo_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 8);
    vtable_and_defaults(o, vt(VT_LEVEL_INFO));
}
static void LevelInfo_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_LEVEL_INFO));
    zs_dtor(o + 8);
    obj_dtor(o);
}
// 0x580320
static void *__cdecl LevelInfo_Create() { return create_one<0x20, LevelInfo_construct>(); }
// 0x580360 (count in ecx)
static void *__fastcall LevelInfo_CreateArray(int count, int, int) { return create_array<0x20, LevelInfo_construct>(count); }
// 0x5803d0 (slot 1)
static void *THISCALL LevelInfo_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, LevelInfo_destroy>(self, flags); }
// 0x5804d0. eax: ClassInfo_AddField's.
static DWORD __cdecl LevelInfo_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD_CTOR, glob_addr(0x61b348) /* "Description" */, 8, 4, 8, 1, glob_addr(0x6225cc) /* "increased power and longer durations" */, 0);
    add_field(LevelInfo_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6225c0) /* "Charge Time" */, glob_addr(0x60b000) /* "Seconds" */, 0xc, 4, 0xc, 1, 0x41200000, 0, 0, 0x497423f0,
                  0);
    add_field(LevelInfo_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6225b4) /* "Duration" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0x3f800000, 0, 0, 0x497423f0, 0);
    add_field(LevelInfo_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6225a4) /* "Activate Sound" */, 0x14, 4, 0x14, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(LevelInfo_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x622594) /* "Activate Action" */, 0x18, 4, 0x18, action_class(), 1, 0, 0, 0);
    add_field(LevelInfo_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x622580) /* "Deactivate Action" */, 0x1c, 4, 0x1c, action_class(), 1, 0, 0, 0);
    return add_field(LevelInfo_ClassInfo(), f);
}

// ---- CCharacterInventoryItemDefinition -------------------------------------------------------------------------

// 0x5807f0 (static registration), 0x5809e0 (slot 2)
static CClassInfo *__cdecl CharDef_ClassInfo()
{
    return class_getter(0x7137b8, 0x622540 /* "CCharacterInventoryItemDefinition" */, 2.0f, getter<0x486220>, 0x180, 0, 0x580850, 0x5808a0,
                        0x580a50);
}
static void CharDef_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x5b7380)(o);           // CZaxInventoryItemDefinition
    fn<void *(THISCALL *)(void *)>(0x486c20)(o + 0x100);   // CEnergyMeter
    vtable_and_defaults(o, vt(VT_ZAX_ITEM_DEF_LEVEL));
    vtable_and_defaults(o, vt(VT_CHAR_DEF));
}
static void CharDef_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_CHAR_DEF));
    vtable_and_free(o, vt(VT_ZAX_ITEM_DEF_LEVEL));
    fn<void(THISCALL *)(void *)>(0x486cf0)(o + 0x100);
    fn<void(THISCALL *)(void *)>(0x5b73f0)(o);
}
// 0x580850
static void *__cdecl CharDef_Create() { return create_one<0x180, CharDef_construct>(); }
// 0x5808a0 (count in ecx)
static void *__fastcall CharDef_CreateArray(int count, int, int) { return create_array<0x180, CharDef_construct>(count); }
// 0x580920 (slot 1)
static void *THISCALL CharDef_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x180, CharDef_destroy>(self, flags); }
// 0x580a50. eax: ClassInfo_AddField's.
static DWORD __cdecl CharDef_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x622604) /* "Special/Special Ability" */, 0x174, 4, 0x174, glob_addr(0x713790), 1, 0, 0, 0);
    add_field(CharDef_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(LIST_FIELD_CTOR)(
            f, glob_addr(0x6225f4) /* "Special/Level" */, 0x178, 4, 0x178, 0x17c, 4, 0x17c, LevelInfo_ClassInfo(), 1, 0);
    return add_field(CharDef_ClassInfo(), f);
}

// ---- CCharacterInventoryTypeDefinition -------------------------------------------------------------------------

// 0x581060 (static registration), 0x5811d0 (slot 2)
static CClassInfo *__cdecl CharType_ClassInfo()
{
    return class_getter(0x7137c0, 0x62251c /* "CCharacterInventoryTypeDefinition" */, 1.0f, getter<0x5b9400>, 0x34, 0, 0x5810b0, 0x5810e0,
                        0x581230);
}
static void CharType_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x5b9b50)(o);   // CZaxInventoryTypeDefinition
    vtable_and_defaults(o, vt(VT_CHAR_TYPE));
}
static void CharType_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_CHAR_TYPE));
    fn<void(THISCALL *)(void *)>(0x5b9b30)(o);
}
// 0x5810b0
static void *__cdecl CharType_Create() { return create_one<0x34, CharType_construct>(); }
// 0x5810e0 (count in ecx)
static void *__fastcall CharType_CreateArray(int count, int, int) { return create_array<0x34, CharType_construct>(count); }
// 0x581140 (slot 1)
static void *THISCALL CharType_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, CharType_destroy>(self, flags); }
// 0x581230. eax: ClassInfo_AddField's.
static DWORD __cdecl CharType_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6226b4) /* "Advance/Item Needed To Advance" */, 0x28, 4, 0x28, glob_addr(G_ITEM_CATALOGUE), 1, 0, 0, 0);
    add_field(CharType_ClassInfo(), f);
    void *element = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6226a0) /* "Required to advance" */, 0, 4, 0, 1, 0, 0, 0, 0);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR, glob_addr(0x622690) /* "Advance/Level" */, 0x30, 4, 0x30, 0x2c, 4, 0x2c, element, 1, 0);
    return add_field(CharType_ClassInfo(), f);
}

// ---- CSphereOfZinEnergyInventoryItem ---------------------------------------------------------------------------

// 0x584510 (static registration), 0x584730 (slot 2)
static CClassInfo *__cdecl SphereItem_ClassInfo()
{
    return class_getter(0x71381c, 0x622b48 /* "CSphereOfZinEnergyInventoryItem" */, 2.0f, energy_item_class, 0x34, 0, 0x584560, 0x5845c0, 0x584790);
}
static void SphereItem_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_ENERGY_ITEM));
    dw(o, 0x24) = vt(VT_BYTE_ARRAY);
    dw(o, 0x28) = 0;
    dw(o, 0x2c) = 0;
    dw(o, 0x30) = 0;
    vtable_and_defaults(o, vt(VT_SPHERE_ITEM));
}
static void SphereItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SPHERE_ITEM));
    dw(o, 0x24) = vt(VT_BYTE_ARRAY);
    if (dw(o, 0x28)) {
        heap_free(bp(dw(o, 0x28)));
        dw(o, 0x28) = 0;
        dw(o, 0x30) = 0;
    }
    dw(o, 0x2c) = 0;
    energy_item_dtor(o);
}
// 0x584560
static void *__cdecl SphereItem_Create() { return create_one<0x34, SphereItem_construct>(); }
// 0x5845c0 (count in ecx)
static void *__fastcall SphereItem_CreateArray(int count, int, int) { return create_array<0x34, SphereItem_construct>(count); }
// 0x584650 (slot 1)
static void *THISCALL SphereItem_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, SphereItem_destroy>(self, flags); }
// 0x584790. eax: ClassInfo_AddField's.
static DWORD __cdecl SphereItem_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622bc0) /* "Debug/Charge Last Update" */, 0x20, 4, 0x20, 1, 0, 0, 0, 0x49742400, 0);
    add_field(SphereItem_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622ba4) /* "Debug/Charge Next Update" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0x49742400, 0);
    add_field(SphereItem_ClassInfo(), f);
    void *element = new_field(0x30, BYTE_FIELD, glob_addr(0x622b90) /* "Retrigerable Now" */, 0, 1, 0, 1, 1, 0);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x622b68) /* "Debug/List of ReTriggerable Thresholds" */, 0x2c, 4, 0x2c, 0x30, 4, 0x30, 0x28, 4,
                  0x28, 0, element, 1, 0);
    return add_field(SphereItem_ClassInfo(), f);
}
// 0x584960 (slot 29, three arguments unused): the next charge becomes the current one, the current the
// last one. eax: the old charge.
static DWORD THISCALL SphereItem_Step(BYTE *self, DWORD, DWORD, DWORD)
{
    const DWORD old = dw(self, 0x18);
    dw(self, 0x20) = old;
    dw(self, 0x18) = dw(self, 0x1c);
    dw(self, 0x1c) = 0;
    return old;
}
// 0x584980 (slot 30: entity, two more unused): the thresholds of the Sphere of Zin data (+0xc records of
// 0x18 bytes: +8 the charge, +0xc the margin, +0x10 the action, +0x14 0 rising only, 1 falling only,
// else both): an armed threshold fires its action (CAction_ExecuteFrom with the entity's layer and the
// item as the caller) when the charge crossed it since the last update (+0x20 to +0x18) and disarms; it is
// armed again once the charge is at least the margin away from it. The armed flags grow with the data's
// thresholds (array slot 2, armed) and shrink to their number. The data is looked up anew (definition
// list object of the item catalogue, +0xcc, 0x585380 or IsKindOf) at every use, as the exe does.
static void THISCALL SphereItem_Thresholds(BYTE *self, BYTE *e, DWORD, DWORD)
{
    if (!item_sphere_data(self))
        return;
    DWORD n = dw(self, 0x2c);
    if (n < dw(item_sphere_data(self), 0x10)) {
        do {
            BYTE armed = 1;
            vcall<DWORD>(self + 0x24, 2, &armed);
            n = dw(self, 0x2c);
        } while (n < dw(item_sphere_data(self), 0x10));
    }
    DWORD i = 0, off = 0;
    if (dw(item_sphere_data(self), 0x10)) {
        do {
            auto record = [&]() { return bp(dw(item_sphere_data(self), 0xc)) + off; };
            bool fire = false;
            if (bp(dw(self, 0x28))[i] && c0(fld(fl<float>(self, 0x20)), fld(fl<float>(record(), 8))) &&
                !c0(fld(fl<float>(self, 0x18)), fld(fl<float>(record(), 8))) && dw(record(), 0x14) != 1)
                fire = true;
            else if (bp(dw(self, 0x28))[i] && !c0(fld(fl<float>(self, 0x20)), fld(fl<float>(record(), 8))) &&
                     c0(fld(fl<float>(self, 0x18)), fld(fl<float>(record(), 8))) && dw(record(), 0x14))
                fire = true;
            if (fire) {
                BYTE *layer = bp(dw(e, 0x48));
                BYTE *action = bp(dw(record(), 0x10));
                fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, void *, void *)>(0x41a740)(action, layer, 0, 0, e, self);
                bp(dw(self, 0x28))[i] = 0;
            }
            BYTE *rec = record();
            BYTE *d = item_sphere_data_kind(self);
            if (c03(fld(fl<float>(bp(dw(d, 0xc)) + off, 0xc)) + fld(fl<float>(rec, 8)), fld(fl<float>(self, 0x18)))) {
                bp(dw(self, 0x28))[i] = 1;
            } else {
                BYTE *d1 = item_sphere_data_kind(self);
                BYTE *d2 = item_sphere_data_kind(self);
                if (!c0(fld(fl<float>(bp(dw(d1, 0xc)) + off, 8)) - fld(fl<float>(bp(dw(d2, 0xc)) + off, 0xc)), fld(fl<float>(self, 0x18))))
                    bp(dw(self, 0x28))[i] = 1;
            }
            i++;
            off += 0x18;
        } while (i < dw(item_sphere_data(self), 0x10));
    }
    for (;;) {
        const DWORD count = dw(self, 0x2c);
        if (count <= dw(item_sphere_data_kind(self), 0x10))
            return;
        dw(self, 0x2c)--;
    }
}
// 0x584d40 (source part, receiving part, amount): evil broadcast within a layer charges the item: the
// amount times the Sphere of Zin options' distance (+8; with +0xc set also the least squared distance)
// over the squared distance between the parts, as a share of what is missing to the maximum charge,
// added to the next charge (+0x1c), at most the maximum
static void THISCALL SphereItem_ReceiveEvil(BYTE *self, BYTE *from, BYTE *to, DWORD amount)
{
    if (c03(max_charge(self), fld(fl<float>(self, 0x1c))))
        return;
    if (!dw(from, 0x48) || !dw(to, 0x48) || dw(from, 0x48) != dw(to, 0x48))
        return;
    float p1[2], p2[2];
    part_position(from, p1);
    part_position(to, p2);
    const long double dy = fld(p1[1]) - fld(p2[1]), dx = fld(p1[0]) - fld(p2[0]);
    float d2 = fstp(dy * dy + dx * dx);
    BYTE *holder = bp(dw(*glob<BYTE **>(0x713f14), 0x2e0));
    BYTE *rules = bp(dw(holder, 0x38));
    if (!rules || !is_kind_of(rules, class_at(fn_addr(0x5a1350))))
        rules = nullptr;
    BYTE *opts = fn<BYTE *(THISCALL *)(void *)>(0x5a21f0)(rules);
    if (opts[0xc] && c0(fld(d2), fld(fl<float>(opts, 8))))
        d2 = fl<float>(opts, 8);
    const float k = fstp(fld(raw_float(amount)) * fld(fl<float>(opts, 8)) / fld(d2));
    fl<float>(self, 0x1c) = fstp((max_charge(self) - fld(fl<float>(self, 0x1c))) * fld(k) + fld(fl<float>(self, 0x1c)));
    if (c0(max_charge(self), fld(fl<float>(self, 0x1c))))
        fl<float>(self, 0x1c) = fstp(max_charge(self));
}

// ---- Hooks ---------------------------------------------------------------------------------------------------

ZAX_HOOKS(char_items)
{
    const Hook list[] = {
#include "char_items_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
