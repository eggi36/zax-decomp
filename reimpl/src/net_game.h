// The game-side code of the net game around GameSpy (0x51f180-0x51f5a0, 0x51f880) and the net traffic
// graph's drawing and update (0x51f9a0, 0x520150).
//
// The net game object is the DirectPlay 8 session of the multiplayer (vtable 0x5f15a8 for the
// DirectPlay flavour, built around 0x47eb76; its base class lives at 0x51e000-0x51efff). The
// functions here only touch its game-side part [confirmed: code of these functions and their
// callers 0x47ee70, 0x47eec0, 0x47eed0, 0x47f0b0, 0x480bd0]:
//   +0x04 the game type object (slots 15-18 add the GameSpy basic/info/rules/team keys of the type,
//         slot 20 makes a player, slot 21 a team)
//   +0x08 the settings: +0x0c max players, +0x10 time limit, +0x14 frag limit, +0x18 byte "dedicated"
//         (chooses the GameSpy think function)
//   +0x10 the players: a pointer array {vtable, data +0x14, count +0x18, capacity +0x1c} of net
//         sessions (NetSession, 0x4f7c50; +0x0c id, +0x14 team, +0x1c the player settings whose first
//         member is the name)
//   +0x20 the teams: a pointer array {vtable, data +0x24, count +0x28, capacity +0x2c} of named values
//         (NamedValue: name, score)
//   +0x30 the GameSpy state text ("openplaying", "exiting")
// The GameSpy query answers append "\key\value" pairs to a ZString with ZString::AppendFormat, which
// returns nothing, so their eax is undefined. GameSpy itself (the qr SDK linked into the exe,
// 0x5c5000-0x5c9fff: init 0x5c82a0, 0x5c5280, the shutdown 0x5c8f20/0x5c8fb0/0x5c5440, the think
// functions 0x5c85c0/0x5c8a60 and 0x5c5840) stays as it is; Start/Stop/Think call it unchanged.
// Names are ours [assumption].
//
// Not hookable (4 bytes): 0x51f450 (player count: mov eax, [ecx + 0x18]; ret).
#pragma once
#include "net_packet.h"

namespace zax {

struct NetGameSettings {
    BYTE unknown0[0x0c];
    DWORD max_players;   // 0x0c
    DWORD time_limit;    // 0x10
    DWORD frag_limit;    // 0x14
    BYTE dedicated;      // 0x18
};

struct NetPlayer;
struct NetGame {
    DWORD vftable;
    void *game_type;            // 0x04
    NetGameSettings *settings;  // 0x08
    DWORD unknownc;             // 0x0c
    ZPtrArray players;          // 0x10 NetPlayer * (net sessions)
    ZPtrArray teams;            // 0x20 NamedValue *
    ZString state;              // 0x30

    void THISCALL GameSpyBasicInfo(ZString *out);    // 0x51f180 (eax: slot 15's)
    void THISCALL GameSpyServerInfo(ZString *out);   // 0x51f190
    void THISCALL GameSpyRules(ZString *out);        // 0x51f1d0
    DWORD THISCALL GameSpyTeams(ZString *out);       // 0x51f220 (the team count)
    void THISCALL GameSpyStart();                    // 0x51f310 (eax: the ZString destructor's)
    DWORD THISCALL GameSpyStop();                    // 0x51f3e0 (0x5c5440's eax)
    DWORD THISCALL GameSpyThink();                   // 0x51f420 (0x5c5840's eax)
    NetPlayer *THISCALL PlayerAt(DWORD i);           // 0x51f440 (no bounds check)
    NamedValue *THISCALL TeamAt(DWORD i);            // 0x51f460
    DWORD THISCALL SetTeamCount(DWORD n);            // 0x51f480
    DWORD THISCALL FindPlayerById(DWORD id);         // 0x51f530
    DWORD THISCALL FirstUnusedTeam();                // 0x51f560
    DWORD THISCALL SmallestTeam(DWORD n);            // 0x51f5a0
};
static_assert(offsetof(NetGame, state) == 0x30);

// The player in the net game's list: a net session (NetSessionBase at the start)
struct NetPlayer {
    NetSessionBase base;
    void THISCALL GameSpyPlayerInfo(ZString *out, DWORD index);   // 0x51f880 ("name", "ping")
};

// The "\key\value" helpers (fastcall: the string in ecx, the key or index in edx)
void __fastcall GameSpy_AppendKeyText(ZString *out, const char *key, const char *value);            // 0x51f290
void __fastcall GameSpy_AppendKeyInt(ZString *out, const char *key, int value);                     // 0x51f2b0
void __fastcall GameSpy_AppendPlayerText(ZString *out, DWORD index, const char *key, const char *value);  // 0x51f2d0
void __fastcall GameSpy_AppendPlayerInt(ZString *out, DWORD index, const char *key, int value);     // 0x51f2f0

// The traffic graph's drawing (thiscall on the graph) and the update of all graphs of a net layer
// (fastcall: the surface in ecx, the net layer in edx, the slot of the "all traffic" graph on the stack)
struct NetTrafficGraphDraw {
    NetTrafficGraph graph;
    void THISCALL Draw(void *surface, DWORD sent, DWORD received, DWORD index);   // 0x51f9a0
};
DWORD __fastcall NetTrafficGraph_Update(void *surface, void *net, NetTrafficGraph **all);   // 0x520150

constexpr DWORD G_ERROR_REPORTER = 0x6c02cc;
constexpr DWORD G_NET_GRAPHS_ON = 0x6d00ec;        // byte: draw the traffic graphs
constexpr DWORD G_NET_GRAPH_SCALE = 0x61caf0;      // the highest sample so far (starts at 1)
constexpr DWORD G_NET_GRAPH_GUARD = 0x6d00c0;      // guard bits of the three colours
constexpr DWORD G_NET_GRAPH_COLORS = 0x6d00b0;     // 0x6d00b0 sent, 0x6d00c8 received, 0x6d00d8 frame
constexpr DWORD G_FONTS = 0x6be1e0;                // the font list (resource list)

// The resource list's inlined Get(i) (the same code in 0x4f7fc0, 0x46a490, …): the records of 0x80
// bytes at list + 0x30 ({…, +0x78 the object, +0x7f "loading"}), the count at + 0x34, the folder at
// + 0x38; an object not loaded yet is created through slot 37 (folder, name, &object)
void *resource_list_get(DWORD list, DWORD index);

}  // namespace zax
