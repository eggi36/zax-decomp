// The disk stream of Zax.exe (a FILE * of the C runtime behind the stream vtable 0x5f40dc), File_Open
// that picks the stream for a path (archive writer, archive entry or disk file), the line reader
// shared by the streams, the CRC of a stream and the "~CFileBuf 0x" paths that name a stream in memory.
//
// File_Open (0x4a2440, fastcall path, flags):
// - flags 2 and 1 (create): "archive;name" makes a ZZipWriter (0x5c3660); otherwise first the mounted
//   archives (ZZipList::Open, unless 0x20), then (unless 0x40) the disk;
// - flags 2 without 1 (change an existing file): only the disk (unless 0x40);
// - flag 4 (read): the archives with 0x8000 (entries of the last listing), the disk (unless 0x40),
//   then the archives again with the flags as they are; neither 2 nor 4: null.
// Every stream it returns has "owned" (+8) set.
//
// DiskStream::Open (0x4a2760): flags 3 open "wb" below the data folder (flag 0x20: the path as it is);
// if that fails, the file is not found (0x4a3cb0) and flag 0x100 is set, the folders are created
// (0x4a3ec0) and "wb" is tried again. Otherwise "r+b" (flag 2) or "rb", first below the data folder
// (unless 0x20), then the path as it is.
#pragma once
#include "file_buffer.h"

namespace zax {

// A disk file (0xc bytes from the pool, vtable 0x5f40dc). The slots 1 and 10 (0x401200, xor al,al),
// 11 (0x52dc80, 0) and 12 (0x46b760, 0) are shared with other classes and stay in the exe.
struct DiskStream : ZStream {
    DiskStream *THISCALL Ctor();                          // 0x4a23a0 (base of all streams)
    DiskStream *THISCALL DeletingDtor(DWORD flags);       // 0x4a23c0 (slot 0)
    DWORD THISCALL Dtor();                                // 0x4a2410 (base of all streams)
    DWORD THISCALL Open(const char *path, DWORD flags);   // 0x4a2760 (al = opened, the rest from the FILE *)
    DWORD THISCALL Write(const void *p, DWORD n);         // 0x4a28e0 (slot 2)
    DWORD THISCALL Read(void *p, DWORD n);                // 0x4a2910 (slot 3)
    DWORD THISCALL Skip(DWORD delta);                     // 0x4a2940 (slot 4)
    DWORD THISCALL Seek(DWORD position);                  // 0x4a2970 (slot 5)
    DWORD THISCALL Close();                               // 0x4a29a0 (slot 9)
    DWORD THISCALL Tell();                                // 0x4a2b80 (slot 6)
    DWORD THISCALL Size();                                // 0x4a2ba0 (slot 7)
    DWORD THISCALL SetSize(DWORD n);                      // 0x4a2bf0 (slot 8, also of the memory stream)
};
static_assert(sizeof(DiskStream) == 0xc);

// Slot 13 of the disk stream: one line through the stream's own slots (Tell, Read, Seek)
struct LineReader : ZStream {
    DWORD THISCALL ReadLine(char *p, DWORD n);            // 0x4a2ad0 (al = a line end was found)
};

ZStream *__fastcall File_Open(const char *path, DWORD flags);                    // 0x4a2440
DWORD __fastcall Stream_Crc(ZStream *s);                                          // 0x4a26c0
// out = "~CFileBuf 0x" + the stream's address as 8 hex digits in memory order + "/" + name (edx unused)
void __fastcall Stream_FileBufPath(ZStream *s, DWORD unused, ZString *out, const char *name);   // 0x4a29d0
ZStream *__fastcall Stream_FromFileBufPath(const char *path);                     // 0x4a2a10
DWORD __fastcall Stream_DeleteFileBufPath(const char *path);                      // 0x4a2a70

// Mode strings of the exe for fopen
constexpr DWORD MODE_RB = 0x613840, MODE_RPB = 0x613844, MODE_WB = 0x613848;

}  // namespace zax
