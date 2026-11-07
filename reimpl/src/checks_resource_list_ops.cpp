// Checks of the resource lists' own functions and their alias array (resource_list_ops.cpp) against
// the original, in the game thread. Each case runs twice (side 0 original, side 1 reimplementation).
//
// The three walks run on fake lists whose slots (count, loaded, in use, keep) answer from bit
// patterns and record their calls in an event log. Find, AddAlias and the alias array run on resource
// lists of our own: records of 0x80 bytes with game-like names (terminated by an empty name), the
// vtable a copy of the real one 0x5ed4b4 whose slot 2 is the side's Find (FindOrFail calls it), and a
// real alias array (vtable 0x5ed558). AddAlias only gets names that exist (FindOrFail ends the game
// for a missing one). Compared: the results and the aliases' count, capacity, texts and indices.
#include "check_util.h"
#include "resource_list_ops.h"

WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }

int g_cur;
constexpr int LOG = 1024;
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

// ---- The walks on fake lists ---------------------------------------------------------------------

struct FakeList {
    DWORD vftable;
    DWORD count, loaded, in_use, keep;
};
DWORD next_bit(DWORD &bits)
{
    DWORD b = bits & 1;
    bits = (bits >> 1) | (b << 15);
    return b;
}
DWORD THISCALL f_bad(FakeList *) { ev(0x5ff); return result(0x5f); }
DWORD THISCALL f_count(FakeList *l) { ev(0x516); return l->count; }
DWORD THISCALL f_loaded(FakeList *l, DWORD i) { DWORD b = next_bit(l->loaded); ev(0x508, i, b); return result(b ? 0x31 : 0); }
DWORD THISCALL f_in_use(FakeList *l, DWORD i) { DWORD b = next_bit(l->in_use); ev(0x51d, i, b); return result(b ? 0x80 : 0); }
DWORD THISCALL f_unload(FakeList *, DWORD i) { ev(0x51f, i); return result(0x1f); }
DWORD THISCALL f_keep(FakeList *l, DWORD i) { DWORD b = next_bit(l->keep); ev(0x520, i, b); return result(b); }
DWORD THISCALL f_set_keep(FakeList *, DWORD i, DWORD x) { ev(0x521, i, x); return result(0x21); }
void *g_fake_vtable[40];

void test_walks(Tally &t)
{
    for (int i = 0; i < 40; i++)
        g_fake_vtable[i] = (void *)&f_bad;
    g_fake_vtable[8] = (void *)&f_loaded;
    g_fake_vtable[22] = (void *)&f_count;
    g_fake_vtable[29] = (void *)&f_in_use;
    g_fake_vtable[31] = (void *)&f_unload;
    g_fake_vtable[32] = (void *)&f_keep;
    g_fake_vtable[33] = (void *)&f_set_keep;
    const DWORD fns[] = {0x523d00, 0x523d40, 0x523d70};
    const char *const names[] = {"KeepLoaded", "UnloadLoaded", "UnloadUnused"};
    for (int f = 0; f < 3; f++)
        for (unsigned k = 0; k < 120; k++) {
            DWORD r[2];
            for (int v = 0; v < 2; v++) {
                g_seed = 900u * (unsigned)f + k;
                FakeList l = {(DWORD)(uintptr_t)g_fake_vtable, k % 12, random() >> 9, random() >> 11, random() >> 7};
                begin(v);
                r[v] = pick<DWORD(THISCALL *)(FakeList *)>(v, fns[f])(&l);
            }
            char d[160];
            wsprintfA(d, "[%s case %u: %x / %x, log %d / %d]", names[f], k, r[0], r[1], g_nlog[0], g_nlog[1]);
            t.check(r[0] == r[1] && same_log(), "list ops", d);
        }
}

// ---- Find, AddAlias and the alias array on lists of our own ----------------------------------------

const char *const RECORDS[] = {"Normal", "Fire", "Sounds/Zax/Hit.wav", "Acid", "Models/Korbo.mdl16", "Ice", "Plasma",
                               "Poison"};
const char *const TEXTS[] = {"Normal", "normal", "FIRE", "Hit.wav", "Sounds/Zax/Hit.wav", "Korbo.mdl16", "Laser",
                             "laser", "Electrical", "Physical", "", "Nothing", "Plasma", "Sounds\\Zax\\hit.WAV"};
constexpr int NTEXTS = 14;
const char *const ALIASES[] = {"Laser", "Electrical", "Physical", "Bullet", "Hit.wav", "Normal"};

struct OwnList {
    ZResourceList list;
    void *vtable[40];
    char records[9][0x80];
};
OwnList g_own[2];

void build(int v, int n)
{
    OwnList &o = g_own[v];
    memset(&o, 0, sizeof o);
    memcpy(o.vtable, at<void *>(0x5ed4b4), sizeof o.vtable);
    o.vtable[2] = v ? at<void *>(0x523df0) : original_address(0x523df0);
    for (int i = 0; i < n; i++)
        lstrcpyA(o.records[i], RECORDS[i]);
    ZRecordList &r = o.list.records;
    r.vftable = (DWORD)(uintptr_t)o.vtable;
    r.data = n < 0 ? nullptr : o.records[0];
    r.stride = 0x80;
    r.num = n < 0 ? 0 : (DWORD)n;
    o.list.aliases.vftable = RESOURCE_ALIASES_VTABLE;
}

DWORD aliases_hash(const ZResourceAliases &a)
{
    DWORD x[3] = {a.vftable, a.count, a.capacity};
    unsigned h = fnv(2166136261u, x, 12);
    for (DWORD i = 0; i < a.count; i++)
        if (ZResourceAlias *e = a.data[i]) {
            h = fnv_text(h, e->name.c_str());
            h = fnv(h, &e->index, 4);
        }
        else
            h = fnv(h, "-", 1);
    return h ^ (a.data ? 0x80000000u : 0);
}

void test_find(Tally &t)
{
    for (unsigned k = 0; k < 90; k++) {
        int n = (int)(k % 10) - 1;   // -1: no records at all
        DWORD r[2], h[2];
        char what[100] = "";
        bool same = true;
        unsigned seed = 5000u + k;
        // Aliases
        int naliases = (int)(k % 7);
        for (int v = 0; v < 2; v++)
            build(v, n);
        for (int j = 0; j < naliases && n > 0; j++) {
            g_seed = seed * 31u + (unsigned)j;
            const char *alias = ALIASES[random() % 6];
            const char *name = RECORDS[random() % (DWORD)n];
            for (int v = 0; v < 2; v++) {
                r[v] = pick<DWORD(THISCALL *)(ZResourceList *, const char *, const char *)>(v, 0x523dc0)(&g_own[v].list, alias, name);
                h[v] = aliases_hash(g_own[v].list.aliases);
            }
            if (r[0] != r[1] || h[0] != h[1]) {
                same = false;
                wsprintfA(what, "AddAlias %s %s: %x / %x", alias, name, r[0], r[1]);
            }
        }
        // Find with every text and several hints
        const int hints[] = {-1, 0, n > 0 ? n - 1 : -1, n > 2 ? 2 : -1};
        for (int i = 0; i < NTEXTS; i++)
            for (int hi = 0; hi < 4; hi++) {
                if (n < 0 && hints[hi] != -1)
                    continue;
                for (int v = 0; v < 2; v++)
                    r[v] = (DWORD)pick<int(THISCALL *)(ZResourceList *, const char *, int)>(v, 0x523df0)(&g_own[v].list, TEXTS[i], hints[hi]);
                if (r[0] != r[1] && same) {
                    same = false;
                    wsprintfA(what, "Find %s hint %d: %x / %x", TEXTS[i], hints[hi], r[0], r[1]);
                }
                for (int v = 0; v < 2; v++)
                    r[v] = (DWORD)pick<int(THISCALL *)(ZResourceAliases *, const char *)>(v, 0x55d1a0)(&g_own[v].list.aliases, TEXTS[i]);
                if (r[0] != r[1] && same) {
                    same = false;
                    wsprintfA(what, "Aliases.Find %s: %x / %x", TEXTS[i], r[0], r[1]);
                }
            }
        // Add directly (also an index of -1 and a text of the list)
        for (int j = 0; j < 3; j++) {
            for (int v = 0; v < 2; v++) {
                r[v] = pick<DWORD(THISCALL *)(ZResourceAliases *, const char *, int)>(v, 0x55d150)(&g_own[v].list.aliases, TEXTS[(k + (unsigned)j) % NTEXTS], j - 1);
                h[v] = aliases_hash(g_own[v].list.aliases);
            }
            if ((r[0] != r[1] || h[0] != h[1]) && same) {
                same = false;
                wsprintfA(what, "Aliases.Add %d: %x / %x", j, r[0], r[1]);
            }
        }
        // Destructor (twice: the second on the emptied array); its result is operator delete's eax
        // while there is a block
        for (int pass = 0; pass < 2; pass++) {
            bool had[2];
            for (int v = 0; v < 2; v++) {
                had[v] = g_own[v].list.aliases.data != nullptr;
                g_own[v].list.aliases.vftable = 0x12345678;
                r[v] = pick<DWORD(THISCALL *)(ZResourceAliases *)>(v, 0x55d0c0)(&g_own[v].list.aliases);
                h[v] = aliases_hash(g_own[v].list.aliases);
            }
            if ((had[0] != had[1] || (!had[0] && r[0] != r[1]) || h[0] != h[1]) && same) {
                same = false;
                wsprintfA(what, "Aliases.Dtor pass %d: %x / %x", pass, r[0], r[1]);
            }
        }
        char d[200];
        wsprintfA(d, "[case %u, %d records, %d aliases: %s]", k, n, naliases, what);
        t.check(same, "list ops", d);
    }
}

}  // namespace

ZAX_CHECKS(resource_list_ops)
{
    (void)cls;
    (void)n;
    Tally t{"Resource list ops"};
    DWORD t0 = GetTickCount();
    test_walks(t);
    test_find(t);
    log("  resource list ops checks: %u ms", GetTickCount() - t0);
    t.report();
}

}  // namespace zax
