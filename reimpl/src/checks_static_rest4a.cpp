// Checks of the static initializers, atexit stubs, inline blend mode constructors and name registrations
// (static_rest4a.cpp) against the original, with the toolkit's harness (checks_ui_common.h): every callee is a
// recording stand-in (constructors, destructors, atexit, BlendMode_RegisterBlitter, Cpu_HasMmx, the name list,
// CallMember), so the live globals are never constructed or destroyed again; the globals the functions write
// themselves (the Bink system object, the two links, the palette guard, the modes' vtables) are saved, set to the
// case's values for both sides, compared and put back; the Overlap mode's four span tables are filled with a
// pattern of the case, hashed into the log and put back after the test.
#include "checks_ui_common.h"

namespace zax {

int hooks_static_rest4a(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"static_rest4a", &hooks_static_rest4a};
Globals g_globals;

const Stub STUBS[] = {
    {0x5d0863, CV_CDECL, 1, K_VALUE, "atexit"},
    {0x501030, CV_THIS, 2, K_VALUE, "LocalString_Ctor", "sv"},
    {0x4defd0, CV_THIS, 0, K_VALUE, "ZString_Dtor"},
    {0x5266d0, CV_THIS, 0, 'e', "Palette_Ctor"},
    {0x5266e0, CV_THIS, 0, K_VALUE, "NameEntry_FreeName"},
    {0x432da0, CV_THIS, 0, K_VALUE, "CBlendMode_Dtor"},
    {0x432d60, CV_THIS, 0, 'e', "CBlendMode_Ctor"},
    {0x432de0, CV_THIS, 5, K_VALUE, "BlendMode_RegisterBlitter", "vvvvv"},
    {0x433df0, CV_THIS, 0, 'e', "CBlendAmplifyLight_Ctor"},
    {0x4386f0, CV_THIS, 0, 'e', "CBlendBlackBias_Ctor"},
    {0x43eac0, CV_THIS, 0, 'e', "CBlendCopy_Ctor"},
    {0x443220, CV_THIS, 0, 'e', "CBlendScreenUp555_Ctor"},
    {0x443310, CV_THIS, 0, 'e', "CBlendScreenDown555_Ctor"},
    {0x444ea0, CV_THIS, 0, 'e', "CBlendScreenUp565_Ctor"},
    {0x444fd0, CV_THIS, 0, 'e', "CBlendScreenDown565_Ctor"},
    {0x4479c0, CV_THIS, 0, 'e', "CBlendTransparentMask_Ctor"},
    {0x5156b0, CV_CDECL, 0, K_BOOL, "Cpu_HasMmx"},
    {0x4669a0, CV_CDECL, 0, K_VALUE, "BlendModeNames"},
    {0x5922b0, CV_THIS, 2, K_VALUE, "FieldNameList::AddValue", "sv"},
    {0x55d050, CV_THIS, 1, K_VALUE, "CallMember", "v"},
    {0x53f010, CV_THIS, 4, K_VALUE, "FieldColor_CtorRGBA", "vvvv"},
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x417690, CV_THIS, 1, K_VALUE, "ClassInfo_StaticRegister", "v"},
    {0x431e50, CV_THIS, 0, K_VALUE, "BinkSystem_Dtor"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// The Overlap mode's span tables (64 dwords each)
constexpr DWORD OVERLAP_TABLES[] = {0x6bca4c, 0x6bc84c, 0x6bc518, 0x6bc94c};
DWORD g_tables_saved[4][0x40];

constexpr DWORD BINK_DWORDS[] = {0x62eaf0, 0x62eaf8, 0x62eafc, 0x62eb00, 0x62eb04, 0x62eb08, 0x62eb0c, 0x62eb10};

constexpr DWORD MODE_VTABLES[] = {0x6b85f0, 0x6b9728, 0x6b9c60, 0x6b9ca0, 0x6bb520, 0x6bbaa0, 0x6bc4a8, 0x6bc4e0, 0x6bc618};

void base(Gen &) {}
void s_none(Gen &) {}
void s_guard(Gen &g) { case_global(0x6b67dc, g.chance(4) ? 0 : g.below(16)); }
void s_bink(Gen &g)
{
    for (DWORD a : BINK_DWORDS)
        case_global(a, g.next());
    case_global(0x62eaf4, g.below(256));
    case_global(0x62eb14, g.below(256));
}
void s_overlap(Gen &g)
{
    for (DWORD t : OVERLAP_TABLES)
        for (int i = 0; i < 0x40; i++)
            at<DWORD *>(t)[i] = g.next();
}
void st_overlap()
{
    unsigned h = 2166136261u;
    for (DWORD t : OVERLAP_TABLES)
        h = fnv(h, at<void *>(t), 0x100);
    log_event(0x5a01, h, 0);
}
void s_names(Gen &g)
{
    g_self = xalloc(0x20);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.next();
}

#define CD(name, addr) {name, addr, CALL_CDECL, 0, ~0u, 6, s_none, STUBS, NSTUBS}
#define CDG(name, addr) {name, addr, CALL_CDECL, 0, 0xff, 40, s_guard, STUBS, NSTUBS}
const Test TESTS[] = {
    {"StaticInit_BinkSystem", 0x431dd0, CALL_CDECL, 0, ~0u, 20, s_bink, STUBS, NSTUBS},
    CD("StaticExit_BinkSystem", 0x431e20),
    CD("StaticInit_FatalErrorText", 0x432b60),
    CD("StaticExit_FatalErrorText", 0x432b80),
    CD("StaticInit_LocalString_630360", 0x432b90),
    CD("StaticExit_LocalString_630360", 0x432bb0),
    CD("StaticInit_Link_630374", 0x432bc0),
    CD("StaticInit_Link_6753b4", 0x432bd0),
    CD("StaticInit_Palette_633f98", 0x432be0),
    CD("StaticExit_Palette_633f98", 0x432c00),
    CD("StaticInit_Palette_633388", 0x432c60),
    CDG("StaticExit_Palette_633388", 0x432c80),
    CD("StaticInit_Palette_630368", 0x432ca0),
    CDG("StaticExit_Palette_630368", 0x432cc0),
    CD("StaticInit_Palette_6767d0", 0x432ce0),
    CDG("StaticExit_Palette_6767d0", 0x432d00),
    CD("StaticInit_Palette_6767c0", 0x432d20),
    CDG("StaticExit_Palette_6767c0", 0x432d40),
    CD("StaticInit_BlendAmplifyLight", 0x433d50),
    CD("StaticExit_BlendAmplifyLight", 0x433d70),
    CD("StaticInit_RegisterAmplifyLightName", 0x433dc0),
    CD("RegisterBlend_AmplifyLight", 0x433dd0),
    CD("StaticInit_BlendBlackBias", 0x438600),
    CD("StaticExit_BlendBlackBias", 0x438620),
    CD("StaticInit_BlendTestLight", 0x438670),
    CD("StaticExit_BlendTestLight", 0x438690),
    CD("StaticInit_RegisterBlackBiasNames", 0x4386b0),
    CD("RegisterBlend_BlackBias_TestLight", 0x4386c0),
    CD("CBlendFilter_Ctor", 0x43ad30),
    CD("StaticExit_BlendFilter", 0x43ad80),
    CD("StaticInit_BlendCopy", 0x43ea80),
    CD("StaticExit_BlendCopy", 0x43eaa0),
    CD("StaticInit_BlendCopyNoName", 0x43eab0),
    CD("CBlendNormalScale_Ctor", 0x441a70),
    CD("StaticExit_BlendNormalScale", 0x441b10),
    CD("StaticInit_RegisterNormalScaleName", 0x441b20),
    CD("RegisterBlend_NormalScale", 0x441b30),
    {"CBlendScreenCopy_Ctor", 0x443130, CALL_CDECL, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    CD("StaticExit_BlendScreenCopy", 0x4431b0),
    CD("StaticInit_BlendScreenUp555", 0x4431c0),
    CD("StaticExit_BlendScreenUp555", 0x4431e0),
    CD("StaticInit_BlendScreenDown555", 0x4431f0),
    CD("StaticExit_BlendScreenDown555", 0x443210),
    CD("StaticInit_BlendScreenUp565", 0x444e70),
    CD("StaticExit_BlendScreenUp565", 0x444e90),
    CD("StaticInit_BlendScreenDown565", 0x444fa0),
    CD("StaticExit_BlendScreenDown565", 0x444fc0),
    CD("CBlendTileWrap_Ctor", 0x446110),
    CD("StaticExit_BlendTileWrap", 0x446180),
    CD("StaticInit_BlendTileWrapNoName", 0x446510),
    CD("CBlendTile_Ctor", 0x446520),
    CD("StaticExit_BlendTile", 0x446590),
    CD("StaticInit_BlendTileNoName", 0x4465a0),
    CD("StaticInit_OverlapColor", 0x446940),
    {"CBlendOverlap_Ctor", 0x446960, CALL_CDECL, 0, ~0u, 12, s_overlap, STUBS, NSTUBS, nullptr, &st_overlap},
    CD("StaticExit_BlendOverlap", 0x446a50),
    CD("StaticInit_RegisterOverlapName", 0x446a60),
    CD("RegisterBlend_Overlap", 0x446a70),
    CD("StaticInit_BlendTransparentMask", 0x447960),
    CD("StaticExit_BlendTransparentMask", 0x447980),
    CD("StaticInit_RegisterTransparentMaskName", 0x447990),
    CD("RegisterBlend_TransparentMask", 0x4479a0),
    {"BlendNames_Add", 0x4669f0, CALL_THIS, 2, ~0u, 30, s_names, STUBS, NSTUBS},
    CD("StaticInit_FrameScratch", 0x4aeca0),
    CD("StaticInit_CFrameInterfaceClass", 0x4b2c10),
};

}  // namespace

ZAX_CHECKS(static_rest4a)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : BINK_DWORDS)
            g_globals.add(a, 4);
        g_globals.add(0x62eaf4, 1);
        g_globals.add(0x62eb14, 1);
        g_globals.add(0x630374, 4);
        g_globals.add(0x6753b4, 4);
        g_globals.add(0x6b67dc, 1);
        for (DWORD a : MODE_VTABLES)
            g_globals.add(a, 4);
    }
    for (int i = 0; i < 4; i++)
        memcpy(g_tables_saved[i], at<void *>(OVERLAP_TABLES[i]), 0x100);
    Tally t{"static rest 4a: the static initializers and atexit stubs, the inline blend modes, the name registrations"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 41000);
    for (int i = 0; i < 4; i++)
        memcpy(at<void *>(OVERLAP_TABLES[i]), g_tables_saved[i], 0x100);
    t.report();
}

}  // namespace zax
