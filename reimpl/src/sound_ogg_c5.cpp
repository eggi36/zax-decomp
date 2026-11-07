// The Ogg Vorbis reader's slots, the library side of the sound reader (docs/DECOMPILATION.md 10, "Batch C5"; the
// reader's object in sound_reader.cpp: +4 the PCM format, +8 the file buffer, +0xc the PCM bytes, +0x10 the file's
// bytes, +0x18 the OggVorbis_File, +0x1c open, +0x1d reading). vorbisfile is built fastcall (docs/DECOMPILATION.md
// 11): vf in ecx, the second argument in edx. Every call goes through its exe address.
//
// [confirmed: code; VM]
//  - Read 0x5259b0 (slot 2: buffer, bytes): nothing (0) while +0x1d is set; else ov_read (little endian, 16-bit,
//    signed) until the bytes are read or it returns 0; OV_EBADLINK (-137) and OV_HOLE (-3) are skipped, other
//    negative results are added like counts (as in the original); the largest result is kept in 0x6d0160; the
//    bitstream goes into the slot of the bytes argument. eax: the bytes read.
//  - Tell 0x525a30 (slot 3): ov_time_tell · the bytes per second (+4's +8), __ftol.
//  - Seek 0x525a60 (slot 4: position): ov_time_seek(position / bytes per second as a double); eax its result.
//  - Open 0x525aa0 (slot 0: file buffer): Close first (slot 5), +8 the file; a file is opened with the game's
//    callbacks 0x6d0150 (ov_open_callbacks; a failure gives al 0 and leaves); then (also without a file) the PCM
//    bytes ov_pcm_total · channels · 2, the file's size (0x4a2190), the format (a pool block of 0x12 bytes, not
//    tested for null): PCM, the channels and rate of ov_info, 16 bits, block align, bytes per second; +0x1c set,
//    al 1 (eax's upper bytes the format's address).
//  - Close 0x525ba0 (slot 5): ov_clear when open, +0x1c cleared.
#include "reimpl.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

using LD = long double;

inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
inline BYTE &b8(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
inline DWORD &d32(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
inline WORD &w16(void *p, DWORD off) { return *reinterpret_cast<WORD *>(static_cast<BYTE *>(p) + off); }

// 0x5259b0
int THISCALL OggStream_Read(BYTE *self, BYTE *buffer, int bytes)
{
    if (b8(self, 0x1d))
        return 0;
    b8(self, 0x1d) = 1;
    int got = 1, total = 0;
    int bitstream = bytes;
    while (total < bytes && got) {
        got = fn<int(__fastcall *)(void *, BYTE *, int, int, int, int, int *)>(0x5c4e80)(bp(d32(self, 0x18)), buffer + total, bytes - total, 0, 2,
                                                                                    1, &bitstream);
        if (got > *glob<int *>(0x6d0160))
            *glob<int *>(0x6d0160) = got;
        if (got == -137 || got == -3)
            continue;
        if (!got)
            break;
        total += got;
    }
    b8(self, 0x1d) = 0;
    return total;
}

// 0x525a30
int THISCALL OggStream_Tell(BYTE *self)
{
    const LD t = fn<LD(__fastcall *)(void *)>(0x5c4d90)(bp(d32(self, 0x18)));
    return x87_ftol(fild64((long long)d32(bp(d32(self, 4)), 8)) * t);
}

// 0x525a60
int THISCALL OggStream_Seek(BYTE *self, DWORD position)
{
    const double t = (double)(fild64((long long)position) / fild64((long long)d32(bp(d32(self, 4)), 8)));
    return fn<int(__fastcall *)(void *, DWORD, double)>(0x5c4ca0)(bp(d32(self, 0x18)), 0, t);
}

// 0x525aa0
DWORD THISCALL OggStream_Open(BYTE *self, void *file)
{
    vcall<DWORD>(self, 5);
    d32(self, 8) = ptrv(file);
    if (file) {
        struct Callbacks {
            DWORD read, seek, close, tell;
        };
        const Callbacks cb = *glob<Callbacks *>(0x6d0150);
        const DWORD r = fn<DWORD(__fastcall *)(void *, void *, DWORD, DWORD, Callbacks)>(0x5c3ce0)(self, bp(d32(self, 0x18)), 0, 0, cb);
        if (r)
            return r & 0xffffff00u;
    }
    BYTE *info = fn<BYTE *(__fastcall *)(void *, int)>(0x5c4e40)(bp(d32(self, 0x18)), -1);
    const DWORD total = (DWORD)fn<long long(__fastcall *)(void *, int)>(0x5c4590)(bp(d32(self, 0x18)), -1);
    d32(self, 0xc) = total * d32(info, 4) << 1;
    d32(self, 0x10) = fn<DWORD(THISCALL *)(void *)>(0x4a2190)(bp(d32(self, 8)));
    BYTE *fmt = static_cast<BYTE *>(pool_alloc(0x12));
    d32(self, 4) = ptrv(fmt);
    d32(fmt, 0) = 0;
    d32(fmt, 4) = 0;
    d32(fmt, 8) = 0;
    d32(fmt, 0xc) = 0;
    w16(fmt, 0x10) = 0;
    w16(bp(d32(self, 4)), 0) = 1;
    w16(bp(d32(self, 4)), 2) = w16(info, 4);
    d32(bp(d32(self, 4)), 4) = d32(info, 8);
    BYTE *f = bp(d32(self, 4));
    w16(f, 0xe) = 0x10;
    w16(f, 0xc) = (WORD)((int)((DWORD)w16(f, 0xe) * (DWORD)w16(f, 2)) / 8);
    d32(f, 8) = (DWORD)w16(f, 0xc) * d32(f, 4);
    b8(self, 0x1c) = 1;
    return (ptrv(f) & 0xffffff00u) | 1;
}

// 0x525ba0
void THISCALL OggStream_Close(BYTE *self)
{
    if (b8(self, 0x1c)) {
        fn<int(__fastcall *)(void *)>(0x5c3bf0)(bp(d32(self, 0x18)));
        b8(self, 0x1c) = 0;
    }
}

}  // namespace

ZAX_HOOKS(sound_ogg_c5)
{
    const Hook list[] = {
#include "sound_ogg_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
