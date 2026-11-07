// Checks of zax_behaviors.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters (both copies), Create, CreateArray, deleting
//    destructors, RegisterFields and the registered descriptors of CCameraTrakerAI, CEntityBehaviorWarpEffect,
//    CEntityBehaviorFireWeapon, -FirePrimary, -FireSecondary, CEntityBehaviorRestartMap; the out-of-line
//    constructors of the camera AI and the warp behaviour on pool blocks filled with a pattern.
//  - The slots in the fake world (checks_rest_world.h): the camera's Update (speeds 0, negative, NaN,
//    huge; network game or not), the warp behaviour's Make (a new pool object, its fields reset by the real
//    slot 19, freed by the original destructor after each side), Begin and Update (warping in and out, before
//    and after the warp time), the fire behaviours' Make, Begin, the primary fire and the drop of a
//    secondary item (weapons that can or cannot fire, stacks empty or not, behaviours pushed or not, the
//    discharge action on the stack, batteries, projectiles), the restart behaviour's Begin on the fake game
//    object, and the six client block readers' registration, accessors, reads and writes (packet functions,
//    the net game, its net layer and GameSpy's CD key check are stand-ins; the game is never restarted, no
//    player is disconnected).
#include "checks_rest_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_zax_behaviors(Hook *out);

namespace {

using namespace rw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CCameraTrakerAI", 0x20, 0x714030, 0x5aa070, 0x5aa0c0, 0x5aa240, {0x5aa020, 0x5aa1e0}, true},
    {"CEntityBehaviorWarpEffect", 0x20, 0x71403c, 0x5aa6c0, 0x5aa700, 0x5aa880, {0x5aa670, 0x5aa820}, true, false, nullptr, 0x18},
    {"CEntityBehaviorFireWeapon", 0x10, 0x714040, 0x5aae50, 0x5aae80, 0x5aafc0, {0x5aae00, 0x5aaf60}, true},
    {"CEntityBehaviorFirePrimary", 0x10, 0x714044, 0x5ab100, 0x5ab140, 0, {0x5ab050, 0x5ab240}, true},
    {"CEntityBehaviorFireSecondary", 0x10, 0x714048, 0x5ab700, 0x5ab740, 0, {0x5ab650, 0x5ab840}, true},
    {"CEntityBehaviorRestartMap", 0xc, 0x71404c, 0x5abc00, 0x5abc30, 0, {0x5abbb0, 0x5abd10}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

// An out-of-line constructor on a pool block filled with a pattern (all bytes compared, the C heap from
// the arena), then the object destroyed through its slot 1 with flag 1
void test_ctor(Tally &t, const invcheck::ClassDef &c, DWORD ctor, unsigned seed)
{
    using namespace invcheck;
    bool reused[2];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        invcheck::side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        arena_on(side, (BYTE)seed);
        r[side] = at<DWORD(THISCALL *)(void *)>(ctor)(p) == invcheck::ptrv(p);
        reused[side] = true;
        memcpy(g_obj[side], p, c.size);
        vcall<void *>(p, 1, 1);
        arena_off();
        invcheck::side_end(side);
    }
    char d[160];
    wsprintfA(d, "[ctor 0x%x seed %u: eax this %u / %u, heap %u / %u, first difference +%x: %x / %x]", ctor, seed, r[0], r[1], g_used[0],
              g_used[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(reused[0] && reused[1] && r[0] && r[1] && g_used[0] == g_used[1] && same_region(g_obj[0], g_obj[1], c.size) &&
                same_region(g_arena[0], g_arena[1], g_used[0]),
            c.name, d);
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_zax_behaviors;
    if (!invcheck::arena_setup()) {
        objects.check(false, "zax_behaviors", "[no arena]");
        return;
    }
    invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++) {
        test_ctor(objects, CLASSES[0], 0x5aa4d0, seed);
        test_ctor(objects, CLASSES[1], 0x5aabb0, seed + 100);
    }
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
    return g.fbits_(0.0f, 0.5f);
}
DWORD entity(Gen &g) { return ptrv(any_entity(g)); }
DWORD other(Gen &g, int null_one_in)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER)));
}

enum Self { S_CAMERA, S_WARP, S_BEHAVIOR, S_READER, NSELF };
int g_self;
void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    switch (g_self) {
    case S_CAMERA:
        fl<DWORD>(s, 0) = 0x603c98;
        fl<DWORD>(s, 0x10) = g.next() % 4;
        fl<BYTE>(s, 0x14) = (BYTE)(g.next() % 5 != 0);
        fl<DWORD>(s, 0x18) = g.next() % 5 == 0 ? 0x4cbebc20u : time_value(g, -50.0f, 900.0f);
        fl<BYTE>(s, 0x1c) = (BYTE)(g.next() % 2);
        break;
    case S_WARP:
        fl<DWORD>(s, 0) = 0x603d40;
        fl<BYTE>(s, 0xc) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0xd) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x10) = g.next() % 6 == 0 ? time_value(g, -1.0f, 2.0f) : g.fbits_(0.2f, 3.0f);
        fl<DWORD>(s, 0x14) = time_value(g, -0.5f, 3.0f);
        fl<DWORD>(s, 0x18) = other(g, 4);
        fl<DWORD>(s, 0x1c) = g.next() % 2 ? entity(g) : 0;
        break;
    case S_BEHAVIOR:
        fl<DWORD>(s, 0) = 0x603e38;
        fl<DWORD>(s, 0xc) = g.fbits_(-1.0f, 6.3f);
        break;
    default:
        break;
    }
}

void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = seconds(g);
}
void a_seconds_entity(Gen &g, DWORD *a)
{
    a[0] = seconds(g);
    a[1] = entity(g);
}
void a_entity(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_entity_or_null(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g, 5)); }
void a_entity_count(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = g.next() % 4 ? g.next() % 4 : g.next();
}
void a_warp_make(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = other(g, 3);
    a[2] = time_value(g, 0.0f, 3.0f);
    a[3] = g.next();
    a[4] = g.next();
}
void a_angle(Gen &g, DWORD *a) { a[0] = time_value(g, -1.0f, 6.3f); }
void a_packet(Gen &, DWORD *a) { a[0] = ptrv(obj(O_PACKET)); }
const char *const CHAT[] = {"", "gg", "Where is the ore?", "%s %n %d"};
void a_packet_text(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_PACKET));
    a[1] = ptrv(CHAT[g.next() % 4]);
}
void a_answer(Gen &g, DWORD *a)
{
    a[0] = g.next() % 100;
    a[1] = g.next() % 3 == 0 ? g.next() : 0;
    a[2] = ptrv(g.next() % 2 ? "Bad CD Key" : "CD Key in use");
    a[3] = g.next();
}

// The objects Make gives (pool blocks of the real classes): their bytes into the log, then freed by the
// original's destructor after the side
DWORD g_made_dtor;
DWORD g_made_size;
void hash_made()
{
    if (g_last_eax && readable(at<void *>(g_last_eax), g_made_size))
        g_log[g_side].hash = fnv(g_log[g_side].hash, at<void *>(g_last_eax), g_made_size);
}
void free_made()
{
    if (g_last_eax)
        original<void *(THISCALL *)(void *, DWORD)>(g_made_dtor)(at<void *>(g_last_eax), 1);
}

struct ClassSlot {
    int self;
    SlotTest t;
    DWORD made_dtor, made_size;   // Make: the original's deleting destructor and the size of what it gives
    DWORD nonnull;                // fakes of these types never 0
};
constexpr DWORD NN_ITEM_DEF = 1u << T_ITEM | 1u << T_DEF;
const ClassSlot SLOTS[] = {
    {S_CAMERA, {"CCameraTrakerAI::Update", 0x5aa510, 2, 0, 600, a_entity_seconds}, 0, 0, 0},
    {S_WARP, {"EntityBehaviorWarpEffect_Make", 0x5aabe0, CALL_FASTCALL5, 0xffffffff, 60, a_warp_make}, 0x5aa760, 0x20, 0},
    {S_WARP, {"CEntityBehaviorWarpEffect::Begin", 0x5aac50, 1, 0xff, 80, a_entity}, 0, 0, 0},
    {S_WARP, {"CEntityBehaviorWarpEffect::Update", 0x5aac90, 2, 0xff, 600, a_seconds_entity}, 0, 0, 0},
    {S_BEHAVIOR, {"EntityBehaviorFirePrimary_Make", 0x5ab300, 1, 0xffffffff, 40, a_angle}, 0x5ab1a0, 0x10, 0},
    {S_BEHAVIOR, {"EntityBehaviorFireSecondary_Make", 0x5ab900, 1, 0xffffffff, 40, a_angle}, 0x5ab7a0, 0x10, 0},
    {S_BEHAVIOR, {"CEntityBehaviorFirePrimary::Begin", 0x5ab360, 1, 0xff, 300, a_entity}, 0, 0, 0},
    {S_BEHAVIOR, {"CEntityBehaviorFirePrimary::Fire", 0x5ab390, 2, 0xff, 500, a_entity_count}, 0, 0, 0},
    {S_BEHAVIOR, {"CEntityBehaviorFireSecondary::Begin", 0x5ab960, 1, 0xff, 300, a_entity_or_null}, 0, 0, NN_ITEM_DEF},
    {S_BEHAVIOR, {"Zax_DropSecondary", 0x5ab9a0, 1, 0xff, 500, a_entity}, 0, 0, NN_ITEM_DEF},
    {S_BEHAVIOR, {"CEntityBehaviorRestartMap::Begin", 0x5abd70, 1, 0xff, 20, a_entity}, 0, 0, 0},
    {S_READER, {"ClientReaders_Register", 0x5abd90, 0, 0xffffffff, 10, nullptr}, 0, 0, 0},
    {S_READER, {"ClientOptionsReader_Get", 0x5abe30, CALL_CDECL0, 0xffffffff, 2, nullptr}, 0, 0, 0},
    {S_READER, {"CdKeyResponseReader_Get", 0x5ac0a0, CALL_CDECL0, 0xffffffff, 2, nullptr}, 0, 0, 0},
    {S_READER, {"ReadyToSpawnReader_Get", 0x5ac210, CALL_CDECL0, 0xffffffff, 2, nullptr}, 0, 0, 0},
    {S_READER, {"ClientChatReader_Get", 0x5ac380, CALL_CDECL0, 0xffffffff, 2, nullptr}, 0, 0, 0},
    {S_READER, {"ChangeTeamReader_Get", 0x5ac4a0, CALL_CDECL0, 0xffffffff, 2, nullptr}, 0, 0, 0},
    {S_READER, {"ContinueReader_Get", 0x5ac560, CALL_CDECL0, 0xffffffff, 2, nullptr}, 0, 0, 0},
    {S_READER, {"ClientOptionsReader::Read", 0x5abe70, 1, 0, 400, a_packet}, 0, 0, 0},
    {S_READER, {"ClientOptionsReader::Write", 0x5abfe0, 0, 0, 100, nullptr}, 0, 0, 0},
    {S_READER, {"CdKeyResponse_Answer", 0x5ac0b0, CALL_CDECL4, 0, 100, a_answer}, 0, 0, 0},
    {S_READER, {"CdKeyResponseReader::Read", 0x5ac100, 1, 0xffffffff, 100, a_packet}, 0, 0, 0},
    {S_READER, {"CdKeyResponseReader::Write", 0x5ac180, 2, 0xff, 40, a_packet_text}, 0, 0, 0},
    {S_READER, {"ReadyToSpawnReader::Read", 0x5ac220, 1, 0, 200, a_packet}, 0, 0, 0},
    {S_READER, {"ReadyToSpawnReader::Write", 0x5ac330, 1, 0xff, 20, a_packet}, 0, 0, 0},
    {S_READER, {"ClientChatReader::Read", 0x5ac390, 1, 0, 200, a_packet}, 0, 0, 0},
    {S_READER, {"ClientChatReader::Write", 0x5ac440, 2, 0xff, 40, a_packet_text}, 0, 0, 0},
    {S_READER, {"ChangeTeamReader::Read", 0x5ac4b0, 1, 0, 60, a_packet}, 0, 0, 0},
    {S_READER, {"ContinueReader::Write", 0x5ac500, 0, 0xff, 20, nullptr}, 0, 0, 0},
    {S_READER, {"ContinueReader::Read", 0x5ac570, 1, 0, 60, a_packet}, 0, 0, 0},
};

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self = s.self;
        g_made_dtor = s.made_dtor;
        g_made_size = s.made_size;
        g_before_state = s.made_dtor ? hash_made : nullptr;
        g_after_side = s.made_dtor ? free_made : nullptr;
        Policy p{};
        p.find_minus_one = 4;
        p.nonnull = s.nonnull;
        const unsigned n = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, p);
    }
    g_before_state = nullptr;
    g_after_side = nullptr;
}

}  // namespace

ZAX_CHECKS(zax_behaviors)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"zax_behaviors: class getters"}, objects{"zax_behaviors: Create, CreateArray, constructors, destructors"},
        fields{"zax_behaviors: RegisterFields"}, real_fields{"zax_behaviors: registered descriptors and classes (real data)"},
        slots{"zax_behaviors: slots, Make, the client block readers in the fake world"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        world_off();
    } else {
        slots.check(false, "zax_behaviors", "[the fake world could not be set up]");
    }
    slots.report();
    restore_bytes(saved_random);
    log("  zax_behaviors: %u ms", GetTickCount() - start);
}

}  // namespace zax
