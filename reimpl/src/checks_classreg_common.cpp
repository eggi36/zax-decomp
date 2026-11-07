// The shared part of the checks of batch C1 (checks_classreg_common.h).
#include "checks_classreg_common.h"

namespace zax::classreg {

DWORD g_vars[MAXVARS], g_var_values[MAXVARS];
int g_nvars;

void classreg_base(Gen &) {}

void register_effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls != -1 || (DWORD)slot != 0x415780)
        return;
    for (int i = 0; i < g_nvars; i++)
        if (args[0] == g_vars[i])
            *at<DWORD *>(args[0]) = g_effect_ecx;
    DWORD rest[5] = {args[4], args[5], args[6], args[7], args[8]};
    log_event(0x415, fnv(2166136261u, rest, sizeof rest), args[4]);
}

void set_vars(Gen &g)
{
    for (int i = 0; i < g_nvars; i++) {
        g_var_values[i] = g.below(3) ? 0 : g.next();
        case_global(g_vars[i], g_var_values[i]);
    }
    g_case.effect = &register_effect;
}

namespace {

const VarTest *g_var_test;
void var_setup(Gen &g)
{
    g_nvars = g_var_test->nvars;
    for (int i = 0; i < g_nvars; i++)
        g_vars[i] = g_var_test->vars[i];
    set_vars(g);
}

}  // namespace

void run_var_tests(Tally &t, ModuleHooks &hooks, const VarTest *tests, int n, const Stub *stubs, int nstubs, int count,
                   unsigned seed)
{
    for (int i = 0; i < n; i++) {
        Globals globals;
        for (int k = 0; k < tests[i].nvars; k++)
            globals.add(tests[i].vars[k], 4);
        g_var_test = &tests[i];
        const Test one[1] = {{tests[i].name, tests[i].address, CALL_CDECL, 0, ~0u, count, &var_setup, stubs, nstubs}};
        run_tests(t, hooks, &globals, one, 1, &classreg_base, seed + (unsigned)i * 10007u);
    }
}

}  // namespace zax::classreg
