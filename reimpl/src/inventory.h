// The inventory of a character (CInventory, 0x48 bytes, constructor 0x424b30, vtable 0x5ee0c0) and the
// bookkeeping of its items: the functions 0x4251c0-0x4268d0 that only walk the item list, the per
// category selection and the list of discovered items.
//
// Layout [confirmed: constructor, fields of CInventory in docs/fields.csv]:
//   +0x08 "Properly Inited" byte (set by Repair 0x4262e0; several functions call Repair while it is 0)
//   +0x0c "Item In Focus" (index or -1)
//   +0x10 "Item Type Info": a new[] block of CItemTypeData (0x18 bytes, vtable 0x5ee064), one per item
//         category, count at +0x14 (taken from the category list 0x6c0800 when the inventory is made)
//   +0x18 "Inventory List": pointer array of CInventoryItem (vtable 0x5ee144), data +0x1c, count +0x20
//   +0x28 "Discovered Items": pointer array (vtable 0x5ee138) of definition objects, data +0x2c, count +0x30
//   +0x38 "Disposed Items": pointer array (vtable 0x5ee144); RemoveAt moves the removed item there
// docs/classes.csv names 0x5ee064 as CInventory's vtable; that is CItemTypeData's (the constructor
// writes 0x5ee0c0 into the inventory and 0x5ee064 into each CItemTypeData).
//
// An item (CInventoryItem) has its definition index at +8 ("Definition Index", an index into the item
// catalogue, a resource list: records of 0x80 bytes at *0x6c0c38, count *0x6c0c3c). 0x4dd480 resolves
// it (loads the definition on first use; an index beyond the count means 0, a record that is being
// loaded gives 0). A definition has its category at +0x18 (an index into the category list 0x6c0800,
// e.g. "Primary", "Secondary") and a sort order at +0xfc. +0x10 "Currently Selected", +0x11 "Marked For
// Deletion". Item slots used here: 2 class info, 1 deleting destructor, 34 (0x88, two arguments), 41
// (0xa4, the count; -1 means one), 44 (0xb0) select and 45 (0xb4) deselect, both with the inventory.
//
// The order of two items (0x42a3b0 "before", 0x42a3f0 "after", both item methods with the other
// item and the two inventory indices) compares the definitions' sort order, then their catalogue
// index, then the inventory index (0x5b8b00, 0x5b8bd0).
//
// Return values are what the original leaves in eax, also for the functions without a result.
//
// Not here (they call other subsystems): ChangeTo 0x425590 (plays "No Inventory Cycle.wav" when the
// category has fewer than two items) and its two callers 0x4256c0/0x425700 (cycle forwards and
// backwards), adding an item 0x4259b0/0x425a70 (stacking, the HUD), Discover 0x426690 (HUD message)
// and the item use with the ore and crystal counters 0x426930.
#pragma once
#include "ptr_array.h"

namespace zax {

struct CItemTypeData {      // one per item category, vtable 0x5ee064
    DWORD vftable;
    DWORD unknown4;
    float seconds_into_change;   // 0x08
    float seconds_to_change;     // 0x0c
    DWORD cur_index;             // 0x10 the selected item of the category, -1 none
    DWORD changing_to;           // 0x14 "Changing To Inventory Index", -1 none
};
static_assert(sizeof(CItemTypeData) == 0x18);

struct ItemDefinition {     // an object of the item catalogue, only the members used here
    BYTE unknown0[0x18];
    DWORD category;         // 0x18
    BYTE unknown1c[0xfc - 0x1c];
    DWORD order;            // 0xfc
};

struct CInventoryItem {
    DWORD vftable;
    DWORD unknown4;
    DWORD definition;       // 0x08 index into the item catalogue
    float current_delay;    // 0x0c
    BYTE selected;          // 0x10
    BYTE marked_for_deletion;   // 0x11
    BYTE pad12[2];
    void *broadcaster;      // 0x14
};
static_assert(sizeof(CInventoryItem) == 0x18);

struct CInventory {
    DWORD vftable;              // 0x5ee0c0
    DWORD unknown4;
    BYTE inited;                // 0x08
    BYTE pad9[3];
    DWORD focus;                // 0x0c
    CItemTypeData *types;       // 0x10
    DWORD num_types;            // 0x14
    ZPtrArray items;            // 0x18 CInventoryItem *
    ZPtrArray discovered;       // 0x28 definition objects
    ZPtrArray disposed;         // 0x38 CInventoryItem *

    int THISCALL CountOfCategory(DWORD category);                       // 0x4251c0
    DWORD THISCALL CollectOfCategory(DWORD category, ZPtrArray *out);   // 0x425220 (indices via slot 2)
    DWORD THISCALL CurIndex(DWORD category);                            // 0x425290
    ItemDefinition *THISCALL CurDefinition(DWORD category);             // 0x4252b0
    CInventoryItem *THISCALL ChangingToItem(DWORD category);            // 0x4252e0
    float THISCALL SecondsToChange(DWORD category);                     // 0x425300
    float THISCALL SecondsIntoChange(DWORD category);                   // 0x425320
    DWORD THISCALL FirstOfCategory(DWORD category);                     // 0x425340
    DWORD THISCALL PrevOfCategory(DWORD from, DWORD category);          // 0x425350
    DWORD THISCALL NextOfCategory(DWORD from, DWORD category);          // 0x425470
    DWORD THISCALL IndexOfSameDefinition(CInventoryItem *item);         // 0x425740
    DWORD THISCALL IndexOfDefinitionIndex(DWORD definition);            // 0x4257a0
    int THISCALL TotalCount(DWORD definition);                          // 0x4257f0
    CInventoryItem *THISCALL FindOfClass(void *class_info);             // 0x425860
    CInventoryItem *THISCALL FindByDefinitionIndex(DWORD definition);   // 0x4258b0
    DWORD THISCALL IndexOfDefinition(ItemDefinition *definition);       // 0x425900
    CInventoryItem *THISCALL PrimaryItem();                             // 0x425950 (slot 27)
    CInventoryItem *THISCALL GetItem(DWORD i);                          // 0x425990 (slot 26)
    DWORD THISCALL ItemInFocus();                                       // 0x426070
    DWORD THISCALL RemoveAt(DWORD i);                                   // 0x426090
    DWORD THISCALL Remove(CInventoryItem *item);                        // 0x426260
    DWORD THISCALL IndexOf(CInventoryItem *item);                       // 0x4262a0
    DWORD THISCALL Repair();                                            // 0x4262e0
    DWORD THISCALL AnyItemSlot34(DWORD a, DWORD b);                     // 0x426600 (slot 28)
    DWORD THISCALL IsDiscovered(void *definition);                      // 0x426650
    DWORD THISCALL Undiscover(void *definition);                        // 0x426740
    DWORD THISCALL SetCurIndex(DWORD index, DWORD category);            // 0x4267f0

    DWORD set_cur(DWORD category, DWORD index, CInventoryItem *old, CInventoryItem *now);
    template <bool PREV> DWORD neighbour(DWORD from, DWORD category);
};
static_assert(offsetof(CInventory, items) == 0x18 && offsetof(CInventory, disposed) == 0x38 && sizeof(CInventory) == 0x48);

DWORD THISCALL LayerPart_GetInventory(void *part);                      // 0x4267e0 (slot 36 of the object)
int __fastcall Part_ItemCount(void *part, DWORD definition);            // 0x426860
int __fastcall Inventory_ItemCount(CInventory *inv, DWORD definition);  // 0x4268d0

constexpr DWORD INVENTORY_VTABLE = 0x5ee0c0;
constexpr DWORD ITEM_CATALOGUE_DATA = 0x6c0c38;    // records of 0x80 bytes, +0x78 the definition
constexpr DWORD ITEM_CATALOGUE_COUNT = 0x6c0c3c;
constexpr DWORD ITEM_CATEGORIES = 0x6c0800;        // resource list of the categories
constexpr DWORD INVENTORY_REPORT_REPAIR = 0x60b5d4;   // byte: report a repaired inventory

}  // namespace zax
