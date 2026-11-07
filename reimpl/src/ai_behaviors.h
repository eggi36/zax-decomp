// The entity behaviours of Zax.exe (phase 3, game logic; 0x494410-0x49a802): the small steps an AI
// queues on an entity's behaviour stack (turn to an angle or a target, play a sequence once or looped,
// idle with fidgets, wait, fade, set flags, play ratio or frame, execute an action, mark for deletion)
// and the stack itself (CEntityBehaviorStack, embedded in every CEntityAnimated at +0x90). Their
// Create, CreateArray and out-of-line constructors are in ai_classes.cpp; here: the class getters,
// destructors, RegisterFields, the behaviour slots and the helpers that make a behaviour ready to push.
//
// CEntityBehavior (0xc bytes, vtable 0x5f2930) [confirmed: code, vtables 0x5f2930-0x5f31cc]:
//  +8 flags: bit 0 set by Begin and when pushed on a stack, bit 1 by the helpers that make one.
//  Slots (names ours): 0 message (sender, text), 1 deleting destructor, 2 class getter, 19 reset the
//  fields, 23 Begin (entity; al: keep it on the stack), 25 Update (seconds, entity; al: still running),
//  26 the same default (al 0), 28 debug drawing (entity, view), 29 End (entity; deletes the behaviour).
//  CEntityBehaviorTurnToAngleBase and its children also have 30 (the entity's current angle) and 31
//  (set it).
// Entity slots the behaviours call: 47 (an int for CLayerPart_AngleFromPart), 58 set flags, 59 clear
// flags (0x200000 = "Stationary"), 84 the current frame (float), 103/104 set the aiming/moving angle
// (angle, 1). Entity fields: +0x1c flags, +0x48 the layer (for actions), +0x90 the behaviour stack,
// +0xb4 the fade (0..1), +0xb8 "Play Ratio", +0xc4/+0xc8 "Aiming Angle"/"Movement Angle", +0x100/+0x104
// two floats CEntityMovable adds to the sequence's motion, CEntityWalking +0x12c "Movement Speed" (2 =
// crouching: "IdleCrouch").
#pragma once
// The table of the module's classes for the checks is in checks_ai_behaviors.h.
#include "class_info.h"
