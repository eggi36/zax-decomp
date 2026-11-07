// The game-side code of the net game around GameSpy and the net traffic graph's drawing and update
// (layouts and what stays with the original: net_game.h).
#include "net_game.h"
#include "exe_base.h"

namespace zax {

namespace {

// memmove of the exe (0x5d04b0), returns the target
void *crt_memmove(void *d, const void *s, DWORD n) { return zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

constexpr DWORD EMPTY = 0x62e558;   // ""

template <typename... A> void append_format(ZString *out, const char *fmt, A... a)
{
    zax::fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e0a30)(out, fmt, a...);
}
template <typename... A> void format(ZString *out, const char *fmt, A... a)
{
    zax::fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(out, fmt, a...);
}
const char *cstr(const ZString *s) { return zax::fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(s); }
DWORD zs_length(const ZString *s) { return zax::fn<DWORD(THISCALL *)(const ZString *)>(0x4e13b0)(s); }
void zs_assign(ZString *s, const char *t) { zax::fn<void(THISCALL *)(ZString *, const char *)>(0x4e1930)(s, t); }
void zs_ctor_text(ZString *s, const char *t) { zax::fn<void(THISCALL *)(ZString *, const char *)>(0x4dee00)(s, t); }
void zs_ctor(ZString *s) { zax::fn<void(THISCALL *)(ZString *)>(0x4dec90)(s); }
void zs_dtor(ZString *s) { zax::fn<void(THISCALL *)(ZString *)>(0x4defd0)(s); }

// 0x4cde90: the error reporter's byte +0x28 (set while GameSpy runs)
void reporter_set_28(BYTE v) { zax::fn<void(THISCALL *)(void *, DWORD)>(0x4cde90)(*glob<void **>(G_ERROR_REPORTER), v); }

// The GameSpy qr SDK (cdecl)
DWORD sdk_call(DWORD fn, DWORD a) { return fn_dyn<DWORD(__cdecl *)(DWORD)>(fn)(a); }

const char *text(GlobKey a) { return glob<const char *>(a); }

}  // namespace

// ---- GameSpy answers ----------------------------------------------------------------------------------

void THISCALL NetGame::GameSpyBasicInfo(ZString *out) { vcall<void>(game_type, 15, out); }

void THISCALL NetGame::GameSpyServerInfo(ZString *out)
{
    vcall<void>(game_type, 16, out);
    append_format(out, text(0x61ca40) /* "\%s\%s" */, text(0x61ca48) /* "gamemode" */, cstr(&state));
}

void THISCALL NetGame::GameSpyRules(ZString *out)
{
    vcall<void>(game_type, 17, out);
    append_format(out, text(0x61ca60) /* "\%s\%d" */, text(0x61ca68) /* "timelimit" */, settings->time_limit);
    append_format(out, text(0x61ca60), text(0x61ca54) /* "fraglimit" */, settings->frag_limit);
}

// Per team "\team_t<i>\<name>" and "\score_t<i>\<score>"; the count is read again for every team
DWORD THISCALL NetGame::GameSpyTeams(ZString *out)
{
    vcall<void>(game_type, 18, out);
    DWORD i = 0;
    for (; i < teams.count; i++) {
        auto *t = (NamedValue *)teams.data[i];
        append_format(out, text(0x61ca88) /* "\%s_t%d\%s" */, text(0x61ca94) /* "team" */, i, cstr(&t->name));
        t = (NamedValue *)teams.data[i];
        append_format(out, text(0x61ca74) /* "\%s_t%d\%d" */, text(0x61ca80) /* "score" */, i, t->value);
    }
    return teams.count;
}

void __fastcall GameSpy_AppendKeyText(ZString *out, const char *key, const char *value)
{
    append_format(out, text(0x61ca40) /* "\%s\%s" */, key, value);
}

void __fastcall GameSpy_AppendKeyInt(ZString *out, const char *key, int value)
{
    append_format(out, text(0x61ca60) /* "\%s\%d" */, key, value);
}

void __fastcall GameSpy_AppendPlayerText(ZString *out, DWORD index, const char *key, const char *value)
{
    append_format(out, text(0x61ca9c) /* "\%s_%d\%s" */, key, index, value);
}

void __fastcall GameSpy_AppendPlayerInt(ZString *out, DWORD index, const char *key, int value)
{
    append_format(out, text(0x61caa8) /* "\%s_%d\%d" */, key, index, value);
}

// "\name_<i>\<name>" and "\ping_<i>\<ping>" (the ping from slot 22 of the world's slot 1 object)
void THISCALL NetPlayer::GameSpyPlayerInfo(ZString *out, DWORD index)
{
    zax::fn<void(__fastcall *)(ZString *, DWORD, const char *, const char *)>(0x51f2d0)(
        out, index, text(0x61cae8) /* "name" */, cstr((const ZString *)base.settings));
    void *world = vcall<void *>(*glob<void **>(0x6c2080), 1);
    const int ping = vcall<int>(world, 22, this);
    zax::fn<void(__fastcall *)(ZString *, DWORD, const char *, int)>(0x51f2f0)(out, index, text(0x61cae0) /* "ping" */, ping);
}

// "openplaying", the reporter's flag, the bind address from the config ("Net Game", "Bind Host to IP
// Address", only used when longer than one character) and qr_init with the query port 0x68aa, the
// game name "zax", the secret key "J3An3s" (built on the stack byte by byte) and the four answer
// callbacks 0x51ef40-0x51f0f0 (through fn_addr: in the standalone exe their own code, batch 6-16a; the plain numbers
// pointed into the inaccessible image range, so the first GameSpy query a host answered ended the exe), then
// 0x5c5280(0x199)
void THISCALL NetGame::GameSpyStart()
{
    zs_assign(&state, text(0x61cacc) /* "openplaying" */);
    reporter_set_28(1);
    char key[8];
    key[0] = 0x4a;
    key[1] = 0x33;
    key[3] = 0x6e;
    key[2] = 0x41;
    key[4] = 0x33;
    key[5] = 0x73;
    key[6] = 0;
    ZString ip;
    zs_ctor_text(&ip, text(EMPTY));
    zax::fn<void(THISCALL *)(void *, const char *, const char *, ZString *)>(0x46ec70)(
        glob<void *>(0x6bd330), text(0x60fa48) /* "Net Game" */, text(0x61cab4) /* "Bind Host to IP Address" */, &ip);
    const char *bind = zs_length(&ip) > 1 ? cstr(&ip) : nullptr;
    zax::fn<DWORD(__cdecl *)(DWORD, const char *, DWORD, const char *, const char *, DWORD, DWORD, DWORD, DWORD, void *)>(0x5c82a0)(
        0, bind, 0x68aa, text(0x615d38) /* "zax" */, key, fn_addr(0x51ef40), fn_addr(0x51efd0), fn_addr(0x51f060), fn_addr(0x51f0f0), this);
    sdk_call(fn_addr(0x5c5280), 0x199);
    zs_dtor(&ip);
}

DWORD THISCALL NetGame::GameSpyStop()
{
    reporter_set_28(0);
    zs_assign(&state, text(0x61cad8) /* "exiting" */);
    sdk_call(fn_addr(0x5c8f20), 0);
    sdk_call(fn_addr(0x5c8fb0), 0);
    return zax::fn<DWORD(__cdecl *)()>(0x5c5440)();
}

DWORD THISCALL NetGame::GameSpyThink()
{
    sdk_call(settings->dedicated ? fn_addr(0x5c85c0) : fn_addr(0x5c8a60), 0);
    return zax::fn<DWORD(__cdecl *)()>(0x5c5840)();
}

// ---- Players and teams ---------------------------------------------------------------------------------

NetPlayer *THISCALL NetGame::PlayerAt(DWORD i) { return (NetPlayer *)players.data[i]; }

NamedValue *THISCALL NetGame::TeamAt(DWORD i) { return i < teams.count ? (NamedValue *)teams.data[i] : nullptr; }

// The teams deleted from the last (slot 0 with flags 1) and removed, n new ones from slot 21 of the
// game type appended, the first named "Blue", the second "Red". eax: 0 for n = 0, otherwise the last
// SetName's.
DWORD THISCALL NetGame::SetTeamCount(DWORD n)
{
    DWORD r = teams.count;
    while (teams.count) {
        const DWORD i = teams.count - 1;
        if (void *t = teams.data[i])
            vcall<void *>(t, 0, 1);
        const DWORD c = teams.count;
        if (i < c) {
            if (i + 1 < c)
                crt_memmove(&teams.data[i], &teams.data[i + 1], (c - (i + 1)) * 4);
            teams.count--;
        }
        r = teams.count;
    }
    for (DWORD k = n; k; k--) {
        void *t = vcall<void *>(game_type, 21);
        vcall<DWORD>(&teams, 2, &t);
    }
    auto set_name = code<DWORD(THISCALL *)(void *, const char *)>(0x51f770);
    if (n >= 1)
        r = set_name(teams.data[0], text(0x60f2d8) /* "Blue" */);
    if (n >= 2)
        r = set_name(teams.data[1], text(0x60f2e0) /* "Red" */);
    return r;
}

// The index of the player whose +0x0c is id, otherwise -1
DWORD THISCALL NetGame::FindPlayerById(DWORD id)
{
    for (DWORD i = 0; i < players.count; i++)
        if (((DWORD *)players.data[i])[3] == id)
            return i;
    return 0xffffffff;
}

// The first team index (below 20) that no player has at +0x14; 0 when all are used or there are no
// players
DWORD THISCALL NetGame::FirstUnusedTeam()
{
    const DWORD n = players.count;
    if (!n)
        return 0;
    for (DWORD t = 0;;) {
        bool used = false;
        for (DWORD i = 0; i < n; i++)
            if (((DWORD *)players.data[i])[5] == t)
                used = true;
        if (!used)
            return t;
        if (++t >= 20)
            return 0;
    }
}

// Of the first n teams (at most 20) the one with the fewest players (the first of equal ones); 0 when
// n is 0
DWORD THISCALL NetGame::SmallestTeam(DWORD n)
{
    if (n > 20)
        n = 20;
    DWORD count[20];
    for (DWORD i = 0; i < n; i++)
        count[i] = 0;
    for (DWORD i = 0; i < players.count; i++) {
        const DWORD t = ((DWORD *)players.data[i])[5];
        if (t < n)
            count[t]++;
    }
    DWORD best = 0, fewest = 0xffffffff;
    for (DWORD i = 0; i < n; i++)
        if (count[i] < fewest) {
            fewest = count[i];
            best = i;
        }
    return best;
}

// ---- Traffic graph ---------------------------------------------------------------------------------------

void *resource_list_get(DWORD list, DWORD index)
{
    BYTE *data = *at<BYTE **>(list + 0x30);
    if (!data)
        return nullptr;
    const DWORD k = index < *at<DWORD *>(list + 0x34) ? index : 0;
    BYTE *rec = data + k * 0x80;
    if (rec[0x7f])
        return nullptr;
    if (!*(void **)(rec + 0x78) && !rec[0x7f]) {
        rec[0x7f] = 1;
        char path[0x104];
        zax::fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, at<const char *>(list + 0x38),
                                                                               vcall<const char *>(at<void *>(list), 1, k));
        char *p = (char *)zax::fn<const char *(__fastcall *)(const char *)>(0x4a4570)(path);
        void *o;
        if (p == path) {
            o = vcall<void *>(at<void *>(list), 37, text(EMPTY), p, (void **)(*at<BYTE **>(list + 0x30) + k * 0x80 + 0x78));
        } else {
            p[-1] = 0;
            o = vcall<void *>(at<void *>(list), 37, path, p, (void **)(*at<BYTE **>(list + 0x30) + k * 0x80 + 0x78));
        }
        *(void **)(*at<BYTE **>(list + 0x30) + k * 0x80 + 0x78) = o;
        (*at<BYTE **>(list + 0x30))[k * 0x80 + 0x7f] = 0;
        data = *at<BYTE **>(list + 0x30);
    }
    return *(void **)(data + k * 0x80 + 0x78);
}

// Six graphs per row, 90 pixels apart from the right (x = (6 - index % 6) * 90), rows 75 pixels apart
// counted from the last graph (y = (index / 6 - graphs / 6) * 75 + 390). The samples move one to the
// left (the copy reads 3 bytes behind the arrays, which the new sample overwrites), the highest sample
// so far (0x61caf0) scales both curves, +0x10 keeps the highest total. Lines through slot 51 of the
// surface, the frame through 0x4b33f0, the texts "Max %d, Cur %d" below and "Player %u" (or, for the
// last of more than two graphs, "All Non-Local Traffic") above in the font "Small Blue".
void THISCALL NetTrafficGraphDraw::Draw(void *surface, DWORD sent, DWORD received, DWORD index)
{
    NetTrafficGraph &g = graph;
    const DWORD graphs = *glob<DWORD *>(NET_GRAPH_COUNT);
    g.y = (int)((index / 6 - graphs / 6) * 75 + 0x186);
    g.x = (int)((6 - index % 6) * 90);
    BYTE *guard = glob<BYTE *>(G_NET_GRAPH_GUARD);
    auto color = code<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010);
    if (!(*guard & 1)) {
        *guard |= 1;
        color(glob<void *>(0x6d00b0), 0xc8, 0xc8, 0, 0xff);
        crt_atexit(code<void(__cdecl *)()>(0x51fe50));
    }
    if (!(*guard & 2)) {
        *guard |= 2;
        color(glob<void *>(0x6d00c8), 0, 0xc8, 0x14, 0xff);
        crt_atexit(code<void(__cdecl *)()>(0x51fe40));
    }
    if (!(*guard & 4)) {
        *guard |= 4;
        color(glob<void *>(0x6d00d8), 0xa, 0x19, 0xf0, 0xff);
        crt_atexit(code<void(__cdecl *)()>(0x51fe30));
    }
    DWORD *scale = glob<DWORD *>(G_NET_GRAPH_SCALE);
    const DWORD high = sent > received ? sent : received;
    if (!(*scale > high))
        *scale = high;
    const DWORD total = sent + received;
    if (!(g.unknown10 > total))
        g.unknown10 = total;
    // The shift as the original does it: (samples * 4 - 1) bytes from +4, dwords first
    auto shift = [](int *a, DWORD samples) {
        const DWORD n = samples * 4 - 1;
        DWORD *d = (DWORD *)a;
        const DWORD *s = (const DWORD *)a + 1;
        for (DWORD i = 0; i < n >> 2; i++)
            d[i] = s[i];
        BYTE *db = (BYTE *)(d + (n >> 2));
        const BYTE *sb = (const BYTE *)(s + (n >> 2));
        for (DWORD i = 0; i < (n & 3); i++)
            db[i] = sb[i];
    };
    shift(g.sent, g.samples);
    g.sent[g.samples - 1] = (int)sent;
    shift(g.received, g.samples);
    g.received[g.samples - 1] = (int)received;
    // y - sample * height / scale + height, 32-bit unsigned like the original (imul, div)
    auto line = [&](const int *a, DWORD i, DWORD color) {
        const DWORD h = g.height, s = *scale;
        const DWORD q1 = (DWORD)a[i + 1] * h / s, q0 = (DWORD)a[i] * h / s;
        vcall<void>(surface, 51, (int)((DWORD)g.x + i), (int)((DWORD)g.y - q0 + h), (int)((DWORD)g.x + i + 1),
                    (int)((DWORD)g.y - q1 + h), at<void *>(color));
    };
    for (DWORD i = 0; i + 1 < g.samples; i++) {
        line(g.sent, i, glob_addr(0x6d00b0));
        line(g.received, i, glob_addr(0x6d00c8));
    }
    zax::fn<void(THISCALL *)(void *, int, int, int, int, void *)>(0x4b33f0)(
        surface, g.x, g.y - 1, g.x + (int)g.samples, g.y + (int)g.height + 2, glob<void *>(0x6d00d8));
    const DWORD font_index = vcall<DWORD>(glob<void *>(G_FONTS), 3, text(0x60c53c) /* "Small Blue" */, -1);
    void *font = resource_list_get(glob_addr(G_FONTS), font_index);
    auto font_height = code<int(THISCALL *)(void *)>(0x4a6be0);
    auto draw_text = code<void(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470);
    ZString s;
    zs_ctor(&s);
    format(&s, text(0x61cb18) /* "Max %d, Cur %d" */, g.unknown10, total);
    const char *t = cstr(&s);
    draw_text(font, surface, g.x, font_height(font) + (int)g.height + g.y, t, 0, 0);
    const DWORD n = *glob<DWORD *>(NET_GRAPH_COUNT);
    if (n > 2 && index == n - 1) {
        format(&s, text(0x61cb00) /* "All Non-Local Traffic" */);
    } else {
        format(&s, text(0x61caf4) /* "Player %u" */, index);
    }
    t = cstr(&s);
    draw_text(font, surface, g.x, g.y - font_height(font), t, 0, 0);
    zs_dtor(&s);
}

// With the graphs off (byte 0x6d00ec) only the counters +0x24 of both packets of every player are
// cleared. Otherwise the "all traffic" graph exists only while there are more than two players
// (created inline like 0x51f8e0, deleted through 0x51f960); every player's graph gets the bytes of
// both packets (+0x20 + +0x24; which one counts as sent depends on slot 12 of the error reporter's
// slot 1 object), then the counters are cleared, and the "all traffic" graph gets the sums.
// Net layer slots: 11 player count, 12/13 the player's two packets, 14 the player's graph.
// eax: the player count, or the last Draw's (undefined).
DWORD __fastcall NetTrafficGraph_Update(void *surface, void *net, NetTrafficGraph **all)
{
    auto count = [net] { return vcall<DWORD>(net, 11); };
    auto packet = [net](int slot, DWORD i) { return vcall<NetPacket *>(net, slot, i); };
    if (!*glob<BYTE *>(G_NET_GRAPHS_ON)) {
        DWORD n = count();
        for (DWORD i = 0; i < n;) {
            packet(13, i)->consumed = 0;
            packet(12, i)->consumed = 0;
            i++;
            n = count();
        }
        return n;
    }
    if (count() <= 2) {
        if (NetTrafficGraph *g = *all) {
            zax::fn<void(THISCALL *)(NetTrafficGraph *)>(0x51f960)(g);
            heap_free(g);
            *all = nullptr;
        }
    } else if (!*all) {
        auto *g = (NetTrafficGraph *)heap_alloc(0x1c);
        if (g) {
            ++*glob<DWORD *>(NET_GRAPH_COUNT);
            g->samples = 0x50;
            g->height = 0x32;
            g->unknown10 = 1;
            g->sent = (int *)heap_alloc(0x140);
            memset(g->sent, 0, g->samples * 4);
            g->received = (int *)heap_alloc(g->samples * 4);
            memset(g->received, 0, g->samples * 4);
        }
        *all = g;
    }
    auto swapped = [] { return (vcall<DWORD>(vcall<void *>(*glob<void **>(G_ERROR_REPORTER), 1), 12) & 0xff) != 0; };
    DWORD sent_all = 0, received_all = 0, n = count();
    for (DWORD i = 0; i < n;) {
        NetPacket *a, *b;
        if (swapped()) {
            a = packet(12, i);
            b = packet(13, i);
        } else {
            a = packet(13, i);
            b = packet(12, i);
        }
        const DWORD ta = a->consumed + a->length, tb = b->consumed + b->length;
        auto *g = vcall<NetTrafficGraphDraw *>(net, 14, i);
        g->Draw(surface, tb, ta, i);
        NetPacket *p13 = packet(13, i);
        sent_all += p13->consumed + p13->length;
        NetPacket *p12 = packet(12, i);
        received_all += p12->consumed + p12->length;
        packet(13, i)->consumed = 0;
        packet(12, i)->consumed = 0;
        i++;
        n = count();
    }
    if (!*all)
        return n;
    auto *g = (NetTrafficGraphDraw *)*all;
    if (swapped())
        g->Draw(surface, sent_all, received_all, count());
    else
        g->Draw(surface, received_all, sent_all, count());
    return 0;
}

ZAX_HOOKS(net_game)
{
    const Hook list[] = {
        {0x51f180, ptr(&NetGame::GameSpyBasicInfo), "NetGame::GameSpyBasicInfo", 7, {0x8b, 0x49, 0x04, 0x8b, 0x54, 0x24, 0x04, 0x52}},
        {0x51f190, ptr(&NetGame::GameSpyServerInfo), "NetGame::GameSpyServerInfo", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x51f1d0, ptr(&NetGame::GameSpyRules), "NetGame::GameSpyRules", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x51f220, ptr(&NetGame::GameSpyTeams), "NetGame::GameSpyTeams", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x8b}},
        {0x51f290, (void *)&GameSpy_AppendKeyText, "GameSpy_AppendKeyText", 5, {0x8b, 0x44, 0x24, 0x04, 0x50, 0x52, 0x68, 0x40}},
        {0x51f2b0, (void *)&GameSpy_AppendKeyInt, "GameSpy_AppendKeyInt", 5, {0x8b, 0x44, 0x24, 0x04, 0x50, 0x52, 0x68, 0x60}},
        {0x51f2d0, (void *)&GameSpy_AppendPlayerText, "GameSpy_AppendPlayerText", 5, {0x8b, 0x44, 0x24, 0x08, 0x50, 0x52, 0x8b, 0x54}},
        {0x51f2f0, (void *)&GameSpy_AppendPlayerInt, "GameSpy_AppendPlayerInt", 5, {0x8b, 0x44, 0x24, 0x08, 0x50, 0x52, 0x8b, 0x54}},
        {0x51f310, ptr(&NetGame::GameSpyStart), "NetGame::GameSpyStart", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x68, 0xcc}},
        {0x51f3e0, ptr(&NetGame::GameSpyStop), "NetGame::GameSpyStop", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x0d, 0xcc, 0x02, 0x6c}},
        {0x51f420, ptr(&NetGame::GameSpyThink), "NetGame::GameSpyThink", 5, {0x8b, 0x41, 0x08, 0x6a, 0x00, 0x8a, 0x48, 0x18}},
        {0x51f440, ptr(&NetGame::PlayerAt), "NetGame::PlayerAt", 7, {0x8b, 0x41, 0x14, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x51f460, ptr(&NetGame::TeamAt), "NetGame::TeamAt", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x28, 0x3b}},
        {0x51f480, ptr(&NetGame::SetTeamCount), "NetGame::SetTeamCount", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0x8b, 0x45}},
        {0x51f530, ptr(&NetGame::FindPlayerById), "NetGame::FindPlayerById", 5, {0x8b, 0x51, 0x18, 0x33, 0xc0, 0x56, 0x85, 0xd2}},
        {0x51f560, ptr(&NetGame::FirstUnusedTeam), "NetGame::FirstUnusedTeam", 7, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x79, 0x18, 0x33}},
        {0x51f5a0, ptr(&NetGame::SmallestTeam), "NetGame::SmallestTeam", 5, {0x83, 0xec, 0x50, 0x8b, 0xd1, 0x56, 0x8b, 0x74}},
        {0x51f880, ptr(&NetPlayer::GameSpyPlayerInfo), "NetPlayer::GameSpyPlayerInfo", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4e, 0x1c}},
        {0x51f9a0, ptr(&NetTrafficGraphDraw::Draw), "NetTrafficGraph::Draw", 5, {0xb8, 0xab, 0xaa, 0xaa, 0xaa, 0x81, 0xec, 0x14}},
        {0x520150, (void *)&NetTrafficGraph_Update, "NetTrafficGraph_Update", 8, {0x83, 0xec, 0x08, 0xa0, 0xec, 0x00, 0x6d, 0x00}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
