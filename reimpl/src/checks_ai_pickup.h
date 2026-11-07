// The classes of ai_pickup.cpp for its checks (checks_ai_pickup.cpp), moved out of ai_pickup.h so that the
// production header names no exe address (docs/DECOMPILATION.md 13, "Batch 6-6").
#pragma once
#include "class_info.h"

namespace zax {

struct PickupClassDef {
    const char *name;
    DWORD var, size, vtable, create, create_array, register_fields, getter, getter2, dtor;
};

constexpr PickupClassDef PICKUP_CLASSES[] = {
    {"CPickupAI", 0x6d0b9c, 0x30, 0x5fcb88, 0x53d1e0, 0x53d240, 0x53d410, 0x53d190, 0x53d3b0, 0x53d2d0},
    {"COverridePickupAI", 0x6d0ba0, 0x38, 0x5f83b4, 0x53e8a0, 0x53e8e0, 0x53ea50, 0x53e850, 0x53e9f0, 0x53e950},
};

}  // namespace zax
