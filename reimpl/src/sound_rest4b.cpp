// The Ogg Vorbis file callbacks (phase 4, sound): ov_callbacks of the music and voice streams over a CFileBuf
// (the data source's +8), 0x525380-0x525400. vorbisfile is built fastcall (section 11).
//
// [confirmed: code; VM]
//  - Read 0x525380 (ptr, size; count, source): FileBuf_Read(ptr, size * count) / size (unsigned; a size of 0
//    divides by zero, as in the original).
//  - Seek 0x5253a0 (source, whence; offset as 64 bits): whence 0 FileBuf_Seek(offset), 1 FileBuf_Skip(offset)
//    (the low dword), 2 FileBuf_Seek(FileBuf_Size()) (the offset ignored), otherwise 0; eax the byte result.
//  - Close 0x525400 (source): the CFileBuf's slot 1 (Close); 1.
// OggCallback_Tell 0x57d310 has a relative jump in its first five bytes (not hookable, section 9 sound).
#include "file_buffer.h"

namespace zax {

namespace {

CFileBuf *buf_of(const void *source) { return *reinterpret_cast<CFileBuf *const *>(static_cast<const BYTE *>(source) + 8); }

}  // namespace

// 0x525380
DWORD __fastcall OggCallback_Read(void *ptr, DWORD size, DWORD count, void *source)
{
    const DWORD got = fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(buf_of(source), ptr, size * count);
    return got / size;
}

// 0x5253a0
DWORD __fastcall OggCallback_Seek(void *source, int whence, DWORD offset, DWORD)
{
    CFileBuf *f = buf_of(source);
    switch (whence) {
    case 0: return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(f, offset) & 0xff;
    case 1: return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20c0)(f, offset) & 0xff;
    case 2: {
        const DWORD size = fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2190)(f);
        return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(buf_of(source), size) & 0xff;
    }
    default: return 0;
    }
}

// 0x525400
DWORD __fastcall OggCallback_Close(void *source)
{
    CFileBuf *f = buf_of(source);
    (*reinterpret_cast<DWORD(THISCALL *const *const *)(CFileBuf *)>(f))[1](f);
    return 1;
}

ZAX_HOOKS(sound_rest4b)
{
    const Hook list[] = {
#include "sound_rest4b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
