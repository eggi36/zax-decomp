// The classes of ai_patrol.cpp for its checks (checks_ai_patrol.cpp), moved out of ai_patrol.h so that the
// production header names no exe address (docs/DECOMPILATION.md 13, "Batch 6-6").
#pragma once
#include "class_info.h"

namespace zax {

// One class of the module for the checks: name, class variable, size, vtable, Create, CreateArray,
// RegisterFields, the getters (static registration's and slot 2's), deleting destructor
struct PatrolClassDef {
    const char *name;
    DWORD var, size, vtable, create, create_array, register_fields, getter, getter2, dtor;
};

constexpr PatrolClassDef PATROL_CLASSES[] = {
    {"COldBadPatrolAI", 0x6d0b10, 0x34, 0x5fc8d4, 0x53a990, 0x53aa00, 0x53aba0, 0x53a940, 0x53ab40, 0x53aaa0},
    {"CPatrolAreaAI", 0x6d0b34, 0xd8, 0x5fc984, 0x53b400, 0x53b450, 0x53b5f0, 0x53b3a0, 0x53b580, 0x53b4e0},
    {"CPatrolBaseAI", 0x6d0b3c, 0x10, 0x5f42d4, 0, 0, 0, 0x53bcc0, 0x53bd10, 0},
    {"CPatrolPathAI", 0x6d0b70, 0xf4, 0x5fca38, 0x53be30, 0x53be90, 0x53c060, 0x53bdd0, 0x53bff0, 0x53bf30},
    {"CFollowPathAI", 0x6d0b74, 0xf4, 0x5fcae0, 0x53cd00, 0x53cd70, 0x53cfc0, 0x53cc50, 0x53cf00, 0x53ce30},
};

}  // namespace zax
