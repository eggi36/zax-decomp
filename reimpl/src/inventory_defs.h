// The inventory definitions 0x4d4e80-0x4d7fff: CInventoryBase (the abstract base of CInventory),
// CProjectileInfo (an entry of a weapon's projectile list), CInventoryItemDefinition (the abstract base
// of the item definitions, CZaxInventoryItemDefinition and the others derive from it in
// 0x5b5000-0x5b9fff), CInventoryTypeDefinition (an item category) and the four definition lists
// around them: the item categories "Inventory Item Types" (0x6c0800), the item catalogue "Inventory
// Item Definitions" (0x6c0c08), the icon and frame image lists (0x6c0aa8, 0x6c0540) and the detail
// and HUD sequence lists (0x6c0948, 0x6c06a0).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CInventoryBase (8, vtable 0x5f7c94, class 0x6c0534, version 1, abstract, parent CObject); the
//     factory of inventories 0x6c052c (set by 0x4d4fe0 with CInventory's Create, called by 0x4d4ff0)
//   CProjectileInfo (0x14, vtable 0x5f7d08, class 0x6c0d50): +8 "Shot Angle" (degrees, -360..360),
//     +0xc "X offset", +0x10 "Y offset"
//   CInventoryItemDefinition (0x60, vtable 0x5f7d64, class 0x6c0d54, version 9, abstract, alternative
//     class name "CInventoryItemDefiniton" (sic)): +8 "Weapon/Discharge Sequence" (ZString, "Shoot1"),
//     +0xc "Weapon/Shot Sound" (the sound list 0x713708, "!None.wav"), +0x10 "Display/Icon Image" and
//     +0x14 "Display/Generator Icon Image" (the icon list 0x6c0aa8), +0x18 "Inventory Type" (alias "HUD
//     Type", the category list 0x6c0800), +0x1c "Default Inventory Item" (owned CInventoryItemBase),
//     +0x20 "Projectiles/Projectile" and +0x24 "Weapon/Shot Flash" (the list 0x6cfdd8), +0x28
//     "Weapon/Max Shoot Angle" (degrees), +0x2c "Weapon/Ammunition" (the item catalogue), +0x34 "Weapon/
//     Strike Damage", +0x38 its damage type, +0x3c its radius, +0x40/+0x44 "Projectiles/Num
//     Projectile" (CProjectileInfo list), +0x48 "Reuse Delay" (alias "Reuse Delay Seconds", 0.3 s),
//     +0x4c "Energy" (mega-joules), +0x50 "Rounds Needed" (1), +0x54 "Weapon/New Shot Action" (alias "Use
//     Action", owned CAction), +0x58/+0x5c the x offsets of the two icons (slot 29)
//   CInventoryTypeDefinition (8, vtable 0x5f7f2c, class 0x6c0d58): no fields of its own
// The definition lists are definition lists of definitions.h (records of 0x80 bytes): the categories
// with vtable 0x5f7f88 (base 0x5f802c), the item catalogue with 0x5f7de4 (base 0x5f7e88).
#pragma once
#include "definitions.h"
#include "file_search.h"
#include "named_list.h"

namespace zax {

constexpr DWORD INVENTORY_BASE_VTABLE = 0x5f7c94;
constexpr DWORD ITEM_DEFINITION_VTABLE = 0x5f7d64, ITEM_DEFINITION_CLASS = 0x6c0d54;
constexpr DWORD TYPE_DEFINITION_VTABLE = 0x5f7f2c, TYPE_DEFINITION_CLASS = 0x6c0d58;
constexpr DWORD ITEM_CATALOGUE_LIST = 0x6c0c08, ITEM_CATALOGUE_LIST_VTABLE = 0x5f7de4, ITEM_CATALOGUE_BASE_VTABLE = 0x5f7e88;
constexpr DWORD ITEM_CATEGORY_LIST = 0x6c0800, ITEM_CATEGORY_LIST_VTABLE = 0x5f7f88, ITEM_CATEGORY_BASE_VTABLE = 0x5f802c;

}  // namespace zax
