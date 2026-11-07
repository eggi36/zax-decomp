// [Reimpl] Coverage=1 (docs/DECOMPILATION.md 13, "Test coverage of the comparison sets, 2026-10-31"): which
// reimplemented functions a hooked run (/Xriginal) entered at least once, logged as lines of Zax-Reimpl.log. Off by
// default: without the switch none of this runs and the hooks jump straight to their replacements as before.
//
// Two ways in, both recorded, and neither changes the code the game runs:
//  1. Through the exe address (a call of the exe's code, a vtable slot, a fn<>() of our own code): the hook's jump at
//     the original goes to a stub of 12 bytes, `mov byte ptr [hit + i], 1; jmp replacement` (main.cpp install), which
//     keeps every register, the flags and the stack as they were. Logged as "Coverage: <exe address> ...".
//  2. Directly from our own C++ (a call that never passes the exe address): the replacement's first 8 bytes are
//     replaced by a jump to a one-shot stub (`push k; jmp coverage_entry`); the first entry marks it, puts the 8 bytes
//     back with one atomic write (lock cmpxchg8b) and goes on into the replacement, which from then on runs unpatched.
//     Only a replacement of its own (no other hook shares it: ClassInfo and ClassInfo2 share theirs, for example; those
//     count through their stubs) at a 16-byte boundary (where clang starts every compiled function) with no other
//     replacement in the 16 bytes before or after it (the assembler thunks lie closer together) is patched. Logged as
//     "Coverage direct: <exe address> ..." when its stub (1) did not fire. An inlined copy of a replacement inside
//     another function is not seen (the build's inlining is unchanged).
// A thread looks every 250 ms and logs what was entered since its last look (the VM helper kills the game at the end of
// a job, so there is no end to write at); zax-testtools/tools/coverage_set.py takes the union of the lines. The thread reads only our
// own memory and calls no exe code.
//
// (A first attempt, compiling everything with -finstrument-functions, changed the code: the instrumentation calls
// clobbered ecx, which ZaxNet_RegisterReaders relied on (fixed in zax_net.cpp), and kept `T *const X = glob<T *>(...)`
// from folding to a constant, so input_rest_a.cpp's key table and mouse stayed null in a DLL without a C runtime to
// run the initializer (now functions); the play still crashed after that, in ButtonLook::Draw.)
#include "reimpl.h"

namespace zax {
namespace {

const int STUB_SIZE = 12;    // way 1
const int ENTRY_SIZE = 10;   // way 2: push imm32; jmp rel32
BYTE *g_stubs;               // way 1, one per hook (executable)
BYTE *g_hit;                 // per hook: entered through its exe address
BYTE *g_logged;              // per hook: 1 logged through way 1, 2 through way 2
const Hook *g_hooks;
int g_count;

}  // namespace

// Way 2: the patched replacements
struct CoveragePatch {
    BYTE *at;
    unsigned long long bytes;   // the original first 8 bytes
    int hook;                   // the hook it belongs to
    volatile BYTE entered;
};
static CoveragePatch *g_patches;
static int g_npatches;
static BYTE *g_entries;   // ENTRY_SIZE bytes per patch (executable)

}  // namespace zax

// The first entry of patch k (from coverage_entry, every register saved there): marked, its bytes put back; the
// replacement's address
extern "C" __attribute__((used)) BYTE *__cdecl coverage_entered(int k)
{
    zax::CoveragePatch &p = zax::g_patches[k];
    volatile unsigned long long *cell = reinterpret_cast<volatile unsigned long long *>(p.at);
    unsigned long long now = *cell;
    while (now != p.bytes) {
        const unsigned long long seen = __sync_val_compare_and_swap(cell, now, p.bytes);
        if (seen == now)
            break;
        now = seen;
    }
    p.entered = 1;
    return p.at;
}

// The common part of the one-shot stubs: [esp] = k, [esp+4] the caller's return address. Saves everything, calls
// coverage_entered(k), and goes into the replacement (its bytes back) with the stack as the caller left it.
extern "C" void coverage_entry();
__asm__(".text\n"
        "_coverage_entry:\n"
        "  pushfl\n"
        "  pushal\n"
        "  movl 36(%esp), %eax\n"   // k
        "  pushl %eax\n"
        "  call _coverage_entered\n"
        "  addl $4, %esp\n"
        "  movl %eax, 36(%esp)\n"   // the replacement's address in k's place
        "  popal\n"
        "  popfl\n"
        "  ret\n");                 // into the replacement, [esp] the caller's return address again

namespace zax {

static DWORD dll_text(HINSTANCE inst, DWORD *size)
{
    const BYTE *base = reinterpret_cast<const BYTE *>(inst);
    const BYTE *nt = base + *reinterpret_cast<const DWORD *>(base + 0x3c);
    const WORD nsec = *reinterpret_cast<const WORD *>(nt + 6), opt = *reinterpret_cast<const WORD *>(nt + 0x14);
    const BYTE *sec = nt + 0x18 + opt;
    for (int i = 0; i < nsec; i++, sec += 40)
        if (*reinterpret_cast<const DWORD *>(sec + 36) & 0x20000000) {   // IMAGE_SCN_MEM_EXECUTE
            *size = *reinterpret_cast<const DWORD *>(sec + 8);
            return (DWORD)(uintptr_t)base + *reinterpret_cast<const DWORD *>(sec + 12);
        }
    *size = 0;
    return 0;
}

// Before the hooks go in (main.cpp start, [Reimpl] Coverage=1): the stubs of way 1
bool coverage_init(const Hook *hooks, int count)
{
    g_stubs = (BYTE *)VirtualAlloc(nullptr, count * STUB_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    g_hit = (BYTE *)VirtualAlloc(nullptr, count * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_stubs || !g_hit) {
        log("Coverage: no memory, off");
        g_stubs = nullptr;
        return false;
    }
    g_logged = g_hit + count;
    g_hooks = hooks, g_count = count;
    return true;
}

// The jump target of hook i (main.cpp install): its stub, which marks it and jumps on to the replacement
void *coverage_stub(int i, void *replacement)
{
    if (!g_stubs)
        return replacement;
    BYTE *s = g_stubs + i * STUB_SIZE;
    s[0] = 0xc6, s[1] = 0x05;   // mov byte ptr [imm32], imm8
    *(DWORD *)(s + 2) = (DWORD)(uintptr_t)(g_hit + i);
    s[6] = 1;
    s[7] = 0xe9;                // jmp rel32
    *(INT32 *)(s + 8) = (INT32)((BYTE *)replacement - (s + 12));
    return s;
}

static DWORD hook_target(int i) { return (DWORD)(uintptr_t)g_hooks[i].replacement; }

// After the hooks (main.cpp start): way 2's patches, then the logging thread
static DWORD WINAPI coverage_thread(void *);
void coverage_start(HINSTANCE inst)
{
    if (!g_stubs)
        return;
    DWORD size;
    const DWORD text = dll_text(inst, &size);
    int *order = (int *)VirtualAlloc(nullptr, g_count * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_patches = (CoveragePatch *)VirtualAlloc(nullptr, g_count * sizeof(CoveragePatch), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_entries = (BYTE *)VirtualAlloc(nullptr, g_count * ENTRY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    DWORD old;
    if (!order || !g_patches || !g_entries || !text ||
        !VirtualProtect((void *)(uintptr_t)text, size, PAGE_EXECUTE_READWRITE, &old)) {
        log("Coverage: on, %d hooks; direct entries not recorded (no memory or the DLL's code not writable)", g_count);
    } else {
        // The replacements by address (shell sort of an index array)
        for (int i = 0; i < g_count; i++)
            order[i] = i;
        for (int gap = g_count / 2; gap > 0; gap /= 2)
            for (int i = gap; i < g_count; i++)
                for (int j = i; j >= gap && hook_target(order[j - gap]) > hook_target(order[j]); j -= gap) {
                    const int t = order[j];
                    order[j] = order[j - gap];
                    order[j - gap] = t;
                }
        for (int n = 0; n < g_count; n++) {
            const DWORD a = hook_target(order[n]);
            const DWORD prev = n > 0 ? hook_target(order[n - 1]) : 0, next = n + 1 < g_count ? hook_target(order[n + 1]) : ~0u;
            if (a < text || a + 16 > text + size || (a & 15) || a - prev < 16 || next - a < 16)
                continue;
            const int k = g_npatches++;
            CoveragePatch &p = g_patches[k];
            p.at = (BYTE *)(uintptr_t)a;
            p.bytes = *reinterpret_cast<const unsigned long long *>(p.at);
            p.hook = order[n];
            BYTE *e = g_entries + k * ENTRY_SIZE;
            e[0] = 0x68;   // push imm32
            *(DWORD *)(e + 1) = (DWORD)k;
            e[5] = 0xe9;   // jmp rel32
            *(INT32 *)(e + 6) = (INT32)((BYTE *)&coverage_entry - (e + 10));
            unsigned long long patched = p.bytes;
            BYTE *b = reinterpret_cast<BYTE *>(&patched);
            b[0] = 0xe9;
            *(INT32 *)(b + 1) = (INT32)(e - (p.at + 5));
            __sync_val_compare_and_swap(reinterpret_cast<volatile unsigned long long *>(p.at), p.bytes, patched);
        }
        FlushInstructionCache(GetCurrentProcess(), (void *)(uintptr_t)text, size);
        log("Coverage: on, %d hooks, %d replacements patched for their direct entries", g_count, g_npatches);
    }
    CloseHandle(CreateThread(nullptr, 0x10000, &coverage_thread, nullptr, 0, nullptr));
}

// Logs the hooks entered since the last call, up to 90 addresses per line
static void put(const char *kind, char *line, int &n, int &k, DWORD address, bool flush)
{
    if (!flush) {
        n += wsprintfA(line + n, " 0x%x", address);
        k++;
    }
    if (k && (flush || k == 90)) {
        log("Coverage%s:%s", kind, line);
        n = k = 0;
    }
}
void coverage_flush()
{
    if (!g_stubs)
        return;
    char line[1000];
    int n = 0, k = 0;
    for (int i = 0; i < g_count; i++)
        if (!g_logged[i] && g_hit[i]) {
            g_logged[i] = 1;
            put("", line, n, k, g_hooks[i].address, false);
        }
    put("", line, n, k, 0, true);
    for (int j = 0; j < g_npatches; j++) {
        const int i = g_patches[j].hook;
        if (g_patches[j].entered && !g_logged[i] && !g_hit[i]) {
            g_logged[i] = 2;
            put(" direct", line, n, k, g_hooks[i].address, false);
        }
    }
    put(" direct", line, n, k, 0, true);
}

static DWORD WINAPI coverage_thread(void *)
{
    for (;;) {
        Sleep(250);
        coverage_flush();
    }
}

}  // namespace zax
