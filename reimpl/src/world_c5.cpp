// The loading screen and the mini map's image (docs/DECOMPILATION.md 10, "Batch C5"). Every call goes through its
// exe address.
//
// [confirmed: code; VM]
//  - 0x4f3ba0 (cdecl, the loading animation's tick, called while a level loads): only while 0x6c28fc is set;
//    counts 0x6c2900; at most every 50 ms (Platform_Ticks24 0x4d2650 against 0x6c28e0) and with a screen
//    (reporter +4) and the world's loading sequence (+0x308): the sequence's frame for the time (slot 26 · ticks ·
//    0x5f08b0, modulo the frame count of slot 25) is only compared with the current one 0x6c2638, which then
//    advances by one (wrapping); that frame (slot 23) is blitted at (515, 335) with the screen's +0xd8 set to 1.0
//    for the blit and the screen flipped (0x5693a0). The time is kept in 0x6c28e0 unless the 50 ms had not
//    passed.
//  - 0x4f3d50 (the world: the loading background): a screen 640 wide uses the image +0x300 as it is; otherwise a
//    frame +0x304 (made once) of the screen's size and the image's depth gets the image scaled (slot 23, mode
//    0x6b9ca0). eax: the frame.
//  - 0x4ece20 (a layer: load the mini map): "Levels/" + the level's name (the loading world's name while it loads
//    this layer, else the layer's +0xdc) split into folder and name, ".tga" appended; the mini map frame +0x138
//    (made on first use with the world's mini map size (slot 99) and the screen's depth, slot 19 first) loads the
//    file of that name (0x4a9a30); when it fails or the size is not the world's mini map size the image is made
//    new (0x4ed150) when the file exists, else "Fatal Error loading Mini-Map image" and exit(-1).
//  - 0x4ed150 (a layer: make the mini map): the layer drawn into a temporary frame (World::RenderLayer 0x4f4ae0
//    with the callback 0x4ed2b0), the mini map frame cleared to the world's background colour (slot 102) and the
//    world's part of the drawing (+0x1a8-+0x1b4 margins, +0x48/+0x4c the size) scaled into the image rectangle
//    (0x4ec280), then saved (0x4ec600); the temporary frame deleted.
//  - 0x4ec600 (a layer: save the mini map image): the mini map frame copied into an 8-bit frame with the game
//    palette and that into a 24-bit frame, written as "Levels/<file>.tga" (0x4aa070), and the frame's file name set
//    (0x4a9900) and saved with the layer's save data (0x4e7100, 0x4a9af0).
#include "ui_screens.h"
#include "x87.h"

namespace zax::worldc5 {

using namespace ui;
using LD = long double;

inline void *world() { return *glob<void **>(0x6c2080); }
inline DWORD colour_depth() { return fn<DWORD(__cdecl *)()>(0x55d030)(); }
inline void frame_ctor(void *f) { fn<void *(THISCALL *)(void *)>(0x4a8a90)(f); }
inline void frame_dtor(void *f) { fn<void(THISCALL *)(void *)>(0x4a8b00)(f); }
inline void set_size(void *f, DWORD w, DWORD h, DWORD bits) { fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, bits); }
inline void blit_at(void *dst, void *src, DWORD x, DWORD y, DWORD mode) { fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4b3280)(dst, src, x, y, mode); }
inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
inline void zs_append(void *z, const char *t) { fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(z, t); }
inline void zs_split_path(void *z, void *dir, void *name) { fn<void(__fastcall *)(void *, void *, void *)>(0x4e0570)(z, dir, name); }
inline void zs_split_text(const char *t, void *out, DWORD a) { fn<void(__fastcall *)(const char *, void *, DWORD)>(0x4e0770)(t, out, a); }
inline void zs_dash_path(void *z, const char *dir, const char *name, const char *ext)
{
    fn<void(THISCALL *)(void *, const char *, const char *, const char *)>(0x4dffd0)(z, dir, name, ext);
}
// The level's name: the loading world's while it loads this layer, else the layer's +0xdc
inline const char *level_name(BYTE *layer)
{
    if ((BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x4f4d30)(world(), layer) && (BYTE)fn<DWORD(THISCALL *)(void *)>(0x4f4d60)(world()))
        return fn<const char *(THISCALL *)(void *)>(0x4f4d50)(world());
    return c_str(layer + 0xdc);
}
// The mini map frame +0x138, made on first use (pool block, CFrame, slot 19, the world's mini map size, the
// screen's depth)
inline BYTE *mini_map_frame(BYTE *layer)
{
    if (!m(layer, 0x138)) {
        BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
        if (f) {
            frame_ctor(f);
        }
        m(layer, 0x138) = ptrv(f);
        vc(f, 19);
        DWORD size[2];
        vc(world(), 99, size);
        const DWORD depth = colour_depth();
        set_size(bp(m(layer, 0x138)), size[0], size[1], depth);
    }
    return bp(m(layer, 0x138));
}
// "Levels/<level>" split into folder and name, ".tga" appended (the name: the file's c_str first)
inline void dash_path(void *out, void *dir, void *file)
{
    const char *f = c_str(file);
    const char *d = c_str(dir);
    zs_dash_path(out, d, f, glob<const char *>(0x613a28) /* ".tga" */);
}
inline void tga_path(void *out, BYTE *layer)
{
    BYTE levels[4], dir[4], file[4];
    zs_ctor_text(levels, glob<const char *>(0x615d48) /* "Levels/" */);
    zs_append(levels, level_name(layer));
    zs_ctor(dir);
    zs_ctor(file);
    zs_split_path(levels, dir, file);
    dash_path(out, dir, file);
    zs_dtor(file);
    zs_dtor(dir);
    zs_dtor(levels);
}

}  // namespace zax::worldc5

namespace zax {

using namespace worldc5;

namespace {

// 0x4f3ba0
void __cdecl World_LoadingTick()
{
    if (!gb(glob_addr(0x6c28fc)))
        return;
    gd(glob_addr(0x6c2900))++;
    const DWORD now = fn<DWORD(__cdecl *)()>(0x4d2650)();
    if (now - gd(glob_addr(0x6c28e0)) <= 0x32)
        return;
    BYTE *rep = bp(gd(glob_addr(0x6c02cc)));
    BYTE *screen = rep ? bp(m(rep, 4)) : nullptr;
    BYTE *seq = screen ? bp(m(bp(gd(glob_addr(0x6c2080))), 0x308)) : nullptr;
    if (seq) {
        const DWORD n = vc(seq, 25);
        const LD rate = vc<LD>(seq, 26);
        const DWORD at_time = (DWORD)x87_ftol(rate * fild((int)now) * cst(glob_addr(0x5f08b0))) % n;
        if (at_time != gd(glob_addr(0x6c2638))) {
            DWORD cur = gd(glob_addr(0x6c2638)) + 1;
            gd(glob_addr(0x6c2638)) = cur;
            if (cur >= n)
                gd(glob_addr(0x6c2638)) = 0;
            BYTE *s = bp(m(bp(gd(glob_addr(0x6c02cc))), 4));
            const DWORD saved = m(s, 0xd8);
            m(s, 0xd8) = 0x3f800000;
            void *image = bp(vc(seq, 23, gd(glob_addr(0x6c2638))));
            blit_at(screen, image, 0x203, 0x14f, glob_addr(0x6bb4e0));
            m(bp(m(bp(gd(glob_addr(0x6c02cc))), 4)), 0xd8) = saved;
            fn<DWORD(THISCALL *)(void *)>(0x5693a0)(bp(m(bp(gd(glob_addr(0x6c02cc))), 4)));
        }
    }
    gd(glob_addr(0x6c28e0)) = now;
}

// 0x4f3d50 (the world)
DWORD THISCALL World_LoadingBackground(BYTE *w)
{
    if (vc(bp(m(w, 4)), 45) == 0x280)
        return m(w, 0x300);
    if (!m(w, 0x304)) {
        BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
        if (f)
            frame_ctor(f);
        m(w, 0x304) = ptrv(f);
        BYTE *screen = bp(m(w, 4));
        const DWORD bits = vc(bp(m(w, 0x300)), 29);
        const DWORD h = vc(screen, 39);
        set_size(bp(m(w, 0x304)), vc(screen, 45), h, bits);
        int src[4], dst[4];
        const DWORD ih = vc(bp(m(w, 0x300)), 39);
        const DWORD iw = vc(bp(m(w, 0x300)), 45);
        src[0] = 0, src[1] = 0, src[2] = (int)iw, src[3] = (int)ih;
        const DWORD fh = vc(bp(m(w, 0x304)), 39);
        const DWORD fw = vc(bp(m(w, 0x304)), 45);
        dst[0] = 0, dst[1] = 0, dst[2] = (int)fw, dst[3] = (int)fh;
        vc(bp(m(w, 0x304)), 23, m(w, 0x300), src, dst, glob_addr(0x6b9ca0));
    }
    return m(w, 0x304);
}

// 0x4ec600 (a layer)
void THISCALL Map_BuildMiniMapImage(BYTE *layer)
{
    // (all locals live for the whole function, as the original's frame: the frames' constructor stand-in of the
    // checks sees their first bytes)
    struct {
        DWORD world_size[2];
        BYTE path[4], levels[4], dir[4], file[4], folder[4], file_name[4];
        alignas(4) BYTE pal8[0x40], rgb24[0x40];
    } loc;
    __asm__ volatile("" ::"r"(&loc) : "memory");
    BYTE *const path = loc.path, *const pal8 = loc.pal8, *const rgb24 = loc.rgb24, *const folder = loc.folder, *const file_name = loc.file_name;
    vc(world(), 99, loc.world_size);
    zs_ctor(path);
    {
        BYTE *const levels = loc.levels, *const dir = loc.dir, *const file = loc.file;
        zs_ctor_text(levels, glob<const char *>(0x615d48) /* "Levels/" */);
        zs_append(levels, fn<const char *(THISCALL *)(void *)>(0x4eb670)(layer));
        zs_ctor(dir);
        zs_ctor(file);
        zs_split_path(levels, dir, file);
        dash_path(path, dir, file);
        zs_dtor(file);
        zs_dtor(dir);
        zs_dtor(levels);
    }
    frame_ctor(pal8);
    mini_map_frame(layer);
    BYTE *a = mini_map_frame(layer);
    BYTE *b = mini_map_frame(layer);
    {
        const DWORD depth = colour_depth();
        const DWORD h = vc(a, 39);
        set_size(pal8, vc(b, 45), h, depth);
    }
    blit_at(pal8, mini_map_frame(layer), 0, 0, glob_addr(0x6bb4e0));
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4aa390)(pal8, fn<DWORD(__cdecl *)()>(0x433a80)());
    frame_ctor(rgb24);
    mini_map_frame(layer);
    a = mini_map_frame(layer);
    b = bp(m(layer, 0x138));
    {
        const DWORD h = vc(a, 39);
        set_size(rgb24, vc(b, 45), h, 0x18);
    }
    blit_at(rgb24, pal8, 0, 0, glob_addr(0x6bb4e0));
    fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4aa070)(rgb24, c_str(path), 0);
    zs_ctor(folder);
    zs_ctor(file_name);
    zs_split_text(c_str(path), folder, 0);
    const char *name = level_name(layer);
    const char *folder_text = c_str(folder);
    BYTE *mm = mini_map_frame(layer);
    const char *pn = fn<const char *(__fastcall *)(const char *)>(0x4a4570)(name);
    fn<DWORD(THISCALL *)(void *, void *, const char *, const char *, DWORD, DWORD)>(0x4a9900)(mm, file_name, folder_text, pn, 0, 1);
    BYTE *f = fn<BYTE *(THISCALL *)(void *, const char *, DWORD)>(0x4e7100)(layer + 0x3c, c_str(file_name), 0);
    fn<DWORD(THISCALL *)(void *)>(0x4a9af0)(f);
    zs_dtor(file_name);
    zs_dtor(folder);
    frame_dtor(rgb24);
    frame_dtor(pal8);
    zs_dtor(path);
    __asm__ volatile("" ::"r"(&loc) : "memory");
}

// 0x4ed150 (a layer)
void THISCALL Map_SetupMiniMap(BYTE *layer)
{
    DWORD size[2], size2[2];
    vc(world(), 99, size);
    BYTE *tmp = static_cast<BYTE *>(pool_alloc(0x40));
    if (tmp) {
        frame_ctor(tmp);
    }
    fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD)>(0x4f4ae0)(world(), layer, tmp, fn_addr(0x4ed2b0), 0);
    if (!m(layer, 0x138)) {
        BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
        if (f)
            frame_ctor(f);
        m(layer, 0x138) = ptrv(f);
        vc(f, 19);
        vc(world(), 99, size2);
        const DWORD depth = colour_depth();
        set_size(bp(m(layer, 0x138)), size2[0], size2[1], depth);
    }
    BYTE *mm = bp(m(layer, 0x138));
    alignas(4) BYTE colour[16];
    fn<void *(THISCALL *)(void *)>(0x53f000)(colour);
    vc(world(), 102, colour);
    {
        const DWORD depth = colour_depth();
        set_size(mm, size[0], size[1], depth);
    }
    vc(mm, 26, colour);
    int src[4], dst[4];
    src[1] = (int)m(layer, 0x1a8);
    src[0] = (int)m(layer, 0x1b0);
    src[3] = (int)(m(layer, 0x4c) - m(layer, 0x1ac));
    src[2] = (int)(m(layer, 0x48) - m(layer, 0x1b4));
    fn<DWORD(THISCALL *)(void *, int *)>(0x4ec280)(layer, dst);
    vc(mm, 23, tmp, src, dst, glob_addr(0x6b9ca0));
    fn<void(THISCALL *)(void *)>(0x4ec600)(layer);
    if (tmp)
        vc(tmp, 1, 1);
}

// 0x4ece20 (a layer)
void THISCALL Map_LoadMiniMap(BYTE *layer)
{
    DWORD size[2];
    vc(world(), 99, size);
    BYTE path[4];
    zs_ctor(path);
    tga_path(path, layer);
    fn<DWORD(THISCALL *)(DWORD, const char *, DWORD, DWORD)>(0x536080)(glob_addr(0x6c0468), glob<const char *>(0x617f7c) /* "Loading Mini-Map" */, 1, 0);
    BYTE folder[4];
    zs_ctor(folder);
    zs_split_text(c_str(path), folder, 0);
    const char *name = level_name(layer);
    const char *folder_text = c_str(folder);
    BYTE *mm = mini_map_frame(layer);
    const char *pn = fn<const char *(__fastcall *)(const char *)>(0x4a4570)(name);
    bool ok = (BYTE)fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD)>(0x4a9a30)(mm, folder_text, pn, 0, 1, 0);
    if (ok)
        ok = vc(mini_map_frame(layer), 45) == size[0] && vc(mini_map_frame(layer), 39) == size[1];
    if (!ok) {
        if (!(BYTE)fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1650)(c_str(path), 0)) {
            BYTE text[4];
            zs_ctor(text);
            zs_format(text, glob<const char *>(0x617e54), c_str(path));
            fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x617e30) /* "Fatal Error loading Mini-Map image" */,
                                                                        c_str(text));
            fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
        }
        fn<void(THISCALL *)(void *)>(0x4ed150)(layer);
    }
    zs_dtor(folder);
    zs_dtor(path);
}

}  // namespace

ZAX_HOOKS(world_c5)
{
    const Hook list[] = {
#include "world_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
