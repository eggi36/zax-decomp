// Checks of the combo boxes, the edit field and the number field (ui_edit.cpp) against the original with the
// toolkit's harness (checks_ui_common.h). The combo box is a fake (window 0) with a fake closed list (window 2)
// and drop list (window 3) over a tree of the case (checks_ui_listmodel.h) and a fake button (window 4); the list
// box's, the drop list's and the button's constructors are stand-ins that make the new object a copy of these
// fakes. The edit field is a fake with a real title (ZString); the number field's edit field a fake of this file
// whose value slots (48 a float in st0, 50) log. The base class's functions, the frame's and the font's drawing, the
// keyboard, the sounds and the clipboard (0x46d700/0x46d760: never the real one) are stand-ins.
#include "checks_ui_listmodel.h"

namespace zax {

int hooks_ui_edit(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_edit", &hooks_ui_edit};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x40a8a0, CV_THIS, 2, 'e', "CJWindow::CJWindow", "vs"},
    {0x40f9b0, CV_THIS, 3, 'e', "Button::Button", "vsv"},
    {0x4041f0, CV_THIS, 4, 'e', "CJListBox::CJListBox"},
    {0x407a50, CV_THIS, 3, 'e', "DropList::DropList"},
    {0x40b190, CV_THIS, 0, K_VALUE, "CJWindow::~CJWindow"},
    {0x40d510, CV_THIS, 2, K_VALUE, "CJWindow::SetSize"},
    {0x40d2a0, CV_THIS, 1, K_VALUE, "CJWindow::SetRect", "r"},
    {0x40d680, CV_THIS, 1, K_VALUE, "CJWindow::SetLeft"},
    {0x40f5b0, CV_THIS, 1, K_VALUE, "CJWindow::SetWidth"},
    {0x40c7c0, CV_THIS, 6, K_VALUE, "Window_AddChild"},
    {0x40ca40, CV_THIS, 1, K_VALUE, "CJWindow::Focus"},
    {0x40bcc0, CV_THIS, 2, K_BOOL, "CJWindow::MouseDown", "vq"},
    {0x40bde0, CV_THIS, 2, K_VALUE, "CJWindow::MouseUp", "vq"},
    {0x40c0a0, CV_THIS, 4, K_BOOL, "Slot_False4", "qqvx"},
    {0x40c170, CV_THIS, 1, K_BOOL, "CJWindow::Key"},
    {0x40c3e0, CV_THIS, 1, K_VALUE, "CJWindow::SetActive"},
    {0x40c630, CV_THIS, 0, K_BOOL, "CJWindow::IsEnabled"},
    {0x40ea00, CV_THIS, 1, K_VALUE, "CJWindow::Cursor", "q"},
    {0x40ef90, CV_THIS, 4, K_BOOL, "CJWindow::VisibleClient", "vvxv"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    {0x40faa0, CV_THIS, 1, K_VALUE, "Button::Init"},
    {0x40cee0, CV_THIS, 1, K_VALUE, "CJWindow::SetTitleChar", "b"},
    {0x40a740, CV_FAST, 6, K_VALUE, "Bevel_Draw", "aarvvvvv"},
    {0x4b3a70, CV_THIS, 5, K_VALUE, "Frame_FillRectXY"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "Font_DrawText", "avvsvv"},
    {0x4a7310, CV_THIS, 2, K_SMALL, "Font_TextWidth", "sv"},
    {0x4a6140, CV_THIS, 0, K_SMALL, "Font_ThumbWidth"},
    {0x4a6290, CV_THIS, 1, K_SMALL, "Font_CharWidth"},
    {0x433be0, CV_THIS, 1, K_VALUE, "CBlendMode_SetOpacity"},
    {0x53f160, CV_THIS, 0, K_SMALL, "FieldColor_PaletteIndex"},
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x4d2460, CV_CDECL, 0, K_VALUE, "time"},
    {0x57e820, CV_THIS, 1, K_VALUE, "Sounds::Play", "s"},
    {0x4e2850, CV_THIS, 1, K_BOOL, "Keys::IsDown"},
    {0x46d700, CV_THIS, 2, K_VALUE, "Clipboard_Copy", "sv"},
    {0x46d760, CV_THIS, 0, K_FIXED4, "Clipboard_Paste"},
    {0x413c20, CV_THIS, 2, K_VALUE, "CJScrollBar::SetPos"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

constexpr int W_SELF = 0, W_BOX = 2, W_DROP = 3, W_BUTTON = 4, W_DOWN = 5, W_CALLBACK = 6, W_PARENT_ = 7, W_FRAME_ = 10;

// ---- The number field's edit field: a fake with a float value ------------------------------------------------

struct FloatEdit {
    DWORD vt;
    DWORD k;
    float values[8];
};
DWORD g_fe_vt[80];
DWORD THISCALL fe_delete(BYTE *s, DWORD f)
{
    log_event(0x600, ptrv(s), f);
    return 0;
}
DWORD THISCALL fe_title(BYTE *s, const char *t)
{
    log_event(0x62b, ptrv(s), t ? fnv(2166136261u, t, (unsigned)lstrlenA(t)) : 0);
    return 7;
}
long double THISCALL fe_get(BYTE *s)
{
    FloatEdit *e = reinterpret_cast<FloatEdit *>(s);
    const float v = e->values[e->k++ & 7];
    log_event(0x630, bits(v), 0);
    return v;
}
DWORD THISCALL fe_set(BYTE *s, float v)
{
    log_event(0x632, ptrv(s), bits(v));
    return 0;
}
DWORD THISCALL fe_other(BYTE *s)
{
    log_event(0x63f, ptrv(s), 0);
    return 0;
}

void init_vtables()
{
    init_model_vtables();
    for (int i = 0; i < 80; i++)
        g_fe_vt[i] = ptrv((void *)&fe_other);
    g_fe_vt[0] = ptrv((void *)&fe_delete);
    g_fe_vt[43] = ptrv((void *)&fe_title);
    g_fe_vt[48] = ptrv((void *)&fe_get);
    g_fe_vt[50] = ptrv((void *)&fe_set);
}

float some_float(Gen &g)
{
    switch (g.below(8)) {
    case 0: return 0.0f;
    case 1: {
        const DWORD nan = 0x7fc00000;
        float f;
        memcpy(&f, &nan, 4);
        return f;
    }
    case 2: return (float)g.range(-5, 5);
    case 3: return (float)g.range(-100, 100) / 7.0f;
    default: return (float)g.range(-30, 30) * 0.5f;
    }
}

// ---- Effects --------------------------------------------------------------------------------------------------

// The constructors' stand-ins: the base constructor keeps a prepared object (a fresh one gets the case's font, an
// empty title and the parent); the list box, the drop list and the button become copies of the case's fakes
void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls == C_CFRAME && slot == 36) {
        int *r = at<int *>(args[0]);
        r[0] = (int)(answer & 0x3f), r[1] = (int)((answer >> 6) & 0x3f), r[2] = r[0] + 60, r[3] = r[1] + 40;
    }
    if (cls != -1)
        return;
    BYTE *o = at<BYTE *>(answer);
    switch (slot) {
    case 0x40a8a0:
        if (!at_(o, 0x7c)) {
            at_(o, 0x7c) = ptrv(real_font());
            make_text(o + 0x50, args[1] ? at<const char *>(args[1]) : "");
            int *r = &at_<int>(o, 4);
            r[2] = r[0] + (int)(answer & 0x7f), r[3] = r[1] + 12 + (int)((answer >> 8) & 0xf);
        }
        at_(o, 0xe8) = args[0];
        break;
    case 0x4041f0: memcpy(o, win(W_BOX), 0x258); break;
    case 0x407a50: memcpy(o, win(W_DROP), 0x25c); break;
    case 0x40f9b0: memcpy(o, win(W_BUTTON), 0x138); break;
    case 0x40ef90: {   // VisibleClient: the client rect
        int *r = at<int *>(args[2]);
        r[0] = (int)(answer >> 8 & 0x1f), r[1] = (int)(answer >> 13 & 0x1f);
        r[2] = r[0] + 20 + (int)(answer >> 18 & 0x7f), r[3] = r[1] + 10 + (int)(answer >> 25 & 0x1f);
        break;
    }
    default: break;
    }
}

Model *g_model;

// A fake list box over the model (the closed list, the drop list)
void fill_list(Gen &g, BYTE *w)
{
    make_rows(g, w, g_model, g.chance(3));
    const DWORD n = at_(w, 0x108);
    at_(w, 0x110) = g.chance(4) || !n ? 0xffffffff : g.below(n);
    at_(w, 0x114) = g.below(100), at_(w, 0x118) = g.below(100);
    at_(w, 0x120) = 1 + g.below(14);
    at_(w, 0x11c) = at_(w, 0x120) > n ? n : at_(w, 0x120);
    at_(w, 0x124) = g.chance(4) ? 0 : 1;
    at_(w, 0x128) = ptrv(g_model);
    at_(w, 0x12c) = g.below(300), at_(w, 0x22c) = g.below(400);
    at_(w, 0x238) = g.below(6);
    at_(w, 0x248) = g.chance(2) ? 0 : ptrv(win(9));
    at_(w, 0x24c) = 0x602590;
}

const char *const SAMPLES[8] = {"", "12", "-3.5", "Zax", "hello world", "x", "1e3", "abcdefghij"};

void base(Gen &g)
{
    g_case.nwin = 8;
    g_case.effect = &effect;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_window(W_FRAME_, C_CFRAME);
    make_plain_window(9, C_SCROLLBAR, g);
    g_cols = g.chance(4) ? 1 : 4;
    g_model = make_model(g);
    fill_list(g, make_plain_window(W_BOX, C_LISTBOX, g));
    fill_list(g, make_plain_window(W_DROP, C_DROPLIST, g));
    make_plain_window(W_BUTTON, C_BUTTON, g);
    make_plain_window(W_DOWN, C_BUTTON, g);
    make_window(W_CALLBACK, C_CALLBACK);
    g_case.fixed[2] = 1 + g.below(25);
    g_case.fixed[4] = g.chance(4) ? 0 : ptrv(SAMPLES[g.below(8)]);   // the clipboard's text
    const int boxes[2] = {C_LISTBOX, C_DROPLIST};
    for (int c : boxes) {
        set_kind(c, 84, K_FIXED2);
        set_kind(c, 85, K_SMALL), set_kind(c, 86, K_SMALL), set_kind(c, 95, K_SMALL);
        set_kind(c, 78, K_TEXT), set_kind(c, 16, K_BOOL);
        set_kind(c, 96, K_FIXED3);   // the scroll bar slot 96 makes
    }
    g_case.fixed[3] = ptrv(win(9));
    set_kind(C_BUTTON, 80, K_BOOL);
    set_kind(C_EDIT, 76, K_BOOL), set_kind(C_EDIT, 23, K_BOOL), set_kind(C_EDIT, 16, K_BOOL);
    case_global(0x60a648, g.chance(2));
    case_global(0x716190, g.chance(4) ? 0 : 1);
}

// The combo box (window 0)
void s_combo(Gen &g)
{
    BYTE *w = make_plain_window(W_SELF, g.chance(2) ? C_COMBO : C_COMBO2, g);
    at_(w, 0x100) = ptrv(win(W_BOX));
    at_(w, 0x104) = g.chance(2) ? 0 : ptrv(win(W_DROP));
    at_(w, 0x108) = ptrv(win(W_BUTTON));
    at_(w, 0x10c) = g.below(300);
    w[0x110] = g.chance(2), w[0x111] = g.chance(2);
    const DWORD n = at_(win(W_BOX), 0x108);
    at_(w, 0x114) = n && !g.chance(4) ? at_(at<BYTE *>(at_(win(W_BOX), 0x104)) + g.below(n) * ROW, 0) : 0xffffffff;
    link_child(win(W_PARENT_), w);
    g_self = w;
}
int *point_in(Gen &g, BYTE *w)
{
    int *p = reinterpret_cast<int *>(xalloc(8));
    if (g.chance(2))
        p[0] = g.range(at_<int>(w, 0x14), at_<int>(w, 0x1c)), p[1] = g.range(at_<int>(w, 0x18), at_<int>(w, 0x20));
    else
        p[0] = g.range(-50, 600), p[1] = g.range(-50, 450);
    return p;
}
void s_combo_none(Gen &g) { s_combo(g); }
void s_combo_mouse(Gen &g)
{
    s_combo(g);
    g_args[0] = g.chance(4) ? g.below(3) : 0;
    g_args[1] = ptrv(point_in(g, win(W_BOX)));
}
void s_combo_key(Gen &g)
{
    s_combo(g);
    const DWORD keys[] = {0xd, 0x14, 6, 0xa, 9, 0x1b, 'a'};
    g_args[0] = keys[g.below(7)];
}
void s_combo_notify(Gen &g)
{
    s_combo(g);
    const int from[4] = {W_BOX, W_DROP, W_BUTTON, 1};
    g_args[0] = ptrv(win(from[g.below(4)]));
    g_args[1] = g.below(4);
}
void s_combo_rect(Gen &g)
{
    s_combo(g);
    if (g.chance(8))
        at_(g_self, 0x108) = 0;
    int *r = reinterpret_cast<int *>(xalloc(16));
    r[0] = g.range(-20, 300), r[1] = g.range(-20, 300), r[2] = r[0] + g.range(0, 300), r[3] = r[1] + g.range(0, 100);
    g_args[0] = ptrv(r);
}
void s_combo_list(Gen &g)
{
    s_combo(g);
    g_args[0] = ptrv(make_files(g));
}
void s_combo_id(Gen &g)
{
    s_combo(g);
    const DWORD n = at_(win(W_BOX), 0x108);
    g_args[0] = g.chance(5) ? 0xffffffff : n && g.chance(2) ? at_(at<BYTE *>(at_(win(W_BOX), 0x104)) + g.below(n) * ROW, 0)
                                                            : g_model->base + g.below(g_model->n);
    g_args[1] = g.chance(2);
}
void s_combo_dtor(Gen &g)
{
    s_combo(g);
    g_args[0] = g.chance(2) ? 1 : 0;
}
void s_combo_button(Gen &g)
{
    s_combo(g);
    g_args[0] = ptrv(win(g.below(8)));
}
// The constructors on window 0 prepared as the base constructor leaves a window (the font's height from 5)
void s_combo_ctor(Gen &g)
{
    BYTE *w = make_plain_window(W_SELF, C_WINDOW, g);
    for (DWORD o = 0x100; o < 0x120; o += 4)
        at_(w, o) = g.next();
    if (g.chance(5))
        at_(w, 0x7c) = ptrv(fake_font(5 + g.below(30)));
    g_self = w;
    g_args[0] = ptrv(win(W_PARENT_));
    g_args[1] = g.chance(3) ? ptrv(make_files(g)) : 0;
    g_args[2] = g.below(40);
}
void s_combo_init(Gen &g)
{
    s_combo(g);
    if (g.chance(5))
        at_(g_self, 0x7c) = ptrv(fake_font(5 + g.below(30)));
    g_args[0] = ptrv(win(W_BUTTON));
    g_args[1] = g.chance(3) ? ptrv(make_files(g)) : 0;
    g_args[2] = g.below(40);
}

// The edit field (window 0) with a title
void s_edit(Gen &g)
{
    BYTE *w = make_plain_window(W_SELF, C_EDIT, g);
    const char *t = SAMPLES[g.below(8)];
    make_text(w + 0x50, t);
    const DWORD len = (DWORD)lstrlenA(t);
    at_(w, 0x100) = g.below(len + 1);
    at_(w, 0x104) = g.chance(2) ? at_(w, 0x100) : g.below(len + 1);
    at_(w, 0x108) = g.chance(4) ? g.below(len + 1) : at_(w, 0x100);
    at_(w, 0x10c) = g.chance(2) ? 0 : g.below(40);
    w[0x110] = g.chance(3), w[0x111] = g.chance(3) ? (BYTE)g.below(12) : 0;
    at_<float>(w, 0x114) = g.chance(2) ? 0.0f : some_float(g);
    at_<float>(w, 0x118) = g.chance(2) ? 0.0f : some_float(g);
    w[0x11c] = g.chance(3), w[0x11d] = g.chance(4) ? 0 : 1;
    if (!g.chance(5))
        link_child(win(W_PARENT_), w);
    g_self = w;
}
void s_edit_flag(Gen &g)
{
    s_edit(g);
    g_args[0] = g.chance(3) ? g.below(3) : 1;
}
void s_edit_frame(Gen &g)
{
    s_edit(g);
    g_args[0] = ptrv(win(W_FRAME_));
}
void s_edit_mouse(Gen &g)
{
    s_edit(g);
    g_args[0] = g.chance(4) ? g.below(3) : 0;
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = at_<int>(g_self, 0x24) + g.range(-10, 120), p[1] = g.range(-10, 300);
    g_args[1] = ptrv(p);
}
void s_edit_drag(Gen &g)
{
    s_edit_mouse(g);
    g_args[0] = g_args[1];
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-5, 5), d[1] = g.range(-5, 5);
    g_args[1] = ptrv(d);
    g_args[2] = g.chance(3) ? g.below(4) : 1;
    g_args[3] = ptrv(xalloc(4));
}
void s_edit_key(Gen &g)
{
    s_edit(g);
    const DWORD keys[] = {1, 16, 8, 11, 12, 5, 14, 0, 3, 'a', 'Z', '5', '.', '-', ' ', 0xe4, 0x7f, 0x1ff};
    g_args[0] = keys[g.below(sizeof keys / 4)];
}
void s_edit_text(Gen &g)
{
    s_edit(g);
    g_args[0] = g.chance(6) ? 0 : ptrv(SAMPLES[g.below(8)]);
}
void s_edit_validate(Gen &g)
{
    s_edit(g);
    const char *const nums[8] = {"", "0", "5", "-7.25", "1e2", "abc", "3x", "  42"};
    g_args[0] = ptrv(nums[g.below(8)]);
}
void s_edit_point(Gen &g)
{
    s_edit(g);
    int *p = reinterpret_cast<int *>(xalloc(8));
    p[0] = g.range(0, 100), p[1] = g.range(0, 100);
    g_args[0] = ptrv(p);
}
void s_edit_ctor(Gen &g)
{
    BYTE *w = make_plain_window(W_SELF, C_WINDOW, g);
    if (g.chance(3))
        at_<int>(w, 0xc) = at_<int>(w, 4);   // no width: sized from the text
    for (DWORD o = 0x100; o < 0x120; o += 4)
        at_(w, o) = g.next();
    g_self = w;
    g_args[0] = ptrv(win(W_PARENT_));
    g_args[1] = g.chance(4) ? 0 : ptrv(SAMPLES[g.below(8)]);
}

// The number field (window 0): the float edit, the buttons (windows 4, 5), the callback (window 6)
void s_number(Gen &g)
{
    BYTE *w = make_plain_window(W_SELF, C_NUMBER, g);
    FloatEdit *e = reinterpret_cast<FloatEdit *>(xalloc(sizeof(FloatEdit)));
    e->vt = ptrv(g_fe_vt);
    for (int i = 0; i < 8; i++)
        e->values[i] = some_float(g);
    at_(w, 0x100) = ptrv(e);
    at_(w, 0x104) = ptrv(win(W_BUTTON)), at_(w, 0x108) = ptrv(win(W_DOWN));
    at_<float>(w, 0x10c) = g.chance(2) ? 0.0f : some_float(g);
    at_<float>(w, 0x110) = g.chance(2) ? 0.0f : some_float(g);
    at_<float>(w, 0x114) = g.chance(3) ? 1.0f : some_float(g);
    at_(w, 0x118) = g.chance(3) ? 0 : ptrv(win(W_CALLBACK));
    link_child(win(W_PARENT_), w);
    g_self = w;
}
void s_number_text(Gen &g)
{
    s_number(g);
    g_args[0] = ptrv(SAMPLES[g.below(8)]);
}
void s_number_flags(Gen &g)
{
    s_number(g);
    g_args[0] = g.chance(2) ? 1 : 0;
}
void s_number_mouse(Gen &g)
{
    s_number(g);
    g_args[0] = g.below(3);
    g_args[1] = ptrv(point_in(g, g_self));
}
void s_number_wheel(Gen &g)
{
    s_number(g);
    g_args[0] = (DWORD)g.range(-3, 3);
    g_args[1] = g.next();
}
void s_number_drag(Gen &g)
{
    s_number(g);
    g_args[0] = ptrv(point_in(g, g_self));
    int *d = reinterpret_cast<int *>(xalloc(8));
    d[0] = g.range(-5, 5), d[1] = g.chance(4) ? 0 : g.range(-9, 9);
    g_args[1] = ptrv(d);
    g_args[2] = g.below(4);
    g_args[3] = ptrv(xalloc(4));
}
void s_number_key(Gen &g)
{
    s_number(g);
    const DWORD keys[] = {6, 10, 5, 'a'};
    g_args[0] = keys[g.below(4)];
}
void s_number_child(Gen &g)
{
    s_number(g);
    g_args[0] = ptrv(win(g.below(8)));
}
void s_number_notify(Gen &g)
{
    s_number(g);
    const DWORD from[4] = {at_(g_self, 0x100), ptrv(win(W_BUTTON)), ptrv(win(W_DOWN)), ptrv(win(1))};
    g_args[0] = from[g.below(4)];
    g_args[1] = g.chance(2) ? 1 : g.below(4);
}
void s_number_ctor(Gen &g)
{
    BYTE *w = make_plain_window(W_SELF, C_WINDOW, g);
    for (DWORD o = 0x100; o < 0x11c; o += 4)
        at_(w, o) = g.next();
    g_self = w;
    g_args[0] = ptrv(win(W_PARENT_));
    g_args[1] = ptrv(SAMPLES[g.below(8)]);
    g_args[2] = ptrv(SAMPLES[g.below(8)]);
}
void clear_colour_garbage()
{
    for (DWORD o = 0x8a; o <= 0xda; o += 0x10)
        g_self[o] = 0;
}

const Test TESTS[] = {
    {"Combo ctor", 0x408040, CALL_THIS, 3, ~0u, 200, s_combo_ctor, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"Combo Slot44", 0x4080a0, CALL_THIS, 0, ~0u, 30, s_combo_none, STUBS, NSTUBS},
    {"Combo dtor", 0x4080b0, CALL_THIS, 1, ~0u, 100, s_combo_dtor, STUBS, NSTUBS},
    {"Combo Init", 0x408110, CALL_THIS, 3, 0, 300, s_combo_init, STUBS, NSTUBS},
    {"Combo MakeButton", 0x408290, CALL_THIS, 1, ~0u, 60, s_combo_button, STUBS, NSTUBS},
    {"Combo SetRect", 0x4082d0, CALL_THIS, 1, 0, 300, s_combo_rect, STUBS, NSTUBS},
    {"Combo MouseDown", 0x408390, CALL_THIS, 2, ~0u, 500, s_combo_mouse, STUBS, NSTUBS},
    {"Combo MouseUp", 0x4085b0, CALL_THIS, 2, ~0u, 500, s_combo_mouse, STUBS, NSTUBS},
    {"Combo Key", 0x4087c0, CALL_THIS, 1, ~0u, 600, s_combo_key, STUBS, NSTUBS},
    {"Combo Notify", 0x408a10, CALL_THIS, 2, 0, 400, s_combo_notify, STUBS, NSTUBS},
    {"Combo SetList", 0x408b50, CALL_THIS, 1, 0, 200, s_combo_list, STUBS, NSTUBS},
    {"Combo SelectedId", 0x408ba0, CALL_THIS, 0, ~0u, 30, s_combo_none, STUBS, NSTUBS},
    {"Combo SelectedText", 0x408bb0, CALL_THIS, 0, ~0u, 30, s_combo_none, STUBS, NSTUBS},
    {"Combo SelectId", 0x408bc0, CALL_THIS, 2, 0, 300, s_combo_id, STUBS, NSTUBS},
    {"Combo2 ctor", 0x408cd0, CALL_THIS, 3, ~0u, 200, s_combo_ctor, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"Edit ctor", 0x410b50, CALL_THIS, 2, ~0u, 300, s_edit_ctor, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"Edit dtor", 0x410c50, CALL_THIS, 1, ~0u, 60, s_edit_flag, STUBS, NSTUBS},
    {"Edit Draw", 0x410c80, CALL_THIS, 1, 0, 800, s_edit_frame, STUBS, NSTUBS},
    {"Edit MouseDown", 0x411020, CALL_THIS, 2, ~0u, 400, s_edit_mouse, STUBS, NSTUBS},
    {"Edit MouseDrag", 0x4110e0, CALL_THIS, 4, 0xff, 400, s_edit_drag, STUBS, NSTUBS},
    {"Edit Key", 0x411170, CALL_THIS, 1, ~0u, 1500, s_edit_key, STUBS, NSTUBS},
    {"Edit Copy", 0x411820, CALL_THIS, 1, 0xff, 200, s_edit_flag, STUBS, NSTUBS},
    {"Edit DeleteSelection", 0x411880, CALL_THIS, 1, 0xff, 300, s_edit_flag, STUBS, NSTUBS},
    {"Edit Cut", 0x4119a0, CALL_THIS, 1, 0xff, 200, s_edit_flag, STUBS, NSTUBS},
    {"Edit Paste", 0x4119e0, CALL_THIS, 1, 0xff, 200, s_edit_flag, STUBS, NSTUBS},
    {"Edit SetActive", 0x411a40, CALL_THIS, 1, ~0u, 100, s_edit_flag, STUBS, NSTUBS},
    {"Edit SetTitle", 0x411a80, CALL_THIS, 1, ~0u, 300, s_edit_text, STUBS, NSTUBS},
    {"Edit SetClipboard", 0x411ad0, CALL_THIS, 1, 0xff, 30, s_edit_flag, STUBS, NSTUBS},
    {"Edit Cursor", 0x411ae0, CALL_THIS, 1, ~0u, 60, s_edit_point, STUBS, NSTUBS},
    {"Edit Validate", 0x411b10, CALL_THIS, 1, 0xff, 400, s_edit_validate, STUBS, NSTUBS},
    {"Number ctor", 0x411b90, CALL_THIS, 3, ~0u, 200, s_number_ctor, STUBS, NSTUBS, nullptr, clear_colour_garbage},
    {"Number SetTitle", 0x411dd0, CALL_THIS, 1, ~0u, 30, s_number_text, STUBS, NSTUBS},
    {"Number dtor", 0x411df0, CALL_THIS, 1, ~0u, 60, s_number_flags, STUBS, NSTUBS},
    {"Number MouseUp", 0x411e50, CALL_THIS, 2, ~0u, 30, s_number_mouse, STUBS, NSTUBS},
    {"Number Wheel", 0x411e70, CALL_THIS, 2, 0xff, 60, s_number_wheel, STUBS, NSTUBS},
    {"Number MouseDrag", 0x411ea0, CALL_THIS, 4, 0xff, 600, s_number_drag, STUBS, NSTUBS},
    {"Number Key", 0x412000, CALL_THIS, 1, ~0u, 60, s_number_key, STUBS, NSTUBS},
    {"Number Focus", 0x412040, CALL_THIS, 1, ~0u, 30, s_number_child, STUBS, NSTUBS},
    {"Number Notify", 0x412050, CALL_THIS, 2, ~0u, 600, s_number_notify, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_edit)
{
    (void)cls;
    (void)n;
    init_vtables();
    if (!g_globals.n) {
        g_globals.add(0x60a648, 1);   // the line caret
        g_globals.add(0x716190, 1);   // the guard of the static rect 0x7161a0
        for (DWORD o = 0; o < 16; o += 4)
            g_globals.add(0x7161a0 + o, 4);
        g_globals.add(0x62e550, 4);   // the list boxes' sort
    }
    Tally t{"ui edit: the combo boxes, the edit field, the number field"};
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 30000);
    t.report();
}

}  // namespace zax
