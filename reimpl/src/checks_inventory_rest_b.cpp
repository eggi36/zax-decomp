// Checks of inventory_rest_b.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): CEnergyItemDefinition's two getters, CEnergyMeter's getter,
//    their RegisterFields with the registered descriptors, Create on a filled pool block and CreateArray
//    with 0-3 elements (through the meter's constructor 0x486c20) and the deleting destructors (through the
//    meter's destructor 0x486cf0). The colour defaults of CEnergyMeter's RegisterFields are colours on the
//    stack whose byte 0xa (the palette index) the colour constructor 0x53f010 leaves; the descriptors copy
//    it, so it is set to the registered descriptor's before the comparisons.
//  - The five getters of CZaxInventoryItemDefinition on random blocks.
//  - The weapon's firing in the fake world (checks_rest_b_world.h): slot 27 (the root a character or not,
//    no item, slot 28 doing it all, a strike, one projectile or the definition's shot entries, a failing
//    projectile), the strike and the projectile on their own (the shoot sequence, an explosion, an enemy's
//    aim at the part in front, the root passable or not, the player's velocity added).
#include "checks_rest_b_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_inventory_rest_b(Hook *out);

namespace {

using namespace rbw;
using Getter = CClassInfo *(__cdecl *)();

// The colour defaults' byte 0xa (+0x32 in the 0x3c-byte colour descriptor, vtable 0x5efe30) as the
// registered descriptor has it
void mask_meter(CClassInfo *c)
{
    const CClassInfo *k = *at<CClassInfo **>(0x6bdce0);
    for (DWORD i = c->num_inherited; i < c->num_fields && k && i < k->num_fields; i++)
        if (CFieldDesc *f = c->fields[i])
            if (f->vftable == 0x5efe30 && k->fields[i])
                reinterpret_cast<BYTE *>(f)[0x32] = reinterpret_cast<const BYTE *>(k->fields[i])[0x32];
}

const invcheck::ClassDef CLASSES[] = {
    {"CEnergyItemDefinition", 0x174, 0x6bdcd8, 0x4862d0, 0x486310, 0x4864b0, {0x486220, 0x486440}, true},
    {"CEnergyMeter", 0x68, 0x6bdce0, 0x486740, 0x486760, 0x4868a0, {0x486840, 0}, true, false, &mask_meter},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

template <typename F> void two_sides(F f)
{
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        f(side);
        invcheck::side_end(side);
    }
}

void test_getters(Tally &t)
{
    const DWORD fns[] = {0x486280, 0x486290, 0x4862a0, 0x4862b0, 0x4862c0};
    alignas(16) BYTE block[0x100];
    for (unsigned k = 0; k < (unsigned)scaled(20); k++) {
        g_seed = 0x486280 + k;
        for (DWORD i = 0; i < sizeof block; i += 4)
            *reinterpret_cast<DWORD *>(block + i) = random();
        DWORD r[2][5];
        two_sides([&](int side) {
            for (int i = 0; i < 5; i++)
                r[side][i] = at<DWORD(THISCALL *)(void *)>(fns[i])(block);
        });
        t.check(!memcmp(r[0], r[1], sizeof r[0]), "CZaxInventoryItemDefinition getters", "");
    }
}

// ---- The weapon in the fake world -------------------------------------------------------------------------------

void make_action(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_ACTION]);
    fl<DWORD>(s, 8) = g.fbits_(-30.0f, 30.0f);
    fl<DWORD>(s, 0xc) = g.fbits_(-30.0f, 30.0f);
    fl<DWORD>(s, 0x10) = g.fbits_(-20.0f, 60.0f);
    fl<DWORD>(s, 0x18) = g.fbits_(-0.5f, 1.2f);
    s[0x1c] = (BYTE)(g.next() % 2);
}
// slot 27: layer, part, a3, a4
void a_discharge(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    a[1] = ptrv(any_entity(g));   // the exe reads the part's facing without a test
    a[2] = g.next();
    a[3] = g.next();
}
// the strike and the projectile: layer, root, definition, angle, spread, dx, dy
void a_fire(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    a[1] = ptrv(any_entity(g));
    a[2] = ptrv(obj(O_DEF + (int)(g.next() % N_DEF)));
    a[3] = g.fbits_(0.0f, 6.3f);
    a[4] = g.fbits_(-30.0f, 30.0f);
    a[5] = g.fbits_(-20.0f, 20.0f);
    a[6] = g.fbits_(-20.0f, 20.0f);
}

const SlotTest WEAPON[] = {
    {"Weapon_DischargeThisFrame", 0x4c5270, 4, 0xff, 1500, a_discharge},
    {"Weapon_Strike", 0x4c4e00, 7, 0xffffffff, 400, a_fire},
    {"Weapon_FireProjectile", 0x4c4ee0, 7, 0xffffffff, 1200, a_fire},
};

void test_weapon(Tally &t)
{
    for (const SlotTest &s : WEAPON) {
        const unsigned n = (unsigned)scaled(s.count);
        for (unsigned seed = 1; seed <= n; seed++) {
            Policy p{};
            // the character, the projectile and the animated part as the case wants them
            if (seed % 4) {
                p.always[0] = at<Getter>(0x49a930)();   // CEntityCharacter
                p.always[1] = at<Getter>(0x491930)();   // CEntityProjectile
            }
            if (seed % 3 == 0)
                p.always[2] = at<Getter>(0x48dcc0)();   // CEntityAnimated
            run_slot(t, s, seed * 2654435761u + s.address, make_action, p);
        }
    }
}

}  // namespace

ZAX_CHECKS(inventory_rest_b)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    invcheck::g_hook_table = hooks_inventory_rest_b;
    if (!invcheck::arena_setup()) {
        log("Check inventory_rest_b: MISMATCH (no arena)");
        return;
    }
    Tally getters{"inventory_rest_b: class getters"}, objects{"inventory_rest_b: Create, CreateArray, destructors"},
        fields{"inventory_rest_b: RegisterFields"}, real_fields{"inventory_rest_b: registered descriptors and classes (real data)"},
        small{"inventory_rest_b: item definition getters"}, slots{"inventory_rest_b: the weapon's firing in the fake world"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    for (int i = 0; i < NCLASSES; i++) {
        log("    > class %s", CLASSES[i].name);
        invcheck::test_classes(objects, getters, fields, real_fields, &CLASSES[i], 1);
    }
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    log("    > getters");
    test_getters(small);
    small.report();
    if (world_on()) {
        test_weapon(slots);
        world_off();
    } else {
        slots.check(false, "inventory_rest_b", "[the fake world could not be set up]");
    }
    slots.report();
    restore_bytes(saved_random);
    log("  inventory_rest_b: %u ms", GetTickCount() - start);
}

}  // namespace zax
