// The image loaders and the blend table setup left by phase 4 (docs/DECOMPILATION.md 9, "Phase 4 leftovers"): the
// .bmp decoder 0x466b30 and its caller 0x4a9b60, the TGA loader (CFrame slot 63) 0x4a9d70 and InitBlendTables
// 0x433000. Every call goes through its exe address (the frames' own functions through their vtables).
//
// Bmp_Decode [confirmed: code] (fastcall frame, file bytes; palette) al: the header (0x466ac0) into an aligned
// struct; the frame reallocated as 8 bits when its pixel count differs or it has no pixels; +0x12/+0x14 the
// file's width and height (words), 0x6bcec4 the pixel offset, 0x6bcec8 the row stride (by the bit count 1/4/8/24,
// another count keeps the old stride; rounded up to 4; 0 ends with 1). A palette given for an 8-bit frame whose own
// palette (slot 58, when slot 59 says there is one) differs in its 0x300 bytes makes an 8-bit file remapped through
// the table 0x6bceb8 (kind 2, entry 0 stays 0, the others the nearest of the given palette). Rows are stored bottom
// up: in an 8-bit frame 1 bit (the masks 0x60f1a4), 4 bits (two per byte, one past an odd width), 8 bits (copied or
// remapped, 0 stays 0) or 24 bits (black stays 0, else the nearest of the given palette, 0x633388 without one); in a
// 16-bit frame (flags 0x40) 8 bits through the file's colour table and 24 bits, both by Pixel16_FromRGB.
// CFrame::LoadImage 0x4a9b60 (thiscall frame; path, palette mode, palette) al: the file read whole (a CFileBuf, the
// bytes from operator new), the palette holder removed, flag 0x4000 cleared, the file's palette taken (mode, through
// 0x466a40), decoded, the pitch set; the given palette assigned to the frame's; mode 1 with an 8-bit frame maps 0 and
// 0xff to the nearest grey of the palette's entries 0 and 255 (blue byte).
// CFrame::LoadTga 0x4a9d70 (slot 63; thiscall frame; path, filter list, palette, open flags) al: the default filter
// list 0x6be3a8 (3, 6, 7, 0xb or 0xc by the colour depth, 8) when none is given; the file read into the shared
// buffer 0x6be7c0 (size 0x6be7c4: at least 0x10000, doubled; the recursion flag 0x6be7c8 ends the game when set); an
// uncompressed true colour TGA that exists on disk is converted to RLE and written back first (a stack CFrame), one
// that does not ends the game; then ImageFile_Read, the filters, the palette; true when read and the frame has pixels.
// InitBlendTables 0x433000 (fastcall folder, name) returns whether the palette changed: the game palette from
// <name>.bmp (fatal without it) compared with <name>.pal; on a change the cached tables are deleted and the .pal
// written again; then each table loaded from its cache file or built and saved (the inverse colour tables 15/16, the
// gamma tables 0.2/0.4/black bias, Amplify Light, Test Light, the 32 lightness remaps of FadeTable.pal); the pixel
// tables (5-5-5 and 5-6-5 channels), the four scale tables, the four darkened palette lookups, the four /100 ramps,
// the palette's channels and six mode constants.
#include "ui_screens.h"

namespace zax::img {

using namespace ui;

constexpr DWORD BMP_OFFSET = 0x6bcec4, BMP_STRIDE = 0x6bcec8, REMAP = 0x6bceb8, PALETTE_HOLDER_FIELD = 0x6be38c;
constexpr DWORD GAME_PALETTE = 0x633f98, INV15 = 0x633388;

inline DWORD fs(BYTE *f, int slot) { return vc(f, slot); }
inline BYTE nearest(DWORD pal, DWORD a, DWORD b, DWORD c, DWORD first, DWORD end)
{
    return (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526e40)(pal, a, b, c, first, end);
}
inline WORD pixel16(DWORD c, DWORD d, DWORD s) { return (WORD)fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(c, d, s); }
inline void remove_holder(void *f)
{
    if ((BYTE)fn<DWORD(THISCALL *)(DWORD, void *)>(0x418fc0)(gd(glob_addr(PALETTE_HOLDER_FIELD)), f))
        fn<void(THISCALL *)(DWORD, void *)>(0x418fb0)(gd(glob_addr(PALETTE_HOLDER_FIELD)), f);
}

// 0x466b30
DWORD __fastcall Bmp_Decode(BYTE *f, const BYTE *bytes, DWORD palette)
{
    struct Header {
        WORD type;
        DWORD size;
        WORD r1, r2;
        DWORD offset;
        DWORD info;
        int width, height;
        WORD planes, bits;
        DWORD rest[6];
    } h;
    if (!fn<DWORD(__fastcall *)(const BYTE *, Header *)>(0x466ac0)(bytes, &h))
        return 0;
    BYTE *pixels = bp(fs(f, 31));
    const DWORD fw = fs(f, 45);
    if (fw * fs(f, 39) != (DWORD)(h.height * h.width) || !pixels) {
        fn<void(THISCALL *)(BYTE *)>(0x4aa790)(f);
        fn<void(THISCALL *)(BYTE *, int, int, DWORD)>(0x4aa5f0)(f, h.width, h.height, 8);
        pixels = bp(fs(f, 31));
    }
    gd(glob_addr(BMP_OFFSET)) = h.offset;
    m<WORD>(f, 0x12) = (WORD)h.width;
    m<WORD>(f, 0x14) = (WORD)h.height;
    DWORD stride;
    switch (h.bits) {
    case 1: stride = gd(glob_addr(BMP_STRIDE)) = fs(f, 45) >> 3; break;
    case 4: stride = gd(glob_addr(BMP_STRIDE)) = fs(f, 45) >> 1; break;
    case 8: stride = gd(glob_addr(BMP_STRIDE)) = fs(f, 45); break;
    case 0x18: stride = gd(glob_addr(BMP_STRIDE)) = fs(f, 45) * 3; break;
    default: stride = gd(glob_addr(BMP_STRIDE)); break;
    }
    if (stride & 3)
        stride = gd(glob_addr(BMP_STRIDE)) = (stride | 3) + 1;
    if (!stride)
        return 1;
    const BYTE *row = bytes + gd(glob_addr(BMP_OFFSET));
    bool remap = false;
    if (!(fs(f, 38) & 0x40) && palette && (BYTE)fs(f, 59)) {
        const DWORD *own = at<const DWORD *>(*at<DWORD *>(fs(f, 58)));
        const DWORD *given = at<const DWORD *>(gd(palette));
        int k = 0xc0;
        bool same = true;
        while (k-- && (same = *given++ == *own++)) {
        }
        if (!same) {
            remap = true;
            if (h.bits == 8) {
                fn<void(THISCALL *)(DWORD, DWORD)>(0x5268f0)(glob_addr(REMAP), 2);
                const BYTE *src = at<const BYTE *>(*at<DWORD *>(fs(f, 58)));
                *at<BYTE *>(gd(glob_addr(REMAP))) = 0;
                for (DWORD i = 1; i < 0x100; i++) {
                    const BYTE *e = src + 3 * i;
                    const BYTE v = nearest(palette, e[0], e[1], e[2], 0, 0x100);
                    *at<BYTE *>(gd(glob_addr(REMAP)) + i) = v;
                }
            }
        }
    }
    const DWORD true_colour_palette = h.bits == 0x18 && !palette ? glob_addr(INV15) : palette;
    for (DWORD i = 0; i < fs(f, 39); i++, row += gd(glob_addr(BMP_STRIDE))) {
        if (fs(f, 38) & 0x40) {
            if (h.bits == 8) {
                const DWORD y = fs(f, 39) - i - 1;
                const DWORD w = fs(f, 45);
                WORD *d = reinterpret_cast<WORD *>(pixels) + w * y;
                WORD *const end = d + w;
                for (const BYTE *s = row; d < end; d++, s++) {
                    DWORD b = 0, g = 0, r = 0;
                    if (*s) {
                        const BYTE *e = bytes + 0x36 + *s * 4;
                        b = e[0], g = e[1], r = e[2];
                    }
                    *d = pixel16(r, g, b);
                }
            } else if (h.bits == 0x18) {
                const DWORD y = fs(f, 39) - i - 1;
                const DWORD w = fs(f, 45);
                WORD *d = reinterpret_cast<WORD *>(pixels) + w * y;
                WORD *const end = d + w;
                for (const BYTE *s = row; d < end; d++, s += 3)
                    *d = pixel16(s[2], s[1], s[0]);
            }
            continue;
        }
        switch (h.bits) {
        case 1: {
            const DWORD y = fs(f, 39) - i - 1;
            const DWORD w = fs(f, 45);
            if (!w)
                break;
            BYTE *d = pixels + w * y;
            DWORD x = 0;
            do {
                d[x] = (glob<const BYTE *>(0x60f1a4)[x & 7] & row[x >> 3]) ? 1 : 0;
            } while (++x < w);
            break;
        }
        case 4: {
            const DWORD y = fs(f, 39) - i - 1;
            const DWORD w = fs(f, 45);
            if (!w)
                break;
            BYTE *d = pixels + w * y;
            const BYTE *s = row;
            DWORD x = 0;
            do {
                const BYTE c = *s++;
                d[x++] = c >> 4;
                d[x++] = c & 0xf;
            } while (x < w);
            break;
        }
        case 8: {
            const DWORD y = fs(f, 39) - i - 1;
            BYTE *d = pixels + y * fs(f, 45);
            if (!fs(f, 45))
                break;
            const BYTE *s = row;
            DWORD x = 0;
            if (remap) {
                do {
                    const BYTE c = s[x];
                    d[x] = c ? *at<BYTE *>(gd(glob_addr(REMAP)) + c) : 0;
                    x++;
                } while (x < fs(f, 45));
            } else {
                do {
                    d[x] = s[x];
                    x++;
                } while (x < fs(f, 45));
            }
            break;
        }
        case 0x18: {
            const DWORD y = fs(f, 39) - i - 1;
            const DWORD w = fs(f, 45);
            if (!w)
                break;
            BYTE *d = pixels + w * y;
            const BYTE *s = row + 2;
            DWORD x = 0;
            do {
                if (!s[0] && !s[-1] && !s[-2])
                    d[x] = 0;
                else
                    d[x] = nearest(true_colour_palette, s[0], s[-1], s[-2], 1, 0x100);
                s += 3;
            } while (++x < w);
            break;
        }
        default: break;
        }
    }
    return 1;
}

// 0x4a9b60
DWORD THISCALL CFrame_LoadImage(BYTE *f, const char *path, DWORD mode, DWORD palette)
{
    BYTE ok = 1;
    alignas(4) BYTE fb[0x20];
    fn<void(THISCALL *)(void *)>(0x4a1780)(fb);
    if (!(BYTE)fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(fb, path, 4)) {
        ok = 0;
    } else {
        const DWORD size = fn<DWORD(THISCALL *)(void *)>(0x4a2190)(fb);
        BYTE *bytes = op_new(size);
        fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, bytes, size);
        fn<DWORD(THISCALL *)(void *)>(0x4a2100)(fb);
        remove_holder(f);
        m(f, 0x3c) &= ~0x4000u;
        if (mode && !(BYTE)fn<DWORD(__fastcall *)(BYTE *, const BYTE *)>(0x466a40)(f, bytes))
            ok = 0;
        if ((BYTE)fn<DWORD(__fastcall *)(BYTE *, const BYTE *, DWORD)>(0x466b30)(f, bytes, palette))
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4aa2c0)(f, m<WORD>(f, 0x12), 1);
        else
            ok = 0;
        if (palette)
            fn<void(THISCALL *)(DWORD, DWORD)>(0x526700)(fs(f, 58), palette);
        if (mode == 1 && fs(f, 29) == 8) {
            const DWORD pal = fs(f, 58);
            BYTE c = *at<BYTE *>(gd(pal) + 2);
            const BYTE black = nearest(pal, c, c, c, 1, 0xff);
            c = *at<BYTE *>(gd(pal) + 0x2ff);
            const BYTE white = nearest(pal, c, c, c, 1, 0xff);
            DWORD n = fs(f, 45);
            n *= fs(f, 39);
            BYTE *p = bp(fs(f, 31));
            for (; n; n--, p++) {
                if (!*p)
                    *p = black;
                if (*p == 0xff)
                    *p = white;
            }
        }
        op_delete(bytes);
    }
    fn<void(THISCALL *)(void *)>(0x4a1800)(fb);
    return ok;
}

// 0x4a9d70, slot 63
DWORD THISCALL CFrame_LoadTga(BYTE *f, const char *path, DWORD filters, DWORD palette, DWORD flags)
{
    constexpr DWORD LIST = 0x6be3a8, BUFFER = 0x6be7c0, BUFFER_SIZE = 0x6be7c4, BUSY = 0x6be7c8;
    if (!filters) {
        using Add = void(THISCALL *)(DWORD, DWORD);
        fn<void(THISCALL *)(DWORD)>(0x4b1d80)(glob_addr(LIST));
        fn<Add>(0x4b1db0)(glob_addr(LIST), 3);
        fn<Add>(0x4b1db0)(glob_addr(LIST), 6);
        fn<Add>(0x4b1db0)(glob_addr(LIST), 7);
        fn<Add>(0x4b1db0)(glob_addr(LIST), fn<int(__cdecl *)()>(0x55d030)() == 8 ? 0xb : 0xc);
        fn<Add>(0x4b1db0)(glob_addr(LIST), 8);
        filters = glob_addr(LIST);
    }
    BYTE ok = 1, result = 0;
    alignas(4) BYTE fb[0x20];
    fn<void(THISCALL *)(void *)>(0x4a1780)(fb);
    if ((BYTE)fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(fb, path, flags | 4)) {
        const DWORD size = fn<DWORD(THISCALL *)(void *)>(0x4a2190)(fb);
        DWORD want = size < 0x10000 ? 0x10000 : size;
        if (gb(glob_addr(BUSY))) {
            fn<void(__fastcall *)(DWORD, DWORD)>(0x4d24c0)(glob_addr(0x613d2c), glob_addr(0x613d48));
            fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
        }
        gb(glob_addr(BUSY)) = 1;
        BYTE *buf;
        if (gd(glob_addr(BUFFER_SIZE)) < want) {
            if (want <= gd(glob_addr(BUFFER_SIZE)) * 2)
                want = gd(glob_addr(BUFFER_SIZE)) * 2;
            if (gd(glob_addr(BUFFER))) {
                op_delete(bp(gd(glob_addr(BUFFER))));
                gd(glob_addr(BUFFER)) = 0;
            }
            buf = op_new(want);
            gd(glob_addr(BUFFER)) = pv(buf);
            gd(glob_addr(BUFFER_SIZE)) = want;
        } else {
            buf = bp(gd(glob_addr(BUFFER)));
        }
        fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, buf, size);
        fn<DWORD(THISCALL *)(void *)>(0x4a2100)(fb);
        remove_holder(f);
        m(f, 0x3c) &= ~0x4000u;
        const DWORD depth = fn<DWORD(__fastcall *)(const char *)>(0x58d7b0)(path);
        using Read = DWORD(__fastcall *)(void *, const BYTE *, const char *, DWORD, DWORD);
        if (depth && !(BYTE)fn<DWORD(__fastcall *)(const char *)>(0x58d710)(path)) {
            if (!(BYTE)fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1650)(path, 0)) {
                DWORD z;
                zs_ctor(&z);
                zs_format(&z, glob<const char *>(0x613cf8), path);
                using Append = void(THISCALL *)(void *, const char *);
                fn<Append>(0x4e1be0)(&z, glob<const char *>(0x613ca4));
                fn<Append>(0x4e1be0)(&z, glob<const char *>(0x613c5c));
                fn<Append>(0x4e1be0)(&z, glob<const char *>(0x613c2c));
                fn<void(__fastcall *)(DWORD, const char *)>(0x4d24c0)(glob_addr(0x613bac), c_str(&z));
                fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
            }
            alignas(4) BYTE tmp[0x48];
            fn<void(THISCALL *)(void *)>(0x4a8a90)(tmp);
            if (fn<Read>(0x58c6f0)(tmp, buf, path, 0, depth))
                fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4aa070)(tmp, path, 0);
            m(tmp, 0) = vt(0x5f4814);
            remove_holder(tmp);
            m(tmp, 0x3c) &= ~0x4000u;
            fn<void(THISCALL *)(void *)>(0x4aa790)(tmp);
            fn<void(THISCALL *)(void *)>(0x4173f0)(tmp);
            m(tmp, 0) = vt(0x5eb534);
            fn<void(THISCALL *)(void *)>(0x4173f0)(tmp);
            fn<void(THISCALL *)(void *)>(0x4166d0)(tmp);
        }
        const DWORD read = fn<Read>(0x58c6f0)(f, buf, path, palette, depth);
        if (read) {
            fn<void(THISCALL *)(DWORD, BYTE *)>(0x4b21a0)(filters, f);
            result = ok;
        }
        if (palette)
            fn<void(THISCALL *)(DWORD, DWORD)>(0x526700)(fs(f, 58), palette);
        gb(glob_addr(BUSY)) = 0;
        result = result && m(f, 0xc) ? 1 : 0;
    }
    fn<void(THISCALL *)(void *)>(0x4a1800)(fb);
    return result;
}

// ---- InitBlendTables -------------------------------------------------------------------------------------------

namespace {

using PathJoin = void(__fastcall *)(char *, const char *, const char *);
using PathJoinExt = void(__fastcall *)(char *, const char *, const char *, const char *);
inline DWORD pal_load(DWORD pal, const char *path, DWORD kind) { return fn<DWORD(THISCALL *)(DWORD, const char *, DWORD)>(0x5269e0)(pal, path, kind); }
inline void pal_save(DWORD pal, const char *path) { fn<DWORD(THISCALL *)(DWORD, const char *)>(0x526b80)(pal, path); }
inline DWORD frame_palette(void *f) { return fn<DWORD(THISCALL *)(void *)>(0x4aac20)(f); }
inline void file_delete(const char *path) { fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a14e0)(path, 0); }
// The palette's channel c scaled by a float constant k (fild, fmul, __ftol)
inline DWORD scaled_channel(BYTE c, DWORD k) { return (DWORD)x87_ftol((long double)(int)c * (long double)*at<float *>(k)); }

}  // namespace

// 0x433000
DWORD __fastcall InitBlendTables(const char *folder, const char *name)
{
    DWORD changed = 0;
    char path[0x100];
    alignas(4) BYTE frame[0x48];
    alignas(4) DWORD pal[3];
    fn<void(THISCALL *)(void *)>(0x4a8a90)(frame);
    fn<PathJoinExt>(0x4a42c0)(path, folder, name, glob<const char *>(0x60c600));
    if (!(BYTE)fn<DWORD(THISCALL *)(void *, const char *)>(0x4aa180)(frame, path)) {
        DWORD z;
        zs_ctor(&z);
        zs_format(&z, c_str(gd(glob_addr(0x630374))), path);
        const char *caption = c_str(gd(glob_addr(0x6753b4)));
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, c_str(&z));
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    fn<void(THISCALL *)(void *)>(0x5266d0)(pal);
    fn<PathJoinExt>(0x4a42c0)(path, folder, name, glob<const char *>(0x60c5f8));
    pal_load(pv(pal), path, 1);
    if (fn<DWORD(THISCALL *)(void *, DWORD)>(0x526870)(pal, frame_palette(frame))) {
        const DWORD CACHES[6] = {glob_addr(0x60c5e8), glob_addr(0x60c5d8), glob_addr(0x60c5c8),
                                 glob_addr(0x60c5b0), glob_addr(0x60c5a0), glob_addr(0x60c58c)};
        for (DWORD c : CACHES) {
            fn<PathJoin>(0x4a4220)(path, folder, at<const char *>(c));
            file_delete(path);
        }
        fn<PathJoinExt>(0x4a42c0)(path, folder, name, glob<const char *>(0x60c5f8));
        file_delete(path);
        pal_save(frame_palette(frame), path);
        changed = 1;
    }
    fn<void(__fastcall *)(DWORD, DWORD, DWORD)>(0x4337e0)(frame_palette(frame), 1, 0);
    // the cached tables
    struct Table {
        DWORD file, object, kind;
    };
    const Table TABLES[7] = {{glob_addr(0x60c578), glob_addr(INV15), 5},       {glob_addr(0x60c564), glob_addr(0x630368), 7},
                             {glob_addr(0x60c5e8), glob_addr(0x6767d0), 4},    {glob_addr(0x60c5d8), glob_addr(0x6767c0), 4},
                             {glob_addr(0x60c5c8), glob_addr(0x6b972c), 4},    {glob_addr(0x60c5b0), glob_addr(0x6b85f4), 4},
                             {glob_addr(0x60c54c), glob_addr(0x6b9c64), 4}};
    for (int k = 0; k < 7; k++) {
        const Table &t = TABLES[k];
        fn<PathJoin>(0x4a4220)(path, folder, at<const char *>(t.file));
        if (pal_load(t.object, path, t.kind))
            continue;
        switch (k) {
        case 0:
        case 1: fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x527da0)(t.object, glob_addr(GAME_PALETTE), t.kind, 1, 0x100); break;
        case 2: fn<void(THISCALL *)(DWORD, DWORD, float)>(0x527700)(t.object, glob_addr(GAME_PALETTE), 0.2f); break;
        case 3: fn<void(THISCALL *)(DWORD, DWORD, float)>(0x527700)(t.object, glob_addr(GAME_PALETTE), 0.4f); break;
        case 4: fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x527700)(t.object, glob_addr(GAME_PALETTE), 0); break;
        case 5: fn<void(THISCALL *)(DWORD, DWORD)>(0x5277b0)(t.object, glob_addr(GAME_PALETTE)); break;
        default: fn<void(THISCALL *)(DWORD, DWORD)>(0x527980)(t.object, glob_addr(GAME_PALETTE)); break;
        }
        pal_save(t.object, path);
    }
    // the fade table: 32 lightness remaps
    fn<PathJoin>(0x4a4220)(path, folder, glob<const char *>(0x60c5a0));
    if (!(BYTE)fn<DWORD(__fastcall *)(const char *, DWORD, DWORD)>(0x4a1300)(path, glob_addr(0x630b78), 0x2000)) {
        for (DWORD k = 0; k < 0x2000; k += 0x100) {
            alignas(4) DWORD remap[3];
            fn<void(THISCALL *)(void *)>(0x5266d0)(remap);
            const DWORD v = k >> 5;
            fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x527040)(remap, glob_addr(GAME_PALETTE), v, v, v, 1,
                                                                                                                 0xff, 1, 0xff, 0);
            memcpy(at<void *>(glob_addr(0x630b78) + k), at<void *>(remap[0]), 0x100);
            fn<void(THISCALL *)(void *)>(0x5266e0)(remap);
        }
        fn<DWORD(__fastcall *)(const char *, DWORD, DWORD)>(0x4a13f0)(path, glob_addr(0x630b78), 0x2000);
    }
    // the pixel tables
    for (DWORD i = 0; i < 0x100; i++) {
        const long double x = (long double)(unsigned long long)i;
        const int b = x87_ftol(x * (long double)*glob<float *>(0x5eee30) + (long double)*glob<float *>(0x5eee2c));
        gd(glob_addr(0x630378) + 4 * i) = (DWORD)b;
        gd(glob_addr(0x6753b8) + 4 * i) = (DWORD)b << 5;
        gd(glob_addr(0x633b94) + 4 * i) = (DWORD)b << 10;
        gd(glob_addr(0x633394) + 4 * i) = (DWORD)b << 11;
        const int g = x87_ftol(x * (long double)*glob<float *>(0x5eee28) + (long double)*glob<float *>(0x5eee2c));
        gd(glob_addr(0x675bb8) + 4 * i) = (DWORD)g << 5;
    }
    // the scale tables, the darkened palette lookups, the ramps
    DWORD r80 = 0, r60 = 0, r40 = 0, r20 = 0;
    for (DWORD i = 0, o = 0; o < 0x300; i++, o += 3) {
        const long double x = (long double)(unsigned long long)i;
        gd(glob_addr(0x62f318) + 4 * i) = (DWORD)x87_ftol(x * (long double)*glob<float *>(0x5eee24));
        gd(glob_addr(0x62ff30) + 4 * i) = (DWORD)x87_ftol(x * (long double)*glob<float *>(0x5eee20));
        gd(glob_addr(0x630778) + 4 * i) = (DWORD)x87_ftol(x * (long double)*glob<float *>(0x5eee1c));
        gd(glob_addr(0x633fb0) + 4 * i) = (DWORD)x87_ftol(x * (long double)*glob<float *>(0x5eee18));
        const DWORD SCALES[4][2] = {{glob_addr(0x5eb2d0), glob_addr(0x632b80)}, {glob_addr(0x5eee14), glob_addr(0x633794)},
                                    {glob_addr(0x5eee10), glob_addr(0x62ef18)}, {glob_addr(0x5eee0c), glob_addr(0x674bb0)}};
        for (const auto &s : SCALES) {
            const BYTE *p = at<const BYTE *>(gd(glob_addr(GAME_PALETTE)) + o);
            const DWORD c0 = scaled_channel(p[0], s[0]);
            const DWORD c1 = scaled_channel(p[1], s[0]);
            const DWORD c2 = scaled_channel(p[2], s[0]);
            gd(s[1] + 4 * i) = (DWORD)nearest(glob_addr(INV15), c2, c1, c0, 0, 0x100) << 8;
        }
        gd(glob_addr(0x6763c0) + 4 * i) = r80 / 100;
        gd(glob_addr(0x632f88) + 4 * i) = r60 / 100;
        gd(glob_addr(0x6747b0) + 4 * i) = r40 / 100;
        gd(glob_addr(0x62f728) + 4 * i) = r20 / 100;
        r80 += 0x50, r60 += 0x3c, r40 += 0x28, r20 += 0x14;
    }
    const BYTE *p = at<const BYTE *>(gd(glob_addr(GAME_PALETTE)));
    for (DWORD i = 0; i < 0x100; i++, p += 3) {
        gd(glob_addr(0x674fb0) + 4 * i) = p[2];
        gd(glob_addr(0x675fb8) + 4 * i) = p[1];
        gd(glob_addr(0x62eb18) + 4 * i) = p[0];
    }
    gd(glob_addr(0x62f720)) = 0xf800f8;
    gd(glob_addr(0x62fb28)) = 0xf800;
    gd(glob_addr(0x630358)) = 0x20000008;
    gd(glob_addr(0x632b78)) = 0x7c001f;
    gd(glob_addr(0x633fa8)) = 0x3e0;
    gd(glob_addr(0x62f718)) = 0x40000001;
    fn<void(THISCALL *)(void *)>(0x5266e0)(pal);
    fn<void(THISCALL *)(void *)>(0x4a8b00)(frame);
    return changed;
}

}  // namespace zax::img

namespace zax {

using namespace img;

ZAX_HOOKS(image_lo4)
{
    const Hook list[] = {
#include "image_lo4_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
