// GameSpy query and reporting of Zax.exe (phase 5b; docs/DECOMPILATION.md 11, "GameSpy SDK (2001)"): qr_init
// 0x5c82a0, qr_process_queries 0x5c85c0 (heartbeats and queries), qr_process_queries_no_heartbeat 0x5c8a60,
// qr_send_statechanged 0x5c8f20, qr_shutdown 0x5c8fb0, qr_value_for_key 0x5c9010 and the packet helpers
// 0x5c9340-0x5c97c0; the master server's challenge algorithms gs_encode 0x5c9110 (base64) and gs_encrypt 0x5c91e0
// (RC4 with the data bytes added to i); nonport.c's msleep, SocketStartUp, SocketShutDown 0x5c9a80-0x5c9ab0; darray.c
// 0x5c9c20-0x5c9dd0 and hashtable.c 0x5c9ac0-0x5c9bd0. Conventions: gamespy.h.
//
// The record (0xf0 bytes; the static one 0x627f88, pointed to by 0x628078, or malloc'ed when qr_init gets a
// pointer for it) [confirmed: code]: +0 the query socket, +4 the heartbeat socket (the same), +8 the game name,
// +0x48 the secret key, +0xc8/+0xcc/+0xd0/+0xd4 the basic/info/rules/players callbacks (cdecl: buffer, 0x578,
// user data), +0xd8 the time of the last heartbeat, +0xdc the query id, +0xe0 the packet number, +0xe4 the query
// port, +0xe8 the state-change resend counter (signed byte), +0xec the user data. The heartbeat address 0x715720
// (master.gamespy.com:27900), the receive buffer 0x715418 (0x100), qr_value_for_key's two result buffers 0x71551c
// (2 x 256, alternating by 0x715518).
//
// A query is "\key\..." with any of basic, info, rules, players, status (all four), packets (the four, each
// flushed with "\queryid\%d.%d" behind it), echo ("\echo\<value>", at most 0x546 characters), secure (the
// challenge, answered with "\validate\<gs_encode(gs_encrypt(challenge, secret key))>"), looked up in that order;
// the answer is collected in a 0x578-byte buffer and sent in packets below 0x546 bytes, split at every second '\'
// (between a key and its value's end), each ending "\queryid\<query id>.<packet number>", the last "\final\".
#include "gamespy.h"

// The import slots the jumps read, through the address layer (batch 6-10a): the address itself in the DLL (the same
// text as before), in the standalone exe the symbol tools/extract_exe_data.py defines for it (our own IAT once its
// home is `own`; as crt_seh.cpp's ZAX_ASM_DATA)
#ifdef ZAX_STANDALONE
#define ZAX_ASM_DATA(a) "_zax_data_" #a
#else
#define ZAX_ASM_DATA(a) #a
#endif

namespace zax {
namespace gamespy {

namespace {

constexpr DWORD G_DEFAULT_RECORD = 0x628078, STATIC_RECORD = 0x627f88, G_HEARTBEAT_ADDRESS = 0x715720,
                G_QUERY = 0x715418, G_VALUE_INDEX = 0x715518, G_VALUES = 0x71551c, QUERY_NAMES = 0x627f64;
constexpr DWORD BUFFER = 0x578;

typedef void(__cdecl *QueryCallback)(char *, int, void *);
struct QrRecord {
    DWORD query_socket;
    DWORD heartbeat_socket;
    char game_name[0x40];
    char secret_key[0x80];
    QueryCallback basic, info, rules, players;
    DWORD last_heartbeat;
    int query_id;
    int packet_number;
    int query_port;
    signed char state_changes;
    BYTE pad[3];
    void *user_data;
};
static_assert(sizeof(QrRecord) == 0xf0);

QrRecord *default_record() { return *glob<QrRecord **>(G_DEFAULT_RECORD); }
const char *text(DWORD a) { return at<const char *>(a); }

void packet_send_at(QrRecord *r, const SockAddr *to, char *out, char *data)
{
    fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, char *)>(0x5c93e0)(r, to, out, data);
}
typedef void(__cdecl *Sender)(QrRecord *, const SockAddr *, char *);
void send_at(DWORD f, QrRecord *r, const SockAddr *to, char *out) { fn_dyn<Sender>(f)(r, to, out); }
constexpr DWORD SEND_BASIC = 0x5c95a0, SEND_INFO = 0x5c9610, SEND_RULES = 0x5c9680, SEND_PLAYERS = 0x5c96f0, ADD_QUERYID = 0x5c9340;

// qr_value_for_key's code (inlined into qr_process_queries_no_heartbeat): "\key\" in the query, its value into the
// next buffer; null when missing
char *value_for_key(const char *query, const char *key)
{
    char search[256];
    search[0] = '\\';
    bzero(search + 1, 255);
    DWORD &index = *glob<DWORD *>(G_VALUE_INDEX);
    index ^= 1;
    scat(search, key);
    scat(search, text(glob_addr(BACKSLASH_TEXT)));
    const char *p = crt_strstr(query, search);
    if (!p)
        return nullptr;
    p += slen(search);
    char *out = at<char *>(glob_addr(G_VALUES) + (index << 8));
    while (*p && *p != '\\')
        *out++ = *p++;
    *out = 0;
    return at<char *>(glob_addr(G_VALUES) + (index << 8));
}

// The answer buffers' start: the first byte copied from "" (0), the rest zeroed
void clear(char *buf, DWORD n)
{
    buf[0] = *empty();
    bzero(buf + 1, n - 1);
}

// "\heartbeat\<query port>\gamename\<name>" to the master
void heartbeat(QrRecord *r)
{
    char buf[256];
    crt_sprintf(buf, text(glob_addr(0x628108)), r->query_port, r->game_name);
    ws_sendto(r->heartbeat_socket, buf, (int)slen(buf), 0, glob<void *>(G_HEARTBEAT_ADDRESS), 0x10);
}

// "\queryid\<id>.<number>" appended and the packet sent when the buffer is not empty (add_queryid, also inlined
// into packet_send and send_final)
void flush(QrRecord *r, const SockAddr *to, char *out)
{
    if (!slen(out))
        return;
    char id[0x50];
    crt_sprintf(id, text(glob_addr(0x628138)), r->query_id, ++r->packet_number);
    scat(out, id);
    ws_sendto(r->query_socket, out, (int)slen(out), 0, to, 0x10);
    out[0] = 0;
}

// The queries waiting on the query socket, answered (both process functions; the heartbeat one calls the send
// functions and qr_value_for_key through their addresses, the other has them inlined)
template <bool INLINE> void answer_queries(QrRecord *r)
{
    SockAddr from;
    int from_length = 0x10;
    TimeVal timeout;
    timeout.sec = 0;
    timeout.usec = 0;
    if (r->query_socket == 0xffffffffu)
        return;
    FdSet set;
    set.sockets[0] = r->query_socket;
    set.count = 1;
    char *const query = glob<char *>(G_QUERY);
    for (;;) {
        const int ready = ws_select(0x40, &set, nullptr, nullptr, &timeout);
        if (ready == -1 || ready == 0)
            return;
        const int len = ws_recvfrom(r->query_socket, query, 0xff, 0, &from, &from_length);
        if (len == -1)
            continue;
        query[len] = 0;
        char out[BUFFER], validate[0x100];
        clear(out, BUFFER);
        clear(validate, 0x100);
        r->packet_number = 0;
        r->query_id++;
        if (r->state_changes > 0)
            r->state_changes = 0;
        for (int i = 0; i < 8; i++) {
            const char *name = glob<const char **>(QUERY_NAMES)[i];
            char *value = INLINE ? value_for_key(query, name)
                                 : fn<char *(__cdecl *)(const char *, const char *)>(0x5c9010)(query, name);
            if (!value)
                continue;
            switch (i) {
            case 0:
            case 1:
            case 2:
            case 3:
                if (INLINE || i == 0) {
                    char data[BUFFER];
                    clear(data, BUFFER);
                    const QueryCallback f = i == 0 ? r->basic : i == 1 ? r->info : i == 2 ? r->rules : r->players;
                    f(data, BUFFER, r->user_data);
                    packet_send_at(r, &from, out, data);
                } else {
                    send_at(i == 1 ? fn_addr(SEND_INFO) : i == 2 ? fn_addr(SEND_RULES) : fn_addr(SEND_PLAYERS), r, &from, out);
                }
                break;
            case 4:
                send_at(fn_addr(SEND_BASIC), r, &from, out);
                send_at(fn_addr(SEND_INFO), r, &from, out);
                send_at(fn_addr(SEND_RULES), r, &from, out);
                send_at(fn_addr(SEND_PLAYERS), r, &from, out);
                break;
            case 5:
                send_at(fn_addr(SEND_BASIC), r, &from, out);
                send_at(fn_addr(ADD_QUERYID), r, &from, out);
                send_at(fn_addr(SEND_INFO), r, &from, out);
                send_at(fn_addr(ADD_QUERYID), r, &from, out);
                send_at(fn_addr(SEND_RULES), r, &from, out);
                send_at(fn_addr(ADD_QUERYID), r, &from, out);
                send_at(fn_addr(SEND_PLAYERS), r, &from, out);
                break;
            case 6:
                if (INLINE) {
                    fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, const char *)>(0x5c9760)(r, &from, out, value);
                } else if (slen(value) <= 0x546) {
                    char echo[BUFFER];
                    crt_sprintf(echo, text(glob_addr(0x6280fc)), value);   // "\echo\%s"
                    packet_send_at(r, &from, out, echo);
                }
                break;
            case 7:
                scpy(validate, value);
                break;
            }
        }
        fn<void(__cdecl *)(QrRecord *, const SockAddr *, char *, char *)>(0x5c97c0)(r, &from, out, validate);
    }
}

void send_callback(QrRecord *r, QueryCallback f, const SockAddr *to, char *out)
{
    char data[BUFFER];
    clear(data, BUFFER);
    f(data, BUFFER, r->user_data);
    packet_send_at(r, to, out, data);
}

}  // namespace

// 0x5c82a0: the query socket bound to the first free port from base_port on (100 tries; ip names the interface,
// 127.0.0.1 means any), the record filled, the master's address; 0, 1 (no socket) or 2 (no port)
int __cdecl qr_init(QrRecord **record, const char *ip, int base_port, const char *game_name, const char *secret_key,
                    QueryCallback basic, QueryCallback info, QueryCallback rules, QueryCallback players, void *user_data)
{
    if (record)
        *record = nullptr;
    SocketStartUp();
    const DWORD s = ws_socket(2, 2, 0x11);
    if (s == 0xffffffffu)
        return 1;
    int port = base_port;
    const int end = base_port + 100;
    int bound = record ? 1 : 0;   // the original's eax when the loop does not run (only with an overflowing base port)
    SockAddr a;
    for (; port < end; port++) {
        a.family = 2;
        a.port = ws_htons((DWORD)port);
        if (!ip)
            a.addr = 0;
        else
            a.addr = resolve(ip);
        if (a.addr == ws_htonl(0x7f000001))
            a.addr = 0;
        bound = ws_bind(s, &a, 0x10);
        if (bound == 0)
            break;
    }
    if (bound != 0)
        return 2;
    if (port == 0) {
        int length = 0x10;
        if (ws_getsockname(s, &a, &length) != 0)
            return 2;
        port = ws_ntohs(*(DWORD *)&a.port) & 0xffff;
    }
    QrRecord *r;
    if (!record) {
        *glob<QrRecord **>(G_DEFAULT_RECORD) = glob<QrRecord *>(STATIC_RECORD);
    } else {
        *glob<QrRecord **>(G_DEFAULT_RECORD) = (QrRecord *)crt_malloc(0xf0);
        *record = default_record();
    }
    r = default_record();
    scpy(r->game_name, game_name);
    scpy(default_record()->secret_key, secret_key);
    default_record()->query_port = port;
    default_record()->last_heartbeat = 0;
    default_record()->heartbeat_socket = s;
    default_record()->query_socket = s;
    default_record()->packet_number = 0;
    default_record()->query_id = 1;
    default_record()->state_changes = 1;
    default_record()->user_data = user_data;
    default_record()->basic = basic;
    default_record()->info = info;
    default_record()->players = players;
    default_record()->rules = rules;
    SockAddr &master = *glob<SockAddr *>(G_HEARTBEAT_ADDRESS);
    master.family = 2;
    master.port = ws_htons(0x6cfc);
    master.addr = resolve(text(glob_addr(0x62807c)));   // "master.gamespy.com"
    return 0;
}

// 0x5c85c0: a heartbeat when 300 s have passed (or none was sent, or the clock went back), while the state change
// counter is positive another one every 30 s (the counter wraps to 0 after 10), then the queries
void __cdecl qr_process_queries(QrRecord *r)
{
    if (!r)
        r = default_record();
    const DWORD now = tick();
    if (r->heartbeat_socket != 0xffffffffu) {
        const DWORD last = r->last_heartbeat, since = now - last;
        if (since > 300000 || last == 0 || now < last) {
            heartbeat(r);
            r->last_heartbeat = tick();
        } else if (r->state_changes > 0 && since > 30000) {
            heartbeat(r);
            r->last_heartbeat = tick();
            if (++r->state_changes > 10)
                r->state_changes = 0;
        }
    }
    answer_queries<false>(r);
}

// 0x5c8a60: the queries only (a game that is not dedicated)
void __cdecl qr_process_queries_no_heartbeat(QrRecord *r)
{
    if (!r)
        r = default_record();
    answer_queries<true>(r);
}

// 0x5c8f20: "\heartbeat\%d\gamename\%s\statechanged\1" to the master
void __cdecl qr_send_statechanged(QrRecord *r)
{
    if (!r)
        r = default_record();
    char buf[256];
    crt_sprintf(buf, text(glob_addr(0x628108)), r->query_port, r->game_name);
    crt_sprintf(buf + slen(buf), text(glob_addr(0x628124)), 1);
    ws_sendto(r->heartbeat_socket, buf, (int)slen(buf), 0, glob<void *>(G_HEARTBEAT_ADDRESS), 0x10);
    r->last_heartbeat = tick();
}

// 0x5c8fb0: both sockets closed, a malloc'ed record freed
void __cdecl qr_shutdown(QrRecord *r)
{
    if (!r)
        r = default_record();
    if (r->query_socket != 0xffffffffu)
        ws_closesocket(r->query_socket);
    if (r->heartbeat_socket != 0xffffffffu && r->heartbeat_socket != r->query_socket)
        ws_closesocket(r->heartbeat_socket);
    r->heartbeat_socket = 0xffffffffu;
    r->query_socket = 0xffffffffu;
    r->last_heartbeat = 0;
    if (r != glob<QrRecord *>(STATIC_RECORD))
        crt_free(r);
    SocketShutDown();
}

// 0x5c9010
char *__cdecl qr_value_for_key(const char *query, const char *key) { return value_for_key(query, key); }

// 0x5c9110: base64 with the standard alphabet, a short last group padded with zero bits (no '='), a terminator
void __cdecl gs_encode(const BYTE *in, int n, char *out)
{
    if (n <= 0) {
        *out = 0;
        return;
    }
    int done = 0;
    while (done < n) {
        BYTE b[3], c[4];
        for (int i = 0; i <= 2; i++, done++)
            b[i] = done < n ? *in++ : 0;
        c[0] = b[0] >> 2;
        c[1] = (BYTE)((b[0] & 3) << 4) + (b[1] >> 4);
        c[2] = (BYTE)((b[1] & 0xf) << 2) + (b[2] >> 6);
        c[3] = b[2] & 0x3f;
        for (int i = 0; i <= 3; i++) {
            const BYTE v = c[i];
            *out++ = (char)(v < 26 ? v + 'A' : v < 52 ? v + 'a' - 26 : v < 62 ? v - 4 : v == 62 ? '+' : v == 63 ? '/' : 0);
        }
    }
    *out = 0;
}

// 0x5c91e0: RC4's key schedule over the key; then each byte: i += data byte + 1, j += s[i], swap, data ^= s[s[i] + s[j]]
void __cdecl gs_encrypt(const BYTE *key, int key_length, BYTE *data, int n)
{
    BYTE s[256];
    for (short i = 0; i < 0x100; i++)
        s[i] = (BYTE)i;
    BYTE k = 0;
    DWORD j = 0;
    for (int i = 0; i < 0x100; i++) {
        j = (s[i] + j + key[k]) & 0xff;
        k = (BYTE)((int)(k + 1) % key_length);
        const BYTE t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
    BYTE x = 0, y = 0;
    for (short m = 0; m < n; m++) {
        x = (BYTE)(data[m] + x + 1);
        y = (BYTE)(s[x] + y);
        const BYTE t = s[x];
        s[x] = s[y];
        s[y] = t;
        data[m] ^= s[(BYTE)(s[y] + s[x])];
    }
}

// 0x5c9340
void __cdecl add_queryid(QrRecord *r, const SockAddr *to, char *out) { flush(r, to, out); }

// 0x5c93e0: data appended to the answer buffer; when both reach 0x546 the buffer is flushed first, and data longer
// than 0x546 is split at the last odd-numbered '\' before 0x546 (sent through a recursive call), nothing when there
// is none
void __cdecl packet_send(QrRecord *r, const SockAddr *to, char *out, char *data)
{
    while (slen(out) + slen(data) >= 0x546) {
        if (slen(data) <= 0x546) {
            flush(r, to, out);
            scpy(out, data);
            return;
        }
        char *cut = data;
        int slashes = 0;
        for (char *p = data; p - data < 0x546; p++)
            if (*p == '\\') {
                if (slashes % 2 == 0)
                    cut = p;
                slashes++;
            }
        if (cut == data)
            return;
        *cut = 0;
        packet_send_at(r, to, out, data);
        *cut = '\\';
        data = cut;
    }
    scat(out, data);
}

// 0x5c95a0, 0x5c9610, 0x5c9680, 0x5c96f0: the callback's answer (a cleared 0x578-byte buffer) through packet_send
void __cdecl send_basic(QrRecord *r, const SockAddr *to, char *out) { send_callback(r, r->basic, to, out); }
void __cdecl send_info(QrRecord *r, const SockAddr *to, char *out) { send_callback(r, r->info, to, out); }
void __cdecl send_rules(QrRecord *r, const SockAddr *to, char *out) { send_callback(r, r->rules, to, out); }
void __cdecl send_players(QrRecord *r, const SockAddr *to, char *out) { send_callback(r, r->players, to, out); }

// 0x5c9760: "\echo\<value>" for a value of at most 0x546 characters
void __cdecl send_echo(QrRecord *r, const SockAddr *to, char *out, const char *value)
{
    if (slen(value) > 0x546)
        return;
    char buf[BUFFER];
    crt_sprintf(buf, text(glob_addr(0x6280fc)), value);
    packet_send_at(r, to, out, buf);
}

// 0x5c97c0: "\validate\..." for a challenge (a challenge over 0x80 characters ends the answer without even
// "\final\"), "\final\", the buffer flushed
void __cdecl send_final(QrRecord *r, const SockAddr *to, char *out, const char *challenge)
{
    char buf[0x100];
    if (*challenge) {
        const int n = (int)slen(challenge);
        if (n > 0x80)
            return;
        BYTE data[0x84];
        char coded[200];
        scpy((char *)data, challenge);
        fn<void(__cdecl *)(const char *, DWORD, BYTE *, int)>(0x5c91e0)(r->secret_key, slen(r->secret_key), data, n);
        fn<void(__cdecl *)(const BYTE *, int, char *)>(0x5c9110)(data, n, coded);
        crt_sprintf(buf, text(glob_addr(0x628148)), coded);   // "\validate\%s"
        packet_send_at(r, to, out, buf);
    }
    crt_sprintf(buf, text(glob_addr(0x627f2c)));   // "\final\"
    packet_send_at(r, to, out, buf);
    flush(r, to, out);
}

// ---- nonport.c ----------------------------------------------------------------------------------------------

// 0x5c9a80
void __cdecl msleep_(DWORD ms) { sleep_ms(ms); }
// 0x5c9a90: WSAStartup(1.1)
void __cdecl SocketStartUp_()
{
    BYTE data[0x190];
    ws_startup(0x101, data);
}
// 0x5c9ab0: WSACleanup through its thunk (a jump, no frame)
extern "C" void SocketShutDown_();
__asm__(".text\n_SocketShutDown_:\n  jmp *" ZAX_ASM_DATA(0x5ea2f0) "\n");

// ---- darray.c -------------------------------------------------------------------------------------------------

// 0x5c9c20: an empty array (n slots, 8 when 0; grows by n)
DArray *__cdecl ArrayNew_(int elem_size, int n, ElemFree free_fn)
{
    DArray *a = (DArray *)crt_malloc(0x18);
    if (!n)
        n = 8;
    a->elem_size = elem_size;
    a->count = 0;
    a->capacity = n;
    a->grow_by = n;
    a->free_fn = free_fn;
    a->list = (BYTE *)crt_malloc((DWORD)(elem_size * n));
    return a;
}
// 0x5c9c70: every element through the free function, the list and the array freed
void __cdecl ArrayFree_(DArray *a)
{
    for (int i = 0; i < a->count; i++)
        if (a->free_fn)
            a->free_fn(a->list + a->elem_size * i);
    crt_free(a->list);
    crt_free(a);
}
// 0x5c9cc0, 0x5c9cd0
int __cdecl ArrayLength_(DArray *a) { return a->count; }
void *__cdecl ArrayNth_(DArray *a, int n) { return a->list + a->elem_size * n; }
// 0x5c9cf0: ArrayInsertAt(a, e, count) inlined (the memmove for an insertion before the end cannot happen)
void __cdecl ArrayAppend_(DArray *a, const void *e)
{
    const int n = a->count;
    if (n == a->capacity) {
        a->capacity += a->grow_by;
        a->list = (BYTE *)crt_realloc(a->list, (DWORD)(a->elem_size * a->capacity));
    }
    const int count = ++a->count;
    if (n < count - 1)
        crt_memmove(a->list + (n + 1) * a->elem_size, a->list + a->elem_size * n, (DWORD)((count - n - 1) * a->elem_size));
    bcopy(a->list + a->elem_size * n, e, (DWORD)a->elem_size);
}
// 0x5c9d80: element n freed and replaced
void __cdecl ArrayReplaceAt_(DArray *a, const void *e, int n)
{
    if (a->free_fn)
        a->free_fn(a->list + a->elem_size * n);
    bcopy(a->list + a->elem_size * n, e, (DWORD)a->elem_size);
}
// 0x5c9dd0: the index of the element matching key from `from` on (linear: compare(key, element); sorted: binary,
// compare(element, key), the first of equal ones), -1 when none
int __cdecl ArraySearch_(DArray *a, const void *key, ElemCompare compare, int from, int sorted)
{
    if (!a || !a->count)
        return -1;
    BYTE *found;
    int match = 1;
    if (!sorted) {
        const int size = a->elem_size, n = a->count - from;
        BYTE *base = a->list + size * from, *p = base;
        int i;
        for (i = 0; i < n; i++, p += size)
            if (compare(key, p) == 0)
                break;
        if (i >= n)
            return -1;
        found = base + i * size;
    } else {
        const int size = a->elem_size;
        BYTE *base = a->list + size * from;
        int low = 0, high = a->count - from - 1;
        match = 0;
        while (low <= high) {
            const int mid = (low + high) >> 1;
            const int c = compare(base + mid * size, key);
            if (c == 0)
                match = 1;
            if (c >= 0)
                high = mid - 1;
            else
                low = mid + 1;
        }
        found = base + low * size;
    }
    if (!found || !match)
        return -1;
    return (int)(found - a->list) / a->elem_size;
}

// ---- hashtable.c ----------------------------------------------------------------------------------------------

// 0x5c9ac0: nbuckets arrays of nchains slots
HashTable *__cdecl TableNew2_(int elem_size, int nbuckets, int nchains, ElemHash hash, ElemCompare compare, ElemFree free_fn)
{
    HashTable *t = (HashTable *)crt_malloc(0x14);
    t->buckets = (DArray **)crt_malloc((DWORD)nbuckets * 4);
    for (int i = 0; i < nbuckets; i++)
        t->buckets[i] = ArrayNew(elem_size, nchains, free_fn);
    t->hash_fn = hash;
    t->nbuckets = nbuckets;
    t->free_fn = free_fn;
    t->compare_fn = compare;
    return t;
}
// 0x5c9b30
void __cdecl TableFree_(HashTable *t)
{
    for (int i = 0; i < t->nbuckets; i++)
        ArrayFree(t->buckets[i]);
    crt_free(t->buckets);
    crt_free(t);
}
// 0x5c9b70: appended to its bucket, or replacing the equal element there
void __cdecl TableEnter_(HashTable *t, const void *e)
{
    const int h = t->hash_fn(e, t->nbuckets);
    const int i = ArraySearch(t->buckets[h], e, t->compare_fn, 0, 0);
    if (i == -1)
        ArrayAppend(t->buckets[h], e);
    else
        ArrayReplaceAt(t->buckets[h], e, i);
}
// 0x5c9bd0: the equal element, or null
void *__cdecl TableLookup_(HashTable *t, const void *e)
{
    const int h = t->hash_fn(e, t->nbuckets);
    const int i = ArraySearch(t->buckets[h], e, t->compare_fn, 0, 0);
    if (i == -1)
        return nullptr;
    return ArrayNth(t->buckets[h], i);
}

}  // namespace gamespy

ZAX_HOOKS(gamespy_qr)
{
    using namespace gamespy;
    const Hook list[] = {
#include "gamespy_qr_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
