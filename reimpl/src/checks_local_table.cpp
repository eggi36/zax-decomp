// Checks of the Local.txt table (local_table.h) against the original, in the game thread.
//
// Every case runs twice: side 0 with all hooks of the module taken out (the original's first bytes put
// back, so nested calls such as Load -> Add -> the comparison run the original too), side 1 through the
// reimplementation. Compared are value streams (hashed, the first 4096 values kept for the log):
//  - Load on tables of our own (never the game's) with test files in the temp folder: empty, one pair,
//    comments and lines that only look like pairs, duplicates, a second line with another prefix,
//    parts over several lines (also empty lines and an empty first line), keys with "[END]" inside
//    or twice, LF-only line ends (one line up to the end), a line too long for the reader, 9-47
//    generated pairs (the growth at 10, 20, …), missing files, relative names (disk, archives, pack),
//    and the real Local.txt of the language pack GERMAN.RED (687 pairs; the game folder has no language pack, so
//    zax-testtools/tools/vmtest.py sends it with every --reimpl job as Zax-Reimpl-Local.txt next to Zax.exe; until 2026-11-01
//    build.sh embedded it, which tied the DLL's bytes to the game data); in sequences so old entries are freed and a failed Load translates
//    with the old ones. Before each Load the registered strings point to a list of our own (or none,
//    or an empty one) whose strings are translated by every Load. Compared: al, the whole table (path
//    bytes, flag, count, capacity, every entry's two strings: text, capacity, references), the
//    registered strings and the loaded flag 0x6c2cc8 (restored afterwards).
//  - Add on tables built by hand (capacities 1-11, counts below the capacity) with 1-25 keys from a
//    pool with duplicates and an empty one; HasData; the comparison on pairs of texts; the pair's
//    constructors and destructor (states of the pair and the sources; not the destructor's eax).
//  - LocalString_Ctor on a registration list of our own (the game's list, its pointer and guard are
//    saved and restored): empty and other texts, lists from empty to full, the first use (guard bit
//    cleared, atexit recorded instead of registered).
//  - The static initialisers and destructors on the game's globals, saved and restored around each
//    call: the four lengths, the table's initialiser (atexit recorded), its destructor on test entries
//    and the registration list's destructor (eax only without data).
#include "check_util.h"
#include "field_slots.h"
#include "init_list.h"
#include "local_table.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);

namespace zax {

int hooks_local_table(Hook *out);

namespace {

// The real Local.txt: Zax-Reimpl-Local.txt next to Zax.exe (zax-testtools/tools/vmtest.py copies it from GERMAN.RED), 0 bytes
// without it (58,542 bytes in GERMAN.RED)
char g_real[128 * 1024];
DWORD g_real_size;

void read_real()
{
    char path[MAX_PATH + 32];
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    while (n && path[n - 1] != '\\')
        n--;
    lstrcpyA(path + n, "Zax-Reimpl-Local.txt");
    g_real_size = 0;
    HANDLE h = CreateFileA(path, 0x80000000 /* GENERIC_READ */, FILE_SHARE_READ, nullptr, 3 /* OPEN_EXISTING */,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD got = 0;
    // a file that does not fit (or a read error) counts as missing
    if (ReadFile(h, g_real, sizeof g_real, &got, nullptr) && got < sizeof g_real)
        g_real_size = got;
    CloseHandle(h);
}

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

void zs_ctor(ZString *z) { at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_ctor_text(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
void zs_dtor(ZString *z) { at<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
// Text into a string that is already constructed
void zs_set(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }

// ---- Value streams of the two sides --------------------------------------------------------------

int g_side;
unsigned g_hash[2];
int g_nvals[2];
constexpr int VALS = 4096;
DWORD g_vals[2][VALS];
void st(DWORD v)
{
    g_hash[g_side] = fnv(g_hash[g_side], &v, 4);
    if (g_nvals[g_side] < VALS)
        g_vals[g_side][g_nvals[g_side]] = v;
    g_nvals[g_side]++;
}
void st_bytes(const void *p, DWORD n) { st(fnv(2166136261u, p, n)); }
void st_string(const ZString *z)
{
    if (!z->buffer) {
        st(0xdead);
        return;
    }
    const StringState s = state(*z, nullptr);
    st(s.hash);
    st(s.length);
    st(s.capacity);
    // the shared empty buffer's count depends on every empty string of the game
    st(s.is_empty_buffer ? 0xe0e0 : s.refs);
    st(s.is_empty_buffer);
}
void log_first_difference()
{
    for (int i = 0; i < g_nvals[0] && i < g_nvals[1] && i < VALS; i++)
        if (g_vals[0][i] != g_vals[1][i]) {
            log("    value %d: %x / %x", i, g_vals[0][i], g_vals[1][i]);
            return;
        }
    if (g_nvals[0] != g_nvals[1])
        log("    values: %d / %d", g_nvals[0], g_nvals[1]);
}

// ---- Hooks of the module out and in --------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 32;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void set_site(BYTE *site, const BYTE *bytes)
{
    write_code(site, bytes);
}
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_local_table(nullptr);
    if (!hook_sites_fit("local_table", n, MAX_SITES))
        return;
    hooks_local_table(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        set_site(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        set_site(g_sites[i].site, g_sites[i].bytes);
    g_nsites = 0;
}

template <typename F> bool both_sides(unsigned seed, F run)
{
    for (int s = 0; s < 2; s++) {
        g_side = s;
        g_seed = seed;
        g_hash[s] = 2166136261u;
        g_nvals[s] = 0;
        if (!s)
            hooks_out();
        run();
        if (!s)
            hooks_in();
    }
    return g_hash[0] == g_hash[1] && g_nvals[0] == g_nvals[1];
}

// ---- Tables, entries, registered strings --------------------------------------------------------------

LocalEntry *make_entries(DWORD n)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(n * 8 + 4));
    block[0] = n;
    LocalEntry *e = reinterpret_cast<LocalEntry *>(block + 1);
    for (DWORD i = 0; i < n; i++) {
        zs_ctor(&e[i].key);
        zs_ctor(&e[i].text);
    }
    return e;
}
void free_entries(LocalEntry *e)
{
    if (!e)
        return;
    DWORD *block = reinterpret_cast<DWORD *>(e) - 1;
    for (DWORD i = 0; i < block[0]; i++) {
        zs_dtor(&e[i].text);
        zs_dtor(&e[i].key);
    }
    heap_free(block);
}
void describe_table(const ZLocalTable *t)
{
    st_bytes(t->path, sizeof t->path);
    st(t->from_archive);
    st(t->count);
    st(t->capacity);
    st(t->entries != nullptr);
    if (!t->entries)
        return;
    const DWORD n = reinterpret_cast<const DWORD *>(t->entries)[-1];
    st(n);
    for (DWORD i = 0; i < n && i < 1000; i++) {
        st_string(&t->entries[i].key);
        st_string(&t->entries[i].text);
    }
}

using LoadFn = DWORD(THISCALL *)(ZLocalTable *, const char *, DWORD);
using AddFn = DWORD(THISCALL *)(ZLocalTable *, const ZString *, const ZString *);

// The registration list of the test (the game's pointer 0x6c2cc4 points to it during a case)
ZLocalPendingList g_plist;
LocalPending g_pdata[16];
ZString g_pstr[16];
int g_npending;
const char CTX_INVENTORY[] = "Lower Inventory Window", CTX_EMPTY[] = "", CTX_OTHER[] = "ctx", CTX_T[] = "Stands for Transparent";
struct PendingText {
    const char *text, *context;
};
const PendingText PENDING[] = {
    {"Close", nullptr}, {"Hide", CTX_INVENTORY}, {"Hide", nullptr}, {"Apple", CTX_EMPTY}, {"", nullptr},
    {"Unknown text", CTX_OTHER}, {"line one\nline two", nullptr}, {"T", CTX_T}, {"Zebra", nullptr},
    {"a[END]b[END]", nullptr}, {"Inventory", nullptr}, {"OK", nullptr},
};
// variant 0: no list, 1: an empty one, 2: all texts
void setup_pending(int variant)
{
    g_npending = variant == 2 ? (int)(sizeof PENDING / sizeof PENDING[0]) : 0;
    for (int i = 0; i < g_npending; i++) {
        zs_ctor_text(&g_pstr[i], PENDING[i].text);
        g_pdata[i] = {&g_pstr[i], PENDING[i].context};
    }
    g_plist = {0x5f99e0, g_pdata, (DWORD)g_npending, 16};
    *at<ZLocalPendingList **>(LOCAL_PENDING_PTR) = variant ? &g_plist : nullptr;
}
void describe_pending()
{
    for (int i = 0; i < g_npending; i++)
        st_string(&g_pstr[i]);
}
void teardown_pending()
{
    for (int i = 0; i < g_npending; i++)
        zs_dtor(&g_pstr[i]);
    g_npending = 0;
}

// ---- Test files ---------------------------------------------------------------------------------------

char g_dir[MAX_PATH];
enum {
    F_EMPTY, F_ONE, F_MIXED, F_MULTI, F_CONTEXT, F_LF, F_LONG, F_GEN9, F_GEN10, F_GEN11, F_GEN19, F_GEN20, F_GEN21, F_GEN47,
    F_REAL, F_COUNT,
    // names that are not our files
    F_MISSING = 100, F_REL_LOCAL, F_REL_RULES, F_REL_MISSING,
};
bool g_have_file[F_COUNT];

const char TEXT_ONE[] = "ENGLISH: Close[END]\r\nLOCALIZED: Schliessen[END]\r\n";
const char TEXT_MIXED[] =
    "; a comment\r\n"
    "\r\n"
    "ENGLISH: Zebra[END]\r\nLOCALIZED: Zebra-DE[END]\r\n"
    "english: lower[END]\r\nLOCALIZED: never[END]\r\n"
    "ENGLISH:nospace[END]\r\nLOCALIZED: x[END]\r\n"
    "ENGLISH: Apple[END]\r\nLOCALIZED: Apfel[END]\r\n"
    "ENGLISH: Apple[END]\r\nLOCALIZED: Apfel zwei[END]\r\n"
    "ENGLISH: Mango[END]\r\nTRANSLATED: Mango-DE[END]\r\n"
    "ENGLISH: Kiwi[END]\r\nLOCALIZED:  two spaces[END]\r\n"
    "  ENGLISH: indented[END]\r\nLOCALIZED: no[END]\r\n"
    "ENGLISH: Close[END]\r\nLOCALIZED: Zu[END]\r\n"
    "LOCALIZED: alone[END]\r\n"
    "ENGLISH: Inventory[END]\r\nLOCALIZED: Inventar[END]";
const char TEXT_MULTI[] =
    "ENGLISH: line one\r\nline two[END]\r\nLOCALIZED: Zeile eins\r\nZeile zwei\r\nZeile drei[END]\r\n"
    "ENGLISH: \r\n\r\nafter blanks[END]\r\nLOCALIZED: nach Leerzeilen[END]\r\n"
    "ENGLISH: single[END]\r\nLOCALIZED: \r\nsecond line[END]\r\n"
    "ENGLISH: OK[END]\r\nLOCALIZED: In Ordnung\r\n\r\n[END]\r\n";
const char TEXT_CONTEXT[] =
    "ENGLISH: Hide[END] (Lower Inventory Window)\r\nLOCALIZED: Ausblenden[END]\r\n"
    "ENGLISH: Hide[END]\r\nLOCALIZED: Verstecken[END]\r\n"
    "ENGLISH: a[END]b[END]\r\nLOCALIZED: c[END]d[END]\r\n"
    "ENGLISH: [END]\r\nLOCALIZED: leer[END]\r\n"
    "ENGLISH: empty text[END]\r\nLOCALIZED: [END]\r\n"
    "ENGLISH: T[END] (Stands for Transparent)\r\nLOCALIZED: T-DE[END]\r\n"
    "ENGLISH: [END][END]\r\nLOCALIZED: zwei[END]\r\n"
    "ENGLISH: Apple[END] ()\r\nLOCALIZED: Apfel leer[END]\r\n";
const char TEXT_LF[] = "ENGLISH: lf one[END]\nLOCALIZED: LF eins[END]\nENGLISH: last[END]\nLOCALIZED: letzte[END]";

void file_path(char *out, int f)
{
    switch (f) {
    case F_MISSING:
        wsprintfA(out, "%smissing.txt", g_dir);
        return;
    case F_REL_LOCAL:
        lstrcpyA(out, "Local.txt");
        return;
    case F_REL_RULES:
        lstrcpyA(out, "Rules.txt");
        return;
    case F_REL_MISSING:
        lstrcpyA(out, "Missing/Nothing.txt");
        return;
    }
    wsprintfA(out, "%slocal%d.txt", g_dir, f);
}
void write_file(int f, const void *p, DWORD n)
{
    char path[MAX_PATH];
    file_path(path, f);
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD w = 0;
    WriteFile(h, p, n, &w, nullptr);
    CloseHandle(h);
    g_have_file[f] = w == n;
}
void write_text(int f, const char *s) { write_file(f, s, (DWORD)lstrlenA(s)); }

char g_gen[64 * 1024];
void write_generated(int f, DWORD pairs, unsigned seed)
{
    g_seed = seed;
    char *p = g_gen;
    for (DWORD i = 0; i < pairs; i++)
        p += wsprintfA(p, "ENGLISH: Key %08x[END]\r\nLOCALIZED: Text %u of %u[END]\r\n\r\n", random(), i, pairs);
    write_file(f, g_gen, (DWORD)(p - g_gen));
}

bool build_files()
{
    char tmp[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_dir, "%sZaxLocalTable\\", tmp);
    if (lstrlenA(g_dir) > 0xc0)
        return false;
    CreateDirectoryA(g_dir, nullptr);
    char path[MAX_PATH];
    file_path(path, F_MISSING);
    DeleteFileA(path);
    write_text(F_EMPTY, "");
    write_text(F_ONE, TEXT_ONE);
    write_text(F_MIXED, TEXT_MIXED);
    write_text(F_MULTI, TEXT_MULTI);
    write_text(F_CONTEXT, TEXT_CONTEXT);
    write_text(F_LF, TEXT_LF);
    // A line the reader cannot take (0x400 characters) between pairs ends the file there
    char *p = g_gen;
    p += wsprintfA(p, "%s", TEXT_ONE);
    for (int i = 0; i < 0x400; i++)
        *p++ = (char)('a' + i % 26);
    p += wsprintfA(p, "\r\nENGLISH: after[END]\r\nLOCALIZED: danach[END]\r\n");
    write_file(F_LONG, g_gen, (DWORD)(p - g_gen));
    const DWORD gens[] = {9, 10, 11, 19, 20, 21, 47};
    for (int i = 0; i < 7; i++)
        write_generated(F_GEN9 + i, gens[i], 0x500ac0u + (unsigned)i);
    read_real();
    if (g_real_size)
        write_file(F_REAL, g_real, g_real_size);
    return true;
}

// ---- Load ---------------------------------------------------------------------------------------------

ZLocalTable g_table;

// A sequence of Loads on one table; prefill: old entries (3 entries, count 2) before the first
void load_scenario(const int *files, int nfiles, int pending, bool prefill)
{
    memset(&g_table, 0, sizeof g_table);
    if (prefill) {
        g_table.entries = make_entries(3);
        zs_set(&g_table.entries[0].key, "Close");
        zs_set(&g_table.entries[0].text, "Old close");
        zs_set(&g_table.entries[1].key, "Hide[END] (Lower Inventory Window)");
        zs_set(&g_table.entries[1].text, "Old hide");
        g_table.count = 2;
        g_table.capacity = 3;
    }
    setup_pending(pending);
    for (int i = 0; i < nfiles; i++) {
        char path[MAX_PATH];
        file_path(path, files[i]);
        *at<BYTE *>(LOCAL_LOADED) = 0x5a;
        const DWORD r = at<LoadFn>(0x500ac0)(&g_table, path, 0x1234);
        st(r & 0xff);
        describe_table(&g_table);
        describe_pending();
        st(*at<BYTE *>(LOCAL_LOADED));
    }
    free_entries(g_table.entries);
    teardown_pending();
}

// ---- Add, HasData, Compare, the pair ----------------------------------------------------------------

const char *const KEYS[] = {"Close", "Hide", "Apple", "apple", "", "Zebra", "Key 1", "Key 10", "Key 2", "a[END]b",
                            "Hide[END] (Lower Inventory Window)", "Ä umlaut", "z", "~", " space", "Close"};
constexpr int NKEYS = sizeof KEYS / sizeof KEYS[0];

void add_scenario(DWORD capacity, DWORD count, int adds)
{
    memset(&g_table, 0, sizeof g_table);
    g_table.entries = make_entries(capacity);
    g_table.capacity = capacity;
    g_table.count = count;
    for (DWORD i = 0; i < count; i++) {
        zs_set(&g_table.entries[i].key, KEYS[random() % NKEYS]);
        zs_set(&g_table.entries[i].text, KEYS[random() % NKEYS]);
    }
    for (int k = 0; k < adds; k++) {
        ZString key, text;
        zs_ctor_text(&key, KEYS[random() % NKEYS]);
        char t[32];
        wsprintfA(t, "text %d", k);
        zs_ctor_text(&text, t);
        at<AddFn>(0x500900)(&g_table, &key, &text);
        st_string(&key);
        st_string(&text);
        describe_table(&g_table);
        st(at<DWORD(THISCALL *)(ZLocalTable *)>(0x500ab0)(&g_table));
        zs_dtor(&text);
        zs_dtor(&key);
    }
    free_entries(g_table.entries);
    g_table.entries = nullptr;
    st(at<DWORD(THISCALL *)(ZLocalTable *)>(0x500ab0)(&g_table));
}

void entry_scenario()
{
    // Compare on all pairs of keys
    LocalEntry e[NKEYS];
    for (int i = 0; i < NKEYS; i++) {
        zs_ctor_text(&e[i].key, KEYS[i]);
        zs_ctor(&e[i].text);
    }
    using CompareFn = int(__cdecl *)(const LocalEntry *, const LocalEntry *);
    for (int i = 0; i < NKEYS; i++)
        for (int j = 0; j < NKEYS; j++)
            st((DWORD)at<CompareFn>(0x5008f0)(&e[i], &e[j]));
    // The constructors and the destructor
    for (int i = 0; i < NKEYS; i++) {
        LocalEntry p;
        memset(&p, 0xcc, sizeof p);
        st(ptrv(at<LocalEntry *(THISCALL *)(LocalEntry *)>(0x500740)(&p)) - ptrv(&p));
        st_string(&p.key);
        st_string(&p.text);
        at<DWORD(THISCALL *)(LocalEntry *)>(0x4270f0)(&p);
        memset(&p, 0xcc, sizeof p);
        const int j = (i * 7 + 3) % NKEYS;
        st(ptrv(at<LocalEntry *(THISCALL *)(LocalEntry *, const ZString *, const ZString *)>(0x500760)(&p, &e[i].key, &e[j].key)) - ptrv(&p));
        st_string(&p.key);
        st_string(&p.text);
        st_string(&e[i].key);
        st_string(&e[j].key);
        at<DWORD(THISCALL *)(LocalEntry *)>(0x4270f0)(&p);
        st_string(&e[i].key);
        st_string(&e[j].key);
    }
    for (int i = 0; i < NKEYS; i++) {
        zs_dtor(&e[i].text);
        zs_dtor(&e[i].key);
    }
}

// ---- Registered strings -------------------------------------------------------------------------------

ZString g_reg[24];
void register_scenario(DWORD count, DWORD capacity, int nreg, bool first_use)
{
    ZLocalPendingList *l = at<ZLocalPendingList *>(LOCAL_PENDING);
    BYTE &guard = *at<BYTE *>(LOCAL_PENDING_GUARD);
    if (first_use)
        count = capacity = 0;   // the first use clears the list without freeing it
    l->vftable = 0x5f99e0;
    l->count = count;
    l->capacity = capacity;
    l->data = capacity ? static_cast<LocalPending *>(heap_alloc(capacity * 8)) : nullptr;
    for (DWORD i = 0; i < count; i++)
        l->data[i] = {reinterpret_cast<ZString *>(0x1000 + i), reinterpret_cast<const char *>(0x2000 + i)};
    *at<ZLocalPendingList **>(LOCAL_PENDING_PTR) = nullptr;
    CodePatch exit_patch;
    if (first_use) {
        guard &= ~1;
        l->vftable = 0x1234;
        g_atexit_calls = 0;
        g_atexit_function = 0;
        patch_jump(exit_patch, 0x5d0863, (void *)&fake_atexit);
    } else {
        guard |= 1;
    }
    for (int k = 0; k < nreg; k++) {
        const int i = (int)(random() % NKEYS);
        const char *context = k % 3 == 0 ? nullptr : KEYS[(i + k) % NKEYS];
        ZString *r = at<ZString *(THISCALL *)(ZString *, const char *, const char *)>(0x501030)(&g_reg[k], KEYS[i], context);
        st(ptrv(r) - ptrv(&g_reg[k]));
        st_string(&g_reg[k]);
        st(l->vftable);
        st(l->count);
        st(l->capacity);
        st(l->data != nullptr);
        st(*at<DWORD *>(LOCAL_PENDING_PTR) == ptrv(l));
        st(guard & 1);
        for (DWORD j = 0; j < l->count && l->data; j++) {
            st(ptrv(l->data[j].text));
            st(ptrv(l->data[j].context));
        }
    }
    if (first_use) {
        unpatch(exit_patch);
        st(g_atexit_calls);
        st(g_atexit_function);
    }
    for (int k = 0; k < nreg; k++)
        zs_dtor(&g_reg[k]);
    if (l->data)
        heap_free(l->data);
}

// ---- Static initialisers and destructors -------------------------------------------------------------

void statics_scenario(int k)
{
    // The lengths
    const DWORD lens[] = {LOCAL_LEN_ENGLISH, LOCAL_LEN_LOCALIZED, LOCAL_LEN_END, LOCAL_LEN_CONTEXT};
    const DWORD fns[] = {0x5007e0, 0x500800, 0x500820, 0x500840};
    for (int i = 0; i < 4; i++) {
        DWORD &v = *at<DWORD *>(lens[i]);
        const DWORD saved = v;
        v = random();
        st(at<DWORD(__cdecl *)()>(fns[i])());
        st(v);
        v = saved;
    }
    ZLocalTable *t = at<ZLocalTable *>(LOCAL_TABLE);
    BYTE saved[sizeof(ZLocalTable)];
    memcpy(saved, t, sizeof saved);
    // The table's initialiser with atexit recorded
    {
        CodePatch exit_patch;
        g_atexit_calls = 0;
        g_atexit_function = 0;
        t->entries = reinterpret_cast<LocalEntry *>(0x12345678);
        t->count = 77;
        patch_jump(exit_patch, 0x5d0863, (void *)&fake_atexit);
        st(at<DWORD(__cdecl *)()>(0x500870)());
        unpatch(exit_patch);
        st(ptrv(t->entries));
        st(t->count);
        st(g_atexit_calls);
        st(g_atexit_function);
    }
    // The table's destructor on test entries (none, 1-12) with a count of its own
    {
        const DWORD n = (DWORD)k % 13;
        t->entries = n ? make_entries(n) : nullptr;
        for (DWORD i = 0; i < n; i++) {
            zs_set(&t->entries[i].key, KEYS[(i + (DWORD)k) % NKEYS]);
            zs_set(&t->entries[i].text, KEYS[(i * 3) % NKEYS]);
        }
        t->count = random() % 20;
        t->capacity = 31;
        st(at<DWORD(__cdecl *)()>(0x500890)());
        st(ptrv(t->entries));
        st(t->count);
        st(t->capacity);
    }
    memcpy(t, saved, sizeof saved);
    // The registration list's destructor (operator delete's eax with data, so only without)
    {
        ZLocalPendingList *l = at<ZLocalPendingList *>(LOCAL_PENDING);
        const ZLocalPendingList saved_list = *l;
        const bool with_data = k & 1;
        *l = {0x4444, with_data ? static_cast<LocalPending *>(heap_alloc(64)) : nullptr, random() % 9, random() % 9};
        const DWORD r = at<DWORD(__cdecl *)()>(0x500a70)();
        if (!with_data)
            st(r);
        st(l->vftable);
        st(ptrv(l->data));
        st(l->count);
        st(l->capacity);
        *l = saved_list;
    }
}

// Runs a check with the game's registration pointer, loaded flag, list and guard saved and restored
struct SavedGlobals {
    DWORD pending_ptr;
    BYTE loaded, guard;
    ZLocalPendingList list;
    void save()
    {
        pending_ptr = *at<DWORD *>(LOCAL_PENDING_PTR);
        loaded = *at<BYTE *>(LOCAL_LOADED);
        guard = *at<BYTE *>(LOCAL_PENDING_GUARD);
        list = *at<ZLocalPendingList *>(LOCAL_PENDING);
    }
    void restore() const
    {
        *at<DWORD *>(LOCAL_PENDING_PTR) = pending_ptr;
        *at<BYTE *>(LOCAL_LOADED) = loaded;
        *at<BYTE *>(LOCAL_PENDING_GUARD) = guard;
        *at<ZLocalPendingList *>(LOCAL_PENDING) = list;
    }
};

void report_case(Tally &t, bool same, const char *what)
{
    t.check(same, "", what);
    if (!same && t.mismatches <= 3)
        log_first_difference();
}

}  // namespace

ZAX_CHECKS(local_table)
{
    (void)cls;
    (void)n;
    SavedGlobals g;
    g.save();
    {
        Tally t{"Local.txt table: Load on test files and the real Local.txt"};
        if (!build_files()) {
            log("  Local.txt table: no temp folder, Load not checked");
        } else {
            if (!g_real_size)
                log("  Local.txt table: no Zax-Reimpl-Local.txt next to Zax.exe (zax-testtools/tools/vmtest.py sends it from GERMAN.RED), "
                    "the real Local.txt is not checked");
            struct Seq {
                int files[4];
                int n;
            };
            const Seq seqs[] = {
                {{F_EMPTY}, 1}, {{F_ONE}, 1}, {{F_MIXED}, 1}, {{F_MULTI}, 1}, {{F_CONTEXT}, 1}, {{F_LF}, 1}, {{F_LONG}, 1},
                {{F_GEN9}, 1}, {{F_GEN10}, 1}, {{F_GEN11}, 1}, {{F_GEN19}, 1}, {{F_GEN20}, 1}, {{F_GEN21}, 1}, {{F_GEN47}, 1},
                {{F_MISSING}, 1}, {{F_REL_LOCAL}, 1}, {{F_REL_RULES}, 1}, {{F_REL_MISSING}, 1},
                {{F_MIXED, F_CONTEXT}, 2}, {{F_GEN21, F_MISSING, F_ONE}, 3}, {{F_CONTEXT, F_MULTI, F_GEN47, F_EMPTY}, 4},
                {{F_ONE, F_ONE, F_REL_MISSING, F_MIXED}, 4}, {{F_REAL}, 1}, {{F_REAL, F_MISSING}, 2}, {{F_CONTEXT, F_REAL, F_ONE}, 3},
            };
            int k = 0;
            for (const Seq &s : seqs) {
                bool usable = true;
                for (int i = 0; i < s.n; i++)
                    usable = usable && (s.files[i] >= F_COUNT || g_have_file[s.files[i]]);
                const int seq_index = (int)(&s - seqs);
                bool real = false;
                for (int i = 0; i < s.n; i++)
                    real = real || s.files[i] == F_REAL;
                if (!usable)
                    continue;
                for (int pending = 0; pending < 3; pending++)
                    for (int prefill = 0; prefill < 2; prefill++) {
                        // the real file (687 sorts of up to 690 entries per Load) once per sequence
                        if (real && (pending != 2 - (seq_index & 1) || prefill != (seq_index & 1)))
                            continue;
                        const bool same = both_sides(0x500ac0u + (unsigned)k++, [&] { load_scenario(s.files, s.n, pending, prefill); });
                        char d[96];
                        wsprintfA(d, "[load %d %d %d %d (%d files), pending %d, prefill %d]", s.files[0], s.files[1], s.files[2],
                                  s.files[3], s.n, pending, prefill);
                        report_case(t, same, d);
                    }
            }
        }
        g.restore();
        t.report();
    }
    {
        Tally t{"Local.txt table: Add, HasData, Compare, the entry pair"};
        int k = 0;
        for (DWORD capacity = 1; capacity <= 11; capacity++)
            for (DWORD count = 0; count < capacity; count += capacity > 5 ? 2 : 1)
                for (int adds : {1, 2, 9, 25}) {
                    const bool same = both_sides(0x500900u + (unsigned)k++, [&] { add_scenario(capacity, count, adds); });
                    char d[80];
                    wsprintfA(d, "[add: capacity %u, count %u, %d adds]", capacity, count, adds);
                    report_case(t, same, d);
                }
        report_case(t, both_sides(0x5008f0u, [] { entry_scenario(); }), "[compare, pair]");
        t.report();
    }
    {
        Tally t{"Local.txt table: registered strings (0x501030)"};
        int k = 0;
        for (DWORD capacity : {0u, 1u, 4u, 10u})
            for (DWORD count = 0; count <= capacity; count += capacity > 4 ? 5 : 1)
                for (int nreg : {1, 3, 24})
                    for (int first = 0; first < 2; first++) {
                        const bool same = both_sides(0x501030u + (unsigned)k++, [&] { register_scenario(count, capacity, nreg, first); });
                        char d[96];
                        wsprintfA(d, "[register: count %u, capacity %u, %d strings, first use %d]", count, capacity, nreg, first);
                        report_case(t, same, d);
                        g.restore();
                    }
        t.report();
    }
    {
        Tally t{"Local.txt table: static initialisers and destructors"};
        for (int k = 0; k < 26; k++) {
            const bool same = both_sides(0x500870u + (unsigned)k, [k] { statics_scenario(k); });
            char d[48];
            wsprintfA(d, "[statics %d]", k);
            report_case(t, same, d);
        }
        g.restore();
        t.report();
    }
    g.restore();
}

}  // namespace zax
