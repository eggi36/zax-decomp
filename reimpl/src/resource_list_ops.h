// The resource lists' own functions (0x523d00-0x523df0) and their alias array (0x55d0c0-0x55d1a0).
//
// A resource list is a ZRecordList of 0x80-byte records (one per resource of a type: name at the
// start, +0x78 the loaded object, +0x7c "keep" byte (slots 32/33), +0x7d "in use" byte (slot 29)),
// followed at +0x1c by an array of aliases (a pointer array, vtable 0x5ed558, heap flavour) of
// 8-byte entries {ZString name, index}. Slot 2 of the 39 resource list vtables is Find (0x523df0):
// the record list's search, then the aliases (no case). The game registers aliases at startup
// (0x5a01b0: "Electrical", "Laser", "Physical" for the damage type "Normal").
//
// The three walks over the records are called by the registry of the resource lists
// (resource_lists.cpp): every loaded resource (slot 8) gets the keep byte (0x523d00, after the game
// data is loaded), is unloaded (slot 31, 0x523d40), or is unloaded unless it is in use or kept
// (0x523d70, when a level ends).
#pragma once
#include "named_list.h"
#include "zstring.h"

namespace zax {

struct ZResourceAlias {
    ZString name;
    int index;
};
static_assert(sizeof(ZResourceAlias) == 8);

struct ZResourceAliases {   // a ZPtrArray, data ZResourceAlias *[]
    DWORD vftable;          // 0x5ed558
    ZResourceAlias **data;
    DWORD count;
    DWORD capacity;

    DWORD THISCALL Dtor();                                   // 0x55d0c0
    DWORD THISCALL Add(const char *name, int index);        // 0x55d150 (slot 2's result)
    int THISCALL Find(const char *name);                    // 0x55d1a0 (-1: none)
};
static_assert(sizeof(ZResourceAliases) == 16);

struct ZResourceList {
    ZRecordList records;         // vtable one of 39 with slot 2 = 0x523df0
    ZResourceAliases aliases;    // 0x1c

    DWORD THISCALL KeepLoaded();                             // 0x523d00
    DWORD THISCALL UnloadLoaded();                           // 0x523d40
    DWORD THISCALL UnloadUnused();                           // 0x523d70
    DWORD THISCALL AddAlias(const char *alias, const char *name);   // 0x523dc0
    int THISCALL Find(const char *s, int hint);              // 0x523df0 (slot 2)
};
static_assert(offsetof(ZResourceList, aliases) == 0x1c);

constexpr DWORD RESOURCE_ALIASES_VTABLE = 0x5ed558;

}  // namespace zax
