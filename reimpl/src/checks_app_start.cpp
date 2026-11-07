// Checks of the start of the game (app_start.cpp) against the original, in the check thread with every exe function the
// start reaches as a recording stand-in (checks_ui_common.h) and the Windows imports as recording stand-ins
// (checks_sys_common.h): WinMain until the third round of its loop (the pump's stand-in leaves the side then, like exit),
// with a failing RegisterClassA, CreateWindowExA and test file; the application base's start-up (the uninstaller's start
// with "REMOVE" on the command line, the INI, the screen depth by the CPU class and the memory, the movies with and
// without /SkipMovies and with movies that do not exist, the start music known or not, the folder writable or not, the
// launcher's /ShowFPS patch); the application object's constructor, destructor and deleting destructor (with members
// present and missing); the application start. Objects the functions build on their own stack (strings, the file spec,
// the test file, the screen image) are logged without their address and contents. Compared: eax, the call log, the
// block of fakes (the application object lies in it) and the globals.
// The real start (the window, DirectDraw, the files) is compared by the logged start (api_trace.cpp, TraceApi=1)
// against /Original.
#include "checks_sys_common.h"

extern "C" DWORD uic_edx;

namespace zax {

int hooks_app_start(Hook *out);

namespace {

using namespace sysc;

constinit ModuleHooks g_hooks{"app_start", &hooks_app_start};
Globals g_globals;

constexpr DWORD REPORTER = 0x6c02cc, G_APP = 0x713f14;

// The "game object" class's arguments for the fakes here: slots 0, 1, 2 and 9 take one (deleting destructors, the game
// data, the game runs); with START_ARGS slot 0 takes none (the resource cache's slot 0 in the start-up)
const char APP_ARGS[] = "1110000001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
const char START_ARGS[] = "0110000001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

// ---- Import stand-ins (WinMain) -----------------------------------------------------------------------------------------

DWORD WINAPI i_mutex(void *sec, DWORD owner, const char *name)
{
    rec(0xa090, ptrv(sec), owner, text_hash(name));
    return 0x30000 | ans_below(0x100);
}
DWORD WINAPI i_load_icon(DWORD inst, DWORD id)
{
    rec(0xa208, inst, id);
    return 0x1c0000 | ans_below(0x100);
}
DWORD WINAPI i_load_cursor(DWORD inst, DWORD id)
{
    rec(0xa204, inst, id);
    return 0xc0c00000 | ans_below(0x100);
}
DWORD WINAPI i_stock(int what)
{
    rec(0xa020, (DWORD)what);
    return 0xb00000 | ans_below(0x100);
}
DWORD WINAPI i_register_class(const DWORD *wc)
{
    rec(0xa200, wc[0], wc[1], wc[2] ^ wc[3] << 8, wc[4]);
    rec(0xa2001, wc[5], wc[6], wc[7], wc[8]);
    log_event(0xa2002, text_hash(at<const char *>(wc[9])), 0);
    return ans_chance(15) ? 0x30000 : 0xc000 | ans_below(0x100);   // (0x30000: the atom's word 0, a failure)
}
int WINAPI i_load_string(DWORD inst, DWORD id, char *buf, int n)
{
    rec(0xa1fc, inst, id, (DWORD)n);
    const char *t = ans_chance(5) ? "" : "Zax: The Alien Hunter";
    lstrcpyA(buf, t);
    return lstrlenA(t);
}
DWORD WINAPI i_create_window(DWORD ex, const char *cls, const char *title, DWORD style, int x, int y, int w, int h, DWORD parent,
                             DWORD menu, DWORD inst, void *param)
{
    rec(0xa27c, ex, text_hash(cls), text_hash(title), style);
    rec(0xa27c1, (DWORD)x ^ (DWORD)y << 8 ^ (DWORD)w << 16 ^ (DWORD)h << 24, parent, menu, inst ^ ptrv(param));
    return ans_chance(15) ? 0 : 0x77770000 | ans_below(0x100);
}
DWORD WINAPI i_update_window(DWORD w)
{
    rec(0xa1f8, w);
    return 1;
}
DWORD WINAPI i_set_focus(DWORD w)
{
    rec(0xa1f4, w);
    return ans();
}
DWORD WINAPI i_register_message(const char *name)
{
    rec(0xa1f0, text_hash(name));
    return 0xc100 | ans_below(0x100);
}
DWORD WINAPI i_current_directory(DWORD n, char *buf)   // (Platform_CurrentDirectory of the uninstaller's start: GetTempPathA)
{
    rec(0xa0b8, n);
    lstrcpyA(buf, ans_chance(2) ? "C:\\Games\\Zax" : "D:\\");
    return (DWORD)lstrlenA(buf);
}

const Imp IMPORTS[] = {{0x5ea090, (void *)&i_mutex},          {0x5ea208, (void *)&i_load_icon},     {0x5ea204, (void *)&i_load_cursor},
                       {0x5ea020, (void *)&i_stock},          {0x5ea200, (void *)&i_register_class}, {0x5ea1fc, (void *)&i_load_string},
                       {0x5ea27c, (void *)&i_create_window},  {0x5ea1f8, (void *)&i_update_window}, {0x5ea1f4, (void *)&i_set_focus},
                       {0x5ea1f0, (void *)&i_register_message}, {0x5ea0b8, (void *)&i_current_directory}};

// ---- Exe stand-ins ------------------------------------------------------------------------------------------------------

// WinMain
const Stub STUBS_WINMAIN[] = {
    {0x4d2190, CV_FAST, 0, K_VALUE, "CommandLine_Set", "sx"},
    {0x4d24c0, CV_FAST, 0, K_VALUE, "ShowErrorBox", "ss"},
    {0x4d2310, CV_CDECL, 0, K_VALUE, "Platform_SetExeDirectory"},
    {0x4a1780, CV_FAST, 0, K_VALUE, "FileBuf_Ctor", "xx"},
    {0x4a1840, CV_FAST, 2, K_FIXED0, "FileBuf_Open", "xxsv"},
    {0x4d2510, CV_FAST, 0, K_VALUE, "Platform_MessageBox", "ss"},
    {0x4a1800, CV_FAST, 0, K_VALUE, "FileBuf_Dtor", "xx"},
    {0x4a2100, CV_FAST, 0, K_VALUE, "FileBuf_Close", "xx"},
    {0x4a14e0, CV_FAST, 0, K_VALUE, "File_Delete", "sv"},
    {0x5a12d0, CV_CDECL, 0, K_VALUE, "App_Start"},
    {0x567cc0, CV_THIS, 0, K_BOOL, "ScreenFrame_IsFullScreen"},
    {0x477b60, CV_CDECL, 0, K_VALUE, "App_SetWindowedSize"},
    {0x5998f0, CV_CDECL, 0, K_VALUE, "App_PumpMessages"},
    {0x4cdeb0, CV_THIS, 1, K_VALUE, "App::SetFlag29", "b"},
    {0x4cdd80, CV_THIS, 0, K_FLOAT, "App::RunSteps"},
};
// The application base's start-up
const Stub STUBS_STARTUP[] = {
    {0x415d20, CV_CDECL, 0, K_VALUE, "ClassRegistry_Get"},
    {0x415d80, CV_THIS, 0, K_VALUE, "ClassRegistry_Sort"},
    {0x4a0c40, CV_FAST, 1, K_VALUE, "Zip_MountAll", "svv"},
    {0x4a1ea0, CV_FAST, 3, K_BOOL, "File_Copy", "svsvv"},
    {0x4d2570, CV_FAST, 1, K_VALUE, "Platform_StartProcess", "ssv"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x5185f0, CV_THIS, 0, K_VALUE, "Cursor::Hide"},
    {0x5d1a29, CV_CDECL, 2, K_FIXED1, "fopen", "ss"},
    {0x5d1808, CV_CDECL, 1, K_VALUE, "fclose"},
    {0x5d17a6, CV_CDECL, 1, K_VALUE, "remove", "s"},
    {0x4a4610, CV_FAST, 1, K_VALUE, "Ini_FileName", "xsb"},
    {0x4e13a0, CV_FAST, 0, K_TEXT, "ZString_c_str", "xx"},
    {0x46ec00, CV_THIS, 1, K_VALUE, "IniFile::SetFile", "s"},
    {0x4defd0, CV_FAST, 0, K_VALUE, "ZString_Dtor", "xx"},
    {0x4dec90, CV_FAST, 0, K_VALUE, "ZString_Ctor", "xx"},
    {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "xss"},
    {0x417710, CV_FAST, 0, 'n', "operator_new_ecx", "vx"},
    {0x5d033a, CV_CDECL, 1, 'n', "operator new"},
    {0x567990, CV_THIS, 0, 'e', "CScreenFrame::CScreenFrame"},
    {0x515620, CV_CDECL, 0, K_FIXED2, "Cpu_Class"},
    {0x4d2490, CV_CDECL, 0, K_FIXED3, "Platform_PhysicalMemory"},
    {0x4a4670, CV_FAST, 2, K_VALUE, "Ini_GetInt", "vvsv"},
    {0x4d2220, CV_FAST, 0, K_VALUE, "CommandLine_Int", "sv"},
    {0x4d22c0, CV_FAST, 0, K_FIXED4, "CommandLine_Has", "sx"},
    {0x4a1570, CV_FAST, 0, K_BOOL, "File_CanOpen", "sv"},
    {0x590ce0, CV_FAST, 1, K_VALUE, "Display_Init", "vvv"},
    {0x57aef0, CV_CDECL, 0, K_FIXED5, "ResourceCache_Get"},
    {0x57c1f0, CV_THIS, 1, K_VALUE, "Listener_SetDistanceFactor"},
    {0x57c210, CV_THIS, 1, K_VALUE, "Listener_Set713698"},
    {0x57ee90, CV_CDECL, 0, K_VALUE, "Sound_LoadOptions"},
    {0x4a09f0, CV_FAST, 0, K_VALUE, "FileSpec_Ctor", "xx"},
    {0x4a0a60, CV_FAST, 2, K_VALUE, "FileSpec_Set", "xxss"},
    {0x4a0a30, CV_FAST, 1, K_VALUE, "FileSpec_AddExtension", "xxs"},
    {0x4e1fe0, CV_THIS, 2, 'e', "SoundStream_Ctor", "xv"},
    {0x4cd5a0, CV_THIS, 3, K_VALUE, "App_PlayMovie", "svv"},
    {0x578eb0, CV_THIS, 0, K_VALUE, "ScreenImage::WaitAndHide"},
    {0x4e2030, CV_THIS, 2, K_FIXED6, "SoundFileList_Find", "sv"},
    {0x4ce510, CV_THIS, 0, K_VALUE, "App_UpdateMusic"},
    {0x5d218b, CV_CDECL, 1, K_VALUE, "srand"},
    {0x508cb0, CV_CDECL, 0, K_VALUE, "Tables_Init"},
    {0x42b320, CV_CDECL, 0, K_VALUE, "SpanTables_Init"},
    {0x500ac0, CV_THIS, 2, K_VALUE, "LocalTable_Load", "sv"},
};
// The application object and the application start
const Stub STUBS_APP[] = {
    {0x4f0780, CV_THIS, 3, K_VALUE, "World::Ctor", "vsv"},
    {0x4dec90, CV_THIS, 0, 'e', "ZString_Ctor"},
    {0x4df2b0, CV_THIS, 0, K_VALUE, "ZString_Clear"},
    {0x4defd0, CV_THIS, 0, K_VALUE, "ZString_Dtor"},
    {0x417710, CV_FAST, 0, 'n', "operator_new_ecx", "vx"},
    {0x5d033a, CV_CDECL, 1, 'n', "operator new"},
    {0x5d0320, CV_CDECL, 1, K_VALUE, "operator delete"},
    {0x416610, CV_THIS, 0, 'e', "CObject_Ctor"},
    {0x4173c0, CV_THIS, 0, K_VALUE, "Object_InitFields"},
    {0x46cd50, CV_THIS, 2, K_VALUE, "Cheats::Register", "sv"},
    {0x5bb6c0, CV_THIS, 0, 'e', "CZaxSaveData::Ctor"},
    {0x5bc230, CV_THIS, 0, 'e', "CZaxSaveGameStats::Ctor"},
    {0x507f20, CV_THIS, 0, K_VALUE, "HudSlide::Ctor"},   // eax 0 in the original, not ecx: the caller keeps edi (6-13)
    {0x595b50, CV_CDECL, 0, K_VALUE, "Voice_Get"},
    {0x595ac0, CV_THIS, 0, K_VALUE, "Voice_Release"},
    {0x4f0c70, CV_THIS, 0, K_VALUE, "World::Dtor"},
    {0x578910, CV_FAST, 0, K_VALUE, "ScreenImage_Init", "xx"},
    {0x578930, CV_FAST, 0, K_VALUE, "ScreenImage::Clear", "xx"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x4a35d0, CV_CDECL, 0, K_VALUE, "ResourceLists_Get"},
    {0x4a37d0, CV_THIS, 0, K_VALUE, "ResourceLists_MarkAll"},
};
// App_Start: the constructor a stand-in too
const Stub STUBS_APP_START[] = {
    {0x59a670, CV_THIS, 1, 'e', "CZaxApp_Ctor", "x"},
    {0x5d033a, CV_CDECL, 1, 'n', "operator new"},
    {0x578910, CV_FAST, 0, K_VALUE, "ScreenImage_Init", "xx"},
    {0x578930, CV_FAST, 0, K_VALUE, "ScreenImage::Clear", "xx"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x4a35d0, CV_CDECL, 0, K_VALUE, "ResourceLists_Get"},
    {0x4a37d0, CV_THIS, 0, K_VALUE, "ResourceLists_MarkAll"},
};

// ---- The cases --------------------------------------------------------------------------------------------------------

enum TestId { T_WINMAIN, T_STARTUP, T_CTOR, T_DTOR, T_DELETING_DTOR, T_APP_START };
int g_test;
constexpr int W_APP = 0, W_FRAME = 1, W_CACHE = 2, W_M1 = 3;   // W_M1..: members
BYTE *g_self;
DWORD g_a[8];
int g_pumps, g_pump_limit;

void effect(int cls, int slot, DWORD *, DWORD)
{
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x5998f0:   // the third pump leaves WinMain (its loop runs for ever)
        if (++g_pumps >= g_pump_limit)
            zax_escape_exit(0x7e01);
        break;
    case 0x59a670:   // the new application object answers through the fake application's vtable
        at_(at<BYTE *>(g_effect_ecx), 0) = at_(win(W_APP), 0);
        break;
    default: break;
    }
}

BYTE g_show_fps_site[3];

void build(Gen &g)
{
    note_stack();
    g_case.effect = &effect;
    g_case.nwin = 8;
    g_case.object_args = g_test == T_STARTUP ? START_ARGS : APP_ARGS;
    g_case.argspec[2][0] = A_IGNORE;   // (the application's slot 2: the screen image on the caller's stack)
    for (int i = 0; i < 8; i++)
        make_window(i, i == W_FRAME ? C_CFRAME : C_OBJECT);
    g_case.fixed[5] = ptrv(win(W_CACHE));
    case_global(REPORTER, g.chance(4) ? 0 : ptrv(win(W_APP)));
    case_global(G_APP, g.chance(2) ? 0 : ptrv(win(W_APP)));
    case_global(0x713f18, g.chance(3) ? 0 : ptrv(win(W_M1 + 4)));
    case_global(0x713e00, 0);
    case_global(0x713e08, 0);
    case_global(0x713e0c, 0);
    case_global(0x713e14, 0);
    case_global(0x713e18, 0);
    case_global(0x713e10, g.below(2));
    case_global(0x713e11, g.chance(3) ? 1 : 0);
    case_global(0x6c02d4, g.below(2));
    at_(win(W_APP), 4) = g.chance(4) ? 0 : ptrv(win(W_FRAME));
    win(W_APP)[0x28] = (BYTE)g.below(2);
    g_pumps = 0;
    g_pump_limit = 1 + (int)g.below(4);
    memset(g_a, 0, sizeof g_a);
    g_self = nullptr;
    switch (g_test) {
    case T_WINMAIN: {
        g_a[0] = 0x400000;
        g_a[1] = 0;
        const char *lines[3] = {"", "/Map \"Test/Nachbau1\" /SkipMovies", "REMOVE"};
        char *line = reinterpret_cast<char *>(xalloc(64));
        lstrcpyA(line, lines[g.below(3)]);
        g_a[2] = ptrv(line);
        g_a[3] = 1;
        g_case.fixed[0] = g.chance(6) ? 0 : 1;   // the test file can be written
        break;
    }
    case T_STARTUP: {
        g_self = xalloc(0x5b4);
        for (DWORD o = 0; o < 0x5b4; o += 4)
            at_(g_self, o) = g.next();
        g_a[0] = 0x3c360b61, g_a[1] = 0x3d088889, g_a[2] = 0x3e2aaaab;
        g_a[3] = ptrv(TEXTS[6]);
        g_a[4] = ptrv(win(W_M1));   // the screen image
        const char *music[4] = {"Startup.wav", "", "Music/Title.ogg", nullptr};
        g_a[5] = ptrv(music[g.below(4)]);
        DWORD *movies = reinterpret_cast<DWORD *>(xalloc(32));
        const DWORD n = g.below(4);
        const char *names[4] = {"Reflexive", "JoWooD", "Intro", "Missing"};
        for (DWORD i = 0; i < n; i++)
            movies[i] = ptrv(names[g.below(4)]);
        movies[n] = 0;
        g_a[6] = g.chance(5) ? 0 : ptrv(movies);
        char *line = reinterpret_cast<char *>(xalloc(64));
        lstrcpyA(line, g.chance(6) ? "/REMOVE" : g.chance(2) ? "/Map x /SkipMovies" : "");
        case_global(0x713e08, ptrv(line));
        g_case.fixed[1] = g.chance(4) ? 0 : 0x55aa00;               // fopen
        g_case.fixed[2] = g.chance(2) ? 6 : g.below(8);              // the CPU class
        g_case.fixed[3] = g.chance(2) ? 0x3d09001 : g.next();        // the memory
        g_case.fixed[4] = g.below(2);                                // /SkipMovies
        g_case.fixed[6] = g.chance(3) ? 0xffffffff : g.below(20);    // the music in the sound list
        // the launcher's /ShowFPS patch at 0x4cdbf7 (setne cl -> mov cl, 1; nop) in some cases
        if (g.chance(4)) {
            const BYTE patch[3] = {0xb1, (BYTE)(g.below(2)), 0x90};
            write_code(at<void *>(0x4cdbf7), patch, 3);
        }
        break;
    }
    case T_CTOR:
        g_self = xalloc(0x5b4);
        for (DWORD o = 0; o < 0x5b4; o += 4)
            at_(g_self, o) = g.next();
        g_a[0] = ptrv(win(W_M1));
        break;
    case T_DTOR:
    case T_DELETING_DTOR: {
        g_self = xalloc(0x5b4);
        for (DWORD o = 0; o < 0x5b4; o += 4)
            at_(g_self, o) = g.next();
        const DWORD members[6] = {0x354, 0x340, 0x34c, 0x350, 0x33c, 0x344};
        for (int i = 0; i < 6; i++)
            at_(g_self, members[i]) = g.chance(4) ? 0 : ptrv(win(W_M1 + (i % 4)));
        if (g_test == T_DELETING_DTOR)
            g_a[0] = g.below(4);
        break;
    }
    default: break;
    }
}

DWORD call_side()
{
    switch (g_test) {
    case T_WINMAIN: return call_this(0x477940, nullptr, 4, g_a);
    case T_STARTUP: return call_this(0x4cd780, g_self, 7, g_a);
    case T_CTOR: return call_this(0x59a670, g_self, 1, g_a);
    case T_DTOR: return call_this(0x59aa10, g_self, 0, nullptr), 0;
    case T_DELETING_DTOR: return call_this(0x59a9f0, g_self, 1, g_a);
    case T_APP_START: return call_this(0x5a12d0, nullptr, 0, nullptr), 0;
    default: return 0;
    }
}

DWORD call()
{
    imports_on(IMPORTS, sizeof IMPORTS / sizeof IMPORTS[0]);
    const DWORD r = run_escapable(&call_side);
    imports_off();
    return r;
}

struct TestDef {
    const char *name;
    int count;
    const Stub *stubs;
    int nstubs;
};
#define STUBS_OF(s) s, (int)(sizeof s / sizeof s[0])
const TestDef TESTS[] = {
    {"WinMain", 150, STUBS_OF(STUBS_WINMAIN)},
    {"Game_Startup", 400, STUBS_OF(STUBS_STARTUP)},
    {"CZaxApp_Ctor", 60, STUBS_OF(STUBS_APP)},
    {"CZaxApp_Dtor", 100, STUBS_OF(STUBS_APP)},
    {"CZaxApp_DeletingDtor", 100, STUBS_OF(STUBS_APP)},
    {"App_Start", 60, STUBS_OF(STUBS_APP_START)},
};

}  // namespace

ZAX_CHECKS(app_start)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        const DWORD dwords[] = {REPORTER, G_APP, 0x713f18, 0x713e00, 0x713e08, 0x713e0c, 0x713e14, 0x713e18};
        for (DWORD a : dwords)
            g_globals.add(a, 4);
        const DWORD bytes[] = {0x713e10, 0x713e11, 0x6c02d4};
        for (DWORD a : bytes)
            g_globals.add(a, 1);
    }
    memcpy(g_show_fps_site, at<void *>(0x4cdbf7), 3);
    Tally t{"app_start: WinMain, the start-up, the application object and its start"};
    const Check c{&g_hooks, &g_globals};
    for (int i = 0; i < (int)(sizeof TESTS / sizeof TESTS[0]); i++) {
        g_test = i;
        stubs_on(TESTS[i].stubs, TESTS[i].nstubs);
        const int count = scaled(TESTS[i].count);
        for (int k = 0; k < count; k++) {
            run_case(t, c, TESTS[i].name, 53000u + (unsigned)(i * 10007 + k), &build, &call);
            write_code(at<void *>(0x4cdbf7), g_show_fps_site, 3);
        }
        stubs_off();
    }
    t.report();
}

}  // namespace zax
