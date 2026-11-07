// The logged start of phase 5a (docs/DECOMPILATION.md 10, batch P5a-1): with [Reimpl] TraceApi=1 in Zax-Reimpl.ini the
// DLL puts recorders into the exe's import table before anything of the exe runs (DllMain, the process still
// suspended; also with /Original), and the main thread's calls of the Windows functions the game's own code uses are
// written to Zax-Reimpl.log ("API " lines: name, arguments, answer) from the C runtime's static initializers on until
// the first PeekMessageA (the end of WinMain's start, before its loop): the window, the class, the cursor, the font,
// the files and the INI, DirectDrawCreate, the clipboard. Then a snapshot of what the start built ("API state" lines:
// the screen frame's fields and modes, DirectDraw's display mode and the surfaces' descriptions, the window's
// globals). The hooked and the /Original start must log the same lines; zax-testtools/tools/api_trace_compare.py compares them.
// The exit (phase 5 leftovers): from the main thread's PostQuitMessage (the game quit through its menu) on, the same
// recording runs again until ExitProcess ("API trace exit" … "API trace exit end"): the window procedure's quit (the
// resource cache, the application's destructor) and the C runtime's exit (the atexit table, _fcloseall, __endstdio),
// with the heap's HeapFree calls and the files' FlushFileBuffers/CloseHandle, which the start does not record.
//
// Handles are numbered by their first appearance (h1, h2, ...), so two runs compare; pointers whose address differs
// between runs are left out, strings and rectangles are logged by content. The game's other threads (music, sound,
// DirectPlay) and the time functions are not recorded.
#include "reimpl.h"
#include "ui_window.h"

namespace zax::apitrace {

using namespace ui;

// Arguments: v value, h handle, s text, x left out (an address), r a rectangle (16 bytes) on the way in, o a
// rectangle or point written by the call (logged after it: R 16 bytes, P 8 bytes), m a MSG's id/wParam/lParam after
// the call, c the class name or atom (a text below 0x10000 is an atom), w WNDCLASS (style, procedure, instance, class)
struct Api {
    ImpKey iat;   // a key (batch 6-10a): our own IAT's slot in the standalone exe once it moved
    const char *name;
    const char *args;
    char ret;   // v value, h handle, b boolean (nonzero), x left out
};
const Api APIS[] = {
    // user32
    {0x5ea1d8, "SetClipboardData", "vh", 'h'}, {0x5ea1dc, "RegisterClipboardFormatA", "s", 'v'},
    {0x5ea1e0, "CloseClipboard", "", 'b'}, {0x5ea1e4, "GetWindowRect", "hR", 'b'}, {0x5ea1e8, "OpenClipboard", "h", 'b'},
    {0x5ea1ec, "GetClipboardData", "v", 'h'}, {0x5ea1f0, "RegisterWindowMessageA", "s", 'b'},
    {0x5ea1f4, "SetFocus", "h", 'h'}, {0x5ea1f8, "UpdateWindow", "h", 'b'}, {0x5ea1fc, "LoadStringA", "hvxv", 'v'},
    {0x5ea200, "RegisterClassA", "w", 'b'}, {0x5ea204, "LoadCursorA", "hv", 'h'}, {0x5ea208, "LoadIconA", "hv", 'h'},
    {0x5ea20c, "SetWindowLongA", "hvv", 'v'}, {0x5ea210, "GetWindowLongA", "hv", 'v'}, {0x5ea214, "EmptyClipboard", "", 'b'},
    {0x5ea218, "MoveWindow", "hvvvvv", 'b'}, {0x5ea21c, "DefWindowProcA", "hvvv", 'v'}, {0x5ea220, "BeginPaint", "hx", 'h'},
    {0x5ea224, "EndPaint", "hx", 'b'}, {0x5ea228, "PostQuitMessage", "v", 'x'}, {0x5ea22c, "IsIconic", "h", 'b'},
    {0x5ea230, "PeekMessageA", "mvvvv", 'b'}, {0x5ea234, "DispatchMessageA", "x", 'v'}, {0x5ea238, "GetClientRect", "hR", 'b'},
    {0x5ea23c, "OffsetRect", "rvv", 'b'}, {0x5ea240, "ClientToScreen", "hP", 'b'}, {0x5ea244, "SetCursorPos", "vv", 'b'},
    {0x5ea248, "GetKeyNameTextA", "vxv", 'v'}, {0x5ea24c, "SetCursor", "h", 'h'}, {0x5ea250, "GetDesktopWindow", "", 'h'},
    {0x5ea254, "SetForegroundWindow", "h", 'b'}, {0x5ea258, "MessageBoxA", "hssv", 'v'}, {0x5ea25c, "PostMessageA", "hvvv", 'b'},
    {0x5ea260, "GetDC", "h", 'h'}, {0x5ea264, "ReleaseDC", "hh", 'v'}, {0x5ea268, "SetRect", "Rvvvv", 'b'},
    {0x5ea26c, "GetMenu", "h", 'h'}, {0x5ea270, "AdjustWindowRectEx", "Rvvv", 'b'}, {0x5ea274, "SetWindowPos", "hhvvvvv", 'b'},
    {0x5ea278, "SystemParametersInfoA", "vvRv", 'b'}, {0x5ea27c, "CreateWindowExA", "vcsvvvvvhhhx", 'h'},
    {0x5ea280, "TranslateMessage", "x", 'b'},
    // gdi32
    {0x5ea020, "GetStockObject", "v", 'h'}, {0x5ea024, "DeleteObject", "h", 'b'},
    {0x5ea028, "CreateFontA", "vvvvvvvvvvvvvs", 'h'}, {0x5ea02c, "GetSystemPaletteEntries", "hvvx", 'v'},
    {0x5ea030, "GetDeviceCaps", "hv", 'v'},
    // ddraw, dsound, winmm (without timeGetTime), ole32, advapi32
    {0x5ea010, "DirectDrawCreate", "xxx", 'v'}, {0x5ea018, "DirectSoundCreate", "xxx", 'v'},
    {0x5ea288, "timeKillEvent", "h", 'v'}, {0x5ea28c, "joyGetDevCapsA", "vxv", 'v'}, {0x5ea290, "timeSetEvent", "vvxvv", 'h'},
    {0x5ea298, "joyGetNumDevs", "", 'v'}, {0x5ea29c, "joyGetPosEx", "vx", 'v'},
    {0x5ea33c, "CoUninitialize", "", 'x'}, {0x5ea340, "CoCreateInstance", "xxvxx", 'v'}, {0x5ea344, "CoInitialize", "x", 'v'},
    {0x5ea000, "RegCloseKey", "h", 'v'}, {0x5ea004, "RegOpenKeyExA", "hsvvx", 'v'}, {0x5ea008, "RegQueryValueExA", "hsxxxx", 'v'},
    // kernel32: files, folders, INI, modules, processes, global memory, the mutex, Sleep
    {0x5ea054, "SetFilePointer", "hvxv", 'v'}, {0x5ea064, "FreeLibrary", "h", 'b'}, {0x5ea068, "GetProcAddress", "hs", 'b'},
    {0x5ea06c, "LoadLibraryA", "s", 'h'}, {0x5ea070, "FindClose", "h", 'b'}, {0x5ea074, "FindNextFileA", "hx", 'b'},
    {0x5ea078, "FindFirstFileA", "sx", 'h'}, {0x5ea07c, "Sleep", "v", 'x'}, {0x5ea080, "GlobalUnlock", "h", 'v'},
    {0x5ea084, "GlobalLock", "h", 'b'}, {0x5ea088, "GlobalAlloc", "vv", 'h'}, {0x5ea090, "CreateMutexA", "xvs", 'h'},
    {0x5ea0b4, "GetTempFileNameA", "ssvx", 'v'}, {0x5ea0b8, "GetTempPathA", "vx", 'v'}, {0x5ea0bc, "GetFileAttributesA", "s", 'v'},
    {0x5ea0c0, "CreateDirectoryA", "sx", 'b'}, {0x5ea0c4, "GetPrivateProfileIntA", "ssvs", 'v'},
    {0x5ea0c8, "WritePrivateProfileStringA", "ssss", 'b'}, {0x5ea0cc, "GetPrivateProfileStringA", "sssxvs", 'v'},
    {0x5ea0d4, "CreateFileA", "svvxvvh", 'h'}, {0x5ea0d8, "CloseHandle", "h", 'b'}, {0x5ea0dc, "WriteFile", "hxvxx", 'b'},
    {0x5ea0e0, "GetModuleFileNameA", "hxv", 'v'}, {0x5ea0e4, "SetCurrentDirectoryA", "s", 'b'},
    {0x5ea0ec, "GlobalMemoryStatus", "x", 'x'}, {0x5ea0f0, "CreateProcessA", "ssxxvvxsxx", 'b'}, {0x5ea0f4, "MulDiv", "vvv", 'v'},
    {0x5ea124, "GlobalSize", "h", 'v'}, {0x5ea12c, "ReadFile", "hxvxx", 'b'}, {0x5ea144, "MoveFileA", "ss", 'b'},
    {0x5ea14c, "DeleteFileA", "s", 'b'},
    // kernel32 of the C runtime's start (phase 5b, crt_startup.cpp, crt_sehc.cpp): heap, TLS, low-level files,
    // command line, environment, code page, filters; not the ones called all the time (HeapAlloc/HeapFree,
    // TlsGetValue, the critical sections' Enter/Leave, Get/SetLastError, the interlocked ones)
    {0x5ea1a8, "HeapCreate", "vvv", 'h'}, {0x5ea1a4, "HeapDestroy", "h", 'b'}, {0x5ea1a0, "GetVersionExA", "x", 'b'},
    {0x5ea0d0, "GetVersion", "", 'v'}, {0x5ea128, "GetCommandLineA", "", 'x'}, {0x5ea118, "GetStartupInfoA", "x", 'x'},
    {0x5ea04c, "GetStdHandle", "v", 'h'}, {0x5ea048, "GetFileType", "h", 'v'}, {0x5ea050, "SetHandleCount", "v", 'v'},
    {0x5ea10c, "GetEnvironmentStringsW", "", 'x'}, {0x5ea100, "GetEnvironmentStrings", "", 'x'},
    {0x5ea104, "FreeEnvironmentStringsW", "x", 'b'}, {0x5ea03c, "FreeEnvironmentStringsA", "x", 'b'},
    {0x5ea120, "WideCharToMultiByte", "vvxvxvxx", 'v'}, {0x5ea0a8, "MultiByteToWideChar", "vvxvxv", 'v'},
    {0x5ea19c, "GetEnvironmentVariableA", "sxv", 'v'}, {0x5ea18c, "TlsAlloc", "", 'v'}, {0x5ea188, "TlsSetValue", "vx", 'b'},
    {0x5ea114, "InitializeCriticalSection", "x", 'x'}, {0x5ea11c, "DeleteCriticalSection", "x", 'x'},
    {0x5ea0a4, "GetCPInfo", "vx", 'b'}, {0x5ea09c, "GetACP", "", 'v'}, {0x5ea08c, "GetOEMCP", "", 'v'},
    {0x5ea110, "GetStringTypeA", "vvxvx", 'b'}, {0x5ea108, "GetStringTypeW", "vxvx", 'b'},
    {0x5ea17c, "LCMapStringA", "vvxvxv", 'v'}, {0x5ea180, "LCMapStringW", "vvxvxv", 'v'},
    {0x5ea0a0, "SetUnhandledExceptionFilter", "x", 'x'}, {0x5ea0b0, "GetSystemTimeAsFileTime", "x", 'x'},
    {0x5ea038, "IsBadCodePtr", "x", 'b'}, {0x5ea130, "GetModuleHandleA", "s", 'h'},
    // kernel32 of the exit (recorded only after PostQuitMessage, see g_exit)
    {0x5ea160, "ExitProcess", "v", 'x'}, {0x5ea178, "FlushFileBuffers", "h", 'b'}, {0x5ea16c, "HeapFree", "hvx", 'b'},
    {0x5ea158, "GetCurrentProcess", "", 'h'}, {0x5ea15c, "TerminateProcess", "hv", 'b'}, {0x5ea1ac, "VirtualFree", "xvv", 'b'},
};
constexpr int NAPI = sizeof APIS / sizeof APIS[0];

DWORD g_real[NAPI];
DWORD g_main_thread;
volatile bool g_active;
// The exit: armed when the start's trace ends, running from PostQuitMessage on
bool g_exit_armed, g_exit;
// A slot's exe address for comparisons (batch 6-10a): slot_address of a constant is consteval, so the DLL's code holds
// the literal as before; SLOT_ADDRESS of a table entry's key is a macro, because an inline function (key_address) gave
// the DLL's enter() other code (a jump table for exit_only's compares)
#ifdef ZAX_STANDALONE
#define SLOT_ADDRESS(k) ((k).address)
#else
#define SLOT_ADDRESS(k) (k)
#endif
consteval DWORD slot_address(ImpKey k)
{
#ifdef ZAX_STANDALONE
    return k.address;
#else
    return k;
#endif
}
inline bool exit_only(DWORD iat)
{
    return iat == slot_address(0x5ea160) || iat == slot_address(0x5ea178) || iat == slot_address(0x5ea16c) || iat == slot_address(0x5ea158) ||
           iat == slot_address(0x5ea15c) || iat == slot_address(0x5ea1ac);
}
int g_lines;
constexpr int MAX_LINES = 300000;

// Handles numbered by first appearance
constexpr int NHANDLE = 8192;
DWORD g_handles[NHANDLE];
int g_nhandles;
DWORD handle_no(DWORD h)
{
    if (!h || h == 0xffffffff)
        return h;
    for (int i = 0; i < g_nhandles; i++)
        if (g_handles[i] == h)
            return (DWORD)i + 1;
    if (g_nhandles < NHANDLE)
        g_handles[g_nhandles++] = h;
    return (DWORD)g_nhandles;
}

// Lines collected in a buffer and written to the log in pieces (one WriteFile per line would slow the start down)
char *g_buf;
DWORD g_used;
constexpr DWORD BUF = 0x400000;
void flush()
{
    DWORD start = 0;
    for (DWORD i = 0; i < g_used; i++)
        if (g_buf[i] == '\n') {
            g_buf[i] = 0;
            log("%s", g_buf + start);
            start = i + 1;
        }
    g_used = 0;
}
void out(const char *s)
{
    if (!g_buf)
        return;
    const int n = lstrlenA(s);
    if (g_used + (DWORD)n + 2 > BUF)
        flush();
    memcpy(g_buf + g_used, s, (DWORD)n);
    g_used += (DWORD)n;
    g_buf[g_used++] = '\n';
    if (++g_lines >= MAX_LINES)
        g_active = false;
}

// Whether n bytes at p can be read (an argument the spec calls a text or a structure can be anything)
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
bool readable(DWORD p, DWORD n)
{
    if (p < 0x10000 || p >= 0x80000000u || p + n < p)
        return false;
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    for (DWORD a = p & ~0xfffu; a < p + n; a += 0x1000)
        if (!VirtualQuery(at<void *>(a), &mi, sizeof mi) || mi.state != 0x1000 || (mi.protect & 0x101) || !(mi.protect & 0xee))
            return false;
    return true;
}
bool readable_text(DWORD p) { return readable(p, 1); }
// A text argument, at most n characters, only from readable memory
char g_text[256];
const char *safe_text(DWORD p, DWORD n)
{
    DWORD k = 0;
    while (k < n && (k && ((p + k) & 0xfff) ? true : readable(p + k, 1)) && at<const char *>(p)[k]) {
        g_text[k] = at<const char *>(p)[k];
        k++;
    }
    g_text[k] = 0;
    return g_text;
}

// Appends one argument to the line
int arg_text(char *o, char kind, DWORD v)
{
    switch (kind) {
    case 'h': return wsprintfA(o, "h%u", handle_no(v));
    case 'x': return wsprintfA(o, "-");
    case 's':
        if (!readable_text(v))
            return wsprintfA(o, "%x", v);
        return wsprintfA(o, "\"%s\"", safe_text(v, 160));
    case 'c':
        if (!readable_text(v))
            return wsprintfA(o, "atom %x", v);
        return wsprintfA(o, "\"%s\"", safe_text(v, 80));
    case 'r': {
        if (!readable(v, 16))
            return wsprintfA(o, "%x", v);
        const int *r = at<const int *>(v);
        return wsprintfA(o, "(%d,%d,%d,%d)", r[0], r[1], r[2], r[3]);
    }
    case 'w': {
        if (!readable(v, 40))
            return wsprintfA(o, "%x", v);
        const DWORD *w = at<const DWORD *>(v);
        return wsprintfA(o, "{style %x proc %x extra %u %u inst h%u icon h%u cursor h%u brush h%u menu %x class \"%.40s\"}", w[0], w[1],
                         w[2], w[3], handle_no(w[4]), handle_no(w[5]), handle_no(w[6]), handle_no(w[7]), w[8],
                         readable_text(w[9]) ? safe_text(w[9], 40) : "");
    }
    case 'R': case 'P': case 'm': return wsprintfA(o, "out");
    default: return wsprintfA(o, "%x", v);
    }
}

struct Frame {
    DWORD ret;
    int api;
    DWORD args[14];
};
constexpr int DEPTH = 64;
Frame g_stack[DEPTH];
int g_depth;

void snapshot();

}  // namespace zax::apitrace

extern "C" {
DWORD __cdecl trace_enter(DWORD *regs);
void __cdecl trace_leave(DWORD *regs);
extern char trace_return[];
extern char trace_common[];
}
// regs: the pushal block (edi, esi, ebp, esp, ebx, edx, ecx, eax), then the API's index, the caller's return address and
// the arguments
__asm__(".text\n"
        ".globl _trace_common\n"
        "_trace_common:\n"
        "  pushal\n"
        "  pushl %esp\n"
        "  call _trace_enter\n"
        "  addl $4, %esp\n"
        "  movl %eax, 28(%esp)\n"
        "  popal\n"
        "  addl $4, %esp\n"
        "  jmp *%eax\n"
        ".globl _trace_return\n"
        "_trace_return:\n"
        "  pushl $0\n"
        "  pushal\n"
        "  pushl %esp\n"
        "  call _trace_leave\n"
        "  addl $4, %esp\n"
        "  popal\n"
        "  ret\n");

namespace zax::apitrace {

DWORD enter(DWORD *regs)
{
    const int i = (int)regs[8];
    if (!g_active && g_exit_armed && SLOT_ADDRESS(APIS[i].iat) == slot_address(0x5ea228) && GetCurrentThreadId() == g_main_thread) {
        g_exit_armed = false;
        g_exit = true;
        g_active = true;
        out("API trace exit");
    }
    if (!g_active || GetCurrentThreadId() != g_main_thread || g_depth >= DEPTH || (exit_only(SLOT_ADDRESS(APIS[i].iat)) && !g_exit))
        return g_real[i];
    if (SLOT_ADDRESS(APIS[i].iat) == slot_address(0x5ea160)) {   // ExitProcess does not come back: its line now
        char line[64];
        wsprintfA(line, "API ExitProcess(%x)", regs[10]);
        out(line);
        out("API trace exit end");
        flush();
        g_active = false;
        return g_real[i];
    }
    const char *a = APIS[i].args;
    const int n = lstrlenA(a);
    Frame &f = g_stack[g_depth++];
    f.ret = regs[9];
    f.api = i;
    for (int k = 0; k < n && k < 14; k++)
        f.args[k] = regs[10 + k];
    regs[9] = (DWORD)(uintptr_t)trace_return;
    return g_real[i];
}

void leave(DWORD *regs)
{
    Frame &f = g_stack[--g_depth];
    regs[8] = f.ret;
    const Api &api = APIS[f.api];
    const DWORD r = regs[7];
    if (!g_active)
        return;
    char line[1024];
    int o = wsprintfA(line, "API %s(", api.name);
    const int n = lstrlenA(api.args);
    for (int k = 0; k < n && k < 14 && o < 700; k++) {
        if (k)
            line[o++] = ',';
        o += arg_text(line + o, api.args[k], f.args[k]);
    }
    o += wsprintfA(line + o, ")");
    switch (api.ret) {
    case 'h': o += wsprintfA(line + o, " = h%u", handle_no(r)); break;
    case 'b': o += wsprintfA(line + o, " = %u", r ? 1 : 0); break;
    case 'x': break;
    default: o += wsprintfA(line + o, " = %x", r); break;
    }
    for (int k = 0; k < n && k < 14; k++) {
        const char kind = api.args[k];
        const DWORD v = f.args[k];
        if (!readable(v, kind == 'm' ? 0x1c : kind == 'P' ? 8 : 16))
            continue;
        if (kind == 'R') {
            const int *q = at<const int *>(v);
            o += wsprintfA(line + o, " [%d,%d,%d,%d]", q[0], q[1], q[2], q[3]);
        } else if (kind == 'P') {
            const int *q = at<const int *>(v);
            o += wsprintfA(line + o, " [%d,%d]", q[0], q[1]);
        } else if (kind == 'm' && r) {
            const DWORD *q = at<const DWORD *>(v);
            o += wsprintfA(line + o, " [msg %x %x %x]", q[1], q[2], q[3]);
        }
    }
    out(line);
    // WinMain's loop starts: the start is over
    if (SLOT_ADDRESS(api.iat) == slot_address(0x5ea230) && !g_exit) {
        g_active = false;
        snapshot();
        out("API trace end");
        flush();
        g_exit_armed = g_lines < MAX_LINES;
    }
}

template <typename... A> inline DWORD com(DWORD obj, int slot, A... a)
{
    using F = DWORD(__stdcall *)(DWORD, A...);
    return (*reinterpret_cast<F *const *>(obj))[slot](obj, a...);
}

void state_line(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(line, fmt, ap);
    va_end(ap);
    if (g_buf) {
        const int n = lstrlenA(line);
        if (g_used + (DWORD)n + 2 > BUF)
            flush();
        memcpy(g_buf + g_used, line, (DWORD)n);
        g_used += (DWORD)n;
        g_buf[g_used++] = '\n';
    }
}

void surface_line(const char *what, DWORD s)
{
    if (!s) {
        state_line("API state %s: none", what);
        return;
    }
    DWORD d[0x1b];
    memset(d, 0, sizeof d);
    d[0] = 0x6c;
    const DWORD r = com(s, 22, d);
    state_line("API state %s: desc %x flags %x size %u x %u pitch %d back buffers %u caps %x format flags %x bits %u masks %x %x %x",
               what, r, d[1], d[3], d[2], (int)d[4], d[5], d[0x1a], d[0x13], d[0x15], d[0x16], d[0x17], d[0x18]);
}

void snapshot()
{
    const DWORD rep = gd(glob_addr(0x6c02cc));
    state_line("API state window h%u instance h%u, active %u minimized %u, full screen %u, joystick message %s, wheel message %s",
               handle_no(gd(glob_addr(0x713e18))), handle_no(gd(glob_addr(0x713e00))), gb(glob_addr(0x713e10)), gb(glob_addr(0x713e11)), gb(glob_addr(0x6210c0)),
               gd(glob_addr(0x713e0c)) ? "set" : "0", gd(glob_addr(0x713e14)) ? "set" : "0");
    state_line("API state font h%u, %u %u %u %u %u", handle_no(gd(glob_addr(0x6c0480))), gd(glob_addr(0x6c0468)), gd(glob_addr(0x6c046c)), gd(glob_addr(0x6c0470)), gd(glob_addr(0x6c0474)),
               gd(glob_addr(0x6c0478)));
    state_line("API state app %s, g_App %s, 0x713f18 %s", rep ? "set" : "0", gd(glob_addr(0x713f14)) == rep ? "= reporter" : "other",
               gd(glob_addr(0x713f18)) ? "set" : "0");
    if (rep && m(bp(rep), 4)) {
        BYTE *s = bp(m(bp(rep), 4));
        state_line("API state screen: size %u x %u, pages %u, back buffers %u, page %u, video memory %u, 565 %u, brightness %x, modes %u",
                   m<WORD>(s, 0x12), m<WORD>(s, 0x14), m(s, 0x40), m(s, 0x44), m(s, 0x48), mb(s, 0x50), mb(s, 0xcc), m(s, 0xd0), m(s, 0x4c));
        for (DWORD i = 0; i < m(s, 0x4c) && i < 10; i++)
            state_line("API state mode %u: %u x %u x %u", i, m(s, 0x54 + i * 0xc), m(s, 0x58 + i * 0xc), m(s, 0x5c + i * 0xc));
        state_line("API state frame: depth %u, width %u, height %u", vc(s, 29), vc(s, 45), vc(s, 39));
    }
    state_line("API state ddraw %s, primary %s, back %s, third %s, clipper %s, palette %s, own back %u, locked %u", gd(glob_addr(0x713334)) ? "set" : "0",
               gd(glob_addr(0x713348)) ? "set" : "0", gd(glob_addr(0x713338)) ? "set" : "0", gd(glob_addr(0x71333c)) ? "set" : "0", gd(glob_addr(0x713340)) ? "set" : "0",
               gd(glob_addr(0x713344)) ? "set" : "0", gb(glob_addr(0x713284)), gd(glob_addr(0x713318)));
    if (const DWORD dd = gd(glob_addr(0x713334))) {
        DWORD d[0x1b];
        memset(d, 0, sizeof d);
        d[0] = 0x6c;
        const DWORD r = com(dd, 12, d);
        state_line("API state display mode: %x, %u x %u, bits %u", r, d[3], d[2], d[0x15]);
    }
    surface_line("primary", gd(glob_addr(0x713348)));
    surface_line("back", gd(glob_addr(0x713338)));
}

// A crash while tracing: the address and the registers into the log, the lines so far written (first exception only;
// the game's own handlers still run)
WINIMPORT void *WINAPI AddVectoredExceptionHandler(unsigned long first, long(WINAPI *handler)(void *info));
bool g_crashed;
long WINAPI on_exception(void *info)
{
    const DWORD *const *p = static_cast<const DWORD *const *>(info);
    const DWORD *rec = p[0], *ctx = p[1];
    if (g_crashed || rec[0] < 0xc0000000u || rec[0] == 0xc0000027 /* unwind */)
        return 0;
    g_crashed = true;
    flush();
    log("API trace: exception %x at %x (access %x) eax %x ecx %x edx %x ebx %x esi %x edi %x esp %x ebp %x thread %u", rec[0],
        rec[3], rec[4] >= 2 ? rec[6] : 0, ctx[0xb0 / 4], ctx[0xac / 4], ctx[0xa8 / 4], ctx[0xa4 / 4], ctx[0xa0 / 4], ctx[0x9c / 4],
        ctx[0xc4 / 4], ctx[0xb4 / 4], GetCurrentThreadId());
    const DWORD *sp = at<const DWORD *>(ctx[0xc4 / 4]);
    for (int i = 0; i < 24; i += 8)
        log("  stack %x %x %x %x %x %x %x %x", sp[i], sp[i + 1], sp[i + 2], sp[i + 3], sp[i + 4], sp[i + 5], sp[i + 6], sp[i + 7]);
    return 0;
}

// The main thread: the only other thread of the process while the loader's thread runs DllMain; in the standalone
// exe (docs/DECOMPILATION.md 13, batch 6-1) the thread of its entry, which goes on to WinMainCRTStartup
DWORD find_main_thread()
{
#ifdef ZAX_STANDALONE
    return GetCurrentThreadId();
#endif
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    THREADENTRY32 e;
    e.dwSize = sizeof e;
    DWORD found = 0;
    const DWORD self = GetCurrentThreadId(), process = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snap, &e); ok; ok = Thread32Next(snap, &e))
        if (e.th32OwnerProcessID == process && e.th32ThreadID != self && !found)
            found = e.th32ThreadID;
    CloseHandle(snap);
    return found;
}

}  // namespace zax::apitrace

extern "C" DWORD __cdecl trace_enter(DWORD *regs) { return zax::apitrace::enter(regs); }
extern "C" void __cdecl trace_leave(DWORD *regs) { zax::apitrace::leave(regs); }

namespace zax {

// Called by DllMain before the hooks (and with /Original): the import table's entries of APIS to recorders
void api_trace_install(const char *ini)
{
    using namespace apitrace;
    if (!GetPrivateProfileIntA("Reimpl", "TraceApi", 0, ini))
        return;
    g_main_thread = find_main_thread();
    g_buf = static_cast<char *>(VirtualAlloc(nullptr, BUF, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    BYTE *thunks = static_cast<BYTE *>(VirtualAlloc(nullptr, NAPI * 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_main_thread || !g_buf || !thunks) {
        log("API trace: not installed (main thread %u)", g_main_thread);
        return;
    }
    for (int i = 0; i < NAPI; i++) {
        BYTE *t = thunks + i * 16;
        t[0] = 0x68;   // push i
        memcpy(t + 1, &i, 4);
        t[5] = 0xe9;   // jmp trace_common
        const INT32 rel = (INT32)((DWORD)(uintptr_t)trace_common - ((DWORD)(uintptr_t)t + 10));
        memcpy(t + 6, &rel, 4);
        DWORD *slot = imp_slot<DWORD *>(APIS[i].iat);
        g_real[i] = *slot;
        DWORD old;
        VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old);
        *slot = (DWORD)(uintptr_t)t;
        VirtualProtect(slot, 4, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), thunks, NAPI * 16);
    AddVectoredExceptionHandler(1, &on_exception);
    g_active = true;
    log("API trace: %d imports recorded in thread %u until the first PeekMessageA", NAPI, g_main_thread);
}

// DllMain at the process's end: what is left of the trace
void api_trace_flush()
{
    if (apitrace::g_buf && apitrace::g_used) {
        apitrace::flush();
        log("API trace: flushed at the process's end");
    }
}

}  // namespace zax
