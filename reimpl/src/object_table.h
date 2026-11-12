// The object stream of Zax.exe (constructor 0x5240b0, vtable 0x5fb8fc) and its object table
// (constructor 0x55c6f0). The stream is a CFileBuf with a table (+0x1c) and a "finished" byte (+0x20);
// the same class writes and reads objects (struct CObjectWriter in field_streams.h, named after its
// first known use). It is opened on a file (0x5242b0, FileBuf_Open) or on a stream (0x5242f0, Attach
// with flags 0); the flags and the mode go to the table.
//
// The table connects object pointers in memory with the object IDs in a file:
//   writing (mode 0): every written object is collected (0x55cb30, the text "Object ID=0x<address>" or
//     the binary address follows), every object reference too (0x55cad0, once each). The first object
//     reserves the place of the list: the text line "Referenced Object List Location=0x00420042"
//     (0x55c780) or the binary dword 0x2a (0x55c7d0). At the finish (0x55cd90, 0x55ce50) the list
//     "Referenced Objects" (text: "{", the count, "0x%08X" per object, "}"; binary: 0x12345678, the
//     count, the addresses) is appended and its position written over the placeholder (the text
//     placeholder has the same length). A referenced object that was not written is reported as a
//     "***Programming Error***" through the reporter *0x6c02cc (slot 16).
//   reading (mode 0, flags with 4): the reader of the location (text 0x55c800, binary 0x55c9b0) seeks
//     to the list, reads the referenced IDs and seeks back. Objects that are read register their ID
//     (0x55cc90; once the list is known only the IDs in it). A reference (0x55cb50) is resolved at once
//     if its object is registered, otherwise the field holds the ID until the finish resolves it (an ID
//     without an object becomes null).
//   mode 1 (the game's change test 0x417100; reading an object from a file, 0x4164c0 creating it by the
//     class name in the first line, 0x416a00 into an existing one) neither writes IDs nor resolves
//     references (they become null).
//
// Return values are what the original leaves in eax. Where it returns early without touching eax
// (0x55c780/0x55c7d0/0x55c800/0x55c9b0 with the location known, 0x55cad0 with null or mode 1, 0x55cb50
// and 0x55cc90 with mode 1, 0x55cd90 when not open, 0x524320 when already finished) the caller's eax
// comes back; the reimplementation returns 0 there (al = 0 for 0x524320, as in the original).
#pragma once
#include "field_streams.h"

namespace zax {

// The four lists of the table: {vtable, data (operator new), count, capacity}, grown like the array
// templates (max(count + 1, capacity * 1.2))
template <typename T> struct ZTableList {
    DWORD vftable;
    T *data;
    DWORD count;
    DWORD capacity;
};
struct ObjectIdEntry {   // +0x1c: an object that was read and its ID in the file
    DWORD id;
    void *object;
};
struct ObjectFixup {     // +0x2c: a reference whose object was not read yet
    DWORD id;
    DWORD zero;          // 0x4 always 0
    void **field;        // 0x8 holds the ID until the finish
    BYTE resolved;       // 0xc always 0 (the finish skips entries where it is set)
    BYTE pad[3];
};

struct ObjectTable {
    BYTE open;           // 0x00 set by Open, cleared by the finish
    BYTE pad1[3];
    DWORD location;      // 0x04 position of the location placeholder (writing) or of the reader (reading), -1 none
    BYTE list_read;      // 0x08 the referenced list was read: only its IDs register
    BYTE pad9[3];
    DWORD format;        // 0x0c 0 text, 1 binary, 2 (constructor) not decided; Open does not reset it
    DWORD flags;         // 0x10 open flags (4: reading)
    CObjectWriter *stream;   // 0x14
    DWORD mode;          // 0x18 0 with IDs and references, 1 without, others neither
    ZTableList<ObjectIdEntry> loaded;     // 0x1c vtable 0x5fb904
    ZTableList<ObjectFixup> fixups;       // 0x2c vtable 0x5fb910
    ZTableList<void *> referenced;        // 0x3c vtable 0x5fb91c (writing: objects, reading: IDs)
    ZTableList<void *> written;           // 0x4c vtable 0x5fb91c

    ObjectTable *THISCALL Ctor();                                          // 0x55c6f0
    DWORD THISCALL Open(CObjectWriter *s, DWORD flags, DWORD mode);       // 0x55c750
    DWORD THISCALL WriteLocation();                                       // 0x55c780
    DWORD THISCALL WriteLocationBinary();                                 // 0x55c7d0
    DWORD THISCALL ReadLocation(const char *text);                       // 0x55c800
    // 0x55c9b0 ReadLocationBinary: object_table.cpp's zax_table_read_location_binary_entry, in the exe's frame
    DWORD THISCALL AddReference(void *object);                            // 0x55cad0
    DWORD THISCALL AddWritten(void *object);                              // 0x55cb30
    DWORD THISCALL Resolve(DWORD id, const char *class_name, void **field);   // 0x55cb50 (class name unused)
    DWORD THISCALL Register(DWORD id, void *object);                     // 0x55cc90
    DWORD THISCALL Finish();                                              // 0x55cd90
    DWORD THISCALL WriteList();                                           // 0x55ce50
};
static_assert(sizeof(ObjectTable) == 0x5c && sizeof(ObjectFixup) == 16);

// The stream's own methods (struct in field_streams.h)
struct CObjectStream {
    static CObjectWriter *THISCALL Ctor(CObjectWriter *self);                                    // 0x5240b0
    static CObjectWriter *THISCALL DeletingDtor(CObjectWriter *self, DWORD flags);               // 0x5240f0 (slot 0)
    static DWORD THISCALL Dtor(CObjectWriter *self);                                             // 0x5241e0
    static DWORD THISCALL OpenFile(CObjectWriter *self, const char *path, DWORD flags, DWORD mode);   // 0x5242b0
    static DWORD THISCALL Open(CObjectWriter *self, ZStream *stream, DWORD flags, DWORD mode);   // 0x5242f0
    static DWORD THISCALL FinishTable(CObjectWriter *self);                                      // 0x524320
    static DWORD THISCALL Finish(CObjectWriter *self);                                           // 0x524340 (slot 1)
};

constexpr DWORD VT_OBJECT_STREAM = 0x5fb8fc;
constexpr DWORD VT_ID_LIST = 0x5fb904, VT_FIXUP_LIST = 0x5fb910, VT_DWORD_LIST = 0x5fb91c;
constexpr DWORD ERROR_REPORTER = 0x6c02cc;      // object pointer, slot 16 (text)
constexpr DWORD TEXT_PROGRAMMING_ERROR = 0x620428;
constexpr DWORD TEXT_REFERENCED_OBJECTS = 0x6203f0;   // -> "Referenced Objects"
constexpr DWORD TEXT_LIST_LOCATION = 0x6203ec;        // -> "Referenced Object List Location"
constexpr DWORD LOCATION_PLACEHOLDER = 0x420042, LOCATION_BINARY_PLACEHOLDER = 0x2a, LIST_MAGIC = 0x12345678;

}  // namespace zax
