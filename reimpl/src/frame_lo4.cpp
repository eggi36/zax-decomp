// Two frame functions over RLE blocks left by phase 4 (docs/DECOMPILATION.md 9, "Phase 4 leftovers"): the outline
// of a static switch's sprite 0x4b3530 with its row helper 0x4b39a0, and the hue remap render effect's slot 29
// 0x43c8e0. The frames' own functions go through their vtables, the RLE helpers 0x43cce0/0x43cd00/0x43cde0 and the
// row helper through their exe addresses.
//
// FrameDraw_StaticSwitch [confirmed: code; DrawItem::StaticSwitch 0x49c860 calls it] (thiscall destination frame;
// source frame, x, y, colour): the source's origin added to (x, y); the row table 0x6be848 (0x200 entries {left,
// right}, the function-local static with the guard bit 1 of 0x6bf849 and the atexit stub 0x4b3a60) cleared to
// {999, 0} for min(height, 0x200) rows; a plain frame (flags & 6 == 0) gets its rectangle (four lines, slot 51), an
// RLE frame the leftmost and rightmost opaque pixel of each row over its pixel block and its edge block (kind 2 also
// the alpha block: 0x4b39a0 per block with the block's data and row table, the width, the rows and the bytes per
// pixel 1/2/3/4 by depth 8/16/24/32) and then the outline: per row the lines between the changes of the left and
// the right edge (slot 51) and the edge pixels where a run starts further in (slot 55).
// 0x4b39a0 (fastcall table, data; row offsets, width, rows, bytes per pixel): per row the skipped pixels before the
// first opaque one and the end of the last opaque run, folded into the entry as min and max (the do-while reads at
// least one code; skip codes 0x80 | n, literal runs 0x40 | n with n pixels, fills n with one pixel).
#include "blitters.h"

namespace zax {

using namespace render::blit;

namespace {

constexpr DWORD EDGES = 0x6be848, EDGES_GUARD = 0x6bf849;

inline DWORD m(const BYTE *p, DWORD off) { return *reinterpret_cast<const DWORD *>(p + off); }
inline DWORD frame_call(BYTE *f, int slot) { return vcall<DWORD>(f, slot); }
inline void line(BYTE *f, int x1, int y1, int x2, int y2, DWORD colour) { vcall<DWORD>(f, 51, x1, y1, x2, y2, colour); }
inline void pixel(BYTE *f, int x, int y, DWORD colour) { vcall<DWORD>(f, 55, x, y, colour); }
using EdgeRows = void(__fastcall *)(DWORD *, const BYTE *, const DWORD *, DWORD, DWORD, DWORD);

}  // namespace

// 0x4b39a0
void __fastcall StaticSwitch_RowEdges(DWORD *edges, const BYTE *data, const DWORD *rows, DWORD width, DWORD count, DWORD bpp)
{
    if (!count)
        return;
    do {
        const BYTE *p = data + *rows;
        DWORD x = 0, first = 0, end = 0;
        bool seen = false;
        do {
            const BYTE c = *p++;
            if (c & 0x80) {
                x += c & 0x7f;
                if (!seen)
                    first += c & 0x7f;
                continue;
            }
            if (c & 0x40)
                p += (c & 0x3f) * bpp;
            else
                p += bpp;
            seen = true;
            x += c & 0x3f;
            end = x;
        } while (x < width);
        if (edges[0] >= first)
            edges[0] = first;
        if (edges[1] > end)
            end = edges[1];
        edges[1] = end;
        rows++;
        edges += 2;
    } while (--count);
}

// 0x4b3530
DWORD THISCALL FrameDraw_StaticSwitch(BYTE *dst, BYTE *src, int x, int y, DWORD colour)
{
    x += (int)frame_call(src, S_ORIGIN_X);
    y += (int)frame_call(src, S_ORIGIN_Y);
    DWORD *edges = glob<DWORD *>(EDGES);
    if (!(*glob<BYTE *>(EDGES_GUARD) & 1)) {
        *glob<BYTE *>(EDGES_GUARD) |= 1;
        for (int i = 0; i < 0x200; i++)
            edges[2 * i] = 999, edges[2 * i + 1] = 0;
        fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x4b3a60));
    }
    DWORD bpp;
    switch (frame_call(src, 29)) {
    case 0x10: bpp = 2; break;
    case 0x18: bpp = 3; break;
    case 0x20: bpp = 4; break;
    default: bpp = 1; break;
    }
    DWORD n = frame_call(src, S_HEIGHT);
    if (n > 0x200)
        n = 0x200;
    for (DWORD i = 0; i < n; i++)
        edges[2 * i] = 999, edges[2 * i + 1] = 0;
    const DWORD kind = frame_call(src, 38) & 6;
    if (kind == 0) {
        const int right = (int)frame_call(src, S_WIDTH) + x;
        const int bottom = (int)frame_call(src, S_HEIGHT) + y;
        line(dst, x, y, right, y, colour);
        line(dst, right, y, right, bottom, colour);
        line(dst, right, bottom, x, bottom, colour);
        line(dst, x, bottom, x, y, colour);
        return 0;
    }
    // the blocks: kind 4 pixels and edge, kind 2 also the alpha block
    const int BLOCKS[3][2] = {{S_PIXELS, S_ROWS}, {S_EDGE, S_EDGE_ROWS}, {S_ALPHA, S_ALPHA_ROWS}};
    if (kind == 2 || kind == 4) {
        for (int b = 0; b < (kind == 2 ? 3 : 2); b++) {
            if (!frame_call(src, BLOCKS[b][0]))
                continue;
            const DWORD w = frame_call(src, S_WIDTH);
            const DWORD *rows = reinterpret_cast<const DWORD *>(frame_call(src, BLOCKS[b][1]));
            const BYTE *data = reinterpret_cast<const BYTE *>(frame_call(src, BLOCKS[b][0]));
            fn<EdgeRows>(0x4b39a0)(edges, data, rows, w, n, bpp);
        }
    }
    // the outline
    DWORD prev_left = 999, prev_right = 0;
    int row = y;
    for (DWORD i = 0; i < n; i++, row++) {
        const DWORD left = edges[2 * i], right = edges[2 * i + 1];
        if (right) {
            if (prev_right) {
                if (prev_left < left) {
                    line(dst, (int)prev_left + x, row, (int)left + x, row, colour);
                } else {
                    line(dst, (int)prev_left + x, (int)i + y - 1, (int)left + x, (int)i + y - 1, colour);
                    pixel(dst, (int)left + x, row, colour);
                }
                if (prev_right < right) {
                    line(dst, (int)prev_right + x, (int)i + y - 1, (int)right + x, (int)i + y - 1, colour);
                    pixel(dst, (int)right + x, row, colour);
                } else {
                    line(dst, (int)prev_right + x, row, (int)right + x, row, colour);
                }
            } else {
                line(dst, (int)left + x, row, (int)right + x, row, colour);
            }
            prev_left = left;
            prev_right = right;
        } else {
            if (prev_right)
                line(dst, (int)prev_left + x, row, (int)prev_right + x, row, colour);
            prev_left = 999;
            prev_right = 0;
        }
    }
    return 0;
}

// ---- The hue remap render effect's slot 29 ------------------------------------------------------------------

namespace {

using Remap8Row = DWORD(__fastcall *)(BYTE *, const BYTE *, DWORD, const BYTE *);
using RemapBlock = DWORD(__fastcall *)(const BYTE *, const DWORD *, BYTE *, DWORD, DWORD, DWORD);

// An 8-bit RLE block copied from the effect's saved copy into the frame's block through the table: `literal`
// says whether literal runs go through Remap8_Row (else inline), `fill` the same for fills
void remap_rows(const BYTE *src, const DWORD *rows, BYTE *dst, DWORD width, DWORD count, const BYTE *table, bool literal_call,
                bool fill_call)
{
    if (!src || !count)
        return;
    do {
        const BYTE *s = src + *rows;
        BYTE *d = dst + *rows;
        DWORD left = width;
        do {
            const BYTE c = *s++;
            *d++ = c;
            if (c & 0x80) {
                left -= c & 0x7f;
            } else if (c & 0x40) {
                const DWORD k = c & 0x3f;
                if (literal_call) {
                    fn<Remap8Row>(0x43cce0)(d, s, k, table);
                } else {
                    DWORD j = k, i = 0;
                    do {
                        d[i] = table[s[i]];
                        i++;
                    } while (--j);
                }
                s += k;
                d += k;
                left -= k;
            } else {
                if (fill_call)
                    fn<Remap8Row>(0x43cce0)(d, s, 1, table);
                else
                    *d = table[*s];
                left -= c & 0x3f;
                s++;
                d++;
            }
        } while (left);
        rows++;
    } while (--count);
}

}  // namespace

// 0x43c8e0 (thiscall effect; frame): the frame's blocks remapped from the effect's saved copies (+0x10 pixels,
// +0x14, +0x18, +0x1c alpha, +0x20 edge) through the effect's table +0x30 (kind 2), or the 16-bit pixel and edge
// blocks through the word table 0x6bb4d8 (kind 4, 0x43cde0)
DWORD THISCALL HueRemapEffect_Slot29(BYTE *self, BYTE *f)
{
    const DWORD w = frame_call(f, S_WIDTH);
    const DWORD h = frame_call(f, S_HEIGHT);
    const DWORD kind = frame_call(f, 38) & 6;
    if (kind == 4) {
        frame_call(f, S_ORIGIN_X);
        frame_call(f, S_ORIGIN_Y);
        BYTE *d = reinterpret_cast<BYTE *>(frame_call(f, S_PIXELS));
        const DWORD *rows = reinterpret_cast<const DWORD *>(frame_call(f, S_ROWS));
        fn<RemapBlock>(0x43cde0)(at<const BYTE *>(m(self, 0x10)), rows, d, w, h, 0);
        d = reinterpret_cast<BYTE *>(frame_call(f, S_EDGE));
        rows = reinterpret_cast<const DWORD *>(frame_call(f, S_EDGE_ROWS));
        return fn<RemapBlock>(0x43cde0)(at<const BYTE *>(m(self, 0x20)), rows, d, w, h, 0);
    }
    if (kind != 2)
        return kind - 4;
    frame_call(f, S_ORIGIN_X);
    frame_call(f, S_ORIGIN_Y);
    const BYTE *table = at<const BYTE *>(m(self, 0x30));
    BYTE *d = reinterpret_cast<BYTE *>(frame_call(f, S_PIXELS));
    const DWORD *rows = reinterpret_cast<const DWORD *>(frame_call(f, S_ROWS));
    remap_rows(at<const BYTE *>(m(self, 0x10)), rows, d, w, h, table, false, false);
    table = at<const BYTE *>(m(self, 0x30));
    d = reinterpret_cast<BYTE *>(frame_call(f, S_EDGE));
    rows = reinterpret_cast<const DWORD *>(frame_call(f, S_EDGE_ROWS));
    remap_rows(at<const BYTE *>(m(self, 0x20)), rows, d, w, h, table, false, true);
    table = at<const BYTE *>(m(self, 0x30));
    d = reinterpret_cast<BYTE *>(frame_call(f, S_ALPHA));
    rows = reinterpret_cast<const DWORD *>(frame_call(f, S_ALPHA_ROWS));
    remap_rows(at<const BYTE *>(m(self, 0x1c)), rows, d, w, h, table, true, true);
    d = reinterpret_cast<BYTE *>(frame_call(f, S_THIRD));
    rows = reinterpret_cast<const DWORD *>(frame_call(f, S_THIRD_ROWS));
    fn<RemapBlock>(0x43cd00)(at<const BYTE *>(m(self, 0x18)), rows, d, w, h, m(self, 0x30));
    d = reinterpret_cast<BYTE *>(frame_call(f, S_EXTRA));
    rows = reinterpret_cast<const DWORD *>(frame_call(f, S_EXTRA_ROWS));
    return fn<RemapBlock>(0x43cd00)(at<const BYTE *>(m(self, 0x14)), rows, d, w, h, m(self, 0x30));
}

ZAX_HOOKS(frame_lo4)
{
    const Hook list[] = {
#include "frame_lo4_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
