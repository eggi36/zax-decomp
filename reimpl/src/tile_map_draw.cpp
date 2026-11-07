// The drawing of CPlasmaTileMap, the Plasma Ground (docs/DECOMPILATION.md 10, "Batch C5"; the rest of the class in
// plasma_tile_map.cpp, its fields in plasma_tile_map.h): 0x53ff90 and 0x540000 draw the cells of a region, 0x540b80
// gets a cell's tile of one layer from the tile cache (building it when the cache has none), 0x540840 draws the
// ground of a cell, 0x540790 prepares a frame on a tile cache handle, 0x540d90 builds a tile (the plasma fractal
// between the cell's four grid points), 0x541250 maps the band indices of a built tile to texture pixels (8, 16 or
// 24 bits), 0x541090 the grid lines of the editor's grid mode and 0x541710 the band table. Every call goes through
// its exe address.
//
// [confirmed: code; VM]
//  - 0x53ff90 (frame, rectangle or null): clears the counters +0x2d2c-+0x2d34, counts +0x2cf8 down (at 0 the tile
//    cache is emptied, HandleHeap::Reset 0x4d2b50), then draws the region's border cells first (0x540000 with the
//    frame's clip rectangle (slot 36) and "all cells") and, given a rectangle, its cells again (border only, at most
//    three tiles built: +0x2d2c).
//  - 0x540000 (frame, rectangle, all): the static frame 0x711078 (guard 0x711070 bit 1, atexit 0x540410; pitch
//    and the words +0x12/+0x14 the cell size + 1, the screen's colour depth, once: 0x7110e4); the cells of the
//    rectangle (signed divisions, clamped to the grid minus one); per cell the layer of +8 (slot 54: 1 or 2) is
//    built or taken (0x540b80) and, drawing all, blitted (frame slot 23, mode 0x6bb4e0); the tile's data is unlocked
//    (0x4d2bc0, 0x711084); drawing all also the grid (0x541090) and, with +0x60, a cross and the rectangle of the
//    cells +8 marks in the layer +0x2d44 (static colour 0x7110c8, guard bit 2, atexit 0x540400).
//  - 0x540840 (frame, column, row): an 8-bit frame whose four corners share one texture without a blend table
//    gets the texture blitted (slot 23, mode 0x6bc4e0) at the cell's position (columns and rows by the shifts
//    +0x2d1c/+0x2d20); otherwise the cell's ground tile of the cache (layer 0) is prepared on the static frame
//    0x6d0ff0 (guard 0x711074, atexit 0x540b70), built when new (0x540d90) and blitted (0x4b3280, mode 0x6bb4e0).
//  - 0x540b80 (column, row, frame, layer 1 or 2): the frame on the handle of the layer; when it was new (+0x2d2c
//    counted): layer 1 draws the ground (0x540840) and the layer's tile once (0x4e8ce0); layer 2 counts +0x2d34
//    and draws the ground or blits the layer 1 tile (static frame 0x711030, guard 0x711071, atexit 0x540d80),
//    then the layer's tile (0x4e8b60; 1 when drawn new).
//  - 0x540d90 (column, row, frame): counts +0x2d30; a frame of another depth than 8 bits draws into the static
//    8-bit frame 0x6d0ba8 (guard 0x711072, atexit 0x541080; sized once); the four elevations at the corners, then
//    the plasma: the random generator 0x6d0be8 seeded from the plasma tables 0x6d1030 by the corners' seeds (+0x55,
//    unchanged, unchanged, +0x55, -0x56), Plasma_Line 0x49f9d0 on the top and bottom rows, Plasma_Column 0x49fe10
//    on the left and right columns, Plasma_Map 0x49fad0 inside; then 0x541250.
//  - 0x541250 (frame, band frame, x, y, width, height): per pixel the band's texture pixel (row (y + j) & 0x7f of
//    the textures, x & 0x7f on), mixed with the band's second texture: 8 bits through the band's table (+0x864),
//    16 bits 5-6-5 by the weight +0xc64 (red and blue by weight >> 3, green by weight >> 2 halved), 24 bits per
//    byte (weight / 256, truncated).
//  - 0x541710 (mode: 0 empties the cache, 1 sets the countdown +0x2cf8 to 5): per band (256) the position in the
//    texture list (band · count · 0x5fce00), its fraction weighted by "Blending" (+0x1068), the two textures and
//    in 8 bits a blend table (0x6767c0/0x6767d0 by the weight's thresholds 0x5fcdf8, 0x5eee10, 0x5eee14,
//    0x5eb2d0, 1), else the weight · 0x5eb728 in +0xc64; then the masks of the textures' sizes (slots 41, 42) and
//    their data (slot 31).
//  - 0x541090 (column, row, frame, rectangle): grid mode +0x2cec 1: two lines of the cell in the static colour
//    0x7110b8 (guard 0x711073, atexit 0x541240) and a small cross at the selected point +0x2cf0/+0x2cf4 in
//    0x6be378; mode 2: the lines in grey, white at the selected point.
#include "ui_screens.h"
#include "x87.h"

namespace zax::tilemap {

using namespace ui;
using LD = long double;

constexpr DWORD TILE_CACHE = 0x7110e0, FRAME_DRAW = 0x711078, FRAME_GROUND = 0x6d0ff0, FRAME_LAYER1 = 0x711030,
                FRAME_BANDS = 0x6d0ba8, RNG = 0x6d0be8, PLASMA_TABLES = 0x6d1030, MODE_COPY = 0x6bb4e0;

inline DWORD &u(BYTE *p, DWORD off) { return m(p, off); }
template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
inline DWORD cw(BYTE *t) { return m(t, 0x2d04); }
inline DWORD ch(BYTE *t) { return m(t, 0x2d08); }
inline DWORD elevation(BYTE *t, DWORD x, DWORD y) { return vc(t + 0x2c6c, 54, x, y); }
inline DWORD seed(BYTE *t, DWORD x, DWORD y) { return vc(t + 0x2cac, 54, x, y); }
inline BYTE *entry(BYTE *t, DWORD band) { return t + 0x106c + band * 0x1c; }
inline DWORD *handle(BYTE *t, DWORD layer, DWORD col, DWORD row) { return reinterpret_cast<DWORD *>(m(t, 0x2d24)) + (layer * m(t, 0x2d10) + row) * m(t, 0x2d0c) + col; }
inline void frame_ctor(DWORD f) { fn<void *(THISCALL *)(DWORD)>(0x4a8a90)(f); }
inline void set_pitch(DWORD f, DWORD pitch, DWORD a) { fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x4aa2c0)(f, pitch, a); }
inline void set_depth(DWORD f, DWORD bits) { fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4aa240)(f, bits); }
inline DWORD colour_depth() { return fn<DWORD(__cdecl *)()>(0x55d030)(); }
inline DWORD heap_lock(DWORD *h) { return fn<DWORD(THISCALL *)(DWORD *)>(0x4d2760)(h); }
inline void heap_unlock(DWORD *h) { fn<DWORD(THISCALL *)(DWORD *)>(0x4d27a0)(h); }
inline void blit_at(DWORD dst, DWORD src) { fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(dst, src, 0, 0, glob_addr(MODE_COPY)); }
inline void set_byte(DWORD f, DWORD x, DWORD y, DWORD v) { fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x4a8b60)(f, x, y, v); }
inline void rng_assign(DWORD table) { fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x55c430)(glob_addr(RNG), table); }
inline DWORD plasma_table(DWORD v) { return glob_addr(PLASMA_TABLES) + ((v & 0xff) << 10); }
inline void plasma_line(DWORD p, DWORD n) { fn<void(__fastcall *)(DWORD, DWORD, DWORD)>(0x49f9d0)(p, n, glob_addr(RNG)); }
inline void plasma_column(DWORD f, DWORD x, DWORD n) { fn<void(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x49fe10)(f, x, n, glob_addr(RNG)); }
// A function-local static of the exe built once (guard bit): a frame or a colour, its destructor with atexit
inline bool first_time(DWORD guard, BYTE bit)
{
    if (gb(guard) & bit)
        return false;
    gb(guard) |= bit;
    return true;
}
inline void static_frame(DWORD guard, BYTE bit, DWORD frame, DWORD dtor)
{
    if (!first_time(guard, bit))
        return;
    frame_ctor(frame);
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(dtor));
}
inline void static_colour(DWORD guard, BYTE bit, DWORD colour, DWORD r, DWORD g, DWORD b, DWORD dtor)
{
    if (!first_time(guard, bit))
        return;
    fn<void *(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, r, g, b, 0xff);
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(dtor));
}

}  // namespace zax::tilemap

namespace zax {

using namespace tilemap;

namespace {

// 0x540790 (frame, handle): the frame (cell size + 1, the screen's depth) on the handle's memory, the handle
// allocated first when empty (frame slots 45 · 39 · 30 bytes). al: the handle had memory.
DWORD THISCALL Tile_PrepareFrame(BYTE *t, BYTE *f, DWORD *h)
{
    fl<WORD>(f, 0x12) = (WORD)(cw(t) + 1);
    fl<WORD>(f, 0x14) = (WORD)(ch(t) + 1);
    if (vc(f, 46))
        set_pitch(ptrv(f), cw(t) + 1, 0);
    else
        set_pitch(ptrv(f), cw(t) + 1, 1);
    set_depth(ptrv(f), colour_depth());
    DWORD size = vc(f, 45);
    size *= vc(f, 39);
    size *= vc(f, 30);
    BYTE had = 1;
    if (!*h) {
        had = 0;
        fn<DWORD(THISCALL *)(DWORD *, DWORD, DWORD, DWORD)>(0x4d2680)(h, size, gd(glob_addr(TILE_CACHE)), 1);
    }
    m(f, 0xc) = heap_lock(h);
    return had;
}

void THISCALL Tile_Build(BYTE *t, DWORD col, DWORD row, BYTE *f);

// 0x540840 (frame, column, row)
void THISCALL Tile_DrawGround(BYTE *t, BYTE *f, DWORD col, DWORD row)
{
    if (vc(f, 29) == 8) {
        BYTE *band = t + 0x106c;
        auto blend_of = [&](DWORD x, DWORD y) { return m(t, (elevation(t, x, y) + 0x97) * 0x1c); };
        auto tex_of = [&](DWORD x, DWORD y) { return m(band, (elevation(t, x, y) & 0xff) * 0x1c); };
        if (!blend_of(col, row) && !blend_of(col + 1, row) && !blend_of(col, row + 1) && !blend_of(col + 1, row + 1)) {
            const DWORD a = tex_of(col, row), b = tex_of(col + 1, row);
            if (a == b) {
                const DWORD c = tex_of(col + 1, row), d = tex_of(col, row + 1);
                if (c == d) {
                    const DWORD e = tex_of(col, row + 1), g = tex_of(col + 1, row + 1);
                    if (e == g) {
                        const DWORD tex = tex_of(col, row);
                        if (tex) {
                            const int w1 = (int)vc(f, 39) - 1;
                            const int h1 = (int)vc(f, 45) - 1;
                            const int y = (int)(row << m(t, 0x2d20)), x = (int)(col << m(t, 0x2d1c));
                            int src[4] = {x, y, h1 + x, w1 + y};
                            int dst[4] = {0, 0, h1, w1};
                            vc(f, 23, tex, src, dst, glob_addr(0x6bc4e0));
                            return;
                        }
                    }
                }
            }
        }
    }
    DWORD *h = handle(t, 0, col, row);
    static_frame(glob_addr(0x711074), 1, glob_addr(FRAME_GROUND), 0x540b70);
    if (!(BYTE)Tile_PrepareFrame(t, glob<BYTE *>(FRAME_GROUND), h))
        Tile_Build(t, col, row, glob<BYTE *>(FRAME_GROUND));
    blit_at(ptrv(f), glob_addr(FRAME_GROUND));
    heap_unlock(h);
    gd(glob_addr(FRAME_GROUND) + 0xc) = 0;
}

// 0x540b80 (column, row, frame, layer)
void THISCALL Tile_DrawLayer(BYTE *t, DWORD col, DWORD row, BYTE *f, DWORD layer)
{
    DWORD *h = handle(t, layer, col, row);
    BYTE had = 1;
    if (!*h) {
        had = 0;
        DWORD size = vc(f, 45);
        size *= vc(f, 39);
        size *= vc(f, 30);
        fn<DWORD(THISCALL *)(DWORD *, DWORD, DWORD, DWORD)>(0x4d2680)(h, size, gd(glob_addr(TILE_CACHE)), 1);
    }
    m(f, 0xc) = heap_lock(h);
    if (had)
        return;
    u(t, 0x2d2c)++;
    BYTE *lay = bp(m(t, 0x2d44)) + 0x3c;
    if (layer == 1) {
        Tile_DrawGround(t, f, col, row);
        fn<DWORD(THISCALL *)(void *, BYTE *, DWORD, DWORD)>(0x4e8ce0)(lay, f, cw(t) * col, ch(t) * row);
    } else if (layer == 2) {
        DWORD *h1 = handle(t, 1, col, row);
        if (*h1) {
            u(t, 0x2d34)++;
            static_frame(glob_addr(0x711071), 1, glob_addr(FRAME_LAYER1), 0x540d80);
            Tile_PrepareFrame(t, glob<BYTE *>(FRAME_LAYER1), h1);
            blit_at(ptrv(f), glob_addr(FRAME_LAYER1));
            heap_unlock(h1);
            gd(glob_addr(FRAME_LAYER1) + 0xc) = 0;
            fn<DWORD(THISCALL *)(void *, BYTE *, DWORD, DWORD, DWORD)>(0x4e8b60)(lay, f, cw(t) * col, ch(t) * row, 0);
        } else {
            u(t, 0x2d34)++;
            Tile_DrawGround(t, f, col, row);
            fn<DWORD(THISCALL *)(void *, BYTE *, DWORD, DWORD, DWORD)>(0x4e8b60)(lay, f, cw(t) * col, ch(t) * row, 1);
        }
    }
}

// 0x541250 (frame, band frame, x, y, width, height)
void THISCALL Tile_MapBands(BYTE *t, BYTE *f, BYTE *bands, DWORD x, DWORD y, DWORD w, DWORD h)
{
    if (!m(bp(m(t, 0x1064)), 0x34))
        return;
    x &= 0x7f;
    const DWORD *tex_rows = reinterpret_cast<const DWORD *>(vc(bp(m(t, 0x106c)), 46));
    const DWORD depth = vc(f, 29);
    DWORD *base = &m(t, 0x64), *second = &m(t, 0x464), *table = &m(t, 0x864), *weight = &m(t, 0xc64);
    if (depth == 0x18) {
        for (DWORD j = 0; j < h; j++) {
            DWORD off = tex_rows[(j + y) & 0x7f] + x;
            const DWORD *brows = reinterpret_cast<const DWORD *>(vc(bands, 46));
            BYTE *src = bp(brows[j] + vc(bands, 31));
            const DWORD *frows = reinterpret_cast<const DWORD *>(vc(f, 46));
            BYTE *dst = bp(frows[j] * 3 + vc(f, 31));
            BYTE *end = src + w;
            off *= 3;
            do {
                const DWORD b = *src;
                if (const DWORD s = second[b]) {
                    const BYTE *c2 = bp(s + off), *c1 = bp(base[b] + off);
                    const DWORD k = weight[b];
                    // (c2 - c1) · k / 256 per byte, signed and truncated (cdq, and 0xff, sar 8), the bytes added up
                    auto part = [k](DWORD a, DWORD b_) { return (DWORD)((int)((a - b_) * k) / 256); };
                    const DWORD hi = (c1[2] + part(c2[2], c1[2])) << 8;
                    const DWORD mid = (part(c2[1], c1[1]) + c1[1] + hi) << 8;
                    const DWORD v = part(c2[0], c1[0]) + c1[0] + mid;
                    fl<WORD>(dst, 0) = (WORD)v;
                    dst[2] = (BYTE)(v >> 16);
                } else {
                    const BYTE *c1 = bp(base[b] + off);
                    fl<WORD>(dst, 0) = fl<WORD>(const_cast<BYTE *>(c1), 0);
                    dst[2] = c1[2];
                }
                off += 3;
                src++;
                dst += 3;
            } while (src < end);
        }
    } else if (depth == 0x10) {
        for (DWORD j = 0; j < h; j++) {
            const DWORD texoff = tex_rows[(j + y) & 0x7f] + x;
            const DWORD *brows = reinterpret_cast<const DWORD *>(vc(bands, 46));
            BYTE *src = bp(brows[j] + vc(bands, 31));
            const DWORD *frows = reinterpret_cast<const DWORD *>(vc(f, 46));
            const DWORD row = frows[j];
            WORD *dst = reinterpret_cast<WORD *>(vc(f, 31) + row * 2);
            BYTE *end = src + w;
            DWORD off = texoff * 2;
            do {
                const DWORD b = *src;
                if (const DWORD s = second[b]) {
                    const DWORD c2 = fl<WORD>(bp(s), off), c1 = fl<WORD>(bp(base[b]), off);
                    DWORD k = weight[b] >> 2;
                    DWORD green = ((((c2 >> 5) & 0x3f) - ((c1 >> 5) & 0x3f)) * k) >> 1;
                    green = (green + (c1 & 0xffe0)) & 0x7e0;
                    k >>= 1;
                    // the red difference with the upper half of the destination pointer above it (masked out below)
                    const DWORD red = (((((DWORD)ptrv(dst) & 0xffff0000u) | (c2 >> 11)) - (c1 >> 11)) * k << 6) + (c1 & 0xf83f);
                    const DWORD blue = (((c2 & 0x1f) - (c1 & 0x1f)) * k) >> 5;
                    *dst = (WORD)((c1 & 0x1f) + blue + green + (red & 0xf800));
                } else {
                    *dst = fl<WORD>(bp(base[b]), off);
                }
                off += 2;
                src++;
                dst++;
            } while (src < end);
        }
    } else if (depth == 8) {
        for (DWORD j = 0; j < h; j++) {
            DWORD off = tex_rows[(j + y) & 0x7f] + x;
            const DWORD *brows = reinterpret_cast<const DWORD *>(vc(bands, 46));
            BYTE *src = bp(brows[j] + vc(bands, 31));
            const DWORD *frows = reinterpret_cast<const DWORD *>(vc(f, 46));
            BYTE *dst = bp(frows[j] + vc(f, 31));
            BYTE *end = src + w;
            do {
                const DWORD b = *src;
                if (const DWORD s = second[b])
                    *dst = bp(table[b])[bp(base[b])[off] + ((DWORD)bp(s)[off] << 8)];
                else
                    *dst = bp(base[b])[off];
                off++;
                src++;
                dst++;
            } while (src < end);
        }
    }
}

// 0x540d90 (column, row, frame)
void THISCALL Tile_Build(BYTE *t, DWORD col, DWORD row, BYTE *f)
{
    u(t, 0x2d30)++;
    BYTE *dst = f;
    if (vc(f, 29) != 8) {
        static_frame(glob_addr(0x711072), 1, glob_addr(FRAME_BANDS), 0x541080);
        if (!fl<WORD>(glob<BYTE *>(FRAME_BANDS), 0x12)) {
            const DWORD a = vc(f, 39);
            fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x4ab5a0)(glob_addr(FRAME_BANDS), vc(f, 45), a, 8);
        }
        dst = glob<BYTE *>(FRAME_BANDS);
    }
    vc(dst, 27, 0);
    set_byte(ptrv(dst), 0, 0, elevation(t, col, row));
    set_byte(ptrv(dst), cw(t), 0, elevation(t, col + 1, row));
    const DWORD row1 = row + 1;
    set_byte(ptrv(dst), 0, ch(t), elevation(t, col, row1));
    set_byte(ptrv(dst), cw(t), ch(t), elevation(t, col + 1, row1));
    rng_assign(plasma_table(seed(t, col, row) + 0x55));
    const DWORD *rows = reinterpret_cast<const DWORD *>(vc(dst, 46));
    DWORD n = cw(t);
    plasma_line(vc(dst, 31) + rows[0], n);
    rng_assign(plasma_table(seed(t, col, row)));
    plasma_column(ptrv(dst), 0, ch(t));
    rng_assign(plasma_table(seed(t, col + 1, row)));
    plasma_column(ptrv(dst), cw(t), ch(t));
    rng_assign(plasma_table(seed(t, col, row1) + 0x55));
    n = cw(t);
    rows = reinterpret_cast<const DWORD *>(vc(dst, 46));
    const DWORD last = ch(t);
    plasma_line(rows[last] + vc(dst, 31), n);
    rng_assign(plasma_table(seed(t, col, row) - 0x56));
    fn<void(__fastcall *)(BYTE *, DWORD)>(0x49fad0)(dst, glob_addr(RNG));
    fn<void(THISCALL *)(BYTE *, BYTE *, BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x541250)(t, f, dst, cw(t) * col, ch(t) * row, cw(t), ch(t));
}

// 0x541090 (column, row, frame, rectangle)
void THISCALL Tile_DrawGrid(BYTE *t, DWORD col, DWORD row, BYTE *f, const int *r)
{
    const DWORD mode = m(t, 0x2cec);
    if (mode == 1) {
        static_colour(glob_addr(0x711073), 1, glob_addr(0x7110b8), 0x80, 0x80, 0x80, 0x541240);
        vc(f, 51, r[0], r[1], r[0], r[3], glob_addr(0x7110b8));
        vc(f, 51, r[0], r[1], r[2], r[1], glob_addr(0x7110b8));
        if (m(t, 0x2cf0) == col && m(t, 0x2cf4) == row) {
            vc(f, 51, r[0] - 3, r[1], r[0] + 3, r[1], glob_addr(0x6be378));
            vc(f, 51, r[0], r[1] - 3, r[0], r[1] + 3, glob_addr(0x6be378));
        }
    } else if (mode == 2) {
        BYTE colour[16];
        fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, 0x80, 0x80, 0x80, 0xff);
        if (m(t, 0x2cf0) == col && m(t, 0x2cf4) == row)
            fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(colour, 0xff, 0xff, 0xff, 0xff);
        vc(f, 51, r[0], r[1], r[0], r[3], colour);
        vc(f, 51, r[0], r[1], r[2], r[1], colour);
    }
}

// 0x540000 (frame, rectangle or null, all cells)
void THISCALL Tile_DrawRegion(BYTE *t, BYTE *frame, const int *rect, BYTE all)
{
    static_frame(glob_addr(0x711070), 1, glob_addr(FRAME_DRAW), 0x540410);
    if (!gb(glob_addr(0x7110e4))) {
        fl<WORD>(glob<BYTE *>(FRAME_DRAW), 0x12) = (WORD)(cw(t) + 1);
        fl<WORD>(glob<BYTE *>(FRAME_DRAW), 0x14) = (WORD)(ch(t) + 1);
        set_pitch(glob_addr(FRAME_DRAW), cw(t) + 1, 1);
        set_depth(glob_addr(FRAME_DRAW), colour_depth());
        gb(glob_addr(0x7110e4)) = 1;
    }
    int r[4];
    if (rect) {
        r[0] = rect[0], r[1] = rect[1], r[2] = rect[2], r[3] = rect[3];
    } else {
        vc(frame, 36, r);
    }
    auto clamp = [](int v, int hi) { return v < 0 ? 0 : v > hi ? hi : v; };
    int hi = (int)m(t, 0x2d0c) - 2;
    const DWORD c0 = (DWORD)clamp(r[0] / (int)cw(t), hi);
    const DWORD c1 = (DWORD)clamp((r[2] - 1) / (int)cw(t), hi);
    hi = (int)m(t, 0x2d10) - 2;
    const DWORD r0 = (DWORD)clamp(r[1] / (int)ch(t), hi);
    const DWORD r1 = (DWORD)clamp((r[3] - 1) / (int)ch(t), hi);
    for (DWORD row = r0; row <= r1; row++) {
        for (DWORD col = c0; col <= c1; col++) {
            if (!all) {
                if (row != r0 && row != r1 && col != c0 && col != c1)
                    continue;
                if (u(t, 0x2d2c) >= 3)
                    return;
            }
            const DWORD w = cw(t), hh = ch(t);
            const int x0 = (int)(w * col), y0 = (int)(hh * row);
            int src[4] = {0, 0, (int)w, (int)hh};
            int cell[4] = {x0, y0, x0 + (int)w, y0 + (int)hh};
            int dst[4] = {x0, y0, x0 + (int)w, y0 + (int)hh};
            int grid[4] = {x0, y0, x0 + (int)w, y0 + (int)hh};
            BYTE *marks = t + 8;
            const DWORD layer = vc(marks, 54, col, row) ? 1 : 2;
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x540b80)(t, col, row, glob_addr(FRAME_DRAW), layer);
            if (all)
                vc(frame, 23, glob_addr(FRAME_DRAW), src, dst, glob_addr(MODE_COPY));
            fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4d2bc0)(gd(glob_addr(TILE_CACHE)), gd(glob_addr(0x711084)));
            gd(glob_addr(0x711084)) = 0;
            if (!all)
                continue;
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, BYTE *, int *)>(0x541090)(t, col, row, frame, grid);
            if (!t[0x60] || !vc(marks, 54, col, row))
                continue;
            BYTE *lay = bp(m(t, 0x2d44));
            vc(lay, 51, cell[0], cell[1], cell[2], cell[3], glob_addr(0x6be378));
            vc(lay, 51, cell[2], cell[1], cell[0], cell[3], glob_addr(0x6be378));
            static_colour(glob_addr(0x711070), 2, glob_addr(0x7110c8), 0x2c, 0x17, 0xff, 0x540400);
            fn<DWORD(THISCALL *)(BYTE *, int *, DWORD)>(0x4b3430)(lay, cell, glob_addr(0x7110c8));
        }
    }
    gd(glob_addr(0x711084)) = 0;
}

// 0x53ff90 (frame, rectangle or null)
void THISCALL Tile_DrawRegions(BYTE *t, BYTE *frame, const int *rect)
{
    u(t, 0x2d2c) = 0;
    u(t, 0x2d30) = 0;
    u(t, 0x2d34) = 0;
    if (u(t, 0x2cf8) && --u(t, 0x2cf8) == 0) {
        u(t, 0x2cf8) = 0;
        fn<DWORD(THISCALL *)(DWORD)>(0x4d2b50)(gd(glob_addr(TILE_CACHE)));
    }
    fn<void(THISCALL *)(BYTE *, BYTE *, const int *, DWORD)>(0x540000)(t, frame, nullptr, 1);
    if (rect)
        fn<void(THISCALL *)(BYTE *, BYTE *, const int *, DWORD)>(0x540000)(t, frame, rect, 0);
}

// 0x541710 (mode)
void THISCALL Tile_BuildBands(BYTE *t, DWORD mode)
{
    BYTE *list = bp(m(t, 0x1064));
    const DWORD n = m(list, 0x34);
    if (mode == 0) {
        u(t, 0x2cf8) = 0;
        fn<DWORD(THISCALL *)(DWORD)>(0x4d2b50)(gd(glob_addr(TILE_CACHE)));
    } else if (mode == 1) {
        u(t, 0x2cf8) = 5;
    }
    if (!n) {
        for (DWORD i = 0; i < 0x100; i++) {
            m(entry(t, i), 0) = 0;
            m(entry(t, i), 0xc) = 0;
            m(entry(t, i), 0x18) = 0;
        }
        return;
    }
    for (DWORD i = 0; i < 0x100; i++) {
        const LD pos = fild64((long long)(DWORD)(i * n)) * cst(glob_addr(0x5fce00));
        const DWORD k = (DWORD)x87_ftol(pos);
        LD frac = pos - fild((int)k);
        const DWORD k1 = k + 1;
        if (k1 >= n)
            frac = cst(glob_addr(C_ZERO));
        const float &blending = m<float>(t, 0x1068);
        const LD rest = cst(glob_addr(C_ONE)) - blending;
        float weight;
        if (!c03(frac, rest))
            weight = fstp((frac - rest) * (cst(glob_addr(C_ONE)) / blending));
        else
            weight = raw_float(0);
        BYTE *e = entry(t, i);
        const DWORD lst = ptrv(bp(m(t, 0x1064)));
        if (colour_depth() == 8) {
            if (c0(fld(weight), (LD)*glob<double *>(0x5fcdf8))) {
                m(e, 0) = record_loaded(lst, k);
                m(e, 0xc) = 0;
                m(e, 0x18) = 0;
            } else if (c0(fld(weight), cst(glob_addr(0x5eee10)))) {
                m(e, 0) = record_loaded(lst, k);
                m(e, 0xc) = record_loaded(ptrv(bp(m(t, 0x1064))), k1);
                m(e, 0x18) = glob_addr(0x6767d0);
            } else if (c0(fld(weight), cst(glob_addr(0x5eee14)))) {
                m(e, 0) = record_loaded(lst, k);
                m(e, 0xc) = record_loaded(ptrv(bp(m(t, 0x1064))), k1);
                m(e, 0x18) = glob_addr(0x6767c0);
            } else if (c0(fld(weight), cst(glob_addr(0x5eb2d0)))) {
                m(e, 0) = record_inline(lst, k1);
                m(e, 0xc) = record_loaded(ptrv(bp(m(t, 0x1064))), k);
                m(e, 0x18) = glob_addr(0x6767c0);
            } else if (c03(fld(weight), cst(glob_addr(C_ONE)))) {
                m(e, 0) = record_loaded(lst, k1);
                m(e, 0xc) = record_inline(ptrv(bp(m(t, 0x1064))), k);
                m(e, 0x18) = glob_addr(0x6767d0);
            }
        } else {
            m(t, 0xc64 + i * 4) = (DWORD)x87_ftol(fld(weight) * cst(glob_addr(0x5eb728)));
            m(e, 0) = record_loaded(lst, k);
            m(e, 0xc) = m(t, 0xc64 + i * 4) ? record_inline(ptrv(bp(m(t, 0x1064))), k1) : 0;
            m(e, 0x18) = 0;
        }
        m(e, 4) = (1u << (vc(bp(m(e, 0)), 41) & 31)) - 1;
        m(e, 8) = (1u << (vc(bp(m(e, 0)), 42) & 31)) - 1;
        if (const DWORD s = m(e, 0xc)) {
            m(e, 0x10) = (1u << (vc(bp(s), 41) & 31)) - 1;
            m(e, 0x14) = (1u << (vc(bp(m(e, 0xc)), 42) & 31)) - 1;
        }
        m(t, 0x64 + i * 4) = vc(bp(m(e, 0)), 31);
        m(t, 0x464 + i * 4) = m(e, 0xc) ? vc(bp(m(e, 0xc)), 31) : 0;
        m(t, 0x864 + i * 4) = m(e, 0x18) ? gd(m(e, 0x18)) : 0;
    }
}

}  // namespace

ZAX_HOOKS(tile_map_draw)
{
    const Hook list[] = {
#include "tile_map_draw_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
