// Checks of the window looks (ui_looks.cpp) against the original with the toolkit's harness (checks_ui_common.h):
// the looks are objects of the extra area whose owner is a fake button (window 1; its drawing members random)
// and whose frame is a fake CFrame (window 2). The blit, the opacity, the font's measures, the palette index, the
// bevel, atexit and the enabled test are stand-ins. The factories' statics (0x6c1040, 0x6c1ac0) and their guards
// are case values; for the text look factory the font list 0x6be1e0 gets the vtable of a fake of this file
// (slot 1: a name, slot 37: the loaded font) and records of the case, and Path_Join/PathName run.
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_looks(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_looks", &hooks_ui_looks};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x40c630, CV_THIS, 0, K_BOOL, "CJWindow::IsEnabled"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"},
    {0x4a6140, CV_FAST, 0, K_SMALL, "CFont::LineHeight", "vx"},
    {0x4a7310, CV_THIS, 2, K_SMALL, "CFont::TextWidth", "sv"},
    {0x53f160, CV_THIS, 0, K_VALUE, "FieldColor_PaletteIndex"},
    {0x40a740, CV_FAST, 6, K_VALUE, "Bevel_Draw", "vvrvvvvv"},
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x5788c0, CV_THIS, 0, K_VALUE, "CMessageBase::Dtor"},
    {0x5994d0, CV_THIS, 0, K_VALUE, "CLook::Dtor"},
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x5a1ee0, CV_THIS, 0, K_FIXED1, "GetMainMenuDisplayOptions"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_OWNER = 1, W_FRAME_ = 2, W_APP = 3;

// The font list's fake vtable: slot 1 the name of an index, slot 37 (folder, file, record) loads a font
DWORD g_list_vt[40];
const char *const NAMES[] = {"Fonts/Arial12", "Big", "", "Interface/Fonts/Small Gold"};
DWORD THISCALL list_name(BYTE *, DWORD i)
{
    log_event(0x7e01, i, 0);
    return ptrv(NAMES[i & 3]);
}
DWORD THISCALL list_load(BYTE *, const char *dir, const char *file, DWORD rec)
{
    log_event(0x7e02, fnv_text(2166136261u, dir), fnv_text(2166136261u, file));
    log_event(0x7e03, rec, 0);
    return 0xf0000000u | (rec & 0xffff);
}
DWORD THISCALL list_other(BYTE *) { return 0; }
void init_vtable()
{
    for (DWORD &v : g_list_vt)
        v = ptrv((void *)&list_other);
    g_list_vt[1] = ptrv((void *)&list_name);
    g_list_vt[37] = ptrv((void *)&list_load);
}

void base(Gen &g)
{
    if (!g_list_vt[0])
        init_vtable();
    BYTE *o = make_plain_window(W_OWNER, C_BUTTON, g);
    for (DWORD k = 0x100; k < 0x130; k++)
        o[k] = g.chance(2) ? 0 : (BYTE)g.next();
    at_(o, 0x108) = g.below(4);
    at_(o, 0x110) = g.chance(3) ? 0 : g.next();
    at_(o, 0x114) = g.chance(2) ? 0 : g.next();
    at_(o, 0x34) = (DWORD)g.range(-5, 400), at_(o, 0x38) = (DWORD)g.range(-5, 100);
    o[0x7a] = (BYTE)g.range(-3, 6);
    o[0x65] = g.chance(4);
    o[0xac] = (BYTE)g.next(), o[0xbc] = (BYTE)g.next(), o[0xaa] = (BYTE)g.next(), o[0xba] = (BYTE)g.next();
    make_window(W_FRAME_, C_CFRAME);
    set_kind(C_BUTTON, 45, K_FIXED0);
    set_kind(C_BUTTON, 79, K_BOOL);
    set_kind(C_BUTTON, 44, g.chance(3) ? K_ZERO : K_TEXT);
    g_case.fixed[0] = g.next();
    BYTE *look = xalloc(0x10);
    at_(look, 0) = 0x5f88b4;
    at_(look, 4) = ptrv(o);
    BYTE *factory = xalloc(0x18);
    at_(factory, 4) = g.next();
    at_(look, 8) = ptrv(factory);
    g_self = look;
    g_args[0] = ptrv(win(W_FRAME_));
    // the statics and their guards
    case_global(0x716190, g.chance(2) ? 1 : 0);
    case_global(0x6c1044, g.chance(2) ? 1 : 0);
    case_global(0x6c1040, g.next());
    case_global(0x6c1ab8, g.chance(4) ? 1 : 0);
    // the main menu's display options (fonts +0x18, +0x1c, +0x20, the hover effect +0x24)
    BYTE *opt = xalloc(0x30);
    for (DWORD k = 0x18; k <= 0x20; k += 4)
        at_(opt, k) = g.chance(4) ? g.next() : g.below(6);
    at_(opt, 0x24) = g.next();
    g_case.fixed[1] = ptrv(opt);
    // the font list's records (0x80 bytes: +0x78 the font, +0x7f being loaded)
    const DWORD n = g.below(5);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(2) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(4) ? 1 : 0;
    }
    case_global(0x6be210, g.chance(8) ? 0 : ptrv(recs));
    case_global(0x6be214, n);
    case_global(0x6be1e0, ptrv(g_list_vt));
    BYTE *app = make_window(W_APP, C_OBJECT);
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : g.next();
    at_(app, 0x2e0) = ptrv(holder);
    case_global(0x713f14, ptrv(app));
}

void s_none(Gen &) {}
void s_flags(Gen &g) { g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_object(Gen &g) { g_self = xalloc(0x10); s_flags(g); }
void s_make(Gen &g)
{
    g_self = xalloc(0x18);
    at_(g_self, 4) = g.next();
    g_args[0] = ptrv(win(W_OWNER));
}
void s_ctor(Gen &g)
{
    g_self = xalloc(0x10);
    g_args[0] = g.next();
    g_args[1] = ptrv(win(W_OWNER));
}

constexpr DWORD GLOBALS4[] = {0x7161a0, 0x7161a4, 0x7161a8, 0x7161ac, 0x6c1040, 0x6c1ac0, 0x6c1ac4, 0x6c1ac8,
                              0x6c1acc, 0x6c1ad0, 0x6be210, 0x6be214, 0x6be1e0, 0x713f14};
constexpr DWORD GLOBALS1[] = {0x716190, 0x6c1044, 0x6c1ab8};

const Test TESTS[] = {
    {"ButtonLook Draw", 0x4e2240, CALL_THIS, 1, 0, 800, s_none, STUBS, NSTUBS},
    {"ButtonLookFactory Get", 0x4e2610, CALL_CDECL, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"ButtonLookFactory dtor", 0x4e2670, CALL_THIS, 1, ~0u, 20, s_object, STUBS, NSTUBS},
    {"ButtonLookFactory Make", 0x4e26a0, CALL_THIS, 1, ~0u, 40, s_make, STUBS, NSTUBS},
    {"TextLook ctor", 0x4e2b80, CALL_THIS, 2, ~0u, 30, s_ctor, STUBS, NSTUBS},
    {"TextLook dtor", 0x4e2bb0, CALL_THIS, 1, ~0u, 20, s_object, STUBS, NSTUBS},
    {"TextLook Fit", 0x4e2df0, CALL_THIS, 0, 0, 150, s_none, STUBS, NSTUBS},
    {"TextLookFactory Get", 0x4e3680, CALL_CDECL, 0, ~0u, 400, s_none, STUBS, NSTUBS},
    {"TextLookFactory dtor", 0x4e38b0, CALL_THIS, 1, ~0u, 20, s_object, STUBS, NSTUBS},
    {"TextLookFactory Make", 0x4e38e0, CALL_THIS, 1, ~0u, 40, s_make, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_looks)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        for (DWORD a : GLOBALS1)
            g_globals.add(a, 1);
    }
    Tally t{"ui looks: the button and text looks and their factories"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 21000);
    t.report();
}

}  // namespace zax
