// Checks of the cached resource (resource_cache.cpp) against the original, in the game thread.
// Every case builds the same random scenario twice (side 0 for the original, side 1 for the
// reimplementation): fake resources whose vtable slots record their calls in an event log and return
// programmed values (slot 14 "valid" from a bit pattern, slot 15 "loaded", slot 1 "accepted") and a
// fake manager (its slot 3 hands out spare fake resources) that *0x713688 points to during the call.
// The exe's own unlink (0x57afa0) runs on the fakes. Pointers into a side's fakes are compared
// relative to them, names by their text (resident names "~CFileBuf 0x..." by their stream contents).
// The files for MakeResident are written to a temp folder of our own and removed again.
#include "check_util.h"
#include "file_buffer.h"
#include "init_list.h"
#include "memory_stream.h"
#include "resource_cache.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }

constexpr int NBASE = 6, NRES = 16;
struct FakeRes {
    CResource r;
    DWORD id, valid, load_ok, accept;
    DWORD pad[12];
};
static_assert(sizeof(FakeRes) == 0x80);
struct FakeCache {
    CResourceCache c;
    DWORD pad[2];
};
struct alignas(256) Side {
    FakeRes res[NRES];
    FakeCache cache;
    int next_extra;
};
alignas(4096) Side g_sides[2];
int g_cur;

constexpr int LOG = 512;
DWORD g_log[2][LOG];
int g_nlog[2];

DWORD rel(int side, DWORD v)
{
    DWORD base = (DWORD)(uintptr_t)&g_sides[side];
    if (v - base < sizeof(Side))
        return 0x40000000 + (v - base);
    return v;
}
DWORD id_of(const void *p) { return rel(g_cur, (DWORD)(uintptr_t)p); }
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

// ---- The fakes' slots ----------------------------------------------------------------------------

DWORD THISCALL s0(FakeRes *r) { ev(0x100, r->id); return result(0x10); }
DWORD THISCALL s1(FakeRes *r, CResource *o) { ev(0x101, r->id, id_of(o)); return result(r->accept); }
DWORD THISCALL s2(FakeRes *r, DWORD a) { ev(0x102, r->id, a); return result(0x12); }
DWORD THISCALL s3(FakeRes *r, DWORD a) { ev(0x103, r->id, a); return result(0x13); }
DWORD THISCALL s_bad(FakeRes *r) { ev(0x1ff, r->id); return result(0x1f); }
DWORD THISCALL s8(FakeRes *r, DWORD v) { ev(0x108, r->id, v); return result(0x18); }
DWORD THISCALL s9(FakeRes *r, DWORD v) { ev(0x109, r->id, v); return result(0x19); }
DWORD THISCALL s10(FakeRes *r, DWORD v) { ev(0x10a, r->id, v); return result(0x1a); }
DWORD THISCALL s11(FakeRes *r) { ev(0x10b, r->id); return result(0x1b); }
DWORD THISCALL s12(FakeRes *r) { ev(0x10c, r->id); return result(0x1c); }
DWORD THISCALL s13(FakeRes *r, DWORD f) { ev(0x10d, r->id, f); return result(0x1d); }
// Valid: the next bit of a 16-bit pattern (rotating)
DWORD THISCALL s14(FakeRes *r)
{
    DWORD v = r->valid & 1;
    r->valid = (r->valid >> 1) | (v << 15);
    ev(0x10e, r->id, v);
    return result(v);
}
DWORD name_hash(const char *n);
DWORD THISCALL s15(FakeRes *r, const char *name, DWORD arg)
{
    ev(0x10f, r->id, name_hash(name), arg);
    return result(r->load_ok);
}
void *g_res_vtable[16] = {(void *)&s0, (void *)&s1, (void *)&s2, (void *)&s3, (void *)&s_bad, (void *)&s_bad,
                          (void *)&s_bad, (void *)&s_bad, (void *)&s8, (void *)&s9, (void *)&s10, (void *)&s11,
                          (void *)&s12, (void *)&s13, (void *)&s14, (void *)&s15};

DWORD THISCALL c_bad(FakeCache *) { ev(0x2ff); return result(0x2f); }
CResource *THISCALL c_create(FakeCache *)
{
    Side &s = g_sides[g_cur];
    int i = s.next_extra < NRES ? s.next_extra++ : NRES - 1;
    ev(0x203, s.res[i].id);
    return &s.res[i].r;
}
void *g_cache_vtable[8] = {(void *)&c_bad, (void *)&c_bad, (void *)&c_bad, (void *)&c_create,
                           (void *)&c_bad, (void *)&c_bad, (void *)&c_bad, (void *)&c_bad};

// ---- Scenarios -----------------------------------------------------------------------------------

char g_dir[MAX_PATH];
char g_files[4][MAX_PATH];   // 3 test files (0, 100, 70,000 bytes) and a missing one

bool resident(const char *name) { return name && memcmp(name, "~CFileBuf 0x", 12) == 0; }
ZStream *stream_of(const char *name) { return original<ZStream *(__fastcall *)(const char *)>(0x4a2a10)(name); }

char *heap_text(const char *s)
{
    DWORD n = (DWORD)lstrlenA(s) + 1;
    char *p = (char *)heap_alloc(n);
    memcpy(p, s, n);
    return p;
}

// A name of a test resource: null, a file (existing or missing) or a resident copy of a file
char *make_name(DWORD k)
{
    switch (k % 6) {
    case 0:
        return nullptr;
    case 5: {
        ZStream *m = original<ZStream *(__fastcall *)(const char *, DWORD)>(0x505e40)(g_files[(k / 6) % 3], 4);
        ZString z;
        at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
        original<void(__fastcall *)(ZStream *, DWORD, ZString *, const char *)>(0x4a29d0)(m, 0, &z, "res.bin");
        char *p = heap_text(z.c_str());
        at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
        return p;
    }
    default:
        return heap_text(g_files[(k - 1) % 4]);
    }
}

void setup(int side, unsigned seed)
{
    Side &s = g_sides[side];
    memset(&s, 0, sizeof s);
    g_seed = seed;
    for (int i = 0; i < NRES; i++) {
        FakeRes &f = s.res[i];
        CResource &r = f.r;
        r.vftable = (DWORD)(uintptr_t)g_res_vtable;
        r.type = random() % 7 == 0 ? 0xffffffff : random() % 3;
        r.cost = random() % 50;
        r.flags = random() & 0x3b;
        r.f18 = random();
        r.stamp = random() % 3;
        r.f28 = random();
        r.load_cost = random() % 4;
        r.f30 = random();
        r.priority = random() % 200 - 50;
        r.f38 = random();
        r.f3c = (float)(random() % 1000) / 7.0f;
        f.id = 0x1000 + (DWORD)i;
        f.valid = random() >> 8;
        f.load_ok = random() >> 4 & 1;
        f.accept = random() % 3 != 0;
        if (i < NBASE)
            r.name = make_name(random() >> 5);
    }
    for (int i = 0; i < NBASE - 1; i++) {
        if (random() % 4 == 0)
            s.res[i].r.source = &s.res[i + 1 + random() % (NBASE - 1 - i)].r;
        if (random() % 5 == 0)
            s.res[i].r.replacement = &s.res[i + 1 + random() % (NRES - 1 - i)].r;
    }
    s.next_extra = NBASE;
    CResourceCache &c = s.cache.c;
    c.vftable = (DWORD)(uintptr_t)g_cache_vtable;
    c.f04 = random() % 5 == 0 ? -4000 : (int)(random() % 100) - 20;
    c.limit = random() % 5;
    c.disabled = random() % 6 == 0;
    c.stamp = 2 + random() % 3;
    c.max_loaded = (int)(random() % 10);
    c.max_cost = (int)(random() % 200);
    c.loaded = (int)(random() % 6);
    const float scales[] = {1.0f, 0.5f, 1.75f, 0.013f, 3.3f};
    c.scale = scales[random() % 5];
    c.cost = (int)(random() % 120);
    c.bonus = random() % 300;
    c.f30 = random();
    // A random part of the base resources in random order in the manager's list
    int order[NBASE];
    for (int i = 0; i < NBASE; i++)
        order[i] = i;
    for (int i = NBASE - 1; i > 0; i--) {
        int j = (int)(random() % (DWORD)(i + 1)), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    CResource **tail = &c.list;
    for (int i = 0; i < NBASE; i++)
        if (random() % 3) {
            *tail = &s.res[order[i]].r;
            tail = &s.res[order[i]].r.next;
        }
}

void cleanup(int side)
{
    for (FakeRes &f : g_sides[side].res)
        if (char *n = f.r.name) {
            if (resident(n))
                original<DWORD(__fastcall *)(const char *)>(0x4a2a70)(n);
            heap_free(n);
            f.r.name = nullptr;
        }
}

// A name: text hash, or for a resident one its stream (size and bytes) and the part behind the address
DWORD name_hash(const char *n)
{
    if (!n)
        return 0;
    if (!resident(n))
        return fnv_text(1, n);
    unsigned h = fnv_text(2, n + 20);
    if (auto *m = (MemoryStream *)stream_of(n)) {
        h = fnv(h, &m->size, 4);
        if (m->data)
            h = fnv(h, m->data, m->size);
    }
    return h;
}

bool same_state(char *why)
{
    for (int i = 0; i < NRES; i++) {
        const CResource &a = g_sides[0].res[i].r, &b = g_sides[1].res[i].r;
        DWORD x[16], y[16];
        memcpy(x, &a, 64);
        memcpy(y, &b, 64);
        x[3] = name_hash(a.name);
        y[3] = name_hash(b.name);
        // +0x28 and +0x30 hold random values, not pointers: compared as they are (a random value can
        // fall into one side's fakes, e.g. run 20261023-165642-27eb after the DLL grew); equal raw values
        // are equal in every field (field 0x18 in run 20261024-202357-54d1)
        for (int k = 0; k < 16; k++)
            if (x[k] != y[k] && (k == 0x28 / 4 || k == 0x30 / 4 || rel(0, x[k]) != rel(1, y[k]))) {
                wsprintfA(why, "res %d field %x: %x / %x", i, k * 4, x[k], y[k]);
                return false;
            }
        if (g_sides[0].res[i].valid != g_sides[1].res[i].valid) {
            wsprintfA(why, "res %d valid", i);
            return false;
        }
    }
    const DWORD *x = (const DWORD *)&g_sides[0].cache, *y = (const DWORD *)&g_sides[1].cache;
    for (int k = 0; k < (int)(sizeof(CResourceCache) / 4); k++)
        if (x[k] != y[k] && (k == 0x30 / 4 || rel(0, x[k]) != rel(1, y[k]))) {
            wsprintfA(why, "cache field %x: %x / %x", k * 4, x[k], y[k]);
            return false;
        }
    if (g_sides[0].next_extra != g_sides[1].next_extra) {
        lstrcpyA(why, "extras");
        return false;
    }
    return true;
}

bool same_log()
{
    if (g_nlog[0] != g_nlog[1])
        return false;
    for (int i = 0; i < g_nlog[0] && i < LOG; i++)
        if (g_log[0][i] != g_log[1][i])
            return false;
    return true;
}

const char *const OPS[] = {"Reload", "MakeResident(0)", "MakeResident(1)", "FindReplacement", "GetF3c", "CheckValid",
                           "Load", "Insert", "ReloadFalse", "SetFlag20", "SetFlag2", "SetKeep", "SetLoadCost",
                           "SetType", "SetPriority", "SetF30", "Unlink", "Unload"};
constexpr int NOPS = 18;

// One operation on resource `target` of the current side; returns the value to compare
DWORD run(int op, int v, CResource *r, DWORD a, DWORD b, DWORD c, const char *path, DWORD &mask)
{
    mask = 0xffffffff;
    switch (op) {
    case 0:
        return pick<DWORD(THISCALL *)(CResource *)>(v, 0x57b520)(r);
    case 1: {
        if (!r->name)
            return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57b560)(r, 0);
        if (resident(r->name)) {
            ZStream *s = stream_of(r->name);
            return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57b560)(r, 0) == (DWORD)(uintptr_t)s;
        }
        pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57b560)(r, 0);
        return 0;   // the ZString destructor's eax (buffer addresses)
    }
    case 2:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57b560)(r, 1 + a % 3);
    case 3:
        return (DWORD)(uintptr_t)pick<CResource *(THISCALL *)(CResource *)>(v, 0x57b630)(r);
    case 4:
        return bits(pick<float(THISCALL *)(CResource *)>(v, 0x57b730)(r));
    case 5:
        return pick<DWORD(THISCALL *)(CResource *)>(v, 0x57b740)(r);
    case 6:
        return pick<DWORD(THISCALL *)(CResource *, const char *, BYTE, DWORD, DWORD)>(v, 0x57b760)(r, path, (BYTE)(a % 3), b % 3, c);
    case 7:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57b950)(r, a % 4);
    case 8:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57bb10)(r, a);
    case 9: {
        BYTE on = (BYTE)(a % 3);
        if (((r->flags >> 5) & 1) == on)
            mask = 0xff;   // the original's upper bytes are eax at entry
        return pick<DWORD(THISCALL *)(CResource *, BYTE)>(v, 0x57bb60)(r, on);
    }
    case 10:
        return pick<DWORD(THISCALL *)(CResource *, BYTE)>(v, 0x57bba0)(r, (BYTE)(a % 3));
    case 11:
        return pick<DWORD(THISCALL *)(CResource *, BYTE)>(v, 0x57bbc0)(r, (BYTE)(a % 3));
    case 12:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57bbf0)(r, a % 4 == 0 ? r->load_cost : b);
    case 13:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57bc50)(r, a);
    case 14:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57bc60)(r, a % 500 - 100);
    case 15:
        return pick<DWORD(THISCALL *)(CResource *, DWORD)>(v, 0x57bcf0)(r, a);
    case 16:
        return pick<DWORD(THISCALL *)(CResource *)>(v, 0x57bd00)(r);
    default:
        mask = 0xff;   // operator delete's eax when the name is freed
        return pick<DWORD(THISCALL *)(CResource *)>(v, 0x57bd30)(r);
    }
}

void test_ops(Tally &t)
{
    DWORD &g = *at<DWORD *>(RESOURCE_CACHE);
    DWORD saved = g;
    for (int op = 0; op < NOPS; op++)
        for (unsigned k = 0; k < 160; k++) {
            unsigned seed = 1000u * (unsigned)op + k;
            DWORD ret[2], mask[2];
            int target = 0;
            for (int v = 0; v < 2; v++) {
                setup(v, seed);
                DWORD a = random(), b = random(), c = random();
                target = (int)(random() % NBASE);
                const char *path = b % 2 ? g_files[a % 4] : "Levels/Test/Nothing.zax";
                g_cur = v;
                g_nlog[v] = 0;
                g = (DWORD)(uintptr_t)&g_sides[v].cache;
                ret[v] = run(op, v, &g_sides[v].res[target].r, a, b, c, path, mask[v]);
                g = saved;
            }
            char why[160] = "";
            bool same = mask[0] == mask[1] && rel(0, ret[0] & mask[0]) == rel(1, ret[1] & mask[1]);
            if (!same)
                wsprintfA(why, "return %x / %x", ret[0], ret[1]);
            else if (!same_log()) {
                int i = 0;
                while (i < g_nlog[0] && i < LOG - 1 && g_log[0][i] == g_log[1][i])
                    i++;
                wsprintfA(why, "log %d / %d, entry %d: %x / %x", g_nlog[0], g_nlog[1], i, g_log[0][i], g_log[1][i]);
            }
            else if (!same_state(why))
                same = false;
            if (why[0])
                same = false;
            char d[260];
            wsprintfA(d, "[%s seed %u target %d: %.160s]", OPS[op], seed, target, why);
            t.check(same, "resource", d);
            cleanup(0);
            cleanup(1);
        }
}

void write_file(const char *path, DWORD n)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    BYTE buf[1000];
    for (DWORD i = 0; i < sizeof buf; i++)
        buf[i] = (BYTE)(i * 7 + n);
    for (DWORD done = 0; done < n;) {
        DWORD k = n - done < sizeof buf ? n - done : sizeof buf, w;
        WriteFile(f, buf, k, &w, nullptr);
        done += k;
    }
    CloseHandle(f);
}

}  // namespace

ZAX_CHECKS(resource_cache)
{
    (void)cls;
    (void)n;
    Tally t{"Resource cache"};
    DWORD t0 = GetTickCount();
    char tmp[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp)) {
        log("  resource cache: no temp folder");
        return;
    }
    wsprintfA(g_dir, "%sZaxResCache\\", tmp);
    CreateDirectoryA(g_dir, nullptr);
    const DWORD sizes[] = {0, 100, 70000};
    for (int i = 0; i < 3; i++) {
        wsprintfA(g_files[i], "%sfile%d.bin", g_dir, i);
        write_file(g_files[i], sizes[i]);
    }
    wsprintfA(g_files[3], "%smissing.bin", g_dir);
    test_ops(t);
    for (int i = 0; i < 3; i++)
        DeleteFileA(g_files[i]);
    if (!RemoveDirectoryA(g_dir))
        log("  could not remove %s", g_dir);
    log("  resource cache checks: %u ms", GetTickCount() - t0);
    t.report();
}

}  // namespace zax
