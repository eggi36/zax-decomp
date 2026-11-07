// Checks of the custom rules and the application's game flow helpers (zax_rules.cpp) against the
// original, in the game thread (machinery: checks_inventory_common.h; the three list loaders of the
// range are checked with the other loaders in checks_world_rest.cpp).
//  - CZaxCustomRules: Create, CreateArray with 0-3 elements, the deleting destructor, both class
//    getters, RegisterFields into a fake class info and with the class variable 0, the registered
//    descriptors (real data); CCustomRules' deleting destructor 0x5a13a0 on a base-level object.
//  - The ten option getters on rules objects made by the original Create (the option made, its slot 19,
//    compared byte for byte, strings by text; a second call; a field set before), and on the game's
//    real custom rules object (real data: the existing options returned).
//  - The application's helpers on a fake application (vtable recorders for slots 16, 84) with the
//    functions of other subsystems recorded (the map caching table, sequences, the HUD, the readers, the
//    game type list's records and the item catalogue swapped for records of our own, the mission timer);
//    the net layer's slot 44 with the global 0x713f18 swapped; the getters also on the real application.
//  - The array capacities on arrays of our own (pool blocks and an operator new block in the arena).
//  - Real data: has-a-camera-tracker (0x5a2bf0) on the level's parts.
#include "checks_inventory_common.h"
#include "layer_parts.h"
#include "ptr_array.h"
#include "x87.h"

namespace zax {

int hooks_zax_rules(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CZaxCustomRules", 0x30, 0x713f24, 0x5a13d0, 0x5a1410, 0x5a1580, {0x5a1350, 0x5a1520}, true},
};

using Create0 = void *(__cdecl *)();
DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
void *vp(DWORD v) { return reinterpret_cast<void *>(static_cast<uintptr_t>(v)); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
DWORD text_of(const char *t) { return t ? fnv_text(2166136261u, t) : 0x0bad; }

// 0x5a13a0 on a CCustomRules made here (CObject's constructor, vtable 0x603084, InitFields)
void test_base_dtor(Tally &t, unsigned seed)
{
    BYTE obj[2][8];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *o = static_cast<BYTE *>(pool_alloc(8));
        D(o, 4) = random();
        at<void *(THISCALL *)(void *)>(0x416610)(o);
        D(o, 0) = 0x603084;
        at<void(THISCALL *)(void *)>(0x4173c0)(o);
        const DWORD flags = seed & 1;
        r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x5a13a0)(o, flags) == ptrv(o);
        memcpy(obj[side], o, 8);
        if (!flags)
            pool_free(o, 8);
        arena_off();
        side_end(side);
    }
    char d[100];
    wsprintfA(d, "[seed %u: eax is this %d / %d]", seed, r[0], r[1]);
    t.check(r[0] == r[1] && r[0] && ((seed & 1) || same_region(obj[0], obj[1], 8)) && same_region(g_arena[0], g_arena[1], g_used[0]),
            "CCustomRules::DeletingDtor", d);
}

// ---- The option getters ----------------------------------------------------------------------------------

struct Option {
    const char *name;
    DWORD fn, offset, size;
};
const Option OPTIONS[] = {
    {"MaterialsGeneratorDisplay", 0x5a1cd0, 8, 0xf4},    {"InventoryDisplay", 0x5a1d40, 0xc, 0x2a0},
    {"DialogDisplay", 0x5a1e80, 0x10, 0x78},             {"MainMenuDisplay", 0x5a1ee0, 0x14, 0x34},
    {"PlayerWalkingControl", 0x5a1f40, 0x18, 0x40},      {"HudOptions", 0x5a1fa0, 0x1c, 0x60},
    {"MissionOverOptions", 0x5a2010, 0x20, 0xec},        {"MultiPlayerMissionOverOptions", 0x5a20d0, 0x24, 0xec},
    {"LoadSaveScreenOptions", 0x5a2190, 0x28, 0xc},      {"SphereOfZinOptions", 0x5a21f0, 0x2c, 0x10},
};
BYTE g_option[2][0x2a0];

void test_option(Tally &t, const Option &o, unsigned seed)
{
    DWORD r[2], again[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        hooks_out();
        BYTE *rules = static_cast<BYTE *>(at<Create0>(0x5a13d0)());
        hooks_in();
        // a pool block of the option's size filled with a pattern first (the constructors leave bytes)
        BYTE *p = static_cast<BYTE *>(pool_alloc(o.size));
        memset(p, (BYTE)random(), o.size);
        pool_free(p, o.size);
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *made = at<BYTE *(THISCALL *)(void *)>(o.fn)(rules);
        r[side] = made == vp(D(rules, o.offset)) && made;
        again[side] = at<BYTE *(THISCALL *)(void *)>(o.fn)(rules) == made;
        memcpy(g_option[side], made, o.size);
        arena_off();
        side_end(side);
        hooks_out();
        arena_on(side, 0, false);   // the bigger options are arena blocks: their frees ignored
        vcall<void *>(rules, 1, (DWORD)1);   // the option deleted with it (an owned field)
        arena_off();
        hooks_in();
    }
    char d[140];
    const bool same_bytes = same_region(g_option[0], g_option[1], o.size);
    wsprintfA(d, "[seed %u: made %d / %d, again %d / %d, first difference +%x: %x / %x]", seed, r[0], r[1], again[0], again[1], g_diff_at,
              g_diff_a, g_diff_b);
    t.check(r[0] && r[1] && again[0] && again[1] && same_bytes && g_used[0] == g_used[1], o.name, d);
}
// An option field set before: returned as it is (no call of its slots)
void test_option_set(Tally &t, const Option &o, unsigned seed)
{
    DWORD r[2];
    alignas(4) BYTE rules[0x30];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (DWORD k = 0; k < sizeof rules; k += 4)
            D(rules, k) = random() | 1;
        side_begin(side);
        r[side] = ptrv(at<void *(THISCALL *)(void *)>(o.fn)(rules));
        side_end(side);
    }
    char d[80];
    wsprintfA(d, "[set: %x / %x]", r[0], r[1]);
    t.check(r[0] == r[1] && r[0] == D(rules, o.offset), o.name, d);
}
// The game's real custom rules object (the application's +0x2e0, its +0x38): every option exists and is
// returned
void test_real_options(Tally &t)
{
    BYTE *app = *at<BYTE **>(0x713f14);
    BYTE *holder = app && readable(app + 0x2e0, 4) ? vp(D(app, 0x2e0)) ? static_cast<BYTE *>(vp(D(app, 0x2e0))) : nullptr : nullptr;
    BYTE *rules = holder && readable(holder + 0x38, 4) ? static_cast<BYTE *>(vp(D(holder, 0x38))) : nullptr;
    if (!rules || class_of(rules) != *at<CClassInfo **>(0x713f24)) {
        log("  zax rules: no CZaxCustomRules found (application +0x2e0, +0x38)");
        return;
    }
    for (const Option &o : OPTIONS) {
        if (!D(rules, o.offset))
            continue;   // made on first use: not here, it would stay
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side] = ptrv(at<void *(THISCALL *)(void *)>(o.fn)(rules));
            side_end(side);
        }
        char d[80];
        wsprintfA(d, "[real: %x / %x]", r[0], r[1]);
        t.check(r[0] == r[1] && r[0] == D(rules, o.offset), o.name, d);
    }
}

// ---- The application's helpers on a fake application -------------------------------------------------------

constexpr int NREC = 6;
struct Record {
    BYTE name[0x78];
    void *object;
    BYTE keep, in_use, unknown7e, loading;
};
struct FakeObject {
    DWORD vftable;
    BYTE body[0x2fc];
    DWORD answer[4];
};
struct Fake {
    BYTE app[0x600];
    BYTE data[0x40];          // the game data (+0x2c the sequence)
    FakeObject objs[6];       // sequences, game types, parts, the timer
    Record types[NREC];
    Record catalogue[NREC];
    DWORD answers[16];
    DWORD counter;            // the fast forward counter 0x6c1d0c
    DWORD net;                // the net layer global 0x713f18
};
Fake g_fake;
Fake g_after[2];
void *g_vtable[100];
void *g_app_vtable[100];

int obj_id(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p), *f = reinterpret_cast<const BYTE *>(&g_fake);
    if (b >= f && b < f + sizeof g_fake)
        return 0x1000 + (int)(b - f);
    if (in_arena(ptrv(p), g_side))
        return 0x2000;
    return p ? 0x99 : 0;
}
DWORD ans(int k) { return g_fake.answers[k]; }

DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_delete(FakeObject *o, DWORD flags) { ev(0x101, (DWORD)obj_id(o), flags); return result(0x01); }
DWORD THISCALL f_start(FakeObject *o) { ev(0x117, (DWORD)obj_id(o)); return result(0x17); }
DWORD THISCALL f_find_ai(FakeObject *o, CClassInfo *c, DWORD a) { ev(0x140, (DWORD)obj_id(o), ptrv(c), a); return o->answer[0] & 1 ? 0x5e7f0000u : 0; }
DWORD THISCALL a_warning(void *, const char *text) { ev(0x210, text_of(text)); return result(0x10); }
DWORD THISCALL a_playing(void *) { ev(0x254); return result(ans(1) & 1); }

DWORD THISCALL s_flush(void *t, DWORD name) { ev(0x701, (DWORD)obj_id(t), name); return result(0x01); }
DWORD THISCALL s_expired(void *t, DWORD name) { ev(0x702, (DWORD)obj_id(t), name); return result(ans(2) & 1); }
DWORD THISCALL s_expire(void *t, DWORD name) { ev(0x703, (DWORD)obj_id(t), name); return result(0x03); }
void *THISCALL s_clone(void *o, DWORD a, DWORD b) { ev(0x704, (DWORD)obj_id(o), a, b); return &g_fake.objs[1]; }
DWORD THISCALL s_seq_start(void *s, DWORD a) { ev(0x705, (DWORD)obj_id(s), a); return result(0x05); }
DWORD THISCALL s_seq_event(void *s, DWORD a) { ev(0x706, (DWORD)obj_id(s), a); return result(0x06); }
DWORD THISCALL s_readers(void *w, void *s, void *a) { ev(0x707, (DWORD)obj_id(w), (DWORD)obj_id(s), (DWORD)obj_id(a)); return result(0x07); }
DWORD THISCALL s_mp_readers(void *s) { ev(0x708, (DWORD)obj_id(s)); return result(0x08); }
DWORD THISCALL s_mp_readers2(void *s) { ev(0x709, (DWORD)obj_id(s)); return result(0x09); }
DWORD THISCALL s_find(void *list, const char *name, DWORD k) { ev(0x70a, ptrv(list), text_of(name), k); return ans(3) % 4 == 3 ? 0xffffffff : ans(3) % (NREC + 2); }
void *__cdecl s_hud() { ev(0x70b); return &g_fake.objs[3]; }
DWORD THISCALL s_hud_item(void *h, DWORD p, DWORD def, DWORD flag) { ev(0x70c, (DWORD)obj_id(h), p, def ^ (flag << 16)); return result(0x0c); }
void *__cdecl s_timer() { ev(0x70d); return &g_fake.objs[4]; }
void *THISCALL s_net_ctor(void *o) { ev(0x70e, (DWORD)obj_id(o)); D(o, 0) = 0x5f91d4; D(o, 4) = 0x5e7e0004; return o; }
DWORD THISCALL s_net_dtor(void *o) { ev(0x70f, (DWORD)obj_id(o)); return result(0x0f); }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x589790, (void *)&s_flush},     {0x5899b0, (void *)&s_expired},   {0x589570, (void *)&s_expire},   {0x416760, (void *)&s_clone},
    {0x521e90, (void *)&s_seq_start}, {0x522110, (void *)&s_seq_event}, {0x4f6a90, (void *)&s_readers},  {0x5bd8b0, (void *)&s_mp_readers},
    {0x5abd90, (void *)&s_mp_readers2}, {0x591fc0, (void *)&s_find},    {0x5bd7b0, (void *)&s_hud},      {0x5bd810, (void *)&s_hud_item},
    {0x429de0, (void *)&s_timer},     {0x4f7df0, (void *)&s_net_ctor},  {0x4f7e30, (void *)&s_net_dtor},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

constexpr DWORD SWAPS[] = {0x6c0318, 0x6c031c, 0x6c0c38, 0x6c0c3c, 0x6c1d0c, 0x713f18};
constexpr int NSWAPS = sizeof SWAPS / sizeof SWAPS[0];
DWORD g_saved[NSWAPS];

void init_tables()
{
    for (int i = 0; i < 100; i++) {
        g_vtable[i] = (void *)&f_bad;
        g_app_vtable[i] = (void *)&f_bad;
    }
    g_vtable[1] = (void *)&f_delete;
    g_vtable[23] = (void *)&f_start;
    g_vtable[64] = (void *)&f_find_ai;
    g_app_vtable[16] = (void *)&a_warning;
    g_app_vtable[84] = (void *)&a_playing;
}

void setup(unsigned seed)
{
    g_seed = seed;
    Fake &f = g_fake;
    memset(&f, 0, sizeof f);
    for (DWORD k = 0; k < sizeof f.app; k += 4)
        D(f.app, k) = random();
    D(f.app, 0) = ptrv(g_app_vtable);
    D(f.app, 0x34c) = ptrv(f.data);
    D(f.app, 0x33c) = ptrv(&f.objs[5]);
    f.app[0x589] = (BYTE)rnd(2);
    D(f.app, 0x58c) = rnd(4) ? rnd(8) : random();
    D(f.app, 0x594) = float_bits((float)(int)rnd(600) * 0.5f);
    for (DWORD k = 0; k < sizeof f.data; k += 4)
        D(f.data, k) = random();
    D(f.data, 0x2c) = rnd(2) ? ptrv(&f.objs[0]) : 0;
    for (FakeObject &o : f.objs) {
        o.vftable = ptrv(g_vtable);
        for (DWORD k = 0; k < sizeof o.body; k += 4)
            *reinterpret_cast<DWORD *>(&o.body[k]) = random();
        for (DWORD &a : o.answer)
            a = random();
    }
    *reinterpret_cast<float *>(&f.objs[4].body[0x270 - 4]) = (float)(int)rnd(900) * 0.25f;   // the timer's +0x270
    for (int i = 0; i < NREC; i++) {
        f.types[i].object = &f.objs[1 + i % 3];
        f.types[i].name[0] = (BYTE)('a' + i);
        f.catalogue[i].object = &f.objs[2 + i % 3];
    }
    for (DWORD &a : f.answers)
        a = random();
    f.counter = rnd(4);
    f.net = rnd(2) ? 0 : ptrv(&f.objs[2]);
}
void fake_in()
{
    Fake &f = g_fake;
    for (int i = 0; i < NSTANDS; i++)
        patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
    const DWORD values[NSWAPS] = {ptrv(f.types), NREC, ptrv(f.catalogue), NREC, f.counter, f.net};
    for (int i = 0; i < NSWAPS; i++) {
        g_saved[i] = *at<DWORD *>(SWAPS[i]);
        *at<DWORD *>(SWAPS[i]) = values[i];
    }
}
void fake_out()
{
    Fake &f = g_fake;
    f.counter = *at<DWORD *>(0x6c1d0c);
    f.net = *at<DWORD *>(0x713f18);
    for (int i = NSWAPS - 1; i >= 0; i--)
        *at<DWORD *>(SWAPS[i]) = g_saved[i];
    for (int i = NSTANDS - 1; i >= 0; i--)
        unpatch(g_stand_patches[i]);
}

struct AppCase {
    const char *name;
    DWORD fn;
    int args;      // stack arguments
    bool eax, al;  // compare eax (al only)
};
const AppCase APP_CASES[] = {
    {"CZaxWorld::GameTypeIndex", 0x5a01a0, 0, true, false},     {"CZaxWorld::GameTypeBits", 0x5a01b0, 0, true, false},
    {"CZaxWorld::EndGame", 0x5a0850, 0, false, false},          {"CZaxWorld::FlushLevel", 0x5a0860, 1, true, false},
    {"CZaxWorld::ExpireLevel", 0x5a0880, 1, true, false},       {"CZaxWorld::SetStatusText", 0x5a0a00, 3, false, false},
    {"CZaxWorld::StatusText", 0x5a0c60, 1, false, false},       {"CZaxWorld::StartSequence", 0x5a2b10, 2, true, false},
    {"CZaxWorld::SequenceEvent", 0x5a2b60, 1, true, false},     {"CZaxWorld::EndSequence", 0x5a2b90, 1, true, false},
    {"CZaxWorld::HasCameraTracker", 0x5a2bf0, 1, true, false},  {"CZaxWorld::RegisterReaders", 0x5a2c80, 2, true, false},
    {"CZaxWorld::SetGameTypeByName", 0x5a3180, 1, true, false}, {"CZaxWorld::SetGameType", 0x5a32b0, 1, true, false},
    {"CZaxWorld::HudItem", 0x5a3480, 3, true, false},           {"CZaxWorld::SetHudItem", 0x5a34b0, 2, false, false},
    {"CZaxWorld::SetMissionTime", 0x5a3600, 1, false, false},   {"CZaxWorld::NetLayer", 0x5a33e0, -1, true, false},
    {"CZaxNetLocal::DeletingDtor", 0x5a3420, -2, true, false},
};
constexpr int NAPP = sizeof APP_CASES / sizeof APP_CASES[0];
const char *const TEXTS[] = {"", "Loading...", "Mission complete", "Quest", "Deathmatch"};

DWORD g_text_hash[2];
DWORD run_app_case(const AppCase &c, int side)
{
    Fake &f = g_fake;
    BYTE *app = f.app;
    g_text_hash[side] = 0;
    DWORD a[3];
    for (DWORD &x : a)
        x = random();
    switch (c.fn) {
    case 0x5a0a00: case 0x5a0c60: {
        // the status text: a real ZString at +0x364 (and an argument string)
        ZString *status = reinterpret_cast<ZString *>(app + 0x364), arg;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(status, TEXTS[rnd(5)]);
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&arg, TEXTS[rnd(5)]);
        DWORD r;
        if (c.fn == 0x5a0a00)
            r = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(c.fn)(app, &arg, float_bits((float)rnd(5) * 0.5f), random());
        else
            r = at<DWORD(THISCALL *)(void *, void *)>(c.fn)(app, &arg);
        g_text_hash[side] = text_of(status->c_str()) ^ (text_of(arg.c_str()) << 1);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&arg);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(status);
        D(app, 0x364) = 0;
        return r;
    }
    case 0x5a3180: return at<DWORD(THISCALL *)(void *, const char *)>(c.fn)(app, TEXTS[rnd(5)]);
    case 0x5a32b0: {   // eax stays as it was for -1 (not compared then)
        const DWORD i = rnd(4) == 3 ? 0xffffffff : rnd(NREC + 2);
        const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(c.fn)(app, i);
        return i == 0xffffffff ? 0 : r;
    }
    case 0x5a34b0: at<void(THISCALL *)(void *, DWORD, DWORD)>(c.fn)(app, rnd(NREC + 2), a[1]); return 0;
    case 0x5a3600: at<void(THISCALL *)(void *, DWORD)>(c.fn)(app, rnd(4)); return 0;
    case 0x5a2b10: return at<DWORD(THISCALL *)(void *, void *, DWORD)>(c.fn)(app, &f.objs[2], a[0]);
    case 0x5a2bf0: return at<DWORD(THISCALL *)(void *, void *)>(c.fn)(app, &f.objs[rnd(6)]);
    case 0x5a2c80: return at<DWORD(THISCALL *)(void *, void *, void *)>(c.fn)(app, &f.objs[1], &f.objs[2]);
    case 0x5a33e0: {
        const DWORD r = rel(ptrv(at<void *(__cdecl *)()>(c.fn)()), side);
        // a net layer made in the arena: its bytes, then the global back to none (the block stays there)
        if (in_arena(*at<DWORD *>(0x713f18), side)) {
            g_text_hash[side] = fnv(2166136261u, vp(*at<DWORD *>(0x713f18)), 8);
            *at<DWORD *>(0x713f18) = 0x5e7e0000;
        }
        return r;
    }
    case 0x5a3420: {
        BYTE *o = static_cast<BYTE *>(heap_alloc(8));   // in the arena (free ignored)
        D(o, 0) = 0x602fa8;
        D(o, 4) = random();
        return rel(at<DWORD(THISCALL *)(void *, DWORD)>(c.fn)(o, rnd(2)), side);
    }
    }
    switch (c.args) {
    case 0: return at<DWORD(THISCALL *)(void *)>(c.fn)(app);
    case 1: return at<DWORD(THISCALL *)(void *, DWORD)>(c.fn)(app, a[0]);
    case 2: return at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(c.fn)(app, a[0], a[1]);
    default: return at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(c.fn)(app, a[0], a[1], a[2]);
    }
}

void test_app_case(Tally &t, const AppCase &c, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup(seed);
        log_reset(side);
        side_begin(side);
        fake_in();
        arena_on(side, 0x3a);
        g_seed = seed * 7u + 3u;
        r[side] = run_app_case(c, side);
        arena_off();
        fake_out();
        side_end(side);
        memcpy(&g_after[side], &g_fake, sizeof g_fake);
    }
    if (!c.eax)
        r[0] = r[1] = 0;
    if (c.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    const bool same = r[0] == r[1] && same_log() && g_text_hash[0] == g_text_hash[1] && g_used[0] == g_used[1] &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(Fake)) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[180];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, text %x / %x, first difference +%x: %x / %x]", seed, r[0], r[1], g_nlog[0] / 4,
              g_nlog[1] / 4, g_text_hash[0], g_text_hash[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, c.name, d);
}

// The getters on the real application (read only)
void test_real_app(Tally &t)
{
    BYTE *app = *at<BYTE **>(0x713f14);
    if (!app)
        return;
    const DWORD fns[2] = {0x5a01a0, 0x5a01b0};
    for (DWORD f : fns) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side] = at<DWORD(THISCALL *)(void *)>(f)(app);
            side_end(side);
        }
        char d[60];
        wsprintfA(d, "[real: %x / %x]", r[0], r[1]);
        t.check(r[0] == r[1], "CZaxWorld (real)", d);
    }
    DWORD h[2];
    for (int side = 0; side < 2; side++) {
        ZString s;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&s, "x");
        side_begin(side);
        at<DWORD(THISCALL *)(void *, void *)>(0x5a0c60)(app, &s);
        side_end(side);
        h[side] = text_of(s.c_str());
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&s);
    }
    t.check(h[0] == h[1], "CZaxWorld::StatusText (real)");
}

// ---- Array capacities ------------------------------------------------------------------------------------

void test_reserve(Tally &t, DWORD fn, unsigned seed)
{
    const DWORD size = fn == 0x5a4080 ? 4 : fn == 0x5a41b0 ? 8 : 4;
    DWORD state[2][4], content[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        alignas(4) BYTE arr[0x10];
        const DWORD cap = rnd(6), count = cap ? rnd(cap + 1) : 0, n = rnd(10);
        D(arr, 0) = 0x5f5290;
        D(arr, 8) = count;
        D(arr, 0xc) = cap;
        D(arr, 4) = 0;
        side_begin(side);
        arena_on(side, 0x77);
        if (fn == 0x5a40f0) {
            if (cap) {
                DWORD *block = static_cast<DWORD *>(heap_alloc(cap * 4 + 4));
                block[0] = cap;
                for (DWORD i = 0; i < cap; i++)
                    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&block[1 + i], TEXTS[(i + seed) % 5]);
                D(arr, 4) = ptrv(&block[1]);
            }
        } else if (cap) {
            BYTE *p = static_cast<BYTE *>(pool_get_alloc(cap * size));
            for (DWORD k = 0; k < cap * size; k++)
                p[k] = (BYTE)random();
            D(arr, 4) = ptrv(p);
        }
        const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(fn)(arr, n);
        state[side][0] = D(arr, 8);
        state[side][1] = D(arr, 0xc);
        state[side][2] = n > cap ? rel(r, side) == rel(D(arr, 4), side) || fn != 0x5a40f0 : 1;
        state[side][3] = D(arr, 4) != 0;
        content[side] = 2166136261u;
        const DWORD m = D(arr, 0xc);
        if (fn == 0x5a40f0) {
            for (DWORD i = 0; i < count && D(arr, 4); i++)
                content[side] = fnv_text(content[side], reinterpret_cast<ZString *>(vp(D(arr, 4) + i * 4))->c_str());
            // the strings destroyed (the block is in the arena)
            for (DWORD i = 0; i < m && D(arr, 4); i++)
                at<DWORD(THISCALL *)(void *)>(0x4defd0)(vp(D(arr, 4) + i * 4));
        } else {
            if (D(arr, 4))
                content[side] = fnv(content[side], vp(D(arr, 4)), count * size);
            if (D(arr, 4) && m)
                pool_get_free(vp(D(arr, 4)), m * size);
        }
        arena_off();
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[seed %u: count %u / %u, capacity %u / %u, eax %u / %u, content %x / %x]", seed, state[0][0], state[1][0], state[0][1],
              state[1][1], state[0][2], state[1][2], content[0], content[1]);
    t.check(memcmp(state[0], state[1], sizeof state[0]) == 0 && content[0] == content[1] && g_used[0] == g_used[1] &&
                same_region(g_arena[0], g_arena[1], g_used[0]),
            fn == 0x5a4080 ? "PtrArray_ReservePool" : fn == 0x5a41b0 ? "Array8_ReservePool" : "StringArray_Reserve", d);
}

// ---- Real data: the camera tracker test on the level's parts ---------------------------------------------------

void test_real_parts(Tally &t, int &cases)
{
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28)) {
        log("  zax rules real data: no level grid found");
        return;
    }
    const DWORD columns = D(layer, 0x60), rows = D(layer, 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return;
    int total = 0;
    for (DWORD i = 0; i < columns * rows; i++)
        total += (int)cells[i].count;
    int index = 0;
    BYTE *app = *at<BYTE **>(0x713f14);
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || (c->count && !readable(c->data, c->count * 4)))
            return;
        for (DWORD k = 0; k < c->count; k++) {
            if (!spread_pick(index++, total, 200))
                continue;
            BYTE *part = static_cast<BYTE *>(c->data[k]);
            if (!class_of(part))
                continue;
            DWORD r[2];
            for (int side = 0; side < 2; side++) {
                side_begin(side);
                r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x5a2bf0)(app, part);
                side_end(side);
            }
            char d[60];
            wsprintfA(d, "[part %d: %x / %x]", index, r[0], r[1]);
            t.check(r[0] == r[1], "CZaxWorld::HasCameraTracker (real)", d);
            cases++;
        }
    }
}

}  // namespace

ZAX_CHECKS(zax_rules)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_zax_rules;
    if (!arena_setup()) {
        log("Check zax rules: no arena");
        return;
    }
    init_tables();
    Tally objects{"zax rules: CZaxCustomRules Create, CreateArray, destructors"}, getters{"zax rules: class getters"},
        fields{"zax rules: RegisterFields"}, registered{"zax rules: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, 1);
    for (unsigned k = 0; k < (unsigned)scaled(20); k++)
        test_base_dtor(objects, k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally options{"zax rules: the option objects made on first use"};
    for (int i = 0; i < 10; i++)
        for (unsigned s = 0; s < (unsigned)scaled(8); s++) {
            test_option(options, OPTIONS[i], 300u * (unsigned)i + s);
            test_option_set(options, OPTIONS[i], 300u * (unsigned)i + s + 100);
        }
    test_real_options(options);
    options.report();

    Tally app{"zax rules: the application's helpers on a fake application"};
    for (int k = 0; k < NAPP; k++) {
        log("  %s", APP_CASES[k].name);
        for (unsigned s = 0; s < (unsigned)scaled(150); s++)
            test_app_case(app, APP_CASES[k], 5000u * (unsigned)k + s);
    }
    test_real_app(app);
    app.report();

    Tally arrays{"zax rules: array capacities"};
    const DWORD fns[3] = {0x5a4080, 0x5a40f0, 0x5a41b0};
    for (DWORD f : fns)
        for (unsigned s = 0; s < (unsigned)scaled(200); s++)
            test_reserve(arrays, f, s);
    for (unsigned s = 0; s < (unsigned)scaled(50); s++) {   // DefinitionRecord_Init on a record of random bytes
        BYTE rec[2][0x80];
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            g_seed = s;
            for (DWORD k = 0; k < 0x80; k++)
                rec[side][k] = (BYTE)random();
            side_begin(side);
            r[side] = at<DWORD(THISCALL *)(void *)>(0x5a4060)(rec[side]) == ptrv(rec[side]);
            side_end(side);
        }
        arrays.check(r[0] && r[1] && memcmp(rec[0], rec[1], 0x80) == 0, "DefinitionRecord_Init");
    }
    arrays.report();

    Tally real{"zax rules: the level's parts (real data)"};
    int cases = 0;
    test_real_parts(real, cases);
    log("  %d real-data cases", cases);
    real.report();
}

}  // namespace zax
