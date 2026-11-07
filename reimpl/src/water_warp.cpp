// The animated water (phase 4, rendering; docs/DECOMPILATION.md 9, "Effects and text: the hue change effect and its
// cache, the text overlays, …"): CWaterWarpEffect, the four wave forms that bend a 128 × 128 texture, and
// CWaterWarpEffectFrame, which picks the texture and keeps the bent frame.
//
// CWaterWarpEffect (0x86c, vtable 0x60292c, class 0x713dc8, version 1, no parent) [confirmed: the constructor
// 0x597480, RegisterFields 0x5970a0]: four embedded CWaterWaveFormData (0x218, vtable 0x6028cc, class 0x713dc4;
// section 6) "V1 Wave" +8, "V2 Wave" +0x220, "H1 Wave" +0x438, "H2 Wave" +0x650 (each +8 the amplitude, +0xc
// the speed, +0x10 the period, +0x14 a table of 128 ints, +0x214 cleared), +0x868 "Show Grid". The
// constructor sets the waves to V1 (3.07, 19.13, 2), V2 (1.33, -37, 6), H1 (3.05, 15.27, 2), H2 (1.1, -29.33, 6)
// (the floats' bits 0x40447ae1, 0x41990a3d, …) and fills each table: entry i = fistp(float(cos(((i · period)
// << 1) · the float at 0x602928) · amplitude)) with the x87 fcos.
// The warps (thiscall effect; destination frame, source frame; 128 × 128, rows of 128 pixels from slot 31)
// [confirmed: code]: a wave's phase is fistp(float(the game time (double at g_ErrorReporter 0x6c02cc +0x40) ×
// its speed)) mod 128 (non-negative). 0x597690 shifts every row (16 bits) by H1[row + phase H1] + H2[row +
// phase H2]; 0x597780 every column by V1 and V2; 0x597880 (8 or 16 bits by the destination's depth, else
// nothing) and 0x597ad0 (16 bits) shift both ways in one pass: a pixel takes the source's pixel at u = x + the
// row's horizontal shift and the row y + the vertical shift at u (0x597ad0 the same with rows and columns
// swapped).
// CWaterWarpEffectFrame (0x880, vtable 0x602988, class 0x713dcc, parent CWaterWarpEffect): +0x86c the bent
// frame (a CFrame of 128 × 128 at the display's depth, made on first use, deleted with the object), +0x870
// "Static Texture" (the ground texture list 0x6c1d50), +0x874 "Animated Texture" (0x6c1ee8), +0x878 "Render
// Type" (the list 0x713c60, "Horizontal Then Vertical"), +0x87c the game time of the last update (float).
// 0x598420 bends the frame once per game time: render types 0-3 take the static texture (loaded through
// DefinitionList_LoadRecord when missing), 4 and above the animated texture's frame at time × its slot 26
// (fistp) mod its frame count; types 0, 1, 2/4, 3 run 0x597690, 0x597780, 0x597880, 0x597ad0.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_EFFECT = 0x60292c, VT_FRAME = 0x602988, VT_WAVE = 0x6028cc;
constexpr DWORD EFFECT_VAR = 0x713dc8, FRAME_VAR = 0x713dcc, WAVE_VAR = 0x713dc4;
constexpr DWORD WAVES[4] = {8, 0x220, 0x438, 0x650};
constexpr DWORD APP = 0x6c02cc;

long double x87_fcos(long double v)
{
    long double r;
    __asm__("fcos" : "=t"(r) : "0"(v));
    return r;
}
void fill_wave(BYTE *w, DWORD period, DWORD amp, DWORD speed)
{
    dw(w, 0x10) = period;
    dw(w, 8) = amp;
    dw(w, 0xc) = speed;
    dw(w, 0x214) = 0;
    for (DWORD i = 0; i < 0x80; i++) {
        const DWORD v = (DWORD)((int)(i & 0x7f) * (int)dw(w, 0x10)) << 1;
        const float f = fstp(x87_fcos(fild64((long long)v) * cst(glob_addr(0x602928))) * fld(fl<float>(w, 8)));
        dw(w, 0x14 + i * 4) = (DWORD)x87_fistp(fld(f));
    }
}
void effect_construct(BYTE *o)
{
    obj_ctor(o);
    for (DWORD off : WAVES) {
        obj_ctor(o + off);
        vtable_and_defaults(o + off, vt(VT_WAVE));
    }
    vtable_and_defaults(o, vt(VT_EFFECT));
    fill_wave(o + 8, 2, 0x40447ae1, 0x41990a3d);
    fill_wave(o + 0x220, 6, 0x3faa3d71, 0xc2140000);
    fill_wave(o + 0x438, 2, 0x40433333, 0x417451ec);
    fill_wave(o + 0x650, 6, 0x3f8ccccd, 0xc1eaa3d7);
    o[0x868] = 0;
}
void effect_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_EFFECT));
    for (int i = 3; i >= 0; i--) {
        vtable_and_free(o + WAVES[i], vt(VT_WAVE));
        obj_dtor(o + WAVES[i]);
    }
    obj_dtor(o);
}
void frame_construct(BYTE *o)
{
    fn<BYTE *(THISCALL *)(void *)>(0x597480)(o);
    vtable_and_defaults(o, vt(VT_FRAME));
    dw(o, 0x86c) = 0;
    dw(o, 0x87c) = 0;
}
void frame_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_FRAME);
    if (BYTE *f = bp(dw(o, 0x86c))) {
        vcall<void>(f, 1, 1);
        dw(o, 0x86c) = 0;
    }
    obj_free_fields(o);
    effect_destroy(o);
}

// A wave's phase: fistp(float(time × speed)) mod 128, non-negative
DWORD phase(BYTE *effect, DWORD speed_at)
{
    const long double t = *reinterpret_cast<const double *>(bp(*glob<DWORD *>(APP)) + 0x40);
    int v = x87_fistp(fld(fstp(t * fld(fl<float>(effect, speed_at)))));
    v %= 0x80;
    if (v < 0)
        v += 0x80;
    return (DWORD)v;
}
inline int tab(BYTE *effect, DWORD table_at, DWORD i) { return (int)dw(effect, table_at + (i & 0x7f) * 4); }
inline BYTE *pixels(void *frame) { return vcall<BYTE *>(frame, 31); }

constexpr DWORD V1 = 0x1c, V2 = 0x234, H1 = 0x44c, H2 = 0x664;              // the tables
constexpr DWORD V1_SPEED = 0x14, V2_SPEED = 0x22c, H1_SPEED = 0x444, H2_SPEED = 0x65c;

}  // namespace

// ---- The classes -----------------------------------------------------------------------------------------------

// 0x597040 (slot 2)
static CClassInfo *__cdecl WaterWarp_ClassInfo()
{
    return class_getter(EFFECT_VAR, 0x624770 /* "CWaterWarpEffect" */, 1.0f, nullptr, 0x86c, 0, 0x596e40, 0x596e60, 0x5970a0);
}
static CClassInfo *wave_class()
{
    return class_getter(WAVE_VAR, 0x624744 /* "CWaterWaveFormData" */, 1.0f, nullptr, 0x218, 0, 0x596a40, 0x596a70, 0x596b70);
}
// 0x597480 (thiscall): the constructor
static BYTE *THISCALL WaterWarp_Ctor(BYTE *o)
{
    effect_construct(o);
    return o;
}
// 0x596e40
static void *__cdecl WaterWarp_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x86c));
    return o ? fn<BYTE *(THISCALL *)(void *)>(0x597480)(o) : nullptr;
}
static void effect_construct_call(BYTE *o) { fn<BYTE *(THISCALL *)(void *)>(0x597480)(o); }
// 0x596e60 (count in ecx)
static void *__fastcall WaterWarp_CreateArray(int count, int, int) { return create_array<0x86c, effect_construct_call>(count); }
// 0x596ec0 (slot 1)
static void *THISCALL WaterWarp_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x86c, effect_destroy>(self, flags); }

// 0x5970a0: the four waves (embedded CWaterWaveFormData, "Amplitude" first), "Show Grid"
static DWORD __cdecl WaterWarp_RegisterFields()
{
    static const GlobKey NAMES[4] = {0x62479c /* "V1 Wave" */, 0x624794 /* "V2 Wave" */, 0x62478c /* "H1 Wave" */, 0x624784 /* "H2 Wave" */};
    for (int i = 0; i < 4; i++) {
        void *f = heap_alloc(0x34);
        if (f) {
            CClassInfo *c = wave_class();
            f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(0x5284c0)(f, key_glob_addr(NAMES[i]), WAVES[i], 0x218, WAVES[i], c,
                                                                                                             glob_addr(0x624758) /* "Amplitude" */, 1);
        }
        add_field(WaterWarp_ClassInfo(), f);
    }
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x618178) /* "Show Grid" */, 0x868, 1, 0x868, 1, 0, 0);
    return add_field(WaterWarp_ClassInfo(), f);
}

// 0x597f30 (slot 2), 0x598a20 (slot 57 of the effect frames' list)
static CClassInfo *__cdecl WaterWarpFrame_ClassInfo()
{
    return class_getter(FRAME_VAR, 0x6247a4 /* "CWaterWarpEffectFrame" */, 1.0f, WaterWarp_ClassInfo, 0x880, 0, 0x597ca0, 0x597cf0, 0x597fe0);
}
// 0x597ca0
static void *__cdecl WaterWarpFrame_Create() { return create_one<0x880, frame_construct>(); }
// 0x597cf0 (count in ecx)
static void *__fastcall WaterWarpFrame_CreateArray(int count, int, int) { return create_array<0x880, frame_construct>(count); }
// 0x597d60 (slot 1)
static void *THISCALL WaterWarpFrame_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x880, frame_destroy>(self, flags); }

// 0x597fe0: the three fields, then 0x598280 (after loading: the list "Water Warp Effects" 0x713c80) added to
// the static list 0x62e6a8 (0x419ea0, fastcall; a tail jump, eax its)
static DWORD __cdecl WaterWarpFrame_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6247cc) /* "Render Type" */, 0x878, 4, 0x878, glob_addr(0x713c60), 1, glob_addr(0x624710) /* "Horizontal Then Vertical" */,
                        0, 0);
    add_field(WaterWarpFrame_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6247bc) /* "Static Texture" */, 0x870, 4, 0x870, glob_addr(0x6c1d50), 1, 0, 0, 0);
    add_field(WaterWarpFrame_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x62054c) /* "Animated Texture" */, 0x874, 4, 0x874, glob_addr(0x6c1ee8), 1, 0, 0, 0);
    add_field(WaterWarpFrame_ClassInfo(), f);
    return fn<DWORD(__fastcall *)(DWORD)>(0x419ea0)(fn_addr(0x598280));
}

// ---- The warps -------------------------------------------------------------------------------------------------

// 0x597690: every row shifted by the H waves (16 bits)
static void THISCALL WaterWarp_Rows(BYTE *e, void *dst, void *src)
{
    const DWORD p1 = phase(e, H1_SPEED), p2 = phase(e, H2_SPEED);
    WORD *d = reinterpret_cast<WORD *>(pixels(dst));
    const BYTE *s = pixels(src);
    for (DWORD row = 0; row < 0x80; row++, s += 0x100) {
        const int shift = tab(e, H2, p2 + row) + tab(e, H1, p1 + row);
        for (DWORD x = 0; x < 0x80; x++)
            *d++ = reinterpret_cast<const WORD *>(s)[(x + (DWORD)shift) & 0x7f];
    }
}

// 0x597780: every column shifted by the V waves (16 bits)
static void THISCALL WaterWarp_Columns(BYTE *e, void *dst, void *src)
{
    DWORD p1 = phase(e, V1_SPEED);
    const DWORD p2 = phase(e, V2_SPEED);
    BYTE *d = pixels(dst);
    const BYTE *s = pixels(src);
    for (DWORD col = 0; col < 0x80; col++, p1++) {
        const int shift = tab(e, V2, p2 + col) + tab(e, V1, p1);
        for (DWORD y = 0; y < 0x80; y++)
            reinterpret_cast<WORD *>(d + col * 2)[y * 0x80] = *reinterpret_cast<const WORD *>(s + col * 2 + (((y + (DWORD)shift) & 0x7f) << 8));
    }
}

// 0x597880: rows and columns in one pass, 8 or 16 bits by the destination's depth (slot 29)
static void THISCALL WaterWarp_Both(BYTE *e, void *dst, void *src)
{
    const DWORD ph1 = phase(e, H1_SPEED), ph2 = phase(e, H2_SPEED), pv1 = phase(e, V1_SPEED), pv2 = phase(e, V2_SPEED);
    const DWORD depth = vcall<DWORD>(dst, 29);
    if (depth == 8) {
        BYTE *d = pixels(dst);
        const BYTE *s = pixels(src);
        for (DWORD y = 0; y < 0x80; y++, d += 0x80) {
            const int a = tab(e, H2, ph2 + y), b = tab(e, H1, y + ph1);
            for (DWORD x = 0; x < 0x80; x++) {
                const DWORD u = (x + (DWORD)a + (DWORD)b) & 0x7f;
                d[x] = s[(((DWORD)tab(e, V2, pv2 + u) + (DWORD)tab(e, V1, pv1 + u) + y) & 0x7f) * 0x80 + u];
            }
        }
    } else if (depth == 16) {
        WORD *d = reinterpret_cast<WORD *>(pixels(dst));
        const WORD *s = reinterpret_cast<const WORD *>(pixels(src));
        for (DWORD y = 0; y < 0x80; y++) {
            const int b = tab(e, H1, y + ph1), a = tab(e, H2, ph2 + y);
            for (DWORD x = 0; x < 0x80; x++) {
                const DWORD u = ((DWORD)a + (DWORD)b + x) & 0x7f;
                *d++ = s[(((DWORD)tab(e, V2, pv2 + u) + (DWORD)tab(e, V1, pv1 + u) + y) & 0x7f) * 0x80 + u];
            }
        }
    }
}

// 0x597ad0: columns and rows in one pass (16 bits)
static void THISCALL WaterWarp_BothColumns(BYTE *e, void *dst, void *src)
{
    const DWORD ph1 = phase(e, H1_SPEED), ph2 = phase(e, H2_SPEED), pv1 = phase(e, V1_SPEED), pv2 = phase(e, V2_SPEED);
    WORD *d = reinterpret_cast<WORD *>(pixels(dst));
    const WORD *s = reinterpret_cast<const WORD *>(pixels(src));
    for (DWORD c = 0; c < 0x80; c++, d++) {
        const int a = tab(e, V1, c + pv1), b = tab(e, V2, pv2 + c);
        WORD *p = d;
        for (DWORD y = 0; y < 0x80; y++, p += 0x80) {
            const DWORD u = ((DWORD)b + (DWORD)a + y) & 0x7f;
            *p = s[(((DWORD)tab(e, H2, ph2 + u) + (DWORD)tab(e, H1, ph1 + u) + c) & 0x7f) + u * 0x80];
        }
    }
}

// 0x598420 (fastcall effect frame): the bent frame for the game time (see the head comment)
static BYTE *__fastcall WaterWarpFrame_Update(BYTE *o)
{
    // the double compared with the float as it is (fcom, NaN counts as equal), then stored rounded
    const long double t = *reinterpret_cast<const double *>(bp(*glob<DWORD *>(APP)) + 0x40);
    if (c3(t, fld(fl<float>(o, 0x87c))))
        return bp(dw(o, 0x86c));
    fl<float>(o, 0x87c) = fstp(t);
    if (!dw(o, 0x86c)) {
        BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
        if (f)
            f = fn<BYTE *(THISCALL *)(void *)>(0x4a8a90)(f);
        dw(o, 0x86c) = (DWORD)(uintptr_t)f;
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, 0x80, 0x80, fn<DWORD(__cdecl *)()>(0x55d030)());
    }
    void *src;
    if (dw(o, 0x878) < 4) {
        DWORD i = dw(o, 0x870);
        src = nullptr;
        if (BYTE *records = *glob<BYTE **>(0x6c1d80)) {
            if (i >= *glob<DWORD *>(0x6c1d84))
                i = 0;
            if (!records[i * 0x80 + 0x7f]) {
                if (!dw(records + i * 0x80, 0x78))
                    fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(0x6c1d50), i);
                src = bp(dw(*glob<BYTE **>(0x6c1d80) + i * 0x80, 0x78));
            }
        }
    } else {
        BYTE *seq = list_object(glob_addr(0x6c1ee8), glob_addr(0x6c1f18), glob_addr(0x6c1f1c), glob_addr(0x6c1f20), dw(o, 0x874));
        const double time = *reinterpret_cast<const double *>(bp(*glob<DWORD *>(APP)) + 0x40);
        const long double rate = vcall<long double>(seq, 26);
        const float f = fstp((long double)time * rate);
        const DWORD k = (DWORD)x87_fistp(fld(f));
        const DWORD n = vcall<DWORD>(seq, 25);
        src = vcall<void *>(seq, 23, k % n);
    }
    using Warp = void(THISCALL *)(void *, void *, void *);
    switch (dw(o, 0x878)) {
    case 0:
        fn<Warp>(0x597690)(o, bp(dw(o, 0x86c)), src);
        break;
    case 1:
        fn<Warp>(0x597780)(o, bp(dw(o, 0x86c)), src);
        break;
    case 2:
    case 4:
        fn<Warp>(0x597880)(o, bp(dw(o, 0x86c)), src);
        break;
    case 3:
        fn<Warp>(0x597ad0)(o, bp(dw(o, 0x86c)), src);
        break;
    }
    return bp(dw(o, 0x86c));
}

ZAX_HOOKS(water_warp)
{
    const Hook list[] = {
#include "water_warp_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
