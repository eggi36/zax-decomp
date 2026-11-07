// Checks of sound_resource.cpp against the original, in the game thread.
//
// In the sound fake world (checks_sound_common.h): DirectSound and its buffers are fake COM objects
// whose slots record their arguments and answer from the case (HRESULTs with failures, written buffers,
// volumes, statuses, locked parts), DirectSoundCreate and the reader factory are recorders, the resource
// cache's and the resource base's methods (resource_manager.cpp, resource_cache.cpp, verified) are
// recorders with answers of the case, the table math is the real one. The manager and the resources
// under test lie in each side's arena with their real vtables. The globals of the sound (0x713680-
// 0x7136ff: DirectSound, the primary buffer, the listener, the manager's static) and the reporter's
// music volume are set per case, compared after it and put back.
//
// Cases: the listener's setters and getters; the buffer format (with and without a buffer, 8/16 bits,
// mono/stereo); the manager's activation (constructed or not), Init (DirectSound present, creation or
// cooperative level failing, the primary buffer failing once or twice), Shutdown, the destructor, the new
// resource, the volume, stereo, level, mute, enable and apply slots with and without DirectSound; every
// slot of CSoundResource on resources with and without a buffer, positioned or not, looping or not; the
// 3D update with listener and sound positions around, inside and beyond the near and far distances (also
// near above far, zero and NaN distances); Load with a reader or none, big sounds, mono/stereo 8/16 bits,
// buffer creation, Lock and Unlock failing; the options from Zax.CFG (mutes on and off).
#include "checks_sound_common.h"

namespace zax {

int hooks_sound_resource(Hook *out);

namespace {

using namespace sndc;

constexpr DWORD VT_MANAGER = 0x600b64, VT_SOUND = 0x600b8c;
constexpr DWORD GLOBALS = 0x713680, GLOBALS_SIZE = 0x80, REPORTER = 0x6c02cc;
enum { K_DS, K_BUF, K_READER };
enum { F_DS, F_PRIMARY, F_BUF, N_BUF = 3, F_READER = F_BUF + N_BUF, F_REPORTER };

BYTE *g_cache;   // what ResourceCache_Get answers (the manager under test)

DWORD c_cache(Call &) { return ptrv(g_cache); }
DWORD c_this(Call &c) { return c.ecx; }
DWORD c_res_ctor(Call &c)
{
    dw(at<void *>(c.ecx), 0) = 0x600b24;
    return c.ecx;
}
DWORD c_bool(Call &)
{
    const DWORD v = nv();
    return (v & 0xffffff00u) | (v % 5 ? 1u : 0u);
}
DWORD c_ds_create(Call &c)
{
    if (nv() % 6 == 0)
        return 0x88780064;
    dw(at<void *>(c.a[1]), 0) = ptrv(fake(F_DS));
    return 0;
}
DWORD c_create_buffer(Call &c)
{
    const DWORD v = nv();
    if (v % 5 == 0)
        return 0x8878000a;
    // a success always gives a buffer (DirectSound does; the exe does not test it for the primary buffer)
    dw(at<void *>(c.a[2]), 0) = ptrv(fake(c.a[1] && dw(at<void *>(c.a[1]), 4) & 1 ? F_PRIMARY : F_BUF + (int)(v % N_BUF)));
    return 0;
}
DWORD c_duplicate(Call &c)
{
    const DWORD v = nv();
    if (v % 5 == 0)
        return 0x88780032;
    dw(at<void *>(c.a[2]), 0) = ptrv(fake(F_BUF + (int)(v % N_BUF)));
    return 0;
}
DWORD c_get_volume(Call &c)
{
    if (nv() % 6 == 0)
        return 0x88780096;
    dw(at<void *>(c.a[1]), 0) = (DWORD)(-(int)(nv() % 4000));
    return 0;
}
DWORD c_get_status(Call &c)
{
    const DWORD v = nv();
    if (v % 6 == 0)
        return 0x88780096;
    if (v % 7)
        dw(at<void *>(c.a[1]), 0) = v >> 4;
    return 0;
}
BYTE g_locked[0x400];
DWORD c_lock(Call &c)
{
    const DWORD v = nv();
    if (v % 6 == 0)
        return 0x887800a0;
    dw(at<void *>(c.a[3]), 0) = ptrv(g_locked);
    dw(at<void *>(c.a[4]), 0) = c.a[2] < sizeof g_locked ? c.a[2] : sizeof g_locked;
    return 0;
}
DWORD c_get_format(Call &c)
{
    BYTE *f = at<BYTE *>(c.a[1]);
    if (nv() % 3 == 0)
        dw(f, 4) = 0x5622 + nv() % 50;   // what the primary buffer had (read back as a dword in the exe)
    return 0;
}
DWORD c_reader(Call &) { return nv() % 6 == 0 ? 0 : ptrv(fake(F_READER)); }
DWORD c_size(Call &) { return nv() % 9 == 0 ? 500001 + nv() % 10 : nv() % 0x800; }
DWORD c_format(Call &c)
{
    BYTE *f = at<BYTE *>(c.a[0]);
    const DWORD v = nv();
    fl<WORD>(f, 0) = 1;
    fl<WORD>(f, 2) = (WORD)(1 + v % 2);
    dw(f, 4) = v % 3 ? 22050 : 11025;
    fl<WORD>(f, 0xe) = (WORD)((v >> 4) % 3 == 0 ? 8 : (v >> 4) % 3 == 1 ? 16 : 12);
    fl<WORD>(f, 0xc) = 2;
    dw(f, 8) = 44100;
    fl<WORD>(f, 0x10) = 0;
    return 0;
}
DWORD c_reader_read(Call &c)
{
    BYTE *out = at<BYTE *>(c.a[0]);
    for (DWORD i = 0; i < c.a[1] && i < sizeof g_locked; i++)
        out[i] = (BYTE)(i * 3);
    return c.a[1];
}
DWORD c_ini(Call &c)
{
    const DWORD v = nv();
    return v % 3 ? c.a[1] : v % 2 ? 1 : (DWORD)(-(int)(v % 3000));
}
DWORD c_ini_float(Call &c)
{
    c.fret = nfloat(0.0f, 1.0f);
    c.fset = true;
    return nv();
}

const Spec DIRECT[] = {
    {0x57aef0, "ResourceCache_Get", 'c', "", 'c', c_cache},
    {0x57b200, "ResourceCache_ForgetAll", 't', "", 'c', c_bool},
    {0x57b170, "ResourceCache_SetByte30", 't', "b", 'c', c_bool},
    {0x57b180, "ResourceCache_SetLevel", 't', "v", 'c', c_bool},
    {0x57b1c0, "ResourceCache_SetDisabled", 't', "v", 'c', c_bool},
    {0x57b1e0, "ResourceCache_CheckEnabled", 't', "", 'c', c_bool},
    {0x57b1d0, "ResourceCache_SetScale", 't', "f", 'b', nullptr},
    {0x57b1b0, "ResourceCache_SetLimit", 't', "v", 'b', nullptr},
    {0x57af00, "ResourceCache_Ctor", 't', "", 'c', c_this},
    {0x57af50, "ResourceCache_Dtor", 't', "", 'c', c_this},
    {0x57b230, "Resource_Ctor", 't', "", 'c', c_res_ctor},
    {0x57b280, "Resource_Dtor", 't', "", 'n', nullptr},
    {0x57b2f0, "Resource_Reset", 't', "", 'c', c_bool},
    {0x57b370, "Resource_Assign", 't', "p", 'c', c_bool},
    {0x57b950, "Resource_Insert", 't', "v", 'c', c_bool},
    {0x57bba0, "Resource_SetFlag2", 't', "v", 'c', c_bool},
    {0x57bb10, "Resource_ReloadFalse", 't', "v", 'c', c_bool},
    {0x57bbf0, "Resource_SetLoadCost", 't', "v", 'c', c_bool},
    {0x57bd00, "Resource_Unlink", 't', "", 'c', c_bool},
    {0x57bd30, "Resource_Unload", 't', "", 'c', c_bool},
    {0x57bc60, "Resource_SetPriority", 't', "v", 'n', nullptr},
    {0x5caa88, "DirectSoundCreate", 's', "ppp", 'c', c_ds_create},
    {0x57ca10, "SoundReader_ForFile", 'f', "", 'c', c_reader},
    {0x483750, "Reader_Size", 't', "", 'c', c_size},
    {0x57d2a0, "CSoundReader::Format", 't', "p", 'c', c_format},
    {0x4a4670, "Ini_GetInt", 'f', "sv", 'c', c_ini},
    {0x4a46e0, "Ini_GetFloat", 'f', "sf", 'c', c_ini_float},
    {0x5d0863, "atexit", 'c', "v", 'z', nullptr},
};
#define NOSLOT {0, nullptr, 0, "", 0, nullptr}
const Spec DS_SLOTS[] = {
    NOSLOT, NOSLOT, {0, "IDirectSound::Release", 'C', "p", 'n', nullptr},
    {0, "IDirectSound::CreateSoundBuffer", 'C', "pDpv", 'c', c_create_buffer}, NOSLOT,
    {0, "IDirectSound::DuplicateSoundBuffer", 'C', "ppp", 'c', c_duplicate},
    {0, "IDirectSound::SetCooperativeLevel", 'C', "pvv", 'h', nullptr},
};
const Spec BUF_SLOTS[] = {
    NOSLOT, NOSLOT, {0, "IDirectSoundBuffer::Release", 'C', "p", 'n', nullptr}, NOSLOT, NOSLOT,
    {0, "IDirectSoundBuffer::GetFormat", 'C', "p-v-", 'c', c_get_format},
    {0, "IDirectSoundBuffer::GetVolume", 'C', "pp", 'c', c_get_volume}, NOSLOT, NOSLOT,
    {0, "IDirectSoundBuffer::GetStatus", 'C', "p-", 'c', c_get_status}, NOSLOT,
    {0, "IDirectSoundBuffer::Lock", 'C', "pvv--vvv", 'c', c_lock},
    {0, "IDirectSoundBuffer::Play", 'C', "pvvv", 'h', nullptr},
    {0, "IDirectSoundBuffer::SetCurrentPosition", 'C', "pv", 'h', nullptr},
    {0, "IDirectSoundBuffer::SetFormat", 'C', "pw", 'h', nullptr},
    {0, "IDirectSoundBuffer::SetVolume", 'C', "pv", 'h', nullptr},
    {0, "IDirectSoundBuffer::SetPan", 'C', "pv", 'h', nullptr},
    {0, "IDirectSoundBuffer::SetFrequency", 'C', "pv", 'h', nullptr},
    {0, "IDirectSoundBuffer::Stop", 'C', "p", 'h', nullptr},
    {0, "IDirectSoundBuffer::Unlock", 'C', "ppvvv", 'h', nullptr},
};
const Spec READER_SLOTS[] = {
    NOSLOT, {0, "Reader::DeletingDtor", 't', "v", 'c', c_this}, {0, "Reader::Read", 't', "pv", 'c', c_reader_read},
};
#undef NOSLOT
const Standins STANDINS[] = {{DIRECT, sizeof DIRECT / sizeof DIRECT[0]}};
const Module MODULE = {hooks_sound_resource, nullptr, 0};

DWORD ff(Gen &g, float lo, float hi) { return g.fbits_(lo, hi); }

BYTE *make_manager(Gen &g)
{
    BYTE *m = alloc(0x40);
    dw(m, 0) = VT_MANAGER;
    dw(m, 4) = (DWORD)(-(int)g.below(4000));
    fl<BYTE>(m, 0xc) = (BYTE)g.below(2);
    dw(m, 0x10) = (DWORD)(-(int)g.below(4000));
    fl<BYTE>(m, 0x30) = (BYTE)(g.next() % 4 ? g.below(2) : g.next());
    return m;
}
BYTE *make_sound(Gen &g)
{
    BYTE *r = alloc(0x9c);
    for (DWORD o = 4; o < 0x9c; o += 4)
        dw(r, o) = g.next() % 3 ? 0 : g.next();
    dw(r, 0) = VT_SOUND;
    dw(r, 0x14) = g.next() & 0x6e;
    const DWORD k = g.below(6);
    dw(r, 0x38) = k == 0 ? 0 : ff(g, 0.0f, 300.0f);
    dw(r, 0x3c) = k == 1 ? dw(r, 0x38) : ff(g, 0.0f, 1500.0f);
    for (DWORD o = 0x54; o <= 0x5c; o += 4)
        dw(r, o) = ff(g, -2000.0f, 2000.0f);
    dw(r, 0x84) = g.next() % 4 ? ptrv(fake(F_BUF + (int)g.below(N_BUF))) : 0;
    dw(r, 0x70) = ptrv(r + 0x88);
    dw(r, 0x8c) = 22050;
    return r;
}
// The globals of the case: DirectSound and the primary buffer there or not, the listener
void set_globals(Gen &g)
{
    BYTE *gl = at<BYTE *>(GLOBALS);
    const DWORD k = g.below(8);
    dw(gl, 0x74) = k ? ptrv(fake(F_DS)) : 0;                       // 0x7136f4
    dw(gl, 0x14) = k > 1 ? ptrv(fake(F_PRIMARY)) : 0;              // 0x713694
    dw(gl, 0x78) = g.below(2);                                     // 0x7136f8
    dw(gl, 0x10) = g.below(3);                                     // 0x713690
    for (DWORD o = 0x20; o <= 0x28; o += 4)                         // the listener 0x7136a0
        dw(gl, o) = ff(g, -2000.0f, 2000.0f);
    dw(gl, 0x2c) = g.next() % 4 ? 0x3f800000 : ff(g, 0.0f, 3.0f);   // the distance factor
}

enum What {
    W_SET_POS, W_GET_POS, W_SET_FACTOR, W_FACTOR, W_SET_98, W_GET_98, W_SET_F0, W_FORMAT, W_ACTIVATE, W_INIT, W_SHUTDOWN,
    W_MANAGER_DTOR, W_NEW, W_READ_VOLUME, W_STEREO, W_LEVEL, W_MUTED, W_ENABLED, W_APPLY, W_RESET, W_ASSIGN, W_UPDATE3D,
    W_INSERT, W_POSITIONED, W_POSITION, W_DISTANCES, W_SET40, W_SET48, W_VOLUME, W_FREQUENCY, W_PAN, W_STOP, W_UNLOAD,
    W_DTOR, W_PLAYING, W_LOAD, W_OPTIONS, NWHAT
};
const DWORD ADDR[] = {0x57c170, 0x57c1c0, 0x57c1f0, 0x57c200, 0x57c210, 0x57c220, 0x57c230, 0x57c7b0, 0x57bdc0, 0x57be50,
                      0x57bf60, 0x57be20, 0x57bfa0, 0x57bfe0, 0x57c010, 0x57c050, 0x57c090, 0x57c110, 0x57c140, 0x57c270,
                      0x57c2c0, 0x57c4f0, 0x57c620, 0x57c8d0, 0x57c8f0, 0x57c930, 0x57c860, 0x57c890, 0x57c710, 0x57c730,
                      0x57c6d0, 0x57c6a0, 0x57c950, 0x57c240, 0x57c780, 0x57c340, 0x57ee90};
const char *const WHAT[] = {"Listener_SetPosition", "Listener_GetPosition", "Listener_SetDistanceFactor", "Listener_DistanceFactor",
                            "Listener_Set713698", "Listener_Get713698", "Listener_Set7136f0", "Sound_SetBufferFormat",
                            "SoundManager_Activate", "CSoundManager::Init", "CSoundManager::Shutdown", "CSoundManager::DeletingDtor",
                            "CSoundManager::NewResource", "CSoundManager::ReadVolume", "CSoundManager::SetStereo",
                            "CSoundManager::SetLevel", "CSoundManager::SetMuted", "CSoundManager::CheckEnabled",
                            "CSoundManager::ApplyVolume", "CSoundResource::Reset", "CSoundResource::Assign", "CSoundResource::Update3D",
                            "CSoundResource::Insert", "CSoundResource::SetPositioned", "CSoundResource::SetPosition",
                            "CSoundResource::SetDistances", "CSoundResource::SetSetting40", "CSoundResource::SetSetting48",
                            "CSoundResource::SetVolume", "CSoundResource::SetFrequency", "CSoundResource::SetPan", "CSoundResource::Stop",
                            "CSoundResource::Unload", "CSoundResource::DeletingDtor", "CSoundResource::IsPlaying", "CSoundResource::Load",
                            "Sound_LoadOptions"};

void run_case(Tally &t, What w, unsigned seed)
{
    Result r{};
    SavedBytes saved_globals, saved_reporter;
    save_bytes(saved_globals, GLOBALS, GLOBALS_SIZE);
    save_bytes(saved_reporter, REPORTER, 4);
    memset(&g_template, 0, sizeof g_template);
    make_fake(F_DS, K_DS);
    make_fake(F_PRIMARY, K_BUF);
    dw(tmpl(F_PRIMARY), 4) = 0x5049;   // tells the primary from the other buffers in the logs
    for (int i = 0; i < N_BUF; i++)
        make_fake(F_BUF + i, K_BUF);
    make_fake(F_READER, K_READER);
    dw(tmpl(F_REPORTER), 0x64) = 0x1234;
    for (int side = 0; side < 2; side++) {
        Gen g{seed * 31u + 7u};
        side_begin(side, seed, MODULE, (BYTE)(0x5a + seed % 3));
        set_globals(g);
        *at<DWORD *>(REPORTER) = ptrv(fake(F_REPORTER));
        g_cache = make_manager(g);
        DWORD a[4] = {0, 0, 0, 0}, ecx = 0, edx = 0;
        int nargs = 0;
        bool float_ret = false, cdecl_ = false;
        switch (w) {
        case W_SET_POS: {
            DWORD *p = reinterpret_cast<DWORD *>(alloc(12));
            for (int i = 0; i < 3; i++)
                p[i] = g.next() % 3 ? dw(at<void *>(GLOBALS), 0x20 + 4 * (DWORD)i) : ff(g, -100.0f, 100.0f);
            a[0] = ptrv(p);
            nargs = 1;
            break;
        }
        case W_GET_POS:
            a[0] = ptrv(alloc(12));
            nargs = 1;
            break;
        case W_SET_FACTOR:
        case W_SET_98:
        case W_SET_F0:
            a[0] = ff(g, 0.0f, 4.0f);
            nargs = 1;
            break;
        case W_FACTOR:
        case W_GET_98:
            float_ret = true;
            break;
        case W_FORMAT:
            // the exe reads the format from the primary buffer, which is there whenever it is called
            dw(at<void *>(GLOBALS), 0x14) = ptrv(fake(F_PRIMARY));
            ecx = g.next() % 5 ? ptrv(fake(g.next() % 2 ? F_PRIMARY : F_BUF)) : 0;
            edx = g.next() % 3 ? 22050 : g.next();
            a[0] = g.next() % 3 ? 16 : g.next() % 2 ? 8 : g.below(40);
            a[1] = g.next() % 4 ? g.below(2) : g.next();
            nargs = 2;
            break;
        case W_ACTIVATE:
            *at<BYTE *>(0x7136b0) = (BYTE)(g.next() % 4 ? 1 : 0);
            cdecl_ = true;
            break;
        case W_OPTIONS:
            cdecl_ = true;
            break;
        case W_INIT:
        case W_SHUTDOWN:
        case W_READ_VOLUME:
        case W_ENABLED:
        case W_APPLY:
            ecx = ptrv(g_cache);
            break;
        case W_MANAGER_DTOR:
            ecx = ptrv(g_cache);
            a[0] = g.below(2);
            nargs = 1;
            break;
        case W_NEW:
            ecx = ptrv(g_cache);
            break;
        case W_STEREO:
        case W_LEVEL:
        case W_MUTED:
            ecx = ptrv(g_cache);
            a[0] = w == W_LEVEL ? (g.next() % 4 ? (DWORD)(-(int)g.below(4000)) : 0xfffff2fb) : g.next() % 4 ? g.below(2) : g.next();
            nargs = 1;
            break;
        default: {
            BYTE *s = make_sound(g);
            ecx = ptrv(s);
            switch (w) {
            case W_ASSIGN: a[0] = ptrv(make_sound(g)); nargs = 1; break;
            case W_INSERT: case W_POSITIONED: case W_DTOR: a[0] = g.below(4); nargs = 1; break;
            case W_POSITION: {
                DWORD *p = reinterpret_cast<DWORD *>(alloc(12));
                for (int i = 0; i < 3; i++)
                    p[i] = ff(g, -2000.0f, 2000.0f);
                a[0] = ptrv(p);
                a[1] = g.next();
                nargs = 2;
                break;
            }
            case W_DISTANCES: a[0] = ff(g, 0.0f, 300.0f); a[1] = ff(g, 0.0f, 1500.0f); a[2] = g.next(); nargs = 3; break;
            case W_SET40:
                a[0] = g.next() % 2 ? dw(s, 0x40) : g.below(4);
                a[1] = g.next() % 2 ? dw(s, 0x44) : ff(g, 0.0f, 2.0f);
                nargs = 2;
                break;
            case W_SET48:
                a[0] = g.next() % 2 ? dw(s, 0x48) : g.below(4);
                a[1] = g.next() % 2 ? dw(s, 0x4c) : ff(g, 0.0f, 2.0f);
                a[2] = g.next() % 2 ? dw(s, 0x50) : ff(g, 0.0f, 2.0f);
                nargs = 3;
                break;
            case W_VOLUME: case W_PAN: a[0] = (DWORD)((int)g.below(8000) - 6000); nargs = 1; break;
            case W_FREQUENCY: a[0] = g.next() % 3 ? g.below(200000) : g.below(150); nargs = 1; break;
            case W_LOAD: a[0] = ptrv("Sounds/x.wav"); a[1] = g.below(8); nargs = 2; break;
            default: break;
            }
            break;
        }
        }
        r.float_ret = float_ret;
        r.eax[side] = call_fn(ADDR[w], ecx, edx, nargs, a, float_ret, &r.st0[side], cdecl_);
        // the globals and the reporter's music volume into the arena for the comparison
        memcpy(alloc(GLOBALS_SIZE), at<void *>(GLOBALS), GLOBALS_SIZE);
        side_end();
        restore_bytes(saved_globals);
        restore_bytes(saved_reporter);
    }
    DWORD mask = 0xff;
    switch (w) {
    case W_SET_POS: case W_GET_POS: case W_SET_FACTOR: case W_SET_98: case W_SET_F0: case W_FORMAT: case W_ACTIVATE:
    case W_UPDATE3D: case W_SET40: case W_SET48: case W_OPTIONS: case W_FACTOR: case W_GET_98:
    case W_SHUTDOWN:   // void: without DirectSound eax stays what it was at the call
        mask = 0;
        break;
    case W_MANAGER_DTOR: case W_NEW: case W_DTOR: case W_LEVEL:
        mask = 0xffffffff;
        break;
    default: break;
    }
    compare(t, WHAT[w], seed, r, mask);
}

void test_all(Tally &t)
{
    const int COUNTS[NWHAT] = {60, 30, 20, 20, 20, 20, 20, 200, 30, 400, 60, 30, 40, 80, 100, 150, 200, 100, 60, 60, 150, 2500,
                               300, 60, 300, 40, 150, 200, 80, 150, 150, 100, 100, 60, 150, 700, 200};
    for (int w = 0; w < NWHAT; w++)
        for (int k = 0; k < scaled(COUNTS[w]); k++)
            run_case(t, (What)w, 0x4000u * (unsigned)(w + 1) + (unsigned)k);
}

// Real data: the level's sound resources (the sound list's, 0x713708: their distances, flags and
// positions) as copies through the 3D update with the real listener
BYTE g_copies[32][0x9c];
int collect_real()
{
    BYTE *list = at<BYTE *>(0x713708);
    const DWORD n = dw(list, 0x14);
    const BYTE *records = at<const BYTE *>(dw(list, 0x34));
    if (!n || !records || !readable(records, 0x108 * n))
        return 0;
    int found = 0;
    for (DWORD i = 0; i < n && found < 32; i++) {
        if (!spread_pick((int)i, (int)n, 32))
            continue;
        const BYTE *res = at<const BYTE *>(*reinterpret_cast<const DWORD *>(records + 0x108 * i + 0x104));
        if (!res || !readable(res, 0x9c) || dw((void *)res, 0) != VT_SOUND)
            continue;
        memcpy(g_copies[found++], res, 0x9c);
    }
    return found;
}
void test_real(Tally &t, int n)
{
    for (int i = 0; i < n; i++)
        for (int side = 0, k = 0; k < 1; k++) {
            (void)side;
            Result r{};
            SavedBytes saved_globals;
            save_bytes(saved_globals, GLOBALS, GLOBALS_SIZE);
            memset(&g_template, 0, sizeof g_template);
            make_fake(F_BUF, K_BUF);
            for (int s = 0; s < 2; s++) {
                side_begin(s, 0x91000u + (unsigned)i, MODULE, 0x5b);
                BYTE *res = alloc(0x9c);
                memcpy(res, g_copies[i], 0x9c);
                dw(res, 4) = 0;
                if (dw(res, 0x84))
                    dw(res, 0x84) = ptrv(fake(F_BUF));
                r.eax[s] = call_fn(0x57c4f0, ptrv(res), 0, 0, nullptr, false, nullptr);
                side_end();
                restore_bytes(saved_globals);
            }
            compare(t, "CSoundResource::Update3D (real)", 0x91000u + (unsigned)i, r, 0);
        }
}

}  // namespace

ZAX_CHECKS(sound_resource)
{
    (void)cls;
    (void)n;
    Tally all{"sound_resource: the sound manager, the sound resources, the listener and the options in the fake world"},
        real{"sound_resource: the sound list's resources through the 3D update with the real listener (real data)"};
    const int found = collect_real();
    set_kind(K_DS, DS_SLOTS, sizeof DS_SLOTS / sizeof DS_SLOTS[0]);
    set_kind(K_BUF, BUF_SLOTS, sizeof BUF_SLOTS / sizeof BUF_SLOTS[0]);
    set_kind(K_READER, READER_SLOTS, sizeof READER_SLOTS / sizeof READER_SLOTS[0]);
    if (world_on(STANDINS, 1)) {
        test_all(all);
        test_real(real, found);
        world_off();
    } else {
        all.check(false, "sound_resource", "[the fake world could not be set up]");
    }
    log("  sound_resource real data: %d resources of the sound list", found);
    all.report();
    real.report();
}

}  // namespace zax
