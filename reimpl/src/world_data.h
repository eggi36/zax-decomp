// Data classes of the world: CLevelOfDifficulty (0x4fd710-0x4fdfe0), CPlasmaTileMap's own Create,
// CreateArray, RegisterFields, class getter and deleting destructor (0x53f290-0x53f3e0) and the slots
// of the texture list the tile map keeps (0x4b45e0-0x4b4880, 0x4b46f0, 0x53f590).
//
// CLevelOfDifficulty (0x28, vtable 0x5f973c, class 0x6c2b48, version 3, parent CObject, alternative class
// name "CLevelOfDificulty" as the files spell it) [confirmed:
// Create, RegisterFields, docs/fields.csv]: +8 "Human Readable Name" (a Local.txt entry, 0x500740: two
// ZStrings; default "This Value Has Not Yet Been Set"; its descriptor is also kept in 0x6c2b44), then
// six floats in "Fraction" units (default 1, 0 to 3; converter 0x6be348, shown in percent 0x60f5d8):
// +0x10 "Enemy Damage Percent", +0x14 "Player Damage Percent", +0x18 "Weapon Drain Percent", +0x1c
// "Weapon Get Pickup Percent", +0x20 "Shield Drain Percent", +0x24 "Shield Get Pickup Percent". Slot
// 18 (after reading): a name still at the default gets the file name of the object's entry in the
// difficulty list 0x6c2a00.
//
// CPlasmaTileMap (0x2d48, vtable 0x5fcd98 with 23 slots, class 0x7110d8, version 3, no parent): the
// Plasma Ground of a level. +0x1064 its texture list (0x160 bytes, vtable 0x5fccf4, built by the
// constructor 0x53f490 through 0x4b40f0), +0x1068 "Blending" (float, default 0.25). Members +8, +0x2c6c
// and +0x2cac are frames (rendering); the constructor and destructor (0x53f490, 0x53f6d0) are not
// reimplemented. docs/classes.csv gives the class the texture list's vtable 0x5fccf4, so
// docs/functions.csv names the texture list's slots "CPlasmaTileMap::vf<n>".
//
// The texture list (a definition list of definitions.h with 0x20 more bytes: +0x140 an object of
// 0x4b1cc0/0x4b1d20, +0x154 the owner given to 0x4b40f0, +0x158 a byte; base vtable 0x5f4a38): slot 40
// clears the entries without deleting the textures (the objects are shared), 35 the file of a texture,
// 39 "<folder>/<name>.TGA", 0 the deleting destructor (0x53f590; 0x4b46f0 is the base list's, 0x140
// bytes, which deletes the entries' objects).
//
// Return values are what the original leaves in eax; documented where that is not reproduced.
#pragma once
#include "definitions.h"

namespace zax {

struct LevelOfDifficulty : CObjectBase {
    ZString name;       // 0x08 the Local.txt entry: key and text
    ZString text;       // 0x0c
    float percent[6];   // 0x10

    static LevelOfDifficulty *THISCALL DeletingDtor(LevelOfDifficulty *self, DWORD flags);   // 0x4fd810 (slot 1)
    static DWORD THISCALL AfterRead(LevelOfDifficulty *self, DWORD arg);                      // 0x4fdf70 (slot 18)
};
static_assert(sizeof(LevelOfDifficulty) == 0x28);

struct TextureList : ZDefinitionList {
    BYTE extra[0x20];   // 0x140

    static TextureList *THISCALL BaseDeletingDtor(TextureList *self, DWORD flags);   // 0x4b46f0 (slot 0 of 0x5f4a38)
    static TextureList *THISCALL DeletingDtor(TextureList *self, DWORD flags);       // 0x53f590 (slot 0 of 0x5fccf4)
    static DWORD THISCALL ClearEntries(TextureList *self);                           // 0x4b45e0 (slot 40)
    static DWORD THISCALL FileOfObject(TextureList *self, void *object, ZString *out);   // 0x4b4820 (slot 35; al)
    static DWORD THISCALL EntryPath(TextureList *self, DWORD i, ZString *out);       // 0x4b4880 (slot 39)
};
static_assert(sizeof(TextureList) == 0x160);

// The base list's destructor body (the entries' objects deleted, the record list emptied, the monitor
// deleted, the aliases destroyed); shared with plasma_tile_map.cpp
void texture_list_destroy_base(TextureList *l);

struct PlasmaTileMap {
    static void *THISCALL DeletingDtor(void *self, DWORD flags);   // 0x53f310 (slot 1)
};

constexpr DWORD VT_LEVEL_OF_DIFFICULTY = 0x5f973c, CLASS_LEVEL_OF_DIFFICULTY = 0x6c2b48, LOD_NAME_FIELD = 0x6c2b44;
constexpr DWORD DIFFICULTY_LIST = 0x6c2a00;
constexpr DWORD VT_PLASMA_TILE_MAP = 0x5fcd98, CLASS_PLASMA_TILE_MAP = 0x7110d8;
constexpr DWORD VT_PLASMA_TEXTURE_LIST = 0x5fccf4, VT_TEXTURE_LIST_BASE = 0x5f4a38;

}  // namespace zax
