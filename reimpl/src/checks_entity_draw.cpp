// Checks of the entity drawing (entity_draw.cpp) against the original, in the game thread.
//  - The draw item's slots on items built in the frame check harness's object memory (checks_frame_common.h):
//    Draw (slot 1) of a copy of a real sprite frame's strip into fake 5-6-5 frames with a copy of a real blend
//    mode (Copy, Transparent Mask, Black Bias, Amplify Light) and its opacity, Rect, IsPixelSet, Merge (both
//    vtables), the static switch (slot 2; 0x4b3530 a recording stand-in), the constructor and the destructor
//    without freeing. Compared by the harness: pixels, frame calls, the objects.
//  - The queues on fake queues in the object memory: Compare, Sort (qsort through the hooked comparator),
//    Add (capacity left, no allocation), the constructor and destructor (data from the pool, one block per
//    side), the slot 1 jump on a recording object, MergeStrips on 2-8 items with random keys, partners and
//    strip rectangles.
//  - The pool: DrawItemPool_Next on a fake pool {vtable with a recording slot 2, data, count, capacity, used}
//    with items left and with none left (one heap item, freed after the case); the growth by 300 is not
//    checked (it would give 301 heap items per side).
//  - Real entities of the level (at most 40 per class kind, spread): slot 23 (CEntityBase::Draw, the walking
//    overlay's and the line's), Entity_Draw with other sequences of the model, AddDirtyRects (0x480fc0 a
//    recording stand-in), HitTest in all modes at points on and around the entity, the line's debug drawing
//    into a fake frame. During a case the world's draw item pool 0x6bdd08 is a fake of 400 items built once
//    by the original constructor (restored before each side), the two queues are fakes that record what is
//    added; the entity's flags (+0x1c), its children's and the overlay's sequence cache (+0x134/+0x138) are
//    saved before and put back after every side. A run of the original before the case loads what the
//    drawing loads on first use (the hue remap cache's blend modes, models of the chain). Compared: the
//    used items field by field (items by index, the clip frame by contents), the queues (by index), eax
//    (al for HitTest), the recorded dirty rectangles and frame calls. The animated sequences' clock
//    (Platform_Ticks24 0x4d2650, GetTickCount & 0xffffff: a synchronised sequence's frame, 0x55fee0) is a
//    stand-in with a fixed value per case, the same for both sides: read live, it could tick between the two
//    sides and give the second side the next frame (batch 6-16b: the intermittent difference of this check
//    on Cave1, the clip frame's target of an item, runs 20261027-201454-eb6c, 20261030-202426-30ef).
#include "checks_hud_common.h"
#include "checks_frame_r1.h"

namespace zax {

int hooks_entity_draw(Hook *out);

namespace {

using namespace framec;
using r1::rnd;
using r1::rnd_range;

int g_trace;
ModuleHooks g_hooks{"entity_draw", &hooks_entity_draw, {}, {}, {}, 0, true};
Sources g_sources;

template <typename R = DWORD, typename... A> R exe(DWORD a, void *self, A... args) { return at<R(THISCALL *)(void *, A...)>(a)(self, args...); }
DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
BYTE *bptr(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD &d32(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
bool kind_of_getter(void *o, DWORD getter) { return o && at<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, at<CClassInfo *(__cdecl *)()>(getter)()); }

constexpr DWORD BLEND_MODES[4] = {0x6bb4e0, 0x6bcc68, 0x6b9728, 0x6b85f0};
constexpr DWORD DRAW_POOL = 0x6bdd08;

// ---- Recording stand-ins and fake vtables -----------------------------------------------------------------------

struct Rec {
    static constexpr int MAX = 2048;
    DWORD v[MAX];
    int n;
    unsigned hash;
    void reset()
    {
        n = 0;
        hash = 2166136261u;
    }
    void add(DWORD x)
    {
        hash = fnv(hash, &x, 4);
        if (n < MAX)
            v[n++] = x;
    }
};
Rec g_rec[2];
int g_side;

// The static switch 0x4b3530 (frame; target, x, y, value), ret 0x10
DWORD THISCALL rec_static_switch(void *frame, void *target, DWORD x, DWORD y, DWORD v)
{
    Rec &r = g_rec[g_side];
    r.add(0x4b3530);
    r.add(map_value(g_side, pv(frame)));
    r.add(map_value(g_side, pv(target)));
    r.add(x);
    r.add(y);
    r.add(v);
    return 0x5eed0000u + x;
}
// The dirty rectangle list's add 0x480fc0 (list; rectangle, flag), ret 8
DWORD THISCALL rec_dirty_add(void *list, const int *rect, DWORD flag)
{
    Rec &r = g_rec[g_side];
    r.add(0x480fc0);
    r.add(pv(list));
    for (int i = 0; i < 4; i++)
        r.add((DWORD)rect[i]);
    r.add(flag);
    return 1;
}

// A fake object whose slot 1 and slot 2 record their calls (slot 2: the pool's and the queues' Add; it
// appends the item to the fake's own list)
struct FakeList {
    DWORD vtable;
    DWORD items[512];
    int n;
};
FakeList g_queue_ground, g_queue, g_pool_owner;
DWORD g_fake_vtable[8];
DWORD THISCALL fake_slot1(FakeList *o)
{
    g_rec[g_side].add(0x51);
    g_rec[g_side].add(o == &g_queue ? 2 : o == &g_queue_ground ? 1 : 0);
    return 0x1111;
}
DWORD THISCALL fake_add(FakeList *o, DWORD *item)
{
    if (o->n < 512)
        o->items[o->n++] = *item;
    return (DWORD)o->n;
}
// The pool object's slot 2 for DrawItemPool_Next (the pool is in the harness's object memory: records the
// item and frees it, a heap item made by the pool)
BYTE *g_pool_made[2];
DWORD THISCALL fake_pool_add(void *, DWORD *item)
{
    g_pool_made[g_side] = bptr(*item);
    g_rec[g_side].add(0x52);
    g_rec[g_side].add(*item ? 1 : 0);
    return 1;
}
DWORD g_pool_vtable[4];
void init_fakes()
{
    memcpy(g_fake_vtable, at<void *>(0x5f3a80), sizeof g_fake_vtable);
    g_fake_vtable[1] = pv((void *)&fake_slot1);
    g_fake_vtable[2] = pv((void *)&fake_add);
    g_queue.vtable = g_queue_ground.vtable = g_pool_owner.vtable = pv(g_fake_vtable);
    memcpy(g_pool_vtable, at<void *>(0x5f3a80), sizeof g_pool_vtable);
    g_pool_vtable[2] = pv((void *)&fake_pool_add);
}

// ---- Items in the harness's object memory -------------------------------------------------------------------

constexpr DWORD ITEM = 0x54;
// An item constructed by the original constructor on both sides (the trampoline: the same bytes)
DWORD make_item()
{
    const DWORD off = alloc_object(ITEM + 0xc);
    if (off == 0xffffffffu)
        return off;
    for (int side = 0; side < 2; side++)
        original<BYTE *(THISCALL *)(void *)>(0x488bb0)(object<BYTE>(side, off));
    return off;
}
void set_item(DWORD off, DWORD field, DWORD v)
{
    for (int side = 0; side < 2; side++)
        d32(object<BYTE>(side, off), field) = v;
}
// The clip frame of an item aimed at frame `target` (an arena frame) with a strip rectangle
void set_strip(DWORD off, int target, const Rect &r)
{
    for (int side = 0; side < 2; side++) {
        BYTE *it = object<BYTE>(side, off);
        d32(it, 0x3c) = target >= 0 ? pv(frame(side, target)) : 0;
        memcpy(it + 0x40, &r, 16);
    }
}

// A real sprite frame copied into the arena; its rectangle (origin, size) in r
int sprite_source(Rect &r)
{
    for (int tries = 0; tries < 4 && g_sources.n; tries++) {
        const Source &so = g_sources.list[rnd((DWORD)g_sources.n)];
        if (so.kind != SRC_SPRITE || !r1::compressed_sane(so.frame) || so.frame->width > 160 || so.frame->height > 160)
            continue;
        const int fi = copy_compressed(so.frame);
        if (fi < 0)
            return -1;
        r = {so.frame->x, so.frame->y, so.frame->x + so.frame->width, so.frame->y + so.frame->height};
        return fi;
    }
    return -1;
}

void check_items(Tally &t)
{
    g_seed = 0x49c740;
    int drawn = 0;
    for (int k = 0; k < scaled(600); k++) {
        begin_case(random());
        Rect fr;
        const int src = sprite_source(fr);
        if (src < 0)
            continue;
        FrameSpec d;
        d.width = rnd_range(8, 96);
        d.height = rnd_range(8, 64);
        const int dst = make_frame(d);
        const DWORD item = make_item(), other = make_item(), out = alloc_object(16);
        // Amplify Light and Black Bias only for strips without an edge block: Amplify Light registers no edge
        // spans and would call address 0 for one (DECOMPILATION.md 9, the blitter map; the original too)
        const bool edges = frame(0, src)->blocks[6] != nullptr;
        const DWORD mode = r1::copy_mode_object(BLEND_MODES[edges ? rnd(2) : rnd(4)]);
        if (dst < 0 || item == 0xffffffffu || other == 0xffffffffu || out == 0xffffffffu || mode == 0xffffffffu)
            continue;
        // a strip of the frame (often the whole frame)
        Rect sr = fr;
        if (rnd(2)) {
            sr.left = rnd_range(fr.left, fr.right - 1);
            sr.right = rnd_range(sr.left + 1, fr.right);
        }
        set_strip(item, src, sr);
        Rect so = fr;
        so.left = rnd_range(fr.left - 8, fr.right);
        so.right = so.left + rnd_range(0, 40);
        so.bottom = so.top + rnd_range(0, fr.bottom - fr.top);
        set_strip(other, src, so);
        set_item(item, 0x1c, (DWORD)rnd_range(-fr.left - 40, d.width + 20));
        set_item(item, 0x20, (DWORD)rnd_range(-fr.top - 40, d.height + 20));
        set_item(other, 0x1c, (DWORD)rnd_range(-20, 60));
        set_item(other, 0x20, (DWORD)rnd_range(-20, 60));
        set_item(item, 0x10, 0);
        set_item(item, 0x2c, 0);
        set_item(item, 0x28, rnd(3) ? 0x3f800000 : fbits((float)rnd(256) / 255.0f));
        for (int side = 0; side < 2; side++)
            d32(object<BYTE>(side, item), 0x30) = pv(object<void>(side, mode));
        // partner links for Merge: the item's partner the other one or a third value
        const DWORD link = rnd(3), other_link = rnd(2) ? 0x5678 : 0;
        for (int side = 0; side < 2; side++) {
            d32(object<BYTE>(side, item), 8) = link == 0 ? pv(object<void>(side, other)) : link == 1 ? 0 : 0x1234;
            d32(object<BYTE>(side, other), 8) = other_link;
        }
        const int px = rnd_range(-60, 160), py = rnd_range(-60, 160);
        const DWORD op = (DWORD)k % 6;
        static const char *const names[6] = {"DrawItem::Draw", "DrawItem::Rect", "DrawItem::IsPixelSet", "DrawItem::Merge", "Drawable::Merge",
                                             "DrawItem::StaticSwitch"};
        char what[96];
        wsprintfA(what, "%s case %d", names[op], k);
        if (g_trace)
            log("  item case %d: %s", k, what);
        CodePatch patch;
        if (op == 5)
            patch_jump(patch, 0x4b3530, (void *)&rec_static_switch);
        g_rec[0].reset();
        g_rec[1].reset();
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            g_side = side;
            BYTE *it = object<BYTE>(side, item);
            if (g_trace)
                log("    side %d: item %x (clip vt %x target %x rect %d %d %d %d) mode %x", side, pv(it), d32(it, 0x34), d32(it, 0x3c),
                    d32(it, 0x40), d32(it, 0x44), d32(it, 0x48), d32(it, 0x4c), d32(it, 0x30));
            switch (op) {
            case 0: return exe(0x49c740, it, frame(side, dst));
            case 1: return map_value(side, exe(0x49c890, it, object<void>(side, out)));
            case 2: return exe(0x49c8d0, it, px, py) & 0xff;
            case 3: return map_value(side, exe(0x49c900, it, object<void>(side, other)));
            case 4: return map_value(side, exe(0x49c720, it, object<void>(side, other)));
            default: return exe(0x49c860, it, frame(side, dst), (DWORD)px);
            }
        });
        if (op == 5) {
            unpatch(patch);
            t.check(g_rec[0].hash == g_rec[1].hash && g_rec[0].n == g_rec[1].n, "", "[static switch arguments]");
        }
        drawn += op == 0;
    }
    log("  entity draw: %d draw item blits", drawn);
}

// ---- Queues and the pool ------------------------------------------------------------------------------------

void check_queues(Tally &t)
{
    g_seed = 0x49ca60;
    for (int k = 0; k < scaled(1200); k++) {
        begin_case(random());
        const int op = k % 8;
        const int n = rnd_range(op == 7 ? 2 : 0, 8);
        DWORD items[8];
        bool ok = true;
        for (int i = 0; i < n; i++) {
            items[i] = make_item();
            ok &= items[i] != 0xffffffffu;
        }
        const DWORD queue = alloc_object(16), data = alloc_object(4 * 16), a = alloc_object(8);
        FrameSpec ts;
        ts.width = 48;
        ts.height = 48;
        const int target = make_frame(ts);   // the strips' clip frames cut their rectangle to it (slot 36)
        if (!ok || queue == 0xffffffffu || data == 0xffffffffu || a == 0xffffffffu || target < 0)
            continue;
        // items: keys, x, strip rectangles (the clip frame without a target), partners
        for (int i = 0; i < n; i++) {
            set_item(items[i], 0x18, (DWORD)rnd_range(-4, 12) * (rnd(4) ? 8 : 1));
            set_item(items[i], 0x1c, (DWORD)rnd_range(-10, 30));
            set_item(items[i], 0x20, (DWORD)rnd_range(-10, 30));
            Rect r;
            r.left = rnd_range(0, 30);
            r.top = rnd_range(0, 30);
            r.right = r.left + rnd_range(0, 24);
            r.bottom = r.top + rnd_range(0, 24);
            set_strip(items[i], target, r);
        }
        if (op == 7) {
            // sorted keys, random partners further on
            for (int i = 0; i < n; i++)
                set_item(items[i], 0x18, (DWORD)(i * 8 + rnd_range(0, 7)));
            for (int i = 0; i < n; i++) {
                const int p = rnd(2) ? rnd_range(0, n - 1) : -1;
                for (int side = 0; side < 2; side++)
                    d32(object<BYTE>(side, items[i]), 8) = p >= 0 ? pv(object<void>(side, items[p])) : 0;
            }
        }
        for (int side = 0; side < 2; side++) {
            BYTE *q = object<BYTE>(side, queue);
            d32(q, 0) = 0x5f3a80;
            d32(q, 4) = pv(object<void>(side, data));
            d32(q, 8) = (DWORD)n;
            d32(q, 0xc) = 16;
            for (int i = 0; i < n; i++)
                object<DWORD>(side, data)[i] = pv(object<void>(side, items[i]));
            object<DWORD>(side, a)[0] = n ? pv(object<void>(side, items[0])) : 0;
            object<DWORD>(side, a)[1] = n > 1 ? pv(object<void>(side, items[n - 1])) : 0;
        }
        static const char *const names[8] = {"DrawQueue_Compare", "DrawQueue_Sort", "DrawQueue_Add", "DrawQueue_Ctor", "DrawQueue_Dtor",
                                             "Object_Slot1", "DrawQueue_Compare (same key)", "DrawQueue_MergeStrips"};
        if (op == 6 && n > 1)
            for (int side = 0; side < 2; side++)
                d32(object<BYTE>(side, items[n - 1]), 0x18) = d32(object<BYTE>(side, items[0]), 0x18);
        if ((op == 0 || op == 6) && n < 2)
            continue;
        BYTE *pool_block[2] = {};
        char what[96];
        wsprintfA(what, "%s, %d items, case %d", names[op], n, k);
        if (g_trace)
            log("  queue case %d: %s", k, what);
        g_rec[0].reset();
        g_rec[1].reset();
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            g_side = side;
            BYTE *q = object<BYTE>(side, queue);
            switch (op) {
            case 0:
            case 6: return (DWORD)at<int(__cdecl *)(void *, void *)>(0x49c9a0)(object<DWORD>(side, a), object<DWORD>(side, a) + 1);
            case 1: at<DWORD(__fastcall *)(void *)>(0x49ca40)(q); return 0;
            case 2: exe(0x49ca30, q, n ? object<void>(side, items[0]) : nullptr); return 0;
            case 3: return map_value(side, exe(0x49c9d0, q));
            case 4:
                pool_block[side] = static_cast<BYTE *>(pool_get_alloc(16 * 4));
                d32(q, 4) = pv(pool_block[side]);
                exe(0x49c9f0, q);
                return 0;
            case 5: {
                FakeList *f = side ? &g_queue : &g_queue_ground;
                at<DWORD(THISCALL *)(void *)>(0x49ccb0)(f);
                return 0;
            }
            default: at<void(__fastcall *)(void *)>(0x49ca60)(q); return 0;
            }
        });
        if (op == 5)
            t.check(g_rec[0].n == 2 && g_rec[1].n == 2 && g_rec[0].v[0] == g_rec[1].v[0], "", "[slot 1 jump]");
    }
}

void check_pool(Tally &t)
{
    g_seed = 0x488bd0;
    for (int k = 0; k < scaled(300); k++) {
        begin_case(random());
        const int n = rnd_range(1, 4);
        DWORD items[4];
        bool ok = true;
        for (int i = 0; i < n; i++) {
            items[i] = make_item();
            ok &= items[i] != 0xffffffffu;
        }
        // used: below the count (an item given out), or at the count at 300 (a new heap item added through
        // slot 2; the fake's slot 2 does not store it, so the item handed out is the data's entry 300)
        const bool full = rnd(3) == 0;
        const DWORD used = full ? 300 : (DWORD)rnd_range(0, n - 1);
        const DWORD pool = alloc_object(0x14), data = alloc_object(4 * (full ? 301 : 8));
        if (!ok || pool == 0xffffffffu || data == 0xffffffffu)
            continue;
        for (int side = 0; side < 2; side++) {
            BYTE *p = object<BYTE>(side, pool);
            DWORD *dd = object<DWORD>(side, data);
            d32(p, 0) = pv(g_pool_vtable);
            d32(p, 4) = pv(dd);
            for (int i = 0; i < n; i++)
                dd[i] = pv(object<void>(side, items[i]));
            if (full)
                dd[300] = dd[0];
            d32(p, 8) = full ? 300 : (DWORD)n;
            d32(p, 0xc) = full ? 301 : 8;
            d32(p, 0x10) = used;
        }
        g_rec[0].reset();
        g_rec[1].reset();
        g_pool_made[0] = g_pool_made[1] = nullptr;
        if (g_trace)
            log("  pool case %d: %d items, full %d", k, n, full);
        run_sides(t, g_hooks, full ? "DrawItemPool_Next, none left" : "DrawItemPool_Next", [&](int side) -> DWORD {
            g_side = side;
            return map_value(side, pv(at<BYTE *(__fastcall *)(void *)>(0x488bd0)(object<void>(side, pool))));
        });
        t.check(g_rec[0].hash == g_rec[1].hash && g_rec[0].n == g_rec[1].n, "", "[pool slot 2]");
        if (g_pool_made[0] && g_pool_made[1])
            t.check(d32(g_pool_made[0], 0) == d32(g_pool_made[1], 0) && d32(g_pool_made[0], 0x34) == d32(g_pool_made[1], 0x34), "",
                    "[the pool's new item: its vtables]");
        for (int side = 0; side < 2; side++)
            if (g_pool_made[side])
                original<BYTE *(THISCALL *)(void *, DWORD)>(0x487fa0)(g_pool_made[side], 1);
    }
}

// ---- Real entities --------------------------------------------------------------------------------------------

// The animated sequences' clock during a case (0x4d2650 patched): one value for both sides
DWORD g_case_ticks;
DWORD __cdecl case_ticks24() { return g_case_ticks & 0xffffff; }

constexpr int MAX_ENTITIES = 4096;
BYTE *g_entities[MAX_ENTITIES];
int g_nentities;
constexpr DWORD MODEL_RECORDS = 0x6cfe08, MODEL_COUNT = 0x6cfe0c;

bool model_loaded(BYTE *e)
{
    const BYTE *r = *at<const BYTE **>(MODEL_RECORDS);
    const DWORD m = d32(e, 0x74);
    return r && m < *at<DWORD *>(MODEL_COUNT) && *reinterpret_cast<const DWORD *>(r + m * 0x80 + 0x78) && !r[m * 0x80 + 0x7f];
}

void find_entities()
{
    g_nentities = 0;
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return;
    BYTE *layer = at<BYTE *(THISCALL *)(void *)>(0x4f1050)(world);
    if (!layer || !geo::readable(layer + 0x50, 0x28))
        return;
    const DWORD columns = d32(layer, 0x60), rows = d32(layer, 0x64);
    BYTE *cells = bptr(d32(layer, 0x68));
    if (columns > 4096 || rows > 4096 || !cells || !geo::readable(cells, columns * rows * 16))
        return;
    for (DWORD i = 0; i < columns * rows; i++) {
        BYTE *c = cells + i * 16;
        const DWORD count = d32(c, 8);
        DWORD *data = reinterpret_cast<DWORD *>(bptr(d32(c, 4)));
        if (count > 10000 || (count && !geo::readable(data, count * 4)))
            return;
        for (DWORD k = 0; k < count; k++) {
            BYTE *p = bptr(data[k]);
            if (!p || !geo::readable(p, 0x140) || !geo::class_of(p) || !kind_of_getter(p, 0x487620) || !model_loaded(p))
                continue;
            bool seen = false;
            for (int j = 0; j < g_nentities && !seen; j++)
                seen = g_entities[j] == p;
            if (!seen && g_nentities < MAX_ENTITIES)
                g_entities[g_nentities++] = p;
        }
    }
}

// The fake pool's items: 400 built once by the original constructor, restored before every side
constexpr int POOL_ITEMS = 400;
BYTE g_pool_items[POOL_ITEMS][ITEM];
BYTE g_pool_init[POOL_ITEMS][ITEM];
DWORD g_pool_data[POOL_ITEMS];
DWORD g_saved_pool[5];

void pool_on()
{
    memcpy(g_saved_pool, at<void *>(DRAW_POOL), 20);
    memcpy(g_pool_items, g_pool_init, sizeof g_pool_items);
    for (int i = 0; i < POOL_ITEMS; i++)
        g_pool_data[i] = pv(g_pool_items[i]);
    DWORD *p = at<DWORD *>(DRAW_POOL);
    p[1] = pv(g_pool_data);
    p[2] = POOL_ITEMS;
    p[3] = POOL_ITEMS;
    p[4] = 0;
    g_queue.n = g_queue_ground.n = 0;
}
void pool_off() { memcpy(at<void *>(DRAW_POOL), g_saved_pool, 20); }

DWORD map_item(DWORD v)
{
    for (int i = 0; i < POOL_ITEMS; i++)
        if (v == pv(g_pool_items[i]))
            return 0x17e40000u + (DWORD)i;
    return v;
}
void record_draw(Rec &r)
{
    const DWORD used = at<DWORD *>(DRAW_POOL)[4];
    r.add(used);
    for (DWORD i = 0; i < used && i < POOL_ITEMS; i++) {
        BYTE *it = g_pool_items[i];
        for (DWORD off = 4; off < 0x34; off += 4)
            r.add(map_item(d32(it, off)));
        r.add(fnv(2166136261u, it + 0x34, 0x1c));
    }
    for (int q = 0; q < 2; q++) {
        FakeList &l = q ? g_queue : g_queue_ground;
        r.add(0xf0f0u + (DWORD)l.n);
        for (int i = 0; i < l.n; i++)
            r.add(map_item(l.items[i]));
    }
}

// The entity state a draw changes (saved before and put back after every side): its flags, its children's,
// the walking overlay's sequence cache
struct EntityState {
    BYTE *parts[16];
    DWORD flags[16];
    int n;
    DWORD overlay[2];
    bool is_overlay;
    void save(BYTE *e, bool overlay_kind)
    {
        n = 0;
        parts[n] = e;
        flags[n++] = d32(e, 0x1c);
        for (BYTE *c = vcall<BYTE *>(e, 86, 0); c && n < 16; c = vcall<BYTE *>(e, 88, c, 0)) {
            parts[n] = c;
            flags[n++] = d32(c, 0x1c);
        }
        is_overlay = overlay_kind;
        if (is_overlay) {
            overlay[0] = d32(e, 0x134);
            overlay[1] = d32(e, 0x138);
        }
    }
    void restore() const
    {
        for (int i = 0; i < n; i++)
            d32(parts[i], 0x1c) = flags[i];
        if (is_overlay) {
            d32(parts[0], 0x134) = overlay[0];
            d32(parts[0], 0x138) = overlay[1];
        }
    }
    void record(Rec &r) const
    {
        for (int i = 0; i < n; i++)
            r.add(d32(parts[i], 0x1c));
        if (is_overlay) {
            r.add(d32(parts[0], 0x134));
            r.add(d32(parts[0], 0x138));
        }
    }
};

template <typename F> bool entity_sides(Tally &t, const char *what, BYTE *e, bool overlay, F call)
{
    EntityState st;
    st.save(e, overlay);
    if (g_trace)
        log("    warm-up");
    // the original once: what the drawing loads or caches on first use
    g_hooks.out();
    g_side = 0;
    pool_on();
    g_rec[0].reset();
    call(0);
    pool_off();
    g_hooks.in();
    st.restore();
    for (int side = 0; side < 2; side++) {
        g_rec[side].reset();
        g_side = side;
        if (side == 0)
            g_hooks.out();
        pool_on();
        if (g_trace)
            log("    side %d", side);
        const DWORD eax = call(side);
        g_rec[side].add(eax);
        record_draw(g_rec[side]);
        st.record(g_rec[side]);
        pool_off();
        if (side == 0)
            g_hooks.in();
        st.restore();
    }
    const Rec &a = g_rec[0], &b = g_rec[1];
    const bool same = g_hooks.fits && a.n == b.n && a.hash == b.hash;
    if (same) {
        t.check(true, "");
        return true;
    }
    int k = 0;
    while (k < a.n && k < b.n && a.v[k] == b.v[k])
        k++;
    char d[200];
    wsprintfA(d, "[%.100s: %d / %d values, first difference at %d: %x / %x]", what, a.n, b.n, k, k < a.n ? a.v[k] : 0, k < b.n ? b.v[k] : 0);
    t.check(false, "", d);
    return false;
}

void check_entities(Tally &t, Tally &lines)
{
    find_entities();
    if (!g_nentities) {
        log("  entity draw: no entities found");
        t.check(false, "", "[no entities]");
        return;
    }
    for (int i = 0; i < POOL_ITEMS; i++) {
        memset(g_pool_init[i], 0, ITEM);
        original<BYTE *(THISCALL *)(void *)>(0x488bb0)(g_pool_init[i]);
    }
    enum { PLAIN, OVERLAY, LINE, KINDS };
    int count[KINDS] = {}, index[KINDS] = {}, tested[KINDS] = {}, hits = 0;
    auto kind = [](BYTE *e) { return kind_of_getter(e, 0x4926c0) ? LINE : kind_of_getter(e, 0x49cd90) ? OVERLAY : PLAIN; };
    for (int i = 0; i < g_nentities; i++)
        count[kind(g_entities[i])]++;
    constexpr int CAP = 40;
    g_seed = 0x488d70;
    CodePatch dirty, ticks;
    patch_jump(dirty, 0x480fc0, (void *)&rec_dirty_add);
    patch_jump(ticks, 0x4d2650, (void *)&case_ticks24);
    for (int i = 0; i < g_nentities; i++) {
        BYTE *e = g_entities[i];
        const int kd = kind(e);
        if (!spread_pick(index[kd]++, count[kd], CAP))
            continue;
        tested[kd]++;
        const int x = d32(e, 0x54), y = d32(e, 0x58);
        for (int op = 0; op < 9; op++) {
            int clip[4], pos[3] = {0, 0, 0};
            const DWORD force = rnd(3) == 0;
            switch (rnd(4)) {
            case 0: clip[0] = x - 400, clip[1] = y - 500, clip[2] = x + 400, clip[3] = y + 300; break;
            case 1: clip[0] = x - rnd_range(0, 30), clip[1] = y - rnd_range(0, 60), clip[2] = x + rnd_range(0, 30), clip[3] = y + rnd_range(0, 20); break;
            case 2: clip[0] = x + 1000, clip[1] = y, clip[2] = x + 1200, clip[3] = y + 100; break;
            default:
                pos[0] = rnd_range(-50, 50), pos[1] = rnd_range(-50, 50), pos[2] = rnd_range(-10, 30);
                clip[0] = x - 300, clip[1] = y - 300, clip[2] = x + 300, clip[3] = y + 300;
            }
            // a fixed clock per case, spread over the 24 bits (not drawn from rnd: the cases stay those of before)
            g_case_ticks = 0x9e3779b9u * (DWORD)(i * 9 + op + 1);
            char what[96];
            wsprintfA(what, "entity %d (kind %d) op %d", i, kd, op);
            if (g_trace)
                log("  %s: class %s at (%d, %d)", what, geo::class_of(e) ? geo::class_of(e)->name : "?", x, y);
            if (op < 4) {
                entity_sides(t, what, e, kd == OVERLAY, [&](int) -> DWORD {
                    vcall<DWORD>(e, 23, &g_queue_ground, &g_queue, clip, force, pos);
                    return 0;
                });
            } else if (op < 6) {
                // another sequence of the model (the model list's record) with a sort extra
                BYTE *records = *at<BYTE **>(MODEL_RECORDS);
                BYTE *model = bptr(d32(records + d32(e, 0x74) * 0x80, 0x78));
                const DWORD nseq = d32(model, 0x48);
                const DWORD seq = nseq ? rnd(nseq) : 0;
                const int extra = rnd_range(-3, 3);
                entity_sides(t, what, e, kd == OVERLAY, [&](int) -> DWORD {
                    at<void(THISCALL *)(void *, void *, DWORD, void *, void *, int *, DWORD, int *, int)>(0x488d70)(e, model, seq, &g_queue_ground,
                                                                                                       &g_queue, clip, force, pos, extra);
                    return 0;
                });
            } else if (op == 6) {
                const DWORD flag = rnd(2);
                entity_sides(t, what, e, kd == OVERLAY, [&](int) -> DWORD {
                    exe(0x489a40, e, flag, (void *)0x12345678);
                    return 0;
                });
            } else {
                // HitTest at a point on or near the entity, every mode
                const int hx = x + rnd_range(-24, 24), hy = y + rnd_range(-60, 10);
                const int mode = op == 7 ? (int)rnd(4) : rnd_range(-1, 4);
                const bool hit = entity_sides(t, what, e, kd == OVERLAY, [&](int) -> DWORD { return exe(0x489cd0, e, hx, hy, mode) & 0xff; });
                hits += hit && (g_rec[0].v[0] & 0xff);
            }
        }
        if (kd == LINE) {
            // the line's debug drawing into a fake frame
            for (int k = 0; k < 2; k++) {
                begin_case(random());
                FrameSpec s;
                s.width = rnd_range(32, 160);
                s.height = rnd_range(32, 120);
                s.x = x - rnd_range(0, s.width);
                s.y = y - rnd_range(0, s.height);
                const int f = make_frame(s);
                if (f < 0)
                    continue;
                run_sides(lines, g_hooks, "CEntityAnimatedLine::DebugDraw", [&](int side) -> DWORD {
                    exe(0x492fc0, e, frame(side, f));
                    return 0;
                });
            }
        }
    }
    unpatch(ticks);
    unpatch(dirty);
    log("  entity draw: %d entities (plain %d of %d, walking overlay %d of %d, line %d of %d), %d hits", g_nentities, tested[PLAIN], count[PLAIN],
        tested[OVERLAY], count[OVERLAY], tested[LINE], count[LINE], hits);
}

}  // namespace

ZAX_CHECKS(entity_draw)
{
    (void)cls;
    (void)n;
    g_trace = r1::read_frame_trace();
    init_fakes();
    collect_sources(g_sources, 24);
    Tally items{"entity draw: draw item slots (fake frames, real sprite strips)"};
    check_items(items);
    items.report();
    Tally queues{"entity draw: draw queues (compare, sort, add, merge strips)"};
    check_queues(queues);
    queues.report();
    Tally pool{"entity draw: draw item pool"};
    check_pool(pool);
    pool.report();
    Tally real{"entity draw: real entities (slot 23, Entity_Draw, dirty rectangles, hit test)"}, lines{"entity draw: line debug drawing"};
    check_entities(real, lines);
    real.report();
    lines.report();
}

}  // namespace zax
