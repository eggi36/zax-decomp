// Checks of the last eleven debug drawings of AIs and behaviours (ai_draw_c5.cpp) against the original with the
// toolkit's harness and the screens' setup (checks_ui_screens.h), as checks_ai_draw_rest4c.cpp: the AI is random
// memory in the extra area with its fields; part, frame, the reporter, the embedded behaviour, +0x14's object and
// the sub-AI are game objects (slot answers of the case, the points they write); the view helpers, the part
// position, the polygon outline, the frame's lines and rectangle, the path's functions, the steering, the
// polygon's constructor and destructor, the beam polygon, the class test and getters, AI_Draw, the sound system's
// listener, sprintf and the fonts' height and drawing are stand-ins; the font list 0x6be1e0 gets a fake vtable and
// records. The colours are the exe's own FieldColor code (local and static ones; the static colours and their
// guard bits are case values); every case runs from a stack area cleared first, so the bytes FieldColor leaves
// unset (7, 0xa, 0xb) and the locals the stand-ins do not write are the same on both sides, and the colours are
// compared by their 16 bytes.
#include "checks_ui_screens.h"

namespace zax {

int hooks_ai_draw_c5(Hook *out);

namespace {

using namespace uic;
using namespace uic::scr;

constinit ModuleHooks g_hooks{"ai_draw_c5", &hooks_ai_draw_c5};
Globals g_globals;

const Stub STUBS[] = {
    UIC_SCREEN_STUBS,
    {0x4fb0a0, CV_THIS, 1, K_VALUE, "LayerPart_Position", "x"},
    {0x4fceb0, CV_FAST, 5, K_VALUE, "DrawView_Arrow", "vqvvvvv"},
    {0x4fccc0, CV_FAST, 3, K_VALUE, "DrawView_Oval", "vqvvr"},
    {0x546d40, CV_THIS, 6, K_VALUE, "DrawPolygonOutline", "vvvr"},
    {0x4b3cb0, CV_THIS, 3, K_VALUE, "Frame_DrawThickLinePts", "qqr"},
    {0x4b33f0, CV_THIS, 5, K_VALUE, "Frame_DrawRectXY", "vvvvr"},
    {0x536e60, CV_THIS, 0, K_BOOL, "CWayPointPath::HasPath"},
    {0x536f70, CV_THIS, 1, K_VALUE, "CWayPointPath::Draw"},
    {0x430c70, CV_THIS, 6, K_BOOL, "CEntityBehaviorMoveToPos::Steer", "vvqx"},
    {0x545cc0, CV_THIS, 0, K_VALUE, "Polygon_Ctor"},
    {0x545cf0, CV_THIS, 0, K_VALUE, "Polygon_Dtor"},
    {0x474510, CV_THIS, 2, K_VALUE, "CFireContinuousFireWeaponBaseAI::BeamPolygon"},
    {0x4fd0b0, CV_THIS, 2, K_VALUE, "AI_Draw"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x48dcc0, CV_CDECL, 0, K_VALUE, "CEntityAnimated_ClassInfo"},
    {0x49b810, CV_CDECL, 0, K_VALUE, "CEntityWalking_ClassInfo"},
    {0x49bde0, CV_CDECL, 0, K_VALUE, "CEntityFlying_ClassInfo"},
    {0x57aef0, CV_CDECL, 0, K_VALUE, "ResourceCache_Get"},
    {0x57c220, CV_THIS, 0, K_FLOAT, "Listener_Get713698"},
    {0x5d0db7, CV_CDECL, 4, K_VALUE, "crt_sprintf", "xsvv"},
    {0x4a6be0, CV_THIS, 0, K_SMALL, "Sequence_Height"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvs"},
    {0x4a6b90, CV_THIS, 5, K_VALUE, "CFont::DrawInt"},
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
    if (g.chance(12)) {
        float f;
        const DWORD b = g.next();
        memcpy(&f, &b, 4);
        return f;
    }
    if (g.chance(6))
        return 0.0f;
    return (float)g.range(-3000, 3000) / 7.0f;
}
float positive_float(Gen &g)
{
    if (g.chance(6))
        return g.chance(2) ? 0.0f : -(float)g.below(50);
    return (float)g.range(1, 3000) / 7.0f;
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

// The part position, slots 43 and 50 and the steering write a point; sprintf writes a text of its arguments
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_OBJECT && (slot == 43 || slot == 50)) {
        write_point(args[0]);
    } else if (cls == -1 && (DWORD)slot == 0x4fb0a0) {
        write_point(args[0]);
    } else if (cls == -1 && (DWORD)slot == 0x430c70) {
        write_point(args[3]);
    } else if (cls == -1 && (DWORD)slot == 0x5d0db7 && args[0]) {
        wsprintfA(at<char *>(args[0]), "%x %x %x", args[2], args[3], fnv_text(2166136261u, at<const char *>(args[1])));
    } else if (cls == -1) {
        if (const int t = template_of((DWORD)slot))
            made(answer, t);
    }
}

char g_object_args[MAXSLOT + 1];
constexpr int W_PART = W_OBJECT, W_FRAME = W_OTHER, W_SUB = W_OTHER2;

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
    object_args(g_object_args, "13:0,17:1,28:2,43:1,48:0,50:1,52:5");
    make_window(W_PART, C_OBJECT);
    make_window(W_FRAME, C_OBJECT);
    make_window(W_SUB, C_OBJECT);
    for (DWORD o = 4; o < 0xfc; o += 4)
        if (o != 0x50)   // (the window's title and scroll info belong to the harness)
            at_(win(W_PART), o) = g.next();
    g_case.argspec[43][0] = A_IGNORE;
    g_case.argspec[50][0] = A_IGNORE;
    g_case.argspec[17][0] = A_TEXT;
    set_kind(C_OBJECT, 13, K_FLOAT);
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
    case_global(0x6bd5c0, g.below(2));
    case_global(0x6bd748, g.below(8));
}

BYTE *new_ai(Gen &g, DWORD size)
{
    BYTE *ai = xalloc(size);
    for (DWORD o = 0; o < size; o += 4)
        at_(ai, o) = g.next();
    g_self = ai;
    g_args[0] = ptrv(win(W_PART));
    g_args[1] = ptrv(win(W_FRAME));
    return ai;
}
void flags(Gen &g, BYTE *ai)
{
    ai[0x1c] = (BYTE)(g.chance(2) ? 0 : 1 + g.below(3));
    ai[0x1d] = (BYTE)(g.chance(2) ? 0 : 1);
}
void s_collide(Gen &g)
{
    BYTE *ai = new_ai(g, 0x40);
    flags(g, ai);
    at_<float>(ai, 0x24) = positive_float(g);
    at_<float>(ai, 0x28) = positive_float(g);
    ai[0x34] = (BYTE)g.below(2);
}
void s_spawn(Gen &g)
{
    BYTE *ai = new_ai(g, 0x20);
    ai[0x14] = (BYTE)g.below(2);
    at_<float>(ai, 0x18) = rnd_float(g);
}
void s_oval(Gen &g)
{
    BYTE *ai = new_ai(g, 0x40);
    flags(g, ai);
    at_<float>(ai, 0x38) = rnd_float(g);
    at_<float>(ai, 0x3c) = rnd_float(g);
}
void s_polygon(Gen &g) { flags(g, new_ai(g, 0x40)); }
void s_ambient(Gen &g)
{
    BYTE *ai = new_ai(g, 0x20);
    at_<float>(ai, 0x10) = positive_float(g);
    at_<float>(ai, 0x14) = positive_float(g);
    at_<float>(ai, 0x18) = g.chance(3) ? positive_float(g) : (float)g.range(1, 40);
}
void s_behaviour(Gen &g, BYTE *ai)
{
    at_(ai, 0x10) = at_(win(W_SUB), 0);   // the embedded behaviour: the game object class's vtable
    if (g.chance(8))
        g_args[0] = 0;
}
void s_approach(Gen &g)
{
    BYTE *ai = new_ai(g, 0xd0);
    s_behaviour(g, ai);
    at_<float>(ai, 0xcc) = g.chance(3) ? 0.0f : rnd_float(g);
}
void s_companion(Gen &g)
{
    BYTE *ai = new_ai(g, 0xe8);
    s_behaviour(g, ai);
    at_<float>(ai, 0xdc) = g.chance(3) ? 0.0f : rnd_float(g);
    at_<float>(ai, 0xe0) = g.chance(3) ? 0.0f : rnd_float(g);
}
void s_chase(Gen &g)
{
    BYTE *ai = new_ai(g, 0x110);
    s_behaviour(g, ai);
    at_(ai, 0x10c) = g.chance(3) ? 0 : ptrv(win(W_SUB));
    at_(ai, 0xf0) = g.below(7);   // (the state names 0x60f874 have seven entries)
    g_args[0] = ptrv(win(W_PART));
}
void s_move_to(Gen &g)
{
    BYTE *ai = new_ai(g, 0xc0);
    const DWORD floats[] = {0x80, 0x84, 0x88, 0x8c, 0x98, 0xac, 0xb0};
    for (DWORD o : floats)
        at_<float>(ai, o) = rnd_float(g);
    if (g.chance(3))
        at_<float>(ai, 0xac) = at_<float>(ai, 0xb0) = 0.0f;
    if (g.chance(2))
        at_<float>(ai, 0x98) = 0.0f;
    if (!g.chance(4)) {   // __ftol of the target: in range
        at_<float>(ai, 0x88) = (float)g.range(-5000, 5000) / 3.0f;
        at_<float>(ai, 0x8c) = (float)g.range(-5000, 5000) / 3.0f;
    }
}
void s_fire(Gen &g)
{
    BYTE *ai = new_ai(g, 0x20);
    ai[8] = (BYTE)(g.chance(5) ? 1 : 0);
    at_(ai, 0x10) = g.chance(5) ? 0 : g.next();
    at_(ai, 0x14) = g.chance(5) ? 0 : ptrv(win(W_SUB));
}
void s_damage(Gen &g)
{
    BYTE *ai = new_ai(g, 0x30);
    at_<float>(ai, 0x18) = rnd_float(g);
    at_<float>(ai, 0x1c) = rnd_float(g);
    at_<float>(ai, 0x20) = rnd_float(g);
    at_<float>(ai, 0x28) = positive_float(g);
}

// Every case runs from a cleared stack area (the drawings' locals start as zeros on both sides)
__attribute__((noinline)) void clear_stack()
{
    volatile BYTE area[0x1400];
    for (int i = (int)sizeof area - 1; i >= 0; i--)
        area[i] = 0;
}
template <DWORD A> DWORD THISCALL from_clear_stack(BYTE *self, DWORD part, DWORD frame)
{
    clear_stack();
    at<void(THISCALL *)(BYTE *, DWORD, DWORD)>(A)(self, part, frame);
    return 0;
}
template <DWORD A> DWORD fn() { return ptrv((void *)&from_clear_stack<A>); }

constexpr DWORD GLOBALS_C5[] = {0x6c02cc, 0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214, 0x6bd5b0, 0x6bd5b4, 0x6bd5b8,
                                0x6bd5bc, 0x6bd710, 0x6bd714, 0x6bd718, 0x6bd71c, 0x6bd738, 0x6bd73c, 0x6bd740, 0x6bd744,
                                0x6bd728, 0x6bd72c, 0x6bd730, 0x6bd734};

}  // namespace

ZAX_CHECKS(ai_draw_c5)
{
    (void)cls;
    (void)n;
    const Test tests[] = {
        {"CCollideTriggerAI Draw", fn<0x41cfb0>(), CALL_THIS, 2, 0, 400, s_collide, STUBS, NSTUBS},
        {"CSpawnPointAI Draw", fn<0x420d00>(), CALL_THIS, 2, 0, 150, s_spawn, STUBS, NSTUBS},
        {"CTouchingOvalTriggerAI Draw", fn<0x421430>(), CALL_THIS, 2, 0, 250, s_oval, STUBS, NSTUBS},
        {"CTouchingPolygonTriggerAI Draw", fn<0x421b80>(), CALL_THIS, 2, 0, 250, s_polygon, STUBS, NSTUBS},
        {"CAmbientSoundAI Draw", fn<0x42dd30>(), CALL_THIS, 2, 0, 800, s_ambient, STUBS, NSTUBS},
        {"CApproachTargetAI Draw", fn<0x42e730>(), CALL_THIS, 2, 0, 250, s_approach, STUBS, NSTUBS},
        {"CEntityBehaviorMoveToPos Draw", fn<0x431530>(), CALL_THIS, 2, 0, 800, s_move_to, STUBS, NSTUBS},
        {"CChaseInvestigateAI Draw", fn<0x46c4b0>(), CALL_THIS, 2, 0, 300, s_chase, STUBS, NSTUBS},
        {"CCompanionAI Draw", fn<0x46e990>(), CALL_THIS, 2, 0, 250, s_companion, STUBS, NSTUBS},
        {"CFireContinuousFireWeaponBaseAI Draw", fn<0x474320>(), CALL_THIS, 2, 0, 300, s_fire, STUBS, NSTUBS},
        {"CDamageExpandingRadiusAI Draw", fn<0x476d00>(), CALL_THIS, 2, 0, 300, s_damage, STUBS, NSTUBS},
    };
    if (!g_globals.n) {
        for (DWORD a : GLOBALS_C5)
            g_globals.add(a, 4);
        g_globals.add(0x6bd5c0, 1);
        g_globals.add(0x6bd748, 1);
    }
    Tally t{"ai draw c5: the debug drawing of eleven AIs and behaviours"};
    run_tests(t, g_hooks, &g_globals, tests, sizeof tests / sizeof tests[0], &base, 52000);
    t.report();
}

}  // namespace zax
