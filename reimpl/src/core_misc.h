// The remaining small functions of the object system:
//  - CClassInfo: the destructors of both levels (0x4152a0 and the deleting destructors 0x415270/0x415820;
//    they only run at exit), the display name 0x4153b0 (the class name without a leading "C" before an
//    upper-case letter and without a given suffix, used by the editor texts of actions), the static
//    registration stub 0x417690 (getter, then the class registry's Add) and the field descriptor setters
//    0x419020 (help), 0x419030 (alternative name), 0x419040 (inherited flag), 0x419060 (units).
//  - Field value helpers of three kinds whose value may live in the object's dynamic properties: the
//    string kind's copy constructor 0x415ef0, value pointer 0x415f50 and set from text 0x415f80, the int
//    kind's 0x54d630/0x54d660 and the float kind's 0x54ede0/0x54ee10 (get: the dynamic value or the
//    default; set: the setter when there is one, else the dynamic value); 0x54d440 (a zeroed array of n
//    dwords, slot of two vtables).
//  - Class getters whose parent getter is inlined: CItemTypeData 0x4242b0 (with its deleting destructor
//    0x424230), CMultiPlayerGameData 0x51bf50, the eight quest statistics classes 0x5557f0-0x55aa30 (each
//    with the abstract CQuestStatData or CQuestStatParScoreData registered on the way), the cast to
//    CSphereOfZinData 0x585380 and the trigger array's deleting destructor 0x5853f0, CHueRemapHolder's
//    constructor 0x4d4cc0.
//  - The base levels of the definition lists: the resource list's deleting destructor 0x41e830, the
//    definition list base's 0x41e940 (objects deleted through slot 0) and its unload of one entry 0x41ec30.
//  - 0x47e540: three ZStrings destroyed (+0xc, +8, +0).
//
// Return values are what the original leaves in eax; where it is undefined the comment says so.
#pragma once
#include "class_info.h"
#include "field_kinds.h"
#include "named_list.h"

namespace zax {

struct CClassInfoDtors {
    static DWORD THISCALL BaseDtor(CClassInfo *self);                                       // 0x4152a0 (eax undefined)
    static CClassInfo *THISCALL BaseDeletingDtor(CClassInfo *self, DWORD flags);            // 0x415270
    static CClassInfo *THISCALL DeletingDtor(CClassInfo *self, DWORD flags);                // 0x415820
    static DWORD THISCALL DisplayName(CClassInfo *self, ZString *out, const char *suffix);  // 0x4153b0
    static void *THISCALL StaticRegister(void *self, CClassInfo *(__cdecl *getter)());      // 0x417690
};

struct CFieldDescSetters {
    static ZString *THISCALL SetHelp(CFieldDesc *d, const char *s);       // 0x419020
    static ZString *THISCALL SetAltName(CFieldDesc *d, const char *s);    // 0x419030
    static void THISCALL SetInherited(CFieldDesc *d);                     // 0x419040 (eax the caller's)
    static ZString *THISCALL SetUnits(CFieldDesc *d, const char *s);      // 0x419060
};

struct CFieldValues {
    static CStringFieldDesc *THISCALL StringCopyCtor(CStringFieldDesc *self, const CStringFieldDesc *other);   // 0x415ef0
    static ZString *THISCALL StringValue(CStringFieldDesc *self, void *object);                // 0x415f50
    static DWORD THISCALL StringSetText(CStringFieldDesc *self, void *object, const char *s);  // 0x415f80
    static DWORD THISCALL IntGet(CIntFieldDesc *self, void *object);                           // 0x54d630
    static DWORD THISCALL IntSet(CIntFieldDesc *self, void *object, DWORD v);                  // 0x54d660
    static float THISCALL FloatGet(CFloatFieldDesc *self, void *object);                       // 0x54ede0
    static DWORD THISCALL FloatSet(CFloatFieldDesc *self, void *object, DWORD v);              // 0x54ee10 (v: float bits)
    static DWORD *THISCALL NewZeroedArray(void *self, DWORD n, DWORD, DWORD);                  // 0x54d440
};

CClassInfo *__cdecl CItemTypeData_GetClassInfo();          // 0x4242b0
CClassInfo *__cdecl CMultiPlayerGameData_GetClassInfo();   // 0x51bf50
template <DWORD VAR> CClassInfo *__cdecl Quest_GetClassInfo();   // 0x5557f0-0x55aa30
void *__fastcall SphereOfZinData_Cast(void *object);        // 0x585380

struct CMiscDtors {
    static void *THISCALL ItemTypeDataDeletingDtor(void *self, DWORD flags);       // 0x424230 (single and array)
    static void *THISCALL TriggerArrayDeletingDtor(void *self, DWORD flags);       // 0x5853f0
    static void *THISCALL HueRemapHolderCtor(void *self);                          // 0x4d4cc0
    static void *THISCALL ResourceListDeletingDtor(void *self, DWORD flags);       // 0x41e830
    static void *THISCALL DefinitionListBaseDeletingDtor(void *self, DWORD flags); // 0x41e940
    static DWORD THISCALL DefinitionListBaseUnload(void *self, DWORD i);           // 0x41ec30
    static DWORD THISCALL StringTripleDtor(ZString *self);                         // 0x47e540 (eax undefined)
};

}  // namespace zax
