// The sound manager and the sound resources 0x57bdc0-0x57c9fe and the sound options 0x57ee90 (phase 4,
// sound): the game side of the sound over DirectSound. The DirectSound calls themselves stay calls into
// DirectSound (phase 5); this is what the game decides around them: which buffer exists, its format,
// volume, pan and frequency, what plays, the priorities from the distance to the listener.
//
// The sound manager (0x7136b8, a function-local static of 0x57bdc0 with guard bit 0x7136b0; vtable
// 0x600b64, derived from the resource cache 0x600af8, resource_manager.cpp) is the resource cache of the
// game (g_ResourceCache 0x713688 = 0x7136b8 after 0x57bdc0): +4 the level (the master volume in
// hundredths of a decibel; below -3000 nothing loads), +0xc muted, +0x10 the primary buffer's volume as
// read, +0x30 stereo. Its slots: 0 Init, 1 Shutdown, 2 deleting destructor, 3 a new CSoundResource, 4 the
// primary buffer's volume read into +0x10, 5 stereo, 6 the master volume, 7 muted, 8 the cache's enable
// test with the volume, 9 the volume +0x10 to the primary buffer.
// DirectSound [interface names: assumption, the slots and arguments match]: the IDirectSound 0x7136f4
// (DirectSoundCreate through the import thunk 0x5caa88; cooperative level 2 with the main window
// 0x713e18) and its primary buffer 0x713694 (flags 0x81, played looping), 0x7136f8 set once it exists.
// The listener: position 0x7136a0-0x7136a8, the distance factor 0x7136ac, 0x713698 and 0x7136f0 (set
// and read only; their users are elsewhere).
//
// CSoundResource (0x9c, vtable 0x600b8c, CResource 0x60 bytes in front): +0x38/+0x3c the near and far
// distance (10, 100), +0x40/+0x44 and +0x48/+0x4c/+0x50 two remembered settings (-1 after an unload),
// +0x54-+0x5c the position, +0x60 the DSBUFFERDESC (0x24 bytes: flags 0xe8 | 4 stereo | 0x40 16-bit),
// +0x84 the IDirectSoundBuffer, +0x88 the WAVEFORMATEX (0x12 bytes). Resource flags (+0x14): 2 positioned
// (3D), 8 looping (CResource's "keep"), 0x20 muted. Slots: 0 Reset, 1 Assign (a duplicate of the other's
// buffer), 2 Insert (play), 3 positioned on/off, 4 SetPosition, 5 the distances, 6 and 7 the remembered
// settings, 8 volume, 9 frequency, 10 pan, 11 Stop, 12 Unload, 13 deleting destructor, 14 IsPlaying,
// 15 Load (a reader of the file, a buffer of its size, the PCM copied in).
#include "reimpl.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }

constexpr DWORD MANAGER = 0x7136b8, MANAGER_GUARD = 0x7136b0, VT_MANAGER = 0x600b64, VT_SOUND = 0x600b8c;
constexpr DWORD DSOUND = 0x7136f4, PRIMARY = 0x713694, PRIMARY_MADE = 0x7136f8, MAIN_WINDOW = 0x713e18;
constexpr DWORD LISTENER = 0x7136a0, DISTANCE_FACTOR = 0x7136ac, LISTENER_713698 = 0x713698, LISTENER_7136F0 = 0x7136f0;
constexpr DWORD BIG_SOUNDS = 0x713690;   // counts the sounds above 500,000 bytes
constexpr DWORD C_VOLUME_RANGE = 0x5f7468, C_PAN_RANGE = 0x600b20;
constexpr DWORD OFF = 0xfffff2fb;   // -3333: silent

// COM calls (stdcall, this first)
template <typename R = DWORD, typename... A> inline R com(void *o, int slot, A... a)
{
    auto fn = (*reinterpret_cast<R(__stdcall *const *const *)(void *, A...)>(o))[slot];
    return fn(o, a...);
}
BYTE *dsound() { return *glob<BYTE **>(DSOUND); }
BYTE *primary() { return *glob<BYTE **>(PRIMARY); }
enum { DS_CREATE_BUFFER = 3, DS_DUPLICATE = 5, DS_COOPERATIVE = 6 };
enum { DSB_RELEASE = 2, DSB_GET_VOLUME = 6, DSB_GET_STATUS = 9, DSB_LOCK = 11, DSB_PLAY = 12, DSB_SET_POSITION = 13,
       DSB_SET_FORMAT = 14, DSB_SET_VOLUME = 15, DSB_SET_PAN = 16, DSB_SET_FREQUENCY = 17, DSB_STOP = 18, DSB_UNLOCK = 19 };
enum { DSB_GET_FORMAT = 5 };

// The resource cache and resource base (resource_manager.cpp, resource_cache.cpp)
BYTE *cache() { return zax::fn<BYTE *(__cdecl *)()>(0x57aef0)(); }
bool cache_forget_all(BYTE *m) { return zax::fn<bool(THISCALL *)(void *)>(0x57b200)(m); }
bool cache_set_byte30(BYTE *m, DWORD b) { return zax::fn<bool(THISCALL *)(void *, DWORD)>(0x57b170)(m, b); }
bool cache_set_disabled(BYTE *m, DWORD b) { return zax::fn<bool(THISCALL *)(void *, DWORD)>(0x57b1c0)(m, b); }
bool cache_check_enabled(BYTE *m) { return zax::fn<bool(THISCALL *)(void *)>(0x57b1e0)(m); }
bool res_reset(BYTE *r) { return zax::fn<bool(THISCALL *)(void *)>(0x57b2f0)(r); }
bool res_assign(BYTE *r, BYTE *from) { return zax::fn<bool(THISCALL *)(void *, void *)>(0x57b370)(r, from); }
bool res_insert(BYTE *r, DWORD how) { return zax::fn<bool(THISCALL *)(void *, DWORD)>(0x57b950)(r, how); }
bool res_flag2(BYTE *r, DWORD on) { return zax::fn<bool(THISCALL *)(void *, DWORD)>(0x57bba0)(r, on); }
bool res_reload_false(BYTE *r, DWORD a) { return zax::fn<bool(THISCALL *)(void *, DWORD)>(0x57bb10)(r, a); }
bool res_set_load_cost(BYTE *r, DWORD a) { return zax::fn<bool(THISCALL *)(void *, DWORD)>(0x57bbf0)(r, a); }
bool res_unlink(BYTE *r) { return zax::fn<bool(THISCALL *)(void *)>(0x57bd00)(r); }
bool res_unload(BYTE *r) { return zax::fn<bool(THISCALL *)(void *)>(0x57bd30)(r); }
void res_set_priority(BYTE *r, int p) { zax::fn<DWORD(THISCALL *)(void *, int)>(0x57bc60)(r, p); }

// The table math (math_tables.cpp): atan2 (y, x), the square root, the cosine
long double table_atan2(DWORD y, DWORD x) { return zax::fn<long double(__stdcall *)(DWORD, DWORD)>(0x509100)(y, x); }
long double table_sqrt(float v) { return zax::fn<long double(__stdcall *)(float)>(0x508c50)(v); }
long double table_cos(DWORD a) { return zax::fn<long double(__stdcall *)(DWORD)>(0x508da0)(a); }

}  // namespace

// ---- The listener --------------------------------------------------------------------------------------------

namespace {

// 0x57c170 (position): kept when any component differs (compared as integers)
void __stdcall Listener_SetPosition(const DWORD *p)
{
    DWORD *l = glob<DWORD *>(LISTENER);
    if (l[0] == p[0] && l[1] == p[1] && l[2] == p[2])
        return;
    l[0] = p[0];
    l[1] = p[1];
    l[2] = p[2];
}
// 0x57c1c0 (out): x through the FPU, y and z as integers
void __stdcall Listener_GetPosition(float *out)
{
    const DWORD *l = glob<const DWORD *>(LISTENER);
    out[0] = fstp(fld(*glob<const float *>(LISTENER)));
    dw(out, 4) = l[1];
    dw(out, 8) = l[2];
}
// 0x57c1f0, 0x57c200, 0x57c210, 0x57c220, 0x57c230
void __stdcall Listener_SetDistanceFactor(DWORD f) { *glob<DWORD *>(DISTANCE_FACTOR) = f; }
long double __cdecl Listener_DistanceFactor() { return fld(*glob<const float *>(DISTANCE_FACTOR)); }
void __stdcall Listener_Set713698(DWORD f) { *glob<DWORD *>(LISTENER_713698) = f; }
long double __cdecl Listener_Get713698() { return fld(*glob<const float *>(LISTENER_713698)); }
void __stdcall Listener_Set7136f0(DWORD f) { *glob<DWORD *>(LISTENER_7136F0) = f; }

// ---- The primary format ----------------------------------------------------------------------------------------

// 0x57c7b0 (ecx buffer, edx rate, bits, stereo): the buffer's format read (0x12 bytes) and set as PCM
// with the bits, 1 or 2 channels and rate + 5 samples per second (as in the exe)
void __fastcall Sound_SetBufferFormat(BYTE *buffer, DWORD rate, DWORD bits, DWORD stereo)
{
    if (!buffer)
        return;
    BYTE f[0x14];
    DWORD written = 0;
    dw(f, 4) = 0;
    dw(f, 8) = 0;
    dw(f, 0xc) = 0;
    dw(f, 0) = 0;
    fl<WORD>(f, 0x10) = 0;
    com(primary(), DSB_GET_FORMAT, f, 0x12, &written);
    fl<WORD>(f, 0) = 1;
    fl<WORD>(f, 0x10) = 0;
    const WORD channels = (WORD)((stereo != 0) + 1);
    fl<WORD>(f, 2) = channels;
    fl<WORD>(f, 0xe) = (WORD)bits;
    const WORD align = (WORD)((bits >> 3) * channels);
    fl<WORD>(f, 0xc) = align;
    dw(f, 4) = rate + 5;
    dw(f, 8) = (DWORD)align * (rate + 5);
    com(buffer, DSB_SET_FORMAT, f);
}

// ---- The sound manager ------------------------------------------------------------------------------------------

// 0x57bdc0: the manager (constructed on first use) becomes the resource cache; eax as the exe leaves it
void __cdecl SoundManager_Activate()
{
    BYTE &guard = *glob<BYTE *>(MANAGER_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        zax::fn<BYTE *(THISCALL *)(void *)>(0x57af00)(glob<void *>(MANAGER));
        *glob<DWORD *>(MANAGER) = vt(VT_MANAGER);
        crt_atexit(code<void(__cdecl *)()>(0x57be10));
    }
    *glob<DWORD *>(0x713688) = glob_addr(MANAGER);
}
// 0x57be50 (slot 0): DirectSound, its cooperative level and the primary buffer (one retry), its format
// (22,050 Hz, 16 bits, stereo from +0x30), the level from its volume, played looping
bool THISCALL SoundManager_Init(BYTE *m)
{
    if (!cache_forget_all(m))
        return false;
    if (dsound())
        return true;
    if (import_thunk<DWORD(__stdcall *)(void *, BYTE **, void *)>(0x5caa88, 0x5ea018)(nullptr, glob<BYTE **>(DSOUND), nullptr))
        return false;
    if (com(dsound(), DS_COOPERATIVE, *glob<DWORD *>(MAIN_WINDOW), 2))
        return false;
    DWORD desc[9];
    for (DWORD &d : desc)
        d = 0;
    desc[0] = 0x24;
    desc[1] = 0x81;
    if ((int)com(dsound(), DS_CREATE_BUFFER, desc, glob<BYTE **>(PRIMARY), 0) < 0) {
        desc[1] = 0x81;
        if ((int)com(dsound(), DS_CREATE_BUFFER, desc, glob<BYTE **>(PRIMARY), 0) < 0)
            return false;
    }
    Sound_SetBufferFormat(primary(), 22050, 16, fl<BYTE>(m, 0x30));
    BYTE *c = cache();
    vcall<DWORD>(c, 4);
    dw(m, 4) = dw(c, 0x10);
    *glob<DWORD *>(PRIMARY_MADE) = 1;
    com(primary(), DSB_PLAY, 0, 0, 1);
    return true;
}
// 0x57bf60 (slot 1): the primary buffer and DirectSound released
DWORD THISCALL SoundManager_Shutdown(BYTE *)
{
    if (!dsound())
        return 0;
    if (primary())
        com(primary(), DSB_RELEASE);
    const DWORD r = com(dsound(), DSB_RELEASE);
    *glob<DWORD *>(PRIMARY) = 0;
    *glob<DWORD *>(DSOUND) = 0;
    return r;
}
// 0x57be20 (slot 2)
BYTE *THISCALL SoundManager_DeletingDtor(BYTE *m, DWORD flags)
{
    zax::fn<void(THISCALL *)(void *)>(0x57af50)(m);
    if (flags & 1)
        heap_free(m);
    return m;
}
// 0x57bfa0 (slot 3): a new CSoundResource (pool block), reset
BYTE *THISCALL SoundManager_NewResource(BYTE *)
{
    BYTE *r = static_cast<BYTE *>(pool_alloc(0x9c));
    if (r) {
        zax::fn<BYTE *(THISCALL *)(void *)>(0x57b230)(r);
        dw(r, 0) = vt(VT_SOUND);
        dw(r, 0x84) = 0;
    }
    vcall<DWORD>(r, 0);
    return r;
}
// 0x57bfe0 (slot 4): the primary buffer's volume into +0x10
bool THISCALL SoundManager_ReadVolume(BYTE *m)
{
    if (!dsound() || !primary())
        return false;
    com(primary(), DSB_GET_VOLUME, m + 0x10);
    return true;
}
// 0x57c010 (slot 5: stereo): the primary format again
bool THISCALL SoundManager_SetStereo(BYTE *m, DWORD stereo)
{
    if (!cache_set_byte30(m, stereo) || !primary())
        return false;
    Sound_SetBufferFormat(primary(), 22050, 16, fl<BYTE>(m, 0x30));
    return true;
}
// 0x57c050 (slot 6: level): the master volume; eax 1, or 0x80000000 when DirectSound failed
DWORD THISCALL SoundManager_SetLevel(BYTE *m, DWORD level)
{
    const DWORD ok = zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x57b180)(m, level);
    if (!(BYTE)ok)
        return ok & 0xffffff00;
    if (!dsound() || !primary())
        return 0;
    const DWORD r = com(primary(), DSB_SET_VOLUME, level) & 0x80000000u;
    return r == 0x80000000u ? r : 1;
}
// 0x57c090 (slot 7: muted): muting keeps the volume (read into +4 unless muted already) and sets -3333;
// unmuting sets +4 again
bool THISCALL SoundManager_SetMuted(BYTE *m, DWORD muted)
{
    const BYTE b = (BYTE)muted;
    if (!cache_set_disabled(m, muted) || !dsound() || !primary())
        return false;
    if (!fl<BYTE>(cache(), 0xc) && b)
        com(primary(), DSB_GET_VOLUME, m + 4);
    if (b)
        com(primary(), DSB_SET_VOLUME, OFF);
    else
        com(primary(), DSB_SET_VOLUME, dw(m, 4));
    return true;
}
// 0x57c110 (slot 8): the cache's enable test, then the volume +4
bool THISCALL SoundManager_CheckEnabled(BYTE *m)
{
    if (!cache_check_enabled(m))
        return false;
    if (dsound() && primary())
        com(primary(), DSB_SET_VOLUME, dw(m, 4));
    return true;
}
// 0x57c140 (slot 9): the volume +0x10
bool THISCALL SoundManager_ApplyVolume(BYTE *m)
{
    if (!dsound() || !primary())
        return false;
    com(primary(), DSB_SET_VOLUME, dw(m, 0x10));
    return true;
}

// ---- CSoundResource ---------------------------------------------------------------------------------------------

// 0x57c270 (slot 0): CResource's reset, the buffer description, position, distances and settings cleared
bool THISCALL Sound_Reset(BYTE *r)
{
    if (!res_reset(r))
        return false;
    for (DWORD o = 0x60; o < 0x84; o += 4)
        dw(r, o) = 0;
    dw(r, 0x54) = 0;
    dw(r, 0x58) = 0;
    dw(r, 0x5c) = 0;
    dw(r, 0x38) = 0x41200000;   // 10
    dw(r, 0x3c) = 0x42c80000;   // 100
    dw(r, 0x40) = 0;
    dw(r, 0x44) = 0;
    dw(r, 0x48) = 0;
    dw(r, 0x4c) = 0;
    dw(r, 0x50) = 0;
    return true;
}
// 0x57c2c0 (slot 1: other): CResource's assign, the description and format copied, the other's buffer
// duplicated
bool THISCALL Sound_Assign(BYTE *r, BYTE *from)
{
    if (!primary() || !res_assign(r, from))
        return false;
    memcpy(r + 0x60, from + 0x60, 0x24);
    memcpy(r + 0x88, from + 0x88, 0x12);
    return com(dsound(), DS_DUPLICATE, dw(from, 0x84), r + 0x84) == 0;
}
// 0x57c4f0: from the listener: beyond the far distance the priority -3333 (silent); else the volume
// from the distance between near and far as the priority (nothing below -3333) and the pan from the
// direction (the cosine of the angle times the distance, at most the far distance)
void THISCALL Sound_Update3D(BYTE *r)
{
    const float dx = fstp(fld(fl<float>(r, 0x54)) - fld(*glob<const float *>(LISTENER)));
    const float dy = fstp(fld(fl<float>(r, 0x58)) - fld(*at<const float *>(glob_addr(LISTENER) + 4)));
    const float dz = fstp(fld(fl<float>(r, 0x5c)) - fld(*at<const float *>(glob_addr(LISTENER) + 8)));
    const float angle = fstp(table_atan2(float_bits(dy), float_bits(dx)));
    const float dist = fstp(table_sqrt(fstp(fld(dz) * fld(dz) + fld(dy) * fld(dy) + fld(dx) * fld(dx))));
    const float far_ = fl<float>(r, 0x3c), near_ = fl<float>(r, 0x38);
    if (!c0(fld(dist), fld(far_))) {
        res_set_priority(r, (int)OFF);
        return;
    }
    int priority = 0;
    if (!c03(fld(dist), fld(near_))) {
        const float v = fstp((fld(dist) - fld(near_)) / (fld(far_) - fld(near_)) * cst(glob_addr(C_VOLUME_RANGE)));
        priority = x87_fistp(fld(v));
        if (priority <= (int)OFF)
            return;
    }
    res_set_priority(r, priority);
    long double c = table_cos(float_bits(angle)) * fld(dist);
    if (!c0(c, fld(far_)))
        c = fld(far_);
    const long double twice = fld(far_) + fld(far_);
    const float pan = fstp(c / twice * cst(glob_addr(C_PAN_RANGE)));
    vcall<DWORD>(r, 10, x87_fistp(fld(pan)));
}
// 0x57c620 (slot 2: how): CResource's insert; positioned sounds placed; the buffer from the start,
// looping (flag 8) or once; al 0 (as in the exe)
bool THISCALL Sound_Insert(BYTE *r, DWORD how)
{
    if (!primary() || !res_insert(r, how))
        return false;
    if ((dw(r, 0x14) >> 1) & 1)
        Sound_Update3D(r);
    BYTE *b = bp(dw(r, 0x84));
    if (!b)
        return false;
    const bool loop = (dw(r, 0x14) >> 3) & 1;
    com(b, DSB_SET_POSITION, 0);
    com(bp(dw(r, 0x84)), DSB_PLAY, 0, 0, loop ? 1 : 0);
    return false;
}
// 0x57c8d0 (slot 3: on): CResource's flag 2 (positioned)
bool THISCALL Sound_SetPositioned(BYTE *r, DWORD on) { return res_flag2(r, on); }
// 0x57c8f0 (slot 4: position, unused): positioned from now on, the position kept, placed
bool THISCALL Sound_SetPosition(BYTE *r, const DWORD *p, DWORD)
{
    if (!((dw(r, 0x14) >> 1) & 1))
        vcall<DWORD>(r, 3, 1);
    dw(r, 0x54) = p[0];
    dw(r, 0x58) = p[1];
    dw(r, 0x5c) = p[2];
    Sound_Update3D(r);
    return true;
}
// 0x57c930 (slot 5: near, far, unused)
bool THISCALL Sound_SetDistances(BYTE *r, DWORD near_, DWORD far_, DWORD)
{
    dw(r, 0x38) = near_;
    dw(r, 0x3c) = far_;
    return true;
}
// 0x57c860 (slot 6: a, b): kept unless both are the same (b compared on the FPU: NaN counts as the same)
void THISCALL Sound_SetSetting40(BYTE *r, DWORD a, DWORD b)
{
    if (dw(r, 0x40) == a && c3(fld(fl<float>(r, 0x44)), fld(raw_float(b))))
        return;
    dw(r, 0x40) = a;
    dw(r, 0x44) = b;
}
// 0x57c890 (slot 7: a, b, c): the same for three values
void THISCALL Sound_SetSetting48(BYTE *r, DWORD a, DWORD b, DWORD c)
{
    if (dw(r, 0x48) == a && c3(fld(fl<float>(r, 0x4c)), fld(raw_float(b))) && c3(fld(fl<float>(r, 0x50)), fld(raw_float(c))))
        return;
    dw(r, 0x48) = a;
    dw(r, 0x4c) = b;
    dw(r, 0x50) = c;
}
// 0x57c710 (slot 8: volume)
bool THISCALL Sound_SetVolume(BYTE *r, DWORD v)
{
    if (!dw(r, 0x84))
        return false;
    com(bp(dw(r, 0x84)), DSB_SET_VOLUME, v);
    return true;
}
// 0x57c730 (slot 9: frequency): through CResource's ReloadFalse, which always gives 0, so nothing more
// happens (as in the exe); the frequency would be kept at +0x18 for 100-100,000
bool THISCALL Sound_SetFrequency(BYTE *r, int f)
{
    if (!res_reload_false(r, (DWORD)f) || !dw(r, 0x84))
        return false;
    if (f >= 100 && f <= 100000) {
        com(bp(dw(r, 0x84)), DSB_SET_FREQUENCY, f);
        dw(r, 0x18) = (DWORD)f;
    }
    return true;
}
// 0x57c6d0 (slot 10: pan): through CResource's SetLoadCost (the pan is its +0x2c), then the buffer's
bool THISCALL Sound_SetPan(BYTE *r, DWORD pan)
{
    if (!res_set_load_cost(r, pan))
        return false;
    if (dw(r, 0x84))
        com(bp(dw(r, 0x84)), DSB_SET_PAN, pan);
    return true;
}
// 0x57c6a0 (slot 11): CResource's unlink, the buffer stopped
bool THISCALL Sound_Stop(BYTE *r)
{
    if (!res_unlink(r))
        return false;
    if (dw(r, 0x84))
        com(bp(dw(r, 0x84)), DSB_STOP);
    return true;
}
// 0x57c950 (slot 12): CResource's unload, the buffer released, the remembered settings -1
bool THISCALL Sound_Unload(BYTE *r)
{
    res_unload(r);
    if (dw(r, 0x84)) {
        com(bp(dw(r, 0x84)), DSB_RELEASE);
        dw(r, 0x84) = 0;
    }
    dw(r, 0x40) = 0xffffffff;
    dw(r, 0x48) = 0xffffffff;
    return true;
}
// 0x57c240 (slot 13)
BYTE *THISCALL Sound_DeletingDtor(BYTE *r, DWORD flags)
{
    dw(r, 0) = vt(VT_SOUND);
    zax::fn<void(THISCALL *)(void *)>(0x57b280)(r);
    if (flags & 1)
        pool_free(r, 0x9c);
    return r;
}
// 0x57c780 (slot 14): the buffer's status has "playing"
bool THISCALL Sound_IsPlaying(BYTE *r)
{
    DWORD status = ptr_value(r);   // the exe's local holds ecx (pushed) until GetStatus writes it
    if (!dw(r, 0x84) || com(bp(dw(r, 0x84)), DSB_GET_STATUS, &status))
        return false;
    return status & 1;
}
// 0x57c340 (slot 15: name, open flags): a static buffer of the file's PCM: the reader (0x57ca10), the
// description (0x24, flags 0xe8, stereo 4, 16-bit 0x40, its size), CreateSoundBuffer (one retry), the PCM
// read into the locked buffer, frequency, volume 0 and pan 0; the rate kept at +0x18 and +0x28
bool THISCALL Sound_Load(BYTE *r, const char *name, DWORD flags)
{
    if (!primary() || !dsound())
        return false;
    if (BYTE *b = bp(dw(r, 0x84))) {
        com(b, DSB_RELEASE);
        dw(r, 0x84) = 0;
    }
    dw(r, 0x6c) = 0;
    dw(r, 0x64) = 0xe8;
    dw(r, 0x60) = 0x24;
    BYTE *reader = zax::fn<BYTE *(__fastcall *)(const char *, DWORD)>(0x57ca10)(name, flags);
    if (!reader)
        return false;
    DWORD size = zax::fn<DWORD(THISCALL *)(void *)>(0x483750)(reader);
    if (size > 500000)
        ++*glob<DWORD *>(BIG_SOUNDS);
    dw(r, 0x68) = size;
    dw(r, 8) = size;
    zax::fn<void(THISCALL *)(void *, void *)>(0x57d2a0)(reader, r + 0x88);
    if (fl<WORD>(r, 0x8a) > 1)
        dw(r, 0x14) |= 4;
    const DWORD bits = fl<WORD>(r, 0x96);
    if (bits == 8)
        dw(r, 0x14) &= ~0x40u;
    else if (bits == 16)
        dw(r, 0x14) |= 0x40;
    dw(r, 0x70) = ptr_value(r + 0x88);
    if ((int)com(dsound(), DS_CREATE_BUFFER, r + 0x60, r + 0x84, 0) < 0) {
        dw(r, 0x64) = 0xe8;
        if ((int)com(dsound(), DS_CREATE_BUFFER, r + 0x60, r + 0x84, 0) < 0)
            return false;
    }
    if (!dw(r, 0x84))
        return true;
    // Lock's outputs go into the argument slots, which hold the name and the size until it writes them
    void *data = const_cast<char *>(name);
    DWORD bytes = size;
    if (com(bp(dw(r, 0x84)), DSB_LOCK, 0, size, &data, &bytes, nullptr, nullptr, 0)) {
        com(bp(dw(r, 0x84)), DSB_RELEASE);
        return false;
    }
    vcall<DWORD>(reader, 2, data, bytes);
    vcall<void *>(reader, 1, 1);
    if (com(bp(dw(r, 0x84)), DSB_UNLOCK, data, bytes, nullptr, 0))
        return false;
    if (BYTE *b = bp(dw(r, 0x84))) {
        com(b, DSB_SET_FREQUENCY, dw(bp(dw(r, 0x70)), 4));
        com(b, DSB_SET_VOLUME, 0);
        com(b, DSB_SET_PAN, 0);
    }
    const DWORD rate = dw(bp(dw(r, 0x70)), 4);
    dw(r, 0x18) = rate;
    dw(r, 0x28) = rate;
    return true;
}

// ---- The sound options ------------------------------------------------------------------------------------------

DWORD ini_int(DWORD key, DWORD def)
{
    return zax::fn<DWORD(__fastcall *)(void *, void *, const char *, DWORD)>(0x4a4670)(glob<void *>(0x6bd330), glob<void *>(0x6bd430), at<const char *>(key),
                                                                               def);
}
// 0x57ee90: Zax.CFG's sound options into the manager and the reporter: "Master Mute" (-3333) or "Master
// Voume" (sic) as the level (slot 6), "SFx Mute" (0.25) or "SFx Voume" (sic, 1.0) as the cache's scale,
// "Sounds Mixing Chanels" (32) as the limit, "Stereo Sounds" (1, slot 5), "Music Mute" (-3333) or "Music
// Volume" (-800) into the reporter's +0x64
void __cdecl Sound_LoadOptions()
{
    const DWORD level = ini_int(glob_addr(0x622420) /* "Master Mute" */, 0) ? OFF : ini_int(glob_addr(0x622410) /* "Master Voume" */, 0);
    vcall<DWORD>(cache(), 6, level);
    float scale;
    if (ini_int(glob_addr(0x622404) /* "SFx Mute" */, 0)) {
        dw(&scale, 0) = 0x3e800000;   // 0.25
    } else {
        scale = fstp(zax::fn<long double(__fastcall *)(void *, void *, const char *, float)>(0x4a46e0)(
            glob<void *>(0x6bd330), glob<void *>(0x6bd430), glob<const char *>(0x6223f8 /* "SFx Voume" */), 1.0f));
    }
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x57b1d0)(cache(), float_bits(scale));
    const DWORD channels = ini_int(glob_addr(0x6223e0) /* "Sounds Mixing Chanels" */, 0x20);
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x57b1b0)(cache(), channels);
    BYTE *c = cache();
    const DWORD stereo = ini_int(glob_addr(0x6223d0) /* "Stereo Sounds" */, 1) != 0;
    vcall<DWORD>(c, 5, stereo);
    if (ini_int(glob_addr(0x6223c4) /* "Music Mute" */, 0)) {
        dw(*glob<void **>(0x6c02cc), 0x64) = OFF;
        return;
    }
    const DWORD music = ini_int(glob_addr(0x6223b4) /* "Music Volume" */, 0xfffffce0 /* -800 */);
    dw(*glob<void **>(0x6c02cc), 0x64) = music;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(sound_resource)
{
    const Hook list[] = {
#include "sound_resource_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
