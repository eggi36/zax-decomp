// The per-frame update of a level (CLayerSaveData, 0x4e71b0-0x4e7b70): the first update that activates
// the parts and sizes the fog of war, the world update over the views and the monitored parts, the
// start of a level after loading, and the multiplayer flag and rectangle messages of the parts.
//
// The world update [confirmed: code] queries the grid (kind 5, all of the active flag 0x800000, in a
// multiplayer game 0x40800000) around every monitored part with a size (its int position, slots 52/53,
// widened by the monitor entry's half width and height), takes the monitored parts without a size as they
// are and adds every view rectangle; the grid then updates the parts found (CSortList2D_Process with dt)
// and the action queue runs (0x41c700).
//
// Return values are what the original leaves in eax where callers can see it; "eax not reproduced" that
// callers use the function as void.
#pragma once
#include "layer.h"

namespace zax {

struct LayerUpdate {
    static void THISCALL FirstUpdate(LayerSaveData *self);                                  // 0x4e71b0 (eax not reproduced)
    static void THISCALL WorldUpdate(LayerSaveData *self, LayerArray *views, float dt);     // 0x4e74a0 (eax not reproduced)
    static void THISCALL OnPartFlags(LayerSaveData *self, BYTE *part, DWORD old_flags, DWORD changed);   // 0x4e76e0 (eax not reproduced)
    static void THISCALL SendPartFlags(LayerSaveData *self, void *packet);                  // 0x4e77e0 (eax not reproduced)
    static void THISCALL Begin(LayerSaveData *self);                                        // 0x4e7870 (eax not reproduced)
    static void THISCALL WriteRect(LayerSaveData *self, void *packet, DWORD unused, const int *rect);   // 0x4e79b0 (eax not reproduced)
};

// Deletes the entries of the static pointer array 0x6c1c30 (vtable 0x5f8b8c, data 0x6c1c34, count 0x6c1c38)
void __cdecl StaticPartArray_DeleteAll();                                                  // 0x4e7b70 (eax not reproduced)

constexpr DWORD STATIC_PART_ARRAY = 0x6c1c30;

}  // namespace zax
