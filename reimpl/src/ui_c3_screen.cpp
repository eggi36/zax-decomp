// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the slots of the toolkit's screen window CScreenWindow (vtable
// 0x5eb198; layout in ui_window.cpp), of the tab control CJTabControl (vtable 0x5eb2d4; layout in ui_tabs.cpp) and
// the frame window's frame (slot 76 of the frame windows, the message box and the dialogs) that no table listed:
// the screen's drawing (the tree through its proxy frame, the tool tip), its keys (the focus keys, the reporter's
// keys), its update (the mouse buttons, the key repeat, the typed characters); the tab headers' drawing, a click
// on a header, the pages' size; the frame window's title bar.
//
// Every call goes through its exe address (the original's side of a check runs the original all the way down).
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD G_RECT_GUARD = 0x716190, G_RECT = 0x7161a0;   // a function-local static rect (0, 0, w, h) and its guard
constexpr DWORD G_MASK = 0x6bcc68, G_REPORTER = 0x6c02cc, G_CURSOR_OBJ = 0x6cff40, G_BUTTONS_DOWN = 0x62e55c;

inline DWORD c_str(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13a0)(z); }
inline bool not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
inline void set_clip(DWORD frame, const int *r) { fn<void(THISCALL *)(DWORD, const int *)>(0x524f50)(frame, r); }
inline void draw_children(BYTE *self, DWORD frame) { fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame); }
inline void set_opacity(DWORD bits) { fn<void(THISCALL *)(DWORD, DWORD)>(0x433be0)(glob_addr(G_MASK), bits); }
inline DWORD tick() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }
inline bool key(DWORD scan) { return fn<bool(THISCALL *)(DWORD, DWORD)>(0x4e2850)(glob_addr(G_KEYS), scan); }
// A colour's palette index: the stored one (+0xa) when +0xc bit 0 says it is valid, else FieldColor_PaletteIndex
inline DWORD palette_index(BYTE *c) { return (mb(c, 0xc) & 1) ? mb(c, 0xa) : (BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x53f160)(c); }
inline void bevel(DWORD font, DWORD frame, const int *r, DWORD light, DWORD dark, DWORD a, DWORD kind, DWORD flag)
{
    fn<void(__fastcall *)(DWORD, DWORD, const int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(font, frame, r, light, dark, a, kind, flag);
}
// The toolkit's function-local static rect 0x7161a0 (its exit stub 0x401270 registered on first use)
inline void static_guard()
{
    if (!(gb(glob_addr(G_RECT_GUARD)) & 1)) {
        gb(glob_addr(G_RECT_GUARD)) |= 1;
        crt_atexit(code<void(__cdecl *)()>(0x401270));
    }
}
inline void set_cursor_image(DWORD cur)
{
    if (cur && cur != gd(glob_addr(0x6cff44)))
        fn<void(THISCALL *)(DWORD, DWORD)>(0x518590)(glob_addr(G_CURSOR_OBJ), cur);
}

}  // namespace

// ---- The frame window -------------------------------------------------------------------------------------------

// Slot 76 of the frame windows, the message box and the dialogs, the frame: (frame) unless transparent the window
// texture (slot 72) over the content, the bevel (0x40a740, kind 15) around it; with a title the title bar's
// texture (0x62e588 active / 0x62e58c inactive by slot 30) inside the bevel, the title centred and two tinted
// lines under it (Frame_TintRectXY 0x4b3ce0 with 0x62e5a8 and 0x62e590)
void THISCALL FrameWindow_DrawFrame(BYTE *self, DWORD frame)   // 0x402f80
{
    const int w = mi(self, W_CONTENT_W), h = mi(self, W_CONTENT_H);
    int size[4] = {0, 0, w, h};
    int r1[4];
    fn<void(THISCALL *)(int *, int, int, int, int)>(0x403240)(r1, 0, 0, w, h);
    int r2[4] = {r1[0], r1[1], r1[2], r1[3]};
    if (!mb(self, W_TRANSPARENT)) {
        const DWORD tex = vc(self, V_TEXTURE);
        vc(bp(frame), 23, tex, size, r2, glob_addr(0x6bc4e0));
    }
    const DWORD dark = palette_index(self + 0xb0);
    const DWORD light = palette_index(self + 0xa0);
    static_guard();
    fn<void(THISCALL *)(DWORD, int, int, int, int)>(0x4ee7f0)(glob_addr(G_RECT), 0, 0, mi(self, W_CONTENT_W), mi(self, W_CONTENT_H));
    bevel(m(self, W_FONT), frame, glob<int *>(G_RECT), light & 0xff, dark & 0xff, 1, 0xf, 0);
    if (!not_empty(self + W_TITLE))
        return;
    if (gd(glob_addr(0x62e588))) {
        int r3[4];
        fn<void(THISCALL *)(int *, int, int, int, int)>(0x403240)(r3, 0, 0, mi(self, W_CONTENT_W), mi(self, W_CONTENT_H));
        const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
        int bar[4] = {r3[0] + 3, r3[1] + 3, r3[2] - 3, (int)(fh + (fh / 5) * 4)};
        if (bar[2] > bar[0] && bar[3] > bar[1] && !mb(self, W_TRANSPARENT)) {
            int sz[4] = {0, 0, bar[2] - bar[0], bar[3] - bar[1]};
            if (vc(self, V_30) & 0xff)
                vc(bp(frame), 23, gd(glob_addr(0x62e588)), sz, bar, glob_addr(0x6bc4e0));
            else
                vc(bp(frame), 23, gd(glob_addr(0x62e58c)), sz, bar, glob_addr(0x6bc4e0));
        }
    }
    const DWORD fh = m(bp(m(self, W_FONT)), 0x80);
    const DWORD x = (DWORD)(mi(self, W_RECT + 8) - mi(self, W_RECT)) >> 1;
    using Draw = void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    fn<Draw>(0x4a6470)(m(self, W_FONT), frame, x, (fh / 5) * 2 + 1, c_str(self + W_TITLE), 1, 0);
    using Tint = void(THISCALL *)(DWORD, int, int, int, int, DWORD);
    const DWORD fh2 = m(bp(m(self, W_FONT)), 0x80);
    const int y = (int)(fh2 + (fh2 / 5) * 4);
    fn<Tint>(0x4b3ce0)(frame, 1, y + 1, mi(self, W_CONTENT_W), y + 2, glob_addr(0x62e5a8));
    const DWORD fh3 = m(bp(m(self, W_FONT)), 0x80);
    const int y2 = (int)(fh3 + (fh3 / 5) * 4);
    fn<Tint>(0x4b3ce0)(frame, 1, y2 + 2, mi(self, W_CONTENT_W), y2 + 3, glob_addr(0x62e590));
}

// ---- CScreenWindow ----------------------------------------------------------------------------------------------

// Slot 4: (unused) when the screen frame's clip (slot 36) changed width, the window takes the frame's size at
// (0, 0) (slot 57) and the proxy frame +0x100 follows the frame (slot 62); the tree drawn through the proxy frame
// at the screen origin, clipped to the content; the tool tip: its background (proxy slot 28 in +0x80), a
// rectangle around it (Frame_DrawRect in +0xa0), the wrapped text (CFont::DrawLines 0x4a66c0 in the tip's font)
// and the accelerator's underline (proxy slot 51)
void THISCALL Screen_Draw(BYTE *self, DWORD)   // 0x4090d0
{
    int clip[4];
    vc(bp(m(self, 0x104)), 36, clip);
    if (clip[2] != mi(self, W_RECT + 8)) {
        BYTE *frame = bp(m(self, 0x104));
        int fr[4];
        vc(frame, 36, fr);
        mi(self, W_RECT + 12) = fr[3];
        mi(self, W_RECT + 8) = fr[2];
        m(self, 0x104) = pv(frame);
        m(self, W_RECT) = 0;
        m(self, W_RECT + 4) = 0;
        vc(self, V_SET_RECT, self + W_RECT);
        vc(bp(m(self, 0x100)), 62, m(self, 0x104));
    }
    draw_children(self, m(self, 0x100));
    fn<void(THISCALL *)(DWORD, int, int)>(0x524fe0)(m(self, 0x100), mi(self, W_SCREEN), mi(self, W_SCREEN + 4));
    static_guard();
    gd(glob_addr(G_RECT)) = 0;
    gd(glob_addr(G_RECT) + 4) = 0;
    gd(glob_addr(G_RECT) + 8) = m(self, W_CONTENT_W);
    gd(glob_addr(G_RECT) + 12) = m(self, W_CONTENT_H);
    set_clip(m(self, 0x100), glob<int *>(G_RECT));
    if (!not_empty(self + 0x118))
        return;
    BYTE *tip = self + 0x120;
    int r[4] = {mi(tip, 0) - 1, mi(tip, 4) - 1, mi(tip, 8) + 1, mi(tip, 12) + 1};
    vc(bp(m(self, 0x100)), 28, tip, self + 0x80);
    fn<DWORD(THISCALL *)(DWORD, int *, BYTE *)>(0x4b3430)(m(self, 0x100), r, self + 0xa0);
    using Lines = void(THISCALL *)(DWORD, DWORD, int, int, DWORD, DWORD, DWORD);
    fn<Lines>(0x4a66c0)(m(self, 0x114), m(self, 0x100), mi(tip, 0), mi(self, 0x124), c_str(self + 0x11c), 2, 0);
    if (mb(self, 0x130)) {
        const int y = mi(self, 0x124) + mi(self, 0x134);
        const int l = mi(tip, 0);
        vc(bp(m(self, 0x100)), 51, mi(self, 0x138) + l, y, mi(self, 0x13c) + l, y, self + 0xa0);
    }
}

// Slot 76: when shown, slot 4 with the proxy frame (al the shown byte otherwise)
DWORD THISCALL Screen_Redraw(BYTE *self)   // 0x4092f0
{
    if (!mb(self, W_SHOWN))
        return 0;
    return vc(self, V_DRAW, m(self, 0x100));
}

// Slot 70: the tool tip hidden (ShowTipAccel 0x4098f0 with no text); true
DWORD THISCALL Screen_HideTip(BYTE *self, DWORD)   // 0x409310
{
    const DWORD r = fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4098f0)(self, 0, 0xffffffff, 0, 0xffffffff, 0xffffffff);
    return (r & 0xffffff00u) | 1;
}

// Slot 16, the keys: the base's (0x40c170), the accelerators (slot 17), the reporter's keys (its slot 10); then
// 3 and 4 slot 23 (Tab / focus forward), Ctrl+16 slot 23 and Shift+16 slot 24, 17 slot 25 (Shift: 22), 22
// slot 24, 24 slot 22, all with 1 (eax their al); 1 when one of the first three took the key, 0 otherwise
DWORD THISCALL Screen_Key(BYTE *self, DWORD k)   // 0x4093e0
{
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, k))
        return 1;
    if (vc(self, V_CHAR, k))
        return 1;
    if (vc(bp(gd(glob_addr(G_REPORTER))), 10, k) & 0xff)
        return 1;
    switch (k) {
    case 3:
    case 4:
        return vc(self, 23, 1) & 0xff;
    case 0x10:
        if (key(0x2a) || key(0x36))
            return vc(self, 24, 1) & 0xff;
        if (key(0x1d) || key(0x11d))
            return vc(self, 23, 1) & 0xff;
        return 0;
    case 0x11:
        if (key(0x2a) || key(0x36))
            return vc(self, 22, 1) & 0xff;
        return vc(self, 25, 1) & 0xff;
    case 0x16:
        return vc(self, 24, 1) & 0xff;
    case 0x18:
        return vc(self, 22, 1) & 0xff;
    default:
        return 0;
    }
}

// Slot 2, the update: (dt) when the cursor is shown (+0x108): the mouse moved (0x409b30), the wheel (slot 15 with
// the cursor position); the presses of the three buttons (slot 9; a handled press marks the button in 0x6cff58,
// an unhandled one is dropped); per button a new press (slot 10, the key repeat entry reset, the focus no longer
// by key) or a held one repeating while its entry's window is under the cursor (that window's slot 12, the entry
// due again), and a release (slot 11); each press and release asks for the cursor (slot 13 at (0, 0)); the
// buttons down kept in 0x62e55c; every typed character (Keys::ReadChar 0x4e2720) to the accelerators with Alt
// (slot 17) or the window (slot 16), the cursor asked again; then the base's update (0x40b5d0)
DWORD THISCALL Screen_Update(BYTE *self, DWORD dt)   // 0x409560
{
    DWORD now = tick();
    if (!mb(self, 0x108))
        return 0;
    fn<void(THISCALL *)(BYTE *)>(0x409b30)(self);
    int pos[2];
    fn<void(THISCALL *)(DWORD, int *)>(0x518500)(glob_addr(G_CURSOR_OBJ), pos);
    if (const DWORD wheel = fn<DWORD(THISCALL *)(DWORD)>(0x518520)(glob_addr(G_CURSOR_OBJ)))
        vc(self, 15, wheel, pos);
    DWORD buttons = fn<DWORD(THISCALL *)(DWORD)>(0x51f450)(glob_addr(G_CURSOR_OBJ));
    for (DWORD b = 0; b < 3; b++) {
        const DWORD bit = 1u << b;
        if (fn<bool(THISCALL *)(DWORD, DWORD)>(0x518540)(glob_addr(G_CURSOR_OBJ), bit)) {
            BYTE handled = 0;
            if (vc(self, 9, b, pos, &handled) & 0xff) {
                if (handled)
                    gd(glob_addr(0x6cff58)) = fn<DWORD(THISCALL *)(DWORD)>(0x51f450)(glob_addr(G_CURSOR_OBJ)) | bit;
                else
                    buttons &= ~bit;
            }
        }
    }
    const DWORD pressed = ~gd(glob_addr(G_BUTTONS_DOWN)) & buttons;
    BYTE *e = self + 0x148;
    for (DWORD i = 0; i < 3; i++, e += 0x10) {
        const DWORD bit = 1u << i;
        if (pressed & bit) {
            m(self, 0x10c) = tick();
            gb(glob_addr(G_FOCUS_BY_KEY)) = 0;
            m(self, 0x178) = i;
            m(e, 0) = 0;
            mb(self, (m(self, 0x178) << 4) + 0x154) = 0;
            vc(self, V_MOUSE_DOWN, i, pos);
            int zero[2] = {0, 0};
            DWORD cur = 0;
            vc(self, V_MOUSE_MOVE, self + 0x140, zero, &cur);
            set_cursor_image(cur);
        } else if (buttons & bit) {
            if (BYTE *w = bp(m(e, 0))) {
                if (now > m(e, 4)) {
                    if (pos[0] >= mi(w, W_VISIBLE) && pos[1] >= mi(w, W_VISIBLE + 4) && pos[0] < mi(w, W_VISIBLE + 8) &&
                        pos[1] < mi(w, W_VISIBLE + 12))
                        vc(w, V_MOUSE_12, i, pos);
                    mb(e, 0xc) = 1;
                    m(e, 4) = now + m(e, 8);
                }
            }
        }
        if ((gd(glob_addr(G_BUTTONS_DOWN)) & bit) && !(buttons & bit)) {
            m(self, 0x178) = i;
            m(self, 0x10c) = tick();
            vc(self, V_MOUSE_UP, i, pos);
            int zero[2] = {0, 0};
            DWORD cur = 0;
            vc(self, V_MOUSE_MOVE, self + 0x140, zero, &cur);
            set_cursor_image(cur);
            m(e, 0) = 0;
        }
    }
    gd(glob_addr(G_BUTTONS_DOWN)) = buttons;
    for (DWORD c; (c = fn<DWORD(THISCALL *)(DWORD)>(0x4e2720)(glob_addr(G_KEYS)) & 0xff) != 0;) {
        m(self, 0x10c) = tick();
        if (key(0x38) || key(0x138)) {
            if (vc(self, V_CHAR, c))
                continue;
        }
        vc(self, V_KEY, c);
        int zero[2] = {0, 0};
        now = 0;
        vc(self, V_MOUSE_MOVE, self + 0x140, zero, &now);
        set_cursor_image(now);
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b5d0)(self, dt);
}

// ---- CJTabControl -----------------------------------------------------------------------------------------------

// Slot 4: (frame) with headers every tab's header: a bevel (kind 7; raised for the current tab, the others one
// pixel lower at opacity 0.8 of Transparent Mask, their text inset by +0x118 less), clipped so the current tab
// overlaps its neighbours, the tab window's title (its slot 44); the pages (0x40b620); then the frame of the
// page area (kind 13) and the two lines left and right of the current tab's header (kind 2)
void THISCALL Tabs_Draw(BYTE *self, DWORD frame)   // 0x409cb0
{
    if (mb(self, 0x100)) {
        for (DWORD i = 0; i < m(self, 0x104); i++) {
            const int *tr = at<int *>(m(self, 0x10c) + i * 0x10);
            int r[4] = {tr[0], tr[1], tr[2], tr[3]};
            DWORD raised = 1;
            int inset = (signed char)mb(self, W_BORDER3);
            if (i != m(self, 0x114)) {
                r[1] += 1;
                set_opacity(0x3f4ccccd);
                raised = 0;
                inset -= mi(self, 0x118);
                if (inset < 0)
                    inset = 0;
            }
            int c[4] = {r[0], r[1], r[2], r[3]};
            const DWORD cur = m(self, 0x114);
            if (i != cur && i + 1 < m(self, 0x104))
                c[2] = *at<int *>(m(self, 0x10c) + i * 0x10 + 0x10);
            if (i && i - 1 == cur)
                c[0] = *at<int *>(m(self, 0x10c) + i * 0x10 - 8);
            set_clip(frame, c);
            const DWORD dark = palette_index(self + 0xb0);
            const DWORD light = palette_index(self + 0xa0);
            bevel(m(self, W_FONT), frame, r, light & 0xff, dark & 0xff, 1, 7, raised);
            const DWORD title = vc(bp(at<DWORD *>(m(self, 0x108))[i]), 44);
            using Draw = void(THISCALL *)(DWORD, DWORD, int, int, DWORD, DWORD, DWORD);
            fn<Draw>(0x4a6470)(m(self, W_FONT), frame, r[0] + inset + 1, (signed char)mb(self, W_BORDER3) + r[1] + 1, title, 0, 0);
            static_guard();
            gd(glob_addr(G_RECT) + 8) = m(self, W_CONTENT_W);
            gd(glob_addr(G_RECT)) = 0;
            gd(glob_addr(G_RECT) + 4) = 0;
            gd(glob_addr(G_RECT) + 12) = m(self, W_CONTENT_H);
            set_clip(frame, glob<int *>(G_RECT));
            set_opacity(0x3f800000);
        }
    }
    draw_children(self, frame);
    if (!mb(self, 0x100))
        return;
    int r[4] = {0, mi(self, 0x110), mi(self, W_CONTENT_W), mi(self, W_CONTENT_H)};
    DWORD dark = palette_index(self + 0xb0);
    DWORD light = palette_index(self + 0xa0);
    bevel(m(self, W_FONT), frame, r, light & 0xff, dark & 0xff, 1, 0xd, 1);
    if (!m(self, 0x104))
        return;
    if (const DWORD cur = m(self, 0x114)) {
        r[2] = *at<int *>(m(self, 0x10c) + (cur << 4));
        dark = palette_index(self + 0xb0);
        light = palette_index(self + 0xa0);
        bevel(m(self, W_FONT), frame, r, light & 0xff, dark & 0xff, 1, 2, 1);
    }
    r[2] = mi(self, W_CONTENT_W);
    r[0] = *at<int *>(m(self, 0x10c) + (m(self, 0x114) << 4) + 8);
    if (r[0] > r[2])
        return;
    dark = palette_index(self + 0xb0);
    light = palette_index(self + 0xa0);
    bevel(m(self, W_FONT), frame, r, light & 0xff, dark & 0xff, 1, 2, 1);
}

// Slot 10, the button press: (button, point) a left click on a header (searched from the last) switches to that
// tab (the switch sound, the new tab shown (slot 26 with 1, 0) and focused (slot 34), the old one hidden) and
// returns 1; anything else the base's (0x40bcc0)
DWORD THISCALL Tabs_MouseDown(BYTE *self, DWORD button, int *pt)   // 0x40a080
{
    if (mb(self, 0x100) && !button) {
        BYTE *s = bp(m(self, W_SCROLL));
        const int x = pt[0] - mi(self, W_SCREEN) + (s ? mi(s, S_X) : 0);
        const int y = pt[1] - mi(self, W_SCREEN + 4) + (s ? mi(s, S_Y) : 0);
        if (y < mi(self, 0x110)) {
            for (int i = (int)m(self, 0x104) - 1; i >= 0; i--) {
                const int *r = at<int *>(m(self, 0x10c) + (DWORD)i * 0x10);
                if (x >= r[0] && y >= r[1] && x < r[2] && y < r[3]) {
                    if (m(self, 0x114) != (DWORD)i) {
                        play_sound(glob_addr(SOUND_SWITCH));
                        vc(bp(at<DWORD *>(m(self, 0x108))[i]), 26, 1, 0);
                        vc(self, V_FOCUS, at<DWORD *>(m(self, 0x108))[i]);
                        vc(bp(at<DWORD *>(m(self, 0x108))[m(self, 0x114)]), 26, 0, 0);
                        m(self, 0x114) = (DWORD)i;
                    }
                    return 1;
                }
            }
        }
    }
    return fn<DWORD(THISCALL *)(BYTE *, DWORD, int *)>(0x40bcc0)(self, button, pt);
}

// Slot 57, the rectangle: the base's (0x40d2a0); with tabs the headers laid out (0x40a550, with headers) and every
// tab window sized to the control below the headers (slot 59)
void THISCALL Tabs_SetRect(BYTE *self, int *rect)   // 0x40a420
{
    fn<void(THISCALL *)(BYTE *, int *)>(0x40d2a0)(self, rect);
    if (!m(self, 0x104))
        return;
    if (mb(self, 0x100))
        fn<void(THISCALL *)(BYTE *)>(0x40a550)(self);
    for (DWORD i = 0; i < m(self, 0x104); i++)
        vc(bp(at<DWORD *>(m(self, 0x108))[i]), V_SET_SIZE, mi(self, W_RECT + 8) - mi(self, W_RECT),
           mi(self, W_RECT + 12) - mi(self, W_RECT + 4) - mi(self, 0x110));
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_c3_screen)
{
    const Hook list[] = {
#include "ui_c3_screen_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
