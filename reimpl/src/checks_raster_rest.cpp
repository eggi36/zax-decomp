// Checks of raster_rest.cpp (the rest of batch R7, done with R6) in the game thread, side 0 with the module's
// hooks out:
//  - PolyRaster_Draw (raster_poly.cpp) on fake frames of 24 bits (all types), 16 and 8 bits (type 5, the
//    texture; also the others) with fake polygons inside, across and outside the frame, origins and pitch
//    padding, a power-of-two texture of the frame's depth;
//  - the whole-polygon fills by their exe address on polygons inside the frame (they do not clip): the six
//    24-bit fills, the 16-bit texture fill, the 8-bit texture triangles, and the original green fills
//    0x549870 / 0x5495a0 through their trampolines on both sides, so their helpers 0x54c8e0, 0x54c790 and
//    0x549820 (this module) are the ones compared;
//  - filter 10's Apply on heap frames of 24 and 32 bits (the exe's constructor and SetSize, the palette
//    the frame makes on request): al and the palette's 0x300 bytes.
#include "checks_frame_r1.h"
#include "blit24.h"

WINIMPORT DWORD WINAPI WaitForSingleObject(HANDLE h, DWORD ms);

namespace zax {

int hooks_raster_rest(Hook *out);

namespace {

using namespace r1;

ModuleHooks g_hooks{"raster_rest", &hooks_raster_rest, {}, {}, {}, 0, true};

struct Pt {
    int x, y;
};
struct PolyObj {   // the polygon's layout (raster_poly.cpp)
    DWORD vftable;
    DWORD pts_vt;
    Pt *data;
    DWORD count, capacity;
    DWORD type;
    void *param;
};

constexpr DWORD MMX_FLAG = 0x7121f4;

int coord(int size, bool inside)
{
    if (inside)
        return rnd(6) == 0 ? (rnd(2) ? 0 : (size - 1) * 16) : rnd_range(0, (size - 1) * 16);
    switch (rnd(8)) {
    case 0: return 0;
    case 1: return size * 16;
    case 2: return rnd_range(-400, size * 16 + 400);
    case 3: return rnd_range(-3000, 3000);
    default: return rnd_range(-40, size * 16 + 40);
    }
}

// A fake polygon of the case in the objects (both sides); returns its offset
DWORD make_poly(int w, int h, DWORD type, DWORD texture, bool inside, int &n)
{
    n = rnd(6) ? rnd_range(3, 8) : rnd_range(0, 12);
    const DWORD off = alloc_object(sizeof(PolyObj)), pts = alloc_object(12 * 8);
    if (off == 0xffffffffu || pts == 0xffffffffu)
        return 0xffffffffu;
    Pt p[12];
    const int shape = rnd(6);
    for (int i = 0; i < n; i++) {
        p[i] = {coord(w, inside), coord(h, inside)};
        if (shape == 0 && i && !inside)   // collinear
            p[i] = {p[0].x + (p[1 % n].x - p[0].x) * i, p[0].y + (p[1 % n].y - p[0].y) * i};
        if (shape == 1 && i) {   // sub-pixel
            p[i] = {p[0].x + rnd_range(-20, 20), p[0].y + rnd_range(-20, 20)};
            if (inside) {
                p[i].x = p[i].x < 0 ? 0 : p[i].x > (w - 1) * 16 ? (w - 1) * 16 : p[i].x;
                p[i].y = p[i].y < 0 ? 0 : p[i].y > (h - 1) * 16 ? (h - 1) * 16 : p[i].y;
            }
        }
        if (shape == 2 && i == 2)   // a flat edge
            p[i].y = p[1].y;
    }
    for (int side = 0; side < 2; side++) {
        PolyObj *o = object<PolyObj>(side, off);
        o->vftable = 0x5fd35c;
        o->pts_vt = 0x5f8dc4;
        o->data = object<Pt>(side, pts);
        memcpy(o->data, p, sizeof p);
        o->count = (DWORD)n;
        o->capacity = 12;
        o->type = type;
        o->param = texture != 0xffffffffu ? object<void>(side, texture) : nullptr;
    }
    return off;
}

struct Case {
    int fi, tex;
    DWORD poly;
    int n;
};
// A frame (with an origin and padding unless `plain`), a texture of its depth, a polygon
bool make_case(Case &c, Format f, DWORD type, bool inside)
{
    FrameSpec s;
    s.format = f;
    s.width = rnd_range(1, 48);
    s.height = rnd_range(1, 40);
    s.pitch = rnd(3) ? 0 : s.width + rnd_range(1, 5);
    if (!inside && rnd(4) == 0) {
        s.x = rnd_range(-3, 3);
        s.y = rnd_range(-3, 3);
    }
    c.fi = make_frame(s);
    FrameSpec ts;
    ts.format = f;
    ts.width = 1 << rnd_range(0, 4);
    ts.height = 1 << rnd_range(0, 4);
    c.tex = make_frame(ts);
    if (c.fi < 0 || c.tex < 0)
        return false;
    c.poly = make_poly(s.width, s.height, type, g_h.frames[c.tex].object, inside, c.n);
    return c.poly != 0xffffffffu;
}

void check_draw(Tally &t)
{
    g_seed = 0x549d50;
    BYTE &mmx = *at<BYTE *>(MMX_FLAG);
    const BYTE saved_mmx = mmx;
    for (int k = 0; k < scaled(2400); k++) {
        begin_case(random());
        const Format f = k % 4 < 2 ? RGB24 : k % 4 == 2 ? RGB565 : PAL8;
        const DWORD type = f == RGB24 || rnd(3) == 0 ? rnd(6) : 5;
        Case c;
        if (!make_case(c, f, type, false))
            continue;
        const bool use_mmx = rnd(2);
        char what[80];
        wsprintfA(what, "PolyRaster_Draw %s type %u points %d mmx %d", format_name(f), type, c.n, use_mmx);
        if (g_frame_trace)
            log("raster_rest: %s", what);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            mmx = use_mmx;
            at<void(THISCALL *)(void *, void *)>(0x549050)(object<void>(side, c.poly), frame(side, c.fi));
            return 0;
        });
    }
    mmx = saved_mmx;
}

// The whole-polygon fills by address on polygons inside the frame
struct Whole {
    DWORD fn;
    Format f;
    bool texture, original;   // original: the trampoline on both sides (the green fills of raster_poly)
    const char *name;
};
constexpr Whole WHOLE[] = {
    {0x549d50, RGB24, false, false, "PolyFill24_Black"},  {0x549fe0, RGB24, false, false, "PolyFill24_HalfBrightness"},
    {0x54a200, RGB24, false, false, "PolyFill24_Blue"},   {0x54a640, RGB24, false, false, "PolyFill24_Green"},
    {0x54a420, RGB24, false, false, "PolyFill24_Red"},    {0x54a860, RGB24, true, false, "PolyFill24_Texture"},
    {0x549aa0, RGB565, true, false, "PolyFill16_Texture"}, {0x549870, RGB565, false, true, "PolyFill16_Green (original)"},
    {0x5495a0, RGB565, false, true, "PolyFill16_Green_MMX (original)"}, {0x54b230, PAL8, true, false, "PolyFill8_Texture"},
};

void check_whole(Tally &t)
{
    g_seed = 0x549aa0;
    for (int k = 0; k < scaled(3000); k++) {
        begin_case(random());
        const Whole &w = WHOLE[(DWORD)k % (sizeof WHOLE / sizeof WHOLE[0])];
        Case c;
        if (!make_case(c, w.f, 0, true))
            continue;
        char what[80];
        wsprintfA(what, "%s points %d", w.name, c.n);
        if (g_frame_trace)
            log("raster_rest: %s", what);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            PolyObj *poly = object<PolyObj>(side, c.poly);
            void *f = frame(side, c.fi), *tex = frame(side, c.tex);
            if (w.fn == 0x54b230) {
                for (DWORD i = 1; i + 1 < poly->count; i++)
                    at<void(__fastcall *)(void *, void *, const Pt *, DWORD, DWORD, DWORD, DWORD)>(0x54b230)(f, tex, poly->data, 0, i, i + 1, 0);
                return 0;
            }
            const DWORD fn = w.original ? ptrv(original_address(w.fn)) : w.fn;
            reinterpret_cast<void(__fastcall *)(void *, void *, void *, DWORD)>(fn)(poly, f, w.texture ? tex : nullptr, 0);
            return 0;
        });
    }
}

// ---- Filter 10 -----------------------------------------------------------------------------------------------------

render::CFrame *heap_frame(DWORD bits, DWORD w, DWORD h, unsigned seed)
{
    auto *f = static_cast<render::CFrame *>(heap_alloc(sizeof(render::CFrame)));
    at<void(THISCALL *)(void *)>(0x4a8a90)(f);
    at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, bits);
    unsigned s = seed | 1;
    const DWORD bpp = bits / 8, n = w * h * bpp;
    DWORD colour = 0;
    for (DWORD i = 0; i + bpp <= n; i += bpp) {
        if (pattern_next(s) % 4 == 0)
            colour = pattern_next(s) & (pattern_next(s) % 3 ? 0xffffffffu : 0x3f3f3f3fu);
        memcpy(f->pixels + i, &colour, bpp);
    }
    return f;
}
void free_heap_frame(render::CFrame *f)
{
    at<void(THISCALL *)(void *)>(0x4a8b00)(f);
    heap_free(f);
}

// Both sides keep the 1 MB histogram on the stack (_chkstk 0x100000; the reimplementation too since batch
// decomp-faithful2): more than the main thread's whole reserve (SizeOfStackReserve 0x100000), so it overflows
// the game thread's stack; both sides run on a thread with a 4 MB stack, and every eighth case also on one of
// 1 MB, where both overflow
struct F10 {
    render::CFrame *f;
    DWORD al;
};
DWORD WINAPI f10_thread(void *p)
{
    F10 *c = static_cast<F10 *>(p);
    DWORD filter[6] = {0x5f4988, 0, 10, 0xffffffffu, 0, 0};
    c->al = vcall<DWORD>(filter, 2, c->f, nullptr) & 0xff;
    return 0;
}

DWORD g_f10_overflows, g_f10_tries;   // the stack overflow cases: overflows seen, calls (both sides)
void check_filter10(Tally &t)
{
    g_f10_overflows = g_f10_tries = 0;
    g_seed = 0x4b2b50;
    for (int k = 0; k < scaled(120); k++) {
        const unsigned seed = random();
        compare_sides(t, g_hooks, "FrameFilter10::Apply", [&](int) -> Snap {
            Snap s;
            unsigned q = seed;
            const DWORD bits = pattern_next(q) % 2 ? 24 : 32;
            F10 c = {heap_frame(bits, 1 + pattern_next(q) % 60, 1 + pattern_next(q) % 40, seed), 0xffffffffu};
            HANDLE h = CreateThread(nullptr, 0x400000, &f10_thread, &c, 0x10000 /* STACK_SIZE_PARAM_IS_A_RESERVATION */, nullptr);
            if (h) {
                WaitForSingleObject(h, 0xffffffffu);
                CloseHandle(h);
            }
            s.add(c.al);
            const render::Palette *pal = vcall<render::Palette *>(c.f, 58);
            s.add_bytes(pal ? pal->data : nullptr, 0x300);
            if (k % 8 == 0) {
                // On a stack of the main thread's size (SizeOfStackReserve 0x100000) the 1 MB histogram does not
                // fit: _chkstk's probe runs into the end of the stack, a stack overflow on both sides (batch
                // decomp-faithful2; the original's crash, docs/ORIGINAL_BUGS.md 2.27). Compared: the exception's
                // code and access kind (the address is the end of each thread's own stack).
                DWORD filter[6] = {0x5f4988, 0, 10, 0xffffffffu, 0, 0};
                const DWORD args[2] = {(DWORD)(uintptr_t)c.f, 0};
                Fault fault;
                call_faulting_on_thread(fault, 0x100000, at<void *>(0x4b2b50), (DWORD)(uintptr_t)filter, args, 2);
                s.add(fault.code);
                s.add(fault.kind);
                g_f10_overflows += fault.code == 0xc00000fdu;
                g_f10_tries++;
            }
            drop_dynamics(c.f);
            free_heap_frame(c.f);
            return s;
        });
    }
    char d[100];
    wsprintfA(d, "[stack overflows on a 1 MB stack: %u of %u calls]", g_f10_overflows, g_f10_tries);
    t.check(g_f10_tries > 0 && g_f10_overflows == g_f10_tries, "FrameFilter10::Apply", d);
    log("  filter 10: %s", d);
}

}  // namespace

ZAX_CHECKS(raster_rest)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    char ini[MAX_PATH + 32];
    GetModuleFileNameA(NULL, ini, MAX_PATH);
    for (int k = lstrlenA(ini); k > 0 && ini[k - 1] != '\\'; k--)
        ini[k - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    render::g_span24_mutation = (int)GetPrivateProfileIntA("Reimpl", "Span24Mutation", 0, ini);
    Tally draw{"raster_rest: PolyRaster_Draw on 24-bit frames and textures (8, 16, 24 bits)"};
    check_draw(draw);
    draw.report();
    Tally whole{"raster_rest: the fills and helpers by address (24-bit fills, textures, the green helpers)"};
    check_whole(whole);
    whole.report();
    Tally f10{"raster_rest: filter 10 (the palette from a colour histogram)"};
    check_filter10(f10);
    f10.report();
}

}  // namespace zax
