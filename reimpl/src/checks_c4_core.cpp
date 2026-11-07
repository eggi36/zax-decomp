// Checks of batch C4's core functions (c4_core.cpp) against the original, in the game thread (machinery:
// checks_inventory_common.h; side 0 with the module's hooks out, the C heap from each side's arena).
//  - The dialogue script list's slots on a list of our own (0x140 bytes, a copy of the vtable 0x5ed5f4
//    whose slots 1, 5 and 37 record their calls): "use" on records with random flags, the file of an
//    object (Object_IsKindOf recorded, its al chosen), the entry path, the load of a CPropertyList
//    (CPropertyList::Load recorded; g_LastFileOpened compared and put back), Object_CreateFromFile
//    recorded, and the clear with loaded objects of a recording class.
//  - The exit functions on the real globals: their bytes saved, set to records, data and monitors of our
//    own, the exit run, compared, put back; RecordList_Set, the aliases' destructor, CBlendHueRemap's
//    destructor and the pool's free recorded (nothing of the game's is destroyed).
//  - CConvertWrapper's slot 2 on recording wrappers; the stuck gun command with the application's first
//    character, the class test, the four shooting functions and the error reporter recorded.
#include "checks_inventory_common.h"
#include "def_list_slots.h"
#include "init_list.h"

namespace zax {

int hooks_c4_core(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
DWORD text_of(const char *t) { return t ? fnv_text(2166136261u, t) : 0x0bad; }
const char *zs_text(const void *z) { return (*reinterpret_cast<ZStringBuffer *const *>(z))->text; }

CodePatch g_p[16];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void stand_ins_off()
{
    while (g_np)
        unpatch(g_p[--g_np]);
}
void begin(int side, BYTE pattern)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, pattern);
}
void end(int side)
{
    stand_ins_off();
    arena_off();
    side_end(side);
}

// ---- A dialogue script list of our own ------------------------------------------------------------------

BYTE g_list[2][0x140];
DWORD g_vt[64];
BYTE *g_records[2];
BYTE g_objs[2][8][8];
DWORD lrel(const void *p)
{
    const DWORD v = ptrv(p);
    if (v - ptrv(g_list[g_side]) < 0x140)
        return 0x11000000u + (v - ptrv(g_list[g_side]));
    if (v - ptrv(g_objs[g_side]) < sizeof g_objs[0])
        return 0x0b000000u + (v - ptrv(g_objs[g_side]));
    return rel(v, g_side);
}
const char *THISCALL l_name(void *l, DWORD i)
{
    ev(0x501, lrel(l), i);
    static const char *const N[] = {"Scripts/Intro", "Plain", "A/B/C", "/root", "trail/", "x.RDS"};
    return N[i % 6];
}
const char *THISCALL l_folder(void *l)
{
    ev(0x505, lrel(l));
    return "Data/Dialog";
}
DWORD THISCALL l_create(void *l, const char *dir, const char *name, DWORD *out)
{
    ev(0x537, lrel(l), text_of(dir) ^ text_of(name), lrel(out));
    return 0x20000u + g_nlog[g_side];
}
DWORD THISCALL obj_slot0(void *o, DWORD flags)
{
    ev(0x5a0, D(o, 4), flags);
    return result(0xa0);
}
DWORD g_obj_vt[16];
DWORD g_kind_al;
DWORD THISCALL s_is_kind_of(void *o, CClassInfo *c)
{
    ev(0x590, lrel(o), ptrv(c));
    return result(g_kind_al);
}
void *__fastcall s_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    ev(0x591, text_of(zs_text(path)), flags * 16 + write_back, lrel(out));
    return at<void *>(0x31000 + g_nlog[g_side]);
}
DWORD THISCALL s_load(void *o, ZStringBuffer *arg)
{
    ZString z{arg};
    ev(0x592, lrel(o), text_of(z.c_str()), text_of(zs_text(at<void *>(0x713c14))));
    at<DWORD(THISCALL *)(void *)>(0x4defd0)(&z);   // the callee destroys the argument
    return result(0x92);
}
DWORD THISCALL s_record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u)
{
    ev(0x593, lrel(l), sorted ^ stride ^ num ^ u, lrel(data));
    return result(0x93);
}
DWORD THISCALL s_aliases_dtor(void *a)
{
    ev(0x594, ptrv(a) & 0xfff);
    return result(0x94);
}
DWORD THISCALL monitor_slot7(void *m, DWORD flags)
{
    ev(0x595, D(m, 4), flags);
    return result(0x95);
}

// Records (count in front, from the arena) with random flags and objects of the recording class
BYTE *make_records(int side, DWORD n)
{
    BYTE *block = static_cast<BYTE *>(heap_alloc(4 + n * 0x80));
    D(block, 0) = n;
    BYTE *r = block + 4;
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = r + i * 0x80;
        memset(e, 0, 0x80);
        wsprintfA(reinterpret_cast<char *>(e), "Entry %u", i);
        D(g_objs[side][i], 0) = ptrv(g_obj_vt);
        D(g_objs[side][i], 4) = 0x700 + i;
        D(e, 0x78) = rnd(3) ? ptrv(g_objs[side][i]) : 0;
        e[0x7c] = (BYTE)rnd(2);
        e[0x7d] = (BYTE)rnd(2);
        e[0x7e] = (BYTE)rnd(2);
        e[0x7f] = rnd(5) == 0;
    }
    return r;
}
// A records region of a side made comparable (object pointers to our objects by index)
DWORD records_hash(int side, BYTE *r, DWORD n)
{
    DWORD h = 2166136261u;
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = r + i * 0x80;
        DWORD o = D(e, 0x78);
        if (o - ptrv(g_objs[side]) < sizeof g_objs[side])
            o = 0x0b000000u + (o - ptrv(g_objs[side]));
        h = fnv(h, e, 0x78);
        h = fnv(h, &o, 4);
        h = fnv(h, e + 0x7c, 4);
    }
    return h;
}

void init_list(int side, DWORD n)
{
    memset(g_list[side], 0, 0x140);
    D(g_list[side], 0) = ptrv(g_vt);
    lstrcpyA(reinterpret_cast<char *>(g_list[side] + 0x38), "Data/Dialog");
    g_records[side] = n == 99 ? nullptr : make_records(side, n);
    D(g_list[side], 0x30) = ptrv(g_records[side]);
    D(g_list[side], 0x34) = n == 99 ? 3 : n;
}

void test_list(Tally &t)
{
    memcpy(g_vt, at<void *>(0x5ed5f4), sizeof g_vt);
    g_vt[1] = ptrv((void *)&l_name);
    g_vt[5] = ptrv((void *)&l_folder);
    g_vt[37] = ptrv((void *)&l_create);
    for (int i = 0; i < 16; i++)
        g_obj_vt[i] = ptrv((void *)&obj_slot0);
    // slot 12 "use"
    for (int k = 0; k < scaled(200); k++) {
        const unsigned seed = 11u * (unsigned)k;
        DWORD e[2], h[2];
        DWORD n = 0, index = 0;
        for (int side = 0; side < 2; side++) {
            begin(side, 0x61);
            g_seed = seed;
            n = rnd(7);
            if (rnd(10) == 0)
                n = 99;   // no records
            init_list(side, n);
            index = n && n != 99 ? rnd(n) : 0;
            e[side] = lrel(at<void *>(at<DWORD(THISCALL *)(void *, DWORD)>(0x41ea70)(g_list[side], index)));
            if (e[side] - ptrv(g_records[side]) < 0x400)
                e[side] = 0x0e000000u + (e[side] - ptrv(g_records[side]));
            h[side] = n == 99 ? 0 : records_hash(side, g_records[side], n);
            end(side);
        }
        char d[100];
        wsprintfA(d, "[use seed %u n %u i %u: %x / %x]", seed, n, index, e[0], e[1]);
        t.check(e[0] == e[1] && h[0] == h[1] && same_log(), "use", d);
    }
    // slot 35: the file of an object
    for (int k = 0; k < scaled(120); k++) {
        const unsigned seed = 13u * (unsigned)k + 1;
        DWORD e[2], text[2];
        for (int side = 0; side < 2; side++) {
            ZString z;
            at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
            begin(side, 0x62);
            g_seed = seed;
            const DWORD n = rnd(6);
            init_list(side, n);
            g_kind_al = rnd(4) != 0;
            void *o = n && rnd(3) ? at<void *>(D(g_records[side] + rnd(n) * 0x80, 0x78)) : g_objs[side][7];
            stand_in(0x416790, (void *)&s_is_kind_of);
            e[side] = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x41ec80)(g_list[side], o, &z);
            end(side);
            text[side] = text_of(z.c_str());
            at<DWORD(THISCALL *)(void *)>(0x4defd0)(&z);
        }
        // eax: al and the upper bytes of the class test's result, the count or slot 39's (a ZString's)
        const bool same_eax = (e[0] & 0xff) == (e[1] & 0xff) && ((e[0] & 1) || e[0] == e[1]);
        t.check(same_eax && text[0] == text[1] && same_log(), "file of object");
    }
    // slot 39 and the two slot 37s
    for (DWORD i = 0; i < 8; i++) {
        DWORD text[2], e[2][2];
        for (int side = 0; side < 2; side++) {
            ZString z;
            at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
            begin(side, 0x63);
            init_list(side, 3);
            at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x41ece0)(g_list[side], i, &z);
            stand_in(0x4164c0, (void *)&s_create_from_file);
            void *out = nullptr;
            e[side][0] = ptrv(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x41e9f0)(g_list[side], i & 1 ? "Data/Dialog" : "",
                                                                                                         i & 2 ? "Intro" : "a/b", i & 4 ? &out : nullptr));
            e[side][1] = ptrv(out);
            end(side);
            text[side] = text_of(z.c_str());
            at<DWORD(THISCALL *)(void *)>(0x4defd0)(&z);
        }
        t.check(text[0] == text[1] && memcmp(e[0], e[1], sizeof e[0]) == 0 && same_log(), "entry path, create from file");
    }
    for (DWORD i = 0; i < 4; i++) {
        BYTE obj[2][0x18];
        DWORD e[2], last[2], before = text_of(zs_text(at<void *>(0x713c14)));
        for (int side = 0; side < 2; side++) {
            begin(side, 0x64);
            stand_in(0x41e0f0, (void *)&s_load);
            void *out = at<void *>(0x1234);
            BYTE *o = static_cast<BYTE *>(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x41e870)(
                g_list[side], i & 1 ? "Data/Dialog" : "", "Intro", i & 2 ? &out : nullptr));
            e[side] = (i & 2 ? ptrv(out) == ptrv(o) : ptrv(out) == 0x1234) && o;
            if (o) {
                memcpy(obj[side], o, 0x18);
                D(obj[side], 0x14) = text_of(zs_text(o + 0x14));
                at<DWORD(THISCALL *)(void *)>(0x4defd0)(o + 0x14);
                pool_free(o, 0x18);
            }
            end(side);
            last[side] = text_of(zs_text(at<void *>(0x713c14)));
        }
        t.check(e[0] == e[1] && e[0] && memcmp(obj[0], obj[1], 0x18) == 0 && last[0] == before && last[1] == before && same_log(), "load property list");
    }
    // slot 40: the clear
    for (DWORD n = 0; n < 6; n++) {
        DWORD e[2];
        BYTE copy[2][0x140];
        for (int side = 0; side < 2; side++) {
            begin(side, 0x65);
            g_seed = n;
            init_list(side, n);
            stand_in(0x591cd0, (void *)&s_record_list_set);
            e[side] = at<DWORD(THISCALL *)(void *)>(0x41ed60)(g_list[side]);
            memcpy(copy[side], g_list[side], 0x140);
            end(side);
        }
        t.check(e[0] == e[1] && same_region(copy[0], copy[1], 0x140) && same_log(), "clear");
    }
}

// ---- The exit functions on the real globals --------------------------------------------------------------

SavedBytes g_saved;
DWORD g_monitor_vt[16];
BYTE g_monitor[2][8];

void test_exits(Tally &t)
{
    for (int i = 0; i < 16; i++)
        g_monitor_vt[i] = ptrv((void *)&monitor_slot7);
    // the definition lists: 0x62e7e0 (slot 0 of the objects), 0x713868, 0x713a58 (slot 1)
    struct L {
        DWORD fn, list;
    };
    const L LISTS[] = {{0x41e770, 0x62e7e0}, {0x5880d0, 0x713868}, {0x58feb0, 0x713a58}};
    for (const L &l : LISTS)
        for (DWORD n = 0; n < 6; n++)
            for (int mon = 0; mon < 2; mon++) {
                DWORD e[2];
                BYTE copy[2][0x140];
                for (int side = 0; side < 2; side++) {
                    save_bytes(g_saved, l.list, 0x140);
                    begin(side, 0x71);
                    g_seed = n * 7 + (DWORD)mon;
                    BYTE *g = at<BYTE *>(l.list);
                    g_records[side] = n == 0 ? nullptr : make_records(side, n);
                    D(g, 0x30) = ptrv(g_records[side]);
                    D(g, 0x34) = n;
                    D(g_monitor[side], 0) = ptrv(g_monitor_vt);
                    D(g_monitor[side], 4) = 0x66;
                    D(g, 0x13c) = mon ? ptrv(g_monitor[side]) : 0;
                    stand_in(0x591cd0, (void *)&s_record_list_set);
                    stand_in(0x55d0c0, (void *)&s_aliases_dtor);
                    e[side] = at<DWORD(__cdecl *)()>(l.fn)();
                    memcpy(copy[side], g, 0x140);
                    D(copy[side], 0x13c) = D(copy[side], 0x13c) ? 0xdead : 0;
                    end(side);
                    restore_bytes(g_saved);
                }
                char d[80];
                wsprintfA(d, "[exit %x n %u monitor %d: %x / %x]", l.fn, n, mon, e[0], e[1]);
                t.check(e[0] == e[1] && same_region(copy[0], copy[1], 0x140) && same_log(), "definition list exit", d);
            }
    // the static lists
    const DWORD STATIC[][2] = {{0x419de0, 0x62e6a8}, {0x419e20, 0x62e6d0}, {0x419e60, 0x62e6c0}, {0x41c8c0, 0x62e790}};
    for (const auto &s : STATIC)
        for (int has = 0; has < 2; has++) {
            DWORD e[2];
            BYTE copy[2][0x10];
            for (int side = 0; side < 2; side++) {
                save_bytes(g_saved, s[1], 0x10);
                begin(side, 0x72);
                BYTE *g = at<BYTE *>(s[1]);
                if (s[0] != 0x41c8c0) {
                    D(g, 4) = has ? ptrv(heap_alloc(32)) : 0;
                    D(g, 8) = 3;
                    D(g, 0xc) = 8;
                }
                e[side] = rel(at<DWORD(__cdecl *)()>(s[0])(), side);
                memcpy(copy[side], g, 0x10);
                end(side);
                restore_bytes(g_saved);
            }
            const bool cmp_eax = s[0] != 0x41c8c0;   // 0x41c8c0 leaves the caller's eax
            t.check((!cmp_eax || e[0] == e[1]) && same_region(copy[0], copy[1], 0x10), "static list exit");
        }
}

// ---- The hue remap list's exit with CBlendHueRemap's destructor and the pool recorded ----------------------

DWORD THISCALL s_hue_dtor(void *e)
{
    ev(0x5b0, rel(ptrv(e), g_side));
    return result(0xb0);
}
DWORD THISCALL s_pool_free(void *pool, void *p, DWORD n)
{
    ev(0x5b1, ptrv(pool) != 0, rel(ptrv(p), g_side), n);
    return result(0xb1);
}
void test_hue_exit(Tally &t)
{
    for (DWORD n = 0; n < 6; n++)
        for (int has = 0; has < 2; has++) {
            DWORD e[2];
            BYTE copy[2][0x10];
            for (int side = 0; side < 2; side++) {
                save_bytes(g_saved, 0x6c0510, 0x10);
                begin(side, 0x73);
                BYTE *g = at<BYTE *>(0x6c0510);
                BYTE *data = has ? static_cast<BYTE *>(heap_alloc(4 * 8)) : nullptr;
                for (DWORD i = 0; i < n && data; i++)
                    D(data, i * 4) = i % 3 == 1 ? 0 : ptrv(heap_alloc(0x40));
                D(g, 4) = ptrv(data);
                D(g, 8) = data ? n : 0;
                D(g, 0xc) = 8;
                stand_in(0x43c0b0, (void *)&s_hue_dtor);
                stand_in(0x5065e0, (void *)&s_pool_free);
                e[side] = rel(at<DWORD(__cdecl *)()>(0x4d4be0)(), side);
                memcpy(copy[side], g, 0x10);
                end(side);
                restore_bytes(g_saved);
            }
            char d[80];
            wsprintfA(d, "[hue exit n %u data %d: %x / %x]", n, has, e[0], e[1]);
            // without data (and so no entries) the original leaves the caller's eax
            t.check((!has || e[0] == e[1]) && same_region(copy[0], copy[1], 0x10) && same_log(), "hue remap list exit", d);
        }
}

// ---- CConvertWrapper's slot 2 and the stuck gun command ----------------------------------------------------

DWORD THISCALL w_slot1(void *w, DWORD flags)
{
    ev(0x5c1, D(w, 4), flags);
    return result(0xc1);
}
DWORD g_w_vt[4];
BYTE *g_char;
DWORD g_still, g_waiting, g_kind;
void *THISCALL s_first_character(void *app)
{
    ev(0x5d0, ptrv(app) == ptrv(*at<void **>(0x713f14)));
    return g_char;
}
DWORD THISCALL s_kind(void *o, CClassInfo *c)
{
    ev(0x5d1, ptrv(o) == ptrv(g_char), ptrv(c));
    return result(g_kind);
}
DWORD THISCALL s_still(void *) { ev(0x5d2); return result(g_still); }
DWORD THISCALL s_waiting(void *) { ev(0x5d3); return result(g_waiting); }
DWORD THISCALL s_set_still(void *, DWORD v) { ev(0x5d4, v); return result(0xd4); }
DWORD THISCALL s_set_waiting(void *, DWORD v) { ev(0x5d5, v); return result(0xd5); }
DWORD THISCALL rep_slot15(void *, const char *s)
{
    ev(0x5d6, text_of(s));
    return result(0xd6);
}
DWORD g_rep_vt[32];

void test_others(Tally &t)
{
    for (int i = 0; i < 4; i++)
        g_w_vt[i] = ptrv((void *)&w_slot1);
    for (int k = 0; k < 4; k++) {
        DWORD e[2], inner[2];
        for (int side = 0; side < 2; side++) {
            BYTE w[0x10];
            DWORD held = 0x4242 + (DWORD)k;
            D(w, 0) = ptrv(g_w_vt);
            D(w, 4) = 0x77;
            D(w, 0xc) = ptrv(&held);
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *, void *)>(0x4b8420)(nullptr, w);
            end(side);
            inner[side] = held;
        }
        t.check(e[0] == e[1] && inner[0] == inner[1] && same_log(), "convert wrapper slot 2");
    }
    for (DWORD i = 0; i < 32; i++)
        g_rep_vt[i] = ptrv((void *)&rep_slot15);
    static BYTE character[0x10];
    BYTE reporter[8];
    D(reporter, 0) = ptrv(g_rep_vt);
    for (DWORD k = 0; k < 24; k++) {
        g_char = k % 6 == 0 ? nullptr : character;
        g_kind = (k / 2) % 3 != 0;
        g_still = k & 1;
        g_waiting = (k >> 2) & 1;
        DWORD e[2];
        const DWORD saved = *at<DWORD *>(0x6c02cc);
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            *at<DWORD *>(0x6c02cc) = ptrv(reporter);
            stand_in(0x4f5d80, (void *)&s_first_character);
            stand_in(0x416790, (void *)&s_kind);
            stand_in(0x48ec30, (void *)&s_still);
            stand_in(0x48ebb0, (void *)&s_waiting);
            stand_in(0x48ec40, (void *)&s_set_still);
            stand_in(0x48ebc0, (void *)&s_set_waiting);
            e[side] = at<DWORD(__cdecl *)()>(0x59a600)();
            *at<DWORD *>(0x6c02cc) = saved;
            end(side);
        }
        char d[80];
        wsprintfA(d, "[stuck gun case %u: %x / %x]", k, e[0], e[1]);
        t.check(e[0] == e[1] && same_log(), "stuck gun", d);
    }
}

}  // namespace

ZAX_CHECKS(c4_core)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_c4_core;
    if (!arena_setup()) {
        log("Check c4 core: no arena");
        return;
    }
    Tally list{"c4 core: dialogue script list slots"};
    test_list(list);
    list.report();
    Tally exits{"c4 core: exit functions of the static lists"};
    test_exits(exits);
    test_hue_exit(exits);
    exits.report();
    Tally others{"c4 core: convert wrapper, stuck gun command"};
    test_others(others);
    others.report();
}

}  // namespace zax
