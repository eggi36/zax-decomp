// The fake world of the AI slot checks (see checks_rest_world.h).
#include "checks_rest_world.h"
#include "class_info.h"

namespace zax {
int hooks_zax_behaviors(Hook *out);
int hooks_ai_chase(Hook *out);
int hooks_control_config(Hook *out);
}  // namespace zax

namespace zax::rw {

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
                                       O_VIEW, O_AI, O_OTHER, O_LAYER, O_ACTION, O_NET, O_NETLAYER, O_PACKET, 0};
static const int TYPE_COUNT[NTYPES] = {N_ENTITY, N_BEHAVIOR, N_INVENTORY, N_ITEM, N_DEF, 1, 1, 1, 1, 1, N_AI, N_OTHER, 1, N_ACTION, 1, 1, 1, 0};
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
static const char *const TEXTS[] = {"", "Hello there", "x", "%s %d", "Sir Zax of the very long name, who does not fit into a short buffer"};
static const char *const JOYSTICKS[] = {"Gamepad Pro", "NO JOYSTICK", "", "Gamepad Pro "};

// An extra entry of the side's log (what a check sees beyond the calls, e.g. a block's hash)
void log_event(DWORD id, DWORD a, DWORD b)
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
static DWORD c_push(Call &c) { return (nv() & 3) == 0 ? 0 : c.a[1]; }
static DWORD c_append(Call &c) { return (nv() & 3) == 0 ? 0 : c.a[0]; }
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}
static DWORD c_minus_one_often(Call &)
{
    const DWORD v = nv();
    return v % 4 == 0 ? 0xffffffffu : (v >> 4) % 8;
}
// 0x41a6b0: CAction's constructor on the stack: the 0x20 bytes cleared
static DWORD c_action_ctor(Call &c)
{
    if (c.ecx)
        memset(at<void *>(c.ecx), 0, 0x20);
    return c.ecx;
}
// A point of the fake block: two floats (slot 72) or three dwords (slot 114)
static DWORD c_point(Call &)
{
    BYTE *p = obj(O_POINT);
    write_pos(ptrv(p), -100.0f, 4000.0f);
    fl<DWORD>(p, 8) = nv();
    return ptrv(p);
}
// 0x4ef8e0: the player list's slot of a player: a fake part or 0 in it
static DWORD c_slot(Call &)
{
    BYTE *s = obj(O_SLOT);
    fl<DWORD>(s, 0) = (nv() & 3) == 0 ? 0 : ptrv(obj(O_ENTITY + (int)(nv() % N_ENTITY)));
    return ptrv(s);
}
static DWORD c_dyn(Call &) { return ptrv(obj(O_DYN)); }
static DWORD c_model(Call &) { return ptrv(obj(O_MODEL)); }
static DWORD c_player(Call &) { return ptrv(obj(O_PLAYER)); }
static DWORD c_display(Call &) { return ptrv(obj(O_OTHER + (int)(nv() % N_OTHER))); }
static DWORD c_read_dword(Call &c)
{
    const DWORD v = nv();
    if (c.a[0])
        *at<DWORD *>(c.a[0]) = (v & 3) == 0 ? v : (v >> 4) % 300;
    return nv();
}
static DWORD c_read_bool(Call &c)
{
    if (c.a[0])
        *at<BYTE *>(c.a[0]) = (BYTE)(nv() % 3);
    return nv();
}
static DWORD c_text(Call &) { return ptrv(TEXTS[nv() % 5]); }
// 0x5908b0: the build the server needs (around 100)
static DWORD c_version(Call &) { return 95 + nv() % 10; }
// The game type's slot 39 (player, the colour): sometimes a new colour
static DWORD c_gt_color(Call &c)
{
    if (c.a[1] && (nv() & 1))
        *at<float *>(c.a[1]) = frange(0.0f, 300.0f);
    return nv();
}
// The INI functions: an int is the default or another value; a text one of the samples (the CD keys for
// "CD Key", the bindings' "key,key2,joystick,mouse" otherwise) or the default (the buffer as it is)
static DWORD c_ini_int(Call &c)
{
    const DWORD v = nv();
    return (v & 3) == 0 ? c.a[1] : (v & 3) == 1 ? 0xffffffffu : (v >> 4) % 6;
}
static const char *const CD_KEYS[] = {"", "1234-5678-9012-3456-7890", "ABCD-EFGH", "o1il-bs00-1234-5678-90123", "12345678901234567890",
                                      "9999999999-9999999999", "0000000000-0000000000"};
static const char *const BINDINGS[] = {"", "65,0,16,0", "1,2,3,4", "-1,30,0,128", "57,58,4294967295,8", "0,0,0,0"};
static DWORD c_ini_string(Call &c)
{
    char *buf = at<char *>(c.a[1]);
    const DWORD n = c.a[2];
    const DWORD v = nv();
    if ((v & 3) == 0 || !buf || !n)
        return nv();   // the default stays
    const char *key = at<const char *>(c.a[0]);
    const bool cd = key && readable(key, 7) && key[0] == 'C' && key[1] == 'D';
    const char *t = cd ? CD_KEYS[(v >> 4) % 7] : BINDINGS[(v >> 4) % 6];
    DWORD k = 0;
    for (; t[k] && k + 1 < n; k++)
        buf[k] = t[k];
    buf[k] = 0;
    return k;
}
// The key table: the key pressed (none, Escape, Delete, a forbidden key, others)
static DWORD c_key(Call &)
{
    const DWORD keys[] = {0, 0, 1, 0x153, 0x3b, 0x44, 0x1e, 0x39, 0xc8, 2};
    return keys[nv() % 10];
}
static DWORD c_any(Call &) { return (nv() & 1) ? 0 : 0x1e; }
static DWORD c_poll(Call &)
{
    const DWORD v = nv();
    return (v & 1) ? 0 : (v & 2) ? 1u << ((v >> 2) % 32) : v;
}
static DWORD c_wheel(Call &)
{
    const DWORD v = nv() % 6;
    return v < 3 ? 0 : v == 3 ? 0xffffffffu : v == 4 ? 1 : 0xfffffff0u;
}
// 0x59ff80: the game object's game type, the same for every call of a case (the readers call it twice
// and use the second without a test)
static DWORD c_game_type(Call &) { return (g_vseed * 2654435761u >> 28) % 4 == 0 ? 0 : ptrv(obj(O_GAMETYPE)); }
static DWORD c_joy_name(Call &) { return ptrv(JOYSTICKS[nv() % 4]); }

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    SC(0x523df0, "p-", "sv", c_find),                        // ResourceList_Find
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    SC(0x55c550, "p-", "vv", c_random_range),                // a random float
    S(0x41a720, "p-", "pppppp", 'b', 0),                     // an action's slot 23
    S(0x41a740, "p-", "ppvvp", 'b', 0),                      // an action's slot 23 from a caller
    // zax_behaviors.cpp
    S(0x5bacd0, "p-", "", 'i', 0),                           // ZaxNet_PlayerIndexOf
    SC(0x4f5da0, "p-", "ov", c_pos),                         // World::ViewTarget
    S(0x4f5dd0, "p-", "vvvv", 'v', 0),                       // World::SetViewTarget
    S(0x49a650, "p-", "pp", 'i', 0),                         // CEntityBehaviorStack::RemoveTop
    S(0x4fc6f0, "p-", "", 'n', T_ENTITY),                    // CLayerPart_Root
    S(0x4267e0, "p-", "", 'p', T_INVENTORY),                 // LayerPart_GetInventory
    SC(0x425290, "p-", "v", c_minus_one_often),              // CInventory_CurIndex
    S(0x4252b0, "p-", "v", 'n', T_DEF),                      // CInventory_CurDefinition
    S(0x4d77c0, "p-", "", 'b', 0),                           // CInventoryItemDefinition::HasAmmunition
    S(0x48b000, "p-", "", 'i', 0),                           // Entity_SequenceNameString
    S(0x496e20, "pv", "vvvvv", 'n', T_BEHAVIOR),             // EntityBehaviorPlaySequenceOnce_Make
    SC(0x49a560, "p-", "ppv", c_push),                       // CEntityBehaviorStack::Push
    SC(0x49a620, "p-", "p", c_append),                       // CEntityBehaviorStack::Append
    SC(0x41a6b0, "p-", "", c_action_ctor),                   // CAction::Ctor
    S(0x41a700, "p-", "", 'v', 0),                           // CAction::Dtor
    S(0x4c4dd0, "p-", "vvvv", 'v', 0),                       // CDischargeWeaponThisFrameBaseAction::Set
    SC(0x4ef9e0, "p-", "p", c_minus_one_often),              // the player list's index of a part
    S(0x4fc8c0, "p-", "s", 'v', 0),                          // CLayerPart_LayerAddNamed
    S(0x4dd480, "p-", "", 'p', T_DEF),                       // ItemRef_Definition
    S(0x426930, "pv", "v", 'v', 0),                          // Part_UseItems
    S(0x5b0da0, "p-", "", 'n', T_OTHER),                     // Inventory_Battery
    S(0x5176f0, "p-", "vppvv", 'p', T_ENTITY),               // Entity_CreateFromModel
    S(0x48f700, "p-", "s", 'b', 0),                          // EntityAnimated_PlaySequence
    S(0x4eb6f0, "p-", "p", 'v', 0),                          // CLayer::AddPart
    S(0x51ff30, "p-", "ps", 'i', 0),                         // NetPacket_RegisterReader
    SC(0x51f440, "p-", "v", c_player),                       // NetGame_PlayerAt
    S(0x520700, "p-", "p", 'b', 0),                          // NetPacket_ReadString
    SC(0x520550, "p-", "o", c_read_dword),                   // NetPacket_ReadDword
    SC(0x520580, "p-", "o", c_read_bool),                    // NetPacket_ReadBool
    SC(0x4ef8e0, "p-", "v", c_slot),                         // the player list's part slot
    S(0x4fc7c0, "p-", "", 'i', 0),                           // CLayerPart_ChildCount
    S(0x4fc7d0, "p-", "v", 'n', T_ENTITY),                   // CLayerPart_ChildAt
    S(0x418fc0, "p-", "p", 'b', 0),                          // FieldDesc_IsDynamicIn
    SC(0x418790, "p-", "p", c_dyn),                          // FieldDesc_DynamicPointer
    SC(0x59ff80, "p-", "", c_game_type),                     // the game object's game type
    SC(0x5908b0, "--", "", c_version),                       // the build a client needs
    S(0x5908a0, "--", "", 'i', 0),                           // the build
    S(0x51ff50, "p-", "pv", 'i', 0),                         // NetPacket_WriteBlockType
    S(0x520770, "p-", "s", 'i', 0),                          // NetPacket_WriteText
    S(0x520940, "p-", "v", 'i', 0),                          // NetPacket_WriteDword
    S(0x520a50, "p-", "b", 'i', 0),                          // NetPacket_WriteBool
    S(0x51f530, "p-", "v", 'i', 0),                          // NetGame_FindPlayerById
    SC(0x5206c0, "p-", "", c_text),                          // NetPacket_ReadText
    {0x5c5540, "--", "vvssvv", 'i', 0, true, nullptr},       // gcd_authenticate_user (cdecl)
    S(0x4f1050, "p-", "", 'n', T_LAYER),                     // World::CurrentLayer
    S(0x4f5d60, "p-", "v", 'p', T_ENTITY),                   // World::Character
    S(0x4e77e0, "p-", "p", 'v', 0),                          // CLayerSaveData::SendPartFlags
    S(0x59b260, "p-", "sv", 'v', 0),                         // a message on the screen
    SC(0x5a1e80, "p-", "", c_display),                       // CZaxCustomRules::DialogDisplay
    S(0x51c800, "p-", "", 'v', 0),                           // CMultiPlayerGameType::PlayerReady
    // ai_chase.cpp
    S(0x4eb050, "p-", "pfvvv", 'n', T_ENTITY),               // CLayer::FindBlocker
    S(0x4303f0, "p-", "pfvv", 'b', 0),                       // CEntityBehaviorMoveToPos's aim
    S(0x4fc020, "pf", "v", 'f', 0),                          // LayerPart_Distance2
    S(0x4fb0f0, "p-", "fvv", 'f', 0),                        // CLayerPart_AngleFrom
    S(0x4953a0, "p-", "v", 'v', 0),                          // CEntityBehaviorTurnToAngle::SetTarget
    S(0x491380, "p-", "pvvvv", 'b', 0),                      // EntityMovable_CanSee
    S(0x53e0d0, "p-", "ppv", 'b', 0),                        // CLayerPartAI::True
    S(0x590400, "p-", "", 'p', T_AI),                        // the object of a user defined class reference
    S(0x467d40, "p-", "ps", 'v', 0),                         // CBroadcaster_Broadcast
    SC(0x48b290, "p-", "", c_model),                         // Entity_Model
    // control_config.cpp
    {0x4a4670, "ps", "sv", 'c', 0, false, c_ini_int},       // Ini_GetInt
    S(0x4a4690, "ps", "sv", 'i', 0),                         // Ini_WriteInt
    {0x4a4790, "ps", "sxv", 'c', 0, false, c_ini_string},   // Ini_GetString
    S(0x4a47c0, "ps", "ss", 'i', 0),                         // Ini_WriteString
    S(0x4e2980, "p-", "v", 'v', 0),                          // Keys::ClearPresses
    SC(0x4e2940, "p-", "v", c_key),                          // Keys::TakeOtherPressed
    SC(0x4e2870, "p-", "", c_any),                           // Keys::TakeAny
    S(0x4e27a0, "p-", "v", 'i', 0),                          // Keys::ClearAll
    SC(0x4ddb80, "p-", "", c_poll),                          // CJoystick::Poll
    S(0x4dda00, "p-", "v", 'v', 0),                          // CJoystick::Init
    SC(0x4dde40, "p-", "", c_joy_name),                      // the joystick's name
    SC(0x51f450, "p-", "", c_poll),                          // the mouse's buttons (+0x18)
    SC(0x518520, "p-", "", c_wheel),                         // the mouse wheel
    S(0x57e820, "p-", "s", 'v', 0),                          // a named sound
    S(0x4d2460, "--", "", 'i', 0),                           // timeGetTime
    S(0x470d20, "p-", "v", 'v', 0),                          // ControlConfig::SetJoystick
    {0x5d0863, "--", "v", 'i', 0, true, nullptr},            // atexit
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 160;
static_assert(NDIRECT <= VBASE);
static_assert(VBASE + NTYPES * 256 == 4768, "the thunk count of rw_thunks");

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
    V(T_ENTITY, 54, "", 'i', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 63, "m", 'b', 0),
    V(T_ENTITY, 68, "", 'n', T_ENTITY),
    V(T_ENTITY, 70, "", 'n', T_ENTITY),
    VC(T_ENTITY, 72, "", c_point),
    V(T_ENTITY, 81, "v", 'v', 0),
    V(T_ENTITY, 91, "v", 'v', 0),
    V(T_ENTITY, 99, "vvp", 'v', 0),
    V(T_ENTITY, 108, "t", 'v', 0),
    VC(T_ENTITY, 114, "", c_point),
    V(T_BEHAVIOR, 1, "v", 'i', 0),
    V(T_BEHAVIOR, 23, "p", 'b', 0),
    V(T_BEHAVIOR, 25, "vp", 'b', 0),
    V(T_BEHAVIOR, 28, "vv", 'i', 0),
    V(T_INVENTORY, 26, "v", 'n', T_ITEM),
    V(T_ITEM, 23, "p", 'v', 0),
    V(T_ITEM, 35, "p", 'i', 0),
    V(T_ITEM, 38, "p", 'b', 0),
    V(T_AI, 1, "v", 'i', 0),
    V(T_AI, 24, "p", 'v', 0),
    V(T_AI, 25, "pv", 'v', 0),
    V(T_AI, 41, "vv", 'v', 0),
    V(T_OTHER, 1, "v", 'i', 0),
    V(T_OTHER, 7, "v", 'i', 0),
    V(T_OTHER, 50, "v", 'v', 0),
    V(T_WORLD, 1, "", 'p', T_NET),
    V(T_WORLD, 44, "", 'p', T_NETLAYER),
    V(T_APP, 113, "pvvv", 'v', 0),
    V(T_NET, 13, "", 'b', 0),
    V(T_NET, 20, "vss", 'v', 0),
    V(T_NET, 21, "p", 'i', 0),
    V(T_NETLAYER, 25, "", 'p', T_PACKET),
    V(T_NETLAYER, 28, "", 'i', 0),
    VC(T_NETLAYER, 29, "", c_player),
    VC(T_GAMETYPE, 39, "pd", c_gt_color),
    V(T_GAMETYPE, 47, "v", 'v', 0),
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
extern char rw_thunks[];
DWORD rw_pop, rw_ret_float, rw_edx;
long double rw_fret;
DWORD __cdecl rw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _rw_thunks\n"
        "_rw_thunks:\n"
        ".set rw_i, 0\n"
        ".rept 4768\n"   // VBASE + NTYPES * 256
        "  movl $rw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _rw_common - . - 4\n"
        ".set rw_i, rw_i + 1\n"
        ".endr\n"
        "_rw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _rw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _rw_ret_float\n"
        "  je 1f\n"
        "  fldt _rw_fret\n"
        "1:\n"
        "  movl _rw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _rw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(rw_thunks) + 10u * (DWORD)i; }

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
        case 'd': return dword_at(v, 0);
        case 't': return dword_at(v, 0) * 31u + dword_at(v, 4) * 7u + dword_at(v, 8);
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

DWORD __cdecl rw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            rw_pop = 4;
            rw_ret_float = 0;
            rw_edx = 0;
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
    rw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    rw_ret_float = c.fset ? 1 : 0;
    rw_fret = c.fret;
    rw_edx = nv();
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
    for (int k = 0; k < VBASE + NTYPES * 256; k += 997) {
        const BYTE *th = reinterpret_cast<const BYTE *>(rw_thunks) + 10 * k;
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
    int (*const tables[])(Hook *) = {hooks_zax_behaviors, hooks_ai_chase, hooks_control_config};
    const char *const names[] = {"zax_behaviors", "ai_chase", "control_config"};
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
    FlushInstructionCache(GetCurrentProcess(), at<void *>(TEXT_FROM), TEXT_SIZE);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(TEXT_FROM), TEXT_SIZE);
    g_nsites = 0;
}

// ---- The world's globals -----------------------------------------------------------------------------------

struct Global {
    DWORD address, size;
};
static const Global GLOBALS[] = {
    {0x6c2080, 4}, {0x713f14, 4}, {0x6c02cc, 4},
    {0x6c29bc, 4}, {0x6c29a0, 4}, {0x6c29a8, 4},   // the flags the light beam and the companion read
    {0x6c29fc, 4},                                 // the light beam last made
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

// Valid ZStrings (their buffer pointers) for the net player's name and challenge, made once
static DWORD g_name_text, g_challenge_text;
void make_static_texts()
{
    if (!g_name_text)
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_name_text, "Crater");
    if (!g_challenge_text)
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_challenge_text, "aXbYcZ");
}

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
        fl<DWORD>(e, 0x1c) = g.next() % 3 ? g.next() : 0;
        fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        fl<DWORD>(e, 0x9c) = g.next() % 3;
        fl<DWORD>(e, 0x12c) = g.next() % 3;
        fl<DWORD>(e, 0xb4) = g.fbits_(-0.5f, 1.5f);
        fl<DWORD>(e, 0xcc) = g.fbits_(0.0f, 6.3f);
    }
    for (int i = 0; i < N_BEHAVIOR; i++)
        vt(O_BEHAVIOR + i, T_BEHAVIOR);
    for (int i = 0; i < N_INVENTORY; i++)
        vt(O_INVENTORY + i, T_INVENTORY);
    for (int i = 0; i < N_ITEM; i++)
        vt(O_ITEM + i, T_ITEM);
    for (int i = 0; i < N_DEF; i++) {
        BYTE *d = obj(O_DEF + i);
        vt(O_DEF + i, T_DEF);
        fl<DWORD>(d, 0x20) = g.next() % 50;
        fl<DWORD>(d, 0x2c) = g.next() % 8;
        fl<DWORD>(d, 0x4c) = g.fbits_(-10.0f, 10.0f);
        fl<DWORD>(d, 0x50) = g.next() % 3 ? g.next() % 6 : 0;
        fl<DWORD>(d, 0x54) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
    }
    vt(O_WORLD, T_WORLD);
    vt(O_APP, T_APP);
    fl<DWORD>(obj(O_APP), 0x2e0) = ptrv(obj(O_RULES_HOLDER));
    fl<DWORD>(obj(O_RULES_HOLDER), 0x38) = g.next() % 4 ? ptrv(obj(O_RULES)) : 0;
    vt(O_REPORTER, T_REPORTER);
    vt(O_GAMETYPE, T_GAMETYPE);
    vt(O_VIEW, T_VIEW);
    vt(O_LAYER, T_LAYER);
    for (int i = 0; i < N_AI; i++)
        vt(O_AI + i, T_AI);
    for (int i = 0; i < N_OTHER; i++)
        vt(O_OTHER + i, T_OTHER);
    for (int i = 0; i < N_ACTION; i++)
        vt(O_ACTION + i, T_ACTION);
    vt(O_NET, T_NET);
    fl<DWORD>(obj(O_NET), 0xc) = ptrv(obj(O_OPTIONS));
    vt(O_NETLAYER, T_NETLAYER);
    vt(O_PACKET, T_PACKET);
    BYTE *p = obj(O_PLAYER);
    fl<DWORD>(p, 0xc) = g.next() % 100;
    fl<DWORD>(p, 0x1c) = ptrv(obj(O_OPTIONS));
    fl<DWORD>(p, 0x24) = g_challenge_text;
    fl<BYTE>(p, 0x28) = (BYTE)(g.next() % 2);
    BYTE *o = obj(O_OPTIONS);
    fl<DWORD>(o, 0) = g_name_text;
    fl<DWORD>(o, 4) = g.next() % 4 ? g.next() % 300 : g.next();
    fl<DWORD>(o, 8) = g.next() % 4 ? g.next() % 300 : g.next();
    fl<BYTE>(o, 0xc) = (BYTE)(g.next() % 2);
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    g_case_globals[0][0] = ptrv(obj(O_WORLD));
    g_case_globals[1][0] = ptrv(obj(O_APP));
    g_case_globals[2][0] = ptrv(obj(O_REPORTER));
    g_case_globals[3][0] = g.next() % 2 ? 0 : 1u << (g.next() % 32);
    g_case_globals[4][0] = g.next() % 2 ? 0 : 1u << (g.next() % 32);
    g_case_globals[5][0] = g.next();
    g_case_globals[6][0] = g.next() % 2 ? 0 : ptrv(obj(O_OTHER));
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
    case CALL_FASTCALL5: return at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3], a[4]);
    case CALL_CDECL0: return at<DWORD(__cdecl *)()>(fn)();
    case CALL_CDECL4: return at<DWORD(__cdecl *)(DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3]);
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
DWORD g_last_eax;
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
            log("      seed %u side %d", seed, side);
        if (side == 0)
            hooks_out();
        s[side].eax = call_slot(st.address, obj(O_SELF), st.nargs, a);
        g_last_eax = s[side].eax;
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

}  // namespace zax::rw
