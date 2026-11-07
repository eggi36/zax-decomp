/* Shared by the wrappers in libs/: each builds one source file of zlib 1.1.3, libogg or libvorbis
 * 1.0 beta 4 (reimpl/third_party) unchanged and exports the addresses of the functions Zax.exe has a
 * copy of, for the hook tables (zlib_hooks.inc, vorbis_hooks.inc). The static functions are hooked as
 * well, so every wrapper includes its source file instead of compiling it on its own.
 *
 * Calling convention: the exe's libraries were compiled with /Gr (__fastcall as the default,
 * docs/DECOMPILATION.md 10), and so are these (build.sh: -fdefault-calling-conv=fastcall). clang
 * applies that default only when the target has SSE2, so build.sh enables SSE2 for the translation
 * unit and the "#pragma clang attribute" around the source turns it off again for every function:
 * the code is x87 only, like the exe's and the rest of the DLL (-mno-sse). */
#ifndef ZAX_LIBS_H
#define ZAX_LIBS_H
#define ZAXLIB_EXPORT(f) void *const zaxlib_##f = (void *)&f
#endif
