// Checks of the skeleton's and the turret's debug drawing (ai_draw_rest4c.cpp) against the original with the toolkit's
// harness and the screens' setup (checks_ui_screens.h): the AI is random memory in the extra area with its state,
// sub-AIs and radii; part, frame, the sub-AIs and the embedded drawable are game objects (slot answers of the case,
// the points they return in the block); the view helpers, the part position, AI_Draw, the class test, the table
// sine and cosine, the colours' constructor and atexit, sprintf (which writes a text of its double) and the font's
// height and drawing are stand-ins; the font list 0x6be1e0 gets a fake vtable and records. The colours' guard bits
// are case values (built or not).
#include "checks_ui_screens.h"

namespace zax {

int hooks_ai_draw_rest4c(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ai_draw_rest4c", &hooks_ai_draw_rest4c};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x4fb0a0, CV_THIS, 1, K_VALUE, "LayerPart_Position", "x"},
    {0x4fceb0, CV_FAST, 5, K_VALUE, "DrawView_Arrow", "vqvvvvv"},
    {0x4fccc0, CV_FAST, 3, K_VALUE, "DrawView_Oval", "vqvvv"},
    {0x4fd0b0, CV_THIS, 2, K_VALUE, "AI_Draw"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x48dcc0, CV_CDECL, 0, K_VALUE, "CEntityAnimated_ClassInfo"},
    {0x53f010, CV_THIS, 4, K_VALUE, "FieldColor_CtorRGBA"},
    {0x508ec0, CV_STD, 1, K_FLOAT, "Table_Sin"},
    {0x508da0, CV_STD, 1, K_FLOAT, "Table_Cos"},
    {0x5d0db7, CV_CDECL, 4, K_VALUE, "crt_sprintf", "xsvv"},
    {0x4a6be0, CV_THIS, 0, K_SMALL, "Sequence_Height"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvsvv"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

DWORD g_list_vt[40];
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7901, i, 0);
    return ptrv(TEXTS[i & 7]);
}
DWORD THISCALL list_find(BYTE *, const char *name, int a)
{
    log_event(0x7902, fnv_text(2166136261u, name), (DWORD)a);
    return g_case.values.below(6);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD)
{
    log_event(0x7903, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    return g_case.values.chance(3) ? 0 : 0xf0001234u;
}
DWORD THISCALL list_other(BYTE *) { return 0; }

float rnd_float(Gen &g)
{
    if (g.chance(10)) {
        float f;
        const DWORD b = g.next();
        memcpy(&f, &b, 4);
        return f;
    }
    if (g.chance(6))
        return 0.0f;
    return (float)g.range(-3000, 3000) / 7.0f;
}

void write_point(DWORD at_address)
{
    if (!at_address)
        return;
    Gen &v = g_case.values;
    float *p = at<float *>(at_address);
    p[0] = rnd_float(v);
    p[1] = rnd_float(v);
}

// The part position and slot 40 write a point; sprintf writes a text of its double
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_OBJECT && slot == 40) {
        write_point(args[0]);
    } else if (cls == -1 && (DWORD)slot == 0x4fb0a0) {
        write_point(args[0]);
    } else if (cls == -1 && (DWORD)slot == 0x5d0db7 && args[0]) {
        wsprintfA(at<char *>(args[0]), "%x %x %x", args[2], args[3], fnv_text(2166136261u, at<const char *>(args[1])));
    } else if (cls == -1) {
        if (const int t = template_of((DWORD)slot))
            made(answer, t);
    }
}

char g_object_args[MAXSLOT + 1];
constexpr int W_PART = W_OBJECT, W_FRAME = W_OTHER, W_SUB = W_OTHER2;

BYTE *points(Gen &g)
{
    BYTE *p = xalloc(8);
    at_<float>(p, 0) = rnd_float(g);
    at_<float>(p, 4) = rnd_float(g);
    return p;
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
    object_args(g_object_args, "0:1,28:2,40:1,46:0,47:0,52:5,70:0,72:0");
    make_window(W_PART, C_OBJECT);
    make_window(W_FRAME, C_OBJECT);
    make_window(W_SUB, C_OBJECT);
    for (DWORD o = 4; o < 0xfc; o += 4)
        if (o != 0x50)   // (the window's title and scroll info belong to the harness)
            at_(win(W_PART), o) = g.next();
    g_case.argspec[40][0] = A_IGNORE;
    set_kind(C_OBJECT, 46, K_FLOAT);
    set_kind(C_OBJECT, 47, K_FIXED0);
    set_kind(C_OBJECT, 70, K_FIXED1);
    set_kind(C_OBJECT, 72, K_FIXED2);
    g_case.fixed[0] = g.chance(5) ? 0 : ptrv(points(g));
    g_case.fixed[1] = g.chance(2) ? 0 : ptrv(win(W_SUB));
    g_case.fixed[2] = ptrv(points(g));
    // the font list: its records and fake vtable
    const DWORD n = g.below(6);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(6) ? 1 : 0;
    }
    case_global(0x6be1e0, ptrv(g_list_vt));
    case_global(0x6be210, g.chance(8) ? 0 : ptrv(recs));
    case_global(0x6be214, n);
    case_global(0x713618, g.below(16));
    case_global(0x713a28, g.below(4));
}

void s_skeleton(Gen &g)
{
    BYTE *ai = xalloc(0xb0);
    for (DWORD o = 0; o < 0xb0; o += 4)
        at_(ai, o) = g.next();
    const DWORD subs[4] = {0x28, 0x60, 0x70, 0x80};
    for (DWORD o : subs)
        at_(ai, o) = g.chance(3) ? 0 : ptrv(win(W_SUB));
    at_(ai, 0x38) = at_(win(W_SUB), 0);   // the embedded drawable: the game object class's vtable
    at_(ai, 0xa8) = g.below(5);   // (the names table 0x621c44 has the five states)
    at_<float>(ai, 0x5c) = rnd_float(g);
    at_<float>(ai, 0x7c) = rnd_float(g);
    g_self = ai;
    g_args[0] = ptrv(win(W_PART));
    g_args[1] = ptrv(win(W_FRAME));
}
void s_turret(Gen &g)
{
    BYTE *ai = xalloc(0x40);
    for (DWORD o = 0; o < 0x40; o += 4)
        at_(ai, o) = g.next();
    at_(ai, 0x10) = g.chance(4) ? g.next() : g.below(800);
    at_(ai, 0x14) = g.chance(4) ? g.next() : g.below(300);
    g_self = ai;
    g_args[0] = g.chance(8) ? 0 : ptrv(win(W_PART));
    g_args[1] = ptrv(win(W_FRAME));
}

constexpr DWORD GLOBALS4[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214};

const Test TESTS[] = {
    {"CSkeletonAI Draw", 0x577030, CALL_THIS, 2, 0, 900, s_skeleton, STUBS, NSTUBS},
    {"CTurretAI Draw", 0x58e390, CALL_THIS, 2, 0, 500, s_turret, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ai_draw_rest4c)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        g_globals.add(0x713618, 1);
        g_globals.add(0x713a28, 1);
    }
    Tally t{"ai draw rest 4c: the debug drawing of CSkeletonAI and CTurretAI"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 48000);
    t.report();
}

}  // namespace zax
