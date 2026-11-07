// Checks of batch C4's small CZaxWorld slots and the difficulty list's slots 37/39 (c4_app.cpp) against
// the original, in the game thread (machinery: checks_inventory_common.h; side 0 with the module's hooks
// out). The application is a block of our own with a recording vtable; the exe functions the slots call
// are recording stand-ins; the globals they read or count (0x713670, 0x713f1c, 0x714558, 0x713f18) are
// saved, set to values of the case and put back. Slots that set only al (24, 80 with the flag set) are
// compared by al; slot 59 without a list and slot 26 without a main menu leave the caller's eax.
#include "checks_inventory_common.h"
#include "def_list_slots.h"
#include "init_list.h"

namespace zax {

int hooks_c4_app(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }

CodePatch g_p[16];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void begin(int side)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, 0x77);
}
void end(int side)
{
    while (g_np)
        unpatch(g_p[--g_np]);
    arena_off();
    side_end(side);
}
BYTE g_obj[2][0x800];
DWORD orel(DWORD v)
{
    const DWORD b = ptrv(g_obj[g_side]);
    return v - b < sizeof g_obj[0] ? 0x0b000000u + (v - b) : rel(v, g_side);
}
DWORD g_ret_al;
template <DWORD A> DWORD THISCALL t0(void *s) { ev(A, orel(ptrv(s))); return result(g_ret_al); }
template <DWORD A> DWORD THISCALL t1(void *s, DWORD a) { ev(A, orel(ptrv(s)), orel(a)); return result(g_ret_al); }
template <DWORD A> DWORD THISCALL t3(void *s, DWORD a, DWORD b, DWORD c) { ev(A, orel(ptrv(s)), orel(a), orel(b) ^ (c << 8)); return result(1); }
template <DWORD A> DWORD __cdecl c0() { ev(A); return result(g_ret_al); }
DWORD __fastcall s_globals3(DWORD a, DWORD b, DWORD c) { ev(0x901, a, b, c); return result(1); }
void *g_ret_ptr;
template <DWORD A> void *THISCALL p0(void *s) { ev(A, orel(ptrv(s))); return g_ret_ptr; }
template <DWORD A> void *THISCALL p1(void *s, DWORD a) { ev(A, orel(ptrv(s)), orel(a)); return g_ret_ptr; }

DWORD g_vt[64];
void *THISCALL net_game(void *s) { ev(0x801, orel(ptrv(s))); return g_obj[g_side] + 0x7c0; }

void test_slots(Tally &t)
{
    for (int i = 0; i < 64; i++)
        g_vt[i] = ptrv((void *)&t0<0x800>);
    g_vt[1] = ptrv((void *)&net_game);
    g_vt[13] = ptrv((void *)&t0<0x80d>);
    g_vt[27] = ptrv((void *)&t0<0x81b>);
    for (int k = 0; k < scaled(96); k++) {
        const unsigned seed = (unsigned)k * 101u;
        DWORD e[2][16];
        const DWORD saved[4] = {*at<DWORD *>(0x713670), *at<DWORD *>(0x713f1c), *at<DWORD *>(0x714558), *at<DWORD *>(0x713f18)};
        for (int side = 0; side < 2; side++) {
            begin(side);
            g_seed = seed;
            BYTE *app = g_obj[side];
            memset(app, 0, 0x600);
            D(app, 0) = ptrv(g_vt);
            BYTE *data = app + 0x600;
            D(data, 8) = 0x5a0750;
            D(data, 0x2c) = rnd(2);
            D(app, 0x34c) = ptrv(data);
            BYTE *table = app + 0x680;
            D(table, 0x100) = rnd(2);
            D(app, 0x340) = ptrv(table);
            D(app, 0x354) = rnd(2) ? ptrv(app + 0x700) : 0;
            app[0x310] = (BYTE)rnd(2);
            app[0x591] = (BYTE)rnd(256);
            BYTE *menu = app + 0x740;
            D(menu, 0) = ptrv(g_vt);
            *at<DWORD *>(0x714558) = rnd(2) ? ptrv(menu) : 0;
            *at<DWORD *>(0x713670) = 0x1367;
            *at<DWORD *>(0x713f1c) = 41;
            *at<DWORD *>(0x713f18) = 0x3f18;
            BYTE *message = app + 0x780;
            D(message, 4) = rnd(2) ? 0x1367 : 0x1368;
            g_ret_al = rnd(2);
            g_ret_ptr = rnd(4) ? app + 0x7c0 : nullptr;   // the net game / the character / the inventory / the item
            stand_in(0x41f660, (void *)&t0<0x902>);   // COtherMapList::Clear
            stand_in(0x4f5110, (void *)&t0<0x903>);   // World::GameStarted
            stand_in(0x40e9c0, (void *)&s_globals3);
            stand_in(0x4cdf20, (void *)&t1<0x904>);   // App::No
            stand_in(0x4f5d80, (void *)&p0<0x905>);   // World::FirstCharacter
            stand_in(0x4267e0, (void *)&p0<0x906>);   // LayerPart_GetInventory
            stand_in(0x425860, (void *)&p1<0x907>);   // CInventory_FindOfClass
            stand_in(0x416790, (void *)&t1<0x908>);   // Object_IsKindOf
            stand_in(0x584d40, (void *)&t3<0x909>);   // ReceiveEvil
            stand_in(0x50db60, (void *)&c0<0x90a>);   // WindowMissionOver_IsOpen
            D(app + 0x7c0, 0) = ptrv(g_vt);
            e[side][0] = orel(at<DWORD(THISCALL *)(void *)>(0x59a9c0)(app));
            e[side][1] = at<DWORD(THISCALL *)(void *)>(0x59a9e0)(app) & 0xff;
            e[side][2] = D(app, 0x354) ? at<DWORD(THISCALL *)(void *)>(0x59b7e0)(app) : 0;
            e[side][3] = at<DWORD(THISCALL *)(void *)>(0x59b9d0)(app);
            if (app[0x310])
                e[side][3] &= 0xff;
            e[side][4] = at<DWORD(THISCALL *)(void *, DWORD)>(0x59bb80)(app, 0x42);
            e[side][5] = at<DWORD(THISCALL *)(void *, void *)>(0x59d110)(app, message);
            e[side][6] = orel(at<DWORD(THISCALL *)(void *)>(0x59db10)(app));
            e[side][7] = *at<DWORD *>(0x714558) ? at<DWORD(THISCALL *)(void *)>(0x59e0a0)(app) : 0;
            e[side][8] = orel(at<DWORD(THISCALL *)(void *)>(0x5a0740)(app));
            e[side][9] = at<DWORD(THISCALL *)(void *)>(0x5a0750)(app);
            e[side][10] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5a2c20)(app, 0x86, 0x87);
            e[side][11] = at<DWORD(THISCALL *)(void *)>(0x5a3150)(app);
            e[side][12] = at<DWORD(THISCALL *)(void *)>(0x5a33d0)(app);
            e[side][13] = *at<DWORD *>(0x713f1c);
            e[side][14] = e[side][15] = 0;
            end(side);
            *at<DWORD *>(0x713670) = saved[0];
            *at<DWORD *>(0x713f1c) = saved[1];
            *at<DWORD *>(0x714558) = saved[2];
            *at<DWORD *>(0x713f18) = saved[3];
        }
        char d[80];
        wsprintfA(d, "[seed %u: first difference at %d]", seed, [&] {
            for (int i = 0; i < 16; i++)
                if (e[0][i] != e[1][i])
                    return i;
            return -1;
        }());
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && same_log(), "application slots", d);
    }
}

// ---- The difficulty list's slots 37 and 39 on a list of our own ------------------------------------------------

BYTE g_list[2][0x140];
DWORD g_list_vt[64];
const char *THISCALL l_name(void *, DWORD i)
{
    ev(0xa01, i);
    static const char *const N[] = {"Easy", "Dificulty Types/Hard", "x.DIFICULTY"};
    return N[i % 3];
}
const char *THISCALL l_folder(void *) { ev(0xa05); return "Data/Difficulty"; }
void *__fastcall s_create(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    ev(0xa37, fnv_text(2166136261u, path->c_str()), flags * 16 + write_back, out != nullptr);
    return at<void *>(0x31000 + g_nlog[g_side]);
}

void test_list(Tally &t)
{
    for (int i = 0; i < 64; i++)
        g_list_vt[i] = ptrv((void *)&t0<0xa00>);
    g_list_vt[1] = ptrv((void *)&l_name);
    g_list_vt[5] = ptrv((void *)&l_folder);
    for (DWORD i = 0; i < 6; i++) {
        DWORD text[2], e[2];
        for (int side = 0; side < 2; side++) {
            memset(g_list[side], 0, 0x140);
            D(g_list[side], 0) = ptrv(g_list_vt);
            ZString z;
            at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
            begin(side);
            at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x4fe210)(g_list[side], i, &z);
            stand_in(0x4164c0, (void *)&s_create);
            void *out = nullptr;
            e[side] = ptrv(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x4fe0e0)(g_list[side], i & 1 ? "Data" : "", "Hard",
                                                                                                   i & 2 ? &out : nullptr));
            end(side);
            text[side] = fnv_text(2166136261u, z.c_str());
            at<DWORD(THISCALL *)(void *)>(0x4defd0)(&z);
        }
        t.check(e[0] == e[1] && text[0] == text[1] && same_log(), "difficulty list slots");
    }
}

}  // namespace

ZAX_CHECKS(c4_app)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_c4_app;
    if (!arena_setup()) {
        log("Check c4 app: no arena");
        return;
    }
    Tally slots{"c4 app: CZaxWorld's small slots, the difficulty list's slots"};
    test_slots(slots);
    test_list(slots);
    slots.report();
}

}  // namespace zax
