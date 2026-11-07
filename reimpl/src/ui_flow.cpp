// Three small pieces of the user interface's flow (docs/DECOMPILATION.md 9, "The rest of the user interface"): the
// application window's slot 65 forwarded, the screen images shown one after another (the intro and outro
// screens), and the end game credits replacing a dialog's window. Every call goes through its exe address.
#include "ui_screens.h"

namespace zax::ui {

// The application window's slot 65: its +0x34's slot 65 (a tail call)
DWORD THISCALL App_WindowSlot65(BYTE *self)   // 0x4cdf60
{
    return vc(bp(m(self, 0x34)), 65);
}

// The screen images of a list of names (null-terminated): each name's image of the lists 0x6bf850 and 0x6bf9b8
// (both loaded inline) shown on a screen image (0x578910: 15 seconds, waits) one after another; then it waits,
// hides and clears
void THISCALL ScreenImages_Show(const char **names)   // 0x5be0d0
{
    alignas(4) BYTE image[0x3c];
    fn<void(THISCALL *)(BYTE *)>(0x578910)(image);
    mb(image, 0x30) = 1;
    m(image, 4) = 0x41700000;
    for (const char **n = names; *n; n++) {
        const DWORD a = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(0x6bf850), *n, -1);
        const DWORD b = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(0x6bf9b8), *n, -1);
        const DWORD first = record_inline(glob_addr(0x6bf850), a);
        const DWORD second = record_inline(glob_addr(0x6bf9b8), b);
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x578e70)(image, first, second);
    }
    fn<void(THISCALL *)(BYTE *)>(0x578eb0)(image);
    fn<void(THISCALL *)(BYTE *)>(0x578930)(image);
}

// The end game credits ("EndGameCreditsWindow", 0x13c, vtable 0x605838): a credits control ("~CJCreditsControl",
// 0x108, vtable 0x605708: +0x100 the credits text of "ZaxCredits.txt" in its font, 40 × 20 font fifths) over the
// whole screen (80, 60 in on 640 wide), 400 high and centred when the "Final End Screen" exists; it replaces the
// dialog's window (CDialog::ReplaceWindow 0x4040c0 with 1)
void THISCALL EndCredits_Show(BYTE *self)   // 0x5bede0
{
    BYTE *d = alloc(0x13c);
    if (!d) {
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4040c0)(self, nullptr, 1);
        return;
    }
    dialog_ctor(d, 0, glob<const char *>(0x627694));
    m(d, 0) = vt(0x605838);
    BYTE *c = alloc(0x108);
    if (c) {
        window_ctor(c, pv(d), glob<const char *>(0x627680));
        m(c, 0) = vt(0x605708);
        BYTE *text = op_new(0x20);
        if (text)
            text = fn<BYTE *(THISCALL *)(BYTE *, const char *, DWORD)>(0x475eb0)(text, glob<const char *>(0x627670), m(c, W_FONT));
        m(c, 0x100) = pv(text);
        const DWORD q = m(bp(m(c, W_FONT)), 0x80) / 5;
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40d510)(c, q * 200, q * 100);
        mb(c, 0x104) = 0;
    }
    m(d, 0x134) = pv(c);
    add_child(d, c, 1, 0, NOWHERE, NOWHERE, 0);
    BYTE *screen = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
    set_width(d, vc(screen, 45));
    screen = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
    set_height(d, vc(screen, 39));
    screen = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
    if (vc(screen, 45) == 0x280)
        move_to(d, 0x50, 0x3c);
    else
        move_to(d, 0, 0);
    const DWORD index = find_index(glob_addr(0x6bf850), glob_addr(0x62765c));
    if (record_loaded(glob_addr(0x6bf850), index)) {
        set_height(bp(m(d, 0x134)), 0x190);
        BYTE *w = bp(m(d, 0x134));
        const int h = mi(w, W_RECT + 0xc) - mi(w, W_RECT + 4);
        const int width = mi(w, W_RECT + 8) - mi(w, W_RECT);
        BYTE *s = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
        const DWORD y = (vc(s, 39) - (DWORD)h) >> 1;
        const DWORD x = (vc(s, 45) - (DWORD)width) >> 1;
        move_to(bp(m(d, 0x134)), (int)x, (int)y);
    }
    m(d, 0x138) = 0;
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4040c0)(self, d, 1);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_flow)
{
    const Hook list[] = {
#include "ui_flow_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
