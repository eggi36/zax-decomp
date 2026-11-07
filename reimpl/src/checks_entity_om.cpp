// Checks of CEntityOm (entity_om.cpp) against the original, in the game thread.
//
//  - The class (checks_inventory_common.h): Create on a pool block filled with a pattern, CreateArray with
//    0-3 elements, the deleting destructor, the class getters (both copies), RegisterFields into a fake
//    class info and against the registered descriptors, the registered class info.
//  - The slots in the fake world (checks_seq_world.h), on fake Oms (a fake vtable for the slots 37, 46
//    and 66, the turned polygon's points in a fake, angles, the eye flag, the rotation action): the turned
//    polygon (CEntityBase's polygon a fake, the polygon functions recorders, the point buffer 0x711120
//    compared), setting the polygon, the collision (the destructable's collision recording the eye flag
//    it sees), the damage scale, the facing (the movement setter a recorder) and the overlap test (the
//    temporary list a fake, FindRect and the overlap recorders).
#include "checks_inventory_common.h"
#include "checks_seq_world.h"

namespace zax {

int hooks_entity_om(Hook *out);

namespace {

const invcheck::ClassDef CLASS{"CEntityOm", 0x16c, 0x6d016c, 0x525cd0, 0x525d20, 0x525ec0, {0x525c70, 0x525e50}, true};

using namespace sqw;

DWORD angle(Gen &g) { return g.next() % 6 == 0 ? 0 : g.fbits_(-1.0f, 7.0f); }
void make_om(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_ENTITY]);
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 0x48) = g.next() % 5 ? ptrv(obj(O_LAYER)) : 0;
    fl<DWORD>(s, 0xc8) = angle(g);
    fl<DWORD>(s, 0x130) = ptrv(obj(O_POINTS + (int)(g.next() % N_POINTS)));
    fl<DWORD>(s, 0x138) = 2 + g.next() % 7;
    fl<DWORD>(s, 0x158) = g.next() % 3 == 0 ? fl<DWORD>(s, 0xc8) : angle(g);
    fl<DWORD>(s, 0x15c) = angle(g);
    fl<DWORD>(s, 0x160) = g.next() % 5 == 0 ? 0 : g.fbits_(0.0f, 3.2f);
    fl<BYTE>(s, 0x164) = (BYTE)(g.next() % 2);
    fl<DWORD>(s, 0x168) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
}

void a_none(Gen &, DWORD *) {}
void a_polygon(Gen &g, DWORD *a) { a[0] = g.next() % 6 ? ptrv(obj(O_POLY + (int)(g.next() % N_POLY))) : 0; }
void a_collide(Gen &g, DWORD *a)
{
    BYTE *p = extra(0);
    fl<DWORD>(p, 0) = g.fbits_(-500.0f, 4500.0f);
    fl<DWORD>(p, 4) = g.fbits_(-500.0f, 4500.0f);
    a[0] = g.next();
    a[1] = g.next() % 6 ? ptrv(p) : 0;
    a[2] = g.next();
    a[3] = g.next();
}
void a_damage(Gen &g, DWORD *a)
{
    a[0] = g.fbits_(-50.0f, 500.0f);
    a[1] = g.next();
}
void a_facing(Gen &g, DWORD *a)
{
    a[0] = g.next() % 4 == 0 ? fl<DWORD>(obj(O_SELF), 0xc8) : angle(g);
    a[1] = g.next() % 3;
}

const SlotTest SLOTS[] = {
    {"CEntityOm::Polygon", 0x5261c0, 0, 0xffffffff, 800, a_none},
    {"CEntityOm::SetPolygon", 0x5262f0, 1, 0, 600, a_polygon},
    {"CEntityOm::Collide", 0x526420, 4, 0xffffffff, 1200, a_collide},
    {"CEntityOm::DamageScale", 0x526550, SLOT_FLOAT | 2, 0xffffffff, 200, a_damage},
    {"CEntityOm::SetFacing", 0x526570, 2, 0, 800, a_facing},
    {"CEntityOm::Overlaps", 0x526600, 0, 0xffffffff, 800, a_none},
};

}  // namespace

ZAX_CHECKS(entity_om)
{
    (void)cls;
    (void)n;
    Tally getters{"entity_om: class getters"}, objects{"entity_om: Create, CreateArray, destructor"}, fields{"entity_om: RegisterFields"},
        real_fields{"entity_om: registered fields and class (real data)"}, slots{"entity_om: slots in the fake world"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_entity_om;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, &CLASS, 1);
    else
        objects.check(false, "entity_om", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    if ((g_modules = M_OM, world_on())) {
        Policy p{};
        for (const SlotTest &st : SLOTS)
            for (unsigned seed = 1; seed <= (unsigned)scaled(st.count); seed++)
                run_slot(slots, st, seed * 2654435761u + st.address, make_om, p);
        world_off();
    } else {
        slots.check(false, "entity_om", "[the fake world could not be set up]");
    }
    slots.report();
    restore_bytes(saved_random);
}

}  // namespace zax
