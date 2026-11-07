// Checks of raster_line.cpp with the frame check harness, in the game thread.
//  - CFrame_DrawLineSpans 0x42b160 on fake frames of 8, 16, 24 and 32 bits (sizes 1-90 x 1-70, pitch
//    padding, origins up to +-6, CDIBFrame), float end points inside, on and across the edges, outside,
//    with fractions, degenerate (a point, horizontal, vertical, 45 degrees, sub-pixel short), colours with
//    and without the palette index; every fourth case on a real frame's pixels of the level.
//  - The three rasterizers directly with 16.16 end points (fractions also for 8 bits), both axes and
//    directions, the unclipped and the clipped paths (end points within 2 pixels of the bottom or right
//    edge), equal end points (MulDiv by 0).
//  - SpanTables_Init with no tables, a cover table big enough (kept) and one too small (freed and made
//    again): both tables and the six globals; SpanTables_Free. The game's own tables are put back.
// The 8-bit cases need the 5-5-5 inverse colour table (0x633388); when the game has none loaded they use
// a pattern table in the harness's memory for the duration of the case.
#include "checks_frame_r1.h"

namespace zax {

int hooks_raster_line(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"raster_line", &hooks_raster_line, {}, {}, {}, 0, true};

constexpr DWORD INV_COLOR15 = 0x633388;
alignas(16) BYTE g_inverse[0x8000];

Sources g_sources;

float frac_coord(int lo, int hi)
{
    const float base = (float)rnd_range(lo, hi);
    switch (rnd(5)) {
    case 0: return base;
    case 1: return base + 0.5f;
    default: return base + (float)rnd(1000) / 1000.0f;
    }
}

float pick_float(int size)
{
    switch (rnd(10)) {
    case 0: return 0.0f;
    case 1: return (float)size;
    case 2: return (float)size - 0.25f;
    case 3: return frac_coord(-60, size + 60);
    case 4: return (float)rnd_range(-20000, 20000);
    default: return frac_coord(-4, size + 3);
    }
}

const Format FORMATS[3] = {PAL8, RGB565, RGB24};
const DWORD RASTERS[3] = {0x42b6e0, 0x42c0f0, 0x42c9e0};
const char *const RASTER_NAMES[3] = {"Raster8_Spans", "Raster16_Spans", "Raster24_Spans"};

Format pick_format(int k) { return k % 23 == 0 ? RGB32 : FORMATS[k % 3]; }

// The inverse colour table for the 8-bit cases (the game's when loaded)
struct InverseTable {
    BYTE *saved;
    bool replaced = false;
    void set()
    {
        saved = *at<BYTE **>(INV_COLOR15);
        if (saved)
            return;
        fill_pattern(g_inverse, sizeof g_inverse, 0x633388);
        *at<BYTE **>(INV_COLOR15) = g_inverse;
        replaced = true;
    }
    ~InverseTable()
    {
        if (replaced)
            *at<BYTE **>(INV_COLOR15) = saved;
    }
};

void check_draw(Tally &t)
{
    g_seed = 0x42b160;
    for (int k = 0; k < scaled(4000); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = pick_format(k);
        s.width = rnd(8) ? rnd_range(4, 90) : rnd_range(1, 3);
        s.height = rnd(8) ? rnd_range(4, 70) : rnd_range(1, 3);
        if (s.format == RGB24 && s.width * s.height > 2400)
            s.height = 2400 / s.width;
        s.pitch = rnd(3) ? 0 : s.width + rnd_range(1, 9);
        if (rnd(3) == 0) {
            s.x = rnd_range(-6, 6);
            s.y = rnd_range(-6, 6);
        }
        s.dib = rnd(6) == 0;
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        if (k % 4 == 1 && g_sources.n)
            copy_source(fi, g_sources.list[rnd((DWORD)g_sources.n)].frame);
        const DWORD col = alloc_object(sizeof(FieldColor));
        FieldColor c;
        for (DWORD i = 0; i < sizeof c; i++)
            reinterpret_cast<BYTE *>(&c)[i] = (BYTE)random();
        c.flags = rnd(2) ? 1 : (rnd(2) ? 2 : 0);
        float p[4];
        for (int i = 0; i < 4; i++)
            p[i] = pick_float(i & 1 ? s.height : s.width);
        switch (rnd(8)) {
        case 0: p[2] = p[0], p[3] = p[1]; break;                          // a point
        case 1: p[3] = p[1]; break;                                       // horizontal
        case 2: p[2] = p[0]; break;                                       // vertical
        case 3: p[3] = p[1] + (p[2] - p[0]); break;                       // 45 degrees
        case 4: p[2] = p[0] + 0.3f, p[3] = p[1] - 0.2f; break;            // sub-pixel
        }
        for (int side = 0; side < 2; side++)
            *object<FieldColor>(side, col) = c;
        if (g_frame_trace)
            log("raster_line draw %d: format %d %dx%d at %d,%d (%d %d %d %d)/1000", k, s.format, s.width, s.height, s.x, s.y,
                (int)(p[0] * 1000), (int)(p[1] * 1000), (int)(p[2] * 1000), (int)(p[3] * 1000));
        InverseTable inv;
        if (s.format == PAL8)
            inv.set();
        run_sides(t, g_hooks, "CFrame_DrawLineSpans", [&](int side) -> DWORD {
            at<void(__fastcall *)(void *, FieldColor *, float, float, float, float)>(0x42b160)(frame(side, fi), object<FieldColor>(side, col),
                                                                                                p[0], p[1], p[2], p[3]);
            return 0;
        });
    }
}

// 16.16 coordinate of a frame axis of `size` pixels: mostly inside, some at the last two pixels (the clipped
// path) or one or two beyond
int pick_fixed(int size)
{
    int v;
    switch (rnd(6)) {
    case 0: v = rnd_range(size - 3, size + 1); break;
    case 1: v = rnd_range(-2, 1); break;
    default: v = rnd_range(0, size - 1); break;
    }
    return v * 0x10000 + (int)rnd(0x10000);
}

void check_rasters(Tally &t)
{
    g_seed = 0x42b6e0;
    for (int k = 0; k < scaled(3000); k++) {
        begin_case(random());
        const int d = k % 3;
        FrameSpec s;
        s.format = FORMATS[d];
        s.width = rnd_range(4, 64);
        s.height = rnd_range(4, 48);
        s.pitch = rnd(3) ? 0 : s.width + rnd_range(1, 7);
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        if (k % 5 == 2 && g_sources.n)
            copy_source(fi, g_sources.list[rnd((DWORD)g_sources.n)].frame);
        int x1 = pick_fixed(s.width), y1 = pick_fixed(s.height), x2 = pick_fixed(s.width), y2 = pick_fixed(s.height);
        if (rnd(10) == 0)
            x2 = x1, y2 = y1;
        // the unclipped paths draw only between the end points: keep them on the frame's pixels
        const DWORD color = random();
        InverseTable inv;
        if (d == 0)
            inv.set();
        run_sides(t, g_hooks, RASTER_NAMES[d], [&](int side) -> DWORD {
            at<void(__fastcall *)(void *, int, int, int, int, DWORD)>(RASTERS[d])(frame(side, fi), x1, y1, x2, y2, color);
            return 0;
        });
    }
}

constexpr DWORD SPAN_GLOBALS = 0x62ea30, SPAN_GLOBALS_BYTES = 0x1c;   // 0x62ea30..0x62ea4b
constexpr DWORD SPAN_SQRT = 0x62ea3c, SPAN_COVER = 0x62ea40, SPAN_COVER_SIZE = 0x62ea48;

void *exe_new(DWORD n) { return at<void *(__cdecl *)(DWORD)>(0x5d033a)(n); }
void exe_delete(void *p) { at<void(__cdecl *)(void *)>(0x5d0320)(p); }

void check_tables(Tally &t)
{
    BYTE saved[SPAN_GLOBALS_BYTES];
    memcpy(saved, at<void *>(SPAN_GLOBALS), sizeof saved);
    for (int k = 0; k < 6; k++) {
        Snap snap[2];
        for (int side = 0; side < 2; side++) {
            memset(at<void *>(SPAN_GLOBALS), 0, SPAN_GLOBALS_BYTES);
            void *given = nullptr;
            __asm__ volatile("emms");   // an earlier blit may leave the MMX state set (0x4355f0 has no emms)
            if (k % 3 == 1) {   // a cover table big enough: kept (filled alike on both sides: entries the
                                // builder does not write keep it)
                given = exe_new(100 * 4);
                memset(given, 0xa5, 100 * 4);
                *at<void **>(SPAN_COVER) = given;
                *at<int *>(SPAN_COVER_SIZE) = 100;
            } else if (k % 3 == 2) {   // too small: freed
                *at<void **>(SPAN_COVER) = exe_new(10 * 4);
                *at<int *>(SPAN_COVER_SIZE) = 10;
                *at<void **>(SPAN_SQRT) = exe_new(8);
            }
            if (side == 0)
                original<void(__cdecl *)()>(0x42b320)();
            else
                at<void(__cdecl *)()>(0x42b320)();
            Snap &s = snap[side];
            const DWORD *g = at<const DWORD *>(SPAN_GLOBALS);
            s.add(g[0]);
            s.add(g[1]);
            s.add(g[2]);
            s.add(g[4] == ptrv(given) ? 1 : 0);
            s.add(g[5]);
            s.add(g[6]);
            const DWORD n = g[6];
            s.add_bytes(*at<void **>(SPAN_COVER), (n < 100 ? n : 100) * 4);
            s.add_bytes(*at<void **>(SPAN_SQRT), (1u << (16 - g[2] < 32 ? 16 - g[2] : 0)) * 4 + 4);
            exe_delete(*at<void **>(SPAN_COVER));
            exe_delete(*at<void **>(SPAN_SQRT));
        }
        const bool same = snap[0] == snap[1];
        char d[120];
        wsprintfA(d, "[SpanTables_Init case %d: %d / %d values]", k, snap[0].n, snap[1].n);
        t.check(same, "", d);
        if (!same)
            for (int i = 0; i < snap[0].n && i < snap[1].n; i++)
                if (snap[0].v[i] != snap[1].v[i]) {
                    log("    value %d: %x / %x", i, snap[0].v[i], snap[1].v[i]);
                    break;
                }
    }
    for (int k = 0; k < 4; k++) {
        DWORD after[2][2];
        for (int side = 0; side < 2; side++) {
            memset(at<void *>(SPAN_GLOBALS), 0, SPAN_GLOBALS_BYTES);
            if (k & 1)
                *at<void **>(SPAN_COVER) = exe_new(16);
            if (k & 2)
                *at<void **>(SPAN_SQRT) = exe_new(16);
            if (side == 0)
                original<void(__cdecl *)()>(0x42b6a0)();
            else
                at<void(__cdecl *)()>(0x42b6a0)();
            after[side][0] = *at<DWORD *>(SPAN_COVER);
            after[side][1] = *at<DWORD *>(SPAN_SQRT);
        }
        t.check(!memcmp(after[0], after[1], sizeof after[0]) && !after[1][0] && !after[1][1], "", "[SpanTables_Free]");
    }
    memcpy(at<void *>(SPAN_GLOBALS), saved, sizeof saved);
}

}  // namespace

ZAX_CHECKS(raster_line)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    collect_sources(g_sources, 6);
    Tally draw{"raster line: CFrame_DrawLineSpans"};
    check_draw(draw);
    draw.report();
    Tally rasters{"raster line: Raster8/16/24_Spans"};
    check_rasters(rasters);
    rasters.report();
    Tally tables{"raster line: SpanTables_Init, SpanTables_Free"};
    check_tables(tables);
    tables.report();
}

}  // namespace zax
