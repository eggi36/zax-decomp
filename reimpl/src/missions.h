// Mission objectives, missions, the mission manager and the mission actions of Zax.exe (phase 3, game
// logic; 0x510850-0x515300): for the 17 classes their Create, CreateArray, class getter, RegisterFields
// and deleting destructor (slot 1), the objectives' slots 23-25 and status update, the mission's and
// the manager's helpers and the actions' slot 23 (execute).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CMissionObjective (0x24 bytes, class 0x6cfd78, vtable 0x5fa3fc, parent CObject): +8 "ID Tag"
//    (ZString), +0xc "Description" and +0x14 "Status" (LocalEntry: key and text), +0x1c "Completed
//    Action" (owned CAction), +0x20 "Old Objective" (owned CMissionObjective: a copy of the objective
//    from before the last status change). Slots 23 increment, 24 complete, 25 failed (names ours).
//    CCountMissionObjective (0x2c, 0x6cfd7c, 0x5fa464): +0x24 "Total Needed", +0x28 "Current Count".
//    CSeriesMissionObjective (0x28, 0x6cfd80, 0x5fa4cc): +0x24 "Next Status" (owned CAction).
//  - CMission (0x40, 0x6cfd88, version 2, 0x5fa534): +8 "ID Tag", +0xc "Description" (LocalEntry),
//    +0x14 the objectives (pointer array: vtable 0x5fa59c, +0x18 data, +0x1c count, +0x20 capacity),
//    +0x24 "Completed Action", +0x28 "Old Num Objectives", +0x2c "Mission Number" (-1 = from the ID
//    tag), +0x30 the par stat data (pointer array 0x5fa590, +0x34/+0x38/+0x3c).
//  - CMissionManager (0x20, 0x6cfd8c, 0x5fa5c0): +8 the missions (pointer array 0x5fa61c, +0xc/+0x10/
//    +0x14), +0x18 "Current Mission Index", +0x1c "Last Changed Time". The world's slot 98 gives it.
//  - The actions (parent CAction; slot 23 executes: (layer, a2, a3, a4), al the result):
//    CDefineMission (0x1c: +8 "Mission", +0xc the maps not to flush: ZString new[] array, vtable 0x5fa6a4,
//    +0x10 data, +0x14 count, +0x18 capacity), CSelectMission (0xc: +8 "Mission Id Tag"),
//    CSelectNoMission (8), CAddMissionObjective (0xc: +8 "New Objective"), CUpdateMissionObjective
//    (0x14: +8 "Objective Id Tag", +0xc "New Status"), CIncrementMissionObjective,
//    CDeleteMissionObjective, CIsMissionObjectiveComplete, CIsMissionObjectiveDefined (0xc: +8
//    "Objective Id Tag"), CIsMissionComplete (8), CDisplayMissionFailedScreenAction and
//    CDisplayMissionCompletedScreenAction (0x10: +8 the message, LocalEntry).
#pragma once
#include "class_info.h"

namespace zax {

// One class of the module, for the checks: name, class variable, Create, CreateArray, RegisterFields
// (0 = none of its own), class getter, deleting destructor, size, vtable
struct MissionClassDef {
    const char *name;
    // key-typed (batch 6-7), so the table's addresses go through the address layer; only the checks read them, in the
    // DLL, where every key is the DWORD address itself
    GlobKey var;
    CodeKey create, create_array, register_fields, getter, dtor;
    DWORD size;
    VtKey vtable;
};

constexpr MissionClassDef MISSION_CLASSES[] = {
    {"CMissionObjective", 0x6cfd78, 0x510850, 0x5108a0, 0x510a60, 0x510a00, 0x510920, 0x24, 0x5fa3fc},
    {"CCountMissionObjective", 0x6cfd7c, 0x511020, 0x511080, 0x511220, 0x510f70, 0x511110, 0x2c, 0x5fa464},
    {"CSeriesMissionObjective", 0x6cfd80, 0x5115c0, 0x511620, 0x511860, 0x5117b0, 0x5116b0, 0x28, 0x5fa4cc},
    {"CMission", 0x6cfd88, 0x511a10, 0x511a80, 0x511d20, 0x511cc0, 0x511b20, 0x40, 0x5fa534},
    {"CMissionManager", 0x6cfd8c, 0x512400, 0x512450, 0x5125e0, 0x512360, 0x5124d0, 0x20, 0x5fa5c0},
    {"CDefineMission", 0x6cfd90, 0x5128c0, 0x512910, 0x512b20, 0x512ac0, 0x512990, 0x1c, 0x5fa634},
    {"CSelectMission", 0x6cfd94, 0x512fd0, 0x513010, 0x513180, 0x513120, 0x513080, 0xc, 0x5fa6b0},
    {"CSelectNoMission", 0x6cfd98, 0x513320, 0x513350, 0, 0x513430, 0x5133b0, 0x8, 0x5fa720},
    {"CAddMissionObjective", 0x6cfd9c, 0x513530, 0x513560, 0x5136a0, 0x513640, 0x5135c0, 0xc, 0x5fa790},
    {"CUpdateMissionObjective", 0x6cfda0, 0x513840, 0x513880, 0x513a10, 0x5139b0, 0x5138f0, 0x14, 0x5fa800},
    {"CIncrementMissionObjective", 0x6cfda4, 0x513ce0, 0x513d20, 0x513e90, 0x513e30, 0x513d90, 0xc, 0x5fa870},
    {"CDeleteMissionObjective", 0x6cfda8, 0x5140d0, 0x514110, 0x514280, 0x514220, 0x514180, 0xc, 0x5fa8e0},
    {"CIsMissionObjectiveComplete", 0x6cfdac, 0x514480, 0x5144c0, 0x514630, 0x5145d0, 0x514530, 0xc, 0x5fa950},
    {"CIsMissionObjectiveDefined", 0x6cfdb0, 0x514800, 0x514840, 0x5149b0, 0x514950, 0x5148b0, 0xc, 0x5fa9c0},
    {"CIsMissionComplete", 0x6cfdb4, 0x514b20, 0x514b50, 0, 0x514c30, 0x514bb0, 0x8, 0x5faa30},
    {"CDisplayMissionFailedScreenAction", 0x6cfdb8, 0x514d50, 0x514d90, 0x514f10, 0x514eb0, 0x514e00, 0x10, 0x5faaa0},
    {"CDisplayMissionCompletedScreenAction", 0x6cfdbc, 0x5150b0, 0x5150f0, 0x515270, 0x515210, 0x515160, 0x10, 0x5fab10},
};

// The descriptor of "Par Stat Data" that CMission's RegisterFields keeps (the element of "Par Stat Data
// List"; written by every call of it)
constexpr DWORD MISSION_PAR_STAT_FIELD = 0x6cfd84;

}  // namespace zax
