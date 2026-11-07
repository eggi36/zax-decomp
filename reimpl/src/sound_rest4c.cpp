// The last small pieces of the game side of the sound in 0x560000-0x5cffff (docs/DECOMPILATION.md 9, "Phase 4
// remains, band C"): the Ogg reader's tell callback, the threaded stream holder's getters and its cross-fade
// gain (the holder: +0 the stream's state, sound_stream.cpp), the voice-over player's stop and "playing".
// Every call goes through its exe address.
#include "reimpl.h"

namespace zax::snd4c {

inline BYTE *state_of(BYTE **holder) { return *holder; }
// An al answer as the exe leaves it: the upper bytes of eax are those of the pointer it tested (in asm: clang
// folded the plain C form into a movzx, upper bytes 0)
inline DWORD low_byte(const void *p, BYTE b)
{
    DWORD r;
    __asm__("movb %2, %%al" : "=a"(r) : "0"(p), "q"(b));
    return r;
}

// 0x57d310 (ecx: the Ogg data source): the file's position (FileBuf_Tell 0x4a2180 on its +8)
DWORD THISCALL OggCallback_Tell(BYTE *source)
{
    return fn<DWORD(THISCALL *)(void *)>(0x4a2180)(*reinterpret_cast<void **>(source + 8));
}

// 0x587290 (gain): the cross-fade's gain (+0x20 of the state); false without a state
DWORD THISCALL Stream_SetCrossFadeGain(BYTE **holder, DWORD gain)
{
    BYTE *s = state_of(holder);
    if (!s)
        return 0;
    *reinterpret_cast<DWORD *>(s + 0x20) = gain;
    return low_byte(s, 1);
}

// 0x5875e0: the reader's position in seconds (CSoundReader::Seconds 0x57d0e0 on +0x14); 0 without a state or
// while the timer callback runs (+0xe)
long double THISCALL Stream_Seconds(BYTE **holder)
{
    BYTE *s = state_of(holder);
    if (s && !s[0xe])
        return fn<long double(THISCALL *)(void *)>(0x57d0e0)(*reinterpret_cast<void **>(s + 0x14));
    return *glob<float *>(0x5ebed0);
}

// 0x5876c0: the volume (+0; 0 without a state)
DWORD THISCALL Stream_Volume(BYTE **holder)
{
    BYTE *s = state_of(holder);
    return s ? *reinterpret_cast<DWORD *>(s) : 0;
}

// 0x5876d0: muted (+0xc)
DWORD THISCALL Stream_IsMuted(BYTE **holder)
{
    BYTE *s = state_of(holder);
    return s ? low_byte(s, s[0xc]) : 0;
}

// 0x5876e0: playing (+0xd)
DWORD THISCALL Stream_IsPlaying(BYTE **holder)
{
    BYTE *s = state_of(holder);
    return s ? low_byte(s, s[0xd]) : 0;
}

// 0x595af0 (ecx: the voice-over player, +0 its stream holder): the stream stopped (CThreadedStream::Stop 0x587600)
DWORD THISCALL Voice_Stop(BYTE **player)
{
    return fn<DWORD(THISCALL *)(void *)>(0x587600)(*player);
}

// 0x595bd0: the voice-over plays (0x5876e0 on its stream holder)
DWORD THISCALL Voice_IsPlaying(BYTE **player)
{
    return fn<DWORD(THISCALL *)(void *)>(0x5876e0)(*player);
}

}  // namespace zax::snd4c

namespace zax {

using namespace snd4c;

ZAX_HOOKS(sound_rest4c)
{
    const Hook list[] = {
#include "sound_rest4c_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
