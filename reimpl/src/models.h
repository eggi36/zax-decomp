// Models and the classes that point at them: CModel (0x515870-0x517510, a model header with its
// sequences), the list of the models (0x517570-0x517950, global 0x6cfdd8), the entity made from a model
// (Entity_CreateFromModel 0x5176f0) and the sequence sync of an entity's children (0x517e70),
// CSpawnableModel (0x4cef30-0x4cf2c0, with CSpawnableItem's deleting destructor 0x4cefd0), the map
// caching CSwappedLayerFilenameMapping and CSwappedLayerFilenameMappingTable (0x588860-0x58a030,
// 0x58b7c0, 0x58b7f0) and COtherMapList (0x41f290-0x41f7d0).
//
// CModel (0xc4 bytes, vtable 0x5facdc, class 0x6cfdd4, version 193, no parent) [confirmed: constructor
// 0x516a90, destructor 0x516c50, RegisterFields 0x5159b0, docs/fields.csv, zax-formats/docs/FORMATS.md 6]:
//   +0x08 "Activity/Activity" (pointer array, vtable 0x5fad38, data +0x0c, count +0x10, capacity +0x14;
//         elements CFlyweightLayerPartAI "Model Activity")
//   +0x18 "Description", +0x1c "Printable Name" (ZString)
//   +0x20 "Damage/Hit Points" (0 "Indestructible"), +0x24 "Damage/Collide Damage", +0x28 "Damage/Collide
//         Damage Type" (index into the damage types 0x6bd760, default "!None"), +0x2c "Damage/Closing
//         Damage" (default 10, alternative name "Closing Damage Per Second"), +0x30 "Damage/Explosion Radial
//         Damage"
//   +0x34 "Move/Bounce Factor" (default 1), +0x38 "Move/Bounce Friction" ("Friction"), +0x3c "Move/Min
//         Bounce Friction" ("Min Friction", 0.1), +0x40 "Move/Min Bounce" (0.1)
//   +0x44 "Debug/Sorting Y" (int; the constructor sets the bottom of the empty bounds polygon)
//   +0x48 the number of sequences, +0x4c their CRotationSequence objects (operator new, 0x6c bytes each from
//         the pool, constructor 0x55f7d0), +0x68 their names (new[] ZString, count in front), +0x6c the
//         model's path "Models/.../-<name>" (operator new; the counted text of the .mdl file)
//   +0x50 "Explosion" (index into the model list), +0x54 "Sound/Death Sound", +0x58 "Sound/Pain Sound"
//         (indexes into the sound manager 0x713708, default "Error.wav"), +0x5c "Sound/Surface Type"
//         (0x713868, "!None")
//   +0x60 "Move/Max Velocity" (500 pixels per second, "Velocity"), +0x64 "Move/Turn Speed" (180 degrees
//         per second)
//   +0x70 "Seq./Default Sequence" (ZString, default "Idle"), +0x74 "Default Entity" (owned CEntity)
//   +0x78 "Bounds Poly" (embedded polygon, 0x28 bytes, constructor 0x545cc0, destructor 0x545cf0; its
//         bounding box at +0xc..+0x18; alternative name "PolyPoint"), +0xa0 a byte (0 in the constructor)
//   +0xa4 "Sound/Activate Sound", +0xa8 "Sound/Close Sound" (sound indexes), +0xac "Also Render" (model
//         index), +0xb0 "Legacy Name" (ZString), +0xb4 a resource handle {vtable 0x5faea4, +4 the model}
//         (8 bytes; the base destructor writes the named-list vtable), +0xbc "Fidget Settings" (owned
//         CFidgetSettings), +0xc0 the file buffer kept after loading (FileBuf_Detach)
// The .mdl file is an object file: Object_ReadBinary reads the fields, then slot 15 the sequences
// (count, the path, per sequence a byte "present", its name and the CRotationSequence); slot 13 writes
// them the same way. The file name: "Cache/<folder>/<name>" + ".mdl" (8 bit), ".mdl16" (16 bit) or the
// dash path of ZString_SetDashPath with ".mdl24"/".mdl32", by the colour depth of a surface or of the
// display (0x55d030; 8 without one).
//
// Model list (a definition list of definitions.h, 0x140 bytes, vtable 0x5fad44, base vtable 0x5fadec
// written by its destructors; global 0x6cfdd8 built by the static initializer 0x517570, exit destructor
// 0x5175e0): records of 0x80 bytes, the loaded CModel at +0x78. Own slots: 0 the deleting destructor
// (0x517820), 34 the element class (0x517810, a jump to CModel's getter), 35 the file of an object
// (0x517950), 37 the model loaded from its file (0x5176a0; the base vtable's 0x5178d0 reads an object
// file with an empty extension), 41 the factory of the editor's preview (0x5176e0: returns 0x518230).
//
// CSpawnableModel (0x10, vtable 0x5f766c, class 0x6c0444, version 1, parent CSpawnableItem: abstract,
// 0xc bytes, vtable 0x5f76cc, class 0x6c0440; alternative class name "CGeneratorListItem", set by
// RegisterFields): +0xc "Model" (index into the model list). Slot 23 makes the entity of that model.
//
// CSwappedLayerFilenameMapping (0x18, vtable 0x6018ec, class 0x7139b8, version 1, no parent): +8 "Partial
// Layer Name" (char[0x100], operator new), +0xc "Current Temp File", +0x10 "Mission Temp File" (temp file
// names, operator new 0xff), +0x14 "Has Expired", +0x15 "Expired This Mission".
// CSwappedLayerFilenameMappingTable (0x50, vtable 0x601948, class 0x7139bc, version 1, no parent):
//   +0x08 "Layer Mapping Array" (pointer array, vtable 0x6019b0, data +0xc, count +0x10, capacity +0x14)
//   +0x18 a byte "busy" (set while the temp files are swapped; then the destructor keeps the files),
//   +0x19 a byte "mission start pending", +0x1c "Mission Marker Temp File"
//   +0x20 the layers to unload (pointer array, vtable 0x6019bc, data +0x24, count +0x28)
//   +0x30 the layers to flush (vtable 0x6019bc, data +0x34, count +0x38)
//   +0x40 "LRU LayerSaveData Array" (vtable 0x6019a4, data +0x44, count +0x48, capacity +0x4c; the save
//         data of the cached layers, a CLayer's +0x3c, least recently used first)
// The table maps a level's partial name to the temp file that holds its saved state: the current one
// (written during the mission) and the one of the last mission start. A layer's file is
// "Levels/<name>.zax" while there is no mapping (or the "Expired Error" level for an expired one).
//
// COtherMapList (0x18, vtable 0x5ed780, class 0x62e938, version 1, parent CObject): +8 "ActionList"
// (pool pointer array, vtable 0x5ed7dc / 0x5ed7e8, data +0xc, count +0x10, capacity +0x14) of
// COtherMapAction (+8 the action, +0xc the target map's name): actions waiting for another map.
//
// Left out: CModel's static field "Model AI" (0x516d10, 0x516d60, 0x516d70: the startup registration
// adds it to CAIBase), the editor's model preview (vtable 0x5fae90: 0x5179b0, 0x517a40, 0x517a80, drawing
// 0x517c60, 0x517f20, 0x518080, 0x518230-0x5182c0; user interface and rendering) and the jump 0x517810.
//
// Return values are what the original leaves in eax where callers can see it; byte results say "al".
#pragma once
#include "exe_base.h"
#include "class_info.h"

namespace zax {

struct ModelArray {   // a pointer array: vtable, data, count, capacity
    DWORD vftable;
    void **data;
    DWORD count;
    DWORD capacity;
};

struct ModelPolygon {   // the embedded polygon (polygon functions, not reimplemented here)
    DWORD d[10];
};

struct CModel {
    DWORD vftable;
    void *dynamic;
    ModelArray activity;          // 0x08
    ZString description;          // 0x18
    ZString printable_name;       // 0x1c
    float hit_points;             // 0x20
    float collide_damage;         // 0x24
    DWORD collide_damage_type;    // 0x28
    float closing_damage;         // 0x2c
    float explosion_damage;       // 0x30
    float bounce_factor;          // 0x34
    float bounce_friction;        // 0x38
    float min_bounce_friction;    // 0x3c
    float min_bounce;             // 0x40
    int sorting_y;                // 0x44
    DWORD num_sequences;          // 0x48
    BYTE **sequences;             // 0x4c CRotationSequence *[]
    DWORD explosion;              // 0x50
    DWORD death_sound;            // 0x54
    DWORD pain_sound;             // 0x58
    DWORD surface_type;           // 0x5c
    float max_velocity;           // 0x60
    float turn_speed;             // 0x64
    ZString *sequence_names;      // 0x68 new[]
    char *path;                   // 0x6c
    ZString default_sequence;     // 0x70
    void *default_entity;         // 0x74
    ModelPolygon bounds;          // 0x78
    BYTE ba0;                     // 0xa0
    BYTE pad[3];
    DWORD activate_sound;         // 0xa4
    DWORD close_sound;            // 0xa8
    DWORD also_render;            // 0xac
    ZString legacy_name;          // 0xb0
    DWORD resource_vftable;       // 0xb4
    CModel *resource_model;       // 0xb8
    void *fidget;                 // 0xbc
    void *file_buffer;            // 0xc0

    CModel *THISCALL Ctor();                                              // 0x516a90
    void *THISCALL DeletingDtor(DWORD flags);                             // 0x515930 (slot 1)
    const char *THISCALL LegacyName();                                    // 0x516b50 (slot 6)
    void THISCALL Precache();                                             // 0x516cd0 (slot 7)
    DWORD THISCALL WriteOwn(void *stream);                                // 0x517160 (slot 13, eax not reproduced)
    DWORD THISCALL ReadOwn(void *stream);                                 // 0x516ff0 (slot 15, al)
    DWORD THISCALL Size();                                                // 0x516f80 (slot 21)
    void THISCALL Clear();                                                // 0x516b60
    void THISCALL Destroy();                                              // 0x516c50
    DWORD THISCALL Load(const char *folder, const char *name);           // 0x516eb0 (al)
    const char *THISCALL SequenceName(DWORD i);                           // 0x517200
    ZString *THISCALL SequenceNameString(DWORD i);                        // 0x517220
    int THISCALL FindSequence(const char *name);                          // 0x517240
    int THISCALL IndexOfSequence(const void *seq);                        // 0x517280
    DWORD THISCALL CollectSequences(ModelArray *out, const char *prefix, BYTE unused_only);   // 0x5172b0 (eax not reproduced)
    void *THISCALL DefaultSequence();                                     // 0x517360
    int THISCALL DefaultSequenceIndex();                                  // 0x5173b0
    void *THISCALL Sequence(DWORD i);                                     // 0x5173f0
    void *THISCALL ResolveSequence(const BYTE *seq, BYTE direct);         // 0x517410
    void *THISCALL ResolveSequenceAt(DWORD i, BYTE direct);               // 0x517460
    const char *THISCALL ShortName();                                     // 0x5174a0
    void *THISCALL Activity(DWORD i);                                     // 0x5174f0
    long double THISCALL TurnSpeedRadians();                             // 0x517510 (st(0))
};
static_assert(sizeof(CModel) == 0xc4);
static_assert(offsetof(CModel, default_entity) == 0x74);
static_assert(offsetof(CModel, legacy_name) == 0xb0);

struct ModelList;   // a ZDefinitionList (definitions.h)

struct ZModelList {
    BYTE raw[0x140];

    void *THISCALL DeletingDtor(DWORD flags);                                    // 0x517820 (slot 0)
    DWORD THISCALL FileOfObject(void *object, ZString *out);                     // 0x517950 (slot 35, al)
    void *THISCALL LoadModel(const char *folder, const char *name, void **out);  // 0x5176a0 (slot 37)
    void *THISCALL CreateFromFile(const char *folder, const char *name, void **out);   // 0x5178d0 (slot 37 of the base)
    DWORD THISCALL PreviewFactory();                                             // 0x5176e0 (slot 41)
    void *THISCALL CreateEntity(DWORD index, CClassInfo *cls, DWORD creator, DWORD a, DWORD b);   // 0x5176f0
};

struct CSpawnableModel {
    DWORD vftable;
    void *dynamic;
    DWORD unknown08;   // 0x08 CSpawnableItem's own dword (not written by the constructors)
    DWORD model;       // 0x0c

    void *THISCALL DeletingDtor(DWORD flags);    // 0x4cf0a0 (slot 1)
    void *THISCALL Spawn(DWORD creator, DWORD unused);   // 0x4cf2c0 (slot 23; the second argument is not used)
};
static_assert(sizeof(CSpawnableModel) == 0x10);

struct CSpawnableItem {
    void *THISCALL DeletingDtor(DWORD flags);    // 0x4cefd0 (slot 1)
};

struct CSwappedLayerFilenameMapping {
    DWORD vftable;
    void *dynamic;
    char *name;            // 0x08
    char *current_temp;    // 0x0c
    char *mission_temp;    // 0x10
    BYTE expired;          // 0x14
    BYTE expired_this_mission;   // 0x15
    BYTE pad[2];

    void *THISCALL DeletingDtor(DWORD flags);    // 0x588940 (slot 1)
};
static_assert(sizeof(CSwappedLayerFilenameMapping) == 0x18);

struct CSwappedLayerFilenameMappingTable {
    DWORD vftable;
    void *dynamic;
    ModelArray mappings;   // 0x08
    BYTE busy;             // 0x18
    BYTE mission_pending;  // 0x19
    BYTE pad[2];
    char *marker_temp;     // 0x1c
    ModelArray unload;     // 0x20
    ModelArray flush;      // 0x30
    ModelArray lru;        // 0x40

    void *THISCALL DeletingDtor(DWORD flags);                   // 0x588e10 (slot 1)
    DWORD THISCALL LoadPath(const char *name, char *out);       // 0x5892b0 (al)
    DWORD THISCALL SavePath(const char *name, char *out);       // 0x5893c0 (al)
    void THISCALL Expire(const char *name);                     // 0x589570
    void THISCALL Flush(const char *name);                      // 0x589790
    void THISCALL FlushAll();                                   // 0x589890
    DWORD THISCALL IsExpired(const char *name);                 // 0x5899b0 (al)
    void THISCALL Touch(const char *name);                      // 0x589a50
    void THISCALL Forget(const char *name);                     // 0x589af0
    void THISCALL Unexpire(const char *name);                   // 0x589b80
    void THISCALL RestartMission();                             // 0x589cc0
    DWORD THISCALL Update();                                    // 0x589db0 (eax not reproduced)
    void THISCALL Clear();                                      // 0x58a030
    static DWORD THISCALL LevelPath(void *self, const char *name, char *out);   // 0x589270 (al)
};
static_assert(sizeof(CSwappedLayerFilenameMappingTable) == 0x50);

struct COtherMapList {
    DWORD vftable;
    void *dynamic;
    ModelArray actions;    // 0x08

    void *THISCALL DeletingDtor(DWORD flags);                   // 0x41f360 (slot 1)
    void THISCALL Clear();                                      // 0x41f660
    void THISCALL RunFor(DWORD instigator, const char *map);    // 0x41f6c0
    int THISCALL CountFor(const char *map);                     // 0x41f780
    void THISCALL RemoveFor(const char *map);                   // 0x41f7d0
};
static_assert(sizeof(COtherMapList) == 0x18);

// 0x58b7c0, 0x58b7f0: a temp file name (char *): deleted (the file and the name), made (a new temp file)
struct TempFileName {
    char *name;
    void THISCALL Delete();   // 0x58b7c0
    void THISCALL Make();     // 0x58b7f0
};

constexpr DWORD MODEL_LIST = 0x6cfdd8, CLASS_MODEL = 0x6cfdd4;
constexpr DWORD MODEL_LIST_VTABLE = 0x5fad44, MODEL_LIST_BASE_VTABLE = 0x5fadec;
constexpr DWORD CLASS_SPAWNABLE_MODEL = 0x6c0444, CLASS_SPAWNABLE_ITEM = 0x6c0440;
constexpr DWORD CLASS_MAPPING = 0x7139b8, CLASS_MAPPING_TABLE = 0x7139bc, CLASS_OTHER_MAP_LIST = 0x62e938,
                CLASS_OTHER_MAP_ACTION = 0x62e934;

}  // namespace zax
