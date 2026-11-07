// The C runtime's exit path and its run-time error messages: atexit/_onexit and the exit table, exit,
// _exit, doexit, _initterm, _lockexit, _amsg_exit, _FF_MSGBANNER, _NMSG_WRITE, __crtMessageBoxA,
// _purecall (docs/DECOMPILATION.md 10, "C runtime").
#include "crt_common.h"

namespace zax::crt {

namespace {

constexpr DWORD ONEXIT_BEGIN = 0x71755c, ONEXIT_END = 0x717558;   // __onexitbegin, __onexitend
constexpr DWORD C_EXIT_DONE = 0x7158bc, C_TERMINATION_DONE = 0x7158b8, EXITFLAG = 0x7158b4;
constexpr DWORD ERROR_MODE = 0x7158e0, APP_TYPE = 0x6298d8, AEXIT_RTN = 0x6298d4, PRINT_HOOK = 0x715ba8;
constexpr DWORD XP_A = 0x60a40c, XP_Z = 0x60a414, XT_A = 0x60a418, XT_Z = 0x60a420;   // pre-terminators, terminators
constexpr DWORD RTERRS = 0x62c488, RTERRS_END = 0x62c518;   // {number, text} of the run-time errors
constexpr DWORD MSGBOX = 0x715c1c, GET_ACTIVE_WINDOW = 0x715c20, GET_LAST_ACTIVE_POPUP = 0x715c24;
constexpr DWORD TXT_UNKNOWN = 0x607640, TXT_DOTS = 0x623cfc, TXT_RUNTIME_ERROR = 0x607624, TXT_NEWLINES = 0x607620,
                TXT_TITLE = 0x6075f8, TXT_USER32 = 0x6076ac, TXT_MESSAGEBOX = 0x6076a0, TXT_ACTIVE = 0x607690,
                TXT_POPUP = 0x60767c;
constexpr DWORD IAT_GET_STD_HANDLE = 0x5ea04c, IAT_GET_MODULE_FILE_NAME = 0x5ea0e0, IAT_LOAD_LIBRARY = 0x5ea06c,
                IAT_TERMINATE_PROCESS = 0x5ea15c, IAT_GET_CURRENT_PROCESS = 0x5ea158;

}  // namespace

size_t __cdecl msize(void *p);                                   // crt_heap.cpp
void *__cdecl realloc_(void *p, size_t n);
size_t __cdecl strlen_(const char *s);                           // crt_string.cpp
char *__cdecl strcpy_(char *d, const char *s);
char *__cdecl strcat_(char *d, const char *s);
char *__cdecl strncpy_(char *d, const char *s, size_t n);

using Pfv = void(__cdecl *)();

// _lockexit 0x5d0efd / _unlockexit 0x5d0f06: lock 13
void __cdecl lockexit() { lock(0xd); }
void __cdecl unlockexit() { unlock(0xd); }

// _initterm 0x5d0f0f: every non-null function pointer of [begin, end)
void __cdecl initterm(Pfv *begin, Pfv *end)
{
    for (; begin < end; begin++)
        if (*begin)
            (*begin)();
}

// _onexit 0x5d07e5: appends to the exit table, which grows by 16 bytes (4 entries) when _msize says it is
// full; null when that fails
Pfv __cdecl onexit(Pfv f)
{
    lockexit();
    Pfv *begin = *glob<Pfv **>(ONEXIT_BEGIN), *end = *glob<Pfv **>(ONEXIT_END);
    if (exe_msize(begin) < (size_t)((BYTE *)end - (BYTE *)begin) + 4) {
        Pfv *grown = (Pfv *)exe_realloc(begin, exe_msize(begin) + 16);
        if (!grown) {
            unlockexit();
            return nullptr;
        }
        end = grown + (end - begin);
        *glob<Pfv **>(ONEXIT_BEGIN) = grown;
    }
    *end = f;
    *glob<Pfv **>(ONEXIT_END) = end + 1;
    unlockexit();
    return f;
}

// atexit 0x5d0863: 0 or -1
int __cdecl atexit_(Pfv f) { return onexit(f) ? 0 : -1; }

// doexit 0x5d0e58: a second exit from within the exit code terminates the process at once. quick (_exit,
// _c_exit) skips the exit table and the pre-terminators; retcaller (_cexit) returns instead of ExitProcess.
void __cdecl doexit(int code, int quick, int retcaller)
{
    lockexit();
    if (*glob<int *>(C_EXIT_DONE) == 1)
        api<BOOL(WINAPI *)(HANDLE, unsigned)>(IAT_TERMINATE_PROCESS)(api<HANDLE(WINAPI *)()>(IAT_GET_CURRENT_PROCESS)(), (unsigned)code);
    *glob<int *>(C_TERMINATION_DONE) = 1;
    *glob<BYTE *>(EXITFLAG) = (BYTE)retcaller;
    if (!quick) {
        if (*glob<Pfv **>(ONEXIT_BEGIN)) {
            // the end is read once, the begin before every entry (an exit function may register another)
            for (Pfv *p = *glob<Pfv **>(ONEXIT_END) - 1; p >= *glob<Pfv **>(ONEXIT_BEGIN); p--)
                if (*p)
                    (*p)();
        }
        initterm(glob<Pfv *>(XP_A), glob<Pfv *>(XP_Z));
    }
    initterm(glob<Pfv *>(XT_A), glob<Pfv *>(XT_Z));
    if (retcaller) {
        unlockexit();
        return;
    }
    *glob<int *>(C_EXIT_DONE) = 1;
    api<void(WINAPI *)(unsigned)>(iat::ExitProcess)((unsigned)code);
}

// exit 0x5d0e36, _exit 0x5d0e47
void __cdecl exit_(int code) { doexit(code, 0, 0); }
void __cdecl quick_exit(int code) { doexit(code, 1, 0); }

// __crtMessageBoxA 0x5dcf82: MessageBoxA of user32 (loaded on the first error), owned by the last active
// popup of the active window
int __cdecl crt_message_box(const char *text, const char *caption, unsigned type)
{
    using Fn = int(WINAPI *)(HANDLE, const char *, const char *, unsigned);
    HANDLE owner = nullptr;
    if (!*glob<void **>(MSGBOX)) {
        HANDLE user32 = api<HANDLE(WINAPI *)(const char *)>(IAT_LOAD_LIBRARY)(glob<const char *>(TXT_USER32));
        if (!user32)
            return 0;
        auto get = api<void *(WINAPI *)(HANDLE, const char *)>(iat::GetProcAddress);
        if (!(*glob<void **>(MSGBOX) = get(user32, glob<const char *>(TXT_MESSAGEBOX))))
            return 0;
        *glob<void **>(GET_ACTIVE_WINDOW) = get(user32, glob<const char *>(TXT_ACTIVE));
        *glob<void **>(GET_LAST_ACTIVE_POPUP) = get(user32, glob<const char *>(TXT_POPUP));
    }
    if (auto active = *glob<HANDLE(WINAPI **)()>(GET_ACTIVE_WINDOW)) {
        owner = active();
        if (owner)
            if (auto popup = *glob<HANDLE(WINAPI **)(HANDLE)>(GET_LAST_ACTIVE_POPUP))
                owner = popup(owner);
    }
    return (int)(*glob<Fn *>(MSGBOX))(owner, text, caption, type);
}

// _NMSG_WRITE 0x5db211: the message of a run-time error number to stderr (__error_mode 1, or 0 in a
// console program) or in a message box "Runtime Error! Program: ..." (the program name shortened to its
// last 59 characters behind "..."); number 252 (the banner) only on the console
void __cdecl nmsg_write(int number)
{
    DWORD i = 0;
    while (glob_addr(RTERRS) + 8 * i < glob_addr(RTERRS_END) && *at<int *>(glob_addr(RTERRS) + 8 * i) != number)
        i++;
    if (*at<int *>(glob_addr(RTERRS) + 8 * i) != number)
        return;
    const char *msg = *at<const char **>(glob_addr(RTERRS) + 8 * i + 4);
    const int mode = *glob<int *>(ERROR_MODE);
    if (mode == 1 || (mode == 0 && *glob<int *>(APP_TYPE) == 1)) {   // _OUT_TO_STDERR, or a console program
        DWORD written;
        HANDLE err = api<HANDLE(WINAPI *)(DWORD)>(IAT_GET_STD_HANDLE)((DWORD)-12);
        api<BOOL(WINAPI *)(HANDLE, const void *, DWORD, DWORD *, void *)>(iat::WriteFile)(err, msg, (DWORD)strlen_(msg), &written, nullptr);
        return;
    }
    if (number == 0xfc)
        return;
    char prog[0x104], out[0xa0];
    char *name = prog;
    if (!api<DWORD(WINAPI *)(HANDLE, char *, DWORD)>(IAT_GET_MODULE_FILE_NAME)(nullptr, prog, 0x104))
        strcpy_(prog, glob<const char *>(TXT_UNKNOWN));
    if (strlen_(prog) + 1 > 0x3c) {
        name = prog + strlen_(prog) - 0x3b;
        strncpy_(name, glob<const char *>(TXT_DOTS), 3);
    }
    strcpy_(out, glob<const char *>(TXT_RUNTIME_ERROR));
    strcat_(out, name);
    strcat_(out, glob<const char *>(TXT_NEWLINES));
    strcat_(out, msg);
    crt_message_box(out, glob<const char *>(TXT_TITLE), 0x12010);
}

// _FF_MSGBANNER 0x5db1d8: on stderr the banner (252), the print hook, a line break (255)
void __cdecl ff_msgbanner()
{
    const int mode = *glob<int *>(ERROR_MODE);
    if (mode == 1 || (mode == 0 && *glob<int *>(APP_TYPE) == 1)) {
        nmsg_write(0xfc);
        if (auto hook = *glob<Pfv *>(PRINT_HOOK))
            hook();
        nmsg_write(0xff);
    }
}

// _amsg_exit 0x5d3397: (with __error_mode 1 the banner,) the message and _aexit_rtn (_exit) with 255
void __cdecl amsg_exit(int code)
{
    if (*glob<int *>(ERROR_MODE) == 1)
        ff_msgbanner();
    nmsg_write(code);
    (*glob<void(__cdecl **)(int)>(AEXIT_RTN))(0xff);
}

// _fast_error_exit 0x5d33bc (startup): the message and ExitProcess(255)
void __cdecl fast_error_exit(int code)
{
    if (*glob<int *>(ERROR_MODE) == 1)
        ff_msgbanner();
    nmsg_write(code);
    api<void(WINAPI *)(unsigned)>(iat::ExitProcess)(0xff);
}

// _purecall 0x5d0331: run-time error R6025
int __cdecl purecall()
{
    amsg_exit(0x19);
    return 0;
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_exit)
{
    const Hook list[] = {
        {0x5d0efd, (void *)&lockexit, "_lockexit", 7, {0x6a, 0x0d, 0xe8, 0x01, 0x34, 0x00, 0x00, 0x59}, 3},
        {0x5d0f06, (void *)&unlockexit, "_unlockexit", 7, {0x6a, 0x0d, 0xe8, 0x59, 0x34, 0x00, 0x00, 0x59}, 3},
        {0x5d0f0f, (void *)&initterm, "_initterm", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0x74, 0x24}},
        {0x5d07e5, (void *)&onexit, "_onexit", 6, {0x56, 0xe8, 0x12, 0x07, 0x00, 0x00, 0xff, 0x35}, 2},
        {0x5d0863, (void *)&atexit_, "atexit", 9, {0xff, 0x74, 0x24, 0x04, 0xe8, 0x79, 0xff, 0xff}, 5},
        {0x5d0e58, (void *)&doexit, "doexit", 6, {0x57, 0xe8, 0x9f, 0x00, 0x00, 0x00, 0x6a, 0x01}, 2},
        {0x5d0e36, (void *)&exit_, "exit", 8, {0x6a, 0x00, 0x6a, 0x00, 0xff, 0x74, 0x24, 0x0c}},
        {0x5d0e47, (void *)&quick_exit, "_exit", 8, {0x6a, 0x00, 0x6a, 0x01, 0xff, 0x74, 0x24, 0x0c}},
        {0x5dcf82, (void *)&crt_message_box, "__crtMessageBoxA", 9, {0x53, 0x33, 0xdb, 0x39, 0x1d, 0x1c, 0x5c, 0x71}},
        {0x5db211, (void *)&nmsg_write, "_NMSG_WRITE", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0xa4, 0x01, 0x00}},
        {0x5db1d8, (void *)&ff_msgbanner, "_FF_MSGBANNER", 5, {0xa1, 0xe0, 0x58, 0x71, 0x00, 0x83, 0xf8, 0x01}},
        {0x5d3397, (void *)&amsg_exit, "_amsg_exit", 7, {0x83, 0x3d, 0xe0, 0x58, 0x71, 0x00, 0x01, 0x75}},
        {0x5d33bc, (void *)&fast_error_exit, "_fast_error_exit", 7, {0x83, 0x3d, 0xe0, 0x58, 0x71, 0x00, 0x01, 0x75}},
        {0x5d0331, (void *)&purecall, "_purecall", 7, {0x6a, 0x19, 0xe8, 0x5f, 0x30, 0x00, 0x00, 0x59}, 3},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
