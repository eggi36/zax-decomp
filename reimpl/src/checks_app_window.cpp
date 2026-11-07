// Checks of the game window's Windows side (app_window.cpp) against the original: every user32, gdi32, kernel32 and
// winmm import the functions call is a recording stand-in (checks_sys_common.h; arguments by value or by what they
// point to, the answers from the case: DefWindowProcA's result, IsIconic, the rectangles of GetWindowRect,
// AdjustWindowRectEx and SystemParametersInfoA, a scripted queue of messages behind PeekMessageA, clipboard data and
// failing GlobalAlloc / GlobalLock / GetClipboardData), the application, the resource cache and the screen frame are
// fakes of the toolkit harness (recording vtables), the exe functions outside the module (the keyboard, the cursor, the
// timers, the flip, the default size, the allocator, atexit) stand-ins, exit leaves the side. Compared: eax, the call
// log, the block of fakes and the globals (the activation and pump flags, the mouse, the window, the font).
// Cases: the window procedure with every message it handles and others (the activation's wParam forms, WM_SETCURSOR in
// and outside the client area, the system keys, SC_KEYMENU, the palette messages, with and without an application and
// a screen, full screen and windowed); the pump with random message queues (keys, characters, the mouse buttons and
// double clicks, the wheel in both forms, others) in the three waiting modes and while already pumping; the window size
// with and without a screen; the cursor; the clipboard both ways (text and registered formats, every failure); the API
// wrappers; the static font and its release.
#include "checks_sys_common.h"

namespace zax {

int hooks_app_window(Hook *out);

namespace {

using namespace sysc;

constinit ModuleHooks g_hooks{"app_window", &hooks_app_window};
Globals g_globals;

constexpr DWORD REPORTER = 0x6c02cc, WINDOW = 0x713e18, ACTIVE = 0x713e10, MINIMIZED = 0x713e11, PUMPING = 0x713e1c,
                JOY_MESSAGE = 0x713e0c, WHEEL_MESSAGE = 0x713e14;

// The "game object" class's stack arguments for the application fake: slot 0 (deleting destructor), 2 (load the game
// data), 9 (the game runs) take one
const char APP_ARGS[] = "1010000001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

// ---- Import stand-ins -----------------------------------------------------------------------------------------------

DWORD WINAPI i_def_proc(DWORD w, DWORD msg, DWORD wp, DWORD lp)
{
    rec(0xa21c, w, msg, wp, lp);
    return ans();
}
DWORD WINAPI i_is_iconic(DWORD w)
{
    rec(0xa22c, w);
    return ans_chance(3) ? 1 + ans_below(5) : 0;
}
DWORD WINAPI i_window_rect(DWORD w, int *r)
{
    rec(0xa1e4, w);
    r[0] = (int)ans_below(400) - 100, r[1] = (int)ans_below(300) - 100;
    r[2] = r[0] + (int)ans_below(900), r[3] = r[1] + (int)ans_below(700);
    return 1;
}
DWORD WINAPI i_move_window(DWORD w, int x, int y, int cx, int cy, DWORD repaint)
{
    rec(0xa218, w, (DWORD)x ^ (DWORD)y << 16, (DWORD)cx ^ (DWORD)cy << 16, repaint);
    return 1;
}
DWORD WINAPI i_begin_paint(DWORD w, BYTE *ps)
{
    rec(0xa220, w);
    for (int i = 0; i < 0x40; i++)
        ps[i] = (BYTE)(i * 7 + 3);
    return 0xdc00;
}
DWORD WINAPI i_end_paint(DWORD w, const BYTE *ps)
{
    rec(0xa224, w, fnv(2166136261u, ps, 0x40));
    return 1;
}
void WINAPI i_post_quit(int code) { rec(0xa228, (DWORD)code); }

// The message queue of the pump
constexpr int MAXQ = 24;
DWORD g_queue[MAXQ][3];
int g_nqueue, g_next;
DWORD WINAPI i_peek(DWORD *msg, DWORD w, DWORD lo, DWORD hi, DWORD remove)
{
    rec(0xa230, w, lo, hi, remove);
    if (g_next >= g_nqueue)
        return 0;
    msg[0] = 0x77770011;
    msg[1] = g_queue[g_next][0];
    msg[2] = g_queue[g_next][1];
    msg[3] = g_queue[g_next][2];
    msg[4] = 1000 + (DWORD)g_next;
    msg[5] = 10;
    msg[6] = 20;
    g_next++;
    return 1;
}
DWORD WINAPI i_translate(const DWORD *msg)
{
    rec(0xa280, fnv(2166136261u, msg, 0x1c));
    return 1;
}
DWORD WINAPI i_dispatch(const DWORD *msg)
{
    rec(0xa234, fnv(2166136261u, msg, 0x1c));
    return ans();
}
void WINAPI i_sleep(DWORD ms)
{
    if (foreign())
        return real<void(WINAPI *)(DWORD)>(0x5ea07c)(ms);
    rec(0xa07c, ms);
}

// The window size
void *WINAPI i_get_dc(DWORD w)
{
    rec(0xa260, w);
    return at<void *>(0xdc0000 | ans_below(0x100));
}
int WINAPI i_device_caps(void *dc, int what)
{
    rec(0xa030, ptrv(dc), (DWORD)what);
    return (int)ans_below(40);
}
int WINAPI i_release_dc(DWORD w, void *dc)
{
    rec(0xa264, w, ptrv(dc));
    return 1;
}
DWORD WINAPI i_get_long(DWORD w, int what)
{
    rec(0xa210, w, (DWORD)what);
    return ans();
}
DWORD WINAPI i_set_long(DWORD w, int what, DWORD v)
{
    rec(0xa20c, w, (DWORD)what, v);
    return ans();
}
DWORD WINAPI i_set_rect(int *r, int a, int b, int c, int d)
{
    rec(0xa268, (DWORD)a, (DWORD)b, (DWORD)c, (DWORD)d);
    r[0] = a, r[1] = b, r[2] = c, r[3] = d;
    return 1;
}
DWORD WINAPI i_get_menu(DWORD w)
{
    rec(0xa26c, w);
    return ans_chance(3) ? 0x3e0000 | ans_below(0x100) : 0;
}
DWORD WINAPI i_adjust(int *r, DWORD style, DWORD menu, DWORD ex_style)
{
    rec(0xa270, fnv(2166136261u, r, 16), style, menu, ex_style);
    r[0] -= (int)ans_below(10), r[1] -= (int)ans_below(30), r[2] += (int)ans_below(10), r[3] += (int)ans_below(10);
    return 1;
}
DWORD WINAPI i_set_pos(DWORD w, DWORD after, int x, int y, int cx, int cy, DWORD flags)
{
    rec(0xa274, w, after, (DWORD)x ^ (DWORD)y << 16, (DWORD)cx ^ (DWORD)cy << 16);
    log_event(0xa2740, flags, 0);
    return 1;
}
DWORD WINAPI i_spi(DWORD what, DWORD a, int *r, DWORD b)
{
    rec(0xa278, what, a, b);
    r[0] = (int)ans_below(100) - 20, r[1] = (int)ans_below(100) - 20, r[2] = 1024, r[3] = 740;
    return 1;
}

// The cursor
DWORD WINAPI i_client_to_screen(DWORD w, int *p)
{
    rec(0xa240, w, (DWORD)p[0], (DWORD)p[1]);
    p[0] += (int)ans_below(300) - 20;
    p[1] += (int)ans_below(300) - 20;
    return 1;
}
DWORD WINAPI i_set_cursor_pos(int x, int y)
{
    rec(0xa244, (DWORD)x, (DWORD)y);
    return ans();
}
DWORD WINAPI i_load_cursor(DWORD inst, DWORD name)
{
    rec(0xa204, inst, name);
    return 0xc0c00000 | ans_below(0x100);
}
DWORD WINAPI i_set_cursor(DWORD c)
{
    rec(0xa24c, c);
    return ans();
}

// The clipboard
BYTE *g_global;   // the global block's memory (in the block of fakes)
DWORD g_global_size;
DWORD WINAPI i_open_clip(DWORD w)
{
    rec(0xa1e8, w);
    return ans() & 1;
}
DWORD WINAPI i_close_clip()
{
    rec(0xa1e0);
    return ans();
}
DWORD WINAPI i_empty_clip()
{
    rec(0xa214);
    return ans();
}
DWORD WINAPI i_register_format(const char *name)
{
    rec(0xa1dc, text_hash(name));
    return 0xc000 | ans_below(0x100);
}
DWORD WINAPI i_set_clip(DWORD format, DWORD h)
{
    rec(0xa1d8, format, h);
    return ans();
}
DWORD WINAPI i_get_clip(DWORD format)
{
    rec(0xa1ec, format);
    return ans_chance(4) ? 0 : 0x6b0000 | ans_below(0x100);
}
DWORD WINAPI i_global_alloc(DWORD flags, DWORD n)
{
    if (foreign())
        return real<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea088)(flags, n);
    rec(0xa088, flags, n);
    return ans_chance(5) ? 0 : 0x6b0000 | ans_below(0x100);
}
BYTE *WINAPI i_global_lock(DWORD h)
{
    if (foreign())
        return real<BYTE *(WINAPI *)(DWORD)>(0x5ea084)(h);
    rec(0xa084, h);
    return g_global;
}
DWORD WINAPI i_global_unlock(DWORD h)
{
    if (foreign())
        return real<DWORD(WINAPI *)(DWORD)>(0x5ea080)(h);
    rec(0xa080, h);
    return ans();
}
DWORD WINAPI i_global_size(DWORD h)
{
    if (foreign())
        return real<DWORD(WINAPI *)(DWORD)>(0x5ea124)(h);
    rec(0xa124, h);
    return g_global_size;
}

// The wrappers, the font
DWORD WINAPI i_time()
{
    if (foreign())
        return real<DWORD(WINAPI *)()>(0x5ea294)();
    rec(0xa294);
    return ans();
}
DWORD WINAPI i_joy_devs()
{
    if (foreign())
        return real<DWORD(WINAPI *)()>(0x5ea298)();
    rec(0xa298);
    return ans_below(17);
}
DWORD WINAPI i_create_font(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike, DWORD charset,
                           DWORD out_precision, DWORD clip, DWORD quality, DWORD pitch, const char *face)
{
    rec(0xa028, (DWORD)h ^ (DWORD)w << 8 ^ (DWORD)esc << 16 ^ (DWORD)orient << 24, (DWORD)weight, italic ^ underline << 8 ^ strike << 16 ^ charset << 24,
        out_precision ^ clip << 8 ^ quality << 16 ^ pitch << 24);
    log_event(0xa0280, text_hash(face), 0);
    return 0xf0000 | ans_below(0x100);
}
DWORD WINAPI i_delete_object(DWORD o)
{
    rec(0xa024, o);
    return ans();
}

const Imp IMPORTS[] = {
    {0x5ea21c, (void *)&i_def_proc},      {0x5ea22c, (void *)&i_is_iconic},     {0x5ea1e4, (void *)&i_window_rect},
    {0x5ea218, (void *)&i_move_window},   {0x5ea220, (void *)&i_begin_paint},   {0x5ea224, (void *)&i_end_paint},
    {0x5ea228, (void *)&i_post_quit},     {0x5ea230, (void *)&i_peek},          {0x5ea280, (void *)&i_translate},
    {0x5ea234, (void *)&i_dispatch},      {0x5ea07c, (void *)&i_sleep},         {0x5ea260, (void *)&i_get_dc},
    {0x5ea030, (void *)&i_device_caps},   {0x5ea264, (void *)&i_release_dc},    {0x5ea210, (void *)&i_get_long},
    {0x5ea20c, (void *)&i_set_long},      {0x5ea268, (void *)&i_set_rect},      {0x5ea26c, (void *)&i_get_menu},
    {0x5ea270, (void *)&i_adjust},        {0x5ea274, (void *)&i_set_pos},       {0x5ea278, (void *)&i_spi},
    {0x5ea240, (void *)&i_client_to_screen}, {0x5ea244, (void *)&i_set_cursor_pos}, {0x5ea204, (void *)&i_load_cursor},
    {0x5ea24c, (void *)&i_set_cursor},    {0x5ea1e8, (void *)&i_open_clip},     {0x5ea1e0, (void *)&i_close_clip},
    {0x5ea214, (void *)&i_empty_clip},    {0x5ea1dc, (void *)&i_register_format}, {0x5ea1d8, (void *)&i_set_clip},
    {0x5ea1ec, (void *)&i_get_clip},      {0x5ea088, (void *)&i_global_alloc},  {0x5ea084, (void *)&i_global_lock},
    {0x5ea080, (void *)&i_global_unlock}, {0x5ea124, (void *)&i_global_size},   {0x5ea294, (void *)&i_time},
    {0x5ea298, (void *)&i_joy_devs},      {0x5ea028, (void *)&i_create_font},   {0x5ea024, (void *)&i_delete_object},
};

// ---- Exe stand-ins --------------------------------------------------------------------------------------------------

const Stub STUBS[] = {
    {0x518650, CV_THIS, 0, K_VALUE, "Cursor::Refresh"},
    {0x567cc0, CV_THIS, 0, K_BOOL, "ScreenFrame_IsFullScreen"},
    {0x5693a0, CV_THIS, 0, K_BOOL, "ScreenFrame_Flip"},
    {0x57aef0, CV_CDECL, 0, K_FIXED5, "ResourceCache_Get"},
    {0x57b210, CV_THIS, 0, K_VALUE, "ResourceCache_UnloadAll"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll"},
    {0x4e2980, CV_THIS, 1, K_VALUE, "Keys::ClearPresses"},
    {0x4e29b0, CV_THIS, 1, K_VALUE, "Keys::ClearDowns"},
    {0x4ce130, CV_THIS, 0, K_VALUE, "App_ResetTimers"},
    {0x599580, CV_FAST, 0, K_VALUE, "Input_VirtualKeyToGameKey", "vx"},
    {0x4e2a00, CV_THIS, 1, K_VALUE, "Keys::AddChar"},
    {0x4e29e0, CV_THIS, 1, K_VALUE, "Keys::Press"},
    {0x4e29f0, CV_THIS, 1, K_VALUE, "Keys::Release"},
    {0x51f450, CV_THIS, 0, K_VALUE, "Cursor::Buttons"},
    {0x4cdc70, CV_CDECL, 0, K_VALUE, "App_DefaultWidth"},
    {0x4cdc80, CV_CDECL, 0, K_VALUE, "App_DefaultHeight"},
    {0x5d033a, CV_CDECL, 1, 'n', "operator new"},
    {0x5d0863, CV_CDECL, 1, K_VALUE, "atexit"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// ---- The cases --------------------------------------------------------------------------------------------------------

constexpr int W_APP = 0, W_FRAME = 1, W_CACHE = 2;
enum TestId { T_PROC, T_PUMP, T_SIZE, T_CURSOR_POS, T_ARROW, T_HIDE, T_SET_TEXT, T_GET_TEXT, T_WRAPPERS, T_FONT, T_FONT_EXIT };
int g_test;
DWORD g_a[4];
BYTE *g_data;

void build(Gen &g)
{
    note_stack();
    g_case.nwin = 3;
    g_case.object_args = APP_ARGS;
    make_window(W_APP, C_OBJECT);
    make_window(W_FRAME, C_CFRAME);
    make_window(W_CACHE, C_CALLBACK);   // the resource cache: slot 9 without arguments
    BYTE *app = win(W_APP);
    at_(app, 4) = g.chance(4) ? 0 : ptrv(win(W_FRAME));
    app[0x28] = (BYTE)g.below(2);
    g_case.fixed[5] = ptrv(win(W_CACHE));
    case_global(REPORTER, g.chance(4) ? 0 : ptrv(app));
    case_global(WINDOW, 0x77770000u | g.below(0x100));
    case_global(ACTIVE, g.below(2));
    case_global(MINIMIZED, g.chance(3) ? 1 : 0);
    case_global(PUMPING, g.chance(8) ? 1 : 0);
    case_global(JOY_MESSAGE, 0xc100 | g.below(0x10));
    case_global(WHEEL_MESSAGE, 0xc110 | g.below(0x10));
    for (DWORD a = 0x6cff50; a <= 0x6cff5c; a += 4)
        case_global(a, g.chance(2) ? g.below(8) : g.next());
    const float wheel = (float)g.range(-500, 500) / 7.0f;
    DWORD wb;
    memcpy(&wb, &wheel, 4);
    case_global(0x6cff60, wb);
    g_a[0] = g_a[1] = g_a[2] = g_a[3] = 0;
    switch (g_test) {
    case T_PROC: {
        const DWORD msgs[] = {2, 3, 5, 6, 0xf, 0x10, 0x12, 0x1c, 0x20, 0x104, 0x112, 0x30f, 0x310, 0, 1, 4, 0x11, 0x100, 0x105,
                              0x200, 0x113, 0x400, 0x30e, 0x311, 0xc100};
        g_a[0] = 0x77770011;
        g_a[1] = g.chance(15) ? 0xc100 + g.below(0x20) : msgs[g.below(sizeof msgs / 4)];
        const DWORD keys[] = {0x12, 0x20, 0x5b, 0x5c, 0x5d, 0x79, 0x41, 0x1b, 0x73};
        const DWORD kind = g.below(5);
        g_a[2] = kind == 0 ? g.next() : kind == 1 ? g.below(3) | (g.chance(2) ? g.below(2) << 16 : 0)
               : kind == 2 ? keys[g.below(9)] | (g.next() & 0xffff00) : kind == 3 ? 0xf100 + (g.chance(2) ? 0 : g.below(0x100)) : g.below(3);
        g_a[3] = g.chance(2) ? 1 | (g.next() & 0xffff0000) : g.next();
        break;
    }
    case T_PUMP: {
        const DWORD ids[] = {0x7b, 0x100, 0x101, 0x102, 0x104, 0x105, 0x106, 0x200, 0x201, 0x202, 0x203, 0x204, 0x205, 0x206,
                             0x207, 0x208, 0x209, 0x20a, 0x0f, 0x113, 0x103, 0x20b, 0x210};
        g_nqueue = (int)g.below(MAXQ + 1);
        g_next = 0;
        for (int i = 0; i < g_nqueue; i++) {
            g_queue[i][0] = g.chance(10) ? 0xc110 + g.below(0x10) : ids[g.below(sizeof ids / 4)];
            g_queue[i][1] = g.chance(2) ? g.next() : g.below(0x100) | (DWORD)(g.range(-5, 5) * 120) << 16;
            g_queue[i][2] = g.next();
        }
        break;
    }
    case T_CURSOR_POS:
        g_a[0] = (DWORD)g.range(-100, 900);
        g_a[1] = (DWORD)g.range(-100, 700);
        break;
    case T_SET_TEXT:
    case T_GET_TEXT: {
        const char *names[] = {"Text", "text", "TEXT", "Zax Objects", "", "Texts"};
        const char *nm = names[g.below(6)];
        char *copy = reinterpret_cast<char *>(xalloc(16));
        lstrcpyA(copy, nm);
        g_a[0] = ptrv(copy);
        g_global_size = g.below(0x300);
        g_global = xalloc(0x400);
        g_data = xalloc(0x400);
        for (DWORD i = 0; i < 0x400; i++)
            g_data[i] = (BYTE)g.next(), g_global[i] = (BYTE)(i * 13);
        g_a[1] = ptrv(g_data);
        g_a[2] = g_global_size;
        if (g_test == T_GET_TEXT) {
            g_a[1] = g.chance(4) ? 0 : ptrv(xalloc(4));
            memcpy(g_global, g_data, 0x400);
        } else if (g.chance(5)) {
            g_global = nullptr;   // GlobalLock fails
        }
        break;
    }
    case T_FONT:
    case T_FONT_EXIT:
        for (DWORD a = 0x6c0468; a <= 0x6c0480; a += 4)
            case_global(a, g.chance(3) ? 0 : g.next());
        break;
    default: break;
    }
}

DWORD call_side()
{
    switch (g_test) {
    case T_PROC: return call_this(0x599d50, nullptr, 4, g_a);
    case T_PUMP: return call_this(0x5998f0, nullptr, 0, nullptr), 0;
    case T_SIZE: return call_this(0x477b60, nullptr, 0, nullptr), 0;
    case T_CURSOR_POS: return call_fast(0x518820, g_a[0], g_a[1], 0, nullptr);
    case T_ARROW: return call_this(0x518860, nullptr, 0, nullptr);
    case T_HIDE: return call_this(0x518880, nullptr, 0, nullptr);
    case T_SET_TEXT: return call_fast(0x46d590, g_a[0], g_a[1], 1, g_a + 2);
    case T_GET_TEXT: return call_fast(0x46d620, g_a[0], g_a[1], 0, nullptr);
    case T_WRAPPERS: {
        const DWORD a = at<DWORD(__cdecl *)()>(0x4d2460)(), b = at<DWORD(__cdecl *)()>(0x4d24b0)(), c = at<DWORD(__cdecl *)()>(0x4dd920)();
        log_event(0xe460, a, b);
        return c;
    }
    case T_FONT: return call_this(0x4d20d0, nullptr, 0, nullptr);
    case T_FONT_EXIT: return call_this(0x4d2140, nullptr, 0, nullptr);
    default: return 0;
    }
}

DWORD call()
{
    imports_on(IMPORTS, sizeof IMPORTS / sizeof IMPORTS[0]);
    const DWORD r = run_escapable(&call_side);
    imports_off();
    if (g_test == T_GET_TEXT && in_block(r))   // the copy (a block of operator new's stand-in): its contents
        log_event(0xe620, fnv(2166136261u, at<void *>(r), g_global_size), r - ptrv(&g_block));
    return r;
}

struct TestDef {
    const char *name;
    int count;
};
// (about 4 ms per case on the VMs; the module stays within the 10 s budget)
const TestDef TESTS[] = {{"App_WindowProc", 900},  {"App_PumpMessages", 300},  {"App_SetWindowedSize", 200},
                         {"Cursor::SetWindowsPos", 60}, {"Cursor::ShowArrow", 20}, {"Cursor::HideWindows", 20},
                         {"Clipboard_SetText", 200}, {"Clipboard_GetText", 200}, {"API wrappers", 40},
                         {"StaticInit_GdiFont", 30}, {"StaticExit_GdiFont", 30}};

}  // namespace

ZAX_CHECKS(app_window)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        const DWORD dwords[] = {REPORTER, WINDOW, JOY_MESSAGE, WHEEL_MESSAGE, 0x6cff50, 0x6cff54, 0x6cff58, 0x6cff5c, 0x6cff60, 0x6c0468,
                                0x6c046c, 0x6c0470, 0x6c0474, 0x6c0478, 0x6c0480};
        for (DWORD a : dwords)
            g_globals.add(a, 4);
        const DWORD bytes[] = {ACTIVE, MINIMIZED, PUMPING};
        for (DWORD a : bytes)
            g_globals.add(a, 1);
    }
    Tally t{"app_window: window procedure, message pump, window size, cursor, clipboard, API wrappers, GDI font"};
    const Check c{&g_hooks, &g_globals};
    stubs_on(STUBS, NSTUBS);
    for (int i = 0; i < (int)(sizeof TESTS / sizeof TESTS[0]); i++) {
        g_test = i;
        const int count = scaled(TESTS[i].count);
        for (int k = 0; k < count; k++)
            run_case(t, c, TESTS[i].name, 52000u + (unsigned)(i * 10007 + k), &build, &call);
    }
    stubs_off();
    t.report();
}

}  // namespace zax
