// Checks of the HUD's render data slots (hud_data.cpp) against the original, in the game thread (machinery:
// checks_inventory_common.h). Every case builds a render data object of the class under test (its real
// vtable) with values from the case seed, then runs one slot on both sides:
//  - the packet functions (0x520490-0x520da0) are stand-ins that record what is written and hand out the
//    seed's values to the reads (bytes, words, dwords, float bits incl. NaN, 0, -0);
//  - CInventoryItemDefinition::HasAmmunition 0x4d77c0 and CZaxInventoryItemDefinition::IsContinuousFire
//    0x5b7640 record their object and answer from the seed;
//  - the item catalogue's records (0x6c0c38/0x6c0c3c) point to fake records during a case (loaded,
//    missing or loading), whose objects are fake definitions with the real CZaxInventoryItemDefinition
//    vtable or another class's; the catalogue list's vtable 0x6c0c08 is swapped for a copy whose slots 1
//    (the entry's name) and 37 (load) record, DefinitionList_LoadRecord 0x55e090 is a stand-in;
//  - CShieldHUDRenderData's elements are real CEnergyHUDRenderData objects (pool, made by the real
//    SetShieldCount before the case); their array and the elements are compared by contents.
// Compared: the event log, the object's bytes (the array's data pointer by its contents), the fake
// records, eax where the slot returns a value (slot 26, al).
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_hud_data(Hook *out);

namespace {

using namespace invcheck;

template <typename T> T &at_off(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &d32(void *p, DWORD off) { return at_off<DWORD>(p, off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
DWORD some_float_bits()
{
    const DWORD r = random() >> 8;
    switch (r % 10) {
    case 0: return 0;
    case 1: return 0x80000000u;
    case 2: return 0x7fc00000u | (r >> 8 & 0xff);
    case 3: return fbits(-(float)(int)(r >> 12 & 0xff) / 8.0f);
    case 4: return fbits(1.0f);
    default: return fbits((float)(int)(r >> 12 & 0x3ff) / 16.0f);
    }
}

constexpr int NREC = 6, NDEF = 3;
constexpr DWORD VT_ZAX_DEF = 0x604c3c, VT_OTHER = 0x5f9e98, ITEM_LIST = 0x6c0c08;

struct World {
    BYTE records[NREC][0x80];
    BYTE defs[NDEF][0x40];
    char names[NREC][16];
};
World g_world;
BYTE g_packet[16];
DWORD g_list_vt[64];
DWORD g_ammo_bits, g_cont_bits;

int fake(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p), *w = reinterpret_cast<const BYTE *>(&g_world);
    if (!p)
        return 0;
    if (p == g_packet)
        return 1;
    if (b >= w && b < w + sizeof g_world)
        return 0x10000 + (int)(b - w);
    return 0x99;
}

// The packet
DWORD THISCALL s_write_byte(void *p, DWORD v) { ev(0x801, (DWORD)fake(p), v & 0xff); return result(1); }
DWORD THISCALL s_write_word(void *p, DWORD v) { ev(0x802, (DWORD)fake(p), v & 0xffff); return result(2); }
DWORD THISCALL s_write_dword(void *p, DWORD v) { ev(0x803, (DWORD)fake(p), v); return result(3); }
DWORD THISCALL s_write_token(void *p, DWORD v, DWORD list) { ev(0x804, (DWORD)fake(p), v & 0xffff, list); return result(4); }
DWORD THISCALL s_read_byte(void *p, BYTE *v)
{
    *v = (BYTE)(random() >> 9);
    ev(0x811, (DWORD)fake(p), *v);
    return result(0x11);
}
DWORD THISCALL s_read_word(void *p, WORD *v)
{
    *v = (WORD)(random() >> 7);
    ev(0x812, (DWORD)fake(p), *v);
    return result(0x12);
}
DWORD THISCALL s_read_dword(void *p, DWORD *v)
{
    *v = random() % 3 ? some_float_bits() : random();
    ev(0x813, (DWORD)fake(p), *v);
    return result(0x13);
}
DWORD THISCALL s_read_token(void *p, DWORD *v, DWORD list)
{
    *(WORD *)v = (WORD)(random() % 3 ? random() % (NREC + 2) : random() >> 9);
    ev(0x814, (DWORD)fake(p), *v, list);
    return result(0x14);
}
// The definitions
DWORD THISCALL s_ammo(void *def)
{
    ev(0x821, (DWORD)fake(def));
    return (result(0x21) & 0xffffff00) | (g_ammo_bits >> (fake(def) & 7) & 1);
}
DWORD THISCALL s_continuous(void *def)
{
    ev(0x822, (DWORD)fake(def));
    return (result(0x22) & 0xffffff00) | (g_cont_bits >> (fake(def) & 7) & 1);
}
DWORD THISCALL s_load_record(void *list, DWORD i)
{
    ev(0x823, list == at<void *>(ITEM_LIST), i);
    if (i < NREC)
        d32(g_world.records[i], 0x78) = ptrv(g_world.defs[i % NDEF]);
    return result(0x23);
}
// The catalogue list's slots 1 (name of entry i) and 37 (load: folder, name, record field)
const char *THISCALL s_list_name(void *list, DWORD i)
{
    ev(0x824, list == at<void *>(ITEM_LIST), i);
    return g_world.names[i % NREC];
}
DWORD THISCALL s_list_load(void *list, const char *folder, const char *name, DWORD *field)
{
    ev(0x825, list == at<void *>(ITEM_LIST), fnv_text(2166136261u, folder) ^ fnv_text(7u, name), (DWORD)fake(field));
    return random() % 4 ? ptrv(g_world.defs[random() % NDEF]) : 0;
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x520820, (void *)&s_write_byte}, {0x5209c0, (void *)&s_write_word}, {0x520940, (void *)&s_write_dword},
    {0x520da0, (void *)&s_write_token}, {0x520490, (void *)&s_read_byte}, {0x520510, (void *)&s_read_word},
    {0x520550, (void *)&s_read_dword}, {0x520670, (void *)&s_read_token}, {0x4d77c0, (void *)&s_ammo},
    {0x5b7640, (void *)&s_continuous}, {0x55e090, (void *)&s_load_record},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_patches[NSTANDS];

void setup_world(unsigned seed)
{
    g_seed = seed;
    World &w = g_world;
    BYTE *all = reinterpret_cast<BYTE *>(&w);
    for (DWORD k = 0; k + 4 <= sizeof w; k += 4)
        *reinterpret_cast<DWORD *>(all + k) = random();
    for (int i = 0; i < NREC; i++) {
        const DWORD k = random() % 6;
        d32(w.records[i], 0x78) = k < 3 ? ptrv(w.defs[k]) : 0;   // loaded, or missing (loaded on use)
        w.records[i][0x7f] = (BYTE)(k == 5);                      // loading
        wsprintfA(w.names[i], "Item %d", i);
    }
    for (int i = 0; i < NDEF; i++) {
        d32(w.defs[i], 0) = i == 2 ? VT_OTHER : VT_ZAX_DEF;
        d32(w.defs[i], 4) = 0;
    }
    g_ammo_bits = random();
    g_cont_bits = random();
}

// The render data object of a case: its vtable and the base values, the rest random
BYTE g_obj[2][0x40];
enum Kind { BASE, COMPASS, ENERGY, SHIELD, WEAPON };
const DWORD VTABLES[] = {0x5ede94, 0x5ede28, 0x5ffd30, 0x5ffcb8, 0x5f1a64};
const DWORD SIZES[] = {0x14, 0x1c, 0x20, 0x28, 0x28};

void fill_object(BYTE *o, Kind k)
{
    for (DWORD i = 0; i < sizeof g_obj[0]; i += 4)
        d32(o, i) = random();
    d32(o, 0) = VTABLES[k];
    d32(o, 4) = 0;
    d32(o, 8) = some_float_bits();
    d32(o, 0xc) = random() % 4 ? random() % (NREC + 2) : random();
    d32(o, 0x10) = random() % 4;
    switch (k) {
    case COMPASS:
        o[0x14] = (BYTE)(random() % 3 ? random() % 2 : random());
        o[0x15] = (BYTE)(random() % 2);
        o[0x16] = (BYTE)(random() % 3 ? random() % 2 : random());
        d32(o, 0x18) = random() % 3 ? random() % 40 - 20 : random();
        break;
    case ENERGY:
        d32(o, 0x14) = some_float_bits();
        d32(o, 0x18) = some_float_bits();
        d32(o, 0x1c) = random() % 4;
        break;
    case SHIELD:
        d32(o, 0x14) = random() % 5;
        d32(o, 0x18) = 0x5ffd24;   // the pointer array, empty; elements added by SetShieldCount
        d32(o, 0x1c) = d32(o, 0x20) = d32(o, 0x24) = 0;
        break;
    case WEAPON:
        d32(o, 0x14) = random() % 3 ? random() % 300 : random();
        d32(o, 0x18) = some_float_bits();
        d32(o, 0x1c) = some_float_bits();
        d32(o, 0x20) = some_float_bits();
        d32(o, 0x24) = random() % 8;
        break;
    default: break;
    }
}

// The shield's elements made by the original SetShieldCount, with values of the seed
void make_elements(BYTE *o, DWORD n)
{
    reinterpret_cast<void(THISCALL *)(void *, DWORD)>(original_address(0x56ec50))(o, n);
    for (DWORD i = 0; i < d32(o, 0x20); i++) {
        BYTE *e = bp(d32(bp(d32(o, 0x1c)), i * 4));
        d32(e, 8) = some_float_bits();
        d32(e, 0xc) = random() % 4;
        d32(e, 0x10) = random() % 4;
        d32(e, 0x14) = some_float_bits();
        d32(e, 0x18) = some_float_bits();
        d32(e, 0x1c) = random() % 4;
    }
}
// The state of a side: the object's bytes (the shield's data pointer replaced by its elements' bytes)
unsigned object_state(BYTE *o, Kind k)
{
    BYTE copy[0x40];
    memcpy(copy, o, sizeof copy);
    unsigned h = 2166136261u;
    if (k == SHIELD) {
        d32(copy, 0x1c) = 0;
        d32(copy, 0x24) = 0;   // the capacity follows the heap's growth steps, the same on both sides anyway
        for (DWORD i = 0; i < d32(o, 0x20) && i < 64; i++) {
            BYTE *e = bp(d32(bp(d32(o, 0x1c)), i * 4));
            h = fnv(h, e, 0x20);
        }
    }
    return fnv(h, copy, SIZES[k]);
}
void free_elements(BYTE *o)
{
    for (DWORD i = 0; i < d32(o, 0x20); i++)
        if (BYTE *e = bp(d32(bp(d32(o, 0x1c)), i * 4)))
            vcall<DWORD>(e, 1, 1);
    if (d32(o, 0x1c))
        heap_free(bp(d32(o, 0x1c)));
    d32(o, 0x1c) = d32(o, 0x20) = d32(o, 0x24) = 0;
}

unsigned g_state[2], g_records[2];

// One case: the object made from the seed, then call(side object) on both sides
void test_slot(Tally &t, const char *name, Kind k, unsigned seed, DWORD mask, DWORD (*call)(BYTE *))
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup_world(seed);
        BYTE *o = g_obj[side];
        fill_object(o, k);
        if (k == SHIELD)
            make_elements(o, random() % 6);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
        BYTE *const records = *at<BYTE **>(0x6c0c38);
        const DWORD count = *at<DWORD *>(0x6c0c3c);
        const DWORD list_vt = *at<DWORD *>(ITEM_LIST);
        memcpy(g_list_vt, at<void *>(list_vt), sizeof g_list_vt);
        g_list_vt[1] = ptrv((void *)&s_list_name);
        g_list_vt[37] = ptrv((void *)&s_list_load);
        *at<DWORD *>(ITEM_LIST) = ptrv(g_list_vt);
        *at<BYTE **>(0x6c0c38) = random() % 12 ? g_world.records[0] : nullptr;
        *at<DWORD *>(0x6c0c3c) = NREC;
        g_seed = seed * 7u + 3u;
        r[side] = call(o) & mask;
        *at<DWORD *>(ITEM_LIST) = list_vt;
        *at<DWORD *>(0x6c0c3c) = count;
        *at<BYTE **>(0x6c0c38) = records;
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_patches[i]);
        side_end(side);
        g_state[side] = object_state(o, k);
        g_records[side] = fnv(2166136261u, &g_world, sizeof g_world);
        if (k == SHIELD)
            free_elements(o);
    }
    const bool same = r[0] == r[1] && same_log() && g_state[0] == g_state[1] && g_records[0] == g_records[1];
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, object %x / %x, records %x / %x]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4,
              g_state[0], g_state[1], g_records[0], g_records[1]);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, name, d);
}

using Slot1 = DWORD(THISCALL *)(void *, void *);
using Slot0 = DWORD(THISCALL *)(void *);
DWORD g_addr;
DWORD call_packet(BYTE *o) { return at<Slot1>(g_addr)(o, g_packet); }
DWORD call_plain(BYTE *o) { return at<Slot0>(g_addr)(o); }
DWORD call_value(BYTE *o) { return at<DWORD(THISCALL *)(void *, DWORD)>(g_addr)(o, random() % 3 ? some_float_bits() : d32(o, 8)); }
DWORD call_count(BYTE *o) { return at<DWORD(THISCALL *)(void *, DWORD)>(g_addr)(o, random() % 3 ? random() % (NREC + 2) : d32(o, 0xc)); }
DWORD call_energy_value(BYTE *o) { return at<DWORD(THISCALL *)(void *, DWORD)>(g_addr)(o, random() % 3 ? some_float_bits() : d32(o, 0x14)); }
DWORD call_energy_max(BYTE *o) { return at<DWORD(THISCALL *)(void *, DWORD)>(g_addr)(o, random() % 3 ? some_float_bits() : d32(o, 0x18)); }
DWORD call_shield_count(BYTE *o) { return at<DWORD(THISCALL *)(void *, DWORD)>(g_addr)(o, random() % 8); }

struct SlotCase {
    const char *name;
    DWORD address;
    Kind kind;
    DWORD mask;
    DWORD (*call)(BYTE *);
    int cases;
};
const SlotCase SLOTS[] = {
    {"CInventoryHUDRenderData::WriteChanged", 0x5ac8a0, BASE, 0, call_packet, 300},
    {"CInventoryHUDRenderData::WriteAll", 0x5ac900, BASE, 0, call_packet, 200},
    {"CInventoryHUDRenderData::Read", 0x5ac950, BASE, 0, call_packet, 400},
    {"CInventoryHUDRenderData::Changed", 0x5aca00, BASE, 0xffffffff, call_plain, 100},
    {"CInventoryHUDRenderData::SetValue", 0x5aca10, BASE, 0, call_value, 300},
    {"CInventoryHUDRenderData::SetCount", 0x5aca30, BASE, 0, call_count, 200},
    {"CCompassRenderData::WriteChanged", 0x423520, COMPASS, 0, call_packet, 300},
    {"CCompassRenderData::WriteAll", 0x423580, COMPASS, 0, call_packet, 200},
    {"CCompassRenderData::Read", 0x4235d0, COMPASS, 0, call_packet, 400},
    {"CCompassRenderData::Changed", 0x423650, COMPASS, 0xffffffff, call_plain, 100},
    {"CEnergyHUDRenderData::WriteChanged", 0x581710, ENERGY, 0, call_packet, 300},
    {"CEnergyHUDRenderData::WriteAll", 0x581760, ENERGY, 0, call_packet, 200},
    {"CEnergyHUDRenderData::Read", 0x5817a0, ENERGY, 0, call_packet, 400},
    {"CEnergyHUDRenderData::Changed", 0x581840, ENERGY, 0xffffffff, call_plain, 100},
    {"CEnergyHUDRenderData::SetValue", 0x581860, ENERGY, 0, call_energy_value, 300},
    {"CEnergyHUDRenderData::SetMaximum", 0x581880, ENERGY, 0, call_energy_max, 300},
    {"CShieldHUDRenderData::SetShieldCount", 0x56ec50, SHIELD, 0, call_shield_count, 300},
    {"CShieldHUDRenderData::WriteChanged", 0x56ea30, SHIELD, 0, call_packet, 300},
    {"CShieldHUDRenderData::WriteAll", 0x56eb00, SHIELD, 0, call_packet, 200},
    {"CShieldHUDRenderData::Read", 0x56eb70, SHIELD, 0, call_packet, 400},
    {"CShieldHUDRenderData::Changed", 0x56ec00, SHIELD, 0xff, call_plain, 200},
    {"CWeaponHUDRenderData::WriteChanged", 0x4851e0, WEAPON, 0, call_packet, 500},
    {"CWeaponHUDRenderData::WriteAll", 0x4853a0, WEAPON, 0, call_packet, 400},
    {"CWeaponHUDRenderData::Read", 0x485550, WEAPON, 0, call_packet, 600},
    {"CWeaponHUDRenderData::Changed", 0x485700, WEAPON, 0xffffffff, call_plain, 100},
};

}  // namespace

ZAX_CHECKS(hud_data)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_hud_data;
    Tally t{"hud data: render data slots on fake packets"};
    unsigned seed = 1;
    for (const SlotCase &s : SLOTS) {
        g_addr = s.address;
        for (int k = 0; k < scaled(s.cases); k++)
            test_slot(t, s.name, s.kind, seed++, s.mask, s.call);
    }
    t.report();
}

}  // namespace zax
