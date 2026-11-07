// Checks of the path classes and the A* search (path_classes.cpp) against the original, in the game
// thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (pointers made relative; only al where the original's upper bytes are
// a node or block address), an event log of the fakes' slots and the state of every object afterwards.
//  - Objects: Create of the four classes (on a pool block filled with a pattern first, so every byte is
//    defined), CreateArray with 0-3 elements, the deleting destructors (single and array, freeing or
//    not) with point arrays and point list arrays filled (the cache's lists are fakes whose slot 1
//    records their deletion).
//  - Class getters and RegisterFields: ClassInfo_Register 0x415780 jumps to a stand-in; RegisterFields
//    run into a fake class info, their descriptors compared with each other and with the descriptors
//    registered at startup (real data).
//  - Path search: way point maps of our own (random graphs with self loops, duplicates and segments
//    that do not touch their way point, grids up to 20 x 20 that run the 300 nodes out, extreme
//    coordinates whose squared distances wrap, odd lengths: 0, negative, NaN, infinity, denormals) and
//    a fake collide checker (slot 23 walkable or not, slot 24 cost factors including 0, negative, NaN
//    and infinity, both by a hash of the two positions); per scenario the search built, 1-6 searches,
//    Unlink, the destructor. Compared: al, every node of the free, open and closed lists (by the order
//    the constructor made them), the segment flags, the point list, the static array 0x6d0500 (built
//    on first use with atexit replaced by a stand-in) and the checker calls.
//  - Point lists and the cache: AddPoint, Acquire on caches of real point lists (made by the side's
//    Create) with paths holding none, a cached or a foreign list.
//  - Helpers: the squared distance, the way point distance (the exe's table square root), the
//    rectangle test, the zero stub, the two empty array constructors, the exit destructor of 0x6d0500.
//  - Real data: the way point map of the running level (CLayer +0x134): searches between random way
//    points with the fake checker (the segment flags saved and put back for each side).
#include "check_util.h"
#include "path_classes.h"
#include "field_kinds.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

int hooks_path_classes(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
bool g_trace = false;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
}
// The state as a hash, plus the first values for the log of a difference
constexpr int STATE_LOG = 40000;
DWORD g_states[2][STATE_LOG];
int g_nstates[2];
void st(DWORD v)
{
    g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4);
    if (g_nstates[g_side] < STATE_LOG)
        g_states[g_side][g_nstates[g_side]] = v;
    g_nstates[g_side]++;
}
void log_first_state_difference()
{
    for (int i = 0; i < g_nstates[0] && i < g_nstates[1] && i < STATE_LOG; i++)
        if (g_states[0][i] != g_states[1][i]) {
            log("    state %d: %x / %x", i, g_states[0][i], g_states[1][i]);
            for (int k = i; k >= 0; k--)
                if ((g_states[0][k] >> 20) == 0x115 || (g_states[0][k] >> 20) == 0x5ea || (g_states[0][k] >> 20) == 0x5e6) {
                    log("    after marker %x at %d", g_states[0][k], k);
                    break;
                }
            for (int k = i; k >= 0; k--)
                if ((g_states[0][k] >> 16) == 0x5ea7) {
                    log("    in search %x at %d", g_states[0][k], k);
                    break;
                }
            for (int k = i - 14 > 0 ? i - 14 : 0; k < i + 8 && k < g_nstates[0] && k < STATE_LOG; k++)
                log("      [%d] %x / %x", k, g_states[0][k], g_states[1][k]);
            return;
        }
}

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_path_classes(nullptr);
    if (!hook_sites_fit("path_classes", n, MAX_SITES))
        return;
    hooks_path_classes(list);
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
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Fakes and relative values -------------------------------------------------------------------------

// Objects in the cache's arrays: slots 0 and 1 record their deletion
constexpr int NFAKE = 16;
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
};
FakeObj g_fakes[NFAKE];
DWORD g_fake_vt[4];
FakeObj *THISCALL fake_delete0(FakeObj *f, DWORD flags)
{
    ev('d', f->id, flags);
    return f;
}
FakeObj *THISCALL fake_delete(FakeObj *f, DWORD flags)
{
    ev('D', f->id, flags);
    return f;
}
void init_fakes()
{
    g_fake_vt[0] = ptrv((void *)&fake_delete0);
    g_fake_vt[1] = ptrv((void *)&fake_delete);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt), 0, (DWORD)i + 1};
}

// The objects and blocks of a case in the order they appeared (same order on both sides)
constexpr int NKNOWN = 64;
struct Known {
    DWORD at_, size;
};
Known g_known[2][NKNOWN];
int g_nknown[2];
void know(const void *p, DWORD size = 4)
{
    if (p && g_nknown[g_side] < NKNOWN)
        g_known[g_side][g_nknown[g_side]++] = {ptrv(p), size};
}
bool is_known(const void *p)
{
    for (int i = 0; i < g_nknown[g_side]; i++)
        if (g_known[g_side][i].at_ == ptrv(p))
            return true;
    return false;
}
DWORD rel(DWORD v)
{
    if (!v)
        return 0;
    for (int i = 0; i < NFAKE; i++)
        if (v == ptrv(&g_fakes[i]))
            return 0xfa000000u + (DWORD)i;
    for (int i = g_nknown[g_side] - 1; i >= 0; i--)
        if (v >= g_known[g_side][i].at_ && v < g_known[g_side][i].at_ + g_known[g_side][i].size)
            return 0xb0000000u + (DWORD)i * 0x10000 + (v - g_known[g_side][i].at_);
    return v;
}

void (*g_side_setup)(int side);   // called before each side (global state the case needs)
void (*g_side_cleanup)(int side);

// Both sides of a case; `run` returns eax (already made comparable) and the mask of its defined bits
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = g_nknown[side] = g_nstates[side] = 0;
        init_fakes();
        if (g_side_setup)
            g_side_setup(side);
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
        if (g_side_cleanup)
            g_side_cleanup(side);
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %d %x / %d %x]", what, seed, r[0], r[1], mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_nstates[0], g_statehash[0], g_nstates[1], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1] && g_nstates[0] == g_nstates[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        log_first_state_difference();
    }
    t.check(same, "path_classes", d);
    return same;
}

DWORD random_float()
{
    static constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3c23d70a, 0x41f00000, 0x7fc00000, 0xffc00000,
                                       0x7f800000, 0xff800000, 0x00000001, 0x80000001, 0x3fc00000, 0x4e6e6b28, 0x43b40000,
                                       0x7f7fffff, 0x7fa00000, 0x42800000};
    return random() % 3 ? VALUES[random() % (sizeof VALUES / 4)] : random();
}

// ---- Functions under test (exe addresses: side 0 runs the original, side 1 the reimplementation) ------

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
using Getter = CClassInfo *(__cdecl *)();
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }
CPointList *new_point_list() { return static_cast<CPointList *>(at<Create0>(0x5360e0)()); }
DWORD add_point(CPointList *l, const ZIntPoint *p) { return at<DWORD(THISCALL *)(CPointList *, const ZIntPoint *)>(0x536450)(l, p); }
DWORD acquire(void *cache, BYTE *path) { return at<DWORD(THISCALL *)(void *, BYTE *)>(0x537370)(cache, path); }
ZPathSearch *search_ctor(ZPathSearch *s) { return at<ZPathSearch *(THISCALL *)(ZPathSearch *)>(0x5354e0)(s); }
void search_dtor(ZPathSearch *s) { at<void(THISCALL *)(ZPathSearch *)>(0x535560)(s); }
DWORD search(ZPathSearch *s, void *mover, void *map, int start, int goal, CPointList *out, void *checker)
{
    return at<DWORD(THISCALL *)(ZPathSearch *, void *, void *, int, int, CPointList *, void *)>(0x535610)(s, mover, map, start, goal,
                                                                                                         out, checker);
}
ZPathNode *search_unlink(ZPathSearch *s, ZPathNode *n) { return at<ZPathNode *(THISCALL *)(ZPathSearch *, ZPathNode *)>(0x535820)(s, n); }

// ---- Objects -------------------------------------------------------------------------------------------

struct ClassDef {
    const char *name;
    DWORD create, create_array, size, array_mask;   // array_mask: bit i = byte i is written by the constructor
};
// CreateArray takes its block from operator new; Length/AngleAToB and "Is Available" are not written
const ClassDef CLASSES[] = {{"CPathCollideChecker", 0x535c50, 0x535c80, 8, 0xff},
                            {"CPointList", 0x5360e0, 0x536130, 0x1c, 0x0ffff0ff},
                            {"CPathPointListListCache", 0x536fe0, 0x537030, 0x18, 0xffffff},
                            {"CPathLineSegment", 0x539b40, 0x539b90, 0x20, 0xff00ffff}};

void st_object(const ClassDef &c, const BYTE *o, bool all_bytes)
{
    st(rel(ptrv(o)));
    for (DWORD i = 0; i < c.size; i++)
        if (all_bytes || (c.array_mask >> i & 1))
            st(o[i]);
}

// The point array or the cache's list array of an object filled (pool data, as the classes keep it)
void fill_arrays(int k, BYTE *o)
{
    if (k == 1) {
        ZPtrArray &a = reinterpret_cast<CPointList *>(o)->points;
        a.capacity = random() % 6;
        a.data = a.capacity ? static_cast<void **>(pool_get_alloc(a.capacity * 8)) : nullptr;
        a.count = a.capacity ? random() % (a.capacity + 1) : 0;
        for (DWORD i = 0; i < a.capacity * 2; i++)
            reinterpret_cast<DWORD *>(a.data)[i] = random();
    } else if (k == 2) {
        ZPtrArray &a = reinterpret_cast<CPathPointListListCache *>(o)->lists;
        a.capacity = random() % 5;
        a.data = a.capacity ? static_cast<void **>(pool_get_alloc(a.capacity * 4)) : nullptr;
        a.count = a.capacity ? random() % (a.capacity + 1) : 0;
        for (DWORD i = 0; i < a.capacity; i++)
            a.data[i] = random() % 4 ? &g_fakes[random() % NFAKE] : nullptr;
    } else if (k == 3) {
        for (DWORD i = 8; i < 0x20; i += 4)
            *reinterpret_cast<DWORD *>(o + i) = random_float();
    }
}

DWORD objects_scenario(int k, DWORD &mask)
{
    const ClassDef &c = CLASSES[k];
    DWORD result = 0;
    if (random() % 2) {
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        know(o, c.size);
        ev('c', o == p);
        st_object(c, o, o == p);
        fill_arrays(k, o);
        const DWORD flags = random() % 2;
        result = rel(ptrv(destroy(o, flags)));
        if (!flags)
            pool_free(o, c.size);
    } else {
        const int n = (int)(random() % 4);
        BYTE *e = static_cast<BYTE *>(at<CreateN>(c.create_array)(n, 0, 0));
        know(e ? e - 4 : nullptr, n * c.size + 4);
        st(rel(ptrv(e)));
        if (!e)
            return 0;
        st(*reinterpret_cast<DWORD *>(e - 4));
        for (int i = 0; i < n; i++) {
            st_object(c, e + i * c.size, false);
            fill_arrays(k, e + i * c.size);
        }
        const DWORD flags = 2 | random() % 2;
        result = rel(ptrv(destroy(e, flags)));
        if (!(flags & 1))
            heap_free(e - 4);
    }
    mask = 0xffffffff;
    return result;
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create),
                  ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

struct GetterDef {
    DWORD address, var;
};
const GetterDef GETTERS[] = {{0x535c00, 0x6d052c}, {0x535d60, 0x6d052c}, {0x536090, 0x6d0658}, {0x5362b0, 0x6d0658},
                             {0x536f90, 0x6d0660}, {0x5371a0, 0x6d0660}, {0x539af0, 0x6d06a4}, {0x539c80, 0x6d06a4}};

bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    const char *x = a.c_str(), *y = b.c_str();
    while (*x && *x == *y)
        x++, y++;
    return *x == *y;
}
bool same_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || !same_text(a->name, b->name) || !same_text(a->help, b->help) ||
        !same_text(a->group, b->group) || !same_text(a->alt_name, b->alt_name) || !same_text(a->units, b->units) ||
        a->offset != b->offset || a->unknown14 != b->unknown14 || a->unknown18 != b->unknown18)
        return false;   // +0x24 is not written by the constructors
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    auto same = [&](DWORD from, DWORD to) { return memcmp(x + from, y + from, to - from) == 0; };
    switch (a->vftable) {
    case VT_BYTE: return same(0x28, 0x2d);
    case VT_INT_POINT: return same(0x28, 0x34);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_ENUM_INT:
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_OBJECT_ARRAY:
        return same(0x28, 0x38) && same(0x3c, 0x45) &&
               same_desc(reinterpret_cast<const CObjectArrayFieldDesc *>(a)->element,
                         reinterpret_cast<const CObjectArrayFieldDesc *>(b)->element);
    }
    return false;
}

void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' group '%s' units '%s' off %x %x %x", what, f->vftable, text(f->name), text(f->group),
        text(f->units), f->offset, f->unknown14, f->unknown18);
    log("      %x %x %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
}

struct RegFields {
    const char *name;
    DWORD address, var;
};
const RegFields REGISTER_FIELDS[] = {{"CPointList", 0x536310, 0x6d0658},
                                     {"CPathPointListListCache", 0x537200, 0x6d0660},
                                     {"CPathLineSegment", 0x539ce0, 0x6d06a4}};

struct Made {
    CFieldDesc *fields[8];
    DWORD n, alt_name;
};
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    at<void(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
    out.alt_name = ptrv(c->alt_name);
    for (DWORD i = 0; i < 8; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < 8; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_classes(Tally &t, Tally &real)
{
    for (const GetterDef &g : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(g.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<Getter>(g.address)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<Getter>(g.address)();
            unpatch(p);
            results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
            calls[side] = g_register;
            if (var)
                pool_free(var, sizeof(CClassInfo));
            var = saved;
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x]", g.address, set[0], set[1],
                  results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "path_classes", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n && m[0].alt_name == m[1].alt_name;
            for (DWORD i = 0; i < m[0].n && i < 8 && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[140];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, alt name %x / %x]", rf.name, var_zero, m[0].n, m[1].n,
                      m[0].alt_name, m[1].alt_name);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < 8; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, rf.name, d);
            // Real data: the class's own descriptors as the original registered them at startup
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == m[1].n;
            for (DWORD i = 0; same_real && i < m[1].n && i < 8; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[1].fields[i]);
            if (rf.address == 0x537200)
                same_real = same_real && ptrv(c->alt_name) == m[1].alt_name;
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c ? c->num_fields - c->num_inherited : 0, m[1].n);
            if (!same_real && real.mismatches < 2)
                for (DWORD i = 0; i < m[1].n && i < 8; i++) {
                    log_desc("registered", c->fields[c->num_inherited + i]);
                    log_desc("made", m[1].fields[i]);
                }
            real.check(same_real, rf.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
}

// ---- Way point maps of our own and the fake collide checker ---------------------------------------------

constexpr int MAXW = 420, MAXS = 1800, MAXSEG_PER_WP = 12;
ZPathWayPoint g_wp[MAXW];
void *g_wp_ptrs[MAXW];
CPathLineSegment g_seg[MAXS];
void *g_seg_ptrs[MAXS];
int g_seg_lists[MAXW][MAXSEG_PER_WP];
DWORD g_fake_map[13];   // CWayPointMap: +0xc the way points, +0x10 their count, +0x1c the segments, +0x20 their count
int g_nwp, g_nseg;

struct MapView {
    void *map;
    ZPathWayPoint **wp;
    CPathLineSegment **seg;
    int nwp, nseg;
};
MapView g_view;

void add_segment(int a, int b, DWORD length)
{
    if (g_nseg >= MAXS)
        return;
    CPathLineSegment &s = g_seg[g_nseg];
    memset(&s, 0, sizeof s);
    s.vftable = 0x5fc810;
    s.a_index = a;
    s.b_index = b;
    *reinterpret_cast<DWORD *>(&s.length) = length;
    const DWORD r = random() % 10;
    s.flags = r == 0 ? 4 : r == 1 ? 6 : r == 2 ? 2 : 0;
    g_seg_ptrs[g_nseg] = &s;
    const int ends[2] = {a, b};
    for (int e = 0; e < (a == b ? 1 : 2); e++)
        if (g_wp[ends[e]].num_segments < MAXSEG_PER_WP)
            g_seg_lists[ends[e]][g_wp[ends[e]].num_segments++] = g_nseg;
    // now and then a segment listed at a way point it does not touch
    if (random() % 40 == 0) {
        const int w = (int)(random() % (DWORD)g_nwp);
        if (g_wp[w].num_segments < MAXSEG_PER_WP)
            g_seg_lists[w][g_wp[w].num_segments++] = g_nseg;
    }
    g_nseg++;
}

DWORD length_of(const ZPathWayPoint &a, const ZPathWayPoint &b)
{
    const float dx = (float)(a.pos.x - b.pos.x), dy = (float)(a.pos.y - b.pos.y);
    float d = dx * dx + dy * dy;
    // a rough length is enough (the search only multiplies it)
    float r = d > 1 ? d / 2 : 1;
    for (int i = 0; i < 12; i++)
        r = (r + d / r) / 2;
    return bits(r);
}

// Odd lengths, but none below 0: a negative length lets the search give an open node a parent that
// descends from it (the original then loops for ever building the path)
DWORD random_length()
{
    const DWORD v = random_float();
    return (v & 0x7fffffff) > 0x7f800000 || v == 0x80000000 ? v : v & 0x7fffffff;
}

// kind 0 random graph, 1 grid (runs the nodes out), 2 extreme coordinates, 3 odd lengths
void build_map(int kind)
{
    g_nwp = g_nseg = 0;
    int cols = 0;
    if (kind == 1) {
        cols = 4 + (int)(random() % 17);
        g_nwp = cols * (4 + (int)(random() % 17));
    } else {
        g_nwp = 2 + (int)(random() % (kind == 3 ? 12 : 40));
    }
    for (int i = 0; i < g_nwp; i++) {
        ZPathWayPoint &w = g_wp[i];
        memset(&w, 0, sizeof w);
        w.vftable = 0x5fc86c;
        if (kind == 1) {
            w.pos = {(i % cols) * 64 + (int)(random() % 5), (i / cols) * 64};
        } else if (kind == 2) {
            const int big = random() % 2 ? 40000 : 0x7fff0000;
            w.pos = {(int)(random() % 3) * big - big, (int)(random() % 3) * big - big};
            if (random() % 2)
                w.pos.x += (int)(random() % 1000);
        } else {
            w.pos = {(int)(random() % 4000) - 2000, (int)(random() % 4000) - 2000};
            if (i && random() % 8 == 0)
                w.pos = g_wp[random() % (DWORD)i].pos;
        }
        w.seg_vftable = 0x5ec9dc;
        w.segments = g_seg_lists[i];
        g_wp_ptrs[i] = &w;
    }
    if (kind == 1) {
        const int rows = g_nwp / cols;
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < cols; x++) {
                const int i = y * cols + x;
                if (x + 1 < cols && random() % 8)
                    add_segment(i, i + 1, length_of(g_wp[i], g_wp[i + 1]));
                if (y + 1 < rows && random() % 8)
                    add_segment(i, i + cols, length_of(g_wp[i], g_wp[i + cols]));
            }
    } else {
        const int m = g_nwp + (int)(random() % (DWORD)(2 * g_nwp + 1));
        for (int k = 0; k < m; k++) {
            int a = (int)(random() % (DWORD)g_nwp), b = (int)(random() % (DWORD)g_nwp);
            if (random() % 15 == 0)
                b = a;
            const DWORD length = kind == 3 || random() % 10 == 0 ? random_length() : length_of(g_wp[a], g_wp[b]);
            add_segment(a, b, length);
        }
    }
    memset(g_fake_map, 0, sizeof g_fake_map);
    g_fake_map[0] = 0x5fc760;
    g_fake_map[3] = ptrv(g_wp_ptrs);
    g_fake_map[4] = (DWORD)g_nwp;
    g_fake_map[7] = ptrv(g_seg_ptrs);
    g_fake_map[8] = (DWORD)g_nseg;
    g_view = {g_fake_map, reinterpret_cast<ZPathWayPoint **>(g_wp_ptrs), reinterpret_cast<CPathLineSegment **>(g_seg_ptrs),
              g_nwp, g_nseg};
}

// The checker: slot 23 walkable or not, slot 24 the cost factor, both by a hash of the positions
DWORD g_checker_salt;
int g_blocked_one_in = 5;
bool g_odd_costs = true;
unsigned pos_hash(const ZIntPoint *a, const ZIntPoint *b)
{
    unsigned h = fnv(2166136261u ^ g_checker_salt, a, 8);
    return fnv(h, b, 8);
}
BYTE THISCALL checker_blocked(void *, void *mover, const ZIntPoint *a, const ZIntPoint *b)
{
    const unsigned h = pos_hash(a, b);
    const BYTE r = h % (unsigned)g_blocked_one_in == 0;
    ev('w', ptrv(mover), (DWORD)a->x ^ ((DWORD)a->y << 13), (DWORD)b->x ^ ((DWORD)b->y << 13) ^ r);
    return r;
}
constexpr DWORD ODD[] = {0, 0x80000000, 0xbf800000, 0x7fc00000, 0x7f800000, 0x00000001, 0x3f000000, 0x40400000, 0x3dcccccd,
                         0x501502f9};
float THISCALL checker_cost(void *, void *mover, const ZIntPoint *a, const ZIntPoint *b)
{
    const unsigned h = pos_hash(a, b) * 2654435761u;
    DWORD c = 0x3f800000;
    if (g_odd_costs) {
        if (h % 4 == 0)
            c = ODD[(h >> 8) % (sizeof ODD / 4)];
        else if (h % 4 == 1)
            c = 0x3f800000 + ((h >> 4) & 0xfffff);
    } else if (h % 6 == 0) {
        c = 0x40000000;
    }
    ev('k', ptrv(mover), (DWORD)a->x ^ ((DWORD)a->y << 13), (DWORD)b->x ^ ((DWORD)b->y << 13));
    float f;
    memcpy(&f, &c, 4);
    return f;
}
DWORD g_checker_vt[27];
struct FakeChecker {
    DWORD vftable;
    DWORD dynamic;
} g_checker;
void init_checker()
{
    memcpy(g_checker_vt, at<const void *>(0x5eebe8), sizeof g_checker_vt);
    g_checker_vt[23] = ptrv((void *)&checker_blocked);
    g_checker_vt[24] = ptrv((void *)&checker_cost);
    g_checker = {ptrv(g_checker_vt), 0};
}

// ---- The search's state --------------------------------------------------------------------------------

alignas(8) BYTE g_search_mem[2][0x60];
// Node ids: the order of the free list right after the constructor
constexpr int HASH = 1024;
DWORD g_node_key[2][HASH];
WORD g_node_id[2][HASH];
void map_nodes(ZPathSearch *s)
{
    memset(g_node_key[g_side], 0, sizeof g_node_key[g_side]);
    int id = 0;
    for (ZPathNode *n = s->free_nodes.next; n && id < 400; n = n->next, id++) {
        DWORD h = (ptrv(n) >> 3) & (HASH - 1);
        while (g_node_key[g_side][h])
            h = (h + 1) & (HASH - 1);
        g_node_key[g_side][h] = ptrv(n);
        g_node_id[g_side][h] = (WORD)id;
    }
}
DWORD node_id(const void *p)
{
    if (!p)
        return 0xffffffff;
    DWORD h = (ptrv(p) >> 3) & (HASH - 1);
    for (int k = 0; k < HASH && g_node_key[g_side][h]; k++, h = (h + 1) & (HASH - 1))
        if (g_node_key[g_side][h] == ptrv(p))
            return g_node_id[g_side][h];
    return 0xee000000u | (rel(ptrv(p)) & 0xffffff);
}
void st_list(const ZPathNode &sentinel)
{
    int k = 0;
    for (const ZPathNode *n = sentinel.next; n && k < 400; n = n->next, k++) {
        st(node_id(n));
        st(bits(n->g));
        st(bits(n->h));
        st(bits(n->f));
        st(ptrv(n->waypoint));
        st((DWORD)n->index);
        st(node_id(n->parent));
    }
    st((DWORD)k);
}
void st_search(ZPathSearch *s)
{
    st(0x11500001);
    st_list(s->open);
    st(0x11500002);
    st_list(s->closed);
    st(0x11500003);
    st_list(s->free_nodes);
    const ZPathNode *const sentinels[] = {&s->free_nodes, &s->open, &s->closed};
    for (const ZPathNode *x : sentinels)
        st(fnv(2166136261u, x, 0x18));
}
void st_segment_flags()
{
    DWORD h = 2166136261u;
    for (int i = 0; i < g_view.nseg; i++)
        h = fnv(h, &g_view.seg[i]->flags, 4);
    st(0x5e600000);
    st(h);
}

// The static array 0x6d0500 and its guard: each side starts from the same state; the game's own is
// put back at the end
constexpr DWORD PATH_NODES = 0x6d0500, PATH_NODES_GUARD = 0x6d0520;
SavedBytes g_saved_path_nodes;
BYTE g_guard_start;
void path_nodes_setup(int)
{
    auto *a = at<ZPtrArray *>(PATH_NODES);
    *a = {0x5fc61c, nullptr, 0, 0};
    BYTE &guard = *at<BYTE *>(PATH_NODES_GUARD);
    guard = (BYTE)((guard & ~1) | (g_guard_start & 1));
    if (!(guard & 1))
        memset(a, 0x5a, sizeof *a);
}
void path_nodes_cleanup(int)
{
    auto *a = at<ZPtrArray *>(PATH_NODES);
    if ((*at<BYTE *>(PATH_NODES_GUARD) & 1) && a->data && a->capacity && a->capacity < 0x100000)
        pool_get_free(a->data, a->capacity * 4);
    *a = {0x5fc61c, nullptr, 0, 0};
}
void st_path_nodes()
{
    const auto *a = at<const ZPtrArray *>(PATH_NODES);
    st(*at<BYTE *>(PATH_NODES_GUARD) & 1);
    if (!(*at<BYTE *>(PATH_NODES_GUARD) & 1))
        return;
    st(a->vftable);
    st(a->count);
    st(a->capacity);
    for (DWORD i = 0; i < a->count && i < 400; i++)
        st(node_id(a->data[i]));
}

alignas(4) BYTE g_out_mem[2][0x1c];
CPointList *new_out()
{
    auto *l = reinterpret_cast<CPointList *>(g_out_mem[g_side]);
    memset(l, 0, sizeof *l);
    l->vftable = 0x5fc634;
    l->points = {0x5f8dc4, nullptr, 0, 0};
    return l;
}
void st_out(CPointList *l)
{
    st(l->points.count);
    st(l->points.capacity);
    if (l->points.data) {
        st(fnv(2166136261u, l->points.data, l->points.count * 8));
        pool_get_free(l->points.data, l->points.capacity * 8);
    }
}

// One search object, 1-6 searches (and Unlink now and then) on the current map, the destructor
DWORD search_scenario(DWORD &mask, int searches, bool unlink)
{
    auto *s = reinterpret_cast<ZPathSearch *>(g_search_mem[g_side]);
    memset(s, 0xcc, sizeof g_search_mem[g_side]);
    know(s, sizeof *s);
    st(rel(ptrv(search_ctor(s))));
    map_nodes(s);
    st_search(s);
    DWORD found = 0;
    for (int k = 0; k < searches; k++) {
        const int start = (int)(random() % (DWORD)g_view.nwp);
        const int goal = random() % 6 ? (int)(random() % (DWORD)g_view.nwp) : start;
        void *mover = random() % 2 ? (void *)&g_fakes[random() % NFAKE] : nullptr;
        CPointList *out = new_out();
        g_checker_salt = random();
        if (g_trace)
            log("    side %d search %d: %d -> %d of %d", g_side, k, start, goal, g_view.nwp);
        const DWORD r = search(s, mover, g_view.map, start, goal, out, &g_checker);
        if (g_trace)
            log("    found %x", r);
        found += r & 1;
        ev('S', start, goal, r & 0xff);
        st(0x5ea70000u + (DWORD)k);
        st_out(out);
        st_search(s);
        st_segment_flags();
        st_path_nodes();
        if (unlink && random() % 2) {
            // a node of the open or closed list, or one of the free list (stale index)
            const ZPathNode *list = random() % 3 == 0 ? &s->open : random() % 2 ? &s->closed : &s->free_nodes;
            ZPathNode *n = list->next;
            for (DWORD i = random() % 8; n && n->next && i; i--)
                n = n->next;
            if (n) {
                st(node_id(search_unlink(s, n)));
                st_search(s);
            }
        }
    }
    search_dtor(s);
    st(ptrv(s->free_nodes.next) | ptrv(s->open.next) | ptrv(s->closed.next));
    mask = 0xffffffff;
    return found;
}

// ---- Point lists and the cache -------------------------------------------------------------------------

alignas(4) BYTE g_paths[2][3][0x40];
DWORD cache_scenario(DWORD &mask)
{
    auto *cache = static_cast<CPathPointListListCache *>(at<Create0>(0x536fe0)());
    know(cache, sizeof *cache);
    const int n = (int)(random() % 6);
    for (int i = 0; i < n; i++) {
        CPointList *l = new_point_list();
        know(l, sizeof *l);
        l->available = (BYTE)(random() % 3 == 0 ? 0 : 1);
        for (DWORD p = random() % 4; p; p--) {
            const ZIntPoint pt = {(int)random(), (int)random()};
            add_point(l, &pt);
        }
        void *e = l;
        vcall<DWORD>(&cache->lists, 2, &e);
    }
    CPointList *foreign = new_point_list();
    know(foreign, sizeof *foreign);
    foreign->available = (BYTE)(random() % 2);   // not written by Create
    BYTE (*paths)[0x40] = g_paths[g_side];
    memset(paths, 0, sizeof g_paths[g_side]);
    for (int i = 0; i < 3; i++) {
        know(paths[i], 0x40);
        const DWORD r = random() % 3;
        void *l = r == 0 || !cache->lists.count ? nullptr : r == 1 ? cache->lists.data[random() % cache->lists.count] : foreign;
        *reinterpret_cast<void **>(paths[i] + 0x30) = l;
    }
    for (DWORD k = 1 + random() % 5; k; k--) {
        BYTE *path = paths[random() % 3];
        ev('A', rel(ptrv(path)), rel(acquire(cache, path)));
        for (DWORD i = 0; i < cache->lists.count; i++)
            if (!is_known(cache->lists.data[i]))
                know(cache->lists.data[i], sizeof(CPointList));
    }
    st(cache->lists.count);
    st(cache->lists.capacity);
    for (DWORD i = 0; i < cache->lists.count; i++) {
        auto *l = static_cast<CPointList *>(cache->lists.data[i]);
        st(rel(ptrv(l)));
        st(l->available);
        st(l->points.count);
        st(l->points.capacity);
        st(l->points.data != nullptr);
    }
    st(foreign->available);
    for (int i = 0; i < 3; i++)
        st(rel(*reinterpret_cast<DWORD *>(paths[i] + 0x30)));
    // the cache deletes its lists (its field frees them through slot 1)
    bool foreign_cached = false;
    for (DWORD i = 0; i < cache->lists.count; i++)
        foreign_cached |= cache->lists.data[i] == foreign;
    destroy(cache, 1);
    if (!foreign_cached)
        destroy(foreign, 1);
    mask = 0xffffffff;
    return 0;
}

DWORD add_point_scenario(DWORD &mask)
{
    CPointList *l = new_point_list();
    know(l, sizeof *l);
    for (DWORD k = 1 + random() % 12; k; k--) {
        const ZIntPoint pt = {(int)random(), (int)(random() % 3 ? random() % 4096 : 0x80000000u)};
        st(add_point(l, &pt));
    }
    st(l->points.count);
    st(l->points.capacity);
    st(fnv(2166136261u, l->points.data, l->points.count * 8));
    destroy(l, 1);
    mask = 0xffffffff;
    return 0;
}

// ---- Helpers -------------------------------------------------------------------------------------------

int random_coordinate()
{
    switch (random() % 6) {
    case 0: return (int)random();
    case 1: return (int)(random() % 64) - 32;
    case 2: return (int)(0x80000000u + random() % 4);
    case 3: return (int)(0x7ffffffcu + random() % 4);
    default: return (int)(random() % 8000) - 4000;
    }
}

void test_helpers(Tally &t)
{
    g_seed = 99;
    for (int i = 0; i < 3000; i++) {
        const int r[4] = {random_coordinate(), random_coordinate(), random_coordinate(), random_coordinate()};
        int x = random_coordinate(), y = random_coordinate();
        if (random() % 4 == 0)
            x = r[random() % 4];
        if (random() % 4 == 0)
            y = r[random() % 4];
        compare_call<DWORD(THISCALL *)(const int *, int, int)>(t, 0x539ac0, "Rect_Contains", r, x, y);
        const ZIntPoint a = {random_coordinate(), random_coordinate()}, b = {random_coordinate(), random_coordinate()};
        compare_call<int(THISCALL *)(const ZIntPoint *, const ZIntPoint *)>(t, 0x535b60, "SquaredDistance", &a, &b);
        ZPathWayPoint wa{}, wb{};
        wa.pos = a;
        wb.pos = random() % 8 ? b : a;
        if (i < 600) {
            wa.pos = {(int)(random() % 3000), (int)(random() % 3000)};
            wb.pos = {(int)(random() % 3000), (int)(random() % 3000)};
        }
        compare_call<float(__fastcall *)(ZPathWayPoint *, ZPathWayPoint *)>(t, 0x5355d0, "WayPoint_Distance", &wa, &wb);
        if (i < 100)
            compare_call<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(t, 0x536080, "Stub_ReturnZero12", (void *)&g_fakes[0],
                                                                         random(), random(), random());
    }
    // The empty array constructors on a pattern
    const DWORD ctors[] = {0x535b90, 0x5396c0};
    for (DWORD address : ctors)
        for (int i = 0; i < 40; i++) {
            DWORD m[2][5];
            DWORD r[2];
            for (int side = 0; side < 2; side++) {
                for (DWORD &d : m[side])
                    d = 0xa5a5a5a5u + (DWORD)i;
                r[side] = ptrv((side ? at<ZPtrArray *(THISCALL *)(void *)>(address) : original<ZPtrArray *(THISCALL *)(void *)>(address))(
                              m[side])) - ptrv(m[side]);
            }
            t.check(r[0] == r[1] && !memcmp(m[0], m[1], sizeof m[0]), "empty array", "");
        }
}

// The exit destructor of the static array, on a test state of the global (saved and put back)
DWORD at_exit_scenario(DWORD &mask)
{
    auto *a = at<ZPtrArray *>(PATH_NODES);
    const DWORD cap = random() % 6;
    *a = {0x5fc61c, cap ? static_cast<void **>(pool_get_alloc(cap * 4)) : nullptr, random() % (cap + 1), cap};
    at<void(__cdecl *)()>(0x5357d0)();
    st(a->vftable);
    st(ptrv(a->data));
    st(a->count);
    st(a->capacity);
    mask = 0xffffffff;
    return 0;
}

// ---- Real data: the way point map of the running level -------------------------------------------------

struct MemoryInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};
bool readable(const void *p, DWORD n)
{
    MemoryInfo mi;
    if (ptrv(p) < 0x10000)
        return false;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps (then the loop below would not run)
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    return true;
}
bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END); }


DWORD *g_real_flags;
bool find_real_map()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x134, 4))
        return false;
    BYTE *map = *reinterpret_cast<BYTE **>(layer + 0x134);   // the level's map, read from the .way file
    if (!heap_pointer(ptrv(map)) || !readable(map, 0x34) || *reinterpret_cast<DWORD *>(map) != 0x5fc760) {
        log("  path_classes: layer %x has no way point map at +0x134 (%x)", ptrv(layer), ptrv(map));
        return false;
    }
    const DWORD *m = reinterpret_cast<const DWORD *>(map);
    const int nwp = (int)m[4], nseg = (int)m[8];
    if (nwp <= 1 || nwp > 100000 || nseg < 0 || nseg > 400000 || !readable(at<void *>(m[3]), (DWORD)nwp * 4) ||
        !readable(at<void *>(m[7]), (DWORD)nseg * 4))
        return false;
    g_view = {map, at<ZPathWayPoint **>(m[3]), at<CPathLineSegment **>(m[7]), nwp, nseg};
    for (int i = 0; i < nwp; i++)
        if (!readable(g_view.wp[i], 0x24) || (g_view.wp[i]->num_segments && !readable(g_view.wp[i]->segments, g_view.wp[i]->num_segments * 4)))
            return false;
    for (int i = 0; i < nseg; i++)
        if (!readable(g_view.seg[i], 0x20))
            return false;
    return true;
}
void real_setup(int side)
{
    path_nodes_setup(side);
    for (int i = 0; i < g_view.nseg; i++)
        g_view.seg[i]->flags = g_real_flags[i];
}

}  // namespace

ZAX_CHECKS(path_classes)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"path_classes: Create, CreateArray, deleting destructors of the four classes"},
        classes{"path_classes: class getters and RegisterFields"},
        registered{"path_classes: RegisterFields against the registered fields (real data)"},
        searches{"path_classes: A* search on own way point maps"}, lists{"path_classes: point lists, AddPoint, the cache's Acquire"},
        helpers{"path_classes: distances, rectangle test, stub, empty arrays, exit destructor"},
        real{"path_classes: A* search on the level's way point map (real data)"};
    init_fakes();
    for (int c = 0; c < 4; c++)
        for (unsigned seed = 1; seed <= (unsigned)scaled(80); seed++)
            two_sides(objects, CLASSES[c].name, seed * 7 + (unsigned)c, [c](DWORD &mask) { return objects_scenario(c, mask); });
    objects.report();
    test_classes(classes, registered);
    classes.report();
    registered.report();

    // The search: the static array and atexit replaced for the duration
    save_bytes(g_saved_path_nodes, PATH_NODES, 0x24);
    const BYTE guard_saved = *at<BYTE *>(PATH_NODES_GUARD);
    CodePatch atexit_patch;
    patch_jump(atexit_patch, 0x5d0863, (void *)&fake_atexit);
    init_checker();
    g_side_setup = &path_nodes_setup;
    g_side_cleanup = &path_nodes_cleanup;
    int atexits = 0, cases = 0;
    for (unsigned seed = 1; seed <= 400; seed++) {
        const int kind = (int)(seed % 4);
        g_guard_start = (BYTE)(seed % 5 != 0);
        g_blocked_one_in = kind == 1 ? 9 : 5;
        g_odd_costs = kind != 1;
        g_atexit_calls = 0;
        if (g_trace)
            log("  search seed %u", seed);
        // the map is built once per side from the same seed (the search changes segment flags)
        const bool same = two_sides(searches, "search", seed, [kind](DWORD &mask) {
            build_map(kind);
            const int searches_n = 1 + (int)(random() % 6);
            return search_scenario(mask, searches_n, true);
        });
        (void)same;
        atexits += (int)g_atexit_calls;
        cases++;
    }
    log("  path_classes: %d search scenarios, atexit recorded %d times", cases, atexits);
    searches.report();

    // Real data
    g_side_setup = &real_setup;
    if (find_real_map()) {
        g_real_flags = static_cast<DWORD *>(heap_alloc((DWORD)g_view.nseg * 4 + 4));
        for (int i = 0; i < g_view.nseg; i++)
            g_real_flags[i] = g_view.seg[i]->flags;
        int paths = 0;
        g_blocked_one_in = 11;
        g_odd_costs = false;
        for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++) {
            g_guard_start = 1;
            DWORD made = 0;
            two_sides(real, "real search", seed, [&made](DWORD &mask) {
                const DWORD r = search_scenario(mask, 1 + (int)(random() % 4), false);
                made = r;
                return r;
            });
            paths += (int)made;
        }
        for (int i = 0; i < g_view.nseg; i++)
            g_view.seg[i]->flags = g_real_flags[i];
        heap_free(g_real_flags);
        log("  path_classes: real way point map %x: %d way points, %d segments, %d paths found", ptrv(g_view.map), g_view.nwp,
            g_view.nseg, paths);
    } else {
        log("  path_classes: no way point map found in the running level, real data skipped");
    }
    real.report();
    g_side_setup = &path_nodes_setup;
    g_guard_start = 1;
    for (unsigned seed = 1; seed <= (unsigned)scaled(20); seed++)
        two_sides(helpers, "at exit", seed, [](DWORD &mask) { return at_exit_scenario(mask); });
    g_side_setup = nullptr;
    g_side_cleanup = nullptr;
    unpatch(atexit_patch);
    restore_bytes(g_saved_path_nodes);
    *at<BYTE *>(PATH_NODES_GUARD) = guard_saved;

    for (unsigned seed = 1; seed <= (unsigned)scaled(150); seed++) {
        two_sides(lists, "cache", seed, [](DWORD &mask) { return cache_scenario(mask); });
        two_sides(lists, "add point", seed, [](DWORD &mask) { return add_point_scenario(mask); });
    }
    lists.report();
    test_helpers(helpers);
    helpers.report();
    log("  path_classes: %u ms", GetTickCount() - start);
}

}  // namespace zax
