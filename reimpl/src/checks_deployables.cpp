// Checks of deployables.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters, Create, CreateArray, deleting destructors,
//    RegisterFields and the registered descriptors of the deployable items, the deploy and recall
//    actions and CPickupDeployedItemAI (CDeployableInventoryItem: its getters only, it is abstract).
//  - The slots in the fake world (checks_weapon_world.h): the deployable's same-kind test and destructor,
//    deploy (0x47a0f0) and recall (0x47a990), the deployed item's damage share and messages, the two
//    actions' execute, CInventoryItemAction's destructor, the pickup's collision and the destructor
//    levels on fake objects. The entity a deploy makes, its placing, its damage and the inventory are
//    recorders: nothing of the running game is deployed, recalled or changed.
//  - Real data: the registered class infos.
#include "checks_weapon_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_deployables(Hook *out);

namespace {

using namespace ww;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CDeployableInventoryItem", 0x18, 0x6bda9c, 0, 0, 0, {0x479c40, 0x479c90}, false},
    {"CUndeployedInventoryItem", 0x1c, 0x6bdaa0, 0x479e10, 0x479e50, 0x47a010, {0x479d30, 0x479f60}, true},
    {"CDeployedInventoryItem", 0x20, 0x6bdaa4, 0x47a4b0, 0x47a4f0, 0x47a700, {0x47a410, 0x47a650}, true},
    {"CDeployCurrentInventoryItemAction", 8, 0x6bdaac, 0x47b2f0, 0x47b330, 0, {0x47b270, 0x47b430}, true},
    {"CRecallCurrentInventoryItemAction", 8, 0x6bdab0, 0x47b600, 0x47b640, 0, {0x47b5b0, 0x47b740}, true},
    {"CPickupDeployedItemAI", 0x30, 0x6bdab4, 0x47b910, 0x47b940, 0, {0x47b8c0, 0x47ba20}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_DEPLOYABLE, C_UNDEPLOYED, C_DEPLOYED, C_DEPLOY_ACTION, C_RECALL_ACTION, C_PICKUP };
constexpr DWORD VTABLES[] = {0x5f0ee0, 0x5f0e18, 0x5f0fa8, 0x5f119c, 0x5f1284, 0x5f12f8};

int g_self_class;

DWORD time_value(Gen &g, float lo, float hi)
{
    const DWORD v = g.next() % 16;
    const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xbf800000u};
    return v < 5 ? sp[v] : g.fbits_(lo, hi);
}
DWORD entity(Gen &g, int null_one_in = 0) { return ptrv(any_entity(g, null_one_in)); }
DWORD item(Gen &g) { return ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM))); }

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    for (DWORD i = 0; i < 0x40; i++)
        fl<DWORD>(s, i * 4) = g.next();
    fl<DWORD>(s, 0) = VTABLES[c];
    fl<DWORD>(s, 4) = 0;
    switch (c) {
    case C_DEPLOYABLE:
    case C_UNDEPLOYED:
    case C_DEPLOYED:
        fl<DWORD>(s, 8) = g.next() % 4;
        fl<DWORD>(s, 0xc) = 0;
        fl<BYTE>(s, 0x10) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x11) = (BYTE)(g.next() % 3 == 0);
        fl<DWORD>(s, 0x14) = 0;
        if (c == C_DEPLOYED) {
            fl<DWORD>(s, 0x18) = entity(g, 4);
            fl<DWORD>(s, 0x1c) = entity(g, 4);
        } else {
            fl<DWORD>(s, 0x18) = time_value(g, 0.0f, 1.0f);
        }
        break;
    case C_PICKUP:
        fl<DWORD>(s, 0xc) = 0;
        break;
    default:
        break;
    }
}

// The deploy and the recall read the catalogue's records without the test the other slots make: they are
// there and not being loaded during these cases (the original reads address 0 otherwise)
void records_ready()
{
    *at<DWORD *>(0x6c0c38) = ptrv(obj(O_CAT_RECORDS));
    *at<DWORD *>(0x6c0c3c) = 4;
    for (DWORD i = 0; i < 4; i++)
        obj(O_CAT_RECORDS)[i * 0x80 + 0x7f] = 0;
}

const char *const NAMES[] = {"Leaving Scope", "Delete", "leaving scope", "DELETE", "Died", "Other"};
void a_part(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_item(Gen &g, DWORD *a) { a[0] = item(g); }
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }
void a_other_name(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    const DWORD k = g.next() % 3;
    a[0] = k == 0 ? fl<DWORD>(s, 0x1c) : k == 1 ? fl<DWORD>(s, 0x18) : entity(g);
    a[1] = ptrv(NAMES[g.next() % 6]);
}
// slot 28 (layer, part, inventory, item, a5, a6)
void a_execute(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    a[1] = entity(g);   // the deploy reads the part's layer without a test
    a[2] = ptrv(obj(O_INVENTORY + (int)(g.next() % N_INVENTORY)));
    a[3] = item(g);
    a[4] = g.next();
    a[5] = g.next();
}
// slot 34 (part, other, a3, a4, a5)
void a_collide(Gen &g, DWORD *a)
{
    a[0] = entity(g, 4);
    a[1] = entity(g, 4);
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next() % 4 ? g.next() & 0xff : 0;
}

enum { P_PLAIN, P_DEPLOY };
struct ClassSlot {
    int cls;
    SlotTest t;
    int policy = P_PLAIN;
};
// OnMessage: al (the recall path gives the recall's al; its upper bytes are the calls' before)
const ClassSlot SLOTS[] = {
    {C_UNDEPLOYED, {"CDeployableInventoryItem::SameKind", 0x479cf0, 1, 0xffffffff, 100, a_item}},
    {C_DEPLOYABLE, {"CDeployableInventoryItem::DeletingDtor", 0x479dd0, 1, 0xffffffff, 30, a_flags0}},
    {C_UNDEPLOYED, {"CUndeployedInventoryItem::DeletingDtor", 0x479ec0, 1, 0xffffffff, 30, a_flags0}},
    {C_UNDEPLOYED, {"CUndeployedInventoryItem::Deploy", 0x47a0f0, 1, 0xff, 800, a_part}, P_DEPLOY},
    {C_DEPLOYED, {"CDeployedInventoryItem::DeletingDtor", 0x47a550, 1, 0xffffffff, 60, a_flags0}},
    {C_DEPLOYED, {"CDeployedInventoryItem::DamageShare", 0x47a8a0, -16, 0xffffffff, 100, nullptr}},
    {C_DEPLOYED, {"CDeployedInventoryItem::OnMessage", 0x47a8d0, 2, 0xff, 400, a_other_name}, P_DEPLOY},
    {C_DEPLOYED, {"CDeployedInventoryItem::Recall", 0x47a990, 1, 0xff, 800, a_part}, P_DEPLOY},
    {C_DEPLOY_ACTION, {"CDeployCurrentInventoryItemAction::Execute", 0x47b490, 6, 0xff, 400, a_execute}, P_DEPLOY},
    {C_RECALL_ACTION, {"CRecallCurrentInventoryItemAction::Execute", 0x47b7a0, 6, 0xff, 400, a_execute}, P_DEPLOY},
    {C_DEPLOY_ACTION, {"CInventoryItemAction::DeletingDtor", 0x47b2c0, 1, 0xffffffff, 30, a_flags0}},
    {C_DEPLOY_ACTION, {"CDeployCurrentInventoryItemAction::DeletingDtor", 0x47b390, 1, 0xffffffff, 30, a_flags0}},
    {C_RECALL_ACTION, {"CRecallCurrentInventoryItemAction::DeletingDtor", 0x47b6a0, 1, 0xffffffff, 30, a_flags0}},
    {C_PICKUP, {"CPickupDeployedItemAI::DeletingDtor", 0x47b9a0, 1, 0xffffffff, 30, a_flags0}},
    {C_PICKUP, {"CPickupDeployedItemAI::Collide", 0x47ba80, 5, 0, 400, a_collide}},
};

Policy deploy_policy(int kind)
{
    Policy p{};
    p.find_minus_one = 4;
    // used without a test: the definitions, the layer, the projectile, the default items (T_OTHER)
    p.nonnull = 1u << T_DEF | 1u << T_LAYER | 1u << T_OTHER;
    if (kind == P_DEPLOY)   // the made entity and the item a clone gives
        p.nonnull |= 1u << T_ENTITY | 1u << T_ITEM;
    return p;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self_class = s.cls;
        g_before_call = s.policy == P_DEPLOY ? records_ready : nullptr;
        const unsigned count = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= count; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, deploy_policy(s.policy));
        g_before_call = nullptr;
    }
}

void test_registered(Tally &registered)
{
    for (const invcheck::ClassDef &c : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        const bool create_ok = !c.create || (ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array);
        registered.check(ci && ci->size == c.size && create_ok && at<Getter>(c.getters[0])() == ci, c.name, d);
    }
}

}  // namespace

ZAX_CHECKS(deployables)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"deployables: class getters"}, objects{"deployables: Create, CreateArray, destructors"}, fields{"deployables: RegisterFields"},
        real_fields{"deployables: RegisterFields against the registered fields (real data)"}, slots{"deployables: slots in the fake world"},
        registered{"deployables: registered class infos (real data)"};
    invcheck::g_hook_table = hooks_deployables;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    else
        objects.check(false, "deployables", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        world_off();
    } else {
        slots.check(false, "deployables", "[the fake world could not be set up]");
    }
    test_registered(registered);
    slots.report();
    registered.report();
    log("  deployables: %u ms", GetTickCount() - start);
}

}  // namespace zax
