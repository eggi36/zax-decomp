// The shared machinery of the sound checks (checks_sound_common.h).
#include "checks_sound_common.h"

WINIMPORT BOOL WINAPI FlushInstructionCache(HANDLE process, const void *base, size_t size);

namespace zax::sndc {

bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) < 0x10000 || ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}

// ---- Arenas -----------------------------------------------------------------------------------------------

BYTE *g_arena[2];
DWORD g_used[2];
int g_side = -1;
bool g_overflow;
static DWORD g_high, g_thread;
static DWORD g_stack_base, g_stack_limit;
static CodePatch g_new_patch, g_free_patch, g_pool_new_patch, g_pool_delete_patch;
static BYTE *g_free_trampoline;
static DWORD g_pool_new_target, g_pool_delete_target;   // the pool's hooks (pool.cpp), for other threads

static DWORD g_foreign;   // recorder calls from other threads
static bool own_thread() { return GetCurrentThreadId() == g_thread; }
// The arena is filled with the case's pattern up to g_filled at the start of a side and further as blocks
// go beyond it, so a block's bytes are the pattern on both sides however far the case goes
static DWORD g_filled[2];
static BYTE g_pattern[2];
static BYTE *arena_block(DWORD n)
{
    const int s = g_side;
    const DWORD size = (n + 7) & ~7u;
    if (s < 0 || g_used[s] + size + 8 > ARENA_SIZE) {
        g_overflow = true;
        return nullptr;
    }
    BYTE *p = g_arena[s] + g_used[s];
    g_used[s] += size ? size : 8;
    if (g_used[s] > g_filled[s]) {
        memset(g_arena[s] + g_filled[s], g_pattern[s], g_used[s] - g_filled[s]);
        g_filled[s] = g_used[s];
    }
    return p;
}
BYTE *alloc(DWORD n)
{
    BYTE *p = arena_block(n);
    if (p)
        memset(p, 0, n);
    return p;
}
static void *__cdecl arena_new(DWORD n)
{
    // a new pool chunk stays in the C heap (check_util.h, pool_grow_call)
    if (g_side >= 0 && own_thread() && !pool_grow_call((DWORD)(uintptr_t)__builtin_return_address(0), n))
        if (BYTE *p = arena_block(n))
            return p;
    return at<void *(__cdecl *)(DWORD, int)>(0x5d1661)(n, 1);   // __nh_malloc, as operator new
}
static DWORD __cdecl arena_free(void *p)
{
    if (in_arena(ptrv(p), 0) || in_arena(ptrv(p), 1))
        return 0;
    return reinterpret_cast<DWORD(__cdecl *)(void *)>(g_free_trampoline)(p);
}
static void *__fastcall arena_pool_new(DWORD n)
{
    if (g_side >= 0 && own_thread())
        if (BYTE *p = arena_block(n))
            return p;
    return at<void *(__fastcall *)(DWORD)>(g_pool_new_target)(n);
}
// eax: the size for an arena block (pool_delete leaves the pool's eax, which no caller reads)
static DWORD __fastcall arena_pool_delete(void *p, DWORD n)
{
    if (in_arena(ptrv(p), 0) || in_arena(ptrv(p), 1))
        return n;
    return at<DWORD(__fastcall *)(void *, DWORD)>(g_pool_delete_target)(p, n);
}
static DWORD jump_target(DWORD site)
{
    const BYTE *b = at<const BYTE *>(site);
    return b[0] == 0xe9 ? site + 5 + *reinterpret_cast<const DWORD *>(b + 1) : 0;
}
static bool arena_setup()
{
    if (g_arena[0])
        return true;
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(g_stack_base), "=r"(g_stack_limit));
    g_arena[0] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_arena[1] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check_region(g_arena[0], ARENA_SIZE, "sound arena 0");
    check_region(g_arena[1], ARENA_SIZE, "sound arena 1");
    g_free_trampoline = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_arena[0] || !g_arena[1] || !g_free_trampoline)
        return false;
    static const BYTE FIRST[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};   // free: push ebp; mov ebp, esp; push -1
    if (memcmp(original_address(0x5d33e0), FIRST, 5) != 0)   // free is hooked (crt_heap.cpp): its trampoline holds the original bytes
        return false;
    memcpy(g_free_trampoline, FIRST, 5);
    g_free_trampoline[5] = 0xe9;
    *(INT32 *)(g_free_trampoline + 6) = (INT32)(0x5d33e5 - (ptrv(g_free_trampoline) + 10));
    g_pool_new_target = jump_target(0x417710);
    g_pool_delete_target = jump_target(0x54d130);
    g_thread = GetCurrentThreadId();
    return true;
}
static void arena_on()
{
    patch_jump(g_new_patch, 0x5d033a, (void *)&arena_new);
    patch_jump(g_free_patch, 0x5d33e0, (void *)&arena_free);
    if (g_pool_new_target && g_pool_delete_target) {
        patch_jump(g_pool_new_patch, 0x417710, (void *)&arena_pool_new);
        patch_jump(g_pool_delete_patch, 0x54d130, (void *)&arena_pool_delete);
    }
}
static void arena_off()
{
    if (g_pool_new_target && g_pool_delete_target) {
        unpatch(g_pool_delete_patch);
        unpatch(g_pool_new_patch);
    }
    unpatch(g_free_patch);
    unpatch(g_new_patch);
}

// ---- Values and logs ----------------------------------------------------------------------------------------

unsigned g_vseed;
DWORD g_vindex;
int g_fail_one_in = 8;
Log g_log[2];
FakeBlock g_fakes, g_template, g_after[2];
DWORD g_vt[NKIND][NSLOT];

DWORD nv()
{
    DWORD x = g_vseed ^ (g_vindex++ * 0x9e3779b9u);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
long double nfloat(float lo, float hi)
{
    const DWORD v = nv();
    if ((v & 15) == 0) {
        const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, bits(lo), bits(hi), 0x7149f2cau, 0x3f800000u};
        return (long double)fbits(sp[(v >> 4) & 7]);
    }
    return (long double)(lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f));
}
static bool on_stack(DWORD v) { return v >= g_stack_limit && v < g_stack_base; }
DWORD map(DWORD v)
{
    if (g_side >= 0 && in_arena(v, g_side))
        return 0xa0000000u | (v - ptrv(g_arena[g_side]));
    if (on_stack(v))
        return 0xb0000000u | (v & 0xff);
    return v;
}
static void log_event(const DWORD *e)
{
    Log &l = g_log[g_side < 0 ? 0 : g_side];
    l.hash = fnv(l.hash, e, 32);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, 32);
    l.calls++;
}
void ev(DWORD a, DWORD b, DWORD c, DWORD d)
{
    const DWORD e[8] = {0xe0000000u | a, map(b), map(c), map(d), 0, 0, 0, 0};
    log_event(e);
}
static unsigned text_hash(DWORD p)
{
    if (!p)
        return 0;
    unsigned h = 2166136261u;
    for (const char *s = at<const char *>(p); readable(s, 1) && *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    return h;
}
static DWORD dword_at(DWORD p, DWORD off) { return p && readable(at<void *>(p + off), 4) ? *at<DWORD *>(p + off) : 0xdeadbeefu; }

// ---- Recorders ------------------------------------------------------------------------------------------------

constexpr int NDIRECT = 128, NTHUNKS = NDIRECT + NKIND * NSLOT;
static const Spec *g_direct[NDIRECT];
static int g_ndirect;
static Kind g_kinds[NKIND];
static CodePatch g_patches[NDIRECT];

extern "C" {
extern char sndc_thunks[];
DWORD sndc_pop, sndc_ret_float, sndc_edx;
long double sndc_fret;
DWORD __cdecl sndc_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _sndc_thunks\n"
        "_sndc_thunks:\n"
        ".set sndc_i, 0\n"
        ".rept 896\n"
        "  movl $sndc_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _sndc_common - . - 4\n"
        ".set sndc_i, sndc_i + 1\n"
        ".endr\n"
        "_sndc_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _sndc_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _sndc_ret_float\n"
        "  je 1f\n"
        "  fldt _sndc_fret\n"
        "1:\n"
        "  movl _sndc_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _sndc_pop, %esp\n"
        "  jmp *%ecx\n");
static_assert(NTHUNKS == 896, "the thunk count of sndc_thunks");
static DWORD thunk(int i) { return ptrv(sndc_thunks) + 10u * (DWORD)i; }

void set_kind(int kind, const Spec *slots, int n)
{
    g_kinds[kind] = {slots ? slots[0].name : "", slots, n};
    for (int s = 0; s < NSLOT; s++)
        g_vt[kind][s] = thunk(NDIRECT + kind * NSLOT + s);
}

static DWORD arg_value(char kind, DWORD v)
{
    switch (kind) {
    case 'p': return map(v);
    case 'v': return v;
    case 'b': return v & 0xff;
    case 'f': return v;
    case 's': return text_hash(v);
    case 'z': return v ? text_hash(dword_at(v, 0) + 8) : 0u;
    case 'x': return on_stack(v) ? 0xb0000000u | (v & 0xff) : map(v);
    case 'D': return v && readable(at<void *>(v), 16) ? fnv(2166136261u, at<void *>(v), 16) : map(v);
    case 'd': return v && readable(at<void *>(v), 4) ? map(*at<DWORD *>(v)) : map(v);
    case 'q': return v && readable(at<void *>(v), 12) ? fnv(2166136261u, at<void *>(v), 12) : map(v);
    case 'w': {
        if (!v || !readable(at<void *>(v), 18))
            return map(v);
        return fnv(2166136261u, at<void *>(v), 18);
    }
    default: return 0;
    }
}

DWORD __cdecl sndc_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
{
    if (!own_thread()) {
        // another thread (none once the streams are paused): not logged, nothing popped but the return
        g_foreign++;
        sndc_pop = 4;
        sndc_ret_float = 0;
        sndc_edx = 0;
        return 0;
    }
    const Spec *s = nullptr;
    DWORD id = idx;
    if (idx < (DWORD)NDIRECT) {
        s = idx < (DWORD)g_ndirect ? g_direct[idx] : nullptr;
        if (s)
            id = s->address;
    } else {
        const DWORD v = idx - NDIRECT;
        const int kind = (int)(v / NSLOT), slot = (int)(v % NSLOT);
        if (slot < g_kinds[kind].nslots && g_kinds[kind].slots[slot].name)
            s = &g_kinds[kind].slots[slot];
        id = 0x100000u * (DWORD)(kind + 1) + (DWORD)slot;
    }
    if (!s) {
        const DWORD e[8] = {0xdead0000u, id, map(ecx), 0, 0, 0, 0, 0};
        log_event(e);
        sndc_pop = 4;
        sndc_ret_float = 0;
        sndc_edx = 0;
        return 0;
    }
    Call c{s, frame + 1, ecx, edx, 0, false, nv()};
    DWORD e[8] = {id, 0, 0, 0, 0, 0, 0, 0};
    int k = 1;
    if (s->conv == 't')
        e[k++] = map(ecx);
    else if (s->conv == 'f') {
        e[k++] = map(ecx);
        e[k++] = map(edx);
    }   // 'F': fastcall with ecx and edx not logged (locals of different frames)
    for (int i = 0; s->args[i] && k < 8; i++)
        if (s->args[i] != '-')
            e[k++] = arg_value(s->args[i], c.a[i]);
    log_event(e);
    DWORD r;
    switch (s->ret) {
    case 'i': r = nv() % 5; break;
    case 'h': r = g_fail_one_in && nv() % (DWORD)g_fail_one_in == 0 ? 0x80004005u : 0; break;
    case 'z': r = 0; break;
    case '1': r = 1; break;
    case 'b': {
        const DWORD v = nv();
        r = (v & 0xffffff00u) | ((v >> 7) & 1);
        break;
    }
    case 'f':
        c.fret = nfloat(-10.0f, 1000.0f);
        c.fset = true;
        r = nv();
        break;
    case 'c': r = s->custom(c); break;
    default: r = nv(); break;
    }
    sndc_pop = 4 + (s->conv == 'c' ? 0 : 4 * (DWORD)lstrlenA(s->args));
    sndc_ret_float = c.fset ? 1 : 0;
    sndc_fret = c.fret;
    sndc_edx = c.edx_out;
    return r;
}

// The streams' multimedia timer (0x587700, every 50 ms in its own thread) runs through the file buffer,
// ACM and the readers while music or a voice plays; with the recorders patched in it would read fake
// data (and log into the case). Its callback returns at once while the state's +0xe ("in the callback")
// is set, so that is set on every live stream state (the world's music mixer, the application's menu
// music, the voice-over player) for the module's checks; set twice with a wait between, because a
// callback that was already running clears it when it ends. Put back afterwards.
constexpr int MAX_STREAMS = 4;
static BYTE *g_paused[MAX_STREAMS];
static BYTE g_paused_flag[MAX_STREAMS];
static int g_npaused;
static void pause_one(BYTE *holder)
{
    if (!holder || !readable(holder, 4))
        return;
    BYTE *state = *reinterpret_cast<BYTE **>(holder);
    if (!state || !readable(state, 0x68) || g_npaused >= MAX_STREAMS)
        return;
    for (int i = 0; i < g_npaused; i++)
        if (g_paused[i] == state)
            return;
    g_paused[g_npaused] = state;
    g_paused_flag[g_npaused] = 0;
    g_npaused++;
}
static void streams_pause()
{
    g_npaused = 0;
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (world && readable(world + 0x334, 4)) {
        BYTE *mixer = *reinterpret_cast<BYTE **>(world + 0x334);
        if (mixer && readable(mixer, 0x34))
            pause_one(*reinterpret_cast<BYTE **>(mixer + 0x30));
    }
    BYTE *app = *at<BYTE **>(0x713f14);
    if (app && readable(app + 0x274, 4))
        pause_one(*reinterpret_cast<BYTE **>(app + 0x274));
    if (*at<BYTE *>(0x713c34) & 1)
        pause_one(*at<BYTE **>(0x713c28));
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < g_npaused; i++) {
            volatile BYTE &busy = g_paused[i][0xe];
            if (pass == 0)
                g_paused_flag[i] = busy;
            busy = 1;
        }
        if (g_npaused)
            Sleep(120);
    }
}
static void streams_resume()
{
    for (int i = 0; i < g_npaused; i++)
        *reinterpret_cast<volatile BYTE *>(g_paused[i] + 0xe) = g_paused_flag[i];
    g_npaused = 0;
}

bool world_on(const Standins *lists, int nlists)
{
    if (!arena_setup())
        return false;
    streams_pause();
    g_foreign = 0;
    const BYTE *th = reinterpret_cast<const BYTE *>(sndc_thunks);
    for (int k = 0; k < NTHUNKS; k += 97)
        if (th[10 * k] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 10 * k + 1) != (DWORD)k || th[10 * k + 5] != 0xe9) {
            log("  sound checks: recorder thunk %d is broken", k);
            return false;
        }
    g_ndirect = 0;
    for (int l = 0; l < nlists; l++)
        for (int i = 0; i < lists[l].n && g_ndirect < NDIRECT; i++) {
            g_direct[g_ndirect] = &lists[l].specs[i];
            patch_jump(g_patches[g_ndirect], lists[l].specs[i].address, (void *)thunk(g_ndirect));
            g_ndirect++;
        }
    return true;
}
void world_off()
{
    for (int i = g_ndirect; i-- > 0;)
        unpatch(g_patches[i]);
    g_ndirect = 0;
    streams_resume();
    if (g_foreign)
        log("  sound checks: %u recorder calls from other threads (answered 0)", g_foreign);
}

// ---- Hooks out and in -----------------------------------------------------------------------------------------

constexpr int MAX_SITES = 256;
struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
static SavedSite g_sites[MAX_SITES];
static Hook g_hooks[MAX_SITES];
static int g_nsites;
void hooks_out(const Module &m)
{
    g_nsites = 0;
    const int n = m.table(nullptr);
    if (!hook_sites_fit("sound_common", n, MAX_SITES))
        return;
    m.table(g_hooks);
    for (int i = 0; i < n; i++) {
        bool never = false;
        for (int k = 0; k < m.n_never; k++)
            never |= m.never_out[k] == g_hooks[i].address;
        BYTE *site = at<BYTE *>(g_hooks[i].address);
        if (never || original_address(g_hooks[i].address) == site)
            continue;
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, g_hooks[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        write_code(g_sites[i].site, g_sites[i].bytes);
    g_nsites = 0;
}

void side_begin(int side, unsigned seed, const Module &m, BYTE pattern)
{
    const DWORD n = g_high + 0x1000 < ARENA_SIZE ? g_high + 0x1000 : ARENA_SIZE;
    memset(g_arena[side], pattern, n);
    g_filled[side] = n;
    g_pattern[side] = pattern;
    g_used[side] = 0;
    memcpy(&g_fakes, &g_template, sizeof g_fakes);
    g_vseed = seed;
    g_vindex = 0;
    g_log[side].hash = 2166136261u;
    g_log[side].calls = 0;
    g_side = side;
    if (side == 0)
        hooks_out(m);
    arena_on();
}
void side_end()
{
    arena_off();
    if (g_side == 0)
        hooks_in();
    if (g_used[g_side] > g_high)
        g_high = g_used[g_side];
    memcpy(&g_after[g_side], &g_fakes, sizeof g_fakes);
    g_side = -1;
}

// ---- Comparing ----------------------------------------------------------------------------------------------

static const char *buffer_text(DWORD p)
{
    if (p < 0x10000 || (p & 3) || !readable(at<void *>(p), 12))
        return nullptr;
    const char *t = at<const char *>(p + 8);
    for (DWORD i = 0; i < 0x400; i++) {
        if ((i & 0xff) == 0 && !readable(t + i, 0x100))
            return nullptr;
        if (!t[i])
            return t;
    }
    return nullptr;
}
bool same_value(DWORD a, DWORD b)
{
    if (a == b)
        return true;
    if (in_arena(a, 0) && in_arena(b, 1))
        return a - ptrv(g_arena[0]) == b - ptrv(g_arena[1]);
    if (on_stack(a) && on_stack(b))
        return (a & 0xff) == (b & 0xff);
    const char *x = buffer_text(a), *y = buffer_text(b);
    return x && y && lstrcmpA(x, y) == 0;
}
static DWORD g_diff_at, g_diff_a, g_diff_b;
static bool same_region(const BYTE *a, const BYTE *b, DWORD n)
{
    for (DWORD i = 0; i + 4 <= n; i += 4) {
        const DWORD x = *reinterpret_cast<const DWORD *>(a + i), y = *reinterpret_cast<const DWORD *>(b + i);
        if (!same_value(x, y)) {
            g_diff_at = i;
            g_diff_a = x;
            g_diff_b = y;
            return false;
        }
    }
    for (DWORD i = n & ~3u; i < n; i++)
        if (a[i] != b[i]) {
            g_diff_at = i;
            g_diff_a = a[i];
            g_diff_b = b[i];
            return false;
        }
    return true;
}
bool compare(Tally &t, const char *what, unsigned seed, const Result &r, DWORD eax_mask)
{
    const bool eax = same_value(r.eax[0], r.eax[1]) || ((r.eax[0] ^ r.eax[1]) & eax_mask) == 0;
    const bool st0 = !r.float_ret || r.st0[0] == r.st0[1];
    const bool logs = g_log[0].hash == g_log[1].hash && g_log[0].calls == g_log[1].calls;
    const bool fakes = same_region(reinterpret_cast<const BYTE *>(&g_after[0]), reinterpret_cast<const BYTE *>(&g_after[1]), sizeof g_after[0]);
    const DWORD fd = g_diff_at, fa = g_diff_a, fb = g_diff_b;
    const bool arenas = g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]);
    const bool same = eax && st0 && logs && fakes && arenas && !g_overflow;
    char d[240];
    wsprintfA(d, "[%s seed %x: eax %x/%x st0 %x/%x, calls %d/%d, fakes +%x %x/%x, arena %x/%x bytes +%x %x/%x%s]", what, seed, r.eax[0],
              r.eax[1], r.st0[0], r.st0[1], g_log[0].calls, g_log[1].calls, fakes ? 0 : fd, fakes ? 0 : fa, fakes ? 0 : fb, g_used[0],
              g_used[1], arenas ? 0 : g_diff_at, arenas ? 0 : g_diff_a, arenas ? 0 : g_diff_b, g_overflow ? " overflow" : "");
    if (!same && !logs && t.mismatches < 3) {
        const int n = g_log[0].calls < LOGN && g_log[1].calls < LOGN ? (g_log[0].calls > g_log[1].calls ? g_log[0].calls : g_log[1].calls) : LOGN;
        for (int i = 0; i < n; i++)
            if (i >= g_log[0].calls || i >= g_log[1].calls || memcmp(g_log[0].ev[i], g_log[1].ev[i], 32)) {
                const DWORD *x = g_log[0].ev[i], *y = g_log[1].ev[i];
                log("    call %d: %x %x %x %x %x %x / %x %x %x %x %x %x", i, x[0], x[1], x[2], x[3], x[4], x[5], y[0], y[1], y[2], y[3], y[4],
                    y[5]);
                break;
            }
    }
    g_overflow = false;
    t.check(same, "", d);
    return same;
}

// ---- Calling ------------------------------------------------------------------------------------------------

DWORD call_fn(DWORD fn, DWORD ecx, DWORD edx, int nargs, const DWORD *a, bool float_ret, DWORD *st0, bool caller_pops)
{
    DWORD r;
    if (caller_pops) {
        // cdecl: the stack arguments only
        switch (nargs) {
        case 0: r = at<DWORD(__cdecl *)()>(fn)(); break;
        case 1: r = at<DWORD(__cdecl *)(DWORD)>(fn)(a[0]); break;
        case 2: r = at<DWORD(__cdecl *)(DWORD, DWORD)>(fn)(a[0], a[1]); break;
        case 3: r = at<DWORD(__cdecl *)(DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2]); break;
        default: r = at<DWORD(__cdecl *)(DWORD, DWORD, DWORD, DWORD)>(fn)(a[0], a[1], a[2], a[3]); break;
        }
        return r;
    }
    if (float_ret) {
        long double v;
        switch (nargs) {
        case 0: v = at<long double(__fastcall *)(DWORD, DWORD)>(fn)(ecx, edx); break;
        case 1: v = at<long double(__fastcall *)(DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0]); break;
        case 2: v = at<long double(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1]); break;
        default: v = at<long double(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1], a[2]); break;
        }
        BYTE raw[10];
        memcpy(raw, &v, 10);
        *st0 = fnv(2166136261u, raw, 10);   // all 80 bits
        return 0;
    }
    // ecx and edx in registers, the rest on the stack, popped by the callee (thiscall, stdcall, fastcall)
    switch (nargs) {
    case 0: r = at<DWORD(__fastcall *)(DWORD, DWORD)>(fn)(ecx, edx); break;
    case 1: r = at<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0]); break;
    case 2: r = at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1]); break;
    case 3: r = at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1], a[2]); break;
    case 4: r = at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1], a[2], a[3]); break;
    case 5: r = at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1], a[2], a[3], a[4]); break;
    default:
        r = at<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]);
        break;
    }
    return r;
}

}  // namespace zax::sndc
