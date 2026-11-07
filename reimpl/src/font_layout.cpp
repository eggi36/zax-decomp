// The fonts' word-wrapped text (phase 4, rendering; docs/DECOMPILATION.md 9, "Effects and text: the hue change
// effect and its cache, the text overlays, …"): the cached layout of a text box and the drawing of text broken into
// lines of a width. The line breaking itself is 0x4a7510 (font; text, width, 0, ZString line out, the break test
// 0x506f60, force): it puts the next line into the ZString and returns the characters it took, 0 when nothing fits
// (with force a word longer than the width is cut). The fonts' loading (0x4a5e90, 0x4a62d0) is in
// text_rest4a.cpp.
//
// The text box layout in CFont [confirmed: 0x4a6190, its callers]: +0x48 the text, +0x4c a heap copy of it,
// +0x50 the box width, +0x54 the line spacing, +0x58/+0x64 the justification of the first and of the
// following lines, +0x5c/+0x60 the first line's left and right margins, +0x68/+0x6c the following lines',
// +0x70 the text's height, +0x74 the line count, +0x78 the widest line's right edge, +0x7c the last line's.
//
// A line's width is that of the box minus its two margins. The breaking asks 0x4a7510 for a line at the
// current width (force when it is at least the next width); when nothing fits and the next width is larger it
// takes the next width and counts an empty line. The loop ends when nothing fits and the width at the last
// line was not below the next width. [observed in the code: with an empty remaining text, a first width below
// the next width never ends (the width kept for the end test is not updated on that path), in the original
// too.] The count of characters left, which the space skipping stops at, is the text's length on the first
// line and the length of the previous line afterwards.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD BREAK_TEST = 0x506f60;

DWORD crt_strlen(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
char *crt_strchr(char *s, int c) { return fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
struct ZStr {
    DWORD buffer;
    ZStr() { zs_ctor(reinterpret_cast<BYTE *>(this)); }
    ~ZStr() { zs_dtor(reinterpret_cast<BYTE *>(this)); }
};
DWORD break_line(BYTE *font, const char *text, DWORD width, ZStr *line, bool force)
{
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD, void *, DWORD, DWORD)>(0x4a7510)(font, text, width, 0, line, fn_addr(BREAK_TEST),
                                                                                                    force);
}
void draw_text(BYTE *font, void *frame, int x, int y, const char *text, DWORD justify)
{
    fn<void(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, justify, 0);
}
// The line height: +0x80 plus the first glyph's slot 44, +0x80 alone without one
int line_height(BYTE *font)
{
    if (dw(font, 8)) {
        const DWORD n = vcall<DWORD>(font, 25);
        for (DWORD i = 0; i < n; i++)
            if (BYTE *g = vcall<BYTE *>(font, 23, i))
                return vcall<int>(g, 44) + (int)dw(font, 0x80);
    }
    return (int)dw(font, 0x80);
}
// The glyph widths of a line (characters below 0x14 count nothing, those beyond the range the first glyph)
int text_width(BYTE *font, const char *text)
{
    int sum = 0;
    if (!*text || !dw(font, 8))
        return 0;
    const DWORD range = dw(font, 0x40) - dw(font, 0x3c);
    for (const BYTE *p = reinterpret_cast<const BYTE *>(text); *p; p++) {
        if (*p < 0x14)
            continue;
        DWORD i = *p - dw(font, 0x3c);
        if (i > range)
            i = 0;
        sum += vcall<int>(bp(reinterpret_cast<DWORD *>(bp(dw(font, 8)))[i]), 45);
    }
    return sum;
}
void skip_spaces(const char *text, DWORD &pos, DWORD &left)
{
    if (text[pos] != ' ')
        return;
    while (left) {
        const char c = text[pos + 1];
        pos++;
        left--;
        if (c != ' ')
            break;
    }
}

}  // namespace

// 0x4a6790 (font; frame, x, y, text, width, line spacing, justification): the text broken into lines of the
// width and drawn (with a frame) one under the other; eax the height of the lines (line height + spacing each)
static int THISCALL Font_DrawWrapped(BYTE *font, void *frame, int x, int y, const char *text, DWORD width, int spacing, DWORD justify)
{
    DWORD left = crt_strlen(text), pos = 0;
    int height = 0;
    ZStr line;
    for (;;) {
        skip_spaces(text, pos, left);
        zs_assign_text(&line, glob<const char *>(TEXT_EMPTY));
        left = break_line(font, text + pos, width, &line, true);
        if (!left)
            break;
        pos += left;
        if (frame)
            draw_text(font, frame, x, y + height, zs_c_str(&line), justify);
        height += line_height(font) + spacing;
    }
    return height;
}

// 0x4a68b0 (font; frame, x, int *y, text, width, spacing, justification, left and right margin, then the
// following lines' justification and margins): one paragraph broken and drawn at *y, which it advances by the
// lines' height; eax the line count. The x of a line: x + left margin, centred + half the line width, right
// + the line width (DrawText justifies from there).
static int THISCALL Font_DrawParagraph(BYTE *font, void *frame, int x, int *y, const char *text, DWORD width, int spacing, DWORD justify,
                                       DWORD m_left, DWORD m_right, DWORD next_justify, DWORD next_left, DWORD next_right)
{
    DWORD cur = width - m_left - m_right, kept = cur, left = crt_strlen(text), pos = 0;
    int lines = 0, height = 0;
    ZStr line;
    for (;;) {
        skip_spaces(text, pos, left);
        zs_assign_text(&line, glob<const char *>(TEXT_EMPTY));
        const DWORD next = width - next_left - next_right;
        const DWORD n = break_line(font, text + pos, cur, &line, cur >= next);
        left = n;
        if (n) {
            pos += n;
            int lx;
            if (justify == 1)
                lx = (int)(cur >> 1) + x + (int)m_left;
            else if (justify == 2)
                lx = (int)cur + x + (int)m_left;
            else
                lx = x + (int)m_left;
            if (frame)
                draw_text(font, frame, lx, *y + height, zs_c_str(&line), justify);
            lines++;
            height += line_height(font) + spacing;
            justify = next_justify;
            m_left = next_left;
            cur = next;
            kept = cur;
            continue;
        }
        if (cur < next) {
            lines++;
            height += line_height(font) + spacing;
            justify = next_justify;
            m_left = next_left;
            cur = next;
        }
        if (!(kept < next))
            break;
    }
    *y += height;
    return lines;
}

// 0x4a6ae0 (font; the arguments of 0x4a68b0 with y by value): the text's paragraphs (split at each '\n' of a
// copy) one under the other; eax the line count
static int THISCALL Font_DrawParagraphs(BYTE *font, void *frame, int x, int y, const char *text, DWORD width, int spacing, DWORD justify,
                                        DWORD m_left, DWORD m_right, DWORD next_justify, DWORD next_left, DWORD next_right)
{
    int lines = 0;
    ZStr copy;
    zs_assign_text(&copy, text);
    char *p = fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(&copy, 0);
    while (*p) {
        char *nl = crt_strchr(p, '\n');
        if (nl)
            *nl = 0;
        lines += fn<int(THISCALL *)(void *, void *, int, int *, const char *, DWORD, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4a68b0)(
            font, frame, x, &y, p, width, spacing, justify, m_left, m_right, next_justify, next_left, next_right);
        if (!nl)
            break;
        p = nl + 1;
        *nl = '\n';
    }
    return lines;
}

// 0x4a6d10 (font; text, width, spacing (unused), justification, margins, the following lines' justification
// and margins, DWORD *widest, DWORD *last): one paragraph measured as 0x4a68b0 breaks it; with an output
// pointer a line's right edge (left margin + its glyph widths, centred (width + glyphs) / 2, right the
// width) goes into *last and raises *widest; eax the line count
static int THISCALL Font_MeasureParagraph(BYTE *font, const char *text, DWORD width, DWORD, DWORD justify, DWORD m_left, DWORD m_right,
                                          DWORD next_justify, DWORD next_left, DWORD next_right, DWORD *widest, DWORD *last)
{
    DWORD cur = width - m_left - m_right, kept = cur, left = crt_strlen(text), pos = 0, edge = 0;
    int lines = 0;
    ZStr line;
    for (;;) {
        skip_spaces(text, pos, left);
        zs_assign_text(&line, glob<const char *>(TEXT_EMPTY));
        const DWORD next = width - next_left - next_right;
        const DWORD n = break_line(font, text + pos, cur, &line, cur >= next);
        left = n;
        if (n) {
            pos += n;
            lines++;
            if (widest || last) {
                if (justify == 0)
                    edge = (DWORD)text_width(font, zs_c_str(&line)) + m_left;
                else if (justify == 1)
                    edge = ((cur + (DWORD)text_width(font, zs_c_str(&line))) >> 1) + m_left;
                else if (justify == 2)
                    edge = cur + m_left;
                else
                    edge = (DWORD)text_width(font, zs_c_str(&line)) + m_left;
            }
        } else {
            if (!(cur < next)) {
                if (!(kept < next))
                    break;
                continue;
            }
            lines++;
        }
        m_left = next_left;
        justify = next_justify;
        cur = next;
        if (n)
            kept = cur;
        if (widest && edge > *widest)
            *widest = edge;
        if (last)
            *last = edge;
        if (!n && !(kept < next))
            break;
    }
    return lines;
}

// 0x4a6190 (fastcall font): the text box's layout from +0x48 into +0x4c..+0x7c; eax the height (the first
// glyph's height (slot 39) × lines + spacing × (lines - 1))
static int __fastcall Font_LayoutTextBox(BYTE *font)
{
    int lines = 0;
    dw(font, 0x78) = 0;
    if (dw(font, 0x4c)) {
        heap_free(bp(dw(font, 0x4c)));
        dw(font, 0x4c) = 0;
    }
    const char *src = reinterpret_cast<const char *>(bp(dw(font, 0x48)));
    const DWORD n = crt_strlen(src) + 1;
    char *copy = static_cast<char *>(heap_alloc(n));
    dw(font, 0x4c) = (DWORD)(uintptr_t)copy;
    memcpy(copy, reinterpret_cast<const char *>(bp(dw(font, 0x48))), n);
    char *p = reinterpret_cast<char *>(bp(dw(font, 0x4c)));
    while (*p) {
        char *nl = crt_strchr(p, '\n');
        if (nl)
            *nl = 0;
        lines += fn<int(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD *, DWORD *)>(0x4a6d10)(
            font, p, dw(font, 0x50), dw(font, 0x54), dw(font, 0x58), dw(font, 0x5c), dw(font, 0x60), dw(font, 0x64), dw(font, 0x68), dw(font, 0x6c),
            &dw(font, 0x78), &dw(font, 0x7c));
        if (!nl)
            break;
        p = nl + 1;
        *nl = '\n';
    }
    dw(font, 0x74) = (DWORD)lines;
    const int h = vcall<int>(bp(*reinterpret_cast<DWORD *>(bp(dw(font, 8)))), 39);
    const int r = h * lines + (int)dw(font, 0x54) * (lines - 1);
    dw(font, 0x70) = (DWORD)r;
    return r;
}

ZAX_HOOKS(font_layout)
{
    const Hook list[] = {
#include "font_layout_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
