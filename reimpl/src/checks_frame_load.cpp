// Checks of frame_load_convert.cpp, frame_load_filters.cpp and frame_load_rle.cpp (the frame load filters,
// batch R7) in the game thread; side 0 with the hooks of all three modules out.
//  - The depth conversion: Apply and To8/To16/To24 on fake plain frames of 16, 24 and 32 bits (random
//    pixels and alphas, real plain frames of the level) with and without dithering, and on fake compressed
//    frames whose blocks hold 3- or 4-byte pixels ("8-bit" and "16-bit" RLE kinds, any of the five
//    blocks; every row's codes add up to the width, no literal of length 0: the encoder never writes one
//    and the original loops 2^32 times on it).
//  - The row encoders and their length helpers on rows with runs, literals, clear stretches and the row
//    width (0x6be7e0) cut anywhere.
//  - The filter list: random sequences of Add (kinds 0-13), Remove, Find, Insert, CloneInto, locks and
//    depths on heap lists, compared by content; Run over a fake frame; the small filters' Apply on fake
//    frames (2, 3, 5, 7, 4 at depth 8, 10) and on heap frames (6, 4 at other depths), the clones.
//  - The RLE encoder's Apply on heap frames of 8, 16, 24 and 32 bits (alpha bands, dark and black
//    pixels) for the 8- and 16-bit encoders with and without a (dithering) conversion filter; every block
//    and table, the flags and origin, the bytes saved (0x6be828) and the scratch frame 0x6be7e8.
#include "checks_frame_r1.h"
#include "frame_load.h"

namespace zax {

int hooks_frame_load_convert(Hook *out);
int hooks_frame_load_filters(Hook *out);
int hooks_frame_load_rle(Hook *out);

namespace {

using namespace r1;
using render::FrameFilter;

int hooks_all(Hook *out)
{
    int n = hooks_frame_load_convert(out);
    n += hooks_frame_load_filters(out ? out + n : nullptr);
    n += hooks_frame_load_rle(out ? out + n : nullptr);
    return n;
}
ModuleHooks g_hooks{"frame_load", &hooks_all, {}, {}, {}, 0, true};

constexpr DWORD ROW_WIDTH = 0x6be7e0, BYTES_SAVED = 0x6be828, SCRATCH = 0x6be7e8, RLE_ON = 0x613d74;

Sources g_sources;

// ---- Fake compressed frames whose rows add up exactly ---------------------------------------------------------

BYTE *exact_row(BYTE *p, int w, DWORD ps, unsigned &s)
{
    for (int x = 0; x < w;) {
        const int left = w - x;
        const DWORD k = pattern_next(s) % 10;
        if (k < 3) {
            const int n = 1 + (int)(pattern_next(s) % (DWORD)(left < 127 ? left : 127));
            *p++ = (BYTE)(0x80 | n);
            x += n;
        } else if (k < 7) {
            const int n = 1 + (int)(pattern_next(s) % (DWORD)(left < 63 ? left : 63));
            *p++ = (BYTE)(0x40 | n);
            for (DWORD i = 0; i < (DWORD)n * ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
            x += n;
        } else {
            const int n = 1 + (int)(pattern_next(s) % (DWORD)(left < 63 ? left : 63));
            *p++ = (BYTE)n;
            for (DWORD i = 0; i < ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
            x += n;
        }
    }
    return p;
}

int make_exact_rle(DWORD flags, int w, int h, DWORD ps, DWORD mask, unsigned seed)
{
    int nb = 0;
    for (int b = 0; b < 5; b++)
        nb += (mask >> b) & 1;
    const DWORD worst = (DWORD)nb * ((DWORD)h * (DWORD)w * (1 + ps) + 8) + 64;
    if (g_h.nframes >= (int)MAX_FRAMES || g_h.next_object + sizeof(FrameData) > ARENA_OBJECTS || g_h.next_rows + (DWORD)(nb * h) > ARENA_ROWS ||
        g_h.next_pixels + GUARD * 2 + worst + 16 > ARENA_PIXELS || h < 1)
        return -1;
    Arena &a0 = g_h.arena[0], &a1 = g_h.arena[1];
    const DWORD start = g_h.next_pixels;
    BYTE *base = a0.pixels + start, *p = base + GUARD;
    DWORD *table = a0.rows + g_h.next_rows;
    FrameData *f = reinterpret_cast<FrameData *>(a0.objects + g_h.next_object);
    memset(f, 0, sizeof *f);
    f->vftable = vtable_for(0, VT_CFRAME, true);
    f->width = (WORD)w;
    f->height = (WORD)h;
    f->flags = flags;
    unsigned s = seed * 2654435761u + 17;
    if (!s)
        s = 1;
    for (int b = 0; b < 5; b++) {
        if (!((mask >> b) & 1))
            continue;
        BYTE *data = p;
        p += 4;
        for (int y = 0; y < h; y++) {
            table[y] = (DWORD)(p - data);
            p = exact_row(p, w, ps, s);
        }
        *reinterpret_cast<DWORD *>(data) = (DWORD)(p - data);
        while ((p - base) & 3)
            *p++ = 0;
        if (b == 0) {
            f->pixels = data;
            f->rows = table;
        } else {
            f->blocks[2 * b - 2] = data;
            f->blocks[2 * b - 1] = table;
        }
        table += h;
    }
    const DWORD bytes = (DWORD)(p - (base + GUARD));
    const DWORD total = (GUARD + bytes + GUARD + 15) & ~15u;
    fill_pattern(base, GUARD, seed * 31u + 1);
    fill_pattern(base + GUARD + bytes, total - GUARD - bytes, seed * 31u + 2);
    FrameInfo &fi = g_h.frames[g_h.nframes];
    fi = {g_h.next_object, start + GUARD, bytes, bytes, 1, 1};
    memcpy(a1.pixels + start, base, total);
    memcpy(a1.rows + g_h.next_rows, a0.rows + g_h.next_rows, (DWORD)(nb * h) * 4);
    FrameData *f1 = reinterpret_cast<FrameData *>(a1.objects + g_h.next_object);
    *f1 = *f;
    auto rebase = [&](void *q) -> void * {
        if (!q)
            return q;
        const DWORD v = ptrv(q);
        if (v >= ptrv(a0.pixels) && v < ptrv(a0.pixels) + ARENA_PIXELS)
            return a1.pixels + (v - ptrv(a0.pixels));
        return a1.rows + (v - ptrv(a0.rows)) / 4;
    };
    f1->pixels = static_cast<BYTE *>(rebase(f->pixels));
    f1->rows = static_cast<DWORD *>(rebase(f->rows));
    for (void *&q : f1->blocks)
        q = rebase(q);
    g_h.next_object += sizeof(FrameData);
    g_h.next_rows += (DWORD)(nb * h);
    g_h.next_pixels += total;
    return g_h.nframes++;
}

// A fake filter object of the case (both sides): vtable, kind, depth, param, the byte at +0x18
DWORD make_filter(DWORD vt, DWORD kind, int depth, DWORD param, BYTE extra)
{
    const DWORD off = alloc_object(0x20);
    for (int side = 0; side < 2; side++) {
        FrameFilter *f = object<FrameFilter>(side, off);
        memset(f, 0, 0x20);
        f->vftable = vt;
        f->kind = kind;
        f->depth = depth;
        f->param = param;
        reinterpret_cast<BYTE *>(f)[0x18] = extra;
    }
    return off;
}

// Pixels of a 32-bit fake frame: some solid stretches (runs), alpha bands, black and dark pixels
void shape_pixels(int fi, int kind)
{
    const FrameInfo &info = g_h.frames[fi];
    for (int side = 0; side < 2; side++) {
        unsigned s = g_h.seed * 7 + 3;
        BYTE *p = g_h.arena[side].pixels + info.buffer;
        DWORD colour = 0;
        for (DWORD i = 0; i + info.bpp <= info.bytes; i += info.bpp) {
            if (pattern_next(s) % 6 == 0)
                colour = pattern_next(s);
            DWORD v = colour;
            if (kind == 1 && pattern_next(s) % 4 == 0)
                v = pattern_next(s);
            static const BYTE ALPHAS[8] = {0, 255, 0xe5, 0xe6, 0xb2, 0x7f, 0x4c, 0x19};
            if (info.bpp == 4)
                v = (v & 0xffffff) | (DWORD)ALPHAS[pattern_next(s) % 8] << 24;
            if (pattern_next(s) % 9 == 0)
                v &= 0xff000000u | 0x0f0f0fu;   // dark
            memcpy(p + i, &v, info.bpp);
        }
    }
}

const Format CONVERT_FORMATS[3] = {RGB32, RGB24, RGB565};

void check_convert_plain(Tally &t)
{
    g_seed = 0x4ae740;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        FrameSpec s;
        s.format = CONVERT_FORMATS[k % 3];
        s.width = rnd_range(1, 40);
        s.height = rnd_range(1, 30);
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        if (s.format == RGB565 && k % 4 == 2 && g_sources.n)
            copy_source(fi, g_sources.list[rnd((DWORD)g_sources.n)].frame);
        else
            shape_pixels(fi, k & 1);
        static const DWORD DEPTHS[4] = {8, 16, 24, 32};
        const DWORD filter = make_filter(render::VT_FILTER_CONVERT, 11, -1, DEPTHS[rnd(4)], (BYTE)(rnd(3) == 0));
        const DWORD rect = alloc_object(16);
        run_sides(t, g_hooks, "FrameFilterConvert::Apply (plain)", [&](int side) -> DWORD {
            return at<DWORD(THISCALL *)(void *, void *, void *)>(0x4ae740)(object<void>(side, filter), frame(side, fi), object<void>(side, rect)) & 0xff;
        });
    }
}

void check_convert_rle(Tally &t)
{
    g_seed = 0x4ad1e0;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        const DWORD depth = rnd(2) ? F_32 : F_24, ps = depth == F_32 ? 4 : 3, rle = rnd(2) ? 2 : 4;
        const int fi = make_exact_rle(depth | rle | 0x2000, rnd_range(1, 40), rnd_range(1, 12), ps, 1 + rnd(31), g_h.seed);
        if (fi < 0)
            continue;
        const DWORD filter = make_filter(render::VT_FILTER_CONVERT, 11, -1, 8 << rnd(2), 0);
        const DWORD fn = k % 4 == 0 ? 0x4ad1e0 : k % 4 == 1 ? 0x4ad920 : k % 4 == 2 ? 0x4ae420 : 0;
        run_sides(t, g_hooks, "FrameConvert_To8/16/24 (RLE)", [&](int side) -> DWORD {
            void *f = object<void>(side, filter);
            if (!fn)
                return at<DWORD(THISCALL *)(void *, void *, void *)>(0x4ae740)(f, frame(side, fi), nullptr) & 0xff;
            return at<DWORD(THISCALL *)(void *, void *, void *)>(fn)(f, frame(side, fi), at<void *>(render::INV_COLOR15_HOLDER)) & 0xff;
        });
    }
}

// The row encoders and their helpers: the source row in a 32-, 16- or 8-bit fake frame, the output in a
// second one; x, size in the case's objects
void check_rows(Tally &t)
{
    g_seed = 0x4b0d80;
    const DWORD saved_width = *at<DWORD *>(ROW_WIDTH);
    static const DWORD ENCODERS[5] = {0x4b0d80, 0x4b10c0, 0x4b1400, 0x4b16d0, 0x4b18b0};
    static const DWORD HELPERS[6] = {0x4b1aa0, 0x4b1b30, 0x4b1b60, 0x4b1bd0, 0x4b1c00, 0x4b1c90};
    for (int k = 0; k < scaled(3000); k++) {
        begin_case(random());
        const int e = k % 5;
        FrameSpec s;
        s.format = e < 3 ? RGB32 : e == 3 ? PAL8 : RGB565;
        s.width = rnd_range(1, 200);
        s.height = 1;
        const int src = make_frame(s);
        FrameSpec o;
        o.format = PAL8;
        o.width = 8 * 200 + 64;
        o.height = 1;
        const int out = make_frame(o);
        if (src < 0 || out < 0)
            continue;
        shape_pixels(src, k & 1);
        if (e >= 3)   // clear pixels for the 8/16-bit sources
            for (int side = 0; side < 2; side++) {
                unsigned q = g_h.seed;
                BYTE *p = g_h.arena[side].pixels + g_h.frames[src].buffer;
                for (DWORD i = 0; i < g_h.frames[src].bytes; i++)
                    if (pattern_next(q) % 5 == 0)
                        p[i] = 0;
            }
        const DWORD state = alloc_object(8);
        const DWORD x0 = rnd(4) ? 0 : rnd((DWORD)s.width), width = rnd(5) ? (DWORD)s.width : rnd((DWORD)s.width + 1);
        for (int side = 0; side < 2; side++) {
            object<DWORD>(side, state)[0] = x0;
            object<DWORD>(side, state)[1] = 4;
        }
        *at<DWORD *>(ROW_WIDTH) = width;
        if (k % 3 == 2) {
            const DWORD h = HELPERS[rnd(6)], max = rnd(80);
            char what[96];
            wsprintfA(what, "RleEncode length helper 0x%x (x %u, max %u, width %u, format %d)", h, x0, max, width, s.format);
            run_sides(t, g_hooks, what, [&](int side) -> DWORD {
                return at<DWORD(__fastcall *)(DWORD, const BYTE *, DWORD)>(h)(x0, frame(side, src)->pixels, max);
            });
            continue;
        }
        run_sides(t, g_hooks, "RleEncode row encoders", [&](int side) -> DWORD {
            DWORD *st = object<DWORD>(side, state);
            at<void(__fastcall *)(const BYTE *, DWORD *, BYTE *, DWORD *, DWORD)>(ENCODERS[e])(frame(side, src)->pixels, &st[0], frame(side, out)->pixels,
                                                                                              &st[1], 0);
            return 0;
        });
    }
    *at<DWORD *>(ROW_WIDTH) = saved_width;
}

// ---- Heap filter lists and heap frames --------------------------------------------------------------------------

FrameFilter *new_head(int depth)
{
    FrameFilter *f = static_cast<FrameFilter *>(render::pool_new(0x14));
    at<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1cc0)(f, 0);
    f->depth = depth;
    return f;
}
void delete_list(FrameFilter *head)
{
    BYTE &chain = *at<BYTE *>(render::FILTER_DELETE_CHAIN);
    const BYTE saved = chain;
    chain = 1;
    vcall<void>(head, 0, 1);
    chain = saved;
}
void snap_list(Snap &s, const FrameFilter *head)
{
    int n = 0;
    for (const FrameFilter *f = head; f && n < 12; f = f->next, n++) {
        s.add(f->vftable);
        s.add(f->kind | (DWORD)f->locked << 16);
        s.add((DWORD)f->depth);
        if (f->vftable != render::VT_FILTER && f->kind != 2 && f->kind != 4 && f->kind != 6 && f->kind != 7 && f->kind != 10)
            s.add(f->param | (f->vftable == render::VT_FILTER_CONVERT ? (DWORD)reinterpret_cast<const BYTE *>(f)[0x18] << 24 : 0));
    }
    s.add((DWORD)n);
}

void check_lists(Tally &t)
{
    g_seed = 0x4b1db0;
    for (int k = 0; k < scaled(600); k++) {
        unsigned seed = random();
        compare_sides(t, g_hooks, "FrameFilterList ops", [&](int) -> Snap {
            Snap s;
            unsigned q = seed;
            FrameFilter *head = new_head(pattern_next(q) % 3 == 0 ? 16 : -1), *other = new_head(-1);
            for (int i = 0; i < 12; i++) {
                const DWORD op = pattern_next(q) % 10, kind = pattern_next(q) % 14;
                if (op < 5) {
                    at<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1db0)(head, kind);
                } else if (op == 5) {
                    s.add(at<DWORD(THISCALL *)(FrameFilter *, DWORD)>(0x4b20e0)(head, kind) & 0xff);
                } else if (op == 6) {
                    const FrameFilter *f = at<FrameFilter *(THISCALL *)(FrameFilter *, DWORD)>(0x4b2140)(head, kind);
                    s.add(f ? f->kind : 0xffff);
                } else if (op == 7) {
                    s.add((DWORD)at<int(THISCALL *)(FrameFilter *)>(0x4b2160)(head));
                    s.add((DWORD)at<int(THISCALL *)(FrameFilter *)>(0x4b2190)(head));
                } else if (op == 8) {
                    at<void(THISCALL *)(FrameFilter *, FrameFilter *)>(0x4b1d50)(head, other);
                    snap_list(s, other);
                } else {
                    head->locked = (BYTE)(pattern_next(q) % 4 == 0);
                }
            }
            head->locked = 0;
            snap_list(s, head);
            at<void(THISCALL *)(FrameFilter *)>(0x4b1d80)(other);
            snap_list(s, other);
            delete_list(other);
            delete_list(head);
            return s;
        });
    }
}

// A heap CFrame (the exe's constructor and SetSize) with pattern pixels; freed with the exe's destructor
render::CFrame *heap_frame(DWORD bits, DWORD w, DWORD h, unsigned seed)
{
    auto *f = static_cast<render::CFrame *>(heap_alloc(sizeof(render::CFrame)));
    at<void(THISCALL *)(void *)>(0x4a8a90)(f);
    at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, bits);
    unsigned s = seed | 1;
    const DWORD bpp = bits / 8, n = w * h * bpp;
    DWORD colour = 0;
    for (DWORD i = 0; i + bpp <= n; i += bpp) {
        if (pattern_next(s) % 5 == 0)
            colour = pattern_next(s);
        DWORD v = colour;
        static const BYTE ALPHAS[10] = {0, 255, 0xe5, 0xe6, 0xb2, 0x7f, 0x4c, 0x19, 0xfa, 4};
        if (bpp == 4)
            v = (v & 0xffffff) | (DWORD)ALPHAS[pattern_next(s) % 10] << 24;
        if (pattern_next(s) % 7 == 0)
            v &= bpp == 4 ? 0xff0f0f0fu : 0;
        memcpy(f->pixels + i, &v, bpp);
    }
    f->x = (short)(seed % 9) - 4;
    f->y = (short)(seed % 7) - 3;
    return f;
}
void free_heap_frame(render::CFrame *f)
{
    at<void(THISCALL *)(void *)>(0x4a8b00)(f);
    at<void(__cdecl *)(void *)>(0x5d0320)(f);
}
void snap_heap_frame(Snap &s, const render::CFrame *f)
{
    s.add(f->flags);
    s.add((DWORD)f->width | (DWORD)f->height << 16);
    s.add((DWORD)(WORD)f->x | (DWORD)(WORD)f->y << 16);
    if (!(f->flags & render::F_COMPRESSED)) {
        snap_frame(s, f);
        return;
    }
    for (int b = 0; b < 5; b++) {
        const BYTE *d = static_cast<const BYTE *>(b ? f->blocks[2 * b - 2] : f->pixels);
        const DWORD *tab = static_cast<const DWORD *>(b ? f->blocks[2 * b - 1] : f->rows);
        s.add_bytes(d, d ? *reinterpret_cast<const DWORD *>(d) : 0);
        s.add_bytes(tab, tab ? f->height * 4u : 0);
    }
}

void check_filters(Tally &t)
{
    g_seed = 0x4b2320;
    // the small filters' Apply on fake 32-bit (and 8/24-bit for kind 4) frames, through Run
    static const DWORD KINDS[6] = {2, 3, 5, 7, 4, 10};
    static const DWORD VTS[6] = {render::VT_FILTER2, render::VT_FILTER3, render::VT_FILTER5, render::VT_FILTER7, render::VT_FILTER4, render::VT_FILTER10};
    static const Format FMT4[3] = {PAL8, RGB24, RGB32};
    for (int k = 0; k < scaled(1800); k++) {
        begin_case(random());
        const int i = k % 5;
        FrameSpec s;
        s.format = i == 4 ? FMT4[rnd(3)] : i == 5 ? RGB24 : RGB32;
        s.width = rnd_range(1, 40);
        s.height = rnd_range(1, 30);
        s.x = rnd_range(-9, 9);
        s.y = rnd_range(-9, 9);
        const int fi = make_frame(s);
        if (fi < 0)
            continue;
        shape_pixels(fi, k & 1);
        const DWORD filter = make_filter(VTS[i], KINDS[i], i == 4 ? 8 : -1, i == 1 ? rnd(256) : rnd(40), 0);
        const DWORD rect = alloc_object(16);
        const render::Rect rc = {rnd_range(-5, 5), rnd_range(-5, 5), rnd_range(0, 50), rnd_range(0, 50)};
        for (int side = 0; side < 2; side++)
            *object<render::Rect>(side, rect) = rc;
        if (g_frame_trace)
            log("frame_load filter %d: kind %u format %d %dx%d", k, KINDS[i], s.format, s.width, s.height);
        const bool run = rnd(3) == 0 && i != 5;
        run_sides(t, g_hooks, run ? "FrameFilterList_Run" : "FrameFilter Apply (fake frames)", [&](int side) -> DWORD {
            FrameFilter *f = object<FrameFilter>(side, filter);
            DWORD r;
            if (run)
                r = at<DWORD(THISCALL *)(FrameFilter *, void *)>(0x4b21a0)(f, frame(side, fi));
            else
                r = vcall<DWORD>(f, 2, frame(side, fi), object<void>(side, rect));
            if (i == 5)
                drop_dynamics(frame(side, fi));
            return r & 0xff;
        });
    }
    // kinds 6 and 4 (depth 16 or 24) on heap frames; the clones
    for (int k = 0; k < scaled(400); k++) {
        const unsigned seed = random();
        const int i = k % 3;
        if (g_frame_trace)
            log("frame_load heap filter %d: %d", k, i);
        compare_sides(t, g_hooks, "FrameFilter Apply (heap frames), Clone", [&](int) -> Snap {
            Snap s;
            unsigned q = seed;
            const DWORD bits = i == 1 ? (pattern_next(q) % 2 ? 24 : 32) : i == 3 ? 24 : 32;
            render::CFrame *f = heap_frame(bits, 1 + pattern_next(q) % 30, 1 + pattern_next(q) % 20, seed);
            FrameFilter filter = {i == 1 ? render::VT_FILTER4 : render::VT_FILTER6, nullptr, i == 1 ? 4u : 6u, i == 1 ? 16 : -1, 0, {}, 0};
            if (i == 3) {   // kind 10: the palette built from the colours
                FrameFilter f10 = {render::VT_FILTER10, nullptr, 10, -1, 0, {}, 0};
                s.add(vcall<DWORD>(&f10, 2, f, nullptr) & 0xff);
                const render::Palette *pal = vcall<render::Palette *>(f, 58);
                s.add_bytes(pal ? pal->data : nullptr, 0x300);
                drop_dynamics(f);
            } else if (i == 2) {
                static const DWORD CLONE_VTS[9] = {render::VT_FILTER, render::VT_FILTER2, render::VT_FILTER3, render::VT_FILTER4, render::VT_FILTER5,
                                                   render::VT_FILTER6, render::VT_FILTER7, render::VT_FILTER10, render::VT_FILTER_CONVERT};
                BYTE raw[0x20] = {};
                FrameFilter *src = reinterpret_cast<FrameFilter *>(raw);
                *src = filter;
                src->vftable = CLONE_VTS[pattern_next(q) % 9];
                src->param = pattern_next(q) % 300;
                raw[0x18] = (BYTE)(pattern_next(q) & 1);
                FrameFilter *c = vcall<FrameFilter *>(src, 1);
                snap_list(s, c);
                vcall<void>(c, 0, 1);
            } else {
                s.add(vcall<DWORD>(&filter, 2, f, nullptr) & 0xff);
                snap_heap_frame(s, f);
            }
            free_heap_frame(f);
            return s;
        });
    }
}

// The RLE encoder's Apply on heap frames
void check_rle(Tally &t)
{
    g_seed = 0x4afd40;
    BYTE &on = *at<BYTE *>(RLE_ON);
    const BYTE saved_on = on;
    const DWORD saved_bytes = *at<DWORD *>(BYTES_SAVED), saved_width = *at<DWORD *>(ROW_WIDTH);
    static const DWORD BITS[4] = {32, 24, 16, 8};
    for (int k = 0; k < scaled(500); k++) {
        const unsigned seed = random();
        const DWORD bits = BITS[k % 4 == 3 ? rnd(4) : 0];
        const DWORD param = rnd(2), depth = 8 << rnd(2), with_convert = rnd(3), dither = rnd(2);
        const int real = k % 5 == 4 && g_sources.n ? (int)rnd((DWORD)g_sources.n) : -1;
        on = 1;
        if (g_frame_trace)
            log("frame_load rle %d: bits %u param %u depth %u convert %u dither %u real %d", k, bits, param, depth, with_convert, dither, real);
        compare_sides(t, g_hooks, "FrameFilterRle::Apply", [&](int) -> Snap {
            Snap s;
            *at<DWORD *>(BYTES_SAVED) = 0;
            unsigned q = seed;
            const FrameData *rs = real >= 0 ? g_sources.list[real].frame : nullptr;
            const bool use_real = rs && !(rs->flags & F_COMPRESSED);
            const DWORD b = use_real ? ((rs->flags & F_DEPTH) == F_8 ? 8 : 16) : bits;
            render::CFrame *f = heap_frame(b, use_real ? rs->width : 1 + pattern_next(q) % 48, use_real ? rs->height : 1 + pattern_next(q) % 24, seed);
            if (use_real)
                for (DWORD y = 0; y < rs->height; y++)
                    memcpy(f->pixels + f->rows[y] * (b / 8), rs->pixels + rs->rows[y] * (b / 8), rs->width * (b / 8));
            BYTE conv[0x20] = {};
            FrameFilter *c = reinterpret_cast<FrameFilter *>(conv);
            *c = {render::VT_FILTER_CONVERT, nullptr, 11, -1, 0, {}, depth};
            conv[0x18] = (BYTE)dither;
            FrameFilter rle = {0x5f4924, with_convert ? c : nullptr, 8, (int)depth, 0, {}, param};
            s.add(vcall<DWORD>(&rle, 2, f, nullptr) & 0xff);
            snap_heap_frame(s, f);
            s.add(*at<DWORD *>(BYTES_SAVED));
            snap_heap_frame(s, at<render::CFrame *>(SCRATCH));
            free_heap_frame(f);
            return s;
        });
    }
    on = saved_on;
    *at<DWORD *>(BYTES_SAVED) = saved_bytes;
    *at<DWORD *>(ROW_WIDTH) = saved_width;
}

}  // namespace

ZAX_CHECKS(frame_load)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    collect_sources(g_sources, 6);
    Tally plain{"frame load: the depth conversion of plain frames"};
    check_convert_plain(plain);
    plain.report();
    Tally rle{"frame load: the depth conversion of RLE frames"};
    check_convert_rle(rle);
    rle.report();
    Tally rows{"frame load: the RLE row encoders and length helpers"};
    check_rows(rows);
    rows.report();
    Tally lists{"frame load: the filter list"};
    check_lists(lists);
    lists.report();
    Tally filters{"frame load: the filters 2-7 and 10, Run, the clones"};
    check_filters(filters);
    filters.report();
    Tally enc{"frame load: the RLE encoder (Apply)"};
    check_rle(enc);
    enc.report();
}

}  // namespace zax
