// Checks of the C runtime's structured exception helpers (crt_seh.cpp) and the C functions with a __try frame
// (crt_sehc.cpp) against the exe's code, in the game thread (docs/DECOMPILATION.md 10, "Phase 5 leftovers").
// The exe is built without C++ exceptions (no __CxxFrameHandler, no throw), so the cases are SEH frames:
//  - crt_seh (and crt_thunks: RtlUnwind's thunk, the virtual call thunk 0x4020c0): side 0 runs the original code with
//    the modules' hooks out (the helpers call each other and
//    _abnormal_termination recognises __local_unwind2's frame by the address of its own unwind handler, so
//    the two sides must not be mixed), side 1 the reimplementation;
//    - real exceptions: nested VC6 frames (try level at [ebp-4], scope table, the saved esp at [ebp-0x18],
//      the exception pointers at [ebp-0x14]) with random scope tables of __except and __finally entries,
//      handled by _except_handler3; RaiseException (filters may continue the execution) or a read of
//      address 0; an outermost frame whose filter takes everything, so nothing leaves the check. Logged: every
//      filter call (frame, entry, code, flags, try level), every __finally (with _abnormal_termination), the
//      __except block reached and the try level it sees, the normal ends; the debugger block of __NLG_Notify;
//    - direct calls on fake frames: __local_unwind2 to random stop levels, _except_handler3's search pass
//      (filters answering only "continue search" or "continue execution": "execute" would unwind a frame that
//      is not on the chain) and its unwind pass, __unwind_handler, _abnormal_termination under a fake top
//      frame (the side's own unwind handler, see above), __NLG_Notify/__NLG_Notify1, _seh_longjmp_unwind,
//      __CallSettingFrame@12 with a recording funclet;
//  - crt_sehc, through the trampolines: terminate with a recording terminate handler in the per-thread data
//    (none, returning, raising, faulting) and abort replaced by an escape back into the check; abort itself
//    (crt_startup) with a recording message box, a recording or ignoring SIGABRT action and _exit replaced;
//    __crtLCMapStringA, __crtGetStringTypeA, __crtCompareStringA on random texts, locales, code pages and
//    buffer sizes, each from the same "which API" state (0x715984, 0x715bac, 0x715c5c: unknown, wide, ANSI).
// WinMainCRTStartup runs once; the game starts on it (the logged start against /Original).
#include "check_util.h"
#include "checks_ui_common.h"

WINIMPORT void WINAPI RaiseException(DWORD code, DWORD flags, DWORD n, const DWORD *args);

extern "C" {
DWORD g_seh_scope[4];   // per frame depth: its scope table
DWORD g_seh_handler;    // the frames' handler
DWORD g_seh_level[4];   // the try level each frame is at when the next is called
DWORD g_seh_depths;
DWORD g_seh_jb_esp, g_seh_jb_fs;
DWORD __cdecl seh_frame(DWORD depth);
DWORD __cdecl seh_guarded(DWORD fn, DWORD arg);
DWORD __cdecl seh_call2_ebp(DWORD fn, DWORD a, DWORD b, DWORD ebp);
DWORD __cdecl seh_abnormal(DWORD fn, DWORD fake_top);
void __cdecl seh_nlg(DWORD fn, DWORD eax, DWORD ecx, DWORD ebp, DWORD arg);
void seh_escape();
void seh_except_body();
void seh_filter_0();
void seh_filter_1();
void seh_filter_2();
void seh_filter_3();
void seh_filter_all();
void seh_finally_0();
void seh_finally_1();
void seh_finally_2();
void seh_finally_3();
void seh_funclet();
DWORD __cdecl seh_filter(DWORD k, DWORD ebp);
void __cdecl seh_finally(DWORD k, DWORD ebp);
void __cdecl seh_raise();
void __cdecl seh_end(DWORD depth, DWORD level);
void __cdecl seh_except(DWORD depth, DWORD level);
void zax___unwind_handler();
void zax_terminate();
}

__asm__(R"(
    .intel_syntax noprefix
    .text

# A VC6 __try frame (depth): the next depth inside it, the innermost raises; its __except block below
    .globl _seh_frame
_seh_frame:
    push ebp
    mov ebp, esp
    push -1
    mov eax, dword ptr [ebp + 8]
    push dword ptr [_g_seh_scope + eax*4]
    push dword ptr [_g_seh_handler]
    push dword ptr fs:[0]
    mov dword ptr fs:[0], esp
    sub esp, 0xc
    push ebx
    push esi
    push edi
    mov dword ptr [ebp - 0x18], esp
    mov eax, dword ptr [ebp + 8]
    mov dword ptr [ebp - 0x1c], eax
    mov ecx, dword ptr [_g_seh_level + eax*4]
    mov dword ptr [ebp - 4], ecx
    inc eax
    cmp eax, dword ptr [_g_seh_depths]
    jae Lseh_innermost
    push eax
    call _seh_frame
    add esp, 4
    jmp Lseh_done
Lseh_innermost:
    call _seh_raise
Lseh_done:
    push dword ptr [ebp - 4]
    push dword ptr [ebp - 0x1c]
    call _seh_end
    add esp, 8
    jmp Lseh_leave
    .globl _seh_except_body
_seh_except_body:
    mov esp, dword ptr [ebp - 0x18]
    push dword ptr [ebp - 4]
    push dword ptr [ebp - 0x1c]
    call _seh_except
    add esp, 8
Lseh_leave:
    mov ecx, dword ptr [ebp - 0x10]
    mov dword ptr fs:[0], ecx
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

# Filters and __finally blocks of the scope tables (called with ebp = the frame's)
    .globl _seh_filter_0
_seh_filter_0:
    push ebp
    push 0
    call _seh_filter
    add esp, 8
    ret
    .globl _seh_filter_1
_seh_filter_1:
    push ebp
    push 1
    call _seh_filter
    add esp, 8
    ret
    .globl _seh_filter_2
_seh_filter_2:
    push ebp
    push 2
    call _seh_filter
    add esp, 8
    ret
    .globl _seh_filter_3
_seh_filter_3:
    push ebp
    push 3
    call _seh_filter
    add esp, 8
    ret
    .globl _seh_filter_all
_seh_filter_all:
    push ebp
    push 99
    call _seh_filter
    add esp, 8
    ret
    .globl _seh_funclet
_seh_funclet:
    push ebp
    push 50
    call _seh_filter
    add esp, 8
    ret
    .globl _seh_finally_0
_seh_finally_0:
    push ebp
    push 0
    call _seh_finally
    add esp, 8
    ret
    .globl _seh_finally_1
_seh_finally_1:
    push ebp
    push 1
    call _seh_finally
    add esp, 8
    ret
    .globl _seh_finally_2
_seh_finally_2:
    push ebp
    push 2
    call _seh_finally
    add esp, 8
    ret
    .globl _seh_finally_3
_seh_finally_3:
    push ebp
    push 3
    call _seh_finally
    add esp, 8
    ret

# fn(a, b) with ebp set (as _except_handler3 calls __local_unwind2)
    .globl _seh_call2_ebp
_seh_call2_ebp:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, dword ptr [esp + 0x14]
    mov ecx, dword ptr [esp + 0x18]
    mov edx, dword ptr [esp + 0x1c]
    mov ebp, dword ptr [esp + 0x20]
    push edx
    push ecx
    call eax
    add esp, 8
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# fn() with a fake frame on top of fs:[0]
    .globl _seh_abnormal
_seh_abnormal:
    mov ecx, dword ptr [esp + 8]
    push dword ptr fs:[0]
    mov dword ptr fs:[0], ecx
    call dword ptr [esp + 8]
    pop ecx
    mov dword ptr fs:[0], ecx
    ret

# fn(arg) with eax, ecx and ebp set (__NLG_Notify, __NLG_Notify1: ret 4)
    .globl _seh_nlg
_seh_nlg:
    push ebp
    push ebx
    mov edx, dword ptr [esp + 0xc]
    mov eax, dword ptr [esp + 0x10]
    mov ecx, dword ptr [esp + 0x14]
    mov ebp, dword ptr [esp + 0x18]
    push dword ptr [esp + 0x1c]
    call edx
    pop ebx
    pop ebp
    ret

# fn(arg) with a way back: seh_escape (abort's stand-in) returns 0xa5a5a5a5 from here with fs:[0] put back
    .globl _seh_guarded
_seh_guarded:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, dword ptr fs:[0]
    mov dword ptr [_g_seh_jb_fs], eax
    mov dword ptr [_g_seh_jb_esp], esp
    push dword ptr [esp + 0x18]
    call dword ptr [esp + 0x18]
    add esp, 4
Lseh_resume:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
    .globl _seh_escape
_seh_escape:
    mov esp, dword ptr [_g_seh_jb_esp]
    mov eax, dword ptr [_g_seh_jb_fs]
    mov dword ptr fs:[0], eax
    mov eax, 0xa5a5a5a5
    jmp Lseh_resume
    .att_syntax
)");

namespace zax {
using uic::ptrv;

int hooks_crt_seh(Hook *out);
int hooks_crt_thunks(Hook *out);

namespace {

constexpr DWORD EXCEPT_HANDLER3 = 0x5d2304, LOCAL_UNWIND2 = 0x5d224e, UNWIND_HANDLER = 0x5d222c,
                ABNORMAL = 0x5d22b6, NLG_NOTIFY1 = 0x5d22d9, NLG_NOTIFY = 0x5d22e2, LONGJMP_UNWIND = 0x5d23c1,
                CALL_SETTING_FRAME = 0x5db470, TERMINATE = 0x5db4bc, ABORT = 0x5dd087, GETPTD = 0x5d4f28,
                LCMAP = 0x5d437b, GETSTRINGTYPE = 0x5db59e, COMPARESTRING = 0x5ddd54, NLG_DATA = 0x6297b0;
constexpr DWORD VCALL_THUNK = 0x4020c0;
constexpr DWORD API_STATE[3] = {0x715984, 0x715bac, 0x715c5c};

constinit uic::ModuleHooks g_hooks{"crt_seh", &hooks_crt_seh};
constinit uic::ModuleHooks g_thunk_hooks{"crt_thunks", &hooks_crt_thunks};   // RtlUnwind's thunk, the virtual call thunk

unsigned g_seed;
DWORD rnd()
{
    g_seed = g_seed * 1103515245u + 12345u;
    return (g_seed >> 8) ^ (g_seed << 13);
}
DWORD below(DWORD n) { return n ? rnd() % n : 0; }

// The side's log of events
struct Events {
    unsigned hash;
    int n;
    DWORD first[12][4];
    void add(DWORD a, DWORD b, DWORD c, DWORD d)
    {
        const DWORD e[4] = {a, b, c, d};
        hash = fnv(hash, e, sizeof e);
        if (n < 12)
            memcpy(first[n], e, sizeof e);
        n++;
    }
};
Events g_ev;
DWORD g_answer[4][4];   // the filters' answers per depth and entry
DWORD g_mode;           // 0 no exception, 1 RaiseException, 2 a read of address 0
DWORD g_code;
DWORD g_rec, g_ctx;     // the record and context of a direct _except_handler3 call
DWORD g_ebp_base;       // ebp values are logged relative to it
DWORD g_funclet_ret;
DWORD g_nlg_target;     // with terminate: the original's address (its __except block is the notified target)
void *g_nlg_mine;       // and the reimplementation's

// The scope tables of the real frames and the direct cases: {enclosing level, filter or 0, handler}
DWORD g_tables[4][4][3];
DWORD g_guard[3];

}  // namespace

extern "C" DWORD __cdecl seh_filter(DWORD k, DWORD ebp)
{
    if (k == 50) {
        g_ev.add(5, ebp - g_ebp_base, 0, 0);
        return g_funclet_ret;
    }
    const DWORD depth = *at<DWORD *>(ebp - 0x1c);
    const DWORD *xp = *at<const DWORD **>(ebp - 0x14);
    DWORD code = 0, flags = 0, same = 0;
    if (xp) {
        const DWORD *rec = at<const DWORD *>(xp[0]);
        code = rec[0], flags = rec[1];
        same = (xp[0] == g_rec ? 1u : 0u) | (xp[1] == g_ctx ? 2u : 0u);
    }
    g_ev.add(1, depth << 8 | k, code, flags ^ same << 16 ^ *at<DWORD *>(ebp - 4) << 20);
    if (k == 99)
        return 1;
    return g_answer[depth & 3][k & 3];
}
extern "C" void __cdecl seh_finally(DWORD k, DWORD ebp)
{
    const DWORD depth = *at<DWORD *>(ebp - 0x1c);
    const DWORD abnormal = at<DWORD(__cdecl *)()>(ABNORMAL)();
    g_ev.add(2, depth << 8 | k, abnormal, *at<DWORD *>(ebp - 4));
}
extern "C" void __cdecl seh_raise()
{
    g_ev.add(6, g_mode, 0, 0);
    if (g_mode == 1) {
        const DWORD args[2] = {0x1234, g_code};
        RaiseException(g_code, 0, 2, args);
    } else if (g_mode == 2) {
        __asm__ volatile("movl 0, %%eax" ::: "eax", "memory");
    }
    g_ev.add(7, 0, 0, 0);
}
extern "C" void __cdecl seh_end(DWORD depth, DWORD level) { g_ev.add(3, depth, level, 0); }
extern "C" void __cdecl seh_except(DWORD depth, DWORD level) { g_ev.add(4, depth, level, 0); }

namespace {

const DWORD FILTERS[4] = {ptrv((void *)&seh_filter_0), ptrv((void *)&seh_filter_1), ptrv((void *)&seh_filter_2),
                          ptrv((void *)&seh_filter_3)};
const DWORD FINALLYS[4] = {ptrv((void *)&seh_finally_0), ptrv((void *)&seh_finally_1), ptrv((void *)&seh_finally_2),
                           ptrv((void *)&seh_finally_3)};

// A random scope table of n entries (each enclosed by an earlier one or by none) into t; __finally entries
// with probability 1/f
void random_table(DWORD t[4][3], int n, DWORD f)
{
    for (int i = 0; i < 4; i++) {
        t[i][0] = i == 0 || below(4) == 0 ? 0xffffffff : below((DWORD)i);
        const bool fin = below(f) == 0;
        t[i][1] = fin ? 0 : FILTERS[i];
        t[i][2] = fin ? FINALLYS[i] : ptrv((void *)&seh_except_body);
        if (i >= n)
            t[i][0] = 0xffffffff;
    }
}
DWORD random_level(int n) { return below(5) == 0 ? 0xffffffff : below((DWORD)n); }

struct Side {
    Events ev;
    BYTE nlg[16];
    DWORD result;
    DWORD frame[12];
    DWORD fs0;
};

constexpr DWORD UNWIND_FLAGS[4] = {2, 4, 6, 3};
inline DWORD at_dw(const BYTE *p, int o)
{
    DWORD v;
    memcpy(&v, p + o, 4);
    return v;
}

DWORD fs0()
{
    DWORD v;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(v));
    return v;
}

// One case on both sides: run() is called with side 0 (the module's hooks out, the original) and 1
template <typename Run> void two_sides(Tally &t, const char *what, unsigned seed, bool hooks_out, Run run)
{
    Side s[2] = {};
    BYTE nlg_saved[16];
    memcpy(nlg_saved, at<void *>(NLG_DATA), 16);
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        memset(&g_ev, 0, sizeof g_ev);
        g_ev.hash = 2166136261u;
        memcpy(at<void *>(NLG_DATA), nlg_saved, 16);
        if (side == 0 && hooks_out)
            g_hooks.out(), g_thunk_hooks.out();
        const DWORD before = fs0();
        s[side].result = run(side, s[side].frame);
        s[side].fs0 = fs0() ^ before;
        if (side == 0 && hooks_out)
            g_thunk_hooks.in(), g_hooks.in();
        s[side].ev = g_ev;
        memcpy(s[side].nlg, at<void *>(NLG_DATA), 16);
        if (g_nlg_target) {   // the target __NLG_Notify records is the side's own code: compared as "the function's"
            DWORD v;
            memcpy(&v, s[side].nlg + 4, 4);
            const DWORD start = side == 0 ? g_nlg_target : ptrv(g_nlg_mine);
            v = v >= start && v < start + 0x100 ? 1 : v;
            memcpy(s[side].nlg + 4, &v, 4);
        }
    }
    memcpy(at<void *>(NLG_DATA), nlg_saved, 16);
    const bool same = s[0].result == s[1].result && s[0].ev.hash == s[1].ev.hash && s[0].ev.n == s[1].ev.n &&
                      !memcmp(s[0].nlg, s[1].nlg, 16) && !memcmp(s[0].frame, s[1].frame, sizeof s[0].frame) &&
                      s[0].fs0 == s[1].fs0;
    char d[200];
    wsprintfA(d, "[%s seed %u: result %x/%x, events %d/%d, fs %x/%x]", what, seed, s[0].result, s[1].result, s[0].ev.n,
              s[1].ev.n, s[0].fs0, s[1].fs0);
    t.check(same, "", d);
    if (!same) {
        for (int i = 0; i < 12 && (i < s[0].ev.n || i < s[1].ev.n); i++)
            log("    event %d: %x %x %x %x / %x %x %x %x", i, s[0].ev.first[i][0], s[0].ev.first[i][1], s[0].ev.first[i][2],
                s[0].ev.first[i][3], s[1].ev.first[i][0], s[1].ev.first[i][1], s[1].ev.first[i][2], s[1].ev.first[i][3]);
        log("    nlg %x %x %x %x / %x %x %x %x", at_dw(s[0].nlg, 0), at_dw(s[0].nlg, 4), at_dw(s[0].nlg, 8), at_dw(s[0].nlg, 12),
            at_dw(s[1].nlg, 0), at_dw(s[1].nlg, 4), at_dw(s[1].nlg, 8), at_dw(s[1].nlg, 12));
    }
}

// ---- Real exceptions through nested frames ------------------------------------------------------------------------

void check_real(Tally &t, int n)
{
    for (int c = 0; c < n; c++) {
        const unsigned seed = 0x5e4000u + (unsigned)c;
        two_sides(t, "frames", seed, true, [](int, DWORD *) -> DWORD {
            g_seh_handler = EXCEPT_HANDLER3;
            g_guard[0] = 0xffffffff, g_guard[1] = ptrv((void *)&seh_filter_all), g_guard[2] = ptrv((void *)&seh_except_body);
            g_seh_scope[0] = ptrv(g_guard), g_seh_level[0] = 0;
            g_seh_depths = 2 + below(3);
            g_mode = below(6) == 0 ? 0 : below(4) == 0 ? 2 : 1;
            const DWORD codes[4] = {0xe0001234, 0xe0004321, 0xc0001000, 0x20000001};
            g_code = codes[below(4)];
            for (DWORD d = 1; d < 4; d++) {
                const int entries = 1 + (int)below(4);
                random_table(g_tables[d], entries, 3);
                g_seh_scope[d] = ptrv(g_tables[d]);
                g_seh_level[d] = random_level(entries);
                for (int k = 0; k < 4; k++) {
                    const DWORD r = below(8);
                    g_answer[d][k] = r < 4 ? 0 : r < 6 || g_mode != 1 ? 1 : 0xffffffff;
                }
            }
            g_rec = g_ctx = 0;
            return seh_frame(0);
        });
    }
}

DWORD THISCALL vcall_target(DWORD *self, DWORD a, DWORD b)
{
    g_ev.add(14, self[1], a, b);
    return a ^ b ^ 0x5a5a;
}

// ---- Direct calls on fake frames ----------------------------------------------------------------------------------

// A fake frame: [0] the depth ([ebp-0x1c]), [1] saved esp, [2] exception pointers, [3] the registration (next,
// handler, scope table, try level), [7] where ebp points, [8]/[9] the frame's arguments
DWORD g_fake[12];
inline DWORD fake_reg() { return ptrv(&g_fake[3]); }
inline DWORD fake_ebp() { return ptrv(&g_fake[7]); }
void make_fake(int entries, DWORD finally_one_in)
{
    memset(g_fake, 0, sizeof g_fake);
    g_fake[0] = 1;
    g_fake[3] = 0xffffffff;
    g_fake[4] = EXCEPT_HANDLER3;
    random_table(g_tables[1], entries, finally_one_in);
    g_fake[5] = ptrv(g_tables[1]);
    g_fake[6] = random_level(entries);
    g_fake[8] = rnd(), g_fake[9] = rnd();
}

void check_direct(Tally &t, int n)
{
    g_ebp_base = fake_ebp();
    for (int c = 0; c < n; c++) {
        const unsigned seed = 0x5e5000u + (unsigned)c;
        const DWORD kind = (DWORD)c % 9;
        two_sides(t, "direct", seed, true, [kind](int side, DWORD *frame) -> DWORD {
            DWORD r = 0;
            make_fake(1 + (int)below(4), 2);
            switch (kind) {
            case 0:
            case 1: {   // __local_unwind2 to a stop level
                const DWORD stop = below(3) == 0 ? 0xffffffff : below(5);
                r = seh_call2_ebp(LOCAL_UNWIND2, fake_reg(), stop, fake_ebp());
                r = 0;   // (eax is what the last __finally left)
                break;
            }
            case 2:
            case 3: {   // _except_handler3: the search pass (no filter executes) or an unwind
                DWORD rec[20] = {};
                rec[0] = 0xe0000000u | below(0x1000), rec[1] = kind == 3 ? UNWIND_FLAGS[below(4)] : below(2);
                DWORD ctx[4] = {rnd(), rnd(), 0, 0};
                g_rec = ptrv(rec), g_ctx = ptrv(ctx);
                for (int k = 0; k < 4; k++)
                    g_answer[1][k] = below(3) == 0 ? 0xffffffff : 0;
                DWORD disp = 0;
                using H = DWORD(__cdecl *)(void *, DWORD, void *, void *);
                r = at<H>(EXCEPT_HANDLER3)(rec, fake_reg(), ctx, &disp);
                g_fake[2] = 0;   // (the pointers lie in the handler's stack frame; the filters log what they point to)
                r ^= disp << 8;
                g_rec = g_ctx = 0;
                break;
            }
            case 4: {   // __unwind_handler
                DWORD rec[8] = {};
                rec[1] = below(8);
                DWORD disp = 0;
                using H = DWORD(__cdecl *)(void *, DWORD, DWORD, DWORD *);
                r = at<H>(UNWIND_HANDLER)(rec, fake_reg(), rnd(), &disp);
                r ^= (disp == fake_reg() ? 1u : disp ? 2u : 0u) << 8;
                break;
            }
            case 5: {   // _abnormal_termination under a fake top frame (the side's own unwind handler or another)
                DWORD top[4];
                const DWORD own = side == 0 ? UNWIND_HANDLER : ptrv((void *)&zax___unwind_handler);
                top[0] = 0xffffffff;
                top[1] = below(4) == 0 ? EXCEPT_HANDLER3 : own;
                top[3] = fake_reg();
                top[2] = below(2) ? g_fake[6] : rnd();
                r = seh_abnormal(ABNORMAL, ptrv(top));
                break;
            }
            case 6: {   // __NLG_Notify / __NLG_Notify1 / _seh_longjmp_unwind
                const DWORD w = below(3);
                if (w < 2) {
                    seh_nlg(w ? NLG_NOTIFY1 : NLG_NOTIFY, rnd(), rnd(), fake_ebp(), rnd());
                } else {
                    DWORD jb[8] = {fake_ebp(), rnd(), rnd(), rnd(), rnd(), rnd(), fake_reg(), below(3) == 0 ? 0xffffffff : below(5)};
                    at<void(__stdcall *)(DWORD *)>(LONGJMP_UNWIND)(jb);
                }
                break;
            }
            case 7: {   // the virtual call thunk 0x4020c0 (jmp [[ecx]+0x130]) into a recording slot
                static DWORD vt[80];
                vt[0x130 / 4] = ptrv((void *)&vcall_target);
                DWORD obj[2] = {ptrv(vt), rnd()};
                using F = DWORD(THISCALL *)(DWORD *, DWORD, DWORD);
                r = at<F>(VCALL_THUNK)(obj, rnd(), rnd());
                if (below(2))
                    break;
                [[fallthrough]];
            }
            default: {   // __CallSettingFrame@12 with a recording funclet
                g_funclet_ret = rnd();
                const DWORD codes[3] = {0x100, 1, 2};
                using F = DWORD(__stdcall *)(DWORD, DWORD, DWORD);
                r = at<F>(CALL_SETTING_FRAME)(ptrv((void *)&seh_funclet), fake_ebp() - 0xc, codes[below(3)]);
                break;
            }
            }
            memcpy(frame, g_fake, sizeof g_fake);
            return r;
        });
    }
}

// ---- terminate ------------------------------------------------------------------------------------------------------

DWORD g_term_mode;
void __cdecl term_handler()
{
    g_ev.add(8, g_term_mode, 0, 0);
    if (g_term_mode == 2) {
        const DWORD args[1] = {7};
        RaiseException(0xe0007777, 0, 1, args);
    } else if (g_term_mode == 3) {
        __asm__ volatile("movl 0, %%eax" ::: "eax", "memory");
    }
    g_ev.add(9, 0, 0, 0);
}

void check_terminate(Tally &t)
{
    BYTE *ptd = at<BYTE *(__cdecl *)()>(GETPTD)();
    const DWORD saved_handler = *reinterpret_cast<DWORD *>(ptd + 0x60);
    BYTE abort_bytes[5];
    memcpy(abort_bytes, at<void *>(ABORT), 5);
    BYTE jump[5] = {0xe9};
    const DWORD rel = ptrv((void *)&seh_escape) - (ABORT + 5);
    memcpy(jump + 1, &rel, 4);
    g_nlg_target = TERMINATE, g_nlg_mine = (void *)&zax_terminate;
    for (DWORD mode = 0; mode < 4; mode++) {
        two_sides(t, "terminate", 0x5e6000u + mode, false, [&](int side, DWORD *) -> DWORD {
            g_term_mode = mode;
            *reinterpret_cast<DWORD *>(ptd + 0x60) = mode ? ptrv((void *)&term_handler) : 0;
            write_code(at<void *>(ABORT), jump);
            const DWORD r = seh_guarded(side == 0 ? ptrv(original_address(TERMINATE)) : TERMINATE, 0);
            write_code(at<void *>(ABORT), abort_bytes);
            *reinterpret_cast<DWORD *>(ptd + 0x60) = saved_handler;
            g_ev.add(10, r, 0, 0);
            return r;
        });
    }
    g_nlg_target = 0;
}

// ---- abort --------------------------------------------------------------------------------------------------------

// abort: "abnormal program termination" (_NMSG_WRITE 10) through a recording message box (the cached MessageBoxA
// pointer of __crtMessageBoxA), raise(SIGABRT) with a recording handler in the global action (0x715c34) or the
// action "ignore", then _exit(3), replaced by the escape
constexpr DWORD MSGBOX = 0x715c1c, MSGBOX_ACTIVE = 0x715c20, MSGBOX_POPUP = 0x715c24, ABORT_ACTION = 0x715c34, EXIT_ = 0x5d0e47;
int WINAPI fake_message_box(DWORD, const char *text, const char *title, DWORD type)
{
    g_ev.add(11, fnv(2166136261u, text, (DWORD)lstrlenA(text)), fnv(2166136261u, title, (DWORD)lstrlenA(title)), type);
    return 1;
}
void __cdecl abort_handler(int sig) { g_ev.add(12, (DWORD)sig, *at<DWORD *>(ABORT_ACTION), 0); }

void check_abort(Tally &t)
{
    const DWORD saved[4] = {*at<DWORD *>(MSGBOX), *at<DWORD *>(MSGBOX_ACTIVE), *at<DWORD *>(MSGBOX_POPUP), *at<DWORD *>(ABORT_ACTION)};
    BYTE exit_bytes[5];
    memcpy(exit_bytes, at<void *>(EXIT_), 5);
    BYTE jump[5] = {0xe9};
    const DWORD rel = ptrv((void *)&seh_escape) - (EXIT_ + 5);
    memcpy(jump + 1, &rel, 4);
    for (DWORD mode = 0; mode < 2; mode++) {
        two_sides(t, "abort", 0x5e6100u + mode, false, [&](int side, DWORD *) -> DWORD {
            *at<void **>(MSGBOX) = (void *)&fake_message_box;
            *at<DWORD *>(MSGBOX_ACTIVE) = 0, *at<DWORD *>(MSGBOX_POPUP) = 0;
            *at<DWORD *>(ABORT_ACTION) = mode ? 1 : ptrv((void *)&abort_handler);   // 1: SIG_IGN
            write_code(at<void *>(EXIT_), jump);
            const DWORD r = seh_guarded(side == 0 ? ptrv(original_address(ABORT)) : ABORT, 0);
            write_code(at<void *>(EXIT_), exit_bytes);
            g_ev.add(13, r, *at<DWORD *>(ABORT_ACTION) == ptrv((void *)&abort_handler) ? 2 : *at<DWORD *>(ABORT_ACTION), 0);
            *at<DWORD *>(MSGBOX) = saved[0], *at<DWORD *>(MSGBOX_ACTIVE) = saved[1], *at<DWORD *>(MSGBOX_POPUP) = saved[2];
            *at<DWORD *>(ABORT_ACTION) = saved[3];
            return r;
        });
    }
}

// ---- The locale string functions ----------------------------------------------------------------------------------

void random_text(char *s, int max)
{
    const int n = (int)below((DWORD)max);
    for (int i = 0; i < n; i++) {
        const DWORD k = below(10);
        s[i] = (char)(k < 5 ? 'a' + below(26) : k < 7 ? 'A' + below(26) : k < 8 ? ' ' + below(32) : 0x80 + below(0x80));
    }
    s[n] = 0;
}

void check_strings(Tally &t, int n)
{
    DWORD state[3];
    for (int i = 0; i < 3; i++)
        state[i] = *at<DWORD *>(API_STATE[i]);
    const DWORD LCIDS[5] = {0x409, 0x407, 0x40c, 0x400, 0};
    const DWORD PAGES[5] = {0, 1252, 850, 437, 1250};
    for (int c = 0; c < n; c++) {
        const DWORD fn = (DWORD)c % 3;
        two_sides(t, fn == 0 ? "__crtLCMapStringA" : fn == 1 ? "__crtGetStringTypeA" : "__crtCompareStringA", 0x5e7000u + (unsigned)c, false,
                  [&](int side, DWORD *frame) -> DWORD {
                      const DWORD use = below(3);
                      for (int i = 0; i < 3; i++)
                          *at<DWORD *>(API_STATE[i]) = use;
                      char a[48], b[48];
                      random_text(a, 40), random_text(b, 40);
                      if (below(4) == 0)
                          memcpy(b, a, sizeof a);
                      const DWORD lcid = LCIDS[below(5)], page = PAGES[below(5)];
                      const int ca = below(3) == 0 ? -1 : (int)below(44), cb = below(3) == 0 ? -1 : (int)below(44);
                      BYTE out[128];
                      memset(out, 0xcd, sizeof out);
                      DWORD r;
                      if (fn == 0) {
                          const DWORD FLAGS[5] = {0x100, 0x200, 0x400, 0x100 | 0x1, 0};
                          const int cd = below(3) == 0 ? 0 : (int)below(128);
                          using F = int(__cdecl *)(DWORD, DWORD, const char *, int, BYTE *, int, DWORD, DWORD);
                          const DWORD f = side == 0 ? ptrv(original_address(LCMAP)) : LCMAP;
                          r = (DWORD)at<F>(f)(lcid, FLAGS[below(5)], a, ca, out, cd, page, below(2));
                      } else if (fn == 1) {
                          const DWORD TYPES[3] = {1, 2, 4};
                          using F = int(__cdecl *)(DWORD, const char *, int, BYTE *, DWORD, DWORD, DWORD);
                          const DWORD f = side == 0 ? ptrv(original_address(GETSTRINGTYPE)) : GETSTRINGTYPE;
                          r = (DWORD)at<F>(f)(TYPES[below(3)], a, ca, out, page, lcid, below(2));
                      } else {
                          using F = int(__cdecl *)(DWORD, DWORD, const char *, int, const char *, int, DWORD);
                          const DWORD f = side == 0 ? ptrv(original_address(COMPARESTRING)) : COMPARESTRING;
                          r = (DWORD)at<F>(f)(lcid, below(3) == 0 ? 1 : 0, a, ca, b, cb, page);
                      }
                      frame[0] = fnv(2166136261u, out, sizeof out);
                      for (int i = 0; i < 3; i++)
                          frame[1 + i] = *at<DWORD *>(API_STATE[i]);
                      return r;
                  });
    }
    for (int i = 0; i < 3; i++)
        *at<DWORD *>(API_STATE[i]) = state[i];
}

}  // namespace

ZAX_CHECKS(crt_seh)
{
    (void)cls;
    (void)n;
    Tally a{"crt_seh: _except_handler3, __global_unwind2, __local_unwind2, the unwind handler, _abnormal_termination, __NLG_Notify on nested frames with real exceptions"};
    check_real(a, scaled(1500));
    a.report();
    Tally b{"crt_seh: __local_unwind2, _except_handler3, __unwind_handler, _abnormal_termination, __NLG_Notify(1), _seh_longjmp_unwind, __CallSettingFrame@12 on fake frames, crt_thunks' virtual call thunk"};
    check_direct(b, scaled(4000));
    b.report();
    Tally c{"crt_sehc: terminate (no handler, returning, raising, faulting; abort replaced), abort (crt_startup; _exit replaced), __crtLCMapStringA, __crtGetStringTypeA, __crtCompareStringA"};
    check_terminate(c);
    check_abort(c);
    check_strings(c, scaled(3000));
    c.report();
}

}  // namespace zax
