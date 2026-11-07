// The fake world of the AI slot checks (see checks_ai_world.h).
#include "checks_ai_world.h"
#include "class_info.h"

namespace zax {
int hooks_ai_patrol(Hook *out);
int hooks_ai_pickup(Hook *out);
int hooks_ai_player(Hook *out);
int hooks_ai_simple(Hook *out);
int hooks_ai_shield(Hook *out);
int hooks_ai_skeleton(Hook *out);
int hooks_behavior_rest(Hook *out);
}  // namespace zax

namespace zax::aiw {

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
    const char *args;   // p pointer, v value, b low byte, f two floats behind it, d the dword behind it, S a sweep's movement, s string,
                        // z a ZString's text,
                        // m a message object, o / x not recorded,
                        // B a byte whose upper three bytes may be a stack address's (tagged like 'p' tags the stack)
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
static const char *const SEQUENCES[] = {"Death", "death", "Idle", "Walk", "Shoot"};
static const char *const NAMES[] = {"Weapons/Pistol", "Pistol", "Items/Ammo Box", ""};

static DWORD c_pos(Call &c) { return write_pos(c.a[0], 0.0f, 4000.0f); }
static DWORD c_offset(Call &c) { return write_pos(c.a[0], -100.0f, 100.0f); }
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
        c.fret = frange(-5.0f, 50.0f);
    else
        c.fret = (long double)fbits(c.a[0]) + ((long double)fbits(c.a[1]) - fbits(c.a[0])) * (long double)(int)(v >> 8) * (1.0L / 16777216.0L);
    c.fset = true;
    return nv();
}
static DWORD c_push(Call &c) { return (nv() & 7) == 0 ? 0 : c.a[1]; }
static DWORD c_append(Call &c) { return c.a[0]; }
static DWORD c_index(Call &) { return nv() % 8; }   // an index into the fake polygons' 16 points
static DWORD c_small(Call &) { return nv() % 5; }   // a count the code loops over
// 0x42a2b0: a CInventoryItem constructed on the stack becomes a fake item
static DWORD c_item_ctor(Call &c)
{
    BYTE *o = at<BYTE *>(c.ecx);
    fl<DWORD>(o, 0) = ptrv(g_fvt[T_ITEM]);
    fl<DWORD>(o, 8) = nv() % 4;
    fl<BYTE>(o, 0x10) = (BYTE)(nv() % 2);
    return c.ecx;
}
static DWORD c_sequence(Call &) { return ptrv(SEQUENCES[nv() % 5]); }
static DWORD c_name(Call &) { return ptrv(NAMES[nv() % 4]); }
static DWORD c_polygon(Call &) { return ptrv(obj(O_POLY + (int)(nv() % N_POLY))); }
static DWORD c_model(Call &) { return ptrv(obj(O_MODEL)); }
static DWORD c_settings(Call &) { return ptrv(obj(O_SETTINGS)); }
static DWORD c_registry(Call &) { return ptrv(obj(O_REGISTRY)); }
static DWORD c_palette(Call &) { return ptrv(obj(O_PALETTE)); }
static DWORD c_projectile(Call &) { return ptrv(obj(O_PROJECTILE)); }
static DWORD c_player(Call &) { return (nv() & 7) == 0 ? 0 : ptrv(obj(O_PLAYER)); }
static DWORD c_time(Call &c)
{
    c.fret = frange(0.0f, 5000.0f);
    c.fset = true;
    return nv();
}
static DWORD c_minus_one_often(Call &)
{
    const DWORD v = nv();
    return v % 4 == 0 ? 0xffffffffu : (v >> 4) % 8;
}
// 0x425220 (category, array): the array (empty before) gets 0-3 item indices; its data from the pool as
// the exe's arrays (the code frees it)
static DWORD c_collect(Call &c)
{
    DWORD *a = at<DWORD *>(c.a[1]);
    const DWORD n = nv() % 4, cap = n + nv() % 2;
    if (cap) {
        DWORD *d = static_cast<DWORD *>(pool_get_alloc(cap * 4));
        for (DWORD i = 0; i < cap; i++)
            d[i] = nv() % 8;
        a[1] = ptrv(d);
    }
    a[2] = n;
    a[3] = cap;
    return nv();
}
static DWORD c_read_dword(Call &c)
{
    if (c.a[0])
        *at<DWORD *>(c.a[0]) = (nv() & 1) ? *at<DWORD *>(ptrv(obj(O_SELF)) + 0x118) : nv();
    return nv();
}
static DWORD c_read_byte(Call &c)
{
    if (c.a[0])
        *at<BYTE *>(c.a[0]) = (BYTE)(nv() % 3);
    return nv();
}
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}

// The behaviours and AIs of 0x49c000-0x4a7fff and 0x578000-0x587fff (checks_behavior_rest.cpp): a weapon's
// sequence name (a ZString the case set up in the extra object 7) and an integer in [lo, hi]
static DWORD c_zstring(Call &) { return ptrv(obj(O_EXTRA + 7)); }
static DWORD c_between(Call &c)
{
    const DWORD lo = c.a[0], hi = c.a[1];
    return hi >= lo && hi - lo < 0x1000 ? lo + nv() % (hi - lo + 1) : lo;
}

// An entity's velocity (slot 116: x out, y out)
static DWORD c_velocity(Call &c)
{
    for (int i = 0; i < 2; i++)
        if (c.a[i] && readable(at<void *>(c.a[i]), 4))
            *at<float *>(c.a[i]) = frange(-300.0f, 300.0f);
    return nv();
}

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    S(0x4916f0, "p-", "pp", 'v', 0),                         // the mover's part of a sweep
    S(0x4e8e00, "p-", "pfSvvvvvv", 'b', 0),                  // CLayer_Sweep
    S(0x4f5d80, "p-", "", 'p', T_ENTITY),                    // World::FirstCharacter
    SC(0x523df0, "p-", "sv", c_find),                        // ResourceList_Find
    S(0x425290, "p-", "v", 'i', 0),                          // CInventory_CurIndex
    S(0x4308c0, "pf", "vvvvv", 'n', T_BEHAVIOR),             // a new CEntityBehaviorMoveToPos
    SC(0x49a560, "p-", "ppv", c_push),                       // CEntityBehaviorStack::Push
    SC(0x49a620, "p-", "p", c_append),                       // CEntityBehaviorStack::Append
    S(0x49a6b0, "p-", "p", 'i', 0),                          // CEntityBehaviorStack::Clear
    S(0x498f10, "pv", "vv", 'n', T_BEHAVIOR),                // EntityBehaviorIdle_Make
    S(0x4953b0, "pv", "vv", 'n', T_BEHAVIOR),                // EntityBehaviorTurnToAngle_Make
    S(0x4974e0, "vv", "v", 'n', T_BEHAVIOR),                 // EntityBehaviorSetPlayRatio_Make
    S(0x5ab300, "vv", "v", 'n', T_BEHAVIOR),                 // a 0x603e38 behaviour
    S(0x495ec0, "pp", "vvvvv", 'n', T_BEHAVIOR),             // EntityBehaviorTurnToTarget_Make
    S(0x48f6a0, "p-", "v", 'v', 0),                          // EntityAnimated_SetPlayRatio
    S(0x48f700, "p-", "s", 'b', 0),                          // EntityAnimated_PlaySequence
    S(0x4303f0, "p-", "pfvv", 'b', 0),                       // CEntityBehaviorMoveToPos's aim
    S(0x41ad40, "pp", "pppvvv", 'n', T_ENTITY),              // the part a name refers to
    S(0x41a720, "p-", "pppppp", 'b', 0),                     // an action's slot 23
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    SC(0x546bc0, "p-", "fvv", c_index),                      // Polygon_NearestVertex
    S(0x491250, "p-", "f", 'b', 0),                          // the way to a point free
    SC(0x55c470, "p-", "v", c_random_below),                 // Random_Below
    SC(0x55c550, "p-", "vv", c_random_range),                // a random float
    {0x5d0863, "--", "v", 'i', 0, true, nullptr},            // atexit
    S(0x4fccc0, "pf", "vvp", 'i', 0),                        // an oval of the view
    S(0x546d40, "p-", "pvvpvv", 'i', 0),                     // a polygon's outline
    S(0x4b33f0, "p-", "vvvvp", 'i', 0),                      // a rectangle of the view
    SC(0x48b290, "p-", "", c_model),                         // Entity_Model
    S(0x4fc8a0, "p-", "v", 'v', 0),                          // CLayerPart_LayerAdd
    S(0x5babc0, "pv", "", 'i', 0),                           // the player of an entity
    S(0x5a3480, "p-", "vvv", 'v', 0),                        // the HUD's item display
    S(0x4f60e0, "p-", "pvpvv", 'v', 0),                      // World::BroadcastExplosionAt
    S(0x416760, "p-", "vv", 'p', T_ENTITY),                  // Object_Clone
    S(0x4fbc90, "p-", "p", 'v', 0),                          // CLayerPart_RemoveAI
    S(0x546a90, "p-", "", 'f', 0),                           // Polygon_Size
    S(0x4fb680, "p-", "vv", 'v', 0),                         // CLayerPart_MoveCenterToXY
    S(0x4f1050, "p-", "", 'n', T_LAYER),                     // World::CurrentLayer
    S(0x4eb6f0, "p-", "p", 'v', 0),                          // CLayer::AddPart
    SC(0x42a2b0, "p-", "", c_item_ctor),                     // CInventoryItem's constructor
    S(0x42a300, "p-", "", 'i', 0),                           // CInventoryItem's destructor
    S(0x417230, "p-", "", 'v', 0),                           // CObject_ResetFields
    S(0x54fdb0, "p-", "v", 'v', 0),                          // the item's definition set
    S(0x4dd480, "p-", "", 'n', T_DEF),                       // ItemRef_Definition
    S(0x48d8f0, "p-", "v", 'p', T_DEF),                      // DefinitionList_Object
    S(0x5b7aa0, "p-", "", 'n', T_ITEM),                      // a definition's default item
    {0x415d20, "--", "", 'c', 0, true, c_registry},          // ClassRegistry_Get
    S(0x415c00, "p-", "svv", 'n', T_ITEM),                   // ClassRegistry_Create
    S(0x416af0, "p-", "p", 'v', 0),                          // Object_ReadFromStream
    S(0x417100, "p-", "p", 'b', 0),                          // Object_HasChanged
    SC(0x591d00, "p-", "v", c_name),                         // RecordList_Name
    S(0x4d24c0, "ss", "", 'v', 0),                           // ShowErrorBox
    {0x433a80, "--", "", 'c', 0, true, c_palette},           // the palette (cdecl)
    S(0x526e40, "p-", "vvvvv", 'i', 0),                      // the nearest palette colour
    S(0x562010, "p-", "pv", 'i', 0),                         // CSameAsParentAI's Update
    S(0x4fc6f0, "p-", "", 'n', T_ENTITY),                    // CLayerPart_Root
    S(0x424ed0, "p-", "", 'v', 0),                           // CInventory::Clear
    S(0x4262e0, "p-", "", 'v', 0),                           // CInventory_Repair
    S(0x424f60, "p-", "v", 'n', T_ITEM),                     // CInventory::ItemAt
    SC(0x425220, "p-", "vp", c_collect),                     // CInventory_CollectOfCategory
    S(0x4252b0, "p-", "v", 'n', T_DEF),                      // CInventory_CurDefinition
    S(0x4252e0, "p-", "v", 'b', 0),                          // CInventory_ChangingToItem
    SC(0x425350, "p-", "vv", c_minus_one_often),             // CInventory_PrevOfCategory
    SC(0x425470, "p-", "vv", c_minus_one_often),             // CInventory_NextOfCategory
    S(0x425590, "p-", "vvpv", 'v', 0),                       // CInventory_ChangeTo
    S(0x4256c0, "p-", "vpv", 'v', 0),                        // CInventory_CycleNext
    S(0x425700, "p-", "vpv", 'v', 0),                        // CInventory_CyclePrev
    SC(0x4257a0, "p-", "v", c_minus_one_often),              // CInventory_IndexOfDefinitionIndex
    S(0x48d020, "p-", "", 'v', 0),                           // EntityDestructable_ResetDamage
    S(0x48d030, "p-", "", 'f', 0),                           // EntityDestructable_HitPointsLeft
    S(0x48fb50, "p-", "v", 'v', 0),                          // EntityAnimated_SetSynch
    S(0x48ebf0, "p-", "b", 'v', 0),                          // EntityAnimated_SetFacingBackwards
    SC(0x48aee0, "p-", "", c_sequence),                      // Entity_SequenceName
    S(0x46ed70, "p-", "v", 'b', 0),                          // the player object: action pressed
    S(0x470b50, "p-", "", 'f', 0),                           // the player object: aiming angle
    SC(0x46f4a0, "p-", "v", c_name),                         // the player object: the item of a key
    S(0x591fc0, "p-", "sv", 'i', 0),                         // NamedList_FindOrFail
    S(0x57e7b0, "p-", "s", 'v', 0),                          // the sound manager: precache
    S(0x57e820, "p-", "s", 'v', 0),                          // the sound manager: play
    S(0x5085f0, "p-", "", 'v', 0),                           // the game object's +0x344 update
    S(0x429e40, "p-", "", 'v', 0),                           // inventory window pages
    S(0x429e30, "p-", "", 'v', 0),
    S(0x429e20, "p-", "", 'v', 0),
    S(0x5a0760, "p-", "", 'v', 0),
    S(0x59ff80, "p-", "", 'n', T_GAMETYPE),                  // the game type
    S(0x59e6f0, "p-", "v", 'b', 0),                          // auto weapon switch for a player
    S(0x5aeec0, "pp", "", 'i', 0),                           // the rounds an entity has
    S(0x4d77c0, "p-", "", 'b', 0),                           // CInventoryItemDefinition::HasAmmunition
    S(0x5b7640, "p-", "", 'b', 0),
    S(0x5b7920, "p-", "p", 'b', 0),
    S(0x5b0e50, "p-", "", 'f', 0),
    // ByteField_Set (object, value): CPlayerWalkingControlAI::Fire (0x543830) passes the firing direction's
    // result in the low byte of its angle argument's dword, i.e. over a pointer into its caller's stack frame:
    // those upper bytes are Update's (0x543b60) frame layout, the original's and ours differ. They showed on
    // real x86 only (0x1ae301 / 0x1ae201, job 20261031-164306-8d3f), where the two frames straddle a 256-byte
    // boundary. 0x62ea2c's descriptor has no setter (0x42b100), so only the low byte is stored.
    S(0x54dec0, "p-", "pB", 'v', 0),
    S(0x5ab900, "vv", "v", 'n', T_BEHAVIOR),
    S(0x47a0f0, "p-", "p", 'v', 0),                          // UndeployedInventoryItem_Deploy
    S(0x47a990, "p-", "p", 'v', 0),
    SC(0x4d61f0, "p-", "", c_projectile),                    // CInventoryItemDefinition::Projectile
    S(0x4f6960, "p-", "v", 'f', 0),                          // World::Zoom
    S(0x4f6770, "p-", "vv", 'v', 0),                         // World::SetZoom
    SC(0x5a1f40, "p-", "", c_settings),                      // the walking settings of the rules
    S(0x4a21c0, "p-", "v", 'v', 0),                          // FileBuf_WriteDword
    S(0x4a21e0, "p-", "b", 'v', 0),                          // FileBuf_WriteByte
    SC(0x4a2270, "p-", "o", c_read_dword),                   // FileBuf_ReadDword
    SC(0x4a22a0, "p-", "o", c_read_byte),                    // FileBuf_ReadByte
    S(0x5176f0, "p-", "vpvvv", 'n', T_ENTITY),               // Entity_CreateFromModel
    S(0x5173f0, "p-", "v", 'i', 0),                          // CModel::Sequence
    {0x5d0e36, "--", "v", 'v', 0, true, nullptr},            // exit
    S(0x4fc8c0, "p-", "s", 'v', 0),                          // CLayerPart_LayerAddNamed
    S(0x417500, "p-", "ssp", 'i', 0),                        // CObject_ReadOwnLine
    // the behaviours and AIs of 0x49c000-0x4a7fff and 0x578000-0x587fff (checks_behavior_rest.cpp)
    SC(0x517240, "p-", "s", c_minus_one_often),              // CModel::FindSequence
    S(0x496e20, "pz", "vpvvv", 'p', T_BEHAVIOR),             // EntityBehaviorPlaySequenceOnce_Make
    S(0x41a740, "p-", "ppppp", 'b', 0),                      // CAction_ExecuteFrom
    S(0x4267e0, "p-", "", 'n', T_INVENTORY),                 // LayerPart_GetInventory
    S(0x4fb3c0, "p-", "pv", 'f', 0),                         // CLayerPart_TurnToPart
    S(0x4d6520, "p-", "", 'f', 0),                           // CInventoryItemDefinition::ShootAngleRadians
    S(0x4fcce0, "pf", "vvpvv", 'i', 0),                      // an arc of the view
    S(0x517510, "p-", "", 'f', 0),                           // CModel::TurnSpeedRadians
    S(0x4faf50, "p-", "", 'f', 0),                           // CLayerPart_SlotDistance2
    S(0x4fc390, "p-", "vvvvvv", 'n', T_ENTITY),              // CLayerPart_FindNearest
    SC(0x55c4e0, "p-", "vv", c_between),                     // Random_Between
    // the AI modules of 0x568000-0x577fff
    S(0x4fd100, "p-", "pm", 'b', 0),                         // CLayerPartAI's slot 29
    S(0x4fd0b0, "p-", "pp", 'i', 0),                         // an AI's debug drawing
    S(0x4fd160, "p-", "pppp", 'i', 0),                       // an AI's action field run
    S(0x48d150, "p-", "", 'f', 0),                           // EntityDestructable_HitPoints
    S(0x48d260, "p-", "", 'f', 0),                           // EntityDestructable_HealthRatio
    SC(0x4251c0, "p-", "v", c_small),                        // CInventory_CountOfCategory
    SC(0x425340, "p-", "v", c_minus_one_often),              // CInventory_FirstOfCategory (its reimplementation calls Next directly)
    S(0x4fba60, "p-", "v", 'n', T_AI),                       // CLayerPart_AIAt
    S(0x4fbc50, "p-", "p", 'v', 0),                          // CLayerPart_AddAI
    S(0x42a550, "p-", "pp", 'i', 0),                         // CInventoryItem::Take
    S(0x42a500, "p-", "p", 'b', 0),                          // CInventoryItem::CanAdd
    S(0x426090, "p-", "v", 'v', 0),                          // CInventory_RemoveAt
    S(0x426260, "p-", "p", 'v', 0),                          // CInventory_Remove
    S(0x425860, "p-", "p", 'n', T_ITEM),                     // CInventory_FindOfClass
    SC(0x470e60, "p-", "dv", c_minus_one_often),             // PtrArray_Find
    S(0x4fc710, "p-", "p", 'v', 0),                          // CLayerPart_AddChild
    S(0x4fc1b0, "p-", "pvs", 'v', 0),                        // LayerPart_AddListener
    S(0x4fc200, "p-", "pvs", 'v', 0),                        // LayerPart_RemoveListener
    S(0x4fbf30, "p-", "", 'i', 0),                           // LayerPart_Died
    S(0x48b590, "p-", "", 'n', T_OTHER),                     // Entity_SequenceObject
    S(0x560300, "p-", "vvp", 'v', 0),                        // the sequence player's frame step
    S(0x48feb0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrame
    S(0x490010, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrameIndex
    S(0x48b6b0, "p-", "v", 'i', 0),                          // Entity_SetRenderHeight
    S(0x55e090, "p-", "v", 'i', 0),                          // the damage type list's load
    S(0x491380, "p-", "pvvvv", 'b', 0),                      // EntityMovable_CanSee
    S(0x4fb340, "p-", "fvv", 'f', 0),                        // CLayerPart_TurnTo
    S(0x506620, "p-", "s", 'b', 0),                          // a message's class name test
    {0x55d070, "pp", "v", 'v', 0, false, nullptr},           // a call for reinforcements (fastcall)
    S(0x5afe50, "p-", "", 'f', 0),                           // CEnergyItem::MaxCharge
    S(0x5afe90, "p-", "v", 'v', 0),                          // CEnergyItem::Add
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
    V(T_ENTITY, 36, "", 'n', T_INVENTORY),
    V(T_ENTITY, 39, "", 'v', 0),
    VC(T_ENTITY, 40, "o", c_pos),
    VC(T_ENTITY, 43, "o", c_pos),
    VC(T_ENTITY, 48, "", c_polygon),
    VC(T_ENTITY, 50, "o", c_offset),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 63, "m", 'b', 0),
    V(T_ENTITY, 64, "pv", 'n', T_AI),
    V(T_ENTITY, 68, "", 'n', T_ENTITY),
    V(T_ENTITY, 86, "p", 'i', 0),
    V(T_ENTITY, 103, "vv", 'v', 0),
    V(T_ENTITY, 104, "vv", 'v', 0),
    V(T_ENTITY, 110, "v", 'v', 0),
    V(T_BEHAVIOR, 1, "v", 'i', 0),
    V(T_BEHAVIOR, 19, "", 'v', 0),
    V(T_BEHAVIOR, 23, "p", 'b', 0),
    V(T_BEHAVIOR, 25, "vp", 'b', 0),
    V(T_BEHAVIOR, 28, "pp", 'i', 0),
    V(T_INVENTORY, 20, "v", 'v', 0),
    V(T_INVENTORY, 26, "v", 'n', T_ITEM),
    V(T_ITEM, 1, "v", 'i', 0),
    V(T_ITEM, 32, "p", 'b', 0),
    V(T_ITEM, 33, "pp", 'i', 0),
    V(T_ITEM, 38, "p", 'b', 0),
    V(T_ITEM, 45, "v", 'i', 0),
    V(T_ITEM, 49, "", 'f', 0),
    V(T_DEF, 25, "pppp", 'i', 0),
    VC(T_WORLD, 89, "v", c_player),
    V(T_APP, 84, "", 'b', 0),
    V(T_GAMETYPE, 30, "", 'b', 0),
    V(T_GAMETYPE, 34, "", 'b', 0),
    V(T_GAMETYPE, 38, "", 'b', 0),
    VC(T_REPORTER, 13, "", c_time),
    VC(T_PALOBJ, 58, "", c_palette),
    V(T_VIEW, 39, "", 'i', 0),
    V(T_VIEW, 45, "", 'i', 0),
    V(T_AI, 1, "v", 'i', 0),
    VC(T_CATALOGUE, 1, "v", c_name),
    VC(T_CATALOGUE, 2, "sv", c_find),
    V(T_CATALOGUE, 37, "ssp", 'n', T_DEF),
    VC(T_MODELS, 1, "v", c_name),
    VC(T_MODELS, 3, "sv", c_find),
    V(T_MODELS, 37, "ssp", 'n', T_OTHER),
    // the behaviours and AIs of 0x49c000-0x4a7fff and 0x578000-0x587fff
    V(T_ENTITY, 46, "", 'f', 0),
    V(T_ENTITY, 47, "v", 'i', 0),
    V(T_ENTITY, 102, "", 'b', 0),
    V(T_ITEM, 23, "p", 'i', 0),
    VC(T_ITEM, 35, "p", c_zstring),
    V(T_ITEM, 39, "", 'f', 0),
    V(T_BEHAVIOR, 29, "p", 'i', 0),
    V(T_WORLD, 86, "pv", 'i', 0),
    // the AI modules of 0x568000-0x577fff
    V(T_ENTITY, 24, "p", 'v', 0),     // an entity copy (Object_Clone's) as an AI
    V(T_ENTITY, 29, "pm", 'b', 0),
    V(T_ENTITY, 41, "vv", 'v', 0),
    V(T_ENTITY, 54, "", 'i', 0),
    V(T_ENTITY, 70, "", 'n', T_ENTITY),
    V(T_ENTITY, 71, "p", 'v', 0),
    V(T_ENTITY, 73, "f", 'v', 0),
    V(T_ENTITY, 84, "", 'f', 0),
    V(T_ENTITY, 97, "", 'v', 0),
    V(T_ENTITY, 99, "vvv", 'v', 0),
    VC(T_ENTITY, 116, "oo", c_velocity),
    V(T_ENTITY, 120, "vv", 'v', 0),
    V(T_BEHAVIOR, 34, "", 'n', T_OTHER),
    V(T_DEF, 34, "", 'n', T_OTHER),
    V(T_ITEM, 50, "v", 'b', 0),
    V(T_AI, 24, "p", 'v', 0),
    V(T_AI, 25, "pv", 'v', 0),
    V(T_AI, 29, "pm", 'b', 0),
    V(T_AI, 34, "ppvvv", 'v', 0),
    V(T_WORLD, 58, "", 'p', T_OTHER),
    V(T_WORLD, 91, "vp", 'v', 0),
    V(T_REPORTER, 12, "p", 'v', 0),
    V(T_REPORTER, 17, "s", 'v', 0),
    V(T_OTHER, 2, "d", 'i', 0),
    V(T_OTHER, 26, "", 'f', 0),
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
extern char aiw_thunks[];
DWORD aiw_pop, aiw_ret_float, aiw_edx;
long double aiw_fret;
DWORD __cdecl aiw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _aiw_thunks\n"
        "_aiw_thunks:\n"
        ".set aiw_i, 0\n"
        ".rept 4296\n"
        "  movl $aiw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _aiw_common - . - 4\n"
        ".set aiw_i, aiw_i + 1\n"
        ".endr\n"
        "_aiw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _aiw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _aiw_ret_float\n"
        "  je 1f\n"
        "  fldt _aiw_fret\n"
        "1:\n"
        "  movl _aiw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _aiw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(aiw_thunks) + 10u * (DWORD)i; }

static void record(DWORD id, const Spec &s, Call &c)
{
    DWORD e[9] = {id, 0, 0, 0, 0, 0, 0, 0, 0};
    int k = 1;
    auto value = [&](char kind, DWORD v) -> DWORD {
        switch (kind) {
        case 'p': return rp(v);
        case 'v': return v;
        case 'b': return v & 0xff;
        case 'B': {
            // The upper bytes of a stack address are the caller's frame layout, which differs between the
            // original and the reimplementation; the rest of the dword is compared as it is
            const DWORD hi = v & 0xffffff00u;
            return hi >= (stack_limit & 0xffffff00u) && hi < stack_base ? 0xb0000000u | (v & 0xff) : v;
        }
        case 's': return (DWORD)text_hash(v);
        case 'f': return dword_at(v, 0) * 31u + dword_at(v, 4);
        case 'd': return dword_at(v, 0);
        case 'S': return dword_at(v, 8) * 31u + dword_at(v, 0xc);
        case 'z': return v ? (DWORD)text_hash(dword_at(v, 0) + 8) : 0u;
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

DWORD __cdecl aiw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            aiw_pop = 4;
            aiw_ret_float = 0;
            aiw_edx = 0;
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
    aiw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    aiw_ret_float = c.fset ? 1 : 0;
    aiw_fret = c.fret;
    aiw_edx = nv();
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
    static_assert(NTHUNKS == 4296, "the thunk count of aiw_thunks");
    for (int k = 0; k < NTHUNKS; k += 997) {
        const BYTE *th = reinterpret_cast<const BYTE *>(aiw_thunks) + 10 * k;
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
    int (*const tables[])(Hook *) = {hooks_ai_patrol, hooks_ai_pickup, hooks_ai_player, hooks_ai_simple, hooks_ai_shield, hooks_ai_skeleton,
                                     hooks_behavior_rest};
    const char *const names[] = {"ai_patrol", "ai_pickup", "ai_player", "ai_simple", "ai_shield", "ai_skeleton", "behavior_rest"};
    for (int t = 0; t < 7; t++) {
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
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x538000), 0x10000);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x568000), 0x10100);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x578000), 0x10000);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x49c000), 0xc000);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x538000), 0x10000);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x568000), 0x10100);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x578000), 0x10000);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x49c000), 0xc000);
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

}  // namespace zax::aiw
