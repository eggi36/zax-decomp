// The pickup AIs and the FieldColor helpers reimplemented (see ai_pickup.h).
//
// Floating point follows the original instruction by instruction (x87.h): long double where it keeps a
// value on the FPU stack, a float or double wherever it stores one. Strings and other constants are
// the exe's, by address.
#include "ai_pickup.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe these slots call; through their exe addresses ----------------------------------

constexpr DWORD CATALOGUE = 0x6c0c08;   // the item catalogue (a definition list: records 0x6c0c38, count 0x6c0c3c, folder 0x6c0c40)
CClassInfo *flyweight_ai_class() { return fn<Getter>(0x4fd390)(); }   // CFlyweightLayerPartAI
CClassInfo *inventory_item_class() { return fn<Getter>(0x429e50)(); }
CClassInfo *character_class() { return fn<Getter>(0x49a930)(); }        // CEntityCharacter
CClassInfo *entity_base_class() { return fn<Getter>(0x487620)(); }
CClassInfo *item_definition_class() { return fn<Getter>(0x5b5f10)(); }  // CZaxInventoryItemDefinition
BYTE *find_ai(BYTE *part, CClassInfo *c) { return vcall<BYTE *>(part, 64, c, 0); }
void obj_reset_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x417230)(o); }
BYTE *obj_clone(BYTE *o) { return fn<BYTE *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
void inv_item_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x42a2b0)(o); }
DWORD inv_item_dtor(BYTE *o) { return fn<DWORD(THISCALL *)(void *)>(0x42a300)(o); }
// 0x54fdb0: the item's definition index set
void item_set_definition(BYTE *item, DWORD def) { fn<void(THISCALL *)(void *, DWORD)>(0x54fdb0)(item, def); }
// 0x4dd480: the item's definition (catalogue record +8), loaded on first use
BYTE *item_definition(BYTE *item) { return fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item); }
// 0x482de0: ecx's +8 (here the item's definition index)
DWORD dword8(BYTE *o) { return fn<DWORD(THISCALL *)(void *)>(0x482de0)(o); }
// 0x48d8f0: the object of a catalogue record
BYTE *catalogue_object(DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(CATALOGUE), i); }
// 0x5b7aa0: an item definition's default item
BYTE *definition_default_item(BYTE *def) { return fn<BYTE *(THISCALL *)(void *)>(0x5b7aa0)(def); }
BYTE *part_inventory(BYTE *e) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(e); }
BYTE *entity_model(BYTE *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
void part_layer_add(BYTE *part, DWORD what) { fn<void(THISCALL *)(void *, DWORD)>(0x4fc8a0)(part, what); }
void part_remove_ai(BYTE *part, BYTE *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbc90)(part, ai); }
void part_move_center(BYTE *part, DWORD x, DWORD y) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4fb680)(part, x, y); }
long double polygon_size(BYTE *poly) { return fn<long double(THISCALL *)(void *)>(0x546a90)(poly); }
void *world_current_layer() { return fn<void *(THISCALL *)(void *)>(0x4f1050)(world()); }
void layer_add_part(void *layer, BYTE *part) { fn<void(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, part); }
void world_explosion_at(void *layer, DWORD effect, BYTE *part)
{
    fn<void(THISCALL *)(void *, void *, DWORD, void *, DWORD, DWORD)>(0x4f60e0)(world(), layer, effect, part, 1, 0);
}
// 0x5babc0 (fastcall entity, 0) and the game object's 0x5a3480 (definition, 2, that): the HUD's picked-up
// item display (zax_net.cpp, zax_rules.cpp)
DWORD entity_player(BYTE *e) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x5babc0)(e, 0); }
void app_show_item(DWORD def, DWORD kind, DWORD player) { fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x5a3480)(*glob<void **>(0x713f14), def, kind, player); }
long double now() { return vcall<long double>(reporter(), 13); }

constexpr DWORD COLLIDEABLE = 0x40000, VISIBLE = 0x20000;

}  // namespace

// ---- COverridePickupAI ----------------------------------------------------------------------------------

constexpr DWORD VT_OVERRIDE_PICKUP = 0x5f83b4;

// 0x53e850 (and 0x53e9f0, slot 2)
static CClassInfo *__cdecl OverridePickup_ClassInfo()
{
    return class_getter(0x6d0ba0, 0x61e4e4 /* "COverridePickupAI" */, 1.0f, layer_part_ai_class, 0x38, 0, 0x53e8a0, 0x53e8e0, 0x53ea50);
}
static void OverridePickup_construct(BYTE *o)
{
    layer_part_ai_ctor(o);
    vtable_and_defaults(o, vt(VT_OVERRIDE_PICKUP));
}
static void OverridePickup_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_OVERRIDE_PICKUP));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x53e8a0
static void *__cdecl OverridePickup_Create() { return create_one<0x38, OverridePickup_construct>(); }
// 0x53e8e0
static void *__fastcall OverridePickup_CreateArray(int count, int, int) { return create_array<0x38, OverridePickup_construct>(count); }
// 0x53e950 (slot 1)
static void *THISCALL OverridePickup_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x38, OverridePickup_destroy>(self, flags); }
// 0x53ea50. eax: the last AddField's.
static DWORD __cdecl OverridePickup_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60af18) /* "Item" */, 0x10, 4, 0x10, inventory_item_class(), 1, 0, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61e4b4) /* "Alternate Item Def" */, 0x14, 4, 0x14, glob_addr(CATALOGUE), 1, glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    set_help(last_field(OverridePickup_ClassInfo()), 0x61e440 /* "The item the powerup should try to give the character if it can't pick up..." */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61e720) /* "Disable CPickupAI's Alternate Item" */, 0x18, 1, 0x18, 1, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    set_help(last_field(OverridePickup_ClassInfo()), 0x61e690 /* "This only has an effect if the \"Alternate Item Def\" is \"!None\" and ..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61e680) /* "Respawn Mode" */, 0x1c, 4, 0x1c, glob_addr(0x6d0b80), 1, glob_addr(0x61e2ec) /* "Don't Respawn" */, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e670) /* "Respawn Delay" */, 0x20, 4, 0x20, 1, 0x41700000 /* 15.0 */, 0, 0x3c23d70a /* 0.01 */,
                  0x47a8c000 /* 86400.0 */, 0);
    add_field(OverridePickup_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e65c) /* "Min Respawn Delay" */, 0x24, 4, 0x24, 1, 0, 0, 0, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e640) /* "Debug/TimeUntilNextRespawn" */, 0x28, 4, 0x28, 3, 0, 0, 0, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e62c) /* "Debug/Time Offset" */, 0x2c, 4, 0x2c, 3, 0, 0, 0, 0, 0);
    add_field(OverridePickup_ClassInfo(), f);
    f = new_field(0x54, DOUBLE_FIELD_CTOR, glob_addr(0x61e610) /* "Debug/Last Simulation Time" */, 0x30, 8, 0x30, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return add_field(OverridePickup_ClassInfo(), f);
}
// 0x53efe0 (item; called by the inventory item actions 0x4dc6fe/0x4dc7f9): "Item" set; a selected item
// (+0x10) is told through its slot 45 with 0 (eax: the item or that slot's)
static DWORD THISCALL OverridePickup_SetItem(BYTE *self, BYTE *item)
{
    dw(self, 0x10) = (DWORD)(uintptr_t)item;
    if (fl<BYTE>(item, 0x10))
        return vcall<DWORD>(item, 45, 0);
    return (DWORD)(uintptr_t)item;
}

// ---- CPickupAI ------------------------------------------------------------------------------------------

constexpr DWORD VT_PICKUP = 0x5fcb88, VT_FLYWEIGHT_AI = 0x5ef1a0;

// 0x53d190 (and 0x53d3b0, slot 2)
static CClassInfo *__cdecl Pickup_ClassInfo()
{
    return class_getter(0x6d0b9c, 0x61e2fc /* "CPickupAI" */, 11.0f, flyweight_ai_class, 0x30, 0, 0x53d1e0, 0x53d240, 0x53d410);
}
static void Pickup_construct(BYTE *o)
{
    layer_part_ai_ctor(o);
    vtable_and_defaults(o, vt(VT_FLYWEIGHT_AI));
    vtable_and_defaults(o, vt(VT_PICKUP));
    dw(o, 0x14) = 0;                  // "Normal Item"
    dw(o, 0x10) = 0;                  // "Special Item"
    dw(o, 0x24) = *glob<DWORD *>(0x6c29a0);   // "Who Can Pick Up"
}
// 0x53d8f0: the constructor (5 callers, e.g. CAutoGunAI's for its +0x30). eax: this.
static BYTE *THISCALL Pickup_Ctor(BYTE *o)
{
    Pickup_construct(o);
    return o;
}
// 0x53d940: the destructor ("Special Item" deleted through its slot 1). eax: AIBase_Dtor's.
static void THISCALL Pickup_Dtor(BYTE *o)
{
    void *special = bp(dw(o, 0x10));
    dw(o, 0) = vt(VT_PICKUP);
    if (special) {
        vcall<void *>(special, 1, 1);
        dw(o, 0x10) = 0;
    }
    obj_free_fields(o);
    vtable_and_free(o, vt(VT_FLYWEIGHT_AI));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
static void Pickup_destroy(BYTE *o) { Pickup_Dtor(o); }
// 0x53d1e0
static void *__cdecl Pickup_Create() { return create_one<0x30, Pickup_construct>(); }
// 0x53d240
static void *__fastcall Pickup_CreateArray(int count, int, int) { return create_array<0x30, Pickup_construct>(count); }
// 0x53d2d0 (slot 1)
static void *THISCALL Pickup_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, Pickup_destroy>(self, flags); }
// 0x53d410. eax: the last SetHelp's.
static DWORD __cdecl Pickup_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61e4d8) /* "Normal Item" */, 0x14, 4, 0x14, glob_addr(CATALOGUE), 1, glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    add_field(Pickup_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61e4c8) /* "Special Item" */, 0x10, 4, 0x10, inventory_item_class(), 1, 0, 0, 0);
    add_field(Pickup_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61e4b4) /* "Alternate Item Def" */, 0x18, 4, 0x18, glob_addr(CATALOGUE), 1, glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    add_field(Pickup_ClassInfo(), f);
    set_help(last_field(Pickup_ClassInfo()), 0x61e440 /* "The item the powerup should try to give the character if it can't pick up..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61e430) /* "Pickup Effect" */, 0x20, 4, 0x20, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(Pickup_ClassInfo(), f);
    f = new_field(0x38, FLAGS_FIELD_CTOR, glob_addr(0x61e418) /* "Pickup/Who Can Pick Up" */, 0x24, 4, 0x24, glob_addr(0x6bd140), 1, glob_addr(0x61807c) /* "Player" */, 0);
    add_field(Pickup_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61e404) /* "Picked Up Action" */, 0x28, 4, 0x28, action_class(), 1, 0, 0, 0);
    add_field(Pickup_ClassInfo(), f);
    set_help(last_field(Pickup_ClassInfo()), 0x61e3d4 /* "Action to execute after the item is picked up" */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61e3c8) /* "Show in HUD" */, 0x2c, 1, 0x2c, 1, 1, 0);
    add_field(Pickup_ClassInfo(), f);
    return set_help(last_field(Pickup_ClassInfo()), 0x61e308 /* "When this box is checked, an image of the item will be shown in the HUD..." */);
}

// The alternate item definition of the pickup on a part: a COverridePickupAI's "Alternate Item Def" when
// it has one or disables CPickupAI's, otherwise "Alternate Item Def"
static DWORD alternate_definition(BYTE *self, BYTE *part)
{
    if (part)
        if (BYTE *ov = find_ai(part, OverridePickup_ClassInfo())) {
            if (dw(ov, 0x14) || fl<BYTE>(ov, 0x18))
                return dw(ov, 0x14);
        }
    return dw(self, 0x18);
}
// The alternate item: the definition's template item (+0x1c) when it is a CInventoryItem, otherwise the
// local item reset and given the definition; 0 without an alternate definition
static BYTE *alternate_item(BYTE *self, BYTE *part, BYTE *local)
{
    if (!alternate_definition(self, part))
        return nullptr;
    BYTE *tmpl = bp(dw(catalogue_object(alternate_definition(self, part)), 0x1c));
    if (tmpl && is_kind_of(tmpl, inventory_item_class()))
        return tmpl;
    obj_reset_fields(local);
    item_set_definition(local, alternate_definition(self, part));
    return local;
}
// The item made ready for a character: the definition's slot 25 (entity, its inventory, the template
// item or 0, the pickup's part)
static void prepare_item(BYTE *item, BYTE *e, BYTE *part)
{
    BYTE *def = item_definition(item);
    BYTE *tmpl = bp(dw(def, 0x1c));
    if (!(tmpl && is_kind_of(tmpl, inventory_item_class())))
        tmpl = nullptr;
    BYTE *inv = part_inventory(e);
    vcall<DWORD>(def, 25, e, inv, tmpl, part);
}
// An entity of a category in "Who Can Pick Up" that is a CEntityCharacter
static bool may_pick_up(BYTE *self, BYTE *e) { return (dw(e, 0x1c) & dw(self, 0x24)) && is_kind_of(e, character_class()); }

// 0x53e4c0 (part): the item the pickup gives: a COverridePickupAI's "Item", else the default item of the
// "Normal Item" definition (loaded on first use as 0x48d8f0 does, inlined), else "Special Item"
static BYTE *THISCALL Pickup_Item(BYTE *self, BYTE *part)
{
    if (part)
        if (BYTE *ov = find_ai(part, OverridePickup_ClassInfo()))
            if (dw(ov, 0x10))
                return bp(dw(ov, 0x10));
    int none = vcall<int>(glob<void *>(CATALOGUE), 2, glob<const char *>(0x60b0a0) /* "!None" */, -1);
    if (none == -1)
        none = 0;
    DWORD i = dw(self, 0x14);
    if (i == (DWORD)none)
        return bp(dw(self, 0x10));
    BYTE *records = *glob<BYTE **>(0x6c0c38);
    if (!records)
        return bp(dw(self, 0x10));
    if (i >= *glob<DWORD *>(0x6c0c3c))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return bp(dw(self, 0x10));
    if (!dw(records + off, 0x78) && records && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(
            path, glob<const char *>(0x6c0c40), vcall<const char *>(glob<void *>(CATALOGUE), 1, i));
        char *name = const_cast<char *>(path_name(path));
        BYTE *slot = *glob<BYTE **>(0x6c0c38) + off + 0x78;
        DWORD o;
        if (name == path) {
            o = vcall<DWORD>(glob<void *>(CATALOGUE), 37, glob<const char *>(0x62e558), name, slot);
        } else {
            name[-1] = 0;
            o = vcall<DWORD>(glob<void *>(CATALOGUE), 37, path, name, *glob<BYTE **>(0x6c0c38) + off + 0x78);
        }
        dw(*glob<BYTE **>(0x6c0c38) + off, 0x78) = o;
        (*glob<BYTE **>(0x6c0c38))[off + 0x7f] = 0;
        records = *glob<BYTE **>(0x6c0c38);
    }
    BYTE *def = bp(dw(records + off, 0x78));
    if (def && is_kind_of(def, item_definition_class()))
        return definition_default_item(def);
    return bp(dw(self, 0x10));
}

// 0x53d990 (slot 24, Begin: entity): a COverridePickupAI on the entity starts its timer:
// "TimeUntilNextRespawn" = "Respawn Delay", "Time Offset" random in [0, delay), "Last Simulation Time" 0
static void THISCALL Pickup_Begin(BYTE *self, BYTE *e)
{
    (void)self;
    if (!e)
        return;
    BYTE *ov = find_ai(e, OverridePickup_ClassInfo());
    if (!ov)
        return;
    const DWORD delay = dw(ov, 0x20);
    fl<float>(ov, 0x28) = fstp(fld(fl<float>(ov, 0x20)));
    fl<float>(ov, 0x2c) = fstp(random_range(GAME_RANDOM, 0, delay));
    dw(ov, 0x30) = 0;
    dw(ov, 0x34) = 0;
}

// A copy of the pickup appears at a random place near it (respawn mode 3 while the pickup is still
// there): the copy's COverridePickupAI does not respawn (and goes when it holds no item), the copy is
// moved by a random distance below 1.5 · its polygon's size and put into the current layer
static void spawn_copy(BYTE *e)
{
    BYTE *copy = obj_clone(e);
    BYTE *ov = find_ai(copy, OverridePickup_ClassInfo());
    const DWORD item = dw(ov, 0x10);
    dw(ov, 0x1c) = 0;
    if (!item) {
        part_remove_ai(copy, ov);
        if (ov)
            vcall<void *>(ov, 1, 1);
    }
    const DWORD range = float_bits(fstp(polygon_size(slot_polygon(copy)) * cst(glob_addr(0x5fcc30))));   // 1.5
    const float r = fstp(random_range(GAME_RANDOM, 0, range));
    const long double a = random_range(GAME_RANDOM, 0, 0x40c90fdb /* 2 pi */);
    float c, d;
    table_sin_cos(&c, &d, fstp(-a));
    const float ox = fstp(fld(r) * fld(c));
    const float oy = fstp(fld(r) * fld(d));
    float pos[2];
    part_position(e, pos);
    const float x = fstp(fld(ox) + fld(pos[0]));
    const float y = fstp(fld(oy) + fld(pos[1]));
    part_move_center(copy, float_bits(x), float_bits(y));
    vcall<void>(copy, 39);
    layer_add_part(world_current_layer(), copy);
}

// 0x53da40 (slot 25, Update: entity, seconds): the respawn of a COverridePickupAI on the entity (the
// seconds since "Last Simulation Time" by the reporter's clock):
//  1 "Respawn Delay After Pickup": a present pickup keeps the timer at "Respawn Delay"; a gone one
//    (neither collideable nor visible) counts it down and comes back at 0.
//  2 "Respawn Regularly If Gone": the timer runs to the next multiple of the delay (from "Min Respawn
//    Delay" + "Time Offset") while present; a gone one counts down and comes back at 0.
//  3 "Respawn Regularly (Accumulate)": for every multiple of the delay passed since the last update a
//    present pickup gets a copy near it, a gone one comes back.
// Then CLayerPartAI's Update (0x46cd50; eax its).
static DWORD THISCALL Pickup_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    BYTE *ov = e ? find_ai(e, OverridePickup_ClassInfo()) : nullptr;
    if (ov) {
        DWORD last[2] = {dw(ov, 0x30), dw(ov, 0x34)};
        const float elapsed = fstp(now() - fld64(last));
        switch (dw(ov, 0x1c)) {
        case 1: {
            fst64(now(), ov + 0x30);
            const DWORD flags = dw(e, 0x1c);
            if ((flags & COLLIDEABLE) && (flags & VISIBLE)) {
                fl<float>(ov, 0x28) = fstp(fld(fl<float>(ov, 0x20)));
            } else if (!(flags & COLLIDEABLE) && !(flags & VISIBLE)) {
                const long double t = fld(fl<float>(ov, 0x28)) - fld(elapsed);
                fl<float>(ov, 0x28) = fstp(t);
                if (c0(t, cst(glob_addr(C_ZERO)))) {
                    slot_set_flags(e, COLLIDEABLE);
                    slot_set_flags(e, VISIBLE);
                    fl<float>(ov, 0x28) = fstp(fld(fl<float>(ov, 0x20)));
                }
            }
            break;
        }
        case 2: {
            const long double t = now();
            fst64(t, ov + 0x30);
            const DWORD flags = dw(e, 0x1c);
            if ((flags & COLLIDEABLE) && (flags & VISIBLE)) {
                const long double base = fld(fl<float>(ov, 0x2c)) + fld(fl<float>(ov, 0x24));
                const int k = x87_ftol((t - base) / fld(fl<float>(ov, 0x20))) + 1;
                fl<float>(ov, 0x28) = fstp(fild(k) * fld(fl<float>(ov, 0x20)) + base - t);
            } else if (!(flags & COLLIDEABLE) && !(flags & VISIBLE)) {
                const long double left = fld(fl<float>(ov, 0x28)) - fld(elapsed);
                fl<float>(ov, 0x28) = fstp(left);
                if (c0(left, cst(glob_addr(C_ZERO)))) {
                    slot_set_flags(e, COLLIDEABLE);
                    slot_set_flags(e, VISIBLE);
                    const long double base = fld(fl<float>(ov, 0x24)) + fld(fl<float>(ov, 0x2c));
                    const int k = x87_ftol((fld64(ov + 0x30) - base) / fld(fl<float>(ov, 0x20)));
                    fl<float>(ov, 0x28) = fstp(fild(k) * fld(fl<float>(ov, 0x20)) + base - fld(fl<float>(ov, 0x2c)));
                }
            }
            break;
        }
        case 3: {
            const int done = x87_ftol((fld64(ov + 0x30) - (fld(fl<float>(ov, 0x2c)) + fld(fl<float>(ov, 0x24)))) / fld(fl<float>(ov, 0x20)));
            const float min = fl<float>(ov, 0x24), off = fl<float>(ov, 0x2c), delay = fl<float>(ov, 0x20);
            const int due = x87_ftol((now() - (fld(min) + fld(off))) / fld(delay));
            const long double d = fld(fl<float>(ov, 0x20)), o = fld(fl<float>(ov, 0x2c)), m = fld(fl<float>(ov, 0x24));
            const float next = fstp(fild(due + 1) * d + (m + o));
            fl<float>(ov, 0x28) = fstp(fld(next) - now());
            if (done < due) {
                DWORD count = (DWORD)(due - done);
                do {
                    const DWORD flags = dw(e, 0x1c);
                    if ((flags & COLLIDEABLE) && (flags & VISIBLE)) {
                        spawn_copy(e);
                    } else if (!(flags & COLLIDEABLE) && !(flags & VISIBLE)) {
                        slot_set_flags(e, COLLIDEABLE);
                        slot_set_flags(e, VISIBLE);
                    }
                } while (--count);
            }
            fst64(now(), ov + 0x30);
            break;
        }
        default:
            break;
        }
    }
    return obj_on_message(self, e, seconds);
}

// 0x53de80 (slot 23: entity): "Tries To Collide" set
static void THISCALL Pickup_Slot23(BYTE *, BYTE *e) { slot_set_flags(e, 0x80000); }

// 0x53dea0 (slot 31: part, entity, -): true without an entity's item to give... al: the item (or the
// alternate item when the item refuses) accepts the entity (its slot 32; characters that may pick it up
// get it prepared first, the definition's slot 25); 1 when there is no item, 0 without an entity
static bool THISCALL Pickup_Accepts(BYTE *self, BYTE *part, BYTE *e, DWORD)
{
    if (!e)
        return false;
    BYTE *item = fn<BYTE *(THISCALL *)(void *, void *)>(0x53e4c0)(self, part);
    alignas(4) BYTE local[0x18];
    inv_item_ctor(local);
    BYTE *alt = alternate_item(self, part, local);
    bool r = true;
    if (item) {
        if (may_pick_up(self, e))
            prepare_item(item, e, part);
        r = vcall<bool>(item, 32, e);
        if (!r && alt) {
            if (may_pick_up(self, e))
                prepare_item(alt, e, part);
            r = vcall<bool>(alt, 32, e);
        }
    }
    inv_item_dtor(local);
    return r;
}

// 0x53e110 (part, entity, three more, out): when the fifth argument is set and the entity's category is
// in "Who Can Pick Up", the item (or, when it refuses, the alternate item; none ends the game as in the
// original) is given (its slot 33 with the part and the entity; the result into *out) and "Picked Up
// Action" runs. Then (also when the alternate item refuses too) the part's model's +0xa4 goes to the
// layer, "Show in HUD" shows the item, the part hides for a respawn (a COverridePickupAI with a
// "Respawn Mode") or is marked for deletion, CLayerPartAI's slot 34 runs and the "Pickup Effect"
// explosion is sent.
static void THISCALL Pickup_Collect(BYTE *self, BYTE *part, BYTE *e, DWORD a3, DWORD a4, DWORD a5, DWORD *out)
{
    if (!(BYTE)a5)
        return;
    BYTE *item = fn<BYTE *(THISCALL *)(void *, void *)>(0x53e4c0)(self, part);
    alignas(4) BYTE local[0x18];
    inv_item_ctor(local);
    BYTE *alt = alternate_item(self, part, local);
    if ((dw(e, 0x1c) & dw(self, 0x24)) && item) {
        if (vcall<bool>(item, 32, e)) {
            const DWORD v = vcall<DWORD>(item, 33, part, e);
            if (out)
                *out = v;
            if (dw(self, 0x28))
                action_execute(bp(dw(self, 0x28)), bp(dw(part, 0x48)), part, self, e, self, self + 0x28);
        } else if (vcall<bool>(alt, 32, e)) {
            const DWORD v = vcall<DWORD>(alt, 33, part, e);
            if (out)
                *out = v;
            if (dw(self, 0x28))
                action_execute(bp(dw(self, 0x28)), bp(dw(part, 0x48)), part, self, e, self, self + 0x28);
            item = alt;
        }
        if (is_kind_of(part, entity_base_class()))
            part_layer_add(part, dw(entity_model(part), 0xa4));
        if (fl<BYTE>(self, 0x2c)) {
            const DWORD player = entity_player(e);
            app_show_item(dword8(item), 2, player);
        }
        BYTE *ov = find_ai(part, OverridePickup_ClassInfo());
        if (ov && dw(ov, 0x1c)) {
            slot_clear_flags(part, COLLIDEABLE);
            slot_clear_flags(part, VISIBLE);
        } else {
            slot_set_flags(part, 0x10000);
        }
        fn<void(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD)>(0x53d140)(self, part, e, a3, a4, a5);
        if (dw(self, 0x20))
            world_explosion_at(bp(dw(part, 0x48)), dw(self, 0x20), part);
    }
    inv_item_dtor(local);
}

// 0x53e0e0 (slot 34, collision: part, entity, three more): 0x53e110 without an out value
static void THISCALL Pickup_Collide(BYTE *self, BYTE *part, BYTE *e, DWORD a3, DWORD a4, DWORD a5)
{
    fn<void(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD, DWORD *)>(0x53e110)(self, part, e, a3, a4, a5, nullptr);
}

// 0x53e680 (slot 11, one line of the object's text: name, value, reader): "Inventory Item" or "Custom
// Item" with a class name creates that object, reads it and, for a CInventoryItem with a definition,
// takes the definition as "Normal Item" (with a message box when the definition has no default item or
// a different one); the object is deleted again. Other lines go to CObject's 0x417500.
static DWORD THISCALL Pickup_ReadOwnLine(BYTE *self, const char *name, const char *value, void *reader)
{
    if (!value || !*value ||
        (crt_stricmp(name, glob<const char *>(0x60b744) /* "Inventory Item" */) && crt_stricmp(name, glob<const char *>(0x61e604) /* "Custom Item" */)))
        return fn<DWORD(THISCALL *)(void *, const char *, const char *, void *)>(0x417500)(self, name, value, reader);
    void *registry = fn<void *(__cdecl *)()>(0x415d20)();
    BYTE *o = fn<BYTE *(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x415c00)(registry, value, 0, 0xffffffff);
    fn<void(THISCALL *)(void *, void *)>(0x416af0)(o, reader);
    BYTE *item = o && is_kind_of(o, inventory_item_class()) ? o : nullptr;
    if (BYTE *def = item_definition(item)) {
        BYTE *default_item = definition_default_item(def);
        if (!default_item || fn<bool(THISCALL *)(void *, void *)>(0x417100)(default_item, item)) {
            alignas(4) BYTE text[4];
            zs_ctor(text);
            const char *list_name;
            if (default_item) {
                zs_assign_text(text, glob<const char *>(0x61e5c4) /* "Please check the Default Item for Inventory Item Definition '" */);
                list_name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(CATALOGUE), dword8(item));
                fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(text, list_name);
                fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(text, glob<const char *>(0x61e580));
                fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x61e570) /* "Please Check " */, zs_c_str(text));
            } else {
                zs_assign_text(text, glob<const char *>(0x61e55c) /* "Item Definition '" */);
                list_name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(CATALOGUE), dword8(item));
                fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(text, list_name);
                fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(text, glob<const char *>(0x61e518));
                fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x61e4f8) /* "Item Definition must be fixed" */,
                                                                            zs_c_str(text));
            }
            zs_dtor(text);
        }
        dw(self, 0x14) = dword8(item);
    }
    if (o)
        return vcall<DWORD>(o, 1, 1);
    return 0;
}

// ---- CLayerPartAI's slot 31/33 ----------------------------------------------------------------------------

// 0x53e0d0 (slots 31 and 33 of the AIs: three arguments): al 1
static bool THISCALL LayerPartAI_True(BYTE *, DWORD, DWORD, DWORD) { return true; }

// ---- FieldColor -------------------------------------------------------------------------------------------

static DWORD pixel16(DWORD r, DWORD g, DWORD b) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(r, g, b); }
static void color_bytes(BYTE *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    c[2] = (BYTE)r;
    c[0] = (BYTE)b;
    c[1] = (BYTE)g;
    c[3] = (BYTE)a;
    c[6] = (BYTE)r;
    c[4] = (BYTE)b;
    c[5] = (BYTE)g;
}
// 0x53f000: flags 0 (eax: this)
static BYTE *THISCALL FieldColor_Ctor(BYTE *c)
{
    dw(c, 0xc) = 0;
    return c;
}
// 0x53f010 (r, g, b, a): flags 0, the bytes and the pixel, flag 2. eax: this.
static BYTE *THISCALL FieldColor_CtorRGBA(BYTE *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    dw(c, 0xc) = 0;
    color_bytes(c, r, g, b, a);
    fl<WORD>(c, 8) = (WORD)pixel16(r, g, b);
    dw(c, 0xc) |= 2;
    return c;
}
// 0x53f060: opaque black, pixel and palette index 0 (the flags kept). eax 0.
static DWORD THISCALL FieldColor_SetBlack(BYTE *c)
{
    dw(c, 0) = 0xff000000;
    dw(c, 4) = 0;
    fl<WORD>(c, 8) = 0;
    c[0xa] = 0;
    return 0;
}
// 0x53f0c0 (r, g, b, a): as 0x53f080 but flag 2 cleared. eax: the flags.
static DWORD THISCALL FieldColor_Set(BYTE *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    color_bytes(c, r, g, b, a);
    fl<WORD>(c, 8) = (WORD)pixel16(r, g, b);
    dw(c, 0xc) &= ~2u;
    return dw(c, 0xc);
}
// 0x53f100 (palette index): the colour of entry i of the palette 0x433a80 (its bytes 0, 1, 2 as g, b,
// r), opaque, the pixel, the index, flags 1 and 2. eax: the flags.
static DWORD THISCALL FieldColor_FromPalette(BYTE *c, DWORD index)
{
    BYTE *palette = fn<BYTE *(THISCALL *)(void *)>(0x433a80)(c);
    const BYTE *e = reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(dw(palette, 0))) + index * 3;
    const DWORD p1 = e[1], p0 = e[0], p2 = e[2];
    c[0] = (BYTE)p1;
    c[4] = (BYTE)p1;
    c[2] = (BYTE)p2;
    c[1] = (BYTE)p0;
    c[3] = 0xff;
    c[6] = (BYTE)p2;
    c[5] = (BYTE)p0;
    fl<WORD>(c, 8) = (WORD)pixel16(p2, p0, p1);
    c[0xa] = (BYTE)index;
    dw(c, 0xc) |= 3;
    return dw(c, 0xc);
}
// 0x53f160: the palette index (nearest colour, 0x526e40 (r, g, b, exact, 256)): with flag 2 in the
// palette 0x633388, otherwise in 0x433a80's or, when that has no data, the reporter's +4 object's slot 58.
// +0xa and flag 1 set. eax: the flags with the index in al.
static DWORD THISCALL FieldColor_PaletteIndex(BYTE *c)
{
    void *palette;
    DWORD exact;
    if (dw(c, 0xc) & 2) {
        palette = glob<void *>(0x633388);
        exact = 0;
    } else {
        exact = 1;
        if (dw(fn<BYTE *(THISCALL *)(void *)>(0x433a80)(c), 0))
            palette = fn<void *(THISCALL *)(void *)>(0x433a80)(c);
        else
            palette = vcall<void *>(bp(dw(reporter(), 4)), 58);
    }
    const BYTE i = (BYTE)fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526e40)(palette, c[6], c[5], c[4], exact, 0x100);
    c[0xa] = i;
    dw(c, 0xc) |= 1;
    return (dw(c, 0xc) & 0xffffff00u) | c[0xa];
}

ZAX_HOOKS(ai_pickup)
{
    const Hook list[] = {
#include "ai_pickup_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
