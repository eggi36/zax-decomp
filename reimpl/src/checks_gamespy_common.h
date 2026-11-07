// Shared parts of the GameSpy checks (checks_gamespy_gcd.cpp, checks_gamespy_qr.cpp, checks_gamespy_sb.cpp): every
// function against its original through the trampoline with all hooks staying in (side 0 calls the trampoline,
// side 1 the exe address, which jumps into the reimplementation; an original that calls another SDK function gets
// that function's reimplementation, which is checked on its own), in the game thread, without any network:
//  - Winsock: the 20 wsock32 import entries 0x5ea2a4-0x5ea2f0 point to recorders for the time of the module (only
//    the SDK uses them). Sockets are numbers 0x1000 + n; received data comes from a script (UDP packets queued per
//    socket, TCP chunks), select reports the sockets with queued data (and keeps only those in the set, as Winsock
//    does), the key server and the queried servers answer the requests sent to them (a reply queued on sendto);
//    sent data is logged by content, addresses by content, buffers by kind (null, stack, heap).
//  - GetTickCount (import 0x5ea0e8, behind the game's wrapper 0x4d2460) and Sleep (0x5ea07c): a scripted clock for
//    the check thread, every other thread gets the real functions; rand 0x5d2198, srand 0x5d218b and time 0x5d20af
//    jump to recorders (only the SDK and the start-up call rand/srand).
//  - The SDK's callbacks (CD key result, the four query answers, the server list's callback) are recorders.
// A budget ends runaway loops: after it, select fails and recv returns 0, so every synchronous loop of the SDK ends.
// The SDK's globals (0x627d30, 0x627f88-0x62807c, 0x714588-0x715730) are saved before the module and put back
// before each side and at the end; the state compared after each side is walked field by field (texts by content,
// heap pointers by what they point to).
#pragma once
#include "checks_net_common.h"
#include "gamespy.h"

namespace zax {
namespace gscheck {

using namespace netcheck;
using namespace gamespy;

inline DWORD g_tid;
inline DWORD tid()
{
    DWORD t;
    __asm__("movl %%fs:0x24, %0" : "=r"(t));
    return t;
}
// A pointer's kind for the log: null 0, the check thread's stack 1, anything else 2 (arena and exe addresses as they are)
inline DWORD kind(const void *p)
{
    DWORD base, limit;
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(base), "=r"(limit));
    const DWORD v = ptrv(p);
    if (!p)
        return 0;
    if (in_arena(v) || (v >= 0x400000 && v < IMAGE_END))
        return v;
    return v >= limit && v < base ? 1 : 2;
}
inline unsigned hash_bytes(const void *p, DWORD n) { return p ? fnv(2166136261u, p, n) : 0xdead0003u; }
inline unsigned hash_text(const char *s) { return s ? fnv(2166136261u, s, slen(s) + 1) : 0xdead0004u; }

// The function through the trampoline (side 0) or through the hooked exe address (side 1)
template <typename F> inline F side_fn(DWORD a) { return g_side == 0 ? original<F>(a) : at<F>(a); }

// What the cases reached (side 1), logged per check: packets sent and received, TCP reads with data, callbacks
// by kind (the check's own counters, see count())
inline int g_count[8];
inline void count(int i)
{
    if (g_side == 1)
        g_count[i]++;
}

// ---- Script ---------------------------------------------------------------------------------------------------

constexpr int NPACKET = 96;
struct Packet {
    DWORD sock;      // 0: any socket
    int length;      // -1: recvfrom fails
    SockAddr from;
    char data[0x300];
};
inline Packet g_packets[NPACKET];
inline int g_npackets;
inline DWORD g_nsockets;
inline DWORD g_clock, g_clock_step, g_time;
inline unsigned g_rand;
inline int g_calls, g_budget;
// Failing calls by number: bit n of a mask = the n-th call of that function fails (all bits: every call)
inline DWORD g_fail_socket, g_fail_bind, g_fail_connect, g_fail_send, g_fail_recv, g_fail_setsockopt, g_fail_select;
inline int g_nsocket_calls, g_nbind_calls, g_nconnect_calls, g_nsend_calls, g_nrecv_calls, g_nsetsockopt_calls, g_nselect_calls;
inline int g_getsockname_result;
inline WORD g_local_port;
inline bool g_host_found;
inline DWORD g_tcp_socket;       // the connected (TCP) socket: always readable (its end reads 0)
inline DWORD g_gcd_socket;       // replies to "\auth\" requests are queued for this socket
inline int g_gcd_reply;          // 0 none, 1 uok, 2 unok, 3 uok with a wrong skey, 4 random
inline int g_server_replies;     // queried servers answer (0 never ... 3 always)
// TCP: the master's challenge and the list in chunks
inline char g_tcp[0x1000];
inline int g_tcp_length, g_tcp_pos, g_tcp_chunk;

inline void reset_script()
{
    g_npackets = 0;
    g_nsockets = 0;
    g_clock = 100000;
    g_clock_step = 7;
    g_time = 0x3a5b0000;
    g_rand = 1;
    g_calls = 0;
    g_budget = 4000;
    g_fail_socket = g_fail_bind = g_fail_connect = g_fail_send = g_fail_recv = g_fail_setsockopt = g_fail_select = 0;
    g_nsocket_calls = g_nbind_calls = g_nconnect_calls = g_nsend_calls = g_nrecv_calls = g_nsetsockopt_calls = g_nselect_calls = 0;
    g_getsockname_result = 0;
    g_local_port = 0x3412;
    g_host_found = true;
    g_gcd_socket = 0;
    g_tcp_socket = 0xffffffffu;
    g_gcd_reply = 0;
    g_server_replies = 0;
    g_tcp_length = g_tcp_pos = 0;
    g_tcp_chunk = 64;
}
inline bool fails(DWORD mask, int &n)
{
    const int i = n++;
    if (mask == 0xffffffffu)
        return true;   // every call
    return i < 32 && (mask >> i & 1);
}
inline bool over_budget() { return ++g_calls > g_budget; }
// The clock set so that the next GetTickCount returns `base + offset` (the boundaries of the SDK's timers)
inline void clock_at(DWORD base, DWORD offset) { g_clock = base + offset - g_clock_step; }

inline Packet *add_packet(DWORD sock, const char *text, int length = -2)
{
    if (g_npackets >= NPACKET)
        return nullptr;
    Packet &p = g_packets[g_npackets++];
    p.sock = sock;
    p.from.family = 2;
    p.from.port = 0x5c6f;
    p.from.addr = 0x0b00000a;
    memset(p.from.zero, 0, 8);
    if (length == -1) {
        p.length = -1;
        p.data[0] = 0;
        return &p;
    }
    const int n = length >= 0 ? length : (int)slen(text);
    memcpy(p.data, text, (DWORD)n);
    p.data[n] = 0;
    p.length = n;
    return &p;
}
inline Packet *find_packet(DWORD sock)
{
    for (int i = 0; i < g_npackets; i++)
        if (g_packets[i].sock == sock || g_packets[i].sock == 0)
            return &g_packets[i];
    return nullptr;
}
inline void drop_packet(Packet *p)
{
    const int i = (int)(p - g_packets);
    for (int j = i; j + 1 < g_npackets; j++)
        g_packets[j] = g_packets[j + 1];
    g_npackets--;
}

// Random text parts
inline const char *const WORDS[] = {"hostname", "Zax Server", "numplayers", "3", "maxplayers", "16", "gamemode", "openplaying",
                                    "mapname", "Nachbau1", "player_0", "Crater", "team_t0", "Red", "x", "", "skey", "cd",
                                    "queryid", "final", "ping", "hostaddr", "other", "ip", "secure", "echo"};
inline void append(char *out, DWORD cap, const char *s)
{
    DWORD n = slen(out);
    while (*s && n + 1 < cap)
        out[n++] = *s++;
    out[n] = 0;
}
inline void append_number(char *out, DWORD cap, DWORD v)
{
    char t[12];
    wsprintfA(t, "%u", v);
    append(out, cap, t);
}
inline const char *word() { return WORDS[rnd(sizeof WORDS / sizeof WORDS[0])]; }
// "\k\v\k\v..." with n pairs
inline void key_values(char *out, DWORD cap, int n)
{
    for (int i = 0; i < n; i++) {
        append(out, cap, "\\");
        append(out, cap, word());
        append(out, cap, "\\");
        if (rnd(4) == 0)
            append_number(out, cap, rnd(100000));
        else
            append(out, cap, word());
    }
}

// ---- Recorders of the wsock32 imports -----------------------------------------------------------------------------

inline void log_address(DWORD id, const void *a, int n)
{
    if (!a) {
        ev_raw(id, 0xdead0005, 0, (DWORD)n);
        return;
    }
    const SockAddr *s = (const SockAddr *)a;
    ev_raw(id, (DWORD)s->family | (DWORD)s->port << 16, s->addr, (DWORD)n);
}

inline DWORD WINAPI f_socket(int af, int type, int proto)
{
    ev_raw(0x200, (DWORD)af, (DWORD)type, (DWORD)proto);
    over_budget();
    if (fails(g_fail_socket, g_nsocket_calls))
        return 0xffffffffu;
    return 0x1000 + g_nsockets++;
}
inline int WINAPI f_bind(DWORD s, const SockAddr *a, int n)
{
    ev_raw(0x201, s);
    log_address(0x301, a, n);
    return fails(g_fail_bind, g_nbind_calls) ? -1 : 0;
}
inline int WINAPI f_getsockname(DWORD s, SockAddr *a, int *n)
{
    ev_raw(0x202, s, n ? (DWORD)*n : 0xdead0006, kind(a) == 1);
    a->family = 2;
    a->port = g_local_port;
    a->addr = 0;
    return g_getsockname_result;
}
inline WORD swap16(DWORD v) { return (WORD)((v & 0xff) << 8 | (v >> 8 & 0xff)); }
inline WORD WINAPI f_htons(DWORD v)
{
    ev_raw(0x203, v);
    return swap16(v);
}
inline WORD WINAPI f_ntohs(DWORD v)
{
    ev_raw(0x204, v);
    return swap16(v);
}
inline DWORD WINAPI f_htonl(DWORD v)
{
    ev_raw(0x205, v);
    return v >> 24 | (v >> 8 & 0xff00) | (v << 8 & 0xff0000) | v << 24;
}
// Dotted quads parse (in network order), anything else is INADDR_NONE
inline DWORD WINAPI f_inet_addr(const char *s)
{
    ev_raw(0x206, hash_text(s));
    DWORD v = 0, part = 0;
    int parts = 0, digits = 0;
    for (const char *p = s;; p++) {
        if (*p >= '0' && *p <= '9') {
            part = part * 10 + (DWORD)(*p - '0');
            digits++;
        } else if (*p == '.' || *p == 0) {
            if (!digits || part > 255 || parts > 3)
                return 0xffffffffu;
            v |= part << (8 * parts++);
            part = 0;
            digits = 0;
            if (!*p)
                break;
        } else {
            return 0xffffffffu;
        }
    }
    return parts == 4 ? v : 0xffffffffu;
}
inline HostEnt g_host;
inline char *g_host_list[2];
inline DWORD g_host_address = 0x0c00000a;
inline HostEnt *WINAPI f_gethostbyname(const char *name)
{
    ev_raw(0x207, hash_text(name));
    if (!g_host_found)
        return nullptr;
    g_host.addresses = g_host_list;
    g_host_list[0] = (char *)&g_host_address;
    g_host_list[1] = nullptr;
    return &g_host;
}
inline char g_ntoa[24];
inline char *WINAPI f_inet_ntoa(DWORD a)
{
    ev_raw(0x208, a);
    wsprintfA(g_ntoa, "%u.%u.%u.%u", a & 0xff, a >> 8 & 0xff, a >> 16 & 0xff, a >> 24);
    return g_ntoa;
}
inline int WINAPI f_closesocket(DWORD s)
{
    ev_raw(0x209, s);
    return 0;
}

// The key server's reply to an "\auth\" request (xored with "gamespy"): uok/unok for its CD key and skey
inline void xor_gamespy(char *b, int n)
{
    static const char key[] = "gamespy";
    for (int i = 0, k = 0; i < n; i++) {
        b[i] ^= key[k];
        k = key[k + 1] ? k + 1 : 0;
    }
}
inline const char *find_value(const char *msg, const char *key, char *out, DWORD cap)
{
    char search[40] = "\\";
    append(search, sizeof search, key);
    append(search, sizeof search, "\\");
    out[0] = 0;
    for (const char *p = msg; *p; p++) {
        DWORD i = 0;
        while (search[i] && p[i] == search[i])
            i++;
        if (!search[i]) {
            p += i;
            DWORD n = 0;
            while (p[n] && p[n] != '\\' && n + 1 < cap)
                out[n] = p[n], n++;
            out[n] = 0;
            return out;
        }
    }
    return out;
}
inline void gcd_reply(const char *buf, int len)
{
    if (g_gcd_reply == 0 || len <= 0 || len > 0x2f0)
        return;
    char msg[0x300];
    memcpy(msg, buf, (DWORD)len);
    msg[len] = 0;
    xor_gamespy(msg, len);
    if (memcmp(msg, "\\auth\\", 6) != 0)
        return;
    char resp[0x100], skey[16], cd[0x40];
    find_value(msg, "resp", resp, sizeof resp);
    find_value(msg, "skey", skey, sizeof skey);
    memcpy(cd, resp, 32);
    cd[32] = 0;
    int r = g_gcd_reply == 4 ? (int)rnd(3) + 1 : g_gcd_reply;
    char reply[0x200] = "";
    append(reply, sizeof reply, r == 2 ? "\\unok\\" : "\\uok\\");
    append(reply, sizeof reply, "\\cd\\");
    append(reply, sizeof reply, cd);
    append(reply, sizeof reply, "\\skey\\");
    append(reply, sizeof reply, r == 3 ? "17" : skey);
    if (r == 2)
        append(reply, sizeof reply, "\\errmsg\\Invalid CD Key");
    const int n = (int)slen(reply);
    xor_gamespy(reply, n);
    add_packet(g_gcd_socket, reply, n);
}
// A queried server's answer: key/value pairs, sometimes split, sometimes from another address, mostly ending "\final\"
inline void server_reply(DWORD s, const SockAddr *to)
{
    if (!to || g_server_replies == 0 || rnd(4) >= (DWORD)g_server_replies + 1)
        return;
    const int parts = 1 + (int)rnd(2);
    for (int i = 0; i < parts; i++) {
        char text[0x200] = "";
        key_values(text, sizeof text, 1 + (int)rnd(6));
        if (i == parts - 1 && rnd(5) != 0)
            append(text, sizeof text, "\\final\\");
        append(text, sizeof text, "\\queryid\\1.1");
        Packet *p = add_packet(s, text, rnd(12) == 0 ? -1 : -2);
        if (p) {
            p->from = *to;
            if (rnd(10) == 0)
                p->from.port ^= 0x100;
        }
    }
}
inline int WINAPI f_sendto(DWORD s, const char *buf, int len, int flags, const SockAddr *to, int tolen)
{
    ev_raw(0x20a, s, (DWORD)len, (DWORD)flags);
    ev_raw(0x30a, hash_bytes(buf, len > 0 ? (DWORD)len : 0), kind(buf) ? 1 : 0);
    log_address(0x40a, to, tolen);
    over_budget();
    count(0);
    if (s == g_gcd_socket)
        gcd_reply(buf, len);
    else if (g_calls < g_budget)
        server_reply(s, to);
    return len;
}
inline int WINAPI f_recvfrom(DWORD s, char *buf, int len, int flags, SockAddr *from, int *fromlen)
{
    ev_raw(0x20b, s, (DWORD)len, (DWORD)flags);
    ev_raw(0x30b, fromlen ? (DWORD)*fromlen : 0xdead0007, kind(buf) == 1, kind(from) == 1);
    Packet *p = find_packet(s);
    if (!p || p->length < 0) {
        if (p) {
            // ServerListQueryLoop compares the sender even after a failed receive (its local is not written then):
            // the stand-in writes the script's sender, so both sides compare the same bytes
            if (from)
                *from = p->from;
            drop_packet(p);
        }
        return -1;
    }
    int n = p->length < len ? p->length : len;
    memcpy(buf, p->data, (DWORD)n);
    if (n < len)
        buf[n] = 0;   // deterministic bytes behind the packet (the LAN list's strstr reads them)
    if (from)
        *from = p->from;
    drop_packet(p);
    count(1);
    return n;
}
inline bool ready(DWORD s) { return find_packet(s) != nullptr; }
inline int WINAPI f_select(int n, FdSet *r, FdSet *w, FdSet *e, const TimeVal *t)
{
    ev_raw(0x20c, (DWORD)n, r ? r->count : 0xdead0008, (w ? 1u : 0u) | (e ? 2u : 0u));
    ev_raw(0x30c, r ? hash_bytes(r->sockets, r->count * 4) : 0, t ? t->sec : 0xdead0009, t ? t->usec : 0);
    bool tcp = false;
    for (DWORD i = 0; r && i < r->count && i < 64; i++)
        tcp |= r->sockets[i] == g_tcp_socket;
    if ((over_budget() && !tcp) || fails(g_fail_select, g_nselect_calls))
        return -1;
    if (!r)
        return 0;
    DWORD k = 0;
    for (DWORD i = 0; i < r->count && i < 64; i++)
        if (ready(r->sockets[i]) || r->sockets[i] == g_tcp_socket)
            r->sockets[k++] = r->sockets[i];
    r->count = k;
    return (int)k;
}
inline int WINAPI f_fdisset(DWORD s, FdSet *set)
{
    ev_raw(0x20d, s, set ? set->count : 0xdead000a);
    for (DWORD i = 0; set && i < set->count; i++)
        if (set->sockets[i] == s)
            return 1;
    return 0;
}
inline int WINAPI f_send(DWORD s, const char *buf, int len, int flags)
{
    ev_raw(0x20e, s, (DWORD)len, (DWORD)flags);
    ev_raw(0x30e, hash_bytes(buf, len > 0 ? (DWORD)len : 0));
    if (fails(g_fail_send, g_nsend_calls))
        return rnd(2) ? -1 : 0;
    return len;
}
inline int WINAPI f_recv(DWORD s, char *buf, int len, int flags)
{
    ev_raw(0x20f, s, (DWORD)len, (DWORD)flags);
    if (over_budget() || fails(g_fail_recv, g_nrecv_calls))
        return rnd(2) ? -1 : 0;
    const int left = g_tcp_length - g_tcp_pos;
    if (left <= 0)
        return 0;
    int n = left < g_tcp_chunk ? left : g_tcp_chunk;
    if (n > len)
        n = len;
    memcpy(buf, g_tcp + g_tcp_pos, (DWORD)n);
    g_tcp_pos += n;
    count(2);
    return n;
}
inline int WINAPI f_connect(DWORD s, const SockAddr *a, int n)
{
    ev_raw(0x210, s);
    log_address(0x310, a, n);
    g_tcp_socket = s;
    return fails(g_fail_connect, g_nconnect_calls) ? -1 : 0;
}
inline int WINAPI f_setsockopt(DWORD s, int level, int opt, const char *v, int n)
{
    ev_raw(0x211, s, (DWORD)level, (DWORD)opt);
    ev_raw(0x311, v ? *(const DWORD *)v : 0xdead000b, (DWORD)n);
    return fails(g_fail_setsockopt, g_nsetsockopt_calls) ? -1 : 0;
}
inline int WINAPI f_wsastartup(DWORD version, void *data)
{
    ev_raw(0x212, version, kind(data) == 1);
    return 0;
}
inline int WINAPI f_wsacleanup()
{
    ev_raw(0x213);
    return 0;
}

// ---- Clock, Sleep and the C runtime's random numbers ------------------------------------------------------------

inline DWORD g_real_tick, g_real_sleep;
inline DWORD WINAPI f_tick()
{
    if (tid() != g_tid)
        return ((DWORD(WINAPI *)())(uintptr_t)g_real_tick)();
    ev_raw(0x220);
    return g_clock += g_clock_step;
}
inline void WINAPI f_sleep(DWORD ms)
{
    if (tid() != g_tid) {
        ((void(WINAPI *)(DWORD))(uintptr_t)g_real_sleep)(ms);
        return;
    }
    ev_raw(0x221, ms);
    g_clock += ms;
}
inline int __cdecl f_rand()
{
    ev_raw(0x222);
    g_rand = g_rand * 214013u + 2531011u;
    return (int)(g_rand >> 16 & 0x7fff);
}
inline void __cdecl f_srand(DWORD seed)
{
    ev_raw(0x223, seed);
    g_rand = seed;
}
inline DWORD __cdecl f_time(DWORD *t)
{
    ev_raw(0x224, kind(t));
    if (t)
        *t = g_time;
    return g_time;
}

// ---- Installing the stand-ins and saving the SDK's globals ----------------------------------------------------------

struct Stand {
    ImportPatch imports[22];
    CodePatch code[3];
};
inline Stand g_stand;
inline BYTE g_saved_region[0x11a8], g_saved_record[0xf8];
inline DWORD g_saved_socket;

inline void save_globals()
{
    memcpy(g_saved_region, at<void *>(0x714588), sizeof g_saved_region);
    memcpy(g_saved_record, at<void *>(0x627f88), sizeof g_saved_record);
    g_saved_socket = *at<DWORD *>(0x627d30);
}
inline void restore_globals()
{
    memcpy(at<void *>(0x714588), g_saved_region, sizeof g_saved_region);
    memcpy(at<void *>(0x627f88), g_saved_record, sizeof g_saved_record);
    *at<DWORD *>(0x627d30) = g_saved_socket;
}

inline void install()
{
    g_tid = tid();
    save_globals();
    g_real_tick = *at<DWORD *>(0x5ea0e8);
    g_real_sleep = *at<DWORD *>(0x5ea07c);
    struct Entry {
        DWORD slot;
        void *fake;
    };
    const Entry list[] = {
        {0x5ea2e0, (void *)&f_gethostbyname}, {0x5ea2d0, (void *)&f_inet_addr}, {0x5ea2d4, (void *)&f_getsockname},
        {0x5ea2d8, (void *)&f_bind}, {0x5ea2dc, (void *)&f_htons}, {0x5ea2a4, (void *)&f_socket},
        {0x5ea2a8, (void *)&f_closesocket}, {0x5ea2ac, (void *)&f_sendto}, {0x5ea2b0, (void *)&f_recvfrom},
        {0x5ea2b4, (void *)&f_select}, {0x5ea2b8, (void *)&f_send}, {0x5ea2bc, (void *)&f_recv},
        {0x5ea2c0, (void *)&f_connect}, {0x5ea2c4, (void *)&f_ntohs}, {0x5ea2c8, (void *)&f_setsockopt},
        {0x5ea2cc, (void *)&f_fdisset}, {0x5ea2ec, (void *)&f_inet_ntoa}, {0x5ea2e4, (void *)&f_htonl},
        {0x5ea2e8, (void *)&f_wsastartup}, {0x5ea2f0, (void *)&f_wsacleanup}, {0x5ea0e8, (void *)&f_tick},
        {0x5ea07c, (void *)&f_sleep},
    };
    for (int i = 0; i < 22; i++)
        patch_import(g_stand.imports[i], list[i].slot, list[i].fake);
    patch_jump(g_stand.code[0], 0x5d2198, (void *)&f_rand);
    patch_jump(g_stand.code[1], 0x5d218b, (void *)&f_srand);
    patch_jump(g_stand.code[2], 0x5d20af, (void *)&f_time);
}
inline void uninstall()
{
    for (int i = 2; i >= 0; i--)
        unpatch(g_stand.code[i]);
    for (int i = 21; i >= 0; i--)
        unpatch_import(g_stand.imports[i]);
    restore_globals();
}

// One case on both sides: the globals put back, the arena and the script reset, the same seed
template <typename F> bool both(unsigned seed, F run)
{
    for (int side = 0; side < 2; side++) {
        begin_side(side, seed);
        g_arena_used = 0;
        restore_globals();
        reset_script();
        run();
    }
    restore_globals();
    return sides_equal();
}
template <typename F> void cases(const char *what, int n, unsigned seed0, F run)
{
    Tally t{what};
    memset(g_count, 0, sizeof g_count);
    for (int k = 0; k < n; k++) {
        const bool same = both(seed0 + (unsigned)k * 7919u, [&] { run(k); });
        char d[64];
        wsprintfA(d, "[case %d]", k);
        t.check(same, "", d);
        if (!same && t.mismatches <= 3)
            log_first_difference();
    }
    log("    reached: %d sent, %d received, %d TCP reads, callbacks %d/%d/%d/%d/%d", g_count[0], g_count[1], g_count[2],
        g_count[3], g_count[4], g_count[5], g_count[6], g_count[7]);
    t.report();
}

}  // namespace gscheck
}  // namespace zax
