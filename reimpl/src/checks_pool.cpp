// Checks of the pool allocator (pool.cpp) against the original, in the game thread. The pool serves every
// thread of the game, so its hooks stay in: side 0 calls the originals through their trampolines (nested
// calls of the pool's own methods reach the reimplementation, which these checks compare directly as well).
//  - Size classes on test objects: the constructor on patterned memory, then 300 sequences of SetSize
//    (0-600), SetChunkBytes (at least one block per chunk), Grow, Alloc and Free of random blocks, and the
//    destructor (blocks still out or none). Compared after every step: eax (block pointers as chunk number
//    and offset), size, blocks per chunk, made, used, the chunk count and the whole free list.
//  - Pools on test objects: the constructor with largest sizes 0-1000 (every class compared), random
//    Alloc/Free sequences of sizes 0 to the largest + 64 (pooled blocks as class, chunk and offset; larger
//    ones from the heap), and 0x5064b0 with the test pool in the global for the duration of the call.
//  - The real pool: pool_get (eax), and operator_new_ecx/pool_delete of the original and the
//    reimplementation crossed for every size 0-600 (a freed block must come back from the other side, the
//    used counts must stay balanced).
#include "checks_core_common.h"
#include "pool.h"

namespace zax {

namespace {

using namespace core_checks;

Sides g_s;

// ---- Pointer mapping ------------------------------------------------------------------------------------

struct ChunkMap {
    DWORD base[256];
    int n = 0;
    void build(const ZPoolClass *c)
    {
        n = 0;
        for (void *p = c->chunks; p && n < 256; p = *static_cast<void **>(p))
            base[n++] = ptrv(p);
    }
    // chunk number (in list order) * 0x100000 + offset, or 0xffffffff when the block is in none
    DWORD map(DWORD p) const
    {
        int best = -1;
        for (int k = 0; k < n; k++)
            if (base[k] <= p && (best < 0 || base[k] > base[best]))
                best = k;
        if (best < 0 || p - base[best] >= 0x100000)
            return p ? 0xffffffff : 0;
        return (DWORD)best * 0x100000 + (p - base[best]);
    }
};

void class_state(const ZPoolClass *c)
{
    ChunkMap m;
    m.build(c);
    g_s.st(c->size);
    g_s.st(c->per_chunk);
    g_s.st(c->total);
    g_s.st(c->used);
    g_s.st((DWORD)m.n);
    DWORD steps = 0;
    for (void *p = c->free_list; p && steps <= c->total + 1; p = *static_cast<void **>(p), steps++)
        g_s.st(m.map(ptrv(p)));
    g_s.st(steps);
}

void free_chunks(ZPoolClass *c)
{
    for (void *p = c->chunks; p;) {
        void *next = *static_cast<void **>(p);
        heap_free(p);
        p = next;
    }
    c->chunks = nullptr;
}

template <typename F> F pick(int side, DWORD address) { return side ? at<F>(address) : original<F>(address); }

// ---- Size classes ----------------------------------------------------------------------------------------

void check_classes(Tally &t)
{
    static const DWORD sizes[] = {0, 1, 2, 3, 4, 5, 7, 8, 12, 16, 24, 100, 252, 500, 600};
    for (int k = 0; k < 300; k++) {
        g_s.reset();
        for (int side = 0; side < 2; side++) {
            g_s.side = side;
            g_seed = 0x506360u + (unsigned)k * 7919u;
            ZPoolClass c;
            memset(&c, 0xa5, sizeof c);
            g_s.st(pick<ZPoolClass *(THISCALL *)(ZPoolClass *)>(side, 0x506360)(&c) == &c);
            class_state(&c);
            DWORD n = (random() >> 8) % 3 ? sizes[(random() >> 8) % 15] : (random() >> 8) % 601;
            g_s.st(pick<DWORD(THISCALL *)(ZPoolClass *, DWORD)>(side, 0x5063b0)(&c, n));
            const DWORD m = c.size * (1 + (random() >> 8) % 40) + (random() >> 8) % c.size;
            g_s.st(pick<DWORD(THISCALL *)(ZPoolClass *, DWORD)>(side, 0x5063d0)(&c, k % 5 ? m : 4000 < c.size ? c.size : 4000));
            class_state(&c);
            void *blocks[128];
            int nb = 0;
            ChunkMap map;
            const int ops = 10 + (int)((random() >> 8) % 120);
            for (int i = 0; i < ops; i++) {
                const DWORD r = (random() >> 8) % 100;
                if (r < 8) {
                    g_s.st(pick<DWORD(THISCALL *)(ZPoolClass *)>(side, 0x506420)(&c));
                } else if (r < 65 && nb < 128) {
                    void *p = pick<void *(THISCALL *)(ZPoolClass *)>(side, 0x5063e0)(&c);
                    map.build(&c);
                    g_s.st(map.map(ptrv(p)));
                    blocks[nb++] = p;
                } else if (nb) {
                    const int j = (int)((random() >> 8) % (DWORD)nb);
                    void *p = blocks[j];
                    blocks[j] = blocks[--nb];
                    map.build(&c);
                    g_s.st(map.map(ptrv(pick<void *(THISCALL *)(ZPoolClass *, void *)>(side, 0x506400)(&c, p))));
                }
                class_state(&c);
            }
            if (k % 3)
                while (nb)
                    pick<void *(THISCALL *)(ZPoolClass *, void *)>(side, 0x506400)(&c, blocks[--nb]);
            class_state(&c);
            const bool frees = !c.used && c.chunks;
            const DWORD r = pick<DWORD(THISCALL *)(ZPoolClass *)>(side, 0x506380)(&c);
            g_s.st(frees ? 1 : r);
            if (!frees)
                free_chunks(&c);
        }
        char d[48];
        wsprintfA(d, "size class sequence %d", k);
        g_s.check(t, d);
    }
}

// ---- Pools -------------------------------------------------------------------------------------------------

void pool_state(const ZPool *p)
{
    g_s.st(p->max_size);
    g_s.st(p->num_classes);
    g_s.st(reinterpret_cast<const DWORD *>(p->classes)[-1]);
    for (DWORD i = 0; i < p->num_classes; i++)
        class_state(&p->classes[i]);
}

void destroy_test_pool(ZPool *p, int side, bool through_global)
{
    if (through_global) {
        // 0x5064b0 works on the global: the test pool stands in for the duration of the call
        ZPool *&g = *at<ZPool **>(POOL_GLOBAL);
        ZPool *saved = g;
        g = p;
        pick<void(__cdecl *)()>(side, 0x5064b0)();
        const DWORD after = ptrv(g);
        g = saved;
        g_s.st(after);
        return;
    }
    for (DWORD i = p->num_classes; i; i--)
        if (!p->classes[i - 1].used)
            p->classes[i - 1].Dtor();
        else
            free_chunks(&p->classes[i - 1]);
    heap_free(reinterpret_cast<DWORD *>(p->classes) - 1);
    heap_free(p);
}

void check_pools(Tally &t)
{
    static const DWORD maxes[] = {0, 1, 3, 4, 5, 8, 15, 64, 100, 255, 500, 501, 1000};
    for (int k = 0; k < 130; k++) {
        g_s.reset();
        const DWORD max = maxes[k % 13];
        for (int side = 0; side < 2; side++) {
            g_s.side = side;
            g_seed = 0x506510u + (unsigned)k * 104729u;
            ZPool *p = static_cast<ZPool *>(heap_alloc(sizeof(ZPool)));
            memset(p, 0x5a, sizeof *p);
            g_s.st(pick<ZPool *(THISCALL *)(ZPool *, DWORD)>(side, 0x506510)(p, max) == p);
            pool_state(p);
            struct Block {
                void *p;
                DWORD n;
            } blocks[64];
            int nb = 0;
            for (int i = 0; i < 60; i++) {
                if ((random() >> 8) % 100 < 60 && nb < 64) {
                    const DWORD r = (random() >> 8) % 8;
                    const DWORD n = r == 0 ? 0 : r == 1 ? max : r == 2 ? max + 1 : (random() >> 8) % (max + 65);
                    void *b = pick<void *(THISCALL *)(ZPool *, DWORD)>(side, 0x5065b0)(p, n);
                    if (n <= max) {
                        ChunkMap m;
                        m.build(&p->classes[(n + 3) / 4]);
                        g_s.st(m.map(ptrv(b)));
                    } else {
                        g_s.st(b ? 0x4ea9 : 0);
                    }
                    blocks[nb++] = {b, n};
                } else if (nb) {
                    const int j = (int)((random() >> 8) % (DWORD)nb);
                    const Block b = blocks[j];
                    blocks[j] = blocks[--nb];
                    const DWORD r = pick<DWORD(THISCALL *)(ZPool *, void *, DWORD)>(side, 0x5065e0)(p, b.p, b.n);
                    if (b.n <= max) {
                        ChunkMap m;
                        m.build(&p->classes[(b.n + 3) / 4]);
                        g_s.st(m.map(r));
                    }
                }
                pool_state(p);
            }
            while (nb) {
                const Block b = blocks[--nb];
                pick<DWORD(THISCALL *)(ZPool *, void *, DWORD)>(side, 0x5065e0)(p, b.p, b.n);
            }
            pool_state(p);
            destroy_test_pool(p, side, k % 2 == 1);
        }
        char d[48];
        wsprintfA(d, "pool sequence %d (largest %u)", k, max);
        g_s.check(t, d);
    }
}

// ---- The real pool -----------------------------------------------------------------------------------------

void check_real_pool(Tally &t)
{
    {
        const DWORD a = ptrv(original<ZPool *(__cdecl *)()>(0x506470)()), b = ptrv(at<ZPool *(__cdecl *)()>(0x506470)());
        char d[64];
        wsprintfA(d, "[pool_get %x %x]", a, b);
        t.check(a == b && a == *at<DWORD *>(POOL_GLOBAL), "", d);
    }
    using NewFn = void *(__fastcall *)(DWORD);
    using DelFn = DWORD(__fastcall *)(void *, DWORD);
    ZPool *pool = *at<ZPool **>(POOL_GLOBAL);
    int bad = 0;
    for (DWORD n = 0; n <= 600; n++) {
        const bool pooled = n <= pool->max_size;
        const DWORD used = pooled ? pool->classes[(n + 3) / 4].used : 0;
        void *p = original<NewFn>(0x417710)(n);
        at<DelFn>(0x54d130)(p, n);
        void *q = at<NewFn>(0x417710)(n);
        original<DelFn>(0x54d130)(q, n);
        void *r = original<NewFn>(0x417710)(n);
        const DWORD used_mid = pooled ? pool->classes[(n + 3) / 4].used : 0;
        at<DelFn>(0x54d130)(r, n);
        const DWORD used_after = pooled ? pool->classes[(n + 3) / 4].used : 0;
        const bool same = !pooled || (p == q && q == r && used_mid == used + 1 && used_after == used);
        if (!same && bad++ < 5) {
            char d[96];
            wsprintfA(d, "[new/delete crossed, %u bytes: %x %x %x, used %u %u %u]", n, ptrv(p), ptrv(q), ptrv(r), used, used_mid,
                      used_after);
            t.check(false, "", d);
        } else {
            t.check(same, "");
        }
    }
}

}  // namespace

int hooks_pool(Hook *out);

ZAX_CHECKS(pool)
{
    (void)cls;
    (void)n;
    {
        Tally t{"pool: size classes"};
        check_classes(t);
        t.report();
    }
    {
        Tally t{"pool: pools, 0x5064b0"};
        check_pools(t);
        t.report();
    }
    {
        Tally t{"pool: the real pool"};
        check_real_pool(t);
        t.report();
    }
}

}  // namespace zax
