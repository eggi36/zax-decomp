// Checks of the patrol AIs (ai_patrol.cpp) against the original, in the game thread.
//
//  - The classes: the class getters (both copies; the class variable set and 0 with ClassInfo_Register
//    replaced by a stand-in), Create on a pool block filled with a pattern and the deleting destructor
//    with flag 1, CreateArray with 0-3 elements and the destructor with flags 2/3, RegisterFields into a
//    fake class info and the registered descriptors against the reimplementation's (the C heap from an
//    arena of each side; the machinery of checks_inventory_common.h); CPatrolBaseAI's level destructor on
//    an object built up to that level.
//  - The slots in the fake world (checks_ai_world.h): the object under test is a copy of an object made
//    by Create with its embedded move behaviour's vtable a recorder table and its fields random; the
//    entities, the path part and its polygon, the behaviours, inventories and items are fakes.
//  - Real data: the registered class infos; the AIs of these classes on the running level's parts (their
//    copies) through every slot of their class in the fake world, the entity a copy of their part.
//  - The timers: QueryPerformanceFrequency equal, QueryPerformanceCounter increasing.
#include "checks_ai_world.h"
#include "checks_inventory_common.h"
#include "checks_ai_patrol.h"

namespace zax {

int hooks_ai_patrol(Hook *out);

namespace {

using namespace aiw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"COldBadPatrolAI", 0x34, 0x6d0b10, 0x53a990, 0x53aa00, 0x53aba0, {0x53a940, 0x53ab40}, true},
    {"CPatrolAreaAI", 0xd8, 0x6d0b34, 0x53b400, 0x53b450, 0x53b5f0, {0x53b3a0, 0x53b580}, true},
    {"CPatrolBaseAI", 0x10, 0x6d0b3c, 0, 0, 0, {0x53bcc0, 0x53bd10}, false},
    {"CPatrolPathAI", 0xf4, 0x6d0b70, 0x53be30, 0x53be90, 0x53c060, {0x53bdd0, 0x53bff0}, true},
    {"CFollowPathAI", 0xf4, 0x6d0b74, 0x53cd00, 0x53cd70, 0x53cfc0, {0x53cc50, 0x53cf00}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
constexpr int C_OLD = 0, C_AREA = 1, C_PATH = 3, C_FOLLOW = 4;

// ---- The classes ----

// CreateArray with count elements, each side destroyed (slot 1 with 2, then the block freed) before the
// other side runs: the embedded move behaviours take a path checker from the pool, which then gives
// the other side the same blocks (invcheck::test_create_array keeps side 0's array alive meanwhile)
void test_create_array_in_turn(Tally &t, const invcheck::ClassDef &c, int count, BYTE pattern)
{
    using namespace invcheck;
    DWORD e[2], destroyed[2], used[2];
    bool arenas_same = false;
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        arena_on(side, pattern);
        void *a = at<CreateN>(c.create_array)(count, 0, 0);
        e[side] = rel(invcheck::ptrv(a), side);
        used[side] = g_used[side];
        if (side == 0)
            memcpy(g_copy, g_arena[0], used[0]);
        else
            arenas_same = used[0] == used[1] && same_region(g_copy, g_arena[1], used[1]);
        destroyed[side] = count > 0 && a ? rel(invcheck::ptrv(vcall<void *>(a, 1, 2)), side) : 0;
        arena_off();
        side_end(side);
    }
    char d[200];
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, destroyed %x / %x, first difference +%x: %x / %x]", count, e[0], e[1], used[0],
              used[1], destroyed[0], destroyed[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(arenas_same && e[0] == e[1] && destroyed[0] == destroyed[1], c.name, d);
}

// CFollowPathAI's RegisterFields makes no descriptor of its own (invcheck::test_register_fields expects
// one): it changes two inherited ones. Both sides into a fake class info (inherited fields, and with the
// class variable 0), the arenas and class states compared; then the registered descriptors against
// side 1's: "Order" (+0x28 the default, +0x18) and "End of path action" (+0x34 the class).
void test_follow_register_fields(Tally &t, Tally &real)
{
    using namespace invcheck;
    const ClassDef &d = CLASSES[C_FOLLOW];
    for (int var_zero = 0; var_zero < 2; var_zero++) {
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            run_register_fields(d, var_zero != 0, side, (BYTE)(0x5a + var_zero));
            side_end(side);
        }
        DWORD s0[9], s1[9];
        class_state(0, s0);
        class_state(1, s1);
        const bool same = g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]) && !memcmp(s0, s1, sizeof s0) &&
                          !memcmp(&g_made[0].call, &g_made[1].call, sizeof g_made[0].call);
        char text[200];
        wsprintfA(text, "[RegisterFields var %d: %u / %u fields, eax %x / %x, first difference +%x: %x / %x]", var_zero, s0[0], s1[0], s0[6],
                  s1[6], g_diff_at, g_diff_a, g_diff_b);
        t.check(same, d.name, text);
        if (!var_zero) {
            const CClassInfo *k = *at<CClassInfo **>(d.var), *m = g_made[1].cls;
            bool same_real = k && k->num_fields == m->num_fields;
            for (DWORD i = 0; same_real && i < m->num_fields; i++) {
                const BYTE *x = reinterpret_cast<const BYTE *>(k->fields[i]), *y = reinterpret_cast<const BYTE *>(m->fields[i]);
                same_real = fl<DWORD>(const_cast<BYTE *>(x), 0) == fl<DWORD>(const_cast<BYTE *>(y), 0) &&
                            fl<DWORD>(const_cast<BYTE *>(x), 0x10) == fl<DWORD>(const_cast<BYTE *>(y), 0x10) &&
                            fl<DWORD>(const_cast<BYTE *>(x), 0x18) == fl<DWORD>(const_cast<BYTE *>(y), 0x18) &&
                            (fl<DWORD>(const_cast<BYTE *>(x), 0) != 0x5fd970 || fl<DWORD>(const_cast<BYTE *>(x), 0x28) == fl<DWORD>(const_cast<BYTE *>(y), 0x28)) &&
                            (fl<DWORD>(const_cast<BYTE *>(x), 0x10) != 0x2c || fl<DWORD>(const_cast<BYTE *>(x), 0x34) == fl<DWORD>(const_cast<BYTE *>(y), 0x34));
            }
            real.check(same_real, d.name, "[registered: Order and End of path action]");
        }
        free_made(0);
        free_made(1);
    }
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_ai_patrol;
    if (!invcheck::arena_setup()) {
        objects.check(false, "ai_patrol", "[no arena]");
        return;
    }
    SavedBytes random2;
    for (int c = 0; c < NCLASSES; c++) {
        const invcheck::ClassDef &d = CLASSES[c];
        for (DWORD gtr : d.getters)
            invcheck::test_getter(getters, d, gtr);
        if (d.create) {
            for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++) {
                save_bytes(random2, 0x6d06c0, 0x400);   // COldBadPatrolAI's constructor reseeds it
                invcheck::test_create(objects, d, seed * 7 + (unsigned)c);
                restore_bytes(random2);
            }
            for (int n = 0; n < 4; n++) {
                save_bytes(random2, 0x6d06c0, 0x400);
                test_create_array_in_turn(objects, d, n, (BYTE)(0x31 * n + c));
                restore_bytes(random2);
            }
        }
        if (c == C_FOLLOW)
            test_follow_register_fields(fields, real_fields);
        else if (d.register_fields)
            for (unsigned seed = 1; seed <= 2; seed++)
                invcheck::test_register_fields(fields, real_fields, d, seed * 13 + (unsigned)c);
    }
    // CPatrolBaseAI's level destructor 0x53bd70 on an object made up to that level
    for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++) {
        alignas(16) BYTE o[2][0x20];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (DWORD i = 0; i < 0x20; i += 4)
                fl<DWORD>(o[side], i) = random();
            at<void *(THISCALL *)(void *)>(0x4fd060)(o[side]);   // CAIBase
            fl<DWORD>(o[side], 0) = 0x5ed1d0;
            at<void(THISCALL *)(void *)>(0x4173c0)(o[side]);
            fl<DWORD>(o[side], 0) = 0x5f42d4;
            at<void(THISCALL *)(void *)>(0x4173c0)(o[side]);
            invcheck::side_begin(side);
            at<void(THISCALL *)(void *)>(0x53bd70)(o[side]);
            invcheck::side_end(side);
        }
        objects.check(!memcmp(o[0], o[1], sizeof o[0]), "CPatrolBaseAI", "[level destructor 0x53bd70]");
    }
}

// ---- The slots ---------------------------------------------------------------------------------------------

BYTE *g_template[NCLASSES];
int g_self_class;
const BYTE *g_real_self;   // a copy of a real AI instead of a random one
const BYTE *g_real_part;   // its part: the fake entities become copies of it
BYTE *g_parts[8192];

void fake_behavior_vtable(BYTE *s, DWORD off) { fl<DWORD>(s, off) = ptrv(g_fvt[T_BEHAVIOR]); }

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    memcpy(s, g_real_self ? g_real_self : g_template[c], CLASSES[c].size);
    fl<DWORD>(s, 4) = 0;     // no dynamic properties, no broadcaster (a copy must not reach the real ones)
    fl<DWORD>(s, 0xc) = 0;
    if (g_real_part)
        for (int i = 0; i < N_ENTITY; i++) {
            BYTE *e = obj(O_ENTITY + i);
            memcpy(e, g_real_part, OBJ);
            fl<DWORD>(e, 0) = ptrv(g_fvt[T_ENTITY]);
            fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        }
    if (c == C_OLD) {
        if (!g_real_self) {
            fl<BYTE>(s, 0x10) = (BYTE)(g.next() % 2);
            const DWORD min = g.next() % 150, max = min + g.next() % 120;
            fl<DWORD>(s, 0x14) = g.next() % 8 ? min : max;
            fl<DWORD>(s, 0x18) = g.next() % 8 ? max : min;
            for (DWORD off = 0x1c; off < 0x2c; off += 4)
                fl<DWORD>(s, off) = g.next();
            fl<BYTE>(s, 0x2c) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x30) = g.next() % 3 ? g.fbits_(-1.0f, 400.0f) : 0xbf800000u;
        }
    } else if (c == C_AREA) {
        fake_behavior_vtable(s, 0x14);
        if (!g_real_self) {
            fl<DWORD>(s, 0x10) = g.fbits_(0.0f, 500.0f);
            fl<DWORD>(s, 0xb8) = g.fbits_(0.0f, 3.0f);
            fl<DWORD>(s, 0xd0) = g.fbits_(0.0f, 100.0f);
            fl<DWORD>(s, 0xd4) = g.next() % 2 ? 0 : g.fbits_(-1.0f, 2.0f);
        }
    } else {
        fake_behavior_vtable(s, 0x34);
        if (!g_real_self) {
            fl<DWORD>(s, 0x10) = g.fbits_(-500.0f, 500.0f);
            fl<DWORD>(s, 0x14) = g.fbits_(-500.0f, 500.0f);
            fl<BYTE>(s, 0x18) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x1c) = g.next() % 6;
            fl<DWORD>(s, 0x24) = g.next() % 3 ? ptrv(any_entity(g)) : 0;
            fl<BYTE>(s, 0x28) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x2c) = g.next() % 2 ? ptrv(obj(O_OTHER)) : 0;
            fl<DWORD>(s, 0x30) = g.next() % 9 - 1;
            fl<DWORD>(s, 0xd8) = g.fbits_(0.0f, 3.0f);
            fl<DWORD>(s, 0xf0) = g.fbits_(-1.0f, 1.0f);
        } else if (fl<DWORD>(s, 0x24)) {
            fl<DWORD>(s, 0x24) = ptrv(any_entity(g));   // the path part: a fake
        }
    }
}

DWORD seconds(Gen &g) { return g.fbits_(0.0f, 0.2f); }
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
void a_view_entity(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_VIEW));
    a[1] = ptrv(any_entity(g));
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next() % 4 ? g.next() & 0xff : 0;
}
void a_center(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    fl<DWORD>(obj(O_OUT), 0) = g.fbits_(-1000.0f, 5000.0f);
    fl<DWORD>(obj(O_OUT), 4) = g.fbits_(-1000.0f, 5000.0f);
    a[1] = ptrv(obj(O_OUT));
}
void a_entity_flag(Gen &g, DWORD *a)
{
    fl<DWORD>(obj(O_SELF), 0x24) = ptrv(any_entity(g));   // the path part (the caller has one)
    a[0] = ptrv(any_entity(g));
    a[1] = g.next() % 2 ? g.next() : 0;
}

struct ClassSlot {
    int cls;
    SlotTest t;
};
const ClassSlot SLOTS[] = {
    {C_OLD, {"COldBadPatrolAI::Wander", 0x53acf0, 1, 0, 600, a_entity}},
    {C_OLD, {"COldBadPatrolAI::Collide", 0x53b1b0, 5, 0, 300, a_collide}},
    {C_OLD, {"COldBadPatrolAI::Update", 0x53b1f0, 2, 0, 600, a_entity_seconds}},
    {C_OLD, {"COldBadPatrolAI::Draw", 0x53b300, 2, 0, 200, a_entity_view}},
    {C_AREA, {"CPatrolAreaAI::Begin", 0x53b830, 1, 0xffffffff, 300, a_entity}},
    {C_AREA, {"CPatrolAreaAI::Update", 0x53b890, 2, 0, 600, a_entity_seconds}},
    {C_AREA, {"CPatrolAreaAI::NewPoint", 0x53b930, 1, 0xffffffff, 300, a_entity}},
    {C_AREA, {"CPatrolAreaAI::PickPoint", 0x53b960, 2, 0xffffffff, 600, a_center}},
    {C_AREA, {"CPatrolAreaAI::Draw", 0x53bbd0, 2, 0xffffffff, 200, a_entity_view}},
    {C_AREA, {"CPatrolAreaAI::DrawArea", 0x53bc10, 2, 0xffffffff, 200, a_view_entity}},
    {C_AREA, {"CPatrolAreaAI::Collide", 0x53bca0, 5, 0, 200, a_collide}},
    {C_PATH, {"CPatrolPathAI::Begin", 0x53c5c0, 1, 0, 400, a_entity}},
    {C_PATH, {"CPatrolPathAI::Update", 0x53c640, 2, 0, 800, a_entity_seconds}},
    {C_PATH, {"CPatrolPathAI::NearestPoint", 0x53c7d0, 1, 0, 400, a_entity}},
    {C_PATH, {"CPatrolPathAI::NextPoint", 0x53c8d0, 2, 0, 800, a_entity_flag}},
    {C_PATH, {"CPatrolPathAI::Draw", 0x53cae0, 2, 0, 400, a_entity_view}},
    {C_FOLLOW, {"CFollowPathAI::Update", 0x53c640, 2, 0, 200, a_entity_seconds}},
    {C_FOLLOW, {"CFollowPathAI::NextPoint", 0x53c8d0, 2, 0, 200, a_entity_flag}},
    {C_AREA, {"CLayerPartAI::Collide", 0x53d140, 5, 0, 50, a_collide}},
};

Policy patrol_policy()
{
    Policy p{};
    p.find_minus_one = 4;
    return p;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self_class = s.cls;
        g_real_self = nullptr;
        for (unsigned seed = 1; seed <= (unsigned)scaled(s.t.count); seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, patrol_policy());
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------

void test_real(Tally &registered, Tally &real_slots)
{
    for (const invcheck::ClassDef &c : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        registered.check(ci && ci->size == c.size && (!c.create || ptrv((void *)ci->create) == c.create) &&
                             (!c.create_array || ptrv((void *)ci->create_array) == c.create_array) && at<Getter>(c.getters[0])() == ci &&
                             at<Getter>(c.getters[1])() == ci,
                         c.name, d);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_patrol real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, ran = 0;
    for (int i = 0; i < n; i++) {
        const BYTE *part = parts[i];
        BYTE *ais[64];
        const int count = part_ais(part, parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const CClassInfo *ci = class_of(ais[k]);
            for (int c = 0; c < NCLASSES; c++)
                if (ci && ci == *at<CClassInfo **>(CLASSES[c].var) && CLASSES[c].create && readable(ais[k], CLASSES[c].size)) {
                    found[c]++;
                    const CClassInfo *by_vtable = class_of(ais[k]);
                    real_slots.check(by_vtable == ci && fl<DWORD>(ais[k], 0) == PATROL_CLASSES[c].vtable, CLASSES[c].name, "[vtable]");
                    if (found[c] > 40)
                        continue;   // enough of one class
                    g_self_class = c;
                    g_real_self = ais[k];
                    g_real_part = readable(part, OBJ) ? part : nullptr;
                    for (const ClassSlot &s : SLOTS)
                        if (s.cls == c)
                            for (unsigned seed = 1; seed <= 3; seed++) {
                                run_slot(real_slots, s.t, seed * 40503u + ptrv(ais[k]), make_self, patrol_policy());
                                ran++;
                            }
                    g_real_self = nullptr;
                    g_real_part = nullptr;
                }
        }
    }
    log("  ai_patrol real data: %d parts, AIs %d COldBadPatrolAI, %d CPatrolAreaAI, %d CPatrolPathAI, %d CFollowPathAI; %d cases", n,
        found[C_OLD], found[C_AREA], found[C_PATH], found[C_FOLLOW], ran);
}

// ---- The timers -------------------------------------------------------------------------------------------

void test_timers(Tally &t)
{
    for (int i = 0; i < 20; i++) {
        unsigned long long f[2], c[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            f[side] = at<unsigned long long(__cdecl *)()>(0x53d170)();
            c[side] = at<unsigned long long(__cdecl *)()>(0x53d150)();
            if (side == 0)
                hooks_in();
        }
        char d[120];
        wsprintfA(d, "[frequency %u / %u, counter %u / %u]", (DWORD)f[0], (DWORD)f[1], (DWORD)c[0], (DWORD)c[1]);
        t.check(f[0] == f[1] && c[0] <= c[1] && c[1] - c[0] < f[0], "timers", d);
    }
}

}  // namespace

ZAX_CHECKS(ai_patrol)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_patrol: class getters"}, objects{"ai_patrol: Create, CreateArray, destructors"},
        fields{"ai_patrol: RegisterFields"}, real_fields{"ai_patrol: RegisterFields against the registered fields (real data)"},
        slots{"ai_patrol: slots in the fake world"}, registered{"ai_patrol: registered class infos (real data)"},
        real_slots{"ai_patrol: the level's patrol AIs through their slots (real data)"}, timers{"ai_patrol: timers"};
    SavedBytes saved_random, saved_random2, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random2, 0x6d06c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    for (int c = 0; c < NCLASSES; c++)
        g_template[c] = CLASSES[c].create ? static_cast<BYTE *>(at<void *(__cdecl *)()>(CLASSES[c].create)()) : nullptr;
    restore_bytes(saved_random2);
    if (world_on()) {
        test_slots(slots);
        test_real(registered, real_slots);
        test_timers(timers);
        world_off();
    } else {
        slots.check(false, "ai_patrol", "[the fake world could not be set up]");
    }
    for (int c = 0; c < NCLASSES; c++)
        if (g_template[c])
            vcall<void *>(g_template[c], 1, 1);
    slots.report();
    registered.report();
    real_slots.report();
    timers.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random2);
    restore_bytes(saved_random);
    log("  ai_patrol: %u ms", GetTickCount() - start);
}

}  // namespace zax
