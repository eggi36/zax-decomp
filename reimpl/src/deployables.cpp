// The deployable inventory items 0x479c40-0x47bbc0 (phase 3, inventory and weapons): an item that is
// put down as an entity (a mine, a turret) and picked up again. CDeployableInventoryItem (abstract),
// CUndeployedInventoryItem (in the inventory) and CDeployedInventoryItem (the deployed entity's item), the
// actions CDeployCurrentInventoryItemAction and CRecallCurrentInventoryItemAction with their base
// CInventoryItemAction's deleting destructor, and CPickupDeployedItemAI (the deployed entity's pickup)
// (docs/DECOMPILATION.md section 8).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CDeployableInventoryItem (0x18, vtable 0x5f0ee0, class 0x6bda9c, abstract, parent CInventoryItem; no
//     fields): the definition's "Ammunition" +0x2c names the other form's definition (the undeployed
//     item's names the deployed one and back), its projectile +0x20 the entity that is deployed
//   CUndeployedInventoryItem (0x1c, vtable 0x5f0e18, class 0x6bdaa0): +0x18 "Debug/Current Damage" (the
//     share of hit points the deployed entity had lost)
//   CDeployedInventoryItem (0x20, vtable 0x5f0fa8, class 0x6bdaa4): +0x18 "Debug/Deployed Entity" (a
//     CEntityDestructable), +0x1c "Debug/Owner" (the part that deployed it); it listens to both
//   CDeployCurrentInventoryItemAction / CRecallCurrentInventoryItemAction (8, vtables 0x5f119c / 0x5f1284,
//     classes 0x6bdaac / 0x6bdab0, parent CInventoryItemAction 0x4dc1f0 with vtable 0x5f1210)
//   CPickupDeployedItemAI (0x30, vtable 0x5f12f8, class 0x6bdab4, parent CPickupAI)
//
// Left out: CUndeployedInventoryItem's slot 49 0x479e00 (`fld [ecx+0x18]; ret`, 4 bytes, not hookable),
// the static ZStrings of the messages (0x479940-0x479c30) and the registration stubs, the detail renderer
// CDeployableInventoryDetailRenderer 0x47ac80-0x47b150 (Rendering) and CInventoryDetailRenderer's
// 0x47ac40/0x47ac50.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD CATALOGUE = 0x6c0c08, CAT_RECORDS = 0x6c0c38, CAT_COUNT = 0x6c0c3c;
constexpr DWORD VT_DEPLOYABLE = 0x5f0ee0, VT_UNDEPLOYED = 0x5f0e18, VT_DEPLOYED = 0x5f0fa8, VT_ITEM_ACTION = 0x5f1210,
                VT_DEPLOY_ACTION = 0x5f119c, VT_RECALL_ACTION = 0x5f1284, VT_PICKUP_DEPLOYED = 0x5f12f8;

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
BYTE *pt(void *p, DWORD off) { return bp(dw(p, off)); }
CClassInfo *character_class() { return fn<Getter>(0x49a930)(); }
CClassInfo *destructable_class() { return fn<Getter>(0x48bdf0)(); }
BYTE *inventory_of(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(part); }
BYTE *definition_of(void *item) { return fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item); }
BYTE *def_list_object(DWORD index) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(CATALOGUE), index); }
BYTE *projectile_of(void *def) { return fn<BYTE *(THISCALL *)(void *)>(0x4d61f0)(def); }
int inventory_index_of(void *inv, void *item) { return fn<int(THISCALL *)(void *, void *)>(0x4262a0)(inv, item); }
void report_string(void *static_text) { vcall<void>(*glob<void **>(G_REPORTER), 17, zs_c_str(static_text)); }
void report(GlobKey static_text) { report_string(glob<void *>(static_text)); }
void add_listener(void *part, void *listener) { fn<void(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, listener, -1, glob<const char *>(TEXT_EMPTY)); }
DWORD remove_listener(void *part, void *listener) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, listener, -1, glob<const char *>(TEXT_EMPTY)); }
void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffffu); }
long double hit_points(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d150)(e); }
long double hit_points_left(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d030)(e); }
// The share of its hit points an entity has lost: (hit points - left) / hit points, as the slots compute
// it (the two intermediate values stored as floats)
long double damage_share(void *e)
{
    float t = fstp(hit_points(e));
    t = fstp(fld(t) - hit_points_left(e));
    return fld(t) / hit_points(e);
}
void item_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x42a2b0)(o); }
void item_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x42a300)(o); }

// ---- CDeployableInventoryItem ---------------------------------------------------------------------------

// 0x479c40, 0x479c90 (copies)
CClassInfo *__cdecl Deployable_ClassInfo()
{
    return class_getter(0x6bda9c, 0x610df0 /* "CDeployableInventoryItem" */, 0.0f, getter<0x429e50>, 0x18, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x479d30, 0x479f60 (slot 2)
CClassInfo *__cdecl Undeployed_ClassInfo()
{
    return class_getter(0x6bdaa0, 0x610e0c /* "CUndeployedInventoryItem" */, 0.0f, getter<0x479c40>, 0x1c, 0, 0x479e10, 0x479e50, 0x47a010);
}
// 0x47a410, 0x47a650 (slot 2)
CClassInfo *__cdecl Deployed_ClassInfo()
{
    return class_getter(0x6bdaa4, 0x610e40 /* "CDeployedInventoryItem" */, 0.0f, getter<0x479c40>, 0x20, 0, 0x47a4b0, 0x47a4f0, 0x47a700);
}
CClassInfo *undeployed_class() { return fn<Getter>(0x479f60)(); }
CClassInfo *deployed_class() { return fn<Getter>(0x47a650)(); }

// 0x479cf0 (slot 46, the same kind of item): the same definition (CInventoryItem's slot 46), or the other
// item's definition is this one's "Ammunition" (the other form). eax 1 or 0.
DWORD THISCALL Deployable_SameKind(BYTE *self, BYTE *other)
{
    if (fn<DWORD(THISCALL *)(void *, void *)>(0x42a390)(self, other) & 0xff)
        return 1;
    return dw(definition_of(self), 0x2c) == dw(other, 8) ? 1 : 0;
}
// 0x479dd0: CDeployableInventoryItem's deleting destructor (no array path; the pool block of 0x18)
void *THISCALL Deployable_DeletingDtor(BYTE *self, DWORD flags)
{
    vtable_and_free(self, vt(VT_DEPLOYABLE));
    item_dtor(self);
    if (flags & 1)
        pool_free(self, 0x18);
    return self;
}

// ---- CUndeployedInventoryItem ---------------------------------------------------------------------------

void Undeployed_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_DEPLOYABLE));
    vtable_and_defaults(o, vt(VT_UNDEPLOYED));
}
void Undeployed_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_UNDEPLOYED));
    vtable_and_free(o, vt(VT_DEPLOYABLE));
    item_dtor(o);
}
// 0x479e10, 0x479e50, 0x479ec0 (slot 1)
void *__cdecl Undeployed_Create() { return create_one<0x1c, Undeployed_construct>(); }
void *__fastcall Undeployed_CreateArray(int count, int, int) { return create_array<0x1c, Undeployed_construct>(count); }
void *THISCALL Undeployed_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, Undeployed_destroy>(self, flags); }
// 0x47a010. eax: ClassInfo_AddField's.
DWORD __cdecl Undeployed_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x610e28) /* "Debug/Current Damage" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0, 0);
    return add_field(Undeployed_ClassInfo(), f);
}
// 0x47a0f0 (part): the item is deployed. Checks first, each reporting a static message and giving al 0:
// the "Ammunition" definition must name this one back (0x6bda7c) and hold a CDeployedInventoryItem as its
// default item (0x6bda5c); the part must have an inventory (0x6bda80) that holds this item (0x6bda54);
// the projectile's default entity must be a CEntityDestructable (0x6bda48). The entity is made from the
// projectile model (Entity_CreateFromModel, the part as creator) next to the part (Entity_PlaceNear,
// 500 pixels), takes "Current Damage" of its hit points as "Unavoidable" damage, gets slot 33 when it
// has the flag 0x800000; a clone of the deployed item (its definition index the "Ammunition", slot 7) is
// taken by the part (slot 33) and listens to the entity and the part (its +0x18/+0x1c); the clone
// template is deleted. This item is marked for deletion. al 1.
DWORD THISCALL Undeployed_Deploy(BYTE *self, BYTE *part)
{
    BYTE *inv = inventory_of(part);
    if (dw(definition_of(self), 0x2c)) {
        BYTE *ammo = def_list_object(dw(definition_of(self), 0x2c));
        if (dw(ammo, 0x2c) != dw(self, 8)) {
            report(0x6bda7c);
            return 0;
        }
    }
    if (dw(definition_of(self), 0x2c)) {
        if (!dw(def_list_object(dw(definition_of(self), 0x2c)), 0x1c)) {
            report(0x6bda5c);
            return 0;
        }
        CClassInfo *c = deployed_class();
        if (!(is_kind_of(pt(def_list_object(dw(definition_of(self), 0x2c)), 0x1c), c))) {
            report(0x6bda5c);
            return 0;
        }
    }
    if (dw(definition_of(self), 0x2c) && !inv) {
        report(0x6bda80);
        return 0;
    }
    if (dw(definition_of(self), 0x2c) && inventory_index_of(inv, self) == -1) {
        report(0x6bda54);
        return 0;
    }
    if (!dw(projectile_of(definition_of(self)), 0x74) || !is_kind_of(pt(projectile_of(definition_of(self)), 0x74), destructable_class())) {
        report(0x6bda48);
        return 0;
    }
    const DWORD model = dw(definition_of(self), 0x20);
    using CreateFromModel = BYTE *(THISCALL *)(void *, DWORD, CClassInfo *, void *, DWORD, DWORD);
    BYTE *e = fn<CreateFromModel>(0x5176f0)(glob<void *>(0x6cfdd8), model, destructable_class(), part, 0, 0xffffffffu);
    fn<DWORD(THISCALL *)(void *, void *, void *, DWORD)>(0x4eb7b0)(pt(part, 0x48), e, part, 0x43fa0000);
    if (!c03(hit_points(e), cst(glob_addr(C_ZERO)))) {
        const DWORD type = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6bd760), glob<const char *>(0x610324), -1);
        vcall<void>(e, 99, fstp(hit_points(e) * fld(fl<float>(self, 0x18))), type, 0);
    }
    if (dw(e, 0x1c) & 0x800000)
        vcall<void>(e, 33);
    if (dw(definition_of(self), 0x2c)) {
        DWORD index = dw(definition_of(self), 0x2c);
        BYTE *d = nullptr;
        if (BYTE *records = pt(glob<void *>(CAT_RECORDS), 0)) {
            if (index >= *glob<DWORD *>(CAT_COUNT))
                index = 0;
            if (!records[index * 0x80 + 0x7f]) {
                if (!dw(records + index * 0x80, 0x78))
                    fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(CATALOGUE), index);
                d = pt(pt(glob<void *>(CAT_RECORDS), 0) + index * 0x80, 0x78);
            }
        }
        BYTE *c = static_cast<BYTE *>(object_clone(pt(d, 0x1c)));
        fn<void(THISCALL *)(void *, DWORD)>(0x54fdb0)(c, dw(definition_of(self), 0x2c));
        vcall<void>(c, 7);
        BYTE *taken = vcall<BYTE *>(c, 33, 0, part);
        dw(taken, 0x18) = ptrv(e);
        add_listener(e, taken);
        dw(taken, 0x1c) = ptrv(part);
        add_listener(part, taken);
        if (c)
            vcall<void *>(c, 1, 1);
    }
    fl<BYTE>(self, 0x11) = 1;
    return 1;
}

// ---- CDeployedInventoryItem ---------------------------------------------------------------------------

void Deployed_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(VT_DEPLOYABLE));
    vtable_and_defaults(o, vt(VT_DEPLOYED));
}
void Deployed_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_DEPLOYED);
    if (dw(o, 0x1c)) {
        remove_listener(pt(o, 0x1c), o);
        dw(o, 0x1c) = 0;
    }
    if (dw(o, 0x18)) {
        remove_listener(pt(o, 0x18), o);
        dw(o, 0x18) = 0;
    }
    obj_free_fields(o);
    vtable_and_free(o, vt(VT_DEPLOYABLE));
    item_dtor(o);
}
// 0x47a4b0, 0x47a4f0, 0x47a550 (slot 1)
void *__cdecl Deployed_Create() { return create_one<0x20, Deployed_construct>(); }
void *__fastcall Deployed_CreateArray(int count, int, int) { return create_array<0x20, Deployed_construct>(count); }
void *THISCALL Deployed_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, Deployed_destroy>(self, flags); }
// 0x47a700. eax: the last call's.
DWORD __cdecl Deployed_RegisterFields()
{
    void *f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD_CTOR)(
            f, glob_addr(0x610e64) /* "Debug/Deployed Entity" */, 0x18, 4, 0x18, destructable_class(), 3, 0);
    add_field(Deployed_ClassInfo(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD_CTOR)(
            f, glob_addr(0x610e58) /* "Debug/Owner" */, 0x1c, 4, 0x1c, class_at(fn_addr(0x4f9bf0)), 3, 0);
    return add_field(Deployed_ClassInfo(), f);
}
// 0x47a8a0 (slot 49): the share of hit points the deployed entity has lost (st(0))
long double THISCALL Deployed_DamageShare(BYTE *self) { return damage_share(pt(self, 0x18)); }
// 0x47a990 (part): the deployed entity is taken back by the part. Checks first, each reporting a static
// message and giving al 0: the part needs an inventory (0x6bda94) that holds this item (0x6bda4c), the
// definition an "Ammunition" (0x6bda44) whose projectile is the entity's model (0x6bda6c) and whose
// default item is a CUndeployedInventoryItem (0x6bda60). A clone of that item gets the entity's lost
// share of hit points as "Current Damage" (0 and the message 0x6bda68 without hit points), this item is
// marked for deletion, the entity dies, the clone is taken by the part (slot 33) and deleted. al 1.
DWORD THISCALL Deployed_Recall(BYTE *self, BYTE *part)
{
    BYTE *inv = inventory_of(part);
    if (!inv) {
        report(0x6bda94);
        return 0;
    }
    if (inventory_index_of(inv, self) == -1) {
        report(0x6bda4c);
        return 0;
    }
    if (!dw(definition_of(self), 0x2c)) {
        report(0x6bda44);
        return 0;
    }
    BYTE *proj = projectile_of(def_list_object(dw(definition_of(self), 0x2c)));
    if (proj != fn<BYTE *(THISCALL *)(void *)>(0x48b290)(pt(self, 0x18))) {
        report(0x6bda6c);
        return 0;
    }
    if (!dw(def_list_object(dw(definition_of(self), 0x2c)), 0x1c)) {
        report(0x6bda60);
        return 0;
    }
    CClassInfo *c = undeployed_class();
    if (!is_kind_of(pt(def_list_object(dw(definition_of(self), 0x2c)), 0x1c), c)) {
        report(0x6bda60);
        return 0;
    }
    BYTE *item = static_cast<BYTE *>(object_clone(pt(def_list_object(dw(definition_of(self), 0x2c)), 0x1c)));
    if (!c03(hit_points(pt(self, 0x18)), cst(glob_addr(C_ZERO)))) {
        fl<float>(item, 0x18) = fstp(damage_share(pt(self, 0x18)));
    } else {
        dw(item, 0x18) = 0;
        report(0x6bda68);
    }
    fl<BYTE>(self, 0x11) = 1;
    fn<DWORD(THISCALL *)(void *)>(0x4fbf30)(pt(self, 0x18));
    vcall<DWORD>(item, 33, 0, part);
    if (item)
        vcall<void *>(item, 1, 1);
    return 1;
}
// 0x47a8d0 (slot 0, a message (other, name)): "Leaving Scope" from the owner (not marked for deletion)
// recalls the entity (0x47a990; eax its); "Delete" from the owner ends listening to it, from the entity
// also marks the item for deletion (eax LayerPart_RemoveListener's); else CObject's slot 0.
DWORD THISCALL Deployed_OnMessage(BYTE *self, BYTE *other, const char *name)
{
    if (pt(self, 0x1c) == other && !fl<BYTE>(self, 0x11) && !crt_stricmp(name, glob<const char *>(0x60aee8)))
        return fn<DWORD(THISCALL *)(void *, void *)>(0x47a990)(self, pt(self, 0x1c));
    if (pt(self, 0x1c) == other && !crt_stricmp(name, glob<const char *>(0x60aef8))) {
        const DWORD r = remove_listener(pt(self, 0x1c), self);
        dw(self, 0x1c) = 0;
        return r;
    }
    if (pt(self, 0x18) == other && !crt_stricmp(name, glob<const char *>(0x60aef8))) {
        const DWORD r = remove_listener(pt(self, 0x18), self);
        dw(self, 0x18) = 0;
        fl<BYTE>(self, 0x11) = 1;
        return r;
    }
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x46cd50)(self, other, name);
}

// ---- The deploy and recall actions --------------------------------------------------------------------

// 0x47b270 (the startup registration's), 0x47b430 (slot 2); 0x47b5b0, 0x47b740 (slot 2)
CClassInfo *__cdecl DeployAction_ClassInfo()
{
    return class_getter(0x6bdaac, 0x610ec0 /* "CDeployCurrentInventoryItemAction" */, 0.0f, getter<0x4dc1f0>, 8, 0, 0x47b2f0, 0x47b330, 0x58ea40);
}
CClassInfo *__cdecl RecallAction_ClassInfo()
{
    return class_getter(0x6bdab0, 0x610ee4 /* "CRecallCurrentInventoryItemAction" */, 0.0f, getter<0x4dc1f0>, 8, 0, 0x47b600, 0x47b640, 0x58ea40);
}
template <DWORD VT> void ItemAction_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x41a6b0)(o);   // CAction
    vtable_and_defaults(o, vt(VT_ITEM_ACTION));
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> void ItemAction_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    vtable_and_free(o, vt(VT_ITEM_ACTION));
    fn<void(THISCALL *)(void *)>(0x41a700)(o);
}
// 0x47b2c0: CInventoryItemAction's deleting destructor (no array path; the pool block of 8)
void *THISCALL ItemAction_DeletingDtor(BYTE *self, DWORD flags)
{
    vtable_and_free(self, vt(VT_ITEM_ACTION));
    fn<void(THISCALL *)(void *)>(0x41a700)(self);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}
// 0x47b2f0, 0x47b330, 0x47b390 (slot 1); 0x47b600, 0x47b640, 0x47b6a0 (slot 1)
template <DWORD VT> void *__cdecl ItemAction_Create() { return create_one<8, ItemAction_construct<VT>>(); }
template <DWORD VT> void *__fastcall ItemAction_CreateArray(int count, int, int) { return create_array<8, ItemAction_construct<VT>>(count); }
template <DWORD VT> void *THISCALL ItemAction_DeletingDtorOf(BYTE *self, DWORD flags) { return deleting_dtor<8, ItemAction_destroy<VT>>(self, flags); }
// 0x47b490 / 0x47b7a0 (slot 28, the item's action (layer, part, inventory, item, a5, a6)): an undeployed
// item is deployed (0x47a0f0) / a deployed one recalled (0x47a990) by the part; then the inventory updates
// (its slot 24 with the part as a character or 0 and 0.01 s) and the layer (CLayer::Update, 0.01 s). Another
// item: the reporter gets "Trying to execute ... on something other that a ..." (0x6bda88 / 0x6bda50), al 0.
// al: the deploy's / recall's.
template <DWORD DO, DWORD MESSAGE> DWORD THISCALL ItemAction_Execute(BYTE *, BYTE *layer, BYTE *part, BYTE *inv, BYTE *item, DWORD, DWORD)
{
    CClassInfo *c = fn_addr(DO) == fn_addr(0x47a0f0) ? undeployed_class() : deployed_class();
    if (!is_kind_of(item, c)) {
        report(MESSAGE);
        return 0;
    }
    const DWORD r = fn<DWORD(THISCALL *)(void *, void *)>(DO)(item, part);
    BYTE *ch = part && is_kind_of(part, character_class()) ? part : nullptr;
    vcall<void>(inv, 24, ch, 0x3c23d70a);
    fn<void(THISCALL *)(void *, DWORD)>(0x4ea220)(layer, 0x3c23d70a);
    return r & 0xff;
}

// ---- CPickupDeployedItemAI --------------------------------------------------------------------------------

// 0x47b8c0 (the startup registration's), 0x47ba20 (slot 2)
CClassInfo *__cdecl PickupDeployed_ClassInfo()
{
    return class_getter(0x6bdab4, 0x610f08 /* "CPickupDeployedItemAI" */, 0.0f, getter<0x53d190>, 0x30, 0, 0x47b910, 0x47b940, 0x58ea40);
}
void PickupDeployed_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_PICKUP_DEPLOYED));
    fn<void(THISCALL *)(void *)>(0x53d940)(o);   // CPickupAI's destructor
}
// 0x47b9a0 (slot 1)
void *THISCALL PickupDeployed_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, PickupDeployed_destroy>(self, flags); }
// 0x47ba80 (slot 34, a collision (part, other, a3, a4, a5)): CPickupAI's collect (0x53e110) gives the
// item; an undeployed item gets the share of hit points the part (a CEntityDestructable with hit points)
// had lost as "Current Damage"; otherwise 0 and "CPickupDeployedItemAI on something with no hit points"
// / "... less than a CEntityDestructable" (0x6bda90 / 0x6bda74); not an undeployed item: "... giving an
// item that isn't a CUndeployedInventoryItem" (0x6bda8c).
void THISCALL PickupDeployed_Collide(BYTE *self, BYTE *part, DWORD other, DWORD a3, DWORD a4, DWORD a5)
{
    BYTE *given = nullptr;
    fn<void(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD, BYTE **)>(0x53e110)(self, part, other, a3, a4, a5, &given);
    BYTE *item = nullptr;
    if (given) {
        CClassInfo *c = undeployed_class();
        if (is_kind_of(given, c))
            item = given;
    }
    BYTE *e = part && is_kind_of(part, destructable_class()) ? part : nullptr;
    if (!item) {
        report(0x6bda8c);
        return;
    }
    if (e && !c03(hit_points(e), cst(glob_addr(C_ZERO)))) {
        fl<float>(item, 0x18) = fstp(damage_share(e));
        return;
    }
    dw(item, 0x18) = 0;
    report_string(e ? glob<void *>(0x6bda90) : glob<void *>(0x6bda74));
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(deployables)
{
    const Hook list[] = {
#include "deployables_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
