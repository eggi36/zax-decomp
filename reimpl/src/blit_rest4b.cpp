// The simple warp blend mode's 16-bit RLE blitters (phase 4, rendering): 0x4e2f10 with MMX and 0x4e32c0 without,
// registered for the blend mode 0x6c1a78 (vtable 0x5f88e0, static constructor at 0x4e2d00; functions.csv does
// not list 0x4e32c0 and the constructor).
//
// [confirmed: code; VM] (fastcall source frame, source rectangle; destination frame, destination rectangle,
// mode): a CSimpleWarpEffect-like object on the stack (0x2c bytes: constructor 0x571b70, vtable 0x60049c;
// +8 1.0, +0x2c the mode's warp value 0x6c1ab0) bends the source (its slots 28 and 29 with the source when it
// is a CFrame, else null; 30 and the destructor 0x571bb0 afterwards) while the source is drawn: the whole
// width (left 0 and right the source's slot 45) through Rle16_DrawFull_Copy_EdgeAlpha_MMX 0x439cd0 at
// opacity 1 (the mode's GetOpacity 0x433c20, equal or NaN), else through
// Rle16_DrawFull_Opacity_MMX_EdgeAlphaOpacity_MMX 0x439eb0 with the fade table 0x6347b0 + (mode +0x28) << 10
// in 0x6b67e8; a cut rectangle row by row: the pixel rows (Draw16_RleRowCopy 0x43a5c0 or
// Rle16_DrawRowClipped_Opacity_MMX 0x437d20, when the source has pixels) and the edge rows
// (RleEdge_RowClipped_Alpha_MMX 0x43a6b0 or RleEdge_RowClipped_AlphaOpacity_MMX 0x43a7c0, when it has an
// edge block) into the destination from the destination rectangle's top row; emms at the end.
// 0x4e32c0 is the same with the plain drawers: Rle16_DrawFull 0x43a090 and 0x43a270, the rows 0x43a5c0 and
// 0x437f90, the edge rows 0x43a9f0 and 0x43ab00, and no emms.
// Quirk kept [confirmed: code]: the full path tests the source's own slot 45 even when the source is not a
// CFrame (the warp object then gets null).
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

using Row = void(__fastcall *)(const BYTE *, const BYTE *, const int *, BYTE *, const int *);
using Full = void(__fastcall *)(void *, const int *, void *, const int *);

}  // namespace

namespace {

struct Drawers {
    CodeKey full_copy, full_opacity, row_copy, row_opacity, edge_copy, edge_opacity;
    bool mmx;
};
constexpr Drawers MMX_DRAWERS{0x439cd0, 0x439eb0, 0x43a5c0, 0x437d20, 0x43a6b0, 0x43a7c0, true};
constexpr Drawers PLAIN_DRAWERS{0x43a090, 0x43a270, 0x43a5c0, 0x437f90, 0x43a9f0, 0x43ab00, false};

void warp_blit(const Drawers &d, BYTE *src, const int *sr, BYTE *dst, const int *dr, BYTE *mode)
{
    alignas(4) BYTE warp[0x2c + 4];
    fn<void *(THISCALL *)(void *)>(0x571b70)(warp);
    const DWORD value = *glob<DWORD *>(0x6c1ab0);
    dw(warp, 8) = 0x3f800000;
    dw(warp, 0x2c) = value;
    BYTE *frame = src && is_kind_of(src, fn<CClassInfo *(__cdecl *)()>(0x4a8760)()) ? src : nullptr;
    fn<void(THISCALL *)(void *, void *)>(0x572020)(warp, frame);
    fn<void(THISCALL *)(void *, void *)>(0x572300)(warp, frame);
    if (sr[0] == 0 && (DWORD)sr[2] == vcall<DWORD>(src, 45)) {
        if (c3(fn<long double(THISCALL *)(void *)>(0x433c20)(mode), cst(glob_addr(C_ONE)))) {
            code<Full>(d.full_copy)(src, sr, dst, dr);
        } else {
            *glob<DWORD *>(0x6b67e8) = (dw(mode, 0x28) << 10) + glob_addr(0x6347b0);
            code<Full>(d.full_opacity)(src, sr, dst, dr);
        }
    } else {
        bool opaque = c3(fn<long double(THISCALL *)(void *)>(0x433c20)(mode), cst(glob_addr(C_ONE)));
        // Kept in a register: clang otherwise spilled both 80-bit operands and redid the compare (fldt, fldt,
        // fucompi) before the edge rows, after the pixel rows' MMX literal spans had left the MMX state set
        // (all x87 tags valid, no emms until the end): on real x87 hardware both loads overflow the stack and
        // give the indefinite NaN, the compare is unordered and the edge rows took the opaque drawer
        // (0x43a6b0 instead of 0x43a7c0). The original compares once (fcomp, fnstsw) and branches into two
        // copies of the loops. Only real x86 showed it, the ARM VMs' emulation loads the values
        // (docs/DECOMPILATION.md 12, "Seven checks that differ only on real x86").
        __asm__ volatile("" : "+r"(opaque));
        if (!opaque)
            *glob<DWORD *>(0x6b67e8) = (dw(mode, 0x28) << 10) + glob_addr(0x6347b0);
        const DWORD pitch = vcall<DWORD>(dst, 40);
        BYTE *pixels = vcall<BYTE *>(src, 31);
        BYTE *dpix = vcall<BYTE *>(dst, 31);
        const DWORD *drows = vcall<const DWORD *>(dst, 46);
        DWORD row = (DWORD)sr[1];
        BYTE *const dstart = dpix + drows[dr[1]] * 2;
        const DWORD *srow = vcall<const DWORD *>(src, 46) + row;
        BYTE *dp = dstart;
        if (pixels)
            for (; row < (DWORD)sr[3]; row++, srow++, dp += pitch * 2)
                code<Row>(opaque ? d.row_copy : d.row_opacity)(pixels + *srow, nullptr, sr, dp, dr);
        dp = dstart;
        if (BYTE *edge = vcall<BYTE *>(src, 35)) {
            BYTE *alpha = vcall<BYTE *>(src, 34);
            const DWORD *erow = vcall<const DWORD *>(src, 50) + sr[1];
            const DWORD *arow = vcall<const DWORD *>(src, 49) + sr[1];
            for (DWORD r = (DWORD)sr[1]; r < (DWORD)sr[3]; r++, erow++, arow++, dp += pitch * 2)
                code<Row>(opaque ? d.edge_copy : d.edge_opacity)(edge + *erow, alpha + *arow, sr, dp, dr);
        }
    }
    if (d.mmx)
        __asm__ volatile("emms");
    fn<void(THISCALL *)(void *, void *)>(0x572680)(warp, frame);
    fn<void(THISCALL *)(void *)>(0x571bb0)(warp);
}

}  // namespace

// 0x4e2f10
void __fastcall SimpleWarp_Blit16_MMX(BYTE *src, const int *sr, BYTE *dst, const int *dr, BYTE *mode) { warp_blit(MMX_DRAWERS, src, sr, dst, dr, mode); }
// 0x4e32c0
void __fastcall SimpleWarp_Blit16(BYTE *src, const int *sr, BYTE *dst, const int *dr, BYTE *mode) { warp_blit(PLAIN_DRAWERS, src, sr, dst, dr, mode); }

ZAX_HOOKS(blit_rest4b)
{
    const Hook list[] = {
        {0x4e2f10, (void *)&SimpleWarp_Blit16_MMX, "SimpleWarp_Blit16_MMX", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x44, 0x53, 0x56}},
        {0x4e32c0, (void *)&SimpleWarp_Blit16, "SimpleWarp_Blit16", 5, {0x83, 0xec, 0x40, 0x53, 0x55, 0x56, 0x57, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
