// The save/load screen's details page 0x564090 (docs/DECOMPILATION.md 9, "Phase 4 remains, band C"; the screen's
// smaller functions are in ui_save_load.cpp). Every call goes through its exe address.
//
// The screen [confirmed: the function below]: +0x78/+0x79 the font's border sizes, +0x138 the list of saves (may be
// null), +0x144 the second list, +0x148 the preview button, +0x14c the tab control (0x11c), +0x150 the page of the
// game's summary, +0x154 the mission details (0x104, vtable 0x5ff098, +0x100 the mission), +0x158/+0x15c the pages
// of the statistics (left and right), +0x160 a null-terminated list of pointers to the page members (their names
// the ZString pointers from 0x620d24 on), +0x18c the saves' statistics, +0x19c the statistics of the save in the
// first list. The statistics (CZaxSaveGameStats): +0xc/+0x14 texts, +0x10 the mission, +0x14 the preview image
// (the button's), +0x54 the percentage, +0x58 the playing time, +0x5c the date, +0x60/+0x64 two counts, +0x70/+0x74
// the items (each: +8 the right page, +0xc/+0x14 name and value, +0x10/+0x18 the values whose colour fonts
// StatsColorFont 0x5aedf0 gives).
// The rows' texts are the exe's function-local static ZStrings 0x7131c8, 0x713204, 0x713188, 0x713194 (guard
// 0x7131b8, atexit); their labels the localised ZStrings 0x7131f0, 0x713198, 0x713190, 0x7131c0, 0x71318c.
#include "ui_screens.h"

namespace zax::ui4c {

using namespace ui;

constexpr DWORD GOLD = 0x612264, BLUE = 0x60c53c, GREEN = 0x60bb18, RED = 0x60baa0, GUARD = 0x7131b8;

inline DWORD selected(DWORD list) { return vc(bp(list), 77); }
inline BYTE *new_page(BYTE *self, DWORD name_zstring)
{
    BYTE *w = alloc(0x100);
    if (w) {
        window_ctor(w, pv(self), c_str(name_zstring));
        m(w, 0) = vt(0x5ea72c);
    }
    return w;
}
inline void show_scroll_bars(DWORD w) { fn<void(THISCALL *)(DWORD, DWORD)>(0x40da30)(w, 1); }
inline void tab_add(BYTE *self, DWORD w) { fn<void(THISCALL *)(DWORD, DWORD)>(0x40a2f0)(m(self, 0x14c), w); }
inline DWORD client_width(DWORD w) { return fn<DWORD(THISCALL *)(DWORD)>(0x40d490)(w); }
inline void content_size(DWORD w, DWORD width) { fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d550)(w, width, m(bp(w), 0x10) - m(bp(w), 8)); }
// The page sized to the tab control's inside
inline void fit_page(BYTE *self, DWORD page)
{
    BYTE *t = bp(m(self, 0x14c));
    vc(bp(page), 59, m(t, 0xc) - m(t, 4), m(t, 0x10) - m(t, 8) - m(t, 0x110));
}
inline void delete_page(BYTE *self, DWORD off)
{
    if (BYTE *p = bp(m(self, off))) {
        vc(p, 0, 1);
        m(self, off) = 0;
    }
}
inline void new_tabs(BYTE *self)
{
    BYTE *t = alloc(0x11c);
    t = t ? fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x409be0)(t, m(self, 0x148)) : nullptr;
    m(self, 0x14c) = pv(t);
}
inline void detail_row(BYTE *self, DWORD page, const char *t1, DWORD f1, int a, int b, const char *t2, DWORD f2, int c, int d)
{
    fn<void(THISCALL *)(BYTE *, DWORD, const char *, DWORD, int, int, const char *, DWORD, int, int)>(0x564ce0)(self, page, t1, f1, a, b,
                                                                                                                t2, f2, c, d);
}
// A function-local static ZString of the exe (built once)
inline void static_zstring(BYTE bit, DWORD z, DWORD dtor)
{
    if (gb(glob_addr(GUARD)) & bit)
        return;
    gb(glob_addr(GUARD)) |= bit;
    zs_ctor(at<void *>(z));
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(dtor));
}
inline DWORD mission_number(DWORD mission) { return fn<DWORD(THISCALL *)(DWORD)>(0x512280)(mission); }
inline int ftol_double(long double v, DWORD k) { return x87_ftol(v * (long double)*at<double *>(k)); }

// The details page of the chosen save (tabs: the summary, the mission, the statistics left and right) or, with
// nothing chosen, empty pages with a label each; the pages are built again until their client width settles (a
// scroll bar appearing narrows them)
void THISCALL SaveLoadScreen_ShowDetails(BYTE *self)   // 0x564090
{
    BYTE *stats;
    if (selected(m(self, 0x144)) == 0xffffffff && (!m(self, 0x138) || selected(m(self, 0x138)) == 0xffffffff)) {
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(m(self, 0x148), 0, 1, 0);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4100f0)(m(self, 0x148), 0);
        DWORD tab = 0;
        if (BYTE *t = bp(m(self, 0x14c))) {
            tab = m(t, 0x114);
            vc(t, 0, 1);
            m(self, 0x14c) = 0;
        }
        new_tabs(self);
        BYTE *l2 = bp(m(self, 0x144));
        vc(bp(m(self, 0x14c)), 59, 200, m(l2, 0x10) - (int)(signed char)mb(self, 0x79) - m(l2, 8) - 0x96);
        vc(self, 31, m(self, 0x14c), 7, m(self, 0x148), NOWHERE, NOWHERE, 0);
        const DWORD *name = glob<const DWORD *>(0x620d24);
        for (DWORD *member = &m(self, 0x160); *member; member++, name++) {
            *at<DWORD *>(*member) = pv(new_page(self, *name));
            BYTE *label = new_label(self, c_str(glob_addr(0x7131ac)));
            vc(bp(*at<DWORD *>(*member)), 31, pv(label), 1, 0, NOWHERE, NOWHERE, 0);
            tab_add(self, *at<DWORD *>(*member));
        }
        fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40a4b0)(m(self, 0x14c), tab, 1);
        mb(bp(m(self, 0x14c)), 0x61) = 0;
        return;
    }
    if (m(self, 0x138) && selected(m(self, 0x138)) != 0xffffffff)
        stats = bp(m(self, 0x19c));
    else
        stats = bp(m(bp(m(self, 0x18c)), selected(m(self, 0x144)) * 4));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(m(self, 0x148), pv(stats + 0x14), 1, 0);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x4100f0)(m(self, 0x148), pv(stats + 0x14));
    DWORD tab = 0;
    BYTE *button = bp(m(self, 0x148));
    int wide = mi(button, 0xc) - mi(button, 4), left = wide, right = wide;
    if (BYTE *t = bp(m(self, 0x14c))) {
        tab = m(t, 0x114);
        wide = mi(bp(m(self, 0x150)), 0xc) - mi(bp(m(self, 0x150)), 4);
        left = mi(bp(m(self, 0x158)), 0xc) - mi(bp(m(self, 0x158)), 4);
        right = mi(bp(m(self, 0x15c)), 0xc) - mi(bp(m(self, 0x15c)), 4);
        vc(t, 0, 1);
        m(self, 0x14c) = 0;
    }
    new_tabs(self);
    BYTE *l2 = bp(m(self, 0x144));
    vc(bp(m(self, 0x14c)), 59, 200, m(l2, 0x10) - m(l2, 8) - (int)(signed char)mb(self, 0x79) - 0x96);
    vc(self, 31, m(self, 0x14c), 7, m(self, 0x148), NOWHERE, NOWHERE, 0);
    const int fh = (signed char)mb(self, 0x78);
    // the summary
    for (;;) {
        m(self, 0x150) = pv(new_page(self, gd(glob_addr(0x620d24))));
        show_scroll_bars(m(self, 0x150));
        const int third = (signed char)mb(self, 0x79) / 3;
        const long double v = (long double)(wide - 2 * (signed char)mb(self, 0x78) - third);
        const int x1 = ftol_double(v, glob_addr(0x5ff1d8));
        const int x2 = ftol_double(v, glob_addr(0x5ff1d0));
        const int column = third + (signed char)mb(self, 0x78) + x1;
        const DWORD page = m(self, 0x150);
        const char *label = c_str(glob_addr(0x7131f0));
        detail_row(self, page, label, glob_addr(BLUE), fh, x1, c_str(stats + 0x5c), glob_addr(GOLD), column, x2);
        static_zstring(1, glob_addr(0x7131c8), fn_addr(0x564cd0));
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4e0bc0)(glob_addr(0x7131c8), m(stats, 0x58));
        label = c_str(glob_addr(0x713198));
        detail_row(self, m(self, 0x150), label, glob_addr(BLUE), fh, x1, c_str(glob_addr(0x7131c8)), glob_addr(GOLD), column, x2);
        DWORD font;
        if (x87_lt((long double)m<float>(stats, 0x54), (long double)*glob<float *>(0x5ff1cc))) {
            font = glob_addr(GOLD);
            if (x87_lt((long double)m<float>(stats, 0x54), (long double)*glob<float *>(0x5ff1c8)))
                font = glob_addr(RED);
        } else {
            font = glob_addr(GREEN);
        }
        static_zstring(2, glob_addr(0x713204), fn_addr(0x564cc0));
        const char *percent = c_str(glob_addr(0x7131bc));
        zs_format(glob<void *>(0x713204), glob<const char *>(0x620f28), x87_ftol((long double)m<float>(stats, 0x54)), percent);
        label = c_str(glob_addr(0x713190));
        detail_row(self, m(self, 0x150), label, glob_addr(BLUE), fh, x1, c_str(glob_addr(0x713204)), font, column, x2);
        const DWORD mission = m(stats, 0x10);
        DWORD limit = mission ? mission_number(mission) << 2 : 0x14;
        if (m(stats, 0x60) >= limit) {
            font = glob_addr(GREEN);
        } else {
            limit = m(stats, 0x10) ? mission_number(m(stats, 0x10)) : 5;
            font = m(stats, 0x60) < limit ? glob_addr(RED) : glob_addr(GOLD);
        }
        static_zstring(4, glob_addr(0x713188), fn_addr(0x564cb0));
        const char *unit = c_str(glob_addr(0x71319c));
        zs_format(glob<void *>(0x713188), glob<const char *>(0x620f20), m(stats, 0x60), unit);
        label = c_str(glob_addr(0x7131c0));
        detail_row(self, m(self, 0x150), label, glob_addr(BLUE), fh, x1, c_str(glob_addr(0x713188)), font, column, x2);
        limit = m(stats, 0x10) ? mission_number(m(stats, 0x10)) << 1 : 10;
        if (m(stats, 0x64) >= limit) {
            font = glob_addr(GREEN);
        } else {
            limit = m(stats, 0x10) ? mission_number(m(stats, 0x10)) >> 1 : 5;
            font = m(stats, 0x64) < limit ? glob_addr(RED) : glob_addr(GOLD);
        }
        static_zstring(8, glob_addr(0x713194), fn_addr(0x564ca0));
        unit = c_str(glob_addr(0x71319c));
        zs_format(glob<void *>(0x713194), glob<const char *>(0x620f20), m(stats, 0x64), unit);
        label = c_str(glob_addr(0x71318c));
        detail_row(self, m(self, 0x150), label, glob_addr(BLUE), fh, x1, c_str(glob_addr(0x713194)), font, column, x2);
        content_size(m(self, 0x150), (DWORD)wide);
        fit_page(self, m(self, 0x150));
        if (client_width(m(self, 0x150)) == (DWORD)wide)
            break;
        wide = (int)client_width(m(self, 0x150));
        delete_page(self, 0x150);
    }
    tab_add(self, m(self, 0x150));
    // the mission details
    BYTE *details = alloc(0x104);
    if (details) {
        const char *name = c_str(gd(glob_addr(0x620d28)));
        const DWORD mission = m(stats, 0x10);
        window_ctor(details, pv(self), name);
        m(details, 0x100) = mission;
        m(details, 0) = vt(0x5ff098);
        show_scroll_bars(pv(details));
    }
    m(self, 0x154) = pv(details);
    fit_page(self, pv(details));
    tab_add(self, m(self, 0x154));
    // the statistics
    for (;;) {
        m(self, 0x158) = pv(new_page(self, gd(glob_addr(0x620d2c))));
        m(self, 0x15c) = pv(new_page(self, gd(glob_addr(0x620d30))));
        show_scroll_bars(m(self, 0x158));
        show_scroll_bars(m(self, 0x15c));
        const int third = (signed char)mb(self, 0x79) / 3;
        int base = -third - 2 * (signed char)mb(self, 0x78);
        const long double v = (long double)(base + left);
        const int xa = ftol_double(v, glob_addr(0x5ff090));
        const int xb = ftol_double(v, glob_addr(0x5ff088));
        base += right;
        const int xc = ftol_double((long double)base, glob_addr(0x5ee658));
        const int column = third + (signed char)mb(self, 0x78);
        const int left_value = xa + column, right_value = column + xc;
        for (DWORD i = 0; i < m(stats, 0x74); i++) {
            BYTE *item = bp(m(bp(m(stats, 0x70)), i * 4));
            DWORD page;
            int pos, x, align;
            if (!m(item, 8)) {
                page = m(self, 0x158), pos = xa, x = left_value, align = xb;
            } else {
                page = m(self, 0x15c), pos = xc, x = right_value, align = xc;
            }
            using Font = DWORD(THISCALL *)(DWORD);
            const DWORD f2 = fn<Font>(0x5aedf0)(m(item, 0x18));
            const char *t2 = c_str(item + 0x14);
            const DWORD f1 = fn<Font>(0x5aedf0)(m(item, 0x10));
            const char *t1 = c_str(item + 0xc);
            detail_row(self, page, t1, f1, fh, pos, t2, f2, x, align);
        }
        content_size(m(self, 0x158), (DWORD)left);
        content_size(m(self, 0x15c), (DWORD)right);
        fit_page(self, m(self, 0x158));
        fit_page(self, m(self, 0x15c));
        if (client_width(m(self, 0x158)) == (DWORD)left && client_width(m(self, 0x15c)) == (DWORD)right)
            break;
        left = (int)client_width(m(self, 0x158));
        right = (int)client_width(m(self, 0x15c));
        delete_page(self, 0x158);
        delete_page(self, 0x15c);
    }
    tab_add(self, m(self, 0x158));
    tab_add(self, m(self, 0x15c));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40a4b0)(m(self, 0x14c), tab, 1);
    mb(bp(m(self, 0x14c)), 0x61) = 1;
}

}  // namespace zax::ui4c

namespace zax {

using namespace ui4c;

ZAX_HOOKS(ui_save_load_rest4c)
{
    const Hook list[] = {
        {0x564090, ptr(&SaveLoadScreen_ShowDetails), "SaveLoadScreen::ShowDetails", 5, {0x83, 0xec, 0x2c, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
