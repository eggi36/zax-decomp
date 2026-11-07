// File access of Zax.exe: the file buffer CFileBuf (the exe's own name, see the prefix "~CFileBuf 0x")
// around a stream, and the file functions built on it (read or write a whole file, delete, copy,
// exists, a unique file name).
//
// A CFileBuf (0x1c bytes, vtable 0x5f40d4 with 2 slots) holds a stream (+4) that File_Open (0x4a2440)
// creates: a disk file (vtable 0x5f40dc), a file in a mounted archive (ZZipStream, vtable 0x6060d0),
// a file appended to an archive ("archive;name", ZZipWriter, vtable 0x606108) or, with flag 0x200, a
// copy of the whole file in memory (0x505e40, vtable 0x5f9dfc). All reads and writes go through the
// stream's vtable. The exe counts the open buffers (0x6bdf60) and their maximum (0x6bdf50).
//
// Open flags (FileBuf_Open, File_Open): 1 with 2 create ("wb"), 2 change an existing file ("r+b"), 4 read,
// 0x20 disk only (no archives), 0x40 archives only, 0x80 set with 0x20 for absolute paths, 0x200 read
// into memory, 0x400 passed by the archives for their own file, 0x8000 (set by File_Open for reads)
// only archive entries marked by the last listing.
#pragma once
#include "exe_base.h"

namespace zax {

// Base of the streams (0xc bytes, constructor 0x4a23a0, destructor 0x4a2410). The vtable slots:
enum StreamSlot {
    STREAM_DELETE = 0,      // deleting destructor (flags)
    STREAM_WRITE = 2,       // (buffer, n) -> bytes written
    STREAM_READ = 3,        // (buffer, n) -> bytes read
    STREAM_SKIP = 4,        // (delta) relative seek -> bool
    STREAM_SEEK = 5,        // (position) -> bool
    STREAM_TELL = 6,        // () -> position, -1 without a file
    STREAM_SIZE = 7,        // () -> size
    STREAM_SET_SIZE = 8,    // (size)
    STREAM_CLOSE = 9,       // ()
    STREAM_ADVANCE = 11,    // (n) memory streams: pointer to the current position, then position + n
    STREAM_DETACH = 12,     // () memory streams: hands the buffer over and closes
    STREAM_READ_LINE = 13,  // (buffer, n) one line without the line end -> bool
};

struct ZStream {
    DWORD vftable;
    void *handle;   // 0x04 FILE * of the disk stream (fclose in the destructor), 0 in the archive streams
    BYTE owned;     // 0x08 set by File_Open: the destructor closes the handle
};
static_assert(sizeof(ZStream) == 0xc);

struct CFileBuf {
    DWORD vftable;       // 0x00 0x5f40d4: slot 0 deleting destructor 0x4a17a0, slot 1 Close 0x4a2100
    ZStream *stream;     // 0x04
    DWORD indent;        // 0x08 tabs after every WriteLine
    DWORD flags;         // 0x0c open flags
    DWORD small_reads;   // 0x10 number of reads of fewer than 2,000 bytes (statistics, never read)
    DWORD advanced;      // 0x14 sum of the Advance sizes
    BYTE option18;       // 0x18 only set (0x4a2140) and cleared by Open

    CFileBuf *THISCALL Ctor();                                     // 0x4a1780
    CFileBuf *THISCALL DeletingDtor(DWORD flags);                  // 0x4a17a0 (slot 0)
    DWORD THISCALL Dtor();                                         // 0x4a1800
    bool THISCALL Open(const char *path, DWORD flags);             // 0x4a1840 FileBuf_Open
    bool THISCALL Attach(ZStream *stream, DWORD flags);            // 0x4a1970
    DWORD THISCALL Write(const void *p, DWORD n);                  // 0x4a19d0
    DWORD THISCALL WriteChar(char c);                              // 0x4a19f0
    DWORD THISCALL WriteText(const char *s);                       // 0x4a1a10
    DWORD THISCALL WriteLine(const char *s);                       // 0x4a1a40
    DWORD THISCALL Indent();                                       // 0x4a1ab0
    DWORD THISCALL Unindent();                                     // 0x4a1ae0
    DWORD THISCALL Read(void *p, DWORD n);                         // 0x4a1b00
    bool THISCALL Move(DWORD dst, DWORD src, DWORD n);             // 0x4a1b30
    bool THISCALL CopyTo(CFileBuf *dst, void(STDCALL *progress)(float));   // 0x4a1c40
    bool THISCALL Equals(CFileBuf *other);                         // 0x4a1d60
    bool THISCALL ReadString(char **out);                          // 0x4a2010
    DWORD THISCALL ReadLine(char *p, DWORD n);                     // 0x4a20a0 (the stream's eax, al = success)
    DWORD THISCALL Skip(DWORD delta);                              // 0x4a20c0
    DWORD THISCALL Seek(DWORD position);                           // 0x4a20e0
    DWORD THISCALL Close();                                        // 0x4a2100 (slot 1)
    BYTE THISCALL SetOption18(BYTE b);                             // 0x4a2140
    void *THISCALL Advance(DWORD n);                               // 0x4a2150
    void *THISCALL Detach();                                       // 0x4a2170
    DWORD THISCALL Tell();                                         // 0x4a2180
    DWORD THISCALL Size();                                         // 0x4a2190
    DWORD THISCALL SetSize(DWORD n);                               // 0x4a21a0
    CFileBuf *THISCALL WriteDword(DWORD v);                        // 0x4a21c0 (operator <<)
    CFileBuf *THISCALL WriteByte(BYTE v);                          // 0x4a21e0
    CFileBuf *THISCALL WriteQword(DWORD lo, DWORD hi);             // 0x4a2200
    CFileBuf *THISCALL WriteCountedText(const char *s);            // 0x4a2220 (length dword, then the text)
    CFileBuf *THISCALL ReadDword(DWORD *v);                        // 0x4a2270 (operator >>)
    CFileBuf *THISCALL ReadByte(BYTE *v);                          // 0x4a22a0
    CFileBuf *THISCALL ReadQword(void *v);                         // 0x4a22d0
    CFileBuf *THISCALL ReadCountedText(char *out);                 // 0x4a2300 (errno 0x22 if it is longer than the rest)
};
static_assert(sizeof(CFileBuf) == 0x1c);

// File functions (register arguments)
bool __fastcall File_ReadExact(const char *path, void *p, DWORD n);                // 0x4a1300
bool __fastcall File_WriteAll(const char *path, const void *p, DWORD n);           // 0x4a13f0
DWORD __fastcall File_Delete(const char *path, DWORD flags);                       // 0x4a14e0
DWORD __fastcall File_BackupData(const char *path);                              // 0x4a1540
bool __fastcall File_CanOpen(const char *path, DWORD flags);                       // 0x4a1570
bool __fastcall File_Exists(const char *path, DWORD flags);                        // 0x4a1650
DWORD __fastcall File_UniqueName(char *out, const char *pattern, DWORD flags);     // 0x4a16e0
bool __fastcall File_Copy(const char *src, DWORD src_flags, const char *dst, DWORD dst_flags,
                          void(STDCALL *progress)(float));                         // 0x4a1ea0

constexpr DWORD FILE_BUF_VTABLE = 0x5f40d4;
constexpr DWORD STREAM_VTABLE = 0x5f40dc;
constexpr DWORD FILE_BUF_PREFIX = 0x6136ec;   // const char *: "~CFileBuf 0x" + 8 hex digits of a stream pointer
constexpr DWORD FILES_OPEN = 0x6bdf60;        // open file buffers
constexpr DWORD FILES_OPEN_MAX = 0x6bdf50;    // their maximum
constexpr DWORD SMALL_READ = 2000;            // reads below count in CFileBuf::small_reads

}  // namespace zax
