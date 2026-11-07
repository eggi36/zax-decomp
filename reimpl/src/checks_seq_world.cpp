// The fake world of the sequences batch's checks (see checks_seq_world.h).
#include "checks_seq_world.h"
#include "class_info.h"

namespace zax {
int hooks_sequences(Hook *out);
int hooks_sequence_player(Hook *out);
int hooks_normal_attack_ai(Hook *out);
int hooks_entity_om(Hook *out);
int hooks_say_quip(Hook *out);
int hooks_random_misc(Hook *out);
}  // namespace zax

namespace zax::sqw {

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

// ---- State ------------------------------------------------------------------------------------------------

Block g_block;
DWORD g_fvt[NTYPES][256];
int g_side;
unsigned g_vseed;
DWORD g_vindex;
bool g_trapped;
Policy g_policy;
Log g_log[2];
ZString g_texts[NTEXTS];
static Block g_block0;   // side 0's block for the report of a difference

void make_texts()
{
    static const char *const texts[NTEXTS] = {"", "Target", "Player", "Om", "Quips"};
    for (int i = 0; i < NTEXTS; i++)
        if (!g_texts[i].buffer)
            at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_texts[i], texts[i]);
}

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
static bool in_texts(DWORD v) { return v >= ptrv(g_texts) && v < ptrv(g_texts) + sizeof g_texts; }
// A value as both sides see it: fakes, the exe, small numbers as they are; the stack by a tag; other
// readable memory by its first dword
static DWORD rp(DWORD v)
{
    if (v < 0x10000 || in_block(v) || in_fvt(v) || in_texts(v) || (v >= 0x400000 && v < IMAGE_END))
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
static unsigned zstring_hash(DWORD z)
{
    if (!z || !readable(at<void *>(z), 4))
        return 0xbad0;
    const DWORD b = *at<DWORD *>(z);
    if (!b || !readable(at<void *>(b), 9))
        return 0xbad1;
    return text_hash(b + 8);
}
static DWORD dword_at(DWORD p, DWORD off) { return p && readable(at<void *>(p + off), 4) ? *at<DWORD *>(p + off) : 0xdeadbeefu; }

void log_value(DWORD v)
{
    const DWORD e[9] = {0x10000000u, v, 0, 0, 0, 0, 0, 0, 0};
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

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
    const char *regs;   // ecx, edx: p pointer, v value, z ZString text, f two floats behind it, - not used
    const char *args;   // p pointer, v value, b low byte, f two floats behind it, d the dword behind it, R the 16 bytes behind
                        // it, s string, z ZString text, m a message object, o / x not recorded
    char ret;           // v void (a junk value), i int, b bool (al), f st(0), p a fake of `type`, n the same or 0, c custom
    int type;
    bool caller_pops;
    Custom custom;
    bool optional;      // installed only for the cases whose policy names it
};

static const int TYPE_FIRST[NTYPES] = {O_ENTITY, O_BEHAVIOR, O_INVENTORY, O_ITEM, O_WORLD, O_APP, O_REPORTER, O_VIEW, O_AI, O_OTHER, O_LAYER,
                                       O_ACTION, O_SEQUENCE, O_LIST, O_DIRLIST, O_FRAME};
static const int TYPE_COUNT[NTYPES] = {N_ENTITY, N_BEHAVIOR, N_INVENTORY, N_ITEM, 1, 1, 1, 1, N_AI, N_OTHER, 1, N_ACTION, 1, 1, N_DIRLIST, 1};
static BYTE *pick(int type, bool may_be_null)
{
    const DWORD v = nv();
    if (may_be_null && (g_policy.always_null & (1u << type)))
        return nullptr;
    if (may_be_null && !(g_policy.nonnull & (1u << type)) && (v & 7) == 0)
        return nullptr;
    return obj(TYPE_FIRST[type] + (int)((v >> 3) % (DWORD)TYPE_COUNT[type]));
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
static const char *const NAMES[] = {"Quips/Hurt", "Hurt", "Sub/Taunt", ""};

static DWORD c_pos(Call &c) { return write_pos(c.a[0], 0.0f, 4000.0f); }
static DWORD c_center(Call &c) { return write_pos(c.a[0], -300.0f, 300.0f); }
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
static DWORD c_small(Call &) { return nv() % 5; }
static DWORD c_mostly_zero(Call &) { return nv() % 4 ? 0 : nv(); }
static DWORD c_sequence(Call &) { return ptrv(SEQUENCES[nv() % 5]); }
static DWORD c_name(Call &) { return ptrv(NAMES[nv() % 4]); }
static DWORD c_path(Call &) { return nv() % 4 == 0 ? 0 : ptrv(NAMES[nv() % 3]); }
static DWORD c_polygon(Call &) { return ptrv(obj(O_POLY + (int)(nv() % N_POLY))); }
static DWORD c_ticks(Call &) { return nv(); }
static DWORD c_this(Call &c) { return c.ecx; }
static DWORD c_writer(Call &) { return ptrv(obj(O_WRITER)); }
static DWORD c_voices(Call &) { return ptrv(obj(O_VOICES)); }
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    for (CClassInfo *k : g_policy.never)
        if (k && ptrv(k) == c.a[0])
            return v & 0xffffff00u;
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}
// 0x5aa4d0: the new camera tracker (a pool block of 0x20) becomes a fake AI; the block goes back
static DWORD c_tracker_ctor(Call &c)
{
    pool_free(at<void *>(c.ecx), 0x20);
    return ptrv(obj(O_AI + (int)(nv() % N_AI)));
}
// 0x545cc0: a polygon on the stack: zeros
static DWORD c_poly_ctor(Call &c)
{
    memset(at<void *>(c.ecx), 0, 0x28);
    return c.ecx;
}
// 0x59a470: the temporary part list is the fake O_PARTS
static DWORD c_temp_list(Call &c)
{
    *at<DWORD *>(c.ecx) = ptrv(obj(O_PARTS));
    return c.ecx;
}
// 0x48cb90: the destructable's collision sees the eye flag of the object under test
static DWORD c_om_collide(Call &)
{
    log_value(fl<BYTE>(obj(O_SELF), 0x164));
    return nv();
}
// A rectangle written (slot 36 of a frame: rect; slot 37 of an entity: rect, 3, 0)
static DWORD c_rect(Call &c)
{
    if (c.a[0] && readable(at<void *>(c.a[0]), 16)) {
        int *r = at<int *>(c.a[0]);
        r[0] = (int)(nv() % 400) - 100;
        r[1] = (int)(nv() % 400) - 100;
        r[2] = r[0] + (int)(nv() % 300);
        r[3] = r[1] + (int)(nv() % 300);
    }
    return nv();
}
// operator delete (optional): recorded, the block left alone
static DWORD c_delete(Call &) { return nv(); }
// operator new (optional): an extra object (big enough for the scene lists of the checks)
static DWORD c_new(Call &c) { return c.a[0] <= OBJ ? ptrv(extra(6)) : 0; }
// 0x415f50 (StringField_Value): one of the valid texts
static DWORD c_text(Call &) { return ptrv(&g_texts[nv() % NTEXTS]); }
// The objects a side made through the pool (handed to CMultipleActionsAction::Add), freed after the side
DWORD g_made[8];
int g_nmade;
static DWORD c_multi_add(Call &c)
{
    bool known = false;
    for (int i = 0; i < g_nmade && !known; i++)
        known = g_made[i] == c.ecx;
    if (!known && g_nmade < 8 && !in_block(c.ecx))
        g_made[g_nmade++] = c.ecx;
    return nv();
}
// slot 2 of a fake action: the class of a CMultipleActionsAction or of CAction
static DWORD c_action_class(Call &) { return ptrv(at<CClassInfo *(__cdecl *)()>(nv() % 2 ? 0x4b8460 : 0x41a650)()); }

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr, false}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn, false}
#define SCD(addr, args, fn) {addr, "--", args, 'c', 0, true, fn, false}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                   // Object_IsKindOf
    S(0x4f5d80, "p-", "", 'p', T_ENTITY),                      // World::FirstCharacter
    S(0x41ad40, "pz", "pppvvv", 'n', T_ENTITY),                // the part a name refers to
    S(0x41a720, "p-", "pppppp", 'b', 0),                       // an action's slot 23
    S(0x4fbc50, "p-", "p", 'v', 0),                            // CLayerPart_AddAI
    S(0x4e7330, "p-", "p", 'v', 0),                            // CLayerSaveData::MonitorAddPart
    S(0x4e73c0, "p-", "p", 'v', 0),                            // CLayerSaveData::MonitorRemove
    S(0x4fbd80, "p-", "p", 'v', 0),                            // CLayerPart_MarkDone
    SC(0x5aa4d0, "p-", "", c_tracker_ctor),                    // CCameraTrakerAI's constructor
    S(0x5a2b10, "p-", "pp", 'i', 0),                           // the application starts a sequence
    S(0x5a2b60, "p-", "p", 'i', 0),                            // the application ends a scene
    S(0x5a2b90, "p-", "p", 'i', 0),                            // the application ends the sequence
    S(0x418fc0, "p-", "p", 'b', 0),                            // FieldDesc_IsDynamicIn
    S(0x418790, "p-", "p", 'n', T_ACTION),                     // FieldDesc_DynamicPointer
    S(0x51e520, "p-", "v", 'i', 0),                            // the music mixer's track switch
    SC(0x4fb0a0, "p-", "o", c_pos),                            // LayerPart_Position
    SC(0x55c470, "p-", "v", c_random_below),                   // Random_Below
    SC(0x55c550, "p-", "vv", c_random_range),                  // a random float
    S(0x4303f0, "p-", "pfvv", 'b', 0),                         // CEntityBehaviorMoveToPos's aim
    S(0x48fb50, "p-", "v", 'v', 0),                            // EntityAnimated_SetSynch
    SC(0x48aee0, "p-", "", c_sequence),                        // Entity_SequenceName
    S(0x54dec0, "p-", "pv", 'v', 0),                           // the byte property set
    S(0x4fb2d0, "p-", "pvv", 'f', 0),                          // CLayerPart_AngleFromPart
    S(0x4267e0, "p-", "", 'n', T_INVENTORY),                   // LayerPart_GetInventory
    S(0x4fccc0, "pf", "vvp", 'i', 0),                          // an oval of the view
    S(0x48ec20, "p-", "", 'b', 0),                             // EntityAnimated_FrameIndexLocked
    S(0x48feb0, "p-", "v", 'v', 0),                            // EntityAnimated_SetFrame
    {0x4e9d80, "--", "", 'b', 0, true, nullptr, false},        // Layer_DrawDirect (cdecl)
    SCD(0x4d2650, "", c_ticks),                                // the tick count (cdecl)
    SC(0x48b860, "p-", "", c_polygon),                         // CEntityBase::Polygon
    S(0x545e80, "p-", "p", 'v', 0),                            // Polygon_Assign
    SC(0x546a70, "p-", "o", c_center),                         // Polygon_Center
    S(0x546580, "p-", "v", 'v', 0),                            // Polygon_SetFromBuffer
    S(0x545d50, "p-", "p", 'v', 0),                            // Polygon_SetFrom
    SC(0x545cc0, "p-", "", c_poly_ctor),                       // Polygon_Ctor
    S(0x545cf0, "p-", "", 'v', 0),                             // Polygon_Dtor
    S(0x546630, "p-", "vvv", 'v', 0),                          // Polygon_SetPoint
    S(0x48b9c0, "p-", "p", 'v', 0),                            // CEntityBase::SetPolygon
    SC(0x48cb90, "p-", "vpvv", c_om_collide),                  // EntityDestructable_OnCollide
    S(0x48fa00, "p-", "vv", 'v', 0),                           // CEntityAnimated::SetMovement
    SC(0x59a470, "p-", "", c_temp_list),                       // PartList_GetTemp
    S(0x59a510, "p-", "", 'i', 0),                             // PartList_ReleaseTemp
    S(0x4ed5e0, "p-", "pRvvvv", 'v', 0),                       // CLayer::FindRect
    S(0x4fc680, "p-", "p", 'b', 0),                            // CLayerPart_OverlapsPart
    S(0x5babc0, "pp", "", 'i', 0),                             // the player of two entities (fastcall)
    SCD(0x5bd300, "", c_writer),                               // the quip packet writer
    S(0x5bd560, "p-", "pvb", 'i', 0),                          // its write
    SC(0x591c90, "p-", "vvvvv", c_this),                       // RecordList_Ctor
    SC(0x468b30, "p-", "", c_this),                            // ResourceAliases::Ctor
    {0x5d0863, "--", "v", 'i', 0, true, nullptr, false},       // atexit
    SCD(0x595b50, "", c_voices),                               // the voice manager
    S(0x595be0, "p-", "svv", 'i', 0),                          // the voice manager's play
    S(0x595b30, "p-", "", 'i', 0),                             // the number of voices playing
    SC(0x4a32b0, "p-", "v", c_path),                           // DirList_NextPath
    S(0x4164c0, "zv", "vp", 'n', T_OTHER),                     // Object_CreateFromFile (fastcall)
    S(0x55b7d0, "p-", "s", 'i', 0),                            // CQuipCategory::ReadSounds
    S(0x591cd0, "p-", "vvvvv", 'v', 0),                        // RecordList_Set
    S(0x55d0c0, "p-", "", 'i', 0),                             // ResourceAliases_Dtor
    S(0x4d24c0, "ss", "", 'v', 0),                             // ShowErrorBox
    {0x5d0e36, "--", "v", 'v', 0, true, nullptr, false},       // exit
    {0x5d0320, "--", "p", 'c', 0, true, c_delete, true},       // operator delete (optional)
    {0x5d033a, "--", "v", 'c', 0, true, c_new, true},          // operator new (optional)
    S(0x418810, "p-", "p", 'n', T_ACTION),                     // FieldDesc_TakeDynamicPointer
    S(0x418fb0, "p-", "p", 'v', 0),                            // FieldDesc_RemoveDynamicFrom
    SC(0x415f50, "p-", "p", c_text),                           // StringField_Value
    S(0x54de90, "p-", "p", 'b', 0),                            // the byte property's value
    SC(0x4b8800, "p-", "p", c_multi_add),                      // CMultipleActionsAction::Add
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 100;
constexpr int NTHUNKS = VBASE + NTYPES * 256;
static_assert(NDIRECT <= VBASE);
static_assert(NTHUNKS == 4196, "the thunk count of sqw_thunks");

// The fakes' slots (type, slot, spec)
struct VSpec {
    int type, slot;
    Spec spec;
};
#define V(t, s, args, ret, rt) {t, s, {0, "p-", args, ret, rt, false, nullptr, false}}
#define VC(t, s, args, fn) {t, s, {0, "p-", args, 'c', 0, false, fn, false}}
static const VSpec VIRTUAL[] = {
    V(T_ENTITY, 1, "v", 'i', 0),
    V(T_ENTITY, 36, "", 'n', T_INVENTORY),
    VC(T_ENTITY, 37, "ovv", c_rect),
    V(T_ENTITY, 46, "", 'f', 0),
    V(T_ENTITY, 64, "pv", 'n', T_AI),
    V(T_ENTITY, 66, "pv", 'b', 0),
    V(T_ENTITY, 70, "", 'n', T_ENTITY),
    V(T_ENTITY, 84, "", 'f', 0),
    VC(T_ENTITY, 86, "p", c_mostly_zero),
    V(T_ENTITY, 102, "", 'b', 0),
    V(T_BEHAVIOR, 1, "v", 'i', 0),
    V(T_BEHAVIOR, 23, "p", 'v', 0),
    V(T_BEHAVIOR, 25, "vp", 'b', 0),
    V(T_BEHAVIOR, 28, "pp", 'v', 0),
    VC(T_INVENTORY, 25, "", c_small),
    V(T_INVENTORY, 26, "v", 'n', T_ITEM),
    V(T_INVENTORY, 27, "", 'n', T_ITEM),
    V(T_ITEM, 39, "", 'f', 0),
    V(T_ITEM, 50, "p", 'v', 0),
    V(T_WORLD, 1, "", 'p', T_OTHER),
    V(T_OTHER, 1, "v", 'i', 0),
    V(T_OTHER, 7, "v", 'i', 0),
    V(T_OTHER, 12, "", 'b', 0),
    V(T_REPORTER, 16, "s", 'v', 0),
    V(T_REPORTER, 17, "s", 'v', 0),
    V(T_AI, 1, "v", 'i', 0),
    V(T_AI, 19, "", 'v', 0),
    V(T_ACTION, 0, "p", 'v', 0),
    V(T_ACTION, 1, "v", 'i', 0),
    {T_ACTION, 2, {0, "p-", "", 'c', 0, false, c_action_class, false}},
    V(T_SEQUENCE, 23, "v", 'n', T_FRAME),
    V(T_SEQUENCE, 26, "", 'f', 0),
    VC(T_FRAME, 36, "o", c_rect),
    VC(T_LIST, 1, "v", c_name),
    VC(T_LIST, 5, "", c_name),
    V(T_LIST, 8, "v", 'b', 0),
    V(T_LIST, 15, "v", 'i', 0),
    VC(T_LIST, 22, "", c_small),
    V(T_LIST, 37, "ssp", 'n', T_OTHER),
    V(T_LIST, 39, "vp", 'i', 0),
    VC(T_DIRLIST, 22, "", c_small),
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
extern char sqw_thunks[];
DWORD sqw_pop, sqw_ret_float, sqw_edx;
long double sqw_fret;
DWORD __cdecl sqw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _sqw_thunks\n"
        "_sqw_thunks:\n"
        ".set sqw_i, 0\n"
        ".rept 4196\n"
        "  movl $sqw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _sqw_common - . - 4\n"
        ".set sqw_i, sqw_i + 1\n"
        ".endr\n"
        "_sqw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _sqw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _sqw_ret_float\n"
        "  je 1f\n"
        "  fldt _sqw_fret\n"
        "1:\n"
        "  movl _sqw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _sqw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(sqw_thunks) + 10u * (DWORD)i; }

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
        case 'z': return (DWORD)zstring_hash(v);
        case 'f': return dword_at(v, 0) * 31u + dword_at(v, 4);
        case 'd': return dword_at(v, 0);
        case 'R': return dword_at(v, 0) * 31u + dword_at(v, 4) * 7u + dword_at(v, 8) * 3u + dword_at(v, 12);
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

DWORD __cdecl sqw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            sqw_pop = 4;
            sqw_ret_float = 0;
            sqw_edx = 0;
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
    sqw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    sqw_ret_float = c.fset ? 1 : 0;
    sqw_fret = c.fret;
    sqw_edx = nv();
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
static const SlotTest *g_last_test;   // the test run last (run_slot) and when it began
static DWORD g_last_start;

bool world_on()
{
    stack_bounds();
    make_texts();
    for (int t = 0; t < NTYPES; t++)
        for (int s = 0; s < 256; s++)
            g_fvt[t][s] = thunk(VBASE + t * 256 + s);
    for (int k = 0; k < NTHUNKS; k += 997) {
        const BYTE *th = reinterpret_cast<const BYTE *>(sqw_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != (DWORD)k || th[5] != 0xe9) {
            log("  sequence checks: recorder thunk %d is broken", k);
            return false;
        }
    }
    DWORD old;
    if (!VirtualProtect(at<void *>(TEXT_FROM), TEXT_SIZE, PAGE_EXECUTE_READWRITE, &old))
        return false;
    g_text_protection = old;
    for (int i = 0; i < NDIRECT; i++)
        if (!DIRECT[i].optional)
            write_jump(g_patches[i], DIRECT[i].id, thunk(i));
    FlushInstructionCache(GetCurrentProcess(), at<void *>(TEXT_FROM), TEXT_SIZE);
    return true;
}
void world_off()
{
    if (g_last_test)
        log("      (%u ms)", GetTickCount() - g_last_start);
    g_last_test = nullptr;
    for (int i = NDIRECT; i--;)
        if (!DIRECT[i].optional)
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
static bool is_stand_in(DWORD a)
{
    for (const Spec &s : DIRECT)
        if (s.id == a)
            return true;
    return false;
}
DWORD g_modules = 0x3f;
void hooks_out()
{
    g_nsites = 0;
    int (*const tables[])(Hook *) = {hooks_sequences, hooks_sequence_player, hooks_normal_attack_ai, hooks_entity_om, hooks_say_quip,
                                     hooks_random_misc};
    const char *const names[] = {"sequences", "sequence_player", "normal_attack_ai", "entity_om", "say_quip", "random_misc"};
    for (int t = 0; t < 6; t++) {
        if (!(g_modules & (1u << t)))
            continue;
        const int n = tables[t](nullptr);
        if (!hook_sites_fit(names[t], n, MAX_SITES - g_nsites))
            continue;
        tables[t](g_hooks);
        for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
            BYTE *site = at<BYTE *>(g_hooks[i].address);
            if (!memcmp(site, g_hooks[i].expected, 5) || is_stand_in(g_hooks[i].address))
                continue;   // not hooked (Disable=), or a stand-in of this world
            SavedSite &s = g_sites[g_nsites++];
            s.site = site;
            memcpy(s.bytes, site, 5);
            memcpy(site, g_hooks[i].expected, 5);
            FlushInstructionCache(GetCurrentProcess(), site, 5);
        }
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
        FlushInstructionCache(GetCurrentProcess(), g_sites[i].site, 5);
    }
    g_nsites = 0;
}

// ---- The world's globals -----------------------------------------------------------------------------------

struct Global {
    DWORD address, size;
};
static const Global GLOBALS[] = {
    {0x6c2080, 4},     // the world
    {0x713f14, 4},     // the application
    {0x6c02cc, 4},     // the error reporter
    {0x711120, 0x40},  // the polygon point buffer
    {0x6bdd48, 1},     // g_NoCutting
    {0x7124b1, 1},     // the quip list's guard
    {0x712370, 0x140}, // the quip list
};
constexpr int NGLOBALS = sizeof GLOBALS / sizeof GLOBALS[0];
constexpr DWORD GMAX = 0x140;
static BYTE g_saved_globals[NGLOBALS][GMAX];
static BYTE g_case_globals[NGLOBALS][GMAX];   // the values of the case (set by build_world)
BYTE *case_global(DWORD address)
{
    for (int i = 0; i < NGLOBALS; i++)
        if (GLOBALS[i].address == address)
            return g_case_globals[i];
    return nullptr;
}
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
        fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        fl<DWORD>(e, 0xc8) = g.fbits_(0.0f, 6.3f);
    }
    for (int i = 0; i < N_BEHAVIOR; i++) {
        vt(O_BEHAVIOR + i, T_BEHAVIOR);
        fl<DWORD>(obj(O_BEHAVIOR + i), 0xa8) = g.next() % 4 ? g.next() & 0x7f : 0;
    }
    for (int i = 0; i < N_INVENTORY; i++)
        vt(O_INVENTORY + i, T_INVENTORY);
    for (int i = 0; i < N_ITEM; i++)
        vt(O_ITEM + i, T_ITEM);
    vt(O_WORLD, T_WORLD);
    fl<DWORD>(obj(O_WORLD), 0x334) = g.next() % 4 ? ptrv(obj(O_MIXER)) : 0;
    vt(O_APP, T_APP);
    vt(O_REPORTER, T_REPORTER);
    vt(O_VIEW, T_VIEW);
    vt(O_LAYER, T_LAYER);
    for (int i = 0; i < N_AI; i++) {
        vt(O_AI + i, T_AI);
        fl<BYTE>(obj(O_AI + i), 0x1c) = (BYTE)(g.next() % 3 != 0);
    }
    for (int i = 0; i < N_OTHER; i++)
        vt(O_OTHER + i, T_OTHER);
    for (int i = 0; i < N_ACTION; i++)
        vt(O_ACTION + i, T_ACTION);
    vt(O_SEQUENCE, T_SEQUENCE);
    vt(O_FRAME, T_FRAME);
    vt(O_LIST, T_LIST);
    for (int i = 0; i < N_DIRLIST; i++)
        vt(O_DIRLIST + i, T_DIRLIST);
    // the categories of the quip list (O_OTHER): a condition action or none, two timers, a sound list
    for (int i = 0; i < N_OTHER; i++) {
        BYTE *q = obj(O_OTHER + i);
        fl<DWORD>(q, 0x10) = g.next() % 3 ? 0 : ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION)));
        fl<DWORD>(q, 0x14) = g.next() % 3 ? 0 : g.fbits_(-1.0f, 3.0f);
        fl<DWORD>(q, 0x18) = g.next() % 3 ? 0 : g.fbits_(-1.0f, 3.0f);
        fl<DWORD>(q, 0x1c) = ptrv(obj(O_DIRLIST + (int)(g.next() % N_DIRLIST)));
    }
    // records of definition lists (4 of 0x80 bytes per object): the object a category or 0, now and then
    // loading; a name
    for (int r = 0; r < N_RECORDS * 4; r++) {
        BYTE *rec = obj(O_RECORDS) + r * 0x80;
        static const char *const names[] = {"Hurt", "Sub/Taunt", "", "Idle"};
        lstrcpyA(reinterpret_cast<char *>(rec), names[g.next() % 4]);
        fl<DWORD>(rec, 0x78) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
        rec[0x7c] = (BYTE)(g.next() % 2);
        rec[0x7d] = (BYTE)(g.next() % 2);
        rec[0x7f] = (BYTE)(g.next() % 8 == 0);
    }
    for (int i = 0; i < N_POLY; i++) {
        BYTE *q = obj(O_POLY + i);
        fl<DWORD>(q, 0) = ptrv(obj(O_POINTS + i));
        fl<DWORD>(q, 4) = 0;
        fl<DWORD>(q, 8) = 2 + g.next() % 7;
        for (DWORD k = 0; k < 32; k++)
            fl<DWORD>(obj(O_POINTS + i), k * 4) = g.fbits_(-500.0f, 500.0f);
    }
    // the temporary part list: vtable, data, count; the data an array of fakes
    BYTE *pl = obj(O_PARTS);
    fl<DWORD>(pl, 4) = ptrv(pl + 0x40);
    fl<DWORD>(pl, 8) = g.next() % 6;
    for (DWORD k = 0; k < 8; k++)
        fl<DWORD>(pl, 0x40 + k * 4) = ptrv(obj(O_ENTITY + (int)(g.next() % N_ENTITY)));
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    fl<DWORD>(g_case_globals[0], 0) = ptrv(obj(O_WORLD));
    fl<DWORD>(g_case_globals[1], 0) = ptrv(obj(O_APP));
    fl<DWORD>(g_case_globals[2], 0) = ptrv(obj(O_REPORTER));
    for (DWORD k = 0; k < 0x40; k += 4)
        fl<DWORD>(g_case_globals[3], k) = g.next();
    g_case_globals[4][0] = (BYTE)(g.next() % 3 == 0);
    g_case_globals[5][0] = 1;
    // the quip list: the fake list's vtable, its records, count and folder
    BYTE *ql = g_case_globals[6];
    fl<DWORD>(ql, 0) = ptrv(g_fvt[T_LIST]);
    fl<DWORD>(ql, 0x30) = g.next() % 8 ? ptrv(obj(O_RECORDS)) : 0;
    fl<DWORD>(ql, 0x34) = 1 + g.next() % 4;
    lstrcpyA(reinterpret_cast<char *>(ql + 0x38), g.next() % 2 ? "Quips" : "Data/Quips");
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
    t.check(same, "sequences", d);
    return same;
}

// ---- Running a slot ------------------------------------------------------------------------------------------

static WORD fpu_status()
{
    WORD sw;
    __asm__ volatile("fnstsw %0" : "=m"(sw));
    return sw;
}
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
    if (n & SLOT_FASTCALL) {
        switch (n & 0xff) {
        case 0: return at<DWORD(__fastcall *)(DWORD, DWORD)>(fn)(a[0], a[1]);
        case 1: return at<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2]);
        case 2: return at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3]);
        case 3: return at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3], a[4]);
        case 4: return at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3], a[4], a[5]);
        }
        return 0;
    }
    if (n & SLOT_CDECL) {
        switch (n & 0xff) {
        case 0: return at<DWORD(__cdecl *)()>(fn)();
        case 1: return at<DWORD(__cdecl *)(DWORD)>(fn)(a[0]);
        }
        return 0;
    }
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
void (*g_before_state)();
bool g_trace;
static void extras(const Policy &policy, bool on)
{
    for (DWORD r : policy.extra)
        for (int i = 0; r && i < NDIRECT; i++)
            if (DIRECT[i].id == r && DIRECT[i].optional) {
                if (on)
                    write_jump(g_patches[i], r, thunk(i));
                else
                    memcpy(g_patches[i].site, g_patches[i].saved, 5);
                FlushInstructionCache(GetCurrentProcess(), g_patches[i].site, 5);
            }
}
bool run_slot(Tally &t, const SlotTest &st, unsigned seed, SelfMaker make_self, const Policy &policy)
{
    if (&st != g_last_test) {   // one line per test (to find a crash), with the time of the one before
        const DWORD now = GetTickCount();
        if (g_last_test)
            log("      (%u ms)", now - g_last_start);
        g_last_test = &st;
        g_last_start = now;
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
        globals_on();
        const WORD before = fpu_status();
        if (g_trace)
            log("      seed %u side %d: %x %x %x %x %x", seed, side, a[0], a[1], a[2], a[3], a[4]);
        if (side == 0)
            hooks_out();
        extras(policy, true);
        const DWORD t0 = GetTickCount();
        s[side].eax = call_slot(st.address, obj(O_SELF), st.nargs, a);
        if (GetTickCount() - t0 > 100)   // a slow case (to find it)
            log("      slow: seed %u side %d %u ms: %x %x %x %x", seed, side, GetTickCount() - t0, a[0], a[1], a[2], a[3]);
        extras(policy, false);
        if (side == 0)
            hooks_in();
        const WORD after = fpu_status();
        if (g_before_state)
            g_before_state();
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

}  // namespace zax::sqw
