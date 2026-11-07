// Checks of the resource cache manager's methods and the resource base's constructor, destructors,
// reset and assignment (resource_manager.cpp) against the original, in the game thread.
// Every case builds the same random scenario twice (side 0 for the original, side 1 for the
// reimplementation): fake resources (0x60 bytes like the base class) whose vtable slots record their
// calls in an event log and return programmed values, and a fake manager that *0x713688 points to
// during the call. Slot 11 (unload) unlinks the resource through the side's manager unlink, so the
// loops of SetLevel and UnloadAll end. Pointers into a side's fakes are compared relative to them,
// names by their text. Names are heap texts of paths that are never opened (slot 15 loads).
#include "check_util.h"
#include "resource_cache.h"

WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }

constexpr int NRES = 10, NLIST = 7;
struct FakeRes {
    CResource r;
    DWORD tail[(RESOURCE_SIZE - sizeof(CResource)) / 4];
    DWORD id, valid, load_ok, accept, unloads;
    DWORD pad[3];
};
static_assert(sizeof(FakeRes) == 0x80);
struct FakeCache {
    CResourceCache c;
    DWORD pad[2];
};
struct alignas(256) Side {
    FakeRes res[NRES];
    FakeCache cache;
};
alignas(4096) Side g_sides[2];
int g_cur;

constexpr int LOG = 1024;
DWORD g_log[2][LOG];
int g_nlog[2];
int g_slot8[2];

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
DWORD name_hash(const char *n) { return n ? fnv_text(1, n) : 0; }

// ---- The fakes' slots ----------------------------------------------------------------------------

DWORD THISCALL s0(FakeRes *r) { ev(0x100, r->id); return result(0x10); }
DWORD THISCALL s1(FakeRes *r, CResource *o) { ev(0x101, r->id, id_of(o)); return result(r->accept); }
DWORD THISCALL s2(FakeRes *r, DWORD a) { ev(0x102, r->id, a); return result(0x12); }
DWORD THISCALL s3(FakeRes *r, DWORD a) { ev(0x103, r->id, a); return result(0x13); }
DWORD THISCALL s4(FakeRes *r, DWORD a, DWORD b) { ev(0x104, r->id, a, b); return result(0x14); }
DWORD THISCALL s_bad(FakeRes *r) { ev(0x1ff, r->id); return result(0x1f); }
DWORD THISCALL s8(FakeRes *r, DWORD v) { g_slot8[g_cur]++; ev(0x108, r->id, v); return result(0x18); }
DWORD THISCALL s9(FakeRes *r, DWORD v) { ev(0x109, r->id, v); return result(0x19); }
DWORD THISCALL s10(FakeRes *r, DWORD v) { ev(0x10a, r->id, v); return result(0x1a); }
// Unload: unlinks itself through the side's manager unlink (a stuck loop is cut after 64 calls)
DWORD THISCALL s11(FakeRes *r)
{
    ev(0x10b, r->id);
    CResourceCache *c = &g_sides[g_cur].cache.c;
    DWORD v = pick<DWORD(THISCALL *)(CResourceCache *, CResource *)>(g_cur, 0x57afa0)(c, &r->r);
    if (++r->unloads > 64)
        c->list = nullptr;
    ev(0x10b, r->id, rel(g_cur, v));
    return result(0x1b);
}
DWORD THISCALL s12(FakeRes *r) { ev(0x10c, r->id); return result(0x1c); }
DWORD THISCALL s13(FakeRes *r, DWORD f) { ev(0x10d, r->id, f); return result(0x1d); }
DWORD THISCALL s14(FakeRes *r)
{
    DWORD v = r->valid & 1;
    r->valid = (r->valid >> 1) | (v << 15);
    ev(0x10e, r->id, v);
    return result(v);
}
DWORD THISCALL s15(FakeRes *r, const char *name, DWORD arg)
{
    ev(0x10f, r->id, name_hash(name), arg);
    return result(r->load_ok);
}
void *g_res_vtable[16] = {(void *)&s0, (void *)&s1, (void *)&s2, (void *)&s3, (void *)&s4, (void *)&s_bad,
                          (void *)&s_bad, (void *)&s_bad, (void *)&s8, (void *)&s9, (void *)&s10, (void *)&s11,
                          (void *)&s12, (void *)&s13, (void *)&s14, (void *)&s15};

DWORD THISCALL c_bad(FakeCache *) { ev(0x2ff); return result(0x2f); }
DWORD THISCALL c7(FakeCache *, DWORD a) { ev(0x207, a); return result(0x27); }
void *g_cache_vtable[10] = {(void *)&c_bad, (void *)&c_bad, (void *)&c_bad, (void *)&c_bad, (void *)&c_bad,
                            (void *)&c_bad, (void *)&c_bad, (void *)&c7, (void *)&c_bad, (void *)&c_bad};

// ---- Scenarios -----------------------------------------------------------------------------------

char *heap_text(const char *s)
{
    DWORD n = (DWORD)lstrlenA(s) + 1;
    char *p = (char *)heap_alloc(n);
    memcpy(p, s, n);
    return p;
}

const char *const PATHS[] = {"Sounds/Zax/Hit.wav", "Music/Act 1.mp3", "Sounds/Korbo/Die 2.wav"};

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
        r.priority = random() % 400 - 100;
        r.f38 = random();
        r.f3c = (float)(random() % 1000) / 7.0f;
        for (DWORD &t : f.tail)
            t = random();
        f.id = 0x1000 + (DWORD)i;
        f.valid = random() >> 8;
        f.load_ok = random() >> 4 & 1;
        f.accept = random() % 3 != 0;
        if (random() % 3 == 0)
            r.name = heap_text(PATHS[random() % 3]);
    }
    for (int i = 0; i < NRES - 1; i++) {
        if (random() % 4 == 0)
            s.res[i].r.source = &s.res[i + 1 + random() % (NRES - 1 - i)].r;
        if (random() % 5 == 0)
            s.res[i].r.replacement = &s.res[i + 1 + random() % (NRES - 1 - i)].r;
    }
    CResourceCache &c = s.cache.c;
    c.vftable = (DWORD)(uintptr_t)g_cache_vtable;
    c.f04 = random() % 5 == 0 ? -4000 : (int)(random() % 100) - 20;
    c.limit = random() % 5;
    c.disabled = random() % 3 == 0 ? (BYTE)(1 + random() % 255) : 0;
    c.f10 = random();
    c.stamp = 2 + random() % 3;
    c.max_loaded = (int)(random() % 10);
    c.max_cost = (int)(random() % 200);
    c.loaded = (int)(random() % 6);
    const float scales[] = {1.0f, 0.5f, 1.75f, 0.013f, 3.3f, -0.7f};
    c.scale = scales[random() % 6];
    c.cost = (int)(random() % 120);
    c.f30 = random();
    c.bonus = random() % 4 == 0 ? (DWORD)RESOURCE_LOW_PRIORITY_BONUS : random() % 300;
    s.cache.pad[0] = random();
    s.cache.pad[1] = random();
    // A random part of the first NLIST resources in random order in the manager's list
    int order[NLIST];
    for (int i = 0; i < NLIST; i++)
        order[i] = i;
    for (int i = NLIST - 1; i > 0; i--) {
        int j = (int)(random() % (DWORD)(i + 1)), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    CResource **tail = &c.list;
    for (int i = 0; i < NLIST; i++)
        if (random() % 4) {
            *tail = &s.res[order[i]].r;
            tail = &s.res[order[i]].r.next;
        }
    *tail = nullptr;
}

void cleanup(int side)
{
    for (FakeRes &f : g_sides[side].res)
        if (char *n = f.r.name) {
            heap_free(n);
            f.r.name = nullptr;
        }
}

bool same_state(char *why)
{
    for (int i = 0; i < NRES; i++) {
        const FakeRes &a = g_sides[0].res[i], &b = g_sides[1].res[i];
        DWORD x[RESOURCE_SIZE / 4], y[RESOURCE_SIZE / 4];
        memcpy(x, &a, RESOURCE_SIZE);
        memcpy(y, &b, RESOURCE_SIZE);
        x[3] = name_hash(a.r.name);
        y[3] = name_hash(b.r.name);
        // Equal raw values are equal: a random value can fall into one side's fakes (field 0x5c in run
        // 20261023-205203-e915), real pointers into the two sides never have the same value
        for (DWORD k = 0; k < RESOURCE_SIZE / 4; k++)
            if (x[k] != y[k] && rel(0, x[k]) != rel(1, y[k])) {
                wsprintfA(why, "res %d field %x: %x / %x", i, k * 4, x[k], y[k]);
                return false;
            }
        if (a.valid != b.valid || a.unloads != b.unloads) {
            wsprintfA(why, "res %d valid/unloads", i);
            return false;
        }
    }
    const DWORD *x = (const DWORD *)&g_sides[0].cache, *y = (const DWORD *)&g_sides[1].cache;
    for (int k = 0; k < (int)(sizeof(FakeCache) / 4); k++)
        if (x[k] != y[k] && rel(0, x[k]) != rel(1, y[k])) {
            wsprintfA(why, "cache field %x: %x / %x", k * 4, x[k], y[k]);
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

const char *const OPS[] = {"Get", "CacheCtor", "CacheDeletingDtor(0)", "CacheDtor", "Release", "NextStamp", "Unlink",
                           "ScaledPriority", "Rescale", "SetLowPriority", "SetByte30", "SetLevel", "SetLimit",
                           "SetDisabled", "SetScale", "CheckEnabled", "ForgetAll", "UnloadAll", "ResCtor",
                           "ResDeletingDtor(0)", "ResDtor", "ReloadThen", "Reset", "Assign"};
constexpr int NOPS = 24;

// One operation of the current side; returns the value to compare, mask = the bits that are defined
DWORD run(int op, int v, CResourceCache *c, CResource *r, CResource *other, DWORD a, DWORD b, DWORD &mask)
{
    mask = 0xffffffff;
    using C = CResourceCache *;
    using R = CResource *;
    switch (op) {
    case 0:
        return (DWORD)(uintptr_t)pick<C(__cdecl *)()>(v, 0x57aef0)();
    case 1:
        memset(reinterpret_cast<BYTE *>(c) + 4, (int)(a & 0xff), sizeof(CResourceCache) - 4);
        return (DWORD)(uintptr_t)pick<C(THISCALL *)(C)>(v, 0x57af00)(c);
    case 2:
        return (DWORD)(uintptr_t)pick<C(THISCALL *)(C, DWORD)>(v, 0x57af30)(c, a & 0xfe);
    case 3:
        mask = 0;
        pick<void(THISCALL *)(C)>(v, 0x57af50)(c);
        return 0;
    case 4:
        return pick<DWORD(THISCALL *)(C, R)>(v, 0x57af60)(c, r);
    case 5:
        mask = 0;
        pick<void(THISCALL *)(C)>(v, 0x57af90)(c);
        return 0;
    case 6:
        return pick<DWORD(THISCALL *)(C, R)>(v, 0x57afa0)(c, r);
    case 7: {
        const int ps[] = {0, 1, -1, 100, -3333, -3334, 0x7ffff000, (int)0x80000000, 12345};
        int p = a % 3 ? (int)(a % 2000) - 1000 : ps[b % 9];
        return (DWORD)pick<int(THISCALL *)(C, int)>(v, 0x57b050)(c, p);
    }
    case 8: {
        DWORD ret = pick<DWORD(THISCALL *)(C, int)>(v, 0x57b080)(c, (int)(a % 1000) - 500);
        if (!g_slot8[v])
            mask = 0;   // the original leaves eax from its entry
        return ret;
    }
    case 9: {
        DWORD ret = pick<DWORD(THISCALL *)(C, BYTE)>(v, 0x57b0d0)(c, (BYTE)(a % 3));
        if (!g_slot8[v])
            mask = 0xff;
        return ret;
    }
    case 10:
        mask = 0xff;
        return pick<DWORD(THISCALL *)(C, BYTE)>(v, 0x57b170)(c, (BYTE)a);
    case 11: {
        const int levels[] = {-4000, -3001, -3000, 0, 50, (int)0x80000000};
        return pick<DWORD(THISCALL *)(C, int)>(v, 0x57b180)(c, a % 3 ? levels[b % 6] : (int)a);
    }
    case 12:
        return pick<DWORD(THISCALL *)(C, DWORD)>(v, 0x57b1b0)(c, a);
    case 13:
        mask = 0xff;
        return pick<DWORD(THISCALL *)(C, BYTE)>(v, 0x57b1c0)(c, (BYTE)(a % 3));
    case 14:
        return pick<DWORD(THISCALL *)(C, DWORD)>(v, 0x57b1d0)(c, bits((float)(a % 5000) / 1000.0f));
    case 15:
        if (!c->disabled)
            mask = 0xff;
        return pick<DWORD(THISCALL *)(C)>(v, 0x57b1e0)(c);
    case 16:
        mask = 0xff;
        return pick<DWORD(THISCALL *)(C)>(v, 0x57b200)(c);
    case 17:
        if (!c->list)
            mask = 0xff;
        return pick<DWORD(THISCALL *)(C)>(v, 0x57b210)(c);
    case 18: {
        // a resource of garbage (its name and source would leak: free the name first)
        if (r->name)
            heap_free(r->name);
        memset(reinterpret_cast<BYTE *>(r) + 4, (int)(a & 0xff), RESOURCE_SIZE - 4);
        return (DWORD)(uintptr_t)pick<R(THISCALL *)(R)>(v, 0x57b230)(r);
    }
    case 19:
        return (DWORD)(uintptr_t)pick<R(THISCALL *)(R, DWORD)>(v, 0x57b250)(r, a & 0xfe);
    case 20:
        mask = 0;
        pick<void(THISCALL *)(R)>(v, 0x57b280)(r);
        return 0;
    case 21:
        return pick<DWORD(THISCALL *)(R, DWORD, DWORD)>(v, 0x57b290)(r, a, b);
    case 22:
        return pick<DWORD(THISCALL *)(R)>(v, 0x57b2f0)(r);
    default:
        if (other->name || other->source)
            mask = 0xff;   // eax from the entry
        return pick<DWORD(THISCALL *)(R, R)>(v, 0x57b370)(r, other);
    }
}

void test_ops(Tally &t)
{
    DWORD &g = *at<DWORD *>(RESOURCE_CACHE);
    DWORD saved = g;
    for (int op = 0; op < NOPS; op++)
        for (unsigned k = 0; k < 160; k++) {
            unsigned seed = 7000u * (unsigned)op + k;
            DWORD ret[2], mask[2];
            int target = 0, from = 0;
            for (int v = 0; v < 2; v++) {
                setup(v, seed);
                DWORD a = random(), b = random();
                target = (int)(random() % NRES);
                from = (int)(random() % NRES);
                Side &s = g_sides[v];
                if (op == 23 && k % 3) {
                    // mostly a source without name and source, so the copy runs
                    if (s.res[from].r.name)
                        heap_free(s.res[from].r.name);
                    s.res[from].r.name = nullptr;
                    s.res[from].r.source = nullptr;
                }
                g_cur = v;
                g_nlog[v] = 0;
                g_slot8[v] = 0;
                g = (DWORD)(uintptr_t)&s.cache;
                ret[v] = run(op, v, &s.cache.c, &s.res[target].r, &s.res[from].r, a, b, mask[v]);
                g = saved;
            }
            char why[160] = "";
            bool same = mask[0] == mask[1] && rel(0, ret[0] & mask[0]) == rel(1, ret[1] & mask[1]);
            if (!same)
                wsprintfA(why, "return %x / %x (mask %x / %x)", ret[0], ret[1], mask[0], mask[1]);
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
            wsprintfA(d, "[%s seed %u target %d from %d: %.160s]", OPS[op], seed, target, from, why);
            t.check(same, "manager", d);
            cleanup(0);
            cleanup(1);
        }
}

// The deleting destructors with flag 1 on blocks of their own (the manager from operator new, the
// resource from the pool); compared: the result and the vtable written before the block is freed
// (read through the freed block is not allowed, so only the result)
void test_delete(Tally &t)
{
    for (int k = 0; k < 8; k++) {
        DWORD r[2];
        for (int v = 0; v < 2; v++) {
            auto *c = (CResourceCache *)heap_alloc(sizeof(CResourceCache));
            memset(c, 0x33 + k, sizeof *c);
            r[v] = (DWORD)(uintptr_t)pick<CResourceCache *(THISCALL *)(CResourceCache *, DWORD)>(v, 0x57af30)(c, 1 | (DWORD)k << 4) ==
                   (DWORD)(uintptr_t)c;
        }
        t.check(r[0] == r[1] && r[0], "manager", "[CacheDeletingDtor(1)]");
        for (int v = 0; v < 2; v++) {
            auto *res = (CResource *)pool_alloc(RESOURCE_SIZE);
            memset(res, 0x44 + k, RESOURCE_SIZE);
            r[v] = (DWORD)(uintptr_t)pick<CResource *(THISCALL *)(CResource *, DWORD)>(v, 0x57b250)(res, 1 | (DWORD)k << 4) ==
                   (DWORD)(uintptr_t)res;
        }
        t.check(r[0] == r[1] && r[0], "manager", "[ResDeletingDtor(1)]");
    }
    // The game's own manager: Get through both sides
    t.check(original<CResourceCache *(__cdecl *)()>(0x57aef0)() == ResourceCache_Get(), "manager", "[Get real]");
}

}  // namespace

ZAX_CHECKS(resource_manager)
{
    (void)cls;
    (void)n;
    Tally t{"Resource manager"};
    DWORD t0 = GetTickCount();
    test_ops(t);
    test_delete(t);
    log("  resource manager checks: %u ms", GetTickCount() - t0);
    t.report();
}

}  // namespace zax
