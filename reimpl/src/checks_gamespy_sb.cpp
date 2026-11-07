// Checks of GameSpy server browsing (gamespy_sb.cpp) against the original through the trampolines, the hooks
// staying in (checks_gamespy_common.h). Cases: ServerListNew/Free; ServerListUpdate synchronous and asynchronous
// (stepped with ServerListThink) against a scripted master (connect, the "\secure\" challenge or none, failing sends,
// the binary list in chunks of every size with servers already known, the closing "\final\" or a broken connection)
// and scripted servers answering the queries (split answers, other senders, failed receives, timeouts on the
// scripted clock), ServerListHalt on the way; ServerListLANUpdate (both ways, port ranges and steps, answers with and
// without "\final\"); ServerListReadList and ServerListParseInfoList on groups/info2 field lists (count, names,
// records with "ip" and "other"); ServerListClear; the servers' functions directly (ServerNew, ServerNewData,
// ServerParseKeyVals, mytok, LookupKey, the getters, AddServer, FindServer, AddServerData, the slash count, the hash
// table callbacks). Compared: results, the call log and the list walked (servers with their keys and values by text,
// the key strings, the query sockets, the read buffer and the field list).
#include "checks_gamespy_common.h"

namespace zax {

namespace {

using namespace gscheck;

struct Server {
    DWORD ip;
    WORD port;
    short ping;
    int query_type;
    HashTable *keyvals;
    HashTable *keylist;
};
struct KeyValue {
    char *key;
    char *value;
};
struct QuerySocket {
    DWORD s;
    Server *server;
    DWORD start;
    SockAddr address;
};
struct ServerList {
    int state;
    DArray *servers;
    QuerySocket *sockets;
    char names[0x60];
    int nsockets;
    int next;
    int abort;
    void *callback;
    void *instance;
    char *sort_key;
    DWORD unknown_84;
    DWORD sock;
    DWORD lan_start;
    int query_type;
    HashTable *keylist;
    int start_count;
    int list_state;
};

ServerList *g_list;
int g_halt_at;   // the callback calls ServerListHalt at this server message (0: never)
int g_messages;

int server_index(const void *s)
{
    if (!g_list || !s)
        return -1;
    for (int i = 0; i < g_list->servers->count; i++)
        if (*(void **)(g_list->servers->list + 4 * i) == s)
            return i;
    return -2;
}
void __cdecl list_cb(ServerList *l, int msg, void *instance, void *a, void *b)
{
    ev_raw(0x240, l == g_list, (DWORD)msg, ptrv(instance));
    count(msg == 2 ? 4 : 3);   // state changes, servers
    if (msg == 2) {
        const Server *s = (const Server *)a;
        ev_raw(0x340, (DWORD)server_index(a), s ? s->ip : 0, s ? s->port : 0);
        ev_raw(0x440, ptrv(b), (DWORD)l->state);
        if (++g_messages == g_halt_at)
            side_fn<int(__cdecl *)(ServerList *)>(0x5c7850)(l);
    } else {
        ev_raw(0x340, ptrv(a), ptrv(b), (DWORD)l->state);
    }
}

void st_strings(const HashTable *t)
{
    st_raw((DWORD)t->nbuckets);
    st_raw(ptrv((void *)t->hash_fn));
    st_raw(ptrv((void *)t->compare_fn));
    st_raw(ptrv((void *)t->free_fn));
    for (int b = 0; b < t->nbuckets; b++) {
        const DArray *a = t->buckets[b];
        st_raw((DWORD)a->count);
        st_raw((DWORD)a->capacity);
        for (int i = 0; i < a->count; i++)
            st_text(*(char **)(a->list + 4 * i));
    }
}
void st_server(const Server *s)
{
    st_raw(s->ip);
    st_raw(s->port);
    st_raw((DWORD)(WORD)s->ping);
    st_raw((DWORD)s->query_type);
    st_raw(g_list && s->keylist == g_list->keylist);
    const HashTable *t = s->keyvals;
    st_raw((DWORD)t->nbuckets);
    st_raw(ptrv((void *)t->hash_fn));
    st_raw(ptrv((void *)t->compare_fn));
    st_raw(ptrv((void *)t->free_fn));
    for (int b = 0; b < t->nbuckets; b++) {
        const DArray *a = t->buckets[b];
        st_raw((DWORD)a->count);
        st_raw((DWORD)a->capacity);
        for (int i = 0; i < a->count; i++) {
            const KeyValue *kv = (const KeyValue *)(a->list + 8 * i);
            st_text(kv->key);
            st_text(kv->value);
        }
    }
}
void st_globals()
{
    const int fill = *at<int *>(0x715410);
    st_raw((DWORD)fill);
    if (fill > 0 && fill <= 0x800)
        st_bytes(at<void *>(0x714bbc), (DWORD)fill);
    st_raw(*at<DWORD *>(0x7153bc));
    for (int i = 0; i < 20; i++)
        st_raw(at<DWORD *>(0x7153c0)[i] != 0);
    const DWORD tok = *at<DWORD *>(0x715414);
    st_raw(tok >= 0x714bbc && tok < 0x7153bc ? tok - 0x714bbc : kind((void *)(uintptr_t)tok));
}
void st_list(const ServerList *l)
{
    st_raw((DWORD)l->state);
    st_text(l->names);          // the names' buffers hold the texts only (the rest as malloc left it)
    st_text(l->names + 0x20);
    st_text(l->names + 0x40);
    st_raw((DWORD)l->nsockets);
    st_raw((DWORD)l->next);
    st_raw((DWORD)l->abort);
    st_raw(ptrv(l->callback));
    st_raw(ptrv(l->instance));
    st_raw(ptrv(l->sort_key));
    st_raw(l->sock);
    st_raw(l->lan_start);
    st_raw((DWORD)l->query_type);
    st_raw((DWORD)l->start_count);
    st_raw((DWORD)l->list_state);
    const DArray *a = l->servers;
    st_raw((DWORD)a->count);
    st_raw((DWORD)a->capacity);
    st_raw((DWORD)a->elem_size);
    st_raw((DWORD)a->grow_by);
    st_raw(ptrv((void *)a->free_fn));
    for (int i = 0; i < a->count; i++)
        st_server(*(Server **)(a->list + 4 * i));
    for (int i = 0; i < l->nsockets; i++) {
        const QuerySocket &q = l->sockets[i];
        st_raw(q.s);
        st_raw((DWORD)server_index(q.server));
        st_raw(q.start);
        st_bytes(&q.address, 8);
    }
    st_strings(l->keylist);
    st_globals();
}

ServerList *new_list(int k)
{
    const int updates = 1 + (int)rnd(6);
    ServerList *l = side_fn<ServerList *(__cdecl *)(const char *, const char *, const char *, int, void *, int, void *)>(0x5c6060)(
        "Zax", "zax", k % 3 ? "J3An3s" : "abcdefgh", updates, (void *)&list_cb, 1, (void *)(uintptr_t)(0x5500 + k));
    g_list = l;
    // the fields ServerListNew leaves as malloc had them, the same on both sides
    l->next = l->abort = 0x3c3c3c3c;
    l->unknown_84 = l->sock = l->lan_start = 0x3c3c3c3c;
    l->query_type = l->list_state = 0x3c3c3c3c;
    st_list(l);
    return l;
}
void free_list(ServerList *l)
{
    side_fn<void(__cdecl *)(ServerList *)>(0x5c6180)(l);
    g_list = nullptr;
}

// The master's TCP data: the challenge, then the list (6-byte records or a field list), "\final\"
void put_tcp(const void *p, int n)
{
    if (g_tcp_length + n > (int)sizeof g_tcp - 1)
        return;
    memcpy(g_tcp + g_tcp_length, p, (DWORD)n);
    g_tcp_length += n;
    g_tcp[g_tcp_length] = 0;
}
void put_text(const char *s) { put_tcp(s, (int)slen(s)); }
void master_list(int records)
{
    for (int i = 0; i < records; i++) {
        BYTE r[6];
        const DWORD ip = rnd(3) ? 0x0a000000u + rnd(4) * 0x1000000u + 0x01 : random();
        memcpy(r, &ip, 4);
        const WORD port = swap16(26794 + rnd(3));
        memcpy(r + 4, &port, 2);
        put_tcp(r, 6);
    }
}
void field_list(int qtype)
{
    const char *const NAMES[] = {"ip", "hostname", "numplayers", "other", "gametype", "mapname"};
    const int n = 1 + (int)rnd(6);
    char t[0x200] = "\\fieldcount\\";
    append_number(t, sizeof t, rnd(15) == 0 ? 25 : (DWORD)n);
    append(t, sizeof t, "\\");
    put_text(t);
    for (int i = 0; i < n; i++) {
        t[0] = 0;
        append(t, sizeof t, "\\");
        append(t, sizeof t, NAMES[i]);
        put_text(t);
    }
    const int records = (int)rnd(5);
    for (int r = 0; r < records; r++) {
        for (int i = 0; i < n; i++) {
            t[0] = 0;
            append(t, sizeof t, "\\");
            if (i == 0) {
                append(t, sizeof t, rnd(4) ? "10.0.0.7:26794" : "badaddress");
            } else if (i == 3 && qtype == 6) {
                append(t, sizeof t, "\x01k1\x01v1\x01queryid\x01q\x01k2\x01v2");
            } else {
                append(t, sizeof t, word());
            }
            put_text(t);
        }
    }
    put_text("\\");
}

void master_script(bool groups)
{
    g_tcp_length = g_tcp_pos = 0;
    if (rnd(8))
        put_text("\\basic\\\\secure\\TXKOAT");
    else
        put_text("\\basic\\nothing");
    // the challenge's recv (at most 0xff bytes) mostly gets the challenge alone, the list follows in later reads
    g_tcp_chunk = rnd(5) ? g_tcp_length : 300;
    if (groups)
        field_list((int)rnd(2) + 6);
    else
        master_list((int)rnd(12));
    if (rnd(6))
        put_text("\\final\\");
}

void update_case(int k)
{
    ServerList *l = new_list(k);
    g_clock_step = 1 + rnd(60);
    g_server_replies = (int)rnd(4);
    g_halt_at = rnd(6) == 0 ? 1 + (int)rnd(4) : 0;
    g_messages = 0;
    g_host_found = rnd(8) != 0;
    if (rnd(10) == 0)
        g_fail_socket = 1u << rnd(3);
    if (rnd(10) == 0)
        g_fail_connect = 1;
    if (rnd(10) == 0)
        g_fail_send = 1u << rnd(2);
    if (rnd(12) == 0)
        g_fail_recv = 1u << rnd(4);
    master_script(false);
    const bool async = rnd(2) != 0;
    const int r = side_fn<int(__cdecl *)(ServerList *, int)>(0x5c61c0)(l, async);
    ev_raw(0x140, (DWORD)r);
    // the chunk size of the list's reads after the challenge
    g_tcp_chunk = 1 + (int)rnd(40);
    if (async) {
        for (int i = 0; i < 400 && l->state != 0; i++) {
            const int t = side_fn<int(__cdecl *)(ServerList *)>(0x5c74c0)(l);
            ev_raw(0x141, (DWORD)t, (DWORD)l->state);
        }
    }
    st_list(l);
    if (rnd(3) == 0) {
        const int c = side_fn<int(__cdecl *)(ServerList *)>(0x5c7870)(l);
        ev_raw(0x142, (DWORD)c);
        st_list(l);
    }
    free_list(l);
}

void lan_case(int k)
{
    ServerList *l = new_list(k);
    g_clock_step = 1 + rnd(80);
    g_server_replies = (int)rnd(4);
    g_halt_at = rnd(8) == 0 ? 1 : 0;
    g_messages = 0;
    if (rnd(10) == 0)
        g_fail_socket = 1u << rnd(4);
    if (rnd(10) == 0)
        g_fail_setsockopt = 1;
    const DWORD lan = 0x1000 + (DWORD)l->nsockets;
    const int answers = (int)rnd(6);
    for (int i = 0; i < answers; i++) {
        char t[0x100] = "";
        key_values(t, sizeof t, 1 + (int)rnd(4));
        if (rnd(5))
            append(t, sizeof t, "\\final\\");
        Packet *p = add_packet(lan, t, rnd(10) == 0 ? -1 : -2);
        if (p) {
            p->from.addr = 0x0a000000u + rnd(3) * 0x1000000u + 0x02;
            p->from.port = swap16(26794 + rnd(3));
        }
    }
    const bool async = rnd(2) != 0;
    const int start = 26794 + (int)rnd(3), end = start + (int)rnd(12) - 1, step = 1 + (int)rnd(3);
    const int r = side_fn<int(__cdecl *)(ServerList *, int, int, int, int)>(0x5c64b0)(l, async, start, end, step);
    ev_raw(0x150, (DWORD)r);
    if (async)
        for (int i = 0; i < 400 && l->state != 0; i++) {
            if (l->state == 2 && rnd(4) == 0)   // the LAN wait's boundary (3000 ms)
                clock_at(l->lan_start, 2999 + rnd(3));
            const int t = side_fn<int(__cdecl *)(ServerList *)>(0x5c74c0)(l);
            ev_raw(0x151, (DWORD)t, (DWORD)l->state);
        }
    st_list(l);
    free_list(l);
}

// ServerListReadList and ServerListParseInfoList on a field list, and the six-byte list with known servers
void read_case(int k)
{
    ServerList *l = new_list(k);
    const int qt = rnd(3) == 0 ? 5 : 6 + (int)rnd(2);
    l->query_type = qt;
    l->list_state = 0;
    l->next = l->abort = 0;   // as ServerListUpdate sets them
    l->state = 1;
    l->sock = 0x1100;
    g_tcp_socket = 0x1100;
    g_tcp_length = g_tcp_pos = 0;
    g_tcp_chunk = 1 + (int)rnd(80);
    if (qt == 5) {
        master_list(2);
        // two servers known before
        const BYTE *rec = (const BYTE *)g_tcp;
        for (int i = 0; i < 2; i++) {
            const DWORD ip = *(const DWORD *)(rec + 6 * i);
            const WORD port = swap16(*(const WORD *)(rec + 6 * i + 4));
            side_fn<void *(__cdecl *)(ServerList *, DWORD, DWORD, int)>(0x5c6870)(l, ip, port, 5);
        }
        l->start_count = l->servers->count;
        master_list((int)rnd(6));
    } else {
        field_list(qt);
    }
    if (rnd(6))
        put_text("\\final\\");
    if (rnd(10) == 0)
        g_fail_recv = 1u << rnd(6);
    for (int i = 0; i < 80 && l->state == 1; i++) {
        const int r = rnd(3) ? side_fn<int(__cdecl *)(ServerList *)>(0x5c6c80)(l) : side_fn<int(__cdecl *)(ServerList *)>(0x5c74c0)(l);
        ev_raw(0x160, (DWORD)r);
    }
    st_list(l);
    // ParseInfoList directly on a buffer of our own
    char buf[0x200];
    l->list_state = (int)rnd(3);
    const int fields = 1 + (int)rnd(4);
    *at<int *>(0x7153bc) = fields;
    for (int i = 0; i < 20; i++)
        at<char **>(0x7153c0)[i] = nullptr;
    if (l->list_state == 2)
        for (int i = 0; i < fields; i++)
            at<char **>(0x7153c0)[i] = (char *)(i == 0 ? "ip" : WORDS[i]);
    if (l->list_state == 1 && rnd(2))
        at<char **>(0x7153c0)[0] = (char *)"hostname";
    buf[0] = 0;
    switch (l->list_state) {
    case 0:
        append(buf, sizeof buf, rnd(4) ? "\\fieldcount\\" : "\\short\\");
        append_number(buf, sizeof buf, rnd(30));
        append(buf, sizeof buf, rnd(4) ? "\\" : "");
        break;
    case 1:
        append(buf, sizeof buf, "\\newfield");   // a name not yet in the key strings (entering one would free it)
        append_number(buf, sizeof buf, (DWORD)k);
        append(buf, sizeof buf, rnd(4) ? "\\" : "");
        break;
    case 2:
        for (int i = 0; i < fields; i++) {
            append(buf, sizeof buf, "\\");
            append(buf, sizeof buf, i == 0 ? "10.0.0.9:26800" : word());
        }
        append(buf, sizeof buf, rnd(4) ? "\\" : "");
        break;
    }
    const int len = (int)slen(buf);
    const int r = side_fn<int(__cdecl *)(ServerList *, char *, int)>(0x5c6a50)(l, buf, rnd(4) ? len : (int)rnd((DWORD)len + 1));
    ev_raw(0x161, (DWORD)r, (DWORD)l->list_state);
    st_bytes(buf, sizeof buf);
    st_list(l);
    l->state = 0;
    free_list(l);
}

// QueryLoop directly with servers of our own (ServerListAddServer), FindServer, Halt, Clear
void query_case(int k)
{
    ServerList *l = new_list(k);
    g_clock_step = 1 + rnd(100);
    g_server_replies = (int)rnd(4);
    g_halt_at = rnd(6) == 0 ? 1 + (int)rnd(3) : 0;
    g_messages = 0;
    for (int i = 0; i < l->nsockets; i++) {
        l->sockets[i].s = 0x1000 + (DWORD)i;
        l->sockets[i].server = nullptr;
    }
    g_nsockets = (DWORD)l->nsockets;
    l->next = l->abort = 0;   // as ServerListUpdate sets them
    l->query_type = 5;
    const int servers = (int)rnd(10);
    for (int i = 0; i < servers; i++) {
        void *s = side_fn<void *(__cdecl *)(ServerList *, DWORD, DWORD, int)>(0x5c6870)(rnd(20) ? l : nullptr, 0x0a000000u + (DWORD)i * 0x1000000u + 3,
                                                                                    26794 + rnd(4), (int)rnd(6));
        ev_raw(0x170, (DWORD)server_index(s));
    }
    for (int i = 0; i < 3; i++) {
        const int r = side_fn<int(__cdecl *)(ServerList *, DWORD, DWORD, int)>(0x5c68c0)(l, 0x0a000000u + rnd(5) * 0x1000000u + 3, 26794 + rnd(4),
                                                                                   (int)rnd((DWORD)l->servers->count + 1));   // more would read behind the list
        ev_raw(0x171, (DWORD)r);
    }
    l->state = 3;
    for (int i = 0; i < 300 && l->state == 3; i++) {
        for (int q = 0; q < l->nsockets; q++)   // the query timeout's boundary (3000 ms) for the first active one
            if (l->sockets[q].server) {
                if (rnd(3) == 0)
                    clock_at(l->sockets[q].start, 2999 + rnd(3));
                break;
            }
        const int r = rnd(4) ? side_fn<int(__cdecl *)(ServerList *)>(0x5c7170)(l) : side_fn<int(__cdecl *)(ServerList *)>(0x5c74c0)(l);
        ev_raw(0x172, (DWORD)r, (DWORD)l->state);
        if (rnd(40) == 0)
            ev_raw(0x173, (DWORD)side_fn<int(__cdecl *)(ServerList *)>(0x5c7850)(l));
    }
    st_list(l);
    ev_raw(0x174, (DWORD)side_fn<int(__cdecl *)(ServerList *)>(0x5c7870)(l));
    st_list(l);
    l->state = 0;
    free_list(l);
}

// The servers' functions directly
void server_case(int k)
{
    ServerList *l = new_list(k);
    HashTable *keys = l->keylist;
    char data[0x200];
    bzero(data, sizeof data);   // mytok(data + 1) on an empty text reads behind it
    switch (rnd(6)) {
    case 0: {
        Server *s = side_fn<Server *(__cdecl *)(DWORD, DWORD, int, HashTable *)>(0x5c7dc0)(random(), random(), (int)rnd(8), keys);
        data[0] = 0;
        key_values(data, sizeof data, (int)rnd(10));
        if (rnd(3))
            append(data, sizeof data, "\\final\\\\queryid\\2.1");
        side_fn<void(__cdecl *)(Server *, char *)>(0x5c7e60)(s, data);
        st_bytes(data, sizeof data);
        st_server(s);
        for (int i = 0; i < 4; i++) {
            char *key = (char *)(rnd(5) == 0 ? "hostaddr" : rnd(5) == 0 ? "ping" : word());
            char *v = side_fn<char *(__cdecl *)(Server *, char *, char *)>(0x5c8080)(s, key, (char *)"fallback");
            ev_raw(0x180, hash_text(v));
            const int iv = side_fn<int(__cdecl *)(Server *, char *, int)>(0x5c8130)(s, key, -7);
            ev_raw(0x181, (DWORD)iv);
        }
        ev_raw(0x182, (DWORD)side_fn<int(__cdecl *)(Server *)>(0x5c8050)(s), side_fn<DWORD(__cdecl *)(Server *)>(0x5c8070)(s));
        ev_raw(0x183, hash_text(side_fn<char *(__cdecl *)(Server *)>(0x5c8060)(s)));
        ev_raw(0x184, hash_text(side_fn<char *(__cdecl *)(Server *, char *)>(0x5c7d70)(s, (char *)word())));
        side_fn<void(__cdecl *)(Server **)>(0x5c78e0)(&s);
        break;
    }
    case 1:
    case 2: {
        const int type = rnd(2) ? 6 : (int)rnd(8);
        const char *const NAMES[] = {"ip", "hostname", "other", "numplayers", "mapname"};
        char *f[5];
        const int nf = (int)rnd(6);
        for (int i = 0; i < nf; i++)
            f[i] = (char *)NAMES[i];
        data[0] = 0;
        const int values = (int)rnd(7);
        for (int i = 0; i < values; i++) {
            append(data, sizeof data, "\\");
            if (i == 0 && rnd(3))
                append(data, sizeof data, rnd(4) ? "10.1.2.3:28000" : "nocolon");
            else if (i == 2 && rnd(2))
                append(data, sizeof data, "\x01key\x01val\x01final\x01x\x01k2\x01v2");
            else
                append(data, sizeof data, word());
        }
        if (rnd(2))
            append(data, sizeof data, "\\");
        Server *s = side_fn<Server *(__cdecl *)(char **, int, char *, int, HashTable *)>(0x5c7900)(f, nf, data, type, keys);
        st_bytes(data, sizeof data);
        st_server(s);
        side_fn<void(__cdecl *)(Server **)>(0x5c78e0)(&s);
        break;
    }
    case 3: {
        data[0] = 0;
        key_values(data, sizeof data, (int)rnd(6));
        char *t = side_fn<char *(__cdecl *)(char *, int)>(0x5c7d10)(rnd(4) ? data : data + 1, rnd(4) ? '\\' : 'e');
        ev_raw(0x185, t ? (DWORD)(t - data) : 0xdead);
        for (int i = 0; i < 6; i++) {
            t = side_fn<char *(__cdecl *)(char *, int)>(0x5c7d10)(nullptr, '\\');
            ev_raw(0x185, t ? (DWORD)(t - data) : 0xdead);
        }
        st_bytes(data, sizeof data);
        st_globals();
        const int len = (int)slen(data);
        const int c = side_fn<int(__cdecl *)(const char *, int, int)>(0x5c7130)(data, len, (int)rnd(8));
        ev_raw(0x186, (DWORD)c);
        break;
    }
    case 4: {
        char *a = (char *)WORDS[rnd(sizeof WORDS / sizeof WORDS[0])], *b = (char *)(rnd(2) ? "HOSTNAME" : WORDS[rnd(10)]);
        ev_raw(0x187, side_fn<DWORD(__cdecl *)(char **, DWORD)>(0x5c81e0)(&a, 1 + rnd(600)));
        KeyValue x = {a, b}, y = {b, a};
        ev_raw(0x188, side_fn<DWORD(__cdecl *)(KeyValue *, DWORD)>(0x5c8220)(&x, 1 + rnd(9)));
        ev_raw(0x189, (DWORD)side_fn<int(__cdecl *)(char **, char **)>(0x5c8260)(&a, &b));
        ev_raw(0x18a, (DWORD)(side_fn<int(__cdecl *)(KeyValue *, KeyValue *)>(0x5c8280)(&x, &y) != 0));
        char *d = at<char *(__cdecl *)(const char *)>(0x5d3606)("freed");
        side_fn<void(__cdecl *)(char **)>(0x5c8290)(&d);
        break;
    }
    case 5: {
        char *f[3] = {(char *)"ip", (char *)"hostname", (char *)"numplayers"};
        data[0] = 0;
        append(data, sizeof data, "\\10.2.3.4:26794\\name\\5\\");
        void *s = side_fn<void *(__cdecl *)(ServerList *, char **, int, char *, int)>(0x5c70f0)(l, f, 3, data, 6 + (int)rnd(2));
        ev_raw(0x18b, (DWORD)server_index(s));
        break;
    }
    }
    st_list(l);
    free_list(l);
}

}  // namespace

ZAX_CHECKS(gamespy_sb)
{
    (void)cls;
    (void)n;
    install();
    cases("gamespy sb: ServerListUpdate (sync and async), Think, query loop, Clear", scaled(200), 0x5c61c0u, [](int k) { update_case(k); });
    cases("gamespy sb: ServerListLANUpdate (sync and async), LAN list", scaled(200), 0x5c64b0u, [](int k) { lan_case(k); });
    cases("gamespy sb: ReadList and ParseInfoList (lists and field lists)", scaled(300), 0x5c6c80u, [](int k) { read_case(k); });
    cases("gamespy sb: QueryLoop, AddServer, FindServer, Halt, Clear", scaled(200), 0x5c7170u, [](int k) { query_case(k); });
    cases("gamespy sb: servers, mytok, keys, hash callbacks", scaled(600), 0x5c7dc0u, [](int k) { server_case(k); });
    uninstall();
}

}  // namespace zax
