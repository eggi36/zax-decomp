// The layer of a level and what belongs to it: CLayerSaveData (the level's data, 0x4e5210-0x4e7bf0),
// CLayerCustomSaveData's class getter and destructor, CZaxLayerSaveData (0x5b9c00-0x5b9ef0), CLayer's
// object functions, loading and saving a layer and its .way (0x4e99a0-0x4ec520, 0x4eca80), the layer's
// part bookkeeping and grid forwarders (0x4eb6f0-0x4ed690), small rectangle and array helpers
// (0x4ee7f0-0x4eeab0) and CLayerHolder (0x4efa20-0x4f0160).
//
// CLayerSaveData (0x190 bytes, vtable 0x5f8ae8, class 0x6c1cf0, version 7, no parent; the text of a
// .zax level file) [confirmed: constructor 0x4e6cb0, destructor 0x4e6e80, RegisterFields 0x4e5350,
// docs/fields.csv]:
//   +0x08/+0x09 two bytes (0 in the constructor)        +0x0c "Width", +0x10 "Height"
//   +0x14 the grid "Tree List" (CSortList2D of sort_list.h, 0x28 bytes; +0x34 = its owner, the CLayer;
//         +0x38 = its next id, set to 1 before reading)  +0x3c pointer to the grid
//   +0x40 "Plasma Ground" (owned CPlasmaTileMap, created by the constructor; +0x2d44 the CLayer)
//   +0x44 "View Position" (float point), +0x4c "Int View Position"
//   +0x54 "Game Type/Suggested Num Players", +0x58 "Game Type/Max Players" (0 "No Limit"),
//   +0x5c "Game Type/Default Game Type" (default "QuestMode"), +0x60 "Game Type/Allowable Games"
//   +0x64 "Action Queue" (embedded CActionQueue, 0x1c: constructor 0x41c6d0, vtable 0x5ed000, its
//         pointer array at +0x6c with vtable 0x5ed068; +0x7c = the CLayer)
//   +0x80 "Has Begin Been Called", +0x84 "Map Description" (a Local.txt entry: two ZStrings)
//   +0x8c "First Fire Action", +0x90 "First Load Action" (owned CAction)
//   +0x94 "Listener Settings/Environment Type" (setter 0x4e6c20), +0x98 ".../Environment Size" (setter
//         0x4e6c80), +0x9c ".../Rolloff" (default 5)
//   +0xa0 "Partial File Name" (ZString; the level's path below Levels/ without the extension)
//   +0xa4 "Team/Team Info" (pointer array, vtable 0x5f8b50 / 0x5f8b5c, of CTeamInfo 0x14 bytes)
//   +0xb4 "Ambient Sounds" (pointer array, vtable 0x5f8b44 / 0x5f8b68, of CAmbientSound)
//   +0xc4 "Shake Time", +0xc8 "Max Shake Time", +0xcc "Pixels To Shake" (embedded CLengthEnvelope,
//         0x24 bytes, vtables 0x5f3cbc then 0x5f3f2c), +0xf0 "Last Shake Delta"
//   +0xf8 "Path Data" (owned CWayPointMap, created on first use 0x4e70d0)
//   +0xfc a frame created on first use (0x4e7100; not a field), +0x100 "Fog Of War" (embedded CFrame,
//         0x40 bytes, sized to the screen by the constructor)
//   +0x140 "Active Paths" (owned CPathPointListListCache, created on first use 0x4e7180)
//   +0x144 "Monitor" (embedded CUpdateMonitorList of broadcast.h: +0x14c its pointer array, vtable
//         0x5f8ad0 / 0x5f8adc, data +0x150, count +0x154, capacity +0x158; entries CUpdateMonitorInfo
//         0x14 bytes, vtable 0x5f8a18: +8 the part, +0xc and +0x10 half width and height)
//   +0x15c "Custom Map Properties" (owned CLayerCustomSaveData), +0x160 "Music Mix" (owned
//         CMusicMixerData), +0x164 the CLayer (set by the layer; lines the save data does not know go to
//         the world's slot 67 first), +0x168 "Mini-Map/Disable Mini-Map", +0x16c/+0x170/+0x174/+0x178
//         the top, bottom, left and right clipping, +0x17c/+0x180/+0x184/+0x188 "Debug/MiniMap X Offset",
//         X Scale, Y Offset, Y Scale, +0x18c a byte
//
// CLayer (0x1d0, vtable 0x5f8bac, class 0x6c1d04, version 1, parent COffsetClippedProxyFrame, a frame
// of 0x3c bytes: rendering) [confirmed: Create 0x4e99b0, deleting destructor 0x4e9ad0]: +0x3c the save
// data ("Save Data", an embedded field), so the layer's +0x50 is the grid, +0x7c the tile map, +0xdc the
// partial file name, +0x100 shake time, +0x108 the shake envelope, +0x134 the way point map, +0x138
// the mini map frame, +0x180 the monitor list; +0x1cc/+0x1cd/+0x1ce three bytes (0 in the
// constructor). Constructing a layer sets the save data's back pointers and clears 0x6c1c70 (the layer
// whose ambient sounds play, 0x4ed960).
//
// CLayerHolder (0x2c, vtable 0x5f8f64, class 0x6c28f0, version 2, no parent; embedded in the world at
// +0x2b0) [confirmed]: +0x08 the layers (pointer array, vtable 0x5f7b88 / 0x5f7b94; not deleted with
// the holder), +0x18 "UniqueID" (made on first use from the generator 0x7124c4-0x7124d8), +0x1c "Total
// Playing Time Up To Last Save", +0x24 "Simulation Running Time" (doubles). Its own lines are
// "Current Active Layer" (the index at +0xdc of the world's slot 44 object's slot 23 (0) object) and one
// "Layer" per layer (the class name, then the save data; empty for a missing layer).
//
// CZaxLayerSaveData (0xc, vtable 0x604d2c, class 0x714440, version 1, parent CLayerCustomSaveData:
// abstract, 8 bytes, vtable 0x604d88): +8 "Compass Target" (ZString).
//
// Drawing is left out (Rendering): the layer's frame part (COffsetClippedProxyFrame), World_DrawView,
// the dirty tiles, the mini map (0x4ec600, 0x4ece20, 0x4ed150) and the frame functions the constructor
// calls (CFrame, 0x4ab5a0).
//
// Return values are what the original leaves in eax where callers can see it; functions used as void
// say "eax not reproduced", byte results say "al".
#pragma once
#include "exe_base.h"
#include "class_info.h"

namespace zax {

struct LayerArray {   // a pointer array: vtable, data, count, capacity
    DWORD vftable;
    void **data;
    DWORD count;
    DWORD capacity;
};

struct LayerSaveData {
    DWORD vftable;
    void *dynamic;
    BYTE byte8, byte9, pad0a[2];
    int width, height;          // 0x0c
    BYTE grid[0x28];            // 0x14 CSortList2D (+0x20 owner = 0x34, +0x24 next id = 0x38)
    void *grid_ptr;             // 0x3c
    BYTE *tile_map;             // 0x40
    float view[2];              // 0x44
    int int_view[2];            // 0x4c
    DWORD suggested_players, max_players, default_game, allowable_games;   // 0x54
    BYTE action_queue[0x1c];    // 0x64
    BYTE begun, pad81[3];       // 0x80
    ZString description[2];     // 0x84
    void *first_fire, *first_load;   // 0x8c
    DWORD environment_type;     // 0x94
    float environment_size;     // 0x98
    float rolloff;              // 0x9c
    ZString partial_name;       // 0xa0
    LayerArray teams;           // 0xa4
    LayerArray ambient;         // 0xb4
    float shake_time, max_shake_time;   // 0xc4
    BYTE pixels_to_shake[0x24]; // 0xcc
    float last_shake[2];        // 0xf0
    void *path_data;            // 0xf8
    void *frame_fc;             // 0xfc
    BYTE fog[0x40];             // 0x100
    void *active_paths;         // 0x140
    DWORD monitor_vftable;      // 0x144
    void *monitor_dynamic;
    LayerArray monitor;         // 0x14c
    void *custom;               // 0x15c
    void *music;                // 0x160
    void *layer;                // 0x164
    BYTE disable_mini_map, pad169[3];   // 0x168
    int clip[4];                // 0x16c
    float mini_map[4];          // 0x17c
    BYTE byte18c, pad18d[3];    // 0x18c

    static LayerSaveData *THISCALL Ctor(LayerSaveData *self);                        // 0x4e6cb0
    static DWORD THISCALL Dtor(LayerSaveData *self);                                 // 0x4e6e80
    static LayerSaveData *THISCALL DeletingDtor(LayerSaveData *self, DWORD flags);   // 0x4e5280 (slot 1)
    static DWORD THISCALL OnMessage(LayerSaveData *self, void *sender, const char *m);   // 0x4e6be0 (slot 0)
    static DWORD THISCALL Precache(LayerSaveData *self);                             // 0x4e6c00 (slot 7)
    static DWORD THISCALL WriteOwnLines(LayerSaveData *self, void *f);               // 0x4e6a50 (slot 9)
    static DWORD THISCALL ReadOwnLine(LayerSaveData *self, const char *name, char *value, void *r);   // 0x4e6a90 (slot 11)
    static DWORD THISCALL WriteBinary(LayerSaveData *self, void *f);                 // 0x4e69d0 (slot 13)
    static DWORD THISCALL ReadBinary(LayerSaveData *self, void *f);                  // 0x4e69f0 (slot 15)
    static DWORD THISCALL SetCustom(LayerSaveData *self, void *p);                   // 0x4e6990 (eax only with an old object)
    static DWORD THISCALL SetEnvironmentType(LayerSaveData *self, DWORD type);       // 0x4e6c20
    static float THISCALL SetEnvironmentSize(LayerSaveData *self, float size);       // 0x4e6c80 (st(0) only)
    static void *THISCALL PathData(LayerSaveData *self);                             // 0x4e70d0
    static void *THISCALL Frame(LayerSaveData *self);                                // 0x4e7100
    static void *THISCALL FogOfWar(LayerSaveData *self);                             // 0x4e7170
    static void *THISCALL ActivePaths(LayerSaveData *self);                          // 0x4e7180
    static DWORD THISCALL MonitorAdd(LayerSaveData *self, void *part, DWORD w, DWORD h);   // 0x4e72a0
    static DWORD THISCALL MonitorAddPart(LayerSaveData *self, void *part);           // 0x4e7330
    static DWORD THISCALL MonitorRemove(LayerSaveData *self, void *part);            // 0x4e73c0 (eax not reproduced)
    static void *THISCALL MusicMix(LayerSaveData *self);                             // 0x4e7bf0
};
static_assert(sizeof(LayerSaveData) == 0x190);
static_assert(offsetof(LayerSaveData, monitor) == 0x14c && offsetof(LayerSaveData, partial_name) == 0xa0);

struct Layer {
    BYTE frame[0x3c];           // COffsetClippedProxyFrame (+0x0c and +0x20 two embedded objects)
    LayerSaveData data;         // 0x3c
    BYTE byte1cc, byte1cd, byte1ce, pad1cf;

    static Layer *THISCALL Ctor(Layer *self);                                        // 0x4e9da0
    static Layer *THISCALL CtorFrame(Layer *self, void *frame);                      // 0x4e9e00
    static Layer *THISCALL DeletingDtor(Layer *self, DWORD flags);                   // 0x4e9ad0 (slot 1)
    static DWORD THISCALL Precache(Layer *self);                                     // 0x4e99a0 (slot 7)
    static void THISCALL SetShake(Layer *self, DWORD time, const BYTE *envelope);    // 0x4e9f00 (eax not reproduced)
    static void THISCALL ViewRect(Layer *self, const int *pos, int *rect);           // 0x4ea110 (eax not reproduced)
    static int THISCALL ViewRectAt(Layer *self, const float *pos, int *rect);        // 0x4ea660
    static const char *THISCALL FileName(Layer *self);                               // 0x4eb670
    static void THISCALL SetFileName(Layer *self, const char *name);                 // 0x4eb6b0 (eax not reproduced)
    static DWORD THISCALL WriteText(Layer *self, void *f);                           // 0x4ebe10 (al)
    static DWORD THISCALL SaveText(Layer *self, const char *path, DWORD mode);       // 0x4ebe30 (al)
    static DWORD THISCALL WriteBinary(Layer *self, void *f);                         // 0x4ebe90 (al)
    static DWORD THISCALL LoadBinary(Layer *self, void *f);                          // 0x4ebeb0 (al)
    static DWORD THISCALL LoadText(Layer *self, void *r);                            // 0x4ec090 (al)
    static void THISCALL SaveWay(Layer *self);                                       // 0x4ec520 (eax not reproduced)
    static void THISCALL LoadOrCalculateWay(Layer *self);                            // 0x4eca80 (eax not reproduced)
    static DWORD THISCALL AddPart(Layer *self, void *part);                          // 0x4eb6f0
    static void THISCALL RemovePart(Layer *self, void *part);                        // 0x4ebcb0 (eax not reproduced)
    static void THISCALL DeletePart(Layer *self, void *part);                        // 0x4ebd60 (eax not reproduced)
    static DWORD THISCALL MarkDied(Layer *self);                                     // 0x4ed5c0
    static DWORD THISCALL FindRect(Layer *self, void *out, const int *rect, DWORD kind, DWORD all, DWORD none, DWORD any);   // 0x4ed5e0
    static DWORD THISCALL AnyAccepts(Layer *self, void *arg, const int *rect, DWORD all, DWORD none, DWORD any);   // 0x4ed610 (al)
    static DWORD THISCALL QueryRadius(Layer *self, void *context, const float *point, DWORD radius, DWORD y_scale,
                                      DWORD all, DWORD none, DWORD any);            // 0x4ed690
};
static_assert(sizeof(Layer) == 0x1d0);

struct LayerHolder {
    DWORD vftable;
    void *dynamic;
    LayerArray layers;          // 0x08
    DWORD unique_id;            // 0x18
    DWORD playing_time[2];      // 0x1c (double)
    DWORD running_time[2];      // 0x24 (double)

    static LayerHolder *THISCALL DeletingDtor(LayerHolder *self, DWORD flags);       // 0x4efb20 (slot 1)
    static DWORD THISCALL UniqueId(LayerHolder *self);                               // 0x4efe20
    static void THISCALL AddPlayingTime(LayerHolder *self, double t);                // 0x4efe90 (eax not reproduced)
    static void THISCALL WriteOwnLines(LayerHolder *self, void *f);                  // 0x4efea0 (slot 9; eax not reproduced)
    static void THISCALL ReadOwnLine(LayerHolder *self, const char *name, const char *value, void *r);   // 0x4eff40 (slot 11; eax not reproduced)
    static void THISCALL WriteBinary(LayerHolder *self, void *f);                    // 0x4f00e0 (slot 13; eax not reproduced)
    static DWORD THISCALL ReadBinary(LayerHolder *self, void *f);                    // 0x4f0160 (slot 15; al)
};
static_assert(sizeof(LayerHolder) == 0x2c);

struct ZaxLayerSaveData {
    DWORD vftable;
    void *dynamic;
    ZString compass_target;     // 0x08

    static ZaxLayerSaveData *THISCALL Ctor(ZaxLayerSaveData *self);                  // 0x5b9ef0
    static ZaxLayerSaveData *THISCALL DeletingDtor(ZaxLayerSaveData *self, DWORD flags);   // 0x5b9d50 (slot 1)
    static ZaxLayerSaveData *THISCALL CustomDeletingDtor(ZaxLayerSaveData *self, DWORD flags);   // 0x5b9c50
};
static_assert(sizeof(ZaxLayerSaveData) == 0xc);

// Small helpers: a rectangle {x0, y0, x1, y1}, a float point, the pointer and 16-byte arrays
struct LayerRect {
    int r[4];
    static int THISCALL Set(LayerRect *self, int x0, int y0, int x1, int y1);        // 0x4ee7f0
    static int THISCALL Union(LayerRect *self, const int *other);                    // 0x4ee810
    static DWORD THISCALL Intersects(const LayerRect *self, const int *other);       // 0x4ee860 (al)
    static long double THISCALL Dot(const float *self, const float *other);          // 0x4ee8c0
    static void THISCALL RemoveAndDelete(LayerArray *self, DWORD i);                 // 0x4eea50 (eax not reproduced)
    static void THISCALL Reserve16(LayerArray *self, DWORD n);                       // 0x4eeab0 (eax not reproduced)
};

constexpr DWORD VT_LAYER_SAVE_DATA = 0x5f8ae8, VT_LAYER = 0x5f8bac, VT_LAYER_HOLDER = 0x5f8f64,
                VT_ZAX_LAYER_SAVE_DATA = 0x604d2c, VT_CUSTOM_SAVE_DATA = 0x604d88, VT_TEAM_INFO = 0x5f89bc,
                VT_MONITOR_INFO = 0x5f8a18, VT_MONITOR_LIST = 0x5f8a74;
constexpr DWORD CLASS_LAYER_SAVE_DATA = 0x6c1cf0, CLASS_LAYER = 0x6c1d04, CLASS_LAYER_HOLDER = 0x6c28f0,
                CLASS_ZAX_LAYER_SAVE_DATA = 0x714440, CLASS_CUSTOM_SAVE_DATA = 0x6c1cec;
constexpr DWORD SOUND_LAYER = 0x6c1c70;     // the layer whose ambient sounds play (0x4ed960)
constexpr DWORD LOADING_LAYER = 0x6c1cdc;   // the layer being loaded
constexpr DWORD DRAW_DIRECT = 0x6c1d08;     // byte of 0x4e9d80/0x4e9d90

}  // namespace zax
