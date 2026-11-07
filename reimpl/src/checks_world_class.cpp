// Checks of the world class (world_class.cpp) and the per-frame level update (layer_update.cpp) against
// the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of both modules taken out (nested
// calls run the original too), side 1 through the reimplementation. Compared: eax (masked where only al
// is defined), st(0) where a function returns a float, an event log of every call into fakes and
// stand-ins, and the state of all fakes and globals afterwards (pointers by number).
//
// Fakes: a world (0x338 bytes, a vtable of 120 recorder thunks), the net layer with player records,
// the game type, layers, parts, generic objects (frames, the display, the error reporter, packets) and
// pointer arrays; each vtable slot is a recorder thunk ("mov eax, id; jmp" into one stub that logs the
// arguments, writes deterministic outputs and returns a value derived from the call). Every exe
// function the code calls outside the two modules jumps to such a recorder too (the layer functions,
// the grid queries, the temporary part lists, the block writers, the timers, the frames, ...); the
// string, path and array helpers run for real.
//  - World slots and methods, ~70 functions on random worlds (layer lists with gaps, 0-5 players,
//    characters, flags), 150-400 cases each; the constructor on patterned blocks (g_World put back),
//    the destructor on worlds built with pool and heap blocks.
//  - The level update: FirstUpdate, WorldUpdate (monitor entries with and without size, views),
//    OnPartFlags, SendPartFlags, Begin and WriteRect on fake save data.
//  - Real data (the running game): the read-only accessors of g_World (current layer, the players'
//    layers, finding the level by name in several spellings, characters, zoom, view target and
//    rectangles, the times, the small slots, level paths and LevelExists) through original and
//    reimplementation directly; SendPartFlags on the running level with the block writer recorded.
#include "check_util.h"
#include "layer_update.h"
#include "world_class.h"
#include "dir_list.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_world_class(Hook *out);
int hooks_layer_update(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
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

// ---- Hooks of both modules out and in ------------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 128;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void sites_out(int (*table)(Hook *))
{
    Hook list[MAX_SITES];
    const int n = table(nullptr);
    if (!hook_sites_fit("world_class", g_nsites + n, MAX_SITES))
        return;
    table(list);
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
void hooks_out()
{
    g_nsites = 0;
    sites_out(hooks_world_class);
    sites_out(hooks_layer_update);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Fakes ------------------------------------------------------------------------------------------------------

constexpr int T_WORLD = 0, T_PLAYERS = 1, T_GT = 2, T_OBJ = 3, T_PART = 4, T_LAYER = 5, T_ARRAY = 6, T_FN = 7;
constexpr int SLOTS = 128;
constexpr int NREC = 6, NOBJ = 16, NLAYER = 6, NPART = 12, NARRAY = 4;

alignas(16) BYTE g_world[0x400];
alignas(16) BYTE g_players[0x40];
alignas(16) BYTE g_rec[NREC][0x100];
alignas(16) BYTE g_gt[0x40];
alignas(16) BYTE g_obj[NOBJ][0x80];
alignas(16) BYTE g_layer[NLAYER][0x200];
alignas(16) BYTE g_part[NPART][0x100];
struct FakeArray {
    LayerArray a;
    DWORD guard[4];   // the dwords before the data (UnloadLayer writes data[-1] for a layer not in the list)
    void *items[32];
};
FakeArray g_array[NARRAY];
FakeArray g_wlayers;        // the world's layer list
FakeArray g_temp;           // the temporary part list (0x59a470)
alignas(16) BYTE g_sd[0x200];   // fake CLayerSaveData for the level update
alignas(16) BYTE g_mon[8][0x20];   // its monitor entries
alignas(16) BYTE g_tile[0x60];
BYTE g_name_buf[NLAYER][32];
int s_views[64 * 2];                // the view list's data while 0x5a41b0 is replaced
alignas(16) BYTE g_ctor_block[0x400];

DWORD g_vt[8][SLOTS];
DWORD g_pop[8][SLOTS];

extern "C" {
extern char wc_thunks[];
DWORD wc_pop, wc_ret_float;
float wc_fret;
DWORD __cdecl wc_record(DWORD *args, DWORD id, BYTE *self, DWORD edx);
}
__asm__(".text\n"
        ".globl _wc_thunks\n"
        "_wc_thunks:\n"
        ".set wc_i, 0\n"
        ".rept 1100\n"
        "  movl $wc_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _wc_common - . - 4\n"
        ".set wc_i, wc_i + 1\n"
        ".endr\n"
        "_wc_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 16(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _wc_record\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _wc_ret_float\n"
        "  je 1f\n"
        "  flds _wc_fret\n"
        "1:\n"
        "  movl _wc_pop, %ecx\n"
        "  popl %edx\n"
        "  addl %ecx, %esp\n"
        "  jmp *%edx\n");
DWORD thunk(DWORD id) { return ptrv(wc_thunks) + 10 * id; }

// The exe functions replaced by recorders during the cases: address, stack bytes popped
struct Fn {
    DWORD address;
    BYTE pop;
};
const Fn FNS[] = {
    {0x4cd780, 0x1c}, {0x416610, 0}, {0x4173c0, 0}, {0x419ea0, 0}, {0x51e360, 0}, {0x4a4670, 8}, {0x4a1780, 0}, {0x505f00, 0},
    {0x4a1970, 8}, {0x4a8a90, 0}, {0x55d030, 0}, {0x4ab5a0, 0xc}, {0x506730, 8}, {0x57e240, 0}, {0x51f960, 0}, {0x5067b0, 0},
    {0x4173f0, 0}, {0x4166d0, 0}, {0x4fc200, 0xc}, {0x4cdcc0, 0}, {0x4eb670, 0}, {0x4e9e00, 4}, {0x4a1540, 0}, {0x4a1570, 0},
    {0x4ebe30, 8}, {0x4ec090, 4}, {0x4a2650, 0}, {0x4a2670, 0}, {0x4f5c80, 4}, {0x4eb6b0, 4}, {0x4eda60, 0}, {0x5240b0, 0},
    {0x505e40, 0}, {0x5242f0, 0xc}, {0x5241e0, 0}, {0x4ea870, 0}, {0x4ed960, 0}, {0x4f6770, 8}, {0x51e3e0, 8}, {0x51e520, 4},
    {0x4e6c20, 4}, {0x4e6c80, 4}, {0x541fb0, 0}, {0x4ebcb0, 4}, {0x4eb6f0, 4}, {0x4f3d50, 0}, {0x579030, 0}, {0x4f3ba0, 0},
    {0x4163a0, 0}, {0x506f70, 0}, {0x506a10, 4}, {0x5067f0, 4}, {0x506b90, 0xc}, {0x5a01a0, 0}, {0x5a32b0, 4}, {0x5ac330, 4},
    {0x4f7950, 8}, {0x4f8b90, 0x14}, {0x4f8a40, 0x20}, {0x4f88e0, 8}, {0x4f87c0, 0xc}, {0x4f8fb0, 0}, {0x4ed5c0, 0}, {0x41a720, 0x18},
    {0x579e70, 8}, {0x416790, 4}, {0x4fb0a0, 4}, {0x4fb680, 8}, {0x420d50, 8}, {0x59a470, 0}, {0x59a510, 0}, {0x57aa80, 0x14},
    {0x55c4e0, 8}, {0x4eb1c0, 0x10}, {0x5bac80, 0}, {0x59ff80, 0}, {0x55c470, 4}, {0x4ea660, 8}, {0x4ea110, 8}, {0x4ce400, 4},
    {0x470050, 0xc}, {0x4713b0, 8}, {0x4cdf60, 0}, {0x4f93e0, 0}, {0x4f7bf0, 0}, {0x5200d0, 0}, {0x520750, 0}, {0x520f50, 0},
    {0x5a41b0, 4}, {0x4ea350, 8}, {0x57a770, 0x18}, {0x57a100, 0x1c}, {0x57aeb0, 4}, {0x59a140, 4}, {0x59a120, 0}, {0x57a030, 8},
    {0x41c700, 4}, {0x53f000, 0}, {0x4fc880, 4}, {0x4fc870, 0}, {0x4fbf30, 0}, {0x4a35d0, 0}, {0x4a3760, 0}, {0x5209c0, 4},
    {0x520f30, 8},
};
constexpr int NFN = sizeof FNS / sizeof FNS[0];
int fn_index(DWORD address)
{
    for (int i = 0; i < NFN; i++)
        if (FNS[i].address == address)
            return i;
    return -1;
}

struct Patches {
    CodePatch p[NFN + 4];
    int n = 0;
    void on()
    {
        for (int i = 0; i < NFN; i++)
            patch_jump(p[n++], FNS[i].address, reinterpret_cast<void *>(static_cast<uintptr_t>(thunk(T_FN * SLOTS + i))));
        patch_jump(p[n++], 0x5d0863, (void *)&fake_atexit);
    }
    void off()
    {
        while (n > 0)
            unpatch(p[--n]);
    }
} g_patches;

// Stand-in allocations made by the original's pool/heap during a case that the check frees afterwards
struct Block {
    void *p;
    DWORD size;   // 0: heap
    bool keep = false;   // only tagged: freed by the code under test
};
Block g_blocks[32];
int g_nblocks;

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
        if (in(g_part[i], 0x100))
            return 0xa6000000u + ((DWORD)i << 12) + (v - ptrv(g_part[i]));
    for (int i = 0; i < NARRAY; i++)
        if (in(&g_array[i], sizeof(FakeArray)))
            return 0xa7000000u + ((DWORD)i << 12) + (v - ptrv(&g_array[i]));
    if (in(&g_wlayers, sizeof g_wlayers))
        return 0xa8000000u + (v - ptrv(&g_wlayers));
    if (in(&g_temp, sizeof g_temp))
        return 0xa9000000u + (v - ptrv(&g_temp));
    if (in(g_sd, sizeof g_sd))
        return 0xaa000000u + (v - ptrv(g_sd));
    for (int i = 0; i < 8; i++)
        if (in(g_mon[i], 0x20))
            return 0xab000000u + ((DWORD)i << 12) + (v - ptrv(g_mon[i]));
    if (in(g_tile, sizeof g_tile))
        return 0xac000000u + (v - ptrv(g_tile));
    for (int i = 0; i < NLAYER; i++)
        if (in(g_name_buf[i], 32))
            return 0xad000000u + ((DWORD)i << 12) + (v - ptrv(g_name_buf[i]));
    for (int i = 0; i < g_nblocks; i++)
        if (in(g_blocks[i].p, g_blocks[i].size ? g_blocks[i].size : 0x2000))
            return 0xb0000000u + ((DWORD)i << 16) + (v - ptrv(g_blocks[i].p));
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

DWORD g_calls;          // calls so far in this side (part of every recorded value)
BYTE g_mp;              // the game type's slot 12: a multiplayer game
DWORD g_nplayers;
int g_can_read[8];      // reads left per packet object
DWORD g_cfg[16];        // per-case switches the recorders read
CClassInfo *g_part_class[4];

float pick_float(DWORD r)
{
    constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x40000000, 0x3fc00000, 0x40200000, 0x42c80000,
                                0xc2c80000, 0x447a0000, 0x7f800000, 0xff800000, 0xffc00000, 0x00000001, 0x4b000000, 0x3c23d70a, 0x3e4ccccd};
    DWORD b = VALUES[r % (sizeof VALUES / 4)];
    if (r % 7 == 3) {
        const float f = (float)((int)(r >> 8) % 4000 - 1000) / 8.0f;
        memcpy(&b, &f, 4);
    }
    float f;
    memcpy(&f, &b, 4);
    return f;
}

// ---- The recorder ------------------------------------------------------------------------------------------------

int obj_index(const BYTE *p, const void *base, DWORD stride, int n)
{
    for (int i = 0; i < n; i++)
        if (p == static_cast<const BYTE *>(base) + i * stride)
            return i;
    return -1;
}

DWORD array_add(LayerArray *a, DWORD cap, DWORD value)
{
    if (a->count < cap) {
        a->data[a->count] = reinterpret_cast<void *>(static_cast<uintptr_t>(value));
        return a->count++;
    }
    return 0xffffffff;
}
DWORD array_cap(const LayerArray *a)
{
    for (int i = 0; i < NARRAY; i++)
        if (a == &g_array[i].a)
            return 32;
    if (a == &g_wlayers.a || a == &g_temp.a)
        return 32;
    return 0;
}

void fill_temp(DWORD r, DWORD max)
{
    g_temp.a.count = 0;
    const DWORD n = r % (max + 1);
    for (DWORD i = 0; i < n; i++)
        g_temp.a.data[g_temp.a.count++] = g_part[rec_hash(r, i, 7) % NPART];
}

extern "C" DWORD __cdecl wc_record(DWORD *args, DWORD id, BYTE *self, DWORD edx)
{
    const DWORD type = id / SLOTS, slot = id % SLOTS;
    if (g_trace)
        log("        rec %x", id);
    wc_ret_float = 0;
    wc_pop = type == T_FN ? FNS[slot].pop : g_pop[type][slot];
    g_calls++;
    const DWORD me = tagp(self);
    const DWORD r = rec_hash(id, me, g_calls);
    const DWORD code = 0x57000000u + id;
    auto log_args = [&](DWORD strmask = 0) {
        const DWORD n = wc_pop / 4;
        DWORD h = 2166136261u;
        for (DWORD i = 0; i < n && i < 8; i++) {
            const DWORD v = (strmask >> i & 1) ? text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[i]))) : tag(args[i]);
            h = fnv(h, &v, 4);
        }
        ev(code, me, h, n);
    };
    auto ret_float = [&](float f) {
        wc_ret_float = 1;
        wc_fret = f;
        return r;
    };
    auto deleted = [&](BYTE *p) {
        ev(code, me, args[0], 0xde1);
        (void)p;
        return ptrv(self);
    };
    if (type == T_FN) {
        const DWORD a = FNS[slot].address;
        switch (a) {
        // constructors of the stand-in objects: a fake vtable, the block noted for freeing
        case 0x51e360:
        case 0x4a1780:
        case 0x505f00:
        case 0x4a8a90:
        case 0x4e9e00: {
            const DWORD t = a == 0x4e9e00 ? T_LAYER : T_OBJ;
            ev(code, g_nblocks, a == 0x4e9e00 ? tag(args[0]) : 0);
            *reinterpret_cast<DWORD *>(self) = ptrv(g_vt[t]);
            if (g_nblocks < 32)
                g_blocks[g_nblocks++] = {self, a == 0x4e9e00 ? 0x1d0u : a == 0x51e360 ? 0x34u : a == 0x4a1780 ? 0x1cu : a == 0x505f00 ? 0x20u : 0x40u};
            if (a == 0x4e9e00)
                memset(self + 4, 0, 0x1cc);
            return ptrv(self);
        }
        case 0x4713b0:   // the object of slot 38 (its 0x16c-byte block noted for freeing)
            log_args();
            if (g_nblocks < 32)
                g_blocks[g_nblocks++] = {self, 0x16c};
            return ptrv(self);
        case 0x506730:
            ev(code, args[0], args[1]);
            if (g_nblocks < 32)
                g_blocks[g_nblocks++] = {self, 0};
            return ptrv(self);
        case 0x4cd780:
            log_args(1u << 5);
            return ptrv(self);
        case 0x4a4670:   // Ini_GetInt (fastcall)
            ev(code, ptrv(self), edx, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[0]))));
            return r % 3 == 0 ? 0 : r % 3 == 1 ? 1 : r;
        case 0x419ea0:
        case 0x4163a0:
            ev(code, ptrv(self));
            return r;
        case 0x55d030:
            ev(code);
            return r % 2 ? 16 : 8;
        case 0x4a2650:   // DebugLog_SectionBegin (fastcall)
        case 0x4a1540:
            ev(code, text_hash(self));
            return r;
        case 0x4a1570:   // File_CanOpen (fastcall)
            ev(code, text_hash(self), edx);
            return r & 0x101;
        case 0x505e40:   // File_OpenInMemory (fastcall)
            ev(code, text_hash(self), edx);
            return g_cfg[0] % 4 ? ptrv(g_obj[9]) : 0;
        case 0x4eb670: {   // CLayer::FileName: the fake layer's name
            ev(code, me);
            const int i = obj_index(self, g_layer, 0x200, NLAYER);
            return i >= 0 ? ptrv(g_name_buf[i]) : ptrv(g_name_buf[0]);
        }
        case 0x4eb6b0:
            ev(code, me, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[0]))));
            return r;
        case 0x4ebe30:
            ev(code, me, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[0]))), args[1]);
            return r;
        case 0x4ec090:
        case 0x5242f0:
        case 0x579e70:
        case 0x4fc880:
        case 0x5a01a0:
            log_args();
            if (a == 0x5a01a0)
                return r % 3 ? 0xffffffff : r % 5;
            return r % 5 ? (r | 1) : (r & ~0xffu);
        case 0x416790:   // Object_IsKindOf
            log_args();
            return g_cfg[1] == 0 ? (r % 4 ? (r | 1) : (r & ~0xffu)) : g_cfg[1] == 1 ? 1 : 0;
        case 0x4e6c80:   // CLayerSaveData::SetEnvironmentSize: st(0)
            log_args();
            return ret_float(pick_float(r));
        case 0x4f3d50:
            ev(code, me);
            return ptrv(g_obj[10]);
        case 0x4fb0a0: {   // LayerPart_Position
            log_args();
            float *out = reinterpret_cast<float *>(args[0]);
            out[0] = pick_float(r);
            out[1] = pick_float(r >> 5);
            return r;
        }
        case 0x4fb680:   // CLayerPart_MoveCenterToXY: the floats by their bits
            ev(code, me, args[0], args[1]);
            return r;
        case 0x59a470:   // PartList_GetTemp
            ev(code, tagp(self));
            *reinterpret_cast<void **>(self) = &g_temp.a;
            g_temp.a.count = 0;
            return r;
        case 0x57aa80:   // CSortList2D_FindName: some parts
            ev(code, me, tag(args[0]), text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[1]))));
            fill_temp(r, g_cfg[2] % 5);
            return r;
        case 0x57a770:   // CSortList2D_FindRect
            ev(code, me, tag(args[0]), args[1] ? fnv(2166136261u, reinterpret_cast<void *>(static_cast<uintptr_t>(args[1])), 16) : 0);
            ev(code, args[2], args[3] ^ (args[4] << 1), args[5]);
            fill_temp(r, 6);
            return r;
        case 0x57a100:   // CSortList2D_QueryRect
            ev(code, me, tag(args[0]), fnv(2166136261u, reinterpret_cast<void *>(static_cast<uintptr_t>(args[1])), 16));
            ev(code, args[2], args[3], args[4] ^ args[5] ^ args[6]);
            return r;
        case 0x55c4e0:   // random in [lo, hi]
            log_args();
            return args[1] >= args[0] ? args[0] + r % (args[1] - args[0] + 1) : args[0];
        case 0x55c470:
            log_args();
            return args[0] ? r % args[0] : 0;
        case 0x4eb1c0:   // FindBlockerHere
            log_args();
            return r % 3 ? 0 : ptrv(g_part[r % NPART]);
        case 0x5bac80:
            ev(code, me);
            return g_cfg[3] % 3 ? ptrv(g_obj[11]) : 0;
        case 0x59ff80:
            ev(code, me);
            return ptrv(g_gt);
        case 0x4ea110:
        case 0x4ea660: {
            const DWORD *pos = reinterpret_cast<DWORD *>(args[0]);
            ev(code, me, pos[0], pos[1]);
            int *rect = reinterpret_cast<int *>(args[1]);
            for (int k = 0; k < 4; k++)
                rect[k] = (int)(rec_hash(r, k, 3) % 3000) - 500;
            return r;
        }
        case 0x520f50: {   // NetPacket_CanRead
            const int i = obj_index(self, g_obj, 0x80, NOBJ);
            ev(code, me);
            if (i >= 0 && i < 8 && g_can_read[i] > 0) {
                g_can_read[i]--;
                return 1;
            }
            return 0x100;
        }
        case 0x5a41b0: {   // the view list's reserve: data in the check's buffer
            ev(code, me, args[0]);
            DWORD *v = reinterpret_cast<DWORD *>(self);
            if (v[1] != ptrv(s_views)) {
                memcpy(s_views, reinterpret_cast<void *>(static_cast<uintptr_t>(v[1])), v[1] ? v[2] * 8 : 0);
                v[1] = ptrv(s_views);
            }
            v[3] = args[0] < 64 ? args[0] : 64;
            return r;
        }
        case 0x4ea350: {   // CLayer::UpdateViews: the view list
            const DWORD *v = reinterpret_cast<DWORD *>(args[0]);
            ev(code, me, v[2], v[2] && v[2] <= 64 ? fnv(2166136261u, reinterpret_cast<void *>(static_cast<uintptr_t>(v[1])), v[2] * 8) : 0);
            ev(code, args[1]);
            return r;
        }
        case 0x4a35d0:
            ev(code);
            return ptrv(g_obj[12]);
        case 0x5209c0:
            log_args();
            return r % 1000;
        case 0x53f000:
            ev(code);
            memset(self, 0x5a, 16);
            return ptrv(self);
        case 0x5240b0:
            ev(code);
            memset(self, 0x33, 0x24);
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
        case 1: ev(code, me); return ptrv(reinterpret_cast<World *>(g_world)->game_type);
        case 17: ev(code, me, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[0])))); return r;
        case 30:
        case 31:
        case 35: ev(code, me); return g_cfg[4 + (slot & 3)] ? (r | 1) : (r & ~0xffu);
        case 43:
        case 44: ev(code, me); return ptrv(g_players);
        case 53: ev(code, me, args[0], fnv(2166136261u, reinterpret_cast<void *>(static_cast<uintptr_t>(args[1])), 8)); return r % 3 ? (r | 1) : (r & ~0xffu);
        case 54: {
            ev(code, me, args[0]);
            int *rect = reinterpret_cast<int *>(args[1]);
            for (int k = 0; k < 4; k++)
                rect[k] = (int)(rec_hash(r, k, 4) % 2000) - 600 + (k >= 2 ? 900 : 0);
            return r;
        }
        case 62:
        case 63: ev(code, me); return ret_float(pick_float(r));
        case 68: {   // the level path: name, a2, out
            ev(code, me, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[0]))), args[1]);
            char *out = reinterpret_cast<char *>(args[2]);
            const char *name = reinterpret_cast<const char *>(args[0]);
            int k = 0;
            out[k++] = 'L';
            out[k++] = '/';
            for (int j = 0; name && j < 40 && name[j]; j++)
                out[k++] = name[j];
            out[k] = 0;
            return r % 4 ? (r | 1) : (r & ~0xffu);
        }
        case 75:
        case 76:
        case 77: {
            const DWORD strmask = slot == 77 ? 0x6 : slot == 75 ? 0x2 : 0x8;
            const DWORD h = fnv(fnv(2166136261u, &args[0], 4), &args[2], 4);
            ev(code, me, tag(args[0]), strmask & 2 ? text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[1]))) : tag(args[1]));
            ev(code, strmask & 4 ? text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[2]))) : tag(args[2]),
               strmask & 8 ? text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[3]))) : (args[3] & 0xff));
            (void)h;
            return g_cfg[8] % 4 ? (r | 1) : (r & ~0xffu);
        }
        case 82: {
            const float *off = reinterpret_cast<const float *>(args[3]);
            ev(code, me, tag(args[0]) ^ tag(args[1]) * 3, tag(args[2]));
            ev(code, off ? fnv(2166136261u, off, 8) : 0, tag(args[3]));
            return r;
        }
        case 99: {
            ev(code, me);
            int *out = reinterpret_cast<int *>(args[0]);
            out[0] = 100 + (int)(r % 900);
            out[1] = 100 + (int)((r >> 10) % 700);
            return r;
        }
        case 107:
            ev(code, me, tag(args[0]), args[1]);
            ev(code, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[2]))), args[3]);
            return r;
        default: log_args(); return r;
        }
    case T_PLAYERS:
        switch (slot) {
        case 11: ev(code); return g_nplayers;
        case 12:
        case 13: ev(code, args[0]); return ptrv(g_obj[args[0] < 8 ? args[0] : 7]);
        case 22:
        case 23: ev(code, args[0]); return ptrv(g_rec[args[0] % NREC]);
        default: log_args(); return r;
        }
    case T_GT:
        switch (slot) {
        case 12: ev(code); return g_mp ? (r | 1) : (r & ~0xffu);
        case 13:
        case 36: ev(code); return r % 2 ? (r | 1) : (r & ~0xffu);
        default: log_args(); return r;
        }
    case T_OBJ:
        switch (slot) {
        case 0:
        case 1: return deleted(self);
        case 17: ev(code, me, text_hash(reinterpret_cast<void *>(static_cast<uintptr_t>(args[0])))); return r;
        case 39:
        case 45: ev(code, me); return g_cfg[9] % 3 ? 480 + (r % 3) * 120 : r % 5000;
        default: log_args(); return r;
        }
    case T_PART:
        switch (slot) {
        case 1: return deleted(self);
        case 2: ev(code, me); return ptrv(g_part_class[(r ^ g_cfg[10]) % 4]);
        case 43: {
            ev(code, me);
            float *out = reinterpret_cast<float *>(args[0]);
            out[0] = pick_float(r);
            out[1] = pick_float(r >> 7);
            return r;
        }
        case 52:
        case 53: ev(code, me); return (r % 4000) - 1000;
        case 64: log_args(); return r % 3 ? ptrv(g_obj[13]) : 0;
        default: log_args(); return r;
        }
    case T_LAYER:
        if (slot == 1)
            return deleted(self);
        log_args();
        return r;
    case T_ARRAY: {
        LayerArray *a = reinterpret_cast<LayerArray *>(self);
        if (slot == 1) {
            ev(code, me, a->count);
            a->count = 0;
            return r;
        }
        if (slot == 2) {
            const DWORD v = *reinterpret_cast<DWORD *>(args[0]);
            ev(code, me, tag(v));
            return array_add(a, array_cap(a), v);
        }
        log_args();
        return r;
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
        {T_WORLD, 17, 4},  {T_WORLD, 23, 8},  {T_WORLD, 52, 8},  {T_WORLD, 53, 8},  {T_WORLD, 54, 8},  {T_WORLD, 55, 4},   {T_WORLD, 68, 12},
        {T_WORLD, 69, 8},  {T_WORLD, 70, 12}, {T_WORLD, 75, 16}, {T_WORLD, 76, 16}, {T_WORLD, 77, 16}, {T_WORLD, 78, 8},   {T_WORLD, 82, 16},
        {T_WORLD, 89, 4},  {T_WORLD, 96, 4},  {T_WORLD, 99, 4},  {T_WORLD, 102, 4}, {T_WORLD, 103, 8}, {T_WORLD, 104, 8},  {T_WORLD, 107, 16},
        {T_PLAYERS, 12, 4}, {T_PLAYERS, 13, 4}, {T_PLAYERS, 22, 4}, {T_PLAYERS, 23, 4}, {T_PLAYERS, 27, 4},
        {T_GT, 0, 4},      {T_GT, 17, 8},
        {T_OBJ, 0, 4},     {T_OBJ, 1, 4},     {T_OBJ, 17, 4},    {T_OBJ, 26, 4},    {T_OBJ, 27, 8},
        {T_PART, 1, 4},    {T_PART, 25, 8},   {T_PART, 43, 4},   {T_PART, 58, 4},   {T_PART, 62, 4},   {T_PART, 64, 8},
        {T_LAYER, 1, 4},   {T_ARRAY, 2, 4},
    };
    for (const auto &p : P)
        g_pop[p.t][p.s] = (DWORD)p.pop;
    g_part_class[0] = at<CClassInfo *(__cdecl *)()>(0x487620)();
    g_part_class[1] = at<CClassInfo *(__cdecl *)()>(0x55d2a0)();
    g_part_class[2] = at<CClassInfo *(__cdecl *)()>(0x4b5c60)();
    g_part_class[3] = at<CClassInfo *(__cdecl *)()>(0x49a930)();
}

bool thunks_ok()
{
    for (DWORD k = 0; k < 1100; k++) {
        const BYTE *th = reinterpret_cast<const BYTE *>(wc_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != k || th[5] != 0xe9)
            return false;
    }
    return true;
}

// ---- Setup of the fakes ----------------------------------------------------------------------------------------------

const char *const NAMES[] = {"Test/Nachbau1", "ACT 1/M1M2/01 Main", "test/NACHBAU1", "Eigene/Level 1", "Levels/x.y.zax", ""};

void setup()
{
    World *w = reinterpret_cast<World *>(g_world);
    memset(g_world, 0, sizeof g_world);
    for (DWORD i = 8; i < 0x280; i += 4)
        *reinterpret_cast<DWORD *>(g_world + i) = random() % 4 ? random() : 0;
    w->vftable = ptrv(g_vt[T_WORLD]);
    w->display = g_obj[0];
    *reinterpret_cast<void **>(g_world + 0x34) = random() % 3 ? g_obj[14] : nullptr;   // the base's +0x34 (0x4cdf30)
    w->timers = g_obj[1];
    w->holder_vftable = VT_CHARACTER_HOLDER;
    w->characters.vftable = VT_CHARACTER_ARRAY;
    w->characters.data = g_array[3].items;
    w->characters.count = random() % 5;
    w->characters.capacity = 32;
    for (DWORD i = 0; i < 32; i++)
        g_array[3].items[i] = random() % 6 ? g_part[random() % NPART] : nullptr;
    w->flag29c = (BYTE)(random() % 2);
    w->game_type = g_gt;
    w->zoom_frame = g_obj[2];
    w->screen_frame = g_obj[3];
    w->file_buffer = g_obj[4];
    w->layer_holder[0] = VT_LAYER_HOLDER;
    w->layers.vftable = ptrv(g_vt[T_ARRAY]);
    w->layers.data = g_wlayers.items;
    w->layers.count = random() % 6;
    w->layers.capacity = 32;
    for (DWORD &g : g_wlayers.guard)
        g = random();
    for (DWORD i = 0; i < 32; i++)
        g_wlayers.items[i] = random() % 4 ? g_layer[random() % NLAYER] : nullptr;
    w->unique_id = random();
    w->playing_time = (double)(random() % 100000) / 7.0;
    w->running_time = (double)(random() % 100000) / 3.0;
    w->rules = g_obj[5];
    for (int &v : w->view)
        v = (int)(random() % 1000);
    w->time = pick_float(random());
    w->byte2f8 = (BYTE)random();
    w->show_polygons = (BYTE)random();
    w->show_grid = (BYTE)random();
    for (int i = 0; i < 4; i++)
        w->loading_frames[i] = random() % 2 ? g_obj[6 + i % 2] : nullptr;
    w->loading = random() % 3 ? 0 : random() % 3;
    w->running = (BYTE)(random() % 2);
    w->level_loaded = (BYTE)(random() % 2);
    w->saved_characters.vftable = ptrv(g_vt[T_ARRAY]);
    w->saved_characters.data = g_array[2].items;
    w->saved_characters.count = random() % 3;
    w->saved_characters.capacity = 32;
    w->mission_time = (double)(random() % 100000) / 11.0;
    w->saving = (BYTE)(random() % 2);
    w->traffic_graph = nullptr;
    w->mixer = g_obj[8];
    *reinterpret_cast<DWORD *>(g_obj[8] + 0x24) = random();
    // the net layer, the players, the game type
    memset(g_players, 0, sizeof g_players);
    *reinterpret_cast<DWORD *>(g_players) = ptrv(g_vt[T_PLAYERS]);
    g_nplayers = random() % 5;
    for (int i = 0; i < NREC; i++) {
        for (DWORD k = 0; k < 0x100; k += 4)
            *reinterpret_cast<DWORD *>(g_rec[i] + k) = random();
        *reinterpret_cast<float *>(g_rec[i] + 0xc0) = (float)((int)(random() % 4000) - 500) + (float)(random() % 4) / 4.0f;
        *reinterpret_cast<float *>(g_rec[i] + 0xc4) = (float)((int)(random() % 4000) - 500) + (float)(random() % 4) / 4.0f;
        *reinterpret_cast<int *>(g_rec[i] + 0xdc) = random() % 4 ? (int)(random() % 7) - 1 : (int)random();
        *reinterpret_cast<float *>(g_rec[i] + 0xd8) = pick_float(random());
    }
    memset(g_gt, 0, sizeof g_gt);
    *reinterpret_cast<DWORD *>(g_gt) = ptrv(g_vt[T_GT]);
    g_mp = (BYTE)(random() % 2);
    for (int i = 0; i < NOBJ; i++) {
        for (DWORD k = 4; k < 0x80; k += 4)
            *reinterpret_cast<DWORD *>(g_obj[i] + k) = random();
        *reinterpret_cast<DWORD *>(g_obj[i]) = ptrv(g_vt[T_OBJ]);
    }
    // rules: +8 and +0xc ZStrings are set by the cases that need them (real ZStrings)
    for (int i = 0; i < NLAYER; i++) {
        BYTE *l = g_layer[i];
        for (DWORD k = 4; k < 0x200; k += 4)
            *reinterpret_cast<DWORD *>(l + k) = random() % 3 ? random() : 0;
        *reinterpret_cast<DWORD *>(l) = ptrv(g_vt[T_LAYER]);
        *reinterpret_cast<void **>(l + 0x7c) = random() % 2 ? g_obj[15] : nullptr;   // tile map
        *reinterpret_cast<void **>(l + 0x3c + 0x160) = random() % 2 ? g_obj[12] : nullptr;   // music mix
        *reinterpret_cast<void **>(l + 0xcc) = random() % 2 ? g_obj[13] : nullptr;          // first load action
        // teams: data +0xe4, count +0xe8 (entries with two ZStrings at +8/+0xc are set where needed)
        *reinterpret_cast<void **>(l + 0xe4) = g_array[1].items;
        *reinterpret_cast<DWORD *>(l + 0xe8) = 0;
        const char *n = NAMES[random() % 6];
        DWORD k = 0;
        for (; n[k] && k < 31; k++)
            g_name_buf[i][k] = (BYTE)n[k];
        g_name_buf[i][k] = 0;
    }
    for (int i = 0; i < NPART; i++) {
        BYTE *p = g_part[i];
        for (DWORD k = 4; k < 0x100; k += 4)
            *reinterpret_cast<DWORD *>(p + k) = random();
        *reinterpret_cast<DWORD *>(p) = ptrv(g_vt[T_PART]);
        *reinterpret_cast<void **>(p + 0x48) = random() % 3 ? g_layer[random() % NLAYER] : nullptr;
        *reinterpret_cast<DWORD *>(p + 0x24) = random() % 4 ? random() % 4 : 0xffffffff;
        *reinterpret_cast<WORD *>(p + 0x28) = (WORD)(random() % 3 ? random() : 0);
    }
    for (int i = 0; i < NARRAY; i++) {
        g_array[i].a.vftable = ptrv(g_vt[T_ARRAY]);
        g_array[i].a.data = g_array[i].items;
        g_array[i].a.count = i == 3 ? g_array[i].a.count : random() % 5;
        g_array[i].a.capacity = 32;
        if (i < 2)
            for (DWORD k = 0; k < 32; k++)
                g_array[i].items[k] = random() % 5 ? g_part[random() % NPART] : nullptr;
    }
    g_temp.a.vftable = ptrv(g_vt[T_ARRAY]);
    g_temp.a.data = g_temp.items;
    g_temp.a.count = 0;
    for (int &c : g_can_read)
        c = (int)(random() % 4);
    for (DWORD &c : g_cfg)
        c = random();
    g_calls = 0;
}

const char *zs_text(void *z) { return static_cast<ZString *>(z)->buffer ? static_cast<ZString *>(z)->c_str() : ""; }

// The state of all fakes, pointers by number
void state()
{
    for (DWORD i = 0; i < 0x338; i += 4)
        st(tag(*reinterpret_cast<DWORD *>(g_world + i)));
    for (int i = 0; i < NREC; i++)
        st(fnv(2166136261u, g_rec[i], 0x100));
    for (int i = 0; i < NLAYER; i++)
        for (DWORD k = 0; k < 0x200; k += 4)
            st(tag(*reinterpret_cast<DWORD *>(g_layer[i] + k)));
    for (int i = 0; i < NPART; i++)
        for (DWORD k = 0; k < 0x100; k += 4)
            st(tag(*reinterpret_cast<DWORD *>(g_part[i] + k)));
    for (int i = 0; i < NOBJ; i++)
        st(fnv(2166136261u, g_obj[i], 0x80));
    auto array_state = [](const FakeArray &a) {
        st(a.a.count);
        st(tag(a.a.vftable));
        st(tagp(a.a.data));
        for (DWORD g : a.guard)
            st(tag(g));
        for (const void *p : a.items)
            st(tagp(p));
    };
    for (const FakeArray &a : g_array)
        array_state(a);
    array_state(g_wlayers);
    array_state(g_temp);
    st(*at<BYTE *>(LOADING_SCREEN));
    st(*at<DWORD *>(LOADING_STEPS));
    st(*at<DWORD *>(UPDATE_TIME));
    st(tag(*at<DWORD *>(LOADING_FILE)));
    st(tag(*at<DWORD *>(G_WORLD)));
    st(text_hash(zs_text(at<void *>(LAST_FILE))));
    st(fnv(2166136261u, at<void *>(GAME_RANDOM), GAME_RANDOM_BYTES));
}

// ---- Globals the cases change, put back after each side ----------------------------------------------------------

SavedBytes g_globals[16];
int g_nglobals;
void save_global(DWORD address, DWORD n) { save_bytes(g_globals[g_nglobals++], address, n); }
ZString g_last_saved;

void globals_save()
{
    g_nglobals = 0;
    save_global(GAME_RANDOM, GAME_RANDOM_BYTES);
    save_global(LOADING_SCREEN, 1);
    save_global(LOADING_STEPS, 4);
    save_global(UPDATE_TIME, 4);
    save_global(LOADING_FILE, 4);
    save_global(VIEW_LIST, 16);
    save_global(0x6c2780, 1);
    save_global(0x6c02cc, 4);
    save_global(0x6c1d0c, 4);
    save_global(0x6c1cf5, 1);
    at<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(&g_last_saved, at<void *>(LAST_FILE));
}
void globals_restore()
{
    for (int i = 0; i < g_nglobals; i++)
        restore_bytes(g_globals[i]);
    at<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(at<void *>(LAST_FILE), &g_last_saved);
}

void free_blocks()
{
    for (int i = 0; i < g_nblocks; i++)
        if (g_blocks[i].keep)
            continue;
        else if (g_blocks[i].size)
            pool_free(g_blocks[i].p, g_blocks[i].size);
        else
            heap_free(g_blocks[i].p);
    g_nblocks = 0;
}

// ---- Two sides ----------------------------------------------------------------------------------------------------------

World *W() { return reinterpret_cast<World *>(g_world); }

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
        if (g_trace)
            log("      setup");
        if (side == 0)
            hooks_out();
        if (g_trace)
            log("      hooks %d", side);
        g_patches.on();
        if (g_trace)
            log("      patches");
        r[side] = run(mask);
        g_patches.off();
        if (g_trace)
            log("      run");
        if (side == 0)
            hooks_in();
        state();
        free_blocks();
        globals_restore();
        at<void(THISCALL *)(void *)>(0x4defd0)(&g_last_saved);
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
    t.check(same, "world class", d);
    return same;
}

// An st(0) result as a dword hash of its 80 bits
DWORD ld_bits(LD v)
{
    BYTE b[10];
    __asm__("fstpt %0" : "=m"(b) : "t"(v) : "st");
    return fnv(2166136261u, b, 10);
}

template <typename R, typename... P> struct CallOf;
template <typename R, typename... P> struct CallOf<R(THISCALL *)(P...)> {
    template <typename... A> static DWORD run(DWORD address, A... a)
    {
        if constexpr (__is_same(R, void)) {
            at<R(THISCALL *)(P...)>(address)(a...);
            return 0;
        } else {
            return (DWORD)at<R(THISCALL *)(P...)>(address)(a...);
        }
    }
};
template <typename R, typename... P> struct CallOf<R(__cdecl *)(P...)> {
    template <typename... A> static DWORD run(DWORD address, A... a)
    {
        at<R(__cdecl *)(P...)>(address)(a...);
        return 0;
    }
};
template <typename F, typename... A> DWORD call(DWORD address, A... a) { return CallOf<F>::run(address, a...); }

void copy_text(char *out, const char *s, int max)
{
    int i = 0;
    for (; s[i] && i < max - 1; i++)
        out[i] = s[i];
    out[i] = 0;
}
bool same_text(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

// A real ZString built in place (the rules' map names, the teams' spawn names)
void zs_set(void *z, const char *s)
{
    at<void *(THISCALL *)(void *)>(0x4dec90)(z);
    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s);
}
void zs_free(void *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }

// ---- The cases --------------------------------------------------------------------------------------------------------

using Fw = World;

void world_methods()
{
    Tally t{"world class: layers, players, slots"};
    unsigned seed = 1000;
    const int N = scaled(160);
    auto rnd_layer = []() -> Layer * { return random() % 5 ? reinterpret_cast<Layer *>(g_layer[random() % NLAYER]) : nullptr; };
    for (int k = 0; k < N; k++) {
        // accessors
        two_sides(t, "CurrentLayer", seed++, [](DWORD &) { return call<Layer *(THISCALL *)(World *)>(0x4f1050, W()); });
        two_sides(t, "PlayerLayer", seed++, [](DWORD &) { return call<Layer *(THISCALL *)(World *, DWORD)>(0x4f10a0, W(), random() % 6); });
        two_sides(t, "FindLayer", seed++, [](DWORD &) {
            const char *const keys[] = {"Test/Nachbau1", "TEST/NACHBAU1.zax", "test/nachbau1.way", "Eigene/Level 1", "Levels/x.y", "", "nix"};
            return call<Layer *(THISCALL *)(World *, const char *)>(0x4f4d70, W(), keys[random() % 7]);
        });
        two_sides(t, "AddLayer", seed++, [&](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(World *, Layer *)>(0x4f4650, W(), rnd_layer());
            return 0u;
        });
        two_sides(t, "UnloadLayer", seed++, [&](DWORD &m) {
            m = 0;
            Layer *l = random() % 4 ? reinterpret_cast<Layer *>(W()->layers.count ? W()->layers.data[random() % W()->layers.count] : nullptr)
                                    : rnd_layer();
            call<void(THISCALL *)(World *, Layer *)>(0x4f3490, W(), l);
            return 0u;
        });
        two_sides(t, "LoadLayer", seed++, [](DWORD &) {
            return call<Layer *(THISCALL *)(World *, const char *, DWORD)>(0x4f45b0, W(), NAMES[random() % 6], random() % 2);
        });
        two_sides(t, "SaveLayer", seed++, [&](DWORD &) {
            return call<DWORD(THISCALL *)(World *, Layer *, DWORD, DWORD)>(0x4f4380, W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]),
                                                                           random() % 2, random() % 3);
        });
        two_sides(t, "LevelExists", seed++,
                  [](DWORD &) { return call<DWORD(THISCALL *)(World *, const char *)>(0x4f4ca0, W(), NAMES[random() % 6]); });
        two_sides(t, "LevelPath", seed++, [](DWORD &m) {
            m = 0xff;
            char out[300];
            const DWORD r = call<DWORD(THISCALL *)(World *, const char *, DWORD, char *)>(0x4f3a40, W(), NAMES[random() % 6], 0, out);
            st(text_hash(out));
            return r;
        });
        two_sides(t, "SaveLayerFile", seed++, [](DWORD &m) {
            m = 0xff;
            return call<DWORD(THISCALL *)(World *, Layer *, const char *, DWORD, DWORD)>(0x4f4330, W(), reinterpret_cast<Layer *>(g_layer[1]),
                                                                                         "Levels/a.zax", random() % 3, random());
        });
        two_sides(t, "LoadLayerStream", seed++, [](DWORD &m) {
            m = 0xff;
            return call<DWORD(THISCALL *)(World *, Layer *, void *, DWORD, const char *)>(0x4f4500, W(), reinterpret_cast<Layer *>(g_layer[2]),
                                                                                          g_obj[9], random() % 2 ? 0x101 : 0x200, NAMES[random() % 6]);
        });
        two_sides(t, "LoadLayerFile", seed++, [](DWORD &m) {
            m = 0xff;
            return call<DWORD(THISCALL *)(World *, Layer *, const char *, const char *, DWORD)>(
                0x4f43f0, W(), reinterpret_cast<Layer *>(g_layer[3]), "Levels/Test/Nachbau1.zax", NAMES[random() % 6], random() % 2 ? 1 : 0x300);
        });
        two_sides(t, "SetPlayerLayer", seed++, [&](DWORD &m) {
            m = 0;
            const DWORD i = random() % 3 ? random() % 4 : 0xffffffff;
            call<void(THISCALL *)(World *, Layer *, DWORD)>(0x4f4700, W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]), i);
            return 0u;
        });
        two_sides(t, "UpdateTileMaps", seed++, [](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(World *)>(0x4f2560, W());
            return 0u;
        });
        two_sides(t, "Remove/AddPart", seed++, [&](DWORD &m) {
            m = 0;
            call<DWORD(THISCALL *)(World *, void *, Layer *)>(0x4f4e30, W(), g_part[random() % NPART], rnd_layer());
            call<DWORD(THISCALL *)(World *, void *, Layer *)>(0x4f4e50, W(), g_part[random() % NPART], rnd_layer());
            return 0u;
        });
        two_sides(t, "LoadingFile", seed++, [](DWORD &) {
            struct {
                void *layer;
                const char *path;
                ZString name;
                BYTE flag;
            } info;
            zs_set(&info.name, NAMES[random() % 6]);
            info.layer = g_layer[random() % 2];
            info.flag = (BYTE)random();
            *at<void **>(LOADING_FILE) = random() % 4 ? &info : nullptr;
            DWORD h = call<DWORD(THISCALL *)(World *, void *)>(0x4f4d30, W(), g_layer[random() % 2]);
            if (*at<void **>(LOADING_FILE)) {
                h = h * 3 + text_hash(call<const char *(THISCALL *)(World *)>(0x4f4d50, W()) ? at<const char *(THISCALL *)(World *)>(0x4f4d50)(W()) : "");
                h = h * 3 + call<DWORD(THISCALL *)(World *)>(0x4f4d60, W());   // all of eax (the info pointer's upper bytes)
            }
            *at<void **>(LOADING_FILE) = nullptr;
            zs_free(&info.name);
            return h;
        });
        // game flow
        two_sides(t, "Begin/EndLoading", seed++, [](DWORD &m) {
            m = 0;
            for (DWORD j = random() % 4; j < 4; j++)
                call<void(THISCALL *)(World *)>(random() % 2 ? 0x4f3cc0 : 0x4f3d30, W());
            return 0u;
        });
        two_sides(t, "GameStarted/EndGame", seed++, [](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(World *)>(0x4f5110, W());
            call<void(THISCALL *)(World *, DWORD)>(0x4f5150, W(), 7);
            return 0u;
        });
        two_sides(t, "Update", seed++, [](DWORD &m) {
            m = 0;
            *at<float *>(UPDATE_TIME) = random() % 3 ? (float)(random() % 300) / 10.0f : pick_float(random());
            call<void(THISCALL *)(World *, float)>(0x4f3560, W(), random() % 4 ? (float)(random() % 1000) / 1000.0f : pick_float(random()));
            return 0u;
        });
        two_sides(t, "UpdateViews", seed++, [](DWORD &m) {
            m = 0;
            if (random() % 3 == 0)
                *at<BYTE *>(0x6c2780) &= ~1;   // the static not built yet (atexit replaced)
            call<void(THISCALL *)(World *, float)>(0x4f37e0, W(), (float)(random() % 1000) / 1000.0f);
            return 0u;
        });
        two_sides(t, "WriteState", seed++, [](DWORD &m) {
            m = 0;
            // Object_WriteBinary/Text on the embedded holders, recorded through code patches
            CodePatch p[2];
            patch_jump(p[0], 0x416f50, reinterpret_cast<void *>(static_cast<uintptr_t>(thunk(T_OBJ * SLOTS + 100))));
            patch_jump(p[1], 0x4168f0, reinterpret_cast<void *>(static_cast<uintptr_t>(thunk(T_OBJ * SLOTS + 101))));
            g_pop[T_OBJ][100] = g_pop[T_OBJ][101] = 4;
            call<DWORD(THISCALL *)(World *, void *, DWORD)>(0x4f3b40, W(), g_obj[9], random() % 2 ? 1 : 0x100);
            unpatch(p[1]);
            unpatch(p[0]);
            return 0u;
        });
        // players and characters
        two_sides(t, "Characters", seed++, [](DWORD &) {
            DWORD h = call<DWORD(THISCALL *)(World *, DWORD)>(0x4f5d60, W(), random() % 6);
            h = h * 7 + call<DWORD(THISCALL *)(World *)>(0x4f5d80, W());
            return tag(h);
        });
        two_sides(t, "DeleteCharacters", seed++, [](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(World *)>(0x4f4ce0, W());
            return 0u;
        });
        two_sides(t, "Take/PutCharacters", seed++, [](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(World *, Layer *, LayerArray *)>(0x4f5270, W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]), &g_array[0].a);
            g_cfg[1] = 1;   // IsKindOf true (a null entry would be called through)
            for (DWORD i = 0; i < 32; i++)
                if (!g_array[1].items[i])
                    g_array[1].items[i] = g_part[i % NPART];
            call<void(THISCALL *)(World *, Layer *, LayerArray *, DWORD)>(0x4f52f0, W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]),
                                                                           &g_array[1].a, random() % 2 ? 1 : 0x100);
            return 0u;
        });
        two_sides(t, "PlaceAt", seed++, [](DWORD &m) {
            m = 0;
            float off[2] = {pick_float(random()), pick_float(random())};
            call<void(THISCALL *)(World *, void *, Layer *, void *, const float *)>(0x4f5360, W(), g_part[0], nullptr,
                                                                                    random() % 3 ? g_part[1] : nullptr, random() % 2 ? off : nullptr);
            return 0u;
        });
        two_sides(t, "MoveToSpawnPoint", seed++, [](DWORD &m) {
            m = 0;
            const char *const names[] = {"Game Start", "$0,0", "$0,0x", "", nullptr, "cheater"};
            float off[2] = {pick_float(random()), (float)(random() % 100)};
            call<void(THISCALL *)(World *, void *, Layer *, const char *, const float *)>(0x4f53f0, W(), g_part[2],
                                                                                          reinterpret_cast<Layer *>(g_layer[0]), names[random() % 6],
                                                                                          random() % 2 ? off : nullptr);
            return 0u;
        });
        two_sides(t, "MoveToTeamSpawn", seed++, [](DWORD &m) {
            m = 0;
            BYTE *l = g_layer[1];
            BYTE team[3][0x10];
            const DWORD n = random() % 4;
            for (DWORD i = 0; i < 3; i++) {
                zs_set(team[i] + 8, i == 1 ? "" : "Spawn A");
                zs_set(team[i] + 0xc, i == 2 ? "" : "MP Spawn");
                g_array[1].items[i] = team[i];
            }
            *reinterpret_cast<DWORD *>(l + 0xe8) = n;
            const char *const names[] = {"Game Start", "", nullptr};
            call<void(THISCALL *)(World *, void *, Layer *, const char *, const float *)>(0x4f4950, W(), g_part[3], reinterpret_cast<Layer *>(l),
                                                                                          names[random() % 3], nullptr);
            for (auto &tm : team) {
                zs_free(tm + 8);
                zs_free(tm + 0xc);
            }
            for (DWORD i = 0; i < 3; i++)
                g_array[1].items[i] = nullptr;
            return 0u;
        });
        two_sides(t, "MoveToSpawnHere/All", seed++, [](DWORD &m) {
            m = 0;
            *reinterpret_cast<void **>(g_part[4] + 0x48) = g_layer[0];
            call<void(THISCALL *)(World *, void *, const char *, const float *)>(0x4f4ac0, W(), g_part[4], "Game Start", nullptr);
            // MoveAllToSpawns: names as ZStrings, offsets as float pairs, layers
            ZString names[4];
            float offs[8];
            for (int i = 0; i < 4; i++) {
                zs_set(&names[i], i % 2 ? "Game Start" : "");
                offs[i * 2] = (float)i;
                offs[i * 2 + 1] = -(float)i;
            }
            LayerArray ln = {ptrv(g_vt[T_ARRAY]), reinterpret_cast<void **>(names), 4, 4};
            LayerArray lo = {ptrv(g_vt[T_ARRAY]), reinterpret_cast<void **>(offs), 4, 4};
            void *ls[4] = {g_layer[0], g_layer[1], g_layer[2], g_layer[0]};
            LayerArray ll = {ptrv(g_vt[T_ARRAY]), ls, 4, 4};
            g_array[0].a.count = 4;
            call<void(THISCALL *)(World *, LayerArray *, LayerArray *, LayerArray *, LayerArray *)>(0x4f4a40, W(), &g_array[0].a, &ln, &lo, &ll);
            for (auto &z : names)
                zs_free(&z);
            return 0u;
        });
        two_sides(t, "Sync", seed++, [](DWORD &) {
            const DWORD i = random() % 3;
            DWORD h = tag(call<DWORD(THISCALL *)(World *, DWORD)>(0x4f3410, W(), random() % 4));
            call<void(THISCALL *)(World *, DWORD)>(0x4f3430, W(), i);
            return h;
        });
        two_sides(t, "Player values", seed++, [](DWORD &) {
            float out[2];
            const DWORD i = random() % 5;
            DWORD h = call<DWORD(THISCALL *)(World *, float *, DWORD)>(0x4f5da0, W(), out, i);
            h = fnv(h, out, 8);
            h = h * 3 + ld_bits(at<LD(THISCALL *)(World *, DWORD)>(0x4f6960)(W(), i));
            h = h * 3 + call<DWORD(THISCALL *)(World *, DWORD)>(0x4f6750, W(), i);
            h = h * 3 + call<DWORD(THISCALL *)(World *)>(0x4f6740, W());
            return h;
        });
        two_sides(t, "View rects", seed++, [](DWORD &) {
            int r[4] = {0, 0, 0, 0};
            const DWORD i = random() % 4;
            DWORD h = call<int(THISCALL *)(World *, DWORD, int *)>(0x4f0710, W(), i, r);
            call<void(THISCALL *)(World *, DWORD, int *)>(0x4f3740, W(), i, r);
            float pos[2] = {(float)((int)(random() % 3000) - 700) + 0.5f, random() % 6 ? (float)((int)(random() % 3000) - 700) : pick_float(random())};
            if (pos[1] != pos[1] || pos[1] > 2e9f || pos[1] < -2e9f)
                pos[1] = 0.25f;
            h = h * 3 + (call<DWORD(THISCALL *)(World *, DWORD, const float *)>(0x4f36e0, W(), i, pos) & 0xff);
            return h;
        });
        two_sides(t, "Broadcasts", seed++, [](DWORD &m) {
            m = 0;
            void *saved = *at<void **>(G_WORLD);
            *at<void **>(G_WORLD) = W();
            *at<DWORD *>(0x6c1d0c) = random() % 4 == 0;
            g_cfg[1] = 0;
            call<void(THISCALL *)(World *, DWORD)>(0x4f6070, W(), random() % 5 ? random() % 50 : 0xffffffff);
            call<void(THISCALL *)(World *, DWORD, DWORD, void *, DWORD, DWORD)>(0x4f60e0, W(), 1, 2, g_part[5], 4, 5);
            float pt[2] = {pick_float(random()), 12.5f};
            call<void(THISCALL *)(World *, DWORD, DWORD, const float *, DWORD, DWORD, DWORD)>(0x4f61c0, W(), 1, 2, pt, 4, 5, 6);
            call<void(THISCALL *)(World *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4f6280, W(), 1, 2, random(), random(), 5, 6, 7);
            *at<DWORD *>(0x6c1d0c) = 0;
            *at<void **>(G_WORLD) = saved;
            return 0u;
        });
        two_sides(t, "RegisterReaders", seed++, [](DWORD &m) {
            m = 0;
            if (random() % 3 == 0)
                W()->game_type = nullptr;
            call<void(THISCALL *)(World *, void *, void *)>(0x4f6a90, W(), g_obj[1], g_obj[2]);
            return 0u;
        });
        // small slots
        two_sides(t, "Small slots", seed++, [](DWORD &) {
            DWORD h = tag(call<DWORD(THISCALL *)(World *)>(0x4f0220, W()));
            h = h * 3 + call<DWORD(THISCALL *)(World *)>(0x4f0a20, W());
            h = h * 3 + (call<DWORD(THISCALL *)(World *)>(0x4f0a30, W()) & 0xff);
            h = h * 3 + (call<DWORD(THISCALL *)(World *, DWORD, DWORD, DWORD, DWORD)>(0x4f0a50, W(), 1, 2, 3, 4) & 0xff);
            h = h * 3 + (call<DWORD(THISCALL *)(World *)>(0x4f0a60, W()) & 0xff);
            h = h * 3 + ld_bits(at<LD(THISCALL *)(World *)>(0x4f0aa0)(W()));
            h = h * 3 + call<DWORD(THISCALL *)(World *, DWORD)>(0x4f0ab0, W(), 5);
            h = h * 3 + call<DWORD(THISCALL *)(World *, DWORD)>(0x4f25a0, W(), random());
            BYTE obj[0x200];
            memset(obj, 0, sizeof obj);
            obj[0x1bc] = (BYTE)(random() % 2);
            h = h * 3 + call<DWORD(THISCALL *)(World *, void *, DWORD)>(0x4f25b0, W(), obj, random());
            h = h * 3 + ld_bits(at<LD(THISCALL *)(World *)>(0x4f2600)(W()));
            h = h * 3 + ld_bits(at<LD(THISCALL *)(World *)>(0x4f2610)(W()));
            h = h * 3 + call<DWORD(THISCALL *)(World *, DWORD)>(0x4f27b0, W(), random());
            DWORD out[2] = {7, 7};
            h = h * 3 + tag(call<DWORD(THISCALL *)(World *, DWORD *)>(0x4f27e0, W(), out)) + out[0] * 5 + out[1];
            h = h * 3 + (call<DWORD(THISCALL *)(World *, DWORD)>(0x4f31c0, W(), random() % 2 ? 0x1b : random() % 300) & 0xff);
            const DWORD floats[] = {0x4f39f0u, 0x4f3a00u, 0x4f3a10u, 0x4f3a20u, 0x4f3a30u};
            for (DWORD a : floats)
                h = h * 3 + ld_bits(at<LD(THISCALL *)(World *)>(a)(W()));
            h = h * 3 + call<DWORD(THISCALL *)(World *, DWORD)>(0x4f6040, W(), random());
            return h;
        });
        two_sides(t, "Slot38", seed++, [](DWORD &) {
            const DWORD r = call<DWORD(THISCALL *)(World *)>(0x4f3170, W());
            // the 0x16c-byte block the original allocates is handed to 0x4713b0 (recorded); free it
            return r;
        });
        two_sides(t, "Random_Next/DeleteAll", seed++, [](DWORD &) {
            BYTE gen[0x200];
            memcpy(gen, at<void *>(GAME_RANDOM), sizeof gen);
            *reinterpret_cast<DWORD *>(gen + 0x10) = random() % 55;
            *reinterpret_cast<DWORD *>(gen + 0x14) = random() % 55;
            DWORD h = 0;
            for (int j = 0; j < 5; j++)
                h = h * 31 + call<DWORD(THISCALL *)(BYTE *)>(0x4f50d0, gen);
            st(fnv(2166136261u, gen, sizeof gen));
            call<void(THISCALL *)(LayerArray *)>(0x4f6b40, &g_array[random() % 3].a);
            return h;
        });
    }
    t.report();
}

// StartNewGame and FinishLevelStart on the fake world
void world_start()
{
    Tally t{"world class: StartNewGame, FinishLevelStart"};
    unsigned seed = 50000;
    for (int k = 0; k < scaled(150); k++) {
        two_sides(t, "StartNewGame", seed++, [](DWORD &m) {
            m = 0;
            void *saved_world = *at<void **>(G_WORLD);
            *at<void **>(G_WORLD) = W();
            zs_set(W()->rules + 8, "Test/Nachbau1");
            zs_set(W()->rules + 0xc, random() % 2 ? "Game Start" : "");
            const bool given = random() % 2;
            g_cfg[0] = 1;   // the file opens
            g_cfg[8] = 1;   // and the layer loads (the original reads the layer's default game type without a null check)
            call<void(THISCALL *)(World *, DWORD, const char *, const char *, DWORD)>(0x4f4e70, W(), random() % 3, given ? "Eigene/Level 1" : nullptr,
                                                                                      given ? "Spawn" : nullptr, random());
            zs_free(W()->rules + 8);
            zs_free(W()->rules + 0xc);
            *at<void **>(G_WORLD) = saved_world;
            return 0u;
        });
        two_sides(t, "FinishLevelStart", seed++, [](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(World *, Layer *)>(0x4f4890, W(), reinterpret_cast<Layer *>(g_layer[random() % NLAYER]));
            return 0u;
        });
    }
    t.report();
}

// The constructor on patterned blocks and the destructor on worlds with pool and heap blocks
void world_ctor_dtor()
{
    Tally t{"world class: constructor, destructor"};
    unsigned seed = 60000;
    for (int k = 0; k < scaled(120); k++) {
        two_sides(t, "Ctor", seed++, [](DWORD &) {
            BYTE *block = g_ctor_block;
            const BYTE pattern = (BYTE)(random() % 2 ? 0xcd : 0x11);
            memset(block, pattern, sizeof g_ctor_block);
            *reinterpret_cast<void **>(block + 4) = g_obj[0];   // the display (set by the base constructor)
            void *saved_world = *at<void **>(G_WORLD);
            const DWORD r = call<World *(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4f0780, block, random(), random(), random()) == ptrv(block);
            st(tag(*at<DWORD *>(G_WORLD)) == 0 ? 1 : *at<DWORD *>(G_WORLD) == ptrv(block));
            *at<void **>(G_WORLD) = saved_world;
            for (DWORD i = 0; i < 0x338; i += 4) {
                const DWORD v = *reinterpret_cast<DWORD *>(block + i);
                st(v == ptrv(block) + 0 ? 0x77777777 : tag(v));
            }
            return r;
        });
        two_sides(t, "Dtor", seed++, [](DWORD &m) {
            m = 0;
            World *w = W();
            // blocks the destructor frees
            const DWORD cap = 4 + random() % 4;
            w->layers.data = static_cast<void **>(pool_get_alloc(cap * 4));
            w->layers.capacity = cap;
            w->layers.count = random() % (cap + 1);
            for (DWORD i = 0; i < cap; i++)
                w->layers.data[i] = random() % 4 ? g_layer[random() % NLAYER] : nullptr;
            w->saved_characters.data = static_cast<void **>(pool_get_alloc(32 * 4));
            w->saved_characters.capacity = 32;
            const DWORD cc = random() % 4;
            void **chars = static_cast<void **>(heap_alloc(8 * 4));
            for (DWORD i = 0; i < 8; i++)
                chars[i] = g_part[random() % NPART];
            w->characters.data = chars;
            w->characters.count = cc;
            w->characters.capacity = 8;
            w->traffic_graph = random() % 2 ? heap_alloc(0x40) : nullptr;
            w->timers = random() % 3 ? heap_alloc(0x100) : nullptr;
            const Block tagged[] = {{w->layers.data, cap * 4, true}, {w->saved_characters.data, 0x80, true}, {chars, 0x20, true},
                                    {w->traffic_graph, 0x40, true}, {w->timers, 0x100, true}};
            for (const Block &b : tagged)
                if (b.p)
                    g_blocks[g_nblocks++] = b;
            void *saved_world = *at<void **>(G_WORLD);
            call<void(THISCALL *)(World *)>(0x4f0c70, w);
            st(*at<DWORD *>(G_WORLD));
            st(tagp(w->layers.data));
            st(tagp(w->saved_characters.data));
            st(tagp(w->characters.data));
            st(tagp(w->traffic_graph));
            st(tagp(w->timers));
            *at<void **>(G_WORLD) = saved_world;
            // the pointers the destructor freed read as 0 in the state; others are freed here
            w->layers.data = g_wlayers.items;
            w->saved_characters.data = g_array[2].items;
            w->characters.data = g_array[3].items;
            return 0u;
        });
    }
    t.report();
}

// The level update on fake save data
void layer_update_cases()
{
    Tally t{"layer update: first update, world update, flag messages, begin, rectangles"};
    unsigned seed = 70000;
    auto setup_sd = []() {
        memset(g_sd, 0, sizeof g_sd);
        LayerSaveData *sd = reinterpret_cast<LayerSaveData *>(g_sd);
        sd->begun = (BYTE)(random() % 2);
        sd->tile_map = g_tile;
        *reinterpret_cast<int *>(g_tile + 0x50) = 1 << (4 + random() % 4);
        *reinterpret_cast<int *>(g_tile + 0x54) = 1 << (4 + random() % 4);
        *reinterpret_cast<DWORD *>(sd->fog) = ptrv(g_vt[T_OBJ]);
        sd->layer = g_layer[0];
        sd->byte18c = (BYTE)(random() % 2);
        const DWORD n = random() % 8;
        sd->monitor.vftable = ptrv(g_vt[T_ARRAY]);
        sd->monitor.data = g_array[2].items;
        sd->monitor.count = n;
        for (DWORD i = 0; i < 8; i++) {
            BYTE *e = g_mon[i];
            memset(e, 0, 0x20);
            *reinterpret_cast<void **>(e + 8) = random() % 5 ? g_part[random() % NPART] : nullptr;
            *reinterpret_cast<int *>(e + 0xc) = random() % 3 ? (int)(random() % 200) : 0;
            *reinterpret_cast<int *>(e + 0x10) = random() % 3 ? (int)(random() % 200) : 0;
            g_array[2].items[i] = e;
        }
        for (int i = 0; i < NPART; i++) {
            DWORD &f = *reinterpret_cast<DWORD *>(g_part[i] + 0x1c);
            f = (random() % 2 ? 0x800000 : 0) | (random() % 2 ? 0x40000000 : 0) | (random() % 3 ? 0 : 0x20000000) | (random() & 0x3f0000);
        }
        return sd;
    };
    for (int k = 0; k < scaled(200); k++) {
        two_sides(t, "FirstUpdate", seed++, [&](DWORD &m) {
            m = 0;
            void *saved_world = *at<void **>(G_WORLD);
            *at<void **>(G_WORLD) = W();
            call<void(THISCALL *)(LayerSaveData *)>(0x4e71b0, setup_sd());
            st(fnv(2166136261u, g_sd, sizeof g_sd));
            *at<void **>(G_WORLD) = saved_world;
            return 0u;
        });
        two_sides(t, "WorldUpdate", seed++, [&](DWORD &m) {
            m = 0;
            void *saved_world = *at<void **>(G_WORLD);
            *at<void **>(G_WORLD) = W();
            LayerSaveData *sd = setup_sd();
            int rects[4][4];
            for (auto &r : rects)
                for (int &v : r)
                    v = (int)(random() % 2000) - 300;
            LayerArray views = {ptrv(g_vt[T_ARRAY]), reinterpret_cast<void **>(rects), random() % 5, 4};
            call<void(THISCALL *)(LayerSaveData *, LayerArray *, float)>(0x4e74a0, sd, &views, (float)(random() % 100) / 100.0f);
            st(fnv(2166136261u, g_sd, sizeof g_sd));
            *at<void **>(G_WORLD) = saved_world;
            return 0u;
        });
        two_sides(t, "OnPartFlags", seed++, [&](DWORD &m) {
            m = 0;
            void *saved_world = *at<void **>(G_WORLD);
            *at<void **>(G_WORLD) = W();
            BYTE *p = g_part[random() % NPART];
            *reinterpret_cast<DWORD *>(p + 0x1c) = random() % 2 ? 0x20000000 | (random() & 0x40ff0000) : random() & 0x1fff0000;
            const DWORD bits[] = {0x10000, 0x20000, 0x200000, 0x400000, 0x800000, 0x30000};
            call<void(THISCALL *)(LayerSaveData *, BYTE *, DWORD, DWORD)>(0x4e76e0, setup_sd(), p, random() % 2 ? 0x200000 : random() & 0xff0000,
                                                                          bits[random() % 6] | (random() % 3 ? 0 : random() & 0xff0000));
            *at<void **>(G_WORLD) = saved_world;
            return 0u;
        });
        two_sides(t, "SendPartFlags", seed++, [&](DWORD &m) {
            m = 0;
            call<void(THISCALL *)(LayerSaveData *, void *)>(0x4e77e0, setup_sd(), g_obj[3]);
            return 0u;
        });
        two_sides(t, "Begin", seed++, [&](DWORD &m) {
            m = 0;
            void *saved_world = *at<void **>(G_WORLD);
            *at<void **>(G_WORLD) = W();
            LayerSaveData *sd = setup_sd();
            call<void(THISCALL *)(LayerSaveData *)>(0x4e7870, sd);
            st(sd->byte18c);
            *at<void **>(G_WORLD) = saved_world;
            return 0u;
        });
        two_sides(t, "WriteRect", seed++, [&](DWORD &m) {
            m = 0;
            int rect[4];
            for (int &v : rect)
                v = (int)(random() % 3000) - 500;
            *at<BYTE *>(0x6c1cf5) ^= (BYTE)(random() % 2);
            call<void(THISCALL *)(LayerSaveData *, void *, DWORD, const int *)>(0x4e79b0, setup_sd(), g_obj[4], random(), rect);
            return 0u;
        });
        two_sides(t, "StaticPartArray_DeleteAll", seed++, [&](DWORD &m) {
            m = 0;
            SavedBytes s;
            save_bytes(s, STATIC_PART_ARRAY, 16);
            LayerArray *a = at<LayerArray *>(STATIC_PART_ARRAY);
            a->data = g_array[0].items;
            a->count = random() % 6;
            for (DWORD i = 0; i < 6; i++)
                g_array[0].items[i] = random() % 4 ? g_part[random() % NPART] : nullptr;
            call<void(__cdecl *)()>(0x4e7b70);
            st(a->count);
            restore_bytes(s);
            return 0u;
        });
    }
    t.report();
}

// ---- Real data: read-only accessors of the running world --------------------------------------------------------------

template <typename F, typename... A> void same_call(Tally &t, DWORD address, const char *what, DWORD mask, A... a)
{
    if (g_trace)
        log("    real %s", what);
    const DWORD o = (DWORD)original<F>(address)(a...) & mask;
    const DWORD r = (DWORD)at<F>(address)(a...) & mask;
    char d[160];
    wsprintfA(d, "[0x%x %s: original %x, reimpl %x]", address, what, o, r);
    t.check(o == r, "real world", d);
}

void real_world()
{
    Tally t{"world class: real data"};
    World *w = *at<World **>(G_WORLD);
    if (!w || w->vftable != VT_ZAX_WORLD) {
        log("  world class: no world (vtable %x)", w ? w->vftable : 0);
        t.check(false, "real world", "no world");
        t.report();
        return;
    }
    Layer *cur = original<Layer *(THISCALL *)(World *)>(0x4f1050)(w);
    log("  world class: %u layers, current %s, %u characters, playing time %d s, unique id %x", w->layers.count,
        cur ? at<const char *(THISCALL *)(Layer *)>(0x4eb670)(cur) : "-", w->characters.count, (int)w->playing_time, w->unique_id);
    same_call<Layer *(THISCALL *)(World *)>(t, 0x4f1050, "CurrentLayer", ~0u, w);
    same_call<Layer *(THISCALL *)(World *, DWORD)>(t, 0x4f10a0, "PlayerLayer", ~0u, w, 0u);   // (only player 0 exists)
    char names[6][128];
    int nn = 0;
    if (cur) {
        const char *n = at<const char *(THISCALL *)(Layer *)>(0x4eb670)(cur);
        copy_text(names[nn++], n, 100);
        copy_upper(names[nn++], n, 100);
        wsprintfA(names[nn++], "%.100s.zax", n);
        wsprintfA(names[nn++], "%.100s.way.zax", n);
    }
    copy_text(names[nn++], "Levels/none", 100);
    copy_text(names[nn++], "", 100);
    for (int i = 0; i < nn; i++)
        same_call<Layer *(THISCALL *)(World *, const char *)>(t, 0x4f4d70, "FindLayer", ~0u, w, (const char *)names[i]);
    for (DWORD i = 0; i < 4; i++)
        same_call<void *(THISCALL *)(World *, DWORD)>(t, 0x4f5d60, "Character", ~0u, w, i);
    same_call<void *(THISCALL *)(World *)>(t, 0x4f5d80, "FirstCharacter", ~0u, w);
    same_call<DWORD(THISCALL *)(World *, DWORD)>(t, 0x4f6750, "PlayerEntity", ~0u, w, 0u);
    same_call<DWORD(THISCALL *)(World *)>(t, 0x4f6740, "LocalEntity", ~0u, w);
    same_call<void *(THISCALL *)(World *)>(t, 0x4f0220, "GameType", ~0u, w);
    same_call<DWORD(THISCALL *)(World *)>(t, 0x4f0a60, "Saving", 0xff, w);
    same_call<DWORD(THISCALL *)(World *)>(t, 0x4f0a30, "Slot61", 0xff, w);
    if (g_trace)
        log("    real floats");
    {
        char d[120];
        const DWORD z0 = ld_bits(original<LD(THISCALL *)(World *, DWORD)>(0x4f6960)(w, 0));
        const DWORD z1 = ld_bits(at<LD(THISCALL *)(World *, DWORD)>(0x4f6960)(w, 0));
        wsprintfA(d, "[Zoom %x / %x]", z0, z1);
        t.check(z0 == z1, "real world", d);
        const DWORD floats[] = {0x4f2600u, 0x4f2610u, 0x4f39f0u, 0x4f3a00u, 0x4f3a10u, 0x4f3a20u, 0x4f3a30u, 0x4f0aa0u};
        for (DWORD a : floats) {
            const DWORD o = ld_bits(original<LD(THISCALL *)(World *)>(a)(w));
            const DWORD r = ld_bits(at<LD(THISCALL *)(World *)>(a)(w));
            wsprintfA(d, "[0x%x st(0) %x / %x]", a, o, r);
            t.check(o == r, "real world", d);
        }
        if (g_trace)
            log("    real view");
        float o[2], r[2];
        const DWORD eo = original<DWORD(THISCALL *)(World *, float *, DWORD)>(0x4f5da0)(w, o, 0);
        const DWORD er = at<DWORD(THISCALL *)(World *, float *, DWORD)>(0x4f5da0)(w, r, 0);
        t.check(eo == er && !memcmp(o, r, 8), "real world", "[ViewTarget]");
        int ro[4], rr[4];
        original<void(THISCALL *)(World *, DWORD, int *)>(0x4f3740)(w, 0, ro);
        at<void(THISCALL *)(World *, DWORD, int *)>(0x4f3740)(w, 0, rr);
        wsprintfA(d, "[PlayerViewRect %d %d %d %d / %d %d %d %d]", ro[0], ro[1], ro[2], ro[3], rr[0], rr[1], rr[2], rr[3]);
        t.check(!memcmp(ro, rr, 16), "real world", d);
        const int ao = original<int(THISCALL *)(World *, DWORD, int *)>(0x4f0710)(w, 0, ro);
        const int ar = at<int(THISCALL *)(World *, DWORD, int *)>(0x4f0710)(w, 0, rr);
        t.check(ao == ar && !memcmp(ro, rr, 16), "real world", "[PlayerViewRectAt]");
        for (int i = 0; i < 40; i++) {
            const float p[2] = {(float)(ro[0] - 100 + (int)(random() % (DWORD)(ro[2] - ro[0] + 200 > 0 ? ro[2] - ro[0] + 200 : 1))),
                                (float)(ro[1] - 100 + (int)(random() % (DWORD)(ro[3] - ro[1] + 200 > 0 ? ro[3] - ro[1] + 200 : 1)))};
            same_call<DWORD(THISCALL *)(World *, DWORD, const float *)>(t, 0x4f36e0, "PlayerSees", 0xff, w, 0u, p);
        }
    }
    char out0[300], out1[300];
    if (g_trace)
        log("    real paths");
    for (int i = 0; i < nn; i++) {
        const DWORD r0 = original<DWORD(THISCALL *)(World *, const char *, DWORD, char *)>(0x4f3a40)(w, names[i], 0, out0) & 0xff;
        const DWORD r1 = at<DWORD(THISCALL *)(World *, const char *, DWORD, char *)>(0x4f3a40)(w, names[i], 0, out1) & 0xff;
        t.check(r0 == r1 && same_text(out0, out1), "real world", out0);
        same_call<DWORD(THISCALL *)(World *, const char *)>(t, 0x4f4ca0, "LevelExists", 0xff, w, (const char *)names[i]);
    }
    log("  world class: level path %s", out0);
    // SendPartFlags on the running level: the block writer recorded
    if (g_trace)
        log("    real SendPartFlags");
    if (cur) {
        unsigned h[2];
        int n[2];
        for (int side = 0; side < 2; side++) {
            g_side = side;
            g_loghash[side] = 2166136261u;
            g_nlog[side] = 0;
            g_calls = 0;
            CodePatch p;
            patch_jump(p, 0x4f88e0, reinterpret_cast<void *>(static_cast<uintptr_t>(thunk(T_FN * SLOTS + fn_index(0x4f88e0)))));
            if (side == 0)
                original<void(THISCALL *)(LayerSaveData *, void *)>(0x4e77e0)(&cur->data, g_obj[3]);
            else
                at<void(THISCALL *)(LayerSaveData *, void *)>(0x4e77e0)(&cur->data, g_obj[3]);
            unpatch(p);
            h[side] = g_loghash[side];
            n[side] = g_nlog[side];
        }
        char d[100];
        wsprintfA(d, "[SendPartFlags: %d events %x / %d events %x]", n[0], h[0], n[1], h[1]);
        t.check(h[0] == h[1] && n[0] == n[1], "real world", d);
        log("  world class: SendPartFlags on the level: %d blocks", n[0]);
    }
    t.report();
}

}  // namespace

ZAX_CHECKS(world_class)
{
    (void)cls;
    (void)n;
    if (!thunks_ok()) {
        log("  world class: recorder thunks are broken");
        return;
    }
    init_vtables();
    world_methods();
    world_start();
    world_ctor_dtor();
    real_world();
}

ZAX_CHECKS(layer_update)
{
    (void)cls;
    (void)n;
    if (!thunks_ok())
        return;
    init_vtables();
    layer_update_cases();
}

}  // namespace zax
