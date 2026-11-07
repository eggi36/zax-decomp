// Small data definition classes of Zax.exe and the definition lists that load them:
// CCategoryDefinition (0x46a240-0x46a3d0) with the list of the entity categories (0x46a490-0x46ae50,
// global 0x6bd140), CDamageTypeDefinition (0x476ee0-0x477060) with the list of the damage types
// (0x4770f0-0x477890, global 0x6bd760), CHueRemapHolder (0x4d44c0-0x4d4a40) and CQuipCategory
// (0x55b290-0x55b7d0).
//
// Every class starts with the CObject part {vtable, dynamic properties} and has the usual set (see
// broadcast.h): the class getter (slot 2), Create, CreateArray, the deleting destructor (slot 1) and
// RegisterFields. Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CCategoryDefinition (0x18, vtable 0x5ef684, class 0x6bd280, version 1, parent CObject):
//     +8 "MiniMap Color" (colour, 0x10 bytes; constructor 0x53f000; default 255,255,255,255)
//   CDamageTypeDefinition (0xc, vtable 0x5f0980, class 0x6bd8ac, version 1, parent CObject):
//     +8 "Unavoidable damage" (byte, 0 from Create and as the default)
//   CHueRemapHolder (0x20, vtable 0x5f7c10, class 0x6c0528, version 2, parent CObject): six floats
//     "Source Hue 1" +8 (268), "New Hue 1" +0xc (0), "Error 1" +0x10 (8), "Source Hue 2" +0x14 (160),
//     "New Hue 2" +0x18 (180), "Error 2" +0x1c (4); hues 0-360, errors 0-100, step 0.5; each with a
//     setter that stores the dword (0x4d49f0-0x4d4a40, shared with other classes by identical code)
//   CQuipCategory (0x20, vtable 0x5fe7f0, class 0x7124b8, version 1, parent CObject):
//     +8 "Delay Before Saying" (1.5 s), +0xc "Minimum Time Between Saying" (30 s), +0x10 "Condition"
//     (owned object), +0x14 and +0x18 two timers (0 from Create), +0x1c the sounds (a ZDirList of
//     0x12c bytes, owned; deleted through slot 0 by the destructor)
//
// Definition list (0x140 bytes; a resource list, see resource_list_ops.h): records of 0x80 bytes (name,
// +0x78 the loaded object, +0x7c keep, +0x7d in use, +0x7f "loading"), one per .TXT file of a folder:
//   +0x2c index in the registry of the resource lists, +0x30 the records (operator new, count in front),
//   +0x34 their count, +0x38 the folder (char[0x100]), +0x138 the list's name, +0x13c an object of
//   0x34 bytes made by 0x47e560 from the list (the name tree, name_lists.h; deleted through its slot 7).
// The globals are built by static initializers (0x46a760, 0x4770f0: vtables 0x5ef6e0, 0x5f09dc) and
// destroyed at exit (0x46a7d0, 0x477160); the deleting destructor (slot 0) writes the base vtable
// (0x5ef784, 0x5f0a80) first. Slots of these lists in this module: 4 the name, 17 (jumps to slot 40),
// 30 load one entry, 34 the element class, 35 the file of an object, 36 (0), 37 create an entry from
// its file, 39 the path of entry i; Init (0x46a940, 0x477220) fills the list from the folder.
//
// Return values are what the original leaves in eax. Where that is only al, the upper bytes are
// reproduced from the call before; where it is undefined, the comment says so.
#pragma once
#include "object_base.h"
#include "field_kinds.h"
#include "resource_list_ops.h"

namespace zax {

struct CCategoryDefinition : CObjectBase {
    FieldColor minimap_color;   // 0x08

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x46a2f0 (slot 1)
};
static_assert(sizeof(CCategoryDefinition) == 0x18);

struct CDamageTypeDefinition : CObjectBase {
    BYTE unavoidable;   // 0x08
    BYTE pad[3];

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x476f80 (slot 1)
};
static_assert(sizeof(CDamageTypeDefinition) == 0xc);

struct CHueRemapHolder : CObjectBase {
    DWORD source_hue1, new_hue1, error1;   // 0x08 floats
    DWORD source_hue2, new_hue2, error2;   // 0x14

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x4d45a0 (slot 1)
    // The setters of the six fields (thiscall (value), eax = the value)
    template <DWORD OFF> DWORD THISCALL Set(DWORD v);
};
static_assert(sizeof(CHueRemapHolder) == 0x20);

struct CQuipCategory : CObjectBase {
    float delay;            // 0x08 "Delay Before Saying"
    float min_time;         // 0x0c "Minimum Time Between Saying"
    void *condition;        // 0x10
    float wait;             // 0x14 time left until the category may speak again [assumption]
    float pending;          // 0x18 delay of a quip about to be said [assumption]
    void *sounds;           // 0x1c ZDirList *

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x55b350 (slot 1)
    DWORD THISCALL Retrigger();                 // 0x55b730 (eax undefined)
    DWORD THISCALL TryStart();                  // 0x55b770 (al)
    DWORD THISCALL ReadSounds(const char *folder);   // 0x55b7d0
};
static_assert(sizeof(CQuipCategory) == 0x20);

struct ZDefinitionRecord {
    char name[0x78];
    void *object;    // 0x78
    BYTE keep;       // 0x7c
    BYTE in_use;     // 0x7d
    BYTE b7e;        // 0x7e
    BYTE loading;    // 0x7f
};
static_assert(sizeof(ZDefinitionRecord) == 0x80);

struct ZDefinitionList : ZResourceList {
    DWORD registry_index;         // 0x2c
    ZDefinitionRecord *entries;   // 0x30
    DWORD num_entries;            // 0x34
    char folder[0x100];           // 0x38
    const char *list_name;        // 0x138
    void *monitor;                // 0x13c the 0x34-byte object of 0x47e560

    template <DWORD BASE_VT> ZDefinitionList *THISCALL DeletingDtor(DWORD flags);   // 0x46a890, 0x477700 (slot 0)
    template <DWORD NAME, DWORD FOLDER> DWORD THISCALL Init();                      // 0x46a940, 0x477220
    const char *THISCALL ListName();                                               // 0x46acb0 (slot 4)
    DWORD THISCALL Slot17();                                                       // 0x46aca0 (slot 17)
    void *THISCALL LoadEntry(DWORD i);                                             // 0x4777b0 (slot 30)
    template <CClassInfo *(__cdecl *GETTER)()> DWORD THISCALL FileOfObject(void *object, ZString *out);   // slot 35
    DWORD THISCALL Slot36(DWORD, DWORD);                                           // 0x46ad10 (slot 36)
    void *THISCALL CreateCategory(const char *dir, const char *name, void **out);  // 0x46ac30 (slot 37)
    void *THISCALL CreateFromFile(const char *dir, const char *name, void **out);  // 0x46ad20 (slot 37)
    DWORD THISCALL EntryPath(DWORD i, ZString *out);                               // 0x46ae50 (slot 39)
};
static_assert(sizeof(ZResourceList) == 0x2c);   // registry_index follows at 0x2c
static_assert(sizeof(ZDefinitionList) == 0x140);

constexpr DWORD CATEGORY_LIST = 0x6bd140, DAMAGE_TYPE_LIST = 0x6bd760;
constexpr DWORD CATEGORY_LIST_VTABLE = 0x5ef6e0, CATEGORY_LIST_BASE_VTABLE = 0x5ef784;
constexpr DWORD DAMAGE_LIST_VTABLE = 0x5f09dc, DAMAGE_LIST_BASE_VTABLE = 0x5f0a80;
constexpr DWORD RESOURCE_LIST_VTABLE = 0x5ed564;   // written by the destructors before the aliases go

}  // namespace zax
