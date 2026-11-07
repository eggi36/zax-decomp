// Checks of batch C3's recording frame slots and replay callbacks (frame_c3_rec.cpp) against the original with the
// toolkit's harness (checks_ui_common.h): a recording frame in the extra area whose memory stream (+0xc) and blend
// mode are "game object" fakes (slot 2 writes are logged with the dword written, slot 3 reads answer the case's
// values in order, slots 4/5 of the mode logged) and whose target (+8) is a fake frame; the static stream's
// constructor and seek, the static colour's constructor and setter and atexit as recording stand-ins, their
// guards and the last colour 0x62ea90 case values.
#include "checks_ui_common.h"
#include "checks_ui_screens.h"

namespace zax {

int hooks_frame_c3_rec(Hook *out);

namespace {

using namespace uic;
using scr::object_args;

constinit ModuleHooks g_hooks{"frame_c3_rec", &hooks_frame_c3_rec};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x505f00, CV_THIS, 0, K_VALUE, "MemoryStream_Ctor"},
    {0x506100, CV_THIS, 1, K_VALUE, "MemoryStream_Seek"},
    {0x53f000, CV_THIS, 0, K_VALUE, "FieldColor_Ctor"},
    {0x53f080, CV_THIS, 4, K_VALUE, "FieldColor_SetRGBA"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

char g_object_args[MAXSLOT + 1];
DWORD g_reads[16];
int g_nread;
BYTE *g_stream;

// A stream read answers the next of the case's values through its buffer
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == C_OBJECT && slot == 3 && g_effect_ecx == ptrv(g_stream) && args[0])
        *at<DWORD *>(args[0]) = g_nread < 16 ? g_reads[g_nread++] : 0;
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    BYTE *r = xalloc(0x10);
    at_(r, 0) = 0x5ee9c0;
    at_(r, 4) = g.next();
    at_(r, 8) = ptrv(make_window(14, C_CFRAME));
    g_stream = make_window(13, C_OBJECT);
    at_(r, 0xc) = ptrv(g_stream);
    make_window(12, C_OBJECT);   // the blend mode
    object_args(g_object_args, "2:2,3:2,4:1,5:1");
    g_case.argspec[3][0] = A_IGNORE;   // (a read's buffer holds what was there before)
    g_case.argspec[28][0] = A_RECT;
    g_case.argspec[24][0] = A_RECT;
    g_case.argspec[23][1] = A_RECT, g_case.argspec[23][2] = A_RECT;
    g_case.effect = &effect;
    g_nread = 0;
    for (DWORD &v : g_reads)
        v = g.chance(4) ? g.below(5) : g.next();
    g_reads[9] = ptrv(win(12));   // the blit's blend mode
    case_global(0x62ea94, g.below(4));
    case_global(0x62eac8, g.below(4));
    case_global(0x62ea90, g.chance(3) ? g_reads[4] : g.chance(2) ? g_reads[2] : g.next());
    g_self = r;
}

DWORD *rect(Gen &g)
{
    DWORD *r = reinterpret_cast<DWORD *>(xalloc(16));
    for (int k = 0; k < 4; k++)
        r[k] = g.chance(3) ? g.below(600) : g.next();
    return r;
}
void s_blit(Gen &g)
{
    g_args[0] = g.next();
    g_args[1] = ptrv(rect(g));
    g_args[2] = ptrv(rect(g));
    g_args[3] = ptrv(win(12));
}
void s_rect(Gen &g) { g_args[0] = ptrv(rect(g)); }
void s_fill(Gen &g)
{
    g_args[0] = ptrv(rect(g));
    g_args[1] = ptrv(rect(g));
}
void s_line(Gen &g)
{
    for (int k = 0; k < 4; k++)
        g_args[k] = g.next();
    g_args[4] = ptrv(rect(g));
}
void s_pixel(Gen &g)
{
    g_args[0] = g.next(), g_args[1] = g.next();
    g_args[2] = ptrv(rect(g));
}
void s_slot57(Gen &g)
{
    g_args[0] = ptrv(rect(g));
    g_args[1] = g.next();
}
void s_replay(Gen &) {}

const Test TESTS[] = {
    {"RecFrame Blit", 0x42e9f0, CALL_THIS, 4, 0xff, 60, s_blit, STUBS, NSTUBS},
    {"RecFrame Slot24", 0x42ec90, CALL_THIS, 1, ~0u, 40, s_rect, STUBS, NSTUBS},
    {"RecFrame Fill", 0x42ed90, CALL_THIS, 2, ~0u, 40, s_fill, STUBS, NSTUBS},
    {"RecFrame Line", 0x42f020, CALL_THIS, 5, ~0u, 40, s_line, STUBS, NSTUBS},
    {"RecFrame Pixel", 0x42f0c0, CALL_THIS, 3, ~0u, 40, s_pixel, STUBS, NSTUBS},
    {"RecFrame Slot57", 0x42f200, CALL_THIS, 2, ~0u, 40, s_slot57, STUBS, NSTUBS},
    {"RecFrame ReplayBlit", 0x42eb10, CALL_THIS, 0, ~0u, 100, s_replay, STUBS, NSTUBS},
    {"RecFrame ReplaySlot24", 0x42ed10, CALL_THIS, 0, ~0u, 50, s_replay, STUBS, NSTUBS},
    {"RecFrame ReplayFill", 0x42ee30, CALL_THIS, 0, ~0u, 150, s_replay, STUBS, NSTUBS},
    {"RecFrame ReplayLine", 0x42ef30, CALL_THIS, 0, ~0u, 150, s_replay, STUBS, NSTUBS},
    {"RecFrame ReplayPixel", 0x42f130, CALL_THIS, 0, ~0u, 150, s_replay, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(frame_c3_rec)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x62ea94, 1);
        g_globals.add(0x62eac8, 1);
        g_globals.add(0x62ea90, 4);
    }
    Tally t{"frame c3 rec: the recording frame's slots and their replay callbacks"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 64500);
    t.report();
}

}  // namespace zax
