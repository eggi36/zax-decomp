// Checks of image_lo4.cpp (the .bmp decoder 0x466b30, CFrame::LoadImage 0x4a9b60, the TGA loader 0x4a9d70,
// InitBlendTables 0x433000) against the original with the toolkit's harness (checks_ui_common.h): the frame is a fake
// CFrame whose slots answer the case's size, flags, pixels and palette holder; the .bmp files are built by the case
// (every bit count, matching and other frame sizes, palettes the same or not); the file buffer, the palette functions,
// the TGA reader and writer, the filters, the object functions and the error box are recording stand-ins (the file
// buffer's Read fills the case's .bmp in; the palette objects' constructor is the exe's); the decoder's globals 0x6bceb8-0x6bcec8 and the TGA loader's buffer
// 0x6be7c0-0x6be7c8 are saved globals; the tables InitBlendTables writes (0x7018 bytes) are put back before each side,
// hashed into the block after it and restored at the end.
#include "checks_ui_common.h"

namespace zax {

int hooks_image_lo4(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"image_lo4", &hooks_image_lo4};
Globals g_globals;

#define FILE_STUBS \
    UIC_ALLOC_STUBS, {0x4a1780, CV_FAST, 0, 'e', "FileBuf_Ctor", "xx"}, {0x4a1840, CV_FAST, 2, K_FIXED6, "FileBuf_Open", "xxsv"}, \
        {0x4a2190, CV_FAST, 0, K_FIXED7, "FileBuf_Size", "xx"}, {0x4a1b00, CV_FAST, 2, K_VALUE, "FileBuf_Read", "xxvv"}, \
        {0x4a2100, CV_FAST, 0, K_VALUE, "FileBuf_Close", "xx"}, {0x4a1800, CV_FAST, 0, K_VALUE, "FileBuf_Dtor", "xx"}, \
        {0x418fc0, CV_FAST, 1, K_BOOL, "FieldDesc_IsDynamicIn", "vxx"}, {0x418fb0, CV_FAST, 1, K_VALUE, "FieldDesc_RemoveDynamicFrom", "vxx"}, \
        {0x526700, CV_THIS, 1, K_VALUE, "Palette_Assign"}, {0x526e40, CV_THIS, 5, K_SMALL, "Palette_Nearest"}

const Stub STUBS_BMP[] = {
    UIC_ALLOC_STUBS,
    {0x4aa790, CV_THIS, 0, K_VALUE, "CFrame_Free"},
    {0x4aa5f0, CV_THIS, 3, K_VALUE, "CFrame_Allocate"},
    {0x5268f0, CV_THIS, 1, K_VALUE, "Palette_SetKind"},
    {0x526e40, CV_THIS, 5, K_SMALL, "Palette_Nearest"},
    {0x433a10, CV_FAST, 1, K_VALUE, "Pixel16_FromRGB", "vvv"},
};
const Stub STUBS_IMAGE[] = {
    FILE_STUBS,
    {0x466a40, CV_FAST, 0, K_BOOL, "Bmp_ReadPalette", "vv"},
    {0x4aa2c0, CV_THIS, 2, K_VALUE, "CFrame_SetPitch"},
    {0x4aa790, CV_THIS, 0, K_VALUE, "CFrame_Free"},
    {0x4aa5f0, CV_THIS, 3, K_VALUE, "CFrame_Allocate"},
    {0x5268f0, CV_THIS, 1, K_VALUE, "Palette_SetKind"},
    {0x433a10, CV_FAST, 1, K_VALUE, "Pixel16_FromRGB", "vvv"},
};
const Stub STUBS_TGA[] = {
    FILE_STUBS,
    {0x4b1d80, CV_THIS, 0, K_VALUE, "FrameFilterList_Clear"},
    {0x4b1db0, CV_THIS, 1, K_VALUE, "FrameFilterList_Add"},
    {0x4b21a0, CV_THIS, 1, K_VALUE, "FrameFilterList_Run"},
    {0x55d030, CV_CDECL, 0, K_FIXED3, "ColorDepth"},
    {0x58d7b0, CV_FAST, 0, K_FIXED4, "Tga_FileDepth", "sx"},
    {0x58d710, CV_FAST, 0, K_BOOL, "Tga_FileIsRle", "sx"},
    {0x4a1650, CV_FAST, 0, K_ONE, "File_Exists", "sv"},
    {0x4a8a90, CV_FAST, 0, 'e', "CFrame::Ctor", "xx"},
    {0x58c6f0, CV_FAST, 3, K_VALUE, "ImageFile_Read", "xvsvv"},
    {0x4aa070, CV_FAST, 2, K_VALUE, "CFrame::WriteTga", "xxsv"},
    {0x4aa790, CV_FAST, 0, K_VALUE, "CFrame_Free", "xx"},
    {0x4173f0, CV_FAST, 0, K_VALUE, "Object_FreeFields", "xx"},
    {0x4166d0, CV_FAST, 0, K_VALUE, "CObject_Dtor", "xx"},
};
const Stub STUBS_TABLES[] = {
    UIC_ALLOC_STUBS,
    {0x4a8a90, CV_FAST, 0, 'e', "CFrame::Ctor", "xx"},
    {0x4a8b00, CV_FAST, 0, K_VALUE, "CFrame::Dtor", "xx"},
    {0x4a42c0, CV_FAST, 2, K_VALUE, "Path_JoinExt", "xsss"},
    {0x4a4220, CV_FAST, 1, K_VALUE, "Path_Join", "xss"},
    {0x4aa180, CV_FAST, 1, K_ONE, "CFrame::LoadBmpPalette", "xxx"},
    {0x5266e0, CV_FAST, 0, K_VALUE, "NameEntry_FreeName", "xx"},
    {0x5269e0, CV_FAST, 2, K_BOOL, "Palette_Load", "xxxv"},
    {0x526b80, CV_FAST, 1, K_VALUE, "Palette_Save", "vxx"},
    {0x4aac20, CV_FAST, 0, K_FIXED0, "CFrame::Palette", "xx"},
    {0x526870, CV_FAST, 1, K_BOOL, "Palette_Differs", "xxv"},
    {0x4a14e0, CV_FAST, 0, K_VALUE, "File_Delete", "xv"},
    {0x4337e0, CV_FAST, 1, K_VALUE, "BuildInvColorTable", "vvv"},
    {0x527da0, CV_THIS, 4, K_VALUE, "Palette_BuildLookup"},
    {0x527700, CV_THIS, 2, K_VALUE, "Palette_BuildGammaTable"},
    {0x5277b0, CV_THIS, 1, K_VALUE, "Palette_BuildAmplifyTable"},
    {0x527980, CV_THIS, 1, K_VALUE, "Palette_BuildHueBlendTable"},
    {0x4a1300, CV_FAST, 1, K_BOOL, "File_ReadExact", "xvv"},
    {0x4a13f0, CV_FAST, 1, K_VALUE, "File_WriteAll", "xvv"},
    {0x527040, CV_THIS, 9, K_VALUE, "Palette_BuildLightnessRemap"},
    {0x526e40, CV_THIS, 5, K_SMALL, "Palette_Nearest"},
};
#define N(a) (int)(sizeof a / sizeof a[0])

// The tables InitBlendTables writes
struct Region {
    DWORD address, size;
};
constexpr Region TABLES[] = {{0x630378, 0x400}, {0x6753b8, 0x400}, {0x633b94, 0x400}, {0x633394, 0x400}, {0x675bb8, 0x400},
                             {0x62f318, 0x400}, {0x62ff30, 0x400}, {0x630778, 0x400}, {0x633fb0, 0x400}, {0x632b80, 0x400},
                             {0x633794, 0x400}, {0x62ef18, 0x400}, {0x674bb0, 0x400}, {0x6763c0, 0x400}, {0x632f88, 0x400},
                             {0x6747b0, 0x400}, {0x62f728, 0x400}, {0x674fb0, 0x400}, {0x675fb8, 0x400}, {0x62eb18, 0x400},
                             {0x630b78, 0x2000}, {0x62f720, 4},     {0x62fb28, 4},     {0x630358, 4},     {0x632b78, 4},
                             {0x633fa8, 4},     {0x62f718, 4}};
BYTE g_tables_saved[0x7100];
DWORD *g_tables_hash;
void tables(bool save)
{
    DWORD o = 0;
    for (const Region &r : TABLES) {
        if (save)
            memcpy(g_tables_saved + o, at<void *>(r.address), r.size);
        else
            memcpy(at<void *>(r.address), g_tables_saved + o, r.size);
        o += r.size;
    }
}
void hash_tables()
{
    unsigned h = 2166136261u;
    for (const Region &r : TABLES)
        h = fnv(h, at<const void *>(r.address), r.size);
    *g_tables_hash = h;
}

// The case's .bmp: header, colour table, rows of the stride
BYTE *g_bmp;
DWORD g_bmp_size;
void make_bmp(Gen &g, int width, int height, WORD bits, DWORD frame_width, DWORD frame_height)
{
    const DWORD row = bits == 1 ? frame_width >> 3 : bits == 4 ? frame_width >> 1 : bits == 8 ? frame_width : frame_width * 3;
    const DWORD stride = (row & 3) ? (row | 3) + 1 : row;
    const DWORD offset = 0x36 + 0x400;
    g_bmp_size = offset + stride * frame_height + 0x20;
    g_bmp = xalloc(g_bmp_size);
    BYTE *b = g_bmp;
    b[0] = 'B', b[1] = 'M';
    at_(b, 2) = g_bmp_size;
    at_(b, 0xa) = offset;
    at_(b, 0xe) = 0x28;
    at_<int>(b, 0x12) = width;
    at_<int>(b, 0x16) = height;
    at_<WORD>(b, 0x1a) = 1;
    at_<WORD>(b, 0x1c) = bits;
    for (DWORD i = 0x36; i < g_bmp_size; i++)
        b[i] = g.chance(5) ? 0 : (BYTE)g.next();
}

// A palette object {data, kind, source} with 0x300 bytes of data (a copy of `like` now and then)
BYTE *make_palette(Gen &g, const BYTE *like)
{
    BYTE *p = xalloc(0x10);
    BYTE *d = xalloc(0x300);
    for (int i = 0; i < 0x300; i++)
        d[i] = like && !g.chance(30) ? like[i] : (BYTE)g.next();
    at_(p, 0) = ptrv(d);
    at_(p, 4) = 1;
    return p;
}

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    (void)answer;
    if (cls != -1)
        return;
    if ((DWORD)slot == 0x4a1b00 && g_bmp && in_block(args[0]))   // the file's bytes
        memcpy(at<void *>(args[0]), g_bmp, args[1] < g_bmp_size ? args[1] : g_bmp_size);
    if ((DWORD)slot == 0x527040) {   // a lightness remap: the local palette object gets data
        BYTE *d = xalloc(0x100);
        if (d) {
            for (int i = 0; i < 0x100; i++)
                d[i] = (BYTE)g_case.values.next();
            *at<DWORD *>(g_effect_ecx) = ptrv(d);
        }
    }
}

void base(Gen &)
{
    BYTE *f = make_window(0, C_CFRAME);
    g_case.nwin = 1;
    g_case.effect = &effect;
    g_bmp = nullptr;
    set_kind(C_CFRAME, 31, K_FIXED0);
    set_kind(C_CFRAME, 45, K_FIXED1);
    set_kind(C_CFRAME, 39, K_FIXED2);
    set_kind(C_CFRAME, 38, K_FIXED3);
    set_kind(C_CFRAME, 59, K_FIXED4);
    set_kind(C_CFRAME, 58, K_FIXED5);
    set_kind(C_CFRAME, 29, K_FIXED4);
    g_self = f;
}

// The frame and the file: the frame's size the file's (now and then another one, then reallocated), 8 or 16 bits,
// with a palette holder; the given palette the holder's or another
void bmp_case(Gen &g)
{
    const WORD BITS[6] = {1, 4, 8, 24, 16, 0};
    const WORD bits = BITS[g.below(6)];
    const DWORD fw = 1 + g.below(40), fh = g.below(24);
    int w = (int)fw, h = (int)fh;
    if (g.chance(5))
        w = (int)g.below(50), h = (int)g.below(30);
    make_bmp(g, w, h, bits, fw, fh);
    const bool wide = g.chance(3);
    BYTE *pixels = xalloc(fw * fh * (wide ? 2 : 1) + 8);
    g_case.fixed[0] = ptrv(pixels);
    g_case.fixed[1] = fw;
    g_case.fixed[2] = fh;
    g_case.fixed[3] = (g.next() & ~0x40u) | (wide ? 0x40 : 0);
    g_case.fixed[4] = g.chance(4) ? 0 : 1;
    BYTE *holder = make_palette(g, nullptr);
    g_case.fixed[5] = ptrv(holder);
    BYTE *given = g.chance(4) ? nullptr : make_palette(g, g.chance(2) ? at_<BYTE *>(holder, 0) : nullptr);
    BYTE *table = xalloc(0x100);
    case_global(0x6bceb8, ptrv(table));
    case_global(0x6bcec8, g.chance(3) ? g.below(20) : 0);
    g_args[2] = ptrv(given);
}
void s_bmp(Gen &g)
{
    bmp_case(g);
    g_args[0] = ptrv(g_self);
    g_args[1] = ptrv(g_bmp);
}
void s_image(Gen &g)
{
    bmp_case(g);
    const DWORD given = g_args[2];
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.below(3);
    g_args[2] = given;
    g_case.fixed[6] = g.chance(6) ? 0 : 1;   // the file opens
    g_case.fixed[7] = g_bmp_size;
    g_case.fixed[4] = g.chance(2) ? 8 : g.below(2);   // slot 29 the depth (and 59)
}
void s_tga(Gen &g)
{
    BYTE *pixels = xalloc(0x40);
    g_case.fixed[0] = ptrv(pixels);
    at_(g_self, 0xc) = g.chance(4) ? 0 : ptrv(pixels);
    at_(g_self, 0x3c) = g.next();
    g_case.fixed[3] = g.chance(2) ? 8 : 16;
    g_case.fixed[4] = g.chance(3) ? 0 : g.chance(2) ? 24 : 32;
    g_case.fixed[5] = ptrv(make_palette(g, nullptr));
    g_case.fixed[6] = g.chance(6) ? 0 : 1;
    g_case.fixed[7] = g.chance(3) ? g.below(0x10000) : 0x10000 + g.below(0x40000);
    case_global(0x6be7c0, g.chance(3) ? 0 : ptrv(xalloc(0x10)));
    case_global(0x6be7c4, g.chance(3) ? 0 : g.chance(2) ? 0x10000 : 0x10000 + g.below(0x30000));
    case_global(0x6be7c8, 0);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.chance(2) ? 0 : ptrv(xalloc(0x20));
    g_args[2] = g.chance(3) ? 0 : ptrv(make_palette(g, nullptr));
    g_args[3] = g.below(4) << 4;
}
void s_tables(Gen &g)
{
    tables(false);
    g_tables_hash = reinterpret_cast<DWORD *>(xalloc(4));
    g_case.fixed[0] = g.next();
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
}

constexpr DWORD GLOBALS4[] = {0x6bceb8, 0x6bcebc, 0x6bcec0, 0x6bcec4, 0x6bcec8, 0x6be7c0, 0x6be7c4};

const Test TESTS[] = {
    {"Bmp_Decode", 0x466b30, CALL_FAST, 3, 0xff, 1500, s_bmp, STUBS_BMP, N(STUBS_BMP)},
    {"CFrame LoadImage", 0x4a9b60, CALL_THIS, 3, 0xff, 600, s_image, STUBS_IMAGE, N(STUBS_IMAGE)},
    {"CFrame LoadTga", 0x4a9d70, CALL_THIS, 4, 0xff, 600, s_tga, STUBS_TGA, N(STUBS_TGA)},
    {"InitBlendTables", 0x433000, CALL_FAST, 2, ~0u, 30, s_tables, STUBS_TABLES, N(STUBS_TABLES), nullptr, &hash_tables},
};

}  // namespace

ZAX_CHECKS(image_lo4)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        g_globals.add(0x6be7c8, 1);
    }
    tables(true);
    Tally t{"image lo4: the .bmp decoder, LoadImage, the TGA loader, InitBlendTables"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 55000);
    tables(false);
    t.report();
}

}  // namespace zax
