// The five screen modes of CScreenFrame::vf23 0x568b40 (batch R5 of the blitter map, docs/DECOMPILATION.md
// 9): ScreenCopy (5-6-5 -> 5-5-5 for a 15-bit display, 8 -> 16 through the 5-5-5 palette) and the brightness
// levels Up/Down for 5-5-5 and 5-6-5 displays, each 16 -> 16 with an MMX and a plain blitter (the
// constructors register one by Cpu_HasMmx, R1).
//
// [confirmed: code; compared in the running game]
//  - All are plain blits (blitters.h: the slots src 31, 46, dst 46, 31, src 40, dst 40; the opaque path asks
//    for the source's palette, the opacity path only sets g_OpacityTable and draws the same).
//  - ScreenCopy's pixel: ((M1 >> 1) & (p >> 1)) | (M0 & p) with the dwords M1 = 0xffc0ffc0 and M0 = 0x001f001f
//    read from .data (0x60cacc/0x60cac8 plain, 0x60cac0/0x60cab8 MMX), the plain row two pixels per dword.
//  - Up/Down (the levels SetLevel writes, R1; their addresses in UP555 ... DOWN565 below): the plain pixel
//    looks up three dword tables (the constructors fill them with min(31, i) or min(63, i), R1) at channel +
//    level; for 5-5-5 the 5-6-5 pixel is first converted like ScreenCopy and green is 5 bits. The MMX groups
//    add (Up, `paddusw`) or subtract (Down, `psubusw`) the level shifted to each channel with saturation: the
//    blitter map's "the same arithmetic" holds for the plain variants only (the MMX Down darkens, the plain
//    Down brightens). The 5-bit level comes as a `psllq` of the four-lane word (lanes carry into the next
//    one), the green step from a word of its own (0x6bbb10 / 0x6bb660 for 5-5-5).
//  - The MMX rows (and the MMX blitters' inline opacity paths): pixels up to an 8-byte aligned destination
//    (stopping at count 0), groups of four, `emms`, the rest with the plain pixel; the rest reads its source
//    from where the groups started, not after them (the destination is advanced) [quirk kept]. The plain
//    ScreenCopy row converts the first pixel of an unaligned destination without testing the count (0 runs
//    away). The MMX blitters end with `emms`.
#include "blitters.h"

namespace zax {

namespace render::blit {

namespace {

// ---- ScreenCopy ----------------------------------------------------------------------------------------------------

inline WORD copy_pixel(DWORD p, DWORD m1, DWORD m0) { return (WORD)(((*at<const DWORD *>(m1) >> 1) & (p >> 1)) | (*at<const DWORD *>(m0) & p)); }

// 0x443cb0 (dst, src; n, palette, left, y): the plain row
void __fastcall ScreenCopyRow(BYTE *dst, const BYTE *src, DWORD n, DWORD, DWORD, DWORD)
{
    WORD *d = reinterpret_cast<WORD *>(dst);
    const WORD *s = reinterpret_cast<const WORD *>(src);
    if ((DWORD)d & 3) {
        *d++ = copy_pixel(*s++, glob_addr(0x60cacc), glob_addr(0x60cac8));
        n--;
    }
    const DWORD pairs = n >> 1, rest = n - pairs * 2;
    for (DWORD i = 0; i < pairs; i++, d += 2, s += 2) {
        const DWORD v = dword_at(reinterpret_cast<const BYTE *>(s));
        *reinterpret_cast<DWORD *>(d) = ((*glob<const DWORD *>(0x60cacc) >> 1) & (v >> 1)) | (*glob<const DWORD *>(0x60cac8) & v);
    }
    for (DWORD i = 0; i < rest; i++)
        *d++ = copy_pixel(*s++, glob_addr(0x60cacc), glob_addr(0x60cac8));
}

// ---- Up / Down -----------------------------------------------------------------------------------------------------

// A data key that may be 0 (the 5-6-5 modes have no 5-5-5 conversion masks, the 5-5-5 ones no 6-bit level): in the
// exe build the own definition or the image's address (as GlobKey; null for 0), in the DLL the address itself
#ifdef ZAX_STANDALONE
struct GlobKeyOr0 {
    DWORD address;
    BYTE *own;
    consteval GlobKeyOr0(DWORD a) : address(a), own(a ? GlobKey(a).own : nullptr) {}
};
template <typename T> inline T glob_or0(GlobKeyOr0 k) { return k.own ? reinterpret_cast<T>(k.own) : at<T>(k.address); }
#else
using GlobKeyOr0 = DWORD;
template <typename T> inline T glob_or0(DWORD a) { return at<T>(a); }
#endif

struct ScreenMode {
    bool rgb555, down;
    GlobKey level5;
    GlobKeyOr0 level6;                     // 5-bit and 6-bit channel levels (5-5-5: level6 unused by the plain pixel)
    GlobKey green_level;                   // the MMX green word
    GlobKey t_green, t_blue, t_red;        // the plain pixel's dword tables
    GlobKeyOr0 row_m1, row_m0;             // 5-5-5: the conversion masks of the row (and the MMX inline path)
    GlobKeyOr0 blit_m1, blit_m0;           // 5-5-5: the plain blitter's
    GlobKey k_red, k_green, k_blue;        // MMX channel masks
    GlobKeyOr0 k_hi, k_lo;                 // 5-5-5: MMX conversion masks
};
constexpr ScreenMode UP555 = {true, false, 0x6bb558, 0, 0x6bbb10, 0x6bb664, 0x6bb764, 0x6bb9a0, 0x60cae8, 0x60cae0, 0x60caf4, 0x60caf0,
                              0x60cad8, 0x60cad0, 0x60cae0, 0x60cae8, 0x60cae0};
constexpr ScreenMode DOWN555 = {true, true, 0x6bb55c, 0, 0x6bb660, 0x6bb864, 0x6bbb14, 0x6bb560, 0x60cb10, 0x60cb08, 0x60cb1c, 0x60cb18,
                                0x60cb00, 0x60caf8, 0x60cb08, 0x60cb10, 0x60cb08};
constexpr ScreenMode UP565 = {false, false, 0x6bbc18, 0x6bc360, 0x6bc360, 0x6bbd24, 0x6bbf24, 0x6bc224, 0, 0, 0, 0,
                              0x60cc48, 0x60cc40, 0x60cc38, 0, 0};
constexpr ScreenMode DOWN565 = {false, true, 0x6bbc1c, 0x6bbd20, 0x6bbd20, 0x6bc024, 0x6bc364, 0x6bbc20, 0, 0, 0, 0,
                                0x60cc60, 0x60cc58, 0x60cc50, 0, 0};

inline DWORD table_at(GlobKey table, int i) { return glob<const DWORD *>(table)[i]; }
inline int level_at(GlobKey a) { return *glob<const int *>(a); }

// The plain pixel; m1/m0 the 5-5-5 conversion masks of the caller
inline WORD level_pixel(const ScreenMode &m, DWORD p, GlobKeyOr0 m1, GlobKeyOr0 m0)
{
    if (m.rgb555) {
        const DWORD c = (((*glob_or0<const DWORD *>(m1) >> 1) & 0xffff) & ((p >> 1) & 0xffff)) | ((*glob_or0<const DWORD *>(m0) & 0xffff) & p);
        const int l = level_at(m.level5);
        return (WORD)(table_at(m.t_green, (int)((c >> 5) & 31) + l) | table_at(m.t_blue, (int)(c & 31) + l) |
                      table_at(m.t_red, (int)(c >> 10) + l));
    }
    const int l6 = *glob_or0<const int *>(m.level6);
    const DWORD g = table_at(m.t_green, (int)((p >> 5) & 63) + l6);
    const int l5 = level_at(m.level5);
    return (WORD)(g | table_at(m.t_blue, (int)(p & 31) + l5) | table_at(m.t_red, (int)(p >> 11) + l5));
}

inline QWORD psubusw(QWORD a, QWORD b)
{
    DWORD r[4];
    for (int i = 0; i < 4; i++)
        r[i] = lane(a, i) > lane(b, i) ? lane(a, i) - lane(b, i) : 0;
    return lanes(r[0], r[1], r[2], r[3]);
}

// Four pixels of an MMX group
inline QWORD level_group(const ScreenMode &m, QWORD q)
{
    const QWORD m4 = dup_low_word(*glob<const DWORD *>(m.level5)) << 11;
    const QWORD m5 = psllw(dup_low_word(*glob<const DWORD *>(m.green_level)), 10);
    auto op = [&](QWORD a, QWORD b) { return m.down ? psubusw(a, b) : paddusw(a, b); };
    const QWORD kr = q_at(m.k_red), kg = q_at(m.k_green), kb = q_at(m.k_blue);
    const QWORD r = op(q & kr, m4) & kr;
    const QWORD g = (op((q & kg) << 5, m5) >> 5) & kg;
    const QWORD b = (op((q & kb) << 11, m4) >> 11) & kb;
    QWORD v = r | g | b;
    if (m.rgb555)
        v = psrlw(v & *glob_or0<const QWORD *>(m.k_hi), 1) | (v & *glob_or0<const QWORD *>(m.k_lo));
    return v;
}
inline QWORD copy_group(QWORD q) { return psrlw(q & q_at(0x60cac0), 1) | (q & q_at(0x60cab8)); }

// The MMX row shape: up to an aligned destination, groups of four, the rest from the groups' start
template <typename PIXEL, typename GROUP> inline void mmx_row(WORD *d, const WORD *s, DWORD n, PIXEL pixel, GROUP group)
{
    while ((DWORD)d & 7) {
        if (!n)
            return;
        *d++ = pixel(*s++);
        n--;
    }
    if (!n)
        return;
    const DWORD groups = n >> 2;
    n -= groups * 4;
    if (groups) {
        const WORD *g = s;
        for (DWORD i = 0; i < groups; i++, g += 4, d += 4)
            q_store(d, group(q_load(g)));
        emms();
    }
    for (; n; n--)
        *d++ = pixel(*s++);
}

// 0x4438e0 ScreenCopy's MMX row and 0x4441e0, 0x444930, 0x445470, 0x445c90 the Up/Down MMX rows
void __fastcall ScreenCopyRowMmx(BYTE *dst, const BYTE *src, DWORD n, DWORD, DWORD, DWORD)
{
    mmx_row(reinterpret_cast<WORD *>(dst), reinterpret_cast<const WORD *>(src), n, [](DWORD p) { return copy_pixel(p, glob_addr(0x60cac0), glob_addr(0x60cab8)); },
            copy_group);
}
template <const ScreenMode &M> void __fastcall LevelRowMmx(BYTE *dst, const BYTE *src, DWORD n, DWORD, DWORD, DWORD)
{
    mmx_row(reinterpret_cast<WORD *>(dst), reinterpret_cast<const WORD *>(src), n, [](DWORD p) { return level_pixel(M, p, M.row_m1, M.row_m0); },
            [](QWORD q) { return level_group(M, q); });
}

// The 16 -> 16 screen blitters: the opaque path through ROW (an exe address) with the source's palette, the
// opacity path with the same row inline (INLINE_ROW, the same as ROW) or through ROW with palette 0
template <DWORD ROW, bool OPACITY_THROUGH_ROW, auto INLINE_ROW>
void __fastcall BlitScreenMmx(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool opaque = opacity_one(mode);
    DWORD palette = 0;
    if (opaque)
        palette = source_palette(src);
    else
        set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 2>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    DWORD y = (DWORD)dr->top;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * 2, d += p.d_pitch * 2, y++) {
        if (opaque || OPACITY_THROUGH_ROW)
            code<PlainRowCall>(ROW)(d, s, p.w, palette, (DWORD)dr->left, y);
        else
            INLINE_ROW(d, s, p.w, 0, (DWORD)dr->left, y);
    }
    emms();
}

// 0x4439e0 ScreenCopy's plain blitter: the opaque path through its row, the opacity path inline
void __fastcall BlitScreenCopy_16To16_NoMMX(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    const bool opaque = opacity_one(mode);
    DWORD palette = 0;
    if (opaque)
        palette = source_palette(src);
    else
        set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 2>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    DWORD y = (DWORD)dr->top;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * 2, d += p.d_pitch * 2, y++) {
        if (opaque)
            fn<PlainRowCall>(0x443cb0)(d, s, p.w, palette, (DWORD)dr->left, y);
        else
            ScreenCopyRow(d, s, p.w, 0, 0, 0);
    }
}

// 0x444410, 0x444b60, 0x445630, 0x445e50: the plain Up/Down blitters, every pixel inline (count tested first)
template <const ScreenMode &M> void __fastcall BlitScreenLevel(CFrame *src, const Rect *sr, CFrame *dst, const Rect *dr, CBlendMode *mode)
{
    if (opacity_one(mode))
        source_palette(src);
    else
        set_opacity_table(mode);
    const PlainSetup p = plain_setup<2, 2>(src, sr, dst, dr);
    BYTE *s = p.s, *d = p.d;
    for (DWORD h = p.h; h; h--, s += p.s_pitch * 2, d += p.d_pitch * 2) {
        const WORD *sw = reinterpret_cast<const WORD *>(s);
        WORD *dw = reinterpret_cast<WORD *>(d);
        for (DWORD n = p.w; n; n--)
            *dw++ = level_pixel(M, *sw++, M.blit_m1, M.blit_m0);
    }
}

}  // namespace

}  // namespace render::blit

using namespace render::blit;

ZAX_HOOKS(blitters_screen)
{
    const Hook list[] = {
#include "blitters_screen_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
