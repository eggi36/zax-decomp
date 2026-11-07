// Checks of the sequence player (sequence_player.cpp) against the original, in the game thread.
//
//  - Direct, on fake CRotationSequence objects (frame deltas of all rotations, random frame counts,
//    rotations, cuts): the rotation count setter, the motion at a fractional frame and between two
//    frames (forward, backward, with wraps), the angle of a rotation (with special floats), the strip
//    offsets and the cut count. Side 0 with the module's hooks taken out, side 1 through the
//    reimplementation, on the same memory; compared: the outputs' bits, eax / st0 and the object.
//  - In the fake world (checks_seq_world.h): the frame index for an angle and a time (the tick count, the
//    direct drawing test and slot 26 recorders), the two synchronised frame helpers, the strip rectangle
//    (slot 23's frame, its slot 36), the frame triggers forward and backward and their dispatcher (the
//    world's running test, the entity's frame lock, frame and play ratio, the triggers' actions all
//    recorders).
//  - Real data: the current sequences of the level's animated entities (0x48b590), read only: the motion
//    at and between their frames for every rotation, the angles of their rotations, cuts and offsets.
#include "checks_seq_common.h"
#include "checks_seq_world.h"

namespace zax {

int hooks_sequence_player(Hook *out);

namespace {

using seqc::hash_ld;

seqc::ModuleHooks g_hooks{"sequence_player", hooks_sequence_player, {}, {}, {}, 0};

unsigned g_rs = 1234;
DWORD rnd()
{
    g_rs = g_rs * 1664525u + 1013904223u;
    DWORD x = g_rs ^ (g_rs >> 15);
    x *= 0x2c1b3c6du;
    return x ^ (x >> 12);
}
DWORD rnd_float(float lo, float hi, bool specials)
{
    const DWORD v = rnd();
    if (specials && (v & 15) == 0) {
        const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xff800000u, 0x3f800000u, 0x501502f9u, 0x00000001u};
        return sp[(v >> 4) & 7];
    }
    const float f = lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
    return bits(f);
}

// ---- Direct: fake sequences ---------------------------------------------------------------------------------

alignas(16) BYTE g_seq[0x80], g_seq0[0x80];
int g_deltas[2 * 12 * 8 + 2];
alignas(16) BYTE g_cuts[0x10 * 8];

void make_sequence(DWORD &frames, DWORD &rotations)
{
    for (DWORD k = 0; k < sizeof g_seq0; k++)
        g_seq0[k] = (BYTE)rnd();
    frames = 1 + rnd() % 12;
    rotations = 1 + rnd() % 8;
    seqc::fl<DWORD>(g_seq0, 0x28) = frames * rotations;
    seqc::fl<DWORD>(g_seq0, 0x44) = seqc::ptrv(g_deltas);
    seqc::fl<DWORD>(g_seq0, 0x48) = frames;
    seqc::fl<DWORD>(g_seq0, 0x4c) = rotations;
    seqc::fl<DWORD>(g_seq0, 0x54) = rnd() % 8;
    seqc::fl<DWORD>(g_seq0, 0x58) = seqc::ptrv(g_cuts);
    for (DWORD k = 0; k < 2 * frames * rotations; k++)
        g_deltas[k] = rnd() % 8 == 0 ? (int)rnd() : (int)(rnd() % 61) - 30;
    for (DWORD k = 0; k < sizeof g_cuts; k += 4)
        seqc::fl<DWORD>(g_cuts, k) = rnd() % 400;
}
// A frame of the rotation: inside [0, frames), now and then a whole one or just below the end
DWORD a_frame(DWORD frames)
{
    const DWORD v = rnd() % 6;
    if (v == 0)
        return bits((float)(rnd() % frames));
    if (v == 1)
        return bits((float)frames - 0.001f);
    return rnd_float(0.0f, (float)frames * 0.9999f, false);
}

using MotionAtFn = void(THISCALL *)(void *, float *, int, DWORD);
using BetweenFn = DWORD(THISCALL *)(void *, float *, int, DWORD, DWORD, DWORD);

void test_direct(Tally &t)
{
    for (int c = 0; c < scaled(1500); c++) {
        DWORD frames, rotations;
        make_sequence(frames, rotations);
        const int kind = c % 7;
        const int rot = (int)(rnd() % rotations);
        const DWORD f1 = a_frame(frames), f2 = a_frame(frames);
        DWORD x1 = 0;
        if (kind == 0)
            x1 = rnd() % 4 ? rnd() % 20 : 0;
        if (kind == 3)
            x1 = rnd() % 2;
        if (kind == 4)
            x1 = (DWORD)((int)(rnd() % 5) - 2);
        if (kind == 5) {
            seqc::fl<DWORD>(g_seq0, 0x38) = rnd_float(-7.0f, 7.0f, true);
            seqc::fl<DWORD>(g_seq0, 0x3c) = rnd_float(0.0f, 7.0f, true);
            if (rnd() % 4 == 0)
                seqc::fl<DWORD>(g_seq0, 0x4c) = rnd() % 3;
            x1 = rnd() % 4 ? rnd() % 12 : rnd();
        }
        if (kind == 6)
            x1 = rnd() % (seqc::fl<DWORD>(g_seq0, 0x54) + 1);
        DWORD r[2];
        float out[2][2];
        BYTE after[2][0x80];
        for (int side = 0; side < 2; side++) {
            memcpy(g_seq, g_seq0, sizeof g_seq);
            out[side][0] = out[side][1] = 0;
            if (side == 0)
                g_hooks.out();
            switch (kind) {
            case 0: r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55f790)(g_seq, x1); break;
            case 1:
                at<MotionAtFn>(0x55fb60)(g_seq, out[side], rot, f1);
                r[side] = 0;
                break;
            case 2:
            case 3: r[side] = at<BetweenFn>(0x55fc70)(g_seq, out[side], rot, f1, f2, x1) - seqc::ptrv(out[side]); break;
            case 4: r[side] = at<BetweenFn>(0x55fd30)(g_seq, out[side], rot, f1, f2, x1) - seqc::ptrv(out[side]); break;
            case 5: r[side] = hash_ld(at<long double(THISCALL *)(void *, DWORD)>(0x55fe90)(g_seq, x1)); break;
            default:
                r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55fc50)(g_seq, x1) ^ at<DWORD(THISCALL *)(void *)>(0x560260)(g_seq) * 31;
                break;
            }
            if (side == 0)
                g_hooks.in();
            memcpy(after[side], g_seq, sizeof g_seq);
        }
        char d[160];
        wsprintfA(d, "[case %d kind %d: frames %u rotations %u rot %d, %x %x: eax %x / %x, out %x %x / %x %x]", c, kind, frames, rotations, rot,
                  f1, f2, r[0], r[1], bits(out[0][0]), bits(out[0][1]), bits(out[1][0]), bits(out[1][1]));
        t.check(r[0] == r[1] && bits(out[0][0]) == bits(out[1][0]) && bits(out[0][1]) == bits(out[1][1]) &&
                    memcmp(after[0], after[1], sizeof after[0]) == 0,
                "CRotationSequence", d);
    }
}

// ---- The fake world -------------------------------------------------------------------------------------------

using namespace sqw;

// The object under test: a fake CRotationSequence with fake slots 23 and 26, its frame deltas, cuts and
// triggers in extra objects
void make_seq(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_SEQUENCE]);
    fl<DWORD>(s, 0x1c) = g.next();
    const DWORD frames = 1 + g.next() % 10, rotations = 1 + g.next() % 6;
    fl<DWORD>(s, 0x28) = frames * rotations;
    fl<DWORD>(s, 0x38) = g.fbits_(-7.0f, 7.0f);
    fl<DWORD>(s, 0x3c) = g.next() % 4 ? bits(6.2831855f) : g.fbits_(0.0f, 7.0f);
    fl<DWORD>(s, 0x44) = ptrv(extra(0));
    fl<DWORD>(s, 0x48) = frames;
    fl<DWORD>(s, 0x4c) = rotations;
    fl<DWORD>(s, 0x54) = g.next() % 5;
    fl<DWORD>(s, 0x58) = ptrv(extra(1));
    for (DWORD k = 0; k < 0x80; k += 4)
        fl<DWORD>(extra(1), k) = g.next() % 400 - 50;
    // triggers: one per frame of a rotation at most, action or 0, flags 0-3
    fl<DWORD>(s, 0x5c) = g.next() % 4 ? frames : g.next() % (frames + 1);
    fl<DWORD>(s, 0x60) = ptrv(extra(2));
    for (DWORD i = 0; i < 16; i++) {
        BYTE *tr = extra(2) + i * 0x10;
        fl<DWORD>(tr, 8) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
        fl<DWORD>(tr, 0xc) = g.next() % 4;
    }
}

// A frame number below limit, whole or not; never NaN, infinite or beyond 2^24 (they floor to 0x80000000
// or far away, and playing from or to there runs up to 2^31 rounds in the original too)
DWORD frame_bits(Gen &g, DWORD limit)
{
    const DWORD f = g.next() % 5 == 0 ? bits((float)(g.next() % limit)) : g.fbits_(0.0f, (float)limit);
    return (f & 0x7fffffffu) >= 0x4b800000u ? bits((float)(limit - 1)) : f;
}
void a_frame_index(Gen &g, DWORD *a)
{
    const DWORD n = fl<DWORD>(obj(O_SELF), 0x48);
    a[0] = g.next() % 5 ? g.next() % n : n + g.next() % 3;
    a[1] = g.fbits_(-10.0f, 10.0f);
    a[2] = g.next();
    a[3] = g.next();
}
void a_sync(Gen &g, DWORD *a)
{
    a[0] = 1 + g.next() % 30;
    a[1] = g.next();
    a[2] = g.fbits_(0.0f, 40.0f);
}
void a_sync_rotation(Gen &g, DWORD *a)
{
    a[0] = 1 + g.next() % 30;
    a[1] = g.next() % 2 ? 0 : g.next();
    a[2] = g.fbits_(0.0f, 40.0f);
    a[3] = g.next();
    a[4] = g.next();
    a[5] = g.next() % 8;
}
void a_strip(Gen &g, DWORD *a)
{
    a[0] = g.next() % 20;
    a[1] = g.next() % 6;
    a[2] = ptrv(extra(3));
}
// from, to within a few rotations' frames (finite); the entity (null only without triggers, as the original
// reads the entity's layer for every trigger)
void a_play(Gen &g, DWORD *a)
{
    const DWORD n = fl<DWORD>(obj(O_SELF), 0x48);
    a[0] = frame_bits(g, 3 * n + 1);
    a[1] = g.next() % 3 ? frame_bits(g, 3 * n + 1) : a[0];
    if (g.next() % 6 == 0) {
        a[2] = 0;
        fl<DWORD>(obj(O_SELF), 0x5c) = 0;
    } else {
        a[2] = ptrv(any_entity(g));
    }
}
void a_play_ratio(Gen &g, DWORD *a)
{
    DWORD b[3];
    a_play(g, b);
    a[0] = g.fbits_(-2.0f, 2.0f);
    a[1] = b[0];
    a[2] = b[1];
    a[3] = b[2];
}

const SlotTest SLOTS[] = {
    {"CRotationSequence_FrameIndex", 0x55fee0, 4, 0xffffffff, 1500, a_frame_index},
    {"Sequence_SyncFrame", 0x55e4b0, SLOT_FASTCALL | 1, 0xffffffff, 400, a_sync},
    {"Sequence_SyncFrameOfRotation", 0x55e500, SLOT_FASTCALL | 4, 0xffffffff, 400, a_sync_rotation},
    {"RotSeq_StripRect", 0x560270, 3, 0xff, 600, a_strip},
    {"CRotationSequence::PlayForward", 0x560300, 3, 0, 1200, a_play},
    {"CRotationSequence::PlayBackward", 0x560480, 3, 0, 1200, a_play},
    {"CRotationSequence::PlayTriggers", 0x560600, 4, 0, 600, a_play_ratio},
};

void test_world(Tally &t)
{
    if (!(g_modules = M_PLAYER, world_on())) {
        t.check(false, "sequence_player", "[the fake world could not be set up]");
        return;
    }
    Policy p{};
    p.find_minus_one = 0;
    for (const SlotTest &st : SLOTS)
        for (unsigned seed = 1; seed <= (unsigned)scaled(st.count); seed++)
            run_slot(t, st, seed * 2654435761u + st.address, make_seq, p);
    world_off();
}

// ---- Real data ----------------------------------------------------------------------------------------------

BYTE *g_parts[8192];

void test_real(Tally &t)
{
    const int n = level_parts(g_parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  sequence_player real data: no level grid found");
        return;
    }
    CClassInfo *animated = at<CClassInfo *(__cdecl *)()>(0x48dcc0)();
    CClassInfo *rotation = at<CClassInfo *(__cdecl *)()>(0x55eae0)();
    BYTE *seen[256];
    int nseen = 0, cases = 0;
    for (int i = 0; i < n && nseen < 256; i++) {
        BYTE *e = g_parts[i];
        if (!(at<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(e, animated) & 0xff))
            continue;
        BYTE *s = at<BYTE *(THISCALL *)(void *)>(0x48b590)(e);
        if (!s || !(at<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(s, rotation) & 0xff))
            continue;
        bool dup = false;
        for (int k = 0; k < nseen && !dup; k++)
            dup = seen[k] == s;
        if (dup)
            continue;
        seen[nseen++] = s;
        const DWORD frames = fl<DWORD>(s, 0x48), rotations = fl<DWORD>(s, 0x4c);
        if (!frames || !rotations || !fl<DWORD>(s, 0x44) || frames * rotations > 4096)
            continue;
        for (DWORD r = 0; r < rotations && r < 32; r++)
            for (int k = 0; k < 4; k++) {
                const DWORD f1 = a_frame(frames), f2 = a_frame(frames);
                DWORD res[2][4];
                for (int side = 0; side < 2; side++) {
                    float o1[2], o2[2], o3[2];
                    if (side == 0)
                        g_hooks.out();
                    at<MotionAtFn>(0x55fb60)(s, o1, (int)r, f1);
                    at<BetweenFn>(0x55fc70)(s, o2, (int)r, f1, f2, (DWORD)(k & 1));
                    at<BetweenFn>(0x55fd30)(s, o3, (int)r, f1, f2, (DWORD)(k - 1));
                    res[side][3] = hash_ld(at<long double(THISCALL *)(void *, DWORD)>(0x55fe90)(s, r)) ^
                                   at<DWORD(THISCALL *)(void *, DWORD)>(0x55fc50)(s, r % (fl<DWORD>(s, 0x54) + 1));
                    if (side == 0)
                        g_hooks.in();
                    res[side][0] = bits(o1[0]) * 31 + bits(o1[1]);
                    res[side][1] = bits(o2[0]) * 31 + bits(o2[1]);
                    res[side][2] = bits(o3[0]) * 31 + bits(o3[1]);
                }
                cases++;
                char d[100];
                wsprintfA(d, "[sequence %x rotation %u: %x %x]", ptrv(s), r, f1, f2);
                t.check(memcmp(res[0], res[1], sizeof res[0]) == 0, "real CRotationSequence", d);
            }
    }
    log("  sequence_player real data: %d parts, %d sequences, %d cases", n, nseen, cases);
}

}  // namespace

ZAX_CHECKS(sequence_player)
{
    (void)cls;
    (void)n;
    Tally direct{"sequence_player: motion, angles, cuts (fakes)"}, world{"sequence_player: frame index, sync, strips, triggers (fake world)"},
        real{"sequence_player: the level's sequences (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    test_direct(direct);
    direct.report();
    test_world(world);
    world.report();
    test_real(real);
    real.report();
    restore_bytes(saved_random);
}

}  // namespace zax
