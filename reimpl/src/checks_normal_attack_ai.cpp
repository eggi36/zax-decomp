// Checks of CNormalAttackAI (normal_attack_ai.cpp) against the original, in the game thread.
//
//  - The class (checks_inventory_common.h): the class getters (both copies), Create and CreateArray
//    (ai_classes.cpp's) with the deleting destructor, RegisterFields into a fake class info and against
//    the registered descriptors, the registered class info.
//  - The slots in the fake world (checks_seq_world.h): Begin, the random place, Update, the debug drawing
//    and the messages, on AIs with random timers and flags, the embedded shoot and turn behaviours and
//    the move behaviour fakes (or no move behaviour), entities that are animated (IsKindOf affirms
//    CEntityAnimated; the others random), with targets, inventories, items, weapons, the property
//    0x62ea2c, the warp effect and "Death" tests all recorders.
//  - Real data: copies of the level's CNormalAttackAIs (their behaviours replaced by fakes) through the
//    slots, at most 30 (the first, the last and every k-th in level order).
#include "checks_inventory_common.h"
#include "checks_seq_world.h"

namespace zax {

int hooks_normal_attack_ai(Hook *out);

namespace {

const invcheck::ClassDef CLASS{"CNormalAttackAI", 0x4c, 0x6d0134, 0x522b90, 0x522bf0, 0x522da0, {0x522b40, 0x522d40}, true};

using namespace sqw;

BYTE *g_real_self;   // a copy of a real AI (null: a made-up one)

// The AI under test: the vtable, the embedded behaviours as fakes, the move behaviour a fake or 0, the
// distances (0 too), the timers and flags
void make_ai(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_real_self)
        memcpy(s, g_real_self, 0x4c);
    else
        fl<DWORD>(s, 0) = 0x5fb6e0;
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 0xc) = 0;
    fl<DWORD>(s, 0x10) = ptrv(g_fvt[T_BEHAVIOR]);
    fl<DWORD>(s, 0x24) = ptrv(g_fvt[T_BEHAVIOR]);
    if (!g_real_self || fl<DWORD>(s, 0x30))
        fl<DWORD>(s, 0x30) = g.next() % 5 ? ptrv(obj(O_BEHAVIOR + (int)(g.next() % N_BEHAVIOR))) : 0;
    if (!g_real_self) {
        fl<DWORD>(s, 0x34) = g.next() % 4 ? g.fbits_(10.0f, 300.0f) : 0;
        fl<DWORD>(s, 0x38) = g.next() % 4 ? g.fbits_(1.0f, 120.0f) : 0;
        fl<BYTE>(s, 0x3c) = (BYTE)(g.next() % 2);
    }
    fl<DWORD>(s, 0x40) = g.next() % 3 ? g.fbits_(-1.0f, 3.0f) : 0;
    for (DWORD off = 0x44; off < 0x48; off++)
        fl<BYTE>(s, off) = (BYTE)(g.next() % 2);
    fl<DWORD>(s, 0x48) = g.next() % 3 ? g.fbits_(-0.5f, 0.6f) : 0;
}

DWORD seconds(Gen &g) { return g.fbits_(0.0f, 0.2f); }
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_entity_maybe(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g, 6)); }
void a_update(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = seconds(g);
}
void a_draw(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(obj(O_VIEW));
}
void a_message(Gen &g, DWORD *a)
{
    BYTE *m = extra(0);
    fl<DWORD>(m, 0) = 0x5eeb64;
    const DWORD k = g.next() % 3;
    fl<DWORD>(m, 4) = k == 0 ? *at<DWORD *>(0x6c29e8) : k == 1 ? *at<DWORD *>(0x6c29d4) : g.next();
    a[0] = ptrv(any_entity(g, 6));
    a[1] = ptrv(m);
}

const SlotTest SLOTS[] = {
    {"CNormalAttackAI::Begin", 0x523150, 1, 0, 300, a_entity_maybe},
    {"CNormalAttackAI::RandomPlace", 0x5231d0, 1, 0, 400, a_entity},
    {"CNormalAttackAI::Update", 0x5232c0, 2, 0, 3000, a_update},
    {"CNormalAttackAI::Draw", 0x523730, 2, 0, 300, a_draw},
    {"CNormalAttackAI::Message", 0x523800, 2, 0xff, 400, a_message},
};
constexpr int NSLOTS = sizeof SLOTS / sizeof SLOTS[0];

Policy policy()
{
    Policy p{};
    p.always[0] = at<CClassInfo *(__cdecl *)()>(0x48dcc0)();   // CEntityAnimated: the slots read every entity as one
    p.nonnull = 1u << T_INVENTORY;   // the slots use an entity's inventory without a test
    return p;
}

BYTE *g_parts[8192];

void test_real(Tally &t)
{
    const int n = level_parts(g_parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  normal_attack_ai real data: no level grid found");
        return;
    }
    const CClassInfo *ci = *at<CClassInfo **>(0x6d0134);
    int total = 0, taken = 0, ran = 0;
    for (int pass = 0; pass < 2; pass++) {
        int seen = 0;
        for (int i = 0; i < n; i++) {
            BYTE *ais[64];
            const int count = part_ais(g_parts[i], g_parts, n, ais, 64);
            for (int k = 0; k < count; k++) {
                if (class_of(ais[k]) != ci || !readable(ais[k], 0x4c))
                    continue;
                if (pass == 0) {
                    total++;
                    t.check(fl<DWORD>(ais[k], 0) == 0x5fb6e0, "CNormalAttackAI", "[vtable]");
                    continue;
                }
                if (!spread_pick(seen++, total, 30))
                    continue;
                taken++;
                g_real_self = ais[k];
                for (int s = 0; s < NSLOTS; s++)
                    for (unsigned seed = 1; seed <= 3; seed++) {
                        run_slot(t, SLOTS[s], seed * 40503u + ptrv(ais[k]), make_ai, policy());
                        ran++;
                    }
                g_real_self = nullptr;
            }
        }
    }
    log("  normal_attack_ai real data: %d parts; CNormalAttackAI %d (%d run); %d cases", n, total, taken, ran);
}

}  // namespace

ZAX_CHECKS(normal_attack_ai)
{
    (void)cls;
    (void)n;
    Tally getters{"normal_attack_ai: class getters"}, objects{"normal_attack_ai: Create, CreateArray, destructor"},
        fields{"normal_attack_ai: RegisterFields"}, real_fields{"normal_attack_ai: registered fields and class (real data)"},
        slots{"normal_attack_ai: slots in the fake world"}, real{"normal_attack_ai: the level's AIs through their slots (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_normal_attack_ai;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, &CLASS, 1);
    else
        objects.check(false, "normal_attack_ai", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    if ((g_modules = M_ATTACK, world_on())) {
        for (const SlotTest &st : SLOTS)
            for (unsigned seed = 1; seed <= (unsigned)scaled(st.count); seed++)
                run_slot(slots, st, seed * 2654435761u + st.address, make_ai, policy());
        test_real(real);
        world_off();
    } else {
        slots.check(false, "normal_attack_ai", "[the fake world could not be set up]");
    }
    slots.report();
    real.report();
    restore_bytes(saved_random);
}

}  // namespace zax
