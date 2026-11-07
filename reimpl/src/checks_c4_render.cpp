// Checks of batch C4's render items, growing arrays and the rest (c4_render.cpp) against the original, in the
// game thread (machinery: checks_inventory_common.h; side 0 with the module's hooks out, the C heap from each
// side's arena). The frame is a block of our own whose recording vtable gives the clip rectangle (slot 36) and
// records the lines (slot 52, the floats by their bits), the image copies (23) and the fills (28); the line
// clipping and Table_SinCos are the exe's; DrawThickLinePts, CFont::DrawLines, the action's constructor and
// field setup, FindField, the quad nodes' and operator delete are recording stand-ins. The arrays grow on the
// arenas from empty through a run of adds (the element helpers real); the image list 0x6c1d50 and the four quad
// tree globals are saved, set to values of the case and put back.
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_c4_render(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
float frnd(int lo, int hi) { return (float)((int)rnd((DWORD)(hi - lo) * 8 + 1) + lo * 8) / 8.0f; }
DWORD fbits(float f)
{
    DWORD b;
    memcpy(&b, &f, 4);
    return b;
}

CodePatch g_p[24];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void begin(int side)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, 0x5d);
}
void end(int side)
{
    while (g_np)
        unpatch(g_p[--g_np]);
    arena_off();
    side_end(side);
}
BYTE g_obj[2][0x1000];
DWORD orel(DWORD v)
{
    volatile BYTE probe = 0;
    if (v - (ptrv((const void *)&probe) - 0x4000) < 0x8000)
        return 0x57ac0000u;
    const DWORD b = ptrv(g_obj[g_side]);
    return v - b < sizeof g_obj[0] ? 0x0b000000u + (v - b) : rel(v, g_side);
}
DWORD rect_hash(const void *p) { return fnv(2166136261u, p, 16); }

// The fakes' vtable
DWORD g_ret;
int g_clip[4];
DWORD THISCALL v0(void *s) { ev(0x100, orel(ptrv(s))); return g_ret; }
DWORD THISCALL v_rect(void *s, int *r)   // frame slot 36 / an item's slot 3: a rectangle of the case
{
    ev(0x124, orel(ptrv(s)));
    memcpy(r, g_clip, 16);
    return g_ret;
}
DWORD THISCALL v_copy(void *, DWORD img, int *src, int *dst, DWORD mask) { ev(0x117, orel(img), rect_hash(src), rect_hash(dst) ^ mask); return g_ret; }
DWORD THISCALL v_fill(void *s, int *r, DWORD c) { ev(0x11c, orel(ptrv(s)), rect_hash(r), c); return g_ret; }
DWORD THISCALL v_line(void *, DWORD x0, DWORD y0, DWORD x1, DWORD y1, DWORD c)
{
    ev(0x134, x0, y0, x1 ^ (y1 << 1) ^ (y1 >> 31));
    ev(0x135, y1, orel(c));
    return g_ret++;
}
DWORD THISCALL v1(void *s, DWORD a) { ev(0x101, orel(ptrv(s)), orel(a)); return g_ret; }
DWORD THISCALL v2(void *s) { ev(0x102, orel(ptrv(s))); return g_ret ^ 0x55; }
DWORD THISCALL v4(void *s, DWORD a, DWORD b, DWORD c) { ev(0x104, orel(ptrv(s)), orel(a), orel(b) ^ orel(c)); return g_ret; }
DWORD THISCALL v19(void *s) { ev(0x113, orel(ptrv(s))); return g_ret; }
DWORD THISCALL v20(void *s, DWORD a) { ev(0x114, orel(ptrv(s)), orel(a)); return g_ret; }
DWORD THISCALL v52(void *s) { ev(0x152, orel(ptrv(s))); return 0x52; }
DWORD THISCALL v53(void *s) { ev(0x153, orel(ptrv(s))); return 0x53; }
DWORD g_vt[64], g_frame_vt[64];
void vtables()
{
    for (int i = 0; i < 64; i++)
        g_vt[i] = g_frame_vt[i] = ptrv((void *)&v0);
    g_frame_vt[23] = ptrv((void *)&v_copy);
    g_frame_vt[28] = ptrv((void *)&v_fill);
    g_frame_vt[36] = ptrv((void *)&v_rect);
    g_frame_vt[52] = ptrv((void *)&v_line);
    g_vt[1] = ptrv((void *)&v1);
    g_vt[2] = ptrv((void *)&v2);
    g_vt[3] = ptrv((void *)&v_rect);
    g_vt[4] = ptrv((void *)&v4);
    g_vt[19] = ptrv((void *)&v19);
    g_vt[20] = ptrv((void *)&v20);
    g_vt[52] = ptrv((void *)&v52);
    g_vt[53] = ptrv((void *)&v53);
}

// Stand-ins
DWORD THISCALL s_thick(void *f, float *a, float *b, DWORD c)
{
    ev(0x401, orel(ptrv(f)), fbits(a[0]) ^ (fbits(a[1]) << 1), fbits(b[0]) ^ (fbits(b[1]) << 3));
    ev(0x402, fbits(a[1]), fbits(b[1]), c);
    return g_ret;
}
DWORD THISCALL s_lines(void *font, void *frame, DWORD a, DWORD b, const char *text, DWORD c, DWORD d)
{
    ev(0x403, orel(ptrv(font)), orel(ptrv(frame)), a ^ (b << 8));
    ev(0x404, fnv_text(2166136261u, text), c, d);
    return 0x404;
}
DWORD THISCALL s_quad(void *q) { ev(0x405, orel(ptrv(q))); return 5; }
void __cdecl s_delete(void *p) { ev(0x406, orel(ptrv(p))); }
DWORD THISCALL s_object_ctor(BYTE *o)
{
    ev(0x40d, orel(ptrv(o)));
    D(o, 4) = 0;
    return ptrv(o);
}
DWORD THISCALL s_vector_delete(BYTE *o, DWORD flags) { ev(0x40e, orel(ptrv(o)), flags); return ptrv(o); }
BYTE *__fastcall s_new(DWORD n)
{
    ev(0x40c, n);
    return g_obj[g_side] + 0x500;
}
DWORD THISCALL s_action_ctor(BYTE *a) { ev(0x407, orel(ptrv(a))); return ptrv(a); }
DWORD THISCALL s_init_fields(BYTE *a)
{
    ev(0x408, D(a, 0), D(a, 8), D(a, 0xc) ^ D(a, 0x10) ^ D(a, 0x14));
    if (D(a, 0) == 0x5f5220)
        D(a, 0) = ptrv(g_vt);   // (the convert wrapper's action: its own slots are fakes from here on)
    return 8;
}
DWORD g_field;
DWORD THISCALL s_find(DWORD ci, DWORD name, DWORD z)
{
    ev(0x409, ci, name, z);
    return rnd(3) ? g_field : 0;
}
DWORD THISCALL s_list_name(void *, DWORD i)
{
    ev(0x40a, i);
    static const char *const N[] = {"Site", "Sites/Big", "x.frm"};
    return ptrv(N[i % 3]);
}
DWORD THISCALL s_list_load(void *, const char *dir, const char *file, DWORD rec)
{
    ev(0x40b, fnv_text(2166136261u, dir), fnv_text(2166136261u, file), orel(rec));
    return 0x7a000000u | (orel(rec) & 0xffff);
}

// One check: build(side, k) on both sides, call, the event logs, results and the blocks compared
template <typename Build, typename Call>
void cases(Tally &t, const char *name, int n, Build build, Call call)
{
    for (int k = 0; k < n; k++) {
        const unsigned seed = (unsigned)k * 7919u + 17;
        DWORD e[2][4] = {};
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            memset(g_obj[side], 0, sizeof g_obj[side]);
            begin(side);
            g_ret = 0x30;
            build(side, k);
            call(side, e[side]);
            end(side);
        }
        char d[80];
        wsprintfA(d, "[case %d: %x %x / %x %x]", k, e[0][0], e[0][1], e[1][0], e[1][1]);
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && same_log() && same_region(g_obj[0], g_obj[1], sizeof g_obj[0]) && g_used[0] == g_used[1] &&
                    same_region(g_arena[0], g_arena[1], g_used[0]),
                name, d);
        if (!same_log())
            log_first_difference();
    }
}

BYTE *frame(int side)
{
    BYTE *f = g_obj[side] + 0xe00;
    D(f, 0) = ptrv(g_frame_vt);
    for (int i = 0; i < 4; i++)
        g_clip[i] = (int)rnd(900) - 100;
    if (g_clip[2] < g_clip[0])
        g_clip[2] = g_clip[0] + (int)rnd(50);
    return f;
}
void floats(BYTE *p, DWORD from, DWORD to, int lo, int hi)
{
    for (DWORD o = from; o <= to; o += 4) {
        const float f = frnd(lo, hi);
        memcpy(p + o, &f, 4);
    }
}

void test_items(Tally &t)
{
    cases(t, "line strip item draw", scaled(400), [](int side, int) {
        BYTE *s = g_obj[side];
        floats(s, 0x28, 0x34, -300, 1200);
        floats(s, 0x38, 0x3c, -8, 8);
        D(s, 0x40) = rnd(99), D(s, 0x50) = rnd(99);
    }, [](int side, DWORD *e) {
        BYTE *s = g_obj[side];
        e[0] = at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x4923e0)(s, frame(side)) & 0xff;
    });
    cases(t, "lightning item draw", scaled(400), [](int side, int) {
        BYTE *s = g_obj[side], *o = s + 0x200, *p = s + 0x600;
        D(s, 0x28) = ptrv(o);
        D(o, 0x78) = ptrv(p);
        floats(p, 0, 0x7c, -400, 900);
        floats(o, 0x84, 0x88, -6, 6);
        D(s, 0x2c) = rnd(10), D(s, 0x30) = rnd(14);
        D(s, 0x44) = rnd(1000) - 200, D(s, 0x48) = rnd(1000) - 200, D(s, 0x4c) = rnd(300);
        D(s, 0x50) = rnd(0x10000), D(s, 0x54) = rnd(0x10000);
    }, [](int side, DWORD *e) { e[0] = at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x4fe7d0)(g_obj[side], frame(side)); });
    cases(t, "lightning item source", scaled(400), [](int side, int) {
        BYTE *s = g_obj[side], *o = s + 0x200;
        stand_in(0x4b3cb0, (void *)&s_thick);
        D(s, 0x28) = ptrv(o);
        D(s, 0x2c) = rnd(4) ? 0 : 1 + rnd(5);
        floats(o, 0x4c, 0x60, -300, 900);
        floats(o, 0x8c, 0x8c, -4, 4);
        D(s, 0x44) = rnd(1000) - 200, D(s, 0x48) = rnd(1000) - 200, D(s, 0x4c) = rnd(300);
    }, [](int side, DWORD *e) { e[0] = at<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4fe930)(g_obj[side], frame(side), 0xc0105); });
    cases(t, "lightning item rect", scaled(100), [](int side, int) {
        BYTE *s = g_obj[side];
        for (DWORD o = 0x34; o <= 0x4c; o += 4)
            D(s, o) = rnd(2000) - 900;
    }, [](int side, DWORD *e) {
        int r[4];
        e[0] = orel(ptrv(at<int *(THISCALL *)(BYTE *, int *)>(0x4fead0)(g_obj[side], r)));
        e[1] = rect_hash(r);
    });
}

// The building site item: its owner (+0x28) with a text (+0x58, a ZString of the exe), a font +0x5c, an image
// index +0x60; the image list 0x6c1d50 a fake of the case
void test_site(Tally &t)
{
    static const char *const TEXT[] = {"", "Under construction", "Line 1\nLine 2"};
    for (int k = 0; k < scaled(300); k++) {
        SavedBytes saved;
        save_bytes(saved, 0x6c1d50, 0x40);
        const unsigned seed = (unsigned)k * 104729u + 3;
        DWORD e[2];
        DWORD lvt[64];
        for (int i = 0; i < 64; i++)
            lvt[i] = ptrv((void *)&v0);
        lvt[1] = ptrv((void *)&s_list_name);
        lvt[37] = ptrv((void *)&s_list_load);
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            memset(g_obj[side], 0, sizeof g_obj[side]);
            BYTE *s = g_obj[side], *o = s + 0x200, *recs = s + 0x400;
            D(s, 0) = ptrv(g_vt), D(o, 0) = ptrv(g_vt);
            D(s, 0x28) = ptrv(o);
            at<void(THISCALL *)(BYTE *, const char *)>(0x4dee00)(o + 0x58, TEXT[k % 3]);
            D(o, 0x5c) = ptrv(s + 0x300);
            D(o, 0x60) = rnd(8);
            const DWORD n = rnd(6);
            for (DWORD i = 0; i < n; i++) {
                D(recs, i * 0x80 + 0x78) = rnd(3) ? 0 : 0x5000 + i;
                recs[i * 0x80 + 0x7f] = rnd(6) ? 0 : 1;
            }
            *at<DWORD *>(0x6c1d50) = ptrv(lvt);
            *at<DWORD *>(0x6c1d80) = rnd(8) ? ptrv(recs) : 0;
            *at<DWORD *>(0x6c1d84) = n;
            begin(side);
            stand_in(0x4a66c0, (void *)&s_lines);
            g_ret = 0x30;
            BYTE *f = frame(side);
            e[side] = at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x58f100)(s, f);
            end(side);
            at<void(THISCALL *)(BYTE *)>(0x4defd0)(o + 0x58);
            D(o, 0x58) = 0;
            restore_bytes(saved);
        }
        char d[64];
        wsprintfA(d, "[case %d: %x / %x]", k, e[0], e[1]);
        t.check(e[0] == e[1] && same_log() && same_region(g_obj[0], g_obj[1], sizeof g_obj[0]), "building site item draw", d);
        if (!same_log())
            log_first_difference();
    }
}

// The arrays: empty, then 1-12 adds of random elements (the old elements copied on growth, the trigger array's
// old block deleted through its first element)
void test_arrays(Tally &t)
{
    cases(t, "lightning segment array add", scaled(200), [](int, int) {}, [](int side, DWORD *e) {
        BYTE *a = g_obj[side], *el = a + 0x100;
        const DWORD n = 1 + rnd(12);
        DWORD h = 0;
        for (DWORD i = 0; i < n; i++) {
            for (DWORD o = 0; o < 0x58; o += 4)
                D(el, o) = (DWORD)random();
            h = h * 31 + at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x500210)(a, el);
        }
        e[0] = h, e[1] = D(a, 8) ^ (D(a, 0xc) << 8), e[2] = orel(D(a, 4));
    });
    // (the elements' CObject constructor, field setup and vector deleting destructor are stand-ins: the real ones
    // keep the objects in the class's instance list)
    cases(t, "trigger array add", scaled(200), [](int, int) {
        stand_in(0x416610, (void *)&s_object_ctor);
        stand_in(0x4173c0, (void *)&s_init_fields);
        stand_in(0x583d30, (void *)&s_vector_delete);
    }, [](int side, DWORD *e) {
        BYTE *a = g_obj[side], *el = a + 0x100;
        const DWORD n = 1 + rnd(12);
        DWORD h = 0;
        for (DWORD i = 0; i < n; i++) {
            for (DWORD o = 0; o < 0x18; o += 4)
                D(el, o) = (DWORD)random();
            D(el, 4) = 0;   // (a CObject's +4 belongs to it: the destructor frees what it points to)
            h = h * 31 + at<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x5851a0)(a, el);
        }
        e[0] = h, e[1] = D(a, 8) ^ (D(a, 0xc) << 8), e[2] = orel(D(a, 4));
    });
}

// The convert wrapper: a new action (the constructor and the field setup stand-ins), the object a fake
void test_convert(Tally &t)
{
    cases(t, "convert wrapper", scaled(200), [](int side, int) {
        BYTE *s = g_obj[side], *obj = s + 0x200, *field = s + 0x300;
        stand_in(0x417710, (void *)&s_new);
        stand_in(0x41a6b0, (void *)&s_action_ctor);
        stand_in(0x4173c0, (void *)&s_init_fields);
        stand_in(0x4154d0, (void *)&s_find);
        D(obj, 0) = ptrv(g_vt), D(field, 0) = ptrv(g_vt);
        g_field = ptrv(field);
        D(s, 0xc) = rnd(3) ? 0x60a000 + rnd(16) : 0;
        D(s, 0x10) = 0x60b000 + rnd(16);
    }, [](int side, DWORD *e) {
        BYTE *s = g_obj[side];
        // (the object's class is asked without a null test when the wrapper names a field)
        e[0] = orel(ptrv(at<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x4c8280)(s, D(s, 0xc) || rnd(4) ? s + 0x200 : nullptr)));
    });
}

// The quad trees' atexit: the four globals (some null) freed through the stand-ins
void test_quads(Tally &t)
{
    for (int k = 0; k < scaled(40); k++) {
        SavedBytes saved;
        save_bytes(saved, 0x712248, 16);
        DWORD g[2][4];
        const unsigned seed = (unsigned)k * 31u + 1;
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (int i = 0; i < 4; i++)
                *at<DWORD *>(0x712248 + 4 * i) = rnd(3) ? ptrv(g_obj[side] + 0x100 * i) : 0;
            begin(side);
            stand_in(0x5396e0, (void *)&s_quad);
            stand_in(0x5d0320, (void *)&s_delete);
            at<DWORD(__cdecl *)()>(0x54fee0)();
            end(side);
            memcpy(g[side], at<void *>(0x712248), 16);
            restore_bytes(saved);
        }
        t.check(memcmp(g[0], g[1], 16) == 0 && same_log(), "quad trees at exit");
    }
}

// A resource list's slot 35: false in al
void test_slot35(Tally &t)
{
    DWORD e[2];
    for (int side = 0; side < 2; side++) {
        begin(side);
        e[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4d5000)(g_obj[side], 1, 2) & 0xff;
        end(side);
    }
    t.check(e[0] == e[1], "resource list slot 35");
}

}  // namespace

ZAX_CHECKS(c4_render)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_c4_render;
    if (!arena_setup()) {
        log("Check c4 render: no arena");
        return;
    }
    vtables();
    Tally items{"c4 render: the line strip, lightning and building site items"};
    test_items(items);
    test_site(items);
    items.report();
    Tally rest{"c4 render: the growing arrays, the convert wrapper, the quad trees' exit, slot 35"};
    test_arrays(rest);
    test_convert(rest);
    test_quads(rest);
    test_slot35(rest);
    rest.report();
}

}  // namespace zax
