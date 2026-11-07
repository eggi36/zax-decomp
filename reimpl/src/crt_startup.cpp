// The C runtime's start-up and shut-down helpers (VC6 heapinit.c, tidtable.c, mlock.c, crt0dat.c,
// onexit.c, clock.c, _file.c, fflush.c, closeall.c, hooks.c, validate.c, _fptostr.c; docs/DECOMPILATION.md
// 11, "C runtime, phase 5b"): _heap_init with __heap_select, _GetLinkerVersion and __sbh_heap_init,
// _mtinit and _mtinitlocks, _cinit, __onexitinit, __inittime, __initstdio, __endstdio, _flushall,
// _fcloseall, __inittermination, __termination, _ValidateExecute, _positive. The hooks are in before the
// exe's first instruction, so WinMainCRTStartup (crt_sehc.cpp) already runs these. The runtime's data
// stays in the exe; Windows through the exe's import slots; the runtime functions they call through the
// exe's entry points, as the original calls them.
#include "crt_common.h"

namespace zax::crt {

namespace {

namespace iatx {
constexpr DWORD HeapCreate = 0x5ea1a8, HeapDestroy = 0x5ea1a4, GetVersionExA = 0x5ea1a0, GetEnvironmentVariableA = 0x5ea19c,
                GetModuleFileNameA = 0x5ea0e0, TlsAlloc = 0x5ea18c, GetSystemTimeAsFileTime = 0x5ea0b0,
                DeleteCriticalSection = 0x5ea11c, SetUnhandledExceptionFilter = 0x5ea0a0, IsBadCodePtr = 0x5ea038;
}

constexpr DWORD SBH_HEADER_LIST = 0x716518, SBH_HEADER_DEFER = 0x716510, SBH_IND_GROUP_DEFER = 0x716514,
                SBH_HEADER_SCAN = 0x71650c, SBH_SIZE_HEADER_LIST = 0x716504;
constexpr DWORD FPINIT = 0x629624;                         // _FPinit (_fpmath when the float support is linked)
constexpr DWORD XI_A = 0x60a3f0, XI_Z = 0x60a408, XC_A = 0x609000, XC_Z = 0x60a3ec;   // C and C++ initializers
constexpr DWORD ONEXIT_BEGIN = 0x71755c, ONEXIT_END = 0x717558;
constexpr DWORD START_TICK = 0x7158e8;                     // __inittime: the FILETIME at the start (clock)
constexpr DWORD NSTREAM = 0x717540, PIOB = 0x716530, IOB = 0x6298e0, EXITFLAG = 0x7158b4;
constexpr DWORD PREV_FILTER = 0x715c28;                    // the filter before __CxxUnhandledExceptionFilter
constexpr DWORD CXX_FILTER = 0x5dd00b;
constexpr DWORD ZERO_DOUBLE = 0x5ee660;
constexpr DWORD TXT_HEAP_SELECT = 0x6072e0, TXT_GLOBAL_HEAP_SELECTED = 0x6072c8;
// the four critical sections _mtinitlocks sets up (the locks the runtime needs before it can allocate)
constexpr GlobKey PRE_LOCKS[] = {0x629dd4, 0x629dc4, 0x629db4, 0x629d94};

struct OsVersionInfo {
    DWORD size, major, minor, build, platform;
    char csd[128];
};
static_assert(sizeof(OsVersionInfo) == 0x94, "OSVERSIONINFOA");

using Strncmp = int(__cdecl *)(const char *, const char *, size_t);
using Strstr = char *(__cdecl *)(const char *, const char *);
using Strchr = char *(__cdecl *)(const char *, int);
using Strtol = long(__cdecl *)(const char *, char **, int);

// a-z to upper case in place (the original compares signed: bytes from 0x80 stay)
void upper_ascii(char *s)
{
    if (!*s)
        return;
    do {
        if ((signed char)*s >= 'a' && (signed char)*s <= 'z')
            *s -= 0x20;
        s++;
    } while (*s);
}

}  // namespace

// _GetLinkerVersion 0x5d67c1: the linker version bytes of the exe's PE header (the DWORD cleared first)
void __cdecl get_linker_version(BYTE *version)
{
    *reinterpret_cast<DWORD *>(version) = 0;
    const BYTE *base = api<const BYTE *(WINAPI *)(const char *)>(iat::GetModuleHandleA)(nullptr);
    if (*reinterpret_cast<const WORD *>(base) != 0x5a4d)
        return;
    const DWORD lfanew = *reinterpret_cast<const DWORD *>(base + 0x3c);
    if (!lfanew)
        return;
    version[0] = base[lfanew + 0x1a];
    version[1] = base[lfanew + 0x1b];
}

// __heap_select 0x5d67ee: 1 (the system heap) on Windows NT 5 and later; otherwise what
// __MSVCRT_HEAP_SELECT says ("__GLOBAL_HEAP_SELECTED,n" or "<exe path>,n;..."), else 3 for a linker from
// version 6 on, 2 before
int __cdecl heap_select()
{
    OsVersionInfo info;
    char path[0x104];
    char env[0x1090];
    // the original's frame of 0x122c bytes goes through _chkstk; here the pages are touched top down
    for (int i = (int)sizeof env - 1; i >= 0; i -= 0x800)
        reinterpret_cast<volatile char *>(env)[i] = 0;
    info.size = sizeof info;
    if (api<BOOL(WINAPI *)(OsVersionInfo *)>(iatx::GetVersionExA)(&info) && info.platform == 2 && info.major >= 5)
        return 1;
    if (api<DWORD(WINAPI *)(const char *, char *, DWORD)>(iatx::GetEnvironmentVariableA)(glob<const char *>(TXT_HEAP_SELECT), env,
                                                                                          sizeof env)) {
        upper_ascii(env);
        char *p;
        if (!fn<Strncmp>(0x5d17d0)(glob<const char *>(TXT_GLOBAL_HEAP_SELECTED), env, 0x16))
            p = env;
        else {
            api<DWORD(WINAPI *)(void *, char *, DWORD)>(iatx::GetModuleFileNameA)(nullptr, path, sizeof path);
            upper_ascii(path);
            p = fn<Strstr>(0x5d1030)(env, path);
        }
        if (p && (p = fn<Strchr>(0x5d08c0)(p, ','))) {
            p++;
            for (char *q = p; *q;) {
                if (*q == ';')
                    *q = 0;
                else
                    q++;
            }
            const long n = fn<Strtol>(0x5d10b0)(p, nullptr, 10);
            if (n == 2 || n == 3 || n == 1)
                return (int)n;
        }
    }
    DWORD version;
    fn<void(__cdecl *)(BYTE *)>(0x5d67c1)(reinterpret_cast<BYTE *>(&version));
    return (BYTE)version < 6 ? 2 : 3;
}

// __sbh_heap_init 0x5d6993: the V6 small-block heap's first header list (16 entries); 0 when it fails
int __cdecl sbh_heap_init(int threshold)
{
    void *list = api<void *(WINAPI *)(HANDLE, DWORD, size_t)>(iat::HeapAlloc)(*glob<HANDLE *>(CRT_HEAP), 0, 0x140);
    *glob<void **>(SBH_HEADER_LIST) = list;
    if (!list)
        return 0;
    *glob<DWORD *>(SBH_HEADER_DEFER) = 0;
    *glob<DWORD *>(SBH_IND_GROUP_DEFER) = 0;
    *glob<void **>(SBH_HEADER_SCAN) = list;
    *glob<int *>(SBH_THRESHOLD) = threshold;
    *glob<DWORD *>(SBH_SIZE_HEADER_LIST) = 0x10;
    return 1;
}

// _heap_init 0x5d6936: the runtime's heap (not serialized for a single-threaded start) and the kind of
// heap __heap_select chooses; 0 when it fails
int __cdecl heap_init(int mtflag)
{
    const HANDLE heap = api<HANDLE(WINAPI *)(DWORD, size_t, size_t)>(iatx::HeapCreate)(mtflag == 0, 0x1000, 0);
    *glob<HANDLE *>(CRT_HEAP) = heap;
    if (!heap)
        return 0;
    const int kind = fn<int(__cdecl *)()>(0x5d67ee)();
    *glob<int *>(ACTIVE_HEAP) = kind;
    int ok;
    if (kind == 3)
        ok = fn<int(__cdecl *)(int)>(0x5d6993)(0x3f8);
    else if (kind == 2)
        ok = exe_or_dead<int(__cdecl *)()>(0x5d74da)();   // __old_sbh_new_region
    else
        return 1;
    if (ok)
        return 1;
    api<BOOL(WINAPI *)(HANDLE)>(iatx::HeapDestroy)(*glob<HANDLE *>(CRT_HEAP));
    return 0;
}

// _mtinitlocks 0x5d42dc: the four critical sections of the locks the runtime needs before it can allocate
void __cdecl mtinitlocks()
{
    const auto init = api<void(WINAPI *)(void *)>(iat::InitializeCriticalSection);
    for (GlobKey slot : PRE_LOCKS)
        init(*glob<void **>(slot));
}

// _mtinit 0x5d4ec1: the locks, the TLS slot and the main thread's per-thread data; 0 when it fails
int __cdecl mtinit()
{
    fn<void(__cdecl *)()>(0x5d42dc)();
    const DWORD index = api<DWORD(WINAPI *)()>(iatx::TlsAlloc)();
    *glob<DWORD *>(TLS_INDEX) = index;
    if (index == 0xffffffff)
        return 0;
    ThreadData *ptd = static_cast<ThreadData *>(exe_calloc(1, sizeof(ThreadData)));
    if (!ptd || !api<BOOL(WINAPI *)(DWORD, void *)>(iat::TlsSetValue)(*glob<DWORD *>(TLS_INDEX), ptd))
        return 0;
    fn<void(__cdecl *)(ThreadData *)>(0x5d4f15)(ptd);   // _initptd
    ptd->tid = api<DWORD(WINAPI *)()>(iat::GetCurrentThreadId)();
    ptd->thandle = 0xffffffff;
    return 1;
}

// _cinit 0x5d0e09: the floating point set-up, then the C and the C++ initializers
void __cdecl cinit()
{
    if (void (*fpinit)() = *glob<void (**)()>(FPINIT))
        fpinit();
    using Initterm = void(__cdecl *)(DWORD, DWORD);
    fn<Initterm>(0x5d0f0f)(glob_addr(XI_A), glob_addr(XI_Z));
    fn<Initterm>(0x5d0f0f)(glob_addr(XC_A), glob_addr(XC_Z));
}

// __onexitinit 0x5d0875: the exit table (32 entries, empty)
void __cdecl onexitinit()
{
    DWORD *table = static_cast<DWORD *>(exe_malloc(0x80));
    *glob<DWORD **>(ONEXIT_BEGIN) = table;
    if (!table) {
        fn<void(__cdecl *)(int)>(0x5d3397)(0x18);   // _amsg_exit(_RT_ONEXIT)
        table = *glob<DWORD **>(ONEXIT_BEGIN);
    }
    *table = 0;
    *glob<DWORD **>(ONEXIT_END) = *glob<DWORD **>(ONEXIT_BEGIN);
}

// __inittime 0x5d3710: the start time for clock(), the FILETIME as 64 bits (the high part through __allmul)
void __cdecl inittime()
{
    DWORD ft[2];
    api<void(WINAPI *)(DWORD *)>(iatx::GetSystemTimeAsFileTime)(ft);
    using Allmul = unsigned long long(__stdcall *)(unsigned long long, unsigned long long);
    const unsigned long long high = fn<Allmul>(0x5d2640)(ft[1], 1ull << 32);
    *glob<unsigned long long *>(START_TICK) = high + ft[0];
}

// __initstdio 0x5d3a74: __piob for _nstream streams (512 by default, at least 20) with _iob in its first
// 20 entries; stdin/out/err without an OS handle get the file number -1
void __cdecl initstdio()
{
    int &nstream = *glob<int *>(NSTREAM);
    if (!nstream)
        nstream = 0x200;
    else if (nstream < 0x14)
        nstream = 0x14;
    void **&piob = *glob<void ***>(PIOB);
    piob = static_cast<void **>(exe_calloc((size_t)nstream, 4));
    if (!piob) {
        nstream = 0x14;
        piob = static_cast<void **>(exe_calloc(0x14, 4));
        if (!piob)
            fn<void(__cdecl *)(int)>(0x5d3397)(0x1a);   // _amsg_exit(_RT_STDIOINIT)
    }
    for (int i = 0; i < 0x14; i++)
        (*glob<void ***>(PIOB))[i] = glob<File *>(IOB) + i;
    for (int i = 0; i < 3; i++) {
        const HANDLE h = ioinfo(i)->osfhnd;
        if (h == (HANDLE)-1 || !h)
            glob<File *>(IOB)[i].file = -1;
    }
}

// _flushall 0x5d190f: flsall(FLUSHALL)
int __cdecl flushall()
{
    return fn<int(__cdecl *)(int)>(0x5d1918)(1);
}

// _fcloseall 0x5db51d: every open stream from the fourth on closed (the count of those closed), the ones
// beyond _iob freed
int __cdecl fcloseall()
{
    int count = 0;
    lock(2);
    for (int i = 3; i < *glob<int *>(NSTREAM); i++) {
        File *f = static_cast<File *>((*glob<void ***>(PIOB))[i]);
        if (!f)
            continue;
        if ((f->flag & 0x83) && fn<int(__cdecl *)(File *)>(0x5d1808)(f) != -1)
            count++;
        if (i >= 0x14) {
            api<void(WINAPI *)(void *)>(iatx::DeleteCriticalSection)(static_cast<BYTE *>((*glob<void ***>(PIOB))[i]) + 0x20);
            exe_free((*glob<void ***>(PIOB))[i]);
            (*glob<void ***>(PIOB))[i] = nullptr;
        }
    }
    unlock(2);
    return count;
}

// __endstdio 0x5d3b1c: the streams flushed, and closed when the process exits
void __cdecl endstdio()
{
    fn<int(__cdecl *)()>(0x5d190f)();
    if (*glob<BYTE *>(EXITFLAG))
        fn<int(__cdecl *)()>(0x5db51d)();
}

// __inittermination 0x5dd051 / __termination 0x5dd062: __CxxUnhandledExceptionFilter in, the previous one back
void __cdecl inittermination()
{
    *glob<DWORD *>(PREV_FILTER) = api<DWORD(WINAPI *)(DWORD)>(iatx::SetUnhandledExceptionFilter)(fn_addr(CXX_FILTER));
}
void __cdecl termination()
{
    api<DWORD(WINAPI *)(DWORD)>(iatx::SetUnhandledExceptionFilter)(*glob<DWORD *>(PREV_FILTER));
}

// _ValidateExecute 0x5dd06f: 1 when the address can be executed
int __cdecl validate_execute(void *p)
{
    return api<BOOL(WINAPI *)(void *)>(iatx::IsBadCodePtr)(p) ? 0 : 1;
}

// _positive 0x5d3e70: 1 when *d >= 0 (fcomp: 0 for a NaN, with invalid raised for any NaN)
int __cdecl positive(const double *d)
{
    WORD sw;
    __asm__ volatile("fldl (%1)\n\tfcompl (%2)\n\tfnstsw %%ax" : "=a"(sw) : "r"(d), "r"(glob<const double *>(ZERO_DOUBLE)));
    return (sw & 0x100) ? 0 : 1;
}

// ---- low-level files, command line and environment at the start ------------------------------------------

namespace {

constexpr DWORD ACMDLN = 0x717544, ASPENVP = 0x7158d8, ENVIRON = 0x71589c, ENV_INITIALIZED = 0x717550;
constexpr DWORD MBCTYPE_INITIALIZED = 0x717554, MBCTYPE = 0x7162e0;
constexpr DWORD PGMNAME = 0x715aa0, PGMPTR = 0x7158ac, ARGV = 0x715894, ARGC = 0x715890;
constexpr DWORD ENV_STRINGS_KIND = 0x715ba4;   // __crtGetEnvironmentStringsA: 1 the wide block, 2 the ANSI one

namespace iaty {
constexpr DWORD GetStartupInfoA = 0x5ea118, GetStdHandle = 0x5ea04c, SetHandleCount = 0x5ea050,
                GetEnvironmentStringsW = 0x5ea10c, GetEnvironmentStrings = 0x5ea100,
                FreeEnvironmentStringsW = 0x5ea104, FreeEnvironmentStringsA = 0x5ea03c;
}

struct StartupInfo {
    DWORD cb;
    char *reserved, *desktop, *title;
    DWORD x, y, xsize, ysize, xcount, ycount, fill, flags;
    WORD show_window, cb_reserved2;
    BYTE *lp_reserved2;
    HANDLE std_input, std_output, std_error;
};
static_assert(sizeof(StartupInfo) == 0x44, "STARTUPINFOA");

inline size_t exe_strlen(const char *s) { return fn<size_t(__cdecl *)(const char *)>(0x5d3be0)(s); }
inline bool mb_lead(char c) { return glob<const BYTE *>(MBCTYPE)[(BYTE)c + 1] & 4; }   // _mbctype[c + 1] & _M1
inline void initmbctable_once()
{
    if (!*glob<int *>(MBCTYPE_INITIALIZED))
        fn<void(__cdecl *)()>(0x5dcf66)();
}

// 32 ioinfo entries closed (no handle, text pipe character LF)
void init_ioinfo_block(IoInfo *block)
{
    for (IoInfo *e = block; e < block + 0x20; e++) {
        e->osfile = 0;
        e->osfhnd = (HANDLE)-1;
        e->lockinitflag = 0;
        e->pipech = 10;
    }
}

}  // namespace

// _ioinit 0x5d8957: __pioinfo's first block, the handles a parent passed in STARTUPINFO.lpReserved2 (up to
// 2048), stdin/out/err from GetStdHandle where none was passed; SetHandleCount
void __cdecl ioinit()
{
    IoInfo **pioinfo = glob<IoInfo **>(PIOINFO);
    int &nhandle = *glob<int *>(NHANDLE);
    IoInfo *first = static_cast<IoInfo *>(exe_malloc(0x480));
    if (!first)
        fn<void(__cdecl *)(int)>(0x5d3397)(0x1b);   // _amsg_exit(_RT_LOWIOINIT)
    pioinfo[0] = first;
    nhandle = 0x20;
    init_ioinfo_block(first);
    StartupInfo si;
    api<void(WINAPI *)(StartupInfo *)>(iaty::GetStartupInfoA)(&si);
    if (si.cb_reserved2 && si.lp_reserved2) {
        int count = *reinterpret_cast<int *>(si.lp_reserved2);
        const BYTE *flags = si.lp_reserved2 + 4;
        const HANDLE *handles = reinterpret_cast<const HANDLE *>(flags + count);
        if (count >= 0x800)
            count = 0x800;
        for (IoInfo **slot = pioinfo + 1; nhandle < count; slot++) {
            IoInfo *block = static_cast<IoInfo *>(exe_malloc(0x480));
            if (!block) {
                count = nhandle;
                break;
            }
            nhandle += 0x20;
            *slot = block;
            init_ioinfo_block(block);
        }
        for (int i = 0; i < count; i++, flags++, handles++)
            if (*handles != (HANDLE)-1 && (*flags & FOPEN) &&
                ((*flags & FPIPE) || api<DWORD(WINAPI *)(HANDLE)>(iat::GetFileType)(*handles))) {
                IoInfo *e = ioinfo(i);
                e->osfhnd = *handles;
                e->osfile = *flags;
            }
    }
    for (int i = 0; i < 3; i++) {
        IoInfo *e = pioinfo[0] + i;
        if (e->osfhnd != (HANDLE)-1) {
            e->osfile |= FTEXT;
            continue;
        }
        e->osfile = FOPEN | FTEXT;
        const DWORD std = i == 0 ? (DWORD)-10 : i == 1 ? (DWORD)-11 : (DWORD)-12;
        const HANDLE h = api<HANDLE(WINAPI *)(DWORD)>(iaty::GetStdHandle)(std);
        DWORD type;
        if (h != (HANDLE)-1 && (type = api<DWORD(WINAPI *)(HANDLE)>(iat::GetFileType)(h)) != 0) {
            e->osfhnd = h;
            type &= 0xff;
            if (type == 2)
                e->osfile |= FDEV;
            else if (type == 3)
                e->osfile |= FPIPE;
        } else
            e->osfile |= FDEV;
    }
    api<DWORD(WINAPI *)(DWORD)>(iaty::SetHandleCount)((DWORD)nhandle);
}

// parse_cmdline 0x5daef2: the command line split into arguments (quotes, backslashes before quotes, "" in
// quotes), counting first (argv and args null) and filling then. As the original, a lead byte in the
// program name is skipped over only while copying.
void __cdecl parse_cmdline(char *cmdstart, char **argv, char *args, int *numargs, int *numchars)
{
    char *p = cmdstart;
    *numchars = 0;
    *numargs = 1;
    if (argv)
        *argv++ = args;
    if (*p == '"') {
        for (;;) {
            const char c = *++p;
            if (c == '"' || !c)
                break;
            if (mb_lead(c)) {
                ++*numchars;
                if (args) {
                    *args++ = *p;
                    p++;
                }
            }
            ++*numchars;
            if (args)
                *args++ = *p;
        }
        ++*numchars;
        if (args)
            *args++ = 0;
        if (*p == '"')
            p++;
    } else {
        char c;
        do {
            ++*numchars;
            if (args)
                *args++ = *p;
            c = *p++;
            if (mb_lead(c)) {
                ++*numchars;
                if (args)
                    *args++ = *p;
                p++;
            }
        } while (c != ' ' && c && c != '\t');
        if (!c)
            p--;
        else if (args)
            args[-1] = 0;
    }
    int inquote = 0;
    for (;;) {
        if (!*p)
            break;
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (argv)
            *argv++ = args;
        ++*numargs;
        for (;;) {
            int copychar = 1;
            unsigned numslash = 0;
            while (*p == '\\') {
                p++;
                numslash++;
            }
            if (*p == '"') {
                if (!(numslash & 1)) {
                    if (inquote && p[1] == '"')
                        p++;   // "" inside quotes: one quote
                    else
                        copychar = 0;
                    inquote = !inquote;
                }
                numslash >>= 1;
            }
            while (numslash--) {
                if (args)
                    *args++ = '\\';
                ++*numchars;
            }
            const char c = *p;
            if (!c || (!inquote && (c == ' ' || c == '\t')))
                break;
            if (copychar) {
                if (args) {
                    if (mb_lead(c)) {
                        *args++ = c;
                        p++;
                        ++*numchars;
                    }
                    *args++ = *p;
                } else if (mb_lead(c)) {
                    p++;
                    ++*numchars;
                }
                ++*numchars;
            }
            p++;
        }
        if (args)
            *args++ = 0;
        ++*numchars;
    }
    if (argv)
        *argv = nullptr;
    ++*numargs;
}

// _setargv 0x5dae59: _pgmptr (the exe's path), __argv/__argc from the command line (or the program name
// when the command line is empty)
int __cdecl setargv()
{
    using Parse = void(__cdecl *)(char *, char **, char *, int *, int *);
    initmbctable_once();
    char *pgm = glob<char *>(PGMNAME);
    api<DWORD(WINAPI *)(void *, char *, DWORD)>(iatx::GetModuleFileNameA)(nullptr, pgm, 0x104);
    *glob<char **>(PGMPTR) = pgm;
    char *cmdstart = **glob<char **>(ACMDLN) ? *glob<char **>(ACMDLN) : pgm;
    int numargs, numchars;
    fn<Parse>(0x5daef2)(cmdstart, nullptr, nullptr, &numargs, &numchars);
    char **p = static_cast<char **>(exe_malloc((size_t)(numchars + numargs * 4)));
    if (!p)
        fn<void(__cdecl *)(int)>(0x5d3397)(8);   // _amsg_exit(_RT_SPACEARG)
    fn<Parse>(0x5daef2)(cmdstart, p, reinterpret_cast<char *>(p + numargs), &numargs, &numchars);
    *glob<char ***>(ARGV) = p;
    *glob<int *>(ARGC) = numargs - 1;
    return numargs - 1;
}

// _setenvp 0x5dada0: _environ from the environment block (entries starting with '=' left out), the block freed
void __cdecl setenvp()
{
    initmbctable_once();
    int count = 0;
    for (const char *s = *glob<char **>(ASPENVP); *s; s += exe_strlen(s) + 1)
        if (*s != '=')
            count++;
    char **env = static_cast<char **>(exe_malloc((size_t)count * 4 + 4));
    *glob<char ***>(ENVIRON) = env;
    if (!env)
        fn<void(__cdecl *)(int)>(0x5d3397)(9);   // _amsg_exit(_RT_SPACEENV)
    for (char *s = *glob<char **>(ASPENVP); *s;) {
        const size_t len = exe_strlen(s) + 1;
        if (*s != '=') {
            *env = static_cast<char *>(exe_malloc(len));
            if (!*env)
                fn<void(__cdecl *)(int)>(0x5d3397)(9);
            fn<char *(__cdecl *)(char *, const char *)>(0x5daae0)(*env, s);
            env++;
        }
        s += len;
    }
    exe_free(*glob<char **>(ASPENVP));
    *glob<char **>(ASPENVP) = nullptr;
    *env = nullptr;
    *glob<int *>(ENV_INITIALIZED) = 1;
}

// __crtGetEnvironmentStringsA 0x5db0a6: a malloc'd copy of the environment block in ANSI: from the wide
// block when the system has one (converted with the ANSI code page), else the ANSI block; null on failure
char *__cdecl crt_get_environment_strings()
{
    using GetW = WORD *(WINAPI *)();
    using Wctmb = int(WINAPI *)(unsigned, DWORD, const WORD *, int, char *, int, const char *, int *);
    int &kind = *glob<int *>(ENV_STRINGS_KIND);
    WORD *wide = nullptr;
    char *ansi = nullptr;
    if (!kind) {
        if ((wide = api<GetW>(iaty::GetEnvironmentStringsW)()) != nullptr)
            kind = 1;
        else {
            if (!(ansi = api<char *(WINAPI *)()>(iaty::GetEnvironmentStrings)()))
                return nullptr;
            kind = 2;
        }
    }
    if (kind == 1) {
        if (!wide && !(wide = api<GetW>(iaty::GetEnvironmentStringsW)()))
            return nullptr;
        const WORD *w = wide;
        if (*w)
            do {
                do
                    w++;
                while (*w);
                w++;
            } while (*w);
        const int wlen = (int)(w - wide) + 1;
        const Wctmb wctmb = api<Wctmb>(iat::WideCharToMultiByte);
        char *result = nullptr;
        const int size = wctmb(0, 0, wide, wlen, nullptr, 0, nullptr, nullptr);
        if (size) {
            char *block = static_cast<char *>(exe_malloc((size_t)size));
            if (block) {
                if (!wctmb(0, 0, wide, wlen, block, size, nullptr, nullptr)) {
                    exe_free(block);
                    block = nullptr;
                }
                result = block;
            }
        }
        api<BOOL(WINAPI *)(WORD *)>(iaty::FreeEnvironmentStringsW)(wide);
        return result;
    }
    if (kind != 2)
        return nullptr;
    if (!ansi && !(ansi = api<char *(WINAPI *)()>(iaty::GetEnvironmentStrings)()))
        return nullptr;
    const char *s = ansi;
    if (*s)
        do {
            do
                s++;
            while (*s);
            s++;
        } while (*s);
    const size_t len = (size_t)(s - ansi) + 1;
    char *copy = static_cast<char *>(exe_malloc(len));
    if (copy)
        fn<void *(__cdecl *)(void *, const void *, size_t)>(0x5d8230)(copy, ansi, len);
    else
        copy = nullptr;
    api<BOOL(WINAPI *)(char *)>(iaty::FreeEnvironmentStringsA)(ansi);
    return copy;
}

// _wincmdln 0x5dad48: the command line after the program name (quoted or up to a blank) and the blanks
char *__cdecl wincmdln()
{
    initmbctable_once();
    char *p = *glob<char **>(ACMDLN);
    if (*p == '"') {
        for (;;) {
            const char c = *++p;
            if (c == '"' || !c)
                break;
            if (fn<int(__cdecl *)(int)>(0x5dcb4c)((BYTE)c))   // _ismbblead
                p++;
        }
        if (*p == '"')
            p++;
    } else if ((BYTE)*p > ' ') {
        do
            p++;
        while ((BYTE)*p > ' ');
    }
    while (*p && (BYTE)*p <= ' ')
        p++;
    return p;
}

// ---- the multibyte code page (mbctype.c) -------------------------------------------------------------------

namespace {

constexpr DWORD MBCODEPAGE = 0x7161b0, ISMBCODEPAGE = 0x7161cc, MBLCID = 0x7163e4, MBULINFO = 0x7161c0;
constexpr DWORD MBCASEMAP = 0x7161e0;               // _mbcasemap[256]
constexpr DWORD SYSTEM_SET = 0x715c18;              // fSystemSet: the code page was asked from the system
constexpr DWORD RGCPINFO = 0x62c588, RGCPINFO_END = 0x62c678;   // __rgcpinfo[5]: the known multibyte code pages
constexpr DWORD RGCTYPEFLAG = 0x62c580;             // __rgctypeflag[4]
constexpr DWORD CTYPE_WORDS = 0x629b6a;             // _ctype + 1 (the entry of character 0)
constexpr int MB_CP_LOCK = 0x19;

namespace iatz {
constexpr DWORD GetCPInfo = 0x5ea0a4, GetOEMCP = 0x5ea08c, GetACP = 0x5ea09c;
}

struct CpInfo {
    DWORD max_char_size;
    BYTE default_char[2];
    BYTE lead_byte[12];
};
static_assert(sizeof(CpInfo) == 0x14, "CPINFO");

inline BYTE *mbctype() { return glob<BYTE *>(MBCTYPE); }
inline void clear_mbctype() { memset_(mbctype(), 0, 0x101); }
inline int cp_to_lcid(int cp) { return fn<int(__cdecl *)(int)>(0x5dcd85)(cp); }
inline void set_sbcs() { fn<void(__cdecl *)()>(0x5dcdb8)(); }
inline void set_sb_up_low() { fn<void(__cdecl *)()>(0x5dcde1)(); }

}  // namespace

// x_ismbbtype 0x5dcb5d: 1 when the byte has one of the _mbctype bits of mask or one of the ctype bits of cmask
int __cdecl x_ismbbtype(unsigned c, int cmask, int mask)
{
    c &= 0xff;
    if (mbctype()[c + 1] & (BYTE)mask)
        return 1;
    const int v = cmask ? glob<const WORD *>(CTYPE_WORDS)[c] & cmask : 0;
    return v ? 1 : 0;
}

// _ismbblead 0x5dcb4c
int __cdecl ismbblead(unsigned c)
{
    return fn<int(__cdecl *)(unsigned, int, int)>(0x5dcb5d)(c, 0, 4);
}

// getSystemCP 0x5dcd3b: -2 the OEM, -3 the ANSI code page, -4 the locale's (fSystemSet then); others stay
int __cdecl get_system_cp(int cp)
{
    *glob<int *>(SYSTEM_SET) = 0;
    if (cp == -2) {
        *glob<int *>(SYSTEM_SET) = 1;
        return (int)api<DWORD(WINAPI *)()>(iatz::GetOEMCP)();
    }
    if (cp == -3) {
        *glob<int *>(SYSTEM_SET) = 1;
        return (int)api<DWORD(WINAPI *)()>(iatz::GetACP)();
    }
    if (cp == -4) {
        cp = *glob<int *>(0x71597c);   // __lc_codepage
        *glob<int *>(SYSTEM_SET) = 1;
    }
    return cp;
}

// CPtoLCID 0x5dcd85: the locale of the four East Asian code pages, 0 for others
int __cdecl cptolcid(int cp)
{
    switch (cp) {
    case 932: return 0x411;
    case 936: return 0x804;
    case 949: return 0x412;
    case 950: return 0x404;
    }
    return 0;
}

// setSBCS 0x5dcdb8: a single-byte code page: no lead bytes, code page and locale 0
void __cdecl setsbcs()
{
    clear_mbctype();
    *glob<int *>(MBCODEPAGE) = 0;
    *glob<int *>(ISMBCODEPAGE) = 0;
    *glob<int *>(MBLCID) = 0;
    memset_(glob<void *>(MBULINFO), 0, 12);
}

// setSBUpLow 0x5dcde1: the upper/lower case bits of _mbctype and _mbcasemap from the code page (the lead
// bytes as blanks), or A-Z/a-z when GetCPInfo fails
void __cdecl setsbuplow()
{
    CpInfo info;
    BYTE *type = mbctype();
    BYTE *casemap = glob<BYTE *>(MBCASEMAP);
    if (api<BOOL(WINAPI *)(DWORD, CpInfo *)>(iatz::GetCPInfo)((DWORD)*glob<int *>(MBCODEPAGE), &info) == 1) {
        BYTE sb[0x100], lower[0x100], upper[0x100];
        WORD ctype1[0x100];
        for (unsigned i = 0; i < 0x100; i++)
            sb[i] = (BYTE)i;
        sb[0] = ' ';
        for (const BYTE *p = info.lead_byte; p[0]; p += 2)
            if (p[0] <= p[1])
                memset_(sb + p[0], ' ', (size_t)(p[1] - p[0] + 1));
        const int cp = *glob<int *>(MBCODEPAGE), lcid = *glob<int *>(MBLCID);
        using StringType = int(__cdecl *)(DWORD, const BYTE *, int, WORD *, int, int, int);
        using MapString = int(__cdecl *)(int, DWORD, const BYTE *, int, BYTE *, int, int, int);
        fn<StringType>(0x5db59e)(1, sb, 0x100, ctype1, cp, lcid, 0);   // __crtGetStringTypeA(CT_CTYPE1)
        fn<MapString>(0x5d437b)(*glob<int *>(MBLCID), 0x100, sb, 0x100, lower, 0x100, *glob<int *>(MBCODEPAGE), 0);
        fn<MapString>(0x5d437b)(*glob<int *>(MBLCID), 0x200, sb, 0x100, upper, 0x100, *glob<int *>(MBCODEPAGE), 0);
        for (unsigned i = 0; i < 0x100; i++) {
            if (ctype1[i] & 1) {   // C1_UPPER
                type[i + 1] |= 0x10;
                casemap[i] = lower[i];
            } else if (ctype1[i] & 2) {   // C1_LOWER
                type[i + 1] |= 0x20;
                casemap[i] = upper[i];
            } else
                casemap[i] = 0;
        }
        return;
    }
    for (unsigned i = 0; i < 0x100; i++) {
        if (i >= 'A' && i <= 'Z') {
            type[i + 1] |= 0x10;
            casemap[i] = (BYTE)(i + 0x20);
        } else if (i >= 'a' && i <= 'z') {
            type[i + 1] |= 0x20;
            casemap[i] = (BYTE)(i - 0x20);
        } else
            casemap[i] = 0;
    }
}

// _setmbcp 0x5dcb8e: the multibyte code page: lead byte ranges from the runtime's table for the known ones,
// from GetCPInfo for others, single-byte for 0; 0, or -1 when an explicit code page is unknown
int __cdecl setmbcp(int codepage)
{
    int result = 0;
    lock(MB_CP_LOCK);
    const int cp = fn<int(__cdecl *)(int)>(0x5dcd3b)(codepage);
    if (cp == *glob<int *>(MBCODEPAGE)) {
        unlock(MB_CP_LOCK);
        return 0;
    }
    if (!cp) {
        set_sbcs();
        set_sb_up_low();
        unlock(MB_CP_LOCK);
        return 0;
    }
    int i = 0;
    DWORD entry;
    for (entry = glob_addr(RGCPINFO); entry < glob_addr(RGCPINFO_END); entry += 0x30, i++)
        if (*at<int *>(entry) == cp)
            break;
    if (entry < glob_addr(RGCPINFO_END)) {
        clear_mbctype();
        for (int j = 0; j < 4; j++) {
            const BYTE *range = at<const BYTE *>(glob_addr(RGCPINFO) + (DWORD)i * 0x30 + 0x10 + (DWORD)j * 8);
            for (const BYTE *p = range; p[0] && p[1]; p += 2)
                for (unsigned c = p[0]; c <= p[1]; c++)
                    mbctype()[c + 1] |= glob<const BYTE *>(RGCTYPEFLAG)[j];
        }
        *glob<int *>(ISMBCODEPAGE) = 1;
        *glob<int *>(MBCODEPAGE) = cp;
        const int lcid = cp_to_lcid(cp);
        memcpy(glob<void *>(MBULINFO), at<const void *>(glob_addr(RGCPINFO) + (DWORD)i * 0x30 + 4), 12);
        *glob<int *>(MBLCID) = lcid;
        set_sb_up_low();
        unlock(MB_CP_LOCK);
        return 0;
    }
    CpInfo info;
    if (api<BOOL(WINAPI *)(DWORD, CpInfo *)>(iatz::GetCPInfo)((DWORD)cp, &info) != 1) {
        if (*glob<int *>(SYSTEM_SET)) {
            set_sbcs();
            set_sb_up_low();
        } else
            result = -1;
        unlock(MB_CP_LOCK);
        return result;
    }
    *glob<int *>(MBLCID) = 0;
    clear_mbctype();
    *glob<int *>(MBCODEPAGE) = cp;
    if (info.max_char_size > 1) {
        if (info.lead_byte[0])
            for (const BYTE *p = info.lead_byte; p[1];) {
                for (unsigned c = p[0]; c <= p[1]; c++)
                    mbctype()[c + 1] |= 4;
                p += 2;
                if (!p[0])
                    break;
            }
        for (unsigned c = 1; c < 0xff; c++)
            mbctype()[c + 1] |= 8;
        *glob<int *>(MBLCID) = cp_to_lcid(cp);
        *glob<int *>(ISMBCODEPAGE) = 1;
    } else
        *glob<int *>(ISMBCODEPAGE) = 0;
    memset_(glob<void *>(MBULINFO), 0, 12);
    set_sb_up_low();
    unlock(MB_CP_LOCK);
    return 0;
}

// __initmbctable 0x5dcf66: _setmbcp(_MB_CP_ANSI) once
void __cdecl initmbctable()
{
    if (*glob<int *>(MBCTYPE_INITIALIZED))
        return;
    fn<int(__cdecl *)(int)>(0x5dcb8e)(-3);
    *glob<int *>(MBCTYPE_INITIALIZED) = 1;
}

// ---- exceptions as signals (winxfltr.c, winsig.c, abort.c, hooks.cpp) -----------------------------------------

namespace {

constexpr DWORD FIRST_FPE_INDX = 0x62c478, NUM_FPE = 0x62c47c, XCPT_ACT_TAB_COUNT = 0x62c484;
constexpr DWORD CTRLC_ACTION = 0x715c2c, CTRLBREAK_ACTION = 0x715c30, ABORT_ACTION = 0x715c34, TERM_ACTION = 0x715c38;
constexpr int SIGNAL_LOCK = 1;

// _XCPT_ACTION: the exception, its signal, the handler (0 default, 1 ignore, 5 acknowledge)
struct XcptAction {
    DWORD xcptnum;
    int signum;
    DWORD action;
};

inline BYTE *ptd_bytes() { return reinterpret_cast<BYTE *>(fn<ThreadData *(__cdecl *)()>(0x5d4f28)()); }
inline XcptAction *&ptd_table(BYTE *ptd) { return *reinterpret_cast<XcptAction **>(ptd + 0x50); }   // _pxcptacttab
inline void *&ptd_infoptrs(BYTE *ptd) { return *reinterpret_cast<void **>(ptd + 0x54); }          // _tpxcptinfoptrs
inline int &ptd_fpecode(BYTE *ptd) { return *reinterpret_cast<int *>(ptd + 0x58); }              // _tfpecode

// every SIGFPE entry of the thread's action table back to the default
void reset_fpe_actions(BYTE *ptd)
{
    for (int i = *glob<int *>(FIRST_FPE_INDX); i < *glob<int *>(FIRST_FPE_INDX) + *glob<int *>(NUM_FPE); i++)
        ptd_table(ptd)[i].action = 0;
}

using Handler1 = void(__cdecl *)(int);
using Handler2 = void(__cdecl *)(int, int);

}  // namespace

// xcptlookup 0x5dad0e: the action table entry of an exception code, null when it has none
XcptAction *__cdecl xcptlookup(DWORD xcptnum, XcptAction *table)
{
    XcptAction *const end = table + *glob<int *>(XCPT_ACT_TAB_COUNT);
    XcptAction *p = table;
    if (p->xcptnum != xcptnum)
        for (p++; p < end && p->xcptnum != xcptnum; p++) {
        }
    return p < end && p->xcptnum == xcptnum ? p : nullptr;
}

// siglookup 0x5dd65d: the action table entry of a signal, null when it has none
XcptAction *__cdecl siglookup(int signum, XcptAction *table)
{
    XcptAction *const end = table + *glob<int *>(XCPT_ACT_TAB_COUNT);
    XcptAction *p = table;
    if (p->signum != signum)
        for (p++; p < end && p->signum != signum; p++) {
        }
    return p < end && p->signum == signum ? p : nullptr;
}

// _XcptFilter 0x5dabd0: a hardware exception passed to its signal handler (SIGFPE with the FPE code);
// without one the system's UnhandledExceptionFilter. The game's WinMainCRTStartup has its own inline filter.
int __cdecl xcpt_filter(DWORD xcptnum, void *infoptrs)
{
    BYTE *ptd = ptd_bytes();
    XcptAction *x = fn<XcptAction *(__cdecl *)(DWORD, XcptAction *)>(0x5dad0e)(xcptnum, ptd_table(ptd));
    const DWORD handler = x ? x->action : 0;
    if (!handler)
        return api<int(WINAPI *)(void *)>(0x5ea040)(infoptrs);   // UnhandledExceptionFilter
    if (handler == 5) {   // SIG_ACKNOWLEDGE
        x->action = 0;
        return 1;
    }
    if (handler == 1)   // SIG_IGN
        return -1;
    void *const old_infoptrs = ptd_infoptrs(ptd);
    ptd_infoptrs(ptd) = infoptrs;
    if (x->signum == 8) {   // SIGFPE
        reset_fpe_actions(ptd);
        const int old_code = ptd_fpecode(ptd);
        switch (x->xcptnum) {
        case 0xc000008e: ptd_fpecode(ptd) = 0x83; break;   // _FPE_ZERODIVIDE
        case 0xc0000090: ptd_fpecode(ptd) = 0x81; break;   // _FPE_INVALID
        case 0xc0000091: ptd_fpecode(ptd) = 0x84; break;   // _FPE_OVERFLOW
        case 0xc0000093: ptd_fpecode(ptd) = 0x85; break;   // _FPE_UNDERFLOW
        case 0xc000008d: ptd_fpecode(ptd) = 0x82; break;   // _FPE_DENORMAL
        case 0xc000008f: ptd_fpecode(ptd) = 0x86; break;   // _FPE_INEXACT
        case 0xc0000092: ptd_fpecode(ptd) = 0x8a; break;   // _FPE_STACKOVERFLOW
        }
        reinterpret_cast<Handler2>(handler)(8, ptd_fpecode(ptd));
        ptd_fpecode(ptd) = old_code;
    } else {
        x->action = 0;
        reinterpret_cast<Handler1>(handler)(x->signum);
    }
    ptd_infoptrs(ptd) = old_infoptrs;
    return -1;
}

// raise 0x5dd4db: the signal's handler (the global ones for SIGINT, SIGBREAK, SIGABRT, SIGTERM under the
// signal lock, the thread's table for SIGILL, SIGFPE, SIGSEGV), reset to the default first; SIG_DFL exits
// with 3; -1 for an unknown signal
int __cdecl raise_(int signum)
{
    int siglock = 0;
    DWORD *psigact;
    BYTE *ptd = nullptr;
    switch (signum) {
    case 2: psigact = glob<DWORD *>(CTRLC_ACTION); break;
    case 21: psigact = glob<DWORD *>(CTRLBREAK_ACTION); break;
    case 22: psigact = glob<DWORD *>(ABORT_ACTION); break;
    case 15: psigact = glob<DWORD *>(TERM_ACTION); break;
    case 4:
    case 8:
    case 11:
        ptd = ptd_bytes();
        psigact = &fn<XcptAction *(__cdecl *)(int, XcptAction *)>(0x5dd65d)(signum, ptd_table(ptd))->action;
        break;
    default:
        return -1;
    }
    const DWORD sigact = *psigact;
    if (!ptd) {
        lock(SIGNAL_LOCK);
        siglock = 1;
    }
    if (sigact == 1) {   // SIG_IGN
        if (siglock)
            unlock(SIGNAL_LOCK);
        return 0;
    }
    if (!sigact) {   // SIG_DFL
        if (siglock)
            unlock(SIGNAL_LOCK);
        fn<void(__cdecl *)(int)>(0x5d0e47)(3);   // _exit
    }
    void *old_infoptrs = nullptr;
    int old_code = 0;
    if (signum == 8 || signum == 11 || signum == 4) {
        old_infoptrs = ptd_infoptrs(ptd);
        ptd_infoptrs(ptd) = nullptr;
        if (signum == 8) {
            old_code = ptd_fpecode(ptd);
            ptd_fpecode(ptd) = 0x8c;   // _FPE_EXPLICITGEN
        }
    }
    if (signum == 8)
        reset_fpe_actions(ptd);
    else
        *psigact = 0;
    if (siglock)
        unlock(SIGNAL_LOCK);
    if (signum == 8)
        reinterpret_cast<Handler2>(sigact)(8, ptd_fpecode(ptd));
    else {
        reinterpret_cast<Handler1>(sigact)(signum);
        if (signum != 11 && signum != 4)
            return 0;
    }
    ptd_infoptrs(ptd) = old_infoptrs;
    if (signum == 8)
        ptd_fpecode(ptd) = old_code;
    return 0;
}

// abort 0x5dd087: "abnormal program termination", raise(SIGABRT), _exit(3)
void __cdecl abort_()
{
    fn<void(__cdecl *)(int)>(0x5db211)(10);   // _NMSG_WRITE(_RT_ABORT)
    fn<int(__cdecl *)(int)>(0x5dd4db)(22);
    fn<void(__cdecl *)(int)>(0x5d0e47)(3);
}

// __CxxUnhandledExceptionFilter 0x5dd00b: an uncaught C++ exception (MSVC's 0xe06d7363, magic 0x19930520)
// goes to terminate; anything else to the filter before it, if that one can still be executed
int WINAPI cxx_unhandled_exception_filter(void **pointers)
{
    const DWORD *rec = static_cast<const DWORD *>(pointers[0]);
    if (rec[0] == 0xe06d7363 && rec[4] == 3 && rec[5] == 0x19930520)
        fn<void(__cdecl *)()>(0x5db4bc)();   // terminate
    using Filter = int(WINAPI *)(void **);
    const Filter prev = *glob<Filter *>(PREV_FILTER);
    if (prev && fn<int(__cdecl *)(void *)>(0x5dd06f)((void *)prev))
        return prev(pointers);
    return 0;
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_startup)
{
    const Hook list[] = {
        {0x5d6936, (void *)&heap_init, "_heap_init", 8, {0x33, 0xc0, 0x6a, 0x00, 0x39, 0x44, 0x24, 0x08}},
        {0x5d67ee, (void *)&heap_select, "__heap_select", 8, {0x55, 0x8b, 0xec, 0xb8, 0x2c, 0x12, 0x00, 0x00}},
        {0x5d67c1, (void *)&get_linker_version, "_GetLinkerVersion", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x6a, 0x00, 0x83}},
        {0x5d6993, (void *)&sbh_heap_init, "__sbh_heap_init", 5, {0x68, 0x40, 0x01, 0x00, 0x00, 0x6a, 0x00, 0xff}},
        {0x5d4ec1, (void *)&mtinit, "_mtinit", 6, {0x56, 0xe8, 0x15, 0xf4, 0xff, 0xff, 0xff, 0x15}, 2},
        {0x5d42dc, (void *)&mtinitlocks, "_mtinitlocks", 7, {0x56, 0x8b, 0x35, 0x14, 0xa1, 0x5e, 0x00, 0xff}},
        {0x5d0e09, (void *)&cinit, "_cinit", 5, {0xa1, 0x24, 0x96, 0x62, 0x00, 0x85, 0xc0, 0x74}},
        {0x5d0875, (void *)&onexitinit, "__onexitinit", 5, {0x68, 0x80, 0x00, 0x00, 0x00, 0xe8, 0xd0, 0x0d}},
        {0x5d3710, (void *)&inittime, "__inittime", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x8d, 0x45, 0xf8}},
        {0x5d3a74, (void *)&initstdio, "__initstdio", 5, {0xa1, 0x40, 0x75, 0x71, 0x00, 0x56, 0x6a, 0x14}},
        {0x5d3b1c, (void *)&endstdio, "__endstdio", 5, {0xe8, 0xee, 0xdd, 0xff, 0xff, 0x80, 0x3d, 0xb4}, 1},
        {0x5d190f, (void *)&flushall, "_flushall", 7, {0x6a, 0x01, 0xe8, 0x02, 0x00, 0x00, 0x00, 0x59}, 3},
        {0x5db51d, (void *)&fcloseall, "_fcloseall", 6, {0x53, 0x57, 0x6a, 0x02, 0x33, 0xdb, 0xe8, 0xdd}},
        {0x5dd051, (void *)&inittermination, "__inittermination", 5, {0x68, 0x0b, 0xd0, 0x5d, 0x00, 0xff, 0x15, 0xa0}},
        {0x5dd062, (void *)&termination, "__termination", 6, {0xff, 0x35, 0x28, 0x5c, 0x71, 0x00, 0xff, 0x15}},
        {0x5dd06f, (void *)&validate_execute, "_ValidateExecute", 8, {0x56, 0x6a, 0x01, 0x5e, 0xff, 0x74, 0x24, 0x08}},
        {0x5d3e70, (void *)&positive, "_positive", 6, {0x8b, 0x44, 0x24, 0x04, 0xdd, 0x00, 0xdc, 0x1d}},
        {0x5d8957, (void *)&ioinit, "_ioinit", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x48, 0x53, 0x56}},
        {0x5daef2, (void *)&parse_cmdline, "parse_cmdline", 6, {0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x18, 0x8b, 0x45}},
        {0x5dae59, (void *)&setargv, "_setargv", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x53, 0x33, 0xdb}},
        {0x5dada0, (void *)&setenvp, "_setenvp", 9, {0x53, 0x33, 0xdb, 0x39, 0x1d, 0x54, 0x75, 0x71}},
        {0x5db0a6, (void *)&crt_get_environment_strings, "__crtGetEnvironmentStringsA", 7, {0x51, 0x51, 0xa1, 0xa4, 0x5b, 0x71, 0x00, 0x53}},
        {0x5dad48, (void *)&wincmdln, "_wincmdln", 7, {0x83, 0x3d, 0x54, 0x75, 0x71, 0x00, 0x00, 0x75}},
        {0x5dcb5d, (void *)&x_ismbbtype, "x_ismbbtype", 5, {0x0f, 0xb6, 0x44, 0x24, 0x04, 0x8a, 0x4c, 0x24}},
        {0x5dcb4c, (void *)&ismbblead, "_ismbblead", 8, {0x6a, 0x04, 0x6a, 0x00, 0xff, 0x74, 0x24, 0x0c}},
        {0x5dcd3b, (void *)&get_system_cp, "getSystemCP", 11, {0x8b, 0x44, 0x24, 0x04, 0x83, 0x25, 0x18, 0x5c}},
        {0x5dcd85, (void *)&cptolcid, "CPtoLCID", 9, {0x8b, 0x44, 0x24, 0x04, 0x2d, 0xa4, 0x03, 0x00}},
        {0x5dcdb8, (void *)&setsbcs, "setSBCS", 6, {0x57, 0x6a, 0x40, 0x59, 0x33, 0xc0, 0xbf, 0xe0}},
        {0x5dcde1, (void *)&setsbuplow, "setSBUpLow", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x14, 0x05, 0x00}},
        {0x5dcb8e, (void *)&setmbcp, "_setmbcp", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x53, 0x56}},
        {0x5dcf66, (void *)&initmbctable, "__initmbctable", 7, {0x83, 0x3d, 0x54, 0x75, 0x71, 0x00, 0x00, 0x75}},
        {0x5dad0e, (void *)&xcptlookup, "xcptlookup", 10, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x0d, 0x84, 0xc4}},
        {0x5dd65d, (void *)&siglookup, "siglookup", 10, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x0d, 0x84, 0xc4}},
        {0x5dabd0, (void *)&xcpt_filter, "_XcptFilter", 5, {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0xe8, 0x4d}},
        {0x5dd4db, (void *)&raise_, "raise", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
        {0x5dd087, (void *)&abort_, "abort", 7, {0x6a, 0x0a, 0xe8, 0x83, 0xe1, 0xff, 0xff, 0x6a}, 3},
        {0x5dd00b, (void *)&cxx_unhandled_exception_filter, "__CxxUnhandledExceptionFilter", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0x06, 0x81}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
