// The game side of the DirectPlay 8 net game (layouts and the message ids: dplay.h). The COM calls and
// the Win32 calls go through the same vtable slots and import entries as in the original.
#include "dplay.h"
#include "exe_base.h"
#include "small_helpers.h"

namespace zax {

namespace {

constexpr DWORD GAME_VTABLE = 0x5f15a8, QUEUE_VTABLE = 0x5f1618, PROVIDER_VTABLE = 0x5f1624;
constexpr DWORD HOST_ITEM_VTABLE = 0x5f1628;
constexpr DWORD COM_GLOBALS[] = {DPLAY_SERVER, DPLAY_CLIENT, DPLAY_ADDRESS};
constexpr DWORD COM_SESSION_GLOBALS[] = {DPLAY_SERVER, DPLAY_CLIENT};
constexpr DWORD DPNERR_BUFFERTOOSMALL = 0x80158100, REGDB_E_CLASSNOTREG = 0x80040154;

void *crt_memmove(void *d, const void *s, DWORD n) { return zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// COM method `slot` of an interface (stdcall, this first)
template <typename R, typename... A> R com(void *obj, int slot, A... a)
{
    auto fn = (*reinterpret_cast<R(STDCALL *const *const *)(void *, A...)>(obj))[slot];
    return fn(obj, a...);
}
void **global(DWORD a) { return at<void **>(a); }

void enter(DWORD cs) { (*imp_slot<void(STDCALL **)(void *)>(IAT_ENTER_CS))(at<void *>(cs)); }
void leave(DWORD cs) { (*imp_slot<void(STDCALL **)(void *)>(IAT_LEAVE_CS))(at<void *>(cs)); }
int w2mb(const WORD *w, char *out, int n)
{
    return (*imp_slot<int(STDCALL **)(DWORD, DWORD, const WORD *, int, char *, int, DWORD, DWORD)>(IAT_W2MB))(0, 0, w, -1, out, n, 0, 0);
}
DWORD wcslen_exe(const WORD *w) { return zax::fn<DWORD(__cdecl *)(const WORD *)>(0x5d1789)(w); }
const char *error_text(DWORD hr) { return zax::fn<const char *(__fastcall *)(DWORD)>(0x47f350)(hr); }
DWORD error_box(DWORD caption, DWORD textp) { return zax::fn<DWORD(__fastcall *)(DWORD, DWORD)>(0x4d24c0)(caption, textp); }

void *player_at(DPlayGame *g, DWORD i) { return zax::fn<void *(THISCALL *)(void *, DWORD)>(0x51f440)(g, i); }

// The players array's index of a player, -1 when it is not there (the original also tests count > -1
// first, which never holds)
DWORD player_index(DPlayGame *g, void *p)
{
    for (DWORD i = 0; i < g->game.players.count; i++)
        if (g->game.players.data[i] == p)
            return i;
    return 0xffffffff;
}

}  // namespace

// ---- GameSpy server info ---------------------------------------------------------------------------------

void THISCALL DPlayGame::ServerInfo(ZString *out)
{
    zax::fn<void(THISCALL *)(void *, ZString *)>(0x51f190)(this, out);
    const DWORD players = game.players.count;
    auto key_int = code<void(__fastcall *)(ZString *, const char *, int)>(0x51f2b0);
    key_int(out, glob<const char *>(0x611598) /* "hostport" */, *glob<int *>(0x611408));
    key_int(out, glob<const char *>(0x61158c) /* "numplayers" */, (int)players);
    key_int(out, glob<const char *>(0x611580) /* "maxplayers" */, (int)game.settings->max_players);
}

// ---- Destruction -------------------------------------------------------------------------------------------

// The three DirectPlay objects released, the three critical sections deleted, CoUninitialize, the queue
// freed, then the net game base (0x51ee30)
DPlayGame *THISCALL DPlayGame::DeletingDtor(DWORD flags)
{
    game.vftable = vt(GAME_VTABLE);
    for (DWORD g : COM_GLOBALS)
        if (void *o = *global(g)) {
            com<DWORD>(o, 2);
            *global(g) = nullptr;
        }
    auto del = *imp_slot<void(STDCALL **)(void *)>(IAT_DELETE_CS);
    del(glob<void *>(DPLAY_LOCK));
    del(glob<void *>(DPLAY_TERMINATE_LOCK));
    del(glob<void *>(DPLAY_QUEUE_LOCK));
    (*imp_slot<void(STDCALL **)()>(IAT_CO_UNINIT))();
    queue.vftable = vt(QUEUE_VTABLE);
    if (queue.data) {
        heap_free(queue.data);
        queue.data = nullptr;
        queue.capacity = 0;
    }
    queue.count = 0;
    zax::fn<void(THISCALL *)(void *)>(0x51ee30)(this);
    if (flags & 1)
        heap_free(this);
    return this;
}

// Both address objects released (Release, slot 2; no null test, as in the original), then the node's destructor
// NetNameNodeB_Dtor 0x51ec00 (the next hosts deleted through their slot 0, the name freed); eax this
DPlayHost *THISCALL DPlayHost::DeletingDtor(DWORD flags)
{
    vftable = vt(HOST_ITEM_VTABLE);
    com<DWORD>(host_address, 2);
    com<DWORD>(device_address, 2);
    zax::fn<void(THISCALL *)(void *)>(0x51ec00)(this);
    if (flags & 1)
        heap_free(this);
    return this;
}

DPlayProvider *THISCALL DPlayProvider::DeletingDtor(DWORD flags)
{
    vftable = vt(PROVIDER_VTABLE);
    if (guid) {
        heap_free(guid);
        guid = nullptr;
    }
    zax::fn<void(THISCALL *)(void *)>(0x51ead0)(this);
    if (flags & 1)
        heap_free(this);
    return this;
}

// ---- Service providers ---------------------------------------------------------------------------------

// The client's EnumServiceProviders into 2000 bytes (again with the size it asks for after
// DPNERR_BUFFERTOOSMALL); every DPN_SERVICE_PROVIDER_INFO (0x20 bytes) becomes an item with the name
// (WideCharToMultiByte into a stack buffer that keeps its old bytes when the name is null) and a
// copy of the GUID, each new item in front of the previous ones. The list, or null with the error text.
void *THISCALL DPlayGame::EnumServiceProviders()
{
    DWORD size = 0x7d0, count = 0;
    BYTE *buf = (BYTE *)heap_alloc(0x7d0);
    void *client = *global(glob_addr(DPLAY_CLIENT));
    DWORD hr = com<DWORD>(client, 4, (void *)nullptr, (void *)nullptr, buf, &size, &count, 0u);
    if (hr == DPNERR_BUFFERTOOSMALL) {
        if (buf)
            heap_free(buf);
        buf = (BYTE *)heap_alloc(size);
        hr = com<DWORD>(*global(glob_addr(DPLAY_CLIENT)), 4, (void *)nullptr, (void *)nullptr, buf, &size, &count, 0u);
    }
    if ((int)hr < 0) {
        if (buf)
            heap_free(buf);
        error_text(hr);
        return nullptr;
    }
    DPlayProvider *list = nullptr;
    char name[0x100];
    for (DWORD i = 0; i < count; i++) {
        BYTE *info = buf + i * 0x20 + 4;   // the GUID; the name pointer follows it
        const WORD *w = *(const WORD **)(info + 0x10);
        if (w) {
            const DWORD n = wcslen_exe(w) + 1;
            w2mb(w, name, (int)(n - 1));
            name[n - 1] = 0;
        }
        auto *p = (DPlayProvider *)heap_alloc(0x10);
        if (p) {
            zax::fn<void(THISCALL *)(void *, const char *, void *)>(0x51ea10)(p, name, list);
            p->vftable = vt(PROVIDER_VTABLE);
            BYTE *g = (BYTE *)heap_alloc(0x10);
            p->guid = g;
            memcpy(g, info, 0x10);
        }
        list = p;
    }
    if (buf)
        heap_free(buf);
    return list;
}

// The old server and client released, then the server (server != 0) or the client created and
// initialised with this object and the message trampoline 0x47ead0, and the address object once.
// al = 1, or 0 after the error box (class not registered) or the error text; the upper bytes of eax
// are those of the last call (or the address object pointer when it existed).
DWORD THISCALL DPlayGame::CreateDirectPlay(DWORD server)
{
    for (DWORD g : COM_SESSION_GLOBALS)
        if (void *o = *global(g)) {
            com<DWORD>(o, 2);
            *global(g) = nullptr;
        }
    auto create = *imp_slot<DWORD(STDCALL **)(DWORD, DWORD, DWORD, DWORD, void **)>(IAT_CO_CREATE);
    const DWORD target = (server & 0xff) ? glob_addr(DPLAY_SERVER) : glob_addr(DPLAY_CLIENT);
    DWORD hr = (server & 0xff) ? create(glob_addr(0x606190), 0, 1, glob_addr(0x606170), global(glob_addr(DPLAY_SERVER)))
                               : create(glob_addr(0x6061a0), 0, 1, glob_addr(0x606180), global(glob_addr(DPLAY_CLIENT)));
    if (hr == REGDB_E_CLASSNOTREG)
        return error_box(glob_addr(0x6114ac) /* "Multi-player Netword Error" */, glob_addr(0x6114c8)) & ~0xffu;
    if ((int)hr < 0)
        return (DWORD)(uintptr_t)error_text(hr) & ~0xffu;
    hr = com<DWORD>(*global(target), 3, (void *)this, fn_addr(0x47ead0u), 0u);
    if ((int)hr < 0)
        return (DWORD)(uintptr_t)error_text(hr) & ~0xffu;
    DWORD r = (DWORD)(uintptr_t)*global(glob_addr(DPLAY_ADDRESS));
    if (!r) {
        r = create(glob_addr(0x6061d0), 0, 7, glob_addr(0x6061c0), global(glob_addr(DPLAY_ADDRESS)));
        if ((int)r < 0)
            return (DWORD)(uintptr_t)error_text(r) & ~0xffu;
    }
    return (r & ~0xffu) | 1;
}

void *THISCALL DPlayGame::InitServiceProviders()
{
    if (!(zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x47fdf0)(this, 0) & 0xff))
        return nullptr;
    if (void *list = zax::fn<void *(THISCALL *)(void *)>(0x47fc20)(this))
        return list;
    error_box(glob_addr(0x6114ac) /* "Multi-player Netword Error" */, glob_addr(0x611ef0) /* "Could not find any DirectPlay service Providers" */);
    return nullptr;
}

// ---- Message handler ---------------------------------------------------------------------------------------

DWORD THISCALL DPlayGame::MessageHandler(DWORD id, BYTE *msg)
{
    switch (id) {
    case 0xffff0011: {   // receive: queued, the buffer kept
        enter(glob_addr(DPLAY_QUEUE_LOCK));
        DWORD want = queue.count + 1;
        if (queue.capacity < want) {
            unsigned long long cap = queue.capacity;
            const DWORD v = (DWORD)x87_ftol((long double)cap * (long double)*glob<const float *>(0x5ea5f4));
            if (v > want)
                want = v;
            if (queue.capacity < want) {
                queue.capacity = want;
                auto *d = (DPlayMessage *)heap_alloc(want * 20);
                for (DWORD i = 0; i < queue.count; i++)
                    d[i] = queue.data[i];
                if (queue.data)
                    heap_free(queue.data);
                queue.data = d;
            }
        }
        DPlayMessage &m = queue.data[queue.count++];
        const DWORD *s = (const DWORD *)msg;
        m.context = s[2];
        m.data = (void *)(uintptr_t)s[3];
        m.size = s[4];
        m.sender = s[1];
        m.handle = s[5];
        leave(glob_addr(DPLAY_QUEUE_LOCK));
        ++*glob<DWORD *>(DPLAY_RECEIVED);
        return 0x15800e;
    }
    case 0xffff0014: {   // send complete: an error counts at +0x10 of the player
        const DWORD slot = ((DWORD *)msg)[2];
        if (slot >= 100)
            return 0;
        BYTE *p = slot == 0 ? (BYTE *)player_at(this, 0) : (BYTE *)slots[slot].session;
        if (!p)
            return 0;
        const DWORD hr = ((DWORD *)msg)[3];
        if ((int)hr >= 0)
            return 0;
        error_text(hr);
        ++*(DWORD *)(p + 0x10);
        return 0;
    }
    case 0xffff000b: {   // enum hosts response: a host item in front of the list the user context names
        void **head = (void **)**(void ***)(msg + 0x18);
        const WORD *w = *(const WORD **)(*(BYTE **)(msg + 0xc) + 0x30);
        char name[0x100];
        if (w) {
            const DWORD n = wcslen_exe(w) + 1;
            w2mb(w, name, (int)(n - 1));
            name[n - 1] = 0;
        }
        BYTE *item = (BYTE *)heap_alloc(0x14);
        if (!item) {
            *head = nullptr;
            return 0;
        }
        void *device = *(void **)(msg + 8), *sender = *(void **)(msg + 4);
        zax::fn<void(THISCALL *)(void *, const char *, void *)>(0x51eb40)(item, name, *head);
        *(DWORD *)item = vt(HOST_ITEM_VTABLE);
        *(void **)(item + 0xc) = sender;
        com<DWORD>(sender, 1);
        *(void **)(item + 0x10) = device;
        com<DWORD>(device, 1);
        *head = item;
        return 0;
    }
    case 0xffff000e:   // indicate connect
        *(DWORD *)(msg + 0x18) = 0xfeadface;
        return 0;
    case 0xffff0007: {   // create player: the highest free slot (100 down to 2) becomes its context
        if (!((DWORD *)msg)[2])
            return 0;
        enter(glob_addr(DPLAY_LOCK));
        DWORD i = 100;
        for (; i > 1; i--) {
            BYTE *s = (BYTE *)slots + i * 12;
            if (!s[0] && !*(DWORD *)(s + 8))
                break;
        }
        if (i <= 1) {
            leave(glob_addr(DPLAY_LOCK));
            return 0x80158260;
        }
        ((DWORD *)msg)[2] = i;
        BYTE *s = (BYTE *)slots + i * 12;
        s[0] = 1;
        slots_changed = 1;
        *(DWORD *)(s + 4) = ((DWORD *)msg)[1];
        leave(glob_addr(DPLAY_LOCK));
        return 0;
    }
    case 0xffff0009: {   // destroy player
        const DWORD slot = ((DWORD *)msg)[2];
        if (!running || !slot || slot >= 100)
            return 0;
        enter(glob_addr(DPLAY_LOCK));
        slots[slot].removed = 1;
        slots_changed = 1;
        leave(glob_addr(DPLAY_LOCK));
        return 0;
    }
    case 0xffff0016: {   // terminate session: the text (or "") copied
        const char *t = *(const char **)(msg + 8);
        if (!t)
            t = glob<const char *>(0x62e558);
        enter(glob_addr(DPLAY_TERMINATE_LOCK));
        terminated = 1;
        DWORD n = 0;
        while (t[n])
            n++;
        memcpy(terminate_text, t, n + 1);
        leave(glob_addr(DPLAY_TERMINATE_LOCK));
        return 0;
    }
    default:
        return 0;
    }
}

// ---- Slots and messages --------------------------------------------------------------------------------------

void THISCALL DPlayGame::ProcessSlots()
{
    enter(glob_addr(DPLAY_LOCK));
    if (slots_changed) {
        slots_changed = 0;
        for (DWORD i = 0; i < 100; i++) {
            DPlaySlot &s = slots[i];
            if (s.added) {
                s.added = 0;
                const DWORD dpnid = s.dpnid;
                BYTE *session = vcall<BYTE *>(game.game_type, 20, 0u);
                s.session = session;
                vcall<DWORD>(&game.players, 2, &session);
                *(DWORD *)(session + 8) = dpnid;
                *(DWORD *)(session + 0xc) = i;
                session[5] = 0;
                session[6] = 1;
                DWORD size = 0;
                const DWORD hr = com<DWORD>(*global(glob_addr(DPLAY_SERVER)), 9, dpnid, (void *)nullptr, &size, 0u);
                if (hr != DPNERR_BUFFERTOOSMALL && (int)hr < 0)
                    error_text(hr);
                BYTE *info = (BYTE *)heap_alloc(size);
                memset(info, 0, size);
                *(DWORD *)info = 0x18;
                const DWORD hr2 = com<DWORD>(*global(glob_addr(DPLAY_SERVER)), 9, dpnid, info, &size, 0u);
                if ((int)hr2 < 0)
                    error_text(hr2);
                char name[0x10];
                if (const WORD *w = *(const WORD **)(info + 8)) {
                    w2mb(w, name, 0xf);
                    name[0xf] = 0;
                }
                zax::fn<void(THISCALL *)(void *, const char *)>(0x4e1930)(*(void **)(session + 0x1c), name);
                if (info)
                    heap_free(info);
                ZString *text = (ZString *)(session + 0x24);
                zax::fn<void(THISCALL *)(ZString *)>(0x4df2b0)(text);
                zax::fn<void(THISCALL *)(ZString *, DWORD)>(0x4e0e60)(text, 10);
                for (int k = 0; k < 10; k++) {
                    const DWORD c = zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x55c4e0)(glob<void *>(0x7124c0), 0x61, 0x7a);
                    zax::fn<void(THISCALL *)(ZString *, DWORD, DWORD)>(0x4df540)(text, c, 1);
                }
                vcall<void>(game.game_type, 9, session);
            }
            if (s.removed) {
                s.removed = 0;
                void *session = s.session;
                s.session = nullptr;
                s.dpnid = 0;
                if (session) {
                    const DWORD k = player_index(this, session);
                    if (k != 0xffffffff)
                        vcall<void>(game.game_type, 10, k);
                    zax::fn<void(__cdecl *)(DWORD)>(0x5c5750)(((DWORD *)session)[3]);
                    if (void *p = game.players.data[k])
                        vcall<void *>(p, 0, 1u);
                    const DWORD c = game.players.count;
                    if (k < c) {
                        if (k + 1 < c)
                            crt_memmove(&game.players.data[k], &game.players.data[k + 1], (c - (k + 1)) * 4);
                        game.players.count--;
                    }
                }
            }
        }
    }
    leave(glob_addr(DPLAY_LOCK));
}

// Until the queue is empty: the first message taken out (the counters 0x6bdc20 per call and 0x6bdc28
// per message), handed to slot 2 of the game type (player index, data, size) when its slot names a
// player whose dpnid is the sender (slot 0 always), then the buffer returned to the server and the
// client. The handle of an empty round is the last message's.
void THISCALL DPlayGame::ProcessMessages()
{
    ++*glob<DWORD *>(DPLAY_PROCESS_CALLS);
    DPlayMessage m = {0, 0, nullptr, 0, 0};
    DWORD handle = 0;
    for (;;) {
        enter(glob_addr(DPLAY_QUEUE_LOCK));
        const bool any = queue.count > 0;
        if (any) {
            ++*glob<DWORD *>(DPLAY_PROCESSED);
            m = queue.data[0];
            const DWORD c = queue.count;
            if (c > 1)
                crt_memmove(queue.data, (BYTE *)queue.data + 20, c * 20 - 20);
            queue.count--;
            handle = m.handle;
        }
        leave(glob_addr(DPLAY_QUEUE_LOCK));
        if (!any)
            return;
        const DWORD slot = m.context;
        if (slot < 100) {
            BYTE *p = slot == 0 ? (BYTE *)player_at(this, 0) : (BYTE *)slots[slot].session;
            if (p && (m.sender == *(DWORD *)(p + 8) || slot == 0)) {
                const DWORD k = player_index(this, p);
                if (k != 0xffffffff)
                    vcall<void>(game.game_type, 2, k, m.data, m.size);
            }
        }
        if (void *o = *global(glob_addr(DPLAY_SERVER)))
            com<DWORD>(o, 25, handle, 0u);
        if (void *o = *global(glob_addr(DPLAY_CLIENT)))
            com<DWORD>(o, 15, handle, 0u);
    }
}

ZAX_HOOKS(dplay)
{
    const Hook list[] = {
        {0x47ee70, ptr(&DPlayGame::ServerInfo), "DPlayGame::ServerInfo", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x47ef10, ptr(&DPlayGame::DeletingDtor), "DPlayGame::DeletingDtor", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0xc7, 0x06}},
        {0x47fc20, ptr(&DPlayGame::EnumServiceProviders), "DPlayGame::EnumServiceProviders", 6, {0x81, 0xec, 0x14, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x47fdb0, ptr(&DPlayProvider::DeletingDtor), "DPlayProvider::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0xc7, 0x06}},
        {0x480310, ptr(&DPlayHost::DeletingDtor), "DPlayHost::DeletingDtor", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0xc7, 0x06}},
        {0x47fdf0, ptr(&DPlayGame::CreateDirectPlay), "DPlayGame::CreateDirectPlay", 5, {0xa1, 0x2c, 0xdc, 0x6b, 0x00, 0x56, 0x57, 0x8b}},
        {0x47ff50, ptr(&DPlayGame::MessageHandler), "DPlayGame::MessageHandler", 10, {0x8b, 0x44, 0x24, 0x04, 0x81, 0xec, 0x0c, 0x01}},
        {0x4803c0, ptr(&DPlayGame::InitServiceProviders), "DPlayGame::InitServiceProviders", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x00, 0xe8, 0x26, 0xfa}},
        {0x480800, ptr(&DPlayGame::ProcessSlots), "DPlayGame::ProcessSlots", 6, {0x83, 0xec, 0x20, 0x53, 0x8b, 0xd9, 0x68, 0xf0}},
        {0x480a70, ptr(&DPlayGame::ProcessMessages), "DPlayGame::ProcessMessages", 8, {0x83, 0xec, 0x14, 0xa1, 0x20, 0xdc, 0x6b, 0x00}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
