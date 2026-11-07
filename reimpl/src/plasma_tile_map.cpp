// CPlasmaTileMap without its drawing and the texture list functions it uses, reimplemented (see
// plasma_tile_map.h).
#include "plasma_tile_map.h"
#include "path_util.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

static DWORD &u32(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static BYTE *&ptr8(void *p, DWORD off) { return *reinterpret_cast<BYTE **>(static_cast<BYTE *>(p) + off); }

static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static void frame_ctor(void *f) { fn<void *(THISCALL *)(void *)>(0x4a8a90)(f); }
static void frame_dtor(void *f) { fn<void(THISCALL *)(void *)>(0x4a8b00)(f); }
static DWORD frame_create(void *f, DWORD w, DWORD h, DWORD bits) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, bits); }
static BYTE *frame_point(void *f, DWORD x, DWORD y) { return fn<BYTE *(THISCALL *)(void *, DWORD, DWORD)>(0x4aad30)(f, x, y); }
static DWORD frame_get(void *f, DWORD x, DWORD y) { return vcall<DWORD>(f, 54, x, y); }
static void handle_ctor(DWORD *h) { fn<void *(THISCALL *)(DWORD *)>(0x4d2660)(h); }
static void handle_release(DWORD *h) { fn<void(THISCALL *)(DWORD *)>(0x4d2670)(h); }
static int log2_floor(DWORD v) { return fn<int(__fastcall *)(DWORD)>(0x509680)(v); }
static int crt_strncmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5d17d0)(a, b, n); }
template <typename... A> static int crt_sprintf(char *out, const char *fmt, A... a)
{
    return fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, a...);
}
static DWORD buf_write_line(void *f, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a40)(f, s); }
static DWORD buf_write_text(void *f, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a10)(f, s); }
static DWORD buf_write(void *f, const void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(f, p, n); }
static DWORD buf_read(void *f, void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(f, p, n); }
static void buf_write_dword(void *f, DWORD v) { fn<void *(THISCALL *)(void *, DWORD)>(0x4a21c0)(f, v); }
static void buf_read_dword(void *f, DWORD *v) { fn<void *(THISCALL *)(void *, DWORD *)>(0x4a2270)(f, v); }
static void buf_write_counted(void *f, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4a2220)(f, s); }
static void buf_read_counted(void *f, char *s) { fn<void *(THISCALL *)(void *, char *)>(0x4a2300)(f, s); }
static DWORD hex_decode(BYTE *out, const char *text, DWORD n) { return fn<DWORD(__fastcall *)(BYTE *, const char *, DWORD)>(0x587ff0)(out, text, n); }

static TextureList *list_of(BYTE *map) { return reinterpret_cast<TextureList *>(ptr8(map, 0x1064)); }
static void *definitions() { return glob<void *>(TEXTURE_DEFINITIONS); }

constexpr DWORD ELEV = 0x2c6c, SEEDS = 0x2cac, COLS = 0x2d0c, ROWS = 0x2d10, HANDLES = 0x2d24, NHANDLES = 0x2d28;
constexpr DWORD TEXT_NUM_TEXTURES_FMT = 0x61e794, TEXT_TEXTURE_FMT = 0x61e784, TEXT_ELEV_FMT = 0x61e770,
                TEXT_SEEDS_FMT = 0x61e760, TEXT_NUM_TEXTURES = 0x61e7cc, TEXT_TEXTURE = 0x61e7c0,
                TEXT_ELEV = 0x61e7b0, TEXT_SEEDS = 0x61e7a4;

// The handle block: released from the last, freed; a new one of +0x2d28 handles (none cleared when the
// count minus 1 is negative). eax as the original's: the new handles or 0.
static void free_handles(BYTE *map)
{
    if (DWORD *h = reinterpret_cast<DWORD *>(ptr8(map, HANDLES))) {
        DWORD *block = h - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                handle_release(&h[i - 1]);
        heap_free(block);
        u32(map, HANDLES) = 0;
    }
}
static DWORD rebuild_handles(BYTE *map)
{
    free_handles(map);
    const DWORD n = u32(map, NHANDLES);
    DWORD *block = static_cast<DWORD *>(heap_alloc(n * 4 + 4));
    if (!block) {
        u32(map, HANDLES) = 0;
        return 0;
    }
    block[0] = n;
    DWORD *h = block + 1;
    if ((int)(n - 1) >= 0)
        for (DWORD i = 0; i < n; i++)
            handle_ctor(&h[i]);
    u32(map, HANDLES) = (DWORD)(uintptr_t)h;
    return (DWORD)(uintptr_t)h;
}

// ---- CPlasmaTileMap --------------------------------------------------------------------------------------

// 0x53f490
void *THISCALL PlasmaMap::Ctor(BYTE *self)
{
    obj_ctor(self);
    fn<void *(THISCALL *)(void *)>(0x480e20)(self + 8);
    frame_ctor(self + ELEV);
    frame_ctor(self + SEEDS);
    u32(self, 0) = vt(VT_PLASMA_TILE_MAP);
    u32(self, HANDLES) = 0;
    u32(self, NHANDLES) = 0;
    obj_init_fields(self);
    u32(self, 0x2d44) = 0;
    if (!*glob<DWORD *>(TILE_MAP_COUNT)) {
        void *c = heap_alloc(0x44);
        *glob<void **>(TILE_CACHE) = c ? fn<void *(THISCALL *)(void *, DWORD)>(0x4d27c0)(c, 0x480000) : nullptr;
    }
    ++*glob<DWORD *>(TILE_MAP_COUNT);
    u32(self, 0x2d38) = 0;
    BYTE *table = glob<BYTE *>(PLASMA_TABLES);
    for (DWORD seed = 0xe6cef; seed < 0xe7b5bef; seed += 0xe6cef, table += 0x400)
        fn<void(THISCALL *)(void *, DWORD)>(0x55c390)(table, seed);
    auto *list = static_cast<TextureList *>(pool_alloc(0x160));
    if (list) {
        fn<void *(THISCALL *)(void *, void *)>(0x4b40f0)(list, fn<void *(__cdecl *)()>(0x4a8760)());
        list->records.vftable = vt(VT_PLASMA_TEXTURE_LIST);
    }
    ptr8(self, 0x1064) = reinterpret_cast<BYTE *>(list);
    self[0x60] = 0;
    u32(self, 0x2d3c) = 0;
    u32(self, 0x2d40) = 0;
    return self;
}

// 0x53f6d0
DWORD THISCALL PlasmaMap::Dtor(BYTE *self)
{
    u32(self, 0) = vt(VT_PLASMA_TILE_MAP);
    obj_free_fields(self);
    if (!--*glob<DWORD *>(TILE_MAP_COUNT))
        if (void *c = *glob<void **>(TILE_CACHE)) {
            fn<void(THISCALL *)(void *)>(0x4d2880)(c);
            heap_free(c);
            *glob<void **>(TILE_CACHE) = nullptr;
        }
    if (BYTE *l = ptr8(self, 0x1064)) {
        vcall<void *>(l, 0, 1);
        u32(self, 0x1064) = 0;
    }
    if (BYTE *o = ptr8(self, 0x2d3c)) {
        vcall<void *>(o, 1, 1);
        u32(self, 0x2d3c) = 0;
    }
    if (BYTE *o = ptr8(self, 0x2d40)) {
        vcall<void *>(o, 0, 1);
        u32(self, 0x2d40) = 0;
    }
    free_handles(self);
    u32(self, NHANDLES) = 0;
    frame_dtor(self + SEEDS);
    frame_dtor(self + ELEV);
    fn<void(THISCALL *)(void *)>(0x480e80)(self + 8);
    return obj_dtor(self);
}

// 0x53f660 (slot 7)
DWORD THISCALL PlasmaMap::Precache(BYTE *self)
{
    fn<DWORD(THISCALL *)(void *)>(0x4167b0)(self);
    return fn<DWORD(THISCALL *)(void *)>(0x4b4670)(list_of(self));
}

// 0x53f7c0 (slot 9). The hex buffer (2 * columns + 3) is taken once, the rows are read every time.
DWORD THISCALL PlasmaMap::WriteOwnLines(BYTE *self, void *f)
{
    char line[0x200];
    const DWORD n = list_of(self)->num_entries;
    crt_sprintf(line, glob<const char *>(TEXT_NUM_TEXTURES_FMT), n);
    buf_write_line(f, line);
    for (DWORD i = 0; i < n; i++) {
        const char *name = vcall<const char *>(list_of(self), 1, i);
        crt_sprintf(line, glob<const char *>(TEXT_TEXTURE_FMT), i, name);
        buf_write_line(f, line);
    }
    char *hex = static_cast<char *>(heap_alloc(u32(self, COLS) * 2 + 3));
    for (DWORD r = 0; r < u32(self, ROWS); r++) {
        crt_sprintf(line, glob<const char *>(TEXT_ELEV_FMT), r);
        buf_write_text(f, line);
        const DWORD w = u32(self, COLS);
        text_hex(hex, frame_point(self + ELEV, 0, r), w);
        buf_write_line(f, hex);
    }
    for (DWORD r = 0; r < u32(self, ROWS); r++) {
        crt_sprintf(line, glob<const char *>(TEXT_SEEDS_FMT), r);
        buf_write_text(f, line);
        const DWORD w = u32(self, COLS);
        text_hex(hex, frame_point(self + SEEDS, 0, r), w);
        buf_write_line(f, hex);
    }
    if (hex)
        heap_free(hex);
    return 0;
}

// 0x53f920 (slot 11). eax: the handled branch's last call, the row number (or what row 0 left) for a
// row outside the grid.
DWORD THISCALL PlasmaMap::ReadOwnLine(BYTE *self, const char *name, const char *value, void *r)
{
    const char *key = glob<const char *>(TEXT_NUM_TEXTURES);
    DWORD k = 0;
    while (name[k] == key[k] && name[k])
        k++;
    if (name[k] == key[k])
        return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b4540)(list_of(self), (DWORD)crt_atoi(value));
    const char *texture = glob<const char *>(TEXT_TEXTURE);
    if (!crt_strncmp(name, texture, (DWORD)lstrlenA(texture))) {
        const DWORD i = (DWORD)crt_atoi(name + lstrlenA(texture));
        int idx = vcall<int>(definitions(), 2, value, -1);
        if (idx == -1)
            idx = 0;
        return (DWORD)(uintptr_t)fn<void *(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4b4430)(list_of(self), i, definitions(), (DWORD)idx);
    }
    const char *elev = glob<const char *>(TEXT_ELEV);
    if (!crt_strncmp(name, elev, (DWORD)lstrlenA(elev))) {
        const DWORD row = (DWORD)crt_atoi(name + lstrlenA(elev));
        DWORD eax = row;
        if (!row) {
            frame_create(self + ELEV, u32(self, COLS), u32(self, ROWS), 8);
            u32(self, NHANDLES) = u32(self, ROWS) * u32(self, COLS) * 3;
            eax = rebuild_handles(self);
        }
        if (row >= u32(self, ROWS))
            return eax;
        return hex_decode(frame_point(self + ELEV, 0, row), value, u32(self, COLS));
    }
    const char *seeds = glob<const char *>(TEXT_SEEDS);
    if (!crt_strncmp(name, seeds, (DWORD)lstrlenA(seeds))) {
        const DWORD row = (DWORD)crt_atoi(name + lstrlenA(seeds));
        DWORD eax = row;
        if (!row)
            eax = frame_create(self + SEEDS, u32(self, COLS), u32(self, ROWS), 8);
        if (row >= u32(self, ROWS))
            return eax;
        return hex_decode(frame_point(self + SEEDS, 0, row), value, u32(self, COLS));
    }
    return fn<DWORD(THISCALL *)(void *, const char *, const char *, void *)>(0x417500)(self, name, value, r);
}

// 0x53fbc0 (slot 13)
DWORD THISCALL PlasmaMap::WriteBinary(BYTE *self, void *f)
{
    const DWORD n = list_of(self)->num_entries;
    buf_write_dword(f, n);
    for (DWORD i = 0; i < n; i++)
        buf_write_counted(f, vcall<const char *>(list_of(self), 1, i));
    DWORD size = u32(self, COLS) * u32(self, ROWS);
    buf_write(f, frame_point(self + ELEV, 0, 0), size);
    size = u32(self, COLS) * u32(self, ROWS);
    return buf_write(f, frame_point(self + SEEDS, 0, 0), size);
}

// 0x53fc50 (slot 15): names into a 200-byte buffer as in the original. al 1.
DWORD THISCALL PlasmaMap::ReadBinary(BYTE *self, void *f)
{
    DWORD n;
    buf_read_dword(f, &n);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b4540)(list_of(self), n);
    for (DWORD i = 0; i < n; i++) {
        char name[200];
        buf_read_counted(f, name);
        int idx = vcall<int>(definitions(), 2, name, -1);
        if (idx == -1)
            idx = 0;
        fn<void *(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4b4430)(list_of(self), i, definitions(), (DWORD)idx);
    }
    frame_create(self + ELEV, u32(self, COLS), u32(self, ROWS), 8);
    frame_create(self + SEEDS, u32(self, COLS), u32(self, ROWS), 8);
    DWORD size = u32(self, COLS) * u32(self, ROWS);
    buf_read(f, frame_point(self + ELEV, 0, 0), size);
    size = u32(self, COLS) * u32(self, ROWS);
    buf_read(f, frame_point(self + SEEDS, 0, 0), size);
    u32(self, NHANDLES) = u32(self, COLS) * u32(self, ROWS) * 3;
    rebuild_handles(self);
    return 1;
}

// 0x53f680: the size in pixels; the grid in 64-pixel cells, rounded up
DWORD THISCALL PlasmaMap::SetPixelSize(BYTE *self, DWORD w, DWORD h)
{
    u32(self, 0x2cfc) = w;
    u32(self, 0x2d00) = h;
    DWORD cols = w >> 6, rows = h >> 6;
    if ((w & 0xffffffc0) < w)
        cols++;
    if ((h & 0xffffffc0) < h)
        rows++;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53fe00)(self, cols, rows, 0x40, 0x40);
}

// 0x53fe00
DWORD THISCALL PlasmaMap::SetGrid(BYTE *self, DWORD cols, DWORD rows, DWORD cw, DWORD ch)
{
    u32(self, 0x2cec) = 0;
    u32(self, 0x2d04) = cw;
    u32(self, 0x2d08) = ch;
    u32(self, 0x2d1c) = (DWORD)log2_floor(cw);
    u32(self, 0x2d20) = (DWORD)log2_floor(u32(self, 0x2d08));
    u32(self, COLS) = cols;
    u32(self, 0x2d14) = (1u << (u32(self, 0x2d1c) & 31)) - 1;
    u32(self, ROWS) = rows;
    frame_create(self + ELEV, cols, rows, 8);
    u32(self, 0x2d18) = (1u << (u32(self, 0x2d20) & 31)) - 1;
    frame_create(self + SEEDS, u32(self, COLS), u32(self, ROWS), 8);
    vcall<DWORD>(self + SEEDS, 27, 0);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x480ee0)(self + 8, u32(self, COLS) * u32(self, 0x2d04), u32(self, ROWS) * u32(self, 0x2d08));
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x480f50)(self + 8, 6, 6);
    u32(self, NHANDLES) = u32(self, COLS) * u32(self, ROWS) * 3;
    return rebuild_handles(self);
}

// 0x540420
DWORD THISCALL PlasmaMap::CellOf(BYTE *self, DWORD v) { return ((u32(self, 0x2d04) >> 1) + v) >> (u32(self, 0x2d1c) & 31); }

// 0x540440
DWORD THISCALL PlasmaMap::Elevation(BYTE *self, DWORD x, DWORD y) { return frame_get(self + ELEV, x, y); }

static DWORD *handle_at(BYTE *self, DWORD x, DWORD y, DWORD layer)
{
    return fn<DWORD *(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x542140)(self, x, y, layer);
}
static DWORD *handle_l0(BYTE *self, DWORD x, DWORD y)
{
    return reinterpret_cast<DWORD *>(u32(self, HANDLES) + (u32(self, COLS) * y + x) * 4);
}
static DWORD *handle_l1(BYTE *self, DWORD x, DWORD y)
{
    return reinterpret_cast<DWORD *>(u32(self, HANDLES) + ((u32(self, ROWS) + y) * u32(self, COLS) + x) * 4);
}

// 0x540460
void THISCALL PlasmaMap::SetElevation(BYTE *self, DWORD x, DWORD y, DWORD v)
{
    if (v == frame_get(self + ELEV, x, y))
        return;
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4a8b60)(self + ELEV, x, y, v);
    handle_release(handle_l0(self, x, y));
    handle_release(handle_l1(self, x, y));
    handle_release(handle_at(self, x, y, 2));
    if (x) {
        handle_release(handle_l0(self, x - 1, y));
        handle_release(handle_l1(self, x - 1, y));
        handle_release(handle_at(self, x - 1, y, 2));
    }
    if (!y)
        return;
    y--;
    handle_release(handle_l0(self, x, y));
    handle_release(handle_l1(self, x, y));
    handle_release(handle_at(self, x, y, 2));
    if (!x)
        return;
    x--;
    handle_release(handle_l0(self, x, y));
    handle_release(handle_at(self, x, y, 1));
    handle_release(handle_at(self, x, y, 2));
}

// 0x5405e0
void THISCALL PlasmaMap::AddElevation(BYTE *self, DWORD x, DWORD y, int d)
{
    int v = (int)(frame_get(self + ELEV, x, y) + (DWORD)d);
    if (v > 0xff)
        v = 0xff;
    else if (v < 0)
        v = 0;
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x540460)(self, x, y, (DWORD)v);
}

static int clamp_cell(int q, int max) { return q < 0 ? 0 : q > max ? max : q; }

// 0x540640 (signed divisions; the cell height is not tested, as in the original)
void THISCALL PlasmaMap::ReleaseRect(BYTE *self, const int *rect, DWORD layer)
{
    if (!*glob<void **>(TILE_CACHE))
        return;
    const int cw = (int)u32(self, 0x2d04);
    if (!cw)
        return;
    const int xmax = (int)(u32(self, COLS) - 1);
    const int x0 = clamp_cell(rect[0] / cw, xmax), x1 = clamp_cell(rect[2] / cw, xmax);
    const int ch = (int)u32(self, 0x2d08);
    const int ymax = (int)(u32(self, ROWS) - 1);
    const int y0 = clamp_cell(rect[1] / ch, ymax), y1 = clamp_cell(rect[3] / ch, ymax);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const DWORD h = u32(self, HANDLES), cols = u32(self, COLS), rows = u32(self, ROWS);
            switch (layer) {
            case 0:
                handle_release(reinterpret_cast<DWORD *>(h + (cols * (DWORD)y + (DWORD)x) * 4));
                [[fallthrough]];
            case 1:
                handle_release(reinterpret_cast<DWORD *>(u32(self, HANDLES) + (((DWORD)y + u32(self, ROWS)) * u32(self, COLS) + (DWORD)x) * 4));
                [[fallthrough]];
            case 2:
                handle_release(reinterpret_cast<DWORD *>(u32(self, HANDLES) + (((DWORD)y + u32(self, ROWS) * 2) * u32(self, COLS) + (DWORD)x) * 4));
                break;
            default:
                break;
            }
            (void)rows;
        }
}

// 0x541fb0
DWORD THISCALL PlasmaMap::FlushCache(BYTE *self)
{
    u32(self, 0x2cf8) = 0;
    return fn<DWORD(THISCALL *)(void *)>(0x4d2b50)(*glob<void **>(TILE_CACHE));
}

// 0x541fd0
DWORD THISCALL PlasmaMap::TextureAt(BYTE *self, DWORD x, DWORD y)
{
    const int band = fn<int(THISCALL *)(void *, DWORD, DWORD)>(0x542000)(self, x, y);
    return u32(self, 0x106c + (DWORD)band * 0x1c);
}

// 0x542000: the four grid points (the masked offsets through fild qword, the cell sizes through fidiv
// dword), summed in the original's order
int THISCALL PlasmaMap::BandAt(BYTE *self, DWORD x, DWORD y)
{
    BYTE *elev = self + ELEV;
    const DWORD cx = x >> (u32(self, 0x2d1c) & 31), cy = y >> (u32(self, 0x2d20) & 31);
    const DWORD e00 = frame_get(elev, cx, cy);
    const DWORD e10 = frame_get(elev, cx + 1, cy);
    const DWORD e01 = frame_get(elev, cx, cy + 1);
    const DWORD e11 = frame_get(elev, cx + 1, cy + 1);
    const long double fx = fild64((long long)(x & u32(self, 0x2d14))) / fild((int)u32(self, 0x2d04));
    const long double gx = cst(glob_addr(C_ONE)) - fx;
    const long double fy = fild64((long long)(y & u32(self, 0x2d18))) / fild((int)u32(self, 0x2d08));
    const long double gy = cst(glob_addr(C_ONE)) - fy;
    long double v = fild64((long long)e00) * gy * gx;
    v = v + fild64((long long)e01) * fy * gx;
    v = v + fild64((long long)e10) * gy * fx;
    v = v + fild64((long long)e11) * fy * fx;
    return x87_ftol(v);
}

// 0x542140
DWORD *THISCALL PlasmaMap::Handle(BYTE *self, DWORD x, DWORD y, DWORD layer)
{
    return reinterpret_cast<DWORD *>(u32(self, HANDLES) + ((u32(self, ROWS) * layer + y) * u32(self, COLS) + x) * 4);
}

// ---- The texture list ----------------------------------------------------------------------------------

// 0x4b40f0
TextureList *THISCALL TextureListOps::Ctor(TextureList *self, void *owner)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(self, 0, 0x80, 0, nullptr, 0);
    u32(self, 0x1c) = vt(0x5ed558);
    u32(self, 0x20) = 0;
    u32(self, 0x24) = 0;
    u32(self, 0x28) = 0;
    u32(self, 0x2c) = 0xffffffff;
    u32(self, 0) = vt(VT_TEXTURE_LIST_BASE);
    u32(self, 0x30) = 0;
    u32(self, 0x34) = 0;
    u32(self, 0x138) = 0;
    u32(self, 0x13c) = 0;
    fn<void *(THISCALL *)(void *, DWORD)>(0x4b1cc0)(self->extra, 0);
    u32(self, 0) = vt(VT_TEXTURE_LIST);
    u32(self, 0x154) = (DWORD)(uintptr_t)owner;
    self->extra[0x18] = 0;
    return self;
}

// 0x4b4160 (slot 0 of 0x5f4994, 0x15c bytes)
TextureList *THISCALL TextureListOps::DeletingDtor(TextureList *self, DWORD flags)
{
    fn<void(THISCALL *)(void *)>(0x4b1d20)(self->extra);
    texture_list_destroy_base(self);
    if (flags & 1)
        pool_free(self, 0x15c);
    return self;
}

// 0x4b4220
DWORD THISCALL TextureListOps::ShareExtra(TextureList *self, void *other)
{
    const DWORD r = fn<DWORD(THISCALL *)(void *, void *)>(0x4b1d50)(other, self->extra);
    self->extra[0x18] = 1;
    return r;
}

// 0x4b4430: entry i of src (loaded first when it has no object and is not being loaded) copied into
// entry dst; a src index past its end loads entry 0, but entry i is copied
TextureList *THISCALL TextureListOps::CopyEntry(TextureList *self, DWORD dst, ZDefinitionList *src, DWORD i)
{
    const BYTE *from = nullptr;
    if (src->entries) {
        const DWORD k = i < src->num_entries ? i : 0;
        BYTE *e = reinterpret_cast<BYTE *>(src->entries) + k * 0x80;
        if (!e[0x7f] && !u32(e, 0x78) && !e[0x7f]) {
            e[0x7f] = 1;
            const char *name = vcall<const char *>(src, 1, k);
            char path[0x100];
            Path_Join(path, src->folder, name);
            char *file = const_cast<char *>(path_name(path));
            void **out = reinterpret_cast<void **>(reinterpret_cast<BYTE *>(src->entries) + k * 0x80 + 0x78);
            void *obj;
            if (file == path) {
                obj = vcall<void *>(src, 37, glob<const char *>(0x62e558), file, out);
            } else {
                file[-1] = 0;
                obj = vcall<void *>(src, 37, path, file, out);
            }
            BYTE *e2 = reinterpret_cast<BYTE *>(src->entries) + k * 0x80;
            *reinterpret_cast<void **>(e2 + 0x78) = obj;
            e2[0x7f] = 0;
        }
        from = reinterpret_cast<BYTE *>(src->entries) + i * 0x80;
    }
    memcpy(reinterpret_cast<BYTE *>(self->entries) + dst * 0x80, from, 0x80);
    u32(self, 0x15c) = (DWORD)(uintptr_t)src;
    return self;
}

// 0x4b4540: slot 40, then n + 1 entries (count n), cleared twice as in the original
DWORD THISCALL TextureListOps::SetCount(TextureList *self, DWORD n)
{
    vcall<DWORD>(self, 40);
    u32(self, 0x34) = n;
    const DWORD m = n + 1;
    DWORD *block = static_cast<DWORD *>(heap_alloc(m * 0x80 + 4));
    BYTE *e = nullptr;
    if (block) {
        block[0] = m;
        e = reinterpret_cast<BYTE *>(block + 1);
        if ((int)(m - 1) >= 0)
            for (DWORD i = 0; i < m; i++) {
                BYTE *p = e + i * 0x80;
                p[0] = 0;
                u32(p, 0x78) = 0;
                p[0x7e] = 0;
                p[0x7d] = 0;
                p[0x7c] = 0;
                p[0x7f] = 0;
            }
    }
    u32(self, 0x30) = (DWORD)(uintptr_t)e;
    u32(self, 4) = (DWORD)(uintptr_t)e;
    const DWORD bytes = (u32(self, 0x34) + 1) << 7;
    for (DWORD i = 0; i < bytes; i++)
        e[i] = 0;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(self, 0, 0x80, u32(self, 0x34),
                                                                                        ptr8(self, 0x30), 0);
}

// 0x4b4670: slot 12 of the source list (+0x15c) with the index of its entry holding the same object
DWORD THISCALL TextureListOps::PrecacheEntries(TextureList *self)
{
    for (DWORD i = 0; i < u32(self, 0x34); i++) {
        auto *src = reinterpret_cast<ZDefinitionList *>(ptr8(self, 0x15c));
        const DWORD object = u32(reinterpret_cast<BYTE *>(self->entries) + i * 0x80, 0x78);
        const DWORD n = src->num_entries;
        DWORD j = 0;
        while (j < n && u32(reinterpret_cast<BYTE *>(src->entries) + j * 0x80, 0x78) != object)
            j++;
        if (j >= n)
            j = 0xffffffff;
        vcall<DWORD>(src, 12, j);
    }
    return 0;
}

// ---- Hooks -----------------------------------------------------------------------------------------------

ZAX_HOOKS(plasma_tile_map)
{
    using P = PlasmaMap;
    using T = TextureListOps;
    const Hook list[] = {
#include "plasma_tile_map_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
