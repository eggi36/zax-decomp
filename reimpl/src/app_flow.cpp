// Game flow helpers of 0x590000-0x59ffff (phase 3): the build number and version texts, the
// application's (CZaxWorld's) current game type, the "Weapon AutoSwitch" option, the pending map switch,
// six of its vtable slots (the frame start and the multiplayer frame wrappers, two menus, the players'
// characters of a game) and the net game's text and chat sends.
//
// The application (g_App 0x713f14, a CZaxWorld): +0x570 the position of a pending map switch (two
// dwords), +0x578 its map name, +0x57c its start point (ZStrings), +0x580 a byte, +0x584 the "Return to
// map" directive, +0x588 the switch is pending (byte), +0x58c the index of the game type in the game type
// list (0x6c02e8, ".ZGT"; -1 none).
//
// Return values are what the original leaves in eax; where that is only al, the upper bytes are those of
// the call before.
#include "class_info.h"
#include "ai_common.h"
#include "def_list_slots.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD BUILD = 0x51c, MIN_BUILD = 0x51a;    // 1308, 1306
constexpr DWORD TEXT_BUILD = 0x713bac, TEXT_DEMO = 0x713bb0, TEXT_VERSION = 0x713bb4;   // registered strings
constexpr DWORD GAME_TYPES = 0x6c02e8;                // the game type list (".ZGT")
constexpr DWORD TEXT_EMPTY = 0x62e558;

template <typename... A> DWORD zs_format(void *z, DWORD fmt, A... a) { return fn<DWORD(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...); }
DWORD zs_destroy(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }

// 0x5908a0: the build number (1308), sent in the net game's packets and the GameSpy key "gamebuild"
DWORD __cdecl Game_BuildNumber() { return BUILD; }
// 0x5908b0: the oldest build a server may have (1306; compared with the server's "gamebuild")
DWORD __cdecl Game_MinBuildNumber() { return MIN_BUILD; }

// 0x5908c0 (the out string in ecx): "<version> <Build> <build>" ("1.0 Build 1308"). eax: ZString_Format's.
DWORD THISCALL Game_VersionText(void *out)
{
    return zs_format(out, glob_addr(0x6236bc) /* "%s %s %d" */, *glob<const char **>(0x623640), zs_c_str(glob<void *>(TEXT_BUILD)), BUILD);
}

// 0x5908f0 (the out string in ecx): "<Zax>[ <Demo>] <Version> <version text>", the demo part when the byte
// 0x62485c is set. eax: the last ZString destructor's.
DWORD THISCALL Game_FullVersionText(void *out)
{
    ZString version, demo;
    zs_ctor(reinterpret_cast<BYTE *>(&version));
    zs_format(&version, glob_addr(0x6236bc) /* "%s %s %d" */, *glob<const char **>(0x623640), zs_c_str(glob<void *>(TEXT_BUILD)), BUILD);
    zs_ctor(reinterpret_cast<BYTE *>(&demo));
    if (*glob<BYTE *>(0x62485c))
        zs_format(&demo, glob_addr(0x6236d4) /* " %s" */, zs_c_str(glob<void *>(TEXT_DEMO)));
    const char *word = zs_c_str(glob<void *>(TEXT_VERSION));
    const char *d = zs_c_str(&demo);
    zs_format(out, glob_addr(0x6236c8) /* "%s%s %s %s" */, *glob<const char **>(0x62363c), d, word, zs_c_str(&version));
    zs_destroy(&demo);
    return zs_destroy(&version);
}

// The object of record i of a definition list, loaded on first use (as 0x590400 in quest_data.cpp; the
// inline lookup of 0x59ff80)
void *record_object(ZDefinitionList *l, DWORD i)
{
    if (!l->entries)
        return nullptr;
    if (i >= l->num_entries)
        i = 0;
    if (l->entries[i].loading)
        return nullptr;
    if (!l->entries[i].object && !l->entries[i].loading) {
        l->entries[i].loading = 1;
        char path[0x100];
        deflist::path_join(path, l->folder, vcall<const char *>(l, 1, i));
        char *name = deflist::path_name(path);
        void *o;
        if (name == path) {
            o = vcall<void *>(l, 37, glob<const char *>(TEXT_EMPTY), name, &l->entries[i].object);
        } else {
            name[-1] = 0;
            o = vcall<void *>(l, 37, path, name, &l->entries[i].object);
        }
        l->entries[i].object = o;
        l->entries[i].loading = 0;
    }
    return l->entries[i].object;
}

// 0x59ff80: the game type of the application (+0x58c the index in the game type list, loaded on first
// use); null for -1, without records or while loading
void *THISCALL App_GameType(BYTE *app)
{
    const DWORD i = dw(app, 0x58c);
    if (i == 0xffffffff)
        return nullptr;
    return record_object(glob<ZDefinitionList *>(GAME_TYPES), i);
}

// 0x59e6f0 (player): whether the player switches weapons when out of ammunition: the local player (+5)
// from Zax.CFG's "Weapon AutoSwitch" (default 1; eax 0 or 1), another one from its settings (+0x1c, +0xc;
// al, the upper bytes of the settings pointer)
DWORD THISCALL App_WeaponAutoSwitch(void *, BYTE *player)
{
    if (fl<BYTE>(player, 5))
        return fn<int(__fastcall *)(void *, void *, const char *, int)>(0x4a4670)(glob<void *>(0x6bd330), glob<void *>(0x6bd430),
                                                                                glob<const char *>(0x60fa14), 1) != 0;
    const DWORD settings = dw(player, 0x1c);
    return deflist::with_low_byte(settings, fl<BYTE>(bp(settings), 0xc));
}

// 0x59db30 (a slot of the application's vtable): 0x4f2d90 (World), then the application base's
// 0x4ce160 when it gave al 1; otherwise al 0
DWORD THISCALL App_FrameStart(BYTE *app)
{
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x4f2d90)(app);
    if (!(r & 0xff))
        return r & ~0xffu;
    return fn<DWORD(THISCALL *)(void *)>(0x4ce160)(app);
}

// 0x59db50 (a slot of the application's vtable; a, b): World::MultiplayerFrame (0x4f0230) with b's low
// byte cleared while the materials generator window is open (0x505330). eax: its.
DWORD THISCALL App_MultiplayerFrame(BYTE *app, DWORD a, DWORD b)
{
    if (fn<DWORD(__cdecl *)()>(0x505330)() & 0xff)
        b &= ~0xffu;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f0230)(app, a, b);
}

// 0x59b3a0 (map, start point, position, a byte, "Return to map"): the pending map switch the frame
// carries out (CSwitchMapAction::Switch). eax: the position's second dword.
DWORD THISCALL App_RequestSwitchMap(BYTE *app, const char *map, const char *spawn, const DWORD *position, DWORD flag, DWORD return_to_map)
{
    zs_assign_text(app + 0x578, map);
    zs_assign_text(app + 0x57c, spawn);
    dw(app, 0x570) = position[0];
    const DWORD y = position[1];
    dw(app, 0x574) = y;
    fl<BYTE>(app, 0x580) = (BYTE)flag;
    dw(app, 0x584) = return_to_map;
    fl<BYTE>(app, 0x588) = 1;
    return y;
}

// ---- Menus, players, chat -------------------------------------------------------------------------------------------

// The application base's +0x34 (0x4cdf30, the screen frame [assumption])
void *app_frame(BYTE *app) { return fn<void *(THISCALL *)(void *)>(0x4cdf30)(app); }

// 0x59baf0 (slot 34): the death screen ("You Have Died" with "Restart Map"; pool 0x13c, constructor
// 0x5c09a0 with the frame) shown through the application's slot 23 (window, 1; a failed allocation shows
// null). eax: slot 23's.
DWORD THISCALL App_OpenDeathScreen(BYTE *app)
{
    void *w = pool_alloc(0x13c);
    if (w)
        w = fn<void *(THISCALL *)(void *, void *)>(0x5c09a0)(w, app_frame(app));
    return vcall<DWORD>(app, 23, w, 1);
}

// 0x59bb30 (slot 35): the game menu (pool 0x154, CZaxMenu_ctor 0x5bfc40 with the frame) shown the same way.
// al 1 over slot 23's eax.
DWORD THISCALL App_OpenGameMenu(BYTE *app)
{
    void *w = pool_alloc(0x154);
    if (w)
        w = fn<void *(THISCALL *)(void *, void *)>(0x5bfc40)(w, app_frame(app));
    return (vcall<DWORD>(app, 23, w, 1) & ~0xffu) | 1;
}

// The net layer singleton 0x713f18 (operator new 8, NetLocal_Ctor, vtable 0x602fa8), made on first use
void *net_local()
{
    void *&n = *glob<void **>(0x713f18);
    if (!n) {
        void *p = heap_alloc(8);
        if (p) {
            fn<void *(THISCALL *)(void *)>(0x4f7df0)(p);
            dw(p, 0) = vt(0x602fa8);
        }
        n = p;
    }
    return n;
}

// 0x59bd50 (slot 107: layer, count, a3, a4): the characters deleted (World::DeleteCharacters); for every
// player i below the count an empty entry in the player part list (+0x284, 0x4ef9b0), the net layer told
// (its slot 9 with ZaxNet_PlayerAt of 0x5fe8f8) and the player's character made (slot 113: layer, i, a3,
// a4); then every part of the list into the layer's monitor list (CLayerSaveData::MonitorAddPart). eax:
// the list's count.
DWORD THISCALL App_CreatePlayers(BYTE *app, BYTE *layer, DWORD count, DWORD a3, DWORD a4)
{
    fn<void(THISCALL *)(void *)>(0x4f4ce0)(app);
    for (DWORD i = 0; i < count; i++) {
        fn<void(THISCALL *)(void *, void *)>(0x4ef9b0)(app + 0x284, nullptr);
        void *n = net_local();
        const auto tell = at<void(THISCALL *)(void *, void *)>(*at<DWORD *>(dw(n, 0) + 0x24));   // slot 9, read first
        void *player = fn<void *(__fastcall *)(DWORD)>(0x5ba810)(*glob<DWORD *>(0x5fe8f8));
        tell(n, player);
        vcall<void>(app, 113, layer, i, a3, a4);
    }
    const auto list_count = code<DWORD(THISCALL *)(void *)>(0x4ef8f0);
    DWORD n = list_count(app + 0x284);
    for (DWORD i = 0; i < n; n = list_count(app + 0x284)) {
        void *part = *fn<void **(THISCALL *)(void *, DWORD)>(0x4ef8e0)(app + 0x284, i);
        fn<void(THISCALL *)(void *, void *)>(0x4e7330)(layer + 0x3c, part);
        i++;
    }
    return n;
}

// 0x59df90 (slot 113: layer, i, a3, a4): player i's character (slot 32) named "<name>'s Player" when the
// net game (slot 1) runs (its slot 13), else "Debug only player"; slot 39 (part, i); the player part list's
// entry i (0x4ef900); its CPlayerControlAI told the part (slots 43 (0, part) and 44); into the layer
// (slot 104) unless already in its grid (+0x50); moved to its team's start point (World::MoveToTeamSpawn
// with a3, a4). eax: the last call's.
DWORD THISCALL App_CreatePlayer(BYTE *app, BYTE *layer, DWORD i, DWORD a3, DWORD a4)
{
    BYTE *part = vcall<BYTE *>(app, 32, i);
    if (vcall<DWORD>(vcall<void *>(app, 1), 13) & 0xff) {
        ZString name;
        zs_ctor(reinterpret_cast<BYTE *>(&name));
        void *player = fn<void *(THISCALL *)(void *, DWORD)>(0x51f440)(vcall<void *>(app, 1), i);
        zs_format(&name, glob_addr(0x624cf8) /* "%s's Player" */, zs_c_str(fn<void *(THISCALL *)(void *)>(0x5b7aa0)(player)));
        fn<void(THISCALL *)(void *, const char *)>(0x4fbf00)(part, zs_c_str(&name));
        zs_destroy(&name);
    } else {
        fn<void(THISCALL *)(void *, const char *)>(0x4fbf00)(part, glob<const char *>(0x624ce4) /* "Debug only player" */);
    }
    vcall<void>(app, 39, part, i);
    fn<void(THISCALL *)(void *, DWORD, void *)>(0x4ef900)(app + 0x284, i, part);
    if (BYTE *ai = vcall<BYTE *>(part, 64, fn<Getter>(0x542170)(), 0)) {   // CPlayerControlAI
        vcall<void>(ai, 43, 0, part);
        vcall<void>(ai, 44);
    }
    if (!(fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x579e70)(layer + 0x50, part, 1) & 0xff))
        vcall<void>(app, 104, part, layer);
    return fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD)>(0x4f4950)(app, part, layer, a3, a4);
}

// 0x59b230 (text): a text block (type 3, 0x5ac440 of the reader 0x714070) into the net game's (world slot
// 44) packet (its slot 25). al 1.
DWORD __stdcall NetGame_SendText(const char *text)
{
    void *net = vcall<void *>(world(), 44);
    void *packet = vcall<void *>(net, 25);
    void *reader = fn<void *(__cdecl *)()>(0x5ac380)();
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x5ac440)(reader, packet, text);
}

// 0x59b260 (text, font): the chat line to every player's packet (the net game's slot 13 for player i,
// ChatFromServerReader::Write); font -1 is the custom rules' chat font (their +0x10 object, made on first
// use: pool 0x78, vtable 0x5f14f0, its slot 19; its +8). eax: the player count.
DWORD __stdcall NetGame_SendChatToAll(const char *text, DWORD font)
{
    if (font == 0xffffffff) {
        BYTE *holder = bp(dw(*glob<BYTE **>(0x713f14) /* g_App */, 0x2e0));
        BYTE *rules = nullptr;
        if (dw(holder, 0x38)) {
            CClassInfo *c = class_getter(0x713f24, 0x6249b0, 1.0f, getter<0x560ac0>, 0x30, 0, 0x5a13d0, 0x5a1410, 0x5a1580);
            if (is_kind_of(bp(dw(holder, 0x38)), c))
                rules = bp(dw(holder, 0x38));
        }
        if (!dw(rules, 0x10)) {
            BYTE *o = static_cast<BYTE *>(pool_alloc(0x78));
            if (o) {
                obj_ctor(o);
                vtable_and_defaults(o, vt(0x5f14f0));
            }
            dw(rules, 0x10) = (DWORD)(uintptr_t)o;
            vcall<void>(o, 19);
        }
        font = dw(bp(dw(rules, 0x10)), 8);
    }
    void *net = vcall<void *>(world(), 44);
    DWORD n = vcall<DWORD>(net, 11);
    for (DWORD i = 0; i < n; n = vcall<DWORD>(net, 11)) {
        void *packet = vcall<void *>(net, 13, i);
        fn<DWORD(THISCALL *)(void *, void *, const char *, DWORD)>(0x5bcd80)(fn<void *(__cdecl *)()>(0x5bcd30)(), packet, text, font);
        i++;
    }
    return n;
}

}  // namespace

ZAX_HOOKS(app_flow)
{
    const Hook list[] = {
        {0x5908a0, ptr(&Game_BuildNumber), "Game_BuildNumber", 5, {0xb8, 0x1c, 0x05, 0x00, 0x00, 0xc3, 0x90, 0x90}},
        {0x5908b0, ptr(&Game_MinBuildNumber), "Game_MinBuildNumber", 5, {0xb8, 0x1a, 0x05, 0x00, 0x00, 0xc3, 0x90, 0x90}},
        {0x5908c0, ptr(&Game_VersionText), "Game_VersionText", 8, {0x56, 0x8b, 0xf1, 0x68, 0x1c, 0x05, 0x00, 0x00}},
        {0x5908f0, ptr(&Game_FullVersionText), "Game_FullVersionText", 6, {0x83, 0xec, 0x08, 0x53, 0x8b, 0xd9, 0x56, 0x57}},
        {0x59ff80, ptr(&App_GameType), "Game_AppGameType", 6, {0x8b, 0x91, 0x8c, 0x05, 0x00, 0x00, 0x81, 0xec}},
        {0x59e6f0, ptr(&App_WeaponAutoSwitch), "Game_WeaponAutoSwitch", 7, {0x8b, 0x44, 0x24, 0x04, 0x8a, 0x48, 0x05, 0x84}},
        {0x59db30, ptr(&App_FrameStart), "Game_AppFrameStart", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x58, 0x52, 0xf5, 0xff}, 4},
        {0x59db50, ptr(&App_MultiplayerFrame), "Game_AppMultiplayerFrame", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xd8, 0x77, 0xf6, 0xff}, 4},
        {0x59b3a0, ptr(&App_RequestSwitchMap), "Game_RequestSwitchMap", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x59baf0, ptr(&App_OpenDeathScreen), "Game_OpenDeathScreen", 9, {0x56, 0x8b, 0xf1, 0x57, 0xb9, 0x3c, 0x01, 0x00}},
        {0x59bb30, ptr(&App_OpenGameMenu), "Game_OpenGameMenu", 9, {0x56, 0x8b, 0xf1, 0x57, 0xb9, 0x54, 0x01, 0x00}},
        {0x59bd50, ptr(&App_CreatePlayers), "Game_CreatePlayers", 5, {0x53, 0x56, 0x57, 0x8b, 0xd9, 0xe8, 0x86, 0x8f}},
        {0x59df90, ptr(&App_CreatePlayer), "Game_CreatePlayer", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x59b230, ptr(&NetGame_SendText), "NetGame_SendText", 6, {0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00, 0x8b, 0x01}},
        {0x59b260, ptr(&NetGame_SendChatToAll), "NetGame_SendChatToAll", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x83, 0xfb}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
