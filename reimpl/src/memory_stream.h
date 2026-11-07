// The memory stream of Zax.exe (vtable 0x5f9dfc): a growing buffer behind the stream slots. FileBuf_Open
// with flag 0x200 reads a whole file into one (File_OpenInMemory 0x505e40); the resource and temp file
// code builds them directly (constructors 0x505f00, 0x505fb0).
//
// Quirks kept: reads stop at the size, but a position beyond it is not caught (Read then copies
// size - position bytes, which wraps around); the line reader only ends lines at '\r' (a following
// '\n' is skipped as well), so text with bare '\n' line ends comes back as one line if it fits; a line
// that does not fit into n - 1 bytes returns false without moving. Reserve only grows a buffer the
// stream owns (or none at all); writes into a borrowed buffer go past its end.
#pragma once
#include "file_buffer.h"

namespace zax {

// 0x20 bytes from the pool. The slots 1 and 10 (0x401260, mov al,1), 6 (Tell, 0x483750: +0xc) and
// 7 (Size, 0x4ef8f0: +0x10) are shared with other classes and stay in the exe; slot 8 is the disk
// stream's SetSize (0x4a2bf0) on the null handle.
struct MemoryStream : ZStream {
    DWORD position;   // 0x0c
    DWORD size;       // 0x10
    DWORD capacity;   // 0x14
    BYTE *data;       // 0x18 operator new
    BYTE owns;        // 0x1c the stream deletes data

    MemoryStream *THISCALL Ctor();                                // 0x505f00
    MemoryStream *THISCALL DeletingDtor(DWORD flags);             // 0x505f50 (slot 0)
    MemoryStream *THISCALL CtorData(BYTE *p, DWORD n);            // 0x505fb0 (takes over p)
    DWORD THISCALL Dtor();                                        // 0x506000 (Stream::Dtor's result)
    DWORD THISCALL Write(const void *p, DWORD n);                 // 0x506040 (slot 2)
    DWORD THISCALL Read(void *p, DWORD n);                        // 0x5060a0 (slot 3)
    DWORD THISCALL Skip(DWORD delta);                             // 0x5060e0 (slot 4, al = 1)
    DWORD THISCALL Seek(DWORD p);                                 // 0x506100 (slot 5, al = 1)
    void THISCALL Close();                                        // 0x506110 (slot 9)
    DWORD THISCALL ReadLine(char *p, DWORD n);                    // 0x506140 (slot 13; al = result, the rest from this)
    void THISCALL Reset(DWORD n);                                 // 0x5061e0 (Close, then Reserve(n))
    BYTE *THISCALL Advance(DWORD n);                              // 0x506210 (slot 11)
    BYTE *THISCALL Detach();                                      // 0x506230 (slot 12)
    void THISCALL Reserve(DWORD n);                               // 0x506250
    DWORD THISCALL Equals(MemoryStream *other);                   // 0x5062d0
};
static_assert(sizeof(MemoryStream) == 0x20);

ZStream *__fastcall File_OpenInMemory(const char *path, DWORD flags);   // 0x505e40

constexpr DWORD MEMORY_STREAM_VTABLE = 0x5f9dfc;

}  // namespace zax
