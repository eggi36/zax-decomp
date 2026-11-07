// Checks of the entity behaviours and the behaviour stack (ai_behaviors.cpp) against the original, in
// the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls and the vtables run the original too), side 1
// through the reimplementation.
//  - The behaviour slots and helpers run on a fake world: a fake entity (0x200 bytes, its own vtable
//    for the entity slots 2, 47, 58, 59, 84, 103 and 104, a real CEntityBehaviorStack at +0x90), a fake
//    model, sequence player and fidget settings, and the exe functions they call (the entity's model
//    and player, CModel::FindSequence, setting sequences, frames and play ratios, the player's frame
//    functions, distances and angles between parts, listeners, the random generator, fidgets, actions,
//    messages, the debug drawing) jump to stand-ins that record their arguments into a trace and
//    answer from a sequence of values that depends only on the case and the number of the call. The
//    behaviour under test is built in a static buffer with a copy of its class's vtable whose slot 1
//    (deleting destructor) records instead of freeing, so End and the stack can "delete" it.
//    Compared: the return value (al for the bool slots), the trace, the behaviour's bytes (strings by
//    text), the entity's bytes, the stack (count, items, InProcess, priority), the counter 0x6bddfc;
//    the helpers that make a behaviour: the new object's bytes, then deleted through slot 1.
//  - The stack's functions on stacks of probe behaviours (fake vtable: Begin, Update, Draw, End and
//    delete record; Begin and End may also take the top off or put another probe on top, so the
//    functions see the stack change under them).
//  - The destructors (slot 1 with flags 0-3 on Create'd objects and arrays, the out-of-line ones), the
//    stack's constructor, the class getters (both copies, with the class variable set, 0, and 0 for
//    the whole parent chain; ClassInfo_Register 0x415780 jumps to a stand-in) and RegisterFields into
//    a fake class info, against each other and (the original's) against the descriptors registered at
//    startup.
//  - Real data, only what leaves the game unchanged: the behaviour stacks of the level's animated
//    entities (vtable, items, Contains for each item), the behaviours on them (their vtables and class
//    getters), the entities' angles through slot 30.
#include "check_util.h"
#include "field_kinds.h"
#include "layer_parts.h"
#include "checks_ai_behaviors.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_ai_behaviors(Hook *out);

namespace {

using Getter = CClassInfo *(__cdecl *)();

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
float fbits(DWORD v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

// ---- Memory helpers (as in checks_geometry.cpp) --------------------------------------------------------

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
    // through the trampoline when the getter is hooked
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000 || !readable(at<void *>(var), 4))
        return nullptr;
    return *at<CClassInfo **>(var);
}

bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END) && p < 0x80000000u; }

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 256;
SavedSite g_sites[MAX_SITES];
int g_nsites;
Hook g_hook_list[MAX_SITES];
// The module's code pages (all its hooks lie in 0x494000-0x49afff) are writable for the whole module's
// checks, so taking the hooks out and in for every side of a case is a plain copy (until 2026-10-25
// two VirtualProtect calls per hook and side made the checks take 105 s)
constexpr DWORD CODE_FROM = 0x494000, CODE_SIZE = 0x7000;
DWORD g_code_protection;
void code_writable(bool on)
{
    DWORD old;
    VirtualProtect(at<void *>(CODE_FROM), CODE_SIZE, on ? PAGE_EXECUTE_READWRITE : g_code_protection, &old);
    if (on)
        g_code_protection = old;
}
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_ai_behaviors(nullptr);
    if (!hook_sites_fit("ai_behaviors", n, MAX_SITES))
        return;
    hooks_ai_behaviors(g_hook_list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(g_hook_list[i].address);
        if (!memcmp(site, g_hook_list[i].expected, 5))
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        memcpy(site, g_hook_list[i].expected, 5);
    }
    FlushInstructionCache(GetCurrentProcess(), at<void *>(CODE_FROM), CODE_SIZE);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(CODE_FROM), CODE_SIZE);
    g_nsites = 0;
}

// ---- The fake world ------------------------------------------------------------------------------------

struct Statics {
    alignas(16) BYTE ent[0x200];
    DWORD ent_vt[128];
    alignas(16) BYTE player[0x60];
    DWORD player_vt[32];
    alignas(16) BYTE model[0xc0];
    alignas(16) BYTE settings[0x10], settings2[0x10];
    alignas(16) BYTE target[0x40], other_part[0x40], view[0x10], msg[0x10], action[0x10], layer[0x10], motion[0x10];
    alignas(16) BYTE fidget_old[0x90];
    alignas(16) BYTE self[0x40];
    DWORD self_vt[40];
    alignas(16) BYTE probe[8][0x10];
    DWORD probe_vt[32];
    alignas(16) BYTE zs[2][4];
    char fidget_text[3][16];
};
Statics g_s;

const char *const TEXTS[] = {"", "Idle", "Walk", "Attack 1", "IdleCrouch", "Fidget A", "Die"};
constexpr int NTEXTS = sizeof TEXTS / sizeof TEXTS[0];
const char *const MESSAGES[] = {"Delete", "delete", "Died", "died", "Leaving Scope", "LEAVING SCOPE", "Diedx", "Other", ""};
constexpr int NMESSAGES = sizeof MESSAGES / sizeof MESSAGES[0];

unsigned th(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }
// A pointer as both sides see it: the fake world, the exe and null as they are, anything else (new
// blocks) by its first dword
DWORD rp(const void *p)
{
    const DWORD v = ptrv(p);
    if (!v || (v >= ptrv(&g_s) && v < ptrv(&g_s) + sizeof g_s) || (v >= 0x400000 && v < IMAGE_END))
        return v;
    return 0xa0000000u | (readable(p, 4) ? (*static_cast<const DWORD *>(p) & 0x0fffffff) : 0x0fffffff);
}

struct World {
    int find_mode, player_mode, depth, probe_next;
    DWORD frames;
    float max_dist, angle_ref;
    CClassInfo *ent_class;
    bool trapped;
};
World g_w;

// The trace of the stand-ins' calls and the values they answer with
struct Trace {
    unsigned hash;
    int calls;
    DWORD log[12][5];
};
Trace g_tr;
unsigned g_vseed;
DWORD g_vindex;
DWORD nv()
{
    DWORD x = g_vseed + ++g_vindex * 0x9e3779b9u;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
void rec(DWORD id, DWORD a = 0, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD v[5] = {id, a, b, c, d};
    g_tr.hash = fnv(g_tr.hash, v, sizeof v);
    if (g_tr.calls < 12)
        memcpy(g_tr.log[g_tr.calls], v, sizeof v);
    g_tr.calls++;
}
// A float between lo and hi; one in 16 a special value
float frange(float lo, float hi)
{
    const DWORD v = nv();
    if ((v & 15) == 0) {
        const DWORD specials[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, bits(lo), bits(hi), 0x7149f2cau, 0x8ac7230au};
        return fbits(specials[(v >> 4) & 7]);
    }
    return lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
}

// The stand-ins of the exe functions
BYTE *THISCALL st_model(void *e)
{
    rec(1, rp(e));
    return g_s.model;
}
BYTE *THISCALL st_player(void *e)
{
    rec(2, rp(e));
    return g_w.player_mode == 1 ? nullptr : g_s.player;
}
int THISCALL st_find_sequence(void *m, const char *name)
{
    rec(3, rp(m), th(name));
    if (g_w.find_mode == 0)
        return (int)(nv() % 20);
    if (g_w.find_mode == 1)
        return -1;
    return (nv() & 3) ? (int)(nv() % 20) : -1;
}
float THISCALL st_turn_speed(void *m)
{
    rec(4, rp(m));
    return frange(0.0f, 12.0f);
}
void THISCALL st_set_sequence(void *e, const char *n) { rec(5, rp(e), th(n)); }
void THISCALL st_set_frame_index(void *e, DWORD f) { rec(6, rp(e), f); }
void THISCALL st_set_frame(void *e, DWORD f) { rec(7, rp(e), f); }
void THISCALL st_set_play_ratio(void *e, DWORD r) { rec(8, rp(e), r); }
bool THISCALL st_backwards(void *e)
{
    rec(9, rp(e));
    return nv() & 1;
}
void *THISCALL st_motion(void *e, void *p)
{
    rec(10, rp(e), rp(p));
    return g_s.motion;
}
void THISCALL st_move(void *e, DWORD x, DWORD y) { rec(11, rp(e), x, y); }
void THISCALL st_died(void *e) { rec(12, rp(e)); }
void THISCALL st_play_forward(void *p, DWORD f, DWORD t, void *e) { rec(13, rp(p), f, t, rp(e)); }
void THISCALL st_play_backward(void *p, DWORD f, DWORD t, void *e) { rec(14, rp(p), f, t, rp(e)); }
void THISCALL st_player_motion(void *p, DWORD *out, void *motion, DWORD f, DWORD t, DWORD fwd)
{
    rec(15, rp(p), rp(motion), f, t);
    rec(16, fwd, out[0], out[1]);
    const DWORD v = nv();
    if (v & 1)
        out[0] = bits(frange(-20.0f, 20.0f));
    if (v & 2)
        out[1] = bits(frange(-20.0f, 20.0f));
}
DWORD __fastcall st_sync(DWORD n, DWORD z, DWORD fps)
{
    rec(17, n, z, fps);
    return nv() % (n + 3);
}
float __fastcall st_distance2(void *a, void *b, DWORD scale)
{
    rec(18, rp(a), rp(b), scale);
    return frange(0.0f, 2.0f) * g_w.max_dist * g_w.max_dist;
}
float THISCALL st_angle(void *a, void *b, DWORD x, DWORD scale)
{
    rec(19, rp(a), rp(b), x, scale);
    if (nv() & 1)
        return g_w.angle_ref + frange(-0.3f, 0.3f);
    return frange(-8.0f, 14.0f);
}
void THISCALL st_add_listener(void *part, void *l, int i, const char *t) { rec(20, rp(part), rp(l), (DWORD)i, th(t)); }
DWORD THISCALL st_remove_listener(void *part, void *l, int i, const char *t)
{
    rec(21, rp(part), rp(l), (DWORD)i, th(t));
    return nv();
}
void *THISCALL st_position(void *part, float *out)
{
    rec(22, rp(part));
    out[0] = frange(0.0f, 4000.0f);
    out[1] = frange(0.0f, 4000.0f);
    return out;
}
DWORD __fastcall st_draw(void *view, const float *pos, DWORD angle, DWORD a, DWORD b, DWORD c, DWORD d)
{
    rec(23, rp(view), bits(pos[0]), bits(pos[1]), angle);
    rec(24, a, b, c, d);
    return nv();
}
float THISCALL st_random(void *gen, DWORD lo, DWORD hi)
{
    rec(25, rp(gen), lo, hi);
    const DWORD v = nv();
    if ((v & 7) == 0)
        return frange(-5.0f, 50.0f);
    return fbits(lo) + (fbits(hi) - fbits(lo)) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
}
const char *THISCALL st_fidget_choose(void *d, void *s, void *e, const char *idle)
{
    rec(26, rp(d), rp(s), rp(e), th(idle));
    return g_s.fidget_text[nv() % 3];
}
float THISCALL st_fidget_delay(void *s)
{
    rec(27, rp(s));
    return frange(0.0f, 8.0f);
}
bool THISCALL st_message_is(void *m, const char *n)
{
    rec(28, rp(m), th(n));
    return nv() % 3 == 0;
}
DWORD THISCALL st_action(void *a, void *layer, void *e, DWORD x, void *inst, void *self, void *field)
{
    rec(29, rp(a), rp(layer), rp(e), x);
    rec(30, rp(inst), rp(self), rp(field));
    return nv();
}
void *THISCALL st_fidget_data_ctor(void *o)
{
    rec(31);
    memset(o, 0x5a, 0x8c);
    return o;
}
DWORD THISCALL st_on_message(void *o, void *sender, const char *t)
{
    rec(32, rp(o), rp(sender), th(t));
    return nv();
}

// The fake entity's slots
CClassInfo *THISCALL ent_class_slot(void *e)
{
    rec(40, rp(e));
    return g_w.ent_class;
}
DWORD THISCALL ent_slot47(void *e)
{
    rec(41, rp(e));
    return nv() & 0xff;
}
void THISCALL ent_set_flags(void *e, DWORD m) { rec(42, rp(e), m); }
void THISCALL ent_clear_flags(void *e, DWORD m) { rec(43, rp(e), m); }
float THISCALL ent_cur_frame(void *e)
{
    rec(44, rp(e));
    return frange(-2.0f, (float)g_w.frames + 3.0f);
}
DWORD THISCALL ent_set_aiming(void *e, DWORD a, DWORD one)
{
    rec(45, rp(e), a, one);
    fl<DWORD>(e, 0xc4) = a;
    return nv();
}
DWORD THISCALL ent_set_moving(void *e, DWORD a, DWORD one)
{
    rec(46, rp(e), a, one);
    fl<DWORD>(e, 0xc8) = a;
    return nv();
}
// Any other slot of the fakes: not expected (logged once per case)
DWORD THISCALL trap_slot(void *o)
{
    rec(99, rp(o));
    g_w.trapped = true;
    return 0;
}
float THISCALL player_fps(void *p)
{
    rec(50, rp(p));
    return frange(0.0f, 40.0f);
}

// Probe behaviours for the stack: id at +0xc
BYTE *next_probe()
{
    if (g_w.probe_next >= 8)
        return nullptr;
    return g_s.probe[g_w.probe_next++];
}
BYTE *ent_stack() { return g_s.ent + 0x90; }
void *THISCALL probe_delete(BYTE *p, DWORD flags)
{
    rec(60, fl<DWORD>(p, 0xc), flags);
    return p;
}
bool THISCALL probe_begin(BYTE *p, BYTE *e)
{
    rec(61, fl<DWORD>(p, 0xc), rp(e));
    const DWORD a = nv() % 8;
    if (a < 3)
        return false;
    if (a == 6 && g_w.depth < 3) {
        g_w.depth++;
        at<DWORD(THISCALL *)(BYTE *, BYTE *, void *)>(0x49a650)(ent_stack(), e, p);   // takes itself off
        g_w.depth--;
    } else if (a == 7 && g_w.depth < 3) {
        if (BYTE *q = next_probe()) {
            g_w.depth++;
            at<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x49a620)(ent_stack(), q);   // another on top
            g_w.depth--;
        }
    }
    return true;
}
DWORD THISCALL probe_update(BYTE *p, DWORD dt, BYTE *e)
{
    rec(62, fl<DWORD>(p, 0xc), dt, rp(e));
    return nv();
}
void THISCALL probe_draw(BYTE *p, void *a, void *b) { rec(63, fl<DWORD>(p, 0xc), rp(a), rp(b)); }
void *THISCALL probe_end(BYTE *p, BYTE *e)
{
    rec(64, fl<DWORD>(p, 0xc), rp(e));
    if (nv() % 8 == 0 && g_w.depth < 3)
        if (BYTE *q = next_probe()) {
            g_w.depth++;
            at<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x49a620)(ent_stack(), q);
            g_w.depth--;
        }
    return p;
}
// Slot 1 of the behaviour under test: records instead of freeing
void *THISCALL self_delete(BYTE *p, DWORD flags)
{
    rec(65, rp(p), flags);
    return p;
}

struct StubDef {
    DWORD address;
    void *stub;
};
const StubDef STUBS[] = {
    {0x48b290, (void *)&st_model},
    {0x48b590, (void *)&st_player},
    {0x517240, (void *)&st_find_sequence},
    {0x517510, (void *)&st_turn_speed},
    {0x48f700, (void *)&st_set_sequence},
    {0x490010, (void *)&st_set_frame_index},
    {0x48feb0, (void *)&st_set_frame},
    {0x48f6a0, (void *)&st_set_play_ratio},
    {0x48f260, (void *)&st_backwards},
    {0x48fe80, (void *)&st_motion},
    {0x491230, (void *)&st_move},
    {0x4fbf30, (void *)&st_died},
    {0x560300, (void *)&st_play_forward},
    {0x560480, (void *)&st_play_backward},
    {0x55fc70, (void *)&st_player_motion},
    {0x55e4b0, (void *)&st_sync},
    {0x4fbf60, (void *)&st_distance2},
    {0x4fb2d0, (void *)&st_angle},
    {0x4fc1b0, (void *)&st_add_listener},
    {0x4fc200, (void *)&st_remove_listener},
    {0x4fb0a0, (void *)&st_position},
    {0x4fceb0, (void *)&st_draw},
    {0x55c550, (void *)&st_random},
    {0x4a0680, (void *)&st_fidget_choose},
    {0x4a02c0, (void *)&st_fidget_delay},
    {0x506620, (void *)&st_message_is},
    {0x41a720, (void *)&st_action},
    {0x4a04e0, (void *)&st_fidget_data_ctor},
    {0x46cd50, (void *)&st_on_message},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
CodePatch g_patches[NSTUBS];
void world_on()
{
    for (int i = 0; i < NSTUBS; i++)
        patch_jump(g_patches[i], STUBS[i].address, STUBS[i].stub);
}
void world_off()
{
    for (int i = NSTUBS; i--;)
        unpatch(g_patches[i]);
}

void init_fakes()
{
    for (DWORD &v : g_s.ent_vt)
        v = ptrv((void *)&trap_slot);
    g_s.ent_vt[2] = ptrv((void *)&ent_class_slot);
    g_s.ent_vt[47] = ptrv((void *)&ent_slot47);
    g_s.ent_vt[58] = ptrv((void *)&ent_set_flags);
    g_s.ent_vt[59] = ptrv((void *)&ent_clear_flags);
    g_s.ent_vt[84] = ptrv((void *)&ent_cur_frame);
    g_s.ent_vt[103] = ptrv((void *)&ent_set_aiming);
    g_s.ent_vt[104] = ptrv((void *)&ent_set_moving);
    for (DWORD &v : g_s.player_vt)
        v = ptrv((void *)&trap_slot);
    g_s.player_vt[26] = ptrv((void *)&player_fps);
    for (DWORD &v : g_s.probe_vt)
        v = ptrv((void *)&trap_slot);
    g_s.probe_vt[1] = ptrv((void *)&probe_delete);
    g_s.probe_vt[23] = ptrv((void *)&probe_begin);
    g_s.probe_vt[25] = ptrv((void *)&probe_update);
    g_s.probe_vt[28] = ptrv((void *)&probe_draw);
    g_s.probe_vt[29] = ptrv((void *)&probe_end);
    lstrcpyA(g_s.fidget_text[0], "Fidget A");
    lstrcpyA(g_s.fidget_text[1], "Scratch");
    lstrcpyA(g_s.fidget_text[2], "");
}

// Exe functions the checks use for the set-up (the reimplementation's, hooks in)
void zs_ctor(void *z) { at<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_set(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
const char *zs_text(const void *z) { return at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
void stack_ctor(BYTE *s) { at<BYTE *(THISCALL *)(BYTE *)>(0x49a3a0)(s); }
void stack_dtor(BYTE *s) { at<void(THISCALL *)(BYTE *)>(0x49a430)(s); }
void stack_append(BYTE *s, BYTE *b) { at<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x49a620)(s, b); }

// ---- One side of a case --------------------------------------------------------------------------------

enum Ret { RET_NONE, RET_AL, RET_EAX, RET_FLOAT };

struct Side {
    DWORD ret, counter, made_vt;
    unsigned hash;
    int calls;
    bool trapped;
    DWORD log[12][5];
    DWORD self[0x10], ent[0x80], made[0x10], stack[12], probes[8];
};
Side g_sd[2];
int g_side;

// The class levels' strings (offset list per class index, 0-terminated)
static_assert(NBEHAVIOR_CLASSES == 19, "class table");
constexpr DWORD ZS_NONE[] = {0}, ZS_SET_ROTATION[] = {0xc, 0}, ZS_PLAY_ONCE[] = {0xc, 0x1c, 0}, ZS_IDLE[] = {0x18, 0}, ZS_LOOPED[] = {0x10, 0};
const DWORD *zstrings(int cls)
{
    switch (cls) {
    case 6: return ZS_SET_ROTATION;
    case 7: return ZS_PLAY_ONCE;
    case 14: return ZS_IDLE;
    case 17: return ZS_LOOPED;
    }
    return ZS_NONE;
}
bool is_zstring(int cls, DWORD off)
{
    for (const DWORD *z = zstrings(cls); *z; z++)
        if (*z == off)
            return true;
    return false;
}

// An object's bytes as dwords, its strings by text
void capture_object(DWORD *out, BYTE *o, DWORD size, int cls)
{
    memset(out, 0, 0x40);
    for (DWORD off = 0; off < size && off < 0x40; off += 4)
        out[off / 4] = is_zstring(cls, off) ? th(zs_text(o + off)) : fl<DWORD>(o, off);
}
void capture_stack(DWORD *out, BYTE *s)
{
    memset(out, 0, 12 * 4);
    out[0] = fl<BYTE>(s, 8);
    out[1] = fl<DWORD>(s, 0xc);
    const DWORD n = fl<DWORD>(s, 0x18);
    out[2] = n;
    out[3] = fl<DWORD>(s, 0x10);
    for (DWORD i = 0; i < n && i < 8; i++)
        out[4 + i] = rp(fl<BYTE **>(s, 0x14)[i]);
}

// The behaviour under test in g_s.self: a copy of its class's vtable (slot 1 records), its fields
// from the seed
struct Build {
    unsigned r;
    DWORD next() { return r = r * 1664525u + 1013904223u; }
    float f(float lo, float hi)
    {
        const DWORD v = next();
        if ((v >> 28) == 0) {
            const DWORD specials[] = {0, 0x80000000u, 0x7fc00000u, 0xff800000u, bits(lo), bits(hi), 0x3dcccccd, 0xbf800000u};
            return fbits(specials[(v >> 8) & 7]);
        }
        return lo + (hi - lo) * (float)(int)((v >> 4) & 0xffffff) * (1.0f / 16777216.0f);
    }
};

void build_self(int cls, Build &b, bool clean_objects)
{
    const BehaviorClassDef &c = BEHAVIOR_CLASSES[cls];
    for (int i = 0; i < 32; i++)
        g_s.self_vt[i] = at<DWORD *>(c.vtable)[i];
    g_s.self_vt[1] = ptrv((void *)&self_delete);
    BYTE *o = g_s.self;
    for (DWORD off = 0; off < sizeof g_s.self; off += 4)
        fl<DWORD>(o, off) = b.next();
    fl<DWORD>(o, 0) = ptrv(g_s.self_vt);
    fl<DWORD>(o, 4) = 0;
    fl<DWORD>(o, 8) = b.next() & 0xff;
    switch (cls) {
    case 2: case 3: case 4: case 5:   // the turn behaviours
        fl<DWORD>(o, 0xc) = b.next() & 1 ? 0x200000 : (b.next() & 3 ? 0 : b.next());
        fl<float>(o, 0x10) = b.next() & 3 ? 0.1f : b.f(0.0f, 1.0f);
        fl<BYTE>(o, 0x14) = (BYTE)(b.next() & 1);
        if (cls == 4)
            fl<float>(o, 0x18) = b.f(-8.0f, 14.0f);
        if (cls == 5) {
            fl<void *>(o, 0x18) = b.next() % 4 ? (void *)g_s.target : nullptr;
            fl<float>(o, 0x1c) = b.f(0.0f, 3000.0f);
            fl<DWORD>(o, 0x20) = b.next() & 1;
        }
        break;
    case 7:   // PlaySequenceOnce
        fl<float>(o, 0x10) = b.f(-2.0f, 2.0f);
        fl<float>(o, 0x14) = b.f(-3.0f, 3.0f);
        fl<float>(o, 0x18) = b.next() & 1 ? fl<float>(o, 0x14) : b.f(-3.0f, 3.0f);
        fl<DWORD>(o, 0x20) = b.next() % 3 ? 0xbf800000u : bits(b.f(0.0f, 45.0f));
        fl<BYTE>(o, 0x24) = (BYTE)(b.next() & 1);
        fl<BYTE>(o, 0x25) = (BYTE)(b.next() & 1);
        break;
    case 8: fl<float>(o, 0xc) = b.f(-2.0f, 2.0f); break;
    case 10: fl<DWORD>(o, 0xc) = b.next() & 0xae0000; break;
    case 11: case 13: case 14:   // Wait, Fade, Idle
        fl<float>(o, 0xc) = b.f(0.0f, 5.0f);
        fl<float>(o, 0x10) = b.f(0.0f, 6.0f);
        if (cls == 13)
            fl<DWORD>(o, 0x14) = b.next() % 3;
        if (cls == 14) {
            fl<DWORD>(o, 0x14) = b.next() % 4;
            fl<BYTE>(o, 0x1c) = (BYTE)(b.next() & 1);
            fl<float>(o, 0x20) = b.f(-2.0f, 5.0f);
            fl<void *>(o, 0x24) = !clean_objects && b.next() % 3 == 0 ? (void *)g_s.settings2 : nullptr;
            fl<void *>(o, 0x28) = !clean_objects && b.next() % 2 == 0 ? (void *)g_s.fidget_old : nullptr;
            fl<float>(o, 0x2c) = b.f(0.9f, 1.1f);
        }
        break;
    case 15: fl<DWORD>(o, 0xc) = b.next() % 50; break;
    case 16: fl<void *>(o, 0xc) = b.next() % 4 ? (void *)g_s.action : nullptr; break;
    case 17: fl<DWORD>(o, 0xc) = b.next() & 0x1f; break;
    }
    for (const DWORD *z = zstrings(cls); *z; z++) {
        zs_ctor(o + *z);
        zs_set(o + *z, TEXTS[b.next() % NTEXTS]);
    }
}
void free_self(int cls)
{
    for (const DWORD *z = zstrings(cls); *z; z++)
        zs_dtor(g_s.self + *z);
}

// The fake entity, model, player and settings from the seed; the stack at +0x90 with stack_kind:
// 0 empty, 1 the behaviour, 2 a probe and the behaviour, 3 the behaviour and a probe, 4 probes only.
// The entity has no sequence player in one of 8 cases with null_player (only the Update slots test
// for one; the other functions read the player's frame count without a test, the original too).
void build_world(unsigned seed, int stack_kind, bool null_player)
{
    Build b{seed * 2654435761u + 77};
    memset(&g_w, 0, sizeof g_w);
    g_w.find_mode = (int)(b.next() % 3);
    const DWORD pm = b.next() % 8;
    g_w.player_mode = null_player && pm == 1 ? 1 : 0;
    g_w.frames = b.next() % 40;
    BYTE *e = g_s.ent;
    for (DWORD off = 0; off < sizeof g_s.ent; off += 4)
        fl<DWORD>(e, off) = b.next();
    fl<DWORD>(e, 0) = ptrv(g_s.ent_vt);
    fl<DWORD>(e, 4) = 0;
    fl<DWORD>(e, 0x1c) = b.next();
    fl<void *>(e, 0x48) = g_s.layer;
    fl<float>(e, 0xb4) = b.f(0.0f, 1.0f);
    const DWORD ratios[] = {0x3f800000u, 0xbf800000u, 0x3f000000u, 0x40000000u, 0, 0x80000000u, 0x3fc00000u, 0xbfc00000u};
    fl<DWORD>(e, 0xb8) = b.next() % 4 ? ratios[b.next() % 8] : bits(b.f(-3.0f, 3.0f));
    fl<float>(e, 0xc4) = b.f(-1.0f, 7.5f);
    fl<float>(e, 0xc8) = b.f(-1.0f, 7.5f);
    fl<float>(e, 0x100) = b.f(-100.0f, 4000.0f);
    fl<float>(e, 0x104) = b.f(-100.0f, 4000.0f);
    fl<DWORD>(e, 0x12c) = b.next() % 4;
    const Getter classes[] = {at<Getter>(0x48dcc0), at<Getter>(0x4903f0), at<Getter>(0x49b810), at<Getter>(0x494410)};
    const DWORD k = b.next() % 8;
    g_w.ent_class = classes[k < 4 ? k : k < 6 ? 2 : 0]();
    g_w.angle_ref = b.next() & 1 ? fl<float>(e, 0xc4) : fl<float>(e, 0xc8);
    memset(g_s.player, 0, sizeof g_s.player);
    fl<DWORD>(g_s.player, 0) = ptrv(g_s.player_vt);
    fl<DWORD>(g_s.player, 0x48) = g_w.frames;
    memset(g_s.model, 0, sizeof g_s.model);
    fl<void *>(g_s.model, 0xbc) = b.next() % 3 ? (void *)g_s.settings : nullptr;
    fl<float>(g_s.settings, 8) = b.f(0.0f, 10.0f);
    fl<float>(g_s.settings2, 8) = b.f(0.0f, 10.0f);
    memset(g_s.fidget_old, 0x33, sizeof g_s.fidget_old);
    for (int i = 0; i < 8; i++) {
        BYTE *p = g_s.probe[i];
        fl<DWORD>(p, 0) = ptrv(g_s.probe_vt);
        fl<DWORD>(p, 4) = 0;
        fl<DWORD>(p, 8) = b.next() & 0xf0;
        fl<DWORD>(p, 0xc) = (DWORD)i + 1;
    }
    BYTE *s = ent_stack();
    stack_ctor(s);
    if (stack_kind == 2 || stack_kind == 4)
        stack_append(s, next_probe());
    if (stack_kind >= 1 && stack_kind <= 3)
        stack_append(s, g_s.self);
    if (stack_kind == 3 || stack_kind == 4)
        stack_append(s, next_probe());
    if (stack_kind == 4)
        for (DWORD n = b.next() % 4; n--;)
            stack_append(s, next_probe());
    fl<BYTE>(s, 8) = (BYTE)(b.next() % 3);
    fl<DWORD>(s, 0xc) = b.next() % 5;
}

// The trace, the behaviour, the entity, the stack and the probes into g_sd[side]
void capture_side(int cls, bool with_self)
{
    Side &sd = g_sd[g_side];
    sd.hash = g_tr.hash;
    sd.calls = g_tr.calls;
    sd.trapped = g_w.trapped;
    memcpy(sd.log, g_tr.log, sizeof sd.log);
    if (with_self) {
        capture_object(sd.self, g_s.self, BEHAVIOR_CLASSES[cls].size, cls);
        sd.self[0] = 0;
        if (cls == 14) {   // a new fidget data: by its content
            BYTE *d = fl<BYTE *>(g_s.self, 0x28);
            if (d && d != g_s.fidget_old)
                sd.self[0x28 / 4] = 0xd0000000u | fnv(2166136261u, d, 0x8c);
        }
    }
    for (DWORD off = 0; off < sizeof g_s.ent; off += 4)
        sd.ent[off / 4] = off >= 0x90 && off < 0xb0 ? 0 : fl<DWORD>(g_s.ent, off);
    capture_stack(sd.stack, ent_stack());
    for (int i = 0; i < 8; i++)
        sd.probes[i] = fl<DWORD>(g_s.probe[i], 8);
}
void free_world()
{
    stack_dtor(ent_stack());
    BYTE *d = fl<BYTE *>(g_s.self, 0x28);
    (void)d;
}

bool compare_sides(Ret mode, char *detail, const char *what)
{
    const Side &a = g_sd[0], &b = g_sd[1];
    DWORD ra = a.ret, rb = b.ret;
    if (mode == RET_AL)
        ra &= 0xff, rb &= 0xff;
    if (mode == RET_NONE)
        ra = rb = 0;
    const bool same = ra == rb && a.hash == b.hash && a.calls == b.calls && !a.trapped && !b.trapped && a.counter == b.counter &&
                      a.made_vt == b.made_vt && !memcmp(a.self, b.self, sizeof a.self) && !memcmp(a.ent, b.ent, sizeof a.ent) &&
                      !memcmp(a.made, b.made, sizeof a.made) && !memcmp(a.stack, b.stack, sizeof a.stack) &&
                      !memcmp(a.probes, b.probes, sizeof a.probes);
    int first = 0;
    while (first < 12 && first < a.calls && first < b.calls && !memcmp(a.log[first], b.log[first], 20))
        first++;
    int self_off = -1, ent_off = -1;
    for (int i = 0; i < 0x10 && self_off < 0; i++)
        if (a.self[i] != b.self[i] || a.made[i] != b.made[i])
            self_off = i * 4;
    for (int i = 0; i < 0x80 && ent_off < 0; i++)
        if (a.ent[i] != b.ent[i])
            ent_off = i * 4;
    wsprintfA(detail, "[%.40s: ret %x / %x, calls %d / %d, first diff call %d (%x %x %x / %x %x %x), obj +%x, ent +%x, stack %d%s]", what,
              a.ret, b.ret, a.calls, b.calls, first, first < 12 ? a.log[first][0] : 0, first < 12 ? a.log[first][1] : 0,
              first < 12 ? a.log[first][2] : 0, first < 12 ? b.log[first][0] : 0, first < 12 ? b.log[first][1] : 0,
              first < 12 ? b.log[first][2] : 0, self_off, ent_off, memcmp(a.stack, b.stack, sizeof a.stack) ? 1 : 0,
              a.trapped || b.trapped ? ", trap" : "");
    return same;
}

// ---- The slot tests ------------------------------------------------------------------------------------

enum Kind {
    BEGIN, UPDATE, END, ANGLE, SET_ANGLE, DRAW, SET_TARGET, TT_MSG, PL_MSG, IDLE_START, RESET, SLOT18, SEQ_NAME,
    MAKE_TTA, MAKE_TTT, MAKE_PSO, MAKE_SPR, MAKE_MFD, MAKE_SCF, MAKE_WAIT, MAKE_IDLE
};
struct SlotTest {
    const char *name;
    DWORD address;
    Kind kind;
    int cls, count;
    bool needs_stack;   // takes the top off unconditionally: never an empty stack
};
const SlotTest SLOT_TESTS[] = {
    {"CEntityBehavior::Begin", 0x494490, BEGIN, 11, 100, false},
    {"CEntityBehavior::Update", 0x4944a0, UPDATE, 9, 50, false},
    {"CEntityBehavior::End", 0x494500, END, 11, 50, false},
    {"CEntityBehaviorTurnToAngleBase::Begin", 0x494680, BEGIN, 4, 200, false},
    {"CEntityBehaviorTurnToAngleBase::End", 0x4946b0, END, 4, 100, false},
    {"CEntityBehaviorTurnMovingAngle::Angle", 0x4949b0, ANGLE, 2, 50, false},
    {"CEntityBehaviorTurnAimingAngle::Angle", 0x494cb0, ANGLE, 3, 50, false},
    {"CEntityBehaviorTurnMovingAngle::SetAngle", 0x4949c0, SET_ANGLE, 2, 50, false},
    {"CEntityBehaviorTurnAimingAngle::SetAngle", 0x494cc0, SET_ANGLE, 3, 50, false},
    {"CEntityBehaviorTurnToAngle::Update", 0x495490, UPDATE, 4, 2000, false},
    {"CEntityBehaviorTurnToAngle::Draw", 0x495620, DRAW, 4, 100, false},
    {"CEntityBehaviorTurnToAngle::SetTarget", 0x4953a0, SET_TARGET, 4, 50, false},
    {"EntityBehaviorTurnToAngle_Make", 0x4953b0, MAKE_TTA, 4, 1000, false},
    {"EntityBehaviorTurnToTarget_Make", 0x495ec0, MAKE_TTT, 5, 1000, false},
    {"CEntityBehaviorTurnToTarget::OnMessage", 0x496040, TT_MSG, 5, 400, false},
    {"CEntityBehaviorTurnToTarget::Update", 0x4960f0, UPDATE, 5, 2000, false},
    {"CEntityBehaviorSetRotationSequence::Begin", 0x4965b0, BEGIN, 6, 400, true},
    {"EntityBehaviorPlaySequenceOnce_Make", 0x496e20, MAKE_PSO, 7, 400, false},
    {"CEntityBehaviorPlaySequenceOnce::Begin", 0x496ee0, BEGIN, 7, 1000, false},
    {"CEntityBehaviorPlaySequenceOnce::Update", 0x497010, UPDATE, 7, 3000, false},
    {"CEntityBehaviorPlaySequenceOnce::End", 0x497270, END, 7, 100, false},
    {"EntityBehaviorSetPlayRatio_Make", 0x4974e0, MAKE_SPR, 8, 100, false},
    {"CEntityBehaviorSetPlayRatio::Begin", 0x497530, BEGIN, 8, 200, true},
    {"EntityBehaviorMarkForDeletion_Make", 0x497720, MAKE_MFD, 9, 50, false},
    {"CEntityBehaviorMarkForDeletion::Begin", 0x497760, BEGIN, 9, 300, false},
    {"CEntityBehaviorClearFlags::Begin", 0x497c10, BEGIN, 10, 300, false},
    {"EntityBehaviorSetCurFrame_Make", 0x4994d0, MAKE_SCF, 15, 100, false},
    {"CEntityBehaviorSetCurFrame::Begin", 0x499520, BEGIN, 15, 100, false},
    {"CEntityBehaviorExecuteAction::Begin", 0x499790, BEGIN, 16, 200, false},
    {"EntityBehaviorWait_Make", 0x497f30, MAKE_WAIT, 11, 100, false},
    {"CEntityBehaviorWait::Update", 0x497f80, UPDATE, 11, 500, false},
    {"CEntityBehaviorFade::Update", 0x498600, UPDATE, 13, 1000, false},
    {"CEntityBehaviorIdle::ResetFields", 0x498df0, RESET, 14, 100, false},
    {"CEntityBehaviorIdle::Slot18", 0x498e10, SLOT18, 14, 50, false},
    {"EntityBehaviorIdle_SequenceName", 0x498e20, SEQ_NAME, 14, 1000, false},
    {"EntityBehaviorIdle_Make", 0x498f10, MAKE_IDLE, 14, 500, false},
    {"CEntityBehaviorIdle::Start", 0x498fa0, IDLE_START, 14, 1500, false},
    {"CEntityBehaviorIdle::Begin", 0x498710, BEGIN, 14, 500, false},
    {"CEntityBehaviorIdle::Update", 0x4990e0, UPDATE, 14, 3000, false},
    {"CEntityBehaviorPlaySequenceLooped::Begin", 0x499d10, BEGIN, 17, 300, false},
    {"CEntityBehaviorPlaySequenceLooped::Update", 0x499d50, UPDATE, 17, 3000, false},
    {"CEntityBehaviorPlaySequenceLooped::OnMessage", 0x499f30, PL_MSG, 17, 300, false},
};

// The object a helper made: its bytes, then deleted through its slot 1
void capture_made(BYTE *o, int cls)
{
    Side &sd = g_sd[g_side];
    memset(sd.made, 0, sizeof sd.made);
    sd.made_vt = 0;
    if (!o)
        return;
    sd.made_vt = fl<DWORD>(o, 0);
    capture_object(sd.made, o, BEHAVIOR_CLASSES[cls].size, cls);
    vcall<void *>(o, 1, 1);
}

float seconds_of(Build &b)
{
    const DWORD v = b.next() % 8;
    const DWORD fixed[] = {0x3c888889u /* 1/60 */, 0x3d4ccccdu /* 0.05 */, 0, 0xbc23d70au /* -0.01 */, 0x3f800000u};
    return v < 5 ? fbits(fixed[v]) : b.f(0.0f, 0.5f);
}

// One side of a slot test
DWORD run_slot(const SlotTest &t, unsigned seed, Build &b)
{
    BYTE *self = g_s.self, *e = g_s.ent;
    const DWORD a = t.address;
    switch (t.kind) {
    case BEGIN: return at<DWORD(THISCALL *)(BYTE *, BYTE *)>(a)(self, e);
    case UPDATE: return at<DWORD(THISCALL *)(BYTE *, float, BYTE *)>(a)(self, seconds_of(b), e);
    case END: return rp(at<void *(THISCALL *)(BYTE *, BYTE *)>(a)(self, e));
    case ANGLE: return bits(at<float(THISCALL *)(BYTE *, BYTE *)>(a)(self, e));
    case SET_ANGLE: return at<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(a)(self, e, bits(b.f(-8.0f, 14.0f)));
    case DRAW: return at<DWORD(THISCALL *)(BYTE *, BYTE *, void *)>(a)(self, e, g_s.view);
    case SET_TARGET: at<void(THISCALL *)(BYTE *, DWORD)>(a)(self, b.next()); return 0;
    case TT_MSG: {
        void *const senders[] = {g_s.target, g_s.other_part, nullptr};
        void *sender = senders[b.next() % 3];
        const DWORD k = b.next() % 4;
        fl<void *>(self, 0x18) = k == 0 ? nullptr : k == 1 ? (void *)g_s.other_part : (void *)g_s.target;
        const char *text = MESSAGES[b.next() % NMESSAGES];
        const bool forwarded = sender != fl<void *>(self, 0x18) || !crt_stricmp(text, "Delete");
        const DWORD r = at<DWORD(THISCALL *)(BYTE *, void *, const char *)>(a)(self, sender, text);
        return forwarded ? r : 0x5a5a5a5au;
    }
    case PL_MSG: return at<DWORD(THISCALL *)(BYTE *, DWORD, void *)>(a)(self, b.next(), g_s.msg);
    case IDLE_START: {
        const DWORD restart = b.next() & 1 ? b.next() & 1 : b.next() & 0xffffff01u;
        return at<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(a)(self, e, restart);
    }
    case RESET: at<void(THISCALL *)(BYTE *)>(a)(self); return 0;
    case SLOT18: at<void(THISCALL *)(BYTE *, DWORD)>(a)(self, b.next()); return 0;
    case SEQ_NAME: {
        BYTE *idle = b.next() % 3 ? self : nullptr;
        BYTE *ent = b.next() % 16 ? e : nullptr;
        if (!ent)
            idle = nullptr;   // the null entity's model would be read with an idle behaviour
        const char *name = at<const char *(__fastcall *)(BYTE *, BYTE *)>(a)(ent, idle);
        return ptrv(name) >= 0x400000 && ptrv(name) < IMAGE_END ? ptrv(name) : 0xb0000000u | (th(name) & 0x0fffffff);
    }
    case MAKE_TTA: {
        const float angle = b.next() & 1 ? fl<float>(e, 0xc8) + b.f(-0.2f, 0.2f) : b.f(-8.0f, 14.0f);
        BYTE *o = at<BYTE *(THISCALL *)(BYTE *, float, DWORD)>(a)(e, angle, b.next());
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        return r;
    }
    case MAKE_TTT: {
        g_w.max_dist = b.f(0.0f, 3000.0f);
        BYTE *o = at<BYTE *(__fastcall *)(BYTE *, void *, float, DWORD, DWORD, DWORD, DWORD)>(a)(e, g_s.target, g_w.max_dist, b.next(),
                                                                                                 b.next(), b.next(), b.next());
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        return r;
    }
    case MAKE_PSO: {
        zs_ctor(g_s.zs[0]);
        zs_ctor(g_s.zs[1]);
        zs_set(g_s.zs[0], TEXTS[b.next() % NTEXTS]);
        zs_set(g_s.zs[1], TEXTS[b.next() % NTEXTS]);
        const ZString *seq = b.next() % 8 ? reinterpret_cast<const ZString *>(g_s.zs[0]) : nullptr;
        BYTE *o = at<BYTE *(__fastcall *)(BYTE *, const ZString *, DWORD, const ZString *, DWORD, DWORD, DWORD)>(a)(
            e, seq, bits(b.f(-3.0f, 3.0f)), reinterpret_cast<const ZString *>(g_s.zs[1]), b.next() % 2 ? 0xbf800000u : bits(b.f(0.0f, 40.0f)),
            b.next(), b.next());
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        zs_dtor(g_s.zs[1]);
        zs_dtor(g_s.zs[0]);
        return r;
    }
    case MAKE_SPR:
    case MAKE_WAIT: {
        BYTE *o = at<BYTE *(__stdcall *)(DWORD)>(a)(bits(b.f(-2.0f, 5.0f)));
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        return r;
    }
    case MAKE_MFD: {
        BYTE *o = at<BYTE *(__cdecl *)()>(a)();
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        return r;
    }
    case MAKE_SCF: {
        BYTE *o = at<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(a)(b.next() % 60, b.next(), b.next());
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        return r;
    }
    case MAKE_IDLE: {
        BYTE *o = at<BYTE *(THISCALL *)(BYTE *, DWORD, DWORD)>(a)(e, bits(b.f(0.0f, 5.0f)), b.next());
        const DWORD r = rp(o);
        capture_made(o, t.cls);
        return r;
    }
    }
    (void)seed;
    return 0;
}

Ret ret_mode(Kind k)
{
    switch (k) {
    case BEGIN: case UPDATE: case PL_MSG: case IDLE_START: return RET_AL;
    case SET_TARGET: case RESET: case SLOT18: return RET_NONE;
    case ANGLE: return RET_FLOAT;
    default: return RET_EAX;
    }
}

void slot_case(Tally &tally, const SlotTest &t, unsigned seed)
{
    Build pick{seed * 40503u + 11};
    const DWORD sk = pick.next() % 8;
    int stack_kind = sk < 3 ? 1 : sk < 5 ? 2 : sk < 6 ? 3 : sk < 7 ? 0 : 4;
    if (t.needs_stack && stack_kind == 0)
        stack_kind = 1;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        memset(&g_sd[side], 0, sizeof g_sd[side]);
        Build b{seed * 747796405u + 2891336453u};
        build_self(t.cls, b, t.kind == RESET);
        build_world(seed, stack_kind, t.kind == UPDATE);
        if (t.kind == MAKE_TTT || t.cls == 5)
            g_w.max_dist = fl<float>(g_s.self, 0x1c);
        memset(&g_tr, 0, sizeof g_tr);
        g_tr.hash = 2166136261u;
        g_vseed = seed * 0x85ebca6bu + (unsigned)t.address;
        g_vindex = 0;
        const DWORD counter = *at<DWORD *>(0x6bddfc);
        if (side == 0)
            hooks_out();
        DWORD r = run_slot(t, seed, b);
        if (side == 0)
            hooks_in();
        if (ret_mode(t.kind) == RET_AL)
            r &= 0xff;
        g_sd[side].ret = r;
        g_sd[side].counter = *at<DWORD *>(0x6bddfc) - counter;
        capture_side(t.cls, true);
        // a fidget data made in the behaviour (not the static one) back to the pool
        if (t.cls == 14) {
            BYTE *d = fl<BYTE *>(g_s.self, 0x28);
            if (d && d != g_s.fidget_old && heap_pointer(ptrv(d)))
                pool_free(d, 0x8c);
        }
        free_self(t.cls);
        free_world();
    }
    char d[300];
    const bool same = compare_sides(ret_mode(t.kind), d, t.name);
    tally.check(same, "ai_behaviors", d);
}

// ---- The stack tests ------------------------------------------------------------------------------------

enum StackOp { S_BEGIN, S_UPDATE, S_DRAW, S_PUSH, S_APPEND, S_REMOVE, S_CLEAR, S_CONTAINS, NSTACK_OPS };
const char *const STACK_OP_NAMES[] = {"Stack::BeginNext", "Stack::Update", "Stack::Draw", "Stack::Push",
                                      "Stack::Append", "Stack::RemoveTop", "Stack::Clear", "Stack::Contains"};

void stack_case(Tally &tally, int op, unsigned seed)
{
    for (int side = 0; side < 2; side++) {
        g_side = side;
        memset(&g_sd[side], 0, sizeof g_sd[side]);
        Build b{seed * 2246822519u + (unsigned)op * 977u};
        build_self(9, b, false);
        build_world(seed * 3 + (unsigned)op, 4, false);
        if (op == S_REMOVE && !fl<DWORD>(ent_stack(), 0x18))
            stack_append(ent_stack(), next_probe());
        if (b.next() % 6 == 0)   // an empty stack now and then
            while (op != S_REMOVE && fl<DWORD>(ent_stack(), 0x18))
                fl<DWORD>(ent_stack(), 0x18)--;
        memset(&g_tr, 0, sizeof g_tr);
        g_tr.hash = 2166136261u;
        g_vseed = seed * 0xc2b2ae35u + (unsigned)op;
        g_vindex = 0;
        BYTE *s = ent_stack(), *e = g_s.ent;
        if (side == 0)
            hooks_out();
        DWORD r = 0;
        switch (op) {
        case S_BEGIN: r = at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x49a760)(s, e); break;
        case S_UPDATE: r = at<DWORD(THISCALL *)(BYTE *, float, BYTE *)>(0x49a480)(s, seconds_of(b), e); break;
        case S_DRAW: at<void(THISCALL *)(BYTE *, void *, void *)>(0x49a530)(s, g_s.view, g_s.other_part); break;
        case S_PUSH: {
            BYTE *p = b.next() % 5 ? next_probe() : nullptr;
            r = rp(at<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *, DWORD)>(0x49a560)(s, e, p, b.next() % 5));
            break;
        }
        case S_APPEND: {
            BYTE *p = b.next() % 5 ? next_probe() : nullptr;
            r = rp(at<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x49a620)(s, p));
            break;
        }
        case S_REMOVE: r = at<DWORD(THISCALL *)(BYTE *, BYTE *, void *)>(0x49a650)(s, e, g_s.probe[b.next() % 8]); break;
        case S_CLEAR: r = at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x49a6b0)(s, e); break;
        case S_CONTAINS: r = at<DWORD(THISCALL *)(BYTE *, void *)>(0x49a730)(s, g_s.probe[b.next() % 8]) & 0xff; break;
        }
        if (side == 0)
            hooks_in();
        g_sd[side].ret = r;
        capture_side(9, false);
        free_self(9);
        free_world();
    }
    char d[300];
    const bool same = compare_sides(op == S_DRAW ? RET_NONE : RET_EAX, d, STACK_OP_NAMES[op]);
    tally.check(same, "ai_behaviors", d);
}

// ---- Destructors and the stack's constructor -------------------------------------------------------------

using Create0 = BYTE *(__cdecl *)();
using CreateN = BYTE *(__fastcall *)(int, int, int);
using DeletingDtor = void *(THISCALL *)(BYTE *, DWORD);
using Dtor = void(THISCALL *)(BYTE *);

// A Create'd object's fields before a destructor: the strings set, everything the constructors leave
// alone (CreateArray's heap and reused pool blocks differ between the sides) a pattern; owned objects
// stay null (Object_FreeFields deletes them), the target a fake part, the stack's array valid
void fill_object(BYTE *o, int cls, Build &b)
{
    const DWORD size = BEHAVIOR_CLASSES[cls].size;
    if (cls != 18)
        for (DWORD off = 8; off < size; off += 4)
            if (!is_zstring(cls, off) && !(cls == 16 && off == 0xc) && !(cls == 14 && (off == 0x24 || off == 0x28)))
                fl<DWORD>(o, off) = b.next();
    for (const DWORD *z = zstrings(cls); *z; z++)
        zs_set(o + *z, TEXTS[b.next() % NTEXTS]);
    if (cls == 5)
        fl<void *>(o, 0x18) = b.next() & 1 ? (void *)g_s.target : nullptr;
    if (cls == 18) {
        fl<DWORD>(o, 8) = b.next() & 1;
        fl<DWORD>(o, 0xc) = b.next() % 5;
        for (DWORD n = b.next() % 4; n--;)
            stack_append(o, g_s.probe[n]);
    }
}
// The bytes of a destroyed object (strings and the stack's data pointer not compared)
void capture_destroyed(DWORD *out, BYTE *o, DWORD size, int cls)
{
    memset(out, 0, 0x40);
    for (DWORD off = 0; off < size && off < 0x40; off += 4)
        out[off / 4] = is_zstring(cls, off) ? 0 : fl<DWORD>(o, off);
}

void dtor_case(Tally &tally, int cls, int mode, unsigned seed)
{
    const BehaviorClassDef &c = BEHAVIOR_CLASSES[cls];
    // the abstract levels' slot 1 and the out-of-line destructors on an object of a class of their size
    for (int side = 0; side < 2; side++) {
        g_side = side;
        memset(&g_sd[side], 0, sizeof g_sd[side]);
        Build b{seed * 2654435761u + (unsigned)cls};
        memset(&g_tr, 0, sizeof g_tr);
        g_tr.hash = 2166136261u;
        g_vseed = seed;
        g_vindex = 0;
        for (int i = 0; i < 8; i++)
            fl<DWORD>(g_s.probe[i], 0) = ptrv(g_s.probe_vt);
        if (mode < 2) {   // slot 1 with flags 0 or 1 on a single object
            BYTE *o = at<Create0>(c.create)();
            fill_object(o, cls, b);
            if (side == 0)
                hooks_out();
            void *r = at<DeletingDtor>(c.dtor)(o, (DWORD)mode);
            if (side == 0)
                hooks_in();
            g_sd[side].ret = r == o ? 'S' : ptrv(r);
            if (mode == 0) {
                capture_destroyed(g_sd[side].made, o, c.size, cls);
                pool_free(o, c.size);
            }
        } else {   // slot 1 with flags 2 or 3 on an array of 0-3
            const int n = (int)(b.next() % 4);
            BYTE *o = at<CreateN>(c.create_array)(n, 0, 0);
            for (int i = 0; i < n; i++)
                fill_object(o + i * c.size, cls, b);
            if (side == 0)
                hooks_out();
            void *r = at<DeletingDtor>(c.dtor)(o, (DWORD)mode);
            if (side == 0)
                hooks_in();
            g_sd[side].ret = r == o - 4 ? 'B' : ptrv(r);
            if (mode == 2) {
                for (int i = 0; i < n && i < 2; i++)
                    capture_destroyed(g_sd[side].made + i * 8, o + i * c.size, c.size < 0x20 ? c.size : 0x20, cls);
                heap_free(o - 4);
            }
        }
        g_sd[side].hash = g_tr.hash;
        g_sd[side].calls = g_tr.calls;
    }
    char d[300];
    const bool same = compare_sides(RET_EAX, d, c.name);
    tally.check(same, "ai_behaviors", d);
}

struct OutOfLine {
    const char *name;
    DWORD address;
    int cls;   // the class whose Create gives the object
    bool deleting;
    DWORD size;
};
const OutOfLine OUT_OF_LINE[] = {
    {"CEntityBehavior::Dtor", 0x4944e0, 9, false, 0xc},
    {"CEntityBehavior::DeletingDtor", 0x4944b0, 9, true, 0xc},
    {"CEntityBehaviorTurnToAngleBase::DeletingDtor", 0x494650, 2, true, 0x14},
    {"CEntityBehaviorTransition::DeletingDtor", 0x498230, 13, true, 0x18},
    {"CEntityBehaviorTurnToAngle::Dtor", 0x495360, 4, false, 0x1c},
    {"CEntityBehaviorIdle::Dtor", 0x498dc0, 14, false, 0x30},
    {"CEntityBehaviorPlaySequenceLooped::Dtor", 0x499cf0, 17, false, 0x14},
    {"CEntityBehaviorStack::Dtor", 0x49a430, 18, false, 0x20},
};

void out_of_line_case(Tally &tally, const OutOfLine &x, unsigned seed)
{
    const BehaviorClassDef &c = BEHAVIOR_CLASSES[x.cls];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        memset(&g_sd[side], 0, sizeof g_sd[side]);
        Build b{seed * 2654435761u + x.address};
        memset(&g_tr, 0, sizeof g_tr);
        g_tr.hash = 2166136261u;
        BYTE *o = at<Create0>(c.create)();
        fill_object(o, x.cls, b);
        const DWORD flags = x.deleting ? b.next() & 1 : 0;
        if (side == 0)
            hooks_out();
        DWORD r = 0;
        if (x.deleting)
            r = rp(at<DeletingDtor>(x.address)(o, flags)) == rp(o) ? 'S' : 0;
        else
            at<Dtor>(x.address)(o);
        if (side == 0)
            hooks_in();
        g_sd[side].ret = r;
        if (!flags) {
            capture_destroyed(g_sd[side].made, o, x.size, x.cls);
            pool_free(o, x.size);
        }
        g_sd[side].hash = g_tr.hash;
        g_sd[side].calls = g_tr.calls;
    }
    char d[300];
    const bool same = compare_sides(RET_EAX, d, x.name);
    tally.check(same, "ai_behaviors", d);
}

void stack_ctor_case(Tally &tally, unsigned seed)
{
    alignas(16) BYTE buf[0x20];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        memset(&g_sd[side], 0, sizeof g_sd[side]);
        Build b{seed};
        for (DWORD off = 0; off < sizeof buf; off += 4)
            fl<DWORD>(buf, off) = seed % 3 == 0 ? 0 : seed % 3 == 1 ? 0xffffffffu : b.next();
        if (side == 0)
            hooks_out();
        BYTE *r = at<BYTE *(THISCALL *)(BYTE *)>(0x49a3a0)(buf);
        if (side == 0)
            hooks_in();
        g_sd[side].ret = r == buf ? 'S' : ptrv(r);
        capture_destroyed(g_sd[side].made, buf, 0x20, 18);
        stack_dtor(buf);
    }
    char d[300];
    tally.check(compare_sides(RET_EAX, d, "CEntityBehaviorStack::Ctor"), "ai_behaviors", d);
}

// ---- Class getters and RegisterFields --------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register[8];
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    if (g_nregister < 8)
        g_register[g_nregister] = {ptrv(var), ptrv(name), bits(version), parent ? ptrv(parent->name) : 0, size,
                                   abstract & 0xff, ptrv(create), ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

// The class variables of a class and its parents (chain: also the parents' set to 0)
int chain_of(int cls, bool chain, DWORD *vars)
{
    int n = 0;
    for (int i = cls; i >= 0 && n < 8; i = chain ? BEHAVIOR_CLASSES[i].parent : -1)
        vars[n++] = BEHAVIOR_CLASSES[i].var;
    return n;
}

void getter_case(Tally &tally, int cls, DWORD getter, int mode)
{
    // mode 0: the variable set; 1: only the class's variable 0; 2: the whole chain 0
    DWORD vars[8], saved[8];
    const int nv_ = chain_of(cls, mode == 2, vars);
    RegisterCall calls[2][8];
    int nreg[2];
    DWORD results[2];
    for (int side = 0; side < 2; side++) {
        for (int i = 0; i < nv_; i++)
            saved[i] = *at<DWORD *>(vars[i]);
        if (mode)
            for (int i = 0; i < nv_; i++)
                *at<DWORD *>(vars[i]) = 0;
        g_nregister = 0;
        memset(g_register, 0, sizeof g_register);
        CodePatch p;
        patch_jump(p, 0x415780, (void *)&fake_register);
        if (side == 0)
            hooks_out();
        CClassInfo *made = at<Getter>(getter)();
        if (side == 0)
            hooks_in();
        unpatch(p);
        results[side] = mode == 0 ? (ptrv(made) == saved[0] ? 1 : 0) : (ptrv(made) == *at<DWORD *>(vars[0]) ? 1 : 0);
        nreg[side] = g_nregister;
        memcpy(calls[side], g_register, sizeof g_register);
        if (mode)
            for (int i = 0; i < nv_; i++) {
                if (*at<DWORD *>(vars[i]))
                    pool_free(*at<void **>(vars[i]), sizeof(CClassInfo));
                *at<DWORD *>(vars[i]) = saved[i];
            }
    }
    char d[200];
    wsprintfA(d, "[getter 0x%x mode %d: made %u / %u, registered %d / %d, size %x / %x, version %x / %x]", getter, mode, results[0],
              results[1], nreg[0], nreg[1], calls[0][0].size, calls[1][0].size, calls[0][0].version, calls[1][0].version);
    const int expected = mode == 0 ? 0 : mode == 1 ? 1 : nv_;
    tally.check(results[0] == 1 && results[1] == 1 && nreg[0] == nreg[1] && nreg[0] == expected &&
                    !memcmp(calls[0], calls[1], sizeof calls[0]),
                BEHAVIOR_CLASSES[cls].name, d);
}

bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    const char *x = a.c_str(), *y = b.c_str();
    while (*x && *x == *y)
        x++, y++;
    return *x == *y;
}
// Descriptors of the kinds these RegisterFields make, member by member (texts by content)
bool same_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || !same_text(a->name, b->name) || !same_text(a->help, b->help) ||
        !same_text(a->group, b->group) || !same_text(a->alt_name, b->alt_name) || !same_text(a->units, b->units) ||
        a->offset != b->offset || a->unknown14 != b->unknown14 || a->unknown18 != b->unknown18)
        return false;   // +0x24 is not written by the constructors
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    auto same = [&](DWORD from, DWORD to) { return memcmp(x + from, y + from, to - from) == 0; };
    switch (a->vftable) {
    case VT_BYTE: return same(0x28, 0x2d);
    case VT_BIT: return same(0x28, 0x29) && same(0x2c, 0x34);
    case VT_ENUM_INT:
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_OBJECT_REF: return same(0x28, 0x30);
    case VT_TYPED_RESOURCE: return same(0x28, 0x2e) && same(0x30, 0x38);
    case VT_STRING:
        return x[0x28] == y[0x28] && same_text(*reinterpret_cast<const ZString *>(x + 0x2c), *reinterpret_cast<const ZString *>(y + 0x2c)) &&
               same(0x30, 0x34);
    case VT_OBJECT_ARRAY:
        return same(0x28, 0x38) && same(0x3c, 0x45) &&
               same_desc(*reinterpret_cast<CFieldDesc *const *>(x + 0x38), *reinterpret_cast<CFieldDesc *const *>(y + 0x38));
    }
    return false;
}

void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' group '%s' alt '%s' units '%s' off %x %x %x", what, f->vftable, text(f->name), text(f->group),
        text(f->alt_name), text(f->units), f->offset, f->unknown14, f->unknown18);
    log("      %x %x %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
}

constexpr DWORD MAXF = 16;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, result, registered;
};
void run_register_fields(const BehaviorClassDef &c, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    memset(g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    const DWORD r = at<DWORD(__cdecl *)()>(c.register_fields)();
    unpatch(p);
    CClassInfo *ci = var;
    out.n = ci->num_fields;
    out.registered = (DWORD)g_nregister;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < ci->num_fields ? ci->fields[i] : nullptr;
    out.result = r;   // AddField's: the count
    if (ci->fields)
        heap_free(ci->fields);
    if (var_zero)
        pool_free(ci, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < MAXF; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void register_fields_case(Tally &tally, Tally &real, int cls, int var_zero)
{
    const BehaviorClassDef &c = BEHAVIOR_CLASSES[cls];
    Made m[2];
    RegisterCall calls[2][8];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks_out();
        run_register_fields(c, var_zero != 0, m[side]);
        memcpy(calls[side], g_register, sizeof g_register);
        if (side == 0)
            hooks_in();
    }
    bool same = m[0].n == m[1].n && m[0].result == m[1].result && m[0].registered == m[1].registered &&
                !memcmp(calls[0], calls[1], sizeof calls[0]);
    for (DWORD i = 0; i < m[0].n && i < MAXF && same; i++)
        same = same_desc(m[0].fields[i], m[1].fields[i]);
    char d[200];
    wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, registered %u / %u]", c.name, var_zero, m[0].n, m[1].n,
              m[0].result, m[1].result, m[0].registered, m[1].registered);
    if (!same && tally.mismatches < 3)
        for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
            log_desc("original", m[0].fields[i]);
            log_desc("reimpl", m[1].fields[i]);
        }
    tally.check(same, c.name, d);
    // Real data: the class's own descriptors as registered at startup
    const CClassInfo *ci = *at<CClassInfo **>(c.var);
    bool same_real = ci && ci->num_fields - ci->num_inherited == m[0].n;
    for (DWORD i = 0; same_real && i < m[0].n && i < MAXF; i++)
        same_real = same_desc(ci->fields[ci->num_inherited + i], m[0].fields[i]);
    wsprintfA(d, "[registered %s: %u own fields, made %u]", c.name, ci ? ci->num_fields - ci->num_inherited : 0, m[0].n);
    if (!same_real && real.mismatches < 2)
        for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
            log_desc("registered", ci->fields[ci->num_inherited + i]);
            log_desc("made", m[0].fields[i]);
        }
    real.check(same_real, c.name, d);
    free_made(m[0]);
    free_made(m[1]);
}

// ---- Real data -------------------------------------------------------------------------------------------


bool derived_from(const CClassInfo *c, const CClassInfo *base)
{
    for (int i = 0; c && i < 32; i++, c = c->parent)
        if (c == base)
            return true;
    return false;
}

int g_found[NBEHAVIOR_CLASSES], g_embedded[NBEHAVIOR_CLASSES];
int g_entities, g_items, g_other_items, g_ais;
// A part lies in every grid cell it touches: each counted once
constexpr int MAX_SEEN = 8192;
DWORD g_seen[MAX_SEEN];
int g_nseen;
bool seen(DWORD p)
{
    for (int i = 0; i < g_nseen; i++)
        if (g_seen[i] == p)
            return true;
    if (g_nseen < MAX_SEEN)
        g_seen[g_nseen++] = p;
    return false;
}

// One animated entity of the level: its stack and the behaviours on it, its angles
void real_entity(Tally &stacks, Tally &angles, BYTE *e)
{
    BYTE *s = e + 0x90;
    const DWORD n = fl<DWORD>(s, 0x18);
    BYTE **data = fl<BYTE **>(s, 0x14);
    char d[200];
    const bool sane = fl<DWORD>(s, 0) == 0x5f31cc && fl<DWORD>(s, 0x10) == 0x5f3228 && n <= fl<DWORD>(s, 0x1c) && n < 64 &&
                      (!n || readable(data, n * 4));
    wsprintfA(d, "[stack of %x: vtable %x, array %x, count %u]", ptrv(e), fl<DWORD>(s, 0), fl<DWORD>(s, 0x10), n);
    stacks.check(sane, "ai_behaviors", d);
    if (!sane)
        return;
    g_entities++;
    for (DWORD i = 0; i < n; i++) {
        BYTE *b = data[i];
        const CClassInfo *ci = class_of(b);
        int m = -1;
        for (int k = 0; k < NBEHAVIOR_CLASSES; k++)
            if (ci && ci == *at<CClassInfo **>(BEHAVIOR_CLASSES[k].var))
                m = k;
        g_items++;
        if (m < 0)
            g_other_items++;
        else
            g_found[m]++;
        DWORD r[2], cl[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            r[side] = at<DWORD(THISCALL *)(BYTE *, void *)>(0x49a730)(s, b) & 0xff;
            cl[side] = ptrv(vcall<CClassInfo *>(b, 2));
            if (side == 0)
                hooks_in();
        }
        wsprintfA(d, "[stack item %x: contains %u / %u, class %x / %x, vtable %x]", ptrv(b), r[0], r[1], cl[0], cl[1], fl<DWORD>(b, 0));
        stacks.check(r[0] == 1 && r[1] == 1 && cl[0] == cl[1] && cl[0] == ptrv(ci) && (m < 0 || fl<DWORD>(b, 0) == BEHAVIOR_CLASSES[m].vtable),
                     m < 0 ? "other" : BEHAVIOR_CLASSES[m].name, d);
    }
    // not on the stack: the entity itself
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks_out();
        r[side] = at<DWORD(THISCALL *)(BYTE *, void *)>(0x49a730)(s, e) & 0xff;
        if (side == 0)
            hooks_in();
    }
    stacks.check(r[0] == 0 && r[1] == 0, "ai_behaviors", "[contains the entity]");
    // the angles through slot 30 of the turn behaviours (on a dummy)
    DWORD a[2][2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks_out();
        a[side][0] = bits(at<float(THISCALL *)(BYTE *, BYTE *)>(0x4949b0)(nullptr, e));
        a[side][1] = bits(at<float(THISCALL *)(BYTE *, BYTE *)>(0x494cb0)(nullptr, e));
        if (side == 0)
            hooks_in();
    }
    wsprintfA(d, "[angles of %x: %x %x / %x %x]", ptrv(e), a[0][0], a[0][1], a[1][0], a[1][1]);
    angles.check(a[0][0] == a[1][0] && a[0][1] == a[1][1] && a[0][0] == fl<DWORD>(e, 0xc8) && a[0][1] == fl<DWORD>(e, 0xc4),
                 "ai_behaviors", d);
}

// The behaviours embedded in an AI of the level (the chase, patrol and go-to AIs embed turn, idle and
// move behaviours): found by their vtables; the class through slot 2 on both sides
void real_ai(Tally &embedded, BYTE *ai)
{
    const CClassInfo *ci = class_of(ai);
    if (!ci || ci->size > 0x1000 || !readable(ai, ci->size))
        return;
    g_ais++;
    for (DWORD off = 4; off + 0xc <= ci->size; off += 4) {
        const DWORD vt = fl<DWORD>(ai, off);
        int m = -1;
        for (int k = 0; k < NBEHAVIOR_CLASSES; k++)
            if (vt == BEHAVIOR_CLASSES[k].vtable)
                m = k;
        if (m < 0)
            continue;
        BYTE *b = ai + off;
        g_embedded[m]++;
        DWORD cl[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            cl[side] = ptrv(vcall<CClassInfo *>(b, 2));
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[%x +%x: class %x / %x, registered %x]", ptrv(ai), off, cl[0], cl[1], *at<DWORD *>(BEHAVIOR_CLASSES[m].var));
        embedded.check(cl[0] == cl[1] && cl[0] == *at<DWORD *>(BEHAVIOR_CLASSES[m].var), BEHAVIOR_CLASSES[m].name, d);
    }
}

void test_real(Tally &stacks, Tally &angles, Tally &registered, Tally &embedded)
{
    // The registered class infos: size, Create and CreateArray, the getters give them
    for (int m = 0; m < NBEHAVIOR_CLASSES; m++) {
        const BehaviorClassDef &c = BEHAVIOR_CLASSES[m];
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        registered.check(ci && ci->size == c.size && (!c.create || ptrv((void *)ci->create) == c.create) &&
                             (!c.create_array || ptrv((void *)ci->create_array) == c.create_array) && at<Getter>(c.getter)() == ci &&
                             (!c.getter2 || at<Getter>(c.getter2)() == ci),
                         c.name, d);
    }
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28)) {
        log("  ai_behaviors real data: no level grid found");
        stacks.check(false, "ai_behaviors", "[no level grid]");
        return;
    }
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16)) {
        stacks.check(false, "ai_behaviors", "[level grid unreadable]");
        return;
    }
    const CClassInfo *animated = at<Getter>(0x48dcc0)();
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *cell = &cells[i];
        if (cell->count > 10000 || !readable(cell->data, cell->count * 4))
            continue;
        for (DWORD k = 0; k < cell->count; k++) {
            BYTE *p = static_cast<BYTE *>(cell->data[k]);
            const CClassInfo *ci = class_of(p);
            if (!ci || seen(ptrv(p)))
                continue;
            if (derived_from(ci, animated) && readable(p, 0x200))
                real_entity(stacks, angles, p);
            const LayerPart *lp = reinterpret_cast<const LayerPart *>(p);
            if (readable(p, 0x48) && lp->activity.count <= 1000 && readable(lp->activity.data, lp->activity.count * 4))
                for (DWORD j = 0; j < lp->activity.count; j++)
                    real_ai(embedded, static_cast<BYTE *>(lp->activity.data[j]));
        }
    }
    log("  ai_behaviors real data: %d animated entities, %d behaviours on their stacks (%d of other classes), %d AIs", g_entities,
        g_items, g_other_items, g_ais);
    for (int m = 0; m < NBEHAVIOR_CLASSES; m++)
        if (g_found[m] || g_embedded[m])
            log("    %s: %d on stacks, %d embedded in AIs", BEHAVIOR_CLASSES[m].name, g_found[m], g_embedded[m]);
}

}  // namespace

ZAX_CHECKS(ai_behaviors)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally slots{"ai_behaviors: behaviour slots and helpers on a fake entity"}, stack{"ai_behaviors: CEntityBehaviorStack on probes"},
        dtors{"ai_behaviors: destructors and the stack's constructor"}, getters{"ai_behaviors: class getters and RegisterFields"},
        fields{"ai_behaviors: RegisterFields against the registered fields (real data)"},
        registered{"ai_behaviors: registered class infos (real data)"}, stacks{"ai_behaviors: stacks of the running level (real data)"},
        angles{"ai_behaviors: angles of the level's entities (real data)"},
        embedded{"ai_behaviors: behaviours embedded in the level's AIs (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    init_fakes();
    code_writable(true);
    world_on();
    for (const SlotTest &t : SLOT_TESTS)
        for (unsigned seed = 1; seed <= (unsigned)scaled(t.count); seed++)
            slot_case(slots, t, seed * 31 + t.address);
    for (int op = 0; op < NSTACK_OPS; op++)
        for (unsigned seed = 1; seed <= (unsigned)scaled(500); seed++)
            stack_case(stack, op, seed * 17 + (unsigned)op);
    for (int c = 0; c < NBEHAVIOR_CLASSES; c++)
        if (BEHAVIOR_CLASSES[c].create)
            for (int mode = 0; mode < 4; mode++)
                for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
                    dtor_case(dtors, c, mode, seed * 5 + (unsigned)c);
    for (const OutOfLine &x : OUT_OF_LINE)
        for (unsigned seed = 1; seed <= (unsigned)scaled(24); seed++)
            out_of_line_case(dtors, x, seed);
    for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
        stack_ctor_case(dtors, seed);
    world_off();
    slots.report();
    stack.report();
    dtors.report();
    for (int c = 0; c < NBEHAVIOR_CLASSES; c++) {
        const BehaviorClassDef &bc = BEHAVIOR_CLASSES[c];
        for (int mode = 0; mode < 3; mode++) {
            getter_case(getters, c, bc.getter, mode);
            if (bc.getter2)
                getter_case(getters, c, bc.getter2, mode);
        }
        if (bc.register_fields)
            for (int var_zero = 0; var_zero < 2; var_zero++)
                register_fields_case(getters, fields, c, var_zero);
    }
    getters.report();
    fields.report();
    test_real(stacks, angles, registered, embedded);
    registered.report();
    stacks.report();
    angles.report();
    embedded.report();
    code_writable(false);
    restore_bytes(saved_random);
    log("  ai_behaviors: %u ms", GetTickCount() - start);
}

}  // namespace zax
