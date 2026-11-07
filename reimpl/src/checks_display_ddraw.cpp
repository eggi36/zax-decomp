// Checks of the DirectDraw surface handling (display_ddraw.cpp) against the original with fake DirectDraw objects:
// an IDirectDraw, the primary surface, the back buffer, a third surface, a clipper and a palette whose vtables are
// recording functions of this file (every call with its arguments into the side's log; the HRESULTs from the case's
// answers: success mostly, a DirectDraw error now and then, DDERR_SURFACELOST from IsLost; CreateSurface,
// GetAttachedSurface, CreateClipper and CreatePalette hand out the fakes; EnumDisplayModes calls the callback with the
// case's modes; Lock fills in the surface description), DirectDrawCreate, GetClientRect, ClientToScreen and OffsetRect
// as recording import stand-ins, the screen frame a fake of the toolkit harness (recording CFrame vtable), the exe
// functions outside the module (the frame's depth and pitch, the palette, the INI, the desktop depth, the error box)
// as stand-ins and exit leaving the side. Compared: al (or eax), the call log, the block of fakes and the globals.
// Cases: full screen and windowed, 8/16/24/32 bits and odd depths, every CreateSurface fallback, failing DirectDraw
// calls at every step, the early return, the desktop depth errors; lost surfaces and failing Locks (up to the 100 tries);
// the flip in both modes; the release with and without pages; the callback on random descriptions (aspect, size,
// depth, 5-6-5 or 5-5-5, a full list); the comparator on random pairs.
#include "checks_sys_common.h"

namespace zax {

int hooks_display_ddraw(Hook *out);

namespace {

using namespace sysc;

constinit ModuleHooks g_hooks{"display_ddraw", &hooks_display_ddraw};
Globals g_globals;

constexpr DWORD G_REPORTER_ADDRESS = 0x6c02cc;
constexpr DWORD DDRAW = 0x713334, BACK = 0x713338, THIRD = 0x71333c, CLIPPER = 0x713340, PALETTE = 0x713344, PRIMARY = 0x713348,
                OWN_BACK = 0x713284, LOCKED = 0x713318, LOCK_DESC = 0x7132a8, FULL_SCREEN = 0x6210c0, WINDOW = 0x713e18;

// ---- The fake DirectDraw objects ----------------------------------------------------------------------------------

struct Fake {
    DWORD vt;
    DWORD id;
};
Fake g_dd, g_primary, g_back, g_third, g_clipper, g_palette;
DWORD g_vt_dd[40], g_vt_surface[40], g_vt_clipper[40], g_vt_palette[40];

// Per case: how often a call fails (1 in n; 0 never), the modes EnumDisplayModes reports, Lock's pitch
DWORD g_fail_one_in;
DWORD g_lost_one_in;
constexpr int MAXMODES = 16;
alignas(4) BYTE g_modes[MAXMODES][0x6c];
int g_nmodes;
int g_pitch;

DWORD hr(DWORD id)
{
    const DWORD r = g_fail_one_in && ans_chance(g_fail_one_in) ? 0x88760000u | (ans() & 0xffff) : 0;
    log_event(0xd0ff, id, r);
    return r;
}
DWORD obj_id(DWORD self) { return self ? at<Fake *>(self)->id : 0; }

DWORD __stdcall c_other(DWORD self)
{
    rec(0xd000, obj_id(self));
    return 0;
}
DWORD __stdcall c_release(DWORD self)
{
    rec(0xd002, obj_id(self));
    return 0;
}
// IDirectDraw
DWORD __stdcall dd_create_clipper(DWORD self, DWORD flags, DWORD *out, DWORD outer)
{
    rec(0xd104, obj_id(self), flags, ptrv(out), outer);
    const DWORD r = hr(0xd104);
    if (!r)
        *out = ptrv(&g_clipper);
    return r;
}
DWORD __stdcall dd_create_palette(DWORD self, DWORD flags, const BYTE *entries, DWORD *out, DWORD outer)
{
    rec(0xd105, obj_id(self), flags, fnv(2166136261u, entries, 0x400), ptrv(out) ^ outer);
    const DWORD r = hr(0xd105);
    if (!r)
        *out = ptrv(&g_palette);
    return r;
}
DWORD __stdcall dd_create_surface(DWORD self, const BYTE *desc, DWORD *out, DWORD outer)
{
    rec(0xd106, obj_id(self), fnv(2166136261u, desc, 0x6c), ptrv(out), outer);
    const DWORD r = hr(0xd106);
    if (!r)
        *out = ptrv(out) == PRIMARY ? ptrv(&g_primary) : ptrv(&g_back);
    return r;
}
DWORD __stdcall dd_enum_modes(DWORD self, DWORD flags, DWORD desc, const DWORD *context, DWORD callback)
{
    rec(0xd108, obj_id(self), flags, desc, canon(ptrv(context)));
    log_event(0xd1080, callback, *context);
    if (g_fail_one_in && ans_chance(4)) {   // (the flag DDEDM_REFRESHRATES refused, the second call without it)
        log_event(0xd1081, flags, 0);
        return 0x88760001;
    }
    for (int i = 0; i < g_nmodes; i++) {
        const DWORD r = at<DWORD(__stdcall *)(const BYTE *, const DWORD *)>(callback)(g_modes[i], context);
        log_event(0xd1082, (DWORD)i, r);
    }
    return hr(0xd108);
}
DWORD __stdcall dd_cooperative(DWORD self, DWORD window, DWORD flags)
{
    rec(0xd114, obj_id(self), window, flags);
    return hr(0xd114);
}
DWORD __stdcall dd_display_mode(DWORD self, DWORD w, DWORD h, DWORD bits)
{
    rec(0xd115, obj_id(self), w, h, bits);
    return hr(0xd115);
}
// IDirectDrawSurface
DWORD __stdcall s_blt(DWORD self, const int *to, DWORD from, const int *from_rect, DWORD flags, DWORD fx)
{
    rec(0xd205, obj_id(self), to ? fnv(2166136261u, to, 16) : 0, obj_id(from), from_rect ? fnv(2166136261u, from_rect, 16) : 0);
    log_event(0xd2050, flags, fx);
    return hr(0xd205);
}
DWORD __stdcall s_flip(DWORD self, DWORD with, DWORD flags)
{
    rec(0xd20b, obj_id(self), with, flags);
    return hr(0xd20b);
}
DWORD __stdcall s_attached(DWORD self, const DWORD *caps, DWORD *out)
{
    rec(0xd20c, obj_id(self), caps ? *caps : 0xdead, ptrv(out));
    const DWORD r = hr(0xd20c);
    if (!r)
        *out = ptrv(&g_back);
    return r;
}
DWORD __stdcall s_is_lost(DWORD self)
{
    rec(0xd218, obj_id(self));
    const DWORD r = g_lost_one_in && ans_chance(g_lost_one_in) ? 0x887601c2 : ans_chance(20) ? 0x88760005 : 0;
    log_event(0xd2180, r, 0);
    return r;
}
DWORD __stdcall s_lock(DWORD self, DWORD rect, BYTE *desc, DWORD flags, DWORD event)
{
    rec(0xd219, obj_id(self), rect, fnv(2166136261u, desc, 0x6c), flags ^ event << 8);
    const DWORD r = hr(0xd219);
    if (!r) {
        at_(desc, 0x10) = (DWORD)g_pitch;
        at_(desc, 0x24) = 0x5f000000u | (ans() & 0xffff00);
        at_(desc, 8) = 0x1e0;
        at_(desc, 0xc) = 0x280;
    }
    return r;
}
DWORD __stdcall s_restore(DWORD self)
{
    rec(0xd21b, obj_id(self));
    return hr(0xd21b);
}
DWORD __stdcall s_set_clipper(DWORD self, DWORD c)
{
    rec(0xd21c, obj_id(self), obj_id(c));
    return hr(0xd21c);
}
DWORD __stdcall s_set_palette(DWORD self, DWORD p)
{
    rec(0xd21f, obj_id(self), obj_id(p));
    return hr(0xd21f);
}
DWORD __stdcall s_unlock(DWORD self, DWORD p)
{
    rec(0xd220, obj_id(self), p);
    return hr(0xd220);
}
// IDirectDrawClipper
DWORD __stdcall c_set_hwnd(DWORD self, DWORD flags, DWORD window)
{
    rec(0xd308, obj_id(self), flags, window);
    return hr(0xd308);
}

void init_fakes()
{
    if (g_vt_dd[0])
        return;
    DWORD *const vts[4] = {g_vt_dd, g_vt_surface, g_vt_clipper, g_vt_palette};
    for (DWORD *vt : vts) {
        for (int i = 0; i < 40; i++)
            vt[i] = ptrv((void *)&c_other);
        vt[2] = ptrv((void *)&c_release);
    }
    g_vt_dd[4] = ptrv((void *)&dd_create_clipper);
    g_vt_dd[5] = ptrv((void *)&dd_create_palette);
    g_vt_dd[6] = ptrv((void *)&dd_create_surface);
    g_vt_dd[8] = ptrv((void *)&dd_enum_modes);
    g_vt_dd[20] = ptrv((void *)&dd_cooperative);
    g_vt_dd[21] = ptrv((void *)&dd_display_mode);
    g_vt_surface[5] = ptrv((void *)&s_blt);
    g_vt_surface[11] = ptrv((void *)&s_flip);
    g_vt_surface[12] = ptrv((void *)&s_attached);
    g_vt_surface[24] = ptrv((void *)&s_is_lost);
    g_vt_surface[25] = ptrv((void *)&s_lock);
    g_vt_surface[27] = ptrv((void *)&s_restore);
    g_vt_surface[28] = ptrv((void *)&s_set_clipper);
    g_vt_surface[31] = ptrv((void *)&s_set_palette);
    g_vt_surface[32] = ptrv((void *)&s_unlock);
    g_vt_clipper[8] = ptrv((void *)&c_set_hwnd);
    g_dd = {ptrv(g_vt_dd), 0xdd01};
    g_primary = {ptrv(g_vt_surface), 0x5a01};
    g_back = {ptrv(g_vt_surface), 0x5a02};
    g_third = {ptrv(g_vt_surface), 0x5a03};
    g_clipper = {ptrv(g_vt_clipper), 0xc101};
    g_palette = {ptrv(g_vt_palette), 0xba01};
}

// ---- Import stand-ins ---------------------------------------------------------------------------------------------

DWORD WINAPI i_ddraw_create(DWORD guid, DWORD *out, DWORD outer)
{
    rec(0xa010, guid, ptrv(out), outer);
    const DWORD r = hr(0xa010);
    if (!r)
        *out = ptrv(&g_dd);
    return r;
}
DWORD WINAPI i_client_rect(DWORD w, int *r)
{
    rec(0xa238, w);
    r[0] = 0, r[1] = 0;
    r[2] = ans_chance(4) ? (int)ans_below(200) : 0x280;
    r[3] = ans_chance(4) ? (int)ans_below(200) : 0x1e0;
    return 1;
}
DWORD WINAPI i_client_to_screen(DWORD w, int *p)
{
    rec(0xa240, w, (DWORD)p[0], (DWORD)p[1]);
    p[0] += (int)ans_below(300) - 20;
    p[1] += (int)ans_below(300) - 20;
    return 1;
}
DWORD WINAPI i_offset_rect(int *r, int dx, int dy)
{
    rec(0xa23c, fnv(2166136261u, r, 16), (DWORD)dx, (DWORD)dy);
    r[0] += dx, r[1] += dy, r[2] += dx, r[3] += dy;
    return 1;
}
const Imp IMPORTS[] = {{0x5ea010, (void *)&i_ddraw_create}, {0x5ea238, (void *)&i_client_rect}, {0x5ea240, (void *)&i_client_to_screen},
                       {0x5ea23c, (void *)&i_offset_rect}};

// ---- Exe stand-ins --------------------------------------------------------------------------------------------------

const Stub STUBS[] = {
    {0x4aa240, CV_THIS, 1, K_VALUE, "CFrame_SetDepth"},
    {0x4aa2c0, CV_THIS, 2, K_VALUE, "CFrame_SetPitch", "vv"},
    {0x5268f0, CV_THIS, 1, K_VALUE, "Palette_SetKind"},
    {0x568870, CV_THIS, 2, K_VALUE, "ScreenFrame_SetPalette", "vb"},
    {0x4a4670, CV_FAST, 2, K_VALUE, "Ini_GetInt", "vvsv"},
    {0x4a4690, CV_FAST, 2, K_VALUE, "Ini_WriteInt", "vvsv"},
    {0x53f010, CV_FAST, 4, K_VALUE, "FieldColor_CtorRGBA", "xxvvvv"},
    {0x4d2530, CV_CDECL, 0, K_FIXED4, "Platform_ScreenDepth"},
    {0x4d24c0, CV_FAST, 0, K_VALUE, "ShowErrorBox", "ss"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// ---- The case -------------------------------------------------------------------------------------------------------

constexpr int W_SELF = 0, W_REPORTER = 1;
int g_test;
BYTE *g_frame;

// The frame's clip rectangle (slot 36) written through its argument (a local of the caller)
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == C_CFRAME && slot == 36 && args[0]) {
        Gen &v = g_case.values;
        int *r = at<int *>(args[0]);
        r[0] = v.range(-10, 50), r[1] = v.range(-10, 50), r[2] = r[0] + v.range(0, 700), r[3] = r[1] + v.range(0, 500);
    }
}

void make_mode(BYTE *d, Gen &g)
{
    memset(d, 0, 0x6c);
    at_(d, 0) = 0x6c;
    const DWORD sizes[6][2] = {{0x280, 0x1e0}, {0x320, 0x258}, {0x400, 0x300}, {0x140, 0xf0}, {0x280, 0x190}, {0x320, 0}};
    const DWORD k = g.chance(5) ? g.below(6) : g.below(2);
    at_(d, 0xc) = g.chance(20) ? g.next() & 0xfff : sizes[k][0];
    at_(d, 8) = g.chance(20) ? g.next() & 0xfff : sizes[k][1];
    const DWORD bits[5] = {8, 16, 24, 32, 16};
    at_(d, 0x54) = g.chance(15) ? g.below(40) : bits[g.below(5)];
    at_(d, 0x5c) = g.chance(2) ? 0x7e0 : 0x3e0;
}

// The common part of every case: the fakes, the screen frame (a CFrame fake with the case's members), the reporter,
// the globals
void base(Gen &g)
{
    init_fakes();
    note_stack();
    g_case.effect = &effect;
    g_case.nwin = 2;
    make_window(W_SELF, C_CFRAME);
    make_window(W_REPORTER, C_OBJECT);
    // the frame in the extra area with the CFrame fake's vtable (a window of the harness has its title ZString at
    // +0x50, the frame's "in video memory" byte)
    BYTE *s = xalloc(0x100);
    at_(s, 0) = at_(win(W_SELF), 0);
    for (DWORD o = 4; o < 0xe8; o += 4)
        at_(s, o) = g.chance(3) ? g.below(4) : g.next();
    at_(s, 0x44) = g.chance(8) ? 0xffffffff : g.below(3);
    at_(s, 0x4c) = g.below(4);
    // (no screen only for the release: the others read the screen's mode list through the reporter)
    at_(win(W_REPORTER), 4) = g_test == 1 && g.chance(5) ? 0 : ptrv(s);
    g_frame = s;
    const DWORD depths[6] = {8, 0x10, 0x18, 0x20, 0x10, 0xf};
    g_case.fixed[0] = depths[g.below(6)];                     // slot 29: the frame's depth
    g_case.fixed[1] = g.chance(3) ? 0x320 : 0x280;           // slot 45: width
    g_case.fixed[2] = g.chance(3) ? 0x258 : 0x1e0;           // slot 39: height
    g_case.fixed[3] = g.chance(6) ? 1 : 0;                   // slot 31: Lock has nothing to do
    g_case.fixed[4] = depths[g.below(4)];                    // the desktop's depth
    set_kind(C_CFRAME, 29, K_FIXED0);
    set_kind(C_CFRAME, 45, K_FIXED1);
    set_kind(C_CFRAME, 39, K_FIXED2);
    set_kind(C_CFRAME, 31, K_FIXED3);
    g_case.argspec[36][0] = A_IGNORE;
    g_case.argspec[28][0] = A_RECT;
    g_case.argspec[28][1] = A_IGNORE;   // (a colour local the colour's stand-in leaves as it is)
    g_case.argspec[24][0] = A_RECT;
    g_fail_one_in = g.chance(3) ? 0 : 3 + g.below(25);
    g_lost_one_in = g.chance(2) ? 0 : 2 + g.below(6);
    const int pitches[6] = {0x500, 0x280, 0xa00, 0x780, 0x640, -0x500};
    g_pitch = g.chance(8) ? (int)g.next() : pitches[g.below(6)];
    g_nmodes = (int)g.below(MAXMODES + 1);
    for (int i = 0; i < g_nmodes; i++)
        make_mode(g_modes[i], g);
    case_global(G_REPORTER_ADDRESS, ptrv(win(W_REPORTER)));
    const bool no_dd = g.chance(3);
    const bool no_back = (g_test == 0 || g_test == 1 || g_test == 5) && g.chance(4);   // (Lock and Unlock use it unchecked)
    case_global(DDRAW, no_dd ? 0 : ptrv(&g_dd));
    case_global(BACK, no_back ? 0 : ptrv(&g_back));
    case_global(THIRD, g.chance(2) ? 0 : ptrv(&g_third));
    case_global(CLIPPER, g.chance(2) ? 0 : ptrv(&g_clipper));
    case_global(PALETTE, g.chance(2) ? 0 : ptrv(&g_palette));
    const DWORD full = g.below(2);
    // no primary surface only where the code tests for one (windowed flip) or makes it (the mode set): Lock, the full
    // screen flip and the release (through the flip) use it unchecked, in the original as well
    // (with DirectDraw and a back buffer there is always a primary surface: the mode set's release flips through it)
    case_global(PRIMARY, ((g_test == 0 && (no_dd || no_back)) || (g_test == 5 && !full)) && g.chance(6) ? 0 : ptrv(&g_primary));
    // (a back buffer is always a surface of its own once a mode was set: a stale one with the flag 0 would make the
    // mode set's release flip through the primary surface it has just released, in the original as well)
    case_global(OWN_BACK, no_back ? g.below(2) : 1);
    case_global(LOCKED, g.chance(3) ? 1 : 0);
    case_global(FULL_SCREEN, full);
    case_global(WINDOW, 0x77770000u | g.below(0x100));
    case_global(0x633388, g.below(2));
}
// The functions and their arguments
DWORD *g_w, *g_h;
DWORD g_a[4];
alignas(4) BYTE g_desc[0x6c];
DWORD g_depth_ctx;

void build(Gen &g)
{
    memset(at<void *>(LOCK_DESC), 0x5d, 0x6c);   // the same start on both sides
    base(g);
    switch (g_test) {
    case 0: {   // SetMode
        g_w = reinterpret_cast<DWORD *>(xalloc(16));
        g_h = g_w + 1;
        const DWORD sizes[5][2] = {{0x280, 0x1e0}, {0x320, 0x258}, {0x400, 0x300}, {0x280, 0x258}, {0x100, 0x100}};
        const DWORD k = g.below(5);
        *g_w = sizes[k][0];
        *g_h = sizes[k][1];
        const DWORD depths[7] = {8, 0x10, 0x18, 0x20, 0xf, 0, 0x10};
        g_a[0] = g.chance(10) ? g.below(40) : depths[g.below(7)];
        g_a[1] = g.chance(3) ? g_a[0] : depths[g.below(7)];
        if (g.chance(4)) {   // the early return: the frame has the mode already
            g_case.fixed[0] = g_a[0];
            g_case.fixed[1] = *g_w;
            g_case.fixed[2] = *g_h;
        }
        break;
    }
    case 4: {   // the callback
        make_mode(g_desc, g);
        if (g.chance(4)) {
            const DWORD sizes[4][2] = {{0x280, 0x1e0}, {0x320, 0x258}, {0x280, 0x1e1}, {0x2bc, 0x1e0}};
            const DWORD k = g.below(4);
            at_(g_desc, 0xc) = sizes[k][0];
            at_(g_desc, 8) = sizes[k][1];
        }
        g_depth_ctx = g.chance(3) ? at_(g_desc, 0x54) : (g.below(4) + 1) * 8;
        at_(g_frame, 0x4c) = g.chance(5) ? 10 + g.below(3) : g.below(10);
        break;
    }
    default: break;
    }
}

DWORD call_side()
{
    const DWORD set_mode[4] = {ptrv(g_w), ptrv(g_h), g_a[0], g_a[1]};
    const DWORD callback[2] = {ptrv(g_desc), ptrv(&g_depth_ctx)};
    switch (g_test) {
    case 0: return call_this(0x567f20, g_frame, 4, set_mode) & 0xff;
    case 1: return call_this(0x5678c0, nullptr, 0, nullptr), 0;
    case 2: return call_this(0x567bb0, g_frame, 0, nullptr), 0;
    case 3: return call_this(0x567c90, g_frame, 0, nullptr), 0;
    case 4: return call_this(0x567d60, nullptr, 2, callback);
    case 5: return call_this(0x5693a0, g_frame, 0, nullptr) & 0xff;
    default: return 0;
    }
}

DWORD call()
{
    imports_on(IMPORTS, sizeof IMPORTS / sizeof IMPORTS[0]);
    const DWORD r = run_escapable(&call_side);
    imports_off();
    // what the call left in Lock's description and the size arguments
    log_event(0xd5c, fnv(2166136261u, at<void *>(LOCK_DESC), 0x6c), g_test == 0 ? *g_w ^ *g_h << 16 : 0);
    return r;
}

struct TestDef {
    const char *name;
    int count;
};
const TestDef TESTS[] = {{"ScreenFrame_SetMode", 1500}, {"ScreenFrame_Release", 200}, {"ScreenFrame_Lock", 400},
                         {"ScreenFrame_Unlock", 100},   {"ScreenFrame_EnumModesCallback", 1000}, {"ScreenFrame_Flip", 400}};

void check_compare(Tally &t)
{
    g_seed = 0x568600;
    for (int k = 0; k < scaled(3000); k++) {
        DWORD a[2], b[2];
        const DWORD pick[4] = {0x280, 0x320, 0x1e0, 0x258};
        for (int i = 0; i < 4; i++)
            (i < 2 ? a : b)[i & 1] = random() % 3 ? pick[random() % 4] : random();
        const int o = original<int(__cdecl *)(const DWORD *, const DWORD *)>(0x568600)(a, b);
        const int r = at<int(__cdecl *)(const DWORD *, const DWORD *)>(0x568600)(a, b);
        char d[96];
        wsprintfA(d, "[CompareModes %x %x / %x %x: %d %d]", a[0], a[1], b[0], b[1], o, r);
        t.check(o == r, "", d);
    }
}

}  // namespace

ZAX_CHECKS(display_ddraw)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        const DWORD dwords[] = {G_REPORTER_ADDRESS, DDRAW, BACK, THIRD, CLIPPER, PALETTE, PRIMARY, LOCKED, WINDOW, 0x633388};
        for (DWORD a : dwords)
            g_globals.add(a, 4);
        g_globals.add(OWN_BACK, 1);
        g_globals.add(FULL_SCREEN, 1);
    }
    SavedBytes desc;
    save_bytes(desc, LOCK_DESC, 0x6c);
    Tally t{"display_ddraw: DirectDraw mode set, release, lock, unlock, flip, the mode callback and comparator"};
    const Check c{&g_hooks, &g_globals};
    stubs_on(STUBS, NSTUBS);
    for (int i = 0; i < (int)(sizeof TESTS / sizeof TESTS[0]); i++) {
        g_test = i;
        const int count = scaled(TESTS[i].count);
        for (int k = 0; k < count; k++) {
            run_case(t, c, TESTS[i].name, 51000u + (unsigned)(i * 10007 + k), &build, &call);
        }
    }
    stubs_off();
    restore_bytes(desc);
    check_compare(t);
    t.report();
}

}  // namespace zax
