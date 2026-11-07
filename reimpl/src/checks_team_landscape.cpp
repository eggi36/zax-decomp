// Checks of CTeamInfo, CLandscapeAI and the layer's block reader (team_landscape.cpp) against the original,
// in the game thread (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with 0-3
//    elements, the deleting destructors, both class getters, RegisterFields into a fake class info and
//    with the class variable 0 and the registered descriptors (real data) of CTeamInfo and CLandscapeAI.
//  - Behaviour: CLandscapeAI's slots 24 and 25 on AIs made by the original Create with their fields from a
//    seed (NaN, negative and zero rates, empty and odd rectangles, both shapes, all "Stop When" values),
//    its part a fake whose slot 43 gives the position; the layer, its update monitor (0x4e7330) and the
//    ground (CPlasmaTileMap 0x540420/0x540440/0x5405e0: a 24 × 24 grid of our own) are fakes. The block
//    reader 0x4e4380 with the packet functions (0x520550, 0x520470), World::CurrentLayer (0x4f1050) and
//    the layer's frame read (0x4ea6e0) as recorders; the local character 0x6bde9c saved and restored.
//    Compared: eax, the event log, the AI's bytes, the fake world and the arena.
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_team_landscape(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CTeamInfo", 0x14, 0x6c1ce0, 0x4e4460, 0x4e44b0, 0x4e4640, {0x4e4410, 0x4e45e0}, true},
    {"CLandscapeAI", 0x34, 0x6c1b10, 0x4e39c0, 0x4e3a00, 0x4e3b70, {0x4e3970, 0x4e3b10}, true},
};

using Create0 = void *(__cdecl *)();

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int GRID = 24;
struct World {
    DWORD part_vftable;
    BYTE part_body[0x44];
    BYTE *part_layer;          // +0x48
    float pos[2];
    BYTE layer[0x1d0];         // +0x3c the save data, +0x7c the ground, +0x1cc/+0x1cd set by the reader
    BYTE map[0x20];
    BYTE grid[GRID][GRID];
    BYTE packet[0x34];
    DWORD answers[16];
};
World g_world;
void *g_part_vtable[64];

DWORD ans(int k) { return g_world.answers[k]; }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
DWORD tag(const void *p)
{
    const DWORD a = ptrv(p), w = ptrv(&g_world);
    return a >= w && a < w + sizeof(World) ? 0xf0000000u + (a - w) : a;
}

DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_position(void *p, float *out)
{
    ev(0x12b, tag(p), out != nullptr);   // a stack buffer: the frames differ
    out[0] = g_world.pos[0];
    out[1] = g_world.pos[1];
    return result(0x2b);
}
DWORD THISCALL s_monitor(void *save_data, void *part) { ev(0x701, tag(save_data), tag(part)); return result(0x01); }
DWORD THISCALL s_cell(void *map, int v)
{
    ev(0x702, tag(map), (DWORD)v);
    if (ans(0) % 16 == 0)
        return (DWORD)v >> 6;   // unclamped: a wide rectangle wraps
    const int c = v < 0 ? 0 : v >> 6;
    return (DWORD)(c >= GRID ? GRID - 1 : c);
}
DWORD THISCALL s_elevation(void *map, DWORD col, DWORD row)
{
    ev(0x703, tag(map), col, row);
    return g_world.grid[row % GRID][col % GRID];
}
DWORD THISCALL s_add_elevation(void *map, DWORD col, DWORD row, int delta)
{
    ev(0x704, tag(map), col | row << 16, (DWORD)delta);
    BYTE &e = g_world.grid[row % GRID][col % GRID];
    e = (BYTE)(e + delta);
    return result(0x04);
}
DWORD THISCALL s_read_dword(void *p, DWORD *out)
{
    ev(0x705, tag(p), *out == ptrv(p));
    if (ans(1) % 4 == 0)
        return result(0);
    *out = ans(2) % 300;
    return result(1);
}
DWORD THISCALL s_skip(void *p, DWORD n) { ev(0x706, tag(p), n == ptrv(p) ? 0x5e7a1234u : n); return result(0x06); }
void *THISCALL s_current_layer(void *w)
{
    ev(0x707, w == *at<void **>(0x6c2080));
    return ans(3) % 3 ? g_world.layer : nullptr;
}
void THISCALL s_read_frame(void *layer, void *p) { ev(0x708, tag(layer), tag(p)); }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x4e7330, (void *)&s_monitor},    {0x540420, (void *)&s_cell},  {0x540440, (void *)&s_elevation},      {0x5405e0, (void *)&s_add_elevation},
    {0x520550, (void *)&s_read_dword}, {0x520470, (void *)&s_skip},  {0x4f1050, (void *)&s_current_layer}, {0x4ea6e0, (void *)&s_read_frame},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

void init_world_tables()
{
    for (int i = 0; i < 64; i++)
        g_part_vtable[i] = (void *)&f_bad;
    g_part_vtable[43] = (void *)&f_position;
}

DWORD float_from(DWORD k)
{
    static const float F[] = {0.0f, 1.0f, 40.0f, 2.5f, -3.0f, 0.016f, 1000.0f, 1e-7f, 7.75f, 255.0f};
    if (k % 13 == 0)
        return 0x7fc00000;   // NaN
    if (k % 13 == 1)
        return 0x7f800000;   // infinity
    return float_bits(F[k % 10]);
}

void setup(unsigned seed)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    w.part_vftable = ptrv(g_part_vtable);
    for (DWORD k = 0; k < sizeof w.part_body; k++)
        w.part_body[k] = (BYTE)random();
    w.part_layer = w.layer;
    for (DWORD k = 0; k < sizeof w.layer; k++)
        w.layer[k] = (BYTE)random();
    *reinterpret_cast<DWORD *>(w.layer + 0x7c) = ptrv(w.map);
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            w.grid[r][c] = (BYTE)(rnd(4) ? rnd(256) : 0x80 + rnd(3));
    w.pos[0] = (float)(int)rnd(GRID * 64 + 200) - 100.0f + (rnd(2) ? 0.5f : 0.0f);
    w.pos[1] = (float)(int)rnd(GRID * 64 + 200) - 100.0f + (rnd(3) ? 0.25f : 0.0f);
    for (int k = 0; k < 16; k++)
        w.answers[k] = random();
}

// The AI's fields from the seed
void fill_ai(BYTE *o)
{
    auto D = [&](DWORD off) -> DWORD & { return *reinterpret_cast<DWORD *>(o + off); };
    static const DWORD SIZES[] = {0, 1, 2, 63, 64, 100, 128, 255, 400, 1000};
    D(0x10) = SIZES[rnd(10)];
    D(0x14) = rnd(6) ? SIZES[rnd(10)] : 0xfffffff0u + rnd(16);
    D(0x18) = rnd(5) ? float_from(rnd(130)) : float_bits(-1.0f);
    D(0x1c) = rnd(3) ? 0x80 + rnd(3) : rnd(256);
    D(0x20) = rnd(3) ? 0 : float_from(rnd(130));
    D(0x24) = rnd(3) ? 0 : rnd(3);
    D(0x28) = rnd(4);
    D(0x2c) = rnd(4) ? 0xff : rnd(256);
    D(0x30) = rnd(4) ? 0 : rnd(256);
}

struct Case {
    const char *name;
    DWORD fn;
};
const Case CASES[] = {
    {"CLandscapeAI::Begin", 0x4e4040},
    {"CLandscapeAI::Update", 0x4e4060},
    {"CLayerBlockReader::Read", 0x4e4380},
};
constexpr int NCASES = sizeof CASES / sizeof CASES[0];

World g_after[2];
BYTE g_ai_after[2][0x34];
DWORD g_local_after[2];

DWORD run_case(int k, int side)
{
    World &w = g_world;
    BYTE *ai = k < 2 ? static_cast<BYTE *>(original<Create0>(0x4e39c0)()) : nullptr;
    DWORD r = 0;
    if (ai)
        fill_ai(ai);
    switch (k) {
    case 0: r = at<DWORD(THISCALL *)(void *, void *)>(CASES[k].fn)(ai, &w.part_vftable); break;
    case 1: {
        const DWORD dt = rnd(4) ? float_bits((float)rnd(100) / 64.0f) : float_from(rnd(130));
        at<void(THISCALL *)(void *, void *, DWORD)>(CASES[k].fn)(ai, &w.part_vftable, dt);
        break;
    }
    case 2: r = tag(reinterpret_cast<void *>(static_cast<uintptr_t>(
                at<DWORD(THISCALL *)(void *, void *)>(CASES[k].fn)(at<void *>(0x6c1cd0), w.packet))));
        break;
    }
    if (ai) {
        memcpy(g_ai_after[side], ai, 0x34);
        vcall<void *>(ai, 1, (DWORD)1);
    } else
        memset(g_ai_after[side], 0, 0x34);
    return r;
}

void test_case(Tally &t, int k, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup(seed);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
        const DWORD local = *at<DWORD *>(0x6bde9c);
        arena_on(side, 0x3c);
        g_seed = seed * 31u + 7u;
        r[side] = run_case(k, side);
        arena_off();
        g_local_after[side] = *at<DWORD *>(0x6bde9c);
        *at<DWORD *>(0x6bde9c) = local;
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_stand_patches[i]);
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    const bool same = r[0] == r[1] && same_log() && g_local_after[0] == g_local_after[1] && g_used[0] == g_used[1] &&
                      same_region(g_ai_after[0], g_ai_after[1], 0x34) &&
                      memcmp(&g_after[0], &g_after[1], sizeof(World)) == 0 && same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    int at_ai = -1;
    for (int i = 0; i < 0x34; i++)
        if (g_ai_after[0][i] != g_ai_after[1][i]) {
            at_ai = i;
            break;
        }
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, local %x / %x, AI differs at %d, heap %u / %u]", seed, r[0], r[1], g_nlog[0] / 4,
              g_nlog[1] / 4, g_local_after[0], g_local_after[1], at_ai, g_used[0], g_used[1]);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, CASES[k].name, d);
}

}  // namespace

ZAX_CHECKS(team_landscape)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_team_landscape;
    if (!arena_setup()) {
        log("Check team landscape: no arena");
        return;
    }
    init_world_tables();
    Tally objects{"team landscape: Create, CreateArray, destructors"}, getters{"team landscape: class getters"},
        fields{"team landscape: RegisterFields"}, registered{"team landscape: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"team landscape: CLandscapeAI's begin and update, the layer's block reader"};
    for (int k = 0; k < NCASES; k++) {
        log("  %s", CASES[k].name);   // the last line before a case that ends the game
        for (unsigned s = 0; s < (unsigned)scaled(k == 1 ? 1500 : 300); s++)
            test_case(behaviour, k, 5000u * (unsigned)k + s);
    }
    behaviour.report();
}

}  // namespace zax
