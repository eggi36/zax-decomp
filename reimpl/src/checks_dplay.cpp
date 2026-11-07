// Checks of the game side of the DirectPlay 8 net game (dplay.cpp) against the original, in the game
// thread (side 0 with the module's hooks out, side 1 through the reimplementation;
// checks_net_common.h). No real DirectPlay object and no network: these functions only run in a real
// network session, so they are checked with fakes and recorded messages only.
//  - COM: fake server, client and address objects (stdcall slots that record their arguments: AddRef,
//    Release, Initialize, EnumServiceProviders filling DPN_SERVICE_PROVIDER_INFO records with wide
//    names, GetClientInfo filling DPN_PLAYER_INFO, ReturnBuffer); the import entries of
//    CoCreateInstance, CoUninitialize and DeleteCriticalSection point to stand-ins during the test (the
//    stand-in gives the fake objects or REGDB_E_CLASSNOTREG and other errors); the error box 0x4d24c0,
//    the GameSpy disconnect 0x5c5750 and GameRandom 0x55c4e0 jump to recorders. The globals
//    0x6bdc20-0x6bdc37 (counters, the three objects) and the error text buffer are saved and restored;
//    the three critical sections are initialised for the test if the game has not done so.
//  - Messages: a net game of our own (fake game type with slots 2, 9, 10, 20, fake player sessions with
//    a name ZString and a text ZString) with random slots, players, queue and running flag; random
//    sequences of recorded messages (create/destroy player with contexts 0-102, enum hosts responses
//    with names and addresses, indicate connect, receive, send complete with errors, terminate with a
//    text, unknown ids) mixed with the slot processing and the message processing. Compared: the
//    results, the changed message fields, the slots, the players, the queue, the terminate text, the
//    counters, the host list, the log.
//  - Creation: CreateDirectPlay (server or client, old objects there or not, every CoCreateInstance
//    result, Initialize failing, the address object there or not), EnumServiceProviders (0-6
//    providers, the buffer too small first, errors, null names), InitServiceProviders; the provider list
//    (names, GUIDs) deleted through its slot 0. Compared: al and the upper bytes, the globals, the list,
//    the log.
//  - The server info answer and the deleting destructor (flags 0) on our net game: the log, the queue,
//    the arrays.
//  - The host item's deleting destructor 0x480310 on chains of hosts (phase 6 batch 6-0).
#include "checks_net_common.h"
#include "dplay.h"
#include "small_helpers.h"
#include "exe_base.h"

WINIMPORT void WINAPI InitializeCriticalSection(void *cs);
WINIMPORT void WINAPI DeleteCriticalSection(void *cs);

namespace zax {

int hooks_dplay(Hook *out);

namespace {

using namespace netcheck;

// ---- Fake COM objects -------------------------------------------------------------------------------------

struct Com {
    DWORD vftable;
    DWORD id;
    DWORD hr[32];
    DWORD vt[32];
};
DWORD STDCALL com_addref(Com *c)
{
    ev(0x600, c->id);
    return 2;
}
DWORD STDCALL com_release(Com *c)
{
    ev(0x601, c->id);
    return 0;
}
DWORD STDCALL com_initialize(Com *c, void *ctx, DWORD handler, DWORD flags)
{
    ev(0x602, c->id, ptrv(ctx), handler);
    ev(0x603, flags);
    return c->hr[3];
}
// EnumServiceProviders: g_providers records (the GUID from the index, the names below)
DWORD g_providers, g_provider_error;
// No null names: the original then uses its stack buffer as it is (stale bytes, not comparable)
const WORD *const WNAMES[] = {(const WORD *)u"DirectPlay8 TCP/IP Service Provider", (const WORD *)u"",
                              (const WORD *)u"A", (const WORD *)u"IPX", (const WORD *)u"Modem Service Provider",
                              (const WORD *)u"Serial"};
DWORD g_extra;
// The kind of a buffer: 0 none, 1 on this thread's stack, 2 elsewhere (the heap). The heap block's
// identity is not compared: whether HeapAlloc gives a just freed block back depends on the heap's state
// (Windows' low-fragmentation heap picks blocks at random once a size class is busy); after checks that
// allocate many blocks (crt_impl, 2026-10-28) the identity differed between the sides in 16 of 200 cases.
DWORD buffer_kind(const void *p)
{
    DWORD base, limit;
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(base), "=r"(limit));
    return !p ? 0 : ptrv(p) >= limit && ptrv(p) < base ? 1 : 2;
}
DWORD STDCALL com_enum(Com *c, void *g1, void *g2, BYTE *buf, DWORD *size, DWORD *count, DWORD flags)
{
    ev(0x604, c->id, ptrv(g1), ptrv(g2));
    ev(0x605, buffer_kind(buf), *size, flags);
    const DWORD need = g_providers * 0x20 + g_extra;
    if (*size < need) {
        *size = need;
        return 0x80158100;
    }
    if (g_provider_error)
        return g_provider_error;
    WORD *strings = (WORD *)(buf + g_providers * 0x20);
    DWORD used = 0;
    for (DWORD i = 0; i < g_providers; i++) {
        BYTE *r = buf + i * 0x20;
        memset(r, 0, 0x20);
        for (int b = 0; b < 16; b++)
            r[4 + b] = (BYTE)(i * 16 + b + 1);
        const WORD *w = WNAMES[(i + c->hr[4]) % 6];
        if (w) {
            WORD *d = strings + used;
            DWORD n = 0;
            while (w[n] && used + n < 0x70)
                d[n] = w[n], n++;
            d[n] = 0;
            used += n + 1;
            *(WORD **)(r + 0x14) = d;
        }
    }
    *count = g_providers;
    return c->hr[5];
}
// GetClientInfo: a DPN_PLAYER_INFO with the name after the header
DWORD STDCALL com_client_info(Com *c, DWORD dpnid, BYTE *info, DWORD *size, DWORD flags)
{
    ev(0x606, c->id, dpnid, *size);
    ev(0x607, ptrv(info) != 0, flags);
    if (!info) {
        *size = 0x40;
        return c->hr[9];
    }
    if (info && *size >= 0x40) {
        WORD *name = (WORD *)(info + 0x18);
        const WORD *w = WNAMES[dpnid % 6];
        if (w) {
            int n = 0;
            for (; w[n] && n < 0x13; n++)
                name[n] = w[n];
            name[n] = 0;
            *(WORD **)(info + 8) = name;
        }
    }
    return c->hr[10];
}
DWORD STDCALL com_return(Com *c, DWORD handle, DWORD flags)
{
    ev(0x608, c->id, handle, flags);
    return 0;
}
Com *new_com(DWORD id)
{
    Com *c = (Com *)arena(sizeof(Com));
    c->vftable = ptrv(c->vt);
    c->id = id;
    c->vt[1] = ptrv((void *)&com_addref);
    c->vt[2] = ptrv((void *)&com_release);
    c->vt[3] = ptrv((void *)&com_initialize);
    c->vt[4] = ptrv((void *)&com_enum);
    c->vt[9] = ptrv((void *)&com_client_info);
    c->vt[15] = ptrv((void *)&com_return);
    c->vt[25] = ptrv((void *)&com_return);
    return c;
}

// The stand-ins of the imports and exe functions
DWORD g_create_hr[3];
Com *g_created[3];
DWORD STDCALL fake_co_create(DWORD clsid, DWORD outer, DWORD ctx, DWORD iid, void **out)
{
    ev(0x610, clsid, outer, ctx);
    ev(0x611, iid, ptrv(out));
    const int k = clsid == 0x606190 ? 0 : clsid == 0x6061a0 ? 1 : 2;
    if ((int)g_create_hr[k] >= 0)
        *out = g_created[k];
    return g_create_hr[k];
}
void STDCALL fake_co_uninit() { ev(0x612); }
void STDCALL fake_delete_cs(void *cs) { ev(0x613, ptrv(cs)); }
DWORD __fastcall fake_error_box(const char *caption, const char *text)
{
    ev(0x614, fnv_text(2166136261u, caption), fnv_text(2166136261u, text));
    return 0x11223301;
}
void __cdecl fake_disconnect(DWORD id) { ev(0x615, id); }
DWORD g_letter;
DWORD THISCALL fake_random(void *rng, DWORD lo, DWORD hi)
{
    ev(0x616, ptrv(rng), lo, hi);
    return lo + (g_letter++ % (hi - lo + 1));
}

struct Patches {
    ImportPatch create, uninit, del;
    CodePatch box, disconnect, rnd_;
    SavedBytes globals, error;
    BYTE cs_saved[3][24];
    bool cs_init[3];
    void on()
    {
        save_bytes(globals, 0x6bdc20, 0x18);
        save_bytes(error, DPLAY_ERROR_BUFFER, 0x40);
        const DWORD cs[3] = {DPLAY_LOCK, DPLAY_QUEUE_LOCK, DPLAY_TERMINATE_LOCK};
        for (int i = 0; i < 3; i++) {
            memcpy(cs_saved[i], at<void *>(cs[i]), 24);
            bool zero = true;
            for (BYTE b : cs_saved[i])
                zero = zero && !b;
            cs_init[i] = zero;
            if (zero)
                InitializeCriticalSection(at<void *>(cs[i]));
        }
        patch_import(create, IAT_CO_CREATE, (void *)&fake_co_create);
        patch_import(uninit, IAT_CO_UNINIT, (void *)&fake_co_uninit);
        patch_import(del, IAT_DELETE_CS, (void *)&fake_delete_cs);
        patch_jump(box, 0x4d24c0, (void *)&fake_error_box);
        patch_jump(disconnect, 0x5c5750, (void *)&fake_disconnect);
        patch_jump(rnd_, 0x55c4e0, (void *)&fake_random);
    }
    void off()
    {
        unpatch(rnd_);
        unpatch(disconnect);
        unpatch(box);
        unpatch_import(del);
        unpatch_import(uninit);
        unpatch_import(create);
        const DWORD cs[3] = {DPLAY_LOCK, DPLAY_QUEUE_LOCK, DPLAY_TERMINATE_LOCK};
        for (int i = 0; i < 3; i++)
            if (cs_init[i]) {
                DeleteCriticalSection(at<void *>(cs[i]));
                memcpy(at<void *>(cs[i]), cs_saved[i], 24);
            }
        restore_bytes(error);
        restore_bytes(globals);
    }
};

// ---- The net game of the test -------------------------------------------------------------------------------

DWORD g_session_vt[4];
BYTE *THISCALL session_delete(BYTE *s, DWORD flags)
{
    ev(0x620, ptrv(s), flags);
    return s;
}
constexpr int MAX_SESSIONS = 48;
BYTE *g_sessions[MAX_SESSIONS];
int g_nsessions;
BYTE *new_session(DWORD dpnid)
{
    BYTE *s = (BYTE *)arena(0x40);
    g_session_vt[0] = ptrv((void *)&session_delete);
    *(DWORD *)s = ptrv(g_session_vt);
    *(DWORD *)(s + 8) = dpnid;
    ZString *name = (ZString *)arena(4);
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(name, "old name");
    *(ZString **)(s + 0x1c) = name;
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)((ZString *)(s + 0x24), rnd(2) ? "" : "abc");
    if (g_nsessions < MAX_SESSIONS)
        g_sessions[g_nsessions++] = s;
    return s;
}
DWORD THISCALL gt_new_player(Fake *f, DWORD flag)
{
    ev(0x630, f->id, flag);
    return ptrv(new_session(0));
}

struct Game {
    DPlayGame *g;
    Fake *type;
};
Game make_game()
{
    Game m;
    g_nsessions = 0;
    m.g = (DPlayGame *)arena(0x920);
    m.type = new_fake(1);
    set3<2>(m.type);
    set1<9>(m.type);
    set1<10>(m.type);
    set1<16>(m.type);   // the GameSpy server info keys of the type
    m.type->vt[20] = ptrv((void *)&gt_new_player);
    DPlayGame *g = m.g;
    g->game.vftable = 0x5f15a8;
    g->game.game_type = m.type;
    g->game.settings = (NetGameSettings *)arena(0x40);
    g->game.settings->max_players = rnd(17);
    const DWORD np = rnd(6);
    g->game.players = {0x5fb558, (void **)heap_alloc(16 * 4), np, 16};
    memset(g->game.players.data, 0, 16 * 4);
    for (DWORD i = 0; i < np; i++)
        g->game.players.data[i] = new_session(0x100 + i);
    g->game.teams = {0x5fb54c, nullptr, 0, 0};
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g->game.state, "openplaying");
    g->running = (BYTE)rnd(2);
    g->hosting = (BYTE)rnd(2);
    g->queue = {0x5f1618, nullptr, 0, 0};
    if (rnd(2)) {
        const DWORD n = rnd(4);
        g->queue.capacity = n + rnd(3);
        g->queue.data = g->queue.capacity ? (DPlayMessage *)heap_alloc(g->queue.capacity * 20) : nullptr;
        g->queue.count = g->queue.data ? n : 0;
        for (DWORD i = 0; i < g->queue.count; i++)
            g->queue.data[i] = {rnd(4) ? rnd(8) : rnd(110), 0x100 + rnd(8), (void *)(uintptr_t)(0x9000 + i), rnd(64), 0x7000 + i};
    }
    // Every player in at most one slot (a session removed twice would not be found: data[-1])
    DWORD next_player = 0;
    for (DWORD i = 0; i < 101; i++) {
        BYTE *s = (BYTE *)g->slots + i * 12;
        if (rnd(4) == 0) {
            s[0] = (BYTE)rnd(2);
            s[1] = (BYTE)rnd(2);
            *(DWORD *)(s + 4) = 0x100 + rnd(8);
            if (next_player < np && rnd(2))
                *(void **)(s + 8) = g->game.players.data[next_player++];
        }
    }
    g->slots_changed = (BYTE)rnd(2);
    return m;
}
void st_game(const DPlayGame *g)
{
    st_bytes(g->slots, 100 * 12);
    st(g->slots_changed);
    st(*(const DWORD *)((const BYTE *)g + 0x900));
    st(g->terminated);
    st_text(g->terminate_text);
    st(g->queue.count);
    st(g->queue.capacity);
    if (g->queue.data)
        st_bytes(g->queue.data, g->queue.count * 20);
    st(g->game.players.count);
    for (DWORD i = 0; i < g->game.players.count; i++) {
        const BYTE *s = (const BYTE *)g->game.players.data[i];
        st(ptrv(s));
        st_dwords(s + 4, 0x14);
        st_zstring(*(ZString *const *)(s + 0x1c));
        st_zstring((const ZString *)(s + 0x24));
    }
    st_raw(*at<DWORD *>(DPLAY_RECEIVED));
    st_raw(*at<DWORD *>(DPLAY_PROCESS_CALLS));
    st_raw(*at<DWORD *>(DPLAY_PROCESSED));
}
void free_game(Game &m)
{
    DPlayGame *g = m.g;
    heap_free(g->game.players.data);
    if (g->queue.data)
        heap_free(g->queue.data);
    for (int i = 0; i < g_nsessions; i++) {
        at<void(THISCALL *)(ZString *)>(0x4defd0)(*(ZString **)(g_sessions[i] + 0x1c));
        at<void(THISCALL *)(ZString *)>(0x4defd0)((ZString *)(g_sessions[i] + 0x24));
    }
    at<void(THISCALL *)(ZString *)>(0x4defd0)(&g->game.state);
}

void set_globals(Com *server, Com *client, Com *address)
{
    *at<Com **>(DPLAY_SERVER) = server;
    *at<Com **>(DPLAY_CLIENT) = client;
    *at<Com **>(DPLAY_ADDRESS) = address;
}

const DWORD IDS[] = {0xffff0007, 0xffff0009, 0xffff000b, 0xffff000e, 0xffff0011, 0xffff0014, 0xffff0016};
const char *const TERMINATE[] = {"Host has quit", "", "Kicked: too many players in this session"};

void messages_case(int k)
{
    Patches pt;
    pt.on();
    g_letter = (DWORD)k;
    Game m = make_game();
    DPlayGame *g = m.g;
    Com *server = new_com(0x51), *client = new_com(0x52), *sender = new_com(0x53), *device = new_com(0x54);
    // The server is there: the slot processing only runs on the host (GetClientInfo)
    set_globals(server, rnd(2) ? client : nullptr, nullptr);
    server->hr[9] = rnd(3) ? 0x80158100 : rnd(2) ? 0 : 0x80004005;
    server->hr[10] = rnd(4) ? 0 : 0x80158130;
    void **hosts = (void **)arena(8);
    void ***host_ctx = (void ***)arena(4);
    *host_ctx = hosts;
    for (int n = 0, steps = 1 + (int)rnd(14); n < steps; n++) {
        const DWORD pick = rnd(10);
        if (pick == 8) {
            at<void(THISCALL *)(DPlayGame *)>(0x480800)(g);
            st_game(g);
            continue;
        }
        if (pick == 9) {
            at<void(THISCALL *)(DPlayGame *)>(0x480a70)(g);
            st_game(g);
            continue;
        }
        DWORD id = rnd(8) ? IDS[pick % 7] : 0xffff0000u + rnd(0x20);
        if (rnd(30) == 0)
            id = random();
        DWORD *msg = (DWORD *)arena(0x40);
        msg[0] = 0x20;
        msg[1] = 0x100 + rnd(8);
        msg[2] = rnd(3) ? rnd(8) : rnd(104);
        msg[3] = rnd(3) ? 0x80158000u + rnd(0x700) : rnd(2);
        msg[4] = rnd(64);
        msg[5] = 0x8000 + rnd(100);
        if (id == 0xffff0007 && rnd(4) == 0)
            msg[2] = 0;
        if (id == 0xffff000b) {
            BYTE *desc = (BYTE *)arena(0x40);
            *(const WORD **)(desc + 0x30) = WNAMES[rnd(6)];
            msg[1] = ptrv(sender);
            msg[2] = ptrv(device);
            msg[3] = ptrv(desc);
            msg[6] = ptrv(host_ctx);
        }
        if (id == 0xffff0016)
            msg[2] = rnd(4) ? ptrv(TERMINATE[rnd(3)]) : 0;
        const DWORD r = at<DWORD(THISCALL *)(DPlayGame *, DWORD, DWORD *)>(0x47ff50)(g, id, msg);
        ev(0x640, id, r);
        st_dwords(msg, 0x20);
    }
    st_game(g);
    // The host list: names, the addresses, then deleted through slot 0 (the addresses released)
    for (BYTE *h = (BYTE *)hosts[0]; h; h = *(BYTE **)(h + 8)) {
        st(*(DWORD *)h);
        st_text(*(const char **)(h + 4));
        st(*(DWORD *)(h + 0xc));
        st(*(DWORD *)(h + 0x10));
    }
    if (hosts[0])
        vcall<void *>(hosts[0], 0, 1u);
    free_game(m);
    pt.off();
}

void create_case(int k)
{
    Patches pt;
    pt.on();
    Game m = make_game();
    DPlayGame *g = m.g;
    Com *old_server = new_com(0x61), *old_client = new_com(0x62), *address = new_com(0x63);
    g_created[0] = new_com(0x64);
    g_created[1] = new_com(0x65);
    g_created[2] = new_com(0x66);
    static_assert(sizeof(DWORD) == 4);
    const DWORD results[] = {0, 0, 1, 0x80040154, 0x80004005, 0x80158080};
    for (DWORD &h : g_create_hr)
        h = results[rnd(6)];
    for (Com *c : g_created) {
        c->hr[3] = rnd(4) ? 0 : 0x80158150;
        c->hr[4] = rnd(6);
        c->hr[5] = rnd(5) ? 0 : 1;
    }
    set_globals(rnd(2) ? old_server : nullptr, rnd(2) ? old_client : nullptr, rnd(2) ? address : nullptr);
    g_providers = rnd(7);
    g_extra = rnd(3) ? 0x100 : 0x800 + rnd(0x400);
    g_provider_error = rnd(6) ? 0 : 0x80158410;
    for (int n = 0, steps = 1 + (int)rnd(3); n < steps; n++) {
        DWORD r;
        void *list = nullptr;
        switch (rnd(3)) {
        case 0:
            r = at<DWORD(THISCALL *)(DPlayGame *, DWORD)>(0x47fdf0)(g, rnd(2) ? random() | 1 : random() & ~0xffu);
            ev_raw(0x650, r & 0xff);
            ev(0x651, r & ~0xffu);
            break;
        case 1:
            if (!*at<void **>(DPLAY_CLIENT))
                *at<Com **>(DPLAY_CLIENT) = g_created[1];
            list = at<void *(THISCALL *)(DPlayGame *)>(0x47fc20)(g);
            break;
        default: list = at<void *(THISCALL *)(DPlayGame *)>(0x4803c0)(g); break;
        }
        for (BYTE *p = (BYTE *)list; p; p = *(BYTE **)(p + 8)) {
            st(*(DWORD *)p);
            st_text(*(const char **)(p + 4));
            BYTE *guid = *(BYTE **)(p + 0xc);
            if (guid)
                st_bytes(guid, 16);
        }
        if (list)
            ev(0x652, ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x47fdb0)(list, 1)) == ptrv(list));
        st(*at<DWORD *>(DPLAY_SERVER));
        st(*at<DWORD *>(DPLAY_CLIENT));
        st(*at<DWORD *>(DPLAY_ADDRESS));
    }
    (void)k;
    free_game(m);
    pt.off();
}

void misc_case(int k)
{
    Patches pt;
    pt.on();
    Game m = make_game();
    DPlayGame *g = m.g;
    ZString out;
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(&out, "\\hostname\\test");
    at<void(THISCALL *)(DPlayGame *, ZString *)>(0x47ee70)(g, &out);
    st_zstring(&out);
    at<void(THISCALL *)(ZString *)>(0x4defd0)(&out);
    if (k % 2) {
        set_globals(rnd(2) ? new_com(0x71) : nullptr, rnd(2) ? new_com(0x72) : nullptr, rnd(2) ? new_com(0x73) : nullptr);
        // The players are fakes (slot 0 records), the queue and arrays are freed by the destructor
        void **players = g->game.players.data;
        const DWORD np = g->game.players.count;
        (void)np;
        ev(0x660, ptrv(at<void *(THISCALL *)(DPlayGame *, DWORD)>(0x47ef10)(g, 0)) == ptrv(g));
        st(g->game.vftable);
        st(g->queue.vftable);
        st(ptrv(g->queue.data));
        st(g->queue.count);
        st(g->queue.capacity);
        st(g->game.players.vftable);
        st(ptrv(g->game.players.data));
        st(g->game.players.count);
        st(g->game.teams.vftable);
        st(*at<DWORD *>(DPLAY_SERVER));
        st(*at<DWORD *>(DPLAY_CLIENT));
        st(*at<DWORD *>(DPLAY_ADDRESS));
        (void)players;
        for (int i = 0; i < g_nsessions; i++) {
            at<void(THISCALL *)(ZString *)>(0x4defd0)(*(ZString **)(g_sessions[i] + 0x1c));
            at<void(THISCALL *)(ZString *)>(0x4defd0)((ZString *)(g_sessions[i] + 0x24));
        }
    } else {
        free_game(m);
    }
    pt.off();
}

// The host item's deleting destructor 0x480310 (phase 6 batch 6-0): a chain of 1-3 hosts (the exe's node
// constructor, the name on the heap, fake address objects), the first deleted through the destructor's address
// with flags 0-3 (the next ones through their slot 0 from the node's destructor). Compared: eax, the releases in
// their order, and with flags 0 the item afterwards (vtable, name, next, the addresses), then freed here.
void host_case(int k)
{
    Patches pt;
    pt.on();
    (void)k;
    const char *names[] = {"Zax Server", "", "LAN game of the long name", "x"};
    DPlayHost *next = nullptr, *first = nullptr;
    for (int i = 0, n = 1 + (int)rnd(3); i < n; i++) {
        DPlayHost *h = (DPlayHost *)heap_alloc(0x14);
        at<void(THISCALL *)(void *, const char *, void *)>(0x51eb40)(h, names[rnd(4)], next);
        h->vftable = 0x5f1628;
        h->host_address = new_com(0x80 + 2 * (DWORD)i);
        h->device_address = new_com(0x81 + 2 * (DWORD)i);
        next = first = h;
    }
    const DWORD flags = rnd(4);
    const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x480310)(first, flags);
    ev(0x690, r == ptrv(first), flags);
    if (!(flags & 1)) {
        st(first->vftable);
        st(ptrv(first->name));
        st(ptrv(first->next));
        st(ptrv(first->host_address));
        st(ptrv(first->device_address));
        heap_free(first);
    }
    pt.off();
}

}  // namespace

ZAX_CHECKS(dplay)
{
    (void)cls;
    (void)n;
    g_table = hooks_dplay;
    run_cases("dplay: messages, slots, received queue", 400, 0x47ff50u, [](int k) { messages_case(k); });
    run_cases("dplay: DirectPlay objects and service providers", 200, 0x47fdf0u, [](int k) { create_case(k); });
    run_cases("dplay: server info and destructor", 60, 0x47ee70u, [](int k) { misc_case(k); });
    run_cases("dplay: the host item's destructor", 60, 0x480310u, [](int k) { host_case(k); });
}

}  // namespace zax
