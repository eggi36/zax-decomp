// Checks of the fonts (font_text.cpp) against the original, in the game thread.
//  - The class CFont (machinery: checks_inventory_common.h): Create, CreateArray, the deleting destructor, both
//    getters, RegisterFields (with the unnamed "Frames Per Second"), the registered descriptors.
//  - The level's real fonts (the font list 0x6be1e0, read only; their glyphs are the real compressed frames):
//    the width of every character 0-255, the line height, the bottom of random texts, the text height with
//    line spacings, the sequence height; compared: eax.
//  - Drawing with the real fonts into fake frames (the frame check harness, checks_frame_common.h; 5-6-5 and a
//    few 8-bit ones): DrawText with random texts (capitals, lower case, digits, punctuation, characters below
//    0x14 and above 0x7f), justification 0-3, no, a printable or a random fixed character, positions on and
//    off the frame; DrawLines with 1-4 lines and spacings; DrawInt with random values and the extremes. The
//    text lies in the harness's object memory (DrawLines cuts it at each '\n' and puts the '\n' back).
//    Compared: pixels, the frame calls, the text, eax where defined.
#include "checks_hud_common.h"
#include "checks_inventory_common.h"
#include "checks_frame_r1.h"

namespace zax {

int hooks_font_text(Hook *out);

namespace {

using r1::rnd;
using r1::rnd_range;

const invcheck::ClassDef CLASSES[] = {
    {"CFont", 0x84, 0x6be1d4, 0x4a5990, 0x4a59f0, 0x4a5bb0, {0x4a5910, 0x4a5b40}, true},
};

framec::ModuleHooks g_hooks{"font_text", &hooks_font_text, {}, {}, {}, 0, true};
template <typename R = DWORD, typename... A> R exe(DWORD a, void *self, A... args) { return at<R(THISCALL *)(void *, A...)>(a)(self, args...); }

constexpr int MAX_FONTS = 16;
BYTE *g_fonts[MAX_FONTS];
int g_nfonts;

void find_fonts()
{
    g_nfonts = 0;
    framec::each_list_object(0x6be1e0, [](void *o) {
        if (g_nfonts < MAX_FONTS && o && invcheck::readable(o, 0x84) && invcheck::class_of(o) &&
            at<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, at<CClassInfo *(__cdecl *)()>(0x4a5910)()) &&
            *reinterpret_cast<DWORD *>(static_cast<BYTE *>(o) + 8))
            g_fonts[g_nfonts++] = static_cast<BYTE *>(o);
    });
}

// The measuring code reads the glyph one past the last for "Last ASCII" + 1 (the original crashes there too):
// texts never contain that character
inline BYTE past_last(BYTE *font) { return (BYTE)(*reinterpret_cast<DWORD *>(font + 0x40) + 1); }
// A character whose glyph both lookups find (not "Last ASCII" + 1, no empty glyph)
bool ok_char(BYTE *font, BYTE c)
{
    if (c == past_last(font))
        return false;
    const DWORD first = *reinterpret_cast<DWORD *>(font + 0x3c), last = *reinterpret_cast<DWORD *>(font + 0x40),
                n = *reinterpret_cast<DWORD *>(font + 0x28);
    const DWORD *frames = *reinterpret_cast<DWORD **>(font + 8);
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
void random_text(char *out, int max, bool lines, BYTE *font)
{
    static const char SET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:;!?-+%/()'\"";
    const int n = rnd_range(0, max - 1);
    for (int i = 0; i < n; i++) {
        const DWORD r = rnd(40);
        char c = r < 30 ? SET[rnd(sizeof SET - 1)] : r < 33 ? (char)rnd(0x14) : r < 36 ? (char)(0x80 + rnd(0x80)) : (char)(0x14 + rnd(0x6c));
        if (lines && rnd(8) == 0)
            c = '\n';
        if (!c || !ok_char(font, (BYTE)c))
            c = (char)*reinterpret_cast<DWORD *>(font + 0x3c);
        out[i] = c;
    }
    out[n] = 0;
}

// Both sides with the module's hooks out on side 0; f(side) returns a value compared
template <typename F> void compare_eax(Tally &t, const char *what, F f)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            g_hooks.out();
        r[side] = f();
        if (side == 0)
            g_hooks.in();
    }
    char d[160];
    wsprintfA(d, "[%.100s: %x / %x]", what, r[0], r[1]);
    t.check(g_hooks.fits && r[0] == r[1], "", d);
}

void check_measure(Tally &t)
{
    g_seed = 0x4a6290;
    for (int f = 0; f < g_nfonts; f++) {
        BYTE *font = g_fonts[f];
        if (r1::g_frame_trace)
            log("  font %d: %x, characters %d-%d, %d frames", f, (DWORD)(uintptr_t)font, *reinterpret_cast<DWORD *>(font + 0x3c),
                *reinterpret_cast<DWORD *>(font + 0x40), *reinterpret_cast<DWORD *>(font + 0x28));
        compare_eax(t, "CharWidth", [&] {
            unsigned h = 2166136261u;
            for (DWORD c = 0; c < 256; c++) {
                if (c >= 0x14 && !ok_char(font, (BYTE)c))
                    continue;
                const DWORD w = exe(0x4a6290, font, c);
                h = fnv(h, &w, 4);
            }
            return h;
        });
        compare_eax(t, "LineHeight", [&] { return at<DWORD(__fastcall *)(void *)>(0x4a6140)(font); });
        compare_eax(t, "Sequence_Height", [&] { return at<DWORD(__fastcall *)(void *)>(0x4a6be0)(font); });
        for (int k = 0; k < scaled(40); k++) {
            char text[48];
            random_text(text, 40, k & 1, font);
            const int spacing = rnd_range(-4, 12);
            compare_eax(t, "TextBottom", [&] { return exe(0x4a6bf0, font, (const char *)text); });
            compare_eax(t, "TextHeight", [&] { return exe(0x4a6c80, font, (const char *)text, spacing); });
        }
    }
}

void check_draw(Tally &t)
{
    g_seed = 0x4a6470;
    int drawn = 0;
    for (int k = 0; k < scaled(1500) && g_nfonts; k++) {
        BYTE *font = g_fonts[rnd((DWORD)g_nfonts)];
        framec::begin_case(random());
        framec::FrameSpec s;
        s.format = rnd(10) ? framec::RGB565 : framec::PAL8;
        s.width = rnd_range(16, 200);
        s.height = rnd_range(8, 60);
        const int fi = framec::make_frame(s);
        const DWORD text = framec::alloc_object(96);
        if (fi < 0 || text == 0xffffffffu)
            continue;
        const int op = (int)rnd(10);
        char buf[96];
        random_text(buf, 80, op >= 7, font);
        for (int side = 0; side < 2; side++)
            lstrcpyA(framec::object<char>(side, text), buf);
        const int x = rnd_range(-60, s.width + 20), y = rnd_range(-20, s.height + 10);
        const DWORD justify = rnd(5) == 0 ? 3 : rnd(3);
        char fixed = 0;
        if (rnd(4) == 0)
            fixed = rnd(2) ? "W0iM"[rnd(4)] : (char)rnd(256);
        if (fixed && (BYTE)fixed >= 0x14 && !ok_char(font, (BYTE)fixed))
            fixed = 0;
        const int spacing = rnd_range(-3, 8);
        int value = (int)random();
        if (rnd(8) == 0)
            value = rnd(2) ? (int)0x80000000 : 0;
        char what[96];
        wsprintfA(what, "font %d op %d: %d chars, justify %d, fixed %x, at (%d, %d)", (int)(font - g_fonts[0]) & 0xff, op, lstrlenA(buf), justify,
                  (BYTE)fixed, x, y);
        if (r1::g_frame_trace)
            log("  draw case %d: %s", k, what);
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            void *fr = framec::frame(side, fi);
            char *tx = framec::object<char>(side, text);
            if (op < 7) {
                exe(0x4a6470, font, fr, x, y, (const char *)tx, justify, (DWORD)(BYTE)fixed);
                return 0;
            }
            if (op < 9)
                return exe(0x4a66c0, font, fr, x, y, tx, spacing, justify);
            exe(0x4a6b90, font, fr, x, y, value, justify);
            return 0;
        });
        drawn++;
    }
    log("  font text: %d fonts, %d draws", g_nfonts, drawn);
}

}  // namespace

ZAX_CHECKS(font_text)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    invcheck::g_hook_table = &hooks_font_text;
    if (!invcheck::arena_setup()) {
        log("Check font text: no arena");
        return;
    }
    Tally objects{"font text: Create, CreateArray, destructors"}, getters{"font text: class getters"}, fields{"font text: RegisterFields"},
        registered{"font text: registered descriptors and classes (real data)"};
    invcheck::test_classes(objects, getters, fields, registered, CLASSES, 1);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    find_fonts();
    Tally measure{"font text: measuring with the real fonts"};
    check_measure(measure);
    measure.report();
    Tally draw{"font text: DrawText, DrawLines, DrawInt into fake frames"};
    check_draw(draw);
    draw.report();
}

}  // namespace zax
