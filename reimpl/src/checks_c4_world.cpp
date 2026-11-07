// Checks of batch C4's small world functions (c4_world.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h; side 0 with the module's hooks out, the C heap from each side's
// arena). The exe functions the code calls are recording stand-ins (the pool's free too); globals the
// functions change are saved, set to values of the case and put back; objects are blocks of our own with
// recording vtables. The float setter 0x4e6c50 is compared by eax and the value it leaves on the FPU stack.
#include "checks_inventory_common.h"
#include "def_list_slots.h"
#include "init_list.h"

namespace zax {

int hooks_c4_world(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }

CodePatch g_p[16];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void begin(int side, BYTE pattern)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, pattern);
}
void end(int side)
{
    while (g_np)
        unpatch(g_p[--g_np]);
    arena_off();
    side_end(side);
}
BYTE g_obj[2][0x200];
DWORD orel(DWORD v)
{
    const DWORD b = ptrv(g_obj[g_side]);
    return v - b < sizeof g_obj[0] ? 0x0b000000u + (v - b) : rel(v, g_side);
}
template <DWORD A> DWORD THISCALL t0(void *s) { ev(A, orel(ptrv(s))); return result(A & 0xff); }
template <DWORD A> DWORD THISCALL t1(void *s, DWORD a) { ev(A, orel(ptrv(s)), orel(a)); return result(A & 0xff); }
template <DWORD A> DWORD THISCALL t2(void *s, DWORD a, DWORD b) { ev(A, orel(ptrv(s)), orel(a), orel(b)); return result(A & 0xff); }
template <DWORD A> DWORD __fastcall f2(DWORD c, DWORD d) { ev(A, orel(c), orel(d)); return result(A & 0xff); }
template <DWORD A> DWORD STDCALL s1(DWORD a) { ev(A, orel(a)); return result(A & 0xff); }
DWORD g_vt[128];

// ---- Wrappers, readers, small slots ------------------------------------------------------------------------

void *g_made;
DWORD THISCALL s_pending(void *o) { g_made = o; ev(0x601, D(o, 0)); return result(1); }

void test_small(Tally &t)
{
    for (int i = 0; i < 128; i++)
        g_vt[i] = ptrv((void *)&t1<0x100>);
    g_vt[63] = ptrv((void *)&t1<0x163>);
    // the two converting wrappers
    for (int k = 0; k < 4; k++) {
        DWORD e[2][3], made[2][4];
        for (int side = 0; side < 2; side++) {
            begin(side, 0x60);
            g_made = nullptr;
            stand_in(0x41a6b0, (void *)&t0<0x602>);   // CAction::Ctor
            stand_in(0x4173c0, (void *)&t0<0x603>);   // Object_InitFields
            stand_in(*at<DWORD *>(0x5ed710 + 19 * 4), (void *)&t0<0x604>);
            BYTE *a = static_cast<BYTE *>(at<void *(THISCALL *)(void *, DWORD)>(0x41f9c0)(nullptr, 0x41f9 + (DWORD)k));
            BYTE *w = g_obj[side];
            D(w, 0) = ptrv(g_vt);
            D(w, 8) = 0x5ed800 + (DWORD)k;
            e[side][0] = at<DWORD(THISCALL *)(void *, void *)>(0x41fb50)(nullptr, w);
            e[side][1] = D(w, 8);
            e[side][2] = at<DWORD(__fastcall *)(void *, DWORD, DWORD)>(0x4ed6d0)(w, 0x63, 7);
            end(side);
            for (int i = 0; i < 3; i++)
                made[side][i] = D(a, 4 * (DWORD)i);
            made[side][3] = fnv_text(2166136261u, (*(ZStringBuffer **)(a + 0xc))->text);
            at<DWORD(THISCALL *)(void *)>(0x4defd0)(a + 0xc);
            pool_free(a, 0x10);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && memcmp(made[0], made[1], sizeof made[0]) == 0 && same_log(), "converting wrappers");
    }
    // CAction's slot 26, the model's sequence names, the block reader base's destructor
    for (int k = 0; k < 6; k++) {
        DWORD e[2][6];
        BYTE copy[2][0x20];
        for (int side = 0; side < 2; side++) {
            begin(side, 0x61);
            BYTE *o = g_obj[side];
            memset(o, 0, 0x80);
            D(o, 4) = ptrv(o + 0x40);
            D(o + 0x40, 0x48) = (DWORD)k;
            stand_in(0x517200, (void *)&t1<0x605>);   // CModel::SequenceName
            stand_in(0x54d130, (void *)&f2<0x606>);   // the pool's free
            e[side][0] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4fc960)(o, 1, 2, 3, 4, 5);
            e[side][1] = at<DWORD(THISCALL *)(void *, DWORD)>(0x5182a0)(o, (DWORD)k / 2 + 1);
            e[side][2] = at<DWORD(THISCALL *)(void *)>(0x5182c0)(o);
            e[side][3] = orel(at<DWORD(THISCALL *)(void *, DWORD)>(0x518270)(o + 0x10, (DWORD)k & 1));
            BYTE *h = static_cast<BYTE *>(heap_alloc(8));
            e[side][4] = orel(at<DWORD(THISCALL *)(void *, DWORD)>(0x5718b0)(k & 1 ? h : o + 0x18, (DWORD)k & 1));
            e[side][5] = 0;
            memcpy(copy[side], o, 0x20);
            D(copy[side], 4) = orel(D(copy[side], 4));
            end(side);
        }
        char d[120];
        wsprintfA(d, "[k %d: %x %x %x %x %x / %x %x %x %x %x, copy %d]", k, e[0][0], e[0][1], e[0][2], e[0][3], e[0][4], e[1][0], e[1][1], e[1][2],
                  e[1][3], e[1][4], memcmp(copy[0], copy[1], 0x20));
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && memcmp(copy[0], copy[1], 0x20) == 0 && same_log(), "small slots", d);
        if (!same_log())
            log_first_difference();
    }
    // the warp effect's reader (the made object freed to the pool afterwards) and the "AI" field
    for (int k = 0; k < 3; k++) {
        DWORD e[2][2], made[2][10];
        for (int side = 0; side < 2; side++) {
            begin(side, 0x62);
            g_made = nullptr;
            stand_in(0x596490, (void *)&t0<0x607>);   // CWarpEffect::Ctor
            stand_in(0x4173c0, (void *)&t0<0x608>);   // Object_InitFields
            stand_in(0x55e490, (void *)&s_pending);
            stand_in(*at<DWORD *>(0x60049c + 24 * 4), (void *)&t1<0x609>);
            stand_in(0x419030, (void *)&t1<0x60a>);   // FieldDesc_SetAltName
            stand_in(0x418fd0, (void *)&t1<0x60b>);   // FieldDesc_AddToClass
            e[side][0] = at<DWORD(THISCALL *)(void *, void *)>(0x571840)(nullptr, g_obj[side]);
            e[side][1] = at<DWORD(__cdecl *)()>(0x516d70)();
            end(side);
            BYTE *o = static_cast<BYTE *>(g_made);
            for (DWORD i = 0; i < 10; i++)
                made[side][i] = o ? D(o, 4 * i + 8) : 0;
            made[side][7] &= 0xff;   // +0x24 a byte
            if (o)
                pool_free(o, 0x30);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && memcmp(made[0], made[1], sizeof made[0]) == 0 && same_log(), "warp reader, AI field");
    }
}

// ---- The float setter ---------------------------------------------------------------------------------------

void test_setter(Tally &t)
{
    for (int k = 0; k < 8; k++) {
        BYTE out[2][16];
        DWORD eax[2], field[2];
        const DWORD v = k & 1 ? 0x3f800000u + (DWORD)k : (DWORD)k * 0x01010101u;
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            BYTE *o = g_obj[side];
            D(o, 0x9c) = 0;
            stand_in(0x57aef0, (void *)&s1<0x701>);   // ResourceCache_Get
            stand_in(0x57c210, (void *)&t0<0x702>);   // Listener_Set713698
            long double r;
            DWORD e;
            void *fn = at<void *>(0x4e6c50);
            __asm__ volatile("pushl %[v]\n\tcall *%[f]" : "=t"(r), "=a"(e), "+c"(o) : [f] "r"(fn), [v] "r"(v) : "edx", "memory");
            end(side);
            memset(out[side], 0, 16);
            memcpy(out[side], &r, 10);
            eax[side] = e;
            field[side] = D(g_obj[side], 0x9c);
        }
        t.check(memcmp(out[0], out[1], 16) == 0 && eax[0] == eax[1] && field[0] == field[1] && same_log(), "float setter");
    }
}

// ---- The exits and the "Top Hide" filter ------------------------------------------------------------------------

DWORD THISCALL s_pool_free(void *pool, void *p, DWORD n)
{
    ev(0x801, pool != nullptr, rel(ptrv(p), g_side), n);
    return result(1);
}
DWORD THISCALL s_record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u)
{
    ev(0x802, ptrv(l), sorted ^ stride ^ num ^ u, ptrv(data));
    return result(2);
}
DWORD THISCALL s_aliases_dtor(void *a) { ev(0x803, ptrv(a)); return result(3); }
DWORD THISCALL obj_slot(void *o, DWORD flags) { ev(0x804, orel(ptrv(o)), flags); return result(4); }
DWORD g_find;
DWORD THISCALL s_find(void *l, const char *name, DWORD hint)
{
    ev(0x805, ptrv(l), fnv_text(2166136261u, name), hint);
    return g_find;
}
DWORD g_kind;
DWORD THISCALL s_kind(void *o, void *c) { ev(0x806, orel(ptrv(o)), ptrv(c)); return result(g_kind); }

void test_exits(Tally &t)
{
    struct A {
        DWORD fn, array;
    };
    const A ARRAYS[] = {{0x4e88b0, 0x6c1c50}, {0x4ea610, 0x6c1bd8}, {0x4f39a0, 0x6c26f0}};
    for (const A &a : ARRAYS)
        for (int has = 0; has < 2; has++) {
            DWORD e[2];
            BYTE copy[2][0x10];
            for (int side = 0; side < 2; side++) {
                SavedBytes saved;
                save_bytes(saved, a.array, 0x10);
                begin(side, 0x70);
                BYTE *g = at<BYTE *>(a.array);
                D(g, 4) = has ? ptrv(heap_alloc(64)) : 0;
                D(g, 8) = 3;
                D(g, 0xc) = 4;
                stand_in(0x5065e0, (void *)&s_pool_free);
                e[side] = rel(at<DWORD(__cdecl *)()>(a.fn)(), side);
                memcpy(copy[side], g, 0x10);
                end(side);
                restore_bytes(saved);
            }
            t.check((!has || e[0] == e[1]) && same_region(copy[0], copy[1], 0x10) && same_log(), "pool array exit");
        }
    // the difficulty list's exit with records, objects and a monitor of our own
    static DWORD obj_vt[16];
    for (int i = 0; i < 16; i++)
        obj_vt[i] = ptrv((void *)&obj_slot);
    for (DWORD n = 0; n < 5; n++) {
        DWORD e[2];
        BYTE copy[2][0x140];
        for (int side = 0; side < 2; side++) {
            SavedBytes saved;
            save_bytes(saved, 0x6c2a00, 0x140);
            begin(side, 0x71);
            BYTE *l = at<BYTE *>(0x6c2a00);
            BYTE *records = nullptr;
            if (n) {
                BYTE *block = static_cast<BYTE *>(heap_alloc(4 + n * 0x80));
                D(block, 0) = n;
                records = block + 4;
                memset(records, 0, n * 0x80);
                for (DWORD i = 0; i < n; i++) {
                    D(g_obj[side] + i * 8, 0) = ptrv(obj_vt);
                    D(records + i * 0x80, 0x78) = i & 1 ? 0 : ptrv(g_obj[side] + i * 8);
                }
            }
            D(l, 0x30) = ptrv(records);
            D(l, 0x34) = n;
            D(g_obj[side] + 0x100, 0) = ptrv(obj_vt);
            D(l, 0x13c) = n & 1 ? ptrv(g_obj[side] + 0x100) : 0;
            stand_in(0x591cd0, (void *)&s_record_list_set);
            stand_in(0x55d0c0, (void *)&s_aliases_dtor);
            e[side] = at<DWORD(__cdecl *)()>(0x4fd650)();
            memcpy(copy[side], l, 0x140);
            end(side);
            restore_bytes(saved);
        }
        t.check(e[0] == e[1] && same_region(copy[0], copy[1], 0x140) && same_log(), "difficulty list exit");
    }
    // "Top Hide": the guard bits in every state, Find giving -1, 0, 5 and 31
    for (int k = 0; k < 32; k++) {
        DWORD e[2], g[2][3];
        const DWORD saved[3] = {*at<BYTE *>(0x6c1cd8), *at<DWORD *>(0x6c1bec), *at<DWORD *>(0x6c1c44)};
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            *at<BYTE *>(0x6c1cd8) = (BYTE)(k & 3) | 0x80;
            *at<DWORD *>(0x6c1bec) = 7;
            *at<DWORD *>(0x6c1c44) = 0x55;
            static const DWORD FINDS[] = {0xffffffff, 0, 5, 31};
            g_find = FINDS[(k >> 2) & 3];
            g_kind = (k >> 4) & 1;
            BYTE *part = k % 3 == 0 ? nullptr : g_obj[side];
            if (part)
                D(part, 0x1c) = k & 8 ? 0x20 : 1;
            stand_in(0x523df0, (void *)&s_find);
            stand_in(0x416790, (void *)&s_kind);
            e[side] = at<DWORD(THISCALL *)(void *)>(0x4ed2b0)(part);
            g[side][0] = *at<BYTE *>(0x6c1cd8);
            g[side][1] = *at<DWORD *>(0x6c1bec);
            g[side][2] = *at<DWORD *>(0x6c1c44);
            end(side);
        }
        *at<BYTE *>(0x6c1cd8) = (BYTE)saved[0];
        *at<DWORD *>(0x6c1bec) = saved[1];
        *at<DWORD *>(0x6c1c44) = saved[2];
        t.check(e[0] == e[1] && memcmp(g[0], g[1], sizeof g[0]) == 0 && same_log(), "top hide filter");
    }
}

}  // namespace

ZAX_CHECKS(c4_world)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_c4_world;
    if (!arena_setup()) {
        log("Check c4 world: no arena");
        return;
    }
    Tally small{"c4 world: wrappers, readers, small slots"};
    test_small(small);
    test_setter(small);
    small.report();
    Tally exits{"c4 world: exits, the top hide filter"};
    test_exits(exits);
    exits.report();
}

}  // namespace zax
