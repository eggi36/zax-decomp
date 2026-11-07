// Checks of the voice log panel (ui_voice_log.cpp) against the original with the toolkit's harness and the
// screens' setup (checks_ui_screens.h). The panel under test is a fake window (window 0) or a real object of the
// extra area (the constructor); the voice log's tree is a game object whose element array (CZaxSaveData::Elements
// 0x5bb9a0, a stand-in) holds elements of the case (times with repeats, orders, actions that are game objects);
// the list box, the header, the bar and Replay are fakes; the dialogue window's functions are stand-ins.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ui_voice_log(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ui_voice_log", &hooks_ui_voice_log};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x42d3d0, CV_THIS, 2, 'e', "PageWindow::Ctor"},
    {0x42d3f0, CV_THIS, 0, K_VALUE, "PageWindow::Dtor"},
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x5a1d40, CV_THIS, 0, K_FIXED0, "CZaxCustomRules::InventoryDisplay"},
    {0x5a1cd0, CV_THIS, 0, K_VALUE, "CZaxCustomRules::MaterialsGeneratorDisplay"},
    {0x4020d0, CV_THIS, 1, 'e', "CJHeader::CJHeader"},
    {0x402b40, CV_THIS, 1, K_VALUE, "CJHeader::SetColor"},
    {0x5007b0, CV_THIS, 0, K_TEXT, "Object_SecondMember"},
    {0x402770, CV_THIS, 5, K_VALUE, "CJHeader::AddColumn", "svvvv"},
    {0x402b90, CV_THIS, 2, K_VALUE, "CJHeader::SetColumnFlag18"},
    {0x47e910, CV_THIS, 0, K_FIXED1, "TreeModel_Root"},
    {0x4041f0, CV_THIS, 4, 'e', "CJListBox::CJListBox"},
    {0x406aa0, CV_THIS, 1, K_VALUE, "CJListBox::SetHeader"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x4144b0, CV_THIS, 2, 'e', "ImageScrollBar::ImageScrollBar"},
    {0x407a00, CV_THIS, 2, K_VALUE, "CJListBox::SetBar"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x40ffd0, CV_THIS, 3, K_VALUE, "Button::SetImage"},
    {0x4101c0, CV_THIS, 1, K_VALUE, "Button::ClearTip"},
    {0x5bb9a0, CV_THIS, 0, K_FIXED2, "CZaxSaveData::Elements"},
    {0x47ca90, CV_CDECL, 0, K_VALUE, "Dialogue_Window"},
    {0x47c9d0, CV_THIS, 1, K_VALUE, "DialogueWindow::SetObject"},
    {0x406890, CV_THIS, 1, K_VALUE, "CJListBox::SetList"},
    {0x406b60, CV_THIS, 1, K_VALUE, "CJListBox::SetSortDown"},
    {0x406b70, CV_THIS, 2, K_VALUE, "CJListBox::Sort"},
    {0x413c20, CV_THIS, 2, K_VALUE, "CJScrollBar::SetPos"},
    {0x40bc30, CV_THIS, 3, K_VALUE, "CJWindow::Slot9"},
    {0x40bcc0, CV_THIS, 2, K_VALUE, "CJWindow::MouseDown"},
    {0x40c0a0, CV_THIS, 4, K_VALUE, "CJWindow::Slot12"},
    {0x40c0b0, CV_THIS, 2, K_VALUE, "CJWindow::Slot15"},
    {0x40c170, CV_THIS, 1, K_VALUE, "CJWindow::Key"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];
static_assert(NSTUBS <= NSTUB);

constexpr int W_TREE = 11, W_ACTION = 13, W_SOURCE = 15;   // (12 is the list box's template, 14 the bar)
const char *const NAMES[4] = {"Fonts/Arial", "Big", "", "Interface/Replay"};

// The resource lists' fake vtable (slot 1 a name, slot 3 an index, slot 37 a load)
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7b01, i, 0);
    return ptrv(NAMES[i & 3]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7b02, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7b03, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    return 0xf0000000u | (rec & 0xffff);
}
DWORD THISCALL list_other(BYTE *) { return 0; }

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)args;
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x42d3d0:
    case 0x4020d0:
    case 0x4144b0: made(answer, W_PLAIN_T); return;
    default: break;
    }
    if (const int t = template_of((DWORD)slot))
        made(answer, t);
}

char g_object_args[MAXSLOT + 1];

void records(Gen &g, DWORD list)
{
    const DWORD n = g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    case_global(list, ptrv(g_list_vt));
    case_global(list + 0x30, g.chance(8) ? 0 : ptrv(recs));
    case_global(list + 0x34, n);
}

DWORD g_count;   // the tree's elements

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
    object_args(g_object_args, "0:1,5:0,22:0,29:5,97:0");
    BYTE *opt = xalloc(0x200);
    for (DWORD o = 0x144; o < 0x200; o += 4)
        at_<int>(opt, o) = g.range(-30, 400);
    at_(opt, 0x144 + 0x6c) = g.below(6);
    g_case.fixed[0] = ptrv(opt);
    // the tree (a game object: slot 22 the count) and its elements
    make_window(W_TREE, C_OBJECT);
    make_window(W_ACTION, C_OBJECT);
    make_window(W_SOURCE, C_OBJECT);
    g_case.fixed[1] = ptrv(win(W_TREE));
    g_count = g.below(8);
    set_kind(C_OBJECT, 22, K_FIXED3);
    g_case.fixed[3] = g_count;
    BYTE *arr = xalloc(0x10);
    BYTE *data = xalloc(4 * (g_count + 1));
    double t = (double)g.below(5);
    for (DWORD i = 0; i < g_count; i++) {
        BYTE *e = xalloc(0x18);
        if (!g.chance(3))
            t += (double)g.range(1, 9) / 4.0;
        at_<double>(e, 0xc) = t;
        at_(e, 0x14) = g.below(100);
        at_(e, 8) = ptrv(win(W_ACTION));
        at_(data, 4 * i) = ptrv(e);
    }
    at_(arr, 4) = ptrv(data);
    g_case.fixed[2] = ptrv(arr);
    set_kind(C_OBJECT, 5, K_FIXED4);
    g_case.fixed[4] = ptrv(win(W_SOURCE));
    set_kind(C_LISTBOX, 77, K_FIXED5);
    g_case.fixed[5] = g.chance(5) ? g_count + g.below(3) : (g_count ? g.below(g_count) : 0);
    records(g, 0x6be1e0);
    records(g, 0x6c0540);
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : g.next();
    at_(win(W_APP), 0x2e0) = ptrv(holder);
    set_kind(C_OBJECT, 97, K_VALUE);
    // the panel under test
    BYTE *w = make_plain_window(0, C_WINDOW, g);
    at_(w, 0x100) = ptrv(win(W_LIST_T));
    at_(w, 0x104) = ptrv(win(W_TREE));
    at_(w, 0x10c) = ptrv(xalloc(0x140));
    at_(w, 0x110) = ptrv(win(W_BUTTON_T));
    at_(w, 0x114) = ptrv(win(W_OTHER));
    at_(w, 0x11c) = 0x5ee168;
    const DWORD q = g.below(4);
    if (q) {
        BYTE *qd = xalloc(4 * (q + 1));
        for (DWORD i = 0; i < q; i++)
            at_(qd, 4 * i) = g.below(10);
        at_(w, 0x120) = ptrv(qd), at_(w, 0x124) = q, at_(w, 0x128) = q + 1;
    }
    g_self = w;
}

void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = ptrv(win(W_PARENT));
    (void)g;
}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_notify(Gen &g)
{
    const DWORD k = g.below(5);
    const DWORD from[5] = {at_(g_self, 0x100), at_(g_self, 0x110), at_(g_self, 0x114), ptrv(win(W_OBJECT)), 0};
    g_args[0] = from[k];
    g_args[1] = g.chance(3) ? g.below(4) : 0;
}
void s_show(Gen &g) { g_args[0] = g.chance(3) ? 0 : 1 + g.below(255); }
void s_args(Gen &g)
{
    for (int i = 0; i < 4; i++)
        g_args[i] = g.next();
}
void s_compare(Gen &g)
{
    g_args[0] = g.chance(2) ? 0 : ptrv(TEXTS[g.below(8)]);
    g_args[1] = ptrv(TEXTS[g.below(4)]);
    g_args[2] = ptrv(TEXTS[g.below(4)]);
    g_args[3] = ptrv(win(W_SOURCE));
    const DWORD n = g_count ? g_count : 1;
    if (!g_count) {   // (the compare reads its rows without a test)
        BYTE *e = xalloc(0x18);
        at_(at_<BYTE *>(at<BYTE *>(g_case.fixed[2]), 4), 0) = ptrv(e);
    }
    g_args[4] = g.below(n);
    g_args[5] = g.below(n);
}
void s_none(Gen &) {}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214, 0x6c0540, 0x6c0570, 0x6c0574};

const Test TESTS[] = {
    {"VoiceLogPanel ctor", 0x5948b0, CALL_THIS, 1, ~0u, 200, s_ctor, STUBS, NSTUBS},
    {"VoiceLogPanel deleting dtor", 0x594dc0, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"VoiceLogPanel Notify", 0x5953d0, CALL_THIS, 2, 0, 600, s_notify, STUBS, NSTUBS},
    {"VoiceLogPanel Show", 0x5956a0, CALL_THIS, 1, 0, 300, s_show, STUBS, NSTUBS},
    {"VoiceLogPanel NextQueued", 0x5957d0, CALL_THIS, 0, 0, 60, s_none, STUBS, NSTUBS},
    {"VoiceLogPanel Mouse9", 0x595830, CALL_THIS, 3, ~0u, 20, s_args, STUBS, NSTUBS},
    {"VoiceLogPanel MouseDown", 0x595880, CALL_THIS, 2, ~0u, 20, s_args, STUBS, NSTUBS},
    {"VoiceLogPanel Mouse12", 0x5958d0, CALL_THIS, 4, ~0u, 20, s_args, STUBS, NSTUBS},
    {"VoiceLogPanel Mouse15", 0x595930, CALL_THIS, 2, ~0u, 20, s_args, STUBS, NSTUBS},
    {"VoiceLogPanel Key", 0x595980, CALL_THIS, 1, ~0u, 20, s_args, STUBS, NSTUBS},
    {"VoiceLogPanel Compare", 0x5959d0, CALL_FAST, 6, ~0u, 300, s_compare, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_voice_log)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"ui voice log: the voice log panel, its replay queue and sort"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 27000);
    t.report();
}

}  // namespace zax
