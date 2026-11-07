// The fonts' text measuring and line breaking, the cached text box layout's three getters, the font list's
// slots, the loading of a font from a .tga strip (phase 4, rendering; docs/DECOMPILATION.md 9, "Phase 4
// remains, band A").
//
// A glyph's width [confirmed: code]: characters below 0x14 count 0; any other character c is looked up as
// glyph c - first (+0x3c; above last (+0x40) - first: glyph 0) and measured through its slot 45; a font
// without glyphs (+8 null) measures 0.
// - TextWidthN 0x4a7310 (font; text, n): the first n characters (to the NUL).
// - MaxLineWidth 0x4a7380 (font; text): the widest of the lines split at '\n' (strchr), unsigned.
// - BreakLine 0x4a7510 (font; text, width, limit, ctx, callback, force): the words ("word": the characters to
//   the next ' ', '\n' or NUL, plus that ' ' or '\n'; a leading ' ' is a word of one) that fit into the width
//   and the limit (0: the text's length + 1); the callback (fastcall ctx, word, its length) is called for
//   every word taken, the first time with length 0. A '\n' ends the line after its word. Nothing fits: with
//   force as many single characters as fit (the callback once with the text and their count), else 0.
//   eax: the characters taken.
// - WrapText 0x4a7450 (font; out, text, width): the text broken into lines with force, each line without its
//   trailing spaces and newlines, joined with "\n"; out is the returned ZString.
// - The text box getters 0x4a7010/0x4a7110/0x4a7210 (font; text, 8 layout values): the layout (CFont
//   +0x48..+0x6c, font_layout.cpp) is redone by LayoutTextBox 0x4a6190 unless the text is the same pointer
//   as +0x48 with the same characters as its copy at +0x4c and all eight values are the same; eax: +0x70
//   (the height), +0x78 (the widest line's right edge), +0x7c (the last line's).
// - The font list (vtable 0x5f4560, base 0x5f4604): slot 0 the definition list's destructor (two copies,
//   0x4a7b20 freeing the list's 0x144 bytes and 0x4a7c30 its base's 0x140); slot 37 makes a CFont (0x84 bytes) and loads it as a sequence with the list's
//   +0x140 (CSequence::LoadCacheBool); slot 35 (0x4a7ce0) the path of a loaded font.
// - Font_SliceStrip 0x4a5e90 (font; image, filters): the glyphs of a font image: one cell per character from
//   first to last, each image width / count wide. A strip without flag 0x20 (+0x1c) marks its cells with
//   opaque pixels (alpha 0xff000000 of GetPixel, slot 54) in row 0 and its last row: the baseline (+0x80) is
//   the first opaque pixel of column 0 within the upper half, the cell edges are found along the last row, and
//   the glyph rectangle leaves out the marker row and column; the glyphs are copied (CopyRect, CopyFrom),
//   filtered and added to the sequence with their origin y = -baseline. A cell beyond the image's width gives
//   an empty glyph of the cell size.
// - Font_LoadTga 0x4a62d0 (slot 28 of CFont; dir, name, flags): the sequence's names and filters
//   (0x56b320, SetupFilters), the file "<dir>/<name>.tga" or "<dir>/-<name>/<name>.tga" read into memory,
//   decoded (ImageFile_Read) and sliced; the file's buffer is not freed when decoding fails. al: loaded.
#include "def_list_slots.h"

namespace zax {

namespace {

inline BYTE *bp_(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
template <typename T = DWORD> inline T &f(const void *o, DWORD off)
{
    return *reinterpret_cast<T *>(const_cast<BYTE *>(static_cast<const BYTE *>(o)) + off);
}

// One character's width (the caller has tested c >= 0x14)
DWORD glyph_width(const BYTE *font, BYTE c, DWORD range)
{
    DWORD i = c - f(font, 0x3c);
    if (i > range)
        i = 0;
    return vcall<DWORD>(bp_(reinterpret_cast<const DWORD *>(bp_(f(font, 8)))[i]), 45);
}
// The width of up to n characters from p, stopping at the NUL (the loop of every measuring function)
DWORD measure(const BYTE *font, const BYTE *p, DWORD n)
{
    DWORD w = 0;
    if (!*p || !f(font, 8))
        return 0;
    const DWORD range = f(font, 0x40) - f(font, 0x3c);
    while (n) {
        if (*p >= 0x14)
            w += glyph_width(font, *p, range);
        const BYTE c = p[1];
        p++;
        n--;
        if (!c)
            break;
    }
    return w;
}

DWORD THISCALL Font_TextWidthN(const BYTE *font, const BYTE *text, DWORD n) { return measure(font, text, n); }

DWORD THISCALL Font_MaxLineWidth(const BYTE *font, const BYTE *text)
{
    using Strchr = const BYTE *(__cdecl *)(const BYTE *, int);
    DWORD best = 0;
    const BYTE *s = text;
    if (!*s)
        return 0;
    do {
        const BYTE *e = fn<Strchr>(0x5d08c0)(s, '\n');
        if (!e)
            e = fn<Strchr>(0x5d08c0)(s, 0);
        if (*s && f(font, 8)) {
            const DWORD w = measure(font, s, (DWORD)(e - s));
            if (w > best)
                best = w;
        }
        s = e;
        if (*e == '\n')
            s = e + 1;
    } while (*s);
    return best;
}

using BreakCallback = void(__fastcall *)(DWORD ctx, const BYTE *word, DWORD n);

DWORD THISCALL Font_BreakLine(const BYTE *font, const BYTE *text, DWORD width, DWORD limit, DWORD ctx, BreakCallback cb, BYTE force)
{
    if (!limit) {
        DWORD n = 0;
        while (text[n])
            n++;
        limit = n + 1;
    }
    DWORD word_width = 0, total = 0, len = 0, count = 0;
    bool newline = false;
    const BYTE *p = text;
    if (*text) {
        for (;;) {
            const DWORD sum = word_width + total;
            if (sum >= width)
                break;
            const DWORD taken = len + count;
            if (taken >= limit)
                break;
            if (cb)
                cb(ctx, p, len);
            p += len;
            count = taken;
            total = sum;
            if (newline)
                break;
            const BYTE c = *p;
            len = 0;
            if (c != ' ')
                for (BYTE a = c; a != '\n' && a;) {
                    a = p[len + 1];
                    len++;
                    if (a == ' ')
                        break;
                }
            const BYTE e = p[len];
            if (e == '\n')
                newline = true;
            if (e)
                len++;
            word_width = c ? measure(font, p, len) : 0;
            if (!*p)
                break;
        }
        if (count || newline)
            return count;
    }
    if (!force)
        return 0;
    // Forced: single characters
    DWORD cw = measure(font, text, 1);
    if (*text) {
        DWORD sum = 0;
        const BYTE *s = text;
        for (;;) {
            cw += sum;
            if (cw >= width)
                break;
            if ((DWORD)(s - text) + 1 >= limit)
                break;
            sum = cw;
            count++;
            s++;
            cw = measure(font, s, 1);
            if (!*s)
                break;
        }
    }
    if (cb)
        cb(ctx, text, count);
    return count;
}

inline void zs_ctor_(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
inline void zs_dtor_(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }

void *THISCALL Font_WrapText(const BYTE *font, void *out, const BYTE *text, DWORD width)
{
    DWORD line;
    zs_ctor_(&line);
    DWORD n = 0;
    while (text[n])
        n++;
    fn<void(THISCALL *)(void *, DWORD)>(0x4e0e60)(&line, n);
    using Append = void(THISCALL *)(void *, const BYTE *, DWORD);
    DWORD k = Font_BreakLine(font, text, width, 0, 0, nullptr, 1);
    while (k) {
        DWORD m = k;
        while (m && (text[m - 1] == ' ' || text[m - 1] == '\n'))
            m--;
        fn<Append>(0x4df440)(&line, text, m);
        const BYTE c = text[k];
        text += k;
        if (c)
            fn<Append>(0x4df440)(&line, glob<const BYTE *>(0x60a684), 1);
        k = Font_BreakLine(font, text, width, 0, 0, nullptr, 1);
    }
    fn<void(THISCALL *)(void *, void *)>(0x4dedf0)(out, &line);
    zs_dtor_(&line);
    return out;
}

// The text box layout, redone unless nothing changed
void layout_box(BYTE *font, const BYTE *text, const DWORD *v)
{
    const BYTE *old = bp_(f(font, 0x48));
    if (text && old && text == old) {
        const BYTE *a = text, *b = bp_(f(font, 0x4c));
        bool same = true;
        for (;;) {
            if (*a != *b) {
                same = false;
                break;
            }
            if (!*a)
                break;
            a++;
            b++;
        }
        if (same) {
            bool all = true;
            for (int i = 0; i < 8; i++)
                all = all && v[i] == f(font, 0x50 + 4 * i);
            if (all)
                return;
        }
    }
    f(font, 0x50) = v[0];
    f(font, 0x58) = v[2];
    f(font, 0x54) = v[1];
    f(font, 0x60) = v[4];
    f(font, 0x5c) = v[3];
    f(font, 0x68) = v[6];
    f(font, 0x48) = (DWORD)(uintptr_t)text;
    f(font, 0x64) = v[5];
    f(font, 0x6c) = v[7];
    fn<void(THISCALL *)(void *)>(0x4a6190)(font);
}
DWORD THISCALL Font_BoxHeight(BYTE *font, const BYTE *text, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD g, DWORD h, DWORD i)
{
    const DWORD v[8] = {a, b, c, d, e, g, h, i};
    layout_box(font, text, v);
    return f(font, 0x70);
}
DWORD THISCALL Font_BoxWidest(BYTE *font, const BYTE *text, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD g, DWORD h, DWORD i)
{
    const DWORD v[8] = {a, b, c, d, e, g, h, i};
    layout_box(font, text, v);
    return f(font, 0x78);
}
DWORD THISCALL Font_BoxLastRight(BYTE *font, const BYTE *text, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD g, DWORD h, DWORD i)
{
    const DWORD v[8] = {a, b, c, d, e, g, h, i};
    layout_box(font, text, v);
    return f(font, 0x7c);
}

// ---- The font list and the texture list --------------------------------------------------------------------

using deflist::Slots;
CClassInfo *__cdecl font_class() { return fn<CClassInfo *(__cdecl *)()>(0x4a5910)(); }

struct FontList : Slots {
    // Slot 0: the definition list's destructor; the font list is 0x144 bytes (its base list's copy 0x4a7c30
    // frees 0x140)
    ZDefinitionList *THISCALL DeletingDtor(DWORD flags)
    {
        deflist::destroy_list(this, 0x5f4604);
        if (flags & 1)
            pool_free(this, 0x144);
        return this;
    }
    // Slot 37: a new CFont loaded as a sequence of the list's cache flag (+0x140)
    void *THISCALL CreateFont(DWORD dir, DWORD name, void **out)
    {
        BYTE *mem = static_cast<BYTE *>(pool_alloc(0x84));
        void *font = mem ? fn<void *(THISCALL *)(void *)>(0x4a5e50)(mem) : nullptr;
        if (out)
            *out = font;
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x56b950)(font, dir, name, f(this, 0x140));
        return font;
    }
};

// ---- Loading a font ----------------------------------------------------------------------------------------

inline void frame_ctor(void *m) { fn<void *(THISCALL *)(void *)>(0x4a8a90)(m); }
inline DWORD frame_dtor(void *m) { return fn<DWORD(THISCALL *)(void *)>(0x4a8b00)(m); }
inline DWORD pixel(BYTE *img, int x, int y) { return vcall<DWORD>(img, 54, x, y); }
BYTE *new_frame()
{
    BYTE *m = static_cast<BYTE *>(pool_alloc(0x40));
    if (!m)
        return nullptr;
    return fn<BYTE *(THISCALL *)(void *)>(0x4a8a90)(m);
}

DWORD THISCALL Font_SliceStrip(BYTE *font, BYTE *img, DWORD filters)
{
    alignas(4) BYTE local[0x40];
    frame_ctor(local);
    const int last_row = (int)vcall<DWORD>(img, 39) - 1;
    DWORD h = vcall<DWORD>(img, 39);
    const DWORD cell = vcall<DWORD>(img, 45) / (f(font, 0x40) - f(font, 0x3c) + 1);
    int baseline = 0;
    int r[4];
    const bool fixed = (f(font, 0x1c) & 0x20) != 0;
    if (fixed) {
        r[0] = 0, r[1] = 0, r[2] = 0, r[3] = (int)h;
        f(font, 0x80) = vcall<DWORD>(img, 39);
    } else {
        h--;
        r[0] = 1, r[1] = 0, r[2] = 1, r[3] = (int)h;
        f(font, 0x80) = 0;
        if (!(pixel(img, 0, 0) & 0xff000000))
            for (;;) {
                if (baseline >= ((int)vcall<DWORD>(img, 39) - 1) / 2)
                    break;
                baseline++;
                if (pixel(img, 0, baseline) & 0xff000000)
                    break;
            }
        f(font, 0x80) = (DWORD)baseline;
        if (!(pixel(img, 0, baseline) & 0xff000000))
            baseline = 0;
        else
            f(font, 0x80) += 1;
        if (!(pixel(img, 0, (int)f(font, 0x80)) & 0xff000000))
            for (;;) {
                const DWORD v = f(font, 0x80);
                if (v >= vcall<DWORD>(img, 39) - 1)
                    break;
                f(font, 0x80) = v + 1;
                if (pixel(img, 0, (int)(v + 1)) & 0xff000000)
                    break;
            }
    }
    for (DWORD c = f(font, 0x3c); c <= f(font, 0x40); c++) {
        BYTE *g;
        if (r[0] >= (int)vcall<DWORD>(img, 45) - 1) {
            g = new_frame();
            const DWORD bits = vcall<DWORD>(img, 29);
            fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(g, cell, h, bits);
        } else {
            if (fixed) {
                r[2] += (int)cell;
            } else if (!(pixel(img, r[2], last_row) & 0xff000000)) {
                for (;;) {
                    if (r[2] >= (int)vcall<DWORD>(img, 45))
                        break;
                    r[2]++;
                    if (pixel(img, r[2], last_row) & 0xff000000)
                        break;
                }
            }
            fn<void(THISCALL *)(void *, void *, int *)>(0x4ab620)(local, img, r);
            g = new_frame();
            fn<void(THISCALL *)(void *, void *)>(0x4ab5d0)(g, local);
        }
        fn<void(THISCALL *)(DWORD, void *)>(0x4b21a0)(filters, g);
        fn<void(THISCALL *)(void *, void *, DWORD, DWORD)>(0x56bb70)(font, g, 0, 0);
        f<WORD>(g, 0xa) = (WORD)-baseline;
        r[0] = r[2];
        if (!fixed)
            r[2] = r[0] + 1;
    }
    return (frame_dtor(local) & ~0xffu) | 1;
}

BYTE THISCALL Font_LoadTga(BYTE *font, const char *dir, const char *name, DWORD flags)
{
    alignas(4) BYTE a[0x40], b[0x40];
    frame_ctor(a);
    frame_ctor(b);
    fn<void(THISCALL *)(void *, const char *, const char *)>(0x56b320)(font, dir, name);
    const DWORD filters = fn<DWORD(THISCALL *)(void *, DWORD)>(0x56b440)(font, flags);
    char path[0x104];
    fn<void(__fastcall *)(char *, const char *, DWORD, const char *, DWORD)>(0x4a4380)(path, dir, glob_addr(0x60f634), name, glob_addr(0x613a28));
    using CanOpen = BYTE(__fastcall *)(const char *, DWORD);
    bool ok = fn<CanOpen>(0x4a1570)(path, 0) != 0;
    if (!ok) {
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(path, glob<const char *>(0x613a18), dir, name, name);
        ok = fn<CanOpen>(0x4a1570)(path, 0) != 0;
    }
    BYTE result = 0;
    if (ok) {
        alignas(4) BYTE fb[0x1c];
        fn<void *(THISCALL *)(void *)>(0x4a1780)(fb);
        if (fn<BYTE(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(fb, path, 4)) {
            const DWORD size = fn<DWORD(THISCALL *)(void *)>(0x4a2190)(fb);
            BYTE *data = static_cast<BYTE *>(heap_alloc(size));
            fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, data, size);
            fn<void(THISCALL *)(void *)>(0x4a2100)(fb);
            alignas(4) BYTE image[0x40];
            frame_ctor(image);
            if (fn<DWORD(__fastcall *)(void *, BYTE *, const char *, DWORD, DWORD)>(0x58c6f0)(image, data, name, 0, 0x20)) {
                Font_SliceStrip(font, image, filters);
                if (data)
                    heap_free(data);
                result = 1;
            }
            frame_dtor(image);
        }
        fn<void(THISCALL *)(void *)>(0x4a1800)(fb);
    }
    frame_dtor(b);
    frame_dtor(a);
    return result;
}

}  // namespace

ZAX_HOOKS(text_rest4a)
{
    const Hook list[] = {
#include "text_rest4a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
