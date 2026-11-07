// Value slots of the field kinds that need no file I/O (see field_kinds.h for the slot numbers):
// slots 4 CopyValue, 6 InitValue, 8 FreeValue and 10 SetDefault of the ZString, flags, object array
// (new[]), pointer array, owned object, dynamic properties and polygon kinds, and the helpers they call:
// resizing the arrays, cloning an object, the dynamic property list, assigning a polygon and the
// translation of ZString names through the Local.txt table.
//
// Return values are what the original leaves in eax. Where that comes from a function that returns
// nothing (the ZString functions, the pool), the reimplementation calls it through its exe address and
// passes its eax on, so original and reimplementation agree as long as both call the same code. The
// two resize functions return nothing.
#pragma once
#include "field_kinds.h"

namespace zax {

// Entry of the dynamic property list: a descriptor and the value it describes (a block from slot 35
// of the descriptor, released with slot 36)
struct DynamicProperty {
    CFieldDesc *desc;
    BYTE *value;
};

// CObject.Dynamic Properties: an 8-byte array {vtable 0x5eca54, data, count, capacity} from the pool
// (0x10 bytes, data from the pool with size capacity * 8); the base vtable 0x5eca60 is written by
// the destructor
struct ZDynamicProperties {
    DWORD vftable;
    DynamicProperty *data;
    DWORD count;
    DWORD capacity;
};
static_assert(sizeof(ZDynamicProperties) == 0x10);

// Polygon value (0x28 bytes): points {x, y} and one 12-byte entry per edge (count - 1), both from
// operator new; the other members are copied as they are
struct ZPolygon {
    DWORD *points;    // 0x00 count * 8 bytes
    DWORD *edges;     // 0x04 (count - 1) * 12 bytes
    DWORD count;      // 0x08
    DWORD f0c[4];     // 0x0c-0x18
    DWORD f1c, f20, f24;
};
static_assert(sizeof(ZPolygon) == 0x28);

constexpr DWORD VT_DYNAMIC_PROPERTIES = 0x5eca54, VT_DYNAMIC_PROPERTIES_BASE = 0x5eca60;
// The table of Local.txt (loaded by 0x500ac0): +0x104 count, +0x10c sorted entries {ZString key,
// ZString text}
constexpr DWORD LOCAL_TABLE = 0x6c2b90;

struct CFieldSlots {
    // ---- ZString kind (0x5ec91c); with +0x28 the string at +4 holds the translated copy ----
    // 0x415e70 slot 4: a copy of the source string through 0x4161d0
    static DWORD THISCALL StringCopyValue(CStringFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from);
    // 0x4161d0 stores the string (passed by value, released at the end): through the setter +0x30, which
    // the code calls as ZString f(ZString) (hidden result, 8 bytes popped), else Assign; with +0x28
    // then 0x5007c0
    static DWORD THISCALL StringSet(CStringFieldDesc *self, BYTE *object, ZString value);
    // 0x415fd0 slot 6: InitEmpty of the string (and of the one at +4 with +0x28)
    static DWORD THISCALL StringInitValue(CStringFieldDesc *self, BYTE *object);
    // 0x416000 slot 8: Empty of the string (and of the one at +4 with +0x28)
    static DWORD THISCALL StringFreeValue(CStringFieldDesc *self, BYTE *object);
    // 0x416030 slot 10: the default (+0x2c), with +0x28 then 0x5007c0
    static DWORD THISCALL StringSetDefault(CStringFieldDesc *self, BYTE *object);
    // 0x5007c0 copies the string to the ZString behind it and translates that through Local.txt
    static DWORD THISCALL StringLocalize(ZString *self);
    // 0x500f50 Local_Translate(text, context): looks the text (with a context: text + "[END]" + " (%s)")
    // up in the sorted table (binary search with ZString::Compare) and replaces it by the translation
    static DWORD THISCALL LocalTranslate(BYTE *table, ZString *text, const char *context);

    // ---- Flags kind (0x5ec684) ----
    // 0x414de0 slot 10: no default names: 0; "$all": every name of the list; else the names through
    // slot 19 (FromText) while g_LastFileOpened names the static text 0x62e658 (for error messages)
    static DWORD THISCALL FlagsSetDefault(CFlagsFieldDesc *self, BYTE *object);

    // ---- Array of objects from new[] (0x5eca78) ----
    // 0x4181f0 resize to count, inserting or removing at `at` (-1 = the end); elements move with slot
    // 20 of the objects, new ones get slot 19
    static void THISCALL ListResize(CListFieldDesc *self, BYTE *object, DWORD count, DWORD at);
    // 0x417a80 slot 4: resize to 0 and to the source count, then slot 20 per element; returns the count
    static DWORD THISCALL ListCopyValue(CListFieldDesc *self, BYTE *dst, BYTE *src, const CListFieldDesc *from);
    // 0x417b70 slot 10: deletes the data (vector deleting destructor), then the count of the default
    // list +0x40 (its slot 22)
    static DWORD THISCALL ListSetDefault(CListFieldDesc *self, BYTE *object);

    // ---- Pointer array with an element descriptor (0x5ecd6c) ----
    // 0x419a10 resize (the fourth argument keeps the capacity at the count); elements go through the
    // slots of the element descriptor: 6 init, 8 free, 10 default, 33 size, 38/40 array, 42 move
    static void THISCALL ObjectArrayResize(CObjectArrayFieldDesc *self, BYTE *object, DWORD count, DWORD at, DWORD exact);
    // 0x4192a0 slot 4: resize to 0 and to the source count, then slot 4 of the element descriptor
    static DWORD THISCALL ObjectArrayCopyValue(CObjectArrayFieldDesc *self, BYTE *dst, BYTE *src,
                                               const CObjectArrayFieldDesc *from);
    // 0x419320 slot 8: slot 8 per element, then the data back (pool with +0x44, else slot 40)
    static DWORD THISCALL ObjectArrayFreeValue(CObjectArrayFieldDesc *self, BYTE *object);
    // 0x4193f0 slot 10: resize to 0, then to the count of the default list +0x40
    static DWORD THISCALL ObjectArraySetDefault(CObjectArrayFieldDesc *self, BYTE *object);

    // ---- Owned object (0x5ecbf4; +0x34 class info of the default, +0x38 setter) ----
    // 0x416760 CObject clone: slot 27 (Create) of the object's class info (slot 2), then slot 20 (copy)
    static void *THISCALL ObjectClone(void *self, DWORD a, DWORD b);
    // 0x418840 slot 4: deletes the old object (slot 1 with 1), stores a clone of the source
    static DWORD THISCALL ObjectCopyValue(CObjectFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from);
    // 0x418890 slot 10: a new object of the class +0x34 (slot 27, then 19) or null; setter or store
    static DWORD THISCALL ObjectSetDefault(CObjectFieldDesc *self, BYTE *object);

    // ---- Dynamic properties (0x5f17ec) ----
    // 0x4828b0 destructor of the list: slots 8 and 36 of every entry's descriptor, then the data back
    static void THISCALL DynamicDtor(ZDynamicProperties *self);
    // 0x482c20 assigns another list (the first argument is not used): clears this one, then per source
    // entry adds an entry (0x482e40, 0x482e10) unless one with that descriptor and a value exists, and
    // copies the value with slot 4 of entry i
    static DWORD THISCALL DynamicAssign(ZDynamicProperties *self, void *unused, const ZDynamicProperties *src);
    // 0x482e40 appends an uninitialised entry (grows like the arrays) and returns it
    static DynamicProperty *THISCALL DynamicAppend(ZDynamicProperties *self);
    // 0x482e10 sets an entry: the descriptor, a value block from its slot 35 and slot 6 on it
    static DWORD THISCALL DynamicInitEntry(DynamicProperty *self, CFieldDesc *desc);
    // 0x482f50 slot 4: deletes the old list, a new one (pool) assigned from the source
    static DWORD THISCALL DynamicCopyValue(CDynamicPropertiesFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from);
    // 0x482fd0 slot 10 and 0x4834c0 slot 8: delete the list
    static DWORD THISCALL DynamicSetDefault(CDynamicPropertiesFieldDesc *self, BYTE *object);
    static DWORD THISCALL DynamicFreeValue(CDynamicPropertiesFieldDesc *self, BYTE *object);

    // ---- Polygon (0x5fd2a0) ----
    // 0x545e80 assigns a polygon (new copies of the points and edges)
    static DWORD THISCALL PolygonAssign(ZPolygon *self, const ZPolygon *src);
    // 0x548050 slot 4
    static DWORD THISCALL PolygonCopyValue(CPolygonFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from);
    // 0x548070 slot 10: cleared, then the default polygon +0x28 if there is one
    static DWORD THISCALL PolygonSetDefault(CPolygonFieldDesc *self, BYTE *object);
};

}  // namespace zax
