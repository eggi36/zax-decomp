// The MSVC 6.0 C runtime of Zax.exe, reimplemented (docs/DECOMPILATION.md 10, "C runtime"): the shared
// pieces of the crt_*.cpp modules. The runtime's data stays where the exe has it (.data: the heap handle,
// the lock table, the stream table, the per-thread data index, the ctype table ...), so the reimplemented
// functions and the exe's remaining runtime code work on the same state. Windows is called through the
// exe's import slots (as the original does), so checks can put stand-ins there (patch_import).
//
// Constants the exe never changes after startup decide some branches: the exe has no setlocale, so
// __lc_handle[LC_CTYPE] (0x71596c) stays 0 and __mb_cur_max (0x629d6c) stays 1; no instruction writes
// either (scan of .text, 2026-10-28). The branches for another locale or a multibyte code page are kept
// by calling the original function (its trampoline), like the exe would; they are never taken.
#pragma once
#include "reimpl.h"

namespace zax::crt {

// ---- the runtime's data in the exe ----------------------------------------------------------------------

constexpr DWORD LC_HANDLE_CTYPE = 0x71596c;      // __lc_handle[LC_CTYPE]
constexpr DWORD MB_CUR_MAX = 0x629d6c;           // __mb_cur_max
constexpr DWORD PCTYPE = 0x629b60;               // _pctype (pointer to the ctype table, index -1..255)
constexpr DWORD SETLC_ACTIVE = 0x716528, UNGUARDED_READLC = 0x71652c;
constexpr DWORD ACTIVE_HEAP = 0x716524;          // __active_heap: 1 system, 2 V5 small blocks, 3 V6 small blocks
constexpr DWORD CRT_HEAP = 0x716520;             // _crtheap
constexpr DWORD SBH_THRESHOLD = 0x71651c;        // __sbh_threshold (V6)
constexpr DWORD OLD_SBH_THRESHOLD = 0x62c0ec;    // __old_sbh_threshold (V5, paragraphs * 16)
constexpr DWORD NEWMODE = 0x715988;              // _newmode
constexpr DWORD PNH = 0x71598c;                  // _pnhHeap (new handler)
constexpr DWORD TLS_INDEX = 0x629f64;            // __tlsindex
constexpr DWORD LOCK_TABLE = 0x629d90;           // _locktable[] of CRITICAL_SECTION pointers
constexpr DWORD ERRNO_TABLE = 0x629630, ERRNO_TABLE_END = 0x629798;   // {oscode, errno} pairs of _dosmaperr
constexpr DWORD INITIAL_LOCALE_INFO = 0x62c400;  // ptd->_ptlocinfo... (+0x50) set by _initptd

// Lock numbers of the exe's runtime (_lock)
constexpr int HEAP_LOCK = 9, LOCKTAB_LOCK = 0x11, SETLOCALE_LOCK = 0x13;

// ---- the exe's import slots (kernel32) ------------------------------------------------------------------

namespace iat {
constexpr DWORD EnterCriticalSection = 0x5ea098, LeaveCriticalSection = 0x5ea094, InitializeCriticalSection = 0x5ea114,
                GetLastError = 0x5ea150, SetLastError = 0x5ea190, TlsGetValue = 0x5ea194, TlsSetValue = 0x5ea188,
                GetCurrentThreadId = 0x5ea184, HeapAlloc = 0x5ea154, HeapFree = 0x5ea16c, HeapReAlloc = 0x5ea170,
                HeapSize = 0x5ea174, InterlockedIncrement = 0x5ea164, InterlockedDecrement = 0x5ea168,
                SetFilePointer = 0x5ea054, ReadFile = 0x5ea12c, WriteFile = 0x5ea0dc, CloseHandle = 0x5ea0d8,
                CreateFileA = 0x5ea0d4, GetFileType = 0x5ea048, FlushFileBuffers = 0x5ea178, SetEndOfFile = 0x5ea148,
                DeleteFileA = 0x5ea14c, MoveFileA = 0x5ea144, SetStdHandle = 0x5ea044, GetLocalTime = 0x5ea138,
                GetSystemTime = 0x5ea13c, GetTimeZoneInformation = 0x5ea140, ExitProcess = 0x5ea160,
                GetModuleHandleA = 0x5ea130, GetProcAddress = 0x5ea068, WideCharToMultiByte = 0x5ea120,
                MultiByteToWideChar = 0x5ea0a8, RaiseException = 0x5ea198;
}
template <typename F> inline F api(ImpKey slot) { return imp<F>(slot); }   // through the layer (our own IAT once it moved)

// ---- per-thread data (_getptd 0x5d4f28) -------------------------------------------------------------------

// The fields the runtime uses (struct _tiddata of VC6, 0x74 bytes)
struct ThreadData {
    DWORD tid;            // +0x00
    DWORD thandle;        // +0x04
    int terrno;           // +0x08 errno
    DWORD tdoserrno;      // +0x0c _doserrno
    DWORD fpds;           // +0x10
    DWORD holdrand;       // +0x14 rand
    char *token;          // +0x18 strtok
    char *wtoken;        // +0x1c
    char *mtoken;         // +0x20
    char *errmsg;         // +0x24 strerror
    char *namebuf0;       // +0x28 tmpnam
    char *namebuf1;       // +0x2c tmpfile
    char *asctimebuf;     // +0x30 asctime
    void *wasctimebuf;    // +0x34
    void *gmtimebuf;      // +0x38 gmtime / localtime
    char *cvtbuf;         // +0x3c ecvt / fcvt
    BYTE pad2[0x74 - 0x40];
};
static_assert(sizeof(ThreadData) == 0x74, "_tiddata");

ThreadData *getptd();
int *errno_ptr();
DWORD *doserrno_ptr();
void dosmaperr(DWORD oserror);
void lock(int n);
void unlock(int n);

// ---- streams and low-level files ------------------------------------------------------------------------

// FILE of VC6 (0x20 bytes); the exe's _iob has 20 of them at 0x6298e0, __piob the table of all
struct File {
    char *ptr;
    int cnt;
    char *base;
    int flag;
    int file;
    int charbuf;
    int bufsiz;
    char *tmpfname;
};
static_assert(sizeof(File) == 0x20, "FILE");
constexpr int IOREAD = 1, IOWRT = 2, IOMYBUF = 8, IOEOF = 0x10, IOERR = 0x20, IOSTRG = 0x40, IORW = 0x80,
              IOYOURBUF = 0x100, IONBF = 4, IOSETVBUF = 0x400, IOFEOF = 0x800, IOFLRTN = 0x1000, IOCTRLZ = 0x2000,
              IOCOMMIT = 0x4000;

// ioinfo of a low-level file handle (0x24 bytes): __pioinfo[h >> 5][h & 31]; handles at or above _nhandle
// are invalid; __badioinfo (0x62c0f0) stands for handle -1
struct IoInfo {
    HANDLE osfhnd;
    BYTE osfile;
    BYTE pipech;
    BYTE pad[2];
    int lockinitflag;
    BYTE lock[0x18];
};
static_assert(sizeof(IoInfo) == 0x24, "ioinfo");
constexpr DWORD PIOINFO = 0x716400, NHANDLE = 0x716500, BADIOINFO = 0x62c0f0;
constexpr BYTE FOPEN = 1, FEOFLAG = 2, FCRLF = 4, FPIPE = 8, FNOINHERIT = 0x10, FAPPEND = 0x20, FDEV = 0x40, FTEXT = 0x80;
inline IoInfo *ioinfo(int h)
{
    if (h == -1)
        return glob<IoInfo *>(BADIOINFO);
    return *at<IoInfo **>(glob_addr(PIOINFO) + 4 * (DWORD)(h >> 5)) + (h & 31);
}

// The heap through the exe's entry points, as the original runtime calls it: checks put stand-ins at
// these addresses (an arena behind operator new 0x5d033a and free 0x5d33e0), so a reimplemented function
// that calls the heap goes through them, not straight to crt_heap.cpp
inline void *exe_malloc(size_t n) { return fn<void *(__cdecl *)(size_t)>(0x5d164f)(n); }
inline void exe_free(void *p) { fn<void(__cdecl *)(void *)>(0x5d33e0)(p); }
inline void *exe_calloc(size_t a, size_t b) { return fn<void *(__cdecl *)(size_t, size_t)>(0x5d34c9)(a, b); }
inline void *exe_realloc(void *p, size_t n) { return fn<void *(__cdecl *)(void *, size_t)>(0x5d3745)(p, n); }
inline size_t exe_msize(void *p) { return fn<size_t(__cdecl *)(void *)>(0x5d41ee)(p); }

// Functions of the crt_*.cpp modules the others use
void *__cdecl malloc_(size_t n);                                  // crt_heap.cpp
void __cdecl free_(void *p);
void *__cdecl memmove_(void *d, const void *s, size_t n);         // crt_string.cpp
void *__cdecl memset_(void *d, int c, size_t n);
size_t __cdecl strlen_(const char *s);
void __cdecl lock_file(BYTE *f);
void __cdecl unlock_file(BYTE *f);
void __cdecl lock_file2(int i, BYTE *f);
void __cdecl unlock_file2(int i, BYTE *f);
int __cdecl output(File *f, const char *format, char *ap);        // crt_format.cpp
int __cdecl flsbuf(int c, File *f);     // crt_stdio.cpp
int __cdecl filbuf(File *f);
int __cdecl ungetc_lk(int c, File *f);

// ---- character classes (C locale; _pctype of the exe) ------------------------------------------------------

constexpr int UPPER = 1, LOWER = 2, DIGIT = 4, SPACE = 8, PUNCT = 0x10, CONTROL = 0x20, BLANK = 0x40, HEX = 0x80;

int isctype(int c, int mask);
// What the exe's macros (isspace(c) etc. in the runtime's own code) do: _isctype when __mb_cur_max > 1,
// else the table entry
inline int ctype(int c, int mask)
{
    if (*glob<int *>(MB_CUR_MAX) > 1)
        return isctype(c, mask);
    return (*glob<const WORD **>(PCTYPE))[c] & mask;
}

}  // namespace zax::crt
