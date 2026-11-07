// CLightningLayerPart, the lightning bolt part (phase 4, rendering; docs/DECOMPILATION.md 9, "The last
// effects: …"): a layer part that draws a jagged line from its position to its position + (X Length, Y Length −
// Height) as a list of segments, made anew on every draw by random midpoint displacement.
//
// Layout (0xb4, vtable 0x5f984c, class 0x6c2b8c, version 1, parent CSimpleLayerPart) [confirmed: Create
// 0x4fecf0, RegisterFields 0x4fefc0, docs/fields.csv]: +0x54 "Position/Z", +0x58 "Position/X Length" (100),
// +0x5c "Position/Y Length" (25), +0x60 "Position/Height" (the four with setters 0x500700-0x500730, which jump
// to slots 83-86), +0x64 the draw items (an array of 0x58-byte records, vtable 0x5f99b4), +0x74 the points
// (8-byte x, y records, vtable 0x5f99a8), +0x84/+0x88 the cosine and sine of the bolt's angle + π/2, +0x8c
// "Width" (percent, 0.25), +0x90 "Min Segment Length" (pixels, 5), +0x94 "Center Color" (default 0x6c2b78:
// white), +0xa4 "Edge Color" (0x6c2b68: 103, 35, 166); the defaults are function-local statics of
// RegisterFields.
//  - 0x500080 (the geometry): the points cleared, the bolt's angle atan2(Y Length − Height, X Length) wrapped
//    into [0, 2π), + π/2 wrapped again, its cosine and sine, the first point and 0x4fff60 from the start to the
//    end with the length.
//  - 0x4fff60 (from, to, length): while a random length below the minimum (Random_Below, rounded) plus the
//    minimum is not above the length, the midpoint moved by a random amount in ±length · width along the
//    normal and both halves recursively; else the end point appended.
//  - Slot 23 (the draw: a1, queue, a3, a4, offset): the flag 0x200000 cleared (slot 59), with a queue the
//    geometry, ⌊|Y Length| · 0.05⌉ (1 … segments) draw items, each a run of the points ((k · per) … ((k + 1) ·
//    per) − 1 with per = segments / items) with its sort y (k + 0.5 rows of Y Length / items down, plus the
//    offset's y and the part's y, · 8), the part's slots 52/53, the offset, the two colours and its bounding
//    box (0x4feb30: the points ± (cosine, sine), rounded, right and bottom + 1), put into the queue; then
//    CLayerPart's slot 23.
//  - Slot 25/27: the network block (CSimpleLayerPart's, Z and Height + 32768 and the lengths as words, the
//    width · 255 as a byte, the minimum − 1 as a word, the colours packed into three or four bytes: red and
//    green in 4 bits each (· 15 / 255), blue in 0 … 10 as centre · 11 + edge with bit 7 for "both opaque",
//    alpha in 4 bits unless opaque; read back as · 17, · 255 / 10).
//  - Slot 37 (rectangle, kind): for kinds 0, 2, 5 the box around the bolt widened by width along the normal
//    (8 rounded corners), else false. Slot 38: kinds 0-3 hit. Slots 83-86: a position field set between two
//    relists (slot 78). Slot 61 (shared with other parts): the class's short name.
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT = 0x5f984c, VT_ITEMS = 0x5f99b4, VT_POINTS = 0x5f99a8, CLASS_VAR = 0x6c2b8c;
constexpr DWORD RANDOM = 0x7124c0;
using LD = long double;

void construct(BYTE *o)
{
    zax::fn<void *(THISCALL *)(void *)>(0x571680)(o);
    dw(o, 0x64) = vt(VT_ITEMS);
    dw(o, 0x68) = dw(o, 0x6c) = dw(o, 0x70) = 0;
    dw(o, 0x74) = vt(VT_POINTS);
    dw(o, 0x78) = dw(o, 0x7c) = dw(o, 0x80) = 0;
    zax::fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0x94);
    zax::fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0xa4);
    vtable_and_defaults(o, vt(VT));
}
// The two arrays' heap data freed (operator delete), their fields cleared; CSimpleLayerPart's destructor
void destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    dw(o, 0x74) = vt(VT_POINTS);
    if (void *d = bp(dw(o, 0x78))) {
        heap_free(d);
        dw(o, 0x78) = 0;
    }
    dw(o, 0x80) = dw(o, 0x7c) = 0;
    dw(o, 0x64) = vt(VT_ITEMS);
    if (void *d = bp(dw(o, 0x68))) {
        heap_free(d);
        dw(o, 0x68) = 0;
    }
    dw(o, 0x70) = dw(o, 0x6c) = 0;
    zax::fn<void(THISCALL *)(void *)>(0x5716d0)(o);
}

int fistp_float(LD v) { return x87_fistp(fld(fstp(v))); }
int clamp_word(int v) { return v > 0xffff ? 0xffff : v < 0 ? 0 : v; }

DWORD write_byte(void *p, DWORD v) { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x520820)(p, v); }
DWORD write_word(void *p, DWORD v) { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x5209c0)(p, v); }
DWORD read_byte(void *p, BYTE *out) { return zax::fn<DWORD(THISCALL *)(void *, BYTE *)>(0x520490)(p, out); }
DWORD read_word(void *p, void *out) { return zax::fn<DWORD(THISCALL *)(void *, void *)>(0x520510)(p, out); }

// A colour channel to 4 bits (· 15 / 255) and to 0 … 10 (· 10 / 255), as the exe's multiplications
DWORD to15(DWORD c) { return (DWORD)(((unsigned long long)(c * 15) * 0x80808081ull) >> 39); }
DWORD to10(DWORD c) { return (DWORD)(((unsigned long long)(c * 10) * 0x80808081ull) >> 39); }

}  // namespace

// 0x4fec90, 0x4fef50 (slot 2)
CClassInfo *__cdecl Lightning_ClassInfo()
{
    return class_getter(CLASS_VAR, 0x619008 /* "CLightningLayerPart" */, 1.0f, getter<0x5714b0>, 0xb4, 0, 0x4fecf0, 0x4fed60, 0x4fefc0);
}
// 0x4fecf0
void *__cdecl Lightning_Create() { return create_one<0xb4, construct>(); }
// 0x4fed60 (count in ecx)
void *__fastcall Lightning_CreateArray(int count, int, int) { return create_array<0xb4, construct>(count); }
// 0x4fee10 (slot 1)
void *THISCALL Lightning_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xb4, destroy>(self, flags); }

// 0x4fefc0: the colours' defaults (statics), the eight fields
DWORD __cdecl Lightning_RegisterFields()
{
    BYTE &guard = *glob<BYTE *>(0x6c2b88);
    if (!(guard & 1)) {
        guard |= 1;
        zax::fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob<void *>(0x6c2b68), 0x67, 0x23, 0xa6, 0xff);
        zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x4ff4e0));
    }
    if (!(guard & 2)) {
        guard |= 2;
        zax::fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob<void *>(0x6c2b78), 0xff, 0xff, 0xff, 0xff);
        zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x4ff4d0));
    }
    constexpr DWORD FLOAT_FIELD = 0x54ecd0, FLOAT_UNITS = 0x54ec40, COLOUR_FIELD = 0x46dcc0;
    // each descriptor made, then the class getter (inline in the exe) and AddField
    void *f;
    f = new_field(0x44, FLOAT_FIELD, glob_addr(0x61907c) /* "Position/Z" */, 0x54, 4, 0x54, 1, 0, fn_addr(0x500700), 0, 0, 0);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD, glob_addr(0x619068) /* "Position/X Length" */, 0x58, 4, 0x58, 1, 0x42c80000, fn_addr(0x500710), 0, 0, 0);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD, glob_addr(0x619054) /* "Position/Y Length" */, 0x5c, 4, 0x5c, 1, 0x41c80000, fn_addr(0x500720), 0, 0, 0);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD, glob_addr(0x6131c0) /* "Position/Height" */, 0x60, 4, 0x60, 1, 0, fn_addr(0x500730), 0, 0, 0);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x44, FLOAT_UNITS, glob_addr(0x60a5e8) /* "Width" */, glob_addr(0x61904c) /* "Percent" */, 0x8c, 4, 0x8c, 1, 0x3e800000, 0, 0, 0x3f800000, 0x3d4ccccd);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x44, FLOAT_UNITS, glob_addr(0x619038) /* "Min Segment Length" */, glob_addr(0x60b360) /* "Pixels" */, 0x90, 4, 0x90, 1, 0x40a00000, 0, 0x3f800000, 0x42c80000, 0x3f800000);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x3c, COLOUR_FIELD, glob_addr(0x619028) /* "Center Color" */, 0x94, 0x10, 0x94, 1, glob_addr(0x6c2b78), 0);
    add_field(Lightning_ClassInfo(), f);
    f = new_field(0x3c, COLOUR_FIELD, glob_addr(0x61901c) /* "Edge Color" */, 0xa4, 0x10, 0xa4, 1, glob_addr(0x6c2b68), 0);
    return add_field(Lightning_ClassInfo(), f);
}

// 0x4ff4f0 (session): the block reader 0x6c2b58 registered ("CLightningBlockReaderObject")
DWORD THISCALL Lightning_RegisterReader(void *session)
{
    return zax::fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(session, glob<void *>(0x6c2b58), glob<const char *>(0x619088));
}

// 0x4fff60 (from, to, length)
void THISCALL Lightning_Subdivide(BYTE *self, const float *from, const float *to, float length)
{
    const int m = x87_fistp(fld(fl<float>(self, 0x90)));
    const DWORD r = zax::fn<DWORD(THISCALL *)(void *, int)>(0x55c470)(glob<void *>(RANDOM), m);
    if (!c03(fild64((long long)r) + fld(fl<float>(self, 0x90)), fld(length))) {
        vcall<void>(self + 0x74, 2, to);
        return;
    }
    const float c = fl<float>(self, 0x84), s = fl<float>(self, 0x88);
    const LD dx = fld(to[0]) - fld(from[0]);
    const float dy = fstp(fld(to[1]) - fld(from[1]));
    float mid[2];
    const LD hx = dx * cst(glob_addr(0x5eee2c));
    const float hy = fstp(fld(dy) * cst(glob_addr(0x5eee2c)));
    mid[0] = fstp(hx + fld(from[0]));
    mid[1] = fstp(fld(hy) + fld(from[1]));
    const float w = fstp(fld(length) * fld(fl<float>(self, 0x8c)));
    const LD d = zax::fn<LD(THISCALL *)(void *, float, float)>(0x55c550)(glob<void *>(RANDOM), fstp(-fld(w)), w);
    const float dc = fstp(d * fld(c));
    const LD ds = d * fld(s);
    mid[0] = fstp(fld(mid[0]) + fld(dc));
    mid[1] = fstp(fld(mid[1]) + ds);
    const float half = fstp(fld(length) * cst(glob_addr(0x5eee2c)));
    zax::fn<void(THISCALL *)(void *, const float *, const float *, float)>(0x4fff60)(self, from, mid, half);
    zax::fn<void(THISCALL *)(void *, const float *, const float *, float)>(0x4fff60)(self, mid, to, half);
}

// 0x500080: the geometry (see the head comment)
void THISCALL Lightning_UpdateGeometry(BYTE *self)
{
    const float d = fstp(fld(fl<float>(self, 0x5c)) - fld(fl<float>(self, 0x60)));
    const float xl = fl<float>(self, 0x58);
    float p0[2] = {fl<float>(self, 0x4c), fstp(fld(fl<float>(self, 0x50)) - fld(fl<float>(self, 0x54)))};
    float p1[2] = {fstp(fld(p0[0]) + fld(xl)), fstp(fld(p0[1]) + fld(d))};
    vcall<void>(self + 0x74, 1);
    const float len2 = fstp(fld(xl) * fld(xl) + fld(d) * fld(d));
    LD a = zax::fn<LD(__stdcall *)(float, float)>(0x509100)(d, xl);
    if (c0(a, cst(glob_addr(C_ZERO))))
        a = a + cst(glob_addr(C_TWO_PI));
    if (!c0(a, cst(glob_addr(C_TWO_PI))))
        a = a - cst(glob_addr(C_TWO_PI));
    const LD ab = a + cst(glob_addr(0x5ee000));
    float b = fstp(ab);
    if (c0(ab, cst(glob_addr(C_ZERO))))   // the unrounded sum compared (fst, then fcomp)
        b = fstp(fld(b) + cst(glob_addr(C_TWO_PI)));
    if (!c0(fld(b), cst(glob_addr(C_TWO_PI))))
        b = fstp(fld(b) - cst(glob_addr(C_TWO_PI)));
    fl<float>(self, 0x84) = fstp(zax::fn<LD(__stdcall *)(float)>(0x508da0)(b));
    fl<float>(self, 0x88) = fstp(zax::fn<LD(__stdcall *)(float)>(0x508ec0)(b));
    const float len = fstp(zax::fn<LD(__stdcall *)(float)>(0x508c50)(len2));
    vcall<void>(self + 0x74, 2, p0);
    zax::fn<void(THISCALL *)(void *, const float *, const float *, float)>(0x4fff60)(self, p0, p1, len);
}

// 0x4feb30 (a draw item): its bounding box from its run of points (see the head comment)
void THISCALL LightningItem_Bounds(BYTE *item)
{
    BYTE *part = bp(dw(item, 0x28));
    const float *pts = reinterpret_cast<const float *>(bp(dw(part, 0x78)));
    const DWORD first = dw(item, 0x2c);
    const int x0 = x87_fistp(fld(pts[first * 2]));
    dw(item, 0x3c) = dw(item, 0x34) = (DWORD)x0;
    const int y0 = x87_fistp(fld(pts[first * 2 + 1]));
    dw(item, 0x40) = dw(item, 0x38) = (DWORD)y0;
    const float c = fl<float>(part, 0x84), s = fl<float>(part, 0x88);
    int *r = reinterpret_cast<int *>(item + 0x34);
    for (DWORD i = first + 1; i <= dw(item, 0x30) + 1; i++) {
        const float px = pts[i * 2], py = pts[i * 2 + 1];
        const int y = fistp_float(fld(s) + fld(py)), x = fistp_float(fld(c) + fld(px));
        r[0] = r[0] < x ? r[0] : x;
        r[1] = r[1] < y ? r[1] : y;
        r[2] = r[2] > x ? r[2] : x;
        r[3] = r[3] > y ? r[3] : y;
        const int y2 = fistp_float(fld(py) - fld(s)), x2 = fistp_float(fld(px) - fld(c));
        r[0] = r[0] < x2 ? r[0] : x2;
        r[1] = r[1] < y2 ? r[1] : y2;
        r[2] = r[2] > x2 ? r[2] : x2;
        r[3] = r[3] > y2 ? r[3] : y2;
    }
    r[2]++;
    r[3]++;
}

// 0x4ff500 (slot 23: a1, queue, a3, a4, offset): the draw (see the head comment)
DWORD THISCALL Lightning_Draw(BYTE *self, DWORD a1, void *queue, DWORD a3, DWORD a4, const int *offset)
{
    if (dw(self, 0x1c) & 0x200000)
        vcall<void>(self, 59, 0x200000);
    if (queue) {
        zax::fn<void(THISCALL *)(void *)>(0x500080)(self);
        BYTE *items = self + 0x64;
        vcall<void>(items, 1);
        const DWORD segments = dw(self, 0x7c) - 1;
        DWORD n = (DWORD)fistp_float(fld(raw_float(dw(self, 0x5c) & 0x7fffffff)) * cst(glob_addr(0x5f99c0)));
        if (n < 1)
            n = 1;
        else if (n > segments)
            n = segments;
        const LD fn = fild64((long long)n);
        const float step = fstp(fld(fl<float>(self, 0x5c)) / fn);
        const float per = fstp(fild64((long long)segments) / fn);
        for (DWORD k = 0; k < n; k++) {
            zax::fn<void(THISCALL *)(void *, DWORD)>(0x500570)(items, dw(items, 8) + 1);
            BYTE *it = bp(dw(items, 4)) + dw(items, 8) * 0x58;
            dw(items, 8)++;
            it[0x24] = 1;
            const float fk = fstp(fild64((long long)k));
            it[0x25] = (BYTE)((dw(self, 0x1c) >> 0x16) & 1);
            const int y = fistp_float((fld(fk) + cst(glob_addr(0x5eee2c))) * fld(step) + (fild(offset[1]) + fld(fl<float>(self, 0x50))));
            dw(it, 0x18) = (DWORD)y * 8;
            dw(it, 0x1c) = vcall<DWORD>(self, 52);
            dw(it, 0x20) = vcall<DWORD>(self, 53);
            dw(it, 0x44) = (DWORD)offset[0];
            dw(it, 0x48) = (DWORD)offset[1];
            dw(it, 0x4c) = (DWORD)offset[2];
            dw(it, 0x50) = ptr_value(self + 0x94);
            dw(it, 0x54) = ptr_value(self + 0xa4);
            dw(it, 0x10) = ptr_value(self);
            dw(it, 8) = dw(it, 4) = dw(it, 0x14) = 0;
            const int first = fistp_float(fld(fk) * fld(per));
            const int last = fistp_float(fild64((long long)(k + 1)) * fld(per));
            dw(it, 0x28) = ptr_value(self);
            dw(it, 0x30) = (DWORD)(last - 1);
            dw(it, 0x2c) = (DWORD)first;
            zax::fn<void(THISCALL *)(void *)>(0x4feb30)(it);
        }
        for (DWORD i = 0; i < n; i++)
            zax::fn<DWORD(THISCALL *)(void *, void *)>(0x49ca30)(queue, bp(dw(self, 0x68)) + i * 0x58);
    }
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD, const int *)>(0x4fa9a0)(self, a1, queue, a3, a4, offset);
}

// 0x4ff710 (slot 25: packet, with the block type). eax: 1, with the type the part's slot 26 + 1.
DWORD THISCALL Lightning_WriteBlock(BYTE *self, void *p, DWORD with_type)
{
    DWORD r = 1;
    if ((BYTE)with_type) {
        r = vcall<DWORD>(self, 26, p) + 1;
        zax::fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(p, glob<void *>(0x6c2b58), 2);
    }
    zax::fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5716f0)(self, p, 0);
    write_word(p, (DWORD)clamp_word(x87_ftol(fld(fl<float>(self, 0x54)) + cst(glob_addr(0x5f99c4)))));
    write_word(p, (DWORD)x87_ftol(fld(fl<float>(self, 0x58))));
    write_word(p, (DWORD)x87_ftol(fld(fl<float>(self, 0x5c))));
    write_word(p, (DWORD)clamp_word(x87_ftol(fld(fl<float>(self, 0x60)) + cst(glob_addr(0x5f99c4)))));
    const int w = x87_ftol(fld(fl<float>(self, 0x8c)) * cst(glob_addr(0x5eb728)));
    const BYTE wb = w > 0xff ? 0xff : w < 0 ? 0 : (BYTE)w;
    write_byte(p, (with_type & 0xffffff00) | wb);
    write_word(p, (DWORD)clamp_word(x87_ftol(fld(fl<float>(self, 0x90)) - cst(glob_addr(0x5ec3f4)))));
    const BYTE *c = self + 0x94, *e = self + 0xa4;   // b, g, r, a
    BYTE x[4];
    x[0] = (BYTE)(((to15(c[2]) & 0xf) << 4) | (to15(e[2]) & 0xf));
    x[1] = (BYTE)(((to15(c[1]) & 0xf) << 4) | (to15(e[1]) & 0xf));
    x[2] = (BYTE)((BYTE)(to10(c[0]) * 11) + (BYTE)to10(e[0]));
    if (e[3] == 0xff && c[3] == 0xff)
        x[2] |= 0x80;
    x[3] = (BYTE)(((to15(c[3]) & 0xf) << 4) | (to15(e[3]) & 0xf));
    write_byte(p, x[0]);
    write_byte(p, x[1]);
    write_byte(p, x[2]);
    if (!(x[2] & 0x80))
        write_byte(p, x[3]);
    return r;
}

// 0x4ff9c0 (slot 27: packet): the block back
void THISCALL Lightning_ReadBlock(BYTE *self, void *p)
{
    zax::fn<void(THISCALL *)(void *, void *)>(0x571780)(self, p);
    DWORD w = 0;
    read_word(p, &w);
    fl<float>(self, 0x54) = fstp(fild((int)(w & 0xffff)) - cst(glob_addr(0x5f99c4)));
    short a = 0, b = 0;
    read_word(p, &a);
    read_word(p, &b);
    fl<float>(self, 0x58) = fstp(fild(a));
    fl<float>(self, 0x5c) = fstp(fild(b));
    DWORD h = 0;
    read_word(p, &h);
    fl<float>(self, 0x60) = fstp(fild((int)(h & 0xffff)) - cst(glob_addr(0x5f99c4)));
    BYTE wb = 0;
    read_byte(p, &wb);
    fl<float>(self, 0x8c) = fstp(fild(wb) * cst(glob_addr(0x5f172c)));
    DWORD m = 0;
    read_word(p, &m);
    fl<float>(self, 0x90) = fstp(fild((int)(m & 0xffff)) + cst(glob_addr(0x5ec3f4)));
    BYTE x[4] = {};
    read_byte(p, &x[0]);
    read_byte(p, &x[1]);
    read_byte(p, &x[2]);
    if (!(x[2] & 0x80))
        read_byte(p, &x[3]);
    else
        x[3] = 0xff;
    using SetRgba = void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
    x[2] &= 0x7f;
    const int ea = (int)(x[3] & 0xf) * 255 / 15, eb = (int)(x[2] % 11) * 255 / 10;
    const int eg = (int)(x[1] & 0xf) * 255 / 15, er = (int)(x[0] & 0xf) * 255 / 15;
    zax::fn<SetRgba>(0x53f080)(self + 0xa4, (DWORD)er, (DWORD)eg, (DWORD)eb, (DWORD)ea);
    x[0] >>= 4;
    x[1] >>= 4;
    x[2] = (BYTE)((x[2] - x[2] % 11) / 11);
    x[3] >>= 4;
    const int ca = (int)x[3] * 255 / 15, cb = (int)x[2] * 255 / 10, cg = (int)x[1] * 255 / 15, cr = (int)x[0] * 255 / 15;
    zax::fn<SetRgba>(0x53f080)(self + 0x94, (DWORD)cr, (DWORD)cg, (DWORD)cb, (DWORD)ca);
}

// 0x4ffcb0 (slot 37: rectangle out, kind, a3): kinds 0, 2 and 5 (see the head comment). al: whether set.
bool THISCALL Lightning_Rect(BYTE *self, int *out, DWORD kind, DWORD)
{
    if (kind > 5 || kind == 1 || kind == 3 || kind == 4)
        return false;
    const float x = fl<float>(self, 0x4c), xl = fl<float>(self, 0x58);
    const float y = fstp(fld(fl<float>(self, 0x50)) - fld(fl<float>(self, 0x54)));
    const float d = fstp(fld(fl<float>(self, 0x5c)) - fld(fl<float>(self, 0x60)));
    float s, c;
    zax::fn<void(__fastcall *)(float *, float *, DWORD)>(0x508fe0)(&s, &c, 0x3fc90fdb);
    const float wv = fl<float>(self, 0x8c);
    const LD u = fld(c) * fld(xl) + fld(s) * fld(d);
    const float v = fstp(-fld(s) * fld(xl) + fld(c) * fld(d));
    const float A = fstp(u * fld(wv)), B = fstp(fld(v) * fld(wv));
    const int e0 = fistp_float(fld(x) - fld(A)), e1 = fistp_float(fld(A) + fld(x));
    const int e2 = fistp_float(fld(xl) + fld(A) + fld(x)), e3 = fistp_float(fld(xl) + fld(x) - fld(A));
    const int f0 = fistp_float(fld(B) + fld(y)), f1 = fistp_float(fld(y) - fld(B));
    const int f2 = fistp_float(fld(d) + fld(y) - fld(B)), f3 = fistp_float(fld(B) + fld(d) + fld(y));
    auto mn = [](int a, int b) { return a < b ? a : b; };
    auto mx = [](int a, int b) { return a > b ? a : b; };
    const int left = mn(mn(e0, e1), mn(e2, e3)), right = mx(mx(e0, e1), mx(e2, e3));
    const int top = mn(mn(f0, f1), mn(f2, f3)), bottom = mx(mx(f0, f1), mx(f2, f3));
    out[0] = left;
    out[1] = top;
    out[2] = right;
    out[3] = bottom;
    return true;
}

// 0x4fff40 (slot 38: a1, a2, kind): kinds 0-3
bool THISCALL Lightning_HitKind(BYTE *, DWORD, DWORD, int kind) { return kind >= 0 && kind <= 3; }

// 0x4ffe80-0x4fff10 (slots 83-86: value): a position field set between two relists (slot 78). st0: it.
template <DWORD OFF> LD THISCALL Lightning_Set(BYTE *self, DWORD v)
{
    vcall<void>(self, 78);
    dw(self, OFF) = v;
    vcall<void>(self, 78);
    return fld(fl<float>(self, OFF));
}
// 0x500700-0x500730 (the fields' setters): slots 83-86
template <int SLOT> LD THISCALL Lightning_FieldSetter(BYTE *self, DWORD v) { return vcall<LD>(self, SLOT, v); }

// 0x5717d0 (slot 61, shared): the class's short name
const char *THISCALL LayerPart_ShortName(void *self) { return zax::fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(vcall<CClassInfo *>(self, 2)); }

ZAX_HOOKS(lightning)
{
    const Hook list[] = {
#include "lightning_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
