// The streamed sounds 0x587060-0x5879f5 (phase 4, sound): the music and the voice-overs play through a
// 100,000-byte DirectSound ring buffer that a multimedia timer (timeSetEvent, every 50 ms) refills from a
// sound reader (sound_reader.cpp); a second reader is mixed in for a cross-fade between two songs. With
// "Use Threaded Stream" (Zax.CFG, default 1) the timer refills it in its own thread, otherwise the game's
// frame (0x587160) does. [Class name ours: CThreadedStream; assumption.]
//
// The holder (8 bytes; the music mixer's +0x30, the application's +0x274, the voice player 0x713c28):
// +0 the state (0x68 bytes, operator new, cleared; none without DirectSound 0x7136f4), +4 threaded.
// The state [confirmed: the functions below]: +0 the volume (hundredths of a decibel), +0xc muted (the
// volume at or below -3000), +0xd playing, +0xe the timer callback is running, +0x10 the timer, +0x14
// the reader, +0x18 the queued reader (cross-fade), +0x1c the queued one loops (is not seeked to the
// position), +0x1d the queued one is seeked, +0x20 the cross-fade's gain (float), +0x24 the PCM bytes,
// +0x28 the bytes still to read, +0x2c the DSBUFFERDESC (flags 0xe0, 100,000 bytes, +0x3c the reader's
// format), +0x50 the IDirectSoundBuffer, +0x54 the write position, +0x58 stop requested, +0x5c the
// reader's end written, +0x60 the last play position, +0x64 the bytes until the end has played.
// The silence the buffer is filled with: 0x80 for 8-bit, 0 for 16-bit PCM.
//
// The timer callback runs in the multimedia timer's thread; the game's own functions wait (spin) on +0xe
// as the exe does.
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }
inline volatile BYTE &flag(BYTE *s, DWORD off) { return *reinterpret_cast<volatile BYTE *>(s + off); }

constexpr DWORD RING = 100000;
constexpr DWORD DSOUND = 0x7136f4;
constexpr DWORD IMPORT_TIME_KILL = 0x5ea288, IMPORT_TIME_SET = 0x5ea290;

template <typename R = DWORD, typename... A> inline R com(void *o, int slot, A... a)
{
    auto fn = (*reinterpret_cast<R(__stdcall *const *const *)(void *, A...)>(o))[slot];
    return fn(o, a...);
}
enum { DS_CREATE_BUFFER = 3 };
enum { DSB_RELEASE = 2, DSB_GET_POSITION = 4, DSB_LOCK = 11, DSB_PLAY = 12, DSB_SET_POSITION = 13, DSB_SET_VOLUME = 15, DSB_STOP = 18,
       DSB_UNLOCK = 19 };

void time_kill(DWORD id) { (*imp_slot<DWORD(__stdcall **)(DWORD)>(IMPORT_TIME_KILL))(id); }
DWORD time_set(DWORD delay, DWORD resolution, void *fn, void *user, DWORD flags)
{
    return (*imp_slot<DWORD(__stdcall **)(DWORD, DWORD, void *, void *, DWORD)>(IMPORT_TIME_SET))(delay, resolution, fn, user, flags);
}
// The reader's format (0x5669d0: its +4)
BYTE *format_of(BYTE *reader) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x5669d0)(reader); }
BYTE *reader_for(const char *path) { return zax::fn<BYTE *(__fastcall *)(const char *, DWORD)>(0x57ca10)(path, 0x410); }
void delete_reader(BYTE *s, DWORD off)
{
    BYTE *r = bp(dw(s, off));
    dw(s, off) = 0;
    if (r)
        vcall<void *>(r, 1, 1);
}
BYTE silence(BYTE *s) { return fl<WORD>(bp(dw(s, 0x3c)), 0xe) == 8 ? 0x80 : 0; }

}  // namespace

// The timer callback (stdcall: timer, message, state, two unused)
void __stdcall Stream_TimerCallback(DWORD, DWORD, BYTE *s, DWORD, DWORD);

namespace {

// 0x587160: without a thread the callback runs here; a stop that was asked for ends the stream (after
// waiting for a running callback): its timer killed, both readers deleted
void THISCALL Stream_Frame(BYTE *h)
{
    if (!fl<BYTE>(h, 4))
        Stream_TimerCallback(0, 0, bp(dw(h, 0)), 0, 0);
    BYTE *s = bp(dw(h, 0));
    if (!s || !dw(s, 0x58) || !dw(s, 0x14))
        return;
    if (flag(s, 0xe))
        while (flag(s, 0xe)) {
        }
    if (!flag(s, 0xe))
        Stream_TimerCallback(0, 0, s, 0, 0);
    time_kill(dw(s, 0x10));
    dw(s, 0x10) = 0;
    delete_reader(s, 0x14);
    delete_reader(s, 0x18);
}

// A stop asked for and waited for (the frame until it no longer plays, unless muted)
void stop_and_wait(BYTE *h, BYTE *s)
{
    dw(s, 0x58) = 1;
    while (flag(s, 0xd) && !flag(s, 0xc))
        Stream_Frame(h);
}

// 0x587060 (unused): the state (when there is DirectSound), "Use Threaded Stream"
BYTE *THISCALL Stream_Ctor(BYTE *h, DWORD)
{
    const DWORD threaded = zax::fn<DWORD(__fastcall *)(void *, const char *, const char *, DWORD)>(0x4a4670)(
        glob<void *>(0x6bd330), glob<const char *>(0x60fc08 /* "Options" */), glob<const char *>(0x622db4 /* "Use Threaded Stream" */), 1);
    fl<BYTE>(h, 4) = threaded != 0;
    dw(h, 0) = 0;
    BYTE *s = static_cast<BYTE *>(heap_alloc(0x68));
    if (s) {
        memset(s, 0, 0x68);
        if (!*glob<DWORD *>(DSOUND)) {
            heap_free(s);
            return h;
        }
        dw(h, 0) = ptr_value(s);
    }
    return h;
}
// 0x5870d0: stopped, the timer killed, the buffer released, the readers deleted, the state freed
void THISCALL Stream_Dtor(BYTE *h)
{
    BYTE *s = bp(dw(h, 0));
    if (!s)
        return;
    if (flag(s, 0xd))
        stop_and_wait(h, s);
    if (dw(s, 0x10)) {
        time_kill(dw(s, 0x10));
        dw(s, 0x10) = 0;
    }
    if (BYTE *b = bp(dw(s, 0x50))) {
        com(b, DSB_RELEASE);
        dw(s, 0x50) = 0;
    }
    delete_reader(s, 0x14);
    delete_reader(s, 0x18);
    heap_free(s);
    dw(h, 0) = 0;
}
// 0x5871f0 (path, gain, loop): the next song queued for a cross-fade; only one with the playing one's
// format (rate, channels, bits, bytes per second) is kept
bool THISCALL Stream_Queue(BYTE *h, const char *path, DWORD gain, BYTE loop)
{
    BYTE *s = bp(dw(h, 0));
    if (!s || !path || !*path)
        return false;
    dw(s, 0x20) = gain;
    BYTE *r = reader_for(path);
    dw(s, 0x18) = ptr_value(r);
    if (!r)
        return false;
    fl<BYTE>(s, 0x1d) = 0;
    fl<BYTE>(s, 0x1c) = loop;
    BYTE *a = format_of(bp(dw(s, 0x14)));
    BYTE *b = format_of(bp(dw(s, 0x18)));
    if (dw(a, 8) == dw(b, 8) && fl<WORD>(a, 2) == fl<WORD>(b, 2) && fl<WORD>(a, 0xe) == fl<WORD>(b, 0xe) && dw(a, 4) == dw(b, 4))
        return true;
    delete_reader(s, 0x18);
    return false;
}
// 0x5872b0: the queued reader becomes the current one (after the callback), the old one deleted
bool THISCALL Stream_TakeQueued(BYTE *h)
{
    BYTE *s = bp(dw(h, 0));
    if (!s)
        return false;
    while (flag(s, 0xe))
        Stream_Frame(h);
    if (dw(s, 0x18)) {
        dw(s, 0x3c) = ptr_value(format_of(bp(dw(s, 0x18))));
        const DWORD old = dw(s, 0x14);
        dw(s, 0x14) = dw(s, 0x18);
        dw(s, 0x18) = 0;
        if (BYTE *r = bp(old))
            vcall<void *>(r, 1, 1);
    }
    return true;
}

// 0x587310 (path, bytes to skip): a stream of the file: the running one stopped, a reader, the ring
// buffer (100,000 bytes, flags 0xe0) created and filled from the start (the rest silence when the file
// is shorter), the timer started (threaded), the volume set and, unless muted, played looping
bool THISCALL Stream_Play(BYTE *h, const char *path, DWORD skip)
{
    BYTE *s = bp(dw(h, 0));
    if (!s || !*path)
        return false;
    if (flag(s, 0xd))
        stop_and_wait(h, s);
    dw(s, 0x58) = 0;
    if (!*glob<DWORD *>(DSOUND)) {
        dw(s, 0x58) = 1;
        return false;
    }
    delete_reader(s, 0x14);
    delete_reader(s, 0x18);
    zax::fn<void(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(0x622dc8 /* "Stream sound" */));
    dw(s, 0x14) = ptr_value(reader_for(path));
    zax::fn<void(__fastcall *)()>(0x4a2670)();
    if (!dw(s, 0x14)) {
        dw(s, 0x58) = 1;
        return false;
    }
    const DWORD total = zax::fn<DWORD(THISCALL *)(void *)>(0x483750)(bp(dw(s, 0x14)));
    dw(s, 0x24) = total;
    dw(s, 0x28) = total - skip;
    for (DWORD o = 0x2c; o < 0x50; o += 4)
        dw(s, o) = 0;
    fl<BYTE>(s, 0x5c) = 0;
    dw(s, 0x64) = 0;
    dw(s, 0x60) = 0;
    dw(s, 0x2c) = 0x24;
    dw(s, 0x30) = 0xe0;
    dw(s, 0x34) = RING;
    dw(s, 0x3c) = ptr_value(format_of(bp(dw(s, 0x14))));
    if (BYTE *b = bp(dw(s, 0x50))) {
        com(b, DSB_RELEASE);
        dw(s, 0x50) = 0;
    }
    if (com(*glob<BYTE **>(DSOUND), DS_CREATE_BUFFER, s + 0x2c, s + 0x50, 0)) {
        dw(s, 0x50) = 0;
        dw(s, 0x58) = 1;
        return false;
    }
    BYTE *first = nullptr, *second = nullptr;
    DWORD n1 = 0, n2 = 0;
    if (com(bp(dw(s, 0x50)), DSB_LOCK, 0, RING, &first, &n1, &second, &n2, 0)) {
        delete_reader(s, 0x14);
        dw(s, 0x58) = 1;
        return false;
    }
    DWORD written = n1;
    if (n1) {
        DWORD want = n1;
        if (want > dw(s, 0x28))
            want = dw(s, 0x28);
        if (skip)
            vcall<DWORD>(bp(dw(s, 0x14)), 4, skip);
        written = vcall<DWORD>(bp(dw(s, 0x14)), 2, first, want);
        dw(s, 0x28) -= written;
        if (!dw(s, 0x28) || !written) {
            fl<BYTE>(s, 0x5c) = 1;
            dw(s, 0x64) = written;
            if (written < n1) {
                memset(first + written, silence(s), n1 - written);
                written = n1;
            }
        }
    }
    com(bp(dw(s, 0x50)), DSB_UNLOCK, first, written, second, 0);
    dw(s, 0x54) = written;
    if (written >= RING)
        dw(s, 0x54) = written - RING;
    if (fl<BYTE>(h, 4) && !dw(s, 0x10)) {
        dw(s, 0x10) = time_set(50, 5, code<void *>(0x587700), s, 1);   // the exe's address (hooked to the callback below)
        if (!dw(s, 0x10))
            zax::fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    com(bp(dw(s, 0x50)), DSB_SET_VOLUME, dw(s, 0));
    if (!fl<BYTE>(s, 0xc)) {
        com(bp(dw(s, 0x50)), DSB_PLAY, 0, 0, 1);
        fl<BYTE>(s, 0xd) = 1;
    }
    return true;
}
// 0x587600: a stop asked for and waited for
void THISCALL Stream_Stop(BYTE *h)
{
    BYTE *s = bp(dw(h, 0));
    if (s && flag(s, 0xd))
        stop_and_wait(h, s);
}
// 0x587640 (volume): kept; at or below -3000 the stream is muted (its buffer stopped), above it unmuted
// (played again) and the buffer's volume set
void THISCALL Stream_SetVolume(BYTE *h, int v)
{
    BYTE *s = bp(dw(h, 0));
    if (!s || (int)dw(s, 0) == v)
        return;
    dw(s, 0) = (DWORD)v;
    BYTE *t = bp(dw(h, 0));
    if (v > -3000) {
        if (t && fl<BYTE>(t, 0xc)) {
            fl<BYTE>(t, 0xc) = 0;
            if (BYTE *b = bp(dw(t, 0x50)))
                com(b, DSB_PLAY, 0, 0, 1);
        }
        if (BYTE *b = bp(dw(s, 0x50)))
            com(b, DSB_SET_VOLUME, v);
    } else if (t && fl<BYTE>(t, 0xc) != 1) {
        fl<BYTE>(t, 0xc) = 1;
        if (BYTE *b = bp(dw(t, 0x50)))
            com(b, DSB_STOP);
    }
}

}  // namespace

// 0x587700 (timer, message, state, unused, unused): the refill. Nothing while it runs already, when the
// stream does not play or is muted (unless stopping) or has no buffer. The bytes until the end has
// played count down with the play position; once the end was written and has played, or on a stop,
// the buffer stops and rewinds. Otherwise, when at least 40,000 bytes are free behind the write
// position: locked; on a stop silence, else the reader's next bytes (ReadParts; the queued reader is
// mixed in with the gain, first seeked to the current position unless it loops), its end marked and
// the rest silence; unlocked, the write position moved on.
void __stdcall Stream_TimerCallback(DWORD, DWORD, BYTE *s, DWORD, DWORD)
{
    if (!s || flag(s, 0xe) || !flag(s, 0xd) || (flag(s, 0xc) && !dw(s, 0x58)) || !dw(s, 0x50))
        return;
    flag(s, 0xe) = 1;
    DWORD play, write;
    if (com(bp(dw(s, 0x50)), DSB_GET_POSITION, &play, &write) == 0) {
        if ((int)dw(s, 0x64) > 0) {
            int d = (int)(play - dw(s, 0x60));
            if (d < 0)
                d += RING;
            dw(s, 0x64) -= (DWORD)d;
        }
        dw(s, 0x60) = play;
        bool stop = dw(s, 0x58) != 0;
        if (fl<BYTE>(s, 0x5c) && !stop && (int)dw(s, 0x64) <= 0) {
            dw(s, 0x58) = 1;
            stop = true;
        }
        if (stop) {
            com(bp(dw(s, 0x50)), DSB_STOP);
            com(bp(dw(s, 0x50)), DSB_SET_POSITION, 0);
            dw(s, 0x54) = 0;
            fl<BYTE>(s, 0x5c) = 0;
            flag(s, 0xd) = 0;
            flag(s, 0xe) = 0;
            return;
        }
        if (play < write) {
            const DWORD at_ = dw(s, 0x54);
            const DWORD n = at_ <= play ? play - at_ : play - at_ + RING;
            BYTE *first, *second;
            DWORD n1, n2;
            if (n >= 40000 && com(bp(dw(s, 0x50)), DSB_LOCK, at_, n, &first, &n1, &second, &n2, 0) == 0) {
                DWORD part1, part2 = 0;
                if (dw(s, 0x58)) {
                    memset(first, silence(s), n1);
                    part1 = n1;
                    if (n2) {
                        memset(second, silence(s), n2);
                        part2 = n2;
                    }
                } else {
                    const DWORD tell = vcall<DWORD>(bp(dw(s, 0x14)), 3);
                    const DWORD got = zax::fn<DWORD(THISCALL *)(void *, BYTE *, DWORD, BYTE *, DWORD)>(0x57d330)(bp(dw(s, 0x14)), first, n1, second, n2);
                    if (BYTE *q = bp(dw(s, 0x18))) {
                        if (!fl<BYTE>(s, 0x1c) && !fl<BYTE>(s, 0x1d)) {
                            fl<BYTE>(s, 0x1d) = 1;
                            vcall<DWORD>(q, 4, tell);
                        }
                        zax::fn<DWORD(THISCALL *)(void *, BYTE *, DWORD, BYTE *, DWORD, DWORD)>(0x57d550)(bp(dw(s, 0x18)), first, n1, second, n2,
                                                                                                   dw(s, 0x20));
                    }
                    const DWORD left = dw(s, 0x28) - got;
                    dw(s, 0x28) = left;
                    if (got <= n1) {
                        part1 = got;
                    } else {
                        part2 = got - n1;
                        part1 = n1;
                    }
                    if (!left || !got) {
                        if (!fl<BYTE>(s, 0x5c)) {
                            fl<BYTE>(s, 0x5c) = 1;
                            dw(s, 0x64) = got - n + RING;
                        }
                        if (part1 < n1) {
                            memset(first + part1, silence(s), n1 - part1);
                            part1 = n1;
                        }
                        if (part2 < n2) {
                            memset(second + part2, silence(s), n2 - part2);
                            part2 = n2;
                        }
                    }
                }
                com(bp(dw(s, 0x50)), DSB_UNLOCK, first, part1, second, part2);
                DWORD w = dw(s, 0x54) + part2 + part1;
                dw(s, 0x54) = w;
                if (w >= RING)
                    dw(s, 0x54) = w - RING;
            }
        }
    }
    flag(s, 0xe) = 0;
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(sound_stream)
{
    const Hook list[] = {
#include "sound_stream_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
