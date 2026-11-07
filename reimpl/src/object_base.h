// The base methods of CObject (vtable 0x5ec9ec, 8 bytes: vtable and the dynamic property list +4), the
// object functions around them, the dynamic property accessors of the field descriptors, the value
// block slots of the descriptor base class, the ZString arrays and three static pointer lists.
//
// CObject vtable 0x5ec9ec: 1 deleting destructor (0x416650), 2 class info (0x4163b0), 7 precache:
// slot 32 of every field (0x4167b0; overrides load sounds, e.g. CPlayerWalkingControlAI and
// CZaxDialogAction) [assumption on the name], 11 own lines (0x417500), 19 every field's default (slot
// 10, 0x417230), 20 copy from another object (0x417420), 21 the class's object size (0x4174f0). Slots
// 16 and 18 are 0x401270 and 0x401110 in the base.
//
// Constructor 0x416610: vtable, no dynamic properties, slot 6 (init) of CObject's own fields (from
// num_inherited to num_fields, the count taken once). The derived constructors call 0x4173c0 (the
// same for the class of the object, slot 2) and their destructors 0x4173f0 (slot 8 (free), the count
// read again each time); the destructor 0x4166d0 deletes the dynamic properties (0x4828b0, pool 0x10)
// and frees CObject's fields the same way; 0x416650 then gives the 8 bytes back to the pool.
//
// Slot 20 (0x417420, copy from src): the classes are compatible if one derives from the other, else
// the source's class is replaced by its parents (+0x18) until this object's class derives from one
// (none: nothing happens). Then slot 4 (dst, src, descriptor) of the first min(field counts) of this
// object's descriptors, and slot 10 (default) of the rest.
//
// Slot 11 (0x417500, name, value, reader): the name is looked up in the class's extra list (with the
// alternative names, 0x483760); found, with a value other than "" and "0": a dynamic property for that
// descriptor (0x418fa0 -> 0x417260) and its slot 19 with the value. Otherwise the block behind the line
// is skipped: an optional class name line (starting with 'C' and without '='), then "{" ... "}"
// counted with nesting; without "{" the reader goes back to where it was.
//
// Dynamic properties (+4, ZDynamicProperties of field_slots.h, created on first use from the pool):
// 0x417260 add (0x482a70: 0 if an entry with a value exists), 0x417300 remove (0x482b80), 0x417320 get
// (0x482940), 0x417340 get or add (0x482970), 0x417390 has. The descriptor side: 0x418fa0/0x418fb0/
// 0x418fc0 (add, remove, has for an object), 0x418790 the pointer at the descriptor's offset in the
// value, 0x4187c0 set it (through the setter +0x38 if there is one, else the old object deleted), 0x418810
// take it (cleared), 0x418fd0 and 0x4190e0 give the descriptor to a class (PropagateExtra; 0x4190e0 also
// sets the help text and returns this).
//
// Descriptor base slots: 35 a value block from the pool (size +0x14), 36 back to the pool, 38 an array
// from operator new (size * n), 43/46 return 0. ZString kind slots 38/40 (0x416240/0x416290): new[]
// and delete[] of ZStrings with the count in front.
//
// Static lists (0x419ea0, 0x419f00, 0x419f60; thiscall with the item in ecx): the pointer array at
// 0x62e6a8 (vtable 0x5ece2c), 0x62e6d0 or 0x62e6c0 (vtable 0x5ece38) is built on first use (guard bit 0
// of 0x62e6b8, 0x62e6e0, 0x62e6a0; atexit 0x419de0, 0x419e20, 0x419e60), then slot 2 appends the item.
//
// Return values are the original's eax (mostly the last call's or a count the loops leave in eax);
// documented where they are not reproduced.
#pragma once
#include "field_slots.h"
#include "class_info.h"
#include "file_buffer.h"

namespace zax {

struct CObjectBase {
    DWORD vftable;
    ZDynamicProperties *dynamic;   // 0x04

    static CObjectBase *THISCALL Ctor(CObjectBase *self);                                   // 0x416610
    static CObjectBase *THISCALL DeletingDtor(CObjectBase *self, DWORD flags);              // 0x416650 (slot 1)
    static DWORD THISCALL Dtor(CObjectBase *self);                                          // 0x4166d0
    static void *THISCALL CreateSame(CObjectBase *self, DWORD a, DWORD b);                  // 0x416740
    static DWORD THISCALL IsKindOf(CObjectBase *self, CClassInfo *cls);                     // 0x416790
    static DWORD THISCALL Precache(CObjectBase *self);                                  // 0x4167b0 (slot 7)
    static DWORD THISCALL Extra(CObjectBase *self);                                         // 0x417220
    static DWORD THISCALL ResetFields(CObjectBase *self);                                   // 0x417230 (slot 19)
    static void *THISCALL AddDynamic(CObjectBase *self, CFieldDesc *d);                     // 0x417260
    static DWORD THISCALL RemoveDynamic(CObjectBase *self, CFieldDesc *d);                  // 0x417300
    static void *THISCALL GetDynamic(CObjectBase *self, CFieldDesc *d);                     // 0x417320
    static void *THISCALL GetOrAddDynamic(CObjectBase *self, CFieldDesc *d);                // 0x417340
    static DWORD THISCALL HasDynamic(CObjectBase *self, CFieldDesc *d);                     // 0x417390
    static DWORD THISCALL InitFields(CObjectBase *self);                                    // 0x4173c0
    static DWORD THISCALL FreeFields(CObjectBase *self);                                    // 0x4173f0
    static DWORD THISCALL Assign(CObjectBase *self, CObjectBase *src);                      // 0x417420 (slot 20)
    static DWORD THISCALL Size(CObjectBase *self);                                          // 0x4174f0 (slot 21)
    static DWORD THISCALL ReadOwnLine(CObjectBase *self, const char *name, char *value, CFileBuf *r);   // 0x417500 (slot 11)
    static DWORD THISCALL SetFieldFromText(CObjectBase *self, const char *name, char *value, CFileBuf *r);   // 0x417660
};
static_assert(sizeof(CObjectBase) == 8);

struct CFieldDescDynamic {
    static void *THISCALL AddTo(CFieldDesc *self, CObjectBase *object);                     // 0x418fa0
    static DWORD THISCALL RemoveFrom(CFieldDesc *self, CObjectBase *object);                // 0x418fb0
    static DWORD THISCALL IsIn(CFieldDesc *self, CObjectBase *object);                      // 0x418fc0
    static void THISCALL AddToClass(CFieldDesc *self, CClassInfo *cls);                     // 0x418fd0
    static void *THISCALL Register(void *self, CFieldDesc *d, CClassInfo *cls, const char *help);   // 0x4190e0
    static DWORD THISCALL GetPointer(CFieldDesc *self, CObjectBase *object);                // 0x418790
    static DWORD THISCALL SetPointer(CObjectFieldDesc *self, CObjectBase *object, void *value);   // 0x4187c0
    static DWORD THISCALL TakePointer(CFieldDesc *self, CObjectBase *object);               // 0x418810
    static void *THISCALL NewValue(CFieldDesc *self, DWORD a, DWORD b);                     // 0x419070 (slot 35)
    static DWORD THISCALL DeleteValue(CFieldDesc *self, void *p);                           // 0x419090 (slot 36)
    static void *THISCALL NewValues(CFieldDesc *self, DWORD n, DWORD a, DWORD b);           // 0x4190b0 (slot 38)
    static DWORD THISCALL Zero(CFieldDesc *self, DWORD a, DWORD b, DWORD c, DWORD d);       // 0x4190d0 (slots 43/46)
    static ZString *THISCALL NewStrings(CFieldDesc *self, DWORD n, DWORD a, DWORD b);                          // 0x416240 (ZString kind slot 38)
    static DWORD THISCALL DeleteStrings(CFieldDesc *self, ZString *p);                       // 0x416290 (ZString kind slot 40)
    static DWORD THISCALL AddToList1(void *item);                                           // 0x419ea0
    static DWORD THISCALL AddToList2(void *item);                                           // 0x419f00
    static DWORD THISCALL AddToList3(void *item);                                           // 0x419f60
};

constexpr DWORD VT_COBJECT = 0x5ec9ec;
constexpr DWORD COBJECT_CLASS_GETTER = 0x5ec9f4;   // slot 2 of 0x5ec9ec, called directly by the constructor
constexpr DWORD TEXT_ZERO = 0x60a8b0;              // "0": a dynamic property line with this value is skipped

// The three static lists: {list, guard, guard bit, vtable, atexit function}
struct StaticList {
    GlobKey list, guard;
    VtKey vtable;
    CodeKey dtor;
};
constexpr StaticList STATIC_LISTS[3] = {{0x62e6a8, 0x62e6b8, 0x5ece2c, 0x419de0},
                                        {0x62e6d0, 0x62e6e0, 0x5ece38, 0x419e20},
                                        {0x62e6c0, 0x62e6a0, 0x5ece38, 0x419e60}};

}  // namespace zax
