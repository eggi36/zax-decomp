// Zax-Reimpl.dll: entry point, hook installation, self-tests.
//
// Sequence (DllMain, Zax.exe has not executed its first instruction yet):
//  1. Self-test: compare original and reimplementation on many inputs (pure functions only).
//  2. Install hooks: jump from the original to the reimplementation, unless the function is disabled
//     (Zax-Reimpl.ini, [Reimpl] Disable=0x55fdd0,...) or /Original is on the command line.
//  3. Checks in the running game: after [Reimpl] CheckAfterMs= milliseconds (default 20000, 0 = off)
//     write the class table to the log and test functions against the original that only make
//     sense once all classes are registered or that allocate (checks.cpp, in the game thread);
//     [Reimpl] OnlyChecks=module,... limits them to some modules ("base": the older checks),
//     [Reimpl] CheckScale=0.1 runs a tenth of the random cases (check_util.h scaled()).
//  [Reimpl] TraceApi=1: the main thread's Windows calls of the start logged (api_trace.cpp), with /Original too.
//  [Reimpl] Record=1 / Replay=1 / FrameLog=1: input recording, replay and the per-frame hashes (replay.cpp), the same.
//  [Reimpl] Coverage=1: the reimplemented functions the hooked run enters, logged as "Coverage" lines (coverage.cpp).
//  [Reimpl] ForceNoMMX=1 / Force555=1: a CPU without MMX, a 5-5-5 16-bit display (sim_hw.cpp), with /Original too.
//  [Reimpl] CrashNames=1: the first hardware exceptions with the instruction and the stack's callers (below).
//  [Reimpl] SampleAfterMs=N: samples of the main thread's registers and stack, for a hung game (below).
//  [Reimpl] TrueColorData16=1: at 24/32 bits the 16-bit data caches (reimpl.h; hooked runs only, not the original).
// Log: Zax-Reimpl.log next to Zax.exe.
#include "reimpl.h"
#include "angle.h"
#include "checks.h"
#include "class_info.h"
#include "class_registry.h"
#include "dir_list.h"
#include "named_list.h"
#include "ptr_array.h"
#include "rotation_sequence.h"
#include "zstring.h"

namespace zax {

static HANDLE g_log = INVALID_HANDLE_VALUE;

void api_trace_install(const char *ini);   // api_trace.cpp: [Reimpl] TraceApi=1
void api_trace_flush();
void replay_install(const char *ini);   // replay.cpp: [Reimpl] Record=1 / Replay=1 / FrameLog=1
void replay_flush();
void sim_hw_install(const char *ini);   // sim_hw.cpp: [Reimpl] ForceNoMMX=1 / Force555=1
bool coverage_init(const Hook *hooks, int count);   // coverage.cpp: [Reimpl] Coverage=1
void *coverage_stub(int i, void *replacement);
void coverage_flush();
void coverage_start(HINSTANCE inst);
static bool g_coverage;

void log(const char *fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE)
        return;
    char buf[1100];
    va_list ap;
    va_start(ap, fmt);
    int n = wvsprintfA(buf, fmt, ap);
    va_end(ap);
    buf[n++] = '\r', buf[n++] = '\n';
    DWORD written;
    WriteFile(g_log, buf, (DWORD)n, &written, NULL);
}

// ---- Self-tests ----------------------------------------------------------------------

static unsigned g_seed = 12345;
static unsigned random() { return g_seed = g_seed * 1664525u + 1013904223u; }
static float random_float(float lo, float hi) { return lo + (hi - lo) * (float)(random() >> 8) / 16777216.0f; }
static float bits(unsigned u) { float f; memcpy(&f, &u, 4); return f; }

static float random_angle()
{
    switch (random() % 8) {
    case 0: return bits(random());                       // any bit pattern, including NaN/infinity
    case 1: return (float)(int)(random() % 17 - 8) * 0.78539819f;  // multiples of pi/4
    case 2: return random_float(-100.0f, 100.0f);
    default: return random_float(-7.0f, 14.0f);
    }
}

static bool test_clamp()
{
    auto orig = original<long double(STDCALL *)(float, float, float)>(0x5096a0);
    int mismatches = 0;
    for (int i = 0; i < 200000; i++) {
        float lo = random() % 2 ? random_float(-7.0f, 7.0f) : -3.14159274f;
        float hi = random() % 2 ? lo + random_float(0.0f, 7.0f) : lo + c_TwoPi;
        float a = random_angle();
        long double o = orig(a, lo, hi), n = ClampAngleToRange(a, lo, hi);
        if (memcmp(&o, &n, 10) != 0 && mismatches++ < 10)
            log("  ClampAngleToRange(%08x, %08x, %08x) differs", *(unsigned *)&a, *(unsigned *)&lo, *(unsigned *)&hi);
    }
    log("Self-test ClampAngleToRange: %s (%d of 200000 differ)", mismatches ? "MISMATCH" : "same", mismatches);
    return !mismatches;
}

static bool test_angle_to_rotation()
{
    auto orig = original<int(THISCALL *)(CRotationSequence *, float)>(0x55fdd0);
    static const int N[] = {1, 2, 4, 8, 12, 16, 24, 32, 36, 64};
    CRotationSequence s;
    memset(&s, 0, sizeof s);
    int mismatches = 0;
    for (int i = 0; i < 200000; i++) {
        s.numRotations = N[random() % 10];
        switch (random() % 4) {
        case 0: s.rotationalOffset = 0.0f, s.rotationalRange = c_TwoPi; break;
        case 1: s.rotationalOffset = -3.14159274f, s.rotationalRange = c_TwoPi; break;
        case 2: s.rotationalOffset = random_float(-4.0f, 4.0f), s.rotationalRange = random_float(0.1f, 6.3f); break;
        default: s.rotationalOffset = random_angle(), s.rotationalRange = random_angle(); break;
        }
        float a = random_angle();
        int o = orig(&s, a), n = s.AngleToRotation(a);
        if (o != n && mismatches++ < 10)
            log("  AngleToRotation(%08x; off %08x, range %08x, n %d): original %d, reimpl %d", *(unsigned *)&a,
                *(unsigned *)&s.rotationalOffset, *(unsigned *)&s.rotationalRange, s.numRotations, o, n);
    }
    log("Self-test CRotationSequence::AngleToRotation: %s (%d of 200000 differ)", mismatches ? "MISMATCH" : "same",
        mismatches);
    return !mismatches;
}

// Call through an exe vtable with our own class declaration (MSVC ABI): slot, order and stack
// cleanup (__thiscall) must match, otherwise this crashes. CPickupAI (vtable 0x5fcb88): slot 0 is an
// empty method with 2 arguments (ret 8), slot 1 the destructor, slot 2 returns the class info,
// slot 3 is empty with 1 argument (ret 4).
struct CPickupAI_Probe {
    virtual void vf0(int, int);
    virtual ~CPickupAI_Probe();
    virtual void *GetClassInfo();
    virtual void vf3(int);
};

static void check_vtable_call()
{
    struct {
        DWORD vftable;
        BYTE rest[44];
    } obj = {0x5fcb88, {}};
    auto *p = reinterpret_cast<CPickupAI_Probe *>(&obj);
    p->vf0(1, 2);
    p->vf3(3);
    log("vtable call through own class (MSVC ABI): slots 0 and 3 of CPickupAI ok");
}

// ---- Hooks ---------------------------------------------------------------------------

// All reimplemented functions: the hook tables of the modules (ZAX_HOOKS in each module's .cpp,
// collected by build.sh into hook_tables.inc; new modules need no change here, add them to
// docs/reimpl.csv).
#define ZAX_HOOK_MODULE(name) int hooks_##name(Hook *out);
#include "hook_tables.inc"
#undef ZAX_HOOK_MODULE

static int hook_list(Hook *out)
{
    int n = 0;
#define ZAX_HOOK_MODULE(name) n += hooks_##name(out ? out + n : nullptr);
#include "hook_tables.inc"
#undef ZAX_HOOK_MODULE
    return n;
}

// Case-insensitive; true if a starts with the first n characters of b
static bool starts_with(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        char x = a[i] >= 'A' && a[i] <= 'Z' ? a[i] + 32 : a[i], y = b[i] >= 'A' && b[i] <= 'Z' ? b[i] + 32 : b[i];
        if (x != y || !x)
            return false;
    }
    return true;
}

static bool is_disabled(const char *list, DWORD address)
{
    char hex[16];
    wsprintfA(hex, "0x%x", address);
    int n = lstrlenA(hex);
    for (const char *p = list; *p; p++)
        if (starts_with(p, hex, n) && (p == list || p[-1] == ',' || p[-1] == ' ') &&
            (!p[n] || p[n] == ',' || p[n] == ' '))
            return true;
    return false;
}

// Trampolines: the overwritten instructions of the original, then a jump back behind the hook
struct Trampoline {
    DWORD address;
    BYTE *code;
};
static const int MAX_HOOKS = 16384;   // 8192 was reached on 2026-10-28 (8208 hooks after the last phase 4 round)
static const int TRAMPOLINE_SIZE = 32;   // copied instructions (at most 27 bytes) + jump back
static Trampoline g_trampolines[MAX_HOOKS];
static int g_num_trampolines;
static BYTE *g_trampoline_memory;

void *original_address(DWORD address)
{
    for (int i = 0; i < g_num_trampolines; i++)
        if (g_trampolines[i].address == address)
            return g_trampolines[i].code;
    return at<void *>(address);
}

static void build_trampoline(const Hook &h)
{
    if (!h.length || g_num_trampolines >= MAX_HOOKS)
        return;
    if (!g_trampoline_memory)
        g_trampoline_memory = (BYTE *)VirtualAlloc(nullptr, MAX_HOOKS * TRAMPOLINE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampoline_memory)
        return;
    BYTE *t = g_trampoline_memory + g_num_trampolines * TRAMPOLINE_SIZE;
    memcpy(t, at<void *>(h.address), h.length);
    if (h.call_at) {   // relative call: same target from the trampoline
        BYTE *ins = t + h.call_at - 1;
        DWORD target = h.address + h.call_at - 1 + 5 + *(INT32 *)(ins + 1);
        *(INT32 *)(ins + 1) = (INT32)(target - (DWORD)(uintptr_t)(ins + 5));
    }
    t[h.length] = 0xe9;
    *(INT32 *)(t + h.length + 1) = (INT32)((h.address + h.length) - (DWORD)(uintptr_t)(t + h.length + 5));
    g_trampolines[g_num_trampolines++] = {h.address, t};
}

static bool install(const Hook &h, int index)
{
    BYTE *site = at<BYTE *>(h.address);
    if (memcmp(site, h.expected, sizeof h.expected) != 0) {
        log("NOT installed %s (0x%x): original bytes do not match (different Zax.exe?)", h.name, h.address);
        return false;
    }
    build_trampoline(h);
    DWORD old;
    VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xe9;
    BYTE *const target = (BYTE *)(g_coverage ? coverage_stub(index, h.replacement) : h.replacement);
    *(INT32 *)(site + 1) = (INT32)(target - (site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    log("installed %s (0x%x)", h.name, h.address);
    return true;
}

// The hook whose replacement starts last at or before an address of this DLL (replay.cpp's heap trace names its
// callers with it; a static helper between two replacements is named after the one before it)
static Hook *g_hooks;
static int g_hook_count;
const char *hook_near(DWORD address, DWORD *start)
{
    const Hook *best = nullptr;
    for (int i = 0; i < g_hook_count; i++) {
        const DWORD r = (DWORD)(uintptr_t)g_hooks[i].replacement;
        if (r <= address && (!best || r > (DWORD)(uintptr_t)best->replacement))
            best = &g_hooks[i];
    }
    if (start)
        *start = best ? (DWORD)(uintptr_t)best->replacement : 0;
    return best ? best->name : "?";
}

// [Reimpl] CrashNames=1 in the DLL (batch 6-12; the standalone exe's is in standalone/loader.cpp): the first four
// hardware exceptions of the process (access violation, stack overflow, …; not C++ exceptions) with the instruction,
// the DLL's base and the return-address-like dwords of the stack. An address of the DLL is named by the hook before it
// and given as an offset from the DLL's base too (an unstripped build of the same sources names it exactly). Logged
// before any handler runs, so an exception the game's own filter turns into a silent exit shows here.
WINIMPORT void *WINAPI AddVectoredExceptionHandler(unsigned long first, long(WINAPI *handler)(void *info));
WINIMPORT BOOL WINAPI IsBadReadPtr(const void *p, size_t n);
static DWORD g_dll_start, g_dll_end;
static int g_crashes;
static void crash_where(DWORD a, char *out)
{
    if (a >= g_dll_start && a < g_dll_end) {
        DWORD f = 0;
        const char *name = hook_near(a, &f);
        wsprintfA(out, "dll+0x%x (%s+0x%x)", a - g_dll_start, name, a - f);
    } else
        wsprintfA(out, "0x%x", a);
}
static long WINAPI on_crash(void *info)
{
    DWORD *const *p = static_cast<DWORD *const *>(info);
    const DWORD *rec = p[0], *ctx = p[1];
    if (rec[0] < 0xc0000000u || rec[0] == 0xc0000027 /* unwind */ || ++g_crashes > 4)
        return 0;
    char line[1100], w[160];
    crash_where(ctx[0xb8 / 4], w);
    int o = wsprintfA(line, "Crash: exception %x, access %x, at %s, thread %u, esp %x; stack", rec[0], rec[4] >= 2 ? rec[6] : 0,
                      w, GetCurrentThreadId(), ctx[0xc4 / 4]);
    const DWORD *sp = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(ctx[0xc4 / 4]));
    for (int i = 0, found = 0; i < 1024 && found < 14 && o < 900; i++) {
        if (IsBadReadPtr(sp + i, 4))
            break;
        const DWORD v = sp[i];
        if ((v >= 0x401000 && v < 0x5ea000) || (v >= g_dll_start && v < g_dll_end)) {
            crash_where(v, w);
            o += wsprintfA(line + o, " | %s", w);
            found++;
        }
    }
    log("%s", line);
    return 0;
}

// [Reimpl] SampleAfterMs=N (0 = off, the default): a diagnostic for a game that hangs (batch hang-32bit). N ms after
// the start a thread of its own samples the exe's main thread SampleCount times (default 20), every SampleEveryMs ms
// (default 250): it suspends the thread, reads its registers (GetThreadContext), logs the instruction pointer with
// its module, the registers and the return-address-like dwords of its stack (exe and DLL addresses, as CrashNames),
// and resumes it. It changes nothing in the game (only the moments of the suspensions), with /Original too.
WINIMPORT HANDLE WINAPI OpenThread(DWORD access, BOOL inherit, DWORD id);
WINIMPORT DWORD WINAPI SuspendThread(HANDLE thread);
WINIMPORT DWORD WINAPI ResumeThread(HANDLE thread);
WINIMPORT BOOL WINAPI GetThreadContext(HANDLE thread, void *context);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
namespace apitrace {
DWORD find_main_thread();
}
static DWORD g_sample_thread, g_sample_after, g_sample_count, g_sample_every;
static void module_of(DWORD a, char *out)
{
    DWORD info[7] = {};
    char path[MAX_PATH + 8];
    path[0] = 0;
    if (VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), info, sizeof info) && info[1])
        GetModuleFileNameA(reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(info[1])), path, MAX_PATH);
    const char *name = path;
    for (const char *p = path; *p; p++)
        if (*p == '\\')
            name = p + 1;
    wsprintfA(out, "%s+0x%x", name[0] ? name : "?", a - info[1]);
}
static DWORD WINAPI sample_thread(void *)
{
    Sleep(g_sample_after);
    HANDLE t = OpenThread(0x0002 | 0x0008 | 0x0040 /* suspend/resume, get context, query */, FALSE, g_sample_thread);
    if (!t) {
        log("Sample: the main thread %u cannot be opened", g_sample_thread);
        return 0;
    }
    alignas(16) static DWORD ctx[0x2cc / 4 + 4];
    for (DWORD n = 0; n < g_sample_count; n++) {
        if (SuspendThread(t) == 0xffffffffu) {
            log("Sample %u: SuspendThread failed (the thread ended?)", n);
            break;
        }
        memset(ctx, 0, sizeof ctx);
        ctx[0] = 0x10007;   // CONTEXT_i386 | CONTROL | INTEGER | SEGMENTS
        char line[1100], w[MAX_PATH + 24];
        if (!GetThreadContext(t, ctx)) {
            ResumeThread(t);
            log("Sample %u: GetThreadContext failed", n);
            Sleep(g_sample_every);
            continue;
        }
        const DWORD eip = ctx[0xb8 / 4], esp = ctx[0xc4 / 4];
        if (eip >= 0x401000 && eip < 0x5ea000)
            wsprintfA(w, "0x%x", eip);
        else if (eip >= g_dll_start && eip < g_dll_end)
            crash_where(eip, w);
        else
            module_of(eip, w);
        int o = wsprintfA(line, "Sample %u: eip %s eax %x ecx %x edx %x ebx %x esi %x edi %x esp %x ebp %x; stack", n, w,
                          ctx[0xb0 / 4], ctx[0xac / 4], ctx[0xa8 / 4], ctx[0xa4 / 4], ctx[0xa0 / 4], ctx[0x9c / 4], esp,
                          ctx[0xb4 / 4]);
        const DWORD *sp = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(esp));
        for (int i = 0, found = 0; i < 4096 && found < 24 && o < 950; i++) {
            if (IsBadReadPtr(sp + i, 4))
                break;
            const DWORD v = sp[i];
            if ((v >= 0x401000 && v < 0x5ea000) || (g_dll_start && v >= g_dll_start && v < g_dll_end)) {
                crash_where(v, w);
                o += wsprintfA(line + o, " | %s", w);
                found++;
            }
        }
        ResumeThread(t);
        log("%s", line);
        Sleep(g_sample_every);
    }
    CloseHandle(t);
    return 0;
}

static void start(HINSTANCE inst)
{
    char path[MAX_PATH + 32], ini[MAX_PATH + 32];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    for (int n = lstrlenA(path); n > 0 && path[n - 1] != '\\'; n--)
        path[n - 1] = 0;
    lstrcpyA(ini, path);
    lstrcatA(path, "Zax-Reimpl.log");
    lstrcatA(ini, "Zax-Reimpl.ini");
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    (void)inst;

    int count = hook_list(nullptr);
    if (count > MAX_HOOKS) {
        log("Too many hooks: %d (at most %d)", count, MAX_HOOKS);
        return;
    }
    // Not on the stack: without stack probes a big frame could skip the guard page
    Hook *hooks = (Hook *)VirtualAlloc(nullptr, count * sizeof(Hook), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!hooks) {
        log("No memory for the hook table");
        return;
    }
    hook_list(hooks);
    g_hooks = hooks;
    g_hook_count = count;
    log("Zax-Reimpl: %d functions", count);
    if (GetPrivateProfileIntA("Reimpl", "CrashNames", 0, ini)) {
        const BYTE *base = reinterpret_cast<const BYTE *>(inst);
        g_dll_start = (DWORD)(uintptr_t)base;
        g_dll_end = g_dll_start + *reinterpret_cast<const DWORD *>(base + *reinterpret_cast<const DWORD *>(base + 0x3c) + 0x50);
        AddVectoredExceptionHandler(1, &on_crash);
        log("CrashNames: the DLL at 0x%x-0x%x", g_dll_start, g_dll_end);
    }
    check_vtable_call();
    WORD cw;
    __asm__("fnstcw %0" : "=m"(cw));
    log("FPU control word at start: %04x", cw);

    bool ok = test_clamp();
    ok = test_angle_to_rotation() && ok;

    DWORD check_after_ms = GetPrivateProfileIntA("Reimpl", "CheckAfterMs", 20000, ini);
    char only[512];
    GetPrivateProfileStringA("Reimpl", "OnlyChecks", "", only, sizeof only, ini);
    char scale[32];
    GetPrivateProfileStringA("Reimpl", "CheckScale", "", scale, sizeof scale, ini);
    set_check_scale(scale);

    if ((g_sample_after = GetPrivateProfileIntA("Reimpl", "SampleAfterMs", 0, ini))) {
        g_sample_count = GetPrivateProfileIntA("Reimpl", "SampleCount", 20, ini);
        g_sample_every = GetPrivateProfileIntA("Reimpl", "SampleEveryMs", 250, ini);
        g_sample_thread = apitrace::find_main_thread();
        if (!g_dll_start) {   // the DLL's range for the stack scan (as CrashNames)
            const BYTE *base = reinterpret_cast<const BYTE *>(inst);
            g_dll_start = (DWORD)(uintptr_t)base;
            g_dll_end = g_dll_start + *reinterpret_cast<const DWORD *>(base + *reinterpret_cast<const DWORD *>(base + 0x3c) + 0x50);
        }
        log("Sample: the main thread %u, %u samples every %u ms after %u ms (DLL at 0x%x)", g_sample_thread, g_sample_count,
            g_sample_every, g_sample_after, g_dll_start);
        if (g_sample_thread)
            CloseHandle(CreateThread(nullptr, 0x10000, &sample_thread, nullptr, 0, nullptr));
    }
    if (GetPrivateProfileIntA("Reimpl", "TrueColorData16", 0, ini)) {   // port direction, not the original (reimpl.h)
        g_truecolor_data16 = true;
        log("TrueColorData16: at 24/32 bits the 16-bit data caches (.frm16, .mdl16, .seq16) are used (not the original)");
    }
    api_trace_install(ini);   // before /Original: both starts are logged
    replay_install(ini);      // the same: input recording and replay with /Original too
    sim_hw_install(ini);      // the simulated display hardware, with /Original too (after the recorders)
    // [Reimpl] NetGraphs=1: the network traffic graphs of a LAN game (NetTrafficGraph_Update 0x520150), drawn only
    // while the byte 0x6d00ec is set, which nothing in the exe writes; set here, with /Original too
    if (GetPrivateProfileIntA("Reimpl", "NetGraphs", 0, ini)) {
        *glob<BYTE *>(0x6d00ec) = 1;
        log("NetGraphs: the network traffic graphs are on (0x6d00ec = 1)");
    }

    const char *cmd = GetCommandLineA();
    for (const char *p = cmd; *p; p++)
        if (starts_with(p, "/Original", 9)) {
            log("/Original: no hooks installed");
            start_checks(check_after_ms, false, false, only);
            return;
        }
    char disabled[1024];
    GetPrivateProfileStringA("Reimpl", "Disable", "", disabled, sizeof disabled, ini);
    if (disabled[0])
        log("Disabled in Zax-Reimpl.ini: %s", disabled);
    bool string_ctor = false;
    g_coverage = GetPrivateProfileIntA("Reimpl", "Coverage", 0, ini) && coverage_init(hooks, count);
    for (int i = 0; i < count; i++) {
        const Hook &h = hooks[i];
        if (is_disabled(disabled, h.address))
            log("disabled %s (0x%x)", h.name, h.address);
        else if (install(h, i) && h.address == 0x4dec90)
            string_ctor = true;
    }
    if (g_coverage)
        coverage_start(inst);
    start_checks(check_after_ms, true, string_ctor, only);
    if (!ok)
        log("Warning: self-test mismatches, hooks installed anyway");
}

}  // namespace zax

extern "C" BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        zax::start(inst);
    } else if (reason == 0) {   // DLL_PROCESS_DETACH
        zax::api_trace_flush();
        zax::replay_flush();
        zax::coverage_flush();
        // Only a process that ends by itself gets here (ExitProcess; reserved non-null): the helper's Stop-Process
        // (TerminateProcess) calls no DllMain, so this line tells a clean quit from a killed game (batch gaps-menu)
        zax::log("DllMain: process detach (%s)", reserved ? "the process ends" : "FreeLibrary");
    }
    return TRUE;
}
