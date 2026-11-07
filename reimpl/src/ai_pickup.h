// The pickup AIs of Zax.exe (phase 3, game logic; 0x53d190-0x53f1f2): CPickupAI (the AI of every item
// lying in a level) and COverridePickupAI (per-object settings a level or a spawned entity puts next to
// it), CLayerPartAI's slot 31/33 default and the FieldColor helpers at the end of the range (Core).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CPickupAI (0x30, class 0x6d0b9c, version 11, vtable 0x5fcb88, parent CFlyweightLayerPartAI): +0x10
//    "Special Item" (owned CInventoryItem, deleted by the destructor), +0x14 "Normal Item" (index into
//    the item catalogue 0x6c0c08, "!None" = none), +0x18 "Alternate Item Def" (catalogue index, given
//    when the character cannot take the item), +0x20 "Pickup Effect" (model index of an explosion),
//    +0x24 "Pickup/Who Can Pick Up" (category mask, from 0x6c29a0 in the constructor; default "Player"),
//    +0x28 "Picked Up Action", +0x2c "Show in HUD". Slots: 11 reads the old lines "Inventory Item" and
//    "Custom Item" (an item object whose definition becomes "Normal Item"), 23 (entity) sets "Tries To
//    Collide", 24 Begin (entity) starts the respawn timer of a COverridePickupAI, 25 Update (entity,
//    seconds) respawns, 31 (part, entity, -) whether the entity may touch it, 34 (part, entity, three
//    more) the pickup (0x53e110).
//  - COverridePickupAI (0x38, class 0x6d0ba0, vtable 0x5f83b4, parent CLayerPartAI; no slots of its own):
//    +0x10 "Item" (CInventoryItem, replaces the pickup's), +0x14 "Alternate Item Def", +0x18 "Disable
//    CPickupAI's Alternate Item", +0x1c "Respawn Mode" (enum 0x6d0b80: 0 "Don't Respawn", 1 "Respawn
//    Delay After Pickup", 2 "Respawn Regularly If Gone", 3 "Respawn Regularly (Accumulate)"), +0x20
//    "Respawn Delay" (15 s), +0x24 "Min Respawn Delay", +0x28 "TimeUntilNextRespawn", +0x2c "Time Offset"
//    (random in [0, delay) at Begin), +0x30 "Last Simulation Time" (double, the reporter's time).
//  - FieldColor (0x10 bytes, Core): +0 b, g, r, a, +4 b, g, r again, +8 the 16-bit pixel (0x433a10),
//    +0xa the palette index, +0xc flags (bit 1: set from RGB, bit 0: palette index valid).
#pragma once
// The table of the module's classes for the checks is in checks_ai_pickup.h.
#include "class_info.h"
