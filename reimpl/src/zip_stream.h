// Streams of the zip archives of Zax.exe: opening a file of a mounted archive (ZZipStream), writing into
// an existing entry of a zip below "." and appending a new file to an archive (ZZipWriter).
//
// File_Open (0x4a2440) asks the mounted archives (ZZipList::Open 0x5c1c70 -> ZZipDir::OpenFile 0x5c2e50)
// for reads (flag 4, first with 0x8000: only entries the last listing added) and for flags 3 (write into
// an existing file); a path "archive;name" with flags 3 creates a ZZipWriter instead. A ZZipStream reads
// the stored data of one entry through the archive's file buffer (ZZipDir +4), which stays open while
// streams are open (ZZipDir +0x1c counts them). Writing (only into the lazily mounted *.zip below ".",
// which have check_disk) first moves the entry to the end of the archive, then writes there and at Close
// the CRC, the local header and, when the last stream closes, the central directory and the end record.
#pragma once
#include "file_buffer.h"
#include "zip_archive.h"

namespace zax {

// The parts of ZZipDir in this module (the structure is in zip_archive.h: +1 is set when the central
// directory has to be written again, +0x1c counts the open streams, ZZipEntry +0x2e marks an open entry)
struct ZZipDirOps : ZZipDir {
    ZZipEntry *THISCALL MoveEntryToEnd(ZZipEntry *e);              // 0x5c2080
    DWORD THISCALL WriteEnd(DWORD directory_offset);               // 0x5c2dd0
    ZStream *THISCALL OpenFile(const char *path, DWORD mode);       // 0x5c2e50
    DWORD THISCALL WriteLocalHeader(const ZZipEntry *e);           // 0x5c30d0
};

// A file of an archive (0x24 bytes from the pool, vtable 0x6060d0)
struct ZZipStream : ZStream {
    DWORD own_file;     // 0x0c (mode >> 4) & 1: reads through a file buffer of its own
    DWORD position;     // 0x10 in the archive
    ZZipEntry *entry;   // 0x14
    CFileBuf *file;     // 0x18 the archive's (ZZipDir +4) or its own
    ZZipDir *dir;       // 0x1c
    DWORD mode;         // 0x20 open flags (2: CRC and local header at Close)

    bool THISCALL Open(DWORD data_offset, ZZipEntry *e, CFileBuf *f, const char *archive, ZZipDir *d, DWORD m);   // 0x5c35c0
    ZZipStream *THISCALL DeletingDtor(DWORD flags);   // 0x5c31f0 (slot 0)
    DWORD THISCALL Write(const void *p, DWORD n);     // 0x5c3220 (slot 2)
    DWORD THISCALL Read(void *p, DWORD n);            // 0x5c3290 (slot 3)
    DWORD THISCALL Skip(DWORD delta);                 // 0x5c3320 (slot 4)
    DWORD THISCALL Seek(DWORD position);              // 0x5c3380 (slot 5)
    DWORD THISCALL Tell();                            // 0x5c33c0 (slot 6)
    DWORD THISCALL Size();                            // 0x5c33f0 (slot 7)
    DWORD THISCALL SetSize(DWORD n);                  // 0x5c3400 (slot 8)
    void THISCALL Close();                            // 0x5c3440 (slot 9)
};
static_assert(sizeof(ZZipStream) == 0x24);

#pragma pack(push, 2)
// End record of an archive without its signature (0x12 bytes)
struct ZipEndRecord {
    WORD disk, directory_disk;
    WORD count;              // entries on this disk
    WORD total;              // entries
    DWORD directory_size;
    DWORD directory_offset;
    WORD comment_length;
};
#pragma pack(pop)
static_assert(sizeof(ZipEndRecord) == 0x12);

// A new file at the end of an archive (0x148 bytes from the pool, vtable 0x606108; "archive;name" with
// flags 3 in File_Open). Open overwrites the central directory with the local header, Close writes the
// central directory back behind the data with one entry more and always the end signature "RE\5\6".
struct ZZipWriter : ZStream {
    CFileBuf file;           // 0x0c the archive, opened with 0x26
    char name[0x100];        // 0x28
    BYTE *directory;         // 0x128 the old central directory (operator new)
    ZipEndRecord end;        // 0x12c
    DWORD header_offset;     // 0x140 the new local header (the old directory offset)
    DWORD data_offset;       // 0x144

    bool THISCALL Open(const char *archive, const char *file_name);   // 0x5c38d0
    ZZipWriter *THISCALL DeletingDtor(DWORD flags);   // 0x5c36c0 (slot 0)
    DWORD THISCALL Read(void *p, DWORD n);            // 0x5c3a70 (slot 3)
    DWORD THISCALL Write(const void *p, DWORD n);     // 0x5c3a90 (slot 2)
    DWORD THISCALL Skip(DWORD delta);                 // 0x5c3ab0 (slot 4)
    DWORD THISCALL Seek(DWORD position);              // 0x5c3ac0 (slot 5)
    DWORD THISCALL Tell();                            // 0x5c3ae0 (slot 6)
    DWORD THISCALL Size();                            // 0x5c3b00 (slot 7)
    DWORD THISCALL SetSize(DWORD n);                  // 0x5c3b20 (slot 8)
    void THISCALL Close();                            // 0x5c3700 (slot 9)
};
static_assert(sizeof(ZZipWriter) == 0x148);

ZZipWriter *__fastcall ZipWriter_Create(const char *archive, const char *file_name);   // 0x5c3660
DWORD __fastcall Crc_Final(DWORD crc);                                                 // 0x5c31a0
DWORD __fastcall Crc_Update(DWORD crc, const void *p, DWORD n);                        // 0x5c31b0

constexpr DWORD ZIP_STREAM_VTABLE = 0x6060d0, ZIP_WRITER_VTABLE = 0x606108;
constexpr DWORD CRC_TABLE = 0x627814;

}  // namespace zax
