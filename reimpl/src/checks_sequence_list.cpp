// Checks of a sequence's properties file, the image sequence list and the debug sequence window (sequence_list.cpp) against the original with
// the toolkit's harness and the screens' setup (checks_ui_screens.h): the record list's, the frame filters', the
// aliases' and the sequences' functions, the window's title and the blit are stand-ins; the lists, their records'
// objects and the sequences are fakes (game objects); the default list 0x713380 gets a fake vtable and records.
#include "checks_ui_screens.h"

namespace zax {

int hooks_sequence_list(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"sequence_list", &hooks_sequence_list};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x591c90, CV_THIS, 5, K_VALUE, "RecordList_Ctor"},
    {0x591cd0, CV_THIS, 5, K_VALUE, "RecordList_Set"},
    {0x4b1cc0, CV_FAST, 1, K_VALUE, "FrameFilter_Ctor", "xxv"},
    {0x4b1d20, CV_FAST, 0, K_VALUE, "FrameFilter_Dtor", "xx"},
    {0x4b1d50, CV_FAST, 1, K_VALUE, "FrameFilterList_CloneInto", "vxx"},
    {0x55d0c0, CV_THIS, 0, K_VALUE, "ResourceAliases_Dtor"},
    {0x56b260, CV_CDECL, 0, K_VALUE, "Sequence_DefaultFilters"},
    {0x56b950, CV_THIS, 3, K_VALUE, "CSequence::LoadCacheBool", "ssx"},
    {0x416610, CV_THIS, 0, 'e', "CObject_Ctor"},
    {0x56b730, CV_THIS, 0, K_VALUE, "CSequence::Init"},
    {0x40cdc0, CV_THIS, 1, K_VALUE, "CJWindow::SetTitle", "s"},
    {0x56bf00, CV_THIS, 0, K_VALUE, "Sequence_BlendMode"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x5240b0, CV_FAST, 0, K_VALUE, "ObjectStream_Ctor", "xx"},
    {0x524340, CV_FAST, 0, K_VALUE, "ObjectStream_Finish", "xx"},
    {0x5241e0, CV_FAST, 0, K_VALUE, "ObjectStream_Dtor", "xx"},
    {0x416af0, CV_THIS, 1, K_VALUE, "Object_ReadFromStream", "x"},
    {0x4168f0, CV_THIS, 1, K_VALUE, "Object_WriteText", "x"},
    {0x4d24c0, CV_FAST, 0, K_VALUE, "ShowErrorBox", "ss"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// The default list's fake vtable
DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7901, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7902, name ? fnv_text(2166136261u, name) : 0, (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7903, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    (void)rec;
    return ptrv(win(W_OBJECT));
}
DWORD THISCALL list_other(BYTE *) { return 0; }

// Records of 0x80 bytes (the count at -4): objects of the case (game objects), some null or being loaded
BYTE *make_records(Gen &g, DWORD n, bool count_before)
{
    BYTE *mem = xalloc(4 + n * 0x80 + 4);
    BYTE *recs = mem + 4;
    if (count_before)
        at_(mem, 0) = n;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : ptrv(win(g.chance(2) ? W_OBJECT : W_OTHER2));
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    return recs;
}

void list_fields(Gen &g, BYTE *list)
{
    const DWORD n = g.below(5);
    at_(list, 0x30) = g.chance(8) ? 0 : ptrv(make_records(g, n, true));
    at_(list, 0x34) = n;
    const char *const folders[3] = {"Cache/Sequences", "", "Interface"};
    const char *f = folders[g.below(3)];
    for (int i = 0; f[i]; i++)
        list[0x38 + i] = (BYTE)f[i];
}

constexpr DWORD DEFAULT_LIST = 0x713380;
char g_object_args[MAXSLOT + 1];

void base(Gen &g)
{
    if (!g_list_vt[0]) {
        for (DWORD &v : g_list_vt)
            v = ptrv((void *)&list_other);
        g_list_vt[1] = ptrv((void *)&list_name);
        g_list_vt[3] = ptrv((void *)&list_find);
        g_list_vt[37] = ptrv((void *)&list_load);
    }
    screen_base(g, nullptr);
    g_case.effect = [](int cls, int slot, DWORD *, DWORD answer) {
        if (cls == -1)
            if (const int t = template_of((DWORD)slot))
                made(answer, t);
    };
    object_args(g_object_args, "0:1,1:1,3:2,7:1,23:1,25:0,26:0,37:3");
    set_kind(C_OBJECT, 1, K_TEXT);
    set_kind(C_OBJECT, 26, K_FLOAT);
    set_kind(C_OBJECT, 25, K_SMALL);
    make_window(W_OTHER2, C_OBJECT);
    const DWORD n = g.below(5);
    case_global(DEFAULT_LIST, ptrv(g_list_vt));
    case_global(DEFAULT_LIST + 0x30, g.chance(8) ? 0 : ptrv(make_records(g, n, true)));
    case_global(DEFAULT_LIST + 0x34, n);
}

// A sequence list in the extra area (random members, its records)
void s_list(Gen &g)
{
    BYTE *s = xalloc(0x158);
    for (DWORD o = 0; o < 0x158; o += 4)
        at_(s, o) = g.next();
    list_fields(g, s);
    at_(s, 0x13c) = g.chance(3) ? 0 : ptrv(win(W_OTHER));
    s[0x154] = g.chance(2) ? 1 : 0;
    make_window(W_OTHER, C_OBJECT);
    g_self = s;
    g_args[0] = g.below(4);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = g.chance(3) ? 0 : ptrv(xalloc(4));
}
void s_load(Gen &g)
{
    s_list(g);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
}
// The window's list: every record a loaded sequence (a missing sequence ends the game on both sides)
BYTE *window_records(Gen &g, DWORD n)
{
    BYTE *recs = make_records(g, n, true);
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = ptrv(win(W_OBJECT));
        recs[i * 0x80 + 0x7f] = 0;
    }
    return recs;
}
void s_window(Gen &g)
{
    g_self = xalloc(0x10c);
    const DWORD n = 1 + g.below(4);
    case_global(DEFAULT_LIST + 0x30, ptrv(window_records(g, n)));
    case_global(DEFAULT_LIST + 0x34, n);
    g_args[0] = g.chance(3) ? 0 : ptrv(win(W_PARENT));
    g_args[1] = g.chance(4) ? 0 : ptrv(TEXTS[g.below(8)]);
    if (g.chance(4)) {
        g_args[2] = 0;
    } else {
        make_window(W_OTHER, C_OBJECT);
        list_fields(g, win(W_OTHER));
        const DWORD m = 1 + g.below(4);
        at_(win(W_OTHER), 0x30) = ptrv(window_records(g, m));
        at_(win(W_OTHER), 0x34) = m;
        g_args[2] = ptrv(win(W_OTHER));
    }
}
void s_window_dtor(Gen &g)
{
    g_args[0] = g.below(4);
}
void s_player(Gen &g)
{
    BYTE *s = xalloc(0x10c);
    at_(s, 0x100) = ptrv(win(W_OBJECT));
    at_<float>(s, 0x104) = g.chance(8) ? (float)g.next() : (float)g.range(-20, 400) / 9.0f;
    at_<float>(s, 0x108) = g.chance(8) ? 0.0f : (float)g.range(1, 60);
    g_self = s;
    const float dt = g.chance(6) ? 0.0f : (float)g.range(0, 3000) / 1000.0f;
    memcpy(&g_args[0], &dt, 4);
    if (g.chance(10))
        g_args[0] = g.next();
}

// The properties file: the object stream's open (patched for the test, its answer by the case: the first open
// reads, else the second writes; never both fail, that ends the game); the last opened file a ZString of the block
BYTE g_open_first;
BYTE __fastcall fake_open(DWORD, DWORD, const char *path, DWORD mode, DWORD flag)
{
    log_event(0x5242, fnv_text(2166136261u, path), mode | flag << 16);
    return mode == 0x204 ? g_open_first : 1;
}
void s_properties(Gen &g)
{
    make_window(W_OTHER2, C_OBJECT);
    g_self = win(W_OTHER2);
    const char *const folders[3] = {"Cache/Sequences", "", "Interface/Cursors"};
    g_args[0] = ptrv(folders[g.below(3)]);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_open_first = g.chance(2) ? 1 : 0;
    BYTE *z = xalloc(4);
    make_text(z, TEXTS[g.below(8)]);
    case_global(0x713c14, at_(z, 0));
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, DEFAULT_LIST, DEFAULT_LIST + 0x30, DEFAULT_LIST + 0x34, 0x713c14};

const Test TESTS[] = {
    {"SequenceList Ctor", 0x56c080, CALL_THIS, 0, ~0u, 20, s_list, STUBS, NSTUBS},
    {"SequenceList DeletingDtor", 0x56c0e0, CALL_THIS, 1, ~0u, 150, s_list, STUBS, NSTUBS},
    {"SequenceList Dtor", 0x56c1a0, CALL_THIS, 0, 0, 100, s_list, STUBS, NSTUBS},
    {"SequenceList DeletingDtor2", 0x56c320, CALL_THIS, 1, ~0u, 100, s_list, STUBS, NSTUBS},
    {"SequenceList Load", 0x56c250, CALL_THIS, 3, ~0u, 200, s_load, STUBS, NSTUBS},
    {"SequenceList SetFilters", 0x56c300, CALL_THIS, 1, 0, 50, s_list, STUBS, NSTUBS},
    {"SequenceWindow DeletingDtor", 0x56c3d0, CALL_THIS, 1, ~0u, 50, s_window_dtor, STUBS, NSTUBS},
    {"SequenceWindow Ctor", 0x56c400, CALL_THIS, 3, ~0u, 300, s_window, STUBS, NSTUBS},
    {"SequencePlayer Advance", 0x56c5a0, CALL_THIS, 1, 0, 400, s_player, STUBS, NSTUBS},
    {"SequencePlayer Draw", 0x56c600, CALL_THIS, 1, 0, 200, s_player, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(sequence_list)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"sequence list: the image sequence list, its loader, the debug sequence window and player"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 45000);
    const Test props[] = {{"Sequence Properties", 0x56b320, CALL_THIS, 2, 0, 200, s_properties, STUBS, NSTUBS}};
    CodePatch open;
    patch_jump(open, 0x5242b0, (void *)&fake_open);
    run_tests(t, g_hooks, &g_globals, props, 1, &base, 45500);
    unpatch(open);
    t.report();
}

}  // namespace zax
