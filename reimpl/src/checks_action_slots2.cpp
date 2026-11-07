// Checks of the action classes' behaviour 0x4c0000-0x4cafff (action_slots2.cpp) against the original,
// in the game thread. The pattern of checks_action_slots.cpp:
//
// Every case runs twice from the same seed: side 0 with the hooks of action_slots2 taken out (nested
// calls run the original too), side 1 through the reimplementation. Compared: eax (al where only al is
// defined), an event log of every call into fakes and stand-ins, and the state of the object under test
// and of the fakes afterwards (pointers to fakes by name, strings by text).
//
// The objects under test are real actions (made with the original Create, fields from the seed); their
// child actions are fake actions, the parts fake entities (slot 2 gives a real class so IsKindOf runs for
// real), the reporter, the world (0x6c2080), the model table (0x6cfe08/0x6cfe0c) and its models and
// sequences are fakes for the duration of a case. The exe functions the slots call outside the module
// jump to stand-ins (the name lookups and the per-target loop, the action queue, Object_Clone, the
// sounds, the layer part, entity and world functions, the message ids). String, class info and C
// runtime functions run for real.
//  - Deleting destructors of the 35 classes (flags 0-3), slot 23 of 17 functions, slot 27 of 25, the
//    helpers (the surface type, the relocation, the setters, the weapon), the ten converting wrappers
//    (real global objects and fakes with a text and field names).
//  - Real data: the module's actions in the running level: copies with their owned children replaced by
//    fakes through slot 23 and slot 27 in the fake world; the converting wrappers' texts for them.
#include "check_util.h"
#include "action_classes.h"
#include "layer_parts.h"
#include "ptr_array.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_action_slots2(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
void *vptr(DWORD v) { return reinterpret_cast<void *>(static_cast<uintptr_t>(v)); }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }

// ---- Sides, log and state ------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 128;
DWORD g_log[2][LOG][4];
DWORD g_calls;   // calls into stand-ins and fakes on this side (part of the answers)

void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
    g_calls++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    if (g_nlog[0] != g_nlog[1])
        log("    log lengths %d / %d", g_nlog[0], g_nlog[1]);
}
constexpr int SLOG = 256;
DWORD g_slog[2][SLOG];
int g_nslog[2];
void st(DWORD v)
{
    g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4);
    if (g_nslog[g_side] < SLOG)
        g_slog[g_side][g_nslog[g_side]] = v;
    g_nslog[g_side]++;
}
void log_first_state_difference()
{
    for (int i = 0; i < g_nslog[0] && i < g_nslog[1] && i < SLOG; i++)
        if (g_slog[0][i] != g_slog[1][i]) {
            log("    state %d: %x / %x (of %d / %d)", i, g_slog[0][i], g_slog[1][i], g_nslog[0], g_nslog[1]);
            return;
        }
    if (g_nslog[0] != g_nslog[1])
        log("    state lengths %d / %d", g_nslog[0], g_nslog[1]);
}
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }
unsigned zs_hash(const void *z) { return z ? text_hash(static_cast<const ZString *>(z)->c_str()) : 0; }
DWORD mix(DWORD a, DWORD b, DWORD c = 0)
{
    DWORD h = 2166136261u;
    h = (h ^ a) * 16777619u;
    h = (h ^ b) * 16777619u;
    h = (h ^ c) * 16777619u;
    return h ^ (h >> 13);
}

// The fixed versions of checks_geometry.cpp
bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) < 0x10000 || ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(vptr(a), &mi, sizeof mi) || mi.state != 0x1000 || (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}
CClassInfo *class_of(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 0x14))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    if (!readable(at<void *>(vt), 12))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || !readable(at<void *>(getter), 5) || *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000 || !readable(at<void *>(var), 4))
        return nullptr;
    return *at<CClassInfo **>(var);
}
bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END) && p < 0x80000000u; }

// ---- The module's hooks out and in ---------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 128;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    Hook list[MAX_SITES];
    const int n = hooks_action_slots2(nullptr);
    if (!hook_sites_fit("action_slots2", n, MAX_SITES))
        return;
    hooks_action_slots2(list);
    for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (site[0] != 0xe9)
            continue;   // not installed (Disable=, or bytes did not match)
        if (list[i].address + 5 + *reinterpret_cast<INT32 *>(site + 1) != ptrv(list[i].replacement))
            continue;
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Fakes ------------------------------------------------------------------------------------------------------

constexpr DWORD CLS_ACTION = 0x41a650, CLS_BASE = 0x487620, CLS_DESTRUCTABLE = 0x48bdf0, CLS_ANIMATED = 0x48dcc0, CLS_MOVABLE = 0x4903f0,
                CLS_CHARACTER = 0x49b810, CLS_MULTI = 0x4b8460, CLS_OPEN = 0x4bdf80;
CClassInfo *cls_at(DWORD getter) { return at<CClassInfo *(__cdecl *)()>(getter)(); }

// Fake actions: slot 23 answers `result` for the first `limit` calls, then 0
struct FakeAction {
    DWORD vt, id, calls, limit, result;
    CClassInfo *cls;
    DWORD pad[2];
};
constexpr int NFA = 24;   // 0-15 children, 16-23 the copies Object_Clone gives
alignas(16) FakeAction g_fa[NFA];
int g_next_fa;

// Fake entities: +0x1c flags, +0x48 the layer, +0x90 the behaviour stack, +0xb4; +0xd0 the index of
// the configuration
constexpr int NFE = 16;   // 0-9 the parts, 10-15 the copies
constexpr DWORD FE_SIZE = 0x140, FE_INDEX = 0xd0;
alignas(16) BYTE g_fe[NFE][FE_SIZE];
struct EntityCfg {
    CClassInfo *cls;
    int model;      // 0 none, 1-2
    DWORD handled;  // slot 63
    BYTE answer;
    int ai;         // slot 64: 0 none, 1-2
    float hp;       // hit points (left)
    int height;     // slot 54
    DWORD children; // 0x4fc7c0
    int inventory;  // slot 36: 0 none, 1
    BYTE shooting, waiting;
};
constexpr int NCFG = 24;
EntityCfg g_cfg[NCFG];
int g_next_fe, g_next_cfg;
alignas(16) BYTE g_model[3][0x100];
alignas(16) BYTE g_ai[3][0x20];
alignas(16) BYTE g_layer[2][0x100];
alignas(16) BYTE g_seq[2][0x60];       // fake sequences: +0x48 frames, slot 26 the speed
alignas(16) BYTE g_model_table[4][0x80];
alignas(16) BYTE g_inventory[0x20];
alignas(16) BYTE g_world[0x300];
alignas(16) BYTE g_world_part[0x20];   // world +0x2a8 (slot 45)
alignas(16) BYTE g_reporter[0x10];
alignas(16) BYTE g_items[8][0x10];     // CPlaySoundBySurfaceTypeAction's specializations
DWORD g_each_result;
int g_resolve;            // 0x41ad40: -1 none, else the fake entity
int g_seq_pick;           // 0x517360: -1 none, else the sequence
DWORD g_surface, g_level_exists, g_count_off;
ZString g_return_map;   // what the string field stand-in (0x415f50) gives

DWORD g_vt_action[64], g_vt_entity[140], g_vt_reporter[32], g_vt_seq[32], g_vt_inventory[32], g_vt_world_part[64];

// Objects of the case (real actions under test) and blocks the code allocated
struct Obj {
    BYTE *p;
    DWORD size;
};
Obj g_obj[8];
int g_nobj;
struct Block {
    void *p;
    DWORD size;
    bool object;   // a real object: destroyed through slot 1
};
Block g_blocks[16];
int g_nblocks;
void note_block(void *p, DWORD size, bool object = false)
{
    if (g_nblocks < 16)
        g_blocks[g_nblocks++] = {p, size, object};
}
void note_obj(BYTE *o, DWORD size)
{
    if (g_nobj < 8)
        g_obj[g_nobj++] = {o, size};
}

DWORD tag(DWORD v)
{
    if (!v)
        return 0;
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    if (in(g_fa, sizeof g_fa))
        return 0xf1000000u + (v - ptrv(g_fa));
    if (in(g_fe, sizeof g_fe))
        return 0xf2000000u + (v - ptrv(g_fe));
    if (in(g_model, sizeof g_model))
        return 0xf3000000u + (v - ptrv(g_model));
    if (in(g_ai, sizeof g_ai))
        return 0xf4000000u + (v - ptrv(g_ai));
    if (in(g_layer, sizeof g_layer))
        return 0xf5000000u + (v - ptrv(g_layer));
    if (in(g_seq, sizeof g_seq))
        return 0xf6000000u + (v - ptrv(g_seq));
    for (int i = 0; i < g_nobj; i++)
        if (in(g_obj[i].p, g_obj[i].size))
            return 0xf7000000u + ((DWORD)i << 12) + (v - ptrv(g_obj[i].p));
    for (int i = 0; i < g_nblocks; i++)
        if (in(g_blocks[i].p, g_blocks[i].size))
            return 0xf8000000u + ((DWORD)i << 12) + (v - ptrv(g_blocks[i].p));
    if (in(g_world, sizeof g_world))
        return 0xf9000000u + (v - ptrv(g_world));
    if (in(g_inventory, sizeof g_inventory))
        return 0xfa000000u + (v - ptrv(g_inventory));
    return v;
}
DWORD tagp(const void *p) { return tag(ptrv(p)); }

int fake_index(const BYTE *e)
{
    if (!readable(e + FE_INDEX, 4))
        return 0;
    const DWORD k = *reinterpret_cast<const DWORD *>(e + FE_INDEX);
    return (k >> 24) == 0xfe && (k & 0xff) < NCFG ? (int)(k & 0xff) : 0;
}
EntityCfg &cfg_of(const BYTE *e) { return g_cfg[fake_index(e)]; }

// -- fake action slots
void *THISCALL fa_delete(FakeAction *a, DWORD flags)
{
    ev(0xa001, tagp(a), flags);
    return a;
}
CClassInfo *THISCALL fa_class(FakeAction *a) { return a->cls; }
void THISCALL fa_19(FakeAction *a) { ev(0xa013, tagp(a)); }
void THISCALL fa_20(FakeAction *a, void *from) { ev(0xa014, tagp(a), tagp(from)); }
DWORD THISCALL fa_execute(FakeAction *a, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    ev(0xa017, tagp(a), mix(tagp(layer), tag(a2), mix(tag(a3), tag(a4))), a->calls);
    const DWORD r = a->calls < a->limit ? a->result : (a->result & 0xffffff00);
    a->calls++;
    return r;
}
template <int S> void THISCALL fa_trap(FakeAction *a) { ev(0xa0ff, tagp(a), S); }

// -- fake entity slots
void *THISCALL fe_delete(BYTE *e, DWORD flags)
{
    ev(0xe001, tagp(e), flags);
    return e;
}
CClassInfo *THISCALL fe_class(BYTE *e) { return cfg_of(e).cls; }
template <int S> void THISCALL fe_noarg(BYTE *e) { ev(0xe000 + S, tagp(e)); }
template <int S> void THISCALL fe_onearg(BYTE *e, DWORD v) { ev(0xe000 + S, tagp(e), tag(v)); }
void *THISCALL fe_inventory(BYTE *e)
{
    ev(0xe024, tagp(e));
    return cfg_of(e).inventory ? g_inventory : nullptr;
}
void THISCALL fe_position(BYTE *e, float *out)
{
    ev(0xe02b, tagp(e));
    out[0] = (float)(int)(mix(tagp(e), 1) % 4000) * 0.5f;
    out[1] = (float)(int)(mix(tagp(e), 2) % 4000) * 0.25f;
}
float THISCALL fe_y(BYTE *e)
{
    ev(0xe02d, tagp(e), g_calls);
    return (float)(int)(mix(tagp(e), g_calls) % 3000) * 0.5f + 0.125f;
}
int THISCALL fe_height(BYTE *e)
{
    ev(0xe036, tagp(e));
    return cfg_of(e).height;
}
void THISCALL fe_set_flags(BYTE *e, DWORD v)
{
    ev(0xe03a, tagp(e), v, dw(e, 0x1c));
    dw(e, 0x1c) |= v;
}
void THISCALL fe_clear_flags(BYTE *e, DWORD v)
{
    ev(0xe03b, tagp(e), v, dw(e, 0x1c));
    dw(e, 0x1c) &= ~v;
}
DWORD THISCALL fe_message(BYTE *e, BYTE *m)
{
    ev(0xe03f, tagp(e), dw(m, 0) ^ dw(m, 4) * 3, m[8]);
    const EntityCfg &c = cfg_of(e);
    m[8] = c.answer;
    return (mix(tagp(e), g_calls) & 0xffff0000) | c.handled;
}
void *THISCALL fe_ai(BYTE *e, DWORD c, DWORD x)
{
    ev(0xe040, tagp(e), c, x);
    return cfg_of(e).ai ? g_ai[cfg_of(e).ai] : nullptr;
}
DWORD THISCALL fe_70(BYTE *e)
{
    ev(0xe046, tagp(e));
    return mix(tagp(e), 70);
}
void THISCALL fe_damage(BYTE *e, DWORD hp, DWORD type, DWORD from) { ev(0xe063, tagp(e), hp ^ type * 3, tag(from)); }
void THISCALL fe_home(BYTE *e, float *pos) { ev(0xe081, tagp(e), bits(pos[0]), bits(pos[1])); }
template <int S> void THISCALL fe_trap(BYTE *e) { ev(0xe0ff, tagp(e), S); }

// -- reporter, sequence, inventory, the world's +0x2a8
float THISCALL rep_time(void *)
{
    ev(0xb00d);
    return (float)(int)(g_calls % 50) * 0.5f + 10.0f;
}
void THISCALL rep_message(void *, const char *text) { ev(0xb011, text_hash(text)); }
template <int S> void THISCALL rep_trap(void *) { ev(0xb0ff, S); }
float THISCALL seq_speed(BYTE *s)
{
    ev(0xc01a, tagp(s));
    return s == g_seq[0] ? 12.5f : 7.0f;
}
template <int S> void THISCALL seq_trap(void *) { ev(0xc0ff, S); }
DWORD THISCALL inv_weapon(void *, DWORD v)
{
    ev(0xd01a, v);
    return 0x7a000000u + (g_calls & 0xff);
}
template <int S> void THISCALL inv_trap(void *) { ev(0xd0ff, S); }
DWORD THISCALL wp_45(void *)
{
    ev(0xd12d, g_calls);
    return 0x2d00 + g_calls;
}
template <int S> void THISCALL wp_trap(void *) { ev(0xd1ff, S); }

template <int... S> struct Seq {};
template <int N, int... S> struct MakeSeq : MakeSeq<N - 1, N - 1, S...> {};
template <int... S> struct MakeSeq<0, S...> {
    using type = Seq<S...>;
};
template <int... S> void fill_action_traps(Seq<S...>) { ((g_vt_action[S] = ptrv((void *)&fa_trap<S>)), ...); }
template <int... S> void fill_entity_traps(Seq<S...>) { ((g_vt_entity[S] = ptrv((void *)&fe_trap<S>)), ...); }
template <int... S> void fill_reporter_traps(Seq<S...>) { ((g_vt_reporter[S] = ptrv((void *)&rep_trap<S>)), ...); }
template <int... S> void fill_seq_traps(Seq<S...>) { ((g_vt_seq[S] = ptrv((void *)&seq_trap<S>)), ...); }
template <int... S> void fill_inventory_traps(Seq<S...>) { ((g_vt_inventory[S] = ptrv((void *)&inv_trap<S>)), ...); }
template <int... S> void fill_world_part_traps(Seq<S...>) { ((g_vt_world_part[S] = ptrv((void *)&wp_trap<S>)), ...); }

void init_vtables()
{
    fill_action_traps(typename MakeSeq<64>::type{});
    fill_entity_traps(typename MakeSeq<140>::type{});
    fill_reporter_traps(typename MakeSeq<32>::type{});
    fill_seq_traps(typename MakeSeq<32>::type{});
    fill_inventory_traps(typename MakeSeq<32>::type{});
    fill_world_part_traps(typename MakeSeq<64>::type{});
    g_vt_action[1] = ptrv((void *)&fa_delete);
    g_vt_action[2] = ptrv((void *)&fa_class);
    g_vt_action[19] = ptrv((void *)&fa_19);
    g_vt_action[20] = ptrv((void *)&fa_20);
    g_vt_action[23] = ptrv((void *)&fa_execute);
    g_vt_entity[1] = ptrv((void *)&fe_delete);
    g_vt_entity[2] = ptrv((void *)&fe_class);
    g_vt_entity[36] = ptrv((void *)&fe_inventory);
    g_vt_entity[39] = ptrv((void *)&fe_noarg<39>);
    g_vt_entity[43] = ptrv((void *)&fe_position);
    g_vt_entity[45] = ptrv((void *)&fe_y);
    g_vt_entity[54] = ptrv((void *)&fe_height);
    g_vt_entity[58] = ptrv((void *)&fe_set_flags);
    g_vt_entity[59] = ptrv((void *)&fe_clear_flags);
    g_vt_entity[63] = ptrv((void *)&fe_message);
    g_vt_entity[64] = ptrv((void *)&fe_ai);
    g_vt_entity[70] = ptrv((void *)&fe_70);
    g_vt_entity[93] = ptrv((void *)&fe_onearg<93>);
    g_vt_entity[99] = ptrv((void *)&fe_damage);
    g_vt_entity[129] = ptrv((void *)&fe_home);
    g_vt_reporter[13] = ptrv((void *)&rep_time);
    g_vt_reporter[17] = ptrv((void *)&rep_message);
    g_vt_seq[26] = ptrv((void *)&seq_speed);
    g_vt_inventory[26] = ptrv((void *)&inv_weapon);
    g_vt_world_part[45] = ptrv((void *)&wp_45);
    dw(g_reporter, 0) = ptrv(g_vt_reporter);
}

void make_entity(BYTE *e, int cfg_index, DWORD seed)
{
    memset(e, 0, 0xd8);
    dw(e, 0) = ptrv(g_vt_entity);
    dw(e, 0x1c) = seed & 0x02c60001;
    dw(e, 0x48) = ptrv(g_layer[1]);
    dw(e, 0xb4) = 0x77;
    dw(e, FE_INDEX) = 0xfe000000u + (DWORD)cfg_index;
}
FakeAction *next_fake_action() { return &g_fa[g_next_fa < NFA ? g_next_fa++ : NFA - 1]; }

float pick_float()
{
    const float v[] = {0, 1, 2.5f, -1, 0.25f, 10, -0.0f, 3};
    return v[random() % 8];
}

// The fake world from a seed
void setup_fakes(unsigned seed)
{
    g_seed = seed * 2654435761u + 12345;
    CClassInfo *const action_classes[] = {cls_at(CLS_ACTION), cls_at(CLS_ACTION), cls_at(CLS_MULTI), cls_at(CLS_OPEN)};
    const DWORD results[] = {0, 1, 1, 2, 0x101, 0x100, 0xffffff01, 0x7fffff00};
    for (int i = 0; i < NFA; i++) {
        FakeAction &a = g_fa[i];
        a = {ptrv(g_vt_action), (DWORD)i + 1, 0, random() % 5, results[random() % 8], action_classes[random() % 4], {0, 0}};
    }
    g_next_fa = 16;
    CClassInfo *const entity_classes[] = {cls_at(CLS_ANIMATED), cls_at(CLS_MOVABLE), cls_at(CLS_BASE),     cls_at(CLS_DESTRUCTABLE),
                                          cls_at(CLS_CHARACTER), cls_at(CLS_ACTION), cls_at(CLS_ANIMATED)};
    const float hps[] = {0, 10, 25, -0.0f, 3.5f, 100, 0.001f, 50};
    for (int i = 0; i < NCFG; i++) {
        EntityCfg &c = g_cfg[i];
        c.cls = entity_classes[random() % 7];
        c.model = (int)(random() % 3);
        c.handled = random() % 3 == 0 ? 1 + (random() % 2) * 0x100 : (random() % 2) * 0x100;
        c.answer = (BYTE)(random() % 3);
        c.ai = (int)(random() % 3);
        c.hp = hps[random() % 8];
        c.height = (int)(random() % 90) - 20;
        c.children = random() % 3;
        c.inventory = (int)(random() % 2);
        c.shooting = (BYTE)(random() % 2);
        c.waiting = (BYTE)(random() % 2);
    }
    for (int i = 0; i < NFE; i++)
        make_entity(g_fe[i], i, random());
    g_next_fe = 10;
    g_next_cfg = 16;
    for (int m = 1; m < 3; m++) {
        memset(g_model[m], 0, 0x100);
        dw(g_model[m], 0x44) = random() % 40;
        dw(g_model[m], 0xa4) = 0x100 + random() % 7;
    }
    for (int a = 1; a < 3; a++)
        memset(g_ai[a], 0, 0x20);
    memset(g_layer, 0, sizeof g_layer);
    for (int s = 0; s < 2; s++) {
        memset(g_seq[s], 0, sizeof g_seq[s]);
        dw(g_seq[s], 0) = ptrv(g_vt_seq);
        dw(g_seq[s], 0x48) = random() % 30;
    }
    memset(g_model_table, 0, sizeof g_model_table);
    for (int i = 0; i < 4; i++) {
        dw(g_model_table[i], 0x78) = random() % 3 ? ptrv(g_model[1 + random() % 2]) : 0;
        g_model_table[i][0x7f] = (BYTE)(random() % 4 == 0);
    }
    memset(g_inventory, 0, sizeof g_inventory);
    dw(g_inventory, 0) = ptrv(g_vt_inventory);
    memset(g_world, 0, sizeof g_world);
    dw(g_world, 0x2a8) = ptrv(g_world_part);
    dw(g_world, 0x280) = ptrv(g_world + 0x200);
    dw(g_world_part, 0) = ptrv(g_vt_world_part);
    for (int i = 0; i < 8; i++) {
        dw(g_items[i], 0) = 0x5f5f20;
        dw(g_items[i], 8) = random() % 4;
        dw(g_items[i], 0xc) = 0x300 + random() % 9;
    }
    g_each_result = random() % 3 == 0 ? 0x100 : random() % 2 ? 1 : 0x12345601;
    g_resolve = random() % 4 == 0 ? -1 : (int)(random() % 10);
    g_seq_pick = random() % 4 == 0 ? -1 : (int)(random() % 2);
    g_surface = random() % 5;
    g_level_exists = random() % 2 ? 0x101 : 0x100;
    g_count_off = 0;
}

// ---- Stand-ins for the exe functions the slots call ---------------------------------------------------------

void *__fastcall sr_resolve(void *layer, void *name, DWORD a2, DWORD a4, DWORD a3, void *caller, DWORD report, DWORD b)
{
    ev(0x41ad40, tagp(layer) ^ zs_hash(name), mix(tag(a2), tag(a4), tag(a3)), tagp(caller) ^ report * 7 ^ b * 13);
    return g_resolve < 0 ? nullptr : g_fe[g_resolve];
}
DWORD THISCALL sr_each(void *self, void *layer, void *name, DWORD a2, DWORD a3, DWORD a4, float delay, CClassInfo *cls)
{
    ev(0x41ae90, tagp(self) ^ tagp(layer), zs_hash(name) ^ mix(tag(a2), tag(a3), tag(a4)), bits(delay) ^ ptrv(cls));
    if (g_count_off)   // the counting tests: the count as slot 27 would leave it
        dw(self, g_count_off) += g_each_result & 3;
    return g_each_result;
}
void THISCALL sr_queue(void *queue, float delay, DWORD a4, DWORD a2, DWORD a3, DWORD b, void *action)
{
    ev(0x41c7a0, tagp(queue) ^ bits(delay), mix(tag(a4), tag(a2), tag(a3)) ^ b, tagp(action));
}
void *THISCALL sr_clone(void *o, DWORD a, DWORD b)
{
    ev(0x416760, tagp(o), a, b);
    FakeAction *c = next_fake_action();
    if ((tagp(o) >> 24) == 0xf1)
        c->result = static_cast<FakeAction *>(o)->result;
    return c;
}
void THISCALL sr_play_sound(void *p, DWORD sound) { ev(0x4fc8a0, tagp(p), sound); }
void THISCALL sr_world_sound(void *w, DWORD sound) { ev(0x4f6070, tagp(w), sound); }
void THISCALL sr_positioned(BYTE *self, BYTE *part, float *out, const char *text, DWORD x)
{
    ev(0x49c620, tagp(self) ^ tagp(part), text_hash(text), x);
    out[0] = (float)(int)(mix(tagp(part), 3) % 5000) * 0.37f - 20.0f;
    out[1] = (float)(int)(mix(tagp(part), 4) % 5000) * 0.61f - 30.0f;
}
DWORD THISCALL sr_surface_at(void *layer, int x, int y)
{
    ev(0x4e9e60, tagp(layer), (DWORD)x, (DWORD)y);
    return g_surface;
}
void THISCALL sr_vo_name(void *m, DWORD vo, void *out)
{
    ev(0x57e5d0, ptrv(m), vo);
    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(out, (vo & 1) ? "Voice/Hello" : "");
}
void *__cdecl sr_vo_player()
{
    ev(0x595b50);
    return g_world + 0x100;
}
DWORD THISCALL sr_vo_play(void *p, const char *name, DWORD a, DWORD b)
{
    ev(0x595be0, tagp(p), text_hash(name), a ^ b * 3);
    return 0x1200 + (g_calls & 0xff);
}
void THISCALL sr_relocated(void *e, DWORD v) { ev(0x48fe40, tagp(e), v); }
const ZString *THISCALL sr_string_field(void *f, void *o)
{
    ev(0x415f50, ptrv(f), tagp(o));
    return &g_return_map;
}
void THISCALL sr_string_field_set(void *f, void *o, const char *t) { ev(0x415f80, ptrv(f), tagp(o), text_hash(t)); }
float THISCALL sr_float_get(void *f, void *o)
{
    ev(0x54ede0, ptrv(f), tagp(o));
    return (float)(int)(mix(ptrv(f), tagp(o)) % 999) * 1.5f;
}
void THISCALL sr_float_set(void *f, void *o, float v) { ev(0x54ee10, ptrv(f), tagp(o), bits(v)); }
const char *THISCALL sr_file_name(void *layer)
{
    ev(0x4eb670, tagp(layer));
    return layer == g_layer[1] ? "Levels/Test/Here" : "Levels/Test/Other";
}
void THISCALL sr_position(BYTE *p, float *out)
{
    ev(0x4fb0a0, tagp(p));
    out[0] = (float)(int)(mix(tagp(p), 1) % 4000) * 0.5f;
    out[1] = (float)(int)(mix(tagp(p), 2) % 4000) * 0.25f;
}
DWORD THISCALL sr_level_exists(void *w, const char *name)
{
    ev(0x4f4ca0, tagp(w), text_hash(name));
    return g_level_exists;
}
void THISCALL sr_message_box(void *m, const char *text, DWORD a, DWORD b) { ev(0x506b90, tagp(m), text_hash(text), a ^ b * 5); }
DWORD THISCALL sr_move_to_spawn(void *w, void *p, const char *name, float *offset)
{
    ev(0x4f4ac0, tagp(w) ^ tagp(p), text_hash(name), bits(offset[0]) ^ bits(offset[1]) * 3);
    return 0x4f00 + (g_calls & 0xff);
}
DWORD THISCALL sr_child_count(BYTE *p)
{
    ev(0x4fc7c0, tagp(p));
    return cfg_of(p).children;
}
void *THISCALL sr_child_at(BYTE *p, DWORD i)
{
    ev(0x4fc7d0, tagp(p), i);
    return g_fe[(fake_index(p) + 3 + i) % 10];
}
DWORD THISCALL sr_shooting(BYTE *e)
{
    ev(0x48ec30, tagp(e));
    return 0x4800 | cfg_of(e).shooting;
}
DWORD THISCALL sr_waiting(BYTE *e)
{
    ev(0x48ebb0, tagp(e));
    return 0x4900 | cfg_of(e).waiting;
}
template <DWORD A> void THISCALL sr_one(void *p, DWORD v) { ev(A, tagp(p), tag(v)); }
BYTE *THISCALL sr_model(BYTE *e)
{
    ev(0x48b290, tagp(e));
    return cfg_of(e).model ? g_model[cfg_of(e).model] : nullptr;
}
void *THISCALL sr_holder(void *self, void *part, void *b, DWORD x)   // 0x58b2a0 on a new 0x1c-byte block
{
    note_block(self, 0x1c);
    ev(0x58b2a0, tagp(self), tagp(part) ^ tagp(b), x);
    return self;
}
void *THISCALL sr_behaviour_ctor(BYTE *self)   // 0x42f5e0 on a new 0x34-byte block: a fake action
{
    note_block(self, 0x34);
    memset(self, 0, 0x34);
    FakeAction *a = reinterpret_cast<FakeAction *>(self);
    a->vt = ptrv(g_vt_action);
    a->id = 0x42f5e0;
    a->cls = cls_at(CLS_ACTION);
    ev(0x42f5e0, tagp(self));
    return self;
}
void THISCALL sr_died(void *p) { ev(0x4fbf30, tagp(p)); }
void THISCALL sr_explosion(void *w, void *layer, DWORD model, float x, float y, int h, DWORD sound, DWORD z)
{
    ev(0x4f6280, tagp(w) ^ tagp(layer) ^ model, bits(x) ^ bits(y) * 3, (DWORD)h ^ sound * 7 ^ z * 11);
}
void THISCALL sr_load_model(void *list, DWORD i)
{
    ev(0x55e090, ptrv(list), i);
    dw(g_model_table[i & 3], 0x78) = ptrv(g_model[2]);
}
BYTE *THISCALL sr_default_seq(BYTE *m)
{
    ev(0x517360, tagp(m));
    return g_seq_pick < 0 ? nullptr : g_seq[g_seq_pick];
}
float THISCALL sr_hp(BYTE *e)
{
    ev(0x48d150, tagp(e));
    return cfg_of(e).hp;
}
float THISCALL sr_hp_left(BYTE *e)
{
    ev(0x48d030, tagp(e));
    return cfg_of(e).hp * 0.5f;
}
DWORD THISCALL sr_find_resource(void *list, const char *name, int hint)
{
    ev(0x523df0, ptrv(list), text_hash(name), (DWORD)hint);
    return 0x13572468;
}
DWORD __fastcall sr_flip(BYTE *e)
{
    ev(0x4bce40, tagp(e));
    return 0x4b00 | (e ? fake_index(e) & 1 : 0);
}
void THISCALL sr_set_position(void *o, DWORD a, DWORD b, DWORD c) { ev(0x49c600, tagp(o), a ^ b * 3, c); }
void *__cdecl sr_sound_env()
{
    ev(0x57aef0);
    return g_world + 0x180;
}
DWORD __fastcall sr_message_id(const char *name)
{
    ev(0x506680, text_hash(name));
    return 0x5060000 + (text_hash(name) & 0xfff);
}
DWORD THISCALL sr_find_in_rect(void *p, BYTE *m, DWORD radius, float f, DWORD flags, DWORD a, DWORD b)
{
    ev(0x4fb910, tagp(p) ^ dw(m, 0) ^ dw(m, 4) * 3 ^ m[8], radius ^ bits(f), flags ^ a * 3 ^ b * 5);
    return 0x4fb900 + (g_calls & 0xff);
}
void THISCALL sr_render_height(void *e, int v) { ev(0x48b6b0, tagp(e), (DWORD)v); }
float THISCALL sr_set_y(void *e, float y)
{
    ev(0x48a0d0, tagp(e), bits(y));
    return y;
}
void *THISCALL sr_root(BYTE *p)
{
    ev(0x4fc6f0, tagp(p));
    return p ? g_fe[(fake_index(p) + 1) % 10] : nullptr;
}
void *THISCALL sr_discharged_message(BYTE *m, void *root, DWORD v)
{
    ev(0x599300, tagp(root), v);
    memset(m, 0, 0x14);
    dw(m, 0) = 0x5eeb64;
    dw(m, 4) = 0x5993;
    dw(m, 0xc) = ptrv(root);
    return m;
}
void THISCALL sr_shake(void *layer, DWORD time, BYTE *env) { ev(0x4e9f00, tagp(layer), time, tagp(env)); }
void THISCALL sr_push(void *stack, void *e, void *b, DWORD x) { ev(0x49a560, tagp(stack), tagp(e) ^ tagp(b), x); }

struct Fn {
    DWORD address;
    void *standin;
};
const Fn FNS[] = {
    {0x41ad40, (void *)&sr_resolve},        {0x41ae90, (void *)&sr_each},           {0x41c7a0, (void *)&sr_queue},
    {0x416760, (void *)&sr_clone},          {0x4fc8a0, (void *)&sr_play_sound},     {0x4f6070, (void *)&sr_world_sound},
    {0x49c620, (void *)&sr_positioned},     {0x4e9e60, (void *)&sr_surface_at},     {0x57e5d0, (void *)&sr_vo_name},
    {0x595b50, (void *)&sr_vo_player},      {0x595be0, (void *)&sr_vo_play},        {0x48fe40, (void *)&sr_relocated},
    {0x415f50, (void *)&sr_string_field},   {0x415f80, (void *)&sr_string_field_set}, {0x54ede0, (void *)&sr_float_get},
    {0x54ee10, (void *)&sr_float_set},      {0x4eb670, (void *)&sr_file_name},      {0x4fb0a0, (void *)&sr_position},
    {0x4f4ca0, (void *)&sr_level_exists},   {0x506b90, (void *)&sr_message_box},    {0x4f4ac0, (void *)&sr_move_to_spawn},
    {0x4fc7c0, (void *)&sr_child_count},    {0x4fc7d0, (void *)&sr_child_at},       {0x48ec30, (void *)&sr_shooting},
    {0x48ebb0, (void *)&sr_waiting},        {0x48ec40, (void *)&sr_one<0x48ec40>},  {0x48ebc0, (void *)&sr_one<0x48ebc0>},
    {0x48b290, (void *)&sr_model},          {0x42f5e0, (void *)&sr_behaviour_ctor}, {0x586f80, (void *)&sr_one<0x586f80>},
    {0x58b2a0, (void *)&sr_holder},         {0x4fbf30, (void *)&sr_died},           {0x4f6280, (void *)&sr_explosion},
    {0x55e090, (void *)&sr_load_model},     {0x517360, (void *)&sr_default_seq},    {0x48d150, (void *)&sr_hp},
    {0x48d030, (void *)&sr_hp_left},        {0x523df0, (void *)&sr_find_resource},  {0x4bce40, (void *)&sr_flip},
    {0x4fbc50, (void *)&sr_one<0x4fbc50>},  {0x4fbd80, (void *)&sr_one<0x4fbd80>},  {0x49c600, (void *)&sr_set_position},
    {0x57aef0, (void *)&sr_sound_env},      {0x506680, (void *)&sr_message_id},     {0x4fb910, (void *)&sr_find_in_rect},
    {0x48b6b0, (void *)&sr_render_height},  {0x48a0d0, (void *)&sr_set_y},          {0x4fc6f0, (void *)&sr_root},
    {0x599300, (void *)&sr_discharged_message}, {0x4e9f00, (void *)&sr_shake},      {0x49a560, (void *)&sr_push},
};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
CodePatch g_patches[NFN];
DWORD g_saved_reporter, g_saved_world, g_saved_table, g_saved_count;
void standins_on()
{
    for (int i = 0; i < NFN; i++)
        patch_jump(g_patches[i], FNS[i].address, FNS[i].standin);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c02cc) = ptrv(g_reporter);
    g_saved_world = *at<DWORD *>(0x6c2080);
    *at<DWORD *>(0x6c2080) = ptrv(g_world);
    g_saved_table = *at<DWORD *>(0x6cfe08);
    g_saved_count = *at<DWORD *>(0x6cfe0c);
    *at<DWORD *>(0x6cfe08) = g_seed % 5 ? ptrv(g_model_table) : 0;
    *at<DWORD *>(0x6cfe0c) = 3;
}
void standins_off()
{
    *at<DWORD *>(0x6cfe0c) = g_saved_count;
    *at<DWORD *>(0x6cfe08) = g_saved_table;
    *at<DWORD *>(0x6c2080) = g_saved_world;
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
    for (int i = NFN; i-- > 0;)
        unpatch(g_patches[i]);
}

// ---- Real objects under test ---------------------------------------------------------------------------------

const ActionClassDef *class_def(const char *name)
{
    for (const ActionClassDef &c : ACTION_CLASSES)
        if (lstrcmpA(c.name, name) == 0)
            return &c;
    return nullptr;
}
// CSwitchMapAction (0x5a5320, 0x20 bytes) is outside ACTION_CLASSES
constexpr DWORD SWITCH_MAP_CREATE = 0x5a5320, SWITCH_MAP_SIZE = 0x20;
DWORD class_size(const char *name)
{
    if (!lstrcmpA(name, "CSwitchMapAction"))
        return SWITCH_MAP_SIZE;
    return class_def(name)->size;
}
BYTE *create(const char *name)
{
    const DWORD c = !lstrcmpA(name, "CSwitchMapAction") ? SWITCH_MAP_CREATE : class_def(name)->create;
    BYTE *o = static_cast<BYTE *>(original<void *(__cdecl *)()>(c)());
    note_obj(o, class_size(name));
    return o;
}
void set_text(BYTE *o, DWORD off, const char *s) { at<ZString *(THISCALL *)(void *, const char *)>(0x4e1930)(o + off, s); }
const char *const TEXTS[] = {"", "$Trigger", "$Instigator", "Door 1", "Switch", "Levels/Test/Here", "Levels/Test/Other", "$return",
                             "Open", "x"};
const char *pick_text() { return TEXTS[random() % 10]; }
DWORD pick_part() { return random() % 4 == 0 ? 0 : ptrv(g_fe[random() % 10]); }
DWORD pick_value()
{
    switch (random() % 3) {
    case 0:
        return 0;
    case 1:
        return pick_part();
    default:
        return ptrv(&g_fa[random() % 16]);
    }
}

// The strings of the classes (a bit per dword of the object) and their owned pointers to fakes (not
// deleted by their destructors)
struct ClassInfo2 {
    const char *cls;
    DWORD strings;
};
const ClassInfo2 CLASSES2[] = {
    {"CPlaySoundAction", 0x8},           {"SyfaceTypeSoundItem", 0},           {"CPlaySoundBySurfaceTypeAction", 0x40},
    {"CPlayVOAction", 0},                {"CSwitchMapAction", 0x70},          {"CShowAction", 0x4},
    {"CHideAction", 0x4},                {"CSetCollidableAction", 0x4},       {"CSetModelAction", 0x4},
    {"CSetCategoryAction", 0x4},         {"CActivateAction", 0x4},            {"CDeactivateAction", 0x4},
    {"CDeleteAction", 0x4},              {"CSpawnEffectAction", 0x4},         {"CPainAction", 0x10},
    {"CDestroyAction", 0x4},             {"CFlipSwitchAction", 0x4},          {"CAddAIAction", 0x4},
    {"CRemoveAIAction", 0x4},            {"CDischargeWeaponThisFrameBaseAction", 0},
    {"CChangeSoundEnvironmentTypeAction", 0}, {"CCheckExistenceAction", 0x4}, {"CIsAliveAction", 0x4},
    {"CHealthIsAtLeast", 0x4},           {"CSendMessageAction", 0xc},         {"CBroadcastMessageToAreaAction", 0xc},
    {"CFakeRenderHeightAction", 0x4},    {"COffsetYSortAction", 0x4},         {"CSendWeaponDischargedMessage", 0},
    {"CSetHomeAction", 0xc},             {"CAddEffectAction", 0x4},           {"CShakeScreenAction", 0},
    {"CMultipleActionsAction", 0},       {"COpenDoorAction", 0x4},            {"CCloseDoorAction", 0x4},
    {"CToggleDoorAction", 0x4},          {"CSeriesAction", 0},                {"CAndAction", 0},
    {"CDelayAction", 0},
};
DWORD string_mask(const char *name)
{
    for (const ClassInfo2 &c : CLASSES2)
        if (!lstrcmpA(c.cls, name))
            return c.strings;
    return 0;
}

// Fields of the classes from the seed
void fill_fields(const char *name, BYTE *o)
{
    auto text = [&](DWORD off) { set_text(o, off, pick_text()); };
    const DWORD strings = string_mask(name);
    for (DWORD off = 8; off < 0x20; off += 4)
        if (strings >> (off / 4) & 1)
            text(off);
    if (!lstrcmpA(name, "CPlaySoundAction") || !lstrcmpA(name, "CPlayVOAction") || !lstrcmpA(name, "CChangeSoundEnvironmentTypeAction"))
        dw(o, 8) = random() % 4 ? 0x200 + random() % 50 : 0;
    else if (!lstrcmpA(name, "CPlaySoundBySurfaceTypeAction")) {
        fl(o, 8) = pick_float();
        fl(o, 0xc) = pick_float();
        fl(o, 0x10) = pick_float() * 7;
        dw(o, 0x14) = 0x200 + random() % 50;
        // the specializations: data and count set for the case, put back before the destructor
        dw(o, 0x1c) = ptrv(g_items);
        dw(o, 0x20) = random() % 9;
    } else if (!lstrcmpA(name, "CSwitchMapAction")) {
        fl(o, 8) = pick_float();
        fl(o, 0xc) = pick_float();
    } else if (!lstrcmpA(name, "CSetCollidableAction"))
        o[0xc] = (BYTE)(random() % 2);
    else if (!lstrcmpA(name, "CSetModelAction")) {
        dw(o, 0xc) = 0x30 + random() % 9;
        dw(o, 0x10) = random() % 2 ? 0x40 + random() % 9 : 0;
    } else if (!lstrcmpA(name, "CSetCategoryAction"))
        dw(o, 0xc) = random() & 0x1ffff;
    else if (!lstrcmpA(name, "CActivateAction")) {
        o[0xc] = (BYTE)(random() % 2);
        fl(o, 0x10) = pick_float();
        dw(o, 0x14) = random() % 2 ? ptrv(&g_fa[random() % 16]) : 0;
    } else if (!lstrcmpA(name, "CSpawnEffectAction")) {
        dw(o, 0xc) = random() % 5;
        fl(o, 0x10) = pick_float() * 3;
        fl(o, 0x14) = pick_float() * 5;
        dw(o, 0x18) = random() % 50;
        o[0x1c] = (BYTE)(random() % 3);
        dw(o, 0x20) = random() % 2 ? ptrv(&g_fa[random() % 16]) : 0;
    } else if (!lstrcmpA(name, "CPainAction")) {
        fl(o, 8) = pick_float() * 11;
        dw(o, 0xc) = random() % 2 ? 0x210 + random() % 9 : 0;
        dw(o, 0x14) = random() % 7;
    } else if (!lstrcmpA(name, "CAddAIAction") || !lstrcmpA(name, "CRemoveAIAction")) {
        dw(o, 0xc) = !lstrcmpA(name, "CAddAIAction") ? (random() % 3 ? ptrv(&g_fa[random() % 16]) : 0)
                                                      : (random() % 3 ? 0x6b0000 + random() % 9 : 0);
        fl(o, 0x10) = pick_float();
    } else if (!lstrcmpA(name, "CDischargeWeaponThisFrameBaseAction")) {
        fl(o, 8) = pick_float();
        fl(o, 0xc) = pick_float();
        fl(o, 0x10) = pick_float();
        fl(o, 0x18) = pick_float();
        o[0x1c] = (BYTE)(random() % 2);
    } else if (!lstrcmpA(name, "CCheckExistenceAction") || !lstrcmpA(name, "CIsAliveAction")) {
        dw(o, 0xc) = random() % 4;
        dw(o, 0x10) = random() % 3;
    } else if (!lstrcmpA(name, "CHealthIsAtLeast")) {
        fl(o, 0xc) = pick_float() * 10;
        dw(o, 0x10) = random() % 4;
        dw(o, 0x14) = random() % 3;
    } else if (!lstrcmpA(name, "CBroadcastMessageToAreaAction"))
        fl(o, 0x10) = pick_float() * 100;
    else if (!lstrcmpA(name, "CFakeRenderHeightAction") || !lstrcmpA(name, "COffsetYSortAction"))
        dw(o, 0xc) = (DWORD)((int)(random() % 80) - 30);
    else if (!lstrcmpA(name, "CAddEffectAction")) {
        dw(o, 0xc) = random() % 2 ? 0x220 + random() % 9 : 0;
        dw(o, 0x10) = ptrv(&g_fa[random() % 16]);
    } else if (!lstrcmpA(name, "CShakeScreenAction"))
        fl(o, 8) = pick_float();
}
// The specializations back to what Create gave, so the destructor sees the original state
void restore_fields(const char *name, BYTE *o)
{
    if (!lstrcmpA(name, "CPlaySoundBySurfaceTypeAction")) {
        dw(o, 0x1c) = 0;
        dw(o, 0x20) = 0;
    }
}

// The object's dwords as state: strings by text
void state_obj(const BYTE *o, DWORD size, DWORD strings)
{
    for (DWORD off = 0; off < size; off += 4) {
        if (strings >> (off / 4) & 1)
            st(zs_hash(o + off));
        else
            st(tag(*reinterpret_cast<const DWORD *>(o + off)));
    }
}
void state_action(const char *name, BYTE *o)
{
    state_obj(o, class_size(name), string_mask(name));
    restore_fields(name, o);
}
void state_fakes()
{
    for (int i = 0; i < NFA; i++)
        st(g_fa[i].calls);
    for (int i = 0; i < NFE; i++)
        st(dw(g_fe[i], 0xb4) ^ tag(dw(g_fe[i], 0x1c)));
    for (int m = 1; m < 3; m++)
        st(dw(g_model[m], 0x44));
    for (int i = 0; i < 4; i++)
        st(tag(dw(g_model_table[i], 0x78)));
}

// Destroys what a case left (through the original slot 1)
void cleanup()
{
    for (int i = 0; i < g_nblocks; i++) {
        if (g_blocks[i].object)
            original<void *(THISCALL *)(void *, DWORD)>(slot(g_blocks[i].p, 1))(g_blocks[i].p, 1);
        else
            pool_free(g_blocks[i].p, g_blocks[i].size);
    }
    g_nblocks = 0;
    for (int i = 0; i < g_nobj; i++)
        if (g_obj[i].p)
            original<void *(THISCALL *)(void *, DWORD)>(slot(g_obj[i].p, 1))(g_obj[i].p, 1);
    g_nobj = 0;
}

// ---- Both sides of a case ---------------------------------------------------------------------------------------

struct Case {
    const char *name;
    DWORD address;
    unsigned seed;
    DWORD ret_mask;   // 0xff: al only
};
DWORD g_ret[2];

template <typename B, typename R, typename S> void side(int s, const Case &c, B build, R run, S state)
{
    g_side = s;
    g_loghash[s] = g_statehash[s] = 2166136261u;
    g_nlog[s] = g_nslog[s] = 0;
    g_calls = 0;
    g_nobj = 0;
    g_nblocks = 0;
    setup_fakes(c.seed);
    build();
    if (s == 0)
        hooks_out();
    standins_on();
    g_ret[s] = run() & c.ret_mask;
    standins_off();
    if (s == 0)
        hooks_in();
    state();
    state_fakes();
    cleanup();
}
template <typename B, typename R, typename S> void run_case(Tally &t, const Case &c, B build, R run, S state)
{
    side(0, c, build, run, state);
    side(1, c, build, run, state);
    const bool same = g_ret[0] == g_ret[1] && g_loghash[0] == g_loghash[1] && g_statehash[0] == g_statehash[1];
    char d[160];
    wsprintfA(d, "[0x%x %s seed %u: eax %x / %x, events %d / %d]", c.address, c.name, c.seed, g_ret[0], g_ret[1], g_nlog[0], g_nlog[1]);
    t.check(same, "", d);
    if (!same && t.mismatches <= 10) {
        log_first_difference();
        log_first_state_difference();
    }
}

// ---- The cases ------------------------------------------------------------------------------------------------------

using Slot23 = DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD);
using Slot27 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *);
using Dtor = void *(THISCALL *)(void *, DWORD);

struct Dtor1 {
    const char *cls;
    DWORD address;
    bool array;
};
const Dtor1 DTORS[] = {
    {"SyfaceTypeSoundItem", 0x4c0290, true},     {"CDischargeWeaponThisFrameBaseAction", 0x4c04d0, false},
    {"CPlaySoundBySurfaceTypeAction", 0x4c05d0, true}, {"CPlayVOAction", 0x4c0b30, true},
    {"CSwitchMapAction", 0x4c0fe0, false},       {"CShowAction", 0x4c14d0, true},            {"CHideAction", 0x4c1790, true},
    {"CSetCollidableAction", 0x4c1a50, true},    {"CSetModelAction", 0x4c1da0, true},        {"CSetCategoryAction", 0x4c22e0, true},
    {"CActivateAction", 0x4c2690, true},         {"CDeactivateAction", 0x4c2bd0, true},      {"CDeleteAction", 0x4c2e90, true},
    {"CSpawnEffectAction", 0x4c3180, true},      {"CPainAction", 0x4c38f0, true},            {"CDestroyAction", 0x4c3df0, true},
    {"CFlipSwitchAction", 0x4c4110, true},       {"CAddAIAction", 0x4c43e0, true},           {"CRemoveAIAction", 0x4c47c0, true},
    {"CDischargeWeaponThisFrameBaseAction", 0x4c4ba0, true}, {"CChangeSoundEnvironmentTypeAction", 0x4c55c0, true},
    {"CCheckExistenceAction", 0x4c5850, true},   {"CIsAliveAction", 0x4c5b90, true},         {"CHealthIsAtLeast", 0x4c5f20, true},
    {"CSendMessageAction", 0x4c6330, true},      {"CBroadcastMessageToAreaAction", 0x4c66f0, true},
    {"CFakeRenderHeightAction", 0x4c6c10, true}, {"COffsetYSortAction", 0x4c6fb0, true},
    {"CSendWeaponDischargedMessage", 0x4c7320, true}, {"CSetHomeAction", 0x4c7540, true},    {"CAddEffectAction", 0x4c7a30, true},
    {"CShakeScreenAction", 0x4c7e80, true},
};

// Deleting destructors: one object (flags 0 and 1) or an array of 0-3 (flags 2 and 3)
void test_dtors(Tally &t)
{
    for (const Dtor1 &d : DTORS) {
        const ActionClassDef *c = class_def(d.cls);
        const DWORD size = class_size(d.cls);
        const int cases = scaled(12);
        for (int k = 0; k < cases; k++) {
            const DWORD flags = d.array ? (DWORD)k % 4 : (DWORD)k % 2;
            const Case cs{d.cls, d.address, 7000u + (unsigned)k * 31 + d.address % 997, 0xffffffff};
            BYTE *obj = nullptr;
            DWORD *block = nullptr;
            DWORD count = 0;
            run_case(
                t, cs,
                [&] {
                    if (flags & 2) {
                        count = (DWORD)k % 4;
                        BYTE *e = static_cast<BYTE *>(original<void *(__fastcall *)(int, int, int)>(c->create_array)((int)count, 0, 0));
                        block = reinterpret_cast<DWORD *>(e) - 1;
                        for (DWORD i = 0; i < count; i++) {
                            fill_fields(d.cls, e + i * size);
                            restore_fields(d.cls, e + i * size);
                        }
                        obj = e;
                    } else {
                        obj = create(d.cls);
                        fill_fields(d.cls, obj);
                        restore_fields(d.cls, obj);
                        g_nobj = 0;   // destroyed by the case
                    }
                },
                [&] {
                    void *r = at<Dtor>(d.address)(obj, flags);
                    return r == obj ? 1u : r == block ? 2u : ptrv(r);
                },
                [&] {
                    if (!(flags & 1)) {   // what the destructor left (vtables; strings no longer valid)
                        if (flags & 2) {
                            for (DWORD i = 0; i < count; i++)
                                st(dw(obj + i * size, 0));
                            heap_free(block);
                        } else {
                            for (DWORD off = 0; off < size; off += 4)
                                st(string_mask(d.cls) >> (off / 4) & 1 ? 0 : tag(dw(obj, off)));
                            pool_free(obj, size);
                        }
                    }
                });
        }
    }
}

// CSwitchMapAction's slot 28 (the switch to another map) replaced by a logging stub in a copy of its
// vtable; the class's vtable 0x6032c4 is put back before the destructor
DWORD g_switch_vt[64];
DWORD THISCALL switch_stub(BYTE *self, ZString *m, ZString *l, void *p)
{
    ev(0x5a5f28, tagp(self), text_hash(m->c_str()) ^ text_hash(l->c_str()), tagp(p));
    return 0x2800 + (g_calls & 0xff);
}
void stub_switch(BYTE *o)
{
    memcpy(g_switch_vt, at<void *>(dw(o, 0)), sizeof g_switch_vt);
    g_switch_vt[28] = ptrv((void *)&switch_stub);
    dw(o, 0) = ptrv(g_switch_vt);
}

// Slot 23 and slot 27 of the classes on objects from the seed
struct Slot1 {
    const char *cls;
    DWORD address;
    DWORD mask;
    bool target;     // slot 27 (else slot 23)
    bool null_target;
};
const Slot1 SLOTS[] = {
    {"CPlaySoundAction", 0x4c01b0, 0xff, false, false},
    {"CPlaySoundBySurfaceTypeAction", 0x4c0950, 0xff, false, false},
    {"CPlayVOAction", 0x4c0ca0, 0xff, false, false},
    {"CSwitchMapAction", 0x4c1030, 0xffffffff, false, false},
    {"CShowAction", 0x4c3020, 0xffffffff, false, false},
    {"CActivateAction", 0x4c29c0, 0xffffffff, false, false},
    {"CSetModelAction", 0x4c2100, 0xffffffff, false, false},
    {"CDestroyAction", 0x4c3f80, 0xffffffff, false, false},
    {"CFlipSwitchAction", 0x4c42a0, 0xffffffff, false, false},
    {"CPainAction", 0x4c3c70, 0xffffffff, false, false},
    {"CChangeSoundEnvironmentTypeAction", 0x4c5730, 0xff, false, false},
    {"CCheckExistenceAction", 0x4c5db0, 0xffffffff, false, false},
    {"CIsAliveAction", 0x4c5db0, 0xffffffff, false, false},
    {"CHealthIsAtLeast", 0x4c61c0, 0xffffffff, false, false},
    {"CSendMessageAction", 0x4c6560, 0xff, false, false},
    {"CBroadcastMessageToAreaAction", 0x4c6a70, 0xff, false, false},
    {"CShakeScreenAction", 0x4c80d0, 0xff, false, false},
    {"CSendWeaponDischargedMessage", 0x4c7400, 0xffffffff, false, false},
    {"CSwitchMapAction", 0x4c1060, 0xff, true, false},
    {"CShowAction", 0x4c1660, 0xff, true, true},
    {"CHideAction", 0x4c1920, 0xff, true, true},
    {"CSetCollidableAction", 0x4c1c60, 0xff, true, true},
    {"CSetModelAction", 0x4c2130, 0xff, true, true},
    {"CSetCategoryAction", 0x4c2550, 0xff, true, true},
    {"CActivateAction", 0x4c29f0, 0xff, true, true},
    {"CDeactivateAction", 0x4c2d60, 0xff, true, true},
    {"CDeleteAction", 0x4c3050, 0xff, true, true},
    {"CSpawnEffectAction", 0x4c36a0, 0xff, true, true},
    {"CPainAction", 0x4c3ca0, 0xff, true, true},
    {"CDestroyAction", 0x4c3fb0, 0xff, true, false},
    {"CFlipSwitchAction", 0x4c42d0, 0xffffffff, true, true},
    {"CAddAIAction", 0x4c4690, 0xff, true, true},
    {"CRemoveAIAction", 0x4c4a70, 0xff, true, true},
    {"CCheckExistenceAction", 0x4c5a70, 0xff, true, true},
    {"CIsAliveAction", 0x4c5df0, 0xff, true, true},
    {"CHealthIsAtLeast", 0x4c6200, 0xff, true, true},
    {"CSendMessageAction", 0x4c6590, 0xff, true, true},
    {"CBroadcastMessageToAreaAction", 0x4c6aa0, 0xff, true, true},
    {"CFakeRenderHeightAction", 0x4c6e30, 0xff, true, true},
    {"COffsetYSortAction", 0x4c71d0, 0xff, true, true},
    {"CSetHomeAction", 0x4c7890, 0xff, true, false},
    {"CAddEffectAction", 0x4c7ce0, 0xff, true, true},
};

// The message slots pass slot 63's or 0x4fb910's eax on, otherwise only al is defined
DWORD target_mask(const Slot1 &x, BYTE *o, void *target)
{
    if (x.address == 0x4c6590 && target && static_cast<const ZString *>(static_cast<void *>(o + 0xc))->c_str()[0])
        return 0xffffffff;
    if (x.address == 0x4c6aa0 && target && static_cast<const ZString *>(static_cast<void *>(o + 8))->c_str()[0])
        return 0xffffffff;
    return x.mask;
}

void test_slots(Tally &t)
{
    for (const Slot1 &x : SLOTS) {
        const int cases = scaled(60);
        for (int k = 0; k < cases; k++) {
            const Case cs{x.cls, x.address, 9000u + (unsigned)k * 17 + x.address % 991, 0xffffffff};
            BYTE *o = nullptr;
            DWORD a2 = 0, a3 = 0, a4 = 0;
            BYTE *target = nullptr;
            run_case(
                t, cs,
                [&] {
                    o = create(x.cls);
                    fill_fields(x.cls, o);
                    if (!lstrcmpA(x.cls, "CSwitchMapAction"))
                        stub_switch(o);
                    a2 = pick_value();
                    a3 = pick_value();
                    a4 = pick_value();
                    target = g_fe[random() % 10];
                    if (x.null_target && random() % 6 == 0)
                        target = nullptr;
                    g_count_off = x.address == 0x4c5db0 ? 0x10 : x.address == 0x4c61c0 ? 0x14 : 0;
                    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(&g_return_map, random() % 2 ? "Levels/Test/Here" : "Levels/Test/Back");
                    if (x.address == 0x4c7400 || x.address == 0x4c0950)   // a2 is not tested
                        a2 = ptrv(g_fe[random() % 10]);
                    if (x.address == 0x4c7890 && g_resolve >= 0)   // the target is not tested as a character
                        g_cfg[fake_index(target)].cls = cls_at(CLS_CHARACTER);
                    if (x.address == 0x4c29f0)   // the model is read without a test (as the original)
                        for (EntityCfg &c : g_cfg)
                            c.model = 1 + (c.model & 1);
                },
                [&] {
                    if (x.target)
                        return at<Slot27>(x.address)(o, a2, a3, a4, target) & target_mask(x, o, target);
                    return at<Slot23>(x.address)(o, g_layer[0], a2, a3, a4) & x.mask;
                },
                [&] {
                    state_action(x.cls, o);
                    if (!lstrcmpA(x.cls, "CSwitchMapAction"))
                        dw(o, 0) = 0x6032c4;
                });
        }
    }
}

// The helpers: the surface type, the relocation, the setters, the weapon slots
void test_helpers(Tally &t)
{
    for (int k = 0; k < scaled(40); k++) {
        const Case cs{"CPlaySoundBySurfaceTypeAction", 0x4c0930, 41000u + (unsigned)k * 7, 0xffffffff};
        BYTE *o = nullptr;
        int x = 0, y = 0;
        run_case(
            t, cs,
            [&] {
                o = create("CPlaySoundBySurfaceTypeAction");
                x = (int)(random() % 5000) - 100;
                y = (int)(random() % 5000) - 100;
            },
            [&] { return at<DWORD(THISCALL *)(void *, void *, int, int)>(0x4c0930)(o, g_layer[1], x, y); },
            [&] { state_action("CPlaySoundBySurfaceTypeAction", o); });
    }
    // 0x4c11a0 with the action's own strings; slot 28 of the action is a logging stub
    for (int k = 0; k < scaled(120); k++) {
        const Case cs{"CSwitchMapAction", 0x4c11a0, 43000u + (unsigned)k * 11, 0xff};
        BYTE *o = nullptr;
        BYTE *target = nullptr;
        ZString *map = nullptr, *location = nullptr;
        run_case(
            t, cs,
            [&] {
                o = create("CSwitchMapAction");
                fill_fields("CSwitchMapAction", o);
                stub_switch(o);
                target = g_fe[random() % 10];
                map = reinterpret_cast<ZString *>(o + 0x10);
                location = reinterpret_cast<ZString *>(o + 0x14);
                at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(&g_return_map, random() % 2 ? "Levels/Test/Here" : "Levels/Test/Back");
            },
            [&] { return at<DWORD(THISCALL *)(void *, void *, void *, void *)>(0x4c11a0)(o, target, map, location); },
            [&] {
                state_action("CSwitchMapAction", o);
                dw(o, 0) = 0x6032c4;   // the class's vtable again for its destructor
            });
    }
    for (int k = 0; k < scaled(40); k++) {
        const bool set_name = k % 2;
        const Case cs{set_name ? "CShowAction" : "CDischargeWeaponThisFrameBaseAction", set_name ? 0x4c3070u : 0x4c4dd0u,
                      45000u + (unsigned)k * 13, 0xffffffff};
        BYTE *o = nullptr;
        DWORD v[4];
        ZString name;
        run_case(
            t, cs,
            [&] {
                o = create(cs.name);
                fill_fields(cs.name, o);
                for (DWORD &x : v)
                    x = random();
                at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&name, pick_text());
            },
            [&] {
                if (set_name)
                    return tag(at<DWORD(THISCALL *)(void *, void *)>(0x4c3070)(o, &name));
                return at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4c4dd0)(o, v[0], v[1], v[2], v[3]);
            },
            [&] {
                state_action(cs.name, o);
                at<void(THISCALL *)(void *)>(0x4defd0)(&name);
            });
    }
    for (int k = 0; k < scaled(40); k++) {
        const bool weapon = k % 2;
        const Case cs{"CDischargeWeaponThisFrameBaseAction", weapon ? 0x4c5480u : 0x4c4af0u, 47000u + (unsigned)k * 19,
                      weapon ? 0xffffffff : 0xff};
        BYTE *o = nullptr;
        BYTE *part = nullptr;
        run_case(
            t, cs,
            [&] {
                o = create(cs.name);
                fill_fields(cs.name, o);
                part = g_fe[random() % 10];
            },
            [&] {
                if (weapon)
                    return at<DWORD(THISCALL *)(void *, void *)>(0x4c5480)(o, part);
                return at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4c4af0)(o, 1, 2, 3, 4, 5, 6);
            },
            [&] { state_action(cs.name, o); });
    }
}

// The converting wrappers: the real global objects and fakes with a text and field names
struct Converter {
    DWORD object, vtable, describe, convert;
    const char *field;
};
const Converter CONVERTERS[] = {
    {0x6bfbc8, 0x5f5330, 0x4c8330, 0x4c85b0, "Action"}, {0x6bfcd0, 0x5f53b0, 0x4c8670, 0x4c88f0, "Action"},
    {0x6bfbe8, 0x5f5430, 0x4c89b0, 0x4c8d30, "Action"}, {0x6bfc68, 0x5f54ac, 0x4c8e00, 0x4c9080, "Action"},
    {0x6bfb10, 0x5f5528, 0x4c9140, 0x4c93c0, "Action"}, {0x6bfb38, 0x5f55a4, 0x4c9480, 0x4c9700, "Action"},
    {0x6bfb70, 0x5f5620, 0x4c97c0, 0x4c9a40, "Action"}, {0x6bfca0, 0x5f5bec, 0x4c9b00, 0x4c9d80, "Door Name"},
    {0x6bfc00, 0x5f5bf8, 0x4c9e30, 0x4ca0b0, "Door Name"}, {0x6bfb98, 0x5f5c04, 0x4ca160, 0x4ca3e0, "Door Name"},
};
const char *const CONVERT_SOURCES[] = {"CMultipleActionsAction", "CSeriesAction", "CAndAction", "COpenDoorAction", "CCloseDoorAction",
                                       "CToggleDoorAction", "CShowAction", "CDelayAction"};
bool is_list(const char *name)
{
    return !lstrcmpA(name, "CMultipleActionsAction") || !lstrcmpA(name, "CSeriesAction") || !lstrcmpA(name, "CAndAction");
}
void fill_convert_source(const char *name, BYTE *o)
{
    if (is_list(name)) {
        void **data = static_cast<void **>(pool_get_alloc(8 * 4));
        const DWORD n = random() % 4;
        for (DWORD i = 0; i < 8; i++)
            data[i] = i < n ? &g_fa[random() % 16] : nullptr;
        dw(o, 0xc) = ptrv(data);
        dw(o, 0x10) = n;
        dw(o, 0x14) = 8;
    } else if (string_mask(name) & 4) {
        set_text(o, 8, pick_text());
        if (class_size(name) >= 0x10)
            fl(o, 0xc) = pick_float();
    }
}
alignas(16) BYTE g_fake_wrapper[0x20];
void test_converters(Tally &t)
{
    for (const Converter &c : CONVERTERS)
        for (int k = 0; k < scaled(48); k++) {
            const char *src = CONVERT_SOURCES[k % 8];
            const bool fake = k / 8 % 2;
            const Case cs{src, c.describe, 49000u + (unsigned)k * 23 + c.describe % 977, 0xffffffff};
            BYTE *o = nullptr;
            CClassInfo *made = nullptr;
            bool out = false;
            run_case(
                t, cs,
                [&] {
                    o = create(src);
                    fill_convert_source(src, o);
                    out = random() % 3 != 0;
                    made = nullptr;
                    memset(g_fake_wrapper, 0, sizeof g_fake_wrapper);
                    dw(g_fake_wrapper, 0) = c.vtable;
                    dw(g_fake_wrapper, 8) = ptrv("Converted");
                },
                [&] {
                    const char *r = at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(c.describe)(
                        fake ? static_cast<void *>(g_fake_wrapper) : at<void *>(c.object), o, out ? &made : nullptr);
                    return r ? text_hash(r) : 0u;
                },
                [&] {
                    st(ptrv(made));
                    state_action(src, o);
                });
        }
    for (const Converter &c : CONVERTERS)
        for (int k = 0; k < scaled(24); k++) {
            const bool doors = c.vtable >= 0x5f5be0;
            const char *const sources[] = {"CMultipleActionsAction", "CSeriesAction", "CAndAction"};
            const char *const door_sources[] = {"COpenDoorAction", "CCloseDoorAction", "CToggleDoorAction"};
            const char *src = doors ? door_sources[k % 3] : sources[k % 3];
            const bool fake = k / 3 % 2;
            const Case cs{src, c.convert, 51000u + (unsigned)k * 29 + c.convert % 971, 0xffffffff};
            BYTE *o = nullptr;
            BYTE *made = nullptr;
            run_case(
                t, cs,
                [&] {
                    o = create(src);
                    fill_convert_source(src, o);
                    g_nobj = 0;   // deleted by the conversion
                    memset(g_fake_wrapper, 0, sizeof g_fake_wrapper);
                    dw(g_fake_wrapper, 0) = c.vtable;
                    dw(g_fake_wrapper, 0xc) = ptrv(c.field);
                    dw(g_fake_wrapper, 0x10) = ptrv(random() % 2 ? c.field : "No Such Field");
                },
                [&] {
                    made = static_cast<BYTE *>(at<void *(THISCALL *)(void *, void *)>(c.convert)(
                        fake ? static_cast<void *>(g_fake_wrapper) : at<void *>(c.object), o));
                    note_block(made, 0x24, true);
                    return dw(made, 0);
                },
                [&] {
                    const DWORD vt = dw(made, 0);
                    const DWORD size = doors ? 0x10 : vt == 0x5f533c || vt == 0x5f53bc ? 0x24 : vt == 0x5f543c ? 0x18 : 0x1c;
                    // the byte fields' upper bytes are what the pool block held
                    const DWORD byte_off = vt == 0x5f533c || vt == 0x5f53bc ? 0x1c : vt == 0x5f54b8 || vt == 0x5f5534 || vt == 0x5f55b0 ? 0x18 : 0;
                    for (DWORD off = 0; off < size; off += 4)
                        st(doors && off == 8 ? zs_hash(made + 8) : off == 0xc && !doors ? 0 : off == byte_off ? dw(made, off) & 0xff : tag(dw(made, off)));
                    if (!doors) {
                        const DWORD n = dw(made, 0x10);
                        for (DWORD i = 0; i < n && i < 9; i++)
                            st(tag(reinterpret_cast<DWORD *>(vptr(dw(made, 0xc)))[i]));
                    }
                });
        }
}

// ---- Real data: the module's actions in the running level -------------------------------------------------

constexpr int MAX_SEEN = 16384, MAX_FOUND = 512;
DWORD g_seen[MAX_SEEN];
int g_nseen, g_objects;
BYTE *g_found[MAX_FOUND];
const char *g_found_class[MAX_FOUND];
int g_nfound;
const char *module_class(const void *p)
{
    CClassInfo *c = class_of(p);
    if (!c)
        return nullptr;
    for (const ClassInfo2 &x : CLASSES2) {
        if (!lstrcmpA(x.cls, "CSwitchMapAction") || !lstrcmpA(x.cls, "SyfaceTypeSoundItem") || !class_def(x.cls))
            continue;
        if (!lstrcmpA(x.cls, "CMultipleActionsAction"))
            break;   // the classes after it are other modules'
        if (*at<CClassInfo **>(class_def(x.cls)->var) == c)
            return x.cls;
    }
    return nullptr;
}
bool seen(DWORD p)
{
    for (int i = 0; i < g_nseen; i++)
        if (g_seen[i] == p)
            return true;
    if (g_nseen < MAX_SEEN)
        g_seen[g_nseen++] = p;
    return false;
}
void visit(BYTE *o, int depth)
{
    const CClassInfo *ci = class_of(o);
    if (!ci || g_nseen >= MAX_SEEN || seen(ptrv(o)))
        return;
    g_objects++;
    if (const char *n = module_class(o))
        if (g_nfound < MAX_FOUND) {
            g_found_class[g_nfound] = n;
            g_found[g_nfound++] = o;
        }
    if (depth >= 8 || ci->size > 0x1000 || !readable(o, ci->size))
        return;
    for (DWORD off = 4; off + 4 <= ci->size; off += 4) {
        const DWORD v = dw(o, off);
        if (!heap_pointer(v) || !readable(vptr(v), 4))
            continue;
        if (class_of(vptr(v)))
            visit(static_cast<BYTE *>(vptr(v)), depth + 1);
        else if (off + 8 <= ci->size && dw(o, off + 4) && dw(o, off + 4) <= 256 && readable(vptr(v), dw(o, off + 4) * 4))
            for (DWORD i = 0; i < dw(o, off + 4); i++)   // a pointer array's data and count
                visit(static_cast<BYTE **>(vptr(v))[i], depth + 1);
    }
}
bool find_level_actions()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return false;
    const DWORD columns = dw(layer, 0x60), rows = dw(layer, 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return false;
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || (c->count && !readable(c->data, c->count * 4)))   // an empty cell may have no data
            return false;
        for (DWORD k = 0; k < c->count; k++) {
            BYTE *part = static_cast<BYTE *>(c->data[k]);
            visit(part, 0);
            const LayerPart *p = reinterpret_cast<const LayerPart *>(part);
            if (readable(p, sizeof *p) && p->activity.count && p->activity.count <= 1000 && readable(p->activity.data, p->activity.count * 4))
                for (DWORD j = 0; j < p->activity.count; j++)
                    visit(static_cast<BYTE *>(p->activity.data[j]), 1);
        }
    }
    return true;
}

// The owned children of a copy (not deleted by the destructors) replaced by fakes, the copies of the
// real children deleted, so nothing of the game runs; the specializations of a copy kept as they are
void replace_children(const char *name, BYTE *o)
{
    const struct {
        const char *cls;
        DWORD off;
    } owned[] = {{"CActivateAction", 0x14}, {"CSpawnEffectAction", 0x20}, {"CAddAIAction", 0xc}, {"CAddEffectAction", 0x10}};
    for (const auto &w : owned)
        if (!lstrcmpA(w.cls, name))
            if (void *a = vptr(dw(o, w.off))) {
                original<Dtor>(slot(a, 1))(a, 1);
                dw(o, w.off) = ptrv(&g_fa[(w.off / 4) % 16]);
            }
}

void test_level(bool found, Tally &t)
{
    if (!found) {
        log("  action slots2 real data: no level grid found");
        return;
    }
    log("  action slots2 real data: %d objects reached from the level's parts, %d of the module's classes", g_objects, g_nfound);
    for (int i = 0; i < g_nfound; i++) {
        BYTE *real = g_found[i];
        const char *name = g_found_class[i];
        for (const Slot1 &x : SLOTS) {
            if (lstrcmpA(x.cls, name))
                continue;
            const Case cs{name, x.address, 55000u + (unsigned)i * 7 + (x.target ? 1u : 0u), 0xffffffff};
            BYTE *copy = nullptr;
            DWORD a1 = 0, a2 = 0, a3 = 0;
            BYTE *target = nullptr;
            run_case(
                t, cs,
                [&] {
                    copy = static_cast<BYTE *>(at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(real, 0, 0xffffffff));
                    replace_children(name, copy);
                    note_obj(copy, class_size(name));
                    a1 = pick_value();
                    a2 = pick_value();
                    a3 = pick_value();
                    target = g_fe[1];
                    g_count_off = x.address == 0x4c5db0 ? 0x10 : x.address == 0x4c61c0 ? 0x14 : 0;
                    if (x.address == 0x4c7400 || x.address == 0x4c0950)
                        a1 = ptrv(g_fe[2]);
                    if (x.address == 0x4c7890 && g_resolve >= 0)
                        g_cfg[1].cls = cls_at(CLS_CHARACTER);
                    if (x.address == 0x4c29f0)
                        for (EntityCfg &c : g_cfg)
                            c.model = 1 + (c.model & 1);
                },
                [&] {
                    if (x.target)
                        return at<Slot27>(x.address)(copy, a1, a2, a3, target) & target_mask(x, copy, target);
                    return at<Slot23>(x.address)(copy, g_layer[0], a1, a2, a3) & x.mask;
                },
                [&] { state_obj(copy, class_size(name), string_mask(name)); });
        }
        for (const Converter &c : CONVERTERS) {
            const Case cs{name, c.describe, 57000u + (unsigned)i, 0xffffffff};
            CClassInfo *made = nullptr;
            run_case(
                t, cs, [&] { made = nullptr; },
                [&] {
                    const char *r = at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(c.describe)(at<void *>(c.object), real, &made);
                    return r ? text_hash(r) : 0u;
                },
                [&] { st(ptrv(made)); });
        }
    }
}

}  // namespace

ZAX_CHECKS(action_slots2)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally dtors{"action slots2: deleting destructors"}, slots{"action slots2: slot 23 and slot 27"},
        helpers{"action slots2: surface type, relocation, setters, weapon"}, converters{"action slots2: the converting wrappers"},
        level{"action slots2: the level's actions (real data)"};
    init_vtables();
    at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_return_map, "");
    const bool found = find_level_actions();
    test_dtors(dtors);
    dtors.report();
    test_slots(slots);
    slots.report();
    test_helpers(helpers);
    helpers.report();
    test_converters(converters);
    converters.report();
    test_level(found, level);
    level.report();
    at<void(THISCALL *)(void *)>(0x4defd0)(&g_return_map);
    log("  action slots2: %u ms", GetTickCount() - start);
}

}  // namespace zax
