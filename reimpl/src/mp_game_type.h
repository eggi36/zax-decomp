// The multiplayer game mode base class CMultiPlayerGameType (0x51a270-0x51d400, slot 36 0x468f20) and
// the match state CMultiPlayerGameData (0x51a040-0x51a160).
//
// CMultiPlayerGameType (0xc8 bytes, class 0x6d0020, version 0, abstract, parent CGameType 0x4ce850):
// its own vtable 0x5fb160 is only written by the constructor 0x51bfa0 and the destructors 0x51bfc0/
// 0x51bff0 (around the CGameType ones 0x4ceaa0/0x4ceb20). The derived CCaptureTheFlagGameType (0x5ef544), CDeathMatchGameType (0x5f0d54)
// and CSalvageKingGameType (0x5fed48) share its slots 23, 26-29, 31, 32, 36-38, 43, 46-48 (DM overrides
// 24). Layout [confirmed: RegisterFields 0x51a320, docs/fields.csv]: the CGameType part (+8 "Game Name"
// local text, +0x10 "Players Initial Inventory", +0x14 "Default Player Model"), then
//   +0x18 "Max Number of Teams" (0 = "Unlimited"), +0x1c "Is Team Game", +0x1d "All Teams Share Spawn
//   Points", +0x20/+0x24/+0x28 the actions "Have Taken/Lost/Tied For the Lead", the mini score board
//   (+0x2c-+0x40 fonts, +0x44/+0x48 the flag strings, +0x4c-+0x84 the columns' x and justification,
//   +0x88 line spacing, +0x8c header spacing, +0x90 top) and the end score board (+0x94-+0xc4).
// Font fields are indices into the font list 0x6be1e0 (records +0x30, count +0x34, folder +0x38),
// justifications indices into the justification list 0x6be1b8.
//
// CMultiPlayerGameData (0x18 bytes, vtable 0x5fb104, class 0x6d001c, version 0, parent CObject):
// +8 "Match Is Over" (byte), +0xc "Players Ready To Start New Game", +0x10 the match time (double, not
// a field; not set by Create). The application's slot 97 gives an object whose +0x30 holds the current
// one; every user fetches it again and casts it with IsKindOf (inlined class getter).
//
// The globals the slots use [confirmed: the static initialisers 0x51ca50-0x51cb90 and 0x51d200-0x51d3f0
// construct them from these texts]: 0x713f14 the application, 0x6c2080 the world (slot 1 the net game,
// slot 44 the connections, +0x284 the player entities), ZStrings 0x6d0018 "Name", 0x6cffe8 "Ping",
// 0x6cfff8 "Score", 0x6cffe4 "Time Limit:", 0x6d000c "Minutes", 0x6d0000 "%s Limit:", 0x6d0004
// "%s changed teams", 0x6d0008 "%s killed TEAMMATE %s", 0x6d0010 "%s killed %s", 0x6cfff0 "%s killed
// himself".
//
// Return values are the original's eax (vf31: st0; vf36/37: al with the caller's upper bytes) for the
// class getters, constructor, destructors, Create, CreateArray, RegisterFields and slots 23, 27-29, 31,
// 36-38 and 46-48. Slots 24, 26, 32, 43 and 0x51c650/0x51c800 are void in the original's use
// [assumption]: their eax (status words, IsKindOf's upper bytes, counts) is not reproduced.
#pragma once
#include "object_base.h"

namespace zax {

struct CMultiPlayerGameData : CObjectBase {
    BYTE match_over;      // 0x08
    BYTE pad09[3];
    DWORD players_ready;  // 0x0c
    double match_time;    // 0x10

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x51a0d0 (slot 1)
};
static_assert(sizeof(CMultiPlayerGameData) == 0x18);

struct CMultiPlayerGameType : CObjectBase {
    BYTE game_type_part[0x10];   // 0x08 the CGameType fields
    DWORD max_teams;             // 0x18
    BYTE is_team_game;           // 0x1c
    BYTE share_spawn_points;     // 0x1d
    BYTE pad1e[2];
    void *taken_lead;            // 0x20 actions
    void *lost_lead;             // 0x24
    void *tied_lead;             // 0x28
    DWORD mini_font;             // 0x2c "Default Mini-ScoreBoard Font"
    DWORD mini_headings_font;    // 0x30
    DWORD mini_blue_font;        // 0x34
    DWORD mini_red_font;         // 0x38
    DWORD mini_blue_flag_font;   // 0x3c
    DWORD mini_red_flag_font;    // 0x40
    DWORD mini_blue_flag;        // 0x44 text
    DWORD mini_red_flag;         // 0x48
    DWORD mini_rank_x, mini_rank_just;           // 0x4c
    DWORD mini_flag_x, mini_flag_just;           // 0x54
    DWORD mini_name_x, mini_name_just;           // 0x5c
    DWORD mini_score_x, mini_score_just;         // 0x64
    DWORD mini_ping_x, mini_ping_just;           // 0x6c
    DWORD mini_alt1_x, mini_alt1_just;           // 0x74
    DWORD mini_alt2_x, mini_alt2_just;           // 0x7c
    DWORD mini_rules_just;                       // 0x84
    DWORD mini_line_spacing;                     // 0x88
    DWORD mini_header_spacing;                   // 0x8c
    DWORD mini_top;                              // 0x90
    BYTE end_board[0x34];                        // 0x94 the end score board's fields

    CMultiPlayerGameType *THISCALL Ctor();                   // 0x51bfa0
    CMultiPlayerGameType *THISCALL DeletingDtor(DWORD flags);   // 0x51bfc0 (slot 1 of 0x5fb160)
    DWORD THISCALL Dtor();                                      // 0x51bff0
    DWORD THISCALL StartMatch();                       // 0x51c010 (slot 23)
    void THISCALL Update(float dt);                    // 0x51c310 (slot 24)
    void THISCALL AddTeamScore(DWORD team, int n);     // 0x51bb90 (slot 26)
    DWORD THISCALL UpdateLeads();                      // 0x51bda0 (slot 27)
    DWORD THISCALL AssignTeam(BYTE *player);           // 0x51bef0 (slot 28)
    DWORD THISCALL Slot29(DWORD a, DWORD entity, DWORD c);   // 0x51d190 (slot 29)
    double THISCALL MatchTime();                       // 0x51c1f0 (slot 31)
    void THISCALL SetMatchTime(float t);               // 0x51c270 (slot 32)
    // slot 36 (0x468f20, +0x1d) and slot 37 (0x51d150, +0x1c) are two lines of assembly in the .cpp
    DWORD THISCALL Slot38();                           // 0x51d160 (slot 38)
    void THISCALL DrawMiniScoreBoard(void *surface);   // 0x51cba0 (slot 43)
    DWORD THISCALL PlayerRemoved(void *entity);        // 0x51d1a0 (slot 46)
    DWORD THISCALL ChangeTeam(DWORD player);           // 0x51d240 (slot 47)
    DWORD THISCALL KillMessage(void *killer, void *victim);  // 0x51d400 (slot 48)
    void THISCALL EndMatch();                          // 0x51c650
    void THISCALL PlayerReady();                       // 0x51c800
};
static_assert(sizeof(CMultiPlayerGameType) == 0xc8);

constexpr DWORD MP_GAME_DATA_CLASS = 0x6d001c, MP_GAME_TYPE_CLASS = 0x6d0020, MP_GAME_DATA_VTABLE = 0x5fb104;
constexpr DWORD MP_GAME_TYPE_VTABLE = 0x5fb160;   // abstract: written by the constructor and destructors
constexpr DWORD MP_APP = 0x713f14, MP_WORLD = 0x6c2080, FONT_LIST = 0x6be1e0;

}  // namespace zax
