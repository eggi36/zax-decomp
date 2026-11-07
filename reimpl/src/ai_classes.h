// Create and CreateArray of the AI classes of Zax.exe (subsystem AI in docs/functions.csv: the AIs,
// the entity behaviours and a few helper classes such as CFidgetData, CDeathBehaviorListDefinition and
// the three CSet...AIAction actions) and the out-of-line constructors they call.
//
// Every Create takes a pool block of the class's size (operator_new_ecx 0x417710) and constructs it;
// every CreateArray takes a heap block with the count in front (operator new 0x5d033a) and constructs
// each element; both return 0 when the allocation fails. The constructors are inlined into both
// functions: per class level the members' constructors, the level's vtable, Object_InitFields
// (0x4173c0, sets no values) and the level's default values [confirmed: Create and CreateArray of all
// 106 classes give the same sequence of calls and stores, compared with capstone]. Most AI classes
// start with CAIBase's constructor (0x4fd060: CObject, vtable 0x5f9540, +8 "Marked For Deletion" 0) and
// the CLayerPartAI level (vtable 0x5ed1d0), the entity behaviours with CEntityBehavior's constructor
// (0x494470: CObject, vtable 0x5f2930, +8 0).
//
// The constructors of this module were written from the exe's code with capstone (the calls and stores
// of Create, cross-checked against CreateArray); the comments name the class of each vtable (through
// its class getter) and the field at each offset (docs/fields.csv).
//
// Also here: the class getters and RegisterFields of the nine trigger AIs (CTriggerAI, CTouchingTriggerAI,
// CollideTriggerAI, CPassThroughTriggerAI, CRelayAI, CRepeatTimerTriggerAI, CSpawnPointAI,
// CTouchingOvalTriggerAI, CTouchingPolygonTriggerAI). Every RegisterFields makes its descriptors on the
// heap and adds each through the class getter, which it inlines before every ClassInfo_AddField
// [confirmed: code]. Two of them also give the class info an alternative class name (+8, searched by
// the class registry): "CProximitySwitchAI" for CTouchingOvalTriggerAI and "CTouchingPolyongTriggerAI"
// (sic) for CTouchingPolygonTriggerAI, whose RegisterFields does nothing else.
#pragma once
#include "exe_base.h"
