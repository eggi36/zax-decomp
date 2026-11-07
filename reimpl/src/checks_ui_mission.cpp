// Checks of the mission over window and the mission details (ui_mission.cpp) against the original with the
// toolkit's harness and the screens' setup (checks_ui_screens.h). The constructors run on real objects of the
// extra area; the options are blocks of the case; the sequence lists 0x6c0540, 0x6c22e0 and the font list get a
// fake vtable (slot 1 a name, slot 3 an index, slot 37 a load) and records whose objects are game objects (slot 36
// writes a rectangle); the image scroll bar's setters, the details page's drawing and the multiplayer continue
// are stand-ins.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_mission(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_mission", &hooks_ui_mission};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x5a2010, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::MissionOverOptions"},
    {0x5a20d0, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::MultiPlayerMissionOverOptions"},
    {0x5a1d40, CV_THIS, 0, K_FIXED1, "CZaxCustomRules::InventoryDisplay"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x50ff80, CV_THIS, 0, K_SMALL, "DrawMissionDetailsPage"},
    {0x40d550, CV_THIS, 2, K_VALUE, "CJWindow::SetContentSize"},
    {0x4144b0, CV_THIS, 2, 'e', "ImageScrollBar::ImageScrollBar"},
    {0x413ce0, CV_THIS, 1, K_VALUE, "CJScrollBar::SetRangeFrom"},
    {0x53f010, CV_FAST, 4, 'e', "FieldColor_CtorRGBA", "xxvvvv"},
    {0x414560, CV_THIS, 1, K_VALUE, "ImageScrollBar::SetColour"},
    {0x4145e0, CV_THIS, 2, K_VALUE, "ImageScrollBar::SetUpImages"},
    {0x414600, CV_THIS, 2, K_VALUE, "ImageScrollBar::SetDownImages"},
    {0x414620, CV_THIS, 1, K_VALUE, "ImageScrollBar::SetTrackImage"},
    {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"},
    {0x4100f0, CV_THIS, 1, K_VALUE, "Button::Set114"},
    {0x4101c0, CV_THIS, 1, K_VALUE, "Button::ClearTip"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x5007b0, CV_THIS, 0, K_FIXED2, "Object_SecondMember"},
    {0x403e40, CV_THIS, 1, K_VALUE, "CDialog::Key"},
    {0x5ac560, CV_CDECL, 0, K_VALUE, "ContinueReader_Get"},
    {0x5ac500, CV_THIS, 0, K_VALUE, "ContinueReader::Write"},
    {0x42d3d0, CV_THIS, 2, 'e', "PageWindow::Ctor"},
    {0x42d3f0, CV_THIS, 0, K_VALUE, "PageWindow::Dtor"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);

DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7a01, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7a02, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7a03, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    log_event(0x7a04, rec, 0);
    return ptrv(win(W_OBJECT));
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_OBJECT && slot == 36) {   // a sequence's rectangle
        int *r = at<int *>(args[0]);
        r[0] = (int)(answer & 0x3f) - 20, r[1] = (int)((answer >> 6) & 0x3f) - 20;
        r[2] = r[0] + (int)((answer >> 12) & 0xff), r[3] = r[1] + (int)((answer >> 20) & 0xff);
        return;
    }
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x4144b0:
    case 0x42d3d0: made(answer, W_PLAIN_T); return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

// Records of a list (the objects game objects; all loaded when `all`)
void records(Gen &g, DWORD list, bool all)
{
    const DWORD n = all ? 6 : g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = all || !g.chance(3) ? ptrv(win(W_OBJECT)) : 0;
        recs[i * 0x80 + 0x7f] = !all && g.chance(6) ? 1 : 0;
    }
    case_global(list, ptrv(g_list_vt));
    case_global(list + 0x30, !all && g.chance(8) ? 0 : ptrv(recs));
    case_global(list + 0x34, n);
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
    object_args(g_object_args, "0:1,23:2,36:1");
    g_case.argspec[36][0] = A_IGNORE;
    set_kind(C_OBJECT, 45, K_SMALL);
    set_kind(C_OBJECT, 39, K_SMALL);
    BYTE *opt = xalloc(0x180);
    for (DWORD o = 8; o < 0x180; o += 4)
        at_<int>(opt, o) = g.range(-30, 300);
    for (DWORD o = 8; o <= 0x2c; o += 4)
        at_(opt, o) = g.below(6);
    at_(opt, 0x7c) = g.below(6), at_(opt, 0xac) = g.below(6), at_(opt, 0xc8) = g.below(6);
    g_case.fixed[0] = ptrv(opt);
    g_case.fixed[1] = ptrv(opt);
    BYTE *title = xalloc(4);
    make_text(title, TEXTS[g.below(8)]);
    g_case.fixed[2] = ptrv(title);
    BYTE *save = xalloc(0x20);
    at_(save, 0x1c) = g.chance(2) ? 0 : g.next();
    at_(win(W_APP), 0x33c) = ptrv(save);
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : g.next();
    at_(win(W_APP), 0x2e0) = ptrv(holder);
    // the frame and the button images are read without a test: all records loaded
    records(g, 0x6c0540, true);
    records(g, 0x6c22e0, g.chance(2));
    records(g, 0x6be1e0, true);
    case_global(0x6cfd34, g.chance(2) ? 0 : ptrv(win(0)));
}

void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    g_args[1] = g.next();
    g_args[2] = g.below(3);
    BYTE *text = xalloc(4);
    make_text(text, TEXTS[g.below(8)]);
    g_args[3] = ptrv(text);
}
void s_flags(Gen &g)
{
    make_text(win(0) + 0x134, TEXTS[g.below(8)]);
    make_text(win(0) + 0x190, TEXTS[g.below(8)]);
    g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_none(Gen &) {}
void s_key(Gen &g)
{
    BYTE *w = win(0);
    at_(w, 0x140) = g.chance(3) ? ptrv(win(W_OTHER)) : 0;
    at_(w, 0x14c) = g.chance(4) ? 0 : ptrv(win(W_BUTTON_T));
    at_(w, 0x1ac) = g.below(3);
    w[0x1b0] = g.chance(2);
    const DWORD keys[8] = {0xd, 0x1b, 0x20, 0xfa, 0xfb, 'A', 0xe, 0x100};
    g_args[0] = keys[g.below(8)];
}
void s_make(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    g_args[1] = g.next(), g_args[2] = g.next(), g_args[3] = g.next(), g_args[4] = g.next(), g_args[5] = g.next();
}
void s_page(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    (void)g;
}
void s_dtor(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6c0540, 0x6c0570, 0x6c0574, 0x6c22e0, 0x6c2310,
                              0x6c2314, 0x6be1e0, 0x6be210, 0x6be214, 0x6cfd34};

const Test TESTS[] = {
    {"MissionOver ctor", 0x50cab0, CALL_THIS, 4, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"MissionOver deleting dtor", 0x50db00, CALL_THIS, 1, ~0u, 40, s_flags, STUBS, NSTUBS},
    {"MissionOver IsOpen", 0x50db60, CALL_CDECL, 0, ~0u, 10, s_none, STUBS, NSTUBS},
    {"MissionOver Close", 0x50db70, CALL_CDECL, 0, 0, 20, s_none, STUBS, NSTUBS},
    {"MissionOver Key", 0x50e860, CALL_THIS, 1, ~0u, 200, s_key, STUBS, NSTUBS},
    {"MissionDetails Make", 0x50fea0, CALL_THIS, 6, ~0u, 30, s_make, STUBS, NSTUBS},
    {"MissionDetails deleting dtor", 0x50fef0, CALL_THIS, 1, ~0u, 20, s_dtor, STUBS, NSTUBS},
    {"MissionPage ctor", 0x50ffe0, CALL_THIS, 1, ~0u, 100, s_page, STUBS, NSTUBS},
    {"MissionPage deleting dtor", 0x5101a0, CALL_THIS, 1, ~0u, 20, s_dtor, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_mission)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui mission: the mission over window, the mission details window and page"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 28000);
    t.report();
}

}  // namespace zax
