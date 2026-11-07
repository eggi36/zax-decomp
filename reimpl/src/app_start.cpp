// The start of the game (phase 5a, group G2, docs/DECOMPILATION.md 10): WinMain 0x477940, the application start 0x5a12d0,
// the application object CZaxApp (constructor 0x59a670, destructor 0x59aa10, deleting destructor 0x59a9f0) and the
// application base's start-up 0x4cd780 (the constructor part of the world's base class that sets up the game: INI file,
// screen, archives, movies, music, tables, Local.txt). The same Windows calls as the original through the exe's import
// table; every exe function they reach is called through its exe address (Disable= of a hook keeps the original).
//
// These functions run only once, at the start (WinMain and everything it calls before its loop) or at the end (the
// destructors, from WM_DESTROY): the hooks are installed in DllMain while the process is still suspended, so a mistake
// shows as a game that does not start; each hook can be switched off on its own ([Reimpl] Disable=0x...).
//
// CZaxApp (0x5b4 bytes, vtable 0x602dc8, base: the world 0x4f0780) [confirmed: the constructor and destructor]: +0x338,
// +0x348, +0x358, +0x360, +0x361, +0x588, +0x589, +0x590, +0x591 flags; +0x33c an object of 0x50 bytes (vtable
// 0x601948), +0x340 an object deleted through slot 0, +0x344 the HUD slide (0x507f20, operator new 0x18), +0x34c the save
// data (CZaxSaveData, 0x34), +0x350 the save game statistics (CZaxSaveGameStats, 0x80), +0x354 an object of 0x18 bytes
// (vtable 0x5ed780), +0x364/+0x578/+0x57c ZStrings, +0x368, +0x58c (-1), +0x594, +0x5a4-+0x5b0. 0x713f14 g_App,
// 0x713f18 an object deleted with the application. The cheat names on 0x6bd2dc (0x46cd50: name, routine).
#include "ui_screens.h"
#include "standalone_launcher.h"

namespace zax::appstart {

using namespace ui;

constexpr DWORD WINDOW = 0x713e18, G_APP_OBJECT = 0x713f14, INI = 0x6bd330, INI_FILE = 0x6bd430, CURSOR = 0x6cff40;

inline void show_error_exit(DWORD caption, DWORD text)
{
    fn<void(__fastcall *)(DWORD, DWORD)>(0x4d24c0)(caption, text);
    fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
}
inline void section_begin(DWORD name) { fn<void(__fastcall *)(DWORD)>(0x4a2650)(name); }
inline void section_end() { fn<void(__cdecl *)()>(0x4a2670)(); }
inline DWORD ini_int(DWORD key, DWORD def) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4670)(glob_addr(INI), glob_addr(INI_FILE), key, def); }
inline DWORD new_ecx(DWORD n) { return fn<DWORD(__fastcall *)(DWORD)>(0x417710)(n); }
using Ctor = DWORD(THISCALL *)(DWORD);

// ---- WinMain --------------------------------------------------------------------------------------------------------

// The mutex "ZaxCurrentlyRunningMutex" (never looked at); the command line kept (0x4d2190, 0x713e08) and the instance
// (0x713e00); the window class "WinGameClass" (CS_DBLCLKS, the window procedure 0x599d50, icon 1, the arrow, a black
// brush) registered and the window (WS_EX_APPWINDOW, WS_POPUP | WS_VISIBLE | WS_SYSMENU, size 0, the title string 1)
// made, shown and focused ("FATAL ERROR" and exit(-1) when either fails); the joystick's and the wheel's registered
// messages; the exe's folder made the current directory; "TestDestinationReadibility.txt" created and deleted again
// (an "Error" box and 0 when the folder cannot be written); the application started (0x5a12d0), the window sized for
// windowed mode; then for ever: the messages pumped, the application told whether it runs (active and not minimized)
// and its steps run (0x4cdd80) while it runs or wants to run in the background (+0x28).
DWORD __stdcall WinMain(DWORD instance, DWORD previous, const char *command_line, DWORD show)   // 0x477940
{
    (void)previous;
    (void)show;
    imp<DWORD(WINAPI *)(void *, DWORD, const char *)>(0x5ea090)(nullptr, 0, glob<const char *>(0x6104cc));
    fn<void(__fastcall *)(const char *)>(0x4d2190)(command_line);
    gd(glob_addr(0x713e08)) = pv(command_line);
    gd(glob_addr(0x713e00)) = instance;
    DWORD wc[10];
    wc[0] = 8;
    wc[1] = fn_addr(0x599d50);
    wc[2] = 0;
    wc[3] = 0;
    wc[4] = instance;
    wc[5] = imp<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea208)(instance, 1);
    wc[6] = imp<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea204)(0, 0x7f00);
    wc[7] = imp<DWORD(WINAPI *)(int)>(0x5ea020)(4);
    wc[8] = 0;
    wc[9] = glob_addr(0x6104bc);
    if (!(WORD)imp<DWORD(WINAPI *)(const DWORD *)>(0x5ea200)(wc))
        show_error_exit(glob_addr(0x610490), glob_addr(0x61049c));   // "Can't register my game class"
    char title[0x34];
    imp<int(WINAPI *)(DWORD, DWORD, char *, int)>(0x5ea1fc)(instance, 1, title, 0x32);
    using CreateWindowExA_ = DWORD(WINAPI *)(DWORD, DWORD, const char *, DWORD, int, int, int, int, DWORD, DWORD, DWORD, void *);
    const DWORD window = imp<CreateWindowExA_>(0x5ea27c)(0x40000, glob_addr(0x6104bc), title, 0x90080000, 0, 0, 0, 0, 0, 0, instance, nullptr);
    gd(glob_addr(WINDOW)) = window;
    if (!window)
        show_error_exit(glob_addr(0x610490), glob_addr(0x610474));   // "Can't allocate my window"
    imp<DWORD(WINAPI *)(DWORD)>(0x5ea1f8)(window);
    imp<DWORD(WINAPI *)(DWORD)>(0x5ea1f4)(gd(glob_addr(WINDOW)));
    const auto register_message = imp<DWORD(WINAPI *)(DWORD)>(0x5ea1f0);
    gd(glob_addr(0x713e0c)) = register_message(glob_addr(0x61045c));   // "MSJSTICK_VJOYD_MSGSTR"
    gd(glob_addr(0x713e14)) = register_message(glob_addr(0x610394));   // "MSWHEEL_ROLLMSG"
    fn<DWORD(__cdecl *)()>(0x4d2310)();
    alignas(4) BYTE file[0x20];
    fn<void(THISCALL *)(void *)>(0x4a1780)(file);
    if (!(BYTE)fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4a1840)(file, glob_addr(0x61043c), 0x23)) {
        fn<void(__fastcall *)(DWORD, DWORD)>(0x4d2510)(glob_addr(0x6103a4), glob_addr(0x6103ac));
        fn<void(THISCALL *)(void *)>(0x4a1800)(file);
        return 0;
    }
    fn<void(THISCALL *)(void *)>(0x4a2100)(file);
    fn<void(__fastcall *)(DWORD, DWORD)>(0x4a14e0)(glob_addr(0x61043c), 0x20);
    fn<void(__cdecl *)()>(0x5a12d0)();
    const DWORD rep = gd(glob_addr(G_REPORTER));
    if (rep && m(bp(rep), 4) && !(BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x567cc0)(m(bp(rep), 4)))
        fn<void(__cdecl *)()>(0x477b60)();
    for (;;) {
        fn<void(__cdecl *)()>(0x5998f0)();
        const BYTE running = gb(glob_addr(0x713e10)) && !gb(glob_addr(0x713e11)) ? 1 : 0;
        DWORD app = gd(glob_addr(G_REPORTER));
        if (app) {
            fn<void(THISCALL *)(DWORD, DWORD)>(0x4cdeb0)(app, running);
            app = gd(glob_addr(G_REPORTER));
        }
        if (!running) {
            if (!app || !(BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x4cde80)(app))
                continue;
            app = gd(glob_addr(G_REPORTER));
        }
        if (!app)
            continue;
        fn<long double(THISCALL *)(DWORD)>(0x4cdd80)(app);
    }
}

// ---- The application base's start-up -------------------------------------------------------------------------------

// The launcher's /ShowFPS (Zax-Launcher.dll) patches this function's `setne cl` behind "Show Frame Rate" (0x4cdbf7) to
// `mov cl, 1`: the frame counter's flag as that code would set it
inline BYTE frame_rate_flag(DWORD value)
{
#ifdef ZAX_STANDALONE
    if (launcher::show_fps())   // the built-in launcher's /ShowFPS (standalone/launcher.cpp, batch 6-10c)
        return 1;
#else
    const BYTE *site = at<const BYTE *>(0x4cdbf7);
    if (site[0] == 0xb1 && site[2] == 0x90)
        return site[1];
#endif
    return value != 0;
}

// (three step lengths, the INI name, the screen image, the start music, the movies (null-terminated list)): the base's
// members; the class registry sorted; with "REMOVE" on the command line the uninstaller's start (the archives mounted,
// RemoveSw.exe, Remove.txt and DeleteMe.exe copied to the current folder, DeleteMe.exe started) and exit(0). Else: the
// reporter set, "Game Startup" in the debug log, the cursor hidden, "TestSave.dat" written and removed (whether the
// folder can be written), the INI file chosen by it (0x4a4610), the screen frame made (0x567990), the screen depth's
// default (16 bits on a CPU of class 6 with more than 64,000,000 bytes, else 8) asked from the INI and the command line
// (both answers unused), the archives mounted, the first movie that exists (unless /SkipMovies) switching the display to
// 640 x 480 x 16 for the movies, else the display set up from the INI (0x590ce0); the cursor hidden; the sound set up
// (the resource cache's slot 0, the listener's 1.0 and 5.0, the sound options), the music stream over "Music/" (wav,
// ogg); the movies played (the last one marked; the screen image waited for when 0x6c02d4); the start music played when
// the sound list knows it, and the music updated once more when the folder could be written (the original reads the
// same stack byte); srand(0x8787), the math and span tables, Local.txt; the flags; "Show Frame Rate" and "Show Overlays
// In Editor" of Zax.CFG.
BYTE *THISCALL Game_Startup(BYTE *self, DWORD a1, DWORD a2, DWORD a3, DWORD name, DWORD image, const char *music,
                            const DWORD *movies)   // 0x4cd780
{
    m(self, 0x40) = 0;
    m(self, 0) = vt(0x5f73e8);
    mb(self, 0x28) = 0;
    mb(self, 0x60) = 1;
    m(self, 0x2c) = 0;
    m(self, 0x44) = 0;
    fn<void(THISCALL *)(DWORD)>(0x415d80)(fn<DWORD(__cdecl *)()>(0x415d20)());
    m(self, 0x48) = 0;
    mb(self, 0x268) = 0;
    mb(self, 0x266) = 0;
    m(self, 0x24) = 0;
    m(self, 0x4c) = 0;
    m(self, 0x50) = 0;
    m(self, 0x54) = a1;
    m(self, 0x58) = a2;
    m(self, 0x5c) = a3;
    using Mount = void(__fastcall *)(DWORD, DWORD, DWORD);
    using CurrentDirectory = DWORD(__fastcall *)(char *, DWORD);
    using Append = void(__fastcall *)(char *, DWORD);
    using Copy = DWORD(__fastcall *)(DWORD, DWORD, const char *, DWORD, DWORD);
    if (fn<DWORD(__cdecl *)(DWORD, DWORD)>(0x5d1030)(gd(glob_addr(0x713e08)), glob_addr(0x615634))) {   // "REMOVE"
        char remover[0x100], deleter[0x100];   // in the temporary folder (0x4d2300: GetTempPathA)
        fn<Mount>(0x4a0c40)(glob_addr(0x61557c), 0, 0);
        fn<CurrentDirectory>(0x4d2300)(remover, 0x100);
        fn<Append>(0x4a44f0)(remover, glob_addr(0x61556c));   // "RemoveSw.exe"
        if ((BYTE)fn<Copy>(0x4a1ea0)(glob_addr(0x61556c), 0, remover, 0x20, 0)) {
            fn<Copy>(0x4a1ea0)(glob_addr(0x615560), 0, glob<const char *>(0x615560), 0x20, 0);   // "Remove.txt"
            fn<CurrentDirectory>(0x4d2300)(deleter, 0x100);
            fn<Append>(0x4a44f0)(deleter, glob_addr(0x615550));   // "DeleteMe.exe"
            fn<Copy>(0x4a1ea0)(glob_addr(0x615550), 0, deleter, 0x20, 0);
            fn<void(__fastcall *)(const char *, const char *, DWORD)>(0x4d2570)(deleter, remover, 1);
        }
        fn<void(__cdecl *)(int)>(0x5d0e36)(0);
    }
    m(self, 0x34) = 0;
    m(self, 0x30) = 0;
    m(self, 4) = 0;
    gd(glob_addr(G_REPORTER)) = pv(self);
    section_begin(glob_addr(0x615624));   // "Game Startup"
    fn<void(THISCALL *)(DWORD)>(0x5185f0)(glob_addr(CURSOR));
    BYTE writable;
    if (const DWORD f = fn<DWORD(__cdecl *)(DWORD, DWORD)>(0x5d1a29)(glob_addr(0x615614), glob_addr(0x613848))) {   // "TestSave.dat", "wb"
        fn<int(__cdecl *)(DWORD)>(0x5d1808)(f);
        fn<int(__cdecl *)(DWORD)>(0x5d17a6)(glob_addr(0x615614));
        writable = 1;
    } else {
        writable = 0;
    }
    {
        DWORD ini_name;
        const DWORD z = fn<DWORD(__fastcall *)(DWORD *, DWORD, DWORD)>(0x4a4610)(&ini_name, name, writable);
        fn<void(THISCALL *)(DWORD, const char *)>(0x46ec00)(glob_addr(INI), c_str(z));
        zs_dtor(&ini_name);
    }
    const DWORD frame = new_ecx(0xe8);
    m(self, 4) = frame ? fn<Ctor>(0x567990)(frame) : 0;
    const int cpu = (int)fn<DWORD(__cdecl *)()>(0x515620)();
    DWORD depth = 8;
    if (cpu > 0 && cpu > 5 && cpu == 6 && fn<DWORD(__cdecl *)()>(0x4d2490)() > 0x3d09000)
        depth = 0x10;
    fn<DWORD(__fastcall *)(DWORD, DWORD)>(0x4d2220)(glob_addr(0x6155f0), ini_int(glob_addr(0x615600), depth));   // "/ScreenBitDepth", "Screen Bit Depth"
    fn<Mount>(0x4a0c40)(glob_addr(0x61557c), 0, 0);   // "Data"
    BYTE movie = 0;
    if (!(BYTE)fn<DWORD(__fastcall *)(DWORD)>(0x4d22c0)(glob_addr(0x6155a8)) && movies) {   // "/SkipMovies"
        for (const DWORD *p = movies; *p; p++) {
            DWORD path;
            zs_ctor(&path);
            zs_format(&path, glob<const char *>(0x615598), *p);   // "Movies/%s.bik"
            if ((BYTE)fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1570)(c_str(&path), 0)) {
                movie = 1;
                zs_dtor(&path);
                break;
            }
            zs_dtor(&path);
        }
    }
    using DisplayInit = void(__fastcall *)(DWORD, DWORD, DWORD);
    if (movie)
        fn<DisplayInit>(0x590ce0)(0x10, 0x280, 0x1e0);
    else
        fn<DisplayInit>(0x590ce0)(0, 0, 0);
    fn<void(THISCALL *)(DWORD)>(0x5185f0)(glob_addr(CURSOR));
    using CacheGet = DWORD(__cdecl *)();
    vc(bp(fn<CacheGet>(0x57aef0)()), 0);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x57c1f0)(fn<CacheGet>(0x57aef0)(), 0x3f800000);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x57c210)(fn<CacheGet>(0x57aef0)(), 0x40a00000);
    fn<void(__cdecl *)()>(0x57ee90)();
    alignas(4) BYTE spec[0x2c];
    fn<void(THISCALL *)(void *)>(0x4a09f0)(spec);
    fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4a0a60)(spec, glob_addr(0x61380c), glob_addr(0x6155ec));   // "Music/", "wav"
    fn<void(THISCALL *)(void *, DWORD)>(0x4a0a30)(spec, glob_addr(0x6155e8));             // "ogg"
    const DWORD stream = pv(op_new(0xc));
    m(self, 0x274) = stream ? fn<DWORD(THISCALL *)(DWORD, void *, DWORD)>(0x4e1fe0)(stream, spec, 1) : 0;
    if (movie) {
        for (const DWORD *p = movies; *p; p++)
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4cd5a0)(self, *p, image, (p[1] & 0xffffff00) | (p[1] == 0));
        if (gb(glob_addr(0x6c02d4)))
            fn<void(THISCALL *)(DWORD)>(0x578eb0)(image);
    }
    if (music && (int)fn<DWORD(THISCALL *)(DWORD, const char *, DWORD)>(0x4e2030)(glob_addr(0x6c0f10), music, (DWORD)-1) != -1) {
        DWORD n = 0;
        while (music[n])
            n++;
        memcpy(self + 0x68, music, n + 1);
        m(self, 0x26c) = 0;
        m(self, 0x270) = 0;
        mb(self, 0x266) = 0;
        mb(self, 0x268) = 0;
        mb(self, 0x267) = 1;
        fn<void(THISCALL *)(BYTE *)>(0x4ce510)(self);
    }
    if (writable)
        fn<void(THISCALL *)(BYTE *)>(0x4ce510)(self);
    fn<void(__cdecl *)(DWORD)>(0x5d218b)(0x8787);
    fn<void(__cdecl *)()>(0x508cb0)();
    fn<void(__cdecl *)()>(0x42b320)();
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x500ac0)(glob_addr(0x6c2b90), glob_addr(0x6155dc), 1);   // "Local.txt"
    mb(self, 0x19) = 0;
    mb(self, 0x1a) = 0;
    mb(self, 0x1d) = 1;
    m(self, 0x20) = 0;
    m(self, 0x38) = 0;
    m(self, 0x3c) = 0;
    mb(self, 0x1b) = 0;
    mb(self, 0x1b) = frame_rate_flag(ini_int(glob_addr(0x6155cc), 0));   // "Show Frame Rate"
    mb(self, 0x1c) = 1;
    mb(self, 0x1c) = ini_int(glob_addr(0x6155b4), 1) != 0;   // "Show Overlays In Editor"
    section_end();
    for (DWORD off = 0x28; off >= 0x18; off -= 4)   // the file spec's five strings
        zs_dtor(spec + off);
    return self;
}

// ---- The application object ------------------------------------------------------------------------------------------

struct Cheat {
    GlobKey name;
    CodeKey routine;
};
constexpr Cheat CHEATS[19] = {
    {0x6249a8, 0x5a2250}, {0x6249a0, 0x5a2270}, {0x624998, 0x5a2270}, {0x624994, 0x5a2630}, {0x62498c, 0x5a2630},
    {0x624980, 0x5a2670}, {0x624978, 0x5a2500}, {0x62496c, 0x5a2500}, {0x624964, 0x5a22b0}, {0x62495c, 0x5a22b0},
    {0x624954, 0x5a2540}, {0x624948, 0x5a2590}, {0x624940, 0x5a26a0}, {0x624938, 0x59a600}, {0x624930, 0x59a600},
    {0x624928, 0x58ea40}, {0x62491c, 0x58ea40}, {0x624914, 0x58ea40}, {0x624908, 0x58ea40},
};

// (the screen image): the world (0x4f0780 with "Zax"), the strings, the members, two small objects, the cheat names
// ("medic" ... "show map") registered on 0x6bd2dc, g_App, the save data, the save game statistics and the HUD slide
BYTE *THISCALL CZaxApp_Ctor(BYTE *self, DWORD image)   // 0x59a670
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4f0780)(self, image, glob_addr(0x61be18), glob_addr(0x624860));
    zs_ctor(self + 0x364);
    zs_ctor(self + 0x578);
    zs_ctor(self + 0x57c);
    m(self, 0) = vt(0x602dc8);
    fn<void(THISCALL *)(BYTE *)>(0x4df2b0)(self + 0x364);
    m(self, 0x368) = 0;
    mb(self, 0x591) = 0;
    m(self, 0x58c) = 0xffffffff;
    mb(self, 0x589) = 0;
    DWORD o = new_ecx(0x50);
    if (o) {
        fn<void(THISCALL *)(DWORD)>(0x416610)(o);
        BYTE *p = bp(o);
        m(p, 8) = vt(0x6019b0);
        m(p, 0xc) = 0;
        m(p, 0x10) = 0;
        m(p, 0x14) = 0;
        m(p, 0x20) = vt(0x6019bc);
        m(p, 0x24) = 0;
        m(p, 0x28) = 0;
        m(p, 0x2c) = 0;
        m(p, 0x30) = vt(0x6019bc);
        m(p, 0x34) = 0;
        m(p, 0x38) = 0;
        m(p, 0x3c) = 0;
        m(p, 0x40) = vt(0x6019a4);
        m(p, 0x44) = 0;
        m(p, 0x48) = 0;
        m(p, 0x4c) = 0;
        m(p, 0) = vt(0x601948);
        mb(p, 0x18) = 0;
        mb(p, 0x19) = 0;
        fn<void(THISCALL *)(DWORD)>(0x4173c0)(o);
    }
    m(self, 0x33c) = o;
    mb(self, 0x360) = 0;
    gd(glob_addr(0x713f18)) = 0;
    mb(self, 0x358) = 0;
    for (const Cheat &c : CHEATS)
        fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x46cd50)(glob_addr(0x6bd2dc), key_glob_addr(c.name), key_fn_addr(c.routine));
    mb(self, 0x588) = 0;
    o = new_ecx(0x18);
    if (o) {
        fn<void(THISCALL *)(DWORD)>(0x416610)(o);
        BYTE *p = bp(o);
        m(p, 0xc) = 0;
        m(p, 0x10) = 0;
        m(p, 0x14) = 0;
        m(p, 8) = vt(0x5ed7dc);
        m(p, 0) = vt(0x5ed780);
        fn<void(THISCALL *)(DWORD)>(0x4173c0)(o);
    }
    m(self, 0x354) = o;
    mb(self, 0x590) = 0;
    gd(glob_addr(G_APP_OBJECT)) = pv(self);
    mb(self, 0x348) = 0;
    o = new_ecx(0x34);
    m(self, 0x34c) = o ? fn<Ctor>(0x5bb6c0)(o) : 0;
    o = new_ecx(0x80);
    m(self, 0x350) = o ? fn<Ctor>(0x5bc230)(o) : 0;
    o = pv(op_new(0x18));
    // the constructor's eax is 0 (its `xor eax, eax`); the original keeps the block in edi and stores that (0x59a985;
    // batch 6-13: the result of the void HudSlide_Ctor was stored, right only while eax still held the block)
    if (o)
        fn<void(THISCALL *)(DWORD)>(0x507f20)(o);
    m(self, 0x344) = o;
    mb(self, 0x361) = 0;
    mb(self, 0x338) = 0;
    m(self, 0x5a4) = 0;
    m(self, 0x5a8) = 0;
    m(self, 0x5ac) = 0;
    m(self, 0x5b0) = 0;
    m(self, 0x594) = 0;
    return self;
}

// The voice released (0x595b50, 0x595ac0); the members deleted (slot 1 or slot 0 with 1) and cleared in the original's
// order, 0x713f18 too, the HUD slide freed; the strings and the world destroyed
void THISCALL CZaxApp_Dtor(BYTE *self)   // 0x59aa10
{
    m(self, 0) = vt(0x602dc8);
    fn<void(THISCALL *)(DWORD)>(0x595ac0)(fn<DWORD(__cdecl *)()>(0x595b50)());
    if (const DWORD o = m(self, 0x354)) {
        vc(bp(o), 1, 1);
        m(self, 0x354) = 0;
    }
    if (const DWORD o = m(self, 0x340)) {
        vc(bp(o), 0, 1);
        m(self, 0x340) = 0;
    }
    if (const DWORD o = m(self, 0x34c)) {
        vc(bp(o), 1, 1);
        m(self, 0x34c) = 0;
    }
    if (const DWORD o = m(self, 0x350)) {
        vc(bp(o), 1, 1);
        m(self, 0x350) = 0;
    }
    if (const DWORD o = m(self, 0x33c)) {
        vc(bp(o), 1, 1);
        m(self, 0x33c) = 0;
    }
    if (const DWORD o = gd(glob_addr(0x713f18))) {
        vc(bp(o), 0, 1);
        gd(glob_addr(0x713f18)) = 0;
    }
    if (const DWORD o = m(self, 0x344)) {
        op_delete(bp(o));
        m(self, 0x344) = 0;
    }
    zs_dtor(self + 0x57c);
    zs_dtor(self + 0x578);
    zs_dtor(self + 0x364);
    fn<void(THISCALL *)(BYTE *)>(0x4f0c70)(self);
}

BYTE *THISCALL CZaxApp_DeletingDtor(BYTE *self, DWORD flags)   // 0x59a9f0
{
    fn<void(THISCALL *)(BYTE *)>(0x59aa10)(self);
    if (flags & 1)
        op_delete(self);
    return self;
}

// ---- The application start ------------------------------------------------------------------------------------------

// A screen image (0x578910) for the start; the application object made into g_App; its game data loaded (slot 2 with the
// image) inside the debug log's "Load Game Data"; every resource list marked; the image cleared
void __cdecl App_Start()   // 0x5a12d0
{
    alignas(4) BYTE image[0x34];
    fn<void(THISCALL *)(void *)>(0x578910)(image);
    const DWORD p = pv(op_new(0x5b4));
    gd(glob_addr(G_APP_OBJECT)) = p ? fn<DWORD(THISCALL *)(DWORD, void *)>(0x59a670)(p, image) : 0;
    section_begin(glob_addr(0x624f30));   // "Load Game Data"
    vc(bp(gd(glob_addr(G_APP_OBJECT))), 2, image);
    section_end();
    fn<void(THISCALL *)(DWORD)>(0x4a37d0)(fn<DWORD(__cdecl *)()>(0x4a35d0)());
    fn<void(THISCALL *)(void *)>(0x578930)(image);
}

}  // namespace zax::appstart

namespace zax {

using namespace appstart;

ZAX_HOOKS(app_start)
{
    const Hook list[] = {
        {0x477940, ptr(&WinMain), "WinMain", 9, {0x83, 0xec, 0x7c, 0x56, 0x68, 0xcc, 0x04, 0x61}},
        {0x4cd780, ptr(&Game_Startup), "Game_Startup", 6, {0x81, 0xec, 0x38, 0x02, 0x00, 0x00, 0x53, 0x55}},
        {0x59a670, ptr(&CZaxApp_Ctor), "CZaxApp_Ctor", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x56, 0x57, 0x68}},
        {0x59a9f0, ptr(&CZaxApp_DeletingDtor), "CZaxApp_DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x18, 0x00, 0x00, 0x00}, 4},
        {0x59aa10, ptr(&CZaxApp_Dtor), "CZaxApp_Dtor", 10, {0x56, 0x8b, 0xf1, 0x57, 0xc7, 0x06, 0xc8, 0x2d}},
        {0x5a12d0, ptr(&App_Start), "App_Start", 7, {0x83, 0xec, 0x34, 0x8d, 0x4c, 0x24, 0x00, 0xe8}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
