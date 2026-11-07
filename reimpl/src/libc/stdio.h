#ifndef ZAX_LIBC_STDIO_H
#define ZAX_LIBC_STDIO_H
/* See zax_libc.h: the seek origins (vorbisfile's seek callback), FILE as an opaque type and the file
 * functions of ov_open */
#include "zax_libc.h"
typedef struct _iobuf FILE;
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
/* Only for ov_open, which the game does not use (the exe has no copy of it, /OPT:REF) */
static __inline size_t __cdecl fread(void *p, size_t n, size_t m, FILE *f)
{
    return ZAX_CRT(size_t(__cdecl *)(void *, size_t, size_t, FILE *), 0x5d1a3c)(p, n, m, f);
}
static __inline int __cdecl fclose(FILE *f) { return ZAX_CRT(int(__cdecl *)(FILE *), 0x5d1808)(f); }
static __inline long __cdecl ftell(FILE *f) { return ZAX_CRT(long(__cdecl *)(FILE *), 0x5d1d45)(f); }
static __inline int __cdecl fseek(FILE *f, long off, int whence)
{
    return ZAX_CRT(int(__cdecl *)(FILE *, long, int), 0x5d1c8c)(f, off, whence);
}
#endif
