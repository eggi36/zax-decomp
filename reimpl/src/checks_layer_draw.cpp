// Checks of layer_draw.cpp with the frame check harness, in the game thread.
//  - The dirty tile frame: Get, Mark and CountDown on fake 8-bit tile frames (the class's fields after the
//    CFrame part) with random tiles, rectangles inside, across and outside, shifts 3-6; the constructor,
//    SetPixelSize, SetShift, the destructor and the deleting destructor on pool blocks (CFrame_SetSize
//    allocates the tiles; compared: the fields, the size and the cleared tiles).
//  - The queue drawing (DrawTileEntries, DrawTile, DrawTileOnce, the outlines, DrawDirtyTiles in both
//    modes, with and without "skip" and 0x6c1cf8): the two global queues 0x6c1bc0/0x6c1ca0 swapped for
//    fake entries (slot 1 fills the entry's rectangle through the frame it gets, slot 3 gives the
//    rectangle; +4, +8, +0xc and +0x24 random) for the case and put back; fake 5-6-5 frames and fake dirty
//    tile frames; the flags 0x6c1cf7-0x6c1cf9 set for the case. The function-local statics (the outline
//    colours, the static clip frame) and their guards are put back before each side and at the end, with
//    atexit a stand-in, so the running game keeps its state. Compared: the entries' calls (entry, the
//    frame they got: arena by offset, stack by low byte, the static clip frame by name), the frame calls,
//    the pixels, eax where reproduced.
//  - Layer_DrawPass and World_DrawView on fake save data, tile map, lists, parts, packets and monitor
//    entries with every exe function they call outside the module a recording stand-in (the temporary
//    list, FindRect, the pool and queue resets, sort and merge, the dirty marks, the tile map, the dirty
//    tiles, the count down, the packet, the last beam, the received array's growth, the grid's insert,
//    remove and delete, the listener removal); the received array 0x6c1c30 swapped for a fake; the
//    application's flag 29 and 0x6c1cf5/0x6c1cfa both ways. Compared: the event log and the fakes.
//  - COffsetClippedProxyFrame's slots 39 and 45 on fake proxies.
#include "checks_frame_r1.h"

namespace zax {

int hooks_layer_draw(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"layer_draw", &hooks_layer_draw, {}, {}, {}, 0, true};

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
template <typename F> DWORD fn_address(F f) { return (DWORD)(uintptr_t)f; }

constexpr DWORD QUEUE_GROUND = 0x6c1bc0, QUEUE_OBJECTS = 0x6c1ca0, RECEIVED = 0x6c1c30;
constexpr DWORD STATIC_CLIP = 0x6c1b18;

// ---- The event log of a side (the fakes' calls), hashed --------------------------------------------------

int g_side;
unsigned g_evhash[2];
int g_nev[2];
DWORD g_evfirst[2][16][4];
// A value as both sides see it: the arena by offset, the stack by its low byte, the static clip frame
// (a clip frame on the stack lies elsewhere in the original's and the reimplementation's frames: "stack")
DWORD tagv(DWORD v)
{
    if (v == STATIC_CLIP)
        return 0xc0000000u;
    if (v >= g_h.stack_limit && v < g_h.stack_base)
        return 0xb5000000u;
    return map_value(g_side, v);
}
void log_object_difference()
{
    const int off = objects_difference();
    if (off < 0)
        return;
    const DWORD *a = reinterpret_cast<const DWORD *>(g_h.arena[0].objects + off), *b = reinterpret_cast<const DWORD *>(g_h.arena[1].objects + off);
    log("    objects at %x: %x %x %x %x / %x %x %x %x", off, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
}
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_evhash[g_side] = fnv(g_evhash[g_side], e, 16);
    if (g_nev[g_side] < 16)
        memcpy(g_evfirst[g_side][g_nev[g_side]], e, 16);
    g_nev[g_side]++;
}
void ev_begin(int side)
{
    g_side = side;
    g_evhash[side] = 2166136261u;
    g_nev[side] = 0;
}
DWORD ev_end(int side) { return g_evhash[side] ^ (DWORD)g_nev[side] * 0x9e3779b1u; }
void log_ev_difference()
{
    for (int i = 0; i < 16 && i < g_nev[0] && i < g_nev[1]; i++)
        if (memcmp(g_evfirst[0][i], g_evfirst[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_evfirst[0][i][0], g_evfirst[0][i][1], g_evfirst[0][i][2], g_evfirst[0][i][3],
                g_evfirst[1][i][0], g_evfirst[1][i][1], g_evfirst[1][i][2], g_evfirst[1][i][3]);
            return;
        }
    log("    events: %d / %d", g_nev[0], g_nev[1]);
}

// ---- Fake queue entries ------------------------------------------------------------------------------------

struct Item {
    DWORD vtable;
    DWORD above;     // 0x04
    DWORD partner;   // 0x08
    BYTE flag_c, pad[3];
    DWORD id;        // 0x10
    Rect rect;       // 0x14
    BYTE opaque, pad2[3];   // 0x24
};
static_assert(sizeof(Item) == 0x28);

DWORD g_colour_obj;   // the case's FieldColor object (offset)
DWORD THISCALL item_draw(Item *self, void *frame)
{
    ev(0x1000 + self->id, tagv(ptrv(frame)));
    // fill the entry's rectangle through the frame it got (a clip frame or the frame itself)
    vcall<DWORD>(frame, 28, &self->rect, object<BYTE>(g_side, g_colour_obj));
    return 0x51000000u + self->id;
}
Rect *THISCALL item_rect(Item *self, Rect *out)
{
    ev(0x3000 + self->id);
    *out = self->rect;
    return out;
}
DWORD g_item_vtable[8];

struct QueueSwap {
    BYTE saved[2][0x10];
    void save()
    {
        memcpy(saved[0], at<void *>(QUEUE_GROUND), 0x10);
        memcpy(saved[1], at<void *>(QUEUE_OBJECTS), 0x10);
    }
    void restore()
    {
        memcpy(at<void *>(QUEUE_GROUND), saved[0], 0x10);
        memcpy(at<void *>(QUEUE_OBJECTS), saved[1], 0x10);
    }
};
// Points a global queue at the side's array of fake entries (data and count; vtable and capacity kept)
void set_queue(DWORD q, DWORD array, DWORD n, int side)
{
    dw(at<void *>(q), 4) = ptrv(object<BYTE>(side, array));
    dw(at<void *>(q), 8) = n;
}

// The function-local statics the drawing builds (guards, objects), put back before each side and at the end
struct Statics {
    SavedBytes clip, clip_guard, colours, colour_guard, array, array_guard, flags;
    void save()
    {
        save_bytes(clip, STATIC_CLIP, 0x1c);
        save_bytes(clip_guard, 0x6c1b90, 1);
        save_bytes(colours, 0x6c1b98, 0x20);
        save_bytes(colour_guard, 0x6c1bd1, 1);
        save_bytes(array, 0x6c1c50, 0x10);
        save_bytes(array_guard, 0x6c1b60, 1);
        save_bytes(flags, 0x6c1cf5, 6);
    }
    void restore() const
    {
        restore_bytes(clip);
        restore_bytes(clip_guard);
        restore_bytes(colours);
        restore_bytes(colour_guard);
        restore_bytes(array);
        restore_bytes(array_guard);
        restore_bytes(flags);
    }
};
Statics g_statics;
QueueSwap g_queues;

// ---- The case's fakes in the arena -------------------------------------------------------------------------

struct QueueCase {
    DWORD items, ground, objects;   // object offsets
    DWORD ng, no;
};
QueueCase make_queues(int max_items, const Rect &area)
{
    QueueCase qc{};
    const int n = (int)rnd((DWORD)max_items + 1);
    qc.items = alloc_object((DWORD)n * sizeof(Item) + 1);
    qc.ground = alloc_object(4 * 8);
    qc.objects = alloc_object(4 * 8);
    Item it[8];
    for (int i = 0; i < n; i++) {
        Item &x = it[i];
        memset(&x, 0, sizeof x);
        x.vtable = ptrv(g_item_vtable);
        x.id = (DWORD)i;
        const int l = rnd_range(area.left - 20, area.right), t = rnd_range(area.top - 20, area.bottom);
        x.rect = {l, t, l + rnd_range(0, 60), t + rnd_range(0, 50)};
        x.opaque = (BYTE)(rnd(3) == 0);
        x.flag_c = (BYTE)rnd(2);
        x.partner = rnd(2) ? 1 : 0;
        x.above = rnd(3) == 0 && i > 0 ? (DWORD)rnd((DWORD)i) + 1 : 0;   // 1-based index, made a pointer below
    }
    for (int side = 0; side < 2; side++) {
        Item *dst = object<Item>(side, qc.items);
        DWORD *g = object<DWORD>(side, qc.ground), *o = object<DWORD>(side, qc.objects);
        DWORD ng = 0, no = 0;
        for (int i = 0; i < n; i++) {
            dst[i] = it[i];
            if (it[i].above)
                dst[i].above = ptrv(&dst[it[i].above - 1]);
            if (it[i].partner)
                dst[i].partner = ptrv(&dst[(i + 1) % n]);
            if ((i & 1) && ng < 8)
                g[ng++] = ptrv(&dst[i]);
            else if (no < 8)
                o[no++] = ptrv(&dst[i]);
        }
        qc.ng = ng;
        qc.no = no;
    }
    return qc;
}

DWORD make_colour()
{
    const DWORD c = alloc_object(sizeof(FieldColor));
    const DWORD r = rnd(256), g = rnd(256), b = rnd(256);
    for (int side = 0; side < 2; side++)
        at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(object<BYTE>(side, c), r, g, b, 0xff);
    return c;
}

// A dirty tile frame: an 8-bit fake frame of cols x rows tiles and the class's fields after it
DWORD make_dirty(int cols, int rows, DWORD sx, DWORD sy, int fill)
{
    FrameSpec s;
    s.format = PAL8;
    s.width = cols;
    s.height = rows;
    const int fi = make_frame(s);
    const DWORD extra = alloc_object(0x18);
    BYTE tiles[256];
    for (int i = 0; i < cols * rows && i < 256; i++)
        tiles[i] = (BYTE)(rnd(4) < (DWORD)fill ? 1 + rnd(3) : 0);
    for (int side = 0; side < 2; side++) {
        BYTE *f = reinterpret_cast<BYTE *>(frame(side, fi));
        DWORD *x = object<DWORD>(side, extra);
        x[0] = (DWORD)(cols << sx);
        x[1] = (DWORD)(rows << sy);
        x[2] = 1u << sx;
        x[3] = 1u << sy;
        x[4] = sx;
        x[5] = sy;
        FrameData *fd = frame(side, fi);
        for (int y = 0; y < rows; y++)
            memcpy(fd->pixels + fd->rows[y], tiles + y * cols, (DWORD)cols);
        (void)f;
    }
    return g_h.frames[fi].object;
}

// ---- The dirty tile frame ----------------------------------------------------------------------------------

void check_dirty_tiles(Tally &t)
{
    g_seed = 0x480e90;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        const DWORD sx = 3 + rnd(4), sy = 3 + rnd(4);
        const int cols = rnd_range(0, 14), rows = rnd_range(0, 12);
        const DWORD d = make_dirty(cols, rows, sx, sy, (int)rnd(4));
        const DWORD rect = alloc_object(sizeof(Rect));
        const int span_x = cols << sx, span_y = rows << sy;
        Rect r;
        r.left = rnd_range(-40, span_x + 20);
        r.top = rnd_range(-40, span_y + 20);
        r.right = rnd(6) ? r.left + rnd_range(0, span_x + 40) : r.left - rnd_range(0, 30);
        r.bottom = rnd(6) ? r.top + rnd_range(0, span_y + 40) : r.top - rnd_range(0, 30);
        for (int side = 0; side < 2; side++)
            *object<Rect>(side, rect) = r;
        const int kind = (int)rnd(3);
        if (kind == 0) {
            const DWORD x = rnd(4) ? rnd((DWORD)cols + 2) : random(), y = rnd(4) ? rnd((DWORD)rows + 2) : random();
            run_sides(t, g_hooks, "DirtyTiles_Get", [&](int side) -> DWORD {
                return at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x480e90)(object<BYTE>(side, d), x, y);
            });
        } else if (kind == 1) {
            const DWORD v = rnd(4) ? rnd(4) : random();
            run_sides(t, g_hooks, "DirtyTiles_Mark", [&](int side) -> DWORD {
                return at<DWORD(THISCALL *)(void *, const Rect *, DWORD)>(0x480fc0)(object<BYTE>(side, d), object<Rect>(side, rect), v);
            });
        } else {
            run_sides(t, g_hooks, "DirtyTiles_CountDown", [&](int side) -> DWORD {
                return at<DWORD(THISCALL *)(void *, const Rect *)>(0x4810b0)(object<BYTE>(side, d), object<Rect>(side, rect));
            });
        }
    }
}

// The constructor, the sizes and the destructors on pool blocks
void check_dirty_lifetime(Tally &t)
{
    g_seed = 0x480e20;
    for (int k = 0; k < scaled(300); k++) {
        const int w = rnd(6) ? rnd_range(0, 900) : rnd_range(0, 20), h = rnd(6) ? rnd_range(0, 700) : rnd_range(0, 20);
        const DWORD sx = rnd(5) ? 3 + rnd(5) : rnd(40), sy = rnd(5) ? 3 + rnd(5) : rnd(40);
        const int order = (int)rnd(2);
        DWORD r[2][3] = {}, fields[2][8] = {};
        unsigned tiles[2] = {};
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                g_hooks.out();
            BYTE *o = static_cast<BYTE *>(at<void *(__fastcall *)(DWORD)>(0x417710)(0x58));
            r[side][0] = at<BYTE *(THISCALL *)(void *)>(0x480e20)(o) == o;
            if (order) {
                r[side][1] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x480f50)(o, sx, sy);
                r[side][2] = at<DWORD(THISCALL *)(void *, int, int)>(0x480ee0)(o, w, h);
            } else {
                r[side][1] = at<DWORD(THISCALL *)(void *, int, int)>(0x480ee0)(o, w, h);
                r[side][2] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x480f50)(o, sx, sy);
            }
            for (int i = 0; i < 6; i++)
                fields[side][i] = dw(o, 0x40 + 4 * (DWORD)i);
            fields[side][6] = dw(o, 0x12) & 0xffff;
            fields[side][7] = dw(o, 0x14) & 0xffff;
            // set-size results that are pointers count by being one
            for (int i = 1; i < 3; i++)
                if (r[side][i] > 0x10000 && r[side][i] < 0x80000000u)
                    r[side][i] = 0xb10cu;
            BYTE *px = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(o, 0xc)));
            tiles[side] = px ? fnv(2166136261u, px, fields[side][6] * fields[side][7]) : 0;
            if (k & 1) {
                at<DWORD(THISCALL *)(void *)>(0x480e80)(o);
                at<void(__fastcall *)(void *, DWORD)>(0x54d130)(o, 0x58);
            } else {
                at<void *(THISCALL *)(void *, DWORD)>(0x480e50)(o, 1);
            }
            if (side == 0)
                g_hooks.in();
        }
        char d[160];
        wsprintfA(d, "[case %d: %d x %d, shifts %u %u: results %x %x / %x %x, size %u x %u / %u x %u]", k, w, h, sx, sy, r[0][1], r[0][2], r[1][1],
                  r[1][2], fields[0][6], fields[0][7], fields[1][6], fields[1][7]);
        t.check(g_hooks.fits && !memcmp(r[0], r[1], sizeof r[0]) && !memcmp(fields[0], fields[1], sizeof fields[0]) && tiles[0] == tiles[1], "", d);
    }
}

// ---- The queue drawing -------------------------------------------------------------------------------------

// Runs a queue drawing case: queues swapped, flags and statics set, atexit a stand-in; eax combined with
// the entries' event log
template <typename F> void run_queue_case(Tally &t, const char *what, const QueueCase &qc, const BYTE flags[3], F call)
{
    run_sides(t, g_hooks, what, [&](int side) -> DWORD {
        g_statics.restore();
        *at<BYTE *>(0x6c1cf7) = flags[0];
        *at<BYTE *>(0x6c1cf8) = flags[1];
        *at<BYTE *>(0x6c1cf9) = flags[2];
        set_queue(QUEUE_GROUND, qc.ground, qc.ng, side);
        set_queue(QUEUE_OBJECTS, qc.objects, qc.no, side);
        CodePatch ax;
        patch_jump(ax, 0x5d0863, (void *)&fake_atexit);
        ev_begin(side);
        const DWORD eax = call(side);
        const DWORD h = ev_end(side);
        unpatch(ax);
        g_queues.restore();
        g_statics.restore();
        return fnv(h, &eax, 4);
    });
    if (g_evhash[0] != g_evhash[1] || g_nev[0] != g_nev[1])
        log_ev_difference();
    log_object_difference();
}

void check_queue_drawing(Tally &t)
{
    g_seed = 0x4e8bd0;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = RGB565;
        s.width = 128;
        s.height = 96;
        const int fi = make_frame(s);
        g_colour_obj = make_colour();
        const Rect area{0, 0, 128, 96};
        const QueueCase qc = make_queues(8, area);
        const DWORD tile = alloc_object(sizeof(Rect));
        const int tx = rnd_range(-20, 100), ty = rnd_range(-20, 70);
        const Rect tr{tx, ty, tx + rnd_range(0, 64), ty + rnd_range(0, 64)};
        for (int side = 0; side < 2; side++)
            *object<Rect>(side, tile) = tr;
        const BYTE flags[3] = {(BYTE)rnd(2), (BYTE)(rnd(4) == 0), (BYTE)rnd(2)};
        const int kind = (int)rnd(4);
        const DWORD queue = rnd(2) ? QUEUE_GROUND : QUEUE_OBJECTS;
        const BYTE ground = (BYTE)rnd(2);
        static const char *const names[4] = {"Layer_DrawTileEntries", "Layer_DrawTile", "Layer_DrawTileOnce", "World_DrawQueueOutlines"};
        run_queue_case(t, names[kind], qc, flags, [&](int side) -> DWORD {
            FrameData *f = frame(side, fi);
            switch (kind) {
            case 0: return at<DWORD(__fastcall *)(void *, DWORD, const Rect *)>(0x4e8bd0)(f, queue, object<Rect>(side, tile));
            case 1: return at<DWORD(__stdcall *)(void *, int, int, DWORD)>(0x4e8b60)(f, tx, ty, ground);
            case 2: return at<DWORD(__stdcall *)(void *, int, int)>(0x4e8ce0)(f, tx, ty);
            default: at<void(THISCALL *)(void *)>(0x4e8650)(f); return 0;
            }
        });
    }
}

void check_dirty_drawing(Tally &t)
{
    g_seed = 0x4e8080;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = RGB565;
        s.width = 128;
        s.height = 96;
        const int fi = make_frame(s);
        g_colour_obj = make_colour();
        const DWORD sx = 4 + rnd(2), sy = 4 + rnd(2);
        const int cols = (int)(128 >> sx) + rnd_range(-1, 1), rows = (int)(96 >> sy) + rnd_range(-1, 1);
        const DWORD d = make_dirty(cols, rows, sx, sy, 1 + (int)rnd(3));
        const Rect area{0, 0, 128, 96};
        const QueueCase qc = make_queues(8, area);
        const DWORD rect = alloc_object(sizeof(Rect));
        Rect r;
        r.left = rnd_range(-30, 100);
        r.top = rnd_range(-30, 80);
        r.right = r.left + rnd_range(0, 160);
        r.bottom = r.top + rnd_range(0, 120);
        // a right or bottom edge before the frame makes both tile bounds negative: compared unsigned, the scan runs
        // through all memory in the original too (its callers pass views inside the layer)
        if (r.right < 0)
            r.right = 0;
        if (r.bottom < 0)
            r.bottom = 0;
        for (int side = 0; side < 2; side++)
            *object<Rect>(side, rect) = r;
        const BYTE flags[3] = {(BYTE)rnd(2), (BYTE)(rnd(5) == 0), (BYTE)(rnd(4) == 0)};
        const BYTE skip = (BYTE)(rnd(5) == 0);
        if (g_frame_trace)
            log("    dirty tiles case %d: shifts %u %u, %d x %d tiles, rect %d %d %d %d, flags %u %u %u, skip %u, entries %u + %u", k, sx, sy, cols, rows, r.left,
                r.top, r.right, r.bottom, flags[0], flags[1], flags[2], skip, qc.ng, qc.no);
        run_queue_case(t, flags[0] ? "World_DrawDirtyTiles (second mode)" : "World_DrawDirtyTiles", qc, flags, [&](int side) -> DWORD {
            at<void(__fastcall *)(void *, void *, const Rect *, DWORD)>(0x4e8080)(frame(side, fi), object<BYTE>(side, d), object<Rect>(side, rect), skip);
            return 0;
        });
    }
}

// ---- The draw pass and the client's view: stand-ins ---------------------------------------------------------

struct PassCase {
    DWORD sd, map, list, list_data, parts, packet, received_data, monitor_data, entries;
    int nparts, nreceived_old, nwords, nmonitor;
    DWORD beams[8];   // 0: none, else 1 + part index
    BYTE flag29;
};
PassCase g_pc;
DWORD g_part_vtable[40], g_entry_vtable[4];
int g_beam_next;

BYTE *obj(DWORD off) { return object<BYTE>(g_side, off); }
DWORD rect_hash(const void *r) { return r ? fnv(2166136261u, r, 16) : 0; }

DWORD THISCALL s_get_temp(void **slot)
{
    ev(0x59a470, tagv(ptrv(slot)));
    *slot = obj(g_pc.list);
    dw(obj(g_pc.list), 8) = 0;
    return 0x59a47000u;
}
DWORD THISCALL s_release_temp(void **slot)
{
    ev(0x59a510, tagv(ptrv(slot)), tagv(ptrv(*slot)));
    return 0x59a51000u + (DWORD)g_nev[g_side];
}
DWORD THISCALL s_find_rect(void *grid, void *list, const Rect *r, DWORD a, DWORD kinds, DWORD b, DWORD c)
{
    ev(0x57a770, tagv(ptrv(grid)), tagv(ptrv(list)), rect_hash(r) ^ a ^ kinds * 3 ^ b * 5 ^ c * 7);
    dw(list, 8) = (DWORD)g_pc.nparts;
    return 0x57a77000u;
}
DWORD __cdecl s_pool_reset() { ev(0x487f90); return 0x487f9000u; }
DWORD THISCALL s_queue_clear(void *q) { ev(0x49ccb0, ptrv(q)); return 1; }
DWORD THISCALL s_sort(void *q) { ev(0x49ca40, ptrv(q)); return 2; }
DWORD THISCALL s_merge(void *q) { ev(0x49ca60, ptrv(q)); return 3; }
DWORD THISCALL s_mark(void *d) { ev(0x4e8810, tagv(ptrv(d))); return 4; }
DWORD THISCALL s_regions(void *map, void *frame, const Rect *r) { ev(0x53ff90, tagv(ptrv(map)), tagv(ptrv(frame)), rect_hash(r)); return 5; }
void __fastcall s_dirty(void *frame, void *d, const Rect *r, DWORD skip) { ev(0x4e8080, tagv(ptrv(frame)) ^ tagv(ptrv(d)) * 3, rect_hash(r), skip & 0xff); }
DWORD THISCALL s_count_down(void *d, const Rect *r) { ev(0x4810b0, tagv(ptrv(d)), rect_hash(r)); return 6; }
DWORD THISCALL s_part_draw(BYTE *p, void *g, void *o, const Rect *clip, DWORD force, const int *pos)
{
    ev(0x23000 + dw(p, 4), ptrv(g) ^ ptrv(o) * 3, rect_hash(clip) ^ force, fnv(2166136261u, pos, 12));
    return 7;
}
DWORD THISCALL s_part_after(BYTE *p, void *frame) { ev(0x30000 + dw(p, 4), tagv(ptrv(frame))); return 8; }
DWORD THISCALL s_remove_delete(void *arr, DWORD i)
{
    ev(0x4eea50, i);
    dw(arr, 8)--;
    return 9;
}
DWORD THISCALL s_read_word(void *packet, WORD *w)
{
    ev(0x520510, tagv(ptrv(packet)));
    *w = (WORD)g_pc.nwords;
    return 10;
}
DWORD THISCALL s_read_block(void *packet) { ev(0x5200d0, tagv(ptrv(packet))); return 11; }
DWORD __cdecl s_take_last()
{
    const DWORD b = g_beam_next < 8 ? g_pc.beams[g_beam_next++] : 0;
    ev(0x4fd5d0, b);
    return b ? ptrv(obj(g_pc.parts) + (b - 1) * 0x20) : 0;
}
DWORD THISCALL s_reserve(void *arr, DWORD n)
{
    ev(0x5a4080, n);
    if (n <= 24)
        dw(arr, 0xc) = n;
    return 12;
}
DWORD THISCALL s_flag29(void *app) { ev(0x4cdea0, ptrv(app)); return g_pc.flag29 ? 0x4cde01u : 0x4cde00u; }
DWORD THISCALL s_insert(void *grid, void *part) { ev(0x579900, tagv(ptrv(grid)), tagv(ptrv(part))); return 13; }
DWORD THISCALL s_remove(void *grid, void *part) { ev(0x579a80, tagv(ptrv(grid)), tagv(ptrv(part))); return 14; }
DWORD THISCALL s_remove_delete_part(void *grid, void *part) { ev(0x579f10, tagv(ptrv(grid)), tagv(ptrv(part))); return 15; }
DWORD THISCALL s_remove_listener(void *part, void *listener, int off, const char *name)
{
    ev(0x4fc200, tagv(ptrv(part)), tagv(ptrv(listener)), (DWORD)off ^ (DWORD)(BYTE)name[0]);
    return 16;
}
DWORD THISCALL s_entry_delete(BYTE *e, DWORD flags) { ev(0x5e1, tagv(ptrv(e)), flags); return ptrv(e); }

struct StandIn {
    DWORD address;
    void *fn;
};
const StandIn PASS_STANDS[] = {
    {0x59a470, (void *)&s_get_temp},
    {0x59a510, (void *)&s_release_temp},
    {0x57a770, (void *)&s_find_rect},
    {0x487f90, (void *)&s_pool_reset},
    {0x49ccb0, (void *)&s_queue_clear},
    {0x49ca40, (void *)&s_sort},
    {0x49ca60, (void *)&s_merge},
    {0x4e8810, (void *)&s_mark},
    {0x53ff90, (void *)&s_regions},
    {0x4e8080, (void *)&s_dirty},
    {0x4810b0, (void *)&s_count_down},
    {0x4eea50, (void *)&s_remove_delete},
    {0x520510, (void *)&s_read_word},
    {0x5200d0, (void *)&s_read_block},
    {0x4fd5d0, (void *)&s_take_last},
    {0x5a4080, (void *)&s_reserve},
    {0x4cdea0, (void *)&s_flag29},
    {0x579900, (void *)&s_insert},
    {0x579a80, (void *)&s_remove},
    {0x579f10, (void *)&s_remove_delete_part},
    {0x4fc200, (void *)&s_remove_listener},
    {0x5d0863, (void *)&fake_atexit},
};
constexpr int NSTANDS = sizeof PASS_STANDS / sizeof PASS_STANDS[0];

void init_vtables()
{
    for (int i = 0; i < 40; i++)
        g_part_vtable[i] = fn_address(&s_part_after);
    g_part_vtable[23] = fn_address(&s_part_draw);
    g_part_vtable[30] = fn_address(&s_part_after);
    for (int i = 0; i < 4; i++)
        g_entry_vtable[i] = fn_address(&s_entry_delete);
    for (int i = 0; i < 8; i++)
        g_item_vtable[i] = fn_address(&item_draw);
    g_item_vtable[3] = fn_address(&item_rect);
}

void make_pass_case()
{
    PassCase &pc = g_pc;
    pc = {};
    pc.sd = alloc_object(0x190);
    pc.map = alloc_object(0x60);
    pc.nparts = (int)rnd(7);
    pc.list = alloc_object(0x10);
    pc.list_data = alloc_object(4 * 8);
    pc.parts = alloc_object(0x20 * 8);
    pc.packet = alloc_object(0x10);
    pc.nreceived_old = (int)rnd(4);
    pc.received_data = alloc_object(4 * 24);
    pc.nwords = rnd(4) ? (int)rnd(6) : 0;
    for (int i = 0; i < 8; i++)
        pc.beams[i] = rnd(4) ? 1 + rnd(8) : 0;
    pc.nmonitor = (int)rnd(6);
    pc.monitor_data = alloc_object(4 * 8);
    pc.entries = alloc_object(0x14 * 8);
    pc.flag29 = (BYTE)rnd(2);
    const DWORD shift = 4 + rnd(3);
    DWORD flags[8];
    for (int i = 0; i < 8; i++)
        flags[i] = rnd(3) == 0 ? 0x10000u : rnd(2) ? 0x30000u : 0x12u;
    const BYTE skip = (BYTE)(random() & 1);
    int mon_part[8];
    for (int i = 0; i < 8; i++)
        mon_part[i] = (int)rnd(8);
    for (int side = 0; side < 2; side++) {
        BYTE *sd = object<BYTE>(side, pc.sd);
        dw(sd, 0x40) = ptrv(object<BYTE>(side, pc.map));
        sd[9] = skip;
        BYTE *dirty = object<BYTE>(side, pc.map) + 8;
        dw(dirty, 0x48) = 1u << shift;
        dw(dirty, 0x4c) = 1u << shift;
        dw(dirty, 0x50) = shift;
        dw(dirty, 0x54) = shift;
        BYTE *list = object<BYTE>(side, pc.list);
        dw(list, 4) = ptrv(object<BYTE>(side, pc.list_data));
        for (int i = 0; i < 8; i++) {
            BYTE *p = object<BYTE>(side, pc.parts) + i * 0x20;
            dw(p, 0) = ptrv(g_part_vtable);
            dw(p, 4) = (DWORD)i;
            dw(p, 0x1c) = flags[i];
            object<DWORD>(side, pc.list_data)[i] = ptrv(p);
        }
        // the monitor list (+0x144: +0xc data, +0x10 count) of entries naming parts
        dw(sd, 0x150) = ptrv(object<BYTE>(side, pc.monitor_data));
        dw(sd, 0x154) = (DWORD)pc.nmonitor;
        for (int i = 0; i < 8; i++) {
            BYTE *e = object<BYTE>(side, pc.entries) + i * 0x14;
            dw(e, 0) = ptrv(g_entry_vtable);
            dw(e, 8) = ptrv(object<BYTE>(side, pc.parts) + mon_part[i] * 0x20);
            object<DWORD>(side, pc.monitor_data)[i] = ptrv(e);
        }
    }
    (void)flags;
}

// The received array 0x6c1c30 swapped for the case's fake (data in the arena, capacity 24 at most)
struct Received {
    BYTE saved[0x10];
    void swap_in(int side)
    {
        memcpy(saved, at<void *>(RECEIVED), 0x10);
        BYTE *a = at<BYTE *>(RECEIVED);
        dw(a, 4) = ptrv(object<BYTE>(side, g_pc.received_data));
        dw(a, 8) = (DWORD)g_pc.nreceived_old;
        dw(a, 0xc) = (DWORD)g_pc.nreceived_old + rnd_side_cap;
        for (int i = 0; i < g_pc.nreceived_old; i++)
            object<DWORD>(side, g_pc.received_data)[i] = ptrv(object<BYTE>(side, g_pc.parts) + (i % 8) * 0x20);
    }
    DWORD state(int side) const
    {
        BYTE *a = at<BYTE *>(RECEIVED);
        DWORD v[3] = {map_value(side, dw(a, 4)), dw(a, 8), dw(a, 0xc)};
        return fnv(2166136261u, v, 12);
    }
    void swap_out() { memcpy(at<void *>(RECEIVED), saved, 0x10); }
    DWORD rnd_side_cap = 0;
};
Received g_received;

void check_pass(Tally &t)
{
    g_seed = 0x4e8900;
    for (int k = 0; k < scaled(1200); k++) {
        begin_case(random());
        make_pass_case();
        const DWORD frame_obj = alloc_object(0x10);
        const DWORD rect = alloc_object(sizeof(Rect));
        Rect r;
        r.left = rnd_range(-100, 900);
        r.top = rnd_range(-100, 700);
        r.right = r.left + rnd_range(0, 700);
        r.bottom = r.top + rnd_range(0, 500);
        for (int side = 0; side < 2; side++)
            *object<Rect>(side, rect) = r;
        const bool view = rnd(2);
        const bool with_packet = view && rnd(3);
        const BYTE hidden = (BYTE)rnd(2), merge = (BYTE)rnd(2);
        g_received.rnd_side_cap = rnd(3);
        DWORD received_state[2] = {};
        run_sides(t, g_hooks, view ? "World_DrawView (stand-ins)" : "Layer_DrawPass (stand-ins)", [&](int side) -> DWORD {
            g_statics.restore();
            *at<BYTE *>(0x6c1cf5) = hidden;
            *at<BYTE *>(0x6c1cfa) = merge;
            g_beam_next = 0;
            g_received.swap_in(side);
            CodePatch p[NSTANDS];
            for (int i = 0; i < NSTANDS; i++)
                patch_jump(p[i], PASS_STANDS[i].address, PASS_STANDS[i].fn);
            ev_begin(side);
            DWORD eax;
            if (view)
                eax = at<DWORD(THISCALL *)(void *, void *, void *, const Rect *)>(0x4e7c00)(object<BYTE>(side, g_pc.sd), object<BYTE>(side, frame_obj),
                                                                                         with_packet ? object<BYTE>(side, g_pc.packet) : nullptr,
                                                                                         object<Rect>(side, rect));
            else
                eax = at<DWORD(THISCALL *)(void *, void *, const Rect *)>(0x4e8900)(object<BYTE>(side, g_pc.sd), object<BYTE>(side, frame_obj),
                                                                                  object<Rect>(side, rect));
            const DWORD h = ev_end(side);
            for (int i = NSTANDS - 1; i >= 0; i--)
                unpatch(p[i]);
            received_state[side] = g_received.state(side);
            g_received.swap_out();
            g_statics.restore();
            const DWORD v[3] = {h, map_value(side, eax), received_state[side]};
            return fnv(2166136261u, v, 12);
        });
        if (g_evhash[0] != g_evhash[1] || g_nev[0] != g_nev[1])
            log_ev_difference();
        log_object_difference();
    }
}

// ---- COffsetClippedProxyFrame's slots 39 and 45 --------------------------------------------------------------

void check_proxy_slots(Tally &t)
{
    g_seed = 0x4e9980;
    for (int k = 0; k < scaled(200); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = RGB565;
        s.width = rnd_range(0, 120);
        s.height = rnd_range(0, 90);
        const int fi = make_frame(s);
        const Rect clip{rnd_range(-10, 30), rnd_range(-10, 30), rnd_range(0, 150), rnd_range(0, 120)};
        const DWORD p = make_offset_clipped(fi, rnd_range(-20, 20), rnd_range(-20, 20), clip);
        const bool height = rnd(2);
        run_sides(t, g_hooks, height ? "COffsetClippedProxyFrame slot 39" : "COffsetClippedProxyFrame slot 45", [&](int side) -> DWORD {
            return at<DWORD(THISCALL *)(void *)>(height ? 0x4e9980 : 0x4e9990)(object<BYTE>(side, p));
        });
    }
}

}  // namespace

ZAX_CHECKS(layer_draw)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    init_vtables();
    g_statics.save();
    g_queues.save();
    Tally dirty{"layer draw: the dirty tile frame (Get, Mark, CountDown)"};
    check_dirty_tiles(dirty);
    dirty.report();
    Tally life{"layer draw: the dirty tile frame's constructor, sizes and destructors"};
    check_dirty_lifetime(life);
    life.report();
    Tally queues{"layer draw: the tile and queue drawing, the outlines"};
    check_queue_drawing(queues);
    queues.report();
    Tally dirty_draw{"layer draw: World_DrawDirtyTiles"};
    check_dirty_drawing(dirty_draw);
    dirty_draw.report();
    Tally pass{"layer draw: Layer_DrawPass and World_DrawView (stand-ins)"};
    check_pass(pass);
    pass.report();
    Tally proxy{"layer draw: COffsetClippedProxyFrame slots 39 and 45"};
    check_proxy_slots(proxy);
    proxy.report();
    g_queues.restore();
    g_statics.restore();
}

}  // namespace zax
