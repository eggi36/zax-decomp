// The patrol AIs of Zax.exe (phase 3, game logic; 0x53a940-0x53d190): COldBadPatrolAI, CPatrolBaseAI,
// CPatrolAreaAI, CPatrolPathAI and CFollowPathAI with their class getters (both copies: the static
// registration's and slot 2), Create/CreateArray where not done before, deleting destructors (slot 1),
// RegisterFields and behaviour slots, CLayerPartAI's empty slot 34 and the two timer helpers
// 0x53d150/0x53d170 (QueryPerformanceCounter/QueryPerformanceFrequency as a qword in edx:eax).
//
// AI slots (CLayerPartAI's vtable 0x5ed1d0; names ours [assumption]): 24 Begin (entity), 25 Update
// (entity, seconds), 34 collision (entity, other part, three more), 41 debug drawing (entity, view);
// the patrol AIs add 43 (drawing of the area: view, entity) and 44 (pick a new point: entity).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - COldBadPatrolAI (0x34, class 0x6d0b10, version 3, vtable 0x5fc8d4, parent CLayerPartAI; alternative
//    class name "CPatrolAI"): +0x10 a byte "walking" (set when "Walk" was started), +0x14/+0x18 the
//    minimum and maximum distance of the next leg (120/200 from the constructor; shrunk by 10/5 after
//    three blocked tries, back to 120/200 after a free one), +0x1c 16 bytes handed to 0x4916f0 (never
//    constructed), +0x2c a byte "too far from home" (set by Update), +0x30 "Return Home Dist" (-1 = off).
//    Its constructor reseeds the random generator 0x6d06c0 (ai_classes.cpp).
//  - CPatrolBaseAI (0x10, abstract, class 0x6d0b3c, vtable 0x5f42d4): the level between CLayerPartAI and
//    the patrol AIs (CPatrolAreaAI, CPatrolPathAI, CGuardPosAI, CScanAreaAI, FollowAI).
//  - CPatrolAreaAI (0xd8, class 0x6d0b34, version 2, vtable 0x5fc984): +0x10 "Radius" (200), +0x14 the
//    embedded CEntityBehaviorMoveToPos "Moving" (its +0xa4 = +0xb8 here the speed), +0xd0
//    "TimeOfLastChangeDirectionDueToCollide" (registered, not used by the slots), +0xd4 "Retry New Point"
//    (seconds until the next try after all tries failed).
//  - CPatrolPathAI (0xf4, class 0x6d0b70, version 3, vtable 0x5fca38): +0x10 "Revise Point", +0x18
//    "Revising Data", +0x1c "Order" (enum 0x6d0b48: 0 "Back and forth", 1 "Forth and back", 2 "Loop",
//    3 "Random", 4 "Go To End"), +0x20 "Path Poly" (the name of the path's part), +0x24
//    "PathPolygonLayerPart" (the part found, with a polygon of at least 2 points), +0x28 "Ignore last
//    point", +0x2c "End of path action", +0x30 "Target Point" (index, -1 = none), +0x34 "Moving"
//    (CEntityBehaviorMoveToPos; its +0xa4 = +0xd8 the speed), +0xf0 "TimeBeforCanReviseData".
//  - CFollowPathAI (0xf4, class 0x6d0b74, vtable 0x5fcae0, parent CPatrolPathAI): no fields or slots of its
//    own; its RegisterFields changes the inherited descriptors: "Order" defaults to "Go To End", "End of
//    path action" takes CSendAIDoneMessage.
#pragma once
// The table of the module's classes for the checks is in checks_ai_patrol.h.
#include "class_info.h"
