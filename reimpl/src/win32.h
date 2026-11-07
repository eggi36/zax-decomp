// The few Windows functions the reimplementation needs, declared here so the MSVC ABI build needs no
// Windows SDK. zig provides the import libraries (kernel32, user32).
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef int BOOL;
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef unsigned long DWORD;
typedef int32_t INT32;
typedef void *HANDLE;
typedef void *HINSTANCE;
typedef void *LPVOID;

#define WINAPI __stdcall
#define WINIMPORT extern "C" __declspec(dllimport)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MAX_PATH 260
#define TRUE 1
#define FALSE 0
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_READWRITE 0x04
#define DLL_PROCESS_ATTACH 1
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define TH32CS_SNAPTHREAD 4

struct THREADENTRY32 {
    DWORD dwSize, cntUsage, th32ThreadID, th32OwnerProcessID;
    long tpBasePri, tpDeltaPri;
    DWORD dwFlags;
};

// runtime.cpp
extern "C" void *memset(void *dst, int c, size_t n);
extern "C" void *memcpy(void *dst, const void *src, size_t n);
extern "C" int memcmp(const void *a, const void *b, size_t n);

WINIMPORT HANDLE WINAPI CreateFileA(const char *name, DWORD access, DWORD share, void *sec, DWORD disposition,
                                    DWORD flags, HANDLE templ);
WINIMPORT BOOL WINAPI WriteFile(HANDLE f, const void *buf, DWORD n, DWORD *written, void *overlapped);
WINIMPORT DWORD WINAPI GetModuleFileNameA(HINSTANCE module, char *out, DWORD size);
WINIMPORT const char *WINAPI GetCommandLineA(void);
WINIMPORT DWORD WINAPI GetPrivateProfileStringA(const char *section, const char *key, const char *def, char *out,
                                                DWORD size, const char *file);
WINIMPORT BOOL WINAPI VirtualProtect(void *addr, size_t size, DWORD prot, DWORD *old);
WINIMPORT BOOL WINAPI FlushInstructionCache(HANDLE process, const void *addr, size_t size);
WINIMPORT HANDLE WINAPI GetCurrentProcess(void);
WINIMPORT BOOL WINAPI DisableThreadLibraryCalls(HINSTANCE module);
WINIMPORT void *WINAPI VirtualAlloc(void *addr, size_t size, DWORD type, DWORD prot);
WINIMPORT HANDLE WINAPI CreateThread(void *sec, size_t stack, DWORD(WINAPI *start)(void *), void *arg, DWORD flags,
                                     DWORD *id);
WINIMPORT void WINAPI Sleep(DWORD ms);
WINIMPORT unsigned WINAPI GetPrivateProfileIntA(const char *section, const char *key, int def, const char *file);
WINIMPORT int WINAPI lstrlenA(const char *s);
WINIMPORT char *WINAPI lstrcpyA(char *dst, const char *src);
WINIMPORT char *WINAPI lstrcatA(char *dst, const char *src);
WINIMPORT int WINAPI wvsprintfA(char *out, const char *fmt, va_list ap);
WINIMPORT int __cdecl wsprintfA(char *out, const char *fmt, ...);
WINIMPORT HANDLE WINAPI CreateToolhelp32Snapshot(DWORD flags, DWORD process);
WINIMPORT BOOL WINAPI Thread32First(HANDLE snapshot, THREADENTRY32 *entry);
WINIMPORT BOOL WINAPI Thread32Next(HANDLE snapshot, THREADENTRY32 *entry);
WINIMPORT BOOL WINAPI CloseHandle(HANDLE h);
WINIMPORT DWORD WINAPI GetCurrentProcessId(void);
WINIMPORT DWORD WINAPI GetCurrentThreadId(void);
