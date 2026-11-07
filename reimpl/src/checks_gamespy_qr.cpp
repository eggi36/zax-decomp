// Checks of GameSpy query and reporting, the packet helpers, gs_encode/gs_encrypt, nonport.c, darray.c, hashtable.c
// (gamespy_qr.cpp) and the wsock32 thunks (gamespy_thunks.cpp) against the original through the trampolines, the
// hooks staying in (checks_gamespy_common.h). Cases: qr_init with every failure (socket, binds on many ports, a host
// name, getsockname for port 0, an own record or the static one), then queued queries (every query word, several in
// one packet, echo, secure challenges of every length, garbage, failed receives) through qr_process_queries (the
// heartbeat timer: none sent yet, 300 s, the clock going back, the state change resends) and
// qr_process_queries_no_heartbeat with callbacks answering up to 0x577 characters, qr_send_statechanged,
// qr_shutdown; packet_send, add_queryid, the send functions and qr_value_for_key directly; gs_encode and gs_encrypt
// on random data; random darray and hash table operations with recording callbacks; each thunk once.
#include "checks_gamespy_common.h"

namespace zax {

namespace {

using namespace gscheck;

struct QrRecord {
    DWORD query_socket;
    DWORD heartbeat_socket;
    char game_name[0x40];
    char secret_key[0x80];
    void *basic, *info, *rules, *players;
    DWORD last_heartbeat;
    int query_id;
    int packet_number;
    int query_port;
    signed char state_changes;
    BYTE pad[3];
    void *user_data;
};

char g_answer[4][0x578];
template <int I> void __cdecl answer(char *buf, int len, void *user)
{
    ev_raw(0x231 + I, kind(buf), (DWORD)len, ptrv(user));
    count(3 + I);
    st_bytes(buf, 0x578);   // cleared by the caller
    scpy(buf, g_answer[I]);
}
void make_answers()
{
    for (int i = 0; i < 4; i++) {
        g_answer[i][0] = 0;
        const int pairs = rnd(5) == 0 ? 40 + (int)rnd(80) : (int)rnd(8);
        key_values(g_answer[i], 0x577, pairs);
        if (rnd(10) == 0) {   // one long value without '\'
            const DWORD n = slen(g_answer[i]);
            for (DWORD j = n; j < 0x570; j++)
                g_answer[i][j] = 'a';
            g_answer[i][0x570] = 0;
        }
    }
}

void st_record(const QrRecord *r)
{
    st_raw(r->query_socket);
    st_raw(r->heartbeat_socket);
    st_text(r->game_name);
    st_text(r->secret_key);
    st_raw(ptrv(r->basic));
    st_raw(ptrv(r->info));
    st_raw(ptrv(r->rules));
    st_raw(ptrv(r->players));
    st_raw(r->last_heartbeat);
    st_raw((DWORD)r->query_id);
    st_raw((DWORD)r->packet_number);
    st_raw((DWORD)r->query_port);
    st_raw((DWORD)(BYTE)r->state_changes);
    st_raw(ptrv(r->user_data));
}
void st_qr()
{
    QrRecord *r = *at<QrRecord **>(0x628078);
    st_raw(kind(r));
    st_record(at<QrRecord *>(0x627f88));
    st_bytes(at<void *>(0x715418), 0x100);
    st_raw(*at<DWORD *>(0x715518));
    st_bytes(at<void *>(0x71551c), 0x200);
    st_bytes(at<void *>(0x715720), 0x10);
}

const char *const QUERIES[] = {"basic", "info", "rules", "players", "status", "packets", "echo", "secure", "final", "queryid"};
void random_query(char *q, DWORD cap)
{
    q[0] = 0;
    const int n = (int)rnd(4);
    for (int i = 0; i < n; i++) {
        append(q, cap, "\\");
        const DWORD w = rnd(10);
        append(q, cap, QUERIES[w]);
        append(q, cap, "\\");
        if (w == 6 || w == 7) {
            const int len = rnd(6) == 0 ? 0x80 + (int)rnd(40) : (int)rnd(40);
            for (int j = 0; j < len; j++) {
                char c[2] = {(char)('A' + rnd(52) % 26 + (rnd(2) ? 32 : 0)), 0};
                append(q, cap, c);
            }
        }
    }
    if (rnd(6) == 0)
        append(q, cap, "garbage\\x");
}

void qr_case(int k)
{
    make_answers();
    if (rnd(15) == 0)
        g_fail_socket = 1;
    const int bind_fails = rnd(4) ? 0 : rnd(4) ? (int)rnd(5) : 32;
    g_fail_bind = bind_fails >= 32 ? 0xffffffffu : (1u << bind_fails) - 1;
    if (rnd(10) == 0)
        g_getsockname_result = -1;
    g_host_found = rnd(3) != 0;
    g_clock_step = 1 + rnd(20);
    const char *const IPS[] = {nullptr, "127.0.0.1", "10.0.0.5", "myhost", "255.255.255.255"};
    const char *ip = IPS[rnd(5)];
    const int port = rnd(4) == 0 ? 0 : 26794 + (int)rnd(20);
    QrRecord *mine = nullptr;
    const bool own = rnd(3) == 0;
    const char *secret = rnd(4) ? "J3An3s" : "k";   // never empty: gs_encrypt divides by its length
    const int r = side_fn<int(__cdecl *)(QrRecord **, const char *, int, const char *, const char *, void *, void *, void *, void *, void *)>(0x5c82a0)(
        own ? &mine : nullptr, ip, port, k % 2 ? "zax" : "zaxtest", secret, (void *)&answer<0>, (void *)&answer<1>,
        (void *)&answer<2>, (void *)&answer<3>, (void *)(uintptr_t)(0x4400 + k));
    ev_raw(0x120, (DWORD)r, own ? kind(mine) : 0xdead);
    st_qr();
    if (r != 0)
        return;
    QrRecord *rec = own ? mine : *at<QrRecord **>(0x628078);
    st_record(rec);
    const DWORD qs = rec->query_socket;
    const int steps = 1 + (int)rnd(5);
    for (int s = 0; s < steps; s++) {
        const int nq = (int)rnd(4);
        for (int i = 0; i < nq; i++) {
            if (rnd(10) == 0) {
                add_packet(qs, "", -1);
                continue;
            }
            char q[0x100];
            random_query(q, 0xff);
            Packet *p = add_packet(qs, q);
            if (p)
                p->from.port = (WORD)random();
        }
        switch (rnd(6)) {
        case 0:
            g_clock += 301000;
            break;
        case 1:
            g_clock += 31000;
            break;
        case 2:
            if (rec->last_heartbeat)
                g_clock = rec->last_heartbeat - 5;
            break;
        case 3:   // the timers' boundaries (30 s, 300 s)
            clock_at(rec->last_heartbeat, (rnd(2) ? 30000 : 300000) - 1 + rnd(3));
            break;
        }
        if (rnd(3) == 0)
            rec->state_changes = (signed char)(rnd(14) - 2);
        if (rnd(5) == 0)
            side_fn<void(__cdecl *)(QrRecord *)>(0x5c8f20)(own ? rec : nullptr);
        if (rnd(2))
            side_fn<void(__cdecl *)(QrRecord *)>(0x5c85c0)(own ? rec : nullptr);
        else
            side_fn<void(__cdecl *)(QrRecord *)>(0x5c8a60)(own ? rec : nullptr);
        st_record(rec);
    }
    st_qr();
    side_fn<void(__cdecl *)(QrRecord *)>(0x5c8fb0)(own ? rec : nullptr);
    st_qr();
}

// packet_send, add_queryid, the send functions, send_final and qr_value_for_key directly on an arena record
void packet_case(int k)
{
    (void)k;
    make_answers();
    QrRecord *r = (QrRecord *)arena(sizeof(QrRecord));
    r->query_socket = 0x1001;
    r->heartbeat_socket = 0x1001;
    scpy(r->game_name, "zax");
    scpy(r->secret_key, rnd(5) ? "J3An3s" : "abc");
    r->basic = (void *)&answer<0>;
    r->info = (void *)&answer<1>;
    r->rules = (void *)&answer<2>;
    r->players = (void *)&answer<3>;
    r->query_id = (int)rnd(1000);
    r->packet_number = (int)rnd(10);
    r->user_data = (void *)0x4321;
    SockAddr to = {2, 0x2211, 0x0e00000a, {}};
    char out[0x580];
    out[0] = 0;
    if (rnd(2))
        key_values(out, rnd(3) ? 0x100 : 0x540, 40);   // the answer buffer never holds more than 0x546
    const int ops = 1 + (int)rnd(6);
    for (int i = 0; i < ops; i++) {
        char data[0x900];
        data[0] = 0;
        switch (rnd(10)) {
        case 9: {   // the sum of both lengths around 0x546
            const int want = 0x544 + (int)rnd(4) - (int)slen(out);
            for (int j = 0; j < want && j < 0x8f0; j++)
                data[j] = j % 7 == 0 ? '\\' : 'k';
            data[want > 0 ? (want < 0x8f0 ? want : 0x8f0) : 0] = 0;
            side_fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, char *)>(0x5c93e0)(r, &to, out, data);
            break;
        }
        case 0:
        case 1:
            key_values(data, rnd(3) ? 0x200 : 0x8ff, rnd(3) ? (int)rnd(20) : 200);
            if (rnd(8) == 0) {
                for (int j = 0; j < 0x600; j++)
                    data[j] = 'b';
                data[0x600] = 0;
            }
            side_fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, char *)>(0x5c93e0)(r, &to, out, data);
            st_bytes(data, slen(data) + 1);
            break;
        case 2:
            side_fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *)>(0x5c9340)(r, &to, out);
            break;
        case 3:
            side_fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *)>(0x5c95a0 + 0x70 * rnd(4))(r, &to, out);
            break;
        case 4: {
            key_values(data, 0x600, (int)rnd(3));
            if (rnd(4) == 0)
                for (int j = (int)slen(data); j < 0x550; j++)
                    data[j] = 'e', data[j + 1] = 0;
            side_fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, const char *)>(0x5c9760)(r, &to, out, data);
            break;
        }
        case 5:
        case 6: {
            char challenge[0x100];
            const int len = rnd(5) == 0 ? (int)rnd(0xc0) : (int)rnd(12);
            for (int j = 0; j < len; j++)
                challenge[j] = (char)('a' + rnd(26));
            challenge[len] = 0;
            side_fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, const char *)>(0x5c97c0)(r, &to, out, challenge);
            break;
        }
        case 7:
        case 8: {
            char q[0x100];
            random_query(q, 0xff);
            key_values(q, 0xff, (int)rnd(3));
            const char *v = side_fn<char *(__cdecl *)(const char *, const char *)>(0x5c9010)(q, rnd(3) ? QUERIES[rnd(10)] : word());
            ev_raw(0x121, ptrv(v), hash_text(v));
            break;
        }
        }
        st_bytes(out, slen(out) + 1);
        st_raw((DWORD)r->packet_number);
    }
    st_bytes(at<void *>(0x715518), 0x204);
}

void crypt_case(int k)
{
    (void)k;
    BYTE key[0x40], data[0x200], copy[0x200];
    char out[0x300];
    const int nk = 1 + (int)rnd(rnd(4) ? 8 : 0x40);
    for (int i = 0; i < nk; i++)
        key[i] = (BYTE)random();
    const int n = rnd(6) == 0 ? -(int)rnd(3) : (int)rnd(rnd(4) ? 16 : 0x200);
    for (int i = 0; i < 0x200; i++)
        data[i] = (BYTE)random();
    memcpy(copy, data, sizeof copy);
    if (rnd(2)) {
        side_fn<void(__cdecl *)(const BYTE *, int, BYTE *, int)>(0x5c91e0)(key, nk, data, n);
        st_bytes(data, sizeof data);
    } else {
        memset(out, 0x55, sizeof out);
        side_fn<void(__cdecl *)(const BYTE *, int, char *)>(0x5c9110)(copy, n, out);
        st_bytes(out, sizeof out);
    }
}

// ---- darray and hashtable ---------------------------------------------------------------------------------------

DWORD elem_key(const void *e) { return *(const DWORD *)e; }
void __cdecl elem_free(void *e)
{
    ev_raw(0x250, elem_key(e));
}
int __cdecl elem_compare(const void *a, const void *b)
{
    ev_raw(0x251, elem_key(a), elem_key(b));
    const DWORD x = elem_key(a) % 97, y = elem_key(b) % 97;
    return x < y ? -1 : x > y ? 1 : 0;
}
int __cdecl elem_hash(const void *e, int n)
{
    ev_raw(0x252, elem_key(e), (DWORD)n);
    return (int)(elem_key(e) % 97 % (DWORD)n);
}
void st_array(const DArray *a)
{
    st_raw((DWORD)a->count);
    st_raw((DWORD)a->capacity);
    st_raw((DWORD)a->elem_size);
    st_raw((DWORD)a->grow_by);
    st_raw(ptrv((void *)a->free_fn));
    st_bytes(a->list, (DWORD)(a->count * a->elem_size));
}

void array_case(int k)
{
    (void)k;
    const int size = 4 * (1 + (int)rnd(3));
    BYTE e[12];
    if (rnd(2)) {
        DArray *a = side_fn<DArray *(__cdecl *)(int, int, ElemFree)>(0x5c9c20)(size, (int)rnd(5), rnd(2) ? elem_free : nullptr);
        st_array(a);
        const int ops = (int)rnd(60);
        for (int i = 0; i < ops; i++) {
            for (int j = 0; j < 12; j++)
                e[j] = (BYTE)random();
            const int n = side_fn<int(__cdecl *)(DArray *)>(0x5c9cc0)(a);
            switch (rnd(5)) {
            case 0:
            case 1:
                side_fn<void(__cdecl *)(DArray *, const void *)>(0x5c9cf0)(a, e);
                break;
            case 2:
                if (n)
                    side_fn<void(__cdecl *)(DArray *, const void *, int)>(0x5c9d80)(a, e, (int)rnd((DWORD)n));
                break;
            case 3:
                if (n) {
                    const BYTE *p = (const BYTE *)side_fn<void *(__cdecl *)(DArray *, int)>(0x5c9cd0)(a, (int)rnd((DWORD)n));
                    ev_raw(0x253, (DWORD)(p - a->list));
                }
                break;
            case 4: {
                const int r = side_fn<int(__cdecl *)(DArray *, const void *, ElemCompare, int, int)>(0x5c9dd0)(
                    rnd(10) ? a : nullptr, e, elem_compare, n ? (int)rnd((DWORD)n) : 0, (int)rnd(2));
                ev_raw(0x254, (DWORD)r);
                break;
            }
            }
        }
        st_array(a);
        side_fn<void(__cdecl *)(DArray *)>(0x5c9c70)(a);
    } else {
        HashTable *t = side_fn<HashTable *(__cdecl *)(int, int, int, DWORD, DWORD, DWORD)>(0x5c9ac0)(
            size, 1 + (int)rnd(8), (int)rnd(4), ptrv((void *)&elem_hash), ptrv((void *)&elem_compare), rnd(2) ? ptrv((void *)&elem_free) : 0);
        const int ops = (int)rnd(60);
        for (int i = 0; i < ops; i++) {
            for (int j = 0; j < 12; j++)
                e[j] = (BYTE)random();
            if (rnd(2)) {
                side_fn<void(__cdecl *)(HashTable *, const void *)>(0x5c9b70)(t, e);
            } else {
                const BYTE *p = (const BYTE *)side_fn<void *(__cdecl *)(HashTable *, const void *)>(0x5c9bd0)(t, e);
                ev_raw(0x255, p ? elem_key(p) : 0xdead000c, p != nullptr);
            }
        }
        st_raw((DWORD)t->nbuckets);
        for (int b = 0; b < t->nbuckets; b++)
            st_array(t->buckets[b]);
        side_fn<void(__cdecl *)(HashTable *)>(0x5c9b30)(t);
    }
}

// ---- The thunks and nonport.c ------------------------------------------------------------------------------------

void thunk_case(int k)
{
    (void)k;
    SockAddr a = {2, 0x1111, 0x0f00000a, {}};
    int n = 0x10;
    char buf[0x40] = "\\status\\";
    FdSet set;
    set.count = 1;
    set.sockets[0] = 0x1003;
    TimeVal tv = {0, 0};
    add_packet(0x1003, "\\x\\y");
    g_tcp_length = 10;
    memcpy(g_tcp, "\\secure\\ab", 10);
    DWORD v = 1;
    BYTE data[0x190];
    ev_raw(0x130, side_fn<DWORD(WINAPI *)(const char *)>(0x5ca9e0)("host") != 0);
    ev_raw(0x130, side_fn<DWORD(WINAPI *)(const char *)>(0x5ca9e6)("1.2.3.4"));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, SockAddr *, int *)>(0x5ca9ec)(0x1003, &a, &n));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, const SockAddr *, int)>(0x5ca9f2)(0x1003, &a, 0x10));
    ev_raw(0x130, side_fn<WORD(WINAPI *)(DWORD)>(0x5ca9f8)(0x1234));
    ev_raw(0x130, side_fn<DWORD(WINAPI *)(int, int, int)>(0x5ca9fe)(2, 2, 17));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD)>(0x5caa04)(0x1003));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, const char *, int, int, const SockAddr *, int)>(0x5caa0a)(0x1003, buf, 8, 0, &a, 0x10));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, FdSet *, FdSet *, FdSet *, TimeVal *)>(0x5caa16)(0x40, &set, nullptr, nullptr, &tv));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, FdSet *)>(0x5caa3a)(0x1003, &set));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, char *, int, int, SockAddr *, int *)>(0x5caa10)(0x1003, buf, 0x3f, 0, &a, &n));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, const char *, int, int)>(0x5caa1c)(0x1003, buf, 4, 0));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, char *, int, int)>(0x5caa22)(0x1003, buf, 0x3f, 0));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, const SockAddr *, int)>(0x5caa28)(0x1003, &a, 0x10));
    ev_raw(0x130, side_fn<WORD(WINAPI *)(DWORD)>(0x5caa2e)(0x3412));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, int, int, const DWORD *, int)>(0x5caa34)(0x1003, 0xffff, 0x20, &v, 4));
    ev_raw(0x130, hash_text(side_fn<char *(WINAPI *)(DWORD)>(0x5caa40)(0x0100007f)));
    ev_raw(0x130, side_fn<DWORD(WINAPI *)(DWORD)>(0x5caa46)(0x7f000001));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)(DWORD, BYTE *)>(0x5caa4c)(0x101, data));
    ev_raw(0x130, (DWORD)side_fn<int(WINAPI *)()>(0x5caa52)());
    st_bytes(buf, sizeof buf);
    st_bytes(&a, sizeof a);
    // nonport.c: msleep, SocketStartUp, SocketShutDown (a jump to the WSACleanup thunk: side 0 runs that thunk)
    side_fn<void(__cdecl *)(DWORD)>(0x5c9a80)(25);
    side_fn<void(__cdecl *)()>(0x5c9a90)();
    if (g_side == 0)
        original<void(__cdecl *)()>(0x5caa52)();
    else
        at<void(__cdecl *)()>(0x5c9ab0)();
}

}  // namespace

ZAX_CHECKS(gamespy_qr)
{
    (void)cls;
    (void)n;
    install();
    cases("gamespy qr: init, process queries (both), state changed, shutdown", scaled(400), 0x5c82a0u, [](int k) { qr_case(k); });
    cases("gamespy qr: packet_send, add_queryid, send_*, qr_value_for_key", scaled(500), 0x5c93e0u, [](int k) { packet_case(k); });
    cases("gamespy qr: gs_encode, gs_encrypt", scaled(500), 0x5c9110u, [](int k) { crypt_case(k); });
    cases("gamespy qr: darray and hashtable", scaled(400), 0x5c9ac0u, [](int k) { array_case(k); });
    cases("gamespy qr: wsock32 thunks, msleep, SocketStartUp, SocketShutDown", scaled(20), 0x5ca9e0u, [](int k) { thunk_case(k); });
    uninstall();
}

}  // namespace zax
