// The classes of the HUD and of the inventory screen's detail pane (phase 4, rendering): class getters (both
// copies where the exe has two: the static registration's and slot 2), Create, CreateArray, deleting
// destructors (slot 1) and RegisterFields. Their drawing and network slots are in hud_render.cpp.
//
// Families [confirmed: constructors, destructors, RegisterFields, docs/classes.csv, docs/fields.csv]:
//  - CInventoryHUDRenderer (abstract, 8 bytes, vtable 0x5edf7c, class 0x7140d4): the object an item
//    definition's "Display/HUD Renderer" holds; slot 25 updates a render data object from the player's
//    item, slot 26 reads one from a packet, slot 28 draws it into the HUD. Its classes: CCompassRenderer,
//    CFlagInventoryHUDRenderer, CWeaponInventoryHUDRenderer, CShieldInventoryHUDRenderer,
//    CEnergyInventoryHUDRenderer with CCharacterInventoryHUDRenderer and
//    CSphereOfZinEnergyInventoryHUDRenderer under it.
//  - CInventoryHUDRenderData (0x14, vtable 0x5ede94, class 0x7140d0, no parent): +8 "Opacity", +0xc
//    "Definition Index" (the item catalogue), +0x10 "Changed Status" (bit 1 opacity, bit 2 index); the
//    values the HUD draws, kept per player (net game) and sent when changed. Its classes: CCompassRenderData,
//    CWeaponHUDRenderData, CShieldHUDRenderData (with an embedded pointer array of CEnergyHUDRenderData),
//    CEnergyHUDRenderData.
//  - CInventoryDetailRenderer (abstract, 8 bytes, vtable 0x5f10d8, class 0x7140d8): an item definition's
//    "Display/Detail Renderer", draws the item's detail in the inventory screen (slot 24) and says how big
//    it is (slot 25). Its classes: the weapon, animation overlay, random overlay, count, ammunition,
//    mineral, charger, health, deployable, battery, energy text, shield, character and Sphere of Zin
//    detail renderers (the segmented and the watch detail renderers are done: ai_shield.cpp,
//    quest_data.cpp).
//  - CHudBarOptions (0x60, vtable 0x5f9e98, class 0x6c2dc0, version 2): the auto-map's layout, sounds,
//    zoom and colours (one of the custom rules' option objects); its three default colours are function-
//    local statics of RegisterFields (guard 0x6c2d7c bits 1, 2, 4), built through the exe's guard, objects
//    and atexit functions, so the reimplementation has no static of its own.
// Constructors are inlined one level per class (CObject, the parent level, the members, the vtable,
// Object_InitFields); destructors the other way round. Members: "Format" strings are Local.txt entries
// (two ZStrings, constructor 0x500740); CHudBarOptions's colours FieldColors (0x53f000, no destructor).
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;       // 0x34: ZString value (+0x28 = 1)
constexpr DWORD POINT_FIELD_CTOR_INT_XY = 0x545530;  // 0x34: integer point
constexpr DWORD COLOR_FIELD_CTOR = 0x46dcc0;         // 0x3c: colour

void local_entry_ctor(BYTE *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }   // two ZStrings (Local.txt)
void field_color_ctor(BYTE *c) { fn<void *(THISCALL *)(void *)>(0x53f000)(c); }
DWORD zs_assign(void *z, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4e1930)(z, at<const char *>(text)); }
DWORD add_enum_float(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54ed60)(f, value, at<const char *>(text)); }
// A function-local static colour of the exe: its guard bit, object and atexit function
void static_color_bit(DWORD guard, BYTE bit, DWORD color, DWORD r, DWORD g, DWORD b, DWORD a, DWORD exit_fn)
{
    BYTE &bits = *at<BYTE *>(guard);
    if (bits & bit)
        return;
    bits |= bit;
    field_color_rgba(color, r, g, b, a);
    crt_atexit(at<void(__cdecl *)()>(exit_fn));
}
// The embedded pointer array of CShieldHUDRenderData (+0 vtable, +4 data, +8 count, +0xc capacity)
void pointer_array_dtor(BYTE *a, DWORD vtable)
{
    dw(a, 0) = vtable;
    if (dw(a, 4)) {
        heap_free(bp(dw(a, 4)));
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
}

}  // namespace

// Getters used before their definition
static CClassInfo *__cdecl InventoryDetailRenderer_ClassInfo();
static CClassInfo *__cdecl InventoryHUDRenderer_ClassInfo();
static CClassInfo *__cdecl InventoryHUDRenderData_ClassInfo();
static CClassInfo *__cdecl EnergyInventoryHUDRenderer_ClassInfo();
static CClassInfo *__cdecl EnergyHUDRenderData_ClassInfo();

// ---- CAmmunitionInventoryDetailRenderer ------------------------------------------------------------------

// 0x5b1500, 0x5b1690
static CClassInfo *__cdecl AmmunitionInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x714350, 0x626598 /* "CAmmunitionInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x5b1550, 0x5b1590, 0x58ea40);
}
static void AmmunitionInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x604738));
}
static void AmmunitionInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x604738));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5b1550
static void *__cdecl AmmunitionInventoryDetailRenderer_Create() { return create_one<0x8, AmmunitionInventoryDetailRenderer_construct>(); }
// 0x5b1590 (count in ecx)
static void *__fastcall AmmunitionInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, AmmunitionInventoryDetailRenderer_construct>(count); }
// 0x5b15f0 (slot 1)
static void *THISCALL AmmunitionInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, AmmunitionInventoryDetailRenderer_destroy>(self, flags); }

// ---- CAnimationOverlayInventoryDetailRenderer ------------------------------------------------------------

// 0x5adb50
static CClassInfo *__cdecl AnimationOverlayInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x7140e0, 0x62618c /* "CAnimationOverlayInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x10, 0, 0x5ada10, 0x5ada50, 0x5adc00);
}
static void AnimationOverlayInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x604050));
}
static void AnimationOverlayInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x604050));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5ada10
static void *__cdecl AnimationOverlayInventoryDetailRenderer_Create() { return create_one<0x10, AnimationOverlayInventoryDetailRenderer_construct>(); }
// 0x5ada50 (count in ecx)
static void *__fastcall AnimationOverlayInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x10, AnimationOverlayInventoryDetailRenderer_construct>(count); }
// 0x5adab0 (slot 1)
static void *THISCALL AnimationOverlayInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, AnimationOverlayInventoryDetailRenderer_destroy>(self, flags); }
// 0x5adc00
static DWORD __cdecl AnimationOverlayInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x622970) /* "Detail Image" */, 8, 4, 8, glob_addr(0x6c0948), 1, 0, 0, 0);
    add_field(AnimationOverlayInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x622960) /* "Next Renderer" */, 0xc, 4, 0xc, InventoryDetailRenderer_ClassInfo(), 1, 0, 0, 0);
    add_field(AnimationOverlayInventoryDetailRenderer_ClassInfo(), f);
    CClassInfo *k = AnimationOverlayInventoryDetailRenderer_ClassInfo();
    dw(k, 8) = glob_addr(0x6261b8);   // alternative name "CAnimationOverlayDetailRenderer"
    return ptr_value(k);
}

// ---- CBatteryInventoryDetailRenderer ---------------------------------------------------------------------

// 0x4839a0, 0x483b30
static CClassInfo *__cdecl BatteryInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x6bdcb8, 0x612198 /* "CBatteryInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x4839f0, 0x483a30, 0x58ea40);
}
static void BatteryInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x5f192c));
}
static void BatteryInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f192c));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x4839f0
static void *__cdecl BatteryInventoryDetailRenderer_Create() { return create_one<0x8, BatteryInventoryDetailRenderer_construct>(); }
// 0x483a30 (count in ecx)
static void *__fastcall BatteryInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, BatteryInventoryDetailRenderer_construct>(count); }
// 0x483a90 (slot 1)
static void *THISCALL BatteryInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, BatteryInventoryDetailRenderer_destroy>(self, flags); }

// ---- CCharacterInventoryDetailRenderer -------------------------------------------------------------------

// 0x580c00, 0x580d90
static CClassInfo *__cdecl CharacterInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x7137bc, 0x62261c /* "CCharacterInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x580c50, 0x580c90, 0x58ea40);
}
static void CharacterInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x600fbc));
}
static void CharacterInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x600fbc));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x580c50
static void *__cdecl CharacterInventoryDetailRenderer_Create() { return create_one<0x8, CharacterInventoryDetailRenderer_construct>(); }
// 0x580c90 (count in ecx)
static void *__fastcall CharacterInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, CharacterInventoryDetailRenderer_construct>(count); }
// 0x580cf0 (slot 1)
static void *THISCALL CharacterInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, CharacterInventoryDetailRenderer_destroy>(self, flags); }

// ---- CCharacterInventoryHUDRenderer ----------------------------------------------------------------------

// 0x581ea0, 0x5820d0
static CClassInfo *__cdecl CharacterInventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x7137cc, 0x62272c /* "CCharacterInventoryHUDRenderer" */, 1.0f, EnergyInventoryHUDRenderer_ClassInfo, 0xc, 0, 0x581f50, 0x581fa0, 0x582190);
}
static void CharacterInventoryHUDRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x601080));
    vtable_and_defaults(o, vt(0x6010fc));
}
static void CharacterInventoryHUDRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x6010fc));
    vtable_and_free(o, vt(0x601080));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x581f50
static void *__cdecl CharacterInventoryHUDRenderer_Create() { return create_one<0xc, CharacterInventoryHUDRenderer_construct>(); }
// 0x581fa0 (count in ecx)
static void *__fastcall CharacterInventoryHUDRenderer_CreateArray(int count, int, int) { return create_array<0xc, CharacterInventoryHUDRenderer_construct>(count); }
// 0x582010 (slot 1)
static void *THISCALL CharacterInventoryHUDRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, CharacterInventoryHUDRenderer_destroy>(self, flags); }
// 0x582190
static DWORD __cdecl CharacterInventoryHUDRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610ea0) /* "Font" */, 8, 4, 8, glob_addr(0x6be1e0), 1, 0, 0, 0);
    return add_field(CharacterInventoryHUDRenderer_ClassInfo(), f);
}

// ---- CChargerInventoryDetailRenderer ---------------------------------------------------------------------

// 0x5b1f70, 0x5b2100
static CClassInfo *__cdecl ChargerInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x714358, 0x6265dc /* "CChargerInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x5b1fc0, 0x5b2000, 0x58ea40);
}
static void ChargerInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x604808));
}
static void ChargerInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x604808));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5b1fc0
static void *__cdecl ChargerInventoryDetailRenderer_Create() { return create_one<0x8, ChargerInventoryDetailRenderer_construct>(); }
// 0x5b2000 (count in ecx)
static void *__fastcall ChargerInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, ChargerInventoryDetailRenderer_construct>(count); }
// 0x5b2060 (slot 1)
static void *THISCALL ChargerInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, ChargerInventoryDetailRenderer_destroy>(self, flags); }

// ---- CCompassRenderData ----------------------------------------------------------------------------------

// 0x423110, 0x4232b0
static CClassInfo *__cdecl CompassRenderData_ClassInfo()
{
    return class_getter(0x62e9e4, 0x60b564 /* "CCompassRenderData" */, 1.0f, InventoryHUDRenderData_ClassInfo, 0x1c, 0, 0x423160, 0x4231a0, 0x423310);
}
static void CompassRenderData_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ede94));
    vtable_and_defaults(o, vt(0x5ede28));
}
static void CompassRenderData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ede28));
    vtable_and_free(o, vt(0x5ede94));
    obj_dtor(o);
}
// 0x423160
static void *__cdecl CompassRenderData_Create() { return create_one<0x1c, CompassRenderData_construct>(); }
// 0x4231a0 (count in ecx)
static void *__fastcall CompassRenderData_CreateArray(int count, int, int) { return create_array<0x1c, CompassRenderData_construct>(count); }
// 0x423210 (slot 1)
static void *THISCALL CompassRenderData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, CompassRenderData_destroy>(self, flags); }
// 0x423310
static DWORD __cdecl CompassRenderData_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b5a4) /* "Display Frame" */, 0x14, 1, 0x14, 1, 0, 0);
    add_field(CompassRenderData_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b594) /* "Frame Changed" */, 0x16, 1, 0x16, 1, 0, 0);
    add_field(CompassRenderData_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b588) /* "Any Change" */, 0x15, 1, 0x15, 1, 1, 0);
    add_field(CompassRenderData_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x60b578) /* "Frame Number" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0);
    return add_field(CompassRenderData_ClassInfo(), f);
}

// ---- CCompassRenderer ------------------------------------------------------------------------------------

// 0x423670, 0x423860
static CClassInfo *__cdecl CompassRenderer_ClassInfo()
{
    return class_getter(0x62e9e8, 0x60b5b4 /* "CCompassRenderer" */, 1.0f, InventoryHUDRenderer_ClassInfo, 0xc, 0, 0x423710, 0x423750, 0x4238c0);
}
static void CompassRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x5edf00));
}
static void CompassRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5edf00));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x423710
static void *__cdecl CompassRenderer_Create() { return create_one<0xc, CompassRenderer_construct>(); }
// 0x423750 (count in ecx)
static void *__fastcall CompassRenderer_CreateArray(int count, int, int) { return create_array<0xc, CompassRenderer_construct>(count); }
// 0x4237c0 (slot 1)
static void *THISCALL CompassRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, CompassRenderer_destroy>(self, flags); }
// 0x4238c0
static DWORD __cdecl CompassRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60b5c8) /* "HUD Overlay" */, 8, 4, 8, glob_addr(0x6c06a0), 1, 0, 0, 0);
    return add_field(CompassRenderer_ClassInfo(), f);
}

// ---- CCountInventoryDetailRenderer -----------------------------------------------------------------------

// 0x5b0f10, 0x5b1100
static CClassInfo *__cdecl CountInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x71434c, 0x6264e0 /* "CCountInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x14, 0, 0x5b0f60, 0x5b0fb0, 0x5b1160);
}
static void CountInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    local_entry_ctor(o + 0x8);
    vtable_and_defaults(o, vt(0x6046d0));
}
static void CountInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x6046d0));
    zs_dtor(o + 0xc);
    zs_dtor(o + 0x8);
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5b0f60
static void *__cdecl CountInventoryDetailRenderer_Create() { return create_one<0x14, CountInventoryDetailRenderer_construct>(); }
// 0x5b0fb0 (count in ecx)
static void *__fastcall CountInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x14, CountInventoryDetailRenderer_construct>(count); }
// 0x5b1030 (slot 1)
static void *THISCALL CountInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, CountInventoryDetailRenderer_destroy>(self, flags); }
// 0x5b1160
static DWORD __cdecl CountInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x610ea8) /* "Format" */, 8, 8, 8, 1, glob_addr(0x62658c) /* "Items: %d" */, 0);
    add_field(CountInventoryDetailRenderer_ClassInfo(), f);
    set_help(last_field(CountInventoryDetailRenderer_ClassInfo()), 0x626500 /* "Text to be printed in the detail window.  MUST contain a %d which w..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610ea0) /* "Font" */, 0x10, 4, 0x10, glob_addr(0x6be1e0), 1, 0, 0, 0);
    return add_field(CountInventoryDetailRenderer_ClassInfo(), f);
}

// ---- CDeployableInventoryDetailRenderer ------------------------------------------------------------------

// 0x47abf0, 0x47ae20
static CClassInfo *__cdecl DeployableInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x6bdaa8, 0x610e7c /* "CDeployableInventoryDetailRenderer" */, 0.0f, InventoryDetailRenderer_ClassInfo, 0x14, 0, 0x47ac80, 0x47acd0, 0x47ae80);
}
static void DeployableInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    local_entry_ctor(o + 0x8);
    vtable_and_defaults(o, vt(0x5f1070));
}
static void DeployableInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1070));
    zs_dtor(o + 0xc);
    zs_dtor(o + 0x8);
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x47ac80
static void *__cdecl DeployableInventoryDetailRenderer_Create() { return create_one<0x14, DeployableInventoryDetailRenderer_construct>(); }
// 0x47acd0 (count in ecx)
static void *__fastcall DeployableInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x14, DeployableInventoryDetailRenderer_construct>(count); }
// 0x47ad50 (slot 1)
static void *THISCALL DeployableInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, DeployableInventoryDetailRenderer_destroy>(self, flags); }
// 0x47ae80
static DWORD __cdecl DeployableInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x610ea8) /* "Format" */, 8, 8, 8, 1, glob_addr(0x610eb0) /* "Damage: %d%%" */, 0);
    add_field(DeployableInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610ea0) /* "Font" */, 0x10, 4, 0x10, glob_addr(0x6be1e0), 1, 0, 0, 0);
    return add_field(DeployableInventoryDetailRenderer_ClassInfo(), f);
}

// ---- CEnergyHUDRenderData --------------------------------------------------------------------------------

// 0x581380, 0x581510
static CClassInfo *__cdecl EnergyHUDRenderData_ClassInfo()
{
    return class_getter(0x7137c4, 0x6226d4 /* "CEnergyHUDRenderData" */, 1.0f, InventoryHUDRenderData_ClassInfo, 0x20, 0, 0x5813d0, 0x581410, 0x581570);
}
static void EnergyHUDRenderData_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ede94));
    vtable_and_defaults(o, vt(0x5ffd30));
}
static void EnergyHUDRenderData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ffd30));
    vtable_and_free(o, vt(0x5ede94));
    obj_dtor(o);
}
// 0x5813d0
static void *__cdecl EnergyHUDRenderData_Create() { return create_one<0x20, EnergyHUDRenderData_construct>(); }
// 0x581410 (count in ecx)
static void *__fastcall EnergyHUDRenderData_CreateArray(int count, int, int) { return create_array<0x20, EnergyHUDRenderData_construct>(count); }
// 0x581470 (slot 1)
static void *THISCALL EnergyHUDRenderData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, EnergyHUDRenderData_destroy>(self, flags); }
// 0x581570
static DWORD __cdecl EnergyHUDRenderData_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622704) /* "Capacity" */, 0x14, 4, 0x14, 1, 0, 0, 0, 0, 0);
    add_field(EnergyHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x612134) /* "Charge" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0, 0);
    add_field(EnergyHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6226ec) /* "EnergyChanged Status" */, 0x1c, 4, 0x1c, 1, 3, 0, 0, 0);
    return add_field(EnergyHUDRenderData_ClassInfo(), f);
}

// ---- CEnergyInventoryHUDRenderer -------------------------------------------------------------------------

// 0x5818a0, 0x581a30
static CClassInfo *__cdecl EnergyInventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x7137c8, 0x622710 /* "CEnergyInventoryHUDRenderer" */, 1.0f, InventoryHUDRenderer_ClassInfo, 0x8, 0, 0x5818f0, 0x581930, 0x58ea40);
}
static void EnergyInventoryHUDRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x601080));
}
static void EnergyInventoryHUDRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601080));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x5818f0
static void *__cdecl EnergyInventoryHUDRenderer_Create() { return create_one<0x8, EnergyInventoryHUDRenderer_construct>(); }
// 0x581930 (count in ecx)
static void *__fastcall EnergyInventoryHUDRenderer_CreateArray(int count, int, int) { return create_array<0x8, EnergyInventoryHUDRenderer_construct>(count); }
// 0x581990 (slot 1)
static void *THISCALL EnergyInventoryHUDRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, EnergyInventoryHUDRenderer_destroy>(self, flags); }

// ---- CEnergyTextInventoryDetailRenderer ------------------------------------------------------------------

// 0x483f50, 0x484160
static CClassInfo *__cdecl EnergyTextInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x6bdcc4, 0x6121d4 /* "CEnergyTextInventoryDetailRenderer" */, 0.0f, InventoryDetailRenderer_ClassInfo, 0x20, 0, 0x483fa0, 0x483ff0, 0x4841c0);
}
static void EnergyTextInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    local_entry_ctor(o + 0x8);
    local_entry_ctor(o + 0x10);
    vtable_and_defaults(o, vt(0x5f1994));
}
static void EnergyTextInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1994));
    zs_dtor(o + 0x14);
    zs_dtor(o + 0x10);
    zs_dtor(o + 0xc);
    zs_dtor(o + 0x8);
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x483fa0
static void *__cdecl EnergyTextInventoryDetailRenderer_Create() { return create_one<0x20, EnergyTextInventoryDetailRenderer_construct>(); }
// 0x483ff0 (count in ecx)
static void *__fastcall EnergyTextInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x20, EnergyTextInventoryDetailRenderer_construct>(count); }
// 0x484070 (slot 1)
static void *THISCALL EnergyTextInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, EnergyTextInventoryDetailRenderer_destroy>(self, flags); }
// 0x4841c0
static DWORD __cdecl EnergyTextInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61221c) /* "Singular Format" */, 8, 8, 8, 1, glob_addr(0x61222c) /* "%s Mj" */, 0);
    *glob<void **>(0x6bdcbc) = f;
    add_field(EnergyTextInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61220c) /* "Plural Format" */, 0x10, 8, 0x10, 1, glob_addr(0x61222c) /* "%s Mj" */, 0);
    *glob<void **>(0x6bdcc0) = f;
    add_field(EnergyTextInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6121f8) /* "Digits Of Precision" */, 0x18, 4, 0x18, 1, 1, 0, 0, 9);
    add_field(EnergyTextInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610ea0) /* "Font" */, 0x1c, 4, 0x1c, glob_addr(0x6be1e0), 1, 0, 0, 0);
    return add_field(EnergyTextInventoryDetailRenderer_ClassInfo(), f);
}

// ---- CFlagInventoryHUDRenderer ---------------------------------------------------------------------------

// 0x469b70, 0x469d00
static CClassInfo *__cdecl FlagInventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x6bd130, 0x60f5bc /* "CFlagInventoryHUDRenderer" */, 0.0f, InventoryHUDRenderer_ClassInfo, 0x10, 0, 0x469bc0, 0x469c00, 0x469d60);
}
static void FlagInventoryHUDRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x5ef608));
}
static void FlagInventoryHUDRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ef608));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x469bc0
static void *__cdecl FlagInventoryHUDRenderer_Create() { return create_one<0x10, FlagInventoryHUDRenderer_construct>(); }
// 0x469c00 (count in ecx)
static void *__fastcall FlagInventoryHUDRenderer_CreateArray(int count, int, int) { return create_array<0x10, FlagInventoryHUDRenderer_construct>(count); }
// 0x469c60 (slot 1)
static void *THISCALL FlagInventoryHUDRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, FlagInventoryHUDRenderer_destroy>(self, flags); }
// 0x469d60
static DWORD __cdecl FlagInventoryHUDRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60f5f8) /* "HUD Sequence" */, 8, 4, 8, glob_addr(0x6c06a0), 1, 0, 0, 0);
    add_field(FlagInventoryHUDRenderer_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x60f5dc) /* "Animation Speed" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0xc, 4, 0xc, 1, 0x3f800000 /* 1.0 */, 0, 0xc1200000 /* -10.0 */, 0x41200000 /* 10.0 */, 0);
    add_field(FlagInventoryHUDRenderer_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    return zs_assign(zstring(f, 0x20), glob_addr(0x60f5d8) /* "%" */);
}

// ---- CHealthInventoryDetailRenderer ----------------------------------------------------------------------

// 0x5b2390, 0x5b2520
static CClassInfo *__cdecl HealthInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x71435c, 0x6265fc /* "CHealthInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x5b23e0, 0x5b2420, 0x58ea40);
}
static void HealthInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x604870));
}
static void HealthInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x604870));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5b23e0
static void *__cdecl HealthInventoryDetailRenderer_Create() { return create_one<0x8, HealthInventoryDetailRenderer_construct>(); }
// 0x5b2420 (count in ecx)
static void *__fastcall HealthInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, HealthInventoryDetailRenderer_construct>(count); }
// 0x5b2480 (slot 1)
static void *THISCALL HealthInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, HealthInventoryDetailRenderer_destroy>(self, flags); }

// ---- CHudBarOptions --------------------------------------------------------------------------------------

// 0x507500
static CClassInfo *__cdecl HudBarOptions_ClassInfo()
{
    return class_getter(0x6c2dc0, 0x619730 /* "CHudBarOptions" */, 2.0f, nullptr, 0x60, 0, 0x5073b0, 0x507400, 0x507560);
}
static void HudBarOptions_construct(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x28);
    field_color_ctor(o + 0x38);
    field_color_ctor(o + 0x48);
    vtable_and_defaults(o, vt(0x5f9e98));
}
static void HudBarOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f9e98));
    obj_dtor(o);
}
// 0x5073b0
static void *__cdecl HudBarOptions_Create() { return create_one<0x60, HudBarOptions_construct>(); }
// 0x507400 (count in ecx)
static void *__fastcall HudBarOptions_CreateArray(int count, int, int) { return create_array<0x60, HudBarOptions_construct>(count); }
// 0x507480 (slot 1)
static void *THISCALL HudBarOptions_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x60, HudBarOptions_destroy>(self, flags); }
// 0x507560
static DWORD __cdecl HudBarOptions_RegisterFields()
{
    static_color_bit(glob_addr(0x6c2d7c), 1, glob_addr(0x6c2da8), 0xd, 0x18, 0x27, 0xff, fn_addr(0x507ea0));
    static_color_bit(glob_addr(0x6c2d7c), 2, glob_addr(0x6c2d90), 0, 0xff, 0, 0xff, fn_addr(0x507e90));
    static_color_bit(glob_addr(0x6c2d7c), 4, glob_addr(0x6c2d80), 0x10, 0, 0xff, 0xff, fn_addr(0x507e80));
    void *f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x619b10) /* "MiniMap Origin" */, 8, 8, 8, 1, 0xd, 0x11, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x619a90 /* "This is the position of the upper-left corner of the mini-map relat..." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x619a80) /* "MiniMap Size" */, 0x10, 8, 0x10, 1, 0x9b, 0x5f, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x619a3c /* "The size of the mini-map blitted within the MiniMap Window Frame." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x619a28) /* "MiniMap Deploy Time" */, glob_addr(0x60b000) /* "Seconds" */, 0x18, 4, 0x18, 1, 0x3ea8f5c3 /* 0.33 */, 0, 0, 0, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x6199f4 /* "The time the MiniMap window takes to open or close." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6199e0) /* "MiniMap Open Sound" */, 0x1c, 4, 0x1c, glob_addr(0x713708), 1, glob_addr(0x60bf8c) /* "Inventory Up.wav" */, 0, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6199cc) /* "MiniMap Close Sound" */, 0x20, 4, 0x20, glob_addr(0x713708), 1, glob_addr(0x60bfcc) /* "Sounds/Inventory Down.wav" */, 0, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6199b0) /* "Max Zoom Factor" */, glob_addr(0x6199c0) /* ":1 Pixels" */, 0x24, 4, 0x24, 1, 0x42400000 /* 48.0 */, 0, 0, 0, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x619918 /* "The Maximum amount the MiniMap will zoom in to fill the MiniMap win..." */);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x6198fc) /* "MiniMap Background Color" */, 0x28, 0x10, 0x28, 1, glob_addr(0x6c2da8), 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x6198dc /* "This is the Fog Of War color." */);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x6198c4) /* "MiniMap Player Color" */, 0x38, 0x10, 0x38, 1, glob_addr(0x6c2d90), 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x619884 /* "This is the color that the player will be drawn on the MiniMap." */);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x61986c) /* "MiniMap Intrest Color" */, 0x48, 0x10, 0x48, 1, glob_addr(0x6c2d80), 0);
    add_field(HudBarOptions_ClassInfo(), f);
    set_help(last_field(HudBarOptions_ClassInfo()), 0x619820 /* "This is the color that \"Points Of Intrest\" will be drawn on the Min..." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6197f8) /* "Fog Of War Update Distance" */, glob_addr(0x619814) /* "Pixels^2" */, 0x58, 4, 0x58, 1, 0x45800000 /* 4096.0 */, 0, 0, 0x4b189680 /* 10000000.0 */, 0);
    add_field(HudBarOptions_ClassInfo(), f);
    add_enum_float(f, *glob<DWORD *>(0x5f9e38), glob_addr(0x6197e8) /* "Always Update" */);
    dw(f, 0x3c) = glob_addr(0x6be350);
    zs_assign(zstring(f, 0x20), glob_addr(0x60b360) /* "Pixels" */);
    set_help(f, 0x6197a0 /* "The distance the screen must move before the Fog Of War gets updated." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61978c) /* "Mini-Map Opacity" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0x5c, 4, 0x5c, 1, 0x3f4ccccd /* 0.8 */, 0, 0, 0x3f800000 /* 1.0 */, 0x41a00000 /* 20.0 */);
    add_field(HudBarOptions_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign(zstring(f, 0x20), glob_addr(0x61904c) /* "Percent" */);
    return set_help(f, 0x619740 /* "How Opaque the Mini-Map (Auto Map) is.... (doesn't affect \"hidefog\"..." */);
}

// ---- CInventoryDetailRenderer ----------------------------------------------------------------------------

// 0x5acc00
static CClassInfo *__cdecl InventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x7140d8, 0x626094 /* "CInventoryDetailRenderer" */, 1.0f, nullptr, 0x8, 1, 0x46b760, 0x52dc80, 0x58ea40);
}

// ---- CInventoryHUDRenderData -----------------------------------------------------------------------------

// 0x5ac6b0
static CClassInfo *__cdecl InventoryHUDRenderData_ClassInfo()
{
    return class_getter(0x7140d0, 0x62604c /* "CInventoryHUDRenderData" */, 1.0f, nullptr, 0x14, 0, 0x5ac5a0, 0x5ac5d0, 0x5ac710);
}
static void InventoryHUDRenderData_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ede94));
}
static void InventoryHUDRenderData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ede94));
    obj_dtor(o);
}
// 0x5ac5a0
static void *__cdecl InventoryHUDRenderData_Create() { return create_one<0x14, InventoryHUDRenderData_construct>(); }
// 0x5ac5d0 (count in ecx)
static void *__fastcall InventoryHUDRenderData_CreateArray(int count, int, int) { return create_array<0x14, InventoryHUDRenderData_construct>(count); }
// 0x5ac630 (slot 1)
static void *THISCALL InventoryHUDRenderData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, InventoryHUDRenderData_destroy>(self, flags); }
// 0x5ac710
static DWORD __cdecl InventoryHUDRenderData_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x626074) /* "Opacity" */, 8, 4, 8, 1, 0, 0, 0, 0, 0);
    add_field(InventoryHUDRenderData_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61717c) /* "Definition Index" */, 0xc, 4, 0xc, glob_addr(0x6c0c08), 1, 0, 0, 0);
    add_field(InventoryHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x626064) /* "Changed Status" */, 0x10, 4, 0x10, 1, 3, 0, 0, 0);
    return add_field(InventoryHUDRenderData_ClassInfo(), f);
}

// ---- CInventoryHUDRenderer -------------------------------------------------------------------------------

// 0x5aca50
static CClassInfo *__cdecl InventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x7140d4, 0x62607c /* "CInventoryHUDRenderer" */, 1.0f, nullptr, 0x8, 1, 0x46b760, 0x52dc80, 0x58ea40);
}

// ---- CMineralInventoryDetailRenderer ---------------------------------------------------------------------

// 0x5b1a70, 0x5b1c00
static CClassInfo *__cdecl MineralInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x714354, 0x6265bc /* "CMineralInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x5b1ac0, 0x5b1b00, 0x58ea40);
}
static void MineralInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x6047a0));
}
static void MineralInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x6047a0));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5b1ac0
static void *__cdecl MineralInventoryDetailRenderer_Create() { return create_one<0x8, MineralInventoryDetailRenderer_construct>(); }
// 0x5b1b00 (count in ecx)
static void *__fastcall MineralInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, MineralInventoryDetailRenderer_construct>(count); }
// 0x5b1b60 (slot 1)
static void *THISCALL MineralInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, MineralInventoryDetailRenderer_destroy>(self, flags); }

// ---- CRandomOverlayInventoryDetailRenderer ---------------------------------------------------------------

// 0x5ae1e0
static CClassInfo *__cdecl RandomOverlayInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x7140e4, 0x6261d8 /* "CRandomOverlayInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x18, 0, 0x5ae080, 0x5ae0d0, 0x5ae290);
}
static void RandomOverlayInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x6040b8));
    dw(o, 0x10) = 0;   // the last index drawn and its time, cleared after Object_InitFields
    dw(o, 0x14) = 0;
}
static void RandomOverlayInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x6040b8));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5ae080
static void *__cdecl RandomOverlayInventoryDetailRenderer_Create() { return create_one<0x18, RandomOverlayInventoryDetailRenderer_construct>(); }
// 0x5ae0d0 (count in ecx)
static void *__fastcall RandomOverlayInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x18, RandomOverlayInventoryDetailRenderer_construct>(count); }
// 0x5ae140 (slot 1)
static void *THISCALL RandomOverlayInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, RandomOverlayInventoryDetailRenderer_destroy>(self, flags); }
// 0x5ae290
static DWORD __cdecl RandomOverlayInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x622970) /* "Detail Image" */, 8, 4, 8, glob_addr(0x6c0948), 1, 0, 0, 0);
    add_field(RandomOverlayInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x622960) /* "Next Renderer" */, 0xc, 4, 0xc, InventoryDetailRenderer_ClassInfo(), 1, 0, 0, 0);
    return add_field(RandomOverlayInventoryDetailRenderer_ClassInfo(), f);
}

// ---- CShieldHUDRenderData --------------------------------------------------------------------------------

// 0x56e640, 0x56e880
static CClassInfo *__cdecl ShieldHUDRenderData_ClassInfo()
{
    return class_getter(0x713524, 0x621990 /* "CShieldHUDRenderData" */, 1.0f, InventoryHUDRenderData_ClassInfo, 0x28, 0, 0x56e690, 0x56e6f0, 0x56e8e0);
}
static void ShieldHUDRenderData_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ede94));
    dw(o, 0x18) = vt(0x5ffd24);
    dw(o, 0x1c) = 0x0;
    dw(o, 0x20) = 0x0;
    dw(o, 0x24) = 0x0;
    vtable_and_defaults(o, vt(0x5ffcb8));
}
static void ShieldHUDRenderData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ffcb8));
    pointer_array_dtor(o + 0x18, vt(0x5ffd24));
    vtable_and_free(o, vt(0x5ede94));
    obj_dtor(o);
}
// 0x56e690
static void *__cdecl ShieldHUDRenderData_Create() { return create_one<0x28, ShieldHUDRenderData_construct>(); }
// 0x56e6f0 (count in ecx)
static void *__fastcall ShieldHUDRenderData_CreateArray(int count, int, int) { return create_array<0x28, ShieldHUDRenderData_construct>(count); }
// 0x56e780 (slot 1)
static void *THISCALL ShieldHUDRenderData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, ShieldHUDRenderData_destroy>(self, flags); }
// 0x56e8e0
static DWORD __cdecl ShieldHUDRenderData_RegisterFields()
{
    void *element = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6219c4) /* "Shield Item" */, 0, 4, 0, EnergyHUDRenderData_ClassInfo(), 1, 0, 0, 0);
    void *f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x6219b8) /* "Sheild List" */, 0x20, 4, 0x20, 0x24, 4, 0x24, 0x1c, 4, 0x1c, 0, element, 1, 0);
    add_field(ShieldHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6219a8) /* "Old ListSize" */, 0x14, 4, 0x14, 1, 0, 0, 0, 0);
    return add_field(ShieldHUDRenderData_ClassInfo(), f);
}

// ---- CShieldInventoryDetailRenderer ----------------------------------------------------------------------

// 0x484850, 0x4849e0
static CClassInfo *__cdecl ShieldInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x6bdcc8, 0x612244 /* "CShieldInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x8, 0, 0x4848a0, 0x4848e0, 0x58ea40);
}
static void ShieldInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x5f19fc));
}
static void ShieldInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f19fc));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x4848a0
static void *__cdecl ShieldInventoryDetailRenderer_Create() { return create_one<0x8, ShieldInventoryDetailRenderer_construct>(); }
// 0x4848e0 (count in ecx)
static void *__fastcall ShieldInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x8, ShieldInventoryDetailRenderer_construct>(count); }
// 0x484940 (slot 1)
static void *THISCALL ShieldInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, ShieldInventoryDetailRenderer_destroy>(self, flags); }

// ---- CShieldInventoryHUDRenderer -------------------------------------------------------------------------

// 0x56ed30, 0x56eec0
static CClassInfo *__cdecl ShieldInventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x713528, 0x6219d0 /* "CShieldInventoryHUDRenderer" */, 1.0f, InventoryHUDRenderer_ClassInfo, 0x8, 0, 0x56ed80, 0x56edc0, 0x58ea40);
}
static void ShieldInventoryHUDRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x5ffd9c));
}
static void ShieldInventoryHUDRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ffd9c));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x56ed80
static void *__cdecl ShieldInventoryHUDRenderer_Create() { return create_one<0x8, ShieldInventoryHUDRenderer_construct>(); }
// 0x56edc0 (count in ecx)
static void *__fastcall ShieldInventoryHUDRenderer_CreateArray(int count, int, int) { return create_array<0x8, ShieldInventoryHUDRenderer_construct>(count); }
// 0x56ee20 (slot 1)
static void *THISCALL ShieldInventoryHUDRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, ShieldInventoryHUDRenderer_destroy>(self, flags); }

// ---- CSphereOfZinEnergyInventoryDetailRenderer -----------------------------------------------------------

// 0x582870, 0x582a10
static CClassInfo *__cdecl SphereOfZinEnergyInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x713804, 0x622888 /* "CSphereOfZinEnergyInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0x24, 0, 0x5828c0, 0x582900, 0x582a70);
}
static void SphereOfZinEnergyInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x6011d4));
}
static void SphereOfZinEnergyInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x6011d4));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5828c0
static void *__cdecl SphereOfZinEnergyInventoryDetailRenderer_Create() { return create_one<0x24, SphereOfZinEnergyInventoryDetailRenderer_construct>(); }
// 0x582900 (count in ecx)
static void *__fastcall SphereOfZinEnergyInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0x24, SphereOfZinEnergyInventoryDetailRenderer_construct>(count); }
// 0x582970 (slot 1)
static void *THISCALL SphereOfZinEnergyInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, SphereOfZinEnergyInventoryDetailRenderer_destroy>(self, flags); }
// 0x582a70
static DWORD __cdecl SphereOfZinEnergyInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x622970) /* "Detail Image" */, 8, 4, 8, glob_addr(0x6c0948), 1, 0, 0, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x622960) /* "Next Renderer" */, 0xc, 4, 0xc, class_at(fn_addr(0x5acc00)) /* CInventoryDetailRenderer */, 1, 0, 0, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622944) /* "Max Energy-Pulse Percentage" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0x10, 4, 0x10, 1, 0x3dcccccd /* 0.1 */, 0, 0, 0x3f800000 /* 1.0 */, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign(zstring(f, 0x20), glob_addr(0x60f5d8) /* "%" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622928) /* "Min Energy-Pulse Percentage" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0x14, 4, 0x14, 1, 0x3c75c28f /* 0.015 */, 0, 0, 0x3f800000 /* 1.0 */, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign(zstring(f, 0x20), glob_addr(0x60f5d8) /* "%" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622904) /* "Max Energy-Pulse Rate" */, glob_addr(0x62291c) /* "Pulses/Sec" */, 0x18, 4, 0x18, 1, 0x40000000 /* 2.0 */, 0, 0, 0x47c34ff3 /* 99999.9 */, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6228ec) /* "Min Energy-Pulse Rate" */, glob_addr(0x62291c) /* "Pulses/Sec" */, 0x1c, 4, 0x1c, 1, 0x3e800000 /* 0.25 */, 0, 0, 0x47c34ff3 /* 99999.9 */, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6228d4) /* "Flip HUD Sequence Order" */, 0x20, 1, 0x20, 1, 0, 0);
    add_field(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), f);
    dw(SphereOfZinEnergyInventoryDetailRenderer_ClassInfo(), 0x8) = glob_addr(0x6228b4) /* "CEnergyInventoryDetailRenderer" */;
    return *glob<DWORD *>(0x713804);
}

// ---- CSphereOfZinEnergyInventoryHUDRenderer --------------------------------------------------------------

// 0x583230, 0x583400
static CClassInfo *__cdecl SphereOfZinEnergyInventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x71380c, 0x622988 /* "CSphereOfZinEnergyInventoryHUDRenderer" */, 1.0f, EnergyInventoryHUDRenderer_ClassInfo, 0x24, 0, 0x583280, 0x5832d0, 0x583460);
}
static void SphereOfZinEnergyInventoryHUDRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x601080));
    vtable_and_defaults(o, vt(0x601240));
}
static void SphereOfZinEnergyInventoryHUDRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601240));
    vtable_and_free(o, vt(0x601080));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x583280
static void *__cdecl SphereOfZinEnergyInventoryHUDRenderer_Create() { return create_one<0x24, SphereOfZinEnergyInventoryHUDRenderer_construct>(); }
// 0x5832d0 (count in ecx)
static void *__fastcall SphereOfZinEnergyInventoryHUDRenderer_CreateArray(int count, int, int) { return create_array<0x24, SphereOfZinEnergyInventoryHUDRenderer_construct>(count); }
// 0x583340 (slot 1)
static void *THISCALL SphereOfZinEnergyInventoryHUDRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, SphereOfZinEnergyInventoryHUDRenderer_destroy>(self, flags); }
// 0x583460
static DWORD __cdecl SphereOfZinEnergyInventoryHUDRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60f5f8) /* "HUD Sequence" */, 8, 4, 8, glob_addr(0x6c06a0), 1, 0, 0, 0);
    add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622944) /* "Max Energy-Pulse Percentage" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0xc, 4, 0xc, 1, 0x3dcccccd /* 0.1 */, 0, 0, 0x3f800000 /* 1.0 */, 0);
    add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign(zstring(f, 0x20), glob_addr(0x60f5d8) /* "%" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622928) /* "Min Energy-Pulse Percentage" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0x10, 4, 0x10, 1, 0x3c75c28f /* 0.015 */, 0, 0, 0x3f800000 /* 1.0 */, 0);
    add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign(zstring(f, 0x20), glob_addr(0x60f5d8) /* "%" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x622904) /* "Max Energy-Pulse Rate" */, glob_addr(0x62291c) /* "Pulses/Sec" */, 0x14, 4, 0x14, 1, 0x40000000 /* 2.0 */, 0, 0, 0x47c34ff3 /* 99999.9 */, 0);
    add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6228ec) /* "Min Energy-Pulse Rate" */, glob_addr(0x62291c) /* "Pulses/Sec" */, 0x18, 4, 0x18, 1, 0x3e800000 /* 0.25 */, 0, 0, 0x47c34ff3 /* 99999.9 */, 0);
    add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6228d4) /* "Flip HUD Sequence Order" */, 0x1c, 1, 0x1c, 1, 0, 0);
    add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6229b0) /* "Overlay HUD Sequence" */, 0x20, 4, 0x20, glob_addr(0x6c06a0), 1, 0, 0, 0);
    return add_field(SphereOfZinEnergyInventoryHUDRenderer_ClassInfo(), f);
}

// ---- CWeaponHUDRenderData --------------------------------------------------------------------------------

// 0x484d40, 0x484ee0
static CClassInfo *__cdecl WeaponHUDRenderData_ClassInfo()
{
    return class_getter(0x6bdccc, 0x61227c /* "CWeaponHUDRenderData" */, 1.0f, InventoryHUDRenderData_ClassInfo, 0x28, 0, 0x484d90, 0x484dd0, 0x484f40);
}
static void WeaponHUDRenderData_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ede94));
    vtable_and_defaults(o, vt(0x5f1a64));
}
static void WeaponHUDRenderData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1a64));
    vtable_and_free(o, vt(0x5ede94));
    obj_dtor(o);
}
// 0x484d90
static void *__cdecl WeaponHUDRenderData_Create() { return create_one<0x28, WeaponHUDRenderData_construct>(); }
// 0x484dd0 (count in ecx)
static void *__fastcall WeaponHUDRenderData_CreateArray(int count, int, int) { return create_array<0x28, WeaponHUDRenderData_construct>(count); }
// 0x484e40 (slot 1)
static void *THISCALL WeaponHUDRenderData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, WeaponHUDRenderData_destroy>(self, flags); }
// 0x484f40
static DWORD __cdecl WeaponHUDRenderData_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x60c134) /* "Rounds" */, 0x14, 4, 0x14, 1, 0, 0, 0, 0);
    add_field(WeaponHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x6122c8) /* "Continuous Fire Rounds" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0, 0);
    add_field(WeaponHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x6122b8) /* "Batt Capacity" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0, 0);
    add_field(WeaponHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x6122ac) /* "Batt Charge" */, 0x20, 4, 0x20, 1, 0, 0, 0, 0, 0);
    add_field(WeaponHUDRenderData_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x612294) /* "WeaponChanged Status" */, 0x24, 4, 0x24, 1, 7, 0, 0, 0);
    return add_field(WeaponHUDRenderData_ClassInfo(), f);
}

// ---- CWeaponInventoryDetailRenderer ----------------------------------------------------------------------

// 0x5acc60
static CClassInfo *__cdecl WeaponInventoryDetailRenderer_ClassInfo()
{
    return class_getter(0x7140dc, 0x6260b0 /* "CWeaponInventoryDetailRenderer" */, 1.0f, InventoryDetailRenderer_ClassInfo, 0xc, 0, 0x5acd00, 0x5acd40, 0x5ace60);
}
static void WeaponInventoryDetailRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));
    vtable_and_defaults(o, vt(0x603fe8));
}
static void WeaponInventoryDetailRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x603fe8));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5acd00
static void *__cdecl WeaponInventoryDetailRenderer_Create() { return create_one<0xc, WeaponInventoryDetailRenderer_construct>(); }
// 0x5acd40 (count in ecx)
static void *__fastcall WeaponInventoryDetailRenderer_CreateArray(int count, int, int) { return create_array<0xc, WeaponInventoryDetailRenderer_construct>(count); }
// 0x5acdb0 (slot 1)
static void *THISCALL WeaponInventoryDetailRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, WeaponInventoryDetailRenderer_destroy>(self, flags); }
// 0x5ace60
static DWORD __cdecl WeaponInventoryDetailRenderer_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6260d0) /* "Warn Getting Low Threshold" */, 8, 4, 8, 1, 5, 0, 0, 0);
    return add_field(WeaponInventoryDetailRenderer_ClassInfo(), f);
}

// ---- CWeaponInventoryHUDRenderer -------------------------------------------------------------------------

// 0x485720, 0x4858c0
static CClassInfo *__cdecl WeaponInventoryHUDRenderer_ClassInfo()
{
    return class_getter(0x6bdcd0, 0x6122e0 /* "CWeaponInventoryHUDRenderer" */, 2.0f, InventoryHUDRenderer_ClassInfo, 0xc, 0, 0x485770, 0x4857b0, 0x485920);
}
static void WeaponInventoryHUDRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5edf7c));
    vtable_and_defaults(o, vt(0x5f1ad0));
}
static void WeaponInventoryHUDRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1ad0));
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}
// 0x485770
static void *__cdecl WeaponInventoryHUDRenderer_Create() { return create_one<0xc, WeaponInventoryHUDRenderer_construct>(); }
// 0x4857b0 (count in ecx)
static void *__fastcall WeaponInventoryHUDRenderer_CreateArray(int count, int, int) { return create_array<0xc, WeaponInventoryHUDRenderer_construct>(count); }
// 0x485820 (slot 1)
static void *THISCALL WeaponInventoryHUDRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, WeaponInventoryHUDRenderer_destroy>(self, flags); }
// 0x485920
static DWORD __cdecl WeaponInventoryHUDRenderer_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6122fc) /* "Show Ammo Icon" */, 8, 1, 8, 1, 0, 0);
    return add_field(WeaponInventoryHUDRenderer_ClassInfo(), f);
}

ZAX_HOOKS(hud_classes)
{
    const Hook list[] = {
        {0x5b1500, (void *)&AmmunitionInventoryDetailRenderer_ClassInfo, "CAmmunitionInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0x50, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b1690, (void *)&AmmunitionInventoryDetailRenderer_ClassInfo, "CAmmunitionInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x50, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b1550, (void *)&AmmunitionInventoryDetailRenderer_Create, "CAmmunitionInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x5b1590, (void *)&AmmunitionInventoryDetailRenderer_CreateArray, "CAmmunitionInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x5b15f0, (void *)&AmmunitionInventoryDetailRenderer_DeletingDtor, "CAmmunitionInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5adb50, (void *)&AnimationOverlayInventoryDetailRenderer_ClassInfo, "CAnimationOverlayInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xe0, 0x40, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5ada10, (void *)&AnimationOverlayInventoryDetailRenderer_Create, "CAnimationOverlayInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x5ada50, (void *)&AnimationOverlayInventoryDetailRenderer_CreateArray, "CAnimationOverlayInventoryDetailRenderer::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x5adab0, (void *)&AnimationOverlayInventoryDetailRenderer_DeletingDtor, "CAnimationOverlayInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x5adc00, (void *)&AnimationOverlayInventoryDetailRenderer_RegisterFields, "CAnimationOverlayInventoryDetailRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0x31, 0x27, 0x02}, 5},
        {0x4839a0, (void *)&BatteryInventoryDetailRenderer_ClassInfo, "CBatteryInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xb8, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x483b30, (void *)&BatteryInventoryDetailRenderer_ClassInfo, "CBatteryInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0xb8, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4839f0, (void *)&BatteryInventoryDetailRenderer_Create, "CBatteryInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x483a30, (void *)&BatteryInventoryDetailRenderer_CreateArray, "CBatteryInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x483a90, (void *)&BatteryInventoryDetailRenderer_DeletingDtor, "CBatteryInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x580c00, (void *)&CharacterInventoryDetailRenderer_ClassInfo, "CCharacterInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xbc, 0x37, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x580d90, (void *)&CharacterInventoryDetailRenderer_ClassInfo, "CCharacterInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0xbc, 0x37, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x580c50, (void *)&CharacterInventoryDetailRenderer_Create, "CCharacterInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x580c90, (void *)&CharacterInventoryDetailRenderer_CreateArray, "CCharacterInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x580cf0, (void *)&CharacterInventoryDetailRenderer_DeletingDtor, "CCharacterInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x581ea0, (void *)&CharacterInventoryHUDRenderer_ClassInfo, "CCharacterInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0xcc, 0x37, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x5820d0, (void *)&CharacterInventoryHUDRenderer_ClassInfo, "CCharacterInventoryHUDRenderer::ClassInfo2", 5, {0xa1, 0xcc, 0x37, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x581f50, (void *)&CharacterInventoryHUDRenderer_Create, "CCharacterInventoryHUDRenderer::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x581fa0, (void *)&CharacterInventoryHUDRenderer_CreateArray, "CCharacterInventoryHUDRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x582010, (void *)&CharacterInventoryHUDRenderer_DeletingDtor, "CCharacterInventoryHUDRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x582190, (void *)&CharacterInventoryHUDRenderer_RegisterFields, "CCharacterInventoryHUDRenderer::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x38, 0xe8, 0xa1, 0xe1, 0x04}, 5},
        {0x5b1f70, (void *)&ChargerInventoryDetailRenderer_ClassInfo, "CChargerInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0x58, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b2100, (void *)&ChargerInventoryDetailRenderer_ClassInfo, "CChargerInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x58, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b1fc0, (void *)&ChargerInventoryDetailRenderer_Create, "CChargerInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x5b2000, (void *)&ChargerInventoryDetailRenderer_CreateArray, "CChargerInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x5b2060, (void *)&ChargerInventoryDetailRenderer_DeletingDtor, "CChargerInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x423110, (void *)&CompassRenderData_ClassInfo, "CCompassRenderData::ClassInfo", 5, {0xa1, 0xe4, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x4232b0, (void *)&CompassRenderData_ClassInfo, "CCompassRenderData::ClassInfo2", 5, {0xa1, 0xe4, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x423160, (void *)&CompassRenderData_Create, "CCompassRenderData::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4231a0, (void *)&CompassRenderData_CreateArray, "CCompassRenderData::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x423210, (void *)&CompassRenderData_DeletingDtor, "CCompassRenderData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x423310, (void *)&CompassRenderData_RegisterFields, "CCompassRenderData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x21, 0xd0, 0x1a}, 5},
        {0x423670, (void *)&CompassRenderer_ClassInfo, "CCompassRenderer::ClassInfo", 5, {0xa1, 0xe8, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x423860, (void *)&CompassRenderer_ClassInfo, "CCompassRenderer::ClassInfo2", 5, {0xa1, 0xe8, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x423710, (void *)&CompassRenderer_Create, "CCompassRenderer::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x423750, (void *)&CompassRenderer_CreateArray, "CCompassRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4237c0, (void *)&CompassRenderer_DeletingDtor, "CCompassRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4238c0, (void *)&CompassRenderer_RegisterFields, "CCompassRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0x71, 0xca, 0x1a}, 5},
        {0x5b0f10, (void *)&CountInventoryDetailRenderer_ClassInfo, "CCountInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0x4c, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b1100, (void *)&CountInventoryDetailRenderer_ClassInfo, "CCountInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x4c, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b0f60, (void *)&CountInventoryDetailRenderer_Create, "CCountInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x5b0fb0, (void *)&CountInventoryDetailRenderer_CreateArray, "CCountInventoryDetailRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5b1030, (void *)&CountInventoryDetailRenderer_DeletingDtor, "CCountInventoryDetailRenderer::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x5b1160, (void *)&CountInventoryDetailRenderer_RegisterFields, "CCountInventoryDetailRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xd1, 0xf1, 0x01}, 5},
        {0x47abf0, (void *)&DeployableInventoryDetailRenderer_ClassInfo, "CDeployableInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xa8, 0xda, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x47ae20, (void *)&DeployableInventoryDetailRenderer_ClassInfo, "CDeployableInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0xa8, 0xda, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x47ac80, (void *)&DeployableInventoryDetailRenderer_Create, "CDeployableInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x47acd0, (void *)&DeployableInventoryDetailRenderer_CreateArray, "CDeployableInventoryDetailRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x47ad50, (void *)&DeployableInventoryDetailRenderer_DeletingDtor, "CDeployableInventoryDetailRenderer::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x47ae80, (void *)&DeployableInventoryDetailRenderer_RegisterFields, "CDeployableInventoryDetailRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xb1, 0x54, 0x15}, 5},
        {0x581380, (void *)&EnergyHUDRenderData_ClassInfo, "CEnergyHUDRenderData::ClassInfo", 5, {0xa1, 0xc4, 0x37, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x581510, (void *)&EnergyHUDRenderData_ClassInfo, "CEnergyHUDRenderData::ClassInfo2", 5, {0xa1, 0xc4, 0x37, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5813d0, (void *)&EnergyHUDRenderData_Create, "CEnergyHUDRenderData::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x581410, (void *)&EnergyHUDRenderData_CreateArray, "CEnergyHUDRenderData::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x581470, (void *)&EnergyHUDRenderData_DeletingDtor, "CEnergyHUDRenderData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x581570, (void *)&EnergyHUDRenderData_RegisterFields, "CEnergyHUDRenderData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xc1, 0xed, 0x04}, 5},
        {0x5818a0, (void *)&EnergyInventoryHUDRenderer_ClassInfo, "CEnergyInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0xc8, 0x37, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x581a30, (void *)&EnergyInventoryHUDRenderer_ClassInfo, "CEnergyInventoryHUDRenderer::ClassInfo2", 5, {0xa1, 0xc8, 0x37, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5818f0, (void *)&EnergyInventoryHUDRenderer_Create, "CEnergyInventoryHUDRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x581930, (void *)&EnergyInventoryHUDRenderer_CreateArray, "CEnergyInventoryHUDRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x581990, (void *)&EnergyInventoryHUDRenderer_DeletingDtor, "CEnergyInventoryHUDRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x483f50, (void *)&EnergyTextInventoryDetailRenderer_ClassInfo, "CEnergyTextInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xc4, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x484160, (void *)&EnergyTextInventoryDetailRenderer_ClassInfo, "CEnergyTextInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0xc4, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x483fa0, (void *)&EnergyTextInventoryDetailRenderer_Create, "CEnergyTextInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x483ff0, (void *)&EnergyTextInventoryDetailRenderer_CreateArray, "CEnergyTextInventoryDetailRenderer::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x484070, (void *)&EnergyTextInventoryDetailRenderer_DeletingDtor, "CEnergyTextInventoryDetailRenderer::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0xf9}},
        {0x4841c0, (void *)&EnergyTextInventoryDetailRenderer_RegisterFields, "CEnergyTextInventoryDetailRenderer::RegisterFields", 7, {0x6a, 0x34, 0xe8, 0x73, 0xc1, 0x14, 0x00, 0x83}, 3},
        {0x469b70, (void *)&FlagInventoryHUDRenderer_ClassInfo, "CFlagInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0x30, 0xd1, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x469d00, (void *)&FlagInventoryHUDRenderer_ClassInfo, "CFlagInventoryHUDRenderer::ClassInfo2", 5, {0xa1, 0x30, 0xd1, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x469bc0, (void *)&FlagInventoryHUDRenderer_Create, "CFlagInventoryHUDRenderer::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x469c00, (void *)&FlagInventoryHUDRenderer_CreateArray, "CFlagInventoryHUDRenderer::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x469c60, (void *)&FlagInventoryHUDRenderer_DeletingDtor, "CFlagInventoryHUDRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x469d60, (void *)&FlagInventoryHUDRenderer_RegisterFields, "CFlagInventoryHUDRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0xd1, 0x65, 0x16}, 5},
        {0x5b2390, (void *)&HealthInventoryDetailRenderer_ClassInfo, "CHealthInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0x5c, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b2520, (void *)&HealthInventoryDetailRenderer_ClassInfo, "CHealthInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x5c, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b23e0, (void *)&HealthInventoryDetailRenderer_Create, "CHealthInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x5b2420, (void *)&HealthInventoryDetailRenderer_CreateArray, "CHealthInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x5b2480, (void *)&HealthInventoryDetailRenderer_DeletingDtor, "CHealthInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x507500, (void *)&HudBarOptions_ClassInfo, "CHudBarOptions::ClassInfo", 5, {0xa1, 0xc0, 0x2d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x5073b0, (void *)&HudBarOptions_Create, "CHudBarOptions::Create", 6, {0x56, 0xb9, 0x60, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x507400, (void *)&HudBarOptions_CreateArray, "CHudBarOptions::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0xc1, 0xe0}},
        {0x507480, (void *)&HudBarOptions_DeletingDtor, "CHudBarOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x507560, (void *)&HudBarOptions_RegisterFields, "CHudBarOptions::RegisterFields", 5, {0xa0, 0x7c, 0x2d, 0x6c, 0x00, 0xa8, 0x01, 0x75}},
        {0x5acc00, (void *)&InventoryDetailRenderer_ClassInfo, "CInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xd8, 0x40, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x5ac6b0, (void *)&InventoryHUDRenderData_ClassInfo, "CInventoryHUDRenderData::ClassInfo", 5, {0xa1, 0xd0, 0x40, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x5ac5a0, (void *)&InventoryHUDRenderData_Create, "CInventoryHUDRenderData::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x5ac5d0, (void *)&InventoryHUDRenderData_CreateArray, "CInventoryHUDRenderData::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5ac630, (void *)&InventoryHUDRenderData_DeletingDtor, "CInventoryHUDRenderData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5ac710, (void *)&InventoryHUDRenderData_RegisterFields, "CInventoryHUDRenderData::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x22, 0x3c, 0x02, 0x00}, 4},
        {0x5aca50, (void *)&InventoryHUDRenderer_ClassInfo, "CInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0xd4, 0x40, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x5b1a70, (void *)&MineralInventoryDetailRenderer_ClassInfo, "CMineralInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0x54, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b1c00, (void *)&MineralInventoryDetailRenderer_ClassInfo, "CMineralInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x54, 0x43, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5b1ac0, (void *)&MineralInventoryDetailRenderer_Create, "CMineralInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x5b1b00, (void *)&MineralInventoryDetailRenderer_CreateArray, "CMineralInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x5b1b60, (void *)&MineralInventoryDetailRenderer_DeletingDtor, "CMineralInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5ae1e0, (void *)&RandomOverlayInventoryDetailRenderer_ClassInfo, "CRandomOverlayInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xe4, 0x40, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5ae080, (void *)&RandomOverlayInventoryDetailRenderer_Create, "CRandomOverlayInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x5ae0d0, (void *)&RandomOverlayInventoryDetailRenderer_CreateArray, "CRandomOverlayInventoryDetailRenderer::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x5ae140, (void *)&RandomOverlayInventoryDetailRenderer_DeletingDtor, "CRandomOverlayInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5ae290, (void *)&RandomOverlayInventoryDetailRenderer_RegisterFields, "CRandomOverlayInventoryDetailRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0xa1, 0x20, 0x02}, 5},
        {0x56e640, (void *)&ShieldHUDRenderData_ClassInfo, "CShieldHUDRenderData::ClassInfo", 5, {0xa1, 0x24, 0x35, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x56e880, (void *)&ShieldHUDRenderData_ClassInfo, "CShieldHUDRenderData::ClassInfo2", 5, {0xa1, 0x24, 0x35, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x56e690, (void *)&ShieldHUDRenderData_Create, "CShieldHUDRenderData::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x56e6f0, (void *)&ShieldHUDRenderData_CreateArray, "CShieldHUDRenderData::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf}},
        {0x56e780, (void *)&ShieldHUDRenderData_DeletingDtor, "CShieldHUDRenderData::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x56e8e0, (void *)&ShieldHUDRenderData_RegisterFields, "CShieldHUDRenderData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x51, 0x1a, 0x06}, 5},
        {0x484850, (void *)&ShieldInventoryDetailRenderer_ClassInfo, "CShieldInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xc8, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4849e0, (void *)&ShieldInventoryDetailRenderer_ClassInfo, "CShieldInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0xc8, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4848a0, (void *)&ShieldInventoryDetailRenderer_Create, "CShieldInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4848e0, (void *)&ShieldInventoryDetailRenderer_CreateArray, "CShieldInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x484940, (void *)&ShieldInventoryDetailRenderer_DeletingDtor, "CShieldInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x56ed30, (void *)&ShieldInventoryHUDRenderer_ClassInfo, "CShieldInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0x28, 0x35, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x56eec0, (void *)&ShieldInventoryHUDRenderer_ClassInfo, "CShieldInventoryHUDRenderer::ClassInfo2", 5, {0xa1, 0x28, 0x35, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x56ed80, (void *)&ShieldInventoryHUDRenderer_Create, "CShieldInventoryHUDRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x56edc0, (void *)&ShieldInventoryHUDRenderer_CreateArray, "CShieldInventoryHUDRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x56ee20, (void *)&ShieldInventoryHUDRenderer_DeletingDtor, "CShieldInventoryHUDRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x582870, (void *)&SphereOfZinEnergyInventoryDetailRenderer_ClassInfo, "CSphereOfZinEnergyInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0x04, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x582a10, (void *)&SphereOfZinEnergyInventoryDetailRenderer_ClassInfo, "CSphereOfZinEnergyInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x04, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5828c0, (void *)&SphereOfZinEnergyInventoryDetailRenderer_Create, "CSphereOfZinEnergyInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x582900, (void *)&SphereOfZinEnergyInventoryDetailRenderer_CreateArray, "CSphereOfZinEnergyInventoryDetailRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x582970, (void *)&SphereOfZinEnergyInventoryDetailRenderer_DeletingDtor, "CSphereOfZinEnergyInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x582a70, (void *)&SphereOfZinEnergyInventoryDetailRenderer_RegisterFields, "CSphereOfZinEnergyInventoryDetailRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0xc1, 0xd8, 0x04}, 5},
        {0x583230, (void *)&SphereOfZinEnergyInventoryHUDRenderer_ClassInfo, "CSphereOfZinEnergyInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0x0c, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x583400, (void *)&SphereOfZinEnergyInventoryHUDRenderer_ClassInfo, "CSphereOfZinEnergyInventoryHUDRenderer::ClassInfo2", 5, {0xa1, 0x0c, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x583280, (void *)&SphereOfZinEnergyInventoryHUDRenderer_Create, "CSphereOfZinEnergyInventoryHUDRenderer::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x5832d0, (void *)&SphereOfZinEnergyInventoryHUDRenderer_CreateArray, "CSphereOfZinEnergyInventoryHUDRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x583340, (void *)&SphereOfZinEnergyInventoryHUDRenderer_DeletingDtor, "CSphereOfZinEnergyInventoryHUDRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x583460, (void *)&SphereOfZinEnergyInventoryHUDRenderer_RegisterFields, "CSphereOfZinEnergyInventoryHUDRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0xd1, 0xce, 0x04}, 5},
        {0x484d40, (void *)&WeaponHUDRenderData_ClassInfo, "CWeaponHUDRenderData::ClassInfo", 5, {0xa1, 0xcc, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x484ee0, (void *)&WeaponHUDRenderData_ClassInfo, "CWeaponHUDRenderData::ClassInfo2", 5, {0xa1, 0xcc, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x484d90, (void *)&WeaponHUDRenderData_Create, "CWeaponHUDRenderData::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x484dd0, (void *)&WeaponHUDRenderData_CreateArray, "CWeaponHUDRenderData::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x484e40, (void *)&WeaponHUDRenderData_DeletingDtor, "CWeaponHUDRenderData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x484f40, (void *)&WeaponHUDRenderData_RegisterFields, "CWeaponHUDRenderData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xf1, 0xb3, 0x14}, 5},
        {0x5acc60, (void *)&WeaponInventoryDetailRenderer_ClassInfo, "CWeaponInventoryDetailRenderer::ClassInfo", 5, {0xa1, 0xdc, 0x40, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5acd00, (void *)&WeaponInventoryDetailRenderer_Create, "CWeaponInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x5acd40, (void *)&WeaponInventoryDetailRenderer_CreateArray, "CWeaponInventoryDetailRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x5acdb0, (void *)&WeaponInventoryDetailRenderer_DeletingDtor, "CWeaponInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5ace60, (void *)&WeaponInventoryDetailRenderer_RegisterFields, "CWeaponInventoryDetailRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xd1, 0x34, 0x02}, 5},
        {0x485720, (void *)&WeaponInventoryHUDRenderer_ClassInfo, "CWeaponInventoryHUDRenderer::ClassInfo", 5, {0xa1, 0xd0, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4858c0, (void *)&WeaponInventoryHUDRenderer_ClassInfo, "CWeaponInventoryHUDRenderer::ClassInfo2", 5, {0xa1, 0xd0, 0xdc, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x485770, (void *)&WeaponInventoryHUDRenderer_Create, "CWeaponInventoryHUDRenderer::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x4857b0, (void *)&WeaponInventoryHUDRenderer_CreateArray, "CWeaponInventoryHUDRenderer::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x485820, (void *)&WeaponInventoryHUDRenderer_DeletingDtor, "CWeaponInventoryHUDRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x485920, (void *)&WeaponInventoryHUDRenderer_RegisterFields, "CWeaponInventoryHUDRenderer::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x11, 0xaa, 0x14}, 5},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
