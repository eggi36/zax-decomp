// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the slots of the game's menus that no table listed: the main
// menu's background and logo (sequence players sized to the display), the main menu's frame and its buttons, the
// options menu's buttons, the end credits' text window and final screen, the chat line's keys and "Select Map"'s
// game type list.
//
// Every call goes through its exe address (the original's side of a check runs the original all the way down).
#include "ui_screens.h"

namespace zax::ui {

namespace {

inline BYTE *display() { return bp(m(bp(gd(glob_addr(G_REPORTER))), 4)); }
inline int display_w() { return vc<int>(display(), 45); }
inline int display_h() { return vc<int>(display(), 39); }
inline void set_left_(BYTE *w, int v) { fn<void(THISCALL *)(BYTE *, int)>(0x40d680)(w, v); }
inline void set_top_(BYTE *w, int v) { fn<void(THISCALL *)(BYTE *, int)>(0x40d6c0)(w, v); }
inline void move_to_(BYTE *w, int x, int y) { fn<void(THISCALL *)(BYTE *, int, int)>(0x40d630)(w, x, y); }
inline DWORD player_draw(BYTE *self, DWORD frame) { return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x56c600)(self, frame); }
inline void blit_at(DWORD frame, DWORD img, int x, int y, DWORD mode)
{
    fn<void(THISCALL *)(DWORD, DWORD, int, int, DWORD)>(0x4b3280)(frame, img, x, y, mode);
}
inline void fill(BYTE *frame, int x1, int y1, int x2, int y2)
{
    fn<void(THISCALL *)(BYTE *, int, int, int, int, DWORD)>(0x4b3a70)(frame, x1, y1, x2, y2, glob_addr(0x6be368));
}
inline DWORD replace_window(BYTE *self, DWORD w) { return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4040c0)(self, w, 1); }

}  // namespace

// The main menu's background (a sequence player, vtable 0x6054a4) slot 4: (frame) at 640 pixels width the player
// itself at (80, 60) in 640 x 480; else 800 x 600 at (0, 0): the background frame (+0x10c, a CFrame) made once
// from the sequence's first frame (slot 23 of +0x100 with 0; CFrame_SetSize 0x4ab5a0 in the source's depth, a copy
// of the source in a local CFrame blitted into it with the mode 0x6b9ca0) and blitted to the frame at (0, 0)
DWORD THISCALL MainMenuBg_Draw(BYTE *self, DWORD frame)   // 0x5beae0
{
    const int w = display_w();
    if (w == 0x280) {
        set_width(self, (DWORD)w);
        set_height(self, 0x1e0);
        move_to_(self, 0x50, 0x3c);
        return player_draw(self, frame);
    }
    set_width(self, 800);
    set_height(self, 600);
    move_to_(self, 0, 0);
    BYTE *src = vc<BYTE *>(bp(m(self, 0x100)), 23, 0);
    BYTE *bg = self + 0x10c;
    if (!vc(bg, 45)) {
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4ab5a0)(bg, 800, 600, vc(src, 29));
        const int sh = vc<int>(src, 39);
        const int sw = vc<int>(src, 45);
        int sr[4] = {0, 0, sw, sh};
        const int dh = vc<int>(bg, 39);
        const int dw = vc<int>(bg, 45);
        int dr[4] = {0, 0, dw, dh};
        alignas(4) BYTE tmp[0x40];
        fn<void(THISCALL *)(BYTE *)>(0x4a8a90)(tmp);
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x4ab5d0)(tmp, src);
        vc(bg, 23, tmp, sr, dr, glob_addr(0x6b9ca0));
        fn<void(THISCALL *)(BYTE *)>(0x4a8b00)(tmp);
    }
    blit_at(frame, pv(bg), 0, 0, glob_addr(0x6bb4e0));
    return 0;
}

// The main menu's logo (a sequence player, vtable 0x605374) slot 4: (frame) placed at its position (+0x10c, +0x110)
// in 640 x 480 coordinates, scaled to 800 x 600 (around the centre, 6 pixels more to the right on the right half);
// the player draws (0x56c600)
DWORD THISCALL MainMenuLogo_Draw(BYTE *self, DWORD frame)   // 0x5bec70
{
    int x, y;
    if (display_w() == 0x280) {
        y = mi(self, 0x110);
        x = mi(self, 0x10c);
    } else {
        const DWORD u = m(self, 0x10c);
        y = (int)((DWORD)(mi(self, 0x110) - 0xf0) * 600u) / 0x1e0 + 0xfa;
        x = (int)((u - 0x140) * 800u) / 0x280 + (u < 0x140 ? 0x136 : 0x13d);
    }
    move_to_(self, x, y);
    return player_draw(self, frame);
}

// The final end screen (vtable 0x605838) slot 4: (frame) the texture "Final End Screen" (the texture list 0x6bf850)
// centred vertically on the display (80 pixels to the left at 640 width); the children
DWORD THISCALL EndScreen_Draw(BYTE *self, DWORD frame)   // 0x5bf080
{
    constexpr DWORD LIST = 0x6bf850;
    const DWORD i = vc(glob<void *>(LIST), 3, glob<const char *>(0x62765c), -1);
    BYTE *img = bp(record_inline(glob_addr(LIST), i));
    BYTE *disp = display();
    const int ih = vc<int>(img, 39);
    const int dh = vc<int>(disp, 39);
    const int y = (dh - ih) / 2;
    const int x = display_w() == 0x280 ? -0x50 : 0;
    blit_at(frame, pv(img), x, y, glob_addr(0x6bb4e0));
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}

// The main menu (vtable 0x6055d4) slot 4: (frame) centred on the display; its corner window (+0x154) moved to the
// bottom right of 800 x 600 (720 x 540 at 640 width); when the frame is larger than the background (+0x134) the
// four margins around it filled black (Frame_FillRectXY 0x4b3a70 with 0x6be368); the dialog's drawing (0x403af0)
DWORD THISCALL MainMenu_Draw(BYTE *self, BYTE *frame)   // 0x5bf290
{
    const int w = mi(self, W_RECT + 8) - mi(self, W_RECT);
    set_left_(self, (display_w() - w) / 2);
    const int h = mi(self, W_RECT + 12) - mi(self, W_RECT + 4);
    set_top_(self, (display_h() - h) / 2);
    int W = 800, H = 600;
    if (display_w() == 0x280)
        W = 0x2d0, H = 0x21c;
    BYTE *c = bp(m(self, 0x154));
    move_to_(c, mi(c, 4) - mi(c, 0xc) + W, mi(c, 8) - mi(c, 0x10) + H);
    BYTE *bg = bp(m(self, 0x134));
    bool margins = mi(bg, 0xc) - mi(bg, 4) < vc<int>(frame, 45);
    if (!margins) {
        BYTE *bg2 = bp(m(self, 0x134));
        margins = mi(bg2, 0x10) - mi(bg2, 8) < vc<int>(frame, 39);
    }
    if (margins) {
        BYTE *b = bp(m(self, 0x134));
        const int bw = mi(b, 0xc) - mi(b, 4), bh = mi(b, 0x10) - mi(b, 8);
        const int top = (vc<int>(frame, 39) - bh) / 2;
        const int bottom = vc<int>(frame, 39) - top;
        const int lm = (vc<int>(frame, 45) - bw) / 2;
        const int right = vc<int>(frame, 45) - lm;
        const int tm = (vc<int>(frame, 39) - bh) / 2;
        const int left = (vc<int>(frame, 45) - bw) / 2;
        fill(frame, 0, 0, vc<int>(frame, 45), tm);
        const int fh = vc<int>(frame, 39);
        fill(frame, 0, bottom, vc<int>(frame, 45), fh);
        fill(frame, 0, tm, left, bottom);
        fill(frame, right, tm, vc<int>(frame, 45), bottom);
    }
    return fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x403af0)(self, frame);
}

// The main menu's slot 21: (from, what) the buttons replace the menu by the difficulty menu (+0x138), the server
// browser (+0x13c; a message instead in the demo 0x62485c or without DirectPlay: the reporter's platform slot 11),
// the save/load screen for loading (+0x140), the credits (+0x144), "Configure Menu" (+0x148, QuitMenu 0x5c0220); the
// exit button (+0x14c) closes the game window (0x4d2470; the demo shows its pictures first)
DWORD THISCALL MainMenu_Notify(BYTE *self, DWORD from, DWORD what)   // 0x5bf4a0
{
    if (what)
        return what;
    using Ctor0 = DWORD(THISCALL *)(BYTE *, DWORD);
    if (from == m(self, 0x138)) {
        BYTE *o = alloc(0x150);
        return replace_window(self, o ? fn<Ctor0>(0x5bf820)(o, 0) : 0);
    }
    if (from == m(self, 0x13c)) {
        using Box = DWORD(THISCALL *)(BYTE *, const char *, const char *);
        if (gb(glob_addr(0x62485c))) {
            const char *title = c_str(bp(glob_addr(0x714518)));
            return fn<Box>(0x40c740)(self, title, c_str(bp(glob_addr(0x714540))));
        }
        BYTE *platform = vc<BYTE *>(bp(gd(glob_addr(G_REPORTER))), 1);
        if (!(vc(platform, 11) & 0xff)) {
            BYTE z[4];
            zs_ctor(z);
            zs_format(z, glob<const char *>(0x6276ac), c_str(bp(glob_addr(0x71454c))));
            const char *title = c_str(bp(glob_addr(0x71451c)));
            fn<Box>(0x40c740)(self, title, c_str(z));
            return fn<DWORD(THISCALL *)(BYTE *)>(0x4defd0)(z);   // (eax: ZString_Dtor's)
        }
        BYTE *o = alloc(0x1a0);
        return replace_window(self, o ? fn<Ctor0>(0x518e00)(o, 0) : 0);
    }
    if (from == m(self, 0x140)) {
        BYTE *o = alloc(0x1a4);
        return replace_window(self, o ? fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x562970)(o, 0, 1) : 0);
    }
    if (from == m(self, 0x144)) {
        BYTE *o = alloc(0x140);
        return replace_window(self, o ? fn<Ctor0>(0x5c07f0)(o, 0) : 0);
    }
    if (from == m(self, 0x14c)) {
        if (gb(glob_addr(0x62485c)))
            fn<void(THISCALL *)(DWORD)>(0x5be0d0)(glob_addr(0x6273fc));
        return fn<DWORD(__cdecl *)()>(0x4d2470)();
    }
    if (from != m(self, 0x148))
        return from;
    BYTE *o = alloc(0x14c);
    if (!o)
        return replace_window(self, 0);
    const char *text = c_str(bp(glob_addr(0x714538)));
    return replace_window(self, fn<DWORD(THISCALL *)(BYTE *, DWORD, const char *)>(0x5c0220)(o, 0, text));
}

// The options menu (vtable 0x605bec) slot 21: (from, what) the back button (+0x148) closes it (slot 5); the video
// (+0x138), controls (+0x13c, with the world's key configuration: its slot 88), network (+0x144, when there is
// one) and sound (+0x140) buttons replace it by their dialog; the rest the frame window's (0x4035f0)
DWORD THISCALL OptionsMenu_Notify(BYTE *self, DWORD from, DWORD what)   // 0x5c0490
{
    if (!what) {
        if (from == m(self, 0x148))
            return vc(self, V_CLOSE);
        using Ctor1 = DWORD(THISCALL *)(BYTE *, BYTE *);
        if (from == m(self, 0x138)) {
            BYTE *o = alloc(0x180);
            return replace_window(self, o ? fn<Ctor1>(0x591020)(o, self) : 0);
        }
        if (from == m(self, 0x13c)) {
            BYTE *o = alloc(0x16c);
            if (!o)
                return replace_window(self, 0);
            const DWORD keys = vc(bp(gd(glob_addr(G_WORLD))), 88);
            return replace_window(self, fn<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4713b0)(o, self, keys));
        }
        if (from == m(self, 0x144) && m(self, 0x144)) {
            BYTE *o = alloc(0x15c);
            return replace_window(self, o ? fn<Ctor1>(0x46d010)(o, self) : 0);
        }
        if (from == m(self, 0x140)) {
            BYTE *o = alloc(0x180);
            return replace_window(self, o ? fn<Ctor1>(0x57efd0)(o, self) : 0);
        }
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4035f0)(self, from, what);
}

// The end credits' text window (vtable 0x605708) slot 4: (frame) framed (+0x104): a bevel around it (kind 15); the
// text (+0x100, CreditsText::Draw 0x475ff0) on its background (frame slot 28 in +0x80 when framed) inside the
// visible client area (0x40ef90, 2 pixels in), the frame's offset and clip set for it and put back
DWORD THISCALL CreditsText_Draw(BYTE *self, BYTE *frame)   // 0x5c0610
{
    if (mb(self, 0x104)) {
        BYTE *dark = self + 0xb0, *light = self + 0xa0;
        const DWORD d = (mb(dark, 0xc) & 1) ? mb(dark, 0xa) : (BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x53f160)(dark);
        const DWORD l = (mb(light, 0xc) & 1) ? mb(light, 0xa) : (BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x53f160)(light);
        if (!(gb(glob_addr(0x716190)) & 1)) {
            gb(glob_addr(0x716190)) |= 1;
            crt_atexit(code<void(__cdecl *)()>(0x401270));
        }
        gd(glob_addr(0x7161ac)) = m(self, W_CONTENT_H);
        gd(glob_addr(0x7161a0)) = 0;
        gd(glob_addr(0x7161a4)) = 0;
        gd(glob_addr(0x7161a8)) = m(self, W_CONTENT_W);
        fn<void(__fastcall *)(DWORD, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(m(self, W_FONT), frame, glob_addr(0x7161a0), l & 0xff,
                                                                                            d & 0xff, 0, 0xf, 0);
    }
    int r[4];
    if (!(fn<DWORD(THISCALL *)(BYTE *, int, int, int *, BYTE *)>(0x40ef90)(self, 2, 2, r, frame) & 0xff))
        return 0;
    const DWORD ox = vc(frame, 43);
    const DWORD oy = vc(frame, 44);
    int clip[4];
    vc(frame, 36, clip);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x524fe0)(frame, ox, oy);
    fn<void(THISCALL *)(BYTE *, int *)>(0x524f50)(frame, r);
    if (mb(self, 0x104))
        vc(frame, 28, r, self + 0x80);
    fn<void(THISCALL *)(DWORD, BYTE *, int *)>(0x475ff0)(m(self, 0x100), frame, r);
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x524fe0)(frame, ox, oy);
    return fn<DWORD(THISCALL *)(BYTE *, int *)>(0x524f50)(frame, clip);
}

// The chat line (vtable 0x605f88) slot 16: Enter sends the edit field's text (+0x100, slot 44; when not empty) to
// the game (NetGame_SendText 0x59b230 on the application) and closes the line (slot 5), Escape closes it; other
// keys the base's (0x40c170)
DWORD THISCALL Chat_Key(BYTE *self, DWORD k)   // 0x5c1170
{
    if (k == 0xd) {
        const char *text = vc<const char *>(bp(m(self, 0x100)), 44);
        if (text && *text)
            fn<void(THISCALL *)(DWORD, const char *)>(0x59b230)(gd(glob_addr(G_APP)), text);
    } else if (k != 0x1b) {
        return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, k);
    }
    vc(self, V_CLOSE);
    return 1;
}

// "Select Map" (vtable 0x605234) slot 79: the game type choice: a name list (FieldNameList 0x592200, vtable
// 0x5f0240) with "Default For Map" (-1) and every game type of the list 0x6c02e8 (its definitions loaded on first
// use; the names through FieldDesc_Name 0x5007a0), a "Game Type" label and a combo box (999999) on it (slot 31,
// placed 1 and 14), the first entry chosen; eax the label
DWORD THISCALL SelectMap_AddGameTypes(BYTE *self)   // 0x5bde70
{
    BYTE *names = alloc(0x14);
    BYTE *list = nullptr;
    if (names) {
        fn<void(THISCALL *)(BYTE *)>(0x592200)(names);
        m(names, 0) = vt(0x5f0240);
        list = names;
    }
    using Add = void(THISCALL *)(BYTE *, const char *, DWORD);
    m(self, 0x148) = pv(list);
    fn<Add>(0x5922b0)(list, glob<const char *>(0x6275b0), 0xffffffff);
    constexpr DWORD TYPES = 0x6c02e8;
    for (DWORD i = 0; i < gd(glob_addr(TYPES) + 0x34); i++) {
        const DWORD def = record_inline(glob_addr(TYPES), i);
        const char *name = fn<const char *(THISCALL *)(DWORD)>(0x5007a0)(def + 8);
        fn<Add>(0x5922b0)(bp(m(self, 0x148)), name, i);
    }
    BYTE *l = alloc(0x128);
    BYTE *label = l ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(l, self, glob<const char *>(0x61bcb0)) : nullptr;
    vc(self, V_ADD_CHILD, label, 1, 0, NOWHERE, NOWHERE, 0);
    BYTE *c = alloc(0x118);
    BYTE *combo = c ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x408040)(c, self, m(self, 0x148), 999999) : nullptr;
    m(self, 0x144) = pv(combo);
    vc(self, V_ADD_CHILD, combo, 0xe, 0, NOWHERE, NOWHERE, 0);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x408bc0)(m(self, 0x144), 0, 0);
    return pv(label);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_c3_menus)
{
    const Hook list[] = {
#include "ui_c3_menus_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
