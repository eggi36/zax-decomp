// Frame helpers of band A: the error diffusion of the depth conversion, the .bmp palette, the frame's file
// name, load and save wrappers, the TGA filter list, the texture list's entry, the hue remap row and entry, the
// Filter mode's state at exit (phase 4, rendering; docs/DECOMPILATION.md 9, "Phase 4 remains, band A").
//
// The error diffusion (0x48 bytes; built by 0x4811b0 for the conversions 0x4ace00, 0x4ad920 and the RLE
// encoder's dithering) [confirmed: code]: +0 the row width, +4 the ring length (width + 1), +8 the ring index,
// +0xc the column, +0x10/+0x14/+0x18 three rings of float errors (r, g, b), +0x1c..+0x24 the last value
// (clamped), +0x28..+0x30 the value before the error, +0x34 the threshold, +0x38 its square, +0x3c "snapped",
// +0x40 the palette (0x527000 gives an index's r, g, b), +0x44 the colour table (0x526e40 finds the nearest
// index). It is Floyd-Steinberg on a ring of width + 1 entries: Begin 0x481380 takes the error of the current
// column out of the value and clamps it to 0..1; Spread 0x4814c0 gives the quantisation error to the right
// (7/16), the lower left (3/16, not in the first column), below (5/16) and the lower right (1/16, not in
// the last column) - the red error from the FPU register, green and blue rounded to float first - unless the
// colour is farther than the threshold from the value before the error: then the value itself is taken once
// (al 1, "snapped"). Quantize 0x481290 (r, g, b as floats 0..1): Begin, then the nearest palette index of
// the values * 255 (fistp), its colour / 255, Spread, again while Spread snapped. eax: the index.
// Bmp_ReadHeader 0x466ac0 (fastcall file, header out): the BITMAPFILEHEADER and BITMAPINFOHEADER copied into
// an aligned struct (the reserved words 0). Bmp_ReadPalette 0x466a40 (fastcall frame, file): an 8-bit
// .bmp's 256 RGBQUADs into the frame's palette (slot 58; a new one when it has no data). LoadBmpPalette
// 0x4aa180 (frame; path): the file read into memory, the frame's palette field and flag 0x4000 dropped, the
// palette taken. Frame_FileName 0x4aa9900 (frame; out, folder, name, filters, cached): "<folder>/<name>"
// (cached: under the cache folder Files_CacheFolder) with ".frm", ".frm16" (SetPath3), ".frm24" or ".frm32"
// (SetDashPath) by the filters' depth (or the display's); al 0 for 32 bits and 1 otherwise. Frame_Load
// 0x4a9a30 (frame; folder, name, filters, cached, mode) and Frame_Save 0x4a9af0 (frame; path, mode): the
// file opened (mode | 4, mode | 0x103) and read (0x4a9340) or written (0x4a9740). Frame_ReadFrm 0x4a9340 (frame;
// file buffer): the header (a first byte other than '1': al 0), the origin, size, log2 and flags; a block
// size above 4 MB shows "Fatal Error loading a frame" and exits; each block and its row table read into new
// memory (the table first filled with i * width, max(height, 2) entries) or, from a memory file buffer (+0x18),
// taken in place (FileBuf_Advance). Frame_WriteFrm 0x4a9740 (frame; file buffer): the header (bytes 0xa-0xb
// left as the stack had them; the size of a plain frame pitch * bytes per pixel * height, of a compressed one
// its blocks' first dwords) and every block with its table.
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

inline BYTE *bp_(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T = DWORD> inline T &f(const void *o, DWORD off)
{
    return *reinterpret_cast<T *>(const_cast<BYTE *>(static_cast<const BYTE *>(o)) + off);
}
inline float cf(DWORD a) { return *at<const float *>(a); }
// A value rounded to float through memory, as fstp dword does (clang keeps a float cast on the FPU otherwise)
inline float mem_float(long double v)
{
    volatile float t = (float)v;
    return t;
}

using Thiscall0 = DWORD(THISCALL *)(void *);

// ---- The error diffusion -----------------------------------------------------------------------------------

constexpr DWORD F_ZERO = 0x5ebed0, F_ONE = 0x5ec3f4, F_255 = 0x5eb728, F_INV255 = 0x5f172c;
constexpr DWORD W1 = 0x5f1730, W5 = 0x5f1734, W3 = 0x5f1738, W7 = 0x5f173c;
inline float *ring(BYTE *d, int k) { return reinterpret_cast<float *>(bp_(f(d, 0x10 + 4 * k))); }

BYTE *THISCALL Dither_Ctor(BYTE *d, DWORD width, float threshold, DWORD palette, DWORD table)
{
    f(d, 0x40) = palette;
    f(d, 0) = width;
    f(d, 0x44) = table;
    f<float>(d, 0x38) = mem_float((long double)threshold * threshold);
    f(d, 4) = width + 1;
    f<float>(d, 0x34) = threshold;
    f(d, 0x10) = pv(heap_alloc(f(d, 4) * 4));
    f(d, 0x14) = pv(heap_alloc(f(d, 4) * 4));
    f(d, 0x18) = pv(heap_alloc(f(d, 4) * 4));
    for (DWORD i = 0; i < f(d, 4); i++) {
        f(bp_(f(d, 0x10)), 4 * i) = 0;
        f(bp_(f(d, 0x14)), 4 * i) = 0;
        f(bp_(f(d, 0x18)), 4 * i) = 0;
    }
    f(d, 8) = 0;
    f(d, 0xc) = 0;
    return d;
}

void THISCALL Dither_Free(BYTE *d)
{
    for (DWORD o = 0x10; o <= 0x18; o += 4)
        if (void *p = bp_(f(d, o))) {
            heap_free(p);
            f(d, o) = 0;
        }
}

float clamp01(float v)
{
    if (x87_lt(v, cf(glob_addr(F_ZERO))))
        return cf(glob_addr(F_ZERO));
    if (v > cf(glob_addr(F_ONE)))
        return cf(glob_addr(F_ONE));
    return v;
}

void THISCALL Dither_Begin(BYTE *d, float *r, float *g, float *b)
{
    f<float>(d, 0x28) = *r;
    f<float>(d, 0x2c) = *g;
    f<float>(d, 0x30) = *b;
    const DWORD i = f(d, 8);
    const float eg = ring(d, 1)[i], eb = ring(d, 2)[i];
    *r = mem_float((long double)*r - ring(d, 0)[i]);
    *g = mem_float((long double)*g - eg);
    *b = mem_float((long double)*b - eb);
    ring(d, 0)[i] = 0;
    ring(d, 1)[i] = 0;
    ring(d, 2)[i] = 0;
    if (++f(d, 8) >= f(d, 4))
        f(d, 8) = 0;
    if (++f(d, 0xc) >= f(d, 0))
        f(d, 0xc) = 0;
    *r = clamp01(*r);
    *g = clamp01(*g);
    *b = clamp01(*b);
    f<float>(d, 0x1c) = *r;
    f<float>(d, 0x20) = *g;
    d[0x3c] = 0;
    f<float>(d, 0x24) = *b;
}

DWORD THISCALL Dither_Spread(BYTE *d, float *r, float *g, float *b)
{
    if (!d[0x3c]) {
        const long double dr = (long double)f<float>(d, 0x28) - *r;
        const long double dg = (long double)f<float>(d, 0x2c) - *g;
        const long double db = (long double)f<float>(d, 0x30) - *b;
        const long double dist = (db * db + dg * dg) + dr * dr;
        if (dist > f<float>(d, 0x38)) {
            d[0x3c] = 1;
            *r = f<float>(d, 0x28);
            *g = f<float>(d, 0x2c);
            *b = f<float>(d, 0x30);
            f<float>(d, 0x1c) = *r;
            f<float>(d, 0x20) = *g;
            f<float>(d, 0x24) = *b;
            return 1;
        }
    }
    const long double er = (long double)*r - f<float>(d, 0x1c);   // kept on the FPU
    const float eg = mem_float((long double)*g - f<float>(d, 0x20));
    const float eb = mem_float((long double)*b - f<float>(d, 0x24));
    auto add = [&](DWORD k, DWORD w) {
        float *rr = ring(d, 0), *rg = ring(d, 1), *rb = ring(d, 2);
        rr[k] = (float)(er * cf(w) + rr[k]);
        rg[k] = (float)((long double)eg * cf(w) + rg[k]);
        rb[k] = (float)((long double)eb * cf(w) + rb[k]);
    };
    add(f(d, 8), glob_addr(W7));
    if (f(d, 0xc)) {
        DWORD k = f(d, 0) + f(d, 8) - 2;
        if (k >= f(d, 4))
            k -= f(d, 4);
        add(k, glob_addr(W3));
    }
    DWORD k = f(d, 0) + f(d, 8) - 1;
    if (k >= f(d, 4))
        k -= f(d, 4);
    add(k, glob_addr(W5));
    if (f(d, 0xc) + 1 < f(d, 0)) {
        k = f(d, 0) + f(d, 8);
        if (k >= f(d, 4))
            k -= f(d, 4);
        add(k, glob_addr(W1));
    }
    return 0;
}

DWORD THISCALL Dither_Quantize(BYTE *d, float r, float g, float b)
{
    Dither_Begin(d, &r, &g, &b);
    DWORD index;
    do {
        const int ib = x87_fistp(mem_float((long double)b * cf(glob_addr(F_255))));
        const int ig = x87_fistp(mem_float((long double)g * cf(glob_addr(F_255))));
        const int ir = x87_fistp(mem_float((long double)r * cf(glob_addr(F_255))));
        index = fn<DWORD(THISCALL *)(void *, int, int, int, DWORD, DWORD)>(0x526e40)(bp_(f(d, 0x44)), ir, ig, ib, 1, 0x100);
        DWORD cr, cg, cb;
        fn<void(THISCALL *)(void *, DWORD, DWORD *, DWORD *, DWORD *)>(0x527000)(bp_(f(d, 0x40)), index, &cr, &cg, &cb);
        r = mem_float((long double)cr * cf(glob_addr(F_INV255)));
        g = mem_float((long double)cg * cf(glob_addr(F_INV255)));
        b = mem_float((long double)cb * cf(glob_addr(F_INV255)));
    } while (Dither_Spread(d, &r, &g, &b) & 0xff);
    return index;
}

// ---- .bmp --------------------------------------------------------------------------------------------------

DWORD __fastcall Bmp_ReadHeader(const BYTE *s, BYTE *h)
{
    h[0] = s[0];
    h[1] = s[1];
    f(h, 4) = f(s, 2);
    f<WORD>(h, 8) = 0;
    f<WORD>(h, 0xa) = 0;
    f(h, 0xc) = f(s, 0xa);
    f(h, 0x10) = f(s, 0xe);
    f(h, 0x14) = f(s, 0x12);
    f(h, 0x18) = f(s, 0x16);
    f<WORD>(h, 0x1c) = f<WORD>(s, 0x1a);
    f<WORD>(h, 0x1e) = f<WORD>(s, 0x1c);
    for (DWORD o = 0x20; o <= 0x34; o += 4)
        f(h, o) = f(s, o - 2);
    return 1;
}

DWORD __fastcall Bmp_ReadPalette(BYTE *frame, const BYTE *file)
{
    alignas(4) BYTE h[0x38];
    if (!Bmp_ReadHeader(file, h) || f<WORD>(h, 0x1e) != 8)
        return 0;   // al 0 (the upper bytes as the header copy left them)
    if (!*vcall<DWORD *>(frame, 58))
        fn<Thiscall0>(0x4aa770)(frame);
    BYTE *p = bp_(*vcall<DWORD *>(frame, 58));
    const BYTE *q = file + 0x36;
    for (int i = 0; i < 0x100; i++, q += 4, p += 3) {
        p[0] = q[0];
        p[1] = q[1];
        p[2] = q[2];
    }
    return 1;
}

inline void palette_field_drop(BYTE *frame)
{
    void *field = bp_(*glob<DWORD *>(0x6be38c));
    if (fn<BYTE(THISCALL *)(void *, void *)>(0x418fc0)(field, frame))
        fn<void(THISCALL *)(void *, void *)>(0x418fb0)(bp_(*glob<DWORD *>(0x6be38c)), frame);
    f(frame, 0x3c) &= ~0x4000u;
}

// FileBuf on the stack (0x1c bytes; constructor 0x4a1780, destructor 0x4a1800)
struct FileBuf {
    alignas(4) BYTE raw[0x1c];
    FileBuf() { fn<Thiscall0>(0x4a1780)(raw); }
    ~FileBuf() { fn<Thiscall0>(0x4a1800)(raw); }
    bool open(const char *path, DWORD mode) { return fn<BYTE(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(raw, path, mode) != 0; }
    void close() { fn<Thiscall0>(0x4a2100)(raw); }
};

BYTE THISCALL LoadBmpPalette(BYTE *frame, const char *path)
{
    BYTE ok = 1;
    FileBuf fb;
    if (!fb.open(path, 4))
        return 0;
    const DWORD size = fn<Thiscall0>(0x4a2190)(fb.raw);
    BYTE *data = static_cast<BYTE *>(heap_alloc(size));
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb.raw, data, size);
    fb.close();
    palette_field_drop(frame);
    if (!(Bmp_ReadPalette(frame, data) & 0xff))
        ok = 0;
    heap_free(data);
    return ok;
}

// ---- The frame's files -------------------------------------------------------------------------------------

inline void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
inline void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
inline const char *zs_c_str(void *z) { return fn<const char *(THISCALL *)(void *)>(0x4e13a0)(z); }

BYTE THISCALL Frame_FileName(BYTE *, void *out, const char *folder, const char *name, DWORD filters, BYTE cached)
{
    DWORD dir;
    zs_ctor(&dir);
    if (cached) {
        const char *cache = fn<const char *(__cdecl *)(const char *)>(0x4a1140)(folder);
        fn<void(THISCALL *)(void *, const char *, const char *)>(0x4dfa50)(&dir, cache, folder);
    } else {
        fn<void(THISCALL *)(void *, const char *)>(0x4e1930)(&dir, folder);
    }
    BYTE r = 1;
    // data_depth: the opt-in TrueColorData16 (reimpl.h); the original's depth without it
    const DWORD depth = data_depth(filters ? fn<DWORD(THISCALL *)(DWORD)>(0x4b2190)(filters) : fn<DWORD(__cdecl *)()>(0x55d030)());
    using Path3 = void(THISCALL *)(void *, const char *, const char *, const char *);
    switch (depth) {
    case 8: fn<Path3>(0x4e02c0)(out, zs_c_str(&dir), name, glob<const char *>(0x613c24)); break;
    case 16: fn<Path3>(0x4e02c0)(out, zs_c_str(&dir), name, glob<const char *>(0x613c1c)); break;
    case 24: fn<Path3>(0x4dffd0)(out, zs_c_str(&dir), name, glob<const char *>(0x613c14)); break;
    case 32:
        fn<Path3>(0x4dffd0)(out, zs_c_str(&dir), name, glob<const char *>(0x613c0c));
        r = 0;
        break;
    default: break;
    }
    zs_dtor(&dir);
    return r;
}

// g_PaletteHolderField 0x6be38c's value for Frame_Load's instructions (a leaf: only its return address goes on the
// stack, into the slot the exe's next push takes)
extern "C" void *__cdecl zax_palette_holder_field() { return bp_(*glob<DWORD *>(0x6be38c)); }

// 0x4a9a30, written as the exe's instructions (batch nulltest, docs/DECOMPILATION.md section 16): on a file that
// ends early CFrame::ReadFrm 0x4a9340 takes its header from its own stack slots as the stack had them, so it and the
// calls before it run at the exe's depth (sub esp, 0x20; ebx, esi; the ZString at +8, the FileBuf at +0xc). The C++
// body called it 8 bytes deeper. Calls go through eax where the exe's relative call leaves eax free.
__attribute__((naked)) BYTE THISCALL Frame_Load(BYTE *, const char *, const char *, DWORD, DWORD, DWORD)
{
    __asm__("subl $0x20, %esp\n\t"
            "pushl %ebx\n\t"
            "pushl %esi\n\t"
            "movl %ecx, %esi\n\t"
            "calll _zax_palette_holder_field\n\t"
            "movl %eax, %ecx\n\t"
            "pushl %esi\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x418fc0) ", %eax\n\t"   // FieldDesc_IsDynamicIn
            "calll *%eax\n\t"
            "testb %al, %al\n\t"
            "je 1f\n\t"
            "calll _zax_palette_holder_field\n\t"
            "movl %eax, %ecx\n\t"
            "pushl %esi\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x418fb0) ", %eax\n\t"   // FieldDesc_RemoveDynamicFrom
            "calll *%eax\n"
            "1:\n\t"
            "movl 0x3c(%esi), %eax\n\t"
            "movl %esi, %ecx\n\t"
            "andb $0xbf, %ah\n\t"
            "movl %eax, 0x3c(%esi)\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4aa790) ", %eax\n\t"   // CFrame_Free
            "calll *%eax\n\t"
            "leal 8(%esp), %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4dec90) ", %eax\n\t"   // ZString_Ctor
            "calll *%eax\n\t"
            "movl 0x38(%esp), %eax\n\t"
            "movl 0x34(%esp), %ecx\n\t"
            "movl 0x30(%esp), %edx\n\t"
            "pushl %eax\n\t"
            "movl 0x30(%esp), %eax\n\t"
            "pushl %ecx\n\t"
            "pushl %edx\n\t"
            "leal 0x14(%esp), %ecx\n\t"
            "pushl %eax\n\t"
            "pushl %ecx\n\t"
            "movl %esi, %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a9900) ", %eax\n\t"   // CFrame::FileName
            "calll *%eax\n\t"
            "leal 0xc(%esp), %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a1780) ", %eax\n\t"   // FileBuf_Ctor
            "calll *%eax\n\t"
            "movl 0x3c(%esp), %edx\n\t"
            "leal 8(%esp), %ecx\n\t"
            "orl $4, %edx\n\t"
            "pushl %edx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4e13a0) ", %eax\n\t"   // ZString_c_str
            "calll *%eax\n\t"
            "pushl %eax\n\t"
            "leal 0x14(%esp), %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a1840) ", %eax\n\t"   // FileBuf_Open
            "calll *%eax\n\t"
            "testb %al, %al\n\t"
            "je 2f\n\t"
            "leal 0xc(%esp), %eax\n\t"
            "movl %esi, %ecx\n\t"
            "pushl %eax\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a9340) ", %eax\n\t"   // CFrame::ReadFrm
            "calll *%eax\n\t"
            "leal 0xc(%esp), %ecx\n\t"
            "movb %al, %bl\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a2100) ", %eax\n\t"   // FileBuf_Close
            "calll *%eax\n\t"
            "jmp 3f\n"
            "2:\n\t"
            "xorb %bl, %bl\n"
            "3:\n\t"
            "leal 0xc(%esp), %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a1800) ", %eax\n\t"   // FileBuf_Dtor
            "calll *%eax\n\t"
            "leal 8(%esp), %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4defd0) ", %eax\n\t"   // ZString_Dtor
            "calll *%eax\n\t"
            "movb %bl, %al\n\t"
            "popl %esi\n\t"
            "popl %ebx\n\t"
            "addl $0x20, %esp\n\t"
            "retl $0x14");
}

BYTE THISCALL Frame_Save(BYTE *frame, const char *path, DWORD mode)
{
    FileBuf fb;
    if (!fb.open(path, mode | 0x103))
        return 0;
    const BYTE ok = fn<BYTE(THISCALL *)(void *, void *)>(0x4a9740)(frame, fb.raw);
    fb.close();
    return ok;
}

// ---- The .frm reader and writer ----------------------------------------------------------------------------

// The five blocks of a frame: (block, its row table) offsets
constexpr DWORD BLOCKS[5][2] = {{0xc, 0x18}, {0x1c, 0x20}, {0x24, 0x28}, {0x2c, 0x30}, {0x34, 0x38}};
inline DWORD fb_read(void *fb, void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, p, n); }
inline DWORD fb_advance(void *fb, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a2150)(fb, n); }
inline void fb_write(void *fb, const void *p, DWORD n) { fn<void(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(fb, p, n); }

BYTE log2_of(WORD v)
{
    BYTE n = 0;
    if (v & 0xfffe) {
        DWORD d = 1;
        do {
            n++;
            d++;
        } while ((WORD)(v >> d));
    }
    return n;
}

// .frm header (0x24): '1', bits, -x, -y, width, height (words), 2 bytes not written, flags, the five block sizes
BYTE THISCALL Frame_ReadFrm(BYTE *frame, BYTE *fb)
{
    alignas(4) BYTE h[0x24];
    fb_read(fb, h, 0x24);
    if (h[0] != '1')
        return 0;
    f<WORD>(frame, 0xa) = (WORD)-f<WORD>(h, 4);
    f<WORD>(frame, 8) = (WORD)-f<WORD>(h, 2);
    f<WORD>(frame, 0x12) = f<WORD>(h, 6);
    f<WORD>(frame, 0x14) = f<WORD>(h, 8);
    frame[0x10] = log2_of(f<WORD>(h, 6));
    frame[0x11] = log2_of(f<WORD>(h, 8));
    f(frame, 0x3c) = f(h, 0xc);
    for (int k = 0; k < 5; k++)
        if (f(h, 0x10 + 4 * k) > 0x400000) {
            fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x613bac), glob<const char *>(0x613bc8));
            fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
        }
    const bool in_memory = fb[0x18] != 0;
    for (int k = 0; k < 5; k++) {
        const DWORD size = f(h, 0x10 + 4 * k);
        if (!size)
            continue;
        if (in_memory) {
            f(frame, BLOCKS[k][0]) = fb_advance(fb, size);
            f(frame, BLOCKS[k][1]) = fb_advance(fb, (DWORD)f<WORD>(frame, 0x14) << 2);
            continue;
        }
        void *block = heap_alloc(size);
        f(frame, BLOCKS[k][0]) = pv(block);
        fb_read(fb, block, size);
        DWORD n = f<WORD>(frame, 0x14);
        const DWORD w = f<WORD>(frame, 0x12);
        if (n < 2)
            n = 2;
        DWORD *t = static_cast<DWORD *>(heap_alloc(n * 4));
        for (DWORD i = 0, v = 0; i < n; i++, v += w)
            t[i] = v;
        f(frame, BLOCKS[k][1]) = pv(t);
        fb_read(fb, t, (DWORD)f<WORD>(frame, 0x14) << 2);
    }
    return 1;
}

BYTE THISCALL Frame_WriteFrm(BYTE *frame, void *fb)
{
    alignas(4) BYTE h[0x24];   // bytes 0xa-0xb are not written (the stack's, as in the original)
    h[0] = '1';
    h[1] = (BYTE)vcall<DWORD>(frame, 29);
    f<WORD>(h, 2) = (WORD)-f<WORD>(frame, 8);
    f<WORD>(h, 6) = f<WORD>(frame, 0x12);
    f<WORD>(h, 4) = (WORD)-f<WORD>(frame, 0xa);
    f<WORD>(h, 8) = f<WORD>(frame, 0x14);
    const DWORD flags = f(frame, 0x3c);
    f(h, 0xc) = flags;
    DWORD size[5] = {};
    if (!(flags & 6)) {
        const DWORD pitch = vcall<DWORD>(frame, 40);
        size[0] = pitch * vcall<DWORD>(frame, 30) * f<WORD>(frame, 0x14);
    } else {
        size[0] = f(bp_(f(frame, 0xc)), 0);
        for (int k = 1; k < 5; k++)
            size[k] = f(frame, BLOCKS[k][0]) ? f(bp_(f(frame, BLOCKS[k][0])), 0) : 0;
    }
    for (int k = 0; k < 5; k++)
        f(h, 0x10 + 4 * k) = size[k];
    fb_write(fb, h, 0x24);
    for (int k = 0; k < 5; k++)
        if (size[k]) {
            fb_write(fb, bp_(f(frame, BLOCKS[k][0])), size[k]);
            fb_write(fb, bp_(f(frame, BLOCKS[k][1])), (DWORD)f<WORD>(frame, 0x14) << 2);
        }
    return 1;
}

// Frame_WriteTga 0x4aa070 (frame; path, flags): a 16-bit frame is copied into a 24-bit one (origin 0 for the
// blit) and that one written; others through ImageFile_Write. al: written.
BYTE THISCALL Frame_WriteTga(BYTE *frame, const char *path, DWORD flags)
{
    if (vcall<DWORD>(frame, 29) != 0x10)
        return fn<DWORD(__fastcall *)(void *, const char *, DWORD)>(0x58cef0)(frame, path, flags) ? 1 : 0;
    alignas(4) BYTE copy[0x40];
    fn<Thiscall0>(0x4a8a90)(copy);
    const DWORD a = fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4aa5f0)(copy, f<WORD>(frame, 0x12), f<WORD>(frame, 0x14), 0x18);
    fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4aa2c0)(copy, a, 1);
    const DWORD origin = f(frame, 8);
    f<WORD>(frame, 8) = 0;
    f<WORD>(frame, 0xa) = 0;
    fn<void(THISCALL *)(void *, void *, int, int, DWORD)>(0x4b3280)(copy, frame, 0, 0, glob_addr(0x6bb4e0));
    f(frame, 8) = origin;
    const BYTE ok = fn<BYTE(THISCALL *)(void *, const char *, DWORD)>(0x4aa070)(copy, path, flags);
    // the CFrame destructor, inline
    f(copy, 0) = vt(0x5f4814);
    palette_field_drop(copy);
    fn<Thiscall0>(0x4aa790)(copy);
    fn<Thiscall0>(0x4173f0)(copy);
    f(copy, 0) = vt(0x5eb534);
    fn<Thiscall0>(0x4173f0)(copy);
    fn<Thiscall0>(0x4166d0)(copy);
    return ok;
}

// TextureList::LoadEntry 0x4b4240 (slot 37 of the ground texture list; dir, name, out): the texture's frame
// object from "<dir>/<name>.TXT" (an object stream: the class name on its first line, made by the class
// registry, read from the stream while g_LastFileOpened 0x713c14 names the file) or, without the file, a new
// object of the list's frame class (+0x154, slot 27; its slot 19 called before the null test); unless it is
// a plain CFrame it is written back as the .TXT; then its image loaded with the list's filters (+0x140,
// made once from the TGA filter list, flag +0x158) as a cached frame. eax: the frame or 0.
void *THISCALL TextureList_LoadEntry(BYTE *list, const char *dir, const char *name, void **out)
{
    using Fn1 = DWORD(THISCALL *)(void *, DWORD);
    DWORD path;
    zs_ctor(&path);
    fn<void(THISCALL *)(void *, const char *, const char *, const char *)>(0x4e02c0)(&path, dir, name, glob<const char *>(0x610388));
    alignas(4) BYTE os[0x24];
    fn<Thiscall0>(0x5240b0)(os);
    BYTE *obj;
    if (fn<BYTE(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(os, zs_c_str(&path), 0x204, 1)) {
        DWORD saved;
        fn<Fn1>(0x4dedf0)(&saved, glob_addr(0x713c14));
        fn<Fn1>(0x4e18f0)(glob<void *>(0x713c14), pv(&path));
        char line[0x400];
        fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(os, line, 0x400);
        void *registry = fn<void *(__cdecl *)()>(0x415d20)();
        obj = fn<BYTE *(THISCALL *)(void *, const char *, DWORD, int)>(0x415c00)(registry, line, 0, -1);
        if (obj && (fn<Fn1>(0x416790)(obj, fn<DWORD(__cdecl *)()>(0x4a8760)()) & 0xff)) {
            if (out)
                *out = obj;
            fn<Fn1>(0x416af0)(obj, pv(os));
        } else {
            obj = nullptr;
        }
        fn<Fn1>(0x4e18f0)(glob<void *>(0x713c14), pv(&saved));
        fn<Thiscall0>(0x524340)(os);
        zs_dtor(&saved);
    } else {
        obj = vcall<BYTE *>(bp_(f(list, 0x154)), 27, 0, -1);
        vcall<DWORD>(obj, 19);
        if (!obj || !(fn<Fn1>(0x416790)(obj, fn<DWORD(__cdecl *)()>(0x4a8760)()) & 0xff)) {
            fn<Thiscall0>(0x5241e0)(os);
            zs_dtor(&path);
            return nullptr;
        }
        if (out)
            *out = obj;
    }
    if (obj) {
        if (vcall<DWORD>(obj, 2) != fn<DWORD(__cdecl *)()>(0x4a8760)())
            fn<void(THISCALL *)(void *, const char *, DWORD)>(0x4167e0)(obj, zs_c_str(&path), 0);
        alignas(4) BYTE filters[0x14];
        fn<Fn1>(0x4b1cc0)(filters, 0);
        if (!list[0x158]) {
            fn<Fn1>(0x4b1d50)(at<void *>(fn<DWORD(__cdecl *)()>(0x4a8a30)()), pv(list + 0x140));
            list[0x158] = 1;
        }
        fn<Fn1>(0x4b1d50)(list + 0x140, pv(filters));
        fn<BYTE(THISCALL *)(void *, const char *, const char *, void *, DWORD, DWORD)>(0x4a9a30)(obj, dir, name, filters, 1, 0);
        fn<Thiscall0>(0x4b1d20)(filters);
    }
    fn<Thiscall0>(0x5241e0)(os);
    zs_dtor(&path);
    return obj;
}

// The TGA loader's filter list 0x6be3a8: 3, 6, 7, the depth conversion (11 for an 8-bit display, else 12), 8
DWORD __cdecl TgaFilters_Setup()
{
    constexpr DWORD LIST = 0x6be3a8;
    using Add = void(THISCALL *)(void *, DWORD);
    fn<Thiscall0>(0x4b1d80)(glob<void *>(LIST));
    fn<Add>(0x4b1db0)(glob<void *>(LIST), 3);
    fn<Add>(0x4b1db0)(glob<void *>(LIST), 6);
    fn<Add>(0x4b1db0)(glob<void *>(LIST), 7);
    fn<Add>(0x4b1db0)(glob<void *>(LIST), fn<DWORD(__cdecl *)()>(0x55d030)() == 8 ? 0xb : 0xc);
    fn<Add>(0x4b1db0)(glob<void *>(LIST), 8);
    return glob_addr(LIST);
}

// ---- The hue remap row and entry, the Filter mode's state ---------------------------------------------------

// fastcall (dst, src; n, table): n bytes through the 256-byte table (n 0 runs 2^32 times, as the original)
DWORD __fastcall Remap8_Row(BYTE *dst, const BYTE *src, DWORD n, const BYTE *table)
{
    DWORD v;
    do {
        v = table[*src++];
        *dst++ = (BYTE)v;
    } while (--n);
    return v;   // eax: the last byte written
}

BYTE *THISCALL HueRemapEntry_DeletingDtor(BYTE *e, DWORD flags)
{
    fn<Thiscall0>(0x571bb0)(e);
    if (flags & 1)
        pool_free(e, 0x38);
    return e;
}

// The Filter mode's two arrays of 8-byte records {?, block} (0x716160 with its count 0x71616c, 0x716164 with
// 0x716178) freed, then its frame 0x716120
DWORD __cdecl StaticExit_FilterState()
{
    for (int k = 0; k < 2; k++) {
        const DWORD arr = k ? glob_addr(0x716164) : glob_addr(0x716160), cnt = k ? glob_addr(0x716178) : glob_addr(0x71616c);
        for (DWORD i = 0; i < *at<DWORD *>(cnt); i++) {
            BYTE *a = bp_(*at<DWORD *>(arr));
            if (void *p = bp_(f(a, i * 8 + 4))) {
                heap_free(p);
                f(bp_(*at<DWORD *>(arr)), i * 8 + 4) = 0;
            }
        }
        if (void *a = bp_(*at<DWORD *>(arr))) {
            heap_free(a);
            *at<DWORD *>(arr) = 0;
        }
        *at<DWORD *>(cnt) = 0;
    }
    return fn<Thiscall0>(0x4a8b00)(glob<void *>(0x716120));
}

}  // namespace

ZAX_HOOKS(frame_rest4a)
{
    const Hook list[] = {
#include "frame_rest4a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
