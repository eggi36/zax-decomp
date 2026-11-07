// Zax-Standalone.exe: the start of the standalone exe (docs/DECOMPILATION.md 13, "Batch 6-1: the scaffold" and, without
// Zax.exe's image, "Batch 6-10c").
//
// Until batch 6-10c this file was a loader: it copied the user's Zax.exe into the range 0x401000-0x75d000, wrote a jump
// to the replacement at every hook site and let the code that was not yet ours run there. Nothing of Zax.exe is needed
// at run time any more: every function the game runs is reimplemented, its data are our own blocks (docs/exe_layout.csv,
// the initial bytes taken from the user's Zax.exe at build time by tools/extract_exe_data.py) and the address layer
// (reimpl.h) reaches both. The entry
//  0. keeps the range of the exe's image inaccessible: in the normal build (linked at 0x400000) the reserved range
//     0x401000-0x75d000 inside our own image (image_space.s) is made PAGE_NOACCESS; the test build at a second image
//     base (build_exe.sh with ZAX_EXE_BASE, ZAX_IMAGE_ALLOC) reserves 0x400000-0x760000 PAGE_NOACCESS itself; when
//     the process already has memory there (the x86 VMs: heap, stacks, NLS sections) it starts itself again suspended
//     with the range reserved before that process initialises; failing that it leaves the process's memory alone,
//     logs its owners and reserves only the free parts. A leftover use of an exe address therefore faults
//     ([Reimpl] CrashNames=1 names it),
//  1. relocates the data pointers between our own blocks and resolves the original's imports into our own IAT (the
//     import descriptors are the copy of the exe's .rdata in our blocks),
//  2. fills the address layer's function table with the replacements of the hook tables (every module's ZAX_HOOKS),
//  3. installs the launcher's switches (standalone/launcher.cpp), the recorders of [Reimpl] TraceApi=1 (api_trace.cpp),
//     the input recording and replay ([Reimpl] Record=1 / Replay=1 / FrameLog=1, replay.cpp) and the simulated display
//     hardware ([Reimpl] ForceNoMMX=1 / Force555=1, sim_hw.cpp), as the DLL does,
//  4. sets the code pointers of the data and of the assembler text to the function table's entries,
//  5. starts the reimplemented WinMainCRTStartup (the exe's entry point 0x5d328f) through the function table.
// No in-process checks (the check modules are not linked), no /Original, no [Reimpl] Disable= (there is no original
// code to fall back to). Log: Zax-Reimpl.log, settings: Zax-Reimpl.ini, both next to the exe, as for the DLL.
#include "../src/reimpl.h"
#include "../src/standalone_launcher.h"

extern "C" char zax_image_space[], zax_image_space_end[];

WINIMPORT HANDLE WINAPI GetModuleHandleA(const char *name);
WINIMPORT HANDLE WINAPI LoadLibraryA(const char *name);
WINIMPORT void *WINAPI GetProcAddress(HANDLE module, const char *name);
WINIMPORT int WINAPI MessageBoxA(HANDLE wnd, const char *text, const char *caption, unsigned type);
WINIMPORT void WINAPI ExitProcess(unsigned code);
WINIMPORT void *WINAPI AddVectoredExceptionHandler(unsigned long first, long(WINAPI *handler)(void *info));
WINIMPORT HANDLE WINAPI GetCurrentThread(void);
WINIMPORT BOOL WINAPI DuplicateHandle(HANDLE sp, HANDLE s, HANDLE tp, HANDLE *t, DWORD access, BOOL inherit, DWORD options);
WINIMPORT DWORD WINAPI SuspendThread(HANDLE t);
WINIMPORT DWORD WINAPI ResumeThread(HANDLE t);
WINIMPORT BOOL WINAPI GetThreadContext(HANDLE t, void *ctx);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
#ifdef ZAX_IMAGE_ALLOC
WINIMPORT BOOL WINAPI CreateProcessA(const char *app, char *cmd, void *psec, void *tsec, BOOL inherit, DWORD flags, void *env,
                                     const char *dir, void *startup, void *info);
WINIMPORT void WINAPI GetStartupInfoA(void *startup);
WINIMPORT void *WINAPI VirtualAllocEx(HANDLE process, void *addr, size_t size, DWORD type, DWORD prot);
WINIMPORT size_t WINAPI VirtualQueryEx(HANDLE process, const void *addr, void *info, size_t size);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);
WINIMPORT BOOL WINAPI WriteProcessMemory(HANDLE process, void *addr, const void *buf, size_t size, size_t *written);
WINIMPORT DWORD WINAPI WaitForSingleObject(HANDLE h, DWORD ms);
WINIMPORT BOOL WINAPI GetExitCodeProcess(HANDLE process, DWORD *code);
WINIMPORT BOOL WINAPI TerminateProcess(HANDLE process, unsigned code);
WINIMPORT DWORD WINAPI SetFilePointer(HANDLE f, long distance, long *high, DWORD method);
// Set by the starting process in us while we are suspended (relaunch_with_range): the range was reserved for us
extern "C" volatile DWORD zax_range_child;
volatile DWORD zax_range_child = 0;
#endif

namespace zax {

void api_trace_install(const char *ini);   // api_trace.cpp: [Reimpl] TraceApi=1
void replay_install(const char *ini);      // replay.cpp: [Reimpl] Record=1 / Replay=1 / FrameLog=1
void sim_hw_install(const char *ini);      // sim_hw.cpp: [Reimpl] ForceNoMMX=1 / Force555=1

// The address layer's exe backend (reimpl.h, docs/DECOMPILATION.md 13, "Batch 6-3"): the function table fn<> calls
// through, the replacements of the hook tables
namespace layer {
extern "C" {
DWORD zax_fn_table[FN_COUNT];
}
}  // namespace layer
// own_blocks.cpp (tools/extract_exe_data.py): the data pointers to own blocks, as exe addresses (location, target)
extern "C" const unsigned long zax_relocs[][2];
extern "C" const int zax_reloc_count;
// the code pointers of the data (batch 6-10b): location as an exe address, index into zax_fn_table
extern "C" const unsigned long zax_code_relocs[][2];
extern "C" const int zax_code_reloc_count;

namespace {

constexpr DWORD IMAGE_BASE = 0x400000;
constexpr DWORD IMAGE_START = 0x401000, IMAGE_END = 0x75d000;   // .text ... .rsrc of Zax.exe
// The test build at a second image base (build_exe.sh with ZAX_EXE_BASE, batch 6-10b): our exe lies elsewhere and the
// range 0x400000-0x760000 is reserved inaccessible here. The normal build has its image space at 0x401000-0x75d000
// inside our own image.
#ifdef ZAX_IMAGE_ALLOC
constexpr DWORD RANGE_START = 0x400000, RANGE_END = 0x760000;
#else
constexpr DWORD RANGE_START = IMAGE_START, RANGE_END = IMAGE_END;
#endif
constexpr DWORD ENTRY = 0x5d328f;                              // WinMainCRTStartup
constexpr DWORD IMPORT_DIR = 0x6077d8;                         // the import descriptors (in .rdata)
constexpr DWORD PAGE_NOACCESS = 0x01;
#ifdef ZAX_IMAGE_ALLOC
constexpr DWORD MEM_RESERVE_ = 0x2000, MEM_COMMIT_ = 0x1000;
#endif

HANDLE g_log = INVALID_HANDLE_VALUE;
DWORD g_module_start, g_module_end;   // our own image, for the names of the diagnosis

}  // namespace

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

namespace {

[[noreturn]] void fail(const char *what)
{
    log("Zax-Standalone: %s", what);
    MessageBoxA(nullptr, what, "Zax-Standalone", 0x10 /* MB_ICONERROR */);
    ExitProcess(1);
    __builtin_unreachable();
}

// A raw address of the process, untranslated (reimpl.h's at<> translates the exe's range to the layer in this build)
template <typename T> inline T raw(DWORD addr) { return reinterpret_cast<T>(static_cast<uintptr_t>(addr)); }

struct MemoryInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};

// ---- 0. The exe's range, inaccessible ----------------------------------------------------------------------------

#ifdef ZAX_IMAGE_ALLOC
// What owns an allocation of the process inside the range (docs/DECOMPILATION.md 12, "The second base on the x86
// VMs"): the x86 Windows VMs (WoW64) have the loader's and the system's memory there before our entry runs, when our
// exe lies at the second base. Named from the process heaps, the threads' stacks (TEB +4 StackBase, +0xe0c DeallocationStack),
// the PEB's pointers, the mapped file and the module of an image, so the log says what was left alone.
constexpr int MAX_OWNERS = 256;
struct Owner {
    DWORD allocation;   // allocation base inside the range
    char what[96];
};
Owner g_owners[MAX_OWNERS];
int g_owner_count;

DWORD allocation_of(DWORD addr)
{
    MemoryInfo mi;
    if (!VirtualQuery(raw<void *>(addr), &mi, sizeof mi) || mi.state == 0x10000 /* MEM_FREE */)
        return 0;
    return (DWORD)(uintptr_t)mi.allocation_base;
}

void add_owner(DWORD addr, const char *fmt, DWORD value)
{
    if (addr < RANGE_START || addr >= RANGE_END || g_owner_count == MAX_OWNERS)
        return;
    const DWORD allocation = allocation_of(addr);
    if (!allocation)
        return;
    Owner &o = g_owners[g_owner_count];
    o.allocation = allocation;
    wsprintfA(o.what, fmt, value);
    for (int i = 0; i < g_owner_count; i++)
        if (g_owners[i].allocation == allocation && !lstrcmpA(g_owners[i].what, o.what))
            return;   // the same owner found twice (a stack by its base and its top)
    g_owner_count++;
}

void find_owners()
{
    using GetProcessHeapsFn = DWORD(WINAPI *)(DWORD, HANDLE *);
    using OpenThreadFn = HANDLE(WINAPI *)(DWORD, BOOL, DWORD);
    using QueryThreadFn = long(WINAPI *)(HANDLE, int, void *, DWORD, DWORD *);
    HANDLE kernel32 = GetModuleHandleA("kernel32.dll");
    // the process heaps
    HANDLE heaps[64];
    if (auto get = reinterpret_cast<GetProcessHeapsFn>(GetProcAddress(kernel32, "GetProcessHeaps"))) {
        const DWORD n = get(64, heaps);
        for (DWORD i = 0; i < n && i < 64; i++)
            add_owner((DWORD)(uintptr_t)heaps[i], i == 0 ? "process heap %u (GetProcessHeap)" : "process heap %u", i);
    }
    // the PEB and the pointers in it (the offsets of the 32-bit PEB: 0x18 ProcessHeap, 0x38 ApiSetMap, 0x4c
    // ReadOnlySharedMemoryBase, 0x58/0x5c/0x60 the code page tables, 0x90 ProcessHeaps, 0x94 GdiSharedHandleTable,
    // 0x1f8/0x200 the activation context data, ...)
    DWORD peb, teb;
    __asm__("movl %%fs:0x30, %0\n\tmovl %%fs:0x18, %1" : "=r"(peb), "=r"(teb));
    add_owner(peb, "the PEB", 0);
    for (DWORD off = 0; off < 0x480; off += 4)
        add_owner(*raw<const DWORD *>(peb + off), "PEB+0x%x points into it", off);
    // the threads and their stacks
    add_owner(teb, "the TEB of our thread", 0);
    auto open = reinterpret_cast<OpenThreadFn>(GetProcAddress(kernel32, "OpenThread"));
    auto query = reinterpret_cast<QueryThreadFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread"));
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (open && query && snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 e;
        e.dwSize = sizeof e;
        const DWORD process = GetCurrentProcessId(), self = GetCurrentThreadId();
        int threads = 0;
        for (BOOL ok = Thread32First(snap, &e); ok; ok = Thread32Next(snap, &e)) {
            if (e.th32OwnerProcessID != process)
                continue;
            threads++;
            HANDLE t = open(0x40 /* THREAD_QUERY_INFORMATION */, FALSE, e.th32ThreadID);
            if (!t)
                continue;
            DWORD tbi[7];   // THREAD_BASIC_INFORMATION: ExitStatus, TebBaseAddress, ClientId, AffinityMask, priorities
            if (query(t, 0 /* ThreadBasicInformation */, tbi, sizeof tbi, nullptr) >= 0 && tbi[1]) {
                const DWORD tteb = tbi[1];
                const bool own = e.th32ThreadID == self;
                add_owner(tteb, own ? "the TEB of our thread %u" : "the TEB of thread %u", e.th32ThreadID);
                add_owner(*raw<const DWORD *>(tteb + 0xe0c), own ? "the stack of our thread %u" : "the stack of thread %u",
                          e.th32ThreadID);
                add_owner(*raw<const DWORD *>(tteb + 4) - 1, own ? "the stack of our thread %u" : "the stack of thread %u",
                          e.th32ThreadID);
                add_owner(*raw<const DWORD *>(tteb + 0x2c), "thread %u's TLS array (TEB+0x2c)", e.th32ThreadID);
            }
            CloseHandle(t);
        }
        CloseHandle(snap);
        log("Zax-Standalone: %d threads in the process", threads);
    }
}

void describe(const MemoryInfo &mi, char *out)
{
    const DWORD allocation = (DWORD)(uintptr_t)mi.allocation_base;
    int o = 0;
    out[0] = 0;
    for (int i = 0; i < g_owner_count && o < 800; i++)
        if (g_owners[i].allocation == allocation)
            o += wsprintfA(out + o, "; %s", g_owners[i].what);
    if (mi.type == 0x1000000 /* MEM_IMAGE */) {
        char name[MAX_PATH];
        if (GetModuleFileNameA(static_cast<HINSTANCE>(mi.allocation_base), name, MAX_PATH))
            o += wsprintfA(out + o, "; image %s", name);
    } else if (mi.type == 0x40000 /* MEM_MAPPED */) {
        using MappedNameFn = DWORD(WINAPI *)(HANDLE, void *, char *, DWORD);
        auto mapped = reinterpret_cast<MappedNameFn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetMappedFileNameA"));
        if (!mapped)
            if (HANDLE psapi = LoadLibraryA("psapi.dll"))
                mapped = reinterpret_cast<MappedNameFn>(GetProcAddress(psapi, "GetMappedFileNameA"));
        char name[MAX_PATH];
        if (mapped && mapped(GetCurrentProcess(), mi.base, name, MAX_PATH))
            o += wsprintfA(out + o, "; file %s", name);
        else
            o += wsprintfA(out + o, "; no file (a section backed by the paging file)");
    }
}
#endif

#ifdef ZAX_IMAGE_ALLOC
// On the x86 VMs (WoW64) the process heap (0x6a0000), thread stacks, the NLS tables and other sections of the system lie
// in the range before our entry (2026-10-31). Leaving them there is not enough: at<> takes every address of the range
// for a key of the address layer, so a heap pointer of the game there (DirectDraw's surface objects, the game's own
// blocks) becomes our own block's place. So the exe starts itself again, suspended, reserves the range in that process
// before its initialisation (ntdll's loader, the heaps, the NLS mappings) runs, and waits for it with its exit code.
// Returns only when the range is free here anyway (nothing to do) or the relaunch failed (logged).
void relaunch_with_range()
{
    void *range = VirtualAlloc(raw<void *>(RANGE_START), RANGE_END - RANGE_START, MEM_RESERVE_, PAGE_NOACCESS);
    if ((DWORD)(uintptr_t)range == RANGE_START) {
        VirtualFree(range, 0, 0x8000 /* MEM_RELEASE */);   // free here: protect_image_range reserves it
        return;
    }
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    DWORD startup[17];          // STARTUPINFOA: the same window state as ours; cbReserved2/lpReserved2 (the CRT's
    GetStartupInfoA(startup);   // inherited handles) cleared, no handles are inherited
    startup[12] &= 0xffff, startup[13] = 0;
    DWORD info[4];              // PROCESS_INFORMATION
    if (!CreateProcessA(path, const_cast<char *>(GetCommandLineA()), nullptr, nullptr, FALSE, 4 /* CREATE_SUSPENDED */,
                        nullptr, nullptr, startup, info)) {
        log("Zax-Standalone: the range is not free and starting ourselves suspended failed; going on in this process");
        return;
    }
    HANDLE process = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(info[0]));
    HANDLE thread = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(info[1]));
    void *got = VirtualAllocEx(process, raw<void *>(RANGE_START), RANGE_END - RANGE_START, MEM_RESERVE_, PAGE_NOACCESS);
    const DWORD one = 1;
    if ((DWORD)(uintptr_t)got != RANGE_START ||
        !WriteProcessMemory(process, const_cast<DWORD *>(&zax_range_child), &one, 4, nullptr)) {
        MemoryInfo mi;
        for (DWORD a = RANGE_START; a < RANGE_END && VirtualQueryEx(process, raw<void *>(a), &mi, sizeof mi);
             a = (DWORD)(uintptr_t)mi.base + mi.size)
            log("Zax-Standalone: suspended process %u: 0x%x: %u bytes, state 0x%x, type 0x%x, allocation 0x%x", info[2],
                (DWORD)(uintptr_t)mi.base, (DWORD)mi.size, mi.state, mi.type, (DWORD)(uintptr_t)mi.allocation_base);
        log("Zax-Standalone: the range cannot be reserved in the suspended process %u either; going on in this process",
            info[2]);
        TerminateProcess(process, 1);
        CloseHandle(thread);
        CloseHandle(process);
        return;
    }
    log("Zax-Standalone: the range is not free here; started ourselves again as process %u, suspended, with 0x%x-0x%x "
        "reserved PAGE_NOACCESS before its initialisation; waiting for it", info[2], RANGE_START, RANGE_END);
    CloseHandle(g_log);   // the started process appends to the log
    g_log = INVALID_HANDLE_VALUE;
    ResumeThread(thread);
    CloseHandle(thread);
    WaitForSingleObject(process, 0xffffffff);
    DWORD code = 1;
    GetExitCodeProcess(process, &code);
    ExitProcess(code);
}
#endif

void protect_image_range()
{
    DWORD old;
#ifdef ZAX_IMAGE_ALLOC
    void *range = VirtualAlloc(raw<void *>(RANGE_START), RANGE_END - RANGE_START, MEM_RESERVE_, PAGE_NOACCESS);
    if ((DWORD)(uintptr_t)range == RANGE_START) {
        log("Zax-Standalone: 0x%x-0x%x reserved PAGE_NOACCESS (our exe at 0x%x)", RANGE_START, RANGE_END, g_module_start);
        return;
    }
    // Reserved by the starting process before our process initialised (relaunch_with_range)
    MemoryInfo first;
    if (zax_range_child && VirtualQuery(raw<void *>(RANGE_START), &first, sizeof first) && first.state == MEM_RESERVE_ &&
        (DWORD)(uintptr_t)first.allocation_base == RANGE_START && first.size >= RANGE_END - RANGE_START) {
        log("Zax-Standalone: 0x%x-0x%x reserved PAGE_NOACCESS by the starting process before ours initialised (our exe at "
            "0x%x)", RANGE_START, RANGE_END, g_module_start);
        return;
    }
    // Something of the process lies there before our entry: on the ARM VMs a reservation from 0x10000 (batch 6-10b), on
    // the x86 VMs (WoW64) the loader's and the system's memory (process heaps, thread stacks, mapped NLS tables and
    // sections; 2026-10-31). None of it is ours: it is left as it is and logged with its owner, and only the free
    // parts are reserved PAGE_NOACCESS (whole 64 KB units, the allocation granularity; the rest of a free region
    // cannot be allocated by anyone either). Until 2026-10-31 the accessible regions were made PAGE_NOACCESS, which
    // ended the process at once on the x86 VMs.
    find_owners();
    int reserved = 0, left = 0;
    DWORD reserved_bytes = 0;
    MemoryInfo mi;
    char owner[1400];
    for (DWORD a = RANGE_START; a < RANGE_END && VirtualQuery(raw<void *>(a), &mi, sizeof mi); a = (DWORD)(uintptr_t)mi.base + mi.size) {
        const DWORD base = (DWORD)(uintptr_t)mi.base;
        DWORD end = base + (DWORD)mi.size;
        if (end > RANGE_END)
            end = RANGE_END;
        if (mi.state == 0x10000 /* MEM_FREE */) {
            const DWORD from = (base + 0xffff) & ~0xffffu, to = end & ~0xffffu;
            if (from < to && (DWORD)(uintptr_t)VirtualAlloc(raw<void *>(from), to - from, MEM_RESERVE_, PAGE_NOACCESS) == from) {
                log("Zax-Standalone: 0x%x: %u bytes free, 0x%x-0x%x reserved PAGE_NOACCESS", base, end - base, from, to);
                reserved++, reserved_bytes += to - from;
            } else {
                log("Zax-Standalone: 0x%x: %u bytes free, %s", base, end - base,
                    from < to ? "not reserved (VirtualAlloc failed)" : "no whole 64 KB unit to reserve");
            }
            continue;
        }
        const bool open = mi.state == MEM_COMMIT_ && !(mi.protect & (PAGE_NOACCESS | 0x100 /* PAGE_GUARD */));
        describe(mi, owner);
        log("Zax-Standalone: 0x%x: %u bytes, state 0x%x, protect 0x%x, type 0x%x, allocation 0x%x%s, left alone%s", base,
            (DWORD)mi.size, mi.state, mi.protect, mi.type, (DWORD)(uintptr_t)mi.allocation_base, open ? " ACCESSIBLE" : "",
            owner);
        if (open)
            left++;
    }
    log("Zax-Standalone: 0x%x-0x%x not ours to reserve as a whole; %d free parts (%u bytes) reserved PAGE_NOACCESS, "
        "%d accessible regions of the process left alone", RANGE_START, RANGE_END, reserved, reserved_bytes, left);
    (void)old;
#else
    if ((DWORD)(uintptr_t)zax_image_space != IMAGE_START || (DWORD)(uintptr_t)zax_image_space_end != IMAGE_END)
        fail("the image space is not at 0x401000-0x75d000 (build error)");
    if (!VirtualProtect(raw<void *>(RANGE_START), RANGE_END - RANGE_START, PAGE_NOACCESS, &old))
        fail("VirtualProtect of the image space failed");
    log("Zax-Standalone: 0x%x-0x%x (the exe's image range inside ours) PAGE_NOACCESS", RANGE_START, RANGE_END);
#endif
}

// ---- 1. Our own blocks and the imports ----------------------------------------------------------------------------

// The data pointers to own blocks (in our own blocks; own_blocks.cpp): the exe address of their target replaced by our
// own block's place
void own_blocks()
{
    DWORD bytes = 0;
    for (int i = 0; i < layer::OWN_COUNT; i++)
        bytes += layer::OWN[i].size;
    for (int i = 0; i < zax_reloc_count; i++)
        *glob_dyn<DWORD *>(zax_relocs[i][0]) = glob_addr_dyn(zax_relocs[i][1]);
    log("Zax-Standalone: %d own blocks (%u bytes), %d pointers to them relocated", layer::OWN_COUNT, bytes, zax_reloc_count);
}

// The import descriptors of the exe's .rdata (our own blocks): every DLL by name, every function by name or ordinal into
// our own IAT slot
void resolve_imports()
{
    int dlls = 0, functions = 0;
    for (const DWORD *d = glob_dyn<const DWORD *>(IMPORT_DIR); d[3]; d += 5, dlls++) {
        const char *name = glob_dyn<const char *>(IMAGE_BASE + d[3]);
        HANDLE module = LoadLibraryA(name);
        if (!module) {
            char msg[MAX_PATH + 64];
            wsprintfA(msg, "Cannot load %s (an import of Zax.exe)", name);
            fail(msg);
        }
        const DWORD *names = glob_dyn<const DWORD *>(IMAGE_BASE + (d[0] ? d[0] : d[4]));
        for (int i = 0; names[i]; i++, functions++) {
            const bool ordinal = (names[i] & 0x80000000) != 0;
            const char *fn = ordinal ? raw<const char *>(names[i] & 0xffff) : glob_dyn<const char *>(IMAGE_BASE + names[i] + 2);
            void *p = GetProcAddress(module, fn);
            if (!p) {
                char msg[MAX_PATH + 96];
                if (ordinal)
                    wsprintfA(msg, "%s has no ordinal %u (an import of Zax.exe)", name, names[i] & 0xffff);
                else
                    wsprintfA(msg, "%s has no %s (an import of Zax.exe)", name, fn);
                fail(msg);
            }
            *glob_dyn<DWORD *>(IMAGE_BASE + d[4] + 4 * i) = (DWORD)(uintptr_t)p;
        }
    }
    log("Zax-Standalone: %d imports of %d DLLs resolved into our own IAT", functions, dlls);
}

}  // namespace

// ---- 2. The function table (the hook tables of main.cpp) ----------------------------------------------------------

#define ZAX_HOOK_MODULE(name) int hooks_##name(Hook *out);
#include "hook_tables.inc"
#undef ZAX_HOOK_MODULE

// The checks' hook in ZString's constructor (zstring.cpp, checks.cpp): no checks in the standalone exe
void run_due_checks() {}

// A path of the exe that never runs in the game reached after all (reimpl.h original_or_dead, exe_or_dead; batch 6-10c)
[[noreturn]] void dead_path(DWORD address)
{
    char msg[160];
    wsprintfA(msg, "the exe's code at 0x%x was reached, a path the standalone exe does not have", address);
    fail(msg);
}

// original<> (reimpl.h): the exe's own code, which the standalone exe does not have; the production code reaches it only
// in the DLL (the uses are #ifndef ZAX_STANDALONE or original_or_dead)
void *original_address(DWORD address) { dead_path(address); }

namespace {

int hook_list(Hook *out)
{
    int n = 0;
#define ZAX_HOOK_MODULE(name) n += hooks_##name(out ? out + n : nullptr);
#include "hook_tables.inc"
#undef ZAX_HOOK_MODULE
    return n;
}

const Hook *g_hook_table;
int g_hook_table_count;

void fill_function_table(const char *ini)
{
    const int count = hook_list(nullptr);
    Hook *hooks = (Hook *)VirtualAlloc(nullptr, count * sizeof(Hook), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!hooks)
        fail("no memory for the hook table");
    hook_list(hooks);
    g_hook_table = hooks;
    g_hook_table_count = count;
    for (const char *p = GetCommandLineA(); *p; p++)
        if (p[0] == '/' && (p[1] | 0x20) == 'o' && (p[2] | 0x20) == 'r' && (p[3] | 0x20) == 'i' && (p[4] | 0x20) == 'g') {
            log("Zax-Standalone: /Original ignored (no original code)");
            break;
        }
    char disabled[1024];
    GetPrivateProfileStringA("Reimpl", "Disable", "", disabled, sizeof disabled, ini);
    if (disabled[0])
        log("Zax-Standalone: [Reimpl] Disable= ignored (no original code to fall back to): %s", disabled);
    int in_table = 0;
    for (int i = 0; i < count; i++) {
        const int k = layer::find(layer::FN, hooks[i].address);
        if (k >= 0 && !layer::zax_fn_table[k]) {
            layer::zax_fn_table[k] = (DWORD)(uintptr_t)hooks[i].replacement;
            in_table++;
        }
    }
    log("Zax-Reimpl: %d functions", count);
    log("Zax-Standalone: function table: %d of %d entries the replacement", in_table, layer::FN_COUNT);
    if (in_table != layer::FN_COUNT)
        for (int k = 0; k < layer::FN_COUNT; k++)
            if (!layer::zax_fn_table[k])
                log("Zax-Standalone: no replacement for 0x%x (a call faults)", layer::FN[k]);
}

// ---- 4. The code values of the data and of the assembler text ------------------------------------------------------

// [Reimpl] CodePointerRange=lo-hi (hex exe addresses, a diagnosis): only the pointers whose location lies in [lo, hi)
DWORD hex_value(const char *&s)
{
    DWORD v = 0;
    for (; (*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F') || *s == 'x' || *s == 'X'; s++)
        if (*s != 'x' && *s != 'X')
            v = v * 16 + (*s <= '9' ? *s - '0' : (*s | 0x20) - 'a' + 10);
    return v;
}

// The code pointers of the data (vtable slots, the C runtime's initializer and atexit tables, callbacks; batch 6-10b)
// set to the function table's entries, as the layer's code values (fn_addr, code; reimpl.h), so a slot compared with a
// code value agrees
void code_pointers(const char *ini)
{
    char range[64];
    GetPrivateProfileStringA("Reimpl", "CodePointerRange", "", range, sizeof range, ini);
    DWORD lo = 0, hi = 0xffffffff;
    if (range[0]) {
        const char *s = range;
        lo = hex_value(s);
        if (*s == '-')
            s++;
        hi = hex_value(s);
        log("Zax-Standalone: CodePointerRange 0x%x-0x%x", lo, hi);
    }
    int set = 0, kept = 0;
    for (int i = 0; i < zax_code_reloc_count; i++) {
        if (zax_code_relocs[i][0] < lo || zax_code_relocs[i][0] >= hi)
            continue;
        DWORD *p = glob_dyn<DWORD *>(zax_code_relocs[i][0]);
        if (*p == layer::FN[zax_code_relocs[i][1]]) {
            *p = layer::zax_fn_table[zax_code_relocs[i][1]];
            set++;
        } else {
            log("Zax-Standalone: code pointer at 0x%x changed before the start (0x%x), left as it is", zax_code_relocs[i][0], *p);
            kept++;
        }
    }
    log("Zax-Standalone: %d code pointers of the data set to the function table, %d left", set, kept);
}

// The code values of the assembler text (reimpl.h ZAX_ASM_CODE_VALUES, ZAX_ASM_PUSH_CODE, ZAX_ASM_CODE_ATT; batch 6-10c):
// every dword of the section .zaxcv is an exe address, replaced here by the function table's entry
extern "C" DWORD zax_cv_start[], zax_cv_end[];
__asm__(".section .zaxcv$a,\"dw\"\n.globl _zax_cv_start\n_zax_cv_start:\n"
        ".section .zaxcv$z,\"dw\"\n.globl _zax_cv_end\n_zax_cv_end:\n.text\n");

void asm_code_values()
{
    int n = 0;
    for (DWORD *p = zax_cv_start; p < zax_cv_end; p++, n++)
        *p = fn_addr_dyn(*p);
    log("Zax-Standalone: %d code values of the assembler text set to the function table", n);
}

}  // namespace

// The hook whose replacement starts last at or before an address of this exe (replay.cpp's heap trace names its callers
// with it), as main.cpp's for the DLL
const char *hook_near(DWORD address, DWORD *start)
{
    const Hook *best = nullptr;
    for (int i = 0; i < g_hook_table_count; i++) {
        const DWORD r = (DWORD)(uintptr_t)g_hook_table[i].replacement;
        if (r <= address && (!best || r > (DWORD)(uintptr_t)best->replacement))
            best = &g_hook_table[i];
    }
    if (start)
        *start = best ? (DWORD)(uintptr_t)best->replacement : 0;
    return best ? best->name : "?";
}

// ---- Diagnosis: [Reimpl] CrashNames=1, StackSamples=n (batch 6-10b) ------------------------------------------------
namespace probe {

// "name+offset" of an address: an exe address by the hook (the function) that starts last before it, an address of
// ours by the replacement before it
const char *exe_name(DWORD a, DWORD *start)
{
    const Hook *best = nullptr;
    for (int i = 0; i < g_hook_table_count; i++)
        if (g_hook_table[i].address <= a && (!best || g_hook_table[i].address > best->address))
            best = &g_hook_table[i];
    *start = best ? best->address : 0;
    return best ? best->name : "?";
}
void where(DWORD a, char *out)
{
    DWORD start = 0;
    const char *name = a >= IMAGE_START && a < IMAGE_END ? exe_name(a, &start) : hook_near(a, &start);
    wsprintfA(out, "0x%x %s+0x%x", a, name, a - start);
}
bool code_like(DWORD v) { return (v >= IMAGE_START && v < 0x5ea000) || (v >= g_module_start && v < g_module_end); }

// StackSamples=n: from 20 s after the start, every 5 s, the main thread's eip and the return-address-like dwords of its
// stack, n times
HANDLE g_main;
DWORD WINAPI sampler(void *arg)
{
    const int n = (int)(uintptr_t)arg;
    Sleep(20000);
    for (int k = 0; k < n; k++, Sleep(5000)) {
        alignas(16) DWORD ctx[0x2cc / 4] = {};
        ctx[0] = 0x10007;   // CONTEXT_FULL (i386)
        if (SuspendThread(g_main) == 0xffffffff)
            return 0;
        const bool ok = GetThreadContext(g_main, ctx);
        char line[1100];
        int o = 0;
        if (ok) {
            char w[160];
            where(ctx[0xb8 / 4], w);
            o = wsprintfA(line, "Stack sample %d: eip %s; stack", k, w);
            const DWORD *sp = raw<const DWORD *>(ctx[0xc4 / 4]);
            for (int i = 0, found = 0; i < 2048 && found < 10 && o < 900; i++)
                if (code_like(sp[i])) {
                    where(sp[i], w);
                    o += wsprintfA(line + o, " | %s", w);
                    found++;
                }
        }
        ResumeThread(g_main);
        if (ok)
            log("%s", line);
    }
    return 0;
}

// CrashNames=1: the first access violation of the process with names, the instruction and the return-address-like
// dwords of the stack (an access to the exe's range, inaccessible since batch 6-10c, shows here)
bool g_crash_named;
long WINAPI on_crash(void *info)
{
    DWORD *const *p = static_cast<DWORD *const *>(info);
    const DWORD *rec = p[0], *ctx = p[1];
    if (g_crash_named || rec[0] != 0xc0000005)
        return 0;
    g_crash_named = true;
    char line[1100], w[160];
    where(ctx[0xb8 / 4], w);
    int o = wsprintfA(line, "Crash: access 0x%x at %s; stack", rec[4] >= 2 ? rec[6] : 0, w);
    const DWORD *sp = raw<const DWORD *>(ctx[0xc4 / 4]);
    for (int i = 0, found = 0; i < 1024 && found < 12 && o < 900; i++)
        if (code_like(sp[i])) {
            where(sp[i], w);
            o += wsprintfA(line + o, " | %s", w);
            found++;
        }
    log("%s", line);
    return 0;
}

void start(const char *ini)
{
    if (GetPrivateProfileIntA("Reimpl", "CrashNames", 0, ini))
        AddVectoredExceptionHandler(1, &on_crash);
    const int n = GetPrivateProfileIntA("Reimpl", "StackSamples", 0, ini);
    if (n <= 0 || !DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main, 0x1fffff, FALSE, 0))
        return;
    CreateThread(nullptr, 0, &sampler, reinterpret_cast<void *>(static_cast<uintptr_t>(n)), 0, nullptr);
}

}  // namespace probe

}  // namespace zax

extern "C" void WINAPI zax_standalone_entry()
{
    using namespace zax;
    char dir[MAX_PATH + 32], path[MAX_PATH + 32], ini[MAX_PATH + 32];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    for (int n = lstrlenA(dir); n > 0 && dir[n - 1] != '\\'; n--)
        dir[n - 1] = 0;
    lstrcpyA(path, dir);
    lstrcatA(path, "Zax-Reimpl.log");
    lstrcpyA(ini, dir);
    lstrcatA(ini, "Zax-Reimpl.ini");
#ifdef ZAX_IMAGE_ALLOC
    // the process our exe started again (relaunch_with_range) appends to its log
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, zax_range_child ? 4 /* OPEN_ALWAYS */ : CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (zax_range_child)
        SetFilePointer(g_log, 0, nullptr, 2 /* FILE_END */);
#else
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
#endif

    HANDLE self = GetModuleHandleA(nullptr);
    g_module_start = (DWORD)(uintptr_t)self;
    const BYTE *pe = static_cast<const BYTE *>(self) + *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(self) + 0x3c);
    g_module_end = g_module_start + *reinterpret_cast<const DWORD *>(pe + 0x50);   // SizeOfImage
    log("Zax-Standalone: module 0x%x-0x%x, no image of Zax.exe", g_module_start, g_module_end);
#ifdef ZAX_IMAGE_ALLOC
    if (g_module_start == IMAGE_BASE)
        fail("the test build at a second image base lies at 0x400000 (build error)");
    if (!zax_range_child)
        relaunch_with_range();
#endif
    protect_image_range();
    own_blocks();
    resolve_imports();
    fill_function_table(ini);
    launcher::install();    // the launcher's switches through the function table (standalone/launcher.cpp, batch 6-10c)
    api_trace_install(ini);
    replay_install(ini);    // as DllMain: after the logged start's recorders
    sim_hw_install(ini);    // as DllMain: the simulated display hardware after the recorders
    code_pointers(ini);
    asm_code_values();
    probe::start(ini);
    log("Zax-Standalone: starting at 0x%x (WinMainCRTStartup)", ENTRY);
    fn<void(__cdecl *)()>(ENTRY)();
    ExitProcess(0);
}
