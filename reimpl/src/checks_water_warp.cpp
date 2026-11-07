// Checks of the animated water (water_warp.cpp) against the original, in the game thread.
//  - The classes CWaterWarpEffect and CWaterWarpEffectFrame (machinery: checks_inventory_common.h): Create (the
//    constructor's wave tables with the x87 fcos), CreateArray, the deleting destructors (the frame's bent frame
//    deleted), the getters (also slot 57 of the effect frames' list), RegisterFields (the static list's Add a
//    recording stand-in), the registered classes.
//  - The four warps on fake 128 × 128 frames (the frame check harness; 5-6-5, and 8 bits for 0x597880 and a
//    32-bit destination for its "nothing" path), sources a pattern or a copy of a real ground texture; the
//    effect made by the original's Create with random speeds (also negative, zero and large) and random tables
//    (also negative and beyond 128). Compared: pixels, frame calls.
//  - Update (0x598420) on copies of the level's "Water Warp Effects" frames (list 0x713c80) and on effects of
//    their own with render types 0-5 and static and animated textures whose records hold their object (so
//    nothing loads), with the last time equal to the game time (nothing done) or not; the bent frame made on
//    the first call; compared: the returned frame's size, flags and pixels, the stored time; the frames and
//    copies deleted afterwards.
#include "checks_hud_common.h"
#include "checks_inventory_common.h"
#include "checks_frame_r1.h"
#include "ai_helpers.h"

namespace zax {

int hooks_water_warp(Hook *out);

namespace {

using namespace ai;
using r1::rnd;
using r1::rnd_range;

const invcheck::ClassDef CLASSES[] = {
    {"CWaterWarpEffect", 0x86c, 0x713dc8, 0x596e40, 0x596e60, 0x5970a0, {0x597040, 0}, true},
    {"CWaterWarpEffectFrame", 0x880, 0x713dcc, 0x597ca0, 0x597cf0, 0x597fe0, {0x597f30, 0x598a20}, true},
};

framec::ModuleHooks g_hooks{"water_warp", &hooks_water_warp, {}, {}, {}, 0, true};

// CWaterWarpEffectFrame's RegisterFields ends adding 0x598280 to the static list 0x62e6a8 (0x419ea0,
// fastcall): a recording stand-in while the classes are tested, so the game's list stays as it was
DWORD g_static_added, g_static_calls;
DWORD __fastcall fake_static_add(DWORD item)
{
    g_static_added = item;
    g_static_calls++;
    return 0x5a5a0000u + item;
}

BYTE *new_effect(bool frame) { return static_cast<BYTE *>(original<void *(__cdecl *)()>(frame ? 0x597ca0 : 0x596e40)()); }
void delete_effect(BYTE *e, bool frame) { original<void *(THISCALL *)(void *, DWORD)>(frame ? 0x597d60 : 0x596ec0)(e, 1); }

float random_speed()
{
    const DWORD r = rnd(8);
    if (r == 0)
        return 0.0f;
    if (r == 1)
        return (float)rnd_range(-100000, 100000);
    return (float)rnd_range(-4000, 4000) / 100.0f;
}
void randomize(BYTE *e)
{
    static const DWORD WAVES[4] = {8, 0x220, 0x438, 0x650};
    for (DWORD w : WAVES) {
        fl<float>(e, w + 0xc) = random_speed();
        const int range = rnd(5) == 0 ? 400 : 12;
        for (DWORD i = 0; i < 0x80; i++)
            dw(e, w + 0x14 + i * 4) = (DWORD)rnd_range(-range, range);
    }
}

BYTE *g_textures[8];
int g_ntextures;
void find_textures()
{
    g_ntextures = 0;
    framec::each_list_object(framec::TEXTURE_LIST, [](void *o) {
        BYTE *f = static_cast<BYTE *>(o);
        if (g_ntextures < 8 && invcheck::readable(f, 0x40) && *reinterpret_cast<WORD *>(f + 0x12) == 0x80 &&
            *reinterpret_cast<WORD *>(f + 0x14) == 0x80 && (dw(f, 0x3c) & 0x46) == 0x40 && dw(f, 0xc))
            g_textures[g_ntextures++] = f;
    });
}

void check_warps(Tally &t)
{
    static const DWORD FNS[4] = {0x597690, 0x597780, 0x597880, 0x597ad0};
    g_seed = 0x597690;
    for (int k = 0; k < scaled(240); k++) {
        const int fn = (int)rnd(4);
        const DWORD kind = fn == 2 ? rnd(6) : 0;   // 0x597880: 0-3 16 bits, 4 8 bits, 5 a 32-bit destination
        framec::begin_case(random());
        framec::FrameSpec s;
        s.format = kind == 4 ? framec::PAL8 : framec::RGB565;
        s.width = s.height = 0x80;
        const int src = framec::make_frame(s);
        if (kind == 5)
            s.format = framec::RGB32;
        const int dst = framec::make_frame(s);
        if (src < 0 || dst < 0)
            continue;
        if (g_ntextures && s.format == framec::RGB565 && rnd(3) == 0) {
            BYTE *tex = g_textures[rnd((DWORD)g_ntextures)];
            for (int side = 0; side < 2; side++)
                memcpy(framec::frame(side, src)->pixels, bp(dw(tex, 0xc)), 0x80 * 0x80 * 2);
        }
        BYTE *e = new_effect(false);
        if (rnd(4))
            randomize(e);
        char what[64];
        wsprintfA(what, "warp 0x%x kind %d case %d", FNS[fn], kind, k);
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            at<void(THISCALL *)(void *, void *, void *)>(FNS[fn])(e, framec::frame(side, dst), framec::frame(side, src));
            return 0;
        });
        delete_effect(e, false);
    }
}

// The record of list `list` at index i holds its object (nothing would load)
bool loaded(DWORD list, DWORD i)
{
    const BYTE *records = *at<BYTE **>(list + 0x30);
    if (!records)
        return false;
    if (i >= *at<DWORD *>(list + 0x34))
        i = 0;
    return records[i * 0x80 + 0x7f] == 0 && dw(const_cast<BYTE *>(records) + i * 0x80, 0x78) != 0;
}
DWORD loaded_index(DWORD list)
{
    const DWORD n = *at<BYTE **>(list + 0x30) ? *at<DWORD *>(list + 0x34) : 0;
    DWORD found[16], m = 0;
    for (DWORD i = 0; i < n && m < 16; i++)
        if (loaded(list, i))
            found[m++] = i;
    return m ? found[rnd(m)] : 0xffffffff;
}

void check_update(Tally &t)
{
    g_seed = 0x598420;
    BYTE *reals[8];
    int nreal = 0;
    framec::each_list_object(0x713c80, [&](void *o) {
        if (nreal < 8 && invcheck::readable(o, 0x880) && dw(o, 0) == 0x602988)
            reals[nreal++] = static_cast<BYTE *>(o);
    });
    const double time = *reinterpret_cast<const double *>(bp(*at<DWORD *>(0x6c02cc)) + 0x40);
    int done = 0;
    for (int k = 0; k < scaled(120) + nreal; k++) {
        BYTE *e = new_effect(true);
        if (k < nreal) {
            BYTE *keep = bp(dw(e, 0));
            memcpy(e + 4, reals[k] + 4, 0x880 - 4);   // the fields; the vtable and CObject's own +4 stay
            dw(e, 4) = 0;
            dw(e, 0x86c) = 0;
            (void)keep;
        } else {
            randomize(e);
            dw(e, 0x878) = rnd(6);
            dw(e, 0x870) = loaded_index(0x6c1d50);
            dw(e, 0x874) = loaded_index(0x6c1ee8);
        }
        const bool animated = dw(e, 0x878) >= 4;
        if (!loaded(animated ? 0x6c1ee8 : 0x6c1d50, dw(e, animated ? 0x874 : 0x870))) {
            delete_effect(e, true);
            continue;
        }
        const DWORD same_time = rnd(6) == 0;
        fl<float>(e, 0x87c) = same_time ? (float)time : (float)time - 1.0f;
        BYTE copy[0x880];
        memcpy(copy, e, 0x880);
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            memcpy(e, copy, 0x880);
            if (side == 0)
                g_hooks.out();
            BYTE *f = at<BYTE *(__fastcall *)(void *)>(0x598420)(e);
            if (side == 0)
                g_hooks.in();
            unsigned hh = fnv(2166136261u, e + 0x87c, 4) ^ (f == bp(dw(e, 0x86c)));
            if (f) {
                hh = fnv(hh, f + 8, 4);
                hh = fnv(hh, f + 0x10, 6);
                hh = fnv(hh, f + 0x3c, 4);
                if (BYTE *px = bp(dw(f, 0xc)))
                    hh = fnv(hh, px, 0x80 * 0x80 * vcall<DWORD>(f, 30));
                vcall<void>(f, 1, 1);
            }
            dw(e, 0x86c) = 0;
            h[side] = hh;
        }
        memcpy(e, copy, 0x880);
        char d[96];
        wsprintfA(d, "[Update case %d, type %d%s: %x / %x]", k, dw(e, 0x878), same_time ? ", same time" : "", h[0], h[1]);
        t.check(g_hooks.fits && h[0] == h[1], "", d);
        delete_effect(e, true);
        done++;
    }
    log("  water warp: %d effect frames of the level, %d updates, %d textures", nreal, done, g_ntextures);
}

}  // namespace

ZAX_CHECKS(water_warp)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    invcheck::g_hook_table = &hooks_water_warp;
    if (!invcheck::arena_setup()) {
        log("Check water warp: no arena");
        return;
    }
    Tally objects{"water warp: Create, CreateArray, destructors"}, getters{"water warp: class getters"}, fields{"water warp: RegisterFields"},
        registered{"water warp: registered descriptors and classes (real data)"};
    CodePatch p;
    patch_jump(p, 0x419ea0, (void *)&fake_static_add);
    invcheck::test_classes(objects, getters, fields, registered, CLASSES, 2);
    unpatch(p);
    log("  water warp: static list adds %d (last %x)", g_static_calls, g_static_added);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    find_textures();
    Tally warps{"water warp: the four warps into fake frames"};
    check_warps(warps);
    warps.report();
    Tally update{"water warp: CWaterWarpEffectFrame::Update"};
    check_update(update);
    update.report();
}

}  // namespace zax
