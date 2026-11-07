// Shared parts of the checks of the net game, net session and DirectPlay modules
// (checks_net_game.cpp, checks_net_session.cpp, checks_dplay.cpp): the event log of each side with
// pointer normalisation, a module's hooks taken out for side 0, the runner of a case and fake objects
// whose vtable slots record their calls.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. The fakes are statics, set up again before each side, so both sides see the same
// addresses; blocks the functions allocate differ between the sides. Values logged or hashed go
// through norm(): small values, addresses in the exe image and in the fakes' arena stay as they are,
// any other value (heap and stack pointers, big numbers) becomes the index of its first appearance on
// that side - equal raw values are equal before any mapping.
#pragma once
#include "check_util.h"
#include "net_packet.h"

namespace zax {
namespace netcheck {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Arena of the fakes ---------------------------------------------------------------------------------

constexpr DWORD ARENA = 0x40000;
alignas(16) inline BYTE g_arena[ARENA];
inline DWORD g_arena_used;
inline void *arena(DWORD n)
{
    n = (n + 15) & ~15u;
    if (g_arena_used + n > ARENA)
        return nullptr;
    void *p = g_arena + g_arena_used;
    memset(p, 0, n);
    g_arena_used += n;
    return p;
}
inline bool in_arena(DWORD v) { return v >= ptrv(g_arena) && v < ptrv(g_arena) + ARENA; }

// ---- Event log and state -----------------------------------------------------------------------------------

inline int g_side;
constexpr int LOG = 512;
inline DWORD g_log[2][LOG][4];
inline int g_nlog[2];
inline unsigned g_loghash[2], g_statehash[2];
constexpr int NPTR = 256;
inline DWORD g_ptrs[2][NPTR];
inline int g_nptrs[2];

inline DWORD norm(DWORD v)
{
    if (v < 0x10000 || (v >= 0x400000 && v < IMAGE_END) || in_arena(v))
        return v;
    for (int i = 0; i < g_nptrs[g_side]; i++)
        if (g_ptrs[g_side][i] == v)
            return 0xa0000000u | (DWORD)i;
    if (g_nptrs[g_side] < NPTR)
        g_ptrs[g_side][g_nptrs[g_side]++] = v;
    return 0xa0000000u | (DWORD)(g_nptrs[g_side] - 1);
}

inline void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, norm(b), norm(c), norm(d)};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
inline void ev_raw(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
inline void st(DWORD v) { v = norm(v); g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
inline void st_raw(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
inline void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }
inline void st_text(const char *s)
{
    if (!s) {
        st_raw(0xdead0001);
        return;
    }
    st_bytes(s, (DWORD)lstrlenA(s) + 1);
}
inline void st_zstring(const ZString *z) { st_text(z && z->buffer ? z->buffer->text : nullptr); }
// The dwords of a block, every one through norm()
inline void st_dwords(const void *p, DWORD n)
{
    for (DWORD i = 0; i < n / 4; i++)
        st(((const DWORD *)p)[i]);
}

inline void begin_side(int side, unsigned seed)
{
    g_side = side;
    g_seed = seed;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
    g_nptrs[side] = 0;
}
inline bool sides_equal() { return g_loghash[0] == g_loghash[1] && g_statehash[0] == g_statehash[1] && g_nlog[0] == g_nlog[1]; }
inline void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    if (g_nlog[0] != g_nlog[1])
        log("    events: %d / %d", g_nlog[0], g_nlog[1]);
    else
        log("    events equal, state differs");
}

// ---- A module's hooks out and in ---------------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
inline SavedSite g_sites[MAX_SITES];
inline int g_nsites;
inline int (*g_table)(Hook *);
inline void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = g_table(nullptr);
    if (!hook_sites_fit("net_common", n, MAX_SITES))
        return;
    g_table(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
inline void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// One case on both sides from the same seed; the arena is cleared before each side
template <typename F> bool both_sides(unsigned seed, F run)
{
    begin_side(0, seed);
    g_arena_used = 0;
    hooks_out();
    run();
    hooks_in();
    begin_side(1, seed);
    g_arena_used = 0;
    run();
    return sides_equal();
}

template <typename F> void run_cases(const char *what, int n, unsigned seed0, F run)
{
    Tally t{what};
    for (int k = 0; k < n; k++) {
        const bool same = both_sides(seed0 + (unsigned)k * 7919u, [&] { run(k); });
        char d[64];
        wsprintfA(d, "[case %d]", k);
        t.check(same, "", d);
        if (!same && t.mismatches <= 3)
            log_first_difference();
    }
    t.report();
}

// ---- Fake objects ------------------------------------------------------------------------------------------

// An object whose vtable slots record (id, slot, arguments) and return ret[slot]; slots without a
// recorder are null (a call would fault, so a test only calls what it set up)
struct Fake {
    DWORD vftable;
    DWORD id;
    DWORD ret[64];
    DWORD vt[64];
    BYTE mem[0x100];
};
inline Fake *new_fake(DWORD id)
{
    Fake *f = (Fake *)arena(sizeof(Fake));
    f->vftable = ptrv(f->vt);
    f->id = id;
    return f;
}
template <int S> DWORD THISCALL rec0(Fake *f)
{
    ev(0x100 + S, f->id);
    return f->ret[S];
}
template <int S> DWORD THISCALL rec1(Fake *f, DWORD a)
{
    ev(0x100 + S, f->id, a);
    return f->ret[S];
}
template <int S> DWORD THISCALL rec2(Fake *f, DWORD a, DWORD b)
{
    ev(0x100 + S, f->id, a, b);
    return f->ret[S];
}
template <int S> DWORD THISCALL rec3(Fake *f, DWORD a, DWORD b, DWORD c)
{
    ev(0x100 + S, f->id, a, b);
    ev(0x180 + S, c);
    return f->ret[S];
}
template <int S> void set0(Fake *f, DWORD r = 0) { f->vt[S] = ptrv((void *)&rec0<S>); f->ret[S] = r; }
template <int S> void set1(Fake *f, DWORD r = 0) { f->vt[S] = ptrv((void *)&rec1<S>); f->ret[S] = r; }
template <int S> void set2(Fake *f, DWORD r = 0) { f->vt[S] = ptrv((void *)&rec2<S>); f->ret[S] = r; }
template <int S> void set3(Fake *f, DWORD r = 0) { f->vt[S] = ptrv((void *)&rec3<S>); f->ret[S] = r; }

// The packet's state through norm() (data bytes, positions, flag, readers, current reader)
inline void st_packet(const NetPacket *p)
{
    st(p->capacity);
    st(p->length);
    st(p->position);
    st(p->consumed);
    st(p->reliable);
    if (p->data && p->length)
        st_bytes(p->data, p->length);
    st(p->readers.count);
    for (DWORD i = 0; i < p->readers.count; i++)
        st(ptrv(p->readers.data[i]));
    st(ptrv(p->current));
}

// A real packet (constructor 0x51fe60, destructor 0x51fec0 through the exe address)
inline void packet_ctor(NetPacket *p) { at<void(THISCALL *)(NetPacket *)>(0x51fe60)(p); }
inline void packet_dtor(NetPacket *p) { at<void(THISCALL *)(NetPacket *)>(0x51fec0)(p); }

inline DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }

}  // namespace netcheck
}  // namespace zax
