// Checks of the simple AIs (ai_simple.cpp) against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): the class getters (both copies; the class variable set and
//    0), Create and CreateArray (ai_classes.cpp's, CSendAIDoneMessage's here) with the deleting destructor
//    (slot 1), RegisterFields into a fake class info and the registered descriptors, the registered class
//    infos.
//  - The slots in the fake world (checks_ai_world.h): the object under test a copy of a template made by
//    Create with random fields, the series' AIs, actions, entities and messages fakes.
//  - Real data: the level's AIs of these classes (copies, their pointers replaced by fakes) through their
//    slots.
#include "checks_ai_misc.h"

namespace zax {

int hooks_ai_simple(Hook *out);

namespace {

using namespace aiw;
using namespace aimisc;

constexpr DWORD MSG_AI_DONE = 0x6c29d8, MSG_DEACTIVATE = 0x6c29e8;

BYTE *fake_or_null(Gen &g, int first, int count, int null_one_in)
{
    return g.next() % (DWORD)null_one_in == 0 ? nullptr : obj(first + (int)(g.next() % (DWORD)count));
}

void fill_none(Gen &, BYTE *, bool) {}
void fill_selective(Gen &g, BYTE *s, bool real)
{
    if (!real)
        fl<DWORD>(s, 0x10) = g.next() % 4 ? g.next() : 0;
}
void fill_series(Gen &g, BYTE *s, bool real)
{
    DWORD n = fl<DWORD>(s, 0x24);
    if (!real) {
        fl<BYTE>(s, 0x10) = (BYTE)(g.next() % 2);
        n = g.next() % 5;
        fl<DWORD>(s, 0x14) = g.next() % (n + 2);
        fl<DWORD>(s, 0x18) = g.next() % 3;
    }
    if (n > 16)
        n = 16;
    fl<DWORD>(s, 0x24) = n;
    fl<DWORD>(s, 0x28) = n;
    fl<DWORD>(s, 0x20) = ptrv(fake_pointers(0, n, g, O_AI, N_AI, 5));
}
void fill_action_ai(Gen &g, BYTE *s, bool real)
{
    if (!real || fl<DWORD>(s, 0x10))
        fl<DWORD>(s, 0x10) = ptrv(real ? obj(O_OTHER) : fake_or_null(g, O_OTHER, N_OTHER, 4));
    if (!real)
        fl<BYTE>(s, 0x14) = (BYTE)(g.next() % 2);
}
void fill_simple_collide(Gen &g, BYTE *s, bool real)
{
    if (!real)
        for (DWORD off = 0x10; off < 0x14; off++)
            fl<BYTE>(s, off) = (BYTE)(g.next() % 3 == 0);
    if (!real || fl<DWORD>(s, 0x14))
        fl<DWORD>(s, 0x14) = ptrv(real ? obj(O_OTHER + 1) : fake_or_null(g, O_OTHER, N_OTHER, 4));
}

enum { C_SELECTIVE, C_SERIES, C_ACTION_AI, C_SEND_DONE, C_SHOT, C_SHOOT_OVER, C_BOUNCE, C_PASS_THROUGH, C_EXPLODE, C_SIMPLE_COLLIDE, NCLASSES };
const SlotClass CLASSES[NCLASSES] = {
    {{"CSelectiveCollideAI", 0x14, 0x71335c, 0x56a8e0, 0x56a930, 0x56aaa0, {0x56a890, 0x56aa40}, true}, 0x5ff5a4, fill_selective},
    {{"CSeriesAI", 0x2c, 0x7134fc, 0x56c730, 0x56c790, 0x56c990, {0x56c680, 0x56c930}, true}, 0x5ff8c4, fill_series},
    {{"CActionAI", 0x18, 0x713500, 0x56ce70, 0x56ceb0, 0x56d020, {0x56ce10, 0x56cfc0}, true}, 0x5ff984, fill_action_ai},
    {{"CSendAIDoneMessage", 0xc, 0x713504, 0x56d1e0, 0x56d220, 0x56d390, {0x56d190, 0x56d330}, true}, 0x5ffa2c, fill_none},
    {{"CShotAI", 0x10, 0x713540, 0x56fef0, 0x56ff40, 0, {0x56fea0, 0x570070}, true}, 0x5fff54, fill_none},
    {{"CShootOverAI", 0x10, 0x713544, 0x570120, 0x570170, 0, {0x5700d0, 0x5702a0}, true}, 0x5ffffc, fill_none},
    {{"CTurnToBounceDirAI", 0x10, 0x713548, 0x570430, 0x570490, 0, {0x570380, 0x5705e0}, true}, 0x6000a4, fill_none},
    {{"CPassThroughShooterAI", 0x10, 0x71354c, 0x570810, 0x570870, 0, {0x570760, 0x5709c0}, true}, 0x60014c, fill_none},
    {{"CExplodeOnImpact", 0x10, 0x713550, 0x570bc0, 0x570c30, 0, {0x570ac0, 0x570db0}, true}, 0x6001f4, fill_none},
    {{"CSimpleCollideTriggerAI", 0x18, 0x713558, 0x570fc0, 0x571010, 0x5711a0, {0x570f70, 0x571140}, true}, 0x60029c, fill_simple_collide},
};

DWORD seconds(Gen &g) { return g.fbits_(0.0f, 0.2f); }
void a_none(Gen &, DWORD *) {}
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = seconds(g);
}
void a_entity_view(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(obj(O_VIEW));
}
void a_touch(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 5));
    a[2] = g.next();
}
// part, other (never null where the slot reads it without a test), a point, two more, a flag
void a_collide(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g));
    BYTE *p = extra(6);
    fl<DWORD>(p, 0) = g.fbits_(-2000.0f, 6000.0f);
    fl<DWORD>(p, 4) = g.fbits_(-2000.0f, 6000.0f);
    a[2] = ptrv(p);
    a[3] = g.next();
    a[4] = g.next() % 3 ? 0 : g.next();
}
void a_message(Gen &g, DWORD *a)
{
    const DWORD ids[] = {MSG_AI_DONE, MSG_DEACTIVATE};
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(fake_message(1, g, ids, 2));
}
void a_target(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next();
    a[2] = g.next();
    a[3] = ptrv(any_entity(g, 5));
}

const ClassSlot SLOTS[] = {
    {C_SELECTIVE, {"CSelectiveCollideAI::Slot33", 0x56ab30, 3, 0xffffffff, 200, a_touch}},
    {C_SERIES, {"CSeriesAI::Reset", 0x56c6d0, 0, 0xffffffff, 100, a_none}},
    {C_SERIES, {"CSeriesAI::Collide", 0x56cc00, 5, 0xffffffff, 300, a_collide}},
    {C_SERIES, {"CSeriesAI::Message", 0x56cc40, 2, 0xff, 500, a_message}},
    {C_SERIES, {"CSeriesAI::Begin", 0x56ccb0, 1, 0, 300, a_entity}},
    {C_SERIES, {"CSeriesAI::Update", 0x56ccf0, 2, 0, 300, a_entity_seconds}},
    {C_SERIES, {"CSeriesAI::Draw", 0x56cd20, 2, 0, 200, a_entity_view}},
    {C_SERIES, {"CSeriesAI::Advance", 0x56cd50, 1, 0, 400, a_entity}},
    {C_ACTION_AI, {"CActionAI::Reset", 0x56ce60, 0, 0xffffffff, 100, a_none}},
    {C_ACTION_AI, {"CActionAI::Update", 0x56d130, 2, 0xffffffff, 300, a_entity_seconds}},
    {C_SEND_DONE, {"CSendAIDoneMessage::Target", 0x56d420, 4, 0xff, 200, a_target}},
    {C_SHOOT_OVER, {"CShootOverAI::MayTouch", 0x570300, 3, 0xff, 200, a_touch}},
    {C_BOUNCE, {"CTurnToBounceDirAI::MayTouch", 0x5706a0, 3, 0xffffffff, 200, a_touch}},
    {C_BOUNCE, {"CTurnToBounceDirAI::Collide", 0x5706c0, 5, 0, 400, a_collide}},
    {C_PASS_THROUGH, {"CPassThroughShooterAI::MayTouch", 0x570a80, 3, 0xffffffff, 300, a_touch}},
    {C_EXPLODE, {"CExplodeOnImpact::Collide", 0x570ec0, 5, 0, 400, a_collide}},
    {C_SIMPLE_COLLIDE, {"CSimpleCollideTriggerAI::Collide", 0x571430, 5, 0, 500, a_collide}},
};

Module g_mod{CLASSES, NCLASSES, SLOTS, (int)(sizeof SLOTS / sizeof SLOTS[0]), {}, 0, nullptr, nullptr};

Policy policy()
{
    Policy p{};
    p.find_minus_one = 4;
    return p;
}

}  // namespace

ZAX_CHECKS(ai_simple)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_simple: class getters"}, objects{"ai_simple: Create, CreateArray, destructors"}, fields{"ai_simple: RegisterFields"},
        real_fields{"ai_simple: registered fields and classes (real data)"}, slots{"ai_simple: slots in the fake world"},
        registered{"ai_simple: registered class infos (real data)"}, real_slots{"ai_simple: the level's AIs through their slots (real data)"};
    SavedBytes saved_random, saved_random2, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random2, 0x6d06c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    run_classes(g_mod, hooks_ai_simple, "ai_simple", objects, getters, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_templates(g_mod);
    if (world_on()) {
        run_slots(g_mod, slots, policy());
        run_real(g_mod, "ai_simple", registered, real_slots, policy(), 30);
        world_off();
    } else {
        slots.check(false, "ai_simple", "[the fake world could not be set up]");
    }
    free_templates(g_mod);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random2);
    restore_bytes(saved_random);
    log("  ai_simple: %u ms", GetTickCount() - start);
}

}  // namespace zax
