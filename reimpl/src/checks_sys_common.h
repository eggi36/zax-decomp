// Shared machinery of the phase 5a system layer checks (checks_display_ddraw.cpp, checks_app_window.cpp,
// checks_app_start.cpp; docs/DECOMPILATION.md 10, batch P5a-1), on top of the window toolkit's harness
// (checks_ui_common.h: the block of fakes, recording vtables and exe stand-ins, the two sides of a case):
// - recording stand-ins for the Windows imports: the exe's import table entries point to functions of the check for the
//   duration of a side; every call goes into the side's log (log_event) with its arguments as both sides can compare
//   them (a pointer to the caller's stack by the bytes it points to, a text by its hash), and the answers come from the
//   case's sequence (g_case.values), so original and reimplementation see the same answers as long as they make the
//   same calls. Calls from the game's other threads go to the real function.
// - exit (0x5d0e36) leaves the side like longjmp (zax_call_escapable / zax_escape_exit of check_util.h); the exit code
//   goes into the log.
#pragma once
#include "checks_ui_common.h"

WINIMPORT DWORD WINAPI GetCurrentThreadId(void);

namespace zax::sysc {

using namespace uic;

struct Imp {
    DWORD iat;
    void *fn;
};
constexpr int MAXIMP = 48;
inline ImportPatch g_imp[MAXIMP];
inline DWORD g_imp_iat[MAXIMP];
inline int g_nimp;
inline DWORD g_tid;

inline void imports_on(const Imp *list, int n)
{
    g_tid = GetCurrentThreadId();
    g_nimp = 0;
    for (int i = 0; i < n && i < MAXIMP; i++) {
        patch_import(g_imp[g_nimp], list[i].iat, list[i].fn);
        g_imp_iat[g_nimp++] = list[i].iat;
    }
}
inline void imports_off()
{
    for (int i = g_nimp - 1; i >= 0; i--)
        unpatch_import(g_imp[i]);
    g_nimp = 0;
}
// The real function behind a patched import (for the game's other threads)
template <typename F> inline F real(DWORD iat)
{
    for (int i = 0; i < g_nimp; i++)
        if (g_imp_iat[i] == iat)
            return reinterpret_cast<F>(static_cast<uintptr_t>(g_imp[i].saved));
    return *at<F *>(iat);
}
inline bool foreign() { return GetCurrentThreadId() != g_tid; }

// An argument as both sides see it: raw, unless it points into the stack (the two sides' frames differ): then the n
// bytes it points to
inline DWORD g_stack_lo, g_stack_hi;
inline void note_stack()
{
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(g_stack_hi), "=r"(g_stack_lo));
}
inline DWORD canon(DWORD v, DWORD n = 4)
{
    if (v >= g_stack_lo && v < g_stack_hi)
        return 0x5ac00000u ^ fnv(2166136261u, at<void *>(v), n);
    return v;
}
inline DWORD text_hash(const char *s) { return s ? fnv(2166136261u, s, (unsigned)lstrlenA(s)) : 0xdead; }
// One recorded call: its id and up to four values
inline void rec(DWORD id, DWORD a = 0, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD v[4] = {a, b, c, d};
    log_event(id, fnv(2166136261u, v, sizeof v), a);
}
// The answers
inline DWORD ans() { return g_case.values.next(); }
inline DWORD ans_below(DWORD n) { return g_case.values.below(n); }
inline bool ans_chance(DWORD one_in) { return g_case.values.chance(one_in); }

// ---- Running a function so that exit leaves it ------------------------------------------------------------------------

inline DWORD (*g_escapable)();
inline DWORD THISCALL escapable_target(void *, const char *, int) { return g_escapable(); }
// Runs f with exit (0x5d0e36) leading out of it: the exit code is logged and 0x7fff0001 returned
inline DWORD run_escapable(DWORD (*f)())
{
    CodePatch exit_patch;
    patch_jump(exit_patch, 0x5d0e36, (void *)&zax_escape_exit);
    g_escapable = f;
    zax_exit_code = 0xe0e0e0e0;
    const DWORD r = (DWORD)zax_call_escapable((void *)&escapable_target, nullptr, nullptr, 0);
    unpatch(exit_patch);
    if (r == 0x7fff0001)
        log_event(0xe417, zax_exit_code, 0);
    return r;
}

}  // namespace zax::sysc
