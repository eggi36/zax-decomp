// The fake world of the AI slot checks (see checks_weapon_world.h).
#include "checks_weapon_world.h"
#include "class_info.h"

namespace zax {
int hooks_weapons(Hook *out);
int hooks_deployables(Hook *out);
int hooks_death_match(Hook *out);
}  // namespace zax

namespace zax::ww {

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
bool g_trace;
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
                        // m a message object, c a 5-dword context, o / x not recorded
    char ret;           // v void (a junk value), i int, b bool (al), f st(0), p a fake of `type`, n the same or 0, c custom
    int type;
    bool caller_pops;
    Custom custom;
};

// The fakes of each type: the first object and the count
static const int TYPE_FIRST[NTYPES] = {O_ENTITY, O_BEHAVIOR, O_INVENTORY, O_ITEM, O_DEF, O_WORLD, O_APP, O_REPORTER, O_GAMETYPE,
                                       O_VIEW, O_PALOBJ, O_AI, 0, 0, O_OTHER, O_LAYER, O_ACTION, O_SEQUENCE, O_FRAME};
static const int TYPE_COUNT[NTYPES] = {N_ENTITY, N_BEHAVIOR, N_INVENTORY, N_ITEM, N_DEF, 1, 1, 1, 1, 1, 1, N_AI, 0, 0, N_OTHER, 1, N_ACTION, 1, 1};
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
static DWORD c_name(Call &) { return ptrv(NAMES[nv() % 4]); }
// The same answer for the same object and class within a case (the slots ask twice and read address 0
// when the answers differ), the upper bytes from the case's sequence
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    DWORD h = g_vseed ^ (c.ecx * 0x9e3779b9u) ^ (c.a[0] * 0x85ebca6bu);
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return (v & 0xffffff00u) | ((h >> 7) & 1);
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
static DWORD c_sequence_object(Call &) { return ptrv(obj(O_SEQUENCE)); }
// 0x508fe0 (ecx, edx: the outputs; the angle): two values in [-1, 1] (or specials)
static DWORD c_sin_cos(Call &c)
{
    if (c.ecx && readable(at<void *>(c.ecx), 4))
        *at<float *>(c.ecx) = frange(-1.0f, 1.0f);
    if (c.edx && readable(at<void *>(c.edx), 4))
        *at<float *>(c.edx) = frange(-1.0f, 1.0f);
    return nv();
}
// A sweep's hit point (+0x14, +0x18 of the 0x24-byte result)
static void write_hit(DWORD hit)
{
    if (hit && readable(at<void *>(hit), 0x24)) {
        at<float *>(hit)[5] = frange(-500.0f, 4000.0f);
        at<float *>(hit)[6] = frange(-500.0f, 4000.0f);
    }
}
// 0x547230 Polygon_SweepPoint (offset, point, sweep, hit): al, the hit point written when it hits
static DWORD c_poly_sweep_point(Call &c)
{
    const DWORD v = nv();
    if ((v >> 7) & 1)
        write_hit(c.a[3]);
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}
// 0x4e8d40 CLayer_SweepPoint (point, sweep, hit, out part, ...): al; a hit writes the hit point and a
// fake entity into the out part (when given)
static DWORD c_layer_sweep_point(Call &c)
{
    const DWORD v = nv();
    if ((v >> 7) & 1) {
        write_hit(c.a[2]);
        if (c.a[3] && readable(at<void *>(c.a[3]), 4))
            *at<DWORD *>(c.a[3]) = ptrv(pick(T_ENTITY, false));
    }
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}
// 0x55e090 (the catalogue's record i loaded): the record's object becomes a fake definition (or stays 0)
static DWORD c_load_record(Call &c)
{
    BYTE *records = *at<BYTE **>(0x6c0c38);
    const DWORD i = c.a[0];
    if (records && i < 4)
        fl<DWORD>(records + i * 0x80, 0x78) = ptrv(pick(T_DEF, true));
    return nv();
}
// 0x53e110 CPickupAI::Collect (part, other, a3, a4, a5, out item): a fake item or 0 into out
static DWORD c_collect(Call &c)
{
    if (c.a[5] && readable(at<void *>(c.a[5]), 4))
        *at<DWORD *>(c.a[5]) = ptrv(pick(T_ITEM, true));
    return nv();
}
// 0x42f5e0 (a 0x34-byte AI on a pool block): the block cleared with the fake AI vtable; freed after the side
BYTE *g_made_ai[8];
int g_num_made_ai;   // freed by the check after each side (g_after_side)
static DWORD c_ai_ctor(Call &c)
{
    if (c.ecx) {
        memset(at<void *>(c.ecx), 0, 0x34);
        *at<DWORD *>(c.ecx) = ptrv(g_fvt[T_AI]);
        if (g_num_made_ai < 8)
            g_made_ai[g_num_made_ai++] = at<BYTE *>(c.ecx);
    }
    return c.ecx;
}
// 0x57ac50 CSortList2D_QueryRadius (callback, context, point, except, radius, ...): the context's four
// words logged, the callback (one of this module's, 0x476b20 or 0x477510) for 0-2 fake entities with a
// squared distance
// 0x4c3c20 CPainAction::CtorValues (hit points, target, damage type) on the stack: its target ZString
// (+0x10) made empty, so the caller's destructor finds a valid one
static DWORD c_pain_ctor(Call &c)
{
    if (c.ecx)
        at<void *(THISCALL *)(void *)>(0x4dec90)(at<void *>(c.ecx + 0x10));
    return c.ecx;
}
static DWORD c_query_radius(Call &c)
{
    const DWORD ctx = c.a[1];
    const DWORD cb = c.a[0];
    if (ctx && readable(at<void *>(ctx), 0x14)) {   // 0x476b20's context has two words, 0x477510's five
        const DWORD *w = at<DWORD *>(ctx);
        extra(0x57ac51, w[0], w[1]);
        if (cb == 0x477510)
            extra(0x57ac52, w[2], w[3] * 31u + w[4]);
    }
    if (cb == 0x476b20 || cb == 0x477510) {
        const DWORD n = nv() % 3;
        for (DWORD i = 0; i < n; i++) {
            BYTE *e = pick(T_ENTITY, false);
            at<void(__fastcall *)(void *, DWORD, float)>(cb)(e, ctx, frange(0.0f, 250000.0f));
        }
    }
    return nv();
}

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    S(0x41a740, "p-", "ppvvp", 'b', 0),                      // CAction::ExecuteFrom
    S(0x416760, "p-", "vv", 'p', T_OTHER),                   // Object_Clone
    S(0x4fc6f0, "p-", "", 'n', T_ENTITY),                    // CLayerPart_Root
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    S(0x4267e0, "p-", "", 'n', T_INVENTORY),                 // LayerPart_GetInventory
    S(0x4dd480, "p-", "", 'p', T_DEF),                       // ItemRef_Definition (0x42a330 jumps here)
    S(0x48d8f0, "p-", "v", 'n', T_DEF),                      // DefinitionList_Object
    S(0x5b7920, "p-", "p", 'n', T_OTHER),                    // CZaxInventoryItemDefinition::EnergySource
    S(0x5b7670, "p-", "", 'n', T_ACTION),                    // ... ContinuousFire8 (the start action)
    S(0x5b76a0, "p-", "", 'n', T_ACTION),                    // ... ContinuousFireC (the finish action)
    S(0x5b77a0, "p-", "", 'i', 0),                           // ... ShotsPerSecond (the impact explosion)
    S(0x5b78f0, "p-", "", 'f', 0),                           // ... ContinuousFireRate (the spread)
    S(0x48ec20, "p-", "", 'b', 0),                           // EntityAnimated_FrameIndexLocked
    S(0x48ff90, "p-", "v", 'v', 0),                          // EntityAnimated_LockFrame
    S(0x48ffb0, "p-", "", 'v', 0),                           // EntityAnimated_UnlockFrame
    S(0x48ebc0, "p-", "v", 'i', 0),                          // EntityAnimated_SetWaitingForDischarge
    S(0x4fb910, "p-", "mvvvvv", 'i', 0),                     // CLayerPart_FindInRect (the discharged message)
    S(0x4f6530, "p-", "pvvvvvvvp", 'n', T_ENTITY),           // World::SpawnExplosionEntity
    S(0x4ebcb0, "p-", "p", 'v', 0),                          // CLayer::RemovePart
    S(0x4fc710, "p-", "p", 'v', 0),                          // CLayerPart_AddChild
    S(0x42a370, "p-", "v", 'i', 0),                          // CInventoryItem::Select
    S(0x42a380, "p-", "v", 'i', 0),                          // CInventoryItem::Deselect
    S(0x42a720, "p-", "vm", 'b', 0),                         // CInventoryItem::Message
    S(0x4eaa60, "p-", "vpvvb", 'b', 0),                      // CLayer::SendSoundAt
    S(0x4eaab0, "p-", "vfvvb", 'b', 0),                      // CLayer::SendSound
    S(0x4f1050, "p-", "", 'n', T_LAYER),                     // World::CurrentLayer
    S(0x4fc1b0, "p-", "pvs", 'v', 0),                        // LayerPart_AddListener
    S(0x4fc200, "p-", "pvs", 'v', 0),                        // LayerPart_RemoveListener
    S(0x4fd190, "p-", "pvs", 'v', 0),                        // AIBase_AddListener
    S(0x4fd1e0, "p-", "pvs", 'v', 0),                        // AIBase_RemoveListener
    S(0x4fbf30, "p-", "", 'i', 0),                           // LayerPart_Died
    S(0x4fbc50, "p-", "p", 'v', 0),                          // CLayerPart_AddAI
    S(0x5176f0, "p-", "vppvv", 'n', T_ENTITY),               // Entity_CreateFromModel
    S(0x4eb7b0, "p-", "ppv", 'v', 0),                        // Entity_PlaceNear
    S(0x48d150, "p-", "", 'f', 0),                           // EntityDestructable_HitPoints
    S(0x48d030, "p-", "", 'f', 0),                           // EntityDestructable_HitPointsLeft
    SC(0x523df0, "p-", "sv", c_find),                        // ResourceList_Find
    SC(0x4262a0, "p-", "p", c_find),                         // CInventory_IndexOf
    S(0x425900, "p-", "p", 'i', 0),                          // CInventory_IndexOfDefinition
    S(0x424f60, "p-", "v", 'n', T_ITEM),                     // CInventory::ItemAt
    SC(0x425740, "p-", "p", c_find),                         // CInventory_IndexOfSameDefinition
    S(0x54fdb0, "p-", "v", 'v', 0),                          // the item's definition index set
    S(0x42a390, "p-", "p", 'b', 0),                          // CInventoryItem::SameKind
    S(0x42a500, "p-", "p", 'b', 0),                          // CInventoryItem::CanAdd
    S(0x42a550, "p-", "vp", 'n', T_ITEM),                    // CInventoryItem::Take
    S(0x5afe50, "p-", "", 'f', 0),                           // CEnergyItem::MaxCharge
    S(0x5afe90, "p-", "v", 'v', 0),                          // CEnergyItem::Add
    S(0x48b290, "p-", "", 'n', T_OTHER),                     // Entity_Model
    S(0x4d61f0, "p-", "", 'n', T_OTHER),                     // CInventoryItemDefinition::Projectile
    S(0x4ea220, "p-", "v", 'v', 0),                          // CLayer::Update
    SC(0x53e110, "p-", "pvvvvo", c_collect),                 // CPickupAI::Collect
    S(0x5bac80, "p-", "", 'n', T_OTHER),                     // ZaxNet_PlayerOf
    S(0x5ba420, "p-", "v", 'v', 0),                          // ZaxNetPlayer::SetScore
    S(0x51c650, "p-", "", 'v', 0),                           // CMultiPlayerGameType::EndMatch
    S(0x51c310, "p-", "v", 'i', 0),                          // CMultiPlayerGameType::Update
    S(0x586f80, "p-", "p", 'v', 0),                          // PtrArray_Add
    SC(0x42f5e0, "p-", "", c_ai_ctor),                       // the 0x34-byte AI's constructor
    S(0x4fd100, "p-", "pm", 'i', 0),                         // CLayerPartAI's slot 29
    S(0x46cd50, "p-", "ps", 'i', 0),                         // CObject_OnMessage
    S(0x506680, "s-", "", 'i', 0),                           // the message id of a name
    S(0x508c50, "--", "v", 'f', 0),                          // the table square root
    {0x508fe0, "oo", "v", 'c', 0, false, c_sin_cos},         // the table sine and cosine
    SC(0x55c650, "p-", "vv", c_random_range),                // a random float (0x55c650)
    SC(0x55c5b0, "p-", "vv", c_random_range),                // a random float (0x55c5b0)
    S(0x545cc0, "p-", "", 'i', 0),                           // Polygon_Ctor
    S(0x545cf0, "p-", "", 'i', 0),                           // Polygon_Dtor
    S(0x545e30, "p-", "", 'v', 0),                           // Polygon_Clear
    S(0x5466c0, "p-", "vvv", 'i', 0),                        // Polygon_Insert
    S(0x546810, "p-", "", 'i', 0),                           // Polygon_Close
    S(0x546ad0, "p-", "fp", 'p', T_OTHER),                   // Polygon_Rect
    S(0x5469c0, "p-", "vvv", 'b', 0),                        // Polygon_HitsCircle
    S(0x546a90, "p-", "", 'f', 0),                           // Polygon_Size
    SC(0x547230, "p-", "ffpp", c_poly_sweep_point),          // Polygon_SweepPoint
    S(0x545a20, "p-", "ffvv", 'i', 0),                       // CSweep_CtorFromTo
    S(0x545ab0, "p-", "ff", 'b', 0),                         // CSweep_SetFromTo
    S(0x545c90, "p-", "", 'i', 0),                           // SweepHit_Init
    SC(0x4e8d40, "p-", "fppovvvv", c_layer_sweep_point),     // CLayer_SweepPoint
    S(0x4ed5e0, "p-", "ppvvvv", 'i', 0),                     // CLayer::FindRect
    SC(0x59a470, "--", "", c_temp_list),                     // PartList_GetTemp
    S(0x59a510, "--", "", 'i', 0),                           // PartList_ReleaseTemp
    S(0x59a120, "p-", "", 'i', 0),                           // PartList_Clear
    S(0x57aeb0, "p-", "p", 'i', 0),                          // SortList_NewStamp
    S(0x4920b0, "p-", "", 'i', 0),                           // CEntityProjectile's constructor (the static)
    {0x5d0863, "--", "v", 'i', 0, true, nullptr},            // atexit
    S(0x491380, "p-", "pvvvv", 'b', 0),                      // EntityMovable_CanSee
    S(0x48b6b0, "p-", "v", 'i', 0),                          // Entity_SetRenderHeight
    S(0x59ff80, "p-", "", 'n', T_GAMETYPE),                  // the application's game type
    S(0x552940, "p-", "v", 'n', T_OTHER),                    // CQuestGameType::Stat
    S(0x5525b0, "--", "", 'p', T_OTHER),                     // Quest_SpecificData
    S(0x55ab90, "p-", "vv", 'v', 0),                         // CQuestPercentageStat::AddDenominator
    S(0x55aad0, "p-", "vv", 'v', 0),                         // CQuestPercentageStat::AddNumerator
    SC(0x55e090, "p-", "v", c_load_record),                  // a definition list's record loaded
    SC(0x48b590, "p-", "", c_sequence_object),               // Entity_SequenceObject
    SC(0x57ac50, "p-", "vcfvvvvvvvvv", c_query_radius),      // CSortList2D_QueryRadius
    S(0x41c7a0, "p-", "vppvvp", 'i', 0),                     // CActionQueue::Add
    SC(0x4c3c20, "p-", "vsv", c_pain_ctor),                  // CPainAction::CtorValues
    S(0x5240b0, "p-", "", 'i', 0),                           // ObjectStream_Ctor
    S(0x5241e0, "p-", "", 'i', 0),                           // ObjectStream_Dtor
    S(0x5242b0, "p-", "svv", 'b', 0),                        // ObjectStream_OpenFile
    S(0x524340, "p-", "", 'i', 0),                           // ObjectStream_Finish
    S(0x416af0, "p-", "p", 'i', 0),                          // Object_ReadFromStream
    S(0x4168f0, "p-", "p", 'i', 0),                          // Object_WriteText
    S(0x4173f0, "p-", "", 'i', 0),                           // Object_FreeFields (the destructor levels on fakes)
    S(0x4fd080, "p-", "", 'i', 0),                           // AIBase_Dtor
    S(0x42a300, "p-", "", 'i', 0),                           // CInventoryItem's destructor
    S(0x5afe30, "p-", "", 'i', 0),                           // CEnergyItem's destructor
    S(0x41a700, "p-", "", 'i', 0),                           // CAction's destructor
    S(0x51bff0, "p-", "", 'i', 0),                           // CMultiPlayerGameType's destructor
    S(0x53d940, "p-", "", 'i', 0),                           // CPickupAI's destructor
    S(0x4166d0, "p-", "", 'i', 0),                           // CObject's destructor
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
    V(T_ENTITY, 33, "", 'v', 0),
    V(T_ENTITY, 41, "vv", 'v', 0),
    VC(T_ENTITY, 43, "o", c_pos),
    V(T_ENTITY, 48, "", 'p', T_OTHER),
    V(T_ENTITY, 54, "", 'i', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 63, "m", 'b', 0),
    V(T_ENTITY, 65, "pv", 'b', 0),
    V(T_ENTITY, 68, "", 'n', T_ENTITY),
    V(T_ENTITY, 70, "", 'n', T_OTHER),
    V(T_ENTITY, 85, "", 'i', 0),
    V(T_ENTITY, 99, "vvp", 'v', 0),
    V(T_ITEM, 1, "v", 'i', 0),
    V(T_ITEM, 7, "", 'i', 0),
    V(T_ITEM, 33, "vp", 'n', T_ITEM),
    V(T_ITEM, 49, "v", 'v', 0),
    V(T_ITEM, 50, "p", 'i', 0),
    V(T_DEF, 1, "v", 'i', 0),
    V(T_DEF, 7, "", 'i', 0),
    V(T_DEF, 23, "v", 'f', 0),
    V(T_GAMETYPE, 27, "", 'i', 0),
    V(T_GAMETYPE, 48, "pp", 'v', 0),
    V(T_OTHER, 1, "v", 'i', 0),
    V(T_OTHER, 7, "", 'i', 0),
    V(T_OTHER, 19, "", 'i', 0),
    V(T_OTHER, 24, "", 'i', 0),
    V(T_OTHER, 33, "vp", 'n', T_ITEM),
    V(T_OTHER, 41, "vv", 'v', 0),
    V(T_OTHER, 49, "v", 'v', 0),
    V(T_OTHER, 50, "v", 'b', 0),
    V(T_OTHER, 58, "v", 'v', 0),
    V(T_OTHER, 59, "v", 'v', 0),
    V(T_OTHER, 83, "v", 'f', 0),
    V(T_OTHER, 84, "v", 'f', 0),
    V(T_OTHER, 85, "v", 'f', 0),
    V(T_OTHER, 86, "v", 'f', 0),
    V(T_INVENTORY, 24, "pv", 'v', 0),
    V(T_INVENTORY, 26, "v", 'p', T_OTHER),
    V(T_WORLD, 1, "", 'p', T_OTHER),
    V(T_WORLD, 58, "", 'p', T_OTHER),
    V(T_REPORTER, 17, "s", 'v', 0),
    V(T_VIEW, 7, "v", 'i', 0),           // a definition list's monitor (slot 7 with 1 deletes it)
    V(T_AI, 1, "v", 'i', 0),
    V(T_AI, 19, "", 'i', 0),
    V(T_SEQUENCE, 23, "v", 'n', T_FRAME),
    V(T_FRAME, 39, "", 'i', 0),
    V(T_FRAME, 45, "", 'i', 0),
    VC(T_CATALOGUE, 1, "v", c_name),     // the definition lists' slots: the name of entry i
    VC(T_CATALOGUE, 5, "", c_name),      // the folder
    V(T_CATALOGUE, 7, "v", 'i', 0),
    V(T_CATALOGUE, 27, "p", 'v', 0),
    V(T_CATALOGUE, 37, "ssp", 'n', T_OTHER),
    V(T_CATALOGUE, 39, "vp", 'i', 0),
    V(T_CATALOGUE, 40, "", 'i', 0),
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
extern char ww_thunks[];
DWORD ww_pop, ww_ret_float, ww_edx;
long double ww_fret;
DWORD __cdecl ww_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _ww_thunks\n"
        "_ww_thunks:\n"
        ".set ww_i, 0\n"
        ".rept 5024\n"
        "  movl $ww_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _ww_common - . - 4\n"
        ".set ww_i, ww_i + 1\n"
        ".endr\n"
        "_ww_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _ww_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _ww_ret_float\n"
        "  je 1f\n"
        "  fldt _ww_fret\n"
        "1:\n"
        "  movl _ww_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _ww_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(ww_thunks) + 10u * (DWORD)i; }

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
        case 'c': return dword_at(v, 0);
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

DWORD __cdecl ww_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            ww_pop = 4;
            ww_ret_float = 0;
            ww_edx = 0;
            return 0;
        }
    }
    Call c{frame + 1, ecx, edx, 0, false};
    if (g_trace)
        log("      side %d call %x ecx %x", g_side, id, ecx);
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
    ww_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    ww_ret_float = c.fset ? 1 : 0;
    ww_fret = c.fret;
    ww_edx = nv();
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
    for (int k = 0; k < 5024; k += 997) {
        const BYTE *th = reinterpret_cast<const BYTE *>(ww_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != (DWORD)k || th[5] != 0xe9) {
            log("  weapon checks: recorder thunk %d is broken", k);
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
    int (*const tables[])(Hook *) = {hooks_weapons, hooks_deployables, hooks_death_match};
    const char *const names[] = {"weapons", "deployables", "death_match"};
    for (int t = 0; t < 3; t++) {
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
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x472000), 0xa000);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x472000), 0xa000);
    g_nsites = 0;
}

// ---- The world's globals -----------------------------------------------------------------------------------

struct Global {
    DWORD address, size;
    bool zstring;   // a function-local static ZString: compared by its text, a new one made by a side freed
};
static const Global GLOBALS[] = {
    {0x6c2080, 4, false}, {0x713f14, 4, false}, {0x6c02cc, 4, false},
    {0x6c0c08, 4, false}, {0x6c0c38, 4, false}, {0x6c0c3c, 4, false},   // the item catalogue: vtable, records, count
    {0x6bd5a9, 1, false},                                                // the static projectile's guard
    {0x6bd750, 4, false}, {0x6bd720, 4, false},                          // the damage radius's creator and type
    {0x6bd75c, 4, false}, {0x6bd8a8, 1, false}, {0x6bd8a4, 4, true},     // the pain delay factor, the guard, "$trigger"
    {0x6c29a0, 4, false},                                                // the player's categories
    {0x6bda40, 1, false},
    {0x6bd8b0, 4, false}, {0x6bd8e0, 4, false}, {0x6bd8e4, 4, false},   // the death behaviour list: vtable, records, count
};
constexpr int NGLOBALS = sizeof GLOBALS / sizeof GLOBALS[0];
constexpr int G_PAIN_TEXT = 11, G_PAIN_GUARD = 10;
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

static const char *const RECORD_NAMES[] = {"Flamer", "Sub/Lightning Gun", "-Mine", "Turret"};
static void make_records(BYTE *r, Gen &g)
{
    for (DWORD i = 0; i < 4; i++) {
        BYTE *rec = r + i * 0x80;
        const char *name = RECORD_NAMES[i];
        DWORD k = 0;
        for (; name[k]; k++)
            rec[k] = (BYTE)name[k];
        rec[k] = 0;
        const DWORD v = g.next();
        fl<DWORD>(rec, 0x78) = v % 4 ? ptrv(obj(O_DEF + (int)(g.next() % N_DEF))) : 0;
        rec[0x7c] = (BYTE)(g.next() % 4 == 0);
        rec[0x7d] = (BYTE)(g.next() % 3 == 0);
        rec[0x7e] = (BYTE)(g.next() % 3 == 0);
        rec[0x7f] = (BYTE)(g.next() % 8 == 0);
    }
}

void build_world(unsigned seed)
{
    Gen g{seed * 2654435761u + 77};
    DWORD *all = reinterpret_cast<DWORD *>(&g_block);
    for (DWORD i = 0; i < sizeof g_block / 4; i++)
        all[i] = g.next();
    auto vt = [](int o, int type) { fl<DWORD>(obj(o), 0) = ptrv(g_fvt[type]); };
    auto other = [&](int null_one_in) -> DWORD { return null_one_in && g.next() % (DWORD)null_one_in == 0 ? 0 : ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))); };
    auto entity = [&](int null_one_in) -> DWORD { return null_one_in && g.next() % (DWORD)null_one_in == 0 ? 0 : ptrv(obj(O_ENTITY + (int)(g.next() % N_ENTITY))); };
    for (int i = 0; i < N_ENTITY; i++) {
        BYTE *e = obj(O_ENTITY + i);
        vt(O_ENTITY + i, T_ENTITY);
        fl<DWORD>(e, 0x1c) = g.next();
        fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        fl<DWORD>(e, 0xc4) = g.fbits_(0.0f, 6.3f);
    }
    for (int i = 0; i < N_INVENTORY; i++)
        vt(O_INVENTORY + i, T_INVENTORY);
    for (int i = 0; i < N_ITEM; i++) {
        BYTE *it = obj(O_ITEM + i);
        vt(O_ITEM + i, T_ITEM);
        fl<DWORD>(it, 8) = g.next() % 4;
        fl<BYTE>(it, 0x11) = (BYTE)(g.next() % 2);
        fl<DWORD>(it, 0x18) = entity(3);
        fl<DWORD>(it, 0x1c) = entity(3);
    }
    for (int i = 0; i < N_DEF; i++) {
        BYTE *d = obj(O_DEF + i);
        vt(O_DEF + i, T_DEF);
        fl<DWORD>(d, 0x1c) = other(3);
        fl<DWORD>(d, 0x20) = g.next() % 4;
        fl<DWORD>(d, 0x24) = g.next() % 3 ? 0 : 1 + g.next() % 8;
        fl<DWORD>(d, 0x2c) = g.next() % 3 ? g.next() % 4 : 0;
        fl<DWORD>(d, 0x38) = g.next() % 4;
        fl<DWORD>(d, 0x3c) = g.fbits_(0.0f, 600.0f);
        fl<DWORD>(d, 0x4c) = g.fbits_(-20.0f, 20.0f);
        fl<DWORD>(d, 0xcc) = other(0);   // read without a test (its continuous fire data)
    }
    vt(O_WORLD, T_WORLD);
    vt(O_APP, T_APP);
    vt(O_REPORTER, T_REPORTER);
    vt(O_GAMETYPE, T_GAMETYPE);
    vt(O_VIEW, T_VIEW);
    vt(O_LAYER, T_LAYER);
    fl<DWORD>(obj(O_LAYER), 0x78) = ptrv(obj(O_SPARE));
    for (int i = 0; i < N_AI; i++)
        vt(O_AI + i, T_AI);
    for (int i = 0; i < N_ACTION; i++)
        vt(O_ACTION + i, T_ACTION);
    for (int i = 0; i < N_OTHER; i++) {
        BYTE *o = obj(O_OTHER + i);
        vt(O_OTHER + i, T_OTHER);
        fl<DWORD>(o, 8) = other(0);
        fl<DWORD>(o, 0xc) = other(0);
        fl<DWORD>(o, 0x14) = g.next() % 3 ? g.next() % 12 : 0;
        fl<DWORD>(o, 0x18) = g.fbits_(-10.0f, 200.0f);
        fl<DWORD>(o, 0x1c) = g.next() % 3 ? g.fbits_(0.0f, 3.0f) : 0;
        fl<DWORD>(o, 0x38) = g.next() % 4;
        fl<DWORD>(o, 0x74) = entity(3);
        fl<DWORD>(o, 0xb0) = g.fbits_(0.0f, 400.0f);
        fl<DWORD>(o, 0xb4) = g.fbits_(0.0f, 400.0f);
        fl<DWORD>(o, 0xbc) = g.next() % 2 ? 0 : g.fbits_(0.0f, 4.0f);
        fl<DWORD>(o, 0xd0) = ptrv(g_fvt[T_OTHER]);
        fl<short>(o, 0xf6) = (short)(g.next() % 14 - 2);
    }
    vt(O_SEQUENCE, T_SEQUENCE);
    vt(O_FRAME, T_FRAME);
    make_records(obj(O_CAT_RECORDS), g);
    make_records(obj(O_CAT_RECORDS2), g);
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    g_case_globals[0][0] = ptrv(obj(O_WORLD));
    g_case_globals[1][0] = ptrv(obj(O_APP));
    g_case_globals[2][0] = ptrv(obj(O_REPORTER));
    g_case_globals[3][0] = ptrv(g_fvt[T_CATALOGUE]);
    g_case_globals[4][0] = g.next() % 8 ? ptrv(obj(O_CAT_RECORDS)) : 0;
    g_case_globals[5][0] = g.next() % 5;
    g_case_globals[6][0] = g.next() % 2;
    g_case_globals[7][0] = entity(3);
    g_case_globals[8][0] = g.next() % 4;
    g_case_globals[9][0] = g.fbits_(0.0f, 0.001f);
    g_case_globals[G_PAIN_GUARD][0] = g.next() % 4;
    g_case_globals[G_PAIN_TEXT][0] = g_case_globals[G_PAIN_GUARD][0] & 2 ? g_valid_text[G_PAIN_TEXT] : 0;
    g_case_globals[12][0] = g.next() % 3 ? 0x2 : g.next();
    g_case_globals[13][0] = g.next() % 2;
    g_case_globals[14][0] = ptrv(g_fvt[T_CATALOGUE]);
    g_case_globals[15][0] = g.next() % 8 ? ptrv(obj(O_CAT_RECORDS2)) : 0;
    g_case_globals[16][0] = g.next() % 5;
}

// The valid ZStrings the statics hold when their guard bit is set (made once, kept)
void make_static_texts()
{
    if (!g_valid_text[G_PAIN_TEXT])
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_valid_text[G_PAIN_TEXT], "$trigger");
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
        if (!memcmp(g_log[0].ev, g_log[1].ev, sizeof g_log[0].ev[0] * (n < LOGN ? n : LOGN)))
            log("    the first %d calls are the same", n < LOGN ? n : LOGN);
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
    using F7 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    switch (n) {
    case -1: return at<DWORD(__fastcall *)(void *, DWORD)>(fn)(self, a[0]);   // fastcall: the object, edx
    case -16: {   // no arguments, a float in st(0): its ten bytes hashed
        long double r = at<long double(THISCALL *)(void *)>(fn)(self);
        return fnv(2166136261u, &r, 10);
    }
    case -8:   // fastcall: the object, edx a[0], seven stack arguments
        return at<DWORD(__fastcall *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(self, a[0], a[1], a[2], a[3], a[4],
                                                                                                        a[5], a[6], a[7]);
    case 0: return at<F0>(fn)(self);
    case 1: return at<F1>(fn)(self, a[0]);
    case 2: return at<F2>(fn)(self, a[0], a[1]);
    case 3: return at<F3>(fn)(self, a[0], a[1], a[2]);
    case 4: return at<F4>(fn)(self, a[0], a[1], a[2], a[3]);
    case 5: return at<F5>(fn)(self, a[0], a[1], a[2], a[3], a[4]);
    case 6: return at<F6>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5]);
    case 7: return at<F7>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    }
    return 0;
}

void (*g_after_side)();
void (*g_before_state)();
void (*g_before_call)();
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
        if (g_before_call)
            g_before_call();
        const WORD before = fpu_status();
        if (side == 0)
            hooks_out();
        if (g_trace)
            log("      side %d: calling %x", side, st.address);
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

}  // namespace zax::ww
