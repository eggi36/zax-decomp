// Checks of quest_data.cpp against the original, in the game thread (machinery: checks_inventory_common.h;
// side 0 with all hooks of the module taken out, side 1 through the reimplementation).
//  - Classes: CUserDefinedClassRef, CVoiceLogElement, CWatchInventoryDetailRenderer: Create (pool block
//    filled with a pattern), CreateArray with 0-3 elements, the deleting destructors, the class getters
//    (the class variable set and 0), RegisterFields into a fake class info, the registered descriptors and
//    class infos (real data).
//  - The user-defined class list's slots and CUserDefinedClassRef's lookups on a fake list (records,
//    names, keep and loading bytes and loaded objects from the seed; its vtable a copy of the real one with
//    the loader slot 37 recorded): keep 0x590100, the entry path 0x590180, the file of an object 0x590120,
//    both loaders 0x590020/0x590080 (Object_CreateFromFile a stand-in), the definition 0x590400, the class
//    name 0x590520 and the instance 0x5906a0 (the list getter 0x58fe20 and Object_Clone stand-ins); the
//    objects are fakes whose class (slot 2) is one of four real class infos.
//  - The int name list: constructor, appends, values and destructor on a block of each side.
//  - The voice log: a line added 0x592900 (the elements and the array of each side made from the seed,
//    Object_Clone, the list box 0x406890 and the world's time (slot 13 of a fake world) stand-ins), the
//    text of line i 0x592770 (CZaxDialogAction::SplitLine a stand-in giving speakers and lines of 0 to
//    0x100 characters), the count.
//  - Real data: the real user-defined class list's keep bytes, entry paths and objects' files; the
//    message name list 0x6c2d18's values; the registered class infos.
#include "checks_inventory_common.h"
#include "definitions.h"
#include "x87.h"

namespace zax {

int hooks_quest_data(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0x0badc0deu; }
unsigned zs_hash(const void *z) { return z ? text_hash(static_cast<const ZString *>(z)->c_str()) : 0; }
void zs_init(ZString *z) { at<ZString *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_free(ZString *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_set(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }

// ---- Fakes -------------------------------------------------------------------------------------------------

constexpr int NFAKE = 24;
struct Fake {
    DWORD vftable;
    DWORD id;
    BYTE body[0x78];   // a CZaxDialogAction's display type +0x78 is written into it
};
Fake g_fake[NFAKE];
void *g_fvt[64];
CClassInfo *g_fake_class[NFAKE];
CClassInfo *g_classes[4];   // CObject, CUserDefinedClassRef, CVoiceLogElement, CAction

int fake_index(const void *p)
{
    const DWORD a = ptrv(p), b = ptrv(g_fake);
    if (a >= b && a < b + sizeof g_fake && (a - b) % sizeof(Fake) == 0)
        return (int)((a - b) / sizeof(Fake));
    return -1;
}
// A pointer for the log: fakes by number, null, others as they are
DWORD fid(const void *p)
{
    const int i = fake_index(p);
    return i >= 0 ? 0xf0000100u + (DWORD)i : ptrv(p);
}

// The answers of the stand-ins: a sequence from the case's seed (the same on both sides)
unsigned g_answer_seed;
DWORD ans()
{
    g_answer_seed = g_answer_seed * 1103515245u + 12345u;
    return g_answer_seed >> 4;
}

void *THISCALL f_delete(Fake *self, DWORD flags)
{
    ev(0x9f1, fid(self), flags);
    return self;
}
CClassInfo *THISCALL f_class(Fake *self) { return g_fake_class[self->id % NFAKE]; }
DWORD THISCALL f_bad(Fake *self)
{
    ev(0x9ff, fid(self));
    return 0;
}

bool g_never_null;   // the loader and the clones give a fake always (calls through the result follow)

// ---- The fake definition list (user-defined classes) ------------------------------------------------------

constexpr int NREC = 8;
ZDefinitionList g_list;
ZDefinitionRecord g_records[NREC];
void *g_list_vt[42];

// Slot 37 of the fake list (dir, name, out): a fake or null
void *THISCALL s_list_create(void *list, const char *dir, const char *name, void **out)
{
    ev(0x904, text_hash(dir), text_hash(name), ptrv(out) - ptrv(g_records));
    ev(0x914, list == &g_list);
    const DWORD a = ans();
    return a % 5 || g_never_null ? &g_fake[a % NFAKE] : nullptr;
}   // the loader and the clones give a fake always (the class name slot calls through)

// Object_CreateFromFile 0x4164c0 (fastcall path, flags; write back, out)
void *__fastcall s_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    ev(0x901, zs_hash(path), flags, write_back);
    ev(0x911, out != nullptr);   // a stack address: only whether it is given
    const DWORD a = ans();
    return a % 4 ? &g_fake[a % NFAKE] : nullptr;
}
// Object_Clone 0x416760 (thiscall; a, b)
void *THISCALL s_clone(void *o, DWORD a, DWORD b)
{
    ev(0x902, fid(o), a, b);
    const DWORD r = ans();
    return r % 6 || g_never_null ? &g_fake[r % NFAKE] : nullptr;
}
ZDefinitionList *__cdecl s_list_get()
{
    ev(0x903);
    return &g_list;
}

const char *const NAMES[] = {"Class A", "Folder/Class B", "Deep/Folder/C", "x", "Weapons/Ion Rifle Def", "Z", "Monster/Big/Thing", "q/r"};
const char *const FOLDERS[] = {"Resources/User Classes", "", "Resources", "A/B/"};

void build_list(unsigned seed, bool force_entries)
{
    g_seed = seed;
    const bool entries = force_entries || rnd(8) != 0;
    memset(&g_list, 0, sizeof g_list);
    memset(g_records, 0, sizeof g_records);
    const DWORD n = entries ? 1 + rnd(NREC) : 0;
    for (DWORD i = 0; i < n; i++) {
        lstrcpyA(g_records[i].name, NAMES[(i + seed) % 8]);
        g_records[i].object = rnd(3) ? &g_fake[rnd(NFAKE)] : nullptr;
        g_records[i].keep = (BYTE)rnd(3);
        g_records[i].in_use = (BYTE)rnd(2);
        g_records[i].loading = rnd(6) == 0;
    }
    g_list.records.vftable = ptrv(g_list_vt);
    g_list.records.data = reinterpret_cast<char *>(g_records);
    g_list.records.stride = 0x80;
    g_list.records.num = n;
    g_list.entries = entries ? g_records : nullptr;
    g_list.num_entries = n;
    lstrcpyA(g_list.folder, FOLDERS[rnd(4)]);
    g_list.registry_index = 0xffffffff;
    for (int i = 0; i < NFAKE; i++) {
        g_fake[i].vftable = ptrv(g_fvt);
        g_fake[i].id = (DWORD)i;
        for (DWORD k = 0; k < sizeof g_fake[i].body; k += 4)
            D(g_fake[i].body, k) = random();
        g_fake_class[i] = g_classes[rnd(4)];
    }
    g_answer_seed = random();
}
unsigned list_state()
{
    unsigned h = fnv(2166136261u, &g_list, sizeof g_list);
    h = fnv(h, g_records, sizeof g_records);
    return fnv(h, g_fake, sizeof g_fake);
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand LIST_STANDS[] = {{0x4164c0, (void *)&s_create_from_file}, {0x416760, (void *)&s_clone}, {0x58fe20, (void *)&s_list_get}};
CodePatch g_patches[8];
template <int N> void stands_on(const Stand (&s)[N])
{
    for (int i = 0; i < N; i++)
        patch_jump(g_patches[i], s[i].address, s[i].fn);
}
template <int N> void stands_off(const Stand (&)[N])
{
    for (int i = N - 1; i >= 0; i--)
        unpatch(g_patches[i]);
}

enum ListFn { L_KEEP, L_PATH, L_FILE_OF, L_CREATE, L_CREATE_BASE, L_DEFINITION, L_CLASS_NAME, L_INSTANCE, NLIST };
const char *const LIST_NAMES[NLIST] = {"UdcList::Keep 0x590100", "UdcList::EntryPath 0x590180", "UdcList::FileOfObject 0x590120",
                                        "UdcList::CreateFromFile 0x590020", "UdcList::CreateFromFileBase 0x590080",
                                        "CUserDefinedClassRef::Definition 0x590400", "CUserDefinedClassRef::ClassName 0x590520",
                                        "CUserDefinedClassRef::CreateInstance 0x5906a0"};

BYTE *g_refs[4];   // real CUserDefinedClassRef objects (Create), made once

void test_list(Tally &t, ListFn f, unsigned seed)
{
    DWORD eax[2], state[2], out[2], extra[2];
    for (int side = 0; side < 2; side++) {
        // the class name slot calls the definition through: never null there
        g_never_null = f == L_CLASS_NAME;
        build_list(seed, f == L_CLASS_NAME || f == L_PATH);
        if (f == L_CLASS_NAME)
            for (DWORD i = 0; i < g_list.num_entries; i++)
                g_records[i].loading = 0;
        log_reset(side);
        side_begin(side);
        stands_on(LIST_STANDS);
        ZString z;
        zs_init(&z);
        const DWORD i = rnd(g_list.num_entries + 2);
        BYTE *ref = g_refs[rnd(4)];
        D(ref, 8) = i;
        void *obj;
        switch (rnd(4)) {
        case 0: obj = g_list.num_entries ? g_records[rnd(g_list.num_entries)].object : nullptr; break;
        case 1: obj = &g_fake[rnd(NFAKE)]; break;
        case 2: obj = ref; break;
        default: obj = f == L_FILE_OF ? ref : nullptr; break;
        }
        if (f == L_FILE_OF && !obj)
            obj = &g_fake[0];
        CClassInfo *cls = reinterpret_cast<CClassInfo *>(0x13579bdf);
        switch (f) {
        case L_KEEP: eax[side] = g_list.num_entries ? at<DWORD(THISCALL *)(void *, DWORD)>(0x590100)(&g_list, i % g_list.num_entries) : at<DWORD(THISCALL *)(void *, DWORD)>(0x590100)(&g_list, 0); break;
        case L_PATH:
            at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x590180)(&g_list, g_list.num_entries ? i % g_list.num_entries : 0, &z);
            eax[side] = 0;   // the ZString destructor's
            break;
        case L_FILE_OF:
            eax[side] = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x590120)(&g_list, obj, &z);
            // found: al 1 over slot 39's eax, whose upper bytes are the ZString destructor's (a pool
            // address of the side): only al
            if ((eax[side] & 0xff) == 1)
                eax[side] = 1;
            break;
        case L_CREATE:
        case L_CREATE_BASE: {
            void *o = reinterpret_cast<void *>(0x2468ace0);
            eax[side] = fid(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(f == L_CREATE ? 0x590020 : 0x590080)(
                &g_list, FOLDERS[rnd(4)], NAMES[rnd(8)], rnd(3) ? &o : nullptr));
            break;
        }
        case L_DEFINITION: eax[side] = fid(at<void *(THISCALL *)(void *)>(0x590400)(ref)); break;
        case L_CLASS_NAME:
            if (fake_index(obj) >= 0 && g_fake_class[fake_index(obj)] == g_classes[1])
                g_fake_class[fake_index(obj)] = g_classes[0];   // a fake is no CUserDefinedClassRef (it has no index)
            eax[side] = ptrv(at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(0x590520)(nullptr, obj, rnd(3) ? &cls : nullptr));
            break;
        case L_INSTANCE:
            if (fake_index(obj) >= 0 && g_fake_class[fake_index(obj)] == g_classes[1])
                g_fake_class[fake_index(obj)] = g_classes[0];
            eax[side] = fid(at<void *(THISCALL *)(void *, void *)>(0x5906a0)(nullptr, obj));
            break;
        default: break;
        }
        out[side] = zs_hash(&z);
        extra[side] = ptrv(cls);
        zs_free(&z);
        stands_off(LIST_STANDS);
        side_end(side);
        state[side] = list_state();
    }
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, out %x / %x, class %x / %x, log %d / %d, state %x / %x]", seed, eax[0], eax[1], out[0], out[1], extra[0],
              extra[1], g_nlog[0], g_nlog[1], state[0], state[1]);
    const bool same = eax[0] == eax[1] && out[0] == out[1] && extra[0] == extra[1] && same_log() && state[0] == state[1];
    if (!same)
        log_first_difference();
    t.check(same, LIST_NAMES[f], d);
}

// The real list (read only): keep bytes, entry paths, the files of its loaded objects
void test_real_list(Tally &t)
{
    auto *l = at<ZDefinitionList *(__cdecl *)()>(0x58fe20)();
    log("  user-defined class list: %u records, folder '%s'", l->num_entries, l->folder);
    for (DWORD i = 0; i < l->num_entries; i++) {
        DWORD k[2], p[2], f[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            k[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x590100)(l, i);
            ZString z;
            zs_init(&z);
            at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(0x590180)(l, i, &z);
            p[side] = zs_hash(&z);
            f[side] = 0;
            if (l->entries && l->entries[i].object) {
                zs_set(&z, "");
                f[side] = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x590120)(l, l->entries[i].object, &z);
                if ((f[side] & 0xff) == 1)
                    f[side] = 1;   // the upper bytes the ZString destructor's (see test_list)
                f[side] ^= zs_hash(&z);
            }
            zs_free(&z);
            side_end(side);
        }
        char d[160];
        wsprintfA(d, "[record %u '%s': keep %x / %x, path %x / %x, file %x / %x]", i, l->entries ? l->entries[i].name : "", k[0], k[1], p[0], p[1],
                  f[0], f[1]);
        t.check(k[0] == k[1] && p[0] == p[1] && f[0] == f[1], "UdcList (real)", d);
    }
}

// ---- The int name list ----------------------------------------------------------------------------------------

const char *const ENTRY_NAMES[] = {"", "Left", "Center", "Right", "A rather long entry name of the int name list, 64 characters...", "x"};

void test_names(Tally &t, unsigned seed)
{
    BYTE obj[2][0x14];
    DWORD eax[2], hash[2], after[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        memset(obj[side], (BYTE)seed, 0x14);
        BYTE *o = obj[side];
        eax[side] = at<DWORD(THISCALL *)(void *)>(0x592200)(o) == ptrv(o);
        const DWORD n = rnd(12);
        unsigned h = 2166136261u;
        for (DWORD i = 0; i < n; i++) {
            const DWORD v = rnd(3) ? rnd(10) : random();
            const DWORD r = at<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x5922b0)(o, ENTRY_NAMES[rnd(6)], v);
            h = fnv(h, &r, 4);
        }
        const DWORD count = D(o, 0xc);
        h = fnv(fnv(h, &count, 4), o + 0x10, 4);
        h = fnv(h, o, 4);
        h = fnv(h, o + 4, 4);
        for (DWORD i = 0; i < count; i++) {
            const DWORD v = at<DWORD(THISCALL *)(void *, DWORD)>(0x592310)(o, i);
            h = fnv(h, &v, 4);
            h = fnv_text(h, *at<const char **>(D(at<void *>(D(o, 8)), i * 4)));
        }
        hash[side] = h;
        at<void(THISCALL *)(void *)>(0x592220)(o);
        after[side] = fnv(2166136261u, o, 0x14);
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[seed %u: this %u / %u, contents %x / %x, after the destructor %x / %x]", seed, eax[0], eax[1], hash[0], hash[1], after[0], after[1]);
    t.check(eax[0] == eax[1] && eax[0] && hash[0] == hash[1] && after[0] == after[1], "FieldNameList", d);
}

// The message names (FieldNameList 0x6c2d18, made on first use by 0x506620): every value (real data)
void test_real_names(Tally &t)
{
    if (!(*at<BYTE *>(0x6c2d10) & 1))
        return;
    BYTE *l = at<BYTE *>(0x6c2d18);
    log("  message names: %u", D(l, 0xc));
    for (DWORD i = 0; i < D(l, 0xc); i++) {
        DWORD v[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            v[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x592310)(l, i);
            side_end(side);
        }
        char d[96];
        wsprintfA(d, "[message name %u: %x / %x]", i, v[0], v[1]);
        t.check(v[0] == v[1], "FieldNameList::Value (real)", d);
    }
}

// ---- The voice log --------------------------------------------------------------------------------------------

struct FakeWorld {
    DWORD vftable;
    BYTE body[0x40];
};
FakeWorld g_world;
void *g_wvt[64];
long double g_time;
long double THISCALL s_time(void *)
{
    ev(0x907);
    return g_time;
}
void THISCALL s_split(void *action, ZString *speaker, ZString *line)
{
    ev(0x905, fid(action));
    const char *const SPEAKERS[] = {"", "Zax", "Kiro", "A speaker with a long name"};
    const char *const LINES[] = {
        "", "Hello.", "This line is exactly eighty characters long, which is the limit of the voice log",
        "This line is eighty-one characters long, which is one more than the voice log's li",
        ("A very long line of dialogue that goes on and on well beyond the eighty characters the voice log shows of a "
         "line, so it gets cut at seventy-seven characters and three dots are appended to it by the code.")};
    const DWORD a = ans();
    zs_set(speaker, SPEAKERS[a % 4]);
    zs_set(line, LINES[(a >> 4) % 5]);
}
DWORD THISCALL s_list_box(void *box, void *log_list)
{
    ev(0x906, fid(box), ptrv(log_list));
    return ans();
}
const Stand VOICE_STANDS[] = {{0x416760, (void *)&s_clone}, {0x5a4bd0, (void *)&s_split}, {0x406890, (void *)&s_list_box}};

struct VoiceLogFake {
    DWORD vftable;
    void *save_data;
    void *list_box;
};
VoiceLogFake g_voice;
BYTE g_save[0x40];       // a fake save data: +0x18 the pointer array of the elements
void *g_voice_vt[64];    // slot 22 the real count 0x592a90 (ours or the original's)

const double TIMES[] = {0.0, 12.5, 12.5, 3600.0, 86400.5, 1e7, -3.0, 1.0 / 3.0};

// The elements of a side (made with the class's Create), the array, the world time
DWORD g_saved_world;
void build_voice(unsigned seed)
{
    g_seed = seed;
    g_voice.vftable = ptrv(g_voice_vt);
    g_voice.save_data = g_save;
    g_voice.list_box = &g_fake[NFAKE - 1];
    memset(g_save, 0, sizeof g_save);
    D(g_save, 0x18) = 0x5fa59c;   // a pool pointer array's vtable (not called)
    const DWORD n = rnd(6), cap = n + (rnd(3) ? rnd(4) : 0);
    void **data = cap ? static_cast<void **>(pool_get_alloc(cap * 4)) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x592410)());
        D(e, 8) = ptrv(&g_fake[rnd(NFAKE - 1)]);
        double w = TIMES[rnd(8)];
        if (rnd(10) == 0) {
            const DWORD nan[2] = {0, 0x7ff80000u};
            memcpy(&w, nan, 8);
        }
        memcpy(e + 0xc, &w, 8);
        D(e, 0x14) = rnd(4);
        data[i] = e;
    }
    D(g_save, 0x1c) = ptrv(data);
    D(g_save, 0x20) = n;
    D(g_save, 0x24) = cap;
    for (int i = 0; i < NFAKE; i++) {
        g_fake[i].vftable = ptrv(g_fvt);
        g_fake[i].id = (DWORD)i;
        memset(g_fake[i].body, 0x11, sizeof g_fake[i].body);   // +0x78 compared below
    }
    // the time: one of the elements' (the same moment) or another
    if (n && rnd(3) == 0)
        g_time = *reinterpret_cast<const double *>(static_cast<BYTE *>(data[n - 1]) + 0xc);
    else
        g_time = (long double)TIMES[rnd(8)] + (long double)rnd(1000) / 7.0L;
    g_answer_seed = random();
}
// The elements and array of a side: count, capacity, each element's action, time, order and text
unsigned voice_state()
{
    unsigned h = fnv(2166136261u, &D(g_save, 0x20), 8);
    void **data = at<void **>(D(g_save, 0x1c));
    for (DWORD i = 0; i < D(g_save, 0x20); i++) {
        BYTE *e = static_cast<BYTE *>(data[i]);
        const DWORD action = fid(at<void *>(D(e, 8)));
        h = fnv(h, &action, 4);
        h = fnv(h, e + 0xc, 0xc);
        h = fnv_text(h, reinterpret_cast<ZString *>(e + 0x18)->c_str());
        h = fnv(h, e, 4);   // the vtable
    }
    for (int i = 0; i < NFAKE; i++)
        h = fnv(h, g_fake[i].body + 0x70, 4);   // the display type CZaxDialogAction::SetDisplayType writes
    return h;
}
void free_voice()
{
    void **data = at<void **>(D(g_save, 0x1c));
    for (DWORD i = 0; i < D(g_save, 0x20); i++) {
        BYTE *e = static_cast<BYTE *>(data[i]);
        if (!e)
            continue;
        D(e, 8) = 0;
        vcall<void *>(e, 1, 1);
    }
    if (data)
        pool_get_free(data, D(g_save, 0x24) * 4);
}

enum VoiceFn { V_ADD, V_NAME, V_COUNT, NVOICE };
const char *const VOICE_NAMES[NVOICE] = {"VoiceLog::AddLine 0x592900", "VoiceLog::Name 0x592770", "VoiceLog::Count 0x592a90"};

void test_voice(Tally &t, VoiceFn f, unsigned seed)
{
    DWORD eax[2], state[2];
    for (int side = 0; side < 2; side++) {
        g_never_null = true;   // the copy's display type is set without a test
        build_voice(seed);
        log_reset(side);
        side_begin(side);
        g_saved_world = *at<DWORD *>(0x6c2080);
        *at<DWORD *>(0x6c2080) = ptrv(&g_world);
        stands_on(VOICE_STANDS);
        const DWORD n = D(g_save, 0x20);
        switch (f) {
        case V_ADD: eax[side] = at<DWORD(THISCALL *)(void *, void *)>(0x592900)(&g_voice, &g_fake[rnd(NFAKE - 1)]); break;
        case V_NAME:
            if (n) {
                const DWORD i = rnd(n);
                const char *r = at<const char *(THISCALL *)(void *, DWORD)>(0x592770)(&g_voice, i);
                eax[side] = text_hash(r) ^ (r == reinterpret_cast<ZString *>(at<BYTE *>(D(at<void *>(D(g_save, 0x1c)), i * 4)) + 0x18)->c_str());
            } else {
                eax[side] = 0;
            }
            break;
        default: eax[side] = at<DWORD(THISCALL *)(void *)>(0x592a90)(&g_voice); break;
        }
        stands_off(VOICE_STANDS);
        *at<DWORD *>(0x6c2080) = g_saved_world;
        side_end(side);
        state[side] = voice_state();
        free_voice();
    }
    char d[160];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, state %x / %x]", seed, eax[0], eax[1], g_nlog[0], g_nlog[1], state[0], state[1]);
    const bool same = eax[0] == eax[1] && same_log() && state[0] == state[1];
    if (!same)
        log_first_difference();
    t.check(same, VOICE_NAMES[f], d);
}

const ClassDef CLASSES[] = {
    {"CUserDefinedClassRef", 0xc, 0x713ba8, 0x590250, 0x590280, 0x590370, {0x590200, 0}, true},
    {"CVoiceLogElement", 0x1c, 0x713c1c, 0x592410, 0x592450, 0x5925e0, {0x592580, 0}, true},
    {"CWatchInventoryDetailRenderer", 8, 0x713c58, 0x5966b0, 0x5966f0, 0, {0x596660, 0x5967f0}, true},
};

void init_tables()
{
    for (int i = 0; i < 64; i++) {
        g_fvt[i] = (void *)&f_bad;
        g_wvt[i] = (void *)&f_bad;
        g_voice_vt[i] = (void *)&f_bad;
    }
    g_fvt[1] = (void *)&f_delete;
    g_fvt[2] = (void *)&f_class;
    g_wvt[13] = (void *)&s_time;
    g_voice_vt[22] = at<void *>(0x592a90);
    g_world.vftable = ptrv(g_wvt);
    memcpy(g_list_vt, at<void *>(0x6020f8), sizeof g_list_vt);
    g_list_vt[37] = (void *)&s_list_create;
    g_classes[0] = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
    g_classes[1] = at<CClassInfo *(__cdecl *)()>(0x590200)();
    g_classes[2] = at<CClassInfo *(__cdecl *)()>(0x592580)();
    g_classes[3] = at<CClassInfo *(__cdecl *)()>(0x41a650)();
    for (BYTE *&r : g_refs)
        r = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x590250)());   // kept
}

}  // namespace

ZAX_CHECKS(quest_data)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_quest_data;
    if (!arena_setup()) {
        log("Check quest data: no arena");
        return;
    }
    init_tables();
    Tally objects{"quest data: Create, CreateArray, destructors"}, getters{"quest data: class getters"}, fields{"quest data: RegisterFields"},
        registered{"quest data: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally list{"quest data: the user-defined class list and CUserDefinedClassRef (fakes)"};
    for (int f = 0; f < NLIST; f++) {
        log("  %s", LIST_NAMES[f]);
        for (unsigned s = 0; s < (unsigned)scaled(400); s++)
            test_list(list, (ListFn)f, 0x10000u * (unsigned)(f + 1) + s);
    }
    list.report();
    Tally names{"quest data: the int name list (constructor, append, value, destructor)"};
    for (unsigned s = 0; s < (unsigned)scaled(400); s++)
        test_names(names, s);
    names.report();
    Tally voice{"quest data: the voice log (fakes)"};
    for (int f = 0; f < NVOICE; f++) {
        log("  %s", VOICE_NAMES[f]);
        for (unsigned s = 0; s < (unsigned)scaled(400); s++)
            test_voice(voice, (VoiceFn)f, 0x80000u * (unsigned)(f + 1) + s);
    }
    voice.report();
    Tally real{"quest data: the user-defined class list and the message names (real data)"};
    test_real_list(real);
    test_real_names(real);
    if (void *app = *at<void **>(0x713f14))
        if (BYTE *save = at<BYTE *>(D(app, 0x34c)))
            log("  voice log of the running game: %u lines", D(save, 0x20));
    real.report();
}

}  // namespace zax
