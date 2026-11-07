// The fake world of the AI slot checks (see checks_resta_world.h).
#include "checks_resta_world.h"
#include "class_info.h"

namespace zax {
int hooks_ai_rest_a(Hook *out);
int hooks_input_rest_a(Hook *out);
int hooks_dialog_rest_a(Hook *out);
}  // namespace zax

namespace zax::ra {

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
    DWORD e[9] = {id, a, b, 0, 0, 0, 0, 0, 0};
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

static DWORD c_pos(Call &c) { return write_pos(c.a[0], 0.0f, 4000.0f); }
static DWORD c_is_kind_of(Call &c)
{
    const DWORD v = nv();
    for (CClassInfo *k : g_policy.always)
        if (k && ptrv(k) == c.a[0])
            return (v & 0xffffff00u) | 1;
    return (v & 0xffffff00u) | ((v >> 7) & 1);
}
// A point of the fake block: two floats (slot 72) or three dwords (slot 114)
static DWORD c_point(Call &)
{
    BYTE *p = obj(O_POINT);
    write_pos(ptrv(p), -100.0f, 4000.0f);
    fl<DWORD>(p, 8) = nv();
    return ptrv(p);
}
static DWORD c_text(Call &) { return ptrv(TEXTS[nv() % 5]); }
// A finite float in st(0) (frames, frame rates: the update's frame loop must end)
static DWORD c_frame(Call &c)
{
    c.fret = (long double)(float)((nv() >> 8) % 4000) * 0.01L;
    c.fset = true;
    return nv();
}
static DWORD c_fps(Call &c)
{
    const DWORD v = nv();
    c.fret = (v & 7) == 0 ? 0.0L : (v & 7) == 1 ? -12.5L : (long double)(float)((v >> 8) % 3000) * 0.01L;
    c.fset = true;
    return nv();
}
// CWayPointPath::NextPoint (out, mover): sometimes a point
static DWORD c_next_point(Call &c)
{
    const DWORD v = nv();
    if (v & 1)
        write_pos(c.a[0], -100.0f, 4000.0f);
    return (v & 0xffffff00u) | ((v >> 1) & 1);
}
// CRotationSequence::MotionBetween (out, ...): a motion
static DWORD c_motion(Call &c)
{
    write_pos(c.a[0], -20.0f, 20.0f);
    return c.a[0];
}
// The sequence object of an entity: the fake sequence or none
static DWORD c_sequence(Call &) { return nv() % 6 == 0 ? 0 : ptrv(obj(O_SEQ)); }
static DWORD c_this(Call &c) { return c.ecx; }
static const char *const JOYSTICKS[] = {"Gamepad Pro", "NO JOYSTICK", "", "Gamepad Pro "};
static DWORD c_joy_name(Call &) { return ptrv(JOYSTICKS[nv() % 4]); }
static DWORD c_time(Call &) { return 1000 + nv() % 4000; }
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
// The mouse position (two ints) around the middle of the 640 x 480 screen
static DWORD c_mouse_pos(Call &c)
{
    for (int i = 0; i < 2; i++)
        if (c.a[i] && readable(at<void *>(c.a[i]), 4))
            *at<int *>(c.a[i]) = (int)(nv() % 900) - 100;
    return nv();
}
// CJoystick::BitsText (bits, out ZString): a text into the string (the real assignment)
static DWORD c_bits_text(Call &c)
{
    if (c.a[1])
        at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(at<void *>(c.a[1]), TEXTS[nv() % 5]);
    return nv();
}
// CJoystick::Ctor (id) on a new block: the id, an empty name
// The dialogue scripts: a fake script (its lines at +0x100, pointed to by +8) for DefinitionList_Object and
// the list's slot 37
static DWORD c_script(Call &) { return ptrv(obj(O_SCRIPT + (int)(nv() % 2))); }
static const char *const SCRIPT_NAMES[] = {"Mission01", "ACT 1/M1M2/Mission01n2", "a/b", "/x", ""};
static DWORD c_script_name(Call &) { return ptrv(SCRIPT_NAMES[nv() % 5]); }
static DWORD c_sound_find(Call &)
{
    const DWORD v = nv();
    return v % 3 == 0 ? 0xffffffffu : (v >> 4) % 40;
}
// CDialogLine::Text (out ZString): a text into it (the real assignment)
static DWORD c_line_text(Call &c)
{
    if (c.a[0])
        at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(at<void *>(c.a[0]), TEXTS[nv() % 5]);
    return nv();
}
static DWORD c_joy_ctor(Call &c)
{
    if (c.ecx) {
        memset(at<void *>(c.ecx), 0, 0x100);
        *at<DWORD *>(c.ecx) = c.a[0];
        *at<DWORD *>(c.ecx + 0x48) = ZSTRING_EMPTY;
    }
    return c.ecx;
}

#define S(addr, regs, args, ret, type) {addr, regs, args, ret, type, false, nullptr}
#define SC(addr, regs, args, fn) {addr, regs, args, 'c', 0, false, fn}
static const Spec DIRECT[] = {
    SC(0x416790, "p-", "p", c_is_kind_of),                 // Object_IsKindOf
    SC(0x4fb0a0, "p-", "o", c_pos),                          // LayerPart_Position
    {0x5d0863, "--", "v", 'i', 0, true, nullptr},            // atexit
    // ai_rest_a.cpp
    S(0x48f6a0, "p-", "v", 'v', 0),                          // EntityAnimated_SetPlayRatio
    S(0x48f700, "p-", "s", 'b', 0),                          // EntityAnimated_PlaySequence
    S(0x4eb050, "p-", "pfvvv", 'n', T_ENTITY),               // CLayer::FindBlocker
    S(0x536da0, "p-", "ff", 'i', 0),                         // CWayPointPath::InitFloat
    S(0x5386b0, "p-", "pppp", 'b', 0),                       // WayPointMap_FindPath
    S(0x4e7180, "p-", "", 'p', T_OTHER),                     // CLayerSaveData::ActivePaths
    S(0x4e70d0, "p-", "", 'p', T_OTHER),                     // CLayerSaveData::PathData
    SC(0x536e70, "p-", "op", c_next_point),                  // CWayPointPath::NextPoint
    S(0x536f40, "p-", "", 'b', 0),                           // CWayPointPath::Advance
    S(0x536e60, "p-", "", 'b', 0),                           // CWayPointPath::HasPath
    S(0x416610, "p-", "", 'v', 0),                           // CObject_Ctor
    S(0x4173c0, "p-", "", 'v', 0),                           // Object_InitFields
    S(0x4173f0, "p-", "", 'v', 0),                           // Object_FreeFields
    S(0x4166d0, "p-", "", 'v', 0),                           // CObject_Dtor
    S(0x536e30, "p-", "", 'i', 0),                           // CWayPointPath::Dtor
    S(0x498dc0, "p-", "", 'i', 0),                           // CEntityBehaviorIdle::Dtor
    S(0x495360, "p-", "", 'i', 0),                           // CEntityBehaviorTurnToAngle::Dtor
    SC(0x4302c0, "p-", "", c_this),                          // CEntityBehaviorMoveToPos::Ctor
    S(0x4fb0f0, "p-", "fvv", 'f', 0),                        // CLayerPart_AngleFrom
    S(0x495490, "p-", "vp", 'b', 0),                         // CEntityBehaviorTurnToAngle::Update
    SC(0x48b590, "p-", "", c_sequence),                      // Entity_SequenceObject
    S(0x48f260, "p-", "", 'b', 0),                           // EntityAnimated_UpdateFacing
    S(0x48feb0, "p-", "v", 'v', 0),                          // EntityAnimated_SetFrame
    S(0x560600, "p-", "vvvp", 'v', 0),                       // CRotationSequence::PlayTriggers
    S(0x48fe80, "p-", "p", 'i', 0),                          // EntityAnimated_Rotation
    SC(0x55fc70, "p-", "ovvvb", c_motion),                   // CRotationSequence::MotionBetween
    S(0x491230, "p-", "vv", 'v', 0),                         // EntityMovable_SetTarget
    S(0x48b770, "p-", "v", 'f', 0),                          // Entity_SetRenderHeightFloat
    // input_rest_a.cpp
    SC(0x4d2460, "--", "", c_time),                          // timeGetTime
    S(0x4dda00, "p-", "v", 'v', 0),                          // CJoystick::Init
    SC(0x4dde40, "p-", "", c_joy_name),                      // the joystick's name
    SC(0x4ddb80, "p-", "", c_poll),                          // CJoystick::Poll
    SC(0x51f450, "p-", "", c_poll),                          // the mouse's buttons (+0x18)
    SC(0x518520, "p-", "", c_wheel),                         // the mouse wheel
    SC(0x5184e0, "p-", "oo", c_mouse_pos),                   // the mouse position
    S(0x518570, "p-", "vv", 'v', 0),                         // the mouse moved
    S(0x4e2850, "p-", "v", 'b', 0),                          // Keys::IsDown
    S(0x4e2820, "p-", "v", 'b', 0),                          // Keys::TakePress
    S(0x4e2980, "p-", "v", 'v', 0),                          // Keys::ClearPresses
    S(0x46fb60, "p-", "v", 'v', 0),                          // ControlConfig::WriteIni
    S(0x46fcc0, "p-", "v", 'v', 0),                          // ControlConfig::ReadIni
    S(0x4f7a70, "p-", "ppfvv", 'v', 0),                      // ClientConfigControlBlockReader_Write
    SC(0x4e2a90, "p-", "v", c_text),                         // KeyName
    SC(0x4de080, "p-", "vo", c_bits_text),                   // CJoystick::BitsText
    SC(0x4dd9a0, "--", "v", c_joy_ctor),                     // CJoystick::Ctor (on a fresh heap block)
    S(0x4a4690, "ps", "sv", 'i', 0),                         // Ini_WriteInt
    // dialog_rest_a.cpp
    SC(0x48d8f0, "p-", "v", c_script),                       // DefinitionList_Object
    SC(0x57e690, "p-", "sv", c_sound_find),                  // the sound manager's lookup
    S(0x57e5d0, "p-", "vz", 'v', 0),                         // the sound manager's prepare
    SC(0x41e010, "p-", "o", c_line_text),                    // CDialogLine::Text
};
constexpr int NDIRECT = sizeof DIRECT / sizeof DIRECT[0];
constexpr int VBASE = 160;
static_assert(NDIRECT <= VBASE);
static_assert(VBASE + NTYPES * 256 == 4768, "the thunk count of ra_thunks");

// The fakes' slots (type, slot, spec)
struct VSpec {
    int type, slot;
    Spec spec;
};
#define V(t, s, args, ret, rt) {t, s, {0, "p-", args, ret, rt, false, nullptr}}
#define VC(t, s, args, fn) {t, s, {0, "p-", args, 'c', 0, false, fn}}
static const VSpec VIRTUAL[] = {
    V(T_ENTITY, 55, "", 'f', 0),
    V(T_ENTITY, 58, "v", 'v', 0),
    V(T_ENTITY, 59, "v", 'v', 0),
    VC(T_ENTITY, 84, "", c_frame),
    V(T_ENTITY, 107, "t", 'v', 0),
    VC(T_ENTITY, 114, "", c_point),
    V(T_ENTITY, 120, "vv", 'v', 0),
    V(T_BEHAVIOR, 23, "p", 'b', 0),
    V(T_BEHAVIOR, 25, "vp", 'b', 0),
    V(T_OTHER, 25, "pf", 'b', 0),
    VC(T_OTHER, 26, "", c_fps),
    V(T_WORLD, 17, "s", 'v', 0),
    V(T_REPORTER, 30, "", 'b', 0),
    VC(T_CATALOGUE, 1, "v", c_script_name),
    VC(T_CATALOGUE, 37, "ssp", c_script),
    V(T_ACTION, 33, "zzvvvvv", 'b', 0),
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
extern char ra_thunks[];
DWORD ra_pop, ra_ret_float, ra_edx;
long double ra_fret;
DWORD __cdecl ra_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _ra_thunks\n"
        "_ra_thunks:\n"
        ".set ra_i, 0\n"
        ".rept 4768\n"   // VBASE + NTYPES * 256
        "  movl $ra_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _ra_common - . - 4\n"
        ".set ra_i, ra_i + 1\n"
        ".endr\n"
        "_ra_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _ra_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _ra_ret_float\n"
        "  je 1f\n"
        "  fldt _ra_fret\n"
        "1:\n"
        "  movl _ra_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _ra_pop, %esp\n"
        "  jmp *%ecx\n");

static DWORD thunk(int i) { return ptrv(ra_thunks) + 10u * (DWORD)i; }

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

DWORD __cdecl ra_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
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
            ra_pop = 4;
            ra_ret_float = 0;
            ra_edx = 0;
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
    ra_pop = 4 + (s->caller_pops ? 0 : 4 * (DWORD)lstrlenA(s->args));
    ra_ret_float = c.fset ? 1 : 0;
    ra_fret = c.fret;
    ra_edx = nv();
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
        const BYTE *th = reinterpret_cast<const BYTE *>(ra_thunks) + 10 * k;
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
    int (*const tables[])(Hook *) = {hooks_ai_rest_a, hooks_input_rest_a, hooks_dialog_rest_a};
    const char *const names[] = {"ai_rest_a", "input_rest_a", "dialog_rest_a"};
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
    {0x62ead8, 4}, {0x62eae0, 8},   // the move behaviour's static collide checker and its guard
    // the control configuration's statics: the vector, the three bit sets, their guard (the ZStrings
    // between them stay as they are)
    {0x6bd538, 8}, {0x6bd548, 12}, {0x6bd558, 12}, {0x6bd564, 4}, {0x6bd570, 16},
    // the dialogue scripts: the list's vtable, its records and count
    {0x62e7e0, 4}, {0x62e810, 8},
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
DWORD *case_global(DWORD address)
{
    for (int i = 0; i < NGLOBALS; i++)
        if (GLOBALS[i].address == address)
            return g_case_globals[i];
    return nullptr;
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

// Valid ZStrings (their buffer pointers) for the net player's name and challenge and the move behaviour's
// sequence, made once
static DWORD g_name_text, g_challenge_text;
DWORD g_texts[3];
void make_static_texts()
{
    if (!g_name_text)
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_name_text, "Crater");
    if (!g_challenge_text)
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_challenge_text, "aXbYcZ");
    const char *const t[3] = {"", "Walk", "No Such Sequence"};
    for (int i = 0; i < 3; i++)
        if (!g_texts[i])
            at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&g_texts[i], t[i]);
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
        // the angle reaches the sine and cosine tables, which read outside them for NaN, infinity and huge
        // values (the original as well): finite only
        fl<DWORD>(e, 0xc8) = bits(-7.0f + 14.0f * (float)((g.next() >> 8) % 100000) * 0.00001f);
        fl<DWORD>(e, 0x108) = g.fbits_(-50.0f, 300.0f);
        fl<DWORD>(e, 0x100) = g.fbits_(0.0f, 4000.0f);
        fl<DWORD>(e, 0x104) = g.fbits_(0.0f, 4000.0f);
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
    g_case_globals[3][0] = g.next() % 3 ? 1 : 0;
    g_case_globals[4][0] = 0x5eec54;
    // the fake sequence: its frames' first dwords, the frame count (at least 1: the update's frame loop)
    BYTE *seq = obj(O_SEQ);
    vt(O_SEQ, T_OTHER);
    fl<DWORD>(seq, 0x44) = ptrv(seq + 0x100);
    if (g.next() % 4 == 0)
        fl<DWORD>(seq, 0x100) = fl<DWORD>(seq, 0x104) = 0;
    fl<DWORD>(seq, 0x48) = 1 + g.next() % 40;
    // the two fake dialogue scripts: 4 lines each, a line's +8 one of the static texts
    for (int k = 0; k < 2; k++) {
        BYTE *sc = obj(O_SCRIPT + k);
        fl<DWORD>(sc, 8) = ptrv(sc + 0x100);
        for (int i = 0; i < 4; i++) {
            BYTE *line = obj(O_LINES) + 0x40 * (DWORD)(k * 4 + i);
            fl<DWORD>(sc, 0x100 + 4 * (DWORD)i) = ptrv(line);
            fl<DWORD>(line, 8) = g_texts[g.next() % 3];
        }
    }
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
    case CALL_FASTCALL7:
        return at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
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

}  // namespace zax::ra
