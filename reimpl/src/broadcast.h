// Small object infrastructure classes of Zax.exe: the broadcaster with its listener references
// (0x4674f0-0x467ee0), the update monitor list with its entries (0x4e4830-0x4e5090), the child list
// (0x4f97e0-0x4f9b70) and the broadcaster helpers of the layer parts and the AI objects.
//
// Every class here starts with the CObject part {vtable, dynamic properties} (8 bytes). The pieces of
// CObject used here stay in the exe (range 0x416000-0x419fff, another batch) and are called through
// their addresses: 0x416610 constructor (vtable 0x5ec9ec, +4 = 0, slot 6 of CObject's own fields),
// 0x4173c0 slot 6 ("init") of the fields of the object's own class (from its slot 2), 0x4173f0 slot 8
// ("free") of those fields, 0x4166d0 destructor (dynamic properties freed, slot 8 of CObject's fields).
// Slot 0 of every CObject is the message handler (sender, message text); CObject's own (0x46cd50) does
// nothing. Slot 1 is the deleting destructor: flags bit 0 frees, bit 1 is the array form (the count
// in the dword before the first element, block from operator new). Slot 2 is the class getter.
//
// Every class has the usual set: the class getter (slot 2; registers the class on first use through
// ClassInfo_Register 0x415780), Create (pool block + constructor), CreateArray (fastcall, count in ecx,
// one unused stack argument; heap block with the count in front) and RegisterFields (the field
// descriptors, each added through ClassInfo_AddField 0x415700; their own copy of the getter inlined).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CListenerReference (0x14, vtable 0x5ef248, class 0x6bced8, version 0, parent CObject):
//     +8 "Listener" (object reference), +0xc "Member Offset" (-1 from every caller checked), +0x10
//     "Member Name" (ZString, empty from every caller checked). The constructor leaves +0xc as it was
//     (the block's old content), as CUpdateMonitorInfo's leaves +0xc/+0x10.
//   CBroadcaster (0x28, vtable 0x5ef2a4, class 0x6bcedc, version 2, no parent class):
//     +0x08 "Objects Listening": pointer array (vtable 0x5ef30c, destructor writes 0x5ef318) of
//           CListenerReference, owned
//     +0x18 "Listener": pointer array (vtable 0x5ef300, destructor writes 0x5ef324) of listener objects
//           as read from older files; slot 18 turns them into listener references
//   CUpdateMonitorInfo (0x14, vtable 0x5f8a18, class 0x6c1ce4, version 1, parent CObject):
//     +8 "Update Part" (reference to a CLayerPart), +0xc "Half Width", +0x10 "Half Height"
//   CUpdateMonitorList (0x18, vtable 0x5f8a74, class 0x6c1ce8, version 2, parent CObject):
//     +8 "Monitor List": pointer array (vtable 0x5f8ad0, destructor writes 0x5f8adc) of
//        CUpdateMonitorInfo; the list listens to the broadcaster of every monitored part
//   CChildList (0x18, vtable 0x5f92b8, class 0x6c29c8, version 1, no parent class):
//     +8 "Child List": pointer array (vtable 0x5f8b8c, destructor writes 0x5f8b98) of owned objects;
//        the element descriptor "Child" is also kept in the global 0x6c29c4
// A layer part (CLayerPart) has its child list at +8 and its broadcaster at +0x10 (created on the first
// listener, deleted with the last one); an AI object (CAIBase) has its broadcaster at +0xc.
//
// Return values are what the original leaves in eax. Where that is the caller's eax or a byte with
// undefined upper bytes, the comment says so.
#pragma once
#include "class_info.h"

namespace zax {

struct CObjectBase {
    DWORD vftable;
    void *dynamic_properties;   // 0x04
};

// The CObject functions of the exe (range 0x416000-0x419fff; object_base.cpp, field_slots.cpp and others), called
// through their exe addresses
inline void object_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
inline void object_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
inline void object_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
inline void object_dtor(void *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
inline CClassInfo *object_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }

constexpr DWORD EMPTY_TEXT = 0x62e558;   // ""

struct CListenerReference : CObjectBase {
    CObjectBase *listener;   // 0x08
    int member_offset;       // 0x0c
    ZString member_name;     // 0x10

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x4675a0 (slot 1)
};
static_assert(sizeof(CListenerReference) == 0x14);

struct CBroadcaster : CObjectBase {
    ZPtrArray listening;   // 0x08 CListenerReference *
    ZPtrArray pending;     // 0x18 listener objects read from older files

    CBroadcaster *THISCALL Ctor();                                                  // 0x467d00
    void construct() { Ctor(); }
    void *THISCALL DeletingDtor(DWORD flags);                               // 0x467970 (slot 1)
    DWORD THISCALL Broadcast(CObjectBase *sender, const char *message);             // 0x467d40
    DWORD THISCALL ResolvePending(DWORD unused);                                    // 0x467d80 (slot 18)
    DWORD THISCALL AddListener(CObjectBase *listener, int offset, const char *name);     // 0x467e60
    DWORD THISCALL RemoveListener(CObjectBase *listener, int offset, const char *name);  // 0x467ee0
    void destroy();
};
static_assert(sizeof(CBroadcaster) == 0x28);

// The broadcaster members of a layer part (+0x10) and of an AI object (+0xc)
struct BroadcasterOwner {
    template <DWORD OFF> CBroadcaster *&broadcaster() { return *reinterpret_cast<CBroadcaster **>(reinterpret_cast<BYTE *>(this) + OFF); }
    template <DWORD OFF> DWORD THISCALL AddListener(CObjectBase *listener, int offset, const char *name);
    template <DWORD OFF> DWORD THISCALL RemoveListener(CObjectBase *listener, int offset, const char *name);
    DWORD THISCALL PartBroadcast(const char *message);   // 0x4fc240 (layer part)
};

struct CUpdateMonitorInfo : CObjectBase {
    CObjectBase *part;   // 0x08
    int half_width;      // 0x0c
    int half_height;     // 0x10

    void construct();
    void destroy();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x4e48c0 (slot 1)
};
static_assert(sizeof(CUpdateMonitorInfo) == 0x14);

struct CUpdateMonitorList : CObjectBase {
    ZPtrArray monitors;   // 0x08 CUpdateMonitorInfo *

    CUpdateMonitorList *THISCALL Ctor();                                  // 0x4e4f40
    void construct() { Ctor(); }
    void *THISCALL DeletingDtor(DWORD flags);               // 0x4e4c80 (slot 1)
    DWORD THISCALL OnMessage(CObjectBase *sender, const char *message);   // 0x4e5090 (slot 0)
    DWORD THISCALL Add(CObjectBase *part, int half_width, int half_height);   // 0x4e4f70
    DWORD THISCALL Remove(CObjectBase *part);                             // 0x4e5000
    void destroy();
};
static_assert(sizeof(CUpdateMonitorList) == 0x18);

struct CChildList : CObjectBase {
    ZPtrArray children;   // 0x08 owned objects

    void construct();
    void *THISCALL DeletingDtor(DWORD flags);   // 0x4f9900 (slot 1)
    DWORD THISCALL RemoveChild(CObjectBase *child);   // 0x4f9b70
    void destroy();
};
static_assert(sizeof(CChildList) == 0x18);

}  // namespace zax
