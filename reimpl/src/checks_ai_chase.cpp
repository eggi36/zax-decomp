// Checks of ai_chase.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): the getters of CAIBase, CLayerPartAI, CFlyweightLayerPartAI and
//    CLightBeamAI (both copies each), CAIBase's and CLightBeamAI's RegisterFields with the registered
//    descriptors, CLightBeamAI's Create, CreateArray and deleting destructor.
//  - Small functions directly (side 0 with the module's hooks out): CLayerPartAI's slots 30 (a float
//    through st0) and 33, the empty call on +0xc, the light beam's last-made global, the block reader's
//    constructor, the string kind's clone (on every descriptor of that kind the registered classes have,
//    the copies from the arena).
//  - The slots in the fake world (checks_rest_world.h): the chase investigate and chase pursue AIs' Begin,
//    move retry and Update (every state, blockers, failed moves), the companion AI's Begin, Update, target
//    test and spot, CLayerPartAI's message slot, the debug drawing through user defined class references,
//    the action run, CLightBeamAI's damage on collision, CAIBase's destructor on a fake object, the
//    difficulty list's deleting destructor on a fake list (its records on the heap, the real record list
//    and alias functions).
#include "checks_rest_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_ai_chase(Hook *out);

namespace {

using namespace rw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CAIBase", 0x10, 0x6c29f0, 0, 0, 0x4fcbb0, {0x4fcb00, 0x4fcb50}, false},
    {"CLayerPartAI", 0x10, 0x6c29f4, 0, 0, 0, {0x4fd220, 0x4fd2d0}, false},
    {"CFlyweightLayerPartAI", 0x10, 0x6c29f8, 0, 0, 0, {0x4fd390, 0x4fd490}, false},
    {"CLightBeamAI", 0x18, 0x6c2b50, 0x4fe2e0, 0x4fe330, 0x4fe4c0, {0x4fe290, 0x4fe460}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

// ---- Small functions directly ---------------------------------------------------------------------------

template <typename F> void two_sides(F f)
{
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        f(side);
        invcheck::side_end(side);
    }
}
void test_small(Tally &t)
{
    char d[160];
    for (unsigned k = 0; k < (unsigned)scaled(50); k++) {
        g_seed = 0x77000 + k;
        const DWORD a = random(), c = random();
        DWORD b = random();
        if (k % 8 == 0)
            b = 0x7fc00001u;   // a signalling NaN comes back quiet through fld
        DWORD r30[2], r33[2];
        two_sides([&](int side) {
            float f = at<float(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4fd140)(nullptr, a, b, c);
            memcpy(&r30[side], &f, 4);
            r33[side] = at<BYTE(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4fd150)(nullptr, a, b, c);
        });
        wsprintfA(d, "[slot 30 %x / %x, slot 33 %x / %x]", r30[0], r30[1], r33[0], r33[1]);
        t.check(r30[0] == r30[1] && r33[0] == r33[1], "CLayerPartAI", d);
    }
    // the last light beam (0x6c29fc) set and taken
    const DWORD saved = *at<DWORD *>(0x6c29fc);
    DWORD got[2], left[2];
    two_sides([&](int side) {
        at<void(__fastcall *)(DWORD)>(0x4fd5c0)(0x1234560u + (DWORD)side * 0);
        got[side] = at<DWORD(__cdecl *)()>(0x4fd5d0)();
        left[side] = *at<DWORD *>(0x6c29fc);
    });
    *at<DWORD *>(0x6c29fc) = saved;
    wsprintfA(d, "[taken %x / %x, left %x / %x]", got[0], got[1], left[0], left[1]);
    t.check(got[0] == got[1] && got[0] == 0x1234560u && left[0] == 0 && left[1] == 0, "LightBeam last", d);
    // the empty call on +0xc (with and without an object), the block reader's constructor
    DWORD obj_[4] = {0, 0, 0, 0x5ed1d0};
    DWORD reader[2][3];
    two_sides([&](int side) {
        at<void(THISCALL *)(void *)>(0x4fd180)(obj_);
        memset(reader[side], 0xcd, sizeof reader[side]);
        at<void *(THISCALL *)(void *)>(0x4fd5a0)(reader[side]);
    });
    wsprintfA(d, "[reader %x %x %x / %x %x %x]", reader[0][0], reader[0][1], reader[0][2], reader[1][0], reader[1][1], reader[1][2]);
    t.check(memcmp(reader[0], reader[1], sizeof reader[0]) == 0 && reader[0][0] == 0x5f95e8, "BlockReader 0x5f95e8", d);
}

// 0x4fa710: the clone of every descriptor of the string kind 0x5f9314 in the registered classes (the C heap
// from the arena of each side, compared, the copies' strings destroyed by their slot 0)
void test_clones(Tally &t, CClassInfo **cls, int n)
{
    using namespace invcheck;
    int found = 0;
    for (int i = 0; i < n && found < 40; i++) {
        CClassInfo *c = cls[i];
        if (!c)
            continue;
        for (DWORD k = 0; k < c->num_fields && found < 40; k++) {
            CFieldDesc *f = c->fields[k];
            if (!f || f->vftable != 0x5f9314)
                continue;
            found++;
            DWORD r[2];
            for (int side = 0; side < 2; side++) {
                invcheck::side_begin(side);
                arena_on(side, (BYTE)(0x40 + k));
                r[side] = rel(at<DWORD(THISCALL *)(void *)>(0x4fa710)(f), side);
                arena_off();
                invcheck::side_end(side);
            }
            char d[160];
            wsprintfA(d, "[%s field %u: %x / %x, %u / %u bytes, first difference +%x: %x / %x]", c->name, k, r[0], r[1], g_used[0], g_used[1],
                      g_diff_at, g_diff_a, g_diff_b);
            t.check(r[0] == r[1] && g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]), "StringKind 0x5f9314 clone", d);
            for (int side = 0; side < 2; side++) {
                arena_on(side, 0, false);
                if (r[side])
                    vcall<void *>(g_arena[side] + (r[side] - 0xa0000000u), 0, 0);
                arena_off();
            }
        }
    }
    log("  ai_chase: %d descriptors of the string kind 0x5f9314 cloned", found);
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_ai_chase;
    if (!invcheck::arena_setup()) {
        objects.check(false, "ai_chase", "[no arena]");
        return;
    }
    invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
}

// ---- The slots ---------------------------------------------------------------------------------------------

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
    return g.fbits_(0.0f, 2.0f);
}
DWORD entity(Gen &g) { return ptrv(any_entity(g)); }
DWORD fake(Gen &g, int first, int count, int null_one_in)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(first + (int)(g.next() % (DWORD)count)));
}
void fake_vtable(BYTE *s, DWORD off, int type) { fl<DWORD>(s, off) = ptrv(g_fvt[type]); }

enum Self { S_INVESTIGATE, S_PURSUE, S_COMPANION, S_LAYER_PART_AI, S_BEAM, S_AI_BASE, S_LIST, NSELF };
int g_self;
DWORD g_entries_block;   // the records a fake list got (freed after the side unless the slot did)
void make_list(Gen &g, BYTE *s)
{
    memset(s, 0, 0x140);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_CATALOGUE]);
    fl<DWORD>(s, 0x1c) = 0x5ed558;   // the aliases: an empty pointer array
    const DWORD n = g.next() % 4;
    g_entries_block = 0;
    if (n || g.next() % 2) {
        DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
        block[0] = n + 1;
        BYTE *data = reinterpret_cast<BYTE *>(block + 1);
        for (DWORD i = 0; i < (n + 1) * 0x80; i++)
            data[i] = (BYTE)g.next();
        for (DWORD i = 0; i <= n; i++)
            fl<DWORD>(data + i * 0x80, 0x78) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
        fl<DWORD>(s, 0x30) = ptrv(data);
        g_entries_block = ptrv(block);
    }
    fl<DWORD>(s, 0x34) = n;
    fl<DWORD>(s, 0x13c) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
}
const BYTE *g_real_self;   // a copy of a level's AI (real data) or null
DWORD g_real_size;
void make_real_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    memcpy(s, g_real_self, g_real_size);
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 0xc) = 0;
    fake_vtable(s, 0x10, T_BEHAVIOR);
    switch (g_self) {
    case S_INVESTIGATE:
        fake_vtable(s, 0xcc, T_BEHAVIOR);
        if (fl<DWORD>(s, 0x10c))
            fl<DWORD>(s, 0x10c) = fake(g, O_AI, N_AI, 0);
        if (g.next() % 3 == 0)
            fl<DWORD>(s, 0xf0) = g.next() % 7;   // every state now and then
        break;
    case S_COMPANION:
        fake_vtable(s, 0xec, T_BEHAVIOR);
        if (fl<DWORD>(s, 0xd0))
            fl<DWORD>(s, 0xd0) = fake(g, O_AI, N_AI, 0);
        fl<DWORD>(s, 0xcc) = g.next() % 4 ? entity(g) : 0;
        fl<DWORD>(s, 0xe4) = entity(g);
        break;
    default:
        break;
    }
}
void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_real_self) {
        make_real_self(g);
        return;
    }
    switch (g_self) {
    case S_INVESTIGATE:
        fl<DWORD>(s, 0) = 0x5ef95c;
        fake_vtable(s, 0x10, T_BEHAVIOR);
        fl<DWORD>(s, 0xb4) = time_value(g, -1.0f, 3.0f);
        fl<DWORD>(s, 0xb8) = g.next();
        fake_vtable(s, 0xcc, T_BEHAVIOR);
        fl<DWORD>(s, 0xe8) = g.fbits_(0.0f, 4000.0f);
        fl<DWORD>(s, 0xec) = g.fbits_(0.0f, 4000.0f);
        fl<DWORD>(s, 0xf0) = g.next() % 9 == 0 ? g.next() : g.next() % 7;
        fl<DWORD>(s, 0xf4) = time_value(g, -1.0f, 3.0f);
        fl<DWORD>(s, 0xf8) = g.fbits_(0.0f, 2.0f);
        fl<DWORD>(s, 0xfc) = time_value(g, -1.0f, 2.0f);
        fl<DWORD>(s, 0x100) = time_value(g, 0.0f, 6.0f);
        fl<DWORD>(s, 0x104) = g.next() % 4 == 0 ? 0 : time_value(g, 0.0f, 200.0f);
        fl<BYTE>(s, 0x108) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x10c) = fake(g, O_AI, N_AI, 4);
        break;
    case S_PURSUE:
        fl<DWORD>(s, 0) = 0x5efaac;
        fake_vtable(s, 0x10, T_BEHAVIOR);
        fl<DWORD>(s, 0xb4) = time_value(g, -1.0f, 3.0f);
        fl<DWORD>(s, 0xb8) = g.next();
        fl<DWORD>(s, 0xcc) = g.fbits_(0.0f, 2.0f);
        fl<DWORD>(s, 0xd0) = time_value(g, -1.0f, 2.0f);
        break;
    case S_COMPANION:
        fl<DWORD>(s, 0) = 0x5efef0;
        fl<BYTE>(s, 8) = 0;
        fake_vtable(s, 0x10, T_BEHAVIOR);
        fl<DWORD>(s, 0xcc) = g.next() % 4 ? entity(g) : 0;
        fl<DWORD>(s, 0xd0) = fake(g, O_AI, N_AI, 4);
        fl<DWORD>(s, 0xd4) = g.fbits_(0.0f, 2.0f);
        fl<DWORD>(s, 0xd8) = time_value(g, -1.0f, 2.0f);
        fl<DWORD>(s, 0xdc) = time_value(g, 0.0f, 50.0f);
        fl<DWORD>(s, 0xe0) = time_value(g, 0.0f, 100.0f);
        fl<DWORD>(s, 0xe4) = entity(g);
        fake_vtable(s, 0xec, T_BEHAVIOR);
        break;
    case S_LAYER_PART_AI:
        fake_vtable(s, 0, T_AI);
        break;
    case S_BEAM:
        fl<DWORD>(s, 0) = 0x5f9798;
        fl<DWORD>(s, 0x10) = time_value(g, 0.0f, 100.0f);
        fl<DWORD>(s, 0x14) = time_value(g, 0.0f, 100.0f);
        break;
    case S_AI_BASE:
        fl<DWORD>(s, 0) = 0x5ed1d0;
        fl<DWORD>(s, 4) = 0;   // no dynamic properties
        fl<DWORD>(s, 0xc) = fake(g, O_OTHER, N_OTHER, 3);
        break;
    case S_LIST:
        make_list(g, s);
        break;
    default:
        break;
    }
}

void a_entity(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = seconds(g);
}
void a_out(Gen &, DWORD *a) { a[0] = ptrv(obj(O_SPARE)); }
void a_target(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(any_entity(g, 5));
    a[2] = g.next();
}
void a_message(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    BYTE *m = obj(O_MSG);
    fl<DWORD>(m, 0) = 0x5eeb64;
    fl<DWORD>(m, 4) = g.next() % 3 ? *at<DWORD *>(0x71226c) : g.next();
    const DWORD classes[] = {0x6c29f4, 0x6c29f0, 0x6c2b50, 0x6bd2d0};
    fl<DWORD>(m, 0xc) = *at<DWORD *>(classes[g.next() % 4]);
    fl<BYTE>(m, 8) = (BYTE)g.next();
    fl<DWORD>(m, 0x10) = g.next();
    a[1] = ptrv(m);
}
void a_two(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next();
}
void a_run_action(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    BYTE *field = obj(O_SPARE);
    fl<DWORD>(field, 0) = fake(g, O_ACTION, N_ACTION, 0);
    a[1] = ptrv(field);
    a[2] = entity(g);
    a[3] = ptrv(any_entity(g, 3));
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(any_entity(g, 5));
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next();
}
void a_flags(Gen &g, DWORD *a) { a[0] = g.next() % 2 ? 0 : 2; }

// The fake list's records: by a tag in the list and their bytes in the log; freed after the side
void tag_records()
{
    BYTE *s = obj(O_SELF);
    if (g_entries_block && fl<DWORD>(s, 0x30) == g_entries_block + 4) {
        const DWORD n = *at<DWORD *>(g_entries_block);
        g_log[g_side].hash = fnv(g_log[g_side].hash, at<void *>(g_entries_block + 4), n * 0x80);
        fl<DWORD>(s, 0x30) = 0xe0000004u;
    }
}
void after_list()
{
    BYTE *s = obj(O_SELF);
    if (fl<DWORD>(s, 0x30) && g_entries_block)
        heap_free(at<void *>(g_entries_block));
    g_entries_block = 0;
}

struct ClassSlot {
    int self;
    SlotTest t;
    DWORD always = 0;      // IsKindOf affirms the class of this getter (the slot reads a part of it without a test)
    bool flying = false;   // IsKindOf affirms CEntityFlying (the companion reads the entity without a test)
};
const ClassSlot SLOTS[] = {
    {S_INVESTIGATE, {"CChaseInvestigateAI::Begin", 0x46bfe0, 1, 0, 150, a_entity}},
    {S_INVESTIGATE, {"CChaseInvestigateAI::TryMove", 0x46c0a0, 2, 0, 300, a_entity_seconds}},
    {S_INVESTIGATE, {"CChaseInvestigateAI::Update", 0x46c1c0, 2, 0, 900, a_entity_seconds}},
    {S_PURSUE, {"CChasePursueAI::Begin", 0x46caa0, 1, 0, 150, a_entity}},
    {S_PURSUE, {"CChasePursueAI::TryMove", 0x46cb10, 2, 0, 300, a_entity_seconds}},
    {S_PURSUE, {"CChasePursueAI::Update", 0x46cc40, 2, 0, 400, a_entity_seconds}},
    {S_COMPANION, {"CCompanionAI::Begin", 0x46e800, 1, 0, 150, a_entity}, 0, true},
    {S_COMPANION, {"CCompanionAI::Update", 0x46e890, 2, 0, 400, a_entity_seconds}, 0, true},
    {S_COMPANION, {"CCompanionAI::IsTarget", 0x46ea70, 3, 0xff, 200, a_target}},
    {S_COMPANION, {"CCompanionAI::PickSpot", 0x46ead0, 1, 0xffffffff, 200, a_out}},
    {S_LAYER_PART_AI, {"CLayerPartAI::Message", 0x4fd100, 2, 0xff, 200, a_message}},
    {S_LAYER_PART_AI, {"AI_Draw", 0x4fd0b0, 2, 0, 150, a_two}},
    {S_LAYER_PART_AI, {"AI_RunAction", 0x4fd160, 4, 0xffffffff, 100, a_run_action}},
    {S_BEAM, {"CLightBeamAI::Collide", 0x4fe5f0, 5, 0, 300, a_collide}, 0x487620},
    {S_AI_BASE, {"CAIBase::Dtor", 0x4fd080, 0, 0, 100, nullptr}},
    {S_LIST, {"DifficultyList::DeletingDtor", 0x4fe030, 1, 0xffffffff, 120, a_flags}},
};

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self = s.self;
        g_before_state = s.self == S_LIST ? tag_records : nullptr;
        g_after_side = s.self == S_LIST ? after_list : nullptr;
        Policy p{};
        p.find_minus_one = 4;
        if (s.flying)
            p.always[0] = at<Getter>(0x49bde0)();
        if (s.always)
            p.always[1] = at<Getter>(s.always)();
        const unsigned n = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, p);
    }
    g_before_state = nullptr;
    g_after_side = nullptr;
}

// Real data: copies of the running level's chase and companion AIs (at most 12 of each class, a spread)
// through Begin and Update in the fake world
void test_real(Tally &t)
{
    static_assert(sizeof(BYTE *) == 4);
    BYTE **parts = static_cast<BYTE **>(heap_alloc(8192 * 4));
    BYTE **ais = static_cast<BYTE **>(heap_alloc(64 * 4));
    const int nparts = level_parts(parts, 8192);
    struct Kind {
        DWORD var, size;
        int self;
        int slots[2];   // indices into SLOTS: Begin, Update
    } const kinds[] = {{0x6bd2d0, 0x110, S_INVESTIGATE, {0, 2}}, {0x6bd2d8, 0xd4, S_PURSUE, {3, 5}}, {0x6bd328, 0x10c, S_COMPANION, {6, 7}}};
    int counts[3] = {0, 0, 0};
    for (int pass = 0; pass < 2; pass++) {
        int seen[3] = {0, 0, 0};
        for (int i = 0; i < nparts; i++) {
            const int na = part_ais(parts[i], parts, nparts, ais, 64);
            for (int k = 0; k < na; k++)
                for (int c = 0; c < 3; c++) {
                    if (class_of(ais[k]) != *at<CClassInfo **>(kinds[c].var))
                        continue;
                    const int index = seen[c]++;
                    if (pass == 0 || !spread_pick(index, counts[c], 12))
                        continue;
                    g_real_self = ais[k];
                    g_real_size = kinds[c].size;
                    g_self = kinds[c].self;
                    for (int s : kinds[c].slots) {
                        Policy p{};
                        p.find_minus_one = 4;
                        if (SLOTS[s].flying)
                            p.always[0] = at<Getter>(0x49bde0)();
                        for (unsigned seed = 1; seed <= 4; seed++)
                            run_slot(t, SLOTS[s].t, seed * 7919u + (unsigned)index * 131u + SLOTS[s].t.address, make_self, p);
                    }
                }
        }
        if (pass == 0)
            for (int c = 0; c < 3; c++)
                counts[c] = seen[c];
    }
    g_real_self = nullptr;
    log("  ai_chase: %d parts; real AIs: %d chase investigate, %d chase pursue, %d companion", nparts, counts[0], counts[1], counts[2]);
    heap_free(ais);
    heap_free(parts);
}

}  // namespace

ZAX_CHECKS(ai_chase)
{
    const DWORD start = GetTickCount();
    Tally getters{"ai_chase: class getters"}, objects{"ai_chase: Create, CreateArray, destructors"}, fields{"ai_chase: RegisterFields"},
        real_fields{"ai_chase: registered descriptors and classes (real data)"}, small{"ai_chase: small functions, string kind clones"},
        slots{"ai_chase: chase, companion, base AI slots, difficulty list in the fake world"},
        real{"ai_chase: the level's chase and companion AIs as copies through Begin and Update (real data)"};
    SavedBytes saved_random, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    test_small(small);
    test_clones(small, cls, n);
    small.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        test_real(real);
        world_off();
    } else {
        slots.check(false, "ai_chase", "[the fake world could not be set up]");
    }
    slots.report();
    real.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random);
    log("  ai_chase: %u ms", GetTickCount() - start);
}

}  // namespace zax
