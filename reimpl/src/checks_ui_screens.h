// The shared setup of the checks of the game's screens and dialogs (checks_ui_dialogs.cpp and the other screen
// checks) on the toolkit's harness (checks_ui_common.h), as the menus' checks (checks_ui_menus.cpp) built it:
// the constructors run on real objects in the extra area; the toolkit's constructors they call are stand-ins
// that give the made object the vtable of a fake of its class (the templates below), a random rectangle, the
// real font and a title (a ZString rep of the harness's own); the game's objects behind the globals are fakes of
// the class "game object".
#pragma once
#include "checks_ui_common.h"

namespace zax::uic::scr {

// Windows of the case: 0 the object under test (the Notify slots and destructors run on it), 1-4 and 11-13 the
// templates of the made windows, 5 the reporter, 6 its screen, 7 the parent, 8 the application, 9 the world,
// 10 another game object, 14 and 15 more windows
constexpr int W_BUTTON_T = 1, W_LABEL_T = 2, W_EDIT_T = 3, W_PLAIN_T = 4, W_REPORTER = 5, W_SCREEN = 6, W_PARENT = 7,
              W_APP = 8, W_WORLD = 9, W_OBJECT = 10, W_SLIDER_T = 11, W_LIST_T = 12, W_COMBO_T = 13, W_OTHER = 14,
              W_OTHER2 = 15;

// A made window: the template's vtable, a random rectangle, a title, the real font
inline void made(DWORD obj, int tmpl)
{
    if (!in_block(obj))
        return;
    BYTE *o = at<BYTE *>(obj);
    Gen &v = g_case.values;
    at_(o, 0) = at_(win(tmpl), 0);
    int *r = &at_<int>(o, 4);
    r[0] = v.range(-20, 500), r[1] = v.range(-20, 400), r[2] = r[0] + v.range(0, 300), r[3] = r[1] + v.range(0, 100);
    at_(o, 0x7c) = ptrv(real_font());
    const char *t = TEXTS[v.below(8)];
    BYTE *rep = xalloc(8 + 48);
    if (rep) {
        for (int i = 0; t[i] && i < 47; i++)
            rep[8 + i] = (BYTE)t[i];
        at_(o, 0x50) = ptrv(rep);
    }
}

// The template of a constructor stand-in (0 none); the module's own constructors come from `extra`
inline int template_of(DWORD ctor)
{
    switch (ctor) {
    case 0x40fa60:
    case 0x40f9b0: return W_BUTTON_T;
    case 0x4127d0:
    case 0x412930: return W_LABEL_T;
    case 0x410b50: return W_EDIT_T;
    case 0x4101f0: return W_SLIDER_T;
    case 0x4041f0:
    case 0x404520: return W_LIST_T;
    case 0x408040:
    case 0x408cd0: return W_COMBO_T;
    case 0x40a8a0:
    case 0x403d00:
    case 0x403ab0:
    case 0x402d80:
    case 0x56c400:
    case 0x599330: return W_PLAIN_T;
    default: return 0;
    }
}

// The INI file's GetDefault (0x46ec30: key, ZString out) gives one of the texts (assigned through the exe's
// ZString code, so the caller's own destructor frees it) when its answer is odd
inline void ini_effect(DWORD *args, DWORD answer)
{
    if (answer & 1)
        at<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(args[1], TEXTS[(answer >> 1) & 7]);
}

// The stand-ins every screen check uses: the allocators, the toolkit's constructors (made windows) and the
// toolkit's functions the screens call
#define UIC_SCREEN_STUBS \
    UIC_ALLOC_STUBS, {0x403d00, CV_THIS, 2, 'e', "CDialog::CDialog", "vs"}, {0x403ab0, CV_THIS, 2, 'e', "CDialog2::CDialog2", "vs"}, \
        {0x402d80, CV_THIS, 2, 'e', "CFrameWindow::CFrameWindow", "vs"}, {0x40a8a0, CV_THIS, 2, 'e', "CJWindow::CJWindow", "vs"}, \
        {0x40fa60, CV_THIS, 3, 'e', "Button_ctor"}, {0x40f9b0, CV_THIS, 3, 'e', "Button::Button", "vsv"}, \
        {0x4127d0, CV_THIS, 2, 'e', "TextLabel::TextLabel", "vs"}, {0x412930, CV_THIS, 2, 'e', "TextLabel::TextLabel(ZString)"}, \
        {0x410b50, CV_THIS, 2, 'e', "EditField::EditField", "vs"}, {0x4101f0, CV_THIS, 3, 'e', "CJSlider::CJSlider"}, \
        {0x40c7c0, CV_THIS, 6, K_VALUE, "Window_AddChild"}, {0x40c8d0, CV_THIS, 6, K_VALUE, "CJWindow::AddChildFront"}, \
        {0x40f5b0, CV_THIS, 1, K_VALUE, "CJWindow::SetWidth"}, {0x40f5d0, CV_THIS, 1, K_VALUE, "CJWindow::SetHeight"}, \
        {0x40d630, CV_THIS, 2, K_VALUE, "CJWindow::MoveTo"}, {0x40d680, CV_THIS, 1, K_VALUE, "CJWindow::SetLeft"}, \
        {0x40d6c0, CV_THIS, 1, K_VALUE, "CJWindow::SetTop"}, {0x40d510, CV_THIS, 2, K_VALUE, "CJWindow::SetSize"}, \
        {0x40ca40, CV_THIS, 1, K_VALUE, "CJWindow::Focus"}, {0x40d230, CV_THIS, 1, K_VALUE, "CJWindow::SameWidth"}, \
        {0x40e590, CV_THIS, 2, K_VALUE, "CJWindow::FitChild"}, {0x4035f0, CV_THIS, 2, K_VALUE, "CFrameWindow::Notify"}, \
        {0x403d70, CV_THIS, 0, K_VALUE, "CDialog::~CDialog"}, {0x40b190, CV_THIS, 0, K_VALUE, "CJWindow::~CJWindow"}, \
        {0x402f00, CV_THIS, 0, K_VALUE, "CFrameWindow::~CFrameWindow"}, {0x57e820, CV_THIS, 1, K_VALUE, "CSoundList::PlayName", "s"}, \
        {0x46ec30, CV_THIS, 2, K_VALUE, "IniFile::GetDefault", "sx"}, {0x523df0, CV_THIS, 2, K_SMALL, "ResourceList_Find", "sv"}, \
        {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"}, \
        {0x4a7310, CV_THIS, 2, K_VALUE, "CFont::TextWidth", "sv"}, {0x4a6140, CV_FAST, 0, K_SMALL, "CFont::LineHeight", "vx"}

// A fake window as the object under test (window 0) of the given class, with members at the offsets (fakes of
// the extra area or other windows; some null)
inline BYTE *members(Gen &g, BYTE *w, DWORD first, DWORD last)
{
    for (DWORD o = first; o <= last; o += 4)
        at_(w, o) = g.chance(8) ? 0 : ptrv(xalloc(0x10));
    return w;
}

// The "game object" class's stack arguments for a case from a list "slot:n,slot:n" (the others 0), kept in buf
// (113 bytes) for the case
inline const char *object_args(char *buf, const char *spec)
{
    for (int i = 0; i < MAXSLOT; i++)
        buf[i] = '0';
    buf[MAXSLOT] = 0;
    for (const char *s = spec; *s;) {
        int slot = 0;
        while (*s >= '0' && *s <= '9')
            slot = slot * 10 + (*s++ - '0');
        if (*s == ':')
            s++;
        if (slot < MAXSLOT && *s)
            buf[slot] = *s;
        while (*s && *s != ',')
            s++;
        if (*s == ',')
            s++;
    }
    g_case.object_args = buf;
    return buf;
}

// The base of a screen check: the templates, the game objects, the reporter and its screen, the globals of the
// application, the world and the reporter
inline void screen_base(Gen &g, void (*effect)(int, int, DWORD *, DWORD))
{
    g_case.effect = effect;
    g_case.nwin = 1;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_plain_window(0, C_DIALOG, g);
    make_window(W_BUTTON_T, C_BUTTON);
    make_window(W_LABEL_T, C_LABEL);
    make_window(W_EDIT_T, C_EDIT);
    make_window(W_SLIDER_T, C_SLIDER);
    make_window(W_LIST_T, C_LISTBOX);
    make_window(W_COMBO_T, C_COMBO);
    const int OBJECTS[] = {W_REPORTER, W_SCREEN, W_APP, W_WORLD, W_OBJECT};
    for (int i : OBJECTS)
        make_window(i, C_OBJECT);
    at_(win(W_REPORTER), 4) = ptrv(win(W_SCREEN));
    set_kind(C_OBJECT, 1, K_WINDOW_NN);
    set_kind(C_EDIT, 44, K_TEXT);
    set_kind(C_LABEL, 44, K_TEXT);
    set_kind(C_DIALOG, 58, K_RECT);
    set_kind(C_LABEL, 58, K_RECT);
    case_global(0x6c02cc, ptrv(win(W_REPORTER)));
    case_global(0x713f14, ptrv(win(W_APP)));
    case_global(0x6c2080, ptrv(win(W_WORLD)));
    g_self = win(0);
}

}  // namespace zax::uic::scr
