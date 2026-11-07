// The C runtime's string, memory, character class and conversion functions, the per-thread data, errno,
// the locks, qsort, rand and the vector constructor iterator (docs/DECOMPILATION.md 10, "C runtime").
// The behaviour comes from crt_portable.h where a portable version exists; the hooked functions here add
// the exe's state (errno in the per-thread data, holdrand, the ctype table, the lock table).
#include "crt_common.h"
#include "crt_portable.h"

namespace zax::crt {

// ---- per-thread data, errno, locks --------------------------------------------------------------------------

// _initptd 0x5d4f15
void __cdecl initptd(ThreadData *p)
{
    *(DWORD *)((BYTE *)p + 0x50) = glob_addr(INITIAL_LOCALE_INFO);
    p->holdrand = 1;
}

void __cdecl amsg_exit(int code);                 // crt_exit.cpp

// _getptd 0x5d4f28: the thread's block from TLS, made (calloc) on the first use; GetLastError survives
ThreadData *getptd()
{
    const DWORD saved = api<DWORD(WINAPI *)()>(iat::GetLastError)();
    const DWORD index = *glob<DWORD *>(TLS_INDEX);
    auto *p = (ThreadData *)api<void *(WINAPI *)(DWORD)>(iat::TlsGetValue)(index);
    if (!p) {
        p = (ThreadData *)exe_calloc(1, sizeof(ThreadData));
        if (p && api<BOOL(WINAPI *)(DWORD, void *)>(iat::TlsSetValue)(index, p)) {
            initptd(p);
            p->tid = api<DWORD(WINAPI *)()>(iat::GetCurrentThreadId)();
            p->thandle = 0xffffffff;
        } else
            amsg_exit(0x10);   // _RT_THREAD
    }
    api<void(WINAPI *)(DWORD)>(iat::SetLastError)(saved);
    return p;
}
ThreadData *__cdecl getptd_() { return getptd(); }

// _errno 0x5d1014, __doserrno 0x5d101d
int *errno_ptr() { return &getptd()->terrno; }
DWORD *doserrno_ptr() { return &getptd()->tdoserrno; }
int *__cdecl errno_() { return errno_ptr(); }
DWORD *__cdecl doserrno_() { return doserrno_ptr(); }

// _dosmaperr 0x5d0fa1: _doserrno = the Windows error, errno from the exe's table; unknown codes:
// 19-36 EACCES, 188-202 ENOEXEC, others EINVAL
void dosmaperr(DWORD oserror)
{
    *doserrno_ptr() = oserror;
    for (DWORD e = glob_addr(ERRNO_TABLE); e < glob_addr(ERRNO_TABLE_END); e += 8)
        if (*at<DWORD *>(e) == oserror) {
            *errno_ptr() = *at<int *>(e + 4);
            return;
        }
    *errno_ptr() = oserror >= 0x13 && oserror <= 0x24 ? 13 : oserror >= 0xbc && oserror <= 0xca ? 8 : 22;
}
void __cdecl dosmaperr_(DWORD oserror) { dosmaperr(oserror); }


// _lock 0x5d4305: the critical section of lock n, made on first use under lock 0x11 (_LOCKTAB_LOCK)
void lock(int n)
{
    void **slot = at<void **>(glob_addr(LOCK_TABLE) + 4 * (DWORD)n);
    if (!*slot) {
        void *cs = exe_malloc(0x18);
        if (!cs)
            amsg_exit(0x11);
        lock(LOCKTAB_LOCK);
        if (!*slot) {
            api<void(WINAPI *)(void *)>(iat::InitializeCriticalSection)(cs);
            *slot = cs;
        } else
            exe_free(cs);
        unlock(LOCKTAB_LOCK);
    }
    api<void(WINAPI *)(void *)>(iat::EnterCriticalSection)(*slot);
}
// _unlock 0x5d4366
void unlock(int n) { api<void(WINAPI *)(void *)>(iat::LeaveCriticalSection)(*at<void **>(glob_addr(LOCK_TABLE) + 4 * (DWORD)n)); }
void __cdecl lock_(int n) { lock(n); }
void __cdecl unlock_(int n) { unlock(n); }

// _lock_file 0x5d3b30 / _unlock_file 0x5d3b82: the 20 static streams (_iob 0x6298e0, 0x20 bytes each)
// have the locks 0x1c + index, every other stream its own critical section at +0x20
constexpr DWORD IOB = 0x6298e0, IOB_LAST = 0x629b40;
void __cdecl lock_file(BYTE *stream)
{
    const DWORD a = (DWORD)(uintptr_t)stream;
    if (a >= glob_addr(IOB) && a <= glob_addr(IOB_LAST))
        lock((int)((a - glob_addr(IOB)) >> 5) + 0x1c);
    else
        api<void(WINAPI *)(void *)>(iat::EnterCriticalSection)(stream + 0x20);
}
void __cdecl unlock_file(BYTE *stream)
{
    const DWORD a = (DWORD)(uintptr_t)stream;
    if (a >= glob_addr(IOB) && a <= glob_addr(IOB_LAST))
        unlock((int)((a - glob_addr(IOB)) >> 5) + 0x1c);
    else
        api<void(WINAPI *)(void *)>(iat::LeaveCriticalSection)(stream + 0x20);
}
// _lock_file2 0x5d3b5f / _unlock_file2 0x5d3bb1: by the stream's index in __piob
void __cdecl lock_file2(int index, BYTE *stream)
{
    if (index < 20)
        lock(index + 0x1c);
    else
        api<void(WINAPI *)(void *)>(iat::EnterCriticalSection)(stream + 0x20);
}
void __cdecl unlock_file2(int index, BYTE *stream)
{
    if (index < 20)
        unlock(index + 0x1c);
    else
        api<void(WINAPI *)(void *)>(iat::LeaveCriticalSection)(stream + 0x20);
}

// ---- character classes and case ---------------------------------------------------------------------------

constexpr DWORD ISCTYPE = 0x5d3c5b, TOUPPER = 0x5d0c4e, TOUPPER_LK = 0x5d0cbd, TOLOWER = 0x5d272a, TOLOWER_LK = 0x5d2799,
                STRICMP = 0x5d3640, STRNICMP = 0x5dd9c0;

// _isctype 0x5d3c5b: the table for -1..255; anything else goes through GetStringTypeA (multibyte code
// pages only, never with the exe's constant __mb_cur_max = 1): the original
int isctype(int c, int mask)
{
    if ((unsigned)(c + 1) <= 0x100)
        return (*glob<const WORD **>(PCTYPE))[c] & mask;
#ifdef ZAX_STANDALONE
    // [confirmed: code 0x5d3c79] reachable with a character below -1 (a signed char above 0x7f): the type from
    // __crtGetStringTypeA 0x5db59e (code page 0, locale 0), of two bytes when the high byte is a lead byte (its table
    // entry's 0x8000), else of one; 0 when that fails. The original writes the type into the high word of c's slot.
    const BYTE hi = (BYTE)(c >> 8);
    BYTE buf[4] = {};
    int n = 1;
    if ((*glob<const BYTE **>(PCTYPE))[hi * 2 + 1] & 0x80)
        buf[0] = hi, buf[1] = (BYTE)c, n = 2;
    else
        buf[0] = (BYTE)c;
    WORD type = (WORD)((DWORD)c >> 16);
    if (!fn<int(__cdecl *)(DWORD, const char *, int, WORD *, int, int, int)>(0x5db59e)(1, reinterpret_cast<const char *>(buf), n, &type, 0,
                                                                                        0, 1))
        return 0;
    return type & mask;
#else
    return original<int(__cdecl *)(int, int)>(fn_exe(ISCTYPE))(c, mask);
#endif
}
int __cdecl isctype_(int c, int mask) { return isctype(c, mask); }

// isupper 0x5d0f29, islower 0x5d0f51, isspace 0x5d0f79: the table bit (1, 2, 8)
int __cdecl isupper_(int c) { return ctype(c, UPPER); }
int __cdecl islower_(int c) { return ctype(c, LOWER); }
int __cdecl isspace_(int c) { return ctype(c, SPACE); }

bool c_locale() { return *glob<DWORD *>(LC_HANDLE_CTYPE) == 0; }

// toupper 0x5d0c4e, _toupper_lk 0x5d0cbd, tolower 0x5d272a, _tolower_lk 0x5d2799: ASCII in the C locale
int __cdecl toupper_(int c) { return c_locale() ? (c >= 'a' && c <= 'z' ? c - 0x20 : c) : original_or_dead<int(__cdecl *)(int)>(fn_exe(TOUPPER))(c); }
int __cdecl toupper_lk(int c) { return c_locale() ? (c >= 'a' && c <= 'z' ? c - 0x20 : c) : original_or_dead<int(__cdecl *)(int)>(fn_exe(TOUPPER_LK))(c); }
int __cdecl tolower_(int c) { return c_locale() ? (c >= 'A' && c <= 'Z' ? c + 0x20 : c) : original_or_dead<int(__cdecl *)(int)>(fn_exe(TOLOWER))(c); }
int __cdecl tolower_lk(int c) { return c_locale() ? (c >= 'A' && c <= 'Z' ? c + 0x20 : c) : original_or_dead<int(__cdecl *)(int)>(fn_exe(TOLOWER_LK))(c); }

// _stricmp 0x5d3640, _strnicmp 0x5dd9c0
int __cdecl stricmp_(const char *a, const char *b)
{
    return c_locale() ? stricmp(a, b) : original_or_dead<int(__cdecl *)(const char *, const char *)>(fn_exe(STRICMP))(a, b);
}
int __cdecl strnicmp_(const char *a, const char *b, size_t n)
{
    return c_locale() ? strnicmp(a, b, n) : original_or_dead<int(__cdecl *)(const char *, const char *, size_t)>(fn_exe(STRNICMP))(a, b, n);
}

// ---- memory -----------------------------------------------------------------------------------------------

// memmove 0x5d04b0 and memcpy 0x5d8230 (the same code in VC6: memcpy handles overlap like memmove).
// The return value is the destination.
void *__cdecl memmove_(void *dst, const void *src, size_t n)
{
    BYTE *d = (BYTE *)dst;
    const BYTE *s = (const BYTE *)src;
    if (d <= s || d >= s + n) {
        size_t words = n >> 2, rest = n & 3;
        __asm__ volatile("rep movsl\n\tmovl %3, %%ecx\n\trep movsb"
                         : "+D"(d), "+S"(s), "+c"(words)
                         : "r"(rest)
                         : "memory");
    } else {
        d += n - 1;
        s += n - 1;
        __asm__ volatile("std\n\trep movsb\n\tcld" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
    }
    return dst;
}

// memset 0x5d8e60
void *__cdecl memset_(void *dst, int c, size_t n)
{
    BYTE *d = (BYTE *)dst;
    const DWORD v = (BYTE)c * 0x01010101u;
    size_t words = n >> 2, rest = n & 3;
    __asm__ volatile("rep stosl\n\tmovl %2, %%ecx\n\trep stosb" : "+D"(d), "+c"(words) : "r"(rest), "a"(v) : "memory");
    return dst;
}

// memchr 0x5e9730
void *__cdecl memchr_(const void *p, int c, size_t n)
{
    for (const BYTE *b = (const BYTE *)p; n; n--, b++)
        if (*b == (BYTE)c)
            return (void *)b;
    return nullptr;
}

// ---- strings ----------------------------------------------------------------------------------------------

size_t __cdecl strlen_(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

size_t __cdecl wcslen_(const WORD *s)
{
    const WORD *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

char *__cdecl strcpy_(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++))
        ;
    return r;
}

char *__cdecl strcat_(char *d, const char *s)
{
    strcpy_(d + strlen_(d), s);
    return d;
}

// strncpy 0x5d0b50: at most n bytes, the rest of n filled with zeros
char *__cdecl strncpy_(char *d, const char *s, size_t n)
{
    char *r = d;
    while (n && (*d = *s++))
        d++, n--;
    while (n--)
        *d++ = 0;
    return r;
}

// strcmp (0x5db3e0, named _strcmp): -1, 0, 1 by unsigned bytes
int __cdecl strcmp_(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (*a != *b)
            return (BYTE)*a < (BYTE)*b ? -1 : 1;
        if (!*a)
            return 0;
    }
}

// strncmp 0x5d17d0: -1, 0, 1 by unsigned bytes, up to the end of the first string or n
int __cdecl strncmp_(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b)
            return (BYTE)*a < (BYTE)*b ? -1 : 1;
        if (!*a)
            return 0;
    }
    return 0;
}

// strchr 0x5d08c0 (c == 0 finds the terminator), strrchr 0x5d0d90
char *__cdecl strchr_(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c)
            return (char *)s;
        if (!*s)
            return nullptr;
    }
}
char *__cdecl strrchr_(const char *s, int c)
{
    const char *found = nullptr;
    for (;; s++) {
        if (*s == (char)c)
            found = s;
        if (!*s)
            return (char *)found;
    }
}

// strstr 0x5d1030: an empty needle finds the start
char *__cdecl strstr_(const char *s, const char *needle)
{
    if (!*needle)
        return (char *)s;
    for (; *s; s++) {
        const char *a = s, *b = needle;
        while (*a && *a == *b)
            a++, b++;
        if (!*b)
            return (char *)s;
    }
    return nullptr;
}

// _strdup 0x5d3606
char *__cdecl strdup_(const char *s)
{
    if (!s)
        return nullptr;
    char *d = (char *)exe_malloc(strlen_(s) + 1);
    return d ? strcpy_(d, s) : nullptr;
}

// ---- numbers ----------------------------------------------------------------------------------------------

// atol 0x5d0348, atoi 0x5d03d3 (crt::atol with the exe's ctype table; the C locale's is the portable one)
long __cdecl atol_(const char *s)
{
    const BYTE *p = (const BYTE *)s;
    while (ctype(*p, SPACE))
        p++;
    int c = *p++;
    const int sign = c;
    if (c == '-' || c == '+')
        c = *p++;
    unsigned total = 0;
    while (ctype(c, DIGIT)) {
        total = total * 10 + (unsigned)(c - '0');
        c = *p++;
    }
    return (long)(sign == '-' ? 0u - total : total);
}
int __cdecl atoi_(const char *s) { return (int)atol_(s); }

// strtol 0x5d10b0 / strtoxl 0x5d10c7 (flags 0: signed; the exe calls it with 0 only): crt::strtol, errno
// ERANGE on overflow
long __cdecl strtol_(const char *s, char **end, int base)
{
    if (*glob<int *>(MB_CUR_MAX) > 1)
        return original_or_dead<long(__cdecl *)(const char *, char **, int)>(fn_exe(0x5d10b0))(s, end, base);
    bool range = false;
    const char *e;
    const long v = strtol(s, &e, base, &range);
    if (range)
        *errno_ptr() = 34;
    if (end)
        *end = (char *)e;
    return v;
}

// xtoa 0x5dd95a / _itoa 0x5dd92d: digits 0-9, a-z; a sign only for base 10
void __cdecl xtoa(unsigned v, char *out, unsigned radix, int negative)
{
    char *p = out;
    if (negative) {
        *p++ = '-';
        v = 0u - v;
    }
    char *first = p;
    do {
        const unsigned d = v % radix;
        v /= radix;
        *p++ = (char)(d > 9 ? d + 'a' - 10 : d + '0');
    } while (v);
    *p-- = 0;
    while (first < p) {
        const char t = *p;
        *p-- = *first;
        *first++ = t;
    }
}
char *__cdecl itoa_(int v, char *out, int radix)
{
    xtoa((unsigned)v, out, (unsigned)radix, radix == 10 && v < 0);
    return out;
}

// ---- qsort, rand ------------------------------------------------------------------------------------------

void __cdecl qsort_(void *base, unsigned num, unsigned width, Compare comp) { qsort(base, num, width, comp); }

// srand 0x5d218b / rand 0x5d2198: holdrand in the thread's data
void __cdecl srand_(unsigned seed) { getptd()->holdrand = seed; }
int __cdecl rand_()
{
    ThreadData *p = getptd();
    Rand r;
    r.seed(p->holdrand);
    const int v = r.next();
    p->holdrand = r.holdrand;
    return v;
}

}  // namespace zax::crt

namespace zax {

// `vector constructor iterator' 0x402090 (__stdcall): the constructor (__thiscall) on n elements
void STDCALL vector_ctor_iterator(BYTE *p, unsigned size, int n, void(THISCALL *ctor)(void *))
{
    for (; n > 0; n--, p += size)
        ctor(p);
}

using namespace crt;

ZAX_HOOKS(crt_string)
{
    const Hook list[] = {
        {0x402090, (void *)&vector_ctor_iterator, "vector_ctor_iterator", 5, {0x8b, 0x44, 0x24, 0x0c, 0x48, 0x78, 0x20, 0x53}},
        {0x5d4f15, (void *)&initptd, "_initptd", 11, {0x8b, 0x44, 0x24, 0x04, 0xc7, 0x40, 0x50, 0x00}},
        {0x5d4f28, (void *)&getptd_, "_getptd", 8, {0x56, 0x57, 0xff, 0x15, 0x50, 0xa1, 0x5e, 0x00}},
        {0x5d1014, (void *)&errno_, "_errno", 5, {0xe8, 0x0f, 0x3f, 0x00, 0x00, 0x83, 0xc0, 0x08}, 1},
        {0x5d101d, (void *)&doserrno_, "__doserrno", 5, {0xe8, 0x06, 0x3f, 0x00, 0x00, 0x83, 0xc0, 0x0c}, 1},
        {0x5d0fa1, (void *)&dosmaperr_, "_dosmaperr", 6, {0x56, 0xe8, 0x76, 0x00, 0x00, 0x00, 0x8b, 0x4c}, 2},
        {0x5d4305, (void *)&lock_, "_lock", 6, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x56, 0x83}},
        {0x5d4366, (void *)&unlock_, "_unlock", 6, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0xff, 0x34}},
        {0x5d3b30, (void *)&lock_file, "_lock_file", 9, {0x8b, 0x44, 0x24, 0x04, 0xb9, 0xe0, 0x98, 0x62}},
        {0x5d3b82, (void *)&unlock_file, "_unlock_file", 9, {0x8b, 0x44, 0x24, 0x04, 0xb9, 0xe0, 0x98, 0x62}},
        {0x5d3b5f, (void *)&lock_file2, "_lock_file2", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0x14, 0x7d}},
        {0x5d3bb1, (void *)&unlock_file2, "_unlock_file2", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0x14, 0x7d}},
        {0x5d3c5b, (void *)&isctype_, "_isctype", 7, {0x55, 0x8b, 0xec, 0x51, 0x8b, 0x45, 0x08, 0x8d}},
        {0x5d0f29, (void *)&isupper_, "isupper", 7, {0x83, 0x3d, 0x6c, 0x9d, 0x62, 0x00, 0x01, 0x7e}},
        {0x5d0f51, (void *)&islower_, "islower", 7, {0x83, 0x3d, 0x6c, 0x9d, 0x62, 0x00, 0x01, 0x7e}},
        {0x5d0f79, (void *)&isspace_, "isspace", 7, {0x83, 0x3d, 0x6c, 0x9d, 0x62, 0x00, 0x01, 0x7e}},
        {0x5d0c4e, (void *)&toupper_, "toupper", 9, {0x53, 0x33, 0xdb, 0x39, 0x1d, 0x6c, 0x59, 0x71}},
        {0x5d0cbd, (void *)&toupper_lk, "_toupper_lk", 11, {0x55, 0x8b, 0xec, 0x51, 0x83, 0x3d, 0x6c, 0x59}},
        {0x5d272a, (void *)&tolower_, "tolower", 9, {0x53, 0x33, 0xdb, 0x39, 0x1d, 0x6c, 0x59, 0x71}},
        {0x5d2799, (void *)&tolower_lk, "_tolower_lk", 11, {0x55, 0x8b, 0xec, 0x51, 0x83, 0x3d, 0x6c, 0x59}},
        {0x5d3640, (void *)&stricmp_, "_stricmp", 5, {0x55, 0x8b, 0xec, 0x57, 0x56, 0x53, 0x8b, 0x75}},
        {0x5dd9c0, (void *)&strnicmp_, "_strnicmp", 5, {0x55, 0x8b, 0xec, 0x57, 0x56, 0x53, 0x8b, 0x4d}},
        {0x5d04b0, (void *)&memmove_, "memmove", 5, {0x55, 0x8b, 0xec, 0x57, 0x56, 0x8b, 0x75, 0x0c}},
        {0x5d8230, (void *)&memmove_, "memcpy", 5, {0x55, 0x8b, 0xec, 0x57, 0x56, 0x8b, 0x75, 0x0c}},
        {0x5d8e60, (void *)&memset_, "memset", 8, {0x8b, 0x54, 0x24, 0x0c, 0x8b, 0x4c, 0x24, 0x04}},
        {0x5e9730, (void *)&memchr_, "memchr", 5, {0x8b, 0x44, 0x24, 0x0c, 0x53, 0x85, 0xc0, 0x74}},
        {0x5d3be0, (void *)&strlen_, "strlen", 10, {0x8b, 0x4c, 0x24, 0x04, 0xf7, 0xc1, 0x03, 0x00}},
        {0x5d1789, (void *)&wcslen_, "wcslen", 8, {0x8b, 0x4c, 0x24, 0x04, 0x66, 0x83, 0x39, 0x00}},
        {0x5daae0, (void *)&strcpy_, "strcpy", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0xeb, 0x6a, 0x8d}},
        {0x5daaf0, (void *)&strcat_, "strcat", 5, {0x8b, 0x4c, 0x24, 0x04, 0x57, 0xf7, 0xc1, 0x03}},
        {0x5d0b50, (void *)&strncpy_, "strncpy", 5, {0x8b, 0x4c, 0x24, 0x0c, 0x57, 0x85, 0xc9, 0x74}},
        {0x5db3e0, (void *)&strcmp_, "strcmp", 8, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x4c, 0x24, 0x08}},
        {0x5d17d0, (void *)&strncmp_, "strncmp", 5, {0x55, 0x8b, 0xec, 0x57, 0x56, 0x53, 0x8b, 0x4d}},
        {0x5d08c0, (void *)&strchr_, "strchr", 6, {0x33, 0xc0, 0x8a, 0x44, 0x24, 0x08, 0x53, 0x8b}},
        {0x5d0d90, (void *)&strrchr_, "strrchr", 7, {0x55, 0x8b, 0xec, 0x57, 0x8b, 0x7d, 0x08, 0x33}},
        {0x5d1030, (void *)&strstr_, "strstr", 5, {0x8b, 0x4c, 0x24, 0x08, 0x57, 0x53, 0x56, 0x8a}},
        {0x5d3606, (void *)&strdup_, "_strdup", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x85, 0xf6, 0x74}},
        {0x5d0348, (void *)&atol_, "atol", 8, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14}},
        {0x5d03d3, (void *)&atoi_, "atoi", 9, {0xff, 0x74, 0x24, 0x04, 0xe8, 0x6c, 0xff, 0xff}, 5},
        {0x5d10b0, (void *)&strtol_, "strtol", 6, {0x6a, 0x00, 0xff, 0x74, 0x24, 0x10, 0xff, 0x74}},
        {0x5dd95a, (void *)&xtoa, "xtoa", 7, {0x55, 0x8b, 0xec, 0x83, 0x7d, 0x14, 0x00, 0x8b}},
        {0x5dd92d, (void *)&itoa_, "_itoa", 7, {0x55, 0x8b, 0xec, 0x83, 0x7d, 0x10, 0x0a, 0x75}},
        {0x5d097c, (void *)&qsort_, "qsort", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0xf8, 0x00, 0x00}},
        {0x5d218b, (void *)&srand_, "srand", 5, {0xe8, 0x98, 0x2d, 0x00, 0x00, 0x8b, 0x4c, 0x24}, 1},
        {0x5d2198, (void *)&rand_, "rand", 5, {0xe8, 0x8b, 0x2d, 0x00, 0x00, 0x8b, 0x48, 0x14}, 1},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
