// The GameSpy SDK (about 2001) statically linked into Zax.exe, 0x5c5280-0x5caa90 (phase 5b; docs/DECOMPILATION.md 11,
// "GameSpy SDK (2001)"): the CD key SDK (gamespy_gcd.cpp), query and reporting with its helpers, the darray and
// hashtable (gamespy_qr.cpp), the server list ("CEngine", gamespy_sb.cpp) and the wsock32 import thunks
// 0x5ca9e0-0x5caa52 (gamespy_thunks.cpp). All functions are cdecl as in the exe.
//
// What the reimplementation keeps from the original [confirmed: code]: every C runtime call goes through the exe's
// address (sprintf 0x5d0db7, strncpy, strchr, strstr, strncmp, _stricmp, tolower, atoi, _strdup, malloc, realloc,
// free, memmove, rand, srand, time), every Winsock call through the exe's thunk (which jumps through the import
// table), GetTickCount through the game's wrapper 0x4d2460 and Sleep through its import entry, in the original's
// order and with its arguments. The string operations the compiler inlined in the exe (strlen, strcpy, strcat,
// strcmp as repne scasb / rep movs / the two-byte compare loop) are inlined here too (local loops, no call), and
// every SDK function calls the other SDK functions through their exe addresses where the original calls them (the
// static helpers the compiler inlined stay inlined).
#pragma once
#include "reimpl.h"

namespace zax::gamespy {

// ---- C runtime of the exe ---------------------------------------------------------------------------------

template <typename... A> inline int crt_sprintf(char *out, const char *fmt, A... a)
{
    return fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, a...);
}
inline char *crt_strncpy(char *d, const char *s, DWORD n) { return fn<char *(__cdecl *)(char *, const char *, DWORD)>(0x5d0b50)(d, s, n); }
inline char *crt_strchr(const char *s, int c) { return fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
inline char *crt_strstr(const char *s, const char *t) { return fn<char *(__cdecl *)(const char *, const char *)>(0x5d1030)(s, t); }
inline int crt_strncmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5d17d0)(a, b, n); }
inline int crt_stricmp(const char *a, const char *b) { return fn<int(__cdecl *)(const char *, const char *)>(0x5d3640)(a, b); }
inline int crt_tolower(int c) { return fn<int(__cdecl *)(int)>(0x5d272a)(c); }
inline int crt_atoi(const char *s) { return fn<int(__cdecl *)(const char *)>(0x5d03d3)(s); }
inline char *crt_strdup(const char *s) { return fn<char *(__cdecl *)(const char *)>(0x5d3606)(s); }
inline void *crt_malloc(DWORD n) { return fn<void *(__cdecl *)(DWORD)>(0x5d164f)(n); }
inline void *crt_realloc(void *p, DWORD n) { return fn<void *(__cdecl *)(void *, DWORD)>(0x5d3745)(p, n); }
inline void crt_free(void *p) { fn<void(__cdecl *)(void *)>(0x5d33e0)(p); }
inline void *crt_memmove(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
inline int crt_rand() { return fn<int(__cdecl *)()>(0x5d2198)(); }
inline void crt_srand(DWORD seed) { fn<void(__cdecl *)(DWORD)>(0x5d218b)(seed); }
inline DWORD crt_time(DWORD *t) { return fn<DWORD(__cdecl *)(DWORD *)>(0x5d20af)(t); }

// GetTickCount through the game's wrapper 0x4d2460 (jmp [0x5ea0e8]); Sleep through its import entry 0x5ea07c
inline DWORD tick() { return fn<DWORD(WINAPI *)()>(0x4d2460)(); }
inline void sleep_ms(DWORD ms) { (*imp_slot<void(WINAPI **)(DWORD)>(0x5ea07c))(ms); }

// ---- Winsock through the exe's thunks (stdcall, wsock32 by ordinal) -------------------------------------------

constexpr DWORD T_GETHOSTBYNAME = 0x5ca9e0, T_INET_ADDR = 0x5ca9e6, T_GETSOCKNAME = 0x5ca9ec, T_BIND = 0x5ca9f2,
                T_HTONS = 0x5ca9f8, T_SOCKET = 0x5ca9fe, T_CLOSESOCKET = 0x5caa04, T_SENDTO = 0x5caa0a,
                T_RECVFROM = 0x5caa10, T_SELECT = 0x5caa16, T_SEND = 0x5caa1c, T_RECV = 0x5caa22, T_CONNECT = 0x5caa28,
                T_NTOHS = 0x5caa2e, T_SETSOCKOPT = 0x5caa34, T_FDISSET = 0x5caa3a, T_INET_NTOA = 0x5caa40,
                T_HTONL = 0x5caa46, T_WSASTARTUP = 0x5caa4c, T_WSACLEANUP = 0x5caa52;

struct SockAddr {   // sockaddr_in
    WORD family;
    WORD port;
    DWORD addr;
    BYTE zero[8];
};
struct HostEnt {
    char *name;
    char **aliases;
    WORD type, length;
    char **addresses;
};
struct FdSet {
    DWORD count;
    DWORD sockets[64];
};
struct TimeVal {
    DWORD sec, usec;
};

inline DWORD ws_socket(int af, int type, int proto) { return fn<DWORD(WINAPI *)(int, int, int)>(T_SOCKET)(af, type, proto); }
inline int ws_bind(DWORD s, const SockAddr *a, int n) { return fn<int(WINAPI *)(DWORD, const SockAddr *, int)>(T_BIND)(s, a, n); }
inline int ws_getsockname(DWORD s, SockAddr *a, int *n) { return fn<int(WINAPI *)(DWORD, SockAddr *, int *)>(T_GETSOCKNAME)(s, a, n); }
inline WORD ws_htons(DWORD v) { return fn<WORD(WINAPI *)(DWORD)>(T_HTONS)(v); }
inline WORD ws_ntohs(DWORD v) { return fn<WORD(WINAPI *)(DWORD)>(T_NTOHS)(v); }
inline DWORD ws_htonl(DWORD v) { return fn<DWORD(WINAPI *)(DWORD)>(T_HTONL)(v); }
inline DWORD ws_inet_addr(const char *s) { return fn<DWORD(WINAPI *)(const char *)>(T_INET_ADDR)(s); }
inline char *ws_inet_ntoa(DWORD a) { return fn<char *(WINAPI *)(DWORD)>(T_INET_NTOA)(a); }
inline HostEnt *ws_gethostbyname(const char *s) { return fn<HostEnt *(WINAPI *)(const char *)>(T_GETHOSTBYNAME)(s); }
inline int ws_closesocket(DWORD s) { return fn<int(WINAPI *)(DWORD)>(T_CLOSESOCKET)(s); }
inline int ws_sendto(DWORD s, const void *b, int n, int f, const void *a, int an)
{
    return fn<int(WINAPI *)(DWORD, const void *, int, int, const void *, int)>(T_SENDTO)(s, b, n, f, a, an);
}
inline int ws_recvfrom(DWORD s, void *b, int n, int f, void *a, int *an)
{
    return fn<int(WINAPI *)(DWORD, void *, int, int, void *, int *)>(T_RECVFROM)(s, b, n, f, a, an);
}
inline int ws_select(int n, FdSet *r, FdSet *w, FdSet *e, const TimeVal *t)
{
    return fn<int(WINAPI *)(int, FdSet *, FdSet *, FdSet *, const TimeVal *)>(T_SELECT)(n, r, w, e, t);
}
inline int ws_send(DWORD s, const void *b, int n, int f) { return fn<int(WINAPI *)(DWORD, const void *, int, int)>(T_SEND)(s, b, n, f); }
inline int ws_recv(DWORD s, void *b, int n, int f) { return fn<int(WINAPI *)(DWORD, void *, int, int)>(T_RECV)(s, b, n, f); }
inline int ws_connect(DWORD s, const SockAddr *a, int n) { return fn<int(WINAPI *)(DWORD, const SockAddr *, int)>(T_CONNECT)(s, a, n); }
inline int ws_setsockopt(DWORD s, int level, int opt, const void *v, int n)
{
    return fn<int(WINAPI *)(DWORD, int, int, const void *, int)>(T_SETSOCKOPT)(s, level, opt, v, n);
}
inline int ws_fdisset(DWORD s, FdSet *set) { return fn<int(WINAPI *)(DWORD, FdSet *)>(T_FDISSET)(s, set); }
inline int ws_startup(DWORD version, void *data) { return fn<int(WINAPI *)(DWORD, void *)>(T_WSASTARTUP)(version, data); }

// ---- The inlined string operations -------------------------------------------------------------------------

inline DWORD slen(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
inline void scpy(char *d, const char *s)
{
    DWORD i = 0;
    do
        d[i] = s[i];
    while (s[i++]);
}
inline void scat(char *d, const char *s) { scpy(d + slen(d), s); }
// strcmp as the exe's inlined compare: 0 when equal, otherwise -1/1 by the first differing byte (unsigned)
inline int scmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        const BYTE x = (BYTE)*a, y = (BYTE)*b;
        if (x != y)
            return x < y ? -1 : 1;
        if (!x)
            return 0;
    }
}
inline void bzero(void *p, DWORD n)
{
    volatile BYTE *b = (volatile BYTE *)p;
    for (DWORD i = 0; i < n; i++)
        b[i] = 0;
}
inline void bcopy(void *d, const void *s, DWORD n)
{
    volatile BYTE *a = (volatile BYTE *)d;
    const BYTE *b = (const BYTE *)s;
    for (DWORD i = 0; i < n; i++)
        a[i] = b[i];
}

constexpr DWORD EMPTY = 0x62e558;            // ""
inline char *empty() { return glob<char *>(EMPTY); }
constexpr DWORD BROADCAST_TEXT = 0x627d34;   // "255.255.255.255"
constexpr DWORD BACKSLASH_TEXT = 0x627248;   // "\\"

// inet_addr of a host name; a name that is not an address (and not "255.255.255.255") through gethostbyname,
// whose first address replaces it when found (the SDK's get_sockaddrin, inlined in gcd_init and qr_init)
inline DWORD resolve(const char *host)
{
    DWORD a = ws_inet_addr(host);
    if (a == 0xffffffffu && scmp(host, glob<const char *>(BROADCAST_TEXT)) != 0) {
        HostEnt *h = ws_gethostbyname(host);
        if (h)
            a = *(DWORD *)h->addresses[0];
    }
    return a;
}

// ---- darray (0x5c9c20-0x5c9dd0) and hashtable (0x5c9ac0-0x5c9bd0) ------------------------------------------

typedef void(__cdecl *ElemFree)(void *);
typedef int(__cdecl *ElemCompare)(const void *, const void *);
typedef int(__cdecl *ElemHash)(const void *, int);

struct DArray {
    int count;
    int capacity;
    int elem_size;
    int grow_by;
    ElemFree free_fn;
    BYTE *list;
};
struct HashTable {
    DArray **buckets;
    int nbuckets;
    ElemFree free_fn;
    ElemHash hash_fn;
    ElemCompare compare_fn;
};

inline DArray *ArrayNew(int elem_size, int n, ElemFree f) { return fn<DArray *(__cdecl *)(int, int, ElemFree)>(0x5c9c20)(elem_size, n, f); }
inline void ArrayFree(DArray *a) { fn<void(__cdecl *)(DArray *)>(0x5c9c70)(a); }
inline int ArrayLength(DArray *a) { return fn<int(__cdecl *)(DArray *)>(0x5c9cc0)(a); }
inline void *ArrayNth(DArray *a, int n) { return fn<void *(__cdecl *)(DArray *, int)>(0x5c9cd0)(a, n); }
inline void ArrayAppend(DArray *a, const void *e) { fn<void(__cdecl *)(DArray *, const void *)>(0x5c9cf0)(a, e); }
inline void ArrayReplaceAt(DArray *a, const void *e, int n) { fn<void(__cdecl *)(DArray *, const void *, int)>(0x5c9d80)(a, e, n); }
inline int ArraySearch(DArray *a, const void *key, ElemCompare c, int from, int sorted)
{
    return fn<int(__cdecl *)(DArray *, const void *, ElemCompare, int, int)>(0x5c9dd0)(a, key, c, from, sorted);
}
inline HashTable *TableNew2(int elem_size, int nbuckets, int nchains, DWORD hash, DWORD compare, DWORD free_fn)
{
    return fn<HashTable *(__cdecl *)(int, int, int, DWORD, DWORD, DWORD)>(0x5c9ac0)(elem_size, nbuckets, nchains, hash, compare, free_fn);
}
inline void TableFree(HashTable *t) { fn<void(__cdecl *)(HashTable *)>(0x5c9b30)(t); }
inline void TableEnter(HashTable *t, const void *e) { fn<void(__cdecl *)(HashTable *, const void *)>(0x5c9b70)(t, e); }
inline void *TableLookup(HashTable *t, const void *e) { return fn<void *(__cdecl *)(HashTable *, const void *)>(0x5c9bd0)(t, e); }

// ---- SDK functions called through their exe addresses ---------------------------------------------------------

inline void SocketStartUp() { fn<void(__cdecl *)()>(0x5c9a90)(); }
inline void SocketShutDown() { fn<void(__cdecl *)()>(0x5c9ab0)(); }
inline void msleep(DWORD ms) { fn<void(__cdecl *)(DWORD)>(0x5c9a80)(ms); }

}  // namespace zax::gamespy
