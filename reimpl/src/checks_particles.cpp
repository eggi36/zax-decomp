// Checks of particles.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters (both copies), Create/CreateArray with the
//    deleting destructors, RegisterFields and the registered descriptors; the abstract classes'
//    destructors 0x528920 (on CFloatParticleAI objects, same size) and 0x52dea0/0x52ded0 (on
//    CGenericParticleSystemAI objects, the block freed by the check).
//  - The slots in the fake world (checks_particle_world.h): the particle AIs (with the entities, their
//    parents, sequences, models and velocity fakes) and the particle system (its envelopes real objects
//    of the exe's envelope classes inside the object, the particle infos and a start point type in the
//    fake block, the start point type real or a fake; the made particles and the cloned AIs are fakes).
//  - Real data: the registered class infos; the running level's particle systems as copies (their start
//    point type and particle infos copied into the fake block) and the particle AIs of their infos and
//    of the level's parts as copies, through their slots.
#include "checks_particle_world.h"
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_particles(Hook *out);

namespace {

using namespace pw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CParticleAI", 0x24, 0x6d0174, 0, 0, 0x528600, {0x528550, 0x5285a0}, false},
    {"CGravityParticleAI", 0x34, 0x6d0178, 0x528c10, 0x528c70, 0x528e90, {0x528b60, 0x528dd0}, true},
    {"CFloatParticleAI", 0x24, 0x6d017c, 0x529740, 0x5297a0, 0, {0x529690, 0x529900}, true},
    {"CSwirlParticleAI", 0x34, 0x6d0180, 0x529a90, 0x529af0, 0x529d10, {0x5299c0, 0x529c50}, true},
    {"CParticleSystemAI", 0x134, 0x6d04f4, 0, 0, 0, {0x52dc20, 0x52dc90}, false},
    {"CGenericParticleSystemAI", 0x148, 0x6d04fc, 0x52f040, 0x52f070, 0x52f220, {0x52ef30, 0x52f160}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_PARTICLE, C_GRAVITY, C_FLOAT, C_SWIRL, C_SYSTEM, C_GENERIC, C_VECTOR };
constexpr DWORD VTABLES[] = {0x5fbd58, 0x5fbe10, 0x5fbed8, 0x5fbf90, 0x5fc470, 0x5fc554};

// ---- The classes ------------------------------------------------------------------------------------------

// An abstract class's destructor on an object of a class of the same (or the given) size: the bytes
// after flags 0 and eax compared; the block then freed by the check (flags 1 only when the sizes match)
void test_abstract_dtor(Tally &t, const char *name, DWORD dtor, DWORD create, DWORD size, DWORD flags)
{
    BYTE after[2][0x148];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        BYTE *o = static_cast<BYTE *>(at<invcheck::Create0>(create)());
        invcheck::side_begin(side);
        r[side] = invcheck::ptrv(at<void *(THISCALL *)(void *, DWORD)>(dtor)(o, flags)) - invcheck::ptrv(o);
        invcheck::side_end(side);
        if (!(flags & 1)) {
            memcpy(after[side], o, size);
            pool_free(o, size);
        }
    }
    char d[120];
    wsprintfA(d, "[0x%x flags %u: eax %x / %x]", dtor, flags, r[0], r[1]);
    t.check(r[0] == r[1] && r[0] == 0 && ((flags & 1) || !memcmp(after[0], after[1], size)), name, d);
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_particles;
    if (!invcheck::arena_setup()) {
        objects.check(false, "particles", "[no arena]");
        return;
    }
    invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    for (int k = 0; k < scaled(6); k++) {
        test_abstract_dtor(objects, "CParticleAI", 0x528920, 0x529740, 0x24, (DWORD)k & 1);
        test_abstract_dtor(objects, "CParticleSystemAI", 0x52dea0, 0x52f040, 0x148, 0);
    }
}

// ---- The slots ---------------------------------------------------------------------------------------------

int g_self_class;
const BYTE *g_real_self;            // a copy of a real object (the self of a real-data case)
const BYTE *g_real_start;           // a real particle system's start point type
const BYTE *const *g_real_infos;    // and its particle infos
DWORD g_real_ninfos;
int g_start_kind;                   // the start point type of the particle system cases: 0 none, 1 fake, 2.. real classes
int g_start_policy;                 // -1: by the generator; else g_start_kind fixed

DWORD seconds(Gen &g)
{
    const DWORD v = g.next() % 16;
    if (v == 0)
        return 0;
    if (v == 1)
        return 0x7fc00000u;
    if (v == 2)
        return 0xbf800000u;
    return g.fbits_(0.0f, 0.3f);
}
DWORD small_float(Gen &g, float lo, float hi) { return g.next() % 10 ? g.fbits_(lo, hi) : g.next() % 2 ? 0 : g.next(); }
// A finite value in [lo, hi] (now and then 0): angles and what becomes one. The exe's table functions
// 0x508da0/0x508ec0 read outside their table for huge angles; the game never gives them such.
DWORD finite_float(Gen &g, float lo, float hi)
{
    if (g.next() % 10 == 0)
        return 0;
    const float x = lo + (hi - lo) * (float)(g.next() >> 8) * (1.0f / 16777216.0f);
    DWORD b;
    memcpy(&b, &x, 4);
    return b;
}
DWORD finite_seconds(Gen &g)
{
    const DWORD v = g.next() % 16;
    return v == 0 ? 0 : v == 1 ? 0xbf800000u : finite_float(g, 0.0f, 0.3f);
}
// An envelope inside an object: the exe's class (its vtable), start, current, end, speed, direction,
// "Using Start", "Ending Behavior"
void make_envelope(Gen &g, BYTE *e, DWORD vtable, float lo, float hi, bool angle = false)
{
    DWORD (*const value)(Gen &, float, float) = angle ? finite_float : small_float;
    fl<DWORD>(e, 0) = vtable;
    fl<DWORD>(e, 4) = 0;
    fl<DWORD>(e, 8) = value(g, lo, hi);
    fl<DWORD>(e, 0xc) = value(g, lo, hi);
    fl<DWORD>(e, 0x10) = value(g, lo, hi);
    fl<DWORD>(e, 0x14) = g.next() % 3 ? 0 : value(g, -2.0f, 2.0f);
    fl<DWORD>(e, 0x18) = g.next() % 2 ? 0x3f800000u : 0xbf800000u;
    fl<BYTE>(e, 0x1c) = (BYTE)(g.next() % 2);
    fl<DWORD>(e, 0x20) = g.next() % 4;
}
// A start point type in O_START: the class's vtable and envelopes (kind 2 point, 3 disk, 4 line, 5
// cylinder, 6 sphere)
DWORD make_start(Gen &g, int kind)
{
    BYTE *s = obj(O_START);
    const DWORD vt[] = {0x5fc198, 0x5fc280, 0x5fc2fc, 0x5fc374, 0x5fc3f4};
    fl<DWORD>(s, 0) = vt[kind - 2];
    fl<DWORD>(s, 4) = 0;
    make_envelope(g, s + 8, 0x5f3f2c, 0.0f, 60.0f);
    make_envelope(g, s + 0x2c, 0x5f3f2c, 0.0f, 60.0f);
    make_envelope(g, s + 0x58, 0x5f3f2c, 0.0f, 60.0f);
    fl<BYTE>(s, 0x50) = (BYTE)(g.next() % 2);
    fl<DWORD>(s, 0x54) = small_float(g, 0.05f, 2.0f);
    return ptrv(s);
}
// The particle infos: pointers in O_INFOS, the objects in O_INFO_OBJS (0x24 bytes each)
void make_infos(Gen &g, BYTE *self)
{
    const DWORD n = g.next() % 4;
    BYTE *list = obj(O_INFOS);
    for (DWORD i = 0; i < n; i++) {
        BYTE *info = obj(O_INFO_OBJS) + 0x24 * i;
        fl<DWORD>(list, 4 * i) = ptrv(info);
        fl<DWORD>(info, 0) = 0x5fc138;
        fl<DWORD>(info, 8) = g.next() % 5 ? g.next() % 40 : g.next();
        fl<DWORD>(info, 0x10) = g.next() % 3 ? ptrv(obj(O_AI + (int)(g.next() % N_AI))) : 0;
        fl<BYTE>(info, 0x14) = (BYTE)(g.next() % 2);
        fl<BYTE>(info, 0x15) = (BYTE)(g.next() % 2);
        fl<DWORD>(info, 0x18) = g.next() % 4;
        fl<DWORD>(info, 0x1c) = g.next() % 3;
        fl<BYTE>(info, 0x20) = (BYTE)(g.next() % 2);
        fl<BYTE>(info, 0x21) = (BYTE)(g.next() % 2);
    }
    fl<DWORD>(self, 0x114) = 0x5fc534;
    fl<DWORD>(self, 0x118) = n || g.next() % 2 ? ptrv(list) : 0;
    fl<DWORD>(self, 0x11c) = n;
    fl<DWORD>(self, 0x120) = n;
    const DWORD sounds = g.next() % 4;
    for (DWORD i = 0; i < sounds; i++)
        fl<DWORD>(obj(O_SOUNDS), 4 * i) = g.next() % 50;
    fl<DWORD>(self, 0x124) = 0x5ee138;
    fl<DWORD>(self, 0x128) = ptrv(obj(O_SOUNDS));
    fl<DWORD>(self, 0x12c) = sounds;
    fl<DWORD>(self, 0x130) = sounds;
}

void make_particle_ai(Gen &g, BYTE *s, int c)
{
    // the entities' age stamps finite and the sequences' frame counts small (now and then 0)
    for (int i = 0; i < N_ENTITY; i++)
        fl<float>(obj(O_ENTITY + i), 0x10c) = (float)(g.next() % 4000) * 0.001f;
    for (int i = 0; i < N_OTHER; i++)
        fl<DWORD>(obj(O_OTHER + i), 0x48) = g.next() % 8 ? 1 + g.next() % 60 : 0;
    fl<DWORD>(s, 0) = VTABLES[c];
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 8) = g.next();
    fl<DWORD>(s, 0xc) = 0;
    if (!g_real_self) {
        for (DWORD o = 0x10; o < 0x1c; o += 4)
            fl<DWORD>(s, o) = small_float(g, -500.0f, 2000.0f);
        fl<BYTE>(s, 0x1c) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x1d) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x1e) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x20) = small_float(g, 0.1f, 2.0f);
        switch (c) {
        case C_GRAVITY:
            fl<DWORD>(s, 0x24) = small_float(g, -20.0f, 20.0f);
            fl<DWORD>(s, 0x28) = g.next() % 2 ? 0 : g.next() % 60;
            fl<BYTE>(s, 0x2c) = (BYTE)(g.next() % 2);
            fl<BYTE>(s, 0x2d) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x30) = small_float(g, -50.0f, 50.0f);
            break;
        case C_SWIRL:
            fl<DWORD>(s, 0x24) = finite_float(g, -6.3f, 6.3f);
            fl<DWORD>(s, 0x28) = finite_float(g, -6.3f, 6.3f);
            fl<BYTE>(s, 0x2c) = (BYTE)(g.next() % 2);
            fl<DWORD>(s, 0x30) = finite_float(g, -6.3f, 6.3f);
            break;
        default:
            break;
        }
    }
}

void make_system(Gen &g, BYTE *s)
{
    fl<DWORD>(s, 0) = VTABLES[C_GENERIC];
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 0xc) = 0;
    if (!g_real_self) {
        make_envelope(g, s + 0x10, 0x5f3d8c, 0.0f, 180.0f, true);
        make_envelope(g, s + 0x34, 0x5f3d8c, 0.0f, 180.0f, true);
        make_envelope(g, s + 0x58, 0x5f3ec4, -360.0f, 360.0f, true);
        make_envelope(g, s + 0x7c, 0x5f3e5c, -90.0f, 90.0f, true);
        make_envelope(g, s + 0xa8, 0x5f3df4, 0.0f, 400.0f);
        make_envelope(g, s + 0xd0, 0x5f3d24, 0.0f, 4.0f);
        fl<DWORD>(s, 0xa0) = small_float(g, 0.0f, 20.0f);
        fl<DWORD>(s, 0xa4) = small_float(g, 0.0f, 5.0f);
        fl<DWORD>(s, 0xcc) = small_float(g, 0.0f, 100.0f);
        fl<DWORD>(s, 0xf4) = small_float(g, 0.0f, 2.0f);
        fl<DWORD>(s, 0xf8) = g.next() % 3 ? 0x497423f0u : small_float(g, 0.0f, 500.0f);
        fl<DWORD>(s, 0xfc) = g.next() % 3 ? 0 : small_float(g, 0.0f, 300.0f);
        fl<DWORD>(s, 0x100) = small_float(g, 0.0f, 3.0f);
        fl<DWORD>(s, 0x104) = g.next() % 4 ? small_float(g, 0.0f, 20.0f) : 0;
        fl<BYTE>(s, 0x108) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x109) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x10a) = (BYTE)(g.next() % 6 != 0);
        fl<BYTE>(s, 0x10b) = (BYTE)(g.next() % 2);
        fl<DWORD>(s, 0x10c) = g.next() % 6;
        fl<DWORD>(s, 0x134) = g.next() % 200 - 100;
        fl<DWORD>(s, 0x138) = g.next() % 200 - 100;
        fl<DWORD>(s, 0x13c) = g.next() % 100;
        fl<DWORD>(s, 0x140) = g.next() % 3 ? 0 : g.next() % 8;
        fl<BYTE>(s, 0x144) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x145) = (BYTE)(g.next() % 2);
        fl<BYTE>(s, 0x146) = (BYTE)(g.next() % 2);
        make_infos(g, s);
        const int kind = g_start_policy >= 0 ? g_start_policy : (int)(g.next() % 7);
        g_start_kind = kind;
        fl<DWORD>(s, 0x110) = kind == 0 ? 0 : kind == 1 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : make_start(g, kind);
    } else {
        // the copy's start point type and particle infos in the fake block (never the real ones)
        fl<DWORD>(s, 0x110) = 0;
        if (g_real_start) {
            const CClassInfo *ci = class_of(g_real_start);
            memcpy(obj(O_START), g_real_start, ci && ci->size <= OBJ ? ci->size : 8);
            fl<DWORD>(s, 0x110) = ptrv(obj(O_START));
        }
        for (DWORD i = 0; i < g_real_ninfos; i++) {
            memcpy(obj(O_INFO_OBJS) + 0x24 * i, g_real_infos[i], 0x24);
            fl<DWORD>(obj(O_INFOS), 4 * i) = ptrv(obj(O_INFO_OBJS) + 0x24 * i);
        }
        fl<DWORD>(s, 0x118) = ptrv(obj(O_INFOS));
        fl<DWORD>(s, 0x11c) = g_real_ninfos;
        fl<BYTE>(s, 0x10a) = (BYTE)(g.next() % 4 != 0);
        fl<DWORD>(s, 0x100) = small_float(g, 0.0f, 3.0f);
        if (fl<DWORD>(s, 0x12c) > 64)
            fl<DWORD>(s, 0x12c) = 0;
    }
}

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_real_self)
        memcpy(s, g_real_self, g_self_class == C_GENERIC ? 0x148 : CLASSES[g_self_class].size);
    switch (g_self_class) {
    case C_GENERIC:
    case C_SYSTEM:
        make_system(g, s);
        if (g_self_class == C_SYSTEM)
            fl<DWORD>(s, 0) = VTABLES[C_SYSTEM];
        break;
    case C_VECTOR:
        for (DWORD o = 0; o < 0x20; o += 4)
            fl<DWORD>(s, o) = small_float(g, -1000.0f, 1000.0f);
        break;
    default:
        make_particle_ai(g, s, g_self_class);
        break;
    }
}

DWORD entity(Gen &g) { return ptrv(any_entity(g)); }
// An entity (or 0 when the slot reads it only with "Animated" or the parent switch off)
void a_entity(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    if (!fl<BYTE>(obj(O_SELF), 0x1c) && g.next() % 6 == 0)
        a[0] = 0;
}
void a_entity_seconds(Gen &g, DWORD *a)
{
    a_entity(g, a);
    a[1] = seconds(g);
}
void a_bounce(Gen &g, DWORD *a)
{
    a[0] = seconds(g);
    a[1] = entity(g);
    a[2] = small_float(g, -40.0f, 0.0f);
    a[3] = g.next() % 4 ? 0 : small_float(g, -10.0f, 10.0f);
}
void a_touch(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next();
}
void a_vector(Gen &g, DWORD *a)
{
    BYTE *v = obj(O_OUT);
    for (DWORD o = 0; o < 16; o += 4)
        fl<DWORD>(v, o) = small_float(g, -1000.0f, 1000.0f);
    a[0] = ptrv(v);
}
void a_flags(Gen &g, DWORD *a)
{
    a[0] = g.next() % 3 ? g.next() & 1 : g.next();
    a[1] = g.next() % 3 ? g.next() & 1 : g.next();
    a[2] = g.next() % 3 ? g.next() & 1 : g.next();
}
void a_value(Gen &g, DWORD *a) { a[0] = small_float(g, -10.0f, 10.0f); }
void a_none(Gen &, DWORD *) {}
void a_part_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = finite_seconds(g);
}
void a_entity_finite_seconds(Gen &g, DWORD *a)
{
    a_entity(g, a);
    a[1] = finite_seconds(g);
}
void a_part(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_message(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    BYTE *m = obj(O_OUT);
    const DWORD ids[] = {0x6d0210, 0x6d04d4, 0x6c29e8};
    fl<DWORD>(m, 0) = 0x5eeb64;
    const DWORD k = g.next() % 4;
    fl<DWORD>(m, 4) = k < 3 ? *at<DWORD *>(ids[k]) : g.next();
    a[1] = ptrv(m);
}
void a_origin(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(obj(O_OUT));
    a[2] = finite_float(g, -360.0f, 360.0f);
    a[3] = g.next() % 3 ? g.next() & 1 : g.next();
}
void a_direction(Gen &g, DWORD *a)
{
    a[0] = finite_float(g, -3.2f, 3.2f);
    a[1] = ptrv(obj(O_OUT));
}
void a_turn(Gen &g, DWORD *a)
{
    BYTE *in = obj(O_OUT) + 0x40;
    for (DWORD o = 0; o < 12; o += 4)
        fl<DWORD>(in, o) = small_float(g, -2.0f, 2.0f);
    a[0] = entity(g);
    a[1] = ptrv(in);
    a[2] = ptrv(obj(O_OUT));
    a[3] = finite_float(g, -360.0f, 360.0f);
    a[4] = finite_float(g, -90.0f, 90.0f);
}
const char *const LINE_NAMES[] = {"Cone Angle", "cone angle spread", "Facing Angle", "Trajectory", "Particle Speed", "Particle Age",
                                  "Particle Speed Variance", "PARTICLE AGE VARIANCE", "Burst Frequency", "Cone"};
const char *const LINE_VALUES[] = {"12.5", "-3", "abc", "", "1e3", "0.000001"};
void a_read_line(Gen &g, DWORD *a)
{
    a[0] = ptrv(LINE_NAMES[g.next() % 10]);
    a[1] = ptrv(LINE_VALUES[g.next() % 6]);
    a[2] = g.next() % 2 ? ptrv(obj(O_OUT)) : 0;
}
void a_reset_start(Gen &g, DWORD *)
{
    // slot 19 deletes the start point type through its slot 1: a fake or none
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0x110) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
}

struct ClassSlot {
    int cls;
    SlotTest t;
    bool emits = false;    // the system's update: the made particles and AIs are always there
    bool angles = false;   // the fakes' float answers finite (they become angles of the table functions)
};
const ClassSlot SLOTS[] = {
    {C_PARTICLE, {"CParticleAI::MayTouch", 0x528960, 3, 0xff, 60, a_touch}},
    {C_PARTICLE, {"CParticleAI::SetCenter", 0x5289a0, 1, 0xffffffff, 20, a_vector}},
    {C_PARTICLE, {"CParticleAI::SetFlags", 0x5289c0, 3, 0xff, 20, a_flags}},
    {C_PARTICLE, {"CParticleAI::SetRatio", 0x5289e0, 1, 0xffffffff, 20, a_value}},
    {C_PARTICLE, {"CParticleAI::Update", 0x5289f0, 2, 0, 200, a_entity_seconds}},
    {C_FLOAT, {"CFloatParticleAI::Update", 0x5289f0, 2, 0, 100, a_entity_seconds}},
    {C_PARTICLE, {"CParticleAI::Frame", 0x528a40, 2, 0xff, 300, a_entity_seconds}},
    {C_GRAVITY, {"CGravityParticleAI::Begin", 0x5292c0, 1, 0, 100, a_entity}},
    {C_GRAVITY, {"CGravityParticleAI::Frame", 0x529320, 2, 0xff, 300, a_entity_seconds}},
    {C_GRAVITY, {"CGravityParticleAI::Update", 0x5293d0, 2, 0, 100, a_entity_seconds}},
    {C_GRAVITY, {"CGravityParticleAI::Bounce", 0x529420, 4, 0, 400, a_bounce}},
    {C_GRAVITY, {"CGravityParticleAI::Collide", 0x529600, 2, 0, 400, a_entity_seconds}},
    {C_SWIRL, {"CSwirlParticleAI::Reset", 0x529a70, 0, 0xffffffff, 20, a_none}},
    {C_SWIRL, {"CSwirlParticleAI::Begin", 0x52a070, 1, 0, 100, a_entity}},
    {C_SWIRL, {"CSwirlParticleAI::Update", 0x52a0d0, 2, 0, 400, a_entity_finite_seconds}, false, true},
    {C_VECTOR, {"Vec3_Sub", 0x52a200, 1, 0, 60, a_vector}},
    {C_VECTOR, {"Vec3_Scale", 0x535400, 1, 0, 60, a_value}},
    {C_VECTOR, {"Vec3_Add", 0x535420, 1, 0, 60, a_vector}},
    {C_VECTOR, {"PtrArray_Ctor_5ee15c", 0x535440, 0, 0xffffffff, 10, a_none}},
    {C_SYSTEM, {"CParticleSystemAI::Reset", 0x52e050, 0, 0xffffffff, 20, a_none}},
    {C_GENERIC, {"CParticleSystemAI::Origin", 0x52e080, 4, 0, 300, a_origin}, false, true},
    {C_GENERIC, {"CParticleSystemAI::Direction", 0x52eb40, 2, 0, 100, a_direction}, false, true},
    {C_GENERIC, {"CParticleSystemAI::Turn", 0x52ebd0, 5, 0, 200, a_turn}, false, true},
    {C_GENERIC, {"CGenericParticleSystemAI::XOffset", 0x52efe0, 0, 0xffffffff, 10, a_none}},
    {C_GENERIC, {"CGenericParticleSystemAI::Height", 0x52eff0, 0, 0xffffffff, 10, a_none}},
    {C_GENERIC, {"CGenericParticleSystemAI::StopAfter", 0x52f000, 0, 0xffffffff, 10, a_none}},
    {C_GENERIC, {"CGenericParticleSystemAI::AsShot", 0x52f010, 0, 0xff, 10, a_none}},
    {C_GENERIC, {"CGenericParticleSystemAI::Orient", 0x52f020, 0, 0xff, 10, a_none}},
    {C_GENERIC, {"CGenericParticleSystemAI::SoundByBurst", 0x52f030, 0, 0xff, 10, a_none}},
    {C_GENERIC, {"CGenericParticleSystemAI::Reset", 0x530800, 0, 0xffffffff, 40, a_reset_start}},
    {C_GENERIC, {"CGenericParticleSystemAI::ReadLine", 0x530690, 3, 0, 100, a_read_line}},
    {C_GENERIC, {"CGenericParticleSystemAI::Begin", 0x52e220, 1, 0, 150, a_part}},
    {C_GENERIC, {"CGenericParticleSystemAI::Message", 0x52eef0, 2, 0xff, 100, a_message}},
    {C_GENERIC, {"CGenericParticleSystemAI::Update", 0x52e2d0, 2, 0, 700, a_part_seconds}, true, true},
};

Policy particle_policy(bool emits, bool angles)
{
    Policy p{};
    p.finite = angles;
    if (emits) {
        // the made particles and the cloned AIs are used without a test
        p.nonnull = 1u << T_ENTITY | 1u << T_AI;
        p.always[0] = at<Getter>(0x491930)();   // CEntityProjectile
        p.always[1] = at<Getter>(0x528550)();   // CParticleAI
    }
    return p;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self_class = s.cls;
        g_start_policy = -1;
        const unsigned n = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, particle_policy(s.emits, s.angles));
    }
    // Fixed cases: seeds of real-data cases that once differed (their seed follows the heap address of the
    // level's AI). Run 20261029-195055-51bc: Bounce's second Entity_SetRenderHeightFloat got a NaN
    // payload in the original and the default NaN in the reimplementation (operation forms). The self is a
    // zeroed copy like a real one (make_self then draws the same values; Bounce only writes +0x2d).
    static const BYTE zero_self[0x34] = {};
    for (const ClassSlot &s : SLOTS)
        if (s.t.address == 0x529420) {
            g_self_class = s.cls;
            g_start_policy = -1;
            g_real_self = zero_self;
            run_slot(t, s.t, 457861943u, make_self, particle_policy(s.emits, s.angles));
            g_real_self = nullptr;
        }
}

// ---- Real data ---------------------------------------------------------------------------------------------

BYTE *g_parts[8192];
BYTE g_real_copy[0x148];

void run_real(Tally &t, int cls, unsigned base, int &ran)
{
    g_self_class = cls;
    g_start_policy = -1;
    for (const ClassSlot &s : SLOTS) {
        const bool fits = s.cls == cls || (cls != C_GENERIC && s.cls == C_PARTICLE) ||
                          (cls == C_GENERIC && s.cls == C_SYSTEM && s.t.address == 0x52e050);
        if (!fits || s.t.address == 0x530800 || s.t.address == 0x52a200 || s.t.address == 0x535400 || s.t.address == 0x535420)
            continue;
        for (unsigned seed = 1; seed <= 2; seed++) {
            run_slot(t, s.t, seed * 40503u + base + s.t.address, make_self, particle_policy(s.emits, s.angles));
            ran++;
        }
    }
}

int class_index(const CClassInfo *ci)
{
    for (int c = 0; c < NCLASSES; c++)
        if (ci && ci == *at<CClassInfo **>(CLASSES[c].var))
            return c;
    return -1;
}

void test_real(Tally &registered, Tally &real_slots)
{
    for (int c = 0; c < NCLASSES; c++) {
        const invcheck::ClassDef &d = CLASSES[c];
        const CClassInfo *ci = *at<CClassInfo **>(d.var);
        char text[160];
        wsprintfA(text, "[%s: class info %x, size %x]", d.name, ptrv(ci), ci ? ci->size : 0);
        registered.check(ci && ci->size == d.size && at<Getter>(d.getters[0])() == ci && (!d.create || ptrv((void *)ci->create) == d.create),
                         d.name, text);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  particles real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, templates = 0, ran = 0;
    for (int i = 0; i < n; i++) {
        BYTE *ais[64];
        const int count = part_ais(parts[i], parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const int c = class_index(class_of(ais[k]));
            if (c < 0 || !readable(ais[k], CLASSES[c].size) || found[c]++ >= 12)
                continue;
            memcpy(g_real_copy, ais[k], CLASSES[c].size);
            g_real_self = g_real_copy;
            g_real_start = nullptr;
            g_real_infos = nullptr;
            g_real_ninfos = 0;
            if (c == C_GENERIC) {
                const BYTE *start = at<const BYTE *>(fl<DWORD>(g_real_copy, 0x110));
                const CClassInfo *sc = class_of(start);
                g_real_start = sc && sc->size <= OBJ ? start : nullptr;
                const DWORD ninfos = fl<DWORD>(g_real_copy, 0x11c);
                const BYTE *const *infos = at<const BYTE *const *>(fl<DWORD>(g_real_copy, 0x118));
                if (ninfos > 14 || (ninfos && !readable(infos, ninfos * 4)))
                    continue;
                bool ok = true;
                for (DWORD j = 0; j < ninfos && ok; j++)
                    ok = readable(infos[j], 0x24);
                if (!ok)
                    continue;
                g_real_infos = infos;
                g_real_ninfos = ninfos;
                run_real(real_slots, C_GENERIC, ptrv(ais[k]), ran);
                // the particle AIs of its infos (templates the system clones)
                for (DWORD j = 0; j < ninfos; j++) {
                    const BYTE *ai = at<const BYTE *>(fl<DWORD>(const_cast<BYTE *>(infos[j]), 0x10));
                    const int pc = class_index(class_of(ai));
                    if (pc < 0 || pc == C_GENERIC || pc == C_SYSTEM || templates >= 24)
                        continue;
                    templates++;
                    memcpy(g_real_copy, ai, CLASSES[pc].size);
                    g_real_infos = nullptr;
                    g_real_ninfos = 0;
                    g_real_start = nullptr;
                    run_real(real_slots, pc, ptrv(ai), ran);
                }
            } else {
                run_real(real_slots, c, ptrv(ais[k]), ran);
            }
        }
    }
    g_real_self = nullptr;
    g_real_start = nullptr;
    g_real_infos = nullptr;
    g_real_ninfos = 0;
    log("  particles real data: %d parts, %d CGenericParticleSystemAI, %d CGravityParticleAI, %d CFloatParticleAI, %d CSwirlParticleAI, "
        "%d templates; %d cases",
        n, found[C_GENERIC], found[C_GRAVITY], found[C_FLOAT], found[C_SWIRL], templates, ran);
}

}  // namespace

ZAX_CHECKS(particles)
{
    (void)cls;
    (void)n;
    Tally getters{"particles: class getters"}, objects{"particles: Create, CreateArray, destructors"}, fields{"particles: RegisterFields"},
        real_fields{"particles: RegisterFields against the registered fields (real data)"}, slots{"particles: slots in the fake world"},
        registered{"particles: registered class infos (real data)"}, real_slots{"particles: the level's particle AIs through their slots (real data)"};
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
        slots.check(false, "particles", "[the fake world could not be set up]");
    }
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random);
}

}  // namespace zax
