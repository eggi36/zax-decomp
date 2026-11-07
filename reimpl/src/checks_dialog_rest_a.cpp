// Checks of dialog_rest_a.cpp against the original, in the game thread, in the fake world of
// checks_resta_world.h: the script list (its vtable, records and count are case values; its slots 1 and 37
// are recorders), DefinitionList_Object, the sound manager, CDialogLine::Text and the action's slot 33 are
// stand-ins; Path_Join, PathName and the ZString functions are the exe's.
//
//  - The line lookup and slot 29 on actions with script indices inside and beyond the count (taken as 0),
//    line indices 0 to 3, records loaded or not (loaded through slot 37 with names with and without a
//    folder, so both of PathName's cases), the voice found or not; the five arguments passed through.
#include "checks_resta_world.h"

namespace zax {

namespace {

using namespace ra;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    D(s, 0) = ptrv(g_fvt[T_ACTION]);
    D(s, 8) = g.next() % 5 == 0 ? 4 + g.next() % 3 : g.next() % 4;
    D(s, 0xc) = g.next() % 4;
    // the records: loaded (one of the fake scripts) or not, never "loading" (the original reads a script
    // through null then)
    BYTE *r = obj(O_RECORDS);
    for (DWORD i = 0; i < 4; i++) {
        D(r + i * 0x80, 0x78) = g.next() % 2 ? ptrv(obj(O_SCRIPT + (int)(g.next() % 2))) : 0;
        r[i * 0x80 + 0x7f] = 0;
    }
    D(case_global(0x62e7e0), 0) = ptrv(g_fvt[T_CATALOGUE]);
    DWORD *rc = case_global(0x62e810);
    rc[0] = ptrv(r);
    rc[1] = 1 + g.next() % 4;
}

void a_none(Gen &, DWORD *) {}
void a_five(Gen &g, DWORD *a)
{
    for (int i = 0; i < 5; i++)
        a[i] = g.next();
}

const SlotTest TESTS[] = {
    {"CZaxDialogAction::Line", 0x41dac0, 0, 0xffffffff, 300, a_none},
    {"CZaxDialogAction::ShowLine29", 0x41d830, 5, 0xff, 400, a_five},
};

}  // namespace

ZAX_CHECKS(dialog_rest_a)
{
    (void)cls;
    (void)n;
    Tally t{"dialog_rest_a: the dialogue line lookup and CZaxDialogAction's slot 29 in the fake world"};
    make_static_texts();
    if (world_on()) {
        for (const SlotTest &x : TESTS) {
            Policy p{};
            const unsigned k = (unsigned)scaled(x.count);
            for (unsigned seed = 1; seed <= k; seed++)
                run_slot(t, x, seed * 2654435761u + x.address, make_self, p);
        }
        world_off();
    } else {
        t.check(false, "dialog_rest_a", "[the fake world could not be set up]");
    }
    t.report();
}

}  // namespace zax
