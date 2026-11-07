// The rest of the DirectPlay 8 net game (phase 5a, group G3; docs/DECOMPILATION.md 10, "Group G3" and the
// batch P5a-2): the factory with its static initializer, the message callback DirectPlay calls on its own
// threads, and the slots of the net game's vtable 0x5f15a8 that dplay.cpp does not have. Layouts and the
// message handler: dplay.h. Every COM call goes through the same vtable slot and every Win32 call through
// the same import entry as in the original (read when called), so recording stand-ins in the import table
// and fake COM objects see the original's call sequence.
//
// COM slots [confirmed: slot numbers and arguments from the code; the method names after the DirectPlay 8
// headers: assumption]:
//   server (IDirectPlay8Server, 0x6bdc2c): 2 Release, 8 SetServerInfo, 9 GetClientInfo, 10 GetClientAddress,
//     11 GetLocalHostAddresses, 13 Host, 14 SendTo, 23 Close, 24 DestroyClient, 32 GetConnectionInfo
//   client (IDirectPlay8Client, 0x6bdc30): 2 Release, 5 EnumHosts, 7 Connect, 8 Send, 11 SetClientInfo,
//     14 Close, 20 GetConnectionInfo
//   address (IDirectPlay8Address, 0x6bdc34 and a temporary one): 0 QueryInterface (IID 0x6061b0, the IP
//     interface: 6 GetSockAddress), 2 Release, 4 BuildFromURLA, 10 GetURLA, 13 SetSP, 18 AddComponent
//   The application GUID is the 16 bytes at 0x5f1598, the TCP/IP provider's CLSID 0x606160, the port the
//   dword 0x611408 (26784), the component names L"hostname" 0x611ed4 and L"port" 0x611ec8.
//
// The vtable slots (names ours, from what they do) [confirmed: code]:
//   1  0x47efd0 ResetPlayers: the players deleted, the slots cleared under g_DPlayLock, a new local player
//   2  0x47f0b0 Host(player settings, settings): CreateDirectPlay(server), hosting and running set, the
//      players and slots reset, the local player named after the player settings (SetServerInfo, sync),
//      Host with the application description (client/server, max 16 players, the session name, the
//      settings' name as reserved data) on the address object, then GameSpy_Start; the local session or 0
//   3  0x47f5d0 Join(host name, host item, player settings): CreateDirectPlay(client) when there is no
//      client, hosting set (sic) and running cleared, the players and slots reset, SetClientInfo, then
//      Connect: to a host name through a new address object (BuildFromURLA; when that says
//      DPNERR_INVALIDURL 0x80158450, SetSP(TCP/IP), "hostname" and "port" added) or to the host item of an
//      enumeration (+0xc the host address, +0x10 the device address); +0x34 cleared, slot 7 of the game type
//   6  0x480350 SetProvider(provider item or null): SetSP of the address object (null: CreateDirectPlay(1),
//      TCP/IP and the port); al
//   7  0x47ff30 CreateDirectPlay(second argument), then slot 6
//   8  0x4803f0 SetProviderByName(name): the provider list (slot 4) searched by name (0x51eb10), slot 6 on
//      the item, the list deleted; a missing provider shows "Could not find an "%s" service Provider" and
//      leaks the list; no list at all gives 1
//   9  0x480480 EnumHosts(list head or null): EnumHosts sync on the address object with the address of the
//      argument as user context (the message handler puts every answering host in front of the list)
//   10 0x480530 Close: Close of the server (and GameSpy_Stop while running) or the client, the players and
//      slots reset, running set, the static part array emptied (0x4e7b70), slot 6 of the game type, the
//      three DirectPlay objects released
//   11 0x480690 IsAvailable: a client made through CoCreateInstance and released at once; al
//   14 0x4807e0 slot 15, then NetPacket_Reset
//   15 0x4806f0 Send(player index, packet): index 0 the client's Send, else SendTo the player's dpnid with
//      its slot index as context; guaranteed (flag 8, no timeout) when the packet is reliable, else a time
//      out of 100 ms; then the packet skipped to its end
//   16 0x480bd0 Frame: Sleep(0), the slots (host only) and GameSpy_Think, a terminated session closed
//      (slot 10) and reported to slot 5 of the game type ("reason;text" split at the first ';'), else the
//      received messages processed
//   20 0x47fa30 Kick(index, a, b): DestroyClient with "a;b" as data
//   21 0x47fac0 PlayerAddress(player): the IPv4 address of a client (GetClientAddress, the IP interface,
//      GetSockAddress), 0 on any error
//   22 0x47fba0 Ping(player): GetConnectionInfo's round trip time (client, or the server for the player);
//      0 for the local player (+5)
//   24 0x47ed00 GameSpyBasic: "hostname", "DirectPlayAddressCount" and "DirectPlayAddress_%d" (the URLs of
//      the server's local addresses), then GameSpy_BasicInfo
//   26 0x47eec0, 27 0x47eed0: GameSpy_Rules; the players' info and the teams
// Not hookable (4 bytes): slot 13 0x47ecf0 (al = +0x38 hosting) and slot 12 0x4806e0 (al = +0x39 running).
//
// The factory 0x47eb10 (thiscall on the game type, one unused argument) is what the static initializer
// 0x47eb00 registers through 0x47eaf0 (NetHandler_Set 0x51eca0, the global 0x6d00a8): a 0x900-byte object
// (the slot array runs past it: dplay.h), CoInitialize, the three critical sections initialised, the queue
// reserved for 100 messages, the slots and players reset, a local player. The callback 0x47ead0 (stdcall:
// context, message id, message) is what Initialize gets; it runs on DirectPlay's threads and only calls the
// message handler 0x47ff50, which takes the locks itself.
//
// Locking kept as in the original: g_DPlayLock 0x6bdbf0 around every reset of the slots (the factory clears
// +0x8fc once before taking it), the terminate lock 0x6bdad0 around the terminate check of the frame; the
// players array itself is not locked.
#include "dplay.h"
#include "exe_base.h"
#include "small_helpers.h"

namespace zax {

namespace {

constexpr DWORD GAME_VTABLE = 0x5f15a8, QUEUE_VTABLE = 0x5f1618;
constexpr DWORD IAT_SLEEP = 0x5ea07c, IAT_INIT_CS = 0x5ea114, IAT_CO_INIT = 0x5ea344, IAT_MB2W = 0x5ea0a8;
constexpr DWORD CLSID_CLIENT = 0x6061a0, IID_CLIENT = 0x606180, CLSID_ADDRESS = 0x6061d0, IID_ADDRESS = 0x6061c0;
constexpr DWORD IID_ADDRESS_IP = 0x6061b0, CLSID_TCPIP = 0x606160, APP_GUID = 0x5f1598;
constexpr DWORD PORT = 0x611408, W_HOSTNAME = 0x611ed4, W_PORT = 0x611ec8, EMPTY = 0x62e558;
constexpr DWORD DPNERR_INVALIDURL = 0x80158450, REGDB_E_CLASSNOTREG = 0x80040154, DPN_SYNC = 0x80000000;

template <typename T = DWORD> T &f(void *o, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(o) + off); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

// COM method `slot` of an interface (stdcall, this first)
template <typename R, typename... A> R com(void *obj, int slot, A... a)
{
    auto fn = (*reinterpret_cast<R(STDCALL *const *const *)(void *, A...)>(obj))[slot];
    return fn(obj, a...);
}
void *global(DWORD a) { return *at<void **>(a); }

void enter(DWORD cs) { (*imp_slot<void(STDCALL **)(void *)>(IAT_ENTER_CS))(at<void *>(cs)); }
void leave(DWORD cs) { (*imp_slot<void(STDCALL **)(void *)>(IAT_LEAVE_CS))(at<void *>(cs)); }
// MultiByteToWideChar(CP_ACP, 0, s, -1, out, n)
int mb2w(const char *s, WORD *out, int n)
{
    return (*imp_slot<int(STDCALL **)(DWORD, DWORD, const char *, int, WORD *, int)>(IAT_MB2W))(0, 0, s, -1, out, n);
}
DWORD strlen_(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
DWORD error_text(DWORD hr) { return pv(zax::fn<const char *(__fastcall *)(DWORD)>(0x47f350)(hr)); }
DWORD create_dplay(void *g, DWORD server) { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x47fdf0)(g, server); }

const char *c_str(const void *z) { return zax::fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
DWORD zs_length(const void *z) { return zax::fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(z); }
void zs_ctor(void *z) { zax::fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
DWORD zs_dtor(void *z) { return zax::fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }

// A player removed from the end of the array (the inlined PtrArray_RemoveAt of slots 1, 3 and 10)
void delete_players_inline(DPlayGame *g)
{
    ZPtrArray &a = g->game.players;
    while (a.count) {
        const DWORD i = a.count - 1;
        if (void *p = a.data[i])
            vcall<void *>(p, 0, 1u);
        const DWORD c = a.count;
        if (i < c) {
            if (i + 1 < c)
                zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(&a.data[i], &a.data[i + 1], (c - (i + 1)) * 4);
            a.count--;
        }
    }
}
// The same through PtrArray_RemoveAt 0x586fc0 (the factory and Host)
void delete_players_call(DPlayGame *g)
{
    ZPtrArray &a = g->game.players;
    while (a.count) {
        const DWORD i = a.count - 1;
        if (void *p = a.data[i])
            vcall<void *>(p, 0, 1u);
        zax::fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x586fc0)(&a, i);
    }
}
// The slots' added, removed and session cleared under g_DPlayLock, then "slots changed"
void clear_slots_locked(DPlayGame *g)
{
    enter(glob_addr(DPLAY_LOCK));
    for (DPlaySlot &s : g->slots) {
        s.added = 0;
        s.removed = 0;
        s.session = nullptr;
    }
    g->slots_changed = 0;
    leave(glob_addr(DPLAY_LOCK));
}
// The local player: slot 20 of the game type, appended, dpnid -1, slot 0, local, not ready
BYTE *new_local_player(DPlayGame *g)
{
    BYTE *s = vcall<BYTE *>(g->game.game_type, 20, 1u);
    vcall<DWORD>(&g->game.players, 2, &s);
    f(s, 8) = 0xffffffff;
    f(s, 0xc) = 0;
    s[5] = 1;
    s[6] = 0;
    return s;
}
// The local player's settings copied from the caller's: the name (ZString assignment), two dwords, a byte
void copy_player_settings(BYTE *session, BYTE *from)
{
    BYTE *to = f<BYTE *>(session, 0x1c);
    zax::fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(to, from);
    f(to, 4) = f(from, 4);
    f(to, 8) = f(from, 8);
    to[0xc] = from[0xc];
}
// A name as wide text for DirectPlay: n - 1 characters (the original passes the length without the
// terminator, so MultiByteToWideChar does not write the terminator itself), the terminator after them
void wide_name(const char *s, WORD *out)
{
    const DWORD n = strlen_(s) + 1;
    mb2w(s, out, (int)(n - 1));
    out[n - 1] = 0;
}
// The application description: size 0x48, the GUID at +0x18
void app_desc(DWORD *desc)
{
    for (int i = 0; i < 0x12; i++)
        desc[i] = 0;
    desc[0] = 0x48;
    desc[6] = glob<DWORD *>(APP_GUID)[0];
    desc[7] = glob<DWORD *>(APP_GUID)[1];
    desc[8] = glob<DWORD *>(APP_GUID)[2];
    desc[9] = glob<DWORD *>(APP_GUID)[3];
}

}  // namespace

// ---- The factory, its static initializer, the callback ------------------------------------------------------

DPlayGame *THISCALL DPlayGame_New(void *game_type, DWORD)
{
    auto *g = (DPlayGame *)heap_alloc(0x900);
    if (!g)
        return nullptr;
    zax::fn<void *(THISCALL *)(void *, void *)>(0x51ecb0)(g, game_type);
    g->queue.vftable = vt(QUEUE_VTABLE);
    g->queue.data = nullptr;
    g->queue.count = 0;
    g->queue.capacity = 0;
    g->game.vftable = vt(GAME_VTABLE);
    (*imp_slot<DWORD(STDCALL **)(void *)>(IAT_CO_INIT))(nullptr);
    auto init = *imp_slot<void(STDCALL **)(void *)>(IAT_INIT_CS);
    init(glob<void *>(DPLAY_TERMINATE_LOCK));
    init(glob<void *>(DPLAY_QUEUE_LOCK));
    init(glob<void *>(DPLAY_LOCK));
    g->hosting = 0;
    g->running = 1;
    g->terminated = 0;
    if (g->queue.capacity < 100) {
        g->queue.capacity = 100;
        auto *d = (DPlayMessage *)heap_alloc(0x7d0);
        if (g->queue.count)
            for (DWORD i = 0; i < g->queue.count; i++)
                d[i] = g->queue.data[i];
        if (g->queue.data)
            heap_free(g->queue.data);
        g->queue.data = d;
    }
    g->slots_changed = 0;   // also before the lock here
    clear_slots_locked(g);
    delete_players_call(g);
    clear_slots_locked(g);
    new_local_player(g);
    return g;
}

DWORD __cdecl StaticInit_DPlayGame() { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x55d050)(glob<void *>(0x6bdbe8), fn_addr(0x47eaf0)); }

DWORD STDCALL DPlay_MessageHandlerCallback(DPlayGame *context, DWORD id, BYTE *msg)
{
    return zax::fn<DWORD(THISCALL *)(DPlayGame *, DWORD, BYTE *)>(0x47ff50)(context, id, msg);
}

namespace {

struct DPlayGameRest : DPlayGame {
    BYTE *THISCALL ResetPlayers();                                         // slot 1
    BYTE *THISCALL Host(BYTE *player, void *settings);                     // slot 2
    BYTE *THISCALL Join(const char *host, BYTE *item, BYTE *player);       // slot 3
    DWORD THISCALL SetProvider(BYTE *provider);                            // slot 6
    DWORD THISCALL CreateAndSetProvider(BYTE *provider, DWORD server);     // slot 7
    DWORD THISCALL SetProviderByName(const char *name);                    // slot 8
    void *THISCALL EnumHosts(void **head);                                 // slot 9
    DWORD THISCALL Close();                                                // slot 10
    DWORD THISCALL IsAvailable();                                          // slot 11
    DWORD THISCALL SendAndReset(DWORD player, NetPacket *p);               // slot 14
    DWORD THISCALL Send(DWORD player, NetPacket *p);                       // slot 15
    void THISCALL Frame();                                                 // slot 16
    DWORD THISCALL Kick(DWORD index, const char *a, const char *b);        // slot 20
    DWORD THISCALL PlayerAddress(BYTE *player);                            // slot 21
    DWORD THISCALL Ping(BYTE *player);                                     // slot 22
    DWORD THISCALL GameSpyBasic(ZString *out);                             // slot 24
    DWORD THISCALL GameSpyRules(ZString *out);                             // slot 26
    DWORD THISCALL GameSpyPlayers(ZString *out);                           // slot 27
};

}  // namespace

// ---- Players and sessions ------------------------------------------------------------------------------------

BYTE *THISCALL DPlayGameRest::ResetPlayers()
{
    delete_players_inline(this);
    clear_slots_locked(this);
    return new_local_player(this);
}

BYTE *THISCALL DPlayGameRest::Host(BYTE *player, void *settings)
{
    game.settings = (NetGameSettings *)settings;
    game.unknownc = pv(player);
    create_dplay(this, 1);
    hosting = 1;
    running = 1;
    delete_players_call(this);
    clear_slots_locked(this);
    while (game.players.count)
        zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x480cc0)(&game.players, game.players.count - 1);
    zax::fn<DWORD(THISCALL *)(void *)>(0x480c80)(this);
    BYTE *session = new_local_player(this);
    copy_player_settings(session, player);
    WORD wname[0x104];
    if (const char *name = c_str(player))
        wide_name(name, wname);
    DWORD info[6] = {0x18, 1, pv(wname), 0, 0, 0};
    DWORD hr = com<DWORD>(global(glob_addr(DPLAY_SERVER)), 8, info, 0u, 0u, DPN_SYNC);
    if ((int)hr < 0) {
        error_text(hr);
        return nullptr;
    }
    WORD wsession[0x104];
    if (const char *name = c_str(game.settings)) {
        mb2w(name, wsession, 0x103);
        wsession[0x103] = 0;
    }
    DWORD desc[0x12];
    app_desc(desc);
    desc[1] = 1;      // DPNSESSION_CLIENT_SERVER
    desc[10] = 0x10;  // max players
    desc[12] = pv(wsession);
    desc[16] = pv(c_str(game.settings));
    desc[17] = zs_length(game.settings) + 1;
    hr = com<DWORD>(global(glob_addr(DPLAY_SERVER)), 13, desc, glob<void *>(DPLAY_ADDRESS), 1u, 0u, 0u, 0u, 0u);
    if ((int)hr < 0) {
        error_text(hr);
        return nullptr;
    }
    zax::fn<void(THISCALL *)(void *)>(0x51f310)(this);   // GameSpy_Start
    return session;
}

BYTE *THISCALL DPlayGameRest::Join(const char *host, BYTE *item, BYTE *player)
{
    game.unknownc = pv(player);
    if (!global(glob_addr(DPLAY_CLIENT)))
        create_dplay(this, 0);
    hosting = 1;
    running = 0;
    delete_players_inline(this);
    clear_slots_locked(this);
    while (game.players.count)
        zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x480cc0)(&game.players, game.players.count - 1);
    clear_slots_locked(this);
    BYTE *session = new_local_player(this);
    copy_player_settings(session, player);
    WORD wname[0x104];
    if (const char *name = c_str(player))
        wide_name(name, wname);
    DWORD info[6] = {0x18, 1, pv(wname), 0, 0, 0};
    DWORD hr = com<DWORD>(global(glob_addr(DPLAY_CLIENT)), 11, info, 0u, 0u, DPN_SYNC);
    if ((int)hr < 0) {
        error_text(hr);
        return nullptr;
    }
    DWORD desc[0x12];
    app_desc(desc);
    if (host) {
        void *addr = nullptr;
        hr = (*imp_slot<DWORD(STDCALL **)(DWORD, DWORD, DWORD, DWORD, void **)>(IAT_CO_CREATE))(glob_addr(CLSID_ADDRESS), 0, 7, glob_addr(IID_ADDRESS), &addr);
        if ((int)hr < 0) {
            error_text(hr);
            return nullptr;
        }
        hr = com<DWORD>(addr, 4, host);
        if (hr == DPNERR_INVALIDURL) {
            hr = com<DWORD>(addr, 13, glob_addr(CLSID_TCPIP));
            if ((int)hr < 0) {
                error_text(hr);
                return nullptr;
            }
            WORD whost[0x100];
            wide_name(host, whost);
            const DWORD size = zax::fn<DWORD(__cdecl *)(const WORD *)>(0x5d1789)(whost) * 2 + 2;
            hr = com<DWORD>(addr, 18, glob_addr(W_HOSTNAME), whost, size, 1u);
            if ((int)hr < 0) {
                error_text(hr);
                return nullptr;
            }
            hr = com<DWORD>(addr, 18, glob_addr(W_PORT), glob_addr(PORT), 4u, 2u);
        }
        if ((int)hr < 0) {
            error_text(hr);
            return nullptr;
        }
        hr = com<DWORD>(global(glob_addr(DPLAY_ADDRESS)), 13, glob_addr(CLSID_TCPIP));
        if ((int)hr < 0) {
            error_text(hr);
            return nullptr;
        }
        hr = com<DWORD>(global(glob_addr(DPLAY_CLIENT)), 7, desc, addr, global(glob_addr(DPLAY_ADDRESS)), 0u, 0u, 0u, 0u, 0u, 0u, DPN_SYNC);
        com<DWORD>(addr, 2);
        if ((int)hr < 0) {
            error_text(hr);
            return nullptr;
        }
    } else {
        hr = com<DWORD>(global(glob_addr(DPLAY_CLIENT)), 7, desc, f<void *>(item, 0xc), f<void *>(item, 0x10), 0u, 0u, 0u, 0u, 0u, 0u, DPN_SYNC);
        if ((int)hr < 0) {
            error_text(hr);
            return nullptr;
        }
    }
    f(this, 0x34) = 0;
    vcall<void>(game.game_type, 7);
    return session;
}

DWORD THISCALL DPlayGameRest::Close()
{
    if (hosting) {
        if (running) {
            zax::fn<DWORD(THISCALL *)(void *)>(0x51f3e0)(this);   // GameSpy_Stop
            com<DWORD>(global(glob_addr(DPLAY_SERVER)), 23, 0u);
        } else {
            com<DWORD>(global(glob_addr(DPLAY_CLIENT)), 14, 0u);
        }
        hosting = 0;
    }
    delete_players_inline(this);
    clear_slots_locked(this);
    new_local_player(this);
    running = 1;
    zax::fn<void(__cdecl *)()>(0x4e7b70)();   // StaticPartArray_DeleteAll
    if (void *type = game.game_type)
        vcall<void>(type, 6);
    DWORD r = 0;
    const DWORD objects[] = {DPLAY_SERVER, DPLAY_CLIENT, DPLAY_ADDRESS};
    for (DWORD o : objects) {
        r = pv(global(o));
        if (r) {
            r = com<DWORD>(global(o), 2);
            *at<void **>(o) = nullptr;
        }
    }
    return r;
}

// ---- Service providers and hosts -------------------------------------------------------------------------------

DWORD THISCALL DPlayGameRest::SetProvider(BYTE *provider)
{
    DWORD hr;
    if (provider) {
        hr = com<DWORD>(global(glob_addr(DPLAY_ADDRESS)), 13, f<void *>(provider, 0xc));
    } else {
        create_dplay(this, 1);
        hr = com<DWORD>(global(glob_addr(DPLAY_ADDRESS)), 13, glob_addr(CLSID_TCPIP));
        if ((int)hr < 0)
            return error_text(hr) & ~0xffu;
        hr = com<DWORD>(global(glob_addr(DPLAY_ADDRESS)), 18, glob_addr(W_PORT), glob_addr(PORT), 4u, 2u);
    }
    if ((int)hr < 0)
        return error_text(hr) & ~0xffu;
    return (hr & ~0xffu) | 1;
}

DWORD THISCALL DPlayGameRest::CreateAndSetProvider(BYTE *provider, DWORD server)
{
    create_dplay(this, server);
    return vcall<DWORD>(this, 6, provider);
}

DWORD THISCALL DPlayGameRest::SetProviderByName(const char *name)
{
    BYTE ok = 1;
    void *list = vcall<void *>(this, 4);
    DWORD r = 0;
    if (list) {
        void *found = zax::fn<void *(THISCALL *)(void *, const char *)>(0x51eb10)(list, name);
        if (!found) {
            char text[0xc8];
            zax::fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x611f20), name);
            return zax::fn<DWORD(__fastcall *)(DWORD, const char *)>(0x4d24c0)(glob_addr(0x6114ac) /* "Multi-player Netword Error" */, text) & ~0xffu;
        }
        if (!(vcall<DWORD>(this, 6, found) & 0xff))
            ok = 0;
        r = pv(vcall<void *>(list, 0, 1u));
    }
    return (r & ~0xffu) | ok;
}

void *THISCALL DPlayGameRest::EnumHosts(void **head)
{
    DWORD local = 0;
    DWORD desc[0x12];
    app_desc(desc);
    if (!head)
        head = (void **)&local;
    void **context = head;   // the argument's slot: the message handler reads the head through it
    const DWORD hr = com<DWORD>(global(glob_addr(DPLAY_CLIENT)), 5, desc, 0u, global(glob_addr(DPLAY_ADDRESS)), 0u, 0u, 0u, 0u, 0u, &context, 0u, DPN_SYNC);
    if ((int)hr < 0) {
        error_text(hr);
        return nullptr;
    }
    return *context;
}

DWORD THISCALL DPlayGameRest::IsAvailable()
{
    void *obj = nullptr;
    BYTE ok = 1;
    const DWORD hr = (*imp_slot<DWORD(STDCALL **)(DWORD, DWORD, DWORD, DWORD, void **)>(IAT_CO_CREATE))(glob_addr(CLSID_CLIENT), 0, 1, glob_addr(IID_CLIENT), &obj);
    if (hr == REGDB_E_CLASSNOTREG) {
        ok = 0;
    } else if ((int)hr < 0) {
        error_text(hr);
        ok = 0;
    }
    DWORD r = pv(obj);
    if (obj)
        r = com<DWORD>(obj, 2);
    return (r & ~0xffu) | ok;
}

// ---- Sending ---------------------------------------------------------------------------------------------------

DWORD THISCALL DPlayGameRest::SendAndReset(DWORD player, NetPacket *p)
{
    vcall<DWORD>(this, 15, player, p);
    return zax::fn<DWORD(THISCALL *)(NetPacket *)>(0x520750)(p);
}

DWORD THISCALL DPlayGameRest::Send(DWORD player, NetPacket *p)
{
    auto length = [p] { return zax::fn<DWORD(THISCALL *)(NetPacket *)>(0x5203b0)(p); };
    if (length()) {
        const BYTE reliable = (BYTE)zax::fn<DWORD(THISCALL *)(NetPacket *)>(0x5203c0)(p);
        const DWORD n = length();
        const DWORD data = zax::fn<DWORD(THISCALL *)(NetPacket *)>(0x4ef8f0)(p);
        DWORD buffer[2] = {n, data};   // DPN_BUFFER_DESC
        DWORD handle = 0;
        const DWORD flags = reliable ? 8 : 0, timeout = reliable ? 0 : 100;
        DWORD hr;
        if (player == 0) {
            hr = com<DWORD>(global(glob_addr(DPLAY_CLIENT)), 8, buffer, 1u, timeout, 0u, &handle, flags);
        } else {
            BYTE *s = (BYTE *)game.players.data[player];
            hr = com<DWORD>(global(glob_addr(DPLAY_SERVER)), 14, f(s, 8), buffer, 1u, timeout, f(s, 0xc), &handle, flags);
        }
        if ((int)hr < 0)
            error_text(hr);
    }
    return zax::fn<DWORD(THISCALL *)(NetPacket *)>(0x520f60)(p);
}

// ---- The frame -------------------------------------------------------------------------------------------------

void THISCALL DPlayGameRest::Frame()
{
    (*imp_slot<void(STDCALL **)(DWORD)>(IAT_SLEEP))(0);
    if (running && hosting) {
        zax::fn<void(THISCALL *)(void *)>(0x480800)(this);
        zax::fn<DWORD(THISCALL *)(void *)>(0x51f420)(this);   // GameSpy_Think
    }
    bool done = false;
    enter(glob_addr(DPLAY_TERMINATE_LOCK));
    if (terminated) {
        done = true;
        vcall<void>(this, 10);
        const char *reason = glob<const char *>(EMPTY), *text = glob<const char *>(EMPTY);
        if (terminate_text[0]) {
            text = terminate_text;
            if (char *semi = zax::fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(terminate_text, ';')) {
                *semi = 0;
                text = semi + 1;
                reason = terminate_text;
            }
        }
        vcall<void>(game.game_type, 5, reason, text);
        terminated = 0;
    }
    leave(glob_addr(DPLAY_TERMINATE_LOCK));
    if (!done)
        zax::fn<void(THISCALL *)(void *)>(0x480a70)(this);
}

// ---- Players: kick, address, ping --------------------------------------------------------------------------------

DWORD THISCALL DPlayGameRest::Kick(DWORD index, const char *a, const char *b)
{
    ZString z;
    zs_ctor(&z);
    zax::fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&z, glob<const char *>(0x611ee8) /* "%s;%s" */, a, b);
    BYTE *p = (BYTE *)game.players.data[index];
    DWORD size = 0;
    if (c_str(&z))
        size = strlen_(c_str(&z)) + 1;
    com<DWORD>(global(glob_addr(DPLAY_SERVER)), 24, f(p, 8), c_str(&z), size, 0u);
    return zs_dtor(&z);
}

DWORD THISCALL DPlayGameRest::PlayerAddress(BYTE *player)
{
    void *addr = nullptr;
    DWORD hr = com<DWORD>(global(glob_addr(DPLAY_SERVER)), 10, f(player, 8), &addr, 0u);
    if ((int)hr < 0) {
        error_text(hr);
        return 0;
    }
    if (!addr)
        return 0;
    void *ip = nullptr;
    hr = com<DWORD>(addr, 0, glob_addr(IID_ADDRESS_IP), &ip);
    if ((int)hr < 0) {
        error_text(hr);
        com<DWORD>(addr, 2);
        return 0;
    }
    DWORD result = 0;
    if (ip) {
        DWORD size = 0x10;
        BYTE sockaddr[0x10];
        hr = com<DWORD>(ip, 6, sockaddr, &size);
        com<DWORD>(ip, 2);
        if ((int)hr < 0) {
            error_text(hr);
            com<DWORD>(addr, 2);
            return 0;
        }
        result = f(sockaddr, 4);   // sin_addr
    }
    com<DWORD>(addr, 2);
    return result;
}

DWORD THISCALL DPlayGameRest::Ping(BYTE *player)
{
    if (player[5])
        return 0;
    DWORD info[0x17];   // DPN_CONNECTION_INFO
    for (DWORD &d : info)
        d = 0;
    info[0] = 0x5c;
    if (void *client = global(glob_addr(DPLAY_CLIENT))) {
        com<DWORD>(client, 20, info, 0u);
        return info[1];
    }
    com<DWORD>(global(glob_addr(DPLAY_SERVER)), 32, f(player, 8), info, 0u);
    return info[1];
}

// ---- GameSpy answers -----------------------------------------------------------------------------------------------

DWORD THISCALL DPlayGameRest::GameSpyBasic(ZString *out)
{
    zax::fn<void(__fastcall *)(ZString *, const char *, const char *)>(0x51f290)(out, glob<const char *>(0x611574) /* "hostname" */, c_str(game.settings));
    DWORD count = 0;
    com<DWORD>(global(glob_addr(DPLAY_SERVER)), 11, (void *)nullptr, &count, 0u);
    void **list = (void **)heap_alloc(count * 4);
    com<DWORD>(global(glob_addr(DPLAY_SERVER)), 11, list, &count, 0u);
    zax::fn<void(__fastcall *)(ZString *, const char *, int)>(0x51f2b0)(out, glob<const char *>(0x61155c) /* "DirectPlayAddressCount" */, (int)count);
    ZString key, url;
    zs_ctor(&key);
    zs_ctor(&url);
    for (DWORD i = 0; i < count; i++) {
        zax::fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&key, glob<const char *>(0x611544) /* "DirectPlayAddress_%d" */, i);
        void *a = list[i];
        DWORD size = 0;
        com<DWORD>(a, 10, (char *)nullptr, &size);
        char *buf = zax::fn<char *(THISCALL *)(ZString *, DWORD)>(0x4e1660)(&url, size - 1);
        com<DWORD>(a, 10, buf, &size);
        zax::fn<void(THISCALL *)(ZString *, int)>(0x4e16d0)(&url, -1);
        com<DWORD>(list[i], 2);
        list[i] = nullptr;
        const char *k = c_str(&key);
        zax::fn<void(__fastcall *)(ZString *, const char *, const char *)>(0x51f290)(out, k, c_str(&url));
    }
    if (list)
        heap_free(list);
    zax::fn<void(THISCALL *)(void *, ZString *)>(0x51f180)(this, out);   // GameSpy_BasicInfo
    zs_dtor(&url);
    return zs_dtor(&key);
}

DWORD THISCALL DPlayGameRest::GameSpyRules(ZString *out) { return zax::fn<DWORD(THISCALL *)(void *, ZString *)>(0x51f1d0)(this, out); }

DWORD THISCALL DPlayGameRest::GameSpyPlayers(ZString *out)
{
    for (DWORD i = 0; i < game.players.count; i++)
        zax::fn<void(THISCALL *)(void *, ZString *, DWORD)>(0x51f880)(game.players.data[i], out, i);
    return zax::fn<DWORD(THISCALL *)(void *, ZString *)>(0x51f220)(this, out);   // GameSpy_Teams
}

ZAX_HOOKS(dplay_rest5a)
{
    const Hook list[] = {
        {0x47ead0, (void *)&DPlay_MessageHandlerCallback, "DPlay_MessageHandlerCallback", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x4c, 0x24, 0x08}},
        {0x47eb00, (void *)&StaticInit_DPlayGame, "StaticInit_DPlayGame", 5, {0x68, 0xf0, 0xea, 0x47, 0x00, 0xb9, 0xe8, 0xdb}},
        {0x47eb10, (void *)&DPlayGame_New, "DPlayGame_New", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x68, 0x00}},
        {0x47ed00, ptr(&DPlayGameRest::GameSpyBasic), "DPlayGame::GameSpyBasic", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x47eec0, ptr(&DPlayGameRest::GameSpyRules), "DPlayGame::GameSpyRules", 5, {0x8b, 0x44, 0x24, 0x04, 0x50, 0xe8, 0x06, 0x03}},
        {0x47eed0, ptr(&DPlayGameRest::GameSpyPlayers), "DPlayGame::GameSpyPlayers", 6, {0x53, 0x56, 0x8b, 0x5c, 0x24, 0x0c, 0x57, 0x8b}},
        {0x47efd0, ptr(&DPlayGameRest::ResetPlayers), "DPlayGame::ResetPlayers", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x47f0b0, ptr(&DPlayGameRest::Host), "DPlayGame::Host", 10, {0x8b, 0x44, 0x24, 0x08, 0x81, 0xec, 0x74, 0x04}},
        {0x47f5d0, ptr(&DPlayGameRest::Join), "DPlayGame::Join", 6, {0x81, 0xec, 0x74, 0x04, 0x00, 0x00, 0x53, 0x55}},
        {0x47fa30, ptr(&DPlayGameRest::Kick), "DPlayGame::Kick", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8d, 0x4c}},
        {0x47fac0, ptr(&DPlayGameRest::PlayerAddress), "DPlayGame::PlayerAddress", 8, {0x83, 0xec, 0x18, 0xa1, 0x2c, 0xdc, 0x6b, 0x00}},
        {0x47fba0, ptr(&DPlayGameRest::Ping), "DPlayGame::Ping", 7, {0x8b, 0x54, 0x24, 0x04, 0x83, 0xec, 0x5c, 0x8a}},
        {0x47ff30, ptr(&DPlayGameRest::CreateAndSetProvider), "DPlayGame::CreateAndSetProvider", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x50}},
        {0x480350, ptr(&DPlayGameRest::SetProvider), "DPlayGame::SetProvider", 6, {0x8b, 0x54, 0x24, 0x04, 0x85, 0xd2, 0x74, 0x11}},
        {0x4803f0, ptr(&DPlayGameRest::SetProviderByName), "DPlayGame::SetProviderByName", 6, {0x81, 0xec, 0xc8, 0x00, 0x00, 0x00, 0x53, 0x55}},
        {0x480480, ptr(&DPlayGameRest::EnumHosts), "DPlayGame::EnumHosts", 8, {0x83, 0xec, 0x4c, 0x56, 0x8b, 0x74, 0x24, 0x54}},
        {0x480530, ptr(&DPlayGameRest::Close), "DPlayGame::Close", 5, {0x51, 0x53, 0x55, 0x8b, 0xe9, 0x33, 0xdb, 0x38}},
        {0x480690, ptr(&DPlayGameRest::IsAvailable), "DPlayGame::IsAvailable", 5, {0x51, 0x8d, 0x44, 0x24, 0x00, 0x53, 0x50, 0x68}},
        {0x4806f0, ptr(&DPlayGameRest::Send), "DPlayGame::Send", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x4807e0, ptr(&DPlayGameRest::SendAndReset), "DPlayGame::SendAndReset", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x56, 0x8b}},
        {0x480bd0, ptr(&DPlayGameRest::Frame), "DPlayGame::Frame", 6, {0x51, 0x56, 0x8b, 0xf1, 0x6a, 0x00, 0xff, 0x15}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
