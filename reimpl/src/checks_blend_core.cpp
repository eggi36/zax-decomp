// Checks of blend_core.cpp, in the game thread. Global tables a function writes are saved and put back
// after each side (SavedMemory), the live blend objects are only read or copied.
//  - The registries of the 16 live blend objects: the nine built by a constructor against a fresh original
//    construction (and the dynamic span tables against what the constructor writes), the six built inline in
//    static initializers against the registrations decoded from their code (docs/DECOMPILATION.md 9).
//  - The nine constructors with MMX on and off (object, registry, global tables), CBlendMode's constructor
//    and destructor, RegisterBlitter sequences with replaced keys, the deleting destructors.
//  - FindBlitter on copies of every live object (Overlap through its own slot 2) with fake frames of every
//    depth and flag combination, twice (the cache).
//  - SetOpacity, WriteOpacity and ReadOpacity (a fake stream), the screen modes' SetLevel.
//  - The hue remap: constructor and destructors, Build8 and Build16 (the whole 64K table and the palette),
//    Hue_Lerp; the Overlap colour ramp (its maps and guard saved); Pixel15_FromRGB with and without the
//    tables; GamePalette_Get; Pixel16_BuildTables and BuildInvColorTable (the product, fade, palette and
//    opacity tables saved) with the game palette and other palettes.
#include "checks_frame_r1.h"

namespace zax {

int hooks_blend_core(Hook *out);

namespace {

using namespace r1;
using render::BlendRecord;
using render::CBlendMode;
using render::Palette;

ModuleHooks g_hooks{"blend_core", &hooks_blend_core, {}, {}, {}, 0, true};

template <typename R, typename... A> R exe(DWORD address, A... a) { return at<R(THISCALL *)(A...)>(address)(a...); }

void snap_registry(Snap &s, const CBlendMode *m)
{
    s.add(m->registry.vftable);
    s.add(m->registry.count);
    s.add(m->registry.capacity);
    s.add_bytes(m->registry.data, m->registry.count * sizeof(BlendRecord));
}
void snap_mode(Snap &s, const CBlendMode *m, DWORD extra_words)
{
    s.add(m->vftable);
    s.add_bytes(&m->palette.d4, 8);
    s.add(m->palette.data != nullptr);
    snap_registry(s, m);
    s.add(float_bits(m->opacity));
    s.add(m->opacity_entry);
    s.add(m->opacity255);
    s.add(m->cache_key);
    s.add(m->cache_fn);
    for (DWORD i = 0; i < extra_words; i++)
        s.add(reinterpret_cast<const DWORD *>(m)[0xd + i]);
}

// ---- The constructors and the tables they write ----------------------------------------------------------------

struct Ctor {
    DWORD ctor, object, dtor, size;
    const char *name;
};
constexpr Ctor CTORS[] = {
    {0x43eac0, 0x6bb4e0, 0x432da0, 0x34, "Copy"},          {0x4479c0, 0x6bcc68, 0x432da0, 0x34, "Transparent Mask"},
    {0x4386f0, 0x6b9728, 0x432da0, 0x34, "Black Bias"},    {0x4386f0, 0x6b9c60, 0x432da0, 0x34, "Test Light"},
    {0x433df0, 0x6b85f0, 0x432da0, 0x34, "Amplify Light"}, {0x443220, 0x6bb968, 0x432da0, 0x38, "ScreenUp555"},
    {0x443310, 0x6bbad8, 0x432da0, 0x38, "ScreenDown555"}, {0x444ea0, 0x6bc328, 0x432da0, 0x38, "ScreenUp565"},
    {0x444fd0, 0x6bc468, 0x432da0, 0x38, "ScreenDown565"}, {0x43bfe0, 0, 0x43c0b0, 0x48, "HueRemap"},
};
// The tables a constructor writes (address, bytes)
struct Table {
    DWORD ctor, address, bytes;
};
constexpr Table TABLES[] = {
    // Amplify Light
    {0x433df0, 0x6b7cf0, 0x800}, {0x433df0, 0x6b8928, 0x800}, {0x433df0, 0x6b67f0, 0x800}, {0x433df0, 0x6b74f0, 0x100}, {0x433df0, 0x6b6ff0, 0x100},
    {0x433df0, 0x6b7af0, 0x100}, {0x433df0, 0x6b78f0, 0x100}, {0x433df0, 0x6b8828, 0x100}, {0x433df0, 0x6b73f0, 0x100}, {0x433df0, 0x6b72f0, 0x100},
    {0x433df0, 0x6b8728, 0x100}, {0x433df0, 0x6b8628, 0x100}, {0x433df0, 0x6b9128, 0x100}, {0x433df0, 0x6b71f0, 0x100}, {0x433df0, 0x6b77f0, 0x100},
    {0x433df0, 0x6b7bf0, 0x100}, {0x433df0, 0x6b79f0, 0x100}, {0x433df0, 0x6b76f0, 0x100}, {0x433df0, 0x6b70f0, 0x100}, {0x433df0, 0x6b75f0, 0x100},
    {0x433df0, 0x6b84f0, 0x100},
    // Black Bias
    {0x4386f0, 0x6b9528, 0x100}, {0x4386f0, 0x6b9328, 0x100}, {0x4386f0, 0x6b995c, 0x100}, {0x4386f0, 0x6b985c, 0x100}, {0x4386f0, 0x6b9228, 0x100},
    {0x4386f0, 0x6b9628, 0x100}, {0x4386f0, 0x6b975c, 0x100}, {0x4386f0, 0x6b9b5c, 0x100}, {0x4386f0, 0x6b9a5c, 0x100}, {0x4386f0, 0x6b9428, 0x100},
    // Transparent Mask: its two 8 -> 16 tables and the generic entries 17-63 of T0-T35
    {0x4479c0, 0x6bcda0, 0x100}, {0x4479c0, 0x6bcc9c, 0x100}, {0x4479c0, 0x60cdd4, 0x2400 - 0x44},
    // the screen modes
    {0x443220, 0x6bb9a0, 0x100}, {0x443220, 0x6bb764, 0x100}, {0x443220, 0x6bb664, 0x100},
    {0x443310, 0x6bb560, 0x100}, {0x443310, 0x6bbb14, 0x100}, {0x443310, 0x6bb864, 0x100},
    {0x444ea0, 0x6bbd24, 0x200}, {0x444ea0, 0x6bc224, 0x100}, {0x444ea0, 0x6bbf24, 0x100},
    {0x444fd0, 0x6bc024, 0x200}, {0x444fd0, 0x6bbc20, 0x100}, {0x444fd0, 0x6bc364, 0x100},
};
void save_tables(SavedMemory &m, DWORD ctor)
{
    for (const Table &t : TABLES)
        if (t.ctor == ctor)
            m.add(t.address, t.bytes);
}

// The registrations of the six modes built inline in their static initializers (fn_mmx used when the CPU has
// MMX) [confirmed: code of 0x441a70, 0x443130, 0x446110, 0x446520, 0x446960, 0x43ad30]
struct Reg {
    DWORD sb, sf, db, df, fn, fn_mmx;
};
struct Inline {
    DWORD object;
    const char *name;
    Reg regs[5];
    int n;
};
constexpr Inline INLINE_MODES[] = {
    {0x6bb520, "Normal Scale", {{8, 0, 8, 0, 0x441b70, 0}, {8, 0, 16, 0, 0x441f70, 0}, {16, 0, 16, 0, 0x4423c0, 0}, {24, 0, 24, 0, 0x4427c0, 0}, {24, 0, 32, 0, 0x442bf0, 0}}, 5},
    {0x6bbaa0, "ScreenCopy", {{8, 0, 16, 0, 0x443400, 0}, {16, 0, 16, 0, 0x4439e0, 0x443600}, {24, 0, 16, 0, 0x443d70, 0}}, 3},
    {0x6bc4a8, "TileWrap", {{8, 0, 8, 0, 0x446190, 0}, {16, 0, 16, 0, 0x4462b0, 0}, {24, 0, 24, 0, 0x4463d0, 0}}, 3},
    {0x6bc4e0, "Tile", {{8, 0, 8, 0, 0x4465b0, 0}, {16, 0, 16, 0, 0x4466d0, 0}, {24, 0, 24, 0, 0x4467f0, 0}}, 3},
    {0x6bc618, "Overlap Blit", {{16, 4, 16, 0, 0x446e30, 0}, {8, 0, 8, 0, 0x447170, 0}, {16, 0, 16, 0, 0x447300, 0}}, 3},
    {0x6b9ca0, "Filter", {{8, 0, 8, 0, 0x43bad0, 0}, {16, 0, 16, 0, 0x43bd10, 0}}, 2},
};

void check_live_registries(Tally &t)
{
    const bool mmx = cpu_has_mmx();
    alignas(16) BYTE mem[0x48];
    for (const Ctor &c : CTORS) {
        if (!c.object)
            continue;
        const CBlendMode *live = at<const CBlendMode *>(c.object);
        SavedMemory tables;
        save_tables(tables, c.ctor);
        const unsigned before = tables.hash();
        memset(mem, 0, sizeof mem);
        CBlendMode *fresh = original<CBlendMode *(THISCALL *)(void *)>(c.ctor)(mem);
        const unsigned after = tables.hash();
        bool same = live->registry.count == fresh->registry.count && live->vftable == fresh->vftable &&
                    !memcmp(live->registry.data, fresh->registry.data, live->registry.count * sizeof(BlendRecord));
        char d[120];
        wsprintfA(d, "[live %s 0x%x: %u records, fresh %u; tables %s]", c.name, c.object, live->registry.count, fresh->registry.count,
                  before == after ? "unchanged" : "CHANGED");
        t.check(same && before == after, "", d);
        log("  live %s 0x%x: %u records%s, its tables as the constructor writes them: %s", c.name, c.object, live->registry.count,
            same ? " as constructed" : " DIFFER", before == after ? "yes" : "no");
        original<void(THISCALL *)(void *)>(c.dtor)(fresh);
        tables.release();
    }
    for (const Inline &m : INLINE_MODES) {
        const CBlendMode *live = at<const CBlendMode *>(m.object);
        bool same = live->registry.count == (DWORD)m.n;
        for (int i = 0; same && i < m.n; i++) {
            const BlendRecord &r = live->registry.data[i];
            const Reg &e = m.regs[i];
            same = r.src_bpp == e.sb && r.src_flags == e.sf && r.dst_bpp == e.db && r.dst_flags == e.df && r.fn == (mmx && e.fn_mmx ? e.fn_mmx : e.fn);
        }
        char d[120];
        wsprintfA(d, "[live %s 0x%x: %u records, decoded %d]", m.name, m.object, live->registry.count, m.n);
        t.check(same, "", d);
        log("  live %s 0x%x: %u records %s", m.name, m.object, live->registry.count, same ? "as decoded" : "DIFFER");
    }
}

void check_ctors(Tally &t)
{
    g_seed = 0x432d60;
    MmxSwitch sw;
    alignas(16) BYTE mem[2][0x48];
    for (int k = 0; k < scaled(40); k++) {
        const Ctor &c = CTORS[k % (int)(sizeof CTORS / sizeof CTORS[0])];
        const bool mmx = (k / 10) & 1;
        sw.set(mmx);
        SavedMemory tables;
        save_tables(tables, c.ctor);
        char what[64];
        wsprintfA(what, "%s constructor mmx %d", c.name, mmx);
        compare_sides(t, g_hooks, what, [&](int side) -> Snap {
            Snap s;
            memset(mem[side], 0xcd, sizeof mem[side]);
            CBlendMode *m = exe<CBlendMode *>(c.ctor, mem[side]);
            s.add(m == reinterpret_cast<CBlendMode *>(mem[side]));
            snap_mode(s, m, c.size > 0x34 ? 1 : 0);
            if (c.size == 0x48) {
                s.add_bytes(m->palette.data, 0x100);
                s.add(reinterpret_cast<DWORD *>(m)[0x11] != 0);
            }
            s.add(tables.hash());
            tables.restore();
            original<void(THISCALL *)(void *)>(c.dtor)(m);
            return s;
        });
        tables.release();
    }
    sw.restore();
    // the base constructor and destructor, RegisterBlitter
    for (int k = 0; k < scaled(200); k++) {
        const int n = rnd_range(0, 30);
        DWORD keys[30][5];
        for (int i = 0; i < n; i++) {
            keys[i][0] = 8 * (1 + rnd(4));
            keys[i][1] = rnd(3) * 2;
            keys[i][2] = 8 * (1 + rnd(4));
            keys[i][3] = rnd(4) ? 0 : 2;
            keys[i][4] = random();
        }
        const bool destroy = rnd(2);
        compare_sides(t, g_hooks, "CBlendMode ctor/RegisterBlitter/dtor", [&](int side) -> Snap {
            Snap s;
            memset(mem[side], 0xcd, sizeof mem[side]);
            CBlendMode *m = exe<CBlendMode *>(0x432d60, mem[side]);
            snap_mode(s, m, 0);
            for (int i = 0; i < n; i++) {
                const DWORD r = exe<DWORD>(0x432de0, m, keys[i][0], keys[i][1], keys[i][2], keys[i][3], keys[i][4]);
                s.add(r >= ptrv(m->registry.data) && r < ptrv(m->registry.data) + m->registry.count * sizeof(BlendRecord) ? r - ptrv(m->registry.data) : 0xffffu);
            }
            snap_registry(s, m);
            if (destroy) {
                exe<void>(0x432da0, m);
                snap_mode(s, m, 0);
            } else {
                original<void(THISCALL *)(void *)>(0x432da0)(m);
            }
            return s;
        });
    }
    // the deleting destructors (heap objects)
    constexpr DWORD DTORS[][3] = {{0x441b50, 0x43eac0, 0x34}, {0x433d90, 0x433df0, 0x34}, {0x438640, 0x4386f0, 0x34}, {0x43c070, 0x43bfe0, 0x48}};
    for (int k = 0; k < scaled(24); k++) {
        const DWORD *d = DTORS[k % 4];
        const DWORD flags = rnd(2);
        SavedMemory tables;
        save_tables(tables, d[1]);
        compare_sides(t, g_hooks, "blend deleting destructor", [&](int) -> Snap {
            Snap s;
            CBlendMode *m = static_cast<CBlendMode *>(heap_alloc(d[2]));
            original<CBlendMode *(THISCALL *)(void *)>(d[1])(m);
            tables.restore();
            s.add(exe<void *>(d[0], m, flags) == m);
            if (!(flags & 1)) {
                snap_mode(s, m, 0);
                heap_free(m);
            }
            return s;
        });
        tables.release();
    }
}

// ---- FindBlitter -------------------------------------------------------------------------------------------------

constexpr DWORD LIVE_MODES[] = {0x6bb4e0, 0x6bcc68, 0x6b9728, 0x6b9c60, 0x6b85f0, 0x6bb520, 0x6b9ca0, 0x6bc618,
                                0x6bc4e0, 0x6bc4a8, 0x6bbaa0, 0x6bb968, 0x6bbad8, 0x6bc328, 0x6bc468};
constexpr DWORD DEPTHS[] = {0x20, 0x40, 0x80, 0x100, 0, 0x60};
constexpr DWORD EXTRA[] = {0, 0, 0, 2, 4, 6, 0x2000, 0x4000, 0x8, 0x10, 0x100};

void check_find(Tally &t)
{
    g_seed = 0x432e80;
    SavedMemory overlap;
    overlap.add(0x6bc64c, 0x200);
    overlap.add(0x6bcb4c, 0x100);
    overlap.add(0x6bcc50, 0x10);
    overlap.add(0x6bcc60, 1);
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        FrameSpec s;
        s.width = 4;
        s.height = 4;
        const int a = make_frame(s), b = make_frame(s);
        DWORD fa = DEPTHS[rnd(4)] | EXTRA[rnd(11)], fb = DEPTHS[rnd(4)] | EXTRA[rnd(11)];
        if (rnd(10) == 0)
            fa = DEPTHS[rnd(6)] | EXTRA[rnd(11)] | EXTRA[rnd(11)];
        for (int side = 0; side < 2; side++) {
            frame(side, a)->flags = fa;
            frame(side, b)->flags = fb;
        }
        const DWORD mode = copy_mode_object(LIVE_MODES[(DWORD)k % (sizeof LIVE_MODES / 4)]);
        if (rnd(3) == 0) {   // a matching cache
            const DWORD fn = random();
            for (int side = 0; side < 2; side++) {
                object<CBlendMode>(side, mode)->cache_key = ((fa & 0x1ee) << 16) + (fb & 0x1ee);
                object<CBlendMode>(side, mode)->cache_fn = fn;
            }
        }
        char what[64];
        wsprintfA(what, "FindBlitter mode %x flags %x -> %x", LIVE_MODES[(DWORD)k % (sizeof LIVE_MODES / 4)], fa, fb);
        run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            void *m = object<void>(side, mode);
            const DWORD r = call_slot(m, 2, frame(side, a), frame(side, b));
            const DWORD again = call_slot(m, 2, frame(side, a), frame(side, b));
            overlap.restore();
            return r ^ (again << 1);
        });
    }
    overlap.release();
}

// ---- Opacity, levels -----------------------------------------------------------------------------------------------

void check_opacity(Tally &t)
{
    g_seed = 0x433be0;
    for (int k = 0; k < scaled(1500); k++) {
        begin_case(random());
        const DWORD mode = copy_mode_object(LIVE_MODES[rnd(15)]), stream = alloc_object(sizeof(FakeStream));
        float o;
        switch (rnd(6)) {
        case 0: o = 0.0f; break;
        case 1: o = 1.0f; break;
        case 2: o = (float)rnd(256) / 255.0f; break;
        default: o = (float)rnd(100000) / 100000.0f; break;
        }
        for (int side = 0; side < 2; side++) {
            FakeStream *f = object<FakeStream>(side, stream);
            init_fake_stream(f);
            memcpy(f->data, &o, 4);
        }
        const DWORD op = (DWORD)k % 3;
        run_sides(t, g_hooks, "opacity", [&](int side) -> DWORD {
            void *m = object<void>(side, mode);
            FakeStream *f = object<FakeStream>(side, stream);
            switch (op) {
            case 0: exe<void>(0x433be0, m, float_bits(o)); return 0;
            case 1: return exe<DWORD>(0x432f90, m, f);
            default: exe<void>(0x432fb0, m, f); return 0;
            }
        });
    }
    SavedMemory levels;
    constexpr DWORD LEVEL_GLOBALS[] = {0x6bb558, 0x6bbb10, 0x6bb55c, 0x6bb660, 0x6bc360, 0x6bbc18, 0x6bbd20, 0x6bbc1c};
    for (DWORD g : LEVEL_GLOBALS)
        levels.add(g, 4);
    constexpr DWORD SET_LEVEL[] = {0x4432d0, 0x4433c0, 0x444f60, 0x445090};
    for (int k = 0; k < scaled(400); k++) {
        begin_case(random());
        const DWORD mode = alloc_object(0x38);
        const int level = rnd(3) ? rnd_range(-40, 300) : (int)random();
        const DWORD fn = SET_LEVEL[k % 4];
        run_sides(t, g_hooks, "SetLevel", [&](int side) -> DWORD {
            exe<void>(fn, object<void>(side, mode), level);
            const DWORD h = levels.hash();
            levels.restore();
            return h;
        });
    }
    levels.release();
}

// ---- Hue remap, Overlap, pixels, palettes ---------------------------------------------------------------------------

void check_hue(Tally &t)
{
    g_seed = 0x43c0e0;
    for (int k = 0; k < scaled(3000); k++) {
        const int p = rnd_range(-60, 300), q = rnd_range(-60, 300), h = rnd(4) ? rnd_range(-0x700, 0xd00) : (int)random() >> 8;
        const int o = original<int(__fastcall *)(int, int, int)>(0x43c390)(p, q, h), r = at<int(__fastcall *)(int, int, int)>(0x43c390)(p, q, h);
        char d[80];
        wsprintfA(d, "[Hue_Lerp(%d, %d, %d): %d / %d]", p, q, h, o, r);
        t.check(o == r, "", d);
    }
    alignas(16) BYTE mem[2][0x48];
    MmxSwitch sw;
    for (int k = 0; k < scaled(16); k++) {
        const float hue = (float)rnd(3600) / 10.0f, nh = (float)rnd(3600) / 10.0f, range = rnd(4) ? (float)rnd(900) / 10.0f : 400.0f;
        const DWORD op = (DWORD)k % 4;
        sw.set(k & 4);
        char what[80];
        wsprintfA(what, "hue remap op %u", op);
        compare_sides(t, g_hooks, what, [&](int side) -> Snap {
            Snap s;
            memset(mem[side], 0, sizeof mem[side]);
            CBlendMode *m = reinterpret_cast<CBlendMode *>(mem[side]);
            if (op == 0)
                exe<void *>(0x43bfe0, m);
            else
                original<void *(THISCALL *)(void *)>(0x43bfe0)(m);
            if (op == 1)
                exe<void>(0x43c0e0, m, float_bits(hue), float_bits(nh), float_bits(range));
            if (op == 2) {
                original<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x43c0e0)(m, float_bits(nh), float_bits(hue), float_bits(range));
                exe<void>(0x43c410, m, float_bits(hue), float_bits(nh), float_bits(range));
            }
            snap_mode(s, m, 3);
            s.add_bytes(m->palette.data, 0x100);
            const BYTE *table = reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(reinterpret_cast<DWORD *>(m)[0x11]));
            if (op)
                s.add_bytes(table, 0x20000);
            if (op == 3) {
                exe<void>(0x43c0b0, m);
                snap_mode(s, m, 0);
            } else {
                original<void(THISCALL *)(void *)>(0x43c0b0)(m);
            }
            return s;
        });
    }
    sw.restore();
}

void check_overlap(Tally &t)
{
    SavedMemory maps;
    maps.add(0x6bc64c, 0x200);
    maps.add(0x6bcb4c, 0x100);
    maps.add(0x6bcc50, 0x10);
    maps.add(0x6bcc60, 1);
    for (int k = 0; k < 4; k++)
        compare_sides(t, g_hooks, "Overlap_BuildColor", [&](int) -> Snap {
            Snap s;
            if (k < 3)
                *at<BYTE *>(0x6bcc60) = 0;
            if (k == 1)
                at<render::FieldColor *>(0x6bcc50)->flags &= ~1u;
            at<void(__cdecl *)()>(0x446a90)();
            s.add(maps.hash());
            maps.restore();
            return s;
        });
    maps.release();
}

void check_pixels(Tally &t)
{
    g_seed = 0x4339a0;
    using F = DWORD(__fastcall *)(DWORD, DWORD, DWORD);
    Pixel16Tables tables;
    for (int built = 1; built >= 0; built--) {
        tables.set(built != 0);
        for (int k = 0; k < scaled(6000); k++) {
            const DWORD r = rnd(256), g = rnd(256), b = k < 300 && built ? 256 + rnd(40) : rnd(256);
            const DWORD o = original<F>(0x4339a0)(r, g, b), x = at<F>(0x4339a0)(r, g, b);
            char d[80];
            wsprintfA(d, "[Pixel15_FromRGB(%u, %u, %u) tables %d: %x / %x]", r, g, b, built, o, x);
            t.check(o == x, "", d);
        }
    }
    tables.restore();
    compare_sides(t, g_hooks, "GamePalette_Get", [&](int) -> Snap {
        Snap s;
        s.add(ptrv(at<void *(__cdecl *)()>(0x433a80)()));
        return s;
    });
}

void check_palettes(Tally &t)
{
    g_seed = 0x433a90;
    Palette *gp = at<Palette *>(render::GAME_PALETTE);
    if (!gp->data) {
        t.check(false, "", "[the game palette is not loaded]");
        return;
    }
    BYTE data[0x300];
    Pixel16Tables built;
    for (int k = 0; k < scaled(12); k++) {
        for (int i = 0; i < 0x300; i++)
            data[i] = k < 4 ? gp->data[i] : (BYTE)(i * (k + 3) + (k << 4));
        if (k % 4 == 3)
            for (int i = 0; i < 9; i++)
                data[i] = 0;   // black entries (blue 8)
        Palette pal = {data, gp->d4, gp->d8};
        built.set(k & 1);
        SavedMemory m;
        m.add(render::PALETTE15 - 4, 0x400);
        m.add(render::PALETTE16 - 4, 0x400);
        const bool inv = k % 2 == 0 && gp->d4 == 1;
        if (inv) {
            m.add(ptrv(gp->data), 0x300);
            m.add(0x6767dc, 0x40000);
            m.add(render::FADE_TABLES, 0x40000);
            m.add(render::OPACITY_ENTRIES, 0x400);
        }
        const bool clear_opacity = k % 3 == 0;
        compare_sides(t, g_hooks, inv ? "BuildInvColorTable" : "Pixel16_BuildTables", [&](int) -> Snap {
            Snap s;
            const BYTE *before = gp->data;
            if (inv) {
                if (clear_opacity)
                    at<DWORD *>(render::OPACITY_ENTRIES)[0x80] = 0;
                at<void(__fastcall *)(const Palette *, DWORD, DWORD)>(0x4337e0)(k == 0 ? gp : &pal, 1, 0);
            } else {
                exe<void>(0x433a90, k == 1 ? gp : &pal);
            }
            s.add(gp->data == before);
            s.add(m.hash());
            m.restore();
            return s;
        });
        m.release();
    }
    built.restore();
}

}  // namespace

ZAX_CHECKS(blend_core)
{
    (void)cls;
    (void)n;
    log("  CPU MMX flag: %d", cpu_has_mmx());
    Tally live{"blend core: the live registries and tables"};
    check_live_registries(live);
    live.report();
    Tally ctors{"blend core: constructors, RegisterBlitter, destructors"};
    check_ctors(ctors);
    ctors.report();
    Tally find{"blend core: FindBlitter"};
    check_find(find);
    find.report();
    Tally opacity{"blend core: opacity, screen levels"};
    check_opacity(opacity);
    opacity.report();
    Tally hue{"blend core: Hue_Lerp, the hue remap"};
    check_hue(hue);
    hue.report();
    Tally overlap{"blend core: the Overlap colour ramp"};
    check_overlap(overlap);
    overlap.report();
    Tally pixels{"blend core: Pixel15_FromRGB, GamePalette_Get"};
    check_pixels(pixels);
    pixels.report();
    Tally palettes{"blend core: Pixel16_BuildTables, BuildInvColorTable"};
    check_palettes(palettes);
    palettes.report();
}

}  // namespace zax
