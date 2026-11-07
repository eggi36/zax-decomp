// Checks of ai_draw.cpp with the frame check harness, in the game thread: the four AIs' draw slot on fake AI
// objects (the fields of each class at random: shapes and sizes incl. 0, the scan limits and base angle,
// the timeout ovals' radius and ratio, rectangles, texts empty or not, the real loaded fonts) and a fake part
// (slot 43 the position inside, on the edge and off the frame; slot 46 the facing; slot 48 no parent) into
// fake 5-6-5 frames. The static colours with their guards are put back before each side and at the end,
// atexit a stand-in. Compared: pixels, the frame calls, the statics.
#include "checks_frame_r1.h"
#include "checks_hud_common.h"

namespace zax {

int hooks_ai_draw(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"ai_draw", &hooks_ai_draw, {}, {}, {}, 0, true};

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
template <typename F> DWORD fn_address(F f) { return (DWORD)(uintptr_t)f; }
float rnd_float(float lo, float hi) { return lo + (hi - lo) * (float)rnd(100001) / 100000.0f; }

float g_pos[2], g_facing;
DWORD THISCALL part_pos(void *, float *out)
{
    out[0] = g_pos[0];
    out[1] = g_pos[1];
    return 0;
}
long double THISCALL part_facing(void *) { return g_facing; }
DWORD THISCALL part_zero(void *) { return 0; }
DWORD g_part_vt[64];
DWORD g_part[4];

alignas(16) BYTE g_ai[2][0x60];

struct Statics {
    SavedBytes a, ag, b, bg;
    void save()
    {
        save_bytes(a, 0x6bfe38, 0x10);
        save_bytes(ag, 0x6bfe4b, 1);
        save_bytes(b, 0x6bfe28, 0x10);
        save_bytes(bg, 0x6bfe21, 1);
    }
    void restore() const
    {
        restore_bytes(a);
        restore_bytes(ag);
        restore_bytes(b);
        restore_bytes(bg);
    }
};
Statics g_statics;

int loaded_fonts(DWORD *out, int cap)
{
    BYTE *records = *at<BYTE **>(0x6be210);
    const DWORD n = *at<DWORD *>(0x6be214);
    int k = 0;
    for (DWORD i = 0; records && i < n && k < cap; i++)
        if (dw(records + i * 0x80, 0x78) && !records[i * 0x80 + 0x7f])
            out[k++] = dw(records + i * 0x80, 0x78);
    return k;
}

void check_ai_draws(Tally &t)
{
    for (int i = 0; i < 64; i++)
        g_part_vt[i] = fn_address(&part_zero);
    g_part_vt[43] = fn_address(&part_pos);
    g_part_vt[46] = fn_address(&part_facing);
    g_part[0] = ptrv(g_part_vt);
    DWORD fonts[32];
    const int nfonts = loaded_fonts(fonts, 32);
    static const char *const texts[] = {"", "Timeout", "A longer debug text", "x"};
    g_seed = 0x4e4280;
    for (int k = 0; k < scaled(1200); k++) {
        begin_case(random());
        FrameSpec s;
        s.width = rnd_range(60, 200);
        s.height = rnd_range(40, 160);
        const int fi = make_frame(s);
        const int kind = k % 4;
        BYTE ai[0x60];
        for (DWORD o = 0; o < 0x60; o += 4)
            dw(ai, o) = random();
        g_pos[0] = rnd(5) ? rnd_float(-30, (float)s.width + 30) : (float)rnd_range(0, s.width);
        g_pos[1] = rnd(5) ? rnd_float(-30, (float)s.height + 30) : (float)rnd_range(0, s.height);
        g_facing = rnd_float(-7, 14);
        const char *text = texts[rnd(4)];
        switch (kind) {
        case 0:   // CLandscapeAI
            dw(ai, 0x10) = rnd(6) ? rnd(120) : 0;
            dw(ai, 0x14) = rnd(6) ? rnd(120) : 0;
            *reinterpret_cast<float *>(ai + 0x18) = rnd(4) ? 1.0f : rnd(2) ? 0.0f : -1.0f;
            dw(ai, 0x24) = rnd(3);
            if (dw(ai, 0x24) == 0 && !dw(ai, 0x10))
                dw(ai, 0x10) = 1 + rnd(50);   // the ratio divides by the width
            break;
        case 1:   // CScanAreaAI
            *reinterpret_cast<float *>(ai + 0x14) = rnd_float(-200, 200);
            *reinterpret_cast<float *>(ai + 0x18) = rnd_float(-200, 200);
            *reinterpret_cast<float *>(ai + 0x48) = rnd_float(-7, 7);
            ai[0x40] = (BYTE)rnd(2);
            break;
        case 2:   // CDrawTimeoutOvalAI
            *reinterpret_cast<float *>(ai + 0x10) = rnd(6) ? rnd_float(0, 90) : 0.0f;
            *reinterpret_cast<float *>(ai + 0x14) = rnd_float(0, 2);
            break;
        default: {   // CDrawTimeoutRectAI
            int *r = reinterpret_cast<int *>(ai + 0x10);
            r[0] = rnd_range(-20, s.width);
            r[1] = rnd_range(-20, s.height);
            r[2] = r[0] + rnd_range(0, 80);
            r[3] = r[1] + rnd_range(0, 60);
        }
        }
        const DWORD font = nfonts ? fonts[rnd((DWORD)nfonts)] : 0;
        if (!font && (kind == 2 || kind == 3))
            text = "";
        static const DWORD draws[4] = {0x4e4280, 0x565f40, 0x4cad40, 0x4cb110};
        static const char *const names[4] = {"CLandscapeAI draw", "CScanAreaAI draw", "CDrawTimeoutOvalAI draw", "CDrawTimeoutRectAI draw"};
        if (g_frame_trace)
            log("    ai draw case %d: %s, field 0x24 %x, text '%s', font %x", k, names[kind], dw(ai, 0x24), text, font);
        hudcheck::run_sides(t, g_hooks, names[kind], [&](int side) -> DWORD {
            BYTE *a = g_ai[side];
            memcpy(a, ai, sizeof ai);
            const DWORD zs = kind == 2 ? 0x1c : 0x24, fo = kind == 2 ? 0x20 : 0x28;
            if (kind >= 2) {
                at<void *(THISCALL *)(void *)>(0x4dec90)(a + zs);
                at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(a + zs, text);   // ZString = C string
                dw(a, fo) = font;
            }
            g_statics.restore();
            CodePatch ax;
            patch_jump(ax, 0x5d0863, (void *)&fake_atexit);
            at<void(THISCALL *)(void *, void *, void *)>(draws[kind])(a, g_part, frame(side, fi));
            unpatch(ax);
            const DWORD st = fnv(fnv(2166136261u, at<void *>(0x6bfe38), 0x10), at<void *>(0x6bfe28), 0x10) ^ *at<BYTE *>(0x6bfe4b) ^ (DWORD)*at<BYTE *>(0x6bfe21) << 8;
            if (kind >= 2)
                at<void(THISCALL *)(void *)>(0x4defd0)(a + zs);
            g_statics.restore();
            return st;
        });
    }
}

}  // namespace

ZAX_CHECKS(ai_draw)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    g_statics.save();
    Tally t{"ai draw: the draw slots of CLandscapeAI, CScanAreaAI, CDrawTimeoutOvalAI, CDrawTimeoutRectAI"};
    check_ai_draws(t);
    t.report();
    g_statics.restore();
}

}  // namespace zax
