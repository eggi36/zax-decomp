// CJTabControl (vtable 0x5eb2d4, 0x11c bytes, "~CJTabControl"), the tab control of the window toolkit, and the
// toolkit's bevel drawing (docs/DECOMPILATION.md 9, "The window toolkit").
//
// Layout [confirmed: constructor 0x409be0, destructor 0x409c40, the functions below]: the CJWindow part, then
//   +0x100 tabs shown (a strip of tab headers above the pages), +0x104 the number of tabs, +0x108 the tab windows
//   (operator new, count dwords), +0x10c their header rects (16 bytes each, operator new; only with headers),
//   +0x110 the header height (font height + 2 * border + 2; 0 without headers), +0x114 the current tab,
//   +0x118 how much each header overlaps the one before when the strip is wider than the control.
// The tab windows are children of the control; slot 26 (0x68) of a tab window shows it (1, 0) or hides it (0, 0).
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD T_SHOWN = 0x100, T_COUNT = 0x104, T_TABS = 0x108, T_RECTS = 0x10c, T_HEIGHT = 0x110, T_CURRENT = 0x114,
                T_OVERLAP = 0x118;
BYTE *tab(BYTE *self, DWORD i) { return bp(at<DWORD *>(m(self, T_TABS))[i]); }
void *heap_new(DWORD n) { return fn<void *(__cdecl *)(DWORD)>(0x5d033a)(n); }
void heap_delete(DWORD p) { fn<void(__cdecl *)(DWORD)>(0x5d0320)(p); }

// The current tab hides, tab i shows and gets the focus (with the switch sound unless quiet)
void switch_to(BYTE *self, DWORD i)
{
    vc(tab(self, i), 26, 1, 0);
    vc(self, V_FOCUS, tab(self, i));
    vc(tab(self, m(self, T_CURRENT)), 26, 0, 0);
    m(self, T_CURRENT) = i;
}

}  // namespace

// The constructor: "~CJTabControl", headers shown, no tabs, no border (+0x78), takes the focus
BYTE *THISCALL Tabs_Ctor(BYTE *self, BYTE *parent)   // 0x409be0
{
    fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40a8a0)(self, parent, glob_addr(0x60a4fc));
    m(self, 0) = vt(0x5eb2d4);
    mb(self, T_SHOWN) = 1;
    m(self, T_COUNT) = 0;
    m(self, T_TABS) = 0;
    m(self, T_RECTS) = 0;
    mb(self, W_BORDER) = 0;
    mb(self, W_TAB_STOP) = 1;
    m(self, T_HEIGHT) = m(bp(m(self, W_FONT)), 0x80) + 2 * (DWORD)(int)(signed char)mb(self, W_BORDER3) + 2;
    return self;
}

// Slot 0: the two arrays, the base destructor, with bit 0 the pool block (0x11c)
BYTE *THISCALL Tabs_DeletingDtor(BYTE *self, DWORD flags)   // 0x409c40
{
    m(self, 0) = vt(0x5eb2d4);
    if (m(self, T_TABS)) {
        heap_delete(m(self, T_TABS));
        m(self, T_TABS) = 0;
    }
    if (m(self, T_RECTS)) {
        heap_delete(m(self, T_RECTS));
        m(self, T_RECTS) = 0;
    }
    fn<void(THISCALL *)(BYTE *)>(0x40b190)(self);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(self, 0x11c);
    return self;
}

// Slot 16: with headers, Ctrl+Tab (Ctrl+Shift+Tab) switches to the next (previous) tab, wrapping around; every
// other key the base class's
DWORD THISCALL Tabs_Key(BYTE *self, DWORD key)   // 0x40a1a0
{
    if (!mb(self, T_SHOWN) || key != 9 || (!key_down(0x1d) && !key_down(0x11d)))
        return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key);
    DWORD to;
    const DWORD cur = m(self, T_CURRENT);
    if (!key_down(0x2a) && !key_down(0x36)) {
        to = cur + 1;
        if (to >= m(self, T_COUNT))
            to = 0;
    } else {
        to = cur - 1;
        if (!cur)
            to = m(self, T_COUNT) - 1;
    }
    if (cur == to)
        return 1;
    play_sound(glob_addr(SOUND_SWITCH));
    switch_to(self, to);
    return 1;
}

// Headers on or off (their height with them, 0 without)
void THISCALL Tabs_ShowHeaders(BYTE *self, char on)   // 0x40a2b0
{
    mb(self, T_SHOWN) = (BYTE)on;
    if (on)
        m(self, T_HEIGHT) = m(bp(m(self, W_FONT)), 0x80) + 2 * (DWORD)(int)(signed char)mb(self, W_BORDER3) + 2;
    else
        m(self, T_HEIGHT) = 0;
}

// A new tab window: the array grows by one (copied, the old one deleted), the rects are made anew, the window
// becomes a child below the headers (slot 31: placing 5, top at the header height, flags 2) and the current
// tab; the one before hides; the headers are laid out and all tabs made as wide as the widest. Returns the
// tab's index.
DWORD THISCALL Tabs_Add(BYTE *self, BYTE *window)   // 0x40a2f0
{
    const DWORD index = m(self, T_COUNT);
    DWORD *tabs = static_cast<DWORD *>(heap_new(index * 4 + 4));
    if (m(self, T_COUNT)) {
        const DWORD *old = at<DWORD *>(m(self, T_TABS));
        for (DWORD i = 0; i < m(self, T_COUNT); i++)
            tabs[i] = old[i];
        if (m(self, T_TABS)) {
            heap_delete(m(self, T_TABS));
            m(self, T_TABS) = 0;
        }
    }
    m(self, T_COUNT)++;
    if (m(self, T_RECTS)) {
        heap_delete(m(self, T_RECTS));
        m(self, T_RECTS) = 0;
    }
    if (mb(self, T_SHOWN))
        m(self, T_RECTS) = pv(heap_new(m(self, T_COUNT) << 4));
    m(self, T_TABS) = pv(tabs);
    tabs[index] = pv(window);
    vc(self, V_ADD_CHILD, window, 5, 0, 0, m(self, T_HEIGHT), 2);
    if (m(self, T_COUNT) > 1)
        vc(bp(at<DWORD *>(m(self, T_TABS))[index - 1]), 26, 0, 0);
    m(self, T_CURRENT) = index;
    if (mb(self, T_SHOWN))
        fn<void(THISCALL *)(BYTE *)>(0x40a550)(self);
    fn<void(THISCALL *)(BYTE *, BYTE *)>(0x40d230)(bp(m(self, W_FIRST)), bp(m(self, W_LAST)));
    return index;
}

// The tab window with this title (the base class's child search)
BYTE *THISCALL Tabs_Find(BYTE *self, const char *title)   // 0x40a4a0
{
    return fn<BYTE *(THISCALL *)(BYTE *, const char *)>(0x40cc90)(self, title);
}

// Tab i becomes the current one (the switch sound unless quiet)
void THISCALL Tabs_Select(BYTE *self, DWORD i, char quiet)   // 0x40a4b0
{
    if (m(self, T_CURRENT) == i)
        return;
    if (!quiet)
        play_sound(glob_addr(SOUND_SWITCH));
    switch_to(self, i);
}

// Every child fitted in (slot 63 with the default border)
void THISCALL Tabs_FitAll(BYTE *self)   // 0x40a520
{
    for (BYTE *c = bp(m(self, W_FIRST)); c; c = bp(m(c, W_NEXT)))
        vc(self, V_FIT_CHILD, c, (DWORD)-1000);
}

// The header rects: one after another, each as wide as its title (slot 44, the font's width 0x4a7310 of all of it) plus
// 2 * border + 2, as high as the headers; when the strip is wider than the control (and there is more than one
// tab), the excess is spread: header i moves left by i * excess / (count - 1) (unsigned)
void THISCALL Tabs_Layout(BYTE *self)   // 0x40a550
{
    m(self, T_OVERLAP) = 0;
    const DWORD n = m(self, T_COUNT);
    if (!n)
        return;
    int x = 0;
    for (DWORD i = 0; i < m(self, T_COUNT); i++) {
        const char *title = reinterpret_cast<const char *>(vc(tab(self, i), 44));   // the title
        const int w = fn<int(THISCALL *)(BYTE *, const char *, int)>(0x4a7310)(bp(m(self, W_FONT)), title, -1);
        int *r = reinterpret_cast<int *>(m(self, T_RECTS) + i * 16);
        r[0] = x;
        x += w + 2 * (int)(signed char)mb(self, W_BORDER3) + 2;
        r[1] = 0;
        r[2] = x;
        r[3] = mi(self, T_HEIGHT);
    }
    const DWORD count = m(self, T_COUNT);
    const int right = reinterpret_cast<int *>(m(self, T_RECTS))[count * 4 - 2];
    const int width = mi(self, W_RECT + 8) - mi(self, W_RECT);
    if (right <= width || width <= 0 || count <= 1)
        return;
    const DWORD excess = (DWORD)(mi(self, W_RECT) - mi(self, W_RECT + 8) + right);
    m(self, T_OVERLAP) = excess / (count - 1);
    DWORD acc = excess;
    for (DWORD i = 1; i < m(self, T_COUNT); i++, acc += excess) {
        const int d = (int)(acc / (count - 1));
        int *r = reinterpret_cast<int *>(m(self, T_RECTS) + i * 16);
        r[0] -= d;
        r[2] -= d;
    }
}

// A bevel (fastcall: the font in ecx for its height, the frame in edx; the rect, two unused arguments, sunken,
// the edges (1 left, 2 top, 4 right, 8 bottom), flat): font height / 5 rings of one pixel, the top and left edges
// in the light colour, the right and bottom in the dark one (the toolkit's colours 0x62e5b8 / 0x62e5a8 / 0x62e590
// / 0x62e568; sunken swaps them, flat uses the outer pair). eax: 0 after drawing, else the multiplication's low
// half as the original leaves it.
DWORD __fastcall Bevel_Draw(BYTE *font, BYTE *frame, const int *rect, DWORD, DWORD, DWORD sunken, DWORD edges, DWORD flat)   // 0x40a740
{
    DWORD c1, c2, c3, c4;
    if (!sunken)
        c1 = glob_addr(0x62e5b8), c2 = glob_addr(0x62e5a8), c3 = glob_addr(0x62e590), c4 = glob_addr(0x62e568);
    else
        c1 = glob_addr(0x62e568), c2 = glob_addr(0x62e590), c3 = glob_addr(0x62e5a8), c4 = glob_addr(0x62e5b8);
    if (flat == 1)
        c3 = c4, c2 = c1;
    int l = rect[0], t = rect[1], r = rect[2], b = rect[3];
    const DWORD h = m(font, 0x80);
    DWORD n = h / 5;
    if (!n)
        return h * 0xcccccccdu;
    auto fill = [&](int x0, int y0, int x1, int y1, DWORD color) {
        fn<void(THISCALL *)(BYTE *, int, int, int, int, DWORD)>(0x4b3ce0)(frame, x0, y0, x1, y1, color);
    };
    int l1 = l + 1;
    do {
        if (edges & 2)
            fill(l, t, r, t + 1, c2);
        if (edges & 1)
            fill(l, t, l1, b, c2);
        if (edges & 4)
            fill(r - 1, t + 1, r, b - 1, c3);
        if (edges & 8)
            fill(l1, b - 1, r, b, c3);
        l++, l1++, t++, r--, b--;
    } while (--n);
    return 0;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_tabs)
{
    const Hook list[] = {
#include "ui_tabs_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
