// The inventory item actions 0x4dc1f0-0x4dd2c0, CInventoryItemBase and the item reference helpers
// 0x4dd2d0-0x4dd6c0, the item catalogue's slot 35 0x4d82e0 and the abstract AI classes CInvestigateAI
// and CInvestigateAreaAI 0x4dd6d0-0x4dd883 (phase 3).
//
// Layouts [confirmed: constructors, getters, docs/fields.csv]:
//   CInventoryItemAction (8 bytes, abstract, version 0, base vtable 0x5f1210, class 0x6c0d7c; derives from
//     CAction 0x41a6b0/0x41a700, RegisterFields CAction's 0x58ea40): slot 23 (execute, 0x4dc2a0) passes the
//     part's item in focus to slot 28 (layer, part, inventory, item, a3, a4). Its four classes (8 bytes,
//     version 0, no fields of their own):
//       CDropCurrentInventoryItemAction (vtable 0x5f8340, class 0x6c0d80): the item dropped as a pickup
//       COldBadUseCurrentInventoryItemAction (0x5f845c, 0x6c0d84): heals with a "Health" item, otherwise
//         changes to a "Primary"/"Secondary" item
//       CSelectCurrentInventoryItemAction (0x5f84d0, 0x6c0d88): changes to the item (the use action the
//         item definitions give "Primary"/"Secondary" items, section 8)
//       CHealTriggerFromCurrentInventoryItemAction (0x5f8544, 0x6c0d8c): heals with the item ("Health")
//   CInventoryItemBase (0xc, abstract, version 1, vtable 0x5f85b8, class 0x6c0d94; CObject): +8
//     "Definition Index" (an index into the item catalogue 0x6c0c08)
//   CInvestigateAI (0x10, abstract, vtable 0x5efa04, class 0x6c0d9c) and CInvestigateAreaAI (0x10,
//     abstract, 0x5f861c, 0x6c0da4): CLayerPartAI (0x4fd220) children without fields of their own; only
//     their getters and destructors live here.
//
// Return values: eax as the original leaves it; where only al is set (the slot 28s, slot 35), al with
// the upper bytes of the call before; the checks compare al there.
#include "class_info.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD is_kind_of(const void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
template <DWORD A> static CClassInfo *__cdecl getter() { return fn<Getter>(A)(); }
static void action_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static DWORD action_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x41a700)(o); }
static void ai_base_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x4fd060)(o); }
static DWORD ai_base_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4fd080)(o); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
static DWORD list_load(DWORD list, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), i); }
static DWORD resource_find(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), at<const char *>(name), -1); }
static DWORD report(DWORD text) { return vcall<DWORD>(*glob<void **>(0x6c02cc), 17, at<const char *>(text)); }
static BYTE *inventory_of(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(part); }
static DWORD inv_item_in_focus(void *inv) { return fn<DWORD(THISCALL *)(void *)>(0x426070)(inv); }
static BYTE *inv_item_at(void *inv, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x424f60)(inv, i); }
static DWORD inv_index_of(void *inv, void *item) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4262a0)(inv, item); }
static DWORD inv_remove(void *inv, void *item) { return fn<DWORD(THISCALL *)(void *, void *)>(0x426260)(inv, item); }
static DWORD inv_change_to(void *inv, DWORD index, DWORD category, void *part, DWORD mode)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, void *, DWORD)>(0x425590)(inv, index, category, part, mode);
}
static long double hit_points_left(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d030)(e); }
static long double hit_points(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d150)(e); }
static BYTE *power_up_model(void *def) { return fn<BYTE *(THISCALL *)(void *)>(0x5b76d0)(def); }
static BYTE *default_item(void *def) { return fn<BYTE *(THISCALL *)(void *)>(0x5b7aa0)(def); }
static BYTE *entity_from_model(void *model, CClassInfo *cls)
{
    return fn<BYTE *(THISCALL *)(void *, void *, CClassInfo *, DWORD, DWORD, DWORD)>(0x5176f0)(glob<void *>(0x6cfdd8), model, cls, 0, 0,
                                                                                               0xffffffff);
}
static void override_set_item(void *ai, void *item) { fn<void(THISCALL *)(void *, void *)>(0x53efe0)(ai, item); }
static BYTE *pickup_item(void *ai, void *part) { return fn<BYTE *(THISCALL *)(void *, void *)>(0x53e4c0)(ai, part); }
static BYTE *pickup_ai_ctor(void *o) { return fn<BYTE *(THISCALL *)(void *)>(0x53d8f0)(o); }
static void part_add_ai(void *p, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(p, ai); }
static void place_near(void *layer, void *part, void *ref, DWORD dist)
{
    fn<DWORD(THISCALL *)(void *, void *, void *, DWORD)>(0x4eb7b0)(layer, part, ref, dist);
}

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

constexpr DWORD CATALOGUE = 0x6c0c08;    // "Inventory Item Definitions"
constexpr DWORD CATEGORIES = 0x6c0800;   // "Inventory Item Types"
constexpr DWORD ITEM_ACTION_VTABLE = 0x5f1210;
constexpr DWORD AI_BASE_VTABLE = 0x5ed1d0;
constexpr DWORD OVERRIDE_PICKUP_VTABLE = 0x5f83b4;   // COverridePickupAI (0x38 bytes)
constexpr DWORD FLOAT_0_01 = 0x3c23d70a;
constexpr DWORD FLOAT_40 = 0x42200000;
constexpr DWORD FLOAT_500 = 0x43fa0000;
// Class getters of other modules
constexpr DWORD G_OBJECT = 0x4163b0, G_ACTION = 0x41a650, G_LAYER_PART_AI = 0x4fd220, G_ENTITY_BASE = 0x487620,
                G_ENTITY_DESTRUCTABLE = 0x48bdf0, G_ENTITY_CHARACTER = 0x49a930, G_PICKUP_AI = 0x53d190,
                G_OVERRIDE_PICKUP_AI = 0x53e850, G_HEALTH_ITEM = 0x5aefe0, G_ZAX_ITEM_DEFINITION = 0x5b5f10;

// A definition list's records (+0x30, 0x80 bytes each), their count (+0x34) and folder (+0x38)
struct Record {
    char name[0x78];
    void *object;
    BYTE keep, in_use, b7e, loading;
};
static Record *&records_of(DWORD list) { return *at<Record **>(list + 0x30); }
static DWORD count_of(DWORD list) { return *at<DWORD *>(list + 0x34); }

// Record i's object (0 without records or while it is loading), loaded inline as 0x4dd480 does: its
// file from the list's folder and the record's name (slot 1) through slot 37
static BYTE *load_inline(DWORD list, DWORD i)
{
    Record *&data = records_of(list);
    if (!data)
        return nullptr;
    if (i >= count_of(list))
        i = 0;
    if (data[i].loading)
        return nullptr;
    if (!data[i].object && data && !data[i].loading) {
        data[i].loading = 1;
        char path[0x100];
        path_join(path, at<const char *>(list + 0x38), vcall<const char *>(at<void *>(list), 1, i));
        char *p = path_name(path);
        void *o;
        if (p == path)
            o = vcall<void *>(at<void *>(list), 37, glob<const char *>(0x62e558), p, &data[i].object);
        else {
            p[-1] = 0;
            o = vcall<void *>(at<void *>(list), 37, path, p, &data[i].object);
        }
        data[i].object = o;
        data[i].loading = 0;
    }
    return static_cast<BYTE *>(data[i].object);
}

// ---- The shared pattern ---------------------------------------------------------------------------------

using Construct = void (*)(BYTE *);
using Destroy = DWORD (*)(BYTE *);

template <DWORD SIZE, Construct C> static void *__cdecl create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}
template <DWORD SIZE, Construct C> static void *__fastcall create_array(int count, int, int)
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
template <DWORD SIZE, Destroy D> static void *THISCALL deleting_dtor(BYTE *self, DWORD flags)
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
static CClassInfo *class_getter(DWORD var, DWORD name, float version, Getter parent, DWORD size, DWORD abstract, DWORD create,
                                DWORD create_array, DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, parent ? parent() : nullptr, size, abstract, create,
                                     create_array, register_fields);
    return v;
}
template <typename... A> static void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}

// The four action classes: CAction's constructor, the base vtable, InitFields, the own vtable, InitFields
// (and the reverse in the destructor)
template <DWORD VT> static void action_construct(BYTE *o)
{
    action_ctor(o);
    dw(o, 0) = vt(ITEM_ACTION_VTABLE);
    obj_init_fields(o);
    dw(o, 0) = vt(VT);
    obj_init_fields(o);
}
template <DWORD VT> static DWORD action_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT);
    obj_free_fields(o);
    dw(o, 0) = vt(ITEM_ACTION_VTABLE);
    obj_free_fields(o);
    return action_dtor(o);
}

// ---- CInventoryItemAction ----------------------------------------------------------------------------

// 0x4dc1f0, 0x4dc240 (slot 2)
static CClassInfo *__cdecl ItemAction_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d7c), glob_addr(0x616f68) /* "CInventoryItemAction" */, 0.0f, getter<G_ACTION>, 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}

// 0x4dc2a0 (slot 23, execute): the part's inventory and its item in focus; an item not marked for
// deletion goes to slot 28 (layer, part, inventory, item, a3, a4). eax: slot 28's, otherwise al 0
// with the upper bytes of the call before.
static DWORD THISCALL ItemAction_Execute(void *self, DWORD layer, void *part, DWORD a3, DWORD a4)
{
    BYTE *inv = inventory_of(part);
    if (!inv)
        return 0;
    BYTE *item = inv_item_at(inv, inv_item_in_focus(inv));
    if (!item)
        return 0;
    if (by(item, 0x11))
        return with_al(ptrv(item), 0);
    return vcall<DWORD>(self, 28, layer, part, inv, item, a3, a4);
}

// ---- CDropCurrentInventoryItemAction -------------------------------------------------------------------

// 0x4dc300, 0x4dc4e0 (slot 2)
static CClassInfo *__cdecl Drop_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d80), glob_addr(0x616f80) /* "CDropCurrentInventoryItemAction" */, 0.0f, getter<0x4dc1f0>, 8, 0, fn_addr(0x4dc3a0), fn_addr(0x4dc3e0),
                        fn_addr(0x58ea40));
}

// A new COverridePickupAI (0x38 bytes, CAIBase's constructor and both vtables), 0 when the pool has none
static BYTE *new_override_pickup()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x38));
    if (!o)
        return nullptr;
    ai_base_ctor(o);
    dw(o, 0) = vt(AI_BASE_VTABLE);
    obj_init_fields(o);
    dw(o, 0) = vt(OVERRIDE_PICKUP_VTABLE);
    obj_init_fields(o);
    return o;
}
// An AI of the class on the entity (slot 64 with (class, 0)) if it is one of that class, else 0
static BYTE *ai_of(BYTE *entity, DWORD class_getter_address)
{
    BYTE *a = vcall<BYTE *>(entity, 64, class_at(class_getter_address), (DWORD)0);
    if (a && (is_kind_of(a, class_at(class_getter_address)) & 0xff))
        return a;
    return nullptr;
}

// 0x4dc590 (slot 28): drops the item as a pickup. For a CZaxInventoryItemDefinition the definition's
// power-up model becomes an entity (CEntityBase); its CPickupAI (a new one added when it has none) gets
// a COverridePickupAI (made and added when there is none, slot 19 on a new one) with a clone of the
// item, or of the definition's default item when that has fewer rounds (slot 41) than the item; a
// clone that is selected (+0x10) is deselected first (slot 45 with 0) on the new pickup path. An
// override that holds an item already is reported ("DefaultEntity already has …" on an entity that had
// its own CPickupAI, "The Entity already has …" otherwise; an override without a CPickupAI is reported
// too). Then the item leaves the inventory (all rounds: CInventory_Remove) or gives up the pickup's
// rounds (slot 52 with (0, n)), the entity is placed near the part (500 pixels) and the inventory
// updates (slot 24 with the part if it is a character, 0.01). al: 1, 0 without a Zax definition.
static DWORD THISCALL Drop_Use(void *, void *layer, BYTE *part, BYTE *inv, BYTE *item, DWORD, DWORD)
{
    BYTE *def = fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item);
    if (!def)
        return 0;
    const DWORD k = is_kind_of(def, class_at(fn_addr(G_ZAX_ITEM_DEFINITION)));
    if (!(k & 0xff))
        return k;
    CClassInfo *const base = class_at(fn_addr(G_ENTITY_BASE));   // before the model, as the original pushes them
    BYTE *entity = entity_from_model(power_up_model(def), base);
    BYTE *pickup = ai_of(entity, fn_addr(G_PICKUP_AI));
    if (!entity)
        return 0;
    BYTE *src = default_item(def);
    const int n_item = vcall<int>(item, 41);
    BYTE *chosen = vcall<int>(src, 41) >= n_item ? item : default_item(def);
    bool had_override = true;
    BYTE *over;
    if (pickup) {
        over = ai_of(entity, fn_addr(G_OVERRIDE_PICKUP_AI));
        if (!over) {
            had_override = false;
            over = new_override_pickup();
            vcall<DWORD>(over, 19);
        }
        if (dw(over, 0x10))
            report(glob_addr(0x617054));   // "Error: DefaultEntity already has a COverridePickupAI with an Inventory item on it! …"
        else
            override_set_item(over, object_clone(chosen));
    } else {
        BYTE *block = static_cast<BYTE *>(pool_alloc(0x30));
        pickup = block ? pickup_ai_ctor(block) : nullptr;
        vcall<DWORD>(pickup, 19);
        part_add_ai(entity, pickup);
        over = ai_of(entity, fn_addr(G_OVERRIDE_PICKUP_AI));
        if (over)
            report(glob_addr(0x616ff8));   // "Error: You have a COverridePickupAI on an entity that doesn't have a CPickupAI!!! …"
        else {
            had_override = false;
            over = new_override_pickup();
            vcall<DWORD>(over, 19);
        }
        if (dw(over, 0x10))
            report(glob_addr(0x616fa0));   // "Error: The Entity already has a COverridePickupAI with an Inventory item on it! …"
        else {
            BYTE *clone = static_cast<BYTE *>(object_clone(chosen));
            if (by(clone, 0x10))
                vcall<DWORD>(clone, 45, (DWORD)0);
            override_set_item(over, clone);
        }
    }
    if (!had_override)
        part_add_ai(entity, over);
    src = default_item(def);
    const int n_item2 = vcall<int>(item, 41);
    if (vcall<int>(src, 41) >= n_item2)
        inv_remove(inv, item);
    else {
        BYTE *given = pickup_item(pickup, entity);
        vcall<DWORD>(item, 52, (DWORD)0, vcall<DWORD>(given, 41));
    }
    place_near(layer, entity, part, FLOAT_500);
    BYTE *ch = part && (is_kind_of(part, class_at(fn_addr(G_ENTITY_CHARACTER))) & 0xff) ? part : nullptr;
    return with_al(vcall<DWORD>(inv, 24, ch, FLOAT_0_01), 1);
}

// ---- COldBadUseCurrentInventoryItemAction and its two successors --------------------------------------

// 0x4dc8c0, 0x4dcaa0 (slot 2)
static CClassInfo *__cdecl OldBadUse_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d84), glob_addr(0x6170b0) /* "COldBadUseCurrentInventoryItemAction" */, 0.0f, getter<0x4dc1f0>, 8, 0, fn_addr(0x4dc960), fn_addr(0x4dc9a0),
                        fn_addr(0x58ea40));
}
// 0x4dcca0, 0x4dce80 (slot 2)
static CClassInfo *__cdecl Select_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d88), glob_addr(0x617118) /* "CSelectCurrentInventoryItemAction" */, 0.0f, getter<0x4dc1f0>, 8, 0, fn_addr(0x4dcd40), fn_addr(0x4dcd80),
                        fn_addr(0x58ea40));
}
// 0x4dcf90, 0x4dd170 (slot 2)
static CClassInfo *__cdecl HealTrigger_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d8c), glob_addr(0x61713c) /* "CHealTriggerFromCurrentInventoryItemAction" */, 0.0f, getter<0x4dc1f0>, 8, 0, fn_addr(0x4dd030),
                        fn_addr(0x4dd070), fn_addr(0x58ea40));
}

// Healing a destructable part that lacks hit points with the item: a CHealthItem gives its +0x24
// (slot 53 with the part takes it), an old item 40 after the report "Using old-style health item…"
// (the item removed from the inventory); the part's slot 98 heals by that. al: 1, 0 when the part
// is none, not destructable or not hurt (NaN counts as not hurt).
static DWORD heal_with(BYTE *part, BYTE *inv, BYTE *item)
{
    if (!part)
        return 0;
    const DWORD k = is_kind_of(part, class_at(fn_addr(G_ENTITY_DESTRUCTABLE)));
    if (!(k & 0xff))
        return k;
    const float left = (float)hit_points_left(part);
    const long double hp = hit_points(part);
    if (!(hp > (long double)left))
        return 0;
    DWORD amount = FLOAT_40;
    if (is_kind_of(item, class_at(fn_addr(G_HEALTH_ITEM))) & 0xff) {
        amount = dw(item, 0x24);
        vcall<DWORD>(item, 53, part);
    } else {
        report(glob_addr(0x6170d8));   // "Using old-style health item... Please Fix... (--Loren)"
        inv_remove(inv, item);
    }
    return with_al(vcall<DWORD>(part, 98, amount), 1);
}
// Changing to the item in its category (ChangeTo with mode 0), then the inventory's update (slot 24
// with the part if it is a character, 0.01). al 1.
static DWORD change_to(BYTE *part, BYTE *inv, BYTE *item, DWORD category)
{
    inv_change_to(inv, inv_index_of(inv, item), category, part, 0);
    BYTE *ch = part && (is_kind_of(part, class_at(fn_addr(G_ENTITY_CHARACTER))) & 0xff) ? part : nullptr;
    return with_al(vcall<DWORD>(inv, 24, ch, FLOAT_0_01), 1);
}

// 0x4dcb50 (slot 28): the item's category ("Health" heals, "Primary"/"Secondary" changes to it). al: 1,
// 0 for another category or when healing does not apply.
static DWORD THISCALL OldBadUse_Use(void *, void *, BYTE *part, BYTE *inv, BYTE *item, DWORD, DWORD)
{
    const DWORD cat = dw(fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item), 0x18);
    if (cat == resource_find(glob_addr(CATEGORIES), glob_addr(0x617110) /* "Health" */))
        return heal_with(part, inv, item);
    DWORD r = resource_find(glob_addr(CATEGORIES), glob_addr(0x60b780) /* "Primary" */);
    if (cat != r) {
        r = resource_find(glob_addr(CATEGORIES), glob_addr(0x60b788) /* "Secondary" */);
        if (cat != r)
            return with_al(r, 0);
    }
    return change_to(part, inv, item, cat);
}
// 0x4dcf30 (slot 28): changes to the item in its category. al 1.
static DWORD THISCALL Select_Use(void *, void *, BYTE *part, BYTE *inv, BYTE *item, DWORD, DWORD)
{
    const DWORD cat = dw(fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item), 0x18);
    return change_to(part, inv, item, cat);
}
// 0x4dd220 (slot 28): heals the part with the item. al: 1 when it healed.
static DWORD THISCALL HealTrigger_Use(void *, void *, BYTE *part, BYTE *inv, BYTE *item, DWORD, DWORD)
{
    return heal_with(part, inv, item);
}

// ---- CInventoryItemBase and the item references -----------------------------------------------------------

// 0x4dd2d0, 0x4dd320 (slot 2)
static CClassInfo *__cdecl ItemBase_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d94), glob_addr(0x617168) /* "CInventoryItemBase" */, 1.0f, getter<G_OBJECT>, 0xc, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x4dd380));
}
// 0x4dd380: "Definition Index" (+8, a resource field of the item catalogue). eax: AddField's.
static DWORD __cdecl ItemBase_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(0x54e560) /* ResourceField_CtorTyped */, glob_addr(0x61717c) /* "Definition Index" */, 8, 4, 8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    return add_field(ItemBase_ClassInfo(), f);
}
// 0x4dd410: CObject's constructor, the vtable, InitFields. eax: this.
static BYTE *THISCALL ItemBase_Ctor(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(0x5f85b8);
    obj_init_fields(o);
    return o;
}
// 0x4dd460: the vtable, FreeFields, CObject's destructor. eax: that destructor's.
static DWORD THISCALL ItemBase_Dtor(BYTE *o)
{
    dw(o, 0) = vt(0x5f85b8);
    obj_free_fields(o);
    return obj_dtor(o);
}
// 0x4dd430 (slot 1): the destructor, the block given back to the pool with flag 1 (no array path). eax: this.
static BYTE *THISCALL ItemBase_DeletingDtor(BYTE *o, DWORD flags)
{
    ItemBase_Dtor(o);
    if (flags & 1)
        pool_free(o, 0xc);
    return o;
}
// 0x4dd480: the item's definition: record +8 of the item catalogue (index 0 when out of range), loaded
// on first use; 0 without records or while the record loads.
static BYTE *THISCALL ItemRef_Definition(BYTE *item) { return load_inline(glob_addr(CATALOGUE), dw(item, 8)); }
// 0x4dd580: the item's category: the definition (loaded through 0x55e090) and its +0x18 in the category
// list 0x6c0800, loaded inline on first use. A definition that is none or loading is read at address 0
// (the game ends there, as in the original).
static BYTE *THISCALL ItemRef_Category(BYTE *item)
{
    BYTE *def = nullptr;
    if (Record *data = records_of(glob_addr(CATALOGUE))) {
        DWORD i = dw(item, 8);
        if (i >= count_of(glob_addr(CATALOGUE)))
            i = 0;
        if (!data[i].loading) {
            if (!data[i].object)
                list_load(glob_addr(CATALOGUE), i);
            def = static_cast<BYTE *>(records_of(glob_addr(CATALOGUE))[i].object);
        }
    }
    return load_inline(glob_addr(CATEGORIES), *reinterpret_cast<volatile DWORD *>(def + 0x18));
}

// 0x4d82e0 (slot 35 of the item catalogue, (object, a2)): for a CInventoryItemDefinition (its getter
// inline) that is one of the list's records, slot 39 with (index, a2). al: 1 then, else 0.
static DWORD THISCALL Catalogue_Slot35(BYTE *list, void *obj, DWORD a2)
{
    CClassInfo *c = class_getter(glob_addr(0x6c0d54), glob_addr(0x615e8c) /* "CInventoryItemDefinition" */, 9.0f, nullptr, 0x60, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x4d5620));
    const DWORD k = is_kind_of(obj, c);
    if (!(k & 0xff))
        return k;
    const DWORD n = dw(list, 0x34);
    if (!n)
        return 0;
    const Record *data = reinterpret_cast<const Record *>(dw(list, 0x30));
    DWORD i = 0;
    for (; i < n; i++)
        if (data[i].object == obj)
            break;
    if (i == n)
        return with_al(n, 0);
    return with_al(vcall<DWORD>(list, 39, i, a2), 1);
}

// ---- CInvestigateAI, CInvestigateAreaAI -------------------------------------------------------------------

// 0x4dd6d0, 0x4dd720 (slot 2)
static CClassInfo *__cdecl Investigate_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d9c), glob_addr(0x617190) /* "CInvestigateAI" */, 1.0f, getter<G_LAYER_PART_AI>, 0x10, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}
// 0x4dd7b0, 0x4dd800 (slot 2)
static CClassInfo *__cdecl InvestigateArea_ClassInfo()
{
    return class_getter(glob_addr(0x6c0da4), glob_addr(0x6171a0) /* "CInvestigateAreaAI" */, 1.0f, getter<G_LAYER_PART_AI>, 0x10, 1, fn_addr(0x46b760), fn_addr(0x52dc80),
                        fn_addr(0x58ea40));
}
// 0x4dd780, 0x4dd860: the own vtable, FreeFields, CAIBase's vtable, FreeFields, CAIBase's destructor.
// eax: that destructor's.
template <DWORD VT> static DWORD THISCALL ai_dtor(BYTE *o)
{
    dw(o, 0) = vt(VT);
    obj_free_fields(o);
    dw(o, 0) = vt(AI_BASE_VTABLE);
    obj_free_fields(o);
    return ai_base_dtor(o);
}

ZAX_HOOKS(inv_actions)
{
    const Hook list[] = {
#include "inv_actions_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
