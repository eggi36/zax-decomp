// Checks of the list boxes (ui_listbox.cpp) against the original with the toolkit's harness (checks_ui_common.h).
// The list box is a fake (window 0, its own slots recorded) with real rows; its source is a tree of the case
// (checks_ui_listmodel.h), its header a block with columns, its scroll bar a fake (window 9), the frames fakes
// (window 10), the toolkit's icon list (0x62e5c8) a fake with records. The base class's functions, the frame's and
// the font's drawing and the scroll bar's functions are stand-ins; the C runtime's string functions, qsort and the
// text helpers run.
#include "checks_ui_listmodel.h"

namespace zax {

int hooks_ui_listbox(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_listbox", &hooks_ui_listbox};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x40a8a0, CV_THIS, 2, 'e', "CJWindow::CJWindow", "vs"},
    {0x40b190, CV_THIS, 0, K_VALUE, "CJWindow::~CJWindow"},
    {0x40d510, CV_THIS, 2, K_VALUE, "CJWindow::SetSize"},
    {0x40d2a0, CV_THIS, 1, K_VALUE, "CJWindow::SetRect", "r"},
    {0x40d680, CV_THIS, 1, K_VALUE, "CJWindow::SetLeft"},
    {0x40f5b0, CV_THIS, 1, K_VALUE, "CJWindow::SetWidth"},
    {0x40f5d0, CV_THIS, 1, K_VALUE, "CJWindow::SetHeight"},
    {0x40d630, CV_THIS, 2, K_VALUE, "CJWindow::MoveTo"},
    {0x40cc70, CV_THIS, 0, K_WINDOW_NN, "CJWindow::Root"},
    {0x40c400, CV_THIS, 2, K_VALUE, "CJWindow::Notify"},
    {0x40bcc0, CV_THIS, 2, K_BOOL, "CJWindow::MouseDown", "vq"},
    {0x40bde0, CV_THIS, 2, K_VALUE, "CJWindow::MouseUp", "vq"},
    {0x40c0a0, CV_THIS, 4, K_BOOL, "Slot_False4", "qqvx"},
    {0x40c170, CV_THIS, 1, K_BOOL, "CJWindow::Key"},
    {0x40c3e0, CV_THIS, 1, K_VALUE, "CJWindow::SetActive"},
    {0x40c630, CV_THIS, 0, K_BOOL, "CJWindow::IsEnabled"},
    {0x40ea10, CV_THIS, 1, K_VALUE, "CJWindow::ParentSlot70", "q"},
    {0x40cd00, CV_THIS, 3, K_VALUE, "CJWindow::ShowTip", "svv"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x40a740, CV_FAST, 6, K_VALUE, "Bevel_Draw", "aarvvvvv"},
    {0x4b3a70, CV_THIS, 5, K_VALUE, "Frame_FillRectXY"},
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x524f50, CV_THIS, 1, K_VALUE, "ProxyFrame_SetClip", "r"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "Font_DrawText", "avvsvv"},
    {0x4a7310, CV_THIS, 2, K_SMALL, "Font_TextWidth", "sv"},
    {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"},
    {0x53f160, CV_THIS, 0, K_SMALL, "FieldColor_PaletteIndex"},
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x4d2460, CV_CDECL, 0, K_FIXED1, "time"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
    {0x413c20, CV_THIS, 2, K_VALUE, "CJScrollBar::SetPos"},
    {0x413bb0, CV_THIS, 2, K_VALUE, "CJScrollBar::SetRange"},
    {0x412fb0, CV_THIS, 2, K_VALUE, "CJScrollBar::ClipRect", "rq"},
    {0x4028f0, CV_THIS, 1, K_SMALL, "CJHeader::ColumnWidth"},
    {0x402920, CV_THIS, 1, K_BOOL, "CJHeader::ColumnFlag"},
    {0x402990, CV_THIS, 1, K_VALUE, "CJHeader::SetSortCallback"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x402070, CV_FAST, 1, K_VALUE, "Clamp", "vvv"},
    {0x401ef0, CV_THIS, 0, K_FIXED3, "GridControl::MakeBar"},
    {0x40c7c0, CV_THIS, 6, K_VALUE, "Window_AddChild"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_BOX = 0, W_OWNER = 1, W_OWNER_BOX = 2, W_CTOR = 3, W_PARENT_ = 7, W_BAR = 9, W_FRAME_ = 10, W_ICON = 11;

// The toolkit's icon list (0x62e5c8): slot 1 the file of a record, 2 the index of a name, 33 mark used, 37 load
struct Icons {
    DWORD vt;
    BYTE pad[0x2c];
    DWORD records;   // +0x30
    DWORD count;     // +0x34
    char dir[16];    // +0x38
    DWORD index;     // slot 2's answer
};
DWORD g_icons_vt[40];
DWORD THISCALL icons_file(BYTE *s, DWORD i)
{
    log_event(0x521, i, 0);
    (void)s;
    return i & 1 ? ptrv("Icons/Folder") : ptrv("File");
}
DWORD THISCALL icons_find(BYTE *s, const char *name, DWORD a)
{
    log_event(0x522, fnv(2166136261u, name, (unsigned)lstrlenA(name)), a);
    Icons *ic = reinterpret_cast<Icons *>(s);
    return ic->index++ % (ic->count + 1);
}
DWORD THISCALL icons_use(BYTE *s, DWORD h, DWORD on)
{
    (void)s;
    log_event(0x523, h, on);
    return 0;
}
DWORD THISCALL icons_load(BYTE *s, const char *dir, const char *name, DWORD *out)
{
    (void)s;
    log_event(0x525, fnv(2166136261u, dir, (unsigned)lstrlenA(dir)), fnv(2166136261u, name, (unsigned)lstrlenA(name)));
    (void)out;
    return ptrv(win(W_ICON));
}
DWORD THISCALL icons_other(BYTE *s)
{
    log_event(0x52f, ptrv(s), 0);
    return 0;
}

void init_vtables()
{
    init_model_vtables();
    for (int i = 0; i < 40; i++)
        g_icons_vt[i] = ptrv((void *)&icons_other);
    g_icons_vt[1] = ptrv((void *)&icons_file);
    g_icons_vt[2] = ptrv((void *)&icons_find);
    g_icons_vt[33] = ptrv((void *)&icons_use);
    g_icons_vt[37] = ptrv((void *)&icons_load);
}

BYTE *make_header(Gen &g)
{
    BYTE *h = xalloc(0x140);
    const DWORD n = g.chance(3) ? g.below(2) : 2 + g.below(3);
    DWORD *cols = reinterpret_cast<DWORD *>(xalloc(16));
    for (DWORD i = 0; i < 4; i++) {
        BYTE *c = xalloc(0x20);
        at_(c, 0x1c) = g.below(4);
        cols[i] = ptrv(c);
    }
    at_(h, 0x104) = ptrv(cols);
    at_(h, 0x108) = n;
    at_(h, 0x128) = g.below(6);
    at_(h, 0x12c) = g.chance(4) ? n + g.below(3) : g.below(n < g_cols ? n + 1 : g_cols);
    return h;
}

Icons *make_icons(Gen &g)
{
    Icons *ic = reinterpret_cast<Icons *>(xalloc(sizeof(Icons)));
    ic->vt = ptrv(g_icons_vt);
    // (no record table, or a record marked loading, gives a null icon both sides crash on)
    ic->count = 1 + g.below(4);
    BYTE *recs = xalloc(0x80 * 5);
    for (DWORD i = 0; i < 5; i++)
        at_(recs + i * 0x80, 0x78) = g.chance(2) ? ptrv(win(W_ICON)) : 0;
    ic->records = ptrv(recs);
    const char *d = "Cache/Sprites";
    for (int i = 0; d[i]; i++)
        ic->dir[i] = d[i];
    ic->index = g.below(3);
    return ic;
}

// The frame's slot 36 writes the clip into its argument; the icon list's records filled by LoadRecord
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_CFRAME && slot == 36) {
        int *r = at<int *>(args[0]);
        r[0] = (int)(answer & 0x3f), r[1] = (int)((answer >> 6) & 0x3f);
        r[2] = r[0] + 40 + (int)((answer >> 12) & 0xff), r[3] = r[1] + 30 + (int)((answer >> 20) & 0x7f);
    }
    if (cls == -1 && slot == 0x55e090) {
        Icons *ic = *at<Icons **>(0x62e5c8);
        if (ic->records && args[0] < 5)
            at_(at<BYTE *>(ic->records) + args[0] * 0x80, 0x78) = ptrv(win(W_ICON));
    }
    if (cls == -1 && slot == 0x40a8a0)   // the base constructor keeps the prepared object, sets the parent
        at_(at<BYTE *>(ptrv(g_self)), 0xe8) = args[0];
}

Model *g_model;

// A list box (fake or real) with rows of a model: the members of the case
void fill_box(Gen &g, BYTE *w, Model *m)
{
    const bool root_row = g.chance(3);
    make_rows(g, w, m, root_row);
    const DWORD n = at_(w, 0x108);
    at_(w, 0x110) = g.chance(4) || !n ? 0xffffffff : g.chance(12) ? n + g.below(3) : g.below(n);
    at_(w, 0x114) = g.chance(3) ? 0 : g.below(200);
    at_(w, 0x118) = g.chance(2) ? at_(w, 0x114) : g.below(200);
    at_(w, 0x120) = g.chance(5) ? g.below(3) : 1 + g.below(15);
    at_(w, 0x11c) = at_(w, 0x120) > n ? n : at_(w, 0x120);
    at_(w, 0x124) = g.chance(4) ? 0 : 1;
    at_(w, 0x128) = ptrv(m);
    at_(w, 0x12c) = g.below(300);
    char *typed = reinterpret_cast<char *>(w + 0x130);
    if (!g.chance(3)) {
        const char *t = WORDS[g.below(10)];
        const DWORD k = g.below(4);
        for (DWORD i = 0; i < k && t[i]; i++)
            typed[i] = t[i];
    }
    at_(w, 0x224) = g.below(4);
    at_(w, 0x228) = g.chance(2) ? 5000 : 3000;
    g_case.fixed[1] = g.chance(2) ? 5500 : 7000;   // time()
    at_(w, 0x22c) = g.below(400);
    w[0x230] = g.chance(5), w[0x231] = g.chance(6), w[0x232] = g.chance(4) ? 0 : 1, w[0x233] = g.chance(2);
    at_(w, 0x234) = root_row ? 1 : g.chance(5);
    at_(w, 0x238) = g.below(6);
    w[0x23c] = g.chance(2), w[0x23d] = g.chance(3);
    at_(w, 0x240) = 0;
    at_(w, 0x244) = g.chance(2) ? 0 : ptrv(make_header(g));
    at_(w, 0x248) = g.chance(2) ? 0 : ptrv(win(W_BAR));
    at_(w, 0x24c) = 0x602590;
    at_(w, 0x250) = 0;
    // (a foreign bar flag without a bar: the original's UpdateBar writes through null)
    w[0x254] = g.chance(4) ? 0 : 1, w[0x255] = g.chance(3) ? 0 : 1, w[0x256] = at_(w, 0x248) ? g.chance(4) : 0, w[0x257] = g.chance(3) ? 0 : 1;
}

void base(Gen &g)
{
    g_case.nwin = 8;
    g_case.effect = &effect;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_window(W_FRAME_, C_CFRAME);
    make_plain_window(W_BAR, C_SCROLLBAR, g);
    at_<float>(win(W_BAR), 0x14c) = (float)g.range(-50, 400) + (g.chance(2) ? 0.5f : 0.0f);
    make_window(W_ICON, C_OBJECT);
    set_kind(C_OBJECT, 39, K_SMALL), set_kind(C_OBJECT, 45, K_SMALL);
    g_cols = g.chance(4) ? 1 : 4;
    g_model = make_model(g);
    BYTE *w = make_plain_window(W_BOX, C_LISTBOX, g);
    if (g.chance(5))
        at_(w, 0x7c) = ptrv(fake_font(5 + g.below(30)));
    fill_box(g, w, g_model);
    if (!g.chance(5))
        link_child(win(W_PARENT_), w);
    g_case.fixed[2] = 1 + g.below(25);                // the row height (slot 84)
    g_case.fixed[3] = ptrv(win(W_BAR));               // the bar slot 96 makes
    const int box_classes[2] = {C_LISTBOX, C_DROPLIST};
    for (int c : box_classes) {
        set_kind(c, 84, K_FIXED2);
        set_kind(c, 85, K_SMALL), set_kind(c, 86, K_SMALL), set_kind(c, 95, K_SMALL);
        set_kind(c, 96, K_FIXED3);
        set_kind(c, 83, K_TEXT), set_kind(c, 78, K_TEXT), set_kind(c, 16, K_BOOL);
    }
    set_kind(C_CFRAME, 36, K_VALUE);
    Icons *ic = make_icons(g);
    case_global(0x62e5c8, ptrv(ic));
    const DWORD guards[4] = {0x62e534, 0x62e541, 0x62e54c, 0x62e540};
    for (DWORD a : guards)
        case_global(a, g.chance(2) ? 0 : (a == 0x62e540 ? g.below(8) : 1));
    const DWORD handles[6] = {0x62e528, 0x62e524, 0x62e52c, 0x62e538, 0x62e53c, 0x62e530};
    for (DWORD a : handles)
        case_global(a, g.below(ic->count + 2));
    case_global(0x716190, g.chance(4) ? 0 : 1);
    case_global(0x62e550, g.chance(2) ? ptrv(w) : 0);
    case_global(0x60a494, g.below(g_cols));
    case_global(0x62e556, g.chance(2));
    case_global(0x62e54d, g.chance(5));
    case_global(0x62e548, 0);
    g_self = w;
}

// ---- Setups -------------------------------------------------------------------------------------------------

DWORD some_id(Gen &g)
{
    if (g.chance(6))
        return 0xffffffff;
    const DWORD n = at_(g_self, 0x108);
    if (n && g.chance(2))
        return at_(at<BYTE *>(at_(g_self, 0x104)) + g.below(n) * ROW, 0);
    return g_model->base + g.below(g_model->n);
}
DWORD some_row(Gen &g)
{
    const DWORD n = at_(g_self, 0x108);
    if (g.chance(8))
        return 0xffffffff;
    return n ? g.below(n) : 0;
}
int *point(Gen &g)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(g_self, 0x24) + g.range(-30, 300), p[1] = at_<int>(g_self, 0x28) + g.range(-30, 300);
    if (g.chance(2))
        p[0] = g.range(at_<int>(g_self, 0x14), at_<int>(g_self, 0x1c)), p[1] = g.range(at_<int>(g_self, 0x18), at_<int>(g_self, 0x20));
    return p;
}

void s_none(Gen &) {}
void s_id(Gen &g)
{
    g_args[0] = some_id(g);
    g_args[1] = g.chance(2);
}
// A row that exists (Expand and Collapse index the rows without a test)
void s_real_row(Gen &g)
{
    const DWORD n = at_(g_self, 0x108);
    g_args[0] = n ? g.below(n) : 0;
}
void s_frame(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_));
    g_args[1] = (DWORD)g.range(-20, 300);
    g_args[2] = (DWORD)g.range(-20, 300);
    g_args[3] = g.below(5);
    g_args[4] = g.next();
    g_args[5] = some_row(g);
    g_case.argspec[36][0] = A_IGNORE;
}
void s_frame_text(Gen &g)
{
    s_frame(g);
    g_args[3] = ptrv(text_in_extra(g, 1 + (int)g.below(4)));
}
void s_frame_sunken(Gen &g)
{
    g_args[0] = ptrv(win(W_FRAME_));
    g_args[1] = g.chance(2);
    g_case.argspec[36][0] = A_IGNORE;
}
void s_rect(Gen &g)
{
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-50, 300), r[1] = g.range(-50, 300), r[2] = r[0] + g.range(0, 300), r[3] = r[1] + g.range(0, 400);
    g_args[0] = ptrv(r);
    // SetRect's stand-in leaves the rect as the case has it; the rows that fit divide by the row height
}
void s_notify(Gen &g)
{
    g_args[0] = g.chance(3) ? ptrv(win(W_BAR)) : ptrv(win(g.below(8)));
    g_args[1] = g.chance(2) ? 1 : g.below(4);
}
void s_double(Gen &g)
{
    g_args[0] = g.chance(4) ? g.below(3) : 0;
    g_args[1] = ptrv(point(g));
    g_args[2] = ptrv(xalloc(4));
}
void s_mouse(Gen &g)
{
    g_args[0] = g.below(3);
    g_args[1] = ptrv(point(g));
}
void s_drag(Gen &g)
{
    g_args[0] = ptrv(point(g));
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-9, 9), d[1] = g.range(-9, 9);
    g_args[1] = ptrv(d);
    g_args[2] = g.chance(4) ? g.below(8) : 1;
    g_args[3] = ptrv(xalloc(4));
}
void s_wheel(Gen &g)
{
    g_args[0] = (DWORD)g.range(-3, 3);
    g_args[1] = g.next();
}
void s_key(Gen &g)
{
    const DWORD keys[] = {5, 6, 7, 10, 14, 15, 11, 12, '+', '-', '*', '/', 8, 'a', 'z', 'G', ' ', '1', 9, 0x7f, 0x13, 0xd};
    g_args[0] = g.chance(6) ? g.below(0x80) : keys[g.below(sizeof keys / 4)];
    // typing searches the source's file list by entry index as an id: such ids exist (ShowId of an id the
    // source does not know would loop for ever, see checks_ui_listmodel.h)
}
void s_point(Gen &g) { g_args[0] = ptrv(point(g)); }
void s_find_text(Gen &g)
{
    g_args[0] = ptrv(WORDS[g.below(10)] + (g.chance(3) ? 1 : 0));
    g_args[1] = g.chance(4) ? 0 : ptrv(xalloc(4));
    g_args[2] = g.chance(2) ? 1 : g.chance(2) ? 0 : 0x100;
}
void s_set_list(Gen &g)
{
    at_(g_self, 0x244) = 0;   // (the file names have no tabs for a sort column)
    Files *f = make_files(g);
    g_args[0] = ptrv(f);
}
void s_header(Gen &g)
{
    g_args[0] = ptrv(make_header(g));
}
void s_sort_down(Gen &g) { g_args[0] = g.chance(2) ? g.below(2) : g.next(); }
void s_sort(Gen &g)
{
    const DWORD n = at_(g_self, 0x108);
    const DWORD first = n ? g.below(n) : 0;
    g_args[0] = first;
    g_args[1] = g.chance(6) ? 0 : n - first;
}
// The compare function of the sort on two rows of the box, the sort's globals as Sort sets them
void s_compare(Gen &g)
{
    BYTE *rows = at<BYTE *>(at_(g_self, 0x104));
    const DWORD n = at_(g_self, 0x108);
    if (!n) {
        rows = xalloc(2 * ROW);
        at_(rows, 0) = g_model->base, at_(rows + ROW, 0) = g_model->base;
    }
    g_args[0] = ptrv(rows + (n ? g.below(n) : 0) * ROW);
    g_args[1] = ptrv(rows + (n ? g.below(n) : 1) * ROW);
    case_global(0x62e544, ptrv(g_model));
    case_global(0x62e554, g.chance(2));
    case_global(0x62e555, g.chance(3));
    case_global(0x60a494, g.below(g_cols));
}
void s_insert(Gen &g)
{
    g_args[0] = g_model->base + g.below(g_model->n);
    g_args[1] = g.below(4);
    const DWORD n = at_(g_self, 0x108);
    g_args[2] = n ? g.below(n + 1) : 0;
    g_args[3] = g.below(16);
}
// SetSource: the old selection must lie in the rows (an id not shown would loop, as above, when it is less than
// the count)
void s_source(Gen &g)
{
    g_args[0] = ptrv(g.chance(2) ? g_model : make_model(g));   // (null: the original selects through it)
    g_args[1] = g.chance(2) ? 1 : g.chance(2) ? 0 : 0x100;
}
void s_scroll(Gen &g) { g_args[0] = g.chance(4) ? g.next() : g.below(300); }
void s_select(Gen &g)
{
    g_args[0] = some_row(g);
    g_args[1] = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_rows(Gen &g) { g_args[0] = g.below(20); }
void s_column(Gen &g)
{
    g_args[0] = ptrv(point(g));
    g_args[1] = g.chance(4) ? 0 : ptrv(xalloc(4));
}
void s_set_bar(Gen &g)
{
    g_args[0] = g.chance(3) ? 0 : ptrv(win(W_BAR));
    g_args[1] = g.chance(2) ? g.below(2) : g.next();
}
void s_reserve(Gen &g)
{
    g_self += 0x100;   // the rows array
    g_args[0] = g.chance(4) ? g.below(3) : g.below(40);
}

// The constructors on window 3 prepared as the base constructor leaves a window
void s_ctor(Gen &g)
{
    BYTE *w = make_plain_window(W_CTOR, C_WINDOW, g);
    for (DWORD o = 0x100; o < 0x260; o += 4)
        at_(w, o) = g.next();
    if (g.chance(5))
        at_(w, 0x7c) = ptrv(fake_font(5 + g.below(30)));
    g_self = w;
    g_args[0] = g.chance(4) ? 0 : ptrv(win(W_PARENT_));
    g_args[1] = g.chance(3) ? ptrv(make_files(g)) : 0;
    g_args[2] = g.below(40);
    g_args[3] = g.chance(3) ? g.below(3) : 1;
}
void s_ctor2(Gen &g)
{
    s_ctor(g);
    g_args[1] = g.chance(3) ? 0 : ptrv(g_model);
}
void clear_colour_garbage()
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        g_self[o] = 0;
}

// The drop list: window 0 of its class, its combo box window 1 (its box window 2, the drop list, the id)
void s_drop(Gen &g)
{
    BYTE *w = make_plain_window(W_BOX, C_DROPLIST, g);
    fill_box(g, w, g_model);
    BYTE *o = make_plain_window(W_OWNER, C_COMBO, g);
    BYTE *ob = make_plain_window(W_OWNER_BOX, C_LISTBOX, g);
    fill_box(g, ob, g_model);
    at_(ob, 0x110) = 0xffffffff;
    at_(o, 0x100) = ptrv(ob);
    at_(o, 0x104) = g.chance(5) ? 0 : ptrv(w);
    // an id of the combo box's box shown there (or -1)
    const DWORD n = at_(ob, 0x108);
    at_(o, 0x114) = n && !g.chance(4) ? at_(at<BYTE *>(at_(ob, 0x104)) + g.below(n) * ROW, 0) : 0xffffffff;
    at_(w, 0x258) = g.chance(8) ? 0 : ptrv(o);
    link_child(win(W_PARENT_), o);
    g_self = w;
}
void s_drop_code(Gen &g)
{
    s_drop(g);
    g_args[0] = g.below(4);
}
void s_drop_on(Gen &g)
{
    s_drop(g);
    if (!at_(g_self, 0x258))
        at_(g_self, 0x258) = ptrv(win(W_OWNER));
    g_args[0] = g.chance(2) ? 0 : g.chance(2) ? 1 : g.next();
}
void s_drop_mouse(Gen &g)
{
    s_drop(g);
    if (!at_(g_self, 0x258))
        at_(g_self, 0x258) = ptrv(win(W_OWNER));
    s_mouse(g);
}
void s_drop_key(Gen &g)
{
    s_drop(g);
    if (!at_(g_self, 0x258))
        at_(g_self, 0x258) = ptrv(win(W_OWNER));
    const DWORD keys[] = {9, 0x14, 0xd, 0x1b, 5, 6, 'a', 0x20, 0x13};
    g_args[0] = keys[g.below(sizeof keys / 4)];
}
void s_drop_frame(Gen &g)
{
    s_drop(g);
    g_args[0] = ptrv(win(W_FRAME_));
    g_case.argspec[23][1] = A_RECT, g_case.argspec[23][2] = A_RECT;
}
void s_drop_ctor(Gen &g)
{
    s_drop(g);
    s_ctor2(g);
    g_args[0] = ptrv(win(g.chance(2) ? W_OWNER : W_PARENT_));
    g_args[1] = ptrv(win(W_OWNER));
    g_args[2] = g.chance(4) ? 0 : ptrv(g_model);
    at_(win(W_OWNER), 0x24) = (DWORD)g.range(0, 400), at_(win(W_OWNER), 0x28) = (DWORD)g.range(0, 300);
    at_(win(W_OWNER), 0x30) = (DWORD)g.range(0, 400);
}
void s_dtor(Gen &g) { g_args[0] = g.chance(2) ? 1 : g.chance(2) ? 0 : 3; }

// The vector destructor iterator over blocks of the extra area with a destructor that only reads (0x592370)
void s_vector(Gen &g)
{
    const DWORD size = 8 + 4 * g.below(4), n = g.below(5);
    BYTE *p = xalloc(size * (n + 1));
    for (DWORD i = 0; i < size * n / 4; i++)
        at_(p, 4 * i) = g.next();
    g_args[0] = 0, g_args[1] = 0;
    g_args[2] = ptrv(p), g_args[3] = size, g_args[4] = n, g_args[5] = 0x592370;
}

// ListBoxListSource (vtable 0x602590) over a file list of the case
void s_lsource(Gen &g)
{
    BYTE *s = xalloc(8);
    at_(s, 0) = 0x602590;
    at_(s, 4) = ptrv(make_files(g));
    g_self = s;
    g_args[0] = g.chance(3) ? 0xffffffff : g.below(8);
    g_args[1] = g.below(8);
}
void s_lsource_ctor(Gen &g)
{
    g_self = xalloc(8);
    g_args[0] = g.chance(4) ? 0 : ptrv(make_files(g));
}

const Test TESTS[] = {
    {"ListBox ctor", 0x4041f0, CALL_THIS, 4, ~0u, 200, s_ctor, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"ListBox ctor2", 0x404520, CALL_THIS, 4, ~0u, 200, s_ctor2, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"ListBox SelectedId", 0x404330, CALL_THIS, 0, ~0u, 60, s_none, STUBS, NSTUBS},
    {"ListBox SelectedText", 0x404350, CALL_THIS, 0, ~0u, 60, s_none, STUBS, NSTUBS},
    {"ListBox Value", 0x404380, CALL_THIS, 0, ~0u, 30, s_none, STUBS, NSTUBS},
    {"ListBox SelectId", 0x404390, CALL_THIS, 2, 0, 500, s_id, STUBS, NSTUBS},
    {"ListBox SelectId2", 0x407270, CALL_THIS, 2, 0, 300, s_id, STUBS, NSTUBS},
    {"ListBox dtor", 0x4044a0, CALL_THIS, 1, ~0u, 100, s_dtor, STUBS, NSTUBS},
    {"ListBox InitDefaults", 0x404650, CALL_THIS, 0, 0, 50, s_none, STUBS, NSTUBS},
    {"ListBox SetRect", 0x4046f0, CALL_THIS, 1, 0, 300, s_rect, STUBS, NSTUBS},
    {"ListBox LineLast", 0x4047c0, CALL_THIS, 5, 0, 60, s_frame, STUBS, NSTUBS},
    {"ListBox LineFirst", 0x404830, CALL_THIS, 5, 0, 60, s_frame, STUBS, NSTUBS},
    {"ListBox LineMiddle", 0x4048b0, CALL_THIS, 5, 0, 60, s_frame, STUBS, NSTUBS},
    {"ListBox LinePass", 0x404930, CALL_THIS, 5, 0, 60, s_frame, STUBS, NSTUBS},
    {"ListBox IconOpen", 0x404980, CALL_THIS, 3, 0, 150, s_frame, STUBS, NSTUBS},
    {"ListBox IconClosed", 0x404af0, CALL_THIS, 3, 0, 150, s_frame, STUBS, NSTUBS},
    {"ListBox IconFile", 0x404c60, CALL_THIS, 3, 0, 150, s_frame, STUBS, NSTUBS},
    {"ListBox Notify", 0x404e00, CALL_THIS, 2, ~0u, 300, s_notify, STUBS, NSTUBS},
    {"ListBox DrawSelection", 0x404ec0, CALL_THIS, 2, 0, 150, s_frame, STUBS, NSTUBS},
    {"ListBox Draw", 0x404f60, CALL_THIS, 1, 0, 30, s_frame, STUBS, NSTUBS},
    {"ListBox DrawText", 0x404f80, CALL_THIS, 4, 0, 500, s_frame_text, STUBS, NSTUBS},
    {"ListBox DrawRow", 0x405260, CALL_THIS, 6, 0, 150, s_frame, STUBS, NSTUBS},
    {"ListBox IconWidth", 0x4052c0, CALL_THIS, 0, ~0u, 150, s_none, STUBS, NSTUBS},
    {"ListBox DrawList", 0x405480, CALL_THIS, 2, 0, 800, s_frame_sunken, STUBS, NSTUBS},
    {"ListBox DoubleClick", 0x405a90, CALL_THIS, 3, 0xff, 300, s_double, STUBS, NSTUBS},
    {"ListBox MouseDown", 0x405b30, CALL_THIS, 2, ~0u, 60, s_mouse, STUBS, NSTUBS},
    {"ListBox MouseDrag", 0x405b50, CALL_THIS, 4, 0xff, 400, s_drag, STUBS, NSTUBS},
    {"ListBox Wheel", 0x405c40, CALL_THIS, 2, ~0u, 200, s_wheel, STUBS, NSTUBS},
    {"ListBox Key", 0x405ce0, CALL_THIS, 1, ~0u, 1500, s_key, STUBS, NSTUBS},
    {"ListBox Tip", 0x406550, CALL_THIS, 1, 0xff, 500, s_point, STUBS, NSTUBS},
    {"ListBox FindRow", 0x4067b0, CALL_THIS, 1, ~0u, 100, s_id, STUBS, NSTUBS},
    {"ListBox RowText", 0x4067e0, CALL_THIS, 1, ~0u, 60, s_real_row, STUBS, NSTUBS},
    {"ListBox FindText", 0x406810, CALL_THIS, 3, ~0u, 300, s_find_text, STUBS, NSTUBS},
    {"ListBox SetList", 0x406890, CALL_THIS, 1, 0, 150, s_set_list, STUBS, NSTUBS},
    {"ListBox Expand", 0x4068b0, CALL_THIS, 1, 0, 400, s_real_row, STUBS, NSTUBS},
    {"ListBox ShowId", 0x406920, CALL_THIS, 1, 0, 400, s_id, STUBS, NSTUBS},
    {"ListBox Collapse", 0x4069d0, CALL_THIS, 1, 0, 400, s_real_row, STUBS, NSTUBS},
    {"ListBox SetHeader", 0x406aa0, CALL_THIS, 1, 0, 60, s_header, STUBS, NSTUBS},
    {"ListBox HeaderClick", 0x406b00, CALL_THIS, 0, 0, 300, s_none, STUBS, NSTUBS},
    {"ListBox SetSortDown", 0x406b60, CALL_THIS, 1, 0xff, 30, s_sort_down, STUBS, NSTUBS},
    {"ListBox Sort", 0x406b70, CALL_THIS, 2, 0, 400, s_sort, STUBS, NSTUBS},
    {"ListBox Compare", 0x406c60, CALL_CDECL, 2, ~0u, 300, s_compare, STUBS, NSTUBS},
    {"ListBox InsertChildren", 0x406e00, CALL_THIS, 4, 0, 400, s_insert, STUBS, NSTUBS},
    {"ListBox SetSource", 0x406fc0, CALL_THIS, 2, 0, 500, s_source, STUBS, NSTUBS},
    {"ListBox CenterSelection", 0x407380, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"ListBox MostScroll", 0x407480, CALL_THIS, 0, ~0u, 60, s_none, STUBS, NSTUBS},
    {"ListBox SetScroll", 0x4074b0, CALL_THIS, 1, 0, 60, s_scroll, STUBS, NSTUBS},
    {"ListBox UpdateBar", 0x4074f0, CALL_THIS, 0, 0, 300, s_none, STUBS, NSTUBS},
    {"ListBox RemakeBar", 0x407630, CALL_THIS, 0, 0, 100, s_none, STUBS, NSTUBS},
    {"ListBox CheckBar", 0x407660, CALL_THIS, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"ListBox Select", 0x4076c0, CALL_THIS, 2, 0, 600, s_select, STUBS, NSTUBS},
    {"ListBox SetRows", 0x407870, CALL_THIS, 1, 0, 100, s_rows, STUBS, NSTUBS},
    {"ListBox RowHeight", 0x4078e0, CALL_THIS, 0, ~0u, 30, s_none, STUBS, NSTUBS},
    {"ListBox Gap", 0x407900, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"ListBox Indent", 0x407910, CALL_THIS, 0, ~0u, 20, s_none, STUBS, NSTUBS},
    {"ListBox ColumnAt", 0x407930, CALL_THIS, 2, ~0u, 300, s_column, STUBS, NSTUBS},
    {"ListBox SetBar", 0x407a00, CALL_THIS, 2, 0, 60, s_set_bar, STUBS, NSTUBS},
    {"DropList ctor", 0x407a50, CALL_THIS, 3, ~0u, 200, s_drop_ctor, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"DropList dtor", 0x407b80, CALL_THIS, 1, ~0u, 60, s_dtor, STUBS, NSTUBS},
    {"DropList Changed", 0x407c00, CALL_THIS, 1, ~0u, 100, s_drop_code, STUBS, NSTUBS},
    {"DropList SetActive", 0x407c20, CALL_THIS, 1, 0, 150, s_drop_on, STUBS, NSTUBS},
    {"DropList MouseDown", 0x407c60, CALL_THIS, 2, ~0u, 300, s_drop_mouse, STUBS, NSTUBS},
    {"DropList MouseUp", 0x407d90, CALL_THIS, 2, ~0u, 300, s_drop_mouse, STUBS, NSTUBS},
    {"DropList Key", 0x407e80, CALL_THIS, 1, ~0u, 400, s_drop_key, STUBS, NSTUBS},
    {"DropList Draw", 0x407fb0, CALL_THIS, 1, 0, 100, s_drop_frame, STUBS, NSTUBS},
    {"VectorDtorIterator", 0x408e70, CALL_FAST, 6, 0, 60, s_vector, STUBS, NSTUBS},
    {"ListBoxRows Reserve", 0x408eb0, CALL_THIS, 1, 0, 150, s_reserve, STUBS, NSTUBS},
    {"ListBoxListSource ctor", 0x592350, CALL_THIS, 1, ~0u, 30, s_lsource_ctor, STUBS, NSTUBS},
    {"ListBoxListSource Parent", 0x592390, CALL_THIS, 1, ~0u, 30, s_lsource, STUBS, NSTUBS},
    {"ListBoxListSource Text", 0x5923a0, CALL_THIS, 1, ~0u, 60, s_lsource, STUBS, NSTUBS},
    {"ListBoxListSource Count", 0x5923c0, CALL_THIS, 1, ~0u, 60, s_lsource, STUBS, NSTUBS},
    {"ListBoxListSource Child", 0x5923e0, CALL_THIS, 2, ~0u, 60, s_lsource, STUBS, NSTUBS},
    {"ListBoxListSource Selectable", 0x592400, CALL_THIS, 1, ~0u, 60, s_lsource, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_listbox)
{
    (void)cls;
    (void)n;
    init_vtables();
    if (!g_globals.n) {
        const DWORD dwords[11] = {0x62e550, 0x62e544, 0x62e548, 0x60a494, 0x62e5c8, 0x62e528, 0x62e524, 0x62e52c, 0x62e538, 0x62e53c, 0x62e530};
        for (DWORD a : dwords)
            g_globals.add(a, 4);
        const DWORD bytes[9] = {0x62e554, 0x62e555, 0x62e556, 0x62e54d, 0x62e534, 0x62e541, 0x62e54c, 0x62e540, 0x716190};
        for (DWORD a : bytes)
            g_globals.add(a, 1);
        for (DWORD o = 0; o < 16; o += 4)
            g_globals.add(0x7161a0 + o, 4);
    }
    Tally t{"ui listbox: CJListBox, the drop list, ListBoxListSource"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 20000);
    t.report();
}

}  // namespace zax
