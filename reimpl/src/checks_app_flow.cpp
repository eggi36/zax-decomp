// Checks of app_flow.cpp against the original, in the game thread (machinery: checks_inventory_common.h;
// side 0 with all hooks of the module taken out, side 1 through the reimplementation).
//  - The build numbers and both version texts (into strings of each side; the demo byte 0x62485c set and
//    clear, put back afterwards).
//  - The game type 0x59ff80 on a fake application block (+0x58c -1, in and out of range) over the game
//    type list 0x6c02e8 changed for the case and put back: fake records (names, loaded objects, loading
//    bytes from the seed), its vtable a copy with the loader slot 37 recorded; then the running game's
//    own application and list (real data; only when the current game type is loaded already, so nothing
//    is read from disk).
//  - "Weapon AutoSwitch" 0x59e6f0 for fake players (local or not, their settings' byte); the local one
//    reads Zax.CFG (GetPrivateProfileIntA, read only).
//  - The frame start 0x59db30 and the multiplayer frame 0x59db50 on a fake application with their callees
//    (0x4f2d90, 0x4ce160, 0x505330, World::MultiplayerFrame 0x4f0230) stand-ins.
//  - The pending map switch 0x59b3a0 into a fake application (its strings made for the case).
#include "checks_inventory_common.h"
#include "definitions.h"

namespace zax {

int hooks_app_flow(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE &B(void *p, DWORD off) { return static_cast<BYTE *>(p)[off]; }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0x0badc0deu; }
void zs_init(void *z) { at<ZString *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_free(void *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_set(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
const char *zs_text(void *z) { return static_cast<ZString *>(z)->c_str(); }

unsigned g_answer_seed;
DWORD ans()
{
    g_answer_seed = g_answer_seed * 1103515245u + 12345u;
    return g_answer_seed >> 4;
}

// ---- Fakes ---------------------------------------------------------------------------------------------------

constexpr int NFAKE = 8;
struct Fake {
    DWORD vftable;
    DWORD id;
    BYTE body[0x38];
};
Fake g_fake[NFAKE];
DWORD fid(const void *p)
{
    const DWORD a = ptrv(p), b = ptrv(g_fake);
    if (a >= b && a < b + sizeof g_fake)
        return 0xf0000000u + (a - b);
    return a;
}
BYTE g_app[0x600];   // a fake application

struct Stand {
    DWORD address;
    void *fn;
};
CodePatch g_patches[8];
template <int N> void stands_on(const Stand (&s)[N])
{
    for (int i = 0; i < N; i++)
        patch_jump(g_patches[i], s[i].address, s[i].fn);
}
template <int N> void stands_off(const Stand (&)[N])
{
    for (int i = N - 1; i >= 0; i--)
        unpatch(g_patches[i]);
}

// ---- Version texts -----------------------------------------------------------------------------------------------

void test_versions(Tally &t, unsigned k)
{
    const BYTE saved_demo = *at<BYTE *>(0x62485c);
    *at<BYTE *>(0x62485c) = (BYTE)(k & 1);
    DWORD r[2][4];
    unsigned h[2][2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        r[side][0] = at<DWORD(__cdecl *)()>(0x5908a0)();
        r[side][1] = at<DWORD(__cdecl *)()>(0x5908b0)();
        ZString z;
        zs_init(&z);
        if (k & 2)
            zs_set(&z, "some text before");
        r[side][2] = at<DWORD(THISCALL *)(void *)>(0x5908c0)(&z);
        h[side][0] = text_hash(zs_text(&z));
        zs_set(&z, k & 2 ? "x" : "");
        r[side][3] = at<DWORD(THISCALL *)(void *)>(0x5908f0)(&z);
        h[side][1] = text_hash(zs_text(&z));
        if (k == 0 && side == 1)
            log("  version texts: '%s'", zs_text(&z));
        zs_free(&z);
        side_end(side);
    }
    *at<BYTE *>(0x62485c) = saved_demo;
    char d[200];
    wsprintfA(d, "[%u: build %x / %x, min %x / %x, version eax %x / %x text %x / %x, full eax %x / %x text %x / %x]", k, r[0][0], r[1][0], r[0][1],
              r[1][1], r[0][2], r[1][2], h[0][0], h[1][0], r[0][3], r[1][3], h[0][1], h[1][1]);
    t.check(memcmp(r[0], r[1], sizeof r[0]) == 0 && h[0][0] == h[1][0] && h[0][1] == h[1][1], "version", d);
}

// ---- The game type ---------------------------------------------------------------------------------------------

constexpr DWORD GAME_TYPES = 0x6c02e8;
constexpr int NREC = 6;
ZDefinitionRecord g_records[NREC];
void *g_list_vt[42];
BYTE g_saved_list[sizeof(ZDefinitionList)];

void *THISCALL s_list_create(void *list, const char *dir, const char *name, void **out)
{
    ev(0x904, text_hash(dir), text_hash(name), ptrv(out) - ptrv(g_records));
    ev(0x914, list == at<void *>(GAME_TYPES));
    const DWORD a = ans();
    return a % 5 ? &g_fake[a % NFAKE] : nullptr;
}
const char *const NAMES[] = {"Quest", "Multiplayer/Capture The Flag", "Multiplayer/Salvage King", "Deathmatch", "a/b/c", "Z"};
const char *const FOLDERS[] = {"Resources/Game Types", "", "Game Types/"};

void game_list_on(unsigned seed)
{
    g_seed = seed;
    auto *l = at<ZDefinitionList *>(GAME_TYPES);
    memcpy(g_saved_list, l, sizeof g_saved_list);
    memset(g_records, 0, sizeof g_records);
    const bool entries = rnd(8) != 0;
    const DWORD n = entries ? 1 + rnd(NREC) : 0;
    for (DWORD i = 0; i < n; i++) {
        lstrcpyA(g_records[i].name, NAMES[(i + seed) % NREC]);
        g_records[i].object = rnd(3) ? &g_fake[rnd(NFAKE)] : nullptr;
        g_records[i].loading = rnd(6) == 0;
        g_records[i].keep = (BYTE)rnd(2);
    }
    l->records.vftable = ptrv(g_list_vt);
    l->records.data = reinterpret_cast<char *>(g_records);
    l->records.stride = 0x80;
    l->records.num = n;
    l->entries = entries ? g_records : nullptr;
    l->num_entries = n;
    lstrcpyA(l->folder, FOLDERS[rnd(3)]);
    memset(g_app, 0, sizeof g_app);
    const DWORD idx[] = {0xffffffff, 0, 1, 2, n, n + 3, 0x80000000u};
    D(g_app, 0x58c) = idx[rnd(7)];
    g_answer_seed = random();
}
unsigned game_list_state()
{
    unsigned h = fnv(2166136261u, at<void *>(GAME_TYPES), sizeof(ZDefinitionList));
    return fnv(h, g_records, sizeof g_records);
}
void game_list_off() { memcpy(at<void *>(GAME_TYPES), g_saved_list, sizeof g_saved_list); }

void test_game_type(Tally &t, unsigned seed)
{
    DWORD eax[2], state[2];
    for (int side = 0; side < 2; side++) {
        game_list_on(seed);
        log_reset(side);
        side_begin(side);
        eax[side] = fid(at<void *(THISCALL *)(void *)>(0x59ff80)(g_app));
        side_end(side);
        state[side] = game_list_state();
        game_list_off();
    }
    char d[160];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, list %x / %x]", seed, eax[0], eax[1], g_nlog[0], g_nlog[1], state[0], state[1]);
    const bool same = eax[0] == eax[1] && same_log() && state[0] == state[1];
    if (!same)
        log_first_difference();
    t.check(same, "CZaxWorld::GameType 0x59ff80", d);
}

// The running game's application and list, when its game type is loaded (read only)
void test_real_game_type(Tally &t)
{
    BYTE *app = *at<BYTE **>(0x713f14);
    auto *l = at<ZDefinitionList *>(GAME_TYPES);
    if (!app)
        return;
    const DWORD i = D(app, 0x58c);
    const bool loaded = i == 0xffffffff || !l->entries ||
                        (i < l->num_entries ? l->entries[i].object && !l->entries[i].loading : l->num_entries && l->entries[0].object && !l->entries[0].loading);
    log("  the running game's game type: index %d of %u, %s", (int)i, l->num_entries, loaded ? "loaded" : "not loaded (not called)");
    if (!loaded)
        return;
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        r[side] = ptrv(at<void *(THISCALL *)(void *)>(0x59ff80)(app));
        side_end(side);
    }
    char d[96];
    wsprintfA(d, "[running game: %x / %x]", r[0], r[1]);
    t.check(r[0] == r[1], "CZaxWorld::GameType (real)", d);
}

// ---- Weapon AutoSwitch, the frame wrappers, the map switch ------------------------------------------------------

void test_auto_switch(Tally &t, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *player = g_fake[0].body;
        for (DWORD k = 0; k < 0x30; k += 4)
            D(player, k) = random();
        B(player, 5) = (BYTE)(rnd(3) ? 0 : rnd(2) ? 1 : random());
        D(player, 0x1c) = ptrv(&g_fake[1 + rnd(NFAKE - 1)]);
        r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x59e6f0)(g_app, player);
        side_end(side);
    }
    char d[96];
    wsprintfA(d, "[seed %u: %x / %x]", seed, r[0], r[1]);
    t.check(r[0] == r[1], "CZaxWorld::WeaponAutoSwitch 0x59e6f0", d);
}

DWORD THISCALL s_4f2d90(void *app) { ev(0x921, fid(app) == ptrv(g_app)); const DWORD a = ans(); return (a & ~0xffu) | (a >> 9 & 1); }
DWORD THISCALL s_4ce160(void *app) { ev(0x922, ptrv(app) == ptrv(g_app)); return ans(); }
DWORD __cdecl s_505330() { ev(0x923); const DWORD a = ans(); return (a & ~0xffu) | (a >> 9 & 1); }
DWORD THISCALL s_mp_frame(void *app, DWORD a, DWORD b) { ev(0x924, ptrv(app) == ptrv(g_app), a, b); return ans(); }
const Stand FRAME_STANDS[] = {{0x4f2d90, (void *)&s_4f2d90}, {0x4ce160, (void *)&s_4ce160}, {0x505330, (void *)&s_505330}, {0x4f0230, (void *)&s_mp_frame}};

void test_frames(Tally &t, unsigned seed, bool mp)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        g_answer_seed = seed * 7919u;
        log_reset(side);
        side_begin(side);
        stands_on(FRAME_STANDS);
        const DWORD a = random(), b = random();
        r[side] = mp ? at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x59db50)(g_app, a, b) : at<DWORD(THISCALL *)(void *)>(0x59db30)(g_app);
        stands_off(FRAME_STANDS);
        side_end(side);
    }
    char d[96];
    wsprintfA(d, "[seed %u: %x / %x, log %d / %d]", seed, r[0], r[1], g_nlog[0], g_nlog[1]);
    t.check(r[0] == r[1] && same_log(), mp ? "CZaxWorld::MultiplayerFrame 0x59db50" : "CZaxWorld::FrameStart 0x59db30", d);
}

const char *const MAPS[] = {"", "ACT 1/M1M2/01 Main", "Test/Nachbau1", "Eigene/Level 1", "A rather long map name for a pending switch of the map, longer"};
void test_switch(Tally &t, unsigned seed)
{
    unsigned h[2];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        for (DWORD k = 0x560; k < 0x5a0; k += 4)
            D(g_app, k) = random();
        zs_init(g_app + 0x578);
        zs_init(g_app + 0x57c);
        if (rnd(2))
            zs_set(g_app + 0x578, MAPS[rnd(5)]);
        const DWORD pos[2] = {random(), rnd(2) ? 0x44800000u : random()};
        r[side] = at<DWORD(THISCALL *)(void *, const char *, const char *, const DWORD *, DWORD, DWORD)>(0x59b3a0)(
            g_app, MAPS[rnd(5)], MAPS[rnd(5)], pos, random(), random());
        unsigned x = fnv(2166136261u, g_app + 0x560, 0x18);
        x = fnv(x, g_app + 0x580, 0x20);
        x = fnv_text(fnv_text(x, zs_text(g_app + 0x578)), zs_text(g_app + 0x57c));
        h[side] = x;
        zs_free(g_app + 0x578);
        zs_free(g_app + 0x57c);
        side_end(side);
    }
    char d[96];
    wsprintfA(d, "[seed %u: eax %x / %x, fields %x / %x]", seed, r[0], r[1], h[0], h[1]);
    t.check(r[0] == r[1] && h[0] == h[1], "CZaxWorld::RequestSwitchMap 0x59b3a0", d);
}

// ---- Menus, players, chat (a fake application, net game and world) -------------------------------------------------

// Fakes with recording vtables: the application (g_app), the net game of its slot 1 (N), the net layer of
// the world's slot 44 (L), the world (W), parts (P), their CPlayerControlAI (A), the net layer singleton (S),
// the custom rules holder and rules (H, R) and a chat font object (F)
enum { X_NETGAME, X_LAYER, X_WORLD, X_PART, X_PART2, X_AI, X_SINGLETON, X_HOLDER, X_RULES, X_FONT, X_LEVEL, X_PACKET, X_NAME, NX };
struct XFake {
    DWORD vftable;
    DWORD id;
    BYTE body[0x2f8];
};
XFake g_x[NX];
void *g_app_vt[128], *g_ng_vt[32], *g_layer_vt[48], *g_world_vt[64], *g_part_vt[80], *g_ai_vt[64], *g_single_vt[16], *g_rules_vt[8];
CClassInfo *g_rules_class;
BYTE *x(int i) { return reinterpret_cast<BYTE *>(&g_x[i]); }
DWORD xid(const void *p)
{
    const DWORD a = ptrv(p), b = ptrv(g_x);
    if (a >= b && a < b + sizeof g_x)
        return 0xf1000000u + (a - b);
    if (a >= ptrv(g_app) && a < ptrv(g_app) + sizeof g_app)
        return 0xa9900000u + (a - ptrv(g_app));
    return a;
}
DWORD bool_ans() { const DWORD a = ans(); return (a & ~0xffu) | ((a >> 9) & 1); }

// app slots
DWORD THISCALL a_net_game(void *) { ev(0xa01); return ptrv(x(X_NETGAME)); }
DWORD THISCALL a_show(void *, void *w, DWORD flag) { ev(0xa23, w != nullptr, flag); return ans(); }
BYTE *THISCALL a_part(void *, DWORD i) { ev(0xa32, i); return ans() % 4 ? x(X_PART) : x(X_PART2); }
void THISCALL a_39(void *, void *part, DWORD i) { ev(0xa39, xid(part), i); }
void THISCALL a_104(void *, void *part, void *layer) { ev(0xa68, xid(part), xid(layer)); }
void THISCALL a_113(void *, void *layer, DWORD i, DWORD a3, DWORD a4) { ev(0xa71, xid(layer), i, a3 ^ (a4 << 4)); }
// the net game: slot 13 runs
DWORD THISCALL n_runs(void *) { ev(0xb0d); return bool_ans(); }
// the net layer: 11 the players, 13 packet of player i, 25 the server's packet
DWORD THISCALL l_count(void *) { ev(0xc0b); return ans() % 4; }
DWORD THISCALL l_packet_of(void *, DWORD i) { ev(0xc0d, i); return ptrv(x(X_PACKET)) + i * 4; }
DWORD THISCALL l_server_packet(void *) { ev(0xc19); return ptrv(x(X_PACKET)); }
DWORD THISCALL w_net(void *) { ev(0xd2c); return ptrv(x(X_LAYER)); }
BYTE *THISCALL p_find_ai(void *part, CClassInfo *c, DWORD a) { ev(0xe40, xid(part), ptrv(c), a); return ans() % 3 ? x(X_AI) : nullptr; }
void THISCALL ai_43(void *ai, DWORD a, void *part) { ev(0xf2b, xid(ai), a, xid(part)); }
void THISCALL ai_44(void *ai) { ev(0xf2c, xid(ai)); }
void THISCALL s_tell(void *n, void *player) { ev(0x909, rel(xid(n), g_side), xid(player)); }   // a new singleton by its arena offset
CClassInfo *THISCALL r_class(void *) { return g_rules_class; }
DWORD THISCALL x_bad(void *self) { ev(0x9ff, xid(self)); return 0; }

// stand-ins
void *THISCALL s_frame(void *app) { ev(0x930, xid(app)); return x(X_WORLD) + 0x100; }
void *g_menu;   // the window block the original pool gave (freed after the side)
DWORD g_menu_size;
template <DWORD SIZE> void *THISCALL s_menu_ctor(void *w, void *frame)
{
    ev(0x931, w != nullptr, xid(frame) ^ SIZE);
    g_menu = w;
    g_menu_size = SIZE;
    return ans() % 6 ? w : nullptr;
}
void THISCALL s_delete_characters(void *app) { ev(0x932, xid(app)); }
DWORD g_list_n;
void THISCALL s_list_add(void *list, void *part) { ev(0x933, xid(list), xid(part)); g_list_n++; }
DWORD THISCALL s_list_count(void *list) { ev(0x934, xid(list)); return g_list_n; }
void *g_list_entries[16];
void **THISCALL s_list_at(void *list, DWORD i) { ev(0x935, xid(list), i); return &g_list_entries[i % 16]; }
void THISCALL s_list_set(void *list, DWORD i, void *part) { ev(0x936, xid(list), i, xid(part)); g_list_entries[i % 16] = part; }
void *THISCALL s_net_local_ctor(void *p) { ev(0x937, p != nullptr); return p; }
void *__fastcall s_zax_player(DWORD i) { ev(0x938, i); return x(X_NAME) + 0x80; }
void THISCALL s_monitor_add(void *save, void *part) { ev(0x939, xid(save), xid(part)); }
void *THISCALL s_player_at(void *ng, DWORD i) { ev(0x93a, xid(ng), i); return x(X_NAME); }
ZString g_names[4];
void *THISCALL s_player_name(void *p) { ev(0x93b, xid(p)); return &g_names[ans() % 4]; }
void THISCALL s_set_name(void *part, const char *name) { ev(0x93c, xid(part), text_hash(name)); }
DWORD THISCALL s_listed(void *grid, void *part, DWORD check) { ev(0x93d, xid(grid), xid(part), check); return bool_ans(); }
DWORD THISCALL s_team_spawn(void *app, void *part, void *layer, DWORD a3, DWORD a4) { ev(0x93e, xid(app), xid(part), xid(layer) ^ a3 ^ (a4 << 8)); return ans(); }
void *__cdecl s_reader() { ev(0x93f); return x(X_WORLD) + 0x200; }
DWORD THISCALL s_write_text(void *reader, void *packet, const char *text) { ev(0x940, xid(reader), xid(packet), text_hash(text)); return bool_ans(); }
void *__cdecl s_chat_reader() { ev(0x941); return x(X_WORLD) + 0x280; }
DWORD THISCALL s_write_chat(void *reader, void *packet, const char *text, DWORD font) { ev(0x942, xid(reader), xid(packet), text_hash(text) ^ font); return ans(); }

const Stand MP_STANDS[] = {
    {0x4cdf30, (void *)&s_frame},        {0x5c09a0, (void *)&s_menu_ctor<0x13c>}, {0x5bfc40, (void *)&s_menu_ctor<0x154>},
    {0x4f4ce0, (void *)&s_delete_characters}, {0x4ef9b0, (void *)&s_list_add}, {0x4ef8f0, (void *)&s_list_count},
    {0x4ef8e0, (void *)&s_list_at},      {0x4ef900, (void *)&s_list_set},    {0x4f7df0, (void *)&s_net_local_ctor},
    {0x5ba810, (void *)&s_zax_player},   {0x4e7330, (void *)&s_monitor_add}, {0x51f440, (void *)&s_player_at},
    {0x5b7aa0, (void *)&s_player_name},  {0x4fbf00, (void *)&s_set_name},    {0x579e70, (void *)&s_listed},
    {0x4f4950, (void *)&s_team_spawn},   {0x5ac380, (void *)&s_reader},      {0x5ac440, (void *)&s_write_text},
    {0x5bcd30, (void *)&s_chat_reader},  {0x5bcd80, (void *)&s_write_chat},
};
CodePatch g_mp_patches[sizeof MP_STANDS / sizeof MP_STANDS[0]];

enum MpFn { M_DEATH, M_MENU, M_PLAYERS, M_PLAYER, M_TEXT, M_CHAT, NMP };
const char *const MP_NAMES[NMP] = {"CZaxWorld::OpenDeathScreen 0x59baf0", "CZaxWorld::OpenGameMenu 0x59bb30", "CZaxWorld::CreatePlayers 0x59bd50",
                                    "CZaxWorld::CreatePlayer 0x59df90", "NetGame_SendText 0x59b230", "NetGame_SendChatToAll 0x59b260"};
const char *const TEXTS[] = {"", "hello", "A chat line from the server", "Zax"};

void build_mp(unsigned seed)
{
    g_seed = seed;
    for (int i = 0; i < NX; i++) {
        g_x[i].id = (DWORD)i;
        for (DWORD k = 0; k < sizeof g_x[i].body; k += 4)
            D(g_x[i].body, k) = random();
    }
    g_x[X_NETGAME].vftable = ptrv(g_ng_vt);
    g_x[X_LAYER].vftable = ptrv(g_layer_vt);
    g_x[X_WORLD].vftable = ptrv(g_world_vt);
    g_x[X_PART].vftable = g_x[X_PART2].vftable = ptrv(g_part_vt);
    g_x[X_AI].vftable = ptrv(g_ai_vt);
    g_x[X_SINGLETON].vftable = ptrv(g_single_vt);
    g_x[X_RULES].vftable = ptrv(g_rules_vt);
    g_x[X_FONT].vftable = 0;
    memset(g_app, 0, sizeof g_app);
    D(g_app, 0) = ptrv(g_app_vt);
    // the custom rules: the holder +0x38 the rules (or another object), their +0x10 the chat font object
    // (+8 the font) or none
    D(g_app, 0x2e0) = ptrv(x(X_HOLDER));
    D(x(X_HOLDER), 0x38) = ptrv(x(X_RULES));   // without rules the original reads +0x10 of null
    D(x(X_RULES), 0x10) = rnd(3) ? ptrv(x(X_FONT)) : 0;
    D(x(X_FONT), 0x10) = ptrv(x(X_FONT));
    g_list_n = rnd(4);
    for (void *&e : g_list_entries)
        e = rnd(3) ? x(X_PART) : nullptr;
    g_answer_seed = random();
}
unsigned mp_state()
{
    unsigned h = fnv(2166136261u, g_x, sizeof g_x);
    h = fnv(h, g_app, sizeof g_app);
    for (void *e : g_list_entries) {
        const DWORD v = xid(e);
        h = fnv(h, &v, 4);
    }
    return fnv(h, &g_list_n, 4);
}

void test_mp(Tally &t, MpFn f, unsigned seed)
{
    DWORD eax[2], state[2], made[2];
    for (int side = 0; side < 2; side++) {
        build_mp(seed);
        log_reset(side);
        side_begin(side);
        const DWORD saved_world = *at<DWORD *>(0x6c2080), saved_app = *at<DWORD *>(0x713f14), saved_single = *at<DWORD *>(0x713f18);
        *at<DWORD *>(0x6c2080) = ptrv(x(X_WORLD));
        *at<DWORD *>(0x713f14) = ptrv(g_app);
        *at<DWORD *>(0x713f18) = rnd(3) ? ptrv(x(X_SINGLETON)) : 0;
        for (int i = 0; i < (int)(sizeof MP_STANDS / sizeof MP_STANDS[0]); i++)
            patch_jump(g_mp_patches[i], MP_STANDS[i].address, MP_STANDS[i].fn);
        // a singleton made in the case gets the real vtable 0x602fa8 written after its (stand-in)
        // constructor: its slot 9 recorded for the case
        ImportPatch tell;
        patch_import(tell, 0x602fa8 + 9 * 4, (void *)&s_tell);
        arena_on(side, (BYTE)seed);
        BYTE *level = x(X_LEVEL);
        const DWORD a3 = random(), a4 = random();
        switch (f) {
        case M_DEATH: eax[side] = at<DWORD(THISCALL *)(void *)>(0x59baf0)(g_app); break;
        case M_MENU: eax[side] = at<DWORD(THISCALL *)(void *)>(0x59bb30)(g_app); break;
        case M_PLAYERS: eax[side] = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x59bd50)(g_app, level, rnd(4), a3, a4); break;
        case M_PLAYER: eax[side] = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x59df90)(g_app, level, rnd(6), a3, a4); break;
        case M_TEXT: eax[side] = at<DWORD(__stdcall *)(const char *)>(0x59b230)(TEXTS[rnd(4)]); break;
        default: eax[side] = at<DWORD(__stdcall *)(const char *, DWORD)>(0x59b260)(TEXTS[rnd(4)], rnd(3) ? rnd(5) : 0xffffffff); break;
        }
        arena_off();
        unpatch_import(tell);
        for (int i = (int)(sizeof MP_STANDS / sizeof MP_STANDS[0]) - 1; i >= 0; i--)
            unpatch(g_mp_patches[i]);
        if (g_menu)
            pool_free(g_menu, g_menu_size);
        g_menu = nullptr;
        // what the case made: the net layer singleton (arena), the chat font object (pool 0x78), the menus
        made[side] = rel(*at<DWORD *>(0x713f18), side) ^ (D(x(X_RULES), 0x10) && D(x(X_RULES), 0x10) != ptrv(x(X_FONT)) ? 0x100u : 0);
        if (D(x(X_RULES), 0x10) && D(x(X_RULES), 0x10) != ptrv(x(X_FONT))) {
            BYTE *o = at<BYTE *>(D(x(X_RULES), 0x10));
            made[side] ^= fnv(2166136261u, o, 4);   // its vtable
            vcall<void *>(o, 1, 1);
            D(x(X_RULES), 0x10) = 0x0f0f0f0f;
        }
        *at<DWORD *>(0x6c2080) = saved_world;
        *at<DWORD *>(0x713f14) = saved_app;
        *at<DWORD *>(0x713f18) = saved_single;
        side_end(side);
        state[side] = mp_state();
    }
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, made %x / %x, log %d / %d, state %x / %x]", seed, eax[0], eax[1], made[0], made[1], g_nlog[0], g_nlog[1],
              state[0], state[1]);
    const bool same = eax[0] == eax[1] && made[0] == made[1] && same_log() && state[0] == state[1];
    if (!same)
        log_first_difference();
    t.check(same, MP_NAMES[f], d);
}

void init_tables()
{
    void **tables[8] = {g_app_vt, g_ng_vt, g_layer_vt, g_world_vt, g_part_vt, g_ai_vt, g_single_vt, g_rules_vt};
    const int sizes[8] = {128, 32, 48, 64, 80, 64, 16, 8};
    for (int k = 0; k < 8; k++)
        for (int i = 0; i < sizes[k]; i++)
            tables[k][i] = (void *)&x_bad;
    g_app_vt[1] = (void *)&a_net_game;
    g_app_vt[23] = (void *)&a_show;
    g_app_vt[32] = (void *)&a_part;
    g_app_vt[39] = (void *)&a_39;
    g_app_vt[104] = (void *)&a_104;
    g_app_vt[113] = (void *)&a_113;
    g_ng_vt[13] = (void *)&n_runs;
    g_layer_vt[11] = (void *)&l_count;
    g_layer_vt[13] = (void *)&l_packet_of;
    g_layer_vt[25] = (void *)&l_server_packet;
    g_world_vt[44] = (void *)&w_net;
    g_part_vt[64] = (void *)&p_find_ai;
    g_ai_vt[43] = (void *)&ai_43;
    g_ai_vt[44] = (void *)&ai_44;
    g_single_vt[9] = (void *)&s_tell;
    g_rules_vt[1] = (void *)&x_bad;
    g_rules_vt[2] = (void *)&r_class;
    g_rules_class = at<CClassInfo *(__cdecl *)()>(0x5a1350)();   // CZaxCustomRules
    const char *names[4] = {"Hunter", "", "Player 2", "A rather long player name for the chat"};
    for (int i = 0; i < 4; i++) {
        if (!g_names[i].buffer)
            zs_init(&g_names[i]);
        zs_set(&g_names[i], names[i]);
    }
    memcpy(g_list_vt, *at<void **>(GAME_TYPES), sizeof g_list_vt);
    g_list_vt[37] = (void *)&s_list_create;
    for (int i = 0; i < NFAKE; i++) {
        g_fake[i].vftable = 0;
        g_fake[i].id = (DWORD)i;
    }
}

}  // namespace

ZAX_CHECKS(app_flow)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_app_flow;
    init_tables();
    Tally t{"app flow: build numbers, version texts, game type, Weapon AutoSwitch, frame wrappers, map switch"};
    for (unsigned k = 0; k < 4; k++)
        test_versions(t, k);
    for (unsigned s = 0; s < (unsigned)scaled(600); s++)
        test_game_type(t, s);
    test_real_game_type(t);
    for (unsigned s = 0; s < (unsigned)scaled(300); s++)
        test_auto_switch(t, s);
    for (unsigned s = 0; s < (unsigned)scaled(300); s++) {
        test_frames(t, s, false);
        test_frames(t, s, true);
    }
    for (unsigned s = 0; s < (unsigned)scaled(300); s++)
        test_switch(t, s);
    t.report();
    Tally mp{"app flow: the death screen and the game menu, the players' characters, the net game's text and chat (fakes)"};
    if (!arena_setup()) {
        mp.check(false, "app flow", "[no arena]");
    } else {
        for (int f = 0; f < NMP; f++) {
            log("  %s", MP_NAMES[f]);
            for (unsigned s = 0; s < (unsigned)scaled(400); s++)
                test_mp(mp, (MpFn)f, 0x40000u * (unsigned)(f + 1) + s);
        }
    }
    mp.report();
}

}  // namespace zax
