// The game window's Windows side (phase 5a, group G2, docs/DECOMPILATION.md 10): the window procedure 0x599d50, the
// message pump 0x5998f0, the window's size in windowed mode 0x477b60, Windows' cursor 0x518820/0x518860/0x518880,
// the clipboard 0x46d590/0x46d620, the three API wrappers 0x4d2460/0x4d24b0/0x4dd920 and the static GDI font
// 0x4d20d0/0x4d2140. The same Windows calls through the exe's import table as the original, in the same order and with
// the same arguments; every exe function they reach is called through its exe address.
//
// Globals [confirmed: the functions below and WinMain 0x477940]: 0x713e18 the game window, 0x713e10 the application is
// active (WM_ACTIVATE / WM_ACTIVATEAPP: wParam's low word 1 or 2), 0x713e11 the window is minimized (IsIconic, or the
// activation's high word), 0x713e1c the pump runs (a second call returns at once), 0x713e0c and 0x713e14 the registered
// messages "MSJSTICK_VJOYD_MSGSTR" and "MSWHEEL_ROLLMSG"; the mouse (the cursor object 0x6cff40): 0x6cff50/0x6cff54 the
// position of the last WM_MOUSEMOVE (x the low word unsigned, y the high word signed), 0x6cff58 the buttons (bit 0 left,
// 1 right, 2 middle; from 0x51f450's answer), 0x6cff5c the double clicks, 0x6cff60 the wheel (a float: 1/120 per
// notch, 0x602db8); the keyboard 0x6c1050. Cursor::Shown 0x518690 (4 bytes: the shown flag +0xd) cannot be hooked.
#include "ui_screens.h"
#include "x87.h"

// The import slots the jumps read, through the address layer (batch 6-10a): the address itself in the DLL (the same
// text as before), in the standalone exe the symbol tools/extract_exe_data.py defines for it (our own IAT once its
// home is `own`; as crt_seh.cpp's ZAX_ASM_DATA)
#ifdef ZAX_STANDALONE
#define ZAX_ASM_DATA(a) "_zax_data_" #a
#else
#define ZAX_ASM_DATA(a) #a
#endif

namespace zax::appwin {

using namespace ui;

constexpr DWORD WINDOW = 0x713e18, ACTIVE = 0x713e10, MINIMIZED = 0x713e11, PUMPING = 0x713e1c, JOY_MESSAGE = 0x713e0c,
                WHEEL_MESSAGE = 0x713e14, KEYS = 0x6c1050, CURSOR = 0x6cff40;

inline DWORD def_proc(DWORD w, DWORD msg, DWORD wp, DWORD lp) { return imp<DWORD(WINAPI *)(DWORD, DWORD, DWORD, DWORD)>(0x5ea21c)(w, msg, wp, lp); }
inline DWORD is_iconic(DWORD w) { return imp<DWORD(WINAPI *)(DWORD)>(0x5ea22c)(w); }
inline DWORD get_window_rect(DWORD w, int *r) { return imp<DWORD(WINAPI *)(DWORD, int *)>(0x5ea1e4)(w, r); }
inline BYTE is_full_screen(DWORD frame) { return (BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x567cc0)(frame); }
// The application's slot 9 (the game runs: active and not minimized)
inline void app_set_running(DWORD app, DWORD running) { vc(bp(app), 9, running); }
inline DWORD running() { return gb(glob_addr(ACTIVE)) && !gb(glob_addr(MINIMIZED)) ? 1 : 0; }

// ---- The window procedure ---------------------------------------------------------------------------------------------

// WM_DESTROY and WM_CLOSE (after PostQuitMessage(0)) and WM_QUIT: the resource cache unloaded (0x57b210) and its slot 9
// called, the application deleted (slot 0 with 1) and cleared, exit(0)
[[noreturn]] void quit()
{
    fn<void(THISCALL *)(DWORD)>(0x57b210)(fn<DWORD(__cdecl *)()>(0x57aef0)());
    vc(bp(fn<DWORD(__cdecl *)()>(0x57aef0)()), 9);
    if (const DWORD app = gd(glob_addr(G_REPORTER))) {
        vc(bp(app), 0, 1);
        gd(glob_addr(G_REPORTER)) = 0;
    }
    fn<void(__cdecl *)(int)>(0x5d0e36)(0);
    __builtin_unreachable();
}

// The registered joystick message goes straight to DefWindowProcA. WM_MOVE/WM_SIZE: the minimized flag from IsIconic,
// the application told whether it runs, and in full screen the window moved back to 0, 0 at its size (MoveWindow).
// WM_ACTIVATE/WM_ACTIVATEAPP: the flags from wParam, the application told, the keyboard's keys, presses and downs cleared
// and the timers reset. WM_PAINT: the back buffer flipped in windowed mode, 1. WM_SETCURSOR in the client area: the game's
// cursor refreshed, 1. WM_SYSKEYDOWN of Alt, Space, the Windows and menu keys and F10 and WM_SYSCOMMAND SC_KEYMENU: 0
// (no menu). WM_QUERYNEWPALETTE / WM_PALETTEISCHANGING: the application told whether it runs. All else, and after
// the handlers that do not answer themselves, DefWindowProcA.
DWORD __stdcall WindowProc(DWORD hwnd, DWORD msg, DWORD wp, DWORD lp)   // 0x599d50
{
    if (msg == gd(glob_addr(JOY_MESSAGE)))
        return def_proc(hwnd, msg, wp, lp);
    if (msg > 0x12) {
        if (msg > 0x112) {
            if (msg == 0x30f) {
                const DWORD app = gd(glob_addr(G_REPORTER));
                if (app && m(bp(app), 4))
                    app_set_running(app, running());
            } else if (msg == 0x310) {
                if (const DWORD app = gd(glob_addr(G_REPORTER)))
                    app_set_running(app, running());
            }
            return def_proc(hwnd, msg, wp, lp);
        }
        if (msg == 0x112) {
            if (wp == 0xf100)
                return 0;
            return def_proc(hwnd, msg, wp, lp);
        }
        if (msg == 0x1c)
            goto activate;
        if (msg == 0x20) {
            if ((WORD)lp != 1)
                return def_proc(hwnd, msg, wp, lp);
            fn<void(THISCALL *)(DWORD)>(0x518650)(glob_addr(CURSOR));
            return 1;
        }
        if (msg == 0x104) {
            const DWORD k = wp & 0xff;
            if (k == 0x12 || k == 0x20 || k == 0x5b || k == 0x5c || k == 0x5d || k == 0x79)
                return 0;
        }
        return def_proc(hwnd, msg, wp, lp);
    }
    if (msg == 0x12)
        quit();
    switch (msg) {
    case 2:
    case 0x10:
        imp<void(WINAPI *)(int)>(0x5ea228)(0);
        quit();
    case 3:
    case 5: {
        gb(glob_addr(MINIMIZED)) = is_iconic(hwnd) != 0;
        const DWORD app = gd(glob_addr(G_REPORTER));
        if (!app)
            break;
        app_set_running(app, gb(glob_addr(ACTIVE)) && !gb(glob_addr(MINIMIZED)) ? 1 : 0);
        const DWORD app2 = gd(glob_addr(G_REPORTER));
        if (!app2 || !m(bp(app2), 4) || !is_full_screen(m(bp(app2), 4)))
            break;
        int r[4];
        get_window_rect(gd(glob_addr(WINDOW)), r);
        imp<DWORD(WINAPI *)(DWORD, int, int, int, int, int)>(0x5ea218)(gd(glob_addr(WINDOW)), 0, 0, r[2] - r[0], r[3] - r[1], 1);
        break;
    }
    case 6:
        goto activate;
    case 0xf: {
        BYTE ps[0x40];
        imp<DWORD(WINAPI *)(DWORD, void *)>(0x5ea220)(hwnd, ps);
        const DWORD app = gd(glob_addr(G_REPORTER));
        if (app && m(bp(app), 4) && !is_full_screen(m(bp(app), 4)))
            fn<DWORD(THISCALL *)(DWORD)>(0x5693a0)(m(bp(gd(glob_addr(G_REPORTER))), 4));
        imp<DWORD(WINAPI *)(DWORD, void *)>(0x5ea224)(hwnd, ps);
        return 1;
    }
    default: break;
    }
    return def_proc(hwnd, msg, wp, lp);
activate: {
    const DWORD lo = wp & 0xffff;
    const BYTE minimized = (wp >> 16) != 0;
    const BYTE active = lo == 1 || lo == 2;
    gb(glob_addr(ACTIVE)) = active;
    gb(glob_addr(MINIMIZED)) = minimized;
    const DWORD app = gd(glob_addr(G_REPORTER));
    if (app) {
        app_set_running(app, active && !minimized ? 1 : 0);
        using Clear = void(THISCALL *)(DWORD, DWORD);
        fn<Clear>(0x4e27a0)(glob_addr(KEYS), 1);
        fn<Clear>(0x4e2980)(glob_addr(KEYS), 1);
        fn<Clear>(0x4e29b0)(glob_addr(KEYS), 1);
        fn<void(THISCALL *)(DWORD)>(0x4ce130)(gd(glob_addr(G_REPORTER)));
    }
    return def_proc(hwnd, msg, wp, lp);
}
}

// ---- The message pump -------------------------------------------------------------------------------------------------

// Every waiting message taken (PeekMessageA with PM_REMOVE): while the game runs (active, not minimized) without
// waiting; else with Sleep(1) when the application's flag +0x28 is set (0x4cde80), else Sleep(100) and the minimized
// flag from IsIconic after the peek. Keys go to the keyboard 0x6c1050 (down: the game key 0x599580 of the virtual key
// added (0x4e2a00) and the scan code pressed (0x4e29e0); up: released (0x4e29f0); characters added; WM_CONTEXTMENU:
// 0x12), the mouse into 0x6cff50-0x6cff60 (the buttons from 0x51f450, a double click also in 0x6cff5c; the wheel from
// WM_MOUSEWHEEL's high word or the registered wheel message's low word), then TranslateMessage and DispatchMessageA.
void __cdecl PumpMessages()   // 0x5998f0
{
    if (gb(glob_addr(PUMPING)))
        return;
    gb(glob_addr(PUMPING)) = 1;
    using Peek = DWORD(WINAPI *)(void *, DWORD, DWORD, DWORD, DWORD);
    using Sleep_ = void(WINAPI *)(DWORD);
    for (;;) {
        DWORD msg[7];
        DWORD got;
        if (gb(glob_addr(ACTIVE)) && !gb(glob_addr(MINIMIZED))) {
            got = imp<Peek>(0x5ea230)(msg, 0, 0, 0, 1);
        } else if (gd(glob_addr(G_REPORTER)) && (BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x4cde80)(gd(glob_addr(G_REPORTER)))) {
            imp<Sleep_>(0x5ea07c)(1);
            got = imp<Peek>(0x5ea230)(msg, 0, 0, 0, 1);
        } else {
            imp<Sleep_>(0x5ea07c)(100);
            got = imp<Peek>(0x5ea230)(msg, 0, 0, 0, 1);
            gb(glob_addr(MINIMIZED)) = is_iconic(gd(glob_addr(WINDOW))) != 0;
        }
        if (!got)
            break;
        const DWORD id = msg[1], wp = msg[2], lp = msg[3];
        using Key = void(THISCALL *)(DWORD, DWORD);
        using Buttons = DWORD(THISCALL *)(DWORD);
        int wheel = 0;
        bool is_wheel = false;
        if (id == gd(glob_addr(WHEEL_MESSAGE))) {
            wheel = (short)wp;
            is_wheel = true;
        } else {
            switch (id) {
            case 0x7b: fn<Key>(0x4e2a00)(glob_addr(KEYS), 0x12); break;
            case 0x100:
            case 0x104:
                fn<Key>(0x4e2a00)(glob_addr(KEYS), fn<DWORD(__fastcall *)(DWORD)>(0x599580)(wp));
                fn<Key>(0x4e29e0)(glob_addr(KEYS), (lp >> 16) & 0x1ff);
                break;
            case 0x101:
            case 0x105: fn<Key>(0x4e29f0)(glob_addr(KEYS), (lp >> 16) & 0x1ff); break;
            case 0x102:
            case 0x106: fn<Key>(0x4e2a00)(glob_addr(KEYS), wp & 0xff); break;
            case 0x200:
                gd(glob_addr(0x6cff50)) = lp & 0xffff;
                gd(glob_addr(0x6cff54)) = (DWORD)((int)lp >> 16);
                break;
            case 0x201: gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) | 1; break;
            case 0x202: gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) & ~1u; break;
            case 0x203:
                gd(glob_addr(0x6cff5c)) |= 1;
                gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) | 1;
                break;
            case 0x204: gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) | 2; break;
            case 0x205: gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) & ~2u; break;
            case 0x206:
                gd(glob_addr(0x6cff5c)) |= 2;
                gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) | 2;
                break;
            case 0x207: gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) | 4; break;
            case 0x208: gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) & ~4u; break;
            case 0x209:
                gd(glob_addr(0x6cff5c)) |= 4;
                gd(glob_addr(0x6cff58)) = fn<Buttons>(0x51f450)(glob_addr(CURSOR)) | 4;
                break;
            case 0x20a:
                wheel = (short)(wp >> 16);
                is_wheel = true;
                break;
            default: break;
            }
        }
        if (is_wheel)
            *glob<float *>(0x6cff60) = (float)((long double)wheel * (long double)*glob<float *>(0x602db8) + (long double)*glob<float *>(0x6cff60));
        imp<DWORD(WINAPI *)(void *)>(0x5ea280)(msg);
        imp<DWORD(WINAPI *)(void *)>(0x5ea234)(msg);
    }
    gb(glob_addr(PUMPING)) = 0;
}

// ---- The window's size in windowed mode -------------------------------------------------------------------------------

// The screen's depth asked (GetDeviceCaps 14 and 12 of the desktop DC, the answers unused); the window's style made a
// captioned, minimizable, system-menu window (style & 0x7f3dffff | 0xc20000); its outer size for a client area of the
// screen frame's size (or the default 640 x 480 without one) set (AdjustWindowRectEx, SetWindowPos 0x16); not topmost
// (0x13); and its top left corner kept inside the work area (SPI_GETWORKAREA; SetWindowPos 0x15)
void __cdecl SetWindowedSize()   // 0x477b60
{
    void *dc = imp<void *(WINAPI *)(DWORD)>(0x5ea260)(0);
    const auto caps = imp<int(WINAPI *)(void *, int)>(0x5ea030);
    caps(dc, 0xe);
    caps(dc, 0xc);
    imp<int(WINAPI *)(DWORD, void *)>(0x5ea264)(0, dc);
    const auto get_long = imp<DWORD(WINAPI *)(DWORD, int)>(0x5ea210);
    const DWORD style = get_long(gd(glob_addr(WINDOW)), -16);
    imp<DWORD(WINAPI *)(DWORD, int, DWORD)>(0x5ea20c)(gd(glob_addr(WINDOW)), -16, (style & 0x7f3dffff) | 0xc20000);
    int r[4];
    const auto set_rect = imp<DWORD(WINAPI *)(int *, int, int, int, int)>(0x5ea268);
    const DWORD rep = gd(glob_addr(G_REPORTER));
    if (rep && m(bp(rep), 4)) {
        BYTE *frame = bp(m(bp(rep), 4));
        const int h = (int)vc(frame, 39);
        const int w = (int)vc(frame, 45);
        set_rect(r, 0, 0, w, h);
    } else {
        const int h = (int)fn<DWORD(__cdecl *)()>(0x4cdc80)();
        const int w = (int)fn<DWORD(__cdecl *)()>(0x4cdc70)();
        set_rect(r, 0, 0, w, h);
    }
    const DWORD ex_style = get_long(gd(glob_addr(WINDOW)), -20);
    const DWORD menu = imp<DWORD(WINAPI *)(DWORD)>(0x5ea26c)(gd(glob_addr(WINDOW))) != 0;
    const DWORD style2 = get_long(gd(glob_addr(WINDOW)), -16);
    imp<DWORD(WINAPI *)(int *, DWORD, DWORD, DWORD)>(0x5ea270)(r, style2, menu, ex_style);
    const auto set_pos = imp<DWORD(WINAPI *)(DWORD, DWORD, int, int, int, int, DWORD)>(0x5ea274);
    set_pos(gd(glob_addr(WINDOW)), 0, 0, 0, r[2] - r[0], r[3] - r[1], 0x16);
    set_pos(gd(glob_addr(WINDOW)), (DWORD)-2, 0, 0, 0, 0, 0x13);
    int work[4];
    imp<DWORD(WINAPI *)(DWORD, DWORD, void *, DWORD)>(0x5ea278)(0x30, 0, work, 0);
    get_window_rect(gd(glob_addr(WINDOW)), r);
    int x = r[0], y = r[1];
    if (x < work[0])
        x = work[0];
    if (y < work[1])
        y = work[1];
    set_pos(gd(glob_addr(WINDOW)), 0, x, y, 0, 0, 0x15);
}

// ---- Windows' cursor ----------------------------------------------------------------------------------------------------

// (x, y in the client area): Windows' cursor put there (ClientToScreen, SetCursorPos)
DWORD __fastcall Cursor_SetWindowsPos(int x, int y)   // 0x518820
{
    int pt[2] = {x, y};
    imp<DWORD(WINAPI *)(DWORD, int *)>(0x5ea240)(gd(glob_addr(WINDOW)), pt);
    return imp<DWORD(WINAPI *)(int, int)>(0x5ea244)(pt[0], pt[1]);
}

// Windows' arrow (IDC_ARROW) shown
DWORD __cdecl Cursor_ShowArrow()   // 0x518860
{
    const DWORD c = imp<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea204)(0, 0x7f00);
    return imp<DWORD(WINAPI *)(DWORD)>(0x5ea24c)(c);
}

// Windows' cursor hidden (SetCursor(NULL))
DWORD __cdecl Cursor_HideWindows()   // 0x518880
{
    return imp<DWORD(WINAPI *)(DWORD)>(0x5ea24c)(0);
}

// ---- The clipboard ------------------------------------------------------------------------------------------------------

inline DWORD clipboard_format(const char *name)
{
    if (!crt_stricmp(name, glob<const char *>(0x60b078)))   // "Text": CF_TEXT
        return 1;
    return imp<DWORD(WINAPI *)(const char *)>(0x5ea1dc)(name);
}

// (format name, data, size): the clipboard opened (no owner); a moveable, shared global block (GMEM_MOVEABLE |
// GMEM_DDESHARE) with the data emptied into the clipboard under the format ("Text" is CF_TEXT, any other name
// registered); nothing but the close when the block cannot be made or locked (the block is not freed then). The
// CloseClipboard's answer.
DWORD __fastcall Clipboard_SetText(const char *format, const BYTE *data, DWORD size)   // 0x46d590
{
    imp<DWORD(WINAPI *)(DWORD)>(0x5ea1e8)(0);
    const DWORD h = imp<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea088)(0x2002, size);
    if (h) {
        if (BYTE *p = imp<BYTE *(WINAPI *)(DWORD)>(0x5ea084)(h)) {
            memcpy(p, data, size);
            imp<DWORD(WINAPI *)(DWORD)>(0x5ea080)(h);
            imp<DWORD(WINAPI *)()>(0x5ea214)();
            imp<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea1d8)(clipboard_format(format), h);
        }
    }
    return imp<DWORD(WINAPI *)()>(0x5ea1e0)();
}

// (format name, &size or null): a copy of the clipboard's data in that format (operator new, its size in *size), or 0
// when there is none
BYTE *__fastcall Clipboard_GetText(const char *format, DWORD *size)   // 0x46d620
{
    imp<DWORD(WINAPI *)(DWORD)>(0x5ea1e8)(0);
    const DWORD h = imp<DWORD(WINAPI *)(DWORD)>(0x5ea1ec)(clipboard_format(format));
    if (!h) {
        imp<DWORD(WINAPI *)()>(0x5ea1e0)();
        return nullptr;
    }
    const BYTE *p = imp<const BYTE *(WINAPI *)(DWORD)>(0x5ea084)(h);
    const DWORD n = imp<DWORD(WINAPI *)(DWORD)>(0x5ea124)(h);
    if (size)
        *size = n;
    BYTE *copy = op_new(n);
    memcpy(copy, p, n);
    imp<DWORD(WINAPI *)(DWORD)>(0x5ea080)(h);
    imp<DWORD(WINAPI *)()>(0x5ea1e0)();
    return copy;
}

// ---- The static GDI font ------------------------------------------------------------------------------------------------

// Static initializer (CRT table 0x609778): a 12-pixel Arial (CreateFontA(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0,
// VARIABLE_PITCH, "Arial")) into 0x6c0480 with the values of the object around it (0x6c0468 1, 0x6c046c 1, 0x6c0470 10,
// 0x6c0474 0, 0x6c0478 0), its release registered with atexit
DWORD __cdecl StaticInit_GdiFont()   // 0x4d20d0
{
    using CreateFontA_ = DWORD(WINAPI *)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char *);
    gd(glob_addr(0x6c0480)) = imp<CreateFontA_>(0x5ea028)(0xc, 0, 0, 0, 0x190, 0, 0, 0, 0, 0, 0, 0, 2, glob<const char *>(0x615b8c));
    gd(glob_addr(0x6c0470)) = 10;
    gd(glob_addr(0x6c0474)) = 0;
    gd(glob_addr(0x6c0468)) = 1;
    gd(glob_addr(0x6c046c)) = 1;
    gd(glob_addr(0x6c0478)) = 0;
    return (DWORD)fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x4d2140));
}

DWORD __cdecl StaticExit_GdiFont()   // 0x4d2140
{
    const DWORD font = gd(glob_addr(0x6c0480));
    if (!font)
        return 0;
    return imp<DWORD(WINAPI *)(DWORD)>(0x5ea024)(font);
}

// ---- The API wrappers -----------------------------------------------------------------------------------------------

// The game's own six-byte wrappers: an indirect jump through the import, kept as such (no frame, every register as the
// caller left it)
extern "C" void Platform_GetTickCount();   // 0x4d2460: GetTickCount
extern "C" void Platform_TimeGetTime();    // 0x4d24b0: timeGetTime
extern "C" void Joystick_NumDevs();        // 0x4dd920: joyGetNumDevs
__asm__(".text\n"
        "_Platform_GetTickCount:\n  jmp *" ZAX_ASM_DATA(0x5ea0e8) "\n"
        "_Platform_TimeGetTime:\n  jmp *" ZAX_ASM_DATA(0x5ea294) "\n"
        "_Joystick_NumDevs:\n  jmp *" ZAX_ASM_DATA(0x5ea298) "\n");

}  // namespace zax::appwin

namespace zax {

using namespace appwin;

ZAX_HOOKS(app_window)
{
    const Hook list[] = {
        {0x599d50, ptr(&WindowProc), "App_WindowProc", 5, {0xa1, 0x0c, 0x3e, 0x71, 0x00, 0x83, 0xec, 0x50}},
        {0x5998f0, ptr(&PumpMessages), "App_PumpMessages", 5, {0xa0, 0x1c, 0x3e, 0x71, 0x00, 0x83, 0xec, 0x20}},
        {0x477b60, ptr(&SetWindowedSize), "App_SetWindowedSize", 5, {0x83, 0xec, 0x20, 0x56, 0x57, 0x6a, 0x00, 0xff}},
        {0x518820, ptr(&Cursor_SetWindowsPos), "Cursor::SetWindowsPos", 7, {0x83, 0xec, 0x08, 0x89, 0x4c, 0x24, 0x00, 0x8b}},
        {0x518860, ptr(&Cursor_ShowArrow), "Cursor::ShowArrow", 5, {0x68, 0x00, 0x7f, 0x00, 0x00, 0x6a, 0x00, 0xff}},
        {0x518880, ptr(&Cursor_HideWindows), "Cursor::HideWindows", 8, {0x6a, 0x00, 0xff, 0x15, 0x4c, 0xa2, 0x5e, 0x00}},
        {0x46d590, ptr(&Clipboard_SetText), "Clipboard_SetText", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf2, 0x8b, 0xe9}},
        {0x46d620, ptr(&Clipboard_GetText), "Clipboard_GetText", 5, {0x53, 0x55, 0x56, 0x8b, 0xea, 0x8b, 0xf1, 0x6a}},
        {0x4d20d0, ptr(&StaticInit_GdiFont), "StaticInit_GdiFont", 5, {0x68, 0x8c, 0x5b, 0x61, 0x00, 0x6a, 0x02, 0x6a}},
        {0x4d2140, ptr(&StaticExit_GdiFont), "StaticExit_GdiFont", 5, {0xa1, 0x80, 0x04, 0x6c, 0x00, 0x85, 0xc0, 0x74}},
        {0x4d2460, ptr(&Platform_GetTickCount), "Platform_GetTickCount", 6, {0xff, 0x25, 0xe8, 0xa0, 0x5e, 0x00, 0x90, 0x90}},
        {0x4d24b0, ptr(&Platform_TimeGetTime), "Platform_TimeGetTime", 6, {0xff, 0x25, 0x94, 0xa2, 0x5e, 0x00, 0x90, 0x90}},
        {0x4dd920, ptr(&Joystick_NumDevs), "Joystick_NumDevs", 6, {0xff, 0x25, 0x98, 0xa2, 0x5e, 0x00, 0x90, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
