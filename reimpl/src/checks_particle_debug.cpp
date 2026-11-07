// Checks of particle_debug.cpp with the frame check harness, in the game thread.
//  - The particle systems: copies of the running level's CGenericParticleSystemAI objects (with their
//    part, read only) and fakes (a copy of a real one or a new one from Create, with random angles, spread
//    envelopes, offsets and "Orient To Parent's Rotation"; any level part); the start point type a copy of
//    the real one or a new object of each of the five types with random envelope values. Drawn into fake
//    5-6-5 frames whose origin lies around the emitter: slot 41 (0x530830), each type's slot 24, the cone
//    0x52ecd0, the cylinder's shape 0x52cfb0, the circle 0x52b010 (pixels, frame calls) and the circle
//    point 0x52af80 (the point written). The game's random generator (the direction's random turn) and the
//    function-local colours with their guards are put back before each side, atexit a stand-in.
//  - CPointList's drawing (0x536480) on fake lists of 0-6 points (eax, pixels, frame calls).
//  - The hue remap table (0x5280e0) on the game palette and random palettes, hue parameters (centre, new
//    hue, range; also negative, beyond 360, 0) and both clear flags: the 256 bytes.
#include "checks_frame_r1.h"
#include "checks_hud_common.h"
#include "checks_particle_world.h"

namespace zax {

int hooks_particle_debug(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"particle_debug", &hooks_particle_debug, {}, {}, {}, 0, true};

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
float &ff(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
float rnd_float(float lo, float hi) { return lo + (hi - lo) * (float)rnd(100001) / 100000.0f; }
using Create0 = void *(__cdecl *)();

// The colours (FieldColor, 0x10) and their guards, the game's random generator
constexpr DWORD STATIC_COLOURS[] = {0x6d0248, 0x6d0338, 0x6d01b8, 0x6d03c0, 0x6d0450, 0x6d0200, 0x6d0460, 0x6d03d0, 0x6d0398, 0x6d0580, 0x6d0598};
constexpr DWORD GUARDS[] = {0x6d022d, 0x6d0471, 0x6d040c, 0x6d0298, 0x6d05aa};
struct Statics {
    SavedBytes c[11], g[5], rnd;
    void save()
    {
        for (int i = 0; i < 11; i++)
            save_bytes(c[i], STATIC_COLOURS[i], 0x10);
        for (int i = 0; i < 5; i++)
            save_bytes(g[i], GUARDS[i], 1);
        save_bytes(rnd, 0x7124c0, GAME_RANDOM_BYTES);
    }
    void restore() const
    {
        for (const SavedBytes &s : c)
            restore_bytes(s);
        for (const SavedBytes &s : g)
            restore_bytes(s);
        restore_bytes(rnd);
    }
    DWORD hash() const
    {
        unsigned h = 2166136261u;
        for (DWORD a : STATIC_COLOURS)
            h = fnv(h, at<void *>(a), 0x10);
        for (DWORD a : GUARDS)
            h = fnv(h, at<void *>(a), 1);
        return h;
    }
};
Statics g_statics;

// The start point types: Create of each
const DWORD START_CREATE[5] = {0x52b2d0, 0x52b600, 0x52c140, 0x52c8f0, 0x52d6c0};   // point, disk, line, cylinder, sphere
const DWORD START_SIZE[5] = {8, 0x58, 0x2c, 0x7c, 0x58};
const char *const START_NAME[5] = {"point", "disk", "line", "cylinder", "sphere"};
const DWORD START_VTABLE[5] = {0x5fc198, 0x5fc280, 0x5fc2fc, 0x5fc374, 0x5fc3f4};
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

void random_envelope(BYTE *e, float lo, float hi)
{
    ff(e, 8) = rnd(6) ? rnd_float(lo, hi) : 0.0f;
    ff(e, 0xc) = rnd_float(lo, hi);
    ff(e, 0x14) = rnd(2) ? 0.0f : rnd_float(-5, 5);
}
void randomize_start(BYTE *o, int kind)
{
    if (kind == 0)
        return;
    random_envelope(o + 8, 0, kind == 2 ? 200.0f : 90.0f);
    if (kind != 2) {
        random_envelope(o + 0x2c, 0, 90);
        ff(o, 0x54) = rnd(4) ? rnd_float(0.2f, 2) : 1.0f;
    }
    if (kind == 3)
        random_envelope(o + 0x58, -40, 120);
}

// Real systems of the level (at most 12) and their parts
constexpr int MAX_REAL = 12;
BYTE *g_real[MAX_REAL], *g_real_part[MAX_REAL];
int g_nreal;
BYTE *g_any_part;
BYTE *g_parts[4096];
void find_systems()
{
    g_nreal = 0;
    g_any_part = nullptr;
    const int n = pw::level_parts(g_parts, 4096);
    CClassInfo *generic = at<CClassInfo *(__cdecl *)()>(0x52ef30)();
    for (int i = 0; i < n; i++) {
        if (!g_any_part)
            g_any_part = g_parts[i];
        BYTE *ais[64];
        const int k = pw::part_ais(g_parts[i], g_parts, n, ais, 64);
        for (int j = 0; j < k && g_nreal < MAX_REAL; j++)
            if (pw::class_of(ais[j]) == generic && pw::readable(ais[j], 0x148)) {
                g_real[g_nreal] = ais[j];
                g_real_part[g_nreal++] = g_parts[i];
            }
    }
}

alignas(16) BYTE g_sys[0x148];
BYTE g_ratio_obj[0x58];   // the cylinder helper's object: only its ratio +0x54 is read
BYTE *g_start_obj;
int g_start_kind;
BYTE *g_part;

// A system for a case: a copy of a real one, or a fake; its start point type a new object (+0x110)
bool make_system(int k)
{
    const bool real = g_nreal && k < g_nreal * 2;
    BYTE *src = g_nreal ? g_real[k % g_nreal] : nullptr;
    g_part = g_nreal ? g_real_part[k % g_nreal] : g_any_part;
    if (!g_part)
        return false;
    if (src)
        memcpy(g_sys, src, 0x148);
    else {
        // a new one: its fields copied, the object deleted (the drawing reads only the embedded envelopes,
        // the offsets and +0x110, which the case sets)
        BYTE *o = static_cast<BYTE *>(at<Create0>(0x52f040)());
        memcpy(g_sys, o, 0x148);
        vcall<void *>(o, 1, 1);
    }
    dw(g_sys, 4) = 0;
    const BYTE *real_start = bp(dw(g_sys, 0x110));
    g_start_kind = (int)rnd(5);
    int real_kind = -1;
    if (real && real_start && pw::readable(real_start, 8))
        for (int i = 0; i < 5; i++)
            if (dw(const_cast<BYTE *>(real_start), 0) == START_VTABLE[i] && pw::readable(real_start, START_SIZE[i]))
                real_kind = i;
    if (real_kind >= 0)
        g_start_kind = real_kind;
    g_start_obj = static_cast<BYTE *>(at<Create0>(START_CREATE[g_start_kind])());
    if (real_kind >= 0)
        memcpy(g_start_obj + 8, real_start + 8, START_SIZE[g_start_kind] - 8);   // the envelopes' values (their vtables the same)
    else
        randomize_start(g_start_obj, g_start_kind);
    dw(g_sys, 0x110) = rnd(8) ? (DWORD)(uintptr_t)g_start_obj : 0;
    if (!real) {
        random_envelope(g_sys + 0x10, 0, 80);   // the spread angles (degrees)
        random_envelope(g_sys + 0x34, 0, 60);
        random_envelope(g_sys + 0x58, -180, 180);   // the face angle
        random_envelope(g_sys + 0x7c, -90, 90);     // the trajectory
        g_sys[0x108] = (BYTE)rnd(2);
    }
    return true;
}
void free_start()
{
    if (g_start_obj)
        vcall<void *>(g_start_obj, 1, 1);
    g_start_obj = nullptr;
}

// The emitter's position (the original's 0x52e080, random state put back)
void origin(float *o)
{
    g_statics.restore();
    original<void(THISCALL *)(void *, void *, float *, DWORD, DWORD)>(0x52e080)(g_sys, g_part, o, 0, 1);
    g_statics.restore();
}

void check_systems(Tally &t)
{
    g_seed = 0x530830;
    const int ncases = scaled(600) + g_nreal * 2;
    for (int k = 0; k < ncases; k++) {
        if (!make_system(k))
            return;
        float o[3];
        origin(o);
        begin_case(random());
        FrameSpec s;
        s.width = rnd_range(160, 320);
        s.height = rnd_range(120, 240);
        const int ox = (o[0] == o[0] && o[0] > -30000 && o[0] < 30000) ? (int)o[0] : 0, oy = (o[1] == o[1] && o[1] > -30000 && o[1] < 30000) ? (int)o[1] : 0;
        s.x = (short)(ox - s.width / 2 + rnd_range(-40, 40));
        s.y = (short)(oy - s.height / 2 + rnd_range(-40, 40));
        const int fi = make_frame(s);
        if (fi < 0) {
            free_start();
            continue;
        }
        const int what = k < g_nreal * 2 ? 0 : (int)rnd(7);
        float c[3] = {rnd_float(-60, 60) + o[0], rnd_float(-60, 60) + o[1], rnd(3) ? 0.0f : rnd_float(-40, 80)};
        float d[3] = {rnd_float(-1, 1), rnd_float(-1, 1), rnd_float(-1, 1)};
        const float len = rnd_float(0, 90), r1v = rnd_float(0, 60), r2v = rnd(3) ? rnd_float(0, 60) : 0.0f, h = rnd(3) ? rnd_float(-50, 100) : 0.0f;
        const float face = rnd_float(-400, 400), traj = rnd_float(-200, 200), ratio = rnd(3) ? rnd_float(0.1f, 2) : 1.0f;
        const float angle = rnd_float(-7, 14);
        static const char *const names[7] = {"slot 41 (0x530830)", "start type slot 24", "cone 0x52ecd0", "cylinder shape 0x52cfb0", "circle 0x52b010",
                                             "circle point 0x52af80", "start type slot 24 (fake centre)"};
        char what_text[96];
        wsprintfA(what_text, "%s, %s, case %d", names[what], START_NAME[g_start_kind], k);
        const DWORD fc = 0x6d0248, fc2 = 0x6d0338;
        hudcheck::run_sides(t, g_hooks, what_text, [&](int side) -> DWORD {
            g_statics.restore();
            CodePatch ax;
            patch_jump(ax, 0x5d0863, (void *)&fake_atexit);
            void *f = frame(side, fi);
            DWORD r = 0;
            float out[3] = {};
            switch (what) {
            case 0:
                at<void(THISCALL *)(void *, void *, void *)>(0x530830)(g_sys, g_part, f);
                break;
            case 1:
            case 6:
                vcall<void>(g_start_obj, 24, g_part, f, g_sys, what == 1 ? o : c);
                break;
            case 2:
                at<void(THISCALL *)(void *, void *, void *, const float *, const float *, float, float, float, float, DWORD)>(0x52ecd0)(
                    g_sys, g_part, f, d, c, len, r1v, face, traj, fc);
                break;
            case 3: {
                // the helper reads only the ratio +0x54 of its object
                ff(g_ratio_obj, 0x54) = ratio;
                at<void(THISCALL *)(void *, void *, void *, void *, const float *, float, float, float, float, float, DWORD, DWORD)>(0x52cfb0)(
                    g_ratio_obj, g_part, f, g_sys, c, h, r1v, r2v, face, traj, fc2, fc);
                break;
            }
            case 4:
                at<void(__fastcall *)(void *, void *, void *, const float *, float, float, float, float, DWORD)>(0x52b010)(g_part, f, g_sys, c, r1v, face, traj,
                                                                                                                   ratio, fc);
                break;
            default:
                at<void(__fastcall *)(void *, const float *, float, float, float, float, float, void *, float *)>(0x52af80)(g_part, c, angle, r1v, face, traj,
                                                                                                                     ratio, g_sys, out);
                r = fnv(2166136261u, out, 12);
            }
            unpatch(ax);
            r ^= g_statics.hash();
            g_statics.restore();
            return r;
        });
        free_start();
    }
    log("  particle debug: %d real systems", g_nreal);
}

void check_point_lists(Tally &t)
{
    g_seed = 0x536480;
    for (int k = 0; k < scaled(300); k++) {
        begin_case(random());
        FrameSpec s;
        s.width = rnd_range(40, 200);
        s.height = rnd_range(30, 160);
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        int pts[2][14];
        const DWORD n = rnd(7);
        for (DWORD i = 0; i < 14; i++)
            pts[0][i] = rnd_range(-20, (i & 1) ? s.height + 20 : s.width + 20);
        memcpy(pts[1], pts[0], sizeof pts[0]);
        BYTE list[2][0x18];
        char what[48];
        wsprintfA(what, "point list, %d points, case %d", n, k);
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            memset(list[side], 0, 0x18);
            dw(list[side], 0x10) = (DWORD)(uintptr_t)pts[side];
            dw(list[side], 0x14) = n;
            g_statics.restore();
            CodePatch ax;
            patch_jump(ax, 0x5d0863, (void *)&fake_atexit);
            const DWORD r = at<DWORD(THISCALL *)(void *, void *)>(0x536480)(list[side], frame(side, fi));
            unpatch(ax);
            const DWORD h = g_statics.hash();
            g_statics.restore();
            return n <= 1 ? r ^ h : h;   // eax of the last rectangle is the frame's (the call log has it)
        });
    }
}

void check_hue_table(Tally &t)
{
    g_seed = 0x5280e0;
    BYTE *gp = *at<BYTE **>(0x633f98);
    for (int k = 0; k < scaled(120); k++) {
        BYTE pal[768];
        if (gp && k % 3 == 0)
            memcpy(pal, gp, 768);
        else
            for (int i = 0; i < 768; i++)
                pal[i] = rnd(6) ? (BYTE)random() : 0;
        DWORD src[3] = {(DWORD)(uintptr_t)pal, 0x100, 0};
        float p[3] = {rnd_float(-60, 420), rnd_float(-60, 420), rnd(6) ? rnd_float(0, 180) : 0.0f};
        const BYTE clear = (BYTE)rnd(2);
        BYTE table[2][256], init[256];
        for (int i = 0; i < 256; i++)
            init[i] = (BYTE)random();
        for (int side = 0; side < 2; side++) {
            memcpy(table[side], init, 256);
            DWORD dst[3] = {(DWORD)(uintptr_t)table[side], 0, 0};
            if (side == 0)
                g_hooks.out();
            at<void(THISCALL *)(void *, void *, const float *, BYTE)>(0x5280e0)(dst, src, p, clear);
            if (side == 0)
                g_hooks.in();
        }
        char d[96];
        wsprintfA(d, "[hue table case %d, clear %d: %x / %x]", k, clear, fnv(2166136261u, table[0], 256), fnv(2166136261u, table[1], 256));
        t.check(g_hooks.fits && !memcmp(table[0], table[1], 256), "", d);
    }
}

}  // namespace

ZAX_CHECKS(particle_debug)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    g_statics.save();
    find_systems();
    Tally systems{"particle debug: the particle systems' and start point types' drawing"}, lists{"particle debug: CPointList's drawing"},
        hue{"particle debug: the hue remap table 0x5280e0"};
    check_systems(systems);
    check_point_lists(lists);
    check_hue_table(hue);
    systems.report();
    lists.report();
    hue.report();
    g_statics.restore();
}

}  // namespace zax
