// Checks of the AI classes 0x4ca590-0x4cbfff (ai_slots.cpp) against the original, in the game thread.
//  - Class getters (the class variable set and 0; ClassInfo_Register a stand-in recording its arguments)
//    and RegisterFields into a fake class info (descriptors compared member by member, texts by content,
//    an object array's element descriptor too); real data: the descriptors registered at startup.
//  - Deleting destructors (flags 0-3, objects from the original Create and CreateArray).
//  - The behaviour slots, with the pattern of checks_action_slots.cpp: both sides from the same seed (side
//    0 with the module's hooks out), real AIs with fake parts, child AIs, actions and models; the embedded
//    behaviour of CPlaySequenceLoopedAI gets a fake vtable for the case; compared eax, an event log and
//    the AI afterwards.
#include "check_util.h"
#include "field_desc.h"
#include "field_kinds.h"

WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_ai_slots(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }

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
    const int n = hooks_ai_slots(nullptr);
    if (!hook_sites_fit("ai_slots", n, MAX_SITES))
        return;
    hooks_ai_slots(list);
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

// ---- The classes ------------------------------------------------------------------------------------------------

struct AiClass {
    const char *name;
    DWORD var, getter, register_fields, create, create_array, size, dtor;
};
const AiClass CLASSES[] = {
    {"CGravityAI", 0x6bfe50, 0x4ca630, 0x4ca690, 0x4ca4e0, 0x4ca520, 0x18, 0x4ca590},
    {"CDrawTimeoutOvalAI", 0x6bfe54, 0x4cab40, 0x4caba0, 0x4ca9c0, 0x4caa10, 0x24, 0x4caa90},
    {"CDrawTimeoutRectAI", 0x6bfe58, 0x4cb020, 0x4cb080, 0x4cae90, 0x4caee0, 0x2c, 0x4caf60},
    {"CPlaySequenceLoopedAI", 0x6bfe5c, 0x4cb420, 0x4cb480, 0x4cb280, 0x4cb2d0, 0x28, 0x4cb350},
    {"CSimultaneousAIsAI", 0x6bfe60, 0x4cb800, 0x4cb860, 0x4cb6b0, 0x4cb6f0, 0x18, 0x4cb760},
    {"CWhileAI", 0x6bfe64, 0x4cbc80, 0x4cbce0, 0x4cbb30, 0x4cbb70, 0x1c, 0x4cbbe0},
};

// ---- Class getters and RegisterFields ----------------------------------------------------------------------------

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
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create), ptrv(create_array),
                  ptrv(register_fields)};
    g_nregister++;
    return self;
}

bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    return lstrcmpA(a.c_str(), b.c_str()) == 0;
}
// Descriptors of the kinds these RegisterFields make, member by member (texts by content; +0x24 is not
// written by the constructors)
bool same_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || !same_text(a->name, b->name) || !same_text(a->help, b->help) || !same_text(a->group, b->group) ||
        !same_text(a->alt_name, b->alt_name) || !same_text(a->units, b->units) || a->offset != b->offset ||
        a->unknown14 != b->unknown14 || a->unknown18 != b->unknown18)
        return false;
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    auto same = [&](DWORD from, DWORD to) { return memcmp(x + from, y + from, to - from) == 0; };
    switch (a->vftable) {
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_EMBEDDED: return same(0x28, 0x30);
    case VT_OBJECT_ARRAY:   // +0x38 the element descriptor
        return same(0x28, 0x38) && same(0x3c, 0x45) &&
               same_desc(*reinterpret_cast<CFieldDesc *const *>(x + 0x38), *reinterpret_cast<CFieldDesc *const *>(y + 0x38));
    }
    return false;
}

constexpr DWORD MAXF = 8;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, result, alt_name, registered;
};
void run_register_fields(const AiClass &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    memset(&g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    const DWORD r = at<DWORD(__cdecl *)()>(rf.register_fields)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
    out.alt_name = ptrv(c->alt_name);
    out.registered = (DWORD)g_nregister;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    const DWORD last = c->num_fields ? ptrv(c->fields[c->num_fields - 1]) : 0;
    out.result = r == ptrv(c) ? 'C' : last && r >= last && r < last + 0x48 ? 0xd0000000u + (r - last) : r;
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < MAXF; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_registration(Tally &t, Tally &real)
{
    for (const AiClass &c : CLASSES) {
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
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, parent %x / %x, version %x / %x]", c.getter, set[0], set[1], results[0],
                  results[1], calls[0].parent, calls[1].parent, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                c.name, d);
    }
    for (const AiClass &rf : CLASSES)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            RegisterCall calls[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                calls[side] = g_register;
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n && m[0].result == m[1].result && m[0].alt_name == m[1].alt_name &&
                        m[0].registered == m[1].registered && memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0;
            for (DWORD i = 0; i < m[0].n && i < MAXF && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, alt name %x / %x, registered %u / %u]", rf.name, var_zero,
                      m[0].n, m[1].n, m[0].result, m[1].result, m[0].alt_name, m[1].alt_name, m[0].registered, m[1].registered);
            t.check(same, rf.name, d);
            if (!same && t.mismatches <= 2)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
                    const BYTE *x = reinterpret_cast<const BYTE *>(m[0].fields[i]), *y = reinterpret_cast<const BYTE *>(m[1].fields[i]);
                    for (DWORD off = 0x24; off < 0x48; off += 4)
                        if (memcmp(x + off, y + off, 4))
                            log("    field %u +%x: %x / %x", i, off, *reinterpret_cast<const DWORD *>(x + off), *reinterpret_cast<const DWORD *>(y + off));
                }
            // Real data: the class's own descriptors as registered at startup, and its alternative name
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == m[0].n && ptrv(c->alt_name) == m[0].alt_name;
            for (DWORD i = 0; same_real && i < m[0].n && i < MAXF; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[0].fields[i]);
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c ? c->num_fields - c->num_inherited : 0, m[0].n);
            real.check(same_real, rf.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
}

// ---- The behaviour slots ------------------------------------------------------------------------------------------

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
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }

constexpr DWORD CLS_ACTION = 0x41a650, CLS_BASE = 0x487620, CLS_ANIMATED = 0x48dcc0, CLS_MOVABLE = 0x4903f0;
CClassInfo *cls_at(DWORD getter) { return at<CClassInfo *(__cdecl *)()>(getter)(); }

// Fake parts (entities; slot 2 a real class) with vectors for the gravity; fake child AIs and actions
constexpr int NFE = 6, NFAI = 6, NFA = 6;
alignas(16) BYTE g_fe[NFE][0x100];
alignas(16) BYTE g_fai[NFAI][0x20];
alignas(16) BYTE g_fa[NFA][0x20];
alignas(16) BYTE g_model[0x40];
alignas(16) BYTE g_layer[0x100];
CClassInfo *g_fe_cls[NFE];
float g_vel[NFE][3], g_acc[NFE][3];
DWORD g_heights[NFE][4];
int g_hcount[NFE];
DWORD g_vt_entity[130], g_vt_ai[40], g_vt_action[30], g_vt_behaviour[30];

DWORD tag(DWORD v)
{
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    if (in(g_fe, sizeof g_fe))
        return 0xf2000000u + (v - ptrv(g_fe));
    if (in(g_fai, sizeof g_fai))
        return 0xf4000000u + (v - ptrv(g_fai));
    if (in(g_fa, sizeof g_fa))
        return 0xf1000000u + (v - ptrv(g_fa));
    if (in(g_layer, sizeof g_layer))
        return 0xf5000000u + (v - ptrv(g_layer));
    return v;
}
DWORD tagp(const void *p) { return tag(ptrv(p)); }
int fe_index(const BYTE *e) { return (int)((ptrv(e) - ptrv(g_fe)) / 0x100) % NFE; }

CClassInfo *THISCALL fe_class(BYTE *e) { return g_fe_cls[fe_index(e)]; }
float *THISCALL fe_vel(BYTE *e)
{
    ev(0xe07c, tagp(e));
    return g_vel[fe_index(e)];
}
float *THISCALL fe_acc(BYTE *e)
{
    ev(0xe072, tagp(e));
    return g_acc[fe_index(e)];
}
float THISCALL fe_height(BYTE *e)
{
    const int i = fe_index(e);
    ev(0xe037, tagp(e), (DWORD)g_hcount[i]);
    float h;
    memcpy(&h, &g_heights[i][g_hcount[i]++ & 3], 4);
    return h;
}
template <int S> void THISCALL fe_vector(BYTE *e, float *v) { ev(0xe000 + S, tagp(e), bits(v[0]) ^ bits(v[1]) * 3, bits(v[2])); }
template <int S> void THISCALL fe_trap(BYTE *e) { ev(0xe0ff, tagp(e), S); }

template <int S> DWORD THISCALL ai_slot1(BYTE *a, DWORD x) { ev(0xc000 + S, tagp(a), tag(x)); return 0x1100 + (g_calls & 0xff); }
template <int S> DWORD THISCALL ai_slot2(BYTE *a, DWORD x, DWORD y)
{
    ev(0xc000 + S, tagp(a), tag(x), y);
    return (g_calls & 1) | 0x2200;
}
DWORD THISCALL ai_slot34(BYTE *a, DWORD x, DWORD y, DWORD z, DWORD w, DWORD v)
{
    ev(0xc022, tagp(a), tag(x) ^ y * 3, z ^ w * 5 ^ v * 7);
    return 0;
}
template <int S> void THISCALL ai_trap(BYTE *a) { ev(0xc0ff, tagp(a), S); }
DWORD THISCALL fa_execute(BYTE *a, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    ev(0xa017, tagp(a), tagp(layer) ^ tag(a2), tag(a3) ^ tag(a4) * 3);
    return dw(a, 4);
}
template <int S> void THISCALL fa_trap(BYTE *a) { ev(0xa0ff, tagp(a), S); }
DWORD THISCALL bh_begin(BYTE *, DWORD part)
{
    ev(0xb017, tag(part));
    return 0;
}
DWORD THISCALL bh_update(BYTE *, DWORD dt, DWORD part)
{
    ev(0xb019, dt, tag(part));
    return g_calls & 1;
}
template <int S> void THISCALL bh_trap(BYTE *) { ev(0xb0ff, S); }

template <int... S> struct Seq {};
template <int N, int... S> struct MakeSeq : MakeSeq<N - 1, N - 1, S...> {};
template <int... S> struct MakeSeq<0, S...> {
    using type = Seq<S...>;
};
template <int... S> void fill_entity(Seq<S...>) { ((g_vt_entity[S] = ptrv((void *)&fe_trap<S>)), ...); }
template <int... S> void fill_ai(Seq<S...>) { ((g_vt_ai[S] = ptrv((void *)&ai_trap<S>)), ...); }
template <int... S> void fill_action(Seq<S...>) { ((g_vt_action[S] = ptrv((void *)&fa_trap<S>)), ...); }
template <int... S> void fill_behaviour(Seq<S...>) { ((g_vt_behaviour[S] = ptrv((void *)&bh_trap<S>)), ...); }
void init_vtables()
{
    fill_entity(typename MakeSeq<130>::type{});
    fill_ai(typename MakeSeq<40>::type{});
    fill_action(typename MakeSeq<30>::type{});
    fill_behaviour(typename MakeSeq<30>::type{});
    g_vt_entity[2] = ptrv((void *)&fe_class);
    g_vt_entity[55] = ptrv((void *)&fe_height);
    g_vt_entity[107] = ptrv((void *)&fe_vector<107>);
    g_vt_entity[114] = ptrv((void *)&fe_acc);
    g_vt_entity[118] = ptrv((void *)&fe_vector<118>);
    g_vt_entity[124] = ptrv((void *)&fe_vel);
    g_vt_ai[24] = ptrv((void *)&ai_slot1<24>);
    g_vt_ai[25] = ptrv((void *)&ai_slot2<25>);
    g_vt_ai[29] = ptrv((void *)&ai_slot2<29>);
    g_vt_ai[34] = ptrv((void *)&ai_slot34);
    g_vt_action[23] = ptrv((void *)&fa_execute);
    g_vt_behaviour[23] = ptrv((void *)&bh_begin);
    g_vt_behaviour[25] = ptrv((void *)&bh_update);
}

void setup(unsigned seed)
{
    g_seed = seed * 2654435761u + 12345;
    CClassInfo *const classes[] = {cls_at(CLS_MOVABLE), cls_at(CLS_ANIMATED), cls_at(CLS_BASE), cls_at(CLS_ACTION)};
    const float vz[] = {-321.52231f, 0, 5, -10, 2.5f};
    const float az[] = {-1, -9.5f, 0, 3, -0.25f};
    const float hs[] = {0, 10, -5, 20, 0.5f, 100};
    for (int i = 0; i < NFE; i++) {
        memset(g_fe[i], 0, 0x100);
        dw(g_fe[i], 0) = ptrv(g_vt_entity);
        dw(g_fe[i], 0x48) = ptrv(g_layer);
        g_fe_cls[i] = classes[random() % 4];
        g_vel[i][0] = (float)(int)(random() % 9);
        g_vel[i][1] = 1.5f;
        g_vel[i][2] = vz[random() % 5];
        g_acc[i][0] = 0.5f;
        g_acc[i][1] = (float)(int)(random() % 5);
        g_acc[i][2] = az[random() % 5];
        for (DWORD &h : g_heights[i])
            h = bits(hs[random() % 6]);
        g_hcount[i] = 0;
    }
    for (int i = 0; i < NFAI; i++) {
        memset(g_fai[i], 0, 0x20);
        dw(g_fai[i], 0) = ptrv(g_vt_ai);
    }
    for (int i = 0; i < NFA; i++) {
        memset(g_fa[i], 0, 0x20);
        dw(g_fa[i], 0) = ptrv(g_vt_action);
        dw(g_fa[i], 4) = random() % 2 ? 0x101 : 0x200;
    }
    memset(g_model, 0, sizeof g_model);
    const float bounces[] = {0.5f, 0, 1, 0.75f};
    fl(g_model, 0x34) = bounces[random() % 4];
}

BYTE *THISCALL sr_model(BYTE *e)
{
    ev(0x48b290, tagp(e));
    return g_model;
}
float THISCALL sr_render_height(BYTE *e, float h)
{
    ev(0x48b770, tagp(e), bits(h));
    return h;
}
void THISCALL sr_died(BYTE *e) { ev(0x4fbf30, tagp(e)); }
DWORD THISCALL sr_draw(BYTE *ai, DWORD a, DWORD b)
{
    ev(0x4fd0b0, tagp(ai), a, b);
    return 0;
}
DWORD THISCALL sr_behaviour_message(BYTE *, BYTE *part, DWORD m)
{
    ev(0x499f30, tagp(part), m);
    return 0x9900 | (m & 1);
}
DWORD THISCALL sr_on_message(void *, DWORD a, DWORD b)
{
    ev(0x46cd50, tag(a), b);
    return 0;
}
struct Fn {
    DWORD address;
    void *standin;
};
const Fn FNS[] = {{0x48b290, (void *)&sr_model}, {0x48b770, (void *)&sr_render_height}, {0x4fbf30, (void *)&sr_died},
                  {0x4fd0b0, (void *)&sr_draw},  {0x499f30, (void *)&sr_behaviour_message}, {0x46cd50, (void *)&sr_on_message}};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
CodePatch g_patches[NFN];

struct Slot {
    int cls;   // index into CLASSES
    DWORD address;
    int args;  // 1, 2 or 5
    DWORD mask;
};
const Slot SLOTS[] = {
    {0, 0x4ca7b0, 2, 0},          {1, 0x4cae00, 2, 0},          {2, 0x4cb1f0, 2, 0},          {3, 0x4cb590, 1, 0},
    {3, 0x4cb5c0, 2, 0},          {3, 0x4cb620, 2, 0xffffffff}, {4, 0x4cb990, 5, 0},          {4, 0x4cb9e0, 2, 0xff},
    {4, 0x4cba30, 1, 0},          {4, 0x4cba60, 2, 0},          {4, 0x4cbaa0, 2, 0},          {5, 0x4cbe90, 1, 0},
    {5, 0x4cbeb0, 2, 0},          {5, 0x4cbf10, 2, 0},          {5, 0x4cbf30, 5, 0},          {5, 0x4cbf60, 2, 0xff},
};

DWORD g_ret[2];
void test_slots(Tally &t)
{
    for (const Slot &s : SLOTS) {
        const AiClass &c = CLASSES[s.cls];
        for (int k = 0; k < scaled(80); k++) {
            const unsigned seed = 2000u + (unsigned)k * 13 + s.address % 997;
            for (int side = 0; side < 2; side++) {
                g_side = side;
                g_loghash[side] = g_statehash[side] = 2166136261u;
                g_nlog[side] = 0;
                g_calls = 0;
                setup(seed);
                BYTE *ai = static_cast<BYTE *>(original<void *(__cdecl *)()>(c.create)());
                // fields from the seed
                DWORD saved_behaviour = 0;
                void **children = nullptr;
                switch (s.cls) {
                case 0:
                    dw(ai, 0x10) = random() % 3 ? ptrv(g_fa[random() % NFA]) : 0;
                    fl(ai, 0x14) = (float)(int)(random() % 30) - 5;
                    break;
                case 1:
                    fl(ai, 0x18) = (float)(int)(random() % 8) * 0.25f - 0.5f;
                    break;
                case 2:
                    fl(ai, 0x20) = (float)(int)(random() % 8) * 0.25f - 0.5f;
                    break;
                case 3:
                    saved_behaviour = dw(ai, 0x10);
                    dw(ai, 0x10) = ptrv(g_vt_behaviour);
                    dw(ai, 0x24) = random() % 3 ? ptrv(g_fa[random() % NFA]) : 0;
                    break;
                case 4: {
                    const DWORD n = random() % 5;
                    children = static_cast<void **>(pool_get_alloc(8 * 4));
                    for (DWORD i = 0; i < 8; i++)
                        children[i] = i < n && random() % 4 ? g_fai[random() % NFAI] : nullptr;
                    dw(ai, 0x10) = ptrv(children);
                    dw(ai, 0x14) = n;
                    break;
                }
                default:
                    dw(ai, 0x10) = random() % 3 ? ptrv(g_fa[random() % NFA]) : 0;
                    dw(ai, 0x14) = random() % 3 ? ptrv(g_fai[random() % NFAI]) : 0;
                    dw(ai, 0x18) = random() % 3 ? ptrv(g_fa[random() % NFA]) : 0;
                    break;
                }
                BYTE *part = g_fe[random() % NFE];
                const float dts[] = {0.25f, 1, 0, 0.1f};
                const float dt = dts[random() % 4];
                const DWORD v[5] = {ptrv(part), bits(dt), random(), random() % 3, random() % 2};
                if (side == 0)
                    hooks_out();
                for (int i = 0; i < NFN; i++)
                    patch_jump(g_patches[i], FNS[i].address, FNS[i].standin);
                DWORD r = 0;
                if (s.args == 1)
                    r = at<DWORD(THISCALL *)(void *, DWORD)>(s.address)(ai, v[0]);
                else if (s.args == 2)
                    r = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(s.address)(ai, v[0], v[1]);
                else
                    r = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(s.address)(ai, v[0], v[1], v[2], v[3], v[4]);
                for (int i = NFN; i-- > 0;)
                    unpatch(g_patches[i]);
                if (side == 0)
                    hooks_in();
                g_ret[side] = r & s.mask;
                for (DWORD off = 0x10; off < c.size; off += 4)
                    st(tag(dw(ai, off)));
                // the fakes out again before the destructor (no deletes of fakes)
                switch (s.cls) {
                case 0:
                    dw(ai, 0x10) = 0;
                    break;
                case 3:
                    dw(ai, 0x10) = saved_behaviour;
                    dw(ai, 0x24) = 0;
                    break;
                case 4:
                    dw(ai, 0x10) = 0;
                    dw(ai, 0x14) = 0;
                    pool_get_free(children, 8 * 4);
                    break;
                case 5:
                    dw(ai, 0x10) = dw(ai, 0x14) = dw(ai, 0x18) = 0;
                    break;
                }
                original<void *(THISCALL *)(void *, DWORD)>(slot(ai, 1))(ai, 1);
            }
            const bool same = g_ret[0] == g_ret[1] && g_loghash[0] == g_loghash[1] && g_statehash[0] == g_statehash[1];
            char d[160];
            wsprintfA(d, "[0x%x %s seed %u: eax %x / %x, events %d / %d]", s.address, c.name, seed, g_ret[0], g_ret[1], g_nlog[0], g_nlog[1]);
            t.check(same, "", d);
            if (!same && t.mismatches <= 10)
                log_first_difference();
        }
    }
}

// Deleting destructors (flags 0-3) of objects from the original Create and CreateArray
void test_dtors(Tally &t)
{
    for (const AiClass &c : CLASSES)
        for (int k = 0; k < scaled(12); k++) {
            const DWORD flags = (DWORD)k % 4;
            DWORD r[2], vt[2];
            for (int side = 0; side < 2; side++) {
                BYTE *obj;
                DWORD *block = nullptr;
                const DWORD count = (DWORD)k % 4;
                if (flags & 2) {
                    obj = static_cast<BYTE *>(original<void *(__fastcall *)(int, int, int)>(c.create_array)((int)count, 0, 0));
                    block = reinterpret_cast<DWORD *>(obj) - 1;
                } else
                    obj = static_cast<BYTE *>(original<void *(__cdecl *)()>(c.create)());
                if (side == 0)
                    hooks_out();
                void *res = at<void *(THISCALL *)(void *, DWORD)>(c.dtor)(obj, flags);
                if (side == 0)
                    hooks_in();
                r[side] = res == obj ? 1 : res == block ? 2 : 3;
                vt[side] = 0;
                if (!(flags & 1)) {
                    vt[side] = count && (flags & 2) ? dw(obj, 0) : !(flags & 2) ? dw(obj, 0) : 0;
                    if (flags & 2)
                        heap_free(block);
                    else
                        pool_free(obj, c.size);
                }
            }
            char d[96];
            wsprintfA(d, "[0x%x flags %u: %u / %u, vtable %x / %x]", c.dtor, flags, r[0], r[1], vt[0], vt[1]);
            t.check(r[0] == r[1] && vt[0] == vt[1], c.name, d);
        }
}

}  // namespace

ZAX_CHECKS(ai_slots)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally reg{"ai slots: class getters and RegisterFields"}, real{"ai slots: RegisterFields against the registered fields (real data)"},
        dtors{"ai slots: deleting destructors"}, slots{"ai slots: the behaviour slots"};
    init_vtables();
    test_registration(reg, real);
    reg.report();
    real.report();
    test_dtors(dtors);
    dtors.report();
    test_slots(slots);
    slots.report();
    log("  ai slots: %u ms", GetTickCount() - start);
}

}  // namespace zax
