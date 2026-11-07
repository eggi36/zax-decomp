// Checks of ai_tasks.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): the class getters (both copies; the class variable set and
//    0 with ClassInfo_Register replaced by a stand-in), Create on a pool block filled with a pattern and
//    the deleting destructor with flag 1, CreateArray with 0-3 elements (each side destroyed before the
//    other runs, as the move behaviours take pool blocks) and the destructor with flags 2/3, RegisterFields
//    into a fake class info and the registered descriptors against the reimplementation's; the
//    constructors CIdleAI 0x4cc7a0 and CGameType 0x4ceaa0 with the destructors 0x4ceb20/0x4cead0 on
//    filled blocks.
//  - The slots in the fake world (checks_task_world.h): the object under test is a copy of an object made
//    by Create, its owned objects fakes, its embedded behaviours with a recorder vtable, its fields random;
//    CGoToAI's own drawing slot 43 (not reimplemented) a recorder in a copy of its vtable.
//  - Real data: the registered class infos; the AIs of these classes on the running level's parts (and
//    the AIs their fields hold) as copies with their owned objects replaced by fakes, through the slots
//    of their class, at most 12 per class.
#include "checks_task_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_ai_tasks(Hook *out);

namespace {

using namespace tw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CWaitAI", 0x20, 0x6bfe68, 0x4cbfd0, 0x4cc010, 0x4cc170, {0x4cbf80, 0x4cc110}, true},
    {"CIdleAI", 0x40, 0x6bfe6c, 0x4cc4e0, 0x4cc530, 0x4cc6b0, {0x4cc490, 0x4cc650}, true},
    {"CForceUpdatingAI", 0x18, 0x6bfe70, 0x4cc890, 0x4cc8d0, 0x4cca40, {0x4cc840, 0x4cc9e0}, true},
    {"CLimitedTimeAI", 0x20, 0x6bfe74, 0x4ccca0, 0x4ccce0, 0x4cce40, {0x4ccc50, 0x4ccde0}, true},
    {"CHandleMessageAI", 0x18, 0x6bfe78, 0x4cd1d0, 0x4cd220, 0x4cd3d0, {0x4cd180, 0x4cd370}, true},
    {"CGeneratorAI", 0x48, 0x6c0450, 0x4cfa60, 0x4cfab0, 0x4cfc40, {0x4cfa10, 0x4cfbe0}, true},
    {"CGoToAI", 0x118, 0x6c0458, 0x4d0a00, 0x4d0a60, 0x4d0c80, {0x4d09a0, 0x4d0c10}, true},
    {"CSpawnableEntity", 0x10, 0x6c0448, 0x4cf300, 0x4cf340, 0x4cf4f0, {0x4cf440, 0}, true},
    {"CSpawnableCloneByName", 0x10, 0x6c044c, 0x4cf600, 0x4cf650, 0x4cf820, {0x4cf770, 0}, true},
    {"CGroundTextureFrame", 0x4c, 0x6c0460, 0x4d1da0, 0x4d1dd0, 0x4d1f20, {0x4d1d50, 0x4d1ec0}, true},
    {"CGameType", 0x18, 0x6c0430, 0, 0, 0x4ce900, {0x4ce850, 0x4ce8a0}, false},
    {"CSpawnableItem", 0xc, 0x6c0440, 0, 0, 0x4ceea0, {0x4cee40, 0}, false},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_WAIT, C_IDLE, C_FORCE, C_LIMITED, C_HANDLE, C_GENERATOR, C_GOTO, C_SPAWN_ENTITY, C_SPAWN_CLONE, C_FRAME, C_GAMETYPE, C_SPAWN_ITEM };

// ---- The classes ----------------------------------------------------------------------------------------

// CreateArray with count elements, each side destroyed (slot 1 with 2) before the other side runs
void test_create_array_in_turn(Tally &t, const invcheck::ClassDef &c, int count, BYTE pattern)
{
    using namespace invcheck;
    DWORD e[2], destroyed[2], used[2];
    bool arenas_same = false;
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
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
        invcheck::side_end(side);
    }
    char d[200];
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, destroyed %x / %x, first difference +%x: %x / %x]", count, e[0], e[1], used[0],
              used[1], destroyed[0], destroyed[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(arenas_same && e[0] == e[1] && destroyed[0] == destroyed[1], c.name, d);
}

// A constructor (and a destructor) on a block filled with random dwords: all bytes and eax compared
void test_ctor(Tally &t, const char *what, DWORD ctor, DWORD size, DWORD dtor, bool deleting)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++) {
        alignas(16) BYTE o[2][0x60];
        DWORD r[2], d[2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed * 31u + ctor;
            for (DWORD i = 0; i < size; i += 4)
                fl<DWORD>(o[side], i) = random();
            invcheck::side_begin(side);
            r[side] = at<DWORD(THISCALL *)(void *)>(ctor)(o[side]) - ptrv(o[side]);
            d[side] = 0;
            if (dtor)
                d[side] = deleting ? at<DWORD(THISCALL *)(void *, DWORD)>(dtor)(o[side], 0) - ptrv(o[side])
                                   : at<DWORD(THISCALL *)(void *)>(dtor)(o[side]);
            invcheck::side_end(side);
        }
        char text[160];
        wsprintfA(text, "[%s seed %u: eax %x / %x, destructor %x / %x]", what, seed, r[0], r[1], d[0], d[1]);
        t.check(!memcmp(o[0], o[1], size) && r[0] == r[1] && (deleting ? d[0] == d[1] : true), what, text);
    }
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_ai_tasks;
    if (!invcheck::arena_setup()) {
        objects.check(false, "ai_tasks", "[no arena]");
        return;
    }
    for (int c = 0; c < NCLASSES; c++)
        invcheck::warm_up(CLASSES[c]);
    for (int c = 0; c < NCLASSES; c++) {
        const invcheck::ClassDef &d = CLASSES[c];
        for (DWORD g : d.getters)
            if (g)
                invcheck::test_getter(getters, d, g);
        if (d.create && d.destroy) {
            for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
                invcheck::test_create(objects, d, seed * 7 + (unsigned)c);
            for (int n = 0; n < 4; n++)
                test_create_array_in_turn(objects, d, n, (BYTE)(0x31 * n + c));
        }
        if (d.register_fields)
            for (unsigned seed = 1; seed <= 2; seed++)
                invcheck::test_register_fields(fields, real_fields, d, seed * 13 + (unsigned)c);
    }
    // the third copy of CGameType's getter (slot 34 of the game type list)
    invcheck::ClassDef game_type = CLASSES[C_GAMETYPE];
    game_type.getters[0] = 0x4cec40;
    invcheck::test_getter(getters, game_type, 0x4cec40);
    // the constructors and destructors called on their own (the C heap from the arena: CGameType's
    // Local.txt entry)
    test_ctor(objects, "CIdleAI::Ctor", 0x4cc7a0, 0x40, 0x4cc5a0, true);
    test_ctor(objects, "CGameType::Ctor/DeletingDtor", 0x4ceaa0, 0x18, 0x4cead0, true);
    test_ctor(objects, "CGameType::Ctor/Dtor", 0x4ceaa0, 0x18, 0x4ceb20, false);
}

// ---- The slots ---------------------------------------------------------------------------------------------

BYTE *g_template[NCLASSES];
int g_self_class;
const BYTE *g_real_self;   // a copy of a real AI instead of a random one
const BYTE *g_real_part;   // its part: the fake entities become copies of it
BYTE *g_parts[8192];
DWORD g_goto_vtable[48];   // CGoToAI's vtable with slot 43 (its drawing) a recorder
ZString g_texts[5];        // valid names for the ZString fields
const char *const TEXTS[5] = {"", "Player", "Start Animation", "Idle", "Generated Thing"};

DWORD fake(int first, int n, Gen &g, int null_one_in)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(first + (int)(g.next() % (DWORD)n)));
}
DWORD fake_action(Gen &g, int null_one_in = 4) { return fake(O_ACTION, N_ACTION, g, null_one_in); }
DWORD fake_ai(Gen &g, int null_one_in = 4) { return fake(O_AI, N_AI, g, null_one_in); }
DWORD fake_behavior(Gen &g, int null_one_in = 4) { return fake(O_BEHAVIOR, N_BEHAVIOR, g, null_one_in); }
DWORD text(Gen &g) { return *reinterpret_cast<DWORD *>(&g_texts[g.next() % 5]); }
// Seconds, the unusual values included but none that would keep a count-down loop going for ever
DWORD seconds(Gen &g)
{
    const DWORD v = g.next() % 16;
    if (v == 0)
        return 0;
    if (v == 1)
        return 0x80000000u;
    if (v == 2)
        return 0x7fc00000u;
    return g.fbits_(0.0f, 0.3f);
}
DWORD time_value(Gen &g, float lo, float hi)
{
    const DWORD v = g.next() % 16;
    const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xbf800000u};
    return v < 5 ? sp[v] : g.fbits_(lo, hi);
}
// A float in [lo, hi) without the unusual values
DWORD plain(Gen &g, float lo, float hi) { return bits(lo + (hi - lo) * (float)(int)(g.next() >> 8) * (1.0f / 16777216.0f)); }
void fake_behavior_vtable(BYTE *s, DWORD off) { fl<DWORD>(s, off) = ptrv(g_fvt[T_BEHAVIOR]); }
// An owned pointer of a copied real object: a fake of the kind, or 0 when it was 0
void replace_owned(BYTE *s, DWORD off, DWORD replacement)
{
    if (fl<DWORD>(s, off))
        fl<DWORD>(s, off) = replacement;
}

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    memcpy(s, g_real_self ? g_real_self : g_template[c], CLASSES[c].size);
    fl<DWORD>(s, 4) = 0;   // no dynamic properties, no broadcaster (a copy must not reach the real ones)
    if (c != C_SPAWN_ENTITY && c != C_SPAWN_CLONE)
        fl<DWORD>(s, 0xc) = 0;
    if (g_real_part)
        for (int i = 0; i < N_ENTITY; i++) {
            BYTE *e = obj(O_ENTITY + i);
            memcpy(e, g_real_part, OBJ);
            fl<DWORD>(e, 0) = ptrv(g_fvt[T_ENTITY]);
            fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
            fl<DWORD>(e, 0x9c) = g.next() % 4;
            fl<DWORD>(e, 0x40) = g.next() % 3;
        }
    const bool r = g_real_self != nullptr;
    switch (c) {
    case C_WAIT:
        if (!r) {
            fl<DWORD>(s, 0x10) = time_value(g, 0.0f, 3.0f);
            fl<DWORD>(s, 0x14) = time_value(g, -1.0f, 1.0f);
            fl<DWORD>(s, 0x18) = fake_action(g);
            fl<DWORD>(s, 0x1c) = fake_behavior(g);
        } else {
            replace_owned(s, 0x18, fake_action(g, 0));
            replace_owned(s, 0x1c, fake_behavior(g, 0));
        }
        break;
    case C_IDLE:
        fake_behavior_vtable(s, 0x10);
        break;
    case C_FORCE:
        if (!r) {
            fl<DWORD>(s, 0x10) = fake_ai(g);
            fl<DWORD>(s, 0x14) = g.next() % 600;
        } else {
            replace_owned(s, 0x10, fake_ai(g, 0));
        }
        break;
    case C_LIMITED:
        if (!r) {
            fl<DWORD>(s, 0x10) = time_value(g, 0.0f, 3.0f);
            fl<DWORD>(s, 0x14) = time_value(g, -1.0f, 1.0f);
            fl<DWORD>(s, 0x18) = fake_ai(g, 6);
            fl<DWORD>(s, 0x1c) = fake_action(g);
        } else {
            replace_owned(s, 0x18, fake_ai(g, 0));
            replace_owned(s, 0x1c, fake_action(g, 0));
        }
        break;
    case C_HANDLE:
        if (!r) {
            fl<DWORD>(s, 0x10) = text(g);
            fl<DWORD>(s, 0x14) = fake_action(g);
        } else {
            replace_owned(s, 0x14, fake_action(g, 0));
        }
        break;
    case C_GENERATOR: {
        // the items: fake definitions (slot 23 spawns) with a weight at +8, in a pointer array; the AIs to add
        BYTE *items = obj(O_SPARE), *ais = obj(O_SPARE) + 0x40;
        const DWORD n = r ? (fl<DWORD>(s, 0x20) < 8 ? fl<DWORD>(s, 0x20) : 8) : g.next() % 5;
        for (DWORD i = 0; i < 8; i++) {
            fl<DWORD>(items, 4 * i) = g.next() % 6 ? ptrv(obj(O_DEF + (int)(g.next() % N_DEF))) : 0;
            fl<DWORD>(ais, 4 * i) = fake_ai(g, 5);
        }
        for (int i = 0; i < N_DEF; i++)
            fl<DWORD>(obj(O_DEF + i), 8) = g.next() % 4 ? g.next() % 6 : g.next();
        fl<DWORD>(s, 0x1c) = ptrv(items);
        fl<DWORD>(s, 0x20) = n;
        if (!r || fl<DWORD>(s, 0x30))
            fl<DWORD>(s, 0x2c) = ptrv(ais);
        if (!r) {
            fl<DWORD>(s, 0x10) = g.next() % 5;
            const DWORD k = g.next() % 8;
            fl<DWORD>(s, 0x14) = k == 0 ? 0 : k == 1 ? 0xbf800000u : k == 2 ? 0x7fc00000u : k == 3 ? 0x7f800000u : plain(g, 0.05f, 3.0f);
            fl<BYTE>(s, 0x18) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x24) = text(g);
            fl<BYTE>(s, 0x28) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x30) = g.next() % 4;
            fl<DWORD>(s, 0x34) = fake_action(g);
            fl<DWORD>(s, 0x38) = g.next() % 3;
            fl<DWORD>(s, 0x3c) = plain(g, -1.0f, 2.0f);   // NaN would loop for ever
            fl<DWORD>(s, 0x40) = time_value(g, -1.0f, 7.0f);
            fl<DWORD>(s, 0x44) = time_value(g, 0.0f, 4.0f);
        } else {
            replace_owned(s, 0x34, fake_action(g, 0));
            if (fl<DWORD>(s, 0x30) > 8)
                fl<DWORD>(s, 0x30) = 8;
            if (!(fl<float>(s, 0x14) >= 0.05f) && fl<float>(s, 0x14) > 0.0f)
                fl<DWORD>(s, 0x14) = 0x3f800000;   // a delay that small would take ages
        }
        break;
    }
    case C_GOTO:
        fl<DWORD>(s, 0) = ptrv(g_goto_vtable);
        fake_behavior_vtable(s, 0x1c);
        fake_behavior_vtable(s, 0xdc);
        if (!r) {
            const DWORD bytes[] = {0x10, 0x11, 0x18, 0x19, 0x1a, 0xd8, 0x108};
            for (DWORD off : bytes)
                fl<BYTE>(s, off) = (BYTE)(g.next() % 3 == 0);
            fl<DWORD>(s, 0x14) = text(g);
            fl<DWORD>(s, 0xc0) = g.next() % 3 ? 0 : g.fbits_(-1.0f, 2.0f);
            fl<DWORD>(s, 0xc4) = g.next();
            fl<DWORD>(s, 0xf8) = fake_action(g);
            fl<DWORD>(s, 0xfc) = fake_action(g);
            fl<DWORD>(s, 0x100) = g.fbits_(-50.0f, 50.0f);
            fl<DWORD>(s, 0x104) = g.fbits_(-50.0f, 50.0f);
            fl<DWORD>(s, 0x10c) = g.fbits_(0.0f, 2.0f);
            fl<DWORD>(s, 0x110) = time_value(g, -0.5f, 1.0f);
            fl<DWORD>(s, 0x114) = text(g);
        } else {
            replace_owned(s, 0xf8, fake_action(g, 0));
            replace_owned(s, 0xfc, fake_action(g, 0));
            fl<DWORD>(s, 0xc4) ^= g.next() & 0x40;
        }
        break;
    case C_SPAWN_ENTITY:
        fl<DWORD>(s, 0xc) = r ? (fl<DWORD>(s, 0xc) ? ptrv(any_entity(g)) : 0) : ptrv(any_entity(g, 4));
        break;
    case C_SPAWN_CLONE:
        if (!r)
            fl<DWORD>(s, 0xc) = text(g);
        break;
    default:
        break;
    }
}

DWORD entity(Gen &g) { return ptrv(any_entity(g)); }
void a_part(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_part_or_null(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g, 5)); }
void a_part_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = seconds(g);
}
// CGeneratorAI's count-down loops while the time is not above 0: NaN seconds would keep it going for
// ever (in the original too), so its seconds are finite
void a_part_finite_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = g.next() % 8 ? plain(g, 0.0f, 0.3f) : 0;
}
void a_two(Gen &g, DWORD *a)
{
    a[0] = g.next() % 2 ? entity(g) : g.next();
    a[1] = g.next();
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next() % 4 ? g.next() & 0xff : 0;
}
// A message object {vtable 0x5eeb64, id, a byte}: one of the ids the slots look for or another
DWORD message(Gen &g)
{
    BYTE *m = obj(O_OUT);
    const DWORD ids[] = {0x6c29e8, 0x6c29e0, 0x6c29d8, 0x6c29d4};
    fl<DWORD>(m, 0) = 0x5eeb64;
    const DWORD k = g.next() % 5;
    fl<DWORD>(m, 4) = k < 4 ? *at<DWORD *>(ids[k]) : g.next();
    fl<DWORD>(m, 8) = g.next() & 0xff;
    return ptrv(m);
}
void a_part_message(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = message(g);
}
void a_object(Gen &g, DWORD *a) { a[0] = ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))); }
void a_spawn(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(obj(O_AI));
}
void a_generator_spawn(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    if (!fl<DWORD>(obj(O_SELF), 0x20))
        fl<DWORD>(obj(O_SELF), 0x20) = 1;   // an index the array has
    a[1] = g.next() % fl<DWORD>(obj(O_SELF), 0x20);
}
// CLimitedTimeAI's Begin calls "What To Do" without a test
void a_limited_begin(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    if (!fl<DWORD>(obj(O_SELF), 0x18))
        fl<DWORD>(obj(O_SELF), 0x18) = ptrv(obj(O_AI));
}
void a_read_line(Gen &g, DWORD *a)
{
    const char *const names[] = {"X Offset", "y offset", "Destination", "X Offse"};
    const char *const values[] = {"12.5", "-3", "abc", "1e40"};
    a[0] = ptrv(names[g.next() % 4]);
    a[1] = ptrv(values[g.next() % 4]);
    a[2] = g.next();
}

struct ClassSlot {
    int cls;
    SlotTest t;
};
const ClassSlot SLOTS[] = {
    {C_WAIT, {"CWaitAI::Begin", 0x4cc3b0, 1, 0, 120, a_part_or_null}},
    {C_WAIT, {"CWaitAI::Update", 0x4cc400, 2, 0, 200, a_part_seconds}},
    {C_IDLE, {"CIdleAI::Begin", 0x4cc7d0, 1, 0, 80, a_part_or_null}},
    {C_IDLE, {"CIdleAI::Update", 0x4cc800, 2, 0, 80, a_part_seconds}},
    {C_FORCE, {"CForceUpdatingAI::Begin", 0x4ccb60, 1, 0, 80, a_part}},
    {C_FORCE, {"CForceUpdatingAI::Message", 0x4ccb90, 2, 0xff, 120, a_part_message}},
    {C_FORCE, {"CForceUpdatingAI::Collide", 0x4ccbe0, 5, 0, 60, a_collide}},
    {C_FORCE, {"CForceUpdatingAI::Update", 0x4ccc10, 2, 0, 60, a_part_seconds}},
    {C_FORCE, {"CForceUpdatingAI::Draw", 0x4ccc30, 2, 0, 60, a_two}},
    {C_LIMITED, {"CLimitedTimeAI::Begin", 0x4cd080, 1, 0, 80, a_limited_begin}},
    {C_LIMITED, {"CLimitedTimeAI::Update", 0x4cd0a0, 2, 0, 200, a_part_seconds}},
    {C_LIMITED, {"CLimitedTimeAI::Collide", 0x4cd110, 5, 0, 80, a_collide}},
    {C_LIMITED, {"CLimitedTimeAI::Message", 0x4cd150, 2, 0xff, 80, a_part_message}},
    {C_HANDLE, {"CHandleMessageAI::Message", 0x4cd4f0, 2, 0xff, 160, a_part_message}},
    {C_HANDLE, {"CHandleMessageAI::Draw", 0x4cd540, 2, 0, 60, a_two}},
    {C_GENERATOR, {"CGeneratorAI::Begin", 0x4d0410, 1, 0xffffffff, 40, a_part}},
    {C_GENERATOR, {"CGeneratorAI::Update", 0x4d0420, 2, 0, 400, a_part_finite_seconds}},
    {C_GENERATOR, {"CGeneratorAI::Generate", 0x4d0520, 1, 0, 200, a_part}},
    {C_GENERATOR, {"CGeneratorAI::Spawn", 0x4d05a0, 2, 0, 400, a_generator_spawn}},
    {C_GENERATOR, {"CGeneratorAI::Full", 0x4d08e0, 1, 0xff, 200, a_part}},
    {C_GOTO, {"CGoToAI::ReadLine", 0x4d1520, 3, 0, 60, a_read_line}},
    {C_GOTO, {"CGoToAI::Collide", 0x4d15a0, 5, 0, 120, a_collide}},
    {C_GOTO, {"CGoToAI::Message", 0x4d15e0, 2, 0xff, 160, a_part_message}},
    {C_GOTO, {"CGoToAI::Begin", 0x4d1640, 1, 0, 160, a_part}},
    {C_GOTO, {"CGoToAI::CheckTarget", 0x4d1680, 1, 0, 120, a_part}},
    {C_GOTO, {"CGoToAI::Update", 0x4d16b0, 2, 0, 500, a_part_seconds}},
    {C_GOTO, {"CGoToAI::Draw", 0x4d1af0, 2, 0, 60, a_two}},
    {C_GOTO, {"CGoToAI::StartMove", 0x4d1b20, 1, 0, 300, a_part}},
    {C_GOTO, {"CGoToAI::StartTurn", 0x4d1c30, 1, 0, 120, a_part_or_null}},
    {C_GOTO, {"CGoToAI::Finish", 0x4d1c90, 1, 0, 160, a_part}},
    {C_GOTO, {"CGoToAI::Done", 0x4d1ce0, 1, 0, 160, a_part}},
    {C_SPAWN_ENTITY, {"CSpawnableEntity::Spawn", 0x4cf5d0, 2, 0xffffffff, 80, a_spawn}},
    {C_SPAWN_CLONE, {"CSpawnableCloneByName::Spawn", 0x4cf9a0, 2, 0xffffffff, 120, a_spawn}},
    {C_SPAWN_ENTITY, {"CGameType::Start", 0x4cebb0, 0, 0, 80, nullptr}},
    {C_SPAWN_ENTITY, {"CQuestGameType::ScoreName", 0x4ceba0, 0, 0xffffffff, 2, nullptr}},
    {C_SPAWN_ENTITY, {"CQuestGameType::Slot28", 0x4cec30, 1, 0xffffffff, 10, a_object}},
    {C_SPAWN_ENTITY, {"CObject::AppSlot14", 0x4ceb60, 0, 0xffffffff, 20, nullptr}},
};

Policy task_policy()
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
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, task_policy());
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
                             (!c.create_array || ptrv((void *)ci->create_array) == c.create_array) && at<Getter>(c.getters[0])() == ci,
                         c.name, d);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_tasks real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, ran = 0;
    for (int i = 0; i < n; i++) {
        const BYTE *part = parts[i];
        BYTE *ais[64];
        const int count = part_ais(part, parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const CClassInfo *ci = class_of(ais[k]);
            for (int c = 0; c < NCLASSES; c++) {
                if (!ci || ci != *at<CClassInfo **>(CLASSES[c].var) || !CLASSES[c].create || !readable(ais[k], CLASSES[c].size))
                    continue;
                if (found[c]++ >= 12)
                    continue;   // enough of one class
                g_self_class = c;
                g_real_self = ais[k];
                g_real_part = readable(part, OBJ) ? part : nullptr;
                for (const ClassSlot &s : SLOTS)
                    if (s.cls == c && s.t.address != 0x4cebb0 && s.t.address != 0x4ceba0 && s.t.address != 0x4cec30 && s.t.address != 0x4ceb60)
                        for (unsigned seed = 1; seed <= 2; seed++) {
                            run_slot(real_slots, s.t, seed * 40503u + ptrv(ais[k]), make_self, task_policy());
                            ran++;
                        }
                g_real_self = nullptr;
                g_real_part = nullptr;
            }
        }
    }
    log("  ai_tasks real data: %d parts, AIs %d CWaitAI, %d CIdleAI, %d CForceUpdatingAI, %d CLimitedTimeAI, %d CHandleMessageAI, %d "
        "CGeneratorAI, %d CGoToAI; %d cases",
        n, found[C_WAIT], found[C_IDLE], found[C_FORCE], found[C_LIMITED], found[C_HANDLE], found[C_GENERATOR], found[C_GOTO], ran);
}

}  // namespace

ZAX_CHECKS(ai_tasks)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_tasks: class getters"}, objects{"ai_tasks: Create, CreateArray, constructors, destructors"},
        fields{"ai_tasks: RegisterFields"}, real_fields{"ai_tasks: RegisterFields against the registered fields (real data)"},
        slots{"ai_tasks: slots in the fake world"}, registered{"ai_tasks: registered class infos (real data)"},
        real_slots{"ai_tasks: the level's AIs through their slots (real data)"};
    SavedBytes saved_random, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    for (int c = 0; c < NCLASSES; c++)
        g_template[c] = CLASSES[c].create ? static_cast<BYTE *>(at<void *(__cdecl *)()>(CLASSES[c].create)()) : nullptr;
    for (int i = 0; i < 5; i++)
        if (!g_texts[i].buffer)
            at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_texts[i], TEXTS[i]);
    memcpy(g_goto_vtable, at<void *>(0x5f7894), sizeof g_goto_vtable);
    make_static_texts();
    if (world_on()) {
        g_goto_vtable[43] = g_fvt[T_AI][43];
        test_slots(slots);
        test_real(registered, real_slots);
        world_off();
    } else {
        slots.check(false, "ai_tasks", "[the fake world could not be set up]");
    }
    for (int c = 0; c < NCLASSES; c++)
        if (g_template[c])
            vcall<void *>(g_template[c], 1, 1);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random);
    log("  ai_tasks: %u ms", GetTickCount() - start);
}

}  // namespace zax
