// Checks of the small object classes and the canned object list (object_classes.cpp) against the
// original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (pointers made relative; masked where the original leaves undefined
// bits), an event log of the fakes' slots and the state of every object afterwards.
//  - Objects of the seven classes: Create (on a pool block filled with a pattern first, so every byte is
//    defined; CWaterWaveFormData is bigger than the pool's largest size and comes from the C heap, which
//    other threads share: only the bytes its constructor writes), CreateArray with 0-3 elements, the
//    deleting destructors (single through slot 1, array called by address: an empty array has no element
//    whose vtable could be read) with fakes as owned objects and arrays, freeing or not, and once more
//    with the class info swapped for one without fields, so the destructors' own code meets the members
//    (CSphereOfZinData's trigger array).
//  - Class getters and RegisterFields: ClassInfo_Register 0x415780 jumps to a stand-in; RegisterFields
//    run into a fake class info, their descriptors compared with each other and with the descriptors
//    registered at startup.
//  - The canned object list: fake lists (the real vtable or a copy with recorders in slots 1, 37, 39)
//    with records of fake objects whose slot 2 gives the CCannedObject or the CObject class: slots 35,
//    39, 40, the registry index setter, the deleting destructor; the constructor 0x468ba0 and the alias
//    constructor 0x468b30 on patterned memory; the function-local static 0x468000, its exit destructor
//    and the "Use" names' initializer and exit function on the globals (saved and restored, atexit
//    replaced by a stand-in).
//  - The wave form setters on random objects with zeros, NaNs, infinities, denormals, huge and random
//    values and the values of the game's water files.
//  - Files in a temp folder (reached from the data folder by a relative path): slot 37 with .can files
//    (written back: the bytes compared).
//  - Real data: the global canned object list (its 7 entries: slots 39 and 35, every .can file read
//    with Object_CreateFromFile without writing back, compared with the loaded objects), the three
//    "Water Warp Effects" files (their four wave forms each) and Rules.txt (its CSphereOfZinOptions).
#include "check_util.h"
#include "object_classes.h"
#include "field_kinds.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI GetFullPathNameA(const char *name, DWORD size, char *out, char **file);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);

namespace zax {

int hooks_object_classes(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
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
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_text(const char *s) { g_statehash[g_side] = fnv_text(g_statehash[g_side], s ? s : "(null)"); }
void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }
unsigned text_hash(const char *s) { return fnv_text(2166136261u, s ? s : "(null)"); }

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
    const int n = hooks_object_classes(nullptr);
    if (!hook_sites_fit("object_classes", n, MAX_SITES))
        return;
    hooks_object_classes(list);
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

// ---- Functions under test (exe addresses: side 0 runs the original, side 1 the reimplementation) ------

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
using Getter = CClassInfo *(__cdecl *)();
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }
DWORD list_file_of(void *l, void *o, ZString *out) { return at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x468c70)(l, o, out); }
DWORD list_entry_path(void *l, DWORD i, ZString *out) { return at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x468d20)(l, i, out); }
DWORD list_clear(void *l) { return at<DWORD(THISCALL *)(void *)>(0x468da0)(l); }
DWORD list_set_index(void *l, DWORD i) { return at<DWORD(THISCALL *)(void *, DWORD)>(0x523e20)(l, i); }
DWORD list_dtor(void *l, DWORD flags) { return ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x468150)(l, flags)); }
void *list_create_from_file(void *l, const char *dir, const char *name, void **out)
{
    return at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x468bf0)(l, dir, name, out);
}

void zs_ctor(ZString *z) { original<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_ctor_text(ZString *z, const char *s) { original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
void zs_dtor(ZString *z) { original<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
void st_zstring(const ZString &z) { st_text(z.buffer ? z.c_str() : nullptr); }
void *create_from_file(const char *path, DWORD write_back)
{
    ZString p;
    zs_ctor_text(&p, path);
    void *o = at<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(&p, 0, write_back, nullptr);
    zs_dtor(&p);
    return o;
}
const char *class_name(const void *o)
{
    if (!o)
        return nullptr;
    const CClassInfo *c = vcall<CClassInfo *>(o, 2);
    return c ? c->name : nullptr;
}

// ---- Fakes ---------------------------------------------------------------------------------------------

// Objects: slot 1 records its deletion, slot 2 gives a class (by kind), slot 7 (the monitor) records
constexpr int NFAKE = 24;
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
};
FakeObj g_fakes[NFAKE];
DWORD g_fake_vt[2][8];
CClassInfo *__cdecl fake_class_canned() { return *at<CClassInfo **>(0x6bd10c); }
CClassInfo *__cdecl fake_class_object() { return at<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
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
DWORD THISCALL fake_slot7(FakeObj *f, DWORD flags)
{
    ev('7', f->id, flags);
    return 0x700 + f->id;
}
void init_fakes()
{
    for (int k = 0; k < 2; k++) {
        for (DWORD &s : g_fake_vt[k])
            s = 0;
        g_fake_vt[k][0] = ptrv((void *)&fake_delete0);
        g_fake_vt[k][1] = ptrv((void *)&fake_delete);
        g_fake_vt[k][7] = ptrv((void *)&fake_slot7);
    }
    g_fake_vt[0][2] = ptrv((void *)&fake_class_canned);
    g_fake_vt[1][2] = ptrv((void *)&fake_class_object);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt[i % 2]), 0, (DWORD)i + 1};
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
// A pointer made comparable: fakes by number, known blocks by order and offset, else as it is (exe
// addresses are the same on both sides). Only pointer members go through it; other values are compared
// raw.
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

// Both sides of a case; `run` returns eax (already made comparable) and the mask of its defined bits
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = g_nknown[side] = 0;
        init_fakes();
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r[0], r[1], mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10)
        log_first_difference();
    t.check(same, "object classes", d);
    return same;
}

DWORD random_float()
{
    static constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3c23d70a, 0x41f00000, 0x7fc00000, 0xffc00000,
                                       0x7f800000, 0xff800000, 0x00000001, 0x80000001, 0x7f7fffff, 0x4cbebc20, 0x47c80000,
                                       0x400ccccd, 0x3f666666, 0x40066666, 0x3f8ccccd, 0x40000000, 0x40333333, 0x41700000,
                                       0xc1400000, 0xc1000000, 0x42c80000, 0xc2c80000, 0x5f000000, 0xdf000000};
    return random() % 3 ? VALUES[random() % (sizeof VALUES / 4)] : random();
}

// ---- Objects -------------------------------------------------------------------------------------------

struct ClassDef {
    const char *name;
    DWORD create, create_array, size, var;
    DWORD array_bytes;   // the bytes the constructors (and the list fields' init) write: compared in arrays
    DWORD owned;         // offset of an owned object (slot 1 with 1), 0 = none
    DWORD array;         // offset of an owned new[] array (slot 1 with 3), 0 = none
    DWORD dtor;          // the deleting destructor (slot 1)
};
const ClassDef CLASSES[] = {{"CCannedObject", 0x468280, 0x4682b0, 0x10, 0x6bd10c, 8, 8, 0, 0x468310},
                            {"CObjectHolder", 0x5238c0, 0x5238f0, 0xc, 0x6d013c, 8, 8, 0, 0x523950},
                            {"CObjectCollection", 0x523ac0, 0x523af0, 0x10, 0x6d0140, 0x10, 0, 8, 0x523b50},
                            {"CSphereOfZinOptions", 0x582570, 0x5825a0, 0x10, 0x713800, 8, 0, 0, 0x582600},
                            {"CSphereOfZinData", 0x584210, 0x584260, 0x18, 0x713818, 0x18, 0, 0xc, 0x5842e0},
                            {"CWaterWaveFormData", 0x596a40, 0x596a70, 0x218, 0x713dc4, 8, 0, 0, 0x596ad0},
                            {"CMiniMapOverlay", 0x507070, 0x5070b0, 0x14, 0x6c2dbc, 0xc, 0, 0, 0x507120}};
constexpr DWORD POOL_MAX = 500;   // the pool's largest size: bigger blocks come from the C heap
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

BYTE g_raw[2][0x20];
void st_object(const ClassDef &c, const BYTE *o, bool all_bytes)
{
    memcpy(g_raw[g_side], o, c.size < 0x20 ? c.size : 0x20);
    st(rel(ptrv(o)));
    const DWORD n = all_bytes ? c.size : c.array_bytes;
    for (DWORD i = 0; i < n; i += 4) {
        const DWORD v = *reinterpret_cast<const DWORD *>(o + i);
        st(i == 4 || (c.owned && i == c.owned) || (c.array && i == c.array) ? rel(v) : v);
    }
}

// Values put into an object before it is destroyed: plain fields random (all written, so no heap bytes
// of an array element stay undefined), owned objects and arrays fakes
void fill_object(const ClassDef &c, BYTE *o, int fake)
{
    for (DWORD i = c.create == 0x507070 ? 0xc : 8; i + 4 <= c.size && i < 0x18; i += 4)   // (CMiniMapOverlay: +8 a ZString)
        *reinterpret_cast<DWORD *>(o + i) = random() % 2 ? random_float() : 0;
    if (c.create == 0x468280)
        *reinterpret_cast<DWORD *>(o + 0xc) = random() % 2;   // "Use": an index into the two names
    if (c.owned)
        *reinterpret_cast<DWORD *>(o + c.owned) = random() % 3 ? ptrv(&g_fakes[fake]) : 0;
    if (c.array)
        *reinterpret_cast<DWORD *>(o + c.array) = random() % 3 ? ptrv(&g_fakes[fake]) : 0;
}

// The class info swapped for a copy without fields while `run` runs
template <typename F> void without_fields(const ClassDef &c, F run)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE copy[sizeof(CClassInfo)];
    memcpy(copy, saved, sizeof copy);
    reinterpret_cast<CClassInfo *>(copy)->num_fields = 0;
    reinterpret_cast<CClassInfo *>(copy)->num_inherited = 0;
    var = reinterpret_cast<CClassInfo *>(copy);
    run();
    var = saved;
}

DWORD objects_scenario(int k, DWORD &mask)
{
    const ClassDef &c = CLASSES[k];
    const bool bare = random() % 3 == 0;
    DWORD result = 0;
    if (random() % 2) {
        // Create on a block filled with a pattern. A heap block is not handed back reliably (other threads
        // allocate from the same heap), so a heap-sized object compares only what its constructor writes.
        const bool pooled = c.size <= POOL_MAX;
        BYTE *p = nullptr;
        if (pooled) {
            p = static_cast<BYTE *>(pool_alloc(c.size));
            const BYTE fill = (BYTE)random();
            memset(p, fill, c.size);
            pool_free(p, c.size);
        }
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        know(o, c.size);
        if (pooled)
            ev('c', o == p);
        st_object(c, o, pooled && o == p);
        fill_object(c, o, 1);
        const DWORD flags = random() % 2;
        if (bare)
            without_fields(c, [&] { result = rel(ptrv(destroy(o, flags))); });
        else
            result = rel(ptrv(destroy(o, flags)));
        if (pooled || !flags)   // (a freed heap block may already belong to another thread)
            st_bytes(o + 8, c.size < 0x18 ? c.size - 8 : 0x10);
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
            fill_object(c, e + i * c.size, 2 + i);
        }
        const DWORD flags = 2 | random() % 2;
        auto dtor = at<void *(THISCALL *)(void *, DWORD)>(c.dtor);
        if (bare)
            without_fields(c, [&] { result = rel(ptrv(dtor(e, flags))); });
        else
            result = rel(ptrv(dtor(e, flags)));
        if (!(flags & 1)) {
            for (int i = 0; i < n; i++)
                st_bytes(e + i * c.size + 8, c.size < 0x18 ? c.size - 8 : 0x10);
            heap_free(e - 4);
        }
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
const GetterDef GETTERS[] = {{0x468230, 0x6bd10c}, {0x468390, 0x6bd10c}, {0x468b50, 0x6bd10c}, {0x5239d0, 0x6d013c},
                             {0x523bd0, 0x6d0140}, {0x582520, 0x713800}, {0x582680, 0x713800}, {0x5841c0, 0x713818},
                             {0x5843d0, 0x713818}, {0x5969f0, 0x713dc4}, {0x5071c0, 0x6c2dbc}};

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
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_ENUM_INT: return same(0x28, 0x44);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_LIST: return same(0x28, 0x44);
    case VT_TYPED_RESOURCE: return same(0x28, 0x2e) && same(0x30, 0x38);   // +0x2e, +0x2f padding
    case VT_STRING:
        return same(0x28, 0x29) && same(0x30, 0x34) &&
               same_text(*reinterpret_cast<const ZString *>(x + 0x2c), *reinterpret_cast<const ZString *>(y + 0x2c));
    case VT_INT: return same(0x28, 0x40);
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
    log("    %s: vt %x name '%s' units '%s' help '%.30s' off %x %x %x", what, f->vftable, text(f->name), text(f->units),
        text(f->help), f->offset, f->unknown14, f->unknown18);
    log("      %x %x %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
}

struct RegFields {
    const char *name;
    DWORD address, var;
};
const RegFields REGISTER_FIELDS[] = {{"CCannedObject", 0x4683f0, 0x6bd10c},       {"CObjectHolder", 0x523a30, 0x6d013c},
                                     {"CObjectCollection", 0x523c30, 0x6d0140},   {"CSphereOfZinOptions", 0x5826e0, 0x713800},
                                     {"CSphereOfZinData", 0x584430, 0x713818},    {"CWaterWaveFormData", 0x596b70, 0x713dc4},
                                     {"CMiniMapOverlay", 0x507220, 0x6c2dbc}};

struct Made {
    CFieldDesc *fields[8];
    DWORD n, result;
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
    out.result = at<DWORD(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
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
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x, parent %x / %x]", g.address, set[0],
                  set[1], results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version,
                  calls[0].parent, calls[1].parent);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "object classes", d);
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
            bool same = m[0].n == m[1].n && m[0].n > 0;
            // eax: ClassInfo_AddField's (undefined) or, for CSphereOfZinOptions, SetHelp's (the help string)
            if (rf.address == 0x5826e0 && same)
                same = m[0].result - ptrv(m[0].fields[m[0].n - 1]) == m[1].result - ptrv(m[1].fields[m[1].n - 1]);
            for (DWORD i = 0; i < m[0].n && i < 8 && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[140];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x]", rf.name, var_zero, m[0].n, m[1].n, m[0].result,
                      m[1].result);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < 8; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, rf.name, d);
            // Real data: the class's own descriptors as registered at startup
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == m[1].n;
            for (DWORD i = 0; same_real && i < m[1].n && i < 8; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[1].fields[i]);
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

// ---- The canned object list ----------------------------------------------------------------------------

alignas(4) BYTE g_list_mem[2][0x140];
DWORD g_vt_copy[2][48];

void *THISCALL rec_slot37(ZDefinitionList *, const char *dir, const char *name, void **out)
{
    ev('C', text_hash(dir), text_hash(name), rel(ptrv(out)));
    return &g_fakes[NFAKE - 3];
}
DWORD THISCALL rec_slot39(ZDefinitionList *, DWORD i, ZString *out)
{
    ev('9', i, rel(ptrv(out)));
    return 0x3900 + i;
}
const char *THISCALL rec_slot1(ZDefinitionList *l, DWORD i)
{
    ev('1', i);
    return i < l->num_entries ? l->entries[i].name : "Out Of Range/Name";
}

const char *const NAMES[] = {"Returned a Flag", "Drop Ore in Container", "Sub/Stupid Print Test", "x y", "a/b/c",
                             "Missing One", "Picked up a Flag", "!None", "Normal.can"};
const char *const FOLDERS[] = {"Resources/Canned Objects", "", "Some/Folder/", "x"};

ZDefinitionList *make_list(int n, bool recorders)
{
    auto *l = reinterpret_cast<ZDefinitionList *>(g_list_mem[g_side]);
    memset(l, 0, sizeof *l);
    at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {RESOURCE_ALIASES_VTABLE, nullptr, 0, 0};
    l->registry_index = random();
    if (recorders) {
        memcpy(g_vt_copy[g_side], at<void *>(CANNED_LIST_VTABLE), 41 * 4);
        if (random() % 2)
            g_vt_copy[g_side][1] = ptrv((void *)&rec_slot1);
        g_vt_copy[g_side][37] = ptrv((void *)&rec_slot37);
        if (random() % 2)
            g_vt_copy[g_side][39] = ptrv((void *)&rec_slot39);
        l->records.vftable = ptrv(g_vt_copy[g_side]);
        know(g_vt_copy[g_side], sizeof g_vt_copy[0]);
    } else {
        l->records.vftable = CANNED_LIST_VTABLE;
    }
    lstrcpyA(l->folder, FOLDERS[random() % 4]);
    l->list_name = "Canned Objects";
    know(l, sizeof *l);
    if (n) {
        DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
        block[0] = (DWORD)n + 1;
        auto *e = reinterpret_cast<ZDefinitionRecord *>(block + 1);
        memset(e, 0, (n + 1) * 0x80);
        for (int i = 0; i < n; i++) {
            lstrcpyA(e[i].name, NAMES[random() % 9]);
            e[i].object = random() % 8 < 3 ? nullptr : &g_fakes[i];
            e[i].keep = (BYTE)(random() % 2);
            e[i].in_use = (BYTE)(random() % 2);
            e[i].loading = random() % 6 == 0;
        }
        l->entries = e;
        l->num_entries = (DWORD)n;
        know(block, (n + 1) * 0x80 + 4);
        at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, 1, 0x80, (DWORD)n, e, 0);
    }
    if (random() % 2) {
        const int na = (int)(random() % 4);
        for (int i = 0; i < na; i++)
            at<DWORD(THISCALL *)(void *, const char *, int)>(0x55d150)(&l->aliases, NAMES[random() % 9], (int)(random() % 5));
    }
    if (random() % 2)
        l->monitor = &g_fakes[NFAKE - 1];
    return l;
}

void st_list(const ZDefinitionList *l)
{
    st(rel(l->records.vftable));
    st(rel(ptrv(l->records.data)));
    st(l->records.unknown08);
    st(l->records.stride);
    st(rel(l->records.unknown10));
    st(l->records.num);
    st(l->records.sorted);
    st(l->aliases.vftable);
    st(l->aliases.count);
    for (DWORD i = 0; i < l->aliases.count && l->aliases.data; i++) {
        st_zstring(l->aliases.data[i]->name);
        st((DWORD)l->aliases.data[i]->index);
    }
    st(l->registry_index);
    st(rel(ptrv(l->entries)));
    st(l->num_entries);
    if (l->entries)
        for (DWORD i = 0; i < l->num_entries && i < 64; i++) {
            const ZDefinitionRecord &r = l->entries[i];
            st_text(r.name);
            st(rel(ptrv(r.object)));
            st(r.keep | r.in_use << 8 | r.b7e << 16 | (DWORD)r.loading << 24);
        }
    st_text(l->folder);
    st(rel(ptrv(l->list_name)));
    st(rel(ptrv(l->monitor)));
}

DWORD list_scenario(DWORD &mask)
{
    const int n = (int)(random() % 9);
    const bool recorders = random() % 4 != 0;
    ZDefinitionList *l = make_list(n, recorders);
    st_list(l);
    const int ops = 3 + (int)(random() % 14);
    for (int k = 0; k < ops; k++) {
        ZString out;
        zs_ctor(&out);
        switch (random() % 7) {
        case 0:
        case 1: {
            // An object of the records, a stranger or a null record's object
            const DWORD m = l->entries ? l->num_entries : 0;
            void *o = m && random() % 4 ? l->entries[random() % m].object : &g_fakes[12 + random() % 6];
            if (!o)
                o = &g_fakes[random() % 2];
            // Found, the upper bytes are slot 39's: with the real slot the ZString destructor's (a pool
            // address), so only compared with the recorder
            const DWORD r = list_file_of(l, o, &out);
            const bool defined = !(r & 0xff) || slot(l, 39) == ptrv((void *)&rec_slot39);
            ev('F', r & 0xff, defined ? rel(r & ~0xffu) >> 8 : 0);
            st_zstring(out);
            break;
        }
        case 2: {
            if (!l->entries || !l->num_entries)
                break;
            const DWORD i = random() % l->num_entries;
            ev('P', i, list_entry_path(l, i, &out) != 0);
            st_zstring(out);
            break;
        }
        case 3: ev('I', list_set_index(l, random())); break;
        case 4:
            if (random() % 4 == 0) {
                ev('X', rel(list_clear(l)) != 0);
                st_list(l);
            }
            break;
        default: st_list(l); break;
        }
        zs_dtor(&out);
    }
    st_list(l);
    const DWORD r = rel(list_dtor(l, 0));
    st_list(l);
    mask = 0xffffffff;
    return r;
}

// The constructor 0x468ba0 on patterned memory (pool), then the deleting destructor freeing it; the
// alias array constructor 0x468b30 on patterned memory
DWORD construct_scenario(DWORD &mask)
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0x140));
    memset(p, (int)(random() & 0xff), 0x140);
    know(p, 0x140);
    auto *l = reinterpret_cast<ZDefinitionList *>(p);
    ev('c', rel(ptrv(at<void *(THISCALL *)(void *)>(0x468ba0)(p))));
    st_list(l);
    st_bytes(p + 0x38, 0x100);   // the folder is left as it was
    st(l->records.vftable);
    l->records.vftable = CANNED_LIST_VTABLE;
    if (random() % 2)
        l->monitor = &g_fakes[NFAKE - 2];
    const DWORD flags = random() % 2;
    const DWORD r = rel(list_dtor(l, flags));
    if (!flags) {
        st_list(l);
        pool_free(p, 0x140);
    }
    alignas(4) BYTE a[0x18];
    for (BYTE &b : a)
        b = (BYTE)random();
    know(a, sizeof a);
    ev('a', rel(ptrv(at<void *(THISCALL *)(void *)>(0x468b30)(a))));
    st_bytes(a, sizeof a);
    mask = 0xffffffff;
    return r;
}

// The function-local static 0x468000 and the exit destructor 0x468090 on the global list, the "Use"
// names' initializer and exit function on theirs (all saved and restored, atexit replaced)
DWORD statics_scenario(unsigned seed, DWORD &mask)
{
    SavedBytes saved, guard, names;
    save_bytes(saved, CANNED_LIST, 0x140);
    save_bytes(guard, CANNED_LIST_GUARD, 4);
    save_bytes(names, CANNED_USE_NAMES, 0x18);
    auto *l = at<ZDefinitionList *>(CANNED_LIST);
    memset(l, (int)(random() & 0xff), 0x140);
    BYTE &g = *at<BYTE *>(CANNED_LIST_GUARD);
    g = (BYTE)((random() & 0xfe) | (seed % 4 == 0 ? 1 : 0));
    g_atexit_calls = 0;
    g_atexit_function = 0;
    CodePatch p;
    patch_jump(p, 0x5d0863, (void *)&fake_atexit);
    const DWORD r = ptrv(at<void *(__cdecl *)()>(0x468000)());
    ev('G', r, g, g_atexit_calls * 0x1000000 + g_atexit_function);
    if (seed % 4)
        st_list(l);
    else
        st_bytes(l, 0x140);
    memset(at<void *>(CANNED_USE_NAMES), (int)(random() & 0xff), 0x18);
    g_atexit_calls = 0;
    const DWORD u = at<DWORD(__cdecl *)()>(0x468200)();
    ev('U', u, g_atexit_calls * 0x1000000 + g_atexit_function);
    st_bytes(at<void *>(CANNED_USE_NAMES), 0x18);
    at<void(__cdecl *)()>(0x468220)();   // eax: the caller's
    st_bytes(at<void *>(CANNED_USE_NAMES), 0x18);
    unpatch(p);
    if (seed % 4) {
        if (random() % 2) {
            // Fakes: records with fake objects, aliases, a monitor
            const int n = 1 + (int)(random() % 6);
            DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
            block[0] = (DWORD)n + 1;
            auto *e = reinterpret_cast<ZDefinitionRecord *>(block + 1);
            memset(e, 0, (n + 1) * 0x80);
            for (int i = 0; i < n; i++) {
                lstrcpyA(e[i].name, NAMES[random() % 9]);
                e[i].object = random() % 3 ? &g_fakes[i] : nullptr;
            }
            l->entries = e;
            l->num_entries = (DWORD)n;
            at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, 1, 0x80, (DWORD)n, e, 0);
            at<DWORD(THISCALL *)(void *, const char *, int)>(0x55d150)(&l->aliases, "Electrical", 1);
            if (random() % 2)
                l->monitor = &g_fakes[NFAKE - 2];
        }
        ev('X', at<DWORD(__cdecl *)()>(0x468090)() != 0);
        st_list(l);
    }
    restore_bytes(saved);
    restore_bytes(guard);
    restore_bytes(names);
    mask = 0xffffffff;
    return 0;
}

// ---- The wave form setters -----------------------------------------------------------------------------

DWORD wave_value(int which)
{
    static constexpr int COUNTS[] = {0, 1, 2, 4, 6, 16, 17, 64, 127, 128, 255, -1, -6, 0x7fffffff, (int)0x80000000, 0x40000000};
    if (which == 2)
        return random() % 4 ? (DWORD)COUNTS[random() % 16] : random();
    return random_float();
}

DWORD wave_scenario(DWORD &mask)
{
    alignas(4) BYTE mem[0x218];
    for (DWORD i = 0; i < sizeof mem; i += 4)
        *reinterpret_cast<DWORD *>(mem + i) = random();
    auto *w = reinterpret_cast<CWaterWaveFormData *>(mem);
    *reinterpret_cast<DWORD *>(&w->amplitude) = random_float();
    w->num_waves = (int)wave_value(2);
    DWORD r = 0;
    const int ops = 1 + (int)(random() % 3);
    for (int k = 0; k < ops; k++) {
        const int which = (int)(random() % 3);
        const DWORD v = wave_value(which);
        if (which == 2)
            r = at<DWORD(THISCALL *)(void *, DWORD)>(0x596de0)(mem, v);
        else
            r = bits(at<float(THISCALL *)(void *, DWORD)>(which ? 0x596d80 : 0x596d20)(mem, v));
        ev('S', (DWORD)which, v, r);
        st_bytes(mem, sizeof mem);
    }
    mask = 0xffffffff;
    return r;
}

// ---- Files in the temp folder --------------------------------------------------------------------------

char g_temp[MAX_PATH];       // absolute, with a final backslash
char g_temp_rel[MAX_PATH];   // the same, relative to the data folder ("" when not reachable)

void write_file(const char *name, const char *text)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s.can", g_temp, name);
    HANDLE f = CreateFileA(path, 0x40000000, 0, nullptr, 2, 0x80, nullptr);
    if (f == (HANDLE)(intptr_t)-1)
        return;
    DWORD w;
    WriteFile(f, text, (DWORD)lstrlenA(text), &w, nullptr);
    CloseHandle(f);
}
BYTE g_file_buf[4096];
// Hash and length of the file (0xffffffff: missing), then deleted
DWORD read_and_delete(const char *name)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s.can", g_temp, name);
    HANDLE f = CreateFileA(path, 0x80000000, 1, nullptr, 3, 0x80, nullptr);
    if (f == (HANDLE)(intptr_t)-1)
        return 0xffffffff;
    DWORD n = 0;
    ReadFile(f, g_file_buf, sizeof g_file_buf, &n, nullptr);
    CloseHandle(f);
    DeleteFileA(path);
    st(n);
    st_bytes(g_file_buf, n);
    return n;
}

const char *const CANNED_FILES[] = {
    "CCannedObject\r\n{\r\n\tObject=CSphereOfZinOptions\r\n\t{\r\n\t\tEvil Unit Lenght=102400\r\n\t}\r\n\tUse=Shared Global Instance\r\n}\r\n",
    "CCannedObject\r\n{\r\n\tUse=Local Copy\r\n}\r\n",
    "CCannedObject\r\n{\r\n}\r\n",
    "CCannedObject\r\n{\r\n\tObject=CCannedObject\r\n\t{\r\n\t\tUse=Local Copy\r\n\t}\r\n\tUnknown Field=7\r\n}\r\n",
    "CWaterWaveFormData\r\n{\r\n\tNum Waves=6\r\n\tAmplitude=0.9\r\n\tPixels Per Second=-12\r\n}\r\n",
    "CObjectCollection\r\n{\r\n\tNum Objects=Array\r\n\t{\r\n\t\tItem Count=2\r\n\t\tNum Objects=CObjectHolder\r\n\t\t{\r\n\t\t}\r\n"
    "\t\tNum Objects=CObjectHolder\r\n\t\t{\r\n\t\t}\r\n\t}\r\n}\r\n",
    "CSphereOfZinData\r\n{\r\n}\r\n",
    nullptr,   // missing
};

// An object read from a file: its class, the values of the classes of this module, owned objects' classes
void st_read_object(const void *o, int depth = 0)
{
    st(rel(ptrv(o)));
    if (!o)
        return;
    const char *name = class_name(o);
    st_text(name);
    const BYTE *b = static_cast<const BYTE *>(o);
    const DWORD vt = *reinterpret_cast<const DWORD *>(o);
    if (vt == 0x5ef478 || vt == 0x5fb788) {
        if (vt == 0x5ef478)
            st(*reinterpret_cast<const DWORD *>(b + 0xc));
        const void *inner = *reinterpret_cast<void *const *>(b + 8);
        know(inner, 0x10);
        if (depth < 3)
            st_read_object(inner, depth + 1);
    } else if (vt == 0x5fb7e4) {
        const DWORD n = *reinterpret_cast<const DWORD *>(b + 0xc);
        st(n);
        auto *e = *reinterpret_cast<const BYTE *const *>(b + 8);
        for (DWORD i = 0; i < n && e && i < 8; i++)
            st_bytes(e + i * 0xc, 4);
    } else if (vt == 0x601178) {
        st_bytes(b + 8, 5);
    } else if (vt == 0x6028cc) {
        st_bytes(b + 8, 0x20c);
    } else if (vt == 0x601318) {
        st_bytes(b + 8, 4);
        st_bytes(b + 0x10, 8);
    }
}

DWORD file_scenario(unsigned seed, DWORD &mask)
{
    const DWORD last_hash = text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str());
    char name[32];
    wsprintfA(name, "Canned %u", seed);
    const char *text = CANNED_FILES[seed % 8];
    if (text)
        write_file(name, text);
    ZDefinitionList *l = make_list(0, false);
    void *slot = nullptr;
    void *o = list_create_from_file(l, g_temp_rel, name, random() % 2 ? &slot : nullptr);
    know(o, 0x218);
    const DWORD r = rel(ptrv(o));
    ev('s', rel(ptrv(slot)));
    st_read_object(o);
    ev('f', read_and_delete(name));
    if (o)
        destroy(o, 1);
    list_dtor(l, 0);
    ev('g', text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str()) == last_hash);
    mask = 0xffffffff;
    return r;
}

// The relative path from the data folder to the temp folder (same drive only)
bool make_temp_paths()
{
    char tmp[MAX_PATH], data[MAX_PATH];
    g_temp[0] = g_temp_rel[0] = 0;
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_temp, "%sZaxObjectClasses\\", tmp);
    CreateDirectoryA(g_temp, nullptr);
    if (!GetFullPathNameA(at<const char *>(0x6135ec /* the data folder */), MAX_PATH, data, nullptr))
        return false;
    if ((data[0] | 0x20) != (g_temp[0] | 0x20) || data[1] != ':' || g_temp[1] != ':')
        return false;
    int depth = 0;
    for (const char *s = data + 2; *s; s++)
        if ((*s == '\\' || *s == '/') && s[1] && s[1] != '\\' && s[1] != '/')
            depth++;
    char *o = g_temp_rel;
    for (int i = 0; i < depth; i++) {
        lstrcpyA(o, "..\\");
        o += 3;
    }
    lstrcpyA(o, g_temp + 3);
    const int n = lstrlenA(g_temp_rel);
    if (n && g_temp_rel[n - 1] == '\\')
        g_temp_rel[n - 1] = 0;
    return true;
}

// ---- Real data -----------------------------------------------------------------------------------------

// The global canned object list: slots 39 and 35 for every entry, its file read (not written back) and
// compared with the loaded object
void test_canned_globals(Tally &t, int &entries, int &loaded)
{
    if (!(*at<BYTE *>(CANNED_LIST_GUARD) & 1))
        return;
    auto *g = at<ZDefinitionList *>(CANNED_LIST);
    entries = (int)g->num_entries;
    for (DWORD i = 0; i < g->num_entries; i++) {
        void *o = g->entries[i].object;
        if (o)
            loaded++;
        two_sides(t, "global canned objects", i, [&](DWORD &mask) {
            ZString out, path;
            zs_ctor(&out);
            zs_ctor(&path);
            ev('P', list_entry_path(g, i, &path) != 0);
            st_zstring(path);
            void *probe = o ? o : &g_fakes[0];
            const DWORD f = list_file_of(g, probe, &out);
            ev('F', f & 0xff);
            st_zstring(out);
            void *read = create_from_file(path.c_str(), 0);
            know(read, 0x10);
            st_read_object(read);
            if (read && o)
                ev('=', *reinterpret_cast<DWORD *>(static_cast<BYTE *>(read) + 0xc) ==
                            *reinterpret_cast<DWORD *>(static_cast<BYTE *>(o) + 0xc),
                   lstrcmpiA(class_name(*reinterpret_cast<void **>(static_cast<BYTE *>(read) + 8)),
                             class_name(*reinterpret_cast<void **>(static_cast<BYTE *>(o) + 8))) == 0);
            if (read)
                destroy(read, 1);
            zs_dtor(&path);
            zs_dtor(&out);
            mask = 0xffffffff;
            return 0u;
        });
    }
    two_sides(t, "global canned objects: strangers", 0, [&](DWORD &mask) {
        ZString out;
        zs_ctor(&out);
        for (int k = 0; k < 6; k++) {
            const DWORD f = list_file_of(g, &g_fakes[k], &out);
            ev('F', f & 0xff, rel(f & ~0xffu));
        }
        st_zstring(out);
        zs_dtor(&out);
        mask = 0xffffffff;
        return 0u;
    });
}

// The water warp effect files: the frame's four wave forms (+8, 4 x 0x218 bytes) and "Show Grid"
const char *const WATER_FILES[] = {"Water Warp Effects/Generic Water.txt", "Water Warp Effects/Planet 01 Water.txt",
                                   "Water Warp Effects/!None.txt"};
// Rules.txt: CRules +0x38 the custom rules, +0x2c of those the CSphereOfZinOptions
DWORD real_file_scenario(int k, DWORD &mask)
{
    const DWORD last_hash = text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str());
    void *o = create_from_file(k < 3 ? WATER_FILES[k] : "Rules.txt", 0);
    know(o, 0x880);
    st_text(class_name(o));
    if (o && k < 3) {
        const BYTE *b = static_cast<const BYTE *>(o);
        for (int w = 0; w < 4; w++) {
            const BYTE *wave = b + 8 + w * 0x218;
            st(*reinterpret_cast<const DWORD *>(wave));
            st_bytes(wave + 8, 0x20c);
        }
        st(b[0x868]);
    } else if (o) {
        const BYTE *custom = *reinterpret_cast<BYTE *const *>(static_cast<BYTE *>(o) + 0x38);
        const BYTE *options = custom ? *reinterpret_cast<BYTE *const *>(custom + 0x2c) : nullptr;
        know(custom, 0x30);
        know(options, 0x10);
        st(rel(ptrv(options)));
        if (options) {
            st(*reinterpret_cast<const DWORD *>(options));
            st_bytes(options + 8, 5);
            ev('R', *reinterpret_cast<const DWORD *>(options + 8), options[0xc]);
        }
    }
    if (o)
        destroy(o, 1);
    ev('g', text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str()) == last_hash);
    mask = 0xffffffff;
    return o != nullptr;
}

}  // namespace

ZAX_CHECKS(object_classes)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"object classes: Create, CreateArray, deleting destructors of the seven classes"},
        classes{"object classes: class getters and RegisterFields"},
        registered{"object classes: RegisterFields against the registered fields (real data)"},
        lists{"object classes: canned object list slots, constructors, destructors, statics"},
        waves{"object classes: wave form setters"}, files{"object classes: canned object list slot 37 with files"},
        real{"object classes: canned objects, water files, Rules.txt (real data)"};
    for (int c = 0; c < NCLASSES; c++) {
        log("  object classes: objects of %s", CLASSES[c].name);
        for (unsigned seed = 1; seed <= (unsigned)scaled(80); seed++)
            if (!two_sides(objects, CLASSES[c].name, seed * 7 + (unsigned)c, [c](DWORD &mask) { return objects_scenario(c, mask); }) &&
                objects.mismatches <= 3)
                for (int side = 0; side < 2; side++) {
                    const DWORD *w = reinterpret_cast<const DWORD *>(g_raw[side]);
                    log("    last object side %d: %x %x %x %x %x %x %x %x", side, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
                }
    }
    objects.report();
    log("  object classes: class getters and RegisterFields");
    test_classes(classes, registered);
    classes.report();
    registered.report();
    log("  object classes: canned object list");
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
        two_sides(lists, "list", seed, [](DWORD &mask) { return list_scenario(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++)
        two_sides(lists, "constructors", seed, [](DWORD &mask) { return construct_scenario(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(24); seed++)
        two_sides(lists, "statics", seed, [seed](DWORD &mask) { return statics_scenario(seed, mask); });
    lists.report();
    log("  object classes: wave form setters");
    for (unsigned seed = 1; seed <= (unsigned)scaled(600); seed++)
        two_sides(waves, "wave", seed, [](DWORD &mask) { return wave_scenario(mask); });
    waves.report();
    if (make_temp_paths()) {
        log("  object classes: files in %s (from the data folder: %s)", g_temp, g_temp_rel);
        for (unsigned seed = 1; seed <= (unsigned)scaled(48); seed++)
            two_sides(files, "file", seed, [seed](DWORD &mask) { return file_scenario(seed, mask); });
        if (!RemoveDirectoryA(g_temp))
            log("  could not remove %s", g_temp);
    } else {
        log("  object classes: no temp folder reachable from the data folder, file cases skipped");
    }
    files.report();
    log("  object classes: real data");
    int entries = 0, loaded = 0;
    test_canned_globals(real, entries, loaded);
    int read = 0;
    for (int k = 0; k < 4; k++)
        if (two_sides(real, k < 3 ? "water file" : "Rules.txt", (unsigned)k, [k](DWORD &mask) { return real_file_scenario(k, mask); }))
            read++;
    log("  real data: %d canned objects (%d loaded), %d of 4 files same, %u ms", entries, loaded, read, GetTickCount() - start);
    real.report();
}

}  // namespace zax
