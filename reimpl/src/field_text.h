// Field values as text and back (vtable slots 16 and 19 of the field kinds, field_kinds.h) for the
// simple kinds: int, enum int, byte, bit, float, double, float and integer point, colour.
//
//   slot 16 ToText(object, ZString *out)              writes the value at object + offset as text
//   slot 19 FromText(object, const char *text, DWORD)  parses the text and stores it (or calls the setter);
//                                                     the third argument is not used
//
// The number kinds with a list of names (enum int, float, double; +0x40/+0x50) look the value up in
// it first: slot 27 of the list finds the entry with that value (-1 = none), slot 1 gives its name,
// slot 2 Find(name, -1) the entry of a name. Equal limits mean "no limits": then FromText takes the
// number as it is and does not consult the names either.
//
// ToText returns nothing (eax is what ZString's Format/SetText leave, see zstring.h). FromText
// returns what the original leaves in eax: the setter's result, or the value (int), the object (byte,
// float, points), object + offset (bit), 0 (enum int), the offset (double), the colour's flags.
#pragma once
#include "field_kinds.h"

namespace zax {

struct CFieldText {
    // 0x417c40 "%d" of the dword (also slot 16 of the list and object array kinds: the count)
    static void THISCALL IntToText(CFieldDesc *self, BYTE *object, ZString *out);
    // 0x54d690 atoi, clamped to [min, max] (signed) unless min == max; setter +0x38
    static DWORD THISCALL IntFromText(CIntFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x54db10 the name of the value or "%u"
    static void THISCALL EnumIntToText(CEnumIntFieldDesc *self, BYTE *object, ZString *out);
    // 0x54db70 atoi; a value that has a name stays, else a name gives its value, else clamped (unsigned)
    static DWORD THISCALL EnumIntFromText(CEnumIntFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x54df00 "%u" of the byte
    static void THISCALL ByteToText(CByteFieldDesc *self, BYTE *object, ZString *out);
    // 0x54df30 atoi != 0; the setter's argument keeps the upper bytes of the text pointer
    static DWORD THISCALL ByteFromText(CByteFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x54e0f0 "1" if a bit of the mask is set, else "0"
    static void THISCALL BitToText(CBitFieldDesc *self, BYTE *object, ZString *out);
    // 0x54e130 atoi != 0 sets or clears the mask; setter +0x30
    static DWORD THISCALL BitFromText(CBitFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x54ee60 the name of the value or "%g"
    static void THISCALL FloatToText(CFloatFieldDesc *self, BYTE *object, ZString *out);
    // 0x54ef30 atof rounded to float, then like the enum int (clamped with x87 compares)
    static DWORD THISCALL FloatFromText(CFloatFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x54f130 the name of the value or "%g"
    static void THISCALL DoubleToText(CDoubleFieldDesc *self, BYTE *object, ZString *out);
    // 0x54f220 atof, then like the float
    static DWORD THISCALL DoubleFromText(CDoubleFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x4a8110 "%f,%f"
    static void THISCALL PointToText(CPointFieldDesc *self, BYTE *object, ZString *out);
    // 0x4a8140 sscanf "%f,%f", no setter; a missing x keeps the bits of the text pointer, a missing y
    // those of the descriptor pointer (the stack slots sscanf writes into)
    static BYTE *THISCALL PointFromText(CPointFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x5455a0 "%d,%d"
    static void THISCALL IntPointToText(CPointFieldDesc *self, BYTE *object, ZString *out);
    // 0x5455d0 sscanf "%d,%d", like the float point
    static BYTE *THISCALL IntPointFromText(CPointFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x46dee0 "%d,%d,%d" of the bytes +6, +5, +4 (red, green, blue)
    static void THISCALL ColorToText(CColorFieldDesc *self, BYTE *object, ZString *out);
    // 0x46df60 sscanf "%d,%d,%d", then FieldColor_SetRGBA(r, g, b, 0xff); no setter. A missing red or
    // green is stack garbage in the original (uninitialised locals); here 0. A missing blue keeps
    // the text pointer.
    static DWORD THISCALL ColorFromText(CColorFieldDesc *self, BYTE *object, const char *text, DWORD unused);

    // 0x53f080 FieldColor_SetRGBA: bytes b, g, r, a at +0, b, g, r at +4, the 16-bit pixel
    // 0x433a10(r, g, b) at +8 and flag 2 at +0xc; returns the new flags dword
    static DWORD THISCALL ColorSetRGBA(FieldColor *c, DWORD r, DWORD g, DWORD b, DWORD a);
};

}  // namespace zax
