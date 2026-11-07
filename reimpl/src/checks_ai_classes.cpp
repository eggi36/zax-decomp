// Checks of the AI classes' Create and CreateArray and the out-of-line constructors (ai_classes.cpp)
// against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (pointers made relative), the bytes of every object afterwards and
// the state of the random generator 0x6d06c0 (COldBadPatrolAI's constructor seeds it). Dwords that
// differ between the sides count as equal when both point to the same offset of the object or block
// under test, or when both point to a new block whose first 8 bytes (vtable, dynamic properties) agree
// (the path collide checker of CEntityBehaviorMoveToPos).
//  - Create of all 106 classes on a pool block filled with a pattern first (0, 0xff, a random byte,
//    random dwords), so every byte is defined; the pattern decides whether CEntityBehaviorMoveToPos
//    finds a path collide checker at +0xb4 (InitFields sets no values). Then deleted through slot 1.
//  - CreateArray with 0-3 elements: only the bytes the constructor writes (found by running the
//    original Create on blocks filled with 0 and with 0xff), then deleted through slot 1 (flags 3).
//  - The 16 out-of-line constructors called directly on a buffer filled with a pattern (all 0x150
//    bytes compared), then destroyed through slot 1 without freeing (not the 6 abstract classes: their
//    slot 1 is _purecall).
//  - Class getters (the 12 of the trigger AIs, with the class variable set and 0; ClassInfo_Register
//    0x415780 jumps to a stand-in) and the RegisterFields of the 9 trigger AIs into a fake class info
//    (variable set and 0): the descriptors member by member, eax, the alternative class name, against
//    each other and (the original's) against the descriptors registered at startup (real data).
//  - Real data: the registered class infos of the 106 classes (size, Create and CreateArray as the
//    original registered them at startup) and the objects of these classes in the running level (the
//    parts in the level's grid, their activity arrays' AIs and the objects embedded in or pointed to by
//    them, e.g. the behaviour stack every CEntityAnimated embeds): per class found, its vtable against
//    the one Create gives, and Create through the class info's pointer on both sides.
#include "check_util.h"
#include "field_kinds.h"
#include "layer_parts.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

int hooks_ai_classes(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

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
constexpr int MAX_SITES = 512;   // 249 hooks on 2026-10-25
SavedSite g_sites[MAX_SITES];
Hook g_hook_list[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook *const list = g_hook_list;
    g_nsites = 0;
    const int n = hooks_ai_classes(nullptr);
    if (!hook_sites_fit("ai_classes", n, MAX_SITES))
        return;
    hooks_ai_classes(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- The classes ---------------------------------------------------------------------------------------

struct ClassDef {
    const char *name;
    DWORD create, create_array, size, class_var;
};
const ClassDef CLASSES[] = {
    {"CollideTriggerAI", 0x41c9d0, 0x41ca30, 0x38, 0x62e7c0},
    {"CPassThroughTriggerAI", 0x41d120, 0x41d170, 0x24, 0x62e7c4},
    {"CRelayAI", 0x41fc20, 0x41fc60, 0x18, 0x62e940},
    {"CRepeatTimerTriggerAI", 0x4203a0, 0x4203e0, 0x20, 0x62e94c},
    {"CSpawnPointAI", 0x4209a0, 0x4209e0, 0x1c, 0x62e954},
    {"CTouchingOvalTriggerAI", 0x420f30, 0x420f60, 0x44, 0x62e98c},
    {"CTouchingPolygonTriggerAI", 0x4218d0, 0x421900, 0x38, 0x62e9bc},
    {"CAttractCompassAI", 0x422f10, 0x422f50, 0x10, 0x62e9e0},
    {"CAmbientSoundAI", 0x42d660, 0x42d6b0, 0x28, 0x62ea7c},
    {"CApproachTargetAI", 0x42e150, 0x42e1a0, 0xd8, 0x62ea84},
    {"CBehaviorActivity", 0x42f360, 0x42f3b0, 0x34, 0x62ead0},
    {"CEntityBehaviorMoveToPos", 0x42f730, 0x42f750, 0xbc, 0x62eae8},
    {"CBounceAI", 0x467180, 0x4671d0, 0x14, 0x6bced0},
    {"CChaseInvestigateAI", 0x46b810, 0x46b870, 0x110, 0x6bd2d0},
    {"CChasePursueAI", 0x46c6f0, 0x46c740, 0xd4, 0x6bd2d8},
    {"CCompanionAI", 0x46e120, 0x46e190, 0x10c, 0x6bd328},
    {"CFireContinuousFireWeaponBaseAI", 0x472f30, 0x472f80, 0x34, 0x6bd6f8},
    {"CFireElectricalWeaponAI", 0x474880, 0x4748f0, 0x40, 0x6bd6fc},
    {"CFireParticleWeaponAI", 0x475240, 0x4752b0, 0x40, 0x6bd700},
    {"CDamageExpandingRadiusAI", 0x4764f0, 0x476540, 0x34, 0x6bd74c},
    {"CDeathBehaviorListDefinition", 0x477d30, 0x477d80, 0x18, 0x6bd9f4},
    {"CPickupDeployedItemAI", 0x47b910, 0x47b940, 0x30, 0x6bdab4},
    {"CDoorAI", 0x481760, 0x4817b0, 0x48, 0x6bdc6c},
    {"CEntityBehaviorTurnMovingAngle", 0x494790, 0x4947e0, 0x14, 0x6bde08},
    {"CEntityBehaviorTurnAimingAngle", 0x494a90, 0x494ae0, 0x14, 0x6bde0c},
    {"CEntityBehaviorTurnToAngle", 0x494de0, 0x494e40, 0x1c, 0x6bde10},
    {"CEntityBehaviorTurnToTarget", 0x495760, 0x4957b0, 0x24, 0x6bde14},
    {"CEntityBehaviorSetRotationSequence", 0x496370, 0x4963b0, 0x10, 0x6bde18},
    {"CEntityBehaviorPlaySequenceOnce", 0x4966d0, 0x496720, 0x28, 0x6bde1c},
    {"CEntityBehaviorSetPlayRatio", 0x4972e0, 0x497310, 0x10, 0x6bde20},
    {"CEntityBehaviorMarkForDeletion", 0x4975b0, 0x4975e0, 0xc, 0x6bde24},
    {"CEntityBehaviorClearFlags", 0x4977f0, 0x497830, 0x10, 0x6bde28},
    {"CEntityBehaviorWait", 0x497ca0, 0x497cd0, 0x14, 0x6bde2c},
    {"CEntityBehaviorFade", 0x498370, 0x4983c0, 0x18, 0x6bde34},
    {"CEntityBehaviorIdle", 0x498720, 0x498770, 0x30, 0x6bde38},
    {"CEntityBehaviorSetCurFrame", 0x499360, 0x499390, 0x10, 0x6bde3c},
    {"CEntityBehaviorExecuteAction", 0x499590, 0x4995c0, 0x10, 0x6bde40},
    {"CEntityBehaviorPlaySequenceLooped", 0x499800, 0x499840, 0x14, 0x6bde44},
    {"CEntityBehaviorStack", 0x49a000, 0x49a050, 0x20, 0x6bde64},
    {"CFidgetSettings", 0x49ffa0, 0x49ffd0, 0x14, 0x6bdf3c},
    {"CFidgetData", 0x4a0380, 0x4a03a0, 0x8c, 0x6bdf40},
    {"FollowAI", 0x4a4fa0, 0x4a5010, 0xf8, 0x6be1b0},
    {"CGravityAI", 0x4ca4e0, 0x4ca520, 0x18, 0x6bfe50},
    {"CDrawTimeoutOvalAI", 0x4ca9c0, 0x4caa10, 0x24, 0x6bfe54},
    {"CDrawTimeoutRectAI", 0x4cae90, 0x4caee0, 0x2c, 0x6bfe58},
    {"CPlaySequenceLoopedAI", 0x4cb280, 0x4cb2d0, 0x28, 0x6bfe5c},
    {"CSimultaneousAIsAI", 0x4cb6b0, 0x4cb6f0, 0x18, 0x6bfe60},
    {"CWhileAI", 0x4cbb30, 0x4cbb70, 0x1c, 0x6bfe64},
    {"CWaitAI", 0x4cbfd0, 0x4cc010, 0x20, 0x6bfe68},
    {"CIdleAI", 0x4cc4e0, 0x4cc530, 0x40, 0x6bfe6c},
    {"CForceUpdatingAI", 0x4cc890, 0x4cc8d0, 0x18, 0x6bfe70},
    {"CLimitedTimeAI", 0x4ccca0, 0x4ccce0, 0x20, 0x6bfe74},
    {"CHandleMessageAI", 0x4cd1d0, 0x4cd220, 0x18, 0x6bfe78},
    {"CGeneratorAI", 0x4cfa60, 0x4cfab0, 0x48, 0x6c0450},
    {"CGoToAI", 0x4d0a00, 0x4d0a60, 0x118, 0x6c0458},
    {"CLandscapeAI", 0x4e39c0, 0x4e3a00, 0x34, 0x6c1b10},
    {"CLightBeamAI", 0x4fe2e0, 0x4fe330, 0x18, 0x6c2b50},
    {"CPointOfIntrestAI", 0x508730, 0x508780, 0x1c, 0x6c2dc4},
    {"CNormalAttackAI", 0x522b90, 0x522bf0, 0x4c, 0x6d0134},
    {"CGravityParticleAI", 0x528c10, 0x528c70, 0x34, 0x6d0178},
    {"CFloatParticleAI", 0x529740, 0x5297a0, 0x24, 0x6d017c},
    {"CSwirlParticleAI", 0x529a90, 0x529af0, 0x34, 0x6d0180},
    {"CGenericParticleSystemAI", 0x52f040, 0x52f070, 0x148, 0x6d04fc},
    {"COldBadPatrolAI", 0x53a990, 0x53aa00, 0x34, 0x6d0b10},
    {"CPatrolAreaAI", 0x53b400, 0x53b450, 0xd8, 0x6d0b34},
    {"CPatrolPathAI", 0x53be30, 0x53be90, 0xf4, 0x6d0b70},
    {"CPlayerWalkingWeaponAI", 0x544df0, 0x544e40, 0x14, 0x711118},
    {"CPlayerWalkingItemAI", 0x5450b0, 0x545100, 0x1c, 0x71111c},
    {"CSameAsParentAI", 0x561d30, 0x561d70, 0x14, 0x713178},
    {"CScanAreaAI", 0x565060, 0x5650b0, 0x4c, 0x713214},
    {"CScanInvestigateAI", 0x566060, 0x5660f0, 0x5c, 0x71321c},
    {"CSelectiveCollideAI", 0x56a8e0, 0x56a930, 0x14, 0x71335c},
    {"CSeriesAI", 0x56c730, 0x56c790, 0x2c, 0x7134fc},
    {"CActionAI", 0x56ce70, 0x56ceb0, 0x18, 0x713500},
    {"CShieldAI", 0x56d4b0, 0x56d500, 0x18, 0x71350c},
    {"CPercentInvulnerableAI", 0x56dcf0, 0x56dd30, 0x18, 0x713510},
    {"CShotAI", 0x56fef0, 0x56ff40, 0x10, 0x713540},
    {"CShootOverAI", 0x570120, 0x570170, 0x10, 0x713544},
    {"CTurnToBounceDirAI", 0x570430, 0x570490, 0x10, 0x713548},
    {"CPassThroughShooterAI", 0x570810, 0x570870, 0x10, 0x71354c},
    {"CExplodeOnImpact", 0x570bc0, 0x570c30, 0x10, 0x713550},
    {"CSimpleCollideTriggerAI", 0x570fc0, 0x571010, 0x18, 0x713558},
    {"CSetPatrolAIAction", 0x573670, 0x5736b0, 0x10, 0x713580},
    {"CSetPursueAIAction", 0x5739f0, 0x573a30, 0x10, 0x713584},
    {"CSetAttackAIAction", 0x573d70, 0x573db0, 0x10, 0x713588},
    {"CSkeletonTransition", 0x574c60, 0x574c90, 0x1c, 0x713638},
    {"CSkeletonAI", 0x575000, 0x575070, 0xac, 0x71363c},
    {"CShootEntityBehavior", 0x577ea0, 0x577ee0, 0x14, 0x713668},
    {"CPointWeaponAtTargetEntityBehavior", 0x5785a0, 0x5785d0, 0xc, 0x71366c},
    {"CBroadcastEvilAI", 0x584ea0, 0x584ef0, 0x14, 0x713820},
    {"CStandStillAttackAI", 0x5856f0, 0x585750, 0x30, 0x713828},
    {"CGuardPosAI", 0x585af0, 0x585b50, 0x108, 0x713830},
    {"CEntityBehaviorStateSetClosestTarget", 0x586310, 0x586350, 0x20, 0x713850},
    {"CEntityBehaviorQueue", 0x586980, 0x5869d0, 0x24, 0x713858},
    {"CTemporaryTaskAI", 0x58af50, 0x58af90, 0x1c, 0x7139d0},
    {"CTenaciousInvestigateAI", 0x58c220, 0x58c270, 0xd4, 0x7139dc},
    {"CTurretAI", 0x58d890, 0x58d900, 0x30, 0x713a2c},
    {"CSentinonAI", 0x58e790, 0x58e810, 0x30, 0x713a30},
    {"CAutoGunAI", 0x58eb60, 0x58ebf0, 0x64, 0x713a34},
    {"CWalkingUpperBodyAI", 0x595cd0, 0x595d20, 0x1c, 0x713c48},
    {"CCameraTrakerAI", 0x5aa070, 0x5aa0c0, 0x20, 0x714030},
    {"CEntityBehaviorWarpEffect", 0x5aa6c0, 0x5aa700, 0x20, 0x71403c},
    {"CEntityBehaviorFireWeapon", 0x5aae50, 0x5aae80, 0x10, 0x714040},
    {"CEntityBehaviorFirePrimary", 0x5ab100, 0x5ab140, 0x10, 0x714044},
    {"CEntityBehaviorFireSecondary", 0x5ab700, 0x5ab740, 0x10, 0x714048},
    {"CEntityBehaviorRestartMap", 0x5abc00, 0x5abc30, 0xc, 0x71404c},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

// The out-of-line constructors (thiscall, no arguments, return this)
struct CtorDef {
    const char *name;
    DWORD address;
    bool abstract;   // slot 1 of an abstract class's vtable is _purecall: not destroyed
};
const CtorDef CTORS[] = {{"CTouchingTriggerAI", 0x4223f0, true},
                         {"CEntityBehaviorMoveToPos", 0x4302c0, false},
                         {"CEntityBehavior", 0x494470, true},
                         {"CEntityBehaviorTurnToAngle", 0x495310, false},
                         {"CEntityBehaviorIdle", 0x498d90, false},
                         {"CEntityBehaviorPlaySequenceLooped", 0x499cc0, false},
                         {"CFidgetData", 0x4a04e0, false},
                         {"CAIBase", 0x4fd060, true},
                         {"CParticleSystemAI", 0x52dd20, true},
                         {"CEnvelope", 0x52de80, false},
                         {"CRenderEffectBehavior", 0x55e400, true},
                         {"CSkeletonBaseAI", 0x577db0, true},
                         {"CShootEntityBehavior", 0x5780b0, false},
                         {"CPointWeaponAtTargetEntityBehavior", 0x578780, false},
                         {"CEntityBehaviorStateSetClosestTarget", 0x586770, false},
                         {"CEntityBehaviorQueue", 0x586e10, false}};

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
using CtorFn = void *(THISCALL *)(void *);
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }

// The random generator that COldBadPatrolAI's constructor seeds (0x55c390 on 0x6d06c0)
constexpr DWORD RANDOM_6D06C0 = 0x6d06c0;
SavedBytes g_saved_random;

// ---- One side of a case: the compared bytes ------------------------------------------------------------

constexpr DWORD MAXB = 4 + 3 * 0x150;
struct Side {
    DWORD eax;           // relative to the base when it points into the range
    DWORD base, n;       // compared range
    BYTE bytes[MAXB];
    BYTE head[MAXB / 4][8];   // first 8 bytes of a new block a dword points to (else 0)
    unsigned random;     // hash of the random generator afterwards
};
Side g_s[2];
BYTE g_mask[2][MAXB];   // 1 = byte compared (per side; a byte counts when both sides compare it)
int g_side;

void snapshot(const BYTE *p, DWORD n, const BYTE *mask)
{
    Side &s = g_s[g_side];
    s.base = ptrv(p);
    s.n = n;
    memcpy(s.bytes, p, n);
    memcpy(g_mask[g_side], mask, n);
    for (DWORD i = 0; i + 4 <= n; i += 4) {
        const DWORD v = *reinterpret_cast<const DWORD *>(p + i);
        memset(s.head[i / 4], 0, 8);
        if (heap_pointer(v) && (v < s.base || v >= s.base + n) && readable(at<void *>(v), 8))
            memcpy(s.head[i / 4], at<void *>(v), 8);
    }
}

DWORD rel(DWORD v)
{
    const Side &s = g_s[g_side];
    return v >= s.base && v <= s.base + s.n ? 0xb0000000u + (v - s.base) : v;   // the end too (an empty array)
}

bool compare_sides(char *detail)
{
    const Side &a = g_s[0], &b = g_s[1];
    if (a.n != b.n) {
        wsprintfA(detail, "[size %u / %u]", a.n, b.n);
        return false;
    }
    if (a.eax != b.eax) {
        wsprintfA(detail, "[eax %x / %x]", a.eax, b.eax);
        return false;
    }
    if (a.random != b.random) {
        wsprintfA(detail, "[random generator %x / %x]", a.random, b.random);
        return false;
    }
    for (DWORD i = 0; i < a.n; i += 4) {
        bool all = i + 4 <= a.n;
        for (DWORD k = i; k < i + 4 && k < a.n; k++)
            all = all && g_mask[0][k] && g_mask[1][k];
        const DWORD w0 = all ? *reinterpret_cast<const DWORD *>(a.bytes + i) : 0, w1 = all ? *reinterpret_cast<const DWORD *>(b.bytes + i) : 0;
        if (all) {
            if (w0 == w1)
                continue;   // equal raw values first
            if (w0 >= a.base && w0 < a.base + a.n && w1 >= b.base && w1 < b.base + b.n && w0 - a.base == w1 - b.base)
                continue;   // the same offset of the object or block
            if (heap_pointer(w0) && heap_pointer(w1) && !memcmp(a.head[i / 4], b.head[i / 4], 8) &&
                (a.head[i / 4][0] | a.head[i / 4][1] | a.head[i / 4][2] | a.head[i / 4][3]))
                continue;   // new blocks with the same vtable and dynamic properties
            wsprintfA(detail, "[+%x: %x / %x]", i, w0, w1);
            return false;
        }
        for (DWORD k = i; k < i + 4 && k < a.n; k++)
            if (g_mask[0][k] && g_mask[1][k] && a.bytes[k] != b.bytes[k]) {
                wsprintfA(detail, "[byte +%x: %x / %x]", k, a.bytes[k], b.bytes[k]);
                return false;
            }
    }
    return true;
}

// Both sides of a case: side 0 with the module's hooks out, side 1 through the reimplementation; the
// random generator 0x6d06c0 from the same state on both sides
bool g_trace = false;
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    if (g_trace)
        log("  > %s %u", what, seed);
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        memset(&g_s[side], 0, sizeof g_s[side]);
        restore_bytes(g_saved_random);
        if (side == 0)
            hooks_out();
        run();
        if (side == 0)
            hooks_in();
        g_s[side].random = fnv(2166136261u, at<const BYTE *>(RANDOM_6D06C0), 0x400);
    }
    restore_bytes(g_saved_random);
    char d[160];
    const bool same = compare_sides(d);
    char line[240];
    wsprintfA(line, "%s seed %u %s", what, seed, same ? "" : d);
    t.check(same, "ai_classes", line);
    return same;
}

void fill(BYTE *p, DWORD n, unsigned kind)
{
    switch (kind % 4) {
    case 0: memset(p, 0, n); break;
    case 1: memset(p, 0xff, n); break;
    case 2: memset(p, (int)(random() & 0xff), n); break;
    default:
        for (DWORD i = 0; i + 4 <= n; i += 4)
            *reinterpret_cast<DWORD *>(p + i) = random();
    }
}

// The bytes a class's constructor writes: the original Create on pool blocks filled with 0 and 0xff
BYTE g_class_mask[0x150];
bool class_mask(const ClassDef &c)
{
    BYTE first[0x150];
    for (int pass = 0; pass < 2; pass++) {
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, pass ? 0xff : 0, c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(original<Create0>(c.create)());
        if (o != p) {
            if (o)
                destroy(o, 1);
            return false;
        }
        if (pass)
            for (DWORD i = 0; i < c.size; i++)
                g_class_mask[i] = first[i] == o[i];
        else
            memcpy(first, o, c.size);
        destroy(o, 1);
    }
    restore_bytes(g_saved_random);
    return true;
}

// Create on a pool block filled with a pattern (all bytes compared when Create gets that block)
void create_case(const ClassDef &c, unsigned kind)
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
    fill(p, c.size, kind);
    pool_free(p, c.size);
    BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
    if (!o)
        return;
    BYTE all[0x150];
    memset(all, 1, sizeof all);
    snapshot(o, c.size, o == p ? all : g_class_mask);
    g_s[g_side].eax = rel(ptrv(o));
    destroy(o, 1);
}

// CreateArray with n elements: the count and the bytes the constructor writes
void array_case(const ClassDef &c, int n)
{
    BYTE *e = static_cast<BYTE *>(at<CreateN>(c.create_array)(n, 0, 0));
    if (!e)
        return;
    BYTE mask[MAXB];
    memset(mask, 1, 4);
    for (int i = 0; i < n; i++)
        memcpy(mask + 4 + i * c.size, g_class_mask, c.size);
    snapshot(e - 4, 4 + n * c.size, mask);
    g_s[g_side].eax = rel(ptrv(e));
    if (n)
        destroy(e, 3);
    else
        heap_free(e - 4);
}

// An out-of-line constructor on a buffer filled with a pattern, then destroyed without freeing (an
// abstract class is left as it is: its members hold no blocks, only references to the empty string)
alignas(16) BYTE g_buffer[0x150];
void ctor_case(const CtorDef &c, unsigned kind)
{
    fill(g_buffer, sizeof g_buffer, kind);
    BYTE all[0x150];
    memset(all, 1, sizeof all);
    void *r = at<CtorFn>(c.address)(g_buffer);
    snapshot(g_buffer, sizeof g_buffer, all);
    g_s[g_side].eax = rel(ptrv(r));
    if (!c.abstract)
        destroy(g_buffer, 0);
}

// ---- Class getters and RegisterFields of the trigger AIs ---------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create),
                  ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

using Getter = CClassInfo *(__cdecl *)();
struct GetterDef {
    DWORD address, var;
};
const GetterDef GETTERS[] = {{0x41c900, 0x62e7c0}, {0x41cb90, 0x62e7c0}, {0x41d2c0, 0x62e7c4}, {0x41fd70, 0x62e940},
                             {0x420350, 0x62e94c}, {0x4204e0, 0x62e94c}, {0x420950, 0x62e954}, {0x420af0, 0x62e954},
                             {0x421050, 0x62e98c}, {0x4219f0, 0x62e9bc}, {0x421f40, 0x62e9c4}, {0x422730, 0x62e9cc}};

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
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_OBJECT_REF: return same(0x28, 0x30);
    case VT_TYPED_RESOURCE: return same(0x28, 0x2e) && same(0x30, 0x38);
    case VT_STRING:
    case 0x5ed924:   // CTriggerAI's "Triggered By Name": a string kind of its own
        return x[0x28] == y[0x28] && same_text(*reinterpret_cast<const ZString *>(x + 0x2c), *reinterpret_cast<const ZString *>(y + 0x2c)) &&
               same(0x30, 0x34);
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

struct RegFields {
    const char *name;
    DWORD address, var;
};
const RegFields REGISTER_FIELDS[] = {{"CTriggerAI", 0x4227e0, 0x62e9cc},
                                     {"CTouchingTriggerAI", 0x421ff0, 0x62e9c4},
                                     {"CollideTriggerAI", 0x41cbf0, 0x62e7c0},
                                     {"CPassThroughTriggerAI", 0x41d320, 0x62e7c4},
                                     {"CRelayAI", 0x41fdd0, 0x62e940},
                                     {"CRepeatTimerTriggerAI", 0x420540, 0x62e94c},
                                     {"CSpawnPointAI", 0x420b50, 0x62e954},
                                     {"CTouchingOvalTriggerAI", 0x4210b0, 0x62e98c},
                                     {"CTouchingPolygonTriggerAI", 0x421a50, 0x62e9bc}};

constexpr DWORD MAXF = 16;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, result, alt_name, registered;
};
// One side of a RegisterFields: into a fake class info (var_zero: the class variable 0, so the inlined
// getter registers through the stand-in). eax made comparable: the class info as 'C', a pointer into
// the last descriptor by its offset.
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    memset(&g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    const DWORD r = at<DWORD(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
    out.alt_name = ptrv(c->alt_name);
    out.registered = (DWORD)g_nregister;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    const DWORD last = c->num_fields ? ptrv(c->fields[c->num_fields - 1]) : 0;
    out.result = r == ptrv(c) ? 'C' : last && r >= last && r < last + 0x44 ? 0xd0000000u + (r - last) : r;
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < MAXF; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_classes(Tally &t, Tally &real)
{
    for (const GetterDef &g : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(g.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<Getter>(g.address)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<Getter>(g.address)();
            unpatch(p);
            results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
            calls[side] = g_register;
            if (var)
                pool_free(var, sizeof(CClassInfo));
            var = saved;
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x]", g.address, set[0], set[1],
                  results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "ai_classes", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            RegisterCall calls[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                calls[side] = g_register;
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n && m[0].result == m[1].result && m[0].alt_name == m[1].alt_name &&
                        m[0].registered == m[1].registered && memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0;
            for (DWORD i = 0; i < m[0].n && i < MAXF && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, alt name %x / %x, registered %u / %u]", rf.name,
                      var_zero, m[0].n, m[1].n, m[0].result, m[1].result, m[0].alt_name, m[1].alt_name, m[0].registered,
                      m[1].registered);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, rf.name, d);
            // Real data: the class's own descriptors as registered at startup, and its alternative name
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == m[0].n && ptrv(c->alt_name) == m[0].alt_name;
            for (DWORD i = 0; same_real && i < m[0].n && i < MAXF; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[0].fields[i]);
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c ? c->num_fields - c->num_inherited : 0, m[0].n);
            if (!same_real && real.mismatches < 2)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
                    log_desc("registered", c->fields[c->num_inherited + i]);
                    log_desc("made", m[0].fields[i]);
                }
            real.check(same_real, rf.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
}

// ---- Real data -----------------------------------------------------------------------------------------


// The objects of the classes of the table in the running level: every part in the level's grid and the
// objects reachable from it (embedded objects found by their vtable, pointers to objects, the
// activity arrays' AIs), two levels deep; each object counted once
int g_found[NCLASSES];
const BYTE *g_example[NCLASSES];
constexpr int MAX_SEEN = 8192;
DWORD g_seen[MAX_SEEN];
int g_nseen, g_objects;
bool seen(DWORD p)
{
    for (int i = 0; i < g_nseen; i++)
        if (g_seen[i] == p)
            return true;
    if (g_nseen < MAX_SEEN)
        g_seen[g_nseen++] = p;
    return false;
}
void visit(const BYTE *o, int depth)
{
    const CClassInfo *ci = class_of(o);
    if (!ci || seen(ptrv(o)))
        return;
    g_objects++;
    for (int m = 0; m < NCLASSES; m++)
        if (CLASSES[m].class_var && ci == *at<CClassInfo **>(CLASSES[m].class_var) && !g_found[m]++)
            g_example[m] = o;
    if (depth >= 3 || ci->size > 0x1000 || !readable(o, ci->size))
        return;
    for (DWORD off = 4; off + 4 <= ci->size; off += 4) {
        const DWORD v = *reinterpret_cast<const DWORD *>(o + off);
        if (v >= 0x5e0000 && v < 0x610000)
            visit(o + off, depth + 1);   // an embedded object
        else if (heap_pointer(v))
            visit(at<const BYTE *>(v), depth + 1);
    }
    // the activity array of a part: its AIs
    if (depth == 0) {
        const LayerPart *p = reinterpret_cast<const LayerPart *>(o);
        if (p->activity.count <= 1000 && readable(p->activity.data, p->activity.count * 4))
            for (DWORD j = 0; j < p->activity.count; j++)
                visit(static_cast<const BYTE *>(p->activity.data[j]), 1);
    }
}

bool find_level_objects()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return false;
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return false;
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || !readable(c->data, c->count * 4))
            return false;
        for (DWORD k = 0; k < c->count; k++)
            visit(static_cast<const BYTE *>(c->data[k]), 0);
    }
    return true;
}

void test_real(Tally &registered, Tally &level)
{
    // The registered class infos: size, Create and CreateArray
    for (int m = 0; m < NCLASSES; m++) {
        const ClassDef &c = CLASSES[m];
        const CClassInfo *ci = c.class_var ? *at<CClassInfo **>(c.class_var) : nullptr;
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x, create array %x]", c.name, ptrv(ci), ci ? ci->size : 0,
                  ci ? ptrv((void *)ci->create) : 0, ci ? ptrv((void *)ci->create_array) : 0);
        registered.check(ci && ci->size == c.size && ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array,
                         c.name, d);
    }
    // The AIs of the running level
    if (!find_level_objects()) {
        log("  ai_classes real data: no level grid found");
        level.check(false, "ai_classes", "[no level grid]");
        return;
    }
    int classes = 0;
    for (int m = 0; m < NCLASSES; m++) {
        if (!g_found[m])
            continue;
        classes++;
        const ClassDef &c = CLASSES[m];
        log("    %s: %d in the level", c.name, g_found[m]);
        // The level's object has the vtable Create gives (the last level of the constructor)
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        const DWORD vt = o ? *reinterpret_cast<DWORD *>(o) : 0;
        if (o)
            destroy(o, 1);
        char d[120];
        wsprintfA(d, "[%s: level vtable %x, Create %x]", c.name, *reinterpret_cast<const DWORD *>(g_example[m]), vt);
        level.check(vt == *reinterpret_cast<const DWORD *>(g_example[m]), c.name, d);
        // Create through the class info, as the level loading does
        const CClassInfo *ci = *at<CClassInfo **>(c.class_var);
        for (unsigned kind = 0; kind < 4; kind++)
            two_sides(level, c.name, kind, [&]() {
                ClassDef viaInfo = c;
                viaInfo.create = ptrv((void *)ci->create);
                create_case(viaInfo, kind);
            });
    }
    log("  ai_classes real data: %d objects reached from the level's parts, %d classes of this module among them", g_objects,
        classes);
}

}  // namespace

ZAX_CHECKS(ai_classes)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally creates{"ai_classes: Create of 106 classes"}, arrays{"ai_classes: CreateArray of 106 classes"},
        ctors{"ai_classes: out-of-line constructors"}, registered{"ai_classes: registered class infos (real data)"},
        level{"ai_classes: AIs of the running level (real data)"}, getters{"ai_classes: class getters and RegisterFields of the trigger AIs"},
        fields{"ai_classes: RegisterFields against the registered fields (real data)"};
    save_bytes(g_saved_random, RANDOM_6D06C0, 0x400);
    int no_mask = 0;
    for (int m = 0; m < NCLASSES; m++) {
        const ClassDef &c = CLASSES[m];
        if (!class_mask(c)) {
            memset(g_class_mask, 0, sizeof g_class_mask);
            memset(g_class_mask, 1, 4);   // the vtable at least
            no_mask++;
        }
        for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
            two_sides(creates, c.name, seed * 7 + (unsigned)m, [&]() { create_case(c, seed); });
        for (unsigned seed = 1; seed <= 8; seed++)
            two_sides(arrays, c.name, seed * 11 + (unsigned)m, [&]() { array_case(c, (int)(seed % 4)); });
    }
    if (no_mask)
        log("  ai_classes: %d classes without a byte mask (Create did not reuse the pool block)", no_mask);
    creates.report();
    arrays.report();
    g_trace = true;
    for (const CtorDef &c : CTORS)
        for (unsigned seed = 1; seed <= (unsigned)scaled(16); seed++)
            two_sides(ctors, c.name, seed, [&]() { ctor_case(c, seed); });
    ctors.report();
    g_trace = false;
    test_classes(getters, fields);
    getters.report();
    fields.report();
    test_real(registered, level);
    restore_bytes(g_saved_random);
    registered.report();
    level.report();
    log("  ai_classes: %u ms", GetTickCount() - start);
}

}  // namespace zax
