// Checks of the hand-written functions of batch C1 (classreg_slots.cpp) against the original, on the toolkit's
// harness (checks_ui_common.h) with the class registrations' stand-ins (checks_classreg_common.h): every callee a
// recording stand-in except the path functions Path_Join/PathName (pure, on the function's own buffer); the
// class variables, the descriptor and field globals, the water warp list's globals and g_App saved, set per
// case and put back. Fakes: the actions, wrappers and lists are "game object" windows (recording vtables), the
// records, descriptors, the application, world, rules and display object blocks of the extra area.
#include "checks_classreg_common.h"

namespace zax {

int hooks_classreg_slots(Hook *out);

namespace {

using namespace classreg;

constinit ModuleHooks g_hooks{"classreg_slots", &hooks_classreg_slots};

#define REG_STUBS                                                                                                        \
    {0x415780, CV_THIS, 9, K_VALUE, "ClassInfo_Register", "vvvv"}, {0x41a650, CV_CDECL, 0, K_VALUE, "CAction_ClassInfo"},  \
        {0x4163b0, CV_CDECL, 0, K_VALUE, "CObject_ClassInfo"}, {0x5acc00, CV_CDECL, 0, K_VALUE, "getter 0x5acc00"},        \
        {0x48bdf0, CV_CDECL, 0, K_VALUE, "getter 0x48bdf0"}, {0x4b2bc0, CV_CDECL, 0, K_VALUE, "getter 0x4b2bc0"},          \
        {0x560ac0, CV_CDECL, 0, K_VALUE, "getter 0x560ac0"},                                                               \
        {0x4190e0, CV_THIS, 3, K_VALUE, "FieldDesc_RegisterExtra", "vvv"}, {0x5d033a, CV_CDECL, 1, K_FIXED1, "operator new"}, \
        {0x4186f0, CV_THIS, 9, K_VALUE, "ObjectField_Ctor", "svvv"}, {0x415370, CV_THIS, 0, K_TEXT, "ClassInfo_ShortName"},  \
        {0x4153b0, CV_THIS, 2, K_VALUE, "ClassInfo_DisplayName", "xs"}, {0x4dec90, CV_FAST, 0, K_VALUE, "ZString_Ctor", "xx"}, \
        {0x4defd0, CV_FAST, 0, K_VALUE, "ZString_Dtor", "xx"}, {0x4e13a0, CV_FAST, 0, K_TEXT, "ZString_c_str", "xx"},         \
        {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "vvv"}, {0x4e02c0, CV_THIS, 3, K_VALUE, "ZString_SetPath3", "vsv"}, \
        {0x416610, CV_THIS, 0, 'e', "CObject_Ctor"}, {0x53f000, CV_THIS, 0, 'e', "FieldColor_Ctor"},                         \
        {0x4173c0, CV_THIS, 0, K_VALUE, "Object_InitFields"}, {0x417230, CV_THIS, 0, K_VALUE, "CObjectBase::ResetFields"},   \
        {0x417710, CV_FAST, 0, K_FIXED0, "operator_new_ecx", "vx"}

// IsKindOf answers 0 or 1 (random upper bytes) ...
constinit const Stub STUBS[] = {REG_STUBS, {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf", "v"}};
// ... or always 1 (CZaxApp's slots read the rules through a null pointer otherwise, the original too)
constinit const Stub STUBS_KIND1[] = {REG_STUBS, {0x416790, CV_THIS, 1, K_ONE, "Object_IsKindOf", "v"}};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// The "game object" class's stack arguments of the case: slot 1 one (the list's name), 2 none (GetClassInfo),
// 19 none, 37 three (the list's load), 39 two (the level list's set)
char g_object_args[MAXSLOT + 1];
void set_object_args()
{
    for (int i = 0; i < MAXSLOT; i++)
        g_object_args[i] = '0';
    g_object_args[MAXSLOT] = 0;
    g_object_args[1] = '1', g_object_args[37] = '3', g_object_args[39] = '2';
    g_case.object_args = g_object_args;
}

// ObjectField_Ctor's last five arguments (the harness logs four), then the class registrations' effect
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == -1 && (DWORD)slot == 0x4186f0) {
        DWORD rest[5] = {args[4], args[5], args[6], args[7], args[8]};
        log_event(0x4186, fnv(2166136261u, rest, sizeof rest), 0);
    }
    register_effect(cls, slot, args, answer);
}

void use_vars(const DWORD *v, int n, Gen &g)
{
    g_nvars = n;
    for (int i = 0; i < n; i++)
        g_vars[i] = v[i];
    set_vars(g);
    g_case.effect = &effect;
}

void base(Gen &g)
{
    set_object_args();
    g_case.argspec[37][0] = A_TEXT, g_case.argspec[37][1] = A_TEXT, g_case.argspec[37][2] = A_RAW;
    g_case.fixed[0] = g.chance(4) ? 0 : ptrv(xalloc(0x60));   // operator_new_ecx
    g_case.fixed[1] = g.chance(4) ? 0 : ptrv(xalloc(0x3c));   // operator new
    for (int i = 0; i < 4; i++)
        make_window(i, C_OBJECT);
    g_case.nwin = 4;
}

template <DWORD V1, DWORD V2 = 0> void s_vars(Gen &g)
{
    const DWORD v[2] = {V1, V2};
    use_vars(v, V2 ? 2 : 1, g);
}

// ---- Static initializers ---------------------------------------------------------------------------------------

template <DWORD V1, DWORD V2, DWORD DESC> void s_extra(Gen &g)
{
    s_vars<V1, V2>(g);
    case_global(DESC, g.chance(5) ? 0 : g.next());
}
template <DWORD V, DWORD G> void s_field(Gen &g)
{
    s_vars<V>(g);
    case_global(G, g.next());
}
template <DWORD V1, DWORD V2, DWORD DESC> void s_desc_class(Gen &g)
{
    s_vars<V1, V2>(g);
    BYTE *d = xalloc(0x40);
    for (int i = 0; i < 0x10; i++)
        at_(d, 4 * i) = g.next();
    case_global(DESC, ptrv(d));
}

// ---- Describe slots ---------------------------------------------------------------------------------------------

// The action (window 1): +8 an inner action (window 2) or 0, +0xc an array whose first element is window 2 or 0,
// +0x10 a count 0-3; the wrapper (window 0) +8 a text or 0; slot 2 answers the class variable's starting value,
// the new class info or a value
template <DWORD V> void s_describe(Gen &g)
{
    s_vars<V>(g);
    BYTE *a = win(1);
    at_(a, 8) = g.chance(3) ? 0 : ptrv(win(2));
    BYTE *arr = xalloc(0x10);
    at_(arr, 0) = g.chance(3) ? 0 : ptrv(win(2));
    at_(a, 0xc) = ptrv(arr);
    at_(a, 0x10) = g.below(4);
    g_self = win(0);
    at_(win(0), 8) = g.chance(2) ? 0 : ptrv(TEXTS[g.below(8)]);
    g_args[0] = ptrv(a);
    g_args[1] = g.chance(3) ? 0 : ptrv(xalloc(4));
    const DWORD k = g.below(3);
    g_case.fixed[2] = k == 0 ? g_var_values[0] : k == 1 ? g_case.fixed[0] : g.next();
    set_kind(C_OBJECT, 2, K_FIXED2);
}

// ---- The record lists ---------------------------------------------------------------------------------------------

// n records of 0x80 bytes: a name at +0, the object at +0x78 (one of a few values, 0 among them), the busy flag +0x7f
BYTE *records(Gen &g, DWORD n)
{
    BYTE *r = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = r + i * 0x80;
        const char *t = TEXTS[1 + g.below(7)];
        for (int k = 0; t[k] && k < 0x20; k++)
            e[k] = (BYTE)t[k];
        at_(e, 0x78) = g.chance(3) ? 0 : 0x1000 * (1 + g.below(4));
        e[0x7f] = g.chance(4) ? 1 : 0;
    }
    return r;
}
DWORD pick_object(Gen &g) { return g.chance(4) ? g.next() : g.chance(3) ? 0 : 0x1000 * (1 + g.below(4)); }

// The level list (window 0): records +0x30, count +0x34
void s_level_list(Gen &g)
{
    s_vars<0x6c2b48>(g);
    const DWORD n = g.below(6);
    g_self = win(0);
    at_(win(0), 0x30) = ptrv(records(g, n));
    at_(win(0), 0x34) = n;
    g_args[0] = pick_object(g);
    g_args[1] = g.next();
}

// The water warp list 0x713c80 (its vtable the recording one of window 3), records 0x713cb0, count 0x713cb4
void s_water_warp(Gen &g)
{
    s_vars<0x713dc8>(g);
    const DWORD n = g.below(6);
    case_global(0x713c80, at_(win(3), 0));
    case_global(0x713cb0, ptrv(records(g, n)));   // (null records with a count crash both sides)
    case_global(0x713cb4, n);
    g_args[0] = pick_object(g);   // ecx: the object
    g_args[1] = ptrv(xalloc(4));  // edx: the out ZString (a stand-in writes it)
    set_kind(C_OBJECT, 1, K_TEXT);
}

// ---- CZaxApp's rules -------------------------------------------------------------------------------------------------

// g_App -> +0x2e0 the world -> +0x38 the rules -> +0x1c the display object (0 or a block of values)
void s_rules(Gen &g)
{
    s_vars<0x713f24>(g);
    BYTE *app = xalloc(0x300), *world = xalloc(0x40), *rules = xalloc(0x20);
    at_(app, 0x2e0) = ptrv(world);
    at_(world, 0x38) = ptrv(rules);
    if (!g.chance(3)) {
        BYTE *d = xalloc(0x60);
        for (int i = 0; i < 0x18; i++)
            at_(d, 4 * i) = g.next();
        at_(rules, 0x1c) = ptrv(d);
    }
    case_global(0x713f14, ptrv(app));
    g_case.fixed[0] = ptrv(xalloc(0x60));   // never null here: the exe calls slot 19 on it
    g_self = win(0);
    g_args[0] = ptrv(xalloc(0x10));
}
// The float slot through a thunk: its st0 as eax
DWORD __cdecl call_rules_float()
{
    const float f = at<float(THISCALL *)(void *)>(0x59fd80)(win(0));
    DWORD b;
    memcpy(&b, &f, 4);
    return b;
}

struct SlotTest {
    Test test;
    DWORD globals[6];
    int nglobals;
};

#define SX(addr, name, v1, v2, desc) \
    {{name, addr, CALL_CDECL, 0, ~0u, 10, &s_extra<v1, v2, desc>, STUBS, NSTUBS}, {v1, v2 ? v2 : v1, desc}, 3}
#define SF(addr, name, v, g) {{name, addr, CALL_CDECL, 0, ~0u, 10, &s_field<v, g>, STUBS, NSTUBS}, {v, g}, 2}
#define SD(addr, name, v1, v2, desc) \
    {{name, addr, CALL_THIS, 0, ~0u, 12, &s_desc_class<v1, v2, desc>, STUBS, NSTUBS}, {v1, v2, desc}, 3}
#define DS(addr, name, v) {{name, addr, CALL_THIS, 2, ~0u, 30, &s_describe<v>, STUBS, NSTUBS}, {v}, 1}

constinit const SlotTest TESTS[] = {
    SX(0x483ee0, "StaticInit_ExtraEntry_6bdcb1", 0x6bdcc4, 0, 0x6bdca0),
    SX(0x48dc50, "StaticInit_ExtraEntry_6bdd5a", 0x6bdd98, 0, 0x6bdd50),
    SX(0x4a86f0, "StaticInit_ExtraEntry_6be7cc", 0x6be7d4, 0, 0x6be38c),
    SX(0x4f95d0, "StaticInit_ExtraEntry_6c29ac", 0x6c29cc, 0, 0x6c29b8),
    SX(0x4f9680, "StaticInit_ExtraEntry_6c29b4", 0x6c29cc, 0, 0x6c299c),
    SX(0x4fca90, "StaticInit_ExtraEntry_6c29ec", 0x6c29f0, 0, 0x6c29dc),
    SX(0x521d20, "StaticInit_ExtraEntry_6d00f9", 0x6d0128, 0, 0x6d00f4),
    SX(0x521d90, "StaticInit_ExtraEntry_6d0120", 0x6d0128, 0, 0x6d00f0),
    SX(0x521e00, "StaticInit_ExtraEntry_6d0101", 0x6d0128, 0, 0x6d00fc),
    SX(0x5b4590, "StaticInit_ExtraEntry_71437a", 0x7143c8, 0x7143c0, 0x7143ac),
    SF(0x4a8660, "StaticInit_PaletteHolderField", 0x6be7d0, 0x6be38c),
    SF(0x4d4410, "StaticInit_HueRemapField", 0x6c0528, 0x6c0520),
    SF(0x4fca00, "StaticInit_SubAIField", 0x6c29f0, 0x6c29dc),
    SD(0x4f96f0, "StaticInit_ChildFieldClass", 0x6c29cc, 0x6c29c8, 0x6c29c4),
    SD(0x560660, "StaticInit_CustomRulesListClass", 0x713150, 0x71314c, 0x713148),
    DS(0x41f880, "OtherMapWrapper_Describe", 0x62e934),
    DS(0x41fa50, "ThisMapWrapper_Describe", 0x62e934),
    DS(0x4b8300, "SingleActionWrapper_Describe", 0x6bfd30),
    DS(0x4c80f0, "MultipleActionsBaseWrapper_Describe", 0x6bfd30),
    {{"LevelOfDifficultyList_SetObjectValue", 0x4fe160, CALL_THIS, 2, 0xff, 40, &s_level_list, STUBS, NSTUBS}, {0x6c2b48}, 1},
    {{"WaterWarpEffect_FilePath", 0x598280, CALL_FAST, 2, 0xff, 60, &s_water_warp, STUBS, NSTUBS},
     {0x713dc8, 0x713c80, 0x713cb0, 0x713cb4},
     4},
    {{"ZaxApp_RulesPair", 0x59fca0, CALL_THIS, 1, ~0u, 20, &s_rules, STUBS_KIND1, NSTUBS}, {0x713f24, 0x713f14}, 2},
    {{"ZaxApp_RulesFloat", 0 /* call_rules_float, set at run time */, CALL_CDECL, 0, ~0u, 20, &s_rules, STUBS_KIND1, NSTUBS},
     {0x713f24, 0x713f14},
     2},
    {{"ZaxApp_RulesColor", 0x59fe70, CALL_THIS, 1, ~0u, 20, &s_rules, STUBS_KIND1, NSTUBS}, {0x713f24, 0x713f14}, 2},
};

}  // namespace

ZAX_CHECKS(classreg_slots)
{
    (void)cls;
    (void)n;
    Tally t{"class registration: the functions that begin with a registration (static initializers, Describe slots, lists, rules)"};
    const int count = sizeof TESTS / sizeof TESTS[0];
    for (int i = 0; i < count; i++) {
        Globals globals;
        for (int k = 0; k < TESTS[i].nglobals; k++) {
            bool dup = false;
            for (int j = 0; j < globals.n; j++)
                dup = dup || globals.address[j] == TESTS[i].globals[k];
            if (!dup)
                globals.add(TESTS[i].globals[k], 4);
        }
        Test test = TESTS[i].test;
        if (!test.address)
            test.address = ptrv((void *)&call_rules_float);
        run_tests(t, g_hooks, &globals, &test, 1, &base, 61000u + (unsigned)i * 10007u);
    }
    t.report();
}

}  // namespace zax
