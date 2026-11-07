// The check harness of batch C2 of phase 5a (checks_static_c2.h).
#include "checks_static_c2.h"

namespace zax::sc2 {

using namespace uic;

const STest *g_test;
DWORD g_object_vtable;

namespace {

// The tested function's hook alone (ModuleHooks takes a table function)
Hook g_one;
int one_hook(Hook *out)
{
    if (out)
        out[0] = g_one;
    return 1;
}
constinit ModuleHooks g_hooks{"static_c2", &one_hook};

constexpr int MAX_MODULE_HOOKS = 512;
Hook g_table[MAX_MODULE_HOOKS];

// The ranges of the running test: saved before its cases, put back after
constexpr int MAX_RANGES = 4;
constexpr DWORD MAX_RANGE_BYTES = 0x800;
BYTE g_range_saved[MAX_RANGES][MAX_RANGE_BYTES];

void generic_setup(Gen &g)
{
    for (int i = 0; i < g_test->nglobals; i++) {
        const SGlobal &s = g_test->globals[i];
        if (s.kind == 'v') {
            // the game's value now and then (a store of the same value must still be the same), else a
            // random one; bytes mostly small (flags and guard bits)
            if (g.chance(5))
                continue;
            const DWORD v = s.size == 1 ? (g.chance(2) ? g.below(16) : g.below(256)) : s.size == 2 ? g.below(0x10000) : g.next();
            case_global(s.address, v);
        } else if (s.kind == 'p') {
            if (g.chance(4)) {
                case_global(s.address, 0);
                continue;
            }
            BYTE *b = xalloc(0x40);
            for (int k = 0; b && k < 0x10; k++)
                at_(b, 4 * k) = g.chance(4) ? g.next() : g.below(3);
            case_global(s.address, ptrv(b));
        } else if (s.kind == 'r') {
            BYTE *r = at<BYTE *>(s.address);
            for (DWORD k = 0; k + 4 <= s.size; k += 4)
                at_(r, k) = g.next();
        }
    }
    if (g_test->setup)
        g_test->setup(g);
}

void ranges_state()
{
    for (int i = 0; i < g_test->nglobals; i++) {
        const SGlobal &s = g_test->globals[i];
        if (s.kind == 'r')
            log_event(0xc2a0, s.address, fnv(2166136261u, at<void *>(s.address), s.size));
    }
}

void base(Gen &) {}

}  // namespace

void run_static_tests(const char *title, int (*hooks)(Hook *), const STest *tests, int n, const DWORD *rets, int nrets,
                      unsigned seed)
{
    Tally t{title};
    // the single rets (rets) are hooked through their padding since the milestone audit and checked with the
    // other short functions (checks_short_hooks.cpp)
    (void)rets;
    (void)nrets;
    const int nhooks = hooks(nullptr);
    if (!hook_sites_fit(title, nhooks, MAX_MODULE_HOOKS)) {
        t.report();
        return;
    }
    hooks(g_table);
    if (!g_object_vtable) {
        BYTE *w = make_window(0, C_OBJECT);
        g_object_vtable = at_(w, 0);
        original<void(THISCALL *)(void *)>(0x4defd0)(w + 0x50);   // its title again
        memset(w, 0, WIN);
    }
    for (int i = 0; i < n; i++) {
        const STest &st = tests[i];
        int h = 0;
        while (h < nhooks && g_table[h].address != st.address)
            h++;
        if (h == nhooks) {
            char d[64];
            wsprintfA(d, "[%s: no hook at %x]", st.name, st.address);
            t.check(false, "", d);
            continue;
        }
        g_one = g_table[h];
        Globals globals;
        int nr = 0;
        for (int k = 0; k < st.nglobals; k++) {
            const SGlobal &s = st.globals[k];
            if (s.kind == 'r') {
                if (nr < MAX_RANGES && s.size <= MAX_RANGE_BYTES)
                    memcpy(g_range_saved[nr++], at<void *>(s.address), s.size);
            } else if (globals.n < Globals::MAX) {
                globals.add(s.address, s.size);
            }
        }
        g_test = &st;
        Stub stubs[NSTUB];
        const int ns = st.nstubs < NSTUB ? st.nstubs : NSTUB;
        memcpy(stubs, st.stubs, sizeof(Stub) * (DWORD)ns);
        for (int pass = 0; pass < (st.has_new ? 2 : 1); pass++) {
            if (pass == 1)
                for (int k = 0; k < ns; k++)
                    if (stubs[k].address == 0x5d033a)
                        stubs[k].kind = K_ZERO;
            const Test test{st.name, st.address, CALL_CDECL, 0, st.mask, st.count, &generic_setup, stubs, ns,
                            nullptr, &ranges_state};
            run_tests(t, g_hooks, &globals, &test, 1, &base, seed + (unsigned)(i * 7919 + pass * 104729));
        }
        nr = 0;
        for (int k = 0; k < st.nglobals; k++) {
            const SGlobal &s = st.globals[k];
            if (s.kind == 'r' && nr < MAX_RANGES && s.size <= MAX_RANGE_BYTES)
                memcpy(at<void *>(s.address), g_range_saved[nr++], s.size);
        }
    }
    g_test = nullptr;
    t.report();
}

}  // namespace zax::sc2

namespace zax {

void log_start_order()
{
    const DWORD *begin = *at<const DWORD **>(0x71755c), *end = *at<const DWORD **>(0x717558);
    const int n = begin && end > begin ? (int)(end - begin) : 0;
    log("Start order: %d atexit entries, hash %x", n, n ? fnv(2166136261u, begin, (unsigned)n * 4) : 0);
    for (int i = 0; i < n; i += 8) {
        DWORD v[8] = {};
        for (int k = 0; k < 8 && i + k < n; k++)
            v[k] = begin[i + k];
        log("  atexit %d: %x %x %x %x %x %x %x %x", i, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    }
}

}  // namespace zax
