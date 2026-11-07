// Checks of the player's control AIs (ai_player.cpp) against the original, in the game thread.
//
//  - The classes: the class getters (both copies), Create/CreateArray with the deleting destructor,
//    RegisterFields and the registered descriptors (checks_inventory_common.h; CPlayerWalkingWeaponAI
//    has no RegisterFields of its own).
//  - The slots in the fake world (checks_ai_world.h): CPlayerControlAI's constructor, destructor, the
//    direction and player number setters and the mouse direction; CPlayerWalkingControlAI (a copy of a
//    Create'd object with its embedded move and idle behaviours' vtables recorder tables, the player
//    object a fake with a random mouse vector, random state bytes) through its constructor (the
//    "Direction Pointer" load with and without the model), Update, the keys, the fire, the weapon keys,
//    the key vector and firing direction, the respawn, Begin, slots 23/36/43/44, the precache and the
//    projectile filter; the weapon and item AIs' Update.
//  - Real data: the registered class infos; the level's AIs of these classes (copies) through their slots,
//    the entities copies of their parts.
#include "checks_ai_world.h"
#include "checks_inventory_common.h"
#include "checks_ai_player.h"

namespace zax {

int hooks_ai_player(Hook *out);

namespace {

using namespace aiw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CPlayerControlAI", 0x20, 0x7110ec, 0, 0, 0x542220, {0x542170, 0x5421c0}, false},
    {"CPlayerWalkingControlSettings", 0x40, 0x711108, 0x5425d0, 0x542600, 0x542740, {0x542580, 0x5426e0}, true},
    {"CPlayerWalkingControlAI", 0x120, 0x71110c, 0x542f30, 0x542f50, 0x543100, {0x542ed0, 0x543090}, true},
    {"CPlayerWalkingWeaponAI", 0x14, 0x711118, 0x544df0, 0x544e40, 0, {0x544da0, 0x544f70}, true},
    {"CPlayerWalkingItemAI", 0x1c, 0x71111c, 0x5450b0, 0x545100, 0x5452a0, {0x545060, 0x545240}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
constexpr int C_CONTROL = 0, C_WALKING = 2, C_WEAPON = 3, C_ITEM = 4;

// CreateArray with count elements, each side destroyed (slot 1 with 2, then the block freed) before the
// other side runs: the embedded move behaviours take a path checker from the pool, which then gives
// the other side the same blocks (invcheck::test_create_array keeps side 0's array alive meanwhile)
void test_create_array_in_turn(Tally &t, const invcheck::ClassDef &c, int count, BYTE pattern)
{
    using namespace invcheck;
    DWORD e[2], destroyed[2], used[2];
    BYTE checkers[2][4][8];
    bool arenas_same = false;
    const bool walking = c.size == 0x120;
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        arena_on(side, pattern);
        void *a = at<CreateN>(c.create_array)(count, 0, 0);
        e[side] = rel(invcheck::ptrv(a), side);
        used[side] = g_used[side];
        // CPlayerWalkingControlAI: each element's path checker pointer (+0xd4) by the checker's bytes
        BYTE saved[4][4];
        for (int i = 0; walking && a && i < count && i < 4; i++) {
            BYTE *o = static_cast<BYTE *>(a) + i * 0x120;
            memcpy(checkers[side][i], fl<BYTE *>(o, 0xd4), 8);
            memcpy(saved[i], o + 0xd4, 4);
            memset(o + 0xd4, 0, 4);
        }
        if (side == 0)
            memcpy(g_copy, g_arena[0], used[0]);
        else
            arenas_same = used[0] == used[1] && same_region(g_copy, g_arena[1], used[1]) &&
                          (!walking || !memcmp(checkers[0], checkers[1], 8 * (count < 4 ? count : 4)));
        for (int i = 0; walking && a && i < count && i < 4; i++)
            memcpy(static_cast<BYTE *>(a) + i * 0x120 + 0xd4, saved[i], 4);
        destroyed[side] = count > 0 && a ? rel(invcheck::ptrv(vcall<void *>(a, 1, 2)), side) : 0;
        arena_off();
        side_end(side);
    }
    char d[200];
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, destroyed %x / %x, first difference +%x: %x / %x]", count, e[0], e[1], used[0],
              used[1], destroyed[0], destroyed[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(arenas_same && e[0] == e[1] && destroyed[0] == destroyed[1], c.name, d);
}

// Create of CPlayerWalkingControlAI (0x120 bytes: from operator new, the arena) and its destructor with
// flag 1, side by side: its move behaviour's path checker (+0x20 +0xb4) is a pool block, compared by
// its 8 bytes (the pool gives the sides different blocks)
void test_create_walking(Tally &t, unsigned seed, bool load_pointer)
{
    using namespace invcheck;
    const ClassDef &c = CLASSES[C_WALKING];
    BYTE checker[2][8];
    DWORD loaded[2];
    const DWORD saved_pointer = *at<DWORD *>(0x711110);
    for (int side = 0; side < 2; side++) {
        if (load_pointer)
            *at<DWORD *>(0x711110) = 0;   // the constructor loads the "Direction Pointer" model's sequence again
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        memcpy(g_obj[side], o, c.size);
        memcpy(checker[side], fl<BYTE *>(o, 0xd4), 8);
        fl<DWORD>(g_obj[side], 0xd4) = 0;
        vcall<void *>(o, 1, 1);
        arena_off();
        side_end(side);
        loaded[side] = *at<DWORD *>(0x711110);
    }
    *at<DWORD *>(0x711110) = saved_pointer;
    char d[160];
    const bool same = same_region(g_obj[0], g_obj[1], c.size) && !memcmp(checker[0], checker[1], 8) && loaded[0] == loaded[1] &&
                      (!load_pointer || loaded[0] == saved_pointer);
    wsprintfA(d, "[Create seed %u: first difference +%x: %x / %x]", seed, g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_ai_player;
    if (!invcheck::arena_setup()) {
        objects.check(false, "ai_player", "[no arena]");
        return;
    }
    for (int c = 0; c < NCLASSES; c++) {
        const invcheck::ClassDef &d = CLASSES[c];
        for (DWORD gtr : d.getters)
            invcheck::test_getter(getters, d, gtr);
        if (d.create) {
            for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
                if (c == C_WALKING)
                    test_create_walking(objects, seed, seed <= 2);
                else
                    invcheck::test_create(objects, d, seed * 7 + (unsigned)c);
            for (int n = 0; n < 4; n++)
                test_create_array_in_turn(objects, d, n, (BYTE)(0x37 * n + c));
        }
        if (d.register_fields)
            for (unsigned seed = 1; seed <= 2; seed++)
                invcheck::test_register_fields(fields, real_fields, d, seed * 13 + (unsigned)c);
    }
}

// ---- The slots ---------------------------------------------------------------------------------------------

BYTE *g_template[NCLASSES];
int g_self_class;
const BYTE *g_real_self, *g_real_part;
BYTE *g_parts[8192];
bool g_construct;   // the constructor's test: the embedded behaviours stay real (destroyed again after the case)

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    const DWORD size = c == C_CONTROL ? 0x20 : CLASSES[c].size;
    if (g_construct) {   // a cleared block (the embedded behaviours' constructors leave pointers they test later)
        memset(s, 0, OBJ);
        return;
    }
    if (c == C_CONTROL) {   // the random block
        fl<DWORD>(s, 4) = 0;   // no dynamic properties, no broadcaster for the destructor
        fl<DWORD>(s, 0xc) = 0;
        fl<DWORD>(s, 0xd4) = 0;   // the move behaviour's path checker: made by its constructor only when 0
        return;
    }
    memcpy(s, g_real_self ? g_real_self : g_template[c], size);
    fl<DWORD>(s, 4) = 0;     // no dynamic properties, no broadcaster (a copy must not reach the real ones)
    fl<DWORD>(s, 0xc) = 0;
    if (g_real_part)
        for (int i = 0; i < N_ENTITY; i++) {
            BYTE *e = obj(O_ENTITY + i);
            memcpy(e, g_real_part, OBJ);
            fl<DWORD>(e, 0) = ptrv(g_fvt[T_ENTITY]);
            fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        }
    if (c == C_WALKING) {
        fl<DWORD>(s, 0x20) = ptrv(g_fvt[T_BEHAVIOR]);
        fl<DWORD>(s, 0xdc) = ptrv(g_fvt[T_BEHAVIOR]);
        fl<DWORD>(s, 0x10) = ptrv(obj(O_PLAYER));
        if (!g_real_self) {
            fl<DWORD>(s, 0x14) = g.next() % 3 ? 0 : g.fbits_(-1.0f, 1.0f);
            fl<DWORD>(s, 0x18) = g.next() % 3 ? 0 : g.fbits_(-1.0f, 1.0f);
            fl<DWORD>(s, 0x10c) = g.fbits_(0.0f, 4000.0f);
            fl<DWORD>(s, 0x110) = g.fbits_(0.0f, 4000.0f);
            for (DWORD off = 0x115; off <= 0x11e; off++)
                if (off < 0x118 || off > 0x11b)
                    fl<BYTE>(s, off) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x118) = g.fbits_(0.0f, 1000.0f);
        }
    } else if (c == C_ITEM && !g_real_self) {
        fl<DWORD>(s, 0x14) = g.next() % 4;
        fl<DWORD>(s, 0x18) = g.next() % 4;
    }
}

DWORD seconds(Gen &g) { return g.fbits_(0.0f, 0.1f); }
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = seconds(g);
}
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }
void a_vector(Gen &g, DWORD *a)
{
    fl<DWORD>(obj(O_OUT), 0) = g.fbits_(-1.0f, 1.0f);
    fl<DWORD>(obj(O_OUT), 4) = g.fbits_(-1.0f, 1.0f);
    a[0] = ptrv(obj(O_OUT));
}
void a_player_num(Gen &g, DWORD *a) { a[0] = g.next() % 4; }
void a_mouse(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_OUT));
    a[1] = ptrv(obj(O_OUT) + 8);
    a[2] = ptrv(obj(O_OUT) + 0x10);
    a[3] = g.next() % 4 ? g.next() % 100 : g.next() % 0x10000;
    a[4] = g.next() % 4 ? g.next() % 300 : 0;
}
void a_out(Gen &, DWORD *a) { a[0] = ptrv(obj(O_OUT)); }
void a_fire_direction(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g, 4));
    a[1] = ptrv(obj(O_OUT));
}
void a_entity_angle(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    fl<DWORD>(obj(O_OUT), 0) = g.fbits_(0.0f, 6.3f);
    a[1] = ptrv(obj(O_OUT));
}
const char *const FALLBACKS[] = {"Multiplayer Full Auto Pistol", "Pistol"};
void a_weapon_key(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = 0x1c + g.next() % 15;
    a[2] = g.next() % 3 ? 0 : ptrv(FALLBACKS[g.next() % 2]);
}
void a_x_entity(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = ptrv(any_entity(g));
}
void a_collides(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next();
}

struct ClassSlot {
    int cls;
    SlotTest t;
    int policy;
    bool fresh_only = false;
};
constexpr int P_PLAIN = 0, P_WALK = 1, P_WALK_NO_GAME_TYPE = 2, P_ROOT = 3, P_CONSTRUCT = 4, P_FIRE = 5;
const ClassSlot SLOTS[] = {
    {C_CONTROL, {"CPlayerControlAI::Ctor", 0x5422b0, 0, 0xffffffff, 100, nullptr}, P_PLAIN, true},
    {C_CONTROL, {"CPlayerControlAI::DeletingDtor", 0x542300, 1, 0xffffffff, 100, a_flags0}, P_PLAIN, true},
    {C_CONTROL, {"CPlayerControlAI::SetDirection", 0x542340, 1, 0, 50, a_vector}, P_PLAIN},
    {C_CONTROL, {"CPlayerControlAI::SetPlayerNum", 0x542550, 1, 0, 100, a_player_num}, P_PLAIN},
    {C_WALKING, {"CPlayerControlAI::MouseDirection", 0x542360, 5, 0, 1200, a_mouse}, P_WALK},
    {C_WALKING, {"CPlayerWalkingControlAI::Slot44", 0x543280, 0, 0xffffffff, 50, nullptr}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::LoadDirectionPointer", 0x5432a0, 0, 0xffffffff, 300, nullptr}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::Respawn", 0x5433e0, 2, 0xff, 300, a_x_entity}, P_WALK},
    {C_WALKING, {"CPlayerWalkingControlAI::Respawn (no game type)", 0x5433e0, 2, 0xff, 100, a_x_entity}, P_WALK_NO_GAME_TYPE},
    {C_WALKING, {"CPlayerWalkingControlAI::Begin", 0x543470, 1, 0, 200, a_entity}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::KeyVector", 0x5434e0, 1, 0xff, 1000, a_out}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::FireDirection", 0x5436f0, 2, 0xff, 1000, a_fire_direction}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::Fire", 0x543830, 2, 0, 1500, a_entity_angle}, P_FIRE},
    {C_WALKING, {"CPlayerWalkingControlAI::Update", 0x543b60, 2, 0, 2000, a_entity_seconds}, P_WALK},
    {C_WALKING, {"CPlayerWalkingControlAI::Update (no game type)", 0x543b60, 2, 0, 300, a_entity_seconds}, P_WALK_NO_GAME_TYPE},
    {C_WALKING, {"CPlayerWalkingControlAI::Demo", 0x544110, 0, 0, 300, nullptr}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::Slot23", 0x544240, 1, 0, 50, a_entity}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::Keys", 0x544260, 1, 0, 1500, a_entity}, P_WALK},
    {C_WALKING, {"CPlayerWalkingControlAI::Keys (no game type)", 0x544260, 1, 0, 300, a_entity}, P_WALK_NO_GAME_TYPE},
    {C_WALKING, {"CPlayerWalkingControlAI::WeaponKey", 0x544b10, 3, 0, 1000, a_weapon_key}, P_WALK},
    {C_WALKING, {"CPlayerWalkingControlAI::Precache", 0x544cc0, 0, 0, 20, nullptr}, P_PLAIN},
    {C_WALKING, {"CPlayerWalkingControlAI::Collides", 0x544cf0, 3, 0xff, 600, a_collides}, P_PLAIN},
    {C_WEAPON, {"CPlayerWalkingWeaponAI::Update", 0x544fd0, 2, 0xffffffff, 400, a_entity_seconds}, P_ROOT},
    {C_ITEM, {"CPlayerWalkingItemAI::Update", 0x5453c0, 2, 0xffffffff, 400, a_entity_seconds}, P_ROOT},
};

Policy policy_of(int kind)
{
    Policy p{};
    p.find_minus_one = 4;
    switch (kind) {
    case P_FIRE:
    case P_WALK:
    case P_WALK_NO_GAME_TYPE:
        // the code reads the walking character, the inventory, the undeployed items and the catalogue's
        // definitions without a test (null ends the game in the original too)
        p.always[0] = at<Getter>(0x49b810)();   // CEntityWalking
        p.always[1] = at<Getter>(0x49a930)();   // CEntityCharacter
        p.always[2] = at<Getter>(0x479d30)();   // the undeployed items
        p.nonnull = (1u << T_INVENTORY) | (1u << T_ITEM) | (1u << T_DEF);
        if (kind == P_WALK)
            p.nonnull |= 1u << T_GAMETYPE;
        if (kind == P_WALK_NO_GAME_TYPE)
            p.always_null = 1u << T_GAMETYPE;
        p.loading_never = true;
        break;
    case P_ROOT:
        p.always[0] = at<Getter>(0x49a930)();   // CEntityCharacter
        p.nonnull = 1u << T_ENTITY;
        break;
    case P_CONSTRUCT:
        p.find_minus_one = 6;
        p.real_list_vtables = true;
        p.real[0] = 0x417230;   // CObject_ResetFields: the behaviours' slot 19 resets them for real
        break;
    }
    return p;
}

// After a side of the constructor's test: the real behaviours it made destroyed (their pool blocks back
// before the other side)
void destroy_constructed()
{
    at<void(THISCALL *)(void *)>(0x430350)(obj(O_SELF) + 0x20);
    at<void(THISCALL *)(void *)>(0x498dc0)(obj(O_SELF) + 0xdc);
}

void run(Tally &t, const ClassSlot &s, unsigned seed)
{
    g_self_class = s.cls;
    g_construct = s.policy == P_CONSTRUCT;
    g_after_side = g_construct ? destroy_constructed : nullptr;
    run_slot(t, s.t, seed, make_self, policy_of(s.policy));
    g_after_side = nullptr;
    g_construct = false;
}

void test_slots(Tally &t)
{
    g_real_self = g_real_part = nullptr;
    for (const ClassSlot &s : SLOTS)
        for (unsigned seed = 1; seed <= (unsigned)scaled(s.t.count); seed++)
            run(t, s, seed * 2654435761u + s.t.address);
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
    const int n = level_parts(g_parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_player real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, ran = 0;
    for (int i = 0; i < n; i++) {
        BYTE *part = g_parts[i];
        BYTE *ais[64];
        const int count = part_ais(part, g_parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const CClassInfo *ci = class_of(ais[k]);
            for (int c = 0; c < NCLASSES; c++)
                if (ci && ci == *at<CClassInfo **>(CLASSES[c].var) && CLASSES[c].create && readable(ais[k], CLASSES[c].size)) {
                    found[c]++;
                    real_slots.check(fl<DWORD>(ais[k], 0) == PLAYER_CLASSES[c].vtable, CLASSES[c].name, "[vtable]");
                    if (found[c] > 20)
                        continue;
                    g_real_self = ais[k];
                    g_real_part = readable(part, OBJ) ? part : nullptr;
                    for (const ClassSlot &s : SLOTS)
                        if (s.cls == c && !s.fresh_only)
                            for (unsigned seed = 1; seed <= 4; seed++) {
                                run(real_slots, s, seed * 40503u + ptrv(ais[k]) + s.t.address);
                                ran++;
                            }
                    g_real_self = g_real_part = nullptr;
                }
        }
    }
    log("  ai_player real data: %d parts, %d CPlayerWalkingControlAI, %d weapon AIs, %d item AIs; %d cases", n, found[C_WALKING],
        found[C_WEAPON], found[C_ITEM], ran);
}

}  // namespace

ZAX_CHECKS(ai_player)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_player: class getters"}, objects{"ai_player: Create, CreateArray, destructors"}, fields{"ai_player: RegisterFields"},
        real_fields{"ai_player: RegisterFields against the registered fields (real data)"}, slots{"ai_player: slots in the fake world"},
        registered{"ai_player: registered class infos (real data)"},
        real_slots{"ai_player: the level's player AIs through their slots (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    for (int c = 0; c < NCLASSES; c++)
        g_template[c] = CLASSES[c].create ? static_cast<BYTE *>(at<void *(__cdecl *)()>(CLASSES[c].create)()) : nullptr;
    if (world_on()) {
        test_slots(slots);
        test_real(registered, real_slots);
        world_off();
    } else {
        slots.check(false, "ai_player", "[the fake world could not be set up]");
    }
    for (int c = 0; c < NCLASSES; c++)
        if (g_template[c])
            vcall<void *>(g_template[c], 1, 1);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random);
    log("  ai_player: %u ms", GetTickCount() - start);
}

}  // namespace zax
