// Checks of the world update and placement code (world_update.cpp) against the original, in the game
// thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (nested calls
// run the original too), side 1 through the reimplementation. Compared: eax (masked where only al is
// defined), an event log of the stand-ins and fake slots, and the state of every object afterwards
// (pointers by number). The game's random generator 0x7124c0 is put back before each side.
//  - Part queries in fresh layers: real CFreeRangePoly parts with random rectangles and flags in two
//    sized layers (some watched by the monitor list), FindBlocker, FindBlockerHere, FindInFront (also
//    with the static's guard cleared), Part_SweepBlocked (the real collision sweep 0x4e8e00) and
//    Entity_PlaceNear (also with an "animated" reference: a part built in a buffer with a facing at
//    +0xc4 while the CEntityAnimated getter 0x48dcc0 returns CFreeRangePoly), compared: results, every
//    part's position, flags, layer and grid cells, the monitor lists, the corridor 0x6c1cb0.
//  - Entity type fix: parts of a fresh layer counted as entities (0x487620 returns CFreeRangePoly) with
//    fake definitions whose +0x74 is a CWayPointsPolygon, a CFreeRangePoly or nothing.
//  - Bounce: fake movers (slots 94, 95, 41, 67, 48) and a stand-in collision sweep 0x4e8e00 that gives
//    scripted hits; the error reporter 0x6c02cc replaced by a fake.
//  - Views: a fake layer (slots 36, 39, 45, the envelope's slot 23, ambient sounds) through ApplyShake,
//    Update, UpdateViews (the world update 0x4e74a0, the listener 0x57c1c0 and 0x4ee100 recorded, the
//    static array 0x6c1bd8 on a block of the check) and DrawFrame (a fake world 0x6c2080, the packet
//    words scripted, the clamp, frame, drawing and the "frame received" writer recorded).
//  - Multiplayer: the sound senders and the local player on a fake world with fake players (zoom +0xd8,
//    view +0xc0), the block writers and the sound manager/resource functions recorded; position words
//    written and read back.
//  - Dirty marks on the real draw queues with random "moving" bytes (0x480fc0 recorded).
//  - Real data (the running level): SurfaceAt on a grid of points, FindBlockerHere and FindInFront for
//    every part of the level (read-only; the sweep 0x4ebbc0 is left out here: the collision sweep tells
//    the parts it hits through slot 67).
#include "check_util.h"
#include "world_update.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

int hooks_world_update(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
using LD_t = long double;

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    if (g_nlog[0] != g_nlog[1])
        log("    log lengths %d / %d", g_nlog[0], g_nlog[1]);
}
constexpr int SLOG = 4096;
DWORD g_slog[2][SLOG];
int g_nslog[2];
void st(DWORD v)
{
    g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4);
    if (g_nslog[g_side] < SLOG)
        g_slog[g_side][g_nslog[g_side]] = v;
    g_nslog[g_side]++;
}
void st_f(float f) { st(bits(f)); }
void log_first_state_difference()
{
    for (int i = 0; i < g_nslog[0] && i < g_nslog[1] && i < SLOG; i++)
        if (g_slog[0][i] != g_slog[1][i]) {
            log("    state %d: %x / %x (of %d / %d)", i, g_slog[0][i], g_slog[1][i], g_nslog[0], g_nslog[1]);
            return;
        }
    if (g_nslog[0] != g_nslog[1])
        log("    state lengths %d / %d", g_nslog[0], g_nslog[1]);
}

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_world_update(nullptr);
    if (!hook_sites_fit("world_update", n, MAX_SITES))
        return;
    hooks_world_update(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) < 0x10000 || ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}

SavedBytes g_generator;
bool g_trace = false;   // true: every step logged before it runs (the last line names a case that ends the game)

// ---- Values ---------------------------------------------------------------------------------------------

DWORD random_float_bits()
{
    constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0xbf000000, 0x3fc00000, 0x40200000,
                                0xc0200000, 0xffc00000, 0x7f800000, 0xff800000, 0x00000001, 0x80000001, 0x7f7fffff,
                                0x4f000000, 0xcf000000, 0x4b000000, 0x447a0000, 0x44fa0000, 0x45bb8000, 0x3f7fffff,
                                0x3effffff, 0x40400000, 0x42c80000, 0xc2c80000, 0x40490fdb, 0xc0490fdb, 0x40c90fdb};
    if (random() % 4 == 0)
        return VALUES[random() % (sizeof VALUES / 4)];
    const float f = (float)((int)(random() % 8000) - 2000) + (float)(random() % 1000) / 1000.0f;
    return bits(f);
}
float random_float()
{
    const DWORD b = random_float_bits();
    float f;
    memcpy(&f, &b, 4);
    return f;
}
float small_float(int range) { return (float)((int)(random() % (2 * range)) - range) + (float)(random() % 1000) / 1000.0f; }

// ---- Two sides -------------------------------------------------------------------------------------------

template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        if (g_trace)
            log("    %s seed %u side %d", what, seed, side);
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = g_nslog[side] = 0;
        restore_bytes(g_generator);
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
        st(fnv(2166136261u, at<void *>(GAME_RANDOM), GAME_RANDOM_BYTES));
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r[0], r[1], mask, g_nlog[0],
              g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        log_first_state_difference();
    }
    t.check(same, "world update", d);
    return same;
}

struct Patches {
    CodePatch p[40];
    int n = 0;
    void add(DWORD address, void *target) { patch_jump(p[n++], address, target); }
    void off()
    {
        while (n > 0)
            unpatch(p[--n]);
    }
};

// ---- Fresh layers with real parts ------------------------------------------------------------------------

Layer *new_layer()
{
    const DWORD saved_sound = *at<DWORD *>(SOUND_LAYER);
    Layer *l = static_cast<Layer *>(at<void *(__cdecl *)()>(0x4e99b0)());
    *at<DWORD *>(SOUND_LAYER) = saved_sound;
    return l;
}
void delete_layer(Layer *l) { vcall<void *>(l, 1, 1); }
void sized(Layer *l, int w, int h)
{
    l->data.width = w;
    l->data.height = h;
    at<void *(THISCALL *)(void *, int, int)>(0x5793e0)(l->data.grid, w, h);
    at<void(THISCALL *)(void *, int, int)>(0x53f680)(l->data.tile_map, w, h);
}
CFieldDesc *polygon_field()
{
    CClassInfo *c = *at<CClassInfo **>(0x6bfb08);
    for (DWORD i = 0; c && i < c->num_fields; i++)
        if (c->fields[i]->offset == 0x4c && c->fields[i]->vftable == 0x5fd2a0)
            return c->fields[i];
    return nullptr;
}
CClassInfo *free_range_class() { return *at<CClassInfo **>(0x6bfb08); }

constexpr int NPARTS = 10;
BYTE *g_parts[NPARTS];
alignas(16) BYTE g_anim[0x100];   // the "animated" reference, built in place
Layer *g_layers[2];

DWORD part_index(const void *p)
{
    if (!p)
        return 0;
    for (int i = 0; i < NPARTS; i++)
        if (p == g_parts[i])
            return 0x9a000000u + (DWORD)i;
    if (p == g_anim)
        return 0x9b000000u;
    for (int i = 0; i < 2; i++)
        if (p == g_layers[i])
            return 0x9c000000u + (DWORD)i;
    if (readable(p, 4))
        return 0xc0000000u | (*static_cast<const DWORD *>(p) & 0xffffff);   // by vtable
    return 0xdeadu;
}

void set_polygon(void *part, int x, int y, int w, int h)
{
    char text[200];
    wsprintfA(text, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d", x, y, x + w, y, x + w, y + h, x, y + h, x, y);
    if (CFieldDesc *pf = polygon_field())
        vcall<DWORD>(pf, 19, part, text, 0);
}

void random_part(BYTE *p, int width, int height)
{
    const int w = 4 + (int)(random() % 200), h = 4 + (int)(random() % 160);
    set_polygon(p, (int)(random() % (DWORD)width) - 50, (int)(random() % (DWORD)height) - 50, w, h);
    DWORD &flags = *reinterpret_cast<DWORD *>(p + 0x1c);
    if (random() % 5 == 0)
        flags &= ~0x40000u;
    if (random() % 6 == 0)
        flags |= 0x2000000;
    if (random() % 7 == 0)
        flags &= ~0x800000u;
}

void st_part(const BYTE *p)
{
    float pos[2];
    vcall<DWORD>(p, 43, pos);
    st_f(pos[0]);
    st_f(pos[1]);
    st(*reinterpret_cast<const DWORD *>(p + 0x1c));
    st(part_index(*reinterpret_cast<void *const *>(p + 0x48)));
    st(*reinterpret_cast<const DWORD *>(p + 0x30));
    st(*reinterpret_cast<const DWORD *>(p + 0x34));
}

void st_layer(Layer *l)
{
    const LayerArray &m = l->data.monitor;
    st(m.count);
    for (DWORD i = 0; i < m.count; i++)
        st(part_index(*reinterpret_cast<void *const *>(static_cast<const BYTE *>(m.data[i]) + 8)));
    const BYTE *g = l->data.grid;
    const DWORD cols = *reinterpret_cast<const DWORD *>(g + 0x10), rows = *reinterpret_cast<const DWORD *>(g + 0x14);
    const LayerArray *cells = *reinterpret_cast<LayerArray *const *>(g + 0x18);
    for (DWORD i = 0; cells && i < cols * rows; i++) {
        st(cells[i].count);
        for (DWORD k = 0; k < cells[i].count; k++)
            st(part_index(cells[i].data[k]));
    }
}

void build_layers(bool anim, bool monitors = true)
{
    for (int i = 0; i < 2; i++) {
        g_layers[i] = new_layer();
        sized(g_layers[i], i ? 1000 + (int)(random() % 2000) : 2000, 1500);
    }
    for (int i = 0; i < NPARTS; i++) {
        g_parts[i] = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x4b5d50)());
        random_part(g_parts[i], 1900, 1400);
        const DWORD where = random() % 6;
        if (where < 5)
            at<DWORD(THISCALL *)(void *, void *)>(0x4eb6f0)(g_layers[where < 4 ? 0 : 1], g_parts[i]);
        if (monitors && where < 5 && random() % 4 == 0)
            at<DWORD(THISCALL *)(void *, void *)>(0x4e7330)(&g_layers[where < 4 ? 0 : 1]->data, g_parts[i]);
    }
    if (anim) {
        memset(g_anim, 0, sizeof g_anim);
        at<void *(THISCALL *)(void *)>(0x4b5fa0)(g_anim);
        random_part(g_anim, 1900, 1400);
        const float facing = random() % 3 ? small_float(7) : small_float(100000);
        memcpy(g_anim + 0xc4, &facing, 4);
        at<DWORD(THISCALL *)(void *, void *)>(0x4eb6f0)(g_layers[0], g_anim);
    }
}

void destroy_layers(bool anim)
{
    if (anim) {
        if (Layer *l = *reinterpret_cast<Layer **>(g_anim + 0x48))
            at<void(THISCALL *)(void *, void *)>(0x4ebcb0)(l, g_anim);
        at<DWORD(THISCALL *)(void *)>(0x4b5fd0)(g_anim);
    }
    for (int i = 0; i < NPARTS; i++)
        if (g_parts[i] && !*reinterpret_cast<void **>(g_parts[i] + 0x48))
            vcall<void *>(g_parts[i], 1, 1);
    for (Layer *&l : g_layers) {
        delete_layer(l);
        l = nullptr;
    }
    for (BYTE *&p : g_parts)
        p = nullptr;
}

CClassInfo *THISCALL anim_class() { return free_range_class(); }

DWORD queries_case(DWORD &mask)
{
    const bool anim = random() % 4 == 0;
    build_layers(anim);
    Patches patches;
    if (anim)
        patches.add(0x48dcc0, (void *)&anim_class);
    CClassInfo *const classes[3] = {nullptr, free_range_class(), *at<CClassInfo **>(0x713df4)};
    const DWORD alls[3] = {0, 0x840000, 0x40000}, nones[3] = {0, 0x2000000, 0x20000};
    for (int step = 0; step < 6; step++) {
        BYTE *p = g_parts[random() % NPARTS];
        Layer *l = g_layers[random() % 4 == 0 ? 1 : 0];
        DWORD v = 0;
        const DWORD op = random() % 6;
        if (g_trace)
            log("    step %d op %u", step, op);
        switch (op) {
        case 0: {
            const float pt[2] = {small_float(1100) + 900, small_float(800) + 700};
            v = part_index(at<void *(THISCALL *)(Layer *, void *, const float *, CClassInfo *, DWORD, DWORD)>(0x4eb050)(
                l, p, pt, classes[random() % 3], alls[random() % 3], nones[random() % 3]));
            break;
        }
        case 1:
            v = part_index(at<void *(THISCALL *)(Layer *, void *, CClassInfo *, DWORD, DWORD)>(0x4eb1c0)(l, p, classes[random() % 3],
                                                                                                     alls[random() % 3], nones[random() % 3]));
            break;
        case 2: {
            if (random() % 8 == 0)
                *at<BYTE *>(0x6c1c88) &= ~1;
            g_atexit_calls = 0;
            const float from[2] = {small_float(1100) + 900, small_float(800) + 700};
            // (no NaN or infinite angles: 0x508fe0 indexes its tables with them)
            const float angle = random() % 5 ? small_float(7) : small_float(100000);
            const float half = random() % 5 ? (float)(random() % 60) : small_float(100000);
            const float dist = random() % 5 ? (float)(random() % 900) : (float)(random() % 30000);
            v = part_index(at<void *(THISCALL *)(Layer *, void *, const float *, float, float, float, DWORD, DWORD)>(0x4eb200)(
                l, p, from, angle, half, dist, alls[random() % 3], nones[random() % 3]));
            *at<BYTE *>(0x6c1c88) |= 1;
            st(g_atexit_calls * 0x100 + g_atexit_function % 0x100);
            for (int k = 0; k < 8; k++)
                st(at<DWORD *>(0x6c1cb0)[k]);
            break;
        }
        case 3: {
            const float a[4] = {small_float(1100) + 900, small_float(800) + 700, small_float(1100) + 900, small_float(800) + 700};
            const bool own = random() % 3 == 0 && *reinterpret_cast<void **>(p + 0x48);   // null: the part's layer
            v = at<DWORD(__fastcall *)(void *, Layer *, float, float, float, float)>(0x4ebbc0)(p, own ? nullptr : l, a[0], a[1], a[2],
                                                                                             a[3]) & 0xff;
            break;
        }
        default: {
            void *ref = anim ? static_cast<void *>(g_anim) : static_cast<void *>(g_parts[random() % NPARTS]);
            if (ref == p)
                break;
            const float maxd = random() % 6 ? (float)(random() % 700) : -small_float(100);   // finite: NaN never ends
            v = at<DWORD(THISCALL *)(Layer *, void *, void *, float)>(0x4eb7b0)(l, p, ref, maxd);
            v = 0;   // CSortList2D_Insert's eax (the cell array)
            break;
        }
        }
        ev('Q', op, v);
    }
    patches.off();
    for (BYTE *p : g_parts)
        st_part(p);
    if (anim)
        st_part(g_anim);
    st_layer(g_layers[0]);
    st_layer(g_layers[1]);
    destroy_layers(anim);
    mask = 0;
    return 0;
}

// ---- Entity type fix ---------------------------------------------------------------------------------------

alignas(4) BYTE g_defs[NPARTS][0x80];
void *g_templates[3];
CClassInfo *__cdecl entity_class() { return free_range_class(); }
BYTE *THISCALL fake_definition(void *e)
{
    const DWORD i = part_index(e) - 0x9a000000u;
    ev('d', part_index(e));
    return i < NPARTS ? g_defs[i] : g_defs[0];
}
void __cdecl fake_pump() { ev('p'); }

DWORD entity_case(DWORD &mask)
{
    // No monitor entries here: a replaced part is deleted by the grid, and the copy takes over its fields
    // (CObject_Assign) including the broadcaster the monitor list listens to
    build_layers(false, false);
    g_templates[0] = at<void *(__cdecl *)()>(0x4b5d50)();
    g_templates[1] = pool_alloc(0x78);
    at<void *(THISCALL *)(void *)>(0x599290)(g_templates[1]);
    g_templates[2] = nullptr;
    for (int i = 0; i < NPARTS; i++) {
        memset(g_defs[i], 0, sizeof g_defs[i]);
        *reinterpret_cast<void **>(g_defs[i] + 0x74) = g_templates[random() % 3];
    }
    bool in_layer[NPARTS];
    for (int i = 0; i < NPARTS; i++)
        in_layer[i] = *reinterpret_cast<void **>(g_parts[i] + 0x48) != nullptr;
    Patches patches;
    patches.add(0x487620, (void *)&entity_class);
    patches.add(0x48b290, (void *)&fake_definition);
    patches.add(0x5998f0, (void *)&fake_pump);
    at<void(THISCALL *)(LayerSaveData *)>(0x4e97c0)(&g_layers[random() % 2]->data);
    patches.off();
    st_layer(g_layers[0]);
    st_layer(g_layers[1]);
    // Parts replaced by the fix were deleted by the grid: forget them before the layers go
    for (int i = 0; i < NPARTS; i++) {
        bool found = false;
        for (Layer *l : g_layers) {
            const BYTE *g = l->data.grid;
            const DWORD cols = *reinterpret_cast<const DWORD *>(g + 0x10), rows = *reinterpret_cast<const DWORD *>(g + 0x14);
            const LayerArray *cells = *reinterpret_cast<LayerArray *const *>(g + 0x18);
            for (DWORD c = 0; cells && c < cols * rows && !found; c++)
                for (DWORD k = 0; k < cells[c].count; k++)
                    if (cells[c].data[k] == g_parts[i])
                        found = true;
        }
        if (!found && !in_layer[i])
            found = true;   // never in a layer: deleted below (a replaced part's memory is not read again)
        if (!found)
            g_parts[i] = nullptr;
    }
    for (BYTE *p : g_parts)
        if (p)
            st_part(p);
    destroy_layers(false);
    vcall<void *>(g_templates[0], 1, 1);
    vcall<void *>(g_templates[1], 1, 1);
    mask = 0;
    return 0;
}

// ---- Fakes for the bounce, the views and the multiplayer --------------------------------------------------

constexpr int NFAKE = 12;
struct Fake {
    DWORD vftable;
    DWORD id;
    void *parent;   // +0x08 (unused)
    void *up;       // +0x0c the parent chain of 0x4fc6f0
    BYTE data[0x120];
};
Fake g_fakes[NFAKE];
DWORD g_vt_fake[100];
DWORD fid(const void *p)
{
    for (int i = 0; i < NFAKE; i++)
        if (p == &g_fakes[i])
            return 0xfa000000u + (DWORD)i;
    return part_index(p);
}
DWORD tag(const void *p)
{
    if (!p)
        return 0;
    const DWORD f = fid(p);
    if ((f >> 24) == 0xfa)
        return f;
    return ptrv(p) > 0x10000 && ptrv(p) < 0x400000 ? 0x57ac0000u : (ptrv(p) >= 0x400000 && ptrv(p) < IMAGE_END ? ptrv(p) : 0x77777777u);
}
DWORD script(DWORD id, DWORD slot)
{
    const DWORD c = g_nlog[g_side];
    return fnv(fnv(fnv(2166136261u, &id, 4), &slot, 4), &c, 4) ^ g_seed;
}

// Mover slots
DWORD THISCALL mover_motion(Fake *f, BounceMotion *m)
{
    ev('m', fid(f), tag(m));
    m->move[0] = random() % 4 == 0 ? 0.0f : small_float(40);
    m->move[1] = random() % 4 == 0 ? 0.0f : small_float(40);
    if (random() % 10 == 0)
        m->move[0] = random_float();
    m->velocity[0] = small_float(200);
    m->velocity[1] = random() % 8 ? small_float(200) : random_float();
    m->friction = (float)(random() % 100) / 50.0f;
    m->bounce = random() % 10 ? (float)(random() % 100) / 100.0f : 1.5f;
    m->damping = random() % 6 ? (float)(random() % 100) / 100.0f : random_float();
    m->min_speed = (float)(random() % 30);
    return 1;
}
DWORD THISCALL mover_result(Fake *f, const BounceResult *r) { ev('r', fid(f), fnv(2166136261u, r, sizeof *r)); return 0; }
DWORD THISCALL fake_move(Fake *f, DWORD x, DWORD y) { ev('p', fid(f), x, y); return 0; }
DWORD THISCALL mover_hit(Fake *f, void *hit, const float *point, const float *normal, DWORD z)
{
    ev('h', fid(f), tag(hit), fnv(fnv(2166136261u, point, 8), normal, 8) ^ z);
    return 0;
}
void *THISCALL fake_shape(Fake *f) { return &g_fakes[(f->id + 1) % NFAKE]; }
// Layer slots
DWORD THISCALL fake_rect(Fake *f, int *r)
{
    ev('R', fid(f));
    for (int k = 0; k < 4; k++)
        r[k] = (int)(script(fid(f), 36 + k) % 3000) - 500;
    return 0;
}
DWORD THISCALL fake_size(Fake *f, DWORD slot)
{
    const DWORD v = script(fid(f), slot);
    ev('S', fid(f), slot);
    switch (v % 5) {
    case 0: return v % 2000;
    case 1: return 640 + v % 3;
    case 2: return v;
    case 3: return 0;
    default: return 800;
    }
}
DWORD THISCALL fake_slot39(Fake *f) { return fake_size(f, 39); }
DWORD THISCALL fake_slot45(Fake *f) { return fake_size(f, 45); }
DWORD THISCALL fake_envelope(Fake *f, DWORD dt) { ev('E', fid(f), dt); return 0; }
// World and players
DWORD THISCALL world_object(Fake *f) { ev('w', fid(f)); return ptrv(&g_fakes[2]); }
DWORD THISCALL world_players(Fake *f) { ev('W', fid(f)); return ptrv(&g_fakes[3]); }
DWORD THISCALL fake_byte(Fake *f) { const DWORD v = script(fid(f), 12); ev('b', fid(f)); return v & 0x101; }
DWORD THISCALL players_count(Fake *f) { ev('n', fid(f)); return f->data[0]; }
DWORD THISCALL players_slot12(Fake *f, DWORD a) { ev('c', fid(f), a); return 0x1200 + a; }
DWORD THISCALL players_packet(Fake *f, DWORD i) { ev('k', fid(f), i); return ptrv(&g_fakes[8]); }
DWORD THISCALL players_player(Fake *f, DWORD i) { ev('y', fid(f), i); return ptrv(&g_fakes[4 + i % 3]); }
// Sound resources
DWORD THISCALL res_place(Fake *f, const float *p, DWORD a) { ev('4', fid(f), fnv(2166136261u, p, 12), a); return 4; }
DWORD THISCALL res_radii(Fake *f, DWORD a, DWORD b, DWORD c) { ev('5', fid(f), a ^ (b << 1), c); return 5; }
DWORD THISCALL res_stop(Fake *f) { ev('6', fid(f)); return 6; }
// Error reporter
DWORD THISCALL reporter_warn(Fake *f, const char *text) { ev('!', fid(f), fnv_text(2166136261u, text)); return 0; }
// Fake part for the senders: slot 43 a point
DWORD THISCALL part_point(Fake *f, float *out)
{
    ev('3', fid(f));
    out[0] = small_float(1500);
    out[1] = random() % 6 ? small_float(1500) : random_float();
    return 43;
}

void init_fakes()
{
    memset(g_fakes, 0, sizeof g_fakes);
    for (DWORD &s : g_vt_fake)
        s = 0;
    g_vt_fake[4] = ptrv((void *)&res_place);
    g_vt_fake[5] = ptrv((void *)&res_radii);
    g_vt_fake[11] = ptrv((void *)&res_stop);
    g_vt_fake[17] = ptrv((void *)&reporter_warn);
    g_vt_fake[23] = ptrv((void *)&fake_envelope);
    g_vt_fake[36] = ptrv((void *)&fake_rect);
    g_vt_fake[39] = ptrv((void *)&fake_slot39);
    g_vt_fake[41] = ptrv((void *)&fake_move);
    g_vt_fake[43] = ptrv((void *)&part_point);
    g_vt_fake[45] = ptrv((void *)&fake_slot45);
    g_vt_fake[48] = ptrv((void *)&fake_shape);
    g_vt_fake[67] = ptrv((void *)&mover_hit);
    g_vt_fake[94] = ptrv((void *)&mover_motion);
    g_vt_fake[95] = ptrv((void *)&mover_result);
    for (int i = 0; i < NFAKE; i++) {
        g_fakes[i].vftable = ptrv(g_vt_fake);
        g_fakes[i].id = (DWORD)i;
    }
}
// Different objects need different slots with the same number: per-object tables
DWORD g_vt_world[100], g_vt_obj1[100], g_vt_players[100];
void init_world_fakes()
{
    memcpy(g_vt_world, g_vt_fake, sizeof g_vt_world);
    memcpy(g_vt_obj1, g_vt_fake, sizeof g_vt_obj1);
    memcpy(g_vt_players, g_vt_fake, sizeof g_vt_players);
    g_vt_world[1] = ptrv((void *)&world_object);
    g_vt_world[44] = ptrv((void *)&world_players);
    g_vt_obj1[12] = ptrv((void *)&fake_byte);
    g_vt_players[11] = ptrv((void *)&players_count);
    g_vt_players[12] = ptrv((void *)&players_slot12);
    g_vt_players[13] = ptrv((void *)&players_packet);
    g_vt_players[23] = ptrv((void *)&players_player);
    g_fakes[1].vftable = ptrv(g_vt_world);
    g_fakes[2].vftable = ptrv(g_vt_obj1);
    g_fakes[3].vftable = ptrv(g_vt_players);
    g_fakes[3].data[0] = (BYTE)(random() % 4);
    for (int i = 4; i < 7; i++) {   // players: view +0xc0/+0xc4, zoom +0xd8 (the fake's data starts at +0x10)
        float *f = reinterpret_cast<float *>(reinterpret_cast<BYTE *>(&g_fakes[i]) + 0xc0);
        f[0] = small_float(1500);
        f[1] = small_float(1500);
        float zoom = random() % 5 ? (float)(1 + random() % 3) / 2.0f : random_float();
        memcpy(reinterpret_cast<BYTE *>(&g_fakes[i]) + 0xd8, &zoom, 4);
    }
}

// ---- Stand-ins for the exe functions -----------------------------------------------------------------------

DWORD THISCALL rec_sweep(void *sd, void *shape, float *pos, BounceMotion *m, SweepHit *h, void **hit_part, void *mover, DWORD a6, DWORD a7,
                         DWORD a8)
{
    // (owner, move and the two bytes: +0x10..+0x1b stay unset until 0x545b30 sets them after a hit, so in the
    // first sweep of 0x4e96c0 they are whatever the stack held on each side)
    ev('X', tag(sd), tag(shape), fnv(fnv(fnv(2166136261u, pos, 8), reinterpret_cast<BYTE *>(m) + 4, 0xc), reinterpret_cast<BYTE *>(m) + 0x1c, 2));
    ev('x', tag(h), tag(mover), a6 ^ a7 ^ a8);
    st(fnv(2166136261u, reinterpret_cast<BYTE *>(m) + 0x20, 0x18));
    if (random() % 3 == 0)
        return 0x100;   // al 0
    h->t = random() % 6 ? (float)(random() % 100) / 100.0f : random_float();
    h->delta[0] = (float)((LD_t)m->move[0] * 0.5f);
    h->delta[1] = random() % 8 ? small_float(10) : random_float();
    const DWORD n = random() % 4;
    const float normals[4][2] = {{1, 0}, {0, -1}, {0.6f, 0.8f}, {-0.70710678f, 0.70710678f}};
    h->normal[0] = random() % 10 ? normals[n][0] : random_float();
    h->normal[1] = normals[n][1];
    h->point[0] = small_float(1000);
    h->point[1] = small_float(1000);
    h->other[0] = small_float(10);
    h->other[1] = small_float(10);
    if (hit_part)
        *hit_part = &g_fakes[9];
    return 1;
}
DWORD THISCALL rec_world_update(void *sd, LayerArray *rects, DWORD dt)
{
    ev('U', tag(sd), rects->count, dt);
    ev('u', fnv(2166136261u, rects->data, rects->count * 16), rects->capacity >= rects->count);
    return 0;
}
DWORD THISCALL rec_ambient(void *sound, const float *listener) { ev('A', tag(sound), fnv(2166136261u, listener, 12)); return 0; }
DWORD THISCALL rec_listener(void *cache, float *out)
{
    ev('L', cache != nullptr);
    out[0] = small_float(1500);
    out[1] = small_float(100);
    out[2] = random() % 8 ? small_float(1500) : random_float();
    return 0;
}
DWORD THISCALL rec_clamp(void *layer, float *view)
{
    ev('C', tag(layer), bits(view[0]), bits(view[1]));
    if (random() % 2)
        view[0] = (float)(random() % 900);
    return 0;
}
DWORD THISCALL rec_frame_pos(void *layer, int x, int y) { ev('F', tag(layer), (DWORD)x, (DWORD)y); return 0; }
DWORD THISCALL rec_draw_view(void *sd, void *layer, void *packet, const int *rect) { ev('V', tag(sd) ^ tag(layer), tag(packet), fnv(2166136261u, rect, 16)); return 0; }
DWORD THISCALL rec_draw_layer(void *sd, void *layer, const int *rect) { ev('v', tag(sd) ^ tag(layer), fnv(2166136261u, rect, 16)); return 0; }
DWORD THISCALL rec_frame_received(void *reader, DWORD c) { ev('Z', reader != nullptr, c); return 1; }
DWORD THISCALL rec_read_word(void *packet, WORD *out)
{
    ev('r', tag(packet));
    const DWORD v = random();
    if (v % 9 == 0)
        return v & 0xff00;   // al 0, nothing written
    *out = (WORD)(v % 7 == 0 ? 0xffff : v % 5 == 0 ? 0x8000 + (v >> 8) % 0x100 : (v >> 8) % 3000);
    return 1;
}
DWORD THISCALL rec_write_word(void *packet, DWORD w) { ev('w', tag(packet), w); return 0x5209c000u + (w & 0xff); }
DWORD THISCALL rec_simple(void *reader, void *p, void *player, void *layer, DWORD sound, const float *pos)
{
    ev('s', reader != nullptr, tag(p) ^ tag(player), tag(layer) ^ sound);
    ev('t', fnv(2166136261u, pos, 8));
    return 1;
}
DWORD THISCALL rec_play(void *reader, void *p, void *player, void *layer, DWORD sound, const float *pos, DWORD mode, DWORD keep, DWORD ch)
{
    ev('P', reader != nullptr, tag(p) ^ tag(player), tag(layer) ^ sound);
    ev('q', fnv(2166136261u, pos, 8), mode ^ (keep << 8), ch & 0xff);
    return 1;
}
DWORD THISCALL rec_free_channel(void *reader, DWORD sound) { ev('f', reader != nullptr, sound); return random() % 4 ? random() % 8 : 0xff; }
void *THISCALL rec_sound_at(void *reader, DWORD sound, DWORD ch)
{
    ev('a', reader != nullptr, sound, ch);
    return random() % 3 ? &g_fakes[10] : nullptr;
}
DWORD THISCALL rec_sound_remove(void *reader, DWORD sound, DWORD ch) { ev('-', reader != nullptr, sound, ch); return 1; }
DWORD THISCALL rec_mgr_play(void *mgr, DWORD sound) { ev('M', ptrv(mgr), sound); return 1; }
void *THISCALL rec_mgr_find(void *mgr, DWORD sound) { ev('m', ptrv(mgr), sound); return &g_fakes[11]; }
void *THISCALL rec_replacement(void *o) { ev('o', tag(o)); return random() % 4 ? &g_fakes[10] : nullptr; }
DWORD THISCALL rec_keep(void *r, DWORD k) { ev('K', tag(r), k); return 8; }
DWORD THISCALL rec_resident(void *r, DWORD a) { ev('N', tag(r), a); return 1; }
DWORD THISCALL rec_valid(void *r) { ev('I', tag(r)); return random() % 3 ? 0x101 : 0x100; }
DWORD THISCALL rec_reload(void *r, const float *p, DWORD a) { ev('O', tag(r), fnv(2166136261u, p, 12), a); return 1; }
DWORD THISCALL rec_dirty(void *frame, const int *rect, DWORD flag) { ev('D', tag(frame), fnv(2166136261u, rect, 16), flag); return 0; }


// ---- Bounce ------------------------------------------------------------------------------------------------

DWORD bounce_case(DWORD &mask)
{
    init_fakes();
    Fake *mover = &g_fakes[0];
    float *mf = reinterpret_cast<float *>(reinterpret_cast<BYTE *>(mover) + 0x4c);
    mf[0] = small_float(1000);
    mf[1] = small_float(1000);
    float *back = reinterpret_cast<float *>(reinterpret_cast<BYTE *>(mover) + 0x100);
    back[0] = small_float(1000);
    back[1] = small_float(1000);
    void **reporter = at<void **>(0x6c02cc);
    void *saved_reporter = *reporter;
    *reporter = &g_fakes[7];
    Patches patches;
    patches.add(0x4e8e00, (void *)&rec_sweep);
    BounceResult out;
    memset(&out, 0x5a, sizeof out);
    DWORD r;
    alignas(4) BYTE sd[0x190];
    memset(sd, 0, sizeof sd);
    if (random() % 2) {
        r = at<DWORD(THISCALL *)(void *, void *, BounceResult *, DWORD)>(0x4e96c0)(sd, mover, &out, random() % 3) & 0xff;
    } else {
        BounceMotion m;
        memset(&m, 0, sizeof m);
        m.vftable = 0x5f8ba8;
        mover_motion(mover, &m);
        float pos[2] = {small_float(1000), small_float(1000)};
        void *hit = nullptr;
        r = at<DWORD(THISCALL *)(void *, void *, float *, BounceMotion *, BounceResult *, void **, void *, DWORD, DWORD, DWORD)>(0x4e91e0)(
                sd, &g_fakes[5], pos, &m, &out, &hit, mover, 1, 2, 3) & 0xff;
        st_f(pos[0]);
        st_f(pos[1]);
        st(tag(hit));
    }
    patches.off();
    *reporter = saved_reporter;
    for (DWORD i = 0; i < sizeof out / 4; i++)
        st(reinterpret_cast<DWORD *>(&out)[i]);
    mask = 0xff;
    return r;
}

// ---- Views ---------------------------------------------------------------------------------------------------

alignas(16) BYTE g_layer_buf[0x1d0];
void *g_ambient[6];

Layer *fake_layer()
{
    BYTE *l = g_layer_buf;
    memset(l, 0, sizeof g_layer_buf);
    *reinterpret_cast<DWORD *>(l) = ptrv(g_vt_fake);
    for (DWORD off = 0x80; off < 0x88; off += 4)
        *reinterpret_cast<float *>(l + off) = random() % 3 ? (float)(random() % 1500) : random_float();
    *reinterpret_cast<int *>(l + 0x88) = (int)(random() % 1500);
    *reinterpret_cast<int *>(l + 0x8c) = (int)(random() % 1500);
    *reinterpret_cast<int *>(l + 0x48) = 500 + (int)(random() % 5000);   // width, height of the save data
    *reinterpret_cast<int *>(l + 0x4c) = 500 + (int)(random() % 5000);
    if (random() % 4)
        *reinterpret_cast<void **>(l + 0x7c) = &g_fakes[6];   // tile map
    const DWORD floats[] = {0x100, 0x104, 0x110, 0x114, 0x11c, 0x12c, 0x130};
    for (DWORD off : floats) {
        float f = random() % 3 ? (float)(random() % 40) / 4.0f : random_float();
        if (random() % 5 == 0)
            f = 0.0f;
        memcpy(l + off, &f, 4);
    }
    *reinterpret_cast<DWORD *>(l + 0x108) = ptrv(g_vt_fake);   // the envelope's vtable
    for (int i = 0; i < 6; i++)
        g_ambient[i] = random() % 3 ? static_cast<void *>(&g_fakes[i]) : nullptr;
    *reinterpret_cast<void ***>(l + 0xf4) = g_ambient;
    *reinterpret_cast<DWORD *>(l + 0xf8) = random() % 7;
    return reinterpret_cast<Layer *>(l);
}

void st_fake_layer()
{
    for (DWORD i = 4; i < sizeof g_layer_buf / 4; i++)
        st(reinterpret_cast<DWORD *>(g_layer_buf)[i]);
}

struct ViewState {
    LayerArray rects;
    BYTE guard, guard_find;
    DWORD world, sound_layer, net, draw_direct;
};
ViewState g_view_saved;

void save_view_state()
{
    g_view_saved.rects = *at<LayerArray *>(VIEW_RECTS);
    g_view_saved.guard = *at<BYTE *>(0x6c1be8);
    g_view_saved.guard_find = *at<BYTE *>(0x6c1c88);
    g_view_saved.world = *at<DWORD *>(0x6c2080);
    g_view_saved.sound_layer = *at<DWORD *>(SOUND_LAYER);
    g_view_saved.net = *at<DWORD *>(NET_MODE);
    g_view_saved.draw_direct = *at<BYTE *>(DRAW_DIRECT);
}
void restore_view_state()
{
    *at<LayerArray *>(VIEW_RECTS) = g_view_saved.rects;
    *at<BYTE *>(0x6c1be8) = g_view_saved.guard;
    *at<BYTE *>(0x6c1c88) = g_view_saved.guard_find;
    *at<DWORD *>(0x6c2080) = g_view_saved.world;
    *at<DWORD *>(SOUND_LAYER) = g_view_saved.sound_layer;
    *at<DWORD *>(NET_MODE) = g_view_saved.net;
    *at<BYTE *>(DRAW_DIRECT) = (BYTE)g_view_saved.draw_direct;
}

void add_view_patches(Patches &p)
{
    p.add(0x4e74a0, (void *)&rec_world_update);
    p.add(0x4ee100, (void *)&rec_ambient);
    p.add(0x57c1c0, (void *)&rec_listener);
    p.add(0x4ed7f0, (void *)&rec_clamp);
    p.add(0x524fe0, (void *)&rec_frame_pos);
    p.add(0x4e7c00, (void *)&rec_draw_view);
    p.add(0x4e8900, (void *)&rec_draw_layer);
    p.add(0x4f7bd0, (void *)&rec_frame_received);
    p.add(0x520510, (void *)&rec_read_word);
    p.add(0x5209c0, (void *)&rec_write_word);
    p.add(0x4f8b90, (void *)&rec_simple);
    p.add(0x4f8e30, (void *)&rec_play);
    p.add(0x4f8ec0, (void *)&rec_free_channel);
    p.add(0x4f8f30, (void *)&rec_sound_at);
    p.add(0x4f8f90, (void *)&rec_sound_remove);
    p.add(0x57e890, (void *)&rec_mgr_play);
    p.add(0x57e650, (void *)&rec_mgr_find);
    p.add(0x57b630, (void *)&rec_replacement);
    p.add(0x57bbc0, (void *)&rec_keep);
    p.add(0x57b560, (void *)&rec_resident);
    p.add(0x57b740, (void *)&rec_valid);
    p.add(0x57b290, (void *)&rec_reload);
    p.add(0x5d0863, (void *)&fake_atexit);
}

// The static rectangle array on a block of the check (capacity 0-3), the guard sometimes cleared
void prepare_rects()
{
    LayerArray *a = at<LayerArray *>(VIEW_RECTS);
    const DWORD cap = random() % 4;
    a->vftable = 0x5f8b74;
    a->data = cap ? static_cast<void **>(pool_get_alloc(cap * 16)) : nullptr;
    a->count = random() % 2;
    a->capacity = cap;
    if (random() % 10 == 0)
        *at<BYTE *>(0x6c1be8) &= ~1;
}
void finish_rects()
{
    LayerArray *a = at<LayerArray *>(VIEW_RECTS);
    st(a->count);
    st(a->capacity);
    st(fnv(2166136261u, a->data, a->count * 16));
    st(*at<BYTE *>(0x6c1be8));
    st(g_atexit_calls);
    if (a->data)
        pool_get_free(a->data, a->capacity * 16);
    a->data = nullptr;
    a->capacity = 0;
}

DWORD views_case(DWORD &mask)
{
    init_fakes();
    init_world_fakes();
    Layer *l = fake_layer();
    g_atexit_calls = 0;
    *at<DWORD *>(0x6c2080) = ptrv(&g_fakes[1]);
    *at<DWORD *>(SOUND_LAYER) = random() % 2 ? ptrv(l) : 0;
    *at<BYTE *>(DRAW_DIRECT) = (BYTE)(random() % 2);
    DWORD r = 0;
    mask = 0;
    const DWORD op = random() % 4;
    switch (op) {
    case 0: {
        float pos[2] = {small_float(2000), random() % 6 ? small_float(2000) : random_float()};
        r = at<DWORD(THISCALL *)(Layer *, float *)>(0x4e9f70)(l, pos) & 0xff;
        mask = 0xff;
        st_f(pos[0]);
        st_f(pos[1]);
        break;
    }
    case 1:
        prepare_rects();
        at<void(THISCALL *)(Layer *, float)>(0x4ea220)(l, random() % 4 ? (float)(random() % 100) / 50.0f : random_float());
        finish_rects();
        break;
    case 2: {
        prepare_rects();
        int views[4][2];
        LayerArray va = {0x5f8dc4, reinterpret_cast<void **>(views), random() % 5, 4};
        for (auto &v : views) {
            v[0] = (int)(random() % 4000) - 500;
            v[1] = (int)(random() % 4000) - 500;
        }
        at<void(THISCALL *)(Layer *, LayerArray *, float)>(0x4ea350)(l, &va, random() % 4 ? (float)(random() % 100) / 50.0f : random_float());
        finish_rects();
        break;
    }
    default:
        at<void(THISCALL *)(Layer *, void *)>(0x4ea6e0)(l, random() % 3 ? static_cast<void *>(&g_fakes[8]) : nullptr);
        break;
    }
    st_fake_layer();
    for (int i = 4; i < 7; i++)
        st(fnv(2166136261u, reinterpret_cast<BYTE *>(&g_fakes[i]) + 0xc0, 8));
    restore_view_state();
    return r;
}

// ---- Multiplayer ----------------------------------------------------------------------------------------------

DWORD net_case(DWORD &mask)
{
    init_fakes();
    init_world_fakes();
    Layer *l = fake_layer();
    *at<DWORD *>(0x6c2080) = ptrv(&g_fakes[1]);
    *at<DWORD *>(SOUND_LAYER) = random() % 4 ? ptrv(l) : 0;
    *at<DWORD *>(NET_MODE) = random() % 6 ? 0 : 0x1234500 + random() % 2;
    // A chain of fake parts for the "at part" senders: 9 -> 10 -> 11
    g_fakes[9].up = random() % 2 ? &g_fakes[10] : nullptr;
    g_fakes[10].up = random() % 2 ? &g_fakes[11] : nullptr;
    const DWORD sounds[] = {0xffffffff, 0, 7, 0x123};
    const DWORD sound = sounds[random() % 4];
    float pos[2] = {small_float(2000), random() % 8 ? small_float(2000) : random_float()};
    const DWORD mode = random() % 4, keep = random() % 2;
    const DWORD channel = random() % 3 ? 0xff : random() % 8 + (random() % 2 ? 0x4400 : 0);
    DWORD r = 0;
    mask = 0;
    switch (random() % 7) {
    case 0: at<void(THISCALL *)(Layer *, DWORD, void *)>(0x4ea880)(l, sound, &g_fakes[9]); break;
    case 1: at<void(THISCALL *)(Layer *, DWORD, const float *)>(0x4ea8c0)(l, sound, pos); break;
    case 2:
        r = at<DWORD(THISCALL *)(Layer *, DWORD, void *, DWORD, DWORD, DWORD)>(0x4eaa60)(l, sound, &g_fakes[9], mode, keep, channel);
        mask = 0xff;
        break;
    case 3:
        r = at<DWORD(THISCALL *)(Layer *, DWORD, const float *, DWORD, DWORD, DWORD)>(0x4eaab0)(l, sound, pos, mode, keep, channel);
        mask = 0xff;
        break;
    case 4:
        at<void(THISCALL *)(Layer *, DWORD, const float *, DWORD, DWORD, DWORD)>(0x4eaca0)(l, sound, random() % 6 ? pos : nullptr, mode, keep,
                                                                                         channel);
        break;
    case 5: {
        for (int k = 0; k < 2; k++) {
            float p[2] = {random_float(), random_float()};
            if (random() % 2) {
                p[0] = small_float(4000);
                p[1] = small_float(4000);
            }
            r = at<DWORD(THISCALL *)(Layer *, void *, const float *)>(0x4eae90)(l, &g_fakes[8], random() % 6 ? p : nullptr);
        }
        break;
    }
    default: {
        float p[2] = {1.5f, 2.5f};
        BYTE ok = 0x77;
        r = tag(at<BYTE *(THISCALL *)(Layer *, void *, float *, BYTE *)>(0x4eaf80)(l, &g_fakes[8], p, random() % 4 ? &ok : nullptr));
        st_f(p[0]);
        st_f(p[1]);
        st(ok);
        break;
    }
    }
    st_fake_layer();
    restore_view_state();
    return r;
}

// ---- Dirty marks on the real queues ---------------------------------------------------------------------------

BYTE g_moving[2][512];
DWORD dirty_case(DWORD &mask)
{
    const DWORD queues[2] = {DRAW_QUEUE_GROUND, DRAW_QUEUE_OBJECTS};
    DWORD n[2];
    for (int q = 0; q < 2; q++) {
        n[q] = at<DWORD(THISCALL *)(void *)>(0x482de0)(at<void *>(queues[q]));
        for (DWORD i = 0; i < n[q] && i < 512; i++) {
            BYTE *e = at<BYTE *(THISCALL *)(void *, DWORD)>(0x566e40)(at<void *>(queues[q]), i);
            g_moving[q][i] = e[0x24];
            if (g_seed > 1)
                e[0x24] = (BYTE)(random() % 3 == 0);
        }
    }
    Patches patches;
    patches.add(0x480fc0, (void *)&rec_dirty);
    at<void(THISCALL *)(void *)>(0x4e8810)(&g_fakes[3]);
    patches.off();
    for (int q = 0; q < 2; q++)
        for (DWORD i = 0; i < n[q] && i < 512; i++)
            at<BYTE *(THISCALL *)(void *, DWORD)>(0x566e40)(at<void *>(queues[q]), i)[0x24] = g_moving[q][i];
    st(n[0]);
    st(n[1]);
    mask = 0;
    return 0;
}

// ---- Real data ------------------------------------------------------------------------------------------------

Layer *running_layer()
{
    BYTE *w = *at<BYTE **>(0x6c2080);
    if (!w)
        return nullptr;
    const LayerArray *layers = reinterpret_cast<const LayerArray *>(w + 0x2b8);
    if (!readable(layers, 0x10) || !layers->count || !readable(layers->data, 4))
        return nullptr;
    Layer *l = static_cast<Layer *>(layers->data[0]);
    return readable(l, 0x1d0) && *reinterpret_cast<DWORD *>(l) == VT_LAYER ? l : nullptr;
}

constexpr int NREAL = 64;
BYTE *g_real[NREAL];
int g_nreal;
DWORD real_index(const void *p)
{
    for (int i = 0; i < g_nreal; i++)
        if (p == g_real[i])
            return 0x9e000000u + (DWORD)i;
    return p ? 0xc0000000u | (*static_cast<const DWORD *>(p) & 0xffffff) : 0;
}

DWORD real_case(Layer *l, int which, DWORD &mask)
{
    mask = 0;
    if (which == 0) {   // SurfaceAt on a grid over the level (and outside)
        const int w = l->data.width, h = l->data.height;
        for (int y = -64; y < h + 64; y += 97)
            for (int x = -64; x < w + 64; x += 89) {
                const float f[2] = {(float)x + 0.5f, (float)y};
                DWORD b[2];
                memcpy(b, f, 8);
                st(at<DWORD(THISCALL *)(Layer *, DWORD, DWORD)>(0x4e9e60)(l, b[0], b[1]));
            }
        return 0;
    }
    BYTE *p = g_real[(which - 1) % g_nreal];
    st(real_index(at<void *(THISCALL *)(Layer *, void *, CClassInfo *, DWORD, DWORD)>(0x4eb1c0)(l, p, nullptr, 0x840000, 0x2000000)));
    float pos[2];
    at<void(THISCALL *)(void *, float *)>(0x4fb0a0)(p, pos);
    for (int k = 0; k < 8; k++) {
        const float angle = (float)k * 0.78539816f;
        st(real_index(at<void *(THISCALL *)(Layer *, void *, const float *, float, float, float, DWORD, DWORD)>(0x4eb200)(
            l, p, pos, angle, 20.0f + (float)k, 300.0f, 0x840000, 0x2000000)));
        for (int c = 0; c < 8; c++)
            st(at<DWORD *>(0x6c1cb0)[c]);
    }
    return 0;
}

}  // namespace

ZAX_CHECKS(world_update)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally queries{"world update: part queries and placing in fresh layers"}, entities{"world update: entity type fix"},
        bounce{"world update: bounce"}, views{"world update: shake, update, views, frame"},
        net{"world update: multiplayer sounds and positions"}, dirty{"world update: dirty marks"},
        real{"world update: surface, blockers, corridor (real data)"};
    save_bytes(g_generator, GAME_RANDOM, GAME_RANDOM_BYTES);
    save_view_state();
    for (unsigned seed = 1; seed <= (unsigned)scaled(150); seed++)
        two_sides(queries, "queries", seed, [](DWORD &mask) { return queries_case(mask); });
    queries.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++)
        two_sides(entities, "entities", seed, [](DWORD &mask) { return entity_case(mask); });
    entities.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(600); seed++)
        two_sides(bounce, "bounce", seed, [](DWORD &mask) { return bounce_case(mask); });
    bounce.report();
    {
        Patches patches;
        add_view_patches(patches);
        for (unsigned seed = 1; seed <= (unsigned)scaled(800); seed++)
            two_sides(views, "views", seed, [](DWORD &mask) { return views_case(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(1200); seed++)
            two_sides(net, "net", seed, [](DWORD &mask) { return net_case(mask); });
        patches.off();
    }
    restore_view_state();
    views.report();
    net.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(30); seed++)
        two_sides(dirty, "dirty", seed, [](DWORD &mask) { return dirty_case(mask); });
    dirty.report();
    if (Layer *l = running_layer()) {
        g_nreal = 0;
        LayerArray *list;
        at<void *(THISCALL *)(LayerArray **)>(0x59a470)(&list);
        at<DWORD(THISCALL *)(void *, LayerArray *, const int *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(l->data.grid, list, nullptr, 0, 0, 0, 0);
        for (DWORD i = 0; i < list->count && g_nreal < NREAL; i++)
            g_real[g_nreal++] = static_cast<BYTE *>(list->data[i]);
        at<DWORD(THISCALL *)(LayerArray **)>(0x59a510)(&list);
        log("  world update: running level %dx%d, %d parts", l->data.width, l->data.height, g_nreal);
        for (int which = 0; which <= g_nreal; which++)
            two_sides(real, "real", (unsigned)which + 1, [l, which](DWORD &mask) { return real_case(l, which, mask); });
    } else {
        log("  world update: no running layer found, real data skipped");
    }
    restore_bytes(g_generator);
    restore_view_state();
    real.report();
    log("  world update checks took %u ms", GetTickCount() - start);
}

}  // namespace zax
