// Checks of weapons.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters, Create, CreateArray, deleting destructors,
//    RegisterFields and the registered descriptors of CContinuousFireWeaponData, CContinuousFireWeaponItem,
//    the three fire AIs, CEnergyAmmoItem and CDamageExpandingRadiusAI.
//  - The slots in the fake world (checks_weapon_world.h): the item's slots (may fire, select, deselect,
//    message, start and stop firing), the fire AIs' begin, update, messages, the beam polygon, the
//    lightning's and the particle system's beam drawing and the destructor levels (slot 1 with flag 0 on
//    a fake object), the energy ammunition's slots, the damage message, the expanding radius's update
//    and the damage in a radius with their callbacks. Every exe function they reach beyond the module is
//    a recorder: no sound is played, no entity spawned, no part damaged, the running game is not changed.
//  - Real data: the registered class infos; the level's objects of these classes (AIs of its parts,
//    items of the player's inventory) copied into the fake world and run through their slots.
#include "checks_weapon_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_weapons(Hook *out);

namespace {

using namespace ww;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CContinuousFireWeaponData", 0x20, 0x6bd6f0, 0x472440, 0x472470, 0x472560, {0x472e80, 0}, true},
    {"CContinuousFireWeaponItem", 0x20, 0x6bd6f4, 0x4728d0, 0x472900, 0x472a40, {0x472880, 0x4729e0}, true},
    {"CFireContinuousFireWeaponBaseAI", 0x34, 0x6bd6f8, 0x472f30, 0x472f80, 0x4730d0, {0x472ed0, 0x473070}, true},
    {"CFireElectricalWeaponAI", 0x40, 0x6bd6fc, 0x474880, 0x4748f0, 0x474af0, {0x4747e0, 0x474a40}, true},
    {"CFireParticleWeaponAI", 0x40, 0x6bd700, 0x475240, 0x4752b0, 0x4754d0, {0x4751a0, 0x475420}, true},
    {"CEnergyAmmoItem", 0x1c, 0x6bd704, 0x475b50, 0x475b90, 0, {0x475b00, 0x475c90}, true},
    {"CDamageExpandingRadiusAI", 0x34, 0x6bd74c, 0x4764f0, 0x476540, 0x4766b0, {0x4764a0, 0x476650}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_DATA, C_ITEM, C_BASE_AI, C_ELECTRICAL, C_PARTICLE, C_ENERGY_AMMO, C_RADIUS, C_MESSAGE, C_QUERY };
constexpr DWORD VTABLES[] = {0x5f03e4, 0x5f0440, 0x5f050c, 0x5f05bc, 0x5f0668, 0x5f0714, 0x5f08c8};

// ---- The slots ---------------------------------------------------------------------------------------------

int g_self_class;
DWORD g_slot_address;   // the slot under test (Update and DrawBeam read the explosion and the beam object without a test)
const BYTE *g_real_self;
BYTE *g_parts[8192];

DWORD time_value(Gen &g, float lo, float hi)
{
    const DWORD v = g.next() % 16;
    const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xbf800000u};
    return v < 5 ? sp[v] : g.fbits_(lo, hi);
}
DWORD seconds(Gen &g)
{
    const DWORD v = g.next() % 16;
    if (v == 0)
        return 0;
    if (v == 1)
        return 0x7fc00000u;
    if (v == 2)
        return 0xbf800000u;
    return g.fbits_(0.0f, 0.5f);
}
DWORD entity(Gen &g, int null_one_in = 0) { return ptrv(any_entity(g, null_one_in)); }
DWORD other(Gen &g, int null_one_in = 0)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER)));
}
DWORD item(Gen &g, int null_one_in = 0)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM)));
}
// A valid empty ZString at +off of the fake (the destructor levels free it)
void empty_zstring(BYTE *s, DWORD off) { at<void *(THISCALL *)(void *)>(0x4dec90)(s + off); }

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    if (c == C_MESSAGE || c == C_QUERY)
        return;
    if (g_real_self) {
        memcpy(s, g_real_self, CLASSES[c].size);
    } else {
        for (DWORD i = 0; i < 0x40; i++)
            fl<DWORD>(s, i * 4) = g.next();
    }
    fl<DWORD>(s, 0) = VTABLES[c];
    fl<DWORD>(s, 4) = 0;
    const bool used = g_slot_address == 0x4737c0 || g_slot_address == 0x474dc0 || g_slot_address == 0x4757a0 || g_slot_address == 0x474460 ||
                      g_slot_address == 0x475a50;   // these reach Update and DrawBeam (OnMessage: "I Moved")
    switch (c) {
    case C_ITEM:
        fl<DWORD>(s, 8) = g.next() % 4;
        fl<DWORD>(s, 0xc) = g.next() % 3 ? 0 : time_value(g, -1.0f, 1.0f);
        fl<DWORD>(s, 0x14) = 0;
        fl<DWORD>(s, 0x18) = g.next() % 3 ? entity(g) : 0;
        fl<DWORD>(s, 0x1c) = g.next() % 3 ? entity(g) : 0;
        break;
    case C_BASE_AI:
    case C_ELECTRICAL:
    case C_PARTICLE:
        // the pointers become fakes; a real AI keeps its values (the definition index taken modulo the
        // fake records)
        fl<DWORD>(s, 0xc) = 0;
        fl<DWORD>(s, 0x10) = item(g, 4);
        fl<DWORD>(s, 0x14) = g.next() % 4 ? entity(g) : 0;
        fl<DWORD>(s, 0x1c) = used || g.next() % 3 ? entity(g) : 0;
        empty_zstring(s, 0x20);
        if (!g_real_self) {
            fl<BYTE>(s, 8) = (BYTE)(g.next() % 6 == 0);
            fl<DWORD>(s, 0x18) = g.next() % 4;
            fl<DWORD>(s, 0x24) = time_value(g, 0.0f, 3.0f);
            fl<DWORD>(s, 0x28) = g.next() % 2 ? 0xffffffffu : 1 + g.next() % 20;
            fl<BYTE>(s, 0x2c) = (BYTE)(g.next() % 3 ? 0xff : g.next());
            fl<DWORD>(s, 0x30) = g.next() % 3 ? g.next() % 10 : g.next();
        } else {
            fl<DWORD>(s, 0x18) %= 5;
        }
        if (c != C_BASE_AI) {
            fl<DWORD>(s, 0x34) = other(g, g_real_self ? 0 : 6);
            fl<DWORD>(s, 0x38) = used || g.next() % 3 ? other(g) : 0;
            empty_zstring(s, 0x3c);
        }
        break;
    case C_ENERGY_AMMO:
        fl<DWORD>(s, 8) = g.next() % 4;
        fl<BYTE>(s, 0x11) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x14) = 0;
        fl<DWORD>(s, 0x18) = time_value(g, -5.0f, 200.0f);
        break;
    case C_RADIUS:
        fl<BYTE>(s, 0x10) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x11) = (BYTE)(g.next() % 2);
        if (!g_real_self) {
            fl<DWORD>(s, 0x14) = time_value(g, 0.0f, 200.0f);
            fl<DWORD>(s, 0x18) = time_value(g, 0.0f, 200.0f);
            fl<DWORD>(s, 0x1c) = time_value(g, 0.0f, 400.0f);
            fl<DWORD>(s, 0x20) = time_value(g, 0.1f, 2.0f);
            fl<DWORD>(s, 0x24) = time_value(g, 0.0f, 4.0f);
            fl<DWORD>(s, 0x28) = time_value(g, 0.0f, 400.0f);
            fl<DWORD>(s, 0x2c) = time_value(g, 0.0f, 6.0f);
            fl<DWORD>(s, 0x30) = g.next() % 4;
        }
        break;
    default:
        break;
    }
}

DWORD message(Gen &g)
{
    BYTE *m = obj(O_OUT);
    const DWORD ids[] = {0x6c29e8, 0x6c29d4, 0x6bde98, 0x6c29d8};
    fl<DWORD>(m, 0) = 0x5eeb64;
    const DWORD k = g.next() % 5;
    fl<DWORD>(m, 4) = k < 4 ? *at<DWORD *>(ids[k]) : g.next();
    fl<DWORD>(m, 8) = g.next() & 0xff;
    fl<DWORD>(m, 0xc) = g.next() % 2 ? entity(g) : g.next();
    return ptrv(m);
}
const char *const NAMES[] = {"I Moved", "Died", "Delete", "i moved", "DELETE", "Leaving Scope", "Other"};
void a_part(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_part_or_null(Gen &g, DWORD *a) { a[0] = entity(g, 5); }
void a_part_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = seconds(g);
}
void a_value(Gen &g, DWORD *a) { a[0] = g.next() % 3 ? entity(g) : g.next(); }
void a_part_message(Gen &g, DWORD *a)
{
    a[0] = entity(g, 6);
    a[1] = message(g);
}
void a_other_name(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    const DWORD k = g.next() % 4;
    a[0] = k == 0 ? fl<DWORD>(s, 0x14) : k == 1 ? fl<DWORD>(s, 0x38) : k == 2 ? fl<DWORD>(s, 0x1c) : entity(g);
    a[1] = ptrv(NAMES[g.next() % 7]);
}
void a_start_firing(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = g.next() % 2 ? g.next() % 8 : g.next();
}
void a_beam_polygon(Gen &g, DWORD *a)
{
    a[0] = entity(g, 6);
    a[1] = ptrv(obj(O_POLY));
}
// slot 42 (part, target, polygon, muzzle position, hit offset, height, height 2)
void a_draw_beam(Gen &g, DWORD *a)
{
    BYTE *p = obj(O_POINTS);
    for (DWORD i = 0; i < 4; i++)
        fl<DWORD>(p, i * 4) = g.fbits_(-300.0f, 300.0f);
    a[0] = entity(g);
    a[1] = g.next() % 2 ? entity(g) : 0;
    a[2] = ptrv(obj(O_POLY));
    a[3] = ptrv(p);
    a[4] = ptrv(p + 8);
    a[5] = g.next() % 4 ? g.next() % 80 : g.next();
    a[6] = a[1] ? a[5] : 0;
}
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }
void a_amount(Gen &g, DWORD *a) { a[0] = time_value(g, -10.0f, 300.0f); }
void a_take(Gen &g, DWORD *a)
{
    a[0] = g.next() % 3 ? entity(g) : 0;
    a[1] = entity(g);
}
// 0x476450 (a1, a2, instigator) on a fake message block
void a_damage_message(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next();
    a[2] = entity(g, 4);
}
// 0x477640 (ecx: the layer, edx: the centre; except, radius, damage, type, instigator, a6, a7)
void a_damage_radius(Gen &g, DWORD *a)
{
    BYTE *p = obj(O_POINTS);
    fl<DWORD>(p, 0) = g.fbits_(0.0f, 4000.0f);
    fl<DWORD>(p, 4) = g.fbits_(0.0f, 4000.0f);
    a[0] = ptrv(p);
    a[1] = entity(g, 4);
    a[2] = time_value(g, 1.0f, 300.0f);
    a[3] = time_value(g, 0.0f, 200.0f);
    a[4] = g.next() % 4;
    a[5] = entity(g, 3);
    a[6] = g.next() % 2;
    a[7] = g.next() % 3 ? 0 : g.next();
}

void free_made_ai()
{
    for (int i = 0; i < g_num_made_ai; i++)
        pool_free(g_made_ai[i], 0x34);
    g_num_made_ai = 0;
}
void after_zstrings()
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    if (c == C_BASE_AI || c == C_ELECTRICAL || c == C_PARTICLE) {
        at<void(THISCALL *)(void *)>(0x4defd0)(s + 0x20);
        if (c != C_BASE_AI)
            at<void(THISCALL *)(void *)>(0x4defd0)(s + 0x3c);
    }
}

// The policies: P_PLAIN for slots that test what they get; P_FIRE for those that use a part's root, the
// gun owner or the inventory's item without a test (the original would read address 0 otherwise)
enum { P_PLAIN, P_FIRE };
struct ClassSlot {
    int cls;
    SlotTest t;
    void (*after)();
    int policy = P_PLAIN;
};
const ClassSlot SLOTS[] = {
    {C_ITEM, {"CContinuousFireWeaponItem::MayFire", 0x472b50, 1, 0xff, 200, a_part_or_null}, nullptr},
    {C_ITEM, {"CContinuousFireWeaponItem::Select", 0x472bf0, 1, 0xffffffff, 30, a_value}, nullptr},
    {C_ITEM, {"CContinuousFireWeaponItem::Deselect", 0x472c00, 1, 0xffffffff, 120, a_value}, nullptr, P_FIRE},
    {C_ITEM, {"CContinuousFireWeaponItem::Message", 0x472c30, 2, 0xff, 200, a_part_message}, nullptr, P_FIRE},
    {C_ITEM, {"CContinuousFireWeaponItem::StartFiring", 0x472c90, 2, 0xffffffff, 300, a_start_firing}, nullptr, P_FIRE},
    {C_ITEM, {"CContinuousFireWeaponItem::StopFiring", 0x472dc0, 1, 0xffffffff, 300, a_part}, nullptr, P_FIRE},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::Begin", 0x4735c0, 1, 0, 400, a_part}, after_zstrings, P_FIRE},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::Update", 0x4737c0, 2, 0, 1200, a_part_seconds}, after_zstrings, P_FIRE},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::OnMessage", 0x474460, 2, 0xffffffff, 400, a_other_name}, after_zstrings, P_FIRE},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::BeamPolygon", 0x474510, 2, 0, 300, a_beam_polygon}, after_zstrings},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::AIMessage", 0x4746c0, 2, 0xffffffff, 300, a_part_message}, after_zstrings},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::Dtor", 0x4734e0, 0, 0, 200, nullptr}, nullptr},
    {C_BASE_AI, {"CFireContinuousFireWeaponBaseAI::DeletingDtor", 0x473000, 1, 0xffffffff, 60, a_flags0}, nullptr},
    {C_ELECTRICAL, {"CFireElectricalWeaponAI::Begin", 0x474d70, 1, 0, 300, a_part}, after_zstrings, P_FIRE},
    {C_ELECTRICAL, {"CFireElectricalWeaponAI::Update", 0x4737c0, 2, 0, 600, a_part_seconds}, after_zstrings, P_FIRE},
    {C_ELECTRICAL, {"CFireElectricalWeaponAI::DrawBeam", 0x474dc0, 7, 0, 800, a_draw_beam}, after_zstrings, P_FIRE},
    {C_ELECTRICAL, {"CFireElectricalWeaponAI::AIMessage", 0x475150, 2, 0xffffffff, 300, a_part_message}, after_zstrings},
    {C_ELECTRICAL, {"CFireElectricalWeaponAI::DeletingDtor", 0x474980, 1, 0xffffffff, 100, a_flags0}, nullptr},
    {C_PARTICLE, {"CFireParticleWeaponAI::Begin", 0x475750, 1, 0, 300, a_part}, after_zstrings, P_FIRE},
    {C_PARTICLE, {"CFireParticleWeaponAI::Update", 0x4737c0, 2, 0, 600, a_part_seconds}, after_zstrings, P_FIRE},
    {C_PARTICLE, {"CFireParticleWeaponAI::DrawBeam", 0x4757a0, 7, 0, 800, a_draw_beam}, after_zstrings, P_FIRE},
    {C_PARTICLE, {"CFireParticleWeaponAI::OnMessage", 0x475a50, 2, 0xffffffff, 300, a_other_name}, after_zstrings, P_FIRE},
    {C_PARTICLE, {"CFireParticleWeaponAI::AIMessage", 0x475aa0, 2, 0xffffffff, 300, a_part_message}, after_zstrings},
    {C_PARTICLE, {"CFireParticleWeaponAI::DeletingDtor", 0x475340, 1, 0xffffffff, 100, a_flags0}, nullptr},
    {C_ENERGY_AMMO, {"CEnergyAmmoItem::SetAmount", 0x475cf0, 1, 0, 200, a_amount}, nullptr},
    {C_ENERGY_AMMO, {"CEnergyAmmoItem::Take", 0x475d40, 2, 0xffffffff, 300, a_take}, nullptr},
    {C_ENERGY_AMMO, {"CEnergyAmmoItem::MayTake", 0x475e40, 1, 0xff, 300, a_part}, nullptr},
    {C_ENERGY_AMMO, {"CEnergyAmmoItem::DeletingDtor", 0x475c00, 1, 0xffffffff, 40, a_flags0}, nullptr},
    {C_MESSAGE, {"CDamageMessage::Ctor", 0x476450, 3, 0xffffffff, 200, a_damage_message}, nullptr},
    {C_RADIUS, {"CDamageExpandingRadiusAI::Update", 0x476b60, 2, 0, 800, a_part_seconds}, free_made_ai},
    {C_RADIUS, {"CDamageExpandingRadiusAI::DeletingDtor", 0x4765b0, 1, 0xffffffff, 40, a_flags0}, nullptr},
    {C_QUERY, {"DamageRadius_Query", 0x477640, -8, 0xffffffff, 400, a_damage_radius}, nullptr},
};

Policy weapon_policy(int kind)
{
    Policy p{};
    p.find_minus_one = 4;
    // used without a test: the definitions (as CZaxInventoryItemDefinitions, their continuous fire data),
    // the layer, the energy source, projectile, players and stats (T_OTHER)
    p.nonnull = 1u << T_DEF | 1u << T_LAYER | 1u << T_OTHER;
    p.always[0] = at<Getter>(0x5b5f10)();
    p.always[1] = at<Getter>(0x472e80)();
    if (kind == P_FIRE) {
        p.nonnull |= 1u << T_ENTITY | 1u << T_INVENTORY | 1u << T_ITEM;
        p.always[2] = at<Getter>(0x49a930)();   // CEntityCharacter: the gun owner's root
    }
    return p;
}

void run_slots(Tally &t, const ClassSlot &s, unsigned count, unsigned base)
{
    g_self_class = s.cls;
    g_slot_address = s.t.address;
    g_after_side = s.after;
    for (unsigned seed = 1; seed <= count; seed++) {
        run_slot(t, s.t, seed * 2654435761u + base, make_self, weapon_policy(s.policy));
    }
    g_after_side = nullptr;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_real_self = nullptr;
        run_slots(t, s, (unsigned)scaled(s.t.count), s.t.address);
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------

void test_real(Tally &registered, Tally &real_slots)
{
    for (const invcheck::ClassDef &c : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        registered.check(ci && ci->size == c.size && ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array &&
                             at<Getter>(c.getters[0])() == ci,
                         c.name, d);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  weapons real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, ran = 0;
    for (int i = 0; i < n; i++) {
        BYTE *ais[64];
        const int count = part_ais(parts[i], parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const CClassInfo *ci = class_of(ais[k]);
            for (int c = 0; c < NCLASSES; c++) {
                if (!ci || ci != *at<CClassInfo **>(CLASSES[c].var) || !readable(ais[k], CLASSES[c].size))
                    continue;
                if (found[c]++ >= 12)
                    continue;
                g_real_self = ais[k];
                for (const ClassSlot &s : SLOTS)
                    if (s.cls == c && s.t.nargs >= 0) {
                        g_self_class = c;
                        g_slot_address = s.t.address;
                        g_after_side = s.after;
                        for (unsigned seed = 1; seed <= 2; seed++) {
                            run_slot(real_slots, s.t, seed * 40503u + ptrv(ais[k]), make_self, weapon_policy(s.policy));
                            ran++;
                        }
                        g_after_side = nullptr;
                    }
                g_real_self = nullptr;
            }
        }
    }
    log("  weapons real data: %d parts; AIs %d base, %d electrical, %d particle, %d damage radius; %d cases", n, found[C_BASE_AI],
        found[C_ELECTRICAL], found[C_PARTICLE], found[C_RADIUS], ran);
}

}  // namespace

ZAX_CHECKS(weapons)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"weapons: class getters"}, objects{"weapons: Create, CreateArray, destructors"}, fields{"weapons: RegisterFields"},
        real_fields{"weapons: RegisterFields against the registered fields (real data)"}, slots{"weapons: slots in the fake world"},
        registered{"weapons: registered class infos (real data)"}, real_slots{"weapons: the level's objects through their slots (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_weapons;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    else
        objects.check(false, "weapons", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        test_real(registered, real_slots);
        world_off();
    } else {
        slots.check(false, "weapons", "[the fake world could not be set up]");
    }
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random);
    log("  weapons: %u ms", GetTickCount() - start);
}

}  // namespace zax
