// The music and voice-over side of the sound 0x4ce3e0-0x4ce633, 0x4e1fb0-0x4e2238, 0x51d4f0-0x51e670 and
// 0x595ac0-0x595c68 (phase 4, sound): CMusicMixerData and CMusicMixer (the level's music: three songs,
// cross-faded on a switch), the application's menu music, the voice-over player, the music stream with
// its folder and the sound file list. The streams themselves (0x587060-0x587700, CThreadedStream) are
// sound_stream.cpp; called here through the exe.
//
// Layouts [confirmed: Create, RegisterFields, docs/fields.csv]:
//   CMusicMixerData (0x18, class 0x6d004c, vtable 0x5fb224, no parent): +8 "Num Seconds To Switch" (5),
//     +0xc "Full Mix Song", +0x10 "Med Mix Song", +0x14 "Ambiant Song" (indices into the music folder
//     list 0x6c0f10; resource descriptors with the vtable 0x5fb280).
//   CMusicMixer (0x34, class 0x6d0058, version 2, vtable 0x5fb420, no parent): +8 the seconds since the
//     last switch, +0xc "Mixer Data" (embedded CMusicMixerData, so +0x14 the fade time and +0x18/+0x1c/
//     +0x20 the songs), +0x24 "Cur Song Index" (2), +0x28 "Qued Song Index" (2), +0x2c the songs changed,
//     +0x30 the stream (8 bytes, 0x587060).
//   The music stream object (8 bytes, 0x4e1fe0 on a 0x587060 stream): +0 the stream state, +4 "Use
//     Threaded Stream", +8 a function giving the next song's name (none in the exe's callers).
//   The voice-over player (0x713c28, function-local static of 0x595b50, guard bit 0x713c34): +0 the
//     stream, +4 the priority of the playing voice, +8 the other sounds lowered (ResourceCache 0x57b0d0).
//   The music volume is the reporter's +0x64 (0x6c02cc, written from Zax.CFG by 0x57ee90): -3333 is off.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD CLS_DATA = 0x6d004c, VT_DATA = 0x5fb224, CLS_MIXER = 0x6d0058, VT_MIXER = 0x5fb420, VT_SONG_FIELD = 0x5fb280;
constexpr DWORD MUSIC_FILES = 0x6c0f10;   // the music folder's file list (CDirList, vtable 0x5f8848)
constexpr DWORD RESOURCE_FIELD_CTOR = 0x54e220;
constexpr DWORD VOICE = 0x713c28, VOICE_GUARD = 0x713c34;
constexpr DWORD C_VOLUME_SCALE = 0x5f7468;   // the fade's volume range (float)

DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }

// The stream (sound_stream.cpp): ecx = the 8-byte stream object
BYTE *stream_ctor(void *s) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x587060)(s, 1); }
void stream_dtor(void *s) { fn<void(THISCALL *)(void *)>(0x5870d0)(s); }
void stream_frame(void *s) { fn<void(THISCALL *)(void *)>(0x587160)(s); }
bool stream_queue(void *s, const char *path, DWORD gain, DWORD loop) { return fn<bool(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5871f0)(s, path, gain, loop); }
void stream_gain(void *s, float gain) { fn<DWORD(THISCALL *)(void *, float)>(0x587290)(s, gain); }
void stream_take_queued(void *s) { fn<DWORD(THISCALL *)(void *)>(0x5872b0)(s); }
bool stream_play(void *s, const char *path, DWORD skip) { return fn<bool(THISCALL *)(void *, const char *, DWORD)>(0x587310)(s, path, skip); }
long double stream_seconds(void *s) { return fn<long double(THISCALL *)(void *)>(0x5875e0)(s); }
void stream_stop(void *s) { fn<void(THISCALL *)(void *)>(0x587600)(s); }
void stream_set_volume(void *s, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x587640)(s, v); }
DWORD stream_volume(void *s) { return fn<DWORD(THISCALL *)(void *)>(0x5876c0)(s); }
bool stream_muted(void *s) { return fn<bool(THISCALL *)(void *)>(0x5876d0)(s); }
bool stream_playing(void *s) { return fn<bool(THISCALL *)(void *)>(0x5876e0)(s); }

// The music folder list (core: dir_list.cpp)
const char *music_name(DWORD i) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x4a3310)(glob<void *>(MUSIC_FILES), i); }
const char *music_path(const char *name) { return fn<const char *(THISCALL *)(void *, const char *)>(0x4a33b0)(glob<void *>(MUSIC_FILES), name); }
const char *music_current() { return fn<const char *(THISCALL *)(void *)>(0x4a31c0)(glob<void *>(MUSIC_FILES)); }
const char *music_next_path(const char *name) { return fn<const char *(THISCALL *)(void *, const char *)>(0x4a32b0)(glob<void *>(MUSIC_FILES), name); }
DWORD music_read(const char *folder) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD)>(0x4a2ea0)(glob<void *>(MUSIC_FILES), folder, 1, 0, 0); }
DWORD music_volume() { return dw(*glob<void **>(G_REPORTER), 0x64); }

void *resource_cache() { return fn<void *(__cdecl *)()>(0x57aef0)(); }
void lower_other_sounds(DWORD on) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x57b0d0)(resource_cache(), on); }

// ---- Constructors and destructors as Create, CreateArray and slot 1 inline them --------------------------

void data_level(BYTE *d)
{
    obj_ctor(d);
    vtable_and_defaults(d, vt(VT_DATA));
    dw(d, 8) = 0x40a00000;   // 5 seconds
    dw(d, 0xc) = 0;
    dw(d, 0x10) = 0;
    dw(d, 0x14) = 0;
}
void data_destroy(BYTE *d)
{
    vtable_and_free(d, vt(VT_DATA));
    obj_dtor(d);
}
// CObject, the embedded data, the mixer's level and its stream (operator new; null when it gives none)
void mixer_construct(BYTE *m)
{
    obj_ctor(m);
    data_level(m + 0xc);
    vtable_and_defaults(m, vt(VT_MIXER));
    dw(m, 8) = 0;
    dw(m, 0x24) = 2;
    dw(m, 0x28) = 2;
    fl<BYTE>(m, 0x2c) = 0;
    void *s = heap_alloc(8);
    dw(m, 0x30) = s ? ptr_value(stream_ctor(s)) : 0;
}
// The stream released (0x5870d0 and operator delete), the mixer's and the data's levels; the data's
// CObject level after the data's own, then the mixer's
void mixer_destroy(BYTE *m)
{
    dw(m, 0) = vt(VT_MIXER);
    if (BYTE *s = bp(dw(m, 0x30))) {
        stream_dtor(s);
        heap_free(s);
        dw(m, 0x30) = 0;
    }
    obj_free_fields(m);
    vtable_and_free(m + 0xc, vt(VT_DATA));
    obj_dtor(m + 0xc);
    obj_dtor(m);
}

// The song of an index (0 full mix, 1 medium, 2 ambient; else 0)
DWORD song_of(BYTE *m, DWORD index)
{
    switch (index) {
    case 0: return dw(m, 0x18);
    case 1: return dw(m, 0x1c);
    case 2: return dw(m, 0x20);
    default: return 0;
    }
}

}  // namespace

// ---- CMusicMixerData -----------------------------------------------------------------------------------------

namespace {

// 0x51d4f0 (slot 2; the static registration 0x51d690 uses it too)
CClassInfo *__cdecl MusicMixerData_ClassInfo()
{
    return class_getter(CLS_DATA, 0x61c918 /* "CMusicMixerData" */, 1.0f, nullptr, 0x18, 0, 0x51d540, 0x51d590, 0x51d6a0);
}
// 0x51d540, 0x51d590, 0x51d610 (slot 1)
void *__cdecl MusicMixerData_Create() { return create_one<0x18, data_level>(); }
void *__fastcall MusicMixerData_CreateArray(int count, int, int) { return create_array<0x18, data_level>(count); }
void *THISCALL MusicMixerData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, data_destroy>(self, flags); }

// 0x51d6a0: the fade time and the three songs
DWORD __cdecl MusicMixerData_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61c958) /* "Num Seconds To Switch" */, 8, 4, 8, 1, 0x40a00000, 0, 0, 0, 0);
    add_field(MusicMixerData_ClassInfo(), f);
    f = new_field_vt(0x34, VT_SONG_FIELD, RESOURCE_FIELD_CTOR, glob_addr(0x61c948) /* "Full Mix Song" */, 0xc, 4, 0xc, 1, 0, 0, 0);
    add_field(MusicMixerData_ClassInfo(), f);
    f = new_field_vt(0x34, VT_SONG_FIELD, RESOURCE_FIELD_CTOR, glob_addr(0x61c938) /* "Med Mix Song" */, 0x10, 4, 0x10, 1, 0, 0, 0);
    add_field(MusicMixerData_ClassInfo(), f);
    f = new_field_vt(0x34, VT_SONG_FIELD, RESOURCE_FIELD_CTOR, glob_addr(0x61c928) /* "Ambiant Song" */, 0x14, 4, 0x14, 1, 0, 0, 0);
    return add_field(MusicMixerData_ClassInfo(), f);
}

// ---- CMusicMixer ---------------------------------------------------------------------------------------------

// 0x51df00 (slot 2; the static registration 0x51e1d0 uses it too)
CClassInfo *__cdecl MusicMixer_ClassInfo()
{
    return class_getter(CLS_MIXER, 0x61c9ac /* "CMusicMixer" */, 2.0f, nullptr, 0x34, 0, 0x51df50, 0x51dfe0, 0x51e1e0);
}
// 0x51df50, 0x51dfe0, 0x51e0c0 (slot 1)
void *__cdecl MusicMixer_Create() { return create_one<0x34, mixer_construct>(); }
void *__fastcall MusicMixer_CreateArray(int count, int, int) { return create_array<0x34, mixer_construct>(count); }
void *THISCALL MusicMixer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, mixer_destroy>(self, flags); }
// 0x51e360: the constructor (out of line; the world's mixer at +0x334)
BYTE *THISCALL MusicMixer_Ctor(BYTE *self)
{
    mixer_construct(self);
    return self;
}

// 0x51e1e0
DWORD __cdecl MusicMixer_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61c980) /* "Mixer Data" */, 0xc, 0x18, 0xc, 1);
    add_field(MusicMixer_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61c9d0) /* "Debug/Cur Song Index" */, 0x24, 4, 0x24, 3, 0, 0, 0, 0);
    add_field(MusicMixer_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61c9b8) /* "Debug/Qued Song Index" */, 0x28, 4, 0x28, 3, 0, 0, 0, 0);
    return add_field(MusicMixer_ClassInfo(), f);
}

// 0x51e3e0 (index, song): a song of the mix; a changed one marks the mixer changed (eax: the low byte the
// or leaves)
DWORD THISCALL MusicMixer_SetSong(BYTE *self, DWORD index, DWORD song)
{
    DWORD off;
    switch (index) {
    case 0: off = 0x18; break;
    case 1: off = 0x1c; break;
    case 2: off = 0x20; break;
    default: return index - 2;
    }
    if (dw(self, off) == song) {
        fl<BYTE>(self, 0x2c) |= 0;
        return song & 0xffffff00;
    }
    dw(self, off) = song;
    fl<BYTE>(self, 0x2c) |= 1;
    return (song & 0xffffff00) | 1;
}

// 0x51e520 (track): the track the mixer plays. During a fade it is only queued. Otherwise: the volume
// from the reporter; nothing while the stream is muted; a stopped stream starts the song; a playing one
// cross-fades into it (0x5871f0, gain 1, looping when the songs changed) unless it is the same song
// unchanged, and starts it outright when the stream cannot queue it.
void THISCALL MusicMixer_SwitchTrack(BYTE *self, DWORD track)
{
    if (c0(fld(fl<float>(self, 8)), fld(fl<float>(self, 0x14)))) {
        dw(self, 0x28) = track;
        return;
    }
    const DWORD song = song_of(self, track);
    BYTE *s = bp(dw(self, 0x30));
    stream_set_volume(s, music_volume());
    if (!stream_muted(bp(dw(self, 0x30)))) {
        if (!stream_playing(bp(dw(self, 0x30)))) {
            stream_play(bp(dw(self, 0x30)), music_path(music_name(song)), 0);
            dw(self, 8) = dw(self, 0x14);
        } else if (song_of(self, dw(self, 0x24)) != song || fl<BYTE>(self, 0x2c)) {
            const DWORD loop = fl<BYTE>(self, 0x2c);
            if (stream_queue(bp(dw(self, 0x30)), music_path(music_name(song)), 0x3f800000, loop)) {
                dw(self, 0x24) = track;
                dw(self, 0x28) = track;
                fl<BYTE>(self, 0x2c) = 0;
                dw(self, 8) = 0;
                return;
            }
            stream_stop(bp(dw(self, 0x30)));
            stream_play(bp(dw(self, 0x30)), music_path(music_name(song)), 0);
            const DWORD fade = dw(self, 0x14);
            dw(self, 0x24) = track;
            dw(self, 0x28) = track;
            dw(self, 8) = fade;
            return;
        }
    }
    dw(self, 0x24) = track;
    dw(self, 0x28) = track;
}
// 0x51e450: the queued song becomes the current one and the stream stops
void THISCALL MusicMixer_Stop(BYTE *self)
{
    stream_take_queued(bp(dw(self, 0x30)));
    stream_stop(bp(dw(self, 0x30)));
}
// 0x51e470 (seconds): the fade: the old song's gain falls from 1 to 0 over the fade time, then the queued
// song is current; after the fade a queued track switches; a stream that stopped starts the current track
// again; the volume follows the reporter's
void THISCALL MusicMixer_Update(BYTE *self, float seconds)
{
    const long double old = fld(fl<float>(self, 8));
    fl<float>(self, 8) = fstp(fld(seconds) + fld(fl<float>(self, 8)));
    if (c0(old, fld(fl<float>(self, 0x14)))) {
        const long double ratio = old / fld(fl<float>(self, 0x14));
        stream_gain(bp(dw(self, 0x30)), fstp(cst(glob_addr(C_ONE)) - ratio));
        if (!c0(fld(fl<float>(self, 8)), fld(fl<float>(self, 0x14))))
            stream_take_queued(bp(dw(self, 0x30)));
    } else if (dw(self, 0x28) != dw(self, 0x24)) {
        MusicMixer_SwitchTrack(self, dw(self, 0x28));
    }
    if (!stream_playing(bp(dw(self, 0x30)))) {
        dw(self, 8) = dw(self, 0x14);
        MusicMixer_SwitchTrack(self, dw(self, 0x24));
    }
    const DWORD v = music_volume();
    if (v != stream_volume(bp(dw(self, 0x30))))
        stream_set_volume(bp(dw(self, 0x30)), music_volume());
}

// ---- The application's music (the menus; CZaxApp +0x274 the stream object, +0x64 the volume) ---------------

// 0x4ce510: the stream's frame; the volume (+0x64) with the fade (+0x270 of +0x26c seconds, scaled); while
// the music is on (+0x267) and audible the next song opens (the one named at +0x68 once, else the
// folder's next), a stream that cannot play turns the volume off (-3333); otherwise a playing stream
// stops. Then the window messages (0x5998f0).
void THISCALL App_UpdateMusic(BYTE *app)
{
    stream_frame(bp(dw(app, 0x274)));
    int volume = (int)dw(app, 0x64);
    if (!c3(fld(fl<float>(app, 0x26c)), cst(glob_addr(C_ZERO)))) {
        const float f = fstp(fld(fl<float>(app, 0x270)) / fld(fl<float>(app, 0x26c)) * cst(glob_addr(C_VOLUME_SCALE)));
        volume += x87_fistp(fld(f));
    }
    if (fl<BYTE>(app, 0x267) && volume > -3333) {
        BYTE *s = bp(dw(app, 0x274));
        char *name = reinterpret_cast<char *>(app + 0x68);
        if (!stream_playing(s)) {
            stream_set_volume(bp(dw(app, 0x274)), (DWORD)volume);
            if (*name) {
                fn<bool(THISCALL *)(void *, const char *)>(0x4e2150)(bp(dw(app, 0x274)), name);
                *name = 0;
            } else {
                fn<bool(THISCALL *)(void *, const char *)>(0x4e2150)(bp(dw(app, 0x274)), nullptr);
            }
            if (!stream_playing(bp(dw(app, 0x274))))
                dw(app, 0x64) = 0xfffff2fb;
        } else if (*name) {
            fn<bool(THISCALL *)(void *, const char *)>(0x4e2150)(bp(dw(app, 0x274)), name);
            *name = 0;
        }
        if (stream_volume(bp(dw(app, 0x274))) != (DWORD)volume)
            stream_set_volume(bp(dw(app, 0x274)), (DWORD)volume);
    } else if (stream_playing(bp(dw(app, 0x274)))) {
        stream_stop(bp(dw(app, 0x274)));
    }
    fn<void(__cdecl *)()>(0x5998f0)();
}
// 0x4ce3e0: the stream's volume from +0x64, then the music's frame
void THISCALL App_SetMusicVolume(BYTE *app)
{
    stream_set_volume(bp(dw(app, 0x274)), dw(app, 0x64));
    App_UpdateMusic(app);
}

// ---- The music stream object and the sound file list --------------------------------------------------------

// 0x4e1fe0 (folder, a): the stream (0x587060 with a); the folder's files read into the list 0x6c0f10
BYTE *THISCALL SoundStream_Ctor(BYTE *self, const char *folder, DWORD a)
{
    fn<BYTE *(THISCALL *)(void *, DWORD)>(0x587060)(self, a);
    if (folder)
        music_read(folder);
    dw(self, 8) = 0;
    return self;
}
// 0x4e2150 (name or null): plays a song of the list: the one playing already stays; otherwise the stream
// stops and, unless it still plays, the named song (or the next of the list, or the callback's) starts;
// false when the list is empty
bool THISCALL SoundStream_Open(BYTE *self, const char *name)
{
    if (name) {
        if (stream_playing(self)) {
            ZString cur;
            fn<ZString *(THISCALL *)(void *)>(0x4dec90)(&cur);
            const DWORD i = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x4e2030)(glob<void *>(MUSIC_FILES), name, -1);
            zs_assign_text(&cur, music_name(i));
            const char *text = zs_c_str(&cur);
            const bool same = !crt_stricmp(music_current(), text);
            zs_dtor(reinterpret_cast<BYTE *>(&cur));
            if (same)
                return true;
        }
        stream_stop(self);
    }
    if (stream_playing(self))
        return true;
    if (!*glob<DWORD *>(0x6c101c))
        return false;
    if (!name && dw(self, 8))
        name = at<const char *(__cdecl *)()>(dw(self, 8))();
    return stream_play(self, music_next_path(name), 0);
}

// 0x4e1fb0 (slot 0 of the list, 0x12c bytes): CDirList's destructor, the pool block with bit 0
void *THISCALL SoundFileList_DeletingDtor(BYTE *self, DWORD flags)
{
    fn<void(THISCALL *)(void *)>(0x4a2db0)(self);
    if (flags & 1)
        pool_free(self, 0x12c);
    return self;
}
// The name with the other extension (.wav -> .ogg; the second branch, meant for .ogg -> .wav, tests
// ".wav" again and is never taken, as in the exe), looked up again
DWORD find_other_extension(void *list, const char *name, DWORD hint, GlobKey ext)
{
    ZString z;
    fn<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&z, name);
    const DWORD n = fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(&z);
    char *buf = fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(&z, n + 4);
    fn<void(__fastcall *)(char *, const char *)>(0x4a3fb0)(buf, glob<const char *>(ext));
    fn<void(THISCALL *)(void *, DWORD)>(0x4e16d0)(&z, hint);
    const DWORD r = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591bd0)(list, zs_c_str(&z), hint);
    zs_dtor(reinterpret_cast<BYTE *>(&z));
    return r;
}
// 0x4e2030 (slot 2: name, hint): the list's Find (0x591bd0); a .wav that is missing is looked up as .ogg
DWORD THISCALL SoundFileList_Find(BYTE *self, const char *name, DWORD hint)
{
    const DWORD r = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591bd0)(self, name, hint);
    if (r != 0xffffffff)
        return r;
    const char *ext = fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(name);
    if (!crt_stricmp(ext, glob<const char *>(0x615658 /* ".wav" */)))
        return find_other_extension(self, name, r, 0x615660 /* ".ogg" */);
    ext = fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(name);
    if (!crt_stricmp(ext, glob<const char *>(0x615658)))
        return find_other_extension(self, name, 0xffffffff, 0x615658);
    return r;
}

// ---- The voice-over player ------------------------------------------------------------------------------------

// 0x595b50: the player (created on first use, with its stream); null when it has no stream
BYTE *__cdecl Voice_Get()
{
    BYTE &guard = *glob<BYTE *>(VOICE_GUARD);
    if (!(guard & 1)) {
        *at<BYTE *>(glob_addr(VOICE) + 8) = 0;
        guard |= 1;
        void *s = heap_alloc(8);
        *glob<DWORD *>(VOICE) = s ? ptr_value(stream_ctor(s)) : 0;
        *at<DWORD *>(glob_addr(VOICE) + 4) = 0;
        crt_atexit(code<void(__cdecl *)()>(0x595bc0));
    }
    return *glob<DWORD *>(VOICE) ? glob<BYTE *>(VOICE) : nullptr;
}
// 0x595ac0: the stream released
void THISCALL Voice_Release(BYTE *self)
{
    if (BYTE *s = bp(dw(self, 0))) {
        stream_dtor(s);
        heap_free(s);
        dw(self, 0) = 0;
    }
}
// 0x595b00: after a voice ended the other sounds are no longer lowered
void THISCALL Voice_Frame(BYTE *self)
{
    if (!fl<BYTE>(self, 8) || stream_playing(bp(dw(self, 0))))
        return;
    lower_other_sounds(0);
    fl<BYTE>(self, 8) = 0;
}
// 0x595b30: the priority of the playing voice (0 once it ended)
DWORD THISCALL Voice_Priority(BYTE *self)
{
    if (!stream_playing(bp(dw(self, 0))))
        dw(self, 4) = 0;
    return dw(self, 4);
}
// 0x595be0 (file, priority, out seconds): plays a voice-over when "Play Voiceovers" (Zax.CFG) is on and
// the priority is at least the playing one's; the other sounds are lowered; the voice's length in seconds
DWORD THISCALL Voice_Play(BYTE *self, const char *file, int priority, float *seconds)
{
    if (seconds)
        dw(seconds, 0) = 0;
    if (!fn<DWORD(__fastcall *)(void *, void *, const char *, DWORD)>(0x4a4670)(glob<void *>(0x6bd330), glob<void *>(0x6bd430),
                                                                            glob<const char *>(0x60b05c /* "Play Voiceovers" */), 1))
        return 0;
    if (!stream_playing(bp(dw(self, 0))))
        dw(self, 4) = 0;
    if (priority < (int)dw(self, 4))
        return 0;
    dw(self, 4) = (DWORD)priority;
    if (!stream_play(bp(dw(self, 0)), file, 0))
        return 0;
    lower_other_sounds(1);
    fl<BYTE>(self, 8) = 1;
    if (seconds)
        *seconds = fstp(stream_seconds(bp(dw(self, 0))));
    return 1;
}

// ---- Not sound: the looks of the window toolkit's buttons --------------------------------------------------
// docs/functions.csv counts these as Sound ("SoundObject"): they are the base of the button and text looks
// (vtable 0x602da8/0x602dac: slot 0 the deleting destructor; the button look 0x5f88b4 with its drawing
// 0x4e2240, user interface).

// 0x5994d0: the base look's vtable
void THISCALL Look_Ctor(BYTE *o) { dw(o, 0) = vt(0x602da8); }
// 0x5994e0 (slot 0 of 0x602da8): its vtable, the pool block of 4 with bit 0
BYTE *THISCALL Look_DeletingDtor(BYTE *o, DWORD flags)
{
    dw(o, 0) = vt(0x602da8);
    if (flags & 1)
        pool_free(o, 4);
    return o;
}
// 0x599510 (owner): the look with its owner (+4)
BYTE *THISCALL Look_CtorOwner(BYTE *o, DWORD owner)
{
    dw(o, 0) = vt(0x602dac);
    dw(o, 4) = owner;
    return o;
}
// 0x4e2220 (a, owner): the button look (vtable 0x5f88b4, +8 a)
BYTE *THISCALL ButtonLook_Ctor(BYTE *o, DWORD a, DWORD owner)
{
    Look_CtorOwner(o, owner);
    dw(o, 0) = vt(0x5f88b4);
    dw(o, 8) = a;
    return o;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(music)
{
    const Hook list[] = {
#include "music_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
