// Checks of the Plasma Ground's drawing (tile_map_draw.cpp) against the original with the toolkit's harness
// (checks_ui_common.h, checks_ui_screens.h): the tile map is random memory in the extra area with its grid, cell
// sizes and shifts, tile cache handles, band entries and tables; frames, the two embedded grid frames (+0x2c6c,
// +0x2cac), the marks (+8), the layer (+0x2d44) and the textures are fakes of the frame class (slot answers of the
// case; depth, row table and pixel base fixed per case), the band frame of 0x541250 a game object fake. The
// exe's static frames (0x711078, 0x6d0ff0, 0x711030, 0x6d0ba8) get the fake frame vtable for the case, their
// fields and the guard bits are case values. The tile cache (alloc, lock, unlock, reset), the frame constructor,
// pitch, depth, blit, byte, the plasma functions and the random generator, the layer's tile drawing and atexit are
// stand-ins (the colours are the exe's FieldColor code; the static colours are case values); the texture list of 0x541710 is a fake list. 0x541250 computes real pixels: band
// frame, destination and the two texture buffers lie in the extra area (16-bit and 24-bit mixes with weights up
// to 300 and beyond, 8 bits through the game's blend tables 0x6767c0/0x6767d0). Every case runs from a cleared
// stack area (the local colour of the grid's mode 2 is left unset by the stand-in).
#include "checks_ui_screens.h"

namespace zax {

int hooks_tile_map_draw(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"tile_map_draw", &hooks_tile_map_draw};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4aa2c0, CV_THIS, 2, K_VALUE, "CFrame_SetPitch"},
    {0x4aa240, CV_THIS, 1, K_VALUE, "CFrame_SetDepth"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize"},
    {0x4a8b60, CV_THIS, 3, K_VALUE, "CFrame_SetByte"},
    {0x55d030, CV_CDECL, 0, K_FIXED6, "ColorDepth"},
    {0x4d2680, CV_THIS, 3, K_VALUE, "Handle_Alloc"},
    {0x4d2760, CV_THIS, 0, K_VALUE, "HandleHeap_Lock"},
    {0x4d27a0, CV_THIS, 0, K_VALUE, "HandleHeap_Unlock"},
    {0x4d2bc0, CV_THIS, 1, K_VALUE, "HandleHeap_DataUnlock"},
    {0x4d2b50, CV_THIS, 0, K_VALUE, "HandleHeap::Reset"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4b3430, CV_THIS, 2, K_VALUE, "Frame_DrawRect", "rv"},
    {0x55c430, CV_THIS, 1, K_VALUE, "Random_Assign"},
    {0x49f9d0, CV_FAST, 1, K_VALUE, "Plasma_Line"},
    {0x49fe10, CV_FAST, 2, K_VALUE, "Plasma_Column"},
    {0x49fad0, CV_FAST, 0, K_VALUE, "Plasma_Map"},
    {0x4e8b60, CV_THIS, 4, K_VALUE, "Layer_DrawTile"},
    {0x4e8ce0, CV_THIS, 3, K_VALUE, "Layer_DrawTileOnce"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_F = 1, W_TEX = 2, W_BANDS = 3, W_LAYER = 4, W_TEX2 = 6;

// The texture list of 0x541710: names, loading gives a texture fake
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7901, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD)
{
    log_event(0x7903, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    return ptrv(win(g_case.values.chance(2) ? W_TEX : W_TEX2));
}
DWORD THISCALL list_other(BYTE *) { return 0; }

BYTE *g_list;   // the case's texture list (records at +0x30)
char g_object_args[MAXSLOT + 1];
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)answer;
    if (cls == C_CFRAME && slot == 36 && args[0]) {   // the clip rectangle
        Gen &v = g_case.values;
        int *r = at<int *>(args[0]);
        r[0] = v.range(-80, 300), r[1] = v.range(-80, 300), r[2] = r[0] + v.range(-10, 300), r[3] = r[1] + v.range(-10, 300);
    } else if (cls == -1 && (DWORD)slot == 0x55e090 && g_list) {   // LoadRecord fills the record
        BYTE *recs = bp(at_(g_list, 0x30));
        if (recs && args[0] < at_(g_list, 0x34))
            at_(recs, args[0] * 0x80 + 0x78) = ptrv(win(W_TEX2));
    }
}

float rnd_float(Gen &g)
{
    if (g.chance(12)) {
        float f;
        const DWORD b = g.next();
        memcpy(&f, &b, 4);
        return f;
    }
    return (float)g.range(0, 1000) / 1000.0f;
}

constexpr DWORD STATIC_FRAMES[4] = {0x711078, 0x6d0ff0, 0x711030, 0x6d0ba8};

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[1] = ptrv((void *)&list_name);
        g_list_vt[37] = ptrv((void *)&list_load);
    }
    screen_base(g, &effect);
    for (int i = W_F; i <= W_TEX2; i++)
        make_window(i, C_CFRAME);
    make_window(W_BANDS, C_OBJECT);
    object_args(g_object_args, "31:0,46:0");
    g_case.argspec[36][0] = A_IGNORE;   // (slot 36 writes the caller's uninitialised rectangle)
    static const int SMALL[] = {29, 30, 39, 45, 41, 42, 54};
    for (int s : SMALL)
        set_kind(C_CFRAME, s, K_SMALL);
    set_kind(C_CFRAME, 29, K_FIXED0);
    set_kind(C_CFRAME, 46, K_FIXED1);
    set_kind(C_CFRAME, 31, K_FIXED2);
    set_kind(C_OBJECT, 46, K_FIXED3);
    set_kind(C_OBJECT, 31, K_FIXED4);
    const DWORD depths[5] = {8, 16, 24, 32, 8};
    g_case.fixed[0] = depths[g.below(5)];
    g_case.fixed[6] = g.chance(2) ? 8 : 16;
    // the shared row table of the frame fakes (128 rows, pitch 16)
    DWORD *rows = reinterpret_cast<DWORD *>(xalloc(128 * 4));
    for (DWORD i = 0; i < 128; i++)
        rows[i] = i * 16;
    g_case.fixed[1] = ptrv(rows);
    g_case.fixed[2] = ptrv(xalloc(16 * 16 * 3 + 64));
    DWORD *brows = reinterpret_cast<DWORD *>(xalloc(16 * 4));
    for (DWORD i = 0; i < 16; i++)
        brows[i] = i * 16;
    g_case.fixed[3] = ptrv(brows);
    BYTE *bands = xalloc(16 * 16 + 16);
    for (DWORD i = 0; i < 16 * 16 + 16; i++)
        bands[i] = (BYTE)g.below(4);
    g_case.fixed[4] = ptrv(bands);
    BYTE *dst = bp(g_case.fixed[2]);
    for (DWORD i = 0; i < 16 * 16 * 3 + 64; i++)
        dst[i] = (BYTE)g.next();
    // the exe's static frames: the fake frame vtable, fields of the case
    const DWORD vt = at_(win(W_F), 0);
    for (DWORD f : STATIC_FRAMES) {
        case_global(f, vt);
        case_global(f + 0xc, g.chance(2) ? 0 : g.next());
    }
    case_global(0x711088, g.next());
    case_global(0x71108c, g.next());
    case_global(0x6d1000, g.next());
    case_global(0x6d1004, g.next());
    case_global(0x711040, g.next());
    case_global(0x711044, g.next());
    case_global(0x6d0bb8, g.chance(2) ? 0 : g.next());
    case_global(0x711070, g.next() & 0x03030303u);
    case_global(0x711074, g.below(2));
    case_global(0x7110e4, g.below(2));
    g_list = nullptr;
}

// The case's tile map: grid, cells, handles, embedded frames, band entries and tables
BYTE *new_tile_map(Gen &g)
{
    BYTE *t = xalloc(0x2d48);
    for (DWORD o = 0; o < 0x2d48; o += 4)
        at_(t, o) = g.next();
    const DWORD vt = at_(win(W_F), 0);
    at_(t, 8) = vt;
    at_(t, 0x2c6c) = vt;
    at_(t, 0x2cac) = vt;
    at_(t, 0x2d44) = ptrv(win(W_LAYER));
    const DWORD cols = 2 + g.below(4), rows = 2 + g.below(4);
    at_(t, 0x2d0c) = cols;
    at_(t, 0x2d10) = rows;
    const DWORD sizes[4] = {8, 16, 32, 64};
    at_(t, 0x2d04) = sizes[g.below(4)];
    at_(t, 0x2d08) = sizes[g.below(4)];
    at_(t, 0x2d1c) = g.below(7);
    at_(t, 0x2d20) = g.below(7);
    DWORD *handles = reinterpret_cast<DWORD *>(xalloc(3 * cols * rows * 4));
    for (DWORD i = 0; i < 3 * cols * rows; i++)
        handles[i] = g.chance(2) ? 0 : g.next();
    at_(t, 0x2d24) = ptrv(handles);
    at_(t, 0x2d2c) = g.below(5);
    at_(t, 0x2d30) = g.below(5);
    at_(t, 0x2d34) = g.below(5);
    at_(t, 0x2cf8) = g.below(3);
    at_(t, 0x2cec) = g.below(4);
    at_(t, 0x2cf0) = g.below(cols);
    at_(t, 0x2cf4) = g.below(rows);
    t[0x60] = (BYTE)g.below(2);
    // the band entries 0..31 (elevations are 0..20): texture W_TEX or null, second texture, blend table
    for (DWORD b = 0; b < 32; b++) {
        BYTE *e = t + 0x106c + b * 0x1c;
        at_(e, 0) = g.chance(4) ? 0 : ptrv(win(g.chance(3) ? W_TEX2 : W_TEX));
        at_(e, 0xc) = g.chance(2) ? 0 : ptrv(win(W_TEX2));
        at_(e, 0x18) = g.chance(3) ? 0x6767c0 : 0;
    }
    at_(t, 0x106c) = ptrv(win(W_TEX));
    // the texture list (empty: 0x541250 returns at once)
    g_list = xalloc(0x40);
    at_(g_list, 0) = ptrv(g_list_vt);
    at_(g_list, 0x30) = 0;
    at_(g_list, 0x34) = 0;
    at_(t, 0x1064) = ptrv(g_list);
    return t;
}

void s_map_bands(Gen &g)
{
    BYTE *t = new_tile_map(g);
    at_(g_list, 0x34) = g.chance(8) ? 0 : 1 + g.below(5);
    BYTE *t1 = xalloc(1400), *t2 = xalloc(1400);
    for (DWORD i = 0; i < 1400; i++)
        t1[i] = (BYTE)g.next(), t2[i] = (BYTE)g.next();
    const DWORD tables[2] = {*at<DWORD *>(0x6767c0), *at<DWORD *>(0x6767d0)};
    for (DWORD b = 0; b < 4; b++) {
        at_(t, 0x64 + b * 4) = ptrv(t1 + g.below(32));
        at_(t, 0x464 + b * 4) = g.chance(3) ? 0 : ptrv(t2 + g.below(32));
        at_(t, 0x864 + b * 4) = tables[g.below(2)];
        at_(t, 0xc64 + b * 4) = g.chance(6) ? g.next() : g.below(300);
    }
    if (!tables[0] || !tables[1])   // (no 8-bit blend tables: no second textures in 8 bits)
        for (DWORD b = 0; b < 4; b++)
            if (g_case.fixed[0] == 8)
                at_(t, 0x464 + b * 4) = 0;
    g_self = t;
    g_args[0] = ptrv(win(W_F));
    g_args[1] = ptrv(win(W_BANDS));
    g_args[2] = g.below(24) + (g.chance(4) ? 0x80 * g.below(3) : 0);
    g_args[3] = g.below(9) + (g.chance(4) ? 0x80 * g.below(3) : 0);
    g_args[4] = g.below(17);
    g_args[5] = g.below(17);
}
void s_build_bands(Gen &g)
{
    BYTE *t = new_tile_map(g);
    const DWORD n = g.chance(8) ? 0 : 1 + g.below(5);
    BYTE *recs = xalloc(6 * 0x80);
    for (DWORD i = 0; i < 6; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : ptrv(win(g.chance(2) ? W_TEX : W_TEX2));
        recs[i * 0x80 + 0x7f] = 0;
    }
    at_(g_list, 0x30) = ptrv(recs);
    at_(g_list, 0x34) = n;
    for (DWORD b = 0; b < 0x100; b++) {   // entries of the bands not set by an 8-bit pass keep valid textures
        BYTE *e = t + 0x106c + b * 0x1c;
        at_(e, 0) = ptrv(win(W_TEX));
        at_(e, 0xc) = g.chance(2) ? 0 : ptrv(win(W_TEX2));
        at_(e, 0x18) = g.chance(2) ? 0 : 0x6767d0;
    }
    at_<float>(t, 0x1068) = g.chance(5) ? rnd_float(g) : (float)g.range(1, 1000) / 1000.0f;
    g_self = t;
    g_args[0] = g.below(3);
}
void s_grid(Gen &g)
{
    BYTE *t = new_tile_map(g);
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 300), r[1] = g.range(-50, 300), r[2] = g.range(-50, 300), r[3] = g.range(-50, 300);
    g_self = t;
    g_args[0] = g.below(at_(t, 0x2d0c));
    g_args[1] = g.below(at_(t, 0x2d10));
    g_args[2] = ptrv(win(W_F));
    g_args[3] = ptrv(r);
    case_global(0x711070, g.next() & 0x03030303u);
}
void cell(Gen &g, BYTE *t)
{
    g_self = t;
    g_args[0] = g.below(at_(t, 0x2d0c) - 1);
    g_args[1] = g.below(at_(t, 0x2d10) - 1);
}
void s_build(Gen &g)
{
    BYTE *t = new_tile_map(g);
    cell(g, t);
    g_args[2] = ptrv(win(W_F));
}
void s_ground(Gen &g)
{
    BYTE *t = new_tile_map(g);
    cell(g, t);
    g_args[2] = g_args[1];
    g_args[1] = g_args[0];
    g_args[0] = ptrv(win(W_F));
}
void s_layer(Gen &g)
{
    BYTE *t = new_tile_map(g);
    cell(g, t);
    g_args[2] = ptrv(win(W_F));
    g_args[3] = g.below(4);
}
void s_prepare(Gen &g)
{
    BYTE *t = new_tile_map(g);
    g_self = t;
    g_args[0] = ptrv(win(W_F));
    g_args[1] = at_(t, 0x2d24) + 4 * g.below(at_(t, 0x2d0c));
}
int *rect_or_null(Gen &g)
{
    if (g.chance(3))
        return nullptr;
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-80, 300), r[1] = g.range(-80, 300), r[2] = r[0] + g.range(-10, 300), r[3] = r[1] + g.range(-10, 300);
    return r;
}
void s_region(Gen &g)
{
    BYTE *t = new_tile_map(g);
    g_self = t;
    g_args[0] = ptrv(win(W_F));
    g_args[1] = ptrv(rect_or_null(g));
    g_args[2] = g.below(2) | (g.next() & 0xffffff00u);
}
void s_regions(Gen &g)
{
    BYTE *t = new_tile_map(g);
    g_self = t;
    g_args[0] = ptrv(win(W_F));
    g_args[1] = ptrv(rect_or_null(g));
}

// Every case runs from a cleared stack area
__attribute__((noinline)) void clear_stack()
{
    volatile BYTE area[0x1400];
    for (int i = (int)sizeof area - 1; i >= 0; i--)
        area[i] = 0;
}
template <DWORD A, typename... Args> DWORD THISCALL from_clear_stack(BYTE *self, Args... a)
{
    clear_stack();
    at<void(THISCALL *)(BYTE *, Args...)>(A)(self, a...);
    return 0;
}
template <DWORD A, typename... Args> DWORD fn() { return ptrv((void *)&from_clear_stack<A, Args...>); }

constexpr DWORD GLOBALS_TILE[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x711078, 0x6d0ff0, 0x711030, 0x6d0ba8, 0x711084, 0x6d0ffc,
                                  0x71103c, 0x6d0bb4, 0x711088, 0x71108c, 0x6d1000, 0x6d1004, 0x711040, 0x711044, 0x6d0bb8,
                                  0x711070, 0x7110b8, 0x7110bc, 0x7110c0, 0x7110c4, 0x7110c8, 0x7110cc, 0x7110d0, 0x7110d4};

}  // namespace

ZAX_CHECKS(tile_map_draw)
{
    (void)cls;
    (void)n;
    using D = DWORD;
    const Test tests[] = {
        {"CPlasmaTileMap MapBands", fn<0x541250, D, D, D, D, D, D>(), CALL_THIS, 6, 0, 1500, s_map_bands, STUBS, NSTUBS},
        {"CPlasmaTileMap BuildBands", fn<0x541710, D>(), CALL_THIS, 1, 0, 150, s_build_bands, STUBS, NSTUBS},
        {"CPlasmaTileMap DrawGrid", fn<0x541090, D, D, D, D>(), CALL_THIS, 4, 0, 300, s_grid, STUBS, NSTUBS},
        {"CPlasmaTileMap BuildTile", fn<0x540d90, D, D, D>(), CALL_THIS, 3, 0, 300, s_build, STUBS, NSTUBS},
        {"CPlasmaTileMap DrawGround", fn<0x540840, D, D, D>(), CALL_THIS, 3, 0, 500, s_ground, STUBS, NSTUBS},
        {"CPlasmaTileMap DrawLayer", fn<0x540b80, D, D, D, D>(), CALL_THIS, 4, 0, 500, s_layer, STUBS, NSTUBS},
        {"CPlasmaTileMap PrepareFrame", fn<0x540790, D, D>(), CALL_THIS, 2, 0, 200, s_prepare, STUBS, NSTUBS},
        {"CPlasmaTileMap DrawRegion", fn<0x540000, D, D, D>(), CALL_THIS, 3, 0, 300, s_region, STUBS, NSTUBS},
        {"CPlasmaTileMap DrawRegions", fn<0x53ff90, D, D>(), CALL_THIS, 2, 0, 200, s_regions, STUBS, NSTUBS},
    };
    if (!g_globals.n) {
        for (DWORD a : GLOBALS_TILE)
            g_globals.add(a, 4);
        g_globals.add(0x711074, 1);
        g_globals.add(0x7110e4, 1);
    }
    Tally t{"tile map draw: the Plasma Ground's drawing"};
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 61000);
    t.report();
}

}  // namespace zax
