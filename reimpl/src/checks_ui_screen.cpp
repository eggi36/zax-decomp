// Checks of the screen window and of the window base class's construction and destruction (ui_window.cpp)
// against the original, with the toolkit's harness (checks_ui_common.h):
//  - CScreenWindow's tool tip, key repeat, cursor and mouse functions on a fake screen window (the real font,
//    the cursor and keyboard objects and the time recorded);
//  - the constructors and destructors of CJWindow and CScreenWindow on real objects in the block (their own
//    vtables, the allocators as stand-ins so that everything they make lies in the block too; parent, children,
//    textures and the frame are recording fakes).
// Not covered: the first-time branch of the init (textures, sounds and bevel colours of the first window; it ran
// before the checks) and the dirty-rect cache of the destructor (made by the drawing, Rendering).
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_window(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_window", &hooks_ui_window};
Globals g_globals;

void init_globals()
{
    if (g_globals.n)
        return;
    g_globals.add(0x62e5e0, 1);
    g_globals.add(0x60a528, 4);
    g_globals.add(0x60a52c, 1);
    g_globals.add(0x62e5c4, 1);   // in a destructor
    g_globals.add(0x62e5d0, 4);   // the default font
}

// ---- The stand-ins --------------------------------------------------------------------------------------------

const Stub S_TIME[] = {{0x4d2460, CV_CDECL, 0, K_VALUE, "time"}};
const Stub S_CURSOR[] = {
    {0x518620, CV_THIS, 0, K_VALUE, "Cursor::Show"},
    {0x5185f0, CV_THIS, 0, K_VALUE, "Cursor::Hide"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll"},
    {0x4e2760, CV_THIS, 1, K_VALUE, "Keys::ClearChars"},
};
const Stub S_MOUSE[] = {
    {0x5184e0, CV_THIS, 2, K_VALUE, "Cursor::GetPos", "xx"},
    {0x518590, CV_THIS, 1, K_VALUE, "Cursor::Set"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "time"},
};
const Stub S_LIFE[] = {
    UIC_ALLOC_STUBS,
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vssv"},
    {0x518620, CV_THIS, 0, K_VALUE, "Cursor::Show"},
    {0x5186a0, CV_THIS, 1, K_VALUE, "Cursor::Restore", "b"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll"},
    {0x4e2760, CV_THIS, 1, K_VALUE, "Keys::ClearChars"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "time"},
};
constexpr int N_LIFE = sizeof S_LIFE / sizeof S_LIFE[0];

// ---- The effects: output arguments ---------------------------------------------------------------------------

DWORD g_last_pos[2];   // the screen's last mouse position (GetPos answers it now and then)

void effect(int cls, int slot, DWORD *args, DWORD)
{
    Gen &v = g_case.values;
    if (cls == -1 && slot == 0x5184e0) {   // Cursor::GetPos(&x, &y)
        const bool same = v.chance(3);
        if (readable(at<void *>(args[0]), 4))
            *at<int *>(args[0]) = same ? (int)g_last_pos[0] : v.range(-50, 700);
        if (readable(at<void *>(args[1]), 4))
            *at<int *>(args[1]) = same ? (int)g_last_pos[1] : v.range(-50, 500);
    } else if (cls == C_SCREEN && slot == 13 && readable(at<void *>(args[2]), 4)) {   // the cursor out
        *at<DWORD *>(args[2]) = v.chance(2) ? 0 : v.next();
    } else if (cls == C_CFRAME && slot == 36 && readable(at<void *>(args[0]), 16)) {   // the frame's rect
        int *r = at<int *>(args[0]);
        r[0] = v.range(-5, 5), r[1] = v.range(-5, 5), r[2] = v.range(0, 1024), r[3] = v.range(0, 768);
    }
}

// ---- The fake screen window -----------------------------------------------------------------------------------

const char *const TIPS[] = {"", "Save the game", "Load &Game", "Two\nlines",
                            ("A long tip text that wraps around at the edge of the screen, with more words than fit on "
                             "one line of the tool tip window")};

void base(Gen &g)
{
    g_case.effect = &effect;
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    BYTE *s = make_plain_window(0, C_SCREEN, g);
    at_<int>(s, 4) = 0, at_<int>(s, 8) = 0;
    at_<int>(s, 0xc) = g.chance(3) ? g.range(100, 1024) : 640, at_<int>(s, 0x10) = g.chance(3) ? g.range(100, 768) : 480;
    s[0x108] = (BYTE)g.below(2);
    at_(s, 0x10c) = g.next();
    s[0x110] = (BYTE)g.next();
    at_(s, 0x114) = ptrv(real_font());
    make_text(s + 0x118, g.chance(2) ? "" : TIPS[g.below(5)]);
    make_text(s + 0x11c, "");
    for (DWORD o = 0x120; o < 0x140; o += 4)
        at_(s, o) = (DWORD)g.range(-20, 700);
    g_last_pos[0] = (DWORD)g.range(-50, 700), g_last_pos[1] = (DWORD)g.range(-50, 500);
    at_(s, 0x140) = g_last_pos[0], at_(s, 0x144) = g_last_pos[1];
    for (int e = 0; e < 3; e++) {
        BYTE *en = s + 0x148 + 0x10 * e;
        at_(en, 0) = g.chance(3) ? 0 : ptrv(win((int)g.below(NWIN)));
        at_(en, 4) = g.next(), at_(en, 8) = g.below(1000), en[0xc] = (BYTE)g.below(2);
    }
    at_(s, 0x178) = g.below(3);
    g_self = s;
}

void s_out(Gen &) { g_args[0] = ptrv(xalloc(16)); }
void s_repeat(Gen &g)
{
    BYTE *e = g_self + 0x148 + (at_(g_self, 0x178) << 4);
    g_args[0] = g.chance(3) ? at_(e, 0) : ptrv(win((int)g.below(NWIN)));
    g_args[1] = g.chance(2) ? 0 : g.below(2000);
    g_args[2] = g.chance(2) ? 0 : g.below(500);
}
void s_none(Gen &) {}
// KeyRepeatDue: in a third of the cases an entry index of 16..31 (still inside the 0x400 block), so the entry's
// offset has upper bytes and eax shows them (the plain byte merge lost them, docs/DECOMPILATION.md section 13,
// "Batch section-12 leftovers")
void s_repeat_due(Gen &g)
{
    if (!g.chance(3))
        return;
    at_(g_self, 0x178) = 16 + g.below(16);
    g_self[0x154 + (at_(g_self, 0x178) << 4)] = (BYTE)g.below(2);
}
void s_tip(Gen &g)
{
    g_args[0] = ptrv(real_font());
    char *t = reinterpret_cast<char *>(xalloc(200));
    const char *src = TIPS[g.below(5)];
    if (g.chance(4))   // the text the tip shows already
        src = *reinterpret_cast<const char *const *>(g_self + 0x118) + 8;
    lstrcpyA(t, src);
    g_args[1] = g.chance(10) ? 0 : ptrv(t);
    g_args[2] = g.chance(2) ? (DWORD)-1 : (DWORD)g.range(-30, 700);
    g_args[3] = (DWORD)g.range(-30, 500);
}
void s_tip_accel(Gen &g)
{
    s_tip(g);
    g_args[4] = g_args[3], g_args[3] = g_args[2], g_args[2] = g_args[1];
    g_args[1] = g.chance(2) ? (DWORD)-1 : g.below(6);
}

// The constructors set the six colours from colours built on the stack whose palette index byte (+0xa) was never
// set: those bytes of the window (+0x8a ... +0xda) are left over from the stack (in the original as here)
void clear_colour_garbage(BYTE *w)
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        w[o] = 0;
}
void clear_garbage() { clear_colour_garbage(g_self); }

// ---- Construction and destruction ------------------------------------------------------------------------------

// The object under test lies in the extra area; window 0 is its parent
void s_ctor(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    g_self = obj;
    g_args[0] = g.chance(4) ? 0 : ptrv(win(0));
    char *t = reinterpret_cast<char *>(xalloc(64));
    lstrcpyA(t, TEXTS[g.below(8)]);
    g_args[1] = g.chance(5) ? 0 : ptrv(t);
    if (g.chance(3))
        at_(win(0), 0x4c) = 0;   // a parent without a look: the factory's
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
void s_ctor_string(Gen &g)
{
    s_ctor(g);
    BYTE *z = xalloc(4);
    make_text(z, g_args[1] ? at<const char *>(g_args[1]) : "");
    g_args[1] = ptrv(z);
}
// A window built by the original constructor, with fake children, a scroll info, textures and an INI section
void built_window(Gen &g)
{
    BYTE *obj = xalloc(0x200);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x40a8a0)(obj, win(0), TEXTS[g.below(8)]);
    clear_colour_garbage(obj);
    at_(obj, 0xe8) = g.chance(4) ? 0 : ptrv(win(0));
    const int kids = (int)g.below(4);
    for (int k = 0; k < kids; k++)
        link_child(obj, win(2 + k));
    if (g.chance(2))
        make_scroll(obj, g.range(0, 50), g.range(0, 50), g.range(0, 500), g.range(0, 500), g.chance(2), nullptr, nullptr);
    at_(obj, 0xe0) = g.chance(3) ? ptrv(win(8)) : 0;
    at_(obj, 0xe4) = g.chance(3) ? ptrv(win(9)) : 0;
    at_(obj, 0x54) = g.chance(4) ? ptrv(TEXTS[1 + g.below(7)]) : 0;
    obj[0x58] = (BYTE)g.below(2);
    g_self = obj;
}
void s_dtor(Gen &g) { built_window(g); }
void s_deleting(Gen &g)
{
    built_window(g);
    g_args[0] = g.below(4);
}
void s_screen_ctor(Gen &g)
{
    BYTE *obj = xalloc(0x180);
    g_self = obj;
    BYTE *frame = make_window(10, C_CFRAME);
    g_case.argspec[36][0] = A_IGNORE;   // the frame's slot 36 writes into a local of the constructor
    g_args[0] = ptrv(frame);
    g_args[1] = g.chance(2) ? 0 : ptrv(real_font());
    case_global(0x60a52c, g.chance(4) ? 0xff : g.below(12));
}
void s_screen_dtor(Gen &g)
{
    s_screen_ctor(g);
    original<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x408f20)(g_self, win(10), g_args[1]);
    clear_colour_garbage(g_self);
    const int kids = (int)g.below(3);
    for (int k = 0; k < kids; k++)
        link_child(g_self, win(2 + k));
    g_args[0] = g.below(4);
    g_args[1] = 0;
}

// The objects the constructor tests built, destroyed again (their parts may be known elsewhere)
void destroy_window() { original<void(THISCALL *)(BYTE *)>(0x40b190)(g_self); }

void destroy_screen() { original<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x409040)(g_self, 0); }

const Test TESTS[] = {
    {"Screen TipRect", 0x409290, CALL_THIS, 1, 0xff, 300, s_out},
    {"Screen StartKeyRepeat", 0x409330, CALL_THIS, 3, ~0u, 400, s_repeat, S_TIME, 1},
    {"Screen KeyRepeatDue", 0x4093c0, CALL_THIS, 0, ~0u, 100, s_repeat_due},
    {"Screen ShowCursor", 0x409870, CALL_THIS, 0, 0, 100, s_none, S_CURSOR, 4},
    {"Screen HideCursor", 0x4098b0, CALL_THIS, 0, 0, 100, s_none, S_CURSOR, 4},
    {"Screen ShowTip", 0x4098d0, CALL_THIS, 4, 0, 400, s_tip, S_MOUSE, 3},
    {"Screen ShowTipAccel", 0x4098f0, CALL_THIS, 5, 0, 600, s_tip_accel, S_MOUSE, 3},
    {"Screen MouseUpdate", 0x409b30, CALL_THIS, 0, 0, 400, s_none, S_MOUSE, 3},
    {"Window ctor", 0x40a8a0, CALL_THIS, 2, ~0u, 300, s_ctor, S_LIFE, N_LIFE, destroy_window, clear_garbage},
    {"Window ctor ZString", 0x40a940, CALL_THIS, 2, ~0u, 200, s_ctor_string, S_LIFE, N_LIFE, destroy_window, clear_garbage},
    {"Window dtor", 0x40b190, CALL_THIS, 0, 0, 300, s_dtor, S_LIFE, N_LIFE},
    {"Window deleting dtor", 0x40a910, CALL_THIS, 1, ~0u, 200, s_deleting, S_LIFE, N_LIFE},
    {"Screen ctor", 0x408f20, CALL_THIS, 2, ~0u, 200, s_screen_ctor, S_LIFE, N_LIFE, destroy_screen, clear_garbage},
    {"Screen deleting dtor", 0x409040, CALL_THIS, 1, ~0u, 200, s_screen_dtor, S_LIFE, N_LIFE},
};

}  // namespace

ZAX_CHECKS(ui_screen)
{
    (void)cls;
    (void)n;
    init_globals();
    Tally t{"ui screen: CScreenWindow, CJWindow construction and destruction"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 7700);
    t.report();
}

}  // namespace zax
