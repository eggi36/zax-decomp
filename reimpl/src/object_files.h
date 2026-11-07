// Objects read from and written to files, the change test, reading an object from a stream, and the
// ZString stream operators.
//
// Object_CreateFromFile (0x4164c0, fastcall: path ZString, flags, write back, &object): opens an object
// stream on the file (flags | 0x204, mode 1), sets g_LastFileOpened to the path (restored at the end),
// reads the first line and creates the class it names (ClassRegistry_Create (name, 0, -1): a missing or
// abstract class ends the game), stores the object at once if asked, calls its slots 16 and 19 (19
// sets the field defaults), reads it with Object_ReadText and, if that succeeded, visits its objects
// (slot 18) with a QueryVisitor when the table's mode is not 0 (mode 1 here: always). After the finish
// it writes the object back to the file (0x4167e0 with flags 0) if asked and the read succeeded; a
// failed read deletes the object (slot 1 with 1) and gives null. The stored pointer then dangles.
// The line read for the class name is not checked: an empty file leaves the buffer as it was.
//
// Object_ReadFile (0x416a00, thiscall object (path, flags)): the same into an existing object (flags | 4,
// mode 1); g_LastFileOpened is restored before the finish. A file that does not open still gets slot 19.
//
// Object_WriteFile (0x4167e0, thiscall object (path, flags)): only if File_Exists(path, flags); writes
// the object as text (mode 1) into a memory stream, compares it with the file (flags | 4) and rewrites
// the file (flags | 3, CopyTo) when it differs or cannot be read. True if the file holds the text now.
//
// Object_HasChanged (0x417100, thiscall object (other)): both objects written as text (mode 1) into
// memory streams; true unless the two texts have the same length and bytes.
//
// Object_ReadFromStream (0x416af0, thiscall object (stream)): slots 16 and 19, Object_ReadText, and if
// that succeeded the visit as in 0x4164c0 (with the visitor if the stream's table mode is not 0).
//
// FileBuf_WriteZString / FileBuf_ReadZString (0x4162d0 / 0x416310, fastcall (stream, ZString)): the
// string as counted text (empty strings as EmptyText); reading sets errno to 0, reads the length and,
// if it is not longer than the rest of the stream (else errno ERANGE), the text into the string's
// buffer (GetBufferDiscard, Read, ReleaseBuffer) or clears the string for length 0. The length is read
// into an uninitialised slot that holds the caller's ecx (the stream) when the read fails.
//
// Return values are the original's eax: the booleans carry in the upper bytes the eax of the last call
// before (the stream destructor 0x5241e0 for 0x416a00 and 0x4167e0, File_Exists when the file is
// missing, slot 18 for 0x416af0); 0x417100 returns 0 or 1.
#pragma once
#include "object_table.h"

namespace zax {

struct CObjectFiles {
    static void *__fastcall CreateFromFile(ZString *path, DWORD flags, DWORD write_back, void **out);   // 0x4164c0
    static DWORD THISCALL WriteFile(void *object, const char *path, DWORD flags);                       // 0x4167e0
    static DWORD THISCALL ReadFile(void *object, const char *path, DWORD flags);                        // 0x416a00
    static DWORD THISCALL ReadFromStream(void *object, CObjectWriter *r);                               // 0x416af0
    static DWORD THISCALL HasChanged(void *object, void *other);                                        // 0x417100
    static CFileBuf *__fastcall WriteZString(CFileBuf *f, ZString *s);                                  // 0x4162d0
    static CFileBuf *__fastcall ReadZString(CFileBuf *f, ZString *s);                                   // 0x416310
};

constexpr DWORD OBJECT_FILE_FLAGS = 0x204;   // or-ed into the flags of 0x4164c0

}  // namespace zax
