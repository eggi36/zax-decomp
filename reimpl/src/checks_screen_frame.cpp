// Checks of the screen frame's drawing logic and the display setup (screen_frame.cpp) against the original with the
// toolkit's harness (checks_ui_common.h): the screen frame is a fake of the CFrame class's recording vtable with the
// case's members; the DirectDraw back buffer and palette behind 0x713338/0x713344 are fake COM objects of this file
// (their Blt, Unlock and SetEntries recorded with what they point to); the phase 5a functions (Lock, flip, release,
// set mode, the constructor), the CFrame drawing functions, the blend levels, the palette helpers, the tick count,
// the INI file and the command line are stand-ins; the palette holders 0x713298/0x713320 point into the block.
#include "checks_ui_common.h"

namespace zax {

int hooks_screen_frame(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"screen_frame", &hooks_screen_frame};
Globals g_globals;

constexpr DWORD LOCKED = 0x713318, BACK = 0x713338, PALETTE_OBJECT = 0x713344, FULL_SCREEN = 0x6210c0;

// ---- Fake COM objects ---------------------------------------------------------------------------------------------

bool g_all_entries;   // SetEntries' last 11 entries are set (the palette's flag), else stack garbage of the caller

DWORD com_record(DWORD slot, DWORD self, int n, const DWORD *a)
{
    unsigned h = fnv(2166136261u, &slot, 4);
    h = fnv(h, &self, 4);
    if (slot == 5) {   // Blt (dest rect, source, source rect, flags, effects): the rect's contents, the fill colour
        h = a[0] ? fnv(h, at<void *>(a[0]), 16) : fnv(h, a, 4);
        h = fnv(h, a + 1, 16);
        if (a[4])
            h = fnv(h, at<BYTE *>(a[4] + 0x50), 4);
    } else if (slot == 6 && n == 4) {   // SetEntries (flags, first, count, entries)
        h = fnv(h, a, 12);
        if (a[3])
            h = fnv(h, at<void *>(a[3]), g_all_entries ? 0x400 : 0xf5 * 4);
    } else {
        h = fnv(h, a, 4 * (DWORD)n);
    }
    log_event(0xc000 | slot, h, (DWORD)n);
    return 0;
}
DWORD __stdcall com_c0(DWORD self) { return com_record(99, self, 0, nullptr); }
DWORD __stdcall com_unlock(DWORD self, DWORD a) { return com_record(32, self, 1, &a); }
DWORD __stdcall com_blt(DWORD self, DWORD a, DWORD, DWORD, DWORD, DWORD) { return com_record(5, self, 5, &a); }
DWORD __stdcall com_entries(DWORD self, DWORD a, DWORD, DWORD, DWORD) { return com_record(6, self, 4, &a); }

DWORD g_com_vt[40];
DWORD g_back[4], g_palette_object[4];

// ---- Stand-ins ------------------------------------------------------------------------------------------------------

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a8a90, CV_THIS, 0, K_VALUE, "CFrame::Ctor"},
    {0x4173c0, CV_THIS, 0, K_VALUE, "Object_InitFields"},
    {0x4aac20, CV_THIS, 0, K_FIXED4, "CFrame::Palette"},
    {0x5678c0, CV_CDECL, 0, K_VALUE, "ScreenFrame_Release"},
    {0x4a8480, CV_THIS, 0, K_VALUE, "RowTable_Free"},
    {0x4173f0, CV_THIS, 0, K_VALUE, "Object_FreeFields"},
    {0x4a8b00, CV_THIS, 0, K_VALUE, "CFrame::Dtor"},
    {0x415780, CV_THIS, 9, K_VALUE, "ClassInfo_Register"},
    {0x4a8760, CV_CDECL, 0, K_VALUE, "CFrame_ClassInfo"},
    {0x567bb0, CV_THIS, 0, K_VALUE, "ScreenFrame_Lock"},
    {0x5693a0, CV_THIS, 0, K_VALUE, "ScreenFrame_Flip"},
    {0x4a8bb0, CV_THIS, 3, K_VALUE, "CFrame::SetPixel"},
    {0x4ab120, CV_THIS, 2, K_VALUE, "CFrame::GetPixel"},
    {0x4abae0, CV_THIS, 5, K_VALUE, "CFrame::DrawLine"},
    {0x4abe60, CV_THIS, 5, K_VALUE, "CFrame::DrawThickLine"},
    {0x4b3430, CV_THIS, 2, K_VALUE, "Frame_DrawRect"},
    {0x4b34b0, CV_THIS, 3, K_VALUE, "Frame_DrawRectTwoColors"},
    {0x4ac710, CV_THIS, 2, K_VALUE, "CFrame::TintRect"},
    {0x4a8f40, CV_THIS, 4, K_BOOL, "CFrame::Blit"},
    {0x4432d0, CV_THIS, 1, K_VALUE, "CBlendScreenUp555_SetLevel"},
    {0x4433c0, CV_THIS, 1, K_VALUE, "CBlendScreenDown555_SetLevel"},
    {0x444f60, CV_THIS, 1, K_VALUE, "CBlendScreenUp565_SetLevel"},
    {0x445090, CV_THIS, 1, K_VALUE, "CBlendScreenDown565_SetLevel"},
    {0x4b2cd0, CV_FAST, 0, K_BOOL, "Rect_Intersect", "rr"},
    {0x53f160, CV_THIS, 0, K_SMALL, "FieldColor_PaletteIndex"},
    {0x53f010, CV_FAST, 4, K_VALUE, "FieldColor_CtorRGBA", "xxvvvv"},
    {0x4aa390, CV_THIS, 1, K_VALUE, "CFrame_SetPalette"},
    {0x5268f0, CV_THIS, 1, K_VALUE, "Palette_Resize"},
    {0x526fd0, CV_THIS, 4, K_VALUE, "Palette_SetEntry"},
    {0x433a90, CV_THIS, 0, K_VALUE, "Pixel16_BuildTables"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "Tick"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4cdc70, CV_CDECL, 0, K_VALUE, "App_DefaultWidth"},
    {0x4cdc80, CV_CDECL, 0, K_VALUE, "App_DefaultHeight"},
    {0x4cdc90, CV_CDECL, 0, K_VALUE, "App_DefaultDepth"},
    {0x4a4670, CV_FAST, 2, K_VALUE, "Ini_GetInt", "vvsv"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vvsv"},
    {0x4a46e0, CV_FAST, 2, K_FLOAT, "Ini_GetFloat", "vvsv"},
    {0x4d2220, CV_FAST, 0, K_VALUE, "CommandLine_Int", "sv"},
    {0x567f20, CV_THIS, 4, K_VALUE, "ScreenFrame_SetMode", "aavv"},
    {0x4ce500, CV_THIS, 1, K_VALUE, "App::Set24"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// The set mode stand-in: the first call fails (width 0) in some cases, a 24-bit failure's 32-bit retry succeeds
int g_mode_calls;
bool g_mode_fails;

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x567bb0:   // Lock: locked in about half the cases
        if (answer & 1)
            *at<DWORD *>(LOCKED) = 1;
        break;
    case 0x567f20:
        if (args[0]) {
            const bool fail = g_mode_calls == 0 && g_mode_fails && args[2] == 0x18;
            *at<DWORD *>(args[0]) = fail ? 0 : 1 + (answer & 0x7ff);
        }
        g_mode_calls++;
        break;
    default: break;
    }
}

// ---- The case -----------------------------------------------------------------------------------------------------

constexpr int W_SELF = 0, W_REPORTER = 1, W_SCREEN = 2, W_FRAME = 3;
BYTE *g_holder;   // the screen's palette holder (slot 58): {data, ...} with 0x300 bytes of data

BYTE *palette_holder(Gen &g)
{
    BYTE *h = xalloc(0x10);
    BYTE *d = xalloc(0x300);
    for (DWORD i = 0; i < 0x300; i++)
        d[i] = (BYTE)g.next();
    at_(h, 0) = ptrv(d);
    at_(h, 4) = 0x100;
    return h;
}

float some_float(Gen &g)
{
    const DWORD k = g.below(8);
    const float specials[4] = {0.0f, 1.0f, 0.001f, 5.0f};
    if (k < 4)
        return specials[k];
    if (k == 4) {
        float f;
        const DWORD b = g.next();
        memcpy(&f, &b, 4);
        return f;
    }
    return (float)g.range(-200, 3000) / 997.0f;
}

void base(Gen &g)
{
    if (!g_com_vt[0]) {
        for (DWORD &v : g_com_vt)
            v = ptrv((void *)&com_c0);
        g_com_vt[5] = ptrv((void *)&com_blt);
        g_com_vt[6] = ptrv((void *)&com_entries);
        g_com_vt[32] = ptrv((void *)&com_unlock);
        g_back[0] = g_palette_object[0] = ptrv(g_com_vt);
    }
    g_case.effect = &effect;
    g_case.nwin = 1;
    g_mode_calls = 0;
    g_mode_fails = g.chance(3);
    g_all_entries = true;
    make_window(W_SELF, C_CFRAME);
    make_window(W_REPORTER, C_OBJECT);
    make_window(W_SCREEN, C_CFRAME);
    make_window(W_FRAME, C_OBJECT);
    BYTE *s = win(W_SELF);
    for (DWORD o = 4; o < 0xe8; o += 4)
        if (o != 0x50)   // (the window's title: a ZString of the harness)
            at_(s, o) = g.next();
    at_(s, 0x40) = g.below(4);
    s[0xcc] = (BYTE)g.below(2);
    at_<float>(s, 0xd0) = g.chance(3) ? 1.0f : some_float(g);
    at_(s, 0xd4) = g.chance(6) ? g.next() : g.below(3);
    at_<float>(s, 0xd8) = some_float(g);
    at_<float>(s, 0xdc) = g.chance(3) ? 1.0f : some_float(g);
    at_<float>(s, 0xe0) = some_float(g);
    at_<float>(s, 0xe4) = some_float(g);
    at_(win(W_REPORTER), 4) = ptrv(win(W_SCREEN));
    g_holder = palette_holder(g);
    g_case.fixed[4] = ptrv(g_holder);
    set_kind(C_CFRAME, 58, K_FIXED4);
    set_kind(C_CFRAME, 29, K_FIXED0);
    set_kind(C_CFRAME, 38, K_FIXED1);
    const DWORD depths[5] = {8, 16, 24, 32, 0x10};
    g_case.fixed[0] = g.chance(8) ? g.below(40) : depths[g.below(5)];
    g_case.fixed[1] = g.chance(8) ? g.next() : (g_case.fixed[0] == 8 ? 0x20 : g_case.fixed[0] == 16 ? 0x40 : g_case.fixed[0] == 24 ? 0x80 : 0x100) | (g.next() & ~0x1e0u);
    set_kind(C_CFRAME, 45, g.chance(2) ? K_FIXED2 : K_VALUE);
    set_kind(C_CFRAME, 39, g.chance(2) ? K_FIXED3 : K_VALUE);
    g_case.fixed[2] = g.chance(2) ? 0x280 : g.below(2000);
    g_case.fixed[3] = g.chance(2) ? 0x1e0 : g.below(2000);
    g_case.argspec[36][0] = A_IGNORE;   // (a rectangle local of the caller, written by the slot)
    g_case.argspec[28][1] = A_IGNORE;   // (ClearPages' black: a local the colour constructor stand-in leaves)
    case_global(0x6c02cc, ptrv(win(W_REPORTER)));
    case_global(LOCKED, g.below(2));
    case_global(BACK, ptrv(g_back));
    case_global(PALETTE_OBJECT, g.chance(4) ? 0 : ptrv(g_palette_object));
    case_global(FULL_SCREEN, g.chance(3) ? 0 : 1);
    case_global(0x713278, ptrv(xalloc(0x300)));
    case_global(0x713288, ptrv(xalloc(0x300)));
    case_global(0x713298, ptrv(xalloc(0x300)));
    case_global(0x713320, ptrv(xalloc(0x300)));
    case_global(0x633388, g.below(2));
    g_self = s;
}

// The frame slots' clip rectangle (slot 36) is written through its argument
void rect_effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_CFRAME && slot == 36 && args[0]) {
        Gen &v = g_case.values;
        int *r = at<int *>(args[0]);
        r[0] = v.range(-10, 50), r[1] = v.range(-10, 50), r[2] = r[0] + v.range(0, 700), r[3] = r[1] + v.range(0, 500);
        return;
    }
    effect(cls, slot, args, answer);
}

int *rect(Gen &g)
{
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 700), r[1] = g.range(-50, 500), r[2] = r[0] + g.range(-10, 300), r[3] = r[1] + g.range(-10, 300);
    return r;
}

void s_none(Gen &) {}
void s_ctor(Gen &)
{
    g_self = xalloc(0xe8);
}
void s_args(Gen &g)
{
    g_case.effect = &rect_effect;
    for (int i = 0; i < 6; i++)
        g_args[i] = g.chance(3) ? ptrv(xalloc(0x10)) : g.next();
}
void s_dtor(Gen &g)
{
    BYTE *mem = xalloc(4 + 3 * 0xe8);
    const DWORD n = g.below(4);
    at_(mem, 0) = n;
    g_self = g.chance(2) ? mem + 4 : win(W_SELF);
    g_args[0] = g.below(4);
}
void s_class(Gen &g)
{
    case_global(0x71332c, g.chance(2) ? 0 : g.next());
}
void s_create_array(Gen &g)
{
    g_self = at<BYTE *>(g.below(4));   // (ecx: the count)
    g_args[0] = g.next();
}
void s_fade(Gen &g)
{
    const float dt = some_float(g);
    memcpy(&g_args[0], &dt, 4);
}
void s_modes(Gen &g)
{
    BYTE *s = win(W_SCREEN);
    const DWORD n = g.below(12);
    at_(s, 0x4c) = n;
    const DWORD sizes[4] = {0x280, 0x320, 0x400, 0x1e0};
    for (DWORD i = 0; i < n; i++) {
        at_(s, 0x54 + i * 0xc) = g.chance(2) && i ? at_(s, 0x54 + (i - 1) * 0xc) : sizes[g.below(4)];
        at_(s, 0x58 + i * 0xc) = g.chance(2) && i ? at_(s, 0x58 + (i - 1) * 0xc) : sizes[g.below(4)] * 3 / 4;
        at_(s, 0x5c + i * 0xc) = g.below(4) * 8 + 8;
    }
    g_self = s;
    DWORD *w = reinterpret_cast<DWORD *>(xalloc(8));
    w[0] = sizes[g.below(4)] + (g.chance(4) ? g.below(3) : 0);
    w[1] = w[0] * 3 / 4 + (g.chance(4) ? g.below(3) : 0);
    g_args[0] = ptrv(w);
    g_args[1] = ptrv(w + 1);
}
void s_palette(Gen &g)
{
    g_args[0] = g.chance(8) ? ptrv(xalloc(8)) : ptrv(palette_holder(g));
    g_args[1] = g.chance(2) ? 1 : (g.next() & 0xffffff00u);
    g_all_entries = (g_args[1] & 0xff) != 0;
}
void s_fade_to(Gen &g)
{
    g_case.effect = &rect_effect;
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_FRAME));
    g_args[1] = g.chance(4) ? 0 : ptrv(rect(g));
    g_args[2] = g.chance(4) ? 0 : ptrv(rect(g));
    g_args[3] = g.chance(4) ? 0 : 0x6bb4e0;
    g_args[4] = g.chance(8) ? ptrv(xalloc(8)) : ptrv(palette_holder(g));
    g_args[5] = g.below(600);
    g_args[6] = 1;   // (the screen image and the fade out keep the system colours' flag 1)
    g_all_entries = true;
}
void s_fade_out(Gen &g)
{
    s_fade_to(g);
    g_args[4] = g_args[5];
}
void s_brightness(Gen &g)
{
    const float v = some_float(g);
    memcpy(&g_args[0], &v, 4);
}
void s_blit(Gen &g)
{
    s_args(g);
    g_args[3] = g.chance(2) ? 0x6bb4e0 : g.next();
}
void s_fill(Gen &g)
{
    g_case.effect = &rect_effect;
    g_args[0] = ptrv(rect(g));
    BYTE *c = xalloc(0x10);
    for (int i = 0; i < 0x10; i++)
        c[i] = (BYTE)g.next();
    g_args[1] = ptrv(c);
}
void s_display_init(Gen &g)
{
    const DWORD depths[4] = {0, 0x10, 0x18, 0x20};
    g_args[0] = depths[g.below(4)];
    g_args[1] = g.chance(2) ? 0 : 0x280;
    g_args[2] = g.chance(2) ? 0 : 0x1e0;
    case_global(0x713bd0, depths[g.below(4)]);
    // a failed first mode is only retried at 24 bits: the other depths would end the game
    if (g_args[0] ? g_args[0] != 0x18 : true)
        g_mode_fails = false;
}
void s_display_640(Gen &g)
{
    case_global(0x713bd0, g.chance(2) ? 0x18 : 0x10);
    (void)g;
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, LOCKED, BACK, PALETTE_OBJECT, 0x621118, 0x621188, 0x71332c, 0x713bd0, 0x713278, 0x713288, 0x713298, 0x713320,
                              0x633388};

const Test TESTS[] = {
    {"ScreenFrame Create", 0x567630, CALL_CDECL, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"ScreenFrame Ctor", 0x567990, CALL_THIS, 0, ~0u, 30, s_ctor, STUBS, NSTUBS},
    {"ScreenFrame CreateArray", 0x567650, CALL_THIS, 1, ~0u, 40, s_create_array, STUBS, NSTUBS},
    {"ScreenFrame DeletingDtor", 0x5676b0, CALL_THIS, 1, ~0u, 100, s_dtor, STUBS, NSTUBS},
    {"ScreenFrame ClassInfo", 0x567760, CALL_CDECL, 0, ~0u, 20, s_class, STUBS, NSTUBS},
    {"ScreenFrame UpdateFade", 0x567ab0, CALL_THIS, 1, 0, 600, s_fade, STUBS, NSTUBS},
    {"ScreenFrame IsFullScreen", 0x567cc0, CALL_CDECL, 0, 0xff, 10, s_none, STUBS, NSTUBS},
    {"ScreenFrame FindMode", 0x567cd0, CALL_THIS, 2, ~0u, 400, s_modes, STUBS, NSTUBS},
    {"ScreenFrame DropDuplicateModes", 0x567e90, CALL_CDECL, 0, 0, 300, s_modes, STUBS, NSTUBS},
    {"ScreenFrame SetPalette", 0x568870, CALL_THIS, 2, 0, 150, s_palette, STUBS, NSTUBS},
    {"ScreenFrame FadeTo", 0x568660, CALL_THIS, 7, 0, 150, s_fade_to, STUBS, NSTUBS},
    {"ScreenFrame FadeOut", 0x568630, CALL_THIS, 5, 0, 60, s_fade_out, STUBS, NSTUBS},
    {"ScreenFrame SetBrightness", 0x569520, CALL_THIS, 1, 0, 60, s_brightness, STUBS, NSTUBS},
    {"ScreenFrame SetPixel", 0x568ac0, CALL_THIS, 3, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame GetPixel", 0x568c90, CALL_THIS, 2, ~0u, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame DrawLine", 0x568d00, CALL_THIS, 5, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame DrawThickLine", 0x568d90, CALL_THIS, 5, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame DrawRect", 0x568e20, CALL_THIS, 2, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame DrawRectTwoColors", 0x568e90, CALL_THIS, 3, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame TintRect", 0x569210, CALL_THIS, 2, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame Blit", 0x568b40, CALL_THIS, 4, 0xff, 600, s_blit, STUBS, NSTUBS},
    {"ScreenFrame FillRect", 0x568f10, CALL_THIS, 2, 0, 600, s_fill, STUBS, NSTUBS},
    {"ScreenFrame ClearRect", 0x5692b0, CALL_THIS, 1, 0, 200, s_fill, STUBS, NSTUBS},
    {"ScreenFrame FillAll", 0x5691e0, CALL_THIS, 1, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame ClearAll", 0x569280, CALL_THIS, 0, 0, 60, s_args, STUBS, NSTUBS},
    {"ScreenFrame ClearPages", 0x5694b0, CALL_THIS, 0, 0, 100, s_args, STUBS, NSTUBS},
    {"Display Init", 0x590ce0, CALL_FAST, 3, 0, 300, s_display_init, STUBS, NSTUBS},
    {"Display Set640x480", 0x590f00, CALL_CDECL, 0, 0, 100, s_display_640, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(screen_frame)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        g_globals.add(FULL_SCREEN, 1);
    }
    Tally t{"screen frame: CScreenFrame's drawing slots, fills, fades, palettes and modes, the display setup"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 47000);
    t.report();
}

}  // namespace zax
