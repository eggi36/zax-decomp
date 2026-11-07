// The looks of the window toolkit's windows (docs/DECOMPILATION.md 9, "The rest of the user interface"): the
// button look (vtable 0x5f88b4; its constructor 0x4e2220 and the base look 0x5994d0/0x599510 are in music.cpp,
// which docs/functions.csv files as Sound) with its drawing and factory, and the text look with its factory, which
// takes its fonts from the main menu's display options.
//
// Layouts [confirmed: the constructors and the slots below]:
//   look (0xc bytes): +0 vtable, +4 the owner window, +8 the factory
//   button look factory 0x6c1040 (a function-local static, vtable 0x5f88c0, guard 0x6c1044 bit 0)
//   text look factory 0x6c1ac0 (vtable 0x5f88fc, guard 0x6c1ab8 bit 0): +4 the font of "Font" (+0x18 of the
//     options), +8 "Shadow Font" (+0x20), +0xc "Hover Font" (+0x1c), +0x10 "Font Hover Effect" (+0x24)
//   the button's members the drawing reads (a button 0x40fa60): +0x100 pressed, +0x101 the mouse over it, +0x103
//     pushed, +0x108 the text's alignment (0 left, 1 centre, 2 right), +0x110 the image, +0x114 the disabled
//     image's shadow, +0x118 the image centred, +0x11a no frame, +0x12c the text drawn with an image
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD BLEND_OPACITY = 0x6bb4e0, BLEND_TEXTURE = 0x6bc4e0;
constexpr DWORD FONT_LIST = 0x6be1e0, FONT_RECORDS = 0x6be210, FONT_COUNT = 0x6be214;

void blit_at(BYTE *frame, DWORD src, int x, int y) { zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, int, int, DWORD)>(0x4b3280)(frame, src, x, y, glob_addr(BLEND_OPACITY)); }
void set_opacity(DWORD bits) { zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x433be0)(glob_addr(BLEND_OPACITY), bits); }
int line_height(DWORD font) { return zax::fn<int(__fastcall *)(DWORD)>(0x4a6140)(font); }
BYTE color_index(BYTE *color)   // a FieldColor's palette index (+0xa when flag 1 of +0xc is set)
{
    if (mb(color, 0xc) & 1)
        return mb(color, 0xa);
    return (BYTE)zax::fn<DWORD(THISCALL *)(BYTE *)>(0x53f160)(color);
}
void atexit_(DWORD fn) { zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(fn)); }

// The font of a font list index as DefinitionList_LoadRecord 0x55e090 gives it (index 0 when out of range, null
// when the list is empty or the record is being loaded)
DWORD font_record(DWORD index)
{
    DWORD recs = gd(glob_addr(FONT_RECORDS));
    if (!recs)
        return 0;
    if (index >= gd(glob_addr(FONT_COUNT)))
        index = 0;
    const DWORD rec = index << 7;
    if (gb(recs + rec + 0x7f))
        return 0;
    if (!m(bp(recs), rec + 0x78)) {
        zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(glob_addr(FONT_LIST), index);
        recs = gd(glob_addr(FONT_RECORDS));
    }
    return m(bp(recs), rec + 0x78);
}

}  // namespace

// ---- The button look ------------------------------------------------------------------------------------------

// Slot 2 (the frame): the owner's background texture (slot 72 over its client rect, pushed in by one pixel while
// pressed) unless it has an image and a frame or is transparent; the image (the disabled one's shadow first and
// 40 % opacity when disabled; centred, at the border or not at all while not pressed without a frame); the text
// (slot 74: frame, x, y, alignment) unless an image stands alone; the bevel around the client area unless the
// button has no frame (sunken while pressed)
void THISCALL ButtonLook_Draw(BYTE *self, BYTE *frame)   // 0x4e2240
{
    BYTE *o = bp(m(self, 4));
    const DWORD font = vc(o, 45);
    int pressed = 0;
    if ((mb(o, 0x100) && mb(o, 0x101)) || mb(o, 0x103) || vc(o, 79))
        pressed = 1;
    o = bp(m(self, 4));
    const int w = mi(o, W_CONTENT_W), h = mi(o, W_CONTENT_H);
    int r1[4] = {mi(o, W_RECT), mi(o, W_RECT + 4), w - pressed + mi(o, W_RECT), h - pressed + mi(o, W_RECT + 4)};
    int r2[4] = {pressed, pressed, w, h};
    if ((!m(o, 0x110) || mb(o, W_BORDER3)) && !mb(o, W_TRANSPARENT))
        vc(frame, 23, vc(o, V_TEXTURE), r1, r2, glob_addr(BLEND_TEXTURE));
    o = bp(m(self, 4));
    if (m(o, 0x110)) {
        if (!(BYTE)zax::fn<DWORD(THISCALL *)(BYTE *)>(0x40c630)(o)) {
            o = bp(m(self, 4));
            if (m(o, 0x114)) {
                const int b = (signed char)mb(o, W_BORDER3);
                blit_at(frame, m(o, 0x114), b, b);
            }
            set_opacity(0x3ecccccd);
        }
        o = bp(m(self, 4));
        if (mb(o, 0x11a)) {
            if (pressed) {
                const int b = (signed char)mb(o, W_BORDER3);
                blit_at(frame, m(o, 0x110), b, b);
            }
        } else if (mb(o, 0x118)) {
            blit_at(frame, m(o, 0x110), mi(o, W_CONTENT_W) / 2 + pressed, mi(o, W_CONTENT_H) / 2 + pressed);
        } else {
            const int b = (signed char)mb(o, W_BORDER3) + pressed;
            blit_at(frame, m(o, 0x110), b, b);
        }
        set_opacity(0x3f800000);
    }
    o = bp(m(self, 4));
    if (!m(o, 0x110) || mb(o, 0x12c)) {
        const DWORD align = m(o, 0x108);
        if (align <= 2) {
            const DWORD y = ((DWORD)(mi(o, W_CONTENT_H) - line_height(font)) >> 1) + pressed;
            int x;
            if (align == 0)
                x = (signed char)mb(o, W_BORDER3) + pressed + 1;
            else if (align == 1)
                x = mi(o, W_CONTENT_W) / 2 + pressed;
            else
                x = mi(o, W_CONTENT_W) - (signed char)mb(o, W_BORDER3) + pressed - 1;
            vc(o, 74, frame, x, y, align);
        }
    }
    o = bp(m(self, 4));
    if (mb(o, 0x11a))
        return;
    const BYTE dark = color_index(o + 0xb0);
    const BYTE light = color_index(bp(m(self, 4)) + 0xa0);
    o = bp(m(self, 4));
    if (!(gb(glob_addr(0x716190)) & 1)) {
        gb(glob_addr(0x716190)) |= 1;
        atexit_(fn_addr(0x401270));
    }
    gd(glob_addr(0x7161ac)) = m(o, W_CONTENT_H);
    gd(glob_addr(0x7161a0)) = 0;
    gd(glob_addr(0x7161a4)) = 0;
    gd(glob_addr(0x7161a8)) = m(o, W_CONTENT_W);
    zax::fn<DWORD(__fastcall *)(DWORD, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40a740)(
        font, frame, glob_addr(0x7161a0), light, dark, pressed == 0, 0xf, 0);
}

// The button look factory: a function-local static (vtable 0x5f88c0) made on the first call
DWORD __cdecl ButtonLookFactory_Get()   // 0x4e2610
{
    if (!(gb(glob_addr(0x6c1044)) & 1)) {
        gd(glob_addr(0x6c1040)) = vt(0x5f88c0);
        gb(glob_addr(0x6c1044)) |= 1;
        atexit_(fn_addr(0x4e2650));
    }
    return glob_addr(0x6c1040);
}
// Its deleting destructor (the base message's destructor 0x5788c0; operator delete)
BYTE *THISCALL ButtonLookFactory_DeletingDtor(BYTE *self, DWORD flags)   // 0x4e2670
{
    m(self, 0) = vt(0x5f88c0);
    zax::fn<void(THISCALL *)(BYTE *)>(0x5788c0)(self);
    if (flags & 1)
        zax::fn<void(__cdecl *)(BYTE *)>(0x5d0320)(self);
    return self;
}
// Slot 1 (the owner): a new button look (0xc bytes of the pool) of this factory
BYTE *THISCALL ButtonLookFactory_Make(BYTE *self, DWORD owner)   // 0x4e26a0
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0xc));
    if (!o)
        return nullptr;
    return zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4e2220)(o, self, owner);
}

// ---- The text look --------------------------------------------------------------------------------------------

// The constructor (the factory, the owner): the owner loses its frame (+0x63)
BYTE *THISCALL TextLook_Ctor(BYTE *self, DWORD factory, DWORD owner)   // 0x4e2b80
{
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x599510)(self, owner);
    m(self, 0) = vt(0x5f88d4);
    m(self, 8) = factory;
    mb(bp(m(self, 4)), W_FRAMED) = 0;
    return self;
}
// The deleting destructor (the base look's vtable 0x5994d0; a pool block of 0xc)
BYTE *THISCALL TextLook_DeletingDtor(BYTE *self, DWORD flags)   // 0x4e2bb0
{
    zax::fn<void(THISCALL *)(BYTE *)>(0x5994d0)(self);
    if (flags & 1)
        pool_free(self, 0xc);
    return self;
}
// Slot 3: the owner sized to its text ("XXX" without one) in the factory's font plus 2, the line height plus 10
void THISCALL TextLook_Fit(BYTE *self)   // 0x4e2df0
{
    const DWORD font = m(bp(m(self, 8)), 4);
    const char *text = vc<const char *>(bp(m(self, 4)), 44) ? vc<const char *>(bp(m(self, 4)), 44) : glob<const char *>(0x60a680);
    BYTE *o = bp(m(self, 4));
    const int h = line_height(font) + 10;
    const int w = zax::fn<int(THISCALL *)(DWORD, const char *, int)>(0x4a7310)(font, text, -1);
    vc(o, V_SET_SIZE, w + 2, h);
}

// The text look factory: a function-local static (vtable 0x5f88fc) with the main menu's fonts, made on the first
// call (the custom rules' test of the rules holder decides nothing: GetMainMenuDisplayOptions 0x5a1ee0 takes the
// rules object in ecx when it is one); "Shadow Font" is loaded through the font list's slots inline
DWORD __cdecl TextLookFactory_Get()   // 0x4e3680
{
    if (gb(glob_addr(0x6c1ab8)) & 1)
        return glob_addr(0x6c1ac0);
    gb(glob_addr(0x6c1ab8)) |= 1;
    gd(glob_addr(0x6c1ac0)) = vt(0x5f88fc);
    BYTE *holder = bp(m(bp(gd(glob_addr(0x713f14))), 0x2e0));
    DWORD rules = 0;
    if (m(holder, 0x38)) {
        const DWORD ci = zax::fn<DWORD(__cdecl *)()>(0x5a1350)();
        if ((BYTE)zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(m(holder, 0x38), ci))
            rules = m(holder, 0x38);
    }
    BYTE *opt = zax::fn<BYTE *(THISCALL *)(DWORD)>(0x5a1ee0)(rules);
    gd(glob_addr(0x6c1ac4)) = font_record(m(opt, 0x18));
    DWORD font = 0;
    DWORD recs = gd(glob_addr(FONT_RECORDS));
    if (recs) {
        DWORD index = m(opt, 0x20);
        if (index >= gd(glob_addr(FONT_COUNT)))
            index = 0;
        const DWORD rec = index << 7;
        if (!gb(recs + rec + 0x7f)) {
            if (!m(bp(recs), rec + 0x78) && recs && !gb(recs + rec + 0x7f)) {
                gb(recs + rec + 0x7f) = 1;
                char path[0x100];
                const DWORD name = vc(glob<void *>(FONT_LIST), 1, index);
                zax::fn<void(__fastcall *)(char *, DWORD, DWORD)>(0x4a4220)(path, glob_addr(0x6be218), name);
                char *file = zax::fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
                DWORD loaded;
                if (file == path) {
                    loaded = vc(glob<void *>(FONT_LIST), 37, glob_addr(0x62e558), file, gd(glob_addr(FONT_RECORDS)) + rec + 0x78);
                } else {
                    file[-1] = 0;
                    loaded = vc(glob<void *>(FONT_LIST), 37, path, file, gd(glob_addr(FONT_RECORDS)) + rec + 0x78);
                }
                m(bp(gd(glob_addr(FONT_RECORDS))), rec + 0x78) = loaded;
                gb(gd(glob_addr(FONT_RECORDS)) + rec + 0x7f) = 0;
                recs = gd(glob_addr(FONT_RECORDS));
            }
            font = m(bp(recs), rec + 0x78);
        }
    }
    gd(glob_addr(0x6c1ac8)) = font;
    gd(glob_addr(0x6c1acc)) = font_record(m(opt, 0x1c));
    gd(glob_addr(0x6c1ad0)) = m(opt, 0x24);
    atexit_(fn_addr(0x4e3890));
    return glob_addr(0x6c1ac0);
}
// Its deleting destructor
BYTE *THISCALL TextLookFactory_DeletingDtor(BYTE *self, DWORD flags)   // 0x4e38b0
{
    m(self, 0) = vt(0x5f88fc);
    zax::fn<void(THISCALL *)(BYTE *)>(0x5788c0)(self);
    if (flags & 1)
        zax::fn<void(__cdecl *)(BYTE *)>(0x5d0320)(self);
    return self;
}
// Slot 1 (the owner): a new text look of this factory
BYTE *THISCALL TextLookFactory_Make(BYTE *self, DWORD owner)   // 0x4e38e0
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0xc));
    if (!o)
        return nullptr;
    return zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4e2b80)(o, self, owner);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_looks)
{
    const Hook list[] = {
#include "ui_looks_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
