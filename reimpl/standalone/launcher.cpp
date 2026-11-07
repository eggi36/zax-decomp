// The launcher's switches built into the standalone exe (docs/DECOMPILATION.md 13, "Batch 6-10c").
//
// Zax-Launcher.exe starts Zax.exe with Zax-Launcher.dll (zax-launcher/zax_launcher_dll.c), which patches the exe's code: the
// six calls of the main menu constructor, the game menu's "End Game" allocation (its "Load Map" button), DispatchMessageA's
// import slot (/Cheats), the section log functions (/DebugLog) and the frame counter's setne (/ShowFPS). Until batch
// 6-10c the standalone exe compiled that file as it is and let it patch the copied image. Without the image the same
// switches work through the address layer: the function table's entries of the main menu constructor 0x5be4a0 and the
// section log 0x4a2650/0x4a2670 point at the functions below (every caller of those goes through the table), our own
// import slot of DispatchMessageA is replaced, and the reimplemented game menu and frame counter ask this file
// (standalone_launcher.h). The behaviour, the order and the log lines (Zax-Launcher.log, Zax-Debug.log) are the DLL's;
// the checks of the exe's bytes are gone (there are none to check). Command line as for Zax-Launcher.dll:
//   /Map "Act 1/M1M2/01 Main" /Spawn "Game Start" /Difficulty Normal /NoLoadMap /Cheats /DebugLog /ShowFPS /NetGraphs
//   /DebugDraw /ShowHidden /DrawOutlines /AltDirtyDraw /NoEntryDraw
// (/NetGraphs and the draw bytes are set as the DLL sets them, through the layer, where the reimplemented code reads
// them; /DebugDraw is asked by the reimplemented draw pass Layer_DrawPass instead of the DLL's patch at 0x4e8b3c)
#include "../src/reimpl.h"
#include "../src/standalone_launcher.h"

typedef unsigned int UINT;
typedef uintptr_t UINT_PTR;
typedef long LRESULT;
struct MSG {
    HANDLE hwnd;
    UINT message;
    UINT_PTR wParam;
    long lParam;
    DWORD time;
    long pt[2];
};

WINIMPORT HANDLE WINAPI GetModuleHandleA(const char *name);
WINIMPORT void *WINAPI GetProcAddress(HANDLE module, const char *name);
WINIMPORT short WINAPI GetKeyState(int key);
WINIMPORT UINT_PTR WINAPI SetTimer(HANDLE wnd, UINT_PTR id, UINT elapse, void(WINAPI *proc)(HANDLE, UINT, UINT_PTR, DWORD));
WINIMPORT BOOL WINAPI KillTimer(HANDLE wnd, UINT_PTR id);
WINIMPORT BOOL WINAPI IsBadStringPtrA(const char *p, UINT_PTR max);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax::launcher {

namespace {

// Zax.exe (the addresses of zax-launcher/zax_launcher_dll.c, through the layer)
constexpr DWORD MAINMENU_CTOR = 0x5be4a0;    // CMainMenu::CMainMenu(arg), ret 4
constexpr DWORD WORLD = 0x6c2080;            // [WORLD] = world object
constexpr DWORD GAME = 0x713f14;             // [GAME]: target of difficulty and game mode
constexpr DWORD DIFF_CATALOG = 0x6c2a00;     // catalogue CLevelOfDificulty
constexpr DWORD DIFF_COUNT = 0x6c2a14;
constexpr DWORD SET_DIFFICULTY = 0x5bb780;   // thiscall [[GAME]+0x34c], (Index), ret 4
constexpr DWORD SET_MODE = 0x5a3180;         // thiscall [GAME], (char *mode), ret 4
constexpr DWORD PREPARE_START = 0x51e450;    // thiscall [[WORLD]+0x334], ret
constexpr DWORD CSTRING_CSTR = 0x4e13a0;     // thiscall CString*, returns char*
constexpr DWORD SET_RETURN_WIN = 0x404180;   // thiscall window, (window *), ret 4
constexpr DWORD QUEST_MODE = 0x6179f0;       // "QuestMode"
constexpr DWORD VT_START_GAME = 0x13c;       // world: StartNewGame(1, map, start point, 0)
constexpr DWORD VT_CLOSE = 0x14;             // close a window (deletes the object)
constexpr DWORD APP_VT_STEP = 0x602dd4;      // application vtable 0x602dc8, slot 3: the step (dt), ret 4
constexpr DWORD APP_STEP = 0x59d140;         // CZaxWorld::Frame, the slot's original target
constexpr DWORD MENU_LOAD_MAP = 0x13c;       // the button the game menu's click handler knows
constexpr DWORD ALLOC = 0x417710;            // operator new, size in ecx
constexpr DWORD BUTTON_SIZE = 0x138;
constexpr DWORD BUTTON_CTOR = 0x40fa60;      // thiscall button, (parent, CString *text, 0), ret 0xc
constexpr DWORD ADD_CONTROL = 0x40c7c0;      // thiscall window, (button, placement, 0, x, y, 0), ret 0x18
constexpr DWORD LABEL_LOAD_MAP = 0x7144d4;   // CString "Load &Map (F9)", created in the exe but unused
constexpr DWORD VT_LOCAL_PLAYER = 0x04;      // world: object for the multiplayer query
constexpr DWORD VT_IS_CLIENT = 0x34;         // true as multiplayer client
constexpr DWORD IAT_DISPATCH = 0x5ea234;     // import slot DispatchMessageA, only call 0x599b62
constexpr DWORD GET_PLAYER = 0x4f5d80;       // thiscall [GAME], the player character or 0
constexpr DWORD SECTION_BEGIN = 0x4a2650;    // name in ecx: write "*** name ***", depth + 1
constexpr DWORD SECTION_END = 0x4a2670;      // depth - 1, then "section done" or "END of all active section"
constexpr DWORD SECTION_DEPTH = 0x6bdf68;    // depth, not used anywhere else by the exe

constexpr UINT WM_KEYDOWN = 0x100, WM_KEYUP = 0x101;
constexpr int VK_CONTROL = 0x11;

struct Cheat {
    UINT key;
    CodeKey routine;
    const char *name;
};
// key with Ctrl, routine (cdecl, no arguments), name in the exe
const Cheat CHEATS[] = {
    {0x70, 0x5a2500, "giveall"},  {0x71, 0x5a22b0, "loadup"},   {0x72, 0x5a2540, "cash"},
    {0x73, 0x5a2630, "GunIt"},    {0x74, 0x5a2670, "chargeit"}, {0x75, 0x5a26a0, "mine"},
    {0x76, 0x5a2250, "medic"},    {0x77, 0x59a600, "fixgun"},   {0x78, 0x5a2590, "generator"},
};

using CtorFn = void *(THISCALL *)(void *self, DWORD arg);

char g_map[MAX_PATH], g_spawn[MAX_PATH], g_difficulty[64] = "Normal";
void *g_menu;
bool g_done, g_started, g_load_map, g_show_fps, g_debug_draw;
UINT_PTR g_timer;   // the fallback timer, killed once the first step started the map
HANDLE g_log = INVALID_HANDLE_VALUE;
CtorFn g_main_menu_ctor;   // the reimplemented constructor (the table's entry before the redirection)

void log_line(const char *fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE)
        return;
    char buf[1100];
    va_list ap;
    va_start(ap, fmt);
    int n = wvsprintfA(buf, fmt, ap);
    va_end(ap);
    buf[n++] = '\r', buf[n++] = '\n';
    DWORD written;
    WriteFile(g_log, buf, (DWORD)n, &written, nullptr);
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
int str_nieq(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (lower(a[i]) != lower(b[i]))
            return 1;
        if (!a[i])
            return 0;
    }
    return 0;
}
const char *str_find(const char *s, const char *what)
{
    const int n = lstrlenA(what);
    for (; *s; s++)
        if (str_nieq(s, what, n) == 0)
            return s;
    return nullptr;
}
bool has_switch(const char *args, const char *name)
{
    const int n = lstrlenA(name);
    for (const char *p = args; (p = str_find(p, name)) != nullptr; p++)
        if ((p == args || p[-1] == ' ' || p[-1] == '\t') && (!p[n] || p[n] == ' ' || p[n] == '\t'))
            return true;
    return false;
}
void exe_dir(char *out)
{
    DWORD n = GetModuleFileNameA(nullptr, out, MAX_PATH);
    while (n > 0 && out[n - 1] != '\\' && out[n - 1] != '/')
        n--;
    out[n] = 0;
}

// Value after /Name on the command line, also in quotes
bool arg_value(const char *cmd, const char *name, char *out, int size)
{
    const int n = lstrlenA(name);
    for (const char *p = cmd; *p; p++) {
        if (*p != '/' || str_nieq(p + 1, name, n) != 0 || (p[1 + n] != ' ' && p[1 + n] != '\t'))
            continue;
        const char *v = p + 1 + n;
        while (*v == ' ' || *v == '\t')
            v++;
        char end = ' ';
        if (*v == '"')
            end = *v++;
        int i = 0;
        while (v[i] && v[i] != end && i + 1 < size)
            out[i] = v[i], i++;
        out[i] = 0;
        return i > 0;
    }
    return false;
}

DWORD rd(DWORD addr) { return *reinterpret_cast<DWORD *>(static_cast<uintptr_t>(addr)); }
DWORD slot(void *object, DWORD offset) { return rd(*static_cast<DWORD *>(object) + offset); }

int find_difficulty()
{
    void *catalog = glob<void *>(DIFF_CATALOG);
    using NameFn = const char *(THISCALL *)(void *, int);
    const NameFn name_of = reinterpret_cast<NameFn>(static_cast<uintptr_t>(slot(catalog, 4)));
    const int count = *glob<int *>(DIFF_COUNT), n = lstrlenA(g_difficulty);
    int found = -1;
    for (int i = 0; i < count; i++) {
        const char *name = name_of(catalog, i);
        const bool bad = name == nullptr || IsBadStringPtrA(name, 256);
        log_line("Difficulty %d: %s", i, bad ? "(unreadable)" : name);
        if (bad)
            continue;
        const char *base = name;   // the name comes with its folder: "Dificulty Types/Normal"
        for (const char *p = name; *p; p++)
            if (*p == '/' || *p == '\\')
                base = p + 1;
        if (found < 0 && str_nieq(base, g_difficulty, n) == 0 && (base[n] == 0 || base[n] == '.'))
            found = i;
    }
    return found;
}

void start_map()
{
    if (g_started)
        return;
    g_started = true;
    void *world = *glob<void **>(WORLD), *game = *glob<void **>(GAME);
    log_line("World %08lX, game %08lX, menu %08lX", (DWORD)(uintptr_t)world, (DWORD)(uintptr_t)game, (DWORD)(uintptr_t)g_menu);
    if (!world || !game) {
        log_line("Aborted: game not ready");
        return;
    }
    const int diff = find_difficulty();
    if (diff >= 0)
        fn<void(THISCALL *)(void *, int)>(SET_DIFFICULTY)(reinterpret_cast<void *>(static_cast<uintptr_t>(rd((DWORD)(uintptr_t)game + 0x34c))), diff);
    else
        log_line("Difficulty \"%s\" not found, keeping the game default", g_difficulty);
    fn<void(THISCALL *)(void *, const void *)>(SET_MODE)(game, glob<const char *>(QUEST_MODE));
    fn<void(THISCALL *)(void *)>(PREPARE_START)(reinterpret_cast<void *>(static_cast<uintptr_t>(rd((DWORD)(uintptr_t)world + 0x334))));

    const char *spawn = g_spawn;
    if (!*spawn) {   // like StartNewGame without a map: First Map Spawn Point from CRules
        const DWORD rules = rd((DWORD)(uintptr_t)world + 0x2e0);
        spawn = fn<const char *(THISCALL *)(void *)>(CSTRING_CSTR)(reinterpret_cast<void *>(static_cast<uintptr_t>(rules + 0xc)));
    }
    log_line("StartNewGame(1, \"%s\", \"%s\", 0)", g_map, spawn);
    using StartFn = void(THISCALL *)(void *, int, const char *, const char *, int);
    reinterpret_cast<StartFn>(static_cast<uintptr_t>(slot(world, VT_START_GAME)))(world, 1, g_map, spawn, 0);

    // like the difficulty dialog: clear the return target, close the window
    fn<void(THISCALL *)(void *, const void *)>(SET_RETURN_WIN)(g_menu, nullptr);
    reinterpret_cast<void(THISCALL *)(void *)>(static_cast<uintptr_t>(slot(g_menu, VT_CLOSE)))(g_menu);
    log_line("done");
}

void WINAPI on_timer(HANDLE, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    g_timer = 0;
    if (!g_started)
        log_line("Starting map from the timer (the first step did not come)");
    start_map();
}

// The application's step slot (own vtable block; its value the function table's entry since batch 6-10b)
void set_step_slot(DWORD target) { *glob<DWORD *>(APP_VT_STEP) = target; }

// The application's first step after the main menu: put the slot back, start the map, then the step
void THISCALL first_step(void *self, float dt)
{
    set_step_slot(fn_addr(APP_STEP));
    if (g_timer)
        KillTimer(nullptr, g_timer), g_timer = 0;
    log_line("First step, starting map");
    start_map();
    fn<void(THISCALL *)(void *, float)>(APP_STEP)(self, dt);
}

using DispatchFn = LRESULT(WINAPI *)(const MSG *);
DispatchFn g_dispatch;

void run_cheat(int i)
{
    void *game = *glob<void **>(GAME);
    if (!game || !fn<void *(THISCALL *)(void *)>(GET_PLAYER)(game)) {
        log_line("Cheat %s: no player, ignored", CHEATS[i].name);
        return;
    }
    log_line("Cheat %s", CHEATS[i].name);
    code<void(__cdecl *)()>(CHEATS[i].routine)();
}

// Replacement for DispatchMessageA: catch Ctrl+F keys, pass everything else on as usual
LRESULT WINAPI dispatch_hook(const MSG *msg)
{
    if ((msg->message == WM_KEYDOWN || msg->message == WM_KEYUP) && (GetKeyState(VK_CONTROL) & 0x8000))
        for (int i = 0; i < (int)(sizeof CHEATS / sizeof *CHEATS); i++)
            if (msg->wParam == CHEATS[i].key) {
                if (msg->message == WM_KEYDOWN && !(msg->lParam & 0x40000000))   // only on the first press
                    run_cheat(i);
                return 0;   // the game does not see the key (F9 would otherwise be "Load Map")
            }
    return g_dispatch(msg);
}

void install_cheats()
{
    DWORD *slot = imp_slot<DWORD *>(IAT_DISPATCH);
    void *real = GetProcAddress(GetModuleHandleA("user32.dll"), "DispatchMessageA");
    if (!real || *slot != (DWORD)(uintptr_t)real) {
        log_line("DispatchMessageA import does not match, different Zax.exe? Cheats stay off.");
        return;
    }
    g_dispatch = reinterpret_cast<DispatchFn>(real);
    *slot = (DWORD)(uintptr_t)&dispatch_hook;
    log_line("Cheats enabled: Ctrl+F1 giveall, F2 loadup, F3 cash, F4 GunIt, F5 chargeit, "
             "F6 mine, F7 medic, F8 fixgun, F9 generator");
}

HANDLE g_debug = INVALID_HANDLE_VALUE;
DWORD g_debug_start;

// line as in the exe ("*** %s ***"), preceded by the time since start in ms and indentation per depth
void debug_line(const char *text, int depth)
{
    char buf[1100];
    int n = wsprintfA(buf, "%7lu ", GetTickCount() - g_debug_start);
    for (int i = 0; i < depth && i < 20; i++)
        buf[n++] = ' ', buf[n++] = ' ';
    n += wsprintfA(buf + n, "*** %.1000s ***\r\n", text ? text : "(null)");
    DWORD written;
    WriteFile(g_debug, buf, (DWORD)n, &written, nullptr);
}

void __fastcall section_begin(const char *name)
{
    int *depth = glob<int *>(SECTION_DEPTH);
    debug_line(name, *depth);
    ++*depth;
}

void __cdecl section_end()
{
    int *depth = glob<int *>(SECTION_DEPTH);
    --*depth;
    debug_line(*depth ? "section done" : "END of all active section", *depth);
}

void set_table(DWORD exe_address, const void *target)
{
    const int i = layer::find(layer::FN, exe_address);
    if (i >= 0)
        layer::zax_fn_table[i] = (DWORD)(uintptr_t)target;
}

void install_debug_log()
{
    char path[MAX_PATH + 16];
    exe_dir(path);
    lstrcatA(path, "Zax-Debug.log");
    g_debug = CreateFileA(path, 0x40000000 /* GENERIC_WRITE */, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_debug == INVALID_HANDLE_VALUE) {
        log_line("Cannot create %s, debug log stays off.", path);
        return;
    }
    g_debug_start = GetTickCount();
    set_table(SECTION_BEGIN, reinterpret_cast<const void *>(&section_begin));
    set_table(SECTION_END, reinterpret_cast<const void *>(&section_end));
    log_line("Debug log enabled: %s", path);
}

// Replacement for the main menu constructor (all six callers go through the function table)
void *THISCALL main_menu_ctor(void *self, DWORD arg)
{
    void *menu = g_main_menu_ctor(self, arg);
    if (!g_done && menu) {
        g_done = true;
        g_menu = menu;
        // only run once the main loop is running: in its first step, before anything is drawn
        if (*glob<DWORD *>(APP_VT_STEP) == fn_addr(APP_STEP))
            set_step_slot((DWORD)(uintptr_t)&first_step);
        else
            log_line("Step slot %08lX does not match, starting map from the timer", *glob<DWORD *>(APP_VT_STEP));
        g_timer = SetTimer(nullptr, 0, 300, &on_timer);
        log_line("Main menu created, starting map in the first step");
    }
    return menu;
}

void install_main_menu()
{
    const int i = layer::find(layer::FN, MAINMENU_CTOR);
    g_main_menu_ctor = reinterpret_cast<CtorFn>(static_cast<uintptr_t>(layer::zax_fn_table[i]));
    layer::zax_fn_table[i] = (DWORD)(uintptr_t)&main_menu_ctor;
    log_line("Main menu redirected (%u call sites)", 6u);
}

}  // namespace

bool show_fps() { return g_show_fps; }
bool debug_draw() { return g_debug_draw; }

// Called by the reimplemented game menu constructor before "End Game" (where the DLL jumps in at 0x5bfe14)
void add_load_map(BYTE *menu)
{
    if (!g_load_map)
        return;
    void *world = *glob<void **>(WORLD);
    void *obj = reinterpret_cast<void *(THISCALL *)(void *)>(static_cast<uintptr_t>(slot(world, VT_LOCAL_PLAYER)))(world);
    if (reinterpret_cast<char(THISCALL *)(void *)>(static_cast<uintptr_t>(slot(obj, VT_IS_CLIENT)))(obj))
        return;
    void *button = fn<void *(__fastcall *)(DWORD)>(ALLOC)(BUTTON_SIZE);
    if (button)
        button = fn<void *(THISCALL *)(void *, void *, void *, int)>(BUTTON_CTOR)(button, menu, glob<void *>(LABEL_LOAD_MAP), 0);
    *reinterpret_cast<void **>(menu + MENU_LOAD_MAP) = button;
    log_line("Game menu: button \"%s\" created", fn<const char *(THISCALL *)(void *)>(CSTRING_CSTR)(glob<void *>(LABEL_LOAD_MAP)));
    fn<void(THISCALL *)(void *, void *, int, int, int, int, int)>(ADD_CONTROL)(menu, button, 8, 0, -1000, -1000, 0);
}

// At the start, after the function table holds the replacements (loader.cpp), as the DLL's DllMain
void install()
{
    char path[MAX_PATH + 16];
    exe_dir(path);
    lstrcatA(path, "Zax-Launcher.log");
    g_log = CreateFileA(path, 0x40000000 /* GENERIC_WRITE */, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    const char *cmd = GetCommandLineA();
    log_line("Command line: %s", cmd);
    if (has_switch(cmd, "/NoLoadMap")) {
        log_line("Game menu: Load Map button disabled (/NoLoadMap)");
    } else {
        g_load_map = true;
        log_line("Game menu: Load Map button enabled");
    }
    if (has_switch(cmd, "/Cheats"))
        install_cheats();
    if (has_switch(cmd, "/DebugLog"))
        install_debug_log();
    if (has_switch(cmd, "/ShowFPS")) {
        g_show_fps = true;
        log_line("Frame counter enabled");
    }
    if (has_switch(cmd, "/NetGraphs")) {
        *glob<BYTE *>(0x6d00ec) = 1;   // the traffic graphs (G_NET_GRAPHS_ON, net_game.h)
        log_line("Network traffic graphs enabled");
    }
    if (has_switch(cmd, "/DebugDraw")) {
        g_debug_draw = true;
        log_line("Debug drawing of the AIs and parts enabled");
    }
    auto draw_byte = [cmd](const char *name, const char *what, BYTE *flag) {
        if (has_switch(cmd, name)) {
            *flag = 1;
            log_line("%s enabled", what);
        }
    };
    draw_byte("/ShowHidden", "Hidden parts", glob<BYTE *>(0x6c1cf5));
    draw_byte("/DrawOutlines", "Draw queue outlines", glob<BYTE *>(0x6c1cf9));
    draw_byte("/AltDirtyDraw", "Alternative dirty tile drawing", glob<BYTE *>(0x6c1cf7));
    draw_byte("/NoEntryDraw", "No queue entries in the dirty tiles", glob<BYTE *>(0x6c1cf8));
    if (!arg_value(cmd, "Map", g_map, sizeof g_map)) {
        log_line("No /Map given, game starts normally");
        return;
    }
    arg_value(cmd, "Spawn", g_spawn, sizeof g_spawn);
    arg_value(cmd, "Difficulty", g_difficulty, sizeof g_difficulty);
    for (char *p = g_map; *p; p++)   // as in Rules.txt: paths with /
        if (*p == '\\')
            *p = '/';
    log_line("Map \"%s\", spawn point \"%s\", difficulty \"%s\"", g_map, g_spawn[0] ? g_spawn : "(from Rules.txt)", g_difficulty);
    install_main_menu();
}

}  // namespace zax::launcher
