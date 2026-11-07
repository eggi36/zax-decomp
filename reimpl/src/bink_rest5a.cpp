// The Bink wrapper's functions that call binkw32.dll, and the file callbacks Bink calls itself (phase 5a,
// group G4; docs/DECOMPILATION.md 10, "Group G4" and the batch P5a-2). The rest of the wrapper (the movie
// frame, AddMovie, AtEnd, BinkIo_Open, App_PlayMovie) is movie_rest4a.cpp. Every binkw32 and kernel32
// call goes through the same import entry as in the original.
//
// The Bink system (the global 0x62eaf0, vtable 0x5eede0) [confirmed: code]: +4 byte "sound system set",
// +8 the records (0x18 bytes: +0 the Bink handle, +4 a Bink buffer (closed, never opened here), +8 the frame
// shown, +0x14 byte "first wait"), +0xc their count, +0x1c/+0x20 a scratch block of 24-bit pixels and its
// size (the 8-bit path of DecodeToFrame), +0x24 byte: sound track 0 instead of -1.
//   BinkSystem_Dtor 0x431e50: the vtable, the sound flag cleared, every record's buffer and handle closed
//     (BinkBufferClose, BinkClose; the inlined CloseMovie keeps its "record not null" test, an offset of
//     -0x18), the records and the scratch block freed; eax 0 or operator delete's.
//   OpenMovie 0x431fd0 (name, flags): BinkSetIO(BinkIo_Open 0x432aa0), once BinkSetSoundSystem(
//     BinkOpenDirectSound, the DirectSound object 0x7136f4); no name: BinkLogoAddress() with 0x4000000; flag 1
//     0x4000000, flag 2 0x800000 (the stream), neither 0x2000000; always | 0x4000; BinkSetSoundTrack(0 or -1),
//     BinkOpen, AddMovie; the record index or -1.
//   CloseMovie 0x432080 (id): BinkBufferClose and BinkClose of the record, both cleared.
//   Wait 0x4320d0 (id): -1 gives al 0; the first wait of a record gives 1 and clears the flag; else
//     BinkWait() == 0.
//   DecodeToFrame 0x432110 (id, dest, pitch, height, x, y, bits): bits 16, 24, 32 copy straight into the
//     destination (BinkCopyToBuffer flags 10 = 565, 1 = 24 bit, 3 = 32 bit); 8 bits decode as 24 bits into
//     the scratch block (grown to pitch * height * 3) and map every pixel through the 16-bit tables
//     0x630378 (first byte), 0x6753b8 (second), 0x633b94 (third) and the byte table *0x633388; any other
//     depth does nothing (eax bits - 8). The frame counter of the record then counts up and starts again at 1
//     behind the handle's frame count (+8); eax the record. A pitch of 0 in the 8-bit path loops 2^32 times
//     (do/while, as the original).
//   GetSummary 0x432320 (id, out): BinkGetSummary into 0x7c bytes; out = width, height, +0x20, +0x14.
//   SetPause 0x4323a0 (id, byte): BinkPause(handle, byte != 0).
//
// The file callbacks (stdcall, the BINKIO block first; the block's fields after the Bink 1 SDK header:
// +0x18 the Bink handle, +0x1c "doing a read", +0x20 bytes read, +0x24 "working", +0x28 total time, +0x2c
// foreground time, +0x30 idle time, +0x34 thread time, +0x38 buffer size, +0x3c buffer high water mark,
// +0x40 current buffer size, +0x44 bytes buffered, +0x48 the game's data [names: assumption; the uses:
// confirmed]). The game's data: +0x48 the FileBuf, +0x4c the file position, +0x50 Bink's position, +0x54
// the buffer's read pointer, +0x58 the free space, +0x5c the foreground counter, +0x60 the background
// counter, +0x64/+0x68 the buffer's start and end, +0x6c the write pointer, +0x70 the stream's base offset,
// +0x74 the file size, +0x78 the simulated rate (bytes per second, 0 = none), +0x7c the rate's time debt.
//   The "stream flag": the foreground callbacks take the foreground counter with lock inc byte and wait
//   while the background counter is not 0 (Sleep(1), BinkService); the background callback (Bink's
//   thread) takes both and works only when both are exactly 1 (dword compares), else gives -1 after
//   Sleep(1). Kept exactly, with the atomic byte increments.
//   0x4323d0 ReadHeader (offset, dest, size): a seek when the offset is not -1 and not the file position,
//     the read, the positions and +0x40.
//   0x432480 ReadFrame (frame, offset, dest, size): a jump inside the buffered data or a seek that empties
//     the buffer, the buffered bytes copied (wrapping at the buffer's end), the rest read from the file
//     with the simulated rate's waiting; the times and +0x40.
//   0x432770 GetBufferSize (size): rounded up to 4 KB.
//   0x4327e0 SetInfo (buffer, size, file size, rate): the buffer (size down to 4 KB) and the rate.
//   0x432870 Close: the FileBuf deleted (slot 0).
//   0x4328d0 Idle (the background): 4 KB from the file into the buffer when there are 4 KB of room and of
//     file left, with the rate's waiting; the bytes read, -1 when busy.
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

constexpr DWORD IAT_PAUSE = 0x5ea2f8, IAT_BUFFER_CLOSE = 0x5ea2fc, IAT_CLOSE = 0x5ea300, IAT_DO_FRAME = 0x5ea304;
constexpr DWORD IAT_NEXT_FRAME = 0x5ea308, IAT_SUMMARY = 0x5ea30c, IAT_OPEN = 0x5ea310, IAT_SOUND_TRACK = 0x5ea314;
constexpr DWORD IAT_LOGO = 0x5ea318, IAT_SOUND_SYSTEM = 0x5ea31c, IAT_OPEN_DS = 0x5ea320, IAT_SET_IO = 0x5ea324;
constexpr DWORD IAT_WAIT = 0x5ea328, IAT_SERVICE = 0x5ea32c, IAT_COPY = 0x5ea330, IAT_TIMER = 0x5ea334, IAT_SLEEP = 0x5ea07c;
constexpr DWORD SYSTEM_VTABLE = 0x5eede0;

template <typename T = DWORD> T &f(void *o, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(o) + off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

// An import (stdcall) called through its entry
template <typename R, typename... A> R api(ImpKey iat, A... a) { return (*imp_slot<R(STDCALL **)(A...)>(iat))(a...); }

inline void lock_inc(BYTE *p) { __asm__ volatile("lock incb %0" : "+m"(*p) : : "memory"); }
inline void lock_dec(BYTE *p) { __asm__ volatile("lock decb %0" : "+m"(*p) : : "memory"); }
inline DWORD vread(BYTE *io, DWORD off) { return *reinterpret_cast<volatile DWORD *>(io + off); }

// (n * 1000) / rate with the 64-bit product and a 32-bit quotient (mul, div)
inline DWORD rate_time(DWORD n, DWORD rate)
{
    DWORD q, d;
    __asm__("mull %[k]\n\tdivl %[r]" : "=a"(q), "=&d"(d) : "a"(n), [k] "r"(1000u), [r] "r"(rate) : "cc");
    return q;
}

// While the background callback works: Sleep(1), BinkService(handle)
void wait_background(BYTE *io)
{
    while (vread(io, 0x60)) {
        api<DWORD>(IAT_SLEEP, 1u);
        api<DWORD>(IAT_SERVICE, f(io, 0x18));
    }
}

DWORD timer() { return api<DWORD>(IAT_TIMER); }
DWORD filebuf_read(void *fb, void *dest, DWORD n) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, dest, n); }
void filebuf_seek(void *fb, DWORD pos) { fn<void(THISCALL *)(void *, DWORD)>(0x4a20e0)(fb, pos); }

// The simulated rate's waiting after reading n bytes since t0: the debt grows by the time the bytes should
// have taken minus the time they took and is slept off in steps of Sleep(1)
void rate_wait(BYTE *io, DWORD n, DWORD t0)
{
    const DWORD q = rate_time(n, f(io, 0x78));
    DWORD base = timer();
    f(io, 0x7c) += q - base + t0;
    if ((int)f(io, 0x7c) <= 0)
        return;
    do {
        api<DWORD>(IAT_SLEEP, 1u);
        const DWORD t = timer();
        if ((int)(t - base) < (int)f(io, 0x7c))
            continue;
        f(io, 0x7c) += base - t;
        base = t;
    } while ((int)f(io, 0x7c) > 0);
}

// The current buffer size: what is left of the file, at most the buffer
void cur_size(BYTE *io)
{
    const DWORD left = f(io, 0x74) - f(io, 0x50);
    f(io, 0x40) = left < f(io, 0x38) ? left : f(io, 0x38);
}

}  // namespace

// ---- The Bink system ----------------------------------------------------------------------------------------

DWORD THISCALL BinkSystem_Dtor(BYTE *s)
{
    f(s, 0) = vt(SYSTEM_VTABLE);
    s[4] = 0;
    if ((int)f(s, 0xc) > 0) {
        auto buffer_close = *imp_slot<DWORD(STDCALL **)(DWORD)>(IAT_BUFFER_CLOSE);
        DWORD off = 0;
        int i = 0;
        do {
            if (const DWORD b = f(bp(f(s, 8)) + off, 4)) {
                buffer_close(b);
                f(bp(f(s, 8)) + off, 4) = 0;
            }
            if (f(bp(f(s, 8)) + off, 0) && off != 0xffffffe8u) {
                if (const DWORD b = f(bp(f(s, 8)) + off, 4)) {
                    buffer_close(b);
                    f(bp(f(s, 8)) + off, 4) = 0;
                }
                if (const DWORD h = f(bp(f(s, 8)) + off, 0)) {
                    api<DWORD>(IAT_CLOSE, h);
                    f(bp(f(s, 8)) + off, 0) = 0;
                }
            }
            i++;
            off += 0x18;
        } while (i < (int)f(s, 0xc));
    }
    if (f(s, 8)) {
        heap_free(bp(f(s, 8)));
        f(s, 8) = 0;
    }
    const DWORD scratch = f(s, 0x1c);
    f(s, 0xc) = 0;
    if (scratch) {
        heap_free(bp(scratch));
        f(s, 0x1c) = 0;
    }
    return 0;   // the original's eax: 0, or operator delete's when there was a scratch block
}

DWORD THISCALL BinkSystem_OpenMovie(BYTE *s, const char *name, DWORD flags)
{
    api<DWORD>(IAT_SET_IO, fn_addr(0x432aa0u));
    if (!s[4]) {
        const DWORD open_ds = *imp_slot<DWORD *>(IAT_OPEN_DS);
        s[4] = 1;
        api<DWORD>(IAT_SOUND_SYSTEM, open_ds, *glob<DWORD *>(0x7136f4));
    }
    DWORD open = 0;
    if (!name) {
        name = api<const char *>(IAT_LOGO);
        open = 0x4000000;
    } else {
        if (flags & 1)
            open = 0x4000000;
        if (flags & 2)
            open |= 0x800000;
        if (!open)
            open = 0x2000000;
    }
    open |= 0x4000;
    api<DWORD>(IAT_SOUND_TRACK, s[0x24] ? 0u : 0xffffffffu);
    const DWORD h = api<DWORD>(IAT_OPEN, name, open);
    if (!h)
        return 0xffffffff;
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x431f20)(s, h);
}

DWORD THISCALL BinkSystem_CloseMovie(BYTE *s, DWORD id)
{
    if (id == 0xffffffff)
        return id;
    const DWORD off = id * 0x18;
    DWORD r = f(bp(f(s, 8)) + off, 4);
    if (r) {
        api<DWORD>(IAT_BUFFER_CLOSE, r);
        f(bp(f(s, 8)) + off, 4) = 0;
    }
    r = f(bp(f(s, 8)) + off, 0);
    if (r) {
        api<DWORD>(IAT_CLOSE, r);
        r = f(s, 8);
        f(bp(r) + off, 0) = 0;
    }
    return r;
}

DWORD THISCALL BinkMovie_Wait(BYTE *s, DWORD id)
{
    if (id == 0xffffffff)
        return 0xffffff00;
    BYTE *r = bp(f(s, 8)) + id * 0x18;
    if (r[0x14]) {
        r[0x14] = 0;
        return (pv(r) & ~0xffu) | 1;
    }
    return api<DWORD>(IAT_WAIT, f(r, 0)) == 0 ? 1 : 0;
}

DWORD THISCALL BinkMovie_DecodeToFrame(BYTE *s, DWORD id, BYTE *dest, DWORD pitch, DWORD height, DWORD x, DWORD y, DWORD bits)
{
    const DWORD off = id * 0x18;
    const DWORD bink = f(bp(f(s, 8)) + off, 0);
    const DWORD k = bits - 8;
    if (k > 0x18)
        return k;
    const BYTE kind = glob<const BYTE *>(0x4322f8)[k];
    DWORD flags;
    if (kind == 0) {
        const DWORD need = pitch * height * 3;
        if (f(s, 0x20) < need) {
            if (f(s, 0x1c)) {
                heap_free(bp(f(s, 0x1c)));
                f(s, 0x1c) = 0;
            }
            f(s, 0x20) = need;
            f(s, 0x1c) = pv(heap_alloc(need));
        }
        api<DWORD>(IAT_DO_FRAME, bink);
        api<DWORD>(IAT_COPY, bink, f(s, 0x1c), pitch * 3, height, x, y, 1u);
        const BYTE *src = bp(f(s, 0x1c));
        BYTE *row = dest;
        for (DWORD r = height; r; r--) {
            const BYTE *p = src;
            BYTE *d = row;
            DWORD n = pitch;
            do {
                const DWORD idx = glob<const DWORD *>(0x630378)[p[0]] + glob<const DWORD *>(0x6753b8)[p[1]] + glob<const DWORD *>(0x633b94)[p[2]];
                *d++ = (*glob<const BYTE **>(0x633388))[idx];
                p += 3;
            } while (--n);
            row += pitch;
            src += pitch * 3;
        }
        api<DWORD>(IAT_NEXT_FRAME, bink);
    } else if (kind <= 3) {
        flags = kind == 1 ? 10 : kind == 2 ? 1 : 3;
        api<DWORD>(IAT_DO_FRAME, bink);
        api<DWORD>(IAT_COPY, bink, dest, pitch, height, x, y, flags);
        api<DWORD>(IAT_NEXT_FRAME, bink);
    } else {
        return k;
    }
    f(bp(f(s, 8)) + off, 8) += 1;
    BYTE *r = bp(f(s, 8)) + off;
    if (f(r, 8) > f(bp(f(r, 0)), 8))
        f(r, 8) = 1;
    return pv(r);
}

DWORD *THISCALL BinkMovie_GetSummary(BYTE *s, DWORD id, DWORD *out)
{
    DWORD summary[0x1f];
    api<DWORD>(IAT_SUMMARY, f(bp(f(s, 8)) + id * 0x18, 0), summary);
    out[0] = summary[0];
    out[1] = summary[1];
    out[2] = summary[8];
    out[3] = summary[5];
    return out;
}

DWORD THISCALL BinkMovie_SetPause(BYTE *s, DWORD id, DWORD pause)
{
    return api<DWORD>(IAT_PAUSE, f(bp(f(s, 8)) + id * 0x18, 0), (pause & 0xff) ? 1u : 0u);
}

// ---- The file callbacks --------------------------------------------------------------------------------------------

DWORD STDCALL BinkIo_ReadHeader(BYTE *io, DWORD offset, void *dest, DWORD size)
{
    void *fb = f<void *>(io, 0x48);
    lock_inc(io + 0x5c);
    wait_background(io);
    if (offset != 0xffffffff && f(io, 0x4c) != offset) {
        filebuf_seek(fb, f(io, 0x70) + offset);
        f(io, 0x4c) = offset;
    }
    const DWORD n = filebuf_read(fb, dest, size);
    f(io, 0x4c) += n;
    f(io, 0x50) = f(io, 0x4c);
    cur_size(io);
    lock_dec(io + 0x5c);
    return n;
}

DWORD STDCALL BinkIo_ReadFrame(BYTE *io, DWORD, DWORD offset, BYTE *dest, DWORD size)
{
    bool locked = false;
    DWORD total = 0;
    const DWORD start = timer();
    void *fb = f<void *>(io, 0x48);
    if (offset != 0xffffffff && f(io, 0x50) != offset) {
        locked = true;
        lock_inc(io + 0x5c);
        wait_background(io);
        const DWORD cur = f(io, 0x50);
        if (offset > cur && offset <= f(io, 0x4c)) {   // forward inside the buffered data
            const DWORD d = offset - cur;
            f(io, 0x50) = offset;
            f(io, 0x58) += d;
            f(io, 0x44) -= d;
            f(io, 0x54) += d;
            if (f(io, 0x54) > f(io, 0x68))
                f(io, 0x54) -= f(io, 0x38);
        } else {
            filebuf_seek(fb, f(io, 0x70) + offset);
            f(io, 0x4c) = offset;
            f(io, 0x50) = offset;
            f(io, 0x58) = f(io, 0x38);
            f(io, 0x44) = 0;
            f(io, 0x54) = f(io, 0x64);
            f(io, 0x6c) = f(io, 0x64);
        }
    }
    for (;;) {
        if (f(io, 0x44)) {
            DWORD n = f(io, 0x44);
            if (n > size)
                n = size;
            size -= n;
            const BYTE *src = bp(f(io, 0x54));
            total += n;
            f(io, 0x50) += n;
            const DWORD room = f(io, 0x68) - pv(src);
            if (room <= n) {   // the buffer wraps
                memcpy(dest, src, room);
                dest += room;
                f(io, 0x54) = f(io, 0x64);
                n -= room;
                f(io, 0x44) -= room;
                f(io, 0x58) += room;
            }
            if (n) {
                memcpy(dest, bp(f(io, 0x54)), n);
                dest += n;
                f(io, 0x54) += n;
                f(io, 0x44) -= n;
                f(io, 0x58) += n;
            }
        }
        if (!size) {
            f(io, 0x2c) += timer() - start;
            break;
        }
        if (!locked) {   // take the lock and look at the buffer again
            locked = true;
            lock_inc(io + 0x5c);
            wait_background(io);
            continue;
        }
        const DWORD t0 = timer();
        const DWORD r = filebuf_read(fb, dest, size);
        f(io, 0x4c) += r;
        f(io, 0x50) += r;
        f(io, 0x20) += r;
        total += r;
        if (f(io, 0x78))
            rate_wait(io, r, t0);
        const DWORD t = timer();
        f(io, 0x28) += t - t0;
        f(io, 0x2c) += t - start;
        break;
    }
    cur_size(io);
    if (f(io, 0x44) + 0x1000 > f(io, 0x40))
        f(io, 0x40) = f(io, 0x44);
    if (locked)
        lock_dec(io + 0x5c);
    return total;
}

DWORD STDCALL BinkIo_GetBufferSize(BYTE *io, DWORD size)
{
    lock_inc(io + 0x5c);
    wait_background(io);
    lock_dec(io + 0x5c);
    return (size + 0xfff) & ~0xfffu;
}

DWORD STDCALL BinkIo_SetInfo(BYTE *io, DWORD buffer, DWORD size, DWORD file_size, DWORD rate)
{
    lock_inc(io + 0x5c);
    wait_background(io);
    size &= ~0xfffu;
    f(io, 0x64) = buffer;
    f(io, 0x54) = buffer;
    f(io, 0x6c) = buffer;
    f(io, 0x68) = buffer + size;
    f(io, 0x38) = size;
    f(io, 0x58) = size;
    f(io, 0x44) = 0;
    f(io, 0x74) = file_size;
    f(io, 0x78) = rate;
    lock_dec(io + 0x5c);
    return pv(io + 0x5c);
}

DWORD STDCALL BinkIo_Close(BYTE *io)
{
    lock_inc(io + 0x5c);
    wait_background(io);
    if (void *fb = f<void *>(io, 0x48))
        vcall<void *>(fb, 0, 1u);
    lock_dec(io + 0x5c);
    return pv(io + 0x5c);
}

DWORD STDCALL BinkIo_Idle(BYTE *io)
{
    bool busy = false;
    DWORD n = 0;
    const DWORD working = f(io, 0x24);
    void *fb = f<void *>(io, 0x48);
    lock_inc(io + 0x60);
    lock_inc(io + 0x5c);
    if (vread(io, 0x60) != 1 || vread(io, 0x5c) != 1) {
        busy = true;
        n = 0xffffffff;
    } else if (f(io, 0x58) < 0x1000 || f(io, 0x74) - f(io, 0x4c) < 0x1000) {
        f(io, 0x40) = f(io, 0x44);
    } else {
        const DWORD t0 = timer();
        f(io, 0x1c) = 1;
        n = filebuf_read(fb, bp(f(io, 0x6c)), 0x1000);
        f(io, 0x1c) = 0;
        f(io, 0x20) += n;
        f(io, 0x4c) += n;
        f(io, 0x6c) += n;
        if (f(io, 0x6c) >= f(io, 0x68))
            f(io, 0x6c) = f(io, 0x64);
        f(io, 0x58) -= n;
        f(io, 0x44) += n;
        if (f(io, 0x44) > f(io, 0x3c))
            f(io, 0x3c) = f(io, 0x44);
        if (f(io, 0x78))
            rate_wait(io, n, t0);
        const DWORD el = timer() - t0;
        f(io, 0x28) += el;
        if (!working && !f(io, 0x24))
            f(io, 0x30) += el;
        else
            f(io, 0x34) += el;
    }
    lock_dec(io + 0x5c);
    lock_dec(io + 0x60);
    if (busy)
        api<DWORD>(IAT_SLEEP, 1u);
    return n;
}

ZAX_HOOKS(bink_rest5a)
{
    const Hook list[] = {
        {0x431e50, (void *)&BinkSystem_Dtor, "BinkSystem_Dtor", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x89}},
        {0x431fd0, (void *)&BinkSystem_OpenMovie, "BinkSystem_OpenMovie", 8, {0x57, 0x8b, 0xf9, 0x68, 0xa0, 0x2a, 0x43, 0x00}},
        {0x432080, (void *)&BinkSystem_CloseMovie, "BinkSystem_CloseMovie", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x57, 0x83, 0xf8}},
        {0x4320d0, (void *)&BinkMovie_Wait, "BinkMovie_Wait", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0xff, 0x74}},
        {0x432110, (void *)&BinkMovie_DecodeToFrame, "BinkMovie_DecodeToFrame", 5, {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56}},
        {0x432320, (void *)&BinkMovie_GetSummary, "BinkMovie_GetSummary", 7, {0x83, 0xec, 0x7c, 0x8d, 0x44, 0x24, 0x00, 0x50}},
        {0x4323a0, (void *)&BinkMovie_SetPause, "BinkMovie_SetPause", 6, {0x8a, 0x54, 0x24, 0x08, 0x33, 0xc0, 0x84, 0xd2}},
        {0x4323d0, (void *)&BinkIo_ReadHeader, "BinkIo_ReadHeader", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x432480, (void *)&BinkIo_ReadFrame, "BinkIo_ReadFrame", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x53, 0x56}},
        {0x432770, (void *)&BinkIo_GetBufferSize, "BinkIo_GetBufferSize", 5, {0x55, 0x8b, 0xec, 0x51, 0x56, 0x8b, 0x75, 0x08}},
        {0x4327e0, (void *)&BinkIo_SetInfo, "BinkIo_SetInfo", 5, {0x55, 0x8b, 0xec, 0x51, 0x56, 0x8b, 0x75, 0x08}},
        {0x432870, (void *)&BinkIo_Close, "BinkIo_Close", 5, {0x55, 0x8b, 0xec, 0x51, 0x56, 0x8b, 0x75, 0x08}},
        {0x4328d0, (void *)&BinkIo_Idle, "BinkIo_Idle", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c, 0x53, 0x56}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
