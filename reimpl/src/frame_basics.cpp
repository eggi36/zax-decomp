// The proving module of the frame check harness: Pixel16_FromRGB and four CFrame slots (see frame_basics.h).
#include "frame_basics.h"
#include "checks_frame_common.h"

namespace zax {

using framec::FieldColor;
using framec::FrameData;
using framec::Rect;

namespace {

constexpr DWORD BLUE_TABLE = 0x630378, GREEN_TABLE = 0x675bb8, RED_TABLE = 0x633394, TABLES_BUILT = 0x630578;

template <typename R, typename... A> R slot(void *obj, int i, A... a)
{
    return reinterpret_cast<R(THISCALL *)(void *, A...)>((*static_cast<DWORD **>(obj))[i])(obj, a...);
}

// 0x432c10 (fastcall dst, value, count on the stack): count bytes of the value's bytes; eax the value
DWORD fill_bytes(BYTE *dst, DWORD value, DWORD count)
{
    return fn<DWORD(__fastcall *)(BYTE *, DWORD, DWORD)>(0x432c10)(dst, value, count);
}

// One 16-bit pixel blended towards c with alpha a as the MMX code computes it (word lanes, 0x613a90..0x613ac0)
WORD blend_mmx(DWORD c, DWORD d, DWORD a)
{
    const DWORD a2 = a >> 2, a3 = a >> 3;
    DWORD t = ((((c >> 1) & 0x3f0) * a2 + ((d >> 1) & 0x3f0) * (0x3f - a2)) & 0xffff) + 0x200;
    t &= 0xffff;
    const DWORD green = ((((t >> 6) + t) & 0xffff) >> 5) & 0x7e0;
    DWORD u = ((d & 0x1f) * (0x1f - a3) + (c & 0x1f) * a3 + ((g_frame_mutation & 1) ? 0x11 : 0x10)) & 0xffff;
    const DWORD blue = (((u >> 5) + u) & 0xffff) >> 5;
    DWORD v = (((d & 0xf800) >> 11) * (0x1f - a3) + ((c & 0xf800) >> 11) * a3 + 0x10) & 0xffff;
    const DWORD red = (((((v >> 5) + v) & 0xffff) >> 5) << 11) & 0xffff;
    return (WORD)(blue | green | red);
}

// The scalar blend of the rest pixels (0x4ac1c4, three copies)
WORD blend_scalar(DWORD c, DWORD d, DWORD a)
{
    const DWORD g = ((((c >> 5) & 0x3f) - ((d >> 5) & 0x3f)) * (a >> 2) >> 1) + (d & 0xffe0);
    const DWORD r = ((c >> 11) - (d >> 11)) * (a >> 3) << 6;
    const DWORD b = ((c & 0x1f) - (d & 0x1f)) * (a >> 3) >> 5;
    return (WORD)((d & 0x1f) + b + (g & 0x7e0) + ((r + (d & 0xf83f)) & 0xf800));
}

}  // namespace

// 0x433a10
DWORD __fastcall Pixel16_FromRGB(DWORD r, DWORD g, DWORD b)
{
    if (*glob<DWORD *>(TABLES_BUILT))
        return glob<DWORD *>(RED_TABLE)[r] + glob<DWORD *>(GREEN_TABLE)[g] + glob<DWORD *>(BLUE_TABLE)[b];
    DWORD r5 = (r + 4) >> 3, g6 = (g + 2) >> 2, b5 = (b + 4) >> 3;
    if (r5 >= 0x20)
        r5 = 0x1f;
    if (g6 >= 0x40)
        g6 = 0x3f;
    if (b5 >= 0x20)
        b5 = 0x1f;
    return (((r5 << 6) + g6) << 5) + b5;
}

namespace {

// 0x4abe90 (CFrame slot 28)
DWORD THISCALL Frame_FillRect(FrameData *f, const Rect *rect, FieldColor *color)
{
    if (!f->pixels)
        return 0;
    Rect r = *rect, clip;
    slot<void>(f, 36, &clip);
    const DWORD hit = fn<DWORD(__fastcall *)(const Rect *, Rect *)>(0x4b2cd0)(&clip, &r);
    if (!(BYTE)hit)
        return hit;
    r.left -= f->x;
    r.right -= f->x;
    r.top -= f->y;
    r.bottom -= f->y;
    const DWORD w = (DWORD)(r.right - r.left), h = (DWORD)(r.bottom - r.top);
    const DWORD pitch = f->rows[1], skip = pitch - w;
    const DWORD depth = f->flags & framec::F_DEPTH;
    if (depth == framec::F_8) {
        const BYTE index = (color->flags & 1) ? color->index : (BYTE)fn<DWORD(THISCALL *)(FieldColor *)>(0x53f160)(color);
        BYTE *p = f->pixels + f->rows[r.top] + r.left;
        DWORD eax = index;
        for (DWORD y = 0; y < h; y++, p += pitch)
            eax = fill_bytes(p, index * 0x01010101u, w);
        return eax;
    }
    if (depth == framec::F_16) {
        const DWORD c = color->pixel16;
        WORD *p = reinterpret_cast<WORD *>(f->pixels) + f->rows[r.top] + r.left;
        if (color->a == 0xff) {
            for (DWORD y = 0; y < h; y++, p += pitch) {
                for (DWORD x = 0; x < w; x++)
                    p[x] = (WORD)c;
                if ((g_frame_mutation & 2) && y == 0)
                    p[w] = (WORD)c;
            }
            return h ? 0 : c;
        }
        const BYTE mmx = (BYTE)fn<DWORD(__cdecl *)()>(0x5156b0)();
        const DWORD a = color->a, groups = w >> 2, rest = w - groups * 4;
        for (DWORD y = 0; y < h; y++) {
            if (mmx && groups) {
                for (DWORD k = 0; k < groups * 4; k++)
                    p[k] = blend_mmx(c, p[k], a);
                p += groups * 4;
            }
            for (DWORD k = 0; k < rest; k++, p++)
                *p = blend_scalar(c, *p, a);
            p += skip;
        }
        return c;
    }
    if (depth == framec::F_24) {
        BYTE *p = f->pixels + (f->rows[r.top] + r.left) * 3;
        const WORD lo = (WORD)(color->b2 | color->g2 << 8);
        const BYTE hi = color->r2;
        if (!h)
            return ((DWORD)r.left & 0xffff0000u) | lo;
        for (DWORD y = 0; y < h; y++) {
            if (w) {
                p[0] = (BYTE)lo;
                p[1] = (BYTE)(lo >> 8);
                p[2] = hi;
                for (DWORD i = 3; i < w * 3; i++)
                    p[i] = p[i - 3];
                p += w * 3;
            }
            p += skip * 3;
        }
        return 0;
    }
    return depth - 0x80;
}

// 0x4a9010 (CFrame slot 25)
DWORD THISCALL Frame_Clear(FrameData *f)
{
    if (!f->pixels)
        return 0;
    const DWORD depth = f->flags & framec::F_DEPTH, k = depth - 0x20;
    if (k > 0xe0)
        return k;
    DWORD n;
    switch (depth) {
    case framec::F_8: n = slot<DWORD>(f, 40) * f->height; break;
    case framec::F_16: n = slot<DWORD>(f, 40) * f->height * 2; break;
    case framec::F_24: n = slot<DWORD>(f, 40) * f->height * 3; break;
    case framec::F_32: n = slot<DWORD>(f, 40) * f->height * 4; break;
    default: return k;
    }
    return fill_bytes(f->pixels, 0, n);
}

// 0x4ac490 (CFrame slot 27)
DWORD THISCALL Frame_Fill(FrameData *f, DWORD value)
{
    const DWORD n = slot<DWORD>(f, 30) * f->height * f->width;
    return fill_bytes(f->pixels, value * 0x01010101u, n);
}

// 0x4aa520 (CFrame slot 36)
DWORD THISCALL Frame_ClipRect(const FrameData *f, Rect *out)
{
    out->left = f->x;
    out->top = f->y;
    out->right = f->x + f->width;
    out->bottom = f->y + f->height;
    return (DWORD)out->bottom;
}

}  // namespace

ZAX_HOOKS(frame_basics)
{
    const Hook list[] = {
        {0x433a10, (void *)&Pixel16_FromRGB, "Pixel16_FromRGB", 5, {0xa1, 0x78, 0x05, 0x63, 0x00, 0x85, 0xc0, 0x74}},
        {0x4abe90, ptr(&Frame_FillRect), "Frame_FillRect", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x4c, 0x53, 0x56}},
        {0x4a9010, ptr(&Frame_Clear), "CFrame::Clear", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0x85, 0xc0}},
        {0x4ac490, ptr(&Frame_Fill), "CFrame::Fill", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x78}},
        {0x4aa520, ptr(&Frame_ClipRect), "CFrame::ClipRect", 7, {0x33, 0xd2, 0x56, 0x66, 0x8b, 0x51, 0x12, 0x33}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
