// The fake world of the AI slot checks (see checks_task_world.h).
#include "checks_task_world.h"
#include "class_info.h"

namespace zax {
int hooks_ai_tasks(Hook *out);
int hooks_ai_turrets(Hook *out);
}  // namespace zax

namespace zax::tw {

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
    if ((v & 15) == 0) {
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
    const char *args;   // p pointer, v value, b low byte, f two floats behind it, S a sweep's movement, s string,
                        // m a message object, o / x not recorded
    char ret;           // v void (a junk value), i int, b bool (al), f st(0), p a fake of `type`, n the same or 0, c custom
    int type;
    bool caller_pops;
    Custom custom;
};

// The fakes of each type: the first object and the count
static const int TYPE_FIRST[NTYPES] = {O_ENTITY, O_BEHAVIOR, O_INVENTORY, O_ITEM, O_DEF, O_WORLD, O_APP, O_REPORTER, O_GAMETYPE,
                                       O_VIEW, O_PALOBJ, O_AI, 0, 0, O_OTHER, O_LAYER, O_ACTION, O_SEQUENCE};
static const int TYPE_COUNT[NTYPES] = {N_ENTITY, N_BEHAVIOR, N_INVENTORY, N_ITEM, N_DEF, 1, 1, 1, 1, 1, 1, N_AI, 0, 0, N_OTHER, 1, N_ACTION, 1};
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
static const char *const SEQUENCES[] = {"Death", "Shoot1", "Idle", "idle", "Deploy", "SHOOT1"};
static const char *const NAMES[] = {"Map01.zax", "Sub/Map 2.zax", "x", ""};

// An extra entry of the side's log (what a custom stand-in sees beyond its arguments)
static void extra(DWORD id, DWORD a, DWORD b)
{
    DWORD e[9] = {id, a, b, 0, 0, 0, 0, 0, 0};
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

static DWORD c_pos(Call &c) { return write_pos(c.a[0], 0.0f, 4000.0f); }
static DWORD c_find(Call &)
{
    const DWORD v = nv();
    if (g_policy.find_minus_one && v % (DWORD)g_policy.find_minus_one == 0)
        return 0xffffffffu;
    return (v >> 8) % 4;
}
static DWORD c_random_below(Call &c)
{
    const DWORD n = c.a[0];
    return nv() % (n > 8 ? n : 8);
}
static DWORD c_random_range(Call &c)
{
    const DWORD v = nv();
    if ((v & 7) == 0)
        c.fret = frange(-10.0f, 10.0f);
    else
        c.fret = (long double)fbits(c.a[0]) + ((long double)fbits(c.a[1]) - fbits(c.a[0])) * (long double)(int)(v >> 8) * (1.0L / 16777216.0L);
    c.fset = true;
    return nv();
}
static DWORD c_push(Call &c) { return (nv() & 7) == 0 ? 0 : c.a[1]; }
static DWORD c_append(Call &c) { return c.a[0]; }
static DWORD c_sequence(Call &) { return ptrv(SEQUENCES[nv() % 6]); }
static DWORD c_name(Call &) { return ptrv(NAMES[nv() % 4]); }
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}
// 0x571b70: the 0x30-byte behaviour's constructor: the block cleared, returned
static DWORD c_ctor_571b70(Call &c)
{
    if (c.ecx)
        memset(at<void *>(c.ecx), 0, 0x30);
    return c.ecx;
}
// 0x5aabe0 (cl, the 0x30-byte behaviour; three more): what it got at +0x2c logged, the block freed
// (it takes it over), a fake behaviour back
static DWORD c_warp(Call &c)
{
    if (c.edx) {
        extra(0x5aabe1, *at<DWORD *>(c.edx + 0x2c), 0);
        pool_free(at<void *>(c.edx), 0x30);
    }
    return ptrv(pick(T_BEHAVIOR, false));
}
// 0x59a470 (the slot in ecx): a fake list {vtable, data +4, count +8} of 0-4 fake entities
static DWORD c_temp_list(Call &c)
{
    BYTE *l = obj(O_TEMPLIST);
    const DWORD n = nv() % 5;
    fl<DWORD>(l, 4) = ptrv(l + 0x20);
    fl<DWORD>(l, 8) = n;
    for (DWORD i = 0; i < n; i++)
        fl<DWORD>(l, 0x20 + 4 * i) = ptrv(pick(T_ENTITY, false));
    if (c.ecx)
        *at<DWORD *>(c.ecx) = ptrv(l);
    return c.ecx;
}
// 0x4fba60 (CLayerPart_AIAt): this AI, a fake AI or 0
static DWORD c_ai_at(Call &)
{
    const DWORD v = nv() % 3;
    return v == 0 ? ptrv(obj(O_SELF)) : v == 1 ? ptrv(pick(T_AI, false)) : 0;
}
static DWORD c_dir_count(Call &) { return nv() % 4; }
static DWORD c_sequence_object(Call &) { return ptrv(obj(O_SEQUENCE)); }

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    S(0x41a720, "p-", "pppppp", 'b', 0),                     // an action's slot 23
    S(0x41a740, "p-", "ppppp", 'b', 0),                      // an action's slot 23 from a caller
    S(0x4fd0b0, "p-", "vv", 'v', 0),                         // an AI's debug drawing
    S(0x4e72a0, "p-", "pvv", 'v', 0),                        // CLayerSaveData::MonitorAdd
    S(0x4e73c0, "p-", "p", 'v', 0),                          // CLayerSaveData::MonitorRemove
    S(0x506620, "p-", "s", 'b', 0),                          // the message of a name
    SC(0x51f450, "p-", "", c_dir_count),                     // NetGame_PlayerCount (0-3)
    S(0x51f440, "p-", "v", 'p', T_OTHER),                    // NetGame_PlayerAt
    S(0x416760, "p-", "vv", 'p', T_ENTITY),                  // Object_Clone
    S(0x41ad40, "pp", "pppvvv", 'n', T_ENTITY),              // the part a name refers to
    S(0x4eb1c0, "p-", "ppvv", 'n', T_ENTITY),                // CLayer::FindBlockerHere
    S(0x4f5d80, "p-", "", 'p', T_ENTITY),                    // World::FirstCharacter
    SC(0x55c470, "p-", "v", c_random_below),                 // Random_Below
    SC(0x55c550, "p-", "vv", c_random_range),                // a random float
    S(0x4fbf10, "p-", "p", 'v', 0),                          // CLayerPart_SetName
    S(0x4fbd00, "p-", "", 'v', 0),                           // CLayerPart_DeleteAIs
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    S(0x4fb5e0, "p-", "f", 'v', 0),                          // CLayerPart_MoveCenterTo
    S(0x4eb6f0, "p-", "p", 'v', 0),                          // CLayer::AddPart
    S(0x4fbc50, "p-", "p", 'v', 0),                          // CLayerPart_AddAI
    SC(0x571b70, "--", "", c_ctor_571b70),                   // a 0x30-byte behaviour's constructor
    SC(0x523df0, "p-", "sv", c_find),                        // ResourceList_Find
    {0x5aabe0, "b-", "vvv", 'c', 0, false, c_warp},          // the warp behaviour
    {0x5d0863, "--", "v", 'i', 0, true, nullptr},            // atexit
    S(0x48d030, "p-", "", 'f', 0),                           // EntityDestructable_HitPointsLeft
    SC(0x59a470, "--", "", c_temp_list),                     // PartList_GetTemp
    S(0x59a510, "--", "", 'i', 0),                           // PartList_ReleaseTemp
    S(0x57aa80, "p-", "psvvv", 'v', 0),                      // CSortList2D_FindName
    S(0x4fd100, "p-", "pm", 'i', 0),                         // CLayerPartAI's slot 29
    S(0x4303f0, "p-", "pfvv", 'b', 0),                       // CEntityBehaviorMoveToPos's aim
    SC(0x48b590, "p-", "", c_sequence_object),               // Entity_SequenceObject
    S(0x560300, "p-", "vvp", 'v', 0),                        // the sequence's motion
    S(0x48feb0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrame
    S(0x48f700, "p-", "s", 'b', 0),                          // EntityAnimated_PlaySequence
    S(0x417500, "p-", "ssv", 'i', 0),                        // CObject_ReadOwnLine
    S(0x4f4ca0, "p-", "s", 'b', 0),                          // World::LevelExists
    S(0x4f4d70, "p-", "s", 'n', T_LAYER),                    // World::FindLayer
    S(0x4f1050, "p-", "", 'n', T_LAYER),                     // World::CurrentLayer
    S(0x5a0860, "p-", "s", 'v', 0),                          // the application: flush a map
    S(0x5a0880, "p-", "s", 'v', 0),                          // the application: expire a map
    S(0x41f780, "p-", "s", 'i', 0),                          // COtherMapList::CountFor
    S(0x41f7d0, "p-", "s", 'v', 0),                          // COtherMapList::RemoveFor
    SC(0x4a2ea0, "--", "-vvv", c_dir_count),                 // DirList_Read
    SC(0x4a33f0, "--", "v", c_name),                         // DirList_Name
    S(0x4fba80, "p-", "v", 'n', T_AI),                       // CLayerPart_TakeAI
    S(0x4fbaa0, "p-", "vp", 'i', 0),                         // CLayerPart_ReplaceAI
    SC(0x4fba60, "p-", "v", c_ai_at),                        // CLayerPart_AIAt
    S(0x4fc390, "p-", "vvvvvv", 'n', T_ENTITY),              // CLayerPart_FindNearest
    S(0x4912b0, "p-", "fvvv", 'b', 0),                       // EntityMovable_PointFree
    SC(0x49a560, "p-", "ppv", c_push),                       // CEntityBehaviorStack::Push
    SC(0x49a620, "p-", "p", c_append),                       // CEntityBehaviorStack::Append
    S(0x497f30, "vv", "v", 'n', T_BEHAVIOR),                 // EntityBehaviorWait_Make
    S(0x4994d0, "vv", "v", 'n', T_BEHAVIOR),                 // EntityBehaviorSetCurFrame_Make
    S(0x5ab300, "vv", "v", 'n', T_BEHAVIOR),                 // a fire behaviour
    S(0x495ec0, "pp", "vvvvv", 'n', T_BEHAVIOR),             // EntityBehaviorTurnToTarget_Make
    S(0x496e20, "pz", "vzvvv", 'n', T_BEHAVIOR),             // EntityBehaviorPlaySequenceOnce_Make
    S(0x4953b0, "pv", "vv", 'n', T_BEHAVIOR),                // EntityBehaviorTurnToAngle_Make
    SC(0x48aee0, "p-", "", c_sequence),                      // Entity_SequenceName
    S(0x54d630, "p-", "p", 'i', 0),                          // IntField_Get
    S(0x54d660, "p-", "pv", 'i', 0),                         // IntField_Set
    S(0x4164c0, "zv", "vp", 'n', T_OTHER),                   // Object_CreateFromFile
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 160;
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
    V(T_ENTITY, 24, "p", 'v', 0),   // a clone (Object_Clone's answer) begun as an AI
    V(T_ENTITY, 33, "", 'v', 0),
    V(T_ENTITY, 36, "", 'p', T_OTHER),
    V(T_ENTITY, 39, "", 'v', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 63, "m", 'b', 0),
    V(T_ENTITY, 66, "pv", 'b', 0),
    V(T_ENTITY, 69, "p", 'v', 0),
    V(T_ENTITY, 70, "", 'n', T_ENTITY),
    V(T_ENTITY, 71, "p", 'v', 0),
    V(T_ENTITY, 84, "", 'f', 0),
    V(T_ENTITY, 103, "vv", 'v', 0),
    V(T_ENTITY, 104, "vv", 'v', 0),
    V(T_DEF, 23, "pp", 'n', T_ENTITY),
    V(T_BEHAVIOR, 1, "v", 'i', 0),
    V(T_BEHAVIOR, 23, "p", 'b', 0),
    V(T_BEHAVIOR, 25, "vp", 'b', 0),
    V(T_BEHAVIOR, 28, "vv", 'i', 0),
    V(T_AI, 1, "v", 'i', 0),
    V(T_AI, 24, "p", 'v', 0),
    V(T_AI, 25, "pv", 'v', 0),
    V(T_AI, 29, "pm", 'b', 0),
    V(T_AI, 31, "ppv", 'b', 0),
    V(T_AI, 33, "ppv", 'b', 0),
    V(T_AI, 34, "ppvvv", 'v', 0),
    V(T_AI, 43, "vv", 'v', 0),
    V(T_ACTION, 1, "v", 'i', 0),
    V(T_ACTION, 25, "vv", 'i', 0),
    V(T_WORLD, 1, "", 'p', T_OTHER),
    V(T_WORLD, 17, "s", 'v', 0),
    V(T_APP, 14, "", 'i', 0),
    V(T_APP, 97, "", 'p', T_OTHER),
    V(T_REPORTER, 17, "s", 'v', 0),
    V(T_OTHER, 1, "v", 'i', 0),
    V(T_OTHER, 2, "", 'v', 0),
    V(T_OTHER, 7, "v", 'i', 0),
    VC(T_CATALOGUE, 1, "v", c_name),     // the definition lists' slots: the name of entry i
    VC(T_CATALOGUE, 5, "", c_name),      // the folder
    V(T_CATALOGUE, 37, "ssp", 'n', T_OTHER),
    V(T_CATALOGUE, 39, "vp", 'i', 0),
    V(T_SEQUENCE, 26, "", 'f', 0),
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
extern char tw_thunks[];
DWORD tw_pop, tw_ret_float, tw_edx;
long double tw_fret;
DWORD __cdecl tw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _tw_thunks\n"
        "_tw_thunks:\n"
        ".set tw_i, 0\n"
        ".rept 4768\n"
        "  movl $tw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _tw_common - . - 4\n"
        ".set tw_i, tw_i + 1\n"
        ".endr\n"
        "_tw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _tw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _tw_ret_float\n"
        "  je 1f\n"
        "  fldt _tw_fret\n"
        "1:\n"
        "  movl _tw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _tw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(tw_thunks) + 10u * (DWORD)i; }

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
        case 'z': return v && readable(at<void *>(v), 4) && *at<DWORD *>(v) ? (DWORD)text_hash(*at<DWORD *>(v) + 8) : 0;
        case 'f': return dword_at(v, 0) * 31u + dword_at(v, 4);
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

DWORD __cdecl tw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            tw_pop = 4;
            tw_ret_float = 0;
            tw_edx = 0;
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
    tw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    tw_ret_float = c.fset ? 1 : 0;
    tw_fret = c.fret;
    tw_edx = nv();
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
    for (int k = 0; k < 4768; k += 997) {
        const BYTE *th = reinterpret_cast<const BYTE *>(tw_thunks) + 10 * k;
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
constexpr int MAX_SITES = 256;
static SavedSite g_sites[MAX_SITES];
static Hook g_hooks[MAX_SITES];
static int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    int (*const tables[])(Hook *) = {hooks_ai_tasks, hooks_ai_turrets};
    const char *const names[] = {"ai_tasks", "ai_turrets"};
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
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x4cb000), 0xc5000);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x4cb000), 0xc5000);
    g_nsites = 0;
}

// ---- The world's globals -----------------------------------------------------------------------------------

struct Global {
    DWORD address, size;
    bool zstring;   // a function-local static ZString: compared by its text, a new one made by a side freed
};
static const Global GLOBALS[] = {
    {0x6c2080, 4, false}, {0x713f14, 4, false}, {0x6c02cc, 4, false},
    {0x6c043d, 1, false}, {0x6c0438, 4, true},   // CGeneratorAI's "$Instigator"
    {0x713a1e, 1, false}, {0x713a10, 4, true}, {0x713a18, 4, true}, {0x713a20, 4, true}, {0x713a24, 4, true},   // CTurretAI's sequences
};
constexpr int NGLOBALS = sizeof GLOBALS / sizeof GLOBALS[0];
static BYTE g_saved_globals[NGLOBALS][16];
static DWORD g_case_globals[NGLOBALS][4];   // the values of the case (set by build_world)
static DWORD g_valid_text[NGLOBALS];         // valid ZStrings for the statics whose guard bit a case sets
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
    for (int i = 0; i < NGLOBALS; i++) {
        const DWORD v = *at<DWORD *>(GLOBALS[i].address);
        if (GLOBALS[i].zstring) {
            const unsigned t = v ? text_hash(v + 8) : 0;
            h = fnv(h, &t, 4);
        } else {
            h = fnv(h, at<void *>(GLOBALS[i].address), GLOBALS[i].size);
        }
    }
    return h;
}
void globals_off()
{
    for (int i = NGLOBALS; i--;) {
        if (GLOBALS[i].zstring && *at<DWORD *>(GLOBALS[i].address) != g_case_globals[i][0])
            at<void(THISCALL *)(void *)>(0x4defd0)(at<void *>(GLOBALS[i].address));   // made by the side
        memcpy(at<void *>(GLOBALS[i].address), g_saved_globals[i], GLOBALS[i].size);
    }
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
    for (int i = 0; i < N_ACTION; i++)
        vt(O_ACTION + i, T_ACTION);
    for (int i = 0; i < N_OTHER; i++) {
        vt(O_OTHER + i, T_OTHER);
        fl<DWORD>(obj(O_OTHER + i), 0x30) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
    }
    vt(O_SEQUENCE, T_SEQUENCE);
    fl<DWORD>(obj(O_SEQUENCE), 0x48) = g.next() % 8 ? 1 + g.next() % 40 : 0;
    fl<DWORD>(obj(O_APP), 0x354) = ptrv(obj(O_OTHERMAPS));
    for (int i = 0; i < N_ENTITY; i++)
        fl<DWORD>(obj(O_ENTITY + i), 0x40) = g.next() % 3;
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    g_case_globals[0][0] = ptrv(obj(O_WORLD));
    g_case_globals[1][0] = ptrv(obj(O_APP));
    g_case_globals[2][0] = ptrv(obj(O_REPORTER));
    g_case_globals[3][0] = g.next() % 2;
    g_case_globals[5][0] = g.next() % 16;
    const DWORD bits[NGLOBALS] = {0, 0, 0, 0, 1, 0, 1, 8, 2, 4};
    for (int i = 0; i < NGLOBALS; i++)
        if (GLOBALS[i].zstring)
            g_case_globals[i][0] = g_case_globals[i == 4 ? 3 : 5][0] & bits[i] ? g_valid_text[i] : 0;
}

// The valid ZStrings the statics hold when their guard bit is set (made once, kept)
void make_static_texts()
{
    const char *const texts[NGLOBALS] = {nullptr, nullptr, nullptr, nullptr, "$Instigator", nullptr, "Deploy", "Idle", "Shoot1", "Deploy"};
    for (int i = 0; i < NGLOBALS; i++)
        if (GLOBALS[i].zstring && !g_valid_text[i])
            at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_valid_text[i], texts[i]);
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
DWORD call_slot(DWORD fn, void *self, int n, const DWORD *a)
{
    using F0 = DWORD(THISCALL *)(void *);
    using F1 = DWORD(THISCALL *)(void *, DWORD);
    using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
    using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
    using F4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
    using F5 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F6 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    switch (n) {
    case -1: return at<DWORD(__fastcall *)(void *, DWORD)>(fn)(self, a[0]);   // fastcall: the object, edx
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
void (*g_before_state)();
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
        if (side == 0)
            hooks_out();
        s[side].eax = call_slot(st.address, obj(O_SELF), st.nargs, a);
        if (side == 0)
            hooks_in();
        const WORD after = fpu_status();
        if (g_before_state)
            g_before_state();
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

}  // namespace zax::tw
