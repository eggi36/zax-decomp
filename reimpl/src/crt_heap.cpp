// The C runtime's heap entry points: operator new/delete, malloc, _nh_malloc, _heap_alloc, free,
// calloc, realloc, _msize, _callnewh (docs/DECOMPILATION.md 10, "C runtime").
//
// __heap_select (0x5d67ee, at startup) picks the heap: on Windows NT 5 and later (GetVersionExA: platform
// 2, major >= 5) always the system heap (1), so every block of the game comes from HeapAlloc on
// _crtheap and the two small-block heaps (V6 __sbh_*, V5 __old_sbh_*) are never used; only on Windows
// 9x/NT 4 they are (3, or 2 for old linker versions or __MSVCRT_HEAP_SELECT). Their branches are kept
// here as in the exe and call the exe's small-block functions under the heap lock.
// The system heap rounds every request up to 16 bytes (a request of 0 is 1), also in realloc and calloc.
#include "crt_common.h"

namespace zax::crt {

namespace {

constexpr DWORD SBH_FIND = 0x5d69db, SBH_FREE = 0x5d6a06, SBH_ALLOC = 0x5d6d2f, SBH_RESIZE = 0x5d71e4;
constexpr DWORD OLD_FIND = 0x5d7736, OLD_FREE = 0x5d778d, OLD_ALLOC = 0x5d77d2, OLD_RESIZE = 0x5d7afe;
constexpr DWORD MEMCPY = 0x5d8230, MEMSET = 0x5d8e60;

int active_heap() { return *glob<int *>(ACTIVE_HEAP); }
HANDLE crtheap() { return *glob<HANDLE *>(CRT_HEAP); }
size_t round16(size_t n) { return (n ? n : 1) + 15 & ~(size_t)15; }

void *sbh_find(void *p) { return exe_or_dead<void *(__cdecl *)(void *)>(SBH_FIND)(p); }
void sbh_free(void *header, void *p) { exe_or_dead<void(__cdecl *)(void *, void *)>(SBH_FREE)(header, p); }
void *sbh_alloc(size_t n) { return exe_or_dead<void *(__cdecl *)(size_t)>(SBH_ALLOC)(n); }
int sbh_resize(void *header, void *p, size_t n) { return exe_or_dead<int(__cdecl *)(void *, void *, size_t)>(SBH_RESIZE)(header, p, n); }
BYTE *old_find(void *p, void **region, void **page) { return exe_or_dead<BYTE *(__cdecl *)(void *, void **, void **)>(OLD_FIND)(p, region, page); }
void old_free(void *region, void *page, BYTE *map) { exe_or_dead<void(__cdecl *)(void *, void *, BYTE *)>(OLD_FREE)(region, page, map); }
void *old_alloc(size_t paras) { return exe_or_dead<void *(__cdecl *)(size_t)>(OLD_ALLOC)(paras); }
int old_resize(void *region, void *page, BYTE *map, size_t paras)
{
    return exe_or_dead<int(__cdecl *)(void *, void *, BYTE *, size_t)>(OLD_RESIZE)(region, page, map, paras);
}
void copy(void *d, const void *s, size_t n) { fn<void *(__cdecl *)(void *, const void *, size_t)>(MEMCPY)(d, s, n); }

void *heap_alloc_api(DWORD flags, size_t n) { return api<void *(WINAPI *)(HANDLE, DWORD, size_t)>(iat::HeapAlloc)(crtheap(), flags, n); }
void *heap_realloc_api(void *p, size_t n) { return api<void *(WINAPI *)(HANDLE, DWORD, void *, size_t)>(iat::HeapReAlloc)(crtheap(), 0, p, n); }

}  // namespace

// _callnewh 0x5d67a6: the new handler (_pnhHeap) decides whether to try again
int __cdecl callnewh(size_t n)
{
    auto h = *glob<int(__cdecl **)(size_t)>(PNH);
    return h && h(n) ? 1 : 0;
}

// _heap_alloc 0x5d168d
void *__cdecl heap_alloc(size_t n)
{
    const int heap = active_heap();
    if (heap == 3) {
        if (n <= *glob<size_t *>(SBH_THRESHOLD)) {
            lock(HEAP_LOCK);
            void *p = sbh_alloc(n);
            unlock(HEAP_LOCK);
            if (p)
                return p;
        }
    } else if (heap == 2) {
        n = n ? (n + 15) & ~(size_t)15 : 16;
        if (n <= *glob<size_t *>(OLD_SBH_THRESHOLD)) {
            lock(HEAP_LOCK);
            void *p = old_alloc(n >> 4);
            unlock(HEAP_LOCK);
            if (p)
                return p;
        }
        return heap_alloc_api(0, n);
    }
    return heap_alloc_api(0, round16(n));
}

// _nh_malloc 0x5d1661: requests above 0xffffffe0 fail at once; with nhFlag the new handler may retry
void *__cdecl nh_malloc(size_t n, int nh)
{
    if (n > 0xffffffe0u)
        return nullptr;
    for (;;) {
        if (void *p = fn<void *(__cdecl *)(size_t)>(0x5d168d)(n))   // _heap_alloc
            return p;
        if (!nh || !fn<int(__cdecl *)(size_t)>(0x5d67a6)(n))         // _callnewh
            return nullptr;
    }
}

// malloc 0x5d164f (_newmode decides the retry), operator new 0x5d033a (always retries)
void *__cdecl malloc_(size_t n) { return fn<void *(__cdecl *)(size_t, int)>(0x5d1661)(n, *glob<int *>(NEWMODE)); }
void *__cdecl operator_new(size_t n) { return fn<void *(__cdecl *)(size_t, int)>(0x5d1661)(n, 1); }

// free 0x5d33e0
void __cdecl free_(void *p)
{
    if (!p)
        return;
    const int heap = active_heap();
    if (heap == 3) {
        lock(HEAP_LOCK);
        void *header = sbh_find(p);
        if (header)
            sbh_free(header, p);
        unlock(HEAP_LOCK);
        if (header)
            return;
    } else if (heap == 2) {
        lock(HEAP_LOCK);
        void *region, *page;
        BYTE *map = old_find(p, &region, &page);
        if (map)
            old_free(region, page, map);
        unlock(HEAP_LOCK);
        if (map)
            return;
    }
    api<BOOL(WINAPI *)(HANDLE, DWORD, void *)>(iat::HeapFree)(crtheap(), 0, p);
}
// operator delete 0x5d0320
void __cdecl operator_delete(void *p) { exe_free(p); }

// calloc 0x5d34c9: num * size without an overflow check (32-bit product), zeroed
void *__cdecl calloc_(size_t num, size_t size)
{
    const size_t bytes = num * size;
    size_t n = bytes;
    if (n <= 0xffffffe0u)
        n = round16(n);
    for (;;) {
        void *p = nullptr;
        if (n <= 0xffffffe0u) {
            const int heap = active_heap();
            if (heap == 3 && bytes <= *glob<size_t *>(SBH_THRESHOLD)) {
                lock(HEAP_LOCK);
                p = sbh_alloc(bytes);
                unlock(HEAP_LOCK);
                if (p)
                    fn<void *(__cdecl *)(void *, int, size_t)>(MEMSET)(p, 0, bytes);
            } else if (heap == 2 && n <= *glob<size_t *>(OLD_SBH_THRESHOLD)) {
                lock(HEAP_LOCK);
                p = old_alloc(n >> 4);
                unlock(HEAP_LOCK);
                if (p)
                    fn<void *(__cdecl *)(void *, int, size_t)>(MEMSET)(p, 0, n);
            }
            if (!p)
                p = heap_alloc_api(8, n);   // HEAP_ZERO_MEMORY
        }
        if (p || !*glob<int *>(NEWMODE))
            return p;
        if (!fn<int(__cdecl *)(size_t)>(0x5d67a6)(n))
            return nullptr;
    }
}

// realloc 0x5d3745: null pointer = malloc, size 0 = free (returns null); in the small-block heaps a block
// that cannot grow in place moves (to a small block or HeapAlloc), the copy takes the smaller size
void *__cdecl realloc_(void *p, size_t n)
{
    if (!p)
        return exe_malloc(n);
    if (!n) {
        exe_free(p);
        return nullptr;
    }
    const int heap = active_heap();
    if (heap == 3) {
        for (;;) {
            void *r = nullptr;
            if (n <= 0xffffffe0u) {
                lock(HEAP_LOCK);
                void *header = sbh_find(p);
                if (header) {
                    if (n <= *glob<size_t *>(SBH_THRESHOLD)) {
                        if (sbh_resize(header, p, n))
                            r = p;
                        else if ((r = sbh_alloc(n))) {
                            const size_t old = *((DWORD *)p - 1) - 1;
                            copy(r, p, old < n ? old : n);
                            header = sbh_find(p);
                            sbh_free(header, p);
                        }
                    }
                    if (!r) {
                        n = round16(n);
                        if ((r = heap_alloc_api(0, n))) {
                            const size_t old = *((DWORD *)p - 1) - 1;
                            copy(r, p, old < n ? old : n);
                            sbh_free(header, p);
                        }
                    }
                }
                unlock(HEAP_LOCK);
                if (!header) {
                    n = round16(n);
                    r = heap_realloc_api(p, n);
                }
            }
            if (r || !*glob<int *>(NEWMODE))
                return r;
            if (!fn<int(__cdecl *)(size_t)>(0x5d67a6)(n))
                return nullptr;
        }
    }
    if (heap == 2) {
        if (n <= 0xffffffe0u)
            n = (n + 15) & ~(size_t)15;   // n > 0 here
        for (;;) {
            void *r = nullptr;
            if (n <= 0xffffffe0u) {
                lock(HEAP_LOCK);
                void *region, *page;
                BYTE *map = old_find(p, &region, &page);
                if (map) {
                    if (n < *glob<size_t *>(OLD_SBH_THRESHOLD)) {
                        if (old_resize(region, page, map, n >> 4))
                            r = p;
                        else if ((r = old_alloc(n >> 4))) {
                            const size_t old = (size_t)*map << 4;
                            copy(r, p, old < n ? old : n);
                            old_free(region, page, map);
                        }
                    }
                    if (!r && (r = heap_alloc_api(0, n))) {
                        const size_t old = (size_t)*map << 4;
                        copy(r, p, old < n ? old : n);
                        old_free(region, page, map);
                    }
                } else
                    r = heap_realloc_api(p, n);
                unlock(HEAP_LOCK);
            }
            if (r || !*glob<int *>(NEWMODE))
                return r;
            if (!fn<int(__cdecl *)(size_t)>(0x5d67a6)(n))
                return nullptr;
        }
    }
    for (;;) {
        void *r = nullptr;
        if (n <= 0xffffffe0u) {
            n = round16(n);
            r = heap_realloc_api(p, n);
        }
        if (r || !*glob<int *>(NEWMODE))
            return r;
        if (!fn<int(__cdecl *)(size_t)>(0x5d67a6)(n))
            return nullptr;
    }
}

// _msize 0x5d41ee: the small-block sizes (V6: the block header - 9; V5: paragraphs * 16), else HeapSize
size_t __cdecl msize(void *p)
{
    const int heap = active_heap();
    if (heap == 3) {
        lock(HEAP_LOCK);
        void *header = sbh_find(p);
        const size_t n = header ? *((DWORD *)p - 1) - 9 : 0;
        unlock(HEAP_LOCK);
        if (header)
            return n;
    } else if (heap == 2) {
        lock(HEAP_LOCK);
        void *region, *page;
        BYTE *map = old_find(p, &region, &page);
        const size_t n = map ? (size_t)*map << 4 : 0;
        unlock(HEAP_LOCK);
        if (map)
            return n;
    }
    return api<size_t(WINAPI *)(HANDLE, DWORD, void *)>(iat::HeapSize)(crtheap(), 0, p);
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_heap)
{
    const Hook list[] = {
        {0x5d0320, (void *)&operator_delete, "operator_delete", 6, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x50, 0xe8}},
        {0x5d033a, (void *)&operator_new, "operator_new", 6, {0x6a, 0x01, 0xff, 0x74, 0x24, 0x08, 0xe8, 0x1c}},
        {0x5d164f, (void *)&malloc_, "malloc", 6, {0xff, 0x35, 0x88, 0x59, 0x71, 0x00, 0xff, 0x74}},
        {0x5d1661, (void *)&nh_malloc, "_nh_malloc", 5, {0x83, 0x7c, 0x24, 0x04, 0xe0, 0x77, 0x22, 0xff}},
        {0x5d168d, (void *)&heap_alloc, "_heap_alloc", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x38, 0x6f}},
        {0x5d33e0, (void *)&free_, "free", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x90, 0x70}},
        {0x5d34c9, (void *)&calloc_, "calloc", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xa8, 0x70}},
        {0x5d3745, (void *)&realloc_, "realloc", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xc0, 0x70}},
        {0x5d41ee, (void *)&msize, "_msize", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x18, 0x71}},
        {0x5d67a6, (void *)&callnewh, "_callnewh", 5, {0xa1, 0x8c, 0x59, 0x71, 0x00, 0x85, 0xc0, 0x74}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
