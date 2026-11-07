// The shields and the segmented items of 0x569560-0x56fe93 reimplemented: CSegmentedInventoryItem and the
// non-drawing part of its CSegmentedInventoryDetailRenderer, CShieldAI, CPercentInvulnerableAI,
// CShieldInventoryItem, CShieldChargeInventoryItem and CRechargeShieldAction: class getters (both
// copies), Create/CreateArray where ai_classes.cpp has none, deleting destructors (slot 1),
// RegisterFields, the behaviour slots and their helpers, and the heap pointer array constructor 0x56f8a0.
// Left out: the detail renderer's drawing (slot 24 0x56a3a0), the shield's HUD render data and HUD
// renderer (0x56e640-0x56f4a0; Rendering).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CSegmentedInventoryItem (0x28, class 0x713350, vtable 0x5ff478, parent CInventoryItem): +0x18 the
//    "Segments" it holds (pointer array vtable 0x5ee138, base 0x5ee15c; data +0x1c, count +0x20, capacity
//    +0x24; the item catalogue's "Segment" records 0x6c0948 as indices).
//  - CSegmentedInventoryDetailRenderer (0x20, class 0x713354, vtable 0x5ff53c, parent
//    CInventoryDetailRenderer, vtable 0x5f10d8): +8 "Completed Item" (alternative name "Compleated Item";
//    an item catalogue index), +0xc the segments needed (pointer array as above, data +0x10, count +0x14),
//    +0x1c "Compleated Action" ("Action to execute when the gun is fully assembled").
//  - CShieldAI (0x18, class 0x71350c, version 4, vtable 0x5ffa9c): +0x10 "Debug/ShieldEntity" (the shield's
//    entity, a child of the character, listened to), +0x14 "Damage Type" (the type the shield blocks).
//  - CPercentInvulnerableAI (0x18, class 0x713510, version 4, vtable 0x5ffb44): +0x10 "Damage To Take" (a
//    ratio, shown in %), +0x14 "Damage Type/Damage Types" (flags).
//  - CShieldInventoryItem (0x28, class 0x713520, version 3, vtable 0x5ffbec, parent CEnergyItem, "Current
//    Charge" +0x18): +0x1c "AI" (the CShieldAI template given to the character), +0x20 "Damage Type",
//    +0x24 "Shield Gfx" (a model index).
//  - CShieldChargeInventoryItem (0x1c, class 0x71352c, vtable 0x5ffe18, parent CEnergyItem): no fields of its
//    own; "Current Charge" becomes "Charge to Add" in "MJ".
//  - CRechargeShieldAction (0x18, class 0x713534, version 3, vtable 0x5ffee4, parent CAction): +8 "Target"
//    ("$Instigator"), +0xc "Recharge Percent" (1), +0x10 "Recharge Uints" (sic; 0 = use the percent), +0x14
//    "Damage Type".
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_SEGMENTED = 0x5ff478, VT_DETAIL_RENDERER = 0x5f10d8, VT_SEGMENTED_RENDERER = 0x5ff53c, VT_SHIELD_AI = 0x5ffa9c,
                VT_PERCENT = 0x5ffb44, VT_ENERGY_ITEM = 0x5f07e0, VT_SHIELD_ITEM = 0x5ffbec, VT_SHIELD_CHARGE = 0x5ffe18,
                VT_RECHARGE = 0x5ffee4;
constexpr DWORD CATALOGUE = 0x6c0c08;   // the item catalogue (records 0x6c0c38, count 0x6c0c3c, folder 0x6c0c40)
constexpr DWORD TEXT_NO_RENDERER = 0x621440;   // "Found a CSegmentedInventoryItem that does not have the REQUIRED ..."
constexpr DWORD TEXT_NOT_SEGMENTED = 0x6213e8; // "Tried to use an inventory item as a segmented Inventory item ..."

// The error reporter's slot 17 (a message)
void report(GlobKey text) { vcall<void>(reporter(), 17, glob<const char *>(text)); }
// CEnergyItem (zax_items.cpp): destructor 0x5afe30, the most charge 0x5afe50 (st0), Add 0x5afe90
DWORD energy_item_dtor(BYTE *o) { return fn<DWORD(THISCALL *)(void *)>(0x5afe30)(o); }
long double max_charge(BYTE *item) { return fn<long double(THISCALL *)(void *)>(0x5afe50)(item); }
void energy_add(BYTE *item, DWORD amount) { fn<void(THISCALL *)(void *, DWORD)>(0x5afe90)(item, amount); }
// CInventory helpers
DWORD inventory_index_of(BYTE *inv, DWORD definition) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4257a0)(inv, definition); }
BYTE *inventory_find_of_class(BYTE *inv, CClassInfo *c) { return fn<BYTE *(THISCALL *)(void *, CClassInfo *)>(0x425860)(inv, c); }
// 0x470e60: the index of a value in a pointer array (the second argument a start index), -1 if none
DWORD array_find(BYTE *a, DWORD *v, DWORD start) { return fn<DWORD(THISCALL *)(void *, DWORD *, DWORD)>(0x470e60)(a, v, start); }
// The game world's slot 58 (the difficulty settings: +0x20 the damage a player takes, +0x24 the charge a
// player gets)
BYTE *world_settings() { return vcall<BYTE *>(world(), 58); }
// The pointer array's linear search as the exe inlines it (the test of the element before the data
// for start -1 is always skipped): the index, -1 if none
DWORD find_inline(BYTE *data, DWORD count, DWORD v)
{
    for (DWORD i = 0; i < count; i++)
        if (dw(data, i * 4) == v)
            return i;
    return 0xffffffffu;
}

}  // namespace

// ---- CSegmentedInventoryDetailRenderer -------------------------------------------------------------------

// 0x569e50 (static registration), 0x56a0a0 (slot 2)
static CClassInfo *__cdecl SegmentedRenderer_ClassInfo()
{
    return class_getter(0x713354, 0x621524 /* "CSegmentedInventoryDetailRenderer" */, 1.0f, getter<0x5acc00>, 0x20, 0, 0x569ea0, 0x569f00,
                        0x56a100);
}
static void SegmentedRenderer_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_DETAIL_RENDERER));
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    dw(o, 0xc) = vt(0x5ee138);
    vtable_and_defaults(o, vt(VT_SEGMENTED_RENDERER));
}
static void SegmentedRenderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SEGMENTED_RENDERER));
    pool_array_dtor(o + 0xc, vt(0x5ee15c));
    vtable_and_free(o, vt(VT_DETAIL_RENDERER));
    obj_dtor(o);
}
// 0x569ea0
static void *__cdecl SegmentedRenderer_Create() { return create_one<0x20, SegmentedRenderer_construct>(); }
// 0x569f00
static void *__fastcall SegmentedRenderer_CreateArray(int count, int, int) { return create_array<0x20, SegmentedRenderer_construct>(count); }
// 0x569f90 (slot 1)
static void *THISCALL SegmentedRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, SegmentedRenderer_destroy>(self, flags); }
// 0x56a100. eax: FieldDesc_SetHelp's.
static DWORD __cdecl SegmentedRenderer_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6215a0) /* "Completed Item" */, 8, 4, 8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(SegmentedRenderer_ClassInfo(), f);
    set_alt_name(last_field(SegmentedRenderer_ClassInfo()), glob_addr(0x621590) /* "Compleated Item" */);
    void *element = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x621394) /* "Segment" */, 0, 4, 0, glob_addr(0x6c0948), 1, 0, 0, 0);
    f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x621388) /* "Segments" */, 0x14, 4, 0x14, 0x18, 4, 0x18, 0x10, 4, 0x10, 1, element, 1, 0);
    add_field(SegmentedRenderer_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x62157c) /* "Compleated Action" */, 0x1c, 4, 0x1c, action_class(), 1, 0, 0, 0);
    add_field(SegmentedRenderer_ClassInfo(), f);
    return set_help(last_field(SegmentedRenderer_ClassInfo()), 0x621548 /* "Action to execute when the gun is fully assembled" */);
}
// 0x56a7e0 (ecx): the object when it is a CZaxInventoryItemDefinition, otherwise 0
static BYTE *THISCALL AsZaxDefinition(BYTE *o) { return o && is_kind_of(o, zax_item_definition_class()) ? o : nullptr; }
// 0x56a800 (ecx): the object when it is a CSegmentedInventoryDetailRenderer, otherwise 0
static BYTE *THISCALL AsSegmentedRenderer(BYTE *o)
{
    if (o) {
        CClassInfo *c = SegmentedRenderer_ClassInfo();
        if (is_kind_of(o, c))
            return o;
    }
    return nullptr;
}
// 0x56a870 (ecx): the object when it is a CInventoryItem, otherwise 0
static BYTE *THISCALL AsInventoryItem(BYTE *o) { return o && is_kind_of(o, inventory_item_class()) ? o : nullptr; }
// 0x56a6b0 (ecx: the renderer): the template item (+0x1c) of the "Completed Item" catalogue record
// (loaded on first use, the lookup 0x48d8f0 inlined); a message and 0 when it is no CInventoryItem
static BYTE *THISCALL SegmentedRenderer_CompletedItem(BYTE *self)
{
    BYTE *def = *glob<BYTE **>(0x6c0c38) ? list_object(glob_addr(CATALOGUE), glob_addr(0x6c0c38), glob_addr(0x6c0c3c), glob_addr(0x6c0c40), dw(self, 8)) : nullptr;
    BYTE *item = bp(dw(def, 0x1c));   // no record: the exe reads address 0x1c too
    if (!item || !is_kind_of(item, inventory_item_class())) {
        report(0x6214a4);
        return nullptr;
    }
    return item;
}
// 0x56a5a0 (slot 25: item, -): the detail text of a segmented item: a new line object (0x1c from the pool,
// CObject with two strings, vtable 0x5f1140, +8 0, +0x10 and +0x18 3) with the item's display name and "%u
// of %u Pieces" (the item's segments, the renderer's). A message and 0 for an item that is not segmented.
static BYTE *THISCALL SegmentedRenderer_Describe(BYTE *self, BYTE *item, DWORD)
{
    if (!item || !is_kind_of(item, class_at(fn_addr(0x569560)) /* CSegmentedInventoryItem */)) {
        vcall<void>(reporter(), 17, glob<const char *>(0x6215b0) /* "A CSegmentedInventoryDetailRenderer is attached to a non-..." */);
        return nullptr;
    }
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x1c));
    if (o) {
        obj_ctor(o);
        zs_ctor(o + 0xc);
        zs_ctor(o + 0x14);
        vtable_and_defaults(o, vt(0x5f1140));
        dw(o, 8) = 2;
    }
    dw(o, 8) = 0;   // a failed allocation writes to address 8 as in the exe
    dw(o, 0x10) = 3;
    dw(o, 0x18) = 3;
    fn<void(__fastcall *)(DWORD, void *)>(0x4d66d0)(dw(item, 8), o + 0xc);   // Item_DisplayName
    fn<int(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(o + 0x14, glob<const char *>(0x621604) /* "%u of %u Pieces" */, dw(item, 0x20),
                                                           dw(self, 0x14));
    return o;
}

// ---- CSegmentedInventoryItem -------------------------------------------------------------------------------

// 0x569560 (static registration), 0x569770 (slot 2)
static CClassInfo *__cdecl SegmentedItem_ClassInfo()
{
    return class_getter(0x713350, 0x621370 /* "CSegmentedInventoryItem" */, 1.0f, inventory_item_class, 0x28, 0, 0x5695b0, 0x569600, 0x5697d0);
}
static void SegmentedItem_construct(BYTE *o)
{
    item_ctor(o);
    dw(o, 0x1c) = 0;
    dw(o, 0x20) = 0;
    dw(o, 0x24) = 0;
    dw(o, 0x18) = vt(0x5ee138);
    vtable_and_defaults(o, vt(VT_SEGMENTED));
}
static void SegmentedItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SEGMENTED));
    pool_array_dtor(o + 0x18, vt(0x5ee15c));
    item_dtor(o);
}
// 0x5695b0
static void *__cdecl SegmentedItem_Create() { return create_one<0x28, SegmentedItem_construct>(); }
// 0x569600
static void *__fastcall SegmentedItem_CreateArray(int count, int, int) { return create_array<0x28, SegmentedItem_construct>(count); }
// 0x569680 (slot 1)
static void *THISCALL SegmentedItem_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, SegmentedItem_destroy>(self, flags); }
// 0x5697d0. eax: ClassInfo_AddField's.
static DWORD __cdecl SegmentedItem_RegisterFields()
{
    void *element = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x621394) /* "Segment" */, 0, 4, 0, glob_addr(0x6c0948), 1, 0, 0, 0);
    void *f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x621388) /* "Segments" */, 0x20, 4, 0x20, 0x24, 4, 0x24, 0x1c, 4, 0x1c, 1, element, 1, 0);
    return add_field(SegmentedItem_ClassInfo(), f);
}
// The segmented renderer of an item's definition (slot 34 of a CZaxInventoryItemDefinition), 0 if none
static BYTE *definition_renderer(BYTE *item)
{
    BYTE *def = item_definition(item);
    if (def && is_kind_of(def, zax_item_definition_class()))
        return AsSegmentedRenderer(vcall<BYTE *>(def, 34));
    return nullptr;
}
// The same through 0x56a7e0 as the exe writes it in some places
static BYTE *definition_renderer2(BYTE *item)
{
    if (BYTE *def = AsZaxDefinition(item_definition(item)))
        return AsSegmentedRenderer(vcall<BYTE *>(def, 34));
    return nullptr;
}
// 0x569da0 (ecx: an item): its definition's segmented renderer; a message and 0 if none
static BYTE *THISCALL SegmentedItem_Renderer(BYTE *item)
{
    BYTE *r = nullptr;
    BYTE *def = item_definition(item);
    if (def && is_kind_of(def, zax_item_definition_class()))
        if (BYTE *x = vcall<BYTE *>(def, 34)) {
            CClassInfo *c = SegmentedRenderer_ClassInfo();
            if (is_kind_of(x, c))
                return x;
        }
    report(TEXT_NO_RENDERER);
    return r;
}
// 0x5698a0 (slot 33, taken: taker, part): a part without such an item takes it as any item
// (CInventoryItem::Take); otherwise the segments it brings that the renderer needs are added to the
// segmented item the part has (made the inventory's focus +0xc), and once that has as many segments as
// the renderer needs, the "Completed Item" is given (its slots 32 and 33; the renderer's "Compleated
// Action" runs) and the segmented item is removed. eax: the item that took the segments or the completed
// item's answer, 0 otherwise.
static DWORD THISCALL SegmentedItem_Take(BYTE *self, BYTE *taker, BYTE *part)
{
    DWORD result = 0;
    BYTE *renderer = definition_renderer(self);
    if (!renderer)
        report(TEXT_NO_RENDERER);
    BYTE *inv = part_inventory(part);
    if (!inv)
        return 0;
    const DWORD idx = inventory_index_of(inv, dw(self, 8));
    if (idx == 0xffffffffu)
        return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x42a550)(self, taker, part);
    BYTE *existing = inventory_item(inv, idx);
    CClassInfo *c = SegmentedItem_ClassInfo();
    if (!is_kind_of(existing, c)) {
        report(TEXT_NOT_SEGMENTED);
        return 0;
    }
    for (DWORD i = 0; i < dw(self, 0x20); i++) {
        DWORD seg = dw(bp(dw(self, 0x1c)), i * 4);
        if (BYTE *r = SegmentedItem_Renderer(existing)) {
            DWORD v = seg;
            if (array_find(r + 0xc, &v, 0xffffffffu) != 0xffffffffu) {
                DWORD w = seg;
                if (array_find(existing + 0x18, &w, 0xffffffffu) == 0xffffffffu) {
                    vcall<void>(existing + 0x18, 2, &seg);
                    result = ptr_value(existing);
                }
            }
        }
    }
    dw(inv, 0xc) = idx;
    BYTE *r2 = definition_renderer2(existing);
    if (!r2) {
        report(TEXT_NO_RENDERER);
        return result;
    }
    if (dw(existing, 0x20) != dw(r2, 0x14))
        return result;
    BYTE *r3 = definition_renderer(self);
    if (!r3) {
        report(TEXT_NO_RENDERER);
        return result;
    }
    BYTE *completed = SegmentedRenderer_CompletedItem(r3);
    if (!completed)
        return result;
    if (vcall<DWORD>(completed, 32, part) & 0xff) {
        result = vcall<DWORD>(completed, 33, taker, part);
        if (dw(renderer, 0x1c))
            action_execute(bp(dw(renderer, 0x1c)), bp(dw(taker, 0x48)), taker, nullptr, taker, renderer, renderer + 0x1c);
    } else {
        report(0x62139c /* "CanPickup failed for the Completed version of the segmended inventory item" */);
        result = 0;
    }
    fn<void(THISCALL *)(void *, DWORD)>(0x426090)(inv, idx);   // CInventory_RemoveAt
    return result;
}
// 0x569b70 (slot 32, may the part take it: part): the "Completed Item" must accept the part (its slot
// 32); a part without the item may take it as any item (CInventoryItem::CanAdd); one with it only when
// this item brings a segment the renderer needs that it lacks. al.
static DWORD THISCALL SegmentedItem_MayTake(BYTE *self, BYTE *part)
{
    BYTE *renderer = definition_renderer2(self);
    if (!renderer) {
        report(TEXT_NO_RENDERER);
        return 0;
    }
    BYTE *record = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(CATALOGUE), dw(renderer, 8));
    BYTE *completed = AsInventoryItem(bp(dw(record, 0x1c)));
    if (!completed) {
        report(0x6214a4);
        return 0;
    }
    if (!(vcall<DWORD>(completed, 32, part) & 0xff))
        return 0;
    BYTE *inv = part_inventory(part);
    if (!inv)
        return 0;
    const DWORD idx = inventory_index_of(inv, dw(self, 8));
    if (idx == 0xffffffffu)
        return fn<DWORD(THISCALL *)(void *, void *)>(0x42a500)(self, part);
    BYTE *existing = inventory_item(inv, idx);
    CClassInfo *c = SegmentedItem_ClassInfo();
    if (!is_kind_of(existing, c)) {
        report(TEXT_NOT_SEGMENTED);
        return 0;
    }
    BYTE any = 0;
    for (DWORD i = 0; i < dw(self, 0x20); i++) {
        const DWORD seg = dw(bp(dw(self, 0x1c)), i * 4);
        BYTE *r = nullptr;
        BYTE *def = item_definition(existing);
        if (def && is_kind_of(def, zax_item_definition_class()))
            r = vcall<BYTE *>(def, 34);
        if (!r || !is_kind_of(r, SegmentedRenderer_ClassInfo())) {
            report(TEXT_NO_RENDERER);
            continue;
        }
        if (find_inline(bp(dw(r, 0x10)), dw(r, 0x14), seg) == 0xffffffffu)
            continue;
        if (find_inline(bp(dw(existing, 0x1c)), dw(existing, 0x20), seg) == 0xffffffffu)
            any = 1;
    }
    return any;
}

// ---- CShieldAI ---------------------------------------------------------------------------------------------

// 0x56d460 (static registration), 0x56d650 (slot 2)
static CClassInfo *__cdecl ShieldAI_ClassInfo()
{
    return class_getter(0x71350c, 0x6218fc /* "CShieldAI" */, 4.0f, layer_part_ai_class, 0x18, 0, 0x56d4b0, 0x56d500, 0x56d6b0);
}
static void ShieldAI_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_SHIELD_AI);
    if (BYTE *shield = bp(dw(o, 0x10))) {
        remove_listener(shield, o, -1, glob_addr(TEXT_EMPTY));
        part_died(bp(dw(o, 0x10)));
        dw(o, 0x10) = 0;
    }
    obj_free_fields(o);
    layer_part_ai_dtor(o);
}
// 0x56d570 (slot 1)
static void *THISCALL ShieldAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, ShieldAI_destroy>(self, flags); }
// 0x56d6b0. eax: ClassInfo_AddField's.
static DWORD __cdecl ShieldAI_RegisterFields()
{
    void *f = new_field(0x30, OBJECT_REF_FIELD_CTOR, glob_addr(0x621908) /* "Debug/ShieldEntity" */, 0x10, 4, 0x10, layer_part_class(), 3, 0);
    add_field(ShieldAI_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610318) /* "Damage Type" */, 0x14, 4, 0x14, glob_addr(0x6bd760), 1, glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    return add_field(ShieldAI_ClassInfo(), f);
}
// 0x56d7d0 (character or 0): the character's CShieldInventoryItem of the AI's damage type (searched in the
// category "Shield"), 0 if none
static BYTE *THISCALL ShieldAI_FindItem(BYTE *self, BYTE *character)
{
    BYTE *inv = part_inventory(character);
    const DWORD n = inventory_count_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    DWORD i = inventory_first_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    for (DWORD k = 0; k < n; k++) {
        BYTE *item = inventory_item(inv, i);
        if (item && is_kind_of(item, class_at(fn_addr(0x56e010)) /* CShieldInventoryItem */) && dw(item, 0x20) == dw(self, 0x14))
            return item;
        i = inventory_next_of(inv, i, category_index(glob_addr(TEXT_SHIELD)));
    }
    return nullptr;
}
// 0x56d880 (slot 25, Update: entity, seconds): a visible shield entity plays its sequence on (the frame
// advanced by the sequence player's rate (slot 26) × the seconds) and is hidden at the end of the sequence
// (slot 59 with "Visible")
static void THISCALL ShieldAI_Update(BYTE *self, BYTE *e, float seconds)
{
    BYTE *shield = bp(dw(self, 0x10));
    if (!shield || !(dw(shield, 0x1c) & 0x20000))
        return;
    float pos[2];
    slot_position(e, pos);
    BYTE *(THISCALL *const player)(void *) = code<BYTE *(THISCALL *)(void *)>(0x48b590);   // Entity_SequenceObject
    if (!player(shield))
        return;
    const long double f = vcall<long double>(shield, 84);
    const float frame = fstp(f);
    const float old = fstp(f);
    const float next = fstp(vcall<long double>(player(shield), 26) * fld(seconds) + fld(frame));
    const DWORD frames = dw(player(shield), 0x48);
    if (c03(fild64((long long)frames), next)) {
        slot_clear_flags(shield, 0x20000);
        return;
    }
    BYTE *p = player(shield);
    fn<void(THISCALL *)(void *, DWORD, DWORD, void *)>(0x560300)(p, float_bits(old), float_bits(next), shield);
    fn<void(THISCALL *)(void *, DWORD)>(0x48feb0)(shield, float_bits(next));   // EntityAnimated_SetFrame
}
// 0x56d960 (slot 34: entity, other, point, two more): the first hit makes the shield entity from the
// item's "Shield Gfx" (a CEntityAnimated child of the character, "Visible" and "Stationary" cleared,
// listened to); the shield turns towards the point (atan2 of the point minus the character's centre, y
// first; slot 104) and gets an entity's rendering height (slot 54)
static void THISCALL ShieldAI_Collide(BYTE *self, BYTE *e, DWORD, const float *point, DWORD, DWORD)
{
    if (!dw(self, 0x10)) {
        BYTE *character = e && is_kind_of(e, entity_character_class()) ? e : nullptr;
        BYTE *item = ShieldAI_FindItem(self, character);
        if (!item)
            return;
        const DWORD model = dw(item, 0x24);
        BYTE *shield = fn<BYTE *(THISCALL *)(void *, DWORD, CClassInfo *, DWORD, DWORD, int)>(0x5176f0)(glob<void *>(0x6cfdd8), model,
                                                                                                       entity_animated_class(), 0, 0, -1);
        dw(self, 0x10) = ptr_value(shield);
        slot_clear_flags(shield, 0x220000);
        fn<void(THISCALL *)(void *, void *)>(0x4fc710)(e, bp(dw(self, 0x10)));   // CLayerPart_AddChild
        add_listener(bp(dw(self, 0x10)), self, -1, glob_addr(TEXT_EMPTY));
    }
    float pos[2];
    part_position(e, pos);
    const long double dx = fld(pos[0]) - fld(point[0]);
    const float dy = fstp(fld(pos[1]) - fld(point[1]));
    const float mdx = fstp(-dx);
    const float mdy = fstp(-fld(dy));
    const float angle = fstp(table_atan2(mdy, mdx));
    vcall<void>(bp(dw(self, 0x10)), 104, float_bits(angle), 1);
    if (is_kind_of(e, entity_base_class())) {
        const DWORD h = vcall<DWORD>(e, 54);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x48b6b0)(bp(dw(self, 0x10)), h);   // Entity_SetRenderHeight
    }
}
// 0x56da70 (slot 30: entity, damage, type): the damage left after the shield (st0). Damage of the item's
// type (unless its damage type record +8 says it cannot be blocked) is absorbed while the item has charge:
// a player's damage scaled by the world settings' +0x20 first; less than the charge: the shield plays
// the model's default sequence (+0x70) and the item uses that much, otherwise "Shutdown" and the item
// uses its whole charge; either way the whole damage is absorbed, the model's sound (+0xa4 or +0x54) is
// added at the entity, the shield made visible from frame 0.
static long double THISCALL ShieldAI_Damage(BYTE *self, BYTE *e, float damage, DWORD type)
{
    if (!dw(self, 0x10))
        return fld(damage);
    BYTE *character = e && is_kind_of(e, entity_character_class()) ? e : nullptr;
    BYTE *item = ShieldAI_FindItem(self, character);
    if (!item)
        return fld(damage);
    BYTE *records = *glob<BYTE **>(0x6bd790);
    BYTE *dtype = nullptr;
    if (records) {
        DWORD i = dw(item, 0x20);
        if (i >= *glob<DWORD *>(0x6bd794))
            i = 0;
        if (!records[(i << 7) + 0x7f]) {
            if (!dw(records + (i << 7), 0x78)) {
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(0x6bd760), i);
                records = *glob<BYTE **>(0x6bd790);
            }
            dtype = bp(dw(records + (i << 7), 0x78));
        }
    }
    if (fl<BYTE>(dtype, 8))   // no record: the exe reads address 8 too
        return fld(damage);
    if (type != dw(item, 0x20))
        return fld(damage);
    float absorbed = 0;
    if (dw(self, 0x10) && !c03(fld(damage), cst(glob_addr(C_ZERO)))) {
        const float charge = fl<float>(item, 0x18);
        if (!c3(fld(charge), cst(glob_addr(C_ZERO)))) {
            if (*glob<DWORD *>(CATEGORY_PLAYER) & dw(character, 0x1c))
                damage = fstp(fld(fl<float>(world_settings(), 0x20)) * fld(damage));
            absorbed = damage;
            BYTE *shield = bp(dw(self, 0x10));
            DWORD what;
            if (!c0(fld(damage), charge)) {
                ent_play_sequence(shield, glob_addr(0x62191c) /* "Shutdown" */);
                vcall<void>(item, 50, float_bits(charge));
                what = dw(entity_model(bp(dw(self, 0x10))), 0x54);
            } else {
                ent_play_sequence(shield, ptr_value(zs_c_str(entity_model(shield) + 0x70)));
                vcall<void>(item, 50, float_bits(damage));
                what = dw(entity_model(bp(dw(self, 0x10))), 0xa4);
            }
            part_layer_add(e, what);
            float p[2];
            slot_position(character, p);
            slot_set_flags(bp(dw(self, 0x10)), 0x20000);
            fn<void(THISCALL *)(void *, DWORD)>(0x490010)(bp(dw(self, 0x10)), 0);   // EntityAnimated_SetFrameIndex
        }
    }
    return fld(damage) - fld(absorbed);
}
// 0x56dc50 (slot 0, a message: sender, text): "Died" or "Delete" (case ignored) of the shield entity: no
// longer listened to, forgotten
static void THISCALL ShieldAI_OnMessage(BYTE *self, void *, const char *text)
{
    if (!crt_stricmp(text, glob<const char *>(TEXT_DIED)) || !crt_stricmp(text, glob<const char *>(TEXT_DELETE))) {
        remove_listener(bp(dw(self, 0x10)), self, -1, glob_addr(TEXT_EMPTY));
        dw(self, 0x10) = 0;
    }
}

// ---- CPercentInvulnerableAI --------------------------------------------------------------------------------

// 0x56dca0 (static registration), 0x56de40 (slot 2)
static CClassInfo *__cdecl PercentInvulnerableAI_ClassInfo()
{
    return class_getter(0x713510, 0x621928 /* "CPercentInvulnerableAI" */, 4.0f, layer_part_ai_class, 0x18, 0, 0x56dcf0, 0x56dd30, 0x56dea0);
}
static void PercentInvulnerableAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_PERCENT));
    layer_part_ai_dtor(o);
}
// 0x56dda0 (slot 1)
static void *THISCALL PercentInvulnerableAI_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x18, PercentInvulnerableAI_destroy>(self, flags);
}
// 0x56dea0. eax: ClassInfo_AddField's.
static DWORD __cdecl PercentInvulnerableAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x62195c) /* "Damage To Take" */, glob_addr(0x60f5ec) /* "PerWhole" */, 0x10, 4, 0x10, 1,
                        0x3f800000 /* 1.0 */, 0, 0, 0x3f800000 /* 1.0 */, 0);
    add_field(PercentInvulnerableAI_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be348);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60f5d8) /* "%" */);
    f = new_field(0x38, FLAGS_FIELD_CTOR, glob_addr(0x621940) /* "Damage Type/Damage Types" */, 0x14, 4, 0x14, glob_addr(0x6bd760), 2, glob_addr(0x6179cc) /* "$All" */, 0);
    return add_field(PercentInvulnerableAI_ClassInfo(), f);
}
// 0x56dfe0 (slot 30: entity, damage, type): damage of one of the types times "Damage To Take" (st0)
static long double THISCALL PercentInvulnerableAI_Damage(BYTE *self, DWORD, float damage, DWORD type)
{
    if (dw(self, 0x14) & (1u << (type & 31)))
        return fld(damage) * fld(fl<float>(self, 0x10));
    return fld(damage);
}

// ---- CShieldInventoryItem -------------------------------------------------------------------------------------

// 0x56e010 (static registration), 0x56e1a0 (slot 2)
static CClassInfo *__cdecl ShieldItem_ClassInfo()
{
    return class_getter(0x713520, 0x62196c /* "CShieldInventoryItem" */, 3.0f, energy_item_class, 0x28, 0, 0x56e060, 0x56e0b0, 0x56e200);
}
static void ShieldItem_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_ENERGY_ITEM));
    vtable_and_defaults(o, vt(VT_SHIELD_ITEM));
    dw(o, 0x20) = 0;
}
static void ShieldItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SHIELD_ITEM));
    energy_item_dtor(o);
}
// 0x56e060
static void *__cdecl ShieldItem_Create() { return create_one<0x28, ShieldItem_construct>(); }
// 0x56e0b0
static void *__fastcall ShieldItem_CreateArray(int count, int, int) { return create_array<0x28, ShieldItem_construct>(count); }
// 0x56e120 (slot 1)
static void *THISCALL ShieldItem_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, ShieldItem_destroy>(self, flags); }
// 0x56e200. eax: ClassInfo_AddField's.
static DWORD __cdecl ShieldItem_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610318) /* "Damage Type" */, 0x20, 4, 0x20, glob_addr(0x6bd760), 1, glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    add_field(ShieldItem_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x621984) /* "Shield Gfx" */, 0x24, 4, 0x24, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(ShieldItem_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x614e3c) /* "AI" */, 0x1c, 4, 0x1c, ShieldAI_ClassInfo(), 1, ShieldAI_ClassInfo(), 0, 0);
    return add_field(ShieldItem_ClassInfo(), f);
}
// 0x56e410 (fastcall part, damage type): the part's CShieldInventoryItem of that type (searched in the
// category "Shield"), 0 if none
static BYTE *__fastcall ShieldItem_FindOfType(BYTE *part, DWORD type)
{
    BYTE *inv = part_inventory(part);
    const DWORD n = inventory_count_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    DWORD i = inventory_first_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    for (DWORD k = 0; k < n; k++) {
        BYTE *item = inventory_item(inv, i);
        if (item) {
            CClassInfo *c = ShieldItem_ClassInfo();
            if (is_kind_of(item, c) && dw(item, 0x20) == type)
                return item;
        }
        i = inventory_next_of(inv, i, category_index(glob_addr(TEXT_SHIELD)));
    }
    return nullptr;
}
// The "Energy" of an item's definition below this item's
static bool weaker(BYTE *other, BYTE *self) { return c0(fld(fl<float>(item_definition(other), 0x4c)), fld(fl<float>(item_definition(self), 0x4c))); }
// 0x56e3b0 (slot 32, may the part take it: part): a character without a shield of the type or with a
// weaker one (the definitions' "Energy"). al.
static DWORD THISCALL ShieldItem_MayTake(BYTE *self, BYTE *part)
{
    if (!part || !is_kind_of(part, entity_character_class()))
        return 0;
    BYTE *other = ShieldItem_FindOfType(part, dw(self, 0x20));
    if (!other)
        return 1;
    return weaker(other, self) ? 1 : 0;
}
// 0x56e520 (slot 33, taken: taker, part): a character gets a copy of the item's "AI" unless it has a
// CShieldAI of the type, loses a weaker shield of the type; the charge is kept at most the most charge
// (slot 49); then CInventoryItem::Take (its eax)
static DWORD THISCALL ShieldItem_Take(BYTE *self, BYTE *taker, BYTE *part)
{
    if (part && is_kind_of(part, entity_character_class())) {
        const DWORD n = dw(part, 0x40);
        BYTE found = 0;
        for (DWORD k = 0; k < n; k++) {
            BYTE *ai = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4fba60)(part, k);   // CLayerPart_AIAt
            if (ai && is_kind_of(ai, class_at(fn_addr(0x56d460)) /* CShieldAI */) && dw(ai, 0x14) == dw(self, 0x20))
                found = 1;
        }
        if (!found) {
            void *copy = fn<void *(THISCALL *)(void *, DWORD, int)>(0x416760)(bp(dw(self, 0x1c)), 0, -1);   // Object_Clone
            fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(part, copy);                                      // CLayerPart_AddAI
        }
        BYTE *other = ShieldItem_FindOfType(part, dw(self, 0x20));
        if (other && weaker(other, self))
            fn<void(THISCALL *)(void *, void *)>(0x426260)(part_inventory(part), other);   // CInventory_Remove
    }
    if (c0(max_charge(self), fl<float>(self, 0x18))) {
        const float m = fstp(max_charge(self));
        vcall<void>(self, 49, float_bits(m));
    }
    return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x42a550)(self, taker, part);
}

// ---- CShieldChargeInventoryItem -------------------------------------------------------------------------------

// 0x56f4a0 (static registration), 0x56f630 (slot 2)
static CClassInfo *__cdecl ShieldCharge_ClassInfo()
{
    return class_getter(0x71352c, 0x6219ec /* "CShieldChargeInventoryItem" */, 1.0f, energy_item_class, 0x1c, 0, 0x56f4f0, 0x56f530, 0x56f690);
}
static void ShieldCharge_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_ENERGY_ITEM));
    vtable_and_defaults(o, vt(VT_SHIELD_CHARGE));
}
static void ShieldCharge_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SHIELD_CHARGE));
    energy_item_dtor(o);
}
// 0x56f4f0
static void *__cdecl ShieldCharge_Create() { return create_one<0x1c, ShieldCharge_construct>(); }
// 0x56f530
static void *__fastcall ShieldCharge_CreateArray(int count, int, int) { return create_array<0x1c, ShieldCharge_construct>(count); }
// 0x56f5a0 (slot 1)
static void *THISCALL ShieldCharge_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, ShieldCharge_destroy>(self, flags); }
// 0x56f690: the inherited "Current Charge" becomes "Charge to Add" in "MJ" with +0x34 = 10. eax:
// FieldDesc_SetUnits'.
static DWORD __cdecl ShieldCharge_RegisterFields()
{
    void *d = find_field(ShieldCharge_ClassInfo(), glob_addr(0x621a18) /* "Current Charge" */, 0);
    set_name(d, glob_addr(0x621a08) /* "Charge to Add" */);
    const DWORD r = set_units(d, glob_addr(0x6120dc) /* "MJ" */);
    dw(d, 0x34) = 0x41200000;   // 10.0
    return r;
}
// The character's CShieldInventoryItem (CInventory_FindOfClass; the class first, then the inventory)
static BYTE *character_shield(BYTE *part)
{
    CClassInfo *c = ShieldItem_ClassInfo();
    return inventory_find_of_class(part_inventory(part), c);
}
// 0x56f710 (slot 32, may the part take it: part): a character whose shield is below its most charge. al.
static DWORD THISCALL ShieldCharge_MayTake(BYTE *, BYTE *part)
{
    if (!part || !is_kind_of(part, entity_character_class()))
        return 0;
    BYTE *shield = character_shield(part);
    if (!shield)
        return 0;
    return c03(max_charge(shield), fl<float>(shield, 0x18)) ? 0 : 1;
}
// 0x56f7c0 (slot 33, taken: taker, part): the character's shield gets "Charge to Add" (a player's scaled
// by the world settings' +0x24). eax: the shield, 0 if none.
static DWORD THISCALL ShieldCharge_Take(BYTE *self, DWORD, BYTE *part)
{
    if (!part || !is_kind_of(part, entity_character_class()))
        return 0;
    BYTE *shield = character_shield(part);
    if (!shield)
        return 0;
    float amount = fl<float>(self, 0x18);
    if (*glob<DWORD *>(CATEGORY_PLAYER) & dw(part, 0x1c))
        amount = fstp(fld(fl<float>(world_settings(), 0x24)) * fld(amount));
    energy_add(shield, float_bits(amount));
    return ptr_value(shield);
}

// 0x56f8a0: a heap pointer array (vtable 0x5ffd24) constructed empty. eax: this.
static BYTE *THISCALL HeapPointerArray_Ctor(BYTE *a)
{
    dw(a, 0) = vt(0x5ffd24);
    dw(a, 4) = 0;
    dw(a, 8) = 0;
    dw(a, 0xc) = 0;
    return a;
}

// ---- CRechargeShieldAction --------------------------------------------------------------------------------------

// 0x56f910 (static registration), 0x56fab0 (slot 2)
static CClassInfo *__cdecl RechargeShield_ClassInfo()
{
    return class_getter(0x713534, 0x621a28 /* "CRechargeShieldAction" */, 3.0f, action_class, 0x18, 0, 0x56f960, 0x56f9a0, 0x56fb10);
}
static void RechargeShield_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);   // "Target"
    vtable_and_defaults(o, vt(VT_RECHARGE));
}
static void RechargeShield_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_RECHARGE));
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x56f960
static void *__cdecl RechargeShield_Create() { return create_one<0x18, RechargeShield_construct>(); }
// 0x56f9a0
static void *__fastcall RechargeShield_CreateArray(int count, int, int) { return create_array<0x18, RechargeShield_construct>(count); }
// 0x56fa10 (slot 1)
static void *THISCALL RechargeShield_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, RechargeShield_destroy>(self, flags); }
// 0x56fb10. eax: ClassInfo_AddField's.
static DWORD __cdecl RechargeShield_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(RechargeShield_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610318) /* "Damage Type" */, 0x14, 4, 0x14, glob_addr(0x6bd760), 1, glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    add_field(RechargeShield_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x621a50) /* "Recharge Percent" */, 0xc, 4, 0xc, 1, 0x3f800000 /* 1.0 */, 0, 0, 0x42c80000 /* 100.0 */,
                  0x3f800000 /* 1.0 */);
    add_field(RechargeShield_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x621a40) /* "Recharge Uints" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0, 0);
    return add_field(RechargeShield_ClassInfo(), f);
}
// 0x56fd40 (slot 27: a1, a2, a3, target): the first CShieldInventoryItem of the damage type in the
// character's "Shield" category that is below its most charge gets "Recharge Uints", or "Recharge Percent"
// of its most charge when that is 0 (a player's scaled by the world settings' +0x24). al: one was charged.
static DWORD THISCALL RechargeShield_Target(BYTE *self, DWORD, DWORD, DWORD, BYTE *target)
{
    if (!target || !is_kind_of(target, entity_character_class()))
        return 0;
    BYTE *inv = part_inventory(target);
    const DWORD n = inventory_count_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    DWORD i = inventory_first_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    for (DWORD k = 0; k < n; k++) {
        BYTE *item = inventory_item(inv, i);
        if (item && is_kind_of(item, class_at(fn_addr(0x56e010)) /* CShieldInventoryItem */) && dw(item, 0x20) == dw(self, 0x14) &&
            !c03(max_charge(item), fl<float>(item, 0x18))) {
            float amount = fstp(fld(fl<float>(self, 0x10)));
            if (c3(fld(amount), cst(glob_addr(C_ZERO))))
                amount = fstp(max_charge(item) * fld(fl<float>(self, 0xc)) * cst(glob_addr(C_EPS2)));
            if (dw(target, 0x1c) & *glob<DWORD *>(CATEGORY_PLAYER))
                amount = fstp(fld(fl<float>(world_settings(), 0x24)) * fld(amount));
            energy_add(item, float_bits(amount));
            return 1;
        }
        i = inventory_next_of(inv, i, category_index(glob_addr(TEXT_SHIELD)));
    }
    return 0;
}

ZAX_HOOKS(ai_shield)
{
    const Hook list[] = {
#include "ai_shield_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
