#ifndef ZAX_LIBC_STDLIB_H
#define ZAX_LIBC_STDLIB_H
/* See zax_libc.h */
#include "zax_libc.h"
#ifndef NULL
#define NULL ((void *)0)
#endif

/* qsort 0x5d097c. Its comparator here has the default convention of the libraries (__fastcall, /Gr), so
 * the exe's cdecl qsort calls it through a cdecl adapter. The only caller is lsp.c's vorbis_lpc_to_lsp
 * (encoder only); in the exe its comparator comp 0x5e9430 is __cdecl itself (see libs/vorbis_lsp.c). */
static int (*zax_qsort_cmp)(const void *, const void *);
static int __cdecl zax_qsort_adapter(const void *a, const void *b) { return zax_qsort_cmp(a, b); }
static __inline void __cdecl qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    zax_qsort_cmp = cmp;
    ZAX_CRT(void(__cdecl *)(void *, size_t, size_t, int(__cdecl *)(const void *, const void *)), 0x5d097c)(base, n, size,
                                                                                                          zax_qsort_adapter);
}

/* labs: MSVC 6 compiles it inline (/Oi) */
static __inline long __cdecl labs(long x) { return x < 0 ? -x : x; }
#endif
