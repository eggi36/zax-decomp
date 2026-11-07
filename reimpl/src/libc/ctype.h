#ifndef ZAX_LIBC_CTYPE_H
#define ZAX_LIBC_CTYPE_H
/* See zax_libc.h: toupper 0x5d0c4e of the exe's C runtime ("C" locale) */
#include "zax_libc.h"
static __inline int __cdecl toupper(int c) { return ZAX_CRT(int(__cdecl *)(int), 0x5d0c4e)(c); }
#endif
