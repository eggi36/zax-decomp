// Objects read as text, objects written and read as binary, the object reference kind's read slot 19
// and binary slots 26/29, the temp file kind's slot 23, and the white space skipper of the text reader.
//
// Object_ReadText (0x416b60, reader = the object stream of object_table.h): an optional first line with
// a class name (it must name a registered class the object derives from), then "{", then one line
// per field until "}". Lines are taken behind their leading white space (0x587ad0); empty lines and
// lines without "=" are skipped, a line "{" ends the game ("Found unexpected '{' while parsing class",
// ShowErrorBox, exit(-1)). For "Name=value":
//   - "Object ID" registers the object under the ID (0x55cc90), but only as the first field line or
//     right behind "Referenced Object List Location" lines;
//   - if the object is *0x62e68c and the name is *0x62e690 the reading stops there with true (set
//     around 0x416a00 by the level loader 0x4d33f9 to read a layer only up to "Tree List");
//   - a field of the object's class (ClassInfo_FindField) reads the value with its slot 19 (object,
//     value, reader); otherwise "Class::Name" is looked up by the part behind "::" (the part before it
//     is not checked) and, found or not, goes on with that part as the name:
//   - "Referenced Object List Location" reads the list (0x55c800), anything else goes to slot 11 of
//     the object (name, value, reader; its own lines, e.g. "Tree List" of a layer).
// The result is true if the last line read (without white space, or as it is) is "}".
// Before reading, the function *0x62e688 (set by 0x4163a0) is called if set.
//
// Binary (0x416f50 / 0x416fe0): the format version 27.0f, (mode 0) the location placeholder and the
// object's address as its ID, the class version (ClassInfo_GetVersion), slot 26 of every field, slot 13
// of the object. The reader takes version 24 (mode 1 only: one more dword skipped, then treated as 25)
// and needs version 27 and the same class version to read the fields (slot 29 each, stops at the
// first false) and slot 15 of the object; a version below 4 (mode 0, or mode 1 without 24) fails at
// once. x87 comparisons as in the original: NaN counts as equal and as less.
//
// Object references (vtable 0x5fb840): slot 19 (0x523eb0) reads "0x%08X (Class)" (the class part is
// cut out while parsing and put back; the class name goes to 0x55cb50, which ignores it), no number
// gives null; slot 26 (0x524000) collects the object as referenced and writes the pointer; slot 29
// (0x524030) reads the ID (the reader's own address if the stream has no 4 bytes left: the original
// reads into the argument's stack slot) and resolves it.
//
// Temp file slot 23 (0x58bd50): "TempFile", "{", and if there is a file: indented "Length=<n>
// bytes", "#### BEGIN TEMPORARY FILE ####", the file's n bytes as they are (the packed form), "####
// END TEMPORARY FILE ####"; then "}" (without a file only "}").
//
// Return values are the original's eax (al for the booleans); Object_ReadText's false results carry
// the upper bytes the original leaves (0xffffff00 or 0 after its inlined strcmp, the upper bytes of
// IsDerivedFrom's result); 0x416fe0's false results after a version comparison carry the FPU status
// word and leftovers of the calls before in the original (not reproduced: 0), after a field's slot 29
// the upper bytes of its result.
#pragma once
#include "object_table.h"

namespace zax {

struct CObjectIO {
    static DWORD THISCALL ObjectReadText(void *object, CObjectWriter *r);      // 0x416b60
    static DWORD THISCALL ObjectWriteBinary(void *object, CObjectWriter *w);   // 0x416f50
    static DWORD THISCALL ObjectReadBinary(void *object, CObjectWriter *r);    // 0x416fe0
    static void __fastcall SetReadHook(void(__cdecl *hook)());                 // 0x4163a0 (ecx)
    static const char *THISCALL SkipSpace(const char *s);                      // 0x587ad0
    static DWORD THISCALL RefFromText(CObjectRefFieldDesc *self, BYTE *object, char *text, CObjectWriter *r);   // 0x523eb0
    static DWORD THISCALL RefWriteBinary(CObjectRefFieldDesc *self, BYTE *object, CObjectWriter *w);           // 0x524000
    static DWORD THISCALL RefReadBinary(CObjectRefFieldDesc *self, BYTE *object, CObjectWriter *r);            // 0x524030
    static DWORD THISCALL TempFileWriteText(CFieldDesc *self, BYTE *object, CObjectWriter *w);                 // 0x58bd50
};

constexpr DWORD READ_HOOK = 0x62e688;           // void (*)(), called before an object is read
constexpr DWORD READ_STOP_OBJECT = 0x62e68c;    // reading this object stops at the field ...
constexpr DWORD READ_STOP_FIELD = 0x62e690;     // ... of this name
constexpr DWORD WHITE_SPACE = 0x622dd8;         // const char *: the characters 0x587ad0 skips
constexpr DWORD TEXT_OBJECT_ID_NAME = 0x60a7b8; // -> "Object ID"
constexpr DWORD TEXT_UNEXPECTED_BRACE = 0x60a808;   // "Found unexpected '{' while parsing class \"%s\". ..."
constexpr DWORD TEXT_FATAL_ERROR = 0x60a7fc;    // "Fatal Error"
constexpr float BINARY_VERSION = 27.0f;

}  // namespace zax
