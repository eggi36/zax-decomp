// Checks of batch C3's atexit destructors (static_c3.cpp) with batch C2's harness (checks_static_c2.h): every
// callee a recording stand-in, the lists' globals case values, their entry arrays in the harness's block with
// "game object" fakes (slots 1 and 7 recorded with their argument).
#include "checks_static_c2.h"

namespace zax {

int hooks_static_c3(Hook *out);

namespace {

using namespace uic;
using namespace sc2;

constexpr char OBJ_ARGS[] = "0100000100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
static_assert(sizeof OBJ_ARGS == 113, "112 slots");

int g_next_win;
DWORD fake_object(Gen &g, int one_in_null)
{
    if (g.chance((DWORD)one_in_null) || g_next_win >= NWIN)
        return 0;
    BYTE *w = win(g_next_win++);
    memset(w, 0, WIN);
    at_(w, 0) = g_object_vtable;
    return ptrv(w);
}
void resource_list(Gen &g, DWORD b)
{
    g_next_win = 0;
    g_case.object_args = OBJ_ARGS;
    DWORD data = 0;
    if (!g.chance(4)) {
        const DWORD count = g.below(4);
        BYTE *block = xalloc(4 + count * 0x80);
        if (block) {
            at_(block, 0) = count;
            for (DWORD k = 0; k < count; k++)
                at_(block, 4 + k * 0x80 + 0x78) = fake_object(g, 3);
            data = ptrv(block) + 4;
        }
    }
    case_global(b + 0x30, data);
    case_global(b + 0x13c, fake_object(g, 3));
}
void setup_font_list(Gen &g) { resource_list(g, 0x6be1e0); }
void setup_overlay_list(Gen &g) { resource_list(g, 0x6bf9b8); }
void setup_water_list(Gen &g) { resource_list(g, 0x713c80); }

const Stub S_LIST[] = {{0x5d0320, CV_CDECL, 1, 'v', "operator_delete"}, {0x591cd0, CV_THIS, 5, 'v', "RecordList_Set"},
                       {0x55d0c0, CV_THIS, 0, 'v', "ResourceAliases_Dtor"}};
const SGlobal G_FONT[] = {{0x6be1e0, 4, 'v'}, {0x6be210, 4, 'v'}, {0x6be214, 4, 'v'}, {0x6be31c, 4, 'v'}};
const SGlobal G_OVERLAY[] = {{0x6bf9b8, 4, 'v'}, {0x6bf9e8, 4, 'v'}, {0x6bf9ec, 4, 'v'}, {0x6bfaf4, 4, 'v'}};
const SGlobal G_WATER[] = {{0x713c80, 4, 'v'}, {0x713cb0, 4, 'v'}, {0x713cb4, 4, 'v'}, {0x713dbc, 4, 'v'}};
const SGlobal G_6D04C0[] = {{0x6d04c0, 4, 'v'}};
const SGlobal G_6D04A8[] = {{0x6d04a8, 4, 'v'}};
const SGlobal G_6D0218[] = {{0x6d0218, 4, 'v'}};
const SGlobal G_6D0270[] = {{0x6d0270, 4, 'v'}};
const SGlobal G_6D03F8[] = {{0x6d03f8, 4, 'v'}};
const SGlobal G_6D03E0[] = {{0x6d03e0, 4, 'v'}};

const STest TESTS[] = {
    {"StaticExit_FontList", 0x4a7a60, 0xffffffffu, 8, G_FONT, 4, S_LIST, 3, setup_font_list, true, false},
    {"StaticExit_OverlayOptionsList", 0x4b49a0, 0xffffffffu, 8, G_OVERLAY, 4, S_LIST, 3, setup_overlay_list, true, false},
    {"StaticExit_WaterWarpList", 0x598850, 0xffffffffu, 8, G_WATER, 4, S_LIST, 3, setup_water_list, true, false},
    {"StaticExit_52a3f0", 0x52a3f0, 0x0u, 3, G_6D04C0, 1, nullptr, 0, nullptr, false, false},
    {"StaticExit_52a530", 0x52a530, 0x0u, 3, G_6D04A8, 1, nullptr, 0, nullptr, false, false},
    {"StaticExit_52a670", 0x52a670, 0x0u, 3, G_6D0218, 1, nullptr, 0, nullptr, false, false},
    {"StaticExit_52a7b0", 0x52a7b0, 0x0u, 3, G_6D0270, 1, nullptr, 0, nullptr, false, false},
    {"StaticExit_52a800", 0x52a800, 0x0u, 3, G_6D03F8, 1, nullptr, 0, nullptr, false, false},
    {"StaticExit_52a850", 0x52a850, 0x0u, 3, G_6D03E0, 1, nullptr, 0, nullptr, false, false},
};

}  // namespace

ZAX_CHECKS(static_c3)
{
    (void)cls;
    (void)n;
    run_static_tests("static c3: the rendering lists' atexit destructors and six vtable exit stubs", &hooks_static_c3, TESTS,
                     sizeof TESTS / sizeof TESTS[0], nullptr, 0, 65000);
}

}  // namespace zax
