// Checks of the net game's GameSpy answers, player and team helpers and the traffic graph (net_game.cpp)
// against the original, in the game thread (side 0 with the module's hooks out, side 1 through the
// reimplementation; checks_net_common.h). No network: the GameSpy SDK entries (0x5c82a0, 0x5c5280,
// 0x5c8f20, 0x5c8fb0, 0x5c5440, 0x5c85c0, 0x5c8a60, 0x5c5840) and the config read 0x46ec70 jump to
// stand-ins that record their arguments.
//  - Answers: a fake net game (game type whose slots 15-18 append a marker, settings, 0-5 real named
//    values as teams, a state text) and fake players (id, team, a name ZString); random sequences of the
//    four answers, the four "\key\value" helpers and the player answer (the world 0x6c2080 swapped for
//    a fake whose slot 1 object gives the ping). Compared: the ZString, eax of the team answer, the log.
//  - Players and teams: 0-9 players with ids and teams (0-24), 0-4 teams; PlayerAt, TeamAt (also past
//    the count), FindPlayerById (present and absent ids), FirstUnusedTeam, SmallestTeam (0-24 teams),
//    SetTeamCount (0-3, the teams deleted through slot 0, new ones from slot 21): eax (a team pointer as
//    its index in the team list), the teams' names and values, the deletions.
//  - Start/Stop/Think: the error reporter 0x6c02cc swapped for a fake (its byte +0x28), the config
//    giving "", "1" or an address, dedicated or not: the stand-ins' log (the bind address, game name and
//    key as text), the state text, the byte.
//  - Drawing: real graphs (constructor 0x51f8e0) with random samples, peak and height, the graph count
//    0x6d00e8 set to 1-13 (rows, labels), the colour guards cleared now and then (atexit recorded);
//    slot 51 of a fake surface, the frame 0x4b33f0 and the text 0x4a6470 jump to recorders (the font
//    "Small Blue" and its height are the game's). Compared: the log, the graph, the scale 0x61caf0, the
//    colours and guards (all saved and restored).
//  - Update: a fake net layer with 0-5 players (fake packets, real graphs), the graphs on or off
//    (0x6d00ec), the "all traffic" graph missing or there, the reporter's choice of which packet is
//    sent: the log, the packets' counters, every graph, the graph count.
#include "checks_net_common.h"
#include "net_game.h"
#include "exe_base.h"

namespace zax {

int hooks_net_game(Hook *out);

namespace {

using namespace netcheck;

// ---- Stand-ins ---------------------------------------------------------------------------------------------

DWORD __cdecl sdk_init(DWORD a, const char *ip, DWORD port, const char *name, const char *key, DWORD c1, DWORD c2,
                       DWORD c3, DWORD c4, DWORD self)
{
    ev(0x300, a, port, self);
    ev(0x301, ip ? fnv_text(2166136261u, ip) : 0, fnv_text(2166136261u, name), fnv_text(2166136261u, key));
    ev(0x302, c1, c2, c3);
    ev(0x303, c4);
    return 0x77;
}
template <int ID> DWORD __cdecl sdk1(DWORD a)
{
    ev(0x310 + ID, a);
    return 0x1000 + ID;
}
template <int ID> DWORD __cdecl sdk0()
{
    ev(0x320 + ID);
    return 0x2000 + ID;
}
const char *g_config_text;
void THISCALL fake_config(void *cfg, const char *section, const char *key, ZString *out)
{
    ev(0x330, ptrv(cfg), fnv_text(2166136261u, section), fnv_text(2166136261u, key));
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(out, g_config_text);
}

struct SdkPatches {
    CodePatch p[9];
    void on()
    {
        patch_jump(p[0], 0x5c82a0, (void *)&sdk_init);
        patch_jump(p[1], 0x5c5280, (void *)&sdk1<0>);
        patch_jump(p[2], 0x5c8f20, (void *)&sdk1<1>);
        patch_jump(p[3], 0x5c8fb0, (void *)&sdk1<2>);
        patch_jump(p[4], 0x5c85c0, (void *)&sdk1<3>);
        patch_jump(p[5], 0x5c8a60, (void *)&sdk1<4>);
        patch_jump(p[6], 0x5c5440, (void *)&sdk0<0>);
        patch_jump(p[7], 0x5c5840, (void *)&sdk0<1>);
        patch_jump(p[8], 0x46ec70, (void *)&fake_config);
    }
    void off()
    {
        for (CodePatch &c : p)
            unpatch(c);
    }
};

// The drawing stand-ins (thiscall like the originals)
void THISCALL fake_frame(void *surface, int x, int y, int x2, int y2, void *color)
{
    ev(0x340, ptrv(surface), (DWORD)x, (DWORD)y);
    ev(0x341, (DWORD)x2, (DWORD)y2, ptrv(color));
}
void THISCALL fake_text(void *font, void *surface, int x, int y, const char *text, DWORD a, DWORD b)
{
    ev(0x350, ptrv(font), ptrv(surface), (DWORD)x);
    ev(0x351, (DWORD)y, fnv_text(2166136261u, text), a);
    ev(0x352, b);
}
DWORD THISCALL fake_line(Fake *f, int x1, int y1, int x2, int y2, void *color)
{
    ev(0x360, f->id, (DWORD)x1, (DWORD)y1);
    ev(0x361, (DWORD)x2, (DWORD)y2, ptrv(color));
    return 0;
}
struct DrawPatches {
    CodePatch frame, text;
    void on()
    {
        patch_jump(frame, 0x4b33f0, (void *)&fake_frame);
        patch_jump(text, 0x4a6470, (void *)&fake_text);
    }
    void off()
    {
        unpatch(frame);
        unpatch(text);
    }
};

// ---- Fake net game ----------------------------------------------------------------------------------------

DWORD THISCALL gt_answer(Fake *f, ZString *out, int slot)
{
    ev(0x200 + (DWORD)slot, f->id);
    char m[8] = {'\\', 'g', 't', (char)('0' + slot - 10), 0};
    at<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(out, m);
    return 0x3000 + (DWORD)slot;
}
template <int S> DWORD THISCALL gt_slot(Fake *f, ZString *out) { return gt_answer(f, out, S); }
// Slot 21: a real named value
DWORD THISCALL gt_new_team(Fake *f)
{
    ev(0x215, f->id);
    void *p = heap_alloc(0xc);
    at<void *(THISCALL *)(void *)>(0x51f700)(p);
    return ptrv(p);
}

struct Game {
    NetGame *g;
    Fake *type;
    NetGameSettings *settings;
    ZString *names[10];
    DWORD nplayers;
};

void *new_named_value(const char *name, DWORD value)
{
    void *p = heap_alloc(0xc);
    at<void *(THISCALL *)(void *)>(0x51f700)(p);
    at<DWORD(THISCALL *)(void *, const char *)>(0x51f770)(p, name);
    ((DWORD *)p)[2] = value;
    return p;
}

const char *const NAMES[] = {"Zax", "", "A rather long player name of thirty", "Korbo", "x", "Name with spaces"};
const char *const TEAMS[] = {"Blue", "Red", "", "Green team", "T"};

Game make_game(DWORD nplayers, DWORD nteams, DWORD team_range)
{
    Game m;
    memset(&m, 0, sizeof m);
    m.g = (NetGame *)arena(0x60);
    m.type = new_fake(1);
    m.type->vt[15] = ptrv((void *)&gt_slot<15>);
    m.type->vt[16] = ptrv((void *)&gt_slot<16>);
    m.type->vt[17] = ptrv((void *)&gt_slot<17>);
    m.type->vt[18] = ptrv((void *)&gt_slot<18>);
    m.type->vt[21] = ptrv((void *)&gt_new_team);
    m.g->game_type = m.type;
    m.settings = (NetGameSettings *)arena(0x40);
    m.settings->max_players = rnd(17);
    m.settings->time_limit = random() % 3 ? rnd(3600) : random();
    m.settings->frag_limit = random() % 3 ? rnd(100) : (DWORD)-1;
    m.settings->dedicated = (BYTE)rnd(2);
    m.g->settings = m.settings;
    m.g->players = {0x5fb558, (void **)heap_alloc(16 * 4), nplayers, 16};
    for (DWORD i = 0; i < nplayers; i++) {
        BYTE *p = (BYTE *)arena(0x40);
        ((DWORD *)p)[3] = rnd(4) ? rnd(1000) : random();   // id
        ((DWORD *)p)[5] = rnd(team_range);                // team
        ZString *z = (ZString *)arena(4);
        at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, NAMES[rnd(6)]);
        m.names[i] = z;
        *(ZString **)(p + 0x1c) = z;
        m.g->players.data[i] = p;
    }
    m.nplayers = nplayers;
    m.g->teams = {0x5fb54c, (void **)heap_alloc(8 * 4), nteams, 8};
    for (DWORD i = 0; i < nteams; i++)
        m.g->teams.data[i] = new_named_value(TEAMS[rnd(5)], rnd(3) ? rnd(50) : random());
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(&m.g->state, rnd(2) ? "openplaying" : "");
    return m;
}

void st_teams(const NetGame *g)
{
    st(g->teams.count);
    for (DWORD i = 0; i < g->teams.count; i++) {
        const BYTE *t = (const BYTE *)g->teams.data[i];
        st_zstring((const ZString *)(t + 4));
        st(((const DWORD *)t)[2]);
        st(((const DWORD *)t)[0]);
    }
}

void free_game(Game &m)
{
    NetGame *g = m.g;
    for (DWORD i = 0; i < g->teams.count; i++)
        vcall<void *>(g->teams.data[i], 0, 1u);
    heap_free(g->teams.data);
    heap_free(g->players.data);
    for (DWORD i = 0; i < m.nplayers; i++)
        at<void(THISCALL *)(ZString *)>(0x4defd0)(m.names[i]);
    at<void(THISCALL *)(ZString *)>(0x4defd0)(&g->state);
}

// The world for the player answer: slot 1 gives an object whose slot 22 (player) is the ping
struct WorldSwap {
    DWORD saved;
    void on(Fake *w)
    {
        saved = *at<DWORD *>(0x6c2080);
        *at<DWORD *>(0x6c2080) = ptrv(w);
    }
    void off() { *at<DWORD *>(0x6c2080) = saved; }
};

const char *const CONFIG_TEXTS[] = {"", "1", "198.51.100.5", "10.0.0.1 "};
const char *const KEYS[] = {"hostname", "", "mapname", "a key with spaces", "numplayers"};

void answers_case(int k)
{
    Game m = make_game(rnd(6), rnd(6), 4);
    Fake *world = new_fake(2), *wo = new_fake(3);
    set0<1>(world, ptrv(wo));
    set1<22>(wo, k % 5 == 0 ? 0xffffffff : rnd(500));
    ZString out;
    at<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(&out, k % 4 ? "" : "\\gamename\\zax");
    WorldSwap ws;
    for (int n = 0, steps = 1 + (int)rnd(8); n < steps; n++) {
        switch (rnd(9)) {
        case 0: at<void(THISCALL *)(NetGame *, ZString *)>(0x51f180)(m.g, &out); break;
        case 1: at<void(THISCALL *)(NetGame *, ZString *)>(0x51f190)(m.g, &out); break;
        case 2: at<void(THISCALL *)(NetGame *, ZString *)>(0x51f1d0)(m.g, &out); break;
        case 3: ev(0x400, at<DWORD(THISCALL *)(NetGame *, ZString *)>(0x51f220)(m.g, &out)); break;
        case 4: at<void(__fastcall *)(ZString *, const char *, const char *)>(0x51f290)(&out, KEYS[rnd(5)], NAMES[rnd(6)]); break;
        case 5: at<void(__fastcall *)(ZString *, const char *, int)>(0x51f2b0)(&out, KEYS[rnd(5)], (int)(random() % 3 ? rnd(100) : random())); break;
        case 6: at<void(__fastcall *)(ZString *, DWORD, const char *, const char *)>(0x51f2d0)(&out, rnd(20), KEYS[rnd(5)], NAMES[rnd(6)]); break;
        case 7: at<void(__fastcall *)(ZString *, DWORD, const char *, int)>(0x51f2f0)(&out, rnd(20), KEYS[rnd(5)], (int)rnd(1000) - 10); break;
        default:
            if (m.nplayers) {
                ws.on(world);
                at<void(THISCALL *)(void *, ZString *, DWORD)>(0x51f880)(m.g->players.data[rnd(m.nplayers)], &out, rnd(16));
                ws.off();
            }
            break;
        }
    }
    st_zstring(&out);
    at<void(THISCALL *)(ZString *)>(0x4defd0)(&out);
    free_game(m);
}

// A team or a member of one (at offset off) as its index in the team list, any other value raw:
// teams are heap blocks that SetTeamCount frees and makes again, and the heap may give a new team a
// freed one's address on one side only, so numbering them by first sight (norm) would differ
DWORD team_rel(const NetGame *g, DWORD v, DWORD off = 0)
{
    for (DWORD i = 0; i < g->teams.count; i++)
        if (v == ptrv(g->teams.data[i]) + off)
            return 0xb0000000u + i;
    return v;
}

void helpers_case(int k)
{
    Game m = make_game(rnd(10), rnd(5), k % 3 == 0 ? 25 : 5);
    NetGame *g = m.g;
    for (int n = 0, steps = 1 + (int)rnd(10); n < steps; n++) {
        switch (rnd(6)) {
        case 0:
            if (g->players.count)
                ev(0x410, at<DWORD(THISCALL *)(NetGame *, DWORD)>(0x51f440)(g, rnd(g->players.count)));
            break;
        case 1: ev_raw(0x411, team_rel(g, at<DWORD(THISCALL *)(NetGame *, DWORD)>(0x51f460)(g, rnd(g->teams.count + 3)))); break;
        case 2: {
            DWORD id = rnd(1000);
            if (g->players.count && rnd(2))
                id = ((DWORD *)g->players.data[rnd(g->players.count)])[3];
            ev(0x412, at<DWORD(THISCALL *)(NetGame *, DWORD)>(0x51f530)(g, id));
            break;
        }
        case 3: ev(0x413, at<DWORD(THISCALL *)(NetGame *)>(0x51f560)(g)); break;
        case 4: ev(0x414, at<DWORD(THISCALL *)(NetGame *, DWORD)>(0x51f5a0)(g, rnd(4) ? rnd(6) : rnd(25))); break;
        default: {
            const DWORD r = at<DWORD(THISCALL *)(NetGame *, DWORD)>(0x51f480)(g, rnd(4));
            // eax: the name string inside a new team (heap) or 0
            ev_raw(0x415, team_rel(g, r, 4));
            st_teams(g);
            break;
        }
        }
    }
    st_teams(g);
    free_game(m);
}

void gamespy_case(int k)
{
    Game m = make_game(rnd(3), 0, 2);
    Fake *reporter = new_fake(4);
    const DWORD saved = *at<DWORD *>(G_ERROR_REPORTER);
    *at<DWORD *>(G_ERROR_REPORTER) = ptrv(reporter);
    reporter->ret[8] = random();
    g_config_text = CONFIG_TEXTS[k % 4];
    SdkPatches sdk;
    sdk.on();
    for (int n = 0, steps = 1 + (int)rnd(4); n < steps; n++) {
        m.settings->dedicated = (BYTE)rnd(2);
        switch (rnd(3)) {
        case 0: at<void(THISCALL *)(NetGame *)>(0x51f310)(m.g); break;
        case 1: ev(0x420, at<DWORD(THISCALL *)(NetGame *)>(0x51f3e0)(m.g)); break;
        default: ev(0x421, at<DWORD(THISCALL *)(NetGame *)>(0x51f420)(m.g)); break;
        }
        st_zstring(&m.g->state);
        st(reporter->ret[8]);
    }
    sdk.off();
    *at<DWORD *>(G_ERROR_REPORTER) = saved;
    free_game(m);
}

// ---- Traffic graph ------------------------------------------------------------------------------------------

struct GraphGlobals {
    SavedBytes colors;   // 0x6d00b0-0x6d00ef: colours, guard, graph count, graphs-on flag
    DWORD scale;
    void save()
    {
        save_bytes(colors, 0x6d00b0, 0x40);
        scale = *at<DWORD *>(G_NET_GRAPH_SCALE);
    }
    void restore()
    {
        restore_bytes(colors);
        *at<DWORD *>(G_NET_GRAPH_SCALE) = scale;
    }
};

NetTrafficGraph *new_graph()
{
    auto *g = (NetTrafficGraph *)heap_alloc(0x1c);
    at<void(THISCALL *)(void *)>(0x51f8e0)(g);
    for (DWORD i = 0; i < g->samples; i++) {
        g->sent[i] = (int)(rnd(4) ? rnd(300) : rnd(5000));
        g->received[i] = (int)(rnd(4) ? rnd(300) : rnd(5000));
    }
    g->unknown10 = rnd(3) ? rnd(2000) : 1;
    if (rnd(4) == 0)
        g->height = 1 + rnd(120);
    g->x = (int)random();
    g->y = (int)random();
    return g;
}
void free_graph(NetTrafficGraph *g)
{
    at<void(THISCALL *)(void *)>(0x51f960)(g);
    heap_free(g);
}
void st_graph(const NetTrafficGraph *g)
{
    if (!g) {
        st_raw(0xdead0002);
        return;
    }
    st_raw((DWORD)g->x);
    st_raw((DWORD)g->y);
    st_raw(g->samples);
    st_raw(g->height);
    st_raw(g->unknown10);
    st_bytes(g->sent, g->samples * 4);
    st_bytes(g->received, g->samples * 4);
}
void st_graph_globals()
{
    st_bytes(at<void *>(0x6d00b0), 0x3c);
    st_raw(*at<DWORD *>(G_NET_GRAPH_SCALE));
}

void prepare_graph_globals(int k)
{
    *at<DWORD *>(NET_GRAPH_COUNT) = 1 + rnd(13);
    if (k % 7 == 0)
        *at<BYTE *>(G_NET_GRAPH_GUARD) &= (BYTE)~rnd(8);
    *at<DWORD *>(G_NET_GRAPH_SCALE) = rnd(3) ? 1 + rnd(3000) : 1;
}

void draw_case(int k)
{
    GraphGlobals saved;
    saved.save();
    CodePatch ae;
    patch_jump(ae, 0x5d0863, (void *)&fake_atexit);
    g_atexit_calls = 0;
    DrawPatches dp;
    dp.on();
    prepare_graph_globals(k);
    Fake *surface = new_fake(5);
    surface->vt[51] = ptrv((void *)&fake_line);
    NetTrafficGraph *g = new_graph();
    for (int n = 0, steps = 1 + (int)rnd(3); n < steps; n++) {
        const DWORD index = rnd(4) ? rnd(*at<DWORD *>(NET_GRAPH_COUNT) + 1) : *at<DWORD *>(NET_GRAPH_COUNT) - 1;
        at<void(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x51f9a0)(g, surface, rnd(4) ? rnd(400) : rnd(8000),
                                                                         rnd(4) ? rnd(400) : rnd(8000), index);
        st_graph(g);
        st_graph_globals();
    }
    st_raw(g_atexit_calls);
    free_graph(g);
    dp.off();
    unpatch(ae);
    saved.restore();
}

// The net layer: slot 11 count, 12/13 the player's packets, 14 the player's graph
struct Layer {
    Fake *f;
    NetPacket *p12, *p13;
    NetTrafficGraph *graphs[8];
    DWORD n;
};
Layer g_layer;
DWORD THISCALL layer_count(Fake *f)
{
    ev(0x370, f->id);
    return g_layer.n;
}
DWORD THISCALL layer_p12(Fake *f, DWORD i)
{
    ev(0x371, f->id, i);
    return ptrv(&g_layer.p12[i]);
}
DWORD THISCALL layer_p13(Fake *f, DWORD i)
{
    ev(0x372, f->id, i);
    return ptrv(&g_layer.p13[i]);
}
DWORD THISCALL layer_graph(Fake *f, DWORD i)
{
    ev(0x373, f->id, i);
    return ptrv(g_layer.graphs[i]);
}

void update_case(int k)
{
    GraphGlobals saved;
    saved.save();
    DrawPatches dp;
    dp.on();
    prepare_graph_globals(k);
    *at<DWORD *>(NET_GRAPH_COUNT) = 0;
    *at<BYTE *>(G_NET_GRAPH_GUARD) = 7;
    Fake *reporter = new_fake(6), *ro = new_fake(7);
    set0<1>(reporter, ptrv(ro));
    set0<12>(ro, rnd(2) ? 0x12345601 : 0x12345600);
    const DWORD saved_reporter = *at<DWORD *>(G_ERROR_REPORTER);
    *at<DWORD *>(G_ERROR_REPORTER) = ptrv(reporter);
    *at<BYTE *>(G_NET_GRAPHS_ON) = (BYTE)(k % 5 != 0);
    Layer &l = g_layer;
    l.n = rnd(6);
    l.f = new_fake(8);
    l.f->vt[11] = ptrv((void *)&layer_count);
    l.f->vt[12] = ptrv((void *)&layer_p12);
    l.f->vt[13] = ptrv((void *)&layer_p13);
    l.f->vt[14] = ptrv((void *)&layer_graph);
    l.p12 = (NetPacket *)arena(sizeof(NetPacket) * 8);
    l.p13 = (NetPacket *)arena(sizeof(NetPacket) * 8);
    for (DWORD i = 0; i < l.n; i++) {
        l.p12[i].length = rnd(2000);
        l.p12[i].consumed = rnd(2000);
        l.p13[i].length = rnd(2000);
        l.p13[i].consumed = rnd(2000);
        l.graphs[i] = new_graph();
    }
    NetTrafficGraph **all = (NetTrafficGraph **)arena(4);
    if (rnd(2))
        *all = new_graph();
    Fake *surface = new_fake(9);
    surface->vt[51] = ptrv((void *)&fake_line);
    const DWORD r = at<DWORD(__fastcall *)(void *, void *, NetTrafficGraph **)>(0x520150)(surface, l.f, all);
    if (!*at<BYTE *>(G_NET_GRAPHS_ON) || !*all)
        ev(0x430, r);
    for (DWORD i = 0; i < l.n; i++) {
        st_raw(l.p12[i].consumed);
        st_raw(l.p13[i].consumed);
        st_graph(l.graphs[i]);
        free_graph(l.graphs[i]);
    }
    st_graph(*all);
    if (*all)
        free_graph(*all);
    st_raw(*at<DWORD *>(NET_GRAPH_COUNT));
    st_graph_globals();
    *at<DWORD *>(G_ERROR_REPORTER) = saved_reporter;
    dp.off();
    saved.restore();
}

}  // namespace

ZAX_CHECKS(net_game)
{
    (void)cls;
    (void)n;
    g_table = hooks_net_game;
    run_cases("net game: GameSpy answers", 300, 0x51f180u, [](int k) { answers_case(k); });
    run_cases("net game: players and teams", 300, 0x51f440u, [](int k) { helpers_case(k); });
    run_cases("net game: GameSpy start, stop, think", 60, 0x51f310u, [](int k) { gamespy_case(k); });
    run_cases("net game: traffic graph drawing", 120, 0x51f9a0u, [](int k) { draw_case(k); });
    run_cases("net game: traffic graph update", 120, 0x520150u, [](int k) { update_case(k); });
}

}  // namespace zax
