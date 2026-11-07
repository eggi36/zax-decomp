// Checks of draw_rest4b.cpp against the original, in the game thread.
//  - The inventory window's frame and border pieces (frame check harness, hudcheck::run_sides): fake 5-6-5
//    sources and destinations (some through a COffsetClippedProxyFrame), fractions from 0 to beyond 1 (n
//    below 9 and negative), positions inside and across the edges, the border with a < 0 and a == b; the real
//    g_BlendTransparentMask, its opacity 1 afterwards.
//  - The message lines' drawing into fake 5-6-5 frames: 0-25 lines of random text with the level's loaded
//    fonts (indices into the font list whose objects are loaded), ages from 0 to beyond 12 s, margins.
//  - Adding messages (checks_inventory_common.h's arena for operator new and delete, side 0 with the hooks
//    out): lines objects in the arena with 0-25 lines and heap copies, random texts with leading and doubled
//    spaces, widths and margins, every font kind of AddWithFontKind; compared: the arena.
//  - The mission details page on fake pages with CMissionManager::Current 0x5127e0 and
//    MissionObjectives_Draw 0x509910 recording stand-ins (the world's slot 98 the exe's).
#include "checks_frame_r1.h"
#include "checks_hud_common.h"
#include "checks_inventory_common.h"
#include "ai_common.h"

namespace zax {

int hooks_draw_rest4b(Hook *out);

namespace {

using r1::rnd;
using r1::rnd_range;
using framec::FrameSpec;

framec::ModuleHooks g_hooks{"draw_rest4b", &hooks_draw_rest4b, {}, {}, {}, 0, true};

float rnd_float(float lo, float hi) { return lo + (hi - lo) * (float)rnd(100001) / 100000.0f; }
DWORD fdw(const BYTE *f, DWORD off) { return *reinterpret_cast<const DWORD *>(f + off); }

// ---- The inventory window's frame -------------------------------------------------------------------------

struct Target {
    int frame;
    DWORD proxy;
    void *on(int side) const { return proxy != ~0u ? (void *)framec::object<BYTE>(side, proxy) : (void *)framec::frame(side, frame); }
};
Target make_target(int w, int h)
{
    FrameSpec s;
    s.format = framec::RGB565;
    s.width = w;
    s.height = h;
    s.pitch = rnd(3) ? 0 : w + (int)rnd(5);
    Target t{framec::make_frame(s), ~0u};
    if (rnd(5) == 0) {
        framec::Rect clip{rnd_range(-10, w / 2), rnd_range(-10, h / 2), rnd_range(w / 2, w + 10), rnd_range(h / 2, h + 10)};
        t.proxy = framec::make_offset_clipped(t.frame, rnd_range(-15, 15), rnd_range(-15, 15), clip);
    }
    return t;
}

void check_inventory_frame(Tally &t)
{
    g_seed = 0x4db960;
    for (int k = 0; k < scaled(900); k++) {
        framec::begin_case(random());
        const Target dest = make_target(96, 90);
        FrameSpec s;
        s.format = framec::RGB565;
        s.width = rnd_range(1, 70);
        s.height = rnd_range(1, 40);
        const int src = framec::make_frame(s);
        const int x = rnd_range(-30, 90), y = rnd_range(-10, 100);
        const bool border = k % 4 == 0;
        const float f = rnd(6) == 0 ? 0.0f : rnd(6) == 0 ? rnd_float(1.0f, 1.5f) : rnd_float(0.0f, 1.0f);
        const int a = rnd(6) == 0 ? -1 - (int)rnd(3) : rnd_range(0, s.height), b = rnd(6) == 0 ? a : rnd_range(a, s.height + 2);
        hudcheck::run_sides(t, g_hooks, border ? "InventoryWindow_DrawBorder" : "InventoryWindow_DrawFrame", [&](int side) -> DWORD {
            if (border)
                at<void(__fastcall *)(void *, void *, int, int, int, int, float)>(0x4db8e0)(dest.on(side), framec::frame(side, src), x, y, a, b, f);
            else
                at<void(__fastcall *)(void *, void *, int, int, float)>(0x4db960)(dest.on(side), framec::frame(side, src), x, y, f);
            return 0;
        });
    }
    at<DWORD(THISCALL *)(void *, float)>(0x433be0)(at<void *>(0x6bcc68), 1.0f);
}

// ---- The message lines' drawing ------------------------------------------------------------------------------

constexpr int MAX_FONTS = 16;
DWORD g_font_index[MAX_FONTS];
BYTE *g_font[MAX_FONTS];
int g_nfonts;

void find_fonts()
{
    g_nfonts = 0;
    BYTE *list = at<BYTE *>(0x6be1e0);
    BYTE *records = at<BYTE *>(fdw(list, 0x30));
    const DWORD n = fdw(list, 0x34);
    for (DWORD i = 0; records && i < n && g_nfonts < MAX_FONTS; i++) {
        BYTE *o = at<BYTE *>(fdw(records, i * 0x80 + 0x78));
        if (records[i * 0x80 + 0x7f] || !o || !invcheck::readable(o, 0x84) || !invcheck::class_of(o) ||
            !at<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, at<CClassInfo *(__cdecl *)()>(0x4a5910)()) || !fdw(o, 8) ||
            !*reinterpret_cast<DWORD *>(fdw(o, 8)))
            continue;
        g_font_index[g_nfonts] = i;
        g_font[g_nfonts++] = o;
    }
}
// A character every lookup of the font finds a glyph for (checks_font_layout.cpp)
bool ok_char(BYTE *font, BYTE c)
{
    const DWORD first = fdw(font, 0x3c), last = fdw(font, 0x40), n = fdw(font, 0x28);
    if (c == (BYTE)(last + 1))
        return false;
    const DWORD *frames = reinterpret_cast<const DWORD *>(fdw(font, 8));
    DWORD i = c - first;
    if (i > last - first + 1)
        i = 0;
    DWORD j = (DWORD)(int)(signed char)c;
    j = (j < first || j > last + 1) ? 0 : j - first;
    DWORD k = c - first;
    if (k >= n)
        k = 0;
    return i < n && j < n && frames[i] && frames[j] && frames[k] && frames[0];
}
// Words of the font's characters, single and doubled spaces, a leading space now and then
void random_text(char *out, int max, BYTE *font)
{
    static const char LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,:;!?-'";
    int n = 0;
    if (rnd(4) == 0)
        out[n++] = ' ';
    const int words = rnd_range(0, 16);
    for (int w = 0; w < words && n < max - 24; w++) {
        const int len = rnd(12) == 0 ? rnd_range(10, 20) : rnd_range(1, 8);
        for (int i = 0; i < len && n < max - 3; i++) {
            char c = LETTERS[rnd(sizeof LETTERS - 1)];
            if (!ok_char(font, (BYTE)c))
                c = (char)fdw(font, 0x3c);
            out[n++] = c;
        }
        out[n++] = ' ';
        if (rnd(6) == 0)
            out[n++] = ' ';
    }
    if (n && rnd(2))
        n--;
    out[n] = 0;
}
void put_float(BYTE *p, DWORD off, float f) { memcpy(p + off, &f, 4); }

// The font kinds of AddWithFontKind whose font the list has and has loaded (FindOrFail would end the game
// on a missing name, a font not loaded would load from files): the font object, else null
const DWORD KIND_NAMES[5] = {0x618264, 0x6196d4, 0x6196cc, 0x6196c4, 0x6196b8};
BYTE *g_kind_font[5];
void find_kind_fonts()
{
    for (int k = 0; k < 5; k++) {
        g_kind_font[k] = nullptr;
        const DWORD i = vcall<DWORD>(at<void *>(0x6be1e0), 2, at<const char *>(KIND_NAMES[k]), 0xffffffffu);
        for (int f = 0; f < g_nfonts; f++)
            if (g_font_index[f] == i)
                g_kind_font[k] = g_font[f];
    }
}

// The lines objects (read only by the drawing; too big for the harness's object arena)
alignas(4) BYTE g_lines[2][0x1e84];

void check_lines_draw(Tally &t)
{
    g_seed = 0x5067f0;
    for (int k = 0; k < scaled(300) && g_nfonts; k++) {
        framec::begin_case(random());
        const Target dest = make_target(rnd_range(80, 320), rnd_range(60, 220));
        BYTE *l0 = g_lines[0];
        memset(l0, 0, sizeof g_lines[0]);
        const DWORD count = rnd(26);
        *reinterpret_cast<DWORD *>(l0) = count;
        for (DWORD i = 0; i < count; i++) {
            const int f = (int)rnd((DWORD)g_nfonts);
            *reinterpret_cast<DWORD *>(l0 + i * 0x138 + 4) = g_font_index[f];
            random_text(reinterpret_cast<char *>(l0 + i * 0x138 + 8), 120, g_font[f]);
            const float age = rnd(10) == 0 ? 0.0f : rnd(8) == 0 ? -rnd_float(0, 2) : rnd_float(0.0f, 13.0f);
            put_float(l0, (i + 1) * 0x138, age);
        }
        *reinterpret_cast<DWORD *>(l0 + 0x1e7c) = rnd(30);
        *reinterpret_cast<DWORD *>(l0 + 0x1e80) = rnd(40);
        memcpy(g_lines[1], l0, sizeof g_lines[1]);
        hudcheck::run_sides(t, g_hooks, "MessageLines_Draw", [&](int side) -> DWORD {
            at<void(THISCALL *)(void *, void *)>(0x5067f0)(g_lines[side], dest.on(side));
            return 0;
        });
    }
    at<DWORD(THISCALL *)(void *, float)>(0x433be0)(at<void *>(0x6bcc68), 1.0f);
}

// ---- Adding messages (the inventory checks' arena) -------------------------------------------------------------

int add_hooks(Hook *out) { return hooks_draw_rest4b(out); }

void check_lines_add(Tally &t)
{
    using namespace invcheck;
    g_hook_table = &add_hooks;
    if (!arena_setup()) {
        t.check(false, "", "[no arena]");
        return;
    }
    char text[260];
    for (int k = 0; k < scaled(1500) && g_nfonts; k++) {
        const unsigned seed = 0x506c30u + (unsigned)k;
        DWORD off[2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            arena_on(side, 0x4b);
            BYTE *l = static_cast<BYTE *>(arena_new(0x1e84));
            memset(l, 0, 0x1e84);
            const DWORD count = rnd(8) == 0 ? 25 : rnd(25);
            *reinterpret_cast<DWORD *>(l) = count;
            for (DWORD i = 0; i < 25; i++) {
                *reinterpret_cast<DWORD *>(l + i * 0x138 + 4) = random();
                if (rnd(3) == 0) {
                    BYTE *copy = static_cast<BYTE *>(arena_new(8));
                    memcpy(copy, "old", 4);
                    *reinterpret_cast<DWORD *>(l + i * 0x138 + 0x134) = ptrv(copy);
                }
            }
            *reinterpret_cast<DWORD *>(l + 0x1e7c) = rnd(30);
            const int f = (int)rnd((DWORD)g_nfonts);
            DWORD kind = rnd(5);
            const bool by_kind = k % 5 == 4 && g_kind_font[kind];
            random_text(text, rnd(3) ? 120 : 250, by_kind ? g_kind_font[kind] : g_font[f]);
            const int width = rnd(6) == 0 ? rnd_range(0, 60) : rnd_range(60, 420);
            side_begin(side);
            if (by_kind) {
                at<void(THISCALL *)(void *, const char *, DWORD, int)>(0x506b90)(l, text, kind, width);
            } else {
                at<void(THISCALL *)(void *, const char *, DWORD, int)>(0x506c30)(l, text, g_font_index[f], width);
            }
            side_end(side);
            off[side] = ptrv(l) - ptrv(g_arena[side]);
            arena_off();
        }
        const DWORD n = g_used[0] > g_used[1] ? g_used[0] : g_used[1];
        const bool same = off[0] == off[1] && same_region(g_arena[0], g_arena[1], n) && !g_overflow;
        char d[160];
        wsprintfA(d, "[%s seed %u: arena at %x: %x / %x, used %x / %x]", k % 5 == 4 ? "AddWithFontKind" : "Add", seed, g_diff_at, g_diff_a, g_diff_b,
                  g_used[0], g_used[1]);
        t.check(same, "", d);
        g_overflow = false;
    }
}

// ---- The mission details page -------------------------------------------------------------------------------

BYTE g_mission[16];
void *THISCALL s_current(void *manager)
{
    using namespace invcheck;
    ev(0x5127e0, ptrv(manager));
    return g_mission;
}
DWORD __fastcall s_objectives_draw(void *mission, DWORD edx, DWORD a, DWORD b, DWORD c, float d, DWORD e, DWORD f, DWORD g)
{
    using namespace invcheck;
    ev(0x509910, mission == g_mission, edx, a);
    ev(b, c, as_dword(d), e);
    ev(f, g);
    return result(0x10);
}

void check_mission_page(Tally &t)
{
    using namespace invcheck;
    g_hook_table = &add_hooks;
    CodePatch p1, p2;
    for (int k = 0; k < scaled(200); k++) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            g_seed = 0x50ff80u + (unsigned)k;
            BYTE page[0x120];
            for (DWORD i = 0; i < sizeof page; i += 4)
                *reinterpret_cast<DWORD *>(page + i) = random();
            log_reset(side);
            patch_jump(p1, 0x5127e0, (void *)&s_current);
            patch_jump(p2, 0x509910, (void *)&s_objectives_draw);
            side_begin(side);
            r[side] = at<DWORD(__fastcall *)(void *)>(0x50ff80)(page);
            side_end(side);
            unpatch(p2);
            unpatch(p1);
        }
        char d[100];
        wsprintfA(d, "[case %d: eax %x / %x, log %d / %d]", k, r[0], r[1], g_nlog[0], g_nlog[1]);
        t.check(r[0] == r[1] && same_log(), "", d);
    }
}

// ---- CGoToAI's debug drawing -------------------------------------------------------------------------------

BYTE g_target[16];
bool g_resolve_null;
float g_pos[2][2];
void *__fastcall s_resolve(DWORD, void *, void *, void *, void *, DWORD, DWORD, DWORD) { return g_resolve_null ? nullptr : g_target; }
float *THISCALL s_position(void *o, float *out)
{
    const int i = o == g_target ? 0 : 1;
    out[0] = g_pos[i][0];
    out[1] = g_pos[i][1];
    return out;
}

void check_goto(Tally &t)
{
    g_seed = 0x4d18b0;
    CodePatch p1, p2;
    for (int k = 0; k < scaled(600); k++) {
        framec::begin_case(random());
        const Target dest = make_target(112, 96);
        const DWORD ai = framec::alloc_object(0x110), part = framec::alloc_object(0x60);
        const bool named = rnd(8) != 0;
        g_resolve_null = rnd(8) == 0;
        for (int i = 0; i < 2; i++) {
            g_pos[i][0] = rnd_float(-20, 130);
            g_pos[i][1] = rnd_float(-20, 110);
        }
        float off[2] = {rnd(3) ? 0.0f : rnd_float(-40, 40), rnd(3) ? 0.0f : rnd_float(-40, 40)};
        if (rnd(10) == 0)
            off[0] = raw_float(0x7fc00000);
        const float angle = rnd_float(-7, 7);
        const BYTE arrow = (BYTE)rnd(2);
        // one string for both sides (its buffer pointer is part of the compared object)
        BYTE name[4];
        ai::zs_ctor(name);
        ai::zs_assign_text(name, named ? "Target" : "");
        for (int side = 0; side < 2; side++) {
            BYTE *a = framec::object<BYTE>(side, ai);
            memcpy(a + 0x14, name, 4);
            memcpy(a + 0x100, off, 8);
            memcpy(a + 0xf4, &angle, 4);
            a[0xd8] = arrow;
            *reinterpret_cast<DWORD *>(framec::object<BYTE>(side, part) + 0x48) = 0x12345678;
        }
        patch_jump(p1, 0x41ad40, (void *)&s_resolve);
        patch_jump(p2, 0x4fb0a0, (void *)&s_position);
        hudcheck::run_sides(t, g_hooks, "CGoToAI::DebugDraw", [&](int side) -> DWORD {
            at<void(THISCALL *)(void *, void *, void *)>(0x4d18b0)(framec::object<BYTE>(side, ai), framec::object<BYTE>(side, part), dest.on(side));
            return 0;
        });
        unpatch(p2);
        unpatch(p1);
        ai::zs_dtor(name);
    }
}

}  // namespace

ZAX_CHECKS(draw_rest4b)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    find_fonts();
    find_kind_fonts();
    Tally frame{"draw rest 4b: the inventory window's frame and border"};
    check_inventory_frame(frame);
    frame.report();
    Tally draw{"draw rest 4b: the message lines' drawing"};
    check_lines_draw(draw);
    draw.check(g_nfonts > 0, "", "[no loaded font]");
    draw.report();
    Tally add{"draw rest 4b: adding message lines"};
    check_lines_add(add);
    add.report();
    Tally go{"draw rest 4b: CGoToAI's debug drawing"};
    check_goto(go);
    go.report();
    Tally page{"draw rest 4b: the mission details page"};
    check_mission_page(page);
    page.report();
}

}  // namespace zax
