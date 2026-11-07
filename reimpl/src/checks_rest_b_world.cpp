// The fake world of the AI slot checks (see checks_rest_b_world.h).
#include "checks_rest_b_world.h"
#include "class_info.h"

namespace zax {
int hooks_objects_rest_b(Hook *out);
int hooks_inventory_rest_b(Hook *out);
}  // namespace zax

namespace zax::rbw {

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
                                       O_VIEW, O_AI, O_OTHER, O_LAYER, O_ACTION, O_NET, O_NETLAYER, O_PACKET, 0, O_SEQ, O_FRAME};
static const int TYPE_COUNT[NTYPES] = {N_ENTITY, N_BEHAVIOR, N_INVENTORY, N_ITEM, N_DEF, 1, 1, 1, 1, 1, N_AI, N_OTHER, 1, N_ACTION, 1, 1, 1, 0,
                                       N_SEQ, N_FRAME};
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
static const char *const SEQUENCE_NAMES[] = {"Open", "Closed", "open", "CLOSED", "Opening", "Closing", "", "Idle"};

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
static DWORD c_model(Call &) { return ptrv(obj(O_MODEL)); }
static DWORD c_seq_name(Call &) { return ptrv(SEQUENCE_NAMES[nv() % 8]); }
// 0x59a470: the temporary list (in the fake block) written into the slot
static DWORD c_temp_list(Call &c)
{
    if (c.ecx && readable(at<void *>(c.ecx), 4))
        *at<DWORD *>(c.ecx) = ptrv(obj(O_LIST));
    return c.ecx;
}
// 0x41a760: the names resolved into the list: 0-4 parts (entities, now and then the part itself)
static DWORD c_resolve(Call &c)
{
    BYTE *l = obj(O_LIST);
    DWORD *data = &fl<DWORD>(l, 0x40);
    const DWORD n = nv() % 5;
    for (DWORD i = 0; i < n; i++)
        data[i] = (nv() & 7) == 0 ? c.a[0] : ptrv(obj(O_ENTITY + (int)(nv() % N_ENTITY)));
    fl<DWORD>(l, 4) = ptrv(data);
    fl<DWORD>(l, 8) = n;
    return nv();
}
// 0x48d8f0: the model of an index: the second model for the first model's "Also Render", otherwise the
// first; now and then none
static DWORD c_def_object(Call &c)
{
    const DWORD v = nv();
    if ((v & 7) == 0)
        return 0;
    if (c.a[0] && c.a[0] == fl<DWORD>(obj(O_MODEL), 0xac))
        return ptrv(obj(O_MODEL2));
    return ptrv(obj(O_MODEL));
}
// 0x5173f0: a sequence of the model (never null: kind 5 reads it without a test, as the exe does)
static DWORD c_sequence(Call &) { return ptrv(obj(O_SEQ + (int)(nv() % N_SEQ))); }
// 0x517410: the next sequence to render with it, or none (half of the time)
static DWORD c_next_sequence(Call &) { return (nv() & 1) ? 0 : ptrv(obj(O_SEQ + (int)(nv() % N_SEQ))); }
static void write_rect(DWORD p)
{
    if (!p || !readable(at<void *>(p), 16))
        return;
    int *r = at<int *>(p);
    const int x = (int)(nv() % 600) - 100, y = (int)(nv() % 600) - 100;
    r[0] = x;
    r[1] = y;
    r[2] = x + (int)(nv() % 200);
    r[3] = y + (int)(nv() % 200);
}
static DWORD c_poly_rect(Call &c)
{
    write_rect(c.a[1]);
    return nv();
}
// 0x4faff0: the children's rectangles united into it (now and then)
static DWORD c_children_rect(Call &c)
{
    if ((nv() & 3) == 0)
        write_rect(c.a[0]);
    return nv();
}
// A frame's slot 36: its rectangle
static DWORD c_rect(Call &c)
{
    write_rect(c.a[0]);
    return nv();
}
// 0x508fe0 (fastcall out, out; angle): two values in [-1, 1]
static DWORD c_sin_cos(Call &c)
{
    if (c.ecx && readable(at<void *>(c.ecx), 4))
        *at<float *>(c.ecx) = frange(-1.0f, 1.0f);
    if (c.edx && readable(at<void *>(c.edx), 4))
        *at<float *>(c.edx) = frange(-1.0f, 1.0f);
    return nv();
}
// 0x599300 (message on the stack; entity, target): the message's five dwords
static DWORD c_message(Call &c)
{
    if (c.ecx && readable(at<void *>(c.ecx), 0x14)) {
        DWORD *m = at<DWORD *>(c.ecx);
        m[0] = 0x602c74;
        m[1] = nv();
        m[2] = 0;
        m[3] = rp(c.a[0]);
        m[4] = rp(c.a[1]);
    }
    return c.ecx;
}

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    SC(0x55c550, "p-", "vv", c_random_range),                // Random_Range
    S(0x41a720, "p-", "pppppp", 'b', 0),                     // CAction::Execute
    // objects_rest_b.cpp: the door
    SC(0x48aee0, "p-", "", c_seq_name),                      // Entity_SequenceName
    S(0x48b590, "p-", "", 'p', T_SEQ),                       // Entity_SequenceObject
    S(0x48feb0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrame
    S(0x560600, "p-", "vvvp", 'v', 0),                       // CRotationSequence::PlayTriggers
    S(0x48f700, "p-", "s", 'b', 0),                          // EntityAnimated_PlaySequence
    SC(0x48b290, "p-", "", c_model),                         // Entity_Model
    SC(0x59a470, "p-", "", c_temp_list),                     // PartList_GetTemp
    SC(0x41a760, "ps", "ppp", c_resolve),                    // Action_ResolveNames
    S(0x59a510, "p-", "", 'v', 0),                           // PartList_ReleaseTemp
    S(0x48d030, "p-", "", 'f', 0),                           // EntityDestructable_HitPointsLeft
    S(0x591fc0, "p-", "sv", 'i', 0),                         // NamedList_FindOrFail
    S(0x4fc8a0, "p-", "v", 'v', 0),                          // CLayerPart_LayerAdd
    S(0x48f600, "p-", "", 'f', 0),                           // EntityAnimated_FramePart
    S(0x48ffc0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrameFraction
    // objects_rest_b.cpp: the overlay walking reader's registration
    S(0x51ff30, "p-", "ps", 'i', 0),                         // NetPacket_RegisterReader
    // objects_rest_b.cpp: CEntityBase's rectangle
    SC(0x48d8f0, "p-", "v", c_def_object),                   // DefinitionList_Object
    SC(0x5173f0, "p-", "v", c_sequence),                     // CModel::Sequence
    SC(0x517410, "p-", "pv", c_next_sequence),               // CModel::ResolveSequence
    S(0x5173b0, "p-", "", 'i', 0),                           // CModel::DefaultSequenceIndex
    SC(0x55fee0, "p-", "vvpv", c_minus_one_often),           // CRotationSequence_FrameIndex
    SC(0x546ad0, "p-", "fp", c_poly_rect),                   // Polygon_Rect
    SC(0x4faff0, "p-", "pvt", c_children_rect),              // CLayerPart_ChildrenRect
    S(0x55e090, "p-", "v", 'v', 0),                          // DefinitionList_LoadRecord
    // inventory_rest_b.cpp: the weapon
    S(0x4fc6f0, "p-", "", 'n', T_ENTITY),                    // CLayerPart_Root
    S(0x4dd480, "p-", "", 'p', T_DEF),                       // ItemRef_Definition
    S(0x5176f0, "p-", "vppvv", 'p', T_ENTITY),               // Entity_CreateFromModel
    S(0x4d6540, "p-", "v", 'i', 0),                          // CInventoryItemDefinition::ShootSequence
    S(0x49b1a0, "p-", "fv", 'v', 0),                         // EntityCharacter_SetDischarge
    S(0x55fdd0, "p-", "v", 'i', 0),                          // CRotationSequence_AngleToRotation
    S(0x55fe90, "p-", "v", 'f', 0),                          // CRotationSequence_RotationToAngle
    SC(0x508fe0, "pp", "v", c_sin_cos),                      // Table_SinCos
    S(0x508da0, "--", "v", 'f', 0),                          // a table value of an angle (stdcall)
    S(0x508ec0, "--", "v", 'f', 0),                          // the other table value (stdcall)
    S(0x48b6b0, "p-", "v", 'v', 0),                          // Entity_SetRenderHeight
    S(0x4fb5e0, "p-", "f", 'v', 0),                          // CLayerPart_MoveCenterTo
    S(0x491230, "p-", "vv", 'v', 0),                         // EntityMovable_SetTarget
    S(0x4f61c0, "p-", "pvfvvv", 'v', 0),                     // World::BroadcastExplosionPoint
    S(0x4eb6f0, "p-", "p", 'v', 0),                          // CLayer::AddPart
    S(0x4eb200, "p-", "pfvvvvv", 'n', T_ENTITY),             // CLayer::FindInFront
    S(0x4fb2d0, "p-", "pvv", 'f', 0),                        // CLayerPart_AngleFromPart
    SC(0x599300, "p-", "pv", c_message),                     // CWeaponDischargedMessage::Ctor
    S(0x4fb910, "p-", "mvvvvv", 'i', 0),                     // CLayerPart_FindInRect
    S(0x477640, "pf", "pvvvpvv", 'v', 0),                    // DamageRadius_Query
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 160;
static_assert(NDIRECT <= VBASE);
static_assert(VBASE + NTYPES * 256 == 5280, "the thunk count of rbw_thunks");

// The fakes' slots (type, slot, spec)
struct VSpec {
    int type, slot;
    Spec spec;
};
#define V(t, s, args, ret, rt) {t, s, {0, "p-", args, ret, rt, false, nullptr}}
#define VC(t, s, args, fn) {t, s, {0, "p-", args, 'c', 0, false, fn}}
static const VSpec VIRTUAL[] = {
    V(T_ENTITY, 1, "v", 'i', 0),
    V(T_ENTITY, 6, "v", 'i', 0),
    V(T_ENTITY, 27, "p", 'i', 0),
    V(T_ENTITY, 35, "v", 'v', 0),
    VC(T_ENTITY, 43, "o", c_pos),
    V(T_ENTITY, 46, "", 'f', 0),
    V(T_ENTITY, 52, "", 'i', 0),
    V(T_ENTITY, 53, "", 'i', 0),
    V(T_ENTITY, 54, "", 'i', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    V(T_ENTITY, 70, "", 'n', T_ENTITY),
    V(T_ENTITY, 84, "", 'f', 0),
    V(T_ENTITY, 85, "", 'i', 0),
    V(T_ENTITY, 99, "vvp", 'v', 0),
    V(T_ENTITY, 104, "vv", 'v', 0),
    V(T_ENTITY, 107, "t", 'v', 0),
    VC(T_ENTITY, 117, "o", c_pos),
    V(T_ITEM, 23, "p", 'v', 0),
    V(T_DEF, 23, "v", 'f', 0),
    V(T_ACTION, 28, "ppvvpp", 'b', 0),
    V(T_ACTION, 29, "p", 'n', T_ITEM),
    V(T_REPORTER, 17, "s", 'v', 0),
    V(T_SEQ, 23, "v", 'n', T_FRAME),
    V(T_SEQ, 26, "", 'f', 0),
    V(T_FRAME, 31, "", 'i', 0),
    VC(T_FRAME, 36, "o", c_rect),
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
extern char rbw_thunks[];
DWORD rbw_pop, rbw_ret_float, rbw_edx;
long double rbw_fret;
DWORD __cdecl rbw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _rbw_thunks\n"
        "_rbw_thunks:\n"
        ".set rbw_i, 0\n"
        ".rept 5280\n"   // VBASE + NTYPES * 256
        "  movl $rbw_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _rbw_common - . - 4\n"
        ".set rbw_i, rbw_i + 1\n"
        ".endr\n"
        "_rbw_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _rbw_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _rbw_ret_float\n"
        "  je 1f\n"
        "  fldt _rbw_fret\n"
        "1:\n"
        "  movl _rbw_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _rbw_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(rbw_thunks) + 10u * (DWORD)i; }

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

DWORD __cdecl rbw_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            rbw_pop = 4;
            rbw_ret_float = 0;
            rbw_edx = 0;
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
    rbw_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    rbw_ret_float = c.fset ? 1 : 0;
    rbw_fret = c.fret;
    rbw_edx = nv();
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
        const BYTE *th = reinterpret_cast<const BYTE *>(rbw_thunks) + 10 * k;
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
    int (*const tables[])(Hook *) = {hooks_objects_rest_b, hooks_inventory_rest_b};
    const char *const names[] = {"objects_rest_b", "inventory_rest_b"};
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
    {0x6c29bc, 4}, {0x6c29a0, 4}, {0x6c29a8, 4},   // the category flags the weapon reads
    {0x6c29fc, 4},                                 // the light beam last made
    {0x6cfe08, 4}, {0x6cfe0c, 4},                  // the model list's records and their number (kind 4 of the rectangle)
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
        fl<DWORD>(e, 0x1c) = g.next() % 3 ? g.next() : 0;
        fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        fl<DWORD>(e, 0xc4) = g.fbits_(0.0f, 6.3f);
        fl<DWORD>(e, 0xc8) = g.fbits_(0.0f, 6.3f);
        fl<DWORD>(e, 0x100) = g.fbits_(-20.0f, 20.0f);
        fl<DWORD>(e, 0x104) = g.fbits_(-20.0f, 20.0f);
    }
    for (int i = 0; i < N_BEHAVIOR; i++)
        vt(O_BEHAVIOR + i, T_BEHAVIOR);
    for (int i = 0; i < N_INVENTORY; i++)
        vt(O_INVENTORY + i, T_INVENTORY);
    for (int i = 0; i < N_ITEM; i++)
        vt(O_ITEM + i, T_ITEM);
    // the shot entries of the definitions (0x14 bytes: +8 the spread, +0xc/+0x10 the offset)
    BYTE *shots = obj(O_SHOTS);
    for (DWORD k = 0; k < 4; k++) {
        fl<DWORD>(shots, k * 0x14 + 8) = g.fbits_(-30.0f, 30.0f);
        fl<DWORD>(shots, k * 0x14 + 0xc) = g.fbits_(-20.0f, 20.0f);
        fl<DWORD>(shots, k * 0x14 + 0x10) = g.fbits_(-20.0f, 20.0f);
    }
    for (int i = 0; i < N_DEF; i++) {
        BYTE *d = obj(O_DEF + i);
        vt(O_DEF + i, T_DEF);
        fl<DWORD>(d, 0x20) = g.next() % 50;
        fl<DWORD>(d, 0x24) = g.next() % 2 ? g.next() : 0;
        // the shot entries (the exe reads them without a test when there is a count)
        fl<DWORD>(d, 0x40) = g.next() % 4;
        fl<DWORD>(d, 0x44) = fl<DWORD>(d, 0x40) || g.next() % 2 ? ptrv(shots) : 0;
        fl<DWORD>(d, 0x54) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
    }
    vt(O_WORLD, T_WORLD);
    vt(O_APP, T_APP);
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
    vt(O_NETLAYER, T_NETLAYER);
    vt(O_PACKET, T_PACKET);
    // the sequences: +0x1c flags (0x20 shifted by the height, 0x40 y kept), +0xc..+0x18 a rectangle, +0x48
    // the frame count
    for (int i = 0; i < N_SEQ; i++) {
        BYTE *q = obj(O_SEQ + i);
        vt(O_SEQ + i, T_SEQ);
        fl<DWORD>(q, 0x1c) = (g.next() % 4) << 5 | (g.next() % 2 ? 0x100 : 0);
        const int x = (int)(g.next() % 400) - 200, y = (int)(g.next() % 400) - 200;
        fl<int>(q, 0xc) = x;
        fl<int>(q, 0x10) = y;
        fl<int>(q, 0x14) = x + (int)(g.next() % 300);
        fl<int>(q, 0x18) = y + (int)(g.next() % 300);
        fl<DWORD>(q, 0x48) = g.next() % 4 ? g.next() % 40 : 0;
    }
    for (int i = 0; i < N_FRAME; i++)
        vt(O_FRAME + i, T_FRAME);
    // the models: +0x2c "Closing Damage", +0x48 the sequence count, +0x60 "Max Velocity", +0x98/+0x9c the
    // hot spot, +0xa4/+0xa8 the sounds, +0xac "Also Render" (the second model's index or 0)
    for (int m = 0; m < 2; m++) {
        BYTE *o = obj(m ? O_MODEL2 : O_MODEL);
        fl<DWORD>(o, 0x2c) = g.next() % 3 ? g.fbits_(-2.0f, 20.0f) : (g.next() % 2 ? 0x41200000 : 0);
        fl<DWORD>(o, 0x48) = g.next() % 4;
        fl<DWORD>(o, 0x60) = g.fbits_(0.0f, 900.0f);
        fl<DWORD>(o, 0x98) = g.fbits_(-50.0f, 50.0f);
        fl<DWORD>(o, 0x9c) = g.fbits_(-50.0f, 50.0f);
        fl<DWORD>(o, 0xac) = m == 0 && g.next() % 3 == 0 ? 1 + g.next() % 7 : 0;
    }
    // the model list's records (0x80 bytes: +0x78 the model, +0x7f loading)
    BYTE *rec = obj(O_RECORDS);
    for (DWORD k = 0; k < 4; k++) {
        fl<DWORD>(rec, k * 0x80 + 0x78) = g.next() % 4 ? ptrv(obj(O_MODEL)) : 0;
        rec[k * 0x80 + 0x7f] = (BYTE)(g.next() % 5 == 0);
    }
    // the temporary list (filled by Action_ResolveNames)
    fl<DWORD>(obj(O_LIST), 8) = 0;
    // the globals of the case
    memset(g_case_globals, 0, sizeof g_case_globals);
    g_case_globals[0][0] = ptrv(obj(O_WORLD));
    g_case_globals[1][0] = ptrv(obj(O_APP));
    g_case_globals[2][0] = ptrv(obj(O_REPORTER));
    g_case_globals[3][0] = g.next() % 2 ? 0 : 1u << (g.next() % 32);
    g_case_globals[4][0] = g.next() % 2 ? 0 : 1u << (g.next() % 32);
    g_case_globals[5][0] = g.next();
    g_case_globals[6][0] = g.next() % 2 ? 0 : ptrv(obj(O_OTHER));
    g_case_globals[7][0] = g.next() % 6 ? ptrv(rec) : 0;
    g_case_globals[8][0] = g.next() % 5;
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
    using F7 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
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
    case 7: return at<F7>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
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

}  // namespace zax::rbw
