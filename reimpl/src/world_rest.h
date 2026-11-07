// The rest of the world class of Zax.exe (base vtable 0x5f8fc0, see world_class.h) and of CZaxWorld's
// StartNewGame: the game data loading (slot 2), the multiplayer frame, the zoom and the view target,
// loading a save game, the fixups after loading, the explosion spawns, the definition preloads and list
// loaders, the Properties.txt path of a sequence; plus the small World functions around it: the layer's
// point query over the draw queues and its ambient sound layer, the collide checker's slots and filters,
// the layer part's SetLayer, two vector helpers, a resource handle constructor and two class getters.
//
// World members used here besides those of world_class.h [confirmed: code]:
//   +0x2e4 the view rectangle, which 0x4f6980 sets and the zoom divides: the zoom frame (+0x2a4) gets
//   the rectangle's size times the zoom (0x4aa550, __ftol)
// Player record (see world_class.h): +0x3c the player's packet (NetPacket, 0x520f50 CanRead, 0x5200d0
// ReadBlock, 0x520750 Reset), +0x39 "send the view rectangle", +0xc8/+0xcc the view target, +0xd0/+0xd4
// the smoothed target, +0xd8 the zoom, +0xe0 the zoom keys (bit 0 out, bit 1 in, bit 2 reset).
// CZaxWorld (0x598000-0x5a4fff): +0x33c the swapped layer file name table (0x58a030 Clear), +0x34c the
// game statistics (slot 19, 0x5bb780), +0x354 COtherMapList (0x41f660 Clear), +0x58c an index into the
// definition list 0x6c02e8 (-1: none).
//
// The zoom [confirmed: code]: a zoom key steps the player's zoom by 0.01 between 0x618034 (lower limit)
// and 0x618038 (upper limit); without a key the zoom drifts back to 1 (slower above 1: (upper - 1) * 0.25
// + 1, below 1: (1 - lower) * 0x5f14e8 + lower, each step dt * 0x5f14ec) and snaps to 1 when it crosses
// it. 0x4f6770 limits the zoom so the zoomed view stays inside the tile map (width and height rounded
// down to whole 128-pixel tiles), and only changes anything when the new zoom differs by more than 0.0001:
// the zoom frame is resized, the view target kept at the centre (0x4f5dd0) and the frame rebuilt
// (0x4f2800). 0x4f5dd0 sets a player's view target: mode 1 smooths it (unless the game is paused, slots
// 30/31) by k = 1 - 1 / (1 + d^2 * 0x5f9180), mode 1 and 2 scroll the view toward the smoothed target
// (0x4ed6e0 with the view margins of the rules), mode 0 centres the view on it and clamps it (0x4ed7f0);
// player 0's view is then set (0x4ed8f0). Player -1 means every player.
//
// Floating point follows the original instruction by instruction (x87.h).
//
// Elsewhere (left out of this module in section 7, reimplemented since): CZaxWorld's main menu 0x59bab0 (misc_c5.cpp;
// the launcher redirects its call 0x59bace), the drawing and the loading screen of the class, the loaders
// 0x4d7920/0x4d7c40 that slot 2 calls (inventory range, inventory_defs.cpp) and the ambient sound object functions
// 0x4ee100 (Sound, ambient_sound.cpp).
//
// Return values are what the original leaves in eax where callers can see it; "al" means only the low
// byte is defined, "eax not reproduced" that callers use the function as void.
#pragma once
#include "world_class.h"

namespace zax {

struct WorldRest {
    // The world class (this = the world)
    static void THISCALL LoadGameData(World *self, void *app);                              // 0x4f13a0 (slot 2; eax not reproduced)
    static void THISCALL MultiplayerFrame(World *self, DWORD a1, DWORD draw_views);        // 0x4f0230 (slot 56; eax not reproduced)
    static DWORD __fastcall PropertiesPath(void *object, void *out);                       // 0x4f0ac0 (registered in 0x62e6a8)
    static void THISCALL PreloadDefinitions(World *self);                                  // 0x4f10f0 (slot 45; eax not reproduced)
    static void THISCALL ZoomKeys(World *self, DWORD dt, DWORD player);                    // 0x4f3200 (slot 52; eax not reproduced)
    static void THISCALL CurrentLevelPath(World *self, void *out);                         // 0x4f3ac0 (eax not reproduced)
    static DWORD THISCALL ReadSaveGame(World *self, void *stream, DWORD binary);           // 0x4f3e70 (slot 74, al)
    static void THISCALL ReloadResources(World *self, DWORD kind, DWORD keep_character);   // 0x4f4190 (slot 27; eax not reproduced)
    static void THISCALL FixPart(World *self, Layer *layer, void *part);                   // 0x4f5660 (slot 87; eax not reproduced)
    static void THISCALL FixLegacyData(World *self, Layer *layer);                         // 0x4f5c80 (eax not reproduced)
    static void THISCALL SetViewTarget(World *self, DWORD x, DWORD y, int mode, DWORD player);   // 0x4f5dd0 (eax not reproduced)
    static void THISCALL SpawnExplosion(World *self, Layer *layer, DWORD model, DWORD x, DWORD y, DWORD a5, DWORD sound,
                                        DWORD a7);                                         // 0x4f6320 (eax not reproduced)
    static void *THISCALL SpawnExplosionEntity(World *self, Layer *layer, DWORD model, DWORD x, DWORD y, DWORD a5,
                                               DWORD add, DWORD a7, DWORD with_ai, DWORD a9);   // 0x4f6530
    static void THISCALL SetZoom(World *self, DWORD zoom, DWORD player);                   // 0x4f6770 (eax not reproduced)
    static void THISCALL SetViewRect(World *self, const int *rect);                        // 0x4f6980 (eax not reproduced)
    // The definition list loaders (this = the list): register, empty, list the folder's files
    static DWORD THISCALL LoadStyList(void *list, const char *name, const char *folder, DWORD flags);      // 0x4f6ba0
    static DWORD THISCALL LoadModelList(void *list, const char *name, const char *folder, DWORD flags);    // 0x4f6ea0
    static DWORD THISCALL LoadRdsList(void *list, const char *name, const char *folder, DWORD flags);      // 0x4f71a0
    static DWORD THISCALL LoadTxtList(void *list, const char *name, const char *folder, DWORD flags);      // 0x4f74a0
    // The same three times in CZaxWorld's range 0x5a3650-0x5a3c50: files with the extension, else
    // the "property" files of "Cache/<folder>"
    static DWORD THISCALL LoadQuipList(void *list, const char *name, const char *folder, DWORD flags);     // 0x5a3650 ("Quips")
    static DWORD THISCALL LoadCannedList(void *list, const char *name, const char *folder, DWORD flags);   // 0x5a3950 ("can")
    static DWORD THISCALL LoadDifficultyList(void *list, const char *name, const char *folder, DWORD flags);   // 0x5a3c50 ("DIFICULTY")
    // CZaxWorld
    static void THISCALL ZaxStartNewGame(World *self, DWORD a1, const char *map, const char *spawn, DWORD a4);   // 0x59b7f0 (slot 79; eax not reproduced)

    // The layer
    static void *THISCALL PartAt(Layer *self, const int *point, DWORD queues, CClassInfo *skip, DWORD a4, DWORD a5,
                                 DWORD a6);                                                // 0x4ed350
    static void THISCALL SetSoundLayer(Layer *self);                                       // 0x4ed960 (eax not reproduced)
    static DWORD THISCALL SweepAccepts(void *sweep, void *mover, void *part);              // 0x4e96b0 (al = 1)
    static CClassInfo *__cdecl LayerClassInfo();                                           // 0x4e9920
    static void THISCALL SetLayer(void *part, void *layer);                                // 0x4fc8f0 (eax not reproduced)
    // The collide checker (this = the checker)
    static DWORD THISCALL IsBlocked(void *checker, void *mover, const int *from, const int *to);   // 0x535dc0 (slot 23, al)
    static long double THISCALL Cost(void *checker, void *mover, const int *from, const int *to);  // 0x535eb0 (slot 24, st(0))
    static DWORD THISCALL CanStand(void *checker, void *mover, const float *pos);          // 0x536030 (slot 25, al)
    static DWORD __fastcall IgnoreUnnamed(void *, void *part);                             // 0x535e70 (sweep filter, al)
    static DWORD __fastcall IgnoreNot49a930(void *, void *part);                           // 0x536010 (sweep filter, al)
    // Small helpers
    static void THISCALL Normalize(float *v);                                              // 0x548430 (eax not reproduced)
    static const float *THISCALL AddVectors(float *out, const float *a, const float *b);   // 0x548470 (eax: a)
    static void *THISCALL ResourceHandleCtor(DWORD *handle);                               // 0x518260
    static CClassInfo *__cdecl FreeRangePolyClassInfo();                                   // 0x4b5c60
};

constexpr DWORD ZOOM_MIN = 0x618034, ZOOM_MAX = 0x618038;   // the player's zoom range
constexpr DWORD DRAW_QUEUE_GROUND = 0x6c1bc0, DRAW_QUEUE_OBJECTS = 0x6c1ca0;
constexpr DWORD MODEL_LIST = 0x6cfdd8;         // the CModel definition list
constexpr DWORD DEATH_BEHAVIORS = 0x6bd8b0;    // the definition list "Death Behaviors" (0x4784e0)
constexpr DWORD SEQUENCE_LIST = 0x6c2788;      // "Sequences" (loaded by 0x4d7c40 in 0x4f13a0)
constexpr DWORD ANIMATED_TEXTURE_LIST = 0x6c1ee8;   // "Animated Textures" (0x4d7c40)
constexpr DWORD GAME_TYPE_LIST = 0x6c02e8;     // "Game Types" (0x5a01ec)

}  // namespace zax
