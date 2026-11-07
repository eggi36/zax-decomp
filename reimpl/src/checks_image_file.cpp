// Checks of the frames' TGA files (image_file.cpp) against the original with the toolkit's harness (checks_ui_common.h):
// TGA bytes of the case (headers valid and not, true colour and RLE, 24 and 32 bits, top down and bottom up) in the
// extra area; the frame a fake of the CFrame class's recording vtable whose size, pixels and depth are case answers;
// its free, allocate and pitch, the palette's nearest colour, Pixel16_FromRGB and the file buffer are stand-ins (the
// file buffer's read writes a header of the case). RLE rows never overflow their row: that ends the game on both sides.
#include "checks_ui_common.h"

namespace zax {

int hooks_image_file(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"image_file", &hooks_image_file};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4d24c0, CV_FAST, 0, K_VALUE, "ShowErrorBox", "ss"},
    {0x526e40, CV_THIS, 5, K_SMALL, "Palette_Nearest"},
    {0x433a10, CV_FAST, 1, K_VALUE, "Pixel16_FromRGB"},
    {0x4aa790, CV_THIS, 0, K_VALUE, "CFrame_Free"},
    {0x4aa5f0, CV_THIS, 3, K_VALUE, "CFrame_Allocate"},
    {0x4aa2c0, CV_THIS, 2, K_VALUE, "CFrame_SetPitch"},
    {0x4a1780, CV_FAST, 0, K_VALUE, "FileBuf_Ctor", "xx"},
    {0x4a1840, CV_FAST, 2, K_VALUE, "FileBuf_Open", "xxsv"},
    {0x4a1b00, CV_FAST, 2, K_VALUE, "FileBuf_Read", "xxvv"},
    {0x4a2100, CV_FAST, 0, K_VALUE, "FileBuf_Close", "xx"},
    {0x4a1800, CV_FAST, 0, K_VALUE, "FileBuf_Dtor", "xx"},
    {0x4a19d0, CV_FAST, 2, K_VALUE, "FileBuf_Write", "xxxv"},
    {0x4a19f0, CV_FAST, 1, K_VALUE, "FileBuf_WriteChar", "xxb"},
    {0x4a8b00, CV_FAST, 0, K_VALUE, "CFrame::Dtor", "xx"},
    {0x4ab5a0, CV_FAST, 3, K_VALUE, "CFrame_SetSize", "xxvvv"},
    {0x4b3280, CV_FAST, 4, K_VALUE, "Frame_BlitAt", "xxxvvv"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

BYTE g_header[18];
DWORD g_pixels;

// A header: mostly readable (no colour map, type 2/9/10, 24/32 bits), sometimes not
void make_header(Gen &g, BYTE *h, int width, int height, BYTE type, BYTE depth, bool rle_ok = true)
{
    for (int i = 0; i < 18; i++)
        h[i] = (BYTE)g.next();
    h[0] = g.chance(3) ? (BYTE)g.below(8) : 0;
    h[1] = g.chance(10) ? 1 : 0;
    h[2] = g.chance(12) ? (BYTE)g.below(12) : type;
    if (h[2] == 10 && !rle_ok)   // (raw rows read as RLE would overflow a row: that ends the game)
        h[2] = 9;
    h[12] = (BYTE)width, h[13] = (BYTE)(width >> 8);
    h[14] = (BYTE)height, h[15] = (BYTE)(height >> 8);
    h[16] = g.chance(12) ? (BYTE)(g.below(5) * 8) : depth;
    h[17] = g.chance(2) ? 0x20 : (BYTE)(g.next() & ~0x20);
}

// One RLE row of `count` pixels of `bpp` bytes into out; returns its end
BYTE *rle_row(Gen &g, BYTE *out, int count, int bpp)
{
    int left = count;
    while (left > 0) {
        const int n = 1 + (int)g.below((DWORD)(left < 128 ? left : 128));
        if (g.chance(2)) {
            *out++ = (BYTE)(0x80 | (n - 1));
            for (int b = 0; b < bpp; b++)
                *out++ = g.chance(5) ? 0 : (BYTE)g.next();
        } else {
            *out++ = (BYTE)(n - 1);
            for (int i = 0; i < n * bpp; i++)
                *out++ = g.chance(5) ? 0 : (BYTE)g.next();
        }
        left -= n;
    }
    return out;
}

// The file buffer's read writes the case's header
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == -1 && (DWORD)slot == 0x4a1b00 && args[0])
        memcpy(at<void *>(args[0]), g_header, 18);
    if (cls == -1 && (DWORD)slot == 0x4aa5f0)   // the frame has pixels after its allocation
        g_case.fixed[2] = g_pixels;
    if (cls == -1 && (DWORD)slot == 0x4a19d0)   // a write: its bytes
        log_event(0x19d0, args[0] && args[1] < 0x10000 ? fnv(2166136261u, at<void *>(args[0]), args[1]) : args[0], args[1]);
}

void base(Gen &)
{
    g_case.effect = &effect;
    g_case.nwin = 1;
    make_window(0, C_CFRAME);
}

// ---- The row converters (dst, src, count, row[, palette]) --------------------------------------------------------

void s_row(Gen &g)
{
    const DWORD count = g.chance(10) ? 0 : g.below(40) + 1, rows = g.below(4) + 1;
    BYTE *dst = xalloc(count * rows * 4 + 16);
    BYTE *src = xalloc(count * 4 + 16);
    for (DWORD i = 0; i < count * 4; i++)
        src[i] = g.chance(4) ? 0 : (BYTE)g.next();
    g_args[0] = ptrv(dst);
    g_args[1] = ptrv(src);
    g_args[2] = count;
    g_args[3] = g.below(rows);
    g_args[4] = g.next();
}

void s_parse(Gen &g)
{
    BYTE *src = xalloc(20);
    make_header(g, src, g.range(0, 300), g.range(0, 300), g.chance(2) ? 2 : 10, g.chance(2) ? 0x18 : 0x20);
    g_args[0] = ptrv(src);
    g_args[1] = ptrv(xalloc(0x14));
}

void s_rle(Gen &g)
{
    const int count = 1 + (int)g.below(60), depth = g.chance(6) ? (int)g.below(40) : g.chance(2) ? 0x18 : 0x20;
    const int bpp = depth == 0x20 ? 4 : 3;
    BYTE *src = xalloc((DWORD)count * 5 + 16);
    if (depth == 0x18 || depth == 0x20) {
        rle_row(g, src, count, bpp);
    } else {   // another depth copies nothing and does not move on in a raw packet: one packet for the row
        const int n = count < 128 ? count : 128;
        g_args[2] = (DWORD)n;
        src[0] = (BYTE)((g.chance(2) ? 0x80 : 0) | (n - 1));
    }
    g_args[0] = ptrv(xalloc((DWORD)count * 4 + 16));
    g_args[1] = ptrv(src);
    if (depth == 0x18 || depth == 0x20)
        g_args[2] = (DWORD)count;
    g_args[3] = (DWORD)depth;
    g_args[4] = ptrv(TEXTS[g.below(8)]);
    g_args[5] = g.below(100);
}

// ---- The reader (frame, data, name, palette, flag) ---------------------------------------------------------------

void s_read(Gen &g)
{
    const int w = (int)g.below(24) + 1, h = (int)g.below(10) + 1;
    const BYTE type = g.chance(2) ? 2 : 10, depth = g.chance(2) ? 0x18 : 0x20;
    const int bpp = depth == 0x20 ? 4 : 3;
    BYTE *data = xalloc(18 + 8 + (DWORD)(w * h * (bpp + 1) + h * 16) + 16);
    make_header(g, data, w, h, type, depth, type == 10);
    if (data[2] == 10 && (data[16] == 0x18 || data[16] == 0x20))   // (RLE packets of the other depth would overflow)
        data[16] = depth;
    BYTE *p = data + 18 + (signed char)data[0];
    if ((signed char)data[0] < 0)
        data[0] = 0, p = data + 18;
    for (int y = 0; y < h; y++)
        if (data[2] == 10) {
            p = rle_row(g, p, w, data[16] == 0x20 ? 4 : 3);
        } else {
            for (int i = 0; i < w * bpp; i++)
                *p++ = g.chance(4) ? 0 : (BYTE)g.next();
        }
    // the frame: its size the file's (mostly; an RLE file's rows only at the file's width), its depth 8/16/24/32
    // (sometimes another), its pixels in the block (sometimes none until CFrame_Allocate)
    const DWORD depths[4] = {8, 16, 24, 32};
    g_case.fixed[0] = data[2] == 10 || g.chance(2) ? (DWORD)w : g.below((DWORD)w) + 1;
    g_case.fixed[1] = g.chance(2) ? (DWORD)h : g.below((DWORD)h) + 1;
    g_pixels = ptrv(xalloc((DWORD)(w * h * 4) + 64));
    g_case.fixed[2] = g.chance(6) ? 0 : g_pixels;
    g_case.fixed[3] = g.chance(10) ? g.below(40) : depths[g.below(4)];
    set_kind(C_CFRAME, 45, K_FIXED0);
    set_kind(C_CFRAME, 39, K_FIXED1);
    set_kind(C_CFRAME, 31, K_FIXED2);
    set_kind(C_CFRAME, 29, K_FIXED3);
    g_args[0] = ptrv(win(0));
    g_args[1] = ptrv(data);
    g_args[2] = ptrv(TEXTS[g.below(8)]);
    g_args[3] = g.chance(3) ? 0 : g.next();
    g_args[4] = g.below(3);
}

void s_file(Gen &g)
{
    make_header(g, g_header, g.range(0, 300), g.range(0, 300), g.chance(2) ? 2 : 10, g.chance(2) ? 0x18 : 0x20);
    set_kind(C_OBJECT, 0, K_VALUE);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.next();
}

// ---- The writer (frame, file, mode) -----------------------------------------------------------------------------

void s_write(Gen &g)
{
    const DWORD depths[5] = {8, 0x18, 0x20, 16, 8};
    const DWORD depth = g.chance(10) ? g.below(40) : depths[g.below(5)];
    const DWORD bpp = depth == 0x20 ? 4 : depth == 0x18 ? 3 : 1;
    const DWORD w = g.chance(10) ? 0 : 1 + g.below(160), h = g.chance(10) ? 0 : 1 + g.below(6), pitch = w + g.below(4);
    BYTE *pix = xalloc(pitch * (h ? h : 1) * bpp + 16);
    // rows with runs: a few colours, runs of random length
    for (DWORD y = 0; y < (h ? h : 1); y++) {
        BYTE c[4] = {(BYTE)g.next(), (BYTE)g.next(), (BYTE)g.next(), (BYTE)g.next()};
        for (DWORD x = 0; x < pitch; x++) {
            if (g.chance(3))
                for (int b = 0; b < 4; b++)
                    c[b] = g.chance(3) ? (BYTE)g.below(3) : (BYTE)g.next();
            memcpy(pix + (y * pitch + x) * bpp, c, bpp);
        }
    }
    DWORD *rows = reinterpret_cast<DWORD *>(xalloc(4 * (h + 2)));
    for (DWORD y = 0; y < h + 2; y++)
        rows[y] = y * pitch;
    set_kind(C_CFRAME, 45, K_FIXED0);
    set_kind(C_CFRAME, 39, K_FIXED1);
    set_kind(C_CFRAME, 31, K_FIXED2);
    set_kind(C_CFRAME, 29, K_FIXED3);
    set_kind(C_CFRAME, 46, K_FIXED4);
    set_kind(C_CFRAME, 58, K_FIXED5);
    set_kind(C_CFRAME, 59, K_BOOL);
    set_kind(C_CFRAME, 38, g.chance(4) ? K_VALUE : K_ZERO);
    g_case.fixed[0] = w;
    g_case.fixed[1] = h;
    g_case.fixed[2] = g.chance(12) ? 0 : ptrv(pix);
    g_case.fixed[3] = depth;
    g_case.fixed[4] = ptrv(rows);
    BYTE *holder = xalloc(8);
    BYTE *data = xalloc(0x300);
    for (DWORD i = 0; i < 0x300; i++)
        data[i] = (BYTE)g.next();
    at_(holder, 0) = ptrv(data);
    g_case.fixed[5] = ptrv(holder);
    set_kind(C_OBJECT, 0, K_VALUE);
    g_args[0] = ptrv(win(0));
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = g.below(0x100);
}

// The writer's plain frame (a local of the writer): its constructor gives it the fake frame class
BYTE *THISCALL fake_frame_ctor(BYTE *f)
{
    at_(f, 0) = at_(win(0), 0);
    log_event(0x8a90, 0, 0);
    return f;
}

constexpr DWORD GLOBALS4[] = {0x7139ec};

const Test TESTS[] = {
    {"Tga Row24To8", 0x58cab0, CALL_FAST, 5, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga Row32To8", 0x58cb20, CALL_FAST, 5, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga Row32To16", 0x58cb90, CALL_FAST, 4, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga Row24To24", 0x58cbe0, CALL_FAST, 4, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga Row32To24", 0x58cc50, CALL_FAST, 4, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga Row24To32", 0x58cc90, CALL_FAST, 4, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga Row32To32", 0x58ccf0, CALL_FAST, 4, 0, 200, s_row, STUBS, NSTUBS},
    {"Tga ParseHeader", 0x58cd30, CALL_FAST, 2, ~0u, 600, s_parse, STUBS, NSTUBS},
    {"Tga DecodeRleRow", 0x58cde0, CALL_FAST, 6, ~0u, 400, s_rle, STUBS, NSTUBS},
    {"Tga Read", 0x58c6f0, CALL_FAST, 5, ~0u, 600, s_read, STUBS, NSTUBS},
    {"Tga FileIsRle", 0x58d710, CALL_FAST, 2, 0xff, 200, s_file, STUBS, NSTUBS},
    {"Tga FileDepth", 0x58d7b0, CALL_FAST, 2, ~0u, 200, s_file, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(image_file)
{
    (void)cls;
    (void)n;
    if (!g_globals.n)
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
    Tally t{"image file: the TGA reader and writer, the row converters, header parser and RLE rows, the header queries"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 46000);
    const Test write[] = {{"Tga Write", 0x58cef0, CALL_FAST, 3, ~0u, 500, s_write, STUBS, NSTUBS}};
    CodePatch ctor;
    patch_jump(ctor, 0x4a8a90, (void *)&fake_frame_ctor);
    run_tests(t, g_hooks, &g_globals, write, 1, &base, 46500);
    unpatch(ctor);
    t.report();
}

}  // namespace zax
