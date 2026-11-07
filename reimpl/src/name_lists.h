// Two helpers built on the named-list interface (see named_list.h):
//
// The name tree (0x47e560-0x47ea20; called DefinitionList_Monitor_Ctor before): a folder tree over the names
// of a named list, made by every definition list's Init and by DirList_Read (17 callers) and kept at +0x13c
// of the definition lists. The constructor splits every name i of the list (slot 1) at its last '/' and
// files the index under the folder node of that path (created on the way, compared with _strnicmp); names
// without a folder go to the root. Its vtable 0x5f154c (base 0x5f1578, pure) is a tree model whose items
// are entry indices (below the list's count) or node pointers: slot 0 the root (0x47e910, not hookable),
// 1 the folder of item i, 2 the item's name (the file name of an entry, the node's name), 3 the number of
// children (the node's entries first, then its child nodes), 4 child k, 7 the deleting destructor.
//
// The name lists of the number field kinds (0x54f390-0x54fa70; FieldNameList in field_kinds.h): a named
// list {vtable, pool pointer array} of entries {char *name (operator new), value} of 8 bytes (int and float
// kinds, vtables 0x5fd5d8, 0x5fdaec and five more lists) or 12 bytes (double kind, 0x5fdc28). Slots: 1 the
// name of entry i, 2 find (hint first, then all entries with _stricmp, then by file name 0x591ac0), 17
// remove all, 18 add, 20 remove at, 21 swap, 0 the deleting destructors.
//
// Return values are what the original leaves in eax.
#pragma once
#include "named_list.h"

namespace zax {

struct ZNameTreeNode {
    ZString name;                // 0x00
    ZPtrArray children;          // 0x04 heap pointer array (vtable 0x5f156c) of nodes (operator new, 0x28)
    ZPtrArray entries;           // 0x14 heap array (vtable 0x5ee168) of entry indices
    ZNameTreeNode *parent;       // 0x24

    DWORD THISCALL Dtor();                                   // 0x47e750
    ZNameTreeNode *THISCALL FindOrMake(const char *path);    // 0x47e800
};
static_assert(sizeof(ZNameTreeNode) == 0x28);

struct ZNameTree {
    DWORD vftable;          // 0x00 0x5f154c
    void *list;             // 0x04 the named list
    DWORD count;            // 0x08 its count (slot 22) at construction
    ZNameTreeNode root;     // 0x0c named after the list (slot 4)

    ZNameTree *THISCALL Ctor(void *list);                     // 0x47e560
    ZNameTree *THISCALL DeletingDtor(DWORD flags);            // 0x47e670 (slot 7, operator delete)
    ZNameTreeNode *THISCALL FolderOf(DWORD item);             // 0x47e920 (slot 1)
    const char *THISCALL ItemName(DWORD item);                // 0x47e9a0 (slot 2)
    DWORD THISCALL ChildCount(DWORD item);                    // 0x47e9d0 (slot 3)
    DWORD THISCALL Child(DWORD item, DWORD k);                // 0x47e9f0 (slot 4)
};
static_assert(sizeof(ZNameTree) == 0x34);

struct ZNameEntry {
    char *name;
    DWORD value;
    DWORD value2;   // 12-byte entries only

    DWORD THISCALL FreeName();                                // 0x5266e0
};

template <DWORD SIZE> struct ZFieldNames {   // FieldNameList with entries of SIZE bytes
    DWORD vftable;
    ZPtrArray entries;

    const char *THISCALL Name(DWORD i);                       // 0x54f530 (slot 1)
    int THISCALL Find(const char *name, int hint);            // 0x54f590 (slot 2)
    DWORD THISCALL RemoveAll();                               // 0x54f400, 0x54f600 (slot 17)
    DWORD THISCALL Add(const char *name);                     // 0x54f470, 0x54f670 (slot 18)
    DWORD THISCALL RemoveAt(DWORD i);                         // 0x54f4d0, 0x54f6d0 (slot 20)
    DWORD THISCALL Swap(DWORD i, DWORD j);                    // 0x54f390 (slot 21)
    template <DWORD VT, DWORD INNER_VT> ZFieldNames *THISCALL DeletingDtor(DWORD flags);   // 0x54f830, 0x54f950, 0x54fa70
};

constexpr DWORD NAME_TREE_VTABLE = 0x5f154c, NAME_TREE_BASE_VTABLE = 0x5f1578;
constexpr DWORD NAME_TREE_CHILDREN_VTABLE = 0x5f156c, NAME_TREE_ENTRIES_VTABLE = 0x5ee168;

}  // namespace zax
