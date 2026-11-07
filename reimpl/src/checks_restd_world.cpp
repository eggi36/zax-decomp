// The fake world of the AI slot checks (see checks_restd_world.h).
#include "checks_restd_world.h"
#include "class_info.h"

namespace zax {
int hooks_objects_rest_d(Hook *out);
int hooks_ai_rest_d(Hook *out);
int hooks_game_rest_d(Hook *out);
}  // namespace zax

namespace zax::rdw {

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

// An extra entry of the side's log (what a check sees beyond the calls, e.g. a block's hash)
void log_event(DWORD id, DWORD a, DWORD b)
{
    DWORD e[LOGW] = {id, a, b};
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

// Pool blocks the stand-in constructors took over (an item, a control configuration): freed after the side
struct Made {
    void *p;
    DWORD size;
};
static Made g_made[8];
static int g_nmade;
static void free_made()
{
    for (int i = g_nmade; i--;)
        pool_get_free(g_made[i].p, g_made[i].size);   // not 0x54d130: a stand-in while the world is on
    g_nmade = 0;
}

static DWORD c_pos(Call &c) { return write_pos(c.a[0], 0.0f, 4000.0f); }
static DWORD c_find(Call &)
{
    const DWORD v = nv();
    if (g_policy.find_minus_one && v % (DWORD)g_policy.find_minus_one == 0)
        return 0xffffffffu;
    return (v >> 8) % 4;
}
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
static DWORD c_text(Call &) { return ptrv(TEXTS[nv() % 5]); }
static DWORD c_player(Call &) { return ptrv(obj(O_PLAYER)); }
// 0x59ff80: the game object's game type, the same for every call of a case
static DWORD c_game_type(Call &) { return (g_vseed * 2654435761u >> 28) % 4 == 0 ? 0 : ptrv(obj(O_GAMETYPE)); }
// 0x48b590: the entity's sequence (a fake with the frame count at +0x48)
static DWORD c_seq_object(Call &)
{
    BYTE *s = obj(O_OTHER + 1);
    const DWORD v = nv();
    fl<DWORD>(s, 0x48) = (v & 7) == 0 ? 0 : (v & 7) == 1 ? v : (v >> 4) % 40;
    return ptrv(s);
}
// 0x49c620 (part, out, sequence name, 0): an offset written to out
static DWORD c_offset(Call &c) { return write_pos(c.a[1], -50.0f, 50.0f); }
// 0x5205d0 (out, token table): a dword token
static DWORD c_read_dword(Call &c)
{
    const DWORD v = nv();
    if (c.a[0])
        *at<DWORD *>(c.a[0]) = (v & 3) == 0 ? v : (v >> 4) % 6;
    return nv();
}
// gcd_compute_response (cd key, challenge, out): a response text written to out (the caller's 0x48 bytes)
static DWORD c_response(Call &c)
{
    const char *t = TEXTS[nv() % 5];
    char *out = at<char *>(c.a[2]);
    DWORD k = 0;
    for (; t[k] && k < 0x3f; k++)
        out[k] = t[k];
    out[k] = 0;
    return nv();
}
// The constructors of a pool block (CInventoryItem 0x42a2b0, ControlConfig 0x46ef80): the block gets the
// vtable of a fake type and is freed after the side
static DWORD take_block(DWORD ecx, int type, DWORD size)
{
    if (ecx) {
        fl<DWORD>(at<void *>(ecx), 0) = ptrv(g_fvt[type]);
        if (g_nmade < 8)
            g_made[g_nmade++] = {at<void *>(ecx), size};
    }
    return ecx;
}
static DWORD c_item_ctor(Call &c) { return take_block(c.ecx, T_ITEM, 0x18); }
static DWORD c_config_ctor(Call &c) { return take_block(c.ecx, T_OTHER, 0x1c8); }
// DefinitionList_LoadRecord on the game type list (index): the fake record (the records 0x6c0318 point into the
// fakes during a case) gets the game type fake
static DWORD c_load_record(Call &c)
{
    BYTE *records = *at<BYTE **>(0x6c0318);
    if (records && c.a[0] < 4)
        fl<DWORD>(records, c.a[0] * 0x80 + 0x78) = ptrv(obj(O_GAMETYPE));   // the original calls it without a test
    return nv();
}
// The net layer's slot 11: the number of players (0 to 3)
static DWORD c_count(Call &) { return nv() % 4; }
// The game object's slot 3 (seconds) while it fast-forwards a sequence: one time in four the sequence
// ends (the "playing" byte +0x589 of the object under test cleared)
static DWORD c_tick(Call &)
{
    if ((nv() & 3) == 0)
        fl<BYTE>(g_self, 0x589) = 0;
    return nv();
}
// A slot that writes a value through its last pointer argument (a record's icon, slot 25)
static DWORD c_out4(Call &c)
{
    const DWORD v = nv();
    if (c.a[3])
        *at<DWORD *>(c.a[3]) = v % 5 ? (v >> 4) % 200 : v;
    return (v & 0xffffff00u) | ((v >> 9) & 1);
}

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    SC(0x523df0, "p-", "sv", c_find),                        // ResourceList_Find
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    S(0x41a740, "p-", "ppppp", 'b', 0),                      // an action's slot 23 from a caller
    // objects_rest_d.cpp
    S(0x4fc200, "p-", "pvs", 'i', 0),                        // LayerPart_RemoveListener
    S(0x4fc1b0, "p-", "pvs", 'i', 0),                        // LayerPart_AddListener
    S(0x46cd50, "p-", "ps", 'i', 0),                         // CObject_OnMessage
    {0x5d0320, "--", "p", 'v', 0, true, nullptr},            // operator delete (cdecl)
    S(0x4173f0, "p-", "", 'v', 0),                           // Object_FreeFields
    S(0x4166d0, "p-", "", 'v', 0),                           // CObject_Dtor
    S(0x54d130, "pv", "", 'v', 0),                           // pool_delete
    S(0x51ff30, "p-", "ps", 'i', 0),                         // NetPacket_RegisterReader
    S(0x51ff50, "p-", "pv", 'i', 0),                         // NetPacket_WriteBlockType
    S(0x520940, "p-", "v", 'i', 0),                          // NetPacket_WriteDword
    S(0x5209c0, "p-", "w", 'i', 0),                          // NetPacket_WriteWord
    S(0x4e79b0, "p-", "pvp", 'i', 0),                        // CLayerSaveData::WriteRect
    S(0x5203b0, "p-", "", 'i', 0),                           // NetPacket_Length
    S(0x520f10, "p-", "vv", 'i', 0),                         // NetPacket_PatchDword
    // ai_rest_d.cpp
    S(0x48b120, "p-", "v", 'v', 0),                          // Entity_SetSequence
    SC(0x48aee0, "p-", "", c_text),                          // Entity_SequenceName
    S(0x48f700, "p-", "s", 'b', 0),                          // EntityAnimated_PlaySequence
    SC(0x48b590, "p-", "", c_seq_object),                    // Entity_SequenceObject
    S(0x560300, "p-", "vvp", 'v', 0),                        // CRotationSequence::PlayForward
    S(0x48feb0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrame
    S(0x508da0, "--", "v", 'f', 0),                          // Table_Cos (stdcall)
    // game_rest_d.cpp
    S(0x4fc6f0, "p-", "", 'n', T_ENTITY),                    // CLayerPart_Root
    S(0x48d150, "p-", "", 'f', 0),                           // EntityDestructable_HitPoints
    S(0x48d030, "p-", "", 'f', 0),                           // EntityDestructable_HitPointsLeft
    S(0x4267e0, "p-", "", 'n', T_INVENTORY),                 // LayerPart_GetInventory
    S(0x5b7640, "p-", "", 'b', 0),                           // CZaxInventoryItemDefinition::IsContinuousFire
    S(0x54de90, "p-", "p", 'b', 0),                          // the continuous fire list: part in it
    S(0x54dec0, "p-", "pv", 'v', 0),                         // the continuous fire list: part set
    SC(0x49c620, "p-", "posv", c_offset),                    // CPositionedAction::Offset
    S(0x42e820, "v-", "v", 'f', 0),                          // Angle_ToOval
    S(0x4fc240, "p-", "s", 'v', 0),                          // LayerPart_Broadcast
    S(0x5b7920, "p-", "p", 'n', T_OTHER),                    // CZaxInventoryItemDefinition::EnergySource
    S(0x4d77c0, "p-", "", 'b', 0),                           // CInventoryItemDefinition::HasAmmunition
    S(0x5b0da0, "p-", "", 'n', T_OTHER),                     // Inventory_Battery
    S(0x426930, "pv", "v", 'v', 0),                          // Part_UseItems
    S(0x4a2650, "s-", "", 'v', 0),                           // DebugLog_SectionBegin
    S(0x4a2670, "--", "", 'v', 0),                           // DebugLog_SectionEnd
    S(0x4f5d60, "p-", "v", 'n', T_ENTITY),                   // World::Character
    S(0x48d8f0, "p-", "v", 'n', T_DEF),                      // DefinitionList_Object
    S(0x416760, "p-", "vv", 'n', T_ITEM),                    // Object_Clone
    SC(0x42a2b0, "--", "", c_item_ctor),                     // CInventoryItem::CInventoryItem (a pool block: not recorded)
    S(0x54fdb0, "p-", "v", 'v', 0),                          // an item's catalogue index set
    SC(0x425900, "p-", "p", c_minus_one_often),              // CInventory_IndexOfDefinition
    SC(0x5bac80, "v-", "", c_player),                        // ZaxNet_PlayerOf
    S(0x4dd480, "p-", "", 'n', T_DEF),                       // ItemRef_Definition
    S(0x5ba450, "p-", "v", 'v', 0),                          // ZaxNetPlayer::SetRecordCount
    S(0x482de0, "p-", "", 'i', 0),                           // DynamicProperties_Count
    S(0x5ba100, "p-", "v", 'v', 0),                          // ZaxNetRecord::SetIndex
    S(0x5ba120, "p-", "v", 'v', 0),                          // ZaxNetRecord::SetIcon
    S(0x59b260, "p-", "sv", 'v', 0),                         // NetGame_SendChatToAll
    S(0x4f3ac0, "p-", "p", 'v', 0),                          // World::CurrentLevelPath
    S(0x5bcf60, "p-", "pp", 'v', 0),                         // StartGameInfoReader::Write
    SC(0x59ff80, "p-", "", c_game_type),                     // the game object's game type
    SC(0x5205d0, "p-", "op", c_read_dword),                  // NetPacket_ReadDwordToken
    SC(0x5206c0, "p-", "", c_text),                          // NetPacket_ReadText
    S(0x46b4e0, "p-", "", 'v', 0),                           // CdKey_Get (the string stays empty)
    {0x5c9960, "--", "sso", 'c', 0, true, c_response},       // gcd_compute_response (cdecl)
    S(0x5ac180, "p-", "vs", 'v', 0),                         // CdKeyResponseReader::Write
    S(0x4f3cc0, "p-", "", 'v', 0),                           // World::BeginLoading
    S(0x5a32b0, "p-", "v", 'v', 0),                          // CZaxWorld::SetGameType
    S(0x4f3d30, "p-", "", 'v', 0),                           // World::EndLoading
    S(0x5ac330, "p-", "v", 'v', 0),                          // BlockReader_Send
    SC(0x46ef80, "--", "vvvv", c_config_ctor),               // ControlConfig::Ctor (a pool block: not recorded)
    S(0x46f430, "p-", "vssvvvv", 'v', 0),                    // ControlConfig::SetBinding
    S(0x46f4c0, "p-", "vvvvv", 'v', 0),                      // KeyBinding_SetDefault
    S(0x46f480, "p-", "vs", 'v', 0),                         // ControlConfig::SetItem
    S(0x46f980, "p-", "", 'v', 0),                           // ControlConfig::SaveCopy
    S(0x46fcc0, "p-", "v", 'v', 0),                          // ControlConfig::ReadIni
    S(0x46f3e0, "p-", "v", 'v', 0),                          // ControlConfig::ForbidKey
    S(0x5a0c00, "p-", "pvv", 'v', 0),                        // the status text (drawing)
    S(0x4f1050, "p-", "", 'n', T_LAYER),                     // World::CurrentLayer
    S(0x522110, "p-", "p", 'v', 0),                          // CBeginNonInteractiveSequenceAction::EndScene
    S(0x4f5d80, "p-", "", 'n', T_ENTITY),                    // World::FirstCharacter
    S(0x4f31c0, "p-", "v", 'b', 0),                          // World::KeyEscape
    S(0x590f00, "--", "", 'v', 0),                           // the display mode set
    SC(0x591fc0, "p-", "sv", c_find),                        // NamedList_FindOrFail
    S(0x5bb780, "p-", "v", 'v', 0),                          // CZaxSaveData::SetDifficulty
    SC(0x55e090, "p-", "v", c_load_record),                  // DefinitionList_LoadRecord
    S(0x404180, "p-", "v", 'v', 0),                          // a window closed
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 160;
static_assert(NDIRECT <= VBASE);
static_assert(VBASE + NTYPES * 256 == 4768, "the thunk count of rdw_thunks");

// The fakes' slots (type, slot, spec)
struct VSpec {
    int type, slot;
    Spec spec;
};
#define V(t, s, args, ret, rt) {t, s, {0, "p-", args, ret, rt, false, nullptr}}
#define VC(t, s, args, fn) {t, s, {0, "p-", args, 'c', 0, false, fn}}
static const VSpec VIRTUAL[] = {
    V(T_ENTITY, 41, "vv", 'v', 0),
    VC(T_ENTITY, 43, "o", c_pos),
    V(T_ENTITY, 46, "", 'f', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 84, "", 'f', 0),
    V(T_ENTITY, 104, "vv", 'v', 0),
    V(T_ITEM, 1, "v", 'i', 0),
    V(T_ITEM, 7, "", 'v', 0),
    V(T_ITEM, 19, "", 'v', 0),
    V(T_ITEM, 32, "p", 'b', 0),
    V(T_ITEM, 33, "vp", 'v', 0),
    V(T_ITEM, 49, "pv", 'v', 0),
    V(T_ITEM, 50, "v", 'v', 0),
    V(T_DEF, 32, "", 'n', T_OTHER),
    V(T_OTHER, 2, "d", 'i', 0),
    V(T_OTHER, 5, "", 'v', 0),
    V(T_OTHER, 13, "", 'b', 0),
    VC(T_OTHER, 25, "vpvp", c_out4),
    V(T_OTHER, 26, "", 'f', 0),
    V(T_OTHER, 28, "p", 'v', 0),
    V(T_OTHER, 41, "vv", 'v', 0),
    V(T_OTHER, 50, "v", 'v', 0),
    V(T_OTHER, 73, "", 'b', 0),
    V(T_REPORTER, 1, "", 'p', T_VIEW),
    V(T_REPORTER, 16, "s", 'v', 0),
    V(T_REPORTER, 17, "s", 'v', 0),
    V(T_VIEW, 2, "vp", 'n', T_OTHER),
    V(T_VIEW, 3, "vvv", 'n', T_OTHER),
    V(T_GAMETYPE, 23, "", 'v', 0),
    VC(T_WORLD, 3, "v", c_tick),
    V(T_WORLD, 16, "s", 'v', 0),
    V(T_WORLD, 17, "s", 'v', 0),
    V(T_WORLD, 44, "", 'p', T_NETLAYER),
    V(T_WORLD, 58, "", 'p', T_OTHER),
    V(T_WORLD, 79, "vsvv", 'v', 0),
    V(T_WORLD, 80, "", 'v', 0),
    V(T_WORLD, 84, "", 'b', 0),
    V(T_WORLD, 109, "s", 'v', 0),
    V(T_WORLD, 110, "s", 'b', 0),
    VC(T_NETLAYER, 11, "", c_count),
    V(T_NETLAYER, 12, "v", 'i', 0),
    V(T_NETLAYER, 24, "", 'v', 0),
    V(T_GAMETYPE, 28, "p", 'v', 0),
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
extern char rdw_thunks[];
DWORD rdw_pop, rdw_ret_float, rdw_edx;
long double rdw_fret;
DWORD __cdecl rdw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _rdw_thunks\n"
        "_rdw_thunks:\n"
        ".set rdw_i, 0\n"
        ".rept 4768\n"   // VBASE + NTYPES * 256
        "  movl $rdw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _rdw_common - . - 4\n"
        ".set rdw_i, rdw_i + 1\n"
        ".endr\n"
        "_rdw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _rdw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _rdw_ret_float\n"
        "  je 1f\n"
        "  fldt _rdw_fret\n"
        "1:\n"
        "  movl _rdw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _rdw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(rdw_thunks) + 10u * (DWORD)i; }

static void record(DWORD id, const Spec &s, Call &c)
{
    DWORD e[LOGW] = {id};
    int k = 1;
    auto value = [&](char kind, DWORD v) -> DWORD {
        switch (kind) {
        case 'p': return rp(v);
        case 'v': return v;
        case 'b': return v & 0xff;
        case 'w': return v & 0xffff;
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
    for (int i = 0; s.args[i] && k < LOGW; i++)
        e[k++] = value(s.args[i], c.a[i]);
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

DWORD __cdecl rdw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            const DWORD e[LOGW] = {0xdead0000u, id, rp(ecx)};
            l.hash = fnv(l.hash, e, sizeof e);
            if (l.calls < LOGN)
                memcpy(l.ev[l.calls], e, sizeof e);
            l.calls++;
            rdw_pop = 4;
            rdw_ret_float = 0;
            rdw_edx = 0;
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
    rdw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    rdw_ret_float = c.fset ? 1 : 0;
    rdw_fret = c.fret;
    rdw_edx = nv();
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
        const BYTE *th = reinterpret_cast<const BYTE *>(rdw_thunks) + 10 * k;
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
    int (*const tables[])(Hook *) = {hooks_objects_rest_d, hooks_ai_rest_d, hooks_game_rest_d};
    const char *const names[] = {"objects_rest_d", "ai_rest_d", "game_rest_d"};
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
    {0x6c29a0, 4},   // the cheat flags the weapon action tests against a part's flags
    {0x6c1d0c, 4},   // the counter of sequence skips (the game object's keys)
    {0x6c1d08, 1},   // the layer block's "rectangles too" byte, Layer_SetDrawDirect's
    {0x6c0318, 4}, {0x6c031c, 4},   // the game type list's records and their count
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

DWORD static_text(int i) { return i ? g_challenge_text : g_name_text; }

void build_world(unsigned seed)
{
    Gen g{seed * 2654435761u + 77};
    DWORD *all = reinterpret_cast<DWORD *>(&g_block);
    for (DWORD i = 0; i < sizeof g_block / 4; i++)
        all[i] = g.next();
    auto vt = [](int o, int type) { fl<DWORD>(obj(o), 0) = ptrv(g_fvt[type]); };
    auto any = [&g](int first, int count) { return ptrv(obj(first + (int)(g.next() % (DWORD)count))); };
    for (int i = 0; i < N_ENTITY; i++) {
        BYTE *e = obj(O_ENTITY + i);
        vt(O_ENTITY + i, T_ENTITY);
        fl<DWORD>(e, 0x1c) = g.next() % 3 ? g.next() : 0;
        fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        fl<DWORD>(e, 0xc4) = g.fbits_(0.0f, 6.3f);
    }
    for (int i = 0; i < N_BEHAVIOR; i++)
        vt(O_BEHAVIOR + i, T_BEHAVIOR);
    for (int i = 0; i < N_INVENTORY; i++) {
        BYTE *v = obj(O_INVENTORY + i);
        vt(O_INVENTORY + i, T_INVENTORY);
        fl<DWORD>(v, 0xc) = g.next() % 8;
        fl<DWORD>(v, 0x20) = g.next() % 4;
    }
    for (int i = 0; i < N_ITEM; i++) {
        BYTE *it = obj(O_ITEM + i);
        vt(O_ITEM + i, T_ITEM);
        fl<DWORD>(it, 0x18) = g.next() % 4 ? any(O_ENTITY, N_ENTITY) : 0;
    }
    for (int i = 0; i < N_DEF; i++) {
        BYTE *d = obj(O_DEF + i);
        vt(O_DEF + i, T_DEF);
        fl<DWORD>(d, 0x10) = g.fbits_(-1.0f, 30.0f);
        fl<DWORD>(d, 0x18) = g.next() % 6;
        fl<DWORD>(d, 0x1c) = g.next() % 3 ? 0 : any(O_ITEM, N_ITEM);
        fl<DWORD>(d, 0x24) = g.next() % 4 ? any(O_OTHER, N_OTHER) : 0;
        fl<DWORD>(d, 0x2c) = g.next() % 8;
        fl<DWORD>(d, 0x4c) = g.fbits_(-10.0f, 10.0f);
        fl<DWORD>(d, 0x50) = g.next() % 3 ? g.next() % 6 : 0;
    }
    vt(O_WORLD, T_WORLD);
    vt(O_APP, T_APP);
    // the application's player list (a CCharacterListHolder at +0x284): its array's vtable a fake one
    fl<DWORD>(obj(O_APP), 0x284) = 0x5f8ef0;
    fl<DWORD>(obj(O_APP), 0x28c) = ptrv(g_fvt[T_OTHER]);
    vt(O_REPORTER, T_REPORTER);
    // the reporter's +0x34 (0x4cdf30): an object with a chain of fakes from +0xec (next +0xf4, end +0xf0)
    BYTE *chain = obj(O_OTHER + 3);
    fl<DWORD>(obj(O_REPORTER), 0x34) = ptrv(chain);
    fl<DWORD>(chain, 0xec) = g.next() % 5 ? ptrv(obj(O_OTHER)) : 0;
    fl<DWORD>(chain, 0xf0) = ptrv(obj(O_OTHER + (int)(g.next() % 3)));
    for (int i = 0; i < 3; i++)
        fl<DWORD>(obj(O_OTHER + i), 0xf4) = g.next() % 4 ? ptrv(obj(O_OTHER + i + 1)) : 0;
    vt(O_GAMETYPE, T_GAMETYPE);
    vt(O_VIEW, T_VIEW);
    vt(O_LAYER, T_LAYER);
    for (int i = 0; i < N_AI; i++)
        vt(O_AI + i, T_AI);
    for (int i = 0; i < N_OTHER; i++) {
        vt(O_OTHER + i, T_OTHER);
        fl<DWORD>(obj(O_OTHER + i), 0x18) = g.fbits_(-2.0f, 50.0f);
    }
    for (int i = 0; i < N_ACTION; i++)
        vt(O_ACTION + i, T_ACTION);
    vt(O_NET, T_NET);
    vt(O_NETLAYER, T_NETLAYER);
    vt(O_PACKET, T_PACKET);
    // the net player: its records (+0x100 data, +0x104 count, +0x114 the next one), name +0x1c
    BYTE *p = obj(O_PLAYER);
    BYTE *records = obj(O_SPARE + N_SPARE - 1);
    for (int i = 0; i < 16; i++)
        fl<DWORD>(records, 4 * (DWORD)i) = ptrv(obj(O_OTHER + i % N_OTHER));
    fl<DWORD>(p, 0x1c) = ptrv(obj(O_OPTIONS));   // the player's options: +0 the name (a ZString)
    fl<DWORD>(obj(O_OPTIONS), 0) = g_name_text;
    fl<DWORD>(p, 0x100) = ptrv(records);
    fl<DWORD>(p, 0x104) = g.next() % 12;
    fl<DWORD>(p, 0x114) = g.next() % 12;
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    g_case_globals[0][0] = ptrv(obj(O_WORLD));
    g_case_globals[1][0] = ptrv(obj(O_APP));
    g_case_globals[2][0] = ptrv(obj(O_REPORTER));
    g_case_globals[3][0] = g.next() % 2 ? 0 : 1u << (g.next() % 32);
    g_case_globals[4][0] = g.next() % 1000;
    g_case_globals[5][0] = g.next() % 2;
    // the game type list's records: four fakes, loaded or not (never "loading", and the list never without
    // records: the game start calls the game type's slot 23 without a test)
    BYTE *types = obj(O_SPARE + 7);
    for (DWORD i = 0; i < 4; i++) {
        fl<DWORD>(types, i * 0x80 + 0x78) = g.next() % 3 ? ptrv(obj(O_GAMETYPE)) : 0;
        fl<BYTE>(types, i * 0x80 + 0x7f) = 0;
    }
    g_case_globals[6][0] = ptrv(types);
    g_case_globals[7][0] = 4;
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
                log("    call %d: %x %x %x | %x %x %x %x %x %x %x %x / %x %x %x | %x %x %x %x %x %x %x %x", i, a[0], a[1], a[2], a[3], a[4], a[5],
                    a[6], a[7], a[8], a[9], a[10], b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10]);
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

BYTE *g_self;
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
        g_self = obj(O_SELF);
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
        s[side].eax = call_slot(st.address, g_self, st.nargs, a);
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
        free_made();
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

}  // namespace zax::rdw
