// Checks of the movie object, the Bink wrapper's own functions, App_PlayMovie and App_DrawFrame
// (movie_rest4a.cpp) against the original, with the toolkit's harness (checks_ui_common.h): every function that
// calls binkw32.dll is a recording stand-in, so no movie is opened; the Bink system global 0x62eaf0 is given
// the case's record array (handles with frame counts in the block) and put back; the movie's frame is a
// recorded CFrame of the block (ShowFrame) or the real CFrame code on App_PlayMovie's local; the display mode,
// the keys, the message pump, the page flip, the frame rate counters and the font are stand-ins.
// App_PlayMovie's loop ends through the message pump's stand-in, which marks every movie as at its end after
// the case's number of calls and toggles the application's "active" byte by the case's mask, so the pause
// path runs too.
#include "checks_ui_common.h"

namespace zax {

int hooks_movie_rest4a(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"movie_rest4a", &hooks_movie_rest4a};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4a8b00, CV_THIS, 0, K_VALUE, "CFrame::Dtor"},
    {0x432080, CV_THIS, 1, K_VALUE, "BinkSystem_CloseMovie", "v"},
    {0x4320d0, CV_THIS, 1, K_BOOL, "BinkMovie_Wait", "v"},
    {0x432110, CV_THIS, 7, K_VALUE, "BinkMovie_DecodeToFrame", "vvvvvvv"},
    {0x431fd0, CV_THIS, 2, K_FIXED0, "BinkSystem_OpenMovie", "sv"},
    {0x432320, CV_THIS, 2, K_VALUE, "BinkMovie_GetSummary", "vx"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize", "vvv"},
    {0x4323a0, CV_THIS, 2, K_VALUE, "BinkMovie_SetPause", "vv"},
    {0x431e50, CV_THIS, 0, K_VALUE, "BinkSystem_Dtor"},
    {0x4a2c20, CV_FAST, 0, K_WINDOW_NN, "Stream_New", "sx"},
    {0x4a2440, CV_FAST, 0, K_WINDOW, "File_Open", "sv"},
    {0x4a1780, CV_THIS, 0, 'e', "FileBuf_Ctor"},
    {0x4a1970, CV_THIS, 2, K_VALUE, "FileBuf_Attach", "vv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

const Stub APP_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x432080, CV_THIS, 1, K_VALUE, "BinkSystem_CloseMovie", "v"},
    {0x4320d0, CV_THIS, 1, K_BOOL, "BinkMovie_Wait", "v"},
    {0x432110, CV_THIS, 7, K_VALUE, "BinkMovie_DecodeToFrame", "vvvvvvv"},
    {0x431fd0, CV_THIS, 2, K_FIXED0, "BinkSystem_OpenMovie", "sv"},
    {0x432320, CV_THIS, 2, K_VALUE, "BinkMovie_GetSummary", "vx"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize", "vvv"},
    {0x4323a0, CV_THIS, 2, K_VALUE, "BinkMovie_SetPause", "vv"},
    {0x4d22c0, CV_FAST, 0, K_FIXED3, "CommandLine_Has", "sx"},
    {0x4e27a0, CV_THIS, 1, K_VALUE, "Keys::ClearAll", "v"},
    {0x590ce0, CV_FAST, 1, K_VALUE, "Display_SetMode", "vvv"},
    {0x5694b0, CV_THIS, 0, K_VALUE, "ScreenFrame_5694b0"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt", "avvv"},
    {0x5693a0, CV_THIS, 0, K_VALUE, "ScreenFrame_Flip"},
    {0x4e2900, CV_THIS, 0, K_FIXED1, "Keys::TakeAnyPressed"},
    {0x5998f0, CV_CDECL, 0, K_VALUE, "App_PumpMessages"},
    {0x578cb0, CV_THIS, 2, K_VALUE, "ScreenImage::ShowFile", "sv"},
    {0x4a8290, CV_THIS, 0, K_FLOAT, "FrameRate::Sample"},
    {0x4a83c0, CV_THIS, 0, K_VALUE, "FrameRate::Start"},
    {0x4a6be0, CV_THIS, 0, K_SMALL, "Sequence_Height"},
    {0x4a7310, CV_THIS, 2, K_SMALL, "CFont::TextWidth", "sv"},
    {0x4a6290, CV_THIS, 1, K_SMALL, "CFont::CharWidth", "v"},
    {0x567bb0, CV_THIS, 0, K_VALUE, "ScreenFrame_Lock"},
    {0x567c90, CV_THIS, 0, K_VALUE, "ScreenFrame_Unlock"},
    {0x4b3a70, CV_THIS, 5, K_VALUE, "Frame_FillRectXY", "vvvvx"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"},
};
constexpr int NAPP = sizeof APP_STUBS / sizeof APP_STUBS[0];

constexpr DWORD SYS = 0x62eaf0;
constexpr int NREC = 4;
BYTE *g_recs;

// The Bink system's records: NREC records, each with a handle whose +8 is its frame count
void bink_records(Gen &g)
{
    g_recs = xalloc(NREC * 0x18);
    for (int i = 0; i < NREC; i++) {
        BYTE *h = xalloc(0x10);
        at_(h, 8) = 40 + (DWORD)i;
        at_(g_recs, i * 0x18) = ptrv(h);
        at_(g_recs, i * 0x18 + 8) = g.chance(3) ? 40 + (DWORD)i : g.below(40);
        at_(g_recs, i * 0x18 + 4) = g.next();
    }
    case_global(SYS + 8, ptrv(g_recs));
    case_global(SYS + 0xc, NREC);
}
DWORD movie_id(Gen &g) { return g.chance(4) ? 0xffffffff : g.below(NREC); }

int g_pumps;
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls != -1)
        return;
    if ((DWORD)slot == 0x432320)   // GetSummary: the summary's width and height
        if (BYTE *s = at<BYTE *>(args[1])) {
            at_(s, 0) = g_case.fixed[5] & 0x3ff;
            at_(s, 4) = g_case.fixed[5] >> 16 & 0x3ff;
        }
    if ((DWORD)slot == 0x4ab5a0) {   // SetSize: the words the movie loop reads
        at_<WORD>(at<void *>(g_effect_ecx), 0x12) = (WORD)args[0];
        at_<WORD>(at<void *>(g_effect_ecx), 0x14) = (WORD)args[1];
    }
    if ((DWORD)slot == 0x4b3a70) {   // the FieldColor without its padding and the 8-bit index (+0xa, unset for a 16-bit colour)
        const BYTE *c = at<const BYTE *>(args[4]);
        log_event(0x5b01, (DWORD)c[0] | c[1] << 8 | c[2] << 16 | (DWORD)c[3] << 24, (DWORD)c[4] | c[5] << 8 | c[6] << 16);
        log_event(0x5b02, (DWORD)c[8] | c[9] << 8, at_(const_cast<BYTE *>(c), 12));   // +0xa (the index) is left unset
    }
    if ((DWORD)slot == 0x5998f0) {
        if (g_case.fixed[4] >> (g_pumps & 31) & 1)
            g_self[0x60] ^= 1;
        if (++g_pumps >= (int)g_case.fixed[2] && g_recs)
            for (int i = 0; i < NREC; i++)
                at_(g_recs, i * 0x18 + 8) = 40 + (DWORD)i;
    }
}

void base(Gen &g)
{
    g_recs = nullptr;
    g_pumps = 0;
    g_case.effect = &effect;
    bink_records(g);
}

BYTE *movie(Gen &g, bool recorded)
{
    BYTE *m = recorded ? make_window(0, C_CFRAME) : xalloc(0x48);
    if (!recorded)
        for (int o = 0; o < 0x48; o += 4)
            at_(m, o) = g.next();
    at_(m, 0x40) = movie_id(g);
    m[0x44] = g.chance(3) ? 1 : 0;
    m[0x45] = g.chance(3) ? 0 : 1;
    return m;
}
void s_movie(Gen &g)
{
    g_self = movie(g, false);
    g_args[0] = g.below(2) ? 1 : g.next();
}
void s_show(Gen &g)
{
    g_self = movie(g, true);
    if (at_(g_self, 0x40) == 0xffffffff)
        at_(g_self, 0x40) = g.below(NREC);
}
void s_show_none(Gen &g)
{
    g_self = movie(g, true);
    at_(g_self, 0x40) = 0xffffffff;
}
void s_open(Gen &g)
{
    g_self = movie(g, false);
    g_case.fixed[0] = movie_id(g);
    g_case.fixed[5] = g.next();
    g_args[0] = ptrv(TEXTS[g.below(8)]);
}
void s_system(Gen &g)
{
    g_self = xalloc(0x28);
    g_args[0] = g.below(2) ? 1 : g.next();
}
void s_add(Gen &g)
{
    BYTE *s = xalloc(0x10);
    const DWORD n = g.below(5);
    BYTE *r = n || g.chance(2) ? xalloc(0x18 * (n ? n : 1)) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(r, i * 0x18) = g.chance(3) ? 0 : g.next();
        for (DWORD o = 4; o < 0x18; o += 4)
            at_(r, i * 0x18 + o) = g.next();
    }
    at_(s, 8) = ptrv(r);
    at_(s, 0xc) = n;
    g_self = s;
    g_args[0] = g.chance(5) ? 0 : g.next();
}
void s_at_end(Gen &g)
{
    g_self = at<BYTE *>(SYS);
    g_args[0] = g.below(NREC);
}
void s_io(Gen &g)
{
    make_window(0, C_OBJECT);
    make_window(1, C_OBJECT);
    g_case.nwin = 2;
    BYTE *io = xalloc(200);
    for (int o = 0; o < 200; o += 4)
        at_(io, o) = g.next();
    static const DWORD FLAGS[] = {0, 0x800000, 0x4000000, 0x4800000, 0x12345};
    g_args[0] = 0;
    g_args[1] = 0;
    g_args[2] = ptrv(io);
    g_args[3] = ptrv(TEXTS[g.below(8)]);
    g_args[4] = FLAGS[g.below(5)] | (g.chance(3) ? g.next() & ~0x4800000u : 0);
}
void s_play(Gen &g)
{
    BYTE *app = xalloc(0x80);
    at_(app, 4) = ptrv(make_window(1, C_CFRAME));
    app[0x60] = g.chance(3) ? 0 : 1;
    g_self = app;
    g_case.fixed[0] = movie_id(g);
    g_case.fixed[1] = g.chance(4) ? 1 : 0;
    g_case.fixed[2] = g.below(5);
    g_case.fixed[3] = g.chance(6) ? 1 : 0;
    g_case.fixed[4] = g.chance(2) ? g.next() : 0;
    g_case.fixed[5] = g.next();
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.chance(2) ? ptrv(xalloc(0x10)) : 0;
    g_args[2] = g.chance(2) ? 1 : (g.next() & ~0xffu);
}
void s_draw(Gen &g)
{
    BYTE *app = xalloc(0x80);
    for (int o = 0; o < 0x80; o += 4)
        at_(app, o) = g.below(4) ? g.below(60) : g.next();
    at_(app, 4) = ptrv(make_window(1, C_CFRAME));
    at_(app, 0x30) = g.chance(4) ? 0 : ptrv(xalloc(0x10));
    app[0x1b] = g.chance(4) ? 0 : 1;
    app[0x29] = g.chance(3) ? 0 : 1;
    g_self = app;
    case_global(0x6c02dc, g.below(80));
    case_global(0x6bff94, g.below(4));
    case_global(0x6bfe80, g.chance(2) ? 0x41f00000 : g.next());
    case_global(0x6bff98, g.chance(2) ? 0x41a00000 : 0x3f800000);
    case_global(0x6be7dc, g.below(256));
    case_global(0x6c02e4, g.next());
    case_global(0x6c02d0, g.chance(2) ? 0x42700000 : g.next());
}

const Test TESTS[] = {
    {"Movie_NewFrame", 0x431ac0, CALL_THIS, 0, ~0u, 20, s_movie, STUBS, NSTUBS},
    {"BinkMovie_DeletingDtor", 0x431ba0, CALL_THIS, 1, ~0u, 80, s_movie, STUBS, NSTUBS},
    {"BinkMovie_Dtor", 0x431bf0, CALL_THIS, 0, ~0u, 60, s_movie, STUBS, NSTUBS},
    {"BinkMovie_Close", 0x431c30, CALL_THIS, 0, ~0u, 60, s_movie, STUBS, NSTUBS},
    {"BinkMovie_ShowFrame", 0x431c60, CALL_THIS, 0, ~0u, 150, s_show, STUBS, NSTUBS},
    {"BinkMovie_ShowFrame no movie", 0x431c60, CALL_THIS, 0, 0, 10, s_show_none, STUBS, NSTUBS},
    {"BinkMovie_Open", 0x431cf0, CALL_THIS, 1, ~0u, 120, s_open, STUBS, NSTUBS},
    {"BinkMovie_Pause", 0x431d80, CALL_THIS, 1, ~0u, 60, s_movie, STUBS, NSTUBS},
    {"BinkMovie_IsDone", 0x431da0, CALL_THIS, 0, ~0u, 100, s_movie, STUBS, NSTUBS},
    {"BinkSystem_DeletingDtor", 0x431e30, CALL_THIS, 1, ~0u, 30, s_system, STUBS, NSTUBS},
    {"BinkSystem_Get", 0x431f10, CALL_CDECL, 0, ~0u, 3, s_system, STUBS, NSTUBS},
    {"BinkSystem_AddMovie", 0x431f20, CALL_THIS, 1, ~0u, 200, s_add, STUBS, NSTUBS},
    {"BinkMovie_AtEnd", 0x432370, CALL_THIS, 1, ~0u, 60, s_at_end, STUBS, NSTUBS},
    {"BinkIo_Open", 0x432aa0, CALL_FAST, 5, ~0u, 120, s_io, STUBS, NSTUBS},
    {"App_PlayMovie", 0x4cd5a0, CALL_THIS, 3, 0, 300, s_play, APP_STUBS, NAPP},
    {"App_DrawFrame", 0x4ce160, CALL_THIS, 0, 0xff, 400, s_draw, APP_STUBS, NAPP},
};

}  // namespace

ZAX_CHECKS(movie_rest4a)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(SYS + 8, 4);
        g_globals.add(SYS + 0xc, 4);
        g_globals.add(0x6c02d4, 1);
        g_globals.add(0x6c02dc, 4);
        g_globals.add(0x6bff94, 1);
        g_globals.add(0x6bfe80, 4);
        g_globals.add(0x6bff98, 4);
        g_globals.add(0x6be7dc, 1);
        g_globals.add(0x6c02e4, 4);
        g_globals.add(0x6c02d0, 4);
    }
    Tally t{"movie rest 4a: the movie object, the Bink wrapper's own functions, App_PlayMovie, App_DrawFrame"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 42000);
    t.report();
}

}  // namespace zax
