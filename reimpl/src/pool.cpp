// The pool allocator (see pool.h).
#include "pool.h"

namespace zax {

// replay.cpp: [Reimpl] PoolLog=<n>, PoolLogSize=<bytes> (diagnosis, batch 6-12): set while pool calls are logged
extern DWORD g_pool_log_size;
void pool_line(char op, DWORD n, const void *p);

// 0x506360
ZPoolClass *THISCALL ZPoolClass::Ctor()
{
    size = 0;
    free_list = nullptr;
    per_chunk = 0x40;
    total = 0;
    used = 0;
    chunks = nullptr;
    return this;
}

// 0x506380. eax: the count in use when blocks are out, 0 without chunks, otherwise operator delete's.
DWORD THISCALL ZPoolClass::Dtor()
{
    if (used)
        return used;
    DWORD r = 0;
    for (void *c = chunks; c;) {
        void *next = *static_cast<void **>(c);
        r = fn<DWORD(__cdecl *)(void *)>(0x5d0320)(c);
        c = next;
    }
    return r;
}

// 0x5063b0
DWORD THISCALL ZPoolClass::SetSize(DWORD n)
{
    size = n < 4 ? 4 : n;
    return n;
}

// 0x5063d0
DWORD THISCALL ZPoolClass::SetChunkBytes(DWORD n) { return per_chunk = n / size; }

// 0x5063e0
void *THISCALL ZPoolClass::Alloc()
{
    if (!free_list)
        Grow();
    used++;
    void *p = free_list;
    free_list = *static_cast<void **>(p);
    return p;
}

// 0x506400
void *THISCALL ZPoolClass::Free(void *p)
{
    used--;
    *static_cast<void **>(p) = free_list;
    free_list = p;
    return p;
}

// 0x506420: a new chunk in front of the list, its blocks linked from the first to the last (built from the
// last block down). eax: the blocks per chunk.
DWORD THISCALL ZPoolClass::Grow()
{
    const DWORD bytes = size * per_chunk;
    void **chunk = static_cast<void **>(heap_alloc(bytes + 4));
    chunk[0] = chunks;
    chunks = chunk;
    const DWORD first = (DWORD)(uintptr_t)(chunk + 1);
    DWORD next = 0;
    for (DWORD b = first - size + bytes; b >= first; b -= size) {   // unsigned, as in the original
        *at<DWORD *>(b) = next;
        next = b;
    }
    free_list = at<void *>(next);
    total += per_chunk;
    return per_chunk;
}

// 0x506510
ZPool *THISCALL ZPool::Ctor(DWORD max)
{
    max_size = (max + 3) & ~3u;
    num_classes = (max_size + 3) / 4 + 1;
    DWORD *block = static_cast<DWORD *>(heap_alloc(num_classes * sizeof(ZPoolClass) + 4));
    if (block) {
        block[0] = num_classes;
        classes = reinterpret_cast<ZPoolClass *>(block + 1);
        for (DWORD i = 0; i < num_classes; i++)
            classes[i].Ctor();
    } else {
        classes = nullptr;
    }
    for (DWORD i = 0; i < num_classes; i++) {
        classes[i].SetSize(i * 4);
        classes[i].SetChunkBytes(4000);
    }
    return this;
}

// 0x5065b0
void *THISCALL ZPool::Alloc(DWORD n)
{
    if (n > max_size)
        return heap_alloc(n);
    void *p = classes[(n + 3) / 4].Alloc();
    if (g_pool_log_size)
        pool_line('a', n, p);
    return p;
}

// 0x5065e0. eax: operator delete's or the block.
DWORD THISCALL ZPool::Free(void *p, DWORD n)
{
    if (n > max_size)
        return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p);
    if (g_pool_log_size)
        pool_line('f', n, p);
    return (DWORD)(uintptr_t)classes[(n + 3) / 4].Free(p);
}

// 0x506470
ZPool *__cdecl Pool_Get()
{
    BYTE &guard = *glob<BYTE *>(POOL_GUARD);
    ZPool *&pool = *glob<ZPool **>(POOL_GLOBAL);
    if (!guard)
        return pool;
    guard = 0;
    ZPool *p = static_cast<ZPool *>(heap_alloc(sizeof(ZPool)));
    return pool = p ? p->Ctor(500) : nullptr;
}

// 0x5064b0: every size class destroyed from the last, the classes and the pool freed, the global cleared
void __cdecl Pool_Destroy()
{
    ZPool *&pool = *glob<ZPool **>(POOL_GLOBAL);
    if (ZPool *p = pool) {
        if (ZPoolClass *c = p->classes) {
            DWORD *block = reinterpret_cast<DWORD *>(c) - 1;
            for (DWORD i = *block; (int)i > 0; i--)
                c[i - 1].Dtor();
            heap_free(block);
        }
        p->classes = nullptr;
        heap_free(p);
    }
    pool = nullptr;
}

// 0x54d130
DWORD __fastcall Pool_Delete(void *p, DWORD n) { return Pool_Get()->Free(p, n); }

// 0x417710
void *__fastcall Pool_New(DWORD n) { return Pool_Get()->Alloc(n); }

ZAX_HOOKS(pool)
{
    const Hook list[] = {
        {0x417710, (void *)&Pool_New, "Pool_New", 6, {0x51, 0xe8, 0x5a, 0xed, 0x0e, 0x00, 0x8b, 0xc8}, 2},
        {0x506360, ptr(&ZPoolClass::Ctor), "ZPoolClass::Ctor", 6, {0x8b, 0xc1, 0x33, 0xc9, 0x89, 0x08, 0x89, 0x48}},
        {0x506380, ptr(&ZPoolClass::Dtor), "ZPoolClass::Dtor", 6, {0x8b, 0x41, 0x14, 0x56, 0x85, 0xc0, 0x75, 0x18}},
        {0x5063b0, ptr(&ZPoolClass::SetSize), "ZPoolClass::SetSize", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0x04, 0x89}},
        {0x5063d0, ptr(&ZPoolClass::SetChunkBytes), "ZPoolClass::SetChunkBytes", 6, {0x8b, 0x44, 0x24, 0x04, 0x33, 0xd2, 0xf7, 0x31}},
        {0x5063e0, ptr(&ZPoolClass::Alloc), "ZPoolClass::Alloc", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x08, 0x85, 0xc0}},
        {0x506400, ptr(&ZPoolClass::Free), "ZPoolClass::Free", 6, {0x8b, 0x41, 0x14, 0x8b, 0x51, 0x08, 0x48, 0x89}},
        {0x506420, ptr(&ZPoolClass::Grow), "ZPoolClass::Grow", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x3e, 0x0f}},
        {0x506470, (void *)&Pool_Get, "Pool_Get", 5, {0xa0, 0xb4, 0x96, 0x61, 0x00, 0x84, 0xc0, 0x74}},
        {0x5064b0, (void *)&Pool_Destroy, "Pool_Destroy", 7, {0x55, 0x8b, 0x2d, 0x0c, 0x2d, 0x6c, 0x00, 0x57}},
        {0x506510, ptr(&ZPool::Ctor), "ZPool::Ctor", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x83, 0xc0, 0x03}},
        {0x5065b0, ptr(&ZPool::Alloc), "ZPool::Alloc", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x11, 0x3b, 0xc2}},
        {0x5065e0, ptr(&ZPool::Free), "ZPool::Free", 6, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x11, 0x3b, 0xc2}},
        {0x54d130, (void *)&Pool_Delete, "Pool_Delete", 7, {0x52, 0x51, 0xe8, 0x39, 0x93, 0xfb, 0xff, 0x8b}, 3},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
