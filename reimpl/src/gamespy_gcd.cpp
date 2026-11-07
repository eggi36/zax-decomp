// The GameSpy CD key SDK of Zax.exe (phase 5b; docs/DECOMPILATION.md 11, "GameSpy SDK (2001)"): the server side
// 0x5c5280-0x5c6060 (gcd_init, gcd_shutdown, gcd_authenticate_user, gcd_disconnect_user, gcd_think and its three
// helpers), the client side gcd_compute_response 0x5c9960 and RSA's MD5 0x5c9ee0-0x5ca8b0 (MD5Update, MD5Transform,
// MD5Digest = MD5Init + MD5Update + MD5Final + "%02x" text). Conventions: gamespy.h.
//
// State [confirmed: code]: the UDP socket 0x627d30 (-1 when closed), the key server's address 0x714ba0 (16 bytes;
// port htons(0xffff) in this exe, the crack's change: the original sends to port 29910, EXE_FINDINGS.md "Exe
// variants"), the local port 0x71499c (getsockname), the game id 0x714994 (409 from GameSpy_Start), the clients as a
// doubly linked list whose head node is 0x714bb0 (data, next 0x714bb4, prev 0x714bb8), the xor key "gamespy"
// 0x714988 (written byte by byte by gcd_init, every message to and from the key server is xored with it), the
// receive buffer 0x714588 (0x400) and gcd_value_for_key's two result buffers 0x7149a0 (2 x 256, alternating by
// 0x714998).
//
// A client (0x50 bytes): +0 local id, +4 the CD key hash (the response's first 32 characters), +0x28 the session key
// skey ((rand ^ GetTickCount) & 0x3fff), +0x2c the user's IP, +0x30 the time of the last request, +0x34 the number
// of requests sent, +0x38 the state (0 waiting, 1 validated, 2 rejected, 3 done), +0x3c the instance, +0x40 the
// callback (cdecl: local id, authenticated, message, instance), +0x44 the error message (_strdup), +0x48/+0x4c the
// request (malloc) and its length, resent every 2 s up to 3 times; after the third a timeout counts as validated
// ("Validation Timeout", authenticated 1).
#include "gamespy.h"

namespace zax {
namespace gamespy {

namespace {

constexpr DWORD G_SOCKET = 0x627d30, G_LOCAL_PORT = 0x71499c, G_GAME_ID = 0x714994, G_SERVER = 0x714ba0,
                G_HEAD = 0x714bb0, G_KEY = 0x714988, G_RECV = 0x714588, G_VALUE_INDEX = 0x714998, G_VALUES = 0x7149a0;

struct Client {
    int local_id;
    char cd[0x24];
    int skey;
    DWORD ip;
    DWORD sent_time;
    int tries;
    int state;
    void *instance;
    void(__cdecl *auth)(int, int, const char *, void *);
    char *error;
    char *request;
    int request_length;
};
static_assert(sizeof(Client) == 0x50);
struct Node {
    Client *data;
    Node *next;
    Node *prev;
};

DWORD &sock() { return *glob<DWORD *>(G_SOCKET); }
Node &head() { return *glob<Node *>(G_HEAD); }
int game_id() { return *glob<int *>(G_GAME_ID); }
const char *text(DWORD a) { return at<const char *>(a); }

// The message xored with the key "gamespy", cyclically
void xor_key(char *buf, int n)
{
    const char *k = glob<const char *>(G_KEY);
    for (int i = 0; i < n; i++) {
        buf[i] ^= *k;
        k = k[1] ? k + 1 : glob<const char *>(G_KEY);
    }
}
int send_to_server(const char *buf, int n) { return ws_sendto(sock(), buf, n, 0, glob<void *>(G_SERVER), 0x10); }

// The list macro's removal: the node or, when null, the first node
void unlink(Node *n)
{
    Node *e = n ? n : head().next;
    if (!e)
        return;
    e->prev->next = e->next;
    if (e->next)
        e->next->prev = e->prev;
}

// "\disc\\pid\%d\cd\%s\ip\%d" for the client of a node, then the node removed and freed
void disconnect(Node *n)
{
    char buf[512];
    const int len = crt_sprintf(buf, text(glob_addr(0x627d44)), game_id(), n->data->cd, n->data->ip);
    xor_key(buf, len);
    send_to_server(buf, len);
    unlink(n);
    if (n->data->request)
        crt_free(n->data->request);
    crt_free(n->data);
    crt_free(n);
}

// gcd_value_for_key's code (also inlined into gcd_process_ison): "\key\" searched in the message, the value up to
// the next '\' into the next of the two buffers; "" when the key is missing
char *value_for_key(const char *msg, const char *key)
{
    char search[256];
    search[0] = '\\';
    bzero(search + 1, 255);
    DWORD &index = *glob<DWORD *>(G_VALUE_INDEX);
    index ^= 1;
    scat(search, key);
    scat(search, text(glob_addr(BACKSLASH_TEXT)));
    const char *p = crt_strstr(msg, search);
    if (!p)
        return empty();
    p += slen(search);
    char *out = at<char *>(glob_addr(G_VALUES) + (index << 8));
    while (*p && *p != '\\')
        *out++ = *p++;
    *out = 0;
    return at<char *>(glob_addr(G_VALUES) + (index << 8));
}

char *gcd_value_for_key_at(const char *msg, const char *key) { return fn<char *(__cdecl *)(const char *, const char *)>(0x5c5f50)(msg, key); }
Client *gcd_find_client_at(const char *cd) { return fn<Client *(__cdecl *)(const char *)>(0x5c5c40)(cd); }

}  // namespace

// 0x5c5280: the socket bound to a free port, the key server's address, the list emptied, srand(GetTickCount()),
// the xor key; 0 or -1 (socket, bind or getsockname failed)
int __cdecl gcd_init(int game)
{
    SocketStartUp();
    sock() = ws_socket(2, 2, 0x11);
    if (sock() == 0xffffffffu)
        return -1;
    SockAddr local;
    local.addr = 0;
    *(DWORD *)local.zero = 0;
    local.family = 2;
    *(DWORD *)(local.zero + 4) = 0;
    local.port = ws_htons(0);
    local.addr = 0;
    if (ws_bind(sock(), &local, 0x10) == -1)
        return -1;
    int length = 0x10;
    if (ws_getsockname(sock(), &local, &length) == -1)
        return -1;
    SockAddr &server = *glob<SockAddr *>(G_SERVER);
    *(DWORD *)&server = 0;
    server.addr = 0;
    *glob<WORD *>(G_LOCAL_PORT) = local.port;
    *(DWORD *)server.zero = 0;
    server.family = 2;
    *(DWORD *)(server.zero + 4) = 0;
    server.port = ws_htons(0xffffffffu);
    server.addr = resolve(text(glob_addr(0x627cf0)));   // "key.gamespy.com"
    head().next = nullptr;
    *glob<int *>(G_GAME_ID) = game;
    head().prev = nullptr;
    head().data = nullptr;
    crt_srand(tick());
    char *k = glob<char *>(G_KEY);
    k[7] = 0;
    k[0] = 'g', k[1] = 'a', k[2] = 'm', k[3] = 'e', k[4] = 's', k[5] = 'p', k[6] = 'y';
    return 0;
}

// 0x5c5440: every client disconnected ("\disc\"), the socket closed
void __cdecl gcd_shutdown()
{
    for (Node *n = head().next; n; n = head().next)
        disconnect(n);
    ws_closesocket(sock());
    sock() = 0xffffffffu;
    SocketShutDown();
}

// 0x5c5540: a new client with the response's CD key hash; a response shorter than 72 characters gives "Bad CD Key", a
// hash already in the list "CD Key in use" (state 2, no request); otherwise the "\auth\" request is sent and kept
void __cdecl gcd_authenticate_user(int local_id, DWORD ip, const char *challenge, const char *response,
                                   void(__cdecl *auth)(int, int, const char *, void *), void *instance)
{
    char *error = nullptr;
    char cd[0x21];
    crt_strncpy(cd, response, 0x20);
    cd[0x20] = 0;
    if (slen(response) < 0x48)
        error = crt_strdup(text(glob_addr(0x627d9c)));   // "Bad CD Key"
    for (Node *n = head().next; n; n = n->next)
        if (scmp(cd, n->data->cd) == 0) {
            error = crt_strdup(text(glob_addr(0x627d8c)));   // "CD Key in use"
            break;
        }
    Client *c = (Client *)crt_malloc(0x50);
    c->local_id = local_id;
    c->ip = ip;
    c->auth = auth;
    c->instance = instance;
    c->error = nullptr;
    c->request = nullptr;
    scpy(c->cd, cd);
    Node *node = (Node *)crt_malloc(0xc);
    node->data = c;
    Node *last = &head();
    while (last->next)
        last = last->next;
    last->next = node;
    node->prev = last;
    node->next = nullptr;
    if (error) {
        c->state = 2;
        c->error = error;
        return;
    }
    c->state = 0;
    const int r = crt_rand();
    c->skey = (r ^ (int)tick()) & 0x3fff;
    c->sent_time = tick();
    c->tries = 1;
    char buf[512];
    const int len = crt_sprintf(buf, text(glob_addr(0x627d60)), game_id(), challenge, response, c->ip, c->skey);
    xor_key(buf, len);
    send_to_server(buf, len);
    c->request = (char *)crt_malloc((DWORD)len);
    crt_memmove(c->request, buf, (DWORD)len);
    c->request_length = len;
}

// 0x5c5750: the client with this local id disconnected ("\disc\") and removed
void __cdecl gcd_disconnect_user(int local_id)
{
    for (Node *n = head().next; n; n = n->next)
        if (n->data->local_id == local_id) {
            disconnect(n);
            return;
        }
}

// 0x5c5840: the key server's answers ("uok": validated, "unok": rejected with "errmsg", "ison": the "\uon\" /
// "\uoff\" answer for a CD key) while select says there are some, then the clients: requests resent, the callback
// called for validated (or timed out) and rejected clients, rejected ones removed
void __cdecl gcd_think()
{
    FdSet set;
    TimeVal timeout;
    SockAddr from;
    int from_length = 0x10;
    timeout.sec = 0;
    timeout.usec = 0;
    set.sockets[0] = sock();
    set.count = 1;
    char *const buf = glob<char *>(G_RECV);
    for (;;) {
        const int ready = ws_select(0x40, &set, nullptr, nullptr, &timeout);
        if (ready == -1 || ready == 0)
            break;
        const int len = ws_recvfrom(sock(), buf, 0x3ff, 0, &from, &from_length);
        if (len == -1)
            continue;
        buf[len] = 0;
        xor_key(buf, len);
        char type[0x21];
        type[0] = 0;
        if (buf[0] != '\\')
            continue;
        const int end = (int)((DWORD)(uintptr_t)crt_strchr(buf + 1, '\\') - glob_addr(G_RECV));   // a missing '\\' gives a huge count (crash)
        if (end <= 0x20) {
            crt_strncpy(type, buf + 1, (DWORD)(end - 1));
            type[end - 1] = 0;
        }
        if (!type[0])
            continue;
        if (scmp(type, text(glob_addr(0x627dec))) == 0) {   // "uok"
            const int skey = crt_atoi(gcd_value_for_key_at(buf, text(glob_addr(0x627de4))));   // "skey"
            char cd[0x24];
            crt_strncpy(cd, gcd_value_for_key_at(buf, text(glob_addr(0x627de0))), 0x20);   // "cd"
            cd[0x20] = 0;
            Client *c = gcd_find_client_at(cd);
            if (c && c->skey == skey && c->state != 3)
                c->state = 1;
        } else if (scmp(type, text(glob_addr(0x627dd8))) == 0) {   // "unok"
            const int skey = crt_atoi(gcd_value_for_key_at(buf, text(glob_addr(0x627de4))));
            char cd[0x24];
            crt_strncpy(cd, gcd_value_for_key_at(buf, text(glob_addr(0x627de0))), 0x20);
            cd[0x20] = 0;
            Client *c = gcd_find_client_at(cd);
            if (c && c->skey == skey && c->state != 3) {
                c->state = 2;
                c->error = crt_strdup(gcd_value_for_key_at(buf, text(glob_addr(0x627dd0))));   // "errmsg"
            }
        } else if (scmp(type, text(glob_addr(0x627dc8))) == 0) {   // "ison"
            fn<void(__cdecl *)(const char *, const SockAddr *)>(0x5c5ca0)(buf, &from);
        }
    }
    for (Node *n = head().next; n; n = n->next) {
        Client *c = n->data;
        switch (c->state) {
        case 0:
            if (tick() < c->sent_time + 2000)
                break;
            if (c->tries <= 2) {
                c->sent_time = tick();
                c->tries++;
                ws_sendto(sock(), c->request, c->request_length, 0, glob<void *>(G_SERVER), 0x10);
                break;
            }
            [[fallthrough]];
        case 1:
            c->auth(c->local_id, 1, text(c->state == 1 ? glob_addr(0x627dbc) : glob_addr(0x627da8)), c->instance);   // "Validated", "Validation Timeout"
            n->data->state = 3;
            crt_free(n->data->request);
            n->data->request = nullptr;
            break;
        case 2: {
            Node *prev = n->prev;
            unlink(n);
            c->auth(c->local_id, 0, c->error ? c->error : empty(), c->instance);
            crt_free(n->data->request);
            if (n->data->error)
                crt_free(n->data->error);
            crt_free(n->data);
            crt_free(n);
            n = prev;
            break;
        }
        }
    }
}

// 0x5c5c40: the client whose CD key hash is this one, or null
Client *__cdecl gcd_find_client(const char *cd)
{
    for (Node *n = head().next; n; n = n->next)
        if (scmp(cd, n->data->cd) == 0)
            return n->data;
    return nullptr;
}

// 0x5c5ca0: the key server asks whether a CD key is in use: "\uon\\skey\%d" when a validated (done) client has it,
// otherwise "\uoff\\skey\%d", sent back to the asking address
void __cdecl gcd_process_ison(const char *msg, const SockAddr *from)
{
    const int skey = crt_atoi(value_for_key(msg, text(glob_addr(0x627de4))));   // "skey"
    const char *cd = value_for_key(msg, text(glob_addr(0x627de0)));              // "cd"
    const char *fmt = text(glob_addr(0x627e00));                                 // "\uoff\\skey\%d"
    for (Node *n = head().next; n; n = n->next)
        if (scmp(cd, n->data->cd) == 0) {
            if (n->data && n->data->state == 3)
                fmt = text(glob_addr(0x627df0));   // "\uon\\skey\%d"
            break;
        }
    char buf[0x40];
    const int len = crt_sprintf(buf, fmt, skey);
    xor_key(buf, len);
    ws_sendto(sock(), buf, len, 0, from, 0x10);
}

// 0x5c5f50
char *__cdecl gcd_value_for_key(const char *msg, const char *key) { return value_for_key(msg, key); }

// ---- The client side ------------------------------------------------------------------------------------------

// 0x5c9960: the 72-character response to a challenge: MD5 of the CD key (32 hex digits), a random number as
// "%.8x" (srand(time ^ 0x33333333), rand << 16 | rand) and MD5 of "<key><number % 0xffff><challenge>"; too long
// inputs give "CD Key or challenge too long"
void __cdecl gcd_compute_response(const char *cd_key, const char *challenge, char *out)
{
    const DWORD nk = slen(cd_key), nc = slen(challenge);
    if (nc + nk * 2 + 8 >= 0x200) {
        bcopy(out, text(glob_addr(0x628168)), 29);
        return;
    }
    crt_srand(crt_time(nullptr) ^ 0x33333333u);
    DWORD r = (DWORD)crt_rand() << 16;
    r |= (DWORD)crt_rand();
    char number[12];
    char buf[512];
    crt_sprintf(number, text(glob_addr(0x628160)), r);   // "%.8x"
    crt_sprintf(buf, text(glob_addr(0x628158)), cd_key, r % 0xffff, challenge);   // "%s%d%s"
    fn<void(__cdecl *)(const char *, DWORD, char *)>(0x5ca8b0)(cd_key, slen(cd_key), out);
    scpy(out + 0x20, number);
    fn<void(__cdecl *)(const char *, DWORD, char *)>(0x5ca8b0)(buf, slen(buf), out + 0x28);
}

// ---- MD5 (RSA's md5c.c) ---------------------------------------------------------------------------------------

namespace {

struct Md5 {
    DWORD state[4];
    DWORD count[2];
    BYTE buffer[64];
};

inline DWORD rol(DWORD x, int n) { return x << n | x >> (32 - n); }
#define MD5_STEP(f, a, b, c, d, x, s, ac) (a) = rol((a) + f((b), (c), (d)) + (x) + (DWORD)(ac), (s)) + (b)
#define MD5_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | ~(z)))

void transform_at(DWORD *state, const BYTE *block) { fn<void(__cdecl *)(DWORD *, const BYTE *)>(0x5c9fa0)(state, block); }
void update_at(Md5 *ctx, const BYTE *in, DWORD n) { fn<void(__cdecl *)(Md5 *, const BYTE *, DWORD)>(0x5c9ee0)(ctx, in, n); }

}  // namespace

// 0x5c9ee0
void __cdecl MD5Update(Md5 *ctx, const BYTE *in, DWORD n)
{
    DWORD index = ctx->count[0] >> 3 & 0x3f;
    if ((ctx->count[0] += n << 3) < (n << 3))
        ctx->count[1]++;
    ctx->count[1] += n >> 29;
    const DWORD part = 64 - index;
    DWORD i;
    if (n >= part) {
        bcopy(ctx->buffer + index, in, part);
        transform_at(ctx->state, ctx->buffer);
        for (i = part; i + 63 < n; i += 64)
            transform_at(ctx->state, in + i);
        index = 0;
    } else {
        i = 0;
    }
    bcopy(ctx->buffer + index, in + i, n - i);
}

// 0x5c9fa0
void __cdecl MD5Transform(DWORD *state, const BYTE *block)
{
    DWORD a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0; i < 16; i++)
        x[i] = (DWORD)block[i * 4] | (DWORD)block[i * 4 + 1] << 8 | (DWORD)block[i * 4 + 2] << 16 | (DWORD)block[i * 4 + 3] << 24;
    MD5_STEP(MD5_F, a, b, c, d, x[0], 7, 0xd76aa478);
    MD5_STEP(MD5_F, d, a, b, c, x[1], 12, 0xe8c7b756);
    MD5_STEP(MD5_F, c, d, a, b, x[2], 17, 0x242070db);
    MD5_STEP(MD5_F, b, c, d, a, x[3], 22, 0xc1bdceee);
    MD5_STEP(MD5_F, a, b, c, d, x[4], 7, 0xf57c0faf);
    MD5_STEP(MD5_F, d, a, b, c, x[5], 12, 0x4787c62a);
    MD5_STEP(MD5_F, c, d, a, b, x[6], 17, 0xa8304613);
    MD5_STEP(MD5_F, b, c, d, a, x[7], 22, 0xfd469501);
    MD5_STEP(MD5_F, a, b, c, d, x[8], 7, 0x698098d8);
    MD5_STEP(MD5_F, d, a, b, c, x[9], 12, 0x8b44f7af);
    MD5_STEP(MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1);
    MD5_STEP(MD5_F, b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_STEP(MD5_F, a, b, c, d, x[12], 7, 0x6b901122);
    MD5_STEP(MD5_F, d, a, b, c, x[13], 12, 0xfd987193);
    MD5_STEP(MD5_F, c, d, a, b, x[14], 17, 0xa679438e);
    MD5_STEP(MD5_F, b, c, d, a, x[15], 22, 0x49b40821);
    MD5_STEP(MD5_G, a, b, c, d, x[1], 5, 0xf61e2562);
    MD5_STEP(MD5_G, d, a, b, c, x[6], 9, 0xc040b340);
    MD5_STEP(MD5_G, c, d, a, b, x[11], 14, 0x265e5a51);
    MD5_STEP(MD5_G, b, c, d, a, x[0], 20, 0xe9b6c7aa);
    MD5_STEP(MD5_G, a, b, c, d, x[5], 5, 0xd62f105d);
    MD5_STEP(MD5_G, d, a, b, c, x[10], 9, 0x02441453);
    MD5_STEP(MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681);
    MD5_STEP(MD5_G, b, c, d, a, x[4], 20, 0xe7d3fbc8);
    MD5_STEP(MD5_G, a, b, c, d, x[9], 5, 0x21e1cde6);
    MD5_STEP(MD5_G, d, a, b, c, x[14], 9, 0xc33707d6);
    MD5_STEP(MD5_G, c, d, a, b, x[3], 14, 0xf4d50d87);
    MD5_STEP(MD5_G, b, c, d, a, x[8], 20, 0x455a14ed);
    MD5_STEP(MD5_G, a, b, c, d, x[13], 5, 0xa9e3e905);
    MD5_STEP(MD5_G, d, a, b, c, x[2], 9, 0xfcefa3f8);
    MD5_STEP(MD5_G, c, d, a, b, x[7], 14, 0x676f02d9);
    MD5_STEP(MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8a);
    MD5_STEP(MD5_H, a, b, c, d, x[5], 4, 0xfffa3942);
    MD5_STEP(MD5_H, d, a, b, c, x[8], 11, 0x8771f681);
    MD5_STEP(MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122);
    MD5_STEP(MD5_H, b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_STEP(MD5_H, a, b, c, d, x[1], 4, 0xa4beea44);
    MD5_STEP(MD5_H, d, a, b, c, x[4], 11, 0x4bdecfa9);
    MD5_STEP(MD5_H, c, d, a, b, x[7], 16, 0xf6bb4b60);
    MD5_STEP(MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_STEP(MD5_H, a, b, c, d, x[13], 4, 0x289b7ec6);
    MD5_STEP(MD5_H, d, a, b, c, x[0], 11, 0xeaa127fa);
    MD5_STEP(MD5_H, c, d, a, b, x[3], 16, 0xd4ef3085);
    MD5_STEP(MD5_H, b, c, d, a, x[6], 23, 0x04881d05);
    MD5_STEP(MD5_H, a, b, c, d, x[9], 4, 0xd9d4d039);
    MD5_STEP(MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_STEP(MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8);
    MD5_STEP(MD5_H, b, c, d, a, x[2], 23, 0xc4ac5665);
    MD5_STEP(MD5_I, a, b, c, d, x[0], 6, 0xf4292244);
    MD5_STEP(MD5_I, d, a, b, c, x[7], 10, 0x432aff97);
    MD5_STEP(MD5_I, c, d, a, b, x[14], 15, 0xab9423a7);
    MD5_STEP(MD5_I, b, c, d, a, x[5], 21, 0xfc93a039);
    MD5_STEP(MD5_I, a, b, c, d, x[12], 6, 0x655b59c3);
    MD5_STEP(MD5_I, d, a, b, c, x[3], 10, 0x8f0ccc92);
    MD5_STEP(MD5_I, c, d, a, b, x[10], 15, 0xffeff47d);
    MD5_STEP(MD5_I, b, c, d, a, x[1], 21, 0x85845dd1);
    MD5_STEP(MD5_I, a, b, c, d, x[8], 6, 0x6fa87e4f);
    MD5_STEP(MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_STEP(MD5_I, c, d, a, b, x[6], 15, 0xa3014314);
    MD5_STEP(MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_STEP(MD5_I, a, b, c, d, x[4], 6, 0xf7537e82);
    MD5_STEP(MD5_I, d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_STEP(MD5_I, c, d, a, b, x[2], 15, 0x2ad7d2bb);
    MD5_STEP(MD5_I, b, c, d, a, x[9], 21, 0xeb86d391);
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

// 0x5ca8b0: the digest of n bytes as 32 lower-case hex digits (sprintf "%02x" per byte) and a terminator
void __cdecl MD5Digest(const BYTE *in, DWORD n, char *out)
{
    Md5 ctx;
    ctx.count[1] = 0;
    ctx.count[0] = 0;
    ctx.state[0] = 0x67452301;
    ctx.state[1] = 0xefcdab89;
    ctx.state[2] = 0x98badcfe;
    ctx.state[3] = 0x10325476;
    update_at(&ctx, in, n);
    BYTE bits[8], digest[16];
    for (int i = 0; i < 8; i += 4)
        for (int b = 0; b < 4; b++)
            bits[i + b] = (BYTE)(ctx.count[i / 4] >> (8 * b));
    const DWORD index = ctx.count[0] >> 3 & 0x3f;
    update_at(&ctx, glob<const BYTE *>(0x62818c), (index < 56 ? 56 : 120) - index);
    update_at(&ctx, bits, 8);
    for (int i = 0; i < 16; i += 4)
        for (int b = 0; b < 4; b++)
            digest[i + b] = (BYTE)(ctx.state[i / 4] >> (8 * b));
    bzero(&ctx, 0x58);
    for (int i = 0; i < 16; i++, out += 2)
        crt_sprintf(out, text(glob_addr(0x6281cc)), (DWORD)digest[i]);   // "%02x"
}

}  // namespace gamespy

ZAX_HOOKS(gamespy_gcd)
{
    using namespace gamespy;
    const Hook list[] = {
#include "gamespy_gcd_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
