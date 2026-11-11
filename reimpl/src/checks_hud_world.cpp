// Checks of hud_world.cpp with the frame check harness, in the game thread.
//  - CZaxWorld::DrawHud on the running world into a fake 5-6-5 frame of the size of the world's HUD frame
//    (+0x2a8): the real HUD images, fonts, icons, inventory display options, custom rules, the player's HUD
//    records with their real renderers and render data. Per case the world's game type (+0x58c: its own
//    or none), the help and message timers (+0x594, +0x5a4-+0x5b0), the message kind, icon and item
//    (+0x5a0, +0x598, +0x59c), the direct drawing flag 0x6c1d08 and the player's counts (+0xec-+0xf6) are
//    set and put back afterwards. Stand-ins: the clocks (0x4d24b0, 0x4d2460, 0x4d2650), the auto-map
//    (HudMiniMap_Draw 0x508180: its arguments recorded; it is the HUD batch's, it changes the static map
//    frame and the fog), atexit. The Sphere of Zin's statics, the Transparent Mask's opacity and the blend
//    modes' FindBlitter caches are set alike before each side. Everything a draw loads on first use is
//    loaded by a run of the original before the cases.
//  - The status text (0x5a0a30, 0x5a0c00) on a fake world (+0x2a8 a fake frame of 5-6-5 or 8 bits, the
//    text a ZString of the case, the fraction from 0 to beyond 1 and NaN, the bar flag; the display a fake
//    frame with +0x44 the flip count, the flip 0x5693a0 a recording stand-in).
//  - The game types' HUD slot 42 on fake game type objects (the real fonts and images that are loaded,
//    positions and justifications), the net player and the scoreboard stand-ins (ZaxNet_PlayerAt 0x5ba810,
//    Scoreboard_Get 0x566a80) with fake players (team, score) and scoreboards (teams with scores, rows with
//    names and frags in the layout the exe's accessors read); up to four teams, so Capture the Flag's walk
//    past its two team names reaches its return address and the scoreboard (not the crash at the fifth).
#include "checks_hud_common.h"
#include "x87.h"

namespace zax {

int hooks_hud_world(Hook *out);

namespace {

using namespace framec;

ModuleHooks g_hooks{"hud_world", &hooks_hud_world, {}, {}, {}, 0, true};

DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }
int rnd_range(int lo, int hi) { return lo + (int)rnd((DWORD)(hi - lo + 1)); }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// ---- Stand-ins ---------------------------------------------------------------------------------------------

DWORD g_time, g_ticks, g_tick_calls, g_map_calls, g_flips;
DWORD g_map_args[4];
DWORD __stdcall s_time() { return g_time; }
DWORD __stdcall s_ticks() { return g_ticks + 37u * g_tick_calls++; }
DWORD __cdecl s_ticks24() { return (g_ticks + 11u * g_tick_calls++) & 0xffffff; }
DWORD THISCALL s_map(void *self, void *frame, int x, int y)
{
    g_map_calls++;
    g_map_args[0] = ptrv(self);
    g_map_args[1] = in_arena(0, ptrv(frame)) || in_arena(1, ptrv(frame)) ? 1 : 0;
    g_map_args[2] = (DWORD)x;
    g_map_args[3] = (DWORD)y;
    return 0;
}
DWORD THISCALL s_flip(void *display)
{
    g_flips = g_flips * 31 + 1;
    (void)display;
    return 0;
}

// The net player and the scoreboard of the game type cases
BYTE g_player[0x100];
BYTE *__fastcall s_player_at(DWORD i)
{
    (void)i;
    return g_player;
}
alignas(4) BYTE g_board[0x40], g_teams[4][8], g_rows[6][0x10];
DWORD g_team_ptrs[4], g_row_ptrs[6], g_sorted[6];
void *__cdecl s_board() { return g_board; }

constexpr int NSTANDS = 6;
CodePatch g_patches[NSTANDS];
void stands_on()
{
    patch_jump(g_patches[0], 0x4d24b0, (void *)&s_time);
    patch_jump(g_patches[1], 0x4d2460, (void *)&s_ticks);
    patch_jump(g_patches[2], 0x4d2650, (void *)&s_ticks24);
    patch_jump(g_patches[3], 0x508180, (void *)&s_map);
    patch_jump(g_patches[4], 0x5d0863, (void *)&fake_atexit);
    patch_jump(g_patches[5], 0x5693a0, (void *)&s_flip);
}
void stands_off()
{
    for (int i = NSTANDS - 1; i >= 0; i--)
        unpatch(g_patches[i]);
}

// The statics a HUD draw changes (the Sphere of Zin's phase), set from the case before each side
struct Statics {
    DWORD phase, last;
    BYTE guard;
    void save()
    {
        phase = *at<DWORD *>(0x713810);
        last = *at<DWORD *>(0x7137f8);
        guard = *at<BYTE *>(0x7137d9);
    }
    void set() const
    {
        *at<DWORD *>(0x713810) = phase;
        *at<DWORD *>(0x7137f8) = last;
        *at<BYTE *>(0x7137d9) = guard;
    }
};
void set_mask_opacity(float v) { at<void(THISCALL *)(void *, float)>(0x433be0)(at<void *>(0x6bcc68), v); }

// Loaded records of a definition list (records +0x30, count +0x34): their indices
int loaded_indices(DWORD list, DWORD *out, int cap)
{
    BYTE *records = *at<BYTE **>(list + 0x30);
    const DWORD n = *at<DWORD *>(list + 0x34);
    int k = 0;
    for (DWORD i = 0; records && i < n && k < cap; i++)
        if (dw(records + i * 0x80, 0x78) && !records[i * 0x80 + 0x7f])
            out[k++] = i;
    return k;
}

// ---- DrawHud on the running world --------------------------------------------------------------------------

struct WorldFields {
    BYTE f[0x30];   // +0x58c .. +0x5bb
    BYTE direct;
    BYTE counts[12];   // the player's +0xec .. +0xf7
};
void fields_save(WorldFields &s, BYTE *w, BYTE *player)
{
    memcpy(s.f, w + 0x58c, sizeof s.f);
    s.direct = *at<BYTE *>(0x6c1d08);
    memcpy(s.counts, player + 0xec, sizeof s.counts);
}
void fields_set(const WorldFields &s, BYTE *w, BYTE *player)
{
    memcpy(w + 0x58c, s.f, sizeof s.f);
    *at<BYTE *>(0x6c1d08) = s.direct;
    memcpy(player + 0xec, s.counts, sizeof s.counts);
}

void check_draw_hud(Tally &t)
{
    BYTE *w = *at<BYTE **>(0x6c2080);
    if (!w || !w[0x310] || !w[0x311] || !dw(w, 0x2a8)) {
        log("  hud world: no game view to draw the HUD of (skipped)");
        return;
    }
    BYTE *player = at<BYTE *(__fastcall *)(DWORD)>(0x5ba810)(*at<DWORD *>(0x5fe8f8));
    void *real_frame = bp(dw(w, 0x2a8));
    const int fwid = (int)vcall<DWORD>(real_frame, 45), fhgt = (int)vcall<DWORD>(real_frame, 39);
    DWORD icons[64], items = *at<DWORD *>(0x6c0c3c);
    const int nicons = loaded_indices(0x6c0aa8, icons, 64);
    log("  hud world: DrawHud on a %d x %d frame, game type %d, %u HUD records, %d loaded icons", fwid, fhgt, (int)dw(w, 0x58c), dw(player, 0x104), nicons);
    WorldFields real;
    fields_save(real, w, player);
    Statics real_statics;
    real_statics.save();
    // a run of the original: what the draw loads on first use
    {
        begin_case(1);
        FrameSpec s;
        s.width = fwid;
        s.height = fhgt;
        const int fi = make_frame(s);
        if (fi < 0) {
            log("  hud world: frame %d x %d does not fit the harness", fwid, fhgt);
            return;
        }
        stands_on();
        reinterpret_cast<void(THISCALL *)(void *, void *)>(original_address(0x59e730))(w, frame(0, fi));
        stands_off();
        real_statics.set();
        set_mask_opacity(1.0f);
    }
    g_seed = 0x59e730;
    for (int k = 0; k < scaled(160); k++) {
        begin_case(random());
        FrameSpec s;
        s.width = fwid;
        s.height = fhgt;
        const int fi = make_frame(s);
        FrameSpec room;
        room.width = fwid;
        room.height = 2;
        make_frame(room);
        WorldFields c = real;
        if (rnd(4) == 0)
            dw(c.f, 0) = 0xffffffffu;   // no game type
        float *timers = reinterpret_cast<float *>(c.f + 8);   // +0x594
        const DWORD mode = rnd(6);
        timers[0] = mode ? 2.5f : 0.0f;                        // +0x594
        dw(c.f, 0xc) = nicons ? icons[rnd((DWORD)nicons)] : 0;     // +0x598
        dw(c.f, 0x10) = items ? rnd(items) : 0;                    // +0x59c
        dw(c.f, 0x14) = rnd(4);                                    // +0x5a0
        *reinterpret_cast<float *>(c.f + 0x18) = mode == 4 ? 1.0f : 0.0f;   // +0x5a4
        *reinterpret_cast<float *>(c.f + 0x1c) = mode == 1 ? 1.0f : 0.0f;   // +0x5a8
        *reinterpret_cast<float *>(c.f + 0x20) = mode == 2 ? 1.0f : 0.0f;   // +0x5ac
        *reinterpret_cast<float *>(c.f + 0x24) = mode == 3 ? 1.0f : rnd(2) ? 0.0f : -1.0f;   // +0x5b0
        c.direct = (BYTE)rnd(2);
        for (int i = 0; i < 6; i++)
            reinterpret_cast<short *>(c.counts)[i] = rnd(3) ? (short)rnd(300) : (short)(random() >> 16);
        g_time = random();
        g_ticks = random();
        Statics cs = real_statics;
        cs.last = g_ticks - rnd(400);
        char what[96];
        wsprintfA(what, "DrawHud case %d, mode %u, game type %d", k, mode, (int)dw(c.f, 0));
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            fields_set(c, w, player);
            cs.set();
            set_mask_opacity(1.0f);
            g_tick_calls = 0;
            g_map_calls = 0;
            memset(g_map_args, 0, sizeof g_map_args);
            stands_on();
            at<void(THISCALL *)(void *, void *)>(0x59e730)(w, frame(side, fi));
            stands_off();
            Statics after;
            after.save();
            fields_set(real, w, player);
            return fnv(fnv(fnv(2166136261u, &after, sizeof after), g_map_args, 16), &g_map_calls, 4);
        });
    }
    fields_set(real, w, player);
    real_statics.set();
    set_mask_opacity(1.0f);
}

// ---- The status text ---------------------------------------------------------------------------------------

alignas(16) BYTE g_world[2][0x380];

void check_status_text(Tally &t)
{
    g_seed = 0x5a0a30;
    static const char *const texts[] = {"Loading...", "Saving Game", "", "A somewhat longer status line with words", "x"};
    for (int k = 0; k < scaled(300); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = rnd(4) ? RGB565 : PAL8;
        s.width = rnd_range(60, 320);
        s.height = rnd_range(40, 240);
        const int fi = make_frame(s);
        FrameSpec d = s;   // the display of the same depth (an 8-bit copy to 16 bits would take the frames' palette)
        const int di = make_frame(d);
        const DWORD display_extra = alloc_object(0x10);   // +0x40..+0x4f after the display frame: +0x44 the flips
        const int flips = rnd(3) ? (int)rnd(3) : -1;
        for (int side = 0; side < 2; side++)
            dw(object<BYTE>(side, display_extra), 4) = (DWORD)flips;
        const char *text = texts[rnd(5)];
        float fraction;
        switch (rnd(5)) {
        case 0: fraction = 0.0f; break;
        case 1: fraction = (float)rnd(200) / 100.0f; break;
        case 2: fraction = __builtin_nanf(""); break;
        default: fraction = (float)rnd(101) / 100.0f; break;
        }
        const BYTE no_bar = (BYTE)rnd(2);
        const bool show = rnd(2);
        hudcheck::run_sides(t, g_hooks, show ? "ShowStatusText" : "DrawStatusText", [&](int side) -> DWORD {
            BYTE *w = g_world[side];
            memset(w, 0, sizeof g_world[side]);
            dw(w, 4) = ptrv(frame(side, di));
            dw(w, 0x2a8) = ptrv(frame(side, fi));
            at<void *(THISCALL *)(void *)>(0x4dec90)(w + 0x364);
            set_mask_opacity(1.0f);
            g_flips = 0;
            stands_on();
            BYTE source[4];   // the text as a ZString (0x5a0c00 and 0x4e18f0 take a ZString)
            at<void *(THISCALL *)(void *)>(0x4dec90)(source);
            at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(source, text);
            if (show) {
                at<void(THISCALL *)(void *, const void *, float, DWORD)>(0x5a0c00)(w, source, fraction, no_bar);
            } else {
                at<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(w + 0x364, source);
                *reinterpret_cast<float *>(w + 0x368) = fraction;
                w[0x36c] = no_bar;
                at<void(THISCALL *)(void *)>(0x5a0a30)(w);
            }
            stands_off();
            const float op = fstp(at<long double(THISCALL *)(void *)>(0x433c20)(at<void *>(0x6bcc68)));
            const DWORD v[4] = {dw(w, 0x368), w[0x36c], g_flips, fbits(op)};
            at<void(THISCALL *)(void *)>(0x4defd0)(w + 0x364);
            at<void(THISCALL *)(void *)>(0x4defd0)(source);
            set_mask_opacity(1.0f);
            return fnv(2166136261u, v, 16);
        });
    }
}

// ---- The game types' HUD -----------------------------------------------------------------------------------

BYTE g_zs[2][16][4];   // the rows' names (ZStrings), per side
void check_game_types(Tally &t)
{
    DWORD fonts[64], images[64];
    const int nfonts = loaded_indices(0x6be1e0, fonts, 64), nimages = loaded_indices(0x6c0540, images, 64);
    if (!nfonts || !nimages) {
        log("  hud world: no loaded fonts or images for the game type HUDs (skipped)");
        return;
    }
    g_seed = 0x4698f0;
    static const char *const names[] = {"Zax", "Rogcoil", "Player 3", "", "A very long player name"};
    for (int k = 0; k < scaled(600); k++) {
        begin_case(random());
        FrameSpec s;
        s.width = rnd_range(120, 320);
        s.height = rnd_range(80, 240);
        const int fi = make_frame(s);
        BYTE gt[0x100];
        memset(gt, 0, sizeof gt);
        for (DWORD o = 0xc8; o < 0x100; o += 4)
            dw(gt, o) = (DWORD)rnd_range(-20, 200);
        dw(gt, 0xc8) = fonts[rnd((DWORD)nfonts)];
        dw(gt, 0xd0) = rnd(2) ? fonts[rnd((DWORD)nfonts)] : (DWORD)rnd_range(0, 120);
        dw(gt, 0xd4) = fonts[rnd((DWORD)nfonts)];
        dw(gt, 0xe0) = rnd(8);
        dw(gt, 0xe8) = rnd(2) ? images[rnd((DWORD)nimages)] : (DWORD)rnd_range(0, 120);
        dw(gt, 0xec) = rnd(3);
        dw(gt, 0xf4) = rnd(3);
        dw(gt, 0xfc) = rnd(3);
        const int kind = (int)rnd(3);
        if (kind == 0) {   // CTF: the flag images are loaded ones, the font index too
            dw(gt, 0xe8) = images[rnd((DWORD)nimages)];
            dw(gt, 0xec) = images[rnd((DWORD)nimages)];
            dw(gt, 0xe4) = (DWORD)rnd_range(-20, 200);
        }
        if (kind == 2)
            dw(gt, 0xd0) = fonts[rnd((DWORD)nfonts)];
        memset(g_player, 0, sizeof g_player);
        dw(g_player, 0x14) = rnd(2);
        *reinterpret_cast<short *>(g_player + 0xf6) = rnd(3) ? (short)rnd(500) : (short)(random() >> 16);
        // the scoreboard: teams (+0x24, +0x28) and rows (+4 data, +8 count, +0x14 the sorted order)
        memset(g_board, 0, sizeof g_board);
        // 0-4 teams: Capture the Flag reads a third team's name from its return address (here the call below, the
        // same on both sides) and a fourth's from the scoreboard (its first dwords as text: +0 four non-zero
        // bytes, then the row pointer and count). A fifth team would read the address 4 and crash, in the
        // original as in the reimplementation; that case is left out.
        const DWORD nteams = rnd(5), nrows = rnd(7);
        dw(g_board, 0) = random() | 0x01010101;
        for (DWORD i = 0; i < 4; i++) {
            dw(g_teams[i], 4) = rnd(30);
            g_team_ptrs[i] = rnd(5) ? ptrv(g_teams[i]) : 0;
        }
        dw(g_board, 0x24) = ptrv(g_team_ptrs);
        dw(g_board, 0x28) = nteams;
        dw(g_board, 4) = ptrv(g_row_ptrs);
        dw(g_board, 8) = nrows;
        dw(g_board, 0x14) = ptrv(g_sorted);
        const DWORD x = (DWORD)rnd_range(-30, 60), y = (DWORD)rnd_range(-30, 60);
        const char *row_names[6];
        DWORD frags[6], order[6];
        for (DWORD i = 0; i < 6; i++) {
            row_names[i] = names[rnd(5)];
            frags[i] = rnd(40);
            order[i] = rnd(nrows ? nrows : 1);
        }
        static const DWORD draws[3] = {0x4698f0, 0x4796a0, 0x561760};
        static const char *const what[3] = {"CTF DrawHUD", "death match DrawHUD", "Salvage King DrawHud"};
        hudcheck::run_sides(t, g_hooks, what[kind], [&](int side) -> DWORD {
            for (DWORD i = 0; i < 6; i++) {
                BYTE *row = g_rows[i];
                at<void *(THISCALL *)(void *)>(0x4dec90)(g_zs[side][i]);
                at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(g_zs[side][i], row_names[i]);
                dw(row, 0) = dw(g_zs[side][i], 0);
                dw(row, 0xc) = frags[i];
                g_row_ptrs[i] = ptrv(row);
                g_sorted[i] = order[i];
            }
            CodePatch a, b;
            patch_jump(a, 0x5ba810, (void *)&s_player_at);
            patch_jump(b, 0x566a80, (void *)&s_board);
            at<void(THISCALL *)(void *, void *, DWORD, DWORD)>(draws[kind])(gt, frame(side, fi), x, y);
            unpatch(b);
            unpatch(a);
            for (DWORD i = 0; i < 6; i++)
                at<void(THISCALL *)(void *)>(0x4defd0)(g_zs[side][i]);
            return 0;
        });
    }
}

}  // namespace

ZAX_CHECKS(hud_world)
{
    (void)cls;
    (void)n;
    Tally hud{"hud world: CZaxWorld::DrawHud on the running world"};
    check_draw_hud(hud);
    hud.report();
    Tally status{"hud world: the status text"};
    check_status_text(status);
    status.report();
    Tally types{"hud world: the game types' HUD (CTF, death match, Salvage King)"};
    check_game_types(types);
    types.report();
}

}  // namespace zax
