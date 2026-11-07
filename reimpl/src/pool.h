// The pool allocator of Zax.exe (0x506360-0x5065e0): small blocks come from free lists per size class,
// larger ones from operator new. Almost every object of the exe is allocated through it
// (operator_new_ecx 0x417710, pool_delete 0x54d130; 204 callers of pool_get).
//
// The global pool (0x6c2d0c) is made on first use by pool_get (guard byte 0x6196b4, set to 1 in the data
// and cleared on first use; no atexit) with a largest size of 500 bytes: 126 size classes of 4 bytes each
// (class i holds blocks of i * 4 bytes, class 0 blocks of 4), every class grows by 4000 bytes' worth of
// blocks at a time. A block of n bytes comes from class (n + 3) / 4; above the largest size operator new
// and operator delete are used. Nothing is ever given back to the C heap while the game runs; 0x5064b0
// (not referenced by any code or table) would free the whole pool.
//
// Return values are what the original leaves in eax.
#pragma once
#include "exe_base.h"

namespace zax {

struct ZPoolClass {
    DWORD size;         // 0x00 block size (at least 4)
    void *chunks;       // 0x04 list of the chunks (operator new; the first dword links to the next)
    void *free_list;    // 0x08 free blocks (the first dword links to the next)
    DWORD per_chunk;    // 0x0c blocks per chunk
    DWORD total;        // 0x10 blocks made so far
    DWORD used;         // 0x14 blocks handed out

    ZPoolClass *THISCALL Ctor();               // 0x506360
    DWORD THISCALL Dtor();                     // 0x506380 frees the chunks only when no block is in use
    DWORD THISCALL SetSize(DWORD n);           // 0x5063b0
    DWORD THISCALL SetChunkBytes(DWORD n);     // 0x5063d0
    void *THISCALL Alloc();                    // 0x5063e0
    void *THISCALL Free(void *p);              // 0x506400
    DWORD THISCALL Grow();                     // 0x506420
};
static_assert(sizeof(ZPoolClass) == 0x18);

struct ZPool {
    DWORD max_size;        // 0x00 largest pooled size (rounded up to 4)
    DWORD num_classes;     // 0x04 max_size / 4 + 1
    ZPoolClass *classes;   // 0x08 vector new (the count in front)

    ZPool *THISCALL Ctor(DWORD max_size);      // 0x506510
    void *THISCALL Alloc(DWORD n);             // 0x5065b0
    DWORD THISCALL Free(void *p, DWORD n);     // 0x5065e0
};
static_assert(sizeof(ZPool) == 0xc);

ZPool *__cdecl Pool_Get();                       // 0x506470
void __cdecl Pool_Destroy();                     // 0x5064b0 (eax undefined)
DWORD __fastcall Pool_Delete(void *p, DWORD n);  // 0x54d130
void *__fastcall Pool_New(DWORD n);              // 0x417710 operator_new_ecx

constexpr DWORD POOL_GLOBAL = 0x6c2d0c, POOL_GUARD = 0x6196b4;

}  // namespace zax
