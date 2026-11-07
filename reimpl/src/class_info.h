// CClassInfo: runtime class description of Zax.exe (name, parent class, size, factory, field
// descriptors for loading/saving and the editor). Every class has a class variable that its getter
// fills on first call (ClassInfo_Register 0x415780).
//
// Two levels as in the original:
//  - Base (0x40 bytes, vtable 0x5ec740, constructor 0x415110): name, version, parent, children,
//    fields, extra list. The vtable belongs to a generic named-list interface with 29 slots
//    (slot 1 = name of the i-th field, 22 = count, 27/28 = create, pure virtual).
//  - CClassInfo (0x48 bytes, vtable 0x5ec83c, constructor = Register): adds factory and array
//    factory.
#pragma once
#include "field_desc.h"
#include "ptr_array.h"

namespace zax {

// Extra list per class (a ZNamedList, 0x18 bytes, vtable 0x5f18a8, module 0x483500-0x4838d0). The
// entries are field descriptors (name = entry +4); they are passed on to subclasses on derivation
// (CClassInfo::PropagateExtra), and a list without the flag at +0x14 also adds every new entry once to
// the global list 0x6bdc78 (the flag set, created on first use, destroyed at exit by 0x483650). Only a
// list with the flag deletes its entries (slot 0 with flags 1); it also sets the byte 0x6bdc90.
struct CExtraList {
    DWORD vftable;
    ZPtrArray entries;   // 0x04 pool pointer array (vtable 0x5f1914)
    BYTE global;         // 0x14 the global list (owns its entries)
    BYTE pad[3];
    DWORD count() const { return vcall<DWORD>(this, 22); }
    void *entry(DWORD i) const { return entries.data[i]; }
    void append(void *e) { fn<void(THISCALL *)(CExtraList *, void *)>(0x4837d0)(this, e); }

    CExtraList *THISCALL Ctor(BYTE global);                  // 0x483500
    CExtraList *THISCALL DeletingDtor(DWORD flags);          // 0x483530 (slot 0)
    const char *THISCALL Name(DWORD i);                      // 0x483730 (slot 1, no check of i)
    DWORD THISCALL Count();                                  // 0x483750 (slot 22; generic getter of +0xc)
    void *THISCALL FindWithAltName(const char *name);        // 0x483760
    DWORD THISCALL Append(void *entry);                      // 0x4837d0
    void *THISCALL Entry(DWORD i);                           // 0x4838d0
};
static_assert(sizeof(CExtraList) == 0x18);
static_assert(offsetof(CExtraList, global) == 0x14);

void __cdecl CExtraList_GlobalDtor();                        // 0x483650 (atexit)

constexpr DWORD EXTRA_LIST_GLOBAL = 0x6bdc78;
constexpr DWORD EXTRA_LIST_GLOBAL_GUARD = 0x6bdc70;   // bit 0: global list constructed
constexpr DWORD EXTRA_LIST_DELETING = 0x6bdc90;       // set by the destructor of a global list

using CreateFn = void *(__fastcall *)(int, int);
using CreateArrayFn = void *(__fastcall *)(int, int, int);
using RegisterFieldsFn = void(__cdecl *)();

struct CClassInfo {
    DWORD vftable;           // 0x00
    CFieldDesc **fields;     // 0x04
    const char *alt_name;    // 0x08 alternative class name, searched by the registry (0 in the constructor)
    const char *name;        // 0x0c
    float version;           // 0x10 version, after registration a checksum over the field counts
    DWORD module;            // 0x14 DLL the class comes from (FreeLibrary), set by the registry (0 in the constructor)
    CClassInfo *parent;      // 0x18
    ZPtrArray children;      // 0x1c
    CExtraList *extra;       // 0x2c
    DWORD num_fields;        // 0x30
    DWORD num_inherited;     // 0x34
    BYTE is_abstract;        // 0x38
    BYTE pad39[3];
    DWORD size;              // 0x3c object size in bytes
    CreateFn create;         // 0x40
    CreateArrayFn create_array;  // 0x44

    CClassInfo *THISCALL BaseCtor(const char *name, float version, CClassInfo *parent, DWORD size);
    CClassInfo *THISCALL Register(CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                  DWORD size, bool is_abstract, CreateFn create, CreateArrayFn create_array,
                                  RegisterFieldsFn register_fields);
    void THISCALL PropagateExtra(void *entry);
    void THISCALL InheritFields();
    void THISCALL AddField(CFieldDesc *field);
    CFieldDesc *THISCALL LastField();
    bool THISCALL IsDerivedFrom(const CClassInfo *other);
    const char *THISCALL FieldName(DWORD i);
    DWORD THISCALL FieldCount();
    float THISCALL GetVersion();
    const char *THISCALL ShortName();
    CFieldDesc *THISCALL FindField(const char *name, int *index);
    void *THISCALL Create(int a, int b);
    void *THISCALL CreateArray(int a, int b, int c);
};
static_assert(sizeof(CClassInfo) == 0x48);
static_assert(offsetof(CClassInfo, children) == 0x1c && offsetof(CClassInfo, extra) == 0x2c);
static_assert(offsetof(CClassInfo, is_abstract) == 0x38 && offsetof(CClassInfo, create_array) == 0x44);

// CObject: root of almost all classes. Getter 0x4163b0, fields 0x416450.
CClassInfo *CObject_GetClassInfo();
void __cdecl CObject_RegisterFields();

constexpr DWORD CLASSVAR_COBJECT = 0x62e694;
constexpr DWORD FIND_FIELD_CACHE = 0x62e664;   // last found field + 1 (shortcut for FindField)

}  // namespace zax
