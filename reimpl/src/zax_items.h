// The game's own inventory item classes 0x5ae930-0x5b0f08 (phase 3, inventory and weapons): the
// health, mineral, ammunition and charger items (CCountInventoryItem / CInventoryItem), the energy
// items (CEnergyItem and the storage, battery charge and battery items derived from it) and the stats
// item of the HUD; their registration (Create, CreateArray, the deleting destructors, the class getters
// and their slot 2 copies, RegisterFields) and their slots (may a part take it, take it, count down,
// add or use energy) and the helpers that find the battery or count the ammunition and minerals.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CStatsInventoryItem (0x1c, vtable 0x5f1140, class 0x714328, version 1, no parent, alternative
//     class name "CZaxStatsInventoryItem"): +0x08 "Item Catagory" (sic; 2 after construction, the
//     enum list 0x714120), +0x0c "Label Text", +0x10 "Label Color" (the colour list 0x714300), +0x14
//     "Quantity Text", +0x18 "Quantity Color"
//   CHealthItem (0x2c, vtable 0x604120, class 0x71432c, version 6, parent CCountInventoryItem): +0x24
//     "Num Health" (hit points, 10), +0x28 "Storable Item" (byte); its RegisterFields sets "Rounds"'
//     +0x34 to 1
//   CMineralInventoryItem (0x24, vtable 0x6041f8, class 0x714330, version 2, alternative class name
//     "CMineralItem"), CAmmunitionInventoryItem (0x24, vtable 0x6042d0, class 0x714334, version 2):
//     CCountInventoryItem without fields of their own
//   CChargerInventoryItem (0x18, vtable 0x6043a8, class 0x714338, version 1, parent CInventoryItem)
//   CEnergyItem (0x1c, vtable 0x5f07e0, class 0x71433c, version 2, parent CInventoryItem): +0x18
//     "Current Charge" (mega-joules; the most is the definition's "Energy" +0x4c)
//   CEnergyStorageItem (0x20, vtable 0x60446c, class 0x714340, version 2): +0x1c "Pickup If Full"
//   CBatteryChargeInventoryItem (0x1c, vtable 0x604538, class 0x714344, version 1): RegisterFields
//     renames the inherited "Current Charge" to "Charge to Add" (units "MJ", +0x34 = 10)
//   CBatteryInventoryItem (0x1c, vtable 0x604604, class 0x714348, version 2)
// The constructors are inlined into Create and CreateArray one level per class: CInventoryItem's
// constructor 0x42a2b0, the count item's level (vtable 0x5ee538, InitFields, "Max Rounds" +0x1c and
// "Pickup If Full" +0x20 cleared), then each level's vtable and InitFields.
#pragma once
#include "inventory_items.h"

namespace zax {

constexpr DWORD STATS_ITEM_CLASS = 0x714328, HEALTH_ITEM_CLASS = 0x71432c, MINERAL_ITEM_CLASS = 0x714330,
                AMMUNITION_ITEM_CLASS = 0x714334, CHARGER_ITEM_CLASS = 0x714338, ENERGY_ITEM_CLASS = 0x71433c,
                ENERGY_STORAGE_ITEM_CLASS = 0x714340, BATTERY_CHARGE_ITEM_CLASS = 0x714344, BATTERY_ITEM_CLASS = 0x714348;
constexpr DWORD STATS_ITEM_VTABLE = 0x5f1140, HEALTH_ITEM_VTABLE = 0x604120, MINERAL_ITEM_VTABLE = 0x6041f8,
                AMMUNITION_ITEM_VTABLE = 0x6042d0, CHARGER_ITEM_VTABLE = 0x6043a8, ENERGY_ITEM_VTABLE = 0x5f07e0,
                ENERGY_STORAGE_ITEM_VTABLE = 0x60446c, BATTERY_CHARGE_ITEM_VTABLE = 0x604538, BATTERY_ITEM_VTABLE = 0x604604;

}  // namespace zax
