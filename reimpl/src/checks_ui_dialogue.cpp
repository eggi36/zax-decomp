// Checks of the dialogue window (ui_dialogue.cpp) against the original with the toolkit's harness and the
// screens' setup (checks_ui_screens.h): the window under test is a fake (window 0) with lines and speakers of
// the case, or a real object of the extra area (the constructor); the frame is a fake CFrame (window 13) whose
// slot 36 writes a rectangle of the case; the sequence list 0x6c0540 gets a fake vtable (as in the inventory's
// checks); the voice (Voice_Get 0x595b50 and its play, stop and playing test), the font's measures and wrap, the
// opacity, the blit and the text drawing are stand-ins.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_dialogue(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_dialogue", &hooks_ui_dialogue};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "asa"},
    {0x523df0, CV_THIS, 2, K_FIXED5, "ResourceList_Find", "sv"},
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x5a1e80, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::DialogDisplay"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x506f70, CV_THIS, 0, K_VALUE, "MessageLines_Clear"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"},
    {0x595b50, CV_CDECL, 0, K_FIXED4, "Voice_Get"},
    {0x595af0, CV_THIS, 0, K_VALUE, "Voice_Stop"},
    {0x595bd0, CV_THIS, 0, K_BOOL, "Voice_IsPlaying"},
    {0x595be0, CV_THIS, 3, K_VALUE, "Voice_Play", "svx"},
    {0x47e540, CV_THIS, 0, K_VALUE, "DialogLine_Dtor"},
    {0x40bcc0, CV_THIS, 2, K_VALUE, "CJWindow::MouseDown", "vq"},
    {0x524f50, CV_THIS, 1, K_VALUE, "COffsetClippedProxyFrame_SetClip", "r"},
    {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4a66c0, CV_THIS, 6, K_VALUE, "CFont::DrawLines", "vvvsvv"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x48d8f0, CV_THIS, 1, K_FIXED3, "DefinitionList_Object"},
    {0x4cdf30, CV_THIS, 0, K_VALUE, "App_Window"},
    {0x4a7450, CV_THIS, 3, 'p', "CFont::Wrap", "xsv"},
    {0x4a6c80, CV_THIS, 2, K_SMALL, "CFont::TextHeight", "sv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);

constexpr int W_FRAME_ = 13;   // (14 and 15 are game objects here)

// The sequence list's fake vtable (slot 1 a name, slot 3 an index, slot 37 a load)
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7d01, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7d02, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7d03, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    log_event(0x7d04, rec, 0);
    return ptrv(win(W_OBJECT));
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    // a frame's or a sequence's slot 36 writes a rectangle
    if ((cls == C_CFRAME || cls == C_OBJECT) && slot == 36) {
        int *r = at<int *>(args[0]);
        r[0] = (int)(answer & 0x3f) - 20, r[1] = (int)((answer >> 6) & 0x3f) - 20;
        r[2] = r[0] + (int)((answer >> 12) & 0xff), r[3] = r[1] + (int)((answer >> 20) & 0xff);
        return;
    }
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x46ec30: ini_effect(args, answer); return;
    case 0x595be0: {   // Voice_Play (text, 2, &seconds): the seconds of the case
        const float s = (answer & 3) == 0 ? 0.0f : (float)(answer >> 20) / 64.0f;
        memcpy(at<void *>(args[2]), &s, 4);
        return;
    }
    case 0x4a7450:   // the font's wrap (out, text, width): a wrapped text into out
        at<void(THISCALL *)(DWORD, const char *)>(0x4dee00)(args[0], TEXTS[answer & 7]);
        return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

void records(Gen &g, DWORD list, bool all)
{
    const DWORD n = all ? 6 : g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = all || !g.chance(3) ? ptrv(win(W_OBJECT)) : 0;
        recs[i * 0x80 + 0x7f] = !all && g.chance(6) ? 1 : 0;
        recs[i * 0x80 + 0x7d] = g.chance(2), recs[i * 0x80 + 0x7e] = g.chance(2);
    }
    case_global(list + 0x30, !all && g.chance(8) ? 0 : ptrv(recs));
    case_global(list + 0x34, n);
}

// A line {ZString text, height, ZString voice, ZString speaker}
BYTE *new_line(Gen &g)
{
    BYTE *l = xalloc(0x10);
    make_text(l, TEXTS[g.below(8)]);
    at_(l, 4) = g.below(60);
    make_text(l + 8, TEXTS[g.below(8)]);
    make_text(l + 0xc, TEXTS[g.below(8)]);
    return l;
}

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[1] = ptrv((void *)&list_name);
        g_list_vt[3] = ptrv((void *)&list_find);
        g_list_vt[37] = ptrv((void *)&list_load);
    }
    screen_base(g, &effect);
    BYTE *opt = xalloc(0x80);
    for (DWORD o = 0x10; o < 0x80; o += 4)
        at_<int>(opt, o) = g.range(-30, 400);
    at_(opt, 0x54) = g.below(300), at_(opt, 0x68) = g.below(200), at_(opt, 0x6c) = g.below(100), at_(opt, 0x70) = g.below(6);
    g_case.fixed[0] = ptrv(opt);
    g_case.fixed[3] = ptrv(win(W_OBJECT));
    g_case.fixed[4] = g.chance(4) ? 0 : ptrv(win(W_APP));
    g_case.fixed[5] = g.chance(3) ? 0xffffffff : g.below(6);
    object_args(g_object_args, "0:1,23:2,36:1");
    g_case.argspec[36][0] = A_IGNORE;   // (slot 36 writes the rectangle: its old contents are the caller's stack)
    make_window(W_FRAME_, C_CFRAME);
    case_global(0x6c0540, ptrv(g_list_vt));
    records(g, 0x6c0540, false);
    records(g, 0x6be1e0, false);
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : g.next();
    at_(win(W_APP), 0x2e0) = ptrv(holder);
    BYTE *world = win(W_WORLD);
    at_(world, 0x280) = g.next();
    // the window under test
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    const DWORD n = g.below(6);
    BYTE *d = xalloc(4 * (n + 2));
    for (DWORD i = 0; i < n; i++)
        at_(d, 4 * i) = ptrv(new_line(g));
    at_(w, 0x104) = 0x5f14d0, at_(w, 0x108) = ptrv(d), at_(w, 0x10c) = n, at_(w, 0x110) = n + 2;
    for (DWORD o = 0x114; o < 0x12c; o += 0xc) {
        make_text(w + o, TEXTS[g.below(8)]);
        at_(w, o + 4) = g.below(6);
        w[o + 8] = g.chance(2);
    }
    at_(w, 0x12c) = g.below(2);
    at_(w, 0x130) = g.chance(3) ? 0xffffffff : g.below(2);
    for (DWORD o = 0x134; o <= 0x140; o += 4)
        at_(w, o) = g.below(6);
    const float scroll = (float)g.range(-300, 300) / 7.0f, speed = (float)g.range(0, 200) / 3.0f;
    memcpy(w + 0x144, &scroll, 4);
    memcpy(w + 0x148, &speed, 4);
    at_(w, 0x14c) = n && !g.chance(4) ? g.below(n) : 0xffffffff;
    w[0x154] = g.chance(2), w[0x155] = g.chance(2);
    at_(w, 0x158) = g.chance(2) ? 0 : ptrv(win(W_OTHER));
    make_window(W_OTHER, C_OBJECT);
    make_window(W_OTHER2, C_OBJECT);
    case_global(0x6bdabc, g.chance(2) ? 0 : ptrv(w));
    g_self = w;
}

void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(6) ? 0 : ptrv(win(W_PARENT));
    records(g, 0x6c0540, true);   // (the constructor reads the frame sequence without a test)
}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_mouse(Gen &g)
{
    g_args[0] = g.below(3);
    int *pt = reinterpret_cast<int *>(xalloc(8));
    pt[0] = g.range(-50, 700), pt[1] = g.range(-50, 500);
    g_args[1] = ptrv(pt);
}
void s_frame(Gen &) { g_args[0] = ptrv(win(W_FRAME_)); }
void s_dt(Gen &g)
{
    const float dt = g.chance(5) ? 0.0f : (float)g.range(-3, 60) / 16.0f;
    memcpy(&g_args[0], &dt, 4);
    // (Update reads the first line without a test when one is said and none is left)
    if (!at_(g_self, 0x10c))
        at_(g_self, 0x14c) = 0xffffffff;
}
void s_object(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.next(); }
DWORD zstring(const char *text)
{
    BYTE *z = xalloc(4);
    make_text(z, text);
    return ptrv(z);
}
void s_say(Gen &g)
{
    records(g, 0x6c0540, true);
    records(g, 0x6be1e0, true);   // (AddLine reads the font's height without a test)
    g_args[0] = zstring(TEXTS[g.below(8)]);
    g_args[1] = zstring(TEXTS[g.below(8)]);
    g_args[2] = zstring(TEXTS[g.below(8)]);
    g_args[3] = g.chance(2) ? 0 : 1;
}
void s_preload(Gen &g)
{
    // (a window made for the preload reads the frame sequence without a test)
    const bool had = g.chance(2);
    case_global(0x6bdabc, had ? ptrv(win(0)) : 0);
    records(g, 0x6c0540, !had || g.chance(2));
}
void s_portrait(Gen &g)
{
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.next();
}
void s_add_line(Gen &g)
{
    records(g, 0x6be1e0, true);   // (the font's height is read without a test)
    g_args[0] = zstring(g.chance(4) ? "  " : TEXTS[g.below(8)]);
    g_args[1] = zstring(TEXTS[g.below(8)]);
    g_args[2] = zstring(TEXTS[g.below(8)]);
    if (g.chance(3))
        at_(g_self, 0x7c) = 0;
    // (StartLine reads the line being said: a line is said when the window gets its first one)
}
void s_start(Gen &g)
{
    if (!at_(g_self, 0x10c)) {
        BYTE *d = at_<BYTE *>(g_self, 0x108);
        at_(d, 0) = ptrv(new_line(g));
        at_(g_self, 0x10c) = 1;
    }
    at_(g_self, 0x14c) = g.below(at_(g_self, 0x10c));
}
void s_none(Gen &) {}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6c0540, 0x6c0570, 0x6c0574, 0x6be210, 0x6be214, 0x6bdabc};

const Test TESTS[] = {
    {"DialogueWindow ctor", 0x47bbd0, CALL_THIS, 1, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"DialogueWindow deleting dtor", 0x47bfe0, CALL_THIS, 1, ~0u, 150, s_flags, STUBS, NSTUBS},
    {"DialogueWindow MouseDown", 0x47c250, CALL_THIS, 2, ~0u, 150, s_mouse, STUBS, NSTUBS},
    {"DialogueWindow Draw", 0x47c2e0, CALL_THIS, 1, 0, 400, s_frame, STUBS, NSTUBS},
    {"DialogueWindow Update", 0x47c810, CALL_THIS, 1, 0, 600, s_dt, STUBS, NSTUBS},
    {"DialogueWindow SetObject", 0x47c9d0, CALL_THIS, 1, 0, 40, s_object, STUBS, NSTUBS},
    {"Dialogue Say", 0x47ca10, CALL_FAST, 4, 0, 200, s_say, STUBS, NSTUBS},
    {"Dialogue Window", 0x47ca90, CALL_CDECL, 0, ~0u, 10, s_none, STUBS, NSTUBS},
    {"Dialogue Preload", 0x47caa0, CALL_CDECL, 0, 0, 200, s_preload, STUBS, NSTUBS},
    {"Dialogue Portrait", 0x47ccc0, CALL_FAST, 2, ~0u, 60, s_portrait, STUBS, NSTUBS},
    {"DialogueWindow AddLine", 0x47cd20, CALL_THIS, 3, 0, 300, s_add_line, STUBS, NSTUBS},
    {"DialogueWindow StartLine", 0x47cfa0, CALL_THIS, 0, 0, 600, s_start, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_dialogue)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui dialogue: the dialogue window, its lines, speakers and speed"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 25000);
    t.report();
}

}  // namespace zax
