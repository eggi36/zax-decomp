// Checks of the skeleton AI and its actions (ai_skeleton.cpp) against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): getters (both copies), Create/CreateArray with slot 1,
//    RegisterFields and the registered descriptors, the registered class infos; CSkeletonBaseAI's level
//    destructor and deleting destructor on an object built up to that level; the transition list's
//    append and deleting destructor on lists of 0-9 elements (the C heap from the arena).
//  - The slots and helpers in the fake world (checks_ai_world.h): the object under test a copy of a
//    template made by Create with random fields: the failed list's data and the transitions in extra
//    fakes, the state AIs, actions, the embedded behaviour, entities and messages fakes. The actions' skeleton
//    AIs (what the target's slot 64 gives) are fakes prepared like a skeleton AI (no failed list, no
//    transitions, the state AIs fakes), so the AI's setters run on them.
//  - Real data: the level's AIs of these classes (copies, their pointers and transitions replaced by
//    fakes) through their slots.
#include "checks_ai_misc.h"

namespace zax {

int hooks_ai_skeleton(Hook *out);

namespace {

using namespace aiw;
using namespace aimisc;

constexpr DWORD AI_FIELDS[] = {0x28, 0x60, 0x70, 0x80};   // the state AIs
constexpr DWORD FLAG_BYTES[] = {0x34, 0x35, 0x58, 0x59, 0x94};
constexpr DWORD MSG_SEEN = 0x713df8, MSG_DAMAGE = 0x6bd708, MSG_REINFORCE = 0x7130c0, MSG_AI_DONE = 0x6c29d8;

BYTE *bp_(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
BYTE *g_failed_data;
DWORD g_failed_bytes;
void free_failed_data()
{
    if (g_failed_data && fl<DWORD>(obj(O_SELF), 0x14) == ptrv(g_failed_data))
        pool_get_free(g_failed_data, g_failed_bytes);
    g_failed_data = nullptr;
}
DWORD fake_or_zero(Gen &g, int first, int count, int null_one_in)
{
    return g.next() % (DWORD)null_one_in == 0 ? 0 : ptrv(obj(first + (int)(g.next() % (DWORD)count)));
}
// The fake AIs prepared as skeleton AIs: no failed list or transitions, the state AIs fakes or 0, a state
void prepare_ai_fakes(Gen &g)
{
    for (int i = 0; i < N_AI; i++) {
        BYTE *a = obj(O_AI + i);
        fl<DWORD>(a, 0x14) = 0;
        fl<DWORD>(a, 0x18) = 0;
        fl<DWORD>(a, 0xa0) = 0;
        for (DWORD off : AI_FIELDS)
            fl<DWORD>(a, off) = fake_or_zero(g, O_AI, N_AI, 3);
        fl<DWORD>(a, 0xa8) = g.next() % 6;
    }
}

constexpr DWORD ACTION_FIELDS[] = {0x2c, 0x30, 0x64, 0x68, 0x74, 0x78, 0x84, 0x88, 0x90};
void fill_skeleton(Gen &g, BYTE *s, bool real)
{
    prepare_ai_fakes(g);
    // the failed list: a recorder vtable, its data fakes
    DWORD nf = real ? fl<DWORD>(s, 0x18) : g.next() % 4;
    if (nf > 8)
        nf = 8;
    // the data a real pool block (the attack state gives it back to the pool), freed after the side when it
    // is still there (the pool gives the other side the same block)
    fl<DWORD>(s, 0x10) = ptrv(g_fvt[T_OTHER]);
    BYTE *data = nf ? static_cast<BYTE *>(pool_get_alloc(nf * 4)) : nullptr;
    if (data)
        memcpy(data, fake_pointers(0, nf, g, O_ENTITY, N_ENTITY, 0), nf * 4);
    g_failed_data = data;
    g_failed_bytes = nf * 4;
    fl<DWORD>(s, 0x14) = ptrv(data);
    fl<DWORD>(s, 0x18) = nf;
    fl<DWORD>(s, 0x1c) = nf;
    fl<DWORD>(s, 0x24) = ptrv(any_entity(g));
    for (DWORD off : AI_FIELDS)
        if (!real || fl<DWORD>(s, off))
            fl<DWORD>(s, off) = real ? ptrv(obj(O_AI + (int)(g.next() % N_AI))) : fake_or_zero(g, O_AI, N_AI, 4);
    for (DWORD off : ACTION_FIELDS)
        if (!real || fl<DWORD>(s, off))
            fl<DWORD>(s, off) = real ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : fake_or_zero(g, O_OTHER, N_OTHER, 3);
    fl<DWORD>(s, 0x38) = ptrv(g_fvt[T_BEHAVIOR]);   // the embedded behaviour's slots: recorders
    fl<DWORD>(s, 0x44) = 0;
    // the transitions: copies in extra 1 (the real ones' actions fakes)
    BYTE *tr = extra(1);
    DWORD nt = real ? fl<DWORD>(s, 0xa0) : g.next() % 7;
    if (nt > 8)
        nt = 8;
    const BYTE *real_tr = real && readable(reinterpret_cast<const void *>(static_cast<uintptr_t>(fl<DWORD>(s, 0x9c))), nt * 0x1c)
                              ? reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(fl<DWORD>(s, 0x9c)))
                              : nullptr;
    if (real && !real_tr)
        nt = 0;
    for (DWORD i = 0; i < nt; i++) {
        BYTE *t = tr + i * 0x1c;
        if (real_tr) {
            memcpy(t, real_tr + i * 0x1c, 0x1c);
            fl<DWORD>(t, 4) = 0;
            if (fl<DWORD>(t, 0x14))
                fl<DWORD>(t, 0x14) = ptrv(obj(O_OTHER + (int)(i % N_OTHER)));
        } else {
            fl<DWORD>(t, 0) = 0x6007c4;
            fl<DWORD>(t, 8) = g.next() % 3;
            fl<DWORD>(t, 0xc) = g.next() % 6;
            fl<DWORD>(t, 0x10) = g.next() % 8;
            fl<DWORD>(t, 0x14) = fake_or_zero(g, O_OTHER, N_OTHER, 3);
            fl<BYTE>(t, 0x18) = (BYTE)(g.next() % 3 == 0);
        }
    }
    fl<DWORD>(s, 0x9c) = ptrv(tr);
    fl<DWORD>(s, 0xa0) = nt;
    fl<DWORD>(s, 0xa4) = nt;
    if (!real) {
        fl<DWORD>(s, 0x20) = g.next() % 3 ? g.fbits_(-0.5f, 3.0f) : 0;
        for (DWORD off : FLAG_BYTES)
            fl<BYTE>(s, off) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x48) = g.next() % 900;
        fl<DWORD>(s, 0x4c) = g.fbits_(0.0f, 3.2f);
        fl<DWORD>(s, 0x50) = g.next() % 4 ? g.next() : 0;
        fl<DWORD>(s, 0x5c) = g.fbits_(0.0f, 1500.0f);
        fl<DWORD>(s, 0x7c) = g.fbits_(0.0f, 600.0f);
        fl<DWORD>(s, 0x8c) = g.fbits_(0.0f, 100.0f);
        fl<DWORD>(s, 0xa8) = g.next() % 6;
    }
}
void fill_transition(Gen &g, BYTE *s, bool) { fl<DWORD>(s, 0x10) = g.next() % 8; }
void fill_action(Gen &g, BYTE *s, bool real)
{
    prepare_ai_fakes(g);
    if (!real || fl<DWORD>(s, 0xc))
        fl<DWORD>(s, 0xc) = real ? ptrv(obj(O_OTHER)) : fake_or_zero(g, O_OTHER, N_OTHER, 4);
}
void fill_action_value(Gen &g, BYTE *s, bool real)
{
    prepare_ai_fakes(g);
    if (!real)
        fl<DWORD>(s, 0xc) = g.next() % 3 ? g.fbits_(0.0f, 2000.0f) : g.next();
}
void fill_none(Gen &g, BYTE *, bool) { prepare_ai_fakes(g); }

enum {
    C_TRANSITION, C_BASE, C_SKELETON, C_SHOOT_BEHAVIOR, C_SET_PATROL, C_SET_PURSUE, C_SET_ATTACK, C_REINFORCE, C_SET_TARGET_TYPE, C_SET_TARGET,
    NCLASSES
};
const SlotClass CLASSES[NCLASSES] = {
    {{"CSkeletonTransition", 0x1c, 0x713638, 0x574c60, 0x574c90, 0x574d00, {0x574c10, 0x574c10}, true}, 0x6007c4, fill_transition},
    {{"CSkeletonBaseAI", 0x10, 0x713644, 0, 0, 0, {0x577d00, 0x577d50}, false}, 0x6008e0, fill_none},
    {{"CSkeletonAI", 0xac, 0x71363c, 0x575000, 0x575070, 0x575330, {0x574fa0, 0x5752c0}, true}, 0x600820, fill_skeleton},
    {{"CShootEntityBehavior", 0x14, 0x713668, 0x577ea0, 0x577ee0, 0, {0x577e50, 0x577fc0}, true}, 0x60098c, fill_none},
    {{"CSetPatrolAIAction", 0x10, 0x713580, 0x573670, 0x5736b0, 0x573820, {0x573620, 0x5737c0}, true}, 0x600524, fill_action},
    {{"CSetPursueAIAction", 0x10, 0x713584, 0x5739f0, 0x573a30, 0x573ba0, {0x5739a0, 0x573b40}, true}, 0x600594, fill_action},
    {{"CSetAttackAIAction", 0x10, 0x713588, 0x573d70, 0x573db0, 0x573f20, {0x573d20, 0x573ec0}, true}, 0x600604, fill_action},
    {{"CCallForReinforcementsAction", 0x10, 0x71358c, 0x5740f0, 0x574130, 0x5742a0, {0x5740a0, 0x574240}, true}, 0x600674, fill_action_value},
    {{"CSetTargetTypeAction", 0x10, 0x713590, 0x574440, 0x574480, 0x5745f0, {0x5743f0, 0x574590}, true}, 0x6006e4, fill_action_value},
    {{"CSetTargetAction", 0x10, 0x713594, 0x5747a0, 0x5747e0, 0x574960, {0x574750, 0x574900}, true}, 0x600754, fill_none},
};

void a_value(Gen &g, DWORD *a) { a[0] = g.next(); }
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.fbits_(0.0f, 0.2f);
}
void a_entity_other(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 6));
}
void a_entity_state(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.next() % 8;
}
void a_transitions(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.next() % 6;
    a[2] = g.next() % 8;
    a[3] = ptrv(any_entity(g, 4));
}
void a_run_action(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(obj(O_SELF) + ACTION_FIELDS[g.next() % 9]);
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 5));
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next();
}
void a_message(Gen &g, DWORD *a)
{
    const DWORD ids[] = {MSG_SEEN, MSG_DAMAGE, MSG_REINFORCE, MSG_AI_DONE};
    a[0] = ptrv(any_entity(g));
    BYTE *m = fake_message(2, g, ids, 4);
    fl<DWORD>(m, 0xc) = g.next() % 4 ? ptrv(any_entity(g)) : 0;
    fl<DWORD>(m, 0x10) = g.next() % 4 ? ptrv(any_entity(g)) : 0;
    fl<DWORD>(m, 0x14) = g.next();
    fl<DWORD>(m, 0x18) = g.next() % 4 ? ptrv(any_entity(g)) : 0;
    a[1] = ptrv(m);
}
void a_on_message(Gen &g, DWORD *a)
{
    const DWORD texts[] = {0x60aef8 /* "Delete" */, 0x60aee8 /* "Leaving Scope" */, 0x610178 /* "Died" */, 0x61968c /* "Shield" */};
    const DWORD k = g.next() % 3;
    // the sender: one on the failed list, the entity's target (a fake entity) or 0
    a[0] = k == 0 && fl<DWORD>(obj(O_SELF), 0x18) ? fl<DWORD>(bp_(fl<DWORD>(obj(O_SELF), 0x14)), 0) : (k == 1 ? 0 : ptrv(any_entity(g)));
    a[1] = texts[g.next() % 4];
}
void a_sight(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = ptrv(extra(3));
    a[2] = ptrv(extra(3) + 4);
}
void a_entity_ai(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(obj(O_AI + (int)(g.next() % N_AI)));
}
void a_entity_mask(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.next();
}
void a_target(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.next();
    a[2] = ptrv(any_entity(g));
    a[3] = ptrv(any_entity(g, 6));
}
void a_target_nonnull(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next();
    a[2] = g.next();
    a[3] = ptrv(any_entity(g));
}

const ClassSlot SLOTS[] = {
    {C_TRANSITION, {"CSkeletonTransition::SetToState", 0x574f90, 1, 0, 30, a_value}},
    {C_SKELETON, {"CSkeletonAI::RunTransitions", 0x577a90, 4, 0, 400, a_transitions}},
    {C_SKELETON, {"CSkeletonAI::RunAction", 0x577970, 2, 0, 100, a_run_action}},
    {C_SKELETON, {"AIMessage_Ctor", 0x577950, 1, 0xffffffff, 30, a_value}},
    {C_SKELETON, {"CSkeletonAI::SetState", 0x5776d0, 2, 0, 600, a_entity_state}},
    {C_SKELETON, {"CSkeletonAI::TargetSet", 0x5779a0, 1, 0, 300, a_entity}},
    {C_SKELETON, {"CSkeletonAI::SetTarget", 0x576e50, 2, 0, 300, a_entity_other}},
    {C_SKELETON, {"CSkeletonAI::TooFar", 0x576f80, 2, 0xff, 300, a_entity_other}},
    {C_SKELETON, {"CSkeletonAI::Notice", 0x576c50, 2, 0xff, 500, a_entity_other}},
    {C_SKELETON, {"CSkeletonAI::Begin", 0x576200, 1, 0, 200, a_entity}},
    {C_SKELETON, {"CSkeletonAI::Collide", 0x576260, 5, 0xff, 300, a_collide}},
    {C_SKELETON, {"CSkeletonAI::Message", 0x5762e0, 2, 0xff, 800, a_message}},
    {C_SKELETON, {"CSkeletonAI::OnMessage", 0x5765c0, 2, 0, 400, a_on_message}},
    {C_SKELETON, {"CSkeletonAI::Update", 0x576730, 2, 0, 1200, a_entity_seconds}},
    {C_SKELETON, {"CSkeletonAI::SightOf", 0x577b30, 3, 0, 50, a_sight}},
    {C_SKELETON, {"CSkeletonAI::SetPatrolAI", 0x5775e0, 2, 0, 100, a_entity_ai}},
    {C_SKELETON, {"CSkeletonAI::SetPursueAI", 0x577620, 2, 0, 100, a_entity_ai}},
    {C_SKELETON, {"CSkeletonAI::SetAttackAI", 0x577660, 2, 0, 100, a_entity_ai}},
    {C_SKELETON, {"CSkeletonAI::SetTargetTypes", 0x5776b0, 2, 0, 100, a_entity_mask}},
    {C_SET_PATROL, {"CSetPatrolAIAction::Target", 0x573950, 4, 0xff, 300, a_target_nonnull}},
    {C_SET_PURSUE, {"CSetPursueAIAction::Target", 0x573cd0, 4, 0xff, 300, a_target_nonnull}},
    {C_SET_ATTACK, {"CSetAttackAIAction::Target", 0x574050, 4, 0xff, 300, a_target_nonnull}},
    {C_REINFORCE, {"CCallForReinforcementsAction::Target", 0x5743c0, 4, 0xff, 200, a_target}},
    {C_SET_TARGET_TYPE, {"CSetTargetTypeAction::Target", 0x574710, 4, 0xff, 300, a_target_nonnull}},
    {C_SET_TARGET, {"CSetTargetAction::Target", 0x574a80, 4, 0xff, 400, a_target}},
};

Module g_mod{CLASSES, NCLASSES, SLOTS, (int)(sizeof SLOTS / sizeof SLOTS[0]), {}, 0, nullptr, nullptr};

Policy policy()
{
    Policy p{};
    p.find_minus_one = 4;
    p.nonnull = 1u << T_OTHER;
    return p;
}

// ---- CSkeletonBaseAI's destructors, the transition list -----------------------------------------------------

void test_base_dtors(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(16); seed++) {
        alignas(16) BYTE o[2][0x20];
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (DWORD i = 0; i < 0x20; i += 4)
                fl<DWORD>(o[side], i) = random();
            at<void *(THISCALL *)(void *)>(0x4fd060)(o[side]);   // CAIBase
            fl<DWORD>(o[side], 0) = 0x5ed1d0;
            at<void(THISCALL *)(void *)>(0x4173c0)(o[side]);
            fl<DWORD>(o[side], 0) = 0x6008e0;
            at<void(THISCALL *)(void *)>(0x4173c0)(o[side]);
            invcheck::side_begin(side);
            if (seed & 1)
                r[side] = (at<void *(THISCALL *)(void *, DWORD)>(0x577de0)(o[side], 0) == o[side]) ? 1 : 0;   // flag 0: not freed
            else
                r[side] = (at<void(THISCALL *)(void *)>(0x577e20)(o[side]), 1);
            invcheck::side_end(side);
        }
        t.check(r[0] == r[1] && !memcmp(o[0], o[1], sizeof o[0]), "CSkeletonBaseAI", seed & 1 ? "[deleting destructor 0x577de0]" : "[destructor 0x577e20]");
    }
}

// n appends (random fields, +4 0) to a fresh list in the arena, then the list's deleting destructor with
// flag 0 (the list lives on the stack); the arenas, the list and the results compared
void test_transition_list(Tally &t)
{
    using namespace invcheck;
    for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++) {
        DWORD list[2][4], results[2][10], destroyed[2];
        const int n = (int)(seed % 10);
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            invcheck::side_begin(side);
            arena_on(side, (BYTE)(0x40 + seed));
            DWORD *l = list[side];
            l[0] = 0x6008d0;
            l[1] = l[2] = l[3] = 0;
            for (int i = 0; i < n; i++) {
                alignas(4) BYTE e[0x1c];
                for (DWORD k = 0; k < 0x1c; k += 4)
                    fl<DWORD>(e, k) = random();
                // valid enum values and no action: the list's destructor deletes the elements
                fl<DWORD>(e, 4) = 0;
                fl<DWORD>(e, 8) %= 3;
                fl<DWORD>(e, 0xc) %= 6;
                fl<DWORD>(e, 0x10) %= 8;
                fl<DWORD>(e, 0x14) = 0;
                fl<BYTE>(e, 0x18) &= 1;
                results[side][i] = at<DWORD(THISCALL *)(void *, void *)>(0x577b60)(l, e);
            }
            const DWORD data = rel(l[1], side);
            destroyed[side] = rel(aiw::ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x577cb0)(l, 0)), side) == rel(aiw::ptrv(l), side) ? 1 : 0;
            l[1] = data;
            arena_off();
            invcheck::side_end(side);
        }
        char d[160];
        wsprintfA(d, "[%d appends: data %x / %x, capacity %u / %u, arena %u / %u bytes, first difference +%x]", n, list[0][1], list[1][1], list[0][3],
                  list[1][3], g_used[0], g_used[1], g_diff_at);
        t.check(!memcmp(list[0], list[1], sizeof list[0]) && !memcmp(results[0], results[1], (DWORD)n * 4) && destroyed[0] == destroyed[1] &&
                    g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]),
                "SkeletonTransitionList", d);
    }
}

}  // namespace

ZAX_CHECKS(ai_skeleton)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_skeleton: class getters"}, objects{"ai_skeleton: Create, CreateArray, destructors"},
        fields{"ai_skeleton: RegisterFields"}, real_fields{"ai_skeleton: registered fields and classes (real data)"},
        lists{"ai_skeleton: the transition list"}, slots{"ai_skeleton: slots in the fake world"},
        registered{"ai_skeleton: registered class infos (real data)"}, real_slots{"ai_skeleton: the level's AIs through their slots (real data)"};
    SavedBytes saved_random, saved_random2, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random2, 0x6d06c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    run_classes(g_mod, hooks_ai_skeleton, "ai_skeleton", objects, getters, fields, real_fields);
    invcheck::g_hook_table = hooks_ai_skeleton;
    test_base_dtors(objects);
    test_transition_list(lists);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    lists.report();
    make_templates(g_mod);
    if (world_on()) {
        const Policy p = policy();
        g_after_side = free_failed_data;
        run_slots(g_mod, slots, p);
        run_real(g_mod, "ai_skeleton", registered, real_slots, p, 30);
        g_after_side = nullptr;
        world_off();
    } else {
        slots.check(false, "ai_skeleton", "[the fake world could not be set up]");
    }
    free_templates(g_mod);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random2);
    restore_bytes(saved_random);
    log("  ai_skeleton: %u ms", GetTickCount() - start);
}

}  // namespace zax
