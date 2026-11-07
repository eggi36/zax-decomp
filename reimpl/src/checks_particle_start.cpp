// Checks of particle_start.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters (both copies), Create/CreateArray and the
//    deleting destructors (the cylinder's through 0x52cc70 too), RegisterFields and the registered
//    descriptors; the abstract start point type's destructor 0x52b280 on point objects (same size).
//  - The slots in the fake world (checks_particle_world.h): the start point types built in the fake
//    block (their envelopes real objects of the exe's CLengthEnvelope), the start point (slot 25), the
//    envelopes' values, advance and reset, the old text lines; CParticleInfo's reset.
//  - Real data: the registered class infos; the start point types of the running level's particle
//    systems as copies through their slots.
#include "checks_particle_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_particle_start(Hook *out);

namespace {

using namespace pw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CParticleInfo", 0x24, 0x6d04d8, 0x52a8c0, 0x52a900, 0x52aa40, {0x52a860, 0x52a9e0}, true},
    {"CParticleStartPositionType", 8, 0x6d04dc, 0, 0, 0, {0x52aed0, 0x52af20}, false},
    {"CPointParticleStartPositionType", 8, 0x6d04e0, 0x52b2d0, 0x52b310, 0, {0x52b1d0, 0x52b410}, true},
    {"CDiskParticleStartPositionType", 0x58, 0x6d04e4, 0x52b600, 0x52b6a0, 0x52b980, {0x52b4d0, 0x52b8c0}, true},
    {"CLineParticleStartPositionType", 0x2c, 0x6d04e8, 0x52c140, 0x52c1b0, 0x52c400, {0x52c040, 0x52c340}, true},
    {"CCylinderParticleStartPositionType", 0x7c, 0x6d04ec, 0x52c8f0, 0x52c9c0, 0x52cda0, {0x52c770, 0x52cc90}, true},
    {"CSphereParticleStartPositionType", 0x58, 0x6d04f0, 0x52d6c0, 0x52d770, 0, {0x52d5c0, 0x52d9c0}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_INFO, C_TYPE, C_POINT, C_DISK, C_LINE, C_CYLINDER, C_SPHERE };
constexpr DWORD VTABLES[] = {0x5fc138, 0x5fc20c, 0x5fc198, 0x5fc280, 0x5fc2fc, 0x5fc374, 0x5fc3f4};

// ---- The classes ------------------------------------------------------------------------------------------

// 0x52b280 on a point object (8 bytes): flags 0 (the bytes compared, the block freed by the check) and 1
void test_abstract_dtor(Tally &t, DWORD flags)
{
    BYTE after[2][8];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        BYTE *o = static_cast<BYTE *>(at<invcheck::Create0>(0x52b2d0)());
        invcheck::side_begin(side);
        r[side] = invcheck::ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x52b280)(o, flags)) - invcheck::ptrv(o);
        invcheck::side_end(side);
        if (!(flags & 1)) {
            memcpy(after[side], o, 8);
            pool_free(o, 8);
        }
    }
    char d[120];
    wsprintfA(d, "[0x52b280 flags %u: eax %x / %x]", flags, r[0], r[1]);
    t.check(r[0] == r[1] && r[0] == 0 && ((flags & 1) || !memcmp(after[0], after[1], 8)), "CParticleStartPositionType", d);
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_particle_start;
    if (!invcheck::arena_setup()) {
        objects.check(false, "particle_start", "[no arena]");
        return;
    }
    invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    for (int k = 0; k < scaled(6); k++)
        test_abstract_dtor(objects, (DWORD)k & 1);
}

// ---- The slots ---------------------------------------------------------------------------------------------

int g_self_class;
const BYTE *g_real_self;

DWORD small_float(Gen &g, float lo, float hi) { return g.next() % 10 ? g.fbits_(lo, hi) : g.next() % 2 ? 0 : g.next(); }
void make_envelope(Gen &g, BYTE *e)
{
    fl<DWORD>(e, 0) = 0x5f3f2c;
    fl<DWORD>(e, 4) = 0;
    fl<DWORD>(e, 8) = small_float(g, 0.0f, 80.0f);
    fl<DWORD>(e, 0xc) = small_float(g, 0.0f, 80.0f);
    fl<DWORD>(e, 0x10) = small_float(g, 0.0f, 80.0f);
    fl<DWORD>(e, 0x14) = g.next() % 3 ? 0 : small_float(g, -5.0f, 5.0f);
    fl<DWORD>(e, 0x18) = g.next() % 2 ? 0x3f800000u : 0xbf800000u;
    fl<BYTE>(e, 0x1c) = (BYTE)(g.next() % 2);
    fl<DWORD>(e, 0x20) = g.next() % 4;
}
void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    if (g_real_self) {
        memcpy(s, g_real_self, CLASSES[c].size);
    } else {
        make_envelope(g, s + 8);
        make_envelope(g, s + 0x2c);
        make_envelope(g, s + 0x58);
        fl<BYTE>(s, 0x50) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x54) = small_float(g, 0.05f, 2.0f);
        if (c == C_INFO)
            for (DWORD o = 8; o < 0x24; o += 4)
                fl<DWORD>(s, o) = g.next();
    }
    fl<DWORD>(s, 0) = VTABLES[c];
    fl<DWORD>(s, 4) = 0;
}

void a_point(Gen &g, DWORD *a)
{
    BYTE *dir = obj(O_OUT) + 0x40;
    const DWORD k = g.next() % 4;
    for (DWORD o = 0; o < 12; o += 4)
        fl<DWORD>(dir, o) = k == 0 ? 0 : small_float(g, -1.0f, 1.0f);
    if (k == 1)
        fl<DWORD>(dir, 4) = 0;   // y 0, z not
    a[0] = ptrv(obj(O_OUT));
    a[1] = ptrv(dir);
}
void a_seconds(Gen &g, DWORD *a) { a[0] = g.next() % 8 ? g.fbits_(0.0f, 0.3f) : g.next() % 2 ? 0 : 0x7fc00000u; }
void a_none(Gen &, DWORD *) {}
void a_any(Gen &g, DWORD *a) { a[0] = g.next(); }
const char *const LINE_NAMES[] = {"Inner Radius", "OUTER RADIUS", "Length From Center", "Cylinder Height", "YX Ratio", "Radius"};
const char *const LINE_VALUES[] = {"12.5", "-3", "x", "", "4e2"};
void a_read_line(Gen &g, DWORD *a)
{
    a[0] = ptrv(LINE_NAMES[g.next() % 6]);
    a[1] = ptrv(LINE_VALUES[g.next() % 5]);
    a[2] = g.next() % 2 ? ptrv(obj(O_OUT)) : 0;
}

struct ClassSlot {
    int cls;
    SlotTest t;
};
constexpr int F = SLOT_FLOAT;
const ClassSlot SLOTS[] = {
    {C_INFO, {"CParticleInfo::Reset", 0x52a8b0, 0, 0xffffffff, 20, a_none}},
    {C_POINT, {"CPointParticleStartPositionType::StartPoint", 0x52b2b0, 2, 0, 20, a_point}},
    {C_DISK, {"CDiskParticleStartPositionType::StartPoint", 0x52bed0, 2, 0, 400, a_point}},
    {C_LINE, {"CLineParticleStartPositionType::StartPoint", 0x52c720, 2, 0, 200, a_point}},
    {C_CYLINDER, {"CCylinderParticleStartPositionType::StartPoint", 0x52cf70, 2, 0, 300, a_point}},
    {C_SPHERE, {"CSphereParticleStartPositionType::StartPoint", 0x52dad0, 2, 0, 400, a_point}},
    {C_LINE, {"CParticleStartPositionType::Value8", 0x52c120, F | 0, 0xffffffff, 60, a_none}},
    {C_DISK, {"CDiskParticleStartPositionType::Value2c", 0x52b5e0, F | 0, 0xffffffff, 60, a_none}},
    {C_CYLINDER, {"CCylinderParticleStartPositionType::Value58", 0x52c8d0, F | 0, 0xffffffff, 60, a_none}},
    {C_DISK, {"CDiskParticleStartPositionType::Advance", 0x52b580, 1, 0, 60, a_seconds}},
    {C_LINE, {"CLineParticleStartPositionType::Advance", 0x52c0f0, 1, 0, 60, a_seconds}},
    {C_CYLINDER, {"CCylinderParticleStartPositionType::Advance", 0x52c870, 1, 0, 60, a_seconds}},
    {C_DISK, {"CDiskParticleStartPositionType::ResetEnvelopes", 0x52b5b0, 1, 0, 30, a_any}},
    {C_LINE, {"CLineParticleStartPositionType::ResetEnvelopes", 0x52c110, 1, 0, 30, a_any}},
    {C_CYLINDER, {"CCylinderParticleStartPositionType::ResetEnvelopes", 0x52c8a0, 1, 0, 30, a_any}},
    {C_DISK, {"CDiskParticleStartPositionType::ReadLine", 0x52bce0, 3, 0, 60, a_read_line}},
    {C_LINE, {"CLineParticleStartPositionType::ReadLine", 0x52c4e0, 3, 0, 60, a_read_line}},
    {C_CYLINDER, {"CCylinderParticleStartPositionType::ReadLine", 0x52ced0, 3, 0, 60, a_read_line}},
};

// The random angles of the start points come from the fakes: finite (the table functions 0x508da0/0x508ec0
// read outside their table for huge angles)
Policy start_policy()
{
    Policy p{};
    p.finite = true;
    return p;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self_class = s.cls;
        const unsigned n = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, start_policy());
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------

BYTE *g_parts[8192];
BYTE g_copy[0x80];

void test_real(Tally &registered, Tally &real_slots)
{
    for (const invcheck::ClassDef &d : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(d.var);
        char text[160];
        wsprintfA(text, "[%s: class info %x, size %x]", d.name, ptrv(ci), ci ? ci->size : 0);
        registered.check(ci && ci->size == d.size && at<Getter>(d.getters[0])() == ci && (!d.create || ptrv((void *)ci->create) == d.create),
                         d.name, text);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  particle_start real data: no level grid found");
        return;
    }
    const CClassInfo *generic = *at<CClassInfo **>(0x6d04fc);
    int systems = 0, types = 0, ran = 0;
    for (int i = 0; i < n; i++) {
        BYTE *ais[64];
        const int count = part_ais(parts[i], parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            if (!generic || class_of(ais[k]) != generic || !readable(ais[k], 0x148) || systems >= 40)
                continue;
            systems++;
            const BYTE *start = at<const BYTE *>(fl<DWORD>(ais[k], 0x110));
            const CClassInfo *sc = class_of(start);
            int c = -1;
            for (int j = 0; j < NCLASSES; j++)
                if (sc && sc == *at<CClassInfo **>(CLASSES[j].var))
                    c = j;
            if (c < C_POINT || types >= 24 || !readable(start, CLASSES[c].size))
                continue;
            types++;
            memcpy(g_copy, start, CLASSES[c].size);
            g_real_self = g_copy;
            g_self_class = c;
            for (const ClassSlot &s : SLOTS) {
                if (s.cls != c && !(s.cls == C_LINE && s.t.address == 0x52c120 && c != C_POINT) &&
                    !(s.cls == C_DISK && c >= C_CYLINDER && (s.t.address == 0x52bed0 || s.t.address == 0x52b5e0)))
                    continue;
                if (s.t.address == 0x52bce0 || s.t.address == 0x52c4e0 || s.t.address == 0x52ced0)
                    continue;
                for (unsigned seed = 1; seed <= 3; seed++) {
                    run_slot(real_slots, s.t, seed * 40503u + ptrv(start) + s.t.address, make_self, start_policy());
                    ran++;
                }
            }
            g_real_self = nullptr;
        }
    }
    log("  particle_start real data: %d parts, %d particle systems, %d start point types; %d cases", n, systems, types, ran);
}

}  // namespace

ZAX_CHECKS(particle_start)
{
    (void)cls;
    (void)n;
    Tally getters{"particle_start: class getters"}, objects{"particle_start: Create, CreateArray, destructors"},
        fields{"particle_start: RegisterFields"}, real_fields{"particle_start: RegisterFields against the registered fields (real data)"},
        slots{"particle_start: slots in the fake world"}, registered{"particle_start: registered class infos (real data)"},
        real_slots{"particle_start: the level's start point types through their slots (real data)"};
    SavedBytes saved_random, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    if (world_on()) {
        test_slots(slots);
        test_real(registered, real_slots);
        world_off();
    } else {
        slots.check(false, "particle_start", "[the fake world could not be set up]");
    }
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random);
}

}  // namespace zax
