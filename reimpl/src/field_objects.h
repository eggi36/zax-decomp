// The slots of the field kinds that create objects: 16 (value as text), 19 (from text), 26 (write
// binary), 29 (read binary) and 32 (precache: the objects' slot 7, see object_base.h) of the owned
// object kind (0x5ecbf4), the pointer array kind (0x5ecd6c) and the new[] array kind (0x5eca78), the
// binary slots of the class pointer kind (0x5ecb38), and the pointer array's append and remove.
//
// Owned object (+0x2c the class the object must derive from):
//   16 (0x418970, also slot 16 of the object reference kind): the object's class name or "";
//   19 (0x4189b0, object, text, reader): "" deletes the object; the same class name as the current
//     object reads into it, else the object is deleted and the class looked up (missing, not derived
//     from +0x2c or abstract: "Invalid class type" box and exit(-1), with the field name and
//     g_LastFileOpened), created (Create (name, 0, -1)), slots 16 and 19 of the new object,
//     Object_ReadText, then slot 13 of the descriptor (object, table mode != 0). Without an object
//     (Create gave null) the block behind the line is skipped ("{" ... "}" with nesting, else back);
//   26 (0x418c60): the class name as counted text ("" without an object), then Object_WriteBinary;
//   29 (0x418cb0): the object is deleted, the class name read (a 0x64-byte buffer), an object of that
//     class created (no checks) and read with Object_ReadBinary; true without a name or an object;
//   32 (0x418d70): slot 7 of the object, if any.
// Pointer array (+0x30 data offset, +0x38 element descriptor, +0x3c element size, +0x40 name list):
//   19 (0x419470): without a reader the count from the text (atoi). Else (without a name list the array
//     is emptied first) the line after the field line is skipped and the block read: "{" and "}" nest
//     (the "}" at depth 0 ends), "Item Count=" or "Array Count=" resize to the number; with a name list
//     "<name>=value" reads element slot 2(name) of the list (unknown names are skipped); without one,
//     lines named like the element descriptor (or its alternative name) read the next element, the
//     array grows by one if the count read so far is too small;
//   26 (0x4197a0): the count, per element its name (with a name list) and slot 26 of the element desc;
//   29 (0x419810): emptied, the count read, resized to it (or to the name list's count, exact); with a
//     name list every element gets slot 6 (init) and the stored elements are read by name; slot 29 of
//     the element descriptor for each, false at the first failure or unknown name;
//   32 (0x419960): slot 32 of the element descriptor per element.
//   0x4199b0 appends an element (returns it), 0x4199f0 removes the one at the index.
// new[] array (+0x28 capacity offset, +0x30 data offset, +0x38 element class, +0x3c element size, +0x40
// name list):
//   19 (0x417c60): without a reader the count from the text. Else (without a name list emptied) the line
//     after the field line is skipped; per line up to "}": "Array Count=" / "Item Count=" (strnicmp)
//     take the number: without a name list and with a count or capacity that differs the data is
//     deleted (vector destructor, slot 1 with 3) and created anew (slot 28 of the class: n, 0, -1),
//     the next element is 0. Other lines (cut at '='): with a name list the element of that name is
//     read (0x416af0; an unknown name reads into a scratch object of the class (slot 27 with 0, -1)
//     that is deleted); without one the next element: before any count line the array grows by one
//     per line (0x4181f0), after a count line the elements are read in order without growing;
//   26 (0x417ff0): the count, per element its name (with a name list) and Object_WriteBinary;
//   29 (0x418050): the count read (or the name list's), count and capacity set, the data recreated as
//     in 19; with a name list every element gets slot 19 and the stored ones are read by name; then
//     Object_ReadBinary for each, false at the first failure or unknown name;
//   32 (0x4181b0): slot 7 of every element.
// Class pointer (0x5ecb38): 26 (0x418620) the class name as counted text, 29 (0x418650) reads it
// (0x64-byte buffer) and looks it up with ClassRegistry_FindChecked; "" gives null.
//
// Return values are the original's eax where it is defined by the code; the results of the void
// resize functions (0x4181f0, 0x419a10) and of loops ending on a text pointer (a stack address with the
// character in al) are not reproduced and documented in the checks.
#pragma once
#include "field_streams.h"

namespace zax {

struct CFieldObjects {
    // owned object
    static DWORD THISCALL ObjectToText(CObjectFieldDesc *self, BYTE *object, ZString *out);                  // 0x418970
    static DWORD THISCALL ObjectFromText(CObjectFieldDesc *self, BYTE *object, char *text, CFileBuf *r);     // 0x4189b0
    static DWORD THISCALL ObjectWriteBinary(CObjectFieldDesc *self, BYTE *object, CFileBuf *w);              // 0x418c60
    static DWORD THISCALL ObjectReadBinary(CObjectFieldDesc *self, BYTE *object, CFileBuf *r);               // 0x418cb0
    static DWORD THISCALL ObjectPrecache(CObjectFieldDesc *self, BYTE *object);                                // 0x418d70
    // pointer array
    static DWORD THISCALL ArrayFromText(CObjectArrayFieldDesc *self, BYTE *object, char *text, CFileBuf *r); // 0x419470
    static DWORD THISCALL ArrayWriteBinary(CObjectArrayFieldDesc *self, BYTE *object, CFileBuf *w);          // 0x4197a0
    static DWORD THISCALL ArrayReadBinary(CObjectArrayFieldDesc *self, BYTE *object, CFileBuf *r);           // 0x419810
    static DWORD THISCALL ArrayPrecache(CObjectArrayFieldDesc *self, BYTE *object);                            // 0x419960
    static BYTE *THISCALL ArrayAppend(CObjectArrayFieldDesc *self, BYTE *object);                            // 0x4199b0
    static DWORD THISCALL ArrayRemoveAt(CObjectArrayFieldDesc *self, BYTE *object, DWORD at);                // 0x4199f0
    // new[] array
    static DWORD THISCALL ListFromText(CListFieldDesc *self, BYTE *object, char *text, CFileBuf *r);         // 0x417c60
    static DWORD THISCALL ListWriteBinary(CListFieldDesc *self, BYTE *object, CFileBuf *w);                  // 0x417ff0
    static DWORD THISCALL ListReadBinary(CListFieldDesc *self, BYTE *object, CFileBuf *r);                   // 0x418050
    static DWORD THISCALL ListPrecache(CListFieldDesc *self, BYTE *object);                                    // 0x4181b0
    // class pointer
    static DWORD THISCALL ClassWriteBinary(CAIFieldDesc *self, BYTE *object, CFileBuf *w);                   // 0x418620
    static DWORD THISCALL ClassReadBinary(CAIFieldDesc *self, BYTE *object, CFileBuf *r);                    // 0x418650
};

constexpr DWORD TEXT_INVALID_CLASS_TYPE = 0x60a9c0;   // "Invalid class type"
constexpr DWORD TEXT_UNKNOWN_CLASS = 0x60a9d4;        // "Tried to use an unknown class \"%s\" for a \"%s\" (%s). ..."
constexpr DWORD TEXT_WRONG_CLASS = 0x60a974;          // "Tried to use a %s for a %s when a %s is expected. ..."
constexpr DWORD TEXT_ABSTRACT_CLASS = 0x60a924;       // "Tried to use the abstract class \"%s\" for a \"%s\" (%s). ..."
constexpr DWORD TEXT_ITEM_COUNT_NAME = 0x60aa20;      // -> "Item Count"
constexpr DWORD TEXT_ARRAY_COUNT = 0x60aa30;          // "Array Count"
constexpr DWORD TEXT_ARRAY_COUNT_EQ = 0x60a8ec;       // "Array Count="
constexpr DWORD TEXT_ITEM_COUNT_EQ = 0x60a8e0;        // "Item Count="

}  // namespace zax
