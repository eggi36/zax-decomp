// The frames' TGA files 0x58c6f0-0x58d83d (docs/DECOMPILATION.md 9, "Phase 4 remains, band C"): the reader of a
// TGA in memory into a frame (CFrame slot 63, CFont slot 28, the earlier TGA loader 0x4aa070), its row converters
// per pixel format, the header parser, the RLE row decoder, the writer (FileBuf) and two header queries of a file.
// Every call goes through its exe address.
//
// The header as the parser leaves it (0x14 bytes) [confirmed: 0x58cd30]: +0 the id length, +1 the colour map
// type, +2 the image type (2 true colour, 9 RLE colour mapped, 10 RLE true colour), +4/+6/+8 the colour map
// (start, length, depth), +0xa/+0xc the origin, +0xe/+0x10 width and height, +0x12 the depth (24 or 32 bits),
// +0x13 the descriptor (bit 0x20: rows top down). Only maps without a colour map, of types 2, 9 or 10 and of 24
// or 32 bits are read.
#include "ui_screens.h"

namespace zax::tga {

using namespace ui;

constexpr DWORD ROW_BYTES = 0x7139ec;   // the bytes of a source row (width * 3 or * 4), a global of the exe

inline DWORD vf(BYTE *frame, int slot) { return vc(frame, slot); }
inline WORD pixel16(BYTE r, BYTE g, BYTE b) { return (WORD)fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(r, g, b); }
inline BYTE nearest(DWORD palette, BYTE r, BYTE g, BYTE b)
{
    return (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526e40)(palette, r, g, b, 1, 0x100);
}

// 24 bits to 8 (the palette's nearest colour; black stays index 0)
void __fastcall Tga_Row24To8(BYTE *dst, const BYTE *src, DWORD count, DWORD row, DWORD palette)   // 0x58cab0
{
    if (!count)
        return;
    dst += count * row;
    for (DWORD i = 0; i < count; i++, src += 3)
        dst[i] = !src[2] && !src[1] && !src[0] ? 0 : nearest(palette, src[2], src[1], src[0]);
}

// 32 bits to 8
void __fastcall Tga_Row32To8(BYTE *dst, const BYTE *src, DWORD count, DWORD row, DWORD palette)   // 0x58cb20
{
    if (!count)
        return;
    dst += count * row;
    for (DWORD i = 0; i < count; i++, src += 4)
        dst[i] = !src[2] && !src[1] && !src[0] ? 0 : nearest(palette, src[2], src[1], src[0]);
}

// 32 bits to 16 (Pixel16_FromRGB)
void __fastcall Tga_Row32To16(BYTE *dst, const BYTE *src, DWORD count, DWORD row)   // 0x58cb90
{
    WORD *p = reinterpret_cast<WORD *>(dst + count * row * 2);
    WORD *const end = p + count;
    for (; p < end; p++, src += 4)
        *p = pixel16(src[2], src[1], src[0]);
}

// 24 bits to 24 (a copy)
void __fastcall Tga_Row24To24(BYTE *dst, const BYTE *src, DWORD count, DWORD row)   // 0x58cbe0
{
    if (!count)
        return;
    BYTE *p = dst + count * row * 3;
    for (DWORD i = 0; i < count * 3; i++)
        p[i] = src[i];
}

// 32 bits to 24
void __fastcall Tga_Row32To24(BYTE *dst, const BYTE *src, DWORD count, DWORD row)   // 0x58cc50
{
    if (!count)
        return;
    BYTE *p = dst + count * row * 3;
    for (DWORD i = 0; i < count; i++, p += 3, src += 4) {
        p[0] = src[0];
        p[1] = src[1];
        p[2] = src[2];
    }
}

// 24 bits to 32 (alpha 0 for black, else 0xff)
void __fastcall Tga_Row24To32(BYTE *dst, const BYTE *src, DWORD count, DWORD row)   // 0x58cc90
{
    if (!count)
        return;
    BYTE *p = dst + count * row * 4;
    for (DWORD i = 0; i < count; i++, p += 4, src += 3) {
        p[0] = src[0];
        p[1] = src[1];
        p[2] = src[2];
        p[3] = !src[0] && !src[1] && !src[2] ? 0 : 0xff;
    }
}

// 32 bits to 32
void __fastcall Tga_Row32To32(BYTE *dst, const BYTE *src, DWORD count, DWORD row)   // 0x58ccf0
{
    if (!count)
        return;
    BYTE *p = dst + count * row * 4;
    for (DWORD i = 0; i < count; i++, p += 4, src += 4) {
        p[0] = src[0];
        p[1] = src[1];
        p[2] = src[2];
        p[3] = src[3];
    }
}

// (file bytes, out header): the 18 bytes of the file's header into the padded header; whether it can be read
DWORD __fastcall Tga_ParseHeader(const BYTE *src, BYTE *h)   // 0x58cd30
{
    memcpy(h, src, 0x12);
    h[0] = src[0];
    h[1] = src[1];
    h[2] = src[2];
    memcpy(h + 4, src + 3, 2);
    memcpy(h + 6, src + 5, 2);
    h[8] = src[7];
    memcpy(h + 0xa, src + 8, 2);
    memcpy(h + 0xc, src + 0xa, 2);
    memcpy(h + 0xe, src + 0xc, 2);
    memcpy(h + 0x10, src + 0xe, 2);
    h[0x12] = src[0x10];
    h[0x13] = src[0x11];
    if (h[1] == 0 && (h[2] == 2 || h[2] == 10 || h[2] == 9) && (h[0x12] == 0x18 || h[0x12] == 0x20))
        return 1;
    return 0;
}

// (dst, src, pixels, depth, file name, row): one RLE row decoded; returns the next packet. More pixels than the
// row holds end the game ("The TGA you loaded '%s' is corrupt (line %d).")
const BYTE *__fastcall Tga_DecodeRleRow(BYTE *dst, const BYTE *src, DWORD count, int depth, DWORD name, DWORD row)   // 0x58cde0
{
    DWORD done = 0;
    do {
        const BYTE head = *src++;
        DWORD n = (head & 0x7f) + 1;
        done += n;
        if (head & 0x80) {
            const BYTE c0 = src[0], c1 = src[1], c2 = src[2];
            src += 3;
            if (depth == 0x18) {
                for (; n; n--, dst += 3)
                    dst[0] = c0, dst[1] = c1, dst[2] = c2;
            } else if (depth == 0x20) {
                const BYTE c3 = *src++;
                for (; n; n--, dst += 4)
                    dst[0] = c0, dst[1] = c1, dst[2] = c2, dst[3] = c3;
            }
        } else if (depth == 0x18) {
            for (n *= 3; n; n--)
                *dst++ = *src++;
        } else if (depth == 0x20) {
            for (n *= 4; n; n--)
                *dst++ = *src++;
        }
    } while (done < count);
    if (done > count) {
        char text[0x100];
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x6233b4), name, row);
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x610490), text);
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    return src;
}

// (frame, file bytes, palette (0x633388 by default), allocation flag): the TGA into the frame, reallocated when
// its size differs; every row through the converter of the frame's and the file's depth, bottom up unless the
// descriptor says top down. Returns the file's depth (0 when it cannot be read or the frame's depth is none of
// 8, 16, 24, 32).
DWORD __fastcall Tga_Read(BYTE *frame, const BYTE *data, DWORD name, DWORD palette, DWORD flag)   // 0x58c6f0
{
    alignas(4) BYTE h[0x14];
    if (!fn<DWORD(__fastcall *)(const BYTE *, BYTE *)>(0x58cd30)(data, h))
        return 0;
    const WORD w = m<WORD>(h, 0xe), hh = m<WORD>(h, 0x10);
    const DWORD size = vf(frame, 45) * vf(frame, 39);
    if (size != (DWORD)w * hh || !vf(frame, 31)) {
        fn<void(THISCALL *)(BYTE *)>(0x4aa790)(frame);
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4aa5f0)(frame, w, hh, flag);
    }
    m<WORD>(frame, 0x12) = w;
    m<WORD>(frame, 0x14) = hh;
    const BYTE depth = h[0x12];
    if (depth == 0x18)
        gd(glob_addr(ROW_BYTES)) = vf(frame, 45) * 3;
    else if (depth == 0x20)
        gd(glob_addr(ROW_BYTES)) = vf(frame, 45) << 2;
    if (gd(glob_addr(ROW_BYTES))) {
        const BYTE *src = data + (signed char)h[0] + 0x12;
        BYTE *buffer = nullptr;
        if (h[2] == 10)
            buffer = op_new(gd(glob_addr(ROW_BYTES)) * 2);
        if (!palette)
            palette = glob_addr(0x633388);
        for (DWORD y = 0; y < vf(frame, 39); y++) {
            const DWORD row = (h[0x13] & 0x20) ? y : vf(frame, 39) - y - 1;
            const BYTE *line = src;
            if (h[2] == 10) {
                const DWORD width = vf(frame, 45);
                src = fn<const BYTE *(__fastcall *)(BYTE *, const BYTE *, DWORD, int, DWORD, DWORD)>(0x58cde0)(
                    buffer, src, width, (signed char)depth, name, row);
                line = buffer;
            }
            using Row8 = void(__fastcall *)(BYTE *, const BYTE *, DWORD, DWORD, DWORD);
            using Row = void(__fastcall *)(BYTE *, const BYTE *, DWORD, DWORD);
            switch (vf(frame, 29)) {
            case 8:
                if (depth == 0x18) {
                    const DWORD width = vf(frame, 45);
                    fn<Row8>(0x58cab0)(bp(vf(frame, 31)), line, width, row, palette);
                } else if (depth == 0x20) {
                    const DWORD width = vf(frame, 45);
                    fn<Row8>(0x58cb20)(bp(vf(frame, 31)), line, width, row, palette);
                }
                break;
            case 16:
                if (depth == 0x18) {
                    const DWORD width = vf(frame, 45);
                    WORD *p = reinterpret_cast<WORD *>(bp(vf(frame, 31))) + width * row;
                    WORD *const end = p + width;
                    for (; p < end; p++, line += 3)
                        *p = pixel16(line[2], line[1], line[0]);
                } else if (depth == 0x20) {
                    const DWORD width = vf(frame, 45);
                    fn<Row>(0x58cb90)(bp(vf(frame, 31)), line, width, row);
                }
                break;
            case 24:
                if (depth == 0x18) {
                    const DWORD width = vf(frame, 45);
                    fn<Row>(0x58cbe0)(bp(vf(frame, 31)), line, width, row);
                } else if (depth == 0x20) {
                    const DWORD width = vf(frame, 45);
                    fn<Row>(0x58cc50)(bp(vf(frame, 31)), line, width, row);
                }
                break;
            case 32:
                if (depth == 0x18) {
                    const DWORD width = vf(frame, 45);
                    fn<Row>(0x58cc90)(bp(vf(frame, 31)), line, width, row);
                } else if (depth == 0x20) {
                    const DWORD width = vf(frame, 45);
                    fn<Row>(0x58ccf0)(bp(vf(frame, 31)), line, width, row);
                }
                break;
            default:
                return 0;   // (the row buffer is not freed)
            }
            if (h[2] != 10)
                src += gd(glob_addr(ROW_BYTES));
        }
        if (buffer)
            op_delete(buffer);
    }
    const int d = (signed char)depth;
    if (d)
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4aa2c0)(frame, vf(frame, 45), 1);
    return (DWORD)d;
}

// ---- The writer ------------------------------------------------------------------------------------------------

inline void fb_write(BYTE *fb, const void *p, DWORD n) { fn<void(THISCALL *)(BYTE *, const void *, DWORD)>(0x4a19d0)(fb, p, n); }
inline void fb_char(BYTE *fb, DWORD c) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x4a19f0)(fb, c); }

// (frame, file, open mode): the frame as an RLE TGA (8 bits: colour mapped, type 9, with its palette or a grey
// ramp; 24 and 32 bits: type 10), rows bottom up; a compressed frame is drawn into a plain one first. Runs: 8 bits
// at least 3 equal pixels, 24 and 32 bits at least 2; raw packets up to the next pixel equal to its successor; at
// most 128 pixels, and never past the second to last pixel's limit (the row's last pixel compares with the byte
// behind the row). False when the frame is empty, the file does not open or the depth is another.
DWORD __fastcall Tga_Write(BYTE *frame, const char *file, DWORD mode)   // 0x58cef0
{
    alignas(4) BYTE fb[0x28];
    alignas(4) BYTE plain[0x40];
    fn<void(THISCALL *)(BYTE *)>(0x4a1780)(fb);
    fn<void(THISCALL *)(BYTE *)>(0x4a8a90)(plain);
    BYTE *f = frame;
    const DWORD depth = vf(f, 29);
    if (vf(f, 38) & 6) {
        const DWORD h = vf(f, 39), w = vf(f, 45);
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4ab5a0)(plain, w, h, depth);
        const DWORD y = 0 - vf(f, 44);
        const DWORD x = 0 - vf(f, 43);
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD, DWORD)>(0x4b3280)(plain, f, x, y, glob_addr(0x6bb4e0));
        f = plain;
    }
    if (!vf(f, 31) || !vf(f, 45) || !vf(f, 39) || !fn<BYTE(THISCALL *)(BYTE *, const char *, DWORD)>(0x4a1840)(fb, file, mode | 3)) {
        fn<void(THISCALL *)(BYTE *)>(0x4a8b00)(plain);
        fn<void(THISCALL *)(BYTE *)>(0x4a1800)(fb);
        return 0;
    }
    DWORD row;
    if (depth == 8) {
        row = vf(f, 45);
    } else if (depth == 0x18) {
        row = vf(f, 45) * 3;
    } else if (depth == 0x20) {
        row = vf(f, 45) << 2;
    } else {
        fn<void(THISCALL *)(BYTE *)>(0x4a2100)(fb);
        fn<void(THISCALL *)(BYTE *)>(0x4a8b00)(plain);
        fn<void(THISCALL *)(BYTE *)>(0x4a1800)(fb);
        return 0;
    }
    BYTE b;
    WORD w16;
    b = 0;
    fb_write(fb, &b, 1);
    if (depth == 8) {
        b = 1, fb_write(fb, &b, 1);
        b = 9, fb_write(fb, &b, 1);
        w16 = 0, fb_write(fb, &w16, 2);
        w16 = 0x100, fb_write(fb, &w16, 2);
        b = 0x18;
    } else {
        b = 0, fb_write(fb, &b, 1);
        b = 10, fb_write(fb, &b, 1);
        w16 = 0, fb_write(fb, &w16, 2);
        w16 = 0, fb_write(fb, &w16, 2);
        b = 0;
    }
    fb_write(fb, &b, 1);
    w16 = 0, fb_write(fb, &w16, 2);
    w16 = 0, fb_write(fb, &w16, 2);
    w16 = (WORD)vf(f, 45), fb_write(fb, &w16, 2);
    w16 = (WORD)vf(f, 39), fb_write(fb, &w16, 2);
    b = (BYTE)depth, fb_write(fb, &b, 1);
    b = 0, fb_write(fb, &b, 1);
    BYTE *out = op_new(row * 2);
    if (depth == 8) {
        if ((BYTE)vf(f, 59)) {
            for (DWORD i = 0; i < 0x300; i += 3) {
                fb_char(fb, bp(m(bp(vf(f, 58)), 0))[i]);
                fb_char(fb, bp(m(bp(vf(f, 58)), 0))[i + 1]);
                fb_char(fb, bp(m(bp(vf(f, 58)), 0))[i + 2]);
            }
        } else {
            for (DWORD i = 0; i < 0x100; i++) {
                fb_char(fb, i);
                fb_char(fb, i);
                fb_char(fb, i);
            }
        }
        for (int y = (int)vf(f, 39) - 1; y >= 0; y--) {
            const DWORD w = vf(f, 45);
            const DWORD *rows = reinterpret_cast<const DWORD *>(bp(vf(f, 46)));
            const BYTE *src = bp(vf(f, 31)) + rows[y];
            DWORD n = 0, x = 0;
            while (x < w) {
                const DWORD rest = w - x - 1, lim = rest > 0x7f ? 0x7f : rest;
                DWORD run = 0;
                if (src[x] == src[x + 1]) {
                    while (run < lim) {
                        run++;
                        if (src[x] != src[x + 1 + run])
                            break;
                    }
                }
                if (src[x] == src[x + 1] && run >= 2) {
                    out[n++] = (BYTE)(run | 0x80);
                    out[n++] = src[x];
                    x += run + 1;
                } else {
                    const DWORD k = lim ? lim : 1;
                    out[n++] = (BYTE)(k - 1);
                    for (DWORD i = 0; i < k; i++)
                        out[n++] = src[x++];
                }
            }
            fb_write(fb, out, n);
        }
    } else if (depth == 0x18) {
        for (int y = (int)vf(f, 39) - 1; y >= 0; y--) {
            const DWORD w = vf(f, 45);
            const DWORD *rows = reinterpret_cast<const DWORD *>(bp(vf(f, 46)));
            const DWORD start = rows[y] * 3;
            const BYTE *src = bp(vf(f, 31)) + start;
            DWORD n = 0, x = 0;
            while (x < w) {
                const DWORD rest = w - x - 1, lim = rest > 0x7f ? 0x7f : rest;
                const BYTE *cur = src + x * 3;
                DWORD run = 0;
                if (!memcmp(cur, cur + 3, 3)) {
                    while (run < lim) {
                        run++;
                        if (memcmp(cur, cur + 3 + run * 3, 3))
                            break;
                    }
                }
                if (!memcmp(cur, cur + 3, 3) && run) {
                    out[n] = (BYTE)(run | 0x80);
                    out[n + 1] = cur[0];
                    out[n + 2] = cur[1];
                    out[n + 3] = cur[2];
                    n += 4;
                    x += run + 1;
                } else {
                    DWORD k = 0;
                    if (memcmp(cur, cur + 3, 3))
                        while (k < lim) {
                            k++;
                            if (!memcmp(cur + k * 3, cur + k * 3 + 3, 3))
                                break;
                        }
                    if (!k)
                        k = 1;
                    out[n++] = (BYTE)(k - 1);
                    for (DWORD i = 0; i < k; i++, n += 3) {
                        out[n] = cur[i * 3];
                        out[n + 1] = cur[i * 3 + 1];
                        out[n + 2] = cur[i * 3 + 2];
                    }
                    x += k;
                }
            }
            fb_write(fb, out, n);
        }
    } else {
        for (int y = (int)vf(f, 39) - 1; y >= 0; y--) {
            const DWORD w = vf(f, 45);
            const DWORD *rows = reinterpret_cast<const DWORD *>(bp(vf(f, 46)));
            const DWORD *src = reinterpret_cast<const DWORD *>(bp(vf(f, 31))) + rows[y];
            DWORD n = 0, x = 0;
            while (x < w) {
                const DWORD rest = w - x - 1, lim = rest > 0x7f ? 0x7f : rest;
                const DWORD *cur = src + x;
                DWORD run = 0;
                if (cur[0] == cur[1]) {
                    while (run < lim) {
                        run++;
                        if (cur[0] != cur[1 + run])
                            break;
                    }
                }
                if (cur[0] == cur[1] && run) {
                    const BYTE *c = reinterpret_cast<const BYTE *>(cur);
                    out[n] = (BYTE)(run | 0x80);
                    out[n + 1] = c[0];
                    out[n + 2] = c[1];
                    out[n + 3] = c[2];
                    out[n + 4] = c[3];
                    n += 5;
                    x += run + 1;
                } else {
                    DWORD k = 0;
                    if (cur[0] != cur[1])
                        while (k < lim) {
                            k++;
                            if (cur[k] == cur[k + 1])
                                break;
                        }
                    if (!k)
                        k = 1;
                    out[n++] = (BYTE)(k - 1);
                    const BYTE *c = reinterpret_cast<const BYTE *>(cur);
                    for (DWORD i = 0; i < k * 4; i++)
                        out[n++] = c[i];
                    x += k;
                }
            }
            fb_write(fb, out, n);
        }
    }
    if (out)
        op_delete(out);
    fn<void(THISCALL *)(BYTE *)>(0x4a2100)(fb);
    fn<void(THISCALL *)(BYTE *)>(0x4a8b00)(plain);
    fn<void(THISCALL *)(BYTE *)>(0x4a1800)(fb);
    return 1;
}

// The header of a TGA file (a FileBuf 0x4a1780 opened with 4, 0x14 bytes read into a new block, parsed); null
// when the file does not open or the header cannot be read
template <typename R> inline R with_header(const char *file, R fail, R (*answer)(const BYTE *h))
{
    alignas(4) BYTE fb[0x18];
    alignas(4) BYTE h[0x14];
    fn<void(THISCALL *)(BYTE *)>(0x4a1780)(fb);
    if (fn<BYTE(THISCALL *)(BYTE *, const char *, DWORD)>(0x4a1840)(fb, file, 4)) {
        BYTE *raw = op_new(0x14);
        fn<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4a1b00)(fb, raw, 0x14);
        fn<void(THISCALL *)(BYTE *)>(0x4a2100)(fb);
        if (fn<DWORD(__fastcall *)(const BYTE *, BYTE *)>(0x58cd30)(raw, h)) {
            if (raw)
                op_delete(raw);
            const R r = answer(h);
            fn<void(THISCALL *)(BYTE *)>(0x4a1800)(fb);
            return r;
        }
        if (raw)
            op_delete(raw);
    }
    fn<void(THISCALL *)(BYTE *)>(0x4a1800)(fb);
    return fail;
}

// (file): the TGA is RLE true colour (type 10)
bool __fastcall Tga_FileIsRle(const char *file)   // 0x58d710
{
    return with_header<bool>(file, false, [](const BYTE *h) { return h[2] == 10; });
}

// (file): the TGA's depth (0 when unreadable)
int __fastcall Tga_FileDepth(const char *file)   // 0x58d7b0
{
    return with_header<int>(file, 0, [](const BYTE *h) { return (int)(signed char)h[0x12]; });
}

}  // namespace zax::tga

namespace zax {

using namespace tga;

ZAX_HOOKS(image_file)
{
    const Hook list[] = {
#include "image_file_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
