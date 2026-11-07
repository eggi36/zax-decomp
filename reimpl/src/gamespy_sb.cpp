// GameSpy server browsing ("CEngine") of Zax.exe, 0x5c6060-0x5c8290 (phase 5b; docs/DECOMPILATION.md 11, "GameSpy SDK
// (2001)"): the server list (sb_serverlist.c: New, Free, Update, LANUpdate, Think, Halt, Clear and the static
// helpers, some of which the compiler inlined into Update, LANUpdate and Think as well as keeping them as functions),
// the servers (server.c: ServerNew, ServerNewData, ServerParseKeyVals, the getters) and the hash table callbacks.
// The SDK names are after the SDK of that time [assumption]; what each function does is [confirmed: code].
// Conventions: gamespy.h.
//
// The list (0xa0 bytes): +0 the state (0 idle, 1 reading the master's list, 2 waiting for LAN answers, 3 querying
// the servers), +4 the servers (darray of Server pointers, freed through 0x5c78e0), +8 the query sockets (+0x6c of
// them, 0x1c bytes each: socket, the server being queried, the query's start time, its address), +0xc the game
// name ("Zax"), +0x2c the secret key, +0x4c the engine name ("zax"), +0x70 the next server to query, +0x74 the abort
// flag, +0x78/+0x7c the callback (list, 1 = state changed / 2 = a server, instance, server, percent) and its
// instance, +0x80 the sort key (""), +0x88 the TCP or LAN socket, +0x8c the LAN search's start, +0x90 the query type
// (5 status; 6 groups and 7 info2 lists come with field names), +0x94 the key strings (hash table of 500 buckets,
// shared by all servers), +0x98 the server count when the update began, +0x9c the field list's parse state.
// The master's list is read into 0x714bbc (0x800 bytes, 0x715410 filled); a field list's names are 0x7153c0 (at
// most 20, 0x7153bc of them); mytok's position 0x715414.
//
// A server (0x14 bytes): +0 the IP, +4 the query port, +6 the ping (9999 until answered), +8 the query type, +0xc
// its key/value pairs (a hash table of {key, value} pointers into the list's key strings, hashed by the key's text
// and compared by the key's pointer), +0x10 the list's key strings.
#include "gamespy.h"

namespace zax {
namespace gamespy {

namespace {

constexpr DWORD G_LIST_BUFFER = 0x714bbc, G_LIST_FILL = 0x715410, G_FIELD_COUNT = 0x7153bc, G_FIELDS = 0x7153c0,
                G_TOKEN = 0x715414, QUERY_TEXTS = 0x627e50, QUERY_LENGTHS = 0x606148;
constexpr DWORD SERVER_FREE = 0x5c78e0, STRING_HASH = 0x5c81e0, CASE_COMPARE = 0x5c8260, STRING_FREE = 0x5c8290,
                KEY_HASH = 0x5c8220, KEY_COMPARE = 0x5c8280;

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
struct ServerList;
typedef void(__cdecl *ListCallback)(ServerList *, int, void *, void *, void *);
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
    char game_name[0x20];
    char secret_key[0x20];
    char engine_name[0x20];
    int nsockets;
    int next;
    int abort;
    ListCallback callback;
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
static_assert(sizeof(ServerList) == 0xa0);

const char *text(DWORD a) { return at<const char *>(a); }
int &fill() { return *glob<int *>(G_LIST_FILL); }
char *list_buffer() { return glob<char *>(G_LIST_BUFFER); }
int &field_count() { return *glob<int *>(G_FIELD_COUNT); }
char **fields() { return glob<char **>(G_FIELDS); }
char *&token() { return *glob<char **>(G_TOKEN); }

void notify(ServerList *l, int msg, void *a = nullptr, void *b = nullptr) { l->callback(l, msg, l->instance, a, b); }
void mode_change_at(ServerList *l, int state) { fn<void(__cdecl *)(ServerList *, int)>(0x5c6850)(l, state); }
Server *server_new_at(DWORD ip, DWORD port, int type, HashTable *keylist)
{
    return fn<Server *(__cdecl *)(DWORD, DWORD, int, HashTable *)>(0x5c7dc0)(ip, port, type, keylist);
}
Server *server_new_data_at(char **f, int n, char *data, int type, HashTable *keylist)
{
    return fn<Server *(__cdecl *)(char **, int, char *, int, HashTable *)>(0x5c7900)(f, n, data, type, keylist);
}
int read_list_at(ServerList *l) { return fn<int(__cdecl *)(ServerList *)>(0x5c6c80)(l); }
int lan_list_at(ServerList *l) { return fn<int(__cdecl *)(ServerList *)>(0x5c6920)(l); }
int query_loop_at(ServerList *l) { return fn<int(__cdecl *)(ServerList *)>(0x5c7170)(l); }
DWORD server_ip_at(Server *s) { return fn<DWORD(__cdecl *)(Server *)>(0x5c9cc0)(s); }   // ArrayLength's code (folded)
DWORD server_port_at(Server *s) { return fn<DWORD(__cdecl *)(Server *)>(0x5c8070)(s); }

// mytok(s, '\\') (0x5c7d10, also inlined): the next token from s (or from where the last one ended), null for an
// empty one; the separator is overwritten with 0
char *tok(char *s)
{
    if (s)
        token() = s;
    char *start = token();
    while (*token() && *token() != '\\')
        token()++;
    if (token() == start)
        start = nullptr;
    if (*token()) {
        *token() = 0;
        token()++;
    }
    return start;
}

// The key strings: the list's copy of a text, made (_strdup + TableEnter) when it is new (LookupKey, also inlined)
char *intern(HashTable *keylist, char *k)
{
    char **found = (char **)TableLookup(keylist, &k);
    if (found)
        return *found;
    k = crt_strdup(k);
    TableEnter(keylist, &k);
    return k;
}

// The new query sockets (UDP) of an update; 0, or 1 when not even the first one could be made (the count shrinks to
// the ones made otherwise)
bool make_sockets(ServerList *l)
{
    for (int i = 0; i < l->nsockets; i++) {
        l->sockets[i].s = ws_socket(2, 2, 0x11);
        if (l->sockets[i].s == 0xffffffffu) {
            if (i == 0)
                return false;
            l->nsockets = i;
            break;
        }
        l->sockets[i].server = nullptr;
        l->sockets[i].start = 0;
    }
    return true;
}

// Counts n '\' from p: the length up to and with the n-th, -1 when len bytes do not hold them (0x5c7130, inlined in
// 0x5c6a50)
int slashes(const char *p, int len, int n)
{
    const char *q = p;
    if (n != 0) {
        while (q != p + len) {
            if (*q == '\\')
                n--;
            q++;
            if (n == 0)
                return (int)(q - p);
        }
        if (n != 0)
            return -1;
    }
    return (int)(q - p);
}

// A groups/info2 list: the field count ("\fieldcount\<n>\" after a 12-character header), then the field names, then
// one record per server (field count + 1 '\'); the bytes used minus one, 0 when more data is needed, -1 on errors.
// STANDALONE: ServerListReadList's inlined copy (helpers 0x5c7130 and 0x5c70f0), otherwise 0x5c6a50's own code.
template <bool STANDALONE> int parse_info_list(ServerList *l, char *p, int len)
{
    switch (l->list_state) {
    case 0: {
        const int n = slashes(p, len, 3);
        if (n == -1)
            return 0;
        if (n < 12)
            return -1;
        char number[0x40];
        crt_strncpy(number, p + 12, (DWORD)(n - 12));
        number[n - 13] = 0;
        field_count() = crt_atoi(number);
        if (field_count() > 20)
            return -1;
        l->list_state = 1;
        if (field_count() > 0)
            bzero(fields(), (DWORD)field_count() * 4);
        return n - 1;
    }
    case 1: {
        const int n = slashes(p, len, 2);
        if (n == -1)
            return 0;
        char name[0x40];
        crt_strncpy(name, p + 1, (DWORD)(n - 2));
        name[n - 2] = 0;
        char *s = crt_strdup(name);
        TableEnter(l->keylist, &s);
        if (field_count() <= 0)
            return -1;
        int i;
        for (i = 0; i < field_count(); i++)
            if (!fields()[i])
                break;
        if (i == field_count())
            return -1;
        fields()[i] = s;
        if (i == field_count() - 1)
            l->list_state = 2;
        return n - 1;
    }
    case 2: {
        int n;
        Server *s;
        if (STANDALONE) {
            n = fn<int(__cdecl *)(const char *, int, int)>(0x5c7130)(p, len, field_count() + 1);
            if (n == -1)
                return 0;
            s = fn<Server *(__cdecl *)(ServerList *, char **, int, char *, int)>(0x5c70f0)(l, fields(), field_count(), p, l->query_type);
        } else {
            n = slashes(p, len, field_count() + 1);
            if (n == -1)
                return 0;
            s = server_new_data_at(fields(), field_count(), p, l->query_type, l->keylist);
            ArrayAppend(l->servers, &s);
        }
        p[n - 1] = '\\';
        notify(l, 2, s);
        return n - 1;
    }
    }
    return -1;
}

// Reading the master's list (TCP): what select says is there is appended to the buffer; "\final\" (or the abort
// flag) closes the socket and ends the state (to idle for groups/info2 lists, to querying otherwise); records of six
// bytes (IP, port) add the servers not yet in the list, field lists go through parse_info_list. KIND 0 is
// ServerListReadList 0x5c6c80, 1 its copy inlined into ServerListLANUpdate, 2 the copy in ServerListThink.
template <int KIND> int read_list(ServerList *l)
{
    FdSet set;
    TimeVal timeout;
    if (KIND != 0) {
        timeout.sec = 0;
        timeout.usec = 0;
    }
    set.sockets[0] = l->sock;
    set.count = 1;
    if (KIND == 0) {
        timeout.sec = 0;
        timeout.usec = 0;
    }
    if (ws_select(0x40, &set, nullptr, nullptr, &timeout) <= 0)
        return 0;
    const int n = ws_recv(l->sock, list_buffer() + fill(), 0x7ff - fill(), 0);
    if (n == -1 || n == 0) {
        ws_closesocket(l->sock);
        l->sock = 0xffffffffu;
        fill() = 0;
        if (l->state != 0)
            l->abort = 1;
        if (KIND == 1) {
            mode_change_at(l, 3);
        } else {
            l->state = 3;
            notify(l, 1);
        }
        return 3;
    }
    char *p = list_buffer();
    fill() += n;
    if (fill() - 6 >= 0) {
        for (;;) {
            if (crt_strncmp(p, text(glob_addr(0x627f2c)), 7) == 0 || l->abort != 0) {   // "\final\"
                ws_closesocket(l->sock);
                l->sock = 0xffffffffu;
                fill() = 0;
                const int next = l->query_type == 6 || l->query_type == 7 ? 0 : 3;
                if (KIND == 1) {
                    mode_change_at(l, next);
                } else {
                    l->state = next;
                    notify(l, 1);
                }
                return 0;
            }
            if (fill() < 6)
                break;
            if (l->query_type == 6 || l->query_type == 7) {
                const int used = KIND == 0 ? parse_info_list<true>(l, p, (int)(list_buffer() + fill() - p))
                                           : fn<int(__cdecl *)(ServerList *, char *, int)>(0x5c6a50)(l, p, (int)(list_buffer() + fill() - p));
                if (used < 0)
                    l->abort = 1;
                else if (used == 0)
                    break;
                else
                    p += used;
            } else {
                const DWORD ip = *(DWORD *)p;
                // ntohs gets the register the port word was loaded into: in ReadList its high half is the query
                // type's (0 for the types the SDK has), elsewhere 0
                const DWORD port = (KIND == 0 ? (DWORD)l->query_type & 0xffff0000u : 0) | *(WORD *)(p + 4);
                p += 6;
                if (KIND == 0) {
                    const int count = l->start_count;
                    const DWORD host_port = ws_ntohs(port) & 0xffff;
                    int i;
                    for (i = 0; i < count; i++) {
                        Server *s = *(Server **)ArrayNth(l->servers, i);
                        if (host_port == server_port_at(s) && server_ip_at(s) == ip)
                            break;
                    }
                    if (i >= count) {
                        const int type = l->query_type;
                        const WORD p2 = ws_ntohs(port);
                        Server *s = server_new_at(ip, p2, type, l->keylist);
                        ArrayAppend(l->servers, &s);
                    }
                } else {
                    const int count = l->start_count;
                    const DWORD host_port = ws_ntohs(port) & 0xffff;
                    if (fn<int(__cdecl *)(ServerList *, DWORD, DWORD, int)>(0x5c68c0)(l, ip, host_port, count) == -1) {
                        const int type = l->query_type;
                        fn<Server *(__cdecl *)(ServerList *, DWORD, DWORD, int)>(0x5c6870)(l, ip, ws_ntohs(port), type);
                    }
                }
            }
            if ((int)(p - list_buffer()) > fill() - 6)
                break;
        }
    }
    fill() += (int)(list_buffer() - p);
    crt_memmove(list_buffer(), p, (DWORD)fill());
    return 0;
}

// The LAN answers on the broadcast socket: every packet with "\final\" adds its sender (status query); after 3 s the
// socket is closed and the servers are queried. THINK: ServerListThink's copy (through ServerListModeChange).
template <bool THINK> int lan_list(ServerList *l)
{
    SockAddr from;
    int from_length;
    TimeVal timeout;
    timeout.sec = 0;
    timeout.usec = 0;
    from_length = 0x10;
    for (;;) {
        FdSet set;
        set.sockets[0] = l->sock;
        set.count = 1;
        const int ready = ws_select(0x40, &set, nullptr, nullptr, &timeout);
        if (ready == -1 || ready == 0)
            break;
        char buf[0x5dc];
        if (ws_recvfrom(l->sock, buf, 0x5db, 0, &from, &from_length) == -1)
            continue;
        if (!crt_strstr(buf, text(glob_addr(0x627f2c))))   // the packet is not terminated: strstr reads what the buffer held
            continue;
        const WORD port = ws_ntohs(*(DWORD *)&from.port);
        if (THINK || l) {
            Server *s = server_new_at(from.addr, port, 5, l->keylist);
            ArrayAppend(l->servers, &s);
        }
    }
    if (tick() - l->lan_start > 3000) {
        ws_closesocket(l->sock);
        l->sock = 0xffffffffu;
        if (THINK) {
            mode_change_at(l, 3);
        } else {
            l->state = 3;
            notify(l, 1);
        }
    }
    return 0;
}

// The synchronous loop of Update and LANUpdate: the state's work every 10 ms until idle
template <bool LAN> void run(ServerList *l)
{
    while (l->state != 0) {
        if (l) {
            switch (l->state) {
            case 1:
                if (LAN)
                    read_list<1>(l);
                else
                    read_list_at(l);
                break;
            case 2:
                lan_list_at(l);
                break;
            case 3:
                query_loop_at(l);
                break;
            }
        }
        msleep(10);
    }
}

}  // namespace

// 0x5c6060: a new list (the callback type argument is unused); WSAStartup
ServerList *__cdecl ServerListNew(const char *game_name, const char *engine_name, const char *secret_key, int max_updates,
                                  ListCallback callback, int callback_type, void *instance)
{
    (void)callback_type;
    ServerList *l = (ServerList *)crt_malloc(0xa0);
    l->state = 0;
    l->servers = ArrayNew(4, 0x40, code<ElemFree>(SERVER_FREE));
    l->keylist = TableNew2(4, 500, 4, fn_addr(STRING_HASH), fn_addr(CASE_COMPARE), fn_addr(STRING_FREE));
    l->nsockets = max_updates;
    l->sockets = (QuerySocket *)crt_malloc((DWORD)max_updates * 0x1c);
    bzero(l->sockets, (DWORD)max_updates * 0x1c);
    scpy(l->game_name, game_name);
    scpy(l->secret_key, secret_key);
    scpy(l->engine_name, engine_name);
    l->instance = instance;
    l->callback = callback;
    l->sort_key = empty();
    l->start_count = 0;
    SocketStartUp();
    return l;
}

// 0x5c6180
void __cdecl ServerListFree(ServerList *l)
{
    ArrayFree(l->servers);
    TableFree(l->keylist);
    crt_free(l->sockets);
    crt_free(l);
    SocketShutDown();
}

// 0x5c61c0: the master's list (TCP to master.gamespy.com:28900): its "\secure\" challenge answered with the engine
// name, "1.6" and gs_encode(gs_encrypt(challenge, secret key)), then "\list\cmp\gamename\<name>\final\"; the query
// sockets; without async the whole update runs here. 0, 1 (no socket), 2 (no host), 3 (connection), 4 (busy),
// 5 (no challenge); only a failed connect closes the socket
int __cdecl ServerListUpdate(ServerList *l, int async)
{
    if (l->state != 0)
        return 4;
    SockAddr master;
    l->query_type = 5;
    master.family = 2;
    master.port = ws_htons(0x70e4);
    master.addr = ws_inet_addr(text(glob_addr(0x627e10)));   // "master.gamespy.com"
    if (master.addr == 0xffffffffu) {
        HostEnt *h = ws_gethostbyname(text(glob_addr(0x627e10)));
        if (!h)
            return 2;
        master.addr = *(DWORD *)h->addresses[0];
    }
    l->sock = ws_socket(2, 1, 6);
    if (l->sock == 0xffffffffu)
        return 1;
    char buf[0x100], coded[0x20];
    if (ws_connect(l->sock, &master, 0x10) != 0) {
        ws_closesocket(l->sock);
        return 3;
    }
    {
        const int n = ws_recv(l->sock, buf, 0xff, 0);
        if (n == -1)
            return 3;
        buf[n] = 0;
        char *challenge = crt_strstr(buf, text(glob_addr(0x627f20)));   // "\secure\"
        if (!challenge)
            return 5;
        challenge += 8;
        fn<void(__cdecl *)(const char *, int, char *, int)>(0x5c91e0)(l->secret_key, 6, challenge, 6);
        fn<void(__cdecl *)(const char *, int, char *)>(0x5c9110)(challenge, 6, coded);
        crt_sprintf(buf, text(glob_addr(0x627ed8)), l->engine_name, text(glob_addr(0x627f1c)), coded);
        int r = ws_send(l->sock, buf, (int)slen(buf), 0);
        if (r == -1 || r == 0)
            return 3;
        const char *type;
        if (l->query_type == 6) {
            type = text(glob_addr(0x627ed0));   // "groups"
            l->list_state = 0;
        } else if (l->query_type == 7) {
            type = text(glob_addr(0x627ec8));   // "info2"
            l->list_state = 0;
        } else {
            type = text(glob_addr(0x627ec4));   // "cmp"
        }
        crt_sprintf(buf, text(glob_addr(0x627ea8)), type, l->game_name);
        r = ws_send(l->sock, buf, (int)slen(buf), 0);
        if (r == -1 || r == 0)
            return 3;
    }
    l->state = 1;
    notify(l, 1);
    if (!make_sockets(l))
        return 1;
    l->next = 0;
    l->abort = 0;
    l->start_count = ArrayLength(l->servers);
    if (!async)
        run<false>(l);
    return 0;
}

// 0x5c64b0: "\status\" broadcast (SO_BROADCAST) to the ports start..end in steps of delta (compared as a short);
// without async the whole search runs here. 0, 1 (no socket), 4 (busy)
int __cdecl ServerListLANUpdate(ServerList *l, int async, int start_port, int end_port, int delta)
{
    if (l->state != 0)
        return 4;
    if (!make_sockets(l))
        return 1;
    DWORD on = 1;
    l->sock = ws_socket(2, 2, 0x11);
    if (l->sock == 0xffffffffu)
        return 1;
    if (ws_setsockopt(l->sock, 0xffff, 0x20, &on, 4) != 0)
        return 1;
    SockAddr to;
    to.family = 2;
    to.addr = 0xffffffffu;
    for (int port = start_port; (short)port <= end_port; port += delta) {
        to.port = ws_htons((DWORD)port);
        ws_sendto(l->sock, text(glob_addr(0x627e68)), 8, 0, &to, 0x10);   // "\status\"
    }
    l->state = 2;
    notify(l, 1);
    l->lan_start = tick();
    l->next = 0;
    l->abort = 0;
    if (!async)
        run<true>(l);
    return 0;
}

// 0x5c6850 (ServerListModeChange)
void __cdecl ServerListModeChange(ServerList *l, int state)
{
    l->state = state;
    notify(l, 1);
}

// 0x5c6870: a new server appended; null for a null list
Server *__cdecl ServerListAddServer(ServerList *l, DWORD ip, DWORD port, int type)
{
    if (!l)
        return nullptr;
    Server *s = server_new_at(ip, port, type, l->keylist);
    ArrayAppend(l->servers, &s);
    return s;
}

// 0x5c68c0: the index of the server with this port and IP among the first count, or -1
int __cdecl ServerListFindServer(ServerList *l, DWORD ip, DWORD port, int count)
{
    for (int i = 0; i < count; i++) {
        Server *s = *(Server **)ArrayNth(l->servers, i);
        if (port == server_port_at(s) && server_ip_at(s) == ip)
            return i;
    }
    return -1;
}

// 0x5c6920
int __cdecl ServerListLANList(ServerList *l) { return lan_list<false>(l); }

// 0x5c6a50
int __cdecl ServerListParseInfoList(ServerList *l, char *p, int len) { return parse_info_list<false>(l, p, len); }

// 0x5c6c80
int __cdecl ServerListReadList(ServerList *l) { return read_list<0>(l); }

// 0x5c70f0: a server from a field list's record, appended
Server *__cdecl ServerListAddServerData(ServerList *l, char **f, int n, char *data, int type)
{
    Server *s = server_new_data_at(f, n, data, type, l->keylist);
    ArrayAppend(l->servers, &s);
    return s;
}

// 0x5c7130
int __cdecl CountSlashes(const char *p, int len, int n) { return slashes(p, len, n); }

// 0x5c7170: the query sockets: answers of the queried servers (ping from the first answer, the keys, "\final\"
// reports the server with the percentage done), queries older than 3 s dropped, idle sockets query the next servers
// (the query type's text, inet_addr(inet_ntoa(ip)) and the query port); when nothing is left (or aborted) the
// sockets are closed and the list is idle
int __cdecl ServerListQueryLoop(ServerList *l)
{
    int active = 0;
    TimeVal timeout;
    SockAddr from;
    int from_length;
    timeout.sec = 0;
    timeout.usec = 0;
    from_length = 0x10;
    FdSet set;
    set.count = 0;
    QuerySocket *q = l->sockets;
    for (int n = l->nsockets; n > 0; n--, q++)
        if (q->server) {
            active++;
            if (set.count < 0x40)
                set.sockets[set.count++] = q->s;
        }
    if (active > 0) {
        const int ready = ws_select(0x40, &set, nullptr, nullptr, &timeout);
        if (ready != -1 && ready != 0)
            for (int i = 0; i < l->nsockets; i++) {
                if (!l->sockets[i].server || !ws_fdisset(l->sockets[i].s, &set))
                    continue;
                char buf[0x5dc];
                from_length = 0x10;
                const int n = ws_recvfrom(l->sockets[i].s, buf, 0x5db, 0, &from, &from_length);
                QuerySocket &r = l->sockets[i];
                if (from.addr != r.address.addr || from.port != r.address.port)
                    continue;
                if (n == -1) {
                    r.server = nullptr;
                    continue;
                }
                buf[n] = 0;
                const bool final = crt_strstr(buf, text(glob_addr(0x627f2c))) != nullptr;
                Server *s = l->sockets[i].server;
                if (s->ping == 9999)
                    s->ping = (short)((WORD)tick() - (WORD)l->sockets[i].start);
                fn<void(__cdecl *)(Server *, char *)>(0x5c7e60)(s, buf);
                if (final) {
                    const int count = ArrayLength(l->servers);
                    notify(l, 2, s, (void *)(uintptr_t)(l->next * 100 / count));
                    l->sockets[i].server = nullptr;
                }
            }
    }
    for (int i = 0; i < l->nsockets; i++)
        if (l->sockets[i].server) {
            QuerySocket &r = l->sockets[i];
            if (tick() - r.start > 3000)
                r.server = nullptr;
        }
    if (l->abort == 0 && (l->next < ArrayLength(l->servers) || active != 0)) {
        for (int i = 0; i < l->nsockets; i++) {
            const int next = l->next;
            if (next >= ArrayLength(l->servers))
                return 0;
            if (l->sockets[i].server)
                continue;
            Server *s = *(Server **)ArrayNth(l->servers, next);
            l->next++;
            l->sockets[i].server = s;
            l->sockets[i].address.family = 2;
            l->sockets[i].address.addr = ws_inet_addr(fn<char *(__cdecl *)(Server *)>(0x5c8060)(s));
            l->sockets[i].address.port = ws_htons(server_port_at(s));
            QuerySocket &r = l->sockets[i];
            const int type = s->query_type;
            ws_sendto(r.s, glob<const char **>(QUERY_TEXTS)[type], glob<const int *>(QUERY_LENGTHS)[type], 0, &r.address, 0x10);
            l->sockets[i].start = tick();
        }
        return 0;
    }
    for (int i = 0; i < l->nsockets; i++)
        ws_closesocket(l->sockets[i].s);
    l->state = 0;
    notify(l, 1);
    return 0;
}

// 0x5c74c0: one step of an asynchronous update; 5 for a null list, 3 when the master's connection broke
int __cdecl ServerListThink(ServerList *l)
{
    if (!l)
        return 5;
    switch (l->state) {
    case 1:
        return read_list<2>(l);
    case 2:
        lan_list<true>(l);
        return 0;
    case 3:
        return query_loop_at(l);
    }
    return 0;
}

// 0x5c7850: abort a running update
int __cdecl ServerListHalt(ServerList *l)
{
    if (l->state != 0)
        l->abort = 1;
    return 0;
}

// 0x5c7870: the servers and key strings freed and made new; 4 while an update runs
int __cdecl ServerListClear(ServerList *l)
{
    if (l->state != 0)
        return 4;
    ArrayFree(l->servers);
    l->servers = ArrayNew(4, 0x40, code<ElemFree>(SERVER_FREE));
    TableFree(l->keylist);
    l->keylist = TableNew2(4, 500, 4, fn_addr(STRING_HASH), fn_addr(CASE_COMPARE), fn_addr(STRING_FREE));
    l->start_count = 0;
    return 0;
}

// 0x5c78e0: the servers array's free function
void __cdecl ServerFree(Server **e)
{
    Server *s = *e;
    TableFree(s->keyvals);
    crt_free(s);
}

// ---- server.c -------------------------------------------------------------------------------------------------

namespace {

// The key/value table's size by query type (buckets, chains)
HashTable *new_keyvals(int type)
{
    int buckets, chains;
    switch (type) {
    case 0:
        buckets = 4, chains = 2;
        break;
    case 1:
        buckets = 6, chains = 2;
        break;
    case 2:
    case 3:
        buckets = 8, chains = 2;
        break;
    default:
        buckets = 8, chains = 4;
    }
    return TableNew2(8, buckets, chains, fn_addr(KEY_HASH), fn_addr(KEY_COMPARE), 0);
}

bool is_query_key(const char *k) { return scmp(k, text(glob_addr(0x627f3c))) == 0 || scmp(k, text(glob_addr(0x627f34))) == 0; }   // "queryid", "final"

}  // namespace

// 0x5c7900: a server from a field list's record "\v1\v2\...": the field "ip" gives "a.b.c.d:port", groups' field
// "other" holds further "\key\value" pairs (with 0x01 for '\'), every other field a key/value pair
Server *__cdecl ServerNewData(char **f, int nfields, char *data, int type, HashTable *keylist)
{
    Server *s = (Server *)crt_malloc(0x14);
    s->ip = 0;
    s->port = 0;
    s->ping = 9999;
    s->query_type = type;
    s->keyvals = new_keyvals(type);
    s->keylist = keylist;
    char *value = tok(data + 1);
    for (int i = 0; i < nfields; i++) {
        char *field = f[i];
        if (!value)
            value = empty();
        if (scmp(field, text(glob_addr(0x627f4c))) == 0) {   // "ip"
            char *colon = crt_strchr(value, ':');
            if (colon) {
                *colon = 0;
                s->ip = ws_inet_addr(value);
                s->port = (WORD)crt_atoi(colon + 1);
            }
        } else if (type == 6 && scmp(field, text(glob_addr(0x627f44))) == 0) {   // "other"
            for (char *q = value; *q; q++)
                if (*q == 1)
                    *q = '\\';
            char *k = tok(value + 1);
            if (k) {
                do {
                    char *v = fn<char *(__cdecl *)(char *, int)>(0x5c7d10)(nullptr, '\\');
                    if (!v)
                        v = empty();
                    if (!is_query_key(k)) {
                        KeyValue kv;
                        kv.key = fn<char *(__cdecl *)(Server *, char *)>(0x5c7d70)(s, k);
                        kv.value = fn<char *(__cdecl *)(Server *, char *)>(0x5c7d70)(s, v);
                        TableEnter(s->keyvals, &kv);
                    }
                    k = fn<char *(__cdecl *)(char *, int)>(0x5c7d10)(nullptr, '\\');
                } while (k);
            }
        } else {
            KeyValue kv;
            kv.key = intern(s->keylist, field);
            kv.value = intern(s->keylist, value);
            TableEnter(s->keyvals, &kv);
        }
        if (i + 1 >= nfields)
            return s;
        value = tok(nullptr);
    }
    return s;
}

// 0x5c7d10
char *__cdecl mytok(char *s, int delim)
{
    if (s)
        token() = s;
    char *start = token();
    while (*token() && *token() != (char)delim)
        token()++;
    if (token() == start)
        start = nullptr;
    if (*token()) {
        *token() = 0;
        token()++;
    }
    return start;
}

// 0x5c7d70 (LookupKey)
char *__cdecl LookupKey(Server *s, char *k) { return intern(s->keylist, k); }

// 0x5c7dc0
Server *__cdecl ServerNew(DWORD ip, DWORD port, int type, HashTable *keylist)
{
    Server *s = (Server *)crt_malloc(0x14);
    s->ip = ip;
    s->port = (WORD)port;
    s->ping = 9999;
    s->query_type = type;
    s->keyvals = new_keyvals(type);
    s->keylist = keylist;
    return s;
}

// 0x5c7e60: the "\key\value" pairs of a query answer into the server's table ("queryid" and "final" skipped)
void __cdecl ServerParseKeyVals(Server *s, char *data)
{
    char *k = tok(data + 1);
    while (k) {
        char *v = tok(nullptr);
        if (!v)
            v = empty();
        if (!is_query_key(k)) {
            KeyValue kv;
            kv.key = intern(s->keylist, k);
            kv.value = intern(s->keylist, v);
            TableEnter(s->keyvals, &kv);
        }
        k = tok(nullptr);
    }
}

// 0x5c8050, 0x5c8060, 0x5c8070
int __cdecl ServerGetPing(Server *s) { return s->ping; }
char *__cdecl ServerGetAddress(Server *s) { return ws_inet_ntoa(s->ip); }
DWORD __cdecl ServerGetQueryPort(Server *s) { return s->port; }

// 0x5c8080: a key's value; "hostaddr" gives the IP's text
char *__cdecl ServerGetStringValue(Server *s, char *key, char *fallback)
{
    if (scmp(key, text(glob_addr(0x627f50))) == 0)   // "hostaddr"
        return ws_inet_ntoa(s->ip);
    char **k = (char **)TableLookup(s->keylist, &key);
    if (!k)
        return fallback;
    KeyValue kv;
    kv.key = *k;
    KeyValue *found = (KeyValue *)TableLookup(s->keyvals, &kv);
    return found ? found->value : fallback;
}

// 0x5c8130: a key's value through atoi; "ping" gives the ping
int __cdecl ServerGetIntValue(Server *s, char *key, int fallback)
{
    if (scmp(key, text(glob_addr(0x61cae0))) == 0)   // "ping"
        return s->ping;
    char **k = (char **)TableLookup(s->keylist, &key);
    if (!k)
        return fallback;
    KeyValue kv;
    kv.key = *k;
    KeyValue *found = (KeyValue *)TableLookup(s->keyvals, &kv);
    return found ? crt_atoi(found->value) : fallback;
}

// ---- The hash table callbacks ------------------------------------------------------------------------------------

namespace {

DWORD hash_text(const char *s, DWORD buckets)
{
    DWORD h = 0;
    for (; *s; s++)
        h = (DWORD)crt_tolower(*s) - h * 0x63306ce7u;
    return h % buckets;
}

}  // namespace

// 0x5c81e0: the key strings' hash (the text, case-folded with tolower)
DWORD __cdecl StringHash(char **e, DWORD buckets) { return hash_text(*e, buckets); }
// 0x5c8220: the key/value pairs' hash (the key's text)
DWORD __cdecl KeyValHash(KeyValue *e, DWORD buckets) { return hash_text(e->key, buckets); }
// 0x5c8260: _stricmp of two key strings
int __cdecl CaseInsensitiveCompare(char **a, char **b) { return crt_stricmp(*a, *b); }
// 0x5c8280: the pairs' keys by pointer
int __cdecl KeyValCompareKey(KeyValue *a, KeyValue *b) { return (int)((DWORD)(uintptr_t)a->key - (DWORD)(uintptr_t)b->key); }
// 0x5c8290: a key string freed
void __cdecl StringFree(char **e) { crt_free(*e); }

}  // namespace gamespy

ZAX_HOOKS(gamespy_sb)
{
    using namespace gamespy;
    const Hook list[] = {
#include "gamespy_sb_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
