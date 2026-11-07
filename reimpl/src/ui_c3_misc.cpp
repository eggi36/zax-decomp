// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the smaller slots and callbacks of the game's own windows and of
// rendering objects outside the toolkit that no table listed: getters shared by many vtables, the image field's file
// slots, the application's music thunk, the button look's size, the text append callback of the font layout, the
// resource lists' record loader (slot 7), the texture list's loader (slot 37), the windowed browsable image, the
// popup menu, the server browser's keys, the menus' small slots, the end credits' timing, the polygon render
// type slots, the renderable polygon's vector deleting destructor and its slot 33 (vtable 0x5fe900; slot 33 added in
// phase 6 batch 6-0).
//
// Every call goes through its exe address (the original's side of a check runs the original all the way down).
#include "ui_screens.h"

namespace zax::ui {

namespace {

inline DWORD key_base(BYTE *self, DWORD k) { return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, k); }
inline DWORD frame_window_draw(BYTE *self, DWORD frame) { return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x402f30)(self, frame); }
inline DWORD frame_window_notify(BYTE *self, DWORD from, DWORD what)
{
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4035f0)(self, from, what);
}

}  // namespace

// ---- Getters and thunks -----------------------------------------------------------------------------------------

// CFrame slot 48 and eight other classes' slots: +0x28
DWORD THISCALL Shared_Field28(BYTE *self)   // 0x4a5970
{
    return m(self, 0x28);
}

// ZNameTree slot 5, the list source and the file name buffer: +4
DWORD THISCALL Shared_Field4(BYTE *self)   // 0x592370
{
    return m(self, 4);
}

// The image field's slot 26: (object, stream) the frame in the object (at the field's offset +0x10) written as .frm
DWORD THISCALL ImageField_Write(BYTE *self, DWORD object, DWORD stream)   // 0x4b5c20
{
    return zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4a9740)(m(self, 0x10) + object, stream);
}

// The image field's slot 29: (object, stream) the frame read
DWORD THISCALL ImageField_Read(BYTE *self, DWORD object, DWORD stream)   // 0x4b5c40
{
    return zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x4a9340)(m(self, 0x10) + object, stream);
}

// Slot 5 of the application, the world and CZaxApp: a jump to the music update (App_UpdateMusic 0x4ce510)
DWORD THISCALL App_Slot5(BYTE *self)   // 0x4ce3d0
{
    return zax::fn<DWORD(THISCALL *)(BYTE *)>(0x4ce510)(self);
}

// Slot 66 of the end credits' text window: a jump to the base's slot 66 (0x40f840)
DWORD THISCALL Credits_Slot66(BYTE *self)   // 0x5c0780
{
    return zax::fn<DWORD(THISCALL *)(BYTE *)>(0x40f840)(self);
}

// The renderable polygon's slot 25 (vtable 0x5fe900): (a, unused) the always-zero stub 0x53bbc0 with (a, 0)
DWORD THISCALL Polygon_Slot25(BYTE *self, DWORD a, DWORD)   // 0x55dd80
{
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x53bbc0)(self, a, 0);
}

// ---- The button look --------------------------------------------------------------------------------------------

// The text button look's slot 2 (vtable 0x5f88b4): the window (+4) sized to its title ("XXX" without one) in its font
// (slot 45) plus twice the border (+0x7a) and 2 each way (slot 59)
DWORD THISCALL ButtonLook_FitTitle(BYTE *self)   // 0x4e2590
{
    BYTE *w = bp(m(self, 4));
    const DWORD font = vc(w, 45);
    DWORD title = vc(w, 44) ? vc(bp(m(self, 4)), 44) : glob_addr(0x60a680);
    BYTE *win = bp(m(self, 4));
    const int border = (signed char)mb(win, W_BORDER3);
    const int lh = zax::fn<int(THISCALL *)(DWORD)>(0x4a6140)(font);
    const int h = lh + border * 2 + 2;
    const int tw = zax::fn<int(THISCALL *)(DWORD, DWORD, int)>(0x4a7310)(font, title, -1);
    return vc(bp(m(self, 4)), V_SET_SIZE, tw + border * 2 + 2, h);
}

// ---- Text, lists ------------------------------------------------------------------------------------------------

// The font layout's append callback (fastcall string, text; count): ZString_AppendTextN 0x4df440
DWORD __fastcall Text_AppendCallback(BYTE *z, const char *text, DWORD n)   // 0x506f60
{
    return zax::fn<DWORD(THISCALL *)(BYTE *, const char *, DWORD)>(0x4df440)(z, text, n);
}

// The resource lists' slot 7 (13 vtables): (i) record i's object, loaded on first use (slot 1 the name, Path_Join
// with the folder +0x38, PathName; slot 37 loads into the record: the folder part cut off at the separator, or the
// empty folder when the name is the whole path); index 0 when out of range; null without records or while the
// record is being loaded
DWORD THISCALL ResourceList_Record(BYTE *self, DWORD i)   // 0x598750
{
    char path[0x100];
    if (!m(self, 0x30))
        return 0;
    if (i >= m(self, 0x34))
        i = 0;
    const DWORD rec = i << 7;
    BYTE *flag = bp(m(self, 0x30) + rec + 0x7f);
    if (*flag)
        return 0;
    if (!m(bp(m(self, 0x30)), rec + 0x78) && !*flag) {
        *flag = 1;
        const DWORD name = vc(self, 1, i);
        zax::fn<void(__fastcall *)(char *, DWORD, DWORD)>(0x4a4220)(path, pv(self + 0x38), name);
        char *file = zax::fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        DWORD loaded;
        if (file == path) {
            loaded = vc(self, 37, glob_addr(EMPTY_TEXT), file, m(self, 0x30) + rec + 0x78);
        } else {
            file[-1] = 0;
            loaded = vc(self, 37, path, file, m(self, 0x30) + rec + 0x78);
        }
        m(bp(m(self, 0x30)), rec + 0x78) = loaded;
        mb(bp(m(self, 0x30)), rec + 0x7f) = 0;
        return m(bp(m(self, 0x30)), rec + 0x78);
    }
    return m(bp(m(self, 0x30)), rec + 0x78);
}

// The texture list's slot 37: (folder, name, out) the object made from the file folder/name.TGA
// (Object_CreateFromFile 0x4164c0)
DWORD THISCALL TextureList_Load(BYTE *, DWORD folder, DWORD name, DWORD out)   // 0x4b47a0
{
    BYTE ext[4], path[4];
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(ext);
    zax::fn<void(__cdecl *)(BYTE *, const char *, ...)>(0x4e09b0)(ext, glob<const char *>(0x60b0ac), glob<const char *>(0x613d8c));
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(path);
    zax::fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4e02c0)(path, folder, name, pv(c_str(ext)));
    const DWORD obj = zax::fn<DWORD(__fastcall *)(BYTE *, DWORD, DWORD, DWORD)>(0x4164c0)(path, 0, 1, out);
    zax::fn<void(THISCALL *)(BYTE *)>(0x4defd0)(path);
    zax::fn<void(THISCALL *)(BYTE *)>(0x4defd0)(ext);
    return obj;
}

// ---- The windowed browsable image (vtable 0x602c78; +0x100 the image, +0x104 its offset) ------------------------

// Slot 2: (dt) the image's slot 2
DWORD THISCALL BrowsableImage_Update(BYTE *self, DWORD dt)   // 0x599440
{
    if (BYTE *img = bp(m(self, 0x100)))
        return vc(img, 2, dt);
    return 0;
}

// Slot 4: (frame) the image's slot 1 at the offset
DWORD THISCALL BrowsableImage_Draw(BYTE *self, DWORD frame)   // 0x599460
{
    if (BYTE *img = bp(m(self, 0x100)))
        return vc(img, 1, frame, self + 0x104);
    return 0;
}

// Slot 10: (button, point) the base's (0x40bcc0); a left click it does not take invalidates (slot 38 with 0)
DWORD THISCALL BrowsableImage_MouseDown(BYTE *self, DWORD button, DWORD pt)   // 0x599480
{
    if (zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bcc0)(self, button, pt))
        return 1;
    if (button)
        return 0;
    vc(self, V_INVALIDATE, button);
    return 1;
}

// ---- The popup menu (vtable 0x5f86d0; +0x124 its items {.., +4 the item's window}, +0x128 their count, +0x134
// the listeners, +0x138 their count, +0x14c the item chosen) ----------------------------------------------------

// Slot 21, the notification: (from, what) a click (0) on an item's window chooses it (the first one) and closes the
// menu (slot 5); every listener whose window (slot 1) sent it gets slot 2 with what
void THISCALL PopupMenu_Notify(BYTE *self, DWORD from, DWORD what)   // 0x4de870
{
    if (!what) {
        for (DWORD i = 0; i < m(self, 0x128) && m(self, 0x14c) == 0xffffffff; i++) {
            if (from == m(bp(at<DWORD *>(m(self, 0x124))[i]), 4)) {
                m(self, 0x14c) = i;
                vc(self, V_CLOSE);
            }
        }
    }
    for (DWORD i = 0; i < m(self, 0x138); i++) {
        if (from == vc(bp(at<DWORD *>(m(self, 0x134))[i]), 1))
            vc(bp(at<DWORD *>(m(self, 0x134))[i]), 2, what);
    }
}

// Slot 16, the keys: the base's (0x40c170); Enter and Escape activate the first item's window (slot 18) when there
// is one
DWORD THISCALL PopupMenu_Key(BYTE *self, DWORD k)   // 0x4de900
{
    if (key_base(self, k))
        return 1;
    if ((k == 0xd || k == 0x1b) && m(self, 0x128) >= 1) {
        vc(bp(m(bp(at<DWORD *>(m(self, 0x124))[0]), 4)), V_ACTIVATE);
        return 1;
    }
    return 0;
}

// Slot 5, the close: the parent removes the menu (slot 33); true (al 1; the upper bytes the call's)
DWORD THISCALL PopupMenu_Close(BYTE *self)   // 0x4dea40
{
    return (vc(bp(m(self, W_PARENT)), 33, self) & 0xffffff00u) | 1;
}

// ---- Dialogs and menus ------------------------------------------------------------------------------------------

// The server browser's slot 16: Escape while a refresh runs (+0x195) presses the stop button (+0x140, slot 18);
// else the dialog's keys (0x403e40) as 0 or 1
DWORD THISCALL ServerBrowser_Key(BYTE *self, DWORD k)   // 0x519470
{
    if (k == 0x1b && mb(self, 0x195)) {
        vc(bp(m(self, 0x140)), V_ACTIVATE);
        return 1;
    }
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x403e40)(self, k) != 0;
}

// The save/load screen's slot 4: the frame window's drawing (0x402f30)
DWORD THISCALL SaveLoad_Draw(BYTE *self, DWORD frame)   // 0x564080
{
    return frame_window_draw(self, frame);
}

// "Select Map"'s slot 4 (and its variants): the children only while +0x134, else the frame window's drawing
DWORD THISCALL SelectMap_Draw(BYTE *self, DWORD frame)   // 0x5bdd80
{
    if (mb(self, 0x134))
        return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
    return frame_window_draw(self, frame);
}

// "Select Map"'s slot 60: no title bar (0) while +0x134, else the frame window's border height (0x403a80)
DWORD THISCALL SelectMap_BorderHeight(BYTE *self)   // 0x5bddb0
{
    if (mb(self, 0x134))
        return 0;
    return zax::fn<DWORD(THISCALL *)(BYTE *)>(0x403a80)(self);
}

// The end credits' slot 2: (dt) the time shown (+0x138, a float) grows by dt; the base's update (0x40b5d0)
DWORD THISCALL Credits_Update(BYTE *self, float dt)   // 0x5bf000
{
    m<float>(self, 0x138) = (float)((long double)dt + (long double)m<float>(self, 0x138));
    DWORD bits;
    memcpy(&bits, &dt, 4);
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b5d0)(self, bits);
}

// The end credits' slot 11: (button, point) after the minimum time (0x5f08b8) a release closes them (slot 5, 0);
// before it the base's (0x40bde0)
DWORD THISCALL Credits_MouseUp(BYTE *self, DWORD button, DWORD pt)   // 0x5bf020
{
    if (x87_le(m<float>(self, 0x138), *glob<float *>(0x5f08b8)))
        return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40bde0)(self, button, pt);
    vc(self, V_CLOSE);
    return 0;
}

// The end credits' slot 16: (key) the same with the base's keys (0x40c170)
DWORD THISCALL Credits_Key(BYTE *self, DWORD k)   // 0x5bf050
{
    if (x87_le(m<float>(self, 0x138), *glob<float *>(0x5f08b8)))
        return key_base(self, k);
    vc(self, V_CLOSE);
    return 0;
}

// The credits menu's slot 21: (from, what) a click on its button (+0x13c) closes the menu (slot 5); else the frame
// window's notification (0x4035f0)
DWORD THISCALL CreditsMenu_Notify(BYTE *self, DWORD from, DWORD what)   // 0x5c0970
{
    if (!what && from == m(self, 0x13c))
        return vc(self, V_CLOSE);
    return frame_window_notify(self, from, what);
}

// The "you died" menu's slot 21: a click on its button (+0x138) asks the world to restart (+0x312) and closes it
DWORD THISCALL DiedMenu_Notify(BYTE *self, DWORD from, DWORD what)   // 0x5c0a70
{
    if (!what && from == m(self, 0x138)) {
        mb(bp(gd(glob_addr(0x6c2080))), 0x312) = 1;
        return vc(self, V_CLOSE);
    }
    return frame_window_notify(self, from, what);
}

// ---- The renderable polygon (vtable 0x5fe900, 0x9c bytes: a CFreeRangePoly with +0x78 the render type, +0x7c the
// texture, +0x8c an object, +0x94 the animation time, +0x98 the frame) -------------------------------------------

// The vector deleting destructor: (flags) bit 1 an array (count at -4, from the last element), each element's
// fields freed (0x4173f0), its object deleted (slot 6 with 1) and CFreeRangePoly's destructor (0x4b5fd0); bit 0
// the memory (operator delete of the array, the pool for one)
BYTE *THISCALL Polygon_VectorDeletingDtor(BYTE *self, DWORD flags)   // 0x55d370
{
    auto one = [](BYTE *e) {
        m(e, 0) = vt(0x5fe900);
        zax::fn<void(THISCALL *)(BYTE *)>(0x4173f0)(e);
        if (BYTE *o = bp(m(e, 0x8c))) {
            vc(o, 6, 1);
            m(e, 0x8c) = 0;
        }
        zax::fn<void(THISCALL *)(BYTE *)>(0x4b5fd0)(e);
    };
    if ((BYTE)flags & 2) {
        BYTE *base = self - 4;
        const DWORD n = m(base, 0);
        BYTE *e = self + n * 0x9c;
        for (DWORD k = (int)(n - 1) < 0 ? 0 : n; k != 0; k--) {
            e -= 0x9c;
            one(e);
        }
        if (flags & 1)
            zax::fn<void(__cdecl *)(void *)>(0x5d0320)(base);
        return base;
    }
    one(self);
    if (flags & 1)
        pool_free(self, 0x9c);
    return self;
}

// Slot 4: (unused) the render types 0-5 slot 58, 6 and 7 (animated) slot 59, each with 0x200000 (eax the caller's
// for others)
DWORD THISCALL Polygon_UpdateFlags(BYTE *self, DWORD)   // 0x55d9a0
{
    switch (m(self, 0x78)) {
    case 0: case 1: case 2: case 3: case 4: case 5:
        return vc(self, 58, 0x200000);
    case 6: case 7:
        return vc(self, 59, 0x200000);
    default:
        return 0;
    }
}

// Slot 33 (phase 6 batch 6-0; no table had it): the same as slot 4 without its argument; for a render type above 7
// eax is the type, as the original leaves it after its compare
DWORD THISCALL Polygon_Slot33(BYTE *self)   // 0x55da60
{
    const DWORD type = m(self, 0x78);
    switch (type) {
    case 0: case 1: case 2: case 3: case 4: case 5:
        return vc(self, 58, 0x200000);
    case 6: case 7:
        return vc(self, 59, 0x200000);
    default:
        return type;
    }
}

// The render type field's set callback: (type) stored (+0x78), the flags as in slot 4, the layer told about the
// rectangle (0x4fa8c0), the animation time reset, slot 35 with 0
DWORD THISCALL Polygon_SetRenderType(BYTE *self, DWORD type)   // 0x55d9f0
{
    m(self, 0x78) = type;
    switch (type) {
    case 0: case 1: case 2: case 3: case 4: case 5:
        vc(self, 58, 0x200000);
        break;
    case 6: case 7:
        vc(self, 59, 0x200000);
        break;
    default:
        break;
    }
    zax::fn<void(THISCALL *)(BYTE *)>(0x4fa8c0)(self);
    m(self, 0x94) = 0;
    return vc(self, 35, 0);
}


// ---- The controls dialog's key rows (vtable 0x5effb0, a button: +0x138 the label showing the binding, +0x13c the
// control configuration, +0x140 the binding, +0x144 waiting for a key) ----------------------------------------------

namespace {
// The label shows the binding's text (ControlConfig::BindingText 0x470b60), or nothing without a configuration
void show_binding(BYTE *self)
{
    if (!m(self, 0x13c)) {
        vc(bp(m(self, 0x138)), 77, glob_addr(EMPTY_TEXT));
        return;
    }
    BYTE z[4];
    zax::fn<void(THISCALL *)(BYTE *)>(0x4dec90)(z);
    zax::fn<void(THISCALL *)(DWORD, DWORD, BYTE *)>(0x470b60)(m(self, 0x13c), m(self, 0x140), z);
    vc(bp(m(self, 0x138)), 77, c_str(z));
    zax::fn<void(THISCALL *)(BYTE *)>(0x4defd0)(z);
}
}  // namespace

// Slot 16: the button's keys (0x40fd80); Delete (0x11) and Backspace (8) reset the binding to none
// (KeyBinding_SetDefault 0x46f4c0) and show it; 1 when handled
DWORD THISCALL ControlRow_Key(BYTE *self, DWORD k)   // 0x471160
{
    if (zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40fd80)(self, k))
        return 1;
    if (k != 0x11 && k != 8)
        return 0;
    zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x46f4c0)(m(self, 0x13c), m(self, 0x140), 0, 0, 0, 0);
    show_binding(self);
    return 1;
}

// Slot 86, the update while waiting for a key: when the configuration took one (ControlConfig::Capture 0x46f680),
// the screen's cursor shown again (0x409870), the waiting ended (slot 78 with 0), the binding shown and, unless the
// capture says so, the row invalidated (slot 38 with 1); else the binding shown
void THISCALL ControlRow_Update(BYTE *self)   // 0x471220
{
    if (!mb(self, 0x144))
        return;
    BYTE keep = 0;
    if (!zax::fn<bool(THISCALL *)(DWORD, DWORD, BYTE *)>(0x46f680)(m(self, 0x13c), m(self, 0x140), &keep)) {
        show_binding(self);
        return;
    }
    if (BYTE *scr = zax::fn<BYTE *(THISCALL *)(BYTE *)>(0x40ccd0)(self))
        zax::fn<void(THISCALL *)(BYTE *)>(0x409870)(scr);
    mb(self, 0x144) = 0;
    vc(self, 78, 0);
    show_binding(self);
    if (!keep)
        vc(self, V_INVALIDATE, 1);
}

// The popup menu's slot 57, the rectangle: the frame window's (0x403640); with a title (+0x11c) the title label
// (+0x118) as wide as the menu inside its borders, the items' windows in a row below it (or below the last
// listener's window), the first centred by the items' width (+0x140), each next one right of the one before
void THISCALL PopupMenu_SetRect(BYTE *self, int *rect)   // 0x4de950
{
    zax::fn<void(THISCALL *)(BYTE *, int *)>(0x403640)(self, rect);
    if (!mb(self, 0x11c))
        return;
    BYTE *t = bp(m(self, 0x118));
    vc(t, V_SET_SIZE, mi(self, W_RECT + 8) - (signed char)mb(self, W_BORDER) * 2 - mi(self, W_RECT), mi(t, 0x10) - mi(t, 8));
    const int x0 = (mi(self, W_RECT + 8) - mi(self, 0x140) - mi(self, W_RECT)) / 2;
    t = bp(m(self, 0x118));
    const int gap = (signed char)mb(self, W_BORDER2);
    int y = mi(t, 0x10) + gap;
    if (const DWORD n = m(self, 0x138)) {
        BYTE *e = bp(at<DWORD *>(m(self, 0x134))[n - 1]);
        y = mi(bp(m(e, 4)), 8) + (mi(t, 0x10) - mi(t, 8)) + gap;
    }
    for (DWORD i = 0; i < m(self, 0x128); i++) {
        int x = x0;
        if (i) {
            BYTE *pw = bp(m(bp(at<DWORD *>(m(self, 0x124))[i - 1]), 4));
            x = mi(pw, 0xc) + (signed char)mb(self, W_BORDER2);
        }
        zax::fn<void(THISCALL *)(DWORD, int, int)>(0x40d630)(m(bp(at<DWORD *>(m(self, 0x124))[i]), 4), x, y);
    }
}

// The text look's slot 1, the drawing (vtable 0x5f88d4; +4 the button, +8 the style: +4 the font, +8 the shadow
// font, +0xc the highlight font, +0x10 the warp value): (frame) the title centred, its shadow 2 pixels down and
// right (less when pressed: 1 for a pressed or checked button, -1 when only hovered), then the text in the font;
// a hovered button's text through the highlight font with the simple warp blend mode 0x6c1a78 (made on first use
// with its 8- and 16-bit blitters) for the time of the call
void THISCALL TextLook_Draw(BYTE *self, DWORD frame)   // 0x4e2be0
{
    int off = 0;
    BYTE *w = bp(m(self, 4));
    const DWORD font = m(bp(m(self, 8)), 4);
    if ((mb(w, 0x100) && mb(w, 0x101)) || mb(w, 0x103) || vc(w, 79))
        off = 1;
    else if (mb(bp(m(self, 4)), 0x101))
        off = -1;
    const int shadow = 2 - off;
    w = bp(m(self, 4));
    const DWORD tw = zax::fn<DWORD(THISCALL *)(DWORD, DWORD, int)>(0x4a7310)(font, vc(w, 44), -1);
    const DWORD x = (DWORD)(mi(w, W_RECT + 8) - mi(w, W_RECT) - (int)tw) >> 1;
    BYTE *w2 = bp(m(self, 4));
    const DWORD y = (DWORD)(mi(w2, W_RECT + 12) - mi(bp(font), 0x80) - mi(w2, W_RECT + 4)) >> 1;
    const DWORD shadow_font = m(bp(m(self, 8)), 8);
    using Draw = void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    zax::fn<Draw>(0x4a6470)(shadow_font, frame, x + shadow, y + shadow, vc(w2, 44), 0, 0);
    if (!mb(bp(m(self, 4)), 0x101)) {
        zax::fn<Draw>(0x4a6470)(font, frame, x + off, y + off, vc(bp(m(self, 4)), 44), 0, 0);
        return;
    }
    const DWORD saved = m(bp(m(bp(m(self, 8)), 0xc)), 0x38);
    if (!(gb(glob_addr(0x6c1aac)) & 1)) {
        gb(glob_addr(0x6c1aac)) |= 1;
        zax::fn<void(THISCALL *)(DWORD)>(0x432d60)(glob_addr(0x6c1a78));
        gd(glob_addr(0x6c1a78)) = vt(0x5f88e0);
        using Reg = void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
        zax::fn<Reg>(0x432de0)(glob_addr(0x6c1a78), 8, 2, 8, 0, fn_addr(0x4e2e50));
        const bool mmx = zax::fn<bool(__cdecl *)()>(0x5156b0)();
        zax::fn<Reg>(0x432de0)(glob_addr(0x6c1a78), 0x10, 4, 0x10, 0, mmx ? fn_addr(0x4e2f10) : fn_addr(0x4e32c0));
        crt_atexit(code<void(__cdecl *)()>(0x4e2de0));
    }
    m(bp(m(bp(m(self, 8)), 0xc)), 0x38) = glob_addr(0x6c1a78);
    gd(glob_addr(0x6c1ab0)) = m(bp(m(self, 8)), 0x10);
    const DWORD hfont = m(bp(m(self, 8)), 0xc);
    zax::fn<Draw>(0x4a6470)(hfont, frame, x + off, y + off, vc(bp(m(self, 4)), 44), 0, 0);
    m(bp(m(bp(m(self, 8)), 0xc)), 0x38) = saved;
}

// The simple warp mode's 8-bit blitter (registered by the text look): (fastcall source, source rect; destination,
// destination rect, mode) like the 16-bit ones (blit_rest4b.cpp): a CSimpleWarpEffect on the stack (+8 1.0, +0x2c
// the warp value 0x6c1ab0) bends the source (a CFrame) while it is drawn through the 8-bit RLE copy, whole
// (0x43e380) or clipped (0x43dbe0), with the mode's table (+0x24)
void __fastcall SimpleWarp_Blit8(BYTE *src, const int *sr, DWORD dst, DWORD dr, BYTE *mode)   // 0x4e2e50
{
    alignas(4) BYTE warp[0x30];
    zax::fn<void *(THISCALL *)(void *)>(0x571b70)(warp);
    const DWORD value = gd(glob_addr(0x6c1ab0));
    m(warp, 8) = 0x3f800000;
    m(warp, 0x2c) = value;
    BYTE *frame = nullptr;
    if (src) {
        const DWORD ci = zax::fn<DWORD(__cdecl *)()>(0x4a8760)();
        if (zax::fn<bool(THISCALL *)(BYTE *, DWORD)>(0x416790)(src, ci))
            frame = src;
    }
    zax::fn<void(THISCALL *)(void *, void *)>(0x572020)(warp, frame);
    zax::fn<void(THISCALL *)(void *, void *)>(0x572300)(warp, frame);
    using Rle = void(__fastcall *)(BYTE *, const int *, DWORD, DWORD, DWORD, DWORD);
    if (sr[0] != 0 || (DWORD)sr[2] != vc(src, 45))
        zax::fn<Rle>(0x43dbe0)(src, sr, dst, dr, m(mode, 0x24), 0);
    else
        zax::fn<Rle>(0x43e380)(src, sr, dst, dr, m(mode, 0x24), 0);
    zax::fn<void(THISCALL *)(void *, void *)>(0x572680)(warp, frame);
    zax::fn<void(THISCALL *)(void *)>(0x571bb0)(warp);
}

// The model preview's drawing callback (vtable 0x5fae90 slot 1): (frame, point) the model (+0xc) fills two draw
// queues (function-local statics 0x6cff18 and 0x6cff28, made on first use) through its slot 23 at the point, the
// queues sorted and every item drawn (slot 1) through a clip frame on the target without a limit; the queues
// emptied before and after
void THISCALL ModelPreview_Draw(BYTE *self, DWORD frame, const int *pt)   // 0x517c60
{
    int r[4] = {-9999, -9999, 9999, 9999};
    int p[3] = {pt[0], pt[1], 0};
    if (!(gb(glob_addr(0x6cff38)) & 1)) {
        gb(glob_addr(0x6cff38)) |= 1;
        zax::fn<void(THISCALL *)(DWORD)>(0x49c9d0)(glob_addr(0x6cff18));
        crt_atexit(code<void(__cdecl *)()>(0x517e60));
    }
    if (!(gb(glob_addr(0x6cff38)) & 2)) {
        gb(glob_addr(0x6cff38)) |= 2;
        zax::fn<void(THISCALL *)(DWORD)>(0x49c9d0)(glob_addr(0x6cff28));
        crt_atexit(code<void(__cdecl *)()>(0x517e50));
    }
    zax::fn<void(THISCALL *)(DWORD)>(0x49ccb0)(glob_addr(0x6cff18));
    zax::fn<void(THISCALL *)(DWORD)>(0x49ccb0)(glob_addr(0x6cff28));
    vc(bp(m(self, 0xc)), 23, glob_addr(0x6cff18), glob_addr(0x6cff28), r, 1, p);
    const DWORD QUEUES[2] = {glob_addr(0x6cff18), glob_addr(0x6cff28)};
    for (DWORD q : QUEUES) {
        zax::fn<void(THISCALL *)(DWORD)>(0x49ca40)(q);
        const DWORD n = zax::fn<DWORD(THISCALL *)(DWORD)>(0x482de0)(q);
        for (DWORD i = 0; i < n; i++) {
            BYTE *item = zax::fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x566e40)(q, i);
            alignas(4) BYTE cf[0x1c];
            zax::fn<void(THISCALL *)(BYTE *)>(0x46d790)(cf);
            zax::fn<void(THISCALL *)(BYTE *, DWORD, int *)>(0x46dc00)(cf, frame, r);
            vc(item, 1, cf);
            m(cf, 0) = vt(0x5eb62c);
            zax::fn<void(THISCALL *)(BYTE *)>(0x4173f0)(cf);
            m(cf, 0) = vt(0x5eb534);
            zax::fn<void(THISCALL *)(BYTE *)>(0x4173f0)(cf);
            zax::fn<void(THISCALL *)(BYTE *)>(0x4166d0)(cf);
        }
    }
    zax::fn<void(THISCALL *)(DWORD)>(0x49ccb0)(glob_addr(0x6cff18));
    zax::fn<void(THISCALL *)(DWORD)>(0x49ccb0)(glob_addr(0x6cff28));
}

// The server browser's slot 21, the notification: (from, what) for a click: the refresh button (+0x13c) refreshes
// unless a refresh runs (0x519cf0), the stop button (+0x140) stops it (0x5c7850 with +0x190; the buttons' enabled
// bytes), the server dialog button (+0x168) hands over to a new server dialog (0x4d3080, 0x1a4 bytes), the
// options button (+0x16c) opens the network options (0x46d010, 0x15c bytes) modally, the network type list
// (+0x14c) saves the type ("Prefered Network Type" of "Net Game"), stops a running refresh and refreshes, the close
// button (+0x170) closes (slot 5), the join button (+0x164) and the server list (+0x138) join (0x519730); a
// selection in the server list (what 1) enables the join button for a server with room (numplayers below
// maxplayers or no maximum); the rest the frame window's (0x4035f0)
DWORD THISCALL ServerBrowser_Notify(BYTE *self, DWORD from, DWORD what)   // 0x5194e0
{
    auto stop = [](BYTE *s) {
        zax::fn<void(__cdecl *)(DWORD)>(0x5c7850)(m(s, 0x190));
        mb(s, 0x195) = 0;
        mb(bp(m(s, 0x13c)), W_ENABLED) = 1;
        mb(bp(m(s, 0x140)), W_ENABLED) = mb(s, 0x195);
    };
    using Refresh = DWORD(THISCALL *)(BYTE *);
    if (!what) {
        if (from == m(self, 0x13c)) {
            if (mb(self, 0x195))
                return from;
            return zax::fn<Refresh>(0x519cf0)(self);
        }
        if (from == m(self, 0x140)) {
            if (!mb(self, 0x195))
                return from;
            stop(self);
            return 0;
        }
        if (from == m(self, 0x168)) {
            BYTE *d = static_cast<BYTE *>(pool_alloc(0x1a4));
            const DWORD dlg = d ? zax::fn<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4d3080)(d, self, 0) : 0;
            return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x404000)(self, dlg, 1);
        }
        if (from == m(self, 0x16c)) {
            BYTE *d = static_cast<BYTE *>(pool_alloc(0x15c));
            const DWORD dlg = d ? zax::fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x46d010)(d, self) : 0;
            return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40c6e0)(self, dlg, 6);
        }
        if (from == m(self, 0x14c)) {
            const DWORD sel = vc(bp(m(self, 0x14c)), 76);
            const int v = zax::fn<int(THISCALL *)(BYTE *, DWORD)>(0x592310)(self + 0x150, sel);
            zax::fn<void(__fastcall *)(DWORD, const char *, const char *, int)>(0x4a4690)(glob_addr(0x6bd330), glob<const char *>(0x60fa48),
                                                                                  glob<const char *>(0x61be28), v);
            mb(self, 0x194) = v == 1;
            if (mb(self, 0x195)) {
                stop(self);
                if (mb(self, 0x195))
                    return 0;
            }
            return zax::fn<Refresh>(0x519cf0)(self);
        }
        if (from == m(self, 0x170))
            return vc(self, V_CLOSE);
        if (from == m(self, 0x164) || from == m(self, 0x138))
            return zax::fn<Refresh>(0x519730)(self);
    } else if (what == 1 && from == m(self, 0x138)) {
        const DWORD i = vc(bp(m(self, 0x138)), 77);
        BYTE room = 0;
        if (i != 0xffffffff && zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x592310)(self + 0x17c, i) != 0xffffffff) {
            const DWORD server = zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x592310)(self + 0x17c, i);
            using Get = int(__cdecl *)(DWORD, const char *, int);
            const int players = zax::fn<Get>(0x5c8130)(server, glob<const char *>(0x61158c), 0);
            const int max = zax::fn<Get>(0x5c8130)(server, glob<const char *>(0x611580), 0);
            if (players < max || !max)
                room = 1;
        }
        mb(bp(m(self, 0x164)), W_ENABLED) = room;
        return 0;
    }
    return frame_window_notify(self, from, what);
}

namespace {
// _CIfmod 0x5d260a: st(1) modulo st(0), both popped
long double ci_fmod(long double x, long double y)
{
    long double r;
    const DWORD target = fn_addr(0x5d260a);
    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "m"(target) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}
}  // namespace

// The renderable polygon's slot 35, the animation: (dt) for an animated texture (render type 6) the sequence of
// the animated texture list 0x6c1ee8 (record +0x7c, loaded on first use) advances the time (+0x94) by its rate
// (slot 26) times dt, wrapped by its frame count (slot 25, _CIfmod), the frame (+0x98) truncated from it
void THISCALL Polygon_Animate(BYTE *self, float dt)   // 0x55dab0
{
    if (m(self, 0x78) != 6)
        return;
    constexpr DWORD LIST = 0x6c1ee8;
    BYTE *seq = nullptr;
    if (DWORD recs = gd(glob_addr(LIST) + 0x30)) {
        DWORD i = m(self, 0x7c);
        if (i >= gd(glob_addr(LIST) + 0x34))
            i = 0;
        const DWORD rec = i << 7;
        if (!gb(recs + rec + 0x7f)) {
            if (!m(bp(recs), rec + 0x78) && recs && !gb(recs + rec + 0x7f)) {
                gb(recs + rec + 0x7f) = 1;
                char path[0x100];
                const DWORD name = vc(glob<void *>(LIST), 1, i);
                zax::fn<void(__fastcall *)(char *, DWORD, DWORD)>(0x4a4220)(path, glob_addr(LIST) + 0x38, name);
                char *file = zax::fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
                DWORD loaded;
                if (file == path) {
                    loaded = vc(glob<void *>(LIST), 37, glob_addr(EMPTY_TEXT), file, gd(glob_addr(LIST) + 0x30) + rec + 0x78);
                } else {
                    file[-1] = 0;
                    loaded = vc(glob<void *>(LIST), 37, path, file, gd(glob_addr(LIST) + 0x30) + rec + 0x78);
                }
                m(bp(gd(glob_addr(LIST) + 0x30)), rec + 0x78) = loaded;
                gb(gd(glob_addr(LIST) + 0x30) + rec + 0x7f) = 0;
                recs = gd(glob_addr(LIST) + 0x30);
            }
            seq = bp(m(bp(recs), rec + 0x78));
        }
    }
    const long double rate = vc<long double>(seq, 26);
    m<float>(self, 0x94) = (float)(rate * (long double)dt + (long double)m<float>(self, 0x94));
    const DWORD n = vc(seq, 25);
    if (!((long double)(unsigned long long)n > (long double)m<float>(self, 0x94)))   // (fcomp; test ah, 0x41: NaN wraps too)
        m<float>(self, 0x94) = (float)ci_fmod(m<float>(self, 0x94), (long double)(unsigned long long)n);
    m(self, 0x98) = (DWORD)x87_ftol(m<float>(self, 0x94));
}

// The renderable polygon's slot 29, the drawing: (frame) with flag 0x400000 a line along its bottom offset by +0x74
// (its rectangle by slot 37 with 3, 0; a static FieldColor 0x7130d0 (0x1c, 0xff, 0x64) made on first use), then
// CFreeRangePoly's (0x4b5ff0)
DWORD THISCALL Polygon_Draw(BYTE *self, BYTE *frame)   // 0x55dcd0
{
    if (m(self, 0x1c) & 0x400000) {
        int r[4];
        vc(self, 37, r, 3, 0);
        if (!(gb(glob_addr(0x7130c8)) & 1)) {
            gb(glob_addr(0x7130c8)) |= 1;
            zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob_addr(0x7130d0), 0x1c, 0xff, 0x64, 0xff);
            crt_atexit(code<void(__cdecl *)()>(0x55dd70));
        }
        const int y = mi(self, 0x74) + r[3];
        vc(frame, 51, r[0], y, r[2], y, glob_addr(0x7130d0));
    }
    return zax::fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x4b5ff0)(self, frame);
}

// ---- The save/load screen (vtable 0x5fef48) and its details page (vtable 0x5ff098) --------------------------------

// The save/load screen's slot 57, the rectangle: the frame window's (0x403640); when laid out (+0x1a0) its parts
// follow the change of size: the list's header (+0x134, CJHeader::SetWidth), the column title window (+0x138, as wide
// as the header) and its first two columns' marker (+0x13c, slot 57), the list (+0x144, slot 59), the buttons
// (+0x140, +0x148 moved left), the page area (+0x14c, moved and resized; its pages (the null-terminated +0x160, pointers to
// the pages' window pointers) sized to it, slot 59), the three buttons below (+0x178, +0x17c, +0x174; a quarter of the width each); the details shown
// again (0x564090) when a page's client width changed or the height did
void THISCALL SaveLoad_SetRect(BYTE *self, int *rect)   // 0x563d40
{
    const int ol = mi(self, W_RECT), ot = mi(self, W_RECT + 4), orr = mi(self, W_RECT + 8), ob = mi(self, W_RECT + 12);
    zax::fn<void(THISCALL *)(BYTE *, int *)>(0x403640)(self, rect);
    if (!mb(self, 0x1a0))
        return;
    const int dw = rect[0] - rect[2] - ol + orr;
    const int dh = rect[1] - rect[3] - ot + ob;
    using SetW = void(THISCALL *)(BYTE *, int);
    BYTE *hdr = bp(m(self, 0x134));
    zax::fn<void(THISCALL *)(BYTE *, int)>(0x402a10)(hdr, mi(hdr, 0xc) - mi(hdr, 4) - dw);
    if (BYTE *t = bp(m(self, 0x138))) {
        BYTE *h2 = bp(m(self, 0x134));
        zax::fn<SetW>(0x40f5b0)(t, mi(h2, 0xc) - mi(h2, 4));
        BYTE *t2 = bp(m(self, 0x138));
        int r[4] = {0, 0, 0, mi(t2, 0x10) - mi(t2, 8)};
        r[0] = zax::fn<int(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(bp(m(self, 0x134)), 0) + r[0];
        r[2] = zax::fn<int(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(bp(m(self, 0x134)), 1) + r[0];
        vc(bp(m(self, 0x13c)), V_SET_RECT, r);
    }
    BYTE *l = bp(m(self, 0x144));
    vc(l, V_SET_SIZE, mi(l, 0xc) - mi(l, 4) - dw, mi(l, 0x10) - mi(l, 8) - dh);
    if (BYTE *b = bp(m(self, 0x140)))
        zax::fn<SetW>(0x40d680)(b, mi(b, 4) - dw);
    zax::fn<SetW>(0x40d680)(bp(m(self, 0x148)), mi(bp(m(self, 0x148)), 4) - dw);
    int widths[4];
    {
        int *dst = widths;
        for (DWORD *p = reinterpret_cast<DWORD *>(self + 0x160); *p; p++)
            *dst++ = zax::fn<int(THISCALL *)(BYTE *)>(0x40d490)(bp(m(bp(*p), 0)));
    }
    zax::fn<SetW>(0x40d680)(bp(m(self, 0x14c)), mi(bp(m(self, 0x148)), 4) - dw);
    BYTE *area = bp(m(self, 0x14c));
    zax::fn<SetW>(0x40f5d0)(area, mi(area, 0x10) - mi(area, 8) - dh);
    bool changed = false;
    {
        int *w = widths;
        for (DWORD *p = reinterpret_cast<DWORD *>(self + 0x160); *p; p++, w++) {
            BYTE *a = bp(m(self, 0x14c));
            vc(bp(m(bp(*p), 0)), V_SET_SIZE, mi(a, 0xc) - mi(a, 4), mi(a, 0x10) - mi(a, 0x110) - mi(a, 8));
            changed |= *w != zax::fn<int(THISCALL *)(BYTE *)>(0x40d490)(bp(m(bp(*p), 0)));
        }
    }
    const int b78 = (signed char)mb(self, W_BORDER), b79 = (signed char)mb(self, W_BORDER2);
    zax::fn<SetW>(0x40f5b0)(bp(m(self, 0x178)), (mi(self, W_RECT + 8) - (b78 + b79 * 2) * 2 - mi(self, W_RECT)) / 4);
    BYTE *b1 = bp(m(self, 0x178));
    move_to(b1, (mi(b1, 0xc) - mi(b1, 4)) * 3 / 2 + b79 * 2 + b78, mi(b1, 8) - dh);
    b1 = bp(m(self, 0x178));
    zax::fn<SetW>(0x40f5b0)(bp(m(self, 0x17c)), mi(b1, 0xc) - mi(b1, 4));
    BYTE *b2 = bp(m(self, 0x17c));
    b1 = bp(m(self, 0x178));
    move_to(b2, (mi(b1, 0xc) - mi(b1, 4)) * 5 / 2 + b79 * 3 + b78, mi(b2, 8) - dh);
    if (BYTE *b3 = bp(m(self, 0x174))) {
        b1 = bp(m(self, 0x178));
        zax::fn<SetW>(0x40f5b0)(b3, mi(b1, 0xc) - mi(b1, 4));
        b3 = bp(m(self, 0x174));
        b1 = bp(m(self, 0x178));
        move_to(b3, (mi(b1, 0xc) - mi(b1, 4)) / 2 + b78 + b79, mi(b3, 8) - dh);
    }
    if (changed || dh)
        zax::fn<void(THISCALL *)(BYTE *)>(0x564090)(self);
}

// The details page's slot 4: (frame) the mission objectives (+0x100, MissionObjectives_Draw 0x509910) drawn inside
// the borders in the save/load options of the custom rules (CZaxCustomRules::LoadSaveScreenOptions 0x5a2190: +8 the
// colour; the rules of the application's world +0x2e0 when they are CZaxCustomRules), the content size set to their
// height and the client width; the children
DWORD THISCALL SaveLoadDetails_Draw(BYTE *self, DWORD frame)   // 0x564b90
{
    BYTE *world = bp(m(bp(gd(glob_addr(G_APP))), 0x2e0));
    BYTE *rules = nullptr;
    if (m(world, 0x38)) {
        const DWORD ci = zax::fn<DWORD(__cdecl *)()>(0x5a1350)();
        if (zax::fn<bool(THISCALL *)(BYTE *, DWORD)>(0x416790)(bp(m(world, 0x38)), ci))
            rules = bp(m(world, 0x38));
    }
    BYTE *opts = zax::fn<BYTE *(THISCALL *)(BYTE *)>(0x5a2190)(rules);
    const DWORD color = m(opts, 8);
    const int border = (signed char)mb(self, W_BORDER);
    const int w = zax::fn<int(THISCALL *)(BYTE *)>(0x40d490)(self) - border * 2;
    const int t = vc<int>(self, V_BORDER_H) + border;
    const int l = vc<int>(self, V_BORDER_W) + (signed char)mb(self, W_BORDER);
    using Draw = int(__fastcall *)(DWORD, DWORD, int, int, int, DWORD, DWORD, DWORD, DWORD);
    const int h = zax::fn<Draw>(0x509910)(m(self, 0x100), frame, l, t, w, 0x3f800000, 0, 1, color);
    BYTE *s = bp(m(self, W_SCROLL));
    using Size = void(THISCALL *)(BYTE *, int, int);
    zax::fn<Size>(0x40d550)(self, s ? mi(s, S_W) : 0, h + (signed char)mb(self, W_BORDER) * 2);
    const int cw = zax::fn<int(THISCALL *)(BYTE *)>(0x40d490)(self);
    s = bp(m(self, W_SCROLL));
    zax::fn<Size>(0x40d550)(self, cw, s ? mi(s, S_H) : 0);
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}

// ---- qsort comparators ------------------------------------------------------------------------------------------

namespace {
// A record of a definition list by index, loaded on first use (DefinitionList_LoadRecord 0x55e090); null without
// records or while loading
DWORD def_record(DWORD list, DWORD i)
{
    DWORD recs = gd(list + 0x30);
    if (!recs)
        return 0;
    if (i >= gd(list + 0x34))
        i = 0;
    const DWORD rec = i << 7;
    if (gb(recs + rec + 0x7f))
        return 0;
    if (!m(bp(recs), rec + 0x78)) {
        zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(list, i);
        recs = gd(list + 0x30);
    }
    return m(bp(recs), rec + 0x78);
}
}  // namespace

// The item categories' order (qsort with category indices): by their inventory type definitions' order (+0x1c,
// the second's minus the first's) when both entries have an object and both are CZaxInventoryTypeDefinition, else 0
int __cdecl CategoryOrder_Compare(const DWORD *a, const DWORD *b)   // 0x5b29a0
{
    constexpr DWORD LIST = 0x6c0800;
    const DWORD ia = *a, ib = *b;
    const DWORD recs = gd(glob_addr(LIST) + 0x30);
    if (!m(bp(recs), (ia << 7) + 0x78))
        return 0;
    if (!m(bp(recs), (ib << 7) + 0x78))
        return 0;
    BYTE *da = bp(def_record(glob_addr(LIST), ia));
    BYTE *db = bp(def_record(glob_addr(LIST), ib));
    using Kind = bool(THISCALL *)(BYTE *, DWORD);
    if (da && !zax::fn<Kind>(0x416790)(da, zax::fn<DWORD(__cdecl *)()>(0x5b9400)()))
        da = nullptr;
    if (db && !zax::fn<Kind>(0x416790)(db, zax::fn<DWORD(__cdecl *)()>(0x5b9400)()))
        db = nullptr;
    if (da && db)
        return mi(db, 0x1c) - mi(da, 0x1c);
    return 0;
}

// The difficulties' order (qsort with indices of the difficulty list 0x6c2a00): the second's value (+0x10, a float)
// minus the first's, times 0x5eb2cc, truncated (the first loaded through 0x55e090, the second inline)
int __cdecl DifficultyOrder_Compare(const DWORD *a, const DWORD *b)   // 0x5bf690
{
    BYTE *da = bp(def_record(glob_addr(0x6c2a00), *a));
    BYTE *db = bp(record_inline(glob_addr(0x6c2a00), *b));
    return x87_ftol(((long double)m<float>(db, 0x10) - (long double)m<float>(da, 0x10)) * (long double)*glob<float *>(0x5eb2cc));
}

// ---- The server browser's server list callback ------------------------------------------------------------------

namespace {
// The status line (+0x148, slot 43): the state text (+0x198) with the progress (+0x19c) unless 0 or 100
void browser_status(BYTE *self)
{
    BYTE z[4];
    zs_copy(z, pv(self + 0x198));
    if (const DWORD p = m(self, 0x19c); p && p != 100)
        zax::fn<void(__cdecl *)(BYTE *, const char *, ...)>(0x4e0a30)(z, glob<const char *>(0x61be74), p);
    vc(bp(m(self, 0x148)), V_SET_TITLE, c_str(z));
    zs_dtor(z);
}
}  // namespace

// The GameSpy server list's callback (cdecl list, message, browser, server, progress): a new server (2) is added to
// the list (0x519b60) and the status shows a changed progress; a state change (1, the list's state 0x5c9cc0) sets
// the state text: done (0: the refresh ended, the buttons' enabled bytes, the counter +0x174 reset, a lone
// "none" entry removed from the server names (+0x17c, slots 22 and 17), "no servers" (0x6cffcc) added when empty,
// the text 0x6cffdc), listing (1: 0x6cffb0), querying (2: 0x6cffac) or 3 (0x6cffe0)
void __cdecl ServerList_Callback(DWORD list, DWORD msg, BYTE *self, DWORD server, DWORD progress)   // 0x519870
{
    if (msg == 2) {
        zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x519b60)(self, server);
        if (progress == m(self, 0x19c))
            return;
        m(self, 0x19c) = progress;
        browser_status(self);
        return;
    }
    if (msg != 1)
        return;
    const DWORD state = zax::fn<DWORD(__cdecl *)(DWORD)>(0x5c9cc0)(list);
    if (state > 3)
        return;
    DWORD text;
    if (state == 0) {
        mb(self, 0x195) = 0;
        mb(bp(m(self, 0x13c)), W_ENABLED) = 1;
        mb(bp(m(self, 0x140)), W_ENABLED) = mb(self, 0x195);
        BYTE *names = self + 0x17c;
        m(self, 0x174) = 0;
        if (vc(names, 22) == 1 && zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x592310)(names, 0) == 0xffffffff)
            vc(names, 17);
        if (!vc(names, 22))
            zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x519ea0)(self, glob_addr(0x6cffcc));
        text = glob_addr(0x6cffdc);
    } else {
        text = state == 1 ? glob_addr(0x6cffb0) : state == 2 ? glob_addr(0x6cffac) : glob_addr(0x6cffe0);
    }
    zs_assign(self + 0x198, text);
    browser_status(self);
}

// The save/load screen's slot 21, the notification (+0x13c the name field, +0x140 the save button, +0x134/+0x138
// the list's header and its title bar, +0x144 the list, +0x148 a button, +0x174 the load button, +0x178 the delete
// button, +0x17c cancel, +0x180 the delete question, +0x184 the list's strings, +0x18c/+0x190 the saves, +0x19c the
// statistics): (from, what) for a click: save (the statistics' pattern from the name, filled, the dialog no longer
// replaced, the application's slot 80, the screen closed, the application saves under the file name: its slot
// 109), load (the load button or the list: the world's platform slot 10, the application loads the save chosen,
// its slot 110, and when that worked the world's slot 80; closed), delete (a question dialog 0x5156e0 with the
// texts 0x713180, 0x7131a8, 0x7131fc), the question's yes (the file deleted (File_Delete 0x4a14e0): its entry
// deleted and removed, the list's strings made again, or the "no saves" text; a message box when it failed; the
// list unselected, the list enabled when there are saves, the details), cancel (closed); a list or header change
// (what 1) moves the title bar's column marker or the selection between the list and the bar; then the frame
// window's notification
DWORD THISCALL SaveLoad_Notify(BYTE *self, BYTE *from, DWORD what)   // 0x563890
{
    using Fn0 = DWORD(THISCALL *)(BYTE *);

    if (!what) {
        if (m(self, 0x140) && pv(from) == m(self, 0x140)) {
            BYTE z[4];
            zs_ctor_text(z, vc<const char *>(bp(m(self, 0x13c)), 44));
            zax::fn<void(THISCALL *)(DWORD, BYTE *)>(0x5bc910)(m(self, 0x19c), z);
            zs_dtor(z);
            mb(self, 0x1a1) = 1;
            zax::fn<void(THISCALL *)(DWORD)>(0x5bc2a0)(m(self, 0x19c));
            mb(self, 0x1a1) = 0;
            zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x404180)(self, 0);
            vc(bp(gd(glob_addr(G_APP))), 80);
            BYTE name[4];
            zs_copy(name, m(self, 0x19c) + 0x68);
            vc(self, V_CLOSE);
            const DWORD r = vc(bp(gd(glob_addr(G_APP))), 109, c_str(name));
            zs_dtor(name);
            return r;
        }
        if (m(self, 0x174) && (pv(from) == m(self, 0x174) || pv(from) == m(self, 0x144))) {
            zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x404180)(self, 0);
            BYTE *platform = vc<BYTE *>(bp(gd(glob_addr(G_WORLD))), 1);
            vc(platform, 10);
            const DWORD i = vc(bp(m(self, 0x144)), 77);
            const char *file = c_str(bp(at<DWORD *>(m(self, 0x18c))[i] + 0x68));
            if (vc(bp(gd(glob_addr(G_APP))), 110, file) & 0xff)
                vc(bp(gd(glob_addr(G_WORLD))), 80);
            return vc(self, V_CLOSE);
        }
        if (pv(from) == m(self, 0x178)) {
            BYTE *o = alloc(0x140);
            DWORD q = 0;
            if (o) {
                const char *a = c_str(bp(glob_addr(0x713180))), *b = c_str(bp(glob_addr(0x7131a8))), *c = c_str(bp(glob_addr(0x7131fc)));
                q = zax::fn<DWORD(THISCALL *)(BYTE *, BYTE *, BYTE *, const char *, const char *, const char *)>(0x5156e0)(o, self, self, b, a, c);
            }
            m(self, 0x180) = q;
            return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40c6e0)(self, q, 6);
        }
        if (pv(from) == m(self, 0x180)) {
            DWORD i = vc(bp(m(self, 0x144)), 77);
            const char *file = c_str(bp(at<DWORD *>(m(self, 0x18c))[i] + 0x68));
            if (zax::fn<bool(__fastcall *)(const char *, DWORD)>(0x4a14e0)(file, 0x20)) {
                i = vc(bp(m(self, 0x144)), 77);
                if (BYTE *e = bp(at<DWORD *>(m(self, 0x18c))[i]))
                    vc(e, 1, 1);
                const DWORD n = m(self, 0x190);
                if (i < n) {
                    if (i + 1 < n)
                        memmove_(bp(m(self, 0x18c)) + i * 4, bp(m(self, 0x18c)) + (i + 1) * 4, (n - (i + 1)) * 4);
                    m(self, 0x190) = m(self, 0x190) - 1;
                }
                vc(bp(m(self, 0x184)), 17);
                if (m(self, 0x190)) {
                    for (DWORD k = 0; k < m(self, 0x190); k++) {
                        BYTE z[4];
                        const DWORD d = zax::fn<DWORD(THISCALL *)(DWORD, BYTE *)>(0x5bcb60)(at<DWORD *>(m(self, 0x18c))[k], z);
                        vc(bp(m(self, 0x184)), 18, c_str(bp(d)));
                        zs_dtor(z);
                    }
                } else {
                    BYTE z[4];
                    zs_ctor(z);
                    zs_format(z, glob<const char *>(0x620ee0), c_str(bp(glob_addr(0x7131a4))));
                    vc(bp(m(self, 0x184)), 18, c_str(z));
                    zs_dtor(z);
                }
                zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x406890)(m(self, 0x144), m(self, 0x184));
            } else {
                BYTE *o = alloc(0x11c);
                DWORD box = 0;
                if (o) {
                    const char *title = c_str(bp(glob_addr(0x71317c)));
                    box = zax::fn<DWORD(THISCALL *)(BYTE *, const char *, const char *, DWORD)>(0x403b30)(o, title, c_str(bp(glob_addr(0x7131f8))), 0);
                }
                zax::fn<void(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x40f5f0)(box, m(bp(gd(glob_addr(G_WORLD))), 0x2a8), 0, 0);
            }
            vc(bp(m(self, 0x144)), 80, 0xffffffff, 0);
            mb(bp(m(self, 0x144)), W_ENABLED) = m(self, 0x190) != 0;
            zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x4100f0)(m(self, 0x148), 0);
            return zax::fn<Fn0>(0x564090)(self);
        }
        if (pv(from) == m(self, 0x17c))
            return vc(self, V_CLOSE);
    } else if (what == 1) {
        if (m(self, 0x140) && pv(from) == m(self, 0x134)) {
            BYTE *bar = bp(m(self, 0x138));
            int r[4] = {0, 0, 0, mi(bar, 0x10) - mi(bar, 8)};
            r[0] = zax::fn<int(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(bp(m(self, 0x134)), 0) + r[0];
            r[2] = zax::fn<int(THISCALL *)(BYTE *, DWORD)>(0x4028f0)(bp(m(self, 0x134)), 1) + r[0];
            return vc(bp(m(self, 0x13c)), V_SET_RECT, r);
        }
        if (pv(from) == m(self, 0x144)) {
            if (m(self, 0x138) && vc(bp(m(self, 0x144)), 77) != 0xffffffff)
                vc(bp(m(self, 0x138)), 80, 0xffffffff, 0);
            zax::fn<Fn0>(0x564090)(self);
        } else if (m(self, 0x138) && pv(from) == m(self, 0x138)) {
            if (vc(bp(m(self, 0x138)), 77) != 0xffffffff)
                vc(bp(m(self, 0x144)), 80, 0xffffffff, 0);
            zax::fn<Fn0>(0x564090)(self);
        }
    }

    return zax::fn<DWORD(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4035f0)(self, from, what);
}

// ---- The inventory display page (vtable 0x5f8200; +0x108 the window whose slot 79 says whether the description
// shows, +0x130 the frame's fade, +0x134 the background) ----------------------------------------------------------

// Slot 4: (frame) the background (with the Transparent Mask mode); for the item in focus of the player's inventory
// (0x428b10, 0x426070; a CInventoryItem, its CZaxInventoryItemDefinition's picture: the definition's slot 33 indexes
// the item picture list 0x6c0948, the record's slot 23): while fading (+0x130 below 0x5ec3f4) its frame
// (InventoryWindow_DrawFrame 0x4db960), else the picture and the item's own drawing (slot 28) above the name line;
// the name (Item_DisplayName 0x4d66d0) in the options' font (the custom rules' inventory display +0x3c: +8/+0xc the
// picture, +0x18/+0x1c the name, +0x20 the width, +0x38 the margin, +0x3c/+0x40 the fonts); with the description
// window's slot 79 the description (0x4d67c0) wrapped below; the children
DWORD THISCALL InventoryPage_Draw(BYTE *self, DWORD frame)   // 0x4dbd50
{
    using Kind = bool(THISCALL *)(DWORD, DWORD);
    zax::fn<void(THISCALL *)(DWORD, DWORD, int, int, DWORD)>(0x4b3280)(frame, m(self, 0x134), 0, 0, glob_addr(0x6bcc68));
    BYTE *world = bp(m(bp(gd(glob_addr(G_APP))), 0x2e0));
    DWORD rules = 0;
    if (m(world, 0x38)) {
        const DWORD ci = zax::fn<DWORD(__cdecl *)()>(0x5a1350)();
        if (zax::fn<Kind>(0x416790)(m(world, 0x38), ci))
            rules = m(world, 0x38);
    }
    BYTE *opts = zax::fn<BYTE *(THISCALL *)(DWORD)>(0x5a1d40)(rules) + 0x3c;
    BYTE *inv = zax::fn<BYTE *(__cdecl *)()>(0x428b10)();
    if (m(inv, 0x20)) {
        const DWORD f = zax::fn<DWORD(THISCALL *)(BYTE *)>(0x426070)(inv);
        if (f < gd(glob_addr(0x6c097c))) {
            DWORD item = vc(inv, 26, f);
            if (item && !zax::fn<Kind>(0x416790)(item, zax::fn<DWORD(__cdecl *)()>(0x429e50)()))
                item = 0;
            DWORD def = zax::fn<DWORD(THISCALL *)(DWORD)>(0x4dd480)(item);
            if (def && !zax::fn<Kind>(0x416790)(def, zax::fn<DWORD(__cdecl *)()>(0x5b5f10)()))
                def = 0;
            const DWORD pic = vc(bp(def), 33);
            BYTE *rec = bp(record_loaded(glob_addr(0x6c0948), pic));
            const DWORD img = vc(rec, 23, 0);
            bool describe = false;
            BYTE *dw = bp(m(self, 0x108));
            if (dw && vc(dw, 79)) {
                describe = true;
            } else if (x87_lt(m<float>(self, 0x130), *glob<float *>(0x5ec3f4))) {
                const float fade = m<float>(self, 0x130);
                DWORD bits;
                memcpy(&bits, &fade, 4);
                zax::fn<void(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4db960)(frame, img, m(opts, 8), m(opts, 0xc), bits);
            } else {
                if (img)
                    zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(frame, img, m(opts, 8), m(opts, 0xc), glob_addr(0x6bcc68));
                BYTE *font = bp(record_loaded(glob_addr(0x6be1e0), m(opts, 0x3c)));
                const DWORD h = m(font, 0x80);
                const int t = x87_ftol((long double)(unsigned long long)h * (long double)*glob<double *>(0x5f8338));
                vc(bp(item), 28, frame, m(opts, 0x18), mi(opts, 0x1c) - t, pv(inv));
            }
            BYTE name[4];
            zs_ctor(name);
            zax::fn<void(__fastcall *)(DWORD, BYTE *)>(0x4d66d0)(zax::fn<DWORD(THISCALL *)(DWORD)>(0x482de0)(item), name);
            const DWORD nfont = record_loaded(glob_addr(0x6be1e0), m(opts, 0x3c));
            using Draw = void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, const char *, DWORD, DWORD);
            zax::fn<Draw>(0x4a6470)(nfont, frame, m(opts, 0x18), m(opts, 0x1c), c_str(name), 0, 0);
            if (describe) {
                BYTE desc[4];
                zs_ctor(desc);
                zax::fn<void(__fastcall *)(DWORD, BYTE *)>(0x4d67c0)(zax::fn<DWORD(THISCALL *)(DWORD)>(0x482de0)(item), desc);
                const char *text = c_str(desc);
                const DWORD h = m(bp(record_loaded(glob_addr(0x6be1e0), m(opts, 0x3c))), 0x80);
                const DWORD wfont = record_inline(glob_addr(0x6be1e0), m(opts, 0x40));
                const int margin = mi(opts, 0x38);
                const int t = x87_ftol((long double)(unsigned long long)h * (long double)*glob<double *>(0x5f8338));
                using Wrap = void(THISCALL *)(DWORD, DWORD, int, int, const char *, int, DWORD, DWORD);
                zax::fn<Wrap>(0x4a6790)(wfont, frame, margin + mi(opts, 0x18), margin - t + mi(opts, 0x1c), text, mi(opts, 0x20) - margin * 2, 2, 0);
                zs_dtor(desc);
            }
            zs_dtor(name);
        }
    }
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_c3_misc)
{
    const Hook list[] = {
#include "ui_c3_misc_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
