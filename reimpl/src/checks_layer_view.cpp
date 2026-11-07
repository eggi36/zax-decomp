// Checks of layer_view.cpp against the original, in the game thread (sides as in checks_world_misc.h).
//  - Fake layers (0x1d0 bytes, a vtable whose slot 36 gives a random visible rectangle and records the
//    call, a fake tile map with a random size or none): the mini map image rectangle and the world to
//    mini map mapping (random level sizes and clippings, scales 0, NaN or random, random rectangles; the
//    world's slots 99/100 are the running game's), ClampView, ScrollViewToward and SetView with random
//    positions (fractions, negative, huge, -0, NaN). Compared: every output dword, the stored scales
//    and offsets, the view position, the frame offset, eax.
//  - Real data: the running level's layer: the mini map rectangle, the mapping of the level's corners,
//    of its parts' rectangles and of random rectangles (the stored scales put back before each side),
//    ClampView and ScrollViewToward on copies of its view position, SetView with its own view
//    position and others (view position and frame offset put back afterwards).
#include "checks_world_misc.h"
#include "layer_view.h"
#include "x87.h"

namespace zax {

int hooks_layer_view(Hook *out);

namespace {

using namespace misc;

DWORD g_vtable[64];
int g_view[4];
BYTE g_layer[0x1d0];
BYTE g_map[0x2d48];

void THISCALL fake_view_rect(void *layer, int *out)
{
    ev(36, layer == g_layer);
    for (int i = 0; i < 4; i++)
        out[i] = g_view[i];
}

int &i32(BYTE *p, DWORD off) { return *reinterpret_cast<int *>(p + off); }
DWORD &u32(BYTE *p, DWORD off) { return *reinterpret_cast<DWORD *>(p + off); }

int random_size()
{
    switch (random() % 8) {
    case 0: return (int)(random() % 300);
    case 1: return (int)random();
    case 2: return 64 * (int)(random() % 300);
    default: return 640 + (int)(random() % 12000);
    }
}
float random_scale()
{
    switch (random() % 6) {
    case 0: return 0.0f;
    case 1: return raw_float(random() % 2 ? 0x7fc00000 : 0x80000000);
    case 2: return random_float();
    default: return (float)(random() % 1000) / 4000.0f;
    }
}
float random_pos()
{
    switch (random() % 6) {
    case 0: return raw_float(random() % 3 == 0 ? 0x7fc00000 : random() % 2 ? 0x80000000 : 0);
    case 1: return random_float();
    default: return (float)((int)(random() % 20000) - 2000) + (float)(random() % 8) / 8.0f;
    }
}

void setup_layer()
{
    for (DWORD &d : g_vtable)
        d = 0;
    g_vtable[36] = ptrv(ptr(&fake_view_rect));
    for (DWORD i = 0; i < sizeof g_layer; i += 4)
        u32(g_layer, i) = random();
    u32(g_layer, 0) = ptrv(g_vtable);
    const int w = random_size(), h = random_size();
    i32(g_layer, 0x48) = w;
    i32(g_layer, 0x4c) = h;
    for (DWORD off = 0x1a8; off < 0x1b8; off += 4)
        i32(g_layer, off) = random() % 4 ? (int)(random() % 400) : random_size();
    for (DWORD off = 0x1b8; off < 0x1c8; off += 4)
        *reinterpret_cast<float *>(g_layer + off) = random_scale();
    u32(g_layer, 0x7c) = random() % 6 ? ptrv(g_map) : 0;
    i32(g_map, 0x2cfc) = random() % 5 ? random_size() : (int)(random() % 200);
    i32(g_map, 0x2d00) = random() % 5 ? random_size() : (int)(random() % 200);
    g_view[0] = (int)(random() % 2000) - 200;
    g_view[1] = (int)(random() % 2000) - 200;
    g_view[2] = g_view[0] + (random() % 6 ? 640 + (int)(random() % 400) : (int)random());
    g_view[3] = g_view[1] + (random() % 6 ? 480 + (int)(random() % 300) : (int)random());
}

void st_layer(BYTE *layer)
{
    st_bytes(layer + 0x80, 0x10);
    st_bytes(layer + 0x18, 8);
    st_bytes(layer + 0x1b8, 0x10);
}

void random_rect(int *r)
{
    for (int i = 0; i < 4; i++)
        r[i] = random() % 4 ? (int)(random() % 24000) - 2000 : (int)random();
}

using RectFn = DWORD(THISCALL *)(BYTE *, int *);
using MapFn = DWORD(THISCALL *)(BYTE *, int *, const int *);
using ClampFn = DWORD(THISCALL *)(BYTE *, float *);
using ScrollFn = DWORD(THISCALL *)(BYTE *, float *, float, float, float, float, float, float);
using SetFn = DWORD(THISCALL *)(BYTE *, float, float);

DWORD image_rect_case(DWORD &mask)
{
    setup_layer();
    int out[4];
    const DWORD r = at<RectFn>(0x4ec280)(g_layer, out);
    st_bytes(out, sizeof out);
    (void)mask;
    return r;
}

DWORD map_case(DWORD &mask)
{
    setup_layer();
    DWORD r = 0;
    for (int k = 0; k < 4; k++) {
        int in[4], out[4];
        random_rect(in);
        r = at<MapFn>(0x4ec3c0)(g_layer, out, in);
        st_bytes(out, sizeof out);
        st(r);
    }
    st_layer(g_layer);
    (void)mask;
    return r;
}

DWORD clamp_case(DWORD &mask)
{
    setup_layer();
    float pos[2] = {random_pos(), random_pos()};
    at<ClampFn>(0x4ed7f0)(g_layer, pos);
    st_bytes(pos, sizeof pos);
    mask = 0;
    return 0;
}

DWORD scroll_case(DWORD &mask)
{
    setup_layer();
    float pos[2] = {random_pos(), random_pos()};
    float a[6];
    for (float &f : a)
        f = random() % 3 ? random_pos() : (float)(random() % 1500);
    at<ScrollFn>(0x4ed6e0)(g_layer, pos, a[0], a[1], a[2], a[3], a[4], a[5]);
    st_bytes(pos, sizeof pos);
    mask = 0;
    return 0;
}

DWORD set_view_case(DWORD &mask)
{
    setup_layer();
    const DWORD r = at<SetFn>(0x4ed8f0)(g_layer, random_pos(), random_pos());
    st_layer(g_layer);
    (void)mask;
    return r;
}

// ---- Real data ------------------------------------------------------------------------------------------------

BYTE *g_real;
BYTE g_saved_layer[0x1d0];

void restore_real() { memcpy(g_real, g_saved_layer, sizeof g_saved_layer); }

DWORD real_map_case(DWORD &mask)
{
    restore_real();
    if (g_seed % 3 == 0)   // the scales computed from the level (as for a level without a stored mapping)
        *reinterpret_cast<float *>(g_real + (g_seed % 2 ? 0x1bc : 0x1c4)) = 0.0f;
    int out[4];
    DWORD r = at<RectFn>(0x4ec280)(g_real, out);
    st_bytes(out, sizeof out);
    st(r);
    int in[4] = {0, 0, i32(g_real, 0x48), i32(g_real, 0x4c)};
    if (g_seed > 1)
        random_rect(in);
    r = at<MapFn>(0x4ec3c0)(g_real, out, in);
    st_bytes(out, sizeof out);
    st_bytes(g_real + 0x1b8, 0x10);
    restore_real();
    (void)mask;
    return r;
}

BYTE *g_parts[400];
int g_nparts;

DWORD real_part_case(DWORD &mask)
{
    restore_real();
    BYTE *p = g_parts[g_seed % g_nparts];
    int rect[4] = {};
    if (!(vcall<DWORD>(p, 37, rect, 0, 0) & 0xff))
        rect[0] = rect[1] = rect[2] = rect[3] = 0;
    int out[4];
    const DWORD r = at<MapFn>(0x4ec3c0)(g_real, out, rect);
    st_bytes(out, sizeof out);
    restore_real();
    (void)mask;
    return r;
}

DWORD real_view_case(DWORD &mask)
{
    restore_real();
    float pos[2];
    memcpy(pos, g_real + 0x80, 8);
    if (g_seed > 1) {
        pos[0] = fstp(fld(pos[0]) + (long double)((int)(random() % 4000) - 2000));
        pos[1] = fstp(fld(pos[1]) + (long double)((int)(random() % 4000) - 2000));
    }
    float q[2] = {pos[0], pos[1]};
    at<ClampFn>(0x4ed7f0)(g_real, q);
    st_bytes(q, sizeof q);
    float a[6];
    for (float &f : a)
        f = (float)((int)(random() % 3000) - 500);
    q[0] = pos[0];
    q[1] = pos[1];
    at<ScrollFn>(0x4ed6e0)(g_real, q, a[0], a[1], a[2], a[3], a[4], a[5]);
    st_bytes(q, sizeof q);
    const DWORD r = at<SetFn>(0x4ed8f0)(g_real, pos[0], pos[1]);
    st_layer(g_real);
    restore_real();
    (void)mask;
    return r;
}

}  // namespace

ZAX_CHECKS(layer_view)
{
    (void)cls;
    (void)n;
    use_hooks(&hooks_layer_view);
    Tally fake{"layer view: mini map mapping and view on fake layers"}, real{"layer view: running level"};
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(fake, "image rect", seed, [](DWORD &mask) { return image_rect_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(fake, "map", seed, [](DWORD &mask) { return map_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(2000); seed++)
        two_sides(fake, "clamp", seed, [](DWORD &mask) { return clamp_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(2000); seed++)
        two_sides(fake, "scroll", seed, [](DWORD &mask) { return scroll_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1000); seed++)
        two_sides(fake, "set view", seed, [](DWORD &mask) { return set_view_case(mask); });
    fake.report();
    g_real = running_layer();
    if (g_real && readable(g_real, 0x1d0)) {
        memcpy(g_saved_layer, g_real, sizeof g_saved_layer);
        log("  layer view: running layer %ux%u, clip %d %d %d %d, mini map %x %x %x %x, view %x %x", i32(g_real, 0x48),
            i32(g_real, 0x4c), i32(g_real, 0x1a8), i32(g_real, 0x1ac), i32(g_real, 0x1b0), i32(g_real, 0x1b4),
            u32(g_real, 0x1b8), u32(g_real, 0x1bc), u32(g_real, 0x1c0), u32(g_real, 0x1c4), u32(g_real, 0x80), u32(g_real, 0x84));
        for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
            two_sides(real, "real map", seed, [](DWORD &mask) { return real_map_case(mask); });
        g_nparts = real_parts(g_parts, 400);
        for (unsigned seed = 1; g_nparts && seed <= (unsigned)g_nparts * 2; seed++)
            two_sides(real, "real part", seed, [](DWORD &mask) { return real_part_case(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
            two_sides(real, "real view", seed, [](DWORD &mask) { return real_view_case(mask); });
        restore_real();
    } else {
        log("  layer view: no running layer, real data skipped");
    }
    real.report();
}

}  // namespace zax
