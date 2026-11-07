// Checks of the toolkit's last functions (ui_rest4a.cpp) against the original with the toolkit's harness
// (checks_ui_common.h).
// - DrawChildren on a random tree of recorded windows (window 0 and its children; random rectangles, flags,
//   scroll infos), the frame a recorded COffsetClippedProxyFrame (its offset, clip and target slots
//   answered by the case; the clip rectangles it hands out filled), some children with a window cache: its
//   clip frame and recording frame recorded proxy frames, the recording's stream a recorded object whose reads
//   (slot 3) hand out the case's number of replay functions (a recording function) and then 0; the frame
//   helpers, the memory stream's compare and copy, the cursor's and tool tip's rectangles, the window root
//   and the dirty list stand-ins; the globals of the cache (0x60a524, 0x62e5c4, 0x62e5e0, 0x60a528 and the
//   dirty rectangle 0x62e578) case values.
// - UpdateScrollBars on a recorded window with a scroll info of random content sizes and existing or missing
//   recorded scroll bars (with random range and position floats); new bars come from the allocator stand-in
//   and become recorded scroll bars in CJScrollBar's stand-in.
// - RunModal with a recorded window and frame, the error reporter a fake (its screen the frame or another),
//   the screen window recorded (made in CScreenWindow's stand-in), its "open" slot answered by the case, the
//   tick a stand-in, a recording callback or none.
// - The recording frame's destructors, Replay and Terminate on fakes; the detail renderer's destructor.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_rest4a(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_rest4a", &hooks_ui_rest4a};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x482de0, CV_THIS, 0, K_FIXED1, "ProxyFrame_Target"},
    {0x524fe0, CV_THIS, 2, K_VALUE, "COffsetClippedProxyFrame_SetOffset", "vv"},
    {0x524f50, CV_THIS, 1, K_VALUE, "COffsetClippedProxyFrame_SetClip", "r"},
    {0x506350, CV_THIS, 0, K_VALUE, "MemoryStream_Clear"},
    {0x5062d0, CV_THIS, 1, K_BOOL, "MemoryStream_Equals", "v"},
    {0x506320, CV_THIS, 1, K_VALUE, "MemoryStream_CopyFrom", "v"},
    {0x46db90, CV_THIS, 1, K_VALUE, "ClipFrame_SetClip", "r"},
    {0x4ee7f0, CV_THIS, 4, K_VALUE, "Rect_Set", "vvvv"},
    {0x5187b0, CV_THIS, 1, K_BOOL, "Cursor::Rect", "x"},
    {0x40bac0, CV_THIS, 1, K_VALUE, "CJWindow::WindowCache_AddDirty", "r"},
    {0x40cc70, CV_THIS, 0, K_WINDOW_NN, "CJWindow::Root"},
    {0x409290, CV_THIS, 1, K_BOOL, "CScreenWindow::TipRect", "x"},
    {0x412ea0, CV_THIS, 2, 'e', "CJScrollBar::CJScrollBar", "vv"},
    {0x413ce0, CV_THIS, 1, K_VALUE, "CJScrollBar::SetRangeFrom", "v"},
    {0x413bb0, CV_THIS, 2, K_VALUE, "CJScrollBar::SetRange", "vv"},
    {0x413c20, CV_THIS, 2, K_VALUE, "CJScrollBar::SetPos", "vv"},
    {0x4173f0, CV_THIS, 0, K_VALUE, "Object_FreeFields"},
    {0x4166d0, CV_THIS, 0, K_VALUE, "CObject_Dtor"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

const Stub MODAL_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x567bb0, CV_THIS, 0, K_VALUE, "ScreenFrame_Lock"},
    {0x567c90, CV_THIS, 0, K_VALUE, "ScreenFrame_Unlock"},
    {0x5693a0, CV_THIS, 0, K_VALUE, "ScreenFrame_Flip"},
    {0x4ab5d0, CV_THIS, 1, K_VALUE, "CFrame_CopyFrom", "v"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize", "vvv"},
    {0x55d030, CV_CDECL, 0, K_SMALL, "ColorDepth"},
    {0x408f20, CV_THIS, 2, 'e', "CScreenWindow::CScreenWindow", "vv"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "Platform_Ticks"},
    {0x5998f0, CV_CDECL, 0, K_VALUE, "App_PumpMessages"},
    {0x518700, CV_THIS, 1, K_VALUE, "Cursor::Advance", "v"},
    {0x518760, CV_THIS, 1, K_VALUE, "Cursor::Draw", "v"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt", "avvv"},
};
constexpr int NMODAL = sizeof MODAL_STUBS / sizeof MODAL_STUBS[0];

constexpr int W_FRAME = 10, W_TARGET = 11, W_STREAM = 12, W_OBJ = 13, W_BAR_V = 14, W_BAR_H = 15;

int g_reads;
void __fastcall replay_fn(BYTE *self) { log_event(0x7b01, ptrv(self), (DWORD)g_reads); }
void __cdecl modal_callback() { log_event(0x7b02, 0, 0); }

void fill_rect(DWORD out, Gen &g)
{
    int *r = at<int *>(out);
    r[0] = g.range(-20, 300), r[1] = g.range(-20, 200), r[2] = r[0] + g.range(-5, 200), r[3] = r[1] + g.range(-5, 150);
}
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if ((cls == C_PROXY_FRAME && slot == 36) || (cls == -1 && ((DWORD)slot == 0x5187b0 || (DWORD)slot == 0x409290))) {
        Gen g{g_case.seed * 31u + (DWORD)g_case.answers};
        fill_rect(args[0], g);
    }
    if (cls == C_OBJECT && slot == 3)   // a stream read: the next replay function or the end
        *at<DWORD *>(args[0]) = g_reads++ < (int)g_case.fixed[0] ? ptrv((void *)&replay_fn) : 0;
    if (cls == -1 && (DWORD)slot == 0x412ea0)   // a new scroll bar becomes a recorded one
        at_(at<void *>(g_effect_ecx), 0) = at_(win(W_BAR_V), 0);
    if (cls == -1 && (DWORD)slot == 0x408f20) {   // the modal screen
        at_(at<void *>(g_effect_ecx), 0) = at_(win(W_OBJ), 0);
        at_(at<void *>(g_effect_ecx), 0x10c) = g_case.fixed[2];
    }
}

void base(Gen &)
{
    g_reads = 0;
    g_case.effect = &effect;
}
// The recording frame's stream (a game object): slot 2 writes (data, size), 3 reads (data, size), 5 seeks
char g_object_args[MAXSLOT + 1];
void stream_args()
{
    memcpy(g_object_args, CLASSES[C_OBJECT].args, MAXSLOT);
    g_object_args[2] = '2';
    g_object_args[3] = '2';
    g_object_args[5] = '1';
    g_case.object_args = g_object_args;
}

BYTE *scroll_info(Gen &g, bool bars)
{
    BYTE *si = xalloc(0x1c);
    at_(si, 0) = (DWORD)g.range(-50, 300);
    at_(si, 4) = (DWORD)g.range(-50, 300);
    at_(si, 8) = (DWORD)g.range(0, 900);
    at_(si, 0xc) = (DWORD)g.range(0, 900);
    if (bars) {
        at_(si, 0x14) = g.chance(2) ? ptrv(win(W_BAR_V)) : 0;
        at_(si, 0x18) = g.chance(2) ? ptrv(win(W_BAR_H)) : 0;
    }
    return si;
}
BYTE *bar(int i, Gen &g)
{
    BYTE *b = make_plain_window(i, C_SCROLLBAR, g);
    static const DWORD FLOATS[] = {0x144, 0x148, 0x14c};
    for (DWORD o : FLOATS)
        at_<float>(b, o) = g.chance(3) ? (float)g.range(-5, 900) : (float)g.range(-3000, 30000) / 13.0f;
    return b;
}

void s_draw(Gen &g)
{
    const int n = 1 + (int)g.below(8);
    make_tree(g, n, C_WINDOW, C_WINDOW);
    BYTE *w = win(0);
    BYTE *frame = make_window(W_FRAME, C_PROXY_FRAME);
    make_window(W_TARGET, C_PROXY_FRAME);
    make_window(W_STREAM, C_OBJECT);
    bar(W_BAR_V, g);
    bar(W_BAR_H, g);
    g_case.nwin = n;
    g_case.fixed[0] = g.below(4);
    g_case.fixed[1] = ptrv(win(W_TARGET));
    set_kind(C_PROXY_FRAME, 43, K_SMALL);
    set_kind(C_PROXY_FRAME, 44, K_SMALL);
    g_case.argspec[36][0] = A_IGNORE;   // the clip rectangle the frame fills (the caller's uninitialised local)
    stream_args();
    for (int i = 0; i < n; i++) {
        BYTE *c = win(i);
        at_(c, 0x1c) = g.chance(5) ? (DWORD)g.range(-3, 0) : (DWORD)g.range(1, 300);
        at_(c, 0x20) = g.chance(5) ? (DWORD)g.range(-3, 0) : (DWORD)g.range(1, 300);
        at_(c, 0xfc) = g.chance(2) ? ptrv(scroll_info(g, i == 0)) : 0;
        if (i && g.chance(2)) {
            BYTE *cache = xalloc(0x70);
            at_(cache, 0x30) = ptrv(win(i));
            at_(cache, 0x38) = at_(win(W_TARGET), 0);
            at_(cache, 0x54) = at_(win(W_TARGET), 0);
            at_(cache, 0x60) = ptrv(win(W_STREAM));
            at_(cache, 0x20) = g.below(3);
            at_(cache, 0x28) = g.below(3);
            at_(c, 0x74) = ptrv(cache);
        } else {
            at_(c, 0x74) = 0;
        }
    }
    if (g.chance(3))
        at_(w, 0xe8) = 0;
    case_global(0x60a524, g.chance(4) ? 0 : 1);
    case_global(0x62e5c4, g.chance(2) ? 0 : 1);
    case_global(0x62e5e0, g.chance(2) ? 0 : 1);
    case_global(0x60a528, g.below(8));
    for (DWORD a = 0x62e578; a < 0x62e588; a += 4)
        case_global(a, (DWORD)g.range(-10, 400));
    g_self = w;
    g_args[0] = ptrv(frame);
}

void s_scroll(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    bar(W_BAR_V, g);
    bar(W_BAR_H, g);
    at_(w, 0xfc) = ptrv(scroll_info(g, true));
    set_kind(C_WINDOW, 60, K_SMALL);
    set_kind(C_WINDOW, 61, K_SMALL);
    g_args[0] = ptrv(w);
    g_args[1] = 0;
}

void s_modal(Gen &g)
{
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    BYTE *frame = make_window(1, C_CFRAME);
    make_window(W_OBJ, C_SCREEN);
    BYTE *reporter = xalloc(0x10);
    at_(reporter, 0) = at_(make_window(W_STREAM, C_OBJECT), 0);
    at_(reporter, 4) = g.chance(2) ? ptrv(frame) : ptrv(make_window(2, C_CFRAME));
    case_global(0x6c02cc, ptrv(reporter));
    set_kind(C_SCREEN, 65, K_BOOL);
    g_case.fixed[2] = g.next();
    g_args[0] = ptrv(w);
    g_args[1] = ptrv(frame);
    g_args[2] = g.chance(3) ? 0 : g.below(5);
    g_args[3] = g.chance(2) ? ptrv((void *)&modal_callback) : 0;
}

void s_rec(Gen &g)
{
    stream_args();
    BYTE *r = xalloc(0x10);
    at_(r, 0) = g.next();
    at_(r, 4) = g.next();
    at_(r, 8) = g.next();
    at_(r, 0xc) = g.chance(4) ? 0 : ptrv(make_window(W_STREAM, C_OBJECT));
    g_self = r;
    g_args[0] = g.chance(2) ? 1 : g.next() & ~1u;
    g_case.fixed[0] = g.below(5);
}
void s_rec_stream(Gen &g)
{
    s_rec(g);
    at_(g_self, 0xc) = ptrv(make_window(W_STREAM, C_OBJECT));
}
void s_renderer(Gen &g)
{
    g_self = xalloc(8);
    at_(g_self, 4) = g.next();
    g_args[0] = g.chance(2) ? 1 : g.next() & ~1u;
}

const Test TESTS[] = {
    {"CJWindow::DrawChildren", 0x40b620, CALL_THIS, 1, ~0u, 600, s_draw, STUBS, NSTUBS},
    {"CJWindow::UpdateScrollBars", 0x40f110, CALL_FAST, 2, 0, 800, s_scroll, STUBS, NSTUBS},
    {"CJWindow_RunModal", 0x40f5f0, CALL_FAST, 4, 0xff, 400, s_modal, MODAL_STUBS, NMODAL},
    {"RecFrame::DeletingDtor", 0x42e950, CALL_THIS, 1, ~0u, 40, s_rec, STUBS, NSTUBS},
    {"RecFrame::Dtor", 0x42e9b0, CALL_THIS, 0, ~0u, 40, s_rec, STUBS, NSTUBS},
    {"RecFrame::Replay", 0x42f2a0, CALL_THIS, 0, ~0u, 60, s_rec_stream, STUBS, NSTUBS},
    {"RecFrame::Terminate", 0x42f2f0, CALL_THIS, 0, ~0u, 20, s_rec_stream, STUBS, NSTUBS},
    {"CInventoryDetailRenderer::DeletingDtor", 0x47ac50, CALL_THIS, 1, ~0u, 20, s_renderer, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_rest4a)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x60a524, 1);
        g_globals.add(0x62e5c4, 1);
        g_globals.add(0x62e5e0, 1);
        g_globals.add(0x60a528, 4);
        for (DWORD a = 0x62e578; a < 0x62e588; a += 4)
            g_globals.add(a, 4);
        g_globals.add(0x6c02cc, 4);
        g_globals.add(0x62e648, 4);
        g_globals.add(0x62e64c, 4);
    }
    Tally t{"ui rest 4a: the children's drawing, the scroll bars, the modal loop, the recording frame, the detail renderer"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 44000);
    t.report();
}

}  // namespace zax
