// Checks of the multiplayer game type and the match state (mp_game_type.cpp) against the original, in the
// game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the original's
// first bytes put back, so nested calls through the vtable run the original too), side 1 through the
// reimplementation. Compared: eax where the reimplementation reproduces it, an event log of the fakes'
// slots and of the stand-ins, and the state afterwards (the game type object, the match state(s), the
// fake world as a whole and the font list).
//  - The world around the slots is fake: the application (0x713f14: slots 1, 44, 97 with the match state
//    at +0x30, 113, the entity list +0x284, the score board window +0x2e0), the world (0x6c2080: slot 1
//    the net game with rules, players and teams, slot 44 the connections, +0x284), entities (slot 2 an
//    entity class or not, slot 99), a surface, the ranking, and the font list 0x6be1e0 (its 0x140 bytes
//    swapped for a fake list with records, loaded or not, while a case runs). Stand-ins replace the exe
//    functions that would reach further (0x4cebb0, 0x51f480, 0x51f560, 0x51f5a0, 0x41a720, 0x5ac330,
//    0x5bacd0, 0x5bac80, 0x4ef900, 0x4fbf30, 0x4f1050, 0x4f5d60, 0x59b260, 0x48d030, 0x4a6470, 0x4a6b90,
//    0x4a6140, 0x566a80, 0x5a1fa0, 0x59ff80, 0x55e090) and record their arguments. The small getters
//    (0x5ba810, 0x51f440-0x51f460, 0x51f750, 0x5ba420/0x5ba440, 0x5b7aa0, IsKindOf, the damage list's Find,
//    the ZString functions) run for real on the fakes.
//  - The game type objects are real CDeathMatchGameType objects (Create 0x478d30, one per side) with
//    their fields set at random per case; the match state is a real CMultiPlayerGameData (the original
//    Create), the new ones slots 23 and 0x51c800 make are compared and deleted afterwards.
//  - Real data: the three game types of the game (Resources/Game Types/MultiPlayer/*.zgt, read with
//    Object_CreateFromFile, one copy per side) through every slot with the real font list (the fonts they
//    name loaded first) and their own actions; RegisterFields of both classes into a fake class info
//    compared with each other and with the descriptors registered at startup.
//  - Objects: the match state's Create (on a pattern), CreateArray (0-3), deleting destructor (single,
//    array, freeing or not); the game type's constructor on a pattern-filled block, destructor and
//    deleting destructor; both class getters with the class variable cleared.
#include "check_util.h"
#include "mp_game_type.h"
#include "field_kinds.h"
#include "definitions.h"

WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

// Event tags are four-character constants ('SEND')
#pragma clang diagnostic ignored "-Wmultichar"

// A thiscall without arguments with eax set first (al-only results keep the caller's upper bytes)
extern "C" DWORD __cdecl zax_mp_call_eax(DWORD fn, void *self, DWORD eax);
__asm__(".text\n"
        ".globl _zax_mp_call_eax\n"
        "_zax_mp_call_eax:\n"
        "  movl 8(%esp), %ecx\n"
        "  movl 12(%esp), %eax\n"
        "  calll *4(%esp)\n"
        "  retl\n");

namespace zax {

int hooks_mp_game_type(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 512;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    log("    events: %d / %d", g_nlog[0], g_nlog[1]);
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }
unsigned text_hash(const char *s) { return fnv_text(2166136261u, s ? s : "(null)"); }
void reset_side(int side)
{
    g_side = side;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
}

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 40;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_mp_game_type(nullptr);
    if (!hook_sites_fit("mp_game_type", n, MAX_SITES))
        return;
    hooks_mp_game_type(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- The fake world ------------------------------------------------------------------------------------

constexpr int NPLAYER = 8, NTEAM = 6, NCONN = 6, NENT = 8, NRANK = 6, NFONT = 6;
struct Arena {
    DWORD vt_app[120], vt_upd[16], vt_count[16], vt_world[48], vt_net[16], vt_conns[32], vt_surface[48],
        vt_entity[104], vt_object[4], vt_custom[4], vt_view[36], vt_game[48], vt_font_list[40];
    BYTE app[0x300];      // +0x284 the player entities (+0xc data, +0x10 count), +0x2e0 the score board window
    BYTE upd[8], app_conns[8], session[0x40], window[0x40], custom[8];
    BYTE world[0x300];    // +0x284 the entities
    BYTE net[0x30];       // +8 rules, +0x14 players, +0x18 count, +0x24 teams, +0x28 count
    BYTE rules[0x20];     // +0x10 time limit, +0x14 frag limit, +0x19
    BYTE conns[8];
    BYTE connection[NCONN][8];
    BYTE players[NPLAYER][0x110];   // +0x14 team, +0x18, +0x1c name (ZString *), +0xf6 score, +0xf8 lead, +0x10c
    BYTE *player_ptrs[NPLAYER];
    DWORD teams[NTEAM][3];          // +8 score
    DWORD *team_ptrs[NTEAM];
    BYTE entities[NENT][0x10];      // +4 player index (0x5bacd0), +8 player (0x5bac80)
    void *app_entities[NENT], *world_entities[NENT];
    BYTE surface[8], screen[0x40], view[8], game[8], action[3][8];
    BYTE ranking[0x40];             // +4 entries, +8 count, +0x14 order, +0x38 score limit, +0x3c time limit
    BYTE *rank_ptrs[NRANK];
    DWORD rank_order[NRANK];
    BYTE ranks[NRANK][0x20];        // +0 name (ZString), +4 flag, +8 team, +0xc score, +0x10, +0x14, +0x18
    ZDefinitionRecord font_records[NFONT];
    BYTE fonts[2 * NFONT][8];
    BYTE font_list[0x140];
    // scripted answers
    DWORD free_slot, smallest, width, height, app_conn_count, conn_count, conn_cur, counter;
    float size;
    BYTE upd_flag, server, pad[2];
};
Arena g_a;
ZString g_names[NPLAYER];
SavedBytes g_saved_fonts;
bool g_fake_fonts;

// Pointers in the log and state: the fakes by their offset, the side's objects by tag, texts by content
struct Known {
    const void *p;
    DWORD size, tag;
};
Known g_known[8];
int g_nknown;
void know(const void *p, DWORD size, DWORD tag)
{
    if (g_nknown < 8 && p)
        g_known[g_nknown++] = {p, size, tag};
}
DWORD id(const void *p)
{
    if (!p)
        return 0;
    const BYTE *b = static_cast<const BYTE *>(p), *a = reinterpret_cast<const BYTE *>(&g_a);
    if (b >= a && b < a + sizeof g_a)
        return 0x10000000 + (DWORD)(b - a);
    for (int i = 0; i < g_nknown; i++) {
        const BYTE *k = static_cast<const BYTE *>(g_known[i].p);
        if (b >= k && b < k + g_known[i].size)
            return g_known[i].tag + (DWORD)(b - k);
    }
    for (int i = 0; i < NPLAYER; i++)
        if (p == &g_names[i])
            return 0x18000000 + (DWORD)i;
    return ptrv(p);
}

// Fake slots
BYTE *THISCALL f_app_upd(BYTE *) { return g_a.upd; }
BYTE *THISCALL f_app_conns(BYTE *) { return g_a.app_conns; }
BYTE *THISCALL f_app_session(BYTE *) { return g_a.session; }
DWORD THISCALL f_app_113(BYTE *self, DWORD x, int i, DWORD a, DWORD b)
{
    ev('A113', id(self), x, (DWORD)i);
    ev('A113', a, b);
    return 0x1130 + (DWORD)i;
}
BYTE THISCALL f_upd_flag(BYTE *) { return g_a.upd_flag; }
DWORD THISCALL f_upd_13(BYTE *self)
{
    ev('UP13', id(self));
    return 0x1300;
}
DWORD THISCALL f_app_conn_count(BYTE *) { return g_a.app_conn_count; }
BYTE *THISCALL f_world_net(BYTE *) { return g_a.net; }
BYTE *THISCALL f_world_conns(BYTE *) { return g_a.conns; }
BYTE THISCALL f_net_server(BYTE *) { return g_a.server; }
DWORD THISCALL f_conn_count(BYTE *) { return g_a.conn_count; }
BYTE *THISCALL f_conn_at(BYTE *, DWORD i)
{
    ev('CAT', i);
    return i < NCONN ? g_a.connection[i] : nullptr;
}
DWORD THISCALL f_conn_set(BYTE *, DWORD i)
{
    ev('CSET', i);
    g_a.conn_cur = i;
    return 0x2700 + i;
}
DWORD THISCALL f_conn_get(BYTE *) { return g_a.conn_cur; }
DWORD THISCALL f_surface_26(BYTE *self, BYTE *rect)
{
    ev('S26', id(self), id(rect));
    return 0x26;
}
DWORD THISCALL f_surface_height(BYTE *) { return g_a.height; }
DWORD THISCALL f_surface_width(BYTE *) { return g_a.width; }
CClassInfo *THISCALL f_entity_class(BYTE *) { return at<CClassInfo *(__cdecl *)()>(0x49a930)(); }
CClassInfo *THISCALL f_object_class(BYTE *) { return at<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
CClassInfo *THISCALL f_custom_class(BYTE *) { return at<CClassInfo *(__cdecl *)()>(0x5a1350)(); }
DWORD THISCALL f_entity_99(BYTE *self, float amount, int type, BYTE *by)
{
    ev('E99', id(self), bits(amount), (DWORD)type);
    ev('E99b', id(by));
    return 0x99;
}
DWORD THISCALL f_view_33(BYTE *self)
{
    ev('V33', id(self));
    return 0x3300 + g_a.counter++;
}
ZString *THISCALL f_game_45(BYTE *) { return &g_names[NPLAYER - 1]; }
const char *THISCALL f_font_name(BYTE *, DWORD i) { return i < NFONT ? g_a.font_records[i].name : "Out Of Range"; }
void *THISCALL f_font_create(BYTE *, const char *dir, const char *name, void **out)
{
    ev('FCR', text_hash(dir), text_hash(name), id(out));
    return g_a.fonts[NFONT + (g_a.counter++ % NFONT)];
}

// Stand-ins for exe functions
DWORD THISCALL s_base23(void *self)
{
    ev('B23', id(self));
    return 0x4ceb;
}
DWORD THISCALL s_set_team_count(void *ng, DWORD n)
{
    ev('STC', id(ng), n);
    return 0x480;
}
DWORD THISCALL s_free_slot(void *ng)
{
    ev('FPS', id(ng));
    return g_a.free_slot;
}
DWORD THISCALL s_smallest(void *ng, DWORD n)
{
    ev('SMT', id(ng), n);
    return g_a.smallest;
}
DWORD THISCALL s_fire(void *action, DWORD a, DWORD b, DWORD c, DWORD d, void *self, void **slot)
{
    ev('FIRE', id(action), id(self), id(slot));
    ev('FIR2', a, b, c + d * 16);
    return 0x41a7;
}
DWORD THISCALL s_send(void *reader, void *connection)
{
    ev('SEND', ptrv(reader), id(connection));
    return 0x5ac3;
}
int THISCALL s_player_index(BYTE *e)
{
    ev('PIX', id(e));
    return e ? *reinterpret_cast<int *>(e + 4) : -1;
}
BYTE *THISCALL s_player_of(BYTE *e)
{
    ev('POF', id(e));
    return e ? *reinterpret_cast<BYTE **>(e + 8) : nullptr;
}
DWORD THISCALL s_remove(void *list, int i, DWORD z)
{
    ev('RMV', id(list), (DWORD)i, z);
    return 0x4ef9;
}
DWORD THISCALL s_died(void *e)
{
    ev('DIED', id(e));
    return 0x4fbf;
}
DWORD THISCALL s_4f1050(void *a)
{
    ev('F105', id(a));
    return 0x4f10;
}
BYTE *THISCALL s_view(void *a, int i)
{
    ev('VIEW', id(a), (DWORD)i);
    return g_a.view;
}
DWORD THISCALL s_show(void *a, const char *text, int n)
{
    ev('SHOW', id(a), text_hash(text), (DWORD)n);
    return 0x59b2;
}
float THISCALL s_entity_size(void *e)
{
    ev('SIZE', id(e));
    return g_a.size;
}
DWORD THISCALL s_draw_text(void *font, void *surface, DWORD x, DWORD y, DWORD text, DWORD just, DWORD z)
{
    ev('DTXT', id(font), id(surface), x);
    ev('DTX2', y, text_hash(reinterpret_cast<const char *>(text)), just + z * 16);
    return 0x6470;
}
DWORD THISCALL s_draw_number(void *font, void *surface, DWORD x, DWORD y, DWORD value, DWORD just)
{
    ev('DNUM', id(font), id(surface), x);
    ev('DNU2', y, value, just);
    return 0x6b90;
}
DWORD THISCALL s_font_height(void *font)
{
    ev('FHGT', id(font));
    return 5 + (id(font) >> 3) % 7;
}
BYTE *__cdecl s_ranking()
{
    ev('RANK');
    return g_a.ranking;
}
BYTE *THISCALL s_screen(void *custom)
{
    ev('SCRN', id(custom));
    return g_a.screen;
}
BYTE *THISCALL s_game(void *a)
{
    ev('GAME', id(a));
    return g_a.game;
}
void *THISCALL s_load_font(BYTE *list, DWORD i)
{
    ev('LOAD', id(list), i);
    if (g_fake_fonts && i < NFONT)
        return g_a.font_records[i].object = g_a.fonts[i];
    return nullptr;
}

struct StandIn {
    DWORD address;
    void *fn;
};
const StandIn STAND_INS[] = {
    {0x4cebb0, (void *)&s_base23},      {0x51f480, (void *)&s_set_team_count}, {0x51f560, (void *)&s_free_slot},
    {0x51f5a0, (void *)&s_smallest},    {0x41a720, (void *)&s_fire},           {0x5ac330, (void *)&s_send},
    {0x5bacd0, (void *)&s_player_index}, {0x5bac80, (void *)&s_player_of},     {0x4ef900, (void *)&s_remove},
    {0x4fbf30, (void *)&s_died},        {0x4f1050, (void *)&s_4f1050},         {0x4f5d60, (void *)&s_view},
    {0x59b260, (void *)&s_show},        {0x48d030, (void *)&s_entity_size},    {0x4a6470, (void *)&s_draw_text},
    {0x4a6b90, (void *)&s_draw_number}, {0x4a6140, (void *)&s_font_height},    {0x566a80, (void *)&s_ranking},
    {0x5a1fa0, (void *)&s_screen},      {0x59ff80, (void *)&s_game},           {0x55e090, (void *)&s_load_font},
};
constexpr int NSTAND = sizeof STAND_INS / sizeof STAND_INS[0];
CodePatch g_patches[NSTAND];
BYTE *g_saved_app, *g_saved_world;
void patches_on()
{
    for (int i = 0; i < NSTAND; i++)
        patch_jump(g_patches[i], STAND_INS[i].address, STAND_INS[i].fn);
    g_saved_app = *at<BYTE **>(MP_APP);
    g_saved_world = *at<BYTE **>(MP_WORLD);
    *at<BYTE **>(MP_APP) = g_a.app;
    *at<BYTE **>(MP_WORLD) = g_a.world;
    if (g_fake_fonts) {
        save_bytes(g_saved_fonts, FONT_LIST, sizeof g_a.font_list);
        memcpy(at<void *>(FONT_LIST), g_a.font_list, sizeof g_a.font_list);
    }
}
void patches_off()
{
    if (g_fake_fonts) {
        memcpy(g_a.font_list, at<void *>(FONT_LIST), sizeof g_a.font_list);   // as the case left it
        restore_bytes(g_saved_fonts);
    }
    *at<BYTE **>(MP_APP) = g_saved_app;
    *at<BYTE **>(MP_WORLD) = g_saved_world;
    for (int i = NSTAND - 1; i >= 0; i--)
        unpatch(g_patches[i]);
}

template <typename F> void set_slot(DWORD *vt, int i, F f) { vt[i] = ptrv((void *)f); }
template <typename T> T &field(BYTE *o, DWORD off) { return *reinterpret_cast<T *>(o + off); }
DWORD rnd(DWORD n) { return random() % n; }

// The world for one case, from the current seed (the same on both sides)
void build_world()
{
    memset(&g_a, 0, sizeof g_a);
    set_slot(g_a.vt_app, 1, &f_app_upd);
    set_slot(g_a.vt_app, 44, &f_app_conns);
    set_slot(g_a.vt_app, 97, &f_app_session);
    set_slot(g_a.vt_app, 113, &f_app_113);
    set_slot(g_a.vt_upd, 12, &f_upd_flag);
    set_slot(g_a.vt_upd, 13, &f_upd_13);
    set_slot(g_a.vt_count, 11, &f_app_conn_count);
    set_slot(g_a.vt_world, 1, &f_world_net);
    set_slot(g_a.vt_world, 44, &f_world_conns);
    set_slot(g_a.vt_net, 12, &f_net_server);
    set_slot(g_a.vt_conns, 11, &f_conn_count);
    set_slot(g_a.vt_conns, 13, &f_conn_at);
    set_slot(g_a.vt_conns, 27, &f_conn_set);
    set_slot(g_a.vt_conns, 28, &f_conn_get);
    set_slot(g_a.vt_surface, 26, &f_surface_26);
    set_slot(g_a.vt_surface, 39, &f_surface_height);
    set_slot(g_a.vt_surface, 45, &f_surface_width);
    set_slot(g_a.vt_entity, 2, &f_entity_class);
    set_slot(g_a.vt_entity, 99, &f_entity_99);
    set_slot(g_a.vt_object, 2, &f_object_class);
    set_slot(g_a.vt_custom, 2, &f_custom_class);
    set_slot(g_a.vt_view, 33, &f_view_33);
    set_slot(g_a.vt_game, 45, &f_game_45);
    set_slot(g_a.vt_font_list, 1, &f_font_name);
    set_slot(g_a.vt_font_list, 37, &f_font_create);

    field<DWORD>(g_a.app, 0) = ptrv(g_a.vt_app);
    field<DWORD>(g_a.upd, 0) = ptrv(g_a.vt_upd);
    field<DWORD>(g_a.app_conns, 0) = ptrv(g_a.vt_count);
    field<DWORD>(g_a.world, 0) = ptrv(g_a.vt_world);
    field<DWORD>(g_a.net, 0) = ptrv(g_a.vt_net);
    field<DWORD>(g_a.conns, 0) = ptrv(g_a.vt_conns);
    field<DWORD>(g_a.surface, 0) = ptrv(g_a.vt_surface);
    field<DWORD>(g_a.view, 0) = ptrv(g_a.vt_view);
    field<DWORD>(g_a.game, 0) = ptrv(g_a.vt_game);
    field<DWORD>(g_a.custom, 0) = ptrv(rnd(2) ? g_a.vt_custom : g_a.vt_object);
    field<BYTE *>(g_a.app, 0x2e0) = g_a.window;
    field<BYTE *>(g_a.window, 0x38) = rnd(3) ? g_a.custom : nullptr;

    g_a.upd_flag = (BYTE)rnd(2);
    g_a.server = (BYTE)rnd(2);
    g_a.app_conn_count = rnd(9);
    g_a.conn_count = rnd(NCONN + 1);
    g_a.conn_cur = rnd(5);
    g_a.free_slot = rnd(20);
    g_a.smallest = rnd(4);
    g_a.width = 200 + rnd(300);
    g_a.height = 150 + rnd(300);
    g_a.size = (float)(int)rnd(500) / 4.0f;

    // Rules of the net game: none in some cases, limits 0 or small
    field<BYTE *>(g_a.net, 8) = rnd(5) ? g_a.rules : nullptr;
    field<DWORD>(g_a.rules, 0x10) = rnd(3) ? rnd(4) : 0;
    field<DWORD>(g_a.rules, 0x14) = rnd(3) ? rnd(30) : 0;
    g_a.rules[0x19] = (BYTE)random();

    // Players: teams 0-3, scores with ties, lead states 0-3
    const DWORD players = rnd(NPLAYER + 1);
    for (DWORD i = 0; i < NPLAYER; i++) {
        BYTE *p = g_a.players[i];
        g_a.player_ptrs[i] = p;
        field<DWORD>(p, 0x14) = rnd(4);
        p[0x18] = (BYTE)random();
        field<ZString *>(p, 0x1c) = &g_names[i];
        field<short>(p, 0xf6) = (short)(rnd(6) - 1);
        p[0xf8] = (BYTE)rnd(4);
        field<DWORD>(p, 0x10c) = rnd(3);
    }
    field<BYTE **>(g_a.net, 0x14) = g_a.player_ptrs;
    field<DWORD>(g_a.net, 0x18) = players;
    const DWORD teams = rnd(NTEAM + 1);
    for (DWORD i = 0; i < NTEAM; i++) {
        g_a.teams[i][2] = rnd(30);
        g_a.team_ptrs[i] = g_a.teams[i];
    }
    field<DWORD **>(g_a.net, 0x24) = g_a.team_ptrs;
    field<DWORD>(g_a.net, 0x28) = teams;

    // Entities: some not entities (slot 2 gives CObject), player indices and players
    for (DWORD i = 0; i < NENT; i++) {
        BYTE *e = g_a.entities[i];
        field<DWORD>(e, 0) = ptrv(rnd(4) ? g_a.vt_entity : g_a.vt_object);
        field<int>(e, 4) = rnd(4) ? (int)rnd(NPLAYER) : -1;
        field<BYTE *>(e, 8) = rnd(5) ? g_a.players[rnd(NPLAYER)] : nullptr;
        g_a.app_entities[i] = rnd(4) ? e : nullptr;
        g_a.world_entities[i] = rnd(4) ? e : nullptr;
    }
    field<void **>(g_a.app, 0x284 + 0xc) = g_a.app_entities;
    field<DWORD>(g_a.app, 0x284 + 0x10) = rnd(NENT + 1);
    field<void **>(g_a.world, 0x284 + 0xc) = g_a.world_entities;
    field<DWORD>(g_a.world, 0x284 + 0x10) = rnd(NENT + 1);

    // The ranking of the score board
    const DWORD ranks = rnd(NRANK + 1);
    for (DWORD i = 0; i < NRANK; i++) {
        BYTE *r = g_a.ranks[i];
        g_a.rank_ptrs[i] = r;
        g_a.rank_order[i] = rnd(NRANK);
        field<ZStringBuffer *>(r, 0) = g_names[rnd(NPLAYER)].buffer;
        r[4] = (BYTE)(rnd(3) ? 0 : random());
        field<DWORD>(r, 8) = rnd(3);
        field<DWORD>(r, 0xc) = rnd(50);
        field<DWORD>(r, 0x10) = rnd(2) ? rnd(9) : 0;
        field<DWORD>(r, 0x14) = rnd(2) ? rnd(9) : 0;
        field<DWORD>(r, 0x18) = rnd(400);
    }
    field<BYTE **>(g_a.ranking, 4) = g_a.rank_ptrs;
    field<DWORD>(g_a.ranking, 8) = ranks;
    field<DWORD *>(g_a.ranking, 0x14) = g_a.rank_order;
    field<DWORD>(g_a.ranking, 0x38) = rnd(2) ? rnd(30) : 0;
    field<DWORD>(g_a.ranking, 0x3c) = rnd(2) ? rnd(30) : 0;

    // The font list: records loaded, missing or being loaded, names with and without a folder
    const char *const FONT_NAMES[] = {"Small Green", "Sub/Small Blue", "Small Red", "A/B/Number Grn", "", "Big"};
    for (DWORD i = 0; i < NFONT; i++) {
        ZDefinitionRecord &r = g_a.font_records[i];
        lstrcpyA(r.name, FONT_NAMES[i]);
        const DWORD k = rnd(5);
        r.object = k < 3 ? g_a.fonts[i] : nullptr;
        r.loading = k == 4;
    }
    BYTE *fl = g_a.font_list;
    field<DWORD>(fl, 0) = ptrv(g_a.vt_font_list);
    field<ZDefinitionRecord *>(fl, 0x30) = rnd(6) ? g_a.font_records : nullptr;
    field<DWORD>(fl, 0x34) = 1 + rnd(NFONT);
    lstrcpyA(reinterpret_cast<char *>(fl + 0x38), rnd(2) ? "Cache/Fonts" : "");
}

// The game type object's fields from the seed (actions: the fake ones or none)
void randomize_game_type(CMultiPlayerGameType *t)
{
    t->max_teams = rnd(3) ? rnd(5) : 0;
    t->is_team_game = (BYTE)rnd(2);
    t->share_spawn_points = (BYTE)rnd(2);
    t->taken_lead = rnd(3) ? g_a.action[0] : nullptr;
    t->lost_lead = rnd(3) ? g_a.action[1] : nullptr;
    t->tied_lead = rnd(3) ? g_a.action[2] : nullptr;
    DWORD *fonts = &t->mini_font;
    for (int i = 0; i < 6; i++)
        fonts[i] = rnd(NFONT + 2);
    DWORD *cols = &t->mini_rank_x;
    for (int i = 0; i < 14; i++)
        cols[i] = i % 2 ? rnd(3) : rnd(200);
    t->mini_rules_just = rnd(4);
    t->mini_line_spacing = rnd(5);
    t->mini_header_spacing = rnd(6);
    t->mini_top = rnd(10);
    // the end score board's fields: Create leaves them as the heap block had them, so the two objects
    // differed once the heap's history did (2026-10-25, after the inventory action checks loaded every
    // record of the item catalogue)
    memset(t->end_board, 0, sizeof t->end_board);
}

// ---- Cases ---------------------------------------------------------------------------------------------

enum Fn { F_START, F_UPDATE, F_SCORE, F_LEADS, F_ASSIGN, F_SLOT29, F_TIME, F_SET_TIME, F_SHARE, F_TEAM, F_SLOT38,
          F_DRAW, F_REMOVED, F_CHANGE, F_KILL, F_END, F_READY, NFN };
const char *const FN_NAMES[NFN] = {"StartMatch", "Update", "AddTeamScore", "UpdateLeads", "AssignTeam", "Slot29",
                                   "MatchTime", "SetMatchTime", "ShareSpawnPoints", "IsTeamGame", "Slot38",
                                   "DrawMiniScoreBoard", "PlayerRemoved", "ChangeTeam", "KillMessage", "EndMatch",
                                   "PlayerReady"};
const bool FN_EAX[NFN] = {true, false, false, true, true, true, true, false, true, true, true,
                          false, true, true, true, false, false};

CMultiPlayerGameType *g_obj[2];
CMultiPlayerGameType *g_real[2][3];

using Create0 = void *(__cdecl *)();
using Dtor1 = void *(THISCALL *)(void *, DWORD);
CMultiPlayerGameData *new_game_data() { return static_cast<CMultiPlayerGameData *>(original<Create0>(0x51a040)()); }
void delete_game_data(void *d) { original<Dtor1>(0x51a0d0)(d, 1); }

unsigned g_part[2][2];   // the state hash after the game type and after the match states (for the log)
const BYTE *g_case_object[2];

// The cases without a match state (run_side): 1 null at +0x30, 2 an object of another class; the faults seen and
// those that were not an access violation near 0
int g_no_state;
DWORD g_no_state_faults, g_no_state_bad;

// One side of a case: the world from the seed, the function, the state afterwards
DWORD run_side(Fn fn, CMultiPlayerGameType *t, bool real, DWORD seed)
{
    g_seed = seed;
    build_world();
    g_nknown = 0;
    if (!real)
        randomize_game_type(t);
    else
        t->is_team_game = (BYTE)rnd(2) ? t->is_team_game : (BYTE)!t->is_team_game;   // both kinds of game
    know(t, 0x100, 0x20000000);
    if (real) {
        know(t->taken_lead, 8, 0x21000000);
        know(t->lost_lead, 8, 0x22000000);
        know(t->tied_lead, 8, 0x23000000);
    }
    CMultiPlayerGameData *d = new_game_data();
    know(d, sizeof *d, 0x30000000);
    d->match_over = (BYTE)rnd(2);
    d->players_ready = rnd(6);
    d->match_time = (double)(int)rnd(400) + (double)(int)rnd(8) / 8.0;
    field<CMultiPlayerGameData *>(g_a.session, 0x30) = d;
    // Arguments, also from the seed
    const DWORD a1 = rnd(NPLAYER + 2), a2 = rnd(40) - 10;
    BYTE *e1 = rnd(6) ? g_a.entities[rnd(NENT)] : nullptr, *e2 = rnd(4) ? g_a.entities[rnd(NENT)] : nullptr;
    float f = (float)(int)rnd(64) / 16.0f;
    if (fn == F_UPDATE && field<BYTE *>(g_a.net, 8) && rnd(2))   // close to the time limit
        d->match_time = (double)(field<DWORD>(g_a.rules, 0x10) * 60) - (double)f + (double)((int)rnd(5) - 2) / 4.0;
    const DWORD eax_in = random();
    g_fake_fonts = !real;
    g_a.counter = 0;
    patches_on();
    if (g_side == 0)
        hooks_out();
    DWORD r = 0;
    if (g_no_state) {
        // Without a match state (batch decomp-faithful2): +0x30 null or an object of another class (the game type
        // itself, which IsKindOf refuses); the exe then reads or writes the field through null. The call through
        // call_faulting: the exception's code, access kind and address are compared (not the instruction's
        // address, which is the exe's on one side and ours on the other).
        field<void *>(g_a.session, 0x30) = g_no_state == 1 ? nullptr : static_cast<void *>(t);
        DWORD addr = 0, args[2] = {0, 0}, n = 0;
        switch (fn) {
        case F_UPDATE: addr = 0x51c310, args[0] = bits(f), n = 1; break;
        case F_SCORE: addr = 0x51bb90, args[0] = a1 % (NTEAM + 1), args[1] = a2, n = 2; break;
        case F_TIME: addr = 0x51c1f0; break;
        case F_SET_TIME: addr = 0x51c270, args[0] = bits(f), n = 1; break;
        case F_END: addr = 0x51c650; break;
        default: addr = 0x51c800; break;   // F_READY
        }
        Fault fault;
        r = call_faulting(fault, at<void *>(addr), ptrv(t), args, n);
        if (fn == F_TIME && !fault.code)
            __asm__ volatile("fstp %%st(0)" ::: "st");   // the match time it returned
        st(fault.code);
        st(fault.kind);
        st(fault.address);
        g_no_state_faults += fault.code != 0;
        g_no_state_bad += fault.code && (fault.code != 0xc0000005u || fault.address >= 0x18);
        field<CMultiPlayerGameData *>(g_a.session, 0x30) = d;
        fn = NFN;   // nothing more to call
    }
    switch (fn) {
    case F_START: r = at<DWORD(THISCALL *)(void *)>(0x51c010)(t); break;
    case F_UPDATE: at<void(THISCALL *)(void *, float)>(0x51c310)(t, f); break;
    case F_SCORE: at<void(THISCALL *)(void *, DWORD, int)>(0x51bb90)(t, a1 % (NTEAM + 1), (int)a2); break;
    case F_LEADS: r = at<DWORD(THISCALL *)(void *)>(0x51bda0)(t); break;
    case F_ASSIGN: r = at<DWORD(THISCALL *)(void *, BYTE *)>(0x51bef0)(t, g_a.players[a1 % NPLAYER]); break;
    case F_SLOT29: r = at<DWORD(THISCALL *)(void *, DWORD, void *, DWORD)>(0x51d190)(t, a2, e1, a1); break;
    case F_TIME: {
        const double v = at<double(THISCALL *)(void *)>(0x51c1f0)(t);
        DWORD w[2];
        memcpy(w, &v, 8);
        r = w[0] ^ w[1] * 3;
        break;
    }
    case F_SET_TIME: at<void(THISCALL *)(void *, float)>(0x51c270)(t, f); break;
    case F_SHARE: r = zax_mp_call_eax(0x468f20, t, eax_in); break;
    case F_TEAM: r = zax_mp_call_eax(0x51d150, t, eax_in); break;
    case F_SLOT38: r = at<DWORD(THISCALL *)(void *)>(0x51d160)(t); r = (r & 0xff) | (id(reinterpret_cast<void *>(r & ~0xffu)) << 8); break;
    case F_DRAW: at<void(THISCALL *)(void *, void *)>(0x51cba0)(t, g_a.surface); break;
    case F_REMOVED: r = at<DWORD(THISCALL *)(void *, void *)>(0x51d1a0)(t, e1); break;
    case F_CHANGE: r = at<DWORD(THISCALL *)(void *, DWORD)>(0x51d240)(t, a1 % NPLAYER); break;
    case F_KILL: r = at<DWORD(THISCALL *)(void *, void *, void *)>(0x51d400)(t, e1, e2); break;
    case F_END: at<void(THISCALL *)(void *)>(0x51c650)(t); break;
    case F_READY: at<void(THISCALL *)(void *)>(0x51c800)(t); break;
    default: break;
    }
    if (g_side == 0)
        hooks_in();
    patches_off();
    // State: the game type's own part, the match state(s), the fake world (the state pointer as its tag)
    st_bytes(reinterpret_cast<BYTE *>(t) + 0x18, 6);   // +0x1e/+0x1f padding
    st_bytes(reinterpret_cast<BYTE *>(t) + 0x2c, 0x44 - 0x2c);
    st(text_hash(t->mini_blue_flag ? reinterpret_cast<const char *>(t->mini_blue_flag) : nullptr));   // own copies when read from a file
    st(text_hash(t->mini_red_flag ? reinterpret_cast<const char *>(t->mini_red_flag) : nullptr));
    st_bytes(reinterpret_cast<BYTE *>(t) + 0x4c, 0xc8 - 0x4c);
    st(id(t->taken_lead));
    st(id(t->lost_lead));
    st(id(t->tied_lead));
    g_part[g_side][0] = g_statehash[g_side];
    auto *now = field<CMultiPlayerGameData *>(g_a.session, 0x30);
    st_bytes(reinterpret_cast<BYTE *>(d), sizeof *d);
    if (now != d) {
        know(now, sizeof *now, 0x31000000);
        st(id(now));
        st_bytes(reinterpret_cast<BYTE *>(now) + 8, 8);   // +0x10 is not set by Create
        field<CMultiPlayerGameData *>(g_a.session, 0x30) = nullptr;
        if (now)
            delete_game_data(now);
    }
    field<CMultiPlayerGameData *>(g_a.session, 0x30) = nullptr;
    g_part[g_side][1] = g_statehash[g_side];
    st_bytes(&g_a, sizeof g_a);
    delete_game_data(d);
    return r;
}

void run_case(Tally &t, Fn fn, bool real, int k, DWORD seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        reset_side(side);
        r[side] = run_side(fn, real ? g_real[side][k] : g_obj[side], real, seed);
        g_case_object[side] = reinterpret_cast<const BYTE *>(real ? g_real[side][k] : g_obj[side]);
    }
    const bool same = g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] && g_statehash[0] == g_statehash[1] &&
                      (!FN_EAX[fn] || r[0] == r[1]);
    char d[160];
    wsprintfA(d, "[%s %s%d seed %x: eax %x / %x, %d / %d events, state %x / %x]", FN_NAMES[fn], real ? "real " : "", k, seed, r[0],
              r[1], g_nlog[0], g_nlog[1], g_statehash[0], g_statehash[1]);
    if (!same && t.mismatches < 4) {
        log_first_difference();
        log("    state parts %x %x / %x %x", g_part[0][0], g_part[0][1], g_part[1][0], g_part[1][1]);
        if (g_part[0][0] != g_part[1][0])
            for (DWORD off = 0x18; off < 0xc8; off += 4) {
                const DWORD a = *reinterpret_cast<const DWORD *>(g_case_object[0] + off), b = *reinterpret_cast<const DWORD *>(g_case_object[1] + off);
                if (a != b)
                    log("    +%x: %x / %x", off, a, b);
            }
    }
    t.check(same, "CMultiPlayerGameType", d);
}

// ---- Objects --------------------------------------------------------------------------------------------

using CreateN = void *(__fastcall *)(int, int, int);
void test_objects(Tally &t)
{
    for (int c = 0; c < 40; c++) {
        DWORD res[2];
        for (int side = 0; side < 2; side++) {
            reset_side(side);
            g_seed = 0x5000 + (DWORD)c;
            if (side == 0)
                hooks_out();
            DWORD r = 0;
            const DWORD kind = c % 4;
            if (kind == 0) {
                // Create on a pattern, fields changed, deleting destructor with or without freeing
                BYTE *p = static_cast<BYTE *>(pool_alloc(0x18));
                memset(p, (BYTE)random(), 0x18);
                pool_free(p, 0x18);
                BYTE *o = static_cast<BYTE *>(at<Create0>(0x51a040)());
                st(o == p);
                st_bytes(o, 0x18);
                o[8] = (BYTE)random();
                const DWORD flags = rnd(2);
                r = at<void *(THISCALL *)(void *, DWORD)>(0x51a0d0)(o, flags) == o;
                st_bytes(o, 4);
                if (!flags)
                    pool_free(o, 0x18);
            } else if (kind == 1) {
                const int n = (int)rnd(4);
                BYTE *e = static_cast<BYTE *>(at<CreateN>(0x51a070)(n, 0, 0));
                st(e != nullptr);
                if (e) {
                    st(*reinterpret_cast<DWORD *>(e - 4));
                    for (int i = 0; i < n; i++)
                        st_bytes(e + i * 0x18, 0x10);
                    const DWORD flags = 2 | rnd(2);
                    r = at<BYTE *(THISCALL *)(void *, DWORD)>(0x51a0d0)(e, flags) == e - 4;
                    if (!(flags & 1))
                        heap_free(e - 4);
                }
            } else {
                // The game type's constructor on a pattern, then a destructor
                BYTE *p = static_cast<BYTE *>(pool_alloc(0xc8));
                memset(p, (BYTE)random(), 0xc8);
                BYTE *o = at<BYTE *(THISCALL *)(void *)>(0x51bfa0)(p);
                st(o == p);
                st(field<DWORD>(p, 0));
                // the fields as the defaults set them (the texts by content: their copies differ)
                st_bytes(p + 0x18, 0x2c);
                st(text_hash(field<const char *>(p, 0x44)));
                st(text_hash(field<const char *>(p, 0x48)));
                st_bytes(p + 0x4c, 0xc8 - 0x4c);
                if (kind == 2) {
                    r = at<DWORD(THISCALL *)(void *)>(0x51bff0)(p);
                    st(field<DWORD>(p, 0));
                    pool_free(p, 0xc8);
                } else {
                    const DWORD flags = rnd(2);
                    r = at<BYTE *(THISCALL *)(void *, DWORD)>(0x51bfc0)(p, flags) == p;
                    if (!flags) {
                        st(field<DWORD>(p, 0));
                        pool_free(p, 0xc8);
                    }
                }
            }
            if (side == 0)
                hooks_in();
            res[side] = r;
        }
        char d[120];
        wsprintfA(d, "[object case %d: eax %x / %x, state %x / %x]", c, res[0], res[1], g_statehash[0], g_statehash[1]);
        t.check(res[0] == res[1] && g_statehash[0] == g_statehash[1], "objects", d);
    }
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create),
                  ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    return lstrcmpA(a.c_str(), b.c_str()) == 0;
}
bool same_names(const FieldNameList *a, const FieldNameList *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || a->entries.count != b->entries.count)
        return false;
    for (DWORD i = 0; i < a->entries.count; i++) {
        const DWORD *x = static_cast<const DWORD *>(a->entries.data[i]), *y = static_cast<const DWORD *>(b->entries.data[i]);
        if (x[1] != y[1] || lstrcmpA(reinterpret_cast<const char *>(x[0]), reinterpret_cast<const char *>(y[0])))
            return false;
    }
    return true;
}
bool same_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || !same_text(a->name, b->name) || !same_text(a->help, b->help) ||
        !same_text(a->group, b->group) || !same_text(a->alt_name, b->alt_name) || !same_text(a->units, b->units) ||
        a->offset != b->offset || a->unknown14 != b->unknown14 || a->unknown18 != b->unknown18)
        return false;   // +0x24 is not written by the constructors
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    auto same = [&](DWORD from, DWORD to) { return memcmp(x + from, y + from, to - from) == 0; };
    switch (a->vftable) {
    case VT_BYTE: return same(0x28, 0x2d);
    case VT_ENUM_INT:
        return same(0x28, 0x40) && same_names(static_cast<const CEnumIntFieldDesc *>(a)->names, static_cast<const CEnumIntFieldDesc *>(b)->names);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_TYPED_RESOURCE: return same(0x28, 0x2e) && same(0x30, 0x38);   // +0x2e, +0x2f padding
    case VT_TEXT: return same(0x28, 0x30);
    case VT_INT: return same(0x28, 0x40);
    }
    return false;
}
void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' help '%.40s' group '%s' off %x %x %x", what, f->vftable, text(f->name), text(f->help), text(f->group),
        f->offset, f->unknown14, f->unknown18);
    log("      %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15]);
}

struct RegFields {
    const char *name;
    DWORD address, var;
};
const RegFields REGISTER_FIELDS[] = {{"CMultiPlayerGameData", 0x51a160, MP_GAME_DATA_CLASS},
                                     {"CMultiPlayerGameType", 0x51a320, MP_GAME_TYPE_CLASS}};
constexpr DWORD MAXF = 48;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, result;
};
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    out.result = at<DWORD(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < MAXF; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_classes(Tally &t, Tally &real)
{
    const DWORD GETTERS[][2] = {{0x51a270, MP_GAME_TYPE_CLASS}, {0x51a2c0, MP_GAME_TYPE_CLASS}};
    for (const auto &g : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(g[1]);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<CClassInfo *(__cdecl *)()>(g[0])());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<CClassInfo *(__cdecl *)()>(g[0])();
            unpatch(p);
            results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
            calls[side] = g_register;
            if (var)
                pool_free(var, sizeof(CClassInfo));
            var = saved;
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, abstract %x / %x]", g[0], set[0], set[1], results[0],
                  results[1], calls[0].size, calls[1].size, calls[0].abstract, calls[1].abstract);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "getter", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                if (side == 0)
                    hooks_in();
            }
            // eax: AddField's (the new count, the class's)
            bool same = m[0].n == m[1].n && m[0].result == m[1].result;
            for (DWORD i = 0; i < m[0].n && i < MAXF && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[140];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x]", rf.name, var_zero, m[0].n, m[1].n, m[0].result,
                      m[1].result);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++)
                    if (!same_desc(m[0].fields[i], m[1].fields[i])) {
                        log_desc("original", m[0].fields[i]);
                        log_desc("reimpl", m[1].fields[i]);
                        break;
                    }
            t.check(same, rf.name, d);
            // Real data: the class's own descriptors as the original registered them at startup
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == m[1].n;
            for (DWORD i = 0; same_real && i < m[1].n && i < MAXF; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[1].fields[i]);
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c ? c->num_fields - c->num_inherited : 0, m[1].n);
            if (!same_real && real.mismatches < 2)
                for (DWORD i = 0; i < m[1].n && i < MAXF; i++)
                    if (!same_desc(c->fields[c->num_inherited + i], m[1].fields[i])) {
                        log_desc("registered", c->fields[c->num_inherited + i]);
                        log_desc("made", m[1].fields[i]);
                        break;
                    }
            real.check(same_real, rf.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
}

// ---- Real game types ----------------------------------------------------------------------------------

const char *const GAME_TYPES[3] = {"Resources/Game Types/MultiPlayer/Death Match.zgt",
                                   "Resources/Game Types/MultiPlayer/Capture The Flag.zgt",
                                   "Resources/Game Types/MultiPlayer/Salvage King.zgt"};
void *create_from_file(const char *path)
{
    ZString p;
    original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&p, path);
    void *o = at<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(&p, 0, 0, nullptr);
    original<void(THISCALL *)(ZString *)>(0x4defd0)(&p);
    return o;
}

// The real fonts a game type names are loaded first, so both sides find them loaded
void load_fonts(CMultiPlayerGameType *t)
{
    const DWORD *fonts = &t->mini_font;
    for (int i = 0; i < 6; i++) {
        ZDefinitionRecord *e = *at<ZDefinitionRecord **>(FONT_LIST + 0x30);
        DWORD k = fonts[i];
        if (!e)
            return;
        if (k >= *at<DWORD *>(FONT_LIST + 0x34))
            k = 0;
        if (!e[k].object && !e[k].loading)
            at<void *(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(FONT_LIST), k);
    }
}

}  // namespace

ZAX_CHECKS(mp_game_type)
{
    (void)cls;
    (void)n;
    Tally slots{"Multiplayer game type slots"}, objects{"Multiplayer game type objects"},
        classes{"Multiplayer game type classes"}, real{"Multiplayer game type real data"},
        no_state{"Multiplayer game type without a match state (the fault near address 0)"};
    if (!hook_sites_fit("mp_game_type", hooks_mp_game_type(nullptr), MAX_SITES))
        return;
    for (int i = 0; i < NPLAYER; i++) {
        char name[16];
        wsprintfA(name, "Player %d", i);
        original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g_names[i], i == 3 ? "" : name);
    }
    for (int side = 0; side < 2; side++)
        g_obj[side] = static_cast<CMultiPlayerGameType *>(at<Create0>(0x478d30)());   // CDeathMatchGameType
    // Fake cases
    if (g_obj[0] && g_obj[1]) {
        for (int fn = 0; fn < NFN; fn++) {
            const int cases = fn == F_DRAW || fn == F_LEADS ? 160 : 60;
            for (int c = 0; c < cases; c++)
                run_case(slots, (Fn)fn, false, 0, 0x10000 * (DWORD)(fn + 1) + (DWORD)c);
        }
        // Without a match state: the same read or write near address 0 on both sides
        const Fn users[] = {F_UPDATE, F_SCORE, F_TIME, F_SET_TIME, F_END, F_READY};
        g_no_state_faults = g_no_state_bad = 0;
        for (Fn fn : users)
            for (int c = 0; c < 20; c++) {
                g_no_state = 1 + c % 2;
                run_case(no_state, fn, false, 0, 0x800000 + 0x10000 * (DWORD)fn + (DWORD)c);
                g_no_state = 0;
            }
        char d[120];
        wsprintfA(d, "[faults %u, not an access violation below 0x18: %u]", g_no_state_faults, g_no_state_bad);
        no_state.check(g_no_state_faults > 0 && g_no_state_bad == 0, "faults", d);
        log("  without a match state: %s", d);
        for (int side = 0; side < 2; side++) {
            g_obj[side]->taken_lead = g_obj[side]->lost_lead = g_obj[side]->tied_lead = nullptr;
            vcall<void *>(g_obj[side], 1, 1);
        }
    }
    slots.report();
    no_state.report();
    test_objects(objects);
    objects.report();
    test_classes(classes, real);
    classes.report();
    // Real data: the game's three game types
    for (int k = 0; k < 3; k++) {
        for (int side = 0; side < 2; side++)
            g_real[side][k] = static_cast<CMultiPlayerGameType *>(create_from_file(GAME_TYPES[k]));
        char d[120];
        wsprintfA(d, "[%s: %x / %x]", GAME_TYPES[k], ptrv(g_real[0][k]), ptrv(g_real[1][k]));
        const bool loaded = g_real[0][k] && g_real[1][k];
        real.check(loaded, "load", d);
        if (!loaded)
            continue;
        load_fonts(g_real[0][k]);
        log("  %s: teams %u, team game %u, shared spawns %u, fonts %u %u %u, actions %x %x %x", GAME_TYPES[k], g_real[0][k]->max_teams,
            g_real[0][k]->is_team_game, g_real[0][k]->share_spawn_points, g_real[0][k]->mini_font, g_real[0][k]->mini_headings_font,
            g_real[0][k]->mini_blue_font, ptrv(g_real[0][k]->taken_lead), ptrv(g_real[0][k]->lost_lead), ptrv(g_real[0][k]->tied_lead));
        for (int fn = 0; fn < NFN; fn++)
            for (int c = 0; c < 6; c++)
                run_case(real, (Fn)fn, true, k, 0x7000000 + 0x10000 * (DWORD)k + 0x100 * (DWORD)fn + (DWORD)c);
        for (int side = 0; side < 2; side++)
            vcall<void *>(g_real[side][k], 1, 1);
    }
    real.report();
    for (int i = 0; i < NPLAYER; i++)
        original<void(THISCALL *)(ZString *)>(0x4defd0)(&g_names[i]);
}

}  // namespace zax
