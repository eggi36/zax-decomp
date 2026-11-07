// Checks of ai_rest_a.cpp against the original, in the game thread.
//
//  - CBounceAI's class (checks_inventory_common.h): its getter, RegisterFields with the registered
//    descriptor, Create, CreateArray and the deleting destructor.
//  - The static collide checker's two slots directly (side 0 with the module's hooks out): slot 24's st(0)
//    and slot 1 on objects the exe's constructor levels built (flags 0: the bytes; flags 1: the pool gives
//    the block back).
//  - The slots in the fake world (checks_resta_world.h): CEntityBehaviorMoveToPos's destructor, Begin, aim
//    (unchanged targets, changed gaits, with and without path finding, both searches, the static checker
//    made or there), the maker, the gait sequences, the start of a move (walkers, projectiles, fliers),
//    the steering (every flag state, the counter, the side), Update (finished, turning, arriving, the
//    path's next point, lost, the walker's frames and motion); CBounceAI's Update.
//  - Real data: the CEntityBehaviorMoveToPos objects embedded in the running level's AIs (at most 30, a
//    spread), copied with their real points, speeds and flags, through the aim, the steering and Update.
#include "checks_resta_world.h"
#include "checks_inventory_common.h"
#include "ai_common.h"

namespace zax {

int hooks_ai_rest_a(Hook *out);

namespace {

using namespace ra;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CBounceAI", 0x14, 0x6bced0, 0x467180, 0x4671d0, 0x467360, {0x467300, 0}, true},
};

template <typename F> void two_sides(F f)
{
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        f(side);
        invcheck::side_end(side);
    }
}

// ---- The static checker's slots directly ---------------------------------------------------------------------

void test_checker(Tally &t)
{
    char d[120];
    for (unsigned k = 0; k < (unsigned)scaled(40); k++) {
        g_seed = 0x6100 + k;
        const DWORD a = random(), b = random(), c = random();
        DWORD one[2];
        two_sides([&](int side) {
            const float f = fstp(at<long double(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x430860)(nullptr, a, b, c));
            one[side] = float_bits(f);
        });
        const DWORD flags = k % 2;
        DWORD r[2], again[2];
        BYTE bytes[2][8];
        BYTE *o[2] = {};
        two_sides([&](int side) {
            BYTE *p = static_cast<BYTE *>(pool_alloc(8));
            o[side] = p;
            ai::obj_ctor(p);
            ai::vtable_and_defaults(p, 0x5eebe8);
            ai::dw(p, 0) = 0x5eec54;
            r[side] = (DWORD)(uintptr_t)at<BYTE *(THISCALL *)(void *, DWORD)>(0x430890)(p, flags);
            if (flags) {
                BYTE *n = static_cast<BYTE *>(pool_alloc(8));
                again[side] = n == p;
                pool_free(n, 8);
                memset(bytes[side], 0, 8);
            } else {
                again[side] = 1;
                memcpy(bytes[side], p, 8);
                pool_free(p, 8);
            }
        });
        wsprintfA(d, "[checker case %u: one %x / %x, eax %x / %x, back %u / %u]", k, one[0], one[1], r[0], r[1], again[0], again[1]);
        t.check(one[0] == one[1] && one[0] == 0x3f800000u && r[0] == ptrv(o[0]) && r[1] == ptrv(o[1]) && again[0] && again[1] &&
                    !memcmp(bytes[0], bytes[1], 8),
                "", d);
    }
}

// ---- The slots in the fake world -------------------------------------------------------------------------------

enum Self { S_MOVE, S_BOUNCE };
int g_self;
const BYTE *g_real_self;   // a real behaviour copied into the fake (real data) or null

DWORD point_bits(Gen &g) { return g.next() % 6 == 0 ? (g.next() % 2 ? 0 : 0x7fc00000u) : g.fbits_(-200.0f, 4200.0f); }
DWORD finite(Gen &g, float lo, float hi)
{
    const DWORD v = g.next();
    return v % 9 == 0 ? 0 : float_bits(lo + (hi - lo) * (float)(int)((v >> 8) % 100000) * 0.00001f);
}
DWORD move_flags(Gen &g)
{
    DWORD f = 0;
    const DWORD bits[] = {2, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200};
    for (DWORD b : bits)
        if (g.next() % 2)
            f |= b;
    if (g.next() % 8 == 0)
        f |= g.next() & 0xfffff000u;
    return f;
}

// A move behaviour in a fake object (the real one's bytes when given); its idle behaviour and checker are
// fakes, its sequence one of the static texts
void make_move(Gen &g, BYTE *s, const BYTE *real)
{
    if (real) {
        memcpy(s, real, 0xbc);
    } else {
        for (DWORD off = 0x80; off < 0x98; off += 4)
            fl<DWORD>(s, off) = point_bits(g);
        fl<DWORD>(s, 0x98) = g.next() % 3 == 0 ? 0 : g.fbits_(-60.0f, 60.0f);
        fl<DWORD>(s, 0x9c) = g.next() % 4;
        fl<DWORD>(s, 0xa0) = g.next() % 9;
        fl<DWORD>(s, 0xa4) = g.fbits_(0.0f, 2.0f);
        fl<DWORD>(s, 0xa8) = move_flags(g);
        fl<DWORD>(s, 0xac) = g.fbits_(-1.0f, 1.0f);
        fl<DWORD>(s, 0xb0) = g.fbits_(-1.0f, 1.0f);
        fl<DWORD>(s, 0x18) = g.fbits_(-1.0f, 7.5f);
    }
    fl<DWORD>(s, 0) = 0x5eeb68;
    fl<DWORD>(s, 0x1c) = ptrv(g_fvt[T_BEHAVIOR]);
    fl<DWORD>(s, 0xb4) = ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER)));
    const DWORD t = g.next() % 4;
    fl<DWORD>(s, 0xb8) = g_texts[t == 3 ? 0 : t];
}
void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_self == S_BOUNCE) {
        fl<DWORD>(s, 0) = 0x5ef0f8;
        fl<DWORD>(s, 0x10) = g.next() % 8 == 0 ? 0x7fc00000u : g.fbits_(0.0f, 1.0f);
        return;
    }
    make_move(g, s, g_real_self);
}

// Every entity gets finite rates (the update's frame loop subtracts the frame count until it is below it)
void finite_entities(Gen &g)
{
    for (int i = 0; i < N_ENTITY; i++) {
        BYTE *e = obj(O_ENTITY + i);
        fl<DWORD>(e, 0xb8) = g.next() % 7 == 0 ? 0x7fc00000u : finite(g, -2.0f, 2.0f);
        fl<DWORD>(e, 0x12c) = g.next() % 3 == 0 ? finite(g, 0.0f, 300.0f) : g.next() % 4;
    }
}
DWORD entity(Gen &g) { return ptrv(any_entity(g)); }
DWORD entity_or_null(Gen &g) { return ptrv(any_entity(g, 6)); }
// A point: two floats in the message object, sometimes the behaviour's own target
DWORD point(Gen &g, DWORD off)
{
    BYTE *m = obj(O_MSG) + off;
    BYTE *s = obj(O_SELF);
    if (g.next() % 3 == 0) {
        fl<DWORD>(m, 0) = fl<DWORD>(s, 0x90);
        fl<DWORD>(m, 4) = fl<DWORD>(s, 0x94);
    } else {
        fl<DWORD>(m, 0) = point_bits(g);
        fl<DWORD>(m, 4) = point_bits(g);
    }
    return ptrv(m);
}

void a_entity(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_entity_or_null(Gen &g, DWORD *a) { a[0] = entity_or_null(g); }
void a_aim(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = point(g, 0);
    a[2] = g.next() % 3 == 0 ? fl<DWORD>(obj(O_SELF), 0xa4) : g.fbits_(0.0f, 2.0f);
    const DWORD f[] = {0, 1, 0x100, 0xffffff00u, 2};
    a[3] = f[g.next() % 5];
    finite_entities(g);
}
void a_make(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = point(g, 0);
    a[2] = g.fbits_(0.0f, 2.0f);
    a[3] = g.next() % 3 == 0 ? 0x100 : g.next() % 2;
    a[4] = g.next() % 3 == 0 ? g.next() : g.next() % 2;
    a[5] = g.next();
    a[6] = g.next();
    // the behaviour Make gets from the pool: a block of its size filled and given back first (the pool
    // gives it out again)
    BYTE *b = static_cast<BYTE *>(pool_alloc(0xbc));
    make_move(g, b, nullptr);
    pool_free(b, 0xbc);
}
// Make's behaviour (eax): its bytes into the log, back to the pool after the side
void tag_made()
{
    if (g_last_eax)
        log_event(0xbe000000u, fnv(2166136261u, at<void *>(g_last_eax + 4), 0xb8), 0);
}
void free_made()
{
    if (g_last_eax)
        pool_free(at<void *>(g_last_eax), 0xbc);
}
// The destructor gets its own reference to a static text (the ZString destructor gives it back)
void a_dtor(Gen &, DWORD *)
{
    BYTE *s = obj(O_SELF);
    at<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(s + 0xb8, &g_texts[1]);
}
void a_steer(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = g.fbits_(-1.0f, 7.5f);
    BYTE *m = obj(O_MSG);
    fl<DWORD>(m, 0x10) = g.fbits_(-300.0f, 300.0f);
    fl<DWORD>(m, 0x14) = g.fbits_(-300.0f, 300.0f);
    a[2] = ptrv(m + 0x10);
    a[3] = ptrv(m + 0x20);
    a[4] = g.next() % 5 == 0 ? fl<DWORD>(obj(O_SELF), 0x98) : g.fbits_(-60.0f, 60.0f);
    a[5] = g.next() % 4 == 0 ? (g.next() % 2 ? 1 : 0x100) : 0;
    finite_entities(g);
}
void a_update(Gen &g, DWORD *a)
{
    a[0] = g.next() % 10 == 0 ? 0x7fc00000u : finite(g, 0.0f, 0.5f);
    a[1] = entity_or_null(g);
    finite_entities(g);
}
void a_bounce(Gen &g, DWORD *a)
{
    a[0] = entity_or_null(g);
    a[1] = g.fbits_(0.0f, 1.0f);
}

struct ClassSlot {
    int self;
    SlotTest t;
    DWORD always = 0;   // IsKindOf affirms the class of this getter
    void (*before)() = nullptr;
    void (*after)() = nullptr;
};
const ClassSlot SLOTS[] = {
    {S_MOVE, {"CEntityBehaviorMoveToPos::Dtor", 0x430350, 0, 0xffffffff, 60, a_dtor}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Begin", 0x430390, 1, 0xff, 200, a_entity}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Aim", 0x4303f0, 4, 0xff, 1500, a_aim}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Aim (movable)", 0x4303f0, 4, 0xff, 800, a_aim}, 0x4903f0},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Make", 0x4308c0, CALL_FASTCALL7, 0, 300, a_make}, 0, tag_made, free_made},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Resume", 0x430940, 1, 0, 400, a_entity_or_null}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::StartMoving", 0x430a40, 1, 0xff, 600, a_entity_or_null}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Steer", 0x430c70, 6, 0xff, 1500, a_steer}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Update", 0x430f20, 2, 0xff, 1500, a_update}},
    {S_MOVE, {"CEntityBehaviorMoveToPos::Update (movable)", 0x430f20, 2, 0xff, 1500, a_update}, 0x4903f0},
    {S_BOUNCE, {"CBounceAI::Update", 0x467400, 2, 0, 400, a_bounce}},
    {S_BOUNCE, {"CBounceAI::Update (movable)", 0x467400, 2, 0, 400, a_bounce}, 0x4903f0},
};

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self = s.self;
        g_before_state = s.before;
        g_after_side = s.after;
        Policy p{};
        if (s.always)
            p.always[0] = at<Getter>(s.always)();
        const unsigned n = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, p);
    }
    g_before_state = nullptr;
    g_after_side = nullptr;
}

// Real data: the behaviours embedded in the level's AIs (vtable 0x5eeb68 at a dword of the AI object)
int g_nreal;
void test_real(Tally &t)
{
    BYTE **parts = static_cast<BYTE **>(heap_alloc(8192 * 4));
    BYTE **ais = static_cast<BYTE **>(heap_alloc(64 * 4));
    const int nparts = level_parts(parts, 8192);
    const int tests[] = {2, 3, 7, 8, 9};   // aim (both), steering, Update (both)
    int count = 0;
    for (int pass = 0; pass < 2; pass++) {
        int seen = 0;
        for (int i = 0; i < nparts; i++) {
            const int na = part_ais(parts[i], parts, nparts, ais, 64);
            for (int k = 0; k < na; k++) {
                const CClassInfo *c = class_of(ais[k]);
                if (!c || c->size < 0xbc || c->size > 0x1000 || !readable(ais[k], c->size))
                    continue;
                for (DWORD off = 4; off + 0xbc <= c->size; off += 4) {
                    if (*reinterpret_cast<const DWORD *>(ais[k] + off) != 0x5eeb68)
                        continue;
                    const int index = seen++;
                    if (pass == 0 || !spread_pick(index, count, 30))
                        continue;
                    g_real_self = ais[k] + off;
                    g_self = S_MOVE;
                    for (int s : tests) {
                        Policy p{};
                        if (SLOTS[s].always)
                            p.always[0] = at<Getter>(SLOTS[s].always)();
                        for (unsigned seed = 1; seed <= 3; seed++)
                            run_slot(t, SLOTS[s].t, seed * 7919u + (unsigned)index * 131u + SLOTS[s].t.address, make_self, p);
                    }
                }
            }
        }
        if (pass == 0)
            count = seen;
    }
    g_real_self = nullptr;
    g_nreal = count;
    log("  ai_rest_a: %d parts, %d embedded move behaviours", nparts, count);
    heap_free(ais);
    heap_free(parts);
}

}  // namespace

ZAX_CHECKS(ai_rest_a)
{
    (void)cls;
    (void)n;
    Tally getters{"ai_rest_a: CBounceAI getter"}, objects{"ai_rest_a: CBounceAI Create, CreateArray, destructor; checker slots"},
        fields{"ai_rest_a: CBounceAI RegisterFields"}, real_fields{"ai_rest_a: CBounceAI registered descriptors (real data)"},
        slots{"ai_rest_a: move behaviour and bounce slots in the fake world"},
        real{"ai_rest_a: the level's embedded move behaviours through aim, steering, Update (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_ai_rest_a;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, 1);
    else
        objects.check(false, "ai_rest_a", "[no arena]");
    test_checker(objects);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        test_real(real);
        world_off();
    } else {
        slots.check(false, "ai_rest_a", "[the fake world could not be set up]");
    }
    slots.report();
    real.report();
    restore_bytes(saved_random);
}

}  // namespace zax
