// Shared helpers of the sequences batch's direct checks (checks_random_misc.cpp,
// checks_sequence_player.cpp, the scene list in checks_sequences.cpp): a module's hooks out (side 0, the
// original's first bytes back) and in again, and side-by-side runs on the same memory (the state saved
// before side 0 and put back before side 1).
#pragma once
#include "check_util.h"

namespace zax::seqc {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }

// The hooks of one module out and in again (sites saved in fixed arrays; hook_sites_fit reports a module
// with more hooks than room)
struct ModuleHooks {
    static constexpr int MAX = 128;
    const char *name;
    int (*table)(Hook *);
    Hook hooks[MAX];
    BYTE *sites[MAX];
    BYTE saved[MAX][5];
    int n = 0;
    void out()
    {
        n = 0;
        const int count = table(nullptr);
        if (!hook_sites_fit(name, count, MAX))
            return;
        table(hooks);
        for (int i = 0; i < count; i++) {
            BYTE *site = at<BYTE *>(hooks[i].address);
            if (original_address(hooks[i].address) == site)
                continue;   // not hooked (Disable=)
            sites[n] = site;
            memcpy(saved[n], site, 5);
            write_code(site, hooks[i].expected);
            n++;
        }
    }
    void in()
    {
        for (int i = 0; i < n; i++)
            write_code(sites[i], saved[i]);
        n = 0;
    }
};

// An 80-bit result as a hash (st0 of the float functions)
inline DWORD hash_ld(long double v)
{
    BYTE b[12] = {};
    memcpy(b, &v, 10);
    return fnv(2166136261u, b, 10);
}

// The game's x87 control word for both sides (the reimplementation runs with the same rounding)
inline WORD fpu_control()
{
    WORD cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}

}  // namespace zax::seqc
