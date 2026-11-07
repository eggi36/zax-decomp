// Checks of CHueChangeEffect and the hue remap cache (hue_effect.cpp) against the original, in the game thread.
//  - The class (machinery: checks_inventory_common.h): Create, CreateArray, the deleting destructor, the getter,
//    the registered class.
//  - The cache is game state: every case swaps the game's array for an empty one of its own (the same on both
//    sides), runs on it and frees what it made before the game's array is put back.
//    Get: 1-12 calls with holders from a small set of six floats (repeated values, the real holders of the
//    level's entities, NaN, zero); compared: the index of every returned entry, the count, every entry's key,
//    the blend mode's hue values, palette and 64K table (hashed). Clear: 0-40 entries (constructed blend modes,
//    no tables built); compared: the count left. The cache's deleting destructor on an array of its own with
//    0-5 entries and both flag values; compared: the array's dwords afterwards, eax.
#include "checks_inventory_common.h"
#include "ai_helpers.h"

namespace zax {

int hooks_hue_effect(Hook *out);

namespace {

using namespace ai;

const invcheck::ClassDef CLASSES[] = {
    {"CHueChangeEffect", 8, 0x6c0508, 0x4d42a0, 0x4d42d0, 0, {0x4d43b0, 0}, true},
};

constexpr DWORD CACHE = 0x6c0510, CACHE_GUARD = 0x6c0524;

struct Hooks {
    bool out_ = false;
    void out()
    {
        invcheck::g_hook_table = &hooks_hue_effect;
        invcheck::hooks_out();
    }
    void in() { invcheck::hooks_in(); }
};
Hooks g_hooks;

// The game's array swapped for an empty one while a side runs
struct CacheSwap {
    DWORD saved[4];
    BYTE guard;
    CacheSwap()
    {
        guard = *at<BYTE *>(CACHE_GUARD);
        memcpy(saved, at<void *>(CACHE), 16);
        *at<BYTE *>(CACHE_GUARD) |= 1;
        DWORD *c = at<DWORD *>(CACHE);
        c[0] = 0x5f7c6c;
        c[1] = c[2] = c[3] = 0;
    }
    // frees the entries and the block left in the swapped array, then puts the game's back
    ~CacheSwap()
    {
        DWORD *c = at<DWORD *>(CACHE);
        for (DWORD i = 0; i < c[2]; i++)
            if (BYTE *e = bp(reinterpret_cast<DWORD *>(bp(c[1]))[i])) {
                at<void(THISCALL *)(void *)>(0x43c0b0)(e + 0x18);
                heap_free(e);
            }
        if (c[1])
            pool_get_free(bp(c[1]), c[3] << 2);
        memcpy(at<void *>(CACHE), saved, 16);
        *at<BYTE *>(CACHE_GUARD) = guard;
    }
};

unsigned entry_hash(BYTE *e)
{
    unsigned h = fnv(2166136261u, e, 0x18);
    BYTE *m = e + 0x18;
    h = fnv(h, m + 0x34, 12);
    // the palette object at +4: {data, ...}; 256 entries
    if (BYTE *pal = bp(dw(m, 4)))
        h = fnv(h, pal, 0x100);
    if (BYTE *t = bp(dw(m, 0x44)))
        h = fnv(h, t, 0x20000);
    return h;
}

float g_values[16];
void make_values()
{
    g_seed = 0x4d4ce0;
    for (int i = 0; i < 16; i++) {
        const DWORD r = random() % 8;
        g_values[i] = r == 0 ? 0.0f : r == 1 ? (float)(random() % 360) : r == 2 ? (float)(random() % 1000) / 999.0f : (float)(random() % 60);
    }
    g_values[15] = raw_float(0x7fc00000);   // NaN
}

// The real holders of the level's entities (dynamic field 0x6c0520 through 0x4167f0), at most 8
BYTE *g_holders[8];
int g_nholders;

void check_get(Tally &t, int real)
{
    for (int k = 0; k < scaled(60); k++) {
        float keys[12][6];
        const int calls = 1 + (int)(random() % 12);
        const int pool = 1 + (int)(random() % 4);
        float base[4][6];
        for (int p = 0; p < pool; p++)
            for (int j = 0; j < 6; j++) {
                if (real && g_nholders && random() % 2)
                    base[p][j] = fl<float>(g_holders[random() % g_nholders], 8 + 4 * j);
                else
                    base[p][j] = g_values[random() % (random() % 6 ? 15 : 16)];
            }
        for (int c = 0; c < calls; c++)
            memcpy(keys[c], base[random() % pool], 24);
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                g_hooks.out();
            {
                CacheSwap swap;
                unsigned hh = 2166136261u;
                BYTE holder[0x20];
                memset(holder, 0x5a, sizeof holder);
                for (int c = 0; c < calls; c++) {
                    memcpy(holder + 8, keys[c], 24);
                    BYTE *m = at<BYTE *(__fastcall *)(void *)>(0x4d4ce0)(holder);
                    DWORD *c4 = at<DWORD *>(CACHE);
                    DWORD idx = 0xffffffff;
                    for (DWORD i = 0; i < c4[2]; i++)
                        if (bp(reinterpret_cast<DWORD *>(bp(c4[1]))[i]) + 0x18 == m)
                            idx = i;
                    hh = fnv(hh, &idx, 4);
                }
                DWORD *c4 = at<DWORD *>(CACHE);
                hh = fnv(hh, &c4[2], 4);
                for (DWORD i = 0; i < c4[2]; i++) {
                    const unsigned eh = entry_hash(bp(reinterpret_cast<DWORD *>(bp(c4[1]))[i]));
                    hh = fnv(hh, &eh, 4);
                }
                h[side] = hh;
            }
            if (side == 0)
                g_hooks.in();
        }
        char d[96];
        wsprintfA(d, "[Get case %d: %d calls, %x / %x]", k, calls, h[0], h[1]);
        t.check(h[0] == h[1], "", d);
    }
}

void add_constructed(DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = static_cast<BYTE *>(heap_alloc(0x60));
        memset(e, 0, 0x18);
        at<void *(THISCALL *)(void *)>(0x43bfe0)(e + 0x18);
        BYTE *cache = at<BYTE *>(CACHE);
        vcall<void>(cache, 2, &e);
    }
}

void check_clear(Tally &t)
{
    static const DWORD COUNTS[] = {0, 1, 2, 31, 32, 33, 40};
    for (DWORD n : COUNTS) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                g_hooks.out();
            {
                CacheSwap swap;
                add_constructed(n);
                at<void(__cdecl *)()>(0x4d4a50)();
                r[side] = at<DWORD *>(CACHE)[2] | (*at<BYTE *>(CACHE_GUARD) & 1) << 16;
            }
            if (side == 0)
                g_hooks.in();
        }
        char d[64];
        wsprintfA(d, "[Clear %d: %x / %x]", n, r[0], r[1]);
        t.check(r[0] == r[1], "", d);
    }
    // the static's first use: the guard cleared, atexit a recording stand-in
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            g_hooks.out();
        {
            CacheSwap swap;
            *at<BYTE *>(CACHE_GUARD) &= ~1;
            DWORD *c = at<DWORD *>(CACHE);
            c[0] = c[1] = c[2] = c[3] = 0x77777777;
            CodePatch p;
            g_atexit_calls = g_atexit_function = 0;
            patch_jump(p, 0x5d0863, reinterpret_cast<void *>(&fake_atexit));
            at<void(__cdecl *)()>(0x4d4a50)();
            unpatch(p);
            r[side] = fnv(fnv(2166136261u, c, 16), &g_atexit_function, 4) ^ (*at<BYTE *>(CACHE_GUARD) & 1) ^ g_atexit_calls << 8;
        }
        if (side == 0)
            g_hooks.in();
    }
    char d[64];
    wsprintfA(d, "[first use: %x / %x]", r[0], r[1]);
    t.check(r[0] == r[1], "", d);
}

void check_dtor(Tally &t)
{
    for (DWORD n = 0; n < 6; n++)
        for (DWORD flags = 0; flags < 2; flags++) {
            DWORD r[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    g_hooks.out();
                {
                    CacheSwap swap;
                    add_constructed(n);
                    DWORD *c = at<DWORD *>(CACHE);
                    BYTE *self = static_cast<BYTE *>(heap_alloc(16));
                    memcpy(self, c, 16);
                    c[1] = c[2] = c[3] = 0;
                    BYTE *ret = at<BYTE *(THISCALL *)(void *, DWORD)>(0x4d4b20)(self, flags);
                    DWORD v[5] = {(DWORD)(ret == self)};
                    if (!(flags & 1)) {
                        memcpy(v + 1, self, 16);
                        heap_free(self);
                    }
                    r[side] = fnv(2166136261u, v, flags & 1 ? 4 : 20);
                }
                if (side == 0)
                    g_hooks.in();
            }
            char d[64];
            wsprintfA(d, "[dtor %d entries flags %d: %x / %x]", n, flags, r[0], r[1]);
            t.check(r[0] == r[1], "", d);
        }
}

void find_holders()
{
    g_nholders = 0;
    // the cache's own entries carry the keys of every holder drawn so far
    DWORD *c = at<DWORD *>(CACHE);
    if (!(*at<BYTE *>(CACHE_GUARD) & 1))
        return;
    for (DWORD i = 0; i < c[2] && g_nholders < 8; i++) {
        BYTE *e = bp(reinterpret_cast<DWORD *>(bp(c[1]))[i]);
        if (e && invcheck::readable(e, 0x18))
            g_holders[g_nholders++] = e - 8;   // the key lies where a holder's +8 would be
    }
}

}  // namespace

ZAX_CHECKS(hue_effect)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = &hooks_hue_effect;
    if (!invcheck::arena_setup()) {
        log("Check hue effect: no arena");
        return;
    }
    Tally objects{"hue effect: Create, CreateArray, destructors"}, getters{"hue effect: class getters"}, fields{"hue effect: RegisterFields"},
        registered{"hue effect: registered classes (real data)"};
    invcheck::test_classes(objects, getters, fields, registered, CLASSES, 1);
    objects.report();
    getters.report();
    registered.report();
    make_values();
    find_holders();
    log("  hue effect: %d cached remaps of the level", g_nholders);
    Tally get{"hue effect: HueRemapCache_Get on a swapped cache"};
    check_get(get, 1);
    get.report();
    Tally clear{"hue effect: HueRemapCache_Clear and the cache's destructor"};
    check_clear(clear);
    check_dtor(clear);
    clear.report();
}

}  // namespace zax
