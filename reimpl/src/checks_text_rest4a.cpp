// Checks of the fonts' text measuring and line breaking, the text box getters, the font list's
// slots and the loading of a font strip (text_rest4a.cpp) against the original, with the toolkit's harness
// (checks_ui_common.h).
// - Measuring and breaking run on the game's default font (0x62e5d0, read only; its glyphs are the level's
//   CFrames) and on copies of it with no glyphs or another first/last character, over random texts of
//   letters, spaces, newlines, control characters below 0x14 and characters beyond the font; BreakLine with
//   a recording callback (every call logged with the word's offset and length) or none, with and without
//   force and a limit; WrapText's result compared as the ZString's text.
// - The text box getters on a fake font with LayoutTextBox 0x4a6190 a stand-in: same or other text pointer,
//   same or other characters in the copy, each of the eight values the same or not.
// - The lists on fakes: records with recorded objects (slot 1), the monitor (slot 7), the record list, the
//   aliases' destructor and the allocators as stand-ins.
// - The strip slicer on a recorded image frame (height, width and GetPixel answered by the case; the alpha byte
//   decides the markers), the frame copies, filters and AddFrame stand-ins, the CFrame constructor real (the
//   local frame and the new glyphs are compared); the .tga loader with the file functions stand-ins and the
//   decoded image turned into a recorded frame by ImageFile_Read's stand-in.
#include "checks_ui_common.h"

namespace zax {

int hooks_text_rest4a(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"text_rest4a", &hooks_text_rest4a};

const Stub LIST_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x591cd0, CV_THIS, 5, K_VALUE, "RecordList_Set", "vvvvv"},
    {0x55d0c0, CV_THIS, 0, K_VALUE, "ResourceAliases_Dtor"},
    {0x4a5e50, CV_THIS, 0, 'e', "CFont::Ctor"},
    {0x56b950, CV_THIS, 3, K_VALUE, "CSequence::LoadCacheBool", "vvv"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf", "v"},
    {0x4a6190, CV_THIS, 0, K_VALUE, "CFont::LayoutTextBox"},
};
constexpr int NLIST = sizeof LIST_STUBS / sizeof LIST_STUBS[0];

const Stub FONT_STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x4ab620, CV_THIS, 2, K_VALUE, "CFrame_CopyRect", "ar"},
    {0x4ab5d0, CV_THIS, 1, K_VALUE, "CFrame_CopyFrom", "a"},
    {0x4ab5a0, CV_THIS, 3, K_VALUE, "CFrame_SetSize", "vvv"},
    {0x4b21a0, CV_THIS, 1, K_VALUE, "FrameFilterList_Run", "v"},
    {0x56bb70, CV_THIS, 3, K_VALUE, "CSequence::AddFrame", "vvv"},
    {0x56b320, CV_THIS, 2, K_VALUE, "CSequence_SetNames", "ss"},
    {0x56b440, CV_THIS, 1, K_VALUE, "CSequence::SetupFilters", "v"},
    {0x4a1570, CV_FAST, 0, K_BOOL, "File_CanOpen", "sv"},
    // the FileBuf is the caller's local: its methods logged without ecx (fastcall shape, ecx ignored)
    {0x4a1780, CV_FAST, 0, K_VALUE, "FileBuf_Ctor", "xx"},
    {0x4a1840, CV_FAST, 2, K_BOOL, "FileBuf_Open", "xxsv"},
    {0x4a2190, CV_FAST, 0, K_SMALL, "FileBuf_Size", "xx"},
    {0x4a1b00, CV_FAST, 2, K_VALUE, "FileBuf_Read", "xxvv"},
    {0x4a2100, CV_FAST, 0, K_VALUE, "FileBuf_Close", "xx"},
    {0x4a1800, CV_FAST, 0, K_VALUE, "FileBuf_Dtor", "xx"},
    {0x58c6f0, CV_FAST, 3, K_BOOL, "ImageFile_Read", "xvsvv"},
};
constexpr int NFONT = sizeof FONT_STUBS / sizeof FONT_STUBS[0];

// ---- Texts and fonts ---------------------------------------------------------------------------------------

BYTE *rand_text(Gen &g, DWORD max)
{
    static const char LETTERS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?-'";
    const DWORD n = g.below(max + 1);
    BYTE *t = xalloc(n + 1);
    for (DWORD i = 0; i < n; i++) {
        const DWORD k = g.below(20);
        t[i] = k < 4 ? ' ' : k == 4 ? '\n' : k == 5 ? (BYTE)(1 + g.below(0x13)) : k == 6 ? (BYTE)(0x80 + g.below(0x80))
                                                                                          : (BYTE)LETTERS[g.below(sizeof LETTERS - 1)];
    }
    t[n] = 0;
    return t;
}
// The game's font, or a copy without glyphs or with another character range
BYTE *pick_font(Gen &g)
{
    BYTE *real = real_font();
    const DWORD k = g.below(6);
    if (k < 4 || !real)
        return real;
    BYTE *c = xalloc(0x84);
    memcpy(c, real, 0x84);
    if (k == 4)
        at_(c, 8) = 0;
    else
        at_(c, 0x3c) = 0x41, at_(c, 0x40) = 0x41 + g.below(26);
    return c;
}

BYTE *g_text_base;
void __fastcall break_callback(DWORD ctx, const BYTE *word, DWORD n)
{
    log_event(0x7a01, ctx ^ (DWORD)(word - g_text_base), n);
}

void base(Gen &) {}

void s_width(Gen &g)
{
    g_self = pick_font(g);
    g_args[0] = ptrv(rand_text(g, 40));
    g_args[1] = g.chance(4) ? 0xffffffff : g.below(45);
}
void s_max_line(Gen &g)
{
    g_self = pick_font(g);
    g_args[0] = ptrv(rand_text(g, 60));
}
void s_break(Gen &g)
{
    g_self = pick_font(g);
    g_text_base = rand_text(g, 60);
    g_args[0] = ptrv(g_text_base);
    g_args[1] = g.chance(8) ? 0 : g.below(3) ? g.below(300) : g.next();
    g_args[2] = g.chance(2) ? 0 : g.below(70);
    g_args[3] = g.next() & 0xff;
    g_args[4] = g.chance(3) ? 0 : ptrv((void *)&break_callback);
    g_args[5] = g.chance(2) ? 1 : g.next() & ~0xffu;
}
void s_wrap(Gen &g)
{
    g_self = pick_font(g);
    g_args[0] = ptrv(xalloc(4));
    g_args[1] = ptrv(rand_text(g, 80));
    g_args[2] = g.chance(8) ? 0 : g.below(400);
}
void s_box(Gen &g)
{
    BYTE *f = xalloc(0x84);
    for (int o = 0; o < 0x84; o += 4)
        at_(f, o) = g.next();
    BYTE *text = rand_text(g, 12);
    BYTE *copy = xalloc(16);
    memcpy(copy, text, 13);
    if (g.chance(3) && copy[0])
        copy[g.below(13)] ^= 1;
    const DWORD k = g.below(5);
    at_(f, 0x48) = k == 0 ? 0 : k == 1 ? ptrv(rand_text(g, 5)) : ptrv(text);
    at_(f, 0x4c) = ptrv(copy);
    for (int i = 0; i < 8; i++) {
        at_(f, 0x50 + 4 * i) = g.below(3);
        g_args[1 + i] = g.chance(12) ? 7 : at_(f, 0x50 + 4 * i);
    }
    g_self = f;
    g_args[0] = g.chance(10) ? 0 : ptrv(text);
}

// The list fakes: n records of 0x80 bytes behind a count dword, objects recorded game objects
BYTE *records(Gen &g, DWORD n)
{
    BYTE *b = xalloc(4 + 0x80 * (n ? n : 1));
    at_(b, 0) = n;
    for (DWORD i = 0; i < n; i++)
        at_(b, 4 + i * 0x80 + 0x78) = g.chance(3) ? 0 : ptrv(win((int)(1 + i)));
    return b + 4;
}
// The game objects' slots the lists call: 1 (deleting destructor, flags), 7 (the monitor's, flags), 39 (the
// entry's path: index, out)
char g_object_args[MAXSLOT + 1];
void list_objects()
{
    memcpy(g_object_args, CLASSES[C_OBJECT].args, MAXSLOT);
    g_object_args[1] = '1';
    g_object_args[7] = '1';
    g_object_args[39] = '2';
    g_case.object_args = g_object_args;
}
void s_list_dtor(Gen &g)
{
    list_objects();
    BYTE *l = xalloc(0x140);
    for (int i = 1; i < 8; i++)
        make_window(i, C_OBJECT);
    g_case.nwin = 8;
    const DWORD n = g.below(6);
    at_(l, 0x30) = g.chance(6) ? 0 : ptrv(records(g, n));
    at_(l, 0x34) = n;
    at_(l, 0x13c) = g.chance(2) ? 0 : ptrv(win(7));
    g_self = l;
    g_args[0] = g.chance(2) ? 1 : g.next() & ~1u;
}
void s_create(Gen &g)
{
    BYTE *l = xalloc(0x144);
    at_(l, 0x140) = g.next();
    g_self = l;
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = g.chance(3) ? 0 : ptrv(xalloc(4));
}
void s_file_of(Gen &g)
{
    list_objects();
    BYTE *l = make_window(0, C_OBJECT);
    for (int i = 1; i < 8; i++)
        make_window(i, C_OBJECT);
    g_case.nwin = 8;
    const DWORD n = g.below(6);
    at_(l, 0x30) = ptrv(records(g, n));
    at_(l, 0x34) = n;
    g_self = l;
    g_args[0] = ptrv(win((int)(1 + g.below(7))));
    g_args[1] = ptrv(xalloc(4));
}

// The strip slicer: the font (first/last character, flag 0x20) and the recorded image
BYTE *strip_font(Gen &g)
{
    BYTE *f = xalloc(0x84);
    for (int o = 0; o < 0x84; o += 4)
        at_(f, o) = g.next();
    at_(f, 0x1c) = g.chance(3) ? 0x20 : g.next() & ~0x20u;
    at_(f, 0x3c) = 0x20 + g.below(4);
    at_(f, 0x40) = g.chance(10) ? at_(f, 0x3c) - 2 : at_(f, 0x3c) + g.below(6);   // last = first - 1 divides by 0
    return f;
}
void strip_kinds(Gen &g)
{
    set_kind(C_CFRAME, 39, K_SMALL);
    set_kind(C_CFRAME, 45, g.chance(3) ? K_FIXED0 : K_SMALL);
    g_case.fixed[0] = 20 + g.below(200);
    set_kind(C_CFRAME, 54, K_VALUE);
}
void s_slice(Gen &g)
{
    g_self = strip_font(g);
    g_args[0] = ptrv(make_window(1, C_CFRAME));
    g_args[1] = g.next();
    strip_kinds(g);
}
// The loader's three local frames (two unused ones and the decoded image) are made by a stand-in of the CFrame
// constructor as recorded frames (zeroed, the recording vtable), so the slicer reads the case's image; the
// destructor is a stand-in too
CodePatch g_ctor_patch, g_dtor_patch;
BYTE *__fastcall fake_frame_ctor(BYTE *self)
{
    memset(self, 0, 0x40);
    at_(self, 0) = at_(win(1), 0);
    log_event(0x7a10, 0, 0);
    return self;
}
BYTE *__fastcall fake_frame_dtor(BYTE *self)
{
    log_event(0x7a11, 0, 0);
    return self;
}
void tga_after()
{
    unpatch(g_dtor_patch);
    unpatch(g_ctor_patch);
}
void s_tga(Gen &g)
{
    g_self = strip_font(g);
    make_window(1, C_CFRAME);
    g_args[0] = ptrv(TEXTS[g.below(8)]);
    g_args[1] = ptrv(TEXTS[g.below(8)]);
    g_args[2] = g.next();
    strip_kinds(g);
    patch_jump(g_ctor_patch, 0x4a8a90, (void *)&fake_frame_ctor);
    patch_jump(g_dtor_patch, 0x4a8b00, (void *)&fake_frame_dtor);
}

const Test TESTS[] = {
    {"CFont::TextWidthN", 0x4a7310, CALL_THIS, 2, ~0u, 600, s_width, nullptr, 0},
    {"CFont::MaxLineWidth", 0x4a7380, CALL_THIS, 1, ~0u, 600, s_max_line, nullptr, 0},
    {"CFont::BreakLine", 0x4a7510, CALL_THIS, 6, ~0u, 1500, s_break, nullptr, 0},
    {"CFont::WrapText", 0x4a7450, CALL_THIS, 3, ~0u, 500, s_wrap, LIST_STUBS, 6},
    {"CFont::BoxHeight", 0x4a7010, CALL_THIS, 9, ~0u, 300, s_box, LIST_STUBS, NLIST},
    {"CFont::BoxWidest", 0x4a7110, CALL_THIS, 9, ~0u, 150, s_box, LIST_STUBS, NLIST},
    {"CFont::BoxLastRight", 0x4a7210, CALL_THIS, 9, ~0u, 150, s_box, LIST_STUBS, NLIST},
    {"FontList::DeletingDtor", 0x4a7b20, CALL_THIS, 1, ~0u, 80, s_list_dtor, LIST_STUBS, NLIST},
    {"FontListBase::DeletingDtor", 0x4a7c30, CALL_THIS, 1, ~0u, 40, s_list_dtor, LIST_STUBS, NLIST},
    {"FontList::CreateFont", 0x4a7bd0, CALL_THIS, 3, ~0u, 40, s_create, LIST_STUBS, NLIST},
    {"FontList::FileOfObject", 0x4a7ce0, CALL_THIS, 2, ~0u, 120, s_file_of, LIST_STUBS, NLIST},
    {"Font_SliceStrip", 0x4a5e90, CALL_THIS, 2, ~0u, 300, s_slice, FONT_STUBS, NFONT},
    {"CFont::LoadTga", 0x4a62d0, CALL_THIS, 3, 0xff, 200, s_tga, FONT_STUBS, NFONT, &tga_after},
};

}  // namespace

ZAX_CHECKS(text_rest4a)
{
    (void)cls;
    (void)n;
    if (!real_font()) {
        log("Check text rest 4a: no default font");
        return;
    }
    Tally t{"text rest 4a: the fonts' measuring and line breaking, the text box getters, the font list, the font strips"};
    run_tests(t, g_hooks, nullptr, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 43000);
    t.report();
}

}  // namespace zax
