// The warp effects (phase 4, rendering; docs/DECOMPILATION.md 9, "The last effects: …"): CWarpEffect, the
// render effect base of the warps, and CSimpleWarpEffect, which bends a sprite's compressed frame through the
// frames of an animated texture (a CSequence) while the sprite is drawn.
//
// CWarpEffect (0x10, vtable 0x6027e4, class 0x713c50, abstract, parent CRenderEffect) [confirmed: getter,
// constructor 0x596490, slots]: +8 the strength (0..1, a byte · 1/255 in the network block), +0xc the rate.
// Slot 23/24 write/read the network block (the strength · 255 and slot 31 · 8 as bytes; read back · 1/255
// and · 0.125), slot 25 (entity) begins the effect on the entity's frame (its dynamic property +0x34 when
// it is a CFrame, else 0: slots 28 and 29), slot 26 ends it (slot 30), slot 27 does nothing (six
// arguments), slot 31 is the rate +0xc.
// CSimpleWarpEffect (0x30, vtable 0x60049c, class 0x713574, parent CWarpEffect) [confirmed: Create
// 0x571920, RegisterFields 0x571ad0, slots]: +0x10 … +0x20 the frame's five pixel blocks while the effect
// is on (slots 31 to 35 of the frame: the pixels, +0x1c, +0x24, +0x2c, +0x34), +0x24 on, +0x28 the bytes
// of the copy, +0x2c "Warp Effect" (the sequence list 0x6c2788). The constructor clears them and the
// strength +8.
//  - Slot 28 (begin, frame): the blocks saved; an uncompressed frame gets the render buffer (0x55e2f0, width
//    · height bytes) as its pixels; a compressed one (flags 2: five blocks, 4: two) a copy of its layout in
//    the render buffer (every block's size dword at its start, the blocks one after another), on.
//  - Slot 29 (draw, frame): an uncompressed frame's buffer is copied back through the 8-bit table at
//    0x6767c0 row (byte 0x713570); a compressed frame's blocks are bent from the saved ones into the copy
//    (0x571c50 per block with its row table, slots 46-50, and the frame's origin, slots 43/44).
//  - Slot 30 (end, frame): the buffer released, the blocks put back, off. Slot 31: the sequence's rate
//    (slot 26; the list's inline lookup loads it when missing, 0 without records).
//  - 0x571c50 (block, origin, row table, copy, width, height, strength, rate): the sequence's frame at the
//    game time × rate (with rate 0: ⌊(count − 1) · strength + 0.5⌋ wrapped by subtraction), then the RLE
//    rows of the block (a byte per run: 0x80 transparent skip of n pixels, 0x40 a run of n pixels, else a
//    fill of n pixels by one value; the copy keeps the layout) with the texture's pixel at (x & width mask,
//    y & height mask) of the origin's offset: in 8 bits by the strength the texture alone (≥ 0.9, 0x571ff0),
//    the blend tables 0x6767d0 and 0x6767c0 with the source as row (≥ 0.7, ≥ 0.5) or column (≥ 0.3,
//    ≥ 0.1), else only the table 0x6767d0 on the source (0x572f40); in 16 bits the texture (≥ 1) or a
//    5-6-5 blend with the fade table g_FadeTables + ⌊strength · 255⌉ · 0x400 (0x6b67e8; its last entry the
//    weight), the low bit set (0x5730f0).
// Quirks kept: the run loops and the row loop are do-while loops (a run of 0 pixels runs 2^32 times, a row
// whose runs do not add up to the width never ends); a fill writes one source value for n pixels.
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_WARP = 0x6027e4, VT_SIMPLE = 0x60049c;
constexpr DWORD WARP_VAR = 0x713c50, SIMPLE_VAR = 0x713574;
constexpr DWORD SEQUENCES = 0x6c2788, REPORTER = 0x6c02cc;
using LD = long double;

void warp_construct(BYTE *o)
{
    zax::fn<void *(THISCALL *)(void *)>(0x55e270)(o);
    dw(o, 0) = vt(VT_WARP);
    dw(o, 0xc) = 0;
    obj_init_fields(o);
}
void warp_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_WARP));
    zax::fn<void(THISCALL *)(void *)>(0x55e2c0)(o);
}
void simple_construct(BYTE *o)
{
    zax::fn<void *(THISCALL *)(void *)>(0x596490)(o);
    vtable_and_defaults(o, vt(VT_SIMPLE));
    for (DWORD off = 0x10; off <= 0x20; off += 4)
        dw(o, off) = 0;
    o[0x24] = 0;
    dw(o, 0x28) = 0;
    dw(o, 8) = 0;
}
void simple_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SIMPLE));
    zax::fn<void(THISCALL *)(void *)>(0x5964e0)(o);
}

double game_time() { return *reinterpret_cast<const double *>(bp(*glob<DWORD *>(REPORTER)) + 0x40); }

// The texture's frame for the strength (rate 0) or the game time × rate; null when the sequence has none
BYTE *texture_frame(void *seq, float strength, float rate)
{
    const DWORD n = vcall<DWORD>(seq, 25);
    if (!n)
        return nullptr;
    DWORD k;
    if (c3(fld(rate), cst(glob_addr(C_ZERO)))) {
        k = (DWORD)x87_ftol(fild64((long long)(n - 1)) * fld(strength) + (LD)*glob<const double *>(0x5ee658));
        while (k >= n)
            k -= n;
    } else
        k = (DWORD)x87_fistp(fld(fstp((LD)game_time() * fld(rate)))) % n;
    return vcall<BYTE *>(seq, 23, k);
}

// The texture's lookup: masks, row table and pixels (slots 41, 42, 46, 31, in this order)
struct Texture {
    DWORD wmask, hmask;
    const DWORD *rows;
    BYTE *pixels;
};
Texture texture(void *f)
{
    Texture t;
    t.wmask = (1u << (vcall<DWORD>(f, 41) & 0x1f)) - 1;
    t.hmask = (1u << (vcall<DWORD>(f, 42) & 0x1f)) - 1;
    t.rows = vcall<const DWORD *>(f, 46);
    t.pixels = vcall<BYTE *>(f, 31);
    return t;
}

// The RLE walk over the rows of a block (8 bits): FILL(dst, src, x) for a fill (one value), RUN(dst, src, x)
// for each pixel of a run; x is the texture column (the origin's x plus the pixels so far)
template <typename Op>
void walk8(const BYTE *src, const int *origin, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, int y0, Op op)
{
    if (!height)
        return;
    int y = y0;
    for (DWORD r = height; r; r--, rows++, y++) {
        const BYTE *s = src + *rows;
        BYTE *d = dst + *rows;
        DWORD remain = width;
        DWORD x = (DWORD)-origin[0];
        do {
            const BYTE c = *s++;
            *d++ = c;
            if (c & 0x80) {
                const DWORD n = c & 0x7f;
                remain -= n;
                x += n;
            } else if (c & 0x40) {
                const DWORD n = c & 0x3f;
                DWORD j = 0, k = n;
                do {
                    d[j] = op(s[j], x + j, (DWORD)y);
                    j++;
                } while (--k);
                s += n;
                d += n;
                remain -= n;
                x += n;
            } else {
                const DWORD n = c & 0x3f;
                *d++ = op(*s++, x, (DWORD)y);
                remain -= n;
                x += n;
            }
        } while (remain);
    }
}
template <typename Op>
void walk16(const BYTE *src, const int *origin, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, Op op)
{
    if (!height)
        return;
    DWORD y = (DWORD)-origin[1];
    for (DWORD r = height; r; r--, rows++, y++) {
        const BYTE *s = src + *rows;
        BYTE *d = dst + *rows;
        DWORD remain = width;
        DWORD x = (DWORD)-origin[0];
        do {
            const BYTE c = *s++;
            *d++ = c;
            if (c & 0x80) {
                const DWORD n = c & 0x7f;
                remain -= n;
                x += n;
            } else if (c & 0x40) {
                const DWORD n = c & 0x3f;
                DWORD j = 0, k = n;
                do {
                    reinterpret_cast<WORD *>(d)[j] = op(reinterpret_cast<const WORD *>(s)[j], x + j, y);
                    j++;
                } while (--k);
                s += n * 2;
                d += n * 2;
                remain -= n;
                x += n;
            } else {
                const DWORD n = c & 0x3f;
                *reinterpret_cast<WORD *>(d) = op(*reinterpret_cast<const WORD *>(s), x, y);
                s += 2;
                d += 2;
                remain -= n;
                x += n;
            }
        } while (remain);
    }
}

// A compressed frame's five blocks in the order the slots take them: the data slot, the row table slot,
// where the frame keeps the pointer and where the effect saves it
struct Block {
    int data, rows;
    DWORD frame_at, saved;
};
constexpr Block BLOCKS[5] = {{31, 46, 0xc, 0x10}, {35, 50, 0x34, 0x20}, {34, 49, 0x2c, 0x1c}, {33, 48, 0x24, 0x18}, {32, 47, 0x1c, 0x14}};

inline const BYTE *table(DWORD at_address) { return *at<const BYTE **>(at_address); }

// The 5-6-5 blend of the source pixel s towards the texture pixel w with the weight a (unsigned 32-bit
// arithmetic as the exe's: logical shifts of the signed differences' products)
WORD blend565(DWORD s, DWORD w, DWORD a)
{
    const DWORD a2 = a >> 2, a3 = a2 >> 1;
    const DWORD g = ((((((w >> 5) & 0x3f) - ((s >> 5) & 0x3f)) * a2) >> 1) + (s & 0xffe0)) & 0x7e0;
    const DWORD r = (((((w >> 11) - (s >> 11)) * a3) << 6) + (s & 0xf83f)) & 0xf800;
    const DWORD b = (((w & 0x1f) - (s & 0x1f)) * a3) >> 5;
    return (WORD)(((s & 0x1f) + b + g + r) | 1);
}

}  // namespace

// ---- CWarpEffect -------------------------------------------------------------------------------------------

// 0x5963e0, 0x596430 (slot 2)
CClassInfo *__cdecl Warp_ClassInfo()
{
    return class_getter(WARP_VAR, 0x624610 /* "CWarpEffect" */, 1.0f, getter<0x55e1c0>, 0x10, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x596490 (thiscall)
BYTE *THISCALL Warp_Ctor(BYTE *o)
{
    warp_construct(o);
    return o;
}
// 0x5964b0 (slot 1): no array path
BYTE *THISCALL Warp_DeletingDtor(BYTE *o, DWORD flags)
{
    warp_destroy(o);
    if (flags & 1)
        pool_free(o, 0x10);
    return o;
}
// 0x5964e0
void THISCALL Warp_Dtor(BYTE *o) { warp_destroy(o); }
// 0x596500 (slot 23: packet, a2): CObject's slot 0, the strength · 255 and the rate · 8 as bytes. eax: the
// last write's.
DWORD THISCALL Warp_WriteBlock(BYTE *self, void *packet, DWORD)
{
    obj_on_message(self, packet, 0);
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x520820)(packet, (DWORD)x87_ftol(fld(fl<float>(self, 8)) * cst(glob_addr(0x5eb728))));
    const DWORD v = (DWORD)x87_ftol(vcall<LD>(self, 31) * cst(glob_addr(0x5feb58)));
    return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x520820)(packet, (ptr_value(packet) & 0xffffff00) | (v & 0xff));
}
// 0x596550 (slot 24: packet): the two bytes back (· 1/255, · 0.125)
void THISCALL Warp_ReadBlock(BYTE *self, void *packet)
{
    zax::fn<void(THISCALL *)(void *, void *)>(0x4186e0)(self, packet);
    DWORD a = ptr_value(packet), b;
    zax::fn<DWORD(THISCALL *)(void *, BYTE *)>(0x520490)(packet, reinterpret_cast<BYTE *>(&a));
    fl<float>(self, 8) = fstp(fild((int)(a & 0xff)) * cst(glob_addr(0x5f172c)));
    zax::fn<DWORD(THISCALL *)(void *, BYTE *)>(0x520490)(packet, reinterpret_cast<BYTE *>(&b));
    fl<float>(self, 0xc) = fstp(fild((int)(b & 0xff)) * cst(glob_addr(0x5f08c0)));
}
static BYTE *entity_frame(BYTE *entity)
{
    BYTE *f = zax::fn<BYTE *(THISCALL *)(void *)>(0x482de0)(entity + 0x34);
    if (f && !is_kind_of(f, zax::fn<Getter>(0x4a8760)()))
        f = nullptr;
    return f;
}
// 0x5965d0 (slot 25: entity): begin and draw on the entity's frame
void THISCALL Warp_Begin(BYTE *self, BYTE *entity)
{
    BYTE *f = entity_frame(entity);
    vcall<void>(self, 28, f);
    vcall<void>(self, 29, f);
}
// 0x596610 (slot 26: entity): end
void THISCALL Warp_End(BYTE *self, BYTE *entity) { vcall<void>(self, 30, entity_frame(entity)); }
// 0x596650 (slot 27): nothing (six arguments)
void THISCALL Warp_Slot27(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) {}
// 0x5965c0 (slot 31): the rate
LD THISCALL Warp_Rate(BYTE *self) { return fld(fl<float>(self, 0xc)); }

// ---- CSimpleWarpEffect -----------------------------------------------------------------------------------------

// 0x5718d0, 0x571a70 (slot 2)
CClassInfo *__cdecl SimpleWarp_ClassInfo()
{
    return class_getter(SIMPLE_VAR, 0x621b00 /* "CSimpleWarpEffect" */, 1.0f, Warp_ClassInfo, 0x30, 0, 0x571920, 0x571970, 0x571ad0);
}
// 0x571920
void *__cdecl SimpleWarp_Create() { return create_one<0x30, simple_construct>(); }
// 0x571970 (count in ecx)
void *__fastcall SimpleWarp_CreateArray(int count, int, int) { return create_array<0x30, simple_construct>(count); }
// 0x5719f0 (slot 1)
void *THISCALL SimpleWarp_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, simple_destroy>(self, flags); }
// 0x571b70, 0x571bb0: the constructor and destructor out of line
BYTE *THISCALL SimpleWarp_Ctor(BYTE *o)
{
    simple_construct(o);
    return o;
}
void THISCALL SimpleWarp_Dtor(BYTE *o) { simple_destroy(o); }
// 0x571ad0: "Warp Effect" (the sequence list)
DWORD __cdecl SimpleWarp_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x621b14) /* "Warp Effect" */, 0x2c, 4, 0x2c, glob_addr(SEQUENCES), 1, 0, 0, 0);
    return add_field(SimpleWarp_ClassInfo(), f);
}
// 0x571b60 (session): the block reader 0x713568 registered ("CSimpleWarpEffectBlockReaderObject")
DWORD THISCALL SimpleWarp_RegisterReader(void *session)
{
    return zax::fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(session, glob<void *>(0x713568), glob<const char *>(0x621b20));
}
// 0x571bd0 (slot 23: packet, with the block type): CWarpEffect's block and the sequence as a token
DWORD THISCALL SimpleWarp_WriteBlock(BYTE *self, void *packet, BYTE with_type)
{
    if (with_type)
        zax::fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(packet, glob<void *>(0x713568), 2);
    const DWORD r = zax::fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x596500)(self, packet, 0);
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x520c40)(packet, (r & 0xffffff00) | self[0x2c], glob<void *>(SEQUENCES));
}
// 0x571c10 (slot 24: packet)
void THISCALL SimpleWarp_ReadBlock(BYTE *self, void *packet)
{
    zax::fn<void(THISCALL *)(void *, void *)>(0x596550)(self, packet);
    DWORD b = ptr_value(packet);
    zax::fn<DWORD(THISCALL *)(void *, BYTE *, void *)>(0x520620)(packet, reinterpret_cast<BYTE *>(&b), glob<void *>(SEQUENCES));
    dw(self, 0x2c) = b & 0xff;
}

// 0x571ff0 (dst; count, texture row, x, mask): count texture pixels (do-while)
void THISCALL Warp_CopyRow(BYTE *dst, DWORD count, const BYTE *row, DWORD x, DWORD mask)
{
    do {
        *dst++ = row[x & mask];
        x++;
    } while (--count);
}

// The 8-bit loops (fastcall block, origin; row table, copy, width, height, sequence, strength, rate)
#define WARP8(fn, body)                                                                                                           \
    void __fastcall fn(const BYTE *src, const int *origin, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, void *seq,    \
                       float strength, float rate)                                                                                \
    {                                                                                                                             \
        BYTE *f = texture_frame(seq, strength, rate);                                                                             \
        if (!f)                                                                                                                   \
            return;                                                                                                               \
        const Texture t = texture(f);                                                                                             \
        walk8(src, origin, rows, dst, width, height, -origin[1], [&](BYTE s, DWORD x, DWORD y) -> BYTE {                          \
            const BYTE w = t.pixels[t.rows[y & t.hmask] + (x & t.wmask)];                                                         \
            (void)s;                                                                                                              \
            (void)w;                                                                                                              \
            return body;                                                                                                          \
        });                                                                                                                       \
    }
WARP8(Warp8_Row7d0, table(glob_addr(0x6767d0))[s * 0x100 + w])   // 0x5726c0 (strength ≥ 0.7)
WARP8(Warp8_Row7c0, table(glob_addr(0x6767c0))[s * 0x100 + w])   // 0x5728e0 (≥ 0.5)
WARP8(Warp8_Col7c0, table(glob_addr(0x6767c0))[w * 0x100 + s])   // 0x572b00 (≥ 0.3)
WARP8(Warp8_Col7d0, table(glob_addr(0x6767d0))[w * 0x100 + s])   // 0x572d20 (≥ 0.1)
WARP8(Warp8_Fade, table(glob_addr(0x6767d0))[s])                 // 0x572f40 (below)
#undef WARP8

// 0x5730f0 (16 bits, strength below 1): the blend with the fade table's weight
void __fastcall Warp16_Blend(const BYTE *src, const int *origin, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, void *seq, float strength,
                             float rate)
{
    BYTE *f = texture_frame(seq, strength, rate);
    if (!f)
        return;
    const Texture t = texture(f);
    walk16(src, origin, rows, dst, width, height, [&](WORD s, DWORD x, DWORD y) -> WORD {
        const WORD w = reinterpret_cast<const WORD *>(t.pixels)[t.rows[y & t.hmask] + (x & t.wmask)];
        return blend565(s, w, dw(*glob<BYTE **>(0x6b67e8), 0x3fc));
    });
}
// 0x573440 (16 bits, strength 1 and above): the texture
void __fastcall Warp16_Copy(const BYTE *src, const int *origin, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, void *seq, float strength,
                            float rate)
{
    BYTE *f = texture_frame(seq, strength, rate);
    if (!f)
        return;
    const Texture t = texture(f);
    walk16(src, origin, rows, dst, width, height, [&](WORD, DWORD x, DWORD y) -> WORD {
        return reinterpret_cast<const WORD *>(t.pixels)[t.rows[y & t.hmask] + (x & t.wmask)];
    });
}

// 0x571c50 (block, origin, row table, copy, width, height, strength, rate): by the display's depth and the
// strength (see the head comment)
void THISCALL SimpleWarp_Bend(BYTE *self, const BYTE *src, const int *origin, const DWORD *rows, BYTE *dst, DWORD width, DWORD height, float strength,
                              float rate)
{
    if (!src)
        return;
    void *seq = zax::fn<void *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(SEQUENCES), dw(self, 0x2c));
    using Loop = void(__fastcall *)(const BYTE *, const int *, const DWORD *, BYTE *, DWORD, DWORD, void *, float, float);
    const DWORD depth = dw(bp(*glob<DWORD *>(REPORTER)), 0x24);
    if (depth == 8) {
        if (c0(fld(strength), cst(glob_addr(0x5fc7ec)))) {
            const DWORD loop = !c0(fld(strength), cst(glob_addr(0x600520)))   ? fn_addr(0x5726c0)
                               : !c0(fld(strength), cst(glob_addr(0x5eee2c))) ? fn_addr(0x5728e0)
                               : !c0(fld(strength), cst(glob_addr(0x60051c))) ? fn_addr(0x572b00)
                               : !c0(fld(strength), cst(glob_addr(0x5f14ec))) ? fn_addr(0x572d20)
                                                                              : fn_addr(0x572f40);
            fn_dyn<Loop>(loop)(src, origin, rows, dst, width, height, seq, strength, rate);
            return;
        }
        BYTE *f = texture_frame(seq, strength, rate);
        if (!f)
            return;
        const Texture t = texture(f);
        if (!height)
            return;
        DWORD y = (DWORD)-origin[1];
        for (DWORD r = height; r; r--, rows++, y++) {
            const BYTE *s = src + *rows;
            BYTE *d = dst + *rows;
            DWORD remain = width;
            DWORD x = (DWORD)-origin[0];
            const BYTE *row = t.pixels + t.rows[y & t.hmask];
            do {
                const BYTE c = *s++;
                *d++ = c;
                if (c & 0x80) {
                    const DWORD n = c & 0x7f;
                    remain -= n;
                    x += n;
                } else if (c & 0x40) {
                    const DWORD n = c & 0x3f;
                    zax::fn<void(THISCALL *)(BYTE *, DWORD, const BYTE *, DWORD, DWORD)>(0x571ff0)(d, n, row, x, t.wmask);
                    s += n;
                    d += n;
                    remain -= n;
                    x += n;
                } else {
                    const DWORD n = c & 0x3f;
                    zax::fn<void(THISCALL *)(BYTE *, DWORD, const BYTE *, DWORD, DWORD)>(0x571ff0)(d, 1, row, x, t.wmask);
                    s++;
                    d++;
                    remain -= n;
                    x += n;
                }
            } while (remain);
        }
    } else if (depth == 16) {
        if (c0(fld(strength), cst(glob_addr(0x5ec3f4)))) {
            *glob<DWORD *>(0x6b67e8) = glob_addr(0x6347b0) + ((DWORD)x87_fistp(fld(fstp(fld(strength) * cst(glob_addr(0x5eb728))))) << 10);
            zax::fn<Loop>(0x5730f0)(src, origin, rows, dst, width, height, seq, strength, rate);
        } else
            zax::fn<Loop>(0x573440)(src, origin, rows, dst, width, height, seq, strength, rate);
    }
}

// 0x572020 (slot 28: frame): begin (see the head comment)
void THISCALL SimpleWarp_BeginFrame(BYTE *self, BYTE *f)
{
    BYTE *p = vcall<BYTE *>(f, 31);
    dw(self, 0x10) = vcall<DWORD>(f, 31);
    dw(self, 0x14) = vcall<DWORD>(f, 32);
    dw(self, 0x18) = vcall<DWORD>(f, 33);
    dw(self, 0x1c) = vcall<DWORD>(f, 34);
    dw(self, 0x20) = vcall<DWORD>(f, 35);
    const DWORD w = vcall<DWORD>(f, 45);
    const DWORD h = vcall<DWORD>(f, 39);
    if (!p)
        return;
    const DWORD kind = vcall<DWORD>(f, 38) & 6;
    if (kind == 0) {
        dw(self, 0x10) = vcall<DWORD>(f, 31);
        dw(self, 0x28) = h * w;
        vcall<DWORD>(f, 43);
        vcall<DWORD>(f, 44);
        dw(f, 0xc) = ptr_value(zax::fn<void *(__fastcall *)(DWORD)>(0x55e2f0)(dw(self, 0x28)));
    } else if (kind == 2 || kind == 4) {
        const int nblocks = kind == 2 ? 5 : 2;
        dw(self, 0x28) = 0;
        for (int i = 0; i < nblocks; i++)
            if (vcall<DWORD *>(f, BLOCKS[i].data))
                dw(self, 0x28) += *vcall<DWORD *>(f, BLOCKS[i].data);
        BYTE *cur = static_cast<BYTE *>(zax::fn<void *(__fastcall *)(DWORD)>(0x55e2f0)(dw(self, 0x28)));
        DWORD at_[5] = {};
        for (int i = 0; i < nblocks; i++) {
            const DWORD *b = vcall<DWORD *>(f, BLOCKS[i].data);
            if (!b) {
                at_[i] = 0;
                continue;
            }
            dw(cur, 0) = *b;
            at_[i] = ptr_value(cur);
            if (i + 1 < 5)
                cur += *b;
        }
        for (int i = 0; i < nblocks; i++)
            dw(f, BLOCKS[i].frame_at) = at_[i];
    }
    self[0x24] = 1;
}

// 0x572300 (slot 29: frame): draw (see the head comment)
void THISCALL SimpleWarp_DrawFrame(BYTE *self, BYTE *f)
{
    const DWORD w = vcall<DWORD>(f, 45);
    const DWORD h = vcall<DWORD>(f, 39);
    const float rate = fstp(vcall<LD>(self, 31));
    const float strength = fl<float>(self, 8);
    const DWORD kind = vcall<DWORD>(f, 38) & 6;
    if (kind == 0) {
        BYTE *d = bp(dw(self, 0x10));
        const BYTE *s = vcall<const BYTE *>(f, 31);
        if (!s || !h)
            return;
        for (DWORD r = h; r; r--) {
            if (!w)
                continue;
            for (DWORD k = w; k; k--)
                *d++ = table(glob_addr(0x6767c0))[(*glob<DWORD *>(0x713570) & 0xff) * 0x100 + *s++];
        }
        return;
    }
    if (kind != 2 && kind != 4)
        return;
    int origin[2];
    origin[0] = -vcall<int>(f, 43);
    origin[1] = -vcall<int>(f, 44);
    const int nblocks = kind == 2 ? 5 : 2;
    for (int i = 0; i < nblocks; i++) {
        BYTE *copy = vcall<BYTE *>(f, BLOCKS[i].data);
        const DWORD *rows = vcall<const DWORD *>(f, BLOCKS[i].rows);
        zax::fn<void(THISCALL *)(void *, const BYTE *, const int *, const DWORD *, BYTE *, DWORD, DWORD, float, float)>(0x571c50)(
            self, bp(dw(self, BLOCKS[i].saved)), origin, rows, copy, w, h, strength, rate);
    }
}

// 0x572680 (slot 30: frame): end
void THISCALL SimpleWarp_EndFrame(BYTE *self, BYTE *f)
{
    zax::fn<void(__cdecl *)()>(0x55e2e0)();
    dw(f, 0xc) = dw(self, 0x10);
    dw(f, 0x1c) = dw(self, 0x14);
    dw(f, 0x24) = dw(self, 0x18);
    dw(f, 0x2c) = dw(self, 0x1c);
    dw(f, 0x34) = dw(self, 0x20);
    self[0x24] = 0;
}

// 0x572570 (slot 31): the sequence's rate (slot 26), 0 without it
LD THISCALL SimpleWarp_Rate(BYTE *self)
{
    if (!*glob<BYTE **>(0x6c27b8))
        return cst(glob_addr(C_ZERO));
    BYTE *seq = list_object(glob_addr(SEQUENCES), glob_addr(0x6c27b8), glob_addr(0x6c27bc), glob_addr(0x6c27c0), dw(self, 0x2c));
    if (!seq)
        return cst(glob_addr(C_ZERO));
    return vcall<LD>(seq, 26);
}

ZAX_HOOKS(simple_warp)
{
    const Hook list[] = {
#include "simple_warp_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
