// The last user interface pieces of 0x560000-0x5cffff (docs/DECOMPILATION.md 9, "Phase 4 remains, band C"): the
// windowed browsable image, the watch's detail renderer (the playing time and the real time), the virtual key
// mapping of the message pump. Every call goes through its exe address.
#include "ui_screens.h"

namespace zax::ui4c {

using namespace ui;

constexpr DWORD FONT_LIST = 0x6be1e0;

// ---- CWindowedBrowsableImage ("~CWindowedBrowsableImage", 0x10c, vtable 0x602c78): a window (CJWindow
// 0x40a8a0) with +0x100 the image (deleted through its slot 0), +0x104/+0x108 the image's offset (minus its
// rectangle's top left)

BYTE *THISCALL BrowsableImage_Ctor(BYTE *self, DWORD parent)   // 0x599330
{
    window_ctor(self, parent, glob<const char *>(0x624840));
    m(self, 0) = vt(0x602c78);
    m(self, 0x104) = 0;
    m(self, 0x108) = 0;
    m(self, 0x100) = 0;
    return self;
}

BYTE *THISCALL BrowsableImage_DeletingDtor(BYTE *self, DWORD flags)   // 0x599370
{
    m(self, 0) = vt(0x602c78);
    if (BYTE *image = bp(m(self, 0x100))) {
        vc(image, 0, 1);
        m(self, 0x100) = 0;
    }
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x10c);
    return self;
}

// (image): the old image deleted; the window sized to the new one's rectangle (its slot 3) and the offset set
void THISCALL BrowsableImage_SetImage(BYTE *self, BYTE *image)   // 0x5993c0
{
    if (BYTE *old = bp(m(self, 0x100))) {
        vc(old, 0, 1);
        m(self, 0x100) = 0;
    }
    m(self, 0x100) = pv(image);
    if (!image)
        return;
    int r[4];
    vc(image, 3, r);
    vc(self, 59, r[2] - r[0], r[3] - r[1]);
    m(self, 0x104) = (DWORD)-r[0];
    m(self, 0x108) = (DWORD)-r[1];
}

// ---- The watch (CWatchInventoryDetailRenderer, slot 24): (unused, frame, x, y, unused) the playing time
// (the reporter's slot 14, seconds) as days, hours, minutes and seconds and the real time (ctime) in the font
// "Small Green", two lines apart, 0x2d right of x

void THISCALL Watch_Draw(BYTE *, DWORD, DWORD frame, int x, int y, DWORD)   // 0x596850
{
    BYTE text[4];
    zs_ctor(text);
    const DWORD index = vc(glob<void *>(FONT_LIST), 3, glob<const char *>(0x60bb18), -1);
    const DWORD font = record_loaded(glob_addr(FONT_LIST), index);
    long double t = vc<long double>(bp(gd(glob_addr(G_REPORTER))), 14);
    const int days = x87_ftol(t * (long double)*glob<float *>(0x5f8844));
    t -= (long double)(int)((DWORD)days * 86400u);
    const int hours = x87_ftol(t * (long double)*glob<float *>(0x5f8838));
    t -= (long double)(int)((DWORD)hours * 3600u);
    const int minutes = x87_ftol(t * (long double)*glob<float *>(0x5f8834));
    const int seconds = x87_ftol(t - (long double)(int)((DWORD)minutes * 60u));
    zs_format(text, glob<const char *>(0x624650), days, hours, minutes, seconds);
    using Draw = void(THISCALL *)(DWORD, DWORD, int, int, const char *, DWORD, DWORD);
    using Height = int(THISCALL *)(DWORD);
    fn<Draw>(0x4a6470)(font, frame, x + 0x2d, y + 10, c_str(text), 1, 0);
    const int below = fn<Height>(0x4a6be0)(font) * 2 + 10;
    DWORD now = fn<DWORD(__cdecl *)(DWORD *)>(0x5d20af)(nullptr);
    const char *when = fn<const char *(__cdecl *)(DWORD *)>(0x5d2864)(&now);
    zs_format(text, glob<const char *>(0x62463c), when);
    fn<Draw>(0x4a6470)(font, frame, x + 0x2d, y + below, c_str(text), 1, 0);
    fn<Height>(0x4a6be0)(font);
    zs_dtor(text);
}

// ---- ListBoxListSource slot 0: no index (-1). Four bytes (or eax, -1; ret); the jump of the hook overwrites the
// alignment nops behind it up to 0x592390

DWORD THISCALL ListBoxListSource_NoIndex(BYTE *)   // 0x592380
{
    return 0xffffffff;
}

// ---- The message pump's key mapping: (virtual key) the game's key code of the navigation and function keys;
// Tab and the digits only with Ctrl held (Keys::IsDown 0x4e2850 with 0x1d or 0x11d); 0 for every other key

DWORD __fastcall Input_VirtualKeyToGameKey(DWORD vk)   // 0x599580
{
    switch (vk) {
    case 0x21: return 7;
    case 0x22: return 0xf;
    case 0x23: return 0xe;
    case 0x24: return 5;
    case 0x25: return 0xb;
    case 0x26: return 6;
    case 0x27: return 0xc;
    case 0x28: return 10;
    case 0x2d: return 0x10;
    case 0x2e: return 0x11;
    case 0x70: return 0x13;
    case 0x73: return 0x14;
    case 0x75: return 0x1c;
    case 0x76: return 0x15;
    case 0x77: return 0x17;
    case 0x78: return 0x1d;
    case 0x79: return 0x19;
    case 0x7a: return 0x1a;
    case 0x7b: return 0x1e;
    case 9:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
    case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
        if (key_down(0x1d) || key_down(0x11d))
            return vk;
        return 0;
    default: return 0;
    }
}

}  // namespace zax::ui4c

namespace zax {

using namespace ui4c;

ZAX_HOOKS(ui_rest4c)
{
    const Hook list[] = {
#include "ui_rest4c_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
