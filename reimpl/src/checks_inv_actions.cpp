// Checks of the inventory item actions, CInventoryItemBase, the item references, the catalogue's slot 35
// and the two investigate AI classes (inv_actions.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with 0-3
//    elements, the deleting destructors and both class getters of the four action classes; the getters of
//    the abstract CInventoryItemAction, CInventoryItemBase, CInvestigateAI and CInvestigateAreaAI;
//    CInventoryItemBase's RegisterFields into a fake class info and with the class variable 0 and its
//    registered descriptor (real data); its constructor, destructor and deleting destructor and the two
//    AI destructors called directly on pool blocks.
//  - Behaviour on a fake world: slot 23 (execute) with a recording slot 28, the four slot 28s, the item
//    references 0x4dd480/0x4dd580 and slot 35 on fake lists. The item catalogue 0x6c0c08 and the category
//    list 0x6c0800 get records of our own and a copy of their vtable whose slots 1 (the record's name) and
//    37 (load) record; the error reporter 0x6c02cc is a fake; every function of other subsystems the
//    slots call jumps to a recorder (STANDS below), CAIBase's constructor, InitFields and ResetFields stay
//    real: a COverridePickupAI the drop makes is a real object, compared by its bytes and deleted after the
//    case. Compared: eax (al where only al is defined), the event log, the fake world and the arena.
//  - Real data: the references and slot 35 on the real catalogue's loaded records (slot 39, the entry's
//    path, real; its text compared).
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_inv_actions(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CInventoryItemAction", 8, 0x6c0d7c, 0, 0, 0, {0x4dc1f0, 0x4dc240}, false},
    {"CDropCurrentInventoryItemAction", 8, 0x6c0d80, 0x4dc3a0, 0x4dc3e0, 0, {0x4dc300, 0x4dc4e0}, true},
    {"COldBadUseCurrentInventoryItemAction", 8, 0x6c0d84, 0x4dc960, 0x4dc9a0, 0, {0x4dc8c0, 0x4dcaa0}, true},
    {"CSelectCurrentInventoryItemAction", 8, 0x6c0d88, 0x4dcd40, 0x4dcd80, 0, {0x4dcca0, 0x4dce80}, true},
    {"CHealTriggerFromCurrentInventoryItemAction", 8, 0x6c0d8c, 0x4dd030, 0x4dd070, 0, {0x4dcf90, 0x4dd170}, true},
    {"CInventoryItemBase", 0xc, 0x6c0d94, 0, 0, 0x4dd380, {0x4dd2d0, 0x4dd320}, false},
    {"CInvestigateAI", 0x10, 0x6c0d9c, 0, 0, 0, {0x4dd6d0, 0x4dd720}, false},
    {"CInvestigateAreaAI", 0x10, 0x6c0da4, 0, 0, 0, {0x4dd7b0, 0x4dd800}, false},
};

using T1 = DWORD(THISCALL *)(void *, DWORD);

// CInventoryItemBase's constructor 0x4dd410, destructor 0x4dd460, deleting destructor 0x4dd430 and the
// AI destructors 0x4dd780/0x4dd860 on pool blocks filled with a pattern (bytes and eax compared)
void test_direct(Tally &t, int kind, unsigned seed)
{
    static const char *const NAMES[] = {"CInventoryItemBase::Ctor+Dtor", "CInventoryItemBase::Ctor+DeletingDtor", "CInvestigateAI::Dtor",
                                        "CInvestigateAreaAI::Dtor"};
    const DWORD size = kind < 2 ? 0xc : 0x10;
    BYTE obj[2][2][0x10];
    DWORD r[2][2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *o = static_cast<BYTE *>(pool_alloc(size));
        memset(o, (BYTE)random(), size);
        if (kind < 2)
            r[side][0] = at<DWORD(THISCALL *)(void *)>(0x4dd410)(o) == ptrv(o);
        else {   // a Create of the abstract class: CAIBase's constructor, both vtables, InitFields
            at<void *(THISCALL *)(void *)>(0x4fd060)(o);
            *reinterpret_cast<DWORD *>(o) = 0x5ed1d0;
            at<void(THISCALL *)(void *)>(0x4173c0)(o);
            *reinterpret_cast<DWORD *>(o) = kind == 2 ? 0x5efa04 : 0x5f861c;
            at<void(THISCALL *)(void *)>(0x4173c0)(o);
            r[side][0] = 1;
        }
        memcpy(obj[side][0], o, size);
        const DWORD flags = random() & 1;
        if (kind == 0)
            r[side][1] = at<DWORD(THISCALL *)(void *)>(0x4dd460)(o);
        else if (kind == 1)
            r[side][1] = at<T1>(0x4dd430)(o, flags) == ptrv(o);
        else
            r[side][1] = at<DWORD(THISCALL *)(void *)>(kind == 2 ? 0x4dd780 : 0x4dd860)(o);
        memcpy(obj[side][1], o, size);
        if (kind != 1 || !flags)
            pool_free(o, size);
        arena_off();
        side_end(side);
    }
    char d[160];
    const bool same = r[0][0] == r[1][0] && r[0][1] == r[1][1] && same_region(obj[0][0], obj[1][0], size) &&
                      same_region(obj[0][1], obj[1][1], size) && same_region(g_arena[0], g_arena[1], g_used[0]);
    wsprintfA(d, "[seed %u: eax %x %x / %x %x, first difference +%x: %x / %x]", seed, r[0][0], r[0][1], r[1][0], r[1][1], g_diff_at, g_diff_a,
              g_diff_b);
    t.check(same, NAMES[kind], d);
}

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NPART = 4, NOBJ = 10, NDEF = 6, NCAT = 4;

struct Record {
    char name[0x78];
    void *object;
    BYTE keep, in_use, unknown7e, loading;
};
struct FakeObject {   // parts, items, the inventory, AIs, definitions: slot 2 the class
    DWORD vftable;
    BYTE body[0x2c];
    CClassInfo *cls;
    DWORD id;
    DWORD answer[2];
};
struct World {
    FakeObject parts[NPART];
    FakeObject objs[NOBJ];   // 0 inventory, 1-3 items, 4 entity, 5 and 8 found AIs, 6 clone, 7 default item, 9 new pickup
    FakeObject defs[NDEF];
    FakeObject cats[NCAT];
    FakeObject self;         // the action of slot 23 (its slot 28 records)
    Record records[NDEF];
    Record cat_records[NCAT];
    DWORD list_vftable;      // a list of our own for slot 35: +0x30 records, +0x34 count
    BYTE list_body[0x2c];
    Record *list_records;
    DWORD list_count;
    DWORD reporter_vftable;
    DWORD answers[32];
};
World g_world;

void *g_vtable[128];
void *g_list_vtable[64];
void *g_cat_list_vtable[64];
void *g_own_list_vtable[64];
void *g_reporter_vtable[20];
CClassInfo *g_classes[10];
// The real objects a case made (the drop's new COverridePickupAI), by their order
constexpr int NMADE = 4;
BYTE *g_new[NMADE];
int g_nnew;

int obj_id(const void *p)
{
    World &w = g_world;
    for (int i = 0; i < NPART; i++)
        if (p == &w.parts[i])
            return 0x10 + i;
    for (int i = 0; i < NOBJ; i++)
        if (p == &w.objs[i])
            return 0x20 + i;
    for (int i = 0; i < NDEF; i++)
        if (p == &w.defs[i])
            return 0x30 + i;
    for (int i = 0; i < NCAT; i++)
        if (p == &w.cats[i])
            return 0x38 + i;
    if (p == &w.self)
        return 0x3f;
    for (int i = 0; i < g_nnew; i++)
        if (p == g_new[i])
            return 0x40 + i;
    if (p && g_nnew < NMADE && readable(p, 0x38) && *static_cast<const DWORD *>(p) == 0x5f83b4) {
        g_new[g_nnew] = static_cast<BYTE *>(const_cast<void *>(p));
        return 0x40 + g_nnew++;
    }
    return p ? 0x99 : 0;
}
DWORD ans(int k) { return g_world.answers[k]; }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }

// The fakes' slots
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
CClassInfo *THISCALL f_class(FakeObject *o) { ev(0x102, (DWORD)obj_id(o)); return o->cls; }
DWORD THISCALL f_delete(FakeObject *o, DWORD flags) { ev(0x101, (DWORD)obj_id(o), flags); return result(0x01); }
DWORD THISCALL f_reset(FakeObject *o) { ev(0x119, (DWORD)obj_id(o)); return result(0x19); }
DWORD THISCALL f_update(FakeObject *o, void *ch, DWORD dt) { ev(0x124, (DWORD)obj_id(o), (DWORD)obj_id(ch), dt); return result(0x24); }
DWORD THISCALL f_use(FakeObject *o, DWORD layer, void *part, void *inv, void *item, DWORD a3, DWORD a4)
{
    ev(0x128, (DWORD)obj_id(o), layer, (DWORD)obj_id(part));
    ev(0x129, (DWORD)obj_id(inv), (DWORD)obj_id(item), a3 ^ (a4 << 1));
    return result(0x28);
}
DWORD THISCALL f_count(FakeObject *o) { ev(0x141, (DWORD)obj_id(o)); return o->answer[0]; }
DWORD THISCALL f_deselect(FakeObject *o, DWORD a) { ev(0x145, (DWORD)obj_id(o), a); return result(0x45); }
DWORD THISCALL f_take(FakeObject *o, DWORD a, DWORD n) { ev(0x152, (DWORD)obj_id(o), a, n); return result(0x52); }
DWORD THISCALL f_health(FakeObject *o, void *part) { ev(0x153, (DWORD)obj_id(o), (DWORD)obj_id(part)); return result(0x53); }
void *THISCALL f_find_ai(FakeObject *o, CClassInfo *c, DWORD flag)
{
    const DWORD k = c == g_classes[4] ? 10 : 11;
    ev(0x164, (DWORD)obj_id(o), k, flag);
    if (ans(k) % 3 == 0)
        return nullptr;
    return k == 10 ? &g_world.objs[5] : &g_world.objs[8];
}
DWORD THISCALL f_heal(FakeObject *o, DWORD amount) { ev(0x198, (DWORD)obj_id(o), amount); return result(0x98); }
DWORD THISCALL f_report(void *, const char *text) { ev(0x611, fnv_text(2166136261u, text)); return result(0x11); }
// The lists' slots: the record's name (1), load (37), the entry path (39, the list of slot 35)
const char *THISCALL l_name(void *list, DWORD i)
{
    ev(0x201, ptrv(list), i);
    if (list == at<void *>(0x6c0800))
        return g_world.cat_records[i % NCAT].name;
    return g_world.records[i % NDEF].name;
}
void *THISCALL l_load(void *list, const char *dir, const char *name, void **out)
{
    const bool cats = list == at<void *>(0x6c0800);
    Record *recs = cats ? g_world.cat_records : g_world.records;
    const DWORD n = cats ? NCAT : NDEF;
    DWORD k = 0xff;
    for (DWORD i = 0; i < n; i++)
        if (out == &recs[i].object)
            k = i;
    ev(0x237, ptrv(list), fnv_text(fnv_text(2166136261u, dir), name), k | (DWORD)recs[k < n ? k : 0].loading << 8);
    if (ans(22) % 5 == 0)
        return nullptr;
    return cats ? (void *)&g_world.cats[(k + 1) % NCAT] : (void *)&g_world.defs[(k + 2) % NDEF];
}
DWORD THISCALL l_entry_path(void *list, DWORD i, DWORD a2) { ev(0x239, (DWORD)(list == &g_world.list_vftable), i, a2); return result(0x39); }

// Stand-ins for the functions of other subsystems
void *THISCALL s_inventory(void *p) { ev(0x701, (DWORD)obj_id(p)); return ans(0) % 4 ? &g_world.objs[0] : nullptr; }
DWORD THISCALL s_focus(void *inv) { ev(0x702, (DWORD)obj_id(inv)); return ans(1) % 5; }
void *THISCALL s_item_at(void *inv, DWORD i) { ev(0x703, (DWORD)obj_id(inv), i); return ans(2) % 4 ? &g_world.objs[1 + i % 3] : nullptr; }
DWORD THISCALL s_index_of(void *inv, void *item) { ev(0x704, (DWORD)obj_id(inv), (DWORD)obj_id(item)); return ans(3) % 5; }
DWORD THISCALL s_remove(void *inv, void *item) { ev(0x705, (DWORD)obj_id(inv), (DWORD)obj_id(item)); return result(0x05); }
DWORD THISCALL s_change_to(void *inv, DWORD index, DWORD cat, void *part, DWORD mode)
{
    ev(0x706, (DWORD)obj_id(inv), index, cat);
    ev(0x707, (DWORD)obj_id(part), mode);
    return result(0x06);
}
DWORD THISCALL s_find(void *list, const char *name, int index)
{
    const DWORD k = lstrcmpA(name, "Health") == 0 ? 0 : lstrcmpA(name, "Primary") == 0 ? 1 : lstrcmpA(name, "Secondary") == 0 ? 2 : 3;
    ev(0x708, ptrv(list), k, (DWORD)index);
    return ans(12 + (int)k) % NCAT;
}
const long double HP[8] = {10.0L, 50.0L, 100.0L, __builtin_nanl(""), __builtin_infl(), 0.0L, 99.5L, -5.0L};
long double THISCALL s_hp_left(void *p) { ev(0x709, (DWORD)obj_id(p)); return HP[ans(15) % 8]; }
long double THISCALL s_hp(void *p) { ev(0x70a, (DWORD)obj_id(p)); return HP[ans(16) % 8]; }
DWORD THISCALL s_model(void *def) { ev(0x70b, (DWORD)obj_id(def)); return 0x5e7a0000u + ans(17) % 0x100; }
void *THISCALL s_default_item(void *def) { ev(0x70c, (DWORD)obj_id(def)); return &g_world.objs[7]; }
void *THISCALL s_from_model(void *list, DWORD model, CClassInfo *c, DWORD a, DWORD b, DWORD x)
{
    ev(0x70d, ptrv(list), model, ptrv(c));
    ev(0x70e, a, b, x);
    return &g_world.objs[4];
}
void THISCALL s_set_item(void *ai, void *item) { ev(0x70f, (DWORD)obj_id(ai), (DWORD)obj_id(item)); }
void *THISCALL s_pickup_item(void *ai, void *part) { ev(0x710, (DWORD)obj_id(ai), (DWORD)obj_id(part)); return &g_world.objs[1 + ans(18) % 3]; }
void *THISCALL s_pickup_ctor(void *block)
{
    ev(0x711);
    pool_free(block, 0x30);
    return &g_world.objs[9];
}
void THISCALL s_add_ai(void *p, void *ai) { ev(0x712, (DWORD)obj_id(p), (DWORD)obj_id(ai)); }
DWORD THISCALL s_place_near(void *layer, void *part, void *ref, DWORD dist)
{
    ev(0x713, ptrv(layer), (DWORD)obj_id(part), (DWORD)obj_id(ref) ^ dist);
    return result(0x13);
}
void *THISCALL s_clone(void *o, DWORD a, DWORD b) { ev(0x714, (DWORD)obj_id(o), a, b); return &g_world.objs[6]; }
DWORD THISCALL s_list_load(void *list, DWORD i)
{
    ev(0x715, ptrv(list), i);
    g_world.records[i % NDEF].object = &g_world.defs[(i + 3) % NDEF];
    return result(0x15);
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x4267e0, (void *)&s_inventory},  {0x426070, (void *)&s_focus},        {0x424f60, (void *)&s_item_at},
    {0x4262a0, (void *)&s_index_of},   {0x426260, (void *)&s_remove},       {0x425590, (void *)&s_change_to},
    {0x523df0, (void *)&s_find},       {0x48d030, (void *)&s_hp_left},      {0x48d150, (void *)&s_hp},
    {0x5b76d0, (void *)&s_model},      {0x5b7aa0, (void *)&s_default_item}, {0x5176f0, (void *)&s_from_model},
    {0x53efe0, (void *)&s_set_item},   {0x53e4c0, (void *)&s_pickup_item},  {0x53d8f0, (void *)&s_pickup_ctor},
    {0x4fbc50, (void *)&s_add_ai},     {0x4eb7b0, (void *)&s_place_near},   {0x416760, (void *)&s_clone},
    {0x55e090, (void *)&s_list_load},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

void init_world_tables()
{
    for (int i = 0; i < 128; i++)
        g_vtable[i] = (void *)&f_bad;
    g_vtable[1] = (void *)&f_delete;
    g_vtable[2] = (void *)&f_class;
    g_vtable[19] = (void *)&f_reset;
    g_vtable[24] = (void *)&f_update;
    g_vtable[28] = (void *)&f_use;
    g_vtable[41] = (void *)&f_count;
    g_vtable[45] = (void *)&f_deselect;
    g_vtable[52] = (void *)&f_take;
    g_vtable[53] = (void *)&f_health;
    g_vtable[64] = (void *)&f_find_ai;
    g_vtable[98] = (void *)&f_heal;
    // copies of the two lists' vtables with the record name and load slots recording
    memcpy(g_list_vtable, *at<void ***>(0x6c0c08), sizeof g_list_vtable);
    memcpy(g_cat_list_vtable, *at<void ***>(0x6c0800), sizeof g_cat_list_vtable);
    g_list_vtable[1] = g_cat_list_vtable[1] = (void *)&l_name;
    g_list_vtable[37] = g_cat_list_vtable[37] = (void *)&l_load;
    for (int i = 0; i < 64; i++)
        g_own_list_vtable[i] = (void *)&f_bad;
    g_own_list_vtable[39] = (void *)&l_entry_path;
    for (int i = 0; i < 20; i++)
        g_reporter_vtable[i] = (void *)&f_bad;
    g_reporter_vtable[17] = (void *)&f_report;
    const DWORD getters[9] = {0x4163b0, 0x487620, 0x48bdf0, 0x49a930, 0x53d190, 0x53e850, 0x5aefe0, 0x5b5f10, 0x41a650};
    for (int i = 0; i < 9; i++)
        g_classes[i] = at<CClassInfo *(__cdecl *)()>(getters[i])();
    g_classes[9] = *at<CClassInfo **>(0x6c0d54);   // CInventoryItemDefinition
}

void setup(unsigned seed, int kind)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    FakeObject *all[NPART + NOBJ + NDEF + NCAT + 1];
    int n = 0;
    for (auto &o : w.parts) all[n++] = &o;
    for (auto &o : w.objs) all[n++] = &o;
    for (auto &o : w.defs) all[n++] = &o;
    for (auto &o : w.cats) all[n++] = &o;
    all[n++] = &w.self;
    for (int i = 0; i < n; i++) {
        FakeObject &o = *all[i];
        o.vftable = ptrv(g_vtable);
        for (DWORD k = 0; k < sizeof o.body; k++)
            o.body[k] = (BYTE)random();
        o.cls = g_classes[rnd(10)];
        o.id = (DWORD)i;
        o.answer[0] = rnd(5);
        o.answer[1] = random();
    }
    // parts mostly destructable or characters, definitions mostly Zax definitions, AIs pickups/overrides
    for (auto &o : w.parts)
        o.cls = g_classes[rnd(4) ? 2 + rnd(2) : rnd(10)];
    for (auto &o : w.defs) {
        o.cls = g_classes[rnd(4) ? 7 : rnd(4) ? 9 : rnd(10)];
        *reinterpret_cast<DWORD *>(&o.body[0x14]) = rnd(NCAT + 1);   // +0x18 the category
    }
    w.objs[5].cls = g_classes[rnd(4) ? 4 : rnd(10)];
    w.objs[8].cls = g_classes[rnd(4) ? 5 : rnd(10)];
    for (int i = 1; i <= 3; i++) {
        FakeObject &o = w.objs[i];
        *reinterpret_cast<DWORD *>(&o.body[4]) = rnd(NDEF + 2);     // +8 the definition index
        o.body[0xd] = (BYTE)(rnd(4) == 0);                          // +0x11 marked for deletion
        o.cls = g_classes[rnd(3) ? 6 : rnd(10)];
    }
    w.objs[8].body[0xc] = 0;   // +0x10 the override's item: mostly none
    if (rnd(3) == 0)
        *reinterpret_cast<DWORD *>(&w.objs[8].body[0xc]) = 0x5e7ab000u;
    w.objs[6].body[0xc] = (BYTE)rnd(2);   // the clone selected
    // the records: the Use cases need a definition for every index (they read it without a check)
    const bool safe = kind >= 1 && kind <= 4;
    for (int d = 0; d < NDEF; d++) {
        Record &r = w.records[d];
        wsprintfA(r.name, rnd(3) ? "Item %d" : "Sub/Item %d", d);
        r.object = safe || rnd(3) ? &w.defs[d] : nullptr;
        r.loading = (BYTE)(!safe && rnd(5) == 0);
    }
    for (int d = 0; d < NCAT; d++) {
        Record &r = w.cat_records[d];
        wsprintfA(r.name, rnd(3) ? "Type %d" : "Group/Type %d", d);
        r.object = rnd(3) ? &w.cats[d] : nullptr;
        r.loading = (BYTE)(rnd(5) == 0);
    }
    if (kind == 6)   // the category needs a definition too
        for (Record &r : w.records)
            r.loading = 0;
    w.list_vftable = ptrv(g_own_list_vtable);
    w.list_records = w.records;
    w.list_count = rnd(NDEF + 1);
    w.reporter_vftable = ptrv(g_reporter_vtable);
    for (int k = 0; k < 32; k++)
        w.answers[k] = random();
}

struct Case {
    const char *name;
    DWORD fn;
    bool al;
};
const Case CASES[] = {
    {"CInventoryItemAction::Execute", 0x4dc2a0, false},
    {"CDropCurrentInventoryItemAction::Use", 0x4dc590, true},
    {"COldBadUseCurrentInventoryItemAction::Use", 0x4dcb50, true},
    {"CSelectCurrentInventoryItemAction::Use", 0x4dcf30, true},
    {"CHealTriggerFromCurrentInventoryItemAction::Use", 0x4dd220, true},
    {"ItemRef_Definition", 0x4dd480, false},
    {"ItemRef_Category", 0x4dd580, false},
    {"ItemCatalogue::Slot35", 0x4d82e0, true},
};
constexpr int NCASES = sizeof CASES / sizeof CASES[0];

// The globals a case swaps: the two lists' vtables, records and counts, the reporter
constexpr DWORD SWAPS[] = {0x6c0c08, 0x6c0c38, 0x6c0c3c, 0x6c0800, 0x6c0830, 0x6c0834, 0x6c02cc};
constexpr int NSWAPS = sizeof SWAPS / sizeof SWAPS[0];
DWORD g_saved[NSWAPS];
BYTE g_saved_folder[2];
World g_after[2];
BYTE g_new_after[2][NMADE][0x38];
int g_nnew_after[2];

DWORD run_case(int k)
{
    World &w = g_world;
    const DWORD a = random(), b = random(), x = random();
    FakeObject *part = rnd(6) ? &w.parts[rnd(NPART)] : nullptr;
    FakeObject *item = &w.objs[1 + rnd(3)];
    switch (k) {
    case 0:
        return at<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD)>(CASES[k].fn)(&w.self, a, part, b, x);
    case 1: case 2: case 3: case 4:
        return at<DWORD(THISCALL *)(void *, DWORD, void *, void *, void *, DWORD, DWORD)>(CASES[k].fn)(&w.self, a, part, &w.objs[0], item, b, x);
    case 5: case 6:
        return rel(at<DWORD(THISCALL *)(void *)>(CASES[k].fn)(item), g_side);
    case 7: {
        FakeObject *obj = rnd(4) ? &w.defs[rnd(NDEF)] : &w.objs[rnd(NOBJ)];
        return at<DWORD(THISCALL *)(void *, void *, DWORD)>(CASES[k].fn)(&w.list_vftable, obj, a);
    }
    }
    return 0;
}

int g_ones[16];   // cases whose al was 1 (the paths the fake world reached)
void test_case(Tally &t, int k, unsigned seed)
{
    const Case &c = CASES[k];
    DWORD r[2], raw[2];
    for (int side = 0; side < 2; side++) {
        setup(seed, k);
        World &w = g_world;
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
        const DWORD values[NSWAPS] = {ptrv(g_list_vtable),     rnd(8) ? ptrv(w.records) : 0, NDEF, ptrv(g_cat_list_vtable),
                                      rnd(8) ? ptrv(w.cat_records) : 0, NCAT,          ptrv(&w.reporter_vftable)};
        for (int i = 0; i < NSWAPS; i++) {
            g_saved[i] = *at<DWORD *>(SWAPS[i]);
            *at<DWORD *>(SWAPS[i]) = values[i];
        }
        if (k == 6 && !values[1])
            *at<DWORD *>(0x6c0c38) = ptrv(w.records);   // the category's definition is read without a check
        if (k <= 4)
            *at<DWORD *>(0x6c0c38) = ptrv(w.records);
        // the folders (+0x38) empty sometimes: a path without '/'
        g_saved_folder[0] = *at<BYTE *>(0x6c0c40);
        g_saved_folder[1] = *at<BYTE *>(0x6c0838);
        if (rnd(3) == 0)
            *at<BYTE *>(0x6c0c40) = 0;
        if (rnd(3) == 0)
            *at<BYTE *>(0x6c0838) = 0;
        g_nnew = 0;
        arena_on(side, 0x5c);
        g_seed = seed * 13u + 5u;
        raw[side] = run_case(k);
        r[side] = raw[side];
        // the real objects the case made: their bytes, then deleted
        g_nnew_after[side] = g_nnew;
        for (int i = 0; i < g_nnew; i++) {
            memcpy(g_new_after[side][i], g_new[i], 0x38);
            vcall<void *>(g_new[i], 1, (DWORD)1);
        }
        arena_off();
        *at<BYTE *>(0x6c0c40) = g_saved_folder[0];
        *at<BYTE *>(0x6c0838) = g_saved_folder[1];
        for (int i = NSWAPS - 1; i >= 0; i--)
            *at<DWORD *>(SWAPS[i]) = g_saved[i];
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_stand_patches[i]);
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    if (c.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    g_ones[k] += (r[1] & 0xff) == 1;
    bool same_new = g_nnew_after[0] == g_nnew_after[1];
    for (int i = 0; same_new && i < g_nnew_after[0]; i++)
        same_new = same_region(g_new_after[0][i], g_new_after[1][i], 0x38);
    const bool same = r[0] == r[1] && same_new && same_log() && g_used[0] == g_used[1] &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World)) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, new %d / %d same %d, log %d / %d, heap %u / %u, first difference +%x: %x / %x]", seed, r[0], r[1],
              g_nnew_after[0], g_nnew_after[1], same_new, g_nlog[0] / 4, g_nlog[1] / 4, g_used[0], g_used[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, c.name, d);
}

// ---- Real data ----------------------------------------------------------------------------------------

struct RealRecord {
    char name[0x78];
    void *object;
    BYTE keep, in_use, unknown7e, loading;
};

// The references of an item with every loaded record of the real catalogue (and an index beyond them
// when record 0 is loaded); the category only where the definition's category record is loaded too.
// Slot 35 on the real catalogue with every loaded definition and with a category object.
void test_real(Tally &t, int &cases)
{
    const RealRecord *recs = *at<RealRecord **>(0x6c0c38);
    const DWORD n = *at<DWORD *>(0x6c0c3c);
    const RealRecord *cats = *at<RealRecord **>(0x6c0830);
    const DWORD ncat = *at<DWORD *>(0x6c0834);
    // the records loaded first as the game loads them (the original 0x4dd480 and 0x4dd580), so both
    // sides find them loaded
    BYTE item[0x18];
    DWORD loaded = 0;
    for (DWORD d = 0; recs && cats && d < n; d++) {
        memset(item, 0, sizeof item);
        *reinterpret_cast<DWORD *>(item + 8) = d;
        if (original<DWORD(THISCALL *)(void *)>(0x4dd480)(item))
            original<DWORD(THISCALL *)(void *)>(0x4dd580)(item);
        recs = *at<RealRecord **>(0x6c0c38);
        cats = *at<RealRecord **>(0x6c0830);
        loaded += recs[d].object != nullptr;
    }
    log("  catalogue %x: %u records, %u loaded; categories %x: %u", ptrv(recs), n, loaded, ptrv(cats), ncat);
    if (!recs || !cats)
        return;
    for (DWORD d = 0; d <= n; d++) {
        const DWORD i = d < n ? d : 0;
        if (!recs[i].object || recs[i].loading)
            continue;
        memset(item, 0, sizeof item);
        *reinterpret_cast<DWORD *>(item + 8) = d < n ? d : n + 7;
        DWORD r[2][3];
        const BYTE *def = static_cast<const BYTE *>(recs[i].object);
        const DWORD c = *reinterpret_cast<const DWORD *>(def + 0x18);
        const DWORD ci = c < ncat ? c : 0;
        const bool category = cats[ci].object && !cats[ci].loading;
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side][0] = at<DWORD(THISCALL *)(void *)>(0x4dd480)(item);
            r[side][1] = category ? at<DWORD(THISCALL *)(void *)>(0x4dd580)(item) : 0;
            ZString path;
            at<void *(THISCALL *)(ZString *)>(0x4dec90)(&path);
            const DWORD al = at<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4d82e0)(at<void *>(0x6c0c08), def, ptrv(&path)) & 0xff;
            r[side][2] = al ^ fnv_text(2166136261u, path.c_str());
            at<void(THISCALL *)(ZString *)>(0x4defd0)(&path);
            side_end(side);
        }
        char text[100];
        wsprintfA(text, "[record %u: %x %x %x / %x %x %x]", d, r[0][0], r[0][1], r[0][2], r[1][0], r[1][1], r[1][2]);
        t.check(memcmp(r[0], r[1], sizeof r[0]) == 0, "item references and slot 35", text);
        cases++;
    }
    // slot 35 with an object that is no definition (a category) and with no record: al 0
    for (DWORD k = 0; k < ncat; k++) {
        if (!cats[k].object)
            continue;
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side] = at<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4d82e0)(at<void *>(0x6c0c08), cats[k].object, 0) & 0xff;
            side_end(side);
        }
        char text[80];
        wsprintfA(text, "[category %u: %x / %x]", k, r[0], r[1]);
        t.check(r[0] == r[1], "slot 35 with a category", text);
        cases++;
    }
}

}  // namespace

ZAX_CHECKS(inv_actions)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_inv_actions;
    if (!arena_setup()) {
        log("Check inv actions: no arena");
        return;
    }
    init_world_tables();
    Tally objects{"inv actions: Create, CreateArray, constructors, destructors"}, getters{"inv actions: class getters"},
        fields{"inv actions: RegisterFields"}, registered{"inv actions: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    for (int kind = 0; kind < 4; kind++)
        for (unsigned k = 0; k < (unsigned)scaled(30); k++)
            test_direct(objects, kind, 500u * (unsigned)kind + k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"inv actions: execute, the slot 28s, the item references, slot 35"};
    for (int k = 0; k < NCASES; k++) {
        log("  %s", CASES[k].name);   // the last line before a case that ends the game
        for (unsigned s = 0; s < (unsigned)scaled(400); s++)
            test_case(behaviour, k, 7000u * (unsigned)k + s);
        log("    %d cases with al 1", g_ones[k]);
    }
    behaviour.report();

    Tally real{"inv actions: the real catalogue's records (real data)"};
    int cases = 0;
    test_real(real, cases);
    log("  %d real-data cases", cases);
    real.report();
}

}  // namespace zax
