// Checks of palette_rest4b.cpp against the original, in the game thread (machinery: checks_inventory_common.h:
// side 0 with the module's hooks out, operator new and free on an arena per side filled with the same
// pattern, so every block the functions allocate or free compares by its offset; the arena is compared
// whole, the palette objects with it).
//  - The object functions on objects of every kind (0-8) with and without data: Init, Assign (also from
//    itself and from an empty one), Differs (equal, one byte apart, other kinds), SetKind, Set/GetEntry,
//    Load and Save on a file in the temp folder (whole, short, missing; the written file compared).
//  - RgbToHsl and HueValue on byte colours, random values, edges (0, 60, 180, 240, 360, negatives, NaN):
//    the outputs' bits and st0's ten bytes.
//  - Nearest: kind 1 with the game palette 0x633f98 (copied) and random ones over random ranges (empty
//    too), kinds 5 and 7 with random data, kind 6 filled on demand from a source, the other kinds.
//  - The tables: the lightness remap (factors, 0x80, ranges, a target or none), the gamma table (factor 0
//    and in (0, 1]), the amplify and hue blend tables, the lookups of kinds 5-7 and others; each once on
//    the game palette and otherwise on a source of kind 5 whose first 0x300 bytes are the palette (its
//    Nearest a lookup, so the 65,536 searches of a table stay cheap).
//  - DrawPalette_Build on random 64 x 64 x 64 histograms (single cells, sparse cells, clusters) with 1-255
//    colours: the quantisation 0x54ff70 with its box cut and box functions (its globals 0x712258-0x712268
//    saved, compared and put back); BoxBottom and BoxVolume directly on random boxes.
#include "checks_inventory_common.h"
#include "x87.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);

namespace zax {

int hooks_palette_rest4b(Hook *out);

namespace {

using namespace invcheck;
using LD = long double;

struct PalObj {
    DWORD data, kind, source;
};

DWORD kind_bytes(DWORD kind)
{
    switch (kind) {
    case 1: return 0x300;
    case 2: return 0x100;
    case 3: return 0x400;
    case 4:
    case 7: return 0x10000;
    case 5:
    case 6: return 0x8000;
    default: return 0;
    }
}

DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }

BYTE g_game_palette[0x300];

// A block of the side's arena
BYTE *block(DWORD n) { return static_cast<BYTE *>(arena_new(n)); }
// A palette object in the arena: kind, data (none, or a block of the kind's size, at least 4 bytes) filled
// with the game palette, a pattern or random bytes
enum Fill { F_PALETTE, F_RANDOM, F_ZERO };
PalObj *make(DWORD kind, bool with_data, Fill fill)
{
    PalObj *p = reinterpret_cast<PalObj *>(block(sizeof(PalObj)));
    p->kind = kind;
    p->source = 0;
    p->data = 0;
    if (with_data) {
        const DWORD n = kind_bytes(kind) < 0x300 ? 0x300 : kind_bytes(kind);
        BYTE *d = block(n);
        for (DWORD i = 0; i < n; i++)
            d[i] = fill == F_ZERO ? 0 : fill == F_PALETTE && i < 0x300 ? g_game_palette[i] : (BYTE)(random() >> 13);
        p->data = ptrv(d);
    }
    return p;
}
// A source for the tables: the game palette (kind 1) or a kind 5 object whose first 0x300 bytes are it
PalObj *make_source(bool real)
{
    PalObj *p = make(real ? 1 : 5, true, F_PALETTE);
    if (!real) {
        BYTE *d = at<BYTE *>(p->data);
        for (DWORD i = 0x300; i < 0x8000; i++)
            d[i] = (BYTE)(1 + rnd(254));
    }
    return p;
}

const char *g_what;
DWORD g_r[2];
BYTE g_fp[2][16];
DWORD g_globals[2][5];
char g_detail[200];

// Both sides: setup and call in f(side) (between side_begin and side_end), results in g_r/g_fp; then the
// results, the arena and the extra words compared
template <typename F> void two_sides(Tally &t, const char *what, unsigned seed, F f)
{
    g_what = what;
    memset(g_fp, 0, sizeof g_fp);
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        arena_on(side, 0x5b);
        g_r[side] = rel(f(side), side);
        arena_off();
    }
    // the arena up to the larger use (beyond it the arena is only filled up to the previous cases' high mark)
    const DWORD n = g_used[0] > g_used[1] ? g_used[0] : g_used[1];
    const bool arena_same = same_region(g_arena[0], g_arena[1], n);
    const bool same = g_r[0] == g_r[1] && arena_same && !memcmp(g_fp[0], g_fp[1], sizeof g_fp[0]) &&
                      !memcmp(g_globals[0], g_globals[1], sizeof g_globals[0]);
    wsprintfA(g_detail, "[%s seed %u: eax %x / %x, arena %s (at %x: %x / %x), used %x / %x]", what, seed, g_r[0], g_r[1], arena_same ? "same" : "DIFFERS",
              g_diff_at, g_diff_a, g_diff_b, g_used[0], g_used[1]);
    t.check(same && !g_overflow, "", g_detail);
    g_overflow = false;
}

template <typename R, typename... A> DWORD call(int side, DWORD address, A... a)
{
    side_begin(side);
    // eax as a dword (garbage for the void functions, whose callers ignore it)
    const DWORD r = at<DWORD(THISCALL *)(A...)>(address)(a...);
    (void)sizeof(R *);
    side_end(side);
    return r;
}

using Nearest = DWORD(THISCALL *)(PalObj *, DWORD, DWORD, DWORD, DWORD, DWORD);

// ---- The object functions ------------------------------------------------------------------------------

DWORD some_kind() { return rnd(5) ? 1 + rnd(7) : rnd(2) ? 0 : 8; }

void check_objects(Tally &t)
{
    for (int k = 0; k < scaled(600); k++) {
        const unsigned seed = 0x526700u + (unsigned)k;
        switch (k % 6) {
        case 0:
            two_sides(t, "Init", seed, [&](int side) {
                BYTE *b = block(16);
                return call<void *>(side, 0x5266d0, b);
            });
            break;
        case 1:
            two_sides(t, "Assign", seed, [&](int side) {
                PalObj *a = make(some_kind(), rnd(4) != 0, F_RANDOM);
                PalObj *b = rnd(8) == 0 ? a : make(rnd(4) ? a->kind : some_kind(), rnd(6) != 0, F_RANDOM);
                call<void>(side, 0x526700, a, b);
                return ptrv(a);
            });
            break;
        case 2:
            two_sides(t, "Differs", seed, [&](int side) {
                const DWORD kind = some_kind();
                PalObj *a = make(kind, true, F_RANDOM);
                PalObj *b = make(rnd(5) ? kind : some_kind(), true, F_ZERO);
                const DWORD na = kind_bytes(a->kind) < 0x300 ? 0x300 : kind_bytes(a->kind), nb = kind_bytes(b->kind) < 0x300 ? 0x300 : kind_bytes(b->kind);
                memcpy(at<void *>(b->data), at<void *>(a->data), na < nb ? na : nb);
                if (rnd(2) && kind_bytes(kind))
                    at<BYTE *>(b->data)[rnd(kind_bytes(kind))] ^= (BYTE)(1 + rnd(255));
                return call<DWORD>(side, 0x526870, a, b);
            });
            break;
        case 3:
            two_sides(t, "SetKind", seed, [&](int side) {
                PalObj *a = make(some_kind(), rnd(3) != 0, F_RANDOM);
                const DWORD kind = rnd(3) ? a->kind : some_kind();
                call<void>(side, 0x5268f0, a, kind);
                return ptrv(a);
            });
            break;
        case 4:
            two_sides(t, "SetEntry", seed, [&](int side) {
                PalObj *a = make(1, true, F_RANDOM);
                call<void>(side, 0x526fd0, a, rnd(256), rnd(256), rnd(256), rnd(256) | (rnd(2) ? 0xab00 : 0));
                return ptrv(a);
            });
            break;
        default:
            two_sides(t, "GetEntry", seed, [&](int side) {
                PalObj *a = make(1, true, F_RANDOM);
                DWORD *out = reinterpret_cast<DWORD *>(block(12));
                out[0] = out[1] = out[2] = 0xcdcdcdcd;
                call<void>(side, 0x527000, a, rnd(256), out, out + 1, out + 2);
                return ptrv(a);
            });
        }
    }
}

// ---- Load and Save ---------------------------------------------------------------------------------------

char g_path[MAX_PATH];
DWORD file_hash()
{
    HANDLE f = CreateFileA(g_path, 0x80000000u, FILE_SHARE_READ, nullptr, 3, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return 0xdead;
    static_assert(sizeof(DWORD) == 4);
    BYTE buf[0x1000];
    DWORD h = 2166136261u, got = 0, total = 0;
    while (ReadFile(f, buf, sizeof buf, &got, nullptr) && got) {
        h = fnv(h, buf, got);
        total += got;
    }
    CloseHandle(f);
    return fnv(h, &total, 4);
}
void write_test_file(DWORD n, unsigned seed)
{
    HANDLE f = CreateFileA(g_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    g_seed = seed;
    BYTE buf[0x400];
    while (n) {
        const DWORD m = n < sizeof buf ? n : sizeof buf;
        for (DWORD i = 0; i < m; i++)
            buf[i] = (BYTE)(random() >> 11);
        DWORD w;
        WriteFile(f, buf, m, &w, nullptr);
        n -= m;
    }
    CloseHandle(f);
}

void check_files(Tally &t)
{
    char tmp[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp)) {
        t.check(false, "", "[no temp folder]");
        return;
    }
    wsprintfA(g_path, "%sZaxPaletteCheck.pal", tmp);
    for (int k = 0; k < scaled(60); k++) {
        const unsigned seed = 0x5269e0u + (unsigned)k;
        if (k % 2 == 0) {
            const int how = k % 6;   // 0 whole, 2 short, 4 missing
            const DWORD kind = 1 + (DWORD)(k / 6) % 7;
            DWORD file[2] = {};
            two_sides(t, "Load", seed, [&](int side) {
                DeleteFileA(g_path);
                if (how != 4)
                    write_test_file(how == 0 ? kind_bytes(kind) : kind_bytes(kind) / 3, seed);
                g_seed = seed + 99;
                PalObj *a = make(rnd(2) ? kind : some_kind(), rnd(2) != 0, F_RANDOM);
                const DWORD r = call<DWORD>(side, 0x5269e0, a, g_path, kind);
                file[side] = file_hash();
                g_fp[side][0] = (BYTE)file[side];
                return r;
            });
        } else {
            two_sides(t, "Save", seed, [&](int side) {
                DeleteFileA(g_path);
                PalObj *a = make(1 + rnd(7), true, F_RANDOM);
                const DWORD r = call<DWORD>(side, 0x526b80, a, g_path);
                const DWORD h = file_hash();
                memcpy(g_fp[side], &h, 4);
                return r;
            });
        }
    }
    DeleteFileA(g_path);
}

// ---- RgbToHsl, HueValue --------------------------------------------------------------------------------

float some_unit()
{
    switch (rnd(10)) {
    case 0: return 0.0f;
    case 1: return 1.0f;
    case 2: return -(float)rnd(1000) / 997.0f;
    case 3: return 1.0f + (float)rnd(1000) / 997.0f;
    case 4: return raw_float(0x7fc00000);
    default: return fstp(fild((int)rnd(256)) * cst(0x5f172c));
    }
}
float some_hue()
{
    const float edges[] = {0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f, 360.0f, -0.0f, -120.0f, 480.0f, 720.0f, -400.0f};
    switch (rnd(4)) {
    case 0: return edges[rnd(sizeof edges / sizeof edges[0])];
    case 1: return rnd(10) ? -360.0f + (float)rnd(108000) / 100.0f : raw_float(0xffc00000);
    default: return (float)rnd(36000) / 100.0f;
    }
}

void check_hsl(Tally &t)
{
    for (int k = 0; k < scaled(3000); k++) {
        const unsigned seed = 0x526c40u + (unsigned)k;
        two_sides(t, "RgbToHsl", seed, [&](int side) {
            float *o = reinterpret_cast<float *>(block(16));
            for (int i = 0; i < 4; i++)
                o[i] = raw_float(0x12345678u + (DWORD)i);
            const float r = some_unit(), g = rnd(4) ? some_unit() : r, b = rnd(4) ? some_unit() : g;
            side_begin(side);
            at<void(__fastcall *)(float *, float *, float, float, float, float *)>(0x526c40)(o, o + 1, r, g, b, o + 2);
            side_end(side);
            return 0u;
        });
    }
    for (int k = 0; k < scaled(3000); k++) {
        const unsigned seed = 0x526da0u + (unsigned)k;
        two_sides(t, "HueValue", seed, [&](int side) {
            const float p = some_unit(), q = some_unit(), h = some_hue();
            side_begin(side);
            const LD v = at<LD(__stdcall *)(float, float, float)>(0x526da0)(p, q, h);
            side_end(side);
            memcpy(g_fp[side], &v, 10);
            return 0u;
        });
    }
}

// ---- Nearest ------------------------------------------------------------------------------------------------

void check_nearest(Tally &t)
{
    for (int k = 0; k < scaled(4000); k++) {
        const unsigned seed = 0x526e40u + (unsigned)k;
        two_sides(t, "Nearest", seed, [&](int side) {
            const DWORD kind = k % 3 == 0 ? 1 : rnd(3) ? 5 + rnd(3) : some_kind();
            PalObj *a = make(kind, true, rnd(3) ? F_PALETTE : F_RANDOM);
            if (kind == 6) {
                PalObj *src = make(1, true, rnd(2) ? F_PALETTE : F_RANDOM);
                a->source = ptrv(src);
                BYTE *d = at<BYTE *>(a->data);
                for (DWORD i = 0; i < 0x8000; i++)
                    if (rnd(3) == 0)
                        d[i] = 0;
            }
            const DWORD first = rnd(4) ? rnd(3) : rnd(256);
            const DWORD end = rnd(4) ? 0xff + rnd(2) : first + rnd(257 - first);
            const DWORD r = rnd(256), g = rnd(8) ? rnd(256) : r, b = rnd(256);
            return call<DWORD>(side, 0x526e40, a, r, g, b, first, end);
        });
    }
}

// ---- The tables -------------------------------------------------------------------------------------------

void check_tables(Tally &t)
{
    for (int k = 0; k < scaled(300); k++) {
        const unsigned seed = 0x527040u + (unsigned)k;
        two_sides(t, "BuildLightnessRemap", seed, [&](int side) {
            PalObj *a = make(rnd(3) ? 2 : some_kind(), rnd(3) != 0, F_RANDOM);
            PalObj *src = make(1, true, rnd(4) ? F_PALETTE : F_RANDOM);
            PalObj *target = rnd(3) ? nullptr : make_source(rnd(2) != 0);
            const DWORD factor = rnd(6) == 0 ? 0x80 : rnd(4) ? rnd(256) : rnd(0x1000);
            const DWORD first = rnd(3) ? rnd(3) : rnd(256), end = rnd(3) ? 0xff + rnd(2) : first + rnd(257 - first);
            const DWORD lo = rnd(3) ? 1 : rnd(64), hi = rnd(3) ? 0xff : lo + rnd(257 - lo);
            call<void>(side, 0x527040, a, src, factor, random(), random(), first, end, lo, hi, target);
            return ptrv(a);
        });
    }
    for (int k = 0; k < 8; k++) {
        const unsigned seed = 0x527700u + (unsigned)k;
        two_sides(t, "BuildGammaTable", seed, [&](int side) {
            PalObj *a = make(rnd(2) ? 4 : some_kind(), rnd(2) != 0, F_RANDOM);
            PalObj *src = make_source(k == 0 || k == 1);
            const float factor = k % 2 == 0 ? 0.0f : k == 7 ? 1.0f : (float)(1 + rnd(1000)) / 1000.0f;
            call<void>(side, 0x527700, a, src, factor);
            return ptrv(a);
        });
    }
    for (int k = 0; k < 4; k++) {
        const unsigned seed = 0x5277b0u + (unsigned)k;
        two_sides(t, "BuildAmplifyTable", seed, [&](int side) {
            PalObj *a = make(rnd(2) ? 4 : some_kind(), rnd(2) != 0, F_RANDOM);
            call<void>(side, 0x5277b0, a, make_source(k == 0));
            return ptrv(a);
        });
    }
    for (int k = 0; k < 4; k++) {
        const unsigned seed = 0x527980u + (unsigned)k;
        two_sides(t, "BuildHueBlendTable", seed, [&](int side) {
            PalObj *a = make(rnd(2) ? 4 : some_kind(), rnd(2) != 0, F_RANDOM);
            call<void>(side, 0x527980, a, make_source(k == 0));
            return ptrv(a);
        });
    }
    for (int k = 0; k < scaled(60); k++) {
        const unsigned seed = 0x527da0u + (unsigned)k;
        two_sides(t, "BuildLookup", seed, [&](int side) {
            const DWORD kind = k % 4 == 3 ? some_kind() : 5 + (DWORD)k % 4;
            PalObj *a = make(rnd(2) ? kind : some_kind(), rnd(2) != 0, F_RANDOM);
            PalObj *src = make_source(k < 3);
            const DWORD lo = k < 3 ? 1 : rnd(64), hi = k < 3 ? 0xff : lo + rnd(9);
            call<void>(side, 0x527da0, a, src, kind, lo, hi);
            return ptrv(a);
        });
    }
    for (int k = 0; k < 4; k++) {
        const unsigned seed = 0x527d50u + (unsigned)k;
        two_sides(t, "KindBytes", seed, [&](int side) {
            DWORD r = 0;
            side_begin(side);
            for (DWORD kind = 0; kind < 12; kind++)
                r = r * 31 + at<DWORD(__fastcall *)(DWORD)>(0x527d50)(kind);
            r = r * 31 + at<DWORD(__fastcall *)(DWORD)>(0x527d50)(0xffffffffu - (DWORD)k);
            side_end(side);
            return r;
        });
    }
}

// ---- DrawPalette_Build ------------------------------------------------------------------------------------

int *g_hist;

void check_build(Tally &t)
{
    if (!g_hist)
        g_hist = static_cast<int *>(VirtualAlloc(nullptr, 0x40000 * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!g_hist) {
        t.check(false, "", "[no histogram]");
        return;
    }
    DWORD saved[5];
    memcpy(saved, at<void *>(0x712258), sizeof saved);
    for (int k = 0; k < scaled(40); k++) {
        const unsigned seed = 0x527fa0u + (unsigned)k;
        g_seed = seed;
        memset(g_hist, 0, 0x40000 * 4);
        // never empty: the quantisation's box search then leaves its bounds undefined (the game builds a
        // palette only from a frame's pixels); single cells, sparse cells, clusters
        const DWORD kind = (DWORD)k % 4;
        const DWORD cells = kind == 0 ? 1 + rnd(3) : kind == 1 ? 1 + rnd(400) : 50 + rnd(3000);
        DWORD centre = rnd(0x40000);
        for (DWORD i = 0; i < cells; i++) {
            if (kind == 3 && rnd(40) == 0)
                centre = rnd(0x40000);
            const DWORD cell = kind == 3 ? (centre + rnd(5) * 0x1000 + rnd(5) * 0x40 + rnd(5)) & 0x3ffff : rnd(0x40000);
            g_hist[cell] += 1 + (int)rnd(k % 3 ? 50 : 5000);
        }
        const DWORD colours = k < 4 ? 1 + (DWORD)k : rnd(3) ? 2 + rnd(60) : 100 + rnd(156);
        two_sides(t, "DrawPalette_Build", seed, [&](int side) {
            PalObj *a = make(rnd(2) ? 1 : some_kind(), rnd(2) != 0, F_RANDOM);
            const DWORD first = colours > 200 || rnd(3) ? 0 : rnd(256 - colours);
            memset(at<void *>(0x712258), 0, sizeof saved);
            const DWORD r = call<DWORD>(side, 0x527fa0, a, g_hist, first, colours);
            for (int i = 0; i < 5; i++)
                g_globals[side][i] = rel(at<DWORD *>(0x712258)[i], side);
            return r;
        });
    }
    // the box functions directly on the last histogram: random boxes (inverted and empty ones too), the outputs
    // pattern-filled before
    *at<int **>(0x712258) = g_hist;
    for (int k = 0; k < scaled(600); k++) {
        const unsigned seed = 0x5501f0u + (unsigned)k;
        two_sides(t, k % 2 ? "BoxVolume" : "BoxBottom", seed, [&](int side) {
            int v[6];
            for (int i = 0; i < 3; i++) {
                v[i] = (int)rnd(64);
                v[i + 3] = rnd(8) == 0 ? (int)rnd(64) : v[i] + (int)rnd(64 - v[i]);
            }
            DWORD *o = reinterpret_cast<DWORD *>(block(24));
            for (int i = 0; i < 6; i++)
                o[i] = 0xcc000000u + (DWORD)i;
            side_begin(side);
            if (k % 2)
                at<void(__fastcall *)(int, int, int, int, int, int, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *)>(0x5501f0)(
                    v[0], v[1], v[2], v[3], v[4], v[5], o, o + 1, o + 2, o + 3, o + 4, o + 5);
            else
                at<void(__fastcall *)(int, int, int, int, int, int, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *)>(0x5504f0)(
                    v[0], v[1], v[2], v[3], v[4], v[5], o, o + 1, o + 2, o + 3, o + 4);
            side_end(side);
            return 0u;
        });
    }
    memcpy(at<void *>(0x712258), saved, sizeof saved);
    memset(g_globals, 0, sizeof g_globals);
}

}  // namespace

ZAX_CHECKS(palette_rest4b)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_palette_rest4b;
    Tally setup{"palette: setup"};
    const DWORD *gp = at<const DWORD *>(0x633f98);
    const bool real = arena_setup() && gp[1] == 1 && gp[0] && readable(at<void *>(gp[0]), 0x300);
    setup.check(real, "", "[arena or game palette 0x633f98 missing]");
    setup.report();
    if (!real)
        return;
    memcpy(g_game_palette, at<void *>(gp[0]), 0x300);
    Tally objects{"palette: Init, Assign, Differs, SetKind, Set/GetEntry"};
    check_objects(objects);
    objects.report();
    Tally files{"palette: Load, Save"};
    check_files(files);
    files.report();
    Tally hsl{"palette: RgbToHsl, HueValue"};
    check_hsl(hsl);
    hsl.report();
    Tally near{"palette: Nearest"};
    check_nearest(near);
    near.report();
    Tally tables{"palette: the remap, gamma, amplify, hue blend and lookup tables"};
    check_tables(tables);
    tables.report();
    Tally build{"palette: DrawPalette_Build"};
    check_build(build);
    build.report();
}

}  // namespace zax
