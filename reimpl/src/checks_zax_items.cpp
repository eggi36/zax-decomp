// Checks of the game's own inventory item classes (zax_items.cpp) against the original, in the game
// thread (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with
//    0-3 elements, the deleting destructors, both class getters of every class, RegisterFields into a
//    fake class info and with the class variable 0, the registered descriptors (real data); CEnergyItem's
//    destructor 0x5afe30 called directly.
//  - Behaviour on a fake world set up again from the same seed before each side: an item catalogue of
//    our own (the globals 0x6c0c38/0x6c0c3c point to it during a case) with definitions (category,
//    ammunition, "Energy"); items of every class with copies of the real vtables (slot 38 recording);
//    an inventory with the real vtable; a part whose slot 2 gives a class (a part with hit points or
//    not), slot 36 the inventory and slot 98 records the healing; the world pointer 0x6c2080 swapped for
//    a fake whose slot 58 gives the energy factor. The functions of other subsystems jump to recorders:
//    the hit points 0x48d030/0x48d150, the player 0x5babc0, the local player test 0x59e6f0, the
//    automatic weapon choice 0x425a70, the count-down 0x42a450 and the count item's take 0x42aff0.
//    Compared: eax (al where the upper bytes are the FPU status word), st(0) of the float functions, the
//    event log, all bytes of the fake world and of the arena.
//  - Real data: the player's inventory (the battery and its charge, the minerals and ammunition of
//    every catalogue entry) and items made from every catalogue record that may the player take them
//    (read-only; the items are deleted again).
#include "checks_inventory_common.h"
#include "x87.h"
#include "zax_items.h"

namespace zax {

int hooks_zax_items(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CStatsInventoryItem", 0x1c, STATS_ITEM_CLASS, 0x5ae930, 0x5ae980, 0x5aeb10, {0x5aeab0, 0}, true},
    {"CHealthItem", 0x2c, HEALTH_ITEM_CLASS, 0x5af030, 0x5af080, 0x5af210, {0x5aefe0, 0x5af1b0}, true},
    {"CMineralInventoryItem", 0x24, MINERAL_ITEM_CLASS, 0x5af4d0, 0x5af520, 0x5af690, {0x5af480, 0x5af630}, true, true},
    {"CAmmunitionInventoryItem", 0x24, AMMUNITION_ITEM_CLASS, 0x5af750, 0x5af7a0, 0, {0x5af700, 0x5af8b0}, true},
    {"CChargerInventoryItem", 0x18, CHARGER_ITEM_CLASS, 0x5af960, 0x5af990, 0, {0x5af910, 0x5afa70}, true},
    {"CEnergyItem", 0x1c, ENERGY_ITEM_CLASS, 0x5afc20, 0x5afc50, 0x5afda0, {0x5afbd0, 0x5afd40}, true},
    {"CEnergyStorageItem", 0x20, ENERGY_STORAGE_ITEM_CLASS, 0x5aff70, 0x5affb0, 0x5b0170, {0x5afec0, 0x5b00b0}, true},
    {"CBatteryChargeInventoryItem", 0x1c, BATTERY_CHARGE_ITEM_CLASS, 0x5b03f0, 0x5b0430, 0x5b0600, {0x5b0340, 0x5b0540}, true, true},
    {"CBatteryInventoryItem", 0x1c, BATTERY_ITEM_CLASS, 0x5b0b90, 0x5b0bd0, 0, {0x5b0ae0, 0x5b0ce0}, true},
};

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NDEF = 8, NITEM = 10, NKIND = 9;

struct Record {
    BYTE name[0x78];
    void *object;   // 0x78
    BYTE keep, in_use, unknown7e, loading;
};
static_assert(sizeof(Record) == 0x80);

// The item kinds and their real vtables (index into g_item_vt)
const DWORD KIND_VTABLES[NKIND] = {INVENTORY_ITEM_VTABLE,   COUNT_ITEM_VTABLE,         HEALTH_ITEM_VTABLE,
                                   MINERAL_ITEM_VTABLE,     AMMUNITION_ITEM_VTABLE,    ENERGY_ITEM_VTABLE,
                                   ENERGY_STORAGE_ITEM_VTABLE, BATTERY_CHARGE_ITEM_VTABLE, BATTERY_ITEM_VTABLE};
enum { K_ITEM, K_COUNT, K_HEALTH, K_MINERAL, K_AMMUNITION, K_ENERGY, K_STORAGE, K_CHARGE, K_BATTERY, K_CHARGER };

struct FakePart {
    DWORD vftable;
    BYTE body[0x1c];
    CClassInfo *cls;    // slot 2
    CInventory *inv;    // slot 36
};
struct FakeWorldObject {
    DWORD vftable;
    BYTE settings[0x20];   // slot 58 gives it; +0x1c the energy factor
};

struct World {
    Record records[NDEF];
    BYTE defs[NDEF][0x60];
    BYTE items[NITEM][0x30];
    void *data[NITEM + 2];
    CInventory inv;
    FakePart part;
    FakeWorldObject world;
    DWORD accept[NITEM];   // what slot 38 of each item answers (low byte)
};
World g_world;

void *g_item_vt[NKIND + 1][64];
void *g_part_vtable[100];
void *g_world_vtable[64];
CClassInfo *g_destructable, *g_object_class;
DWORD g_primary, g_secondary;

int item_index(const void *p)
{
    for (int i = 0; i < NITEM; i++)
        if (p == g_world.items[i])
            return i + 1;
    return p ? 0x99 : 0;
}

// The recorders
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_item_38(BYTE *it, void *part)
{
    const int i = item_index(it);
    ev(0x138, (DWORD)i, part == &g_world.part);
    return result(i >= 1 && i <= NITEM ? g_world.accept[i - 1] : 0);
}
CClassInfo *THISCALL f_part_class(FakePart *p) { ev(0x402); return p->cls; }
CInventory *THISCALL f_part_inventory(FakePart *p) { ev(0x436); return p->inv; }
DWORD THISCALL f_part_heal(FakePart *, DWORD bits) { ev(0x498, bits); return result(0x98); }
BYTE *THISCALL f_world_settings(FakeWorldObject *w) { ev(0x53a); return w->settings; }

// Stand-ins for the functions of other subsystems (patched in during a case)
float g_hp_left, g_hp;
long double THISCALL s_hp_left(void *part) { ev(0x701, part == &g_world.part); return fld(g_hp_left); }
long double THISCALL s_hp(void *part) { ev(0x702, part == &g_world.part); return fld(g_hp); }
DWORD __fastcall s_player(void *part, DWORD x) { ev(0x703, part == &g_world.part, x); return 0x5e7aa000u; }
DWORD g_local;
DWORD THISCALL s_is_local(void *app, DWORD player) { ev(0x704, app == *at<void **>(0x713f14), player); return result(g_local); }
DWORD THISCALL s_choose(CInventory *inv, void *item, void *part, ZPtrArray *list)
{
    ev(0x705, inv == &g_world.inv, (DWORD)item_index(item), part == &g_world.part);
    ev(0x706, list->count, list->capacity, list->vftable);
    for (DWORD i = 0; i < list->count && i < 16; i++)
        ev(0x707, (DWORD)(uintptr_t)list->data[i]);
    return result(0x05);
}
DWORD THISCALL s_count_down(void *item, DWORD a, void *inv, DWORD seconds)
{
    ev(0x708, (DWORD)item_index(item), a ^ (inv == &g_world.inv), seconds);
    return result(0x08);
}
DWORD THISCALL s_count_take(void *item, DWORD a, void *part)
{
    ev(0x709, (DWORD)item_index(item), a, part == &g_world.part);
    return result(0x09);
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x48d030, (void *)&s_hp_left},  {0x48d150, (void *)&s_hp},           {0x5babc0, (void *)&s_player},
    {0x59e6f0, (void *)&s_is_local}, {0x425a70, (void *)&s_choose},       {0x42a450, (void *)&s_count_down},
    {0x42aff0, (void *)&s_count_take},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

void init_vtables()
{
    for (int k = 0; k <= NKIND; k++) {
        const DWORD vt = k < NKIND ? KIND_VTABLES[k] : CHARGER_ITEM_VTABLE;
        memcpy(g_item_vt[k], at<void *>(vt), sizeof(void *) * 49);
        for (int i = 49; i < 64; i++)
            g_item_vt[k][i] = (void *)&f_bad;
        g_item_vt[k][1] = (void *)&f_bad;
        g_item_vt[k][38] = (void *)&f_item_38;
    }
    for (int i = 0; i < 100; i++)
        g_part_vtable[i] = (void *)&f_bad;
    g_part_vtable[2] = (void *)&f_part_class;
    g_part_vtable[36] = (void *)&f_part_inventory;
    g_part_vtable[98] = (void *)&f_part_heal;
    for (int i = 0; i < 64; i++)
        g_world_vtable[i] = (void *)&f_bad;
    g_world_vtable[58] = (void *)&f_world_settings;
    g_destructable = at<CClassInfo *(__cdecl *)()>(0x49a930)();
    g_object_class = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
    g_primary = at<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(0x6c0800), "Primary", -1);
    g_secondary = at<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(0x6c0800), "Secondary", -1);
}

float some_float()
{
    const DWORD r = random() >> 8;
    switch (r % 11) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: { DWORD nan = 0x7fc00000u | (r >> 8 & 0xff); float f; memcpy(&f, &nan, 4); return f; }
    case 3: return -(float)(int)(r >> 12 & 0xff) / 8.0f;
    case 4: return 1e30f;
    case 5: return (float)(int)(r >> 12 & 0xf);
    default: return (float)(int)(r >> 12 & 0x3ff) / 16.0f;
    }
}
DWORD some_float_bits()
{
    const float f = some_float();
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// The world of a case; item 0 is of the given kind (the item under test)
void setup(unsigned seed, int kind0)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    for (int d = 0; d < NDEF; d++) {
        Record &r = w.records[d];
        for (int k = 0; k < 8; k++)
            r.name[k] = (BYTE)('A' + d);
        r.object = w.defs[d];
        for (DWORD k = 0; k < 0x60; k += 4)
            *reinterpret_cast<DWORD *>(&w.defs[d][k]) = random();
        const DWORD r1 = random() >> 8;
        *reinterpret_cast<DWORD *>(&w.defs[d][0x18]) = r1 % 4 == 0 ? g_primary : r1 % 4 == 1 ? g_secondary : (r1 >> 4) % 6;
        *reinterpret_cast<DWORD *>(&w.defs[d][0x2c]) = (r1 >> 8) % 3 == 0 ? 0 : (r1 >> 12) % NDEF;
        *reinterpret_cast<DWORD *>(&w.defs[d][0x4c]) = some_float_bits();
    }
    for (int i = 0; i < NITEM; i++) {
        BYTE *it = w.items[i];
        for (DWORD k = 0; k < 0x30; k += 4)
            *reinterpret_cast<DWORD *>(it + k) = random();
        const int kind = i == 0 ? kind0 : (int)((random() >> 8) % (NKIND + 1));
        *reinterpret_cast<void ***>(it) = g_item_vt[kind];
        *reinterpret_cast<DWORD *>(it + 8) = (random() >> 8) % (NDEF + 1);   // NDEF: out of range, the first record
        *reinterpret_cast<DWORD *>(it + 0x18) = (random() >> 8) % 3 ? some_float_bits() : (random() >> 8) % 50;
        it[0x1c] = (BYTE)((random() >> 8) % 3 == 0);
        it[0x20] = (BYTE)((random() >> 8) % 3 == 0);
        it[0x28] = (BYTE)((random() >> 8) % 3 == 0);
        *reinterpret_cast<DWORD *>(it + 0x24) = some_float_bits();
        w.accept[i] = (random() >> 8) & 1;
        w.data[i] = it;
    }
    CInventory &inv = w.inv;
    for (DWORD k = 0; k < sizeof inv; k += 4)
        *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(&inv) + k) = random();
    inv.vftable = INVENTORY_VTABLE;
    inv.items = {0x5ee144, w.data, (random() >> 8) % (NITEM + 1), NITEM + 2};
    w.part.vftable = ptrv(g_part_vtable);
    for (DWORD k = 0; k < sizeof w.part.body; k++)
        w.part.body[k] = (BYTE)random();
    w.part.cls = (random() >> 8) % 5 ? g_destructable : g_object_class;
    w.part.inv = &w.inv;
    w.world.vftable = ptrv(g_world_vtable);
    for (DWORD k = 0; k < sizeof w.world.settings; k += 4)
        *reinterpret_cast<DWORD *>(&w.world.settings[k]) = random();
    *reinterpret_cast<DWORD *>(&w.world.settings[0x1c]) = some_float_bits();
    g_hp_left = some_float();
    g_hp = (random() >> 8) % 3 ? g_hp_left + (float)(int)((random() >> 8) % 7) - 3.0f : some_float();
    g_local = (random() >> 8) & 1;
}

// The functions on the fake world
struct Fn {
    const char *name;
    DWORD address;
    int kind;     // the class of item 0
    bool al;      // only al is defined
    bool fpu;     // st(0) is the result
    bool no_eax;  // eax undefined
};
const Fn FNS[] = {
    {"StatsColorFont", 0x5aedf0, K_ITEM, false, false, false},
    {"Part_AmmunitionRounds", 0x5aeec0, K_ITEM, false, false, false},
    {"Inventory_MineralCount", 0x5aef60, K_ITEM, false, false, false},
    {"CHealthItem::MayTake", 0x5af390, K_HEALTH, true, false, false},
    {"CHealthItem::Take", 0x5af3e0, K_HEALTH, false, false, false},
    {"CChargerInventoryItem::CountDown", 0x5afad0, K_CHARGER, false, false, false},
    {"CEnergyItem::MaxCharge", 0x5afe50, K_ENERGY, false, true, true},
    {"CEnergyItem::Use", 0x5afe60, K_ENERGY, true, false, false},
    {"CEnergyItem::Add", 0x5afe90, K_ENERGY, false, false, true},
    {"CEnergyStorageItem::MayTake", 0x5b0250, K_STORAGE, true, false, false},
    {"CEnergyStorageItem::Take", 0x5b02c0, K_STORAGE, false, false, false},
    {"CBatteryChargeInventoryItem::MayTake", 0x5b06d0, K_CHARGE, true, false, false},
    {"CBatteryChargeInventoryItem::Take", 0x5b07d0, K_CHARGE, false, false, false},
    {"Inventory_Battery", 0x5b0da0, K_ITEM, false, false, false},
    {"Inventory_BatteryCharge", 0x5b0e50, K_ITEM, false, true, true},
};
constexpr int NFNS = sizeof FNS / sizeof FNS[0];

using F1 = DWORD(THISCALL *)(void *, DWORD);
using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using L0 = long double(THISCALL *)(void *);

DWORD call_fn(int f, long double &st)
{
    World &w = g_world;
    BYTE *it = w.items[0];
    const DWORD a = random(), x = random();
    st = 0;
    switch (f) {
    case 0: return at<DWORD(__fastcall *)(DWORD)>(0x5aedf0)((DWORD)((int)(a >> 8) % 7 - 1));
    case 1: return at<DWORD(__fastcall *)(void *, void *)>(0x5aeec0)(&w.part, w.defs[(a >> 8) % NDEF]);
    case 2: return at<DWORD(__fastcall *)(void *, DWORD)>(0x5aef60)(&w.inv, (a >> 8) % (NDEF + 2));
    case 3: return at<F1>(0x5af390)(it, (a >> 8) % 6 ? ptrv(&w.part) : 0);
    case 4: return at<F2>(0x5af3e0)(it, x, (a >> 8) % 6 ? ptrv(&w.part) : 0);
    case 5: return at<F3>(0x5afad0)(it, x, ptrv(&w.inv), some_float_bits());
    case 6: st = at<L0>(0x5afe50)(it); return 0;
    case 7: return at<F1>(0x5afe60)(it, some_float_bits());
    case 8: return at<F1>(0x5afe90)(it, some_float_bits());
    case 9: return at<F1>(0x5b0250)(it, (a >> 8) % 6 ? ptrv(&w.part) : 0);
    case 10: return at<F2>(0x5b02c0)(it, x, (a >> 8) % 6 ? ptrv(&w.part) : 0);
    case 11: return at<F1>(0x5b06d0)(it, (a >> 8) % 6 ? ptrv(&w.part) : 0);
    case 12: return at<F2>(0x5b07d0)(it, x, (a >> 8) % 6 ? ptrv(&w.part) : 0);
    case 13: return at<DWORD(THISCALL *)(void *)>(0x5b0da0)(&w.inv);
    case 14: st = at<L0>(0x5b0e50)(&w.inv); return 0;
    }
    return 0;
}

World g_after[2];
long double g_st[2];

void test_fn(Tally &t, int f, unsigned seed)
{
    DWORD r[2], raw[2];
    for (int side = 0; side < 2; side++) {
        setup(seed, FNS[f].kind);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
        void *const records = *at<void **>(0x6c0c38);
        const DWORD count = *at<DWORD *>(0x6c0c3c);
        void *const world = *at<void **>(0x6c2080);
        *at<void **>(0x6c0c38) = g_world.records;
        *at<DWORD *>(0x6c0c3c) = NDEF;
        *at<void **>(0x6c2080) = &g_world.world;
        arena_on(side, 0x5a);
        g_seed = seed * 7u + 3u;
        raw[side] = call_fn(f, g_st[side]);
        r[side] = rel(raw[side], side);
        arena_off();
        *at<void **>(0x6c2080) = world;
        *at<DWORD *>(0x6c0c3c) = count;
        *at<void **>(0x6c0c38) = records;
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_stand_patches[i]);
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    const Fn &fn = FNS[f];
    if (raw[0] == raw[1])   // equal raw values are equal before the arena mapping
        r[0] = r[1] = raw[0];
    if (fn.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    if (fn.no_eax)
        r[0] = r[1] = 0;
    const bool same_st = memcmp(&g_st[0], &g_st[1], 10) == 0;
    const bool same = r[0] == r[1] && same_st && same_log() && g_used[0] == g_used[1] &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World)) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, st %d, log %d / %d, heap %u / %u, first difference +%x: %x / %x]", seed, r[0], r[1], same_st,
              g_nlog[0] / 4, g_nlog[1] / 4, g_used[0], g_used[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, fn.name, d);
}

// CEnergyItem's destructor 0x5afe30 on an item made by Create (the C heap from the arena)
void test_energy_dtor(Tally &t, unsigned seed)
{
    BYTE obj[2][0x1c];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *o = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x5afc20)());
        *reinterpret_cast<DWORD *>(o + 0x18) = random();
        r[side] = at<DWORD(THISCALL *)(void *)>(0x5afe30)(o) == ptrv(o);
        memcpy(obj[side], o, 0x1c);
        pool_free(o, 0x1c);
        arena_off();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax is this %d / %d, first difference +%x]", seed, r[0], r[1], g_diff_at);
    t.check(r[0] == r[1] && same_region(obj[0], obj[1], 0x1c) && same_region(g_arena[0], g_arena[1], g_used[0]), "CEnergyItem::~CEnergyItem", d);
}

// ---- Real data ----------------------------------------------------------------------------------------

// A read-only function of the player's inventory or part on both sides
template <typename C> void real_case(Tally &t, const char *what, DWORD arg, bool al, C call_it)
{
    DWORD r[2];
    long double st[2] = {0, 0};
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        r[side] = call_it(st[side]);
        side_end(side);
    }
    if (al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    char d[120];
    wsprintfA(d, "[%s %x: %x / %x]", what, arg, r[0], r[1]);
    t.check(r[0] == r[1] && memcmp(&st[0], &st[1], 10) == 0, "player's inventory", d);
}

void test_real(Tally &t, int &cases)
{
    void *world = *at<void **>(0x6c2080);
    void *player = world ? at<void *(THISCALL *)(void *)>(0x4f5d80)(world) : nullptr;
    if (!player) {
        log("  no player");
        return;
    }
    CInventory *inv = at<CInventory *(THISCALL *)(void *)>(0x4267e0)(player);
    if (!inv)
        return;
    const DWORD n = *at<DWORD *>(0x6c0c3c);
    real_case(t, "battery", 0, false, [&](long double &) { return at<DWORD(THISCALL *)(void *)>(0x5b0da0)(inv); });
    real_case(t, "battery charge", 0, false, [&](long double &st) {
        st = at<L0>(0x5b0e50)(inv);
        return 0u;
    });
    cases += 2;
    for (DWORD d = 0; d < n; d++) {
        real_case(t, "minerals", d, false, [&](long double &) { return at<DWORD(__fastcall *)(void *, DWORD)>(0x5aef60)(inv, d); });
        void *def = at<void *(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(0x6c0c08), d);   // loaded (as the game does)
        if (def)
            real_case(t, "ammunition rounds", d, false,
                      [&](long double &) { return at<DWORD(__fastcall *)(void *, void *)>(0x5aeec0)(player, def); });
        cases += 2;
    }
    // items of every class may-take tests on the player (slot 32 of fresh items of definition d)
    const DWORD creates[] = {0x5af030, 0x5aff70, 0x5b03f0};
    const DWORD tests[] = {0x5af390, 0x5b0250, 0x5b06d0};
    for (int k = 0; k < 3; k++)
        for (DWORD d = 0; d < n; d += 7) {
            hooks_out();
            BYTE *it = static_cast<BYTE *>(at<void *(__cdecl *)()>(creates[k])());
            hooks_in();
            if (!it)
                continue;
            *reinterpret_cast<DWORD *>(it + 8) = d;
            real_case(t, "may take", d * 4 + (DWORD)k, true, [&](long double &) { return at<F1>(tests[k])(it, ptrv(player)); });
            hooks_out();
            vcall<void *>(it, 1, (DWORD)1);
            hooks_in();
            cases++;
        }
}

}  // namespace

ZAX_CHECKS(zax_items)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_zax_items;
    if (!arena_setup()) {
        log("Check zax items: no arena");
        return;
    }
    init_vtables();
    Tally objects{"zax items: Create, CreateArray, destructors"}, getters{"zax items: class getters"},
        fields{"zax items: RegisterFields"}, registered{"zax items: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_energy_dtor(objects, k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"zax items: item slots and helpers on fake worlds"};
    for (int f = 0; f < NFNS; f++)
        for (unsigned k = 0; k < (unsigned)scaled(300); k++)
            test_fn(behaviour, f, 5000u * (unsigned)f + k);
    behaviour.report();

    Tally real{"zax items: the player's inventory and items of the real catalogue (real data)"};
    int cases = 0;
    test_real(real, cases);
    log("  %d real-data cases", cases);
    real.report();
}

}  // namespace zax
