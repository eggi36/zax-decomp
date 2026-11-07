// The milestone audit's checks (docs/DECOMPILATION.md 10, "The milestone audit"), in one module instead of the
// per-batch byte checks before (C2's single rets, C5's `c5 bytes`, which are hooked through their padding by
// short_hooks.cpp now):
// - the exe's code the audit leaves to the exe (docs/not_reimplemented.csv: identical by bytes, covered by its
//   caller, no function, dead code) against the hash of the bytes tools/audit.py recorded from Zax.exe, read in
//   the running exe: the code that runs or lies there is the documented one;
// - every hook of the modules' hook tables installed: its site is the jump to its replacement (with or without a
//   trampoline); only the launcher's two (DebugLog_SectionBegin/End, which Zax-Launcher.dll replaces before
//   us) are expected elsewhere;
// - the hook tables against docs/reimpl.csv: every row has a hook, every hook a row, no address twice.
// The tables come from the CSVs at build time (reimpl/build.sh).
#include "check_util.h"

namespace zax {

#define ZAX_HOOK_MODULE(name) int hooks_##name(Hook *out);
#include "hook_tables.inc"
#undef ZAX_HOOK_MODULE

namespace {

int all_hooks(Hook *out)
{
    int n = 0;
#define ZAX_HOOK_MODULE(name) n += hooks_##name(out ? out + n : nullptr);
#include "hook_tables.inc"
#undef ZAX_HOOK_MODULE
    return n;
}

struct ExeBytes {
    DWORD address, size, fnv1a;
    const char *name, *reason;
};
const ExeBytes BYTES[] = {
#include "not_hookable_table.inc"
};
const DWORD REIMPL[] = {
#include "reimpl_addresses.inc"
};
constexpr int NREIMPL = sizeof REIMPL / sizeof REIMPL[0];
// Zax-Launcher.dll replaces these two itself (its /DebugLog), before Zax-Reimpl.dll installs its hooks
const DWORD LAUNCHER[] = {0x4a2650, 0x4a2670};

void sort(DWORD *a, int n)   // Shell sort (no runtime library)
{
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++)
            for (int j = i; j >= gap && a[j - gap] > a[j]; j -= gap) {
                const DWORD t = a[j];
                a[j] = a[j - gap];
                a[j - gap] = t;
            }
}

bool installed(const Hook &h)
{
    const BYTE *site = at<const BYTE *>(h.address);
    return site[0] == 0xe9 && h.address + 5 + *reinterpret_cast<const DWORD *>(site + 1) == (DWORD)(uintptr_t)h.replacement;
}

}  // namespace

ZAX_CHECKS(not_hookable)
{
    (void)cls;
    (void)n;
    Tally b{"not hookable: the exe's code the audit leaves (docs/not_reimplemented.csv) against Zax.exe's recorded bytes"};
    DWORD bytes = 0;
    for (const ExeBytes &e : BYTES) {
        const unsigned h = fnv(2166136261u, at<const void *>(e.address), e.size);
        char d[200];
        wsprintfA(d, "[%x %s (%s): %u bytes, hash %x / %x]", e.address, e.name, e.reason, e.size, h, e.fnv1a);
        b.check(h == e.fnv1a, "", d);
        bytes += e.size;
    }
    log("  not hookable: %d ranges, %u bytes", (int)(sizeof BYTES / sizeof BYTES[0]), bytes);
    b.report();

    const int nh = all_hooks(nullptr);
    // (once per run; kept)
    Hook *hooks = static_cast<Hook *>(VirtualAlloc(nullptr, nh * sizeof(Hook), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    DWORD *addr = static_cast<DWORD *>(VirtualAlloc(nullptr, (nh + NREIMPL) * sizeof(DWORD), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!hooks || !addr) {
        log("  not hookable: no memory for the hook table");
        return;
    }
    all_hooks(hooks);
    Tally inst{"not hookable: every hook of the hook tables installed (its site the jump to its replacement)"};
    int ok = 0, launcher = 0;
    for (int i = 0; i < nh; i++) {
        const Hook &h = hooks[i];
        bool expected = installed(h);
        ok += expected;
        if (!expected)
            for (DWORD a : LAUNCHER)
                if (h.address == a)
                    expected = true, launcher++;
        char d[160];
        wsprintfA(d, "[%s %x not installed: %02x %02x %02x %02x %02x]", h.name, h.address, at<BYTE *>(h.address)[0],
                  at<BYTE *>(h.address)[1], at<BYTE *>(h.address)[2], at<BYTE *>(h.address)[3], at<BYTE *>(h.address)[4]);
        inst.check(expected, "", d);
    }
    log("  not hookable: %d hooks, %d installed, %d of the launcher", nh, ok, launcher);
    inst.report();

    // the hook tables against docs/reimpl.csv (both sorted, then merged)
    Tally tab{"not hookable: the hook tables against docs/reimpl.csv"};
    DWORD *ha = addr, *ra = addr + nh;
    for (int i = 0; i < nh; i++)
        ha[i] = hooks[i].address;
    memcpy(ra, REIMPL, sizeof REIMPL);
    sort(ha, nh);
    sort(ra, NREIMPL);
    int i = 0, j = 0, only_hook = 0, only_row = 0, twice = 0;
    char d[96];
    while (i < nh || j < NREIMPL) {
        if (i + 1 < nh && ha[i] == ha[i + 1]) {
            wsprintfA(d, "[hook %x twice in the tables]", ha[i]);
            tab.check(false, "", d);
            twice++, i++;
        } else if (j + 1 < NREIMPL && ra[j] == ra[j + 1]) {
            wsprintfA(d, "[%x twice in docs/reimpl.csv]", ra[j]);
            tab.check(false, "", d);
            twice++, j++;
        } else if (i < nh && j < NREIMPL && ha[i] == ra[j]) {
            tab.check(true, "");
            i++, j++;
        } else if (j >= NREIMPL || (i < nh && ha[i] < ra[j])) {
            wsprintfA(d, "[hook %x not in docs/reimpl.csv]", ha[i]);
            tab.check(false, "", d);
            if (only_hook++ < 300)
                log("    %s", d);
            i++;
        } else {
            wsprintfA(d, "[%x of docs/reimpl.csv has no hook]", ra[j]);
            tab.check(false, "", d);
            if (only_row++ < 300)
                log("    %s", d);
            j++;
        }
    }
    log("  not hookable: %d rows of docs/reimpl.csv, %d hooks; %d rows without a hook, %d hooks without a row, %d twice",
        NREIMPL, nh, only_row, only_hook, twice);
    tab.report();
}

}  // namespace zax
