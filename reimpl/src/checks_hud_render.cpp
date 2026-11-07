// Checks of the HUD's item renderers (hud_render.cpp) against the original, in the game thread.
//  - Drawing (slot 28 of the seven renderer classes, with the frame check harness checks_frame_common.h): the
//    real item definitions of the catalogue (loaded records only) whose HUD renderer is of a class under
//    test, each drawn into fake frames (5-6-5 and 8 bits, sizes 160-300 x 100-200, origins, x offsets) with
//    render data built in the harness's object memory (opacity, the definition's catalogue index or another
//    loaded one, the class's values: compass shown and frame, weapon rounds, fractions, battery, energy
//    charge and capacity incl. 0 and NaN, up to three shields). The real fonts, HUD sequences, inventory
//    display options and the HUD object draw. Stand-ins: the clocks timeGetTime (0x4d24b0) and
//    GetTickCount (0x4d2460, 0x4d2650 Platform_Ticks24) give the case's values; the error reporter 0x6c02cc
//    is a counting fake; the Sphere of Zin's statics (0x713810, 0x7137f8, guard 0x7137d9) and the Transparent
//    Mask's opacity are set from the case before each side and put back after. Compared: pixels, the frame
//    call log, the render data (unchanged), the statics, the reporter calls. Everything a draw loads on first
//    use (definitions, sequences, fonts) is loaded once by a run of the original before the cases.
//  - The placement 0x4298d0 on fake type definitions and fake frames (all anchors, the HUD bar flag), the
//    inventory display options 0x429de0, the item icon 0x5acab0 and the energy meter 0x486d10 directly
//    (fake frames, both meter shapes, values and maxima incl. 0, negative and NaN).
//  - Update (slot 25) and Read (slot 26) on a fake world (machinery: checks_inventory_common.h): the render
//    data slot empty, of the class or of another class (made by the original Create); the item reference,
//    the definitions, the inventory, the battery and energy source fakes with real vtables where the code
//    asks IsKindOf; stand-ins record ItemRef_Definition, the inventory functions, the rounds, the battery,
//    MaxCharge, the energy source, HasAmmunition, IsContinuousFire, DefinitionList_LoadRecord, the
//    packet; the HUD sequence and item lists' records are fakes during a case (compass: frame counts from
//    0 to 40), the compass target and "known" flag are set from the case. Compared: al, the event log, the
//    render data's contents (a shield's elements by contents).
#include "checks_hud_common.h"
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_hud_render(Hook *out);

namespace {

using framec::Rect;

// Room behind a frame in the arena: CFrame's line (slot 51) draws a horizontal line lying exactly on the frame's
// height one row past the buffer (its clip includes the bottom edge); the game's meters lie inside the screen
void overflow_room(const framec::FrameSpec &s)
{
    framec::FrameSpec r;
    r.width = s.pitch > s.width ? s.pitch : s.width;
    r.height = 2;
    framec::make_frame(r);
}
DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }
int rnd_range(int lo, int hi) { return lo + (int)rnd((DWORD)(hi - lo + 1)); }
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
DWORD some_float_bits()
{
    const DWORD r = random() >> 8;
    switch (r % 12) {
    case 0: return 0;
    case 1: return 0x80000000u;
    case 2: return 0x7fc00000u | (r >> 8 & 0xff);
    case 3: return fbits(-(float)(int)(r >> 12 & 0xff) / 8.0f);
    case 4: return fbits(1.0f);
    default: return fbits((float)(int)(r >> 12 & 0x3ff) / 16.0f);
    }
}
template <typename T> T &at_off(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &d32(void *p, DWORD off) { return at_off<DWORD>(p, off); }
BYTE *bptr(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
bool kind_of(void *o, DWORD getter) { return o && at<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, at<CClassInfo *(__cdecl *)()>(getter)()); }

// ---- The renderer classes --------------------------------------------------------------------------------

enum Kind { COMPASS, FLAG, WEAPON, SHIELD, ENERGY, CHARACTER, SPHERE, KINDS };
struct KindInfo {
    const char *name;
    DWORD vtable, draw, update, read, data_vtable, data_size;
};
const KindInfo KINDINFO[KINDS] = {
    {"CCompassRenderer", 0x5edf00, 0x423c50, 0x423990, 0x423b70, 0x5ede28, 0x1c},
    {"CFlagInventoryHUDRenderer", 0x5ef608, 0x469fa0, 0x469e90, 0x469f20, 0x5ede94, 0x14},
    {"CWeaponInventoryHUDRenderer", 0x5f1ad0, 0x485cc0, 0x4859b0, 0x485be0, 0x5f1a64, 0x28},
    {"CShieldInventoryHUDRenderer", 0x5ffd9c, 0x56f280, 0x56ef20, 0x56f160, 0x5ffcb8, 0x28},
    {"CEnergyInventoryHUDRenderer", 0x601080, 0x581d20, 0x581a90, 0x581c40, 0x5ffd30, 0x20},
    {"CCharacterInventoryHUDRenderer", 0x6010fc, 0x582270, 0x581a90, 0x581c40, 0x5ffd30, 0x20},
    {"CSphereOfZinEnergyInventoryHUDRenderer", 0x601240, 0x583860, 0x581a90, 0x581c40, 0x5ffd30, 0x20},
};

struct Entry {
    BYTE *def, *renderer;
    DWORD index;
    int kind;
};
constexpr int MAX_ENTRIES = 160;
Entry g_entries[MAX_ENTRIES];
int g_nentries, g_found[KINDS];
DWORD g_loaded[512];
int g_nloaded;

int kind_of_renderer(const BYTE *r)
{
    if (!r)
        return -1;
    for (int k = 0; k < KINDS; k++)
        if (d32(const_cast<BYTE *>(r), 0) == KINDINFO[k].vtable)
            return k;
    return -1;
}

// The definition's HUD icon (slot 31 in the HUD sequence list, frame 0) exists: the drawing blits it without a
// null test, as the HUD only shows such items (the list's record loaded as the game loads it)
bool has_hud_icon(BYTE *def)
{
    BYTE *seq = at<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(0x6c06a0), vcall<DWORD>(def, 31));
    return seq && vcall<void *>(seq, 23, 0);
}

// A HUD sequence has frames (Sequence_SyncFrame divides by their count; the game shows no item with an empty
// one); its record loaded as the drawing would
bool hud_sequence_ok(DWORD index)
{
    BYTE *seq = at<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(0x6c06a0), index);
    return seq && vcall<DWORD>(seq, 25) > 0;
}
bool hud_sequences_ok(int k, BYTE *r, BYTE *def)
{
    if (k == SHIELD && !kind_of(def, 0x486220))   // the shield rows read their energy definition without a test
        return false;
    if (k == FLAG || k == SPHERE || k == COMPASS)
        if (!hud_sequence_ok(d32(r, 8)))
            return false;
    return k != SPHERE || !d32(r, 0x20) || hud_sequence_ok(d32(r, 0x20));
}

// The catalogue's loaded game item definitions and their HUD renderers (slot 32, else the type
// definition's +0x24)
// Renderers of the classes the catalogue does not use (compass, energy): objects with their real vtables on real
// definitions (the compass on a weapon with its HUD icon as "HUD Overlay", energy on a shield's energy
// definition). The character renderer needs a CCharacterInventoryItemDefinition (its placement reads the type
// definition without a test), which these levels' catalogue does not have: not drawn here.
BYTE g_synthetic[2][0x40];
void add_synthetic()
{
    int weapon = -1, shield = -1;
    for (int i = 0; i < g_nentries; i++) {
        if (g_entries[i].kind == WEAPON && weapon < 0)
            weapon = i;
        if (g_entries[i].kind == SHIELD && shield < 0)
            shield = i;
    }
    const int kinds[2] = {COMPASS, ENERGY};
    for (int k = 0; k < 2 && g_nentries < MAX_ENTRIES; k++) {
        const int from = kinds[k] == COMPASS ? weapon : shield;
        if (from < 0 || g_found[kinds[k]])
            continue;
        BYTE *r = g_synthetic[k];
        memset(r, 0, sizeof g_synthetic[k]);
        d32(r, 0) = KINDINFO[kinds[k]].vtable;
        if (kinds[k] == COMPASS)
            d32(r, 8) = vcall<DWORD>(g_entries[from].def, 31);
        g_entries[g_nentries++] = {g_entries[from].def, r, g_entries[from].index, kinds[k]};
    }
}

void collect_entries()
{
    g_nentries = g_nloaded = 0;
    for (int &f : g_found)
        f = 0;
    // the definitions loaded first as the game loads them (the original ItemRef_Definition 0x4dd480 on an
    // item of each index), as checks_inv_actions.cpp does
    for (DWORD d = 0; *at<BYTE **>(0x6c0c38) && d < *at<DWORD *>(0x6c0c3c) && d < 4096; d++) {
        BYTE item[0x18] = {};
        d32(item, 8) = d;
        reinterpret_cast<BYTE *(THISCALL *)(void *)>(original_address(0x4dd480))(item);
    }
    BYTE *records = *at<BYTE **>(0x6c0c38);
    const DWORD count = records ? *at<DWORD *>(0x6c0c3c) : 0;
    for (DWORD i = 0; i < count && i < 4096; i++) {
        BYTE *rec = records + i * 0x80;
        BYTE *obj = bptr(d32(rec, 0x78));
        if (rec[0x7f] || !obj)
            continue;
        if (g_nloaded < 512)
            g_loaded[g_nloaded++] = i;
        if (!kind_of(obj, 0x5b5f10))
            continue;
        // the placement reads the type definition without a null test (the HUD only shows items that have one)
        BYTE *td = at<BYTE *(THISCALL *)(void *)>(0x5b7af0)(obj);
        if (!td)
            continue;
        BYTE *r = vcall<BYTE *>(obj, 32);
        if (!r)
            r = bptr(d32(td, 0x24));
        const int k = kind_of_renderer(r);
        if (k < 0 || !has_hud_icon(obj) || !hud_sequences_ok(k, r, obj))
            continue;
        if (k == WEAPON && r[8]) {   // "Show Ammo Icon": the ammunition's icon too
            BYTE *ammo = at<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(0x6c0c08), d32(obj, 0x2c));
            if (!ammo || !kind_of(ammo, 0x5b5f10) || !has_hud_icon(ammo))
                continue;
        }
        g_found[k]++;
        if (g_nentries < MAX_ENTRIES)
            g_entries[g_nentries++] = {obj, r, i, k};
    }
}

// ---- Stand-ins of the drawing cases --------------------------------------------------------------------------

DWORD g_time, g_ticks, g_tick_calls, g_reports;
DWORD __stdcall s_time() { return g_time; }
DWORD __stdcall s_ticks() { return g_ticks + 37u * g_tick_calls++; }
DWORD __cdecl s_ticks24() { return (g_ticks + 11u * g_tick_calls++) & 0xffffff; }
DWORD THISCALL s_report(void *, DWORD) { g_reports++; return 0; }
DWORD g_reporter_vt[64];
BYTE g_reporter[16];
framec::ModuleHooks g_hooks{"hud_render", &hooks_hud_render, {}, {}, {}, 0, true};
CodePatch g_time_patches[3];
DWORD g_saved_reporter;

void stands_on()
{
    for (DWORD &v : g_reporter_vt)
        v = pv((void *)&s_report);
    d32(g_reporter, 0) = pv(g_reporter_vt);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c02cc) = pv(g_reporter);
    patch_jump(g_time_patches[0], 0x4d24b0, (void *)&s_time);
    patch_jump(g_time_patches[1], 0x4d2460, (void *)&s_ticks);
    patch_jump(g_time_patches[2], 0x4d2650, (void *)&s_ticks24);
}
void stands_off()
{
    for (int i = 2; i >= 0; i--)
        unpatch(g_time_patches[i]);
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
}

void set_mask_opacity(DWORD bits) { at<void(THISCALL *)(void *, DWORD)>(0x433be0)(at<void *>(0x6bcc68), bits); }

// The statics a draw changes, set from the case before each side
struct Statics {
    DWORD phase, last;
    BYTE guard;
};
Statics g_real_statics, g_case_statics;
void statics_save(Statics &s)
{
    s.phase = *at<DWORD *>(0x713810);
    s.last = *at<DWORD *>(0x7137f8);
    s.guard = *at<BYTE *>(0x7137d9);
}
void statics_set(const Statics &s)
{
    *at<DWORD *>(0x713810) = s.phase;
    *at<DWORD *>(0x7137f8) = s.last;
    *at<BYTE *>(0x7137d9) = s.guard;
}

// ---- Drawing cases ---------------------------------------------------------------------------------------

// Another catalogue index whose definition has a renderer of the same class (the drawing code reads the
// definition without null tests, as the game only ever shows such items)
DWORD same_kind_index(const Entry &e)
{
    for (int tries = 0; tries < 8; tries++) {
        const Entry &o = g_entries[rnd((DWORD)g_nentries)];
        if (o.kind == e.kind)
            return o.index;
    }
    return e.index;
}

// The render data of a case in the object memory (both sides), returns its offset
DWORD make_data(const Entry &e, int frames)
{
    const KindInfo &k = KINDINFO[e.kind];
    const DWORD off = framec::alloc_object(0x40);
    const DWORD vals[16] = {k.data_vtable, 0, rnd(4) ? fbits((float)rnd(101) / 100.0f) : some_float_bits(), rnd(6) ? e.index : same_kind_index(e),
                            rnd(4), random(), random(), random(), random(), random(), random(), random(), random(), random(), random(), random()};
    DWORD v[16];
    memcpy(v, vals, sizeof v);
    switch (e.kind) {
    case COMPASS:
        v[5] = rnd(5) ? 1 : rnd(3) ? 0 : 0x0101;   // +0x14 shown, +0x15, +0x16
        v[6] = frames > 0 ? rnd((DWORD)frames) : 0;
        break;
    case WEAPON:
        v[5] = rnd(4) ? rnd(300) : random();
        v[6] = rnd(4) ? fbits((float)rnd(4000) / 16.0f) : some_float_bits();
        v[7] = some_float_bits();
        v[8] = some_float_bits();
        v[9] = rnd(8);
        break;
    case ENERGY:
    case CHARACTER:
    case SPHERE:
        v[5] = rnd(4) ? fbits((float)(1 + rnd(400))) : some_float_bits();
        v[6] = rnd(4) ? fbits((float)rnd(400)) : some_float_bits();
        v[7] = rnd(4);
        break;
    case SHIELD: {
        const DWORD n = rnd(4);
        v[5] = n;
        v[6] = 0x5ffd24;
        const DWORD arr = framec::alloc_object(16);
        DWORD elems[3];
        for (DWORD i = 0; i < n; i++) {
            elems[i] = framec::alloc_object(0x20);
            const DWORD ev[8] = {0x5ffd30, 0, fbits(1.0f), rnd(3) ? e.index : same_kind_index(e), 0, fbits((float)(1 + rnd(300))),
                                 rnd(4) ? fbits((float)rnd(300)) : some_float_bits(), 0};
            for (int side = 0; side < 2; side++)
                memcpy(framec::object<BYTE>(side, elems[i]), ev, sizeof ev);
        }
        for (int side = 0; side < 2; side++) {
            BYTE *a = framec::object<BYTE>(side, arr);
            for (DWORD i = 0; i < n; i++)
                d32(a, i * 4) = pv(framec::object<BYTE>(side, elems[i]));
        }
        v[7] = 0;   // filled per side below
        v[8] = n;
        v[9] = n;
        for (int side = 0; side < 2; side++) {
            v[7] = pv(framec::object<BYTE>(side, arr));
            memcpy(framec::object<BYTE>(side, off), v, sizeof v);
        }
        return off;
    }
    default: break;
    }
    for (int side = 0; side < 2; side++)
        memcpy(framec::object<BYTE>(side, off), v, sizeof v);
    return off;
}

int compass_frames(const Entry &e)
{
    if (e.kind != COMPASS)
        return 0;
    BYTE *records = *at<BYTE **>(0x6c06d0);
    const DWORD i = d32(e.renderer, 8);
    if (!records || i >= *at<DWORD *>(0x6c06d4) || records[i * 0x80 + 0x7f] || !d32(records + i * 0x80, 0x78))
        return 0;
    return (int)vcall<DWORD>(bptr(d32(records + i * 0x80, 0x78)), 25);
}

void draw_case(Tally &t, const Entry &e, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    framec::FrameSpec s;
    s.format = framec::RGB565;   // 8-bit drawing would look up a palette (slot 58) on the fake frame
    s.width = rnd_range(160, 300);
    s.height = rnd_range(100, 200);
    s.pitch = rnd(3) ? 0 : s.width + rnd_range(1, 9);
    // origin 0: CFrame's line (slot 51) subtracts the origin before it clips to the rectangle with the origin, so a
    // frame with another origin gets lines outside its buffer (the game's targets have origin 0)
    const int fi = framec::make_frame(s);
    overflow_room(s);
    if (fi < 0)
        return;
    const DWORD data = make_data(e, compass_frames(e));
    const int dx = rnd(3) ? 0 : rnd_range(-60, 60);
    g_time = random();
    g_ticks = random();
    g_case_statics = {rnd(3) ? fbits((float)rnd(800) / 100.0f) : some_float_bits(), g_ticks - rnd(400), (BYTE)rnd(2)};
    const DWORD opacity = rnd(4) ? fbits(1.0f) : fbits((float)rnd(101) / 100.0f);
    statics_save(g_real_statics);
    stands_on();
    char what[96];
    wsprintfA(what, "%s draw, index %u, seed %u", KINDINFO[e.kind].name, e.index, seed);
    hudcheck::run_sides(t, g_hooks, what, [&](int side) {
        statics_set(g_case_statics);
        set_mask_opacity(opacity);
        g_tick_calls = 0;
        g_reports = 0;
        at<void(THISCALL *)(void *, void *, void *, int)>(KINDINFO[e.kind].draw)(e.renderer, framec::object<BYTE>(side, data), framec::frame(side, fi), dx);
        Statics after;
        statics_save(after);
        return fnv(fnv(2166136261u, &after, sizeof after), &g_reports, 4) ^ *at<DWORD *>(0x6bcc68 + 0x20);
    });
    stands_off();
    statics_set(g_real_statics);
    set_mask_opacity(fbits(1.0f));
}

// A run of the original before the cases: what the draws load on first use
void warm_up(const Entry &e)
{
    g_seed = 0x77u + e.index;
    framec::begin_case(1);
    framec::FrameSpec s;
    s.width = 200;
    s.height = 150;
    const int fi = framec::make_frame(s);
    overflow_room(s);
    const DWORD data = make_data(e, compass_frames(e));
    statics_save(g_real_statics);
    stands_on();
    const DWORD f = KINDINFO[e.kind].draw;
    reinterpret_cast<void(THISCALL *)(void *, void *, void *, int)>(original_address(f))(e.renderer, framec::object<BYTE>(0, data), framec::frame(0, fi), 0);
    stands_off();
    statics_set(g_real_statics);
    set_mask_opacity(fbits(1.0f));
}

// ---- The helpers directly -------------------------------------------------------------------------------

void place_case(Tally &t, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    framec::FrameSpec s;
    s.width = rnd_range(0, 300);
    s.height = rnd_range(0, 200);
    const int fi = framec::make_frame(s);
    overflow_room(s);
    const DWORD td = framec::alloc_object(0x20), self = framec::alloc_object(0x110);
    const DWORD v[7] = {0, 0, (DWORD)rnd_range(-50, 300), (DWORD)rnd_range(-50, 200), rnd(4), rnd(4), random()};
    const DWORD bar = random() % 3 ? rnd(60) : random();
    for (int side = 0; side < 2; side++) {
        memcpy(framec::object<BYTE>(side, td), v, sizeof v);
        d32(framec::object<BYTE>(side, self), 0x108) = bar;
    }
    hudcheck::run_sides(t, g_hooks, "InventoryHUD_Place", [&](int side) {
        int xy[2] = {0x5e5e5e5e, 0x5e5e5e5e};
        const DWORD r = at<DWORD(THISCALL *)(void *, void *, void *, int *, int *)>(0x4298d0)(framec::object<BYTE>(side, self), framec::object<BYTE>(side, td),
                                                                                          framec::frame(side, fi), &xy[0], &xy[1]);
        return fnv(fnv(2166136261u, xy, sizeof xy), &r, (r == pv(&xy[1])) ? 0 : 4);
    });
}

void meter_case(Tally &t, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    framec::FrameSpec s;
    s.format = framec::RGB565;   // 8-bit drawing would look up a palette (slot 58) on the fake frame
    s.width = rnd_range(100, 160);
    s.height = rnd_range(80, 120);
    // origin 0: CFrame's line (slot 51) subtracts the origin before it clips to the rectangle with the origin, so a
    // frame with another origin gets lines outside its buffer (the game's targets have origin 0)
    const int fi = framec::make_frame(s);
    overflow_room(s);
    const DWORD meter = framec::alloc_object(0x50), rect = framec::alloc_object(16);
    BYTE m[0x50];
    for (DWORD i = 0; i < sizeof m; i += 4)
        d32(m, i) = random();
    for (DWORD c = 8; c <= 0x38; c += 0x10) {   // four FieldColors: index valid, 16-bit pixel, alpha
        m[c + 3] = (BYTE)(rnd(3) ? 0xff : rnd(256));
        d32(m, c + 0xc) = 1 | (rnd(2) << 1);
    }
    const bool tall = rnd(2);
    // inside the frame with its one-pixel outline, as the game's meters are (CFrame's line clips wrongly at the
    // edges: section 9, "The HUD")
    const int w = tall ? rnd_range(1, 12) : rnd_range(0, 90), h = tall ? rnd_range(w + 1, w + 60) : rnd_range(1, w ? w : 1);
    const int l = rnd_range(1, s.width - w - 2), tp = rnd_range(1, s.height - h - 2);
    const int r[4] = {l, tp, l + w, tp + h};
    for (int side = 0; side < 2; side++) {
        memcpy(framec::object<BYTE>(side, meter), m, sizeof m);
        memcpy(framec::object<BYTE>(side, rect), r, sizeof r);
    }
    // a charge between 0 and the capacity, as the game's items have: the meter's bars take their width from it
    // without clipping it to the meter
    const float maxf = (float)(rnd(200) + rnd(2));
    const DWORD max = rnd(8) ? fbits(maxf) : fbits(0.0f);
    const DWORD value = fbits(maxf * (float)rnd(101) / 100.0f);
    hudcheck::run_sides(t, g_hooks, "EnergyMeter_Draw", [&](int side) {
        at<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD)>(0x486d10)(framec::object<BYTE>(side, meter), framec::frame(side, fi),
                                                                                     framec::object<BYTE>(side, rect), value, max, random());
        return 0u;
    });
}

void icon_case(Tally &t, const Entry &e, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    framec::FrameSpec s;
    s.format = framec::RGB565;   // 8-bit drawing would look up a palette (slot 58) on the fake frame
    s.width = rnd_range(40, 120);
    s.height = rnd_range(40, 100);
    const int fi = framec::make_frame(s);
    overflow_room(s);
    const int x = rnd_range(-30, s.width), y = rnd_range(-30, s.height);
    const DWORD opacity = rnd(3) ? fbits(1.0f) : fbits((float)rnd(101) / 100.0f);
    hudcheck::run_sides(t, g_hooks, "Hud_DrawItemIcon", [&](int side) {
        at<void(__stdcall *)(void *, void *, int, int, DWORD)>(0x5acab0)(e.def, framec::frame(side, fi), x, y, opacity);
        return *at<DWORD *>(0x6bcc68 + 0x20);
    });
}

}  // namespace

// ---- Update and Read on a fake world (invcheck machinery) ---------------------------------------------------

namespace {

using namespace invcheck;

constexpr int NI = 5, ND = 4, NS = 3, NR = 6;
constexpr DWORD VT_ENERGY_ITEM = 0x5f07e0, VT_ENERGY_DEF = 0x5f1b4c, VT_ZAX_DEF = 0x604c3c, VT_OTHER = 0x5f9e98;

struct UWorld {
    BYTE part[0x40], inv[0x40], ref[0x40], battery[0x40], source[0x40];
    BYTE items[NI][0x40];
    BYTE defs[ND][0x40];
    BYTE seqs[NS][0x20];
    BYTE hud_records[NS][0x80];
    BYTE item_records[NR][0x80];
    DWORD order[NI];
    float pos[2];
    DWORD rounds, frames[NS], cap, first;
    DWORD ammo_bits, cont_bits, has_battery, has_source;
};
UWorld g_w;
DWORD g_part_vt[64], g_inv_vt[64], g_seq_vt[64], g_list_vt[64];
BYTE g_packet[16];
DWORD g_slot;

int fake(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p), *w = reinterpret_cast<const BYTE *>(&g_w);
    if (!p)
        return 0;
    if (p == g_packet)
        return 1;
    if (b >= w && b < w + sizeof g_w)
        return 0x10000 + (int)(b - w);
    return 0x99;
}

DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
float *THISCALL s_position(void *p, float *out)
{
    ev(0x901, (DWORD)fake(p));
    out[0] = g_w.pos[0];
    out[1] = g_w.pos[1];
    return out;
}
BYTE *THISCALL s_inv_item(void *inv, DWORD i)
{
    ev(0x902, (DWORD)fake(inv), i);
    return i < NI && g_w.order[i] != 0xff ? g_w.items[g_w.order[i]] : nullptr;
}
DWORD THISCALL s_frames(void *seq) { ev(0x903, (DWORD)fake(seq)); return g_w.frames[(fake(seq) - 0x10000 - (int)__builtin_offsetof(UWorld, seqs)) / 0x20 % NS]; }
DWORD THISCALL s_list_find(void *list, const char *name, int a)
{
    ev(0x904, list == at<void *>(0x6c0c08), fnv_text(2166136261u, name), (DWORD)a);
    return rnd(3) ? rnd(NR) : 0xffffffff;
}
BYTE *THISCALL s_definition(void *ref)
{
    ev(0x911, (DWORD)fake(ref));
    if (!ref)
        return nullptr;
    const DWORD k = d32(ref, 0x10);
    return k < ND ? g_w.defs[k] : nullptr;
}
BYTE *THISCALL s_part_inventory(void *part) { ev(0x912, (DWORD)fake(part)); return g_w.inv; }
DWORD THISCALL s_first(void *inv, DWORD cat) { ev(0x913, (DWORD)fake(inv), cat); return g_w.first; }
DWORD THISCALL s_next(void *inv, DWORD i, DWORD cat) { ev(0x914, (DWORD)fake(inv), i, cat); return (i + 1) % NI; }
DWORD __fastcall s_rounds(void *part, void *def) { ev(0x915, (DWORD)fake(part), (DWORD)fake(def)); return g_w.rounds; }
BYTE *THISCALL s_battery(void *inv) { ev(0x916, (DWORD)fake(inv)); return g_w.has_battery ? g_w.battery : nullptr; }
long double THISCALL s_max_charge(void *item)
{
    ev(0x917, (DWORD)fake(item));
    float f;
    memcpy(&f, &g_w.cap, 4);
    return fld(f);
}
BYTE *THISCALL s_source(void *def, void *inv) { ev(0x918, (DWORD)fake(def), (DWORD)fake(inv)); return g_w.has_source ? g_w.source : nullptr; }
DWORD THISCALL s_ammo(void *def) { ev(0x919, (DWORD)fake(def)); return (result(0x19) & 0xffffff00) | (g_w.ammo_bits >> (fake(def) & 7) & 1); }
DWORD THISCALL s_cont(void *def) { ev(0x91a, (DWORD)fake(def)); return (result(0x1a) & 0xffffff00) | (g_w.cont_bits >> (fake(def) & 7) & 1); }
DWORD THISCALL s_load_record(void *list, DWORD i)
{
    ev(0x91b, pv(list), i);
    if (list == at<void *>(0x6c06a0) && i < NS)
        d32(g_w.hud_records[i], 0x78) = pv(g_w.seqs[i]);
    if (list == at<void *>(0x6c0c08) && i < NR)
        d32(g_w.item_records[i], 0x78) = pv(g_w.defs[i % ND]);
    return result(0x1b);
}
// The packet (as checks_hud_data.cpp)
DWORD THISCALL s_write_byte(void *p, DWORD v) { ev(0x801, (DWORD)fake(p), v & 0xff); return result(1); }
DWORD THISCALL s_write_word(void *p, DWORD v) { ev(0x802, (DWORD)fake(p), v & 0xffff); return result(2); }
DWORD THISCALL s_write_dword(void *p, DWORD v) { ev(0x803, (DWORD)fake(p), v); return result(3); }
DWORD THISCALL s_write_token(void *p, DWORD v, DWORD list) { ev(0x804, (DWORD)fake(p), v & 0xffff, list); return result(4); }
DWORD THISCALL s_read_byte(void *p, BYTE *v) { *v = (BYTE)(random() >> 9); ev(0x811, (DWORD)fake(p), *v); return result(0x11); }
DWORD THISCALL s_read_word(void *p, WORD *v) { *v = (WORD)(random() >> 7); ev(0x812, (DWORD)fake(p), *v); return result(0x12); }
DWORD THISCALL s_read_dword(void *p, DWORD *v) { *v = random() % 3 ? some_float_bits() : random(); ev(0x813, (DWORD)fake(p), *v); return result(0x13); }
DWORD THISCALL s_read_token(void *p, DWORD *v, DWORD list) { *(WORD *)v = (WORD)rnd(NR + 2); ev(0x814, (DWORD)fake(p), *v, list); return result(0x14); }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x4dd480, (void *)&s_definition}, {0x4267e0, (void *)&s_part_inventory}, {0x425340, (void *)&s_first},
    {0x425470, (void *)&s_next},       {0x5aeec0, (void *)&s_rounds},         {0x5b0da0, (void *)&s_battery},
    {0x5afe50, (void *)&s_max_charge}, {0x5b7920, (void *)&s_source},         {0x4d77c0, (void *)&s_ammo},
    {0x5b7640, (void *)&s_cont},       {0x55e090, (void *)&s_load_record},    {0x520820, (void *)&s_write_byte},
    {0x5209c0, (void *)&s_write_word}, {0x520940, (void *)&s_write_dword},    {0x520da0, (void *)&s_write_token},
    {0x520490, (void *)&s_read_byte},  {0x520510, (void *)&s_read_word},      {0x520550, (void *)&s_read_dword},
    {0x520670, (void *)&s_read_token},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_patches[NSTANDS];

void init_vtables()
{
    DWORD *const vts[3] = {g_part_vt, g_inv_vt, g_seq_vt};
    for (DWORD *vt : vts)
        for (int i = 0; i < 64; i++)
            vt[i] = pv((void *)&f_bad);
    memcpy(g_part_vt, at<void *>(0x5f9e98), 21 * 4);   // CObject's slots (IsKindOf: another class)
    memcpy(g_inv_vt, at<void *>(0x5f9e98), 21 * 4);
    memcpy(g_seq_vt, at<void *>(0x5f9e98), 21 * 4);
    g_part_vt[43] = pv((void *)&s_position);
    g_inv_vt[26] = pv((void *)&s_inv_item);
    g_seq_vt[25] = pv((void *)&s_frames);
}

void setup_world(unsigned seed)
{
    g_seed = seed;
    UWorld &w = g_w;
    BYTE *all = reinterpret_cast<BYTE *>(&w);
    for (DWORD k = 0; k + 4 <= sizeof w; k += 4)
        *reinterpret_cast<DWORD *>(all + k) = random();
    d32(w.part, 0) = pv(g_part_vt);
    d32(w.inv, 0) = pv(g_inv_vt);
    for (int i = 0; i < NI; i++) {
        BYTE *it = w.items[i];
        d32(it, 0) = rnd(5) ? VT_ENERGY_ITEM : VT_OTHER;
        d32(it, 4) = 0;
        d32(it, 8) = rnd(NR);
        d32(it, 0x10) = rnd(5) ? rnd(ND) : 0xff;
        d32(it, 0x18) = some_float_bits();
        w.order[i] = rnd(6) ? rnd(NI) : 0xff;
    }
    memcpy(w.ref, w.items[rnd(NI)], 0x40);
    d32(w.ref, 0x18) = some_float_bits();
    for (int i = 0; i < ND; i++) {
        static const DWORD VTS[4] = {VT_ENERGY_DEF, VT_ZAX_DEF, VT_ENERGY_DEF, VT_OTHER};
        d32(w.defs[i], 0) = VTS[(i + rnd(2)) % 4];
        d32(w.defs[i], 4) = 0;
    }
    d32(w.battery, 0x18) = some_float_bits();
    d32(w.source, 0x18) = some_float_bits();
    for (int i = 0; i < NS; i++) {
        d32(w.seqs[i], 0) = pv(g_seq_vt);
        d32(w.seqs[i], 4) = 0;
        w.frames[i] = rnd(4) ? rnd(41) : random();
        d32(w.hud_records[i], 0x78) = rnd(4) ? pv(w.seqs[i]) : 0;
        w.hud_records[i][0x7f] = 0;   // a loading record gives no sequence, which the compass reads without a test
    }
    for (int i = 0; i < NR; i++) {
        d32(w.item_records[i], 0x78) = rnd(4) ? pv(w.defs[i % ND]) : 0;
        w.item_records[i][0x7f] = 0;
    }
    w.pos[0] = (float)rnd_range(-2000, 2000);
    w.pos[1] = (float)rnd_range(-2000, 2000);
    w.rounds = rnd(4) ? rnd(300) : random();
    w.cap = some_float_bits();
    w.first = rnd(NI);
    w.ammo_bits = random();
    w.cont_bits = random();
    w.has_battery = rnd(4) != 0;
    w.has_source = rnd(4) != 0;
}

// A render data object of a class made by the original Create (pool), with the case's values
BYTE *make_initial(int which, unsigned)
{
    static const DWORD CREATES[] = {0x5ac5a0, 0x423160, 0x484d90, 0x56e690, 0x5813d0};
    if (which < 0)
        return nullptr;
    BYTE *o = reinterpret_cast<BYTE *(__cdecl *)()>(original_address(CREATES[which]))();
    if (!o)
        return nullptr;
    d32(o, 8) = some_float_bits();
    d32(o, 0xc) = rnd(NR);
    d32(o, 0x10) = rnd(4);
    if (which == 1) {
        o[0x14] = (BYTE)rnd(2);
        o[0x15] = (BYTE)rnd(2);
        o[0x16] = (BYTE)rnd(2);
        d32(o, 0x18) = rnd(40);
    } else if (which == 2) {
        for (DWORD k = 0x14; k < 0x24; k += 4)
            d32(o, k) = rnd(3) ? rnd(300) : some_float_bits();
        d32(o, 0x24) = rnd(8);
    } else if (which == 3) {
        d32(o, 0x14) = rnd(4);
        reinterpret_cast<void(THISCALL *)(void *, DWORD)>(original_address(0x56ec50))(o, rnd(4));
    } else if (which == 4) {
        d32(o, 0x14) = some_float_bits();
        d32(o, 0x18) = some_float_bits();
        d32(o, 0x1c) = rnd(4);
    }
    return o;
}
DWORD data_size(BYTE *o)
{
    switch (d32(o, 0)) {
    case 0x5ede28: return 0x1c;
    case 0x5f1a64: case 0x5ffcb8: return 0x28;
    case 0x5ffd30: return 0x20;
    default: return 0x14;
    }
}
unsigned data_state(BYTE *o)
{
    if (!o)
        return 0;
    BYTE copy[0x40];
    const DWORD n = data_size(o);
    memcpy(copy, o, n);
    unsigned h = 2166136261u;
    if (d32(o, 0) == 0x5ffcb8) {
        d32(copy, 0x1c) = 0;
        d32(copy, 0x24) = 0;
        for (DWORD i = 0; i < d32(o, 0x20) && i < 32; i++)
            h = fnv(h, bptr(d32(bptr(d32(o, 0x1c)), i * 4)), 0x20);
    }
    return fnv(h, copy, n);
}
void free_data(BYTE *o)
{
    if (!o)
        return;
    if (d32(o, 0) == 0x5ffcb8)
        for (DWORD i = 0; i < d32(o, 0x20); i++)
            if (BYTE *e = bptr(d32(bptr(d32(o, 0x1c)), i * 4)))
                vcall<DWORD>(e, 1, 1);
    if (d32(o, 0) == 0x5ffcb8)
        d32(o, 0x20) = 0;
    vcall<DWORD>(o, 1, 1);
}

unsigned g_state[2];

// One Update or Read case of a renderer kind
void update_case(Tally &t, int kind, bool read, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup_world(seed);
        const int initial = rnd(4) == 0 ? -1 : (int)rnd(5);
        BYTE *renderer = static_cast<BYTE *>(pool_alloc(0x40));
        memset(renderer, 0, 0x40);
        d32(renderer, 0) = KINDINFO[kind].vtable;
        d32(renderer, 8) = rnd(NS + 1);
        g_slot = pv(make_initial(initial, seed));
        const BYTE known = (BYTE)rnd(2);
        const float target[2] = {(float)rnd_range(-2000, 2000), (float)rnd_range(-2000, 2000)};
        const BYTE saved_known = *at<BYTE *>(0x62e9d1);
        DWORD saved_target[2];
        memcpy(saved_target, at<void *>(0x62e9d8), 8);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
        BYTE *const hud_records = *at<BYTE **>(0x6c06d0);
        const DWORD hud_count = *at<DWORD *>(0x6c06d4);
        BYTE *const item_records = *at<BYTE **>(0x6c0c38);
        const DWORD item_count = *at<DWORD *>(0x6c0c3c);
        const DWORD list_vt = *at<DWORD *>(0x6c0c08);
        memcpy(g_list_vt, at<void *>(list_vt), sizeof g_list_vt);
        g_list_vt[2] = pv((void *)&s_list_find);
        *at<DWORD *>(0x6c0c08) = pv(g_list_vt);
        *at<BYTE **>(0x6c06d0) = g_w.hud_records[0];
        *at<DWORD *>(0x6c06d4) = NS;
        *at<BYTE **>(0x6c0c38) = g_w.item_records[0];
        *at<DWORD *>(0x6c0c3c) = NR;
        *at<BYTE *>(0x62e9d1) = known;
        memcpy(at<void *>(0x62e9d8), target, 8);
        g_seed = seed * 7u + 3u;
        if (read)
            r[side] = at<DWORD(THISCALL *)(void *, void *, DWORD *)>(KINDINFO[kind].read)(renderer, g_packet, &g_slot) & 0xff;
        else
            r[side] = at<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD *)>(KINDINFO[kind].update)(renderer, g_w.part, g_w.ref, some_float_bits(),
                                                                                                         &g_slot) & 0xff;
        *at<BYTE *>(0x62e9d1) = saved_known;
        memcpy(at<void *>(0x62e9d8), saved_target, 8);
        *at<DWORD *>(0x6c0c08) = list_vt;
        *at<DWORD *>(0x6c0c3c) = item_count;
        *at<BYTE **>(0x6c0c38) = item_records;
        *at<DWORD *>(0x6c06d4) = hud_count;
        *at<BYTE **>(0x6c06d0) = hud_records;
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_patches[i]);
        side_end(side);
        g_state[side] = data_state(bptr(g_slot));
        free_data(bptr(g_slot));
        g_slot = 0;
        pool_free(renderer, 0x40);
    }
    const bool same = r[0] == r[1] && same_log() && g_state[0] == g_state[1];
    char d[200];
    wsprintfA(d, "[seed %u: al %x / %x, log %d / %d, data %x / %x]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4, g_state[0], g_state[1]);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, read ? "Read" : "Update", d);
}

// ---- The auto-map (the game's own frames saved and put back) ---------------------------------------------------

struct SavedPixels {
    BYTE *pixels;
    DWORD bytes;
    BYTE *copy;
};
DWORD frame_bytes(const BYTE *f)
{
    if (!f || !d32(const_cast<BYTE *>(f), 0xc) || !d32(const_cast<BYTE *>(f), 0x18))
        return 0;
    const DWORD bpp = (d32(const_cast<BYTE *>(f), 0x3c) & 0x1e0) == 0x20 ? 1 : 2;
    const DWORD pitch = bptr(d32(const_cast<BYTE *>(f), 0x18)) ? reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(d32(const_cast<BYTE *>(f), 0x18)))[1] : 0;
    return pitch * *reinterpret_cast<const WORD *>(f + 0x14) * bpp;
}
void save_pixels(SavedPixels &sp, BYTE *f)
{
    sp.bytes = frame_bytes(f);
    sp.pixels = sp.bytes ? bptr(d32(f, 0xc)) : nullptr;
    sp.copy = sp.bytes && sp.bytes <= 0x200000 ? static_cast<BYTE *>(VirtualAlloc(nullptr, sp.bytes, 0x3000, 4)) : nullptr;
    if (sp.copy)
        memcpy(sp.copy, sp.pixels, sp.bytes);
}
void restore_pixels(const SavedPixels &sp)
{
    if (sp.copy)
        memcpy(sp.pixels, sp.copy, sp.bytes);
}

void minimap_checks(Tally &t)
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return;
    BYTE *layer = at<BYTE *(THISCALL *)(void *)>(0x4f1050)(world);
    BYTE *fog = layer ? at<BYTE *(THISCALL *)(void *)>(0x4e7170)(layer + 0x3c) : nullptr;
    SavedPixels fog_saved{}, map_saved{};
    save_pixels(fog_saved, fog);
    // UpdateFog: the state (mode, timer, last position, layer) from the case; compared: the state and the fog
    for (unsigned k = 0; k < (unsigned)scaled(300); k++) {
        g_seed = 0x508000u + k;
        DWORD init[5] = {rnd(4), some_float_bits(), fbits((float)rnd_range(-500, 3000)), fbits((float)rnd_range(-500, 3000)), rnd(3) ? pv(layer) : 0};
        const DWORD dt = rnd(3) ? fbits((float)rnd(100) / 100.0f) : some_float_bits();
        DWORD st[2][5];
        unsigned fh[2];
        for (int side = 0; side < 2; side++) {
            restore_pixels(fog_saved);
            memcpy(st[side], init, sizeof init);
            if (side == 0)
                g_hooks.out();
            at<void(THISCALL *)(void *, DWORD)>(0x508000)(st[side], dt);
            if (side == 0)
                g_hooks.in();
            fh[side] = fog_saved.copy ? fnv(2166136261u, fog_saved.pixels, fog_saved.bytes) : 0;
        }
        char d[120];
        wsprintfA(d, "[UpdateFog case %u: mode %u/%u, fog %x/%x]", k, st[0][0], st[1][0], fh[0], fh[1]);
        t.check(!memcmp(st[0], st[1], sizeof st[0]) && fh[0] == fh[1], "", d);
    }
    restore_pixels(fog_saved);
    // Draw: a run of the original first (it builds the static frame), then fake frames; the static frame's
    // pixels are put back after each side
    {
        framec::begin_case(1);
        framec::FrameSpec s;
        s.width = 220;
        s.height = 160;
        const int fi = framec::make_frame(s);
    overflow_room(s);
        DWORD state[5] = {2, 0, 0, 0, 0};
        reinterpret_cast<void(THISCALL *)(void *, void *, int, int)>(original_address(0x508180))(state, framec::frame(0, fi), 0, 0);
    }
    save_pixels(map_saved, at<BYTE *>(0x6c2d38));
    for (unsigned k = 0; k < (unsigned)scaled(200); k++) {
        g_seed = 0x508180u + k;
        framec::begin_case(0x508180u + k);
        framec::FrameSpec s;
        s.width = rnd_range(160, 300);
        s.height = rnd_range(100, 200);
        const int fi = framec::make_frame(s);
    overflow_room(s);
        DWORD state[5] = {rnd(4) ? 2 + rnd(2) : 0, 0, 0, 0, 0};
        const int x = rnd_range(-60, s.width), y = rnd_range(-60, s.height);
        hudcheck::run_sides(t, g_hooks, "HudMiniMap_Draw", [&](int side) {
            restore_pixels(map_saved);
            at<void(THISCALL *)(void *, void *, int, int)>(0x508180)(state, framec::frame(side, fi), x, y);
            return map_saved.copy ? fnv(2166136261u, map_saved.pixels, map_saved.bytes) : 0u;
        });
    }
    restore_pixels(map_saved);
}

}  // namespace

ZAX_CHECKS(hud_render)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = &hooks_hud_render;
    collect_entries();
    add_synthetic();
    log("  hud render: %d loaded item definitions, renderers: compass %d, flag %d, weapon %d, shield %d, energy %d, character %d, "
        "Sphere of Zin %d",
        g_nloaded, g_found[COMPASS], g_found[FLAG], g_found[WEAPON], g_found[SHIELD], g_found[ENERGY], g_found[CHARACTER], g_found[SPHERE]);
    for (int i = 0; i < g_nentries; i++)
        warm_up(g_entries[i]);

    Tally helpers{"hud render: placement, display options, energy meter, item icon"};
    for (unsigned k = 0; k < (unsigned)scaled(600); k++)
        place_case(helpers, 0x4298d0u + k);
    {
        const DWORD o = reinterpret_cast<DWORD(__cdecl *)()>(original_address(0x429de0))(), x = at<DWORD(__cdecl *)()>(0x429de0)();
        char d[80];
        wsprintfA(d, "[InventoryDisplayOptions %x / %x]", o, x);
        helpers.check(o == x, "", d);
    }
    for (unsigned k = 0; k < (unsigned)scaled(1500); k++)
        meter_case(helpers, 0x486d10u + k);
    for (int i = 0; i < g_nentries; i++)
        for (unsigned k = 0; k < (unsigned)scaled(4); k++)
            icon_case(helpers, g_entries[i], 0x5acab0u + (unsigned)i * 16 + k);
    helpers.report();

    Tally draw{"hud render: slot 28 of the renderers on real definitions, fake frames"};
    for (int i = 0; i < g_nentries; i++)
        for (unsigned k = 0; k < (unsigned)scaled(30); k++)
            draw_case(draw, g_entries[i], 0x28000u + (unsigned)i * 100 + k);
    draw.report();

    if (!invcheck::arena_setup()) {
        log("Check hud render: no arena");
        return;
    }
    init_vtables();
    Tally update{"hud render: Update and Read on fake worlds"};
    for (int kind = 0; kind < KINDS; kind++) {
        if (kind == CHARACTER || kind == SPHERE)
            continue;   // their slots 25 and 26 are CEnergyInventoryHUDRenderer's
        for (unsigned k = 0; k < (unsigned)scaled(600); k++)
            update_case(update, kind, false, 0x25000u + (unsigned)kind * 1000 + k);
        for (unsigned k = 0; k < (unsigned)scaled(300); k++)
            update_case(update, kind, true, 0x26000u + (unsigned)kind * 1000 + k);
    }
    update.report();
    {
        Tally map{"hud render: the auto-map's fog update and drawing (the level's frames saved and put back)"};
        minimap_checks(map);
        map.report();
    }
    {
        Tally begin{"hud render: CCompassRenderer::Begin on the real layers"};
        BYTE *w = *at<BYTE **>(0x6c2080);
        void *const *layers = w ? *reinterpret_cast<void *const *const *>(w + 0x2b8 + 4) : nullptr;
        const DWORD nlayers = w ? *reinterpret_cast<const DWORD *>(w + 0x2b8 + 8) : 0;
        const BYTE saved_known = *at<BYTE *>(0x62e9d1);
        DWORD saved_target[2];
        memcpy(saved_target, at<void *>(0x62e9d8), 8);
        for (DWORD i = 0; i < nlayers && i < 64; i++) {
            BYTE out[2][12] = {};
            for (int side = 0; side < 2; side++) {
                *at<BYTE *>(0x62e9d1) = 0x5a;
                memset(at<void *>(0x62e9d8), 0x5a, 8);
                invcheck::side_begin(side);
                at<DWORD(THISCALL *)(void *, DWORD, DWORD, void *)>(0x423e90)(nullptr, 0, 0, layers[i]);
                invcheck::side_end(side);
                out[side][0] = *at<BYTE *>(0x62e9d1);
                memcpy(out[side] + 4, at<void *>(0x62e9d8), 8);
            }
            begin.check(memcmp(out[0], out[1], 12) == 0, "", "[Begin on a layer]");
        }
        *at<BYTE *>(0x62e9d1) = saved_known;
        memcpy(at<void *>(0x62e9d8), saved_target, 8);
        begin.report();
    }
}

}  // namespace zax
