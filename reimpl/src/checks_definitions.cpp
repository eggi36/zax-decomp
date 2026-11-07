// Checks of the definition classes, the definition lists, the hue remap holder and the quip category
// (definitions.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (pointers made relative; masked where the original leaves undefined
// bits), an event log of the fakes' slots and the state of every object afterwards.
//  - Objects: Create (on a pool block filled with a pattern first, so every byte is defined), CreateArray
//    with 0-3 elements, the deleting destructors (single and array, freeing or not), the quip's sound
//    list deleted through its slot 0.
//  - Class getters and RegisterFields: ClassInfo_Register 0x415780 jumps to a stand-in; RegisterFields
//    run into a fake class info, their descriptors compared with each other and with the descriptors
//    registered at startup (real data). The callback CCategoryDefinition's RegisterFields adds to the
//    static list 0x62e6a8 is taken off again.
//  - Definition lists: fake lists (the real vtables or copies with recorders in slots 1, 37, 39, 40)
//    with records of names, fake objects (slot 1 records deletions, slot 2 gives a class) and flags;
//    random operations on them (slots 4, 17, 30, 35, 36, 39, the deleting destructors with a fake
//    monitor and aliases); the static initializers and exit destructors on the globals (saved and
//    restored, atexit replaced by a stand-in).
//  - Files in a temp folder (reached from the data folder by a relative path): LoadOrCreate with
//    existing, empty, odd and missing files (then written: the bytes compared), slot 37 of both lists,
//    slot 30 with the real slot 37.
//  - Real data: Init of both lists on fake lists (the game's Categories and Damage Types folders, and
//    the fallback with Files_Count answering 0 first), compared with the global lists; the global lists'
//    slots 30 (loaded entries), 35 and 39 and the category file callback 0x46a490 with their loaded
//    objects; LoadOrCreate on the real category files (they exist, so nothing is written); the quip
//    category's sounds read from game folders.
#include "check_util.h"
#include "definitions.h"
#include "field_kinds.h"
#include "file_search.h"
#include "dir_list.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI GetFullPathNameA(const char *name, DWORD size, char *out, char **file);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);

namespace zax {

int hooks_definitions(Hook *out);

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
    const int n = hooks_definitions(nullptr);
    if (!hook_sites_fit("definitions", n, MAX_SITES))
        return;
    hooks_definitions(list);
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
using DtorFn = void *(THISCALL *)(void *, DWORD);
using Getter = CClassInfo *(__cdecl *)();
DWORD list_name(void *l) { return ptrv(at<const char *(THISCALL *)(void *)>(0x46acb0)(l)); }
DWORD list_slot17(void *l) { return at<DWORD(THISCALL *)(void *)>(0x46aca0)(l); }
DWORD list_slot36(void *l, DWORD a, DWORD b) { return at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x46ad10)(l, a, b); }
DWORD list_load(void *l, DWORD i) { return ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4777b0)(l, i)); }
DWORD list_file_of(void *l, bool category, void *o, ZString *out)
{
    return at<DWORD(THISCALL *)(void *, void *, ZString *)>(category ? 0x46ada0 : 0x477890)(l, o, out);
}
DWORD list_entry_path(void *l, DWORD i, ZString *out) { return at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x46ae50)(l, i, out); }
void *list_create_category(void *l, const char *dir, const char *name, void **out)
{
    return at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x46ac30)(l, dir, name, out);
}
void *list_create_from_file(void *l, const char *dir, const char *name, void **out)
{
    return at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x46ad20)(l, dir, name, out);
}
DWORD list_dtor(void *l, bool category, DWORD flags) { return ptrv(at<void *(THISCALL *)(void *, DWORD)>(category ? 0x46a890 : 0x477700)(l, flags)); }
DWORD list_init(void *l, bool category) { return at<DWORD(THISCALL *)(void *)>(category ? 0x46a940 : 0x477220)(l); }
DWORD load_or_create(void *o, const char *dir, const char *name) { return at<DWORD(THISCALL *)(void *, const char *, const char *)>(0x46a630)(o, dir, name); }
DWORD category_file_of(void *o, ZString *out) { return at<DWORD(__fastcall *)(void *, ZString *)>(0x46a490)(o, out); }
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }

void zs_ctor(ZString *z) { original<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_dtor(ZString *z) { original<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
void st_zstring(const ZString &z) { st_text(z.buffer ? z.c_str() : nullptr); }

// ---- Fakes ---------------------------------------------------------------------------------------------

// Objects in the records: slot 1 records its deletion, slot 2 gives a class (by kind), slot 7 (the
// monitor) records its call
constexpr int NFAKE = 24;
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
};
FakeObj g_fakes[NFAKE];
int g_next_fake;
DWORD g_fake_vt[3][8];
CClassInfo *__cdecl fake_class_category() { return *at<CClassInfo **>(0x6bd280); }
CClassInfo *__cdecl fake_class_damage() { return *at<CClassInfo **>(0x6bd8ac); }
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
    for (int k = 0; k < 3; k++) {
        for (DWORD &s : g_fake_vt[k])
            s = 0;
        g_fake_vt[k][0] = ptrv((void *)&fake_delete0);
        g_fake_vt[k][1] = ptrv((void *)&fake_delete);
        g_fake_vt[k][7] = ptrv((void *)&fake_slot7);
    }
    g_fake_vt[0][2] = ptrv((void *)&fake_class_category);
    g_fake_vt[1][2] = ptrv((void *)&fake_class_damage);
    g_fake_vt[2][2] = ptrv((void *)&fake_class_object);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt[i % 3]), 0, (DWORD)i + 1};
    g_next_fake = 12;
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
// A value made comparable: fakes by number, known blocks by order and offset, else as it is
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
    t.check(same, "definitions", d);
    return same;
}

DWORD random_float()
{
    static constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3c23d70a, 0x41f00000, 0x7fc00000, 0xffc00000,
                                       0x7f800000, 0xff800000, 0x00000001, 0x80000001, 0x3fc00000, 0x4e6e6b28, 0x43b40000};
    return random() % 3 ? VALUES[random() % (sizeof VALUES / 4)] : random();
}

// ---- Objects -------------------------------------------------------------------------------------------

struct ClassDef {
    const char *name;
    DWORD create, create_array, size, array_mask;   // array_mask: bit i = byte i is written by the constructor
};
// CreateArray takes its block from operator new and InitFields sets no values, so in arrays only the
// vtable, the dynamic properties and what the constructors write themselves are defined
const ClassDef CLASSES[] = {{"CCategoryDefinition", 0x46a240, 0x46a280, 0x18, 0xf000ff},
                            {"CDamageTypeDefinition", 0x476ee0, 0x476f20, 0xc, 0x1ff},
                            {"CHueRemapHolder", 0x4d4510, 0x4d4540, 0x20, 0xff},
                            {"CQuipCategory", 0x55b290, 0x55b2e0, 0x20, 0xfff000ff}};

BYTE g_raw[2][0x20];
void st_object(const ClassDef &c, const BYTE *o, bool all_bytes)
{
    memcpy(g_raw[g_side], o, c.size);
    st(rel(ptrv(o)));
    for (DWORD i = 0; i < c.size; i++)
        if (all_bytes || (c.array_mask >> i & 1)) {
            const DWORD v = i % 4 == 0 && i + 4 <= c.size ? *reinterpret_cast<const DWORD *>(o + i) : 0;
            if (i % 4 == 0 && i >= 0x10 && c.size == 0x20 && c.create == 0x55b290 && (i == 0x10 || i == 0x1c)) {
                st(rel(v));   // the quip's condition and sound list: pointers
                i += 3;
                continue;
            }
            st(o[i]);
        }
}

DWORD objects_scenario(int k, DWORD &mask)
{
    const ClassDef &c = CLASSES[k];
    DWORD result = 0;
    if (random() % 2) {
        // Create on a block filled with a pattern
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        const BYTE fill = (BYTE)random();
        memset(p, fill, c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        know(o, c.size);
        ev('c', o == p);
        st_object(c, o, o == p);
        // Fields changed, the quip gets a sound list (a fake)
        for (DWORD i = 8; i + 4 <= c.size; i += 4)
            if (random() % 2)
                *reinterpret_cast<DWORD *>(o + i) = random_float();
        if (k == 3) {
            reinterpret_cast<CQuipCategory *>(o)->condition = nullptr;
            reinterpret_cast<CQuipCategory *>(o)->sounds = random() % 2 ? &g_fakes[random() % 12] : nullptr;
        }
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
            if (k == 3)
                reinterpret_cast<CQuipCategory *>(e + i * c.size)->sounds = random() % 2 ? &g_fakes[i] : nullptr;
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
const GetterDef GETTERS[] = {{0x46a370, 0x6bd280}, {0x46acc0, 0x6bd280}, {0x477000, 0x6bd8ac}, {0x4776b0, 0x6bd8ac},
                             {0x4d4620, 0x6c0528}, {0x4d44c0, 0x6c0528}, {0x55b400, 0x7124b8}};

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
    // colour bytes 7 and 0xb are padding; byte 0xa of CCategoryDefinition's default is uninitialised stack (0x53f010)
    case VT_COLOR: return same(0x28, 0x2f) && same(0x30, 0x32) && same(0x34, 0x3c);
    case VT_BYTE: return same(0x28, 0x2d);
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_OBJECT: return same(0x28, 0x3c);
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
const RegFields REGISTER_FIELDS[] = {{"CCategoryDefinition", 0x46a3d0, 0x6bd280}, {"CDamageTypeDefinition", 0x477060, 0x6bd8ac},
                                     {"CHueRemapHolder", 0x4d4680, 0x6c0528}, {"CQuipCategory", 0x55b460, 0x7124b8}};

struct Made {
    CFieldDesc *fields[8];
    DWORD n, result, static_added;
};
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    ZPtrArray *statics = at<ZPtrArray *>(0x62e6a8);
    const DWORD static_count = statics->count;
    g_nregister = 0;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    out.result = at<DWORD(__cdecl *)()>(rf.address)();
    unpatch(p);
    // The static list: what was added is taken off again
    out.static_added = statics->count - static_count;
    if (out.static_added == 1 && statics->data[static_count] == at<void *>(0x46a490)) {
        out.static_added = 0x100;
        statics->count = static_count;
    }
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
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x]", g.address, set[0], set[1],
                  results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "definitions", d);
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
            bool same = m[0].n == m[1].n && m[0].static_added == m[1].static_added;
            // eax: the static list's Add (the count), AddField's or SetHelp's (the descriptor's help string)
            if (rf.address == 0x55b460)
                same = same && m[0].result - ptrv(m[0].fields[m[0].n - 1]) == m[1].result - ptrv(m[1].fields[m[1].n - 1]);
            else if (rf.address == 0x46a3d0)
                same = same && m[0].result == m[1].result;
            for (DWORD i = 0; i < m[0].n && i < 8 && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[140];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, static %x / %x, eax %x / %x]", rf.name, var_zero, m[0].n,
                      m[1].n, m[0].static_added, m[1].static_added, m[0].result, m[1].result);
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

// ---- Definition lists ----------------------------------------------------------------------------------

alignas(4) BYTE g_list_mem[2][0x140];
DWORD g_vt_copy[2][48];

// Recorders for the slots of a list copy
void *THISCALL rec_slot37(ZDefinitionList *, const char *dir, const char *name, void **out)
{
    FakeObj *f = g_next_fake < NFAKE ? &g_fakes[g_next_fake++] : nullptr;
    if (g_trace)
        log("    slot 37 '%s' '%s' %x", dir ? dir : "(null)", name ? name : "(null)", ptrv(out));
    ev('C', text_hash(dir), text_hash(name), rel(ptrv(out)));
    return f;
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

// No empty name: slot 1 gives null for it, and the path functions of slots 30 and 39 read it (in the
// original too; the game's folders never yield one)
const char *const NAMES[] = {"Enemy", "Player", "Top Hide", "NonInteractiveSequence Actor", "Sub/Enemy Shot", "x y", "a/b/c",
                             "Missing One", "Player Friend", "!None", "Normal"};
const char *const FOLDERS[] = {"Categories", "Damage Types", "", "Some/Folder/", "x"};

// A fake list (category or damage type) with n records; vt: 0 the real vtable, 1 a copy with recorders
ZDefinitionList *make_list(bool category, int n, bool recorders)
{
    auto *l = reinterpret_cast<ZDefinitionList *>(g_list_mem[g_side]);
    memset(l, 0, sizeof *l);
    at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {RESOURCE_ALIASES_VTABLE, nullptr, 0, 0};
    l->registry_index = random();
    const DWORD vt = category ? CATEGORY_LIST_VTABLE : DAMAGE_LIST_VTABLE;
    if (recorders) {
        memcpy(g_vt_copy[g_side], at<void *>(vt), 41 * 4);
        if (random() % 2)
            g_vt_copy[g_side][1] = ptrv((void *)&rec_slot1);
        g_vt_copy[g_side][37] = ptrv((void *)&rec_slot37);
        if (random() % 2)
            g_vt_copy[g_side][39] = ptrv((void *)&rec_slot39);
        l->records.vftable = ptrv(g_vt_copy[g_side]);
        know(g_vt_copy[g_side], sizeof g_vt_copy[0]);
    } else {
        l->records.vftable = vt;
    }
    lstrcpyA(l->folder, FOLDERS[random() % 5]);
    l->list_name = category ? "Entity Categories" : "Damage Types";
    know(l, sizeof *l);
    if (n) {
        DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
        block[0] = (DWORD)n + 1;
        auto *e = reinterpret_cast<ZDefinitionRecord *>(block + 1);
        memset(e, 0, (n + 1) * 0x80);
        for (int i = 0; i < n; i++) {
            lstrcpyA(e[i].name, NAMES[random() % 11]);
            const DWORD r = random() % 8;
            e[i].object = r < 3 ? nullptr : &g_fakes[i];
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
            at<DWORD(THISCALL *)(void *, const char *, int)>(0x55d150)(&l->aliases, NAMES[random() % 11], (int)(random() % 5));
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
    const bool category = random() % 2;
    const int n = (int)(random() % 9);
    const bool recorders = random() % 4 != 0;
    ZDefinitionList *l = make_list(category, n, recorders);
    st_list(l);
    const int ops = 3 + (int)(random() % 14);
    for (int k = 0; k < ops; k++) {
        ZString out;
        zs_ctor(&out);
        const DWORD op = random() % 8;
        if (g_trace)
            log("    side %d op %u", g_side, op);
        switch (op) {
        case 0: ev('N', rel(list_name(l))); break;
        case 1: ev('6', list_slot36(l, random(), random())); break;
        case 2: {
            if (!recorders)
                break;   // the real slot 37 would read (or write) files of the game data
            const DWORD i = random() % 3 ? random() % (l->num_entries + 2) : random();
            if (g_trace) {
                const DWORD *vt = *reinterpret_cast<const DWORD *const *>(l);
                log("    load %u of %u entries %x vt %x s1 %x s37 %x s5 %x folder '%s'", i, l->num_entries, ptrv(l->entries), ptrv(vt), vt[1], vt[37], vt[5], l->folder);
                if (l->entries && i < l->num_entries)
                    log("    entry '%s' obj %x loading %u", l->entries[i].name, ptrv(l->entries[i].object), l->entries[i].loading);
            }
            ev('L', i, rel(list_load(l, i)));
            break;
        }
        case 3:
        case 4: {
            // An object of the records, a stranger or a null record's object
            const DWORD m = l->entries ? l->num_entries : 0;
            void *o = m && random() % 4 ? l->entries[random() % m].object : &g_fakes[12 + random() % 6];
            if (!o)
                o = &g_fakes[random() % 3];
            const DWORD r = list_file_of(l, category, o, &out);
            // after slot 39 the upper bytes are left over from a temporary string, a pool address
            // that differs between the sides (Assimilate Cave1): only that it is one counts
            DWORD upper = rel(r & ~0xffu);
            if (upper == (r & ~0xffu) && upper >= 0x10000)
                upper = 0xb5000000u;
            ev('F', r & 0xff, upper >> 8);
            st_zstring(out);
            break;
        }
        case 5: {
            if (!l->entries || !l->num_entries)
                break;
            const DWORD i = random() % l->num_entries;
            ev('P', i, list_entry_path(l, i, &out) != 0);
            st_zstring(out);
            break;
        }
        case 6:
            if (random() % 4 == 0) {
                ev('7', list_slot17(l) != 0);
                st_list(l);
                ev('n', l->entries == nullptr);
            }
            break;
        case 7: st_list(l); break;
        }
        zs_dtor(&out);
    }
    st_list(l);
    const DWORD r = rel(list_dtor(l, category, 0));
    st_list(l);
    mask = 0xffffffff;
    return r;
}

// The static initializers and exit destructors on the globals (saved and restored). The exit destructor
// runs once on the emptied global and once on fakes put into it.
DWORD statics_scenario(bool category, DWORD &mask)
{
    const DWORD addr = category ? CATEGORY_LIST : DAMAGE_TYPE_LIST;
    SavedBytes saved;
    save_bytes(saved, addr, 0x140);
    auto *l = at<ZDefinitionList *>(addr);
    memset(l, (int)(random() & 0xff), 0x140);
    g_atexit_calls = 0;
    g_atexit_function = 0;
    CodePatch p;
    patch_jump(p, 0x5d0863, (void *)&fake_atexit);
    const DWORD r = at<DWORD(__cdecl *)()>(category ? 0x46a760 : 0x4770f0)();
    unpatch(p);
    ev('A', g_atexit_calls, g_atexit_function, r);
    st_list(l);
    const DWORD exit_fn = category ? 0x46a7d0 : 0x477160;
    if (random() % 2) {
        // Fakes: records with fake objects, aliases, a monitor
        const int n = 1 + (int)(random() % 6);
        DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
        block[0] = (DWORD)n + 1;
        auto *e = reinterpret_cast<ZDefinitionRecord *>(block + 1);
        memset(e, 0, (n + 1) * 0x80);
        for (int i = 0; i < n; i++) {
            lstrcpyA(e[i].name, NAMES[random() % 11]);
            e[i].object = random() % 3 ? &g_fakes[i] : nullptr;
        }
        l->entries = e;
        l->num_entries = (DWORD)n;
        at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, 1, 0x80, (DWORD)n, e, 0);
        at<DWORD(THISCALL *)(void *, const char *, int)>(0x55d150)(&l->aliases, "Electrical", 1);
        if (random() % 2)
            l->monitor = &g_fakes[NFAKE - 2];
    }
    ev('X', at<DWORD(__cdecl *)()>(exit_fn)() != 0);
    st_list(l);
    restore_bytes(saved);
    mask = 0xffffffff;
    return 0;
}

// ---- Files in the temp folder --------------------------------------------------------------------------

char g_temp[MAX_PATH];       // absolute, with a final backslash
char g_temp_rel[MAX_PATH];   // the same, relative to the data folder ("" when not reachable)

void write_file(const char *name, const char *text)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s.txt", g_temp, name);
    HANDLE f = CreateFileA(path, 0x40000000, 0, nullptr, 2, 0x80, nullptr);
    if (f == (HANDLE)(intptr_t)-1)
        return;
    DWORD w;
    WriteFile(f, text, (DWORD)lstrlenA(text), &w, nullptr);
    CloseHandle(f);
}
// Hash and length of the file (0xffffffff: missing), then deleted
DWORD read_and_delete(const char *name, const char *ext = ".txt")
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s%s", g_temp, name, ext);
    HANDLE f = CreateFileA(path, 0x80000000, 1, nullptr, 3, 0x80, nullptr);
    if (f == (HANDLE)(intptr_t)-1)
        return 0xffffffff;
    static BYTE buf[4096];
    DWORD n = 0;
    ReadFile(f, buf, sizeof buf, &n, nullptr);
    CloseHandle(f);
    DeleteFileA(path);
    st(n);
    st_bytes(buf, n);
    return n;
}

const char *const CATEGORY_FILES[] = {
    "CCategoryDefinition\r\n{\r\n\tMiniMap Color=200,10,20\r\n}\r\n",
    "CCategoryDefinition\r\n{\r\n}\r\n",
    "CCategoryDefinition\r\n{\r\n\tMiniMap Color=1,2,3,4\r\n\tUnknown Field=7\r\n}\r\n",
    "",
    nullptr,   // missing: written
};
const char *const OBJECT_FILES[] = {
    "CDamageTypeDefinition\r\n{\r\n}\r\n",
    "CDamageTypeDefinition\r\n{\r\n\tUnavoidable damage=1\r\n}\r\n",
    "CDamageTypeDefinition\r\n{\r\n\tUnavoidable damage=0\r\n}\r\n",
    "CCategoryDefinition\r\n{\r\n\tMiniMap Color=200,0,0\r\n}\r\n",
    "CHueRemapHolder\r\n{\r\n\tSource Hue 1=10\r\n\tError 2=50\r\n}\r\n",
    "CQuipCategory\r\n{\r\n\tDelay Before Saying=.5\r\n\tMinimum Time Between Saying=30\r\n}\r\n",
};

void st_definition(const void *o, bool address = true)
{
    st(address ? rel(ptrv(o)) : o != nullptr);
    if (!o)
        return;
    const DWORD vt = *static_cast<const DWORD *>(o);
    st(vt);
    const DWORD n = vt == 0x5ef684 ? 0x18 : vt == 0x5f0980 ? 9 : vt == 0x5f7c10 || vt == 0x5fe7f0 ? 0x20 : 8;
    for (DWORD i = 4; i < n; i++)
        if (!(vt == 0x5ef684 && (i == 0xf || i == 0x13)))
            st(static_cast<const BYTE *>(o)[i]);
}

DWORD file_scenario(unsigned seed, DWORD &mask)
{
    const DWORD last_hash = text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str());
    char name[32];
    wsprintfA(name, "Def %u", seed);
    DWORD r = 0;
    switch (seed % 3) {
    case 0: {
        // LoadOrCreate into a new category definition
        const char *text = CATEGORY_FILES[random() % 5];
        if (text)
            write_file(name, text);
        auto *o = static_cast<BYTE *>(at<Create0>(0x46a240)());
        know(o, 0x18);
        r = load_or_create(o, g_temp_rel, name) & 0xff;
        st_definition(o);
        ev('f', read_and_delete(name));
        destroy(o, 1);
        break;
    }
    case 1: {
        // Slot 37 of the category list (stored or not)
        const char *text = CATEGORY_FILES[random() % 5];
        if (text)
            write_file(name, text);
        ZDefinitionList *l = make_list(true, 0, false);
        void *slot = nullptr;
        void *o = list_create_category(l, g_temp_rel, name, random() % 2 ? &slot : nullptr);
        know(o, 0x18);
        r = rel(ptrv(o));
        ev('s', rel(ptrv(slot)));
        st_definition(o);
        ev('f', read_and_delete(name));
        destroy(o, 1);
        list_dtor(l, true, 0);
        break;
    }
    case 2: {
        // Slot 37 of the other lists: the object of the class the file names, written back
        write_file(name, OBJECT_FILES[random() % 6]);
        ZDefinitionList *l = make_list(false, 0, false);
        void *slot = nullptr;
        void *o = list_create_from_file(l, g_temp_rel, name, random() % 2 ? &slot : nullptr);
        know(o, 0x20);
        r = rel(ptrv(o));
        ev('s', rel(ptrv(slot)));
        st_definition(o);
        ev('f', read_and_delete(name, ".TXT"));
        if (o)
            destroy(o, 1);
        list_dtor(l, false, 0);
        break;
    }
    }
    ev('g', text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str()) == last_hash);
    mask = 0xffffffff;
    return r;
}

// Slot 30 with the real slot 37: the entries' files in the temp folder
DWORD load_scenario(unsigned seed, DWORD &mask)
{
    const bool category = seed % 2;
    const int n = 1 + (int)(random() % 4);
    ZDefinitionList *l = make_list(category, n, false);
    lstrcpyA(l->folder, g_temp_rel);
    char names[4][32];
    for (int i = 0; i < n; i++) {
        wsprintfA(names[i], "Entry %u %d", seed, i);
        lstrcpyA(l->entries[i].name, names[i]);
        l->entries[i].object = nullptr;
        l->entries[i].loading = 0;
        if (category ? random() % 3 != 0 : true)
            write_file(names[i], category ? CATEGORY_FILES[random() % 4] : OBJECT_FILES[random() % 3]);
    }
    for (int k = 0; k < n + 1; k++) {
        const DWORD i = random() % (n + 1);
        const DWORD v = list_load(l, i);
        // the objects of slot 37 by the entry that holds them, not by address: a block of a guessed
        // size, or one the list freed and the pool gave out again, named a later object differently
        // on the two sides
        DWORD named = rel(v);
        for (int j = 0; j < n; j++)
            if (v && v == ptrv(l->entries[j].object))
                named = 0xe0000000u + (DWORD)j;
        ev('L', i, named);
    }
    for (int i = 0; i < n; i++) {
        st_definition(l->entries[i].object, false);
        ev('f', read_and_delete(names[i], category ? ".txt" : ".TXT"));
        if (l->entries[i].object) {
            destroy(l->entries[i].object, 1);
            l->entries[i].object = nullptr;
        }
    }
    st_list(l);
    list_dtor(l, category, 0);
    mask = 0xffffffff;
    return 0;
}

// ---- Hue remap setters and the quip category's timers --------------------------------------------------

DWORD setters_scenario(DWORD &mask)
{
    static constexpr DWORD SETTERS[] = {0x4d49f0, 0x4d4a00, 0x4d4a10, 0x4d4a20, 0x4d4a30, 0x4d4a40};
    alignas(4) BYTE mem[0x20];
    for (BYTE &b : mem)
        b = (BYTE)random();
    DWORD r = 0;
    for (int k = 0; k < 6; k++) {
        const DWORD v = random();
        r = fnv(r, &v, 4) ^ at<DWORD(THISCALL *)(void *, DWORD)>(SETTERS[random() % 6])(mem, v);
    }
    st_bytes(mem, sizeof mem);
    mask = 0xffffffff;
    return r;
}

DWORD quip_timer_scenario(DWORD &mask)
{
    CQuipCategory q;
    memset(&q, 0, sizeof q);
    DWORD *d = reinterpret_cast<DWORD *>(&q);
    for (int i = 2; i <= 6; i++)
        d[i] = i == 4 ? random() : random_float();
    DWORD r = 0;
    const int ops = 1 + (int)(random() % 4);
    for (int k = 0; k < ops; k++) {
        if (random() % 2) {
            at<DWORD(THISCALL *)(void *)>(0x55b730)(&q);   // eax undefined
            ev('R');
        } else {
            r = at<DWORD(THISCALL *)(void *)>(0x55b770)(&q) & 0xff;
            ev('T', r);
        }
        st_bytes(&q, sizeof q);
        if (random() % 2)
            d[5] = random_float();
        if (random() % 3 == 0)
            d[6] = random_float();
    }
    mask = 0xff;
    return r;
}

// The sounds of a quip category from game folders (read only; the game's random generator saved and
// restored, since mode 1 shuffles)
SavedBytes g_saved_random;
DWORD sounds_scenario(const char *folder, DWORD &mask)
{
    restore_bytes(g_saved_random);
    auto *q = static_cast<CQuipCategory *>(at<Create0>(0x55b290)());
    know(q, 0x20);
    if (random() % 2)
        q->sounds = &g_fakes[3];
    const int reads = 1 + (int)(random() % 2);
    DWORD r = 0;
    for (int k = 0; k < reads; k++) {
        r = at<DWORD(THISCALL *)(void *, const char *)>(0x55b7d0)(q, folder);
        auto *l = static_cast<ZDirList *>(q->sounds);
        know(l, 0x12c);
        const DWORD n = l ? vcall<DWORD>(l, 22) : 0;
        st(n);
        for (DWORD i = 0; i < n && i < 200; i++)
            st_text(vcall<const char *>(l, 1, i));
        if (l)
            for (DWORD i = 0; i < n && i < 8; i++)
                st_text(at<const char *(THISCALL *)(ZDirList *, const char *)>(0x4a31e0)(l, nullptr));
    }
    ev('r', r != 0);
    st_bytes(at<const BYTE *>(GAME_RANDOM), GAME_RANDOM_BYTES);
    destroy(q, 1);
    mask = 0;   // the last ZString destructor's eax
    return 0;
}

// ---- Real data -----------------------------------------------------------------------------------------

// Init of a fresh list on the game's folders; the registry entry it adds is taken off again
DWORD g_count_calls;
DWORD __fastcall fake_files_count(ZFileSpec *spec, DWORD flags)
{
    ev('c', text_hash(spec->folder), text_hash(spec->ext[0].buffer ? spec->ext[0].c_str() : nullptr), flags);
    return g_count_calls++ ? 3 : 0;
}

DWORD init_scenario(bool category, bool fallback, DWORD &mask)
{
    auto *l = reinterpret_cast<ZDefinitionList *>(g_list_mem[g_side]);
    memset(l, 0, sizeof *l);
    at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {RESOURCE_ALIASES_VTABLE, nullptr, 0, 0};
    l->registry_index = 0xffffffff;
    l->records.vftable = category ? CATEGORY_LIST_VTABLE : DAMAGE_LIST_VTABLE;
    know(l, sizeof *l);
    // The registry of the resource lists (0x6be088): +8 entries of 16 bytes {name, list, value, flag}, +0xc count
    DWORD &count = *at<DWORD *>(0x6be088 + 0xc);
    DWORD *const *entries = at<DWORD **>(0x6be088 + 8);
    const DWORD before = count;
    CodePatch p;
    g_count_calls = 0;
    if (fallback)
        patch_jump(p, 0x4a12d0, (void *)&fake_files_count);
    const DWORD r = list_init(l, category);
    if (fallback)
        unpatch(p);
    know(l->entries ? reinterpret_cast<DWORD *>(l->entries) - 1 : nullptr, 0x4000);
    know(l->monitor, 0x34);
    ev('r', count - before, l->registry_index - before, count ? ((*entries)[(count - 1) * 4 + 1] == ptrv(l)) : 2);
    if (count == before + 1 && (*entries)[before * 4 + 1] == ptrv(l))
        count = before;
    st_list(l);
    if (auto *m = static_cast<const DWORD *>(l->monitor)) {
        st(m[0]);
        st(rel(m[1]));
        st(m[2]);
    }
    // Real data: the same names as the global list (built by the original at startup)
    const auto *g = at<ZDefinitionList *>(category ? CATEGORY_LIST : DAMAGE_TYPE_LIST);
    bool same = !fallback && g->num_entries == l->num_entries;
    for (DWORD i = 0; same && i < l->num_entries; i++)
        same = lstrcmpiA(g->entries[i].name, l->entries[i].name) == 0;
    ev('G', same || fallback);
    const DWORD d = rel(list_dtor(l, category, 0));
    st_list(l);
    ev('d', d);
    mask = 0;   // the ZString destructor's eax
    return r;
}

// The global lists and their loaded objects
void test_globals(Tally &t, int &loaded)
{
    for (int c = 0; c < 2; c++) {
        auto *g = at<ZDefinitionList *>(c ? DAMAGE_TYPE_LIST : CATEGORY_LIST);
        for (DWORD i = 0; i < g->num_entries; i++) {
            void *o = g->entries[i].object;
            if (o)
                loaded++;
            two_sides(t, c ? "global damage types" : "global categories", i, [&](DWORD &mask) {
                ZString out;
                zs_ctor(&out);
                ev('P', list_entry_path(g, i, &out) != 0);
                st_zstring(out);
                void *probe = o ? o : &g_fakes[c];
                const DWORD f = list_file_of(g, !c, probe, &out);
                ev('F', f & 0xff);
                st_zstring(out);
                if (!c) {
                    const DWORD k = category_file_of(probe, &out);
                    ev('K', k & 0xff);
                    st_zstring(out);
                }
                if (o)
                    ev('L', list_load(g, i) == ptrv(g->entries));
                zs_dtor(&out);
                ev('N', list_name(g) == ptrv(g->list_name));
                st_text(g->list_name);
                st_definition(o);
                mask = 0xffffffff;
                return 0u;
            });
        }
        // A stranger, and the callback with the global list emptied for a moment
        two_sides(t, "global lists: strangers", (unsigned)c, [&](DWORD &mask) {
            ZString out;
            zs_ctor(&out);
            for (int k = 0; k < 6; k++) {
                const DWORD f = list_file_of(g, !c, &g_fakes[k], &out);
                ev('F', f & 0xff, rel(f & ~0xffu));
                const DWORD q = category_file_of(&g_fakes[k], &out);
                ev('K', q & 0xff, rel(q & ~0xffu));
            }
            auto *cat = at<ZDefinitionList *>(CATEGORY_LIST);
            const DWORD saved = cat->num_entries;
            cat->num_entries = 0;
            const DWORD q = cat->entries ? category_file_of(cat->entries[0].object ? cat->entries[0].object : &g_fakes[0], &out) : 0;
            cat->num_entries = saved;
            ev('E', q & 0xff);
            st_zstring(out);
            zs_dtor(&out);
            mask = 0xffffffff;
            return 0u;
        });
    }
}

// The real category files read into new objects (they exist, so nothing is written); compared with
// the objects the game loaded
void test_real_files(Tally &t)
{
    auto *g = at<ZDefinitionList *>(CATEGORY_LIST);
    for (DWORD i = 0; i < g->num_entries; i++) {
        const char *name = g->entries[i].name;
        const void *loaded = g->entries[i].object;
        if (!loaded)
            continue;   // a file that might be missing would be written
        two_sides(t, "real category files", i, [&](DWORD &mask) {
            auto *o = static_cast<BYTE *>(at<Create0>(0x46a240)());
            know(o, 0x18);
            const DWORD r = load_or_create(o, g->folder, name) & 0xff;
            st_definition(o);
            ev('=', memcmp(o + 8, static_cast<const BYTE *>(loaded) + 8, 7) == 0);
            destroy(o, 1);
            mask = 0xffffffff;
            return r;
        });
    }
}

// The relative path from the data folder to the temp folder (same drive only)
bool make_temp_paths()
{
    char tmp[MAX_PATH], data[MAX_PATH];
    g_temp[0] = g_temp_rel[0] = 0;
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_temp, "%sZaxDefinitions\\", tmp);
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

}  // namespace

ZAX_CHECKS(definitions)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"definitions: Create, CreateArray, deleting destructors of the four classes"},
        classes{"definitions: class getters and RegisterFields"},
        registered{"definitions: RegisterFields against the registered fields (real data)"},
        lists{"definitions: definition list slots, destructors, static initializers"},
        files{"definitions: LoadOrCreate, slot 37 and slot 30 with files"},
        small{"definitions: hue remap setters, quip timers"}, sounds{"definitions: quip sounds from game folders (real data)"},
        real{"definitions: Init, global lists, category files (real data)"};
    for (int c = 0; c < 4; c++) {
        log("  definitions: objects of %s", CLASSES[c].name);
        for (unsigned seed = 1; seed <= (unsigned)scaled(80); seed++)
            if (!two_sides(objects, CLASSES[c].name, seed * 5 + (unsigned)c, [c](DWORD &mask) { return objects_scenario(c, mask); }) &&
                objects.mismatches <= 3)
                for (int side = 0; side < 2; side++) {
                    const DWORD *w = reinterpret_cast<const DWORD *>(g_raw[side]);
                    log("    last object side %d: %x %x %x %x %x %x %x %x", side, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
                }
    }
    objects.report();
    log("  definitions: class getters and RegisterFields");
    test_classes(classes, registered);
    classes.report();
    registered.report();
    log("  definitions: list scenarios");
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++) {
        if (g_trace)
            log("  list seed %u", seed);
        two_sides(lists, "list", seed, [](DWORD &mask) { return list_scenario(mask); });
    }
    for (unsigned seed = 1; seed <= (unsigned)scaled(20); seed++)
        two_sides(lists, "statics", seed, [seed](DWORD &mask) { return statics_scenario(seed % 2 != 0, mask); });
    lists.report();
    log("  definitions: setters and timers");
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++) {
        two_sides(small, "setters", seed, [](DWORD &mask) { return setters_scenario(mask); });
        two_sides(small, "quip timers", seed, [](DWORD &mask) { return quip_timer_scenario(mask); });
    }
    small.report();
    if (make_temp_paths()) {
        log("  definitions: files in %s (from the data folder: %s)", g_temp, g_temp_rel);
        for (unsigned seed = 1; seed <= (unsigned)scaled(90); seed++)
            two_sides(files, "file", seed, [seed](DWORD &mask) { return file_scenario(seed, mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(30); seed++)
            two_sides(files, "slot 30", seed, [seed](DWORD &mask) { return load_scenario(seed, mask); });
        if (!RemoveDirectoryA(g_temp))
            log("  could not remove %s", g_temp);
    } else {
        log("  definitions: no temp folder reachable from the data folder, file cases skipped");
    }
    files.report();
    log("  definitions: quip sounds");
    save_bytes(g_saved_random, GAME_RANDOM, GAME_RANDOM_BYTES);
    static const char *const SOUND_FOLDERS[] = {"Sounds", "Sounds/AMBIENT", "Sounds/PLAYER", "Does Not Exist", "Music/Jungle", ""};
    for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
        two_sides(sounds, "sounds", seed, [seed](DWORD &mask) { return sounds_scenario(SOUND_FOLDERS[seed % 6], mask); });
    restore_bytes(g_saved_random);
    sounds.report();
    log("  definitions: real data");
    for (int c = 0; c < 2; c++)
        for (int fallback = 0; fallback < 2; fallback++)
            two_sides(real, "init", (unsigned)(c * 2 + fallback), [c, fallback](DWORD &mask) { return init_scenario(c == 0, fallback != 0, mask); });
    int loaded = 0;
    test_globals(real, loaded);
    test_real_files(real);
    const auto *cat = at<ZDefinitionList *>(CATEGORY_LIST), *dam = at<ZDefinitionList *>(DAMAGE_TYPE_LIST);
    log("  real data: %u categories, %u damage types (%d loaded), %u ms", cat->num_entries, dam->num_entries, loaded,
        GetTickCount() - start);
    real.report();
}

}  // namespace zax
