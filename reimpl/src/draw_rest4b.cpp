// Drawing leftovers of 0x4d0000-0x55ffff (phase 4, rendering and user interface): the inventory window's
// frame 0x4db8e0/0x4db960, the mission details page 0x50ff80 and the world's message lines 0x5067f0-0x506c30.
//
// The inventory window's frame [confirmed: code; VM] (fastcall destination frame, source frame; x, y,
// fraction): n = the source's height (slot 39) - ftol(height * fraction) (the height as an unsigned 64-bit
// fild). For k = 9 down to 3 the source's row n - k (its clip rectangle, slot 36, cut to that row) is blitted
// with g_BlendTransparentMask 0x6bcc68 at opacity 0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65 to (x, y - 22,
// -17, -13, -10, -7, -5, -3) when n - k >= 0; then three border pieces (0x4db8e0: rows a..b of the source at
// opacity f - 0.05 to (x, y), unless a < 0 or a == b): (y - 2, n - 2..n - 1, 0.8), (y - 1, n - 1..n, 0.9),
// (y, n..height, 1.0); two tinted lines (Frame_TintRectXY 0x4b3ce0, the colours 0x62e568 and 0x62e590) one
// and two rows below the source's top + n + y across its clip rectangle moved by x; the mask's opacity 1.
//
// The mission details page 0x50ff80 [confirmed: code; VM] (fastcall page): the world's mission manager
// (slot 98), its current mission (CMissionManager::Current 0x5127e0) and MissionObjectives_Draw 0x509910
// (fastcall mission, 0; +0x110, +0x10c, +0x108, 1.0, 0, +0x104 as a byte in +0x100's upper bytes, +0x100);
// eax its result + 2 * +0x10c.
//
// The message lines [confirmed: code; VM] (0x1e84 bytes, the application's +0x280 and the world's): +0 the
// count (at most 25), line i at +i * 0x138: +4 the font (an index into the font list 0x6be1e0), +8 the text
// (300 bytes), +0x134 a heap copy of the whole message; line i's age (float, seconds) at +(i + 1) * 0x138;
// +0x1e7c the left margin, +0x1e80 the bottom margin.
//  - Draw 0x5067f0 (lines; frame): from the first line until one whose age is 0 or less: y is the frame's
//    height - ftol(age * 7) - the bottom margin for the first line, and grows by fistp(float(previous age * 7
//    - age * 7)) for the others; the font (the list's record object, loaded through the list's slots 1 and
//    37 when missing; none while it loads or without records), the opacity 2 * age below 0.5 s and
//    2 * (12 - age) above 11.5 s (else unchanged), CFont::DrawText 0x4a6470 (frame, left margin, y, text,
//    0, 0), the opacity 1.
//  - Add 0x506c30 (lines; text, font, width): nothing with 25 lines; the text broken into lines of the width
//    less twice the left margin (the font's line breaking 0x4a7510 with the break test 0x506f60, at most 300
//    characters, forced): each line's leading spaces skipped, its text, font and a heap copy of the whole
//    text stored, the next age 0, the count raised (at most 25); an empty piece clears the line's text and
//    copy and ends.
//  - AddWithFontKind 0x506b90 (lines; text, kind, width): the font "Courier", "ShipFont", "ZaxFont",
//    "OmFont" or "OtherFont" (NamedList_FindOrFail 0x591fc0; other kinds 0) and Add.
//
// CGoToAI's debug drawing 0x4d18b0 (slot 43; part, frame) [confirmed: code; VM]: with a target name (+0x14,
// resolved by Action_ResolveOneZ 0x41ad40 in the part's layer) a thick yellow line (Frame_DrawThickLinePts
// 0x4b3cb0) from the part to the target moved by the offset (+0x100, +0x104), a yellow cross (the frame's slot
// 52: (-3, -3)-(4, 4) and (3, -3)-(-4, 4)) at the target and, with an offset, the same cross at the moved
// point; with +0xd8 an arrow (DrawView_Arrow 0x4fceb0) at the moved point at the angle +0xf4 (ratio
// 0x3f3677d4, (200, 205, 25)).
// Quirks kept [confirmed: code]: the second cross is meant to be red: a red FieldColor is made but the lines
// take the yellow one. The space skipping stops at a count that is the text's length only for the
// first line and then the previous line's length; the heap copy holds the whole text in every line.
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

using LD = long double;

constexpr DWORD MASK = 0x6bcc68, FONT_LIST = 0x6be1e0;

void set_opacity(float f) { fn<DWORD(THISCALL *)(void *, float)>(0x433be0)(glob<void *>(MASK), f); }
void blit_rect_at(void *dest, void *src, const int *r, int x, int y)
{
    fn<DWORD(THISCALL *)(void *, void *, const int *, int, int, void *)>(0x4b3390)(dest, src, r, x, y, glob<void *>(MASK));
}
void tint_rect(void *dest, int l, int t, int r, int b, DWORD colour)
{
    fn<DWORD(THISCALL *)(void *, int, int, int, int, const void *)>(0x4b3ce0)(dest, l, t, r, b, at<const void *>(colour));
}
void clip_rect(void *f, int *r) { vcall<DWORD>(f, 36, r); }

// The font list's record object at an index (the inline lookup of the message lines): none without records
// or while it loads; loaded through the list's slots 1 and 37 when missing
BYTE *font_object(DWORD index)
{
    BYTE *records = bp(dw(glob<void *>(FONT_LIST), 0x30));
    if (!records)
        return nullptr;
    if (index >= dw(glob<void *>(FONT_LIST), 0x34))
        index = 0;
    const DWORD off = index << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records, off + 0x78) && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        const char *name = vcall<const char *>(glob<void *>(FONT_LIST), 1, index);
        char buf[0x100];
        fn<void(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(buf, glob<const char *>(0x6be218), name);
        char *pn = const_cast<char *>(path_name(buf));
        DWORD r;
        if (pn == buf) {
            r = vcall<DWORD>(glob<void *>(FONT_LIST), 37, glob<const char *>(0x62e558), pn, bp(dw(glob<void *>(FONT_LIST), 0x30)) + off + 0x78);
        } else {
            pn[-1] = 0;
            r = vcall<DWORD>(glob<void *>(FONT_LIST), 37, buf, pn, bp(dw(glob<void *>(FONT_LIST), 0x30)) + off + 0x78);
        }
        dw(bp(dw(glob<void *>(FONT_LIST), 0x30)), off + 0x78) = r;
        bp(dw(glob<void *>(FONT_LIST), 0x30))[off + 0x7f] = 0;
        records = bp(dw(glob<void *>(FONT_LIST), 0x30));
    }
    return bp(dw(records, off + 0x78));
}

DWORD c_len(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

}  // namespace

// 0x4d18b0
void THISCALL GoToAI_DebugDraw(BYTE *ai, BYTE *part, void *frame)
{
    if (!(fn<DWORD(THISCALL *)(void *)>(0x4e13e0)(ai + 0x14) & 0xff))
        return;
    void *target = fn<void *(__fastcall *)(DWORD, void *, void *, void *, void *, DWORD, DWORD, DWORD)>(0x41ad40)(dw(part, 0x48), ai + 0x14, part, part,
                                                                                                          ai, 0, 0, 0);
    if (!target)
        return;
    float p[2];
    fn<float *(THISCALL *)(void *, float *)>(0x4fb0a0)(target, p);
    float a[2];
    a[0] = fstp((LD)p[0] + fl<float>(ai, 0x100));
    a[1] = fstp((LD)p[1] + fl<float>(ai, 0x104));
    alignas(4) BYTE yellow[16], red[16];
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(yellow, 0xff, 0xff, 0, 0xff);
    float q[2];
    fn<float *(THISCALL *)(void *, float *)>(0x4fb0a0)(part, q);
    fn<DWORD(THISCALL *)(void *, const float *, const float *, const void *)>(0x4b3cb0)(frame, q, a, yellow);
    using Line = DWORD(THISCALL *)(void *, float, float, float, float, const void *);
    const LD three = cst(glob_addr(0x5ec838)), four = cst(glob_addr(0x5eca50));
    (*reinterpret_cast<Line *const *>(frame))[52](frame, fstp((LD)p[0] - three), fstp((LD)p[1] - three), fstp((LD)p[0] + four), fstp((LD)p[1] + four), yellow);
    (*reinterpret_cast<Line *const *>(frame))[52](frame, fstp((LD)p[0] + three), fstp((LD)p[1] - three), fstp((LD)p[0] - four), fstp((LD)p[1] + four), yellow);
    if (!c3(fl<float>(ai, 0x100), cst(glob_addr(C_ZERO))) || !c3(fl<float>(ai, 0x104), cst(glob_addr(C_ZERO)))) {
        fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(red, 0xff, 0, 0, 0xff);
        (*reinterpret_cast<Line *const *>(frame))[52](frame, fstp((LD)a[0] - three), fstp((LD)a[1] - three), fstp((LD)a[0] + four), fstp((LD)a[1] + four),
                                                      yellow);
        (*reinterpret_cast<Line *const *>(frame))[52](frame, fstp((LD)a[0] + three), fstp((LD)a[1] - three), fstp((LD)a[0] - four), fstp((LD)a[1] + four),
                                                      yellow);
    }
    if (ai[0xd8])
        fn<void(__fastcall *)(void *, const float *, float, float, DWORD, DWORD, DWORD)>(0x4fceb0)(frame, a, fl<float>(ai, 0xf4), raw_float(0x3f3677d4),
                                                                                                0xc8, 0xcd, 0x19);
}

// 0x4db8e0
void __fastcall InventoryWindow_DrawBorder(void *dest, void *src, int x, int y, int a, int b, float f)
{
    if (a < 0 || a == b)
        return;
    set_opacity(fstp((LD)f - cst(glob_addr(0x5f8334))));
    int r[4];
    clip_rect(src, r);
    const int top = r[1];
    r[1] = top + a;
    r[3] = top + b;
    blit_rect_at(dest, src, r, x, y);
}

// 0x4db960
void __fastcall InventoryWindow_DrawFrame(void *dest, void *src, int x, int y, float fraction)
{
    int clip[4];
    clip_rect(src, clip);
    const DWORD h = vcall<DWORD>(src, 39);
    const int n = (int)h - x87_ftol(fild64(h) * fraction);
    struct Step {
        int k;
        DWORD opacity;
        int dy;
    };
    const Step steps[] = {{9, 0x3d4ccccd, -0x16}, {8, 0x3e19999a, -0x11}, {7, 0x3e800000, -0xd}, {6, 0x3eb33333, -0xa},
                          {5, 0x3ee66666, -7},    {4, 0x3f0ccccd, -5},    {3, 0x3f266666, -3}};
    for (const Step &s : steps) {
        if (n - s.k < 0 || n - s.k == n - s.k + 1)
            continue;
        set_opacity(raw_float(s.opacity));
        int r[4];
        clip_rect(src, r);
        const int top = r[1];
        r[1] = top + n - s.k;
        r[3] = top + n - s.k + 1;
        blit_rect_at(dest, src, r, x, y + s.dy);
    }
    fn<void(__fastcall *)(void *, void *, int, int, int, int, float)>(0x4db8e0)(dest, src, x, y - 2, n - 2, n - 1, raw_float(0x3f4ccccd));
    fn<void(__fastcall *)(void *, void *, int, int, int, int, float)>(0x4db8e0)(dest, src, x, y - 1, n - 1, n, raw_float(0x3f666666));
    fn<void(__fastcall *)(void *, void *, int, int, int, int, float)>(0x4db8e0)(dest, src, x, y, n, (int)h, raw_float(0x3f800000));
    const int line = clip[1] + n + y;
    tint_rect(dest, clip[0] + x, line, clip[2] + x, line + 1, glob_addr(0x62e568));
    tint_rect(dest, clip[0] + x, line + 1, clip[2] + x, line + 2, glob_addr(0x62e590));
    set_opacity(1.0f);
}

// 0x50ff80
DWORD __fastcall DrawMissionDetailsPage(BYTE *page)
{
    void *manager = vcall<void *>(*glob<void **>(0x6c2080), 98);
    void *mission = fn<void *(THISCALL *)(void *)>(0x5127e0)(manager);
    const DWORD a100 = dw(page, 0x100);
    const DWORD r = fn<DWORD(__fastcall *)(void *, DWORD, DWORD, DWORD, DWORD, float, DWORD, DWORD, DWORD)>(0x509910)(
        mission, 0, dw(page, 0x110), dw(page, 0x10c), dw(page, 0x108), 1.0f, 0, (a100 & 0xffffff00) | page[0x104], a100);
    return r + dw(page, 0x10c) * 2;
}

// 0x5067f0
void THISCALL MessageLines_Draw(BYTE *lines, void *frame)
{
    int y = 0;
    for (DWORD i = 0; i < dw(lines, 0); i++) {
        const float age = fl<float>(lines, (i + 1) * 0x138);
        if (c03(age, cst(glob_addr(C_ZERO))))
            return;
        if (i) {
            const float d = fstp((LD)fl<float>(lines, i * 0x138) * cst(glob_addr(0x5eeeac)) - (LD)age * cst(glob_addr(0x5eeeac)));
            y += x87_fistp(fld(d));
        } else {
            const int h = (int)vcall<DWORD>(frame, 39);
            y = h - x87_ftol((LD)fl<float>(lines, 0x138) * cst(glob_addr(0x5eeeac))) - (int)dw(lines, 0x1e80);
        }
        BYTE *line = lines + i * 0x138;
        BYTE *font = font_object(dw(line, 4));
        const float t = fl<float>(lines, (i + 1) * 0x138);
        if (c0(t, cst(glob_addr(C_HALF))))
            set_opacity(fstp((LD)t + t));
        else if (!c03(t, cst(glob_addr(0x5f9e34)))) {
            const LD v = cst(glob_addr(0x5ec3f8)) - t;
            set_opacity(fstp(v + v));
        }
        fn<DWORD(THISCALL *)(void *, void *, DWORD, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, dw(lines, 0x1e7c), y,
                                                                                            reinterpret_cast<const char *>(line + 8), 0, 0);
        set_opacity(1.0f);
    }
}

// 0x506c30
void THISCALL MessageLines_Add(BYTE *lines, const char *text, DWORD font_index, int width)
{
    if (dw(lines, 0) == 0x19)
        return;
    const int w = width - (int)(dw(lines, 0x1e7c) << 1);
    BYTE *font = font_object(font_index);
    DWORD rest = c_len(text);
    DWORD pos = 0;
    BYTE z[4];
    zs_ctor(z);
    if (dw(lines, 0) == 0x19) {
        zs_dtor(z);
        return;
    }
    for (;;) {
        if (text[pos] == ' ') {
            while (rest) {
                const char c = text[pos + 1];
                pos++;
                rest--;
                if (c != ' ')
                    break;
            }
        }
        zs_assign_text(z, glob<const char *>(0x62e558));
        const DWORD n = fn<DWORD(THISCALL *)(void *, const char *, int, DWORD, void *, DWORD, DWORD)>(0x4a7510)(font, text + pos, w, 0x12c, z,
                                                                                                         fn_addr(0x506f60), 1);
        BYTE *line = lines + dw(lines, 0) * 0x138;
        if (n) {
            if (dw(line, 0x134)) {
                heap_free(bp(dw(line, 0x134)));
                dw(lines + dw(lines, 0) * 0x138, 0x134) = 0;
            }
            char *copy = static_cast<char *>(heap_alloc(c_len(text) + 1));
            dw(lines + dw(lines, 0) * 0x138, 0x134) = ptr_value(copy);
            memcpy(bp(dw(lines + dw(lines, 0) * 0x138, 0x134)), text, c_len(text) + 1);
            const char *piece = zs_c_str(z);
            memcpy(lines + dw(lines, 0) * 0x138 + 8, piece, c_len(piece) + 1);
            pos += n;
            dw(lines + dw(lines, 0) * 0x138, 4) = font_index;
            dw(lines, (dw(lines, 0) + 1) * 0x138) = 0;
            const DWORD count = dw(lines, 0) + 1;
            dw(lines, 0) = count > 0x19 ? 0x19 : count;
        } else {
            line[8] = 0;
            if (dw(line, 0x134)) {
                heap_free(bp(dw(line, 0x134)));
                dw(lines + dw(lines, 0) * 0x138, 0x134) = 0;
            }
        }
        zs_dtor(z);
        if (!n)
            return;
        zs_ctor(z);
        if (dw(lines, 0) == 0x19)
            break;
        rest = n;
    }
    zs_dtor(z);
}

// 0x506b90
void THISCALL MessageLines_AddWithFontKind(BYTE *lines, const char *text, DWORD kind, int width)
{
    static_assert(sizeof(DWORD) == 4);
    DWORD name = 0;
    switch (kind) {
    case 0: name = glob_addr(0x618264); break;   // "Courier"
    case 1: name = glob_addr(0x6196d4); break;   // "ShipFont"
    case 2: name = glob_addr(0x6196cc); break;   // "ZaxFont"
    case 3: name = glob_addr(0x6196c4); break;   // "OmFont"
    case 4: name = glob_addr(0x6196b8); break;   // "OtherFont"
    }
    const DWORD index = name ? fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591fc0)(glob<void *>(FONT_LIST), at<const char *>(name), 0xffffffff) : 0;
    fn<void(THISCALL *)(void *, const char *, DWORD, int)>(0x506c30)(lines, text, index, width);
}

ZAX_HOOKS(draw_rest4b)
{
    const Hook list[] = {
#include "draw_rest4b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
