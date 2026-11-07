// The layer's draw pass (phase 4, rendering): the dirty tile frame of the tile map (0x480e20-0x4810b0),
// World_DrawView 0x4e7c00, World_DrawDirtyTiles 0x4e8080, the debug outlines of the queue entries
// 0x4e8650, the layer's draw pass 0x4e8900, the tile helpers 0x4e8b60/0x4e8bd0/0x4e8ce0 and two slots of
// COffsetClippedProxyFrame (0x4e9980, 0x4e9990).
//
// The dirty tile frame [confirmed: code; VM] (0x58 bytes, vtable 0x5f162c, a CFrame of 8 bits with one byte
// per tile; embedded in CPlasmaTileMap at +0x08): +0x40/+0x44 the size in pixels, +0x48/+0x4c the tile size
// (1 << +0x50, 1 << +0x54; the constructor sets 32 and 5). 0x480ee0 sets the pixel size, 0x480f50 the
// shifts; both size the frame to the tiles that cover the pixels (rounded up; nothing when either count is
// 0). 0x480e90 reads a tile (0 outside). 0x480fc0 (rect, value) sets the tiles a pixel rectangle touches
// (cut to the frame's pixels; rows stepped by the width, not the row table), 0x4810b0 (rect) counts the
// tiles down that are not 0.
//
// The draw pass [confirmed: code; VM]: Layer_DrawPass 0x4e8900 (save data; frame, rect) widens the view
// rectangle by 40 pixels (left and top not below 0), joins it with the rectangle (Rect_Union 0x4ee810)
// and aligns it to tiles; the parts in it (CSortList2D_FindRect 0x57a770 on the save data's grid +0x14 into
// a temporary list 0x59a470, kinds 0x820000, or 0x800000 while 0x6c1cf5 is set) fill the two draw queues
// (slot 23: ground 0x6c1bc0, objects 0x6c1ca0, the tile rectangle, 0, a zero position) after the pool's
// reset 0x487f90 and the queues' 0x49ccb0; the queues are sorted (0x49ca40) and, while 0x6c1cfa is set,
// their strips merged (0x49ca60); the dynamic entries mark their tiles (0x4e8810); the tile map draws the
// widened rectangle (0x53ff90), the dirty tiles of the view rectangle aligned to tiles are drawn
// (0x4e8080, the save data's byte +9 "skip"), the tiles of the tile rectangle count down (0x4810b0) and
// each part gets slot 30 (the frame). World_DrawView 0x4e7c00 (save data; frame, packet, rect) does the
// same for a client: with a packet it first deletes the parts received last time (0x6c1c30), reads a word
// count and that many blocks (NetPacket_ReadBlock 0x5200d0, each block's part taken with 0x4fd5d0 and kept
// in 0x6c1c30, grown by half), gathers the parts with kinds 0x20820000 (0x20800000); only while the
// application's flag 29 is set (0x4cdea0) it inserts the received parts into the grid, adds them to the
// queues and draws as above (the tile map with the unaligned rectangle), then takes each received part out
// of the save data's monitor list (+0x144: its listener removed 0x4fc200, the entry deleted, the array
// closed up) and out of the grid (0x579a80). Last, gathered parts whose flag bit 16 is set are removed and
// deleted (0x579f10), from the last. The function-local static array 0x6c1c50 (guard 0x6c1b60, atexit
// 0x4e88b0) is built and never used.
//
// World_DrawDirtyTiles 0x4e8080 (fastcall frame, dirty tile frame; rect, skip) [confirmed: code; VM]: the
// tiles of the rectangle (cut to the dirty frame's size) are walked row by row; a run of set tiles becomes
// one pixel rectangle. Normally (0x6c1cf7 0) every run draws, unless "skip", the ground queue's entries
// whose byte +0x24 is set and that meet the run (each through a clip frame on the stack, ClipFrame 0x46d790
// / 0x46dc00, and the entry's slot 1; nothing while 0x6c1cf8 is set) and then every entry of the object
// queue the same way without the test. With 0x6c1cf7 set ("skip" returns at once) every entry with +0x24
// set is drawn whole (slot 1 with the frame), the object queue's other entries per run of set tiles under
// the entry's rectangle through the static clip frame 0x6c1b18 (guard 0x6c1b90, atexit 0x4e87d0). The
// outlines 0x4e8650 come last (not after a skip in the second mode).
//
// World_DrawQueueOutlines 0x4e8650 (frame) [confirmed: code; VM]: only while 0x6c1cf9 is set; every queue
// entry with a strip partner (+8): the rectangle of the entry above it (+4, slot 3) in (255, 100, 100) and
// a vertical line at the entry's left edge, or its right edge when +0xc is set, in (255, 220, 220) (both
// colours function-local statics 0x6c1b98/0x6c1ba8, guard 0x6c1bd1, atexit 0x4e87c0/0x4e87b0).
//
// The tile helpers [confirmed: code; VM]: 0x4e8bd0 (fastcall frame, queue; rect) draws the queue's entries
// whose +0x24 is 0 and whose rectangle meets the tile through a clip frame on the stack; eax the count;
// 0x4e8b60 (frame, x, y, ground) moves the frame's origin to the tile (0x4abaa0), draws the ground queue
// (with "ground") and the object queue into the 64 x 64 tile and puts the origin back; 0x4e8ce0 the same
// with the ground queue only. COffsetClippedProxyFrame's slots 39 and 45 forward to the clip frame (+0x20).
//
// Quirk kept [confirmed: code; a check case ended the game on both sides]: World_DrawDirtyTiles compares the
// tile bounds unsigned; a rectangle whose right (or bottom) edge lies before the frame gives a negative first
// and last tile, the scan starts and, after the first set tile, wraps through all memory. The game's
// callers pass views inside the layer.
//
// Return values are what the original leaves in eax where callers can see it; "eax not reproduced" marks
// the functions their callers use as void.
#include "ai_helpers.h"
#include "standalone_launcher.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD QUEUE_GROUND = 0x6c1bc0, QUEUE_OBJECTS = 0x6c1ca0, RECEIVED = 0x6c1c30;
constexpr DWORD ALT_DIRTY_DRAW = 0x6c1cf7, NO_ENTRY_DRAW = 0x6c1cf8, SHOW_OUTLINES = 0x6c1cf9, MERGE_STRIPS = 0x6c1cfa, COLLECT_HIDDEN = 0x6c1cf5;

struct R4 {
    int l, t, r, b;
};

DWORD queue_count(DWORD q) { return zax::fn<DWORD(THISCALL *)(void *)>(0x482de0)(at<void *>(q)); }
BYTE *queue_at(DWORD q, DWORD i) { return zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x566e40)(at<void *>(q), i); }
void item_rect(BYTE *item, R4 *r) { vcall<DWORD>(item, 3, r); }
void item_draw(BYTE *item, void *frame) { vcall<DWORD>(item, 1, frame); }
void clip_ctor(BYTE *c) { zax::fn<void *(THISCALL *)(void *)>(0x46d790)(c); }
void clip_set(BYTE *c, void *frame, const R4 *r) { zax::fn<DWORD(THISCALL *)(void *, void *, const R4 *)>(0x46dc00)(c, frame, r); }
// The inlined destructor of a clip frame on the stack: CProxyFrame's level, CFrameInterface's, CObject's
void clip_dtor(BYTE *c)
{
    dw(c, 0) = vt(0x5eb62c);
    obj_free_fields(c);
    dw(c, 0) = vt(0x5eb534);
    obj_free_fields(c);
    obj_dtor(c);
}
// An entry drawn through a clip frame on the stack (target frame, rectangle); not while 0x6c1cf8 is set
void draw_clipped(BYTE *item, void *frame, const R4 *r)
{
    alignas(4) BYTE clip[0x1c];
    clip_ctor(clip);
    clip_set(clip, frame, r);
    if (!*glob<BYTE *>(NO_ENTRY_DRAW))
        item_draw(item, clip);
    clip_dtor(clip);
}
DWORD frame_width(void *f) { return vcall<DWORD>(f, 45); }
DWORD frame_height(void *f) { return vcall<DWORD>(f, 39); }
DWORD *frame_rows(void *f) { return vcall<DWORD *>(f, 46); }
BYTE *frame_pixels(void *f) { return vcall<BYTE *>(f, 31); }
DWORD frame_set_size(void *f, DWORD w, DWORD h, DWORD bits) { return zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, bits); }
DWORD rect_intersect(const R4 *clip, R4 *r) { return zax::fn<DWORD(__fastcall *)(const R4 *, R4 *)>(0x4b2cd0)(clip, r); }
DWORD set_origin(void *f, int x, int y) { return zax::fn<DWORD(THISCALL *)(void *, int, int)>(0x4abaa0)(f, x, y); }

// The function-local statics the exe guards with a bit
bool first_use(DWORD guard, BYTE bit)
{
    BYTE &g = *at<BYTE *>(guard);
    if (g & bit)
        return false;
    g |= bit;
    return true;
}
void atexit_exe(DWORD fn) { zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(fn)); }

}  // namespace

// ---- The dirty tile frame ----------------------------------------------------------------------------------

// 0x480e20
BYTE *THISCALL DirtyTiles_Ctor(BYTE *self)
{
    zax::fn<void *(THISCALL *)(void *)>(0x4a8a90)(self);
    dw(self, 0) = vt(0x5f162c);
    dw(self, 0x40) = 0;
    dw(self, 0x44) = 0;
    dw(self, 0x50) = 5;
    dw(self, 0x54) = 5;
    dw(self, 0x48) = 0x20;
    dw(self, 0x4c) = 0x20;
    return self;
}
// 0x480e50 (slot 1)
BYTE *THISCALL DirtyTiles_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5f162c);
    zax::fn<void(THISCALL *)(void *)>(0x4a8b00)(self);
    if (flags & 1)
        zax::fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x58);
    return self;
}
// 0x480e80
DWORD THISCALL DirtyTiles_Dtor(BYTE *self)
{
    dw(self, 0) = vt(0x5f162c);
    return zax::fn<DWORD(THISCALL *)(void *)>(0x4a8b00)(self);
}
// 0x480e90
DWORD THISCALL DirtyTiles_Get(BYTE *self, DWORD x, DWORD y)
{
    if (x >= fl<WORD>(self, 0x12) || y >= fl<WORD>(self, 0x14))
        return 0;
    const DWORD row = frame_rows(self)[y];
    return bp(row + dw(self, 0xc))[x];
}
namespace {
DWORD resize_tiles(BYTE *self, int w, int h, int tw, int th)
{
    int cols = w / tw, rows = h / th;
    if ((DWORD)w & ((DWORD)tw - 1))
        cols++;
    if ((DWORD)h & ((DWORD)th - 1))
        rows++;
    if (!cols)
        return (DWORD)rows;
    if (!rows)
        return 0;
    return frame_set_size(self, (DWORD)cols, (DWORD)rows, 8);
}
}  // namespace
// 0x480ee0 (pixel width, height)
DWORD THISCALL DirtyTiles_SetPixelSize(BYTE *self, int w, int h)
{
    dw(self, 0x44) = (DWORD)h;
    const int tw = (int)(1u << (dw(self, 0x50) & 31));
    dw(self, 0x48) = (DWORD)tw;
    dw(self, 0x40) = (DWORD)w;
    const int th = (int)(1u << (dw(self, 0x54) & 31));
    dw(self, 0x4c) = (DWORD)th;
    return resize_tiles(self, w, h, tw, th);
}
// 0x480f50 (shift x, shift y)
DWORD THISCALL DirtyTiles_SetShift(BYTE *self, DWORD sx, DWORD sy)
{
    dw(self, 0x50) = sx;
    const int tw = (int)(1u << (sx & 31));
    dw(self, 0x54) = sy;
    const int th = (int)(1u << (sy & 31));
    dw(self, 0x48) = (DWORD)tw;
    dw(self, 0x4c) = (DWORD)th;
    return resize_tiles(self, (int)dw(self, 0x40), (int)dw(self, 0x44), tw, th);
}
namespace {
// The tile rectangle of a pixel rectangle cut to the frame; false (and the intersection's eax) when empty
bool tile_rect(BYTE *self, const R4 *r, R4 *t, DWORD *eax)
{
    const DWORD sx = dw(self, 0x50), sy = dw(self, 0x54);
    const R4 clip{0, 0, (int)((DWORD)fl<WORD>(self, 0x12) << (sx & 31)), (int)((DWORD)fl<WORD>(self, 0x14) << (sy & 31))};
    R4 c = *r;
    *eax = rect_intersect(&clip, &c);
    if (!(BYTE)*eax)
        return false;
    t->l = c.l >> (sx & 31);
    t->r = (c.r - 1) >> (sx & 31);
    t->t = c.t >> (sy & 31);
    t->b = (c.b - 1) >> (sy & 31);
    return true;
}
}  // namespace
// 0x480fc0 (rect, value)
DWORD THISCALL DirtyTiles_Mark(BYTE *self, const R4 *r, BYTE value)
{
    R4 t;
    DWORD eax;
    if (!tile_rect(self, r, &t, &eax))
        return eax;
    DWORD row = frame_rows(self)[t.t];
    for (int y = t.t; y <= t.b; y++) {
        for (int x = t.l; x <= t.r; x++)
            bp(dw(self, 0xc) + (DWORD)x)[row] = value;
        row += fl<WORD>(self, 0x12);
    }
    return row;
}
// 0x4810b0 (rect)
DWORD THISCALL DirtyTiles_CountDown(BYTE *self, const R4 *r)
{
    R4 t;
    DWORD eax;
    if (!tile_rect(self, r, &t, &eax))
        return eax;
    DWORD row = frame_rows(self)[t.t];
    for (int y = t.t; y <= t.b; y++) {
        for (int x = t.l; x <= t.r; x++) {
            BYTE *p = bp(dw(self, 0xc) + (DWORD)x) + row;
            if (*p)
                (*p)--;
        }
        row += fl<WORD>(self, 0x12);
    }
    return row;
}

// ---- Drawing the queues ------------------------------------------------------------------------------------

// 0x4e8bd0 (fastcall frame, queue; rect)
DWORD __fastcall Layer_DrawTileEntries(void *frame, DWORD queue, const R4 *tile)
{
    const DWORD n = queue_count(queue);
    for (DWORD i = 0; i < n; i++) {
        BYTE *item = queue_at(queue, i);
        if (item[0x24])
            continue;
        R4 r;
        item_rect(item, &r);
        const int l = r.l > tile->l ? r.l : tile->l;
        const int t = r.t > tile->t ? r.t : tile->t;
        const int rr = r.r < tile->r ? r.r : tile->r;
        const int b = r.b >= tile->b ? tile->b : r.b;
        if (l >= rr || t >= b)
            continue;
        alignas(4) BYTE clip[0x1c];
        clip_ctor(clip);
        clip_set(clip, frame, tile);
        item_draw(item, clip);
        clip_dtor(clip);
    }
    return n;
}
namespace {
DWORD draw_entries(void *frame, DWORD queue, const R4 *tile) { return zax::fn<DWORD(__fastcall *)(void *, DWORD, const R4 *)>(0x4e8bd0)(frame, queue, tile); }
}  // namespace
// 0x4e8b60 (frame, x, y, ground)
DWORD __stdcall Layer_DrawTile(void *frame, int x, int y, BYTE ground)
{
    const R4 tile{x, y, x + 0x40, y + 0x40};
    set_origin(frame, x, y);
    if (ground)
        draw_entries(frame, glob_addr(QUEUE_GROUND), &tile);
    draw_entries(frame, glob_addr(QUEUE_OBJECTS), &tile);
    return set_origin(frame, 0, 0);
}
// 0x4e8ce0 (frame, x, y)
DWORD __stdcall Layer_DrawTileOnce(void *frame, int x, int y)
{
    const R4 tile{x, y, x + 0x40, y + 0x40};
    set_origin(frame, x, y);
    draw_entries(frame, glob_addr(QUEUE_GROUND), &tile);
    return set_origin(frame, 0, 0);
}

// 0x4e8650 (frame; eax not reproduced)
void THISCALL World_DrawQueueOutlines(void *frame)
{
    if (!*glob<BYTE *>(SHOW_OUTLINES))
        return;
    for (DWORD q = glob_addr(QUEUE_GROUND); q; q = q == glob_addr(QUEUE_GROUND) ? glob_addr(QUEUE_OBJECTS) : 0) {
        const DWORD n = queue_count(q);
        for (DWORD i = 0; i < n; i++) {
            BYTE *item = queue_at(q, i);
            if (!dw(item, 8))
                continue;
            if (BYTE *above = bp(dw(item, 4))) {
                R4 r;
                item_rect(above, &r);
                if (first_use(glob_addr(0x6c1bd1), 1)) {
                    zax::fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob<void *>(0x6c1b98), 0xff, 0x64, 0x64, 0xff);
                    atexit_exe(fn_addr(0x4e87c0));
                }
                zax::fn<DWORD(THISCALL *)(void *, const R4 *, void *)>(0x4b3430)(frame, &r, glob<void *>(0x6c1b98));
            }
            R4 r;
            item_rect(item, &r);
            if (first_use(glob_addr(0x6c1bd1), 2)) {
                zax::fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob<void *>(0x6c1ba8), 0xff, 0xdc, 0xdc, 0xff);
                atexit_exe(fn_addr(0x4e87b0));
            }
            const int x = item[0xc] ? r.r - 1 : r.l;
            vcall<DWORD>(frame, 51, x, r.t, x, r.b - 1, glob<void *>(0x6c1ba8));
        }
    }
}
namespace {
void draw_outlines(void *frame) { zax::fn<void(THISCALL *)(void *)>(0x4e8650)(frame); }
}  // namespace

// 0x4e8080 (fastcall frame, dirty tiles; rect, skip; eax not reproduced)
void __fastcall World_DrawDirtyTiles(void *frame, BYTE *dirty, const R4 *rect, BYTE skip)
{
    if (*glob<BYTE *>(ALT_DIRTY_DRAW)) {
        if (skip)
            return;
        const DWORD tw = dw(dirty, 0x48), sx = dw(dirty, 0x50) & 31, sy = dw(dirty, 0x54) & 31;
        const int x0 = rect->l >> sx;
        int x1 = (rect->r >> sx) + 1;
        const int w = (int)frame_width(dirty);
        if (x1 >= w)
            x1 = w;
        const int y0 = rect->t >> sy;
        int y1 = (rect->b >> sy) + 1;
        const int h = (int)frame_height(dirty);
        if (y1 >= h)
            y1 = h;
        for (DWORD q = glob_addr(QUEUE_GROUND); q; q = q == glob_addr(QUEUE_GROUND) ? glob_addr(QUEUE_OBJECTS) : 0) {
            const DWORD n = queue_count(q);
            for (DWORD i = 0; i < n; i++) {
                BYTE *item = queue_at(q, i);
                if (item[0x24]) {
                    if (!*glob<BYTE *>(NO_ENTRY_DRAW))
                        item_draw(item, frame);
                    continue;
                }
                if (q == glob_addr(QUEUE_GROUND))
                    continue;
                R4 ir;
                item_rect(item, &ir);
                const int a = ir.l >> sx;
                const int xs = x0 > a ? x0 : a;
                DWORD xe = (DWORD)(((ir.r - 1) >> sx) + 1);
                if ((DWORD)x1 < xe)
                    xe = (DWORD)x1;
                const int b = ir.t >> sy;
                const int ys = y0 > b ? y0 : b;
                DWORD ye = (DWORD)y1;
                const DWORD c = (DWORD)(((ir.b - 1) >> sy) + 1);
                if (ye >= c)
                    ye = c;
                for (DWORD y = (DWORD)ys; y < ye; y++) {
                    DWORD x = (DWORD)xs;
                    const DWORD *rows = frame_rows(dirty);
                    const BYTE *p = bp(rows[y] + ptr_value(frame_pixels(dirty)) + x);
                    while (x < xe) {
                        if (!*p++) {
                            x++;
                            continue;
                        }
                        const int px = (int)(x << sx), py = (int)(y << sy);
                        int pr = px + (int)tw;
                        x++;
                        while (x < xe && *p++) {
                            pr += (int)dw(dirty, 0x48);
                            x++;
                        }
                        R4 run;
                        zax::fn<int(THISCALL *)(R4 *, int, int, int, int)>(0x4ee7f0)(&run, px, py, pr, (int)dw(dirty, 0x4c) + py);
                        if ((BYTE)zax::fn<DWORD(THISCALL *)(const R4 *, const R4 *)>(0x4ee860)(&ir, &run)) {
                            if (first_use(glob_addr(0x6c1b90), 1)) {
                                clip_ctor(glob<BYTE *>(0x6c1b18));
                                atexit_exe(fn_addr(0x4e87d0));
                            }
                            clip_set(glob<BYTE *>(0x6c1b18), frame, &run);
                            if (!*glob<BYTE *>(NO_ENTRY_DRAW))
                                item_draw(item, glob<void *>(0x6c1b18));
                        }
                        x++;
                    }
                }
            }
        }
        draw_outlines(frame);
        return;
    }
    const DWORD sx = dw(dirty, 0x50) & 31;
    const int x0 = rect->l >> sx;
    int x1 = (rect->r >> sx) + 1;
    const int w = (int)frame_width(dirty);
    if (x1 >= w)
        x1 = w;
    const DWORD sy = dw(dirty, 0x54) & 31;
    const int y0 = rect->t >> sy;
    int y1 = (rect->b >> sy) + 1;
    const int h = (int)frame_height(dirty);
    if (y1 >= h)
        y1 = h;
    for (DWORD y = (DWORD)y0; y < (DWORD)y1; y++) {
        DWORD x = (DWORD)x0;
        const DWORD *rows = frame_rows(dirty);
        const BYTE *p = bp(rows[y] + ptr_value(frame_pixels(dirty)) + x);
        for (; x < (DWORD)x1; x++) {
            if (!*p++)
                continue;
            const DWORD tw = dw(dirty, 0x48);
            const int px = (int)(x << (dw(dirty, 0x50) & 31)), py = (int)(y << (dw(dirty, 0x54) & 31));
            int pr = (int)tw + px;
            x++;
            while (x < (DWORD)x1) {
                if (!*p++)
                    break;
                pr += (int)tw;
                x++;
            }
            const R4 run{px, py, pr, (int)dw(dirty, 0x4c) + py};
            if (skip)
                continue;
            const DWORD ng = queue_count(glob_addr(QUEUE_GROUND));
            for (DWORD i = 0; i < ng; i++) {
                BYTE *item = queue_at(glob_addr(QUEUE_GROUND), i);
                if (!item[0x24])
                    continue;
                R4 ir;
                item_rect(item, &ir);
                const int l = ir.l > run.l ? ir.l : run.l;
                const int t = ir.t > run.t ? ir.t : run.t;
                const int r = ir.r < run.r ? ir.r : run.r;
                const int b = ir.b < run.b ? ir.b : run.b;
                if (l >= r || t >= b)
                    continue;
                draw_clipped(item, frame, &run);
            }
            const DWORD no = queue_count(glob_addr(QUEUE_OBJECTS));
            for (DWORD i = 0; i < no; i++) {
                BYTE *item = queue_at(glob_addr(QUEUE_OBJECTS), i);
                R4 ir;
                item_rect(item, &ir);
                draw_clipped(item, frame, &run);
            }
        }
    }
    draw_outlines(frame);
}

// ---- The layer's draw pass and the client's view -----------------------------------------------------------

namespace {
R4 align_to_tiles(const R4 &r, const BYTE *dirty)
{
    const int tw = (int)fl<DWORD>(const_cast<BYTE *>(dirty), 0x48), th = (int)fl<DWORD>(const_cast<BYTE *>(dirty), 0x4c);
    return {r.l & ~(tw - 1), r.t & ~(th - 1), (r.r & ~(tw - 1)) + tw, (r.b & ~(th - 1)) + th};
}
void *temp_list_get(void **slot) { return zax::fn<void *(THISCALL *)(void **)>(0x59a470)(slot); }
DWORD temp_list_release(void **slot) { return zax::fn<DWORD(THISCALL *)(void **)>(0x59a510)(slot); }
void find_rect(BYTE *sd, void *list, const R4 *r, DWORD kinds)
{
    zax::fn<DWORD(THISCALL *)(void *, void *, const R4 *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(sd + 0x14, list, r, 0, kinds, 0x10000, 0);
}
// The parts' slot 23 into both queues (the tile rectangle, not forced, a zero position)
void fill_queues(BYTE *part, const R4 *tiles)
{
    int pos[3] = {0, 0, 0};
    vcall<DWORD>(part, 23, glob<void *>(QUEUE_GROUND), glob<void *>(QUEUE_OBJECTS), tiles, 0, pos);
}
void sort_queues()
{
    zax::fn<DWORD(THISCALL *)(void *)>(0x49ca40)(glob<void *>(QUEUE_OBJECTS));
    zax::fn<DWORD(THISCALL *)(void *)>(0x49ca40)(glob<void *>(QUEUE_GROUND));
    if (*glob<BYTE *>(MERGE_STRIPS)) {
        zax::fn<DWORD(THISCALL *)(void *)>(0x49ca60)(glob<void *>(QUEUE_OBJECTS));
        zax::fn<DWORD(THISCALL *)(void *)>(0x49ca60)(glob<void *>(QUEUE_GROUND));
    }
}
void reset_queues()
{
    zax::fn<DWORD(__cdecl *)()>(0x487f90)();
    zax::fn<DWORD(THISCALL *)(void *)>(0x49ccb0)(glob<void *>(QUEUE_GROUND));
    zax::fn<DWORD(THISCALL *)(void *)>(0x49ccb0)(glob<void *>(QUEUE_OBJECTS));
}
void draw_tiles(BYTE *sd, void *frame, BYTE *dirty, const R4 *regions, const R4 *view, const R4 *tiles)
{
    zax::fn<DWORD(THISCALL *)(void *, void *, const R4 *)>(0x53ff90)(bp(dw(sd, 0x40)), frame, regions);
    zax::fn<void(__fastcall *)(void *, BYTE *, const R4 *, DWORD)>(0x4e8080)(frame, dirty, view, sd[9]);
    zax::fn<DWORD(THISCALL *)(void *, const R4 *)>(0x4810b0)(dirty, tiles);
}
}  // namespace

// The launcher's /DebugDraw (Zax-Launcher.dll) replaces the slot 30 call of this function's part loop
// (0x4e8b3c, `mov eax, [ecx]; call [eax+0x78]`) with a call of its thunk, which after slot 30, unless the mini map
// is drawn, calls slot 29 (the debug drawing) and AI_Draw for each of the part's own AIs; the standalone exe's
// built-in launcher answers instead
static bool debug_draw()
{
#ifdef ZAX_STANDALONE
    return launcher::debug_draw();
#else
    return *at<const BYTE *>(0x4e8b3c) == 0xe8;
#endif
}

// 0x4e8900 (save data; frame, rect)
DWORD THISCALL Layer_DrawPass(BYTE *sd, void *frame, const R4 *rect)
{
    BYTE *dirty = bp(dw(sd, 0x40)) + 8;
    const R4 view = align_to_tiles(*rect, dirty);
    R4 wide{rect->l - 0x28, rect->t - 0x28, rect->r + 0x28, rect->b + 0x28};
    if (wide.l < 0)
        wide.l = 0;
    if (wide.t < 0)
        wide.t = 0;
    zax::fn<int(THISCALL *)(R4 *, const R4 *)>(0x4ee810)(&wide, rect);
    const R4 tiles = align_to_tiles(wide, dirty);
    void *list = nullptr;
    temp_list_get(&list);
    find_rect(sd, list, &tiles, *glob<BYTE *>(COLLECT_HIDDEN) ? 0x800000 : 0x820000);
    const DWORD n = dw(list, 8);
    reset_queues();
    for (DWORD i = 0; i < n; i++)
        fill_queues(bp(reinterpret_cast<DWORD *>(dw(list, 4))[i]), &tiles);
    sort_queues();
    zax::fn<DWORD(THISCALL *)(void *)>(0x4e8810)(dirty);
    draw_tiles(sd, frame, dirty, &wide, &view, &tiles);
    const bool debug = debug_draw();
    for (DWORD i = 0; i < n; i++) {
        BYTE *part = bp(reinterpret_cast<DWORD *>(dw(list, 4))[i]);
        vcall<DWORD>(part, 30, frame);
        if (debug && !*glob<BYTE *>(0x6c2908)) {   // not while the mini map is drawn (0x4f4ae0)
            vcall<DWORD>(part, 29, frame);
            // and each AI of the part itself, as the launcher's thunk does (AI_Draw 0x4fd0b0)
            const DWORD ais = dw(part, 0x3c), count = dw(part, 0x40);
            for (DWORD k = 0; ais && k < count; k++)
                if (const DWORD ai = reinterpret_cast<DWORD *>(ais)[k])
                    zax::fn<void(THISCALL *)(DWORD, void *, void *)>(0x4fd0b0)(ai, part, frame);
        }
    }
    return temp_list_release(&list);
}

// 0x4e7c00 (save data; frame, packet, rect)
DWORD THISCALL World_DrawView(BYTE *sd, void *frame, void *packet, const R4 *rect)
{
    if (first_use(glob_addr(0x6c1b60), 1)) {
        dw(glob<BYTE *>(0x6c1c50), 4) = 0;
        dw(glob<BYTE *>(0x6c1c50), 8) = 0;
        dw(glob<BYTE *>(0x6c1c50), 0xc) = 0;
        dw(glob<BYTE *>(0x6c1c50), 0) = vt(0x5f8b8c);
        atexit_exe(fn_addr(0x4e88b0));
    }
    BYTE *received = glob<BYTE *>(RECEIVED);
    if (packet)
        while (dw(received, 8))
            zax::fn<void(THISCALL *)(void *, DWORD)>(0x4eea50)(received, dw(received, 8) - 1);
    BYTE *dirty = bp(dw(sd, 0x40)) + 8;
    const R4 view = align_to_tiles(*rect, dirty);
    const R4 whole = *rect;
    const R4 tiles = align_to_tiles(*rect, dirty);
    void *list = nullptr;
    temp_list_get(&list);
    find_rect(sd, list, &tiles, *glob<BYTE *>(COLLECT_HIDDEN) ? 0x20800000 : 0x20820000);
    const DWORD n = dw(list, 8);
    reset_queues();
    for (DWORD i = 0; i < n; i++)
        fill_queues(bp(reinterpret_cast<DWORD *>(dw(list, 4))[i]), &tiles);
    if (packet) {
        WORD count = 0;
        zax::fn<DWORD(THISCALL *)(void *, WORD *)>(0x520510)(packet, &count);
        for (DWORD j = 0; j < count; j++) {
            zax::fn<DWORD(THISCALL *)(void *)>(0x5200d0)(packet);
            const DWORD part = zax::fn<DWORD(__cdecl *)()>(0x4fd5d0)();
            if (!part)
                continue;
            DWORD need = dw(received, 8) + 1;
            const DWORD cap = dw(received, 0xc);
            if (cap < need) {
                const DWORD grown = (DWORD)x87_ftol((long double)cap * *glob<const float *>(0x5ea5f4));
                if (grown > need)
                    need = grown;
                zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x5a4080)(received, need);
            }
            reinterpret_cast<DWORD *>(dw(received, 4))[dw(received, 8)] = part;
            dw(received, 8)++;
        }
    }
    if ((BYTE)zax::fn<DWORD(THISCALL *)(void *)>(0x4cdea0)(*glob<void **>(0x6c02cc))) {
        const DWORD m = dw(received, 8);
        for (DWORD k = 0; k < m; k++)
            zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x579900)(sd + 0x14, reinterpret_cast<DWORD *>(dw(received, 4))[k]);
        for (DWORD k = 0; k < m; k++)
            fill_queues(bp(reinterpret_cast<DWORD *>(dw(received, 4))[k]), &tiles);
        sort_queues();
        zax::fn<DWORD(THISCALL *)(void *)>(0x4e8810)(dirty);
        draw_tiles(sd, frame, dirty, &whole, &view, &tiles);
        BYTE *monitor = sd + 0x144;
        for (DWORD k = 0; k < m; k++) {
            const DWORD part = reinterpret_cast<DWORD *>(dw(received, 4))[k];
            const DWORD nm = dw(monitor, 0x10);
            for (DWORD i = 0; i < nm; i++) {
                DWORD *data = reinterpret_cast<DWORD *>(dw(monitor, 0xc));
                if (dw(bp(data[i]), 8) != part)
                    continue;
                remove_listener(bp(dw(bp(data[i]), 8)), monitor, -1, glob_addr(0x62e558));
                if (BYTE *e = bp(reinterpret_cast<DWORD *>(dw(monitor, 0xc))[i]))
                    vcall<DWORD>(e, 1, 1);
                const DWORD cnt = dw(monitor, 0x10);
                if (i < cnt) {
                    if (i + 1 < cnt) {
                        DWORD *d = reinterpret_cast<DWORD *>(dw(monitor, 0xc));
                        zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d + i, d + i + 1, (cnt - (i + 1)) * 4);
                    }
                    dw(monitor, 0x10)--;
                }
                break;
            }
            zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x579a80)(sd + 0x14, part);
        }
    }
    for (int i = (int)n - 1; i >= 0; i--) {
        BYTE *part = bp(reinterpret_cast<DWORD *>(dw(list, 4))[i]);
        if (dw(part, 0x1c) >> 16 & 1)
            zax::fn<DWORD(THISCALL *)(void *, void *)>(0x579f10)(sd + 0x14, part);
    }
    return temp_list_release(&list);
}

// 0x4e9980, 0x4e9990: COffsetClippedProxyFrame's slots 39 and 45 through the clip frame (+0x20)
DWORD THISCALL OffsetClipped_Height(BYTE *self) { return vcall<DWORD>(self + 0x20, 39); }
DWORD THISCALL OffsetClipped_Width(BYTE *self) { return vcall<DWORD>(self + 0x20, 45); }

ZAX_HOOKS(layer_draw)
{
    const Hook list[] = {
#include "layer_draw_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
