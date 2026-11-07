// Checks of the random generator and the small helpers (random_misc.cpp) against the original, in the
// game thread.
//
// Every case runs twice on the same memory: side 0 with the module's hooks taken out (the original's
// first bytes put back, so nested calls run the original too), side 1 through the reimplementation; the
// state is saved before side 0 and put back before side 1. Compared: eax (st0 as its 80 bits), the
// recorders' logs and the memory afterwards.
//  - The generator on copies of states (seeded with random seeds and drawn from; the real three
//    generators copied in too): Random_Seed, the draws of all kinds (integers below n and between two
//    bounds, floats in a range, centred and towards the ends, with NaN, infinite and reversed bounds),
//    the assignment; the constructor and the two static initializers with the time (0x4d2460) answering
//    a fixed value (the three generators and the bit count table saved and restored).
//  - The colour depth with the real reporter, a fake one and none; the member call; the reinforcements
//    message id; the request (0x4fb9e0 a recorder of the message and the arguments); a definition list's
//    record load on fake lists (slots 1 and 37 recorders, random records).
//  - CRenderEffect and CRenderEffectBehavior: constructors, destructors (in place and freeing), the class
//    getters (ClassInfo_Register a stand-in, the class variable set and 0); the scratch buffer (acquire,
//    release, the static initializer with atexit a stand-in, the exit function); the block reader; the
//    pending value.
//  - The node lists of names: constructors, finds (names, substrings, case, null), the destructors of
//    chains (the deleted nodes recorded); the base; the named value; the handler call; the 0x34-byte
//    object's constructor and destructors with elements that record their deletion.
#include "checks_seq_common.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_random_misc(Hook *out);

namespace {

using seqc::ptrv;
using seqc::fl;
using seqc::hash_ld;

seqc::ModuleHooks g_hooks{"random_misc", hooks_random_misc, {}, {}, {}, 0};

void side_out(int side)
{
    if (side == 0)
        g_hooks.out();
}
void side_in(int side)
{
    if (side == 0)
        g_hooks.in();
}

unsigned g_rs = 4711;
DWORD rnd()
{
    g_rs = g_rs * 1664525u + 1013904223u;
    DWORD x = g_rs ^ (g_rs >> 15);
    x *= 0x2c1b3c6du;
    return x ^ (x >> 12);
}
DWORD rnd_float(float lo, float hi)
{
    const DWORD v = rnd();
    if ((v & 15) == 0) {
        const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xff800000u, 0x3f800000u, 0x501502f9u, 0x00000001u};
        return sp[(v >> 4) & 7];
    }
    const float f = lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
    return bits(f);
}

// ---- The generator ---------------------------------------------------------------------------------------

alignas(16) BYTE g_gen[2][0x400], g_gen0[0x400], g_other[0x400];

using SeedFn = DWORD(THISCALL *)(void *, DWORD);
using BelowFn = DWORD(THISCALL *)(void *, DWORD);
using BetweenFn = DWORD(THISCALL *)(void *, DWORD, DWORD);
using RangeFn = long double(THISCALL *)(void *, DWORD, DWORD);
using AssignFn = DWORD(THISCALL *)(void *, void *);

// One operation on the generator of the side (kind, two values); eax or st0's hash
DWORD gen_op(int kind, BYTE *g, DWORD x, DWORD y)
{
    switch (kind) {
    case 0: return at<BelowFn>(0x55c470)(g, x);
    case 1: return at<BetweenFn>(0x55c4e0)(g, x, y);
    case 2: return hash_ld(at<RangeFn>(0x55c550)(g, x, y));
    case 3: return hash_ld(at<RangeFn>(0x55c5b0)(g, x, y));
    case 4: return hash_ld(at<RangeFn>(0x55c650)(g, x, y));
    case 5: return at<SeedFn>(0x55c390)(g, x);
    default: return at<AssignFn>(0x55c430)(g, g_other) == ptrv(g_other) ? 0x51 : 0x52;
    }
}

void test_generator(Tally &t)
{
    for (int c = 0; c < scaled(400); c++) {
        // the start: a seeded generator, or a copy of one of the game's three, or random bytes
        const DWORD start = rnd() % 4;
        if (start == 0) {
            at<SeedFn>(0x55c390)(g_gen0, rnd());
        } else if (start == 1) {
            const DWORD src[3] = {0x7124c0, 0x6d06c0, 0x712cc0};
            memcpy(g_gen0, at<void *>(src[rnd() % 3]), 0x400);
        } else {
            at<SeedFn>(0x55c390)(g_gen0, rnd() % 16);
            const int draws = (int)(rnd() % 600);
            for (int k = 0; k < draws; k++)
                at<BelowFn>(0x55c470)(g_gen0, 7);
        }
        at<SeedFn>(0x55c390)(g_other, rnd());
        DWORD results[2][24];
        int ops[24];
        DWORD xs[24], ys[24];
        const int n = 4 + (int)(rnd() % 20);
        for (int i = 0; i < n; i++) {
            ops[i] = (int)(rnd() % 13);
            if (ops[i] > 6)
                ops[i] = ops[i] % 3;   // more draws than seeds and assignments
            switch (ops[i]) {
            case 0: xs[i] = rnd() % 4 ? rnd() % 1000 : (rnd() % 2 ? 0 : rnd()); break;
            case 1: xs[i] = rnd() % 200 - 100; ys[i] = rnd() % 4 ? xs[i] + rnd() % 300 : (rnd() % 2 ? xs[i] - 1 : rnd()); break;
            case 5: xs[i] = rnd(); break;
            case 6: break;
            default: xs[i] = rnd_float(-1000.0f, 1000.0f); ys[i] = rnd_float(-1000.0f, 1000.0f); break;
            }
        }
        for (int side = 0; side < 2; side++) {
            memcpy(g_gen[side], g_gen0, 0x400);
            side_out(side);
            for (int i = 0; i < n; i++)
                results[side][i] = gen_op(ops[i], g_gen[side], xs[i], ys[i]);
            side_in(side);
        }
        char d[120];
        int bad = -1;
        for (int i = 0; i < n && bad < 0; i++)
            if (results[0][i] != results[1][i])
                bad = i;
        wsprintfA(d, "[generator case %d: %d operations, first difference %d (kind %d)]", c, n, bad, bad >= 0 ? ops[bad] : -1);
        t.check(bad < 0 && memcmp(g_gen[0], g_gen[1], 0x400) == 0, "Random", d);
    }
}

DWORD g_fake_time;
DWORD __cdecl fake_time() { return g_fake_time; }

// The constructor and the static initializers with a fixed time; the generators and the bit table saved
void test_seeding(Tally &t)
{
    static SavedBytes saved[3];
    save_bytes(saved[0], 0x7124c0, 0x400);
    save_bytes(saved[1], 0x7128c0, 0x400);
    save_bytes(saved[2], 0x712cc0, 0x400);
    static BYTE result[2][0xc00 + 0x400];
    for (int c = 0; c < scaled(30); c++) {
        g_fake_time = c % 5 == 0 ? 0 : rnd();
        const int kind = c % 3;
        DWORD eax[2];
        for (int side = 0; side < 2; side++) {
            restore_bytes(saved[0]);
            restore_bytes(saved[1]);
            restore_bytes(saved[2]);
            memset(at<void *>(0x7128c0), 0xcc, 0x400);   // the table written again
            memset(g_gen[side], 0x5a, 0x400);
            CodePatch p;
            patch_jump(p, 0x4d2460, (void *)&fake_time);
            side_out(side);
            if (kind == 0)
                eax[side] = at<DWORD(THISCALL *)(void *)>(0x55c2c0)(g_gen[side]) == ptrv(g_gen[side]) ? 1 : 2;
            else if (kind == 1)
                eax[side] = at<DWORD(__cdecl *)()>(0x55c100)();
            else
                eax[side] = at<DWORD(__cdecl *)()>(0x55c1e0)();
            side_in(side);
            unpatch(p);
            memcpy(result[side], at<void *>(0x7124c0), 0xc00);
            memcpy(result[side] + 0xc00, g_gen[side], 0x400);
        }
        restore_bytes(saved[0]);
        restore_bytes(saved[1]);
        restore_bytes(saved[2]);
        char d[100];
        wsprintfA(d, "[seeding %d kind %d: eax %x / %x]", c, kind, eax[0], eax[1]);
        t.check(eax[0] == eax[1] && memcmp(result[0], result[1], sizeof result[0]) == 0, "Random seeding", d);
    }
}

// ---- Core helpers ------------------------------------------------------------------------------------------

DWORD g_member_ecx, g_member_calls;
void THISCALL member_recorder(void *self)
{
    g_member_ecx = ptrv(self);
    g_member_calls++;
}

struct Request {
    DWORD ecx, vtable, id, handled, part, value, a[5];
};
Request g_request;
DWORD THISCALL request_recorder(void *part, BYTE *m, DWORD a1, DWORD a2, DWORD a3, DWORD a4)
{
    g_request = {ptrv(part), fl<DWORD>(m, 0), fl<DWORD>(m, 4), fl<BYTE>(m, 8), fl<DWORD>(m, 0xc), fl<DWORD>(m, 0x10), {a1, a2, a3, a4, 0}};
    return 0x5e70a1;
}

// Fake definition lists for 0x55e090: a vtable with recorders in slots 1 and 37
DWORD g_list_vt[64];
unsigned g_list_log;
int g_list_calls;
const char *const LIST_NAMES[] = {"Quips/Hurt", "Hurt", "", "A/B/C"};
const char *THISCALL list_name(void *, DWORD i)
{
    g_list_log = fnv(fnv(g_list_log, "N", 1), &i, 4);
    g_list_calls++;
    return LIST_NAMES[i % 4];
}
DWORD THISCALL list_create(void *l, const char *dir, const char *name, void *out)
{
    g_list_log = fnv_text(fnv_text(fnv(g_list_log, "C", 1), dir), name);
    const DWORD rel = ptrv(out) - ptrv(l);
    g_list_log = fnv(g_list_log, &rel, 4);
    g_list_calls++;
    return 0x7e570000u + (DWORD)g_list_calls;
}
alignas(16) BYTE g_list[0x140], g_list0[0x140];
alignas(16) BYTE g_records[8 * 0x80 + 4], g_records0[8 * 0x80 + 4];

void test_core(Tally &t)
{
    // the colour depth: the real reporter, a fake one, none
    static BYTE fake_reporter[0x40];
    for (int c = 0; c < 3; c++) {
        DWORD r[2];
        const DWORD saved = *at<DWORD *>(0x6c02cc);
        fl<DWORD>(fake_reporter, 0x24) = rnd();
        for (int side = 0; side < 2; side++) {
            if (c == 1)
                *at<DWORD *>(0x6c02cc) = ptrv(fake_reporter);
            else if (c == 2)
                *at<DWORD *>(0x6c02cc) = 0;
            side_out(side);
            r[side] = at<DWORD(__cdecl *)()>(0x55d030)();
            side_in(side);
            *at<DWORD *>(0x6c02cc) = saved;
        }
        char d[80];
        wsprintfA(d, "[colour depth %d: %x / %x]", c, r[0], r[1]);
        t.check(r[0] == r[1], "ColorDepth", d);
    }
    // the member call
    for (int c = 0; c < 4; c++) {
        DWORD r[2], e[2], n[2];
        void *self = at<void *>(0x10000 + rnd() % 0x1000 * 4);
        for (int side = 0; side < 2; side++) {
            g_member_calls = 0;
            side_out(side);
            r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x55d050)(self, (void *)&member_recorder);
            side_in(side);
            e[side] = g_member_ecx;
            n[side] = g_member_calls;
        }
        t.check(r[0] == r[1] && e[0] == e[1] && n[0] == n[1] && n[0] == 1, "CallMember", "");
    }
    // the reinforcements message id
    {
        DWORD r[2], v[2];
        const DWORD saved = *at<DWORD *>(0x7130c0);
        for (int side = 0; side < 2; side++) {
            *at<DWORD *>(0x7130c0) = 0x1234;
            side_out(side);
            r[side] = at<DWORD(__cdecl *)()>(0x55d060)();
            side_in(side);
            v[side] = *at<DWORD *>(0x7130c0);
        }
        *at<DWORD *>(0x7130c0) = saved;
        char d[80];
        wsprintfA(d, "[reinforcements id: %x / %x, %x / %x]", r[0], r[1], v[0], v[1]);
        t.check(r[0] == r[1] && v[0] == v[1], "ReinforcementsMessage_Init", d);
    }
    // the request for reinforcements
    static BYTE part[0x40];
    for (int c = 0; c < scaled(30); c++) {
        Request q[2];
        DWORD r[2];
        fl<DWORD>(part, 0x1c) = rnd();
        const DWORD value = rnd(), radius = rnd_float(0.0f, 900.0f);
        for (int side = 0; side < 2; side++) {
            memset(&g_request, 0, sizeof g_request);
            CodePatch p;
            patch_jump(p, 0x4fb9e0, (void *)&request_recorder);
            side_out(side);
            r[side] = at<DWORD(__fastcall *)(void *, DWORD, DWORD)>(0x55d070)(part, value, radius);
            side_in(side);
            unpatch(p);
            q[side] = g_request;
        }
        t.check(r[0] == r[1] && memcmp(&q[0], &q[1], sizeof q[0]) == 0, "RequestReinforcements", "");
    }
    // a definition list's record load
    for (int s = 0; s < 64; s++)
        g_list_vt[s] = 0;
    g_list_vt[1] = ptrv((void *)&list_name);
    g_list_vt[37] = ptrv((void *)&list_create);
    for (int c = 0; c < scaled(200); c++) {
        memset(g_list0, 0, sizeof g_list0);
        fl<DWORD>(g_list0, 0) = ptrv(g_list_vt);
        lstrcpyA(reinterpret_cast<char *>(g_list0 + 0x38), rnd() % 2 ? "Quips" : (rnd() % 2 ? "" : "Data/Sub"));
        for (DWORD k = 0; k < sizeof g_records0; k++)
            g_records0[k] = (BYTE)rnd();
        for (int r = 0; r < 8; r++) {
            BYTE *rec = g_records0 + 4 + r * 0x80;
            rec[0x7f] = (BYTE)(rnd() % 4 == 0);
            fl<DWORD>(rec, 0x78) = rnd() % 2 ? 0 : rnd();
        }
        fl<DWORD>(g_list0, 0x30) = rnd() % 8 ? ptrv(g_records0 + 4) - ptrv(g_records0) + ptrv(g_records) : 0;
        fl<DWORD>(g_list0, 0x34) = 8;
        const DWORD i = rnd() % 8;
        DWORD r[2];
        unsigned h[2], logs[2];
        for (int side = 0; side < 2; side++) {
            memcpy(g_list, g_list0, sizeof g_list);
            memcpy(g_records, g_records0, sizeof g_records);
            g_list_log = 2166136261u;
            g_list_calls = 0;
            side_out(side);
            r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(g_list, i);
            side_in(side);
            h[side] = fnv(fnv(2166136261u, g_list, sizeof g_list), g_records, sizeof g_records);
            logs[side] = g_list_log;
        }
        char d[100];
        wsprintfA(d, "[record load %d, entry %u: eax %x / %x]", c, i, r[0], r[1]);
        t.check(r[0] == r[1] && h[0] == h[1] && logs[0] == logs[1], "DefinitionList_LoadRecord", d);
    }
}

// ---- CRenderEffect, the scratch buffer, CRenderEffectBehavior --------------------------------------------

void test_render_effect(Tally &t, Tally &getters)
{
    alignas(16) static BYTE o[2][0x20];
    for (int c = 0; c < scaled(20); c++) {
        DWORD r[4][2];
        const BYTE pattern = (BYTE)rnd();
        for (int side = 0; side < 2; side++) {
            memset(o[side], pattern, sizeof o[side]);
            side_out(side);
            r[0][side] = at<DWORD(THISCALL *)(void *)>(0x55e270)(o[side]) - ptrv(o[side]);
            at<void(THISCALL *)(void *)>(0x55e2c0)(o[side]);
            r[1][side] = fl<DWORD>(o[side], 0);
            at<DWORD(THISCALL *)(void *)>(0x55e270)(o[side]);
            r[2][side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55e290)(o[side], 0) - ptrv(o[side]);
            BYTE *b = static_cast<BYTE *>(pool_alloc(8));
            at<DWORD(THISCALL *)(void *)>(0x55e270)(b);
            r[3][side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55e290)(b, 1) - ptrv(b);
            side_in(side);
        }
        t.check(r[0][0] == r[0][1] && r[1][0] == r[1][1] && r[2][0] == r[2][1] && r[3][0] == r[3][1] &&
                    memcmp(o[0], o[1], sizeof o[0]) == 0,
                "CRenderEffect", "");
    }
    // CRenderEffectBehavior: constructed (0x55e400), destroyed in place and freeing
    for (int c = 0; c < scaled(20); c++) {
        DWORD r[3][2];
        const BYTE pattern = (BYTE)rnd();
        for (int side = 0; side < 2; side++) {
            memset(o[side], pattern, sizeof o[side]);
            side_out(side);
            at<DWORD(THISCALL *)(void *)>(0x55e400)(o[side]);
            at<void(THISCALL *)(void *)>(0x55e450)(o[side]);
            r[0][side] = fl<DWORD>(o[side], 0);
            at<DWORD(THISCALL *)(void *)>(0x55e400)(o[side]);
            r[1][side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55e420)(o[side], 0) - ptrv(o[side]);
            BYTE *b = static_cast<BYTE *>(pool_alloc(0xc));
            at<DWORD(THISCALL *)(void *)>(0x55e400)(b);
            r[2][side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x55e420)(b, 1) - ptrv(b);
            side_in(side);
        }
        t.check(r[0][0] == r[0][1] && r[1][0] == r[1][1] && r[2][0] == r[2][1] && memcmp(o[0], o[1], sizeof o[0]) == 0,
                "CRenderEffectBehavior", "");
    }
    // the class getters
    invcheck::g_hook_table = hooks_random_misc;
    const invcheck::ClassDef effect{"CRenderEffect", 8, 0x71311c, 0, 0, 0, {0x55e1c0, 0x55e210}, false};
    const invcheck::ClassDef behavior{"CRenderEffectBehavior", 0xc, 0x713124, 0, 0, 0, {0x55e350, 0x55e3a0}, false};
    for (DWORD g : effect.getters)
        invcheck::test_getter(getters, effect, g);
    for (DWORD g : behavior.getters)
        invcheck::test_getter(getters, behavior, g);
    // the block reader
    for (int c = 0; c < 4; c++) {
        const BYTE pattern = (BYTE)rnd();
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            memset(o[side], pattern, sizeof o[side]);
            side_out(side);
            r[side] = at<DWORD(THISCALL *)(void *)>(0x55e470)(o[side]) - ptrv(o[side]);
            side_in(side);
        }
        t.check(r[0] == r[1] && memcmp(o[0], o[1], sizeof o[0]) == 0, "EffectBlockReader_Ctor", "");
    }
    // the pending value
    {
        const DWORD saved = *at<DWORD *>(0x713128);
        DWORD r[2], v[2];
        const DWORD x = rnd();
        for (int side = 0; side < 2; side++) {
            side_out(side);
            at<void(__fastcall *)(DWORD)>(0x55e490)(x);
            v[side] = *at<DWORD *>(0x713128);
            r[side] = at<DWORD(__cdecl *)()>(0x55e4a0)();
            v[side] = v[side] * 31 + *at<DWORD *>(0x713128);
            side_in(side);
        }
        *at<DWORD *>(0x713128) = saved;
        t.check(r[0] == r[1] && v[0] == v[1], "Pending_Set/Take", "");
    }
    // the scratch buffer: acquire from different states, release; the static initializer and the exit
    static SavedBytes saved;
    save_bytes(saved, 0x713108, 0x14);
    for (int c = 0; c < scaled(60); c++) {
        const DWORD cap = rnd() % 4 ? rnd() % 300 : 0, size = rnd() % 300, in_use = rnd() % 2, n = rnd() % 4 ? rnd() % 700 : cap;
        const bool has_data = rnd() % 4 != 0;
        DWORD state[2][6];
        for (int side = 0; side < 2; side++) {
            void *data = has_data ? heap_alloc(cap + 1) : nullptr;
            *at<DWORD *>(0x713108) = ptrv(data);
            *at<DWORD *>(0x71310c) = cap;
            *at<DWORD *>(0x713110) = size;
            *at<BYTE *>(0x713114) = (BYTE)in_use;
            side_out(side);
            void *r = at<void *(__fastcall *)(DWORD)>(0x55e2f0)(n);
            state[side][0] = r == data ? 1 : (r ? 2 : 3);
            state[side][1] = *at<DWORD *>(0x713108) == ptrv(r);
            state[side][2] = *at<DWORD *>(0x71310c);
            state[side][3] = *at<DWORD *>(0x713110) * 2 + *at<BYTE *>(0x713114);
            state[side][4] = at<DWORD(__cdecl *)()>(0x55e2e0)();
            state[side][5] = *at<DWORD *>(0x713110) * 2 + *at<BYTE *>(0x713114);
            side_in(side);
            if (void *d = *at<void **>(0x713108))
                heap_free(d);
        }
        restore_bytes(saved);
        char d[100];
        wsprintfA(d, "[buffer %d: cap %u size %u for %u: %x %x %x / %x %x %x]", c, cap, size, n, state[0][0], state[0][2], state[0][3],
                  state[1][0], state[1][2], state[1][3]);
        t.check(memcmp(state[0], state[1], sizeof state[0]) == 0, "RenderBuffer", d);
    }
    for (int c = 0; c < 4; c++) {
        DWORD state[2][5];
        for (int side = 0; side < 2; side++) {
            for (DWORD k = 0; k < 0x12; k++)
                at<BYTE *>(0x713108)[k] = (BYTE)(0x40 + k);
            g_atexit_calls = 0;
            g_atexit_function = 0;
            CodePatch p;
            patch_jump(p, 0x5d0863, (void *)&fake_atexit);
            side_out(side);
            state[side][0] = at<DWORD(__cdecl *)()>(0x55e150)();
            side_in(side);
            unpatch(p);
            state[side][1] = fnv(2166136261u, at<void *>(0x713108), 0x12);
            state[side][2] = g_atexit_calls * 0x1000000 + g_atexit_function;
            // the exit function on a buffer with data, with the guard set and clear
            *at<BYTE *>(0x713119) = (BYTE)(c & 1);
            *at<void **>(0x713108) = c & 2 ? heap_alloc(16) : nullptr;
            side_out(side);
            at<void(__cdecl *)()>(0x55e180)();
            side_in(side);
            state[side][3] = *at<BYTE *>(0x713119);
            state[side][4] = *at<DWORD *>(0x713108) ? 1 : 0;
            if (*at<void **>(0x713108))
                heap_free(*at<void **>(0x713108));
        }
        restore_bytes(saved);
        state[0][0] = state[1][0] = 0;   // atexit's eax is the stand-in's 0 on both
        t.check(memcmp(state[0], state[1], sizeof state[0]) == 0, "RenderBuffer_Init/Exit", "");
    }
}

// ---- The node lists, the named value, the handler, the 0x34-byte object ------------------------------------

const char *const NODE_NAMES[] = {"Alpha", "beta", "GammaRay", "ALPHA", "", nullptr, "Delta Force", "ray"};
// A chain of n nodes of the kind (0: A, 1: B) made through the side's constructor, from the last
BYTE *make_chain(int kind, int n, const int *names)
{
    BYTE *next = nullptr;
    for (int i = n; i--;) {
        BYTE *o = static_cast<BYTE *>(heap_alloc(0xc));
        at<BYTE *(THISCALL *)(void *, const char *, void *)>(kind ? 0x51eb40 : 0x51ea10)(o, NODE_NAMES[names[i]], next);
        next = o;
    }
    return next;
}
unsigned chain_hash(BYTE *o)
{
    unsigned h = 2166136261u;
    for (; o; o = at<BYTE *>(fl<DWORD>(o, 8))) {
        h = fnv(h, o, 4);
        const char *name = at<const char *>(fl<DWORD>(o, 4));
        h = name ? fnv_text(h, name) : fnv(h, "-", 1);
    }
    return h;
}
int chain_index(BYTE *head, BYTE *o)
{
    int i = 0;
    for (; head; head = at<BYTE *>(fl<DWORD>(head, 8)), i++)
        if (head == o)
            return i;
    return o ? -2 : -1;
}

DWORD g_handler_ecx, g_handler_arg;
DWORD THISCALL handler_recorder(void *self, DWORD a)
{
    g_handler_ecx = ptrv(self);
    g_handler_arg = a;
    return a ^ 0x5a5a;
}

void test_nodes(Tally &t)
{
    for (int c = 0; c < scaled(150); c++) {
        const int kind = (int)(rnd() % 2), n = (int)(rnd() % 5);
        // a null name only without a find (the find reads every name)
        const bool with_null = rnd() % 4 == 0;
        int names[5];
        for (int i = 0; i < 5; i++) {
            names[i] = (int)(rnd() % 8);
            if (names[i] == 5 && !with_null)
                names[i] = 6;
        }
        const char *const finds[] = {"Alpha", "alpha", "ray", "x", "", "a"};
        const char *find = finds[rnd() % 6];
        unsigned h[2];
        int found[2];
        DWORD del[2], deleted[2];
        for (int side = 0; side < 2; side++) {
            side_out(side);
            BYTE *head = make_chain(kind, n, names);
            h[side] = chain_hash(head);
            found[side] = with_null ? -3 : chain_index(head, at<BYTE *(THISCALL *)(void *, const char *)>(0x51eb10)(head, find));
            if (head) {
                // the deleting destructor with 1 frees the chain; with 0 the head stays (then freed)
                const DWORD flags = (DWORD)(c & 1);
                del[side] = at<DWORD(THISCALL *)(void *, DWORD)>(kind ? 0x51ebb0 : 0x51ea80)(head, flags) - ptrv(head);
                deleted[side] = flags ? 0 : fl<DWORD>(head, 0) + fl<DWORD>(head, 4) * 3 + fl<DWORD>(head, 8) * 7;
                if (!flags)
                    heap_free(head);
            } else {
                del[side] = deleted[side] = 0;
            }
            side_in(side);
        }
        char d[120];
        wsprintfA(d, "[nodes %d kind %d n %d: find %d / %d, %x / %x]", c, kind, n, found[0], found[1], h[0], h[1]);
        t.check(h[0] == h[1] && found[0] == found[1] && del[0] == del[1] && deleted[0] == deleted[1], "NetNameNode", d);
    }
    // the plain destructors on a node with a chain behind it
    for (int c = 0; c < scaled(20); c++) {
        const int kind = (int)(rnd() % 2);
        int names[5];
        for (int i = 0; i < 5; i++)
            names[i] = (int)(rnd() % 8);
        DWORD s[2][3];
        const int len = 1 + (int)(rnd() % 3);
        for (int side = 0; side < 2; side++) {
            side_out(side);
            BYTE *head = make_chain(kind, len, names);
            at<void(THISCALL *)(void *)>(kind ? 0x51ec00 : 0x51ead0)(head);
            s[side][0] = fl<DWORD>(head, 0);
            s[side][1] = fl<DWORD>(head, 4);
            s[side][2] = fl<DWORD>(head, 8);
            heap_free(head);
            side_in(side);
        }
        t.check(memcmp(s[0], s[1], sizeof s[0]) == 0, "NetNameNode_Dtor", "");
    }
    // the base: constructor and deleting destructor
    {
        alignas(4) static BYTE b[2][8];
        DWORD r[2][3];
        for (int side = 0; side < 2; side++) {
            memset(b[side], 0x77, 8);
            side_out(side);
            at<void(THISCALL *)(void *)>(0x51ec40)(b[side]);   // returns nothing (eax as it was)
            r[side][0] = fl<DWORD>(b[side], 0);
            r[side][1] = at<DWORD(THISCALL *)(void *, DWORD)>(0x51ec50)(b[side], 0) - ptrv(b[side]);
            BYTE *h = static_cast<BYTE *>(heap_alloc(4));
            r[side][2] = at<DWORD(THISCALL *)(void *, DWORD)>(0x51ec50)(h, 1) - ptrv(h);
            side_in(side);
        }
        t.check(memcmp(r[0], r[1], sizeof r[0]) == 0 && memcmp(b[0], b[1], 8) == 0, "NetNameNodeBase", "");
    }
    // the named value
    for (int c = 0; c < 3; c++) {
        BYTE v[2][0xc];
        for (int side = 0; side < 2; side++) {
            side_out(side);
            BYTE *o = at<BYTE *(__cdecl *)()>(0x51ec70)();
            memcpy(v[side], o, 0xc);
            vcall<void *>(o, 0, (DWORD)1);   // its deleting destructor 0x51f720
            side_in(side);
        }
        t.check(memcmp(v[0], v[1], 0xc) == 0, "NamedValue_New", "");
    }
    // the handler: set, called with ecx and an argument
    {
        const DWORD saved = *at<DWORD *>(0x6d00a8);
        for (int c = 0; c < 6; c++) {
            DWORD r[2][4];
            void *self = at<void *>(0x20000 + rnd() % 0x1000 * 4);
            const DWORD a = rnd();
            for (int side = 0; side < 2; side++) {
                side_out(side);
                at<void(__fastcall *)(DWORD)>(0x51eca0)(ptrv((void *)&handler_recorder));
                r[side][0] = *at<DWORD *>(0x6d00a8);
                r[side][1] = at<DWORD(THISCALL *)(void *, DWORD)>(0x51ec90)(self, a);
                r[side][2] = g_handler_ecx;
                r[side][3] = g_handler_arg;
                side_in(side);
            }
            t.check(memcmp(r[0], r[1], sizeof r[0]) == 0, "NetHandler", "");
        }
        *at<DWORD *>(0x6d00a8) = saved;
    }
    // the 0x34-byte object: constructor on a pattern; destructors with elements that record their deletion
    init_fake_entries();
    for (int c = 0; c < scaled(60); c++) {
        alignas(4) static BYTE h[2][0x40], owner[2][0x10];
        const BYTE pattern = (BYTE)rnd();
        const DWORD n1 = rnd() % 4, n2 = rnd() % 4;
        const int which = c % 3;
        unsigned del[2];
        DWORD r[2], own[2];
        for (int side = 0; side < 2; side++) {
            memset(h[side], pattern, sizeof h[side]);
            memset(owner[side], 0x33, sizeof owner[side]);
            side_out(side);
            r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x51ecb0)(h[side], owner[side]) - ptrv(h[side]);
            // the arrays get elements (data from the heap, capacity one more)
            DWORD *d1 = static_cast<DWORD *>(heap_alloc((n1 + 1) * 4)), *d2 = static_cast<DWORD *>(heap_alloc((n2 + 1) * 4));
            for (DWORD i = 0; i < n1; i++)
                d1[i] = i == 1 ? 0 : ptrv(&g_fake_entries[i]);
            for (DWORD i = 0; i < n2; i++)
                d2[i] = ptrv(&g_fake_entries[8 + i]);
            fl<DWORD>(h[side], 0x14) = ptrv(d1);
            fl<DWORD>(h[side], 0x18) = n1;
            fl<DWORD>(h[side], 0x1c) = n1 + 1;
            fl<DWORD>(h[side], 0x24) = ptrv(d2);
            fl<DWORD>(h[side], 0x28) = n2;
            fl<DWORD>(h[side], 0x2c) = n2 + 1;
            if (which == 2)
                fl<DWORD>(owner[side], 4) = 0;   // the owner points elsewhere
            g_num_deleted = 0;
            if (which == 0)
                at<void(THISCALL *)(void *)>(0x51ee30)(h[side]);
            else
                at<DWORD(THISCALL *)(void *, DWORD)>(0x51ed00)(h[side], 0);
            del[side] = deleted_hash();
            own[side] = fl<DWORD>(owner[side], 4) == ptrv(h[side]) ? 1 : fl<DWORD>(owner[side], 4);
            side_in(side);
        }
        char d[100];
        wsprintfA(d, "[holder %d: %u / %u elements, kind %d]", c, n1, n2, which);
        bool same = r[0] == r[1] && del[0] == del[1] && own[0] == own[1];
        for (DWORD off = 0; off < 0x34 && same; off += 4)
            same = off == 4 ? fl<DWORD>(h[0], 4) - ptrv(owner[0]) == fl<DWORD>(h[1], 4) - ptrv(owner[1]) : fl<DWORD>(h[0], off) == fl<DWORD>(h[1], off);
        t.check(same, "NetHolder", d);
    }
}

}  // namespace

ZAX_CHECKS(random_misc)
{
    (void)cls;
    (void)n;
    Tally random{"random_misc: the random generator"}, seeding{"random_misc: seeding and the static initializers"},
        core{"random_misc: colour depth, member call, reinforcements, record load"}, effects{"random_misc: render effects, buffer, reader"},
        getters{"random_misc: class getters"}, nodes{"random_misc: name nodes, named value, handler, holder"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    test_generator(random);
    random.report();
    test_seeding(seeding);
    seeding.report();
    test_core(core);
    core.report();
    test_render_effect(effects, getters);
    effects.report();
    getters.report();
    test_nodes(nodes);
    nodes.report();
    restore_bytes(saved_random);
}

}  // namespace zax
