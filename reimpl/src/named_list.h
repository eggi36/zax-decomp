// ZNamedList: the named-list interface of Zax.exe. A list of entries with a name and an index;
// class infos (fields), directory lists, resource lists (one per resource type), enum name tables
// and the extra lists derive from it. 63 vtables share its code (module 0x591a90-0x5921f0).
//
// Interface (base vtable 0x5ec7cc, 4 bytes, slots 1, 2 and 22 pure):
//   1 name of entry i, 2 Find(name, hint), 3 FindOrFail (fatal error box), 4 name of the list and
//   5 its folder (for the error text), 6 "holds items", 7 item i, 10 slot 21 of item i, 14 the empty text,
//   18 returns -1, 22 count, 24 binary search, 25 binary search with fallback, 26 search by file
//   name (the part behind the last '/' or '\\').
// Every search returns the index or -1. The hint is checked first; Find falls back to the file-name
// search, so "Sounds/a.wav" also finds an entry "a.wav" and the other way round.
//
// Two concrete lists live in the same module:
//  - ZRecordList (vtable 0x6023dc, 0x1c bytes): a table of fixed-size records with the name at the
//    start of each record; base of the resource lists (41 vtables).
//  - ZEnumTable (vtables 0x6024b8 and 0x602524, base 0x60244c): static records {value, name,
//    alternative name}, terminated by a null name (e.g. "Left", "Center", "Right").
// plus ZListView (vtable 0x5f405c): a subset of another list through an array of indices.
#pragma once
#include "ptr_array.h"

namespace zax {

struct ZNamedList {
    DWORD vftable;

    const char *name(DWORD i) { return vcall<const char *>(this, 1, i); }
    int find(const char *s, DWORD hint) { return vcall<int>(this, 2, s, hint); }
    const char *list_name() { return vcall<const char *>(this, 4); }
    const char *folder() { return vcall<const char *>(this, 5); }
    bool has_items() { return vcall<bool>(this, 6); }
    void *item(DWORD i) { return vcall<void *>(this, 7, i); }
    DWORD count() { return vcall<DWORD>(this, 22); }
    int binary_search(const char *s, int hint) { return vcall<int>(this, 24, s, hint); }
    int find_file_name(const char *s) { return vcall<int>(this, 26, s); }

    ZNamedList *THISCALL DeletingDtor(DWORD flags);          // 0x415340 (slot 0 of the base)
    int THISCALL Find(const char *s, DWORD hint);            // 0x591bd0 (slot 2)
    int THISCALL FindOrFail(const char *s, int hint);        // 0x591fc0 (slot 3)
    DWORD THISCALL ItemValue(DWORD i);                       // 0x591a90 (slot 10)
    const char *THISCALL EmptyText();                        // 0x415250 (slot 14)
    int THISCALL NoIndex(DWORD);                             // 0x5921f0 (slot 18)
    int THISCALL BinarySearch(const char *s, int hint);      // 0x591b20 (slot 24)
    int THISCALL FindSorted(const char *s, int hint);        // 0x591ba0 (slot 25)
    int THISCALL FindFileName(const char *s);                // 0x591ac0 (slot 26)
};
static_assert(sizeof(ZNamedList) == 4);

struct ZRecordList {   // a ZNamedList
    DWORD vftable;
    char *data;        // 0x04 records, the name at the start of each
    DWORD unknown08;   // 0x08 (constructor argument 5)
    DWORD stride;      // 0x0c record size in bytes
    DWORD unknown10;   // 0x10 (0 in the constructor, set by slot 27 0x4d4a30)
    DWORD num;         // 0x14
    BYTE sorted;       // 0x18 Find uses the binary search
    BYTE pad[3];

    ZNamedList *as_list() { return reinterpret_cast<ZNamedList *>(this); }
    ZRecordList *THISCALL Ctor(BYTE sorted, DWORD stride, DWORD num, char *data, DWORD unknown08);   // 0x591c90
    DWORD THISCALL Set(BYTE sorted, DWORD stride, DWORD num, char *data, DWORD unknown08);          // 0x591cd0
    const char *THISCALL Name(DWORD i);                      // 0x591d00 (slot 1)
    int THISCALL Find(const char *s, int hint);              // 0x591d30 (slot 2)
    DWORD THISCALL Count();                                  // 0x591dd0 (slot 22, also of ZEnumTable)
    ZRecordList *THISCALL DeletingDtor(DWORD flags);         // 0x591f90 (also of ZEnumTable 0x602524)
};
static_assert(sizeof(ZRecordList) == 0x1c);
static_assert(offsetof(ZRecordList, num) == 0x14 && offsetof(ZRecordList, sorted) == 0x18);

struct ZEnumTable {   // a ZNamedList
    DWORD vftable;
    const char **names;       // 0x04 &table[1]
    const char **alt_names;   // 0x08 &table[2] or 0
    DWORD *table;             // 0x0c {value, name, alternative name}... (0 for 0x602524)
    DWORD stride;             // 0x10 in dwords, always 3
    DWORD num;                // 0x14 records before the null name
    DWORD *source;            // 0x18 only 0x602524: the table

    ZNamedList *as_list() { return reinterpret_cast<ZNamedList *>(this); }

    ZEnumTable *THISCALL Ctor(DWORD *table);                 // 0x591ee0 (vtable 0x6024b8, 0x18 bytes)
    ZEnumTable *THISCALL CtorNoAlt(DWORD *table);            // 0x591f40 (vtable 0x602524, 0x1c bytes)
    const char *THISCALL Name(DWORD i);                      // 0x591e10 (slot 1)
    int THISCALL Find(const char *s, int hint);              // 0x591e30 (slot 2)
    ZEnumTable *THISCALL DeletingDtor(DWORD flags);          // 0x591de0 (0x18 bytes)
};
static_assert(sizeof(ZEnumTable) == 0x1c);
static_assert(offsetof(ZEnumTable, num) == 0x14 && offsetof(ZEnumTable, source) == 0x18);

struct ZListView {   // a ZNamedList
    DWORD vftable;
    ZNamedList *list;     // 0x04
    ZPtrArray *indices;   // 0x08 entry i of the view is entry indices->data[i] of the list

    const char *THISCALL Name(DWORD i);                      // 0x591c60 (slot 1)
    DWORD THISCALL Count();                                  // 0x591c40 (slot 22)
};

constexpr DWORD NAMED_LIST_VTABLE = 0x5ec7cc;
constexpr DWORD EMPTY_TEXT = 0x62e558;         // "", used as the empty string all over the exe
constexpr DWORD LAST_FILE_OPENED = 0x713c14;   // ZString, named in error messages

}  // namespace zax
