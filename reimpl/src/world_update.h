// The world update and object placement code of CLayer and CLayerSaveData (0x4e8810-0x4ebbc0): the
// dirty marks of the dynamic draw queue entries, the part queries around a part (blockers, the part in
// front, the swept test), placing a part near another one, the bounce of a moving part, the entity type
// fix after loading a save game, the camera shake, the per-frame update of the views, the view position
// from the network, and the multiplayer sound and position packets.
//
// Not here (Rendering, now in layer_draw.cpp): World_DrawDirtyTiles 0x4e8080, the debug outlines of the queue entries 0x4e8650,
// the layer's draw pass 0x4e8900 (grid query of the view, the parts' slot 23 fill both queues, the tile
// map 0x53ff90, the dirty tiles 0x4e8080, the queue frame 0x4810b0, the parts' slot 30), the tile
// drawing 0x4e8b60/0x4e8bd0/0x4e8ce0 (one 64 x 64 tile with the offset 0x4abaa0, the queue entries over
// it drawn through slot 1 into a temporary frame) and World_DrawView 0x4e7c00. The collision sweep
// 0x4e8d40/0x4e8e00 and the polygon functions 0x545620-0x547d60 belong to the polygon batch; the world
// update of the save data 0x4e74a0 (Core in docs/functions.csv; layer_update.cpp) is called through its exe address.
//
// Return values are what the original leaves in eax where callers can see it; "al" means only the low
// byte is defined, "eax not reproduced" that callers use the function as void.
#pragma once
#include "layer.h"

namespace zax {

// The motion of a bouncing part (0x38 bytes, vtable 0x5f8ba8, made by 0x545a00) [names: assumption]
struct BounceMotion {
    DWORD vftable;
    DWORD owner;          // 0x04
    float move[2];        // 0x08 this step's displacement (0x545b30 sets it, +0x10 direction, +0x18 length)
    float dir[2];         // 0x10
    float length;         // 0x18
    BYTE flag1c, flag1d, pad[2];
    float velocity[2];    // 0x20
    float friction;       // 0x28 multiplies the normal part that is taken away
    float bounce;         // 0x2c "Bounce >= 1.1f" is warned about
    float damping;        // 0x30 the remaining parts are multiplied with 1 - damping
    float min_speed;      // 0x34 parts below are cleared
};
static_assert(sizeof(BounceMotion) == 0x38);

// The result of the collision sweep 0x4e8e00 (0x24 bytes, 0x545c90 sets t = 1)
struct SweepHit {
    float t;              // fraction of the move that was free
    float delta[2];       // 0x04 the free part of the move
    float normal[2];      // 0x0c
    float point[2];       // 0x14
    float other[2];       // 0x1c
};

// What 0x4e91e0 writes for the caller: t, delta, normal, the two points and the velocity
struct BounceResult {
    float t;
    float delta[2];       // 0x04
    float normal[2];      // 0x0c
    float point[2];       // 0x14
    float velocity[2];    // 0x1c
};

struct WorldLayer {
    // Part queries (the grid of the layer, +0x50)
    static void *THISCALL FindBlocker(Layer *self, void *part, const float *at, CClassInfo *prefer, DWORD all, DWORD none);   // 0x4eb050
    static void *THISCALL FindBlockerHere(Layer *self, void *part, CClassInfo *prefer, DWORD all, DWORD none);   // 0x4eb1c0
    static void *THISCALL FindInFront(Layer *self, void *part, const float *from, float angle, float half_width, float distance,
                                      DWORD all, DWORD none);                              // 0x4eb200
    static DWORD THISCALL PlaceNear(Layer *self, void *part, void *reference, float max_distance);   // 0x4eb7b0 Entity_PlaceNear
    static DWORD THISCALL SurfaceAt(Layer *self, DWORD x, DWORD y);                        // 0x4e9e60
    // View and update
    static DWORD THISCALL ApplyShake(Layer *self, float *pos);                             // 0x4e9f70 (al)
    static void THISCALL Update(Layer *self, float dt);                                    // 0x4ea220 (eax not reproduced)
    static void THISCALL UpdateViews(Layer *self, LayerArray *views, float dt);            // 0x4ea350 (eax not reproduced)
    static void THISCALL DrawFrame(Layer *self, void *packet);                             // 0x4ea6e0 (eax not reproduced)
    // Multiplayer sounds and positions
    static void THISCALL SendSimpleSoundAt(Layer *self, DWORD sound, void *part);          // 0x4ea880 (eax not reproduced)
    static void THISCALL SendSimpleSound(Layer *self, DWORD sound, const float *pos);      // 0x4ea8c0 (eax not reproduced)
    static DWORD THISCALL SendSoundAt(Layer *self, DWORD sound, void *part, DWORD mode, DWORD keep, DWORD channel);   // 0x4eaa60 (al)
    // 0x4eaab0 (al): world_update.cpp's zax_layer_send_sound_entry, in the exe's frame (exe_frame.h)
    static void THISCALL PlaySoundHere(Layer *self, DWORD sound, const float *pos, DWORD mode, DWORD keep, DWORD channel);   // 0x4eaca0 (eax not reproduced)
    static DWORD THISCALL WritePosition(Layer *self, void *packet, const float *pos);      // 0x4eae90
    static BYTE *THISCALL ReadPosition(Layer *self, void *packet, float *pos, BYTE *ok);   // 0x4eaf80
};

struct WorldSaveData {
    static void THISCALL FixEntityTypes(LayerSaveData *self);                              // 0x4e97c0 (eax not reproduced)
    // 0x4e96c0 (al): world_update.cpp's zax_save_data_bounce_entry, in the exe's frame (exe_frame.h)
    static DWORD THISCALL SweepBounce(LayerSaveData *self, void *shape, float *pos, const BounceMotion *motion, BounceResult *out,
                                      void *hit_part, void *mover, DWORD a6, DWORD a7, DWORD a8);   // 0x4e91e0 (al)
};

// 0x4ebbc0: would the part hit something when it moved from a to b (both moved by the part's offset)?
// ecx = the part, edx = the layer (null: the part's layer); al
DWORD __fastcall Part_SweepBlocked(void *part, Layer *layer, float ax, float ay, float bx, float by);

// 0x4e8810: the rectangles of the queue entries whose byte +0x24 is set marked dirty (0x480fc0) in
// the frame (eax not reproduced)
void THISCALL World_MarkDynamicDirty(void *frame);

constexpr DWORD DRAW_QUEUE_GROUND = 0x6c1bc0, DRAW_QUEUE_OBJECTS = 0x6c1ca0;
constexpr DWORD NET_MODE = 0x6c1d0c;      // nonzero: no sound packets are sent
constexpr DWORD GAME_RANDOM = 0x7124c0;   // the game's random generator (0x55c550: a float in [lo, hi))
constexpr DWORD VIEW_RECTS = 0x6c1bd8;    // static 16-byte array of the view rectangles (guard 0x6c1be8 bit 1)
constexpr DWORD SOUND_MANAGER = 0x713708;

}  // namespace zax
