// Checks of plasma_tile_map.cpp against the original, in the game thread (sides as in
// checks_world_misc.h).
//  - Objects: tile maps built by Create (the constructor of the side) on pool blocks, compared dword by
//    dword (equal raw values are equal, else two heap pointers compare by what they point to), their
//    texture lists, the plasma tables 0x6d1030 (hash), the tile map count and cache; destroyed by the
//    deleting destructor (the destructor of the side) with owned fake objects at +0x2d3c/+0x2d40.
//  - Grids: SetPixelSize and SetGrid with random sizes (1-40 cells, cell sizes 64, powers of two and
//    others), then random elevations and seeds; compared: the grid members, both frames' bytes, the
//    handle block.
//  - The elevation grid: CellOf, Elevation, BandAt and TextureAt at random pixels (fake band entries),
//    SetElevation, AddElevation and ReleaseRect with the handle release 0x4d2670 replaced by a recorder
//    (which handle, by its index), FlushCache with 0x4d2b50 recorded, Handle.
//  - Files: the slots 9 and 13 with the FileBuf writers replaced by recorders (texts and bytes hashed),
//    slot 11 with every kind of line ("Num Textures", "Texture <i>" with names of the running level's
//    textures, elevation and seed rows inside and outside the grid, short and long hex, other fields),
//    slot 15 with the FileBuf readers fed from a made-up file; compared: the state afterwards.
//  - Texture lists: built by 0x4b40f0, a fake source list (slot 37 and 12 recorders), SetCount,
//    CopyEntry (loaded and unloaded source entries, indices past the end), PrecacheEntries, ShareExtra,
//    the deleting destructor 0x4b4160.
//  - Real data: the running level's tile map: slots 9 and 13 (recorded), BandAt/TextureAt/Elevation at
//    many pixels, CellOf, ReleaseRect over its whole area and random rectangles, SetElevation and
//    AddElevation (releases recorded, the point put back), slot 7 (precache).
#include "checks_world_misc.h"
#include "file_buffer.h"
#include "plasma_tile_map.h"
#include "x87.h"

namespace zax {

int hooks_plasma_tile_map(Hook *out);

namespace {

using namespace misc;

DWORD &u32(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *frame_point(void *f, DWORD x, DWORD y) { return at<BYTE *(THISCALL *)(void *, DWORD, DWORD)>(0x4aad30)(f, x, y); }

constexpr DWORD ELEV = 0x2c6c, SEEDS = 0x2cac, COLS = 0x2d0c, ROWS = 0x2d10, HANDLES = 0x2d24, NHANDLES = 0x2d28;

// ---- Recorders --------------------------------------------------------------------------------------------------

BYTE *g_map;   // the tile map of the case (handles by index)
DWORD handle_tag(const DWORD *h)
{
    if (g_map && u32(g_map, HANDLES)) {
        const DWORD base = u32(g_map, HANDLES);
        if (ptrv(h) >= base && ptrv(h) < base + u32(g_map, NHANDLES) * 4 + 0x100000)
            return (ptrv(h) - base) / 4;
    }
    return 0xf0000000 | (ptrv(h) & 0xffff);
}
void THISCALL rec_release(DWORD *h)
{
    ev(0x2670, handle_tag(h), *h);
}
DWORD THISCALL rec_flush(void *cache)
{
    ev(0x2b50, cache == *at<void **>(TILE_CACHE));
    return 0x2b50;
}

BYTE g_fake_file[0x40];   // the file the recorders and feeders serve
bool mine(void *f) { return f == g_fake_file; }
DWORD THISCALL rec_write(CFileBuf *f, const void *p, DWORD n)
{
    if (!mine(f))
        return f->Write(p, n);
    ev(0x19d0, n, fnv(2166136261u, p, n));
    return n;
}
DWORD THISCALL rec_write_text(CFileBuf *f, const char *s)
{
    if (!mine(f))
        return f->WriteText(s);
    ev(0x1a10, fnv_text(2166136261u, s));
    return 1;
}
DWORD THISCALL rec_write_line(CFileBuf *f, const char *s)
{
    if (!mine(f))
        return f->WriteLine(s);
    ev(0x1a40, fnv_text(2166136261u, s));
    return 2;
}
CFileBuf *THISCALL rec_write_dword(CFileBuf *f, DWORD v)
{
    if (!mine(f))
        return f->WriteDword(v);
    ev(0x21c0, v);
    return f;
}
CFileBuf *THISCALL rec_write_counted(CFileBuf *f, const char *s)
{
    if (!mine(f))
        return f->WriteCountedText(s);
    ev(0x2220, fnv_text(2166136261u, s));
    return f;
}

// The made-up file of the readers
BYTE g_input[0x4000];
DWORD g_input_size, g_input_pos;
DWORD THISCALL rec_read(CFileBuf *f, void *p, DWORD n)
{
    if (!mine(f))
        return f->Read(p, n);
    DWORD k = 0;
    for (; k < n && g_input_pos < g_input_size; k++)
        static_cast<BYTE *>(p)[k] = g_input[g_input_pos++];
    ev(0x1b00, n, k);
    return k;
}
CFileBuf *THISCALL rec_read_dword(CFileBuf *f, DWORD *v)
{
    if (!mine(f))
        return f->ReadDword(v);
    *v = 0;
    for (int k = 0; k < 4 && g_input_pos < g_input_size; k++)
        *v |= (DWORD)g_input[g_input_pos++] << (8 * k);
    ev(0x2270, *v);
    return f;
}
CFileBuf *THISCALL rec_read_counted(CFileBuf *f, char *out)
{
    if (!mine(f))
        return f->ReadCountedText(out);
    DWORD n = g_input[g_input_pos++ % sizeof g_input];
    DWORD k = 0;
    for (; k < n && k < 120 && g_input_pos < g_input_size; k++)
        out[k] = (char)g_input[g_input_pos++];
    out[k] = 0;
    ev(0x2300, k);
    return f;
}

// CObject's 0x417500 (the lines the tile map does not know; it reads further lines from the reader)
DWORD THISCALL rec_object_line(void *self, const char *name, const char *value, void *r)
{
    ev(0x7500, self == g_map, fnv_text(fnv_text(2166136261u, name), value), ptrv(r));
    return 0x7500;
}

struct Patches {
    CodePatch p[12];
    int n = 0;
    void release() { patch_jump(p[n++], 0x4d2670, ptr(&rec_release)); }
    void flush() { patch_jump(p[n++], 0x4d2b50, ptr(&rec_flush)); }
    void object_line() { patch_jump(p[n++], 0x417500, ptr(&rec_object_line)); }
    void writers()
    {
        patch_jump(p[n++], 0x4a19d0, ptr(&rec_write));
        patch_jump(p[n++], 0x4a1a10, ptr(&rec_write_text));
        patch_jump(p[n++], 0x4a1a40, ptr(&rec_write_line));
        patch_jump(p[n++], 0x4a21c0, ptr(&rec_write_dword));
        patch_jump(p[n++], 0x4a2220, ptr(&rec_write_counted));
    }
    void readers()
    {
        patch_jump(p[n++], 0x4a1b00, ptr(&rec_read));
        patch_jump(p[n++], 0x4a2270, ptr(&rec_read_dword));
        patch_jump(p[n++], 0x4a2300, ptr(&rec_read_counted));
    }
    void off()
    {
        while (n > 0)
            unpatch(p[--n]);
    }
};

// ---- Fake objects -----------------------------------------------------------------------------------------------

// Owned objects at +0x2d3c (slot 1 deletes) and +0x2d40 (slot 0 deletes); fake textures
DWORD g_owned_vtable[4];
struct Owned {
    DWORD vftable;
    DWORD id;
};
Owned g_owned[2];
void *THISCALL owned_delete0(Owned *o, DWORD flags)
{
    ev(0xd0, o->id, flags);
    return o;
}
void *THISCALL owned_delete1(Owned *o, DWORD flags)
{
    ev(0xd1, o->id, flags);
    return o;
}

// The fake source list: a texture list (0x4b40f0) with a vtable copy whose slots 12 and 37 record
DWORD g_src_vtable[64];
Owned g_textures[16];
int g_ntex_loads;
void *THISCALL src_load(void *list, const char *dir, const char *name, void **out)
{
    ev(37, fnv_text(2166136261u, dir), fnv_text(2166136261u, name), ptrv(out) - u32(list, 0x30));
    return &g_textures[g_ntex_loads++ % 16];
}
DWORD THISCALL src_precache(void *, DWORD j)
{
    ev(12, j);
    return j + 0x100;
}

DWORD tex_tag(DWORD p)
{
    for (int i = 0; i < 16; i++)
        if (p == ptrv(&g_textures[i]))
            return i + 1;
    return p ? 0x1000 : 0;
}

void st_list(const TextureList *l)
{
    if (!l) {
        st(0xdead);
        return;
    }
    st(l->records.vftable);
    st(l->num_entries);
    st(u32(const_cast<TextureList *>(l), 0x154));
    st(u32(const_cast<TextureList *>(l), 0x158) & 0xff);
    const BYTE *e = reinterpret_cast<const BYTE *>(l->entries);
    st(e != nullptr);
    for (DWORD i = 0; e && i <= l->num_entries && i < 64; i++) {
        st_bytes(e + i * 0x80, 0x78);
        st(tex_tag(u32(const_cast<BYTE *>(e), i * 0x80 + 0x78)));
        st(u32(const_cast<BYTE *>(e), i * 0x80 + 0x7c));
    }
}

void st_grid(BYTE *m)
{
    for (DWORD off = 0x2cec; off < 0x2d24; off += 4)
        st(u32(m, off));
    st(u32(m, NHANDLES));
    const DWORD *h = reinterpret_cast<DWORD *>(u32(m, HANDLES));
    st(h != nullptr);
    if (h) {
        st(h[-1]);
        for (DWORD i = 0; i < h[-1] && i < 2000; i++)
            st(h[i]);
    }
    const DWORD cols = u32(m, COLS), rows = u32(m, ROWS);
    if (cols && rows && cols * rows < 0x10000)
        for (DWORD y = 0; y < rows; y++) {
            st_bytes(frame_point(m + ELEV, 0, y), cols);
            st_bytes(frame_point(m + SEEDS, 0, y), cols);
        }
}

// ---- Test tile maps -----------------------------------------------------------------------------------------------

// A pool block filled with a pattern (the constructor leaves most members as they are), then the
// constructor of the side
BYTE *new_map()
{
    BYTE *m = static_cast<BYTE *>(pool_alloc(0x2d48));
    if (!m)
        return nullptr;
    for (DWORD i = 0; i < 0x2d48; i += 4)
        u32(m, i) = 0x5a5a0000 + i;
    return static_cast<BYTE *>(at<void *(THISCALL *)(void *)>(0x53f490)(m));
}
void delete_map(BYTE *m) { at<void *(THISCALL *)(void *, DWORD)>(0x53f310)(m, 1); }

DWORD random_cell()
{
    switch (random() % 6) {
    case 0: return 1u << (random() % 8);
    case 1: return 1 + random() % 100;
    default: return 64;
    }
}

void random_grid(BYTE *m)
{
    if (random() % 2) {
        at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x53f680)(m, 1 + random() % 1500, 1 + random() % 1500);
    } else {
        const DWORD c = 1 + random() % 24, r = 1 + random() % 24;
        at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53fe00)(m, c, r, random_cell(), random_cell());
    }
    const DWORD cols = u32(m, COLS), rows = u32(m, ROWS);
    for (DWORD y = 0; y < rows; y++) {
        BYTE *e = frame_point(m + ELEV, 0, y), *s = frame_point(m + SEEDS, 0, y);
        for (DWORD x = 0; x < cols; x++) {
            e[x] = (BYTE)(random() % 3 ? random() : random() % 4 ? 0 : 0xff);
            s[x] = (BYTE)random();
        }
    }
}

// Band entries for TextureAt: tags instead of textures
void fake_bands(BYTE *m)
{
    for (DWORD i = 0; i < 256; i++)
        u32(m, 0x106c + i * 0x1c) = 0xb0000000 + i;
}

DWORD object_case(DWORD &mask)
{
    BYTE *m = new_map();
    if (!m)
        return 0xdead;
    const DWORD *words = reinterpret_cast<DWORD *>(m);
    for (DWORD i = 0; i < 0x2d48 / 4; i++) {
        const DWORD v = words[i];
        if (heap_pointer(reinterpret_cast<void *>(static_cast<uintptr_t>(v))) && readable(reinterpret_cast<void *>(static_cast<uintptr_t>(v)), 4) &&
            v != ptrv(m) && !(v >= ptrv(m) && v < ptrv(m) + 0x2d48))
            st(0x77770000 ^ *reinterpret_cast<DWORD *>(static_cast<uintptr_t>(v)));
        else if (v >= ptrv(m) && v < ptrv(m) + 0x2d48)
            st(0x66660000 + (v - ptrv(m)));
        else
            st(v);
    }
    st_list(*reinterpret_cast<TextureList **>(m + 0x1064));
    st(fnv(2166136261u, at<void *>(PLASMA_TABLES), 0x40000));
    st(*at<DWORD *>(TILE_MAP_COUNT));
    st(*at<DWORD *>(TILE_CACHE) != 0);
    if (g_seed % 2) {
        random_grid(m);
        st_grid(m);
    }
    for (DWORD &d : g_owned_vtable)
        d = 0;
    g_owned_vtable[0] = ptrv(ptr(&owned_delete0));
    g_owned_vtable[1] = ptrv(ptr(&owned_delete1));
    g_owned[0] = {ptrv(g_owned_vtable), 1};
    g_owned[1] = {ptrv(g_owned_vtable), 2};
    if (g_seed % 3 == 0) {
        u32(m, 0x2d3c) = ptrv(&g_owned[0]);
        u32(m, 0x2d40) = ptrv(&g_owned[1]);
    }
    const DWORD r = ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x53f310)(m, 1));
    st(*at<DWORD *>(TILE_MAP_COUNT));
    st(*at<DWORD *>(TILE_CACHE) != 0);
    mask = 0;
    return r - ptrv(m);
}

// The first tile map makes the shared tile cache, the last one frees it: the count and the cache set
// aside for the case
DWORD own_cache_case(DWORD &mask)
{
    DWORD &count = *at<DWORD *>(TILE_MAP_COUNT);
    void *&cache = *at<void **>(TILE_CACHE);
    const DWORD saved_count = count;
    void *saved_cache = cache;
    count = 0;
    cache = nullptr;
    BYTE *m = new_map();
    st(count);
    st(cache != nullptr);
    BYTE *m2 = g_seed % 2 ? new_map() : nullptr;
    st(count);
    if (m2)
        delete_map(m2);
    st(count);
    st(cache != nullptr);
    delete_map(m);
    st(count);
    st(cache != nullptr);
    count = saved_count;
    cache = saved_cache;
    mask = 0;
    return 0;
}

DWORD grid_case(DWORD &mask)
{
    BYTE *m = new_map();
    g_map = m;
    random_grid(m);
    st_grid(m);
    if (random() % 2) {
        random_grid(m);
        st_grid(m);
    }
    delete_map(m);
    g_map = nullptr;
    mask = 0;
    return 0;
}

DWORD elevation_case(DWORD &mask)
{
    BYTE *m = new_map();
    g_map = m;
    random_grid(m);
    fake_bands(m);
    Patches p;
    p.release();
    p.flush();
    const DWORD cols = u32(m, COLS), rows = u32(m, ROWS);
    const DWORD w = cols * u32(m, 0x2d04), h = rows * u32(m, 0x2d08);
    for (int k = 0; k < 24; k++) {
        const DWORD x = cols > 1 ? random() % ((cols - 1) * u32(m, 0x2d04)) : 0;
        const DWORD y = rows > 1 ? random() % ((rows - 1) * u32(m, 0x2d08)) : 0;
        switch (random() % 9) {
        case 0: ev(1, at<DWORD(THISCALL *)(void *, DWORD)>(0x540420)(m, random() % 3 ? random() % (w + 64) : random())); break;
        case 1: ev(2, at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x540440)(m, random() % cols, random() % rows)); break;
        case 2: ev(3, (DWORD)at<int(THISCALL *)(void *, DWORD, DWORD)>(0x542000)(m, x, y)); break;
        case 3: ev(4, at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x541fd0)(m, x, y)); break;
        case 4: {
            const DWORD gx = random() % cols, gy = random() % rows;
            const DWORD v = random() % 3 ? random() % 256 : *frame_point(m + ELEV, gx, gy);
            at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x540460)(m, gx, gy, v);
            break;
        }
        case 5: at<void(THISCALL *)(void *, DWORD, DWORD, int)>(0x5405e0)(m, random() % cols, random() % rows, (int)(random() % 601) - 300); break;
        case 6: {
            int r[4];
            for (int i = 0; i < 4; i++)
                r[i] = random() % 4 ? (int)(random() % (w + h + 200)) - 100 : (int)(random() % 0x7fffffff) - 0x3fffffff;
            if (u32(m, 0x2d08)) {
                void *cache = *at<void **>(TILE_CACHE);
                if (random() % 8 == 0)   // without the tile cache nothing is released
                    *at<void **>(TILE_CACHE) = nullptr;
                at<void(THISCALL *)(void *, const int *, DWORD)>(0x540640)(m, r, random() % 4);
                *at<void **>(TILE_CACHE) = cache;
            }
            break;
        }
        case 7: ev(5, at<DWORD(THISCALL *)(void *)>(0x541fb0)(m)); break;
        default: ev(6, ptrv(at<DWORD *(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x542140)(m, random() % 50, random() % 50, random() % 4)) - u32(m, HANDLES)); break;
        }
    }
    p.off();
    st_grid(m);
    delete_map(m);
    g_map = nullptr;
    mask = 0;
    return 0;
}

// Names of the running level's textures (known to the texture definitions 0x6c1d50 and loaded there)
constexpr int MAX_NAMES = 32;
char g_names[MAX_NAMES][64];
int g_nnames;
bool g_entry0_loaded;

void collect_names(TextureList *l)
{
    g_nnames = 0;
    for (DWORD i = 0; l && i < l->num_entries && g_nnames < MAX_NAMES; i++) {
        const char *s = vcall<const char *>(l, 1, i);
        if (!s || lstrlenA(s) >= 63)
            continue;
        memcpy(g_names[g_nnames++], s, (DWORD)lstrlenA(s) + 1);
    }
    auto *defs = at<ZDefinitionList *>(TEXTURE_DEFINITIONS);
    g_entry0_loaded = defs->entries && defs->num_entries && u32(defs->entries, 0x78);
}

void hex_row(char *out, DWORD n)
{
    const char *digits = random() % 2 ? "0123456789abcdef" : "0123456789ABCDEF";
    DWORD i = 0;
    for (; i < n * 2 && i < 200; i++)
        out[i] = digits[random() % 16];
    out[i] = 0;
}

DWORD read_line_case(DWORD &mask)
{
    BYTE *m = new_map();
    g_map = m;
    random_grid(m);
    Patches p;
    p.release();
    p.object_line();
    const int steps = 1 + random() % 12;
    for (int s = 0; s < steps; s++) {
        char name[64], value[256];
        value[0] = 0;
        switch (random() % 8) {
        case 0:
            lstrcpyA(name, "Num Textures");
            wsprintfA(value, "%d", (int)(random() % 6));
            break;
        case 1: {
            const DWORD n = (*reinterpret_cast<TextureList **>(m + 0x1064))->num_entries;
            if (!g_nnames || !n) {
                lstrcpyA(name, "Num Textures");
                wsprintfA(value, "%d", 2);
                break;
            }
            wsprintfA(name, "Texture %u", random() % n);
            lstrcpyA(value, g_names[random() % g_nnames]);
            if (g_entry0_loaded && random() % 4 == 0)
                lstrcpyA(value, "No Such Texture");
            break;
        }
        case 2: case 3:
            wsprintfA(name, random() % 2 ? "Elevations Row %u" : "Seeds Row %u", random() % 3 ? random() % (u32(m, ROWS) + 2) : 0);
            hex_row(value, random() % 4 ? u32(m, COLS) : random() % (u32(m, COLS) + 8));
            break;
        case 4:
            lstrcpyA(name, random() % 2 ? "Blending" : random() % 2 ? "Num Texture" : "Texture");
            wsprintfA(value, "0.%u", random() % 1000);
            break;
        default:
            wsprintfA(name, "Elevations Row 0");
            hex_row(value, u32(m, COLS));
            break;
        }
        ev(0x11, fnv_text(2166136261u, name), at<DWORD(THISCALL *)(void *, const char *, char *, void *)>(0x53f920)(m, name, value, g_fake_file) != 0);
        st_grid(m);
        st_list(*reinterpret_cast<TextureList **>(m + 0x1064));
        st(u32(m, 0x1068));
    }
    p.off();
    delete_map(m);
    g_map = nullptr;
    mask = 0;
    return 0;
}

DWORD write_case(DWORD &mask)
{
    BYTE *m = new_map();
    g_map = m;
    random_grid(m);
    if (g_nnames) {
        const DWORD n = random() % 5;
        at<DWORD(THISCALL *)(void *, DWORD)>(0x4b4540)(*reinterpret_cast<void **>(m + 0x1064), n);
        for (DWORD i = 0; i < n; i++) {
            const int idx = vcall<int>(at<void *>(TEXTURE_DEFINITIONS), 2, g_names[random() % g_nnames], -1);
            if (idx >= 0)
                at<void *(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4b4430)(*reinterpret_cast<void **>(m + 0x1064), i,
                                                                               at<void *>(TEXTURE_DEFINITIONS), (DWORD)idx);
        }
    }
    Patches p;
    p.writers();
    ev(9, at<DWORD(THISCALL *)(void *, void *)>(0x53f7c0)(m, g_fake_file) & 0);
    ev(13, at<DWORD(THISCALL *)(void *, void *)>(0x53fbc0)(m, g_fake_file));
    p.off();
    delete_map(m);
    g_map = nullptr;
    mask = 0;
    return 0;
}

DWORD read_binary_case(DWORD &mask)
{
    BYTE *m = new_map();
    g_map = m;
    random_grid(m);
    // the made-up file: count, counted names, both frames' bytes (sometimes short)
    g_input_pos = 0;
    DWORD k = 0;
    const DWORD n = g_nnames ? random() % 5 : 0;
    for (int b = 0; b < 4; b++)
        g_input[k++] = (BYTE)(n >> (8 * b));
    for (DWORD i = 0; i < n; i++) {
        const char *s = g_names[random() % g_nnames];
        const DWORD len = (DWORD)lstrlenA(s);
        g_input[k++] = (BYTE)len;
        for (DWORD c = 0; c < len; c++)
            g_input[k++] = (BYTE)s[c];
    }
    const DWORD bytes = u32(m, COLS) * u32(m, ROWS) * 2;
    for (DWORD i = 0; i < bytes && k < sizeof g_input; i++)
        g_input[k++] = (BYTE)random();
    g_input_size = random() % 4 ? k : random() % (k + 1);
    Patches p;
    p.readers();
    p.release();
    const DWORD r = at<DWORD(THISCALL *)(void *, void *)>(0x53fc50)(m, g_fake_file);
    p.off();
    st_grid(m);
    st_list(*reinterpret_cast<TextureList **>(m + 0x1064));
    delete_map(m);
    g_map = nullptr;
    mask = 0xff;
    return r;
}

// ---- Texture lists --------------------------------------------------------------------------------------------

TextureList *new_source()
{
    auto *l = static_cast<TextureList *>(pool_alloc(0x160));
    at<void *(THISCALL *)(void *, void *)>(0x4b40f0)(l, at<void *(__cdecl *)()>(0x4a8760)());
    memcpy(g_src_vtable, at<void *>(VT_TEXTURE_LIST), sizeof g_src_vtable);
    g_src_vtable[12] = ptrv(ptr(&src_precache));
    g_src_vtable[37] = ptrv(ptr(&src_load));
    l->records.vftable = ptrv(g_src_vtable);
    const DWORD n = 1 + random() % 8;
    at<DWORD(THISCALL *)(void *, DWORD)>(0x4b4540)(l, n);
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = reinterpret_cast<BYTE *>(l->entries) + i * 0x80;
        wsprintfA(reinterpret_cast<char *>(e), random() % 3 ? "Ground/Tex %u" : "Tex%u", random() % 100);
        if (random() % 3 == 0)
            u32(e, 0x78) = ptrv(&g_textures[random() % 16]);
        if (random() % 6 == 0)
            e[0x7f] = 1;
    }
    // the constructor leaves the folder as it is
    lstrcpyA(l->folder, random() % 2 ? "Cache/Textures" : random() % 2 ? "Cache/Textures/" : "");
    return l;
}
void delete_source(TextureList *l)
{
    l->records.vftable = VT_TEXTURE_LIST;
    for (DWORD i = 0; i < l->num_entries; i++)
        u32(l->entries, i * 0x80 + 0x78) = 0;   // fake textures: nothing to delete
    at<void *(THISCALL *)(void *, DWORD)>(0x4b4160)(l, 1);
}

DWORD list_case(DWORD &mask)
{
    for (int i = 0; i < 16; i++)
        g_textures[i] = {0, (DWORD)i + 1};
    g_ntex_loads = 0;
    TextureList *src = new_source();
    BYTE *m = new_map();
    auto *l = *reinterpret_cast<TextureList **>(m + 0x1064);
    const DWORD n = random() % 6;
    ev(1, at<DWORD(THISCALL *)(void *, DWORD)>(0x4b4540)(l, n) != 0);
    st_list(l);
    for (int s = 0, steps = 1 + random() % 10; s < steps; s++) {
        switch (random() % 4) {
        case 0: case 1:
            if (l->num_entries) {
                const DWORD dst = random() % l->num_entries;
                const DWORD i = random() % 4 ? random() % src->num_entries : src->num_entries + random() % 3;
                if (i < src->num_entries || random() % 2)
                    ev(2, ptrv(at<void *(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4b4430)(l, dst, src, i < src->num_entries ? i : src->num_entries)) - ptrv(l));
            }
            break;
        case 2:   // +0x15c is only set by CopyEntry (the constructor leaves it)
            if (u32(l, 0x15c) == ptrv(src))
                at<DWORD(THISCALL *)(void *)>(0x4b4670)(l);
            break;
        default:
            ev(3, at<DWORD(THISCALL *)(void *, DWORD)>(0x4b4540)(l, random() % 4) != 0);
            break;
        }
        st_list(l);
        st_list(src);
        st(u32(l, 0x15c) == ptrv(src));
    }
    for (DWORD i = 0; i < l->num_entries; i++)
        u32(l->entries, i * 0x80 + 0x78) = 0;
    delete_map(m);
    delete_source(src);
    mask = 0;
    return 0;
}

DWORD extra_case(DWORD &mask)
{
    // ShareExtra between two lists built by the constructor, then the deleting destructor 0x4b4160
    auto *a = static_cast<TextureList *>(pool_alloc(0x160));
    auto *b = static_cast<TextureList *>(pool_alloc(0x160));
    for (DWORD i = 0; i < 0x160; i += 4) {   // the constructors leave padding and +0x15c as they are
        u32(a, i) = 0x3c3c0000 + i;
        u32(b, i) = 0x4d4d0000 + i;
    }
    at<void *(THISCALL *)(void *, void *)>(0x4b40f0)(a, at<void *(__cdecl *)()>(0x4a8760)());
    at<void *(THISCALL *)(void *, void *)>(0x4b40f0)(b, reinterpret_cast<void *>(static_cast<uintptr_t>(random())));
    st_bytes(a->extra, 0x1c);
    st(u32(b, 0x154));
    const DWORD r = at<DWORD(THISCALL *)(void *, void *)>(0x4b4220)(a, b);
    st(a->extra[0x18]);
    st(r != 0);
    at<void *(THISCALL *)(void *, DWORD)>(0x4b4160)(b, 1);
    at<void *(THISCALL *)(void *, DWORD)>(0x4b4160)(a, 1);
    mask = 0;
    return 0;
}

// ---- Real data --------------------------------------------------------------------------------------------------

BYTE *find_map()
{
    BYTE *layer = running_layer();
    if (!layer || !readable(layer + 0x7c, 4))
        return nullptr;
    BYTE *m = *reinterpret_cast<BYTE **>(layer + 0x7c);
    return heap_pointer(m) && readable(m, 0x2d48) && u32(m, 0) == VT_PLASMA_TILE_MAP ? m : nullptr;
}

BYTE *g_real;

DWORD real_write_case(DWORD &mask)
{
    g_map = g_real;
    Patches p;
    p.writers();
    if (g_seed == 1)
        at<DWORD(THISCALL *)(void *, void *)>(0x53f7c0)(g_real, g_fake_file);
    else
        ev(13, at<DWORD(THISCALL *)(void *, void *)>(0x53fbc0)(g_real, g_fake_file));
    p.off();
    g_map = nullptr;
    mask = 0;
    return 0;
}

DWORD real_query_case(DWORD &mask)
{
    g_map = g_real;
    const DWORD cols = u32(g_real, COLS), rows = u32(g_real, ROWS);
    const DWORD w = (cols - 1) * u32(g_real, 0x2d04), h = (rows - 1) * u32(g_real, 0x2d08);
    for (int k = 0; k < 64; k++) {
        const DWORD x = random() % w, y = random() % h;
        ev(1, (DWORD)at<int(THISCALL *)(void *, DWORD, DWORD)>(0x542000)(g_real, x, y));
        ev(2, at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x541fd0)(g_real, x, y));
        ev(3, at<DWORD(THISCALL *)(void *, DWORD)>(0x540420)(g_real, x));
        ev(4, at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x540440)(g_real, x % cols, y % rows));
    }
    g_map = nullptr;
    mask = 0;
    return 0;
}

DWORD real_release_case(DWORD &mask)
{
    g_map = g_real;
    Patches p;
    p.release();
    int r[4] = {0, 0, (int)u32(g_real, 0x2cfc), (int)u32(g_real, 0x2d00)};
    if (g_seed > 4)
        for (int i = 0; i < 4; i++)
            r[i] = (int)(random() % (u32(g_real, 0x2cfc) + 400)) - 200;
    if (g_seed <= 12 || g_seed % 4 == 0) {
        at<void(THISCALL *)(void *, const int *, DWORD)>(0x540640)(g_real, r, g_seed % 4);
    } else {
        // a point changed (the releases recorded) and put back
        const DWORD gx = random() % u32(g_real, COLS), gy = random() % u32(g_real, ROWS);
        BYTE *point = frame_point(g_real + ELEV, gx, gy);
        const BYTE old = *point;
        if (g_seed % 2)
            at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x540460)(g_real, gx, gy, random() % 3 ? (old + 1 + random() % 255) & 0xff : old);
        else
            at<void(THISCALL *)(void *, DWORD, DWORD, int)>(0x5405e0)(g_real, gx, gy, (int)(random() % 81) - 40);
        st(*point);
        *point = old;
    }
    p.off();
    g_map = nullptr;
    mask = 0;
    return 0;
}

}  // namespace

ZAX_CHECKS(plasma_tile_map)
{
    (void)cls;
    (void)n;
    use_hooks(&hooks_plasma_tile_map);
    Tally objects{"plasma tile map: objects and grids"}, grid{"plasma tile map: elevation grid and tile cache handles"},
        files{"plasma tile map: file slots"}, lists{"plasma tile map: texture lists"}, real{"plasma tile map: running level"};
    g_real = find_map();
    collect_names(g_real ? *reinterpret_cast<TextureList **>(g_real + 0x1064) : nullptr);
    log("  plasma tile map: running level's map %s, %d texture names, entry 0 of the definitions %s", g_real ? "found" : "missing",
        g_nnames, g_entry0_loaded ? "loaded" : "not loaded");
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        two_sides(objects, "object", seed, [](DWORD &mask) { return object_case(mask); });
    for (unsigned seed = 1; seed <= 4; seed++)
        two_sides(objects, "own cache", seed, [](DWORD &mask) { return own_cache_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
        two_sides(objects, "grid", seed, [](DWORD &mask) { return grid_case(mask); });
    objects.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(1000); seed++)
        two_sides(grid, "elevation", seed, [](DWORD &mask) { return elevation_case(mask); });
    grid.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++)
        two_sides(files, "read line", seed, [](DWORD &mask) { return read_line_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(files, "write", seed, [](DWORD &mask) { return write_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(files, "read binary", seed, [](DWORD &mask) { return read_binary_case(mask); });
    files.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(500); seed++)
        two_sides(lists, "list", seed, [](DWORD &mask) { return list_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(50); seed++)
        two_sides(lists, "extra", seed, [](DWORD &mask) { return extra_case(mask); });
    lists.report();
    if (g_real) {
        log("  plasma tile map: running level %ux%u pixels, grid %ux%u of %ux%u, %u handles, %u textures", u32(g_real, 0x2cfc),
            u32(g_real, 0x2d00), u32(g_real, COLS), u32(g_real, ROWS), u32(g_real, 0x2d04), u32(g_real, 0x2d08),
            u32(g_real, NHANDLES), (*reinterpret_cast<TextureList **>(g_real + 0x1064))->num_entries);
        for (unsigned seed = 1; seed <= 2; seed++)
            two_sides(real, "real write", seed, [](DWORD &mask) { return real_write_case(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(100); seed++)
            two_sides(real, "real query", seed, [](DWORD &mask) { return real_query_case(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
            two_sides(real, "real release", seed, [](DWORD &mask) { return real_release_case(mask); });
        two_sides(real, "real precache", 1, [](DWORD &mask) {
            mask = 0;
            return at<DWORD(THISCALL *)(void *)>(0x53f660)(g_real);
        });
    } else {
        log("  plasma tile map: no running level, real data skipped");
    }
    real.report();
}

}  // namespace zax
