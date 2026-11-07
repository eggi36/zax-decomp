// Checks of the rest of the DirectPlay 8 net game (dplay_rest5a.cpp) against the original, in the game thread
// (side 0 with the module's hooks out, side 1 through the reimplementation; checks_net_common.h). No real
// DirectPlay object and no network:
//  - COM: fake server, client, address and IP-address objects whose vtables have stdcall recorders for every
//    slot the functions use (arguments and the contents of the structures passed: player info, application
//    description with the session name, buffer descriptors, URLs, components, GUIDs); the client's
//    EnumHosts calls the message callback 0x47ead0 with host answers as DirectPlay does (synchronously);
//    the server hands out local addresses with URLs and client addresses, the address the IP interface.
//  - Imports: CoCreateInstance, CoInitialize, CoUninitialize, Initialize/Delete/Enter/LeaveCriticalSection and
//    Sleep point to recorders for the test. The critical sections and Sleep record only calls of the check
//    thread on the three DirectPlay locks (Enter/Leave pass on to the real ones; Initialize/Delete of the
//    DirectPlay locks are only recorded) and pass every other call on, so the game's other threads are not
//    disturbed; the lock order is part of the compared log.
//  - Exe functions as recorders: the error box, GameSpy_Start/Stop/Think and the GameSpy answers
//    (BasicInfo, Rules, Teams, PlayerInfo), the GameSpy disconnect, GameRandom, the static part array's
//    DeleteAll (it would empty the running level's array). The globals 0x6bdc20-0x6bdc37, the error text
//    buffer and the factory pointer 0x6d00a8 are saved and restored.
//  - The net game: our own object with the real vtable 0x5f15a8, a fake game type, fake player sessions
//    (settings with a ZString name), random slots and queue.
// Cases: the factory (and its object deleted by 0x47ef10), the static initializer, the callback with
// messages; random sequences of ResetPlayers, Host, Join (by name with every BuildFromURLA answer, or by host
// item), Close, IsAvailable, SetProvider, CreateAndSetProvider, SetProviderByName, EnumHosts with every
// COM result; Frame (terminated or not, host or client), Send and SendAndReset on real packets, Kick,
// PlayerAddress, Ping; the GameSpy answers. Compared: eax, the log, the game object, the globals.
#include "checks_net_common.h"
#include "dplay.h"
#include "small_helpers.h"
#include "exe_base.h"

WINIMPORT void WINAPI InitializeCriticalSection(void *cs);
WINIMPORT void WINAPI DeleteCriticalSection(void *cs);

namespace zax {

int hooks_dplay_rest5a(Hook *out);

namespace {

using namespace netcheck;

DWORD g_tid;
inline DWORD tid()
{
    DWORD t;
    __asm__("movl %%fs:0x24, %0" : "=r"(t));
    return t;
}
bool dplay_lock(DWORD cs) { return cs == DPLAY_LOCK || cs == DPLAY_QUEUE_LOCK || cs == DPLAY_TERMINATE_LOCK; }

unsigned whash(const WORD *w)
{
    if (!w)
        return 0xdead0002;
    unsigned h = 2166136261u;
    for (int i = 0; i < 0x200 && w[i]; i++)
        h = fnv(h, &w[i], 2);
    return h;
}
DWORD kind(const void *p)
{
    DWORD base, limit;
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(base), "=r"(limit));
    return !p ? 0 : in_arena(ptrv(p)) ? ptrv(p) : ptrv(p) >= limit && ptrv(p) < base ? 1 : 2;
}

// ---- Fake COM objects ---------------------------------------------------------------------------------------------

struct Com {
    DWORD vftable;
    DWORD id;
    DWORD hr[40];
    DWORD vt[40];
    char url[96];
};
constexpr DWORD BUFFER_TOO_SMALL = 0x80158100;
void *g_game;
Com *g_ip;
Com *g_local[4];
DWORD g_nlocal;
Com *g_client_address;
DWORD g_hosts;

DWORD STDCALL c_addref(Com *c) { ev(0x600, c->id); return 2; }
DWORD STDCALL c_release(Com *c) { ev(0x601, c->id); return c->hr[2]; }
DWORD STDCALL c_initialize(Com *c, void *ctx, DWORD handler, DWORD flags)
{
    ev(0x602, c->id, ptrv(ctx), handler);
    ev(0x603, flags);
    return c->hr[3];
}
void log_info(DWORD id, const DWORD *info)
{
    ev(0x604, id, info[0], info[1]);
    ev(0x605, whash((const WORD *)(uintptr_t)info[2]), info[3], info[4]);
}
DWORD STDCALL c_set_info(Com *c, const DWORD *info, DWORD ctx, DWORD handle, DWORD flags)
{
    log_info(c->id, info);
    ev(0x606, ctx, handle, flags);
    return c->hr[c->id == 0x51 ? 8 : 11];
}
// EnumServiceProviders (the client): provider records with names
const WORD *const WNAMES[] = {(const WORD *)u"DirectPlay8 TCP/IP Service Provider", (const WORD *)u"",
                              (const WORD *)u"A", (const WORD *)u"IPX", (const WORD *)u"Modem Service Provider",
                              (const WORD *)u"Serial"};
const char *const NAMES[] = {"DirectPlay8 TCP/IP Service Provider", "IPX", "Serial", "Missing Provider", ""};
DWORD g_providers;
DWORD STDCALL c_enum_providers(Com *c, void *g1, void *g2, BYTE *buf, DWORD *size, DWORD *count, DWORD flags)
{
    ev(0x607, c->id, ptrv(g1), ptrv(g2));
    ev(0x608, buf != nullptr, *size, flags);
    const DWORD need = g_providers * 0x20 + 0x100;
    if (*size < need) {
        *size = need;
        return BUFFER_TOO_SMALL;
    }
    WORD *strings = (WORD *)(buf + g_providers * 0x20);
    DWORD used = 0;
    for (DWORD i = 0; i < g_providers; i++) {
        BYTE *r = buf + i * 0x20;
        memset(r, 0, 0x20);
        for (int b = 0; b < 16; b++)
            r[4 + b] = (BYTE)(i * 16 + b + 1);
        const WORD *w = WNAMES[(i + c->hr[30]) % 6];
        WORD *d = strings + used;
        DWORD n = 0;
        while (w[n] && used + n < 0x70)
            d[n] = w[n], n++;
        d[n] = 0;
        used += n + 1;
        *(WORD **)(r + 0x14) = d;
    }
    *count = g_providers;
    return c->hr[4];
}
DWORD STDCALL c_client_info(Com *c, DWORD dpnid, BYTE *info, DWORD *size, DWORD flags)
{
    ev(0x609, c->id, dpnid, *size);
    ev(0x60a, info != nullptr, flags);
    if (!info) {
        *size = 0x40;
        return BUFFER_TOO_SMALL;
    }
    WORD *name = (WORD *)(info + 0x18);
    const WORD *w = WNAMES[dpnid % 6];
    int n = 0;
    for (; w[n] && n < 0x13; n++)
        name[n] = w[n];
    name[n] = 0;
    *(WORD **)(info + 8) = name;
    return c->hr[9];
}
DWORD STDCALL c_client_address(Com *c, DWORD dpnid, Com **out, DWORD flags)
{
    ev(0x60b, c->id, dpnid, flags);
    *out = g_client_address;
    return c->hr[10];
}
DWORD STDCALL c_local_addresses(Com *c, Com **list, DWORD *count, DWORD flags)
{
    ev(0x60c, c->id, list != nullptr, *count);
    ev(0x60d, flags);
    if (!list) {
        *count = g_nlocal;
        return BUFFER_TOO_SMALL;
    }
    for (DWORD i = 0; i < g_nlocal && i < *count; i++)
        list[i] = g_local[i];
    return c->hr[11];
}
void log_desc(const DWORD *d)
{
    ev(0x610, d[0], d[1], d[10]);
    ev(0x611, fnv(2166136261u, d + 6, 16), whash((const WORD *)(uintptr_t)d[12]), d[17]);
    st_text((const char *)(uintptr_t)d[16]);
    ev(0x612, d[2], d[3], d[4]);
}
DWORD STDCALL c_host(Com *c, const DWORD *desc, Com **addresses, DWORD n, DWORD sec, DWORD cred, DWORD ctx, DWORD flags)
{
    ev(0x613, c->id, ptrv(addresses), addresses ? (*addresses ? (*addresses)->id : 0) : 0xdead);
    log_desc(desc);
    ev(0x614, n, sec, cred);
    ev(0x615, ctx, flags);
    return c->hr[13];
}
void log_buffer(const DWORD *b, DWORD n)
{
    ev(0x616, n, b[0], kind((const void *)(uintptr_t)b[1]));
    st_bytes((const void *)(uintptr_t)b[1], b[0]);
}
DWORD STDCALL c_send_to(Com *c, DWORD dpnid, const DWORD *buffers, DWORD n, DWORD timeout, DWORD ctx, DWORD *handle, DWORD flags)
{
    ev(0x617, c->id, dpnid, timeout);
    log_buffer(buffers, n);
    ev(0x618, ctx, kind(handle), flags);
    if (handle)
        *handle = 0x4711;
    return c->hr[14];
}
DWORD STDCALL c_close(Com *c, DWORD flags) { ev(0x619, c->id, flags); return c->hr[c->id == 0x51 ? 23 : 14]; }
DWORD STDCALL c_destroy_client(Com *c, DWORD dpnid, const char *data, DWORD size, DWORD flags)
{
    ev(0x61a, c->id, dpnid, size);
    ev(0x61b, flags, data ? fnv(2166136261u, data, size) : 0);
    return c->hr[24];
}
DWORD STDCALL c_return(Com *c, DWORD handle, DWORD flags) { ev(0x61c, c->id, handle, flags); return 0; }
DWORD STDCALL c_server_conn(Com *c, DWORD dpnid, DWORD *info, DWORD flags)
{
    ev(0x61d, c->id, dpnid, info[0]);
    ev(0x61e, flags);
    info[1] = c->hr[33];
    return c->hr[32];
}
DWORD STDCALL c_client_conn(Com *c, DWORD *info, DWORD flags)
{
    ev(0x61d, c->id, 0, info[0]);
    ev(0x61e, flags);
    info[1] = c->hr[21];
    return c->hr[20];
}
DWORD STDCALL c_enum_hosts(Com *c, const DWORD *desc, Com *host, Com *device, DWORD data, DWORD size, DWORD count, DWORD retry,
                           DWORD timeout, void ***ctx, DWORD handle, DWORD flags)
{
    ev(0x620, c->id, host ? host->id : 0, device ? device->id : 0);
    log_desc(desc);
    ev(0x621, data, size, count);
    ev(0x622, retry, timeout, handle);
    ev(0x623, flags, kind(ctx));
    // DirectPlay answers synchronously through the message callback
    for (DWORD i = 0; i < g_hosts; i++) {
        DWORD *msg = (DWORD *)arena(0x40);
        BYTE *d = (BYTE *)arena(0x40);
        *(const WORD **)(d + 0x30) = WNAMES[(i + 2) % 6];
        Com *sender = (Com *)arena(sizeof(Com)), *dev = (Com *)arena(sizeof(Com));
        sender->vftable = dev->vftable = c->vt[39];   // the address vtable
        sender->id = 0x70 + i;
        dev->id = 0x78 + i;
        msg[0] = 0x20;
        msg[1] = ptrv(sender);
        msg[2] = ptrv(dev);
        msg[3] = ptrv(d);
        msg[6] = ptrv(ctx);
        const DWORD r = at<DWORD(STDCALL *)(void *, DWORD, DWORD *)>(0x47ead0)(g_game, 0xffff000b, msg);
        ev(0x624, r);
    }
    return c->hr[5];
}
DWORD STDCALL c_connect(Com *c, const DWORD *desc, Com *host, Com *device, DWORD a, DWORD b, DWORD d, DWORD e, DWORD f, DWORD g,
                        DWORD flags)
{
    ev(0x625, c->id, host ? host->id : 0, device ? device->id : 0);
    log_desc(desc);
    ev(0x626, a, b, d);
    ev(0x627, e, f, g);
    ev(0x628, flags);
    return c->hr[7];
}
DWORD STDCALL c_send(Com *c, const DWORD *buffers, DWORD n, DWORD timeout, DWORD ctx, DWORD *handle, DWORD flags)
{
    ev(0x629, c->id, timeout, ctx);
    log_buffer(buffers, n);
    ev(0x62a, kind(handle), flags, handle ? *handle : 0xdead);
    if (handle)
        *handle = 0x4712;
    return c->hr[8];
}
// The address
DWORD STDCALL a_qi(Com *c, DWORD iid, Com **out)
{
    ev(0x630, c->id, iid);
    *out = c->hr[1] ? g_ip : nullptr;
    return c->hr[0];
}
DWORD STDCALL a_build(Com *c, const char *url)
{
    ev(0x631, c->id, fnv_text(2166136261u, url));
    return c->hr[4];
}
DWORD STDCALL a_get_url(Com *c, char *buf, DWORD *size)
{
    ev(0x632, c->id, buf != nullptr, *size);
    const DWORD n = (DWORD)lstrlenA(c->url) + 1;
    if (!buf || *size < n) {
        *size = n;
        return BUFFER_TOO_SMALL;
    }
    memcpy(buf, c->url, n);
    return c->hr[10];
}
DWORD STDCALL a_set_sp(Com *c, const BYTE *guid)
{
    ev(0x633, c->id, guid ? fnv(2166136261u, guid, 16) : 0);
    return c->hr[13];
}
DWORD STDCALL a_add(Com *c, const WORD *name, const BYTE *data, DWORD size, DWORD type)
{
    ev(0x634, c->id, whash(name), size);
    ev(0x635, type, size < 0x400 ? fnv(2166136261u, data, size) : 0);
    return c->hr[18];
}
DWORD STDCALL ip_sock(Com *c, BYTE *sa, DWORD *size)
{
    ev(0x636, c->id, *size);
    for (int i = 0; i < 16; i++)
        sa[i] = (BYTE)(c->hr[7] >> (i % 4 * 8));
    return c->hr[6];
}

DWORD g_server_vt[40], g_client_vt[40], g_address_vt[40], g_ip_vt[40];
void init_vtables()
{
    DWORD *all[] = {g_server_vt, g_client_vt, g_address_vt, g_ip_vt};
    for (DWORD *v : all) {
        v[1] = ptrv((void *)&c_addref);
        v[2] = ptrv((void *)&c_release);
    }
    g_server_vt[3] = g_client_vt[3] = ptrv((void *)&c_initialize);
    g_server_vt[8] = g_client_vt[11] = ptrv((void *)&c_set_info);
    g_server_vt[9] = ptrv((void *)&c_client_info);
    g_server_vt[10] = ptrv((void *)&c_client_address);
    g_server_vt[11] = ptrv((void *)&c_local_addresses);
    g_server_vt[13] = ptrv((void *)&c_host);
    g_server_vt[14] = ptrv((void *)&c_send_to);
    g_server_vt[23] = g_client_vt[14] = ptrv((void *)&c_close);
    g_server_vt[24] = ptrv((void *)&c_destroy_client);
    g_server_vt[25] = g_client_vt[15] = ptrv((void *)&c_return);
    g_server_vt[32] = ptrv((void *)&c_server_conn);
    g_client_vt[4] = ptrv((void *)&c_enum_providers);
    g_client_vt[5] = ptrv((void *)&c_enum_hosts);
    g_client_vt[7] = ptrv((void *)&c_connect);
    g_client_vt[8] = ptrv((void *)&c_send);
    g_client_vt[20] = ptrv((void *)&c_client_conn);
    g_client_vt[39] = ptrv(g_address_vt);
    g_address_vt[0] = ptrv((void *)&a_qi);
    g_address_vt[4] = ptrv((void *)&a_build);
    g_address_vt[10] = ptrv((void *)&a_get_url);
    g_address_vt[13] = ptrv((void *)&a_set_sp);
    g_address_vt[18] = ptrv((void *)&a_add);
    g_ip_vt[6] = ptrv((void *)&ip_sock);
}
const DWORD FAILS[] = {0x80004005, 0x80158450, 0x80158410, 0x80158080};
DWORD maybe_fail(DWORD one_in)
{
    return rnd(one_in) ? 0 : FAILS[rnd(4)];
}
Com *new_com(DWORD id, const DWORD *vt)
{
    Com *c = (Com *)arena(sizeof(Com));
    memcpy(c->vt, vt, sizeof c->vt);
    c->vftable = ptrv(c->vt);
    c->id = id;
    for (int i = 3; i < 40; i++)
        c->hr[i] = maybe_fail(10);
    c->hr[1] = rnd(4);   // QueryInterface: an object or not
    c->hr[2] = rnd(3);   // Release's count
    c->hr[5] = maybe_fail(8);
    c->hr[7] = random();
    c->hr[21] = c->hr[33] = rnd(500);
    c->hr[30] = rnd(6);
    lstrcpyA(c->url, rnd(2) ? "x-directplay:/provider=%7BEBFE7BA0%7D;hostname=10.0.0.5;port=26784" : "");
    if (c->url[0] && rnd(2))
        c->url[20 + rnd(20)] = 0;
    return c;
}

// The address objects CoCreateInstance hands out (one after the other), the server and the client
Com *g_addresses[4];
int g_next_address;
Com *g_new_server, *g_new_client;
DWORD g_create_fail;
bool g_fail_now;
DWORD STDCALL fake_co_create(DWORD clsid, DWORD outer, DWORD ctx, DWORD iid, void **out)
{
    ev(0x640, clsid, outer, ctx);
    ev(0x641, iid, kind(out));
    if (g_fail_now && g_create_fail && rnd(g_create_fail) == 0)
        return rnd(2) ? 0x80040154 : 0x80004005;
    if (clsid == 0x606190)
        *out = g_new_server;
    else if (clsid == 0x6061a0)
        *out = g_new_client;
    else
        *out = g_addresses[g_next_address++ & 3];
    return 0;
}
DWORD STDCALL fake_co_init(void *p) { ev(0x642, ptrv(p)); return 0; }
void STDCALL fake_co_uninit() { ev(0x643); }
DWORD g_real[5];   // InitializeCS, DeleteCS, EnterCS, LeaveCS, Sleep
void STDCALL fake_init_cs(void *cs)
{
    if (tid() == g_tid && dplay_lock(ptrv(cs)))
        return ev(0x644, ptrv(cs));
    ((void(STDCALL *)(void *))(uintptr_t)g_real[0])(cs);
}
void STDCALL fake_delete_cs(void *cs)
{
    if (tid() == g_tid && dplay_lock(ptrv(cs)))
        return ev(0x645, ptrv(cs));
    ((void(STDCALL *)(void *))(uintptr_t)g_real[1])(cs);
}
void STDCALL fake_enter_cs(void *cs)
{
    if (tid() == g_tid && dplay_lock(ptrv(cs)))
        ev(0x646, ptrv(cs));
    ((void(STDCALL *)(void *))(uintptr_t)g_real[2])(cs);
}
void STDCALL fake_leave_cs(void *cs)
{
    if (tid() == g_tid && dplay_lock(ptrv(cs)))
        ev(0x647, ptrv(cs));
    ((void(STDCALL *)(void *))(uintptr_t)g_real[3])(cs);
}
void STDCALL fake_sleep(DWORD ms)
{
    if (tid() == g_tid)
        return ev(0x648, ms);
    ((void(STDCALL *)(DWORD))(uintptr_t)g_real[4])(ms);
}
DWORD __fastcall fake_error_box(const char *caption, const char *text)
{
    ev(0x650, fnv_text(2166136261u, caption), fnv_text(2166136261u, text));
    return 0x11223301;
}
DWORD THISCALL fake_gamespy0(void *g) { ev(0x651, ptrv(g)); return 0x5501; }
DWORD THISCALL fake_gamespy_stop(void *g) { ev(0x652, ptrv(g)); return 0x5502; }
DWORD THISCALL fake_gamespy_think(void *g) { ev(0x653, ptrv(g)); return 0x5503; }
DWORD THISCALL fake_basic_info(void *g, ZString *out) { ev(0x654, ptrv(g), ptrv(out)); return 0x5504; }
DWORD THISCALL fake_rules(void *g, ZString *out) { ev(0x655, ptrv(g), ptrv(out)); return 0x5505; }
DWORD THISCALL fake_teams(void *g, ZString *out) { ev(0x656, ptrv(g), ptrv(out)); return 0x5506; }
DWORD THISCALL fake_player_info(void *p, ZString *out, DWORD i) { ev(0x657, ptrv(p), ptrv(out), i); return 0x5507; }
DWORD __cdecl fake_delete_all() { ev(0x658); return 0x5508; }
void __cdecl fake_disconnect(DWORD id) { ev(0x659, id); }
DWORD g_letter;
DWORD THISCALL fake_random(void *rng, DWORD lo, DWORD hi)
{
    ev(0x65a, ptrv(rng), lo, hi);
    return lo + (g_letter++ % (hi - lo + 1));
}

struct Patches {
    ImportPatch imports[9];
    CodePatch code[12];
    SavedBytes globals, error, handler;
    BYTE cs_saved[3][24];
    bool cs_init[3];
    void on()
    {
        save_bytes(globals, 0x6bdc20, 0x18);
        save_bytes(error, DPLAY_ERROR_BUFFER, 0x40);
        save_bytes(handler, 0x6d00a8, 4);
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
        const DWORD iat[] = {0x5ea114, 0x5ea11c, 0x5ea098, 0x5ea094, 0x5ea07c};
        for (int i = 0; i < 5; i++)
            g_real[i] = *at<DWORD *>(iat[i]);
        patch_import(imports[0], 0x5ea340, (void *)&fake_co_create);
        patch_import(imports[1], 0x5ea344, (void *)&fake_co_init);
        patch_import(imports[2], 0x5ea33c, (void *)&fake_co_uninit);
        patch_import(imports[3], 0x5ea114, (void *)&fake_init_cs);
        patch_import(imports[4], 0x5ea11c, (void *)&fake_delete_cs);
        patch_import(imports[5], 0x5ea098, (void *)&fake_enter_cs);
        patch_import(imports[6], 0x5ea094, (void *)&fake_leave_cs);
        patch_import(imports[7], 0x5ea07c, (void *)&fake_sleep);
        patch_jump(code[0], 0x4d24c0, (void *)&fake_error_box);
        patch_jump(code[1], 0x51f310, (void *)&fake_gamespy0);
        patch_jump(code[2], 0x51f3e0, (void *)&fake_gamespy_stop);
        patch_jump(code[3], 0x51f420, (void *)&fake_gamespy_think);
        patch_jump(code[4], 0x51f180, (void *)&fake_basic_info);
        patch_jump(code[5], 0x51f1d0, (void *)&fake_rules);
        patch_jump(code[6], 0x51f220, (void *)&fake_teams);
        patch_jump(code[7], 0x51f880, (void *)&fake_player_info);
        patch_jump(code[8], 0x4e7b70, (void *)&fake_delete_all);
        patch_jump(code[9], 0x5c5750, (void *)&fake_disconnect);
        patch_jump(code[10], 0x55c4e0, (void *)&fake_random);
    }
    void off()
    {
        for (int i = 10; i >= 0; i--)
            unpatch(code[i]);
        for (int i = 7; i >= 0; i--)
            unpatch_import(imports[i]);
        const DWORD cs[3] = {DPLAY_LOCK, DPLAY_QUEUE_LOCK, DPLAY_TERMINATE_LOCK};
        for (int i = 0; i < 3; i++)
            if (cs_init[i]) {
                DeleteCriticalSection(at<void *>(cs[i]));
                memcpy(at<void *>(cs[i]), cs_saved[i], 24);
            }
        restore_bytes(handler);
        restore_bytes(error);
        restore_bytes(globals);
    }
};

// ---- The net game of the test -----------------------------------------------------------------------------------

DWORD g_session_vt[4];
BYTE *THISCALL session_delete(BYTE *s, DWORD flags)
{
    ev(0x660, ptrv(s), flags);
    return s;
}
constexpr int MAX_SESSIONS = 64;
BYTE *g_sessions[MAX_SESSIONS];
int g_nsessions;
ZString *g_strings[MAX_SESSIONS * 2 + 8];
int g_nstrings;
ZString *new_string(void *at_, const char *text)
{
    ZString *z = (ZString *)at_;
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, text);
    if (g_nstrings < (int)(sizeof g_strings / sizeof g_strings[0]))
        g_strings[g_nstrings++] = z;
    return z;
}
const char *const PLAYER_NAMES[] = {"Zax", "", "A very long player name", "Crater"};
BYTE *new_settings()
{
    BYTE *p = (BYTE *)arena(0x20);
    new_string(p, PLAYER_NAMES[rnd(4)]);
    *(DWORD *)(p + 4) = random();
    *(DWORD *)(p + 8) = random();
    p[0xc] = (BYTE)random();
    return p;
}
BYTE *new_session(DWORD dpnid)
{
    BYTE *s = (BYTE *)arena(0x40);
    g_session_vt[0] = ptrv((void *)&session_delete);
    *(DWORD *)s = ptrv(g_session_vt);
    s[5] = (BYTE)rnd(2);
    *(DWORD *)(s + 8) = dpnid;
    *(DWORD *)(s + 0xc) = rnd(100);
    *(BYTE **)(s + 0x1c) = new_settings();
    new_string(s + 0x24, rnd(2) ? "" : "abc");
    if (g_nsessions < MAX_SESSIONS)
        g_sessions[g_nsessions++] = s;
    return s;
}
DWORD THISCALL gt_new_player(Fake *f, DWORD flag)
{
    ev(0x661, f->id, flag);
    return ptrv(new_session(0));
}
DWORD THISCALL gt_terminated(Fake *f, const char *a, const char *b)
{
    ev(0x662, f->id, fnv_text(2166136261u, a), fnv_text(2166136261u, b));
    return 0;
}

Fake *new_type()
{
    Fake *t = new_fake(1);
    set3<2>(t);
    t->vt[5] = ptrv((void *)&gt_terminated);
    set0<6>(t);
    set0<7>(t);
    set1<9>(t);
    set1<10>(t);
    t->vt[20] = ptrv((void *)&gt_new_player);
    return t;
}

DPlayGame *make_game()
{
    g_nsessions = 0;
    DPlayGame *g = (DPlayGame *)arena(0x920);
    g->game.vftable = 0x5f15a8;
    g->game.game_type = new_type();
    g->game.settings = (NetGameSettings *)arena(0x40);
    new_string(g->game.settings, rnd(3) ? "Zax Server" : "");
    g->game.settings->max_players = rnd(17);
    const DWORD np = rnd(6);
    g->game.players = {0x5fb558, (void **)heap_alloc(32 * 4), np, 32};
    memset(g->game.players.data, 0, 32 * 4);
    for (DWORD i = 0; i < np; i++)
        g->game.players.data[i] = new_session(0x100 + i);
    g->game.teams = {0x5fb54c, nullptr, 0, 0};
    new_string(&g->game.state, "openplaying");
    g->running = (BYTE)rnd(2);
    g->hosting = (BYTE)rnd(2);
    g->queue = {0x5f1618, nullptr, 0, 0};
    if (rnd(2)) {
        g->queue.capacity = 4;
        g->queue.data = (DPlayMessage *)heap_alloc(4 * 20);
        g->queue.count = rnd(4);
        for (DWORD i = 0; i < g->queue.count; i++)
            g->queue.data[i] = {rnd(8), 0x100 + rnd(8), (void *)(uintptr_t)(0x9000 + i), rnd(64), 0x7000 + i};
    }
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
    g->terminated = (BYTE)(rnd(4) == 0);
    const char *texts[] = {"", "Kicked;You were kicked", "Host has quit", ";", "a;b;c"};
    lstrcpyA(g->terminate_text, texts[rnd(5)]);
    g_game = g;
    return g;
}
// A part of the state as one event of its own (its hash), so the report names the part that differs
template <typename F> void section(DWORD id, F f)
{
    const unsigned outer = g_statehash[g_side];
    g_statehash[g_side] = 2166136261u;
    f();
    const unsigned h = g_statehash[g_side];
    g_statehash[g_side] = outer;
    ev_raw(id, h);
}
void st_players(const DPlayGame *g);
void st_game(const DPlayGame *g, bool fresh = false)
{
    section(0x7a0, [g, fresh] {
        if (!fresh) {
            st_bytes(g->slots, 100 * 12);
            return;
        }
        // The factory clears each slot's flags and session but not its dpnid (+4), which keeps what the heap block
        // held before (it differed between the sides in 101 of 120 cases after the merge of 2026-10-28)
        for (const DPlaySlot &sl : g->slots) {
            st_bytes(&sl.added, 2);
            st(ptrv(sl.session));
        }
    });
    section(0x7a1, [g, fresh] {
        st(g->slots_changed);
        if (!fresh) {   // slot 100's dpnid behind the factory's 0x900 bytes; +0x34 and the text not set by it
            st(*(const DWORD *)((const BYTE *)g + 0x900));
            st(*(const DWORD *)((const BYTE *)g + 0x34));
            st_text(g->terminate_text);
        }
        st_bytes(&g->hosting, 3);
        st(g->game.unknownc);
        st(ptrv(g->game.settings));
    });
    section(0x7a2, [g] {
        st(g->queue.count);
        st(g->queue.capacity);
        if (g->queue.data)
            st_bytes(g->queue.data, g->queue.count * 20);
    });
    section(0x7a3, [g] { st_players(g); });
    section(0x7a4, [] {
        for (DWORD a = 0x6bdc20; a < 0x6bdc38; a += 4)
            st(*at<DWORD *>(a));
    });
}
void st_players(const DPlayGame *g)
{
    st(g->game.players.count);
    for (DWORD i = 0; i < g->game.players.count; i++) {
        const BYTE *s = (const BYTE *)g->game.players.data[i];
        st(ptrv(s));
        section(0x7b0 + i % 8, [s] { st_dwords(s + 4, 0x18); });
        const BYTE *ps = *(BYTE *const *)(s + 0x1c);
        section(0x7c0 + i % 8, [ps] {
            st_zstring((const ZString *)ps);
            st_dwords(ps + 4, 8);
            st(ps[0xc]);
        });
        section(0x7d0 + i % 8, [s] { st_zstring((const ZString *)(s + 0x24)); });
    }
}
void free_strings()
{
    for (int i = 0; i < g_nstrings; i++)
        at<void(THISCALL *)(ZString *)>(0x4defd0)(g_strings[i]);
    g_nstrings = 0;
}
void free_game(DPlayGame *g)
{
    if (g->game.players.data)
        heap_free(g->game.players.data);
    if (g->queue.data)
        heap_free(g->queue.data);
    free_strings();
}

void setup_com()
{
    init_vtables();
    for (int i = 0; i < 4; i++) {
        g_addresses[i] = new_com(0x80 + i, g_address_vt);
        g_local[i] = new_com(0x90 + i, g_address_vt);
    }
    g_nlocal = rnd(5);
    g_ip = new_com(0x60, g_ip_vt);
    g_client_address = rnd(5) ? new_com(0x61, g_address_vt) : nullptr;
    g_new_server = new_com(0x51, g_server_vt);
    g_new_client = new_com(0x52, g_client_vt);
    g_next_address = 0;
    g_letter = 0;   // GameRandom's letters (ProcessSlots), the same on both sides
    g_create_fail = rnd(3) ? 0 : 8;
    g_hosts = rnd(4);
    g_providers = rnd(5);
}
// The globals the functions use without a test: a fresh fake where there is none
void fill_globals()
{
    if (!*at<void **>(DPLAY_SERVER))
        *at<Com **>(DPLAY_SERVER) = new_com(0x51, g_server_vt);
    if (!*at<void **>(DPLAY_CLIENT))
        *at<Com **>(DPLAY_CLIENT) = new_com(0x52, g_client_vt);
    if (!*at<void **>(DPLAY_ADDRESS))
        *at<Com **>(DPLAY_ADDRESS) = new_com(0x53, g_address_vt);
}
void set_globals(bool server, bool client, bool address)
{
    *at<Com **>(DPLAY_SERVER) = server ? new_com(0x51, g_server_vt) : nullptr;
    *at<Com **>(DPLAY_CLIENT) = client ? new_com(0x52, g_client_vt) : nullptr;
    *at<Com **>(DPLAY_ADDRESS) = address ? new_com(0x53, g_address_vt) : nullptr;
}
void st_host_list(void *head)
{
    for (BYTE *h = (BYTE *)head; h; h = *(BYTE **)(h + 8)) {
        st(*(DWORD *)h);
        st_text(*(const char **)(h + 4));
        st(*(DWORD *)(h + 0xc));
        st(*(DWORD *)(h + 0x10));
    }
    if (head)
        vcall<void *>(head, 0, 1u);
}

template <typename R = DWORD, typename... A> R gcall(DWORD fn, void *g, A... a) { return at<R(THISCALL *)(void *, A...)>(fn)(g, a...); }

// ---- The cases ----------------------------------------------------------------------------------------------------

void construction_case(int k)
{
    Patches pt;
    pt.on();
    setup_com();
    if (k % 4 == 0) {
        const DWORD r = at<DWORD(__cdecl *)()>(0x47eb00)();
        ev(0x670, r, *at<DWORD *>(0x6d00a8));
    }
    // The factory on a fake game type, then the object deleted by 0x47ef10
    g_nsessions = 0;
    Fake *type = new_type();
    auto *g = gcall<DPlayGame *>(0x47eb10, type, random());
    ev(0x671, g != nullptr, type->id == ptrv(g));
    if (g) {
        g_game = g;
        st_game(g, true);
        st(g->game.vftable);
        st(g->queue.vftable);
        st(g->game.players.vftable);
        st(g->game.teams.vftable);
        st(g->game.teams.count);
        st_zstring(&g->game.state);
        // Messages through the callback (DirectPlay's threads call it so)
        for (int i = 0, n = (int)rnd(4); i < n; i++) {
            DWORD *msg = (DWORD *)arena(0x40);
            // no create player: its slot 100 lies behind the factory's 0x900 bytes (the original writes there too)
            const DWORD ids[] = {0xffff0009, 0xffff000e, 0xffff0011, 0xffff0016, 0xffff0003};
            const DWORD id = ids[rnd(5)];
            msg[0] = 0x20;
            msg[1] = 0x100 + rnd(8);
            msg[2] = id == 0xffff0016 ? ptrv("Host has quit") : rnd(8);
            msg[3] = 0x9000 + rnd(100);
            msg[4] = rnd(64);
            msg[5] = 0x8000 + rnd(100);
            const DWORD r = at<DWORD(STDCALL *)(void *, DWORD, DWORD *)>(0x47ead0)(g, id, msg);
            ev(0x672, id, r);
            st_dwords(msg, 0x20);
        }
        st_game(g, true);
        set_globals(rnd(2), rnd(2), rnd(2));
        ev(0x673, gcall(0x47ef10, g, 1u) == ptrv(g));
    }
    free_strings();
    pt.off();
}

void session_case(int k)
{
    Patches pt;
    pt.on();
    setup_com();
    DPlayGame *g = make_game();
    set_globals(rnd(3), rnd(3), rnd(3));
    for (int step = 0, steps = 1 + (int)rnd(4); step < steps; step++) {
        DWORD r = 0;
        const DWORD op = rnd(10);
        // Host and Join make the DirectPlay objects themselves (on the first step sometimes from none)
        const bool make = step == 0 && (op == 1 || op == 2) && rnd(2);
        if (!make)
            fill_globals();
        else if (op == 2)
            *at<void **>(DPLAY_CLIENT) = nullptr;
        g_fail_now = op == 4 || (op == 2 && !make);
        switch (op) {
        case 0: r = gcall(0x47efd0, g); break;
        case 1: r = gcall(0x47f0b0, g, ptrv(new_settings()), ptrv(g->game.settings)); break;
        case 2: {
            const char *hosts[] = {"10.0.0.5", "zax.example.com", "", "x-directplay:/provider=%7BEBFE7BA0%7D;hostname=1.2.3.4"};
            BYTE *item = (BYTE *)arena(0x20);
            *(Com **)(item + 0xc) = new_com(0x54, g_address_vt);
            *(Com **)(item + 0x10) = rnd(4) ? new_com(0x55, g_address_vt) : nullptr;
            r = gcall(0x47f5d0, g, rnd(3) ? ptrv(hosts[rnd(4)]) : 0u, ptrv(item), ptrv(new_settings()));
            break;
        }
        case 3:
            if (!g->hosting && rnd(2))
                g->hosting = 1;
            r = gcall(0x480530, g);
            break;
        case 4: r = gcall(0x480690, g); break;
        case 5: {
            BYTE *item = (BYTE *)arena(0x20);
            BYTE *guid = (BYTE *)arena(16);
            for (int i = 0; i < 16; i++)
                guid[i] = (BYTE)random();
            *(BYTE **)(item + 0xc) = guid;
            r = gcall(0x480350, g, rnd(2) ? ptrv(item) : 0u);
            break;
        }
        case 6: {
            BYTE *item = (BYTE *)arena(0x20);
            *(BYTE **)(item + 0xc) = (BYTE *)arena(16);
            r = gcall(0x47ff30, g, rnd(2) ? ptrv(item) : 0u, rnd(2) ? random() | 1 : random() & ~0xffu);
            break;
        }
        case 7:
            // al; the upper bytes are the provider list's deleting destructor's (the list's heap address, which may
            // be a block freed before on one side only) or the error box's
            r = gcall(0x4803f0, g, ptrv(NAMES[rnd(5)]));
            r = (r & 0xff) | ((r >> 8) == 0x112233 ? 0x100 : r > 0xff ? 0x200 : 0);
            break;
        default: {
            void **head = rnd(2) ? (void **)arena(8) : nullptr;
            r = gcall(0x480480, g, ptrv(head));
            // the list by its contents: a host item may get a block freed before on one side only
            ev(0x680, head ? *head == (void *)(uintptr_t)r : 0xdead);
            st_host_list((void *)(uintptr_t)r);
            r = r != 0;
            break;
        }
        }
        g_fail_now = false;
        ev(0x681, op, r);
        st_game(g);
    }
    free_game(g);
    (void)k;
    pt.off();
}

void frame_case(int k)
{
    Patches pt;
    pt.on();
    setup_com();
    DPlayGame *g = make_game();
    set_globals(true, rnd(2), true);
    for (int step = 0, steps = 1 + (int)rnd(5); step < steps; step++) {
        fill_globals();
        DWORD r = 0;
        const DWORD op = rnd(9);
        const DWORD count = g->game.players.count;
        switch (op) {
        case 0:
        case 1:
            if (rnd(3) == 0) {
                g->terminated = 1;
                const char *texts[] = {"", "Kicked;You were kicked", "Host has quit", ";", "a;b;c"};
                lstrcpyA(g->terminate_text, texts[rnd(5)]);
            }
            gcall(0x480bd0, g);
            break;
        case 2:
        case 3: {
            NetPacket *p = (NetPacket *)arena(sizeof(NetPacket));
            packet_ctor(p);
            BYTE data[64];
            const DWORD n = rnd(4) ? 1 + rnd(64) : 0;
            for (DWORD i = 0; i < n; i++)
                data[i] = (BYTE)random();
            if (n)
                p->WriteBytes(data, n);
            p->reliable = (BYTE)rnd(2);
            const DWORD player = count && rnd(3) ? rnd(count) : 0;
            r = gcall(op == 2 ? 0x4806f0 : 0x4807e0, g, player, ptrv(p));
            st_packet(p);
            packet_dtor(p);
            break;
        }
        case 4:
            if (!count)
                break;
            r = gcall(0x47fa30, g, rnd(count), ptrv(PLAYER_NAMES[rnd(4)]), ptrv(rnd(2) ? "You were kicked" : ""));
            r = 0;   // the ZString destructor's eax
            break;
        case 5: {
            BYTE *s = count ? (BYTE *)g->game.players.data[rnd(count)] : new_session(0x200);
            r = gcall(0x47fac0, g, ptrv(s));
            break;
        }
        case 6: {
            BYTE *s = count ? (BYTE *)g->game.players.data[rnd(count)] : new_session(0x201);
            if (rnd(2))
                *at<void **>(DPLAY_CLIENT) = nullptr;   // the server's connection info
            r = gcall(0x47fba0, g, ptrv(s));
            break;
        }
        default: {
            DWORD *msg = (DWORD *)arena(0x40);
            const DWORD ids[] = {0xffff0007, 0xffff0009, 0xffff0011, 0xffff0014, 0xffff0016};
            const DWORD id = ids[rnd(5)];
            msg[0] = 0x20;
            msg[1] = 0x100 + rnd(8);
            msg[2] = id == 0xffff0016 ? ptrv("Server closed;bye") : rnd(8);
            msg[3] = id == 0xffff0014 ? (rnd(2) ? 0x80158000u + rnd(0x700) : 0) : 0x9000 + rnd(100);
            msg[4] = rnd(64);
            msg[5] = 0x8000 + rnd(100);
            r = at<DWORD(STDCALL *)(void *, DWORD, DWORD *)>(0x47ead0)(g, id, msg);
            break;
        }
        }
        ev(0x690, op, r);
        st_game(g);
    }
    free_game(g);
    (void)k;
    pt.off();
}

void gamespy_case(int k)
{
    Patches pt;
    pt.on();
    setup_com();
    DPlayGame *g = make_game();
    set_globals(true, rnd(2), true);
    ZString out;
    new_string(&out, rnd(2) ? "\\gamename\\zax" : "");
    DWORD r;
    switch (k % 3) {
    case 0:
        r = gcall(0x47ed00, g, &out);
        r = 0;   // the ZString destructor's eax
        break;
    case 1: r = gcall(0x47eec0, g, &out); break;
    default: r = gcall(0x47eed0, g, &out); break;
    }
    ev(0x6a0, r);
    st_zstring(&out);
    st_game(g);
    free_game(g);
    pt.off();
}

}  // namespace

ZAX_CHECKS(dplay_rest5a)
{
    (void)cls;
    (void)n;
    g_table = hooks_dplay_rest5a;
    g_tid = tid();
    run_cases("dplay rest: factory, static initializer, callback", scaled(120), 0x47eb10u, [](int k) { construction_case(k); });
    run_cases("dplay rest: reset, host, join, close, providers, enum hosts", scaled(400), 0x47f0b0u, [](int k) { session_case(k); });
    run_cases("dplay rest: frame, send, kick, address, ping", scaled(400), 0x480bd0u, [](int k) { frame_case(k); });
    run_cases("dplay rest: GameSpy answers", scaled(90), 0x47ed00u, [](int k) { gamespy_case(k); });
}

}  // namespace zax
