// Checks of the behaviours, AIs and fidgets of behavior_rest.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): the class getters (both copies; the class variable set and
//    0), Create and CreateArray (ai_classes.cpp's) with the deleting destructor (slot 1), RegisterFields
//    into a fake class info and the registered descriptors, the registered class infos; the out-of-line
//    destructors on objects made by Create.
//  - The behaviour and AI slots in the fake world (checks_ai_world.h): the object under test a copy of a
//    template made by Create with random fields; its embedded behaviours get the fake behaviour vtable (their
//    own slots are tested as their classes), owned behaviours and leaders are fakes, a CEntityBehaviorQueue's
//    behaviours sit in an extra fake object with room to append in place. The function-local statics of the
//    slots (the "Idle" string 0x713660, the colours 0x713650 and 0x713840) are made first, as the exe does on
//    their first call, so both sides find them made.
//  - Real data: the level's AIs of these classes (copies, their pointers replaced by fakes) through their
//    slots.
//  - CFidgetData and CFidgetSettings on their own (the lists are arrays on the C heap, compared by their
//    contents): Create'd objects with lists filled through the arrays' own append, the model lookups
//    (0x48d8f0, CollectSequences 0x5172b0, SequenceName 0x517200, Entity_Model 0x48b290 and _ModelIndex
//    0x48b560), Random_Between 0x55c4e0 and the error reporter's slot 17 are stand-ins that record; the
//    game's random generator is saved and put back around each side for the next fidget time.
#include "checks_ai_misc.h"

namespace zax {

int hooks_behavior_rest(Hook *out);

namespace {

using namespace aiw;
using namespace aimisc;

DWORD fake_vt(int type) { return ptrv(g_fvt[type]); }
BYTE *behavior_or_null(Gen &g, int null_one_in) { return g.next() % (DWORD)null_one_in == 0 ? nullptr : obj(O_BEHAVIOR + (int)(g.next() % N_BEHAVIOR)); }
DWORD angle_bits(Gen &g) { return g.next() % 6 == 0 ? 0x40c90fdb : g.fbits_(-7.0f, 7.0f); }

// A ZString in extra object 7 (the weapon's sequence name of slot 35, the name slot 30 plays)
void make_zstring(Gen &g)
{
    BYTE *z = extra(7);
    fl<DWORD>(z, 0) = ptrv(z + 0x10);
    fl<DWORD>(z, 0x10) = 2;
    fl<DWORD>(z, 0x14) = 0x20;
    const char *const names[] = {"Shoot", "Idle", "ShootUp", ""};
    lstrcpyA(reinterpret_cast<char *>(z + 0x18), names[g.next() % 4]);
}

void fill_shoot(Gen &g, BYTE *s, bool real)
{
    if (!real)
        fl<DWORD>(s, 0x10) = angle_bits(g);
    if (!real || fl<DWORD>(s, 0xc))
        fl<DWORD>(s, 0xc) = ptrv(behavior_or_null(g, real ? 1000000 : 2));
    make_zstring(g);
}
void fill_none(Gen &g, BYTE *, bool) { make_zstring(g); }
void fill_closest(Gen &g, BYTE *s, bool real)
{
    if (real)
        return;
    fl<DWORD>(s, 0xc) = g.next();
    fl<DWORD>(s, 0x10) = g.next() % 3 ? g.next() % 2000 : g.next();
    fl<DWORD>(s, 0x14) = g.fbits_(0.0f, 400.0f);
    fl<DWORD>(s, 0x18) = g.next();
    fl<DWORD>(s, 0x1c) = g.next();
}
void fill_queue(Gen &g, BYTE *s, bool real)
{
    DWORD n = real ? fl<DWORD>(s, 0x14) : g.next() % 5;
    if (n > 16)
        n = 16;
    BYTE *d = extra(0);
    fl<DWORD>(s, 0xc) = 0x5f3228;   // the array's vtable (make_self clears +0xc, an AI's broadcaster)
    for (DWORD i = 0; i < 64; i++)
        fl<DWORD>(d, i * 4) = i < n ? ptrv(behavior_or_null(g, 5)) : 0;
    fl<DWORD>(s, 0x10) = ptrv(d);
    fl<DWORD>(s, 0x14) = n;
    fl<DWORD>(s, 0x18) = 64;   // room to append in place (no pool block)
    if (!real) {
        fl<DWORD>(s, 0x1c) = g.next() % (n + 2);
        fl<BYTE>(s, 0x20) = (BYTE)(g.next() % 2);
    } else if (fl<DWORD>(s, 0x1c) > n + 1) {
        fl<DWORD>(s, 0x1c) = n + 1;
    }
}
void fill_broadcast(Gen &g, BYTE *s, bool real)
{
    if (!real)
        fl<DWORD>(s, 0x10) = g.fbits_(0.0f, 1.0f);
}
void fill_stand_still(Gen &, BYTE *s, bool)
{
    fl<DWORD>(s, 0x10) = fake_vt(T_BEHAVIOR);
    fl<DWORD>(s, 0x24) = fake_vt(T_BEHAVIOR);
}
void fill_guard(Gen &g, BYTE *s, bool real)
{
    fl<DWORD>(s, 0x14) = fake_vt(T_BEHAVIOR);
    fl<DWORD>(s, 0xd0) = fake_vt(T_BEHAVIOR);
    if (real)
        return;
    fl<BYTE>(s, 0x10) = (BYTE)(g.next() % 2);
    fl<BYTE>(s, 0x11) = (BYTE)(g.next() % 2);
    fl<DWORD>(s, 0xb8) = g.fbits_(0.0f, 300.0f);
    fl<DWORD>(s, 0xbc) = g.next();
    fl<DWORD>(s, 0x100) = g.next() % 3 ? 0 : g.fbits_(-1.0f, 3.0f);
    fl<DWORD>(s, 0x104) = g.fbits_(0.0f, 10.0f);
}
void fill_follow(Gen &g, BYTE *s, bool real)
{
    fl<DWORD>(s, 0x14) = fake_vt(T_BEHAVIOR);
    fl<DWORD>(s, 0xd0) = fake_vt(T_BEHAVIOR);
    if (!real || fl<DWORD>(s, 0xec))
        fl<DWORD>(s, 0xec) = ptrv(real ? obj(O_ENTITY + 5) : any_entity(g, 4));
    if (real)
        return;
    fl<DWORD>(s, 0xb8) = g.fbits_(0.0f, 300.0f);
    fl<DWORD>(s, 0xf0) = g.next() % 3 ? 0 : g.fbits_(-200.0f, 200.0f);
    fl<DWORD>(s, 0xf4) = g.next() % 3 ? 0 : g.fbits_(-200.0f, 200.0f);
}

enum { C_SHOOT, C_POINT, C_CLOSEST, C_QUEUE, C_BROADCAST, C_STAND_STILL, C_GUARD, C_FOLLOW, C_FIDGET_SETTINGS, C_FIDGET_DATA, NCLASSES };
const SlotClass CLASSES[NCLASSES] = {
    {{"CShootEntityBehavior", 0x14, 0x713668, 0x577ea0, 0x577ee0, 0x578020, {0x577e50, 0x577fc0}, true}, 0x60098c, fill_shoot},
    {{"CPointWeaponAtTargetEntityBehavior", 0xc, 0x71366c, 0x5785a0, 0x5785d0, 0x578710, {0x578550, 0x5786b0}, true, true}, 0x600a08, fill_none},
    {{"CEntityBehaviorStateSetClosestTarget", 0x20, 0x713850, 0x586310, 0x586350, 0x5864a0, {0x5862c0, 0x586440}, true}, 0x601654, fill_closest},
    {{"CEntityBehaviorQueue", 0x24, 0x713858, 0x586980, 0x5869d0, 0x586c40, {0x586930, 0x586be0}, true}, 0x6016d0, fill_queue},
    {{"CBroadcastEvilAI", 0x14, 0x713820, 0x584ea0, 0x584ef0, 0x585080, {0x584e50, 0x585020}, true}, 0x601458, fill_broadcast},
    {{"CStandStillAttackAI", 0x30, 0x713828, 0x5856f0, 0x585750, 0x5858f0, {0x5856a0, 0x585890}, true}, 0x601500, fill_stand_still},
    {{"CGuardPosAI", 0x108, 0x713830, 0x585af0, 0x585b50, 0x585d20, {0x585a90, 0x585cb0}, true, false, nullptr, 0xc8}, 0x6015a8, fill_guard},
    {{"FollowAI", 0xf8, 0x6be1b0, 0x4a4fa0, 0x4a5010, 0x4a5230, {0x4a4f40, 0x4a51c0}, true, false, nullptr, 0xc8}, 0x5f422c, fill_follow},
    {{"CFidgetSettings", 0x14, 0x6bdf3c, 0x49ffa0, 0x49ffd0, 0x4a0110, {0x49ff50, 0x4a00b0}, true}, 0x5f3f94, fill_none},
    {{"CFidgetData", 0x8c, 0x6bdf40, 0x4a0380, 0x4a03a0, 0, {0x4a0320, 0x4a0470}, true}, 0x5f4000, fill_none},
};

DWORD seconds(Gen &g) { return g.fbits_(0.0f, 0.2f); }
void a_none(Gen &, DWORD *) {}
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_seconds_entity(Gen &g, DWORD *a)
{
    a[0] = seconds(g);
    a[1] = ptrv(any_entity(g));
}
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
void a_entity_name(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(extra(7));
}
void a_behavior(Gen &g, DWORD *a) { a[0] = ptrv(behavior_or_null(g, 4)); }
const char *const MESSAGES[] = {"Died", "died", "Delete", "DELETE", "Deleted", "Hit", ""};
void a_message(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g, 3));
    a[1] = ptrv(MESSAGES[g.next() % 7]);
}

const ClassSlot SLOTS[] = {
    {C_SHOOT, {"CShootEntityBehavior::Begin", 0x5780f0, 1, 0xff, 200, a_entity}},
    {C_SHOOT, {"CShootEntityBehavior::PlayOrFire", 0x578120, 2, 0, 400, a_entity_name}},
    {C_SHOOT, {"CShootEntityBehavior::Update", 0x578240, 2, 0xff, 1200, a_seconds_entity}},
    {C_SHOOT, {"CShootEntityBehavior::Draw", 0x578460, 2, 0, 300, a_entity_view}},
    {C_POINT, {"CPointWeaponAtTargetEntityBehavior::Update", 0x5787c0, 2, 0xff, 800, a_seconds_entity}},
    {C_CLOSEST, {"CEntityBehaviorStateSetClosestTarget::ConeRadians", 0x586760, SLOT_FLOAT, 0xffffffff, 200, a_none}},
    {C_CLOSEST, {"CEntityBehaviorStateSetClosestTarget::Begin", 0x5867c0, 1, 0xffffffff, 400, a_entity}},
    {C_CLOSEST, {"CEntityBehaviorStateSetClosestTarget::Draw", 0x586820, 2, 0, 400, a_entity_view}},
    {C_QUEUE, {"CEntityBehaviorQueue::Begin", 0x586ef0, 1, 0xffffffff, 200, a_entity}},
    {C_QUEUE, {"CEntityBehaviorQueue::Update", 0x586f10, 2, 0xff, 600, a_seconds_entity}},
    {C_QUEUE, {"CEntityBehaviorQueue::Append", 0x586f80, 1, 0, 300, a_behavior}},
    {C_QUEUE, {"CEntityBehaviorQueue::Done", 0x586fb0, 0, 0xffffffff, 200, a_none}},
    {C_BROADCAST, {"CBroadcastEvilAI::Update", 0x585140, 2, 0, 400, a_entity_seconds}},
    {C_STAND_STILL, {"CStandStillAttackAI::Begin", 0x5859f0, 1, 0, 200, a_entity}},
    {C_STAND_STILL, {"CStandStillAttackAI::Update", 0x585a30, 2, 0, 200, a_entity_seconds}},
    {C_STAND_STILL, {"CStandStillAttackAI::Draw", 0x585a60, 2, 0, 100, a_entity_view}},
    {C_GUARD, {"CGuardPosAI::Begin", 0x586050, 1, 0, 600, a_entity}},
    {C_GUARD, {"CGuardPosAI::Update", 0x586170, 2, 0, 800, a_entity_seconds}},
    {C_FOLLOW, {"FollowAI::Begin", 0x4a55a0, 1, 0, 600, a_entity}},
    {C_FOLLOW, {"FollowAI::Update", 0x4a5790, 2, 0, 600, a_entity_seconds}},
    {C_FOLLOW, {"FollowAI::Draw", 0x4a5580, 2, 0, 100, a_entity_view}},
    {C_FOLLOW, {"FollowAI::OnMessage", 0x4a5890, 2, 0, 300, a_message}},
};

Module g_mod{CLASSES, NCLASSES, SLOTS, (int)(sizeof SLOTS / sizeof SLOTS[0]), {}, 0, nullptr, nullptr};

Policy policy()
{
    Policy p{};
    p.find_minus_one = 4;
    return p;
}

// The function-local statics of the slots made as the exe makes them on their first call
void make_statics()
{
    BYTE &idle = *at<BYTE *>(0x713666);
    if (!(idle & 1)) {
        idle |= 1;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(0x713660), "Idle");
        crt_atexit(at<void(__cdecl *)()>(0x578230));
    }
    const DWORD colors[2][7] = {{0x713664, 0x713650, 0xff, 0x28, 0x28, 0xff, 0x578540}, {0x713838, 0x713840, 0xff, 0x64, 0x19, 0xff, 0x586920}};
    for (const DWORD *c : colors) {
        BYTE &guard = *at<BYTE *>(c[0]);
        if (guard & 1)
            continue;
        guard |= 1;
        at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(at<void *>(c[1]), c[2], c[3], c[4], c[5]);   // FieldColor_CtorRGBA
        crt_atexit(at<void(__cdecl *)()>(c[6]));
    }
}

// The out-of-line destructors on objects made by Create (each side its own object, compared after)
void test_out_of_line(Tally &t, const char *what, DWORD create, DWORD dtor, DWORD size, unsigned seed)
{
    using namespace invcheck;
    BYTE o[2][0x40];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *p = static_cast<BYTE *>(at<void *(__cdecl *)()>(create)());
        if (p) {
            at<void(THISCALL *)(void *)>(dtor)(p);
            memcpy(o[side], p, size);
            pool_free(p, size);
        }
        arena_off();
        side_end(side);
    }
    t.check(!memcmp(o[0], o[1], size) && same_region(g_arena[0], g_arena[1], g_used[0]), what, "[out-of-line destructor]");
}

// ---- CFidgetData and CFidgetSettings ------------------------------------------------------------------------------

namespace fidget {

using namespace invcheck;

BYTE g_model[0x100];          // the fake model (its sequences +0xb4 only pointed at)
BYTE g_entity[0x20];
DWORD g_reporter_vt[32];
BYTE g_reporter[8];
DWORD g_model_present, g_model_index, g_collect_n, g_collect_values[8];
const char *const SEQ_NAMES[] = {"Fidget1", "FidgetCrouch", "fidgetcrouch2", "FIDGETCROUCHX", "Fidge", "FidgetCrouc", "Idle", "fidget"};

DWORD THISCALL f_report(void *, const char *text) { ev(0x8017, text ? fnv_text(2166136261u, text) : 0); return result(0x17); }
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
BYTE *THISCALL s_model_object(void *list, DWORD m)
{
    ev(0x901, list == at<void *>(0x6cfdd8), m);
    return g_model_present & (1u << (m & 7)) ? g_model : nullptr;
}
DWORD THISCALL s_collect(void *model, BYTE *array, const char *prefix, DWORD flag)
{
    ev(0x902, model == g_model, fnv_text(2166136261u, prefix), flag);
    for (DWORD i = 0; i < g_collect_n; i++) {
        DWORD v = g_collect_values[i];
        vcall<DWORD>(array, 2, &v);
    }
    return result(0x02);
}
const char *THISCALL s_sequence_name(void *model, DWORD i)
{
    ev(0x903, model == g_model, i);
    return SEQ_NAMES[i & 7];
}
DWORD THISCALL s_model_index(void *e) { ev(0x904, e == g_entity); return g_model_index; }
BYTE *THISCALL s_entity_model(void *e) { ev(0x905, e == g_entity); return g_model; }
DWORD THISCALL s_between(void *gen, DWORD lo, DWORD hi)
{
    ev(0x906, gen == at<void *>(0x7124c0), lo, hi);
    return hi >= lo && hi - lo < 0x1000 ? lo + random() % (hi - lo + 1) : lo;
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x48d8f0, (void *)&s_model_object}, {0x5172b0, (void *)&s_collect},     {0x517200, (void *)&s_sequence_name},
    {0x48b560, (void *)&s_model_index},  {0x48b290, (void *)&s_entity_model}, {0x55c4e0, (void *)&s_between},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_patches[NSTANDS];

// The state of a fidget data object: the fields, the lists by their contents
unsigned state(BYTE *o)
{
    unsigned h = fnv(2166136261u, o, 0xc);
    static const DWORD FIELDS[] = {0x20, 0x3c, 0x60, 0x64, 0x84, 0x88, 0x58, 0x5c, 0x7c, 0x80};
    for (DWORD off : FIELDS)
        h = fnv(h, o + off, 4);
    static const DWORD ARRAYS[] = {0xc, 0x28, 0x44, 0x68};
    for (DWORD a : ARRAYS) {
        const DWORD n = *reinterpret_cast<DWORD *>(o + a + 8);
        h = fnv(h, &n, 4);
        h = fnv(h, o + a, 4);   // the vtable
        const DWORD *d = *reinterpret_cast<DWORD **>(o + a + 4);
        if (d && n < 64)
            h = fnv(h, d, n * 4);
    }
    return h;
}

// One case: Create, the lists filled, the function, the state, slot 1
void test_case(Tally &t, int what, unsigned seed)
{
    unsigned st[2];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        g_model_present = random() | 1;
        g_model_index = random() % 4;
        g_collect_n = random() % 7;
        for (DWORD &v : g_collect_values)
            v = random() % 9;
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
        void *const reporter = *at<void **>(0x6c02cc);
        *at<void **>(0x6c02cc) = g_reporter;
        BYTE *o = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x4a0380)());
        const DWORD n_a = random() % 5, n_b = random() % 3;
        for (DWORD i = 0; i < n_a + n_b; i++) {
            DWORD v = random() % 9;
            vcall<DWORD>(o + (i < n_a ? 0xc : 0x28), 2, &v);
        }
        *reinterpret_cast<DWORD *>(o + 8) = random() % 4;
        static const DWORD INDICES[] = {0x60, 0x64, 0x84, 0x88};
        for (DWORD off : INDICES)
            *reinterpret_cast<DWORD *>(o + off) = random() % 6;
        BYTE settings[0x14];
        *reinterpret_cast<DWORD *>(settings + 0x10) = random() % 7;
        const char *const currents[] = {"Idle", "IdleCrouch", "idle", "Walk"};
        const char *current = currents[random() % 4];
        switch (what) {
        case 0: at<void(THISCALL *)(void *, DWORD)>(0x4a0860)(o, random() % 4), r[side] = 0; break;
        case 1: at<DWORD(THISCALL *)(void *)>(0x4a0640)(o), r[side] = 0; break;
        case 2: at<DWORD(THISCALL *)(void *, DWORD)>(0x4a0660)(o, random()), r[side] = 0; break;
        default: r[side] = at<DWORD(THISCALL *)(void *, void *, void *, const char *)>(0x4a0680)(o, settings, g_entity, current); break;
        }
        st[side] = state(o);
        vcall<void *>(o, 1, 1);
        *at<void **>(0x6c02cc) = reporter;
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_patches[i]);
        side_end(side);
    }
    const char *const names[] = {"CFidgetData::SetModel", "CFidgetData::Reset", "CFidgetData::Reload", "CFidgetData::PickFidget"};
    char d[160];
    wsprintfA(d, "[seed %u: eax %x / %x, state %x / %x, log %d / %d]", seed, r[0], r[1], st[0], st[1], g_nlog[0], g_nlog[1]);
    if (!(r[0] == r[1] && st[0] == st[1] && same_log()) && t.mismatches < 4)
        log_first_difference();
    t.check(r[0] == r[1] && st[0] == st[1] && same_log(), names[what], d);
}

// The next fidget time (st0) on settings with random average and variance, the game's random generator the
// same before each side
void test_next_time(Tally &t, unsigned seed)
{
    SavedBytes saved;
    save_bytes(saved, 0x7124c0, GAME_RANDOM_BYTES);
    g_seed = seed;
    BYTE s[0x14] = {};
    const float avg = (float)(int)(random() % 4000) / 100.0f;
    const DWORD k = random() % 5;
    const float var = k == 0 ? 0.0f : k == 1 ? -0.0f : (float)(int)(random() % 2000) / 100.0f;
    memcpy(s + 8, &avg, 4);
    memcpy(s + 0xc, &var, 4);
    long double v[2];
    for (int side = 0; side < 2; side++) {
        restore_bytes(saved);
        side_begin(side);
        v[side] = at<long double(THISCALL *)(void *)>(0x4a02c0)(s);
        side_end(side);
    }
    restore_bytes(saved);
    char d[80];
    wsprintfA(d, "[seed %u]", seed);
    t.check(!memcmp(&v[0], &v[1], 10), "CFidgetSettings::NextTime", d);
}

void run(Tally &t)
{
    for (DWORD &v : g_reporter_vt)
        v = invcheck::ptrv((void *)&f_bad);
    g_reporter_vt[17] = invcheck::ptrv((void *)&f_report);
    *reinterpret_cast<DWORD *>(g_reporter) = invcheck::ptrv(g_reporter_vt);
    for (int what = 0; what < 4; what++)
        for (unsigned k = 0; k < (unsigned)scaled(what == 3 ? 800 : 300); k++)
            test_case(t, what, 9000u * (unsigned)what + k);
    for (unsigned k = 0; k < (unsigned)scaled(400); k++)
        test_next_time(t, k);
}

}  // namespace fidget

}  // namespace

ZAX_CHECKS(behavior_rest)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"behavior_rest: class getters"}, objects{"behavior_rest: Create, CreateArray, destructors"},
        fields{"behavior_rest: RegisterFields"}, real_fields{"behavior_rest: registered fields and classes (real data)"},
        slots{"behavior_rest: slots in the fake world"}, registered{"behavior_rest: registered class infos (real data)"},
        real_slots{"behavior_rest: the level's AIs through their slots (real data)"}, fidgets{"behavior_rest: fidget data and settings"};
    SavedBytes saved_random, saved_random2, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random2, 0x6d06c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_behavior_rest;
    if (!invcheck::arena_setup()) {
        objects.check(false, "behavior_rest", "[no arena]");
    } else {
        for (int c = 0; c < NCLASSES; c++) {
            log("    > %s", CLASSES[c].def.name);   // one line per class (to find a crash)
            invcheck::test_classes(objects, getters, fields, real_fields, &CLASSES[c].def, 1);
        }
    }
    log("    > out-of-line destructors");
    for (unsigned k = 0; k < (unsigned)scaled(20); k++) {
        test_out_of_line(objects, "CShootEntityBehavior", 0x577ea0, 0x5780d0, 0x14, k);
        test_out_of_line(objects, "CPointWeaponAtTargetEntityBehavior", 0x5785a0, 0x5787a0, 0xc, k);
        test_out_of_line(objects, "CEntityBehaviorStateSetClosestTarget", 0x586310, 0x5867a0, 0x20, k);
        test_out_of_line(objects, "CEntityBehaviorQueue", 0x586980, 0x586e40, 0x24, k);
    }
    log("    > out-of-line destructors done");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_statics();
    make_templates(g_mod);
    if (world_on()) {
        run_slots(g_mod, slots, policy());
        run_real(g_mod, "behavior_rest", registered, real_slots, policy(), 30);
        world_off();
    } else {
        slots.check(false, "behavior_rest", "[the fake world could not be set up]");
    }
    free_templates(g_mod);
    slots.report();
    registered.report();
    real_slots.report();
    invcheck::g_hook_table = hooks_behavior_rest;
    log("    > fidgets");
    fidget::run(fidgets);
    fidgets.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random2);
    restore_bytes(saved_random);
    log("  behavior_rest: %u ms", GetTickCount() - start);
}

}  // namespace zax
