// Shared helpers of the checks of pool.cpp, platform.cpp, name_lists.cpp and core_misc.cpp: two sides
// (0 the original, 1 the reimplementation) with an event log and a state hash each, and taking a module's
// hooks out (the original's first bytes put back, so nested calls run the original too) and in again.
#pragma once
#include "check_util.h"

namespace zax::core_checks {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

struct Sides {
    int side = 0;
    unsigned log[2], state[2];
    int nlog[2];
    DWORD first[2][64][4];
    void reset()
    {
        log[0] = log[1] = state[0] = state[1] = 2166136261u;
        nlog[0] = nlog[1] = 0;
    }
    void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
    {
        const DWORD e[4] = {a, b, c, d};
        log[side] = fnv(log[side], e, sizeof e);
        if (nlog[side] < 64)
            memcpy(first[side][nlog[side]], e, sizeof e);
        nlog[side]++;
    }
    void st(DWORD v) { state[side] = fnv(state[side], &v, 4); }
    void st_text(const char *s) { state[side] = fnv_text(state[side], s ? s : "(null)"); }
    void st_bytes(const void *p, DWORD n) { state[side] = fnv(state[side], p, n); }
    bool same() const { return log[0] == log[1] && state[0] == state[1] && nlog[0] == nlog[1]; }
    void check(Tally &t, const char *what)
    {
        char d[160];
        wsprintfA(d, "[%.100s: log %d/%d state %x/%x]", what, nlog[0], nlog[1], state[0], state[1]);
        t.check(same(), "", d);
        if (!same())
            for (int i = 0; i < nlog[0] && i < nlog[1] && i < 64; i++)
                if (memcmp(first[0][i], first[1][i], 16)) {
                    log_event_difference(i);
                    break;
                }
    }
    void log_event_difference(int i)
    {
        zax::log("    event %d: %x %x %x %x / %x %x %x %x", i, first[0][i][0], first[0][i][1], first[0][i][2], first[0][i][3],
                 first[1][i][0], first[1][i][1], first[1][i][2], first[1][i][3]);
    }
};

// A module's hooks taken out for side 0
struct HooksOut {
    struct Site {
        BYTE *site;
        BYTE bytes[5];
    };
    Site sites[64];
    int n = 0;
    void out(int (*table)(Hook *))
    {
        Hook list[64];
        n = 0;
        const int count = table(nullptr);
        if (!hook_sites_fit("core_common", count, 64))
            return;
        table(list);
        for (int i = 0; i < count; i++) {
            BYTE *site = at<BYTE *>(list[i].address);
            if (original_address(list[i].address) == site)
                continue;   // not hooked (Disable=)
            Site &s = sites[n++];
            s.site = site;
            memcpy(s.bytes, site, 5);
            write(site, list[i].expected);
        }
    }
    void in()
    {
        for (int i = 0; i < n; i++)
            write(sites[i].site, sites[i].bytes);
        n = 0;
    }
    static void write(BYTE *site, const BYTE *bytes)
    {
        write_code(site, bytes);
    }
};

// Runs fn(side) for side 0 with the module's hooks out and side 1 with them in
template <typename F> void both_sides(Sides &s, int (*table)(Hook *), F fn)
{
    s.reset();
    HooksOut h;
    s.side = 0;
    h.out(table);
    fn(0);
    h.in();
    s.side = 1;
    fn(1);
}

}  // namespace zax::core_checks
