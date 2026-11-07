// Checks of the fonts' word-wrapped text (font_layout.cpp) against the original, in the game thread.
//  - The level's real fonts (the font list 0x6be1e0, read only; their glyphs are the real compressed frames).
//  - DrawWrapped (0x4a6790), DrawParagraph (0x4a68b0), DrawParagraphs (0x4a6ae0) into fake 5-6-5 frames (the
//    frame check harness) and without a frame (measuring only): random texts of words, spaces (also leading
//    and doubled), '\n' and long words, widths 0-300, spacings, all justifications, margins. Compared: pixels,
//    the frame calls, eax, the y the paragraph advances.
//  - MeasureParagraph (0x4a6d10) with both, one or no output pointers; LayoutTextBox (0x4a6190) on a copy of
//    the font object (its +0x48..+0x7c set by the case, the heap copy of the text freed afterwards).
//  Texts never contain "Last ASCII" + 1 (the measuring reads past the glyphs, in the original too). The first
//  line's width is never below the following lines' (the breaking never ends then on an empty rest, in the
//  original too; font_layout.cpp).
#include "checks_hud_common.h"
#include "checks_inventory_common.h"
#include "checks_frame_r1.h"

namespace zax {

int hooks_font_layout(Hook *out);

namespace {

using r1::rnd;
using r1::rnd_range;

framec::ModuleHooks g_hooks{"font_layout", &hooks_font_layout, {}, {}, {}, 0, true};
template <typename R = DWORD, typename... A> R exe(DWORD a, void *self, A... args) { return at<R(THISCALL *)(void *, A...)>(a)(self, args...); }

constexpr int MAX_FONTS = 16;
BYTE *g_fonts[MAX_FONTS];
int g_nfonts;

DWORD fdw(BYTE *f, DWORD off) { return *reinterpret_cast<DWORD *>(f + off); }

void find_fonts()
{
    g_nfonts = 0;
    framec::each_list_object(0x6be1e0, [](void *o) {
        if (g_nfonts < MAX_FONTS && o && invcheck::readable(o, 0x84) && invcheck::class_of(o) &&
            at<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, at<CClassInfo *(__cdecl *)()>(0x4a5910)()) && fdw(static_cast<BYTE *>(o), 8) &&
            *reinterpret_cast<DWORD *>(fdw(static_cast<BYTE *>(o), 8)))
            g_fonts[g_nfonts++] = static_cast<BYTE *>(o);
    });
}

// A character every lookup of the font finds a glyph for
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
void random_text(char *out, int max, BYTE *font)
{
    static const char LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,:;!?-'";
    int n = 0;
    const int words = rnd_range(0, 14);
    for (int w = 0; w < words && n < max - 20; w++) {
        const DWORD r = rnd(20);
        if (r == 0)
            out[n++] = '\n';
        else if (r == 1)
            out[n++] = ' ';
        const int len = rnd(12) == 0 ? rnd_range(10, 18) : rnd_range(1, 8);
        for (int i = 0; i < len && n < max - 2; i++) {
            char c = LETTERS[rnd(sizeof LETTERS - 1)];
            if (!ok_char(font, (BYTE)c))
                c = (char)fdw(font, 0x3c);
            out[n++] = c;
        }
        out[n++] = rnd(10) ? ' ' : '\n';
    }
    if (n && rnd(3) == 0)
        n--;   // no trailing separator
    out[n] = 0;
}

struct Params {
    DWORD width, justify, m1l, m1r, j2, m2l, m2r;
    int spacing;
};
Params random_params()
{
    Params p;
    p.width = rnd(8) == 0 ? rnd(20) : (DWORD)rnd_range(20, 300);
    p.spacing = rnd_range(-3, 8);
    p.justify = rnd(6) == 0 ? 3 : rnd(3);
    p.j2 = rnd(6) == 0 ? 3 : rnd(3);
    p.m2l = rnd(3) ? 0 : rnd(20);
    p.m2r = rnd(3) ? 0 : rnd(20);
    // the first line's margins never more than the following lines' (see the head comment)
    const DWORD sum = p.m2l + p.m2r;
    p.m1l = sum ? rnd(sum + 1) : 0;
    p.m1r = sum - p.m1l ? rnd(sum - p.m1l + 1) : 0;
    if (p.m2l + p.m2r > p.width) {
        p.m2l = p.m2r = p.m1l = p.m1r = 0;
    }
    return p;
}

void check_draw(Tally &t)
{
    g_seed = 0x4a6790;
    int drawn = 0;
    for (int k = 0; k < scaled(1200) && g_nfonts; k++) {
        BYTE *font = g_fonts[rnd((DWORD)g_nfonts)];
        framec::begin_case(random());
        framec::FrameSpec s;
        s.format = framec::RGB565;
        s.width = rnd_range(16, 220);
        s.height = rnd_range(8, 90);
        const int fi = framec::make_frame(s);
        const DWORD text = framec::alloc_object(256);
        const DWORD yv = framec::alloc_object(16);
        if (fi < 0 || text == 0xffffffffu || yv == 0xffffffffu)
            continue;
        char buf[256];
        random_text(buf, 200, font);
        for (int side = 0; side < 2; side++)
            lstrcpyA(framec::object<char>(side, text), buf);
        const Params p = random_params();
        const int x = rnd_range(-30, s.width), y = rnd_range(-20, s.height);
        const int op = (int)rnd(3);
        const bool with_frame = rnd(6) != 0;
        char what[128];
        wsprintfA(what, "op %d: %d chars, width %d, justify %d/%d, margins %d %d %d %d, at (%d, %d)%s", op, lstrlenA(buf), p.width, p.justify,
                  p.j2, p.m1l, p.m1r, p.m2l, p.m2r, x, y, with_frame ? "" : ", no frame");
        if (r1::g_frame_trace)
            log("  draw case %d: %s", k, what);
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            void *fr = with_frame ? static_cast<void *>(framec::frame(side, fi)) : nullptr;
            char *tx = framec::object<char>(side, text);
            int *py = framec::object<int>(side, yv);
            *py = y;
            if (op == 0)
                return exe(0x4a6790, font, fr, x, y, (const char *)tx, p.width, p.spacing, p.justify);
            if (op == 1) {
                // one paragraph: the text up to its first '\n'
                char *nl = tx;
                while (*nl && *nl != '\n')
                    nl++;
                const char keep = *nl;
                *nl = 0;
                const DWORD r = exe(0x4a68b0, font, fr, x, py, (const char *)tx, p.width, p.spacing, p.justify, p.m1l, p.m1r, p.j2, p.m2l, p.m2r);
                *nl = keep;
                return r;
            }
            return exe(0x4a6ae0, font, fr, x, y, (const char *)tx, p.width, p.spacing, p.justify, p.m1l, p.m1r, p.j2, p.m2l, p.m2r);
        });
        drawn++;
    }
    log("  font layout: %d fonts, %d draws", g_nfonts, drawn);
}

void check_measure(Tally &t)
{
    g_seed = 0x4a6d10;
    for (int k = 0; k < scaled(600) && g_nfonts; k++) {
        BYTE *font = g_fonts[rnd((DWORD)g_nfonts)];
        char buf[256];
        random_text(buf, 200, font);
        const Params p = random_params();
        const int outs = (int)rnd(4);
        const DWORD init_w = rnd(4) ? 0 : rnd(100), init_l = rnd(1000);
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                g_hooks.out();
            char tx[256];
            lstrcpyA(tx, buf);
            char *nl = tx;
            while (*nl && *nl != '\n')
                nl++;
            *nl = 0;
            DWORD widest = init_w, last = init_l;
            const DWORD r = exe(0x4a6d10, font, (const char *)tx, p.width, (DWORD)p.spacing, p.justify, p.m1l, p.m1r, p.j2, p.m2l, p.m2r,
                                outs & 1 ? &widest : (DWORD *)nullptr, outs & 2 ? &last : (DWORD *)nullptr);
            DWORD v[3] = {r, widest, last};
            h[side] = fnv(2166136261u, v, 12);
            if (side == 0)
                g_hooks.in();
        }
        char d[96];
        wsprintfA(d, "[Measure case %d: %x / %x]", k, h[0], h[1]);
        t.check(g_hooks.fits && h[0] == h[1], "", d);
    }
    // the text box layout on a copy of the font
    for (int k = 0; k < scaled(300) && g_nfonts; k++) {
        BYTE *font = g_fonts[rnd((DWORD)g_nfonts)];
        char buf[256];
        random_text(buf, 200, font);
        const Params p = random_params();
        const DWORD last_init = rnd(3) ? 0x77 : 0;
        unsigned h[2];
        DWORD vals[2][5];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                g_hooks.out();
            BYTE copy[0x84];
            memcpy(copy, font, 0x84);
            DWORD *c = reinterpret_cast<DWORD *>(copy);
            c[0x48 / 4] = (DWORD)(uintptr_t)buf;
            c[0x4c / 4] = 0;
            c[0x50 / 4] = p.width;
            c[0x54 / 4] = (DWORD)p.spacing;
            c[0x58 / 4] = p.justify;
            c[0x5c / 4] = p.m1l;
            c[0x60 / 4] = p.m1r;
            c[0x64 / 4] = p.j2;
            c[0x68 / 4] = p.m2l;
            c[0x6c / 4] = p.m2r;
            c[0x70 / 4] = c[0x74 / 4] = 0x55;
            c[0x78 / 4] = 0x66;
            c[0x7c / 4] = last_init;
            const DWORD r = at<DWORD(__fastcall *)(void *)>(0x4a6190)(copy);
            DWORD v[6] = {r, c[0x70 / 4], c[0x74 / 4], c[0x78 / 4], c[0x7c / 4], 0};
            memcpy(vals[side], v, 20);
            const char *dup = reinterpret_cast<const char *>(c[0x4c / 4]);
            unsigned hh = fnv(2166136261u, v, 20);
            if (dup) {
                hh = fnv_text(hh, dup);
                heap_free(const_cast<char *>(dup));
            }
            h[side] = hh;
            if (side == 0)
                g_hooks.in();
        }
        char d[200];
        wsprintfA(d, "[LayoutTextBox case %d: %x %d %d %d %d / %x %d %d %d %d; w %d j %d/%d m %d %d %d %d, '%.40s']", k, vals[0][0], vals[0][1], vals[0][2],
                  vals[0][3], vals[0][4], vals[1][0], vals[1][1], vals[1][2], vals[1][3], vals[1][4], p.width, p.justify, p.j2, p.m1l, p.m1r, p.m2l,
                  p.m2r, buf);
        t.check(g_hooks.fits && h[0] == h[1], "", d);
    }
}

}  // namespace

ZAX_CHECKS(font_layout)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    find_fonts();
    Tally measure{"font layout: MeasureParagraph and LayoutTextBox with the real fonts"};
    check_measure(measure);
    measure.report();
    Tally draw{"font layout: DrawWrapped, DrawParagraph, DrawParagraphs into fake frames"};
    check_draw(draw);
    draw.report();
}

}  // namespace zax
