// The classes of ai_behaviors.cpp for its checks (checks_ai_behaviors.cpp), moved out of ai_behaviors.h so that the
// production header names no exe address (docs/DECOMPILATION.md 13, "Batch 6-6").
#pragma once
#include "class_info.h"

namespace zax {

// One class of the module, for the checks: name, class variable, class getter (the vtable's slot 2),
// a second copy of the getter (used by the class's static registration; 0 = none), Create (0 for the
// abstract classes), size, vtable, deleting destructor (slot 1), RegisterFields (0 = none of its own),
// CreateArray, the parent class (index in the table, -1 = CObject)
struct BehaviorClassDef {
    const char *name;
    DWORD var, getter, getter2, create, size, vtable, dtor, register_fields, create_array;
    int parent;
};

constexpr BehaviorClassDef BEHAVIOR_CLASSES[] = {
    {"CEntityBehavior", 0x6bddcc, 0x494410, 0, 0, 0xc, 0x5f2930, 0x4944b0, 0, 0, -1},
    {"CEntityBehaviorTurnToAngleBase", 0x6bde04, 0x494560, 0x494510, 0, 0x14, 0x5f29a8, 0x494650, 0x4945c0, 0, 0},
    {"CEntityBehaviorTurnMovingAngle", 0x6bde08, 0x4948f0, 0x4946e0, 0x494790, 0x14, 0x5f2a28, 0x494850, 0, 0x4947e0, 1},
    {"CEntityBehaviorTurnAimingAngle", 0x6bde0c, 0x494bf0, 0x4949e0, 0x494a90, 0x14, 0x5f2aa8, 0x494b50, 0, 0x494ae0, 1},
    {"CEntityBehaviorTurnToAngle", 0x6bde10, 0x494f90, 0x494ce0, 0x494de0, 0x1c, 0x5f2b28, 0x494ed0, 0x4950a0, 0x494e40, 2},
    {"CEntityBehaviorTurnToTarget", 0x6bde14, 0x495920, 0x495660, 0x495760, 0x24, 0x5f2ba8, 0x495830, 0x495a30, 0x4957b0, 3},
    {"CEntityBehaviorSetRotationSequence", 0x6bde18, 0x4964c0, 0x496320, 0x496370, 0x10, 0x5f2c28, 0x496420, 0x496520, 0x4963b0, 0},
    {"CEntityBehaviorPlaySequenceOnce", 0x6bde1c, 0x496860, 0x496620, 0x4966d0, 0x28, 0x5f2ca0, 0x4967a0, 0x496920, 0x496720, 6},
    {"CEntityBehaviorSetPlayRatio", 0x6bde20, 0x4973f0, 0x497290, 0x4972e0, 0x10, 0x5f2d18, 0x497370, 0x497450, 0x497310, 0},
    {"CEntityBehaviorMarkForDeletion", 0x6bde24, 0x4976c0, 0x497560, 0x4975b0, 0xc, 0x5f2d90, 0x497640, 0, 0x4975e0, 0},
    {"CEntityBehaviorClearFlags", 0x6bde28, 0x497910, 0x4977a0, 0x4977f0, 0x10, 0x5f2e08, 0x497890, 0x497970, 0x497830, 0},
    {"CEntityBehaviorWait", 0x6bde2c, 0x497db0, 0x497c50, 0x497ca0, 0x14, 0x5f2e80, 0x497d30, 0x497e10, 0x497cd0, 0},
    {"CEntityBehaviorTransition", 0x6bde30, 0x498090, 0x497fe0, 0, 0x18, 0x5f2ef8, 0x498230, 0x498150, 0, 11},
    {"CEntityBehaviorFade", 0x6bde34, 0x4984f0, 0x498270, 0x498370, 0x18, 0x5f2f70, 0x498430, 0, 0x4983c0, 12},
    {"CEntityBehaviorIdle", 0x6bde38, 0x4988a0, 0x498660, 0x498720, 0x30, 0x5f2fe8, 0x4987e0, 0x498960, 0x498770, 11},
    {"CEntityBehaviorSetCurFrame", 0x6bde3c, 0x499470, 0x499310, 0x499360, 0x10, 0x5f3064, 0x4993f0, 0, 0x499390, 0},
    {"CEntityBehaviorExecuteAction", 0x6bde40, 0x4996a0, 0x499540, 0x499590, 0x10, 0x5f30dc, 0x499620, 0x499700, 0x4995c0, 0},
    {"CEntityBehaviorPlaySequenceLooped", 0x6bde44, 0x499950, 0x4997b0, 0x499800, 0x14, 0x5f3154, 0x4998b0, 0x4999b0, 0x499840, 0},
    {"CEntityBehaviorStack", 0x6bde64, 0x499fb0, 0, 0x49a000, 0x20, 0x5f31cc, 0x49a0d0, 0x49a1e0, 0x49a050, -1},
};
constexpr int NBEHAVIOR_CLASSES = sizeof BEHAVIOR_CLASSES / sizeof BEHAVIOR_CLASSES[0];

}  // namespace zax
