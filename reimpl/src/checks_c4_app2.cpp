// Checks of CZaxWorld's large slots (c4_app2.cpp) against the original, in the game thread (machinery:
// checks_net_common.h: side 0 with the module's hooks out, values logged and hashed through norm(), so
// heap and stack pointers compare by their first appearance).
// The application is a fake of 0x800 bytes with a vtable of 128 recording slots (each with its slot's
// argument count); the objects it holds (the screen and HUD frames, the caching table, the inventory screen,
// the HUD slide, the game data, the save game stats, the other map list, layers and characters) are fakes
// too. Every exe function the slots call is a recording stand-in of its convention and argument count whose
// result the case chooses (fakes for the pointers the code goes on with). The game type, difficulty and HUD
// lists' records, g_App, g_World, the error reporter and the other globals the slots read or write are
// saved, set to values of the case and put back. The FPU results (the timers, the regeneration, the scale
// factors) are compared as the bits stored.
#include "checks_net_common.h"
#include "file_search.h"
#include "init_list.h"

namespace zax {

int hooks_c4_app2(Hook *out);

namespace {

using namespace netcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }

bool g_trace = false;
void trace(DWORD a) { if (g_trace) log("    at %x", a); }

// ---- Results the case chooses per stand-in address --------------------------------------------------------

constexpr int NRET = 64;
DWORD g_ret_addr[NRET], g_ret_val[NRET];
int g_nret;
void set_ret(DWORD a, DWORD v)
{
    for (int i = 0; i < g_nret; i++)
        if (g_ret_addr[i] == a) {
            g_ret_val[i] = v;
            return;
        }
    if (g_nret < NRET) {
        g_ret_addr[g_nret] = a;
        g_ret_val[g_nret++] = v;
    }
}
DWORD ret_of(DWORD a)
{
    for (int i = 0; i < g_nret; i++)
        if (g_ret_addr[i] == a)
            return g_ret_val[i];
    return 0x100 + (a & 0xff) + (DWORD)g_nlog[g_side];   // a small value of its own
}

// Stack addresses (the two sides' frames differ) recorded as one value
DWORD sn(DWORD v)
{
    volatile BYTE probe = 0;
    return v - (ptrv((const void *)&probe) - 0x8000) < 0x10000 ? 0x57ac0000u : v;
}

// ---- Recording stand-ins ----------------------------------------------------------------------------------

template <DWORD A> DWORD THISCALL th0(void *s) { trace(A); ev(A, sn(ptrv(s))); return ret_of(A); }
template <DWORD A> DWORD THISCALL th1(void *s, DWORD a) { trace(A); ev(A, sn(ptrv(s)), sn(a)); return ret_of(A); }
template <DWORD A> DWORD THISCALL th2(void *s, DWORD a, DWORD b) { trace(A); ev(A, sn(ptrv(s)), sn(a), sn(b)); return ret_of(A); }
template <DWORD A> DWORD THISCALL th3(void *s, DWORD a, DWORD b, DWORD c) { trace(A); ev(A, sn(ptrv(s)), sn(a), sn(b)); ev(A, sn(c)); return ret_of(A); }
template <DWORD A> DWORD THISCALL th4(void *s, DWORD a, DWORD b, DWORD c, DWORD d) { trace(A); ev(A, sn(ptrv(s)), sn(a), sn(b)); ev(A, sn(c), sn(d)); return ret_of(A); }
template <DWORD A> DWORD THISCALL th5(void *s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e) { trace(A); ev(A, sn(ptrv(s)), sn(a), sn(b)); ev(A, sn(c), sn(d), sn(e)); return ret_of(A); }
template <DWORD A> DWORD THISCALL th6(void *s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{
    ev(A, ptrv(s), a, b);
    ev(A, c, d, e);
    ev(A, f);
    return ret_of(A);
}
template <DWORD A> DWORD THISCALL th7(void *s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f, DWORD g)
{
    ev(A, ptrv(s), a, b);
    ev(A, c, d, e);
    ev(A, f, g);
    return ret_of(A);
}
template <DWORD A> DWORD __fastcall fa1(DWORD c) { trace(A); ev(A, sn(c)); return ret_of(A); }
template <DWORD A> DWORD __fastcall fa2(DWORD c, DWORD d) { trace(A); ev(A, sn(c), sn(d)); return ret_of(A); }
template <DWORD A> DWORD __fastcall fa6(DWORD c, DWORD d, DWORD a, DWORD b, DWORD e, DWORD f) { trace(A); ev(A, sn(c), sn(d), sn(a)); ev(A, sn(b), sn(e), sn(f)); return ret_of(A); }
template <DWORD A> DWORD __cdecl cd0() { trace(A); ev(A); return ret_of(A); }
template <DWORD A> DWORD __cdecl cd1(DWORD a) { trace(A); ev(A, sn(a)); return ret_of(A); }
template <DWORD A> DWORD __cdecl cd4(DWORD a, DWORD b, DWORD c, DWORD d) { trace(A); ev(A, sn(a), sn(b), sn(c)); ev(A, sn(d)); return ret_of(A); }
template <DWORD A> DWORD STDCALL sd1(DWORD a) { trace(A); ev(A, sn(a)); return ret_of(A); }
template <DWORD A> DWORD STDCALL sd4(DWORD a, DWORD b, DWORD c, DWORD d) { trace(A); ev(A, sn(a), sn(b), sn(c)); ev(A, sn(d)); return ret_of(A); }
// FPU results (the hit points)
DWORD g_fpu_val;
template <DWORD A> long double THISCALL fp0(void *s)
{
    ev(A, ptrv(s));
    float f;
    memcpy(&f, &g_fpu_val, 4);
    return (long double)f + (long double)(g_nlog[g_side] & 3);
}

CodePatch g_p[400];
// FileBuf_Read: a value of the case into the buffer (the first read of a stream the magic, 0x3e9 or not)
DWORD g_reads, g_magic;
// ControlConfig::Frame (flag, 1, packet): the flag's dword is al of slot 61 with the original's stack garbage
// above it; recorded by its low byte
DWORD THISCALL s_control_frame(void *c, DWORD flag, DWORD a, DWORD b)
{
    ev(0x470050, ptrv(c), flag & 0xff, a ^ (b << 4));
    return 0x470050;
}
DWORD THISCALL s_position(void *part, DWORD *pos)
{
    ev(0xfb0a0, ptrv(part));
    pos[0] = 0x111;
    pos[1] = 0x222;
    return 0x4fb0a0;
}
DWORD THISCALL s_read(void *f, BYTE *p, DWORD n)
{
    ev(0xa1b00, ptrv(f), n);
    const DWORD v = g_reads++ == 0 ? g_magic : 0x20 + g_reads;
    for (DWORD i = 0; i < n; i++)
        p[i] = (BYTE)(v >> (8 * (i & 3)));
    return ptrv(f);
}
// The pool arrays' reserve functions of slot 112: a block of the arena of that capacity (the string array's
// with real empty ZStrings and its count in front)
template <DWORD A, DWORD SIZE> DWORD THISCALL s_reserve(BYTE *a, DWORD n)
{
    ev(A, n);
    BYTE *p;
    if (A == 0x5a40f0) {   // the string array: operator new with the count in front, real empty ZStrings
        p = static_cast<BYTE *>(heap_alloc(n * 4 + 4));
        D(p, 0) = n;
        p += 4;
        for (DWORD i = 0; i < n; i++)
            at<void *(THISCALL *)(void *)>(0x4dec90)(p + i * 4);
    } else
        p = static_cast<BYTE *>(pool_get_alloc(n * SIZE));   // freed by the slot to the pool
    D(a, 4) = ptrv(p);
    D(a, 0xc) = n;
    return n;
}
int g_np;
// A stand-in only for the game thread: the site jumps to a stub that sends every other thread (the sound
// streaming reads files while a case runs) to what the site did before (a hook's jump target or the
// original's trampoline), the game thread to the stand-in. eax is free at a function's entry.
BYTE *g_stubs;
int g_nstubs;
DWORD g_thread_id;
void si(DWORD a, void *f)
{
    for (int i = 0; i < g_np; i++)
        if (g_p[i].site == at<BYTE *>(a))
            return;
    if (g_np >= 400) {
        log("  c4 app2: too many stand-ins");
        return;
    }
    BYTE *site = at<BYTE *>(a);
    DWORD fallback;
    if (site[0] == 0xe9)
        fallback = a + 5 + *reinterpret_cast<const INT32 *>(site + 1);
    else
        fallback = ptrv(original_address(a));   // the site itself when it is not hooked (no way around)
    if (!g_stubs) {
        g_stubs = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x8000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        check_region(g_stubs, 0x8000, "c4 app2 stubs");
    }
    BYTE *s = g_stubs + (g_nstubs++ % 1000) * 32;
    s[0] = 0x64; s[1] = 0xa1; s[2] = 0x24; s[3] = 0; s[4] = 0; s[5] = 0;   // mov eax, fs:[0x24]
    s[6] = 0x3d; memcpy(s + 7, &g_thread_id, 4);                             // cmp eax, id
    s[11] = 0x0f; s[12] = 0x85;                                              // jne fallback
    const INT32 r1 = (INT32)(fallback - (ptrv(s) + 17));
    memcpy(s + 13, &r1, 4);
    s[17] = 0xe9;                                                            // jmp stand-in
    const INT32 r2 = (INT32)(ptrv(f) - (ptrv(s) + 22));
    memcpy(s + 18, &r2, 4);
    FlushInstructionCache(GetCurrentProcess(), s, 32);
    patch_jump(g_p[g_np++], a, s);
}
void stand_ins_off()
{
    while (g_np)
        unpatch(g_p[--g_np]);
}

// ---- Fake objects with 128 slots ----------------------------------------------------------------------------

struct Obj {
    DWORD vftable;
    DWORD id;
    DWORD vt[128];
    BYTE mem[0x800];
};
Obj *g_app_obj;
DWORD oid(Obj *o) { return o == g_app_obj ? 1 : o->id; }   // the application's +4 is a field
// A slot recorder of an object: the slot and the object's id as the event, the result from ret_of
template <int S> DWORD THISCALL s0(Obj *o) { trace(0x40000 + S); ev(0x40000 + S, oid(o)); return ret_of(0x40000 + oid(o) * 0x100 + S); }
template <int S> DWORD THISCALL s1(Obj *o, DWORD a) { trace(0x40000 + S); ev(0x40000 + S, oid(o), a); return ret_of(0x40000 + oid(o) * 0x100 + S); }
template <int S> DWORD THISCALL s2(Obj *o, DWORD a, DWORD b) { trace(0x40000 + S); ev(0x40000 + S, oid(o), a, b); return ret_of(0x40000 + oid(o) * 0x100 + S); }
template <int S> DWORD THISCALL s3(Obj *o, DWORD a, DWORD b, DWORD c) { trace(0x40000 + S); ev(0x40000 + S, oid(o), a, b); ev(0x40000 + S, c); return ret_of(0x40000 + oid(o) * 0x100 + S); }
template <int S> DWORD THISCALL s4(Obj *o, DWORD a, DWORD b, DWORD c, DWORD d) { trace(0x40000 + S); ev(0x40000 + S, oid(o), a, b); ev(0x40000 + S, c, d); return ret_of(0x40000 + oid(o) * 0x100 + S); }
template <int S> DWORD THISCALL s5(Obj *o, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e) { trace(0x40000 + S); ev(0x40000 + S, oid(o), a, b); ev(0x40000 + S, c, d, e); return ret_of(0x40000 + oid(o) * 0x100 + S); }
DWORD slot_ret(Obj *o, int s) { return 0x40000 + oid(o) * 0x100 + (DWORD)s; }
Obj *obj(DWORD id)
{
    Obj *o = (Obj *)arena(sizeof(Obj));
    o->vftable = ptrv(o->vt);
    o->id = id;
    return o;
}
template <int S> void sl(Obj *o, int n)
{
    void *f = n == 0 ? (void *)&s0<S> : n == 1 ? (void *)&s1<S> : n == 2 ? (void *)&s2<S> : n == 3 ? (void *)&s3<S> : n == 4 ? (void *)&s4<S> : (void *)&s5<S>;
    o->vt[S] = ptrv(f);
}

// The definition lists' slots during a case (called on the real list globals)
unsigned g_k;
const char *THISCALL l_name(void *l, DWORD i) { ev(0x50001, ptrv(l), i); return g_k & 1 ? "Dir/Name" : "Plain"; }
DWORD THISCALL l_find(void *l, const char *name, DWORD hint) { ev(0x50003, ptrv(l), fnv_text(2166136261u, name), hint); return g_k % 3; }
DWORD THISCALL l_monitor(void *l, DWORD m) { ev(0x5001b, ptrv(l), m); return 0x1b; }
DWORD THISCALL l_create(void *l, const char *dir, const char *name, DWORD out)
{
    ev(0x50025, ptrv(l), fnv_text(2166136261u, dir) ^ fnv_text(2166136261u, name), out);
    return 0x25000 + (DWORD)g_nlog[g_side];
}
DWORD THISCALL l_clear(void *l) { ev(0x50028, ptrv(l)); return 0x28; }

// ---- The world of a case --------------------------------------------------------------------------------------

Obj *g_lvt, *g_app, *g_screen, *g_hud, *g_cache, *g_inv, *g_slide, *g_data, *g_stats, *g_other, *g_net, *g_game, *g_layer, *g_char, *g_gt, *g_rep,
    *g_mixer, *g_seq, *g_session, *g_frame2, *g_list;
BYTE g_records[4][3 * 0x80 + 4];
DWORD g_saved[32];
const DWORD LISTS[] = {0x6c02e8, 0x6c0540, 0x6be1e0, 0x6c2a00, 0x713380, 0x62e7e0};
const DWORD SAVED[] = {0x6c02e8, 0x6c0540, 0x6be1e0, 0x6c2a00, 0x713380, 0x62e7e0, 0x713f14, 0x6c2080, 0x6c02cc, 0x6c0318, 0x6c031c, 0x6c2a30, 0x6c2a34, 0x6c0570, 0x6c0574, 0x713f1c, 0x713f20, 0x618034,
                       0x618038, 0x62485c, 0x6c0424, 0x6c02fc, 0x6c2a14, 0x7133b0, 0x7133b4, 0x62e810, 0x62e814, 0x6c0420, 0x6bb4e0};

// The records of a list (3, the object of record i the case's fake game type when k says so)
BYTE *records(int which, Obj *o, unsigned bits)
{
    BYTE *r = g_records[which] + 4;
    memset(g_records[which], 0, sizeof g_records[which]);
    D(g_records[which], 0) = 3;
    for (DWORD i = 0; i < 3; i++) {
        D(r + i * 0x80, 0x78) = (bits >> i) & 1 ? ptrv(o) : 0;
        r[i * 0x80 + 0x7f] = (BYTE)((bits >> (3 + i)) & 1);
        wsprintfA(reinterpret_cast<char *>(r + i * 0x80), "Rec %u", i);
    }
    return r;
}

void make_world(unsigned k)
{
    // the globals saved before anything of the case is written into them
    for (int i = 0; i < (int)(sizeof SAVED / 4); i++)
        g_saved[i] = *at<DWORD *>(SAVED[i]);
    g_k = k;
    g_nret = 0;
    g_app = obj(1);
    g_screen = obj(2);
    g_hud = obj(3);
    g_cache = obj(4);
    g_inv = obj(5);
    g_slide = obj(6);
    g_data = obj(7);
    g_stats = obj(8);
    g_other = obj(9);
    g_net = obj(10);
    g_game = obj(11);
    g_layer = obj(12);
    g_char = obj(13);
    g_gt = obj(14);
    g_rep = obj(15);
    g_mixer = obj(16);
    g_seq = obj(17);
    g_session = obj(18);
    g_frame2 = obj(19);
    g_list = obj(20);
    Obj *all[] = {g_app, g_screen, g_hud, g_cache, g_inv, g_slide, g_data, g_stats, g_other, g_net, g_game, g_layer, g_char, g_gt,
                  g_rep, g_mixer, g_seq, g_session, g_frame2, g_list};
    // the slots the code calls, with their argument counts
    for (Obj *o : all) {
        sl<0>(o, 1);  sl<1>(o, 1);  sl<3>(o, 3);  sl<5>(o, 1);  sl<7>(o, 0);  sl<10>(o, 0);  sl<12>(o, 1);  sl<13>(o, 0);  sl<14>(o, 2);
        sl<19>(o, 0); sl<23>(o, 1); sl<24>(o, 1); sl<25>(o, 0); sl<27>(o, 1); sl<28>(o, 0);  sl<29>(o, 0);  sl<30>(o, 0);
        sl<31>(o, 0); sl<33>(o, 0); sl<34>(o, 0); sl<37>(o, 3); sl<39>(o, 0); sl<41>(o, 2); sl<42>(o, 3);  sl<44>(o, 0);
        sl<45>(o, 0); sl<61>(o, 0); sl<62>(o, 1); sl<63>(o, 1); sl<64>(o, 2); sl<69>(o, 2); sl<70>(o, 3);  sl<73>(o, 0);
        sl<74>(o, 2); sl<76>(o, 0); sl<78>(o, 2); sl<80>(o, 0); sl<81>(o, 1); sl<83>(o, 4); sl<88>(o, 0);  sl<98>(o, 0);
        sl<104>(o, 2); sl<111>(o, 3); sl<112>(o, 1);
    }
    g_app_obj = g_app;
    g_app->vt[1] = ptrv((void *)&s0<1>);     // the net game (no argument; elsewhere slot 1 is the deleting destructor)
    g_app->vt[23] = ptrv((void *)&s2<23>);   // the world's slot 23 (window, 1)
    g_rep->vt[27] = ptrv((void *)&s2<27>);   // the error reporter's slot 27 (1, 1)
    g_session->vt[42] = ptrv((void *)&s1<42>);   // the player control AI's frame
    g_lvt = obj(21);   // the definition lists' vtable during a case
    g_lvt->vt[1] = ptrv((void *)&l_name);
    g_lvt->vt[3] = ptrv((void *)&l_find);
    g_lvt->vt[27] = ptrv((void *)&l_monitor);
    g_lvt->vt[37] = ptrv((void *)&l_create);
    g_lvt->vt[40] = ptrv((void *)&l_clear);
    for (DWORD l : LISTS)
        *at<DWORD *>(l) = ptrv(g_lvt->vt);
    D(g_screen, 0xec) = 0;   // no modal windows
    g_gt->vt[39] = ptrv((void *)&s2<39>);   // the game type's slot 39 (player, point)
    g_gt->vt[23] = ptrv((void *)&s0<23>);   // (no arguments: the quest mode selected)
    g_gt->vt[42] = ptrv((void *)&s3<42>);   // (frame, h, room)
    g_gt->vt[25] = ptrv((void *)&s2<25>);   // (killer, victim)
    g_gt->vt[33] = ptrv((void *)&s0<33>);
    g_gt->vt[34] = ptrv((void *)&s0<34>);
    g_char->vt[98] = ptrv((void *)&s1<98>);   // heal (amount)
    g_char->vt[99] = ptrv((void *)&s3<99>);
    g_char->vt[64] = ptrv((void *)&s2<64>);
    g_session->vt[23] = ptrv((void *)&s1<23>);
    BYTE *a = g_app->mem - 8;   // the application's fields at their offsets (the fake's +8 is mem)
    (void)a;
    // the application's fields: the fake's memory starts at +0x208; the slots read the offsets relative to
    // the object, so the fields live in the object itself (vt and mem are large enough)
    BYTE *self = reinterpret_cast<BYTE *>(g_app);
    D(self, 4) = ptrv(g_screen);
    D(self, 0x2a8) = ptrv(g_hud);
    D(self, 0x2a4) = ptrv(g_frame2);
    D(self, 0x33c) = ptrv(g_cache->mem);   // the caching table is read by fields only
    D(self, 0x340) = ptrv(g_inv->mem);
    D(self, 0x344) = ptrv(g_slide);
    D(self, 0x34c) = ptrv(g_data);
    D(self, 0x350) = ptrv(g_stats);
    D(self, 0x354) = ptrv(g_other);
    D(self, 0x334) = ptrv(g_mixer->mem);
    D(g_mixer->mem, 0x24) = k % 3;
    D(self, 0x2bc) = ptrv(g_list->mem);
    D(self, 0x2c0) = 2;
    D(g_list->mem, 0) = ptrv(g_layer);
    D(g_list->mem, 4) = k & 1 ? ptrv(g_layer) : 0;
    D(self, 0x318) = ptrv(g_list->mem + 0x40);
    D(self, 0x31c) = k & 2 ? 1 : 0;
    D(g_list->mem, 0x40) = ptrv(g_char);
    D(self, 0x58c) = k % 5 == 0 ? 0xffffffff : k % 3;
    self[0x310] = (BYTE)(k & 1);
    self[0x311] = (BYTE)((k >> 1) & 1);
    for (DWORD off : {0x312u, 0x338u, 0x348u, 0x358u, 0x360u, 0x361u, 0x588u, 0x590u, 0x591u, 0x32cu})
        self[off] = (BYTE)((k >> (off & 7)) & 1);
    D(self, 0x314) = ptrv(g_app->vt);   // the embedded object +0x314 (its slot 1 without arguments)
    D(self, 0x2e4) = 10;
    D(self, 0x5a4) = 0x40000000 + (k << 18);
    D(self, 0x5a8) = 0x3f000000 + (k << 19);
    D(self, 0x5ac) = k & 4 ? 0xbf000000 : 0x3e000000;
    D(self, 0x5b0) = 0x3f800000;
    D(self, 0x594) = k & 8 ? 0x3f800000 : 0;
    D(self, 0x368) = 0x3e000000 + (k << 20);
    D(self, 0x35c) = k & 4 ? 0xbf800000 : 0x3f800000;
    // the ZStrings the slots assign or draw
    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(self + 0x364, k & 1 ? "status" : "");
    D(g_data, 0x28) = k % 4;
    D(g_data, 0x2c) = k & 4 ? ptrv(g_seq) : 0;
    g_cache->mem[0x18] = (BYTE)((k >> 3) & 1);
    g_cache->mem[0x19] = 0;
    D(g_inv->mem, 0x100) = k % 3;
    // globals
    *at<DWORD *>(0x713f14) = ptrv(g_app);
    *at<DWORD *>(0x6c2080) = ptrv(g_app);
    *at<DWORD *>(0x6c02cc) = ptrv(g_rep);
    D(g_rep, 0x24) = k & 1 ? 8 : 16;
    *at<DWORD *>(0x6c0318) = ptrv(records(0, g_gt, 7));
    *at<DWORD *>(0x62e810) = ptrv(records(3, g_frame2, 5));
    *at<DWORD *>(0x62e814) = 3;
    *at<DWORD *>(0x6c031c) = 3;
    *at<DWORD *>(0x6c2a30) = ptrv(records(1, g_gt, k >> 1));
    *at<DWORD *>(0x6c2a34) = 3;
    *at<DWORD *>(0x6c0570) = ptrv(records(2, g_frame2, 7));   // the HUD frames are used without a null test
    *at<DWORD *>(0x6c0574) = 3;
    *at<DWORD *>(0x7133b0) = ptrv(records(3, g_frame2, 7));   // shared with the dialogue list's
    *at<DWORD *>(0x7133b4) = 3;
    *at<DWORD *>(0x713f1c) = k % 6;
    *at<DWORD *>(0x713f20) = 0x40a00000 + (k << 16);
    *at<DWORD *>(0x62485c) = k & 1;
    *at<DWORD *>(0x6c02fc) = 3;
    *at<DWORD *>(0x6c2a14) = 3;
    // results the code goes on with
    set_ret(0x4f1050, k & 2 ? ptrv(g_layer) : 0);     // World::CurrentLayer
    set_ret(0x4f5d80, k & 1 ? ptrv(g_char) : 0);      // World::FirstCharacter
    set_ret(slot_ret(g_app, 1), ptrv(g_game));
    set_ret(slot_ret(g_app, 44), ptrv(g_net));
    set_ret(slot_ret(g_app, 98), ptrv(g_session));
    set_ret(slot_ret(g_app, 88), ptrv(g_session));
    set_ret(slot_ret(g_net, 23), ptrv(g_session));
    set_ret(slot_ret(g_net, 12), 0x1212);
    set_ret(slot_ret(g_char, 64), k & 2 ? ptrv(g_session) : 0);
    set_ret(0x4cdf30, ptrv(g_screen));                 // App_Window
    for (DWORD c : {0x543190u, 0x5aa4d0u, 0x5240b0u, 0x4a1780u, 0x505f00u, 0x5be4a0u})
        set_ret(c, ptrv(g_seq));                        // constructors: an object the code goes on with
    set_ret(0x4a8a90, ptrv(g_frame2));
    set_ret(0x428c00, ptrv(g_inv));
    set_ret(0x48d8f0, ptrv(g_frame2));                 // DefinitionList_Object
    set_ret(0x51f440, ptrv(g_session->mem));           // NetGame_PlayerAt
    D(g_session->mem, 0x14) = 0x14;
    D(g_session->mem, 0x1c) = ptrv(g_session->mem + 0x100);
    D(g_session->mem + 0x100, 4) = 100 + k;
    D(g_session->mem + 0x100, 8) = 200 + k;
    D(g_session->mem, 0xe8) = 0xe8e8;
    set_ret(0x5176f0, k & 1 ? ptrv(g_char) : 0);      // Entity_CreateFromModel
    set_ret(0x418fc0, k & 1);                          // FieldDesc_IsDynamicIn
    set_ret(0x418790, ptrv(g_session->mem + 0x200));   // FieldDesc_DynamicPointer
    set_ret(0x4f4d70, k & 4 ? ptrv(g_layer) : 0);     // World::FindLayer
    set_ret(0x4f45b0, ptrv(g_layer));                  // World::LoadLayer
    set_ret(0x416790, k & 1);                          // Object_IsKindOf
    set_ret(0x552940, k & 2 ? ptrv(g_seq) : 0);       // CQuestGameType::Stat
    set_ret(0x5525b0, ptrv(g_seq->mem));               // Quest_SpecificData
    D(g_seq->mem, 0xc) = ptrv(g_seq->mem + 0x100);
    set_ret(0x4ef8f0, k & 1 ? 1 : 0);                  // CCharacterListHolder::Count
    set_ret(0x4ef8e0, ptrv(g_list->mem + 0x40));       // CCharacterListHolder::Slot
    set_ret(0x4267e0, ptrv(g_inv));                    // LayerPart_GetInventory
    set_ret(0x4a2150, ptrv(g_list->mem + 0x100));      // FileBuf_Advance
    set_ret(0x5242b0, k & 1);                          // ObjectStream_OpenFile
    set_ret(0x5242f0, (k >> 1) & 1);                   // ObjectStream_Open
    set_ret(0x4a1840, k & 1);                          // FileBuf_Open
    set_ret(0x4a1570, k & 1);                          // File_CanOpen
    set_ret(0x5899b0, k & 1);                          // IsExpired
    set_ret(0x591fc0, k % 3 == 0 ? 0xffffffff : 1);   // NamedList_FindOrFail
    set_ret(0x523df0, k % 3);                          // ResourceList_Find
    set_ret(0x4eb670, 0x4eb670);                       // CLayer::FileName (a value)
    set_ret(0x4e13a0, ptrv("text"));                   // ZString_c_str is real (not a stand-in)
    set_ret(0x4efe20, k);                              // CLayerHolder::UniqueId
    set_ret(0x4a12d0, k % 3);                          // Files_Count
    set_ret(0x4a1150, k % 3);                          // Files_ListNames
    set_ret(0x5127e0, k & 1 ? ptrv(g_seq) : 0);       // CMissionManager::Current
    set_ret(0x55b940, ptrv(g_seq));                    // QuipList_Get
    set_ret(0x468000, ptrv(g_seq));                    // CannedObjectList_Get
    set_ret(0x595b50, ptrv(g_seq));                    // Voice_Get
    set_ret(0x4a35d0, ptrv(g_seq));                    // ResourceLists_Get
    set_ret(0x46ed70, k & 1);                          // BitSet::Test
    set_ret(0x518690, 0);                              // Cursor::Shown
    set_ret(0x519ef0, k & 1);                          // Net_StartFromCommandLine
    set_ret(0x55d030, 16);                             // ColorDepth
    set_ret(0x429880, 300);                            // RoomAbove
    set_ret(slot_ret(g_frame2, 45), k & 1 ? 0x320 : 600);
    set_ret(slot_ret(g_frame2, 39), 640);
    set_ret(slot_ret(g_hud, 39), 640);
    set_ret(slot_ret(g_hud, 45), 480);
    set_ret(slot_ret(g_screen, 45), 480);
    set_ret(slot_ret(g_gt, 33), k & 1);
    set_ret(slot_ret(g_gt, 34), (k >> 1) & 1);
    set_ret(slot_ret(g_app, 25), k % 7 == 0);
    set_ret(slot_ret(g_app, 30), k & 1);
    set_ret(slot_ret(g_app, 31), (k >> 1) & 1);
    set_ret(slot_ret(g_app, 13), k & 1);
    set_ret(slot_ret(g_game, 13), k & 1);
    set_ret(slot_ret(g_app, 74), 1);
    set_ret(0x51f450, 2);                              // NetGame_PlayerCount
    g_fpu_val = 0x42c80000 + (k << 16);
    D(g_screen, 0x44) = k & 1;
    D(g_app, 0x300) = ptrv(g_frame2);
}
void unmake_world()
{
    stand_ins_off();
    at<DWORD(THISCALL *)(void *)>(0x4defd0)(reinterpret_cast<BYTE *>(g_app) + 0x364);
    for (int i = 0; i < (int)(sizeof SAVED / 4); i++)
        *at<DWORD *>(SAVED[i]) = g_saved[i];
}
// The state after a case: the application's fields, the fakes' memory, the globals
void world_state()
{
    st_dwords(reinterpret_cast<BYTE *>(g_app) + 0x200, 0x400);
    st_dwords(g_cache->mem, 0x60);
    st_dwords(g_inv->mem, 0x120);
    st_dwords(g_session->mem, 0x210);
    for (int w = 0; w < 4; w++)
        st_dwords(g_records[w], sizeof g_records[w]);
    for (DWORD a : {0x713f1cu, 0x713f20u, 0x618034u, 0x618038u, 0x62485cu, 0x6c0424u, 0x6c0420u})
        st(*at<DWORD *>(a));
}

// The stand-ins every case installs (each with its convention and argument count)
void install()
{
    si(0x4a12d0, (void *)&fa2<0x4a12d0>);
    si(0x4a1150, (void *)&fa6<0x4a1150>);
    si(0x587a00, (void *)&fa2<0x587a00>);
    si(0x4a3fb0, (void *)&fa2<0x4a3fb0>);
    si(0x4a2650, (void *)&fa1<0x4a2650>);
    si(0x4a1140, (void *)&cd0<0x4a1140>);
    si(0x4a35d0, (void *)&cd0<0x4a35d0>);
    si(0x5d097c, (void *)&cd4<0x5d097c>);
    si(0x408e70, (void *)&sd4<0x408e70>);
    si(0x55b940, (void *)&cd0<0x55b940>);
    si(0x468000, (void *)&cd0<0x468000>);
    si(0x595b50, (void *)&cd0<0x595b50>);
    si(0x4d4a50, (void *)&cd0<0x4d4a50>);
    si(0x433a80, (void *)&cd0<0x433a80>);
    si(0x5525b0, (void *)&cd0<0x5525b0>);
    si(0x50db60, (void *)&cd0<0x50db60>);
    si(0x4a2670, (void *)&cd0<0x4a2670>);
    si(0x5c1040, (void *)&cd0<0x5c1040>);
    si(0x505370, (void *)&cd0<0x505370>);
    si(0x47caa0, (void *)&cd0<0x47caa0>);
    si(0x505390, (void *)&cd0<0x505390>);
    si(0x5432a0, (void *)&cd0<0x5432a0>);
    si(0x519ef0, (void *)&cd0<0x519ef0>);
    si(0x55d030, (void *)&cd0<0x55d030>);
    si(0x48d030, (void *)&fp0<0x48d030>);
    si(0x4a1b00, (void *)&s_read);
    si(0x470050, (void *)&s_control_frame);
    si(0x4fb0a0, (void *)&s_position);
    si(0x5a41b0, (void *)&s_reserve<0x5a41b0, 8>);
    si(0x5a4080, (void *)&s_reserve<0x5a4080, 4>);
    si(0x5a40f0, (void *)&s_reserve<0x5a40f0, 4>);
    si(0x48d150, (void *)&fp0<0x48d150>);
#define T0(a) si(a, (void *)&th0<a>)
#define T1(a) si(a, (void *)&th1<a>)
#define T2(a) si(a, (void *)&th2<a>)
#define T3(a) si(a, (void *)&th3<a>)
#define T4(a) si(a, (void *)&th4<a>)
#define T5(a) si(a, (void *)&th5<a>)
#define T6(a) si(a, (void *)&th6<a>)
#define T7(a) si(a, (void *)&th7<a>)
    T0(0x4f1050); T0(0x4f5d80); T0(0x595af0); T0(0x58a030); T0(0x41f660); T1(0x4f5150); T0(0x507f20); T0(0x543190); T1(0x542550);
    T1(0x4fbc50); T0(0x5aa4d0); T2(0x589270); T2(0x5893c0); T2(0x5892b0); T4(0x4f5360); T1(0x4fb0a0); T0(0x5bacd0); T4(0x4f5dd0);
    T1(0x55e090); T3(0x4a1b00); T0(0x5664b0); T4(0x5c3ba0); T2(0x505fb0); T0(0x5240b0); T3(0x5242f0); T4(0x4f4500); T0(0x5241e0);
    T0(0x58ea40); T1(0x4a20e0); T0(0x4a2180); T1(0x4a2150); T1(0x57e820); T1(0x5899b0); T1(0x589790); T0(0x589db0); T3(0x5242b0);
    T2(0x4efe90); T1(0x4e18f0); T0(0x5a0a30); T4(0x4b3280); T0(0x5693a0); T1(0x48d8f0); T5(0x5176f0); T1(0x4ce500); T0(0x4fc7c0);
    T1(0x4fc7d0); T1(0x418fc0); T1(0x418790); T0(0x4a1780); T2(0x4a1840); T1(0x4ebe10); T0(0x524320); T0(0x505f00); T3(0x5c3b40);
    T1(0x4a21c0); T2(0x4a19d0); T0(0x4efe20); T1(0x4f4650); T0(0x5bc2a0); T1(0x416f50); T1(0x4168f0);
    T1(0x416fe0); T1(0x416af0); T2(0x4f3b40); T2(0x4f3e70); T1(0x5061e0); T0(0x4a3760); T1(0x416790); T1(0x552940); T2(0x555930);
    T2(0x5575b0); T2(0x55aad0); T2(0x4f4190); T2(0x523df0); T1(0x478940); T2(0x41f6c0); T0(0x4f3cc0); T2(0x4f45b0); T1(0x4f4890);
    T1(0x589b80); T0(0x4f3d30); T1(0x5a41b0); T1(0x5a4080); T1(0x5a40f0); T1(0x493ef0); T1(0x589af0); T1(0x4ea220); T1(0x589a50);
    T1(0x589570); T3(0x4ab5a0); T1(0x4f6980); T2(0x4f6770); T1(0x55bd50); T0(0x589cc0); T1(0x4f2620); T2(0x5222a0); T0(0x5bae00);
    T0(0x595b00); T0(0x5be0d0); T2(0x568870); T1(0x5be4a0); T0(0x5bede0); T0(0x5122f0); T0(0x5127e0); T1(0x51e520); T0(0x4df2b0);
    T0(0x5084a0); T1(0x508000); T0(0x508570); T0(0x4294f0); T2(0x5184e0); T2(0x518570); T0(0x4295e0); T0(0x518690); T0(0x5185f0);
    T1(0x4e7330); T1(0x4f3560); T0(0x51f450); T3(0x470050); T0(0x4ef8f0); T1(0x4ef8e0); T4(0x429ae0); T1(0x4f27b0); T0(0x4e13e0);
    T1(0x55bc60); T1(0x429880); T0(0x4267e0); T2(0x578cb0); T4(0x4a34d0); T1(0x523e20); T2(0x4dfa50); T0(0x4a09f0); T2(0x4a0a60);
    T1(0x4a0b20); T1(0x47e560); T2(0x523dc0); T5(0x591cd0); T3(0x5a3c50); T3(0x5a3650); T3(0x5a3950); T1(0x4f13a0);
    T1(0x4b1cc0); T1(0x4b1db0); T0(0x4a8a90); T5(0x4a9a30); T6(0x4a6470); T1(0x428c00); T0(0x4293c0); T0(0x4b1d20); 
    T2(0x40c680); T2(0x40c740); T2(0x591fc0);
    T1(0x46ed70); T0(0x4cdf30); T0(0x4eb670); T1(0x4f4d70); T1(0x51f440); T0(0x589890);
    si(0x4a1570, (void *)&fa2<0x4a1570>);
#undef T0
#undef T1
#undef T2
#undef T3
#undef T4
#undef T5
#undef T6
#undef T7
}

// The slots, each called with arguments of the case
void call_slot(int which, unsigned k)
{
    BYTE *self = reinterpret_cast<BYTE *>(g_app);
    BYTE *stream = g_list->mem + 0x200;
    D(stream, 0x1c) = ptrv(g_list->mem + 0x280);
    BYTE reloc[0x18] = {};
    D(reloc, 0) = 0x3f800000;
    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(reloc + 8, "Act 1/Map");
    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(reloc + 0xc, "Start");
    reloc[0x10] = (BYTE)((k >> 1) & 1);   // the layer kept and updated (needs a current layer: k & 2)
    D(reloc, 0x14) = k % 5;
    if (!(k & 1))
        D(self, 0x31c) = 0;   // a character that is not one would be used as null (as in the original)
    g_reads = 0;
    g_magic = k & 1 ? 0x3e9 : 0x123;
    switch (which) {
    case 0: {   // without a started game the caller's eax
        const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x59b9e0)(self, k);
        st(self[0x310] ? r : 0);
        break;
    }
    case 1: st(at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x59be20)(self, g_layer, k)); break;
    case 2: st(at<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5a08b0)(self, "Levels/Act 1/Main", k % 4, 0x1234)); break;
    case 3: st(at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x5a0990)(self, g_char, 1, k & 1, 3)); break;
    case 4: st(at<DWORD(THISCALL *)(void *)>(0x5a0090)(self)); break;
    case 5: st(at<DWORD(THISCALL *)(void *, void *, DWORD, void *)>(0x5a2f20)(self, k & 2 ? g_char : g_layer, 0, g_char)); break;
    case 6:   // without a game type (index -1) the caller's eax
    case 7: {
        const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(which == 6 ? 0x5a2ff0 : 0x5a30a0)(self, which == 6 ? 0x3f800000 : 0x40000000);
        st(D(self, 0x58c) == 0xffffffff ? 0 : r);
        break;
    }
    case 8: st(at<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD)>(0x59e3e0)(self, 1, stream, 2, 3) & 0xff); break;
    case 9: st(at<DWORD(THISCALL *)(void *, DWORD)>(0x5a0c80)(self, 0x9999) & 0xff); break;
    case 10:   // the game type object is used without a null test: a game type is set
        if (D(self, 0x58c) == 0xffffffff)
            D(self, 0x58c) = 1;
        st(at<DWORD(THISCALL *)(void *, DWORD)>(0x59bba0)(self, 1));
        break;
    case 11: st(at<DWORD(THISCALL *)(void *, void *, const char *, DWORD, DWORD)>(0x59e520)(self, g_layer, "file", 1, k & 1) & 0xff); break;
    case 12: st(at<DWORD(THISCALL *)(void *, const char *)>(0x5a0e70)(self, "save") & 0xff); break;
    case 13: {   // without a game type what the calls before left
        const DWORD r = at<DWORD(THISCALL *)(void *, void *, void *)>(0x5a2cc0)(self, k & 1 ? g_layer : nullptr, g_char);
        st(D(self, 0x58c) == 0xffffffff ? 0 : r);
        break;
    }
    case 14: st(at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x59ab00)(self, stream, k & 1, (k >> 1) & 1)); break;
    case 15: st(at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x59e0c0)(self, 1, k % 3)); break;
    case 16: at<DWORD(THISCALL *)(void *, void *)>(0x59b400)(self, reloc); break;
    case 17: st(at<DWORD(THISCALL *)(void *, void *)>(0x59db80)(self, g_frame2)); break;
    case 18: st(at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x59add0)(self, stream, k & 1) & 0xff); break;
    case 19: at<DWORD(THISCALL *)(void *, DWORD)>(0x59d140)(self, 0x3d088889); break;
    case 20: at<DWORD(THISCALL *)(void *, DWORD)>(0x59d5c0)(self, 0x3d088889); break;
    case 21: st(at<DWORD(THISCALL *)(void *, void *)>(0x5a01c0)(self, g_seq)); break;
    }
    at<DWORD(THISCALL *)(void *)>(0x4defd0)(reloc + 8);
    at<DWORD(THISCALL *)(void *)>(0x4defd0)(reloc + 0xc);
}

const char *const NAMES[] = {"EndGame81", "AddPlayerAIs", "MapPath", "PlaceAt", "Difficulty", "CountDeath", "QuestAdd14", "QuestAdd13",
                             "LoadLayerStream", "SaveGame", "MakePlayerCharacter", "SaveLayer", "LoadGame", "Killed", "WriteWorld",
                             "PrecacheResources", "SwitchMap", "FrameHud", "ReadSaveGame", "Frame", "PlayerFrame", "LoadGameData"};

}  // namespace

ZAX_CHECKS(c4_app2)
{
    (void)cls;
    (void)n;
    g_thread_id = GetCurrentThreadId();
    netcheck::g_table = hooks_c4_app2;
    for (int which = 0; which < 22; which++) {
        char what[80];
        wsprintfA(what, "c4 app2: CZaxWorld::%s", NAMES[which]);
        run_cases(what, scaled(32), 0x59a000u + (unsigned)which, [which](int k) {
            if (g_trace)
                log("  %s case %d side %d", NAMES[which], k, g_side);
            make_world((unsigned)k);
            install();
            call_slot(which, (unsigned)k);
            if (g_trace)
                log("    returned");
            stand_ins_off();
            world_state();
            unmake_world();
        });
    }
}

}  // namespace zax
