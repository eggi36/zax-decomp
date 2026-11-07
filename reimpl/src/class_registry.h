// CClassRegistry: the list of all classes that can be created by name (a ZNamedList, vtable 0x5ec8b0,
// 0x10 bytes, module 0x415930-0x415db0). One global instance 0x62e668, built on first use by its getter
// 0x415d20 and destroyed at exit (0x415d70). The static initializers of the classes add their class
// info (0x417690, 0x416400: Add with module 0).
//
// Entries are class infos; their name is +0x0c, an alternative name +0x08 and the DLL they come from
// +0x14. Add keeps one class per name: a new one replaces an existing one only with a higher version.
// Find searches the names (binary search once sorted, otherwise the last match wins), then the
// alternative names, then again with a "C" in front. Create (0x415c00) ends the game with a message box
// when the class is missing or abstract.
#pragma once
#include "class_info.h"

namespace zax {

struct CClassRegistry {   // a ZNamedList
    DWORD vftable;
    CClassInfo **entries;   // 0x04 operator new, exactly count entries
    DWORD num;              // 0x08
    BYTE sorted;            // 0x0c set by Sort, cleared by Add
    BYTE pad[3];

    DWORD THISCALL Count();                                          // 0x415930 (slot 22)
    CClassRegistry *THISCALL DeletingDtor(DWORD flags);              // 0x415940 (slot 0)
    void THISCALL Dtor();                                            // 0x415970
    const char *THISCALL Name(DWORD i);                              // 0x415a00 (slot 1)
    bool THISCALL Add(CClassInfo *cls, DWORD module);                // 0x415a20
    CClassInfo *THISCALL Find(const char *name, int *index);         // 0x415ad0
    CClassInfo *THISCALL FindChecked(const char *name, int *index);  // 0x415bc0
    void *THISCALL Create(const char *name, int a, int b);           // 0x415c00
    DWORD THISCALL Remove(DWORD i);                                  // 0x415c90 (slot 20)
    void THISCALL Sort();                                            // 0x415d80
};
static_assert(sizeof(CClassRegistry) == 0x10);

CClassRegistry *CClassRegistry_Get();                                // 0x415d20
void __cdecl CClassRegistry_AtExit();                                // 0x415d70
int __cdecl CClassRegistry_CompareNames(CClassInfo *const *a, CClassInfo *const *b);   // 0x415db0 (qsort)

constexpr DWORD CLASS_REGISTRY = 0x62e668;
constexpr DWORD CLASS_REGISTRY_GUARD = 0x62e680;   // bit 0: registry constructed
constexpr DWORD CLASS_REGISTRY_VTABLE = 0x5ec8b0;

}  // namespace zax
