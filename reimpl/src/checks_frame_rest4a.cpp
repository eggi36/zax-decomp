// Checks of the frame helpers of band A (frame_rest4a.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): the error diffusion on rings of random floats (values inside and outside 0..1, NaN,
// snapped or not, every column), its palette lookups stand-ins answering random colours; the .bmp header and
// palette on random file bytes and a recorded frame whose palette object the case makes; the file name,
// load and save wrappers with the file buffer, the frame reader and writer, the cache folder and the depths
// stand-ins (the FileBuf, a local of the caller, logged without its address); the TGA filter list with its
// list functions stand-ins; a frame field written as text with the file buffer's line functions stand-ins;
// the hue remap row; the remap entry's destructor; the Filter mode's state with the case's arrays in its
// globals and the frame's destructor a stand-in.
#include "checks_ui_common.h"

namespace zax {

int hooks_frame_rest4a(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"frame_rest4a", &hooks_frame_rest4a};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a8b00, CV_THIS, 0, K_VALUE, "CFrame::Dtor"},
    {0x571bb0, CV_THIS, 0, K_VALUE, "HueRemapEntry_Dtor"},
    {0x4aa770, CV_THIS, 0, K_VALUE, "CFrame_NewPalette"},
    {0x526e40, CV_THIS, 5, K_SMALL, "Palette_Nearest", "vvvvv"},
    {0x527000, CV_THIS, 4, K_VALUE, "Palette_GetRGB", "vxxx"},
    {0x4b1d80, CV_THIS, 0, K_VALUE, "FrameFilterList_Clear"},
    {0x4b1db0, CV_THIS, 1, K_VALUE, "FrameFilterList_Add", "v"},
    {0x55d030, CV_CDECL, 0, K_FIXED0, "ColorDepth"},
    {0x4b2190, CV_THIS, 0, K_FIXED0, "FrameFilterList_Depth"},
    {0x4a1140, CV_CDECL, 1, K_TEXT, "Files_CacheFolder", "s"},
    {0x418fc0, CV_THIS, 1, K_BOOL, "FieldDesc_IsDynamicIn", "v"},
    {0x418fb0, CV_THIS, 1, K_VALUE, "FieldDesc_RemoveDynamicFrom", "v"},
    {0x4aa790, CV_THIS, 0, K_VALUE, "CFrame_Free"},
    {0x4a1780, CV_FAST, 0, K_VALUE, "FileBuf_Ctor", "xx"},
    {0x4a1840, CV_FAST, 2, K_BOOL, "FileBuf_Open", "xxsv"},
    {0x4a2190, CV_FAST, 0, K_FIXED1, "FileBuf_Size", "xx"},
    {0x4a1b00, CV_FAST, 2, K_VALUE, "FileBuf_Read", "xxvv"},
    {0x4a2100, CV_FAST, 0, K_VALUE, "FileBuf_Close", "xx"},
    {0x4a1800, CV_FAST, 0, K_VALUE, "FileBuf_Dtor", "xx"},
    {0x4a9340, CV_THIS, 1, K_BOOL, "CFrame::ReadFrm", "x"},
    {0x4a9740, CV_THIS, 1, K_BOOL, "CFrame::WriteFrm", "x"},
    {0x4a1a40, CV_THIS, 1, K_VALUE, "FileBuf_WriteLine", "s"},
    {0x4a1ab0, CV_THIS, 0, K_VALUE, "FileBuf_Indent"},
    {0x4a1ae0, CV_THIS, 0, K_VALUE, "FileBuf_Unindent"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

float rand_float(Gen &g)
{
    const DWORD k = g.below(10);
    if (k == 0)
        return 0.0f;
    if (k == 1)
        return 1.0f;
    if (k == 2) {
        DWORD nan = 0x7fc00000;
        float f;
        memcpy(&f, &nan, 4);
        return f;
    }
    if (k < 5)
        return (float)g.range(-300, 1300) / 1000.0f;
    return (float)g.range(0, 1000) / 997.0f;
}
BYTE *g_palette;
int g_reads;
// The .frm reader's file: the header on the first read (a '1' most of the time, random origin, size up to 15 x
// 12, flags, block sizes 0..80), then random bytes
void frm_read(BYTE *d, DWORD n, Gen &g)
{
    for (DWORD i = 0; i < n; i++)
        d[i] = (BYTE)g.next();
    if (g_reads++ == 0 && n == 0x24) {
        d[0] = g.chance(8) ? '2' : '1';
        at_<WORD>(d, 6) = (WORD)g.below(16);
        at_<WORD>(d, 8) = (WORD)g.below(13);
        for (int k = 0; k < 5; k++)
            at_(d, 0x10 + 4 * k) = g.chance(2) ? 0 : 1 + g.below(80);
    }
}
void effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls != -1)
        return;
    Gen g{g_case.seed * 7u + (DWORD)g_case.answers};
    if ((DWORD)slot == 0x527000)
        for (int i = 1; i <= 3; i++)
            *at<DWORD *>(args[i]) = g.below(256);
    if ((DWORD)slot == 0x4aa770 && g_palette)
        at_(g_palette, 0) = ptrv(xalloc(0x300));
    if ((DWORD)slot == 0x4a1b00 && g_case.fixed[5] == 0xf4) {
        frm_read(at<BYTE *>(args[0]), args[1], g);
        return;
    }
    if ((DWORD)slot == 0x4a19d0) {   // a write: the bytes (a header without its two unwritten bytes 0xa-0xb)
        BYTE *p = at<BYTE *>(args[0]);
        const DWORD n = args[1];
        const unsigned h = n == 0x24 ? fnv(fnv(2166136261u, p, 0xa), p + 0xc, 0x18) : fnv(2166136261u, p, n);
        log_event(0x7c01, h, n);
        return;
    }
    if ((DWORD)slot == 0x4a1b00) {   // the file's bytes: a .bmp header (8 bits most of the time) and a palette
        BYTE *d = at<BYTE *>(args[0]);
        const DWORD n = args[1];
        for (DWORD i = 0; i < n; i++)
            d[i] = (BYTE)g.next();
        if (n >= 0x36)
            at_<WORD>(d, 0x1c) = g.chance(4) ? 24 : 8;
    }
}
void base(Gen &)
{
    g_palette = nullptr;
    g_reads = 0;
    g_case.effect = &effect;
}

// ---- Dither ------------------------------------------------------------------------------------------------

BYTE *dither(Gen &g)
{
    BYTE *d = xalloc(0x48);
    const DWORD w = 1 + g.below(12);
    at_(d, 0) = w;
    at_(d, 4) = w + 1;
    at_(d, 8) = g.below(w + 1);
    at_(d, 0xc) = g.below(w);
    for (int k = 0; k < 3; k++) {
        BYTE *r = xalloc(4 * (w + 1));
        for (DWORD i = 0; i <= w; i++)
            at_<float>(r, 4 * i) = rand_float(g) * 0.2f;
        at_(d, 0x10 + 4 * k) = ptrv(r);
    }
    for (DWORD o = 0x1c; o <= 0x30; o += 4)
        at_<float>(d, o) = rand_float(g);
    const float thr = (float)g.range(0, 100) / 400.0f;
    at_<float>(d, 0x34) = thr;
    at_<float>(d, 0x38) = thr * thr;
    d[0x3c] = g.chance(3) ? 1 : 0;
    at_(d, 0x40) = g.next();
    at_(d, 0x44) = g.next();
    return d;
}
float *rgb(Gen &g)
{
    float *v = reinterpret_cast<float *>(xalloc(12));
    for (int i = 0; i < 3; i++)
        v[i] = rand_float(g);
    return v;
}
void s_dither_ctor(Gen &g)
{
    g_self = xalloc(0x48);
    g_args[0] = g.below(30);
    const float t = rand_float(g);
    memcpy(&g_args[1], &t, 4);
    g_args[2] = g.next();
    g_args[3] = g.next();
}
void s_dither_free(Gen &g)
{
    g_self = dither(g);
    for (int k = 0; k < 3; k++)
        if (g.chance(4))
            at_(g_self, 0x10 + 4 * k) = 0;
}
void s_dither_rgb(Gen &g)
{
    g_self = dither(g);
    float *v = rgb(g);
    g_args[0] = ptrv(v);
    g_args[1] = ptrv(v + 1);
    g_args[2] = ptrv(v + 2);
}
void s_dither_quantize(Gen &g)
{
    g_self = dither(g);
    for (int i = 0; i < 3; i++) {
        const float v = rand_float(g);
        memcpy(&g_args[i], &v, 4);
    }
}

// ---- .bmp, files -------------------------------------------------------------------------------------------

void s_header(Gen &g)
{
    BYTE *s = xalloc(0x40), *d = xalloc(0x38);
    for (int i = 0; i < 0x40; i++)
        s[i] = (BYTE)g.next();
    for (int i = 0; i < 0x38; i++)
        d[i] = (BYTE)g.next();
    g_args[0] = ptrv(s);
    g_args[1] = ptrv(d);
}
BYTE *palette_frame(Gen &g)
{
    BYTE *frame = make_window(1, C_CFRAME);
    g_palette = xalloc(8);
    at_(g_palette, 0) = g.chance(3) ? 0 : ptrv(xalloc(0x300));
    g_case.fixed[2] = ptrv(g_palette);
    set_kind(C_CFRAME, 58, K_FIXED2);
    return frame;
}
void s_bmp_palette(Gen &g)
{
    BYTE *file = xalloc(0x436);
    for (int i = 0; i < 0x436; i++)
        file[i] = (BYTE)g.next();
    at_<WORD>(file, 0x1c) = g.chance(4) ? (WORD)g.below(33) : 8;
    g_args[0] = ptrv(palette_frame(g));
    g_args[1] = ptrv(file);
}
void s_filters(Gen &g) { g_case.fixed[0] = g.chance(2) ? 8 : 16; }
void s_file_name(Gen &g)
{
    BYTE *out = xalloc(4);
    make_text(out, TEXTS[g.below(8)]);
    static const DWORD DEPTHS[] = {8, 16, 24, 32, 15, 0, 9};
    g_case.fixed[0] = DEPTHS[g.below(7)];
    g_self = xalloc(0x40);
    g_args[0] = ptrv(out);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = ptrv(TEXTS[g.below(8)]);
    g_args[3] = g.chance(2) ? 0 : g.next();
    g_args[4] = g.chance(2) ? 1 : g.next() & ~0xffu;
}
void s_load(Gen &g)
{
    s_file_name(g);
    BYTE *frame = g_self;
    for (int o = 0; o < 0x40; o += 4)
        at_(frame, o) = g.next();
    g_args[0] = g_args[1];
    g_args[1] = g_args[2];
    g_args[2] = g_args[3];
    g_args[3] = g_args[4];
    g_args[4] = g.next() & 0xffff;
}
void s_save(Gen &g)
{
    g_self = xalloc(0x40);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.next() & 0xffff;
}
void s_load_bmp(Gen &g)
{
    BYTE *frame = palette_frame(g);
    for (int o = 0x3c; o < 0x40; o += 4)
        at_(frame, o) = g.next();
    g_case.fixed[1] = g.chance(4) ? g.below(0x40) : 0x436;
    g_self = frame;
    g_args[0] = ptrv(TEXTS[g.below(8)]);
}
// ---- Others ------------------------------------------------------------------------------------------------

void s_remap(Gen &g)
{
    const DWORD n = 1 + g.below(300);
    BYTE *dst = xalloc(n + 4), *src = xalloc(n), *table = xalloc(256);
    for (DWORD i = 0; i < n; i++)
        src[i] = (BYTE)g.next();
    for (int i = 0; i < 256; i++)
        table[i] = (BYTE)g.next();
    g_args[0] = ptrv(dst);
    g_args[1] = ptrv(src);
    g_args[2] = n;
    g_args[3] = ptrv(table);
}
void s_entry(Gen &g)
{
    g_self = xalloc(0x38);
    g_args[0] = g.chance(2) ? 1 : g.next() & ~1u;
}
void s_filter_state(Gen &g)
{
    for (int k = 0; k < 2; k++) {
        const DWORD n = g.below(5);
        BYTE *a = g.chance(5) ? nullptr : xalloc(8 * (n ? n : 1));
        for (DWORD i = 0; a && i < n; i++)
            at_(a, i * 8 + 4) = g.chance(3) ? 0 : ptrv(xalloc(8));
        case_global(k ? 0x716164 : 0x716160, ptrv(a));
        case_global(k ? 0x716178 : 0x71616c, a ? n : 0);
    }
}

const Stub FRM_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4a1b00, CV_THIS, 2, K_VALUE, "FileBuf_Read", "xv"},   // the header is a local of the reader
    {0x4a2150, CV_THIS, 1, K_VALUE, "FileBuf_Advance", "v"},
    {0x4a19d0, CV_THIS, 2, K_VALUE, "FileBuf_Write", "xv"},
};
constexpr int NFRM = sizeof FRM_STUBS / sizeof FRM_STUBS[0];

void s_read_frm(Gen &g)
{
    g_case.fixed[5] = 0xf4;
    BYTE *frame = xalloc(0x40), *fb = xalloc(0x1c);
    for (int o = 0; o < 0x40; o += 4)
        at_(frame, o) = g.next();
    fb[0x18] = g.chance(2) ? 1 : 0;
    g_self = frame;
    g_args[0] = ptrv(fb);
}
void s_write_frm(Gen &g)
{
    BYTE *frame = make_window(1, C_CFRAME);
    set_kind(C_CFRAME, 29, K_SMALL);
    set_kind(C_CFRAME, 30, K_SMALL);
    set_kind(C_CFRAME, 40, K_SMALL);
    at_<WORD>(frame, 8) = (WORD)g.next();
    at_<WORD>(frame, 0xa) = (WORD)g.next();
    at_<WORD>(frame, 0x12) = (WORD)g.below(16);
    const WORD h = (WORD)g.below(13);
    at_<WORD>(frame, 0x14) = h;
    at_(frame, 0x3c) = g.chance(2) ? g.next() & ~6u : g.next();
    static const DWORD BLK[5][2] = {{0xc, 0x18}, {0x1c, 0x20}, {0x24, 0x28}, {0x2c, 0x30}, {0x34, 0x38}};
    for (int k = 0; k < 5; k++) {
        if (k && g.chance(3)) {
            at_(frame, BLK[k][0]) = 0;
            continue;
        }
        const DWORD n = g.chance(4) ? 0 : 4 + g.below(60);
        BYTE *b = xalloc(n + 4);
        at_(b, 0) = n;
        for (DWORD i = 4; i < n; i++)
            b[i] = (BYTE)g.next();
        BYTE *t = xalloc(4u * h + 4);
        for (DWORD i = 0; i < 4u * h; i++)
            t[i] = (BYTE)g.next();
        at_(frame, BLK[k][0]) = ptrv(b);
        at_(frame, BLK[k][1]) = ptrv(t);
    }
    g_self = frame;
    g_args[0] = ptrv(xalloc(0x1c));
}

const Stub TGA_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x58cef0, CV_FAST, 1, K_VALUE, "ImageFile_Write", "asv"},
    {0x4aa5f0, CV_THIS, 3, K_VALUE, "CFrame_Allocate", "vvv"},
    {0x4aa2c0, CV_THIS, 2, K_VALUE, "CFrame_SetPitch", "vv"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt", "vvvv"},
    {0x418fc0, CV_THIS, 1, K_BOOL, "FieldDesc_IsDynamicIn", "a"},
    {0x418fb0, CV_THIS, 1, K_VALUE, "FieldDesc_RemoveDynamicFrom", "a"},
    {0x4aa790, CV_THIS, 0, K_VALUE, "CFrame_Free"},
    {0x4173f0, CV_THIS, 0, K_VALUE, "Object_FreeFields"},
    {0x4166d0, CV_THIS, 0, K_VALUE, "CObject_Dtor"},
};
constexpr int NTGA = sizeof TGA_STUBS / sizeof TGA_STUBS[0];
void s_write_tga(Gen &g)
{
    BYTE *frame = make_window(1, C_CFRAME);
    set_kind(C_CFRAME, 29, K_FIXED4);
    static const DWORD BITS[] = {16, 16, 8, 24, 32};
    g_case.fixed[4] = BITS[g.below(5)];
    at_(frame, 8) = g.next();
    at_<WORD>(frame, 0x12) = (WORD)g.below(40);
    at_<WORD>(frame, 0x14) = (WORD)g.below(40);
    g_self = frame;
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = g.next();
}

// TextureList::LoadEntry: the object stream and the filter list are locals (their methods logged without ecx),
// the class registry, the frame loader 0x4a9a30 and the TGA filter list 0x4a8a30 stand-ins; g_LastFileOpened's
// assignments stand-ins (the copy of it is real and given back by the ZString destructor)
const Stub LOAD_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x5240b0, CV_FAST, 0, K_VALUE, "ObjectStream_Ctor", "xx"},
    {0x5242b0, CV_FAST, 3, K_BOOL, "ObjectStream_OpenFile", "xxsvv"},
    {0x524340, CV_FAST, 0, K_VALUE, "ObjectStream_Finish", "xx"},
    {0x5241e0, CV_FAST, 0, K_VALUE, "ObjectStream_Dtor", "xx"},
    {0x4e18f0, CV_THIS, 1, K_VALUE, "ZString_Assign", "a"},
    {0x4a20a0, CV_FAST, 2, K_VALUE, "FileBuf_ReadLine", "xxxv"},
    {0x415d20, CV_CDECL, 0, K_VALUE, "ClassRegistry_Get"},
    {0x415c00, CV_THIS, 3, K_FIXED0, "ClassRegistry_Create", "svv"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf", "v"},
    {0x416af0, CV_THIS, 1, K_VALUE, "Object_ReadFromStream", "x"},
    {0x4167e0, CV_THIS, 2, K_VALUE, "Object_WriteFile", "sv"},
    {0x4b1cc0, CV_FAST, 1, K_VALUE, "FrameFilter_Ctor", "xxv"},
    {0x4b1d20, CV_FAST, 0, K_VALUE, "FrameFilter_Dtor", "xx"},
    {0x4b1d50, CV_THIS, 1, K_VALUE, "FrameFilterList_CloneInto", "x"},
    {0x4a8a30, CV_CDECL, 0, K_VALUE, "TgaFilters_Setup"},
    {0x4a9a30, CV_THIS, 5, K_BOOL, "CFrame::Load", "ssxvv"},
};
constexpr int NLOAD = sizeof LOAD_STUBS / sizeof LOAD_STUBS[0];
char g_object_args[MAXSLOT + 1];
void load_effect(int cls, int slot, DWORD *args, DWORD)
{
    if (cls == -1 && (DWORD)slot == 0x4a20a0) {   // the class name line
        char *line = at<char *>(args[0]);
        const char *t = g_case.fixed[1] ? "CFrame" : "CSomething";
        int i = 0;
        for (; t[i]; i++)
            line[i] = t[i];
        line[i] = 0;
    }
}
void s_load_entry(Gen &g)
{
    g_case.effect = &load_effect;
    memcpy(g_object_args, CLASSES[C_OBJECT].args, MAXSLOT);
    g_object_args[27] = '2';
    g_case.object_args = g_object_args;
    BYTE *list = xalloc(0x160);
    at_(list, 0x154) = ptrv(make_window(2, C_OBJECT));
    list[0x158] = g.chance(2) ? 0 : 1;
    set_kind(C_OBJECT, 27, K_FIXED2);   // never null: the original calls its slot 19 before the test
    g_case.fixed[2] = ptrv(make_window(1, C_CFRAME));
    g_case.fixed[0] = g.chance(4) ? 0 : g_case.fixed[2];
    g_case.fixed[1] = g.below(2);
    g_self = list;
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = g.chance(3) ? 0 : ptrv(xalloc(4));
}

const Test TESTS[] = {
    {"TextureList::LoadEntry", 0x4b4240, CALL_THIS, 3, ~0u, 300, s_load_entry, LOAD_STUBS, NLOAD},
    {"CFrame::WriteTga", 0x4aa070, CALL_THIS, 2, 0xff, 100, s_write_tga, TGA_STUBS, NTGA},
    {"CFrame::ReadFrm", 0x4a9340, CALL_THIS, 1, 0xff, 400, s_read_frm, FRM_STUBS, NFRM},
    {"CFrame::WriteFrm", 0x4a9740, CALL_THIS, 1, 0xff, 300, s_write_frm, FRM_STUBS, NFRM},
    {"StaticExit_FilterState", 0x43bf20, CALL_CDECL, 0, ~0u, 60, s_filter_state, STUBS, NSTUBS},
    {"Remap8_Row", 0x43cce0, CALL_FAST, 4, ~0u, 200, s_remap, STUBS, NSTUBS},
    {"HueRemapEntry_DeletingDtor", 0x43d6f0, CALL_THIS, 1, ~0u, 20, s_entry, STUBS, NSTUBS},
    {"Bmp_ReadHeader", 0x466ac0, CALL_FAST, 2, ~0u, 50, s_header, STUBS, NSTUBS},
    {"Bmp_ReadPalette", 0x466a40, CALL_FAST, 2, 0xff, 100, s_bmp_palette, STUBS, NSTUBS},
    {"Dither::Ctor", 0x4811b0, CALL_THIS, 4, ~0u, 60, s_dither_ctor, STUBS, NSTUBS},
    {"Dither::Free", 0x481240, CALL_THIS, 0, 0, 40, s_dither_free, STUBS, NSTUBS},
    {"Dither::Begin", 0x481380, CALL_THIS, 3, 0, 800, s_dither_rgb, STUBS, NSTUBS},
    {"Dither::Spread", 0x4814c0, CALL_THIS, 3, 0xff, 1500, s_dither_rgb, STUBS, NSTUBS},
    {"Dither::Quantize", 0x481290, CALL_THIS, 3, ~0u, 800, s_dither_quantize, STUBS, NSTUBS},
    {"TgaFilters_Setup", 0x4a8a30, CALL_CDECL, 0, ~0u, 10, s_filters, STUBS, NSTUBS},
    {"CFrame::FileName", 0x4a9900, CALL_THIS, 5, 0xff, 200, s_file_name, STUBS, NSTUBS},
    {"CFrame::Load", 0x4a9a30, CALL_THIS, 5, 0xff, 200, s_load, STUBS, NSTUBS},
    {"CFrame::Save", 0x4a9af0, CALL_THIS, 2, 0xff, 60, s_save, STUBS, NSTUBS},
    {"CFrame::LoadBmpPalette", 0x4aa180, CALL_THIS, 1, 0xff, 100, s_load_bmp, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(frame_rest4a)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        g_globals.add(0x716160, 4);
        g_globals.add(0x716164, 4);
        g_globals.add(0x71616c, 4);
        g_globals.add(0x716178, 4);
    }
    Tally t{"frame rest 4a: the error diffusion, the .bmp palette, the frame's file wrappers, the TGA filters, the texture list entry, the hue remap row"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 45000);
    t.report();
}

}  // namespace zax
