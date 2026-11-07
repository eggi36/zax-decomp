// Field values as text, second part (see field_text.h for the simple kinds): slots 16 ToText and 19
// FromText of the flags, ZString, class pointer and C string kinds, slot 47 "from text through the
// converter" of the number kinds, the functions that build the name lists (FieldDesc_AddEnum) and
// slot 27 of the name lists (find the entry with a value).
//
//   slot 16 ToText(object, ZString *out)              writes the value at object + offset as text
//   slot 19 FromText(object, text, DWORD)             parses the text and stores it (or calls the setter);
//                                                     the third argument is not used
//   slot 47 FromTextConverted(object, text)           the number from the text goes through slot 2 of the
//                                                     converter (+0x3c resp. +0x38/+0x4c), no limits, no names
//
// ToText returns nothing (eax is what ZString's SetText/Append leave). FromText and the slot-47
// functions return what the original leaves in eax (see the comments).
#pragma once
#include "field_kinds.h"

namespace zax {

struct CFieldText2 {
    // 0x414f10 the names of the set bits, joined by ","; "" for 0. Bit i is name i of the list
    // (+0x2c, slot 22 count, slot 1 name); all 32 bits of the dword count
    static void THISCALL FlagsToText(CFlagsFieldDesc *self, BYTE *object, ZString *out);
    // 0x414fa0 the comma-separated names, each through slot 3 of the list (FindOrFail: a missing name
    // ends the game); the text is split in place and restored. Setter +0x30 with the mask, else the low
    // 16 bits of the dword are replaced; returns the setter's result or object + offset
    static DWORD THISCALL FlagsFromText(CFlagsFieldDesc *self, BYTE *object, char *text, DWORD unused);
    // 0x416060 the ZString, or "" when it is empty
    static void THISCALL StringToText(CStringFieldDesc *self, BYTE *object, ZString *out);
    // 0x4160a0 assigns the text; with +0x28 also 0x5007c0 on the string (copy to +4, translated there
    // through the Local.txt table 0x6c2b90) and returns its result, else the string's address with the
    // low byte 0
    static DWORD THISCALL StringFromText(CStringFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x418570 the name (+0xc) of the class info, or ""
    static void THISCALL ClassToText(CAIFieldDesc *self, BYTE *object, ZString *out);
    // 0x4185a0 "" stores null and returns the offset, else ClassRegistry_FindChecked(text) (null when
    // missing) is stored and returned
    static DWORD THISCALL ClassFromText(CAIFieldDesc *self, BYTE *object, const char *text, DWORD unused);
    // 0x54d330 the text, or "" for null
    static void THISCALL TextToText(CTextFieldDesc *self, BYTE *object, ZString *out);
    // 0x54d360 setter +0x2c with the text (its result), else the old text is deleted and a copy stored
    // (returns the length + 1, for null text 0 or what operator delete left)
    static DWORD THISCALL TextFromText(CTextFieldDesc *self, BYTE *object, const char *text, DWORD unused);

    // 0x54dd90 int: atoi, converter +0x3c slot 2; setter +0x38; returns the setter's result or the value
    static DWORD THISCALL IntFromTextConverted(CIntFieldDesc *self, BYTE *object, const char *text);
    // 0x54dc10 enum int: the same with converter +0x38 and setter +0x3c
    static DWORD THISCALL EnumIntFromTextConverted(CEnumIntFieldDesc *self, BYTE *object, const char *text);
    // 0x54eed0 float: atof rounded to float, converter slot 2 (float in st(0)); setter +0x38 (its float
    // result is popped); returns the setter's result or the object
    static DWORD THISCALL FloatFromTextConverted(CFloatFieldDesc *self, BYTE *object, const char *text);
    // 0x54f1b0 double: the same with doubles, converter +0x4c, setter +0x48; returns the setter's
    // result or the offset
    static DWORD THISCALL DoubleFromTextConverted(CDoubleFieldDesc *self, BYTE *object, const char *text);

    // 0x54da60 FieldDesc_AddEnum(value, name) of the enum int kind: creates the list +0x40 on first use
    // (pool, vtable 0x5f0240, array vtable 0x5fd648) and appends {copy of the name, value} (pool, 8
    // bytes) through slot 2 of the array; returns its result
    static DWORD THISCALL AddEnum(CEnumIntFieldDesc *self, DWORD value, const char *name);
    // 0x54ed60 the same for the float kind (list vtable 0x5fdaec, array vtable 0x5fdb5c)
    static DWORD THISCALL AddEnumFloat(CFloatFieldDesc *self, DWORD value, const char *name);

    // Slot 27 of the name lists: index of the first entry with the value, -1 = none
    // 0x592320 int (compared as dwords; also slot 27 of the list vtable 0x5f0240)
    static int THISCALL FindValue(FieldNameList *self, DWORD value);
    // 0x54f3c0 float: fld entry, fcomp value; "equal" is C3, so a NaN matches the first entry
    static int THISCALL FindFloat(FieldNameList *self, DWORD value);
    // 0x54f550 double (the value as two dwords, entries of 12 bytes), like the float
    static int THISCALL FindDouble(FieldNameList *self, DWORD lo, DWORD hi);
};

}  // namespace zax
