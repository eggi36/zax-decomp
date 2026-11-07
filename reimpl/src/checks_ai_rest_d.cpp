// Checks of ai_rest_d.cpp against the original, in the game thread, in the fake world (checks_restd_world.h):
//
//  - CSameAsParentAI's Update: both switches, upper bodies with and without an animated parent, sequences the
//    body does not have, parents ahead of and behind the body (frames and their special values), walkers.
//  - CScanAreaAI's Begin (first time, "Reset Each Begin", animated or not) and Update (waiting and turning,
//    the end of either, with and without an end turn action and "Execute Each Turn", sequence wrap, turning
//    speeds 0 and NaN, seconds 0, NaN and big).
//  - Real data: copies of the running level's CSameAsParentAI and CScanAreaAI and the classes derived from them
//    (at most 12 of each, a spread) through their slots (the scan AIs' actions replaced by fakes).
#include "checks_restd_world.h"

namespace zax {

namespace {

using namespace rdw;
using Getter = CClassInfo *(__cdecl *)();

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
    if (v == 2)
        return g.fbits_(10.0f, 500.0f);
    return g.fbits_(0.0f, 2.0f);
}
DWORD entity(Gen &g) { return ptrv(obj(O_ENTITY + (int)(g.next() % N_ENTITY))); }

const BYTE *g_real_self;   // a copy of a level's AI (real data) or null
DWORD g_real_size;

void make_same_as_parent(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_real_self) {
        memcpy(s, g_real_self, g_real_size);
    } else {
        fl<DWORD>(s, 0) = 0x5fd080;
        fl<BYTE>(s, 0x10) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x11) = (BYTE)(g.next() % 3 == 0);
    }
    // the entity's parent: an animated entity or not, now and then none
    for (int i = 0; i < N_ENTITY; i++)
        fl<DWORD>(obj(O_ENTITY + i), 0xc) = g.next() % 6 == 0 ? 0 : entity(g);
}
void make_scan(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_real_self) {
        memcpy(s, g_real_self, g_real_size);
        if (fl<DWORD>(s, 0x28))
            fl<DWORD>(s, 0x28) = ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION)));
        if (g.next() % 3 == 0)   // now and then the end of a wait or a turn
            fl<DWORD>(s, g.next() % 2 ? 0x30 : 0x44) = g.fbits_(0.0f, 0.1f);
        return;
    }
    fl<DWORD>(s, 0) = 0x5ff1e0;
    fl<DWORD>(s, 0x10) = static_text(0);
    fl<DWORD>(s, 0x14) = time_value(g, -180.0f, 180.0f);
    fl<DWORD>(s, 0x18) = time_value(g, -180.0f, 180.0f);
    if (g.next() % 6 == 0)
        fl<DWORD>(s, 0x18) = fl<DWORD>(s, 0x14);
    fl<DWORD>(s, 0x1c) = time_value(g, 0.0f, 90.0f);
    fl<DWORD>(s, 0x20) = time_value(g, 0.0f, 5.0f);
    for (DWORD o = 0x24; o <= 0x2e; o++)
        fl<BYTE>(s, o) = (BYTE)(g.next() % 2);
    fl<DWORD>(s, 0x28) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
    fl<DWORD>(s, 0x30) = time_value(g, 0.0f, 3.0f);
    fl<DWORD>(s, 0x34) = time_value(g, 0.0f, 6.0f);
    fl<DWORD>(s, 0x38) = time_value(g, -180.0f, 180.0f);
    fl<DWORD>(s, 0x3c) = g.next() % 3 == 0 ? fl<DWORD>(s, 0x18) : time_value(g, -180.0f, 180.0f);
    fl<BYTE>(s, 0x40) = (BYTE)(g.next() % 2);
    fl<DWORD>(s, 0x44) = time_value(g, 0.0f, 3.0f);
    fl<DWORD>(s, 0x48) = g.next() % 3 == 0 ? 0xc479c000u : time_value(g, 0.0f, 6.3f);
}
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = seconds(g);
}
void a_entity(Gen &g, DWORD *a) { a[0] = entity(g); }

struct Test {
    SlotTest t;
    SelfMaker make;
    DWORD size, var;
};
const Test TESTS[] = {
    {{"CSameAsParentAI::Update", 0x562010, 2, 0, 800, a_entity_seconds}, make_same_as_parent, 0x14, 0x713178},
    {{"CScanAreaAI::Begin", 0x565c10, 1, 0, 600, a_entity}, make_scan, 0x4c, 0x713214},
    {{"CScanAreaAI::Update", 0x565ce0, 2, 0, 1200, a_entity_seconds}, make_scan, 0x4c, 0x713214},
};

void test_slots(Tally &t)
{
    for (const Test &x : TESTS) {
        Policy p{};
        const unsigned n = (unsigned)scaled(x.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, x.t, seed * 2654435761u + x.t.address, x.make, p);
    }
}

// Real data: copies of the level's AIs of the two classes through their slots
void test_real(Tally &t)
{
    BYTE **parts = static_cast<BYTE **>(heap_alloc(8192 * 4));
    BYTE **ais = static_cast<BYTE **>(heap_alloc(64 * 4));
    const int nparts = level_parts(parts, 8192);
    int counts[2] = {0, 0};
    for (int pass = 0; pass < 2; pass++) {
        int seen[2] = {0, 0};
        for (int i = 0; i < nparts; i++) {
            const int na = part_ais(parts[i], parts, nparts, ais, 64);
            for (int k = 0; k < na; k++)
                for (int c = 0; c < 2; c++) {
                    const Test &x = TESTS[c == 0 ? 0 : 1];
                    // the class or one derived from it (CPlayerWalkingWeaponAI, CWalkingUpperBodyAI, ...)
                    const CClassInfo *ci = class_of(ais[k]);
                    if (!derived_from(ci, *at<CClassInfo **>(x.var)) || ci->size > OBJ)
                        continue;
                    const int index = seen[c]++;
                    if (pass == 0 || !spread_pick(index, counts[c], 12))
                        continue;
                    g_real_self = ais[k];
                    g_real_size = ci->size;
                    for (int s = c == 0 ? 0 : 1; s <= (c == 0 ? 0 : 2); s++)
                        for (unsigned seed = 1; seed <= 4; seed++) {
                            Policy p{};
                            run_slot(t, TESTS[s].t, seed * 7919u + (unsigned)index * 131u + TESTS[s].t.address, TESTS[s].make, p);
                        }
                }
        }
        if (pass == 0)
            for (int c = 0; c < 2; c++)
                counts[c] = seen[c];
    }
    g_real_self = nullptr;
    log("  ai_rest_d: %d parts; real AIs: %d same as parent, %d scan area", nparts, counts[0], counts[1]);
    // free 0x5d33e0, not operator delete 0x5d0320 (a stand-in while the world is on)
    at<void(__cdecl *)(void *)>(0x5d33e0)(ais);
    at<void(__cdecl *)(void *)>(0x5d33e0)(parts);
}

}  // namespace

ZAX_CHECKS(ai_rest_d)
{
    (void)cls;
    (void)n;
    Tally slots{"ai_rest_d: CSameAsParentAI and CScanAreaAI slots in the fake world"},
        real{"ai_rest_d: the level's same as parent and scan area AIs as copies (real data)"};
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        test_real(real);
        world_off();
    } else {
        slots.check(false, "ai_rest_d", "[the fake world could not be set up]");
    }
    slots.report();
    real.report();
}

}  // namespace zax
