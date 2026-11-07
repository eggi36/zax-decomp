// Checks of the action classes' registration (action_classes.cpp) against the original, in the game
// thread.
//
// Every case runs twice: side 0 with all hooks of the module taken out (the original's first bytes put
// back, so nested calls run the original too), side 1 through the reimplementation. Memory that the C
// heap gives during a case comes from an arena of each side, filled with the same pattern first
// (operator new 0x5d033a and free 0x5d33e0 jump to stand-ins while the game thread runs a case), so
// every byte is defined and pointers into the arena compare by their offset. Values are compared dword
// by dword: equal raw values are equal; otherwise arena pointers by offset and other pointers (string
// buffers of the pool) by their text.
//  - Objects of the 63 classes: Create on a pool block filled with a pattern (all bytes), CreateArray
//    with 0-3 elements (the whole block), each destroyed again through its own (original) slot 1; the
//    three constructors (CAction's, CSwitchMapActionBase's, CPainAction's with values).
//  - Class getters: the four stand-alone getters and the 32 copies in the vtables (slot 2) of the classes
//    0x4b62d0-0x4bffff with the class variable set and 0 (ClassInfo_Register
//    replaced by a stand-in that records its arguments).
//  - RegisterFields of the 59 classes into a fake class info (the parent's fields inherited) and with
//    the class variable 0 (the stand-in registers a fake): everything the C heap gave (descriptors and
//    field arrays), the class info, eax, the FindField cache.
//  - Real data: the registered descriptors of every class (all fields, inherited ones included)
//    against the reimplementation's, bytes the constructors do not write left out (found by a second
//    run with another pattern); every class created and written as text with the registered
//    descriptors; the seven canned object files read (actions in them); the running level's file read
//    into a new layer and written as text again (object ids numbered), with ActionLevelFiles=1 five
//    more levels with 1,139 actions (see LEVELS).
#include "check_util.h"
#include "checks_object_common.h"
#include "action_classes.h"
#include "field_desc.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT DWORD WINAPI GetCurrentThreadId(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT DWORD WINAPI GetModuleFileNameA(HANDLE module, char *out, DWORD n);
WINIMPORT unsigned WINAPI GetPrivateProfileIntA(const char *section, const char *key, int fallback, const char *file);

namespace zax {

int hooks_action_classes(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Memory helpers (as in checks_geometry.cpp) -----------------------------------------------------

bool readable(const void *p, DWORD n)
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

CClassInfo *class_of(const void *p)
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

// ---- Hooks of the module out and in ---------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 512;
SavedSite g_sites[MAX_SITES];
Hook g_hook_list[MAX_SITES];
int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_action_classes(nullptr);
    if (!hook_sites_fit("action_classes", n, MAX_SITES))
        return;
    hooks_action_classes(g_hook_list);
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
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}
int g_side;
void side_begin(int side)
{
    g_side = side;
    if (side == 0)
        hooks_out();
}
void side_end(int side)
{
    if (side == 0)
        hooks_in();
}

// ---- The arena of each side -----------------------------------------------------------------------------

constexpr DWORD ARENA_SIZE = 0x200000;
constexpr int MAX_ALLOCS = 2048;
struct Alloc {
    DWORD offset, size;
};
BYTE *g_arena[2];
BYTE *g_copy;                 // a saved arena (the mask run)
DWORD g_used[2], g_high;
Alloc g_allocs[2][MAX_ALLOCS];
int g_nallocs[2];
int g_arena_side = -1;
DWORD g_thread;
bool g_overflow;
BYTE *g_free_trampoline;
CodePatch g_new_patch, g_free_patch;

bool in_arena(DWORD p, int side) { return g_arena[side] && p >= ptrv(g_arena[side]) && p < ptrv(g_arena[side]) + ARENA_SIZE; }

void *__cdecl arena_new(DWORD n)
{
    const int s = g_arena_side;
    // a new pool chunk stays in the C heap (check_util.h, pool_grow_call)
    if (s >= 0 && GetCurrentThreadId() == g_thread && !pool_grow_call((DWORD)(uintptr_t)__builtin_return_address(0), n)) {
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
DWORD __cdecl arena_free(void *p)
{
    if (in_arena(ptrv(p), 0) || in_arena(ptrv(p), 1))
        return 0;
    return reinterpret_cast<DWORD(__cdecl *)(void *)>(g_free_trampoline)(p);
}

bool arena_setup()
{
    if (g_arena[0])
        return true;
    g_arena[0] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_arena[1] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check_region(g_arena[0], ARENA_SIZE, "action_classes arena 0");
    check_region(g_arena[1], ARENA_SIZE, "action_classes arena 1");
    g_copy = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_free_trampoline = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_arena[0] || !g_arena[1] || !g_copy || !g_free_trampoline)
        return false;
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
void arena_on(int side, BYTE pattern, bool fresh = true)
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
void arena_off()
{
    unpatch(g_free_patch);
    unpatch(g_new_patch);
    if (g_arena_side >= 0 && g_used[g_arena_side] > g_high)
        g_high = g_used[g_arena_side];
    g_arena_side = -1;
}

// ---- Comparing values ------------------------------------------------------------------------------------

// The text of a string buffer pointer (ZStringBuffer: refs, capacity, text), null if it is none
const char *buffer_text(DWORD p)
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
bool same_text(const char *a, const char *b) { return a && b && lstrcmpA(a, b) == 0; }

// A value of side 0 (a) against side 1 (b)
bool same_value(DWORD a, DWORD b)
{
    if (a == b)
        return true;
    if (in_arena(a, 0) && in_arena(b, 1))
        return a - ptrv(g_arena[0]) == b - ptrv(g_arena[1]);
    return same_text(buffer_text(a), buffer_text(b));
}
// Two regions (side 0, side 1) dword by dword; the first difference logged
DWORD g_diff_at, g_diff_a, g_diff_b;
bool same_region(const BYTE *a, const BYTE *b, DWORD n)
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
DWORD g_fake_class[2];
DWORD rel(DWORD v, int side)
{
    if (in_arena(v, side))
        return 0xa0000000u + (v - ptrv(g_arena[side]));
    if (v && v == g_fake_class[side])
        return 0xc1a55000u;
    return v;
}

// ---- Objects ---------------------------------------------------------------------------------------------

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);

BYTE g_obj[2][0x40];

void test_create(Tally &t, const ActionClassDef &c, unsigned seed)
{
    bool reused[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        reused[side] = o == p;
        memcpy(g_obj[side], o, c.size);
        vcall<void *>(o, 1, 1);
        side_end(side);
    }
    char d[160];
    const bool same = reused[0] && reused[1] && same_region(g_obj[0], g_obj[1], c.size);
    wsprintfA(d, "[Create seed %u: block reused %d / %d, first difference +%x: %x / %x]", seed, reused[0], reused[1], g_diff_at, g_diff_a,
              g_diff_b);
    t.check(same, c.name, d);
}

void test_create_array(Tally &t, const ActionClassDef &c, int count, BYTE pattern)
{
    DWORD e[2];
    void *a[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        arena_on(side, pattern);
        a[side] = at<CreateN>(c.create_array)(count, 0, 0);
        e[side] = rel(ptrv(a[side]), side);
        arena_off();
        side_end(side);
    }
    // compared while the elements are alive, then destroyed (array, not freed: the block is the arena's)
    char d[200];
    const bool same = g_used[0] == g_used[1] && e[0] == e[1] && same_region(g_arena[0], g_arena[1], g_used[0]);
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, first difference +%x: %x / %x]", count, e[0], e[1], g_used[0], g_used[1],
              g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
    for (int side = 0; side < 2; side++)
        if (count > 0 && a[side]) {
            arena_on(side, pattern, false);   // the destructors' frees of arena blocks are ignored
            vcall<void *>(a[side], 1, 2);
            arena_off();
        }
}

void test_ctors(Tally &t, unsigned seed)
{
    struct Case {
        const char *name;
        DWORD size;
    };
    static const Case CASES[] = {{"CAction::Ctor", 8}, {"CSwitchMapActionBase::Ctor", 0x1c}, {"CPainAction::CtorValues", 0x18}};
    static const char *const TEXTS[] = {"", "Zax", "$Instigator", LONG_TEXT};
    for (int k = 0; k < 3; k++) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed * 3 + (unsigned)k;
            side_begin(side);
            BYTE *o = static_cast<BYTE *>(pool_alloc(CASES[k].size));
            memset(o, (BYTE)random(), CASES[k].size);
            ZString text;
            original_string_ops().ctor(&text);
            original_string_ops().assign_text(&text, TEXTS[random() % 4]);
            void *made;
            if (k == 0)
                made = at<void *(THISCALL *)(void *)>(0x41a6b0)(o);
            else if (k == 1)
                made = at<void *(THISCALL *)(void *)>(0x4c0fa0)(o);
            else {
                const DWORD hit_points = random(), damage_type = random();
                made = at<void *(THISCALL *)(void *, DWORD, const ZString *, DWORD)>(0x4c3c20)(o, hit_points, &text, damage_type);
            }
            r[side] = made == o;
            memcpy(g_obj[side], o, CASES[k].size);
            if (k == 2)
                vcall<void *>(o, 1, 0);   // the strings released
            original_string_ops().dtor(&text);
            pool_free(o, CASES[k].size);
            side_end(side);
        }
        char d[140];
        const bool same = r[0] == 1 && r[1] == 1 && same_region(g_obj[0], g_obj[1], CASES[k].size);
        wsprintfA(d, "[seed %u: eax this %d / %d, first difference +%x: %x / %x]", seed, r[0], r[1], g_diff_at, g_diff_a, g_diff_b);
        t.check(same, CASES[k].name, d);
    }
}

// ---- Class getters and RegisterFields --------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
bool g_inherit;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
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

void test_getter(Tally &t, const ActionClassDef &c, DWORD getter)
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
Made g_made[2];
void run_register_fields(const ActionClassDef &c, bool var_zero, int side, BYTE pattern)
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
void free_made(int side)
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

void log_field(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' units '%s' help '%.30s' off %x %x %x", what, f->vftable, text(f->name), text(f->units), text(f->help),
        f->offset, f->unknown14, f->unknown18);
    log("      %x %x %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
}

// The class info state of a made class (pointers made comparable)
void class_state(int side, DWORD *out)
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
bool unwritten(DWORD offset, BYTE p, BYTE q) { return g_copy[offset] == q && g_arena[1][offset] == p; }

int find_alloc(DWORD p)
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
bool same_registered(const BYTE *real, DWORD made, BYTE p, BYTE q, int depth)
{
    const int k = find_alloc(made);
    if (k < 0 || !readable(real, g_allocs[1][k].size))
        return false;
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
            for (DWORD b = 0; b < 4; b++)
                if (!unwritten(base + i + b, p, q) && real[i + b] != m[i + b])
                    return false;
            continue;
        }
        if (in_arena(y, 1) && depth < 2 && readable(at<void *>(x), 4)) {   // an element descriptor
            if (!same_registered(at<const BYTE *>(x), y, p, q, depth + 1))
                return false;
            continue;
        }
        if (!same_text(buffer_text(x), buffer_text(y)))
            return false;
    }
    return true;
}

void test_register_fields(Tally &t, Tally &real, const ActionClassDef &c, unsigned seed)
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
        DWORD s0[9], s1[9];
        class_state(0, s0);
        class_state(1, s1);
        bool same = g_used[0] == g_used[1] && g_nallocs[0] == g_nallocs[1] && same_region(g_arena[0], g_arena[1], g_used[0]) &&
                    memcmp(s0, s1, sizeof s0) == 0 && memcmp(&g_made[0].call, &g_made[1].call, sizeof g_made[0].call) == 0 &&
                    s0[0] > s0[1];
        char d[220];
        wsprintfA(d, "[RegisterFields var %d: %u / %u fields (%u inherited), %u / %u bytes, eax %x / %x, first difference +%x: %x / %x]",
                  var_zero, s0[0], s1[0], s0[1], g_used[0], g_used[1], s0[6], s1[6], g_diff_at, g_diff_a, g_diff_b);
        if (!same && t.mismatches < 6) {
            for (int i = 0; i < g_nallocs[0] && i < 24; i++)
                log("    block %d: +%x %x / +%x %x", i, g_allocs[0][i].offset, g_allocs[0][i].size, g_allocs[1][i].offset, g_allocs[1][i].size);
            const DWORD at_ = g_diff_at & ~0x3fu;
            for (DWORD k = 0; k < 0x40 && at_ + k + 4 <= g_used[0]; k += 16) {
                const DWORD *x = reinterpret_cast<const DWORD *>(g_arena[0] + at_ + k), *y = reinterpret_cast<const DWORD *>(g_arena[1] + at_ + k);
                log("    +%x: %x %x %x %x / %x %x %x %x", at_ + k, x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3]);
            }
            log("    class state %x %x %x %x %x %x %x %x %x / %x %x %x %x %x %x %x %x %x", s0[0], s0[1], s0[2], s0[3], s0[4], s0[5], s0[6],
                s0[7], s0[8], s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6], s1[7], s1[8]);
        }
        if (!same && t.mismatches < 3)
            for (DWORD i = 0; i < g_made[0].cls->num_fields && i < g_made[1].cls->num_fields && i < 12; i++) {
                log_field("original", g_made[0].cls->fields[i]);
                log_field("reimpl", g_made[1].cls->fields[i]);
            }
        t.check(same, c.name, d);
        if (var_zero) {
            free_made(0);
            free_made(1);
            continue;
        }
        free_made(0);
        free_made(1);
        // Real data: the registered descriptors against side 1's. A run with pattern q first (its arena
        // saved), then one with p that stays alive: bytes equal to the pattern in both were not written.
        run_register_fields(c, false, 1, q);
        const DWORD used_q = g_used[1];
        memcpy(g_copy, g_arena[1], used_q);
        free_made(1);
        run_register_fields(c, false, 1, p);
        const CClassInfo *k = *at<CClassInfo **>(c.var);
        const CClassInfo *m = g_made[1].cls;
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
        real.check(same_real, c.name, d);
        free_made(1);
    }
    *at<DWORD *>(FIND_FIELD_CACHE) = saved_cache;
}

// ---- Real data: objects written as text ------------------------------------------------------------------

objcheck::Bytes g_text[2];
SavedBytes g_generator;

// The object written as text (mode 1) into out
void text_of(void *o, objcheck::Bytes &out)
{
    CObjectWriter w;
    objcheck::open_stream(&w, 2, 1, nullptr, 0, 0);
    at<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x4168f0)(o, &w);
    const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
    objcheck::store(out, ms->data, ms->data ? ms->size : 0);
    objcheck::close_stream(&w);
}

// A hash of a text with every "0x" + 8 hex digits replaced by the number of its first appearance
unsigned norm_hash(const BYTE *p, DWORD n)
{
    static DWORD seen[8192];
    DWORD nseen = 0;
    unsigned h = 2166136261u;
    auto hex = [](BYTE c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); };
    for (DWORD i = 0; i < n; i++) {
        if (p[i] == '0' && i + 10 <= n && (p[i + 1] == 'x' || p[i + 1] == 'X')) {
            bool all = true;
            DWORD v = 0;
            for (DWORD k = 2; k < 10 && all; k++) {
                all = hex(p[i + k]);
                const BYTE c = p[i + k];
                v = v * 16 + (DWORD)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
            }
            if (all && !(i + 10 < n && hex(p[i + 10]))) {
                DWORD k = 0;
                while (k < nseen && seen[k] != v)
                    k++;
                if (k == nseen && nseen < 8192)
                    seen[nseen++] = v;
                const DWORD tag[2] = {0xbeef, k};
                h = fnv(h, tag, 8);
                i += 9;
                continue;
            }
        }
        h = fnv(h, &p[i], 1);
    }
    return h;
}

// How often "=<class name>" ends a line of the text
int count_class(const objcheck::Bytes &b, const char *name)
{
    const int n = lstrlenA(name);
    int count = 0;
    for (DWORD i = 0; i + (DWORD)n + 1 < b.size; i++)
        if (b.data[i] == '=' && memcmp(b.data + i + 1, name, (DWORD)n) == 0 &&
            (b.data[i + 1 + n] == '\r' || b.data[i + 1 + n] == '\n'))
            count++;
    return count;
}

template <typename F> void both_texts(F run)
{
    for (int side = 0; side < 2; side++) {
        restore_bytes(g_generator);
        side_begin(side);
        g_text[side].size = 0;
        run(side);
        side_end(side);
    }
    restore_bytes(g_generator);
}

void test_object_texts(Tally &t)
{
    for (const ActionClassDef &c : ACTION_CLASSES) {
        if (!c.create)
            continue;
        both_texts([&](int side) {
            void *o = at<Create0>(c.create)();
            text_of(o, g_text[side]);
            vcall<void *>(o, 1, 1);
        });
        char d[120];
        wsprintfA(d, "[created, as text: %u / %u bytes]", g_text[0].size, g_text[1].size);
        t.check(g_text[0].size && objcheck::same_bytes(g_text[0], g_text[1]), c.name, d);
    }
}

void *create_from_file(const char *path)
{
    ZString z;
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&z, path);
    void *o = at<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(&z, 0, 0, nullptr);
    original_string_ops().dtor(&z);
    return o;
}

const char *const CANNED[] = {
    "Resources/Canned Objects/Does player have a flag.can",
    "Resources/Canned Objects/Drop Cystals and Ore.can",
    "Resources/Canned Objects/Drop Ore in Container.can",
    "Resources/Canned Objects/Picked up a Flag.can",
    "Resources/Canned Objects/Returned a Flag.can",
    "Resources/Canned Objects/Stupid Print Test.can",
    "Resources/Canned Objects/Trigger Enemy Killed Quip.can",
};

int g_seen[NUM_ACTION_CLASSES];
void count_seen(const objcheck::Bytes &b)
{
    for (int i = 0; i < NUM_ACTION_CLASSES; i++)
        g_seen[i] += count_class(b, ACTION_CLASSES[i].name);
}

void test_canned(Tally &t)
{
    for (const char *path : CANNED) {
        const char *read_class = nullptr;
        both_texts([&](int side) {
            void *o = create_from_file(path);
            if (side == 0 && class_of(o))
                read_class = class_of(o)->name;
            if (o) {
                text_of(o, g_text[side]);
                vcall<void *>(o, 1, 1);
            }
        });
        count_seen(g_text[0]);
        char d[200];
        wsprintfA(d, "[%s, %u / %u bytes as text]", read_class ? read_class : "(none)", g_text[0].size, g_text[1].size);
        t.check(g_text[0].size && objcheck::same_bytes(g_text[0], g_text[1]), path, d);
    }
}

// Level files read into new layers (as the layer checks do: the world's frame, FileBuf_Open, the first
// line, LoadText 0x4ec090) and written as text into memory (0x4ebe10); the layer deleted again
BYTE *world() { return *at<BYTE **>(0x6c2080); }
BYTE g_entry_flags[2 * 256];
DWORD g_nentries;
void save_entry_flags()
{
    g_nentries = *at<DWORD *>(0x6cfe0c) < 256 ? *at<DWORD *>(0x6cfe0c) : 256;
    for (DWORD i = 0; i < g_nentries; i++) {
        g_entry_flags[2 * i] = (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7d];
        g_entry_flags[2 * i + 1] = (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7e];
    }
}
void restore_entry_flags()
{
    for (DWORD i = 0; i < g_nentries; i++) {
        (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7d] = g_entry_flags[2 * i];
        (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7e] = g_entry_flags[2 * i + 1];
    }
}
constexpr DWORD SOUND_LAYER = 0x6c1c70;

DWORD load_and_write(const char *level, objcheck::Bytes &out)
{
    void *l = pool_alloc(0x1d0);
    at<void *(THISCALL *)(void *, void *)>(0x4e9e00)(l, *reinterpret_cast<void **>(world() + 0x2a8));
    alignas(4) BYTE s[0x24];
    at<void *(THISCALL *)(void *)>(0x5240b0)(s);
    DWORD ok = at<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(s, level, 0x204, 1) & 0xff;
    DWORD r = ok;
    if (ok) {
        char line[256];
        at<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(s, line, sizeof line);
        r |= (at<DWORD(THISCALL *)(void *, void *)>(0x4ec090)(l, s) & 0xff) << 8;
        at<DWORD(THISCALL *)(void *)>(0x524340)(s);
    }
    at<DWORD(THISCALL *)(void *)>(0x5241e0)(s);
    CObjectWriter w;
    objcheck::open_stream(&w, 2, 1, nullptr, 0, 0);
    at<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x4ebe10)(l, &w);
    const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
    objcheck::store(out, ms->data, ms->data ? ms->size : 0);
    objcheck::close_stream(&w);
    vcall<void *>(l, 1, 1);
    return r;
}

// More level files, read only with [Reimpl] ActionLevelFiles=1 in Zax-Reimpl.ini: loading them loads
// resources the running level does not use (fonts, sounds, definitions), and the definitions and the
// multiplayer game type checks after this module then differ between their sides (run
// 20261024-211549-0e57; without them 20261024-213208-d6cc identical). Run 20261024-211414-2fe6 read them.
const char *const LEVELS[] = {
    "Levels/ACT 3/M18/18 Slave Main.zax",
    "Levels/ACT 1/M1M2/01 Main.zax",
    "Levels/ACT 4/M22/22 Heart of Om2.zax",
    "Levels/ACT 2/M13/13 AirTempleMain.zax",
    "Levels/Inside Ship.ZAX",
};

bool more_level_files()
{
    char ini[MAX_PATH + 32];
    GetModuleFileNameA(nullptr, ini, MAX_PATH);
    for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
        ini[n - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    return GetPrivateProfileIntA("Reimpl", "ActionLevelFiles", 0, ini) != 0;
}

void test_level(Tally &t, const char *level)
{
    const DWORD saved_sound = *at<DWORD *>(SOUND_LAYER);
    save_entry_flags();
    DWORD r[2];
    both_texts([&](int side) {
        r[side] = load_and_write(level, g_text[side]);
        restore_entry_flags();
        *at<DWORD *>(SOUND_LAYER) = saved_sound;
    });
    const unsigned h0 = norm_hash(g_text[0].data, g_text[0].size), h1 = norm_hash(g_text[1].data, g_text[1].size);
    int actions = 0;
    for (int i = 0; i < NUM_ACTION_CLASSES; i++)
        actions += count_class(g_text[0], ACTION_CLASSES[i].name);
    count_seen(g_text[0]);
    char d[200];
    wsprintfA(d, "[loaded %x / %x, %u / %u bytes as text, hash %x / %x, %d objects of the module's classes]", r[0], r[1],
              g_text[0].size, g_text[1].size, h0, h1, actions);
    log("  %s: %s", level, d);
    t.check(r[0] == 0x101 && r[0] == r[1] && g_text[0].size == g_text[1].size && h0 == h1, level, d);
}

// The running level's file (its resources are loaded already) and, when switched on, the five above
void test_levels(Tally &t)
{
    BYTE *w = world();
    if (!w || !readable(w + 0x2b8, 0x10))
        return;
    const DWORD *layers = reinterpret_cast<const DWORD *>(w + 0x2b8);   // the world's layer array {.., data, count}
    BYTE *run = layers[2] && readable(at<void *>(layers[1]), 4) ? *at<BYTE **>(layers[1]) : nullptr;
    if (run && readable(run, 0x1d0)) {
        const ZString &name = *reinterpret_cast<const ZString *>(run + 0x3c + 0xa0);   // CLayer.data.partial_name
        char level[MAX_PATH];
        wsprintfA(level, "Levels/%s.zax", name.c_str());
        test_level(t, level);
    }
    if (more_level_files())
        for (const char *level : LEVELS)
            test_level(t, level);
}

}  // namespace

ZAX_CHECKS(action_classes)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"action classes: Create, CreateArray and the constructors"}, getters{"action classes: class getters"},
        fields{"action classes: RegisterFields"}, registered{"action classes: RegisterFields against the registered fields (real data)"},
        texts{"action classes: created objects written as text (real data)"},
        files{"action classes: canned objects and level files read and written (real data)"};
    if (!arena_setup()) {
        log("  action classes: no arena, checks skipped");
        objects.check(false, "arena");
        objects.report();
        return;
    }
    objcheck::init_stream_vtable();
    save_bytes(g_generator, 0x7124c0, GAME_RANDOM_BYTES);
    for (const ActionClassDef &c : ACTION_CLASSES) {
        if (c.create) {
            for (unsigned seed = 1; seed <= 3; seed++)
                test_create(objects, c, seed);
            for (int count = 0; count <= 3; count++)
                test_create_array(objects, c, count, (BYTE)(0x5a + count * 0x31));
        }
        if (c.getter)
            test_getter(getters, c, c.getter);
        if (c.slot_getter)
            test_getter(getters, c, c.slot_getter);
        if (c.register_fields)
            test_register_fields(fields, registered, c, (unsigned)(c.register_fields & 0xffff));
    }
    for (unsigned seed = 1; seed <= 40; seed++)
        test_ctors(objects, seed);
    log("  action classes: arena overflow %d, high water %u bytes", g_overflow, g_high);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    test_object_texts(texts);
    texts.report();
    test_canned(files);
    test_levels(files);
    char seen[1200];
    char *o = seen;
    int kinds = 0;
    for (int i = 0; i < NUM_ACTION_CLASSES && o < seen + sizeof seen - 64; i++)
        if (g_seen[i]) {
            kinds++;
            o += wsprintfA(o, " %s %d", ACTION_CLASSES[i].name, g_seen[i]);
        }
    log("  action classes in the files read: %d classes:%s", kinds, seen);
    files.report();
    restore_bytes(g_generator);
    log("  action classes: %u ms", GetTickCount() - start);
}

}  // namespace zax
