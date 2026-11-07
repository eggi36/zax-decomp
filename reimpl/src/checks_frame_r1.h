// Helpers of the R1 frame checks (checks_frame_class.cpp, checks_frame_draw.cpp, checks_frame_proxy.cpp,
// checks_blend_core.cpp) on top of the frame check harness (checks_frame_common.h):
//  - synthetic RLE frames (both sides, in the harness arenas): rows of skip, literal and run codes with
//    1-3 bytes per pixel, any of the five blocks, some rows starting with a 63-pixel segment (the x = 63
//    quirk of the pixel tests);
//  - saved exe memory (global tables a function writes): saved once, put back after each side, compared
//    by hash between the sides, put back at the end, so the running game keeps its state;
//  - compare_sides(): a side's result as a list of values (heap contents hashed, pointers as null / not
//    null) for the functions that allocate (frames, palettes, registries), side 0 with the module's hooks
//    out;
//  - a fake stream (slots 2 write and 3 read) for the opacity slots;
//  - dropping the dynamic properties (CFrame slot 58's palette holder) a call added to a fake object.
#pragma once
#include "checks_frame_common.h"
#include "frame.h"

namespace zax::r1 {

using namespace framec;

inline DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }
inline int rnd_range(int lo, int hi) { return lo + (int)rnd((DWORD)(hi - lo + 1)); }

template <typename R = DWORD, typename... A> inline R call_slot(void *obj, int i, A... a)
{
    return reinterpret_cast<R(THISCALL *)(void *, A...)>((*static_cast<DWORD **>(obj))[i])(obj, a...);
}

// [Reimpl] FrameTrace= of Zax-Reimpl.ini (checks only): 1 logs every case of the traced loops before it runs
inline int g_frame_trace;
inline int read_frame_trace()
{
    char ini[MAX_PATH + 32];
    GetModuleFileNameA(NULL, ini, MAX_PATH);
    for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
        ini[n - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    return g_frame_trace = (int)GetPrivateProfileIntA("Reimpl", "FrameTrace", 0, ini);
}

// ---- Synthetic RLE frames ------------------------------------------------------------------------------------

inline BYTE *rle_row(BYTE *p, int w, DWORD ps, unsigned &s, bool start63)
{
    int x = 0;
    if (start63) {
        const bool lit = pattern_next(s) & 1;
        *p++ = (BYTE)(lit ? 0x40 | 0x3f : 0x3f);
        const DWORD n = lit ? 0x3f * ps : ps;
        for (DWORD i = 0; i < n; i++)
            *p++ = (BYTE)(pattern_next(s) >> 9);
        x = 0x3f;
    }
    while (x < w) {
        const DWORD k = pattern_next(s) % 20;
        if (k < 6) {
            const DWORD n = 1 + pattern_next(s) % 24;
            *p++ = (BYTE)(0x80 | n);
            x += (int)n;
        } else if (k < 13) {
            const DWORD n = 1 + pattern_next(s) % 14;
            *p++ = (BYTE)(0x40 | n);
            for (DWORD i = 0; i < n * ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
            x += (int)n;
        } else if (k < 19) {
            const DWORD n = 1 + pattern_next(s) % 14;
            *p++ = (BYTE)n;
            for (DWORD i = 0; i < ps; i++)
                *p++ = (BYTE)(pattern_next(s) >> 9);
            x += (int)n;
        } else {
            // a code of length 0
            const DWORD kind = pattern_next(s) % 3;
            *p++ = (BYTE)(kind == 0 ? 0x80 : kind == 1 ? 0x40 : 0);
            if (kind == 2)
                for (DWORD i = 0; i < ps; i++)
                    *p++ = (BYTE)(pattern_next(s) >> 9);
        }
    }
    return p;
}

// A compressed frame (flags: depth | 2 or 4 | extra) of w x h with the blocks of `mask` (bit 0 pixels, 1 the
// extra block +0x1c, 2 the third +0x24, 3 alpha +0x2c, 4 edge +0x34), `ps` bytes per pixel in literals and runs.
// Returns the frame index or -1.
inline int make_rle_frame(DWORD flags, int w, int h, DWORD ps, DWORD mask, unsigned seed)
{
    int nb = 0;
    for (int b = 0; b < 5; b++)
        nb += (mask >> b) & 1;
    const DWORD worst = (DWORD)nb * ((DWORD)h * (DWORD)(w + 0x40) * (1 + ps) + 8) + 64;
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
    f->log2_width = log2_of(f->width);
    f->log2_height = log2_of(f->height);
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
            p = rle_row(p, w, ps, s, w > 0x3f && pattern_next(s) % 5 == 0);
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
    // side 1: the same bytes, pointers rebased
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

// A real compressed frame still sound right before it is copied (the sources are collected once per module;
// by then other checks may have changed the level): every block readable, every row offset inside its block
inline bool compressed_sane(const FrameData *f)
{
    if (!geo::readable(f, sizeof *f) || !(f->flags & F_COMPRESSED) || !compressed_bytes(f))
        return false;
    for (int b = 0; b < 5; b++) {
        const BYTE *d = block_data(f, b);
        const DWORD *t = block_table(f, b);
        if (!d)
            continue;
        if (!t)
            return false;
        const DWORD n = *reinterpret_cast<const DWORD *>(d);
        for (DWORD y = 0; y < f->height; y++)
            if (t[y] >= n)
                return false;
    }
    return true;
}

// ---- Saved exe memory ---------------------------------------------------------------------------------------

struct SavedMemory {
    static constexpr int MAX = 24;
    DWORD addr[MAX], size[MAX];
    BYTE *copy[MAX];
    int n = 0;
    void add(DWORD a, DWORD bytes)
    {
        if (n >= MAX)
            return;
        addr[n] = a;
        size[n] = bytes;
        copy[n] = static_cast<BYTE *>(heap_alloc(bytes));
        memcpy(copy[n], at<void *>(a), bytes);
        n++;
    }
    void restore()
    {
        for (int i = 0; i < n; i++)
            memcpy(at<void *>(addr[i]), copy[i], size[i]);
    }
    unsigned hash() const
    {
        unsigned h = 2166136261u;
        for (int i = 0; i < n; i++)
            h = fnv(h, at<const void *>(addr[i]), size[i]);
        return h;
    }
    void release()
    {
        restore();
        for (int i = 0; i < n; i++)
            heap_free(copy[i]);
        n = 0;
    }
    ~SavedMemory() { release(); }
};

// ---- Comparing results with heap contents -----------------------------------------------------------------------

struct Snap {
    DWORD v[48];
    int n = 0;
    void add(DWORD x)
    {
        if (n < 48)
            v[n++] = x;
    }
    void add_bytes(const void *p, DWORD bytes) { add(p ? fnv(2166136261u, p, bytes) : 0xdead0000u); }
    bool operator==(const Snap &o) const { return n == o.n && !memcmp(v, o.v, (DWORD)n * 4); }
};

// f(side) returns a Snap; side 0 with the module's hooks out
template <typename F> inline bool compare_sides(Tally &t, ModuleHooks &hooks, const char *what, F f)
{
    Snap s[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks.out();
        s[side] = f(side);
        if (side == 0)
            hooks.in();
    }
    const bool same = hooks.fits && s[0] == s[1];
    if (same) {
        t.check(true, "");
        return true;
    }
    int k = 0;
    while (k < s[0].n && k < s[1].n && s[0].v[k] == s[1].v[k])
        k++;
    char d[200];
    wsprintfA(d, "[%.100s: %d / %d values, first difference at %d: %x / %x]", what, s[0].n, s[1].n, k, k < s[0].n ? s[0].v[k] : 0,
              k < s[1].n ? s[1].v[k] : 0);
    t.check(false, "", d);
    return false;
}

// A heap frame's state: the plain fields, the pixel buffer (width * height * bytes per pixel for a plain
// frame) and the row table (max(height, 2) entries) by content, the block pointers as present or not
inline void snap_frame(Snap &s, const render::CFrame *f)
{
    s.add(f->vftable);
    s.add(f->object_04 ? 1 : 0);
    s.add((DWORD)(WORD)f->x | (DWORD)(WORD)f->y << 16);
    s.add((DWORD)f->log2_width | (DWORD)f->log2_height << 8 | (DWORD)f->width << 16);
    s.add(f->height);
    s.add(f->flags);
    DWORD bpp = 0;
    switch (f->flags & render::F_DEPTH) {
    case render::F_8: bpp = 1; break;
    case render::F_16: bpp = 2; break;
    case render::F_24: bpp = 3; break;
    case render::F_32: bpp = 4; break;
    }
    s.add_bytes(f->pixels, (DWORD)f->width * f->height * bpp);
    s.add_bytes(f->rows, (f->height < 2 ? 2u : f->height) * 4u);
    DWORD present = 0;
    for (int b = 0; b < 8; b++)
        present |= (f->blocks[b] ? 1u : 0u) << b;
    s.add(present);
}

// ---- A fake stream for the opacity slots (slot 2 write (data, n), slot 3 read (data, n)) ---------------------------

struct FakeStream {
    DWORD vftable;
    BYTE data[16];
    DWORD written, read_from;
    DWORD calls[4];
};
inline DWORD THISCALL fake_stream_write(FakeStream *s, const void *p, DWORD n)
{
    memcpy(s->data, p, n < 16 ? n : 16);
    s->written = n;
    s->calls[0]++;
    return n ^ 0x5a;
}
inline DWORD THISCALL fake_stream_read(FakeStream *s, void *p, DWORD n)
{
    memcpy(p, s->data, n < 16 ? n : 16);
    s->read_from = n;
    s->calls[1]++;
    return n ^ 0xa5;
}
inline DWORD g_fake_stream_vtable[4];
inline void init_fake_stream(FakeStream *s)
{
    g_fake_stream_vtable[2] = ptrv(ptr(&fake_stream_write));
    g_fake_stream_vtable[3] = ptrv(ptr(&fake_stream_read));
    s->vftable = ptrv(g_fake_stream_vtable);
}

// ---- Dynamic properties a call added to a fake object -----------------------------------------------------------

// CObject's destructor frees the dynamic property list (and the values in it); the vtable put back
inline void drop_dynamics(void *obj)
{
    DWORD *d = static_cast<DWORD *>(obj);
    if (!d[1])
        return;
    const DWORD vt = d[0];
    render::obj_dtor(obj);
    d[0] = vt;
    d[1] = 0;
}

// The CBlendCopy object copied into the case's objects (its registry shared, read only): FindBlitter writes
// only the copy's cache
inline DWORD copy_mode_object(DWORD mode_address = render::COPY_MODE)
{
    const DWORD off = alloc_object(sizeof(render::CBlendMode));
    for (int side = 0; side < 2; side++)
        memcpy(object<void>(side, off), at<void *>(mode_address), sizeof(render::CBlendMode));
    return off;
}

}  // namespace zax::r1
