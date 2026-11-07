// Checks of the GameSpy CD key SDK and MD5 (gamespy_gcd.cpp) against the original through the trampolines, the
// hooks staying in (checks_gamespy_common.h). Cases: gcd_init with failing socket, bind, getsockname and name
// lookups; random clients (responses of every length, CD keys used twice) through gcd_authenticate_user, the key
// server's answers (uok, unok, a wrong skey, "ison", garbage, failed receives) and the resend timer through gcd_think
// with a scripted clock, gcd_disconnect_user, the helpers directly, gcd_shutdown; gcd_compute_response with the
// scripted time and rand (also inputs too long, and one fixed input); MD5Update/MD5Transform on random contexts,
// MD5Digest on random data and RFC 1321's test suite (the digests compared with the RFC's values on both sides).
// Compared: results, the call log (Winsock, clock, rand, callbacks with their arguments and the sent bytes) and the
// SDK's globals with the client list walked.
#include "checks_gamespy_common.h"

namespace zax {

namespace {

using namespace gscheck;

struct Client {
    int local_id;
    char cd[0x24];
    int skey;
    DWORD ip;
    DWORD sent_time;
    int tries;
    int state;
    void *instance;
    void *auth;
    char *error;
    char *request;
    int request_length;
};
struct Node {
    Client *data;
    Node *next;
    Node *prev;
};

void __cdecl auth_cb(int id, int ok, const char *msg, void *instance)
{
    ev_raw(0x230, (DWORD)id, (DWORD)ok, hash_text(msg));
    count(!ok ? 3 : msg[0] == 'V' && msg[11] == 'T' ? 5 : 4);   // rejected, validated, timed out
    ev_raw(0x330, ptrv(instance));
}

void st_gcd()
{
    st_raw(*at<DWORD *>(0x627d30));
    st_raw(*at<WORD *>(0x71499c));
    st_raw(*at<DWORD *>(0x714994));
    st_bytes(at<void *>(0x714ba0), 16);
    st_bytes(at<void *>(0x714988), 8);
    st_raw(*at<DWORD *>(0x714998));
    st_bytes(at<void *>(0x7149a0), 0x200);
    st_bytes(at<void *>(0x714588), 0x400);
    Node *head = at<Node *>(0x714bb0);
    st_raw(head->data != nullptr);
    Node *prev = head;
    int n = 0;
    for (Node *e = head->next; e && n < 64; e = e->next, n++) {
        st_raw(e->prev == prev);
        Client *c = e->data;
        st_raw((DWORD)c->local_id);
        st_text(c->cd);
        st_raw(c->ip);
        st_raw((DWORD)c->state);
        if (c->state != 2) {   // a client rejected by gcd_authenticate_user keeps these as malloc left them
            st_raw((DWORD)c->skey);
            st_raw(c->sent_time);
            st_raw((DWORD)c->tries);
        }
        st_raw(ptrv(c->instance));
        st_raw(ptrv(c->auth));
        st_text(c->error);
        st_raw(c->request != nullptr);
        if (c->request)
            st_bytes(c->request, (DWORD)c->request_length);
        if (c->state != 2)
            st_raw((DWORD)c->request_length);
        prev = e;
    }
    st_raw((DWORD)n);
}

const char HEX[] = "0123456789abcdef";
void random_hex(char *out, int n)
{
    for (int i = 0; i < n; i++)
        out[i] = HEX[rnd(16)];
    out[n] = 0;
}

// A message from the key server: the plain text xored with "gamespy"
void add_gcd_packet(const char *plain)
{
    char b[0x300];
    const int n = (int)slen(plain);
    memcpy(b, plain, (DWORD)n + 1);
    xor_gamespy(b, n);
    add_packet(g_gcd_socket, b, n);
}

char g_keys[8][0x60];
int g_nkeys;

void random_gcd_messages()
{
    const int n = (int)rnd(4);
    for (int i = 0; i < n; i++) {
        char m[0x200] = "";
        const char *cd = g_nkeys && rnd(4) ? g_keys[rnd((DWORD)g_nkeys)] : "0123456789abcdef0123456789abcdef";
        char cd32[0x24];
        memcpy(cd32, cd, 32);
        cd32[32] = 0;
        switch (rnd(7)) {
        case 0:
        case 1:
            append(m, sizeof m, rnd(2) ? "\\uok\\" : "\\unok\\");
            append(m, sizeof m, "\\cd\\");
            append(m, sizeof m, cd32);
            append(m, sizeof m, "\\skey\\");
            append_number(m, sizeof m, rnd(0x4000));
            append(m, sizeof m, "\\errmsg\\Bad");
            break;
        case 2:
        case 3:
            append(m, sizeof m, "\\ison\\\\skey\\");
            append_number(m, sizeof m, rnd(100000));
            append(m, sizeof m, "\\cd\\");
            append(m, sizeof m, rnd(3) ? cd32 : "nothing");
            break;
        case 4:
            append(m, sizeof m, "\\");
            append(m, sizeof m, word());
            append(m, sizeof m, "\\x");
            break;
        case 5:
            append(m, sizeof m, "uok\\x\\");   // no leading '\'
            break;
        case 6:
            append(m, sizeof m, "\\abcdefghijklmnopqrstuvwxyz0123456789\\");   // a type over 0x20 characters
            break;
        }
        if (rnd(8) == 0)
            add_packet(g_gcd_socket, "", -1);
        else
            add_gcd_packet(m);
    }
}

void gcd_case(int k)
{
    if (rnd(12) == 0)
        g_fail_socket = 1;
    if (rnd(12) == 0)
        g_fail_bind = 1;
    if (rnd(12) == 0)
        g_getsockname_result = -1;
    g_host_found = rnd(3) != 0;
    g_clock_step = 1 + rnd(40);
    g_rand = random();
    const int game = k % 3 == 0 ? 409 : (int)rnd(2000);
    const int r = side_fn<int(__cdecl *)(int)>(0x5c5280)(game);
    ev_raw(0x100, (DWORD)r);
    st_gcd();
    if (r != 0) {
        side_fn<void(__cdecl *)()>(0x5c5440)();
        st_gcd();
        return;
    }
    g_gcd_socket = *at<DWORD *>(0x627d30);
    g_gcd_reply = (int)rnd(5);
    g_nkeys = 0;
    const int clients = (int)rnd(6);
    for (int i = 0; i < clients; i++) {
        char *resp = g_keys[g_nkeys < 8 ? g_nkeys : 7];
        if (g_nkeys > 0 && rnd(5) == 0)
            memcpy(resp, g_keys[rnd((DWORD)g_nkeys)], 0x60);
        else
            random_hex(resp, rnd(6) == 0 ? (int)rnd(72) : 72 + (int)rnd(8));
        if (g_nkeys < 8)
            g_nkeys++;
        char challenge[16];
        random_hex(challenge, 1 + (int)rnd(8));
        side_fn<void(__cdecl *)(int, DWORD, const char *, const char *, void *, void *)>(0x5c5540)(
            i + 1, 0x0100007f + rnd(4), challenge, resp, (void *)&auth_cb, (void *)(uintptr_t)(0x7700 + i));
    }
    const int steps = 1 + (int)rnd(6);
    for (int s = 0; s < steps; s++) {
        random_gcd_messages();
        g_clock += rnd(3) ? rnd(2600) : 0;
        Node *first = at<Node *>(0x714bb0)->next;
        if (first && first->data->state == 0 && rnd(3) == 0)   // the resend timer's boundary (2000 ms)
            clock_at(first->data->sent_time, 1999 + rnd(3));
        side_fn<void(__cdecl *)()>(0x5c5840)();
        if (rnd(4) == 0)
            side_fn<void(__cdecl *)(int)>(0x5c5750)((int)rnd((DWORD)clients + 2));
    }
    // The helpers directly
    char msg[0x200] = "";
    key_values(msg, sizeof msg, (int)rnd(5));
    if (rnd(2))
        append(msg, sizeof msg, "\\skey\\42\\cd\\0123");
    for (int i = 0; i < 3; i++) {
        const char *v = side_fn<char *(__cdecl *)(const char *, const char *)>(0x5c5f50)(msg, word());
        ev_raw(0x101, ptrv(v), hash_text(v));
    }
    const char *cd = g_nkeys && rnd(2) ? g_keys[rnd((DWORD)g_nkeys)] : "none";
    char cd32[0x24];
    memcpy(cd32, cd, 32);
    cd32[32] = 0;
    void *found = side_fn<void *(__cdecl *)(const char *)>(0x5c5c40)(cd32);
    int index = -1, i = 0;
    for (Node *e = at<Node *>(0x714bb0)->next; e; e = e->next, i++)
        if (e->data == found)
            index = i;
    ev_raw(0x102, found != nullptr, (DWORD)index);
    char ison[0x100] = "\\ison\\\\skey\\";
    append_number(ison, sizeof ison, rnd(1000));
    append(ison, sizeof ison, "\\cd\\");
    append(ison, sizeof ison, cd32);
    SockAddr from = {2, 0x1234, 0x0d00000a, {}};
    side_fn<void(__cdecl *)(const char *, const SockAddr *)>(0x5c5ca0)(ison, &from);
    st_gcd();
    side_fn<void(__cdecl *)()>(0x5c5440)();
    st_gcd();
}

// ---- gcd_compute_response and MD5 ---------------------------------------------------------------------------------

void response_case(int k)
{
    char key[0x200], challenge[0x200], out[0x80];
    if (k == 0) {
        memcpy(key, "3b8dd8995f7c40a9a5c5b7dd5b481341", 33);   // fixed input
        memcpy(challenge, "aX4p0Lm2", 9);
        g_time = 1000000000;
    } else {
        random_hex(key, rnd(8) == 0 ? (int)rnd(0x180) : (int)rnd(80));
        random_hex(challenge, rnd(8) == 0 ? (int)rnd(0x190) : (int)rnd(40));
        g_time = random();
    }
    memset(out, 0x55, sizeof out);
    side_fn<void(__cdecl *)(const char *, const char *, char *)>(0x5c9960)(key, challenge, out);
    st_bytes(out, sizeof out);
    ev_raw(0x110, hash_text(out));
}

struct Md5 {
    DWORD state[4];
    DWORD count[2];
    BYTE buffer[64];
};

void md5_case(int k)
{
    (void)k;
    BYTE data[0x300];
    const DWORD n = rnd(4) == 0 ? rnd(0x300) : rnd(140);
    for (DWORD i = 0; i < n; i++)
        data[i] = (BYTE)random();
    switch (rnd(3)) {
    case 0: {
        Md5 c;
        for (int i = 0; i < 4; i++)
            c.state[i] = random();
        c.count[0] = rnd(4) ? rnd(64) * 8 : random();
        c.count[1] = rnd(4) ? 0 : random();
        for (int i = 0; i < 64; i++)
            c.buffer[i] = (BYTE)random();
        side_fn<void(__cdecl *)(Md5 *, const BYTE *, DWORD)>(0x5c9ee0)(&c, data, n);
        st_bytes(&c, sizeof c);
        break;
    }
    case 1: {
        DWORD state[4];
        for (int i = 0; i < 4; i++)
            state[i] = random();
        side_fn<void(__cdecl *)(DWORD *, const BYTE *)>(0x5c9fa0)(state, data);
        st_bytes(state, sizeof state);
        break;
    }
    case 2: {
        char out[40];
        memset(out, 0x55, sizeof out);
        side_fn<void(__cdecl *)(const BYTE *, DWORD, char *)>(0x5ca8b0)(data, n, out);
        st_bytes(out, sizeof out);
        break;
    }
    }
}

// RFC 1321, appendix A.5
const char *const RFC_IN[] = {"", "a", "abc", "message digest", "abcdefghijklmnopqrstuvwxyz",
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                              "12345678901234567890123456789012345678901234567890123456789012345678901234567890"};
const char *const RFC_OUT[] = {"d41d8cd98f00b204e9800998ecf8427e", "0cc175b9c0f1b6a831c399e269772661",
                               "900150983cd24fb0d6963f7d28e17f72", "f96b697d7cb7938d525a2f31aaf161d0",
                               "c3fcd3d76192e4007dfb496cca67e13b", "d174ab98d277d9f5a5611c2c9f419d9f",
                               "57edf4a22be3c955ac49da2e2107b67a"};

void md5_rfc()
{
    Tally t{"gamespy gcd: MD5Digest on RFC 1321's test suite"};
    for (int side = 0; side < 2; side++) {
        g_side = side;
        for (int i = 0; i < 7; i++) {
            char out[40];
            side_fn<void(__cdecl *)(const char *, DWORD, char *)>(0x5ca8b0)(RFC_IN[i], slen(RFC_IN[i]), out);
            t.check(scmp(out, RFC_OUT[i]) == 0, side ? "reimpl" : "original", RFC_IN[i]);
        }
    }
    t.report();
}

}  // namespace

ZAX_CHECKS(gamespy_gcd)
{
    (void)cls;
    (void)n;
    install();
    cases("gamespy gcd: init, authenticate, think, disconnect, helpers, shutdown", scaled(300), 0x5c5280u, [](int k) { gcd_case(k); });
    cases("gamespy gcd: gcd_compute_response", scaled(300), 0x5c9960u, [](int k) { response_case(k); });
    cases("gamespy gcd: MD5Update, MD5Transform, MD5Digest", scaled(600), 0x5c9ee0u, [](int k) { md5_case(k); });
    uninstall();
    md5_rfc();
}

}  // namespace zax
