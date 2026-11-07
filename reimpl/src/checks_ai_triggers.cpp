// Checks of the trigger AIs and their neighbours of 0x418000-0x423fff (ai_triggers.cpp) against the
// original, in the game thread.
//  - Class getters (both copies; the class variable set and 0, ClassInfo_Register a stand-in recording its
//    arguments), CTriggerRelayAction's RegisterFields into a fake class info and against the registered
//    descriptors (real data), the "Relay Name" descriptor's clone, CInventoryItemHolder's Create and
//    CreateArray.
//  - Deleting destructors (flags 0-3; objects from the original Create and CreateArray).
//  - The slots and helpers in a fake world: both sides from the same seed (side 0 with the module's hooks
//    out), real objects (the original Create) with their fields from the seed, fake parts (a vtable of
//    recorders: class, rectangle, position, polygon, offset, AI lookup, target, turning), stand-ins for the
//    exe functions beyond the module (listeners, the AI's action 0x4fd160, the switch, the names, hit
//    points, the temporary part lists, CLayer::FindRect, the oval and polygon tests, the positions, the line
//    reader, the message base, the world's first character, the player's direction, the action execute,
//    the random range, the reporter's time). Compared: eax (masked), the event log, the object and the
//    fakes afterwards.
//  - The converting wrappers (the four real global ones and a fake) on real AIs; a converted AI compared by
//    its contents and deleted.
//  - The property list: Load, SaveAs, Save, Lookup, NameAt, At, AddNamed with the file buffer replaced by
//    stand-ins on scripts from the seed (nothing reaches the disk); real data: three .RDS files of the
//    game loaded through the real file buffer (read only) and their lines looked up.
//  - Real data: copies of the running level's trigger AIs (the oval, polygon, collide, repeat timer, spawn
//    point and compass AIs; a spread of at most 40 per class) through the slots in the fake world; nothing
//    fires in the game (the actions only reach the stand-ins).
#include "check_util.h"
#include "checks_ai_world.h"
#include "field_desc.h"
#include "field_kinds.h"

namespace zax {

int hooks_ai_triggers(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
void setf(void *p, DWORD off, float v) { memcpy(static_cast<BYTE *>(p) + off, &v, 4); }
float raw_float(DWORD b)
{
    float f;
    memcpy(&f, &b, 4);
    return f;
}

// From checks_geometry.cpp
bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
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
    const int n = hooks_ai_triggers(nullptr);
    if (!hook_sites_fit("ai_triggers", n, MAX_SITES))
        return;
    hooks_ai_triggers(list);
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

// ---- Values from the seed ------------------------------------------------------------------------------------

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
Gen g_answers;   // the stand-ins' answers (seeded alike on both sides)

// ---- The event log and the state ------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 96;
DWORD g_log[2][LOG][4];
BYTE *g_self;   // the object under test: its addresses tagged
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
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }
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
const char *zs_text(const void *z)
{
    const ZString *s = static_cast<const ZString *>(z);
    return s->buffer ? s->buffer->text : nullptr;
}
void zs_ctor(void *z) { at<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_set(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }

CClassInfo *cls_at(DWORD getter) { return at<CClassInfo *(__cdecl *)()>(getter)(); }
constexpr DWORD CLS_ANIMATED = 0x48dcc0, CLS_BASE = 0x487620, CLS_DESTRUCTABLE = 0x48bdf0, CLS_WALKING = 0x49b810, CLS_LAYER_PART = 0x4f9bf0,
                CLS_RELAY = 0x41fd70, CLS_PLAYER_CONTROL = 0x542170;

// ---- The fakes ------------------------------------------------------------------------------------------------

constexpr int NP = 8, NA = 4;
constexpr DWORD PART = 0x200;
struct alignas(16) Fakes {
    BYTE part[NP][PART];   // layer parts / entities
    BYTE action[NA][0x20];
    BYTE poly[NP][0x40];
    BYTE relay_ai[0x20];   // the CRelayAI slot 64 finds (+0x10 action, +0x14 once, +0x15 triggered)
    BYTE pc[0x20];         // the player control AI (and a CShotAI)
    BYTE layer[0x100];
    BYTE item[0x20];       // CInventoryItemHolder's item
    BYTE buffer[0x100];    // objects built in place
    DWORD cell;            // an action field outside the AI
};
Fakes g_f;
struct PartList {
    DWORD vftable;
    void **data;
    DWORD count;
};
void *g_list_data[16];
PartList g_list;
struct PartCfg {
    CClassInfo *cls;
    float pos[2], offset[2], hp;
    int name;   // index into PART_NAMES, -1: none
    DWORD updates, find_ai, target;
    BYTE has_seq, idle;
};
PartCfg g_cfg[NP];
const char *const PART_NAMES[] = {"Zax", "ZAX", "Door 1", "Other", "door 1"};
DWORD g_time_bits, g_rand_calls;
BYTE *g_first_char;
BYTE g_reporter[0x10];
int part_index(const void *p)
{
    const DWORD v = ptrv(p) - ptrv(g_f.part);
    return v < sizeof g_f.part ? (int)(v / PART) : 0;
}
BYTE *part(Gen &g) { return g_f.part[g.below(NP)]; }
DWORD vt_part[140], vt_reporter[16], vt_item[4];

// Recorders of the fake parts' slots (thiscall on the part)
template <int S> DWORD THISCALL p_trap(BYTE *p) { ev(0xe0ff, ptrv(p), S); return 0; }
CClassInfo *THISCALL p_class(BYTE *p) { return g_cfg[part_index(p)].cls; }
DWORD THISCALL p_rect(BYTE *p, int *rect, DWORD kind, DWORD x)
{
    ev(0xe025, ptrv(p), kind, x);
    for (int i = 0; i < 4; i++)
        rect[i] = (int)g_answers.below(400) - 100;
    return g_answers.next();
}
float *THISCALL p_pos(BYTE *p, float *out)
{
    ev(0xe02b, ptrv(p));
    out[0] = g_cfg[part_index(p)].pos[0];
    out[1] = g_cfg[part_index(p)].pos[1];
    return out;
}
BYTE *THISCALL p_poly(BYTE *p)
{
    ev(0xe030, ptrv(p));
    return g_f.poly[part_index(p)];
}
float *THISCALL p_offset(BYTE *p, float *out)
{
    ev(0xe032, ptrv(p));
    out[0] = g_cfg[part_index(p)].offset[0];
    out[1] = g_cfg[part_index(p)].offset[1];
    return out;
}
void *THISCALL p_find_ai(BYTE *p, CClassInfo *c, DWORD x)
{
    ev(0xe040, ptrv(p), ptrv(c), x);
    const DWORD k = g_cfg[part_index(p)].find_ai;
    if (c == cls_at(CLS_RELAY))
        return k & 1 ? g_f.relay_ai : nullptr;
    if (c == cls_at(CLS_PLAYER_CONTROL))
        return k & 2 ? g_f.pc : nullptr;
    return k & 4 ? g_f.pc : nullptr;   // CShotAI
}
BYTE *THISCALL p_target(BYTE *p)
{
    ev(0xe046, ptrv(p));
    const DWORD t = g_cfg[part_index(p)].target;
    return t < NP ? g_f.part[t] : nullptr;
}
template <int S> void THISCALL p_turn(BYTE *p, DWORD angle, DWORD x) { ev(0xe000 + S, ptrv(p), angle, x); }
long double THISCALL rep_time(void *) { return (long double)raw_float(g_time_bits); }
void *THISCALL item_delete(BYTE *p, DWORD flags)
{
    ev(0xd001, ptrv(p), flags);
    return p;
}

template <int... S> struct Seq {};
template <int N, int... S> struct MakeSeq : MakeSeq<N - 1, N - 1, S...> {};
template <int... S> struct MakeSeq<0, S...> {
    using type = Seq<S...>;
};
template <int... S> void fill_part(Seq<S...>) { ((vt_part[S] = ptrv((void *)&p_trap<S>)), ...); }
void init_vtables()
{
    fill_part(typename MakeSeq<140>::type{});
    vt_part[2] = ptrv((void *)&p_class);
    vt_part[37] = ptrv((void *)&p_rect);
    vt_part[43] = ptrv((void *)&p_pos);
    vt_part[48] = ptrv((void *)&p_poly);
    vt_part[50] = ptrv((void *)&p_offset);
    vt_part[64] = ptrv((void *)&p_find_ai);
    vt_part[70] = ptrv((void *)&p_target);
    vt_part[103] = ptrv((void *)&p_turn<103>);
    vt_part[104] = ptrv((void *)&p_turn<104>);
    for (DWORD &v : vt_reporter)
        v = ptrv((void *)&p_trap<200>);
    vt_reporter[13] = ptrv((void *)&rep_time);
    for (DWORD &v : vt_item)
        v = ptrv((void *)&p_trap<201>);
    vt_item[1] = ptrv((void *)&item_delete);
}

const float POS[] = {0, 10, 100, -50, 512.5f, 33.25f, 1e6f, -0.0f};
const float HP[] = {0, 1, -5, 100, 0.5f};
const float TIMES[] = {0, 1, 2.5f, 10, 100, 0.25f};
void build_world(Gen &g)
{
    CClassInfo *const classes[] = {cls_at(CLS_ANIMATED), cls_at(CLS_BASE), cls_at(CLS_DESTRUCTABLE), cls_at(CLS_WALKING), cls_at(CLS_LAYER_PART)};
    memset(&g_f, 0, sizeof g_f);
    const DWORD players = *at<DWORD *>(0x6c29a0), enemies = *at<DWORD *>(0x6c29a8);
    for (int i = 0; i < NP; i++) {
        BYTE *p = g_f.part[i];
        dw(p, 0) = ptrv(vt_part);
        DWORD f = g.below(4);
        if (g.below(2))
            f |= players;
        if (g.below(2))
            f |= enemies;
        if (g.below(2))
            f |= 0x800000;
        dw(p, 0x1c) = f;
        dw(p, 0x48) = g.below(6) ? ptrv(g_f.layer) : 0;
        PartCfg &c = g_cfg[i];
        c.cls = classes[g.below(5)];
        c.pos[0] = g.pick(POS, 8);
        c.pos[1] = g.pick(POS, 8);
        c.offset[0] = g.pick(POS, 8);
        c.offset[1] = g.pick(POS, 8);
        c.hp = g.pick(HP, 5);
        c.name = (int)g.below(6) - 1;
        c.updates = g.below(8);
        c.find_ai = g.below(8);
        c.target = g.below(NP + 2);
        c.has_seq = (BYTE)g.below(2);
        c.idle = (BYTE)g.below(2);
    }
    if (g.below(3))
        dw(g_f.relay_ai, 0x10) = ptrv(g_f.action[0]);
    by(g_f.relay_ai, 0x14) = (BYTE)g.below(2);
    by(g_f.relay_ai, 0x15) = (BYTE)g.below(2);
    g_f.cell = g.below(2) ? ptrv(g_f.action[1]) : 0;
    dw(g_f.item, 0) = ptrv(vt_item);
    g_time_bits = as_dword(g.pick(TIMES, 6));
    g_rand_calls = 0;
    g_list = {0, g_list_data, g.below(7)};
    for (void *&d : g_list_data)
        d = part(g);
    g_first_char = part(g);
    dw(g_reporter, 0) = ptrv(vt_reporter);
}
void fakes_state()
{
    st(fnv(2166136261u, &g_f, sizeof g_f));
    st(g_list.count);
    for (DWORD i = 0; i < g_list.count && i < 16; i++)
        st(ptrv(g_list_data[i]));
}

// ---- Stand-ins for the exe functions beyond the module --------------------------------------------------------

DWORD THISCALL s_remove_listener(BYTE *p, void *who, int a, const char *t)
{
    ev(0x4fc200, ptrv(p), tagp(who), (DWORD)a ^ text_hash(t));
    return g_answers.next();
}
DWORD THISCALL s_add_listener(BYTE *p, void *who, int a, const char *t)
{
    ev(0x4fc1b0, ptrv(p), tagp(who), (DWORD)a ^ text_hash(t));
    return g_answers.next();
}
DWORD THISCALL s_run_action(void *ai, void *layer, void *field, void *a2, void *a4)
{
    ev(0x4fd160, tagp(ai) ^ ptrv(layer) * 3, tagp(field), ptrv(a2) ^ ptrv(a4) * 5);
    return g_answers.next();
}
DWORD THISCALL s_has_seq(BYTE *p)
{
    ev(0x4bcd90, ptrv(p));
    return (g_answers.next() & ~0xffu) | g_cfg[part_index(p)].has_seq;
}
DWORD THISCALL s_idle(BYTE *p)
{
    ev(0x4bcde0, ptrv(p));
    return (g_answers.next() & ~0xffu) | g_cfg[part_index(p)].idle;
}
void THISCALL s_flip(BYTE *p) { ev(0x4bce40, ptrv(p)); }
const char *THISCALL s_name(BYTE *p)
{
    ev(0x4fbf20, ptrv(p));
    const int k = p ? g_cfg[part_index(p)].name : -1;
    return k < 0 ? nullptr : PART_NAMES[k];
}
long double THISCALL s_hp(BYTE *p)
{
    ev(0x48d030, ptrv(p));
    return (long double)g_cfg[part_index(p)].hp;
}
void THISCALL s_temp_get(PartList **out)
{
    ev(0x59a470);
    *out = &g_list;
}
DWORD THISCALL s_temp_release(PartList **l)
{
    ev(0x59a510, *l == &g_list);
    return g_answers.next();
}
DWORD THISCALL s_find_rect(void *layer, PartList *list, int *rect, DWORD kind, DWORD all, DWORD none, DWORD any)
{
    ev(0x4ed5e0, ptrv(layer) ^ (list == &g_list), fnv(2166136261u, rect, 16), kind ^ all * 3 ^ none * 5 ^ any * 7);
    g_list.count = g_answers.below(6);
    for (DWORD i = 0; i < g_list.count; i++)
        g_list_data[i] = g_f.part[g_answers.below(NP)];
    return g_answers.next();
}
bool __fastcall s_within(BYTE *p, BYTE *tp, DWORD r, DWORD ys)
{
    ev(0x4fc090, ptrv(p), ptrv(tp), r ^ ys * 3);
    return g_answers.below(2) != 0;
}
DWORD THISCALL s_overlaps(void *poly, const float *off, void *poly2, const float *pos)
{
    ev(0x547d60, ptrv(poly) ^ ptrv(poly2) * 3, as_dword(off[0]) ^ as_dword(off[1]) * 3, as_dword(pos[0]) ^ as_dword(pos[1]) * 3);
    return g_answers.next();
}
float *THISCALL s_position(BYTE *p, float *out)
{
    ev(0x4fb0a0, ptrv(p));
    out[0] = g_cfg[part_index(p)].pos[0] + 1;
    out[1] = g_cfg[part_index(p)].pos[1] - 2;
    return out;
}
DWORD THISCALL s_updates(BYTE *p)
{
    ev(0x48fe30, ptrv(p));
    return g_cfg[part_index(p)].updates;
}
DWORD THISCALL s_read_line(void *self, const char *n, const char *v, DWORD a3)
{
    ev(0x417500, tagp(self), text_hash(n), text_hash(v) ^ a3);
    return g_answers.next();
}
DWORD THISCALL s_on_message(void *self, void *a, const char *b)
{
    ev(0x46cd50, tagp(self), ptrv(a), text_hash(b));
    return g_answers.next();
}
BYTE *THISCALL s_first_character(void *)
{
    ev(0x4f5d80);
    return g_first_char;
}
void THISCALL s_set_direction(void *pc, const float *dir) { ev(0x542340, ptrv(pc), as_dword(dir[0]), as_dword(dir[1])); }
DWORD THISCALL s_execute(void *a, void *layer, void *a2, void *a3, void *a4, void *caller, void *field)
{
    ev(0x41a720, ptrv(a) ^ ptrv(layer) * 3, ptrv(a2) ^ tagp(a3) * 5 ^ ptrv(a4) * 7, tagp(caller) ^ tagp(field) * 7);
    return g_answers.next();
}
long double THISCALL s_random(void *gen, DWORD lo, DWORD hi)
{
    ev(0x55c550, ptrv(gen), lo, hi);
    const float v[] = {0.5f, 1, 0.01f, 2, 0, 3.25f};
    return g_rand_calls++ > 20 ? 1000.0L : (long double)v[g_answers.below(6)];
}

struct Fn {
    DWORD address;
    void *standin;
};
const Fn FNS[] = {
    {0x4fc200, (void *)&s_remove_listener}, {0x4fc1b0, (void *)&s_add_listener}, {0x4fd160, (void *)&s_run_action},
    {0x4bcd90, (void *)&s_has_seq},         {0x4bcde0, (void *)&s_idle},         {0x4bce40, (void *)&s_flip},
    {0x4fbf20, (void *)&s_name},            {0x48d030, (void *)&s_hp},           {0x59a470, (void *)&s_temp_get},
    {0x59a510, (void *)&s_temp_release},    {0x4ed5e0, (void *)&s_find_rect},    {0x4fc090, (void *)&s_within},
    {0x547d60, (void *)&s_overlaps},        {0x4fb0a0, (void *)&s_position},     {0x48fe30, (void *)&s_updates},
    {0x417500, (void *)&s_read_line},       {0x46cd50, (void *)&s_on_message},   {0x4f5d80, (void *)&s_first_character},
    {0x542340, (void *)&s_set_direction},   {0x41a720, (void *)&s_execute},      {0x55c550, (void *)&s_random},
};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
CodePatch g_patches[NFN];
DWORD g_saved_reporter;
BYTE g_saved_compass[12];
void world_on()
{
    for (int i = 0; i < NFN; i++)
        patch_jump(g_patches[i], FNS[i].address, FNS[i].standin);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c02cc) = ptrv(g_reporter);
    memcpy(g_saved_compass, at<void *>(0x62e9d0), 12);
    memset(at<void *>(0x62e9d0), 0x5a, 12);
}
void world_off()
{
    for (int i = NFN; i-- > 0;)
        unpatch(g_patches[i]);
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
    st(fnv(2166136261u, at<void *>(0x62e9d0), 12));
    memcpy(at<void *>(0x62e9d0), g_saved_compass, 12);
}

// ---- The classes under test ------------------------------------------------------------------------------------

enum Cls { C_OVAL, C_POLY, C_COLLIDE, C_PASS, C_RELAY, C_REPEAT, C_SPAWN, C_COMPASS, C_TRELAY, C_HOLDER, NCLS, C_BUFFER = -1, C_TRIGGER = -2 };
struct ClassDef {
    const char *name;
    DWORD create, create_array, size, dtor, zstring;   // zstring: the ZString's offset (0: none)
    DWORD owned[4];                                    // pointer fields set to 0 before the object is deleted
};
const ClassDef CLASSES[NCLS] = {
    {"CTouchingOvalTriggerAI", 0x420f30, 0x420f60, 0x44, 0x420fc0, 0x18, {0x20, 0x24, 0x28, 0x30}},
    {"CTouchingPolygonTriggerAI", 0x4218d0, 0x421900, 0x38, 0x421960, 0x18, {0x20, 0x24, 0x28, 0x30}},
    {"CollideTriggerAI", 0x41c9d0, 0x41ca30, 0x38, 0x41cac0, 0x18, {0x20, 0x2c, 0x30, 0}},
    {"CPassThroughTriggerAI", 0x41d120, 0x41d170, 0x24, 0x41d1f0, 0x18, {0x20, 0, 0, 0}},
    {"CRelayAI", 0x41fc20, 0x41fc60, 0x18, 0x41fcd0, 0, {0x10, 0, 0, 0}},
    {"CRepeatTimerTriggerAI", 0x4203a0, 0x4203e0, 0x20, 0x420440, 0, {0x10, 0, 0, 0}},
    {"CSpawnPointAI", 0x4209a0, 0x4209e0, 0x1c, 0x420a50, 0, {0x10, 0, 0, 0}},
    {"CAttractCompassAI", 0x422f10, 0x422f50, 0x10, 0x422fb0, 0, {0, 0, 0, 0}},
    {"CTriggerRelayAction", 0x41ffb0, 0x41fff0, 0xc, 0x420060, 8, {0, 0, 0, 0}},
    {"CInventoryItemHolder", 0x423f50, 0x423f90, 0xc, 0x423ff0, 0, {8, 0, 0, 0}},
};

BYTE *action(Gen &g) { return g.below(3) ? g_f.action[g.below(NA)] : nullptr; }
const float RADII[] = {0, 10, 100, 1000, 0.5f, 64};
const float RATIOS[] = {1, 0.712766f, 0.5f, 2, 0.25f};
void fill_trigger(Gen &g, BYTE *o)
{
    for (DWORD off = 0x10; off <= 0x14; off++)
        by(o, off) = (BYTE)g.below(2);
    if (g.below(3) == 0)
        zs_set(o + 0x18, PART_NAMES[g.below(5)]);
    by(o, 0x1c) = g.below(3) == 0;
    by(o, 0x1d) = (BYTE)g.below(2);
    by(o, 0x1e) = g.below(3) == 0;
}
// The object's fields from the seed (every byte from +0x10 set: padding included), or a real object's
void fill(Gen &g, int c, BYTE *o, const BYTE *real)
{
    const ClassDef &d = CLASSES[c];
    dw(o, 4) = 0;
    for (DWORD off = 8; off < d.size; off += 4)
        if (off != d.zstring)
            dw(o, off) = 0;
    if (real) {
        for (DWORD off = 0x10; off < d.size; off += 4)
            if (off != d.zstring)
                dw(o, off) = dw(const_cast<BYTE *>(real), off);
        if (d.zstring && zs_text(real + d.zstring))
            zs_set(o + d.zstring, zs_text(real + d.zstring));
        if (c <= C_POLY)
            dw(o, 0x30) = g.below(2) ? ptrv(part(g)) : 0;   // the touched part a fake
        return;
    }
    const float times[] = {0, 1, 0.5f, -1, 2, 10};
    switch (c) {
    case C_OVAL:
    case C_POLY:
        fill_trigger(g, o);
        dw(o, 0x20) = ptrv(action(g));
        dw(o, 0x24) = ptrv(action(g));
        dw(o, 0x28) = ptrv(action(g));
        setf(o, 0x2c, g.pick(times, 6));
        dw(o, 0x30) = g.below(2) ? ptrv(part(g)) : 0;
        setf(o, 0x34, g.pick(TIMES, 6));
        if (c == C_OVAL) {
            setf(o, 0x38, g.pick(RADII, 6));
            setf(o, 0x3c, g.pick(RATIOS, 5));
            setf(o, 0x40, g.pick(RATIOS, 5));
        }
        break;
    case C_COLLIDE:
    case C_PASS:
        fill_trigger(g, o);
        dw(o, 0x20) = ptrv(action(g));
        break;
    case C_REPEAT: {
        const float delays[] = {0, 1, 2.5f, 0.01f, 10};
        const float pms[] = {0, 0.5f, 1, -1, 3};
        const float left[] = {0, -1, 0.5f, 5, -0.1f};
        dw(o, 0x10) = ptrv(action(g));
        setf(o, 0x14, g.pick(delays, 5));
        setf(o, 0x18, g.pick(pms, 5));
        setf(o, 0x1c, g.pick(left, 5));
        break;
    }
    case C_SPAWN: {
        const float angles[] = {0, 90, 45, -30, 720, 0.001f};
        dw(o, 0x10) = ptrv(action(g));
        by(o, 0x14) = (BYTE)g.below(2);
        setf(o, 0x18, g.pick(angles, 6));
        break;
    }
    case C_HOLDER:
        dw(o, 8) = g.below(2) ? ptrv(g_f.item) : 0;
        break;
    default:
        break;
    }
}
void object_state(BYTE *o, int c)
{
    const ClassDef &d = CLASSES[c];
    st(dw(o, 0));
    for (DWORD off = 4; off < d.size; off += 4)
        st(off == d.zstring ? text_hash(zs_text(o + off)) : tag(dw(o, off)));
}
void destroy(BYTE *o, int c)
{
    const ClassDef &d = CLASSES[c];
    for (DWORD off : d.owned)
        if (off)
            dw(o, off) = 0;
    original<void *(THISCALL *)(void *, DWORD)>(slot(o, 1))(o, 1);
}

// ---- The case runner -----------------------------------------------------------------------------------------

struct Test {
    const char *name;
    DWORD address;
    int cls;     // a class of CLASSES, C_TRIGGER (oval, polygon or collide by the seed) or C_BUFFER (in place)
    DWORD mask;  // the bits of eax compared
    int count;
    DWORD (*run)(Gen &g, BYTE *self, DWORD address);
    bool consumes;   // the run deletes the object
};
template <typename... A> DWORD call(DWORD address, void *self, A... a) { return at<DWORD(THISCALL *)(void *, A...)>(address)(self, a...); }

int case_class(const Test &t, unsigned seed)
{
    if (t.cls == C_TRIGGER)
        return (int)(seed % 3);   // C_OVAL, C_POLY, C_COLLIDE
    return t.cls;
}
void side_begin(int side, unsigned seed)
{
    g_side = side;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
    g_answers = Gen{seed * 2654435761u + 77};
}
// Values a test notes for the log of a mismatch
DWORD g_diag[2][20];
int g_ndiag[2];
void diag(DWORD v)
{
    if (g_ndiag[g_side] < 20)
        g_diag[g_side][g_ndiag[g_side]] = v;
    g_ndiag[g_side]++;
}
DWORD g_ret[2];
bool run_case(Tally &t, const Test &test, unsigned seed, const BYTE *real)
{
    const int c = case_class(test, seed);
    for (int side = 0; side < 2; side++) {
        side_begin(side, seed);
        g_ndiag[side] = 0;
        Gen g{seed * 69069u + 1};
        build_world(g);
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
        if (c != C_BUFFER && !test.consumes) {
            object_state(self, c);
            destroy(self, c);
        }
        g_self = nullptr;
        fakes_state();
    }
    const bool same = g_ret[0] == g_ret[1] && g_loghash[0] == g_loghash[1] && g_statehash[0] == g_statehash[1];
    char d[192];
    wsprintfA(d, "[0x%x %s %s seed %u: eax %x / %x, events %d / %d, state %x / %x]", test.address, test.name, c >= 0 ? CLASSES[c].name : "",
              seed, g_ret[0], g_ret[1], g_nlog[0], g_nlog[1], g_statehash[0], g_statehash[1]);
    t.check(same, "", d);
    if (!same && t.mismatches <= 10) {
        log_first_difference();
        for (int i = 0; i < g_ndiag[0] && i < g_ndiag[1] && i < 20; i++)
            if (g_diag[0][i] != g_diag[1][i])
                log("    noted %d: %x / %x", i, g_diag[0][i], g_diag[1][i]);
    }
    return same;
}

// ---- The tests ---------------------------------------------------------------------------------------------------

DWORD dt_bits(Gen &g)
{
    const float v[] = {0, 0.1f, 1, 5, 0.016f, 0.5f};
    return as_dword(g.pick(v, 6));
}
DWORD t_allows(Gen &g, BYTE *self, DWORD a)
{
    BYTE *p = g.below(8) ? part(g) : nullptr;
    if (!p)
        by(self, 0x12) = by(self, 0x13) = by(self, 0x14) = 0;   // the original reads the part without a test there
    return call(a, self, part(g), p);
}
DWORD t_mask(Gen &g, BYTE *self, DWORD a)
{
    DWORD m = g.next();
    call(a, self, part(g), &m);
    ev(0x1111, m);
    return 0;
}
DWORD t_fire(Gen &g, BYTE *self, DWORD a)
{
    void *fields[] = {self + 0x20, self + 0x24, self + 0x28, &g_f.cell};
    void *field = fields[g.below(4)];
    return call(a, self, g.below(4) ? g_f.layer : nullptr, field, part(g), g.below(3) ? part(g) : nullptr);
}
DWORD t_read_line(Gen &g, BYTE *self, DWORD a)
{
    const char *names[] = {"Required Name", "required name", "Repeat Every", "X Radius"};
    const char *values[] = {nullptr, "", "Zax", "Door 1"};
    return call(a, self, names[g.below(4)], values[g.below(4)], g.next());
}
DWORD t_set_touching(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g), g.below(4) ? part(g) : nullptr); }
DWORD t_message(Gen &g, BYTE *self, DWORD a)
{
    const char *texts[] = {"Delete", "Leaving Scope", "delete", "LEAVING SCOPE", "Hello"};
    void *from = g.below(2) ? bp(dw(self, 0x30)) : part(g);
    return call(a, self, from, texts[g.below(5)]);
}
DWORD t_update(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g), dt_bits(g)); }
DWORD t_touch(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g), &g_list); }
DWORD t_rect(Gen &g, BYTE *self, DWORD a)
{
    int rect[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
    const DWORD r = call(a, self, part(g), rect);
    ev(0x2222, fnv(2166136261u, rect, 16));
    return r;
}
DWORD t_set_ratio(Gen &g, BYTE *self, DWORD a)
{
    const float v[] = {1, 0.5f, 0, -2, 0.712766f, 3};
    const long double r = at<long double(THISCALL *)(void *, float)>(a)(self, g.pick(v, 6));
    return as_dword((float)r);
}
DWORD t_may_touch(Gen &g, BYTE *self, DWORD a) { return call(a, self, ptrv(part(g)), g.next(), g.next()); }
DWORD t_repeat_begin(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g)); }
DWORD t_repeat_update(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g), dt_bits(g)); }
DWORD t_ctor_args(Gen &g, BYTE *self, DWORD a)
{
    const DWORD r = tag(call(a, self, ptrv(action(g)), as_dword(1.5f), g.next()));
    for (DWORD off = 0; off < 0x20; off += 4)
        st(tag(dw(self, off)));
    dw(self, 0x10) = 0;
    original<void *(THISCALL *)(void *, DWORD)>(0x420440)(self, 0);   // destroyed in place
    return r;
}
DWORD t_spawn(Gen &g, BYTE *self, DWORD a)
{
    BYTE *e = g.below(5) ? part(g) : nullptr;
    if (g.below(2))
        g_first_char = e;
    return call(a, self, part(g), e);
}
DWORD t_relay_target(Gen &g, BYTE *self, DWORD a)
{
    BYTE *target = g.below(6) ? part(g) : nullptr;
    return call(a, self, ptrv(part(g)), g.next(), ptrv(part(g)), target);
}
DWORD t_compass(Gen &g, BYTE *self, DWORD a) { return call(a, self, part(g), dt_bits(g)); }
DWORD t_hud6(Gen &g, BYTE *self, DWORD a) { return call(a, self, g.next(), g.next(), g.next(), g.next(), g.next(), g.next()); }
DWORD t_hud5(Gen &g, BYTE *self, DWORD a) { return call(a, self, g.next(), g.next(), g.next(), g.next(), g.next()); }

// The converting wrappers: the four real global objects and a fake (+8 a text or 0, +0xc/+0x10 the field
// names or 0)
const DWORD WRAPPERS[] = {0x62e978, 0x62e960, 0x62e9a8, 0x62e990};
BYTE g_wrapper[0x14];
BYTE *wrapper(Gen &g)
{
    if (g.below(5))
        return at<BYTE *>(WRAPPERS[g.below(4)]);
    memset(g_wrapper, 0, sizeof g_wrapper);
    dw(g_wrapper, 0) = 0x5edb40;
    dw(g_wrapper, 8) = g.below(2) ? ptrv("A fake wrapper") : 0;
    if (g.below(2)) {
        dw(g_wrapper, 0xc) = 0x60af5c;   // "Enter Action"
        dw(g_wrapper, 0x10) = g.below(2) ? 0x60aed0 : 0x60af5c;   // "Action"
    }
    return g_wrapper;
}
DWORD t_describe(Gen &g, BYTE *self, DWORD a)
{
    CClassInfo *out = reinterpret_cast<CClassInfo *>(0x1234);
    const DWORD r = call(a, wrapper(g), self, g.below(3) ? &out : nullptr);
    ev(0x3333, ptrv(out));
    return r;
}
DWORD t_convert(Gen &g, BYTE *self, DWORD a)
{
    const DWORD owned[] = {0x20, 0x24, 0x28, 0x30};
    for (DWORD off : owned)
        if (off < g_self_size)
            dw(self, off) = 0;   // no owned objects to copy
    if (CLASSES[C_PASS].size == g_self_size)
        dw(self, 0x20) = 0;
    BYTE *made = reinterpret_cast<BYTE *>(call(a, wrapper(g), self));
    g_self = nullptr;   // deleted by the conversion
    const DWORD size = dw(made, 0) == 0x5edb58 ? 0x44 : 0x38;
    st(dw(made, 0));
    // the padding above the byte fields (+8, +0x14, +0x1f) is left as the pool gave the block
    for (DWORD off = 4; off < size; off += 4) {
        const DWORD mask = off == 8 || off == 0x14 ? 0xff : off == 0x1c ? 0xffffff : 0xffffffff;
        const DWORD v = off == 0x18 ? text_hash(zs_text(made + off)) : dw(made, off) & mask;
        st(v);
        diag(v);
    }
    vcall<void *>(made, 1, 1);
    return made ? 1 : 0;
}

// The property list (pool 0x18 as 0x41e870 builds it: +0 0x5ed448, +4 the pointer array 0x5ed698, +0x14
// the file name) with 0-4 items made by the original 0x41e620
const char *const LINE_NAMES[] = {"Intro", "Answer", "Zax", "intro", "Missing"};
const char *const LINE_TEXTS[] = {"Hello there.", "", "Two\nlines", "Trailing   "};
BYTE *make_list(Gen &g)
{
    BYTE *pl = static_cast<BYTE *>(pool_alloc(0x18));
    memset(pl, 0, 0x18);
    dw(pl, 0) = 0x5ed448;
    dw(pl, 4) = 0x5ed698;
    zs_ctor(pl + 0x14);
    zs_set(pl + 0x14, "Levels/Test/Fake Script.RDS");
    const DWORD k = g.below(5);
    for (DWORD i = 0; i < k; i++) {
        const DWORD idx = original<DWORD(THISCALL *)(void *, const char *)>(0x41e620)(pl, i ? LINE_NAMES[g.below(4)] : "!None");
        BYTE *item = static_cast<BYTE **>(static_cast<void *>(bp(dw(pl, 8))))[idx];
        zs_set(item + 0xc, LINE_TEXTS[g.below(4)]);
    }
    return pl;
}
void list_state(BYTE *pl)
{
    st(dw(pl, 0) ^ dw(pl, 4) * 3);
    st(dw(pl, 0xc));
    st(dw(pl, 0x10));
    st(text_hash(zs_text(pl + 0x14)));
    for (DWORD i = 0; i < dw(pl, 0xc); i++) {
        BYTE *item = static_cast<BYTE **>(static_cast<void *>(bp(dw(pl, 8))))[i];
        st(dw(item, 0));
        st(dw(item, 0x10) == ptrv(pl));
        st(text_hash(zs_text(item + 8)));
        st(text_hash(zs_text(item + 0xc)));
    }
}
// The file buffer's stand-ins: a script of lines from the seed, writes logged
const char *const SCRIPT[] = {":Intro", "Hello there.", "  second line  ", "// a comment", "/not a comment", "", ":Answer", "Yes", ":",
                              "text\tafter a tab", ":Intro", "duplicate", LONG_TEXT, "trailing \t ", ":Last"};
const char *g_script[24];
int g_nscript, g_script_pos;
bool g_open_ok;
void *THISCALL fb_ctor(BYTE *b)
{
    ev(0xfb00);
    return b;
}
DWORD THISCALL fb_dtor(BYTE *)
{
    ev(0xfb01);
    return 0;
}
bool THISCALL fb_open(BYTE *, const char *path, DWORD flags)
{
    ev(0xfb02, text_hash(path), flags, g_open_ok);
    g_script_pos = 0;
    return g_open_ok;
}
DWORD THISCALL fb_read_line(BYTE *, char *p, DWORD n)
{
    ev(0xfb03, n);
    if (g_script_pos >= g_nscript)
        return 0;
    const char *s = g_script[g_script_pos++];
    DWORD i = 0;
    for (; s[i] && i + 1 < n; i++)
        p[i] = s[i];
    p[i] = 0;
    return 1;
}
DWORD THISCALL fb_write_text(BYTE *, const char *s)
{
    ev(0xfb04, text_hash(s));
    return 0;
}
DWORD THISCALL fb_write_line(BYTE *, const char *s)
{
    ev(0xfb05, text_hash(s));
    return 0;
}
DWORD THISCALL fb_close(BYTE *)
{
    ev(0xfb06);
    return 0;
}
const Fn FILE_FNS[] = {{0x4a1780, (void *)&fb_ctor},       {0x4a1800, (void *)&fb_dtor},       {0x4a1840, (void *)&fb_open},
                       {0x4a20a0, (void *)&fb_read_line},  {0x4a1a10, (void *)&fb_write_text}, {0x4a1a40, (void *)&fb_write_line},
                       {0x4a2100, (void *)&fb_close}};
CodePatch g_file_patches[7];
template <typename F> DWORD with_list(Gen &g, F f)
{
    g_nscript = (int)g.below(16);
    for (int i = 0; i < g_nscript; i++)
        g_script[i] = SCRIPT[g.below(sizeof SCRIPT / sizeof SCRIPT[0])];
    g_open_ok = g.below(6) != 0;
    BYTE *pl = make_list(g);
    for (int i = 0; i < 7; i++)
        patch_jump(g_file_patches[i], FILE_FNS[i].address, FILE_FNS[i].standin);
    const DWORD r = f(pl);
    for (int i = 7; i-- > 0;)
        unpatch(g_file_patches[i]);
    list_state(pl);
    at<void *(THISCALL *)(void *, DWORD)>(0x41e020)(pl, 1);
    return r;
}
DWORD zs_by_value(const char *s)
{
    DWORD z;
    zs_ctor(&z);
    zs_set(&z, s);
    return z;
}
DWORD t_load(Gen &g, BYTE *, DWORD a)
{
    return with_list(g, [&](BYTE *pl) { return call(a, pl, zs_by_value(g.below(2) ? "Levels/Test/Other.RDS" : "x.rds")); });
}
DWORD t_save_as(Gen &g, BYTE *, DWORD a)
{
    return with_list(g, [&](BYTE *pl) { return call(a, pl, zs_by_value("Levels/Test/Saved.RDS")); });
}
DWORD t_save(Gen &g, BYTE *, DWORD a) { return with_list(g, [&](BYTE *pl) { return call(a, pl); }); }
DWORD t_lookup(Gen &g, BYTE *, DWORD a)
{
    return with_list(g, [&](BYTE *pl) {
        DWORD out;
        zs_ctor(&out);
        call(a, pl, LINE_NAMES[g.below(5)], g.below(4) ? "The default" : "", &out);
        ev(0x4444, text_hash(zs_text(&out)));
        zs_dtor(&out);
        return 0u;
    });
}
DWORD t_name_at(Gen &g, BYTE *, DWORD a)
{
    return with_list(g, [&](BYTE *pl) { return text_hash(reinterpret_cast<const char *>(call(a, pl, g.below(6)))); });
}
DWORD t_at(Gen &g, BYTE *, DWORD a)
{
    return with_list(g, [&](BYTE *pl) {
        const DWORD r = call(a, pl, g.below(6));
        for (DWORD i = 0; i < dw(pl, 0xc); i++)
            if (r == dw(bp(dw(pl, 8)), i * 4))
                return (DWORD)(i + 1);
        return r ? (DWORD)0xffff : (DWORD)0;
    });
}
DWORD t_add_named(Gen &g, BYTE *, DWORD a)
{
    return with_list(g, [&](BYTE *pl) { return call(a, pl, g.below(4) ? LINE_NAMES[g.below(5)] : nullptr); });
}

const Test TESTS[] = {
    {"Allows", 0x422d50, C_TRIGGER, 0xff, 300, t_allows, false},
    {"Mask", 0x422e40, C_TRIGGER, 0, 120, t_mask, false},
    {"Fire", 0x422cd0, C_TRIGGER, 0xff, 300, t_fire, false},
    {"ReadLine", 0x422c70, C_TRIGGER, 0xffffffff, 120, t_read_line, false},
    {"SetTouching", 0x4226f0, C_OVAL, 0, 80, t_set_touching, false},
    {"Message", 0x422680, C_POLY, 0xffffffff, 120, t_message, false},
    {"Update (oval)", 0x422510, C_OVAL, 0, 400, t_update, false},
    {"Update (polygon)", 0x422510, C_POLY, 0, 400, t_update, false},
    {"Oval touch", 0x421320, C_OVAL, 0, 200, t_touch, false},
    {"Oval rect", 0x421390, C_OVAL, 0xffffffff, 200, t_rect, false},
    {"Oval ratio", 0x421410, C_OVAL, 0xffffffff, 60, t_set_ratio, false},
    {"Polygon touch", 0x421ac0, C_POLY, 0, 200, t_touch, false},
    {"Polygon rect", 0x421b60, C_POLY, 0xffffffff, 60, t_rect, false},
    {"Polygon may touch", 0x4218c0, C_POLY, 0xff, 20, t_may_touch, false},
    {"Repeat begin", 0x420880, C_REPEAT, 0, 120, t_repeat_begin, false},
    {"Repeat update", 0x4208c0, C_REPEAT, 0, 300, t_repeat_update, false},
    {"Repeat constructor", 0x420840, C_BUFFER, 0xffffffff, 40, t_ctor_args, false},
    {"Spawn", 0x420d50, C_SPAWN, 0, 300, t_spawn, false},
    {"Relay target", 0x420290, C_TRELAY, 0xff, 200, t_relay_target, false},
    {"Compass", 0x4230b0, C_COMPASS, 0xffffffff, 80, t_compass, false},
    {"HUD slot 24", 0x4236c0, C_BUFFER, 0xffffffff, 10, t_hud6, false},
    {"HUD slot 29", 0x4236d0, C_BUFFER, 0xffffffff, 10, t_hud5, false},
    {"Describe touching to oval", 0x4214b0, C_TRIGGER, 0xffffffff, 60, t_describe, false},
    {"Describe collide to oval", 0x4215f0, C_TRIGGER, 0xffffffff, 60, t_describe, false},
    {"Describe touching to polygon", 0x421c20, C_TRIGGER, 0xffffffff, 60, t_describe, false},
    {"Describe collide to polygon", 0x421d60, C_TRIGGER, 0xffffffff, 60, t_describe, false},
    {"Describe (pass-through)", 0x4214b0, C_PASS, 0xffffffff, 20, t_describe, false},
    {"Describe (relay)", 0x421d60, C_RELAY, 0xffffffff, 20, t_describe, false},
    {"Convert to oval", 0x421730, C_TRIGGER, 0xffffffff, 90, t_convert, true},
    {"Convert to polygon", 0x421ea0, C_TRIGGER, 0xffffffff, 90, t_convert, true},
    {"Property list Load", 0x41e0f0, C_BUFFER, 0xff, 200, t_load, false},
    {"Property list SaveAs", 0x41e380, C_BUFFER, 0xff, 60, t_save_as, false},
    {"Property list Save", 0x41e360, C_BUFFER, 0xff, 40, t_save, false},
    {"Property list Lookup", 0x41e5a0, C_BUFFER, 0, 120, t_lookup, false},
    {"Property list NameAt", 0x41e550, C_BUFFER, 0xffffffff, 40, t_name_at, false},
    {"Property list At", 0x41e580, C_BUFFER, 0xffffffff, 40, t_at, false},
    {"Property list AddNamed", 0x41e620, C_BUFFER, 0xffffffff, 40, t_add_named, false},
};

void test_slots(Tally &t)
{
    for (const Test &test : TESTS)
        for (int k = 0; k < scaled(test.count); k++)
            run_case(t, test, 1000u + (unsigned)k * 7919u + test.address % 1013, nullptr);
}

// ---- Deleting destructors -------------------------------------------------------------------------------------

void test_dtors(Tally &t)
{
    for (int c = 0; c < NCLS; c++)
        for (int k = 0; k < scaled(16) && c != C_COLLIDE && c != C_PASS && c != C_RELAY; k++) {
            const DWORD flags = (DWORD)k % 4, count = (DWORD)k / 4 % 4;
            DWORD r[2], vt[2];
            for (int side = 0; side < 2; side++) {
                side_begin(side, 5000u + (unsigned)k);
                Gen g{(unsigned)k * 31u + (unsigned)c};
                build_world(g);
                BYTE *obj;
                DWORD *block = nullptr;
                if (flags & 2) {
                    obj = static_cast<BYTE *>(original<void *(__fastcall *)(int, int, int)>(CLASSES[c].create_array)((int)count, 0, 0));
                    block = reinterpret_cast<DWORD *>(obj) - 1;
                } else {
                    obj = static_cast<BYTE *>(original<void *(__cdecl *)()>(CLASSES[c].create)());
                    if (c == C_HOLDER)
                        dw(obj, 8) = g.below(2) ? ptrv(g_f.item) : 0;
                }
                if (side == 0)
                    hooks_out();
                world_on();
                void *res = at<void *(THISCALL *)(void *, DWORD)>(CLASSES[c].dtor)(obj, flags);
                world_off();
                if (side == 0)
                    hooks_in();
                r[side] = res == obj ? 1 : res == block ? 2 : 3;
                vt[side] = 0;
                if (!(flags & 1)) {
                    vt[side] = (flags & 2) ? (count ? dw(obj, 0) : 0) : dw(obj, 0);
                    if (flags & 2)
                        heap_free(block);
                    else
                        pool_free(obj, CLASSES[c].size);
                }
            }
            char d[128];
            wsprintfA(d, "[0x%x flags %u count %u: %u / %u, vtable %x / %x, events %d / %d]", CLASSES[c].dtor, flags, count, r[0], r[1], vt[0], vt[1],
                      g_nlog[0], g_nlog[1]);
            t.check(r[0] == r[1] && vt[0] == vt[1] && g_loghash[0] == g_loghash[1], CLASSES[c].name, d);
        }
    // The abstract levels: CTouchingTriggerAI's deleting destructor 0x422440 and destructor 0x4224b0 on an oval
    // trigger (the touched part let go through the listener stand-in), CInventoryHUDRenderer's 0x4236e0 and the
    // wrappers' base 0x41a0c0 on blocks of their own
    for (int k = 0; k < scaled(24); k++) {
        const int kind = k % 4;
        const DWORD flags = (DWORD)(k / 4) % 2;
        DWORD r[2], vt[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side, 6000u + (unsigned)k);
            Gen g{(unsigned)k * 17u};
            build_world(g);
            BYTE *obj;
            DWORD fn, size;
            if (kind <= 1) {
                obj = static_cast<BYTE *>(original<void *(__cdecl *)()>(CLASSES[C_OVAL].create)());
                dw(obj, 0x30) = g.below(2) ? ptrv(part(g)) : 0;
                fn = kind ? 0x4224b0 : 0x422440;
                size = 0x44;
            } else if (kind == 2) {
                obj = static_cast<BYTE *>(pool_alloc(8));
                at<void *(THISCALL *)(void *)>(0x416610)(obj);   // CObject's constructor
                dw(obj, 0) = 0x5edf7c;
                fn = 0x4236e0;
                size = 8;
            } else {
                obj = static_cast<BYTE *>(heap_alloc(0x14));
                memset(obj, 0, 0x14);
                fn = 0x41a0c0;
                size = 0x14;
            }
            g_self = obj;
            g_self_size = size;
            if (side == 0)
                hooks_out();
            world_on();
            const DWORD res = kind == 1 ? (call(fn, obj), ptrv(obj)) : call(fn, obj, flags);
            world_off();
            if (side == 0)
                hooks_in();
            g_self = nullptr;
            r[side] = res == ptrv(obj);
            vt[side] = 0;
            if (kind == 1 || !(flags & 1)) {
                vt[side] = dw(obj, 0);
                if (kind == 3)
                    heap_free(obj);
                else
                    pool_free(obj, size);
            }
        }
        char d[128];
        wsprintfA(d, "[kind %d flags %u: %u / %u, vtable %x / %x, events %d / %d]", kind, flags, r[0], r[1], vt[0], vt[1], g_nlog[0], g_nlog[1]);
        t.check(r[0] == r[1] && vt[0] == vt[1] && g_loghash[0] == g_loghash[1], "abstract levels", d);
    }
}

// ---- Class getters, RegisterFields, the descriptor's clone, Create and CreateArray ------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent, DWORD size,
                                   DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), as_dword(version), ptrv(parent), size, abstract & 0xff, ptrv(create), ptrv(create_array),
                  ptrv(register_fields)};
    g_nregister++;
    return self;
}
struct Getter {
    DWORD var, getter;
};
const Getter GETTERS[] = {{0x62e9cc, 0x422780}, {0x62e9c4, 0x421f90}, {0x62e9e0, 0x423050}, {0x62e9e0, 0x422ec0}, {0x62e944, 0x420100},
                          {0x62e944, 0x41ff60}, {0x62e9f8, 0x423f00}, {0x62e7c4, 0x41d0d0}, {0x62e940, 0x41fbd0}, {0x62e98c, 0x420ee0},
                          {0x62e9bc, 0x421870}};
bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    return lstrcmpA(a.c_str(), b.c_str()) == 0;
}
// A string descriptor (0x34, vtable 0x5ed924, StringField_Ctor 0x415dd0): the common part by text, the byte
// +0x28 (its three upper bytes and +0x24 are left as the heap gave them), the default +0x2c (a ZString) by
// text, +0x30
bool same_string_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    return a->vftable == b->vftable && same_text(a->name, b->name) && same_text(a->help, b->help) && same_text(a->group, b->group) &&
           same_text(a->alt_name, b->alt_name) && same_text(a->units, b->units) && a->offset == b->offset && a->unknown14 == b->unknown14 &&
           a->unknown18 == b->unknown18 && x[0x28] == y[0x28] &&
           same_text(*reinterpret_cast<const ZString *>(x + 0x2c), *reinterpret_cast<const ZString *>(y + 0x2c)) && !memcmp(x + 0x30, y + 0x30, 4);
}
void test_registration(Tally &t, Tally &real)
{
    for (const Getter &c : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(c.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<CClassInfo *(__cdecl *)()>(c.getter)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<CClassInfo *(__cdecl *)()>(c.getter)();
            unpatch(p);
            results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
            calls[side] = g_register;
            if (var)
                pool_free(var, sizeof(CClassInfo));
            var = saved;
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, parent %x / %x, version %x / %x]", c.getter, set[0], set[1], results[0], results[1],
                  calls[0].parent, calls[1].parent, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "", d);
    }
    // CTriggerRelayAction's RegisterFields (the class variable set and 0)
    for (int var_zero = 0; var_zero < 2; var_zero++) {
        CFieldDesc *made[2] = {};
        DWORD n[2], result[2], alt[2], reg[2];
        for (int side = 0; side < 2; side++) {
            CClassInfo *&var = *at<CClassInfo **>(0x62e944);
            CClassInfo *const saved = var;
            alignas(4) BYTE fake_class[sizeof(CClassInfo)];
            memset(fake_class, 0, sizeof fake_class);
            var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
            g_nregister = 0;
            if (side == 0)
                hooks_out();
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            const DWORD r = at<DWORD(__cdecl *)()>(0x420160)();
            unpatch(p);
            if (side == 0)
                hooks_in();
            CClassInfo *c = var;
            n[side] = c->num_fields;
            made[side] = c->num_fields ? c->fields[0] : nullptr;
            result[side] = r == ptrv(c) ? 1 : r;
            alt[side] = ptrv(c->alt_name);
            reg[side] = (DWORD)g_nregister;
            if (c->fields)
                heap_free(c->fields);
            if (var_zero)
                pool_free(c, sizeof(CClassInfo));
            var = saved;
        }
        char d[160];
        wsprintfA(d, "[RegisterFields 0x420160 var %d: %u / %u fields, eax %x / %x, alt %x / %x, registered %u / %u]", var_zero, n[0], n[1], result[0],
                  result[1], alt[0], alt[1], reg[0], reg[1]);
        t.check(n[0] == n[1] && n[0] == 1 && result[0] == result[1] && alt[0] == alt[1] && reg[0] == reg[1] && same_string_desc(made[0], made[1]),
                "CTriggerRelayAction", d);
        const CClassInfo *c = *at<CClassInfo **>(0x62e944);
        const bool ok = c && c->num_fields - c->num_inherited == 1 && same_string_desc(c->fields[c->num_inherited], made[0]) &&
                        ptrv(c->alt_name) == alt[0];
        real.check(ok, "CTriggerRelayAction", "[the registered descriptor]");
        for (CFieldDesc *f : made)
            if (f)
                vcall<void *>(f, 0, 1);
    }
    // The "Relay Name" descriptor's clone (slot 1 of 0x5ed924) on the registered descriptor
    const CClassInfo *c = *at<CClassInfo **>(0x62e944);
    if (c && c->num_fields > c->num_inherited) {
        CFieldDesc *src = c->fields[c->num_inherited];
        CFieldDesc *cl[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            cl[side] = at<CFieldDesc *(THISCALL *)(void *)>(0x420260)(src);
            if (side == 0)
                hooks_in();
        }
        real.check(same_string_desc(cl[0], cl[1]) && same_string_desc(cl[0], src), "Relay Name", "[the clone 0x420260]");
        for (CFieldDesc *f : cl)
            if (f)
                vcall<void *>(f, 0, 1);
    }
}
// CInventoryItemHolder's Create and CreateArray (0-3 elements): every byte
void test_objects(Tally &t)
{
    for (int k = 0; k < scaled(16); k++) {
        const DWORD count = (DWORD)k % 4;
        BYTE bytes[2][4 + 3 * 0xc];
        DWORD n[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            BYTE *o = k < 8 ? static_cast<BYTE *>(at<void *(__cdecl *)()>(0x423f50)())
                            : static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(0x423f90)((int)count, 0, 0));
            if (side == 0)
                hooks_in();
            n[side] = k < 8 ? 0xc : 4 + count * 0xc;
            memcpy(bytes[side], k < 8 ? o : o - 4, n[side]);
            if (k < 8)
                original<void *(THISCALL *)(void *, DWORD)>(0x423ff0)(o, 1);
            else
                original<void *(THISCALL *)(void *, DWORD)>(0x423ff0)(o, 3);
        }
        char d[96];
        wsprintfA(d, "[%s count %u]", k < 8 ? "Create" : "CreateArray", count);
        t.check(n[0] == n[1] && !memcmp(bytes[0], bytes[1], n[0]), "CInventoryItemHolder", d);
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------------

BYTE *g_parts[8192];
void test_real(Tally &t)
{
    const int n = aiw::level_parts(g_parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_triggers real data: no level grid found");
        return;
    }
    // the AIs of the classes, counted first for the spread
    const int REAL[] = {C_OVAL, C_POLY, C_COLLIDE, C_REPEAT, C_SPAWN, C_COMPASS};
    int total[NCLS] = {}, index[NCLS] = {}, ran = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            BYTE *ais[64];
            const int count = aiw::part_ais(g_parts[i], g_parts, n, ais, 64);
            for (int k = 0; k < count; k++) {
                const CClassInfo *ci = class_of(ais[k]);
                for (int c : REAL) {
                    const CClassInfo *want = at<CClassInfo *(__cdecl *)()>(c == C_OVAL      ? 0x421050
                                                                            : c == C_POLY    ? 0x4219f0
                                                                            : c == C_COLLIDE ? 0x41c900
                                                                            : c == C_REPEAT  ? 0x4204e0
                                                                            : c == C_SPAWN   ? 0x420af0
                                                                                             : 0x423050)();
                    if (ci != want || !readable(ais[k], CLASSES[c].size))
                        continue;
                    if (pass == 0) {
                        total[c]++;
                        continue;
                    }
                    if (!spread_pick(index[c]++, total[c], 40))
                        continue;
                    for (const Test &test : TESTS) {
                        const bool trig = c <= C_COLLIDE && (test.address == 0x422d50 || test.address == 0x422e40 || test.address == 0x422cd0);
                        const bool own = (test.cls == c || (test.cls == C_TRIGGER && c <= C_COLLIDE)) && !test.consumes &&
                                         test.address != 0x422c70 && test.address != 0x422680 && test.address != 0x4226f0 &&
                                         (test.address < 0x4214b0 || test.address > 0x421d60);
                        if (!trig && !own)
                            continue;
                        Test real_test = test;
                        real_test.cls = c;
                        run_case(t, real_test, 0x9e3779b9u ^ ptrv(ais[k]) ^ test.address, ais[k]);
                        ran++;
                    }
                }
            }
        }
    log("  ai_triggers real data: %d parts; %d oval, %d polygon, %d collide, %d repeat timer, %d spawn point, %d compass AIs; %d cases", n,
        total[C_OVAL], total[C_POLY], total[C_COLLIDE], total[C_REPEAT], total[C_SPAWN], total[C_COMPASS], ran);
}
// Three .RDS files of the game through the real file buffer (read only): loaded by both sides, compared,
// the lines found by name
void test_real_scripts(Tally &t)
{
    const char *const FILES[] = {"Levels/Global/Inventory Item Text.rds", "Levels/ACT 1/M1M2/Mission01n2.RDS", "Levels/Dialog Script.RDS"};
    for (const char *file : FILES) {
        DWORD loaded[2], count[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side, 7);
            BYTE *pl = static_cast<BYTE *>(pool_alloc(0x18));
            memset(pl, 0, 0x18);
            dw(pl, 0) = 0x5ed448;
            dw(pl, 4) = 0x5ed698;
            zs_ctor(pl + 0x14);
            if (side == 0)
                hooks_out();
            loaded[side] = call(0x41e0f0, pl, zs_by_value(file)) & 0xff;
            count[side] = dw(pl, 0xc);
            // every name looked up (present, so nothing is written)
            for (DWORD i = 0; i < dw(pl, 0xc) && i < 64; i++) {
                const char *name = reinterpret_cast<const char *>(call(0x41e550, pl, i));
                DWORD out;
                zs_ctor(&out);
                call(0x41e5a0, pl, name, "unused default", &out);
                st(text_hash(zs_text(&out)));
                zs_dtor(&out);
            }
            if (side == 0)
                hooks_in();
            list_state(pl);
            at<void *(THISCALL *)(void *, DWORD)>(0x41e020)(pl, 1);
        }
        char d[160];
        wsprintfA(d, "[%s: loaded %u / %u, %u / %u lines]", file, loaded[0], loaded[1], count[0], count[1]);
        t.check(loaded[0] == loaded[1] && count[0] == count[1] && g_statehash[0] == g_statehash[1], "property list", d);
        log("  ai_triggers real script %s: loaded %u, %u lines", file, loaded[0], count[0]);
    }
}

}  // namespace

ZAX_CHECKS(ai_triggers)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally reg{"ai triggers: class getters, RegisterFields, Create and CreateArray"},
        real_reg{"ai triggers: the registered descriptor and its clone (real data)"}, dtors{"ai triggers: deleting destructors"},
        slots{"ai triggers: slots and helpers in the fake world"}, real{"ai triggers: the level's trigger AIs through their slots (real data)"},
        scripts{"ai triggers: the game's .RDS scripts through the property list (real data)"};
    init_vtables();
    test_registration(reg, real_reg);
    test_objects(reg);
    reg.report();
    real_reg.report();
    test_dtors(dtors);
    dtors.report();
    test_slots(slots);
    slots.report();
    test_real(real);
    real.report();
    test_real_scripts(scripts);
    scripts.report();
    log("  ai triggers: %u ms", GetTickCount() - start);
}

}  // namespace zax
