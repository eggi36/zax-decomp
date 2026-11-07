// Shared machinery of the checks of the inventory items and definitions (checks_inventory_items.cpp,
// checks_inventory_defs.cpp, checks_zax_items.cpp, checks_zax_item_defs.cpp) and of the item actions
// (checks_zax_actions.cpp), in the game thread.
//
// Every case runs twice: side 0 with all hooks of the module taken out (the original's first bytes put
// back, so nested calls run the original too), side 1 through the reimplementation. Memory that the C
// heap gives during a case can come from an arena of each side, filled with the same pattern first
// (operator new 0x5d033a and free 0x5d33e0 jump to stand-ins while the game thread runs a case), so
// every byte is defined and pointers into the arena compare by their offset. Values are compared dword
// by dword: equal raw values are equal; otherwise arena pointers by offset and other pointers (string
// buffers of the pool) by their text. Calls into the fake worlds are recorded in an event log per side.
// The class getter and RegisterFields tests are the ones of checks_action_classes.cpp: a stand-in for
// ClassInfo_Register 0x415780, a fake class info, and the registered descriptors (real data) against
// side 1's with the bytes no constructor writes left out.
#pragma once
#include "check_util.h"
#include "class_info.h"
#include "field_desc.h"
#include "pool.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetCurrentThreadId(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax::invcheck {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Memory helpers (the fixed ones of checks_geometry.cpp) ----------------------------------------------

inline bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}

inline CClassInfo *class_of(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 0x14))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    if (!readable(at<void *>(vt), 12))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || !readable(at<void *>(getter), 5) || *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    // through the trampoline when the getter is hooked
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000 || !readable(at<void *>(var), 4))
        return nullptr;
    return *at<CClassInfo **>(var);
}

// ---- Hooks of a module out and in ----------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 256;
inline SavedSite g_sites[MAX_SITES];
inline Hook g_hook_list[MAX_SITES];
inline int g_nsites;
inline int (*g_hook_table)(Hook *);
inline void hooks_out()
{
    g_nsites = 0;
    const int n = g_hook_table(nullptr);
    if (!hook_sites_fit("inventory_common", n, MAX_SITES))
        return;
    g_hook_table(g_hook_list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(g_hook_list[i].address);
        if (original_address(g_hook_list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, g_hook_list[i].expected);
    }
}
inline void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}
inline int g_side;
inline void side_begin(int side)
{
    g_side = side;
    if (side == 0)
        hooks_out();
}
inline void side_end(int side)
{
    if (side == 0)
        hooks_in();
}

// ---- Event log ------------------------------------------------------------------------------------------

constexpr int LOG = 4096;
inline DWORD g_log[2][LOG];
inline int g_nlog[2];
inline unsigned g_loghash[2];
inline void log_reset(int side)
{
    g_nlog[side] = 0;
    g_loghash[side] = 2166136261u;
}
inline void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    int &n = g_nlog[g_side];
    if (n + 4 <= LOG)
        memcpy(&g_log[g_side][n], e, sizeof e);
    n += 4;
}
inline bool same_log() { return g_nlog[0] == g_nlog[1] && g_loghash[0] == g_loghash[1]; }
// A recorder's result: different for every event, the low byte given
inline DWORD result(DWORD low) { return ((0x5e700000u + (DWORD)g_nlog[g_side] * 0x100u) & 0xffffff00) | (low & 0xff); }
// The first event that differs, logged
inline void log_first_difference()
{
    const int n = g_nlog[0] < g_nlog[1] ? g_nlog[0] : g_nlog[1];
    for (int i = 0; i + 4 <= n && i + 4 <= LOG; i += 4)
        if (memcmp(&g_log[0][i], &g_log[1][i], 16) != 0) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i / 4, g_log[0][i], g_log[0][i + 1], g_log[0][i + 2], g_log[0][i + 3],
                g_log[1][i], g_log[1][i + 1], g_log[1][i + 2], g_log[1][i + 3]);
            return;
        }
}

// ---- The arena of each side -------------------------------------------------------------------------------

constexpr DWORD ARENA_SIZE = 0x200000;
constexpr int MAX_ALLOCS = 2048;
struct Alloc {
    DWORD offset, size;
};
inline BYTE *g_arena[2];
inline BYTE *g_copy;                 // a saved arena (the mask run)
inline DWORD g_used[2], g_high;
inline Alloc g_allocs[2][MAX_ALLOCS];
inline int g_nallocs[2];
inline int g_arena_side = -1;
inline DWORD g_thread;
inline bool g_overflow;
inline BYTE *g_free_trampoline;
inline CodePatch g_new_patch, g_free_patch;

inline bool in_arena(DWORD p, int side) { return g_arena[side] && p >= ptrv(g_arena[side]) && p < ptrv(g_arena[side]) + ARENA_SIZE; }

// A new chunk of the pool allocator (ZPoolClass::Grow 0x506420, reimplemented in pool.cpp and inlined
// there into the allocation) is not a block of the case: it stays in the game's heap (in the arena it
// would be overwritten by later cases while the pool still gives out its blocks). A chunk has
// size * (4000 / size) + 4 bytes (SetChunkBytes(4000)), for the pool's sizes 4 to 500 (multiples of 4)
// 48 sizes from 3588 to 4004; no block the checked code makes has such a size.
inline bool from_pool_grow(DWORD ret, DWORD n) { return pool_grow_call(ret, n); }

inline void *__cdecl arena_new(DWORD n)
{
    const int s = g_arena_side;
    if (s >= 0 && GetCurrentThreadId() == g_thread && !from_pool_grow((DWORD)(uintptr_t)__builtin_return_address(0), n)) {
        const DWORD size = (n + 7) & ~7u;
        if (g_used[s] + size + 8 <= ARENA_SIZE && g_nallocs[s] < MAX_ALLOCS) {
            BYTE *p = g_arena[s] + g_used[s];
            g_allocs[s][g_nallocs[s]++] = {g_used[s], n};
            g_used[s] += size ? size : 8;
            return p;
        }
        g_overflow = true;
    }
    return at<void *(__cdecl *)(DWORD, int)>(0x5d1661)(n, 1);   // __nh_malloc, as operator new
}
// eax: 0 for an arena block (free's own eax is undefined; the stand-in keeps it the same on both sides)
inline DWORD __cdecl arena_free(void *p)
{
    if (in_arena(ptrv(p), 0) || in_arena(ptrv(p), 1))
        return 0;
    return reinterpret_cast<DWORD(__cdecl *)(void *)>(g_free_trampoline)(p);
}

inline bool arena_setup()
{
    if (g_arena[0])
        return true;
    g_arena[0] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_arena[1] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_copy = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_free_trampoline = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_arena[0] || !g_arena[1] || !g_copy || !g_free_trampoline)
        return false;
    check_region(g_arena[0], ARENA_SIZE, "invcheck arena 0");
    check_region(g_arena[1], ARENA_SIZE, "invcheck arena 1");
    check_region(g_copy, ARENA_SIZE, "invcheck arena copy");
    // free's first instructions (push ebp; mov ebp, esp; push -1), then back to 0x5d33e5
    static const BYTE FIRST[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
    if (memcmp(original_address(0x5d33e0), FIRST, 5) != 0)   // free is hooked (crt_heap.cpp): its trampoline holds the original bytes
        return false;
    memcpy(g_free_trampoline, FIRST, 5);
    g_free_trampoline[5] = 0xe9;
    *(INT32 *)(g_free_trampoline + 6) = (INT32)(0x5d33e5 - (ptrv(g_free_trampoline) + 10));
    g_thread = GetCurrentThreadId();
    return true;
}
// The arena of a side on, the stand-ins in; a fresh arena is filled with the pattern first, otherwise
// (destroying what a case made) it goes on where it was
inline void arena_on(int side, BYTE pattern, bool fresh = true)
{
    if (fresh) {
        const DWORD n = g_high + 0x10000 < ARENA_SIZE ? g_high + 0x10000 : ARENA_SIZE;
        memset(g_arena[side], pattern, n);
        g_used[side] = 0;
        g_nallocs[side] = 0;
    }
    g_arena_side = side;
    patch_jump(g_new_patch, 0x5d033a, (void *)&arena_new);
    patch_jump(g_free_patch, 0x5d33e0, (void *)&arena_free);
}
inline void arena_off()
{
    unpatch(g_free_patch);
    unpatch(g_new_patch);
    if (g_arena_side >= 0 && g_used[g_arena_side] > g_high)
        g_high = g_used[g_arena_side];
    g_arena_side = -1;
}

// ---- Comparing values ------------------------------------------------------------------------------------

// The text of a string buffer pointer (ZStringBuffer: refs, capacity, text), null if it is none
inline const char *buffer_text(DWORD p)
{
    if (p < 0x10000 || (p & 3) || !readable(at<void *>(p), 12))
        return nullptr;
    const char *t = at<const char *>(p + 8);
    for (DWORD i = 0; i < 0x400; i++) {
        if ((i & 0xff) == 0 && !readable(t + i, 0x100))
            return nullptr;
        if (!t[i])
            return t;
    }
    return nullptr;
}
inline bool same_text(const char *a, const char *b) { return a && b && lstrcmpA(a, b) == 0; }
inline bool is_name_list(DWORD vtable) { return vtable == 0x5f0240 || vtable == 0x5fd5d8 || vtable == 0x5fdaec; }
// A C string at p (readable, at most 0x400 bytes), null if it is none
inline const char *c_text(DWORD p)
{
    if (p < 0x10000)
        return nullptr;
    const char *t = at<const char *>(p);
    for (DWORD i = 0; i < 0x400; i++) {
        if ((i == 0 || ((p + i) & 0xfff) == 0) && !readable(t + i, 1))
            return nullptr;
        if (!t[i])
            return t;
    }
    return nullptr;
}

// Pointers of side 0 and 1 that stand for the same thing (blocks given out during a case): the pair
// is registered, then they compare equal
constexpr int MAX_PAIRS = 64;
inline DWORD g_pairs[MAX_PAIRS][2];
inline int g_npairs;
inline void pair(DWORD a, DWORD b)
{
    if (g_npairs < MAX_PAIRS) {
        g_pairs[g_npairs][0] = a;
        g_pairs[g_npairs][1] = b;
        g_npairs++;
    }
}

// A value of side 0 (a) against side 1 (b). Pointers to blocks of the pool or the heap made during a
// case (the name lists of the enum descriptors, string buffers) compare by what they point to: the
// text of a string buffer, otherwise the first 16 bytes, three levels deep.
inline bool same_value_at(DWORD a, DWORD b, int depth, bool arena_relative)
{
    if (a == b)
        return true;
    if (arena_relative && in_arena(a, 0) && in_arena(b, 1))
        return a - ptrv(g_arena[0]) == b - ptrv(g_arena[1]);
    for (int i = 0; i < g_npairs; i++)
        if (g_pairs[i][0] == a && g_pairs[i][1] == b)
            return true;
    if (same_text(buffer_text(a), buffer_text(b)))
        return true;
    // the name list of an enum descriptor (vtable 0x5f0240, a pointer array at +4: data +8, count +0xc,
    // each entry {name, value}; the name a C string on the heap, Text_Copy 0x587c20 in FieldDesc_AddEnum),
    // or one of the number kinds' name lists of the same layout (0x5fd5d8, 0x5fdaec; name_lists.h), which
    // the clones of inherited enum descriptors hold
    if (a >= 0x10000 && b >= 0x10000 && readable(at<void *>(a), 0x10) && readable(at<void *>(b), 0x10) && is_name_list(*at<DWORD *>(a)) &&
        *at<DWORD *>(b) == *at<DWORD *>(a)) {
        const DWORD n = *at<DWORD *>(a + 0xc);
        if (n != *at<DWORD *>(b + 0xc) || n > 0x100)
            return false;
        const DWORD da = *at<DWORD *>(a + 8), db = *at<DWORD *>(b + 8);
        if (n && (!readable(at<void *>(da), n * 4) || !readable(at<void *>(db), n * 4)))
            return false;
        for (DWORD i = 0; i < n; i++) {
            const DWORD ea = *at<DWORD *>(da + i * 4), eb = *at<DWORD *>(db + i * 4);
            if (!readable(at<void *>(ea), 8) || !readable(at<void *>(eb), 8) || *at<DWORD *>(ea + 4) != *at<DWORD *>(eb + 4) ||
                !same_text(c_text(*at<DWORD *>(ea)), c_text(*at<DWORD *>(eb))))
                return false;
        }
        return true;
    }
    if (depth >= 3 || a < 0x10000 || b < 0x10000 || (a & 3) || (b & 3) || (a >= 0x400000 && a < IMAGE_END) ||
        (b >= 0x400000 && b < IMAGE_END) || !readable(at<void *>(a), 16) || !readable(at<void *>(b), 16))
        return false;
    for (DWORD k = 0; k < 16; k += 4)
        if (!same_value_at(*at<DWORD *>(a + k), *at<DWORD *>(b + k), depth + 1, arena_relative))
            return false;
    return true;
}
inline bool same_value(DWORD a, DWORD b) { return same_value_at(a, b, 0, true); }
// Two regions (side 0, side 1) dword by dword; the first difference logged
inline DWORD g_diff_at, g_diff_a, g_diff_b;
inline bool same_region(const BYTE *a, const BYTE *b, DWORD n)
{
    for (DWORD i = 0; i + 4 <= n; i += 4) {
        const DWORD x = *reinterpret_cast<const DWORD *>(a + i), y = *reinterpret_cast<const DWORD *>(b + i);
        if (!same_value(x, y)) {
            g_diff_at = i;
            g_diff_a = x;
            g_diff_b = y;
            return false;
        }
    }
    for (DWORD i = n & ~3u; i < n; i++)
        if (a[i] != b[i]) {
            g_diff_at = i;
            g_diff_a = a[i];
            g_diff_b = b[i];
            return false;
        }
    return true;
}
// A result made comparable: arena pointers by their offset, the class info of the case by a tag
inline DWORD g_fake_class[2];
inline DWORD rel(DWORD v, int side)
{
    if (in_arena(v, side))
        return 0xa0000000u + (v - ptrv(g_arena[side]));
    if (v && v == g_fake_class[side])
        return 0xc1a55000u;
    return v;
}

// ---- Classes: Create, CreateArray, getters, RegisterFields ------------------------------------------------

struct ClassDef {
    const char *name;
    DWORD size, var, create, create_array, register_fields;
    DWORD getters[2];
    bool destroy;   // slot 1 destroys what Create made (not for the abstract classes)
    bool no_own_fields = false;   // RegisterFields only changes the class or inherited fields
    // bytes a made class's descriptors leave undefined (uninitialised stack copied into a default), set to
    // a fixed value before the comparisons
    void (*mask_made)(CClassInfo *) = nullptr;
    // offset of a pool block the constructor makes (0: none); CreateArray's objects of both sides live at
    // the same time, so the sides get different blocks, compared as a pair
    DWORD pool_block_at = 0;
};

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);

inline BYTE g_obj[2][0x900];   // the biggest class tested: CWaterWarpEffectFrame (0x880)
inline DWORD g_eax[2];

// Create on a pool block filled with a pattern (all bytes compared, the C heap from the arena), then
// destroyed through its own slot 1 with flag 1
inline void test_create(Tally &t, const ClassDef &c, unsigned seed)
{
    bool reused[2];
    DWORD used[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        pool_free(p, c.size);
        arena_on(side, (BYTE)seed);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        used[side] = g_used[side];
        // objects bigger than the biggest pool size come from operator new (the arena)
        reused[side] = o == p || (in_arena(ptrv(o), side) && ptrv(o) == ptrv(g_arena[side]));
        memcpy(g_obj[side], o, c.size);
        vcall<void *>(o, 1, 1);
        arena_off();
        side_end(side);
    }
    char d[180];
    const bool same = reused[0] && reused[1] && used[0] == used[1] && same_region(g_obj[0], g_obj[1], c.size) &&
                      same_region(g_arena[0], g_arena[1], used[0]);
    wsprintfA(d, "[Create seed %u: block reused %d / %d, heap %u / %u, first difference +%x: %x / %x]", seed, reused[0], reused[1],
              used[0], used[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
}

// CreateArray with count elements in the arena (the whole block compared), destroyed as an array
// (slot 1 with 2 and with 3)
inline void test_create_array(Tally &t, const ClassDef &c, int count, BYTE pattern)
{
    DWORD e[2], destroyed[2];
    void *a[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        arena_on(side, pattern);
        a[side] = at<CreateN>(c.create_array)(count, 0, 0);
        e[side] = rel(ptrv(a[side]), side);
        arena_off();
        side_end(side);
    }
    g_npairs = 0;
    for (int i = 0; c.pool_block_at && a[0] && a[1] && i < count; i++)
        pair(*reinterpret_cast<DWORD *>(static_cast<BYTE *>(a[0]) + i * c.size + c.pool_block_at),
             *reinterpret_cast<DWORD *>(static_cast<BYTE *>(a[1]) + i * c.size + c.pool_block_at));
    char d[200];
    const bool same = g_used[0] == g_used[1] && e[0] == e[1] && same_region(g_arena[0], g_arena[1], g_used[0]);
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, first difference +%x: %x / %x]", count, e[0], e[1], g_used[0], g_used[1],
              g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
    for (int side = 0; side < 2; side++) {
        destroyed[side] = 0;
        if (count > 0 && a[side]) {
            side_begin(side);
            arena_on(side, pattern, false);   // the destructors' frees of arena blocks are ignored
            destroyed[side] = rel(ptrv(vcall<void *>(a[side], 1, (DWORD)(count & 1 ? 3 : 2))), side);
            arena_off();
            side_end(side);
        }
    }
    wsprintfA(d, "[array destroyed %d: %x / %x, first difference +%x]", count, destroyed[0], destroyed[1], g_diff_at);
    t.check(destroyed[0] == destroyed[1] && same_region(g_arena[0], g_arena[1], g_used[0]), c.name, d);
}

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
inline RegisterCall g_register;
inline int g_nregister;
inline bool g_inherit;
inline CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                          DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    self->parent = parent;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create), ptrv(create_array),
                  ptrv(register_fields)};
    g_nregister++;
    if (g_inherit)
        at<void(THISCALL *)(CClassInfo *)>(0x415690)(self);   // InheritFields (the registration's next step)
    return self;
}

// A class getter with the class variable set (it returns it) and 0 (the stand-in registers a fake)
inline void test_getter(Tally &t, const ClassDef &c, DWORD getter)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    RegisterCall calls[2];
    DWORD results[2], set[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        set[side] = ptrv(at<CClassInfo *(__cdecl *)()>(getter)());
        var = nullptr;
        g_nregister = 0;
        g_inherit = false;
        memset(&g_register, 0, sizeof g_register);
        CodePatch p;
        patch_jump(p, 0x415780, (void *)&fake_register);
        void *made = at<CClassInfo *(__cdecl *)()>(getter)();
        unpatch(p);
        results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
        calls[side] = g_register;
        if (var)
            pool_free(var, sizeof(CClassInfo));
        var = saved;
        side_end(side);
    }
    char d[180];
    wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x, parent %x / %x]", getter, set[0], set[1],
              results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version, calls[0].parent,
              calls[1].parent);
    t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
            c.name, d);
}

// One RegisterFields run of a side into a fake class info (var_zero: the stand-in registers it)
struct Made {
    CClassInfo *cls;
    alignas(4) BYTE fake[sizeof(CClassInfo)];
    DWORD result, cache, nregister;
    RegisterCall call;
};
inline Made g_made[2];
inline void run_register_fields(const ClassDef &c, bool var_zero, int side, BYTE pattern)
{
    Made &m = g_made[side];
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const real = var;
    *at<DWORD *>(FIND_FIELD_CACHE) = 0;
    arena_on(side, pattern);
    memset(m.fake, 0, sizeof m.fake);
    CClassInfo *fake = reinterpret_cast<CClassInfo *>(m.fake);
    if (var_zero)
        var = nullptr;
    else {
        fake->name = real->name;
        fake->parent = real->parent;
        at<void(THISCALL *)(CClassInfo *)>(0x415690)(fake);   // InheritFields
        var = fake;
    }
    g_nregister = 0;
    g_inherit = true;
    memset(&g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    g_fake_class[side] = ptrv(var);
    const DWORD r = at<DWORD(__cdecl *)()>(c.register_fields)();
    g_fake_class[side] = ptrv(var);
    unpatch(p);
    m.cls = var;
    m.result = rel(r, side);
    m.cache = *at<DWORD *>(FIND_FIELD_CACHE);
    m.nregister = (DWORD)g_nregister;
    m.call = g_register;
    var = real;
    arena_off();
}
// The fields of a made class destroyed (not freed: arena blocks), a registered fake given back
inline void free_made(int side)
{
    Made &m = g_made[side];
    arena_on(side, 0, false);
    for (DWORD i = 0; i < m.cls->num_fields; i++)
        if (m.cls->fields[i])
            vcall<void *>(m.cls->fields[i], 0, 0);
    arena_off();
    if (ptrv(m.cls) != ptrv(m.fake))
        pool_free(m.cls, sizeof(CClassInfo));
}

inline void log_field(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' units '%s' help '%.30s' off %x %x %x", what, f->vftable, text(f->name), text(f->units), text(f->help),
        f->offset, f->unknown14, f->unknown18);
    // +0x28 to +0x44: for a descriptor of 0x44 bytes the last is the next block's (run 20261030-202421-ae04 logged a
    // made descriptor's "last dword" ffffffff there, past the block, not compared)
    log("      %x %x %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
    for (DWORD p = d[16], k = 0; k < 3 && p >= 0x10000 && !(p & 3) && readable(at<void *>(p), 16); k++, p = *at<DWORD *>(p + 4)) {
        const DWORD *q = at<DWORD *>(p);
        log("      at %x: %x %x %x %x", p, q[0], q[1], q[2], q[3]);
    }
}

// The class info state of a made class (pointers made comparable)
inline void class_state(int side, DWORD *out)
{
    const CClassInfo *k = g_made[side].cls;
    out[0] = k->num_fields;
    out[1] = k->num_inherited;
    out[2] = ptrv(k->alt_name);
    out[3] = ptrv(k->name);
    out[4] = ptrv(k->parent);
    out[5] = rel(ptrv(k->fields), side);
    out[6] = g_made[side].result;
    out[7] = g_made[side].cache;
    out[8] = g_made[side].nregister;
}

// The bytes the descriptors' constructors leave unwritten: equal to the pattern in two runs of different
// patterns (g_copy holds the first run's arena)
inline bool unwritten(DWORD offset, BYTE p, BYTE q) { return g_copy[offset] == q && g_arena[1][offset] == p; }

inline int find_alloc(DWORD p)
{
    if (!in_arena(p, 1))
        return -1;
    const DWORD off = p - ptrv(g_arena[1]);
    for (int i = 0; i < g_nallocs[1]; i++)
        if (g_allocs[1][i].offset == off)
            return i;
    return -1;
}

// A registered descriptor (heap) against a made one of side 1 (arena), unwritten bytes left out
// dwords of a made descriptor whose bytes partly equalled the patterns and that compared equal whole (logged)
inline int g_pattern_pointers;

inline bool same_registered(const BYTE *real, DWORD made, BYTE p, BYTE q, int depth)
{
    const int k = find_alloc(made);
    if (k < 0 || !readable(real, g_allocs[1][k].size)) {
        // named in the message (it showed the previous dword's difference before)
        g_diff_at = 0xfffffff0u + (k < 0 ? 1u : 2u);
        g_diff_a = ptrv(real);
        g_diff_b = made;
        return false;
    }
    const DWORD base = g_allocs[1][k].offset, size = g_allocs[1][k].size;
    const BYTE *m = at<const BYTE *>(made);
    for (DWORD i = 0; i + 4 <= size; i += 4) {
        int written = 0;
        for (DWORD b = 0; b < 4; b++)
            written += !unwritten(base + i + b, p, q);
        const DWORD x = *reinterpret_cast<const DWORD *>(real + i), y = *reinterpret_cast<const DWORD *>(m + i);
        if (!written || x == y)
            continue;
        g_diff_at = i;
        g_diff_a = x;
        g_diff_b = y;
        if (written < 4) {   // padding next to a smaller member: the written bytes raw
            bool raw_same = true;
            for (DWORD b = 0; b < 4; b++)
                if (!unwritten(base + i + b, p, q) && real[i + b] != m[i + b])
                    raw_same = false;
            if (raw_same)
                continue;
            // A pointer whose bytes happen to equal the patterns: the q run's block and the p run's are different
            // pool blocks (the pool hands out the last freed block first, and the q run's descriptors are freed
            // before the p run), so a byte of the q run's pointer can be q and the same byte of the p run's p
            // (batch 6-16b: CSphereOfZinEnergyInventoryHUDRenderer's pattern pair is 0xff / 0x00, the name buffer
            // 0x1b7effc4 at +4 counted as two written bytes, run 20261030-202421-ae04). Such a dword is compared
            // whole, by its text or value, before it counts as a difference.
            if (same_text(buffer_text(x), buffer_text(y)) || same_value_at(x, y, 0, false)) {
                g_pattern_pointers++;
                continue;
            }
            return false;
        }
        // a string buffer (the name, units, help) by its text, before any block comparison: its reference
        // count and capacity are not the descriptor's (run 20261029-160246-0f38 reported CLimitedTimeAI's
        // "Max Time" with the name pointers at +4; the cause is not known for sure, docs/DECOMPILATION.md 12)
        if (same_text(buffer_text(x), buffer_text(y)))
            continue;
        // an element descriptor (not an enum descriptor's name list, compared by its entries below): a
        // block the case allocated. Other pointers into the arena (a string buffer whose pool block lay in
        // the arena, run 20261028-114705-ef53: "Units/Sec" at +0x20) compare by value below.
        if (in_arena(y, 1) && find_alloc(y) >= 0 && depth < 2 && readable(at<void *>(x), 4) &&
            !(is_name_list(*at<DWORD *>(x)) && readable(at<void *>(y), 4) && *at<DWORD *>(y) == *at<DWORD *>(x))) {
            if (!same_registered(at<const BYTE *>(x), y, p, q, depth + 1))
                return false;
            continue;
        }
        if (!same_value_at(x, y, 0, false))
            return false;
    }
    return true;
}

// RegisterFields into a fake class info (the parent's fields inherited) and with the class variable 0;
// then the real data: the registered descriptors against side 1's
inline void test_register_fields(Tally &t, Tally &real, const ClassDef &c, unsigned seed)
{
    const DWORD saved_cache = *at<DWORD *>(FIND_FIELD_CACHE);
    g_seed = seed;
    const BYTE p = (BYTE)(random() | 1), q = (BYTE)~p;
    for (int var_zero = 0; var_zero < 2; var_zero++) {
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            run_register_fields(c, var_zero != 0, side, p);
            side_end(side);
        }
        if (c.mask_made) {
            c.mask_made(g_made[0].cls);
            c.mask_made(g_made[1].cls);
        }
        DWORD s0[9], s1[9];
        class_state(0, s0);
        class_state(1, s1);
        bool same = g_used[0] == g_used[1] && g_nallocs[0] == g_nallocs[1] && same_region(g_arena[0], g_arena[1], g_used[0]) &&
                    memcmp(s0, s1, sizeof s0) == 0 && memcmp(&g_made[0].call, &g_made[1].call, sizeof g_made[0].call) == 0 &&
                    (s0[0] > s0[1] || c.no_own_fields);
        char d[220];
        wsprintfA(d, "[RegisterFields var %d: %u / %u fields (%u inherited), %u / %u bytes, eax %x / %x, first difference +%x: %x / %x]",
                  var_zero, s0[0], s1[0], s0[1], g_used[0], g_used[1], s0[6], s1[6], g_diff_at, g_diff_a, g_diff_b);
        if (!same && t.mismatches < 3 && g_nallocs[0] != g_nallocs[1]) {
            log("    allocations %d / %d", g_nallocs[0], g_nallocs[1]);
            const int big = g_nallocs[0] > g_nallocs[1] ? 0 : 1;
            int shown = 0;
            for (int i = 0; i < g_nallocs[big] && shown < 12; i++)
                if (i >= g_nallocs[1 - big] || g_allocs[0][i].size != g_allocs[1][i].size) {
                    log("    alloc %d: %x / %x", i, i < g_nallocs[0] ? g_allocs[0][i].size : 0, i < g_nallocs[1] ? g_allocs[1][i].size : 0);
                    shown++;
                }
        }
        if (!same && t.mismatches < 3) {
            log("    class state %x %x %x %x %x %x %x %x %x / %x %x %x %x %x %x %x %x %x", s0[0], s0[1], s0[2], s0[3], s0[4], s0[5], s0[6],
                s0[7], s0[8], s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6], s1[7], s1[8]);
            for (DWORD i = 0; i < g_made[0].cls->num_fields && i < g_made[1].cls->num_fields && i < 8; i++) {
                log_field("original", g_made[0].cls->fields[i]);
                log_field("reimpl", g_made[1].cls->fields[i]);
            }
        }
        t.check(same, c.name, d);
        free_made(0);
        free_made(1);
        if (var_zero)
            continue;
        // Real data: a run with pattern q first (its arena saved), then one with p that stays alive:
        // bytes equal to the pattern in both were not written
        run_register_fields(c, false, 1, q);
        const DWORD used_q = g_used[1];
        memcpy(g_copy, g_arena[1], used_q);
        free_made(1);
        run_register_fields(c, false, 1, p);
        const CClassInfo *k = *at<CClassInfo **>(c.var);
        const CClassInfo *m = g_made[1].cls;
        if (c.mask_made)
            c.mask_made(g_made[1].cls);
        const int pattern_before = g_pattern_pointers;
        bool same_real = k && used_q == g_used[1] && k->num_fields == m->num_fields && k->num_inherited == m->num_inherited;
        DWORD bad = 0xffffffff;
        for (DWORD i = 0; same_real && i < m->num_fields; i++)
            if (!same_registered(reinterpret_cast<const BYTE *>(k->fields[i]), ptrv(m->fields[i]), p, q, 0)) {
                same_real = false;
                bad = i;
            }
        wsprintfA(d, "[registered: %u fields (%u inherited), made %u; field %d, +%x: %x / %x]", k ? k->num_fields : 0,
                  k ? k->num_inherited : 0, m->num_fields, (int)bad, g_diff_at, g_diff_a, g_diff_b);
        if (!same_real && real.mismatches < 2 && bad != 0xffffffff) {
            log_field("registered", k->fields[bad]);
            log_field("made", m->fields[bad]);
        }
        if (g_pattern_pointers != pattern_before)
            log("    %s: %d dword(s) with bytes equal to the patterns compared whole (pointers of other pool blocks)", c.name,
                g_pattern_pointers - pattern_before);
        real.check(same_real, c.name, d);
        free_made(1);
    }
    *at<DWORD *>(FIND_FIELD_CACHE) = saved_cache;
}

// The registered class info against the table (real data): size, abstract, Create, CreateArray
inline void test_registered_class(Tally &t, const ClassDef &c)
{
    const CClassInfo *k = *at<CClassInfo **>(c.var);
    char d[160];
    const bool same = k && k->size == c.size && ptrv((const void *)k->create) == c.create &&
                      ptrv((const void *)k->create_array) == c.create_array;
    wsprintfA(d, "[registered class: size %x, create %x, array %x]", k ? k->size : 0, k ? ptrv((const void *)k->create) : 0,
              k ? ptrv((const void *)k->create_array) : 0);
    t.check(same, c.name, d);
}

// A first run of the original outside the arena: what the code loads on first use (the fonts of the
// resource fields' defaults, a resource list entry) must not end up in an arena that later cases fill
inline void warm_up(const ClassDef &c)
{
    hooks_out();
    if (c.create && c.destroy)
        at<Create0>(c.create)();   // kept alive, like the descriptors below
    if (c.register_fields) {
        CClassInfo *&var = *at<CClassInfo **>(c.var);
        CClassInfo *const real = var;
        CClassInfo *fake = static_cast<CClassInfo *>(heap_alloc(sizeof(CClassInfo)));
        memset(fake, 0, sizeof(CClassInfo));
        fake->name = real->name;
        fake->parent = real->parent;
        at<void(THISCALL *)(CClassInfo *)>(0x415690)(fake);   // InheritFields
        const DWORD saved_cache = *at<DWORD *>(FIND_FIELD_CACHE);
        var = fake;
        at<DWORD(__cdecl *)()>(c.register_fields)();
        var = real;
        *at<DWORD *>(FIND_FIELD_CACHE) = saved_cache;
        // the descriptors stay alive: the resources their defaults hold (fonts) stay loaded, so later
        // runs find them instead of loading them into an arena
        (void)fake;
    }
    hooks_in();
}

// All object tests of a table of classes
inline void test_classes(Tally &objects, Tally &getters, Tally &fields, Tally &real, const ClassDef *defs, int n)
{
    for (int i = 0; i < n; i++)
        warm_up(defs[i]);
    for (int i = 0; i < n; i++) {
        const ClassDef &c = defs[i];
        if (c.create && c.destroy) {
            for (unsigned k = 0; k < (unsigned)scaled(12); k++)
                test_create(objects, c, 100u * (unsigned)i + k);
            for (int count = 0; count < 4; count++)
                test_create_array(objects, c, count, (BYTE)(0x31 + count * 0x40));
        }
        for (DWORD g : c.getters)
            if (g)
                test_getter(getters, c, g);
        if (c.register_fields)
            test_register_fields(fields, real, c, 7u + (unsigned)i);
        if (c.create && c.var)
            test_registered_class(real, c);
    }
}

}  // namespace zax::invcheck
