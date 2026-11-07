// Text output and the object visit of the field kinds (see field_kinds.h for the slot numbers):
//   slot 23 WriteText(object, writer): writes the value behind "Name=" (Object_WriteText 0x4168f0
//           writes "Name=" for every field whose slot 20 says so, then calls slot 23);
//   slot 20 HasValue(object): whether Object_WriteText writes the field at all (0x46cd80, true, for all
//           kinds except the dynamic properties);
//   slot 13 VisitObjects(object, with_visitor): passes the objects the field holds to their slot 18,
//           with a visitor (a small object on the stack) or null; 0x4f0a80 (nothing) for the kinds
//           without objects. What slot 18 does with the visitor is the objects' business; the two
//           visitors only check a class and store the object or null [assumption on the purpose:
//           validating or resolving object pointers].
// Slots 13 and 20 were earlier assumed to be binary streams; they are not.
//
// The writer is a CFileBuf with more members (0x24 bytes, constructor 0x5240b0, vtable 0x5fb8fc,
// Open(stream, flags, mode) 0x5242f0): +0x1c its object table (0x5c bytes, constructor 0x55c6f0), +0x20
// "finished". With mode 0 (+0x18 of the table) Object_WriteText registers every object it writes
// (0x55cb30) and writes "Object ID=0x<address>"; object references are collected (0x55cad0) and the
// finish (0x55cd90, 0x55ce50) writes the list "Referenced Objects". Mode 1 writes neither (the game's
// change test 0x417100 writes objects that way into memory streams and compares the bytes).
//
// Not here: the image slot 23 (0x4b5b20) and the shared stubs 0x4f0a80, 0x46cd80. The temp file slot 23
// (0x58bd50), the object table (0x55c6f0-0x55ce50) and the stream's own methods (0x5240b0-0x524340)
// are in object_io.h and object_table.h.
//
// Return values are what the original leaves in eax (mostly that of the last CFileBuf call or of the
// ZString destructor). Not reproducible, documented instead: 0x417bd0 with an empty array returns a
// stack address with the flag in al; 0x482d70 with an empty list and 0x417740 with a null object leave
// the caller's eax.
#pragma once
#include "field_kinds.h"
#include "field_slots.h"
#include "class_info.h"
#include "file_buffer.h"

namespace zax {

// CFileBuf with the object table: the writer of 0x5240b0
struct CObjectWriter : CFileBuf {
    BYTE *table;     // 0x1c object table (+0x18 mode, +0x3c referenced objects, +0x4c written objects)
    BYTE finished;   // 0x20 set by the finish (0x524340, destructor 0x5241e0)
    BYTE pad21[3];
};
static_assert(sizeof(CFileBuf) == 0x1c && sizeof(CObjectWriter) == 0x24);

// Visitor of 0x54e750 and 0x417bd0 (vtable 0x5ec9e0, on the stack {result, vtable, &result}): stores the
// object if its class derives from "CClassThatCanNotBeConstructedOrDerivedFrom", else null
struct QueryVisitor {
    DWORD vftable;
    void **result;
};
// Visitor of 0x418920 (vtable 0x5eca6c, constructor 0x417720): stores the object into the field if
// its class derives from the class of the descriptor (+0x2c), else null
struct ClassFilterVisitor {
    DWORD vftable;
    void **field;
    CClassInfo *cls;
};

constexpr DWORD VT_QUERY_VISITOR = 0x5ec9e0, VT_CLASS_FILTER_VISITOR = 0x5eca6c;
constexpr DWORD NO_CLASS_VAR = 0x62e698;   // class variable of "CClassThatCanNotBeConstructedOrDerivedFrom"

struct CFieldStreams {
    // ---- Writing objects as text ----
    // 0x4168f0: class name, "{", indented: (mode 0) "Object ID=0x%08X", then "Name=" + slot 23 for
    // every field with a name whose slot 20 is true, the object's slot 9 (its own lines), "}"
    static DWORD THISCALL ObjectWriteText(void *object, CObjectWriter *w);
    // 0x418fe0 slot 23 of the plain kinds: the value as text (slot 16), one line
    static DWORD THISCALL FieldWriteText(CFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x417f10 slot 23, new[] array: "Array", "{", "Array Count=%u", per element "<name>=" + the
    // object (the name from slot 1 of the list +0x40, else "Array Item"), "}"
    static DWORD THISCALL ListWriteText(CListFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x4185f0 slot 23, class pointer: the class name (+0xc) or an empty line
    static DWORD THISCALL ClassWriteText(CAIFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x418c30 slot 23, owned object: the object or an empty line
    static DWORD THISCALL ObjectFieldWriteText(CObjectFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x4196c0 slot 23, pointer array: "Array", "{", "Item Count=%d" (without a name list +0x40), per
    // element "<name>=" + slot 23 of the element descriptor, "}"
    static DWORD THISCALL ObjectArrayWriteText(CObjectArrayFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x483200 slot 23, dynamic properties: "0" without entries, else the count, "{", "Name=" + slot 23
    // per entry, "}"
    static DWORD THISCALL DynamicWriteText(CDynamicPropertiesFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x523f50 slot 23, object reference: "0x%08X (Class)" or an empty line; the object is collected as
    // referenced (0x55cad0)
    static DWORD THISCALL ObjectRefWriteText(CObjectRefFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x54e7d0 slot 23, embedded object: the object at the offset
    static DWORD THISCALL EmbeddedWriteText(CEmbeddedFieldDesc *self, BYTE *object, CObjectWriter *w);
    // 0x548210 slot 23, polygon: "%g, %g" per point, separated by ", ", one line
    static DWORD THISCALL PolygonWriteText(CPolygonFieldDesc *self, BYTE *object, CObjectWriter *w);

    // ---- Slot 20 ----
    // 0x4831d0: a list with entries
    static DWORD THISCALL DynamicHasValue(CDynamicPropertiesFieldDesc *self, BYTE *object);

    // ---- Slot 13: the objects to their slot 18 ----
    // 0x417bd0 every element of the new[] array, with a QueryVisitor if the flag (low byte) is set
    static DWORD THISCALL ListVisit(CListFieldDesc *self, BYTE *object, DWORD flag);
    // 0x418920 the owned object, with a ClassFilterVisitor on the field if the flag is set
    static DWORD THISCALL ObjectVisit(CObjectFieldDesc *self, BYTE *object, DWORD flag);
    // 0x419430 slot 13 of the element descriptor on every element (the flag passed on as it is)
    static DWORD THISCALL ObjectArrayVisit(CObjectArrayFieldDesc *self, BYTE *object, DWORD flag);
    // 0x483000 the list's 0x482d70
    static DWORD THISCALL DynamicVisit(CDynamicPropertiesFieldDesc *self, BYTE *object, DWORD flag);
    // 0x54e750 the embedded object, with a QueryVisitor if the flag is set
    static DWORD THISCALL EmbeddedVisit(CEmbeddedFieldDesc *self, BYTE *object, DWORD flag);
    // 0x482d70 slot 13 of every entry's descriptor on its value
    static DWORD THISCALL DynamicListVisit(ZDynamicProperties *self, DWORD flag);
    // 0x482df0 / 0x482e00 descriptor and value of entry i
    static CFieldDesc *THISCALL DynamicDescAt(ZDynamicProperties *self, DWORD i);
    static BYTE *THISCALL DynamicValueAt(ZDynamicProperties *self, DWORD i);

    // ---- The visitors ----
    // 0x417720
    static ClassFilterVisitor *THISCALL ClassFilterCtor(ClassFilterVisitor *self, void **field, CClassInfo *cls);
    // 0x417740 slot 0
    static DWORD THISCALL ClassFilterVisit(ClassFilterVisitor *self, void *object);
    // 0x417780 slot 1 (also 0x417810, slot 1 of the QueryVisitor, same code)
    static void *THISCALL ClassFilterResult(ClassFilterVisitor *self);
    static void *THISCALL QueryResult(QueryVisitor *self);
    // 0x417790 slot 0
    static DWORD THISCALL QueryVisit(QueryVisitor *self, void *object);
    // 0x417820 slot 2: the class, registered on first use (0x48 bytes from the pool, ClassInfo_Register)
    static CClassInfo *__cdecl QueryClass();
};

}  // namespace zax
