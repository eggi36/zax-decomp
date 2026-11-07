// Checks of render_poly.cpp against the original, in the game thread (machinery: checks_inventory_common.h:
// side 0 with the module's hooks out, operator new and free on an arena per side, an event log of the
// stand-ins).
//  - The class: Create, CreateArray, slot 1, the getter, RegisterFields into a fake class info and the
//    registered descriptors (real data).
//  - Slots 78 and 23 on objects constructed in the arena (0x55d880) with random YSort, render types 0-8,
//    texture indices, flags (On Ground, the sort key bit, 0x200000, 0x400000) and polygons (the default
//    triangle, more points inserted), views inside, across and outside the bounds, both queues or none: the
//    lists' records (0x6c1d50, 0x713c80) swapped for fake ones (objects, missing, loading), recording
//    stand-ins for DefinitionList_Object 0x48d8f0 (a fake object whose slot 23 records), LoadRecord
//    0x55e090, CWaterWarpEffectFrame::Update 0x598420 and DrawQueue_Add 0x49ca30; the bounds, the polygon,
//    the relisting and the points' conversion are the exe's.
//  - The draw item's slots: 1 with a fake frame (slot 37 the reporter's +4 or not) and point counts 0-5
//    (PolyRaster_Draw 0x549050 recording), 3, and 6 on items the constructor made (flags 0 and 1).
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_render_poly(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CRenderablePolygon", 0x9c, 0x713100, 0x55d300, 0x55d320, 0x55d4a0, {0x55d2a0, 0}, true},
};

DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }
inline DWORD &dwa(BYTE *p, DWORD off) { return *reinterpret_cast<DWORD *>(p + off); }

struct Stand {
    DWORD address;
    void *fn;
};
template <int N> struct Patches {
    CodePatch p[N];
    void in(const Stand (&s)[N])
    {
        for (int i = 0; i < N; i++)
            patch_jump(p[i], s[i].address, s[i].fn);
    }
    void out()
    {
        for (int i = N; i--;)
            unpatch(p[i]);
    }
};

// ---- Stand-ins ---------------------------------------------------------------------------------------

BYTE g_fake_obj[16];
DWORD g_fake_vt[32];
DWORD THISCALL s_obj_slot23(void *o, DWORD arg)
{
    ev(0x23, o == g_fake_obj, arg);
    return result(0x23);
}
void *THISCALL s_list_object(void *list, DWORD index)
{
    ev(0x48d8f0, ptrv(list), index);
    return g_fake_obj;
}
// The fake records: 0x80 bytes each (+0x78 the object, +0x7f loading)
constexpr DWORD NREC = 6;
BYTE g_records[2][NREC * 0x80];
DWORD THISCALL s_load_record(void *list, DWORD index)
{
    ev(0x55e090, ptrv(list), index);
    BYTE *rec = ptrv(list) == 0x6c1d50 ? g_records[0] : g_records[1];
    if (index < NREC)
        dwa(rec, index * 0x80 + 0x78) = 0x7e000000u + index;
    return result(0x90);
}
DWORD __fastcall s_warp_update(void *effect)
{
    ev(0x598420, ptrv(effect));
    return 0x7d000000u + (ptrv(effect) & 0xff);
}
DWORD THISCALL s_queue_add(void *queue, void *item)
{
    ev(0x49ca30, ptrv(queue), rel(ptrv(item), g_side));
    return result(0xad);
}
DWORD THISCALL s_raster_draw(void *points, void *frame)
{
    ev(0x549050, rel(ptrv(points), g_side), ptrv(frame) & 0xff);
    return result(0x50);
}
const Stand STANDS[] = {
    {0x48d8f0, (void *)&s_list_object}, {0x55e090, (void *)&s_load_record}, {0x598420, (void *)&s_warp_update},
    {0x49ca30, (void *)&s_queue_add},   {0x549050, (void *)&s_raster_draw},
};
Patches<5> g_patches;

// The two lists' records swapped for fake ones during a case
struct ListSwap {
    DWORD saved[4];
    void in()
    {
        saved[0] = *at<DWORD *>(0x6c1d80);
        saved[1] = *at<DWORD *>(0x6c1d84);
        saved[2] = *at<DWORD *>(0x713cb0);
        saved[3] = *at<DWORD *>(0x713cb4);
    }
    void set(bool none5, bool none7)
    {
        *at<DWORD *>(0x6c1d80) = none5 ? 0 : ptrv(g_records[0]);
        *at<DWORD *>(0x6c1d84) = NREC;
        *at<DWORD *>(0x713cb0) = none7 ? 0 : ptrv(g_records[1]);
        *at<DWORD *>(0x713cb4) = NREC;
    }
    void out()
    {
        *at<DWORD *>(0x6c1d80) = saved[0];
        *at<DWORD *>(0x6c1d84) = saved[1];
        *at<DWORD *>(0x713cb0) = saved[2];
        *at<DWORD *>(0x713cb4) = saved[3];
    }
};
void fill_records(unsigned seed)
{
    g_seed = seed;
    for (int l = 0; l < 2; l++) {
        memset(g_records[l], 0, sizeof g_records[l]);
        for (DWORD i = 0; i < NREC; i++) {
            g_records[l][i * 0x80 + 0x7f] = rnd(5) == 0 ? 1 : 0;
            dwa(g_records[l], i * 0x80 + 0x78) = rnd(3) ? 0x7c000000u + i * 0x10 + (DWORD)l : 0;
        }
    }
}

// ---- Slots 78 and 23 --------------------------------------------------------------------------------------

char g_detail[220];
DWORD g_r[2];

// An object constructed in the side's arena, its fields set from the case's random values
BYTE *make_part(unsigned seed)
{
    g_seed = seed;
    BYTE *o = static_cast<BYTE *>(arena_new(0x9c));
    at<BYTE *(__fastcall *)(BYTE *)>(0x55d880)(o);
    dwa(o, 0x74) = rnd(4) ? rnd(200) - 100 : random();
    dwa(o, 0x78) = rnd(9);
    dwa(o, 0x7c) = rnd(4);
    dwa(o, 0x80) = rnd(NREC + 2);
    dwa(o, 0x84) = rnd(NREC + 2);
    dwa(o, 0x88) = rnd(4);
    dwa(o, 0x1c) = (rnd(2) ? 0x200000 : 0) | (rnd(2) ? 0x400000 : 0) | (rnd(16) << 4);
    dwa(o, 0x98) = random();
    const DWORD extra = rnd(4);
    for (DWORD i = 0; i < extra; i++)
        at<DWORD(THISCALL *)(void *, float, float, DWORD)>(0x5466c0)(o + 0x4c, (float)(int)rnd(600) - 100.0f, (float)(int)rnd(600) - 100.0f,
                                                                     0xffffffff);
    if (extra)
        at<DWORD(THISCALL *)(void *)>(0x546810)(o + 0x4c);
    return o;
}

void check_slots(Tally &t)
{
    ListSwap lists;
    lists.in();
    g_fake_vt[23] = ptrv((void *)&s_obj_slot23);
    *reinterpret_cast<DWORD *>(g_fake_obj) = ptrv(g_fake_vt);
    for (int k = 0; k < scaled(1500); k++) {
        const unsigned seed = 0x55dd90u + (unsigned)k;
        const bool sort_only = k % 4 == 0;
        g_seed = seed * 7;
        const bool none5 = rnd(6) == 0, none7 = rnd(6) == 0;
        int view[4];
        switch (rnd(4)) {
        case 0: view[0] = -1000, view[1] = -1000, view[2] = 1000, view[3] = 1000; break;
        case 1: view[0] = 400, view[1] = 400, view[2] = 900, view[3] = 900; break;
        default:
            view[0] = (int)rnd(400) - 50, view[1] = (int)rnd(400) - 50;
            view[2] = view[0] + (int)rnd(300), view[3] = view[1] + (int)rnd(300);
        }
        void *ground = rnd(5) ? at<void *>(0x6c1bc0) : nullptr, *objects = rnd(5) ? at<void *>(0x6c1ca0) : nullptr;
        const BYTE flag = (BYTE)rnd(2);
        for (int side = 0; side < 2; side++) {
            fill_records(seed * 3);
            lists.set(none5, none7);
            log_reset(side);
            arena_on(side, 0x6d);
            BYTE *o = make_part(seed);
            g_patches.in(STANDS);
            side_begin(side);
            if (sort_only)
                at<void(THISCALL *)(void *)>(0x55dc30)(o);
            else
                at<void(THISCALL *)(void *, void *, void *, const int *, DWORD, DWORD)>(0x55dd90)(o, ground, objects, view, flag, random());
            side_end(side);
            g_patches.out();
            ev(0xf00d, rel(*at<DWORD *>(0x6c1d84), side), fnv(2166136261u, g_records, sizeof g_records));
            g_r[side] = ptrv(o) - ptrv(g_arena[side]);
            arena_off();
        }
        const DWORD n = g_used[0] > g_used[1] ? g_used[0] : g_used[1];
        const bool arena_same = same_region(g_arena[0], g_arena[1], n);
        wsprintfA(g_detail, "[%s seed %u: arena %s (at %x: %x / %x), log %d / %d]", sort_only ? "slot 78" : "slot 23", seed, arena_same ? "same" : "DIFFERS",
                  g_diff_at, g_diff_a, g_diff_b, g_nlog[0], g_nlog[1]);
        const bool same = arena_same && same_log() && g_r[0] == g_r[1] && !g_overflow;
        if (!same)
            log_first_difference();
        t.check(same, "", g_detail);
        g_overflow = false;
        // the objects destroyed (their polygons' pool blocks); the arena's frees are ignored
        for (int side = 0; side < 2; side++) {
            arena_on(side, 0x6d, false);
            vcall<void *>(g_arena[side] + g_r[side], 1, 0);
            arena_off();
        }
    }
    lists.out();
}

// ---- The draw item's slots ------------------------------------------------------------------------------

DWORD g_frame_vt[64];
DWORD g_frame_answer;
DWORD THISCALL s_frame37(void *f)
{
    ev(0xf37, ptrv(f) & 0xff);
    return g_frame_answer;
}

void check_item(Tally &t)
{
    g_frame_vt[37] = ptrv((void *)&s_frame37);
    alignas(4) BYTE frame[16] = {};
    *reinterpret_cast<DWORD *>(frame) = ptrv(g_frame_vt);
    for (int k = 0; k < scaled(600); k++) {
        const unsigned seed = 0x55d210u + (unsigned)k;
        const int what = k % 3;
        for (int side = 0; side < 2; side++) {
            log_reset(side);
            arena_on(side, 0x3c);
            g_seed = seed;
            BYTE *item = static_cast<BYTE *>(arena_new(0x54));
            for (DWORD i = 0; i < 0x54; i += 4)
                dwa(item, i) = random();
            DWORD out[4] = {1, 2, 3, 4};
            g_patches.in(STANDS);
            side_begin(side);
            if (what == 0) {
                dwa(item, 0x34) = rnd(6);
                g_frame_answer = rnd(2) ? dwa(*at<BYTE **>(0x6c02cc), 4) : random();
                at<void(THISCALL *)(void *, void *)>(0x55d210)(item, frame);
            } else if (what == 1) {
                at<void(THISCALL *)(void *, DWORD *)>(0x55d250)(item, out);
            } else {
                // an item as the constructor makes it (its points empty)
                BYTE *o = static_cast<BYTE *>(arena_new(0x9c));
                at<BYTE *(__fastcall *)(BYTE *)>(0x55d880)(o);
                BYTE *made = at<BYTE *>(dwa(o, 0x8c));
                const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x55d970)(made, rnd(2) | (rnd(2) ? 0xfe : 0));
                ev(0x55d970, rel(r, side));
                dwa(o, 0x8c) = 0;
                vcall<void *>(o, 1, 0);
            }
            side_end(side);
            g_patches.out();
            ev(0xb0, out[0] ^ out[1] * 3, out[2] ^ out[3] * 5);
            arena_off();
        }
        const DWORD n = g_used[0] > g_used[1] ? g_used[0] : g_used[1];
        const bool arena_same = same_region(g_arena[0], g_arena[1], n);
        wsprintfA(g_detail, "[item slot %d seed %u: arena %s (at %x: %x / %x), log %d / %d]", what == 0 ? 1 : what == 1 ? 3 : 6, seed,
                  arena_same ? "same" : "DIFFERS", g_diff_at, g_diff_a, g_diff_b, g_nlog[0], g_nlog[1]);
        const bool same = arena_same && same_log() && !g_overflow;
        if (!same)
            log_first_difference();
        t.check(same, "", g_detail);
        g_overflow = false;
    }
}

}  // namespace

ZAX_CHECKS(render_poly)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_render_poly;
    if (!arena_setup()) {
        Tally setup{"renderable polygon: setup"};
        setup.check(false, "", "[no arena]");
        setup.report();
        return;
    }
    Tally objects{"renderable polygon: Create, CreateArray, slot 1"}, getters{"renderable polygon: class getter"},
        fields{"renderable polygon: RegisterFields"}, real{"renderable polygon: registered descriptors"};
    test_classes(objects, getters, fields, real, CLASSES, 1);
    objects.report();
    getters.report();
    fields.report();
    real.report();
    Tally slots{"renderable polygon: slots 78 and 23"};
    check_slots(slots);
    slots.report();
    Tally item{"renderable polygon: the draw item's slots 1, 3, 6"};
    check_item(item);
    item.report();
}

}  // namespace zax
