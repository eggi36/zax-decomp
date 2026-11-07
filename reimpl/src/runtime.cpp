// What the compiler expects without a C runtime. Memory comes from the Zax.exe heap so objects can
// change owners between original and reimplemented code.
#include "reimpl.h"

extern "C" {

int _fltused = 0;   // referenced by the compiler as soon as floating point is used

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    for (; n--; x++, y++)
        if (*x != *y)
            return *x < *y ? -1 : 1;
    return 0;
}

int __cdecl _purecall(void)
{
    zax::log("pure virtual method called");
    return 0;
}

}  // extern "C"

// 0x5d033a operator new, 0x5d0320 operator delete of Zax.exe (VC++ 6 C runtime)
void *operator new(size_t n) { return zax::fn<void *(__cdecl *)(size_t)>(0x5d033a)(n); }
void *operator new[](size_t n) { return zax::fn<void *(__cdecl *)(size_t)>(0x5d033a)(n); }
void operator delete(void *p) noexcept { zax::fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
void operator delete[](void *p) noexcept { zax::fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
void operator delete(void *p, size_t) noexcept { zax::fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
void operator delete[](void *p, size_t) noexcept { zax::fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
