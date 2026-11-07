// The pool audit after every check module (checks.cpp). A check that lets memory of its own (an arena, a
// block of a fake world) reach the game's pool allocator poisons it: the pool gives those blocks out
// later, to the live game or to a later check, and the next case that reuses the memory overwrites them.
// Two ways were found (2026-10-28): a pool class grows while operator new is redirected (the new chunk
// lands in an arena or in a stub's block), and a block of such memory is freed into a pool class.
//
// The check memories are registered (check_region); after every module the audit walks every size class:
// chunks in check memory and free blocks in check memory are reported as "Pool audit after <module>:
// MISMATCH" and the free blocks are taken out of the free list, so later checks and the game do not get
// them. Free blocks outside the class's chunks in other memory (C heap blocks that code freed into the
// pool) are only counted: the game may do that itself.
#include "check_util.h"
#include "pool.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
extern "C" char __ImageBase;   // this DLL (its static fake objects and buffers are check memory too)

namespace zax {

namespace {

struct MemInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};

bool readable4(DWORD p)
{
    MemInfo mi;
    if (p < 0x10000 || p >= 0x80000000u || (p & 3))
        return false;
    return VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(p)), &mi, sizeof mi) && mi.state == 0x1000 &&
           !(mi.protect & 0x100) && (mi.protect & 0x66);
}

struct Region {
    DWORD start, end;
    const char *name;
};
constexpr int MAX_REGIONS = 64, MAX_CHUNKS = 0x8000;
Region g_regions[MAX_REGIONS];
int g_nregions;
DWORD g_starts[MAX_CHUNKS];
int g_heap_foreign = -1;   // free blocks outside their chunks in other memory, at the last audit

const Region *region_of(DWORD p)
{
    for (int i = 0; i < g_nregions; i++)
        if (p >= g_regions[i].start && p < g_regions[i].end)
            return &g_regions[i];
    return nullptr;
}

void sort(DWORD *a, int n)   // shell sort
{
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++) {
            const DWORD v = a[i];
            int j = i;
            for (; j >= gap && a[j - gap] > v; j -= gap)
                a[j] = a[j - gap];
            a[j] = v;
        }
}

}  // namespace

bool pool_grow_call(DWORD ret, DWORD n)
{
    const DWORD f = (DWORD)(uintptr_t)ptr(&ZPoolClass::Grow);
    if ((ret >= f && ret < f + 0x100) || (ret >= 0x506420 && ret < 0x506470))
        return true;
    for (DWORD size = 4; size <= 500; size += 4)
        if (n == size * (4000 / size) + 4)
            return true;
    return false;
}

void check_region(const void *base, DWORD size, const char *name)
{
    const DWORD b = (DWORD)(uintptr_t)base;
    if (!b || region_of(b) || g_nregions >= MAX_REGIONS)
        return;
    g_regions[g_nregions++] = {b, b + size, name};
}

void pool_audit(const char *module)
{
    if (g_heap_foreign < 0) {
        const BYTE *image = reinterpret_cast<const BYTE *>(&__ImageBase);
        const DWORD nt = *reinterpret_cast<const DWORD *>(image + 0x3c);
        check_region(image, *reinterpret_cast<const DWORD *>(image + nt + 0x50), "Zax-Reimpl.dll");
    }
    ZPool *pool = *at<ZPool **>(POOL_GLOBAL);
    if (!pool || !pool->classes)
        return;
    int bad_chunks = 0, bad_free = 0, heap_foreign = 0;
    DWORD first_chunk = 0, first_free = 0, first_free_size = 0, first_heap = 0;
    const char *chunk_region = "", *free_region = "";
    for (DWORD c = 0; c < pool->num_classes; c++) {
        ZPoolClass &k = pool->classes[c];
        const DWORD size = k.size ? k.size : 4, bytes = k.size * k.per_chunk;
        int n = 0;
        DWORD seen = 0;
        for (DWORD ch = (DWORD)(uintptr_t)k.chunks; ch && seen < 0x100000 && readable4(ch); ch = *at<DWORD *>(ch), seen++) {
            if (const Region *r = region_of(ch)) {
                if (!bad_chunks++) {
                    first_chunk = ch;
                    chunk_region = r->name;
                }
                continue;
            }
            if (n < MAX_CHUNKS)
                g_starts[n++] = ch + 4;
        }
        sort(g_starts, n);
        // the free list; blocks in check memory unlinked
        DWORD *link = reinterpret_cast<DWORD *>(&k.free_list);
        DWORD walked = 0;
        while (*link && walked++ <= k.total + 0x10000) {
            const DWORD p = *link;
            if (const Region *r = region_of(p)) {
                if (!bad_free++) {
                    first_free = p;
                    first_free_size = k.size;
                    free_region = r->name;
                }
                // its link is check memory too, but still the pool's own while the region is not reused
                *link = readable4(p) ? *at<DWORD *>(p) : 0;
                continue;
            }
            if (!readable4(p)) {   // a broken link: the rest of the list is cut off
                if (!bad_free++) {
                    first_free = p;
                    first_free_size = k.size;
                    free_region = "(unreadable)";
                }
                *link = 0;
                break;
            }
            int lo = 0, hi = n - 1, at_ = -1;
            while (lo <= hi) {   // the last chunk start <= p
                const int mid = (lo + hi) / 2;
                if (g_starts[mid] <= p) {
                    at_ = mid;
                    lo = mid + 1;
                } else {
                    hi = mid - 1;
                }
            }
            if (!(at_ >= 0 && p < g_starts[at_] + bytes && (p - g_starts[at_]) % size == 0) && !heap_foreign++)
                first_heap = p;
            link = at<DWORD *>(p);
        }
    }
    if (bad_chunks || bad_free)
        log("Pool audit after %s: MISMATCH (%d chunks in check memory, first %x in %s; %d free blocks in check memory taken out, "
            "first %x of size %u in %s)",
            module, bad_chunks, first_chunk, chunk_region, bad_free, first_free, first_free_size, free_region);
    if (heap_foreign != g_heap_foreign && g_heap_foreign >= 0)
        log("  pool audit after %s: %d free blocks outside their class's chunks (was %d), first %x", module, heap_foreign,
            g_heap_foreign, first_heap);
    g_heap_foreign = heap_foreign;
}

}  // namespace zax
