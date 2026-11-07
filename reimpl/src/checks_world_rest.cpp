// Checks of the rest of the world class and the small World functions (world_rest.cpp) against the
// original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with the hooks of world_rest taken out (nested calls run
// the original too), side 1 through the reimplementation. Compared: eax (masked where only al is defined),
// st(0) where a function returns a float, an event log of every call into fakes and stand-ins, and the
// state of all fakes and globals afterwards (pointers by number).
//
// Fakes: a world (CZaxWorld-sized, a vtable of recorder thunks), the net layer with player records, the
// game type with its net players, layers (tile maps, ambient sounds), parts, generic objects (frames, the
// error reporter, packets, models, draw queue entries), definition lists swapped into the exe's global
// lists for the duration of a case ("Sequences", "Animated Textures", "Death Behaviors", "Game Types");
// each vtable slot is a recorder thunk ("mov eax, id; jmp" into one stub that logs the arguments, writes
// deterministic outputs and returns a value derived from the call). Every exe function the code calls
// outside the module jumps to such a recorder too (layer functions, net packets, part helpers, file search,
// the sweeps, ...); string, path and class helpers run for real.
//  - The world functions on random worlds, the list loaders on fake lists, the layer's point query and
//    sound layer, SetLayer on part trees, the collide checker's slots on fake movers, the vector helpers
//    and class getters.
//  - Real data (the running game): the current level path, the Properties.txt paths of the loaded
//    sequences, the collide checker's slots for player 0's entity between points around it (the sweeps
//    do not notify), the list loaders on the game's folders (fresh lists compared with the global ones).
#include "check_util.h"
#include "definitions.h"
#include "file_search.h"
#include "polygon.h"
#include "world_rest.h"
#include "x87.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_world_rest(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
void *vptr(DWORD v) { return reinterpret_cast<void *>(static_cast<uintptr_t>(v)); }
using LD = long double;

// ---- Sides, log and state ------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
bool g_trace = false;   // true: every case logged before it runs (the last line names a case that ends the game)

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
constexpr int SLOG = 8192;
DWORD g_slog[2][SLOG];
int g_nslog[2];
void st(DWORD v)
{
    g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4);
    if (g_nslog[g_side] < SLOG)
        g_slog[g_side][g_nslog[g_side]] = v;
    g_nslog[g_side]++;
}
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

// The fixed version of checks_geometry.cpp: range checked, no wrap near 0xffffffff
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
        if (!VirtualQuery(vptr(a), &mi, sizeof mi) || mi.state != 0x1000 || (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}
// ---- The module's hooks out and in ---------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    Hook list[MAX_SITES];
    const int n = hooks_world_rest(nullptr);
    if (!hook_sites_fit("world_rest", n, MAX_SITES))
        return;
    hooks_world_rest(list);
    for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (site[0] != 0xe9)
            continue;   // not installed (Disable=, or bytes did not match)
        if (list[i].address + 5 + *reinterpret_cast<INT32 *>(site + 1) != ptrv(list[i].replacement))
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

// ---- Fakes ------------------------------------------------------------------------------------------------------

constexpr int T_WORLD = 0, T_PLAYERS = 1, T_GT = 2, T_OBJ = 3, T_PART = 4, T_LAYER = 5, T_LIST = 6, T_FN = 7;
constexpr int SLOTS = 128;
constexpr int NREC = 6, NOBJ = 24, NLAYER = 4, NPART = 12, NSOUND = 6, NENTRY = 8;
constexpr DWORD PART_SIZE = 0x140;   // the multiplayer frame writes +0x128 of the characters

alignas(16) BYTE g_world[0x600];
alignas(16) BYTE g_players[0x40];
alignas(16) BYTE g_rec[NREC][0x100];
alignas(16) BYTE g_gt[0x40];
alignas(16) BYTE g_np[NREC][0x40];     // the game type's net players (+0x34 a counter)
void *g_np_ptrs[NREC];
alignas(16) BYTE g_obj[NOBJ][0x80];
alignas(16) BYTE g_layer[NLAYER][0x200];
alignas(16) BYTE g_tilemap[2][0x2d10];
alignas(16) BYTE g_part[NPART][PART_SIZE];
alignas(16) BYTE g_sound[NSOUND][0x20];
void *g_sound_ptrs[2][NSOUND];
alignas(16) BYTE g_model[0x100];
alignas(16) BYTE g_def[0x40];          // the model's default entity
alignas(16) BYTE g_childlist[3][0x20]; // child lists: +0xc data, +0x10 count
void *g_child_items[3][4];
struct FakeArray {
    LayerArray a;
    DWORD guard[4];
    void *items[32];
};
FakeArray g_wlayers;        // the world's layer list
FakeArray g_chars;          // the world's characters
FakeArray g_temp;           // the temporary part list (0x59a470)
alignas(16) BYTE g_list[0x140];   // the definition list swapped into a global (or a fresh list of the loaders)
ZDefinitionRecord g_entries[NENTRY + 1];
alignas(16) BYTE g_registry[0x20];
DWORD g_registry_entries[4][4];
alignas(16) BYTE g_table[0x60];   // CZaxWorld +0x33c, the swapped layer file name table

DWORD g_vt[8][SLOTS];
DWORD g_pop[8][SLOTS];

extern "C" {
extern char wr_thunks[];
DWORD wr_pop, wr_ret_float;
float wr_fret;
DWORD __cdecl wr_record(DWORD *args, DWORD id, BYTE *self, DWORD edx);
}
__asm__(".text\n"
        ".globl _wr_thunks\n"
        "_wr_thunks:\n"
        ".set wr_i, 0\n"
        ".rept 1150\n"
        "  movl $wr_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _wr_common - . - 4\n"
        ".set wr_i, wr_i + 1\n"
        ".endr\n"
        "_wr_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 16(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _wr_record\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _wr_ret_float\n"
        "  je 1f\n"
        "  flds _wr_fret\n"
        "1:\n"
        "  movl _wr_pop, %ecx\n"
        "  popl %edx\n"
        "  addl %ecx, %esp\n"
        "  jmp *%edx\n");
DWORD thunk(DWORD id) { return ptrv(wr_thunks) + 10 * id; }
bool thunks_ok()
{
    for (DWORD k = 0; k < 1150; k++) {
        const BYTE *th = reinterpret_cast<const BYTE *>(wr_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != k || th[5] != 0xe9)
            return false;
    }
    return true;
}

// The exe functions replaced by recorders during the cases: address, stack bytes popped
struct Fn {
    DWORD address;
    BYTE pop;
};
const Fn FNS[] = {
    // world, layers and views
    {0x4f5dd0, 0x10}, {0x4f6770, 8}, {0x4f2800, 0}, {0x4aa550, 8}, {0x4e9f70, 4}, {0x4ea660, 8}, {0x4ee760, 0xc}, {0x4e9d80, 0},
    {0x4ea6e0, 4}, {0x53f010, 0x10}, {0x4ed7f0, 4}, {0x4ed6e0, 0x1c}, {0x4ed8f0, 8}, {0x4eb670, 0}, {0x506f70, 0}, {0x4f3d50, 0},
    {0x579030, 0}, {0x416fe0, 4}, {0x416af0, 4}, {0x4a2650, 0}, {0x4a2670, 0}, {0x4ce4f0, 8}, {0x4ebcb0, 4}, {0x4eb6f0, 4},
    {0x4a35d0, 0}, {0x4ebd60, 4}, {0x59a470, 0}, {0x59a510, 0}, {0x57a770, 0x18}, {0x536080, 0xc}, {0x4f4e70, 0x10}, {0x4a3760, 0},
    // net
    {0x520f50, 0}, {0x5200d0, 0}, {0x520750, 0}, {0x4f7950, 8}, {0x5ac330, 4},
    // parts, entities, models
    {0x416790, 4}, {0x417660, 0xc}, {0x4fbba0, 0}, {0x5992b0, 0}, {0x599290, 0}, {0x48b290, 0}, {0x5174f0, 4}, {0x4fbae0, 8},
    {0x4fbd90, 4}, {0x416760, 8}, {0x4fc7e0, 4}, {0x4f9b70, 4}, {0x4fc710, 4}, {0x4fbc50, 4}, {0x5174a0, 0}, {0x4fbf20, 0},
    {0x5176f0, 0x14}, {0x591d00, 4}, {0x48e920, 0}, {0x48b6b0, 4}, {0x490060, 8}, {0x4eaca0, 0x14}, {0x4cc7a0, 0}, {0x4fc8a0, 4},
    {0x58a030, 0}, {0x58ade0, 0}, {0x41f660, 0}, {0x5bb780, 4}, {0x55e090, 4},
    // draw queues, sounds, sweeps
    {0x59a2d0, 0}, {0x49ccb0, 0}, {0x49ca40, 0}, {0x566e40, 4}, {0x4fb0a0, 4}, {0x57b290, 8}, {0x4e8d40, 0x20}, {0x4e8e00, 0x24},
    {0x4eb050, 0x14}, {0x415780, 0x24},
    // the list loaders
    {0x4a34d0, 0x10}, {0x523e20, 4}, {0x4a12d0, 0}, {0x4a1150, 0x10}, {0x591cd0, 0x14}, {0x47e560, 4},
    // the game data (0x4f13a0): the other lists, cursors, textures, sounds, the display
    {0x4cdf70, 4}, {0x51ec90, 4}, {0x4d7c40, 0xc}, {0x56b950, 0xc}, {0x518590, 4}, {0x4e27a0, 4}, {0x433a80, 0}, {0x4aa390, 4},
    {0x4d7330, 0}, {0x4d6e10, 0}, {0x523df0, 8}, {0x4784e0, 0}, {0x477220, 0}, {0x46a940, 0}, {0x591fc0, 8}, {0x4b1cc0, 4},
    {0x4b1db0, 4}, {0x56c300, 4}, {0x4b4220, 4}, {0x4d7920, 0xc}, {0x4a4670, 8}, {0x57e2f0, 0x10}, {0x4a3800, 8}, {0x4b1d80, 0},
    {0x4164c0, 8}, {0x51e3e0, 8}, {0x51e520, 4}, {0x578eb0, 0}, {0x5694b0, 0}, {0x568660, 0x1c}, {0x4b1d20, 0}, {0x48d8f0, 4},
    // the module's own list loaders, recorded only while the game data is checked
    {0x4f6ba0, 0xc}, {0x4f6ea0, 0xc}, {0x4f71a0, 0xc}, {0x4f74a0, 0xc},
};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
static_assert(T_FN * SLOTS + NFN <= 1150);
int fn_index(DWORD address)
{
    for (int i = 0; i < NFN; i++)
        if (FNS[i].address == address)
            return i;
    return -1;
}

bool g_record_loaders;   // the game data case: the module's list loaders are recorded too
bool is_loader(DWORD a) { return a == 0x4f6ba0 || a == 0x4f6ea0 || a == 0x4f71a0 || a == 0x4f74a0; }
struct Patches {
    CodePatch p[NFN + 4];
    int n = 0;
    void on(DWORD skip1 = 0, DWORD skip2 = 0)
    {
        for (int i = 0; i < NFN; i++)
            if (FNS[i].address != skip1 && FNS[i].address != skip2 && (g_record_loaders || !is_loader(FNS[i].address)))
                patch_jump(p[n++], FNS[i].address, vptr(thunk(T_FN * SLOTS + i)));
        patch_jump(p[n++], 0x5d0863, (void *)&fake_atexit);
    }
    void off()
    {
        while (n > 0)
            unpatch(p[--n]);
    }
} g_patches;

// Stand-in allocations (the pool blocks of the constructors replaced by recorders, the loaders' records)
struct Block {
    void *p;
    DWORD size;
    bool heap;
};
Block g_blocks[32];
int g_nblocks;
void note_block(void *p, DWORD size, bool heap = false)
{
    if (g_nblocks < 32)
        g_blocks[g_nblocks++] = {p, size, heap};
}

DWORD tag(DWORD v)
{
    if (!v)
        return 0;
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    if (in(g_world, sizeof g_world))
        return 0xa0000000u + (v - ptrv(g_world));
    if (in(g_players, sizeof g_players))
        return 0xa1000000u + (v - ptrv(g_players));
    for (int i = 0; i < NREC; i++)
        if (in(g_rec[i], 0x100))
            return 0xa2000000u + ((DWORD)i << 12) + (v - ptrv(g_rec[i]));
    if (in(g_gt, sizeof g_gt))
        return 0xa3000000u + (v - ptrv(g_gt));
    for (int i = 0; i < NOBJ; i++)
        if (in(g_obj[i], 0x80))
            return 0xa4000000u + ((DWORD)i << 12) + (v - ptrv(g_obj[i]));
    for (int i = 0; i < NLAYER; i++)
        if (in(g_layer[i], 0x200))
            return 0xa5000000u + ((DWORD)i << 12) + (v - ptrv(g_layer[i]));
    for (int i = 0; i < NPART; i++)
        if (in(g_part[i], PART_SIZE))
            return 0xa6000000u + ((DWORD)i << 12) + (v - ptrv(g_part[i]));
    for (int i = 0; i < NREC; i++)
        if (in(g_np[i], 0x40))
            return 0xa7000000u + ((DWORD)i << 12) + (v - ptrv(g_np[i]));
    for (int i = 0; i < 2; i++)
        if (in(g_tilemap[i], sizeof g_tilemap[i]))
            return 0xa8000000u + ((DWORD)i << 16) + (v - ptrv(g_tilemap[i]));
    for (int i = 0; i < NSOUND; i++)
        if (in(g_sound[i], 0x20))
            return 0xa9000000u + ((DWORD)i << 12) + (v - ptrv(g_sound[i]));
    if (in(g_model, sizeof g_model))
        return 0xaa000000u + (v - ptrv(g_model));
    if (in(g_def, sizeof g_def))
        return 0xab000000u + (v - ptrv(g_def));
    for (int i = 0; i < 3; i++)
        if (in(g_childlist[i], 0x20))
            return 0xac000000u + ((DWORD)i << 12) + (v - ptrv(g_childlist[i]));
    if (in(&g_wlayers, sizeof g_wlayers))
        return 0xad000000u + (v - ptrv(&g_wlayers));
    if (in(&g_chars, sizeof g_chars))
        return 0xae000000u + (v - ptrv(&g_chars));
    if (in(&g_temp, sizeof g_temp))
        return 0xaf000000u + (v - ptrv(&g_temp));
    if (in(g_list, sizeof g_list))
        return 0xb1000000u + (v - ptrv(g_list));
    if (in(g_entries, sizeof g_entries))
        return 0xb2000000u + (v - ptrv(g_entries));
    if (in(g_registry, sizeof g_registry))
        return 0xb3000000u + (v - ptrv(g_registry));
    if (in(g_table, sizeof g_table))
        return 0xb4000000u + (v - ptrv(g_table));
    for (int i = 0; i < g_nblocks; i++)
        if (in(g_blocks[i].p, g_blocks[i].size))
            return 0xb8000000u + ((DWORD)i << 16) + (v - ptrv(g_blocks[i].p));
    BYTE probe = 0;
    if (v <= ptrv(&probe) + 0x10000 && v + 0x10000 > ptrv(&probe))
        return 0xf4000000u;
    return v;
}
DWORD tagp(const void *p) { return tag(ptrv(p)); }

DWORD rec_hash(DWORD a, DWORD b, DWORD c)
{
    DWORD h = fnv(fnv(fnv(2166136261u, &a, 4), &b, 4), &c, 4);
    return h ^ (h >> 13);
}
DWORD text_hash(const void *p)
{
    if (!p || !readable(p, 1))
        return 0xdead;
    const char *s = static_cast<const char *>(p);
    DWORD n = 0;
    while (n < 300 && readable(s + n, 1) && s[n])
        n++;
    return fnv(2166136261u, s, n);
}

// ---- Case configuration (set per case from the seed, the same on both sides) ------------------------------------

DWORD g_calls;
BYTE g_mp;
DWORD g_nplayers;
int g_can_read[8];
DWORD g_cfg[16];
const char *const NAMES[] = {"UnNamed", "add to way points", "Door", "", "unnamed", "ADD TO WAY POINTS", "Crate 3"};
char g_name_buf[NPART][24];

float pick_float(DWORD r)
{
    constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x40000000, 0x3fc00000, 0x40200000, 0x42c80000,
                                0xc2c80000, 0x447a0000, 0x7f800000, 0xff800000, 0xffc00000, 0x00000001, 0x4b000000, 0x3c23d70a, 0x3e4ccccd,
                                0x3f733333, 0x3f8ccccd, 0x3f866666, 0x3f59999a, 0x3f9d70a4};
    DWORD b = VALUES[r % (sizeof VALUES / 4)];
    if (r % 7 == 3) {
        const float f = (float)((int)(r >> 8) % 4000 - 1000) / 8.0f;
        memcpy(&b, &f, 4);
    }
    float f;
    memcpy(&f, &b, 4);
    return f;
}
// A zoom around the game's limits (0x618034/0x618038) and 1
float pick_zoom(DWORD r)
{
    const float lo = *at<const float *>(ZOOM_MIN), hi = *at<const float *>(ZOOM_MAX);
    switch (r % 6) {
    case 0: return lo + (float)(r >> 8 & 7) * 0.005f - 0.01f;
    case 1: return hi + (float)(r >> 8 & 7) * 0.005f - 0.02f;
    case 2: return 1.0f + (float)((int)(r >> 8 & 31) - 16) * 0.004f;
    case 3: return 1.0f;
    case 4: return pick_float(r >> 3);
    default: return lo + (hi - lo) * (float)(r >> 8 & 255) / 255.0f;
    }
}

// ---- The recorder ------------------------------------------------------------------------------------------------

int obj_index(const BYTE *p, const void *base, DWORD stride, int n)
{
    for (int i = 0; i < n; i++)
        if (p == static_cast<const BYTE *>(base) + i * stride)
            return i;
    return -1;
}
void fill_temp(DWORD r, DWORD max)
{
    g_temp.a.count = 0;
    const DWORD n = r % (max + 1);
    for (DWORD i = 0; i < n; i++)
        g_temp.a.data[g_temp.a.count++] = g_part[rec_hash(r, i, 7) % NPART];
}
// The bytes of a sweep its constructor writes (+0x1e on stays as it was: stack garbage)
DWORD sweep_hash(const void *p) { return p ? fnv(2166136261u, p, 0x1e) : 0; }

extern "C" DWORD __cdecl wr_record(DWORD *args, DWORD id, BYTE *self, DWORD edx)
{
    const DWORD type = id / SLOTS, slot = id % SLOTS;
    if (g_trace)
        log("        rec %x", id);
    wr_ret_float = 0;
    wr_pop = type == T_FN ? FNS[slot].pop : g_pop[type][slot];
    g_calls++;
    const DWORD me = tagp(self);
    const DWORD r = rec_hash(id, me, g_calls);
    const DWORD code = 0x57000000u + id;
    auto arg_text = [&](int i) { return text_hash(vptr(args[i])); };
    auto log_args = [&](DWORD strmask = 0) {
        const DWORD n = wr_pop / 4;
        DWORD h = 2166136261u;
        for (DWORD i = 0; i < n && i < 9; i++) {
            const DWORD v = (strmask >> i & 1) ? arg_text((int)i) : tag(args[i]);
            h = fnv(h, &v, 4);
        }
        ev(code, me, h, n);
    };
    auto boolean = [&](DWORD cfg) { return (g_cfg[cfg] & 3) == 0 ? (r % 3 ? (r | 1) : (r & ~0xffu)) : (g_cfg[cfg] & 3) == 1 ? 1u : 0u; };
    if (type == T_FN) {
        const DWORD a = FNS[slot].address;
        switch (a) {
        // constructors of objects made from pool blocks: a fake vtable, the block noted for freeing
        case 0x599290:
        case 0x48e920:
        case 0x4cc7a0: {
            const DWORD size = a == 0x599290 ? 0x78 : a == 0x48e920 ? 0xe4 : 0x40;
            ev(code, g_nblocks);
            memset(self, 0, size);
            *reinterpret_cast<DWORD *>(self) = ptrv(g_vt[T_PART]);
            note_block(self, size);
            return ptrv(self);
        }
        case 0x415780:   // ClassInfo_Register: the class info block noted, the variable set
            log_args(1u << 1);
            *reinterpret_cast<DWORD *>(args[0]) = ptrv(self);
            note_block(self, 0x48);
            return ptrv(self);
        case 0x4a2650:   // DebugLog_SectionBegin (fastcall)
            ev(code, text_hash(self));
            return r;
        case 0x416790:   // Object_IsKindOf
            log_args();
            return boolean(1);
        case 0x4e9f70: {   // CLayer::ApplyShake: moves the position
            float *pos = reinterpret_cast<float *>(args[0]);
            ev(code, me, bits(pos[0]), bits(pos[1]));
            if (r % 3) {
                pos[0] = pos[0] + 1.5f;
                pos[1] = (float)(r % 100) - 50.0f;
            }
            return boolean(2);
        }
        case 0x4ea660: {   // ViewRectAt: a rectangle
            const DWORD *pos = reinterpret_cast<DWORD *>(args[0]);
            ev(code, me, pos[0], pos[1]);
            int *rect = reinterpret_cast<int *>(args[1]);
            for (int k = 0; k < 4; k++)
                rect[k] = (int)(rec_hash(r, k, 3) % 3000) - 500;
            return r;
        }
        case 0x4ee760: {
            const int *rect = reinterpret_cast<int *>(args[2]);
            ev(code, me, tag(args[0]) ^ args[1] * 7, fnv(2166136261u, rect, 16));
            return r;
        }
        case 0x520f50: {   // NetPacket_CanRead
            const int i = obj_index(self - 0x3c, g_rec, 0x100, NREC);
            ev(code, me);
            if (i >= 0 && g_can_read[i] > 0) {
                g_can_read[i]--;
                return 1;
            }
            return 0x100;
        }
        case 0x4e9d80:
        case 0x4f3d50:
            ev(code);
            return a == 0x4f3d50 ? ptrv(g_obj[10]) : boolean(3);
        case 0x4ed7f0: {   // ClampView
            float *pos = reinterpret_cast<float *>(args[0]);
            ev(code, me, bits(pos[0]), bits(pos[1]));
            pos[0] = pos[0] < 0 ? 0 : pos[0];
            return r;
        }
        case 0x4ed6e0: {   // ScrollToward: floats by their bits
            const float *pos = reinterpret_cast<float *>(args[0]);
            ev(code, me, bits(pos[0]) ^ args[1], bits(pos[1]) ^ args[2]);
            ev(code, args[3], args[4], args[5] ^ args[6] * 3);
            return r;
        }
        case 0x4ed8f0:
            ev(code, me, args[0], args[1]);
            return r;
        case 0x4aa550:
            ev(code, me, args[0], args[1]);
            return r;
        case 0x4eb670:   // CLayer::FileName
            ev(code, me);
            return ptrv("Test/Nachbau1");
        case 0x4a35d0:   // ResourceLists_Get: the fake registry
            ev(code);
            return ptrv(g_registry);
        case 0x416fe0:
        case 0x416af0:
            log_args();
            return r % 4 ? (r | 1) : (r & ~0xffu);
        case 0x59a470:   // PartList_GetTemp
            ev(code, tagp(self));
            *reinterpret_cast<void **>(self) = &g_temp.a;
            g_temp.a.count = 0;
            return r;
        case 0x57a770: {   // CSortList2D_FindRect: some parts
            const DWORD rh = args[1] ? fnv(2166136261u, vptr(args[1]), 16) : 0;
            ev(code, me, tag(args[0]), rh);
            ev(code, args[2], args[3] ^ (args[4] << 1), args[5]);
            fill_temp(r, g_cfg[4] % 8);
            return r;
        }
        case 0x536080:
            log_args(1u << 0);
            return 0;
        case 0x4fbf20: {   // LayerPart_Name: the part's name, or null
            ev(code, me);
            const int i = obj_index(self, g_part, PART_SIZE, NPART);
            if (i < 0 || g_cfg[5] % 5 == 0)
                return 0;
            return ptrv(g_name_buf[i]);
        }
        case 0x417660:
            log_args(3);
            return r;
        case 0x48b290:   // the entity's model
            ev(code, me);
            return g_cfg[6] % 6 ? ptrv(g_model) : 0;
        case 0x5174f0:
            log_args();
            return r % 3 ? ptrv(g_obj[11 + r % 3]) : 0;
        case 0x4fbae0:
            log_args();
            return r % 3 == 0 ? 0 : r % 3 == 1 ? ptrv(g_obj[11 + r % 3]) : ptrv(g_obj[15]);
        case 0x416760:   // Object_Clone: a child list's clone is a child list
            log_args();
            if (obj_index(self, g_childlist, 0x20, 3) >= 0)
                return ptrv(g_childlist[r % 3]);
            return r % 4 ? ptrv(g_part[r % NPART]) : 0;
        case 0x4fc7e0:   // CLayerPart_SetChildList
            log_args();
            *reinterpret_cast<DWORD *>(self + 8) = args[0];
            return r;
        case 0x5174a0:
            ev(code, me);
            return ptrv(g_cfg[7] % 3 ? "Crater" : "Rogcoil");
        case 0x5176f0:   // Entity_CreateFromModel
            log_args();
            return g_cfg[8] % 5 ? ptrv(g_part[g_cfg[8] % NPART]) : 0;
        case 0x591d00:
            log_args();
            return ptrv("Effects/Explosion");
        case 0x490060:
            log_args(1);
            return r;
        case 0x4eaca0: {
            const DWORD *pos = reinterpret_cast<DWORD *>(args[1]);
            ev(code, me, tag(args[0]), pos ? pos[0] ^ pos[1] * 3 : 0);
            ev(code, args[2], args[3], args[4]);
            return r;
        }
        case 0x566e40: {   // the draw queue's entry k: an object whose +0x10 is a part
            ev(code, me, args[0]);
            BYTE *e = g_obj[16 + args[0] % 4];
            *reinterpret_cast<void **>(e + 0x10) = g_part[(args[0] + g_cfg[9]) % NPART];
            return ptrv(e);
        }
        case 0x4fb0a0: {   // LayerPart_Position
            log_args();
            float *out = reinterpret_cast<float *>(args[0]);
            out[0] = pick_float(r);
            out[1] = pick_float(r >> 5);
            return r;
        }
        case 0x57b290: {
            const DWORD *v = reinterpret_cast<DWORD *>(args[0]);
            ev(code, me, fnv(2166136261u, v, 12), args[1]);
            return r;
        }
        case 0x4e8d40:   // CLayer_SweepPoint (offset, sweep, hit, out, any, none, mover, filter)
        case 0x4e8e00: { // CLayer_Sweep (polygon, offset, sweep, hit, out, mover, any, none, filter)
            const int o = a == 0x4e8e00 ? 1 : 0;
            const DWORD *off = reinterpret_cast<DWORD *>(args[o]);
            ev(code, me, off ? off[0] ^ off[1] * 3 : 0, sweep_hash(vptr(args[o + 1])));
            DWORD h = 2166136261u;
            for (int k = o + 2; k < o + 8; k++) {
                const DWORD v = tag(args[k]);
                h = fnv(h, &v, 4);
            }
            ev(code, h, a == 0x4e8e00 ? tag(args[0]) : 0);
            return boolean(10);
        }
        case 0x4eb050:   // FindBlocker
            log_args();
            return r % 3 ? ptrv(g_part[r % NPART]) : 0;
        case 0x55e090:
        case 0x4b1cc0:
        case 0x4b1db0:
        case 0x56c300:
        case 0x4b4220:
        case 0x4b1d80:
        case 0x4b1d20:
            log_args();
            return r;
        case 0x523df0:   // ResourceList_Find: no "Red Flag" or "Blue Flag" (the game would end)
            ev(code, me, arg_text(0), args[1]);
            return 0xffffffff;
        case 0x591fc0:   // NamedList_FindOrFail: an index
            ev(code, me, arg_text(0), args[1]);
            return r % 20;
        case 0x4d7c40:
        case 0x4d7920:
        case 0x56b950:
        case 0x4f6ba0:
        case 0x4f6ea0:
        case 0x4f71a0:
        case 0x4f74a0:
            ev(code, me, arg_text(0), arg_text(1) ^ args[2]);
            return r % 7;
        case 0x57e2f0:
            log_args(0xf);
            return r;
        case 0x4a4670:   // Ini_GetInt (fastcall file, section; name, default)
            ev(code, ptrv(self), edx, arg_text(0) ^ args[1]);
            return r % 3 == 0 ? 0 : 1;
        case 0x4164c0:   // Object_CreateFromFile (fastcall path, flags; write back, out): the rules
            ev(code, self ? text_hash(reinterpret_cast<ZString *>(self)->buffer ? reinterpret_cast<ZString *>(self)->c_str() : nullptr) : 0, edx,
               args[0] ^ tag(args[1]));
            return ptrv(g_obj[5]);
        case 0x51ec90:   // the game type of the net layer
            log_args();
            return ptrv(g_gt);
        case 0x433a80:
            ev(code);
            return ptrv(g_obj[19]);
        case 0x48d8f0:
            log_args();
            return r % 3 ? ptrv(g_obj[16 + r % 3]) : 0;
        // the list loaders
        case 0x4a34d0:
            log_args(1);
            return r % 50;
        case 0x4a12d0: {   // Files_Count (fastcall spec, flags)
            const ZFileSpec *s = reinterpret_cast<ZFileSpec *>(self);
            ev(code, text_hash(s->folder), s->ext[0].buffer ? text_hash(s->ext[0].c_str()) : 0, edx ^ s->folders * 7 ^ s->mode * 31);
            const DWORD n = g_cfg[11] % 4 == 0 ? 0 : g_cfg[11] % 4 == 1 ? (g_cfg[12]++ % 2 ? 3 : 0) : 1 + r % 5;
            if (g_trace)
                log("        count %u (folder %s)", n, s->folder ? s->folder : "-");
            return n;
        }
        case 0x4a1150: {   // Files_ListNames (fastcall spec, names; values, stride, max, flags): names in the records
            const ZFileSpec *s = reinterpret_cast<ZFileSpec *>(self);
            ev(code, text_hash(s->folder), args[2] ^ args[3] * 3, args[1] ^ tag(args[0]));
            char *names = reinterpret_cast<char *>(edx);
            const DWORD n = args[2] > 1 && r % 4 == 0 ? args[2] - 1 : args[2];
            const char *pool[] = {"Cache/Fx/-Gloss.STY", "Gloss", "-Ice.txt", "Sand.sty", "Folder/-Deep/Mud.STY", "Mud", "x"};
            for (DWORD i = 0; names && i < n; i++) {
                const char *p = pool[(r + i * 3) % 7];
                char *e = names + i * 0x80;
                if (s->folder) {
                    lstrcpyA(e, s->folder);
                    if (lstrlenA(e) < 60)
                        lstrcatA(e, "/");
                } else {
                    e[0] = 0;
                }
                if (lstrlenA(e) < 90)
                    lstrcatA(e, p);
            }
            return n;
        }
        case 0x591cd0:   // RecordList_Set (sorted, stride, count, records, u08): the records by their names
            ev(code, me, args[0] ^ args[1] * 3, args[2] ^ (args[3] ? 1 : 0) * 5 ^ args[4] * 7);
            for (DWORD i = 0; args[3] && i < args[2] && i < 16; i++)
                ev(code, text_hash(reinterpret_cast<const char *>(args[3]) + i * 0x80));
            return r;
        case 0x47e560:   // NameTree_Ctor: the 0x34-byte heap block noted
            note_block(self, 0x34, true);
            ev(code, tagp(self), tag(args[0]));
            *reinterpret_cast<DWORD *>(self) = ptrv(g_vt[T_OBJ]);
            return ptrv(self);
        default:
            log_args();
            return r;
        }
    }
    // vtable slots
    switch (type) {
    case T_WORLD:
        switch (slot) {
        case 1: ev(code, me); return ptrv(g_gt);
        case 16: ev(code, me, arg_text(0)); return r;
        case 30:
        case 31: ev(code, me); return boolean(13 + (slot & 1));
        case 44: ev(code, me); return ptrv(g_players);
        case 68: {   // the level path: name, a2, out
            ev(code, me, arg_text(0), args[1]);
            char *out = reinterpret_cast<char *>(args[2]);
            lstrcpyA(out, "Levels/");
            lstrcatA(out, reinterpret_cast<const char *>(args[0]));
            return r;
        }
        default: log_args(); return r;
        }
    case T_PLAYERS:
        switch (slot) {
        case 11: ev(code); return g_nplayers;
        case 12: ev(code, args[0]); return ptrv(g_obj[args[0] < 8 ? args[0] : 7]);
        case 23: ev(code, args[0]); return ptrv(g_rec[args[0] % NREC]);
        default: log_args(); return r;
        }
    case T_GT:
        switch (slot) {
        case 12: ev(code); return g_mp ? (r | 1) : (r & ~0xffu);
        default: log_args(); return r;
        }
    case T_OBJ:
        switch (slot) {
        case 1: log_args(); return ptrv(self);
        case 2: ev(code, me); return ptrv(at<CClassInfo *(__cdecl *)()>(0x4163b0)());
        case 4: log_args(); return (r % 5 == 0) ? 1 : 0;
        case 5: ev(code, me); return ptrv("Sequences");
        case 8: log_args(); return r % 3 ? (r | 1) : (r & ~0xffu);
        case 16:
        case 17: ev(code, me, arg_text(0)); return r;
        case 22: ev(code, me); return r % 4;
        case 39:
        case 45: ev(code, me); return g_cfg[15] % 3 ? 480 + (r % 3) * 120 + (slot == 45 ? 160 : 0) : r % 5000;
        case 64: log_args(); return r % 3 ? ptrv(g_obj[14]) : 0;
        default: log_args(); return r;
        }
    case T_PART:
        switch (slot) {
        case 1: log_args(); return ptrv(self);
        case 2: ev(code, me); return ptrv(at<CClassInfo *(__cdecl *)()>(0x4163b0)());
        case 23: {
            const int *rect = reinterpret_cast<int *>(args[2]);
            ev(code, me, tag(args[0]) ^ tag(args[1]) * 3, fnv(2166136261u, rect, 16));
            ev(code, args[3], tag(args[4]));
            return r;
        }
        case 38: log_args(); return (r % 4 == 0) ? 1 : 0;
        case 43: {
            ev(code, me);
            float *out = reinterpret_cast<float *>(args[0]);
            out[0] = pick_float(r);
            out[1] = pick_float(r >> 7);
            return r;
        }
        case 48: ev(code, me); return ptrv(g_obj[20]);
        case 61: ev(code, me); return ptrv("CEntityAnimated");
        case 64: log_args(); return r % 3 ? 0 : ptrv(g_obj[14]);
        default: log_args(); return r;
        }
    case T_LAYER:
        log_args();
        return r;
    case T_LIST: {
        ZDefinitionList *l = reinterpret_cast<ZDefinitionList *>(self);
        switch (slot) {
        case 1: ev(code, me, args[0]); return args[0] <= l->num_entries && l->entries ? ptrv(l->entries[args[0]].name) : 0;
        case 2: ev(code, me, arg_text(0), args[1]); return r % 4 == 0 ? 0xffffffff : r % 4 == 1 ? 0 : r % 9;
        case 5: ev(code, me); return ptrv("Sequences");
        case 37: {   // create the entry's object from its file
            ev(code, me, arg_text(0) ^ arg_text(1), tag(args[2]));
            return r % 4 || g_cfg[0] % 5 == 1 ? ptrv(g_obj[21 + r % 3]) : 0;
        }
        default: log_args(); return r;
        }
    }
    }
    ev(0xbad, id, me);
    return r;
}

// Stack bytes the slots pop (the callers' argument counts)
void init_vtables()
{
    for (int t = 0; t < 8; t++)
        for (int s = 0; s < SLOTS; s++) {
            g_vt[t][s] = thunk((DWORD)(t * SLOTS + s));
            g_pop[t][s] = 0;
        }
    const struct {
        int t, s, pop;
    } P[] = {
        {T_WORLD, 16, 4},   {T_WORLD, 68, 12},  {T_WORLD, 78, 8},   {T_WORLD, 87, 8},  {T_WORLD, 104, 8},
        {T_WORLD, 11, 0x14}, {T_WORLD, 42, 4},  {T_WORLD, 105, 4},
        {T_PLAYERS, 12, 4}, {T_PLAYERS, 23, 4}, {T_PLAYERS, 27, 4},
        {T_GT, 14, 8},
        {T_OBJ, 1, 4},      {T_OBJ, 4, 8},      {T_OBJ, 8, 4},      {T_OBJ, 12, 4},    {T_OBJ, 16, 4},    {T_OBJ, 17, 4},
        {T_OBJ, 27, 8},     {T_OBJ, 28, 8},     {T_OBJ, 64, 8},     {T_OBJ, 69, 4},    {T_OBJ, 93, 4},
        {T_PART, 1, 4},     {T_PART, 20, 4},    {T_PART, 23, 0x14}, {T_PART, 38, 0xc}, {T_PART, 41, 8},   {T_PART, 43, 4},
        {T_PART, 58, 4},    {T_PART, 59, 4},    {T_PART, 64, 8},    {T_PART, 69, 4},   {T_PART, 93, 4},   {T_PART, 104, 8},
        {T_LAYER, 1, 4},    {T_LAYER, 2, 4},
        {T_LIST, 1, 4},     {T_LIST, 2, 8},     {T_LIST, 3, 8},     {T_LIST, 27, 4},    {T_LIST, 37, 12},
    };
    for (const auto &p : P)
        g_pop[p.t][p.s] = (DWORD)p.pop;
}

// ---- Setup of the fakes ----------------------------------------------------------------------------------------------

World *W() { return reinterpret_cast<World *>(g_world); }

// Random filler of the fakes: never a user-space address (below 0x80000000), so a value never falls into a
// heap block of one side by chance (tag() maps those to their block)
DWORD filler() { return random() | 0x80000000u; }

void setup()
{
    World *w = W();
    memset(g_world, 0, sizeof g_world);
    for (DWORD i = 8; i < sizeof g_world; i += 4)
        *reinterpret_cast<DWORD *>(g_world + i) = random() % 4 ? filler() : 0;
    w->vftable = ptrv(g_vt[T_WORLD]);
    w->display = g_obj[0];
    w->timers = g_obj[1];
    w->characters.vftable = ptrv(g_vt[T_OBJ]);
    w->characters.data = g_chars.items;
    w->characters.count = random() % 5;
    w->characters.capacity = 32;
    for (DWORD i = 0; i < 32; i++)
        g_chars.items[i] = random() % 4 ? g_part[random() % NPART] : nullptr;
    w->flag29c = (BYTE)random();
    w->game_type = g_gt;
    w->zoom_frame = g_obj[2];
    w->screen_frame = g_obj[3];
    w->layers.vftable = ptrv(g_vt[T_OBJ]);
    w->layers.data = g_wlayers.items;
    w->layers.count = random() % 5;
    w->layers.capacity = 32;
    for (DWORD i = 0; i < 32; i++)
        g_wlayers.items[i] = random() % 5 ? g_layer[random() % NLAYER] : nullptr;
    w->rules = g_obj[5];
    for (int &v : w->view)
        v = random() % 5 ? (int)(random() % 900) : (int)random();
    if (random() % 3 == 0) {
        w->view[0] = 0;
        w->view[1] = 0;
        w->view[2] = 640;
        w->view[3] = 480 - 0x38;
    }
    w->time = pick_float(random());
    w->saving = (BYTE)random();
    w->traffic_graph = nullptr;
    w->mixer = g_obj[8];
    BYTE *z = g_world;
    *reinterpret_cast<void **>(z + 0x33c) = g_table;
    *reinterpret_cast<void **>(z + 0x34c) = g_obj[9];
    *reinterpret_cast<void **>(z + 0x354) = g_obj[4];
    *reinterpret_cast<DWORD *>(z + 0x58c) = random() % 3 ? random() % 10 : 0xffffffff;
    for (DWORD k = 0; k < sizeof g_table; k += 4)
        *reinterpret_cast<DWORD *>(g_table + k) = random() % 3 ? filler() : 0;
    g_table[0x18] = (BYTE)(random() % 3 == 0);
    g_table[0x19] = (BYTE)(random() % 3 == 0);
    *reinterpret_cast<DWORD *>(g_table + 0x24) = 0;   // heap blocks the code would free: none
    *reinterpret_cast<DWORD *>(g_table + 0x34) = 0;
    // the net layer, the players, the game type
    memset(g_players, 0, sizeof g_players);
    *reinterpret_cast<DWORD *>(g_players) = ptrv(g_vt[T_PLAYERS]);
    g_nplayers = random() % 5;
    for (int i = 0; i < NREC; i++) {
        for (DWORD k = 0; k < 0x100; k += 4)
            *reinterpret_cast<DWORD *>(g_rec[i] + k) = filler();
        *reinterpret_cast<float *>(g_rec[i] + 0xc0) = (float)((int)(random() % 4000) - 500) + (float)(random() % 4) / 4.0f;
        *reinterpret_cast<float *>(g_rec[i] + 0xc4) = (float)((int)(random() % 4000) - 500) + (float)(random() % 4) / 4.0f;
        for (DWORD k = 0xc8; k < 0xd8; k += 4)
            *reinterpret_cast<float *>(g_rec[i] + k) = random() % 5 ? (float)((int)(random() % 4000) - 500) + (float)(random() % 8) / 8.0f : pick_float(random());
        *reinterpret_cast<int *>(g_rec[i] + 0xdc) = random() % 4 ? (int)(random() % 6) - 1 : (int)random();
        *reinterpret_cast<float *>(g_rec[i] + 0xd8) = pick_zoom(random());
        g_rec[i][0xe0] = (BYTE)(random() % 3 ? random() % 8 : 0);
        g_rec[i][0x39] = (BYTE)(random() % 2);
    }
    memset(g_gt, 0, sizeof g_gt);
    *reinterpret_cast<DWORD *>(g_gt) = ptrv(g_vt[T_GT]);
    *reinterpret_cast<void **>(g_gt + 0x14) = g_np_ptrs;
    for (int i = 0; i < NREC; i++) {
        memset(g_np[i], 0, 0x40);
        *reinterpret_cast<DWORD *>(g_np[i]) = ptrv(g_vt[T_GT]);   // slot 1 without arguments
        *reinterpret_cast<int *>(g_np[i] + 0x34) = (int)(random() % 4) - 1;
        g_np_ptrs[i] = g_np[i];
    }
    g_mp = (BYTE)(random() % 2);
    for (int i = 0; i < NOBJ; i++) {
        for (DWORD k = 4; k < 0x80; k += 4)
            *reinterpret_cast<DWORD *>(g_obj[i] + k) = filler();
        *reinterpret_cast<DWORD *>(g_obj[i]) = ptrv(g_vt[T_OBJ]);
    }
    // the rules' view margins
    *reinterpret_cast<float *>(g_obj[5] + 0x10) = (float)(random() % 300);
    *reinterpret_cast<float *>(g_obj[5] + 0x14) = (float)(random() % 300);
    for (int i = 0; i < 2; i++) {
        *reinterpret_cast<DWORD *>(g_tilemap[i] + 0x2cfc) = random() % 4 ? 256 + random() % 6000 : random() % 200;
        *reinterpret_cast<DWORD *>(g_tilemap[i] + 0x2d00) = random() % 4 ? 256 + random() % 6000 : random() % 200;
    }
    for (int i = 0; i < NSOUND; i++) {
        for (DWORD k = 0; k < 0x20; k += 4)
            *reinterpret_cast<DWORD *>(g_sound[i] + k) = filler();
        *reinterpret_cast<void **>(g_sound[i] + 8) = random() % 4 ? g_layer[0] /* a T_LAYER vtable: slots 2 and 11 */ : nullptr;
        *reinterpret_cast<void **>(g_sound[i] + 0x10) = random() % 2 ? g_part[random() % NPART] : nullptr;
    }
    for (int i = 0; i < NLAYER; i++) {
        BYTE *l = g_layer[i];
        for (DWORD k = 4; k < 0x200; k += 4)
            *reinterpret_cast<DWORD *>(l + k) = random() % 3 ? filler() : 0;
        *reinterpret_cast<DWORD *>(l) = ptrv(g_vt[T_LAYER]);
        *reinterpret_cast<void **>(l + 0x7c) = random() % 4 ? g_tilemap[random() % 2] : nullptr;
        *reinterpret_cast<float *>(l + 0x80) = (float)(random() % 3000);
        *reinterpret_cast<float *>(l + 0x84) = pick_float(random());
        l[0x1c8] = (BYTE)(random() % 3 == 0);
        l[0x1cc] = (BYTE)(random() % 3 == 0);
        l[0x1cd] = (BYTE)(random() % 3 == 0);
        const int s = i % 2;
        *reinterpret_cast<void **>(l + 0xf4) = g_sound_ptrs[s];
        *reinterpret_cast<DWORD *>(l + 0xf8) = random() % 4 ? random() % (NSOUND + 1) : 0;
        for (int k = 0; k < NSOUND; k++)
            g_sound_ptrs[s][k] = random() % 5 ? g_sound[(k + s * 3) % NSOUND] : nullptr;
    }
    for (int i = 0; i < NSOUND; i++)
        *reinterpret_cast<void **>(g_sound[i] + 0x14) = random() % 3 ? g_layer[random() % NLAYER] : nullptr;
    for (int i = 0; i < NPART; i++) {
        BYTE *p = g_part[i];
        for (DWORD k = 4; k < PART_SIZE; k += 4)
            *reinterpret_cast<DWORD *>(p + k) = filler();
        *reinterpret_cast<DWORD *>(p) = ptrv(g_vt[T_PART]);
        *reinterpret_cast<void **>(p + 0x48) = g_layer[random() % NLAYER];
        *reinterpret_cast<void **>(p + 8) = random() % 3 ? nullptr : g_childlist[random() % 3];
        *reinterpret_cast<DWORD *>(p + 0x1c) = random() % 3 ? random() : (random() & 0xffff0000u);
        *reinterpret_cast<DWORD *>(p + 0x8c) = random() % 4 ? random() % 10 : random();
        const char *n = NAMES[random() % 7];
        lstrcpyA(g_name_buf[i], n);
    }
    for (int i = 0; i < 3; i++) {
        memset(g_childlist[i], 0, 0x20);
        *reinterpret_cast<void **>(g_childlist[i] + 0xc) = g_child_items[i];
        *reinterpret_cast<DWORD *>(g_childlist[i] + 0x10) = random() % 4;
        for (int k = 0; k < 4; k++)
            g_child_items[i][k] = random() % 4 ? g_part[random() % NPART] : nullptr;
    }
    // the model and its default entity
    for (DWORD k = 0; k < sizeof g_model; k += 4)
        *reinterpret_cast<DWORD *>(g_model + k) = filler();
    *reinterpret_cast<DWORD *>(g_model + 0x10) = random() % 4;
    *reinterpret_cast<float *>(g_model + 0x20) = random() % 3 ? (float)(random() % 100) : pick_float(random());
    *reinterpret_cast<void **>(g_model + 0x74) = random() % 4 ? g_def : nullptr;
    *reinterpret_cast<void **>(g_model + 0xa4) = random() % 3 ? g_obj[13] : nullptr;
    memset(g_def, 0, sizeof g_def);
    *reinterpret_cast<DWORD *>(g_def) = ptrv(g_vt[T_OBJ]);
    *reinterpret_cast<void **>(g_def + 8) = random() % 3 ? g_childlist[random() % 3] : nullptr;
    *reinterpret_cast<DWORD *>(g_def + 0x1c) = random();
    // arrays
    g_temp.a.vftable = ptrv(g_vt[T_OBJ]);
    g_temp.a.data = g_temp.items;
    g_temp.a.count = 0;
    // the registry of the resource lists
    memset(g_registry, 0, sizeof g_registry);
    *reinterpret_cast<DWORD *>(g_registry) = ptrv(g_vt[T_OBJ]);
    *reinterpret_cast<void **>(g_registry + 8) = g_registry_entries;
    for (int i = 0; i < 4; i++)
        g_registry_entries[i][1] = ptrv(g_obj[12 + i]);
    for (int &c : g_can_read)
        c = (int)(random() % 4);
    for (DWORD &c : g_cfg)
        c = random();
    g_calls = 0;
}

// A definition list with fake entries in the given memory (a global list swapped for a case)
void make_list(BYTE *mem)
{
    memset(mem, 0, 0x140);
    ZDefinitionList *l = reinterpret_cast<ZDefinitionList *>(mem);
    *reinterpret_cast<DWORD *>(mem) = ptrv(g_vt[T_LIST]);
    l->entries = random() % 6 ? g_entries : nullptr;
    l->num_entries = random() % (NENTRY + 1);
    lstrcpyA(l->folder, random() % 2 ? "Resources/Death Behaviors" : "Sequences");
    const char *names[] = {"Explode", "-Fade", "Folder/Sub", "Crater Default", "", "Rogcoil Default"};
    for (int i = 0; i <= NENTRY; i++) {
        ZDefinitionRecord &e = g_entries[i];
        memset(&e, 0, sizeof e);
        lstrcpyA(e.name, names[random() % 6]);
        e.object = random() % 3 ? nullptr : g_obj[21 + random() % 3];
        e.loading = (BYTE)(random() % 5 == 0);
        e.keep = (BYTE)random();
    }
}

// The state of all fakes, pointers by number
void state()
{
    for (DWORD i = 0; i < sizeof g_world; i += 4)
        st(tag(*reinterpret_cast<DWORD *>(g_world + i)));
    for (int i = 0; i < NREC; i++) {
        st(fnv(2166136261u, g_rec[i], 0x100));
        st(fnv(2166136261u, g_np[i] + 4, 0x3c));
    }
    for (int i = 0; i < NLAYER; i++)
        for (DWORD k = 0; k < 0x200; k += 4)
            st(tag(*reinterpret_cast<DWORD *>(g_layer[i] + k)));
    for (int i = 0; i < NPART; i++)
        for (DWORD k = 0; k < PART_SIZE; k += 4)
            st(tag(*reinterpret_cast<DWORD *>(g_part[i] + k)));
    for (int i = 0; i < NOBJ; i++)
        for (DWORD k = 0; k < 0x80; k += 4)
            st(tag(*reinterpret_cast<DWORD *>(g_obj[i] + k)));
    for (int i = 0; i < NSOUND; i++)
        st(fnv(2166136261u, g_sound[i], 0x20) ^ tagp(*reinterpret_cast<void **>(g_sound[i] + 8)));
    for (DWORD k = 0; k < sizeof g_table; k += 4)
        st(tag(*reinterpret_cast<DWORD *>(g_table + k)));
    for (int i = 0; i < 3; i++)
        st(fnv(2166136261u, g_childlist[i], 0x20));
    for (DWORD k = 0; k < 0x140; k += 4)
        st(tag(*reinterpret_cast<DWORD *>(g_list + k)));
    for (int i = 0; i <= NENTRY; i++) {
        st(fnv(2166136261u, g_entries[i].name, 0x78));
        st(tagp(g_entries[i].object));
        st(*reinterpret_cast<DWORD *>(&g_entries[i].keep));
    }
    st(g_temp.a.count);
    st(*at<DWORD *>(0x6c28f4));
    st(tag(*at<DWORD *>(0x6bde9c)));
    st(tag(*at<DWORD *>(SOUND_LAYER)));
    st(fnv(2166136261u, at<void *>(0x6c24c8), 8));
    st(fnv(2166136261u, at<void *>(0x62e5e4), 0x5c));
    st(*at<DWORD *>(0x6c29a0) ^ *at<DWORD *>(0x6c29a8) * 3 ^ *at<DWORD *>(0x6c29bc) * 7);
    st(text_hash(reinterpret_cast<ZString *>(at<void *>(LAST_FILE))->buffer ? reinterpret_cast<ZString *>(at<void *>(LAST_FILE))->c_str() : nullptr));
}

// ---- Globals the cases change, put back after each side ----------------------------------------------------------

SavedBytes g_globals[24];
ZString g_last_saved;
int g_nglobals;
void save_global(DWORD address, DWORD n) { save_bytes(g_globals[g_nglobals++], address, n); }
// The function-local static ZStrings and the colour of the code: cleared guards make the code build them;
// what it built is destroyed before the bytes are put back
struct StaticText {
    DWORD z, guard;
};
const StaticText STATICS[] = {{0x6c207c, 0x6c2700}, {0x6c24c0, 0x6c25f8}};
BYTE g_guard_before[2];

void globals_save()
{
    g_nglobals = 0;
    save_global(0x6c28f4, 4);
    save_global(0x6bde9c, 4);
    save_global(SOUND_LAYER, 4);
    save_global(0x6c02cc, 4);
    save_global(G_WORLD, 4);
    save_global(0x6c20f8, 1);
    save_global(0x6c24c8, 16);
    save_global(0x6c29a8, 4);
    save_global(0x5fe8f8, 4);
    save_global(0x6c1d04, 4);
    save_global(0x6bfb08, 4);
    save_global(0x62e5e4, 0x5c);   // the cursor table of 0x4f13a0
    save_global(0x6c29a0, 4);
    save_global(0x6c29bc, 4);
    at<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(&g_last_saved, at<void *>(LAST_FILE));
    for (int i = 0; i < 2; i++) {
        save_global(STATICS[i].z, 4);
        save_global(STATICS[i].guard, 1);
        g_guard_before[i] = *at<BYTE *>(STATICS[i].guard);
    }
}
void globals_restore()
{
    at<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(at<void *>(LAST_FILE), &g_last_saved);
    at<void(THISCALL *)(void *)>(0x4defd0)(&g_last_saved);
    for (int i = 0; i < 2; i++)
        if (!(g_guard_before[i] & 1) && (*at<BYTE *>(STATICS[i].guard) & 1))
            at<void(THISCALL *)(void *)>(0x4defd0)(at<void *>(STATICS[i].z));
    for (int i = g_nglobals - 1; i >= 0; i--)
        restore_bytes(g_globals[i]);
}

void free_blocks()
{
    for (int i = 0; i < g_nblocks; i++)
        if (g_blocks[i].heap)
            heap_free(g_blocks[i].p);
        else
            pool_free(g_blocks[i].p, g_blocks[i].size);
    g_nblocks = 0;
}

// ---- Two sides ----------------------------------------------------------------------------------------------------------

DWORD g_skip[2];   // functions under test that stay out of the recorder list (recursion, a caller's callee)
DWORD g_swap;      // a global definition list swapped with the fake list during the case (0: none)
SavedBytes g_swapped;

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
        globals_save();
        *at<void **>(0x6c02cc) = g_obj[6];   // the error reporter
        setup();
        make_list(g_list);
        if (g_swap) {
            save_bytes(g_swapped, g_swap, 0x140);
            memcpy(at<void *>(g_swap), g_list, 0x140);
        }
        if (side == 0)
            hooks_out();
        g_patches.on(g_skip[0], g_skip[1]);
        r[side] = run(mask);
        g_patches.off();
        if (side == 0)
            hooks_in();
        if (g_swap) {
            memcpy(g_list, at<void *>(g_swap), 0x140);
            restore_bytes(g_swapped);
        }
        state();
        free_blocks();
        globals_restore();
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
    t.check(same, "world rest", d);
    return same;
}

// An st(0) result as a dword hash of its 80 bits
DWORD ld_bits(LD v)
{
    BYTE b[10];
    __asm__("fstpt %0" : "=m"(b) : "t"(v) : "st");
    return fnv(2166136261u, b, 10);
}

void zs_init(ZString *z) { at<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_free(ZString *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
DWORD zs_hash(ZString *z) { return z->buffer ? text_hash(z->c_str()) : 0xffff; }

// ---- The cases --------------------------------------------------------------------------------------------------------

using R = WorldRest;

void world_cases()
{
    Tally t{"world rest: world class"};
    unsigned seed = 5000;
    const int N = scaled(150);
    for (int k = 0; k < N; k++) {
        g_skip[0] = g_skip[1] = 0;
        g_swap = 0;
        two_sides(t, "MultiplayerFrame", seed++, [](DWORD &m) {
            m = 0;
            // a client reads player 0's layer (the original calls through null otherwise)
            if (!g_mp) {
                *reinterpret_cast<int *>(g_rec[0] + 0xdc) = 0;
                if (!W()->layers.count)
                    W()->layers.count = 1;
                W()->layers.data[0] = g_layer[random() % NLAYER];
            }
            at<void(THISCALL *)(World *, DWORD, DWORD)>(0x4f0230)(W(), 7, random() % 2 ? 1 : 0x100);
            return 0u;
        });
        two_sides(t, "ZoomKeys", seed++, [](DWORD &m) {
            m = 0;
            const float dt = random() % 4 ? (float)(random() % 1000) / 1000.0f : pick_float(random());
            at<void(THISCALL *)(World *, DWORD, DWORD)>(0x4f3200)(W(), bits(dt), random() % 4);
            return 0u;
        });
        two_sides(t, "CurrentLevelPath", seed++, [](DWORD &m) {
            m = 0;
            ZString out;
            zs_init(&out);
            at<void(THISCALL *)(World *, void *)>(0x4f3ac0)(W(), &out);
            st(zs_hash(&out));
            zs_free(&out);
            return 0u;
        });
        two_sides(t, "ReadSaveGame", seed++, [](DWORD &m) {
            m = 0xff;
            *at<void **>(G_WORLD) = W();
            // RemoveAndDelete (0x4eea50) runs for real on the fake list: layers deleted through slot 1
            return at<DWORD(THISCALL *)(World *, void *, DWORD)>(0x4f3e70)(W(), g_obj[7], random() % 2 ? 1 : 0x100);
        });
        two_sides(t, "ReloadResources", seed++, [](DWORD &m) {
            m = 0;
            // kind 0 needs a level (the original calls through a null layer otherwise)
            DWORD kind = random() % 4;
            if (kind == 0 && !W()->layers.count)
                kind = 1;
            if (W()->layers.count && !W()->layers.data[0])
                W()->layers.data[0] = g_layer[0];
            int idx = *reinterpret_cast<int *>(g_rec[0] + 0xdc);
            if (kind == 0 && (idx < 0 || idx >= (int)W()->layers.count || !W()->layers.data[idx]))
                *reinterpret_cast<int *>(g_rec[0] + 0xdc) = 0;
            at<void(THISCALL *)(World *, DWORD, DWORD)>(0x4f4190)(W(), kind, random() % 2 ? 1 : 0x100);
            return 0u;
        });
        two_sides(t, "FixLegacyData", seed++, [](DWORD &m) {
            m = 0;
            at<void(THISCALL *)(World *, Layer *)>(0x4f5c80)(W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]));
            return 0u;
        });
        g_skip[0] = 0x4f5dd0;
        two_sides(t, "SetViewTarget", seed++, [](DWORD &m) {
            m = 0;
            const float x = random() % 5 ? (float)((int)(random() % 4000) - 500) + 0.5f : pick_float(random());
            const float y = random() % 5 ? (float)((int)(random() % 4000) - 500) : pick_float(random());
            const int mode = (int)(random() % 5) - 1;
            const DWORD i = random() % 4 ? random() % 4 : 0xffffffff;
            at<void(THISCALL *)(World *, DWORD, DWORD, int, DWORD)>(0x4f5dd0)(W(), bits(x), bits(y), mode, i);
            return 0u;
        });
        g_skip[0] = 0x4f6770;
        two_sides(t, "SetZoom", seed++, [](DWORD &m) {
            m = 0;
            at<void(THISCALL *)(World *, DWORD, DWORD)>(0x4f6770)(W(), bits(pick_zoom(random())), random() % 4);
            return 0u;
        });
        g_skip[0] = 0;
        two_sides(t, "SetViewRect", seed++, [](DWORD &m) {
            m = 0;
            int rect[4];
            for (int &v : rect)
                v = (int)(random() % 1200) - 100;
            at<void(THISCALL *)(World *, const int *)>(0x4f6980)(W(), random() % 2 ? rect : nullptr);
            return 0u;
        });
        two_sides(t, "SpawnExplosion", seed++, [](DWORD &m) {
            m = 0;
            g_cfg[6] = 1;   // the entity has a model (the original reads it through null otherwise)
            if (random() % 3 == 0)
                *at<BYTE *>(0x6c2700) &= ~1;   // the static "Idle" not built yet (atexit replaced)
            const float x = (float)(random() % 3000), y = pick_float(random());
            at<void(THISCALL *)(World *, Layer *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4f6320)(
                W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]), random() % 5 ? random() % 40 : 0, bits(x), bits(y), random(),
                random() % 2 ? 1 : 0x100, random());
            return 0u;
        });
        two_sides(t, "SpawnExplosionEntity", seed++, [](DWORD &) {
            g_cfg[6] = 1;
            if (random() % 3 == 0)
                *at<BYTE *>(0x6c25f8) &= ~1;
            const float x = (float)(random() % 3000), y = pick_float(random());
            return tag(ptrv(at<void *(THISCALL *)(World *, Layer *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4f6530)(
                W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]), random() % 5 ? random() % 40 : 0, bits(x), bits(y), random(),
                random() % 2 ? 1 : 0x100, random(), random() % 2 ? 1 : 0x200, random())));
        });
        g_swap = GAME_TYPE_LIST;
        two_sides(t, "ZaxStartNewGame", seed++, [](DWORD &m) {
            m = 0;
            ZDefinitionList *l = at<ZDefinitionList *>(GAME_TYPE_LIST);
            g_cfg[0] = 1;   // slot 37 makes an object
            if (!g_table[0x18] && !g_table[0x19] && random() % 2) {
                *reinterpret_cast<void **>(g_table + 0x24) = heap_alloc(16);   // freed by the code
                *reinterpret_cast<void **>(g_table + 0x34) = heap_alloc(16);
            }
            // the chosen entry must exist with an object (the original calls through null otherwise)
            if (*reinterpret_cast<DWORD *>(g_world + 0x58c) != 0xffffffff) {
                l->entries = g_entries;
                if (!l->num_entries)
                    l->num_entries = 1;
                const DWORD gt = *reinterpret_cast<DWORD *>(g_world + 0x58c);
                ZDefinitionRecord &e = g_entries[gt < l->num_entries ? gt : 0];
                e.loading = 0;
                if (random() % 2)
                    e.object = g_obj[21];
            }
            at<void(THISCALL *)(World *, DWORD, const char *, const char *, DWORD)>(0x59b7f0)(W(), 1, "Test/Nachbau1", "Game Start", 2);
            return 0u;
        });
        g_swap = SEQUENCE_LIST;
        two_sides(t, "PropertiesPath", seed++, [](DWORD &) {
            ZString out;
            zs_init(&out);
            ZDefinitionList *a = at<ZDefinitionList *>(SEQUENCE_LIST);
            if (!a->entries)
                a->num_entries = 0;   // the search reads the entries without a check
            // the second list: a copy of the fake list with the same entries
            SavedBytes s2;
            save_bytes(s2, ANIMATED_TEXTURE_LIST, 0x140);
            memcpy(at<void *>(ANIMATED_TEXTURE_LIST), a, 0x140);
            if (a->entries)
                at<ZDefinitionList *>(ANIMATED_TEXTURE_LIST)->num_entries = random() % (NENTRY + 1);
            void *obj = random() % 4 ? g_obj[21 + random() % 3] : g_part[0];
            const DWORD r = at<DWORD(__fastcall *)(void *, void *)>(0x4f0ac0)(obj, &out);
            st(zs_hash(&out));
            restore_bytes(s2);
            zs_free(&out);
            return r;
        });
        two_sides(t, "PreloadDefinitions", seed++, [](DWORD &m) {
            m = 0;
            ZDefinitionList *a = at<ZDefinitionList *>(SEQUENCE_LIST);
            SavedBytes s2;
            save_bytes(s2, ANIMATED_TEXTURE_LIST, 0x140);
            memcpy(at<void *>(ANIMATED_TEXTURE_LIST), a, 0x140);
            at<ZDefinitionList *>(ANIMATED_TEXTURE_LIST)->num_entries = random() % (NENTRY + 1);
            at<void(THISCALL *)(World *)>(0x4f10f0)(W());
            memcpy(g_list + 0x30, at<BYTE *>(ANIMATED_TEXTURE_LIST) + 0x30, 8);   // (compared with the fake list's state)
            restore_bytes(s2);
            return 0u;
        });
        g_swap = DEATH_BEHAVIORS;
        two_sides(t, "FixPart", seed++, [](DWORD &m) {
            m = 0;
            *at<DWORD *>(0x6c29a8) = random() % 2 ? 0x10 : random();
            *at<DWORD *>(0x5fe8f8) = random() % 10;
            // the code follows the child lists' first entries: make them parts
            for (auto &items : g_child_items)
                if (!items[0] && random() % 2)
                    items[0] = g_part[random() % NPART];
            void *part = g_part[random() % NPART];
            at<void(THISCALL *)(World *, Layer *, void *)>(0x4f5660)(W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]), part);
            return 0u;
        });
        g_swap = 0;
        g_record_loaders = true;
        two_sides(t, "LoadGameData", seed++, [](DWORD &m) {
            m = 0;
            g_cfg[1] = 1;   // the rules file gives a CRules (IsKindOf)
            *reinterpret_cast<DWORD *>(g_obj[6] + 0x24) = 8 + 8 * (random() % 4);   // the colour depth
            g_cfg[11] = random();
            g_cfg[12] = 0;
            // the three lists the code loads inline: fresh fake lists, their records freed afterwards
            const DWORD lists[3] = {0x6be1e0, 0x6bf9b8, 0x6bf850};
            SavedBytes saved[3];
            for (int k = 0; k < 3; k++) {
                save_bytes(saved[k], lists[k], 0x140);
                memset(at<void *>(lists[k]), 0, 0x140);
                *at<DWORD *>(lists[k]) = ptrv(g_vt[T_LIST]);
            }
            at<void(THISCALL *)(World *, void *)>(0x4f13a0)(W(), g_obj[4]);
            for (int k = 0; k < 3; k++) {
                ZDefinitionList *l = at<ZDefinitionList *>(lists[k]);
                st(l->num_entries);
                st(text_hash(l->folder));
                st(text_hash(l->list_name));
                st(tagp(l->monitor));
                if (l->entries) {
                    for (DWORD i = 0; i < l->num_entries && i < 16; i++)
                        st(text_hash(l->entries[i].name));
                    DWORD *block = reinterpret_cast<DWORD *>(l->entries) - 1;
                    note_block(block, block[0] * 0x80 + 4, true);
                }
                restore_bytes(saved[k]);
            }
            return 0u;
        });
        g_record_loaders = false;
    }
    t.report();
}

// The list loaders on fake lists with the file search recorded
void loader_cases()
{
    Tally t{"world rest: list loaders"};
    unsigned seed = 9000;
    const int N = scaled(100);
    const DWORD fns[7] = {0x4f6ba0, 0x4f6ea0, 0x4f71a0, 0x4f74a0, 0x5a3650, 0x5a3950, 0x5a3c50};
    const char *const folders[] = {"Surface Types", "Models", "Dialogue", "Water Warp Effects", "", "Resources/Frame Overlay Options"};
    g_skip[0] = g_skip[1] = 0;
    g_swap = 0;
    for (int k = 0; k < N; k++)
        for (DWORD f : fns)
            two_sides(t, "LoadList", seed++, [&](DWORD &) {
                g_cfg[12] = 0;
                // the display's colour depth (0x55d030 through the error reporter) picks the model extension
                *reinterpret_cast<DWORD *>(g_obj[6] + 0x24) = 8 + 8 * (random() % 4);
                const char *folder = folders[random() % 6];
                const DWORD flags = random() % 3;
                if (g_trace)
                    log("      loader %x folder '%s' flags %u", f, folder, flags);
                const DWORD r = at<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(f)(g_list, "List Name", folder, flags);
                if (g_trace)
                    log("      returned %u", r);
                ZDefinitionList *l = reinterpret_cast<ZDefinitionList *>(g_list);
                // the records the code allocated: their names into the state, the block freed afterwards
                if (l->entries) {
                    for (DWORD i = 0; i < l->num_entries && i < 16; i++)
                        st(text_hash(l->entries[i].name));
                    DWORD *block = reinterpret_cast<DWORD *>(l->entries) - 1;
                    note_block(block, block[0] * 0x80 + 4, true);
                    l->entries = nullptr;
                }
                st(text_hash(l->folder));
                st(text_hash(l->list_name));
                return r;
            });
    t.report();
}

// The layer: the point query, the sound layer, SetLayer; the collide checker; the small helpers
void layer_cases()
{
    Tally t{"world rest: layer, checker, helpers"};
    unsigned seed = 12000;
    const int N = scaled(200);
    g_skip[0] = g_skip[1] = 0;
    g_swap = 0;
    SavedBytes qg, qo;
    for (int k = 0; k < N; k++) {
        two_sides(t, "PartAt", seed++, [&](DWORD &) {
            // the draw queues' counts (read by 0x482de0, not replaceable): small values for the case
            save_bytes(qg, DRAW_QUEUE_GROUND + 8, 4);
            save_bytes(qo, DRAW_QUEUE_OBJECTS + 8, 4);
            *at<DWORD *>(DRAW_QUEUE_GROUND + 8) = random() % 4;
            *at<DWORD *>(DRAW_QUEUE_OBJECTS + 8) = random() % 4;
            const int point[2] = {(int)(random() % 3000) - 100, (int)(random() % 3000)};
            CClassInfo *skip = random() % 2 ? at<CClassInfo *(__cdecl *)()>(0x4163b0)() : nullptr;
            void *p = at<void *(THISCALL *)(Layer *, const int *, DWORD, CClassInfo *, DWORD, DWORD, DWORD)>(0x4ed350)(
                reinterpret_cast<Layer *>(g_layer[random() % NLAYER]), point, random() % 4, skip, random(), random(), random());
            restore_bytes(qo);
            restore_bytes(qg);
            return tagp(p);
        });
        two_sides(t, "SetSoundLayer", seed++, [](DWORD &m) {
            m = 0;
            *at<void **>(SOUND_LAYER) = random() % 3 ? g_layer[random() % NLAYER] : nullptr;
            at<void(THISCALL *)(Layer *)>(0x4ed960)(random() % 6 ? reinterpret_cast<Layer *>(g_layer[random() % NLAYER]) : nullptr);
            return 0u;
        });
        two_sides(t, "SetLayer", seed++, [](DWORD &m) {
            m = 0;
            // child lists without cycles: each list's items are parts of a later index
            for (int i = 0; i < NPART; i++)
                *reinterpret_cast<void **>(g_part[i] + 8) = nullptr;
            for (int c = 0; c < 3; c++) {
                for (int j = 0; j < 4; j++)
                    g_child_items[c][j] = random() % 4 ? g_part[4 + c * 2 + j % 2] : nullptr;
                *reinterpret_cast<void **>(g_part[c] + 8) = g_childlist[c];
            }
            at<void(THISCALL *)(void *, void *)>(0x4fc8f0)(g_part[random() % 3], random() % 4 ? g_layer[random() % NLAYER] : nullptr);
            return 0u;
        });
        two_sides(t, "Checker", seed++, [](DWORD &m) {
            void *mover = g_part[random() % NPART];
            int from[2] = {(int)(random() % 4000) - 500, random() % 6 ? (int)(random() % 4000) : (int)random()};
            int to[2] = {(int)(random() % 4000) - 500, (int)(random() % 4000)};
            DWORD h = at<DWORD(THISCALL *)(void *, void *, const int *, const int *)>(0x535dc0)(g_obj[0], mover, from, to) & 0xff;
            h = h * 3 + ld_bits(at<LD(THISCALL *)(void *, void *, const int *, const int *)>(0x535eb0)(g_obj[0], mover, from, to));
            const float pos[2] = {pick_float(random()), (float)(random() % 1000)};
            h = h * 3 + (at<DWORD(THISCALL *)(void *, void *, const float *)>(0x536030)(g_obj[0], mover, pos) & 0xff);
            h = h * 3 + (at<DWORD(__fastcall *)(void *, void *)>(0x535e70)(nullptr, g_part[random() % NPART]) & 0xff);
            h = h * 3 + (at<DWORD(__fastcall *)(void *, void *)>(0x536010)(nullptr, g_part[random() % NPART]) & 0xff);
            m = 0xffffffff;
            return h;
        });
        two_sides(t, "Vectors", seed++, [](DWORD &) {
            float v[2] = {pick_float(random()), pick_float(random())};
            if (random() % 4 == 0)
                v[0] = v[1] = 0;
            at<void(THISCALL *)(float *)>(0x548430)(v);
            st(bits(v[0]));
            st(bits(v[1]));
            float a[2] = {pick_float(random()), pick_float(random())}, b[2] = {pick_float(random()), pick_float(random())}, out[2];
            const DWORD r = tagp(at<const float *(THISCALL *)(float *, const float *, const float *)>(0x548470)(out, a, b));
            st(bits(out[0]));
            st(bits(out[1]));
            DWORD hnd[2] = {random(), random()};
            const DWORD r2 = tagp(at<void *(THISCALL *)(DWORD *)>(0x518260)(hnd));
            st(hnd[0]);
            st(hnd[1]);
            st(at<DWORD(THISCALL *)(void *, void *, void *)>(0x4e96b0)(g_obj[0], g_part[0], g_part[1]) & 0xff);
            return r ^ r2 * 3;
        });
        two_sides(t, "Class getters", seed++, [](DWORD &) {
            // the variables cleared: the registration recorded (ClassInfo_Register), the pool block freed
            if (random() % 2)
                *at<DWORD *>(0x6c1d04) = 0;
            if (random() % 2)
                *at<DWORD *>(0x6bfb08) = 0;
            DWORD h = tag(ptrv(at<CClassInfo *(__cdecl *)()>(0x4e9920)()));
            h = h * 3 + tag(ptrv(at<CClassInfo *(__cdecl *)()>(0x4b5c60)()));
            return h;
        });
    }
    t.report();
}

// ---- Real data ----------------------------------------------------------------------------------------------------------

void real_data()
{
    Tally t{"world rest: real data"};
    World *w = *at<World **>(G_WORLD);
    if (!w || w->vftable != VT_ZAX_WORLD) {
        log("  world rest: no world (vtable %x)", w ? w->vftable : 0);
        t.check(false, "real world", "no world");
        t.report();
        return;
    }
    // the current level path
    {
        ZString a, b;
        zs_init(&a);
        zs_init(&b);
        original<void(THISCALL *)(World *, void *)>(0x4f3ac0)(w, &a);
        at<void(THISCALL *)(World *, void *)>(0x4f3ac0)(w, &b);
        t.check(zs_hash(&a) == zs_hash(&b), "real world", a.buffer ? a.c_str() : "(none)");
        log("  world rest: current level path %s", a.buffer ? a.c_str() : "(none)");
        zs_free(&a);
        zs_free(&b);
    }
    // the Properties.txt paths of every loaded entry of both lists, and of objects of neither
    int paths = 0;
    const DWORD lists[2] = {SEQUENCE_LIST, ANIMATED_TEXTURE_LIST};
    for (DWORD li : lists) {
        ZDefinitionList *l = at<ZDefinitionList *>(li);
        for (DWORD i = 0; i < l->num_entries && l->entries; i++) {
            void *o = l->entries[i].object;
            if (!o)
                continue;
            ZString a, b;
            zs_init(&a);
            zs_init(&b);
            const DWORD ra = original<DWORD(__fastcall *)(void *, void *)>(0x4f0ac0)(o, &a);
            const DWORD rb = at<DWORD(__fastcall *)(void *, void *)>(0x4f0ac0)(o, &b);
            t.check(ra == rb && zs_hash(&a) == zs_hash(&b), "real world", a.buffer ? a.c_str() : "(none)");
            paths++;
            zs_free(&a);
            zs_free(&b);
        }
    }
    log("  world rest: %d Properties.txt paths", paths);
    // the collide checker's filters for every part of player 0's level, CanStand for player 0's entity there
    // (the line sweeps of slots 23 and 24 notify the parts they hit, so they are not run on the level)
    Layer *cur = original<Layer *(THISCALL *)(World *)>(0x4f1050)(w);
    void *mover = original<void *(THISCALL *)(World *)>(0x4f5d80)(w);   // the first player character
    int filtered = 0, nparts = 0, free_spots = 0, stands = 0;
    if (cur) {
        LayerArray *list;
        at<void *(THISCALL *)(LayerArray **)>(0x59a470)(&list);
        at<DWORD(THISCALL *)(void *, LayerArray *, const int *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(reinterpret_cast<BYTE *>(cur) + 0x50, list,
                                                                                                 nullptr, 0, 0, 0, 0);
        for (DWORD i = 0; i < list->count; i++) {
            void *p = list->data[i];
            const DWORD a1 = original<DWORD(__fastcall *)(void *, void *)>(0x535e70)(nullptr, p) & 0xff;
            const DWORD b1 = at<DWORD(__fastcall *)(void *, void *)>(0x535e70)(nullptr, p) & 0xff;
            const DWORD a2 = original<DWORD(__fastcall *)(void *, void *)>(0x536010)(nullptr, p) & 0xff;
            const DWORD b2 = at<DWORD(__fastcall *)(void *, void *)>(0x536010)(nullptr, p) & 0xff;
            char d[80];
            wsprintfA(d, "[filters of part %u: %x/%x %x/%x]", i, a1, b1, a2, b2);
            t.check(a1 == b1 && a2 == b2, "real world", d);
            filtered += a1 != 0;
            nparts++;
        }
        at<DWORD(THISCALL *)(LayerArray **)>(0x59a510)(&list);
        if (!mover || !readable(mover, 0x100) || *reinterpret_cast<void **>(static_cast<BYTE *>(mover) + 0x48) != cur)
            log("  world rest: no character in the level (%x)", ptrv(mover));
        else {
            float pos[2];
            at<void(THISCALL *)(void *, float *)>(0x4fb0a0)(mover, pos);
            for (int i = 0; i < 60; i++) {
                const float p[2] = {pos[0] + (float)((int)(random() % 800) - 400), pos[1] + (float)((int)(random() % 800) - 400)};
                const DWORD e = original<DWORD(THISCALL *)(void *, void *, const float *)>(0x536030)(nullptr, mover, p) & 0xff;
                const DWORD f = at<DWORD(THISCALL *)(void *, void *, const float *)>(0x536030)(nullptr, mover, p) & 0xff;
                char d[80];
                wsprintfA(d, "[CanStand %d,%d: %x/%x]", (int)p[0], (int)p[1], e, f);
                t.check(e == f, "real world", d);
                free_spots += e != 0;
                stands++;
            }
        }
    }
    log("  world rest: filters of %d parts (%d pass the first), CanStand at %d spots (%d free)", nparts, filtered, stands, free_spots);
    // SetLayer on the running level's parts (the same layer again: no change)
    t.report();
}

// The loaders on the game's folders: fresh lists compared with the global ones the original built at start
void real_loaders()
{
    Tally t{"world rest: list loaders (real data)"};
    const struct {
        DWORD fn, global;
        const char *name, *folder;
        DWORD getter;   // the function-static lists: their getter
    } L[] = {
        {0x4f6ba0, 0x713868, "Surface Types", "Surface Types", 0},
        {0x4f74a0, 0x713c80, "Water Warp Effects", "Water Warp Effects", 0},
        {0x5a3650, 0, "Quip Categories", "Resources", 0x55b940},
        {0x5a3950, 0, "Canned Object", "Resources", 0x468000},
        {0x5a3c50, 0x6c2a00, "Dificulty Types", "Resources", 0},
    };
    for (const auto &e : L) {
        ZDefinitionList *g = e.getter ? at<ZDefinitionList *(__cdecl *)()>(e.getter)() : at<ZDefinitionList *>(e.global);
        DWORD n[2] = {0, 0}, h[2] = {0, 0};
        bool same_global = true;
        for (int side = 0; side < 2; side++) {
            alignas(16) BYTE mem[0x140];
            memset(mem, 0, sizeof mem);
            ZDefinitionList *l = reinterpret_cast<ZDefinitionList *>(mem);
            *reinterpret_cast<DWORD *>(mem) = ptrv(g_vt[T_LIST]);
            init_vtables();
            g_side = side;
            g_loghash[side] = 2166136261u;
            g_nlog[side] = 0;
            g_calls = 0;
            // the registry entry, the record list setter and the name tree recorded; the file search for real
            CodePatch p[3];
            patch_jump(p[0], 0x4a34d0, vptr(thunk(T_FN * SLOTS + fn_index(0x4a34d0))));
            patch_jump(p[1], 0x523e20, vptr(thunk(T_FN * SLOTS + fn_index(0x523e20))));
            patch_jump(p[2], 0x47e560, vptr(thunk(T_FN * SLOTS + fn_index(0x47e560))));
            g_nblocks = 0;
            const DWORD r = side == 0 ? original<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(e.fn)(l, e.name, e.folder, 0)
                                      : at<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(e.fn)(l, e.name, e.folder, 0);
            for (int i = 2; i >= 0; i--)
                unpatch(p[i]);
            n[side] = r;
            h[side] = 2166136261u;
            for (DWORD i = 0; i < l->num_entries && l->entries; i++)
                h[side] = fnv_text(h[side], l->entries[i].name);
            if (side == 1) {
                same_global = g->num_entries == l->num_entries;
                for (DWORD i = 0; same_global && i < l->num_entries; i++)
                    same_global = crt_stricmp(g->entries[i].name, l->entries[i].name) == 0;
            }
            if (l->entries)
                heap_free(reinterpret_cast<DWORD *>(l->entries) - 1);
            free_blocks();
        }
        char d[160];
        wsprintfA(d, "[%s: %u entries %x / %u entries %x, global %u, log %x / %x]", e.name, n[0], h[0], n[1], h[1], g->num_entries,
                  g_loghash[0], g_loghash[1]);
        t.check(n[0] == n[1] && h[0] == h[1] && same_global && g_loghash[0] == g_loghash[1], "real loaders", d);
        log("  world rest: %s", d);
    }
    t.report();
}

}  // namespace

ZAX_CHECKS(world_rest)
{
    (void)cls;
    (void)n;
    if (!thunks_ok()) {
        log("  world rest: recorder thunks are broken");
        return;
    }
    init_vtables();
    world_cases();
    loader_cases();
    layer_cases();
    real_data();
    real_loaders();
}

}  // namespace zax
