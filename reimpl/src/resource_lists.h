// The rest of the file area 0x4a3480-0x4a3c30: the registry of the resource lists and the file name
// list.
//
// Registry (vtable 0x5f4198, 0x18 bytes, one global instance 0x6be088 built on first use by 0x4a35d0
// and reset at exit by 0x4a3630): a named list (slot 1 = name of entry i) over an array of 16-byte
// entries {name, resource list, value, flag byte} (array vtable 0x5f4204, data from operator new).
// The first Add registers 0x4a3690 with the global callback list 0x62e6a8 (0x419ea0, flag 0x613854);
// 0x4a3690 asks every list (slot 35) and stops at the first that answers true. The other methods call
// a slot or a function of every list: slot 28 (0x4a3730), 0x523d70 for the flagged ones (0x4a3760),
// 0x523d40 from the last to the first (0x4a37a0), 0x523d00 after setting +0x14 (0x4a37d0).
//
// File name list (vtable 0x5f4210, 0x14 bytes): a file sink (slot 0 count 0x4a3990, 1 Add, 2
// Contains, 3 deleting destructor) over a pointer array (vtable 0x5f4220) of 12-byte records
// {ZString name, size, DOS date, DOS time} from operator new. Search empties it and runs the file
// search (0x4a1190) into it.
#pragma once
#include "file_search.h"

namespace zax {

struct ZResourceListEntry {
    const char *name;
    void *list;     // a resource list (named-list interface)
    DWORD value;
    BYTE flag;      // the bytes behind it are never written
    BYTE pad[3];
};
static_assert(sizeof(ZResourceListEntry) == 16);

struct ZResourceListRegistry {
    DWORD vftable;                // 0x5f4198
    DWORD array_vftable;          // 0x04 0x5f4204
    ZResourceListEntry *data;     // 0x08
    DWORD count;                  // 0x0c
    DWORD capacity;               // 0x10
    BYTE marked;                  // 0x14 set by MarkAll

    ZResourceListRegistry *THISCALL DeletingDtor(DWORD flags);                    // 0x4a3480 (slot 0)
    DWORD THISCALL Add(const char *name, void *list, DWORD value, BYTE flag);     // 0x4a34d0 (the old count)
    const char *THISCALL Name(DWORD i);                                          // 0x4a3670 (slot 1)
    DWORD THISCALL CallAll28();                                                  // 0x4a3730
    DWORD THISCALL CallFlagged();                                                // 0x4a3760
    DWORD THISCALL CallAllReverse();                                             // 0x4a37a0
    DWORD THISCALL MarkAll();                                                    // 0x4a37d0
    DWORD THISCALL SetFlag(void *list, BYTE flag);                               // 0x4a3800
};
static_assert(sizeof(ZResourceListRegistry) == 0x18);

ZResourceListRegistry *__cdecl ResourceLists_Get();             // 0x4a35d0
DWORD __cdecl ResourceLists_AtExit();                            // 0x4a3630
DWORD __fastcall ResourceLists_Ask(DWORD a, DWORD b);            // 0x4a3690 (al)

struct ZFileListRecord {
    ZString name;
    DWORD size;
    WORD date, time;
};
static_assert(sizeof(ZFileListRecord) == 12);

struct ZFileNameList {
    DWORD vftable;    // 0x5f4210
    ZPtrArray names;  // 0x04 vtable 0x5f4220, data ZFileListRecord *[]

    ZFileNameList *THISCALL Ctor();                                         // 0x4a3970
    ZFileNameList *THISCALL DeletingDtor(DWORD flags);                      // 0x4a39a0 (slot 3)
    DWORD THISCALL Dtor();                                                  // 0x4a3a50
    DWORD THISCALL Search(const ZFileSpec *spec);                           // 0x4a3ae0
    ZString *THISCALL GetName(ZString *out, DWORD i);                       // 0x4a3b60
    DWORD THISCALL Add(const char *name, const ZFileRecord *record);        // 0x4a3b80 (slot 1)
    DWORD THISCALL Contains(const char *name);                              // 0x4a3bf0 (slot 2, al)
};
static_assert(sizeof(ZFileNameList) == 0x14);

constexpr DWORD RESOURCE_LISTS = 0x6be088, RESOURCE_LISTS_GUARD = 0x6be080, RESOURCE_LISTS_FIRST_ADD = 0x613854;
constexpr DWORD RESOURCE_LISTS_VTABLE = 0x5f4198, RESOURCE_LISTS_ARRAY_VTABLE = 0x5f4204, NAMED_LIST_VTABLE = 0x5ec7cc;
constexpr DWORD FILE_NAME_LIST_VTABLE = 0x5f4210, FILE_NAME_ARRAY_VTABLE = 0x5f4220;

}  // namespace zax
