// Checks of the AI classes of 0x42d720-0x42f8e0 (ai_approach.cpp) against the original, in the game thread.
//  - The classes with the machinery of checks_inventory_common.h: both class getter copies (the class
//    variable set and 0), Create on a filled pool block and CreateArray (destroyed through their slot 1:
//    the deleting destructors), RegisterFields into a fake class info and against the registered
//    descriptors (real data).
//  - The slots and helpers in a fake world as in checks_ai_triggers.cpp: real objects from the original
//    Create with their fields from the seed, the embedded move behaviour and behaviour queue given a
//    recorder vtable for the case, fake parts, stand-ins for the sound layer (0x4ee4e0, 0x4ee6e0, 0x4ee650,
//    0x4ee610), the update monitor list (0x4e7330), the positions (0x4fb0a0), the angle (0x4fb2d0), the
//    move behaviour's aiming (0x4303f0) and the queue's test (0x586fb0). Compared: eax (masked, st(0) by
//    its float bits), the event log, the object and the fakes afterwards.
//  - The constructors and destructors on their own (CBehaviorActivity's 0x42f5e0, CAttackAI's 0x42e920, the
//    AI message's 0x42f680, CAmbientSoundAI's with a sound in the layer), Array_MoveElement on arrays of 4
//    and 8 byte elements, the oval angle 0x42e820 at random and special angles.
//  - Real data: copies of the running level's CAmbientSoundAI, CApproachTargetAI and CBehaviorActivity (their
//    own scalar fields; at most a spread of 40 per class) through the slots in the fake world.
#include "check_util.h"
#include "checks_ai_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_ai_approach(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
void setf(void *p, DWORD off, float v) { memcpy(static_cast<BYTE *>(p) + off, &v, 4); }

const invcheck::ClassDef CLASS_DEFS[] = {
    {"CAmbientSoundAI", 0x28, 0x62ea7c, 0x42d660, 0x42d6b0, 0x42d850, {0x42d610, 0x42d7f0}, true},
    {"CApproachTargetAI", 0xd8, 0x62ea84, 0x42e150, 0x42e1a0, 0x42e340, {0x42e0f0, 0x42e2d0}, true, false, nullptr, 0xc4},
    {"CAttackAI", 0x10, 0x62ea8c, 0, 0, 0, {0x42e870, 0x42e8c0}, false},
    {"CBehaviorActivity", 0x34, 0x62ead0, 0x42f360, 0x42f3b0, 0x42f550, {0x42f310, 0x42f4f0}, true},
    {"CEntityBehaviorMoveToPos", 0xbc, 0x62eae8, 0x42f730, 0x42f750, 0x42f8e0, {0x42f6d0, 0x42f870}, true, false, nullptr, 0xb4},
};

// ---- The module's hooks out and in ---------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    Hook list[MAX_SITES];
    const int n = hooks_ai_approach(nullptr);
    if (!hook_sites_fit("ai_approach", n, MAX_SITES))
        return;
    hooks_ai_approach(list);
    for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (site[0] != 0xe9 || list[i].address + 5 + *reinterpret_cast<INT32 *>(site + 1) != ptrv(list[i].replacement))
            continue;
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        write_code(g_sites[i].site, g_sites[i].bytes);
    g_nsites = 0;
}

// ---- Values, the event log, the state ---------------------------------------------------------------------------

struct Gen {
    unsigned s;
    DWORD next()
    {
        s = s * 1664525u + 1013904223u;
        DWORD x = s ^ (s >> 15);
        x *= 0x2c1b3c6du;
        return x ^ (x >> 12);
    }
    DWORD below(DWORD n) { return next() % n; }
    float pick(const float *v, DWORD n) { return v[below(n)]; }
};
Gen g_answers;
int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 96;
DWORD g_log[2][LOG][4];
BYTE *g_self;
DWORD g_self_size;
DWORD tag(DWORD v)
{
    if (g_self && v >= ptrv(g_self) && v < ptrv(g_self) + g_self_size)
        return 0xa0000000u + (v - ptrv(g_self));
    return v;
}
DWORD tagp(const void *p) { return tag(ptrv(p)); }
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3], g_log[1][i][0],
                g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    if (g_nlog[0] != g_nlog[1])
        log("    log lengths %d / %d", g_nlog[0], g_nlog[1]);
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void side_begin(int side, unsigned seed)
{
    g_side = side;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
    g_answers = Gen{seed * 2654435761u + 91};
}
CClassInfo *cls_at(DWORD getter) { return at<CClassInfo *(__cdecl *)()>(getter)(); }
constexpr DWORD CLS_ANIMATED = 0x48dcc0, CLS_BASE = 0x487620, CLS_WALKING = 0x49b810;

// ---- The fakes ------------------------------------------------------------------------------------------------

constexpr int NP = 6;
constexpr DWORD PART = 0x200;
struct alignas(16) Fakes {
    BYTE part[NP][PART];
    BYTE layer[0x100];
    BYTE resource[0x20];
    BYTE buffer[0x100];
    DWORD array[24];
};
Fakes g_f;
struct PartCfg {
    CClassInfo *cls;
    float pos[2];
    DWORD target;
};
PartCfg g_cfg[NP];
int part_index(const void *p)
{
    const DWORD v = ptrv(p) - ptrv(g_f.part);
    return v < sizeof g_f.part ? (int)(v / PART) : 0;
}
BYTE *part(Gen &g) { return g_f.part[g.below(NP)]; }
DWORD vt_part[140], vt_behaviour[60];

template <int S> DWORD THISCALL p_trap(BYTE *p) { ev(0xe0ff, ptrv(p), S); return 0; }
CClassInfo *THISCALL p_class(BYTE *p) { return g_cfg[part_index(p)].cls; }
void THISCALL p_message(BYTE *p, const BYTE *m) { ev(0xe03f, ptrv(p), *reinterpret_cast<const DWORD *>(m) ^ *reinterpret_cast<const DWORD *>(m + 4) * 3, m[8]); }
BYTE *THISCALL p_target(BYTE *p)
{
    ev(0xe046, ptrv(p));
    const DWORD t = g_cfg[part_index(p)].target;
    return t < NP ? g_f.part[t] : nullptr;
}
template <int S> DWORD THISCALL b_trap(BYTE *b) { ev(0xb0ff, tagp(b), S); return 0; }
DWORD THISCALL b_begin(BYTE *b, BYTE *e)
{
    ev(0xb017, tagp(b), ptrv(e));
    return g_answers.next();
}
DWORD THISCALL b_update(BYTE *b, DWORD dt, BYTE *e)
{
    ev(0xb019, tagp(b), dt, ptrv(e));
    return g_answers.next();
}

template <int... S> struct Seq {};
template <int N, int... S> struct MakeSeq : MakeSeq<N - 1, N - 1, S...> {};
template <int... S> struct MakeSeq<0, S...> {
    using type = Seq<S...>;
};
template <int... S> void fill_part(Seq<S...>) { ((vt_part[S] = ptrv((void *)&p_trap<S>)), ...); }
template <int... S> void fill_behaviour(Seq<S...>) { ((vt_behaviour[S] = ptrv((void *)&b_trap<S>)), ...); }
void init_vtables()
{
    fill_part(typename MakeSeq<140>::type{});
    fill_behaviour(typename MakeSeq<60>::type{});
    vt_part[2] = ptrv((void *)&p_class);
    vt_part[63] = ptrv((void *)&p_message);
    vt_part[70] = ptrv((void *)&p_target);
    vt_behaviour[23] = ptrv((void *)&b_begin);
    vt_behaviour[25] = ptrv((void *)&b_update);
}

const float POS[] = {0, 10, 100, -50, 512.5f, 33.25f, 1e6f, -0.0f, 150, 149};
void build_world(Gen &g, bool walkers)
{
    CClassInfo *const classes[] = {cls_at(CLS_WALKING), cls_at(CLS_ANIMATED), cls_at(CLS_BASE)};
    memset(&g_f, 0, sizeof g_f);
    for (int i = 0; i < NP; i++) {
        BYTE *p = g_f.part[i];
        dw(p, 0) = ptrv(vt_part);
        dw(p, 0x48) = g.below(6) ? ptrv(g_f.layer) : 0;
        dw(p, 0x12c) = g.next();
        g_cfg[i].cls = classes[walkers && i < 3 ? 0 : g.below(3)];
        g_cfg[i].pos[0] = g.pick(POS, 10);
        g_cfg[i].pos[1] = g.pick(POS, 10);
        g_cfg[i].target = g.below(NP + 2);
    }
    for (int i = 0; i < 24; i++)
        g_f.array[i] = 0x1000u + (DWORD)i;
}
void fakes_state() { st(fnv(2166136261u, &g_f, sizeof g_f)); }

// ---- Stand-ins ---------------------------------------------------------------------------------------------------

DWORD THISCALL s_sound_add(void *layer, DWORD sound, void *p)
{
    ev(0x4ee4e0, ptrv(layer), sound, ptrv(p));
    const DWORD v[] = {0xffffffff, 0, 3, 7};
    return v[g_answers.below(4)];
}
void THISCALL s_sound_distances(void *layer, DWORD h, DWORD inner, DWORD near_, DWORD far_) { ev(0x4ee6e0, ptrv(layer) ^ h * 3, inner, near_ ^ far_ * 5); }
void THISCALL s_sound_start(void *layer, DWORD h) { ev(0x4ee650, ptrv(layer), h); }
void THISCALL s_sound_remove(void *layer, DWORD h) { ev(0x4ee610, ptrv(layer), h); }
DWORD THISCALL s_monitor_add(void *list, void *p)
{
    ev(0x4e7330, ptrv(list), ptrv(p));
    return g_answers.next();
}
float *THISCALL s_position(BYTE *p, float *out)
{
    ev(0x4fb0a0, ptrv(p));
    out[0] = g_cfg[part_index(p)].pos[0];
    out[1] = g_cfg[part_index(p)].pos[1];
    return out;
}
long double THISCALL s_angle(BYTE *a, BYTE *b, DWORD x, DWORD scale)
{
    ev(0x4fb2d0, ptrv(a), ptrv(b), x ^ scale * 3);
    const float v[] = {0, 1.5707964f, -3.1415927f, 0.5f, 4, 2.25f};
    return (long double)v[g_answers.below(6)];
}
DWORD THISCALL s_move_to(BYTE *m, void *e, const float *pt, DWORD speed, DWORD flag)
{
    ev(0x4303f0, tagp(m) ^ ptrv(e) * 3, as_dword(pt[0]) ^ as_dword(pt[1]) * 3, speed ^ flag * 5);
    return g_answers.next();
}
DWORD THISCALL s_queue_empty(BYTE *q)
{
    ev(0x586fb0, tagp(q));
    return g_answers.next();
}
struct Fn {
    DWORD address;
    void *standin;
};
const Fn FNS[] = {{0x4ee4e0, (void *)&s_sound_add}, {0x4ee6e0, (void *)&s_sound_distances}, {0x4ee650, (void *)&s_sound_start},
                  {0x4ee610, (void *)&s_sound_remove}, {0x4e7330, (void *)&s_monitor_add},   {0x4fb0a0, (void *)&s_position},
                  {0x4fb2d0, (void *)&s_angle},       {0x4303f0, (void *)&s_move_to},       {0x586fb0, (void *)&s_queue_empty}};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
CodePatch g_patches[NFN];
void world_on()
{
    for (int i = 0; i < NFN; i++)
        patch_jump(g_patches[i], FNS[i].address, FNS[i].standin);
}
void world_off()
{
    for (int i = NFN; i-- > 0;)
        unpatch(g_patches[i]);
}

// ---- The objects under test -----------------------------------------------------------------------------------

enum Cls { C_AMBIENT, C_APPROACH, C_ACTIVITY, NCLS, C_BUFFER = -1 };
struct ClassDef {
    const char *name;
    DWORD create, size;
};
const ClassDef CLASSES[NCLS] = {{"CAmbientSoundAI", 0x42d660, 0x28}, {"CApproachTargetAI", 0x42e150, 0xd8}, {"CBehaviorActivity", 0x42f360, 0x34}};
DWORD g_saved_vt;   // the embedded object's vtable (put back before the object is destroyed)
const float RADII[] = {0, 10, 100, 1000, 0.5f, 64, 5000};
void fill(Gen &g, int c, BYTE *o, const BYTE *real)
{
    switch (c) {
    case C_AMBIENT:
        if (real) {
            for (DWORD off = 0x10; off < 0x1c; off += 4)
                dw(o, off) = dw(const_cast<BYTE *>(real), off);
        } else {
            setf(o, 0x10, g.pick(RADII, 7));
            setf(o, 0x14, g.pick(RADII, 7));
            setf(o, 0x18, g.pick(RADII, 7));
        }
        dw(o, 0x1c) = g.below(3) ? ptrv(g_f.resource) : 0;
        dw(o, 0x20) = g.below(2) ? 0xffffffff : g.below(9);
        dw(o, 0x24) = g.below(4) ? ptrv(g_f.layer) : 0;
        break;
    case C_APPROACH: {
        const float freq[] = {0, 2, 0.5f, 1};
        const float left[] = {0, -1, 0.5f, 2, 0.01f};
        g_saved_vt = dw(o, 0x10);
        dw(o, 0x10) = ptrv(vt_behaviour);
        dw(o, 0xb8) = g.next();
        if (real) {
            for (DWORD off = 0xcc; off < 0xd8; off += 4)
                dw(o, off) = dw(const_cast<BYTE *>(real), off);
        } else {
            setf(o, 0xcc, g.pick(RADII, 7));
            setf(o, 0xd0, g.pick(freq, 4));
            setf(o, 0xd4, g.pick(left, 5));
        }
        break;
    }
    case C_ACTIVITY:
        g_saved_vt = dw(o, 0x10);
        dw(o, 0x10) = ptrv(vt_behaviour);
        break;
    default:
        break;
    }
}
void object_state(BYTE *o, int c)
{
    for (DWORD off = 0; off < CLASSES[c].size; off += 4)
        st(tag(dw(o, off)));
}
void destroy(BYTE *o, int c)
{
    if (c == C_AMBIENT) {
        dw(o, 0x1c) = 0;
        dw(o, 0x20) = 0xffffffff;
    } else {
        dw(o, 0x10) = g_saved_vt;
        if (c == C_APPROACH)
            dw(o, 0xb8) = 0;
    }
    original<void *(THISCALL *)(void *, DWORD)>(slot(o, 1))(o, 1);
}

struct Test {
    const char *name;
    DWORD address;
    int cls;
    DWORD mask;
    int count;
    DWORD (*run)(Gen &g, BYTE *self, DWORD address);
    bool walkers;   // the first three parts walking entities (the slot reads the walker without a test)
};
template <typename... A> DWORD call(DWORD address, void *self, A... a) { return at<DWORD(THISCALL *)(void *, A...)>(address)(self, a...); }
DWORD g_ret[2];
bool run_case(Tally &t, const Test &test, unsigned seed, const BYTE *real)
{
    const int c = test.cls;
    for (int side = 0; side < 2; side++) {
        side_begin(side, seed);
        Gen g{seed * 69069u + 3};
        build_world(g, test.walkers);
        BYTE *self;
        if (c == C_BUFFER) {
            self = g_f.buffer;
            for (DWORD i = 0; i < sizeof g_f.buffer; i += 4)
                dw(self, i) = 0x5a5a0000u + i;
        } else {
            self = static_cast<BYTE *>(original<void *(__cdecl *)()>(CLASSES[c].create)());
            fill(g, c, self, real);
        }
        g_self = self;
        g_self_size = c == C_BUFFER ? sizeof g_f.buffer : CLASSES[c].size;
        if (side == 0)
            hooks_out();
        world_on();
        const DWORD r = test.run(g, self, test.address);
        world_off();
        if (side == 0)
            hooks_in();
        g_ret[side] = r & test.mask;
        if (c != C_BUFFER) {
            object_state(self, c);
            destroy(self, c);
        }
        g_self = nullptr;
        fakes_state();
    }
    const bool same = g_ret[0] == g_ret[1] && g_loghash[0] == g_loghash[1] && g_statehash[0] == g_statehash[1];
    char d[192];
    wsprintfA(d, "[0x%x %s seed %u: eax %x / %x, events %d / %d, state %x / %x]", test.address, test.name, seed, g_ret[0], g_ret[1], g_nlog[0],
              g_nlog[1], g_statehash[0], g_statehash[1]);
    t.check(same, "", d);
    if (!same && t.mismatches <= 10)
        log_first_difference();
    return same;
}

DWORD dt_bits(Gen &g)
{
    const float v[] = {0, 0.1f, 1, 5, 0.016f, 0.5f, 3};
    return as_dword(g.pick(v, 7));
}
DWORD float_result(long double r) { return as_dword((float)r); }
DWORD t_set_float(Gen &g, BYTE *self, DWORD a) { return float_result(at<long double(THISCALL *)(void *, float)>(a)(self, g.pick(RADII, 7))); }
DWORD t_set_sound(Gen &g, BYTE *self, DWORD a) { return call(a, self, g.below(2) ? ptrv(g_f.resource) : 0); }
DWORD t_ambient_begin(Gen &g, BYTE *self, DWORD a)
{
    BYTE *p = part(g);
    dw(p, 0x48) = ptrv(g_f.layer);
    return call(a, self, p);
}
DWORD t_ambient_update(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g), dt_bits(g)); }
DWORD t_approach_begin(Gen &g, BYTE *self, DWORD a) { return call(a, self, g.below(6) ? part(g) : nullptr); }
DWORD t_approach_update(Gen &g, BYTE *self, DWORD a) { return call(a, self, g_f.part[g.below(3)], dt_bits(g)); }
DWORD t_activity_update(Gen &g, BYTE *self, DWORD a) { return call(a, self, g.below(6) ? part(g) : nullptr, dt_bits(g)); }
DWORD t_activity_ctor(Gen &, BYTE *self, DWORD a)
{
    const DWORD r = tag(call(a, self));
    for (DWORD off = 0; off < 0x34; off += 4)
        st(tag(dw(self, off)));
    original<void *(THISCALL *)(void *, DWORD)>(0x42f430)(self, 0);   // destroyed in place
    return r;
}
DWORD t_attack_dtor(Gen &, BYTE *self, DWORD a)
{
    at<void *(THISCALL *)(void *)>(0x4fd060)(self);   // CAIBase's constructor
    dw(self, 0) = 0x5ee910;
    return tag(call(a, self)) != 0;
}
DWORD t_message_dtor(Gen &g, BYTE *self, DWORD a)
{
    const DWORD flags = g.below(2);
    BYTE *m = flags ? static_cast<BYTE *>(heap_alloc(0xc)) : self;
    dw(m, 4) = 7;
    const DWORD r = call(a, m, flags);
    if (!flags) {
        st(dw(m, 0));
        st(dw(m, 4));
    }
    return r == ptrv(m);
}
DWORD t_ambient_dtor(Gen &, BYTE *self, DWORD a)
{
    // the original Create's object with a sound in the layer (the removal through the stand-in)
    BYTE *o = static_cast<BYTE *>(original<void *(__cdecl *)()>(0x42d660)());
    dw(o, 0x20) = 5;
    dw(o, 0x24) = ptrv(g_f.layer);
    (void)self;
    return call(a, o, 1) == ptrv(o);
}
DWORD t_move_element(Gen &g, BYTE *, DWORD a)
{
    const DWORD size = g.below(2) ? 4 : 8, n = 24 / (size / 4);
    const DWORD to = g.below(n), from = g.below(n);
    const DWORD r = at<DWORD(__fastcall *)(void *, DWORD, DWORD, DWORD)>(a)(g_f.array, size, to, from);
    return r == ptrv(g_f.array) ? 1 : r >= ptrv(g_f.array) && r < ptrv(g_f.array) + sizeof g_f.array ? 2 + (r - ptrv(g_f.array)) : r;
}
DWORD t_angle(Gen &g, BYTE *, DWORD a)
{
    const DWORD special[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xff800000u, 0x40490fdbu, 0xc0490fdbu, 0x3fc90fdbu, 0x501502f9u};
    const DWORD v = g.below(3) ? as_dword((float)(int)(g.next() % 20000) / 1000.0f - 10.0f) : special[g.below(9)];
    return float_result(at<long double(__stdcall *)(DWORD)>(a)(v));
}

const Test TESTS[] = {
    {"Ambient inner radius", 0x42db80, C_AMBIENT, 0xffffffff, 80, t_set_float, false},
    {"Ambient outer radius", 0x42dbc0, C_AMBIENT, 0xffffffff, 80, t_set_float, false},
    {"Ambient max distance", 0x42dc10, C_AMBIENT, 0xffffffff, 80, t_set_float, false},
    {"Ambient sound", 0x42dc60, C_AMBIENT, 0xffffffff, 60, t_set_sound, false},
    {"Ambient begin", 0x42dc90, C_AMBIENT, 0xffffffff, 40, t_ambient_begin, false},
    {"Ambient update", 0x42dcb0, C_AMBIENT, 0, 200, t_ambient_update, false},
    {"Ambient destructor with a sound", 0x42d720, C_BUFFER, 0xffffffff, 10, t_ambient_dtor, false},
    {"Approach begin", 0x42e580, C_APPROACH, 0xffffffff, 80, t_approach_begin, false},
    {"Approach update", 0x42e5d0, C_APPROACH, 0, 400, t_approach_update, true},
    {"Activity update", 0x42f610, C_ACTIVITY, 0, 200, t_activity_update, false},
    {"Activity constructor", 0x42f5e0, C_BUFFER, 0xffffffff, 10, t_activity_ctor, false},
    {"CAttackAI destructor", 0x42e920, C_BUFFER, 0xffffffff, 10, t_attack_dtor, false},
    {"AI message destructor", 0x42f680, C_BUFFER, 0xffffffff, 20, t_message_dtor, false},
    {"Array_MoveElement", 0x42e7c0, C_BUFFER, 0xffffffff, 300, t_move_element, false},
    {"Angle_ToOval", 0x42e820, C_BUFFER, 0xffffffff, 400, t_angle, false},
};

void test_slots(Tally &t)
{
    for (const Test &test : TESTS)
        for (int k = 0; k < scaled(test.count); k++)
            run_case(t, test, 3000u + (unsigned)k * 7919u + test.address % 1013, nullptr);
}

// ---- Real data ---------------------------------------------------------------------------------------------------

BYTE *g_parts[8192];
void test_real(Tally &t)
{
    const int n = aiw::level_parts(g_parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_approach real data: no level grid found");
        return;
    }
    const DWORD GETTERS[NCLS] = {0x42d7f0, 0x42e2d0, 0x42f4f0};
    int total[NCLS] = {}, index[NCLS] = {}, ran = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            BYTE *ais[64];
            const int count = aiw::part_ais(g_parts[i], g_parts, n, ais, 64);
            for (int k = 0; k < count; k++) {
                const CClassInfo *ci = aiw::class_of(ais[k]);
                for (int c = 0; c < NCLS; c++) {
                    if (!ci || ci != cls_at(GETTERS[c]) || !aiw::readable(ais[k], CLASSES[c].size))
                        continue;
                    if (pass == 0) {
                        total[c]++;
                        continue;
                    }
                    if (!spread_pick(index[c]++, total[c], 40))
                        continue;
                    for (const Test &test : TESTS)
                        if (test.cls == c) {
                            run_case(t, test, 0x9e3779b9u ^ ptrv(ais[k]) ^ test.address, ais[k]);
                            ran++;
                        }
                }
            }
        }
    log("  ai_approach real data: %d parts; %d ambient sound, %d approach target, %d behaviour activity AIs; %d cases", n, total[C_AMBIENT],
        total[C_APPROACH], total[C_ACTIVITY], ran);
}

}  // namespace

ZAX_CHECKS(ai_approach)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"ai approach: Create, CreateArray and the deleting destructors"}, getters{"ai approach: class getters"},
        fields{"ai approach: RegisterFields"}, real_fields{"ai approach: RegisterFields against the registered fields (real data)"},
        slots{"ai approach: slots and helpers in the fake world"}, real{"ai approach: the level's AIs through their slots (real data)"};
    init_vtables();
    invcheck::g_hook_table = hooks_ai_approach;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASS_DEFS, sizeof CLASS_DEFS / sizeof CLASS_DEFS[0]);
    else
        objects.check(false, "ai_approach", "[no arena]");
    objects.report();
    getters.report();
    fields.report();
    real_fields.report();
    test_slots(slots);
    slots.report();
    test_real(real);
    real.report();
    log("  ai approach: %u ms", GetTickCount() - start);
}

}  // namespace zax
