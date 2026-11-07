// Checks of the action classes' behaviour (action_slots.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with the hooks of action_slots taken out (nested
// calls run the original too), side 1 through the reimplementation. Compared: eax (al where only al is
// defined), an event log of every call into fakes and stand-ins, and the state of the object under test
// and of the fakes afterwards (pointers to fakes by name, strings by text).
//
// The objects under test are real actions (made with the original Create, fields from the seed). Their
// child actions are fake actions (a vtable whose slot 23 logs its arguments and answers from the seed,
// slot 1 logs the delete); the parts are fake entities (the slots the actions call log and answer, slot
// 2 gives a real class: CEntityAnimated, CEntityMovable, CEntityBase or CAction, so IsKindOf runs for
// real); the reporter is a fake. The exe functions the slots call outside the module jump to stand-ins
// for the duration of a case: the name lookups and the per-target loop (0x41ad40, 0x41ae90), the warning
// (0x41abd0), the action queue (0x41c7a0), Object_Clone, the random generator 0x712cc0, the entity and
// layer part functions, the models' sequence search, the behaviour constructors, atexit. String,
// class info and C runtime functions run for real.
//  - Deleting destructors of the 36 classes (flags 0-3, owned fake children, strings, action lists).
//  - Slot 23 of 26 classes, slot 27 of 12, slot 28 of 3, the CreateLayerPart helper, the read line
//    slot, the switch and door helpers, the shuffle, the editor's wrappers.
//  - Real data: the module's actions in the running level (none on Test/Nachbau1): the wrappers'
//    texts for them; copies with their own child actions replaced by fakes run (slot 23, slot 27 on
//    fake entities) in the fake world.
#include "check_util.h"
#include "action_classes.h"
#include "layer_parts.h"
#include "ptr_array.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_action_slots(Hook *out);

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
// The text of the shared string 0x62e6e4 (the wrappers return it)
const char *zs_c_str_global() { return reinterpret_cast<const ZString *>(at<void *>(0x62e6e4))->c_str(); }
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
    const int n = hooks_action_slots(nullptr);
    if (!hook_sites_fit("action_slots", n, MAX_SITES))
        return;
    hooks_action_slots(list);
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

constexpr DWORD CLS_ACTION = 0x41a650, CLS_BASE = 0x487620, CLS_ANIMATED = 0x48dcc0, CLS_MOVABLE = 0x4903f0, CLS_DELAY = 0x4b6650,
                CLS_MULTI = 0x4b8460, CLS_RANDOM = 0x4b9cd0;
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
    const char *seq;      // current sequence
    int model;            // 0 none, 1-2
    DWORD set_ok;         // 0x48f700
    DWORD handled;        // slot 63
    BYTE answer;
    int ai;               // slot 64: 0 none, 1-2
};
constexpr int NCFG = 24;  // the parts, the copies, the CEntityAnimated wrappers
EntityCfg g_cfg[NCFG];
int g_next_fe, g_next_cfg;
alignas(16) BYTE g_model[3][0x100];
DWORD g_model_seqs[3];    // a bit per name of SEQ_NAMES the model has
alignas(16) BYTE g_ai[3][0x20];
alignas(16) BYTE g_layer[2][0x100];
alignas(16) BYTE g_beh[0x40];
ZString g_seqz;           // what 0x48b000 gives
alignas(16) BYTE g_reporter[0x10];
DWORD g_each_result, g_match_result;
int g_resolve;            // 0x41ad40: -1 none, else the fake entity

const char *const SEQ_NAMES[] = {"Switch", "Switching", "Idle", "Switched", "Opening", "Closing", "Open", "Closed"};
const char *const CUR_SEQS[] = {"Idle", "Switched", "Open", "Closed", "open", "CLOSED", "Walk", ""};

DWORD g_vt_action[64], g_vt_entity[128], g_vt_reporter[32];

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
    if (in(g_beh, sizeof g_beh))
        return 0xf6000000u + (v - ptrv(g_beh));
    for (int i = 0; i < g_nobj; i++)
        if (in(g_obj[i].p, g_obj[i].size))
            return 0xf7000000u + ((DWORD)i << 12) + (v - ptrv(g_obj[i].p));
    for (int i = 0; i < g_nblocks; i++)
        if (in(g_blocks[i].p, g_blocks[i].size))
            return 0xf8000000u + ((DWORD)i << 12) + (v - ptrv(g_blocks[i].p));
    if (v == ptrv(&g_seqz))
        return 0xf9000000u;
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
BYTE *model_ptr(int m) { return m ? g_model[m] : nullptr; }

// -- fake action slots
void *THISCALL fa_delete(FakeAction *a, DWORD flags)
{
    ev(0xa001, tagp(a), flags);
    return a;
}
CClassInfo *THISCALL fa_class(FakeAction *a) { return a->cls; }
void THISCALL fa_19(FakeAction *a) { ev(0xa013, tagp(a)); }
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
float THISCALL fe_height(BYTE *e)
{
    ev(0xe037, tagp(e));
    return (float)(int)(mix(tagp(e), g_calls) % 200) * 0.75f;
}
DWORD THISCALL fe_message(BYTE *e, BYTE *m)
{
    ev(0xe03f, tagp(e), dw(m, 0) ^ dw(m, 4) * 3, m[8]);
    const EntityCfg &c = cfg_of(e);
    m[8] = c.answer;
    return (mix(tagp(e), g_calls) & 0xffff0000) | c.handled;
}
void *THISCALL fe_ai(BYTE *e, CClassInfo *c, DWORD x)
{
    ev(0xe040, tagp(e), ptrv(c), x);
    return cfg_of(e).ai ? g_ai[cfg_of(e).ai] : nullptr;
}
void THISCALL fe_face(BYTE *e, float a, DWORD b) { ev(0xe068, tagp(e), bits(a), b); }
template <int S> void THISCALL fe_vector(BYTE *e, float *v)
{
    ev(0xe000 + S, tagp(e), bits(v[0]) ^ bits(v[1]) * 3, bits(v[2]));
    v[0] = v[0] + 1.5f;
    v[2] = v[2] - (float)S;
}
template <int S> void THISCALL fe_trap(BYTE *e) { ev(0xe0ff, tagp(e), S); }

// -- reporter
float THISCALL rep_time(void *)
{
    ev(0xb00d);
    return (float)(int)(g_calls % 50) * 0.5f + 10.0f;
}
void THISCALL rep_message(void *, const char *text) { ev(0xb011, text_hash(text)); }
template <int S> void THISCALL rep_trap(void *) { ev(0xb0ff, S); }

template <int... S> struct Seq {};
template <int... S> void fill_action_traps(Seq<S...>) { ((g_vt_action[S] = ptrv((void *)&fa_trap<S>)), ...); }
template <int... S> void fill_entity_traps(Seq<S...>) { ((g_vt_entity[S] = ptrv((void *)&fe_trap<S>)), ...); }
template <int... S> void fill_reporter_traps(Seq<S...>) { ((g_vt_reporter[S] = ptrv((void *)&rep_trap<S>)), ...); }
template <int N, int... S> struct MakeSeq : MakeSeq<N - 1, N - 1, S...> {};
template <int... S> struct MakeSeq<0, S...> {
    using type = Seq<S...>;
};

void init_vtables()
{
    fill_action_traps(typename MakeSeq<64>::type{});
    fill_entity_traps(typename MakeSeq<128>::type{});
    fill_reporter_traps(typename MakeSeq<32>::type{});
    g_vt_action[1] = ptrv((void *)&fa_delete);
    g_vt_action[2] = ptrv((void *)&fa_class);
    g_vt_action[19] = ptrv((void *)&fa_19);
    g_vt_action[23] = ptrv((void *)&fa_execute);
    g_vt_entity[1] = ptrv((void *)&fe_delete);
    g_vt_entity[2] = ptrv((void *)&fe_class);
    g_vt_entity[19] = ptrv((void *)&fe_noarg<19>);
    g_vt_entity[20] = ptrv((void *)&fe_onearg<20>);
    g_vt_entity[33] = ptrv((void *)&fe_noarg<33>);
    g_vt_entity[39] = ptrv((void *)&fe_noarg<39>);
    g_vt_entity[55] = ptrv((void *)&fe_height);
    g_vt_entity[58] = ptrv((void *)&fe_onearg<58>);
    g_vt_entity[59] = ptrv((void *)&fe_onearg<59>);
    g_vt_entity[63] = ptrv((void *)&fe_message);
    g_vt_entity[64] = ptrv((void *)&fe_ai);
    g_vt_entity[69] = ptrv((void *)&fe_onearg<69>);
    g_vt_entity[104] = ptrv((void *)&fe_face);
    g_vt_entity[107] = ptrv((void *)&fe_vector<107>);
    g_vt_entity[118] = ptrv((void *)&fe_vector<118>);
    g_vt_reporter[13] = ptrv((void *)&rep_time);
    g_vt_reporter[17] = ptrv((void *)&rep_message);
    dw(g_reporter, 0) = ptrv(g_vt_reporter);
}

void make_entity(BYTE *e, int cfg_index, DWORD seed)
{
    memset(e, 0, 0xd8);
    dw(e, 0) = ptrv(g_vt_entity);
    dw(e, 0x1c) = seed & 0x00c00001;
    dw(e, 0x48) = ptrv(g_layer[1]);
    dw(e, 0xb4) = 0x77;
    dw(e, FE_INDEX) = 0xfe000000u + (DWORD)cfg_index;
}
FakeAction *next_fake_action()
{
    FakeAction *a = &g_fa[g_next_fa < NFA ? g_next_fa++ : NFA - 1];
    return a;
}

// The fake world from a seed
void setup_fakes(unsigned seed, bool models)
{
    g_seed = seed * 2654435761u + 12345;
    CClassInfo *const action_classes[] = {cls_at(CLS_ACTION), cls_at(CLS_ACTION), cls_at(CLS_DELAY), cls_at(CLS_MULTI), cls_at(CLS_RANDOM)};
    const DWORD results[] = {0, 1, 1, 2, 0x101, 0x100, 0xffffff01, 0x7fffff00};
    for (int i = 0; i < NFA; i++) {
        FakeAction &a = g_fa[i];
        a = {ptrv(g_vt_action), (DWORD)i + 1, 0, random() % 5, results[random() % 8], action_classes[random() % 5], {0, 0}};
    }
    g_next_fa = 16;
    CClassInfo *const entity_classes[] = {cls_at(CLS_ANIMATED), cls_at(CLS_ANIMATED), cls_at(CLS_MOVABLE), cls_at(CLS_BASE), cls_at(CLS_ACTION)};
    for (int i = 0; i < NCFG; i++) {
        EntityCfg &c = g_cfg[i];
        c.cls = entity_classes[random() % 5];
        c.seq = CUR_SEQS[random() % 8];
        c.model = models ? 1 + (int)(random() % 2) : (int)(random() % 3);
        c.set_ok = random() % 2;
        c.handled = random() % 3 == 0 ? 1 + (random() % 2) * 0x100 : (random() % 2) * 0x100;
        c.answer = (BYTE)(random() % 3);
        c.ai = (int)(random() % 3);
    }
    for (int i = 0; i < NFE; i++)
        make_entity(g_fe[i], i, random());
    g_next_fe = 10;
    g_next_cfg = 16;
    const float model_floats[] = {0, 1.5f, -1, 2.5f, 0.25f, -3};
    for (int m = 1; m < 3; m++) {
        memset(g_model[m], 0, 0x100);
        fl(g_model[m], 0x20) = model_floats[random() % 6];
        fl(g_model[m], 0x2c) = model_floats[random() % 6];
        dw(g_model[m], 0xa4) = 0x100 + random() % 7;
        dw(g_model[m], 0xa8) = 0x200 + random() % 7;
        g_model_seqs[m] = random() & 0xff;
    }
    for (int a = 1; a < 3; a++) {
        memset(g_ai[a], 0, 0x20);
        g_ai[a][0x10] = (BYTE)(random() % 2);
    }
    memset(g_layer, 0, sizeof g_layer);
    memset(g_beh, 0, sizeof g_beh);
    g_each_result = random() % 3 == 0 ? 0x100 : random() % 2 ? 1 : 0x12345601;
    g_match_result = random() % 2 ? 0 : 0x2301;
    g_resolve = random() % 4 == 0 ? -1 : (int)(random() % 10);
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
    return g_each_result;
}
void __fastcall sr_report(const char *text, const char *name, void *part, DWORD a, DWORD b, void *caller, DWORD report)
{
    ev(0x41abd0, text_hash(text) ^ text_hash(name), tagp(part) ^ mix(tag(a), tag(b)), tagp(caller) ^ report);
}
void THISCALL sr_queue(void *queue, float delay, DWORD a4, DWORD a2, DWORD a3, DWORD b, void *action)
{
    ev(0x41c7a0, tagp(queue) ^ bits(delay), mix(tag(a4), tag(a2), tag(a3)) ^ b, tagp(action));
}
void *THISCALL sr_clone(void *o, DWORD a, DWORD b)
{
    ev(0x416760, tagp(o), a, b);
    const DWORD t = tagp(o);
    if ((t >> 24) == 0xf2 || (t >> 24) == 0xf8) {
        BYTE *e = g_fe[g_next_fe < NFE ? g_next_fe++ : NFE - 1];
        const int k = g_next_cfg < NCFG ? g_next_cfg++ : NCFG - 1;
        g_cfg[k] = cfg_of(static_cast<BYTE *>(o));
        make_entity(e, k, mix(t, g_calls));
        return e;
    }
    FakeAction *c = next_fake_action();
    if ((t >> 24) == 0xf1)
        c->result = static_cast<FakeAction *>(o)->result;
    return c;
}
DWORD THISCALL sr_below(void *g, DWORD n)
{
    ev(0x55c470, ptrv(g), n);
    return n ? mix(n, g_calls) % n : 0;
}
float THISCALL sr_range(void *g, float lo, float hi)
{
    ev(0x55c550, ptrv(g), bits(lo), bits(hi));
    return lo + (hi - lo) * (float)(int)(mix(bits(lo), bits(hi), g_calls) % 1000) * 0.001f;
}
const char *THISCALL sr_seq_name(BYTE *e)
{
    ev(0x48aee0, tagp(e));
    return cfg_of(e).seq;
}
void *THISCALL sr_seq(BYTE *e)
{
    ev(0x48b000, tagp(e));
    return &g_seqz;
}
BYTE *THISCALL sr_model(BYTE *e)
{
    ev(0x48b290, tagp(e));
    return model_ptr(cfg_of(e).model);
}
DWORD THISCALL sr_set_seq(BYTE *e, const char *name)
{
    ev(0x48f700, tagp(e), text_hash(name));
    return cfg_of(e).set_ok ? 0x101 : 0x100;
}
void THISCALL sr_stop(BYTE *e, DWORD v) { ev(0x48b6b0, tagp(e), v); }
float THISCALL sr_height(BYTE *e, float h)
{
    ev(0x48b770, tagp(e), bits(h));
    return h * 2;
}
int THISCALL sr_find(BYTE *m, const char *name)
{
    ev(0x517240, tagp(m), text_hash(name));
    for (int i = 0; i < 8; i++)
        if (crt_stricmp(name, SEQ_NAMES[i]) == 0)
            return m == g_model[1] || m == g_model[2] ? ((g_model_seqs[m == g_model[1] ? 1 : 2] >> i & 1) ? i * 3 : -1) : -1;
    return -1;
}
void *__fastcall sr_play(BYTE *e, void *anim, float speed, void *end, DWORD a, DWORD b, DWORD c)
{
    ev(0x496e20, tagp(e) ^ zs_hash(anim), bits(speed) ^ zs_hash(end), mix(a, b, c));
    return g_beh;
}
void THISCALL sr_push(void *stack, void *e, void *b, DWORD x) { ev(0x49a560, tagp(stack), tagp(e) ^ tagp(b), x); }
void THISCALL sr_layer_add(void *p, DWORD v) { ev(0x4fc8a0, tagp(p), v); }
void THISCALL sr_set_name(void *p, void *name) { ev(0x4fbf10, tagp(p), zs_hash(name)); }
void THISCALL sr_position(BYTE *p, float *out)
{
    ev(0x4fb0a0, tagp(p));
    out[0] = (float)(int)(mix(tagp(p), 1) % 4000) * 0.5f;
    out[1] = (float)(int)(mix(tagp(p), 2) % 4000) * 0.25f;
}
void THISCALL sr_move(void *p, float x, float y) { ev(0x4fb680, tagp(p), bits(x), bits(y)); }
template <DWORD A> void THISCALL sr_one(void *p, void *v) { ev(A, tagp(p), tagp(v)); }
void *THISCALL sr_holder(void *self, void *part, void *b, DWORD x)   // 0x58b2a0 on a new 0x1c-byte block
{
    note_block(self, 0x1c);
    ev(0x58b2a0, tagp(self), tagp(part) ^ tagp(b), x);
    return self;
}
void *THISCALL sr_trigger_ai(void *self, BYTE *pain, float r, DWORD x)   // 0x420840 on a new 0x20-byte block
{
    note_block(self, 0x20);
    if (pain)
        note_block(pain, 0x18, true);
    // the pain action by its fields (vtable, hit points, the string's text, damage type)
    ev(0x420840, tagp(self), pain ? dw(pain, 0) ^ dw(pain, 8) * 3 ^ dw(pain, 0xc) * 5 ^ zs_hash(pain + 0x10) ^ dw(pain, 0x14) * 7 : 0,
       bits(r) ^ x);
    return self;
}
// Constructors of blocks the slots allocate: 0x48e920 (CEntityAnimated, 0xe4) becomes a fake entity,
// 0x42f5e0 (0x34) and 0x5aabb0 (0x20) fake actions
void *THISCALL sr_entity_ctor(BYTE *self)
{
    note_block(self, 0xe4);
    const int k = g_next_cfg < NCFG ? g_next_cfg++ : NCFG - 1;
    g_cfg[k] = g_cfg[0];
    g_cfg[k].cls = cls_at(CLS_ANIMATED);
    make_entity(self, k, k);
    ev(0x48e920, tagp(self));
    return self;
}
template <DWORD A, DWORD SIZE> void *THISCALL sr_object_ctor(BYTE *self)
{
    note_block(self, SIZE);
    memset(self, 0, SIZE);
    FakeAction *a = reinterpret_cast<FakeAction *>(self);
    a->vt = ptrv(g_vt_action);
    a->id = A;
    a->cls = cls_at(CLS_ACTION);
    ev(A, tagp(self));
    return self;
}
void *THISCALL sr_from_model(void *list, DWORD index, CClassInfo *c, DWORD a4, DWORD x, DWORD y)
{
    ev(0x5176f0, ptrv(list) ^ index, ptrv(c) ^ tag(a4), x ^ y * 3);
    if (index % 3 == 2)
        return nullptr;
    BYTE *e = g_fe[g_next_fe < NFE ? g_next_fe++ : NFE - 1];
    make_entity(e, (int)(index % 10), index);
    return e;
}
DWORD __fastcall sr_match(const char *a, const char *b)
{
    ev(0x587d00, text_hash(a), text_hash(b));
    return g_match_result;
}
DWORD THISCALL sr_find_or_fail(void *list, const char *name, DWORD hint)
{
    ev(0x591fc0, ptrv(list), text_hash(name), hint);
    return 0x13572468;
}
void __fastcall sr_sincos(float *s, float *c, float angle)
{
    ev(0x508fe0, bits(angle));
    *s = (float)(int)(bits(angle) % 2001 - 1000) * 0.001f;
    *c = (float)(int)(bits(angle) % 1999 - 1000) * 0.001f;
}
DWORD THISCALL sr_read_line(void *o, const char *name, const char *value, DWORD a3)
{
    ev(0x417500, tagp(o), text_hash(name) ^ text_hash(value), a3);
    return 0x4400 + (text_hash(value) & 0xff);
}
int __cdecl sr_atexit(DWORD f)
{
    ev(0x5d0863, f);
    return 0;
}

struct Fn {
    DWORD address;
    void *standin;
};
const Fn FNS[] = {
    {0x41ad40, (void *)&sr_resolve},       {0x41ae90, (void *)&sr_each},         {0x41abd0, (void *)&sr_report},
    {0x41c7a0, (void *)&sr_queue},         {0x416760, (void *)&sr_clone},        {0x55c470, (void *)&sr_below},
    {0x55c550, (void *)&sr_range},         {0x48aee0, (void *)&sr_seq_name},     {0x48b000, (void *)&sr_seq},
    {0x48b290, (void *)&sr_model},         {0x48f700, (void *)&sr_set_seq},      {0x48b6b0, (void *)&sr_stop},
    {0x48b770, (void *)&sr_height},        {0x517240, (void *)&sr_find},         {0x496e20, (void *)&sr_play},
    {0x49a560, (void *)&sr_push},          {0x4fc8a0, (void *)&sr_layer_add},    {0x4fbf10, (void *)&sr_set_name},
    {0x4fb0a0, (void *)&sr_position},      {0x4fb680, (void *)&sr_move},         {0x4fbd80, (void *)&sr_one<0x4fbd80>},
    {0x4fbc50, (void *)&sr_one<0x4fbc50>}, {0x4eb6f0, (void *)&sr_one<0x4eb6f0>}, {0x586f80, (void *)&sr_one<0x586f80>},
    {0x58b2a0, (void *)&sr_holder},        {0x420840, (void *)&sr_trigger_ai},   {0x48e920, (void *)&sr_entity_ctor},
    {0x42f5e0, (void *)&sr_object_ctor<0x42f5e0, 0x34>},                       {0x5aabb0, (void *)&sr_object_ctor<0x5aabb0, 0x20>},
    {0x5176f0, (void *)&sr_from_model},    {0x587d00, (void *)&sr_match},        {0x591fc0, (void *)&sr_find_or_fail},
    {0x508fe0, (void *)&sr_sincos},        {0x417500, (void *)&sr_read_line},    {0x5d0863, (void *)&sr_atexit},
};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
CodePatch g_patches[NFN];
DWORD g_saved_reporter;
void standins_on()
{
    for (int i = 0; i < NFN; i++)
        patch_jump(g_patches[i], FNS[i].address, FNS[i].standin);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c02cc) = ptrv(g_reporter);
}
void standins_off()
{
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
    for (int i = NFN; i-- > 0;)
        unpatch(g_patches[i]);
}

// The guard bytes and strings of the function-local statics (door and switch functions), the same on
// both sides
const DWORD STATIC_GUARDS[] = {0x6bfc1b, 0x6bfb31, 0x6bfb94};
const DWORD STATIC_STRINGS[] = {0x6bfb24, 0x6bfbb8, 0x6bfbc0, 0x6bfbb0, 0x6bfc1c, 0x6bfcb8, 0x6bfbe0, 0x6bfb50, 0x6bfc5c, 0x6bfc54, 0x6bfb4c};
BYTE g_guards[3];
DWORD g_strings[11];

// ---- Real objects under test ---------------------------------------------------------------------------------

const ActionClassDef *class_def(const char *name)
{
    for (const ActionClassDef &c : ACTION_CLASSES)
        if (lstrcmpA(c.name, name) == 0)
            return &c;
    return nullptr;
}
BYTE *create(const char *name)
{
    const ActionClassDef *c = class_def(name);
    BYTE *o = static_cast<BYTE *>(original<void *(__cdecl *)()>(c->create)());
    if (g_nobj < 8)
        g_obj[g_nobj++] = {o, c->size};
    return o;
}
void set_text(BYTE *o, DWORD off, const char *s) { at<ZString *(THISCALL *)(void *, const char *)>(0x4e1930)(o + off, s); }
const char *const TEXTS[] = {"", "$Trigger", "$Instigator", "Door 1", "Switch", "Idle", "Open", "Walk", "Opening", "x"};
const char *pick_text() { return TEXTS[random() % 10]; }
DWORD pick_part() { return random() % 4 == 0 ? 0 : ptrv(g_fe[random() % 10]); }
// A trigger, instigator or third argument: none, a fake part or a fake action (the slots may ask their class)
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
DWORD pick_child() { return random() % 5 == 0 ? 0 : ptrv(&g_fa[random() % 16]); }
float pick_float()
{
    const float v[] = {0, 1, 2.5f, -1, 0.25f, 10, -0.0f, 3};
    return v[random() % 8];
}
// An action list (+0xc data, +0x10 count, +0x14 capacity) with 8 entries in the pool, `count` of them used
void fill_list(BYTE *o, DWORD count, bool first_non_null)
{
    void **data = static_cast<void **>(pool_get_alloc(8 * 4));
    for (int i = 0; i < 8; i++)
        data[i] = vptr(pick_child());
    if (first_non_null && !data[0])
        data[0] = &g_fa[0];
    dw(o, 0xc) = ptrv(data);
    dw(o, 0x10) = count;
    dw(o, 0x14) = 8;
}

// The object's dwords as state: strings (the offsets in `strings`, a bit per dword) by text
void state_obj(const BYTE *o, DWORD size, DWORD strings)
{
    for (DWORD off = 0; off < size; off += 4) {
        if (strings >> (off / 4) & 1)
            st(zs_hash(o + off));
        else
            st(tag(*reinterpret_cast<const DWORD *>(o + off)));
    }
}
// An action list's entries as state
void state_list(const BYTE *o)
{
    const DWORD n = *reinterpret_cast<const DWORD *>(o + 0x14);
    const DWORD *data = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(*reinterpret_cast<const DWORD *>(o + 0xc)));
    for (DWORD i = 0; data && i < n && i < 8; i++)
        st(tag(data[i]));
}
void state_fakes()
{
    for (int i = 0; i < NFA; i++)
        st(g_fa[i].calls);
    for (int i = 0; i < NFE; i++)
        st(dw(g_fe[i], 0xb4) ^ tag(dw(g_fe[i], 0x1c)));
    for (int i = 0; i < 3; i++)
        st(g_guards[i] ^ *at<BYTE *>(STATIC_GUARDS[i]));
}

// Destroys what a case left (through the original slot 1; the fakes' answers no longer logged)
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
    bool models = false;   // every fake entity has a model (the door functions read it without a test, as the original)
};
DWORD g_ret[2];

// One side: the fakes from the seed, `build` makes the objects, `run` calls the function, `state`
// records what to compare
template <typename B, typename R, typename S> void side(int s, const Case &c, B build, R run, S state)
{
    g_side = s;
    g_loghash[s] = g_statehash[s] = 2166136261u;
    g_nlog[s] = g_nslog[s] = 0;
    g_calls = 0;
    g_nobj = 0;
    g_nblocks = 0;
    setup_fakes(c.seed, c.models);
    for (int i = 0; i < 3; i++)
        *at<BYTE *>(STATIC_GUARDS[i]) = g_guards[i];
    for (int i = 0; i < 11; i++)
        *at<DWORD *>(STATIC_STRINGS[i]) = g_strings[i];
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
    for (int i = 0; i < 3; i++)
        g_guards[i] = *at<BYTE *>(STATIC_GUARDS[i]);
    for (int i = 0; i < 11; i++)
        g_strings[i] = *at<DWORD *>(STATIC_STRINGS[i]);
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
using Slot28 = void *(THISCALL *)(void *, void *, DWORD, DWORD, DWORD);
using Dtor = void *(THISCALL *)(void *, DWORD);

// Fields of the classes from the seed
void fill_fields(const char *name, BYTE *o)
{
    auto child = [&](DWORD off) { dw(o, off) = pick_child(); };
    auto text = [&](DWORD off) { set_text(o, off, pick_text()); };
    if (!lstrcmpA(name, "CDelayAction")) {
        child(0x10);
        fl(o, 8) = pick_float();
        fl(o, 0xc) = pick_float();
        o[0x14] = (BYTE)(random() % 2);
    } else if (!lstrcmpA(name, "COnlyOnceAction") || !lstrcmpA(name, "CSucceedAction") || !lstrcmpA(name, "CFailAction") ||
               !lstrcmpA(name, "CNotAction"))
        child(8);
    else if (!lstrcmpA(name, "COnlyOnceEveryXSecondsAction")) {
        child(8);
        child(0xc);
        fl(o, 0x10) = pick_float();
        const float next[] = {0, 5, 12, 30, -1, 20.5f};
        fl(o, 0x14) = next[random() % 6];
    } else if (!lstrcmpA(name, "CImpersonateAction")) {
        text(8);
        text(0xc);
        child(0x10);
    } else if (!lstrcmpA(name, "CWhileAction")) {
        child(8);
        child(0xc);
        if (dw(o, 8) && random() % 4 == 0) {   // a condition that keeps succeeding: the limit of 100
            reinterpret_cast<FakeAction *>(vptr(dw(o, 8)))->limit = 150;
            reinterpret_cast<FakeAction *>(vptr(dw(o, 8)))->result = 1;
        }
    } else if (!lstrcmpA(name, "CRepeatAction")) {
        dw(o, 8) = random() % 6;
        child(0xc);
        fl(o, 0x10) = pick_float();
    } else if (!lstrcmpA(name, "CMultipleActionsAction") || !lstrcmpA(name, "CRandomAction") || !lstrcmpA(name, "CAndAction") ||
               !lstrcmpA(name, "CXOrAction") || !lstrcmpA(name, "COrAction")) {
        fill_list(o, random() % 6, false);
        if (class_def(name)->size > 0x18)
            o[0x18] = (BYTE)(random() % 2);
    } else if (!lstrcmpA(name, "CMultipleDelayedActionsAction")) {
        fill_list(o, random() % 6, true);
        fl(o, 0x18) = pick_float();
    } else if (!lstrcmpA(name, "CSeriesAction") || !lstrcmpA(name, "CShuffledSeriesAction")) {
        const DWORD n = random() % 6;
        fill_list(o, n, false);
        dw(o, 0x18) = random() % 7 < n + 1 ? random() % (n + 1) : n + random() % 2;
        if (dw(o, 0x18) > 7)
            dw(o, 0x18) = 7;
        o[0x1c] = (BYTE)(random() % 2);
        dw(o, 0x20) = random() % 4;
        if (dw(o, 0x20) == 3 && dw(o, 0x18) >= n)
            dw(o, 0x20) = 0;   // other values run past the end of the list
    } else if (!lstrcmpA(name, "CCloneAction") || !lstrcmpA(name, "CCreateDefaultEntityAction") || !lstrcmpA(name, "CCreateCustomEntityAction")) {
        text(8);
        o[0xc] = (BYTE)(random() % 2);
        fl(o, 0x10) = random() % 2 ? 0 : pick_float();
        dw(o, 0x14) = random() % 2 ? ptrv(&g_fa[random() % 16]) : 0;
        if (!lstrcmpA(name, "CCloneAction")) {
            text(0x18);
            text(0x1c);
            fl(o, 0x20) = pick_float();
            o[0x24] = (BYTE)(random() % 2);
        } else if (!lstrcmpA(name, "CCreateDefaultEntityAction")) {
            dw(o, 0x18) = random() % 9;
            text(0x1c);
        } else
            dw(o, 0x18) = random() % 4 ? ptrv(g_fe[random() % 10]) : 0;
    } else if (!lstrcmpA(name, "CPlayAnimationAction")) {
        text(8);
        text(0xc);
        text(0x10);
        dw(o, 0x14) = random() % 5;
        fl(o, 0x18) = pick_float();
        o[0x1c] = (BYTE)(random() % 2);
    } else if (!lstrcmpA(name, "CSetCurSequenceAction") || !lstrcmpA(name, "CCheckCurSequenceAction")) {
        text(8);
        text(0xc);
    } else if (!lstrcmpA(name, "CPlaySoundAction")) {
        text(0xc);   // "Sound" (+8) is a resource, not a string
    } else if (class_def(name)->size >= 0xc) {
        text(8);   // the door actions: "Door Name"
        if (class_def(name)->size >= 0x10)
            fl(o, 0xc) = pick_float();
    }
}
DWORD string_mask(const char *name)
{
    if (!lstrcmpA(name, "CImpersonateAction"))
        return 0xc;
    if (!lstrcmpA(name, "CCloneAction"))
        return 0x4 | 0x40 | 0x80;
    if (!lstrcmpA(name, "CCreateDefaultEntityAction"))
        return 0x4 | 0x80;
    if (!lstrcmpA(name, "CCreateCustomEntityAction"))
        return 0x4;
    if (!lstrcmpA(name, "CPlayAnimationAction"))
        return 0x1c;
    if (!lstrcmpA(name, "CSetCurSequenceAction") || !lstrcmpA(name, "CCheckCurSequenceAction"))
        return 0xc;
    if (!lstrcmpA(name, "CPlaySoundAction"))
        return 0x8;
    const char *const named[] = {"CLockDoorAction", "CUnlockDoorAction", "COpenDoorAction", "CCloseDoorAction", "CToggleDoorAction",
                                 "CIsDoorOpenAction", "CIsDoorClosedAction", "CIsDoorLockedAction"};
    for (const char *n : named)
        if (!lstrcmpA(name, n))
            return 0x4;
    return 0;
}
bool has_list(const char *name)
{
    const char *const lists[] = {"CMultipleActionsAction", "CMultipleDelayedActionsAction", "CSeriesAction", "CShuffledSeriesAction",
                                 "CRandomAction", "CAndAction", "CXOrAction", "COrAction"};
    for (const char *n : lists)
        if (!lstrcmpA(name, n))
            return true;
    return false;
}

struct Dtor1 {
    const char *cls;
    DWORD address;
    bool array;
};
const Dtor1 DTORS[] = {
    {"CRemoveVelocityAction", 0x4b6360, true},  {"CDelayAction", 0x4b65d0, true},          {"COnlyOnceAction", 0x4b6ad0, true},
    {"COnlyOnceEveryXSecondsAction", 0x4b6de0, true}, {"CImpersonateAction", 0x4b7290, true}, {"CWhileAction", 0x4b79b0, true},
    {"CRepeatAction", 0x4b7d50, true},          {"CMultipleActionsAction", 0x4b8580, true}, {"CMultipleDelayedActionsAction", 0x4b8a20, true},
    {"CSeriesAction", 0x4b8fc0, true},          {"CShuffledSeriesAction", 0x4b9730, true}, {"CRandomAction", 0x4b9bb0, true},
    {"CAndAction", 0x4b9fc0, true},             {"CXOrAction", 0x4ba4e0, true},            {"COrAction", 0x4baa20, true},
    {"CSucceedAction", 0x4bae80, true},         {"CFailAction", 0x4bb100, true},           {"CNotAction", 0x4bb380, true},
    {"CCreateCustomEntityAction", 0x4bb5e0, false}, {"CCloneAction", 0x4bb710, true},      {"CCreateDefaultEntityAction", 0x4bc5e0, true},
    {"CCreateCustomEntityAction", 0x4bcad0, true}, {"CLockDoorAction", 0x4bd1b0, true},    {"CUnlockDoorAction", 0x4bd470, true},
    {"COpenDoorAction", 0x4bddd0, false},       {"COpenDoorAction", 0x4bded0, true},       {"CCloseDoorAction", 0x4be1f0, true},
    {"CToggleDoorAction", 0x4be510, true},      {"CPlayAnimationAction", 0x4be930, true},  {"CSetCurSequenceAction", 0x4bef80, true},
    {"CCheckCurSequenceAction", 0x4bf3d0, true}, {"CIsDoorOpenAction", 0x4bf740, true},    {"CIsDoorClosedAction", 0x4bfa00, true},
    {"CIsDoorLockedAction", 0x4bfcc0, true},    {"CPlaySoundAction", 0x4bff90, true},
};

// Deleting destructors: one object (flags 0 and 1) or an array of 0-3 (flags 2 and 3)
void test_dtors(Tally &t)
{
    for (const Dtor1 &d : DTORS) {
        const ActionClassDef *c = class_def(d.cls);
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
                        for (DWORD i = 0; i < count; i++)
                            fill_fields(d.cls, e + i * c->size);
                        obj = e;
                    } else {
                        obj = create(d.cls);
                        fill_fields(d.cls, obj);
                        g_nobj = 0;   // destroyed by the case
                    }
                },
                [&] {
                    void *r = at<Dtor>(d.address)(obj, flags);
                    return r == obj ? 1u : r == block ? 2u : ptrv(r);
                },
                [&] {
                    if (!(flags & 1)) {   // what the destructor left (strings by text, the list's base vtable ...)
                        if (flags & 2) {
                            for (DWORD i = 0; i < count; i++)
                                st(dw(obj + i * c->size, 0) ^ dw(obj + i * c->size, 0x10) * 0);
                            heap_free(block);
                        } else {
                            for (DWORD off = 0; off < c->size; off += 4)
                                st(string_mask(d.cls) >> (off / 4) & 1 ? 0 : tag(dw(obj, off)));
                            pool_free(obj, c->size);
                        }
                    }
                });
        }
    }
}

// Slot 23 of the classes on objects from the seed
struct Exec1 {
    const char *cls;
    DWORD address;
    DWORD mask;
};
const Exec1 EXECS[] = {
    {"CRemoveVelocityAction", 0x4b6440, 0xff},  {"CDelayAction", 0x4b6950, 0xff},             {"COnlyOnceAction", 0x4b6cb0, 0xff},
    {"COnlyOnceEveryXSecondsAction", 0x4b70f0, 0xff}, {"CImpersonateAction", 0x4b7550, 0xffffffff}, {"CWhileAction", 0x4b7bb0, 0xff},
    {"CRepeatAction", 0x4b7fe0, 0xffffffff},    {"CMultipleActionsAction", 0x4b87a0, 0xff},   {"CMultipleDelayedActionsAction", 0x4b8d00, 0xff},
    {"CSeriesAction", 0x4b9440, 0xff},          {"CShuffledSeriesAction", 0x4b9440, 0xff},    {"CRandomAction", 0x4b9d90, 0xffffffff},
    {"CAndAction", 0x4ba290, 0xff},             {"CXOrAction", 0x4ba7b0, 0xffffffff},         {"COrAction", 0x4bacf0, 0xff},
    {"CSucceedAction", 0x4baff0, 0xffffffff},   {"CFailAction", 0x4bb270, 0xffffffff},        {"CNotAction", 0x4bb4f0, 0xffffffff},
    {"CCloneAction", 0x4bc160, 0xffffffff},     {"CCreateDefaultEntityAction", 0x4bc160, 0xffffffff},
    {"CCreateCustomEntityAction", 0x4bc160, 0xffffffff},
    {"CLockDoorAction", 0x4bd600, 0xffffffff},  {"CUnlockDoorAction", 0x4bd600, 0xffffffff},  {"COpenDoorAction", 0x4bd830, 0xffffffff},
    {"CToggleDoorAction", 0x4bd830, 0xffffffff}, {"CPlayAnimationAction", 0x4bed90, 0xffffffff},
    {"CSetCurSequenceAction", 0x4bf200, 0xffffffff},
};

// The state after a slot: the object, its list, the queue of fakes
void state_action(const char *name, BYTE *o)
{
    state_obj(o, class_def(name)->size, string_mask(name));
    if (has_list(name))
        state_list(o);
}

void test_execs(Tally &t)
{
    for (const Exec1 &x : EXECS) {
        const int cases = scaled(60);
        for (int k = 0; k < cases; k++) {
            const Case cs{x.cls, x.address, 9000u + (unsigned)k * 17 + x.address % 991, x.mask};
            BYTE *o = nullptr;
            DWORD a2 = 0, a3 = 0, a4 = 0;
            run_case(
                t, cs,
                [&] {
                    o = create(x.cls);
                    fill_fields(x.cls, o);
                    a2 = pick_value();
                    a3 = pick_value();
                    a4 = pick_value();
                },
                [&] { return at<Slot23>(x.address)(o, g_layer[0], a2, a3, a4); }, [&] { state_action(x.cls, o); });
        }
    }
}

// Slot 27 (one target), slot 28 (make the part), the helpers
struct Target1 {
    const char *cls;
    DWORD address;
    DWORD mask;
    bool null_target;   // the slot tests for a null target
};
const Target1 TARGETS[] = {
    {"CLockDoorAction", 0x4bd340, 0xffffffff, false},   {"CUnlockDoorAction", 0x4bd630, 0xffffffff, false},
    {"COpenDoorAction", 0x4be040, 0xff, true},          {"CCloseDoorAction", 0x4be360, 0xff, true},
    {"CToggleDoorAction", 0x4be680, 0xff, true},  {"CIsDoorOpenAction", 0x4bf8d0, 0xff, true},
    {"CIsDoorClosedAction", 0x4bfb90, 0xff, true},      {"CIsDoorLockedAction", 0x4bfe50, 0xffffffff, false},
    {"CPlayAnimationAction", 0x4bedc0, 0xff, true},     {"CSetCurSequenceAction", 0x4bf230, 0xff, true},
    {"CCheckCurSequenceAction", 0x4bf5f0, 0xff, true},  {"CCreateCustomEntityAction", 0x4bc1c0, 0xff, false},
    {"CCloneAction", 0x4bc1c0, 0xff, false},            {"CCreateDefaultEntityAction", 0x4bc1c0, 0xff, false},
};
void test_targets(Tally &t)
{
    for (const Target1 &x : TARGETS) {
        const int cases = scaled(60);
        for (int k = 0; k < cases; k++) {
            const bool door = x.address == 0x4be040 || x.address == 0x4be360 || x.address == 0x4be680;
            const Case cs{x.cls, x.address, 11000u + (unsigned)k * 13 + x.address % 983, x.mask, door};
            BYTE *o = nullptr;
            DWORD a1 = 0, a2 = 0, a3 = 0;
            void *target = nullptr;
            run_case(
                t, cs,
                [&] {
                    o = create(x.cls);
                    fill_fields(x.cls, o);
                    a1 = pick_value();
                    a2 = pick_value();
                    a3 = pick_value();
                    target = g_fe[random() % 10];
                    if (x.null_target && random() % 6 == 0)
                        target = nullptr;
                },
                [&] { return at<Slot27>(x.address)(o, a1, a2, a3, target); }, [&] { state_action(x.cls, o); });
        }
    }
    const struct {
        const char *cls;
        DWORD address;
    } makes[] = {{"CCloneAction", 0x4bbcc0}, {"CCreateDefaultEntityAction", 0x4bc910}, {"CCreateCustomEntityAction", 0x4bcd30}};
    for (const auto &x : makes) {
        const int cases = scaled(60);
        for (int k = 0; k < cases; k++) {
            const Case cs{x.cls, x.address, 13000u + (unsigned)k * 19, 0xffffffff};
            BYTE *o = nullptr;
            DWORD a2 = 0, a3 = 0, a4 = 0;
            run_case(
                t, cs,
                [&] {
                    o = create(x.cls);
                    fill_fields(x.cls, o);
                    a2 = pick_value();
                    a3 = pick_value();
                    a4 = pick_value();
                },
                [&] { return tagp(at<Slot28>(x.address)(o, g_layer[1], a2, a3, a4)); }, [&] { state_action(x.cls, o); });
        }
    }
    // 0x4bc230: the part at a position or not, with a height
    const char *const creators[] = {"CCloneAction", "CCreateDefaultEntityAction", "CCreateCustomEntityAction"};
    for (int k = 0; k < scaled(180); k++) {
        const char *cls = creators[k % 3];
        const Case cs{cls, 0x4bc230, 15000u + (unsigned)k * 23, 0xff};
        BYTE *o = nullptr;
        DWORD a2 = 0, a3 = 0, a4 = 0;
        float pos[2];
        float *pp = nullptr;
        float height = 0;
        run_case(
            t, cs,
            [&] {
                o = create(cls);
                fill_fields(cls, o);
                a2 = pick_value();
                a3 = pick_value();
                a4 = pick_value();
                pos[0] = pick_float() * 100;
                pos[1] = pick_float() * 37;
                pp = random() % 2 ? pos : nullptr;
                height = pick_float();
            },
            [&] {
                return at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, float *, float)>(0x4bc230)(o, g_layer[1], a2, a3, a4, pp,
                                                                                                         height);
            },
            [&] {
                state_action(cls, o);
                st(bits(pos[0]) ^ bits(pos[1]) * 3);
            });
    }
}

// The switch and door helpers, the read line slot, the shuffle
void test_helpers(Tally &t)
{
    const DWORD switches[] = {0x4bcd90, 0x4bcde0, 0x4bce40};
    for (DWORD a : switches)
        for (int k = 0; k < scaled(60); k++) {
            const Case cs{"switch", a, 17000u + (unsigned)k * 29 + a % 977, 0xff};
            void *e = nullptr;
            run_case(
                t, cs, [&] { e = random() % 6 ? g_fe[random() % 10] : nullptr; },
                [&] { return at<DWORD(__fastcall *)(void *)>(a)(e); }, [] {});
        }
    const DWORD doors[] = {0x4bd870, 0x4bda50};
    for (DWORD a : doors)
        for (int k = 0; k < scaled(80); k++) {
            const Case cs{"door", a, 19000u + (unsigned)k * 37 + a % 971, 0xff, true};   // eax's upper bytes are the callees' 
            BYTE *o = nullptr;
            void *e = nullptr;
            run_case(
                t, cs,
                [&] {
                    o = create("COpenDoorAction");
                    fill_fields("COpenDoorAction", o);
                    e = g_fe[random() % 10];
                },
                [&] { return at<DWORD(THISCALL *)(void *, void *)>(a)(o, e); }, [&] { state_action("COpenDoorAction", o); });
        }
    const char *const names[] = {"Warp In", "warp in", "WARP IN", "Warp", "New Location", "Copy Height"};
    const char *const values[] = {"0", "1", "7", "abc", "-2", ""};
    const char *const creators[] = {"CCloneAction", "CCreateDefaultEntityAction", "CCreateCustomEntityAction"};
    for (int k = 0; k < scaled(72); k++) {
        const char *cls = creators[k % 3];
        const Case cs{cls, 0x4bc0f0, 21000u + (unsigned)k * 41, 0xffffffff};
        BYTE *o = nullptr;
        const char *name = names[k / 3 % 6], *value = values[k / 18 % 6];
        DWORD a3 = 0;
        run_case(
            t, cs,
            [&] {
                o = create(cls);
                fill_fields(cls, o);
                a3 = random();
            },
            [&] { return at<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4bc0f0)(o, name, value, a3); },
            [&] {
                const DWORD warp = dw(o, 0x14);
                state_action(cls, o);
                if (warp && (tag(warp) >> 24) == 0xf8)
                    dw(o, 0x14) = 0;   // the new warp behaviour is a stand-in's block, freed with the blocks
            });
    }
    for (int k = 0; k < scaled(60); k++) {
        const Case cs{"CShuffledSeriesAction", 0x4b9980, 23000u + (unsigned)k * 43, 0};
        BYTE *o = nullptr;
        run_case(
            t, cs,
            [&] {
                o = create("CShuffledSeriesAction");
                fill_fields("CShuffledSeriesAction", o);
            },
            [&] {
                at<void(THISCALL *)(void *)>(0x4b9980)(o);
                return 0u;
            },
            [&] { state_action("CShuffledSeriesAction", o); });
    }
}

// The editor's wrappers and CMultipleActionsAction's append
void test_wrappers(Tally &t)
{
    const char *const classes[] = {"CDelayAction", "CMultipleActionsAction", "CRandomAction", "CNotAction", "CSeriesAction",
                                   "CPlayAnimationAction", "COpenDoorAction", "CImpersonateAction"};
    const struct {
        DWORD address, wrapper;
    } describe[] = {{0x4b75e0, 0x6bfc28}, {0x4b77a0, 0x6bfd08}, {0x4b80f0, 0x6bfcc0}};
    for (const auto &w : describe)
        for (int k = 0; k < scaled(64); k++) {
            const char *cls = classes[k % 8];
            const Case cs{cls, w.address, 25000u + (unsigned)k * 47 + w.address % 967, 0xffffffff};
            BYTE *o = nullptr;
            CClassInfo *made = nullptr;
            bool out = false, fake = false;
            run_case(
                t, cs,
                [&] {
                    o = create(cls);
                    fill_fields(cls, o);
                    out = random() % 3 != 0;
                    fake = w.address != 0x4b77a0 && random() % 5 == 0;   // a fake CDelayAction has no next action at +0x10
                    made = nullptr;
                },
                [&] {
                    const char *r = at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(w.address)(
                        at<void *>(w.wrapper), fake ? static_cast<void *>(&g_fa[3]) : o, out ? &made : nullptr);
                    return r ? text_hash(r) ^ (r == zs_c_str_global() ? 1u : 0u) : 0u;
                },
                [&] {
                    st(ptrv(made));
                    state_action(cls, o);
                });
        }
    for (int k = 0; k < scaled(64); k++) {
        const Case cs{"wrap", 0x4b7720, 27000u + (unsigned)k * 53, 0xffffffff};
        BYTE *made = nullptr;
        void *child = nullptr;
        run_case(
            t, cs, [&] { child = &g_fa[random() % 16]; },
            [&] {
                made = static_cast<BYTE *>(at<void *(THISCALL *)(void *, void *)>(0x4b7720)(at<void *>(0x6bfc28), child));
                return dw(made, 0);
            },
            [&] {
                state_obj(made, 0x18, 0);
                dw(made, 0x10) = 0;
                original<Dtor>(0x4b65d0)(made, 1);
            });
    }
    for (int k = 0; k < scaled(64); k++) {
        const Case cs{"unwrap", 0x4b78a0, 29000u + (unsigned)k * 59, 0xffffffff};
        BYTE *o = nullptr;
        run_case(
            t, cs,
            [&] {
                o = create("CDelayAction");
                fill_fields("CDelayAction", o);
                g_nobj = 0;   // deleted by the case
            },
            [&] { return tagp(at<void *(THISCALL *)(void *, void *)>(0x4b78a0)(at<void *>(0x6bfd08), o)); }, [] {});
    }
    for (int k = 0; k < scaled(64); k++) {
        const bool add = k % 2;
        const Case cs{add ? "add" : "multi wrap", add ? 0x4b8800u : 0x4b8200u, 31000u + (unsigned)k * 61, 0xffffffff};
        BYTE *o = nullptr;
        void *child = nullptr;
        run_case(
            t, cs,
            [&] {
                child = random() % 4 ? &g_fa[random() % 16] : nullptr;
                o = nullptr;
                if (add) {
                    o = create("CMultipleActionsAction");
                    if (random() % 2)
                        fill_list(o, random() % 6, false);
                }
            },
            [&] {
                if (add) {
                    at<void(THISCALL *)(void *, void *)>(0x4b8800)(o, child);
                    return (DWORD)0;
                }
                o = static_cast<BYTE *>(at<void *(THISCALL *)(void *, void *)>(0x4b8200)(at<void *>(0x6bfcc0), child));
                if (g_nobj < 8)
                    g_obj[g_nobj++] = {o, 0x18};
                return dw(o, 0) ^ dw(o, 8);
            },
            [&] {
                st(dw(o, 0x10));
                const DWORD n = dw(o, 0x10);
                for (DWORD i = 0; i < n && i < 9; i++)
                    st(tag(reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(o, 0xc)))[i]));
            });
    }
}

// ---- Real data: the module's actions in the running level -------------------------------------------------

constexpr int MAX_SEEN = 16384, MAX_FOUND = 512;
DWORD g_seen[MAX_SEEN];
int g_nseen, g_objects;
BYTE *g_found[MAX_FOUND];
int g_nfound;
const ActionClassDef *module_class(const void *p)
{
    CClassInfo *c = class_of(p);
    if (!c)
        return nullptr;
    for (const Dtor1 &d : DTORS)
        if (d.array) {
            const ActionClassDef *def = class_def(d.cls);
            if (*at<CClassInfo **>(def->var) == c)
                return def;
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
    if (module_class(o) && g_nfound < MAX_FOUND)
        g_found[g_nfound++] = o;
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

// The owned child actions of a copy replaced by fakes (the copies of the real children deleted), so
// nothing of the game runs; owned entities and warp behaviours too
void replace_children(const char *name, BYTE *o)
{
    auto swap = [&](DWORD off) {
        if (void *a = vptr(dw(o, off))) {
            original<Dtor>(slot(a, 1))(a, 1);
            dw(o, off) = ptrv(&g_fa[(off / 4) % 16]);
        }
    };
    if (has_list(name)) {
        void **data = static_cast<void **>(vptr(dw(o, 0xc)));
        for (DWORD i = 0; data && i < dw(o, 0x10); i++)
            if (data[i]) {
                original<Dtor>(slot(data[i], 1))(data[i], 1);
                data[i] = &g_fa[i % 16];
            }
        return;
    }
    const struct {
        const char *cls;
        DWORD offs[2];
    } owned[] = {{"CDelayAction", {0x10, 0}},       {"COnlyOnceAction", {8, 0}},   {"COnlyOnceEveryXSecondsAction", {8, 0xc}},
                 {"CImpersonateAction", {0x10, 0}}, {"CWhileAction", {8, 0xc}},    {"CRepeatAction", {0xc, 0}},
                 {"CSucceedAction", {8, 0}},        {"CFailAction", {8, 0}},       {"CNotAction", {8, 0}},
                 {"CCloneAction", {0x14, 0}},       {"CCreateDefaultEntityAction", {0x14, 0}}, {"CCreateCustomEntityAction", {0x14, 0}}};
    for (const auto &w : owned)
        if (!lstrcmpA(w.cls, name))
            for (DWORD off : w.offs)
                if (off)
                    swap(off);
    if (!lstrcmpA(name, "CCreateCustomEntityAction") && dw(o, 0x18)) {
        void *e = vptr(dw(o, 0x18));
        original<Dtor>(slot(e, 1))(e, 1);
        dw(o, 0x18) = ptrv(g_fe[2]);
    }
}
DWORD exec_of(const char *name)
{
    for (const Exec1 &x : EXECS)
        if (!lstrcmpA(x.cls, name))
            return x.address;
    return 0;
}
DWORD target_of(const char *name)
{
    for (const Target1 &x : TARGETS)
        if (!lstrcmpA(x.cls, name))
            return x.address;
    return 0;
}

// The level's actions: the wrappers' texts; copies (children replaced by fakes) through slot 23 and 27
void test_level(bool found, Tally &t)
{
    if (!found) {
        log("  action slots real data: no level grid found");
        return;
    }
    log("  action slots real data: %d objects reached from the level's parts, %d of the module's classes", g_objects, g_nfound);
    const struct {
        DWORD address, wrapper;
    } describe[] = {{0x4b75e0, 0x6bfc28}, {0x4b77a0, 0x6bfd08}, {0x4b80f0, 0x6bfcc0}};
    for (int i = 0; i < g_nfound; i++) {
        BYTE *real = g_found[i];
        const ActionClassDef *def = module_class(real);
        if (!def)
            continue;
        for (const auto &w : describe) {
            const Case cs{def->name, w.address, 33000u + (unsigned)i, 0xffffffff};
            CClassInfo *made = nullptr;
            run_case(
                t, cs, [&] { made = nullptr; },
                [&] {
                    const char *r = at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(w.address)(at<void *>(w.wrapper), real, &made);
                    return r ? text_hash(r) : 0u;
                },
                [&] { st(ptrv(made)); });
        }
        const DWORD exec = exec_of(def->name), target = target_of(def->name);
        for (int round = 0; round < 2; round++) {
            const DWORD address = round ? target : exec;
            if (!address)
                continue;
            const Case cs{def->name, address, 35000u + (unsigned)i * 7 + (unsigned)round, 0xff};
            BYTE *copy = nullptr;
            DWORD a1 = 0, a2 = 0, a3 = 0;
            run_case(
                t, cs,
                [&] {
                    copy = static_cast<BYTE *>(at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(real, 0, 0xffffffff));
                    replace_children(def->name, copy);
                    if (g_nobj < 8)
                        g_obj[g_nobj++] = {copy, def->size};
                    a1 = pick_value();
                    a2 = pick_value();
                    a3 = pick_value();
                },
                [&] {
                    if (round)
                        return at<Slot27>(address)(copy, a1, a2, a3, g_fe[1]);
                    return at<Slot23>(address)(copy, g_layer[0], a1, a2, a3);
                },
                [&] { state_action(def->name, copy); });
        }
    }
}

}  // namespace

ZAX_CHECKS(action_slots)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally dtors{"action slots: deleting destructors"}, execs{"action slots: slot 23 (execute)"},
        targets{"action slots: slot 27, slot 28 and the part creation"}, helpers{"action slots: switch, door, read line, shuffle"},
        wrappers{"action slots: the editor's wrappers, append"}, level{"action slots: the level's actions (real data)"};
    init_vtables();
    at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_seqz, "Seq Walk");
    const bool found = find_level_actions();
    test_dtors(dtors);
    dtors.report();
    test_execs(execs);
    execs.report();
    test_targets(targets);
    targets.report();
    test_helpers(helpers);
    helpers.report();
    test_wrappers(wrappers);
    wrappers.report();
    test_level(found, level);
    level.report();
    at<void(THISCALL *)(void *)>(0x4defd0)(&g_seqz);
    log("  action slots: %u ms", GetTickCount() - start);
}

}  // namespace zax
