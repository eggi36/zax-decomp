// Checks of music.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): CMusicMixerData and CMusicMixer: getters, Create/CreateArray
//    (the mixer's stream object with the exe's real 0x587060, which only reads Zax.CFG and allocates),
//    the deleting destructors, RegisterFields and the registered descriptors.
//  - The functions in the sound fake world (checks_sound_common.h), on objects built in each side's arena
//    from the case seed: the mixer's song setter, track switch, stop and frame (fades in and out, queued
//    and current tracks 0-3, changed songs, muted, stopped and playing streams, queueing that fails), its
//    out-of-line constructor; the application's music frame and volume (fade times 0 and NaN, music on
//    and off, a song name to open or none, volumes around -3333); the music stream object's constructor
//    and Open; the sound file list's destructor and Find (.wav, .ogg, other and no extension, found or
//    not); the voice-over player (first use, frame, priority, play with priorities below, equal and
//    above, "Play Voiceovers" off). The streams, the music folder list, Zax.CFG, the resource cache and
//    the message pump are recorders; the globals they use (the reporter's volume, the folder's count, the
//    voice player) are set per case and put back.
//  - Real data: the world's music mixer (behind +0x334: its fade, songs and tracks), the application's music
//    fields and the voice player, as copies with recorder streams, through their frames.
#include "checks_sound_common.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_music(Hook *out);

namespace {

using namespace sndc;

const invcheck::ClassDef CLASSES[] = {
    {"CMusicMixerData", 0x18, 0x6d004c, 0x51d540, 0x51d590, 0x51d6a0, {0x51d4f0, 0}, true},
    {"CMusicMixer", 0x34, 0x6d0058, 0x51df50, 0x51dfe0, 0x51e1e0, {0x51df00, 0}, true},
};

constexpr DWORD VT_MIXER = 0x5fb420, VT_DATA = 0x5fb224;
constexpr DWORD REPORTER = 0x6c02cc, FOLDER_COUNT = 0x6c101c, VOICE = 0x713c28;
enum { K_NONE };
enum { F_CACHE, F_REPORTER, F_STATE };

const char *const NAMES[] = {"Music/Song A.ogg", "Music/Song B.wav", "Music/Ambient.ogg", "", "Music/Song a.ogg"};
DWORD c_name(Call &) { return ptrv(NAMES[nv() % 5]); }
DWORD c_cache(Call &) { return ptrv(fake(F_CACHE)); }
DWORD c_volume(Call &)
{
    const DWORD v = nv();
    return v % 3 ? dw(fake(F_REPORTER), 0x64) : v % 2 ? 0xfffff2fb : v & 0x7ff;
}
DWORD c_find(Call &)
{
    const DWORD v = nv();
    return v % 3 == 0 ? 0xffffffff : v % 7;
}
DWORD c_ini(Call &) { return nv() % 4 ? 1 : 0; }
DWORD c_seconds(Call &c)
{
    c.fret = nfloat(0.0f, 30.0f);
    c.fset = true;
    return nv();
}
DWORD c_this(Call &c) { return c.ecx; }

const Spec DIRECT[] = {
    {0x587060, "Stream_Ctor", 't', "v", 'c', c_this},
    {0x5870d0, "Stream_Dtor", 't', "", 'n', nullptr},
    {0x587160, "Stream_Frame", 't', "", 'n', nullptr},
    {0x5871f0, "Stream_Queue", 't', "sfb", 'b', nullptr},
    {0x587290, "Stream_SetGain", 't', "f", 'b', nullptr},
    {0x5872b0, "Stream_TakeQueued", 't', "", 'b', nullptr},
    {0x587310, "Stream_Play", 't', "sv", 'b', nullptr},
    {0x5875e0, "Stream_Seconds", 't', "", 'c', c_seconds},
    {0x587600, "Stream_Stop", 't', "", 'n', nullptr},
    {0x587640, "Stream_SetVolume", 't', "v", 'n', nullptr},
    {0x5876c0, "Stream_Volume", 't', "", 'c', c_volume},
    {0x5876d0, "Stream_Muted", 't', "", 'b', nullptr},
    {0x5876e0, "Stream_Playing", 't', "", 'b', nullptr},
    {0x4a3310, "DirList_NameAt", 't', "v", 'c', c_name},
    {0x4a33b0, "DirList_Path", 't', "s", 'c', c_name},
    {0x4a31c0, "DirList_CurrentName", 't', "", 'c', c_name},
    {0x4a32b0, "DirList_NextPath", 't', "s", 'c', c_name},
    {0x4a2ea0, "DirList_Read", 't', "svvv", 'n', nullptr},
    {0x4a2db0, "DirList_Dtor", 't', "", 'n', nullptr},
    {0x591bd0, "NamedList_Find", 't', "sv", 'c', c_find},
    {0x57aef0, "ResourceCache_Get", 'c', "", 'c', c_cache},
    {0x57b0d0, "ResourceCache_SetLowPriority", 't', "v", 'n', nullptr},
    {0x4a4670, "Ini_GetInt", 'f', "sv", 'c', c_ini},
    {0x5998f0, "App_PumpMessages", 'c', "", 'n', nullptr},
    {0x5d0863, "atexit", 'c', "v", 'z', nullptr},
};
const Standins STANDINS[] = {{DIRECT, sizeof DIRECT / sizeof DIRECT[0]}};
const Module MODULE = {hooks_music, nullptr, 0};

DWORD ff(Gen &g, float lo, float hi) { return g.fbits_(lo, hi); }

// A mixer in the running side's arena (or a copy of a real one); its stream an arena block
BYTE *make_mixer(Gen &g, const BYTE *real)
{
    BYTE *m = alloc(0x34);
    BYTE *s = alloc(8);
    if (real) {
        memcpy(m, real, 0x34);
        dw(m, 4) = 0;
        dw(m, 0x10) = 0;
    } else {
        dw(m, 0) = VT_MIXER;
        dw(m, 8) = g.next() % 4 ? ff(g, 0.0f, 8.0f) : 0;
        dw(m, 0xc) = VT_DATA;
        dw(m, 0x14) = g.next() % 5 ? ff(g, 0.0f, 8.0f) : 0x40a00000;
        dw(m, 0x18) = g.below(6);
        dw(m, 0x1c) = g.below(6);
        dw(m, 0x20) = g.next() % 3 ? g.below(6) : dw(m, 0x18);
        dw(m, 0x24) = g.below(4);
        dw(m, 0x28) = g.next() % 2 ? dw(m, 0x24) : g.below(4);
        dw(m, 0x2c) = g.next() & 0xffffff01;
    }
    dw(m, 0x30) = g.next() % 8 ? ptrv(s) : 0;
    if (real || !dw(m, 0x30))
        dw(m, 0x30) = ptrv(s);
    dw(s, 0) = ptrv(fake(F_STATE));
    dw(s, 4) = g.next() & 0x101;
    return m;
}
// The application's music fields in a block of the arena (+0x64 volume, +0x68 a song name, +0x267 on,
// +0x26c/+0x270 the fade, +0x274 the stream object)
BYTE *make_app(Gen &g, const BYTE *real)
{
    BYTE *a = alloc(0x280);
    BYTE *s = alloc(0xc);
    if (real) {
        memcpy(a + 0x64, real + 0x64, 0x210);
        dw(a, 0x26c) = dw((void *)real, 0x26c);
        dw(a, 0x270) = dw((void *)real, 0x270);
        fl<BYTE>(a, 0x267) = fl<BYTE>((void *)real, 0x267);
    } else {
        const DWORD k = g.below(6);
        dw(a, 0x64) = k == 0 ? 0xfffff2fb : k == 1 ? 0xfffff2fc : (DWORD)(-(int)g.below(3000));
        if (g.next() % 3 == 0)
            lstrcpyA(reinterpret_cast<char *>(a + 0x68), NAMES[g.below(3)]);
        fl<BYTE>(a, 0x267) = (BYTE)(g.next() % 4 ? 1 : 0);
        dw(a, 0x26c) = g.next() % 3 ? 0 : ff(g, 0.0f, 5.0f);
        dw(a, 0x270) = ff(g, 0.0f, 5.0f);
    }
    dw(a, 0x274) = ptrv(s);
    dw(s, 0) = ptrv(fake(F_STATE));
    return a;
}

enum What {
    W_SET_SONG, W_SWITCH, W_STOP, W_UPDATE, W_CTOR, W_APP_UPDATE, W_APP_VOLUME, W_STREAM_CTOR, W_OPEN, W_LIST_DTOR, W_FIND,
    W_VOICE_GET, W_VOICE_RELEASE, W_VOICE_FRAME, W_VOICE_PRIORITY, W_VOICE_PLAY, W_LOOK, W_LOOK_DTOR, W_LOOK_OWNER, W_BUTTON_LOOK, NWHAT
};
const char *const WHAT[] = {"CMusicMixer::SetSong",  "CMusicMixer::SwitchTrack", "CMusicMixer::Stop",        "CMusicMixer::Update",
                            "CMusicMixer::Ctor",     "App_UpdateMusic",          "App_SetMusicVolume",       "SoundStream_Ctor",
                            "SoundStream_Open",      "SoundFileList_DeletingDtor", "SoundFileList_Find",     "Voice_Get",
                            "Voice_Release",         "Voice_Frame",              "Voice_Priority",           "Voice_Play",
                            "CLook::Ctor",           "CLook::DeletingDtor",      "CLook::CtorOwner",         "CButtonLook::Ctor"};
const DWORD ADDR[] = {0x51e3e0, 0x51e520, 0x51e450, 0x51e470, 0x51e360, 0x4ce510, 0x4ce3e0, 0x4e1fe0,
                      0x4e2150, 0x4e1fb0, 0x4e2030, 0x595b50, 0x595ac0, 0x595b00, 0x595b30, 0x595be0,
                      0x5994d0, 0x5994e0, 0x599510, 0x4e2220};
const char *const FILES[] = {"Sounds/a.wav", "Sounds/B.WAV", "Sounds/c.ogg", "Sounds/d.Ogg", "Sounds/e", "f.wav", "Voice/x.wav"};

struct Real {
    const BYTE *mixer, *app;
    BYTE voice[12];
    bool has_voice;
};
const Real *g_real;

void run_case(Tally &t, What w, unsigned seed)
{
    Result r{};
    SavedBytes saved_voice, saved_reporter, saved_count;
    save_bytes(saved_voice, VOICE, 0x10);
    save_bytes(saved_reporter, REPORTER, 4);
    save_bytes(saved_count, FOLDER_COUNT, 4);
    Gen g0{seed};
    memset(&g_template, 0, sizeof g_template);
    dw(tmpl(F_REPORTER), 0x64) = g0.next() % 5 ? (DWORD)(-(int)g0.below(2000)) : 0xfffff2fb;
    for (int side = 0; side < 2; side++) {
        Gen g{seed * 31u + 7u};
        side_begin(side, seed, MODULE, (BYTE)(0x5a + seed % 3));
        *at<DWORD *>(REPORTER) = ptrv(fake(F_REPORTER));
        *at<DWORD *>(FOLDER_COUNT) = g.next() % 5 ? 1 + g.below(9) : 0;
        DWORD a[4] = {0, 0, 0, 0}, ecx = 0;
        int nargs = 0;
        bool float_ret = false;
        switch (w) {
        case W_SET_SONG:
            ecx = ptrv(make_mixer(g, nullptr));
            a[0] = g.below(4);
            a[1] = g.next() % 2 ? g.below(6) : dw(at<void *>(ecx), 0x18 + 4 * (a[0] < 3 ? a[0] : 0));
            nargs = 2;
            break;
        case W_SWITCH:
            ecx = ptrv(make_mixer(g, g_real ? g_real->mixer : nullptr));
            a[0] = g.below(4);
            nargs = 1;
            break;
        case W_STOP:
            ecx = ptrv(make_mixer(g, nullptr));
            break;
        case W_UPDATE:
            ecx = ptrv(make_mixer(g, g_real ? g_real->mixer : nullptr));
            a[0] = g_real ? (seed % 2 ? 0x3c888889u : 0x3f000000u) : ff(g, 0.0f, 3.0f);
            nargs = 1;
            break;
        case W_CTOR:
            ecx = ptrv(alloc(0x34));
            memset(at<void *>(ecx), 0x33, 0x34);
            break;
        case W_APP_UPDATE:
        case W_APP_VOLUME:
            ecx = ptrv(make_app(g, g_real ? g_real->app : nullptr));
            break;
        case W_STREAM_CTOR:
            ecx = ptrv(alloc(0xc));
            a[0] = g.next() % 3 ? ptrv("Music") : 0;
            a[1] = g.below(2);
            nargs = 2;
            break;
        case W_OPEN: {
            BYTE *s = alloc(0xc);
            dw(s, 0) = ptrv(fake(F_STATE));
            dw(s, 8) = 0;   // no callback (none in the exe's callers)
            ecx = ptrv(s);
            a[0] = g.next() % 3 ? ptrv(NAMES[g.below(3)]) : 0;
            nargs = 1;
            break;
        }
        case W_LIST_DTOR:
            ecx = ptrv(alloc(0x12c));
            a[0] = g.below(2);
            nargs = 1;
            break;
        case W_FIND:
            ecx = ptrv(alloc(0x12c));
            a[0] = ptrv(FILES[g.below(7)]);
            a[1] = g.next() % 2 ? 0xffffffff : g.below(5);
            nargs = 2;
            break;
        case W_LOOK: case W_LOOK_DTOR: case W_LOOK_OWNER: case W_BUTTON_LOOK:
            ecx = ptrv(alloc(0xc));
            memset(at<void *>(ecx), 0x3c, 0xc);
            a[0] = g.next();
            a[1] = g.next();
            nargs = w == W_LOOK ? 0 : w == W_BUTTON_LOOK ? 2 : 1;
            if (w == W_LOOK_DTOR)
                a[0] = g.below(2);
            break;
        case W_VOICE_GET: {
            BYTE *v = at<BYTE *>(VOICE);
            dw(v, 0) = g.next() % 3 ? ptrv(alloc(8)) : 0;
            dw(v, 4) = g.below(4);
            fl<BYTE>(v, 8) = (BYTE)g.below(2);
            fl<BYTE>(v, 0xc) = (BYTE)(g.next() % 2 ? 1 : 0);   // the guard
            break;
        }
        default: {
            // the voice player's functions on a player in the arena (or the real one's copy)
            BYTE *v = alloc(0xc);
            if (g_real && g_real->has_voice)
                memcpy(v, g_real->voice, 12);
            if (!(g_real && g_real->has_voice) || dw(v, 0))
                dw(v, 0) = ptrv(alloc(8));
            if (!g_real) {
                dw(v, 4) = g.below(5);
                fl<BYTE>(v, 8) = (BYTE)(g.next() % 3 ? 1 : 0);
            }
            dw(at<void *>(dw(v, 0)), 0) = ptrv(fake(F_STATE));
            ecx = ptrv(v);
            if (w == W_VOICE_PLAY) {
                a[0] = ptrv(FILES[6]);
                a[1] = g.below(6);
                a[2] = g.next() % 4 ? ptrv(alloc(4)) : 0;
                nargs = 3;
            }
            break;
        }
        }
        if (w == W_VOICE_GET)
            r.eax[side] = call_fn(ADDR[w], 0, 0, 0, a, false, nullptr, true);
        else
            r.eax[side] = call_fn(ADDR[w], ecx, 0, nargs, a, float_ret, &r.st0[side]);
        if (w == W_VOICE_GET) {
            // the player's state into the arena for the comparison
            BYTE *copy = alloc(0x10);
            memcpy(copy, at<void *>(VOICE), 0x10);
        }
        side_end();
        restore_bytes(saved_voice);
    }
    restore_bytes(saved_count);
    restore_bytes(saved_reporter);
    // void functions leave what their last call left in eax; SetSong, Find, Open, Priority, Play and Get
    // return values
    DWORD mask = 0;
    if (w == W_SET_SONG || w == W_FIND || w == W_VOICE_PRIORITY || w == W_VOICE_GET || w == W_CTOR || w == W_STREAM_CTOR ||
        w == W_LIST_DTOR || w == W_LOOK_DTOR || w == W_LOOK_OWNER || w == W_BUTTON_LOOK)
        mask = 0xffffffff;
    if (w == W_OPEN || w == W_VOICE_PLAY)
        mask = 0xff;
    compare(t, WHAT[w], seed, r, mask);
}

void test_functions(Tally &t)
{
    const int COUNTS[NWHAT] = {200, 600, 50, 800, 30, 400, 100, 60, 400, 30, 300, 60, 40, 80, 80, 300, 10, 20, 10, 10};
    for (int w = 0; w < NWHAT; w++)
        for (int k = 0; k < scaled(COUNTS[w]); k++)
            run_case(t, (What)w, 0x2000u * (unsigned)(w + 1) + (unsigned)k);
}

BYTE g_mixer_copy[0x34], g_app_copy[0x280];
Real g_the_real;

}  // namespace

ZAX_CHECKS(music)
{
    (void)cls;
    (void)n;
    Tally getters{"music: class getters"}, objects{"music: Create, CreateArray, destructors"}, fields{"music: RegisterFields"},
        real_fields{"music: RegisterFields against the registered fields (real data)"},
        functions{"music: the mixer, the application's music, the music stream, the file list, the voice-over player"},
        registered{"music: registered class infos (real data)"},
        real{"music: the world's mixer, the application's music and the voice player as copies (real data)"};
    invcheck::g_hook_table = hooks_music;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, 2);
    else
        objects.check(false, "music", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    for (const invcheck::ClassDef &d : CLASSES)
        invcheck::test_registered_class(registered, d);
    // real data: the world's mixer (behind +0x334), the application (0x713f14), the voice player
    BYTE *world = *at<BYTE **>(0x6c2080);
    BYTE *app = *at<BYTE **>(0x713f14);
    g_the_real = {};
    BYTE *mixer = world && readable(world + 0x334, 4) ? at<BYTE *>(dw(world, 0x334)) : nullptr;
    if (mixer && readable(mixer, 0x34) && dw(mixer, 0) == VT_MIXER) {
        memcpy(g_mixer_copy, mixer, 0x34);
        g_the_real.mixer = g_mixer_copy;
    }
    if (app && readable(app, 0x280)) {
        memcpy(g_app_copy, app, 0x280);
        g_the_real.app = g_app_copy;
    }
    if ((*at<BYTE *>(0x713c34) & 1) && readable(at<void *>(VOICE), 12)) {
        memcpy(g_the_real.voice, at<void *>(VOICE), 12);
        g_the_real.has_voice = true;
    }
    if (world_on(STANDINS, 1)) {
        test_functions(functions);
        g_real = &g_the_real;
        for (unsigned k = 0; k < 6; k++) {
            if (g_the_real.mixer) {
                run_case(real, W_UPDATE, 0x70000u + k);
                run_case(real, W_SWITCH, 0x70100u + k);
            }
            if (g_the_real.app)
                run_case(real, W_APP_UPDATE, 0x70200u + k);
            run_case(real, W_VOICE_FRAME, 0x70300u + k);
            run_case(real, W_VOICE_PRIORITY, 0x70400u + k);
        }
        g_real = nullptr;
        world_off();
    } else {
        functions.check(false, "music", "[the fake world could not be set up]");
    }
    log("  music real data: mixer %d, application %d, voice player %d", g_the_real.mixer ? 1 : 0, g_the_real.app ? 1 : 0,
        g_the_real.has_voice ? 1 : 0);
    functions.report();
    registered.report();
    real.report();
}

}  // namespace zax
