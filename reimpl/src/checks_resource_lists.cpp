// Checks of the resource list registry and the file name list (resource_lists.cpp) against the
// original, in the game thread. Each case runs twice (side 0 original, side 1 reimplementation).
//
// Registry: instances of our own with entries pointing to fake resource lists whose slots (and the
// exe's 0x523d00/0x523d40/0x523d70 on them) record their calls in an event log; the global instance
// 0x6be088 is saved, changed for the test (first use with atexit redirected, fake entries for Ask,
// AtExit) and restored. The first-Add callback registration (0x419ea0) jumps to a stand-in.
// File name list: Add/Contains/GetName on lists of our own, Search on the game's folders.
#include "check_util.h"
#include "init_list.h"
#include "resource_lists.h"

WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }

int g_cur;
constexpr int LOG = 512;
DWORD g_log[2][LOG];
int g_nlog[2];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    int &n = g_nlog[g_cur];
    if (n + 4 <= LOG) {
        DWORD *l = &g_log[g_cur][n];
        l[0] = a;
        l[1] = b;
        l[2] = c;
        l[3] = d;
    }
    n += 4;
}
DWORD result(DWORD low) { return ((0x5e700000u + (DWORD)g_nlog[g_cur] * 0x100u) & 0xffffff00) | (low & 0xff); }
bool same_log()
{
    if (g_nlog[0] != g_nlog[1])
        return false;
    for (int i = 0; i < g_nlog[0] && i < LOG; i++)
        if (g_log[0][i] != g_log[1][i])
            return false;
    return true;
}
void begin(int v)
{
    g_cur = v;
    g_nlog[v] = 0;
}
void check(Tally &t, bool same, const char *what, DWORD r0, DWORD r1)
{
    char d[200];
    wsprintfA(d, "[%.120s: %x / %x, log %d / %d]", what, r0, r1, g_nlog[0], g_nlog[1]);
    t.check(same, "lists", d);
}

// ---- Fake resource lists ---------------------------------------------------------------------------

struct FakeList {
    DWORD vftable;
    DWORD id, count, bits;
};
FakeList g_lists[8];
// The next bit of the list's pattern (rotating)
DWORD next_bit(FakeList *l)
{
    DWORD b = l->bits & 1;
    l->bits = (l->bits >> 1) | (b << 15);
    return b;
}
DWORD THISCALL f_bad(FakeList *l) { ev(0x5ff, l->id); return result(0x5f); }
DWORD THISCALL f_item(FakeList *l, DWORD i) { DWORD b = next_bit(l); ev(0x508, l->id, i, b); return result(b); }
DWORD THISCALL f_count(FakeList *l) { ev(0x516, l->id); return l->count; }
DWORD THISCALL f_28(FakeList *l) { ev(0x51c, l->id); return result(0x1c); }
DWORD THISCALL f_29(FakeList *l, DWORD i) { DWORD b = next_bit(l); ev(0x51d, l->id, i, b); return result(b); }
DWORD THISCALL f_31(FakeList *l, DWORD i) { ev(0x51f, l->id, i); return result(0x1f); }
DWORD THISCALL f_32(FakeList *l, DWORD i) { DWORD b = next_bit(l); ev(0x520, l->id, i, b); return result(b); }
DWORD THISCALL f_33(FakeList *l, DWORD i, DWORD x) { ev(0x521, l->id, i, x); return result(0x21); }
DWORD THISCALL f_35(FakeList *l, DWORD a, DWORD b) { DWORD r = next_bit(l); ev(0x523, l->id, a, b * 16 + r); return result(r); }
void *g_vtable[40];

void init_lists(unsigned seed)
{
    for (int i = 0; i < 40; i++)
        g_vtable[i] = (void *)&f_bad;
    g_vtable[8] = (void *)&f_item;
    g_vtable[22] = (void *)&f_count;
    g_vtable[28] = (void *)&f_28;
    g_vtable[29] = (void *)&f_29;
    g_vtable[31] = (void *)&f_31;
    g_vtable[32] = (void *)&f_32;
    g_vtable[33] = (void *)&f_33;
    g_vtable[35] = (void *)&f_35;
    g_seed = seed;
    for (DWORD i = 0; i < 8; i++)
        g_lists[i] = {(DWORD)(uintptr_t)g_vtable, 0x700 + i, random() % 4, random() >> 9};
}
DWORD list_id(const void *p)
{
    for (FakeList &l : g_lists)
        if (p == &l)
            return l.id;
    return (DWORD)(uintptr_t)p;
}

const char *const NAMES[] = {"Sounds", "Models", "Fonts", "Scripts", "Textures", "Music", "Overlays", "Cursors"};

// A registry of our own with n entries in a block of the given capacity (operator new)
void fill_registry(ZResourceListRegistry *r, DWORD n, DWORD capacity, unsigned seed)
{
    g_seed = seed;
    memset(r, 0, sizeof *r);
    r->vftable = RESOURCE_LISTS_VTABLE;
    r->array_vftable = RESOURCE_LISTS_ARRAY_VTABLE;
    r->count = n;
    r->capacity = capacity;
    r->marked = (BYTE)(random() & 1);
    r->data = capacity ? (ZResourceListEntry *)heap_alloc(capacity * 16) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        ZResourceListEntry &e = r->data[i];
        e.name = NAMES[random() % 8];
        e.list = &g_lists[random() % 8];
        e.value = random();
        e.flag = (BYTE)(random() % 3 == 0 ? 0 : random());
        e.pad[0] = e.pad[1] = e.pad[2] = 0x5a;
    }
}
// Entries without the 3 bytes behind the flag (never written by Add)
DWORD registry_hash(const ZResourceListRegistry *r)
{
    unsigned h = fnv(2166136261u, &r->vftable, 8);
    h = fnv(h, &r->count, 8);
    h = fnv(h, &r->marked, 1);
    for (DWORD i = 0; i < r->count; i++) {
        const ZResourceListEntry &e = r->data[i];
        DWORD x[4] = {(DWORD)(uintptr_t)e.name, list_id(e.list), e.value, e.flag};
        h = fnv(h, x, 16);
    }
    return h;
}

DWORD g_callbacks, g_callback_fn;
DWORD __fastcall fake_add_callback(DWORD fn)
{
    g_callbacks++;
    g_callback_fn = fn;
    return 0;
}

void test_registry(Tally &t)
{
    ZResourceListRegistry reg[2];
    BYTE &first = *at<BYTE *>(RESOURCE_LISTS_FIRST_ADD);
    BYTE saved_first = first;
    CodePatch cb;
    patch_jump(cb, 0x419ea0, (void *)&fake_add_callback);
    unsigned seed = 1;
    // Add: counts 0-20, capacities count and count + 3, the first-Add flag set and clear
    for (DWORD n = 0; n <= 20; n++)
        for (DWORD extra : {0u, 3u})
            for (BYTE flag_first = 0; flag_first < 2; flag_first++) {
                DWORD r[2], h[2], grown[2], calls[2], fn[2], fl[2];
                for (int v = 0; v < 2; v++) {
                    begin(v);
                    init_lists(seed);
                    fill_registry(&reg[v], n, n + extra, seed);
                    void *old = reg[v].data;
                    first = flag_first;
                    g_callbacks = g_callback_fn = 0;
                    r[v] = pick<DWORD(THISCALL *)(void *, const char *, void *, DWORD, BYTE)>(v, 0x4a34d0)(&reg[v], NAMES[n % 8], &g_lists[n % 8], n * 77,
                                                                                                         (BYTE)(n * 13));
                    h[v] = registry_hash(&reg[v]);
                    grown[v] = reg[v].data != old;
                    calls[v] = g_callbacks;
                    fn[v] = g_callback_fn;
                    fl[v] = first;
                    if (reg[v].data)
                        heap_free(reg[v].data);
                }
                char what[80];
                wsprintfA(what, "Registry Add n %u capacity %u first %d", n, n + extra, flag_first);
                check(t, r[0] == r[1] && h[0] == h[1] && grown[0] == grown[1] && calls[0] == calls[1] && fn[0] == fn[1] && fl[0] == fl[1], what,
                      h[0], h[1]);
                seed++;
            }
    first = saved_first;
    unpatch(cb);
    // The methods over all entries, Name, SetFlag
    for (DWORD n = 0; n <= 9; n++)
        for (int k = 0; k < 3; k++) {
            seed++;
            const struct {
                DWORD fn;
                const char *what;
            } ops[] = {{0x4a3730, "CallAll28"}, {0x4a3760, "CallFlagged"}, {0x4a37a0, "CallAllReverse"}, {0x4a37d0, "MarkAll"}};
            for (const auto &op : ops) {
                DWORD r[2], h[2];
                for (int v = 0; v < 2; v++) {
                    begin(v);
                    init_lists(seed);
                    fill_registry(&reg[v], n, n + 2, seed);
                    r[v] = pick<DWORD(THISCALL *)(void *)>(v, op.fn)(&reg[v]);
                    h[v] = registry_hash(&reg[v]);
                    heap_free(reg[v].data);
                }
                char what[80];
                wsprintfA(what, "Registry %s n %u", op.what, n);
                check(t, r[0] == r[1] && h[0] == h[1] && same_log(), what, r[0], r[1]);
            }
            for (DWORD i = 0; i <= n + 1; i++) {
                DWORD r[2], h[2];
                for (int v = 0; v < 2; v++) {
                    begin(v);
                    init_lists(seed);
                    fill_registry(&reg[v], n, n, seed);
                    r[v] = (DWORD)(uintptr_t)pick<const char *(THISCALL *)(void *, DWORD)>(v, 0x4a3670)(&reg[v], i);
                    h[v] = registry_hash(&reg[v]);
                    DWORD s = pick<DWORD(THISCALL *)(void *, void *, BYTE)>(v, 0x4a3800)(&reg[v], &g_lists[(i + (DWORD)k) % 8], (BYTE)(i + 1));
                    h[v] = fnv(fnv(h[v], &s, 4), &r[v], 4);
                    h[v] = fnv(h[v], &reg[v].data, 0) ^ registry_hash(&reg[v]);
                    if (reg[v].data)
                        heap_free(reg[v].data);
                }
                char what[80];
                wsprintfA(what, "Registry Name/SetFlag n %u i %u", n, i);
                check(t, r[0] == r[1] && h[0] == h[1], what, r[0], r[1]);
            }
        }
    // Deleting destructor: flags 0 (state afterwards) and 1 (a pool block)
    for (DWORD n : {0u, 1u, 5u})
        for (DWORD flags : {0u, 1u, 2u}) {
            DWORD r[2], h[2];
            for (int v = 0; v < 2; v++) {
                begin(v);
                init_lists(seed);
                ZResourceListRegistry *p = flags & 1 ? (ZResourceListRegistry *)pool_alloc(sizeof *p) : &reg[v];
                fill_registry(p, n, n + 1, seed);
                r[v] = (DWORD)(uintptr_t)pick<void *(THISCALL *)(void *, DWORD)>(v, 0x4a3480)(p, flags) - (DWORD)(uintptr_t)p;
                h[v] = flags & 1 ? 0 : fnv(2166136261u, p, sizeof *p) ^ (DWORD)(uintptr_t)p->data;
            }
            char what[80];
            wsprintfA(what, "Registry DeletingDtor n %u flags %u", n, flags);
            check(t, r[0] == r[1] && h[0] == h[1] && r[0] == 0, what, h[0], h[1]);
        }
}

// The global instance: first use, AtExit and Ask on a changed copy, restored afterwards
void test_global(Tally &t)
{
    SavedBytes saved;
    save_bytes(saved, RESOURCE_LISTS_GUARD, 0x20);
    ZResourceListRegistry *g = at<ZResourceListRegistry *>(RESOURCE_LISTS);
    CodePatch exit_patch;
    patch_jump(exit_patch, 0x5d0863, (void *)&fake_atexit);
    // First use (guard cleared, garbage in the object) and later use
    for (int used = 0; used < 2; used++) {
        DWORD r[2], h[2], fn[2], calls[2];
        for (int v = 0; v < 2; v++) {
            begin(v);
            memset(at<void *>(RESOURCE_LISTS), 0x6b, sizeof(ZResourceListRegistry));
            *at<BYTE *>(RESOURCE_LISTS_GUARD) = 0x6b;
            if (used)
                *at<BYTE *>(RESOURCE_LISTS_GUARD) |= 1;
            else
                *at<BYTE *>(RESOURCE_LISTS_GUARD) &= ~1;
            g_atexit_calls = g_atexit_function = 0;
            r[v] = (DWORD)(uintptr_t)pick<void *(__cdecl *)()>(v, 0x4a35d0)();
            h[v] = fnv(2166136261u, at<void *>(RESOURCE_LISTS_GUARD), 0x20);
            fn[v] = g_atexit_function;
            calls[v] = g_atexit_calls;
        }
        check(t, r[0] == r[1] && r[0] == RESOURCE_LISTS && h[0] == h[1] && fn[0] == fn[1] && calls[0] == calls[1], used ? "Get (built)" : "Get (first use)",
              h[0], h[1]);
    }
    // AtExit with and without data
    for (DWORD n : {0u, 3u}) {
        DWORD r[2], h[2];
        for (int v = 0; v < 2; v++) {
            begin(v);
            init_lists(4);
            fill_registry(g, n, n, 4);
            r[v] = pick<DWORD(__cdecl *)()>(v, 0x4a3630)();
            h[v] = fnv(2166136261u, g, sizeof *g);
        }
        // With data the result is operator delete's eax
        check(t, (n || r[0] == r[1]) && h[0] == h[1], n ? "AtExit (data)" : "AtExit (none)", r[0], r[1]);
    }
    // Ask on 0-6 fake entries, also when the instance is built by Ask itself
    for (DWORD n = 0; n <= 6; n++)
        for (int k = 0; k < 4; k++) {
            DWORD r[2], h[2], fn[2];
            for (int v = 0; v < 2; v++) {
                begin(v);
                init_lists(100 + n * 4 + (DWORD)k);
                fill_registry(g, n, n, 100 + n * 4 + (DWORD)k);
                *at<BYTE *>(RESOURCE_LISTS_GUARD) |= 1;
                if (k == 3) {   // not yet built: Ask builds an empty instance (the entries leak, as they would)
                    *at<BYTE *>(RESOURCE_LISTS_GUARD) &= ~1;
                }
                ZResourceListEntry *data = g->data;
                g_atexit_function = 0;
                r[v] = pick<DWORD(__fastcall *)(DWORD, DWORD)>(v, 0x4a3690)(0x1234 + (DWORD)k, 0x55aa00 + n);
                h[v] = registry_hash(g);
                fn[v] = g_atexit_function;
                if (data)
                    heap_free(data);
                g->data = nullptr;
            }
            char what[80];
            wsprintfA(what, "Ask n %u case %d", n, k);
            check(t, r[0] == r[1] && h[0] == h[1] && fn[0] == fn[1] && same_log(), what, r[0], r[1]);
        }
    unpatch(exit_patch);
    restore_bytes(saved);
}

// ---- File name list --------------------------------------------------------------------------------

DWORD list_hash(const ZFileNameList *l)
{
    unsigned h = fnv(2166136261u, &l->vftable, 4);
    h = fnv(h, &l->names.vftable, 4);
    h = fnv(h, &l->names.count, 8);
    for (DWORD i = 0; i < l->names.count; i++) {
        auto *r = (const ZFileListRecord *)l->names.data[i];
        h = fnv_text(h, r->name.c_str());
        h = fnv(h, &r->size, 8);
    }
    return h;
}
void make_list(ZFileNameList *l) { original<ZFileNameList *(THISCALL *)(ZFileNameList *)>(0x4a3970)(l); }
void free_list(ZFileNameList *l) { original<DWORD(THISCALL *)(ZFileNameList *)>(0x4a3a50)(l); }

const char *const FILES[] = {"a.zax", "B.ZAX", "Level 1/main.zax", "c", "", "long name with spaces.wav", "dup.txt", "DUP.TXT"};

void add_files(int v, ZFileNameList *l, DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        ZFileRecord rec = {i * 1000 + 7, (WORD)(0x5000 + i), (WORD)(0x6000 + i * 3)};
        pick<DWORD(THISCALL *)(void *, const char *, const ZFileRecord *)>(v, 0x4a3b80)(l, FILES[i % 8], &rec);
    }
}

void test_file_list(Tally &t)
{
    // Constructor on prefilled bytes
    {
        BYTE obj[2][0x18];
        DWORD r[2];
        for (int v = 0; v < 2; v++) {
            memset(obj[v], 0x6b, sizeof obj[v]);
            r[v] = (DWORD)(uintptr_t)pick<void *(THISCALL *)(void *)>(v, 0x4a3970)(obj[v]) - (DWORD)(uintptr_t)obj[v];
        }
        check(t, r[0] == r[1] && memcmp(obj[0], obj[1], sizeof obj[0]) == 0, "FileNameList Ctor", r[0], r[1]);
    }
    // Add (returns the index), Contains, GetName
    for (DWORD n = 0; n <= 16; n++) {
        ZFileNameList l[2];
        DWORD r[2], h[2];
        for (int v = 0; v < 2; v++) {
            make_list(&l[v]);
            add_files(v, &l[v], n);
            ZFileRecord rec = {123, 0x1234, 0x5678};
            r[v] = pick<DWORD(THISCALL *)(void *, const char *, const ZFileRecord *)>(v, 0x4a3b80)(&l[v], "last.one", &rec);
            h[v] = list_hash(&l[v]);
        }
        char what[80];
        wsprintfA(what, "FileNameList Add n %u", n);
        check(t, r[0] == r[1] && h[0] == h[1], what, r[0], r[1]);
        for (const char *q : {"a.zax", "A.ZAX", "b.zax", "dup.txt", "", "missing", "last.one", "LEVEL 1/MAIN.ZAX", "Level 1\\main.zax"}) {
            for (int v = 0; v < 2; v++)
                r[v] = pick<DWORD(THISCALL *)(void *, const char *)>(v, 0x4a3bf0)(&l[v], q);
            wsprintfA(what, "FileNameList Contains n %u \"%s\"", n, q);
            check(t, r[0] == r[1], what, r[0], r[1]);
        }
        for (DWORD i = 0; i <= n; i++) {
            StringState st[2];
            for (int v = 0; v < 2; v++) {
                ZString z;
                r[v] = (DWORD)(uintptr_t)pick<ZString *(THISCALL *)(void *, ZString *, DWORD)>(v, 0x4a3b60)(&l[v], &z, i) - (DWORD)(uintptr_t)&z;
                st[v] = state(z, nullptr);
                at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
            }
            wsprintfA(what, "FileNameList GetName n %u i %u", n, i);
            check(t, r[0] == r[1] && memcmp(&st[0], &st[1], sizeof st[0]) == 0, what, r[0], r[1]);
        }
        // Destructor (the result is operator delete's eax when there is data) and deleting destructor
        for (int v = 0; v < 2; v++)
            r[v] = pick<DWORD(THISCALL *)(void *)>(v, 0x4a3a50)(&l[v]);
        check(t, (n || r[0] == r[1]) && memcmp(&l[0], &l[1], sizeof l[0]) == 0, "FileNameList Dtor", r[0], r[1]);
        for (DWORD flags : {0u, 1u}) {
            for (int v = 0; v < 2; v++) {
                ZFileNameList *p = flags ? (ZFileNameList *)heap_alloc(sizeof(ZFileNameList)) : &l[v];
                make_list(p);
                add_files(v, p, n);
                r[v] = (DWORD)(uintptr_t)pick<void *(THISCALL *)(void *, DWORD)>(v, 0x4a39a0)(p, flags) - (DWORD)(uintptr_t)p;
                h[v] = flags ? 0 : fnv(2166136261u, p, sizeof *p);
            }
            wsprintfA(what, "FileNameList DeletingDtor n %u flags %u", n, flags);
            check(t, r[0] == r[1] && r[0] == 0 && h[0] == h[1], what, h[0], h[1]);
        }
    }
    // Search: the game's folders (read only) into lists with and without old entries
    const struct {
        const char *folder, *ext;
        DWORD mode;
    } searches[] = {{"Levels", "zax", 0},       {"Levels/", "zax", 2}, {"Sounds", "wav", 1},
                    {"Nothing here", "wav", 1}, {"", "txt", 2},        {"Cache/Fonts", "seq16", 1}};
    for (const auto &s : searches)
        for (DWORD old : {0u, 3u}) {
            ZFileNameList l[2];
            DWORD r[2], h[2];
            for (int v = 0; v < 2; v++) {
                ZFileSpec spec;
                original<ZFileSpec *(THISCALL *)(ZFileSpec *)>(0x4a09f0)(&spec);
                original<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(&spec, s.folder, s.ext);
                spec.mode = s.mode;
                make_list(&l[v]);
                add_files(v, &l[v], old);
                r[v] = pick<DWORD(THISCALL *)(void *, const ZFileSpec *)>(v, 0x4a3ae0)(&l[v], &spec);
                h[v] = list_hash(&l[v]);
                free_list(&l[v]);
                StringOps o = original_string_ops();
                for (int k = 4; k >= 0; k--)
                    o.dtor(&spec.ext[k]);
            }
            char what[80];
            wsprintfA(what, "FileNameList Search \"%s\" old %u: count", s.folder, old);
            check(t, r[0] == r[1] && h[0] == h[1], what, r[0], r[1]);
        }
}

}  // namespace

ZAX_CHECKS(resource_lists)
{
    (void)cls;
    (void)n;
    Tally t{"Resource list registry, file name list"};
    DWORD t0 = GetTickCount();
    test_registry(t);
    test_global(t);
    test_file_list(t);
    log("  resource list checks: %u ms", GetTickCount() - t0);
    t.report();
}

}  // namespace zax
