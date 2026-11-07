// Small object classes of Zax.exe and the list of the canned objects:
//   CCannedObject (0x468230-0x4683f0) with the canned object list (0x468000-0x468150, 0x468b30-0x468da0;
//     the function-local static 0x6bcfa8) and the "Use" names (0x468200, 0x468220; ZEnumTable 0x6bd0f0)
//   CObjectHolder, CObjectCollection (0x5238c0-0x523c30) and the setter 0x523e20 of the resource lists
//   CSphereOfZinOptions (0x582520-0x5826e0), CSphereOfZinData (0x5841c0-0x584430)
//   CWaterWaveFormData (0x5969f0-0x596b70) with its three field setters (0x596d20-0x596de0)
//   CMiniMapOverlay (0x507070-0x507220)
//
// Every class has the usual set (see definitions.h): the class getter (slot 2, some with a copy for
// other callers), Create (pool), CreateArray (operator new, count in front), the deleting destructor
// (slot 1) and RegisterFields. Layouts [confirmed: constructors, RegisterFields, docs/fields.csv, game
// data]:
//   CCannedObject (0x10, vtable 0x5ef478, class 0x6bd10c, version 1, parent CObject): +8 "Object" (an
//     owned CObject: the action of a .can file), +0xc "Use" (one of "Local Copy" 0, "Shared Global
//     Instance" 1; a typed resource field over the names 0x6bd0f0; all 7 files say "Shared Global
//     Instance")
//   CObjectHolder (0xc, vtable 0x5fb788, class 0x6d013c, version 1, no parent): +8 "Object" (an owned
//     CLayerPart)
//   CObjectCollection (0x10, vtable 0x5fb7e4, class 0x6d0140, version 1, no parent): "Num Objects", a
//     new[] array of CObjectHolder (+8 the data, +0xc the count)
//   CSphereOfZinOptions (0x10, vtable 0x601178, class 0x713800, version 0, parent CObject): +8 "Evil
//     Unit Lenght" (sic; float, units "Pixels^2" shown as "Pixels" through the converter 0x6be350,
//     default 40000, 1 to 1e8; Rules.txt gives 102400), +0xc "Apply Ceiling Prematurely" (byte, default
//     1, help text)
//   CSphereOfZinData (0x18, vtable 0x601318, class 0x713818, version 0, parent CObject): +8 an embedded
//     array object {vtable 0x601374, +0xc data, +0x10 count, +0x14 capacity}: "Trigger List", a new[]
//     array of CThresholdTrigger (deleted through slot 1 with 3)
//   CWaterWaveFormData (0x218, vtable 0x6028cc, class 0x713dc4, version 1, no parent): +8 "Amplitude"
//     (float, default 2, 0 to 30, step 0.1), +0xc "Pixels Per Second" (float, default 15, -100 to
//     100), +0x10 "Num Waves" (int, default 4, 0 to 16), +0x14 128 ints: the wave table (see Set),
//     +0x214 a dword the class's own code never writes
//   CMiniMapOverlay (0x14, vtable 0x5f9e3c, class 0x6c2dbc, version 1, no parent): +8 "MiniMap Overlay Frame
//     Name" (ZString, default "MiniMapOverlayFrame"), +0xc "Travel Distance" (int, units "Pixels", default
//     70), +0x10 "Height Offset" (int, units "Pixels", default 0); no level or resource file uses the class
//     [assumption: a mini map frame of the world's user interface]
//
// Canned object list (a definition list of definitions.h, 0x140 bytes; vtable 0x5ef330, base vtable
// 0x5ef3d4 written by its destructors): the global 0x6bcfa8 is a function-local static of 0x468000
// (guard bit 0 of 0x6bd108, exit destructor 0x468090); 0x5a3950 (world_rest.cpp, outside the
// range) fills it from the .can files of "Resources/Canned Objects". Own slots: 0 the deleting
// destructor, 34 the element class, 35 the file of an object, 37 an entry from its file, 39 the path
// of entry i (all with the extension "can"), 40 (shared by all definition lists) empties the records.
//
// Return values are what the original leaves in eax; documented where that is not reproduced.
#pragma once
#include "definitions.h"

namespace zax {

struct CCannedObject : CObjectBase {
    void *object;   // 0x08 owned
    DWORD use;      // 0x0c 0 "Local Copy", 1 "Shared Global Instance"

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x468310 (slot 1)
};
static_assert(sizeof(CCannedObject) == 0x10);

struct CObjectHolder : CObjectBase {
    void *object;   // 0x08 owned CLayerPart

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x523950 (slot 1)
};
static_assert(sizeof(CObjectHolder) == 0xc);

struct CObjectCollection : CObjectBase {
    CObjectHolder *objects;   // 0x08 new[]
    DWORD num_objects;        // 0x0c

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x523b50 (slot 1)
};
static_assert(sizeof(CObjectCollection) == 0x10);

struct CSphereOfZinOptions : CObjectBase {
    float evil_unit_length;   // 0x08
    BYTE apply_ceiling;       // 0x0c
    BYTE pad[3];

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x582600 (slot 1)
};
static_assert(sizeof(CSphereOfZinOptions) == 0x10);

struct CSphereOfZinData : CObjectBase {
    DWORD triggers_vftable;   // 0x08 0x601374
    void *triggers;           // 0x0c new[] array of CThresholdTrigger
    DWORD num_triggers;       // 0x10
    DWORD capacity;           // 0x14

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x5842e0 (slot 1)
};
static_assert(sizeof(CSphereOfZinData) == 0x18);

struct CWaterWaveFormData : CObjectBase {
    float amplitude;    // 0x08
    float speed;        // 0x0c "Pixels Per Second"
    int num_waves;      // 0x10
    int table[128];     // 0x14
    DWORD unknown214;   // 0x214 not written by the class's own code

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x596ad0 (slot 1)
    float THISCALL SetAmplitude(DWORD v);       // 0x596d20
    float THISCALL SetSpeed(DWORD v);           // 0x596d80
    DWORD THISCALL SetNumWaves(DWORD v);        // 0x596de0
};
static_assert(sizeof(CWaterWaveFormData) == 0x218);

struct CMiniMapOverlay : CObjectBase {
    ZString frame_name;    // 0x08 "MiniMap Overlay Frame Name"
    int travel_distance;   // 0x0c
    int height_offset;     // 0x10

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x507120 (slot 1)
};
static_assert(sizeof(CMiniMapOverlay) == 0x14);

struct ZCannedObjectList : ZDefinitionList {
    ZCannedObjectList *THISCALL Ctor();                                         // 0x468ba0
    ZCannedObjectList *THISCALL DeletingDtor(DWORD flags);                      // 0x468150 (slot 0)
    DWORD THISCALL FileOfObject(void *object, ZString *out);                    // 0x468c70 (slot 35)
    void *THISCALL CreateFromFile(const char *dir, const char *name, void **out);   // 0x468bf0 (slot 37)
    DWORD THISCALL EntryPath(DWORD i, ZString *out);                            // 0x468d20 (slot 39)
    DWORD THISCALL ClearEntries();                                              // 0x468da0 (slot 40)
    DWORD THISCALL SetRegistryIndex(DWORD i);                                   // 0x523e20
};

struct ZResourceAliasesCtor {
    static ZResourceAliases *THISCALL Ctor(ZResourceAliases *self);   // 0x468b30
};

constexpr DWORD CANNED_LIST = 0x6bcfa8, CANNED_LIST_GUARD = 0x6bd108;
constexpr DWORD CANNED_LIST_VTABLE = 0x5ef330, CANNED_LIST_BASE_VTABLE = 0x5ef3d4;
constexpr DWORD CANNED_USE_NAMES = 0x6bd0f0;   // ZEnumTable over the records 0x60f238
constexpr DWORD TEXT_CAN = 0x60f2c0;           // "can"

}  // namespace zax
