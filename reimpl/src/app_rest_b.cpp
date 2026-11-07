// The last game flow functions of 0x480000-0x4dffff (phase 3 remains, band B; docs/DECOMPILATION.md section
// 8): the debug log's sections 0x4a2650/0x4a2670, the frame rate counter 0x4a81f0-0x4a83c0, and of the
// application base (vtable 0x5f73e8, app_base.cpp) the destructor 0x4cdcc0/0x4cdc50, the start-up loading
// of palettes and sounds 0x4cdf70, the frame 0x4cdfd0 and the switch between active and inactive 0x4ce400.
//
// The debug log [confirmed: code; EXE_FINDINGS.md]: the file 0x6bdf48 (open with the launcher's /DebugLog)
// and the depth 0x6bdf68. A section begins with its name ("%s\n" 0x613800) and ends with "section done", the
// outermost with "END of all active section".
//
// The frame rate counter [confirmed: code; checked] (0x10c bytes; the application's three at 0x6c00b0,
// 0x6bfe88, 0x6c01c0 and the frame counter's 0x6bffa0): 30 samples (8 bytes, +0), their sum +0xf0 (64 bit),
// the last counter +0xf8 (64 bit, QueryPerformanceCounter 0x53d150), the next sample +0x100, the number of
// samples +0x104 (at most 30), the frequency +0x108 (0x53d170). The constructor and the reset clear it;
// Start takes the counter; Sample adds the time since the last counter (its low 32 bits, at most one second)
// in place of the oldest sample and returns the rate frequency * 30 / sum (st0) once 30 samples are in, 0
// before (and when no counter was taken).
//
// The application base [confirmed: code; checked; layout in app_base.cpp]: +4 the display, +0x34 the window,
// +0x38 the frame number, +0x60 active, +0x64 the music volume, +0x68 the next music (char[0xff]), +0x167 the
// music after a fade, +0x266 fading, +0x267 the music changed, +0x268 fade into the next music, +0x26c/+0x270
// the fade's time and how far it is, +0x274 the music stream.
//  - The destructor unmounts the archives, deletes the window (slot 0) and the music stream (0x4e2020, then
//    operator delete) and the display (slot 1), runs the empty 0x58ea40 and 0x42b6a0, all resource lists'
//    reverse calls, the resource cache's slot 1, clears the error reporter 0x6c02cc and the flag 0x6bda40.
//  - The frame (seconds): the frame timers 0x6c01c0 and 0x6c00b0 start once per frame number (0x6c02d8,
//    0x6c02e0), the resource cache gets a new stamp, a running fade adds the seconds and when it is over
//    sets the volume and takes the next music (or none), then the music is updated (0x4ce510, Sound); the
//    display (0x567ab0) and the window (slot 2) get the seconds, and the rate of 0x6c01c0 is stored in
//    0x6c02d0.
//  - Active (bool): nothing when unchanged; else the keys are cleared, and the resource cache is restored
//    (slots 4 and 8) and the mouse 0x6cff40 acquired (0x5185e0), or the cache released (slot 9) and the
//    mouse given up (0x5185d0). al: changed.
//  - The start-up loading (World::LoadGameData): the blend tables of "Palettes"/"GamePalette" (Rendering),
//    the sound manager's folders (Sound) and, with "Pre-load all sounds" in the INI, all sounds.
//
// Return values: the original's eax where it is defined (the fprintf / API results, the timer's frequency
// and this, the deleting destructor's this, st0 for the rate); al for Active.
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

// ---- The debug log ---------------------------------------------------------------------------------------

constexpr DWORD G_LOG_FILE = 0x6bdf48, G_LOG_DEPTH = 0x6bdf68;
DWORD fprintf_(DWORD file, DWORD fmt, const char *s) { return fn<DWORD(__cdecl *)(DWORD, DWORD, const char *)>(0x5d19bc)(file, fmt, s); }

// 0x4a2650 DebugLog_SectionBegin (name)
DWORD __fastcall DebugLog_SectionBegin(const char *name)
{
    DWORD r = *glob<DWORD *>(G_LOG_FILE);
    if (r)
        r = fprintf_(r, glob_addr(0x613800), name);
    ++*glob<DWORD *>(G_LOG_DEPTH);
    return r;
}
// 0x4a2670 DebugLog_SectionEnd
DWORD __cdecl DebugLog_SectionEnd()
{
    const DWORD depth = --*glob<DWORD *>(G_LOG_DEPTH);
    const DWORD file = *glob<DWORD *>(G_LOG_FILE);
    if (!file)
        return file;
    return fprintf_(file, glob_addr(0x613800), depth ? glob<const char *>(0x613830 /* "section done" */) : glob<const char *>(0x613814 /* "END of all active section" */));
}

// ---- The frame rate counter --------------------------------------------------------------------------------

unsigned long long timer_counter() { return fn<unsigned long long(__cdecl *)()>(0x53d150)(); }
DWORD timer_frequency() { return fn<DWORD(__cdecl *)()>(0x53d170)(); }

DWORD clear_timer(BYTE *t)
{
    dw(t, 0xf8) = 0;
    dw(t, 0xf0) = 0;
    dw(t, 0x104) = 0;
    dw(t, 0xfc) = 0;
    dw(t, 0x100) = 0;
    dw(t, 0xf4) = 0;
    memset(t, 0, 0xf0);
    return dw(t, 0x108) = timer_frequency();
}
// 0x4a81f0: the constructor
BYTE *THISCALL FrameRate_Ctor(BYTE *t)
{
    clear_timer(t);
    return t;
}
// 0x4a8240: the reset (eax: the frequency)
DWORD THISCALL FrameRate_Reset(BYTE *t) { return clear_timer(t); }
// 0x4a83c0: the counter taken
void THISCALL FrameRate_Start(BYTE *t) { fl<unsigned long long>(t, 0xf8) = timer_counter(); }
// 0x4a8290: a sample taken (st0: the rate, see above)
long double THISCALL FrameRate_Sample(BYTE *t)
{
    const unsigned long long now = timer_counter();
    long double rate = fld(0.0f);
    if (fl<unsigned long long>(t, 0xf8)) {
        if (++dw(t, 0x104) > 30)
            dw(t, 0x104) = 30;
        DWORD delta = (DWORD)now - dw(t, 0xf8);
        if (delta > dw(t, 0x108))
            delta = dw(t, 0x108);
        unsigned long long &sum = fl<unsigned long long>(t, 0xf0);
        sum += delta;
        unsigned long long &sample = fl<unsigned long long>(t, dw(t, 0x100) * 8);
        sum -= sample;
        sample = delta;
        if (++dw(t, 0x100) >= 30)
            dw(t, 0x100) = 0;
        if (sum && dw(t, 0x104) >= 30)
            rate = fild64((long long)dw(t, 0x108)) * fild((int)dw(t, 0x104)) / fild64((long long)sum);
    }
    fl<unsigned long long>(t, 0xf8) = now;
    return rate;
}

// ---- The application base ----------------------------------------------------------------------------------

void *resource_cache() { return fn<void *(__cdecl *)()>(0x57aef0)(); }
void timer_start(DWORD timer) { fn<void(THISCALL *)(void *)>(0x4a83c0)(at<void *>(timer)); }
void music_update(BYTE *app) { fn<void(THISCALL *)(void *)>(0x4ce510)(app); }
void stream_volume(BYTE *app) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x587640)(bp(dw(app, 0x274)), dw(app, 0x64)); }

// 0x4cdcc0: the destructor (see above); eax: 0x479900's (a tail jump in the original)
DWORD THISCALL App_Dtor(BYTE *app)
{
    dw(app, 0) = vt(0x5f73e8);
    fn<void(__cdecl *)()>(0x4a1120)();   // Zip_UnmountAll
    if (void *w = bp(dw(app, 0x34))) {
        vcall<DWORD>(w, 0, 1);
        dw(app, 0x34) = 0;
    }
    if (BYTE *s = bp(dw(app, 0x274))) {
        fn<void(THISCALL *)(void *)>(0x4e2020)(s);
        heap_free(s);
        dw(app, 0x274) = 0;
    }
    if (void *d = bp(dw(app, 4))) {
        vcall<DWORD>(d, 1, 1);
        dw(app, 4) = 0;
    }
    fn<void(__cdecl *)()>(0x58ea40)();
    fn<void(__cdecl *)()>(0x42b6a0)();
    fn<void(THISCALL *)(void *)>(0x4a37a0)(fn<void *(__cdecl *)()>(0x4a35d0)());   // ResourceLists_CallAllReverse
    DWORD r = vcall<DWORD>(resource_cache(), 1);
    *glob<DWORD *>(0x6c02cc) = 0;   // the error reporter
    // the tail jump: 0x479900 sets al only, the rest of eax is the cache's slot 1's
    __asm__ volatile("call *%1" : "+a"(r) : "r"(code<void *>(0x479900)) : "ecx", "edx", "memory", "cc");
    return r;
}
// 0x4cdc50 (slot 0): the deleting destructor
BYTE *THISCALL App_DeletingDtor(BYTE *app, DWORD flags)
{
    fn<void(THISCALL *)(void *)>(0x4cdcc0)(app);
    if (flags & 1)
        heap_free(app);
    return app;
}
// 0x4cdf70 (slot 2, one unused argument): palettes and sounds at the start
DWORD THISCALL App_LoadPalettesAndSounds(BYTE *, DWORD)
{
    fn<void(__fastcall *)(const char *, const char *)>(0x433000)(glob<const char *>(0x615668) /* "Palettes" */, glob<const char *>(0x615674) /* "GamePalette" */);
    fn<void(THISCALL *)(void *, const char *, const char *, const char *, const char *)>(0x57e2f0)(
        glob<void *>(0x713708), glob<const char *>(0x615650) /* "Sounds" */, glob<const char *>(0x615650), glob<const char *>(0x615658) /* ".wav" */,
        glob<const char *>(0x615660) /* ".ogg" */);
    const DWORD r = fn<DWORD(__fastcall *)(void *, void *, const char *, DWORD)>(0x4a4670)(glob<void *>(0x6bd330), glob<void *>(0x6bd430),
                                                                                        glob<const char *>(0x61563c) /* "Pre-load all sounds" */, 0);
    if (!r)
        return r;
    return fn<DWORD(THISCALL *)(void *)>(0x57e590)(glob<void *>(0x713708));
}
// 0x4cdfd0 (slot 3: seconds): the frame (see above). The seconds are passed on as their bits.
void THISCALL App_Frame(BYTE *app, DWORD seconds)
{
    DWORD &t1 = *glob<DWORD *>(0x6c02d8), &t2 = *glob<DWORD *>(0x6c02e0);
    if (t1 < dw(app, 0x38)) {
        timer_start(glob_addr(0x6c01c0));
        t1 = dw(app, 0x38);
    }
    if (t2 < dw(app, 0x38)) {
        timer_start(glob_addr(0x6c00b0));
        t2 = dw(app, 0x38);
    }
    dw(app, 0x38)++;
    fn<void(THISCALL *)(void *)>(0x57af90)(resource_cache());   // the next stamp
    if (app[0x266]) {
        const long double t = fld(raw_float(seconds)) + fld(fl<float>(app, 0x270));
        fl<float>(app, 0x270) = fstp(t);
        if (!c0(t, fld(fl<float>(app, 0x26c)))) {
            if (app[0x268]) {
                stream_volume(app);
                char *next = reinterpret_cast<char *>(app + 0x167);
                if (*next) {
                    DWORD i = 0;
                    do
                        app[0x68 + i] = next[i];
                    while (next[i++]);
                } else
                    app[0x68] = 0;
                dw(app, 0x26c) = 0;
                dw(app, 0x270) = 0;
                app[0x266] = 0;
                app[0x268] = 0;
                app[0x267] = 1;
            } else {
                dw(app, 0x26c) = 0;
                dw(app, 0x270) = 0;
                app[0x266] = 0;
                app[0x267] = 0;
                app[0x268] = 0;
            }
            music_update(app);
        }
    }
    music_update(app);
    if (void *d = bp(dw(app, 4)))
        fn<void(THISCALL *)(void *, DWORD)>(0x567ab0)(d, seconds);
    if (void *w = bp(dw(app, 0x34)))
        vcall<DWORD>(w, 2, seconds);
    *glob<float *>(0x6c02d0) = fstp(fn<long double(THISCALL *)(void *)>(0x4a8290)(glob<void *>(0x6c01c0)));
}
// 0x4ce400 (slot 9: active)
DWORD THISCALL App_SetActive(BYTE *app, DWORD active)
{
    const BYTE a = (BYTE)active;
    if (app[0x60] == a)
        return 0;
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(0x6c1050), 1);   // Keys::ClearAll
    if (a) {
        vcall<DWORD>(resource_cache(), 4);
        vcall<DWORD>(resource_cache(), 8);
        fn<void(THISCALL *)(void *)>(0x5185e0)(glob<void *>(0x6cff40));
    } else {
        vcall<DWORD>(resource_cache(), 9);
        fn<void(THISCALL *)(void *)>(0x5185d0)(glob<void *>(0x6cff40));
    }
    app[0x60] = a;
    return 1;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(app_rest_b)
{
    const Hook list[] = {
#include "app_rest_b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
