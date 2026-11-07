// Checks of CLevelOfDifficulty, CPlasmaTileMap's object functions and the texture list
// (world_data.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out, side 1
// through the reimplementation. Compared: eax, an event log of the fakes and the state afterwards.
//  - Objects: CLevelOfDifficulty Create (on a pool block filled with a pattern), CreateArray 0-3, the
//    deleting destructors with texts set; CPlasmaTileMap Create/CreateArray/deleting destructor (the
//    constructor and destructor are the original's on both sides: the object's vtable and texture list
//    class compared; "Blending" is not set by the constructor).
//  - Class getters and RegisterFields (ClassInfo_Register replaced by a stand-in; descriptors member by
//    member, and against the descriptors registered at startup).
//  - Slot 18 of the difficulty: names at the default or not, the object in the difficulty list
//    0x6c2a00 (an entry's object swapped in for the case) or not.
//  - Texture lists built by the exe's constructor 0x4b40f0 with records of fake objects (slot 1 records
//    the deletion, slot 2 gives CFrame or another class), a fake monitor: slots 40, 35, 39 and both
//    deleting destructors (0x53f590, 0x4b46f0).
//  - Real data: the texture list of the running level's CPlasmaTileMap (found through the layer): slot
//    39 for every entry and slot 35 with every loaded texture and with a non-texture; the difficulty
//    list's objects through slot 18 (only those whose name is set, so nothing changes).
#include "check_util.h"
#include "world_data.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);

namespace zax {

int hooks_world_data(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

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

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 32;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_world_data(nullptr);
    if (!hook_sites_fit("world_data", n, MAX_SITES))
        return;
    hooks_world_data(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;
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

constexpr int NKNOWN = 32;
struct Known {
    DWORD at_, size;
};
Known g_known[2][NKNOWN];
int g_nknown[2];
void know(const void *p, DWORD size)
{
    if (p && g_nknown[g_side] < NKNOWN)
        g_known[g_side][g_nknown[g_side]++] = {ptrv(p), size};
}

// Fake objects: slot 1 records its deletion, slot 2 gives a class, slot 7 (the monitor) records
constexpr int NFAKE = 24;
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
    DWORD frame;   // slot 2: CFrame or CObject
};
FakeObj g_fakes[NFAKE];
DWORD g_fake_vt[8];
FakeObj *THISCALL fake_delete(FakeObj *f, DWORD flags)
{
    ev('D', f->id, flags);
    return f;
}
CClassInfo *THISCALL fake_class(FakeObj *f)
{
    return f->frame ? at<CClassInfo *(__cdecl *)()>(0x4a8760)() : at<CClassInfo *(__cdecl *)()>(0x4163b0)();
}
DWORD THISCALL fake_slot7(FakeObj *f, DWORD flags)
{
    ev('7', f->id, flags);
    return 0x700 + f->id;
}
void init_fakes()
{
    for (DWORD &s : g_fake_vt)
        s = 0;
    g_fake_vt[1] = ptrv((void *)&fake_delete);
    g_fake_vt[2] = ptrv((void *)&fake_class);
    g_fake_vt[7] = ptrv((void *)&fake_slot7);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt), 0, (DWORD)i + 1, (DWORD)(i % 3 != 0)};
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
    t.check(same, "world data", d);
    return same;
}

bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}

void zs_ctor_text(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
void zs_ctor(ZString *z) { at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_dtor(ZString *z) { at<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
void zs_assign_text(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
const char *text_of(const ZString &z) { return z.buffer ? z.c_str() : nullptr; }

// ---- Objects -------------------------------------------------------------------------------------------

void st_difficulty(const BYTE *o, bool all)
{
    const auto *l = reinterpret_cast<const LevelOfDifficulty *>(o);
    st(l->vftable);
    st(rel(ptrv(l->dynamic)));
    st_text(text_of(l->name));
    st_text(text_of(l->text));
    if (all)
        for (float f : l->percent)
            st(bits(f));
}

constexpr const char *NAMES[] = {"Easy", "Normal", "Hard", "This Value Has Not Yet Been Set", "", "LOD_IMPOSSIBLE"};

DWORD difficulty_case(DWORD &mask)
{
    using Destroy = void *(THISCALL *)(void *, DWORD);
    DWORD r;
    mask = 0xffffffff;
    if (random() % 2) {
        BYTE *p = static_cast<BYTE *>(pool_alloc(0x28));
        memset(p, (int)(random() & 0xff), 0x28);
        pool_free(p, 0x28);
        BYTE *o = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x4fd760)());
        know(o, 0x28);
        ev('c', o == p);
        st_difficulty(o, true);
        zs_assign_text(&reinterpret_cast<LevelOfDifficulty *>(o)->name, NAMES[random() % 6]);
        const DWORD flags = random() % 2;
        r = rel(ptrv(at<Destroy>(0x4fd810)(o, flags)));
        if (!flags)
            pool_free(o, 0x28);
    } else {
        const int n = (int)(random() % 4);
        BYTE *e = static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(0x4fd7a0)(n, 0, 0));
        know(e ? e - 4 : nullptr, n * 0x28 + 8);   // +8: e itself for an empty array
        st(rel(ptrv(e)));
        if (!e)
            return 0;
        for (int i = 0; i < n; i++) {
            st_difficulty(e + i * 0x28, false);   // the percentages keep the heap's bytes
            zs_assign_text(&reinterpret_cast<LevelOfDifficulty *>(e + i * 0x28)->text, NAMES[random() % 6]);
        }
        const DWORD flags = 2 | random() % 2;
        r = rel(ptrv(at<Destroy>(0x4fd810)(e, flags)));
        if (!(flags & 1))
            heap_free(e - 4);
    }
    return r;
}

DWORD plasma_case(DWORD &mask)
{
    using Destroy = void *(THISCALL *)(void *, DWORD);
    mask = 0xffffffff;
    DWORD r;
    // "Blending" (+0x1068) is not set by the constructor (its default comes with slot 19): the block's
    // old bytes, so not compared
    auto st_plasma = [](const BYTE *o) {
        st(*reinterpret_cast<const DWORD *>(o));
        const BYTE *list = *reinterpret_cast<BYTE *const *>(o + 0x1064);
        st(list ? *reinterpret_cast<const DWORD *>(list) : 0);
    };
    if (random() % 2) {
        BYTE *o = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x53f290)());
        know(o, 0x2d48);
        st_plasma(o);
        const DWORD flags = random() % 2;
        r = rel(ptrv(at<Destroy>(0x53f310)(o, flags)));
        if (!flags)
            pool_free(o, 0x2d48);
    } else {
        const int n = (int)(random() % 3);
        BYTE *e = static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(0x53f2b0)(n, 0, 0));
        know(e ? e - 4 : nullptr, n * 0x2d48 + 8);   // +8: e itself for an empty array
        st(rel(ptrv(e)));
        if (!e)
            return 0;
        st(*reinterpret_cast<DWORD *>(e - 4));
        for (int i = 0; i < n; i++)
            st_plasma(e + i * 0x2d48);
        const DWORD flags = 2 | random() % 2;
        r = rel(ptrv(at<Destroy>(0x53f310)(e, flags)));
        if (!(flags & 1))
            heap_free(e - 4);
    }
    return r;
}

// ---- Slot 18 of the difficulty --------------------------------------------------------------------------

DWORD after_read_case(DWORD &mask)
{
    LevelOfDifficulty *o = static_cast<LevelOfDifficulty *>(at<void *(__cdecl *)()>(0x4fd760)());
    know(o, 0x28);
    zs_assign_text(&o->name, NAMES[random() % 6]);
    auto *list = at<ZDefinitionList *>(DIFFICULTY_LIST);
    DWORD k = 0xffffffff;
    void *saved = nullptr;
    if (list->num_entries && list->entries && random() % 2) {
        k = random() % list->num_entries;
        saved = list->entries[k].object;
        list->entries[k].object = o;
    }
    const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x4fdf70)(o, random());
    if (k != 0xffffffff)
        list->entries[k].object = saved;
    st_text(text_of(o->name));
    st_text(text_of(o->text));
    vcall<void *>(o, 1, 1);
    mask = 0;   // the comparison's result or slot 19's
    return r;
}

// ---- Texture lists ------------------------------------------------------------------------------------

TextureList *make_list(int n)
{
    auto *l = static_cast<TextureList *>(pool_alloc(0x160));
    at<void *(THISCALL *)(void *, DWORD)>(0x4b40f0)(l, random() % 2 ? ptrv(&g_fakes[0]) : 0);
    l->records.vftable = VT_PLASMA_TEXTURE_LIST;
    lstrcpyA(l->folder, random() % 2 ? "Cache/Textures" : "");
    know(l, 0x160);
    if (n) {
        DWORD *block = static_cast<DWORD *>(heap_alloc(n * 0x80 + 4));
        block[0] = (DWORD)n;
        auto *rec = reinterpret_cast<ZDefinitionRecord *>(block + 1);
        memset(rec, 0, n * 0x80);
        constexpr const char *TEX[] = {"Grass", "jungle dirt 2", "Water/waterFractal", "Special/Under Construction", "Rock"};
        for (int i = 0; i < n; i++) {
            lstrcpyA(rec[i].name, TEX[(i + random()) % 5]);
            rec[i].object = random() % 4 ? &g_fakes[1 + i] : nullptr;
            rec[i].loading = random() % 8 == 0;
        }
        l->entries = rec;
        l->num_entries = (DWORD)n;
        know(block, n * 0x80 + 4);
        at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, 0, 0x80, n, rec, 0);
    }
    l->monitor = random() % 3 ? &g_fakes[20] : nullptr;
    return l;
}
void st_list(const TextureList *l)
{
    st(l->records.vftable);
    st(rel(ptrv(l->entries)));
    st(l->num_entries);
    st(l->records.num);
    st(rel(ptrv(l->records.data)));
    st(rel(ptrv(l->monitor)));
    for (DWORD i = 0; l->entries && i < l->num_entries; i++)
        st(rel(ptrv(l->entries[i].object)));
}

DWORD list_case(DWORD &mask)
{
    const int n = (int)(random() % 6);
    TextureList *l = make_list(n);
    DWORD r = 0;
    mask = 0xffffffff;
    switch (random() % 6) {
    case 0:
        r = at<DWORD(THISCALL *)(void *)>(0x4b45e0)(l);
        st_list(l);
        break;
    case 1: {
        ZString out;
        zs_ctor(&out);
        void *obj = random() % 4 ? &g_fakes[random() % 8] : &g_fakes[22];
        r = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x4b4820)(l, obj, &out) & 0xff;
        st_text(text_of(out));
        zs_dtor(&out);
        mask = 0xff;
        break;
    }
    case 2:
        if (n) {
            ZString out;
            zs_ctor_text(&out, "old");
            r = at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x4b4880)(l, random() % (DWORD)n, &out);
            st_text(text_of(out));
            zs_dtor(&out);
            mask = 0;   // the ZString destructor's
        }
        break;
    case 3: {
        const DWORD flags = random() % 2;
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x53f590)(l, flags)));
        st(l->records.vftable);
        if (!flags)
            pool_free(l, 0x160);
        return r;
    }
    case 4: {
        // The base list's destructor (0x140 bytes): not freeing, the 0x160 block goes back here
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4b46f0)(l, 0)));
        st(l->records.vftable);
        pool_free(l, 0x160);
        return r;
    }
    default:
        // Slot 40 twice
        at<DWORD(THISCALL *)(void *)>(0x4b45e0)(l);
        r = at<DWORD(THISCALL *)(void *)>(0x4b45e0)(l);
        st_list(l);
        break;
    }
    // Free without deleting the fakes: the records cleared first (slot 40 of the original), then the
    // list's own destructor
    original<DWORD(THISCALL *)(void *)>(0x4b45e0)(l);
    original<void *(THISCALL *)(void *, DWORD)>(0x53f590)(l, 1);
    return r;
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

bool same_text_buffer(DWORD a, DWORD b)
{
    if (!readable(reinterpret_cast<void *>(a), 12) || !readable(reinterpret_cast<void *>(b), 12))
        return false;
    const char *x = reinterpret_cast<const char *>(a + 8), *y = reinterpret_cast<const char *>(b + 8);
    for (int i = 0; i < 400; i++) {
        if (x[i] != y[i])
            return false;
        if (!x[i])
            return true;
    }
    return false;
}
// Two descriptors compared on the bytes their constructors write: the base part (+0x24 is not written),
// then per kind; raw equal dwords are equal, otherwise ZString buffers by text, the activity array's
// element descriptor by its own comparison. Padding bytes and the heap behind a byte member are skipped.
DWORD g_fake_class_at, g_real_class_at;   // a class pointer of a fake class info that stands for the real one
bool same_dword(const BYTE *a, const BYTE *b, DWORD off)
{
    const DWORD x = *reinterpret_cast<const DWORD *>(a + off), y = *reinterpret_cast<const DWORD *>(b + off);
    return x == y || same_text_buffer(x, y) || (g_fake_class_at && x == g_real_class_at && y == g_fake_class_at);
}
bool same_desc(const BYTE *a, const BYTE *b, int depth = 0)
{
    if (!a || !b)
        return a == b;
    for (DWORD off = 0; off < 0x24; off += 4)
        if (!same_dword(a, b, off))
            return false;
    auto d = [&](DWORD from, DWORD to) {
        for (DWORD off = from; off < to; off += 4)
            if (!same_dword(a, b, off))
                return false;
        return true;
    };
    auto byte = [&](DWORD off) { return a[off] == b[off]; };
    const DWORD vt = *reinterpret_cast<const DWORD *>(a);
    switch (vt) {
    case 0x5f9314:                                 // the layer part's "Name" (string kind, own vtable)
    case 0x5ec91c: return d(0x2c, 0x34);           // string: +0x28 is not written
    case 0x5ecbf4: return d(0x28, 0x3c);           // object
    case 0x5fd7f4: return byte(0x28) && d(0x2c, 0x34);   // bit: the default byte
    case 0x5ecd6c:                                 // object array: the element descriptor at +0x38
        if (!d(0x28, 0x38) || !d(0x3c, 0x44) || !byte(0x44))
            return false;
        return depth == 0 ? same_desc(*reinterpret_cast<BYTE *const *>(a + 0x38), *reinterpret_cast<BYTE *const *>(b + 0x38), 1)
                          : true;
    case 0x5ec684: return d(0x28, 0x34) && byte(0x34);   // flags
    case 0x5fd518:                                 // enum int: +0x40 the enum names
        return d(0x28, 0x40) && (*reinterpret_cast<const DWORD *>(a + 0x40) != 0) == (*reinterpret_cast<const DWORD *>(b + 0x40) != 0);
    case 0x5fb840: return d(0x28, 0x30);           // object reference
    case 0x5fd2a0: return d(0x28, 0x2c);           // polygon: +0x2c is not written
    case 0x5fdb68: return d(0x28, 0x40);           // float
    case 0x5fd970:                                 // typed resource: +0x2e/+0x2f padding
        return byte(0x28) && byte(0x29) && byte(0x2a) && byte(0x2b) && byte(0x2c) && byte(0x2d) && d(0x30, 0x38);
    }
    return true;   // other kinds (inherited clones such as "Dynamic Properties"): the base part
}
DWORD desc_size(const CFieldDesc *f) { return f->vftable == 0x5fdb68 ? 0x44 : 0x34; }

struct GetterDef {
    DWORD address, var;
};
constexpr GetterDef GETTERS[] = {{0x4fd8c0, CLASS_LEVEL_OF_DIFFICULTY}, {0x4fd710, CLASS_LEVEL_OF_DIFFICULTY},
                                 {0x4fdfe0, CLASS_LEVEL_OF_DIFFICULTY}, {0x53f380, CLASS_PLASMA_TILE_MAP}};
struct RegFields {
    const char *name;
    DWORD address, var;
};
constexpr RegFields REGISTER_FIELDS[] = {{"CLevelOfDifficulty", 0x4fd920, CLASS_LEVEL_OF_DIFFICULTY},
                                         {"CPlasmaTileMap", 0x53f3e0, CLASS_PLASMA_TILE_MAP}};

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
            set[side] = ptrv(at<CClassInfo *(__cdecl *)()>(g.address)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<CClassInfo *(__cdecl *)()>(g.address)();
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
                "world data", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            CFieldDesc *fields[2][8];
            DWORD n[2], result[2], name_field[2], alt[2];
            void *const saved_name = *at<void **>(LOD_NAME_FIELD);
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                CClassInfo *&var = *at<CClassInfo **>(rf.var);
                CClassInfo *const saved = var;
                alignas(4) BYTE fake_class[sizeof(CClassInfo)];
                memset(fake_class, 0, sizeof fake_class);
                var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
                CodePatch p;
                patch_jump(p, 0x415780, (void *)&fake_register);
                // Side 1 calls the reimplementation directly, so it is compared also while its hook is disabled
                void *fn = at<void *>(rf.address);
                if (side == 1) {
                    Hook list[MAX_SITES];
                    const int nh = hooks_world_data(list);
                    for (int i = 0; i < nh; i++)
                        if (list[i].address == rf.address)
                            fn = list[i].replacement;
                }
                result[side] = reinterpret_cast<DWORD(__cdecl *)()>(fn)();
                unpatch(p);
                CClassInfo *c = var;
                n[side] = c->num_fields < 8 ? c->num_fields : 8;
                for (DWORD i = 0; i < 8; i++)
                    fields[side][i] = i < n[side] ? c->fields[i] : nullptr;
                name_field[side] = *at<DWORD *>(LOD_NAME_FIELD) == ptrv(fields[side][0]);
                alt[side] = ptrv(c->alt_name);
                result[side] = result[side] == ptrv(c) ? 1 : result[side] ? 2 : 0;   // the class (CLevelOfDifficulty) or AddField's
                if (c->fields)
                    heap_free(c->fields);
                if (var_zero)
                    pool_free(c, sizeof(CClassInfo));
                var = saved;
                *at<void **>(LOD_NAME_FIELD) = saved_name;
                if (side == 0)
                    hooks_in();
            }
            bool same = n[0] == n[1] && n[0] > 0 && name_field[0] == name_field[1] && alt[0] == alt[1] &&
                        (rf.address != 0x4fd920 || result[0] == result[1]);
            for (DWORD i = 0; i < n[0] && same; i++)
                same = same_desc(reinterpret_cast<const BYTE *>(fields[0][i]), reinterpret_cast<const BYTE *>(fields[1][i]));
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x]", rf.name, var_zero, n[0], n[1], result[0], result[1]);
            if (!same)
                for (DWORD i = 0; i < n[0] && i < n[1]; i++) {
                    const DWORD *a = reinterpret_cast<const DWORD *>(fields[0][i]), *b = reinterpret_cast<const DWORD *>(fields[1][i]);
                    for (DWORD k = 0; k < desc_size(fields[0][i]) / 4; k++)
                        if (a[k] != b[k] && !same_text_buffer(a[k], b[k]))
                            log("    field %u dword %u: %x / %x%s", i, k, a[k], b[k], same_desc(reinterpret_cast<const BYTE *>(a), reinterpret_cast<const BYTE *>(b)) ? " (kind: same)" : "");
                }
            t.check(same, rf.name, d);
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == n[1];
            for (DWORD i = 0; same_real && i < n[1]; i++)
                same_real = same_desc(reinterpret_cast<const BYTE *>(c->fields[c->num_inherited + i]), reinterpret_cast<const BYTE *>(fields[1][i]));
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c ? c->num_fields - c->num_inherited : 0, n[1]);
            real.check(same_real, rf.name, d);
            for (int side = 0; side < 2; side++)
                for (DWORD i = 0; i < n[side]; i++)
                    if (fields[side][i])
                        vcall<void *>(fields[side][i], 0, 1);
        }
}

// ---- Real data -----------------------------------------------------------------------------------------

bool heap_pointer(const void *p) { return ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && (ptrv(p) < 0x400000 || ptrv(p) >= IMAGE_END); }
// The level's tile map: an object with the tile map's vtable that the world reaches within two pointers
// (the CLayer's "Plasma Ground"). Only live blocks are read (check_real_objects.cpp, live_block_of): the
// world within its block, a pointer within the live block it points into (from there to the block's end, at
// most 0x400 bytes one level down and 0x200 two levels down, as before), and the tile map is taken only at
// the start of a live block that holds its fields up to the blending at +0x1068, so a stale pointer into a
// freed or neighbouring block gives nothing (docs/DECOMPILATION.md section 12, "The real objects of
// field_streams on real x86")
constexpr DWORD TILE_MAP_MIN = 0x106c;
DWORD g_path[4];
int g_tile_maps_rejected;
BYTE *find_tile_map_in(BYTE *block, DWORD size, int depth)
{
    if (!readable(block, size))
        return nullptr;
    for (DWORD off = 0; off + 4 <= size; off += 4) {
        BYTE *p = *reinterpret_cast<BYTE **>(block + off);
        DWORD start, bsize;
        if (!heap_pointer(p) || !live_block_of(ptrv(p), start, bsize))
            continue;
        g_path[depth] = off;
        if (*reinterpret_cast<DWORD *>(p) == VT_PLASMA_TILE_MAP) {
            if (start == ptrv(p) && bsize >= TILE_MAP_MIN) {
                g_path[depth + 1] = 0xffffffff;
                return p;
            }
            g_tile_maps_rejected++;
        }
        const DWORD rest = start + bsize - ptrv(p), cap = depth == 0 ? 0x400 : 0x200;
        if (depth < 2)
            if (BYTE *q = find_tile_map_in(p, rest < cap ? rest : cap, depth + 1))
                return q;
    }
    return nullptr;
}
BYTE *find_tile_map()
{
    g_tile_maps_rejected = 0;
    if (!live_blocks_take())
        return nullptr;
    BYTE *world = *at<BYTE **>(0x6c2080), *map = nullptr;
    DWORD start = 0, size = 0;
    if (world && live_block_of(ptrv(world), start, size) && start == ptrv(world))
        map = find_tile_map_in(world, size < 0x1000 ? size : 0x1000, 0);
    DWORD ls, lz;
    const bool list_live = map && live_block_of(*reinterpret_cast<DWORD *>(map + 0x1064), ls, lz);
    live_blocks_log("world data real data");
    char path[60] = "none";
    if (map) {
        int k = wsprintfA(path, "world +%x", g_path[0]);
        for (int d = 1; d < 3 && g_path[d] != 0xffffffff; d++)
            k += wsprintfA(path + k, ", +%x", g_path[d]);
    }
    log("  world data real data: tile map %x (%s; world block %x bytes; %d pointers with the tile map's vtable not "
        "taken: no live block of its size starts there)",
        ptrv(map), path, start == ptrv(world) ? size : 0, g_tile_maps_rejected);
    if (map && !list_live)
        log("  world data real data: the tile map's texture list %x lies in no live block", *reinterpret_cast<DWORD *>(map + 0x1064));
    live_blocks_free();
    return list_live ? map : nullptr;
}

void test_real(Tally &t)
{
    BYTE *map = find_tile_map();
    TextureList *l = map ? *reinterpret_cast<TextureList **>(map + 0x1064) : nullptr;
    if (!l || l->records.vftable != VT_PLASMA_TEXTURE_LIST) {
        log("  world data real data: no tile map found");
    } else {
        log("  world data real data: tile map texture list with %u entries, blending %x", l->num_entries,
            *reinterpret_cast<DWORD *>(map + 0x1068));
        for (DWORD i = 0; i < l->num_entries; i++) {
            two_sides(t, "real path", i, [&](DWORD &mask) -> DWORD {
                ZString out;
                zs_ctor(&out);
                at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x4b4880)(l, i, &out);
                st_text(text_of(out));
                if (i == 0)
                    log("    texture 0: %s", text_of(out));
                zs_dtor(&out);
                mask = 0;
                return 0;
            });
            if (void *tex = l->entries[i].object)
                two_sides(t, "real file of", i, [&](DWORD &mask) -> DWORD {
                    ZString out;
                    zs_ctor(&out);
                    const DWORD r = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x4b4820)(l, tex, &out);
                    st_text(text_of(out));
                    zs_dtor(&out);
                    mask = 0xff;
                    return r;
                });
        }
        two_sides(t, "real file of map", 0, [&](DWORD &mask) -> DWORD {
            ZString out;
            zs_ctor(&out);
            const DWORD r = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x4b4820)(l, map, &out);
            st_text(text_of(out));
            zs_dtor(&out);
            mask = 0xff;
            return r;
        });
    }
    auto *list = at<ZDefinitionList *>(DIFFICULTY_LIST);
    int n = 0;
    for (DWORD i = 0; list->entries && i < list->num_entries; i++) {
        auto *o = static_cast<LevelOfDifficulty *>(list->entries[i].object);
        if (!o || !o->name.buffer || !lstrcmpiA(o->name.c_str(), "This Value Has Not Yet Been Set"))
            continue;
        n++;
        two_sides(t, "real difficulty", i, [&](DWORD &mask) -> DWORD {
            const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x4fdf70)(o, 0);
            st_text(text_of(o->name));
            for (float f : o->percent)
                st(bits(f));
            mask = 0xffffffff;
            return r;
        });
    }
    log("  world data real data: %d difficulties of %u entries", n, list->num_entries);
}

}  // namespace

ZAX_CHECKS(world_data)
{
    (void)cls;
    (void)n;
    Tally objects{"world data: objects (difficulty, plasma tile map)"}, after{"world data: difficulty slot 18"},
        lists{"world data: texture lists"}, classes{"world data: class getters and RegisterFields"},
        registered{"world data: registered descriptors (real data)"}, real{"world data: tile map and difficulties (real data)"};
    log("  world data: objects");
    // Warm-up: the first tile map built outside the compared runs (the constructor sets up shared tables)
    if (void *m = at<void *(__cdecl *)()>(0x53f290)())
        at<void *(THISCALL *)(void *, DWORD)>(0x53f310)(m, 1);
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(objects, "difficulty", seed, [](DWORD &mask) { return difficulty_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
        two_sides(objects, "plasma", seed, [](DWORD &mask) { return plasma_case(mask); });
    objects.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(100); seed++)
        two_sides(after, "after read", seed, [](DWORD &mask) { return after_read_case(mask); });
    after.report();
    log("  world data: texture lists");
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
        two_sides(lists, "list", seed, [](DWORD &mask) { return list_case(mask); });
    lists.report();
    test_classes(classes, registered);
    classes.report();
    registered.report();
    test_real(real);
    real.report();
}

}  // namespace zax
