// Checks of envelopes.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): both class getter copies, Create/CreateArray and the
//    deleting destructors, RegisterFields and the registered descriptors of all seven classes.
//  - The slots Advance, Reset and Assign on envelopes in the check's own memory: random values, the edge
//    values (the current value at the start or end value, NaN, infinities, -0, a zero speed, both
//    directions, every ending behaviour and unknown ones) and random seconds (also 0, negative, NaN).
//    The slots call nothing, so the original runs through its trampoline.
//  - Real data: the envelopes embedded in the running level's parts and AIs (found by their vtables), as
//    copies through the three slots.
#include "check_util.h"
#include "checks_inventory_common.h"
#include "checks_ai_world.h"

namespace zax {

int hooks_envelopes(Hook *out);

namespace {

const invcheck::ClassDef CLASSES[] = {
    {"CEnvelope", 0x24, 0x6bdefc, 0x49d7a0, 0x49d7e0, 0x49d920, {0x49d610, 0x49d8c0}, true},
    {"CTimeEnvelope", 0x24, 0x6bdf00, 0x49dda0, 0x49ddf0, 0x49dfc0, {0x49dcf0, 0x49df00}, true, true},
    {"CConeAngleEnvelope", 0x24, 0x6bdf04, 0x49e1e0, 0x49e230, 0x49e400, {0x49e130, 0x49e340}, true, true},
    {"CSpeedEnvelope", 0x24, 0x6bdf08, 0x49e700, 0x49e750, 0x49e920, {0x49e650, 0x49e860}, true, true},
    {"CTrajectoryEnvelope", 0x24, 0x6bdf0c, 0x49eb40, 0x49eb90, 0x49ed60, {0x49ea90, 0x49eca0}, true, true},
    {"CFacingAngleEnvelope", 0x24, 0x6bdf10, 0x49f060, 0x49f0b0, 0x49f280, {0x49efb0, 0x49f1c0}, true, true},
    {"CLengthEnvelope", 0x24, 0x6bdf14, 0x49f580, 0x49f5d0, 0x49f7a0, {0x49f4d0, 0x49f6e0}, true, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
constexpr DWORD VTABLES[] = {0x5f3cbc, 0x5f3d24, 0x5f3d8c, 0x5f3df4, 0x5f3e5c, 0x5f3ec4, 0x5f3f2c};

using Advance = void(THISCALL *)(void *, float);
using Reset = void(THISCALL *)(void *);
using Assign = void *(THISCALL *)(void *, const void *);
constexpr DWORD A_ADVANCE = 0x49d660, A_RESET = 0x49d730, A_ASSIGN = 0x49d760;

// ---- Values ------------------------------------------------------------------------------------------------

constexpr DWORD SPECIAL[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000,
                             0x7fa00000 /* signalling NaN */, 0x00000001, 0x42c80000, 0xc2c80000};
DWORD rnd_float(float lo, float hi)
{
    const float f = lo + (hi - lo) * (float)(random() >> 8) * (1.0f / 16777216.0f);
    return bits(f);
}
DWORD a_value()
{
    const DWORD k = random() % 16;
    if (k < 3)
        return SPECIAL[random() % (sizeof SPECIAL / 4)];
    if (k == 3)
        return random();
    return rnd_float(-200.0f, 200.0f);
}
void make_envelope(BYTE *e)
{
    memset(e, 0, 0x24);
    *reinterpret_cast<DWORD *>(e) = VTABLES[random() % 7];
    DWORD *v = reinterpret_cast<DWORD *>(e);
    v[2] = a_value();                                     // start
    v[4] = random() % 5 ? a_value() : v[2];               // end (sometimes the start value)
    const DWORD k = random() % 6;
    v[3] = k == 0 ? v[2] : k == 1 ? v[4] : a_value();     // current: at an end or anywhere
    v[5] = random() % 6 ? rnd_float(0.0f, 300.0f) : a_value();   // speed
    v[6] = random() % 8 ? (random() % 2 ? 0x3f800000 : 0xbf800000) : a_value();   // direction
    e[0x1c] = random() % 8 ? (BYTE)(random() % 2) : (BYTE)random();
    e[0x1d] = (BYTE)random();   // the padding stays as it is
    v[8] = random() % 8 ? random() % 3 : random() % 4 == 0 ? random() : 3 + random() % 5;
}
DWORD a_seconds()
{
    const DWORD k = random() % 10;
    if (k == 0)
        return SPECIAL[random() % (sizeof SPECIAL / 4)];
    if (k == 1)
        return rnd_float(-1.0f, 0.0f);
    return rnd_float(0.0f, 0.5f);
}

constexpr DWORD EDGE_SECONDS[] = {0, 0x3e4ccccd /* 0.2 */, 0x40000000 /* 2 */, 0xbe4ccccd /* -0.2 */};
constexpr DWORD REAL_SECONDS[] = {0x3c888889 /* 1/60 */, 0x3d088889 /* 1/30 */, 0x3f000000, 0x41200000 /* 10 */};

// ---- Slot cases ---------------------------------------------------------------------------------------------

// One envelope through a slot on both sides; `what` 0 Advance, 1 Reset, 2 Assign
void run_case(Tally &t, const BYTE *start, int what, DWORD seconds, const BYTE *other)
{
    alignas(4) BYTE e[2][0x24];
    DWORD eax[2] = {0, 0};
    for (int side = 0; side < 2; side++) {
        memcpy(e[side], start, 0x24);
        if (what == 0) {
            float s;
            memcpy(&s, &seconds, 4);
            (side ? at<Advance>(A_ADVANCE) : original<Advance>(A_ADVANCE))(e[side], s);
        } else if (what == 1) {
            (side ? at<Reset>(A_RESET) : original<Reset>(A_RESET))(e[side]);
        } else {
            eax[side] = invcheck::ptrv((side ? at<Assign>(A_ASSIGN) : original<Assign>(A_ASSIGN))(e[side], other)) - invcheck::ptrv(e[side]);
        }
    }
    const DWORD *a = reinterpret_cast<const DWORD *>(e[0]), *b = reinterpret_cast<const DWORD *>(e[1]);
    const DWORD *s = reinterpret_cast<const DWORD *>(start);
    char d[200];
    wsprintfA(d, "[%s seconds %x: start %x cur %x end %x speed %x dir %x using %x ending %x -> cur %x/%x dir %x/%x using %x/%x]",
              what == 0 ? "Advance" : what == 1 ? "Reset" : "Assign", seconds, s[2], s[3], s[4], s[5], s[6], start[0x1c], s[8], a[3],
              b[3], a[6], b[6], e[0][0x1c], e[1][0x1c]);
    t.check(!memcmp(e[0], e[1], 0x24) && eax[0] == eax[1] && eax[0] == 0, "CEnvelope", d);
}

void test_slots(Tally &t)
{
    alignas(4) BYTE start[0x24], other[0x24];
    const int n = scaled(4000);
    for (int i = 0; i < n; i++) {
        make_envelope(start);
        run_case(t, start, 0, a_seconds(), nullptr);
        if (i % 4 == 0)
            run_case(t, start, 1, 0, nullptr);
        if (i % 8 == 0) {
            make_envelope(other);
            run_case(t, start, 2, 0, other);
        }
    }
    // every ending behaviour at both ends, both directions and both states of "Using Start"
    for (DWORD ending = 0; ending < 4; ending++)
        for (int k = 0; k < 8; k++) {
            make_envelope(start);
            DWORD *v = reinterpret_cast<DWORD *>(start);
            v[2] = 0x41200000;   // 10
            v[4] = k & 4 ? 0x40a00000 : 0x42480000;   // 5 or 50
            v[3] = k & 1 ? v[4] : v[2];
            start[0x1c] = (BYTE)((k >> 1) & 1);
            v[5] = 0x41f00000;   // 30/s
            v[6] = k & 4 ? 0xbf800000 : 0x3f800000;
            v[8] = ending;
            for (DWORD sec : EDGE_SECONDS)
                run_case(t, start, 0, sec, nullptr);
        }
}

// ---- Real data ------------------------------------------------------------------------------------------------

BYTE *g_parts[8192];
BYTE *g_found[64];

void test_real(Tally &registered, Tally &real)
{
    for (const invcheck::ClassDef &d : CLASSES)
        invcheck::test_registered_class(registered, d);
    BYTE **parts = g_parts;
    const int n = aiw::level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  envelopes real data: no level grid found");
        return;
    }
    int found = 0, objects = 0;
    for (int i = 0; i < n && found < 64; i++) {
        BYTE *ais[64];
        const int count = aiw::part_ais(parts[i], parts, n, ais + 1, 63);
        ais[0] = parts[i];
        for (int k = 0; k <= count && found < 64; k++) {
            const CClassInfo *c = invcheck::class_of(ais[k]);
            if (!c || c->size > 0x1000 || !invcheck::readable(ais[k], c->size))
                continue;
            objects++;
            for (DWORD off = 8; off + 0x24 <= c->size && found < 64; off += 4) {
                const DWORD v = *reinterpret_cast<const DWORD *>(ais[k] + off);
                bool is = false;
                for (DWORD vt : VTABLES)
                    is |= v == vt;
                bool dup = false;
                for (int j = 0; j < found && !dup; j++)
                    dup = g_found[j] == ais[k] + off;
                if (is && !dup)
                    g_found[found++] = ais[k] + off;
            }
        }
    }
    alignas(4) BYTE copy[0x24];
    for (int i = 0; i < found; i++) {
        memcpy(copy, g_found[i], 0x24);
        for (DWORD sec : REAL_SECONDS)
            run_case(real, copy, 0, sec, nullptr);
        run_case(real, copy, 1, 0, nullptr);
        run_case(real, copy, 2, 0, g_found[(i + 1) % found]);
    }
    log("  envelopes real data: %d parts, %d objects searched, %d envelopes", n, objects, found);
}

}  // namespace

ZAX_CHECKS(envelopes)
{
    (void)cls;
    (void)n;
    Tally getters{"envelopes: class getters"}, objects{"envelopes: Create, CreateArray, destructors"},
        fields{"envelopes: RegisterFields"}, real_fields{"envelopes: RegisterFields against the registered fields (real data)"},
        slots{"envelopes: Advance, Reset, Assign"}, registered{"envelopes: registered class infos (real data)"},
        real{"envelopes: the level's envelopes through their slots (real data)"};
    invcheck::g_hook_table = hooks_envelopes;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    else
        objects.check(false, "envelopes", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    g_seed = 0x5eed0e;
    test_slots(slots);
    slots.report();
    test_real(registered, real);
    registered.report();
    real.report();
}

}  // namespace zax
