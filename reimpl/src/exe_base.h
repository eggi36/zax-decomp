// Basic building blocks of Zax.exe that the reimplementation uses until they are reimplemented
// themselves: memory, strings, calls through the exe's vtables. Calls through the address layer's fn<> (reimpl.h).
#pragma once
#include "reimpl.h"
#include "zstring.h"

namespace zax {

// Memory of the exe. Small objects come from per-size pools (0x506470 returns the pool):
// operator_new_ecx 0x417710 (size in ecx), release 0x54d130 (pointer in ecx, size in edx).
// Large blocks and arrays come from the C heap (operator new 0x5d033a, delete 0x5d0320).
inline void *pool_alloc(unsigned n) { return fn<void *(__fastcall *)(unsigned)>(0x417710)(n); }
inline void pool_free(void *p, unsigned n) { fn<void(__fastcall *)(void *, unsigned)>(0x54d130)(p, n); }
inline void *heap_alloc(unsigned n) { return fn<void *(__cdecl *)(unsigned)>(0x5d033a)(n); }
inline void heap_free(void *p) { fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
// The global pool itself (0x506470, created on first use) and its methods (thiscall); sizes above the
// biggest pool size go to operator new/delete.
inline void *pool_get() { return fn<void *(__cdecl *)()>(0x506470)(); }
inline void *pool_get_alloc(unsigned n) { return fn<void *(THISCALL *)(void *, unsigned)>(0x5065b0)(pool_get(), n); }
inline void pool_get_free(void *p, unsigned n) { fn<void(THISCALL *)(void *, void *, unsigned)>(0x5065e0)(pool_get(), p, n); }

// C runtime of the exe (honours its locale)
inline int crt_stricmp(const char *a, const char *b) { return fn<int(__cdecl *)(const char *, const char *)>(0x5d3640)(a, b); }
inline int crt_isupper(int c) { return fn<int(__cdecl *)(int)>(0x5d0f29)(c); }
inline int crt_atexit(void(__cdecl *f)()) { return fn<int(__cdecl *)(void(__cdecl *)())>(0x5d0863)(f); }
inline int crt_atoi(const char *s) { return fn<int(__cdecl *)(const char *)>(0x5d03d3)(s); }
inline int crt_vsprintf(char *out, const char *fmt, va_list ap)
{
    return fn<int(__cdecl *)(char *, const char *, va_list)>(0x5d21ba)(out, fmt, ap);
}

// Text functions of the exe (0x587b30-0x587fa0; in place, register arguments)
inline void text_trim_left(char *s) { fn<void(__fastcall *)(char *)>(0x587b30)(s); }
inline void text_trim_right(char *s) { fn<void(__fastcall *)(char *)>(0x587b80)(s); }
inline void text_keep_chars(char *s, const char *chars) { fn<void(__fastcall *)(char *, const char *)>(0x587bc0)(s, chars); }
inline void text_upper(char *s) { fn<void(__fastcall *)(char *)>(0x587c00)(s); }
inline void text_replace_char(char *s, char from, char to) { fn<void(__fastcall *)(char *, char, char)>(0x587ce0)(s, from, to); }
inline void text_hex(char *out, const void *data, DWORD n) { fn<void(__fastcall *)(char *, const void *, DWORD)>(0x587fa0)(out, data, n); }
// 0x4a4570: the part behind the last '/' or '\\'; null gives "In Path Name Null!"
inline const char *path_name(const char *path) { return fn<const char *(__fastcall *)(const char *)>(0x4a4570)(path); }

// Call method number `slot` from the vtable of an exe object (__thiscall)
template <typename R, typename... A> inline R vcall(const void *obj, int slot, A... a)
{
    auto fn = (*reinterpret_cast<R(THISCALL *const *const *)(const void *, A...)>(obj))[slot];
    return fn(obj, a...);
}

}  // namespace zax
