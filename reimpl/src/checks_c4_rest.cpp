// Checks of batch C4's second part, small group (c4_rest.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h; side 0 with the module's hooks out, the C heap from each side's
// arena). The exe functions the code calls are recording stand-ins (GiveItemsToPlayers records the names of
// its list); g_App, the error reporter and the globals the functions read or write (the catalogue's count,
// the game type list, the scale factors, the dialogue list's records and its static path, the Ogg pool's
// arrays and flags) are saved, set to values of the case and put back.
#include "checks_inventory_common.h"
#include "file_buffer.h"
#include "init_list.h"
#include "memory_stream.h"
#include "x87.h"

namespace zax {

int hooks_c4_rest(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
DWORD text_of(const char *t) { return t ? fnv_text(2166136261u, t) : 0x0bad; }

CodePatch g_p[24];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void begin(int side)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, 0x5c);
}
void end(int side)
{
    while (g_np)
        unpatch(g_p[--g_np]);
    arena_off();
    side_end(side);
}
BYTE g_obj[2][0x1000];
// Pointers to the checks' own blocks by offset, stack addresses (the two sides' frames differ) as one value
DWORD orel(DWORD v)
{
    volatile BYTE probe = 0;
    if (v - (ptrv((const void *)&probe) - 0x4000) < 0x8000)
        return 0x57ac0000u;
    const DWORD b = ptrv(g_obj[g_side]);
    return v - b < sizeof g_obj[0] ? 0x0b000000u + (v - b) : rel(v, g_side);
}
DWORD g_al;
template <DWORD A> DWORD THISCALL t0(void *s) { ev(A, orel(ptrv(s))); return result(g_al); }
template <DWORD A> DWORD THISCALL t1(void *s, DWORD a) { ev(A, orel(ptrv(s)), orel(a)); return result(g_al); }
template <DWORD A> DWORD THISCALL t2(void *s, DWORD a, DWORD b) { ev(A, orel(ptrv(s)), orel(a), orel(b)); return result(g_al); }
template <DWORD A> DWORD THISCALL t3(void *s, DWORD a, DWORD b, DWORD c) { ev(A, orel(ptrv(s)), orel(a), orel(b) ^ (orel(c) << 3)); return result(g_al); }
template <DWORD A> DWORD __cdecl c0() { ev(A); return result(g_al); }
template <DWORD A> DWORD __cdecl c1(DWORD a) { ev(A, orel(a)); return result(1); }
template <DWORD A> void *THISCALL ctor0(void *s) { ev(A, orel(ptrv(s))); return s; }
template <DWORD A> DWORD __fastcall f1(DWORD c) { ev(A, orel(c)); return result(1); }
template <DWORD A> DWORD __fastcall f2(DWORD c, DWORD d) { ev(A, orel(c), orel(d)); return result(5); }
DWORD g_vt[128];
void fill_vt()
{
    for (int i = 0; i < 128; i++)
        g_vt[i] = ptrv((void *)&t1<0x100>);
}

// ---- The chat commands ----------------------------------------------------------------------------------------

DWORD __fastcall s_give(const DWORD *names)
{
    DWORD h = 2166136261u, n = 0;
    for (; names[n] && n < 200; n++)
        h = fnv_text(h, at<const char *>(names[n]));
    ev(0x201, n, h);
    return result(1);
}
const char *THISCALL s_record_name(void *, DWORD i)
{
    ev(0x202, i);
    static const char *const N[] = {"Medic Kit", "Bullets", "Twin Disruptor"};
    return N[i % 3];
}
DWORD THISCALL s_find(void *l, const char *name, DWORD hint) { ev(0x203, ptrv(l), text_of(name), hint); return 4; }
long double THISCALL s_hp(void *c) { ev(0x204, orel(ptrv(c))); return 37.5L; }
DWORD THISCALL s_damage(void *c, float hp, DWORD type, void *by)
{
    DWORD b;
    memcpy(&b, &hp, 4);
    ev(0x205, orel(ptrv(c)), b, type ^ orel(ptrv(by)));
    return result(5);
}
void *THISCALL s_first(void *a) { ev(0x206, orel(ptrv(a))); return g_obj[g_side] + 0x400; }

void test_commands(Tally &t)
{
    fill_vt();
    g_vt[99] = ptrv((void *)&s_damage);
    g_vt[39] = ptrv((void *)&t0<0x139>);
    g_vt[45] = ptrv((void *)&t0<0x145>);
    const DWORD FNS[] = {0x5a2250, 0x5a2270, 0x5a22b0, 0x5a2500, 0x5a2540, 0x5a2590, 0x5a2630, 0x5a2670, 0x5a26a0};
    for (DWORD fn : FNS)
        for (int k = 0; k < 4; k++) {
            DWORD e[2], st[2][4];
            const DWORD saved_app = *at<DWORD *>(0x713f14), saved_count = *at<DWORD *>(0x6c0c1c);
            for (int side = 0; side < 2; side++) {
                begin(side);
                g_seed = fn + (DWORD)k;
                BYTE *app = g_obj[side];
                memset(app, 0, 0x800);
                D(app + 0x600, 0x100) = (DWORD)k & 3;   // the inventory screen's state
                D(app, 0x340) = ptrv(app + 0x600);
                D(app + 0x700, 0) = ptrv(g_vt);
                D(app, 0x2a8) = ptrv(app + 0x700);
                D(app, 0x344) = ptrv(app + 0x780);
                D(app + 0x400, 0) = ptrv(g_vt);
                *at<DWORD *>(0x713f14) = ptrv(app);
                *at<DWORD *>(0x6c0c1c) = (DWORD)k;
                g_al = 1;
                stand_in(0x5a26f0, (void *)&s_give);
                stand_in(0x591d00, (void *)&s_record_name);
                stand_in(0x4f5d80, (void *)&s_first);
                stand_in(0x523df0, (void *)&s_find);
                stand_in(0x48d030, (void *)&s_hp);
                stand_in(0x5184e0, (void *)&t2<0x207>);   // Cursor::GetXY
                stand_in(0x518570, (void *)&t2<0x208>);   // Cursor::SetPos
                stand_in(0x508570, (void *)&t0<0x209>);   // HudSlide::HideForInventory
                stand_in(0x4294f0, (void *)&t0<0x20a>);   // CZaxInventoryScreen::Open
                stand_in(0x504050, (void *)&ctor0<0x20b>);   // MatWindow::MatWindow
                const DWORD r = at<DWORD(__cdecl *)()>(fn)();
                e[side] = fn == 0x5a2500 && k == 0 ? 0 : orel(r);   // no item: the caller's eax
                if (fn == 0x5a2590 && r)
                    e[side] = 0x140;   // the pool block (freed below)
                st[side][0] = app[0x358];
                st[side][1] = D(app, 0x35c);
                st[side][2] = st[side][3] = 0;
                end(side);
                if (fn == 0x5a2590 && r)
                    pool_free(at<void *>(r), 0x140);
                *at<DWORD *>(0x713f14) = saved_app;
                *at<DWORD *>(0x6c0c1c) = saved_count;
            }
            char d[60];
            wsprintfA(d, "[command %x case %d: %x / %x]", fn, k, e[0], e[1]);
            t.check(e[0] == e[1] && memcmp(st[0], st[1], sizeof st[0]) == 0 && same_log(), "chat commands", d);
            if (!same_log())
                log_first_difference();
        }
}

// ---- The game type list's exit, the world's slots 8 and 105 ------------------------------------------------------

DWORD THISCALL s_record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u)
{
    ev(0x301, ptrv(l), sorted ^ stride ^ num ^ u, ptrv(data));
    return result(1);
}
DWORD THISCALL s_aliases(void *a) { ev(0x302, ptrv(a)); return result(2); }
DWORD THISCALL s_load(void *f, const char *dir, const char *name, void *filter, DWORD a, DWORD b)
{
    ev(0x303, f != nullptr, text_of(dir) ^ text_of(name), orel(ptrv(filter)) ^ (a << 8) ^ (b << 9));
    return result(g_al);
}
BYTE *g_frame;
void *THISCALL s_frame_ctor(BYTE *f)
{
    ev(0x309);
    g_frame = f;
    D(f, 0) = ptrv(g_vt);
    return f;
}
// the frame's slot 1 (deleting destructor) after a failed load: recorded as "the new frame", freed
DWORD THISCALL s_frame_delete(BYTE *f, DWORD a)
{
    ev(0x101, f == g_frame, a);
    if (f == g_frame)
        pool_free(f, 0x40);
    return result(g_al);
}
DWORD g_depth;
DWORD __cdecl s_depth() { ev(0x304); return g_depth; }
DWORD THISCALL s_width(void *) { ev(0x305); return 640; }
DWORD g_height;
DWORD THISCALL s_height(void *) { ev(0x306); return g_height; }
DWORD THISCALL s_session(void *, DWORD i) { ev(0x307, i); return ptrv(g_obj[g_side] + 0x500); }
DWORD THISCALL s_net(void *) { ev(0x308); return ptrv(g_obj[g_side] + 0x600); }

void test_world(Tally &t)
{
    fill_vt();
    g_vt[1] = ptrv((void *)&t1<0x101>);
    // the game type list's exit
    for (DWORD n = 0; n < 4; n++) {
        DWORD e[2];
        BYTE copy[2][0x140];
        for (int side = 0; side < 2; side++) {
            SavedBytes saved;
            save_bytes(saved, 0x6c02e8, 0x140);
            begin(side);
            BYTE *l = at<BYTE *>(0x6c02e8);
            BYTE *rec = nullptr;
            if (n) {
                BYTE *block = static_cast<BYTE *>(heap_alloc(4 + n * 0x80));
                D(block, 0) = n;
                rec = block + 4;
                memset(rec, 0, n * 0x80);
                for (DWORD i = 0; i < n; i++) {
                    D(g_obj[side] + i * 8, 0) = ptrv(g_vt);
                    D(rec + i * 0x80, 0x78) = i == 1 ? 0 : ptrv(g_obj[side] + i * 8);
                }
            }
            D(l, 0x30) = ptrv(rec);
            D(l, 0x34) = n;
            D(g_obj[side] + 0x100, 0) = ptrv(g_vt);
            D(l, 0x13c) = n & 1 ? ptrv(g_obj[side] + 0x100) : 0;
            stand_in(0x591cd0, (void *)&s_record_list_set);
            stand_in(0x55d0c0, (void *)&s_aliases);
            e[side] = at<DWORD(__cdecl *)()>(0x4ce6e0)();
            memcpy(copy[side], l, 0x140);
            end(side);
            restore_bytes(saved);
        }
        t.check(e[0] == e[1] && same_region(copy[0], copy[1], 0x140) && same_log(), "game type list exit");
    }
    // slot 105: the load succeeding or failing, the reporter's depth 8 or 16
    for (int k = 0; k < 4; k++) {
        DWORD e[2], out[2];
        const DWORD saved_rep = *at<DWORD *>(0x6c02cc);
        for (int side = 0; side < 2; side++) {
            begin(side);
            BYTE *rep = g_obj[side] + 0x200;
            D(rep, 0x24) = k & 2 ? 8 : 16;
            *at<DWORD *>(0x6c02cc) = ptrv(rep);
            g_al = (DWORD)k & 1;
            g_vt[1] = ptrv((void *)&s_frame_delete);
            stand_in(0x4a8a90, (void *)&s_frame_ctor);
            stand_in(0x4b1cc0, (void *)&t1<0x30a>);     // FrameFilter_Ctor
            stand_in(0x4b1db0, (void *)&t1<0x30b>);     // FrameFilterList_Add
            stand_in(0x4a9a30, (void *)&s_load);
            stand_in(0x4b1d20, (void *)&t0<0x30c>);     // FrameFilter_Dtor
            void *frame = nullptr;
            e[side] = at<DWORD(THISCALL *)(void *, void **)>(0x4f12f0)(nullptr, &frame);
            out[side] = frame ? 1 : 0;
            end(side);
            *at<DWORD *>(0x6c02cc) = saved_rep;
            if (frame)
                pool_free(frame, 0x40);
        }
        t.check(e[0] == e[1] && out[0] == out[1] && same_log(), "menu background");
        if (!same_log())
            log_first_difference();
    }
    g_vt[1] = ptrv((void *)&t1<0x101>);
    // slot 8: zooms, view rectangles, depths, screen heights
    static const DWORD ZOOMS[] = {0x3f800000, 0x40000000, 0x3f000000, 0x3fc00000};
    static const DWORD HEIGHTS[] = {480, 600, 1, 2000};
    for (int k = 0; k < 16; k++) {
        DWORD e[2], g[2][2];
        const DWORD saved[2] = {*at<DWORD *>(0x618034), *at<DWORD *>(0x618038)};
        for (int side = 0; side < 2; side++) {
            begin(side);
            BYTE *w = g_obj[side];
            memset(w, 0, 0x400);
            static DWORD wvt[64], svt[64];
            for (int i = 0; i < 64; i++)
                wvt[i] = svt[i] = ptrv((void *)&t1<0x100>);
            wvt[44] = ptrv((void *)&s_net);
            D(w, 0) = ptrv(wvt);
            D(g_obj[side] + 0x600, 0) = ptrv(svt);
            svt[23] = ptrv((void *)&s_session);
            D(g_obj[side] + 0x500, 0xd8) = ZOOMS[k & 3];
            D(w, 0x2e4) = 10;
            D(w, 0x2e8) = 20;
            D(w, 0x2ec) = 10 + 333 * (DWORD)(k + 1);
            D(w, 0x2f0) = 20 + 211 * (DWORD)(k + 1);
            D(w, 0x2a4) = 0x2a4;
            D(w, 0x2a8) = 0x2a8;
            BYTE *screen = g_obj[side] + 0x700;
            static DWORD scvt[64];
            for (int i = 0; i < 64; i++)
                scvt[i] = ptrv((void *)&t1<0x100>);
            scvt[39] = ptrv((void *)&s_width);
            scvt[45] = ptrv((void *)&s_height);
            D(screen, 0) = ptrv(scvt);
            D(w, 4) = ptrv(screen);
            g_depth = k & 4 ? 16 : 8;
            g_height = HEIGHTS[(k >> 2) & 3];
            stand_in(0x55d030, (void *)&s_depth);
            stand_in(0x4ab5a0, (void *)&t3<0x30d>);   // CFrame_SetSize
            stand_in(0x4f6980, (void *)&t1<0x30e>);   // World::SetViewRect
            stand_in(0x4f6770, (void *)&t2<0x30f>);   // World::SetZoom
            e[side] = at<DWORD(THISCALL *)(void *)>(0x4f2430)(w);
            g[side][0] = *at<DWORD *>(0x618034);
            g[side][1] = *at<DWORD *>(0x618038);
            end(side);
            *at<DWORD *>(0x618034) = saved[0];
            *at<DWORD *>(0x618038) = saved[1];
        }
        t.check(e[0] == e[1] && memcmp(g[0], g[1], 8) == 0 && same_log(), "screen size changed");
        if (!same_log())
            log_first_difference();
    }
}

// ---- CInventory's slot 11 ------------------------------------------------------------------------------------------

CFileBuf g_file;
DWORD THISCALL s_read_stream(void *o, CFileBuf *f)
{
    char line[0x100] = {};
    const DWORD r = at<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(f, line, 0xff);
    ev(0x401, orel(ptrv(o)), text_of(line), r & 0xff);
    D(o, 8) = 0x4444;
    return result(1);
}
void test_inventory(Tally &t)
{
    fill_vt();
    g_vt[2] = ptrv((void *)&t1<0x102>);
    static const char *const TEXTS[] = {"{\r\nA\r\nB\r\n}\r\n", "{\r\n}\r\n", "{\r\nOne\r\nTwo\r\nThree\r\nFour\r\n}\r\nrest\r\n", "{\r\nx\r\ny\r\n  }\r\n"};
    for (int k = 0; k < 8; k++) {
        DWORD e[2], pos[2];
        for (int side = 0; side < 2; side++) {
            BYTE *inv = g_obj[side];
            memset(inv, 0, 0x40);
            D(inv + 0x18, 0) = ptrv(g_vt);
            const char *text = TEXTS[k % 4];
            const DWORD n = (DWORD)lstrlenA(text);
            at<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&g_file);
            BYTE *copy = static_cast<BYTE *>(heap_alloc(n));
            memcpy(copy, text, n);
            void *m = pool_alloc(sizeof(MemoryStream));
            at<MemoryStream *(THISCALL *)(void *, BYTE *, DWORD)>(0x505fb0)(m, copy, n);
            at<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(&g_file, (ZStream *)m, 4);
            begin(side);
            stand_in(0x417500, (void *)&t3<0x402>);   // CObject_ReadOwnLine
            stand_in(0x416610, (void *)&t0<0x403>);   // CObject_Ctor
            stand_in(0x4173c0, (void *)&t0<0x404>);   // Object_InitFields
            stand_in(0x416af0, (void *)&s_read_stream);
            stand_in(0x4173f0, (void *)&t0<0x405>);   // Object_FreeFields
            stand_in(0x4166d0, (void *)&t0<0x406>);   // CObject_Dtor
            e[side] = orel(at<DWORD(THISCALL *)(void *, const char *, const char *, void *)>(0x424da0)(inv, k & 4 ? "Inventory Holders" : "Name", "v",
                                                                                                    &g_file));
            pos[side] = at<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(&g_file);
            end(side);
            at<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&g_file);
        }
        // eax: a pointer into the line buffer on the stack when the block is empty (by its low byte)
        const bool same_eax = e[0] == e[1] || (e[0] & 0xff) == (e[1] & 0xff);
        t.check(same_eax && pos[0] == pos[1] && same_log(), "inventory slot 11");
        if (!same_log())
            log_first_difference();
    }
}

// ---- The C2 leftovers ------------------------------------------------------------------------------------------------

DWORD g_list_vt[64];
const char *THISCALL l_name(void *, DWORD i)
{
    ev(0x501, i);
    static const char *const N[] = {"Scripts/A", "B", "C/D/E"};
    return N[i % 3];
}
const char *THISCALL l_folder(void *) { ev(0x505); return "Data/Dialog"; }

void test_file_of_object(Tally &t)
{
    for (int i = 0; i < 64; i++)
        g_list_vt[i] = ptrv((void *)&t1<0x100>);
    g_list_vt[1] = ptrv((void *)&l_name);
    g_list_vt[5] = ptrv((void *)&l_folder);
    for (int k = 0; k < 12; k++) {
        DWORD e[2], text[2];
        const DWORD saved[4] = {*at<DWORD *>(0x62e810), *at<DWORD *>(0x62e814), *at<DWORD *>(0x62e7e0), *at<DWORD *>(0x62e7d4)};
        const BYTE saved_guard = *at<BYTE *>(0x62e7d8);
        for (int side = 0; side < 2; side++) {
            begin(side);
            g_seed = (unsigned)k * 7u;
            const DWORD n = rnd(5);
            BYTE *rec = static_cast<BYTE *>(heap_alloc(n * 0x80 + 1));
            memset(rec, 0, n * 0x80);
            for (DWORD i = 0; i < n; i++)
                D(rec + i * 0x80, 0x78) = ptrv(g_obj[side] + i * 0x10);
            *at<DWORD *>(0x62e810) = ptrv(rec);
            *at<DWORD *>(0x62e814) = n;
            *at<DWORD *>(0x62e7e0) = ptrv(g_list_vt);
            const bool guard = k & 1;
            if (guard) {   // the static exists: a ZString of ours
                at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(0x62e7d4), "old");
                *at<BYTE *>(0x62e7d8) = saved_guard | 1;
            } else
                *at<BYTE *>(0x62e7d8) = saved_guard & ~1;
            stand_in(0x5d0863, (void *)&c1<0x506>);   // atexit
            void *self = n && rnd(4) ? g_obj[side] + rnd(n) * 0x10 : g_obj[side] + 0x800;
            const DWORD r = at<DWORD(THISCALL *)(void *)>(0x41e460)(self);
            e[side] = r ? 1 : 0;
            text[side] = r ? text_of(at<const char *>(r)) : 0;
            const bool made = r || guard;
            text[side] ^= *at<BYTE *>(0x62e7d8) << 24;
            end(side);
            if (made)
                at<DWORD(THISCALL *)(void *)>(0x4defd0)(at<void *>(0x62e7d4));
            *at<DWORD *>(0x62e810) = saved[0];
            *at<DWORD *>(0x62e814) = saved[1];
            *at<DWORD *>(0x62e7e0) = saved[2];
            *at<DWORD *>(0x62e7d4) = saved[3];
            *at<BYTE *>(0x62e7d8) = saved_guard;
        }
        t.check(e[0] == e[1] && text[0] == text[1] && same_log(), "dialogue script file of object");
    }
}

// The Ogg reader's destructor on arrays of our own (a vtable whose slot 2 appends)
struct Arr {
    DWORD vftable, data, count, capacity;
};
DWORD g_states[2][8];
BYTE g_flags[2][8];
DWORD THISCALL arr_add(Arr *a, const void *v)
{
    const bool states = a == at<Arr *>(0x7160e0);
    ev(0x601, states, states ? orel(*static_cast<const DWORD *>(v)) : *static_cast<const BYTE *>(v));
    if (a->count < 8) {
        if (states)
            g_states[g_side][a->count] = *static_cast<const DWORD *>(v);
        else
            g_flags[g_side][a->count] = *static_cast<const BYTE *>(v);
        a->count++;
    }
    return result(1);
}
DWORD g_arr_vt[8];
DWORD g_take_n;
DWORD THISCALL s_take(void *) { ev(0x602); return g_states[g_side][g_take_n++ % 4]; }

void test_ogg(Tally &t)
{
    for (int i = 0; i < 8; i++)
        g_arr_vt[i] = ptrv((void *)&t1<0x100>);
    g_arr_vt[2] = ptrv((void *)&arr_add);
    for (int k = 0; k < 16; k++) {
        DWORD e[2];
        BYTE fl[2][8];
        Arr arr[2][2];
        SavedBytes s1, s2;
        const BYTE saved_guard = *at<BYTE *>(0x7160c0), saved_once = *at<BYTE *>(0x6d0164);
        for (int side = 0; side < 2; side++) {
            save_bytes(s1, 0x7160e0, 0x10);
            save_bytes(s2, 0x7160f0, 0x10);
            begin(side);
            g_seed = (unsigned)k * 3u;
            const DWORD n = 1 + rnd(4);
            for (DWORD i = 0; i < 8; i++) {
                g_states[side][i] = ptrv(g_obj[side] + 0x100 + i * 0x10);
                g_flags[side][i] = (BYTE)rnd(2);
            }
            *at<Arr *>(0x7160e0) = {ptrv(g_arr_vt), ptrv(g_states[side]), n, 8};
            *at<Arr *>(0x7160f0) = {ptrv(g_arr_vt), ptrv(g_flags[side]), n, 8};
            *at<BYTE *>(0x7160c0) = (BYTE)(k & 1 ? 1 : 0);
            *at<BYTE *>(0x6d0164) = (BYTE)((k >> 1) & 1);
            g_take_n = (DWORD)k;
            BYTE *self = g_obj[side];
            memset(self, 0, 0x20);
            self[0x1c] = (BYTE)((k >> 2) & 1);
            D(self, 0x18) = g_states[side][rnd(6)];
            stand_in(0x5c3bf0, (void *)&f1<0x603>);    // ov_clear
            stand_in(0x525740, (void *)&t0<0x604>);    // OggStates::ArrayCtor
            stand_in(0x525760, (void *)&t0<0x605>);    // PoolFlags::ArrayCtor
            stand_in(0x5d0863, (void *)&c1<0x606>);    // atexit
            stand_in(0x5255e0, (void *)&s_take);
            stand_in(0x57cdb0, (void *)&t1<0x607>);    // StreamBuffers::Give
            stand_in(0x57d110, (void *)&t0<0x608>);    // CSoundReader::Dtor
            stand_in(0x54d130, (void *)&f2<0x609>);    // the pool's free
            e[side] = orel(at<DWORD(THISCALL *)(void *, DWORD)>(0x525820)(self, (DWORD)k >> 3));
            memcpy(fl[side], g_flags[side], 8);
            memcpy(arr[side], at<void *>(0x7160e0), 0x10);
            memcpy(arr[side][1].count ? &arr[side][1] : &arr[side][1], at<void *>(0x7160f0), 0x10);
            arr[side][0].data = arr[side][1].data = 0;
            fl[side][7] ^= *at<BYTE *>(0x7160c0) ^ (*at<BYTE *>(0x6d0164) << 4);
            end(side);
            restore_bytes(s1);
            restore_bytes(s2);
            *at<BYTE *>(0x7160c0) = saved_guard;
            *at<BYTE *>(0x6d0164) = saved_once;
        }
        t.check(e[0] == e[1] && memcmp(fl[0], fl[1], 8) == 0 && memcmp(arr[0], arr[1], sizeof arr[0]) == 0 && same_log(), "ogg reader destructor");
        if (!same_log())
            log_first_difference();
    }
}

}  // namespace

ZAX_CHECKS(c4_rest)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_c4_rest;
    if (!arena_setup()) {
        log("Check c4 rest: no arena");
        return;
    }
    Tally commands{"c4 rest: chat commands"};
    test_commands(commands);
    commands.report();
    Tally world{"c4 rest: game type list exit, the world's slots 8 and 105"};
    test_world(world);
    world.report();
    Tally inv{"c4 rest: CInventory's slot 11"};
    test_inventory(inv);
    inv.report();
    Tally left{"c4 rest: the C2 leftovers"};
    test_file_of_object(left);
    test_ogg(left);
    left.report();
}

}  // namespace zax
