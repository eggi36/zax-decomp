// Checks of world_frame.cpp against the original, in the game thread.
//
// Every case runs on both sides from the same seed (side 0 with the module's hooks out) on a fake world in
// the check's own memory: the world (its vtable recording stand-ins for the slots the frame code calls),
// the net layer with player records (packets, zoom, layer index, entity), the slot 1 object (the player
// count), layers, frames (display, zoom frame, the HUD frame, the menu background), the traffic graph's
// owner, the application's window and, for 0x4f4ae0, a part list with flag slots and the two predicates.
// Every exe function the code calls outside the module is a recording stand-in (the timers, the cursor,
// the music, the listener, PlayerLayer, the static proxy's and the layers' frame functions, the packets,
// TexturesOn, the window, the traffic graph, the frame sizes and blits, the layer update, the screen shot
// keys and file, FindRect, the tile map, the draw pass, atexit). The function-local statics 0x6c2040 and
// 0x6c2548 with their guards and the globals 0x6c28e8, 0x60a528, 0x6c1cf4, 0x6c2908 are set for the case
// (guards both ways) and put back. Compared: the event log (pointers into the fakes by offset, the stack
// as one value), the fakes' bytes and eax (al for slot 4).
#include "check_util.h"

namespace zax {

int hooks_world_frame(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename F> DWORD fn_address(F f) { return (DWORD)(uintptr_t)f; }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }

// ---- The hooks out (side 0) --------------------------------------------------------------------------------

constexpr int MAX_SITES = 16;
Hook g_list[MAX_SITES];
BYTE *g_sites[MAX_SITES];
BYTE g_saved[MAX_SITES][5];
int g_nsites;
bool g_fits = true;
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_world_frame(nullptr);
    g_fits = hook_sites_fit("world_frame", n, MAX_SITES);
    if (!g_fits)
        return;
    hooks_world_frame(g_list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(g_list[i].address);
        if (original_address(g_list[i].address) == site)
            continue;
        g_sites[g_nsites] = site;
        memcpy(g_saved[g_nsites], site, 5);
        write_code(site, g_list[i].expected);
        g_nsites++;
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        write_code(g_sites[i], g_saved[i]);
    g_nsites = 0;
}

// ---- The fakes ---------------------------------------------------------------------------------------------

constexpr DWORD MEM = 0x1000;
alignas(16) BYTE g_mem[2][MEM];
int g_side;
BYTE *m(DWORD off) { return g_mem[g_side] + off; }
constexpr DWORD O_WORLD = 0, O_NL = 0x340, O_SLOT1 = 0x350, O_WINDOW = 0x370, O_GRAPH = 0x380, O_DISPLAY = 0x390, O_ZOOM = 0x3a0, O_FRAME = 0x3b0,
                O_BG = 0x3c0, O_E0 = 0x3d0, O_PLAYERS = 0x400, O_LAYERS = 0x800, O_LAYER_ARRAY = 0xe00, O_PARTS = 0xe10, O_LIST = 0xf10,
                O_LIST_DATA = 0xf20, O_RC = 0xf40, O_MIXER = 0xf50;
constexpr DWORD STACK_TAG = 0xb5000000u;
DWORD g_stack_lo, g_stack_hi;
DWORD tag(DWORD v)
{
    if (v >= ptrv(g_mem[g_side]) && v < ptrv(g_mem[g_side]) + MEM)
        return 0xa0000000u | (v - ptrv(g_mem[g_side]));
    if (v >= ptrv(g_mem[1 - g_side]) && v < ptrv(g_mem[1 - g_side]) + MEM)
        return 0xa8000000u | (v - ptrv(g_mem[1 - g_side]));
    if (v >= g_stack_lo && v < g_stack_hi)
        return STACK_TAG;
    return v;
}
DWORD tagp(const void *p) { return tag(ptrv(p)); }

// The event log of a side
unsigned g_evhash[2];
int g_nev[2];
DWORD g_evfirst[2][16][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_evhash[g_side] = fnv(g_evhash[g_side], e, 16);
    if (g_nev[g_side] < 16)
        memcpy(g_evfirst[g_side][g_nev[g_side]], e, 16);
    g_nev[g_side]++;
}

// The case's answers
struct Case {
    BYTE slot26, slot30, slot31, slot61, textures_on, window, graph12, combo;
    int can_read;
    DWORD player_layer;   // offset of the layer PlayerLayer gives, 0: null
    DWORD part_flags[8];
    BYTE show[8], hide[8];
};
Case g_c;
int g_can_read_left;
DWORD g_calls;
DWORD res(DWORD id) { return 0x5a000000u + id * 0x100 + (DWORD)g_nev[g_side]; }

// World slots
DWORD THISCALL w_slot1(void *w) { ev(0x101, tagp(w)); return ptrv(m(O_SLOT1)); }
DWORD THISCALL w_slot19(void *w) { ev(0x119, tagp(w)); return res(19); }
DWORD THISCALL w_slot26(void *w) { ev(0x126, tagp(w)); return 0x2600u | g_c.slot26; }
DWORD THISCALL w_slot30(void *w) { ev(0x130, tagp(w)); return 0x3000u | g_c.slot30; }
DWORD THISCALL w_slot31(void *w) { ev(0x131, tagp(w)); return 0x3100u | g_c.slot31; }
DWORD THISCALL w_slot44(void *w) { ev(0x144, tagp(w)); return ptrv(m(O_NL)); }
DWORD THISCALL w_slot48(void *w, DWORD s) { ev(0x148, tagp(w), s); return res(48); }
DWORD THISCALL w_slot51(void *w, void *e, DWORD b) { ev(0x151, tagp(w), tagp(e), b & 0xff); return res(51); }
DWORD THISCALL w_slot56(void *w, void *f, DWORD b) { ev(0x156, tagp(w), tagp(f), b); return res(56); }
DWORD THISCALL w_slot60(void *w) { ev(0x160, tagp(w)); return 0x6000u | g_c.slot61; }
DWORD THISCALL w_slot61(void *w) { ev(0x161, tagp(w)); return 0x6100u | g_c.slot61; }
DWORD THISCALL w_slot95(void *w, void *f) { ev(0x195, tagp(w), tagp(f)); return res(95); }
DWORD THISCALL w_any(void *w) { ev(0x1ff, tagp(w)); return 0; }
// Net layer
DWORD THISCALL nl_count(void *n) { ev(0x211, tagp(n)); return dw(m(O_SLOT1), 0x18); }
DWORD THISCALL nl_player(void *n, DWORD i) { ev(0x223, tagp(n), i); return ptrv(m(O_PLAYERS + (i & 3) * 0x100)); }
DWORD THISCALL nl_slot27(void *n, DWORD i) { ev(0x227, tagp(n), i); return res(27); }
// Frames
DWORD THISCALL f_slot29(void *f) { ev(0x329, tagp(f)); return 16; }
DWORD THISCALL f_width(void *f) { ev(0x345, tagp(f)); return dw(f, 4); }
DWORD THISCALL f_height(void *f) { ev(0x339, tagp(f)); return dw(f, 8); }
DWORD THISCALL f_slot62(void *f, void *src, const int *sr, const int *dr, void *mode)
{
    ev(0x362, tagp(f), tagp(src), fnv(fnv(2166136261u, sr, 16), dr, 16) ^ ptrv(mode));
    return res(62);
}
// Layers
DWORD THISCALL l_slot62(void *l, void *target) { ev(0x462, tagp(l), tagp(target)); return res(162); }
// Others
DWORD THISCALL win_slot27(void *wnd) { ev(0x527, tagp(wnd)); return g_c.window; }
DWORD THISCALL graph_slot12(void *g) { ev(0x612, tagp(g)); return 0x1200u | g_c.graph12; }
DWORD THISCALL graph_slot14(void *g, DWORD i, void *p) { ev(0x614, tagp(g), i, tagp(p)); return res(14); }
DWORD THISCALL part_slot58(BYTE *p, DWORD bits) { ev(0x758, tagp(p), bits); dw(p, 0x1c) |= bits; return res(58); }
DWORD THISCALL part_slot59(BYTE *p, DWORD bits) { ev(0x759, tagp(p), bits); dw(p, 0x1c) &= ~bits; return res(59); }
DWORD THISCALL pred_show(BYTE *p) { ev(0x7a0, tagp(p)); return 0x700u | g_c.show[dw(p, 4) & 7]; }
DWORD THISCALL pred_hide(BYTE *p) { ev(0x7a1, tagp(p)); return 0x700u | g_c.hide[dw(p, 4) & 7]; }

DWORD g_world_vt[100], g_nl_vt[30], g_frame_vt[64], g_layer_vt[64], g_win_vt[30], g_graph_vt[16], g_part_vt[64];

// Global stand-ins
DWORD THISCALL s_this0(void *t) { ev(0x8000 + g_calls++, tagp(t)); return res(1); }
DWORD THISCALL s_this1(void *t, DWORD a) { ev(0x8100 + g_calls++, tagp(t), tag(a)); return res(2); }
DWORD THISCALL s_this2(void *t, DWORD a, DWORD b) { ev(0x8200 + g_calls++, tagp(t), tag(a), tag(b)); return res(3); }
DWORD THISCALL s_rect(void *t, const int *r) { ev(0x8300 + g_calls++, tagp(t), fnv(2166136261u, r, 16)); return res(4); }
DWORD THISCALL s_clip_rect(void *t, int *r)
{
    ev(0x8400 + g_calls++, tagp(t), fnv(2166136261u, r, 16));
    r[0] += 1;
    r[3] -= 2;
    return res(5);
}
DWORD THISCALL s_player_layer(void *w, DWORD i) { ev(0x8500, tagp(w), i); return g_c.player_layer ? ptrv(m(g_c.player_layer)) : 0; }
DWORD THISCALL s_listener(void *rc, const float *pos) { ev(0x8600, tagp(rc), fnv(2166136261u, pos, 12)); return res(6); }
DWORD __cdecl s_rc() { ev(0x8700); return ptrv(m(O_RC)); }
DWORD THISCALL s_can_read(void *pk) { ev(0x8800, tagp(pk)); return g_can_read_left-- > 0 ? 0x101u : 0x100u; }
DWORD __cdecl s_textures() { ev(0x8900); return g_c.textures_on; }
DWORD THISCALL s_window(void *app) { ev(0x8a00, app == *at<void **>(0x6c02cc)); return ptrv(m(O_WINDOW)); }
DWORD __fastcall s_traffic(void *f, void *nl, void *g) { ev(0x8b00, tagp(f), tagp(nl), tagp(g)); return res(7); }
DWORD THISCALL s_set_size(BYTE *f, DWORD w, DWORD h, DWORD bits)
{
    ev(0x8c00, tagp(f), w * 0x10000 + h, bits);
    if (f == at<BYTE *>(0x6c2548)) {
        *reinterpret_cast<WORD *>(f + 0x12) = (WORD)w;
        *reinterpret_cast<WORD *>(f + 0x14) = (WORD)h;
    }
    return res(8);
}
DWORD THISCALL s_blit(void *f, void *src, const int *sr, const int *dr, void *mode)
{
    ev(0x8d00, tagp(f), tagp(src), fnv(fnv(2166136261u, sr, 16), dr, 16) ^ ptrv(mode));
    return res(9);
}
DWORD THISCALL s_blit_at(void *f, void *src, int x, int y, void *mode) { ev(0x8e00, tagp(f), tagp(src), (DWORD)x ^ (DWORD)y * 7 ^ ptrv(mode)); return res(10); }
DWORD THISCALL s_blit_at_clipped(void *f, void *src, int x, int y, const int *clip, void *mode)
{
    ev(0x8f00, tagp(f), tagp(src), (DWORD)x ^ (DWORD)y * 7 ^ ptrv(mode) ^ ptrv(clip));
    return res(11);
}
DWORD THISCALL s_combo(void *keys, DWORD k, DWORD a, DWORD b, DWORD c) { ev(0x9000, ptrv(keys), k, a ^ b ^ c); return 0x4400u | g_c.combo; }
DWORD __fastcall s_unique(char *out, const char *pattern, DWORD n)
{
    ev(0x9100, fnv(2166136261u, pattern, (DWORD)lstrlenA(pattern)), n, out ? 1 : 0);
    lstrcpyA(out, "Screen Shot 07.TGA");
    return res(12);
}
DWORD THISCALL s_save_tga(void *f, const char *name, DWORD bits) { ev(0x9200, tagp(f), fnv(2166136261u, name, (DWORD)lstrlenA(name)), bits); return res(13); }
DWORD THISCALL s_find_rect(void *grid, BYTE *list, const void *r, DWORD a, DWORD kinds, DWORD b, DWORD c)
{
    ev(0x9300, tagp(grid), tagp(list), (r ? 1 : 0) ^ a ^ kinds ^ b ^ c);
    dw(list, 4) = ptrv(m(O_LIST_DATA));
    dw(list, 8) = 8;
    return res(14);
}
DWORD THISCALL s_get_temp(void **slot) { ev(0x9400, tagp(slot)); *slot = m(O_LIST); return res(15); }
DWORD THISCALL s_release_temp(void **slot) { ev(0x9500, tagp(slot), tagp(*slot)); return res(16); }
DWORD THISCALL s_release_rect(void *map, const int *r, DWORD k) { ev(0x9600, tagp(map), fnv(2166136261u, r, 16), k); return res(17); }
DWORD THISCALL s_pass(void *sd, void *f, const int *r)
{
    ev(0x9700, tagp(sd), tagp(f), fnv(2166136261u, r, 16) ^ *at<BYTE *>(0x6c2908) ^ (DWORD)*at<BYTE *>(0x6c1cf4) << 8);
    return res(18);
}
DWORD THISCALL s_frame_ctor(void *f) { ev(0x9800, ptrv(f)); return ptrv(f); }
DWORD THISCALL s_set_palette(void *f, void *p) { ev(0x9900, tagp(f), ptrv(p)); return res(19); }
int __cdecl s_atexit(DWORD f) { ev(0x9a00, f); return 0; }

struct StandIn {
    DWORD address;
    void *fn;
};
const StandIn STANDS[] = {
    {0x4ce470, (void *)&s_this0},
    {0x4ce4c0, (void *)&s_this0},
    {0x518700, (void *)&s_this1},
    {0x51e470, (void *)&s_this1},
    {0x4cdfd0, (void *)&s_this1},
    {0x57aef0, (void *)&s_rc},
    {0x57c170, (void *)&s_listener},
    {0x4f10a0, (void *)&s_player_layer},
    {0x524770, (void *)&s_frame_ctor},
    {0x524f10, (void *)&s_this1},
    {0x524fe0, (void *)&s_this2},
    {0x524f50, (void *)&s_rect},
    {0x524a10, (void *)&s_clip_rect},
    {0x5d0863, (void *)&s_atexit},
    {0x520f50, (void *)&s_can_read},
    {0x5200d0, (void *)&s_this0},
    {0x520750, (void *)&s_this0},
    {0x40e9b0, (void *)&s_textures},
    {0x4cdf30, (void *)&s_window},
    {0x520150, (void *)&s_traffic},
    {0x5186a0, (void *)&s_this1},
    {0x518760, (void *)&s_this1},
    {0x51e3e0, (void *)&s_this2},
    {0x51e520, (void *)&s_this1},
    {0x4a8a90, (void *)&s_frame_ctor},
    {0x4ab5a0, (void *)&s_set_size},
    {0x4a8f40, (void *)&s_blit},
    {0x4b3280, (void *)&s_blit_at},
    {0x4b3300, (void *)&s_blit_at_clipped},
    {0x4ea220, (void *)&s_this1},
    {0x4e28b0, (void *)&s_combo},
    {0x4a16e0, (void *)&s_unique},
    {0x4aa070, (void *)&s_save_tga},
    {0x59a470, (void *)&s_get_temp},
    {0x59a510, (void *)&s_release_temp},
    {0x57a770, (void *)&s_find_rect},
    {0x541fb0, (void *)&s_this0},
    {0x540640, (void *)&s_release_rect},
    {0x4e8900, (void *)&s_pass},
    {0x4aa390, (void *)&s_set_palette},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];

void init_vtables()
{
    for (int i = 0; i < 100; i++)
        g_world_vt[i] = fn_address(&w_any);
    g_world_vt[1] = fn_address(&w_slot1);
    g_world_vt[19] = fn_address(&w_slot19);
    g_world_vt[26] = fn_address(&w_slot26);
    g_world_vt[30] = fn_address(&w_slot30);
    g_world_vt[31] = fn_address(&w_slot31);
    g_world_vt[44] = fn_address(&w_slot44);
    g_world_vt[48] = fn_address(&w_slot48);
    g_world_vt[51] = fn_address(&w_slot51);
    g_world_vt[56] = fn_address(&w_slot56);
    g_world_vt[60] = fn_address(&w_slot60);
    g_world_vt[61] = fn_address(&w_slot61);
    g_world_vt[95] = fn_address(&w_slot95);
    for (int i = 0; i < 30; i++)
        g_nl_vt[i] = fn_address(&w_any);
    g_nl_vt[11] = fn_address(&nl_count);
    g_nl_vt[23] = fn_address(&nl_player);
    g_nl_vt[27] = fn_address(&nl_slot27);
    for (int i = 0; i < 64; i++) {
        g_frame_vt[i] = fn_address(&w_any);
        g_layer_vt[i] = fn_address(&w_any);
        g_part_vt[i] = fn_address(&w_any);
    }
    g_frame_vt[29] = fn_address(&f_slot29);
    g_frame_vt[39] = fn_address(&f_height);
    g_frame_vt[45] = fn_address(&f_width);
    g_frame_vt[62] = fn_address(&f_slot62);
    g_layer_vt[62] = fn_address(&l_slot62);
    g_part_vt[58] = fn_address(&part_slot58);
    g_part_vt[59] = fn_address(&part_slot59);
    for (int i = 0; i < 30; i++)
        g_win_vt[i] = fn_address(&w_any);
    g_win_vt[27] = fn_address(&win_slot27);
    for (int i = 0; i < 16; i++)
        g_graph_vt[i] = fn_address(&w_any);
    g_graph_vt[12] = fn_address(&graph_slot12);
    g_graph_vt[14] = fn_address(&graph_slot14);
}

float pick_float()
{
    switch (rnd(5)) {
    case 0: return 0.0f;
    case 1: return (float)rnd(4000) - 500.0f;
    default: return (float)rnd(100000) / 37.0f;
    }
}

// Builds the case's fakes on both sides (the same bytes, pointers into the side's memory)
void build_case()
{
    Case &c = g_c;
    c.slot26 = (BYTE)rnd(2);
    c.slot30 = (BYTE)rnd(2);
    c.slot31 = (BYTE)rnd(2);
    c.slot61 = (BYTE)rnd(256);
    c.textures_on = rnd(2);
    c.window = (BYTE)(rnd(3) != 0);
    c.graph12 = (BYTE)rnd(2);
    c.combo = (BYTE)(rnd(4) == 0);
    c.can_read = (int)rnd(4);
    const DWORD nlayers = rnd(4);
    c.player_layer = rnd(5) == 0 ? 0 : O_LAYERS + rnd(3) * 0x200;
    for (int i = 0; i < 8; i++) {
        c.part_flags[i] = random() & 0x7f0000u;
        c.show[i] = (BYTE)rnd(2);
        c.hide[i] = (BYTE)rnd(2);
    }
    BYTE world[0x340], players[4][0x100], layers[3][0x200];
    memset(world, 0, sizeof world);
    memset(players, 0, sizeof players);
    memset(layers, 0, sizeof layers);
    world[0x29] = (BYTE)rnd(2);
    world[0x310] = (BYTE)(rnd(4) != 0);
    world[0x311] = (BYTE)(rnd(4) != 0);
    dw(world, 0x2c0) = nlayers;
    const int vw = 160, vh = 120;
    int *view = reinterpret_cast<int *>(world + 0x2e4);
    if (rnd(2)) {
        view[0] = 0;
        view[1] = 0;
        view[2] = vw;
        view[3] = vh;
    } else {
        view[0] = (int)rnd(30);
        view[1] = (int)rnd(20);
        view[2] = vw - (int)rnd(30);
        view[3] = vh - (int)rnd(20);
    }
    if (rnd(4) == 0)
        view[2] = (int)rnd(200);
    const DWORD sizes[4][2] = {{640, 480}, {vw, vh}, {(DWORD)vw, (DWORD)vh}, {rnd(3) ? (DWORD)vw : 320, rnd(3) ? (DWORD)vh : 240}};
    for (int i = 0; i < 4; i++) {
        BYTE *p = players[i];
        dw(p, 0xd8) = rnd(3) ? 0x3f800000u : 0x3f400000u;
        dw(p, 0xdc) = rnd(5);
        dw(p, 0xe8) = 0x1000u + (DWORD)i;   // an entity value (only passed on)
    }
    for (int i = 0; i < 3; i++) {
        BYTE *l = layers[i];
        *reinterpret_cast<float *>(l + 0x80) = pick_float();
        *reinterpret_cast<float *>(l + 0x84) = pick_float();
        dw(l, 0x88) = rnd(3000);
        dw(l, 0x8c) = rnd(3000);
        dw(l, 0x48) = 64 + rnd(4000);
        dw(l, 0x4c) = 64 + rnd(4000);
        l[0x1ce] = (BYTE)rnd(2);
    }
    const DWORD nplayers = rnd(5);
    const bool graph = rnd(4) != 0;
    // player records name an existing layer (every path that looks player 0's layer up uses it)
    for (int i = 0; i < 4; i++)
        dw(players[i], 0xdc) = nlayers ? dw(players[i], 0xdc) % nlayers : 0;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        memset(g_mem[side], 0, MEM);
        memcpy(m(O_WORLD), world, sizeof world);
        BYTE *w = m(O_WORLD);
        dw(w, 0) = ptrv(g_world_vt);
        dw(w, 4) = ptrv(m(O_DISPLAY));
        dw(w, 0x2a0) = graph ? ptrv(m(O_GRAPH)) : 0;
        dw(w, 0x2a4) = ptrv(m(O_ZOOM));
        dw(w, 0x2a8) = ptrv(m(O_FRAME));
        dw(w, 0x2bc) = ptrv(m(O_LAYER_ARRAY));
        dw(w, 0x2e0) = ptrv(m(O_E0));
        dw(w, 0x2ec) = (DWORD)vw;
        dw(w, 0x2f0) = (DWORD)vh;
        dw(w, 0x2fc) = ptrv(m(O_BG));
        dw(w, 0x334) = ptrv(m(O_MIXER));
        dw(m(O_NL), 0) = ptrv(g_nl_vt);
        dw(m(O_SLOT1), 0x18) = nplayers;
        dw(m(O_WINDOW), 0) = ptrv(g_win_vt);
        dw(m(O_GRAPH), 0) = ptrv(g_graph_vt);
        const DWORD frames[4] = {O_DISPLAY, O_ZOOM, O_FRAME, O_BG};
        for (int i = 0; i < 4; i++) {
            dw(m(frames[i]), 0) = ptrv(g_frame_vt);
            dw(m(frames[i]), 4) = sizes[i][0];
            dw(m(frames[i]), 8) = sizes[i][1];
        }
        dw(m(O_E0), 0x18) = 0x5e000000u + nplayers;
        for (int i = 0; i < 4; i++)
            memcpy(m(O_PLAYERS + (DWORD)i * 0x100), players[i], 0x100);
        for (int i = 0; i < 3; i++) {
            BYTE *l = m(O_LAYERS + (DWORD)i * 0x200);
            memcpy(l, layers[i], 0x200);
            dw(l, 0) = ptrv(g_layer_vt);
            dw(l, 0x7c) = ptrv(m(O_RC)) + 4;   // the tile map (only passed on)
            reinterpret_cast<DWORD *>(m(O_LAYER_ARRAY))[i] = ptrv(l);
        }
        for (int i = 0; i < 8; i++) {
            BYTE *p = m(O_PARTS + (DWORD)i * 0x20);
            dw(p, 0) = ptrv(g_part_vt);
            dw(p, 4) = (DWORD)i;
            dw(p, 0x1c) = c.part_flags[i];
            reinterpret_cast<DWORD *>(m(O_LIST_DATA))[i] = ptrv(p);
        }
    }
}

// The globals and statics the frame code reads or builds
struct Globals {
    SavedBytes proxy, proxy_guard, frame, frame_guard, shot_flag, shot_count, cf4, c2908;
    void save()
    {
        save_bytes(proxy, 0x6c2040, 0x3c);
        save_bytes(proxy_guard, 0x6c2478, 1);
        save_bytes(frame, 0x6c2548, 0x40);
        save_bytes(frame_guard, 0x6c2678, 1);
        save_bytes(shot_flag, 0x6c28e8, 1);
        save_bytes(shot_count, 0x60a528, 4);
        save_bytes(cf4, 0x6c1cf4, 1);
        save_bytes(c2908, 0x6c2908, 1);
    }
    void restore() const
    {
        restore_bytes(proxy);
        restore_bytes(proxy_guard);
        restore_bytes(frame);
        restore_bytes(frame_guard);
        restore_bytes(shot_flag);
        restore_bytes(shot_count);
        restore_bytes(cf4);
        restore_bytes(c2908);
    }
};
Globals g_globals;

struct CaseGlobals {
    BYTE proxy_guard, frame_guard, shot_flag, cf4;
    DWORD shot_count;
    WORD static_w, static_h;
    void set() const
    {
        *at<BYTE *>(0x6c2478) = proxy_guard ? (BYTE)(*at<BYTE *>(0x6c2478) | 1) : (BYTE)(*at<BYTE *>(0x6c2478) & ~1);
        *at<BYTE *>(0x6c2678) = frame_guard ? (BYTE)(*at<BYTE *>(0x6c2678) | 1) : (BYTE)(*at<BYTE *>(0x6c2678) & ~1);
        *at<BYTE *>(0x6c28e8) = shot_flag;
        *at<DWORD *>(0x60a528) = shot_count;
        *at<BYTE *>(0x6c1cf4) = cf4;
        *at<WORD *>(0x6c2548 + 0x12) = static_w;
        *at<WORD *>(0x6c2548 + 0x14) = static_h;
    }
    DWORD state() const
    {
        const DWORD v[4] = {*at<BYTE *>(0x6c2478) & 1u, *at<BYTE *>(0x6c2678) & 1u, *at<DWORD *>(0x60a528), (DWORD)*at<BYTE *>(0x6c1cf4) << 8 | *at<BYTE *>(0x6c2908)};
        return fnv(fnv(2166136261u, v, 16), at<void *>(0x6c2548 + 0x12), 4);
    }
};

// The fakes' bytes, pointers into the side's memory by offset
unsigned mem_hash(int side)
{
    g_side = side;
    unsigned h = 2166136261u;
    for (DWORD i = 0; i < MEM; i += 4) {
        const DWORD v = tag(*reinterpret_cast<DWORD *>(g_mem[side] + i));
        h = fnv(h, &v, 4);
    }
    return h;
}

template <typename F> void two_sides(Tally &t, const char *what, unsigned seed, DWORD eax_mask, F call)
{
    g_seed = seed;
    build_case();
    CaseGlobals cg;
    cg.proxy_guard = (BYTE)rnd(2);
    cg.frame_guard = (BYTE)rnd(2);
    cg.shot_flag = (BYTE)(rnd(3) == 0);
    cg.shot_count = rnd(3);
    cg.cf4 = (BYTE)rnd(2);
    cg.static_w = (WORD)(rnd(2) ? 160 : 640);
    cg.static_h = (WORD)(rnd(2) ? 120 : 480);
    DWORD eax[2], st[2];
    unsigned mh[2];
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(g_stack_hi), "=r"(g_stack_lo));
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_evhash[side] = 2166136261u;
        g_nev[side] = 0;
        g_calls = 0;
        g_can_read_left = g_c.can_read;
        g_globals.restore();
        cg.set();
        if (side == 0)
            hooks_out();
        CodePatch p[NSTANDS];
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(p[i], STANDS[i].address, STANDS[i].fn);
        eax[side] = tag(call(side) & eax_mask);
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(p[i]);
        if (side == 0)
            hooks_in();
        st[side] = cg.state();
        g_globals.restore();
        mh[side] = mem_hash(side);
    }
    char d[200];
    wsprintfA(d, "[%s seed %x: eax %x / %x, events %d / %d, fakes %x / %x, globals %x / %x]", what, seed, eax[0], eax[1], g_nev[0], g_nev[1], mh[0], mh[1], st[0],
              st[1]);
    const bool same = g_fits && eax[0] == eax[1] && g_evhash[0] == g_evhash[1] && g_nev[0] == g_nev[1] && mh[0] == mh[1] && st[0] == st[1];
    const int before = t.mismatches;
    t.check(same, "", d);
    if (!same && before < 10)
        for (int i = 0; i < 16 && i < g_nev[0] && i < g_nev[1]; i++)
            if (memcmp(g_evfirst[0][i], g_evfirst[1][i], 16)) {
                log("    event %d: %x %x %x %x / %x %x %x %x", i, g_evfirst[0][i][0], g_evfirst[0][i][1], g_evfirst[0][i][2], g_evfirst[0][i][3],
                    g_evfirst[1][i][0], g_evfirst[1][i][1], g_evfirst[1][i][2], g_evfirst[1][i][3]);
                break;
            }
}

void check_frames(Tally &t)
{
    for (int k = 0; k < scaled(3000); k++) {
        const unsigned seed = 0x4f2000u + (unsigned)k * 7919u;
        const int kind = k % 5;
        const DWORD seconds = 0x3d088889u + (DWORD)k;
        switch (kind) {
        case 0:
            two_sides(t, "World_Frame (slot 3)", seed, ~0u, [&](int side) -> DWORD {
                return at<DWORD(THISCALL *)(void *, DWORD)>(0x4f2620)(g_mem[side] + O_WORLD, seconds);
            });
            break;
        case 1:
            two_sides(t, "World_DrawFrame (slot 4)", seed, 0xff, [&](int side) -> DWORD { return at<DWORD(THISCALL *)(void *)>(0x4f2d90)(g_mem[side] + O_WORLD); });
            break;
        case 2:
            two_sides(t, "World_SetViewTarget", seed, ~0u, [&](int side) -> DWORD {
                // the letterbox path and the zoomed path dereference player 0's layer: only with layers that exist
                BYTE *w = g_mem[side] + O_WORLD;
                if (!dw(w, 0x2c0))
                    dw(w, 0x2c0) = 1;
                for (int i = 0; i < 4; i++)
                    dw(g_mem[side] + O_PLAYERS + (DWORD)i * 0x100, 0xdc) %= dw(w, 0x2c0);
                if (!g_c.player_layer)
                    g_c.player_layer = O_LAYERS;
                return at<DWORD(THISCALL *)(void *)>(0x4f2800)(w);
            });
            break;
        case 3:
            two_sides(t, "World_FrameEnd", seed, ~0u, [&](int side) -> DWORD { return at<DWORD(THISCALL *)(void *)>(0x4f2be0)(g_mem[side] + O_WORLD); });
            break;
        default: {
            const DWORD use = rnd(4);
            two_sides(t, "World_RenderLayer", seed, ~0u, [&](int side) -> DWORD {
                return at<DWORD(__stdcall *)(void *, void *, DWORD, DWORD)>(0x4f4ae0)(g_mem[side] + O_LAYERS, g_mem[side] + O_ZOOM,
                                                                                     use & 1 ? fn_address(&pred_show) : 0, use & 2 ? fn_address(&pred_hide) : 0);
            });
        }
        }
    }
}

}  // namespace

ZAX_CHECKS(world_frame)
{
    (void)cls;
    (void)n;
    init_vtables();
    g_globals.save();
    Tally t{"world frame: slots 3 and 4, the view's target, the frame's end, a layer rendered whole (stand-ins)"};
    check_frames(t);
    t.report();
    g_globals.restore();
}

}  // namespace zax
