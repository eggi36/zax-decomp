// The palette object and its tables (phase 4, rendering): 0x5266d0-0x527fa0, the code InitBlendTables
// 0x433000, the frames' palettes (0x4a9b60, CFrame_SetPalette, CFrame_CopyRect), the toolkit's bevel
// colours (CJWindow::Init) and FieldColor_PaletteIndex use.
//
// The palette object [confirmed: code; VM] (12 bytes): +0 the data (operator new), +4 the kind, +8 the
// source palette of kind 6. The kind gives the data's size (0x527d50, inline everywhere else): 1 a palette
// of 256 entries b, g, r (0x300 bytes), 2 a remap of 256 indices (0x100), 3 0x400 bytes, 4 a remap of
// 256 x 256 indices (0x10000; the blend tables: entry src * 256 + dst), 5 and 6 an index per 5-5-5 colour
// (0x8000; 6 filled on demand from the source palette), 7 an index per 5-6-5 colour (0x10000); any other
// kind 0 bytes. Every function that gives the object a kind frees the data first when the kind changes
// (0x526700, 0x5268f0, 0x5269e0, 0x527da0 also for kind 0) and, when there is no data, allocates it and
// fills it with the identity (byte i = i).
//
// Nearest 0x526e40 [confirmed: code; VM] (r, g, b, first, end): kind 1 searches entries first..end - 1
// for the smallest 30 dr^2 + 59 dg^2 + 11 db^2 (the square table 0x61d3e8, indexed by the absolute
// difference; unsigned compares; a distance of 0 stops) and returns (first + the entry's offset) & 0xff
// (first + first when the range is empty); kind 5 the byte for Pixel15_FromRGB 0x4339a0, kind 6 the same,
// filled from the source palette's Nearest(r, g, b, 1, 0xff) when 0, kind 7 the byte for Pixel16_FromRGB
// 0x433a10; the other kinds 0.
//
// The tables [confirmed: code; VM]:
//  - RgbToHsl 0x526c40 (fastcall h out, s out; r, g, b in 0..1, l out): the usual HSL (h in degrees,
//    0..360), with the x87 order of the original: l = (max + min) / 2 from the unrounded sum, s = d / sum
//    or d / ((2 - max) - min), h = 60 * ((g - b), (b - r) + 2 or (r - g) + 4) / d; max == min gives h = s = 0.
//    HueValue 0x526da0 (p, q, h; st0): h brought into 0..360 by one step, then p + (q - p) * h / 60, q,
//    p + (q - p) * (240 - h) / 60 or p.
//  - BuildLightnessRemap 0x527040 (src, factor, two unused, first, end, lo, hi, target): a remap of 256
//    indices: below first and from end on the identity, factor 0x80 the identity, otherwise every source
//    entry's lightness times factor / 128 (saturation and hue kept) back to RGB and the nearest index of
//    the target palette (the source when null) in lo..hi - 1. The red channel's hue value is computed inline
//    with the unrounded hue + 120, green and blue through 0x526da0.
//  - FillGammaTable 0x527420 (fastcall table, palette; factor) and BuildGammaTable 0x527700 (palette,
//    factor; kind 4): for every source entry s a row of 256: the nearest index (1..255) of
//    s * k + d * (1 - k) for every entry d, k the factor; with factor 0 (or NaN) k = (max(s) / 255)^1.5
//    (_CIpow 0x5d23e0) and s the entry scaled to a maximum of 255 (none 0; black (1 for the last channel));
//    entry 0 counts as white.
//  - BuildAmplifyTable 0x5277b0 (palette; kind 4): d * (1 + s / 255) per channel, at most 255, the nearest
//    index (1..255).
//  - BuildHueBlendTable 0x527980 (palette; kind 4): the hue of d moved towards s's by s's saturation times
//    min(4 l(s), 1) (the shorter way round, 342.128 for a turn), the larger saturation, l(d) + l(s) / 3,
//    back to RGB (truncated) and the nearest index.
//  - BuildLookup 0x527da0 (src, kind, lo, hi): the source kept at +8; kind 5 every 5-5-5 colour's nearest
//    index (r, g, b times 8), kind 6 cleared (filled on demand), kind 7 every 5-6-5 colour's (g times 4).
//  - DrawPalette_Build 0x527fa0 (histogram, first, colours): kind 1, cleared, the last entry white, and the
//    colour quantisation 0x54ff70 of the 64 x 64 x 64 histogram into the entries from first (its count
//    output a local that starts as the palette object's address); eax the quantisation's.
//  - Load 0x5269e0 (path, kind) and Save 0x526b80 (path) read and write the data whole through a CFileBuf
//    on the stack (Open flags 4 and 3); 1, or 0 when the file does not open.
//
// Quirks kept [confirmed: code]: Nearest of an empty range returns first * 2; FillGammaTable rescales by the
// first, unrounded maximum and then by every channel that is still above 255 times it; BuildLightnessRemap
// ignores its third and fourth arguments; the lookup's counters reuse the argument slots (no effect).
//
// Return values: Init returns the object, Differs and Load/Save their values, Nearest the index; the others are void for
// their callers (eax not reproduced).
#include "exe_base.h"
#include "file_buffer.h"
#include "x87.h"

namespace zax {

namespace {

using LD = long double;

struct PalObj {
    BYTE *data;
    DWORD kind;
    PalObj *source;
};

DWORD kind_bytes(DWORD kind)
{
    switch (kind) {
    case 1: return 0x300;
    case 2: return 0x100;
    case 3: return 0x400;
    case 4:
    case 7: return 0x10000;
    case 5:
    case 6: return 0x8000;
    default: return 0;
    }
}

// The kind set, the data freed when it changes (and, with zero_frees, for kind 0), allocated and filled with
// the identity when missing
void prepare(PalObj *p, DWORD kind, bool zero_frees, DWORD new_bytes)
{
    if ((p->kind != kind || (zero_frees && kind == 0)) && p->data) {
        heap_free(p->data);
        p->data = nullptr;
    }
    p->kind = kind;
    if (!p->data) {
        p->data = static_cast<BYTE *>(heap_alloc(new_bytes));
        for (DWORD i = 0; i < kind_bytes(p->kind); i++)
            p->data[i] = (BYTE)i;
    }
}

DWORD nearest(PalObj *p, DWORD r, DWORD g, DWORD b, DWORD first, DWORD end)
{
    return zax::fn<DWORD(THISCALL *)(PalObj *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526e40)(p, r, g, b, first, end);
}
void rgb_to_hsl(float *h, float *s, float r, float g, float b, float *l)
{
    zax::fn<void(__fastcall *)(float *, float *, float, float, float, float *)>(0x526c40)(h, s, r, g, b, l);
}
LD hue_value(float p, float q, float h) { return zax::fn<LD(__stdcall *)(float, float, float)>(0x526da0)(p, q, h); }
DWORD pixel15(DWORD r, DWORD g, DWORD b) { return zax::fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x4339a0)(r, g, b); }
DWORD pixel16(DWORD r, DWORD g, DWORD b) { return zax::fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(r, g, b); }
int ftol(LD v) { return x87_ftol(v); }
// _CIpow 0x5d23e0: st(1) to the power of st(0)
LD ci_pow(LD x, LD y)
{
    const DWORD fn = fn_addr(0x5d23e0);
    LD r;
    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "m"(fn) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}
LD byte_unit(BYTE v) { return fild(v) * cst(glob_addr(0x5f172c)); }   // v / 255
int clamp_byte(int v) { return v < 0 ? 0 : v > 0xff ? 0xff : v; }

}  // namespace

// 0x5266d0
PalObj *__fastcall Palette_Init(PalObj *p)
{
    p->data = nullptr;
    p->kind = 0;
    p->source = nullptr;
    return p;
}

// 0x526700
void THISCALL Palette_Assign(PalObj *self, const PalObj *src)
{
    if (!src->data || src == self)
        return;
    prepare(self, src->kind, true, kind_bytes(src->kind));
    memcpy(self->data, src->data, kind_bytes(src->kind));
}

// 0x526870
DWORD THISCALL Palette_Differs(const PalObj *self, const PalObj *other)
{
    if (self->kind != other->kind)
        return 1;
    DWORD n = kind_bytes(self->kind);
    const BYTE *a = self->data, *b = other->data;
    bool equal = true;   // xor eax, eax: ZF set when nothing is compared
    while (n) {
        n--;
        equal = *a++ == *b++;
        if (!equal)
            break;
    }
    return equal ? 0 : 1;
}

// 0x5268f0
void THISCALL Palette_SetKind(PalObj *self, DWORD kind) { prepare(self, kind, true, kind_bytes(kind)); }

// 0x5269e0
DWORD THISCALL Palette_Load(PalObj *self, const char *path, DWORD kind)
{
    prepare(self, kind, true, kind_bytes(kind));
    CFileBuf f;
    zax::fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&f);
    if (!zax::fn<bool(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(&f, path, 4)) {
        zax::fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
        return 0;
    }
    zax::fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(&f, self->data, kind_bytes(self->kind));
    zax::fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(&f);
    zax::fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
    return 1;
}

// 0x526b80
DWORD THISCALL Palette_Save(PalObj *self, const char *path)
{
    CFileBuf f;
    zax::fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&f);
    if (!zax::fn<bool(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(&f, path, 3)) {
        zax::fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
        return 0;
    }
    zax::fn<DWORD(THISCALL *)(CFileBuf *, const void *, DWORD)>(0x4a19d0)(&f, self->data, kind_bytes(self->kind));
    zax::fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(&f);
    zax::fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
    return 1;
}

// 0x526c40
void __fastcall Color_RgbToHsl(float *h, float *s, float r, float g, float b, float *l)
{
    float mx = c03(r, g) ? g : r;
    if (c03(mx, b))
        mx = b;
    float mn = c0(r, g) ? r : g;
    if (!c0(mn, b))
        mn = b;
    const LD sum = (LD)mn + mx;
    const float sumf = fstp(sum);
    const LD lx = sum * cst(glob_addr(C_HALF));
    *l = fstp(lx);
    if (c3(mx, mn)) {
        *s = 0.0f;
        *h = 0.0f;
        return;
    }
    const LD d = (LD)mx - mn;
    if (c03(lx, cst(glob_addr(C_HALF))))
        *s = fstp(d / sumf);
    else
        *s = fstp(d / (cst(glob_addr(C_TWO)) - mx - mn));
    const float df = fstp(d);
    if (c3(r, mx))
        *h = fstp(((LD)g - b) / df);
    else if (c3(g, mx))
        *h = fstp(((LD)b - r) / df + cst(glob_addr(C_TWO)));
    else if (c3(b, mx))
        *h = fstp(((LD)r - g) / df + cst(glob_addr(0x5eca50)));
    const LD v = (LD)*h * cst(glob_addr(0x5fbc78));
    *h = fstp(v);
    if (c0(v, cst(glob_addr(C_ZERO))))
        *h = fstp(v + cst(glob_addr(0x5ee470)));
}

// 0x526da0
LD __stdcall Color_HueValue(float p, float q, float h)
{
    LD x = h;
    if (!c03(x, cst(glob_addr(0x5ee470))))
        x = x - cst(glob_addr(0x5ee470));
    else if (c0(x, cst(glob_addr(C_ZERO))))
        x = x + cst(glob_addr(0x5ee470));
    if (c0(x, cst(glob_addr(0x5fbc78))))
        return ((LD)q - p) * x * cst(glob_addr(0x5f8834)) + p;
    if (c0(x, cst(glob_addr(0x5fbc7c))))
        return q;
    if (c0(x, cst(glob_addr(0x5eff9c))))
        return ((LD)q - p) * (cst(glob_addr(0x5eff9c)) - x) * cst(glob_addr(0x5f8834)) + p;
    return p;
}

// 0x526e40
DWORD THISCALL Palette_Nearest(PalObj *self, int r, int g, int b, DWORD first, DWORD end)
{
    switch (self->kind) {
    case 1: {
        const DWORD n = end - first;
        DWORD best = first;
        if (n) {
            const int *sq = glob<const int *>(0x61d3e8);
            const BYTE *e = self->data + first * 3;
            DWORD best_d = 0xffffffff;
            for (DWORD i = 0; i < n; i++, e += 3) {
                const int dg = g - e[1], dr = r - e[2], db = b - e[0];
                const DWORD d = (DWORD)sq[dg < 0 ? -dg : dg] * 59 + (DWORD)sq[dr < 0 ? -dr : dr] * 30 + (DWORD)sq[db < 0 ? -db : db] * 11;
                if (d < best_d) {
                    best = i;
                    if (!d)
                        break;
                    best_d = d;
                }
            }
        }
        return (first + best) & 0xff;
    }
    case 5: return self->data[pixel15(r, g, b)];
    case 6: {
        DWORD v = self->data[pixel15(r, g, b)];
        if (!v) {
            v = nearest(self->source, r, g, b, 1, 0xff);
            self->data[pixel15(r, g, b)] = (BYTE)v;
        }
        return v;
    }
    case 7: return self->data[pixel16(r, g, b)];
    default: return 0;
    }
}

// 0x526fd0
void THISCALL Palette_SetEntry(PalObj *self, DWORD i, BYTE r, BYTE g, BYTE b)
{
    self->data[i * 3 + 2] = r;
    self->data[i * 3 + 1] = g;
    self->data[i * 3] = b;
}

// 0x527000
void THISCALL Palette_GetEntry(const PalObj *self, DWORD i, DWORD *r, DWORD *g, DWORD *b)
{
    *r = self->data[i * 3 + 2];
    *g = self->data[i * 3 + 1];
    *b = self->data[i * 3];
}

// 0x527040
void THISCALL Palette_BuildLightnessRemap(PalObj *self, PalObj *src, DWORD factor, DWORD, DWORD, DWORD first, DWORD end, DWORD lo, DWORD hi,
                                          PalObj *target)
{
    prepare(self, 2, false, 0x100);
    for (DWORD i = 0; i < first; i++)
        self->data[i] = (BYTE)i;
    const BYTE *e = src->data + first * 3;
    DWORD i = end;
    if (first < end) {
        for (DWORD n = first; n < end; n++) {
            if (factor == 0x80) {
                self->data[n] = (BYTE)n;
                continue;
            }
            float h, s, l;
            rgb_to_hsl(&h, &s, fstp(byte_unit(e[2])), fstp(byte_unit(e[1])), fstp(byte_unit(e[0])), &l);
            const float l2 = fstp(fild64(factor) * l * cst(glob_addr(0x5ee004)));
            const LD qx = c03(l2, cst(glob_addr(C_HALF))) ? ((LD)s + cst(glob_addr(C_ONE))) * l2 : (LD)s + l2 - (LD)s * l2;
            const float q = fstp(qx);
            const float p = fstp((LD)l2 + l2 - q);
            float red, green;
            LD blue;
            if (c3(s, cst(glob_addr(C_ZERO)))) {
                red = l2;
                green = l2;
                blue = l2;
            } else {
                LD x = (LD)h + cst(glob_addr(0x5fbc80));
                if (!c03(x, cst(glob_addr(0x5ee470))))
                    x = x - cst(glob_addr(0x5ee470));
                else if (c0(x, cst(glob_addr(C_ZERO))))
                    x = x + cst(glob_addr(0x5ee470));
                LD rv;
                if (c0(x, cst(glob_addr(0x5fbc78))))
                    rv = ((LD)q - p) * x * cst(glob_addr(0x5f8834)) + p;
                else if (c0(x, cst(glob_addr(0x5fbc7c))))
                    rv = q;
                else if (c0(x, cst(glob_addr(0x5eff9c))))
                    rv = ((LD)q - p) * (cst(glob_addr(0x5eff9c)) - x) * cst(glob_addr(0x5f8834)) + p;
                else
                    rv = p;
                red = fstp(rv);
                green = fstp(hue_value(p, q, h));
                blue = hue_value(p, q, fstp((LD)h - cst(glob_addr(0x5fbc80))));
            }
            if (c0(red, cst(glob_addr(C_ZERO))))
                red = 0.0f;
            if (c0(green, cst(glob_addr(C_ZERO))))
                green = 0.0f;
            if (c0(blue, cst(glob_addr(C_ZERO))))
                blue = cst(glob_addr(C_ZERO));
            if (!c03(red, cst(glob_addr(C_ONE))))
                red = 1.0f;
            if (!c03(green, cst(glob_addr(C_ONE))))
                green = 1.0f;
            if (!c03(blue, cst(glob_addr(C_ONE))))
                blue = cst(glob_addr(C_ONE));
            const int ri = ftol((LD)red * cst(glob_addr(0x5eb728)) + cst(glob_addr(C_HALF)));
            const int gi = ftol((LD)green * cst(glob_addr(0x5eb728)) + cst(glob_addr(C_HALF)));
            const int bi = ftol(blue * cst(glob_addr(0x5eb728)) + cst(glob_addr(C_HALF)));
            e += 3;
            self->data[n] = (BYTE)nearest(target ? target : src, ri, gi, bi, lo, hi);
        }
        i = end;
    }
    for (; i < 0x100; i++)
        self->data[i] = (BYTE)i;
}

// 0x527420
void __fastcall Palette_FillGammaTable(BYTE *out, PalObj *src, float factor)
{
    const BYTE *e = src->data;
    for (int k = 0x100; k; k--, e += 3) {
        BYTE c2 = e[2], c1 = e[1], c0b = e[0];
        LD f;
        if (c3(factor, cst(glob_addr(C_ZERO)))) {
            BYTE m = c2 > c1 ? c2 : c1;
            if (!(m > c0b))
                m = c0b;
            LD mx = fild(m) * cst(glob_addr(0x5f172c));
            const float mxf = fstp(mx);
            const float f2 = fstp(fild(c2)), f1 = fstp(fild(c1)), f0 = fstp(fild(c0b));
            if (!c03((LD)f2 / mxf, cst(glob_addr(0x5eb728))))
                mx = (LD)f2 * cst(glob_addr(0x5f172c));
            if (!c03((LD)f1 / mx, cst(glob_addr(0x5eb728))))
                mx = (LD)f1 * cst(glob_addr(0x5f172c));
            if (!c03((LD)f0 / mx, cst(glob_addr(0x5eb728))))
                mx = (LD)f0 * cst(glob_addr(0x5f172c));
            DWORD v2, v1, v0;
            if (c03(mx, cst(glob_addr(C_ZERO)))) {
                v2 = v1 = v0 = 0;
            } else {
                const DWORD a = (DWORD)ftol((LD)f2 / mx + cst(glob_addr(C_HALF)));
                v2 = a < 0xff ? a : 0xff;
                const DWORD b = (DWORD)ftol(fild64(c1) / mx + cst(glob_addr(C_HALF)));
                v1 = b < 0xff ? b : 0xff;
                const DWORD c = (DWORD)ftol(fild64(c0b) / mx + cst(glob_addr(C_HALF)));
                v0 = c < 0xff ? c : 0xff;
                if (!v2 && !v1 && !v0)
                    v0 = 1;
            }
            c2 = (BYTE)v2;
            c1 = (BYTE)v1;
            c0b = (BYTE)v0;
            f = ci_pow(fld(mxf), (LD)*glob<const double *>(0x5fbc88));
        } else {
            f = factor;
        }
        const BYTE *base = src->data;
        if (e == base)
            c0b = c1 = c2 = 0xff;
        const float a2 = fstp(fild(c2) * f);
        const float a1 = fstp(fild(c1) * f);
        const double d = (double)((LD)*glob<const double *>(0x5ee678) - f);
        const float a0 = fstp(fild(c0b) * f);
        const BYTE *q = base;
        for (int j = 0x100; j; j--, q += 3) {
            const int r = ftol(fild(q[2]) * (LD)d + a2);
            const int g = ftol(fild(q[1]) * (LD)d + a1);
            const int b = ftol(fild(q[0]) * (LD)d + a0);
            *out++ = (BYTE)nearest(src, r, g, b, 1, 0xff);
        }
    }
}

// 0x527700
void THISCALL Palette_BuildGammaTable(PalObj *self, PalObj *src, float factor)
{
    prepare(self, 4, false, 0x10000);
    zax::fn<void(__fastcall *)(BYTE *, PalObj *, float)>(0x527420)(self->data, src, factor);
}

// 0x5277b0
void THISCALL Palette_BuildAmplifyTable(PalObj *self, PalObj *src)
{
    prepare(self, 4, false, 0x10000);
    BYTE *out = self->data;
    const BYTE *s = src->data;
    for (int i = 0x100; i; i--, s += 3) {
        const BYTE *d = src->data;
        for (int j = 0x100; j; j--, d += 3) {
            int v0 = ftol((byte_unit(s[0]) + cst(glob_addr(C_ONE))) * fild(d[0]));
            v0 = v0 > 0xff ? 0xff : v0;
            int v1 = ftol((byte_unit(s[1]) + cst(glob_addr(C_ONE))) * fild(d[1]));
            v1 = v1 > 0xff ? 0xff : v1;
            int v2 = ftol((byte_unit(s[2]) + cst(glob_addr(C_ONE))) * fild(d[2]));
            v2 = v2 > 0xff ? 0xff : v2;
            *out++ = (BYTE)nearest(src, v2, v1, v0, 1, 0xff);
        }
    }
}

// 0x527980
void THISCALL Palette_BuildHueBlendTable(PalObj *self, PalObj *src)
{
    prepare(self, 4, false, kind_bytes(4));
    BYTE *out = self->data;
    const BYTE *s = src->data;
    for (int i = 0x100; i; i--, s += 3) {
        float h1, s1, l1;
        rgb_to_hsl(&h1, &s1, fstp(byte_unit(s[2])), fstp(byte_unit(s[1])), fstp(byte_unit(s[0])), &l1);
        LD t = (LD)l1 * cst(glob_addr(0x5eca50));
        if (c0(t, cst(glob_addr(C_ZERO))))
            t = cst(glob_addr(C_ZERO));
        else if (!c03(t, cst(glob_addr(C_ONE))))
            t = cst(glob_addr(C_ONE));
        const float w = fstp(t * s1);
        const float v = fstp(cst(glob_addr(C_ONE)) - w);
        const BYTE *d = src->data;
        for (int j = 0x100; j; j--, d += 3) {
            float h2, s2, l2;
            rgb_to_hsl(&h2, &s2, fstp(byte_unit(d[2])), fstp(byte_unit(d[1])), fstp(byte_unit(d[0])), &l2);
            float h2a = h2;
            if (!c03((LD)h1 - h2, cst(glob_addr(0x5fbc94))))
                h2a = fstp((LD)h2 + cst(glob_addr(0x5fbc90)));
            LD a = h1;
            if (!c03((LD)h2a - h1, cst(glob_addr(0x5fbc94))))
                a = (LD)h1 + cst(glob_addr(0x5fbc90));
            const LD hm = (LD)v * h2a + a * w;
            float hmf = fstp(hm);
            if (!c03(hm, cst(glob_addr(0x5fbc90))))
                hmf = fstp((LD)hmf - cst(glob_addr(0x5fbc90)));
            const float smax = !c03(s2, s1) ? s2 : s1;
            const LD lm = (LD)l1 * cst(glob_addr(0x5f88c8)) + l2;
            const float lmf = fstp(lm);
            LD lv, qx;
            if (c0(lm, cst(glob_addr(C_ZERO)))) {
                lv = cst(glob_addr(C_ZERO));
                qx = ((LD)smax + cst(glob_addr(C_ONE))) * lv;
            } else if (!c03(lmf, cst(glob_addr(C_ONE)))) {
                lv = cst(glob_addr(C_ONE));
                qx = lv + smax - lv * smax;
            } else {
                lv = lmf;
                qx = !c03(lmf, cst(glob_addr(C_HALF))) ? lv + smax - lv * smax : ((LD)smax + cst(glob_addr(C_ONE))) * lv;
            }
            const float q = fstp(qx);
            const float p = fstp(lv + lv - q);
            float blue, green;
            LD red;
            if (c3(smax, cst(glob_addr(C_ZERO)))) {
                blue = fstp(lv);
                green = blue;
                red = lv;
            } else {
                const float c1 = fstp(hue_value(p, q, fstp((LD)hmf + cst(glob_addr(0x5fbc80)))));
                green = fstp(hue_value(p, q, hmf));
                blue = fstp(hue_value(p, q, fstp((LD)hmf - cst(glob_addr(0x5fbc80)))));
                red = c1;
            }
            const int bi = clamp_byte(ftol((LD)blue * cst(glob_addr(0x5eb728))));
            const int gi = clamp_byte(ftol((LD)green * cst(glob_addr(0x5eb728))));
            const int ri = clamp_byte(ftol(red * cst(glob_addr(0x5eb728))));
            *out++ = (BYTE)nearest(src, ri, gi, bi, 1, 0xff);
        }
    }
}

// 0x527d50
DWORD __fastcall Palette_KindBytes(DWORD kind) { return kind_bytes(kind); }

// 0x527da0
void THISCALL Palette_BuildLookup(PalObj *self, PalObj *src, DWORD kind, DWORD lo, DWORD hi)
{
    prepare(self, kind, true, kind_bytes(kind));
    self->source = src;
    DWORD n = 0;
    if (kind == 5) {
        for (DWORD r = 0; r < 0x20; r++)
            for (DWORD g = 0; g < 0x20; g++)
                for (DWORD b = 0; b < 0x20; b++) {
                    const BYTE v = (BYTE)nearest(src, r << 3, g * 8, b * 8, lo, hi);
                    self->data[n++] = v;
                }
    } else if (kind == 6) {
        memset(self->data, 0, 0x8000);
    } else if (kind == 7) {
        for (DWORD r = 0; r < 0x20; r++)
            for (DWORD g = 0; g < 0x40; g++)
                for (DWORD b = 0; b < 0x20; b++) {
                    const BYTE v = (BYTE)nearest(src, r * 8, g * 4, b * 8, lo, hi);
                    self->data[n++] = v;
                }
    }
}

// 0x527fa0
DWORD THISCALL DrawPalette_Build(PalObj *self, void *histogram, DWORD first, DWORD colours)
{
    PalObj *count = self;   // the count output, a local that starts as the object's address
    prepare(self, 1, false, 0x300);
    memset(self->data, 0, kind_bytes(self->kind));
    self->data[0x2ff] = 0xff;
    self->data[0x2fe] = 0xff;
    self->data[0x2fd] = 0xff;
    return zax::fn<DWORD(__fastcall *)(void *, DWORD, BYTE *, PalObj **)>(0x54ff70)(histogram, colours, first * 3 + self->data, &count);
}

// ---- The colour quantisation 0x54ff70-0x550660 ----------------------------------------------------------------
//
// [confirmed: code; VM] A variance-minimising box cut (Wu's quantiser) over a histogram of 64 x 64 x 64 dwords
// (index (r * 64 + g) * 64 + b; 0x712258 while it runs). A box (0x30 bytes, the array 0x712260): +0 its
// variance, +4 the count, +8 the sum of squares, +0xc/+0x10/+0x14 the sums of r, g and b, +0x18.. r0, g0, b0,
// r1, g1, b1 (inclusive). 0x712264 is a max-heap (from index 1) of box pointers by variance, 0x71225c its
// size, 0x712268 the number of boxes.
//  - BoxBottom 0x5504f0 (fastcall r0, g0; b0, r1, g1, b1, count, squares, r, g, b out): the moments of the
//    box's cells (32-bit sums; the squares r^2 + g^2 + b^2 times the cell).
//  - BoxVolume 0x5501f0 (fastcall r0, g0; b0, r1, g1, b1, six outputs): the bounds of the cells that are
//    not 0, each searched over the whole input box; an output is written only when a cell is found.
//  - Cut 0x550660 (fastcall box): a new box from the array; for each axis every split plane s in [lo, hi)
//    rates the two halves by (squares - (r^2 + g^2 + b^2) / count) truncated (x87, the sums as unsigned
//    64-bit values) and summed as an unsigned dword; the smallest of the three axes' best (r before g
//    before b on ties) splits the box into [lo, s] and [s + 1, hi]; both halves shrunk (BoxVolume), the
//    first's moments recomputed and the second's the rest, their variances, and the heap updated: a half
//    whose extents sum to 0 (one cell) leaves it (the top replaced by the second half or the last entry;
//    sifted down), a second non-empty half is inserted (sifted up).
//  - Quantize 0x54ff70 (fastcall histogram, colours; palette out, count out): the heap (colours * 4 + 4) and
//    the boxes (colours * 0x30) from operator new (1 when either fails), the first box the histogram's
//    bounds (BoxVolume of 0..63) with its moments and variance 1 in the heap unless it is a single cell,
//    then Cut of the top until there are as many boxes as colours, the heap is empty or its top null; each
//    box's colour (count / 2 + sum * 4) / count per channel (r, g, b in that order) into the palette, the
//    box count into the count; both blocks freed; eax 0.
// Quirks kept [confirmed: code]: the first box's variance is 1, not computed; an empty histogram leaves the
// bounds undefined (the outputs are not written).

namespace {

constexpr DWORD Q_HIST = 0x712258, Q_HEAP_SIZE = 0x71225c, Q_BOXES = 0x712260, Q_HEAP = 0x712264, Q_NBOXES = 0x712268;
DWORD &qg(DWORD a) { return *at<DWORD *>(a); }
DWORD *heap_of() { return at<DWORD *>(qg(glob_addr(Q_HEAP))); }
DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }

using BoxBottomFn = void(__fastcall *)(int, int, int, int, int, int, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *);
using BoxVolumeFn = void(__fastcall *)(int, int, int, int, int, int, int *, int *, int *, int *, int *, int *);
void box_bottom(int r0, int g0, int b0, int r1, int g1, int b1, DWORD *c, DWORD *q, DWORD *r, DWORD *g, DWORD *b)
{
    zax::fn<BoxBottomFn>(0x5504f0)(r0, g0, b0, r1, g1, b1, c, q, r, g, b);
}
void box_volume(int r0, int g0, int b0, int r1, int g1, int b1, int *o0, int *o1, int *o2, int *o3, int *o4, int *o5)
{
    zax::fn<BoxVolumeFn>(0x5501f0)(r0, g0, b0, r1, g1, b1, o0, o1, o2, o3, o4, o5);
}
LD ufild(DWORD v) { return fild64((long long)v); }
// squares - (r^2 + g^2 + b^2) / count, truncated
int box_rate(DWORD q, DWORD r, DWORD g, DWORD b, DWORD c)
{
    const LD lr = ufild(r), lg = ufild(g), lb = ufild(b);
    return x87_ftol(ufild(q) - (lr * lr + lg * lg + lb * lb) / ufild(c));
}

// Sift the heap's entry at position 1 down (the key the entry's variance)
void sift_down(DWORD key, DWORD entry)
{
    DWORD *heap = heap_of();
    DWORD pos = 1, child = 2;
    if (qg(glob_addr(Q_HEAP_SIZE)) >= 2) {
        for (;;) {
            if (child < qg(glob_addr(Q_HEAP_SIZE)) && *at<DWORD *>(heap[child]) < *at<DWORD *>(heap[child + 1]))
                child++;
            if (key >= *at<DWORD *>(heap[child]))
                break;
            heap[pos] = heap[child];
            heap = heap_of();
            pos = child;
            child <<= 1;
            if (child > qg(glob_addr(Q_HEAP_SIZE)))
                break;
        }
    }
    heap[pos] = entry;
}

}  // namespace

// 0x5504f0
void __fastcall DrawPalette_BoxBottom(int r0, int g0, int b0, int r1, int g1, int b1, DWORD *count, DWORD *squares, DWORD *sr, DWORD *sg, DWORD *sb)
{
    DWORD n = 0, sq = 0, tr = 0, tg = 0, tb = 0;
    const DWORD *hist = at<const DWORD *>(qg(glob_addr(Q_HIST)));
    const DWORD *plane = hist + ((((DWORD)r0 << 6) + (DWORD)g0) << 6) + (DWORD)b0;
    for (int r = r0; r <= r1; r++, plane += 0x1000) {
        const DWORD rr = (DWORD)r * (DWORD)r;
        const DWORD *row = plane;
        for (int g = g0; g <= g1; g++, row += 0x40) {
            const DWORD gg = (DWORD)g * (DWORD)g + rr;
            const DWORD *cell = row;
            for (int b = b0; b <= b1; b++) {
                const DWORD v = *cell++;
                if (!v)
                    continue;
                n += v;
                tr += v * (DWORD)r;
                tg += v * (DWORD)g;
                tb += v * (DWORD)b;
                sq += ((DWORD)b * (DWORD)b + gg) * v;
            }
        }
    }
    *count = n;
    *squares = sq;
    *sr = tr;
    *sg = tg;
    *sb = tb;
}

// 0x5501f0
void __fastcall DrawPalette_BoxVolume(int r0, int g0, int b0, int r1, int g1, int b1, int *or0, int *og0, int *ob0, int *or1, int *og1, int *ob1)
{
    const DWORD *hist = at<const DWORD *>(qg(glob_addr(Q_HIST)));
    auto cell = [&](int r, int g, int b) { return hist[((((DWORD)r << 6) + (DWORD)g) << 6) + (DWORD)b]; };
    [&] {
        for (int r = r0; r <= r1; r++)
            for (int g = g0; g <= g1; g++)
                for (int b = b0; b <= b1; b++)
                    if (cell(r, g, b)) {
                        *or0 = r;
                        return;
                    }
    }();
    [&] {
        for (int g = g0; g <= g1; g++)
            for (int r = r0; r <= r1; r++)
                for (int b = b0; b <= b1; b++)
                    if (cell(r, g, b)) {
                        *og0 = g;
                        return;
                    }
    }();
    [&] {
        for (int b = b0; b <= b1; b++)
            for (int r = r0; r <= r1; r++)
                for (int g = g0; g <= g1; g++)
                    if (cell(r, g, b)) {
                        *ob0 = b;
                        return;
                    }
    }();
    [&] {
        for (int r = r1; r >= r0; r--)
            for (int g = g1; g >= g0; g--)
                for (int b = b1; b >= b0; b--)
                    if (cell(r, g, b)) {
                        *or1 = r;
                        return;
                    }
    }();
    [&] {
        for (int g = g1; g >= g0; g--)
            for (int r = r1; r >= r0; r--)
                for (int b = b1; b >= b0; b--)
                    if (cell(r, g, b)) {
                        *og1 = g;
                        return;
                    }
    }();
    for (int b = b1; b >= b0; b--)
        for (int g = g1; g >= g0; g--)
            for (int r = r1; r >= r0; r--)
                if (cell(r, g, b)) {
                    *ob1 = b;
                    return;
                }
}

// 0x550660
void __fastcall DrawPalette_Cut(DWORD *box)
{
    const DWORD n = qg(glob_addr(Q_NBOXES));
    DWORD *nb = at<DWORD *>(qg(glob_addr(Q_BOXES))) + n * 12;
    qg(glob_addr(Q_NBOXES)) = n + 1;
    const int r0 = (int)box[6], g0 = (int)box[7], b0 = (int)box[8], r1 = (int)box[9], g1 = (int)box[10], b1 = (int)box[11];
    const DWORD tc = box[1], tq = box[2], tr = box[3], tg = box[4], tb = box[5];
    // the best plane of each axis: [lo, s] and [s + 1, hi]
    DWORD best[3], at_[3] = {0, 0, 0};
    for (int axis = 0; axis < 3; axis++) {
        DWORD ac = 0, aq = 0, ar = 0, ag = 0, ab = 0, rc = tc, rq = tq, rr = tr, rg = tg, rb = tb;
        best[axis] = 0xffffffff;
        const int lo = axis == 0 ? r0 : axis == 1 ? g0 : b0, hi = axis == 0 ? r1 : axis == 1 ? g1 : b1;
        for (int s = lo; s < hi; s++) {
            DWORD c, q, r, g, b;
            if (axis == 0)
                box_bottom(s, g0, b0, s, g1, b1, &c, &q, &r, &g, &b);
            else if (axis == 1)
                box_bottom(r0, s, b0, r1, s, b1, &c, &q, &r, &g, &b);
            else
                box_bottom(r0, g0, s, r1, g1, s, &c, &q, &r, &g, &b);
            ac += c, aq += q, ar += r, ag += g, ab += b;
            rc -= c, rq -= q, rr -= r, rg -= g, rb -= b;
            const int rest = box_rate(rq, rr, rg, rb, rc);
            const DWORD v = (DWORD)rest + (DWORD)box_rate(aq, ar, ag, ab, ac);
            if (v < best[axis]) {
                best[axis] = v;
                at_[axis] = (DWORD)s;
            }
        }
    }
    int axis = 0;
    DWORD least = best[0];
    if (best[1] < least) {
        axis = 1;
        least = best[1];
    }
    if (best[2] < least)
        axis = 2;
    // the first half keeps the box's lower corner, the second its upper
    int ar0 = r0, ag0 = g0, ab0 = b0, ar1 = r1, ag1 = g1, ab1 = b1;
    int br0 = r0, bg0 = g0, bb0 = b0, br1 = r1, bg1 = g1, bb1 = b1;
    if (axis == 0) {
        ar1 = (int)at_[0];
        br0 = (int)at_[0] + 1;
    } else if (axis == 1) {
        ag1 = (int)at_[1];
        bg0 = (int)at_[1] + 1;
    } else {
        ab1 = (int)at_[2];
        bb0 = (int)at_[2] + 1;
    }
    box_volume(r0, g0, b0, ar1, ag1, ab1, &ar0, &ag0, &ab0, &ar1, &ag1, &ab1);
    box_volume(br0, bg0, bb0, br1, bg1, bb1, &br0, &bg0, &bb0, &br1, &bg1, &bb1);
    DWORD c, q, r, g, b;
    box_bottom(ar0, ag0, ab0, ar1, ag1, ab1, &c, &q, &r, &g, &b);
    const DWORD bc = tc - c, bq = tq - q, brr = tr - r, bgg = tg - g, bbb = tb - b;
    box[0] = (DWORD)box_rate(q, r, g, b, c);
    box[3] = r;
    box[2] = q;
    box[4] = g;
    box[6] = (DWORD)ar0;
    box[5] = b;
    box[7] = (DWORD)ag0;
    box[9] = (DWORD)ar1;
    box[8] = (DWORD)ab0;
    box[10] = (DWORD)ag1;
    box[11] = (DWORD)ab1;
    box[1] = c;
    nb[0] = (DWORD)box_rate(bq, brr, bgg, bbb, bc);
    nb[1] = bc;
    nb[2] = bq;
    nb[3] = brr;
    nb[4] = bgg;
    nb[5] = bbb;
    nb[6] = (DWORD)br0;
    nb[7] = (DWORD)bg0;
    nb[8] = (DWORD)bb0;
    nb[9] = (DWORD)br1;
    nb[10] = (DWORD)bg1;
    nb[11] = (DWORD)bb1;
    int kind = 0;
    if (ab1 - ab0 - ag0 - ar0 + ag1 + ar1)
        kind = 2;
    if (bb1 - bb0 - bg0 - br0 + bg1 + br1)
        kind++;
    DWORD *heap = heap_of();
    switch (kind) {
    case 0: {
        heap[1] = heap[qg(glob_addr(Q_HEAP_SIZE))];
        const DWORD size = --qg(glob_addr(Q_HEAP_SIZE));
        if (!size)
            return;
        const DWORD top = heap_of()[1];
        sift_down(*at<DWORD *>(top), top);
        return;
    }
    case 1: {
        heap[1] = ptr_value(nb);
        const DWORD top = heap_of()[1];
        sift_down(*at<DWORD *>(top), top);
        return;
    }
    case 2: {
        const DWORD top = heap[1];
        sift_down(*at<DWORD *>(top), top);
        return;
    }
    default: {
        const DWORD top = heap[1];
        sift_down(*at<DWORD *>(top), top);
        const DWORD key = nb[0];
        DWORD pos = ++qg(glob_addr(Q_HEAP_SIZE)), parent = pos >> 1;
        while (parent) {
            DWORD *h = heap_of();
            const DWORD p = h[parent];
            if (*at<DWORD *>(p) >= key)
                break;
            h[pos] = p;
            pos = parent;
            parent >>= 1;
        }
        heap_of()[pos] = ptr_value(nb);
        return;
    }
    }
}

// 0x54ff70
DWORD __fastcall DrawPalette_Quantize(DWORD *histogram, int colours, BYTE *out, int *count)
{
    qg(glob_addr(Q_HIST)) = ptr_value(histogram);
    qg(glob_addr(Q_HEAP)) = 0;
    qg(glob_addr(Q_BOXES)) = 0;
    DWORD result;
    qg(glob_addr(Q_HEAP)) = ptr_value(heap_alloc((DWORD)colours * 4 + 4));
    if (!qg(glob_addr(Q_HEAP))) {
        result = 1;
    } else {
        qg(glob_addr(Q_BOXES)) = ptr_value(heap_alloc((DWORD)colours * 0x30));
        if (!qg(glob_addr(Q_BOXES))) {
            result = 1;
        } else {
            int r0, g0, b0, r1, g1, b1;
            box_volume(0, 0, 0, 0x3f, 0x3f, 0x3f, &r0, &g0, &b0, &r1, &g1, &b1);
            DWORD c, q, r, g, b;
            box_bottom(r0, g0, b0, r1, g1, b1, &c, &q, &r, &g, &b);
            DWORD *boxes = at<DWORD *>(qg(glob_addr(Q_BOXES)));
            boxes[1] = c;
            boxes[0] = 1;
            boxes = at<DWORD *>(qg(glob_addr(Q_BOXES)));
            boxes[2] = q;
            boxes[3] = r;
            boxes[4] = g;
            boxes[5] = b;
            boxes[6] = (DWORD)r0;
            boxes[7] = (DWORD)g0;
            boxes[8] = (DWORD)b0;
            boxes[9] = (DWORD)r1;
            boxes[10] = (DWORD)g1;
            boxes[11] = (DWORD)b1;
            qg(glob_addr(Q_NBOXES)) = 1;
            qg(glob_addr(Q_HEAP_SIZE)) = 0;
            if (r1 - r0 - g0 - b0 + g1 + b1) {
                heap_of()[1] = qg(glob_addr(Q_BOXES));
                qg(glob_addr(Q_HEAP_SIZE)) = 1;
            }
            result = 0;
            while ((int)qg(glob_addr(Q_NBOXES)) < colours && qg(glob_addr(Q_HEAP_SIZE)) && heap_of()[1])
                zax::fn<void(__fastcall *)(DWORD)>(0x550660)(heap_of()[1]);
            const int nboxes = (int)qg(glob_addr(Q_NBOXES));
            for (int i = 0; i < (int)qg(glob_addr(Q_NBOXES)); i++) {
                const DWORD *bx = at<const DWORD *>(qg(glob_addr(Q_BOXES))) + i * 12;
                const DWORD cnt = bx[1], half = cnt >> 1;
                *out++ = (BYTE)((half + bx[3] * 4) / cnt);
                *out++ = (BYTE)((half + bx[4] * 4) / cnt);
                *out++ = (BYTE)((half + bx[5] * 4) / cnt);
            }
            *count = nboxes > 0 ? (int)qg(glob_addr(Q_NBOXES)) : nboxes;
        }
    }
    if (qg(glob_addr(Q_HEAP))) {
        heap_free(at<void *>(qg(glob_addr(Q_HEAP))));
        qg(glob_addr(Q_HEAP)) = 0;
    }
    if (qg(glob_addr(Q_BOXES))) {
        heap_free(at<void *>(qg(glob_addr(Q_BOXES))));
        qg(glob_addr(Q_BOXES)) = 0;
    }
    return result;
}

ZAX_HOOKS(palette_rest4b)
{
    const Hook list[] = {
#include "palette_rest4b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
