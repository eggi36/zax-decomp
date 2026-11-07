// The world class of Zax.exe (the game object in g_World 0x6c2080; base vtable 0x5f8fc0 of 108 slots,
// derived by CZaxWorld 0x602dc8, whose own code lies in 0x598000-0x5a4fff): its constructor and
// destructor, the layer list (current layer, the layer of a player, finding, adding, unloading, loading
// and saving layers), the loading counter, starting a new game, the per-frame time and view update, the
// player characters and spawn points, the multiplayer broadcasts and the small slots of the class.
//
// The class [confirmed: constructor 0x4f0780, destructor 0x4f0c70] derives from the application base
// (constructor 0x4cd780, destructor 0x4cdcc0; +0x04 the display, whose slots 39/45 give its height and
// width) and is 0x338 bytes:
//   +0x280 the message lines (0x1e84 bytes, constructor 0x506730(0xb4, 0x41), destructor 0x5067b0;
//          rest_c.cpp; until 2026-10-26 called a timer pool here)
//   +0x284 the character list holder (a CObject, vtable 0x5f8ef0, 0x18 bytes) with its pointer array
//          (vtable 0x5f8f4c) at +0x28c: data +0x290, count +0x294, capacity +0x298 (the player characters;
//          the holder listens to each of them)
//   +0x29c byte (read by the multiplayer flag messages 0x4e76e0)
//   +0x2a0 the game type (slot 1; its slot 12 tells a multiplayer game, slot 17 registers block readers)
//   +0x2a4 the zoom frame, +0x2a8 the screen frame (CFrame, 0x40 bytes), +0x2ac a file buffer over a
//          memory stream
//   +0x2b0 CLayerHolder (0x2c bytes, vtable 0x5f8f64) whose pointer array (vtable 0x5f7b88) at +0x2b8 is
//          the layer list: data +0x2bc, count +0x2c0, capacity +0x2c4; +0x2c8 "UniqueID", +0x2cc
//          "Playing Time" and +0x2d4 "Running Time" (doubles)
//   +0x2e0 CRules (+8 "First Map Name", +0xc "First Map Spawn Point", +0x10/+0x14 the view margins)
//   +0x2e4 the view rectangle (left, top, right, bottom; bottom = display height - 0x38)
//   +0x2f4 float time since the game started, +0x2f8 byte, +0x2f9 "Show Polygons", +0x2fa "Show Grid"
//          (Zax.ini)
//   +0x2fc..+0x308 four owned frames of the loading screen, +0x30c the loading counter (BeginLoading)
//   +0x310 byte "game running", +0x311 byte "a level is loaded", +0x312 byte
//   +0x314 a pointer array (vtable 0x5f8b8c; data +0x318, count +0x31c, capacity +0x320) that holds the
//          player characters while a layer is saved
//   +0x324 double "mission time", +0x32c byte "saving", +0x330 the traffic graph (heap), +0x334 the music
//          mixer (0x34 bytes, constructor 0x51e360)
//
// The players (slot 44 returns the net layer: slot 11 count, slot 12 the packet of player 0, slot 13 the
// packet of player i, slot 23 player i, slot 24/27 per-frame calls) are records with +0x38/+0x39 bytes
// (sync requested), +0xc0/+0xc4 the view position, +0xc8/+0xcc the view target, +0xd0/+0xd4 the smoothed
// target, +0xd8 the zoom, +0xdc the index of the player's layer in the layer list, +0xe0 the zoom keys,
// +0xe8 the controlled entity. Slot 43 returns the same players through its slot 22. [names of the slots
// and fields: assumption from what the code does with them]
//
// Not here: the frame drawing (slot 4 0x4f2d90, 0x4f2800, 0x4f2be0, slot 3 0x4f2620) and rendering a layer
// into a frame (0x4f4ae0) are in world_frame.cpp; the loading screen (0x4f3ba0, 0x4f3d50), the display mode
// change (slot 8 0x4f2430) and the menu background (slot 105 0x4f12f0) are open (rendering). The game data loading, save games, fixups,
// zoom, multiplayer frame, explosion spawns, preloads and list loaders are in world_rest.h.
//
// Return values are what the original leaves in eax where callers can see it; "al" means only the low
// byte is defined, "st(0)" a floating-point result, "eax not reproduced" that callers use the function
// as void.
#pragma once
#include "layer.h"

namespace zax {

#pragma pack(push, 4)   // the doubles at +0x2cc, +0x2d4 and +0x324 are only 4-byte aligned
struct World {
    DWORD vftable;
    void *display;              // 0x04
    BYTE base[0x278];           // 0x08 the application base
    void *timers;               // 0x280
    DWORD holder_vftable;       // 0x284 the character list holder (CObject)
    DWORD holder_fields;        // 0x288
    LayerArray characters;      // 0x28c
    BYTE flag29c, pad29d[3];    // 0x29c
    void *game_type;            // 0x2a0
    void *zoom_frame;           // 0x2a4
    void *screen_frame;         // 0x2a8
    void *file_buffer;          // 0x2ac
    DWORD layer_holder[2];      // 0x2b0 CLayerHolder: vtable, fields
    LayerArray layers;          // 0x2b8
    DWORD unique_id;            // 0x2c8
    double playing_time;        // 0x2cc
    double running_time;        // 0x2d4
    DWORD pad2dc;               // 0x2dc
    BYTE *rules;                // 0x2e0
    int view[4];                // 0x2e4
    float time;                 // 0x2f4
    BYTE byte2f8, show_polygons, show_grid, pad2fb;   // 0x2f8
    void *loading_frames[4];    // 0x2fc
    DWORD loading;              // 0x30c
    BYTE running, level_loaded, byte312, pad313;      // 0x310
    LayerArray saved_characters;   // 0x314
    double mission_time;        // 0x324
    BYTE saving, pad32d[3];     // 0x32c
    void *traffic_graph;        // 0x330
    void *mixer;                // 0x334

    static World *THISCALL Ctor(World *self, DWORD a1, DWORD a2, DWORD a3);               // 0x4f0780
    static World *THISCALL DeletingDtor(World *self, DWORD flags);                         // 0x4f0c50 (slot 0)
    static void THISCALL Dtor(World *self);                                                // 0x4f0c70 (eax not reproduced)
    // Layers
    static Layer *THISCALL CurrentLayer(World *self);                                      // 0x4f1050
    static Layer *THISCALL PlayerLayer(World *self, DWORD player);                         // 0x4f10a0
    static Layer *THISCALL FindLayer(World *self, const char *name);                       // 0x4f4d70
    static void THISCALL AddLayer(World *self, Layer *layer);                              // 0x4f4650 (eax not reproduced)
    static void THISCALL UnloadLayer(World *self, Layer *layer);                           // 0x4f3490 (eax not reproduced)
    static Layer *THISCALL LoadLayer(World *self, const char *name, DWORD a2);             // 0x4f45b0
    static DWORD THISCALL SaveLayer(World *self, Layer *layer, DWORD a2, DWORD mode);      // 0x4f4380
    static DWORD THISCALL LevelExists(World *self, const char *name);                      // 0x4f4ca0
    static DWORD THISCALL LevelPath(World *self, const char *name, DWORD unused, char *out);   // 0x4f3a40 (slot 68, al)
    static DWORD THISCALL SaveLayerFile(World *self, Layer *layer, const char *path, DWORD mode, DWORD unused);   // 0x4f4330 (slot 75, al)
    static DWORD THISCALL LoadLayerStream(World *self, Layer *layer, void *stream, DWORD fix, const char *name);   // 0x4f4500 (slot 76, al)
    static DWORD THISCALL LoadLayerFile(World *self, Layer *layer, const char *path, const char *name, DWORD fix);   // 0x4f43f0 (slot 77, al)
    static void THISCALL SetPlayerLayer(World *self, Layer *layer, DWORD player);          // 0x4f4700 (slot 78; eax not reproduced)
    static void THISCALL UpdateTileMaps(World *self);                                      // 0x4f2560 (slot 7; eax not reproduced)
    static DWORD THISCALL RemovePartFrom(World *self, void *part, Layer *layer);           // 0x4f4e30 (slot 103; eax not reproduced)
    static DWORD THISCALL AddPartTo(World *self, void *part, Layer *layer);                // 0x4f4e50 (slot 104; eax not reproduced)
    static DWORD __stdcall IsLoadingLayer(Layer *layer);                                   // 0x4f4d30 (thiscall, this unused)
    static const char *__stdcall LoadingName();                                            // 0x4f4d50 (thiscall, this unused)
    static DWORD __stdcall LoadingFlag();                                                  // 0x4f4d60 (thiscall, this unused; al)
    // Game flow
    static void THISCALL BeginLoading(World *self);                                        // 0x4f3cc0 (eax not reproduced)
    static void THISCALL EndLoading(World *self);                                          // 0x4f3d30 (eax not reproduced)
    static void THISCALL StartNewGame(World *self, DWORD a1, const char *map, const char *spawn, DWORD a4);   // 0x4f4e70 (slot 79; eax not reproduced)
    static void THISCALL FinishLevelStart(World *self, Layer *layer);                      // 0x4f4890 (eax not reproduced)
    static void THISCALL GameStarted(World *self);                                         // 0x4f5110 (slot 80; eax not reproduced)
    static void THISCALL EndGame(World *self, DWORD unused);                               // 0x4f5150 (slot 81; eax not reproduced)
    static void THISCALL Update(World *self, float dt);                                    // 0x4f3560 (slot 48; eax not reproduced)
    static void THISCALL UpdateViews(World *self, float dt);                               // 0x4f37e0 (slot 55; eax not reproduced)
    static DWORD THISCALL WriteState(World *self, void *stream, DWORD binary);             // 0x4f3b40 (slot 72)
    // Players and characters
    static void *THISCALL Character(World *self, DWORD i);                                 // 0x4f5d60
    static void *THISCALL FirstCharacter(World *self);                                     // 0x4f5d80
    static void THISCALL DeleteCharacters(World *self);                                    // 0x4f4ce0 (eax not reproduced)
    static void THISCALL TakeCharacters(World *self, Layer *layer, LayerArray *out);       // 0x4f5270 (slot 69; eax not reproduced)
    static void THISCALL PutCharacters(World *self, Layer *layer, LayerArray *list, DWORD notify);   // 0x4f52f0 (slot 70; eax not reproduced)
    static void THISCALL PlaceAt(World *self, void *part, Layer *layer, void *reference, const float *offset);   // 0x4f5360 (slot 82; eax not reproduced)
    static void THISCALL MoveToSpawnPoint(World *self, void *part, Layer *layer, const char *name, const float *offset);   // 0x4f53f0 (eax not reproduced)
    static void THISCALL MoveToTeamSpawn(World *self, void *part, Layer *layer, const char *name, const float *offset);   // 0x4f4950 (eax not reproduced)
    static void THISCALL MoveToSpawnHere(World *self, void *part, const char *name, const float *offset);   // 0x4f4ac0 (eax not reproduced)
    static void THISCALL MoveAllToSpawns(World *self, LayerArray *parts, LayerArray *names, LayerArray *offsets, LayerArray *layers);   // 0x4f4a40 (slot 83; eax not reproduced)
    static void *THISCALL SetSyncRequested(World *self, DWORD player);                     // 0x4f3410 (slot 49)
    static void THISCALL RequestSync(World *self, DWORD player);                           // 0x4f3430 (slot 50; eax not reproduced)
    static DWORD THISCALL ViewTarget(World *self, float *out, DWORD player);               // 0x4f5da0 (eax: y)
    static long double THISCALL Zoom(World *self, DWORD player);                           // 0x4f6960 (st(0))
    static DWORD THISCALL PlayerEntity(World *self, DWORD player);                         // 0x4f6750 (slot 89)
    static DWORD THISCALL LocalEntity(World *self);                                        // 0x4f6740 (slot 88)
    static int THISCALL PlayerViewRectAt(World *self, DWORD player, int *rect);            // 0x4f0710 (slot 57)
    static void THISCALL PlayerViewRect(World *self, DWORD player, int *rect);             // 0x4f3740 (slot 54; eax not reproduced)
    static DWORD THISCALL PlayerSees(World *self, DWORD player, const float *pos);         // 0x4f36e0 (slot 53, al)
    // Multiplayer broadcasts (to every player whose view sees the point; player 0 always)
    static void THISCALL BroadcastSimpleSound(World *self, DWORD sound);                   // 0x4f6070 (this unused; eax not reproduced)
    static void THISCALL BroadcastExplosionAt(World *self, DWORD a1, DWORD a2, void *part, DWORD a4, DWORD a5);   // 0x4f60e0 (eax not reproduced)
    static void THISCALL BroadcastExplosionPoint(World *self, DWORD a1, DWORD a2, const float *pos, DWORD a4, DWORD a5, DWORD a6);   // 0x4f61c0
    static void THISCALL BroadcastExplosion(World *self, DWORD a1, DWORD a2, DWORD x, DWORD y, DWORD a5, DWORD a6, DWORD a7);   // 0x4f6280
    static void THISCALL RegisterReaders(World *self, void *server_packet, void *client_packet);   // 0x4f6a90 (slot 106; eax not reproduced)
    // Small slots
    static void *THISCALL GameType(World *self);                                           // 0x4f0220 (slot 1)
    static DWORD THISCALL Slot60(World *self);                                             // 0x4f0a20 (slot 60: tail call of slot 30)
    static DWORD THISCALL Slot61(World *self);                                             // 0x4f0a30 (slot 61, al)
    static DWORD THISCALL Slot67(World *self, DWORD, DWORD, DWORD, DWORD);                 // 0x4f0a50 (slot 67, al = 0)
    static DWORD THISCALL Saving(World *self);                                             // 0x4f0a60 (slot 25, al)
    static long double THISCALL One(World *self);                                          // 0x4f0aa0 (slot 100, st(0))
    static DWORD THISCALL Zero4(World *self, DWORD);                                       // 0x4f0ab0 (slots 40, 71, 73)
    static DWORD THISCALL Slot9(World *self, DWORD a);                                     // 0x4f25a0 (slot 9)
    static DWORD THISCALL Slot51(World *self, void *obj, DWORD a);                         // 0x4f25b0 (slot 51)
    static long double THISCALL RunningTime(World *self);                                  // 0x4f2600 (slot 13, st(0))
    static long double THISCALL TotalTime(World *self);                                    // 0x4f2610 (slot 14, st(0))
    static DWORD THISCALL Slot95(World *self, DWORD a);                                    // 0x4f27b0 (slot 95)
    static DWORD *THISCALL Slot99(World *self, DWORD *out);                                // 0x4f27e0 (slot 99)
    static DWORD THISCALL Slot38(World *self);                                             // 0x4f3170 (slot 38)
    static DWORD THISCALL KeyEscape(World *self, DWORD key);                               // 0x4f31c0 (slot 10, al)
    static long double THISCALL Slot62(World *self);                                       // 0x4f39f0 (slot 62, st(0))
    static long double THISCALL Slot63(World *self);                                       // 0x4f3a00 (slot 63, st(0))
    static long double THISCALL Slot64(World *self);                                       // 0x4f3a10 (slot 64, st(0))
    static long double THISCALL Slot65(World *self);                                       // 0x4f3a20 (slot 65, st(0))
    static long double THISCALL Slot66(World *self);                                       // 0x4f3a30 (slot 66: tail call of slot 62)
    static DWORD THISCALL Slot15(World *self, DWORD a);                                    // 0x4f6040 (slot 15)
};
#pragma pack(pop)
static_assert(sizeof(World) == 0x338);
static_assert(__builtin_offsetof(World, layers) == 0x2b8 && __builtin_offsetof(World, mission_time) == 0x324);

// The game's lagged xor generator (0x7124c0: +4 count, +0x10/+0x14 indices through the successor table
// 0x620004, +0x18 the table)
DWORD THISCALL Random_Next(BYTE *gen);                                                     // 0x4f50d0
// Deletes every element of a pointer array from the last (slot 1 with 1) and empties it
void THISCALL PtrArray_DeleteAll(LayerArray *a);                                           // 0x4f6b40 (eax not reproduced)

constexpr DWORD VT_WORLD = 0x5f8fc0, VT_ZAX_WORLD = 0x602dc8, VT_CHARACTER_HOLDER = 0x5f8ef0,
                VT_CHARACTER_ARRAY = 0x5f8f4c, VT_CHARACTER_ARRAY_BASE = 0x5f8f58, VT_LAYER_LIST = 0x5f7b88,
                VT_LAYER_LIST_BASE = 0x5f7b94, VT_SAVED_CHARACTERS = 0x5f8b8c, VT_SAVED_CHARACTERS_BASE = 0x5f8b98;
constexpr DWORD G_WORLD = 0x6c2080;
constexpr DWORD LOADING_FILE = 0x6c2904;    // the layer file being loaded (0x4f43f0): layer, path, name, flag
constexpr DWORD LOADING_SCREEN = 0x6c28fc;  // byte: the loading screen is up
constexpr DWORD LOADING_STEPS = 0x6c2900;   // the read hook's step counter
constexpr DWORD UPDATE_TIME = 0x6c28f8;     // float: time a paused multiplayer game has gone on (0x4f3560)
constexpr DWORD VIEW_LIST = 0x6c26f0;       // the function-local static view list of 0x4f37e0 (guard 0x6c2780)
constexpr DWORD LAST_FILE = 0x713c14;       // ZString g_LastFileOpened
constexpr DWORD SPAWN_RANDOM = 0x712cc0;    // the random generator of the spawn point choice

}  // namespace zax
