// Checks of the mouse cursor object (ui_cursor.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): a cursor of the case in the extra area (never the game's 0x6cff40), its image a fake
// sequence of this file (slot 23 a frame: a fake CFrame, window 10; slot 25 the frame count; slot 26 the frames
// per second in st0), the frames fakes. Windows' cursor (0x518820, 0x518860, 0x518880), the blit and the
// sequence's blend mode are stand-ins.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_cursor(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_cursor", &hooks_ui_cursor};

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x518820, CV_FAST, 0, K_VALUE, "Cursor_SetWindowsPos", "vv"},
    {0x518860, CV_CDECL, 0, K_VALUE, "Cursor_WindowsArrow"},
    {0x518880, CV_CDECL, 0, K_VALUE, "Cursor_WindowsNone"},
    {0x56bf00, CV_THIS, 0, K_VALUE, "Sequence_BlendMode"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_FRAME_ = 10, W_IMAGE_FRAME = 11;

struct Seq {
    DWORD vt;
    DWORD count;
    float fps;
    DWORD frames;   // slot 23 answers a frame (window 11) or null
};
DWORD g_seq_vt[30];
DWORD THISCALL seq_frame(BYTE *s, int i)
{
    log_event(0x717, (DWORD)i, 0);
    return reinterpret_cast<Seq *>(s)->frames ? ptrv(win(W_IMAGE_FRAME)) : 0;
}
DWORD THISCALL seq_count(BYTE *s)
{
    log_event(0x719, 0, 0);
    return reinterpret_cast<Seq *>(s)->count;
}
long double THISCALL seq_fps(BYTE *s)
{
    log_event(0x71a, 0, 0);
    return reinterpret_cast<Seq *>(s)->fps;
}
DWORD THISCALL seq_other(BYTE *s)
{
    log_event(0x71f, ptrv(s), 0);
    return 0;
}
void init_vtables()
{
    for (int i = 0; i < 30; i++)
        g_seq_vt[i] = ptrv((void *)&seq_other);
    g_seq_vt[23] = ptrv((void *)&seq_frame);
    g_seq_vt[25] = ptrv((void *)&seq_count);
    g_seq_vt[26] = ptrv((void *)&seq_fps);
}

float some_float(Gen &g)
{
    switch (g.below(7)) {
    case 0: return 0.0f;
    case 1: {
        const DWORD nan = 0x7fc00000;
        float f;
        memcpy(&f, &nan, 4);
        return f;
    }
    case 2: return (float)g.range(-3, 30);
    default: return (float)g.range(-300, 3000) / 97.0f;
    }
}

// The image frame's slot 36 writes its rectangle
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_CFRAME && slot == 36) {
        int *r = at<int *>(args[0]);
        r[0] = (int)(answer & 0x1f) - 8, r[1] = (int)((answer >> 5) & 0x1f) - 8;
        r[2] = r[0] + 16 + (int)((answer >> 10) & 0x1f), r[3] = r[1] + 16 + (int)((answer >> 15) & 0x1f);
    }
}

void base(Gen &g)
{
    g_case.effect = &effect;
    make_window(W_FRAME_, C_CFRAME);
    make_window(W_IMAGE_FRAME, C_CFRAME);
    Seq *s = reinterpret_cast<Seq *>(xalloc(sizeof(Seq)));
    s->vt = ptrv(g_seq_vt);
    s->count = g.chance(4) ? g.below(2) : g.below(20);
    s->fps = some_float(g);
    s->frames = g.chance(5) ? 0 : 1;
    BYTE *c = xalloc(0x30);
    at_(c, 4) = g.chance(4) ? 0 : ptrv(s);
    at_<float>(c, 8) = some_float(g);
    c[0xc] = g.chance(2), c[0xd] = g.chance(2);
    at_(c, 0x10) = (DWORD)g.range(-20, 700), at_(c, 0x14) = (DWORD)g.range(-20, 500);
    at_(c, 0x18) = g.below(8), at_(c, 0x1c) = g.below(8);
    at_<float>(c, 0x20) = some_float(g);
    g_self = c;
    g_args[0] = ptrv(xalloc(16));
    g_args[1] = ptrv(xalloc(4));
}

void s_none(Gen &) {}
void s_mask(Gen &g) { g_args[0] = g.below(8); }
void s_xy(Gen &g)
{
    g_args[0] = (DWORD)g.range(-50, 900);
    g_args[1] = (DWORD)g.range(-50, 700);
}
void s_image(Gen &g)
{
    if (at_(g_self, 4) && g.chance(2)) {
        g_args[0] = at_(g_self, 4);
        return;
    }
    Seq *s = reinterpret_cast<Seq *>(xalloc(sizeof(Seq)));   // (slot 25 is called on the new image: never null)
    s->vt = ptrv(g_seq_vt);
    s->count = g.below(20);
    s->fps = some_float(g);
    s->frames = 1;
    g_args[0] = ptrv(s);
}
void s_on(Gen &g) { g_args[0] = g.chance(3) ? g.next() : g.below(2); }
void s_dt(Gen &g)
{
    const float dt = some_float(g);
    memcpy(&g_args[0], &dt, 4);
}
void s_frame(Gen &) { g_args[0] = ptrv(win(W_FRAME_)); }

const Test TESTS[] = {
    {"Cursor GetXY", 0x5184e0, CALL_THIS, 2, 0, 20, s_none, STUBS, NSTUBS},
    {"Cursor GetPos", 0x518500, CALL_THIS, 1, 0, 20, s_none, STUBS, NSTUBS},
    {"Cursor KeepWheelFraction", 0x518520, CALL_THIS, 0, ~0u, 100, s_none, STUBS, NSTUBS},   // eax: the steps (6-13)
    {"Cursor TakePress", 0x518540, CALL_THIS, 1, 0xff, 60, s_mask, STUBS, NSTUBS},
    {"Cursor SetPos", 0x518570, CALL_THIS, 2, 0, 30, s_xy, STUBS, NSTUBS},
    {"Cursor SetImage", 0x518590, CALL_THIS, 1, ~0u, 150, s_image, STUBS, NSTUBS},
    {"Cursor ArrowOn", 0x5185d0, CALL_THIS, 0, 0, 10, s_none, STUBS, NSTUBS},
    {"Cursor ArrowOff", 0x5185e0, CALL_THIS, 0, 0, 10, s_none, STUBS, NSTUBS},
    {"Cursor Hide", 0x5185f0, CALL_THIS, 0, 0xff, 40, s_none, STUBS, NSTUBS},
    {"Cursor Show", 0x518620, CALL_THIS, 0, 0xff, 40, s_none, STUBS, NSTUBS},
    {"Cursor Refresh", 0x518650, CALL_THIS, 0, 0, 40, s_none, STUBS, NSTUBS},
    {"Cursor SetShown", 0x5186a0, CALL_THIS, 1, 0xff, 80, s_on, STUBS, NSTUBS},
    {"Cursor Advance", 0x518700, CALL_THIS, 1, 0, 300, s_dt, STUBS, NSTUBS},
    {"Cursor Draw", 0x518760, CALL_THIS, 1, 0, 100, s_frame, STUBS, NSTUBS},
    {"Cursor Rect", 0x5187b0, CALL_THIS, 1, 0xff, 100, s_none, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_cursor)
{
    (void)cls;
    (void)n;
    init_vtables();
    Tally t{"ui cursor: the mouse cursor object"};
    run_tests(t, g_hooks, nullptr, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 40000);
    t.report();
}

}  // namespace zax
