// The player's control AIs of Zax.exe (phase 3, game logic; 0x542170-0x545442): CPlayerControlAI,
// CPlayerWalkingControlSettings, CPlayerWalkingControlAI (Zax walking under the player's control: the
// mouse and keyboard turned into movement, aiming, firing, weapon and item keys, the zoom keys), and the
// two AIs that show or hide the weapon and item models on the player's character, CPlayerWalkingWeaponAI
// and CPlayerWalkingItemAI.
//
// The raw input is not read here: the player object (world slot 89, +0x10 of the AI) answers whether an
// action is pressed (0x46ed70 with the action number) and gives the mouse vector (+0x5c/+0x60) and the
// aiming angle (0x470b50); this module maps those to the character [confirmed: code]. Action numbers
// (names ours, from what the code does with them [assumption]): 0-3 the movement keys (back, forward,
// left, right), 4 aim with the mouse, 5 secondary fire, 6/7 next/previous weapon, 9-0xb inventory
// window pages, 0xc-0xe zoom out/in/reset, 0x10-0x14 the strafe keys, 0x15/0x16 one/all companion
// drones, 0x17 the autogun, 0x18-0x1b the firing directions, 0x1c-0x2a the weapon keys.
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CPlayerControlAI (0x20, abstract, class 0x7110ec, version 5, vtable 0x5fce04): +0x10 the player
//    object, +0x14/+0x18 the last movement direction, +0x1c "Player Num" (its setter 0x542550 takes the
//    world's player object).
//  - CPlayerWalkingControlSettings (0x40, class 0x711108, version 0, parent CObject, vtable 0x5fcebc):
//    the walking settings of the custom rules (0x5a1f40): +8 "Strafe Offset", +0xc/+0x10 "Start Walk/Run
//    Mouse Dist", +0x14/+0x18 "Min Walk/Run Ratio", +0x1c "Max Player Lives", +0x20/+0x24 the missile
//    trail rates, +0x28 "Min Move Dist", +0x2c "Max Cursor Distance", +0x30-+0x38 the turn rates
//    (radians, shown in degrees), +0x3c "Gun Height".
//  - CPlayerWalkingControlAI (0x120, class 0x71110c, version 5, vtable 0x5fcf18): +0x20 the embedded
//    CEntityBehaviorMoveToPos, +0xdc the embedded CEntityBehaviorIdle, +0x10c/+0x110 the character's
//    position, +0x115-+0x117 state bytes (+0x117: mouse control on), +0x118 the time played, +0x11c
//    "moved by a key", +0x11d "walking backwards", +0x11e "Played DryFire Sound". The constructor loads
//    the "Direction Pointer" model's first sequence into 0x711110 once (a fatal error box and exit(-1)
//    without the model).
//  - CPlayerWalkingWeaponAI (0x14, class 0x711118, version 2) and CPlayerWalkingItemAI (0x1c, class
//    0x71111c; +0x14 "Type Index" (item category), +0x18 "Definition Index"), parent CSameAsParentAI:
//    Update hides the model ("Force Invisable") unless the character's current item of category "Primary"
//    (the item AI: of "Type Index", and with that definition) exists.
#pragma once
// The table of the module's classes for the checks is in checks_ai_player.h.
#include "class_info.h"
