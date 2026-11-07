// The checks of batch C1 (the class registrations, docs/DECOMPILATION.md 10, "Batch C1") on the toolkit's harness
// (checks_ui_common.h). The registrations run before the check thread (the C runtime's initializer table), so
// the checks call them again with everything they call as recording stand-ins and the class variables saved,
// set per case and put back:
// - ClassInfo_StaticRegister 0x417690 (the stubs' only callee): its object and getter logged;
// - operator_new_ecx 0x417710 answers 0, 1 or a random value (the class info is never touched, only handed on);
// - ClassInfo_Register 0x415780 writes its object into the variable it is given, as the real one does (only a
//   variable of the case), and logs its last five arguments (the harness logs the first four);
// - the parents' getters, the class registry and the other callees of the hand-written functions answer
//   from the case's values.
#pragma once
#include "checks_ui_common.h"

namespace zax::classreg {

using namespace uic;

// (constexpr in the header: the test tables that use them must be constant-initialised, the DLL has no C runtime
// to run dynamic initialisers; an extern array left the tables of classreg_a-d empty, 0 cases, in the first run)
inline constexpr Stub STATIC_REGISTER_STUBS[] = {
    {0x417690, CV_THIS, 1, K_VALUE, "ClassInfo_StaticRegister", "v"},
};
inline constexpr int NSTATIC_REGISTER_STUBS = sizeof STATIC_REGISTER_STUBS / sizeof STATIC_REGISTER_STUBS[0];

void classreg_base(Gen &g);

// The class variables of a case: set to 0 (two in three) or a random value per case, compared afterwards
constexpr int MAXVARS = 4;
extern DWORD g_vars[MAXVARS];
extern DWORD g_var_values[MAXVARS];   // the values set_vars chose for the case
extern int g_nvars;
void set_vars(Gen &g);
// ClassInfo_Register's stand-in effect (also for checks with an effect of their own: call it from theirs)
void register_effect(int cls, int slot, DWORD *args, DWORD answer);

// A getter test: the function and the class variables it can register
struct VarTest {
    const char *name;
    DWORD address;
    DWORD vars[MAXVARS];
    int nvars;
};
// Runs every test with `stubs` on: `count` cases each, the variables saved in a Globals of the test
void run_var_tests(Tally &t, ModuleHooks &hooks, const VarTest *tests, int n, const Stub *stubs, int nstubs, int count,
                   unsigned seed);

}  // namespace zax::classreg

namespace zax {
using classreg::classreg_base;
using classreg::NSTATIC_REGISTER_STUBS;
using classreg::STATIC_REGISTER_STUBS;
}  // namespace zax
