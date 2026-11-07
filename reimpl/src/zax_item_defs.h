// The game's own inventory definitions 0x5b5f10-0x5b9ba8 (phase 3, inventory and weapons):
// CZaxInventoryItemDefinition (every record of the item catalogue "Inventory Item Definitions" is one)
// and CZaxInventoryTypeDefinition (every item category); their registration, lifetime and the slots of
// the item definition: the fix-ups after loading (slot 18), the continuous fire weapon data, the button
// labels, the battery and ammunition lookups, the strike damage over a distance (slot 23), the switch
// sound (slot 24), may a part take an item of it (slot 25: the quest's fruit statistics, the "Generate/
// Max Count" and "Generate/Required Item" rules), the required items it replaces (slot 26), the detail
// sequence's animation (slot 28), the icon bounds (slot 29), loading its images (slot 30), may a part
// use it as a weapon (slot 35), the item order of the inventory (0x5b8a80/0x5b8ac0 with
// ItemDefinition_Before/After) and the helpers the item classes call (0x5b7a60, 0x5b7ab0, 0x5b7e20).
//
// Layouts [confirmed: constructor, destructor, RegisterFields, docs/fields.csv]:
//   CZaxInventoryItemDefinition (0x100, vtable 0x604c3c, class 0x714434, version 1, parent
//     CInventoryItemDefinition 0x60): +0x60/+0x68/+0x70/+0x78 the labels of the Place, Disable, Use and
//     Undefined buttons (Local.txt entries of two ZStrings, 0x500740), +0x80/+0x88/+0x8c/+0x94 their
//     actions (owned CAction; +0x84 an older action, deleted by slot 18 with bit 2 of +0xf4), +0x98
//     "Switch Inventory Sound" (the sound list 0x713708), +0x9c "Generate/Must be discovered fist",
//     +0xa0 "Generate/PowerUp Model" (the projectile list 0x6cfdd8), +0xa4..+0xb8 the three ingredients
//     (quantity, catalogue index), +0xbc a heap pointer array (vtable 0x604c30, data +0xc0, count +0xc4,
//     capacity +0xc8), +0xcc "InventoryItem Specific Data" (owned object; a CContinuousFireWeaponData
//     0x472e80 for continuous fire weapons), +0xd0 "Display/Detail Image" (the detail sequence list
//     0x6c0948), +0xd4 "Display/HUD Image" (the HUD sequence list 0x6c06a0), +0xd8/+0xdc the HUD and
//     detail renderers (owned), +0xe0 the detail animation's frame time (float), +0xe4/+0xe8 the HUD
//     image's offset and +0xec the generator icon's x of slot 29, +0xf0 "Generate/Max Count", +0xf4 the
//     button bits (Place 1, Disable 4, Use 8, Undefined 0x40), +0xf8 "Generate/Required Item", +0xfc
//     "Item Priority"
//   CZaxInventoryTypeDefinition (0x28, vtable 0x604cd0, class 0x714438, version 1, parent
//     CInventoryTypeDefinition 8): +0x08/+0x0c "X/Y HUD Position", +0x10/+0x14 their screen justify
//     (the lists 0x7143d8/0x714418), +0x18 bits "Render In HUD" 1, "Lock to Inventory Bar" 2, "Render In
//     Editor" 4, +0x1c "Sort Priority", +0x20 "Default Inventory Select Sound", +0x24 "Default HUD
//     Renderer" (owned)
#pragma once
#include "inventory.h"
#include "inventory_defs.h"

namespace zax {

constexpr DWORD ZAX_ITEM_DEFINITION_VTABLE = 0x604c3c, ZAX_ITEM_DEFINITION_CLASS = 0x714434;
constexpr DWORD ZAX_TYPE_DEFINITION_VTABLE = 0x604cd0, ZAX_TYPE_DEFINITION_CLASS = 0x714438;

}  // namespace zax
