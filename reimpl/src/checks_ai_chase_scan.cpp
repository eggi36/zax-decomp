// Checks of the chase, companion, scan and same-as-parent AIs' registration (ai_chase_scan.cpp) against
// the original, in the game thread (machinery: checks_inventory_common.h; side 0 with the module's hooks
// taken out, side 1 through the reimplementation).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena; Create and CreateArray
//    are ai_classes.cpp's) with this module's deleting destructors, CreateArray 0-3 destroyed as arrays,
//    both class getters (class variable set and 0), RegisterFields into a fake class info and with the
//    class variable 0, the registered descriptors and classes (real data).
//  - The slots that pass calls on, on fakes whose vtables record: CChaseInvestigateAI's slot 29 (+0x10c
//    a fake AI or none), CChasePursueAI's slot 41 (+0x10), CScanInvestigateAI's slots 23, 24, 25, 41
//    (+0x10), CSameAsParentAI's Begin (the entity's slot 59), CScanAreaAI's slot 23 (a fake entity that
//    is a CEntityAnimated or not, slot 46 giving floats), the CFailedTargetMessage constructor.
#include "checks_inventory_common.h"

namespace zax {

int hooks_ai_chase_scan(Hook *out);

namespace {

using namespace invcheck;

// the move behaviour at +0x10 holds its path collide checker (a pool block) at +0xb4
const ClassDef CLASSES[] = {
    {"CChaseInvestigateAI", 0x110, 0x6bd2d0, 0x46b810, 0x46b870, 0x46ba40, {0x46b7b0, 0x46b9d0}, true, false, nullptr, 0x10 + 0xb4},
    {"CChasePursueAI", 0xd4, 0x6bd2d8, 0x46c6f0, 0x46c740, 0x46c8f0, {0x46c690, 0x46c880}, true, false, nullptr, 0x10 + 0xb4},
    {"CCompanionAI", 0x10c, 0x6bd328, 0x46e120, 0x46e190, 0x46e380, {0x46e0c0, 0x46e310}, true, false, nullptr, 0x10 + 0xb4},
    {"CSameAsParentAI", 0x14, 0x713178, 0x561d30, 0x561d70, 0x561ee0, {0x561ce0, 0x561e80}, true},
    {"CScanAreaAI", 0x4c, 0x713214, 0x565060, 0x5650b0, 0x565240, {0x565010, 0x5651e0}, true},
    {"CScanInvestigateAI", 0x5c, 0x71321c, 0x566060, 0x5660f0, 0x5662e0, {0x566010, 0x566280}, true},
};

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }

// A fake object: its vtable's slots record (slot, arguments) and answer from the case
struct Fake {
    DWORD vftable;
    DWORD id;
    CClassInfo *cls;
    float facing;
};
DWORD g_answer;
template <int S> DWORD THISCALL f_rec1(Fake *f, DWORD a) { ev(0x100 + S, f->id, a); return g_answer + S; }
template <int S> DWORD THISCALL f_rec2(Fake *f, DWORD a, DWORD b) { ev(0x100 + S, f->id, a, b); return g_answer + S; }
CClassInfo *THISCALL f_class(Fake *f) { return f->cls; }
float THISCALL f_facing(Fake *f) { ev(0x146, f->id); return f->facing; }
DWORD THISCALL f_bad(Fake *f) { ev(0xbad, f->id); return 0xbad; }
void *g_vt[64];
Fake g_fakes[3];

void init_fakes(unsigned seed)
{
    g_seed = seed;
    for (int i = 0; i < 64; i++)
        g_vt[i] = (void *)&f_bad;
    g_vt[2] = (void *)&f_class;
    g_vt[23] = (void *)&f_rec1<23>;
    g_vt[24] = (void *)&f_rec1<24>;
    g_vt[25] = (void *)&f_rec2<25>;
    g_vt[28] = (void *)&f_rec2<28>;
    g_vt[29] = (void *)&f_rec2<29>;
    g_vt[41] = (void *)&f_rec2<41>;
    g_vt[46] = (void *)&f_facing;
    g_vt[59] = (void *)&f_rec1<59>;
    const float facings[] = {0.0f, -0.0f, 1.5707964f, -999.0f, 1e30f, 3.0e-39f};
    for (int i = 0; i < 3; i++) {
        g_fakes[i].vftable = (DWORD)(uintptr_t)g_vt;
        g_fakes[i].id = (DWORD)i + 1;
        g_fakes[i].cls = at<CClassInfo *(__cdecl *)()>(rnd(3) ? 0x48dcc0 : 0x4163b0)();   // CEntityAnimated or CObject
        g_fakes[i].facing = facings[rnd(6)];
    }
    if (rnd(4) == 0) {
        DWORD nan = 0x7fc00000u | random();
        memcpy(&g_fakes[0].facing, &nan, 4);
    }
    g_answer = random();
}

enum Fn { S_CHASE29, S_PURSUE41, S_SCAN_INV23, S_SCAN_INV24, S_SCAN_INV25, S_SCAN_INV41, S_SAME_BEGIN, S_SCAN23, S_MESSAGE, NFN };
const char *const NAMES[NFN] = {"CChaseInvestigateAI::Slot29", "CChasePursueAI::Slot41", "CScanInvestigateAI::Slot23",
                                "CScanInvestigateAI::Slot24",  "CScanInvestigateAI::Slot25", "CScanInvestigateAI::Slot41",
                                "CSameAsParentAI::Begin",      "CScanAreaAI::Slot23",        "CFailedTargetMessage::Ctor"};

void test_slot(Tally &t, Fn f, unsigned seed)
{
    DWORD r[2];
    BYTE obj[2][0x110];
    for (int side = 0; side < 2; side++) {
        init_fakes(seed);
        log_reset(side);
        BYTE self[0x110];
        for (DWORD k = 0; k < sizeof self; k += 4)
            D(self, k) = random();
        // the embedded object at +0x10 (a fake's vtable) and the AI at +0x10c
        D(self, 0x10) = (DWORD)(uintptr_t)g_vt;
        D(self, 0x14) = 0x10;
        D(self, 0x10c) = rnd(4) ? (DWORD)(uintptr_t)&g_fakes[1] : 0;
        const DWORD a = random(), b = random();
        void *entity = rnd(6) ? (void *)&g_fakes[rnd(3)] : nullptr;
        side_begin(side);
        using T1 = DWORD(THISCALL *)(void *, DWORD);
        using T2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
        switch (f) {
        case S_CHASE29: r[side] = at<T2>(0x46c660)(self, a, b); break;
        case S_PURSUE41: r[side] = at<T2>(0x46cce0)(self, a, b); break;
        case S_SCAN_INV23: r[side] = at<T1>(0x566370)(self, a); break;
        case S_SCAN_INV24: r[side] = at<T1>(0x566390)(self, a); break;
        case S_SCAN_INV25: r[side] = at<T2>(0x5663b0)(self, a, b); break;
        case S_SCAN_INV41: r[side] = at<T2>(0x5663d0)(self, a, b); break;
        case S_SAME_BEGIN: r[side] = at<T1>(0x561ff0)(self, (DWORD)(uintptr_t)(entity ? entity : &g_fakes[0])); break;
        case S_SCAN23: at<T1>(0x565be0)(self, (DWORD)(uintptr_t)entity); r[side] = 0; break;
        case S_MESSAGE: r[side] = at<T1>(0x46cd00)(self, a) == (DWORD)(uintptr_t)self; break;
        default: break;
        }
        side_end(side);
        if (f == S_CHASE29 && !D(self, 0x10c))
            r[side] &= 0xff;   // al 0 without an AI
        memcpy(obj[side], self, sizeof self);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, first difference +%x]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4, g_diff_at);
    t.check(r[0] == r[1] && same_log() && same_region(obj[0], obj[1], sizeof obj[0]), NAMES[f], d);
}

}  // namespace

ZAX_CHECKS(ai_chase_scan)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_ai_chase_scan;
    if (!arena_setup()) {
        log("Check ai chase scan: no arena");
        return;
    }
    Tally objects{"ai chase scan: Create, CreateArray, destructors"}, getters{"ai chase scan: class getters"},
        fields{"ai chase scan: RegisterFields"}, registered{"ai chase scan: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    Tally slots{"ai chase scan: the slots that pass calls on (fakes)"};
    for (int f = 0; f < NFN; f++)
        for (unsigned s = 0; s < (unsigned)scaled(100); s++)
            test_slot(slots, (Fn)f, 0x500000u + 0x1000u * (unsigned)f + s);
    slots.report();
}

}  // namespace zax
