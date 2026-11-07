// Checks of the inventory definitions (inventory_defs.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h).
//  - Classes: Create, CreateArray, destructors, getters and RegisterFields of CProjectileInfo and
//    CInventoryTypeDefinition, the getters and RegisterFields of the abstract CInventoryBase and
//    CInventoryItemDefinition (with its alternative class name); the constructors and destructors called
//    directly on filled buffers (the C heap from the arena); the pair object; the inventory factory.
//  - Item definitions on fake objects: the projectile, icon and frame lists used by the code are swapped
//    for fake contents during a case (their bytes saved and restored; vtables with recorders in slots 1
//    and 37; image objects whose slot 36 gives a rectangle); the part, its owner and model and
//    CModel::FindSequence of the shoot sequence choice are fakes or recorders.
//  - Lists: the static initializers and exit destructors on the globals (saved, given a fake content,
//    restored; atexit replaced), the deleting destructors on fake lists, slot 7, 35 and 37 of the
//    category list (Object_CreateFromFile replaced by a recorder), Init on fresh lists.
//  - Real data: Init of fresh lists on the game's folders (the names against the global lists built at
//    startup; the registry entry taken off again), the fallback with Files_Count answering 0 first; the
//    catalogue's Init with the five Inits and the format list's sharing replaced by recorders; the
//    display names and descriptions of every catalogue entry (Local.txt); the loaded definitions'
//    projectile values, shoot angle, damage and ammunition.
#include "checks_inventory_common.h"
#include "inventory_defs.h"

WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);
WINIMPORT char *WINAPI lstrcpynA(char *d, const char *s, int n);

namespace zax {

int hooks_inventory_defs(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CInventoryBase", 8, 0x6c0534, 0, 0, 0, {0x4d4ec0, 0x4d4f10}, false},
    {"CProjectileInfo", 0x14, 0x6c0d50, 0x4d5010, 0x4d5060, 0x4d51b0, {0x4d5150, 0}, true},
    {"CInventoryItemDefinition", 0x60, ITEM_DEFINITION_CLASS, 0, 0, 0x4d5620, {0x4d55c0, 0x4d7820}, false},
    {"CInventoryTypeDefinition", 8, TYPE_DEFINITION_CLASS, 0x4d6fa0, 0x4d6fd0, 0, {0x4d70b0, 0x4d77d0}, true},
};

using T0 = DWORD(THISCALL *)(void *);
using T1 = DWORD(THISCALL *)(void *, DWORD);
using T2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using T3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);

template <typename C> void two_sides(Tally &t, const char *what, DWORD arg, C call_it)
{
    DWORD r[2];
    unsigned h[2];
    for (int side = 0; side < 2; side++) {
        log_reset(side);
        side_begin(side);
        r[side] = call_it(side);
        side_end(side);
        h[side] = g_loghash[side];
    }
    char d[160];
    wsprintfA(d, "[%s %x: %x / %x, log %d / %d]", what, arg, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4);
    const bool same = r[0] == r[1] && h[0] == h[1] && g_nlog[0] == g_nlog[1];
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, "inventory definitions", d);
}
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0x0bad; }
unsigned zs_hash(const ZString &z) { return z.buffer ? text_hash(z.c_str()) : 0x0bad; }

// ---- Constructors and destructors called directly ------------------------------------------------------

struct Life {
    const char *name;
    DWORD size, ctor, dtor, deleting;
};
const Life LIVES[] = {
    {"CInventoryBase", 8, 0x4d4f70, 0x4d4fc0, 0x4d4f90},
    {"CInventoryItemDefinition", 0x60, 0x4d6130, 0x4d61d0, 0x4d6190},
    {"CInventoryTypeDefinition", 8, 0x4d7130, 0x4d7110, 0x4d7030},
};

void test_lifetime(Tally &t, const Life &l, unsigned seed)
{
    DWORD r[2], e[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)(seed * 3));
        BYTE *o = static_cast<BYTE *>(pool_alloc(l.size));
        memset(o, (BYTE)random(), l.size);
        e[side] = at<T0>(l.ctor)(o) == ptrv(o);
        memcpy(g_obj[side], o, l.size);
        if (seed & 1) {
            r[side] = at<T0>(l.dtor)(o);
            pool_free(o, l.size);
        } else
            r[side] = at<T1>(l.deleting)(o, 1) == ptrv(o);
        memcpy(g_obj[side] + l.size, o, l.size < 0x40 ? l.size : 0x40);
        arena_off();
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[%s seed %u: eax %x / %x, +%x: %x / %x]", l.name, seed, r[0], r[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(e[0] && e[1] && r[0] == r[1] && same_region(g_obj[0], g_obj[1], l.size) && same_region(g_arena[0], g_arena[1], g_used[0]),
            l.name, d);
}

// The pair object, and the inventory factory (its variable 0x6c052c saved and restored)
void *__cdecl s_factory() { ev(0x801); return reinterpret_cast<void *>((uintptr_t)result(0x01)); }
void test_small(Tally &t, unsigned seed)
{
    DWORD r[2];
    DWORD mem[2][4];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        log_reset(side);
        side_begin(side);
        arena_on(side, 0x77);
        DWORD *o = static_cast<DWORD *>(heap_alloc(12));
        const DWORD a = random(), b = random();
        r[side] = at<T2>(0x4d4e80)(o, a, b) == ptrv(o);
        memcpy(mem[side], o, 12);
        r[side] += 2 * (rel(at<T1>(0x4d4ea0)(o, random() & 1), side) == rel(ptrv(o), side));
        mem[side][3] = o[0];
        DWORD &factory = *at<DWORD *>(0x6c052c);
        const DWORD saved = factory;
        at<void(__fastcall *)(DWORD, DWORD)>(0x4d4fe0)(ptrv((void *)&s_factory), 0);
        const DWORD set = factory;
        r[side] ^= at<DWORD(__cdecl *)()>(0x4d4ff0)() ^ (set == ptrv((void *)&s_factory) ? 0x100 : 0);
        factory = saved;
        arena_off();
        side_end(side);
    }
    t.check(r[0] == r[1] && memcmp(mem[0], mem[1], sizeof mem[0]) == 0 && same_log(), "pair and factory", "[pair object, factory]");
}

// ---- Fake lists ------------------------------------------------------------------------------------------

constexpr int NREC = 6;
struct FakeImage {
    DWORD vftable;
    DWORD id;
    int rect[4];
};
void *g_list_vtable[64];
void *g_image_vtable[40];
FakeImage g_images[NREC];
DWORD g_created;      // what slot 37 answers: an image, null, a projectile object
BYTE g_projectile[0x40];

const char *THISCALL f_list_name(ZDefinitionList *l, DWORD i)
{
    ev(0x901, i);
    return l->entries ? l->entries[i < l->num_entries ? i : 0].name : "";
}
void *THISCALL f_list_create(ZDefinitionList *, const char *dir, const char *name, void **out)
{
    ev(0x937, text_hash(dir), text_hash(name), out != nullptr);
    switch (g_created % 3) {
    case 0: return &g_images[g_created / 3 % NREC];
    case 1: return nullptr;
    default: return g_projectile;
    }
}
DWORD THISCALL f_list_path(ZDefinitionList *, DWORD i, ZString *out) { ev(0x939, i, out != nullptr); return result(0x39); }
DWORD THISCALL f_image_rect(FakeImage *im, int *out)
{
    ev(0xa36, im->id);
    memcpy(out, im->rect, 16);
    return result(0x36);
}
DWORD THISCALL f_object_delete(FakeImage *im, DWORD flags) { ev(0xa01, im->id, flags); return result(0x01); }
DWORD THISCALL f_monitor_delete(void *, DWORD flags) { ev(0xa07, flags); return result(0x07); }
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
CClassInfo *g_object_class_for_images;
CClassInfo *THISCALL f_image_class(void *) { ev(0xa02); return g_object_class_for_images; }
void *g_monitor_vtable[10];
DWORD g_monitor = 0;

void init_vtables()
{
    for (int i = 0; i < 40; i++)
        g_image_vtable[i] = (void *)&f_bad;
    g_image_vtable[1] = (void *)&f_object_delete;
    g_image_vtable[2] = (void *)&f_image_class;
    g_object_class_for_images = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
    g_image_vtable[36] = (void *)&f_image_rect;
    for (int i = 0; i < 10; i++)
        g_monitor_vtable[i] = (void *)&f_bad;
    g_monitor_vtable[7] = (void *)&f_monitor_delete;
    g_monitor = ptrv(g_monitor_vtable);
}
// A list vtable: the real one copied, slots 1, 37 and 39 recording
void make_list_vtable(DWORD real)
{
    memcpy(g_list_vtable, at<void *>(real), 41 * 4);
    for (int i = 41; i < 64; i++)
        g_list_vtable[i] = (void *)&f_bad;
    g_list_vtable[1] = (void *)&f_list_name;
    g_list_vtable[37] = (void *)&f_list_create;
    g_list_vtable[39] = (void *)&f_list_path;
}

// Fake records in the arena (count in front), names, some loaded, some being loaded
ZDefinitionRecord *make_records(DWORD n)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
    block[0] = n + 1;
    auto *r = reinterpret_cast<ZDefinitionRecord *>(block + 1);
    memset(r, 0, (n + 1) * 0x80);
    for (DWORD i = 0; i < n; i++) {
        wsprintfA(r[i].name, random() % 4 ? "Sub/Name %u" : "Name%u", i);
        const DWORD k = random() % 6;
        r[i].object = k == 0 ? &g_images[i % NREC] : nullptr;
        r[i].loading = k == 1;
    }
    return r;
}

// A global list given a fake content for the case (its bytes saved and restored)
SavedBytes g_saved_list[3];
void fake_global(int k, DWORD list, DWORD real_vtable, bool records)
{
    save_bytes(g_saved_list[k], list, 0x140);
    auto *l = at<ZDefinitionList *>(list);
    make_list_vtable(real_vtable);
    l->records.vftable = ptrv(g_list_vtable);
    const DWORD n = records ? 1 + random() % NREC : 0;
    l->entries = records ? make_records(n) : nullptr;
    l->num_entries = n;
    lstrcpyA(l->folder, random() % 3 ? "Fake/Folder" : "");
}
void restore_global(int k) { restore_bytes(g_saved_list[k]); }

void setup_images(unsigned seed)
{
    g_seed = seed * 31 + 7;
    for (int i = 0; i < NREC; i++) {
        g_images[i] = {ptrv(g_image_vtable), (DWORD)i + 1, {}};
        for (int k = 0; k < 4; k++)
            g_images[i].rect[k] = (int)(random() % 200) - 100;
    }
    for (DWORD i = 0; i < sizeof g_projectile; i++)
        g_projectile[i] = (BYTE)random();
    g_created = random();
}

// The item definition functions on a fake definition with the projectile and icon lists swapped
struct DefFn {
    DWORD address;
    const char *name;
};
const DefFn DEF_FNS[] = {
    {0x4d61f0, "Projectile"}, {0x4d6300, "ProjectileFloat24"}, {0x4d6410, "ProjectileFloat30"}, {0x4d68a0, "LoadIcons"},
    {0x4d6b70, "IconBounds"}, {0x4d6520, "ShootAngle"},          {0x4d6530, "StrikeDamage"},      {0x4d77c0, "HasAmmunition"},
    {0x4d62f0, "Accept"},
};
constexpr int NDEF_FNS = sizeof DEF_FNS / sizeof DEF_FNS[0];
BYTE g_def[2][0x60];
int g_rect[2][4];

DWORD run_def_fn(int f, int side, unsigned seed)
{
    setup_images(seed);
    BYTE *d = g_def[side];
    g_seed = seed;
    for (DWORD i = 0; i < 0x60; i++)
        d[i] = (BYTE)random();
    // the indices: 0 (none), within the records, beyond them
    static const DWORD INDEX_FIELDS[3] = {0x10, 0x14, 0x20};
    for (DWORD off : INDEX_FIELDS)
        *reinterpret_cast<DWORD *>(d + off) = random() % 4 == 0 ? 0 : random() % (NREC + 3);
    const bool records = f == 3 || seed % 7 != 0;
    arena_on(side, 0x5c);
    fake_global(0, 0x6cfdd8, *at<DWORD *>(0x6cfdd8), records || f > 2);
    fake_global(1, 0x6c0aa8, *at<DWORD *>(0x6c0aa8), records);
    // without records or with a record being loaded the projectile floats read from address 0x24 or
    // 0x30 (the original ends the game): only cases with a loaded projectile then
    if (f == 1 || f == 2) {
        auto *l = at<ZDefinitionList *>(0x6cfdd8);
        if (!l->entries || !l->num_entries) {
            restore_global(1);
            restore_global(0);
            arena_off();
            return 0x5a5a;
        }
        DWORD i = *reinterpret_cast<DWORD *>(d + 0x20);
        if (i >= l->num_entries)
            i = 0;
        l->entries[i].loading = 0;
        if (!l->entries[i].object)
            g_created = 2;   // slot 37 gives the projectile
    }
    if (f == 4) {   // IconBounds: the icons' images must exist
        auto *l = at<ZDefinitionList *>(0x6c0aa8);
        if (!l->entries || !l->num_entries) {
            *reinterpret_cast<DWORD *>(d + 0x10) = 0;
            *reinterpret_cast<DWORD *>(d + 0x14) = 0;
        } else
            for (DWORD k = 0; k < l->num_entries && k <= NREC; k++) {
                l->entries[k].loading = 0;
                l->entries[k].object = &g_images[k % NREC];
            }
    }
    DWORD r = 0;
    side_begin(side);
    switch (f) {
    case 1: case 2: case 5: r = bits(at<float(THISCALL *)(void *)>(DEF_FNS[f].address)(d)); break;
    case 6: r = bits(at<float(THISCALL *)(void *, DWORD)>(DEF_FNS[f].address)(d, 0)); break;   // one argument (ret 4)
    case 4: r = at<T1>(DEF_FNS[f].address)(d, ptrv(g_rect[side])); break;
    case 8: r = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(DEF_FNS[f].address)(d, 0, 0, 0, 0) & 0xff; break;   // al only (ret 0x10)
    default: r = at<T0>(DEF_FNS[f].address)(d); break;
    }
    side_end(side);
    // the lists' records and the definition in the event log, made comparable
    auto *p = at<ZDefinitionList *>(0x6cfdd8), *ic = at<ZDefinitionList *>(0x6c0aa8);
    ZDefinitionList *const lists[2] = {p, ic};
    for (auto *l : lists)
        for (DWORD i = 0; l->entries && i < NREC; i++)
            ev(0xc01, i, rel(ptrv(l->entries[i].object), side), l->entries[i].loading);
    if (r && f == 0)
        r = rel(r, side);
    if (f == 3)
        r = rel(r, side);
    restore_global(1);
    restore_global(0);
    arena_off();
    return r;
}

bool g_trace = false;   // a log line per case (to find a case that ends the game)
void test_def_fn(Tally &t, int f, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        if (g_trace && seed % 3000 < 2)
            log("  trace def fn %d seed %u side %d", f, seed, side);
        g_side = side;
        log_reset(side);
        r[side] = run_def_fn(f, side, seed);
    }
    char d[160];
    wsprintfA(d, "[%s seed %u: %x / %x, log %d / %d]", DEF_FNS[f].name, seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4);
    const bool same = r[0] == r[1] && same_log() && memcmp(g_def[0], g_def[1], 0x60) == 0 &&
                      (f != 4 || memcmp(g_rect[0], g_rect[1], 16) == 0);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, "item definitions", d);
}

// ---- The shoot sequence ------------------------------------------------------------------------------------

struct FakeOwner {
    DWORD vftable;
    CClassInfo *cls;
    BYTE body[0xc4 - 8];
    float a, b;   // 0xc4, 0xc8
};
static_assert(offsetof(FakeOwner, a) == 0xc4);
struct FakePart {
    BYTE body[0xc];
    FakeOwner *owner;   // 0xc
};
void *g_owner_vtable[4];
CClassInfo *THISCALL f_owner_class(FakeOwner *o) { ev(0xd02); return o->cls; }
DWORD g_model_marker = 0x4d0de1;
void *THISCALL s_part_model(void *part) { ev(0xd01, part != nullptr); return &g_model_marker; }
int g_find_result;
int THISCALL s_find_sequence(void *model, const char *name) { ev(0xd03, model == &g_model_marker, text_hash(name)); return g_find_result; }
CodePatch g_seq_patches[2];
void seq_in()
{
    patch_jump(g_seq_patches[0], 0x48b290, (void *)&s_part_model);
    patch_jump(g_seq_patches[1], 0x517240, (void *)&s_find_sequence);
}
void seq_out()
{
    unpatch(g_seq_patches[1]);
    unpatch(g_seq_patches[0]);
}
CClassInfo *g_owner_class, *g_object_class;
float angle(DWORD r)
{
    switch (r % 8) {
    case 0: return -1.5707964f;
    case 1: return 1.5707964f;
    case 2: { DWORD nan = 0x7fc00000; float f; memcpy(&f, &nan, 4); return f; }
    default: return ((float)(int)(r % 2001) - 1000.0f) / 100.0f;
    }
}
void test_shoot_sequence(Tally &t, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        if (g_trace && seed < 3)
            log("  trace shoot sequence seed %u side %d", seed, side);
        g_seed = seed;
        g_side = side;
        log_reset(side);
        alignas(4) BYTE def[0x60] = {};
        at<ZString *(THISCALL *)(void *)>(0x4dec90)(def + 8);   // the discharge sequence (empty)
        FakeOwner owner = {};
        owner.vftable = ptrv(g_owner_vtable);
        owner.cls = random() % 4 ? g_owner_class : g_object_class;
        owner.a = angle(random());
        owner.b = angle(random());
        FakePart part = {};
        part.owner = random() % 6 ? &owner : nullptr;
        g_find_result = random() % 3 ? 0 : -1;
        seq_in();
        side_begin(side);
        const DWORD z = at<T1>(0x4d6540)(def, random() % 7 ? ptrv(&part) : 0);
        side_end(side);
        seq_out();
        r[side] = z == ptrv(def + 8) ? 1 : z;
    }
    char d[120];
    wsprintfA(d, "[ShootSequence seed %u: %x / %x]", seed, r[0], r[1]);
    t.check(r[0] == r[1] && same_log(), "item definitions", d);
}
// Both function-local statics built once before the cases (as the original builds them), so the cases
// do not depend on which side ran first
void warm_up_sequences()
{
    alignas(4) BYTE def[0x60] = {};
    at<ZString *(THISCALL *)(void *)>(0x4dec90)(def + 8);
    FakeOwner owner = {};
    owner.vftable = ptrv(g_owner_vtable);
    owner.cls = g_owner_class;
    FakePart part = {};
    part.owner = &owner;
    seq_in();
    hooks_out();
    owner.a = -1.5707964f;
    at<T1>(0x4d6540)(def, ptrv(&part));
    owner.a = 1.5707964f;
    at<T1>(0x4d6540)(def, ptrv(&part));
    hooks_in();
    seq_out();
}

// ---- The list functions --------------------------------------------------------------------------------------

BYTE g_list_mem[2][0x140];

// A fake definition list in g_list_mem (records in the arena, a fake name tree)
ZDefinitionList *fake_list(int side, DWORD vtable)
{
    auto *l = reinterpret_cast<ZDefinitionList *>(g_list_mem[side]);
    memset(l, 0, sizeof *l);
    at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {RESOURCE_ALIASES_VTABLE, nullptr, 0, 0};
    l->registry_index = 0xffffffff;
    l->records.vftable = vtable;
    const DWORD n = random() % (NREC + 1);
    l->entries = n ? make_records(n) : nullptr;
    l->num_entries = n;
    lstrcpyA(l->folder, "Fake/Folder");
    l->monitor = random() % 3 ? &g_monitor : nullptr;
    return l;
}
DWORD list_state(int side)
{
    auto *l = reinterpret_cast<ZDefinitionList *>(g_list_mem[side]);
    DWORD h = fnv(2166136261u, l, 0x30);
    const DWORD tail[3] = {rel(ptrv(l->entries), side), l->num_entries, ptrv(l->monitor)};
    return fnv(h, tail, sizeof tail);
}

// Object_CreateFromFile replaced: records the path and answers an object of the list's class or not
BYTE g_made_object[2][8];
DWORD g_create_mode;
void *__fastcall s_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    ev(0xe01, zs_hash(*path), flags, write_back * 2 + (out != nullptr));
    if (g_create_mode % 3 == 0)
        return nullptr;
    // an object whose slot 2 is the list's class (CInventoryTypeDefinition or CInventoryItemDefinition) or CObject's
    DWORD *o = reinterpret_cast<DWORD *>(g_made_object[g_side]);
    o[0] = g_create_mode % 3 == 1 ? TYPE_DEFINITION_VTABLE : 0x5ec9ec;
    o[1] = 0;
    return o;
}

void test_lists(Tally &t, unsigned seed)
{
    const int k = (int)(seed % 6);
    DWORD r[2];
    unsigned s[2];
    for (int side = 0; side < 2; side++) {
        if (g_trace)
            log("  trace list case %d seed %u side %d", k, seed, side);
        g_seed = seed;
        g_side = side;
        log_reset(side);
        setup_images(seed);
        arena_on(side, 0x3e);
        make_list_vtable(ITEM_CATEGORY_LIST_VTABLE);
        ZDefinitionList *l = fake_list(side, ptrv(g_list_vtable));
        g_create_mode = random();
        ZString out;
        at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&out);
        side_begin(side);
        switch (k) {
        case 0: {   // slot 7: load an entry
            r[side] = rel(at<T1>(0x4d76c0)(l, random() % (NREC + 3)), side);
            break;
        }
        case 1: {   // slot 35: the file of an object
            void *probe = l->entries && random() % 2 ? l->entries[random() % (l->num_entries ? l->num_entries : 1)].object : &g_images[0];
            DWORD *typed = reinterpret_cast<DWORD *>(g_made_object[side]);
            typed[0] = TYPE_DEFINITION_VTABLE;
            if (l->entries && random() % 2)
                l->entries[0].object = typed, probe = random() % 2 ? typed : probe;
            if (!probe)
                probe = &g_images[1];   // the original reads the class of the object
            r[side] = at<T2>(0x4d7f40)(l, ptrv(probe), ptrv(&out));
            r[side] = (r[side] & 0xff) | (rel(r[side] & ~0xffu, side) << 8);
            break;
        }
        case 2: case 3: {   // slot 37 of both lists
            CodePatch p;
            patch_jump(p, 0x4164c0, (void *)&s_create_from_file);
            void *slot = nullptr;
            const DWORD fn = k == 2 ? 0x4d7620 : 0x4d6f00;
            if (k == 3)
                *reinterpret_cast<DWORD *>(g_made_object[side]) = g_create_mode % 3 == 1 ? ITEM_DEFINITION_VTABLE : 0x5ec9ec;
            r[side] = at<T3>(fn)(l, ptrv(random() % 2 ? "Inventory/Item Types" : ""), ptrv("Pistol"), random() % 2 ? ptrv(&slot) : 0);
            unpatch(p);
            r[side] = r[side] == ptrv(g_made_object[side]) ? 1 : r[side];
            break;
        }
        case 4: case 5: {   // the deleting destructors (not freed: the list memory is ours)
            const DWORD fn = k == 4 ? 0x4d7280 : 0x4d7870;
            r[side] = at<T1>(fn)(l, 0) == ptrv(l);
            break;
        }
        }
        side_end(side);
        at<DWORD(THISCALL *)(ZString *)>(0x4defd0)(&out);
        s[side] = k >= 4 ? list_state(side) : 0;
        arena_off();
    }
    char d[140];
    wsprintfA(d, "[list case %d seed %u: %x / %x, state %x / %x, log %d / %d]", k, seed, r[0], r[1], s[0], s[1], g_nlog[0] / 4, g_nlog[1] / 4);
    const bool same = r[0] == r[1] && s[0] == s[1] && same_log();
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, "definition lists", d);
}

// The static initializers and exit destructors on the globals: saved, given a fake content (records in
// the arena, a fake name tree), run, compared, restored; atexit replaced
void test_globals_init(Tally &t, unsigned seed)
{
    static const DWORD LISTS[2] = {ITEM_CATALOGUE_LIST, ITEM_CATEGORY_LIST};
    static const DWORD INIT[2] = {0x4d6ce0, 0x4d7150}, EXIT[2] = {0x4d6d50, 0x4d71c0};
    const int k = (int)(seed % 2);
    const bool exit_dtor = seed & 2;
    DWORD r[2];
    BYTE state[2][0x140];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        g_side = side;
        log_reset(side);
        setup_images(seed);
        SavedBytes saved;
        save_bytes(saved, LISTS[k], 0x140);
        arena_on(side, 0x21);
        auto *l = at<ZDefinitionList *>(LISTS[k]);
        l->aliases = {RESOURCE_ALIASES_VTABLE, nullptr, 0, 0};   // the exit destructor frees the aliases
        const DWORD n = random() % (NREC + 1);
        l->entries = n ? make_records(n) : nullptr;
        l->num_entries = n;
        l->monitor = random() % 2 ? &g_monitor : nullptr;
        g_atexit_calls = 0;
        g_atexit_function = 0;
        CodePatch p;
        patch_jump(p, 0x5d0863, (void *)&fake_atexit);
        CodePatch q;   // RecordList_Set / the ctor touch only the list
        side_begin(side);
        r[side] = exit_dtor ? at<DWORD(__cdecl *)()>(EXIT[k])() : at<DWORD(__cdecl *)()>(INIT[k])();
        side_end(side);
        (void)q;
        unpatch(p);
        r[side] = (exit_dtor ? r[side] : 0) ^ g_atexit_calls ^ g_atexit_function;
        memcpy(state[side], l, 0x140);
        reinterpret_cast<ZDefinitionList *>(state[side])->entries =
            reinterpret_cast<ZDefinitionRecord *>((uintptr_t)rel(ptrv(l->entries), side));
        arena_off();
        restore_bytes(saved);
    }
    char d[120];
    wsprintfA(d, "[list global %d %s seed %u: %x / %x]", k, exit_dtor ? "exit" : "init", seed, r[0], r[1]);
    t.check(r[0] == r[1] && memcmp(state[0], state[1], 0x140) == 0 && same_log(), "definition lists", d);
}

// ---- Real data: Init on the game's folders ------------------------------------------------------------------

DWORD g_count_calls;
DWORD __fastcall fake_files_count(ZFileSpec *spec, DWORD flags)
{
    ev(0xf01, text_hash(spec->folder), text_hash(spec->ext[0].buffer ? spec->ext[0].c_str() : nullptr), flags);
    return g_count_calls++ ? 2 : 0;
}

struct InitCase {
    const char *what;
    DWORD init, global, name, folder;   // name/folder 0: the function's own (0x4d7330)
};
const InitCase INITS[] = {
    {"Inventory Item Types", 0x4d7330, ITEM_CATEGORY_LIST, 0, 0},
    {"Inventory Icons", 0x4d7920, 0x6c0aa8, 0x616124, 0x616134},
    {"Inventory Frames", 0x4d7920, 0x6c0540, 0x6160fc, 0x616110},
    {"Inventory Detail Sequences", 0x4d7c40, 0x6c0948, 0x6160c4, 0x6160e0},
    {"Inventory HUD Sequences", 0x4d7c40, 0x6c06a0, 0x616094, 0x6160ac},
    {"Inventory Item Definitions", 0x4d7fe0, ITEM_CATALOGUE_LIST, 0x6161c0, 0x6161dc},
};

char g_names[2][3][0x80];
void test_init(Tally &t, const InitCase &c, bool fallback)
{
    memset(g_names, 0, sizeof g_names);
    DWORD r[2];
    unsigned names[2];
    int same_global = 0;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        log_reset(side);
        auto *l = reinterpret_cast<ZDefinitionList *>(g_list_mem[side]);
        memset(l, 0, sizeof *l);
        at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
        l->aliases = {RESOURCE_ALIASES_VTABLE, nullptr, 0, 0};
        l->registry_index = 0xffffffff;
        l->records.vftable = *at<DWORD *>(c.global);
        DWORD &count = *at<DWORD *>(0x6be088 + 0xc);
        DWORD *const *entries = at<DWORD **>(0x6be088 + 8);
        const DWORD before = count;
        CodePatch p;
        g_count_calls = 0;
        if (fallback)
            patch_jump(p, 0x4a12d0, (void *)&fake_files_count);
        arena_on(side, 0x11);
        side_begin(side);
        if (c.name)
            r[side] = at<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(c.init)(l, at<const char *>(c.name), at<const char *>(c.folder), 0);
        else
            r[side] = at<T0>(c.init)(l);
        side_end(side);
        if (fallback)
            unpatch(p);
        ev(0xf02, count - before, l->registry_index - before, count ? ((*entries)[(count - 1) * 4 + 1] == ptrv(l)) : 2);
        if (count == before + 1 && (*entries)[before * 4 + 1] == ptrv(l))
            count = before;
        unsigned h = fnv(2166136261u, &l->num_entries, 4);
        for (DWORD i = 0; i < l->num_entries; i++)
            h = fnv(h, l->entries[i].name, 0x80);
        h = fnv(h, l->folder, 0x100);
        names[side] = h;
        for (DWORD i = 0; i < l->num_entries && i < 3; i++)
            lstrcpynA(g_names[side][i], l->entries[i].name, 0x80);
        const auto *g = at<ZDefinitionList *>(c.global);
        bool same = !fallback && g->num_entries == l->num_entries;
        for (DWORD i = 0; same && i < l->num_entries; i++)
            same = lstrcmpiA(g->entries[i].name, l->entries[i].name) == 0;
        same_global += same;
        if (side == 0 && !fallback)
            log("  Init %s: %u entries (global list %u)", c.what, l->num_entries, g->num_entries);
        // the list destroyed again (the base vtable of the definition lists, records and name tree freed)
        at<T1>(0x4d7870)(l, 0);
        arena_off();
        if (r[side] == ptrv(l))
            r[side] = 1;
    }
    char d[160];
    wsprintfA(d, "[Init %s%s: eax %x / %x, names %x / %x, global %d]", c.what, fallback ? " (fallback)" : "", r[0], r[1], names[0], names[1],
              same_global);
    // 0x4d7330 returns the last ZString destructor's eax (a pool address of the side)
    // In the fallback Files_Count is a stand-in, and Files_ListNames of a folder listing then works on
    // what the last real count left: the sequence lists' fallback names are not compared
    const bool names_compared = !(fallback && c.init == 0x4d7c40);
    const bool same = (!c.name || r[0] == r[1]) && (!names_compared || names[0] == names[1]) && same_log() && (fallback || same_global == 2);
    if (!same)
        for (int i = 0; i < 3; i++)
            log("    name %d: '%s' / '%s'", i, g_names[0][i], g_names[1][i]);
    t.check(same, "definition lists (real data)", d);
}

// The catalogue's Init with the five Inits and the sharing replaced by recorders (the global lists stay)
DWORD THISCALL s_init(ZDefinitionList *l, const char *name, const char *folder, DWORD flags)
{
    ev(0xf11, ptrv(l), text_hash(name), text_hash(folder) ^ flags);
    return result(0x11);
}
DWORD THISCALL s_share(void *list, void *formats) { ev(0xf12, ptrv(list), formats != nullptr); return result(0x12); }
void test_catalogue_init(Tally &t)
{
    static const DWORD STAND[4] = {0x4d7920, 0x4d7c40, 0x4d7fe0, 0x4b4220};
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        log_reset(side);
        CodePatch p[4];
        side_begin(side);   // first: taking the hooks out puts the original bytes back where the stand-ins go
        for (int i = 0; i < 4; i++)
            patch_jump(p[i], STAND[i], i == 3 ? (void *)&s_share : (void *)&s_init);
        r[side] = at<T0>(0x4d6e10)(g_list_mem[side]) == 0 ? 0 : 1;
        for (int i = 3; i >= 0; i--)
            unpatch(p[i]);
        side_end(side);
        // the item catalogue is called with the list; made comparable in the log by this replacement
        for (int i = 0; i < g_nlog[side]; i += 4)
            if (g_log[side][i] == 0xf11 && g_log[side][i + 1] == ptrv(g_list_mem[side]))
                g_log[side][i + 1] = 0x11571;
    }
    bool same = r[0] == r[1] && g_nlog[0] == g_nlog[1] && memcmp(g_log[0], g_log[1], (DWORD)g_nlog[0] * 4) == 0;
    t.check(same, "definition lists (real data)", "[the item catalogue's Init]");
}

// The display names and descriptions of every catalogue entry, the loaded definitions' values
void test_texts(Tally &t, int &n)
{
    const auto *cat = at<ZDefinitionList *>(ITEM_CATALOGUE_LIST);
    for (DWORD i = 0; i < cat->num_entries; i++) {
        for (int k = 0; k < 2; k++)
            two_sides(t, k ? "Description" : "DisplayName", i, [&](int) {
                ZString out;
                at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&out);
                at<DWORD(__fastcall *)(DWORD, ZString *)>(k ? 0x4d67c0 : 0x4d66d0)(i, &out);
                const DWORD h = zs_hash(out);
                at<DWORD(THISCALL *)(ZString *)>(0x4defd0)(&out);
                return h;
            });
        n++;
        BYTE *d = static_cast<BYTE *>(cat->entries[i].object);
        if (!d)
            continue;
        two_sides(t, "ShootAngle", i, [&](int) { return bits(at<float(THISCALL *)(void *)>(0x4d6520)(d)); });
        two_sides(t, "StrikeDamage", i, [&](int) { return bits(at<float(THISCALL *)(void *, DWORD)>(0x4d6530)(d, 0)); });
        two_sides(t, "HasAmmunition", i, [&](int) { return at<T0>(0x4d77c0)(d); });
        two_sides(t, "Projectile", i, [&](int) { return at<T0>(0x4d61f0)(d); });
        if (at<T0>(0x4d61f0)(d)) {
            two_sides(t, "ProjectileFloat24", i, [&](int) { return bits(at<float(THISCALL *)(void *)>(0x4d6300)(d)); });
            two_sides(t, "ProjectileFloat30", i, [&](int) { return bits(at<float(THISCALL *)(void *)>(0x4d6410)(d)); });
        }
    }
}

}  // namespace

ZAX_CHECKS(inventory_defs)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_inventory_defs;
    if (!arena_setup()) {
        log("Check inventory definitions: no arena");
        return;
    }
    init_vtables();
    for (int i = 0; i < 4; i++)
        g_owner_vtable[i] = (void *)&f_bad;
    g_owner_vtable[2] = (void *)&f_owner_class;
    g_owner_class = at<CClassInfo *(__cdecl *)()>(0x49b810)();
    g_object_class = at<CClassInfo *(__cdecl *)()>(0x4163b0)();

    Tally objects{"inventory definitions: Create, CreateArray, destructors"}, getters{"inventory definitions: class getters"},
        fields{"inventory definitions: RegisterFields"}, registered{"inventory definitions: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    Tally life{"inventory definitions: constructors and destructors"};
    for (const Life &l : LIVES) {
        // a first run outside the arena (the defaults' resources loaded on first use)
        hooks_out();
        if (void *o = pool_alloc(l.size))
            at<T0>(l.ctor)(o);   // kept alive: the resources its defaults hold stay loaded
        hooks_in();
        for (unsigned k = 0; k < (unsigned)scaled(40); k++)
            test_lifetime(life, l, k);
    }
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_small(life, k);
    objects.report();
    getters.report();
    fields.report();
    life.report();

    Tally defs{"inventory definitions: item definition functions on fake lists"};
    for (int f = 0; f < NDEF_FNS; f++)
        for (unsigned k = 0; k < (unsigned)scaled(120); k++)
            test_def_fn(defs, f, 3000u * (unsigned)f + k);
    warm_up_sequences();
    for (unsigned k = 0; k < (unsigned)scaled(300); k++)
        test_shoot_sequence(defs, k);
    defs.report();

    Tally lists{"inventory definitions: definition lists"};
    for (unsigned k = 0; k < (unsigned)scaled(240); k++)
        test_lists(lists, k);
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_globals_init(lists, k);
    lists.report();

    Tally real{"inventory definitions: Init on the game's folders, texts, loaded definitions (real data)"};
    for (const InitCase &c : INITS) {
        test_init(real, c, false);
        test_init(real, c, true);
    }
    test_catalogue_init(real);
    int texts = 0;
    test_texts(real, texts);
    log("  %d catalogue entries' texts", texts);
    real.report();
    registered.report();
}

}  // namespace zax
