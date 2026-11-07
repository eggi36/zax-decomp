// Checks of the loading screen and the mini map's image (world_c5.cpp) against the original with the toolkit's
// harness (checks_ui_common.h, checks_ui_screens.h): the layer and the world object of the loading background are
// random memory in the extra area (the layer's name a ZString of the case); world, reporter and screen are the
// screens' game objects, the loading sequence a game object, frames fakes of the frame class (the frame
// constructor, patched for the module's run, gives a made frame the fake vtable and logs without the address). The world's loading queries, the layer's file name, the frame
// functions (size, blit, palette, load, write, file name, save), the save data, the image rectangle, RenderLayer,
// the file test (always "exists": exit never returns, the original would run on into the next function), the error box, exit, the ticks, the flip and the progress stub are stand-ins; ZString and path
// code is the exe's. The loading globals 0x6c28fc, 0x6c2900, 0x6c28e0, 0x6c2638 are case values. Every case runs
// from a cleared stack area (the local frames of 0x4ec600 are made by the stand-in constructor).
#include "checks_ui_screens.h"

namespace zax {

int hooks_world_c5(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"world_c5", &hooks_world_c5};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a8b00, CV_THIS, 0, K_VALUE, "CFrame::Dtor"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4aa390, CV_THIS, 1, K_VALUE, "CFrame_SetPalette"},
    {0x433a80, CV_CDECL, 0, K_VALUE, "GamePalette_Get"},
    {0x4aa070, CV_THIS, 2, K_VALUE, "CFrame::WriteTga", "sv"},
    {0x4a9900, CV_THIS, 5, K_VALUE, "CFrame::FileName", "xssv"},
    {0x4a9a30, CV_THIS, 5, K_BOOL, "CFrame::Load", "ssvv"},
    {0x4a9af0, CV_THIS, 0, K_VALUE, "CFrame::Save"},
    {0x4e7100, CV_THIS, 2, K_VALUE, "CLayerSaveData::Frame", "sv"},
    {0x4ec280, CV_THIS, 1, K_VALUE, "Map_MiniMapImageRect", "x"},
    {0x4f4ae0, CV_THIS, 4, K_VALUE, "World::RenderLayer"},
    {0x4f4d30, CV_THIS, 1, K_BOOL, "World::IsLoadingLayer"},
    {0x4f4d60, CV_THIS, 0, K_BOOL, "World::LoadingFlag"},
    {0x4f4d50, CV_THIS, 0, K_TEXT, "World::LoadingName"},
    {0x4eb670, CV_THIS, 0, K_TEXT, "CLayer::FileName"},
    {0x536080, CV_THIS, 3, K_VALUE, "Stub_ReturnZero12", "svv"},
    {0x4a1650, CV_FAST, 0, K_ONE, "File_Exists", "sv"},
    {0x4d24c0, CV_FAST, 0, K_VALUE, "ShowErrorBox", "ss"},
    {0x5d0e36, CV_CDECL, 1, K_VALUE, "crt_exit"},
    {0x55d030, CV_CDECL, 0, K_SMALL, "ColorDepth"},
    {0x4d2650, CV_CDECL, 0, K_VALUE, "Platform_Ticks24"},
    {0x5693a0, CV_THIS, 0, K_VALUE, "ScreenFrame_Flip"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_FRAME = W_OTHER, W_SEQ = W_OTHER2, W_IMAGE = W_SLIDER_T;
char g_object_args[MAXSLOT + 1];

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)answer;
    Gen &v = g_case.values;
    if (cls == C_OBJECT && slot == 99 && args[0]) {   // the world's mini map size
        at<DWORD *>(args[0])[0] = v.chance(2) ? g_case.fixed[1] : v.below(300);
        at<DWORD *>(args[0])[1] = v.chance(2) ? g_case.fixed[2] : v.below(300);
    } else if (cls == -1 && (DWORD)slot == 0x4ec280 && args[0]) {
        int *r = at<int *>(args[0]);
        r[0] = v.range(0, 50), r[1] = v.range(0, 50), r[2] = v.range(50, 200), r[3] = v.range(50, 200);
    }
}

void base(Gen &g)
{
    screen_base(g, &effect);
    make_window(W_FRAME, C_CFRAME);
    make_window(W_IMAGE, C_CFRAME);
    make_window(W_SEQ, C_OBJECT);
    object_args(g_object_args, "23:1,25:0,26:0,99:1,102:1");
    g_case.argspec[99][0] = A_IGNORE;   // (the size the world writes into the caller's uninitialised local)
    g_case.argspec[102][0] = A_IGNORE;  // (the background colour, a FieldColor the world fills: uninitialised but +0xc,
    g_case.argspec[26][0] = A_IGNORE;   // then the frame cleared with it)
    set_kind(C_OBJECT, 25, K_FIXED0);
    set_kind(C_OBJECT, 26, K_FLOAT);
    set_kind(C_CFRAME, 45, K_FIXED1);
    set_kind(C_CFRAME, 39, K_FIXED2);
    set_kind(C_CFRAME, 29, K_SMALL);
    g_case.fixed[0] = 1 + g.below(12);
    g_case.fixed[1] = g.chance(3) ? 0x280 : g.below(300);
    g_case.fixed[2] = g.below(300);
    at_(win(W_WORLD), 0x308) = g.chance(5) ? 0 : ptrv(win(W_SEQ));
    at_(win(W_SCREEN), 0xd8) = g.next();
    case_global(0x6c28fc, g.chance(5) ? 0 : 1);
    case_global(0x6c2900, g.next());
    case_global(0x6c28e0, g.next());
    case_global(0x6c2638, g.below(14));
    if (g.chance(10))
        at_(win(W_REPORTER), 4) = 0;
}

BYTE *new_layer(Gen &g)
{
    BYTE *l = xalloc(0x1c0);
    for (DWORD o = 0; o < 0x1c0; o += 4)
        at_(l, o) = g.next();
    make_text(l + 0xdc, TEXTS[g.below(8)]);
    at_(l, 0x138) = g.chance(2) ? 0 : ptrv(win(W_FRAME));
    for (DWORD o = 0x1a8; o <= 0x1b4; o += 4)
        at_(l, o) = g.below(40);
    at_(l, 0x48) = g.below(4000);
    at_(l, 0x4c) = g.below(4000);
    g_self = l;
    return l;
}
void s_layer(Gen &g) { new_layer(g); }
void s_tick(Gen &) {}
void s_background(Gen &g)
{
    BYTE *w = xalloc(0x320);
    for (DWORD o = 0; o < 0x320; o += 4)
        at_(w, o) = g.next();
    at_(w, 4) = ptrv(win(W_FRAME));
    at_(w, 0x300) = ptrv(win(W_IMAGE));
    at_(w, 0x304) = g.chance(2) ? 0 : ptrv(win(W_FRAME));
    g_self = w;
}

// CFrame's constructor: the made frame gets the fake frame class; logged without its address (the frames of 0x4ec600
// lie on the stack, at other depths below the original's and the reimplementation's callers)
void *THISCALL fake_frame_ctor(void *f)
{
    log_event(0x7a01, 0, 0);
    if (f)
        *static_cast<DWORD *>(f) = at_(win(W_FRAME), 0);
    return f;
}

__attribute__((noinline)) void clear_stack()
{
    volatile BYTE area[0x1400];
    for (int i = (int)sizeof area - 1; i >= 0; i--)
        area[i] = 0;
}
template <DWORD A> DWORD THISCALL from_clear_stack(BYTE *self)
{
    clear_stack();
    return at<DWORD(THISCALL *)(BYTE *)>(A)(self);
}
template <DWORD A> DWORD fn() { return ptrv((void *)&from_clear_stack<A>); }

constexpr DWORD GLOBALS_C5[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6c2900, 0x6c28e0, 0x6c2638};

}  // namespace

ZAX_CHECKS(world_c5)
{
    (void)cls;
    (void)n;
    const Test tests[] = {
        {"World_LoadingTick", fn<0x4f3ba0>(), CALL_THIS, 0, 0, 400, s_tick, STUBS, NSTUBS},
        {"World::LoadingBackground", fn<0x4f3d50>(), CALL_THIS, 0, 0xffffffff, 200, s_background, STUBS, NSTUBS},
        {"Map_BuildMiniMapImage", fn<0x4ec600>(), CALL_THIS, 0, 0, 200, s_layer, STUBS, NSTUBS},
        {"Map_SetupMiniMap", fn<0x4ed150>(), CALL_THIS, 0, 0, 200, s_layer, STUBS, NSTUBS},
        {"Map_LoadMiniMap", fn<0x4ece20>(), CALL_THIS, 0, 0, 300, s_layer, STUBS, NSTUBS},
    };
    if (!g_globals.n) {
        for (DWORD a : GLOBALS_C5)
            g_globals.add(a, 4);
        g_globals.add(0x6c28fc, 1);
    }
    Tally t{"world c5: the loading screen and the mini map image"};
    CodePatch ctor;
    patch_jump(ctor, 0x4a8a90, (void *)&fake_frame_ctor);
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 71000);
    unpatch(ctor);
    t.report();
}

}  // namespace zax
