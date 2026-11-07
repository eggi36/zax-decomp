// Value slots of the resource and temp file field kinds (0x54e2c0-0x54e490, 0x54e790, 0x58b8b0-0x58bf20).
//
// Resource kinds (vtables 0x5fd8b0, 0x5fd970, 0x5faf10, 0x5ed320, 0x5fb280): the value is an index
// into a resource list (the named-list interface) that slot 47 of the descriptor returns for the
// object: slot 10 SetDefault, 16 ToText, 19 FromText, 26 WriteBinary, 29 ReadBinary, 32 Use. The
// name is resolved by 0x54e490: the list's Find, then (flag +0x2d) a number below the list's count,
// then FindOrFail (which ends the game for a missing name); setter +0x30 or stored at the offset.
// The default (+0x28, a name) is resolved with g_LastFileOpened set to the static text
// "The Program EXE (C++ Source Code's default value)" (0x712200, built on first use).
//
// Temp file kind (vtable 0x601c30): the value is a char[0xff] from operator new with the path of a
// temp file (GetTempFileNameA, prefix = the exe's name). The data is kept compressed: dword 0x3e9,
// compressed size, size, then the bytes of 0x5c3b40 (data that already starts with 0x3e9 is taken
// as it is). In a text file the data follows "TempFile", "{", "Length=<n> bytes", the line
// "#### BEGIN TEMPORARY FILE ####", the n bytes, "#### END TEMPORARY FILE ####", "}" (slot 23
// 0x58bd50 writes it, in object_io.cpp); in a binary stream the size dword (-1: none) and the
// bytes. Slot 10 deletes the file and frees the path; slot 16 (0x54e790, shared with the embedded
// object kinds) gives the empty text.
#pragma once
#include "field_kinds.h"
#include "file_buffer.h"

namespace zax {

struct CResourceFieldSlots : CResourceFieldDesc {
    DWORD THISCALL SetDefault(BYTE *object);                                  // 0x54e2c0 (slot 10)
    DWORD THISCALL ToText(BYTE *object, ZString *out);                        // 0x54e370 (slot 16)
    DWORD THISCALL FromText(BYTE *object, const char *text, CFileBuf *in);   // 0x54e3a0 (slot 19)
    DWORD THISCALL WriteBinary(BYTE *object, CFileBuf *out);                  // 0x54e3c0 (slot 26)
    DWORD THISCALL ReadBinary(BYTE *object, CFileBuf *in);                    // 0x54e3f0 (slot 29, al)
    DWORD THISCALL Use(BYTE *object);                                         // 0x54e460 (slot 32)
    DWORD THISCALL SetByName(BYTE *object, const char *text);                 // 0x54e490
};

struct CTempFileFieldSlots : CTextFieldDesc {
    DWORD THISCALL ToTextEmpty(BYTE *object, ZString *out);                   // 0x54e790 (slot 16, also 0x5fda30, 0x5fbc9c)
    DWORD THISCALL SetDefault(BYTE *object);                                  // 0x58b8b0 (slot 10)
    DWORD THISCALL FromText(BYTE *object, const char *text, CFileBuf *in);   // 0x58b8f0 (slot 19)
    DWORD THISCALL WriteBinary(BYTE *object, CFileBuf *out);                  // 0x58be70 (slot 26)
    DWORD THISCALL ReadBinary(BYTE *object, CFileBuf *in);                    // 0x58bf20 (slot 29, al = 1)
};

constexpr DWORD RESOURCE_DEFAULT_TEXT = 0x712200, RESOURCE_DEFAULT_GUARD = 0x712211;
constexpr DWORD RESOURCE_DEFAULT_SOURCE = 0x60a6c0;   // "The Program EXE (C++ Source Code's default value)"
constexpr DWORD RESOURCE_DEFAULT_EXIT = 0x54e360;     // atexit: destructor of the static text
constexpr DWORD LAST_FILE_OPENED = 0x713c14;          // ZString g_LastFileOpened
constexpr DWORD TEMP_FILE_END = 0x623320;             // "#### END TEMPORARY FILE ####"
constexpr DWORD TEMP_FILE_PACKED = 0x3e9;

}  // namespace zax
