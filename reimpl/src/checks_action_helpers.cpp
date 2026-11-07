// Checks of the actions' helpers and small classes 0x418000-0x41ffff (action_helpers.cpp) against the
// original, in the game thread, with the pattern of checks_action_slots.cpp: every case runs twice from
// the same seed, side 0 with the hooks of action_helpers taken out, side 1 through the reimplementation;
// compared are eax, an event log of every call into fakes and stand-ins and the state afterwards.
//
// The name lookups run on a fake layer whose sort list and rectangle queries are stand-ins that add
// fake parts through the real part list functions (0x59a140; the temporary lists 0x59a470/0x59a510 are
// the real ones); a stamp stand-in (0x57aeb0) numbers the lists per side. The parts are fake entities
// (names through a stand-in for 0x4fbf20, slot 2 a real class), the reporter and the world fakes. The
// queue checks use real CActionQueue and CActionQueueItem objects (the original Create) with fake
// actions and parts (the listener functions are stand-ins); the conditional actions, the dialogue line,
// COtherMapAction and the trigger AIs are real objects (Create) with fake children.
//  - CAction's destructors and the execute wrappers; the name lookups 0x41a930, 0x41a760, 0x41a800,
//    0x41ad80/0x41ad40, the warning 0x41abd0, the per-target loop 0x41aed0/0x41ae90.
//  - The conditional execute 0x41bba0; the queue (add, update, item set, message, destructors).
//  - The deleting destructors of the classes, the trigger AIs' collisions and action setter, the AI
//    converting wrapper, the dialogue helpers, the property list's destructor, COtherMapAction's execute,
//    the field descriptor's alternative name over the registered fields (real data).
#include "check_util.h"
#include "action_classes.h"
#include "layer_parts.h"
#include "ptr_array.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_action_helpers(Hook *out);

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
DWORD g_calls;

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
DWORD text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }
DWORD text_hash_n(const char *s, DWORD n) { return s ? fnv(2166136261u, s, n) : 0; }
DWORD zs_hash(const void *z) { return z ? text_hash(static_cast<const ZString *>(z)->c_str()) : 0; }
DWORD mix(DWORD a, DWORD b, DWORD c = 0)
{
    DWORD h = 2166136261u;
    h = (h ^ a) * 16777619u;
    h = (h ^ b) * 16777619u;
    h = (h ^ c) * 16777619u;
    return h ^ (h >> 13);
}


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
    const int n = hooks_action_helpers(nullptr);
    if (!hook_sites_fit("action_helpers", n, MAX_SITES))
        return;
    hooks_action_helpers(list);
    for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (site[0] != 0xe9)
            continue;
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

constexpr DWORD CLS_ACTION = 0x41a650, CLS_BASE = 0x487620, CLS_ANIMATED = 0x48dcc0, CLS_MULTI = 0x4b8460;
CClassInfo *cls_at(DWORD getter) { return at<CClassInfo *(__cdecl *)()>(getter)(); }

struct FakeAction {
    DWORD vt, id, calls, limit, result;
    CClassInfo *cls;
    DWORD pad[2];
};
constexpr int NFA = 24;
alignas(16) FakeAction g_fa[NFA];
int g_next_fa;

constexpr int NFE = 12;
constexpr DWORD FE_SIZE = 0x100, FE_INDEX = 0xd0;
alignas(16) BYTE g_fe[NFE][FE_SIZE];
struct EntityCfg {
    CClassInfo *cls;
    const char *name;
    int target;    // slot 70: -1 none
};
EntityCfg g_cfg[NFE];
alignas(16) BYTE g_poly[NFE][0x10];
alignas(16) BYTE g_layer[0x200];
alignas(16) BYTE g_reporter[0x10];
alignas(16) BYTE g_world[0x400];
alignas(16) BYTE g_app[0x400];
alignas(16) BYTE g_app_list[0x20];
alignas(16) BYTE g_object[0x20];     // a fake object for 0x41d7f0
DWORD g_stamp;
DWORD g_overlap_mask;

DWORD g_vt_action[64], g_vt_entity[80], g_vt_reporter[32], g_vt_world[32], g_vt_app[32], g_vt_list[8], g_vt_object[8];

const char *const PART_NAMES[] = {"Door 1", "Door 2", "Door 3", "Switch", "", "Gate", nullptr, "Door10", "door 1", "Lever"};

struct Obj {
    BYTE *p;
    DWORD size;
};
Obj g_obj[8];
int g_nobj;
void note_obj(BYTE *o, DWORD size)
{
    if (g_nobj < 8)
        g_obj[g_nobj++] = {o, size};
}

// Objects the code makes during a case (the queue's items), named by the order they first appear in
constexpr int MAX_MADE = 32;
DWORD g_made[MAX_MADE];
int g_nmade;
DWORD tag_made(DWORD v)
{
    if (!v)
        return 0;
    for (int i = 0; i < g_nmade; i++)
        if (g_made[i] == v)
            return 0xfc000000u + (DWORD)i;
    if (g_nmade < MAX_MADE)
        g_made[g_nmade++] = v;
    return 0xfc000000u + (DWORD)(g_nmade - 1);
}

DWORD tag(DWORD v)
{
    if (!v)
        return 0;
    for (int i = 0; i < g_nmade; i++)
        if (g_made[i] == v)
            return 0xfc000000u + (DWORD)i;
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    if (in(g_fa, sizeof g_fa))
        return 0xf1000000u + (v - ptrv(g_fa));
    if (in(g_fe, sizeof g_fe))
        return 0xf2000000u + (v - ptrv(g_fe));
    if (in(g_poly, sizeof g_poly))
        return 0xf3000000u + (v - ptrv(g_poly));
    if (in(g_layer, sizeof g_layer))
        return 0xf5000000u + (v - ptrv(g_layer));
    for (int i = 0; i < g_nobj; i++)
        if (in(g_obj[i].p, g_obj[i].size))
            return 0xf7000000u + ((DWORD)i << 12) + (v - ptrv(g_obj[i].p));
    if (in(g_world, sizeof g_world))
        return 0xf9000000u + (v - ptrv(g_world));
    if (in(g_app, sizeof g_app))
        return 0xfa000000u + (v - ptrv(g_app));
    if (in(g_object, sizeof g_object))
        return 0xfb000000u + (v - ptrv(g_object));
    return v;
}
DWORD tagp(const void *p) { return tag(ptrv(p)); }

int fake_index(const BYTE *e)
{
    if (ptrv(e) < ptrv(g_fe) || ptrv(e) >= ptrv(g_fe) + sizeof g_fe)
        return 0;
    return (int)((ptrv(e) - ptrv(g_fe)) / FE_SIZE);
}
EntityCfg &cfg_of(const BYTE *e) { return g_cfg[fake_index(e)]; }

// -- fake actions
void *THISCALL fa_delete(FakeAction *a, DWORD flags)
{
    ev(0xa001, tagp(a), flags);
    return a;
}
CClassInfo *THISCALL fa_class(FakeAction *a) { return a->cls; }
DWORD THISCALL fa_execute(FakeAction *a, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    ev(0xa017, tagp(a), mix(tagp(layer), tag(a2), mix(tag(a3), tag(a4))), a->calls);
    const DWORD r = a->calls < a->limit ? a->result : (a->result & 0xffffff00);
    a->calls++;
    return r;
}
DWORD THISCALL fa_target(FakeAction *a, DWORD a1, DWORD a2, DWORD a3, void *t)
{
    ev(0xa01b, tagp(a), mix(tag(a1), tag(a2), tag(a3)), tagp(t));
    return (mix(tagp(t), a->calls++) & 0xffffff00) | (a->result & 0xff);
}
template <int S> void THISCALL fa_trap(FakeAction *a) { ev(0xa0ff, tagp(a), S); }

// -- fake entities
CClassInfo *THISCALL fe_class(BYTE *e) { return cfg_of(e).cls; }
void THISCALL fe_rect(BYTE *e, BYTE *rect, DWORD a, DWORD b)
{
    ev(0xe025, tagp(e), a, b);
    dw(rect, 0) = fake_index(e) * 10;
    dw(rect, 4) = 7;
}
void THISCALL fe_position(BYTE *e, float *out)
{
    ev(0xe02b, tagp(e));
    out[0] = (float)(fake_index(e) * 64);
    out[1] = (float)(fake_index(e) * 32 + 5);
}
void *THISCALL fe_poly(BYTE *e)
{
    ev(0xe030, tagp(e));
    return g_poly[fake_index(e)];
}
void THISCALL fe_position50(BYTE *e, float *out)
{
    ev(0xe032, tagp(e));
    out[0] = (float)(fake_index(e) * 16 + 1);
    out[1] = 3.5f;
}
void *THISCALL fe_target(BYTE *e)
{
    ev(0xe046, tagp(e));
    const int t = cfg_of(e).target;
    return t < 0 ? nullptr : g_fe[t];
}
template <int S> void THISCALL fe_trap(BYTE *e) { ev(0xe0ff, tagp(e), S); }

// -- reporter, world, application
float THISCALL rep_time(void *)
{
    ev(0xb00d);
    return (float)(int)(g_calls % 50) * 0.5f + 10.0f;
}
void THISCALL rep_message(void *, const char *text) { ev(0xb011, text_hash(text)); }
template <int S> void THISCALL rep_trap(void *) { ev(0xb0ff, S); }
void THISCALL world_message(void *, const char *text) { ev(0xc011, text_hash(text)); }
template <int S> void THISCALL world_trap(void *) { ev(0xc0ff, S); }
void THISCALL app_message(void *, const char *text) { ev(0xd011, text_hash(text)); }
template <int S> void THISCALL app_trap(void *) { ev(0xd0ff, S); }
void THISCALL list_add(void *, void **a) { ev(0xd102, tagp(*a)); }
template <int S> void THISCALL list_trap(void *) { ev(0xd1ff, S); }
void THISCALL object_slot(void *o, DWORD v) { ev(0xd202, tagp(o), v, g_calls); }
template <int S> void THISCALL object_trap(void *) { ev(0xd2ff, S); }

template <int... S> struct Seq {};
template <int N, int... S> struct MakeSeq : MakeSeq<N - 1, N - 1, S...> {};
template <int... S> struct MakeSeq<0, S...> {
    using type = Seq<S...>;
};
template <int... S> void fill_traps(DWORD *vt, Seq<S...>, int kind)
{
    switch (kind) {
    case 0:
        ((vt[S] = ptrv((void *)&fa_trap<S>)), ...);
        break;
    case 1:
        ((vt[S] = ptrv((void *)&fe_trap<S>)), ...);
        break;
    case 2:
        ((vt[S] = ptrv((void *)&rep_trap<S>)), ...);
        break;
    case 3:
        ((vt[S] = ptrv((void *)&world_trap<S>)), ...);
        break;
    case 4:
        ((vt[S] = ptrv((void *)&app_trap<S>)), ...);
        break;
    case 5:
        ((vt[S] = ptrv((void *)&list_trap<S>)), ...);
        break;
    default:
        ((vt[S] = ptrv((void *)&object_trap<S>)), ...);
        break;
    }
}

void init_vtables()
{
    fill_traps(g_vt_action, typename MakeSeq<64>::type{}, 0);
    fill_traps(g_vt_entity, typename MakeSeq<80>::type{}, 1);
    fill_traps(g_vt_reporter, typename MakeSeq<32>::type{}, 2);
    fill_traps(g_vt_world, typename MakeSeq<32>::type{}, 3);
    fill_traps(g_vt_app, typename MakeSeq<32>::type{}, 4);
    fill_traps(g_vt_list, typename MakeSeq<8>::type{}, 5);
    fill_traps(g_vt_object, typename MakeSeq<8>::type{}, 6);
    g_vt_action[1] = ptrv((void *)&fa_delete);
    g_vt_action[2] = ptrv((void *)&fa_class);
    g_vt_action[23] = ptrv((void *)&fa_execute);
    g_vt_action[27] = ptrv((void *)&fa_target);
    g_vt_entity[2] = ptrv((void *)&fe_class);
    g_vt_entity[37] = ptrv((void *)&fe_rect);
    g_vt_entity[43] = ptrv((void *)&fe_position);
    g_vt_entity[48] = ptrv((void *)&fe_poly);
    g_vt_entity[50] = ptrv((void *)&fe_position50);
    g_vt_entity[70] = ptrv((void *)&fe_target);
    g_vt_reporter[13] = ptrv((void *)&rep_time);
    g_vt_reporter[17] = ptrv((void *)&rep_message);
    g_vt_world[17] = ptrv((void *)&world_message);
    g_vt_app[17] = ptrv((void *)&app_message);
    g_vt_list[2] = ptrv((void *)&list_add);
    g_vt_object[2] = ptrv((void *)&object_slot);
    g_vt_object[3] = ptrv((void *)&object_slot);
    dw(g_reporter, 0) = ptrv(g_vt_reporter);
}

FakeAction *next_fake_action() { return &g_fa[g_next_fa < NFA ? g_next_fa++ : NFA - 1]; }

void setup_fakes(unsigned seed)
{
    g_seed = seed * 2654435761u + 12345;
    CClassInfo *const action_classes[] = {cls_at(CLS_ACTION), cls_at(CLS_ACTION), cls_at(CLS_MULTI)};
    const DWORD results[] = {0, 1, 1, 2, 0x101, 0x100, 0xffffff01, 0x7fffff00};
    for (int i = 0; i < NFA; i++)
        g_fa[i] = {ptrv(g_vt_action), (DWORD)i + 1, 0, random() % 5, results[random() % 8], action_classes[random() % 3], {0, 0}};
    g_next_fa = 16;
    CClassInfo *const entity_classes[] = {cls_at(CLS_ANIMATED), cls_at(CLS_BASE), cls_at(CLS_ACTION), cls_at(CLS_ANIMATED)};
    for (int i = 0; i < NFE; i++) {
        memset(g_fe[i], 0, FE_SIZE);
        dw(g_fe[i], 0) = ptrv(g_vt_entity);
        dw(g_fe[i], FE_INDEX) = 0xfe000000u + (DWORD)i;
        g_cfg[i].cls = entity_classes[random() % 4];
        g_cfg[i].name = PART_NAMES[random() % 10];
        g_cfg[i].target = random() % 3 ? (int)(random() % NFE) : -1;
    }
    memset(g_layer, 0, sizeof g_layer);
    memset(g_world, 0, sizeof g_world);
    dw(g_world, 0) = ptrv(g_vt_world);
    memset(g_app, 0, sizeof g_app);
    dw(g_app, 0) = ptrv(g_vt_app);
    dw(g_app, 0x354) = ptrv(g_app_list);
    memset(g_app_list, 0, sizeof g_app_list);
    dw(g_app_list, 8) = ptrv(g_vt_list);
    memset(g_object, 0, sizeof g_object);
    dw(g_object, 0) = ptrv(g_vt_object);
    g_stamp = 100;
    g_overlap_mask = random();
}

// ---- Stand-ins -----------------------------------------------------------------------------------------------------

void part_list_add(void *list, void *p) { at<DWORD(THISCALL *)(void *, void *)>(0x59a140)(list, p); }

void THISCALL sr_new_stamp(void *sort, BYTE *list)
{
    ev(0x57aeb0, tagp(sort));
    dw(list, 0x10) = ++g_stamp;
}
void THISCALL sr_query_name(void *sort, void *list, const char *text, DWORD len, DWORD a, DWORD b, DWORD c, DWORD d)
{
    ev(0x57a7e0, tagp(sort), text_hash_n(text, len) ^ len, a ^ b * 3 ^ c * 5 ^ d * 7);
    // the parts whose name starts with the text (case ignored), as the sort list would find them
    for (int i = 0; i < NFE; i++)
        if (g_cfg[i].name && len && at<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(g_cfg[i].name, text, len) == 0 &&
            !g_cfg[i].name[len])
            part_list_add(list, g_fe[i]);
}
void THISCALL sr_find_rect(void *layer, BYTE *list, BYTE *rect, DWORD a, DWORD b, DWORD c, DWORD d)
{
    ev(0x4ed5e0, tagp(layer), dw(rect, 0) ^ dw(rect, 4) * 3, a ^ b * 3 ^ c * 5 ^ d * 7);
    at<void(THISCALL *)(void *)>(0x59a120)(list);   // as the real query: a fresh list with its own stamp
    dw(list, 0x10) = ++g_stamp;
    for (int i = 0; i < NFE; i++)
        if (mix(i, dw(rect, 0)) % 3 == 0)
            part_list_add(list, g_fe[i]);
}
DWORD THISCALL sr_overlaps(void *poly, float *p1, void *poly2, float *p2)
{
    ev(0x547d60, tagp(poly) ^ tagp(poly2), bits(p1[0]) ^ bits(p1[1]) * 3, bits(p2[0]) ^ bits(p2[1]) * 5);
    const DWORD i = (ptrv(poly2) - ptrv(g_poly)) / 0x10;
    return 0x5400 | (g_overlap_mask >> (i & 31) & 1);
}
const char *THISCALL sr_part_name(BYTE *p)
{
    ev(0x4fbf20, tagp(p));
    return cfg_of(p).name;
}
void THISCALL sr_queue(void *queue, float delay, DWORD a4, DWORD a2, DWORD a3, DWORD b, void *action)
{
    ev(0x41c7a0, tagp(queue) ^ bits(delay), mix(tag(a4), tag(a2), tag(a3)) ^ tag(b), tagp(action));
}
void *THISCALL sr_clone(void *o, DWORD a, DWORD b)
{
    ev(0x416760, tagp(o), a, b);
    FakeAction *c = next_fake_action();
    if ((tagp(o) >> 24) == 0xf1)
        c->result = static_cast<FakeAction *>(o)->result;
    return c;
}
DWORD THISCALL sr_listen(void *p, void *who, int a, const char *t)
{
    ev(0x4fc1b0, tagp(p), tag_made(ptrv(who)), (DWORD)a ^ text_hash(t));
    return 0x4fc100 + (g_calls & 0xff);
}
DWORD THISCALL sr_unlisten(void *p, void *who, int a, const char *t)
{
    ev(0x4fc200, tagp(p), tag_made(ptrv(who)), (DWORD)a ^ text_hash(t));
    return 0x4fc200 + (g_calls & 0xff);
}
DWORD THISCALL sr_ai_listen(void *p, void *who, int a, const char *t)
{
    ev(0x4fd190, tagp(p), tag_made(ptrv(who)), (DWORD)a ^ text_hash(t));
    return 0x4fd100 + (g_calls & 0xff);
}
DWORD THISCALL sr_ai_unlisten(void *p, void *who, int a, const char *t)
{
    ev(0x4fd1e0, tagp(p), tag_made(ptrv(who)), (DWORD)a ^ text_hash(t));
    return 0x4fd200 + (g_calls & 0xff);
}
DWORD THISCALL sr_allows(void *ai, void *part, DWORD x)
{
    ev(0x422d50, tagp(ai), tagp(part), tag(x));
    return 0x4200 | (mix(tagp(part), g_calls) & 1);
}
DWORD THISCALL sr_fire(void *ai, void *layer, void **action, void *part, DWORD x)
{
    ev(0x422cd0, tagp(ai) ^ tagp(layer), tagp(action) ^ tagp(*action), tagp(part) ^ tag(x));
    return 0x4201;
}
DWORD THISCALL sr_level_exists(void *w, const char *name)
{
    ev(0x4f4ca0, tagp(w), text_hash(name));
    return text_hash(name) & 1 ? 0x101 : 0x100;
}

struct Fn {
    DWORD address;
    void *standin;
};
const Fn FNS[] = {
    {0x57aeb0, (void *)&sr_new_stamp},  {0x57a7e0, (void *)&sr_query_name},  {0x4ed5e0, (void *)&sr_find_rect},
    {0x547d60, (void *)&sr_overlaps},   {0x4fbf20, (void *)&sr_part_name},   {0x41c7a0, (void *)&sr_queue},
    {0x416760, (void *)&sr_clone},      {0x4fc1b0, (void *)&sr_listen},      {0x4fc200, (void *)&sr_unlisten},
    {0x4fd190, (void *)&sr_ai_listen},  {0x4fd1e0, (void *)&sr_ai_unlisten}, {0x422d50, (void *)&sr_allows},
    {0x422cd0, (void *)&sr_fire},       {0x4f4ca0, (void *)&sr_level_exists},
};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
CodePatch g_patches[NFN];
bool g_patch[NFN];
DWORD g_saved_reporter, g_saved_world, g_saved_app;
DWORD g_skip;   // a bit per stand-in index that stays the real function (the queue checks: the real queue)
void standins_on()
{
    for (int i = 0; i < NFN; i++) {
        g_patch[i] = !(g_skip >> i & 1);
        if (g_patch[i])
            patch_jump(g_patches[i], FNS[i].address, FNS[i].standin);
    }
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c02cc) = ptrv(g_reporter);
    g_saved_world = *at<DWORD *>(0x6c2080);
    *at<DWORD *>(0x6c2080) = ptrv(g_world);
    g_saved_app = *at<DWORD *>(0x713f14);
    *at<DWORD *>(0x713f14) = ptrv(g_app);
}
void standins_off()
{
    *at<DWORD *>(0x713f14) = g_saved_app;
    *at<DWORD *>(0x6c2080) = g_saved_world;
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
    for (int i = NFN; i-- > 0;)
        if (g_patch[i])
            unpatch(g_patches[i]);
}

// ---- Objects ---------------------------------------------------------------------------------------------------

BYTE *create_at(DWORD create, DWORD size)
{
    BYTE *o = static_cast<BYTE *>(original<void *(__cdecl *)()>(create)());
    note_obj(o, size);
    return o;
}
const ActionClassDef *class_def(const char *name)
{
    for (const ActionClassDef &c : ACTION_CLASSES)
        if (lstrcmpA(c.name, name) == 0)
            return &c;
    return nullptr;
}
BYTE *create(const char *name) { return create_at(class_def(name)->create, class_def(name)->size); }
void set_text(BYTE *o, DWORD off, const char *s) { at<ZString *(THISCALL *)(void *, const char *)>(0x4e1930)(o + off, s); }
DWORD pick_part() { return random() % 4 == 0 ? 0 : ptrv(g_fe[random() % NFE]); }
DWORD pick_child() { return random() % 5 == 0 ? 0 : ptrv(&g_fa[random() % 16]); }

void state_obj(const BYTE *o, DWORD size, DWORD strings)
{
    for (DWORD off = 0; off < size; off += 4) {
        if (strings >> (off / 4) & 1)
            st(zs_hash(o + off));
        else
            st(tag(*reinterpret_cast<const DWORD *>(o + off)));
    }
}
void state_fakes()
{
    for (int i = 0; i < NFA; i++)
        st(g_fa[i].calls);
    for (int i = 0; i < NFE; i++)
        st(dw(g_fe[i], 0x2c));   // the part lists' stamps
}
void cleanup()
{
    for (int i = 0; i < g_nobj; i++)
        if (g_obj[i].p)
            original<void *(THISCALL *)(void *, DWORD)>(slot(g_obj[i].p, 1))(g_obj[i].p, 1);
    g_nobj = 0;
}

// A part list: the real temporary lists, its parts as state and given back
struct PartList {
    DWORD vtable;
    void **data;
    DWORD count, capacity, stamp;
};
PartList *temp_list()
{
    PartList *l;
    at<void(THISCALL *)(PartList **)>(0x59a470)(&l);
    return l;
}
void release_list(PartList *&l) { at<DWORD(THISCALL *)(PartList **)>(0x59a510)(&l); }
void state_list(PartList *l)
{
    st(l->count);
    st(l->stamp);
    for (DWORD i = 0; i < l->count && i < 32; i++)
        st(tagp(l->data[i]));
}

// A queue item: its fields; the class ids (+0x24 .. +0x30) only for the references they were set for, the
// padding +0x20 left out
void state_item(BYTE *item)
{
    for (DWORD off = 8; off <= 0x1c; off += 4)
        st(tag(dw(item, off)));
    const DWORD refs[] = {0xc, 0x10, 0x18, 0x14};
    for (int i = 0; i < 4; i++)
        st(dw(item, refs[i]) ? dw(item, 0x24 + 4 * (DWORD)i) : 0);
}

// ---- Both sides of a case ---------------------------------------------------------------------------------------

struct Case {
    const char *name;
    DWORD address;
    unsigned seed;
    DWORD ret_mask;
};
DWORD g_ret[2];

template <typename B, typename R, typename S> void side(int s, const Case &c, B build, R run, S state)
{
    g_side = s;
    g_loghash[s] = g_statehash[s] = 2166136261u;
    g_nlog[s] = g_nslog[s] = 0;
    g_calls = 0;
    g_nobj = 0;
    g_skip = 0;
    g_nmade = 0;
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

using Dtor = void *(THISCALL *)(void *, DWORD);

// ---- The name lookups ------------------------------------------------------------------------------------------

const char *const NAMES[] = {"$touching", "$Touching", "$t",        "$trigger",      "$Trigger",       "$instigator",     "$Instig",
                             "Door 1",    "door 1",    "Door 1, Gate", "Door 1,,Gate ,  Lever", "$target(Door 1))", "$target(Gate,Door 2))",
                             "$target()", "Door#1-3#", "Door#3-1#", "Door #1-3#", "Door#2-2#",   "Door#1#",        "Door#1-x",        "",
                             "Lever",     "Nothing",   "Do",        "Door#1-3#0", ", Gate",     "Gate,",           "$trigger,$instigator"};
constexpr int NNAMES = sizeof NAMES / sizeof NAMES[0];

void test_names(Tally &t)
{
    // 0x41a930 with every name and several lengths, 0x41a760 with every list, 0x41a800 with the ranges
    for (int k = 0; k < scaled(NNAMES * 6); k++) {
        const char *text = NAMES[k % NNAMES];
        const int mode = k / NNAMES % 3;   // 0: 0x41a930 full length, 1: 0x41a930 a prefix, 2: 0x41a760
        const DWORD address = mode == 2 ? 0x41a760 : 0x41a930;
        const Case cs{text, address, 61000u + (unsigned)k * 3, 0};
        PartList *list = nullptr;
        DWORD trigger = 0, instigator = 0, len = 0;
        run_case(
            t, cs,
            [&] {
                list = temp_list();
                at<void(THISCALL *)(void *)>(0x59a120)(list);   // the temporary lists are given out as they were left
                trigger = pick_part();
                instigator = pick_part();
                const DWORD n = (DWORD)lstrlenA(text);
                len = mode == 1 && n ? 1 + random() % n : n;
            },
            [&] {
                if (mode == 2)
                    at<void(__fastcall *)(void *, const char *, DWORD, DWORD, void *)>(0x41a760)(g_layer, text, trigger, instigator, list);
                else
                    at<void(__fastcall *)(void *, const char *, DWORD, DWORD, DWORD, void *)>(0x41a930)(g_layer, text, len, trigger, instigator,
                                                                                                    list);
                return DWORD(0);
            },
            [&] {
                state_list(list);
                release_list(list);
            });
    }
    for (int k = 0; k < scaled(64); k++) {
        const char *const ranges[] = {"Door#1-3#", "Door#3-1#", "Door#2-2#", "Door#1#", "Door#1-x", "#0-9#", "Door#10-8#x", "Do#1-3#or"};
        const char *text = ranges[k % 8];
        const Case cs{text, 0x41a800, 63000u + (unsigned)k * 5, 0};
        PartList *list = nullptr;
        DWORD len = 0;
        run_case(
            t, cs,
            [&] {
                list = temp_list();
                at<void(THISCALL *)(void *)>(0x59a120)(list);
                const DWORD n = (DWORD)lstrlenA(text);
                len = random() % 3 ? n : 1 + random() % n;
            },
            [&] {
                at<void(__fastcall *)(void *, const char *, DWORD, void *)>(0x41a800)(g_layer, text, len, list);
                return DWORD(0);
            },
            [&] {
                state_list(list);
                release_list(list);
            });
    }
    // 0x41ad80 / 0x41ad40: one part, with and without a class
    for (int k = 0; k < scaled(NNAMES * 4); k++) {
        const char *text = NAMES[k % NNAMES];
        const bool z = k / NNAMES % 2;
        const Case cs{text, z ? 0x41ad40u : 0x41ad80u, 65000u + (unsigned)k * 7, 0xffffffff};
        DWORD a2 = 0, a3 = 0, a4 = 0, report = 0;
        CClassInfo *cls = nullptr;
        BYTE *caller = nullptr;
        ZString name;
        run_case(
            t, cs,
            [&] {
                a2 = pick_part();
                a3 = random() % 2 ? ptrv(&g_fa[random() % 16]) : 0;
                a4 = pick_part();
                report = random() % 3 ? 1 : random() % 2 ? 0 : 0x100;
                CClassInfo *const classes[] = {nullptr, cls_at(CLS_ANIMATED), cls_at(CLS_BASE), cls_at(CLS_ACTION)};
                cls = classes[random() % 4];
                caller = random() % 3 ? reinterpret_cast<BYTE *>(&g_fa[random() % 16]) : nullptr;
                at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&name, text);
            },
            [&] {
                if (z)
                    return tag(at<DWORD(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD, CClassInfo *)>(0x41ad40)(
                        g_layer, &name, a2, a4, a3, caller, report, cls));
                return tag(at<DWORD(__fastcall *)(void *, const char *, DWORD, DWORD, DWORD, void *, DWORD, CClassInfo *)>(0x41ad80)(
                    g_layer, k % 11 == 5 ? nullptr : text, a2, a4, a3, caller, report, cls));
            },
            [&] { at<void(THISCALL *)(void *)>(0x4defd0)(&name); });
    }
    // 0x41abd0 directly
    for (int k = 0; k < scaled(120); k++) {
        const Case cs{"report", 0x41abd0, 67000u + (unsigned)k * 11, 0};
        const char *text = nullptr, *name = nullptr;
        DWORD part = 0, trigger = 0, object = 0, caller = 0, report = 0;
        run_case(
            t, cs,
            [&] {
                const char *const texts[] = {"A warning", "", "x"};
                text = texts[random() % 3];
                name = random() % 4 ? NAMES[random() % NNAMES] : nullptr;
                part = pick_part();
                trigger = pick_part();
                object = random() % 2 ? ptrv(&g_fa[random() % 16]) : pick_part();
                caller = random() % 2 ? ptrv(&g_fa[random() % 16]) : 0;
                report = random() % 4 ? 1 : random() % 2 ? 0 : 0x300;
            },
            [&] {
                at<void(__fastcall *)(const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x41abd0)(text, name, part, trigger,
                                                                                                             object, caller, report);
                return DWORD(0);
            },
            [] {});
    }
    // 0x41aed0 / 0x41ae90: the per-target loop on a fake action
    for (int k = 0; k < scaled(NNAMES * 6); k++) {
        const char *text = NAMES[k % NNAMES];
        const bool z = k / NNAMES % 2;
        const Case cs{text, z ? 0x41ae90u : 0x41aed0u, 69000u + (unsigned)k * 13, 0xffffffff};
        DWORD a2 = 0, a3 = 0, a4 = 0;
        float delay = 0;
        CClassInfo *cls = nullptr;
        FakeAction *self = nullptr;
        ZString name;
        run_case(
            t, cs,
            [&] {
                a2 = pick_part();
                a3 = random() % 2 ? ptrv(&g_fa[random() % 16]) : 0;
                a4 = pick_part();
                const float delays[] = {0, 0.001f, 0.0011f, 0.5f, 2, -1};
                delay = delays[random() % 6];
                CClassInfo *const classes[] = {nullptr, cls_at(CLS_ANIMATED), cls_at(CLS_BASE), cls_at(CLS_ACTION)};
                cls = classes[random() % 4];
                self = &g_fa[random() % 16];
                at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&name, text);
            },
            [&] {
                if (z)
                    return at<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD, float, CClassInfo *)>(0x41ae90)(
                        self, g_layer, &name, a2, a3, a4, delay, cls);
                const char *p = k % 13 == 7 ? nullptr : name.c_str();
                const DWORD r = at<DWORD(THISCALL *)(void *, void *, const char *, DWORD, DWORD, DWORD, float, CClassInfo *)>(0x41aed0)(
                    self, g_layer, p, a2, a3, a4, delay, cls);
                // without a name eax's upper bytes are the caller's (through 0x41ae90 the name's address)
                return !p || !*p ? r & 0xff : r;
            },
            [&] { at<void(THISCALL *)(void *)>(0x4defd0)(&name); });
    }
}

// ---- CAction, the conditional actions, the queue ---------------------------------------------------------------

void test_actions(Tally &t)
{
    // the execute wrappers on fake actions
    for (int k = 0; k < scaled(40); k++) {
        const bool from = k % 2;
        const Case cs{"execute", from ? 0x41a740u : 0x41a720u, 71000u + (unsigned)k * 3, 0xffffffff};
        FakeAction *a = nullptr;
        DWORD a2 = 0, a3 = 0, a4 = 0;
        run_case(
            t, cs,
            [&] {
                a = &g_fa[random() % 16];
                a2 = pick_part();
                a3 = pick_part();
                a4 = pick_part();
            },
            [&] {
                if (from)
                    return at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *)>(0x41a740)(a, g_layer, a2, a3, a4, &g_fa[20]);
                return at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x41a720)(a, g_layer, a2, a3, a4, &g_fa[20], 7);
            },
            [] {});
    }
    // CAction's destructors on a CRemoveVelocityAction (no members of its own)
    for (int k = 0; k < scaled(24); k++) {
        const bool deleting = k % 2;
        const Case cs{"CAction", deleting ? 0x41a6d0u : 0x41a700u, 72000u + (unsigned)k, 0xffffffff};
        BYTE *o = nullptr;
        DWORD flags = 0;
        run_case(
            t, cs,
            [&] {
                o = create("CRemoveVelocityAction");
                g_nobj = 0;
                flags = random() % 2;
            },
            [&] {
                if (deleting)
                    return at<Dtor>(0x41a6d0)(o, flags) == o ? DWORD(1) : DWORD(2);
                at<void(THISCALL *)(void *)>(0x41a700)(o);
                return DWORD(0);
            },
            [&] {
                if (!deleting || !(flags & 1)) {
                    st(dw(o, 0));
                    pool_free(o, 8);
                }
            });
    }
    // 0x41bba0 on CConditionalAction and CIfAction
    for (int k = 0; k < scaled(120); k++) {
        const char *cls = k % 2 ? "CIfAction" : "CConditionalAction";
        const Case cs{cls, 0x41bba0, 73000u + (unsigned)k * 7, 0xffffffff};
        BYTE *o = nullptr;
        DWORD a2 = 0, a3 = 0, a4 = 0;
        run_case(
            t, cs,
            [&] {
                o = create(cls);
                for (DWORD off = 8; off <= 0x10; off += 4)
                    dw(o, off) = pick_child();
                a2 = pick_part();
                a3 = pick_part();
                a4 = pick_part();
            },
            [&] {
                const DWORD r = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x41bba0)(o, g_layer, a2, a3, a4);
                return dw(o, 8) ? r : r & 0xff;
            },
            [&] { state_obj(o, 0x14, 0); });
    }
    // the queue: add (0x41c7a0, the item's set 0x41c120 inside), then the frame (0x41c700)
    for (int k = 0; k < scaled(120); k++) {
        const int what = k % 3;   // 0 add, 1 update, 2 item message
        const Case cs{"CActionQueue", what == 0 ? 0x41c7a0u : what == 1 ? 0x41c700u : 0x41c230u, 75000u + (unsigned)k * 9, 0xffffffff};
        BYTE *q = nullptr;
        float dt = 0;
        DWORD v[6];
        run_case(
            t, cs,
            [&] {
                q = create("CActionQueue");
                dw(q, 0x18) = ptrv(g_layer);
                g_skip = 1 << 5;   // the real queue
                // a few items added through the original (the listener stand-ins on)
                standins_on();
                const int items = (int)(random() % 5);
                for (int i = 0; i < items; i++) {
                    const float delays[] = {0.5f, 1, 0, -2, 3};
                    original<void(THISCALL *)(void *, float, DWORD, DWORD, DWORD, DWORD, void *)>(0x41c7a0)(
                        q, delays[random() % 5], pick_part(), pick_part(), random() % 2 ? ptrv(g_fe[random() % NFE]) : 0,
                        random() % 2 ? pick_part() : 0, &g_fa[random() % 16]);
                }
                standins_off();
                const float dts[] = {0.25f, 1, 0, 2.5f};
                dt = dts[random() % 4];
                for (DWORD &x : v)
                    x = pick_part();
                v[5] = random() % 4 ? ptrv(&g_fa[random() % 16]) : 0;
                g_skip = 1 << 5;
            },
            [&] {
                if (what == 0) {
                    at<void(THISCALL *)(void *, float, DWORD, DWORD, DWORD, DWORD, void *)>(0x41c7a0)(q, dt, v[0], v[1], v[2], v[3], vptr(v[5]));
                    return DWORD(0);
                }
                if (what == 1) {
                    at<void(THISCALL *)(void *, float)>(0x41c700)(q, dt);
                    return DWORD(0);
                }
                if (!dw(q, 0x10))
                    return DWORD(0);
                BYTE *item = static_cast<BYTE **>(vptr(dw(q, 0xc)))[0];
                const char *const messages[] = {"Delete", "Leaving Scope", "delete", "Other"};
                const DWORD offs[] = {0xc, 0x10, 0x14, 0x18};
                void *sender = vptr(dw(item, offs[v[0] % 4]));
                return at<DWORD(THISCALL *)(void *, void *, const char *)>(0x41c230)(item, sender, messages[v[1] % 4]);
            },
            [&] {
                st(dw(q, 0x10));
                for (DWORD i = 0; i < dw(q, 0x10); i++) {
                    BYTE *item = static_cast<BYTE **>(vptr(dw(q, 0xc)))[i];
                    st(tag_made(ptrv(item)));
                    state_item(item);
                    for (DWORD off = 0xc; off <= 0x18; off += 4)
                        dw(item, off) = 0;   // no listeners off the fakes in the destructor
                }
            });
    }
    // the item's set directly (the listeners off the old references, on the new)
    for (int k = 0; k < scaled(60); k++) {
        const Case cs{"CActionQueueItem", 0x41c120, 77000u + (unsigned)k * 5, 0xffffffff};
        BYTE *item = nullptr;
        DWORD v[6];
        run_case(
            t, cs,
            [&] {
                item = create("CActionQueueItem");
                for (DWORD off = 0xc; off <= 0x18; off += 4)
                    dw(item, off) = random() % 2 ? pick_part() : 0;
                for (DWORD &x : v)
                    x = pick_part();
                v[0] = random();
                v[5] = 0;   // the action stays empty (the item would delete it)
            },
            [&] {
                at<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x41c120)(item, v[0], v[1], v[2], v[3], v[4], v[5]);
                return DWORD(0);
            },
            [&] {
                state_item(item);
                for (DWORD off = 0xc; off <= 0x18; off += 4)
                    dw(item, off) = 0;   // no listeners off the fakes in the destructor
            });
    }
    // the destructors of the queue, its item and its constructor
    for (int k = 0; k < scaled(48); k++) {
        const int what = k % 3;
        const Case cs{"queue dtor", what == 0 ? 0x41c4b0u : what == 1 ? 0x41bcd0u : 0x41c0b0u, 79000u + (unsigned)k * 3, 0xffffffff};
        BYTE *o = nullptr;
        DWORD flags = 0;
        run_case(
            t, cs,
            [&] {
                if (what == 0) {
                    o = create("CActionQueue");
                    g_skip = 1 << 5;
                    standins_on();
                    for (int i = (int)(random() % 4); i > 0; i--)
                        original<void(THISCALL *)(void *, float, DWORD, DWORD, DWORD, DWORD, void *)>(0x41c7a0)(o, 1, pick_part(), 0, 0, 0,
                                                                                                         &g_fa[random() % 16]);
                    standins_off();
                } else {
                    o = create("CActionQueueItem");
                    for (DWORD off = 0xc; off <= 0x18; off += 4)
                        dw(o, off) = random() % 2 ? pick_part() : 0;
                    dw(o, 0x1c) = pick_child();
                }
                g_nobj = 0;
                flags = random() % 2;
                g_skip = 0;
            },
            [&] {
                if (what == 2) {
                    at<void(THISCALL *)(void *)>(0x41c0b0)(o);
                    return DWORD(0);
                }
                return tagp(at<Dtor>(what == 0 ? 0x41c4b0 : 0x41bcd0)(o, flags)) == 0 ? DWORD(1) : DWORD(2);
            },
            [&] {
                if (what == 2 || !(flags & 1)) {
                    st(dw(o, 0));
                    pool_free(o, what == 0 ? 0x1c : 0x34);
                }
            });
    }
    // the wrappers' base constructor on a fake object (the list head put back)
    for (int k = 0; k < scaled(8); k++) {
        const Case cs{"wrapper", 0x41a0a0, 80000u + (unsigned)k, 0xffffffff};
        static BYTE w[0x20];
        DWORD head = 0;
        run_case(
            t, cs,
            [&] {
                memset(w, 0xcd, sizeof w);
                head = *at<DWORD *>(0x62e6ec);
            },
            [&] { return at<void *(THISCALL *)(void *)>(0x41a0a0)(w) == w ? DWORD(1) : DWORD(0); },
            [&] {
                st(dw(w, 0));
                st(dw(w, 4) == head);
                st(*at<DWORD *>(0x62e6ec) == ptrv(w));
                *at<DWORD *>(0x62e6ec) = head;
            });
    }
    for (int k = 0; k < scaled(8); k++) {
        const Case cs{"CActionQueue", 0x41c6d0, 81000u + (unsigned)k, 0xffffffff};
        BYTE *o = nullptr;
        run_case(
            t, cs,
            [&] {
                o = static_cast<BYTE *>(pool_alloc(0x1c));
                memset(o, 0xcd, 0x1c);
                note_obj(o, 0x1c);
            },
            [&] { return tagp(at<void *(THISCALL *)(void *)>(0x41c6d0)(o)); }, [&] { state_obj(o, 0x1c, 0); });
    }
}

// ---- The deleting destructors of the classes --------------------------------------------------------------------

struct Dtor1 {
    const char *cls;
    DWORD create, create_array, size, address, strings;
    bool array;
};
const Dtor1 DTORS[] = {
    {"CConditionalAction", 0x41b120, 0x41b160, 0x14, 0x41b1d0, 0, true},
    {"CIfAction", 0x41b700, 0x41b740, 0x14, 0x41b7b0, 0, true},
    {"CDialogLine", 0x41dd30, 0x41dd80, 0x14, 0x41de00, 0x3 << 2, true},
    {"COtherMapAction", 0x41ee70, 0x41eeb0, 0x10, 0x41ef20, 0x8, true},
    {"CollideTriggerAI", 0x41c9d0, 0x41ca30, 0x38, 0x41cac0, 0x40, true},
    {"CPassThroughTriggerAI", 0x41d120, 0x41d170, 0x24, 0x41d1f0, 0x40, true},
    {"CRelayAI", 0x41fc20, 0x41fc60, 0x18, 0x41fcd0, 0, true},
    {"CRelayAI", 0x41fc20, 0x41fc60, 0x18, 0x41c950, 0, false},                 // CLayerPartAI's level
    {"CPassThroughTriggerAI", 0x41d120, 0x41d170, 0x24, 0x41c980, 0x40, false},  // CTriggerAI's level
};
void test_dtors(Tally &t)
{
    for (const Dtor1 &d : DTORS)
        for (int k = 0; k < scaled(12); k++) {
            const DWORD flags = d.array ? (DWORD)k % 4 : (DWORD)k % 2;
            const Case cs{d.cls, d.address, 83000u + (unsigned)k * 31 + d.address % 997, 0xffffffff};
            BYTE *obj = nullptr;
            DWORD *block = nullptr;
            DWORD count = 0;
            run_case(
                t, cs,
                [&] {
                    if (flags & 2) {
                        count = (DWORD)k % 4;
                        BYTE *e = static_cast<BYTE *>(original<void *(__fastcall *)(int, int, int)>(d.create_array)((int)count, 0, 0));
                        block = reinterpret_cast<DWORD *>(e) - 1;
                        obj = e;
                    } else {
                        obj = create_at(d.create, d.size);
                        g_nobj = 0;
                        for (DWORD off = 8; off < d.size; off += 4)
                            if (d.strings >> (off / 4) & 1)
                                set_text(obj, off, NAMES[random() % NNAMES]);
                    }
                },
                [&] {
                    void *r = at<Dtor>(d.address)(obj, flags);
                    return r == obj ? DWORD(1) : r == block ? DWORD(2) : ptrv(r);
                },
                [&] {
                    if (!(flags & 1)) {
                        if (flags & 2) {
                            for (DWORD i = 0; i < count; i++)
                                st(dw(obj + i * d.size, 0));
                            heap_free(block);
                        } else {
                            st(dw(obj, 0));
                            pool_free(obj, d.size);
                        }
                    }
                });
        }
}

// ---- The trigger AIs, the converting wrapper, the dialogue helpers, COtherMapAction ----------------------------------

void test_classes(Tally &t)
{
    // the collisions of CollideTriggerAI and CPassThroughTriggerAI
    for (int k = 0; k < scaled(120); k++) {
        const bool collide = k % 2 == 0;
        const Case cs{collide ? "CollideTriggerAI" : "CPassThroughTriggerAI", collide ? 0x41cf10u : 0x41d3b0u, 85000u + (unsigned)k * 7, 0};
        BYTE *ai = nullptr;
        DWORD part = 0, x = 0, flag = 0;
        run_case(
            t, cs,
            [&] {
                ai = collide ? create_at(0x41c9d0, 0x38) : create_at(0x41d120, 0x24);
                dw(ai, 0x20) = pick_child();
                if (collide) {
                    const float times[] = {0, 5, 10, 20, -1, 12.5f};
                    fl(ai, 0x24) = times[random() % 6];
                    fl(ai, 0x28) = times[random() % 6];
                    dw(ai, 0x30) = pick_child();
                }
                part = ptrv(g_fe[random() % NFE]);
                x = pick_part();
                flag = random() % 4 == 0 ? 1 : random() % 8 == 0 ? 0x100 : 0;
            },
            [&] {
                at<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(cs.address)(ai, part, x, 0, 0, flag);
                return DWORD(0);
            },
            [&] {
                state_obj(ai, collide ? 0x38 : 0x24, 0);
                dw(ai, 0x20) = 0;
                if (collide)
                    dw(ai, 0x30) = 0;
            });
    }
    // CTriggerAI's action setter
    for (int k = 0; k < scaled(24); k++) {
        const Case cs{"CollideTriggerAI", 0x41d0a0, 87000u + (unsigned)k, 0};
        BYTE *ai = nullptr;
        DWORD a = 0;
        run_case(
            t, cs,
            [&] {
                ai = create_at(0x41c9d0, 0x38);
                dw(ai, 0x20) = pick_child();
                a = pick_child();
            },
            [&] {
                at<void(THISCALL *)(void *, DWORD)>(0x41d0a0)(ai, a);
                return DWORD(0);
            },
            [&] {
                st(tag(dw(ai, 0x20)));
                dw(ai, 0x20) = 0;
            });
    }
    // the AI converting wrapper 0x62e790 and a fake one with a text
    const struct {
        DWORD create, size;
    } ais[] = {{0x420f30, 0x44}, {0x4218d0, 0x38}, {0x41c9d0, 0x38}, {0x41d120, 0x24}, {0x41fc20, 0x18}};
    static BYTE fake_wrapper[0x20];
    for (int k = 0; k < scaled(60); k++) {
        const bool describe = k % 2 == 0;
        const auto &src = ais[k / 2 % 5];
        const bool fake = k / 10 % 2;
        const Case cs{"ai wrapper", describe ? 0x41d3e0u : 0x41d520u, 89000u + (unsigned)k * 11, 0xffffffff};
        BYTE *ai = nullptr;
        BYTE *made = nullptr;
        CClassInfo *mcls = nullptr;
        run_case(
            t, cs,
            [&] {
                if (!describe && k / 2 % 5 >= 2) {
                    ai = nullptr;   // the conversion only for touching triggers (its field copy)
                    return;
                }
                ai = create_at(src.create, src.size);
                if (src.size >= 0x24)
                    dw(ai, 0x20) = pick_child();
                if (src.size >= 0x38) {
                    dw(ai, 0x24) = pick_child();
                    dw(ai, 0x28) = pick_child();
                }
                memset(fake_wrapper, 0, sizeof fake_wrapper);
                dw(fake_wrapper, 0) = 0x5ed074;
                dw(fake_wrapper, 8) = ptrv("Converted");
                dw(fake_wrapper, 0xc) = ptrv("Exit Action");
                dw(fake_wrapper, 0x10) = ptrv("Action");
                if (!describe)
                    g_nobj = 0;   // deleted by the conversion
                mcls = nullptr;
            },
            [&] {
                if (!ai)
                    return DWORD(0);
                void *w = fake ? static_cast<void *>(fake_wrapper) : at<void *>(0x62e790);
                if (describe) {
                    const char *r = at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(0x41d3e0)(w, ai, k % 3 ? &mcls : nullptr);
                    return r ? text_hash(r) : DWORD(0);
                }
                made = static_cast<BYTE *>(at<void *(THISCALL *)(void *, void *)>(0x41d520)(w, ai));
                note_obj(made, 0x38);
                return dw(made, 0);
            },
            [&] {
                st(ptrv(mcls));
                if (made) {
                    // the byte fields' padding is what the pool block held
                    for (DWORD off = 0x10; off < 0x38; off += 4)
                        st(off == 0x18 ? zs_hash(made + off)
                           : off == 0x14 || off == 0x34 ? dw(made, off) & 0xff
                           : off == 0x1c ? dw(made, off) & 0xffffff
                                         : tag(dw(made, off)));
                    dw(made, 0x20) = dw(made, 0x30) = 0;
                    made = nullptr;
                } else if (ai && describe) {
                    for (DWORD off = 0x20; off < src.size && off <= 0x28; off += 4)
                        dw(ai, off) = 0;
                }
            });
    }
    // the dialogue helpers, the property list, the descriptor clone
    for (int k = 0; k < scaled(40); k++) {
        const int what = k % 4;
        const DWORD addresses[] = {0x41e010, 0x41dbe0, 0x41d7f0, 0x41e0e0};
        const Case cs{"dialog", addresses[what], 91000u + (unsigned)k * 3, 0xffffffff};
        BYTE *line = nullptr;
        ZString out;
        run_case(
            t, cs,
            [&] {
                line = create_at(0x41dd30, 0x14);
                set_text(line, 8, NAMES[random() % NNAMES]);
                set_text(line, 0xc, NAMES[random() % NNAMES]);
                at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&out, "old");
            },
            [&] {
                switch (what) {
                case 0:
                    return tagp(vptr(at<DWORD(THISCALL *)(void *, void *)>(0x41e010)(line, &out))) == tagp(&out) ? DWORD(1) : DWORD(2);
                case 1:
                    return at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x41dbe0)(line, 1, 2, 3, 4, 5, 6);
                case 2:
                    at<void(THISCALL *)(void *, DWORD, void *)>(0x41d7f0)(line, 1, k % 8 == 2 ? nullptr : g_object);
                    return DWORD(0);
                default:
                    return ptrv(at<const char *(THISCALL *)(void *)>(0x41e0e0)(line));
                }
            },
            [&] {
                st(zs_hash(&out));
                at<void(THISCALL *)(void *)>(0x4defd0)(&out);
            });
    }
    for (int k = 0; k < scaled(24); k++) {
        const Case cs{"property list", 0x41e020, 93000u + (unsigned)k * 3, 0xffffffff};
        BYTE *o = nullptr;
        DWORD flags = 0;
        run_case(
            t, cs,
            [&] {
                o = static_cast<BYTE *>(pool_alloc(0x18));
                memset(o, 0, 0x18);
                dw(o, 0) = 0x5ed448;
                dw(o, 4) = 0x5ed43c;
                const DWORD n = random() % 5;
                void **data = static_cast<void **>(pool_get_alloc(8 * 4));
                for (DWORD i = 0; i < 8; i++)
                    data[i] = i < n && random() % 4 ? &g_fa[random() % 16] : nullptr;
                dw(o, 8) = ptrv(data);
                dw(o, 0xc) = n;
                dw(o, 0x10) = 8;
                at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(o + 0x14, "Levels/x.prop");
                flags = random() % 2;
            },
            [&] { return tagp(at<Dtor>(0x41e020)(o, flags)) == ptrv(o) ? 1u : DWORD(0); },
            [&] {
                if (!(flags & 1)) {
                    state_obj(o, 0x18, 0);
                    pool_free(o, 0x18);
                }
            });
    }
    // the descriptor clone over the registered "Line" field and copies of it
    for (int k = 0; k < scaled(8); k++) {
        const Case cs{"CDialogActionBase Line", 0x41dbf0, 95000u + (unsigned)k, 0xffffffff};
        void *f = nullptr;
        BYTE *c = nullptr;
        run_case(
            t, cs, [&] { f = at<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(cls_at(0x41d5e0), "Line", 0); },
            [&] {
                c = static_cast<BYTE *>(at<void *(THISCALL *)(void *)>(0x41dbf0)(f));
                return dw(c, 0);
            },
            [&] {
                // the copy constructor leaves +0x24 and the upper half of +0x2c as the heap gave them
                for (DWORD off = 4; off < 0x34; off += 4)
                    st(off == 4 || off == 0x1c ? zs_hash(c + off)
                       : off == 0x24          ? 0
                       : off == 0x2c          ? (dw(c, off) & 0xffff) == (dw(f, off) & 0xffff)
                                              : dw(c, off) == dw(f, off));
                vcall<void *>(c, 0, 1);   // the descriptor's deleting destructor
            });
    }
    // COtherMapAction's execute
    for (int k = 0; k < scaled(48); k++) {
        const Case cs{"COtherMapAction", 0x41f130, 97000u + (unsigned)k * 5, 0xff};
        BYTE *o = nullptr;
        run_case(
            t, cs,
            [&] {
                o = create("COtherMapAction");
                const char *const maps[] = {"", "ACT 1/M1M2/01 Main", "ACT 1\\M1M2\\01 Main", "Test/Nachbau1", "x\\y", "Other"};
                set_text(o, 0xc, maps[random() % 6]);
                dw(o, 8) = pick_child();
            },
            [&] { return at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x41f130)(o, g_layer, 1, 2, 3); },
            [&] { state_obj(o, 0x10, 0x8); });
    }
}

// The field descriptors' alternative names (real data): every registered field of the classes, first all
// through the original (the module's hooks out once), then all through the reimplementation
void test_fields(CClassInfo **cls, int n, Tally &t)
{
    int cases = 0;
    DWORD hash[2] = {2166136261u, 2166136261u};
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0)
            hooks_out();
        for (int i = 0; i < n; i++) {
            CClassInfo *c = cls[i];
            if (!c || !c->fields)
                continue;
            for (DWORD f = 0; f < c->num_fields; f++)
                if (void *d = c->fields[f]) {
                    const char *r = at<const char *(THISCALL *)(void *)>(0x419050)(d);
                    hash[pass] = fnv(hash[pass], &r, 4);
                    cases += pass == 0;
                }
        }
        if (pass == 0)
            hooks_in();
    }
    char d[96];
    wsprintfA(d, "[%d fields]", cases);
    t.check(hash[0] == hash[1], "0x419050", d);
}

// ---- Real data: the names of the running level's parts through the real layer ---------------------------------

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

// Up to 64 named parts of the level's grid; their names, ranges and lists of them through 0x41a760,
// 0x41ad80 and 0x41aed0 on the real layer (sort list, rectangle query, polygons and part names real; the
// reporter a fake, slot 27 of a fake action), "$touching" with a real part as the trigger. Compared: the
// parts found (by address), eax, the events of the fakes.
void test_level(Tally &t)
{
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28)) {
        log("  action helpers real data: no level grid found");
        return;
    }
    const DWORD columns = dw(layer, 0x60), rows = dw(layer, 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return;
    BYTE *parts[64];
    int n = 0;
    for (DWORD i = 0; i < columns * rows && n < 64; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || (c->count && !readable(c->data, c->count * 4)))
            return;
        for (DWORD k = 0; k < c->count && n < 64; k++) {
            const LayerPart *p = static_cast<const LayerPart *>(c->data[k]);
            bool known = false;   // a part in several cells once
            for (int j = 0; j < n; j++)
                known |= parts[j] == c->data[k];
            if (!known && readable(p, sizeof *p) && p->name.buffer && p->name.c_str()[0] && (i * 7 + k) % 5 == 0)
                parts[n++] = static_cast<BYTE *>(c->data[k]);
        }
    }
    log("  action helpers real data: %d named parts of the level", n);
    const DWORD skip = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4;   // stamps, names, rectangles, polygons, part names real
    for (int i = 0; i < n; i++) {
        const char *name = reinterpret_cast<const LayerPart *>(parts[i])->name.c_str();
        char list_text[128];
        wsprintfA(list_text, "%s, %s", name, reinterpret_cast<const LayerPart *>(parts[(i + 1) % n])->name.c_str());
        const char *const texts[] = {name, list_text, "$touching", "$trigger"};
        for (int k = 0; k < 4; k++) {
            const Case cs{texts[k], 0x41a760, 99000u + (unsigned)i * 4 + (unsigned)k, 0};
            PartList *list = nullptr;
            run_case(
                t, cs,
                [&] {
                    list = temp_list();
                    g_skip = skip;
                },
                [&] {
                    at<void(__fastcall *)(void *, const char *, DWORD, DWORD, void *)>(0x41a760)(layer, texts[k], ptrv(parts[i]), 0, list);
                    return DWORD(0);
                },
                [&] {
                    st(list->count);
                    for (DWORD j = 0; j < list->count && j < 64; j++)
                        st(ptrv(list->data[j]));
                    release_list(list);
                });
        }
        for (int k = 0; k < 2; k++) {
            const Case cs{name, k ? 0x41aed0u : 0x41ad80u, 99500u + (unsigned)i * 2 + (unsigned)k, 0xffffffff};
            run_case(
                t, cs, [&] { g_skip = skip; },
                [&] {
                    if (k)
                        return at<DWORD(THISCALL *)(void *, void *, const char *, DWORD, DWORD, DWORD, float, CClassInfo *)>(0x41aed0)(
                                   &g_fa[3], layer, list_text, ptrv(parts[i]), 0, 0, i % 3 ? 0 : 0.5f, i % 2 ? cls_at(CLS_BASE) : nullptr) &
                               0xff;
                    return at<DWORD(__fastcall *)(void *, const char *, DWORD, DWORD, DWORD, void *, DWORD, CClassInfo *)>(0x41ad80)(
                        layer, name, ptrv(parts[i]), 0, 0, &g_fa[1], 1, i % 2 ? cls_at(CLS_ANIMATED) : nullptr);
                },
                [] {});
        }
    }
}

}  // namespace

ZAX_CHECKS(action_helpers)
{
    const DWORD start = GetTickCount();
    Tally names{"action helpers: name lookups, warning, per-target loop"}, actions{"action helpers: CAction, conditional actions, the queue"},
        dtors{"action helpers: deleting destructors"}, classes{"action helpers: trigger AIs, AI wrapper, dialogue, COtherMapAction"},
        fields{"action helpers: field alternative names (real data)"}, level{"action helpers: the level's names (real data)"};
    init_vtables();
    test_names(names);
    names.report();
    test_actions(actions);
    actions.report();
    test_dtors(dtors);
    dtors.report();
    test_classes(classes);
    classes.report();
    test_fields(cls, n, fields);
    fields.report();
    test_level(level);
    level.report();
    log("  action helpers: %u ms", GetTickCount() - start);
}

}  // namespace zax
