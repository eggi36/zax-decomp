// Checks of the text overlays and the image field's default (frame_overlay.cpp) against the original, in the
// game thread.
//  - The classes CFrameOverlayTextOverlay and CFrameOverlayOptions (machinery: checks_inventory_common.h):
//    Create, CreateArray, the deleting destructors, the getters, RegisterFields, the registered classes; the
//    out-of-line constructor 0x4b56a0 and LocalEntry_Assign 0x4b58c0 on blocks of their own.
//  - The overlay array (+8 of the options): Add (0x4b5540) of 1-12 overlays with random fields and texts to an
//    array of its own, growing it; compared: count, capacity, every element's fields and both strings; then the
//    array's slot 0 (0x4b5870) with 0 and 1, the options' destructor with the filled array.
//  - The drawing (0x4b52e0): options of their own with 0-4 overlays whose fonts are the level's loaded fonts
//    (only indices whose record holds its font, so nothing loads), texts, hot spots, widths, spacings and
//    justifications, into fake 5-6-5 frames (the frame check harness). Compared: pixels, frame calls, eax (the
//    overlay count when there are overlays).
//  - The image field's default (0x4b59b0) with the two registered image descriptors (CLayerSaveData "Fog Of
//    War", CZaxSaveGameStats "Screen Shot"; copies, as the fill caches the colour's palette index in it) on
//    objects of their own holding a CFrame (constructor 0x4a8a90, destroyed with 0x4a8b00); compared: the frame's dwords except the buffers, the pixels hashed, eax.
#include "checks_hud_common.h"
#include "checks_inventory_common.h"
#include "checks_frame_r1.h"
#include "ai_helpers.h"

namespace zax {

int hooks_frame_overlay(Hook *out);

namespace {

using namespace ai;
using r1::rnd;
using r1::rnd_range;

const invcheck::ClassDef CLASSES[] = {
    {"CFrameOverlayTextOverlay", 0x28, 0x6bfafc, 0x4b4ab0, 0x4b4af0, 0x4b4c70, {0x4b4a60, 0x4b4c10}, true},
    {"CFrameOverlayOptions", 0x18, 0x6bfb00, 0x4b4fe0, 0x4b5030, 0x4b5200, {0x4b4f90, 0x4b51a0}, true},
};

framec::ModuleHooks g_hooks{"frame_overlay", &hooks_frame_overlay, {}, {}, {}, 0, true};

template <typename F> void compare_sides(Tally &t, const char *what, F f)
{
    unsigned h[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            g_hooks.out();
        h[side] = f();
        if (side == 0)
            g_hooks.in();
    }
    char d[160];
    wsprintfA(d, "[%.100s: %x / %x]", what, h[0], h[1]);
    t.check(g_hooks.fits && h[0] == h[1], "", d);
}

const char *TEXTS[] = {"", "Hello", "The quick brown fox jumps over the lazy dog", "A\nB C", "  spaced   words  ", "1234567890 1234567890 1234567890"};

// An overlay made by the original's Create with random fields and texts (the caller deletes it)
BYTE *make_overlay(unsigned seed)
{
    BYTE *o = original<BYTE *(__cdecl *)()>(0x4b4ab0)();
    g_seed = seed;
    for (DWORD off = 8; off < 0x20; off += 4)
        dw(o, off) = random() % 300;
    zs_assign_text(o + 0x20, TEXTS[random() % 6]);
    zs_assign_text(o + 0x24, TEXTS[random() % 6]);
    return o;
}
void delete_overlay(BYTE *o) { original<void *(THISCALL *)(void *, DWORD)>(0x4b4b60)(o, 1); }

unsigned array_hash(BYTE *a)
{
    unsigned h = fnv(2166136261u, a + 8, 8);
    h = fnv(h, a, 4);
    BYTE *d = bp(dw(a, 4));
    h = fnv(h, &(d ? dw(d - 4, 0) : (DWORD &)h), d ? 4 : 0);
    // only the used elements: the constructor leaves the hot spot of the others as the heap had it
    for (DWORD i = 0; d && i < dw(a, 8); i++) {
        BYTE *e = d + i * 0x28;
        h = fnv(h, e, 0x20);
        h = fnv_text(h, zs_c_str(e + 0x20));
        h = fnv_text(h, zs_c_str(e + 0x24));
    }
    return h;
}

void check_array(Tally &t)
{
    for (int k = 0; k < scaled(80); k++) {
        g_seed = 0x4b5540 + (unsigned)k;
        const int n = 1 + (int)(random() % 12);
        const unsigned base = random();
        const DWORD mode = random() % 3;
        DWORD steps[2][16];
        int side_no = 0;
        char what[96];
        wsprintfA(what, "overlay array Add, case %d, %d overlays, mode %d", k, n, mode);
        compare_sides(t, what, [&]() -> unsigned {
            DWORD *st = steps[side_no++];
            BYTE *opts = static_cast<BYTE *>(original<void *(__cdecl *)()>(0x4b4fe0)());
            BYTE *a = opts + 8;
            unsigned h = 2166136261u;
            for (int i = 0; i < n; i++) {
                BYTE *o = make_overlay(base + (unsigned)i);
                at<void(THISCALL *)(void *, void *)>(0x4b5540)(a, o);
                delete_overlay(o);
                const unsigned ah = array_hash(a);
                st[i] = ah ^ dw(a, 8) << 24 ^ dw(a, 0xc) << 16;
                h = fnv(h, &ah, 4);
            }
            if (mode == 0) {
                at<void *(THISCALL *)(void *, DWORD)>(0x4b50b0)(opts, 1);
            } else {
                // the array's slot 0 alone, then the options without elements
                BYTE copy[0x10];
                memcpy(copy, a, 0x10);
                at<void *(THISCALL *)(void *, DWORD)>(0x4b5870)(copy, 0);
                h = fnv(h, copy, 0x10);
                dw(a, 4) = dw(a, 8) = dw(a, 0xc) = 0;
                at<void *(THISCALL *)(void *, DWORD)>(0x4b50b0)(opts, 1);
            }
            return h;
        });
        for (int i = 0; i < n; i++)
            if (steps[0][i] != steps[1][i]) {
                log("    first differing step %d: %x / %x", i, steps[0][i], steps[1][i]);
                break;
            }
    }
    // the out-of-line constructor and LocalEntry_Assign on blocks of their own
    for (int k = 0; k < scaled(40); k++) {
        g_seed = 0x4b56a0 + (unsigned)k;
        const unsigned pattern = random();
        compare_sides(t, "TextOverlay Ctor, LocalEntry_Assign", [&]() -> unsigned {
            BYTE *o = static_cast<BYTE *>(pool_alloc(0x28));
            for (int i = 0; i < 0x28; i++)
                o[i] = (BYTE)(pattern >> (i & 3) * 8);
            BYTE *r = at<BYTE *(THISCALL *)(void *)>(0x4b56a0)(o);
            unsigned h = fnv(2166136261u, o, 0x20);
            h ^= (r == o);
            BYTE *from = make_overlay(pattern);
            BYTE *r2 = at<BYTE *(THISCALL *)(void *, void *)>(0x4b58c0)(o + 0x20, from + 0x20);
            h = fnv_text(fnv_text(h, zs_c_str(o + 0x20)), zs_c_str(o + 0x24)) ^ (r2 == o + 0x20) << 1;
            delete_overlay(from);
            delete_overlay(o);
            return h;
        });
    }
}

// The fonts whose records hold their object (indices into the font list)
DWORD g_loaded[32];
int g_nloaded;
void find_loaded_fonts()
{
    g_nloaded = 0;
    BYTE *records = *at<BYTE **>(0x6be210);
    const DWORD n = records ? *at<DWORD *>(0x6be214) : 0;
    for (DWORD i = 0; i < n && g_nloaded < 32; i++)
        if (dw(records + i * 0x80, 0x78) && !records[i * 0x80 + 0x7f])
            g_loaded[g_nloaded++] = i;
}

void check_draw(Tally &t)
{
    g_seed = 0x4b52e0;
    if (!g_nloaded || g_loaded[0] != 0) {
        // index 0 is what out-of-range indices fall back to; it must hold its font too
        log("  frame overlay: font 0 not loaded, no drawing cases");
        return;
    }
    int drawn = 0;
    for (int k = 0; k < scaled(300); k++) {
        framec::begin_case(random());
        framec::FrameSpec s;
        s.format = framec::RGB565;
        s.width = rnd_range(40, 320);
        s.height = rnd_range(20, 200);
        const int fi = framec::make_frame(s);
        if (fi < 0)
            continue;
        const int n = (int)rnd(5);
        BYTE *opts = static_cast<BYTE *>(original<void *(__cdecl *)()>(0x4b4fe0)());
        for (int i = 0; i < n; i++) {
            BYTE *o = make_overlay(random());
            dw(o, 8) = rnd(8) ? g_loaded[rnd((DWORD)g_nloaded)] : 0x7fff;
            dw(o, 0xc) = (DWORD)rnd_range(-20, s.width);
            dw(o, 0x10) = (DWORD)rnd_range(-20, s.height);
            dw(o, 0x14) = (DWORD)rnd_range(20, 260);
            dw(o, 0x18) = (DWORD)rnd_range(-2, 6);
            dw(o, 0x1c) = rnd(3);
            original<void(THISCALL *)(void *, void *)>(0x4b5540)(opts + 8, o);
            delete_overlay(o);
        }
        char what[64];
        wsprintfA(what, "Draw case %d: %d overlays", k, n);
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            const DWORD r = at<DWORD(THISCALL *)(void *, void *)>(0x4b52e0)(opts, framec::frame(side, fi));
            return n ? r : 0;
        });
        original<void *(THISCALL *)(void *, DWORD)>(0x4b50b0)(opts, 1);
        drawn++;
    }
    log("  frame overlay: %d loaded fonts, %d draws", g_nloaded, drawn);
}

CClassInfo **g_cls;
int g_ncls;

void check_image_default(Tally &t)
{
    static const char *const NAMES[] = {"Fog Of War", "Screen Shot"};
    int found = 0;
    for (int ci = 0; ci < g_ncls; ci++) {
        CClassInfo *c = g_cls[ci];
        if (!c)
            continue;
        for (const char *name : NAMES) {
            BYTE *real = find_field(c, (DWORD)(uintptr_t)name, 0);
            if (!real || dw(real, 0) != 0x5f4ce8)
                continue;
            // a copy: filling the frame caches the colour's palette index in the colour (+0x40 flags)
            BYTE copy[0x44];
            memcpy(copy, real, 0x44);
            BYTE *d = copy;
            found++;
            for (int k = 0; k < 4; k++) {
                const DWORD off = dw(d, 0x10);
                compare_sides(t, name, [&]() -> unsigned {
                    BYTE obj[0x400];
                    memset(obj, 0x3c, sizeof obj);
                    if (off + 0x40 > sizeof obj)
                        return 0;
                    at<void *(THISCALL *)(void *)>(0x4a8a90)(obj + off);
                    if (k & 1)
                        at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(obj + off, 3, 2, 16);
                    const DWORD r = at<DWORD(THISCALL *)(void *, void *)>(0x4b59b0)(d, obj);
                    BYTE *fr = obj + off;
                    unsigned h = fnv(2166136261u, fr + 8, 4);
                    h = fnv(h, fr + 0x10, 6);
                    h = fnv(h, fr + 0x3c, 4);
                    h = fnv(h, &r, 4);
                    if (BYTE *px = bp(dw(fr, 0xc)))
                        h = fnv(h, px, (DWORD)*reinterpret_cast<WORD *>(fr + 0x12) * *reinterpret_cast<WORD *>(fr + 0x14) * vcall<DWORD>(fr, 30));
                    at<void(THISCALL *)(void *)>(0x4a8b00)(fr);
                    return h;
                });
            }
        }
    }
    log("  frame overlay: %d image descriptors", found);
}

}  // namespace

ZAX_CHECKS(frame_overlay)
{
    invcheck::g_hook_table = &hooks_frame_overlay;
    g_cls = cls;
    g_ncls = n;
    r1::read_frame_trace();
    if (!invcheck::arena_setup()) {
        log("Check frame overlay: no arena");
        return;
    }
    Tally objects{"frame overlay: Create, CreateArray, destructors"}, getters{"frame overlay: class getters"},
        fields{"frame overlay: RegisterFields"}, registered{"frame overlay: registered descriptors and classes (real data)"};
    invcheck::test_classes(objects, getters, fields, registered, CLASSES, 2);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    Tally arr{"frame overlay: the overlay array, the constructor, LocalEntry_Assign"};
    check_array(arr);
    arr.report();
    find_loaded_fonts();
    Tally draw{"frame overlay: CFrameOverlayOptions::Draw into fake frames"};
    check_draw(draw);
    draw.report();
    Tally img{"frame overlay: the image field's default"};
    check_image_default(img);
    img.report();
}

}  // namespace zax
