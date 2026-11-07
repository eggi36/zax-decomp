// The bitmap fonts (phase 4, rendering): CFont's class and the text functions that measure and draw a string
// with its glyph frames (docs/DECOMPILATION.md 9, "Sprites, entities and effects"). The cached layout of a text
// box (0x4a6190, 0x4a6d10) and the word-wrapped drawing (0x4a6790, 0x4a68b0, 0x4a6ae0) are in font_layout.cpp;
// the fonts' loading (0x4a5e90 cutting the glyphs out of a TGA, 0x4a62d0 the loader) is in text_rest4a.cpp.
//
// CFont (0x84, vtable 0x5f437c, class 0x6be1d4, version 5, parent CSequence) [confirmed: Create 0x4a5990,
// RegisterFields 0x4a5bb0, the text functions]: a sequence whose frames are the glyphs of the characters
// "First ASCII" (+0x3c, 32) to "Last ASCII" (+0x40, 125), +0x1c bit 0x20 "Fixed Width", +0x38 the blend
// mode the glyphs are drawn with (the Transparent Mask), +0x44/+0x48/+0x4c a text box's cached layout (+0x4c
// a heap copy of the text), +0x80 the line height above a glyph's slot 44 (an unnamed field; the loader sets
// it to the rows above the glyphs' base line). "Frames Per Second" is unnamed and hidden for fonts.
//   Glyph lookup: characters below 0x14 draw nothing; the index is the character minus "First ASCII", 0 when
//   beyond the range (the drawing compares with the frame count, the measuring with last - first + 1, so the
//   measuring can read the entry one past the last glyph). When "Last ASCII" + 1 is below 0x60 the font has
//   no lower case letters and the drawing turns the text into capitals (the C runtime's toupper).
#include "class_info.h"
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_FONT = 0x5f437c, VT_SEQUENCE = 0x5f43f0, FONT_VAR = 0x6be1d4;
constexpr DWORD TRANSPARENT_MASK = 0x6bcc68;
constexpr DWORD BIT_FIELD_CTOR = 0x54dfc0;   // 0x34

CClassInfo *sequence_class() { return fn<Getter>(0x56ab50)(); }
CClassInfo *frame_class() { return fn<Getter>(0x4a8760)(); }
BYTE *glyph(BYTE *font, DWORD i) { return bp(reinterpret_cast<DWORD *>(bp(dw(font, 8)))[i]); }
int glyph_width(BYTE *g) { return vcall<int>(g, 45); }
DWORD blit_at(void *frame, void *g, int x, int y, DWORD mode) { return fn<DWORD(THISCALL *)(void *, void *, int, int, DWORD)>(0x4b3280)(frame, g, x, y, mode); }
// The width the measuring code reads for a character (0 below 0x14; the index range last - first + 1)
int measure_width(BYTE *font, DWORD c)
{
    if (c < 0x14)
        return 0;
    DWORD i = c - dw(font, 0x3c);
    if (i > dw(font, 0x40) - dw(font, 0x3c) + 1)
        i = 0;
    return glyph_width(glyph(font, i));
}

}  // namespace

// ---- The class --------------------------------------------------------------------------------------------

// 0x4a5910, 0x4a5b40
static CClassInfo *__cdecl Font_ClassInfo()
{
    return class_getter(FONT_VAR, 0x6139d8 /* "CFont" */, 5.0f, sequence_class, 0x84, 0, 0x4a5990, 0x4a59f0, 0x4a5bb0);
}
static void font_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(VT_SEQUENCE);
    fn<BYTE *(THISCALL *)(void *)>(0x56b730)(o);
    vtable_and_defaults(o, vt(VT_FONT));
    dw(o, 0x38) = glob_addr(TRANSPARENT_MASK);
    dw(o, 0x48) = 0;
    dw(o, 0x4c) = 0;
    dw(o, 0x44) = 0;
}
// 0x4a5e50 (thiscall): the out-of-line constructor
static BYTE *THISCALL Font_Ctor(BYTE *o)
{
    font_construct(o);
    return o;
}
static void font_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_FONT);
    if (dw(o, 0x4c)) {
        heap_free(bp(dw(o, 0x4c)));
        dw(o, 0x4c) = 0;
    }
    obj_free_fields(o);
    fn<void(THISCALL *)(void *)>(0x56b760)(o);
}
// 0x4a5990
static void *__cdecl Font_Create() { return create_one<0x84, font_construct>(); }
// 0x4a59f0 (count in ecx)
static void *__fastcall Font_CreateArray(int count, int, int) { return create_array<0x84, font_construct>(count); }
// 0x4a5a80 (slot 1)
static void *THISCALL Font_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x84, font_destroy>(self, flags); }

// 0x4a5bb0
static DWORD __cdecl Font_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(TEXT_EMPTY), 0x80, 4, 0x80, 0, 0, 0, 0, 0);
    add_field(Font_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x613a0c) /* "First ASCII" */, 0x3c, 4, 0x3c, 1, 0x20, 0, 0, 0xff);
    add_field(Font_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x613a00) /* "Last ASCII" */, 0x40, 4, 0x40, 1, 0x7d, 0, 0, 0xff);
    add_field(Font_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x6139f4) /* "Fixed Width" */, 0x1c, 4, 0x1c, 0x20, 0, 1, 0);
    add_field(Font_ClassInfo(), f);
    // "Frames Per Second" of CSequence: unnamed for fonts
    BYTE *fps = find_field(Font_ClassInfo(), glob_addr(0x6139e0) /* "Frames Per Second" */, 0);
    const DWORD r = set_name(fps, glob_addr(TEXT_EMPTY));
    dw(fps, 0x18) = 0;
    return r;
}

// ---- Measuring ----------------------------------------------------------------------------------------------

// The line height: +0x80 plus the first glyph's slot 44, +0x80 alone without one
static int line_height(BYTE *font)
{
    if (dw(font, 8)) {
        const DWORD n = vcall<DWORD>(font, 25);
        for (DWORD i = 0; i < n; i++)
            if (BYTE *g = vcall<BYTE *>(font, 23, i))
                return vcall<int>(g, 44) + (int)dw(font, 0x80);
    }
    return (int)dw(font, 0x80);
}

// 0x4a6140 (fastcall font): the line height
static int __fastcall Font_LineHeight(BYTE *font) { return line_height(font); }

// 0x4a6290 (font; character): the character's glyph width (0 below 0x14)
static int THISCALL Font_CharWidth(BYTE *font, DWORD c) { return measure_width(font, c); }

// 0x4a6be0 (fastcall sequence): the height of the sequence's bounding rectangle (+0x18 - +0x10)
static int __fastcall Sequence_Height(BYTE *seq) { return (int)dw(seq, 0x18) - (int)dw(seq, 0x10); }

// 0x4a6bf0 (font; text): the lowest bottom edge of the text's glyphs (CFrame_BoundingBox 0x4ab7b0), 0 at
// least; characters outside first .. last + 1 (signed) count as the first glyph
static int THISCALL Font_TextBottom(BYTE *font, const char *text)
{
    int bottom = 0;
    for (const char *p = text; *p; p++) {
        const DWORD c = (DWORD)(int)*p;
        const DWORD i = (c < dw(font, 0x3c) || c > dw(font, 0x40) + 1) ? 0 : c - dw(font, 0x3c);
        BYTE *g = glyph(font, i);
        if (g && !is_kind_of(g, frame_class()))
            g = nullptr;
        int r[4];
        fn<int *(THISCALL *)(void *, int *)>(0x4ab7b0)(g, r);
        if (r[3] > bottom)
            bottom = r[3];
    }
    return bottom;
}

// 0x4a6c80 (font; text, line spacing): the height of the text's lines
static int THISCALL Font_TextHeight(BYTE *font, const char *text, int spacing)
{
    int lines = 1;
    for (const char *p = text; *p; p++)
        if (*p == '\n')
            lines++;
    return (lines - 1) * spacing + line_height(font) * lines;
}

// ---- Drawing ------------------------------------------------------------------------------------------------

// 0x4a6470 (font; frame, x, y, text, justification, fixed character): the text drawn at (x, y) with the
// font's blend mode (Frame_BlitAt 0x4b3280), left (0), centred (1) or right (2) of x; with a fixed
// character every glyph takes that character's width, centred in it
static void THISCALL Font_DrawText(BYTE *font, void *frame, int x, int y, const BYTE *text, DWORD justify, char fixed_char)
{
    int fixed = 0;
    if (fixed_char) {
        const DWORD c = (DWORD)(int)fixed_char;
        fixed = c < 0x14 ? 0 : measure_width(font, c);
    }
    if (!text || !*text || !dw(font, 8))
        return;
    if (justify == 1 || justify == 2) {
        DWORD width;
        if (fixed) {
            int n = 0;
            for (const BYTE *p = text; *p; p++)
                if ((signed char)*p >= 0x20)
                    n++;
            width = (DWORD)(n * fixed);
        } else {
            int sum = 0;
            for (const BYTE *p = text; *p; p++)
                sum += measure_width(font, *p);
            width = (DWORD)sum;
        }
        x -= (int)(justify == 1 ? width >> 1 : width);
    }
    for (const BYTE *p = text; *p; p++) {
        DWORD c = *p;
        if (dw(font, 0x40) + 1 < 0x60)
            c = (DWORD)fn<int(__cdecl *)(int)>(0x5d0c4e)((int)c);
        if (c < 0x14)
            continue;
        DWORD i = c - dw(font, 0x3c);
        if (i >= dw(font, 0x28))
            i = 0;
        BYTE *g = glyph(font, i);
        if (fixed) {
            const int w = glyph_width(g);
            blit_at(frame, g, (fixed - w) / 2 + x, y, dw(font, 0x38));
            x += fixed;
        } else {
            blit_at(frame, g, x, y, dw(font, 0x38));
            x += glyph_width(glyph(font, i));
        }
    }
}

// 0x4a66c0 (font; frame, x, y, text, line spacing, justification): the text's lines one under the other (the
// text is cut at each '\n' while it is drawn and put back); eax the number of lines
static int THISCALL Font_DrawLines(BYTE *font, void *frame, int x, int y, char *text, int spacing, DWORD justify)
{
    int lines = 0;
    while (*text) {
        char *nl = fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(text, '\n');
        if (nl)
            *nl = 0;
        fn<void(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, justify, 0);
        lines++;
        if (!nl)
            break;
        *nl = '\n';
        text = nl + 1;
        y += line_height(font) + spacing;
    }
    return lines;
}

// 0x4a6b90 (font; frame, x, y, value, justification): the number in decimal (the C runtime's _itoa)
static void THISCALL Font_DrawInt(BYTE *font, void *frame, int x, int y, int value, DWORD justify)
{
    char text[20];
    fn<char *(__cdecl *)(int, char *, int)>(0x5dd92d)(value, text, 10);
    fn<void(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, justify, 0);
}

ZAX_HOOKS(font_text)
{
    const Hook list[] = {
#include "font_text_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
