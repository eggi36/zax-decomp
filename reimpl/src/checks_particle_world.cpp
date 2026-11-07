// The fake world of the particle checks (see checks_particle_world.h).
#include "checks_particle_world.h"
#include "class_info.h"

namespace zax {
int hooks_particles(Hook *out);
int hooks_particle_start(Hook *out);
}  // namespace zax

namespace zax::pw {

// ---- Memory helpers (the fixed ones of checks_geometry.cpp) ----------------------------------------------

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

bool derived_from(const CClassInfo *c, const CClassInfo *base)
{
    for (int i = 0; c && i < 32; i++, c = c->parent)
        if (c == base)
            return true;
    return false;
}

// ---- State ------------------------------------------------------------------------------------------------

Block g_block;
DWORD g_fvt[NTYPES][256];
int g_side;
unsigned g_vseed;
DWORD g_vindex;
bool g_trapped;
Policy g_policy;
Log g_log[2];
static Block g_block0;   // side 0's block for the report of a difference

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
float frange(float lo, float hi)
{
    const DWORD v = nv();
    if ((v & 15) == 0 && !g_policy.finite) {
        const DWORD specials[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, bits(lo), bits(hi), 0x7149f2cau, 0x8ac7230au};
        return fbits(specials[(v >> 4) & 7]);
    }
    return lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
}

static DWORD stack_base, stack_limit;
static void stack_bounds()
{
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(stack_base), "=r"(stack_limit));
}
static bool in_block(DWORD v) { return v >= ptrv(&g_block) && v < ptrv(&g_block) + sizeof g_block; }
static bool in_fvt(DWORD v) { return v >= ptrv(g_fvt) && v < ptrv(g_fvt) + sizeof g_fvt; }
// A value as both sides see it: fakes, the exe, small numbers as they are; the stack by a tag; other
// readable memory by its first dword
static DWORD rp(DWORD v)
{
    if (v < 0x10000 || in_block(v) || in_fvt(v) || (v >= 0x400000 && v < IMAGE_END))
        return v;
    if (v >= stack_limit && v < stack_base)
        return 0xb0000000u;
    if (!(v & 3) && readable(at<void *>(v), 4))
        return 0xa0000000u | (*at<DWORD *>(v) & 0x0fffffffu);
    return v;
}
static unsigned text_hash(DWORD p)
{
    if (!p)
        return 0;
    if (!readable(at<void *>(p), 1))
        return 0xbad;
    unsigned h = 2166136261u;
    for (const char *s = at<const char *>(p); readable(s, 1) && *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    return h;
}
static DWORD dword_at(DWORD p, DWORD off) { return p && readable(at<void *>(p + off), 4) ? *at<DWORD *>(p + off) : 0xdeadbeefu; }

// ---- Stand-ins ---------------------------------------------------------------------------------------------

struct Call {
    DWORD *a;
    DWORD ecx, edx;
    long double fret;
    bool fset;
};
using Custom = DWORD (*)(Call &);
struct Spec {
    DWORD id;
    const char *regs;   // ecx, edx: p pointer, v value, s string, f two floats behind it, - not used
    const char *args;   // p pointer, v value, b low byte, f two floats behind it, d the dword behind it, S a sweep's movement, s string,
                        // m a message object, o / x not recorded
    char ret;           // v void (a junk value), i int, b bool (al), f st(0), p a fake of `type`, n the same or 0, c custom
    int type;
    bool caller_pops;
    Custom custom;
};

// The fakes of each type: the first object and the count
static const int TYPE_FIRST[NTYPES] = {O_ENTITY, O_BEHAVIOR, O_INVENTORY, O_ITEM, O_DEF, O_WORLD, O_APP, O_REPORTER, O_GAMETYPE,
                                       O_VIEW, O_PALOBJ, O_AI, 0, 0, O_OTHER, O_LAYER};
static const int TYPE_COUNT[NTYPES] = {N_ENTITY, N_BEHAVIOR, N_INVENTORY, N_ITEM, N_DEF, 1, 1, 1, 1, 1, 1, N_AI, 0, 0, N_OTHER, 1};
static BYTE *pick(int type, bool may_be_null)
{
    const int *first = TYPE_FIRST, *count = TYPE_COUNT;
    const DWORD v = nv();
    if (may_be_null && (g_policy.always_null & (1u << type)))
        return nullptr;
    if (may_be_null && !(g_policy.nonnull & (1u << type)) && (v & 7) == 0)
        return nullptr;
    if (!count[type])
        return nullptr;
    return obj(first[type] + (int)((v >> 3) % (DWORD)count[type]));
}
static DWORD small_int()
{
    const DWORD v = nv();
    if ((v & 15) == 0)
        return 0xffffffffu;
    if ((v & 15) == 1)
        return v >> 4;
    return (v >> 4) % 8;
}
static DWORD write_pos(DWORD p, float lo, float hi)
{
    if (p && readable(at<void *>(p), 8)) {
        at<float *>(p)[0] = frange(lo, hi);
        at<float *>(p)[1] = frange(lo, hi);
    }
    return p;
}
static DWORD c_pos(Call &c) { return write_pos(c.a[0], 0.0f, 4000.0f); }
static DWORD c_random_range(Call &c)
{
    const DWORD v = nv();
    if ((v & 7) == 0)
        c.fret = frange(-5.0f, 50.0f);
    else
        c.fret = (long double)fbits(c.a[0]) + ((long double)fbits(c.a[1]) - fbits(c.a[0])) * (long double)(int)(v >> 8) * (1.0L / 16777216.0L);
    c.fset = true;
    return nv();
}
static DWORD c_model(Call &) { return ptrv(obj(O_MODEL)); }
// Object_IsKindOf: the same answer for the same object and class within a case (the code asks twice
// and relies on it); the upper bytes from the value sequence
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    DWORD h = g_vseed ^ (c.ecx * 0x9e3779b9u) ^ (c.a[0] * 0x85ebca6bu);
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return (v & 0xffffff00u) | ((h >> 7) & 1);
}

// Random_Between 0x55c4e0 (lo, hi): an integer in [lo, hi] (lo when hi is below)
static DWORD c_between(Call &c)
{
    const int lo = (int)c.a[0], hi = (int)c.a[1];
    if (hi <= lo)
        return (DWORD)lo;
    const DWORD span = (DWORD)(hi - lo) + 1;
    return (DWORD)lo + (span ? nv() % span : nv());
}
// The game type (0x59ff80): one answer per case (the code asks twice), now and then none
static DWORD c_game_type(Call &)
{
    nv();
    return (g_vseed * 0x9e3779b9u >> 29) == 0 ? 0 : ptrv(obj(O_GAMETYPE));
}
// The parent position helpers (part, point): the point moved a little (2 or 3 floats)
static DWORD shift(Call &c, int n)
{
    if (c.a[0] && readable(at<void *>(c.a[0]), 4u * (DWORD)n))
        for (int i = 0; i < n; i++)
            at<float *>(c.a[0])[i] = at<float *>(c.a[0])[i] + frange(-64.0f, 64.0f);
    return nv();
}
static DWORD c_shift2(Call &c) { return shift(c, 2); }
static DWORD c_shift3(Call &c) { return shift(c, 3); }
// An entity's velocity (slot 114): the same three floats every time
static DWORD c_velocity_ptr(Call &) { return ptrv(obj(O_VEL)); }
// A fake start point type's slot 25 (out, direction): three floats written
static DWORD c_start_point(Call &c)
{
    if (c.a[0] && readable(at<void *>(c.a[0]), 12))
        for (int i = 0; i < 3; i++)
            at<float *>(c.a[0])[i] = frange(-40.0f, 40.0f);
    return nv();
}

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    S(0x48b590, "p-", "", 'p', T_OTHER),                     // Entity_SequenceObject
    S(0x492080, "p-", "", 'f', 0),                           // EntityProjectile_Duration
    S(0x48feb0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrame
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    S(0x4fc8a0, "p-", "v", 'v', 0),                          // CLayerPart_LayerAdd (a sound)
    SC(0x48b290, "p-", "", c_model),                         // Entity_Model
    S(0x48b770, "p-", "v", 'f', 0),                          // Entity_SetRenderHeightFloat
    S(0x48b6b0, "p-", "v", 'i', 0),                          // Entity_SetRenderHeight
    SC(0x55c550, "p-", "vv", c_random_range),                // a random float
    SC(0x55c4e0, "p-", "vv", c_between),                     // a random integer
    SC(0x4fb710, "p-", "f", c_shift2),                       // CLayerPart_AddParentPositions
    SC(0x4fb750, "p-", "f", c_shift3),                       // CLayerPart_AddParentPositions3
    SC(0x4fb7b0, "p-", "f", c_shift3),                       // CLayerPart_SubParentPositions3
    S(0x4fc6f0, "p-", "", 'n', T_ENTITY),                    // CLayerPart_Root
    S(0x5176f0, "p-", "vpvvv", 'n', T_ENTITY),               // Entity_CreateFromModel
    S(0x416760, "p-", "vv", 'p', T_AI),                      // Object_Clone
    S(0x4fc710, "p-", "p", 'v', 0),                          // CLayerPart_AddChild
    S(0x4eb6f0, "p-", "p", 'v', 0),                          // CLayer::AddPart
    S(0x4fbc50, "p-", "p", 'v', 0),                          // CLayerPart_AddAI
    S(0x491230, "p-", "vv", 'v', 0),                         // EntityMovable_SetTarget
    S(0x545a00, "p-", "vv", 'v', 0),                         // CSweep_Ctor
    S(0x545c90, "p-", "", 'v', 0),                           // SweepHit_Init
    S(0x4e8d40, "p-", "pppvvvpv", 'b', 0),                   // CLayer_SweepPoint
    S(0x4ea8c0, "p-", "vf", 'v', 0),                         // CLayer::SendSimpleSound
    SC(0x59ff80, "p-", "", c_game_type),                     // the game type
    S(0x417230, "p-", "", 'i', 0),                           // CObject_ResetFields
    S(0x417500, "p-", "ssv", 'i', 0),                        // CObject_ReadOwnLine
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 200;
constexpr int NTHUNKS = VBASE + NTYPES * 256;
static_assert(NDIRECT <= VBASE);

// The fakes' slots (type, slot, spec)
struct VSpec {
    int type, slot;
    Spec spec;
};
#define V(t, s, args, ret, rt) {t, s, {0, "p-", args, ret, rt, false, nullptr}}
#define VC(t, s, args, fn) {t, s, {0, "p-", args, 'c', 0, false, fn}}
static const VSpec VIRTUAL[] = {
    V(T_ENTITY, 1, "v", 'i', 0),
    V(T_ENTITY, 35, "v", 'v', 0),
    V(T_ENTITY, 41, "vv", 'v', 0),
    VC(T_ENTITY, 43, "o", c_pos),
    V(T_ENTITY, 46, "", 'f', 0),
    V(T_ENTITY, 54, "", 'i', 0),
    V(T_ENTITY, 55, "", 'f', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 68, "", 'n', T_ENTITY),
    V(T_ENTITY, 84, "", 'f', 0),
    V(T_ENTITY, 104, "vv", 'v', 0),
    V(T_ENTITY, 107, "f", 'v', 0),
    VC(T_ENTITY, 114, "", c_velocity_ptr),
    V(T_ENTITY, 118, "f", 'v', 0),
    V(T_AI, 42, "f", 'v', 0),
    V(T_AI, 43, "bbv", 'v', 0),
    V(T_AI, 44, "v", 'v', 0),
    V(T_GAMETYPE, 33, "", 'b', 0),
    V(T_OTHER, 1, "v", 'i', 0),
    VC(T_OTHER, 25, "op", c_start_point),
    V(T_OTHER, 26, "", 'f', 0),
    V(T_OTHER, 27, "v", 'v', 0),
    V(T_OTHER, 28, "p", 'v', 0),
};
#undef V
#undef VC
static const Spec *vspec(int type, int slot)
{
    for (const VSpec &v : VIRTUAL)
        if (v.type == type && v.slot == slot)
            return &v.spec;
    return nullptr;
}

extern "C" {
extern char pw_thunks[];
DWORD pw_pop, pw_ret_float, pw_edx;
long double pw_fret;
DWORD __cdecl pw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _pw_thunks\n"
        "_pw_thunks:\n"
        ".set pw_i, 0\n"
        ".rept 4296\n"
        "  movl $pw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _pw_common - . - 4\n"
        ".set pw_i, pw_i + 1\n"
        ".endr\n"
        "_pw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _pw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _pw_ret_float\n"
        "  je 1f\n"
        "  fldt _pw_fret\n"
        "1:\n"
        "  movl _pw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _pw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(pw_thunks) + 10u * (DWORD)i; }

static void record(DWORD id, const Spec &s, Call &c)
{
    DWORD e[9] = {id, 0, 0, 0, 0, 0, 0, 0, 0};
    int k = 1;
    auto value = [&](char kind, DWORD v) -> DWORD {
        switch (kind) {
        case 'p': return rp(v);
        case 'v': return v;
        case 'b': return v & 0xff;
        case 's': return (DWORD)text_hash(v);
        case 'f': return dword_at(v, 0) * 31u + dword_at(v, 4);
        case 'd': return dword_at(v, 0);
        case 'S': return dword_at(v, 8) * 31u + dword_at(v, 0xc);
        case 'm': return dword_at(v, 0) * 31u + dword_at(v, 4) * 7u + (dword_at(v, 8) & 0xff);
        default: return 0u;
        }
    };
    e[k++] = value(s.regs[0], c.ecx);
    e[k++] = value(s.regs[1], c.edx);
    for (int i = 0; s.args[i] && k < 9; i++)
        e[k++] = value(s.args[i], c.a[i]);
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

DWORD __cdecl pw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
{
    const Spec *s;
    DWORD id;
    if (idx < (DWORD)NDIRECT) {
        s = &DIRECT[idx];
        id = s->id;
    } else {
        const DWORD v = idx - VBASE;
        const int type = (int)(v >> 8), slot = (int)(v & 0xff);
        s = vspec(type, slot);
        id = 0x100000u * (DWORD)(type + 1) + (DWORD)slot;
        if (!s) {
            g_trapped = true;
            Log &l = g_log[g_side];
            const DWORD e[9] = {0xdead0000u, id, rp(ecx), 0, 0, 0, 0, 0, 0};
            l.hash = fnv(l.hash, e, sizeof e);
            if (l.calls < LOGN)
                memcpy(l.ev[l.calls], e, sizeof e);
            l.calls++;
            pw_pop = 4;
            pw_ret_float = 0;
            pw_edx = 0;
            return 0;
        }
    }
    Call c{frame + 1, ecx, edx, 0, false};
    record(id, *s, c);
    DWORD r;
    switch (s->ret) {
    case 'i': r = small_int(); break;
    case 'b': {
        const DWORD v = nv();
        r = (v & 0xffffff00u) | ((v >> 7) & 1);
        break;
    }
    case 'f':
        c.fret = frange(-10.0f, 1000.0f);
        c.fset = true;
        r = nv();
        break;
    case 'p': r = ptrv(pick(s->type, false)); break;
    case 'n': r = ptrv(pick(s->type, true)); break;
    case 'c': r = s->custom(c); break;
    default: r = nv(); break;
    }
    pw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    pw_ret_float = c.fset ? 1 : 0;
    pw_fret = c.fret;
    pw_edx = nv();
    return r;
}

// ---- The stand-ins and hooks in and out --------------------------------------------------------------------

static CodePatch g_patches[NDIRECT];
static DWORD g_text_protection;
constexpr DWORD TEXT_FROM = 0x401000, TEXT_SIZE = 0x1e9000;
static void write_jump(CodePatch &p, DWORD address, DWORD target)
{
    p.site = at<BYTE *>(address);
    memcpy(p.saved, p.site, 5);
    p.site[0] = 0xe9;
    *(INT32 *)(p.site + 1) = (INT32)(target - (address + 5));
}
bool world_on()
{
    stack_bounds();
    for (int t = 0; t < NTYPES; t++)
        for (int s = 0; s < 256; s++)
            g_fvt[t][s] = thunk(VBASE + t * 256 + s);
    static_assert(NTHUNKS == 4296, "the thunk count of pw_thunks");
    for (int k = 0; k < NTHUNKS; k += 997) {
        const BYTE *th = reinterpret_cast<const BYTE *>(pw_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != (DWORD)k || th[5] != 0xe9) {
            log("  ai checks: recorder thunk %d is broken", k);
            return false;
        }
    }
    DWORD old;
    if (!VirtualProtect(at<void *>(TEXT_FROM), TEXT_SIZE, PAGE_EXECUTE_READWRITE, &old))
        return false;
    g_text_protection = old;
    for (int i = 0; i < NDIRECT; i++)
        write_jump(g_patches[i], DIRECT[i].id, thunk(i));
    FlushInstructionCache(GetCurrentProcess(), at<void *>(TEXT_FROM), TEXT_SIZE);
    return true;
}
void world_off()
{
    for (int i = NDIRECT; i--;)
        memcpy(g_patches[i].site, g_patches[i].saved, 5);
    DWORD old;
    VirtualProtect(at<void *>(TEXT_FROM), TEXT_SIZE, g_text_protection, &old);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(TEXT_FROM), TEXT_SIZE);
}

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 512;
static SavedSite g_sites[MAX_SITES];
static Hook g_hooks[MAX_SITES];
static int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    int (*const tables[])(Hook *) = {hooks_particles, hooks_particle_start};
    const char *const names[] = {"particles", "particle_start"};
    for (int t = 0; t < 2; t++) {
        const int n = tables[t](nullptr);
        if (!hook_sites_fit(names[t], n, MAX_SITES - g_nsites))
            continue;
        tables[t](g_hooks);
        for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
            BYTE *site = at<BYTE *>(g_hooks[i].address);
            if (!memcmp(site, g_hooks[i].expected, 5))
                continue;   // not hooked (Disable=)
            SavedSite &s = g_sites[g_nsites++];
            s.site = site;
            memcpy(s.bytes, site, 5);
            memcpy(site, g_hooks[i].expected, 5);
        }
    }
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x528000), 0x10000);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x528000), 0x10000);
    g_nsites = 0;
}

// ---- The world's globals -----------------------------------------------------------------------------------

struct Global {
    DWORD address, size;
};
static const Global GLOBALS[] = {
    {0x6c2080, 4}, {0x713f14, 4}, {0x6c02cc, 4}, {0x6c0c08, 4}, {0x6c0c38, 4}, {0x6c0c3c, 4}, {0x6cfdd8, 4}, {0x6cfe08, 4},
    {0x6cfe0c, 4}, {0x711110, 4}, {0x6bd2e0, 1}, {0x6bd2e4, 4}, {0x6bd2e8, 1}, {0x62ea2c, 4}, {0x6d0af8, 1}, {0x6d0b00, 16},
    {0x6d0b18, 1}, {0x6d0b20, 16}, {0x6d0b41, 1}, {0x6d0b60, 16},
    {0x6bd790, 4}, {0x6bd794, 4},   // the damage type list's records and count
};
constexpr int NGLOBALS = sizeof GLOBALS / sizeof GLOBALS[0];
static BYTE g_saved_globals[NGLOBALS][16];
static DWORD g_case_globals[NGLOBALS][4];   // the values of the case (set by build_world)
void globals_on()
{
    for (int i = 0; i < NGLOBALS; i++) {
        memcpy(g_saved_globals[i], at<void *>(GLOBALS[i].address), GLOBALS[i].size);
        memcpy(at<void *>(GLOBALS[i].address), g_case_globals[i], GLOBALS[i].size);
    }
}
static unsigned globals_hash()
{
    unsigned h = 2166136261u;
    for (int i = 0; i < NGLOBALS; i++)
        h = fnv(h, at<void *>(GLOBALS[i].address), GLOBALS[i].size);
    return h;
}
void globals_off()
{
    for (int i = NGLOBALS; i--;)
        memcpy(at<void *>(GLOBALS[i].address), g_saved_globals[i], GLOBALS[i].size);
}

// ---- Building the world ------------------------------------------------------------------------------------

void build_world(unsigned seed)
{
    Gen g{seed * 2654435761u + 77};
    DWORD *all = reinterpret_cast<DWORD *>(&g_block);
    for (DWORD i = 0; i < sizeof g_block / 4; i++)
        all[i] = g.next();
    auto vt = [](int o, int type) { fl<DWORD>(obj(o), 0) = ptrv(g_fvt[type]); };
    for (int i = 0; i < N_ENTITY; i++) {
        BYTE *e = obj(O_ENTITY + i);
        vt(O_ENTITY + i, T_ENTITY);
        fl<DWORD>(e, 0x1c) = g.next();
        fl<DWORD>(e, 0x24) = g.next() % 3 - 1;
        fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        fl<DWORD>(e, 0x9c) = g.next() % 4;
        fl<DWORD>(e, 0x12c) = g.next() % 2;
        fl<DWORD>(e, 0xc8) = g.fbits_(0.0f, 6.3f);
    }
    for (int i = 0; i < N_BEHAVIOR; i++)
        vt(O_BEHAVIOR + i, T_BEHAVIOR);
    for (int i = 0; i < N_INVENTORY; i++)
        vt(O_INVENTORY + i, T_INVENTORY);
    for (int i = 0; i < N_ITEM; i++) {
        vt(O_ITEM + i, T_ITEM);
        fl<DWORD>(obj(O_ITEM + i), 8) = g.next() % 4;
        fl<BYTE>(obj(O_ITEM + i), 0x10) = (BYTE)(g.next() % 2);
    }
    for (int i = 0; i < N_DEF; i++) {
        BYTE *d = obj(O_DEF + i);
        vt(O_DEF + i, T_DEF);
        fl<DWORD>(d, 0x18) = g.next() % 4;
        const DWORD k = g.next() % 4;
        fl<DWORD>(d, 0x1c) = k == 0 ? 0 : ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM)));   // the template item
        fl<DWORD>(d, 0x4c) = g.fbits_(-10.0f, 10.0f);
        fl<DWORD>(d, 0x50) = g.next() % 6;
    }
    vt(O_WORLD, T_WORLD);
    fl<DWORD>(obj(O_WORLD), 0x2a8) = g.next() % 4 ? ptrv(obj(O_VIEW)) : 0;
    vt(O_APP, T_APP);
    fl<DWORD>(obj(O_APP), 0x2e0) = ptrv(obj(O_APP_RULES_HOLDER));
    fl<DWORD>(obj(O_APP), 0x340) = ptrv(obj(O_APP_340));
    fl<DWORD>(obj(O_APP), 0x344) = ptrv(obj(O_APP_344));
    fl<DWORD>(obj(O_APP_RULES_HOLDER), 0x38) = g.next() % 4 ? ptrv(obj(O_RULES)) : 0;
    vt(O_REPORTER, T_REPORTER);
    fl<DWORD>(obj(O_REPORTER), 4) = ptrv(obj(O_PALOBJ));
    vt(O_GAMETYPE, T_GAMETYPE);
    fl<DWORD>(obj(O_GAMETYPE), 0x10) = g.next() % 2 ? 0 : g.next();
    vt(O_VIEW, T_VIEW);
    vt(O_PALOBJ, T_PALOBJ);
    vt(O_LAYER, T_LAYER);
    for (int i = 0; i < N_OTHER; i++)
        vt(O_OTHER + i, T_OTHER);   // sequences, a fake start point type
    for (int i = 0; i < N_AI; i++) {
        BYTE *a = obj(O_AI + i);
        vt(O_AI + i, T_AI);
        fl<DWORD>(a, 0x10) = g.next() % 3 ? ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM))) : 0;
        fl<DWORD>(a, 0x14) = g.next() % 4;
        fl<BYTE>(a, 0x18) = (BYTE)(g.next() % 2);
        fl<DWORD>(a, 0x1c) = g.next() % 4;
        fl<DWORD>(a, 0x20) = g.fbits_(0.01f, 30.0f);
        fl<DWORD>(a, 0x24) = g.fbits_(0.0f, 10.0f);
        fl<DWORD>(a, 0x28) = g.fbits_(-5.0f, 30.0f);
        fl<DWORD>(a, 0x2c) = g.fbits_(0.0f, 30.0f);
        fl<double>(a, 0x30) = (double)g.f(0.0f, 5000.0f);
    }
    BYTE *p = obj(O_PLAYER);
    fl<DWORD>(p, 0x5c) = g.fbits_(-1.0f, 1.0f);
    fl<DWORD>(p, 0x60) = g.fbits_(-1.0f, 1.0f);
    BYTE *s = obj(O_SETTINGS);
    fl<DWORD>(s, 0xc) = g.fbits_(0.0f, 0.5f);
    fl<DWORD>(s, 0x10) = g.fbits_(0.3f, 1.0f);
    fl<DWORD>(s, 0x14) = g.fbits_(0.0f, 0.5f);
    fl<DWORD>(s, 0x18) = g.fbits_(0.3f, 1.0f);
    fl<DWORD>(s, 0x28) = g.next() % 100;
    fl<DWORD>(s, 0x2c) = g.next() % 200;
    fl<DWORD>(obj(O_PROJECTILE), 0x20) = g.fbits_(0.0f, 10.0f);
    fl<DWORD>(obj(O_PALETTE), 0) = g.next() % 4 ? ptrv(obj(O_PALETTE_DATA)) : 0;
    for (int i = 0; i < N_POLY; i++) {
        BYTE *q = obj(O_POLY + i);
        fl<DWORD>(q, 0) = ptrv(obj(O_POINTS + i));
        fl<DWORD>(q, 4) = 0;
        fl<DWORD>(q, 8) = 2 + g.next() % 7;
        for (DWORD k = 0; k < 32; k++)
            fl<DWORD>(obj(O_POINTS + i), k * 4) = g.fbits_(-500.0f, 500.0f);
    }
    // the item catalogue's and the model list's records (4 each, 0x80 bytes)
    for (int r = 0; r < 4; r++) {
        BYTE *c = obj(O_CAT_RECORDS) + r * 0x80, *m = obj(O_MODEL_RECORDS) + r * 0x80;
        fl<DWORD>(c, 0x78) = g.next() % 4 ? ptrv(obj(O_DEF + (int)(g.next() % N_DEF))) : 0;
        fl<DWORD>(m, 0x78) = g.next() % 4 ? ptrv(obj(O_MODEL)) : 0;
        c[0x7f] = (BYTE)(!g_policy.loading_never && g.next() % 8 == 0);
        m[0x7f] = (BYTE)(!g_policy.loading_never && g.next() % 8 == 0);
    }
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    g_case_globals[0][0] = ptrv(obj(O_WORLD));
    g_case_globals[1][0] = ptrv(obj(O_APP));
    g_case_globals[2][0] = ptrv(obj(O_REPORTER));
    g_case_globals[3][0] = g_policy.real_list_vtables ? *at<DWORD *>(0x6c0c08) : ptrv(g_fvt[T_CATALOGUE]);
    g_case_globals[4][0] = g.next() % 8 || g_policy.loading_never ? ptrv(obj(O_CAT_RECORDS)) : 0;
    g_case_globals[5][0] = 4;
    g_case_globals[6][0] = g_policy.real_list_vtables ? *at<DWORD *>(0x6cfdd8) : ptrv(g_fvt[T_MODELS]);
    g_case_globals[7][0] = g.next() % 8 || g_policy.loading_never ? ptrv(obj(O_MODEL_RECORDS)) : 0;
    g_case_globals[8][0] = 4;
    g_case_globals[9][0] = g.next() % 2 ? 0 : g.next();
    g_case_globals[10][0] = g.next() % 2;
    g_case_globals[11][0] = ptrv(obj(O_FILEBUF));
    g_case_globals[12][0] = g.next() % 2;
    g_case_globals[13][0] = ptrv(obj(O_62EA2C));
    for (int i = 14; i < 20; i++)
        for (int k = 0; k < 4; k++)
            g_case_globals[i][k] = GLOBALS[i].size == 1 ? (g.next() % 2) : g.next();
    // the damage type list: 4 records, the object +0x78 a fake or 0, now and then loading
    for (int r = 0; r < 4; r++) {
        BYTE *d = obj(O_DMG_RECORDS) + r * 0x80;
        fl<DWORD>(d, 0x78) = g.next() % 4 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
        d[0x7f] = (BYTE)(!g_policy.loading_never && g.next() % 8 == 0);
    }
    g_case_globals[20][0] = g.next() % 8 || g_policy.loading_never ? ptrv(obj(O_DMG_RECORDS)) : 0;
    g_case_globals[21][0] = 4;
    // the particle entities' velocity
    for (int i = 0; i < 3; i++)
        fl<DWORD>(obj(O_VEL), 4 * (DWORD)i) = g.fbits_(-400.0f, 400.0f);
}

void side_begin(int side, unsigned seed)
{
    g_side = side;
    g_vseed = seed;
    g_vindex = 0;
    g_trapped = false;
    memset(&g_log[side], 0, sizeof g_log[side]);
    g_log[side].hash = 2166136261u;
}
DWORD side_end()
{
    if (g_side == 0)
        memcpy(&g_block0, &g_block, sizeof g_block);
    return fnv(globals_hash(), &g_block, sizeof g_block);
}

bool compare(Tally &t, const char *what, unsigned seed, const Side *s, DWORD mask)
{
    const bool same = ((s[0].eax ^ s[1].eax) & mask) == 0 && s[0].state == s[1].state && s[0].logh == s[1].logh &&
                      s[0].calls == s[1].calls && !s[0].trapped && !s[1].trapped;
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x, calls %u / %u, log %x / %x, state %x / %x, trapped %d / %d]", what, seed, s[0].eax, s[1].eax,
              s[0].calls, s[1].calls, s[0].logh, s[1].logh, s[0].state, s[1].state, s[0].trapped, s[1].trapped);
    if (!same && t.mismatches < 4) {
        const int n = (int)(s[0].calls < s[1].calls ? s[0].calls : s[1].calls);
        for (int i = 0; i < n && i < LOGN; i++)
            if (memcmp(g_log[0].ev[i], g_log[1].ev[i], sizeof g_log[0].ev[i])) {
                const DWORD *a = g_log[0].ev[i], *b = g_log[1].ev[i];
                log("    call %d: %x %x %x | %x %x %x %x %x %x / %x %x %x | %x %x %x %x %x %x", i, a[0], a[1], a[2], a[3], a[4], a[5], a[6],
                    a[7], a[8], b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8]);
                break;
            }
        if (s[0].calls != s[1].calls) {
            const int m = (int)(s[0].calls > s[1].calls ? s[0].calls : s[1].calls);
            const int side = s[0].calls > s[1].calls ? 0 : 1;
            if (n < LOGN && n < m) {
                const DWORD *a = g_log[side].ev[n];
                log("    side %d call %d: %x %x %x | %x %x %x %x", side, n, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
            }
        }
        const DWORD *x = reinterpret_cast<const DWORD *>(&g_block0), *y = reinterpret_cast<const DWORD *>(&g_block);
        for (DWORD i = 0; i < sizeof g_block / 4; i++)
            if (x[i] != y[i]) {
                log("    fake object %u +%x: %x / %x", i * 4 / OBJ, i * 4 % OBJ, x[i], y[i]);
                break;
            }
    }
    t.check(same, "ai", d);
    return same;
}

// ---- Running a slot ------------------------------------------------------------------------------------------

static WORD fpu_status()
{
    WORD sw;
    __asm__ volatile("fnstsw %0" : "=m"(sw));
    return sw;
}
// n: the number of stack arguments; | SLOT_FLOAT: the result is st(0) (its 80 bits hashed into eax);
// | SLOT_FASTCALL: ecx and edx are the first two arguments (no object)
static DWORD hash_ld(long double v)
{
    BYTE b[12] = {};
    memcpy(b, &v, 10);
    return fnv(2166136261u, b, 10);
}
DWORD call_slot(DWORD fn, void *self, int n, const DWORD *a)
{
    using F0 = DWORD(THISCALL *)(void *);
    using F1 = DWORD(THISCALL *)(void *, DWORD);
    using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
    using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
    using F4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
    using F5 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F6 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using L0 = long double(THISCALL *)(void *);
    using L1 = long double(THISCALL *)(void *, DWORD);
    using L2 = long double(THISCALL *)(void *, DWORD, DWORD);
    using L3 = long double(THISCALL *)(void *, DWORD, DWORD, DWORD);
    if (n & SLOT_FASTCALL)
        return at<DWORD(__fastcall *)(DWORD, DWORD)>(fn)(a[0], a[1]);
    if (n & SLOT_FLOAT) {
        switch (n & 0xff) {
        case 0: return hash_ld(at<L0>(fn)(self));
        case 1: return hash_ld(at<L1>(fn)(self, a[0]));
        case 2: return hash_ld(at<L2>(fn)(self, a[0], a[1]));
        case 3: return hash_ld(at<L3>(fn)(self, a[0], a[1], a[2]));
        }
        return 0;
    }
    switch (n) {
    case 0: return at<F0>(fn)(self);
    case 1: return at<F1>(fn)(self, a[0]);
    case 2: return at<F2>(fn)(self, a[0], a[1]);
    case 3: return at<F3>(fn)(self, a[0], a[1], a[2]);
    case 4: return at<F4>(fn)(self, a[0], a[1], a[2], a[3]);
    case 5: return at<F5>(fn)(self, a[0], a[1], a[2], a[3], a[4]);
    case 6: return at<F6>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5]);
    }
    return 0;
}

void (*g_after_side)();
bool g_trace;
static const SlotTest *g_last_test;
bool run_slot(Tally &t, const SlotTest &st, unsigned seed, SelfMaker make_self, const Policy &policy)
{
    if (&st != g_last_test) {   // one line per test (to find a crash)
        g_last_test = &st;
        log("    > %s", st.name);
    }
    Side s[2];
    for (int side = 0; side < 2; side++) {
        g_policy = policy;
        side_begin(side, seed);
        build_world(seed);
        Gen g{seed * 0x8088405u + st.address};
        make_self(g);
        DWORD a[8] = {};
        if (st.setup)
            st.setup(g, a);
        for (DWORD r : policy.real)
            for (int i = 0; r && i < NDIRECT; i++)
                if (DIRECT[i].id == r)
                    memcpy(g_patches[i].site, g_patches[i].saved, 5);
        globals_on();
        const WORD before = fpu_status();
        if (g_trace)
            log("      seed %u side %d: %x %x %x %x %x", seed, side, a[0], a[1], a[2], a[3], a[4]);
        if (side == 0)
            hooks_out();
        s[side].eax = call_slot(st.address, obj(O_SELF), st.nargs, a);
        if (side == 0)
            hooks_in();
        const WORD after = fpu_status();
        for (DWORD r : policy.real)
            for (int i = 0; r && i < NDIRECT; i++)
                if (DIRECT[i].id == r)
                    write_jump(g_patches[i], r, thunk(i));
        s[side].state = side_end() ^ ((DWORD)((before ^ after) & 0x3800) << 16);
        globals_off();
        if (g_after_side)
            g_after_side();
        s[side].logh = g_log[side].hash;
        s[side].calls = (DWORD)g_log[side].calls;
        s[side].trapped = g_trapped;
    }
    return compare(t, st.name, seed, s, st.mask);
}

// ---- The level's parts -------------------------------------------------------------------------------------


int level_parts(BYTE **out, int max)
{
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return -1;
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const DWORD *cells = *reinterpret_cast<DWORD **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return -1;
    int n = 0;
    for (DWORD i = 0; i < columns * rows; i++) {
        const DWORD *cell = cells + i * 4;   // a pointer array: vtable, data, count, capacity
        const DWORD count = cell[2];
        BYTE **data = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(cell[1]));
        if (count > 10000 || !readable(data, count * 4))
            continue;
        for (DWORD k = 0; k < count; k++) {
            BYTE *p = data[k];
            bool dup = false;
            for (int j = 0; j < n && !dup; j++)
                dup = out[j] == p;
            if (!dup && n < max && class_of(p))
                out[n++] = p;
        }
    }
    return n;
}

static void collect_nested(BYTE *o, int depth, BYTE *const *parts, int nparts, BYTE **out, int &n, int max)
{
    const CClassInfo *ci = class_of(o);
    if (!ci || n >= max)
        return;
    for (int j = 0; j < nparts; j++)
        if (parts[j] == o)
            return;
    for (int j = 0; j < n; j++)
        if (out[j] == o)
            return;
    out[n++] = o;
    const DWORD size = ci->size < 0x400 ? ci->size : 0x400;
    if (depth >= 3 || !readable(o, size))
        return;
    for (DWORD off = 4; off + 4 <= size; off += 4)
        collect_nested(*reinterpret_cast<BYTE **>(o + off), depth + 1, parts, nparts, out, n, max);
}

int part_ais(const BYTE *part, BYTE *const *parts, int nparts, BYTE **out, int max)
{
    if (!readable(part, 0x48))
        return 0;
    const DWORD count = *reinterpret_cast<const DWORD *>(part + 0x40);
    BYTE *const *ais = *reinterpret_cast<BYTE *const *const *>(part + 0x3c);
    if (count > 1000 || !readable(ais, count * 4))
        return 0;
    int n = 0;
    for (DWORD k = 0; k < count; k++)
        collect_nested(ais[k], 0, parts, nparts, out, n, max);
    return n;
}

}  // namespace zax::pw
