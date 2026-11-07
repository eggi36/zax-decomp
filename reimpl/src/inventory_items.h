// The inventory item classes and the rest of the inventory 0x424000-0x42bfff: class registration and
// lifetime of CItemTypeData, CInventory, CInventoryItemHolder, CInventoryDisplayOptions (with its three
// page option members), CInventoryItem and CCountInventoryItem, the item slots (use, add to an
// inventory, delays, counts) and the inventory functions the bookkeeping batch left out (inventory.h):
// the destructor, emptying, the per-frame update of the category changes, changing and cycling the
// selection, adding an item, discovering a definition.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CInventoryItem (0x18, vtable 0x5ee474, class 0x62ea20, version 5, parent CInventoryItemBase
//     0x4dd2d0 whose constructor 0x4dd410 / destructor 0x4dd460 write vtable 0x5f85b8):
//     +0x08 the definition index (the item catalogue 0x6c0c08, records at *0x6c0c38), +0x0c "Debug/
//     Current Delay" (float, the reuse delay left), +0x10 "Debug/Currently Selected", +0x11 "Debug/Marked
//     For Deletion", +0x14 "Debug/Broadcaster" (owned CBroadcaster; told "Delete" by the destructor)
//   CCountInventoryItem (0x24, vtable 0x5ee538, class 0x62ea24, version 2): +0x18 "Rounds" (default 40),
//     +0x1c "Max Rounds" (0 = unlimited, -1 allowed), +0x20 "Pickup If Full" (byte)
//   CInventoryItemHolder (0xc, vtable 0x5ee008, class 0x62e9f8): +0x08 "The Item" (owned item)
//   CInventoryDisplayOptions (0x2a0, vtable 0x5ee1d0, class 0x62ea18, version 3, no parent class):
//     the inventory screen's layout (44 fields: page area, close button, fonts, tab buttons, the HUD
//     positions), with three embedded page option objects: +0x3c CInventoryDisplayPageOptions (0xa8,
//     vtable 0x5ee2e4, two colours at +0x88/+0x98), +0xe4 CMissionDisplayPageOptions (0x60, vtable
//     0x5ee288, a Local.txt entry at +0x18), +0x144 CVoiceLogDisplayPageOptions (0x9c, vtable 0x5ee22c,
//     Local.txt entries at +0x18, +0x38, +0x44, +0x50, +0x90), and Local.txt entries (two ZStrings,
//     0x500740) at +0x260, +0x268, +0x280, +0x288, +0x290, +0x298
//
// Return values are what the original leaves in eax; where only al is defined the upper bytes are
// reproduced from the call before.
#pragma once
#include "inventory.h"

namespace zax {

struct CCountInventoryItem : CInventoryItem {
    int rounds;           // 0x18
    int max_rounds;       // 0x1c
    BYTE pickup_if_full;  // 0x20
    BYTE pad21[3];
};
static_assert(sizeof(CCountInventoryItem) == 0x24);

constexpr DWORD INVENTORY_ITEM_VTABLE = 0x5ee474, COUNT_ITEM_VTABLE = 0x5ee538;
constexpr DWORD INVENTORY_ITEM_CLASS = 0x62ea20, COUNT_ITEM_CLASS = 0x62ea24;
constexpr DWORD ITEM_CATALOGUE = 0x6c0c08;   // the resource list of the item definitions

}  // namespace zax
