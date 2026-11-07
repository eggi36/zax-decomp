// Checks of the inventory screen's detail renderers (hud_details.cpp) against the original, in the game thread,
// with the frame check harness (checks_frame_common.h):
//  - the real item definitions of the catalogue (all loaded first, as checks_hud_render.cpp does) whose
//    "Display/Detail Renderer" is of a class under test; per definition its template item ("Item", +0x1c)
//    copied into the harness's object memory with the case's values (rounds, charge, healing, ability level);
//    the inventory the real one of the first character (read only) or none;
//  - slot 24 (draw) into fake 5-6-5 frames at random detail positions: pixels, the frame call log, the item
//    copy (unchanged) compared; slot 25 (describe): the line's layout values and both texts compared, the
//    line freed; the static "listed elsewhere" 0x714140 and null by value;
//  - stand-ins and state: GetTickCount (0x4d2460) gives the case's ticks; the error reporter 0x6c02cc is a
//    counting fake; the game's random generator (0x7124c0, GAME_RANDOM_BYTES), the renderer's +0x10/+0x14
//    (random overlay), the Sphere of Zin statics (0x713808, 0x7137d4, guard 0x7137fd) and the health message
//    counter 0x62626c are set from the case before each side and put back afterwards. Everything a slot loads
//    on first use is loaded by a run of the original before the cases.
//  - slot 23 0x47ac40 directly, slot 18 of the energy text renderer on copies without the old "Format".
#include "checks_hud_common.h"
#include "x87.h"

namespace zax {

int hooks_hud_details(Hook *out);

namespace {

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
    switch (r % 10) {
    case 0: return 0;
    case 1: return fbits(1.0f);
    case 2: return fbits(-(float)(int)(r >> 12 & 0xff) / 8.0f);
    default: return fbits((float)(int)(r >> 12 & 0x3ff) / 4.0f);
    }
}
template <typename T> T &at_off(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &d32(void *p, DWORD off) { return at_off<DWORD>(p, off); }
BYTE *bptr(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
const char *zs_c_str(const void *z) { return at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
bool kind_of(void *o, DWORD getter) { return o && at<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, at<CClassInfo *(__cdecl *)()>(getter)()); }

struct KindInfo {
    const char *name;
    DWORD vtable;
};
const KindInfo KINDS[] = {
    {"Count", 0x6046d0},          {"Ammunition", 0x604738}, {"Mineral", 0x6047a0},  {"Charger", 0x604808},
    {"Health", 0x604870},         {"Deployable", 0x5f1070}, {"Battery", 0x5f192c},  {"EnergyText", 0x5f1994},
    {"ShieldDetail", 0x5f19fc},   {"Character", 0x600fbc},  {"SphereOfZin", 0x6011d4}, {"Weapon", 0x603fe8},
    {"AnimationOverlay", 0x604050}, {"RandomOverlay", 0x6040b8}, {"Segmented", 0x5ff53c},
};
constexpr int NKINDS = sizeof KINDS / sizeof KINDS[0];

struct Entry {
    BYTE *def, *renderer, *item;
    DWORD index, item_size;
    int kind;
};
constexpr int MAX_ENTRIES = 160;
Entry g_entries[MAX_ENTRIES];
int g_nentries, g_found[NKINDS];

int kind_of_renderer(BYTE *r)
{
    if (!r)
        return -1;
    for (int k = 0; k < NKINDS; k++)
        if (d32(r, 0) == KINDS[k].vtable)
            return k;
    return -1;
}

// A record's object when it is loaded already, else null. The checks load nothing the level has not loaded: the
// resource cache would make room by dropping other resources (models the models checks expect loaded)
BYTE *loaded_record(DWORD records_at, DWORD count_at, DWORD index)
{
    BYTE *records = *at<BYTE **>(records_at);
    if (!records || index >= *at<DWORD *>(count_at) || records[index * 0x80 + 0x7f])
        return nullptr;
    return bptr(d32(records + index * 0x80, 0x78));
}

BYTE *item_definition_of(BYTE *item) { return reinterpret_cast<BYTE *(THISCALL *)(void *)>(original_address(0x4dd480))(item); }

// The "Detail Image" sequences a renderer plays have frames: Sequence_SyncFrame divides by their count and the
// random overlay draws a random one of them without tests (the game's data has none without frames on screen)
bool image_ok(DWORD index)
{
    BYTE *seq = loaded_record(0x6c0978, 0x6c097c, index);
    return seq && vcall<DWORD>(seq, 25) > 0;
}
int kind_of_renderer(BYTE *r);
bool sequences_ok(int k, BYTE *r, BYTE *item)
{
    // the weapon detail's projectile values load the projectile's model (model list 0x6cfdd8, +0x20 of the
    // definition): only weapons whose projectile is loaded already
    if (k == 11) {
        BYTE *def = item_definition_of(item);
        if (!def || !loaded_record(0x6cfe08, 0x6cfe0c, d32(def, 0x20)))
            return false;
    }
    if (k == 10 || k == 12 || k == 13) {   // Sphere of Zin, animation and random overlay: +8, and their next ones
        if (!image_ok(d32(r, 8)))
            return false;
        BYTE *next = bptr(d32(r, 0xc));
        return !next || sequences_ok(kind_of_renderer(next), next, item);
    }
    if (k == 14 && kind_of(item, 0x569560)) {   // segmented: the item's segments
        const DWORD n = d32(item, 0x20);
        for (DWORD i = 0; i < n && i < 64; i++)
            if (!image_ok(d32(bptr(d32(item, 0x1c)), i * 4)))
                return false;
    }
    return true;
}

void collect_entries()
{
    for (DWORD d = 0; *at<BYTE **>(0x6c0c38) && d < *at<DWORD *>(0x6c0c3c) && d < 4096; d++) {
        BYTE item[0x18] = {};
        d32(item, 8) = d;
        reinterpret_cast<BYTE *(THISCALL *)(void *)>(original_address(0x4dd480))(item);
    }
    g_nentries = 0;
    for (int &f : g_found)
        f = 0;
    BYTE *records = *at<BYTE **>(0x6c0c38);
    const DWORD count = records ? *at<DWORD *>(0x6c0c3c) : 0;
    for (DWORD i = 0; i < count && i < 4096; i++) {
        BYTE *rec = records + i * 0x80;
        BYTE *def = bptr(d32(rec, 0x78));
        if (rec[0x7f] || !def || !kind_of(def, 0x5b5f10))
            continue;
        BYTE *r = vcall<BYTE *>(def, 34);
        const int k = kind_of_renderer(r);
        BYTE *item = bptr(d32(def, 0x1c));
        const CClassInfo *c = item ? geo::class_of(item) : nullptr;
        if (k < 0 || !c || c->size > 0x60 || !geo::readable(item, c->size) || !sequences_ok(k, r, item))
            continue;
        g_found[k]++;
        if (g_nentries < MAX_ENTRIES)
            g_entries[g_nentries++] = {def, r, item, i, c->size, k};
    }
}

// Detail renderers of the classes the catalogue does not use, made by their own Create (the registered defaults)
// on real definitions: the energy text, shield and charger renderers on the battery's definition and item, the
// random overlay with an animation overlay's detail image; freed after the checks
BYTE *g_synthetic[4];
void add_synthetic()
{
    int battery = -1, overlay = -1;
    for (int i = 0; i < g_nentries; i++) {
        if (g_entries[i].kind == 6 && battery < 0)
            battery = i;
        if (g_entries[i].kind == 12 && overlay < 0)
            overlay = i;
    }
    const int kinds[4] = {7, 8, 3, 13};
    const DWORD creates[4] = {0x483fa0, 0x4848a0, 0x5b1fc0, 0x5ae080};
    for (int k = 0; k < 4 && g_nentries < MAX_ENTRIES; k++) {
        const int from = kinds[k] == 13 ? overlay : battery;
        g_synthetic[k] = nullptr;
        if (from < 0 || g_found[kinds[k]])
            continue;
        BYTE *r = at<BYTE *(__cdecl *)()>(creates[k])();
        if (!r)
            continue;
        if (kinds[k] == 13)
            d32(r, 8) = d32(g_entries[from].renderer, 8);
        g_synthetic[k] = r;
        Entry e = g_entries[from];
        e.renderer = r;
        e.kind = kinds[k];
        g_entries[g_nentries++] = e;
    }
}
void free_synthetic()
{
    for (BYTE *&r : g_synthetic)
        if (r) {
            vcall<DWORD>(r, 1, 1);
            r = nullptr;
        }
}

// ---- Stand-ins and state ------------------------------------------------------------------------------------

DWORD g_ticks, g_tick_calls, g_reports;
DWORD __stdcall s_ticks() { return g_ticks + 41u * g_tick_calls++; }
DWORD THISCALL s_report(void *, DWORD) { g_reports++; return 0; }
DWORD g_reporter_vt[64];
BYTE g_reporter[16];
CodePatch g_tick_patch;
DWORD g_saved_reporter;
framec::ModuleHooks g_hooks{"hud_details", &hooks_hud_details, {}, {}, {}, 0, true};

struct State {
    BYTE random[GAME_RANDOM_BYTES];
    DWORD phase, last, counter, overlay[2];
    BYTE guard;
};
State g_real, g_case;
BYTE *g_overlay_renderer;
void state_save(State &s)
{
    memcpy(s.random, at<void *>(0x7124c0), GAME_RANDOM_BYTES);
    s.phase = *at<DWORD *>(0x713808);
    s.last = *at<DWORD *>(0x7137d4);
    s.guard = *at<BYTE *>(0x7137fd);
    s.counter = *at<DWORD *>(0x62626c);
    s.overlay[0] = g_overlay_renderer ? d32(g_overlay_renderer, 0x10) : 0;
    s.overlay[1] = g_overlay_renderer ? d32(g_overlay_renderer, 0x14) : 0;
}
void state_set(const State &s)
{
    memcpy(at<void *>(0x7124c0), s.random, GAME_RANDOM_BYTES);
    *at<DWORD *>(0x713808) = s.phase;
    *at<DWORD *>(0x7137d4) = s.last;
    *at<BYTE *>(0x7137fd) = s.guard;
    *at<DWORD *>(0x62626c) = s.counter;
    if (g_overlay_renderer) {
        d32(g_overlay_renderer, 0x10) = s.overlay[0];
        d32(g_overlay_renderer, 0x14) = s.overlay[1];
    }
}
unsigned state_hash()
{
    State s;
    state_save(s);
    return fnv(2166136261u, &s, sizeof s);
}
void stands_on()
{
    for (DWORD &v : g_reporter_vt)
        v = pv((void *)&s_report);
    d32(g_reporter, 0) = pv(g_reporter_vt);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c02cc) = pv(g_reporter);
    patch_jump(g_tick_patch, 0x4d2460, (void *)&s_ticks);
}
void stands_off()
{
    unpatch(g_tick_patch);
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
}

BYTE *g_inventory;   // the first character's (read only)
// A copy of it with no items (+0x20 0): the weapon detail asks the inventory's items for their count (slot 41),
// and some real items' slot 41 reads an owner the check cannot give
BYTE g_empty_inventory[0x100];

// A deployable item's slot 49 (its condition, read from the deployed part, which a template has not): a
// copy of its vtable answers the case's value
DWORD g_deploy_vt[128], g_deploy_value;
long double THISCALL s_deploy_value(void *)
{
    float f;
    memcpy(&f, &g_deploy_value, 4);
    return (long double)f;
}

// The item copy of a case in the object memory (both sides)
DWORD make_item(const Entry &e)
{
    const DWORD off = framec::alloc_object(0x60);
    BYTE v[0x60];
    memcpy(v, e.item, e.item_size);
    if (kind_of(e.item, 0x42a760))   // CCountInventoryItem: "Rounds"
        d32(v, 0x18) = rnd(4) ? rnd(300) : random();
    if (kind_of(e.item, 0x5afbd0))   // CEnergyItem: "Current Charge"
        d32(v, 0x18) = some_float_bits();
    if (kind_of(e.item, 0x5aefe0))   // CHealthItem: "Num Health"
        d32(v, 0x24) = some_float_bits();
    if (kind_of(e.item, 0x57fd00))   // CCharacterInventoryItem: "Special Ability Level"
        d32(v, 0x1c) = rnd(5);
    if (kind_of(e.item, 0x479c40)) {   // CDeployableInventoryItem
        memcpy(g_deploy_vt, bptr(d32(e.item, 0)), sizeof g_deploy_vt);
        g_deploy_vt[49] = pv((void *)&s_deploy_value);
        g_deploy_value = rnd(4) ? fbits((float)rnd(101) / 100.0f) : some_float_bits();
        d32(v, 0) = pv(g_deploy_vt);
    }
    for (int side = 0; side < 2; side++)
        memcpy(framec::object<BYTE>(side, off), v, e.item_size);
    return off;
}

BYTE *case_inventory(const Entry &e)
{
    if (e.kind == 11 || !g_inventory || rnd(3) == 0)
        return g_empty_inventory;
    return g_inventory;
}

void begin_state(const Entry &e)
{
    g_overlay_renderer = e.kind == 13 ? e.renderer : nullptr;
    state_save(g_real);
    g_case = g_real;
    g_case.phase = rnd(3) ? fbits((float)rnd(800) / 100.0f) : some_float_bits();
    g_ticks = random();
    g_case.last = g_ticks - rnd(400);
    g_case.guard = (BYTE)rnd(2);
    g_case.counter = rnd(4) ? rnd(300) : 299 + rnd(3);
    g_case.overlay[0] = g_ticks - rnd(600);
    g_case.overlay[1] = rnd(4);
    stands_on();
}
void end_state()
{
    stands_off();
    state_set(g_real);
}

void draw_case(Tally &t, const Entry &e, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    framec::FrameSpec s;
    s.width = rnd_range(160, 300);
    s.height = rnd_range(100, 200);
    s.pitch = rnd(3) ? 0 : s.width + rnd_range(1, 9);
    const int fi = framec::make_frame(s);
    overflow_room(s);
    if (fi < 0)
        return;
    const DWORD item = make_item(e);
    const int x = rnd_range(-40, s.width - 40), y = rnd_range(-30, s.height - 20);
    BYTE *inv = case_inventory(e);
    begin_state(e);
    char what[96];
    wsprintfA(what, "%s draw, index %u, seed %u", KINDS[e.kind].name, e.index, seed);
    hudcheck::run_sides(t, g_hooks, what, [&](int side) {
        state_set(g_case);
        g_tick_calls = 0;
        g_reports = 0;
        vcall<DWORD>(e.renderer, 24, framec::object<BYTE>(side, item), framec::frame(side, fi), x, y, inv);
        return state_hash() ^ g_reports;
    });
    end_state();
}

// The line of a description made comparable: layout values and texts; then freed
unsigned line_hash(BYTE *line)
{
    if (!line || pv(line) == 0x714140)
        return pv(line);
    unsigned h = fnv(2166136261u, line + 8, 4);
    h = fnv(h, line + 0x10, 4);
    h = fnv(h, line + 0x18, 4);
    h = fnv_text(h, zs_c_str(line + 0xc));
    h = fnv_text(h, zs_c_str(line + 0x14));
    vcall<DWORD>(line, 1, 1);
    return h;
}

void describe_case(Tally &t, const Entry &e, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    const DWORD item = make_item(e);
    BYTE *inv = case_inventory(e);
    begin_state(e);
    char what[96];
    wsprintfA(what, "%s describe, index %u, seed %u", KINDS[e.kind].name, e.index, seed);
    hudcheck::run_sides(t, g_hooks, what, [&](int side) {
        state_set(g_case);
        g_tick_calls = 0;
        g_reports = 0;
        BYTE *line = vcall<BYTE *>(e.renderer, 25, framec::object<BYTE>(side, item), inv);
        return line_hash(line) ^ state_hash() ^ g_reports;
    });
    end_state();
}

int g_h_calls_logged;
// Everything DrawIcon draws is loaded (detail image, overlay image, HUD image, the two inventory icons)
bool icons_loaded(BYTE *def)
{
    if (vcall<DWORD>(def, 33) && !loaded_record(0x6c0978, 0x6c097c, vcall<DWORD>(def, 33)))
        return false;
    BYTE *r = vcall<BYTE *>(def, 34);
    if (r && kind_of(r, 0x5adb50) && !loaded_record(0x6c0978, 0x6c097c, d32(r, 8)))
        return false;
    if (vcall<DWORD>(def, 31) && !loaded_record(0x6c06d0, 0x6c06d4, vcall<DWORD>(def, 31)))
        return false;
    if (d32(def, 0x10) && !loaded_record(0x6c0ad8, 0x6c0adc, d32(def, 0x10)))
        return false;
    return !d32(def, 0x14) || loaded_record(0x6c0ad8, 0x6c0adc, d32(def, 0x14));
}
// The definition's icons (0x5b8ca0, which draws 0x4d69d0) at a random position of a fake frame
void icon_case(Tally &t, BYTE *def, DWORD index, unsigned seed)
{
    g_seed = seed;
    framec::begin_case(seed);
    framec::FrameSpec s;
    s.width = rnd_range(60, 200);
    s.height = rnd_range(60, 150);
    const int fi = framec::make_frame(s);
    overflow_room(s);
    const DWORD pos = framec::alloc_object(8);
    const int p[2] = {rnd_range(-40, s.width), rnd_range(-40, s.height)};
    for (int side = 0; side < 2; side++)
        memcpy(framec::object<BYTE>(side, pos), p, 8);
    char what[64];
    wsprintfA(what, "DrawIcon, index %u, seed %u", index, seed);
    hudcheck::run_sides(t, g_hooks, what, [&](int side) {
        vcall<DWORD>(def, 27, framec::frame(side, fi), framec::object<BYTE>(side, pos));
        return 0u;
    });
    if (t.mismatches && t.mismatches < 3 && (g_h_calls_logged++ < 2))
        for (int side = 0; side < 2; side++) {
            const framec::CallLog &c = framec::g_h.calls[side];
            char line[300];
            int len = wsprintfA(line, "    side %d (%d calls):", side, c.count);
            for (int i = 0; i < c.count && i < 32 && len < 280; i++)
                len += wsprintfA(line + len, " %u", c.first[i][0]);
            log("%s", line);
        }
}

void warm_up(const Entry &e)
{
    g_seed = 0x91u + e.index;
    framec::begin_case(1);
    framec::FrameSpec s;
    s.width = 200;
    s.height = 150;
    const int fi = framec::make_frame(s);
    overflow_room(s);
    const DWORD item = make_item(e);
    begin_state(e);
    state_set(g_case);
    const DWORD draw = d32(bptr(d32(e.renderer, 0)), 24 * 4), describe = d32(bptr(d32(e.renderer, 0)), 25 * 4);
    reinterpret_cast<void(THISCALL *)(void *, void *, void *, int, int, void *)>(original_address(draw))(e.renderer, framec::object<BYTE>(0, item),
                                                                                                    framec::frame(0, fi), 10, 10, case_inventory(e));
    BYTE *line = reinterpret_cast<BYTE *(THISCALL *)(void *, void *, void *)>(original_address(describe))(e.renderer, framec::object<BYTE>(0, item), case_inventory(e));
    line_hash(line);
    end_state();
}

}  // namespace

ZAX_CHECKS(hud_details)
{
    (void)cls;
    (void)n;
    collect_entries();
    add_synthetic();
    BYTE *character = at<BYTE *(THISCALL *)(void *)>(0x4f5d80)(*at<void **>(0x6c2080));
    g_inventory = character ? at<BYTE *(THISCALL *)(void *)>(0x4267e0)(character) : nullptr;
    if (g_inventory) {
        const CClassInfo *c = geo::class_of(g_inventory);
        memcpy(g_empty_inventory, g_inventory, c && c->size <= sizeof g_empty_inventory ? c->size : 0x40);
        d32(g_empty_inventory, 0x20) = 0;
    }
    char counts[200];
    int len = 0;
    for (int k = 0; k < NKINDS; k++)
        len += wsprintfA(counts + len, " %s %d", KINDS[k].name, g_found[k]);
    log("  hud details: inventory %x;%s", pv(g_inventory), counts);
    for (int i = 0; i < g_nentries; i++)
        warm_up(g_entries[i]);
    Tally draw{"hud details: slot 24 of the detail renderers on real definitions, fake frames"};
    for (int i = 0; i < g_nentries; i++)
        for (unsigned k = 0; k < (unsigned)scaled(20); k++)
            draw_case(draw, g_entries[i], 0x24000u + (unsigned)i * 100 + k);
    draw.report();
    Tally describe{"hud details: slot 25 of the detail renderers on real definitions"};
    for (int i = 0; i < g_nentries; i++)
        for (unsigned k = 0; k < (unsigned)scaled(20); k++)
            describe_case(describe, g_entries[i], 0x25000u + (unsigned)i * 100 + k);
    describe.report();
    Tally icons{"hud details: the item definitions' icons (0x5b8ca0, 0x4d69d0), real definitions"};
    {
        BYTE *records = *at<BYTE **>(0x6c0c38);
        const DWORD count = records ? *at<DWORD *>(0x6c0c3c) : 0;
        for (DWORD i = 0; i < count && i < 4096; i++) {
            BYTE *def = bptr(d32(records + i * 0x80, 0x78));
            if (records[i * 0x80 + 0x7f] || !def || !kind_of(def, 0x5b5f10) || !icons_loaded(def))
                continue;
            framec::begin_case(1);
            framec::FrameSpec s;
            const int fi = framec::make_frame(s);
    overflow_room(s);
            const int p[2] = {0, 0};
            reinterpret_cast<void(THISCALL *)(void *, void *, const int *)>(original_address(0x5b8ca0))(def, framec::frame(0, fi), p);
            for (unsigned k = 0; k < (unsigned)scaled(6); k++)
                icon_case(icons, def, i, 0x27000u + i * 16 + k);
        }
    }
    icons.report();
    Tally small{"hud details: slot 23, the energy text's slot 18"};
    for (int k = 0; k < 4; k++) {
        const DWORD o = reinterpret_cast<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(original_address(0x47ac40))(nullptr, k, 1, 2, 3);
        const DWORD x = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x47ac40)(nullptr, k, 1, 2, 3);
        small.check(o == x, "", "[slot 23]");
    }
    for (int i = 0; i < g_nentries; i++) {
        if (g_entries[i].kind != 7)
            continue;
        BYTE copy[2][0x20];
        memcpy(copy[0], g_entries[i].renderer, 0x20);
        memcpy(copy[1], g_entries[i].renderer, 0x20);
        const DWORD o = reinterpret_cast<DWORD(THISCALL *)(void *, DWORD)>(original_address(0x484740))(copy[0], 0) & 0xff;
        const DWORD x = at<DWORD(THISCALL *)(void *, DWORD)>(0x484740)(copy[1], 0) & 0xff;
        small.check(o == x && !memcmp(copy[0], copy[1], 0x20), "", "[CEnergyTextInventoryDetailRenderer slot 18 without the old Format]");
    }
    small.report();
    free_synthetic();
}

}  // namespace zax
